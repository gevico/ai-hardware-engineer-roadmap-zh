---
title: 第 4 讲：Qwen2.5-72B-Instruct FP16 —— 多 GPU 推理
description: 第 4 讲：Qwen2.5-72B-Instruct FP16 —— 多 GPU 推理
---

# 第 4 讲：Qwen2.5-72B-Instruct FP16 —— 多 GPU 推理

## 概述

FP16 下的 Qwen2.5-72B-Instruct 权重约 145 GB。地球上没有任何单块加速器能把这么多权重装进 HBM。该模型只能靠**跨多块 GPU 切分**来跑 —— layer 内做张量并行，layer 间可选流水线并行，全部由位于 decode（逐 token 生成阶段）热路径上的 NCCL 集合通信协调。

本讲从推理工程师的视角出发，讲如何让 Qwen2.5-72B 在真实机器上以生产级质量做推理服务：

* 4 × H100 80 GB（NVL 或 SXM）—— 最优配置。
* 8 × A100 80 GB —— 2024 年的部署中很常见。
* 8 × L40S 48 GB —— 便宜，没有 NVLink。
* 4 × MI300X 192 GB —— 越来越常见。

不做量化。纯 FP16（模型发布时是 BF16；推理时转成 FP16）。目标是切分正确、decode 中集合通信开销最小，以及推理服务吞吐足够高、高到让答案无关紧要 —— 单流延迟与批处理吞吐皆然。

读完后，你应当能够：

* 精确算出在张量并行下，哪些张量的哪些切片驻留在哪块 GPU 上。
* 预测每生成一个 token 所需的 NCCL 带宽需求。
* 给定（模型、硬件、延迟目标）三元组，选择 TP、PP 还是混合方案。
* 诊断 vLLM / SGLang / TRT-LLM 在 roofline（性能上界模型）上的位置。

---

## 1. 占用规模 —— Qwen2.5-72B 装在哪里？

FP16 下各张量的占用：

| 张量组 | 每层 FP16 字节数 | 合计（× 80 层） |
|---|---|---|
| `attn_q.weight` (8192 × 8192) | 134 MB | 10.7 GB |
| `attn_k.weight` (8192 × 1024) | 16.8 MB | 1.3 GB |
| `attn_v.weight` (8192 × 1024) | 16.8 MB | 1.3 GB |
| `attn_o.weight` (8192 × 8192) | 134 MB | 10.7 GB |
| `attn_qkv.bias` | ~26 KB | 2 MB |
| `attn_norm.weight` | 16 KB | 1.3 MB |
| `ffn_gate.weight` (8192 × 29568) | 485 MB | 38.8 GB |
| `ffn_up.weight` (8192 × 29568) | 485 MB | 38.8 GB |
| `ffn_down.weight` (29568 × 8192) | 485 MB | 38.8 GB |
| `ffn_norm.weight` | 16 KB | 1.3 MB |
| 每层合计 | ~1.78 GB | **142.3 GB** |
| `token_embd.weight` (152064 × 8192) | — | 2.49 GB |
| `output.weight` (152064 × 8192，untied) | — | 2.49 GB |
| `output_norm.weight` | — | 16 KB |
| **总计** | | **~147.3 GB** |

KV cache（每 token，所有层）：

```
2 (K+V) × 80 layers × 8 KV heads × 128 head_dim × 2 bytes = 327 680 bytes
At 32 k context, single sequence:                            10.0 GB
At 131 k context, single sequence:                           40.0 GB
```

聚合 VRAM 中需要同时容纳权重 + KV + 激活值 + CUDA 上下文 + NCCL 缓冲区：

| 机器 | 聚合 VRAM | 147 GB 权重之后的余量 | batch=1 @ 32k 的 KV | 备注 |
|---|---|---|---|---|
| 8 × L40S 48 GB | 384 GB | 237 GB | 可忽略 | 无 NVLink → 仅 PCIe 集合通信 |
| 8 × A100 40 GB | 320 GB | 173 GB | 可忽略 | NVLink 3，600 GB/s |
| 4 × A100 80 GB | 320 GB | 173 GB | 可忽略 | NVLink 3 |
| 4 × H100 80 GB | 320 GB | 173 GB | 可忽略 | NVLink 4，900 GB/s |
| 8 × H100 80 GB | 640 GB | 493 GB | 可忽略 | NVLink 4 + NVSwitch |
| 4 × MI300X 192 GB | 768 GB | 621 GB | 可忽略 | Infinity Fabric |

即便最小的机器（8 × L40S）在放下权重后仍有 237 GB 空闲。约束**不是容量** —— 而是 decode 期间**跨 GPU 互连的带宽**。

---

## 2. 张量并行：把每层切分到多块 GPU 上

张量并行（TP）把**每个矩阵乘法**切分到共享该层的各块 GPU 上。每块 GPU 持有**权重的一个切片**，并计算输出的一部分。

### 2.1 QKV 上的 TP

标准模式（Megatron 风格）：

* **Q、K、V 按 head 分片。** 对 Qwen2.5-72B 而言，`n_heads = 64`、`n_kv_heads = 8`：

```
TP=2:  GPU0 gets 32 Q heads + 4 KV heads
       GPU1 gets 32 Q heads + 4 KV heads
TP=4:  each GPU gets 16 Q heads + 2 KV heads
TP=8:  each GPU gets  8 Q heads + 1 KV head   ← matches n_kv_heads exactly
TP=16: cannot evenly split 8 KV heads — KV gets replicated on some pairs
```

自然的 TP 度数是**任何能整除 `n_kv_heads` 的数**。Qwen2.5-72B 有 8 个 KV head，因此 TP=8 是最大的“干净”切分。超过 TP=8 之后，必须成对地在 GPU 间复制 KV，这会浪费 VRAM，但出于延迟考虑实践中仍会这么做。

* **输出投影（W_O）按输入分片。** 每块 GPU 产生一份部分输出，通过 **AllReduce** 在 TP 组内求和。

### 2.2 FFN 上的 TP

* **Gate 与 Up 按输出分片（列并行）。** 每块 GPU 持有 `intermediate / TP` 列。经过 SwiGLU 后，每块 GPU 得到中间向量的一部分切片。
* **Down 按输入分片（行并行）。** 每块 GPU 用自己那部分中间向量乘以自己那部分 W_down。部分结果通过 **AllReduce** 在 TP 组内求和。

因此每层有**两次 AllReduce 操作**（一次在 attention 的 W_O 之后，一次在 FFN 的 W_down 之后），每次作用于大小为 `d_model = 8192` 的向量。


<details>
<summary>English original</summary>

**Lecture 4: Qwen2.5-72B-Instruct FP16 — Multi-GPU Inference**

**Overview**

Qwen2.5-72B-Instruct at FP16 is ~145 GB of weights. No single accelerator on the planet holds that in HBM. The model only runs by **splitting across multiple GPUs** — tensor parallel within a layer, optionally pipeline parallel across layers, all coordinated by NCCL collectives that sit in the decode hot path.

This lecture is the inference engineer's view of getting Qwen2.5-72B to serve at production quality on realistic boxes:

* 4 × H100 80 GB (NVL or SXM) — the sweet spot.
* 8 × A100 80 GB — common in 2024-vintage deployments.
* 8 × L40S 48 GB — cheap, no NVLink.
* 4 × MI300X 192 GB — increasingly common.

No quantization. Pure FP16 (the model ships BF16; we cast to FP16 for inference). The goal is correct partitioning, minimal collective overhead in decode, and serving high enough throughput that the answer doesn't matter — both single-stream latency and batched throughput.

By the end you should be able to:

* Compute exactly which slices of which tensors live on which GPU under tensor parallel.
* Predict NCCL bandwidth requirements per decoded token.
* Choose TP vs PP vs hybrid for a given (model, hardware, latency goal) triple.
* Diagnose where vLLM / SGLang / TRT-LLM is sitting on the roofline.

---

**1. Footprint — Where Does Qwen2.5-72B Fit?**

Per-tensor footprint at FP16:

| Tensor group | Per-layer FP16 bytes | Total (× 80 layers) |
|---|---|---|
| `attn_q.weight` (8192 × 8192) | 134 MB | 10.7 GB |
| `attn_k.weight` (8192 × 1024) | 16.8 MB | 1.3 GB |
| `attn_v.weight` (8192 × 1024) | 16.8 MB | 1.3 GB |
| `attn_o.weight` (8192 × 8192) | 134 MB | 10.7 GB |
| `attn_qkv.bias` | ~26 KB | 2 MB |
| `attn_norm.weight` | 16 KB | 1.3 MB |
| `ffn_gate.weight` (8192 × 29568) | 485 MB | 38.8 GB |
| `ffn_up.weight` (8192 × 29568) | 485 MB | 38.8 GB |
| `ffn_down.weight` (29568 × 8192) | 485 MB | 38.8 GB |
| `ffn_norm.weight` | 16 KB | 1.3 MB |
| Per-layer total | ~1.78 GB | **142.3 GB** |
| `token_embd.weight` (152064 × 8192) | — | 2.49 GB |
| `output.weight` (152064 × 8192, untied) | — | 2.49 GB |
| `output_norm.weight` | — | 16 KB |
| **Grand total** | | **~147.3 GB** |

KV cache (per token, all layers):

```
2 (K+V) × 80 layers × 8 KV heads × 128 head_dim × 2 bytes = 327 680 bytes
At 32 k context, single sequence:                            10.0 GB
At 131 k context, single sequence:                           40.0 GB
```

You need weights + KV + activations + CUDA contexts + NCCL buffers in aggregate VRAM:

| Box | Aggregate VRAM | Headroom after 147 GB weights | KV for batch=1 @ 32k | Notes |
|---|---|---|---|---|
| 8 × L40S 48 GB | 384 GB | 237 GB | trivial | No NVLink → PCIe-only collectives |
| 8 × A100 40 GB | 320 GB | 173 GB | trivial | NVLink 3, 600 GB/s |
| 4 × A100 80 GB | 320 GB | 173 GB | trivial | NVLink 3 |
| 4 × H100 80 GB | 320 GB | 173 GB | trivial | NVLink 4, 900 GB/s |
| 8 × H100 80 GB | 640 GB | 493 GB | trivial | NVLink 4 + NVSwitch |
| 4 × MI300X 192 GB | 768 GB | 621 GB | trivial | Infinity Fabric |

Even the smallest box (8 × L40S) has 237 GB free after weights. The constraint is **not capacity** — it's **bandwidth across the inter-GPU fabric** during decode.

---

**2. Tensor Parallelism: Split Each Layer Across GPUs**

Tensor parallel (TP) splits **each matrix multiply** across the GPUs that share the layer. Each GPU holds a **slice of the weights** and computes a slice of the output.

**2.1 TP on QKV**

Standard pattern (Megatron-style):

* **Q, K, V are sharded by head.** For Qwen2.5-72B with `n_heads = 64`, `n_kv_heads = 8`:

```
TP=2:  GPU0 gets 32 Q heads + 4 KV heads
       GPU1 gets 32 Q heads + 4 KV heads
TP=4:  each GPU gets 16 Q heads + 2 KV heads
TP=8:  each GPU gets  8 Q heads + 1 KV head   ← matches n_kv_heads exactly
TP=16: cannot evenly split 8 KV heads — KV gets replicated on some pairs
```

The natural TP degree is **whatever divides `n_kv_heads`** evenly. Qwen2.5-72B's 8 KV heads makes TP=8 the largest "clean" split. Above TP=8 you must duplicate KV across GPU pairs, which wastes VRAM but is still done in practice for latency reasons.

* **Output projection (W_O) is sharded by input.** Each GPU produces a partial output that gets summed across the TP group with **AllReduce**.

**2.2 TP on FFN**

* **Gate and Up are sharded by output (column-parallel).** Each GPU holds `intermediate / TP` columns. After SwiGLU, each GPU has a slice of the intermediate vector.
* **Down is sharded by input (row-parallel).** Each GPU multiplies its slice of the intermediate by its slice of W_down. Partials get summed across the TP group with **AllReduce**.

So per layer, you have **two AllReduce operations** (one after attention's W_O, one after FFN's W_down), each on a vector of size `d_model = 8192`.

</details>

### 2.3 热点路径中的 NCCL 带宽

```
Per token, per layer:
  AllReduce post-attention:  8192 floats × 2 bytes = 16 KB
  AllReduce post-FFN:        8192 floats × 2 bytes = 16 KB
                                                   = 32 KB / layer
× 80 layers                                        = 2.56 MB / token
```

在 50 tok/s 的目标下，这是 128 MB/s —— 轻松落在 NVLink-4（每对 900 GB/s）甚至 PCIe Gen4 x16（约 32 GB/s）之内。

**但** AllReduce 存在延迟下限。在 NVLink-4 上，一次 16 KB 的 AllReduce 耗时约 10 µs。在纯 PCIe（8 × L40S，无 NVLink）上，可能耗时 50–100 µs。乘以每层 2 次操作 × 80 层：

* NVLink-4：每个 token 约 1.6 ms 延迟下限
* NVLink-3（A100）：约 2.4 ms
* 纯 PCIe（L40S）：8–16 ms

**在 NVLink 机器上，集合通信延迟下限把你限制在约 600 tok/s。** 远高于你实际能跑到的任何吞吐。在纯 PCIe 的 L40S 上，这个下限把你限制在约 60-120 tok/s —— 当你把栈的其余部分往死里优化时，这就**可能**有影响。

### 2.4 面向激活值的序列并行

一个微妙的 TP 优化：在 AllReduce 点之间，每块 GPU 只需要激活值张量的一个切片（因为下一个算子是列并行的）。**序列并行**把 AllReduce 变成 AllGather + ReduceScatter 的一对操作，从而让每块 GPU 每步的激活值内存正比于 `1/TP`，而不是完整的 `d_model`。

对长上下文、高 batch 下的 Qwen2.5-72B，这是实实在在的内存收益。vLLM、TRT-LLM 和 DeepSpeed-Inference 都支持；是否默认开启取决于版本。值得核实。

---

## 3. 流水线并行：把 layer 拆分到多块 GPU 上

流水线并行（PP）把整个 Transformer 块放到不同的 GPU 上：

```
PP=2:  GPU0 = layers 0..39     GPU1 = layers 40..79
PP=4:  GPU0 = 0..19   GPU1 = 20..39   GPU2 = 40..59   GPU3 = 60..79
```

优点：
- layer 计算期间没有集合通信；相邻 stage 之间只有点对点交接。
- 每个 stage 持有的权重更少 —— 每块 GPU 的内存压力更低。

缺点：
- **延迟按串行叠加。** decode（逐 token 生成阶段）一个 token 需要 N 次 stage 往返。
- 对于 batch=1 的 decode，流水线**严格慢于**张量并行，因为没有 batch 维度来填充气泡。
- 经典的「填满流水线」只在 batch >> stage 数时有效，那属于吞吐模式的推理服务。

**建议：** 对 Qwen2.5-72B 推理，节点内用 TP，跨节点可选 PP。绝不要在单个 4 卡或 8 卡的 NVLink 域内用 PP，除非你有非常特定的理由（而这类理由往往最后被证明是错的）。

---

## 4. 生产机器的推荐配置表（recipe）

| 硬件 | 推荐的 TP/PP | 原因 |
|---|---|---|
| **4 × H100 SXM 80 GB** | TP=4（单节点） | 最干净的方案；权重约 37 GB/GPU；NVLink-4 集合通信快 |
| **8 × H100 SXM 80 GB** | TP=8 | 使 KV head 复制数 = 1；批容量最高 |
| **8 × A100 80 GB SXM** | TP=8 | 逻辑与 H100 相同；集合通信略慢 |
| **4 × A100 40 GB** | TP=4 | 偏紧 —— 权重约 37 GB/GPU，长 ctx 下激活值把你推到接近上限 |
| **8 × L40S 48 GB** | PCIe 上 TP=8 且启用 NCCL P2P | 性价比高，但要注意集合通信延迟下限 |
| **2 × node × 4 × H100** | 节点内 TP=4，跨节点 PP=2 | 仅在 batch >> 16 时有用 |
| **4 × MI300X 192 GB** | TP=4 | ROCm + Infinity Fabric；截至 2026 年 vLLM-ROCm 可用 |

---


<details>
<summary>English original</summary>

**2.3 NCCL bandwidth in the hot path**

```
Per token, per layer:
  AllReduce post-attention:  8192 floats × 2 bytes = 16 KB
  AllReduce post-FFN:        8192 floats × 2 bytes = 16 KB
                                                   = 32 KB / layer
× 80 layers                                        = 2.56 MB / token
```

At 50 tok/s target this is 128 MB/s — trivially within NVLink-4 (900 GB/s per pair) or even PCIe Gen4 x16 (~32 GB/s).

**But** AllReduce has a latency floor. On NVLink-4 a 16 KB AllReduce takes ~10 µs. On PCIe-only (8 × L40S without NVLink), it can take 50–100 µs. Times 2 ops per layer × 80 layers:

* NVLink-4: ~1.6 ms latency floor per token
* NVLink-3 (A100): ~2.4 ms
* PCIe-only (L40S): 8–16 ms

**On NVLink boxes the collective latency floor caps you at ~600 tok/s.** Far above any actual throughput you'd hit. On PCIe-only L40S, the floor caps you at ~60-120 tok/s — which **can** matter when you're optimizing the rest of the stack hard.

**2.4 Sequence parallelism for the activations**

A subtle TP optimization: between the AllReduce points, each GPU only needs a slice of the activation tensor (since the next op is column-parallel). **Sequence parallelism** turns the AllReduce into an AllGather + ReduceScatter pair, which keeps each GPU's per-step activation memory proportional to `1/TP` rather than the full `d_model`.

For Qwen2.5-72B at long context with high batch, this is a real memory win. vLLM, TRT-LLM, and DeepSpeed-Inference all support it; whether they enable it by default is version-dependent. Worth checking.

---

**3. Pipeline Parallelism: Split Layers Across GPUs**

Pipeline parallel (PP) puts entire transformer blocks on different GPUs:

```
PP=2:  GPU0 = layers 0..39     GPU1 = layers 40..79
PP=4:  GPU0 = 0..19   GPU1 = 20..39   GPU2 = 40..59   GPU3 = 60..79
```

Pros:
- No collectives during the layer computation; only point-to-point handoff between adjacent stages.
- Each stage holds fewer weights — lower per-GPU memory pressure.

Cons:
- **Latency adds in series.** Decoding one token requires N stage roundtrips.
- For batch=1 decode, a pipeline is **strictly slower** than tensor parallel because there's no batch dimension to fill bubbles.
- The classic "fill the pipeline" only works with batch >> stages, which is throughput-mode serving.

**Recommendation:** for Qwen2.5-72B inference, use TP within a node, optionally PP across nodes. Never use PP within a single 4-or-8-GPU NVLink island unless you have a very specific reason (which usually turns out to be wrong).

---

**4. Recipe Table for Production Boxes**

| Hardware | Recommended TP/PP | Why |
|---|---|---|
| **4 × H100 SXM 80 GB** | TP=4 (one node) | Cleanest setup; weights ~37 GB/GPU; NVLink-4 fast collectives |
| **8 × H100 SXM 80 GB** | TP=8 | Lets KV head replication = 1; highest batch capacity |
| **8 × A100 80 GB SXM** | TP=8 | Same logic as H100; slightly slower collectives |
| **4 × A100 40 GB** | TP=4 | Tight — weights are ~37 GB/GPU, activations push you near limit at long ctx |
| **8 × L40S 48 GB** | TP=8 over PCIe + NCCL P2P enabled | Cost-effective but watch collective latency floor |
| **2 × node × 4 × H100** | TP=4 in-node, PP=2 across | Useful only for batch >> 16 |
| **4 × MI300X 192 GB** | TP=4 | ROCm + Infinity Fabric; vLLM-ROCm works as of 2026 |

---

</details>

## 5. decode 单个 token —— 带注释的墙钟时间

在 4 × H100 SXM 上取 TP=4、batch=1、ctx=2 k。单 token 墙钟时间拆解（近似）：

```
Step                                  Time     What dominates
─────────────────────────────────────────────────────────────
Token embedding lookup                 5 µs    L2 cache hit
Per layer × 80:
  RMSNorm + residual                  10 µs    GPU compute
  QKV GEMV (fused, sharded)           45 µs    HBM bandwidth (W_QKV slice)
  RoPE on Q,K                          5 µs    GPU compute
  KV append + FlashAttention-decode   80 µs    HBM bandwidth (KV cache slice)
  AllReduce post-W_O                  10 µs    NVLink (small payload)
  RMSNorm + residual                  10 µs    GPU compute
  Gate+Up GEMV (fused, sharded)       95 µs    HBM (intermediate matrices)
  SwiGLU                               5 µs    GPU compute
  Down GEMV (sharded)                 95 µs    HBM
  AllReduce post-W_down               10 µs    NVLink
                                  ─────────
                                     365 µs / layer

× 80 layers                       =  29.2 ms
LM head GEMV (sharded)               850 µs    HBM (huge W_lm_head slice)
Softmax + sample                      30 µs
                                  ─────────
Total per token                  ≈   30.1 ms = 33 tok/s
```

这就是 H100 上一套合格 runtime 在稳态单流下应达到的数字。vLLM 在 2026 年中的报告值是该配置下约 30–38 tok/s；带自定义 AllReduce kernel 的 TRT-LLM 在已发表的 benchmark 中达到约 42 tok/s。

对于 **batch=8**，每流每 token 的开销几乎不变（此时受权重带宽限制，而权重在整个批内被复用）。因此总吞吐升到约 250+ tok/s。

---

## 6. 连续批处理与 Paged Attention

在生产 runtime 中，"每秒 8 token" 与 "每秒 300 token" 的 Qwen2.5-72B 推理服务之间最大的差别，就是 **带 paged attention 的连续批处理**（出自 vLLM 论文，如今已是基本要求）。

### 6.1 静态批处理为何失效

朴素批处理：收集一批 N 个请求，一起执行直到 **最慢的那个完成**，再启动下一批。有两个致命问题：

- 各请求的完成长度差异极大。
- 每个批步骤都受最长序列剩余 token 数的制约。

结果：线上流量下 GPU 利用率 < 30%。

### 6.2 连续批处理

把每个请求视为一串 decode 步骤。每一步中，在旧请求结束的同时 **插入新请求** 到批里。前向传播作用在一个 "参差不齐" 的批上，其中各序列的长度与存活时长都任意。

### 6.3 Paged attention

朴素的 KV cache 实现会为每个序列预先分配 `[max_seq_len × n_kv_heads × head_dim]` —— **在短序列上浪费内存**。Paged attention 把 KV cache 按固定大小的 **block** 管理（通常 16 个 token），并为每个序列维护一张 **block table**，把逻辑位置映射到物理 block。

```
Logical KV for sequence A:  [block_3][block_7][block_9][block_2]
Logical KV for sequence B:  [block_4][block_1][block_8]

Physical block pool: { 1: ..., 2: ..., 3: ..., 4: ..., ... }
```

序列结束时，它的 block 归还到池中。新序列按需分配 block。实践中这样可以得到接近 100% 的 KV cache 利用率。

对 kernel 的影响：FlashAttention-decode 在读取 K 和 V 时必须沿着 block table 走。为此有专门的自定义 kernel 变体 —— vLLM 自带一套，TRT-LLM 有一套，SGLang 也有一套。

---

## 7. 长上下文 —— YaRN 与分块 prefill

Qwen2.5-72B 的原生上下文是 32 k。借助 `rope_scaling: yarn`，可扩展到 131 k。

### 7.1 推理时的 YaRN

推理时，YaRN 的 "生效" 方式是：
1. 为 `i = 0..d_head/2` 计算旋转频率 `theta_i = 1e6 ^ (-2i/d_head)`。
2. 用一个与位置相关的因子对这些频率做 **缩放**，该因子在原生范围（不缩放）与扩展范围（对数缩放）之间平滑插值。
3. 用缩放后的频率施加 RoPE。

不需要额外的权重，也没有推理时的开销。runtime 只需在启动时算出正确的 `cos / sin` 表。

**注意：** 那些把 "max context = 32 k" 写死、又不检查 `rope_scaling` 的 runtime 会静默截断。确认你的配置开关是 `--rope-scaling yarn` 或等价设置。

### 7.2 分块 prefill

把一个 100 k token 的 prompt 作为单个 GEMM 做 prefill，需要约 100 GB 的激活值内存 —— 太多了。把 prefill 切成例如每块 2048 个 token，顺序处理，KV 逐步增长。

实现细节：做 decode 步 `seq_len=1` attention 的同一个 kernel 可以复用于 prefill 分块 —— 该块的 query 会 attend 到目前累积的全部 KV。泛化的 FlashAttention 用同一种 kernel 形态实现这一点。

vLLM、SGLang 和 TRT-LLM 都自动做分块 prefill。是否把它与正在进行的 decode 步骤重叠执行（这能改善推理服务的 TTFT）由各 runtime 自行决定。

---


<details>
<summary>English original</summary>

**5. Decoding One Token — The Annotated Wall Clock**

Take TP=4 on 4 × H100 SXM, batch=1, ctx=2 k. Per-token wall clock breakdown (approximate):

```
Step                                  Time     What dominates
─────────────────────────────────────────────────────────────
Token embedding lookup                 5 µs    L2 cache hit
Per layer × 80:
  RMSNorm + residual                  10 µs    GPU compute
  QKV GEMV (fused, sharded)           45 µs    HBM bandwidth (W_QKV slice)
  RoPE on Q,K                          5 µs    GPU compute
  KV append + FlashAttention-decode   80 µs    HBM bandwidth (KV cache slice)
  AllReduce post-W_O                  10 µs    NVLink (small payload)
  RMSNorm + residual                  10 µs    GPU compute
  Gate+Up GEMV (fused, sharded)       95 µs    HBM (intermediate matrices)
  SwiGLU                               5 µs    GPU compute
  Down GEMV (sharded)                 95 µs    HBM
  AllReduce post-W_down               10 µs    NVLink
                                  ─────────
                                     365 µs / layer

× 80 layers                       =  29.2 ms
LM head GEMV (sharded)               850 µs    HBM (huge W_lm_head slice)
Softmax + sample                      30 µs
                                  ─────────
Total per token                  ≈   30.1 ms = 33 tok/s
```

That's the steady-state single-stream number you should expect from a competent runtime on H100. vLLM in mid-2026 reports ~30–38 tok/s on this configuration; TRT-LLM with custom AllReduce kernels has shipped ~42 tok/s in published benchmarks.

For **batch=8**, the cost per-token-per-stream stays nearly the same (you're bandwidth-bound on weights, which are reused across the batch). So aggregate throughput goes to ~250+ tok/s.

---

**6. Continuous Batching and Paged Attention**

The single biggest production runtime difference between "8-tokens-per-second" and "300-tokens-per-second" Qwen2.5-72B serving is **continuous batching with paged attention** (the vLLM paper, now table stakes).

**6.1 Why static batching fails**

Naive batching: collect a batch of N requests, run them together until the **slowest finishes**, then start the next batch. Two killers:

- Requests have wildly different completion lengths.
- Each batch step is bounded by the longest sequence's remaining tokens.

The result: GPU utilization < 30% on production traffic.

**6.2 Continuous batching**

Treat each request as a sequence of decoding steps. At each step, **insert new requests** into the batch as old ones finish. The forward pass operates on a "ragged" batch where sequences have arbitrary lengths and ages.

**6.3 Paged attention**

The naive KV-cache implementation allocates `[max_seq_len × n_kv_heads × head_dim]` per sequence up front — **wastes memory on short sequences**. Paged attention manages the KV cache as fixed-size **blocks** (typical: 16 tokens) and has a per-sequence **block table** mapping logical positions to physical blocks.

```
Logical KV for sequence A:  [block_3][block_7][block_9][block_2]
Logical KV for sequence B:  [block_4][block_1][block_8]

Physical block pool: { 1: ..., 2: ..., 3: ..., 4: ..., ... }
```

When a sequence finishes, its blocks go back into the pool. New sequences allocate blocks as needed. This gives near-100% KV-cache utilization in practice.

The kernel impact: FlashAttention-decode has to follow the block table during the K and V reads. There's a custom kernel variant for this — vLLM ships their own, TRT-LLM has its own, SGLang has its own.

---

**7. Long Context — YaRN and Chunked Prefill**

Qwen2.5-72B's native context is 32 k. With `rope_scaling: yarn`, it extends to 131 k.

**7.1 YaRN at inference time**

At inference, YaRN is "applied" by:
1. Computing rotary frequencies `theta_i = 1e6 ^ (-2i/d_head)` for `i = 0..d_head/2`.
2. **Scaling** those frequencies by a position-dependent factor that smoothly interpolates between native-range (no scaling) and extended-range (logarithmic scaling).
3. Applying RoPE with the scaled frequencies.

No additional weights, no inference-time cost. The runtime just needs to compute the right `cos / sin` table at startup.

**Catch:** runtimes that hard-code "max context = 32 k" without checking `rope_scaling` will silently truncate. Verify your config flag is `--rope-scaling yarn` or equivalent.

**7.2 Chunked prefill**

Prefilling a 100 k-token prompt as one GEMM uses ~100 GB of activation memory — too much. Split prefill into chunks of e.g. 2048 tokens, process sequentially, KV grows incrementally.

Implementation detail: the same kernel that does decode-step `seq_len=1` attention can be reused for prefill chunks — the chunk's queries attend to all of the KV accumulated so far. Generalized FlashAttention does this in one kernel form.

vLLM, SGLang, and TRT-LLM all do chunked prefill automatically. Whether they overlap it with ongoing decode steps (which improves serving TTFT) is a per-runtime decision.

---

</details>

## 8. 2026 年选择生产 runtime

| Runtime | 擅长 | 备注 |
|---|---|---|
| **vLLM** | 兼容 OpenAI API 的推理服务、模型支持面广、AWQ/GPTQ | 默认之选。量化模型用 Marlin kernel。 |
| **SGLang** | 程序化生成控制、结构化输出 | 在带缓存前缀的工作负载上优于 vLLM |
| **TensorRT-LLM** | 原始吞吐最高、仅限 NVIDIA、运维更难 | 已用于生产规模（集成 NVIDIA Triton-LLM） |
| **DeepSpeed-Inference (MII)** | 微软技术栈集成 | 2024 年后式微，但企业仍在用 |
| **LMDeploy (TurboMind)** | 专为 Qwen 最优 —— 源自 InternLM 团队 | 历史上在 Qwen2.5-72B 上开箱数字最好 |
| **vLLM-ROCm / Aiter** | 支持 MI300X | 吞吐正在追赶 CUDA 路径 |

具体到 Qwen2.5-72B，InternLM/阿里巴巴生态为 **LMDeploy** 发布了优化 recipe，在 Qwen 模型上稳定比其他 runtime 快 15–25%。如果被锁定在 Qwen 与 Nvidia 上，在选定 vLLM 之前先评测 LMDeploy。

---

## 9. 实战部署 recipe —— 4 × H100 SXM

```bash
# Pull the model
huggingface-cli download Qwen/Qwen2.5-72B-Instruct --local-dir ./qwen72b

# vLLM
docker run --gpus all --ipc=host -p 8000:8000 \
  -v $(pwd)/qwen72b:/model \
  vllm/vllm-openai:latest \
  --model /model \
  --tensor-parallel-size 4 \
  --max-model-len 32768 \
  --gpu-memory-utilization 0.92 \
  --enable-chunked-prefill \
  --rope-scaling '{"type":"yarn","factor":4.0,"original_max_position_embeddings":32768}' \
  --served-model-name qwen2.5-72b
```

2026 年年中一次干净的 vLLM 部署，预期数字如下：

| 指标 | 数值 |
|---|---|
| 单流 decode（逐 token 生成阶段） | ~30 tok/s |
| Batch=8 聚合 decode | ~220 tok/s |
| Batch=32 聚合 decode | ~560 tok/s |
| 首 token 时延 @ 2 k prompt | ~250 ms |
| 首 token 时延 @ 32 k prompt（chunked） | ~2.5 s |
| 单 GPU 峰值 VRAM | ~75 GB（共 80） |

如果与这些数字相差明显，常见嫌疑如下：
- **NCCL 未走 NVLink** —— 检查 `NCCL_P2P_LEVEL=NVL`，并确认 `nvidia-smi topo -m` 显示四块 GPU 之间都是 NVLink。
- **chunked prefill（首字前的整段计算）未启用** —— 需显式开启。
- vLLM 中 **CUDA Graphs 被禁用**（`--enforce-eager` 不应被设置）。
- **GPU 未跑满功耗** —— 满载时 `nvidia-smi -q -d POWER` 应显示约 700 W/GPU。

---

## 10. AMD MI300X 路线

从简，因为用户没有问，但它会改变打法：

* 截至 2026 年，**vLLM-ROCm** 的吞吐已追到 CUDA 路径的约 85%。
* MI300X 每 GPU 有 192 GB HBM —— Qwen2.5-72B FP16 能放进 **一块** GPU。纯 TP=1 推理即可跑通。
* 对延迟敏感的推理服务，跨两块 MI300X 做 TP=2 仍有益，因为即便在 192 GB 的卡上，瓶颈也是单 GPU 带宽。
* Infinity Fabric 提供约 896 GB/s 的点对点带宽 —— 聚合上可与 NVLink-4 竞争。

---

## 动手练习

1. **TP 度扫描。** 在 4×H100 或 8×A100 的机器上，以 TP=2、4 以及（若有 8 卡）8 运行 Qwen2.5-72B。测量单流 decode 与 batch=16 聚合 decode。确认单流 tok/s 在 TP=8 附近饱和，且批吞吐随 TP 近似线性扩展，直到触及 NCCL 带宽上限。

2. **NCCL 集合通信 trace。** 运行 `NCCL_DEBUG=INFO NCCL_DEBUG_SUBSYS=COLL python serve.py`，找出每生成一个 token 时 AllReduce 调用的大小与次数。确认它们等于 2 × n_layers × `d_model` × 2 bytes。

3. **chunked 与 unchunked prefill 对比。** 在 32 k 的 prompt 上，分别带 `--enable-chunked-prefill` 与不带运行。测量首 token 时延与峰值 VRAM。量化收益。

4. **YaRN 一致性测试。** 在 64 k token 的 prompt **末尾**（带 `--rope-scaling yarn`）生成 100 token 的续写。与在 4 k token prompt 末尾做同样的生成作对比。64 k 的情况仍应产出连贯、合乎语法的输出。若做不到，说明 YaRN 配置未生效 —— 检查 runtime 的开关。

5. **vLLM 对比 LMDeploy。** 在同一硬件上分别用两个 runtime 部署 Qwen2.5-72B。跑一次相同的 5 分钟负载测试（例如 50 个并发用户、混合 prompt 长度）。比较聚合 tok/s、p95 延迟与首 token 时延。找出哪类工作负载更适合哪个 runtime。

6. **Paged attention 的 KV 利用率。** 在真实负载测试中观察 `vllm metrics`（Prometheus 端点）。确认在连续批处理下 KV 缓存利用率升到 >90%。若低于 50%，说明批太小或 paged attention 配置有误。

---


<details>
<summary>English original</summary>

**8. Choosing a Production Runtime in 2026**

| Runtime | Best at | Notes |
|---|---|---|
| **vLLM** | OpenAI-API-compatible serving, broad model support, AWQ/GPTQ | The default. Marlin kernels for quantized models. |
| **SGLang** | Programmatic generation control, structured output | Beats vLLM on cached-prefix workloads |
| **TensorRT-LLM** | Best raw throughput, NVIDIA-only, harder ops | Used at production scale (NVIDIA Triton-LLM integration) |
| **DeepSpeed-Inference (MII)** | Microsoft-stack integration | Faded since 2024 but still used in enterprise |
| **LMDeploy (TurboMind)** | Optimal for Qwen specifically — InternLM-team origin | Best out-of-box numbers on Qwen2.5-72B historically |
| **vLLM-ROCm / Aiter** | MI300X support | Catching up to CUDA path in throughput |

For Qwen2.5-72B specifically, the InternLM/Alibaba ecosystem has shipped optimized recipes for **LMDeploy** that consistently outperform other runtimes by 15–25% on Qwen models. If you're locked to Qwen and Nvidia, evaluate LMDeploy before committing to vLLM.

---

**9. Practical Deployment Recipe — 4 × H100 SXM**

```bash
# Pull the model
huggingface-cli download Qwen/Qwen2.5-72B-Instruct --local-dir ./qwen72b

# vLLM
docker run --gpus all --ipc=host -p 8000:8000 \
  -v $(pwd)/qwen72b:/model \
  vllm/vllm-openai:latest \
  --model /model \
  --tensor-parallel-size 4 \
  --max-model-len 32768 \
  --gpu-memory-utilization 0.92 \
  --enable-chunked-prefill \
  --rope-scaling '{"type":"yarn","factor":4.0,"original_max_position_embeddings":32768}' \
  --served-model-name qwen2.5-72b
```

Expected numbers from a clean vLLM deployment in mid-2026:

| Metric | Value |
|---|---|
| Single-stream decode | ~30 tok/s |
| Batch=8 aggregate decode | ~220 tok/s |
| Batch=32 aggregate decode | ~560 tok/s |
| TTFT @ 2 k prompt | ~250 ms |
| TTFT @ 32 k prompt (chunked) | ~2.5 s |
| Peak VRAM/GPU | ~75 GB (out of 80) |

If you're significantly off these, the usual suspects:
- **NCCL not using NVLink** — check `NCCL_P2P_LEVEL=NVL` and that `nvidia-smi topo -m` shows NVLink between all four GPUs.
- **Chunked prefill disabled** — explicitly enable.
- **CUDA Graphs disabled** in vLLM (`--enforce-eager` should NOT be set).
- **GPU not at full power** — `nvidia-smi -q -d POWER` should show ~700 W/GPU at saturation.

---

**10. AMD MI300X Path**

Brief because the user didn't ask, but it changes the playbook:

* **vLLM-ROCm** has caught up to ~85% of CUDA path throughput as of 2026.
* MI300X holds 192 GB HBM per GPU — Qwen2.5-72B FP16 fits in **one** GPU. Pure TP=1 inference works.
* For latency-sensitive serving, TP=2 across two MI300X still helps because per-GPU bandwidth is the limiter even on 192 GB cards.
* Infinity Fabric provides ~896 GB/s peer-to-peer — competitive with NVLink-4 in aggregate.

---

**Hands-On Exercises**

1. **TP-degree sweep.** On a 4×H100 or 8×A100 box, run Qwen2.5-72B at TP=2, 4, and (if 8 available) 8. Measure single-stream decode and batch=16 aggregate decode. Confirm single-stream tok/s saturates near TP=8 and batch throughput scales nearly linearly with TP up to NCCL bandwidth limits.

2. **NCCL collective trace.** Run `NCCL_DEBUG=INFO NCCL_DEBUG_SUBSYS=COLL python serve.py` and identify, for each decoded token, the size and count of AllReduce calls. Confirm they match 2 × n_layers × `d_model` × 2 bytes.

3. **Chunked vs unchunked prefill.** On a 32 k prompt, run with `--enable-chunked-prefill` and without. Measure TTFT and peak VRAM. Quantify the win.

4. **YaRN coherence test.** Generate a 100-token completion at the **end** of a 64 k-token prompt (with `--rope-scaling yarn`). Compare to the same generation at the end of a 4 k-token prompt. The 64 k case should still produce coherent, grammatical output. If it doesn't, your YaRN config isn't being applied — check the runtime flag.

5. **vLLM vs LMDeploy.** Deploy Qwen2.5-72B on the same hardware under both runtimes. Run an identical 5-minute load test (e.g., 50 concurrent users, mixed prompt lengths). Compare aggregate tok/s, p95 latency, and TTFT. Identify which workload class favors which runtime.

6. **Paged-attention KV utilization.** Watch `vllm metrics` (Prometheus endpoint) during a real load test. Confirm KV-cache utilization climbs to >90% with continuous batching. If it's < 50%, you're under-batched or paged attention is misconfigured.

---

</details>

## 关键要点

| 要点 | 为何重要 |
|---|---|
| 145 GB FP16 权重 — TP 必需 | 单 GPU 放不下；这是一道系统工程题 |
| TP=8 是自然并行度（n_kv_heads=8） | 无需复制即可干净地切分 KV |
| 每层两次 AllReduce，作用在小张量上 | NVLink 的延迟下限比带宽更要紧 |
| 只有当 batch >> stages 时 PP 才有用 | 在同一个 NVLink 域内，TP 胜出 |
| 连续批处理 + paged attention 没有商量余地 | 相对静态批处理有 3-10× 的吞吐倍增 |
| YaRN 在权重不变的前提下给出 131 k 上下文 | 确认你的 runtime 确实启用了它 |
| 在 Qwen 模型上，LMDeploy 往往专门胜过 vLLM | 定型前把两者都测一遍 |

---

## 资源

* **[vLLM 论文 — 用 PagedAttention 为 LLM 推理服务做高效内存管理](https://arxiv.org/abs/2309.06180):** 连续批处理 + paged attention 的参考实现。
* **[Megatron-LM TP 论文](https://arxiv.org/abs/1909.08053):** 张量并行的原始设计。
* **[FlashAttention-2 论文](https://arxiv.org/abs/2307.08691):** 推理 kernel 参考。
* **[FasterTransformer / TRT-LLM kernel 指南](https://github.com/NVIDIA/TensorRT-LLM):** 生产部署用的自定义 AllReduce 与 decode kernel。
* **[NCCL 调优指南](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/index.html):** P2P 配置、算法选择。
* **[LMDeploy / TurboMind](https://github.com/InternLM/lmdeploy):** 面向 Qwen 优化的推理引擎。
* **[YaRN 论文](https://arxiv.org/abs/2309.00071):** 上下文扩展的数学推导。
* **[Qwen2.5 技术报告](https://arxiv.org/abs/2412.15115):** 原始发布版，含配置与 benchmark。
* **[vLLM-ROCm](https://github.com/ROCm/vllm):** AMD 路径。
* **[阶段 5 — GPU 基础设施 — NCCL 深度解析](../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/08-NCCL深度探索/README.md):** 关于集合通信层的配套深度解析。


<details>
<summary>English original</summary>

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| 145 GB FP16 weights — TP is mandatory | No single GPU; this is a system-engineering exercise |
| TP=8 is the natural degree (n_kv_heads=8) | Cleanly partitions KV without replication |
| Two AllReduces per layer, on tiny tensors | NVLink latency floor matters more than bandwidth |
| PP only helps when batch >> stages | Within an NVLink island, TP wins |
| Continuous batching + paged attention is non-negotiable | 3-10× throughput multiplier over static batching |
| YaRN gives you 131 k context with no weight changes | Verify your runtime applies it |
| LMDeploy often beats vLLM on Qwen models specifically | Test both before locking in |

---

**Resources**

* **[vLLM paper — Efficient Memory Management for LLM Serving with PagedAttention](https://arxiv.org/abs/2309.06180):** The continuous-batching + paged-attention reference.
* **[Megatron-LM TP paper](https://arxiv.org/abs/1909.08053):** Original tensor parallelism design.
* **[FlashAttention-2 paper](https://arxiv.org/abs/2307.08691):** Inference kernel reference.
* **[FasterTransformer / TRT-LLM kernel guides](https://github.com/NVIDIA/TensorRT-LLM):** Custom AllReduce and decode kernels for production deployments.
* **[NCCL Tuning Guide](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/index.html):** P2P configuration, algorithm selection.
* **[LMDeploy / TurboMind](https://github.com/InternLM/lmdeploy):** Qwen-optimized inference engine.
* **[YaRN paper](https://arxiv.org/abs/2309.00071):** The context-extension math.
* **[Qwen2.5 technical report](https://arxiv.org/abs/2412.15115):** Original release with config and benchmarks.
* **[vLLM-ROCm](https://github.com/ROCm/vllm):** AMD path.
* **[Phase 5 — GPU Infrastructure — NCCL Deep Dive](../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/08-NCCL深度探索/README.md):** The companion deep dive on the collective layer.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Qwen Inference Optimization/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Qwen%20Inference%20Optimization/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
