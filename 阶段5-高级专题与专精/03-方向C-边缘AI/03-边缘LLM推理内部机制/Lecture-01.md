---
title: 第 1 讲：边缘大语言模型推理内部机制 —— GEMV（矩阵-向量乘）Decode（逐 token 生成阶段）、QKV 投影与内存带宽墙
description: 第 1 讲：边缘大语言模型推理内部机制 —— GEMV（矩阵-向量乘）Decode（逐 token 生成阶段）、QKV 投影与内存带宽墙
---

# 第 1 讲：边缘大语言模型推理内部机制 —— GEMV（矩阵-向量乘）Decode（逐 token 生成阶段）、QKV 投影与内存带宽墙

## 概述

当 Jetson 级边缘设备从大语言模型生成 token 时，你关心的几乎所有东西 —— 延迟、功耗、热管理 —— 都由一个极不显眼的操作决定：对**量化权重**做 **GEMV**（一般矩阵-向量乘）。

数据中心那套经验让你用 TFLOPS 和 张量核心来思考；但在 Orin Nano 上，那套经验完全是谎言。边缘大语言模型 decode 是**内存带宽受限**，而非算力受限，芯片工程师的任务是**每 token 搬移更少字节**，而不是增加更多 MAC。

本讲从一个观察出发 —— 一行 runtime 日志，类似

```
[GEMV #0] type=12 M=2560 K=2560
[embd]   Q6_K dequant token 9707
```

—— 然后把它展开成完整的心智模型：GEMV 是什么，为什么它主导 decode 而不是 prefill（首字前的整段计算），`M=K=2560` 形状说明了模型的什么，QKV 投影在该算术中处于什么位置，为什么存在 Q4_K / Q6_K 这类量化格式，以及具体的 Jetson 调节项如何决定你得到 0.04 tok/s 还是 40 tok/s。

读完本讲，你应能：

* 读懂 llama.cpp / mlc-llm / ggml 的 GEMV trace，并识别每一行属于哪个 Transformer 块。
* 计算给定模型的每 token 字节流量，并解释为什么 decode 是带宽受限。
* 在 Orin Nano vs Orin AGX vs H100 的 roofline（性能上界模型）图上标出 QKV 投影、FFN gate/up/down 和 LM head。
* 用 `nvpmodel`、`jetson_clocks`、`tegrastats` 和 CUDA 性能剖析诊断 "GPU at 0 MHz" 故障。
* 在 Q4_K_M、Q5_K_M、Q6_K、AWQ、GPTQ 和 FP16 之间做选择，*依据带宽*，而不只是困惑度。

---

## 1. GEMM（矩阵-矩阵乘）vs GEMV —— 为什么 Decode 是另一回事

Transformer 前向传播是一串**线性投影**，夹在 attention 和 FFN 之间。数学上，prefill 和 decode 运行的是同一个算子：

```
y = W · x
```

变化的是 **`x`**：

| 阶段 | `x` 形状        | 算子类型 | 算术强度 (flops/byte) |
|---|---|---|---|
| Prefill（处理 prompt） | `[seq_len × d]`     | **GEMM** | 高 —— ~`2·seq_len` |
| Decode（生成 1 token） | `[1 × d]`          | **GEMV** | 低 —— ~`2`             |

算术强度是决定性数字。在 Orin Nano（Ampere 架构，1024 个 CUDA 核心，~50 GB/s LPDDR5 带宽）上，**roofline 拐点**位于 30–40 flops/byte 附近。prefill 期间的 GEMM 以数量级越过该门槛，位于算力受限区间。decode 期间的 GEMV 卡在 2 flops/byte 以下 —— 它位于**带宽受限**区间，在那里增加张量核心毫无作用。

实际后果：一台具有 50 GB/s DRAM 带宽的 Jetson Orin Nano，在 INT4（约 3.5 GB 权重）下 decode 一个 7 B 参数模型，理论上可以做到

```
50 GB/s / 3.5 GB/token = ~14 tokens/s
```

这是一个**硬上限**。只有通过每 token 搬移更少字节才能突破它（更小的模型、更低比特量化、KV 缓存压缩、通过投机/并行 decode 在已解码 token 间复用权重）。

---


<details>
<summary>English original</summary>

**Lecture 1: Edge LLM Inference Internals — GEMV Decode, QKV Projections, and the Memory-Bandwidth Wall**

**Overview**

When a Jetson-class edge device generates tokens from an LLM, almost everything you care about — latency, power, thermal — is decided by a single, deeply unglamorous operation: **GEMV** (general matrix–vector multiply) over **quantized weights**. Datacenter folklore tells you to think in TFLOPS and tensor cores; that folklore actively lies about what happens on an Orin Nano. Edge LLM decode is **memory-bandwidth-bound**, not compute-bound, and the silicon engineer's job is to **move fewer bytes per token**, not to add more MACs.

This lecture takes a single observation — a runtime log line like

```
[GEMV #0] type=12 M=2560 K=2560
[embd]   Q6_K dequant token 9707
```

— and unpacks it into the full mental model: what GEMV is, why it dominates decode but not prefill, what the `M=K=2560` shape says about the model, where QKV projections sit inside that arithmetic, why quantization formats like Q4_K / Q6_K exist, and what concrete Jetson knobs determine whether you get 0.04 tok/s or 40 tok/s.

By the end you should be able to:

* Read a llama.cpp / mlc-llm / ggml GEMV trace and identify which transformer block each line belongs to.
* Compute the per-token byte traffic for a given model and explain why decode is bandwidth-bound.
* Place QKV projections, FFN gate/up/down, and the LM head on a roofline plot for Orin Nano vs Orin AGX vs H100.
* Diagnose "GPU at 0 MHz" failures with `nvpmodel`, `jetson_clocks`, `tegrastats`, and CUDA profiling.
* Choose between Q4_K_M, Q5_K_M, Q6_K, AWQ, GPTQ, and FP16 *based on bandwidth*, not just perplexity.

---

**1. GEMM vs GEMV — Why Decode Is a Different Animal**

A transformer forward pass is a chain of **linear projections** sandwiched between attention and FFN. Mathematically the same op runs in both prefill and decode:

```
y = W · x
```

What changes is **`x`**:

| Phase | Shape of `x`        | Op type | Arithmetic intensity (flops/byte) |
|---|---|---|---|
| Prefill (process prompt) | `[seq_len × d]`     | **GEMM** | High — ~`2·seq_len` |
| Decode (generate 1 token) | `[1 × d]`          | **GEMV** | Low — ~`2`             |

Arithmetic intensity is the deciding number. On Orin Nano (Ampere, 1024 CUDA cores, ~50 GB/s LPDDR5 bandwidth), the **roofline knee** sits around 30–40 flops/byte. GEMM during prefill clears that bar by an order of magnitude and lives in the compute-bound regime. GEMV during decode is stuck below 2 flops/byte — it lives in the **bandwidth-bound** regime, where adding tensor cores does literally nothing.

The practical consequence: a Jetson Orin Nano with 50 GB/s of DRAM bandwidth, decoding a 7 B-parameter model at INT4 (~3.5 GB of weights), can in principle do

```
50 GB/s / 3.5 GB/token = ~14 tokens/s
```

That is a **hard ceiling**. You only beat it by moving fewer bytes per token (smaller model, lower-bit quant, KV-cache compression, weight reuse across decoded tokens via speculative/parallel decoding).

---

</details>

## 2. 逐行阅读 GEMV（矩阵-向量乘） trace

类似这样的一行：

```
[GEMV #0] type=12 M=2560 K=2560
```

解码为：

| 字段      | 含义                              | 你用它做什么 |
|---|---|---|
| `GEMV`     | 矩阵 × 向量 kernel               | 确认是 decode（逐 token 生成阶段）路径，而非 prefill（首字前的整段计算） |
| `type=12`  | 量化 kernel id（Q4_K、Q5_K、Q6_K…） | 告诉你权重的每元素位宽 |
| `M=2560`   | 输出维度（W 的行数，y 的长度）  | 模型的 hidden size / projection 输出 |
| `K=2560`   | 输入维度（W 的列数，x 的长度）   | 模型的 hidden size 输入 |

对于 `d_model = 2560` decoder-only 大语言模型（Phi-2、Stable LM 3B 变体等使用的形状），每个 Transformer block 对每个生成的 token 发出如下 GEMV 序列：

```
hidden ─► RMSNorm ─► [GEMV] Q  : 2560×2560
                   ├► [GEMV] K  : 2560×2560     (× num_kv_heads / num_heads if GQA)
                   └► [GEMV] V  : 2560×2560     (same)
                              │
                     Rotary embed + KV cache append
                              │
                       attention(Q,K,V)
                              │
                   [GEMV] O proj : 2560×2560
                              │
              residual add ─► RMSNorm
                              │
                ├► [GEMV] FFN gate : 2560×~6912
                └► [GEMV] FFN up   : 2560×~6912
                              │
                        SwiGLU activation
                              │
                   [GEMV] FFN down  : 6912×2560
                              │
                      residual add
```

再乘以 `n_layers`（3 B 级模型约为 32）。然后加上最后一个 **LM head** GEMV —— 通常是 `2560 × vocab_size`，在 32 k 词表上，它可能是整个前向传播中 *单个最大* 的算子。

因此，每个 token 的 GEMV *数量* 大约是 `7 × n_layers + 1`。对于 32 层模型，也就是 **每个 token 225 个 GEMV**。其中每一个都要从 DRAM 重新读取自己的权重矩阵。这就是带宽占主导的原因。

---

## 3. QKV 投影详解

每个 layer 的前三个 GEMV 就是 QKV 投影。概念上：

* **Query (Q)** —— “我在找什么？”
* **Key (K)** —— “我提供什么来被匹配？”
* **Value (V)** —— “如果被匹配，我携带什么内容？”

数学上：

```
Q = x · W_Q
K = x · W_K
V = x · W_V
```

然后 attention：

```
attn = softmax( Q · Kᵀ / √d_k ) · V
```

### 3.1 融合 QKV —— 标准 runtime 技巧

对同一个输入 `x` 做三个独立的 GEMV，会把 `x` 读三次，并付出三次 **kernel 启动延迟**。真实的 runtime 会把它们融合：

```
[W_Q | W_K | W_V]    # concatenated along output dim
        │
        ▼
    QKV = x · W_QKV    # single GEMV, output split into Q,K,V slices
```

在 Jetson GPU 上，这是明显的收益 —— 对 `x` 只做一次全局内存读取，一次启动，输出连续。模型导出打包矩阵时，llama.cpp 会这样做；mlc-llm 则通过其 Relax IR 融合 pass 默认这样做。

trace 会暴露这一点：如果你看到的是一个形状为 `M = 3·d, K = d` 的 GEMV，而不是三个形状为 `M = d, K = d` 的 GEMV，说明你的 runtime 做了融合。这正是你想要的。

### 3.2 分组查询注意力、多查询注意力 —— 当 K 和 V 变小时

现代大语言模型（Llama 3、Mistral、Gemma、Phi-3）使用 **分组查询注意力（GQA）** 或 **多查询注意力（MQA）** 来缩小 K 和 V 投影：

| 变体 | Q heads | K/V heads | W_K、W_V 的输出维度 |
|---|---|---|---|
| MHA（原始） | `n_heads` | `n_heads`           | `d` |
| 分组查询注意力           | `n_heads` | `n_heads / g`（如 8） | `d · n_kv / n_heads` |
| 多查询注意力           | `n_heads` | `1`                 | `d_head` |

这纯粹是 **带宽** 技巧。Q 投影保持全尺寸；K 和 V 按分组查询注意力比例缩小。KV cache（每个 token、每个 layer）相应缩小 —— 这对 **长上下文 decode** 的影响比投影本身更大。

### 3.3 共享 / 移除投影 —— 研究，而非部署

“KV Transformer”（去掉 Q，复用 K）和 “K Transformer”（单个投影同时用作 Q、K 和 V）这类变体存在于研究中。它们以表达能力为代价进一步削减带宽。截至 ICLR 2026，它们尚未进入生产部署 —— 但这正是硬件工程师应该跟踪的那类变化，因为每移除一个投影，每个 layer 每个 token 就少一个 GEMV。

### 3.4 PAMM 与训练时压缩

近期工作如 **Projected Attention Memory Mapping (PAMM)** 在 *训练* 期间将 QKV 激活值压缩最多 512×。这是 **训练内存** 问题，而不是推理问题 —— 你的推理 trace 不受影响。但它间接使在相同硬件上训练的更大模型成为可能，而这些模型随后 *确实* 会以每字节训练成本带来更多参数的方式落到你的边缘设备上。

---


<details>
<summary>English original</summary>

**2. Reading a GEMV Trace Line by Line**

A line like:

```
[GEMV #0] type=12 M=2560 K=2560
```

decodes as:

| Field      | Meaning                              | What you do with it |
|---|---|---|
| `GEMV`     | matrix × vector kernel               | confirms decode path, not prefill |
| `type=12`  | quantization kernel id (Q4_K, Q5_K, Q6_K…) | tells you weight bits-per-element |
| `M=2560`   | output dim (rows of W, length of y)  | model's hidden size / projection out |
| `K=2560`   | input dim (cols of W, length of x)   | model's hidden size in |

For a `d_model = 2560` decoder-only LLM (the shape used by Phi-2, Stable LM 3B variants, etc.), each transformer block emits this sequence of GEMVs per generated token:

```
hidden ─► RMSNorm ─► [GEMV] Q  : 2560×2560
                   ├► [GEMV] K  : 2560×2560     (× num_kv_heads / num_heads if GQA)
                   └► [GEMV] V  : 2560×2560     (same)
                              │
                     Rotary embed + KV cache append
                              │
                       attention(Q,K,V)
                              │
                   [GEMV] O proj : 2560×2560
                              │
              residual add ─► RMSNorm
                              │
                ├► [GEMV] FFN gate : 2560×~6912
                └► [GEMV] FFN up   : 2560×~6912
                              │
                        SwiGLU activation
                              │
                   [GEMV] FFN down  : 6912×2560
                              │
                      residual add
```

Multiply by `n_layers` (≈ 32 for a 3 B-class model). Then add one final **LM head** GEMV — usually `2560 × vocab_size`, which on a 32 k vocab can be the *single largest* op of the entire forward pass.

So the *count* of GEMVs per token is roughly `7 × n_layers + 1`. For a 32-layer model, that's **225 GEMVs per token**. Every one of them re-reads its weight matrix from DRAM. That is why bandwidth dominates.

---

**3. QKV Projections in Detail**

The first three GEMVs of every layer are the QKV projections. Conceptually:

* **Query (Q)** — "what am I looking for?"
* **Key (K)** — "what do I offer to be matched against?"
* **Value (V)** — "what content do I carry if matched?"

Mathematically:

```
Q = x · W_Q
K = x · W_K
V = x · W_V
```

Then attention:

```
attn = softmax( Q · Kᵀ / √d_k ) · V
```

**3.1 Fused QKV — the standard runtime trick**

Three separate GEMVs over the same input `x` read `x` three times and pay three **kernel-launch latencies**. Real runtimes fuse them:

```
[W_Q | W_K | W_V]    # concatenated along output dim
        │
        ▼
    QKV = x · W_QKV    # single GEMV, output split into Q,K,V slices
```

On Jetson GPUs this is a clear win — one global-memory read of `x`, one launch, contiguous output. llama.cpp does this when the model export packs the matrices; mlc-llm does it by default via its Relax IR fusion pass.

The trace gives this away: if you see one GEMV of shape `M = 3·d, K = d` instead of three of shape `M = d, K = d`, your runtime fused. That is what you want.

**3.2 GQA, MQA — when K and V get smaller**

Modern LLMs (Llama 3, Mistral, Gemma, Phi-3) use **Grouped-Query Attention (GQA)** or **Multi-Query Attention (MQA)** to shrink the K and V projections:

| Variant | Q heads | K/V heads | Output dim of W_K, W_V |
|---|---|---|---|
| MHA (vanilla) | `n_heads` | `n_heads`           | `d` |
| GQA           | `n_heads` | `n_heads / g` (e.g. 8) | `d · n_kv / n_heads` |
| MQA           | `n_heads` | `1`                 | `d_head` |

This is purely a **bandwidth** trick. The Q projection stays full size; K and V shrink by the GQA ratio. The KV cache (per token, per layer) shrinks correspondingly — which matters more for **long-context decode** than the projection itself.

**3.3 Shared / removed projections — research, not deployment**

The "KV Transformer" (drop Q, reuse K) and "K Transformer" (single projection used as Q, K, and V) variants exist in research. They cut bandwidth further at the cost of expressivity. As of ICLR 2026 they are not in production deployments — but they are exactly the kind of change a hardware engineer should track, because each removed projection is one fewer GEMV per layer per token.

**3.4 PAMM and training-time compression**

Recent work like **Projected Attention Memory Mapping (PAMM)** compresses QKV activations during *training* by up to 512×. This is a **training-memory** story, not an inference one — your inference traces are unaffected. But it indirectly enables larger models trained on the same hardware, which then *do* land on your edge device with more parameters per byte of training cost.

---

</details>

## 4. 量化格式 —— `type=12` 究竟指什么

边缘 LLM runtime 使用高度量化的权重。你在 trace 中看到的 kernel id 对应着某种特定的分块格式。对于 ggml / llama.cpp：

| `type` id | 名称      | 每权重比特数（有效） | 块大小 | 备注 |
|---|---|---|---|---|
| 0  | F32       | 32 | — | 仅供参考 |
| 1  | F16       | 16 | — | "FP16 基线" |
| 8  | Q8_0      | 8.5 | 32 | 每块一个 FP16 scale |
| 10 | Q4_0      | 4.5 | 32 | 对称 INT4 |
| 12 | Q4_K      | ~4.5 | 256 | K-quant 系列 —— 带共享 scale 的 superblock |
| 13 | Q5_K      | ~5.5 | 256 |  |
| 14 | Q6_K      | ~6.5 | 256 | 对许多模型近乎无损 |
| 8+ | IQ 系列 | 1.5–4 | 因格式而异 | "i-quants"，非均匀码本 |

K-quant（Q4_K、Q5_K、Q6_K）采用两级 **superblock + sub-block** 布局：每 256 个权重组成一个 superblock，配一个 FP16 scale，再把更小的 per-sub-block scale 打包进几个字节。计算 kernel 做的是：

```
for each block of 256 weights:
   load packed weights + scales
   dequant on-the-fly into FP16 or FP32 registers
   FMA into accumulator
```

反量化**在寄存器内**发生，而不是在 DRAM 中。这正是全部要点——你从 DRAM 读取每权重 4.5 bit，然后在**片上合成 16 位数值**。带宽为王。

**这对你的 trace 意味着什么：**`type=12`（Q4_K）读取 `~0.56 bytes/weight`；`type=14`（Q6_K）读取 `~0.81 bytes/weight`。在带宽受限的设备上，同一个 GEMV 用 Q6_K 会比 Q4_K 慢约 44%，困惑度再好看也白搭。选择满足你质量门槛的最低比特格式——7B 级通常用 Q4_K_M，3B 级用 Q5_K_M——只有当你在 eval set 上确实看到退化时，才升级到 Q6_K。

---

## 5. KV cache —— 另一半内存故事

每生成一个 token，attention 都要重新读取**到目前为止的整个 KV cache**：

```
KV bytes per layer per token = 2 · n_kv_heads · d_head · bytes_per_elem
```

对于一个 Llama-3-8B 级模型（n_layers=32，n_kv_heads=8，d_head=128），在 FP16 下：

```
2 · 8 · 128 · 2 = 4096 bytes/layer/token
× 32 layers     = 131 072 bytes per cache slot
× context len   (e.g. 4096) = 537 MB
```

这还**叠加在**约 5 GB 的权重之上。在 8 GB 的 Orin Nano 上，long-context decode（逐 token 生成阶段）会很快耗尽 LPDDR，runtime 随即退回到更慢的路径。KV cache 压缩——INT8/INT4 KV、paged attention、滑动窗口、稀疏检索——是带宽优化的第二条轴。

在你的 trace 中，你*看不到*显式的 "KV read" 行。它们被折叠进了 attention kernel。但当你把实测带宽与"仅权重"的预测值相比时，它们就会清楚地显现——这个差值就是 KV 流量。

---

## 6. Edge LLM Decode 的 roofline

对三个平台解码一个 7B Q4_K_M 模型（约 3.5 GB 权重，每 token 约 14 GFLOPS 有效算力）做一个粗略的 roofline（性能上界模型）：

| 平台 | 峰值 FP16 TFLOPS | DRAM 带宽 (GB/s) | 拐点 (flops/byte) | Decode 区间 | 实际 tok/s 上限 |
|---|---|---|---|---|---|
| Orin Nano 8 GB        | ~10              | ~50            | ~200              | 带宽受限 | ~14 |
| Orin NX 16 GB         | ~25              | ~100           | ~250              | 带宽受限 | ~28 |
| Orin AGX 64 GB        | ~50              | ~200           | ~250              | 带宽受限 | ~57 |
| H100 SXM              | ~989             | ~3 350         | ~295              | 带宽受限 | ~957 |

有两点格外突出：

1. **即便是 H100，单流 decode 也是带宽受限的。** 这就是数据中心为何要激进地做批处理——批处理把 GEMV 变回 GEMM，从而越过 roofline 拐点。
2. **在 Jetson 上你无法做批处理。** 边缘用例（助手、端侧 RAG）一次只解码一个序列。所以带宽就是命。你手里唯一的杠杆是：更小的模型、更低比特的权重、更小的 KV。

---

## 7. 诊断 Jetson 上的慢 trace

一个来自实战的真实案例：

```
Prefill: 6 tokens in 145 547 ms
Decode:  4 tokens in 102 662 ms     →  ~0.04 tok/s
Power:   0W mode, GPU @ 0 MHz
```

这个 decode 速率比 roofline 低两个数量级。"GPU @ 0 MHz" 立刻告诉你：GPU **没有在运行**。要么你走的是 CPU 回退路径，要么 DVFS 把时钟频率压住了，要么你卡在 `nvpmodel` 模式 0（15 W 上限）下，而没有用 `jetson_clocks` 把频率锁上去。


<details>
<summary>English original</summary>

**4. Quantization Formats — What `type=12` Actually Means**

Edge LLM runtimes use heavily quantized weights. The kernel id you see in the trace maps to a specific block format. For ggml / llama.cpp:

| `type` id | Name      | Bits/weight (effective) | Block size | Notes |
|---|---|---|---|---|
| 0  | F32       | 32 | — | reference only |
| 1  | F16       | 16 | — | "FP16 baseline" |
| 8  | Q8_0      | 8.5 | 32 | per-block FP16 scale |
| 10 | Q4_0      | 4.5 | 32 | symmetric INT4 |
| 12 | Q4_K      | ~4.5 | 256 | K-quant family — superblocks w/ shared scale |
| 13 | Q5_K      | ~5.5 | 256 |  |
| 14 | Q6_K      | ~6.5 | 256 | near-lossless for many models |
| 8+ | IQ-family | 1.5–4 | varied | "i-quants", non-uniform codebook |

The K-quants (Q4_K, Q5_K, Q6_K) use a two-level **superblock + sub-block** layout: one FP16 scale per superblock of 256 weights, plus smaller per-sub-block scales packed into a few bytes. The compute kernel does:

```
for each block of 256 weights:
   load packed weights + scales
   dequant on-the-fly into FP16 or FP32 registers
   FMA into accumulator
```

Dequant happens **in registers**, not in DRAM. That is the whole point — you read 4.5 bits per weight from DRAM and **synthesize 16-bit values on chip**. Bandwidth wins.

**Why this matters for your trace:** `type=12` (Q4_K) reads `~0.56 bytes/weight`; `type=14` (Q6_K) reads `~0.81 bytes/weight`. The same GEMV will be ~44% slower at Q6_K vs Q4_K on a bandwidth-bound device, perplexity be damned. Choose the lowest-bit format that meets your quality bar — usually Q4_K_M for 7B-class, Q5_K_M for 3B-class — and only escalate to Q6_K if you actually see regression on your eval set.

---

**5. The KV Cache — The Other Memory Story**

Per generated token, attention re-reads **the entire KV cache so far**:

```
KV bytes per layer per token = 2 · n_kv_heads · d_head · bytes_per_elem
```

For a Llama-3-8B-class model (n_layers=32, n_kv_heads=8, d_head=128) at FP16:

```
2 · 8 · 128 · 2 = 4096 bytes/layer/token
× 32 layers     = 131 072 bytes per cache slot
× context len   (e.g. 4096) = 537 MB
```

That is **on top** of the ~5 GB of weights. On an 8 GB Orin Nano, long-context decode runs you out of LPDDR very quickly, and the runtime falls back to slower paths. KV cache compression — INT8/INT4 KV, paged attention, sliding-window, sparse retrieval — is the second axis of bandwidth optimization.

In your trace, you do *not* see explicit "KV read" lines. They are folded into the attention kernel. But they show up clearly when you compare measured bandwidth against the "weights only" prediction — the gap is KV traffic.

---

**6. Roofline for Edge LLM Decode**

A back-of-envelope roofline for three platforms decoding a 7B Q4_K_M model (~3.5 GB weights, ~14 GFLOPS/token of useful compute):

| Platform | Peak FP16 TFLOPS | DRAM BW (GB/s) | Knee (flops/byte) | Decode regime | Practical tok/s ceiling |
|---|---|---|---|---|---|
| Orin Nano 8 GB        | ~10              | ~50            | ~200              | bandwidth-bound | ~14 |
| Orin NX 16 GB         | ~25              | ~100           | ~250              | bandwidth-bound | ~28 |
| Orin AGX 64 GB        | ~50              | ~200           | ~250              | bandwidth-bound | ~57 |
| H100 SXM              | ~989             | ~3 350         | ~295              | bandwidth-bound | ~957 |

Two things jump out:

1. **Even H100 is bandwidth-bound for single-stream decode.** That is why datacenters batch aggressively — batching turns GEMV back into GEMM and crosses the roofline knee.
2. **On Jetson you cannot batch.** Edge use cases (assistants, on-device RAG) decode one sequence at a time. So bandwidth is destiny. Your only levers are: smaller model, lower-bit weights, smaller KV.

---

**7. Diagnosing a Slow Trace on Jetson**

A real example from the wild:

```
Prefill: 6 tokens in 145 547 ms
Decode:  4 tokens in 102 662 ms     →  ~0.04 tok/s
Power:   0W mode, GPU @ 0 MHz
```

That decode rate is two orders of magnitude below the roofline. The "GPU @ 0 MHz" tells you immediately: the GPU is **not running**. Either you're on CPU fallback, or DVFS has parked the clocks, or you're stuck in `nvpmodel` mode 0 (15 W cap) without `jetson_clocks` locking the frequencies up.

</details>

### 7.1 首轮诊断检查清单

```bash
# 1. Lock to max performance
sudo nvpmodel -m 0          # max-N power mode (or -m 2 for MAXN_SUPER on JP6)
sudo jetson_clocks          # pin GPU/CPU/EMC clocks high
sudo jetson_clocks --show

# 2. Watch real-time utilization during inference
tegrastats --interval 500

# Look for:
#   GR3D_FREQ NN%       <- GPU 3D engine load — should be > 80% during decode
#   EMC_FREQ NN%        <- memory controller — should be near 100% if you're bandwidth-bound
#   CPU [...]           <- if CPU is high and GR3D is low, you're on CPU fallback

# 3. Confirm CUDA is actually used
nvidia-smi               # works on AGX; not all Orin variants
# or
sudo /usr/local/cuda/extras/demo_suite/deviceQuery

# 4. Profile the runtime
nsys profile -t cuda,nvtx,osrt -o llm_decode ./your_runtime
nsys-ui llm_decode.nsys-rep
# Look at the timeline: are there big gaps between kernels?
#                        Are kernels < 50 µs each? That's launch overhead dominating.
```

### 7.2 边缘 LLM 常见病症

| 症状 | 可能原因 | 修复 |
|---|---|---|
| `GR3D_FREQ 0%`，CPU 跑满 | CPU 回退；未编译进 CUDA backend | 用 `LLAMA_CUDA=1` 或 `-DGGML_CUDA=ON` 重新编译 |
| `EMC_FREQ < 50%`，GPU 中等 | 权重位于可分页内存，触发统一内存抖动 | pin / 用 cudaMallocHost，或配合 `MADV_HUGEPAGE` 做 mmap |
| 大量细碎的 CUDA launch | QKV 未融合、gate/up 未融合 | 改用带融合的 runtime 构建（llama.cpp ≥ b3000、mlc-llm） |
| 高 `GR3D_FREQ` 下低于 roofline（性能上界模型） | 对给定 shape 而言 kernel occupancy 过低 | 调 block size，或换量化格式（Q4_K kernel 往往优于 Q4_0） |
| 每 N 个 token 出现长停顿 | KV cache 重新分配 | 启动时预分配完整的 `n_ctx`，绝不增长 |
| 高上下文下吞吐下降 | KV 带宽 > 权重带宽 | INT8 KV 或 paged attention |

---

## 8. 优化究竟发生在哪里

带宽墙在四个层面被打破。每一层都是实打实的工程专业方向：

### 8.1 模型层 —— 从设计上减少字节数

* 用 GQA / MQA 取代 MHA。
* 滑动窗口 attention（Mistral）或分块 attention，以限制 KV。
* 绑定 embedding（LM head 与 token embedding 共享权重）—— 省掉最后那个巨大 GEMV（矩阵-向量乘）所对应的独立权重。
* 只有在布线内存预算充足时才用 Mixture-of-Experts；在 8 GB 规模下通常不合适。

### 8.2 数值层 —— 每个权重占用更少字节

* 对 7B 级模型，默认从 Q4_K_M 起步。
* 4-bit 用 AWQ / GPTQ，配合基于校准的权重重排 —— 同等 bit 率下困惑度优于均匀 Q4。
* W4A8 / W4A16 拆分：权重 4 bit，激活值 8 或 16 bit。
* INT8 / INT4 KV cache。

### 8.3 Kernel 层 —— 每字节更少读取

* **融合 QKV**、**融合 gate+up**（SwiGLU）、**融合 dequant+matmul**（K-quant 的 kernel 已经这么做）。
* 常驻 kernel，跨多个已 decode（逐 token 生成阶段）的 token 把权重分块留在共享内存中 —— 只有配合投机解码或并行解码才可行。
* Ampere/Ada 上的 warp 专用 loader（Orin 是 Ampere 架构 —— 收益有限但确实存在）。
* CPU 路径上：用 ARM NEON / SVE 做 dequant，配合仔细对齐的 64-byte 读取。

### 8.4 系统层 —— 更少从 DRAM 取数据

* 把权重固定在统一内存中，并对热点分块关闭 GPU 的 L2 缓存驱逐（在 runtime 暴露该能力的地方）。
* 在 JP6 上，对 mmap 的权重文件使用 `MADV_HUGEPAGE`。
* `jetson_clocks` 让 EMC 保持在最高 —— 若放任不管，DVFS 会在 “0 W 空闲” 启发式下压制 EMC。
* 避免 host↔device 拷贝；在 Jetson 上 GPU 与 CPU 共享物理 DRAM，因此只要用对方式，零拷贝就是免费的（`cudaHostAllocMapped` 或带正确 hint 的统一内存）。

---


<details>
<summary>English original</summary>

**7.1 First-pass diagnostic checklist**

```bash
# 1. Lock to max performance
sudo nvpmodel -m 0          # max-N power mode (or -m 2 for MAXN_SUPER on JP6)
sudo jetson_clocks          # pin GPU/CPU/EMC clocks high
sudo jetson_clocks --show

# 2. Watch real-time utilization during inference
tegrastats --interval 500

# Look for:
#   GR3D_FREQ NN%       <- GPU 3D engine load — should be > 80% during decode
#   EMC_FREQ NN%        <- memory controller — should be near 100% if you're bandwidth-bound
#   CPU [...]           <- if CPU is high and GR3D is low, you're on CPU fallback

# 3. Confirm CUDA is actually used
nvidia-smi               # works on AGX; not all Orin variants
# or
sudo /usr/local/cuda/extras/demo_suite/deviceQuery

# 4. Profile the runtime
nsys profile -t cuda,nvtx,osrt -o llm_decode ./your_runtime
nsys-ui llm_decode.nsys-rep
# Look at the timeline: are there big gaps between kernels?
#                        Are kernels < 50 µs each? That's launch overhead dominating.
```

**7.2 Common edge-LLM pathologies**

| Symptom | Likely cause | Fix |
|---|---|---|
| `GR3D_FREQ 0%`, CPU pegged | CPU fallback; CUDA backend not compiled in | rebuild with `LLAMA_CUDA=1` or `-DGGML_CUDA=ON` |
| `EMC_FREQ < 50%`, GPU mid | weights in pageable memory, hitting unified-memory thrash | pin / use cudaMallocHost or mmap with `MADV_HUGEPAGE` |
| Many tiny CUDA launches | unfused QKV, unfused gate/up | switch to runtime build with fusion (llama.cpp ≥ b3000, mlc-llm) |
| Sub-roofline at high `GR3D_FREQ` | kernel occupancy too low for given shapes | tune block size, or switch quant format (Q4_K kernels often outperform Q4_0) |
| Long pauses every N tokens | KV cache reallocation | preallocate full `n_ctx` at start, never grow |
| Throughput drops at high context | KV bandwidth > weight bandwidth | INT8 KV or paged attention |

---

**8. Where Optimization Actually Lives**

The bandwidth wall is broken at four layers. Each is a real engineering specialty:

**8.1 Model layer — fewer bytes by design**

* GQA / MQA over MHA.
* Sliding-window attention (Mistral) or chunked attention to bound KV.
* Tied embeddings (LM head shares weights with token embedding) — removes one huge final GEMV's worth of unique weights.
* Mixture-of-Experts only if you have the routing memory budget; usually a bad fit at the 8 GB scale.

**8.2 Numerical layer — fewer bytes per weight**

* Q4_K_M as a default starting point for 7B-class.
* AWQ / GPTQ for 4-bit with calibration-based weight reordering — better perplexity than uniform Q4 at the same bit-rate.
* W4A8 / W4A16 split: weights at 4 bits, activations at 8 or 16 bits.
* INT8 / INT4 KV cache.

**8.3 Kernel layer — fewer reads per byte**

* **Fused QKV**, **fused gate+up** (SwiGLU), **fused dequant+matmul** (the K-quant kernels already do this).
* Persistent kernels that hold weight tiles in shared memory across multiple decoded tokens — only practical with speculative or parallel decoding.
* Warp-specialized loaders on Ampere/Ada (Orin is Ampere — limited but real benefit).
* On CPU paths: ARM NEON / SVE for dequant, with carefully-aligned 64-byte reads.

**8.4 System layer — fewer fetches from DRAM**

* Pin weights in unified memory and disable the GPU's L2 cache eviction for hot tiles (where the runtime exposes it).
* On JP6, use `MADV_HUGEPAGE` for the mmapped weight file.
* `jetson_clocks` to keep EMC at max — DVFS will throttle EMC under "0 W idle" heuristics if you let it.
* Avoid host↔device copies; on Jetson the GPU and CPU share physical DRAM, so zero-copy is free if you use it correctly (`cudaHostAllocMapped` or unified memory with the right hints).

---

</details>

## 9. 动手练习

1. **为你的 Jetson 构建 roofline（性能上界模型）图。** 用 bandwidth-test（CUDA samples 中的 `bandwidthTest`）和一个单一 shape 的 GEMM（矩阵-矩阵乘）benchmark，找出实测峰值 FP16 TFLOPS 与实测 DRAM BW。计算拐点。然后对同一模型分别以 Q4_K_M、Q5_K_M、Q6_K、F16 运行 llama.cpp，把每个点放到图上。确认 decode（逐 token 生成阶段）点聚集在带宽斜率上；找出任何落在下方的点（排查是否属于 `nvpmodel`/`jetson_clocks` 问题）。

2. **GEMV（矩阵-向量乘）trace 拆解。** 在你了解其架构的模型上，用 `LLAMA_LOG_LEVEL=DEBUG`（或在暴露该选项的 build 上用 `--verbose`）运行 llama.cpp。保存单个生成 token 的 trace。给每个 GEMV 标注对应的 Transformer block 名称（Q、K、V、O、gate、up、down、LM head）。计算总读取字节数；预测 tokens/sec；与实测对比。

3. **量化阶梯的带宽测量。** 取同一 base model，量化到 Q4_0、Q4_K_M、Q5_K_M、Q6_K、F16。在同一 Jetson 上、同一 prompt 下测 tokens/sec。画 tok/s 对有效 bytes/weight 的图。该关系应近似线性并带一个常数 —— 该常数就是你能达到的带宽，而斜率告诉你是否在某个特定 quant 格式上损失了效率（Q4_0 常常表现不佳，因为它的 kernel 不如 Q4_K 优化充分）。

4. **融合 QKV 检查。** 将同一模型分别以带 QKV 融合和不带 QKV 融合的方式导出（`mlc-llm` 让你在 compile pass 中切换这一项）。比较 Nsight Systems 中的 kernel 数量与端到端 tok/s。量化这一收益在 Orin Nano 与 Orin AGX 上的差异 —— 在 launch 开销占 kernel 时间比例更大的地方，融合更重要。

5. **KV cache 压缩研究。** 在 FP16 KV 与 INT8 KV 下运行 4 k 上下文的 decode（前提是你的 runtime 支持 —— 例如 mlc-llm、带 CUDA-capable backend 的 vLLM）。测量 token 1、1 k、2 k、4 k 附近的 tok/s。作图。你应当看到 FP16 KV 先平缓退化随后陡降；INT8 KV 应能更久地保持平缓。找出交叉点。

6. **诊断一次被拖慢的运行。** 让 Jetson 在 `nvpmodel -m 1`（10 W 上限）下启动，*不要*运行 `jetson_clocks`，然后跑一次推理。抓取 `tegrastats`。接着逐步：(a) 运行 `jetson_clocks`，(b) 切换到 `nvpmodel -m 0`，(c) 确保 runtime 是用 CUDA 构建的。每一步记录 tok/s。你会得到一张四行表，它比任何解释边缘 LLM 性能为何“慢”的博客文章都更有说服力。

---

## 10. 关键要点

| 要点 | 对 AI 硬件而言为何重要 |
|---|---|
| Decode = GEMV；prefill = GEMM | 两条不同的 roofline，两个不同的工程问题 |
| 边缘 decode 就是带宽受限，没有例外 | 以 TFLOPS 为中心的设计与 benchmark 在此工作负载上会误导人 |
| QKV 是每个 layer 的前三个 GEMV | 把它们融合是 ROI 最高的单项 runtime 改动 |
| 量化格式 = bytes/weight = decode 速度 | 选满足质量的最低比特率，而不是塞得下的最高比特率 |
| KV cache 是*第二条*带宽故事线 | 长上下文 decode 的瓶颈在这里，不在权重 |
| `nvpmodel` + `jetson_clocks` 不是可选项 | 默认 DVFS 会悄悄把你的 tok/s 砍半 |
| Roofline 推理胜过凭感觉 | 5 分钟的计算就能判断某个修复是否值得花几周 |

---


<details>
<summary>English original</summary>

**9. Hands-On Exercises**

1. **Build a roofline plot for your Jetson.** Use the bandwidth-test (`bandwidthTest` from CUDA samples) and a single-shape GEMM benchmark to find the measured peak FP16 TFLOPS and measured DRAM BW. Compute the knee. Then run llama.cpp at Q4_K_M, Q5_K_M, Q6_K, F16 for the same model and place each on the plot. Confirm decode points cluster on the bandwidth slope; identify any that fall below (look for an `nvpmodel`/`jetson_clocks` issue).

2. **GEMV trace dissection.** Run llama.cpp with `LLAMA_LOG_LEVEL=DEBUG` (or `--verbose` on builds that expose it) on a model whose architecture you know. Save the trace for a single generated token. Annotate each GEMV with the transformer block name (Q, K, V, O, gate, up, down, LM head). Compute total bytes read; predict tokens/sec; compare to observed.

3. **Quant ladder bandwidth measurement.** Take the same base model and quantize to Q4_0, Q4_K_M, Q5_K_M, Q6_K, F16. Measure tokens/sec on the same Jetson at the same prompt. Plot tok/s vs effective bytes/weight. The relationship should be roughly linear with a constant — that constant is your achievable bandwidth, and the slope tells you whether you're losing efficiency at a specific quant format (often Q4_0 underperforms because its kernels are less optimized than Q4_K).

4. **Fused QKV check.** Export the same model with and without QKV fusion (`mlc-llm` lets you toggle this in the compile pass). Compare kernel counts in Nsight Systems and end-to-end tok/s. Quantify the win on Orin Nano vs Orin AGX — fusion matters more where launch overhead is a larger fraction of kernel time.

5. **KV cache compression study.** Run a 4 k-context decode at FP16 KV vs INT8 KV (where your runtime supports it — e.g. mlc-llm, vLLM with CUDA-capable backend). Measure tok/s near token 1, 1 k, 2 k, 4 k. Plot. You should see FP16 KV degrade gracefully then steeply; INT8 KV should hold flatter for longer. Identify the cross-over point.

6. **Diagnose a sandbagged run.** Boot Jetson in `nvpmodel -m 1` (10 W cap), do *not* run `jetson_clocks`, and run an inference. Capture `tegrastats`. Then progressively (a) run `jetson_clocks`, (b) switch to `nvpmodel -m 0`, (c) ensure the runtime is CUDA-built. At each step record tok/s. You will get a four-row table that is more persuasive than any blog post about why edge LLM perf is "slow".

---

**10. Key Takeaways**

| Takeaway | Why it matters for AI hardware |
|---|---|
| Decode = GEMV; prefill = GEMM | Two different rooflines, two different engineering problems |
| Edge decode is bandwidth-bound, full stop | Designs and benchmarks centered on TFLOPS lie about this workload |
| QKV is the first three GEMVs of every layer | Fusing them is the single highest-ROI runtime change |
| Quant format = bytes/weight = decode speed | Pick the lowest bit-rate that meets quality, not the highest you can fit |
| The KV cache is the *second* bandwidth story | Long-context decode is bottlenecked there, not on weights |
| `nvpmodel` + `jetson_clocks` are not optional | Default DVFS will silently halve your tok/s |
| Roofline reasoning beats vibes | A 5-minute calculation tells you whether a fix is worth weeks of work |

---

</details>

## 资源

* **[ggml / llama.cpp source](https://github.com/ggerganov/llama.cpp):** K-quant kernel、GEMV 实现，以及你以 `type=12` 见到的量化格式 `enum` 的参考来源。
* **[mlc-llm](https://llm.mlc.ai/):** 基于 TVM/Relax 构建的编译器驱动 LLM runtime；在 IR 层面研究 fused QKV 与 fused SwiGLU 最清晰的地方。
* **["Roofline: An Insightful Visual Performance Model" (Williams, Waterman, Patterson, 2009)](https://dl.acm.org/doi/10.1145/1498765.1498785):** 奠基性论文 —— roofline（性能上界模型）至今仍是 LLM decode（逐 token 生成阶段）的正确心智模型。
* **["GPTQ: Accurate Post-Training Quantization for Generative Pre-trained Transformers"](https://arxiv.org/abs/2210.17323):** 4-bit 校准的故事。
* **["AWQ: Activation-aware Weight Quantization for LLM Compression and Acceleration"](https://arxiv.org/abs/2306.00978):** 相同 bit-rate 下往往优于 GPTQ；对移动端友好。
* **["GQA: Training Generalized Multi-Query Transformer Models from Multi-Head Checkpoints"](https://arxiv.org/abs/2305.13245):** Llama 3、Mistral、Gemma 采用的 K/V 归约技巧。
* **["Efficient Streaming Language Models with Attention Sinks"](https://arxiv.org/abs/2309.17453):** 滑动窗口 KV，且不导致质量崩塌。
* **[NVIDIA Jetson Linux Developer Guide — `nvpmodel` & `jetson_clocks`](https://docs.nvidia.com/jetson/archives/r36.2/DeveloperGuide/):** 功耗模式与时钟锁定的权威参考。
* **[`tegrastats` man page / Jetson Stats `jtop`](https://github.com/rbonghi/jetson_stats):** Jetson 上的 GPU/EMC/CPU 实时利用率。
* **[Nsight Systems on Jetson](https://docs.nvidia.com/nsight-systems/):** 真正能在 Orin 上显示 kernel 级间隙的性能分析器。
* **[阶段 4 — Jetson 实时推理指南](../../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/11-Orin-Nano实时推理/Guide.md):** 关于 Jetson Orin Nano 推理的配套深入解析。
* **[阶段 4 — 方向 C — 量化指南](../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md):** 从编译器视角看本文讨论的量化格式。


<details>
<summary>English original</summary>

**Resources**

* **[ggml / llama.cpp source](https://github.com/ggerganov/llama.cpp):** The reference for K-quant kernels, GEMV implementations, and the quant-format `enum` you saw as `type=12`.
* **[mlc-llm](https://llm.mlc.ai/):** Compiler-driven LLM runtime built on TVM/Relax; cleanest place to study fused QKV and fused SwiGLU at IR level.
* **["Roofline: An Insightful Visual Performance Model" (Williams, Waterman, Patterson, 2009)](https://dl.acm.org/doi/10.1145/1498765.1498785):** Foundational paper — still the right mental model for LLM decode.
* **["GPTQ: Accurate Post-Training Quantization for Generative Pre-trained Transformers"](https://arxiv.org/abs/2210.17323):** The 4-bit calibration story.
* **["AWQ: Activation-aware Weight Quantization for LLM Compression and Acceleration"](https://arxiv.org/abs/2306.00978):** Often outperforms GPTQ at the same bit-rate; mobile-friendly.
* **["GQA: Training Generalized Multi-Query Transformer Models from Multi-Head Checkpoints"](https://arxiv.org/abs/2305.13245):** The K/V reduction trick used by Llama 3, Mistral, Gemma.
* **["Efficient Streaming Language Models with Attention Sinks"](https://arxiv.org/abs/2309.17453):** Sliding-window KV without quality collapse.
* **[NVIDIA Jetson Linux Developer Guide — `nvpmodel` & `jetson_clocks`](https://docs.nvidia.com/jetson/archives/r36.2/DeveloperGuide/):** The canonical reference for power modes and clock locking.
* **[`tegrastats` man page / Jetson Stats `jtop`](https://github.com/rbonghi/jetson_stats):** Live GPU/EMC/CPU utilization on Jetson.
* **[Nsight Systems on Jetson](https://docs.nvidia.com/nsight-systems/):** The profiler that actually shows you kernel-level gaps on Orin.
* **[Phase 4 — Jetson Real-Time Inference guide](../../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/11-Orin-Nano实时推理/Guide.md):** Companion deep dive on Jetson Orin Nano inference.
* **[Phase 4 — Track C — Quantization guide](../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md):** Compiler-side view of the quant formats discussed here.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Edge LLM Inference Internals/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Edge%20LLM%20Inference%20Internals/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
