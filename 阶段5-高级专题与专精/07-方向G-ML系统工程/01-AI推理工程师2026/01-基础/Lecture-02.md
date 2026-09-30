---
title: 第 1 部分 · 第 02 讲 — Transformer 执行：从 token 到比特
description: 第 1 部分 · 第 02 讲 — Transformer 执行：从 token 到比特
---

# 第 1 部分 · 第 02 讲 — Transformer 执行：从 token 到比特

## 概述

推理工程建立在一个机械性的洞见之上：现代 decoder-only Transformer 在跑推理时，会在两种完全不同的成本区间里完成**两件完全不同的工作**，而这两件工作由一个数据结构（KV cache）连接起来 —— 该结构单调增长，并在长上下文时压倒一切。

这两件工作就是 **prefill**（首字前的整段计算）和 **decode**（逐 token 生成阶段）。在纸面上，它们的前向传播是同一个；在实践中，它们的瓶颈完全不同。本课程第 2 部分和第 3 部分里的每一项优化，要么降低其中之一的成本，要么在两者之间做取舍。

本讲自底向上构建模型：

1. decoder-only Transformer 的两阶段前向传播。
2. KV cache 解剖 —— 形状、增长、每 token 字节数的计算。
3. FLOPs 花在哪里 vs 字节花在哪里 —— 算力 / 带宽的分野。
4. 三种区间 —— 算力受限、带宽受限、调度器受限 —— 以及如何诊断自己处于哪一种。
5. 一个实验，为某个具体模型 + GPU 给出上述所有内容的数字。

读完之后，你应该仅凭一张 model card 就能推导出：为什么 70B 模型在 H200 上 decode 只有约 30 tokens/sec —— 以及每个杠杆（批大小、精度、投机）会把这个数字变成什么样。

---

## 1. 两个阶段 —— prefill 和 decode

自回归生成文本的 decoder-only Transformer 做两种操作：

### 1.1 Prefill —— 处理 prompt

```text
input: prompt of P tokens
output: hidden states for all P positions
        + populated KV cache (K and V tensors for every layer, every position)
```

具体来说，对 L 个 Transformer layer 中的每一个：

> **RMSNorm** 在每个 sub-layer 之前把每个 token 向量重新缩放到稳定量级（pre-norm）。它是逐元素的、带宽受限的，成本约 0.5 FLOP/B —— 单个看很便宜，但每个 token 在 80 个 layer 上要调用 160 次。完整直觉（RMS 度量的是什么、它为什么取代 LayerNorm、以及 ε 起什么作用）见 [第 2 部分 → 第 01 讲 §1.1](../02-Hopper稠密计算/Lecture-01.md)。

```text
x ── RMSNorm ─► QKV projection ──► (Q, K, V each of shape [P, h_q × head_dim] / [P, h_kv × head_dim])
                                    │
                                    ├── K and V stored into KV cache at positions 0..P-1
                                    └── attention(Q, K_cache, V_cache) → output projection → residual
            ── RMSNorm ─► gate/up projections → SiLU(gate) * up → down projection → residual
```

关键成本形态：

* **prompt 的全部 P 个位置，在每个 layer 的每个 projection 上都在一次矩阵乘中处理完。**
* Q/K/V 矩阵乘是 `(P, d) @ (d, h × head_dim)` —— 一次真正的 GEMM（矩阵-矩阵乘），其算术强度随 P 增长。
* attention 是一个 `(P, P)` 的 softmax，输出为 `(P, head_dim)` —— 对 P 呈二次方。
* FFN 的 gate/up/down 矩阵乘是 `(P, d) @ (d, d_ff)` 和 `(P, d_ff) @ (d_ff, d)` —— 在长 P 时是 FLOPs 最重的阶段，远超其他。

只要 prompt 超过几百个 token，prefill 就是**算力受限**的。Tensor Core 能跑到接近峰值。这正是 Hopper / Blackwell 上 **FP8 / FP4 吞吐**能带来收益的区间。

### 1.2 Decode —— 一次吐一个 token

```text
loop while not done:
    take previous output token
    for each layer:
        x ── RMSNorm ─► QKV projection (only for *current* token, single row)
                        ├── K and V appended to KV cache at position P+t
                        └── attention(Q, K_cache_so_far, V_cache_so_far) → single row out
        FFN on single row
    sample next token
```

关键成本形态：

* 现在每个 projection 都是 `(1, d) @ (d, h × head_dim)` —— 一个 **GEMV**（矩阵-向量乘），而不是 GEMM。
* 权重矩阵很大（GB 级），每生成一个 token 都要*从 HBM 完整重读一遍*。几乎没有可用于喂饱 Tensor Core 的每字节 FLOPs。
* 每生成一个新 token，attention 都要读*整个* KV cache（长度为 P+t）。**KV 读取带宽**随上下文线性增长。

在 batch=1 时，decode 是**带宽受限**的 —— 几乎全部时间都花在**从 HBM 读取**权重 + KV 上。这就是为什么 decode 的 TPOT 跟随的是 HBM 带宽，而不是 FLOPs。

### 1.3 心智图景

```text
prefill:                         decode:
┌──────────────────────────┐     ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
│   P×d × d×4d FFN GEMM    │     │1│ │1│ │1│ │1│ │1│  ← batch=1 GEMV per step
│  compute-bound, fp8 wins │     └─┘ └─┘ └─┘ └─┘ └─┘
└──────────────────────────┘      ▲   ▲   ▲   ▲   ▲
                                  │   │   │   │   │
                                 bandwidth-bound: HBM read of W
                                 every step + growing KV cache
```

两个截然不同的优化问题，套着同一条代码路径。


<details>
<summary>English original</summary>

**Part 1 · Lecture 02 — Transformer Execution, From Tokens to Bits**

**Overview**

Inference engineering rests on one mechanical insight: a modern decoder-only transformer running inference does **two completely different jobs** in two completely different cost regimes, joined by one data structure (the KV cache) that grows monotonically and dominates everything at long context.

The two jobs are **prefill** and **decode**. They have the same forward pass on paper. They have entirely different bottlenecks in practice. Every optimization in Parts 2 and 3 of this course either reduces the cost of one of them, or trades one against the other.

This lecture builds the model from the bottom:

1. The two-phase forward pass of a decoder-only transformer.
2. The KV cache anatomy — shape, growth, bytes-per-token math.
3. Where the FLOPs go vs where the bytes go — the compute / bandwidth split.
4. The three regimes — compute-bound, memory-bound, scheduler-bound — and how to diagnose which one you are in.
5. A lab that puts numbers on all of the above for one specific model + GPU.

By the end you should be able to derive, from a model card alone, why a 70B model decodes at ~30 tokens/sec on H200 — and what each lever (batch size, precision, speculation) would do to that number.

---

**1. The two phases — prefill and decode**

A decoder-only transformer generating text autoregressively does two operations:

**1.1 Prefill — process the prompt**

```text
input: prompt of P tokens
output: hidden states for all P positions
        + populated KV cache (K and V tensors for every layer, every position)
```

Concretely, for each of the L transformer layers:

> **RMSNorm** rescales each token vector to a stable magnitude before every sub-layer (pre-norm). It is elementwise, bandwidth-bound, and costs ~0.5 FLOP/B — cheap individually but called 160 times across 80 layers per token. For the full intuition (what RMS measures, why it replaces LayerNorm, and what ε does) see [Part 2 → Lecture 01 §1.1](../02-Hopper稠密计算/Lecture-01.md).

```text
x ── RMSNorm ─► QKV projection ──► (Q, K, V each of shape [P, h_q × head_dim] / [P, h_kv × head_dim])
                                    │
                                    ├── K and V stored into KV cache at positions 0..P-1
                                    └── attention(Q, K_cache, V_cache) → output projection → residual
            ── RMSNorm ─► gate/up projections → SiLU(gate) * up → down projection → residual
```

Key cost shape:

* **All P positions of the prompt are processed in one matrix multiplication per layer per projection.**
* The Q/K/V matmul is `(P, d) @ (d, h × head_dim)` — a real GEMM, with arithmetic intensity that scales with P.
* The attention is a `(P, P)` softmax with a `(P, head_dim)` output — quadratic in P.
* The FFN gate/up/down matmuls are `(P, d) @ (d, d_ff)` and `(P, d_ff) @ (d_ff, d)` — by far the FLOP-heaviest stage at long P.

Prefill is **compute-bound** for any prompt longer than a few hundred tokens. Tensor cores hit close to peak. This is the regime that benefits from **FP8 / FP4 throughput** on Hopper / Blackwell.

**1.2 Decode — emit one token at a time**

```text
loop while not done:
    take previous output token
    for each layer:
        x ── RMSNorm ─► QKV projection (only for *current* token, single row)
                        ├── K and V appended to KV cache at position P+t
                        └── attention(Q, K_cache_so_far, V_cache_so_far) → single row out
        FFN on single row
    sample next token
```

Key cost shape:

* Every projection is now `(1, d) @ (d, h × head_dim)` — a **GEMV** (matrix-vector), not a GEMM.
* The weight matrices are large (gigabytes) and *fully re-read from HBM* for each token. There is essentially no FLOP-per-byte to keep tensor cores busy.
* Attention reads the *entire* KV cache (length P+t) for each new token. **KV-read bandwidth** grows linearly with context.

Decode is **bandwidth-bound** at batch=1 — almost all of the time is **HBM read** of the weights + KV. This is why decode TPOT tracks HBM bandwidth, not FLOPs.

**1.3 The mental picture**

```text
prefill:                         decode:
┌──────────────────────────┐     ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
│   P×d × d×4d FFN GEMM    │     │1│ │1│ │1│ │1│ │1│  ← batch=1 GEMV per step
│  compute-bound, fp8 wins │     └─┘ └─┘ └─┘ └─┘ └─┘
└──────────────────────────┘      ▲   ▲   ▲   ▲   ▲
                                  │   │   │   │   │
                                 bandwidth-bound: HBM read of W
                                 every step + growing KV cache
```

Two completely different optimization problems wearing the same code path.

</details>

### 1.4 为什么批处理有助于 decode（逐 token 生成阶段）

如果在同一步为 `B` 个并发请求做 decode，权重读取是**共享的**：

* 不使用批处理：B × (读取 W + 1 次矩阵乘) = B 次权重读取
* 使用批处理：1 × 读取 W + B 行矩阵乘（仍然受带宽限制，但被摊销）

有效吞吐会**几乎随 B 线性增长**，直到矩阵乘达到计算上限，或 KV cache 填满 HBM。这正是**连续批处理**（vLLM 的招牌特性）成为 2023 年突破的原因：它无需改变模型，就将 decode 吞吐提升 10–50×。

---

## 2. KV cache——解剖

KV cache 是仅解码器推理的**承重数据结构**。要彻底理解它。

### 2.1 它存储什么

对每个 layer，存储每个过去 token 产生的 K 和 V 张量。形状：

```text
K_cache: [batch, num_kv_heads, seq_len, head_dim]
V_cache: [batch, num_kv_heads, seq_len, head_dim]
```

存储*跨所有 L 个 layer*。

### 2.2 每请求、每 token 字节数

资深工程师能在白板上推导出的公式：

```text
kv_bytes_per_token = 2 (K and V)
                   × L (layers)
                   × num_kv_heads
                   × head_dim
                   × bytes_per_element
```

计算示例：

**Llama 3.3 70B** — L=80、num_kv_heads=8、head_dim=128、FP16（2 字节）：

```text
2 × 80 × 8 × 128 × 2 = 327,680 bytes/token ≈ 320 KB/token
```

**Qwen 2.5 72B** — L=80、num_kv_heads=8、head_dim=128、FP16 — 与 Llama 3.3 **完全相同**：

```text
2 × 80 × 8 × 128 × 2 ≈ 320 KB/token
```

两者都采用 **分组查询注意力**，有 8 个 KV heads，且深度相同 → 每 token 的 KV 开销相同。这*并非*巧合；这是两个团队都收敛到的分组查询注意力配置。这一点将在 [Part 2 Lecture 01](../02-Hopper稠密计算/Lecture-01.md) 中再次讨论。

**带 MLA 的 DeepSeek V3.1** — 多头潜在注意力通过存储一个小的 *latent* 向量而非完整 K/V 来压缩 KV cache。每 token 字节数计算**大幅降低**。将在 Part 3 计算。

### 2.3 规模下的 KV cache 开销

在长上下文下，KV cache 成为 HBM 的主要消耗者：

| 上下文长度 | Llama 3.3 70B FP16 KV | FP8 KV | INT4 KV |
|----------------|------------------------|---------|----------|
| 4,096 | 1.3 GB | 0.7 GB | 0.3 GB |
| 16,384 | 5.2 GB | 2.6 GB | 1.3 GB |
| 65,536 | 21 GB | 10.5 GB | 5.3 GB |
| 131,072 | 42 GB | 21 GB | 10.5 GB |

**每请求。** 在 batch=16、32K 上下文下，仅 KV cache 在 FP16 下就是 ~168 GB——超过 H100 80 GB 容量的两倍；即使 batch=8（~84 GB）也已超出，迫使采用 FP8 KV 或更小 batch。这就是为什么**长上下文推理服务会先迫使 KV 降低精度**，之后才迫使权重降低精度。

### 2.4 KV cache 的带宽一半

除容量外，KV cache 在每个 decode 步都会被*读取*：

```text
KV bandwidth per decode step = 2 (K and V) × L × h_kv × head_dim × seq_len × bytes_per_element
                             = kv_bytes_per_token × seq_len
```

对于 Llama 3.3 70B，在 64K 上下文、FP16 KV 下：320 KB × 65,536 ≈ **每个 decode 步 20 GB**。在 H200（4.8 TB/s HBM3e）上，仅这次读取就 **~4 ms**——还没算任何权重矩阵乘，还没算任何采样。这就是为什么即使 FLOPs 不变，**decode 的 TPOT 也会随上下文长度增长**。

解决方法是降低 KV 精度（FP8 将读取时间减半）或采用稀疏 / 滑动窗口 attention（读取更少 KV token）。

---


<details>
<summary>English original</summary>

**1.4 Why batching helps decode**

If you decode for `B` concurrent requests at the same step, the weight read is **shared**:

* Without batching: B × (read W + 1 matmul) = B weight reads
* With batching: 1 × read W + B-row matmul (still bandwidth-bound but amortized)

Effective throughput scales **nearly linearly with B** until the matmul reaches the compute ceiling or the KV cache fills HBM. This is the entire reason **continuous batching** (vLLM's headline feature) was the breakthrough of 2023: it raises decode throughput by 10–50× without changing the model.

---

**2. The KV cache — anatomy**

The KV cache is the **load-bearing data structure** of decoder-only inference. Understand it cold.

**2.1 What it stores**

For each layer, the K and V tensors produced by every past token. Shape:

```text
K_cache: [batch, num_kv_heads, seq_len, head_dim]
V_cache: [batch, num_kv_heads, seq_len, head_dim]
```

Stored *across all L layers*.

**2.2 Bytes per token, per request**

The formula a senior can derive on a whiteboard:

```text
kv_bytes_per_token = 2 (K and V)
                   × L (layers)
                   × num_kv_heads
                   × head_dim
                   × bytes_per_element
```

Worked examples:

**Llama 3.3 70B** — L=80, num_kv_heads=8, head_dim=128, FP16 (2 bytes):

```text
2 × 80 × 8 × 128 × 2 = 327,680 bytes/token ≈ 320 KB/token
```

**Qwen 2.5 72B** — L=80, num_kv_heads=8, head_dim=128, FP16 — **identical** to Llama 3.3:

```text
2 × 80 × 8 × 128 × 2 ≈ 320 KB/token
```

Both share **GQA** with 8 KV heads and the same depth → identical KV cost per token. This is *not* a coincidence; it is the GQA configuration both teams converged on. We will return to this in [Part 2 Lecture 01](../02-Hopper稠密计算/Lecture-01.md).

**DeepSeek V3.1 with MLA** — Multi-head Latent Attention compresses the KV cache by storing a small *latent* vector instead of full K/V. The bytes-per-token math is **dramatically lower**. We will compute it in Part 3.

**2.3 KV cache cost at scale**

At long context the KV cache becomes the dominant HBM consumer:

| Context length | Llama 3.3 70B FP16 KV | FP8 KV | INT4 KV |
|----------------|------------------------|---------|----------|
| 4,096 | 1.3 GB | 0.7 GB | 0.3 GB |
| 16,384 | 5.2 GB | 2.6 GB | 1.3 GB |
| 65,536 | 21 GB | 10.5 GB | 5.3 GB |
| 131,072 | 42 GB | 21 GB | 10.5 GB |

**Per request.** At batch=16 with 32K context, the KV cache alone is ~168 GB at FP16 — more than double H100 80 GB capacity; even batch=8 (~84 GB) already blows past it, forcing FP8 KV or smaller batch. This is why **long-context serving forces precision drops on KV** before it forces them on weights.

**2.4 The bandwidth half of the KV cache**

Beyond capacity, the KV cache is *read* at every decode step:

```text
KV bandwidth per decode step = 2 (K and V) × L × h_kv × head_dim × seq_len × bytes_per_element
                             = kv_bytes_per_token × seq_len
```

For Llama 3.3 70B at 64K context, FP16 KV: 320 KB × 65,536 ≈ **20 GB per decode step**. On H200 (4.8 TB/s HBM3e) that read alone is **~4 ms** — before any weight matmul, before any sampling. This is why **decode TPOT grows with context length** even though FLOPs do not.

The fix is either KV precision drop (FP8 cuts the read time in half) or sparse / sliding-window attention (read fewer KV tokens).

---

</details>

## 3. FLOPs 用在哪里 vs 字节用在哪里

对于 Transformer 的一个 step，可以把开销拆成三列：

| 阶段 | FLOPs 缩放（decode batch=1，逐 token 生成阶段） | HBM 读取字节缩放 |
|-------|--------------------------------|------------------------|
| QKV 投影 | O(d²) | O(d²)（读取 W） |
| attention | O(seq × d) | O(seq × kv_size_per_token) |
| 输出投影 | O(d²) | O(d²) |
| FFN gate + up | O(d × d_ff) | O(d × d_ff)（读取 W） |
| FFN down | O(d_ff × d) | O(d_ff × d)（读取 W） |

batch=1 时每个 decode step 的总开销：

* **FLOPs ≈ 2 × P**，其中 P 是参数数量。
* **HBM 字节 ≈ model_size_in_bytes + kv_read_bytes** ≈ P × bytes_per_param + seq × kv_bytes_per_token。

比值（FLOPs / bytes）就是**算术强度**。对于 **batch=1 的 decode**，它约为：

```text
arithmetic_intensity = 2 × P / (P × bytes_per_param) = 2 / bytes_per_param
```

因此在 FP16（2 字节）下：**每读 1 字节做 1 个 FLOP**。FP8（1 字节）下：**每字节 2 个 FLOPs**。FP4（0.5 字节）下：**每字节 4 个 FLOPs**。

对比硬件的 ridge point（超过该 FLOPs/byte 上限就进入算力受限）：

| GPU | 峰值 FP16/BF16 TFLOPs | HBM 带宽（TB/s） | Ridge point（FLOPs/byte） |
|-----|------------------------|----------------------|--------------------------|
| H100 SXM | 989 | 3.35 | ~295 |
| H200 SXM | 989 | 4.80 | ~206 |
| B200 SXM | 2,250 | 8.0 | ~280 |

decode 的算术强度（1–4 FLOPs/byte）**比 ridge point 低两个数量级**。decode **是带宽受限的。永远如此。直到你开始 batch。**

**批处理大小为 B 时**，FFN 矩阵乘变成一个 `(B, d_ff)` × `(d_ff, d)` 的运算：算术强度大致随 B 线性增长，直到 GEMM（矩阵-矩阵乘）大到足以打满张量核心。这就是把 **decode 从带宽受限推向算力受限** 的杠杆——在 Hopper 上跑 70B 级模型时，通常出现在 B = 64–256 附近。

---

## 4. 三种状态 — 诊断

面对一个跑得慢的推理工作负载，**第一件事就是判断自己处在哪种状态。**

### 4.1 算力受限

* 张量核心利用率高（Nsight Compute 中 >70%）。
* HBM 带宽利用率中等（<50%）。
* 能改善它的：更低精度（FP8 → FP4）、更大的张量核心 kernel、kernel 融合。
* 不能改善它的：更多 HBM、更快的 HBM。

**快速判别：**prefill（首字前的整段计算）在序列 ≥ 512 时。embedding 推理。重度批处理的 decode（B ≥ 128）。

### 4.2 带宽受限（bandwidth-bound）

* 张量核心利用率低（<30%）。
* HBM 带宽利用率高（>峰值的 80%）。
* 能改善它的：对*读取密集*的张量（权重、KV）降精度、批处理（摊薄读取）、prefix cache（跳过读取）。
* 不能改善它的：更多 FLOPs。

**快速判别：**低并发下的 decode。带宽受限状态正是大多数聊天流量默认所处的位置。

### 4.3 调度器受限

* 张量核心**和** HBM 的利用率都低。
* GPU 时间线上出现大量气泡。
* 能改善它的：更好的批处理、更低的单请求开销、更少的 kernel 启动、CUDA Graphs、更快的 Python 侧路径。
* 不能改善它的：降精度、更快的 HBM、更多张量核心。

**快速判别：**极短的 decode（1–10 个 token）、小 batch、请求快速翻涌的 agent 循环。通常由 Python 开销主导。

### 4.4 快速诊断表

| 症状 | 可能的状态 | 首先尝试 |
|---------|---------------|------------------|
| decode 的 TPOT 随模型大小线性增长 | 带宽受限 | 对权重降精度试试 |
| decode 的 TPOT 随上下文线性增长 | KV 上的带宽受限 | 试试 FP8 KV cache |
| decode 的 TPOT 不随 batch 改善 | 已经是算力受限*或*调度器受限 | profile 以区分 |
| prompt 很长时 prefill 的 TFLOPs/s 仍低 | 怀疑 kernel 选择或精度 | 切到 FP8 / 检查 kernel |
| 带宽和 FLOPs 都低 | 调度器 / Python 开销 | 试试 CUDA Graphs、更大的 batch |

---


<details>
<summary>English original</summary>

**3. Where the FLOPs go vs where the bytes go**

For a transformer step, you can decompose cost into three columns:

| Stage | FLOPs scaling (decode batch=1) | HBM bytes read scaling |
|-------|--------------------------------|------------------------|
| QKV projection | O(d²) | O(d²) (read W) |
| Attention | O(seq × d) | O(seq × kv_size_per_token) |
| Output projection | O(d²) | O(d²) |
| FFN gate + up | O(d × d_ff) | O(d × d_ff) (read W) |
| FFN down | O(d_ff × d) | O(d_ff × d) (read W) |

The total cost per decode step at batch=1:

* **FLOPs ≈ 2 × P** where P is parameter count.
* **HBM bytes ≈ model_size_in_bytes + kv_read_bytes** ≈ P × bytes_per_param + seq × kv_bytes_per_token.

The ratio (FLOPs / bytes) is the **arithmetic intensity**. For **decode at batch=1** it is approximately:

```text
arithmetic_intensity = 2 × P / (P × bytes_per_param) = 2 / bytes_per_param
```

So at FP16 (2 bytes): **1 FLOP per byte read**. At FP8 (1 byte): **2 FLOPs per byte**. At FP4 (0.5 bytes): **4 FLOPs per byte**.

Compare to the hardware ridge point (FLOPs/byte ceiling above which you become compute-bound):

| GPU | Peak FP16/BF16 TFLOPs | HBM bandwidth (TB/s) | Ridge point (FLOPs/byte) |
|-----|------------------------|----------------------|--------------------------|
| H100 SXM | 989 | 3.35 | ~295 |
| H200 SXM | 989 | 4.80 | ~206 |
| B200 SXM | 2,250 | 8.0 | ~280 |

Decode arithmetic intensity (1–4 FLOPs/byte) is **two orders of magnitude below** the ridge point. Decode is **bandwidth-bound. Always. Until you batch.**

**With batching B**, the FFN matmul becomes a `(B, d_ff)` × `(d_ff, d)` operation: arithmetic intensity scales roughly linearly with B until the GEMM is large enough to saturate tensor cores. This is the lever that **moves decode from bandwidth-bound to compute-bound** — typically around B = 64–256 for 70B-class models on Hopper.

---

**4. The three regimes — diagnosis**

Given a slow inference workload, **the first job is to identify which regime you are in.**

**4.1 Compute-bound**

* Tensor cores at high utilization (>70% in Nsight Compute).
* HBM bandwidth at moderate utilization (<50%).
* Improves with: lower precision (FP8 → FP4), larger tensor core kernels, kernel fusion.
* Does *not* improve with: more HBM, faster HBM.

**Smell test:** prefill at sequence ≥ 512. Embedding inference. Heavily batched decode (B ≥ 128).

**4.2 Memory-bound (bandwidth-bound)**

* Tensor cores at low utilization (<30%).
* HBM bandwidth at high utilization (>80% of peak).
* Improves with: precision drop on the *read-heavy* tensors (weights, KV), batching (amortize the read), prefix cache (skip reads).
* Does *not* improve with: more FLOPs.

**Smell test:** decode at low concurrency. Memory-bound regime is where most chat traffic lives by default.

**4.3 Scheduler-bound**

* Tensor cores **and** HBM both at low utilization.
* GPU shows lots of bubbles in the timeline.
* Improves with: better batching, lower per-request overhead, fewer kernel launches, CUDA Graphs, faster Python-side path.
* Does *not* improve with: precision drops, faster HBM, more tensor cores.

**Smell test:** very short decodes (1–10 tokens), small batches, agent loops with rapid request churn. Often dominated by Python overhead.

**4.4 Quick diagnostic table**

| Symptom | Likely regime | First experiment |
|---------|---------------|------------------|
| Decode TPOT scales linearly with model size | Memory-bound | Try precision drop on weights |
| Decode TPOT scales linearly with context | Memory-bound on KV | Try FP8 KV cache |
| Decode TPOT does not improve with batch | Already compute-bound *or* scheduler-bound | Profile to disambiguate |
| Prefill TFLOPs/s low even with long prompt | Suspect kernel selection or precision | Switch FP8 / check kernel |
| Both bandwidth and FLOPs low | Scheduler / Python overhead | Try CUDA Graphs, larger batches |

---

</details>

## 5. 实验 — 针对一个模型 + 一个 GPU 给出具体数字

目标：将第 01 讲的 benchmark harness（agent 运行时框架）扩展为带分阶段测量的版本。

1. **选一个模型**（如果你已有第 01 讲的 Qwen3-4B，就继续用它）。
2. 向你的 harness **添加 `--measure prefill` 和 `--measure decode` 模式**：
   * **Prefill 模式：**（首字前的整段计算）用 `max_new_tokens=1` 输入不同长度的 prompt（128、512、2048、8192 个 token），并用 CUDA events 测量每个阶段的 GPU 时间。报告达到的 TFLOPs/s。
   * **Decode 模式：**（逐 token 生成阶段）用 `max_new_tokens=128` 输入一个 128-token 的 prompt，批大小为（1、4、16、64），并测量每 token 的 decode 延迟。
3. **计算带宽上限**，根据模型卡 + GPU 规格。将实际达到的 decode tokens/sec 与带宽上限作图。
4. **对一个 decode step 做 profiling**，使用 Nsight Systems。识别（a）attention 花费的时间占比，（b）FFN 花费的时间占比，（c）kernel 之间 Python 开销的占比。
5. **计算并绘制算术强度**，对每个 kernel 与 GPU ridge point 作图。标注每个 kernel 处于哪个 regime。

通过标准：你能展示一张 decode 吞吐（tokens/sec）对批大小的图，并叠加带宽上限，同时解释曲线为何在它所在的那个批大小处变平。

---

## 自检

1. 一个 70B 级 dense 模型在 H200 上以 batch=1 做 decode，在 FP16 下达到 32 tokens/sec。你把权重量化到 FP8（1 byte/param）。在不重新运行的情况下，预测新的 TPOT。为什么实际测量值可能比你预测的低 20–30%？
2. Llama 3.3 70B 有 80 个 layer、8 个 KV head，head_dim 128。你在 64K 上下文、FP8 KV、batch=8 下做推理服务。KV cache 占用多少 GB？在 141 GB 的 H200 上，这能与 INT4 量化权重矩阵一起放下吗？
3. 一个 agent 产品的 p99 TPOT 会随机飙到 200 ms。p50 稳定在 30 ms。张量核心在 Nsight 上显示 <10% 利用率。这属于哪个 regime，你最先做什么实验？
4. 你把 32 个 decode 请求合为一批，TPOT 却 *增加*，从每 token 35 ms 变为 70 ms。最可能错的是哪两件事？
5. 对于 DeepSeek V3.1（MoE，即混合专家模型，37B 激活参数，MLA（多头潜在注意力）attention），在同一 GPU 上，其带宽受限的 decode 上限与 70B dense 模型有何不同？写出数学推导。

---

## 参考文献

* "Reducing Activation Recomputation in Large Transformer Models" — [arXiv:2205.05198](https://arxiv.org/abs/2205.05198) — 激活内存计算
* "FlashAttention-3: Fast and Accurate Attention with Asynchrony and Low-precision" — [arXiv:2407.08608](https://arxiv.org/abs/2407.08608)
* "Efficient Memory Management for Large Language Model Serving with PagedAttention" — [arXiv:2309.06180](https://arxiv.org/abs/2309.06180) — KV cache 作为瓶颈
* NVIDIA Nsight Systems 文档 — [docs.nvidia.com/nsight-systems/](https://docs.nvidia.com/nsight-systems/)
* NVIDIA Nsight Compute 文档 — [docs.nvidia.com/nsight-compute/](https://docs.nvidia.com/nsight-compute/)

交叉引用：

* [阶段 5 → 边缘 AI → 边缘 LLM 推理内部机制](../../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) — GEMV（矩阵-向量乘）vs GEMM（矩阵-矩阵乘）
* [阶段 5 → 边缘 AI → Qwen 推理优化 → 第 03 讲 — Jetson 上的 Decode 优化](../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-03.md) — 边缘端的带宽受限 decode

---

## 截至 2026-06

数学计算固定在 FP16 / FP8 / INT4 / FP4，并采用给定的精度大小；ridge-point 数字来自 NVIDIA 公布的 H100 / H200 / B200 数据手册。如果 NVIDIA 发布修正后的峰值数字，或出现新精度（FP6、FP3），请更新。

---

## 下一讲

* 下一讲：[第 03 讲 — Roofline（性能上界模型）、带宽与存储层次](Lecture-03.md)
* 上一讲：[第 01 讲 — 2026 年推理工程师的心智模型](Lecture-01.md)
* 上一级：[第 1 部分 — 基础](README.md)


<details>
<summary>English original</summary>

**5. Lab — put numbers on this for one model + one GPU**

Goal: extend the benchmark harness from Lecture 01 with phase-aware measurements.

1. **Pick one model** (continue with Qwen3-4B from Lecture 01 if you have it).
2. **Add `--measure prefill` and `--measure decode` modes** to your harness:
   * **Prefill mode:** feed prompts of varying length (128, 512, 2048, 8192 tokens) with `max_new_tokens=1` and measure GPU-time per stage with CUDA events. Report TFLOPs/s achieved.
   * **Decode mode:** feed a 128-token prompt with `max_new_tokens=128`, batch sizes (1, 4, 16, 64), and measure per-token decode latency.
3. **Compute the bandwidth ceiling** from the model card + GPU spec. Plot achieved decode tokens/sec against the ceiling.
4. **Profile one decode step** with Nsight Systems. Identify (a) % time spent in attention, (b) % in FFN, (c) % in Python overhead between kernels.
5. **Compute and plot arithmetic intensity** for each kernel against the GPU ridge point. Annotate which regime each kernel is in.

Pass criterion: you can show a chart of decode throughput (tokens/sec) vs batch size with the bandwidth ceiling overlaid, and explain why the curve flattens at the batch size where it does.

---

**Self-check**

1. A 70B-class dense model decoding at batch=1 on H200 hits 32 tokens/sec at FP16. You quantize weights to FP8 (1 byte/param). Without re-running, predict the new TPOT. Why might the actual measurement undershoot your prediction by 20–30%?
2. Llama 3.3 70B has 80 layers, 8 KV heads, head_dim 128. You are serving at 64K context, FP8 KV, batch=8. How many GB does the KV cache occupy? Will this fit alongside an INT4-quantized weight matrix on an H200 141 GB?
3. An agent product has p99 TPOT spiking to 200 ms at random. p50 is stable at 30 ms. Tensor cores show <10% utilization on Nsight. Which regime, and what is your first experiment?
4. You batch 32 decode requests together and TPOT *increases* from 35 ms to 70 ms per token. What two things are most likely wrong?
5. For DeepSeek V3.1 (MoE with 37B active params, MLA attention), how does the bandwidth-bound decode ceiling differ from a 70B dense model on the same GPU? Sketch the math.

---

**References**

* "Reducing Activation Recomputation in Large Transformer Models" — [arXiv:2205.05198](https://arxiv.org/abs/2205.05198) — activation memory math
* "FlashAttention-3: Fast and Accurate Attention with Asynchrony and Low-precision" — [arXiv:2407.08608](https://arxiv.org/abs/2407.08608)
* "Efficient Memory Management for Large Language Model Serving with PagedAttention" — [arXiv:2309.06180](https://arxiv.org/abs/2309.06180) — the KV cache as the bottleneck
* NVIDIA Nsight Systems documentation — [docs.nvidia.com/nsight-systems/](https://docs.nvidia.com/nsight-systems/)
* NVIDIA Nsight Compute documentation — [docs.nvidia.com/nsight-compute/](https://docs.nvidia.com/nsight-compute/)

Cross-references:

* [Phase 5 → Edge AI → Edge LLM Inference Internals](../../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) — GEMV vs GEMM
* [Phase 5 → Edge AI → Qwen Inference Optimization → Lecture 03 — Decode Optimization on Jetson](../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-03.md) — bandwidth-bound decode on the edge end

---

**Current as of 2026-06**

Math pinned at FP16 / FP8 / INT4 / FP4 with the precision sizes given; ridge-point numbers from NVIDIA's published H100 / H200 / B200 datasheets. Update if NVIDIA publishes corrected peak numbers or if a new precision lands (FP6, FP3).

---

**Next**

* Next: [Lecture 03 — Roofline, bandwidth, and the memory hierarchy](Lecture-03.md)
* Previous: [Lecture 01 — The 2026 inference engineer's mental model](Lecture-01.md)
* Up: [Part 1 — Fundamentals](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 1 - Fundamentals/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%201%20-%20Fundamentals/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
