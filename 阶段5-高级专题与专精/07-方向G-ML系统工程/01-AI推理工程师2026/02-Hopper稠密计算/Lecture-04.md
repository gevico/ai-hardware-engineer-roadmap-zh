---
title: Part 2 · 第 04 讲 — 单节点多 GPU 推理服务：8× H100/H200 上的张量并行
description: Part 2 · 第 04 讲 — 单节点多 GPU 推理服务：8× H100/H200 上的张量并行
---

# Part 2 · 第 04 讲 — 单节点多 GPU 推理服务：8× H100/H200 上的张量并行

## 概览

FP16 下的 70B 级稠密模型**约 140 GB**。Hopper 这一代没有哪张单 GPU 能在不量化的前提下装下它——H100 是 80 GB，H200 是 141 GB。若要以高于 INT4 的精度做推理服务，或支持更大的 batch / 更长的上下文，就要把模型**切分到单节点内的多张 GPU** 上，用 NVLink 互联。

2026 年的主流切分方式是**张量并行（TP）**——由 Megatron-LM 开创的经典做法。本讲围绕这一对锚定模型，端到端讲透 TP：

1. attention 与 FFN layer 的 TP 切分——哪些矩阵怎么切。
2. 主导 TP 单步耗时的 NCCL all-reduce。
3. HGX H100/H200 8-GPU 机器上 NVLink + NVSwitch 互联结构的实际情况。
4. TP=2 对 TP=4 对 TP=8——每种取舍在吞吐和延迟上究竟要付出什么。
5. 各 runtime 的 TP 配置——vLLM、SGLang、TRT-LLM。
6. 诊断——all-reduce 何时成为瓶颈，以及该怎么办。
7. 序列并行与张量并行 FFN 的叠加方案。

读完本讲，应当能在 8× H100/H200 上为 Llama 3.3 70B 或 Qwen 2.5 72B 搭起 TP，测量 all-reduce 开销，并判断给定工作负载该用 TP=4 还是 TP=8。

---

## 1. 张量并行——切分方式

TP 把*单个矩阵乘法*切分到多张 GPU 上。对每个 Linear layer，权重矩阵被**切分**，每张 GPU 计算输出的一块切片。各切片通过 **all-reduce** 拼起来。

### 1.1 attention 切分

Q、K、V 投影按 head 切分：

```text
On 4-GPU TP, each GPU holds 16 of 64 Q heads + 2 of 8 KV heads:
  GPU 0: Q_heads_0..15, K_heads_0..1, V_heads_0..1
  GPU 1: Q_heads_16..31, K_heads_2..3, V_heads_2..3
  GPU 2: Q_heads_32..47, K_heads_4..5, V_heads_4..5
  GPU 3: Q_heads_48..63, K_heads_6..7, V_heads_6..7

Attention computed locally per GPU on its head shard:
  attn_local = softmax(Q_local · K_local^T) · V_local

Output projection partitioned by *rows*:
  W_o split: rows 0..2047 on GPU 0, rows 2048..4095 on GPU 1, etc.
  
  Per-GPU partial: attn_local · W_o_local → partial_d
  All-reduce across GPUs: full_d = sum(partial_d) over 4 GPUs
```


**all-reduce** 是跨 GPU 的那一步。没有它，每张 GPU 只持有自己那份 head 分片的贡献。

### 1.2 FFN 切分

FFN 的两个矩阵乘切法不同：

```text
W_gate, W_up: split by columns (output dim).
  Per GPU computes (B, d) × (d, d_ff/N) → (B, d_ff/N)
  No communication yet — local result.

SwiGLU: silu(gate) * up — element-wise, local.

W_down: split by rows (input dim).
  Per GPU computes (B, d_ff/N) × (d_ff/N, d) → (B, d) partial
  All-reduce across GPUs to get full (B, d)
```


同样，FFN 末尾也要做一次 all-reduce。

### 1.3 每个 layer 的 all-reduce 总数

每个 transformer layer 有**两次 all-reduce**——一次在 attention 输出投影之后，一次在 FFN down 投影之后。对 80 个 layer 的模型，decode（逐 token 生成阶段）下每个 token 就是 **160 次 all-reduce**。

这是 TP 推理服务中**最重要的单一性能考量**。

---

## 2. NCCL all-reduce——它究竟做了什么

NCCL（NVIDIA Collective Communications Library）按消息大小和拓扑，把 all-reduce 实现为 **ring** 或 **tree** 算法。

### 2.1 Ring all-reduce

以 4 张 GPU 走 NVLink 为例：

```text
Round 1: GPU 0 sends slice to GPU 1, GPU 1 to GPU 2, etc.
Round 2: each GPU receives + sums slice
... 2(N-1) rounds total to fully reduce + share
```


带宽：每张 GPU 发送 `2 * (N-1) * (size/N)` 字节。对大消息，带宽利用率接近最优。

### 2.2 Tree all-reduce

对 8 张以上 GPU，分层 tree 规约有时更快：

```text
log2(N) levels of pairwise reduce → final value at root → log2(N) levels of broadcast back
```


小消息延迟更低，大消息延迟更高（带宽效率较差）。

### 2.3 NCCL 怎么选

NCCL 按（消息大小，拓扑）自动调优。在带 NVLink Switch（全互联 8-GPU 域）的 HGX H100/H200 上，TP 中的 all-reduce 通常用 ring，因为 prefill（首字前的整段计算）期间消息很大（FP16 下每 layer 约 16-32 MB）；decode 的消息小得多，受延迟约束（§2.4）。

### 2.4 带宽开销

70B FP16 模型 FFN down 投影的一次 all-reduce：

```text
message_size_per_gpu = batch × hidden_size × bytes
                     = 32 × 8192 × 2
                     = 0.5 MB at batch=32

Across 80 layers × 2 all-reduces per layer:
total bytes = 80 × 2 × 0.5 MB = 80 MB per token decode

On NVLink (900 GB/s aggregate) the all-reduce time is small for this message size,
but the *latency* (round-trips) adds up.
```


prefill 在 batch=1、prompt=2048 时：

```text
message_size = 2048 × 8192 × 2 = 32 MB per layer per all-reduce
total bytes per prompt = 80 × 2 × 32 MB = 5.1 GB cross-GPU traffic per prefill
```


这个数字很可观。TP=8 的 prefill 有**约 25% 的单步时间花在 NCCL all-reduce 上**。


<details>
<summary>English original</summary>

**Part 2 · Lecture 04 — Single-Node Multi-GPU Serving: Tensor Parallelism on 8× H100/H200**

**Overview**

A 70B-class dense model in FP16 is **~140 GB**. No single GPU in the Hopper generation holds it without quantization — H100 has 80 GB, H200 has 141 GB. To serve it at higher precision than INT4, or to serve at larger batch / longer context, the model is **split across multiple GPUs** in a single node, joined by NVLink.

The dominant 2026 split is **tensor parallelism (TP)** — the canonical approach pioneered by Megatron-LM. This lecture covers TP end-to-end for the anchor pair:

1. The TP partitioning of attention and FFN layers — which matrices split how.
2. The NCCL all-reduce that dominates TP step time.
3. NVLink + NVSwitch fabric reality on HGX H100/H200 8-GPU boxes.
4. TP=2 vs TP=4 vs TP=8 — what each tradeoff actually costs in throughput and latency.
5. Runtime-specific TP configuration — vLLM, SGLang, TRT-LLM.
6. Diagnosis — when the all-reduce is the bottleneck and what to do.
7. Sequence parallelism and tensor-parallel-FFN overlays.

By the end you should be able to set up TP on 8× H100/H200 for either Llama 3.3 70B or Qwen 2.5 72B, measure the all-reduce overhead, and tell whether a given workload should use TP=4 or TP=8.

---

**1. Tensor parallelism — the partitioning**

TP splits *individual matrix multiplications* across GPUs. For each Linear layer, the weight matrix is **partitioned** and each GPU computes a slice of the output. The slices are joined by **all-reduce**.

**1.1 Attention partition**

Q, K, V projections are partitioned by head:

```text
On 4-GPU TP, each GPU holds 16 of 64 Q heads + 2 of 8 KV heads:
  GPU 0: Q_heads_0..15, K_heads_0..1, V_heads_0..1
  GPU 1: Q_heads_16..31, K_heads_2..3, V_heads_2..3
  GPU 2: Q_heads_32..47, K_heads_4..5, V_heads_4..5
  GPU 3: Q_heads_48..63, K_heads_6..7, V_heads_6..7

Attention computed locally per GPU on its head shard:
  attn_local = softmax(Q_local · K_local^T) · V_local

Output projection partitioned by *rows*:
  W_o split: rows 0..2047 on GPU 0, rows 2048..4095 on GPU 1, etc.
  
  Per-GPU partial: attn_local · W_o_local → partial_d
  All-reduce across GPUs: full_d = sum(partial_d) over 4 GPUs
```

The **all-reduce** is the cross-GPU step. Without it, each GPU has only its head shard's contribution.

**1.2 FFN partition**

The two FFN matmuls split differently:

```text
W_gate, W_up: split by columns (output dim).
  Per GPU computes (B, d) × (d, d_ff/N) → (B, d_ff/N)
  No communication yet — local result.

SwiGLU: silu(gate) * up — element-wise, local.

W_down: split by rows (input dim).
  Per GPU computes (B, d_ff/N) × (d_ff/N, d) → (B, d) partial
  All-reduce across GPUs to get full (B, d)
```

Again, the FFN ends with an all-reduce.

**1.3 Total all-reduces per layer**

For each transformer layer, **two all-reduces** — one after attention output projection, one after FFN down projection. For an 80-layer model that's **160 all-reduces per token** at decode.

This is the **single largest performance consideration** in TP serving.

---

**2. The NCCL all-reduce — what it actually does**

NCCL (NVIDIA Collective Communications Library) implements all-reduce as either **ring** or **tree** algorithm depending on size and topology.

**2.1 Ring all-reduce**

For 4 GPUs with NVLink:

```text
Round 1: GPU 0 sends slice to GPU 1, GPU 1 to GPU 2, etc.
Round 2: each GPU receives + sums slice
... 2(N-1) rounds total to fully reduce + share
```

Bandwidth: each GPU sends `2 * (N-1) * (size/N)` bytes. For large messages, near-optimal bandwidth use.

**2.2 Tree all-reduce**

For 8+ GPUs, hierarchical tree reduces are sometimes faster:

```text
log2(N) levels of pairwise reduce → final value at root → log2(N) levels of broadcast back
```

Lower latency for small messages, higher latency for large messages (less bandwidth-efficient).

**2.3 What NCCL picks**

NCCL auto-tunes per (message size, topology). On HGX H100/H200 with NVLink Switch (fully-connected 8-GPU domain), ring is typical for the all-reduces in TP because the messages are large during prefill (~16-32 MB per layer at FP16); decode messages are far smaller and latency-bound (§2.4).

**2.4 The bandwidth cost**

For an FFN down-projection all-reduce in 70B FP16:

```text
message_size_per_gpu = batch × hidden_size × bytes
                     = 32 × 8192 × 2
                     = 0.5 MB at batch=32

Across 80 layers × 2 all-reduces per layer:
total bytes = 80 × 2 × 0.5 MB = 80 MB per token decode

On NVLink (900 GB/s aggregate) the all-reduce time is small for this message size,
but the *latency* (round-trips) adds up.
```

For prefill at batch=1, prompt=2048:

```text
message_size = 2048 × 8192 × 2 = 32 MB per layer per all-reduce
total bytes per prompt = 80 × 2 × 32 MB = 5.1 GB cross-GPU traffic per prefill
```

This is significant. Prefill TP=8 spends **~25% of step time in NCCL all-reduces**.

---

</details>

## 3. HGX 服务器上的 NVLink + NVSwitch

标准 HGX 配置中 8× H100 / H200 的物理 fabric：

* **NVLink 4** — 每链路 50 GB/s，每 GPU 18 条链路 → 每 GPU 聚合 900 GB/s。
* **NVSwitch v3** — HGX 基板中的 4 个交换机以 all-to-all 拓扑互连 8 块 GPU。
* **每对 GPU 具有 900 GB/s 有效带宽。**
* **延迟** — 小消息亚微秒级。

这个 fabric 是 **TP=8 能够实用的根本原因**。没有 NVLink，通过 PCIe 进行 8 块 GPU 的 TP（到主机 128 GB/s 聚合，如果未被阻塞则对等 64 GB/s）会**慢 10–14×**。

### 3.1 NVLink Switch chiplet 与 H200 NVL 配置

较新的 HGX 配置和即将推出的 **NVL** 机箱使用 NVLink 交换机作为独立芯片，跨服务器互连。对于单节点服务器上的推理工作负载（这是本讲的整个范围），基板 NVSwitch 才是关键。

---

## 4. TP=2 vs TP=4 vs TP=8

对于 Llama 3.3 70B / Qwen 2.5 72B：

| TP | 每 GPU 权重份额（FP16） | 每 GPU KV 份额 | 通信开销 | 使用场景 |
|----|-----------------------------|-------------------|------------------------|-------------|
| 1 | FP16 下放不下 | 不适用 | 无 | 仅 H200 上 INT4 |
| 2 | ~70 GB | 50% | 步骤时间的 ~10% | H100 NVL 或 H200 单节点 |
| 4 | ~35 GB | 25% | 步骤时间的 ~15% | 最佳 chat 形状吞吐 |
| 8 | ~18 GB | 12.5% | 步骤时间的 ~25% | 最大 batch / 长上下文 |

### 4.1 TP 扩展效率

理想：4× 更多 GPU → 4× 更多吞吐。
实际：4× 更多 GPU → 在此硬件/模型上 2.5–3.5× 更多吞吐。

差距在于**通信**。曲线如下：

```text
   per-replica throughput
        ▲
        │       •  TP=2   (95% efficiency vs single-GPU)
        │   •        TP=4   (85% efficiency)
        │            TP=8 •  (65–75% efficiency)
        │
        └────────────────────► TP degree
```

扩展效率取决于工作负载：

* **Chat（decode（逐 token 生成阶段）为主，batch=32）：** TP=4 是最佳选择。TP=8 牺牲约 25% 来换取 KV cache / 更长上下文的余量。
* **Batch / offline（以 prefill（首字前的整段计算）为主）：** TP=8 可能胜出，因为 prefill 是算力受限；FFN 矩阵乘可摊销 all-reduce 开销。
* **长上下文（128K）：** TP=8，因为 KV cache 压力迫使其如此；吞吐损失可接受，因为长上下文吞吐的瓶颈在别处。

### 4.2 每 GPU 吞吐陷阱

常见指标错误：在**每副本吞吐**上比较 TP=4 与 TP=8。

* TP=4 时 1200 tok/s → 300 tok/s/GPU。
* TP=8 时 1600 tok/s → 200 tok/s/GPU。

TP=8 有**更高的绝对吞吐**（更好的 TPOT，每副本服务更多用户），但如果每块 H200 有固定成本，则 **$/MTok 更差**。

对于 chat 产品，**TP=4 通常是成本高效的选择**。TP=8 用于 TP=4 无法容纳工作负载的情况（例如大批量下的 128K 上下文）。

### 4.3 当 TP 大小不由你选择时 — FP8 块对齐

有一个硬约束会覆盖上述成本/吞吐权衡：**块缩放 FP8 权重要求每个张量并行分片的维度是量化块的整数倍。** 如果 `dim / TP` 不是 FP8 块大小的倍数，引擎会加载失败。

对于 Qwen 2.5 72B，FFN 中间维 `29568 = 128 × 231` 在跨 2、4 或 8 块 GPU 切分后*不能*被 128 整除——因此 block-FP8 + TP=8 无法加载，直到你降到能重新对齐的 TP（或使用会做 padding 的框架构建）。机制和修复顺序见 [Lecture 03 §5.4](Lecture-03.md)。*本*讲的要点：当你服务 FP8 时，**在推理吞吐之前先验证模型能否在你选定的 TP 下加载**——量化可能迫使你选择比工作负载本身更小的 TP。

---

## 5. 特定 runtime 的 TP 配置

### 5.1 vLLM

```python
from vllm import LLM, SamplingParams

llm = LLM(
    model="meta-llama/Llama-3.3-70B-Instruct",
    tensor_parallel_size=4,        # the TP degree
    dtype="float16",
    gpu_memory_utilization=0.92,
)
```

vLLM 自动发现 GPU 并分配 rank。在标准 HGX 系统上开箱即用。

### 5.2 SGLang

```bash
python -m sglang.launch_server \
    --model-path Qwen/Qwen2.5-72B-Instruct \
    --tp-size 4 \
    --dtype float16 \
    --port 30000
```

SGLang 的 TP 与其 RadixAttention 和 EP 支持集成。与 vLLM 相同的 fabric 假设。

### 5.3 TensorRT-LLM

```bash
trtllm-build \
    --checkpoint_dir ./fp8_checkpoint \
    --output_dir ./engine \
    --tp_size 4 \
    ...
```

TRT-LLM 为选定的 TP 大小编译模型图。更改 TP 需要重新构建——这是部署时的决定，而非 runtime 配置。


<details>
<summary>English original</summary>

**3. NVLink + NVSwitch on HGX boxes**

The physical fabric for 8× H100 / H200 in standard HGX configurations:

* **NVLink 4** — 50 GB/s per link, 18 links per GPU → 900 GB/s aggregate per GPU.
* **NVSwitch v3** — 4 switches in the HGX baseboard interconnect the 8 GPUs in an all-to-all topology.
* **Each pair of GPUs has 900 GB/s effective bandwidth.**
* **Latency** — sub-microsecond for small messages.

This fabric is the reason **TP=8 is practical at all**. Without NVLink, TP across 8 GPUs through PCIe (128 GB/s aggregate to host, 64 GB/s peer-to-peer if not blocked) would be **10–14× slower**.

**3.1 NVLink Switch chiplets and the H200 NVL configuration**

Newer HGX configurations and the upcoming **NVL** chassis use NVLink switches as separate chips that interconnect across servers. For inference-on-single-node-server workloads (which is the entire scope of this lecture), the baseboard NVSwitch is what matters.

---

**4. TP=2 vs TP=4 vs TP=8**

For Llama 3.3 70B / Qwen 2.5 72B:

| TP | Per-GPU weight share (FP16) | Per-GPU KV share | Communication overhead | When to use |
|----|-----------------------------|-------------------|------------------------|-------------|
| 1 | doesn't fit at FP16 | n/a | none | INT4 only on H200 |
| 2 | ~70 GB | 50% | ~10% of step time | H100 NVL or H200 single-node |
| 4 | ~35 GB | 25% | ~15% of step time | best chat-shape throughput |
| 8 | ~18 GB | 12.5% | ~25% of step time | max batch / long context |

**4.1 The TP scaling efficiency**

Ideal: 4× more GPUs → 4× more throughput.
Real: 4× more GPUs → 2.5–3.5× more throughput at this hardware/model.

The gap is **communication**. The plot looks like:

```text
   per-replica throughput
        ▲
        │       •  TP=2   (95% efficiency vs single-GPU)
        │   •        TP=4   (85% efficiency)
        │            TP=8 •  (65–75% efficiency)
        │
        └────────────────────► TP degree
```

Scaling efficiency depends on workload:

* **Chat (decode-dominant, batch=32):** TP=4 is the sweet spot. TP=8 sacrifices ~25% to gain headroom for KV cache / longer context.
* **Batch / offline (prefill-dominant):** TP=8 can win because prefill is compute-bound; FFN matmuls amortize the all-reduce cost.
* **Long context (128K):** TP=8 because the KV cache pressure forces it; the throughput hit is acceptable because long-context throughput is bottlenecked elsewhere.

**4.2 The per-GPU throughput trap**

A common metric mistake: comparing TP=4 vs TP=8 in **per-replica throughput**.

* TP=4 at 1200 tok/s → 300 tok/s/GPU.
* TP=8 at 1600 tok/s → 200 tok/s/GPU.

TP=8 has **higher absolute throughput** (better TPOT, more users served per replica) but **worse $/MTok** if each H200 has a fixed cost.

For chat products, **TP=4 is usually the cost-efficient pick**. TP=8 is for cases where TP=4 cannot fit the workload (e.g., 128K context at large batch).

**4.3 When the TP size is not yours to choose — FP8 block alignment**

There is a hard constraint that overrides the cost/throughput reasoning above: **block-scaled FP8 weights require every tensor-parallel shard's dimension to be a whole number of quantization blocks.** If `dim / TP` is not a multiple of the FP8 block size, the engine fails to load.

For Qwen 2.5 72B the FFN intermediate `29568 = 128 × 231` is *not* divisible by 128 after splitting across 2, 4, or 8 GPUs — so block-FP8 + TP=8 will not load until you drop to a TP that re-aligns (or use a framework build that pads). The mechanism and the fix-order are in [Lecture 03 §5.4](Lecture-03.md). The takeaway for *this* lecture: when you serve FP8, **validate that the model loads at your chosen TP before you reason about throughput** — quantization can force a smaller TP than the workload alone would pick.

---

**5. Runtime-specific TP configuration**

**5.1 vLLM**

```python
from vllm import LLM, SamplingParams

llm = LLM(
    model="meta-llama/Llama-3.3-70B-Instruct",
    tensor_parallel_size=4,        # the TP degree
    dtype="float16",
    gpu_memory_utilization=0.92,
)
```

vLLM auto-discovers GPUs and assigns ranks. Works on standard HGX systems out of the box.

**5.2 SGLang**

```bash
python -m sglang.launch_server \
    --model-path Qwen/Qwen2.5-72B-Instruct \
    --tp-size 4 \
    --dtype float16 \
    --port 30000
```

SGLang's TP integrates with its RadixAttention and EP support. Same fabric assumptions as vLLM.

**5.3 TensorRT-LLM**

```bash
trtllm-build \
    --checkpoint_dir ./fp8_checkpoint \
    --output_dir ./engine \
    --tp_size 4 \
    ...
```

TRT-LLM compiles the model graph for the chosen TP size. Changing TP requires a rebuild — this is a deployment-time decision, not a runtime config.

</details>

### 5.4 跨 runtime 精度一致性

对于 FP16、TP=4、batch=32 的 Llama 3.3 70B，在 4× H100 上：

| runtime | TPOT（均值） | 吞吐 tok/s/GPU |
|---------|-------------|----------------------|
| vLLM 0.22 | ~30 ms | ~310 |
| SGLang 0.5 | ~29 ms | ~315 |
| TRT-LLM 1.3 BF16 | ~26 ms | ~360 |
| TRT-LLM 1.3 FP8 | ~16 ms | ~580 |

数字为近似值；请在你的实验室复现。TRT-LLM 的领先主要来自 **Hopper 上的 kernel 质量**（FA4、WGMMA、TMA）以及 FP8 的成熟度。vLLM 正在一个季度一个季度地缩小差距。

---

## 6. 诊断 —— all-reduce 是瓶颈吗？

一次 profile 驱动的诊断：

1. **Nsight Systems 时间线** —— 查看 NCCL kernel 时间占 step 时间的比例。
2. **逐层拆解** —— `nsys` 显示每层的 attention 矩阵乘 + all-reduce + FFN 矩阵乘 + all-reduce。
3. **NCCL 通信时间** —— 是否与计算重叠？在 Hopper 上，vLLM 0.22+ 和 TRT-LLM 使用基于 `cudaStream` 的重叠；较旧的 runtime 则是串行。

快速诊断：

| 如果看到 | 可疑原因 |
|------------|---------|
| NCCL 占 step 时间 > 30% | all-reduce 主导 —— 尝试更少的 GPU 或更小的消息 |
| NCCL 与计算串行（无重叠） | runtime 过旧 —— 升级 |
| 单 GPU FLOPs 达成率低 | kernel 质量 / 批大小 —— 尝试更大的批 |
| step 时间随 TP 扩展性差 | all-reduce —— TP=4 → TP=8 很少能达到 2× |

### 6.1 reduce-scatter + all-gather 模式

对于非常大的 all-reduce，NCCL 将其实现为 reduce-scatter（之后每块 GPU 持有该向量中一个完全规约后的 *chunk*）+ all-gather（随后每块 GPU 收集其余的 chunk）。这种分解 *就是* 带宽最优的 ring all-reduce：每块 GPU 搬运 `2(N-1)/N ≈ 2×` 该张量的字节数 —— all-reduce 本质上要传输约两倍于其所规约张量的数据量，这就是通信账单之所以如此的原因。

---

## 7. 序列并行 —— FFN 叠加

一个微妙但重要的优化：**序列并行**（SP）通过在非矩阵乘操作（RMSNorm、residual add 等）期间把序列维度切分到各 GPU 上，降低 TP 的激活值内存开销。

没有 SP 时，TP 切分的模型在 RMSNorm 和 residual 期间会在各 GPU 上**复制完整的激活值**。SP 也把这些操作切分 —— 每块 GPU 只持有 `seq/N` 的激活值。

* **对长上下文工作负载有利** —— 在 128K 上下文时，SP 节省大量 HBM。
* **vLLM 0.22+** 在设置 `--enable-sequence-parallelism` 时透明集成 SP。
* **TRT-LLM** 的 SP 位于一个 flag 之后。

对于 TP=8 + SP，通信量翻倍（用 reduce-scatter + all-gather 取代 all-reduce），但每次通信更小。在激活值内存是约束的长上下文工作负载中，净收益为正。

---

## 实验 —— 测量 Llama 3.3 70B 的 TP 扩展性

目标：在同一硬件上得到 TP=2 / 4 / 8 的吞吐数字，并标注 all-reduce 开销。

1. **硬件** —— 8× H100 SXM（HGX），或者如果有条件用 8× H200。
2. **模型** —— Llama 3.3 70B Instruct，FP16（如果有 TRT-LLM 则用 FP8）。
3. **Runtime** —— vLLM 0.22+ V1。
4. **在三个 TP 度数上做 benchmark** —— TP=2、4、8。相同的批大小（32）、相同的 prompt（1024）、相同的输出（256）、相同的迭代次数（100，其中 20 次预热）。
5. **用 Nsight Systems profile 一次 TP=4 和一次 TP=8 的运行**。找出 NCCL 占 step 时间的比例。
6. **计算** 单 GPU 吞吐、扩展效率和每副本吞吐。
7. **绘制** 扩展曲线。在每个 TP 处标注瓶颈。

通过标准：你能用实测数字为 32K 上下文下的聊天产品选择 TP 做出辩护 —— 并解释为什么长上下文批处理产品会选另一个 TP。

---

## 自检

1. 对于 4× H100 SXM 上的 Llama 3.3 70B FP16，预测 batch=32 时每个 token decode（逐 token 生成阶段）的 all-reduce 时间。使用 900 GB/s 的 NVLink 带宽和每个 token 160 次 all-reduce。
2. 一位同事提议对聊天产品用 TP=8 来“把吞吐翻倍”。TP=4 → TP=8 的实测显示吞吐为 1.5×，单 GPU 效率为 65%。用 $/MTok 的推理，在两句话内支持或否决。
3. 你的 Nsight trace 显示 NCCL kernel 与计算 kernel 串行执行（无重叠）。你会先尝试升级哪个 runtime？
4. 序列并行是“免费”的内存节省 —— 为什么它默认不开？代价是什么？
5. 在相同硬件上，TP=4 的 Qwen 2.5 72B 与 TP=4 的 Llama 3.3 70B 相比，哪个的扩展效率更差，为什么？


<details>
<summary>English original</summary>

**5.4 Cross-runtime parity**

For Llama 3.3 70B at FP16, TP=4, batch=32 on 4× H100:

| Runtime | TPOT (mean) | Throughput tok/s/GPU |
|---------|-------------|----------------------|
| vLLM 0.22 | ~30 ms | ~310 |
| SGLang 0.5 | ~29 ms | ~315 |
| TRT-LLM 1.3 BF16 | ~26 ms | ~360 |
| TRT-LLM 1.3 FP8 | ~16 ms | ~580 |

Numbers approximate; replicate in your lab. TRT-LLM's lead is mostly **kernel quality on Hopper** (FA4, WGMMA, TMA) and FP8 maturity. vLLM is closing the gap quarter by quarter.

---

**6. Diagnosis — is the all-reduce the bottleneck?**

A profile-driven diagnosis:

1. **Nsight Systems timeline** — look for NCCL kernel time as a fraction of step time.
2. **Per-layer breakdown** — `nsys` shows attention matmul + all-reduce + FFN matmul + all-reduce per layer.
3. **NCCL communication time** — is it overlapping with compute? On Hopper, vLLM 0.22+ and TRT-LLM use `cudaStream`-based overlap; older runtimes serialize.

Quick diagnostic:

| If you see | Suspect |
|------------|---------|
| NCCL > 30% of step | All-reduce dominant — try fewer GPUs or smaller messages |
| NCCL serial with compute (no overlap) | Outdated runtime — upgrade |
| Per-GPU FLOPs achievement low | Kernel quality / batch size — try larger batch |
| Step time scales poorly with TP | All-reduce — TP=4 → TP=8 is rarely 2× |

**6.1 The reduce-scatter + all-gather pattern**

For very large all-reduces, NCCL implements it as reduce-scatter (after which each GPU holds one fully-reduced *chunk* of the vector) + all-gather (each GPU then collects the remaining chunks). This decomposition *is* the bandwidth-optimal ring all-reduce: each GPU moves `2(N-1)/N ≈ 2×` the tensor's bytes — an all-reduce inherently transfers about twice the data volume of the tensor it reduces, which is why the communication bill is what it is.

---

**7. Sequence parallelism — the FFN overlay**

A subtle but important optimization: **sequence parallelism** (SP) reduces the activation memory cost of TP by partitioning the sequence dimension across GPUs during non-matmul operations (RMSNorm, residual add, etc.).

Without SP, TP-partitioned models **duplicate full activations** across GPUs during RMSNorm and residual. SP partitions those operations too — each GPU holds only `seq/N` worth of activations.

* **Wins for long-context workloads** — at 128K context, SP saves substantial HBM.
* **vLLM 0.22+** integrates SP transparently when `--enable-sequence-parallelism` is set.
* **TRT-LLM** has SP behind a flag.

For TP=8 + SP, communication doubles (reduce-scatter + all-gather instead of all-reduce), but each communication is smaller. Net win for long-context workloads where the activation memory was the constraint.

---

**Lab — measure TP scaling on Llama 3.3 70B**

Goal: produce TP=2 / 4 / 8 throughput numbers on the same hardware, with the all-reduce cost annotated.

1. **Hardware** — 8× H100 SXM (HGX) or 8× H200 if you have access.
2. **Model** — Llama 3.3 70B Instruct, FP16 (or FP8 if you have TRT-LLM).
3. **Runtime** — vLLM 0.22+ V1.
4. **Bench at three TP degrees** — TP=2, 4, 8. Same batch size (32), same prompt (1024), same output (256), same iterations (100, with 20 warmup).
5. **Profile one TP=4 and one TP=8 run** with Nsight Systems. Identify NCCL fraction of step time.
6. **Compute** per-GPU throughput, scaling efficiency, and per-replica throughput.
7. **Plot** the scaling curve. Annotate the bottleneck at each TP.

Pass criterion: you can defend the choice of TP for a chat product at 32K context with measured numbers — and explain why a different TP would be picked for a long-context batch product.

---

**Self-check**

1. For Llama 3.3 70B FP16 on 4× H100 SXM, predict the all-reduce time per token decode at batch=32. Use 900 GB/s NVLink bandwidth and 160 all-reduces per token.
2. A teammate proposes TP=8 for a chat product to "double throughput." The TP=4 → TP=8 measurement shows 1.5× throughput at 65% per-GPU efficiency. Defend or reject in two sentences using $/MTok reasoning.
3. Your Nsight trace shows NCCL kernels running serially with compute kernels (no overlap). What runtime upgrade would you try first?
4. Sequence parallelism is "free" memory savings — why isn't it on by default? What is the cost?
5. For Qwen 2.5 72B at TP=4 vs Llama 3.3 70B at TP=4 on the same hardware, which has worse scaling efficiency and why?

---

</details>

## References

* Megatron-LM 张量并行论文 — [arXiv:1909.08053](https://arxiv.org/abs/1909.08053)
* "Reducing Activation Recomputation in Large Transformer Models" — [arXiv:2205.05198](https://arxiv.org/abs/2205.05198) — 序列并行
* NCCL 文档 — [docs.nvidia.com/deeplearning/nccl/](https://docs.nvidia.com/deeplearning/nccl/)
* NVIDIA HGX H100 平台简介 — [nvidia.com/en-us/data-center/hgx/](https://www.nvidia.com/en-us/data-center/hgx/)
* vLLM 张量并行文档 — [docs.vllm.ai/en/latest/serving/distributed_serving.html](https://docs.vllm.ai/en/latest/serving/distributed_serving.html)
* TensorRT-LLM 多 GPU — [nvidia.github.io/TensorRT-LLM/](https://nvidia.github.io/TensorRT-LLM/)
* DistServe（用于 disaggregation 对比）— [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)

Cross-references:

* [阶段 5 → ML Systems Engineering Guide → Stage 5 Distributed Training Systems](../../Guide.md) — 训练侧基础
* [阶段 5 → GPU Infrastructure → 8x-H200-Training-Inference → 02 Training Setup](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/01-8x-H200训练推理/02-Training-Setup.md)

---

## Current as of 2026-06

NCCL 2.30+、vLLM 0.22+ V1、SGLang 0.5+、TRT-LLM 1.3+、NVLink 4、NVSwitch v3。当 NCCL 3.0 / vLLM V1 进一步稳定 / 新一代 NVLink 落地时更新。

---

## Next

* Next: [Lecture 05 — Modern serving stack: continuous batching, paged KV, prefix cache, speculation](Lecture-05.md)
* Previous: [Lecture 03 — Quantizing Llama 3.3 70B and Qwen 2.5 72B](Lecture-03.md)
* Up: [Part 2 — Dense at Hopper](README.md)


<details>
<summary>English original</summary>

**References**

* Megatron-LM tensor parallelism paper — [arXiv:1909.08053](https://arxiv.org/abs/1909.08053)
* "Reducing Activation Recomputation in Large Transformer Models" — [arXiv:2205.05198](https://arxiv.org/abs/2205.05198) — sequence parallelism
* NCCL documentation — [docs.nvidia.com/deeplearning/nccl/](https://docs.nvidia.com/deeplearning/nccl/)
* NVIDIA HGX H100 platform brief — [nvidia.com/en-us/data-center/hgx/](https://www.nvidia.com/en-us/data-center/hgx/)
* vLLM TP documentation — [docs.vllm.ai/en/latest/serving/distributed_serving.html](https://docs.vllm.ai/en/latest/serving/distributed_serving.html)
* TensorRT-LLM multi-GPU — [nvidia.github.io/TensorRT-LLM/](https://nvidia.github.io/TensorRT-LLM/)
* DistServe (referenced for disaggregation contrast) — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)

Cross-references:

* [Phase 5 → ML Systems Engineering Guide → Stage 5 Distributed Training Systems](../../Guide.md) — the training-side foundation
* [Phase 5 → GPU Infrastructure → 8x-H200-Training-Inference → 02 Training Setup](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/01-8x-H200训练推理/02-Training-Setup.md)

---

**Current as of 2026-06**

NCCL 2.30+, vLLM 0.22+ V1, SGLang 0.5+, TRT-LLM 1.3+, NVLink 4, NVSwitch v3. Refresh when NCCL 3.0 / vLLM V1 stabilizes further / a new NVLink generation lands.

---

**Next**

* Next: [Lecture 05 — Modern serving stack: continuous batching, paged KV, prefix cache, speculation](Lecture-05.md)
* Previous: [Lecture 03 — Quantizing Llama 3.3 70B and Qwen 2.5 72B](Lecture-03.md)
* Up: [Part 2 — Dense at Hopper](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 2 - Dense at Hopper/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%202%20-%20Dense%20at%20Hopper/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
