---
title: 第 01 讲 — 为什么在边缘用 Gemma 4：架构、竞争定位与关键数字
description: 第 01 讲 — 为什么在边缘用 Gemma 4：架构、竞争定位与关键数字
---

# 第 01 讲 — 为什么在边缘用 Gemma 4：架构、竞争定位与关键数字

**合集：** [Gemma 4 Edge Deployment](README.md) | **下一讲：** [第 02 讲 →](Lecture-02.md)

---

边缘 LLM 部署始终是一个尺寸与质量之间的取舍：小到装得下，强到有意义。Gemma 4 打破这一取舍的方式与以往每一个开放模型都不同，因为其架构是从第一性原理出发、为**有界内存**而构建的 —— 而非事后补丁。本讲将说明这在硅层面究竟意味着什么、Gemma 4 在竞争格局中的位置，以及如何计算那些决定某个配置能否装进你的目标平台的数字。

---

## 学习目标

学完本讲，你应当能够：

1. 描述 Gemma 4 的**交错局部/全局 attention** 机制，并解释为何它使大多数 layer 的 KV cache 随上下文长度呈 O(1) 增长。
2. 针对任意 Gemma 4 配置（批、序列、精度）计算**精确的 KV 占用**，并与纯 attention 的等效方案进行比较。
3. 说明 **QK-norm、GeGLU、GQA** 与 **256K 词表**在 Gemma 4 中的作用，并解释各自对边缘部署的影响。
4. 利用带宽上限公式，为给定的 Jetson 目标平台选择正确的 Gemma 4 模型尺寸。
5. 解释 Gemma 4 在边缘尺寸上相对 Qwen3、Phi-4 与 Llama 3.2 的**竞争定位**，并给出具体的架构原因（而不只是 benchmark 数字）。

---

## 1. 让 Gemma 4 成为边缘模型的架构

在受限设备上做推理的每个 Transformer 都面临同一个问题：KV cache。在长上下文下，KV cache 随序列长度线性增长，并可能超过权重内存 —— 在配备 64 GB 统一内存的 Jetson 上，若使用标准 attention，BF16 的 4B 模型（8 GB 权重）在 128 K token 时其 KV cache 会比权重还大得多。

Gemma 4 用**交错的局部与全局 attention** 解决这一问题，该设计继承自 Gemma 2，并在 Gemma 4 中得到扩展。

### 1.1 交错局部/全局 attention —— 关键架构事实

在标准 transformer 中，每个 attention layer 都计算全序列 attention：每个 token 关注上下文中此前所有 token。KV cache 按 `seq_len × num_kv_heads × head_dim × 2 × num_layers × batch × bytes_per_element` 增长。

Gemma 4 将大多数 attention layer 替换为**滑动窗口局部 attention**：

```text
Gemma 4 attention pattern (conceptual, 42-layer 12B example):

  Layer 0:  LOCAL   (sliding window, W=1024 tokens)
  Layer 1:  LOCAL
  Layer 2:  LOCAL
  Layer 3:  LOCAL
  Layer 4:  LOCAL
  Layer 5:  LOCAL
  Layer 6:  GLOBAL  ← 1 global per 6 local
  Layer 7:  LOCAL
  Layer 8:  LOCAL
  ...
  Layer 41: LOCAL

  Ratio: 1 GLOBAL per 6 LOCAL = ~6 global + ~36 local in 42 layers
```

**对于局部（滑动窗口）layer：**

- 每个 token 只关注最近的 W=1024 个 token。
- 每个局部 layer 的 KV cache = `batch × W × num_kv_heads × head_dim × 2 × bytes` —— **在超过 W 之后与 seq_len 无关**。
- 当 seq_len = 128 K 时：局部 KV 与 seq_len = 1024 时的局部 KV 完全相同。

**对于全局 layer：**

- 标准全上下文 attention。KV 随 seq_len 增长。
- 但其数量只有约 N_global ≈ N_layers/7 个。

这就是根本洞见：**大部分 KV cache 在上下文长度上是 O(1) 的；只有全局 attention layer 是 O(n)**。全局 layer 承担“跨文档”的整合机制；局部 layer 做廉价的顺序处理。

### 1.2 Gemma 4 模型规格

Gemma 4 以四种尺寸发布。以下为根据 Gemma 2 谱系与已发布 model card 推导出的架构近似值（确切配置可能因变体略有差异）：

| 模型 | 参数量 | 层数 | 隐藏维 | Q heads | KV heads | Head dim | FFN dim | 上下文 |
|-------|--------|--------|--------|---------|---------|---------|---------|---------|
| Gemma 4 1B | ~1.0B | 18 | 1152 | 4 | 1 | 256 | 6912 | 32K |
| Gemma 4 4B | ~4.3B | 34 | 2560 | 8 | 4 | 256 | 15360 | 128K |
| Gemma 4 12B | ~12.7B | 46 | 3840 | 16 | 8 | 256 | 24576 | 128K |
| Gemma 4 27B | ~27.4B | 62 | 5376 | 32 | 16 | 256 | 36864 | 128K |

除 1B 外均支持 128 K 上下文。4B–27B 模型全部采用 1:6 的交错局部/全局 attention 模式。

**视觉变体：** Gemma 4 4B、12B 与 27B 拥有多模态（视觉）变体，其构建围绕一个融合进 decoder 的 **SigLIP-400M** 图像编码器。详见第 05 讲。


<details>
<summary>English original</summary>

**Lecture 01 — Why Gemma 4 at the Edge: Architecture, Competitive Position, and the Numbers**

**Collection:** [Gemma 4 Edge Deployment](README.md) | **Next:** [Lecture 02 →](Lecture-02.md)

---

Edge LLM deployment has always been a size-vs-quality tradeoff: small enough to fit, capable enough to matter. Gemma 4 breaks that tradeoff differently from every prior open model because its architecture was built from first principles for **bounded memory** — not as an afterthought. This lecture explains exactly what that means in silicon terms, where Gemma 4 sits in the competitive landscape, and how to compute the numbers that determine whether a configuration fits your target.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Describe Gemma 4's **interleaved local/global attention** mechanism and explain why it produces a KV cache that grows O(1) with context length for most layers.
2. Compute the **exact KV footprint** for any Gemma 4 configuration (batch, seq, precision) and compare it to a pure-attention equivalent.
3. State the role of **QK-norm, GeGLU, GQA,** and **256K vocabulary** in Gemma 4 and explain each's impact on edge deployment.
4. Select the right Gemma 4 model size for a given Jetson target using the bandwidth-ceiling formula.
5. Explain Gemma 4's **competitive position** vs Qwen3, Phi-4, and Llama 3.2 at the edge sizes, with specific architectural reasons (not just benchmark numbers).

---

**1. The architecture that makes Gemma 4 an edge model**

Every transformer for inference on a constrained device faces the same problem: the KV cache. At long contexts, the KV cache grows linearly with sequence length and can exceed weight memory — on a Jetson with 64 GB unified memory, a 4B model at BF16 (8 GB weights) can have its KV cache dwarf its weights at 128 K tokens if standard attention is used.

Gemma 4 solves this with **interleaved local and global attention**, a design inherited from Gemma 2 and extended in Gemma 4.

**1.1 Interleaved local/global attention — the key architectural fact**

In a standard transformer, every attention layer computes full-sequence attention: each token attends to every previous token in the context. KV cache grows as `seq_len × num_kv_heads × head_dim × 2 × num_layers × batch × bytes_per_element`.

Gemma 4 replaces most attention layers with **sliding-window local attention**:

```text
Gemma 4 attention pattern (conceptual, 42-layer 12B example):

  Layer 0:  LOCAL   (sliding window, W=1024 tokens)
  Layer 1:  LOCAL
  Layer 2:  LOCAL
  Layer 3:  LOCAL
  Layer 4:  LOCAL
  Layer 5:  LOCAL
  Layer 6:  GLOBAL  ← 1 global per 6 local
  Layer 7:  LOCAL
  Layer 8:  LOCAL
  ...
  Layer 41: LOCAL

  Ratio: 1 GLOBAL per 6 LOCAL = ~6 global + ~36 local in 42 layers
```

**For local (sliding-window) layers:**

- Each token attends only to the most recent W=1024 tokens.
- KV cache per local layer = `batch × W × num_kv_heads × head_dim × 2 × bytes` — **independent of seq_len beyond W**.
- At seq_len = 128 K: local KV is IDENTICAL to local KV at seq_len = 1024.

**For global layers:**

- Standard full-context attention. KV grows with seq_len.
- But there are only ~N_global ≈ N_layers/7 of them.

This is the fundamental insight: **most of the KV cache is O(1) in context length; only the global-attention layers are O(n)**. The global layers are the "cross-document" integration mechanism; the local layers do the cheap sequential processing.

**1.2 Gemma 4 model specifications**

Gemma 4 launches in four sizes. These are architecture approximations derived from Gemma 2 lineage and published model cards (exact configs may differ slightly by variant):

| Model | Params | Layers | Hidden | Q heads | KV heads | Head dim | FFN dim | Context |
|-------|--------|--------|--------|---------|---------|---------|---------|---------|
| Gemma 4 1B | ~1.0B | 18 | 1152 | 4 | 1 | 256 | 6912 | 32K |
| Gemma 4 4B | ~4.3B | 34 | 2560 | 8 | 4 | 256 | 15360 | 128K |
| Gemma 4 12B | ~12.7B | 46 | 3840 | 16 | 8 | 256 | 24576 | 128K |
| Gemma 4 27B | ~27.4B | 62 | 5376 | 32 | 16 | 256 | 36864 | 128K |

All except the 1B support 128 K context. All use the interleaved 1:6 local/global attention pattern in the 4B–27B models.

**Vision variants:** Gemma 4 4B, 12B, and 27B have multimodal (vision) variants built around a **SigLIP-400M** image encoder fused into the decoder. Covered in Lecture 05.

</details>

### 1.3 KV cache 的算术 —— 把数字算一遍

这是每位边缘工程师都必须能心算的算式。以 Gemma 4 4B、128 K 上下文、batch=1、BF16（2 字节）为例来算：

```text
Gemma 4 4B configuration:
  N_layers = 34
  W = 1024 (local attention window)
  n_global ≈ 34 / 7 ≈ 5 global layers
  n_local  ≈ 34 - 5 = 29 local layers
  KV heads = 4, head_dim = 256
  seq_len  = 128,000 tokens
  batch    = 1
  bytes    = 2 (BF16)

KV bytes per layer per token = 2 × KV_heads × head_dim × bytes
                              = 2 × 4 × 256 × 2 = 4,096 bytes/token

LOCAL layers (29 layers, window=1024):
  KV bytes = n_local × 2 × W × KV_heads × head_dim × bytes
           = 29 × 2 × 1024 × 4 × 256 × 2
           = 29 × 4,194,304 bytes
           = 121.6 MB

GLOBAL layers (5 layers, full seq_len=128K):
  KV bytes = n_global × 2 × seq_len × KV_heads × head_dim × bytes
           = 5 × 2 × 128,000 × 4 × 256 × 2
           = 5 × 524,288,000 bytes
           = 2,621 MB ≈ 2.56 GB

TOTAL KV at 128K context:   0.12 + 2.56 = ~2.68 GB
```

再与一个**假设的纯 attention 4B**（相同配置，无 local layer）对比：

```text
Pure-attention 4B at 128K context:
  KV bytes = 34 × 2 × 128,000 × 4 × 256 × 2
           = 34 × 524,288,000
           = 17,825,792,000 bytes ≈ 17.8 GB
```

**Gemma 4 4B 在 128K 下：2.68 GB，而纯 attention 模型为 17.8 GB —— KV cache 小 6.6×。**

在 4K 上下文下（一次典型的 chat 轮次）：

```text
LOCAL  (29 layers, window=1024, capped at 4096): 29 × 2 × 1024 × 4 × 256 × 2 = 122 MB (same!)
GLOBAL (5 layers, seq=4096):                     5 × 2 × 4096 × 4 × 256 × 2  =  84 MB
TOTAL:                                           ~206 MB
```

对 local layer 而言，4K 下的 KV 与 128K 下的 KV 几乎相同 —— local layer 是开销便宜的那部分，却占主导。这就是设计意图：无论上下文多长，local attention 的代价都很低；global attention 的代价只与 global 上下文成正比。

### 1.4 Jetson 目标平台上的带宽上限

使用 [Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md) 中的标准公式：

```text
decode_ceiling(tok/s) = HBM_bandwidth_GBs / bytes_per_token_step

bytes_per_token_step ≈ weight_bytes + KV_bytes_per_step
                     ≈ weight_bytes (at short ctx, batch=1, KV small)
```

```text
Jetson AGX Orin:   204 GB/s bandwidth, 64 GB unified LPDDR5
Jetson AGX Thor:   273 GB/s bandwidth, 128 GB LPDDR5X

Model weights (INT4, 4-bit = 0.5 bytes/param):
  Gemma 4 1B  → 0.5 GB
  Gemma 4 4B  → 2.15 GB
  Gemma 4 12B → 6.35 GB
  Gemma 4 27B → 13.7 GB

Batch-1 decode ceiling (INT4 weights, short context):
  Target     │ 1B            │ 4B            │ 12B           │ 27B
  ───────────┼───────────────┼───────────────┼───────────────┼──────────────
  Orin 204   │ 204/0.5 =408  │ 204/2.15 = 95 │ 204/6.35 = 32 │ doesn't fit*
  Thor 273   │ 273/0.5 =546  │ 273/2.15 =127 │ 273/6.35 = 43 │ 273/13.7 =20

  *27B INT4 (13.7 GB) fits Orin 64 GB; the ceiling is 204/13.7 ≈ 15 tok/s — marginal
```

**如何使用这张表：** 这些是算术上限，不是可达吞吐。由于 KV 访问开销、kernel 启动间隙和 CUDA 同步，实际吞吐通常是上限的 60–80%。用上限 × 0.7 得到现实的估计。

上限告诉你所处的档位：
- **1B 在 Orin 上**：408 tok/s 上限 → 即便只到 60% = 245 tok/s，对任何实时用途都足够快
- **4B 在 Orin 上**：95 tok/s → 约 65 tok/s 可用，适合 chat
- **12B 在 Thor 上**：43 tok/s → 约 30 tok/s，适合自主决策的延迟要求
- **27B 在 Thor 上**：20 tok/s → 约 14 tok/s，可用于离线任务，用于实时则偏紧

---

## 2. 面向边缘工程师的架构组件

### 2.1 分组查询注意力（GQA）—— 为什么重要

Gemma 4 在所有模型规模上都采用 **Q:KV head 比为 2:1 的 GQA**（4B 为 8Q/4KV，12B 为 16Q/8KV，27B 为 32Q/16KV）。相对于多头注意力（MHA），这把 KV cache 减半，且不损失准确率。

```text
KV reduction from GQA:
  MHA (naive):       KV bytes ∝ n_Q_heads × head_dim × seq_len
  MQA (extreme):     KV bytes ∝ 1 × head_dim × seq_len  (one shared KV)
  Gemma 4 GQA 2:1:   KV bytes ∝ n_KV_heads × head_dim × seq_len
                               = (n_Q_heads / 2) × head_dim × seq_len
                               = half of MHA

Combined with local attention (1024 window):
  Total KV savings vs MHA-dense = GQA factor × local-attention factor
                                = 2× × 6.6×  = ~13× smaller KV at 128K context
```

正是这 13× 的缩减，让 128K 上下文的 Gemma 4 不再只是猎奇 —— 它确实可以部署在那些会被纯 attention 模型压垮的 Jetson 硬件上。


<details>
<summary>English original</summary>

**1.3 KV cache math — working through the numbers**

This is the calculation every edge engineer must be able to do in their head. Let's work Gemma 4 4B at 128 K context, batch=1, BF16 (2 bytes):

```text
Gemma 4 4B configuration:
  N_layers = 34
  W = 1024 (local attention window)
  n_global ≈ 34 / 7 ≈ 5 global layers
  n_local  ≈ 34 - 5 = 29 local layers
  KV heads = 4, head_dim = 256
  seq_len  = 128,000 tokens
  batch    = 1
  bytes    = 2 (BF16)

KV bytes per layer per token = 2 × KV_heads × head_dim × bytes
                              = 2 × 4 × 256 × 2 = 4,096 bytes/token

LOCAL layers (29 layers, window=1024):
  KV bytes = n_local × 2 × W × KV_heads × head_dim × bytes
           = 29 × 2 × 1024 × 4 × 256 × 2
           = 29 × 4,194,304 bytes
           = 121.6 MB

GLOBAL layers (5 layers, full seq_len=128K):
  KV bytes = n_global × 2 × seq_len × KV_heads × head_dim × bytes
           = 5 × 2 × 128,000 × 4 × 256 × 2
           = 5 × 524,288,000 bytes
           = 2,621 MB ≈ 2.56 GB

TOTAL KV at 128K context:   0.12 + 2.56 = ~2.68 GB
```

Now compare to a **hypothetical pure-attention 4B** (same config, no local layers):

```text
Pure-attention 4B at 128K context:
  KV bytes = 34 × 2 × 128,000 × 4 × 256 × 2
           = 34 × 524,288,000
           = 17,825,792,000 bytes ≈ 17.8 GB
```

**Gemma 4 4B at 128K: 2.68 GB vs 17.8 GB for a pure-attention model — 6.6× smaller KV cache.**

At 4K context (a typical chat turn):

```text
LOCAL  (29 layers, window=1024, capped at 4096): 29 × 2 × 1024 × 4 × 256 × 2 = 122 MB (same!)
GLOBAL (5 layers, seq=4096):                     5 × 2 × 4096 × 4 × 256 × 2  =  84 MB
TOTAL:                                           ~206 MB
```

KV at 4K is almost identical to KV at 128K for the local layers — the local layers are the cheap ones, and they dominate. This is the design: pay for local attention cheaply regardless of context, pay for global attention only in proportion to the global context.

**1.4 Bandwidth ceiling on Jetson targets**

Use the standard formula from [Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md):

```text
decode_ceiling(tok/s) = HBM_bandwidth_GBs / bytes_per_token_step

bytes_per_token_step ≈ weight_bytes + KV_bytes_per_step
                     ≈ weight_bytes (at short ctx, batch=1, KV small)
```

```text
Jetson AGX Orin:   204 GB/s bandwidth, 64 GB unified LPDDR5
Jetson AGX Thor:   273 GB/s bandwidth, 128 GB LPDDR5X

Model weights (INT4, 4-bit = 0.5 bytes/param):
  Gemma 4 1B  → 0.5 GB
  Gemma 4 4B  → 2.15 GB
  Gemma 4 12B → 6.35 GB
  Gemma 4 27B → 13.7 GB

Batch-1 decode ceiling (INT4 weights, short context):
  Target     │ 1B            │ 4B            │ 12B           │ 27B
  ───────────┼───────────────┼───────────────┼───────────────┼──────────────
  Orin 204   │ 204/0.5 =408  │ 204/2.15 = 95 │ 204/6.35 = 32 │ doesn't fit*
  Thor 273   │ 273/0.5 =546  │ 273/2.15 =127 │ 273/6.35 = 43 │ 273/13.7 =20

  *27B INT4 (13.7 GB) fits Orin 64 GB; the ceiling is 204/13.7 ≈ 15 tok/s — marginal
```

**How to use this table:** These are arithmetic ceilings, not achievable throughput. Actual throughput is typically 60–80% of ceiling due to KV access overhead, kernel launch gaps, and CUDA synchronization. Multiply ceiling × 0.7 for a realistic estimate.

The ceiling tells you the regime:
- **1B at Orin**: 408 tok/s ceiling → even 60% = 245 tok/s, fast enough for any real-time use
- **4B at Orin**: 95 tok/s → ~65 tok/s usable, good for chat
- **12B at Thor**: 43 tok/s → ~30 tok/s, good for autonomous decision latency
- **27B at Thor**: 20 tok/s → ~14 tok/s, usable for offline tasks, tight for real-time

---

**2. Architecture components for edge engineers**

**2.1 Grouped Query Attention (GQA) — why it matters**

Gemma 4 uses **GQA with a 2:1 Q:KV head ratio** across all model sizes (8Q/4KV for 4B, 16Q/8KV for 12B, 32Q/16KV for 27B). This halves the KV cache relative to Multi-Head Attention (MHA) without accuracy loss.

```text
KV reduction from GQA:
  MHA (naive):       KV bytes ∝ n_Q_heads × head_dim × seq_len
  MQA (extreme):     KV bytes ∝ 1 × head_dim × seq_len  (one shared KV)
  Gemma 4 GQA 2:1:   KV bytes ∝ n_KV_heads × head_dim × seq_len
                               = (n_Q_heads / 2) × head_dim × seq_len
                               = half of MHA

Combined with local attention (1024 window):
  Total KV savings vs MHA-dense = GQA factor × local-attention factor
                                = 2× × 6.6×  = ~13× smaller KV at 128K context
```

This 13× reduction is why 128K-context Gemma 4 is not a curiosity — it is actually deployable on Jetson hardware that would choke on a pure-attention model.

</details>

### 2.2 QK-Norm——为什么 Gemma 4 的量化更稳定

Gemma 4 在 attention 点积之前对 **Q 和 K 施加 RMSNorm**：

```python
# Standard attention (no QK-norm):
scores = Q @ K.T / sqrt(head_dim)  # Q, K can be large → scores blow up at long seq

# Gemma 4 QK-norm:
Q_normed = rms_norm(Q, weight=q_scale)   # normalize Q
K_normed = rms_norm(K, weight=k_scale)   # normalize K
scores = Q_normed @ K_normed.T / sqrt(head_dim)  # bounded inputs → stable scores
```

**这对边缘量化意味着什么：**

没有 QK-norm 时，Q 和 K 的激活值在长上下文下幅值可能无界——尤其是在 attention 尚未稳定的早期 layer 中。这使 Q 和 K 投影的 INT8/INT4 量化容易出错：单个离群激活值就能让 per-tensor scale 饱和，并破坏整个分块。

有了 QK-norm：
- Q 和 K 经过 L2 归一化 → 在学习到的 scale 之前，所有值都被限制在 [-1, 1] 内
- per-tensor INT8/INT4 量化效果良好，因为动态范围可预测
- attention logits 同样有界 → 在 128K 序列长度下不会出现 attention logit 溢出
- QK-norm 还能防止困扰长上下文部署的 attention 熵坍缩（即“attention sink”问题）

实践中：**INT4 下的 Gemma 4 比没有 QK-norm 的模型准确率损失更小**，因为归一化驯服了那些会毁掉量化质量的激活值离群点。

### 2.3 GeGLU 激活函数

Gemma 4 在 FFN 中使用**门控 GELU（GeGLU）**：

```python
# Standard FFN:
out = W_down( SiLU(W_gate(x)) * W_up(x) )     # SwiGLU (used in Llama/Qwen)

# Gemma 4 FFN:
out = W_down( GELU(W_gate(x)) * W_up(x) )     # GeGLU (GELU instead of SiLU)
```

`GELU(x) = x × Φ(x)`（高斯 CDF）对比 `SiLU(x) = x × sigmoid(x)`。实践中吞吐差异可忽略不计（两者都是硬件融合的逐元素运算）。建模上的差异只对微调有影响；对推理而言则是透明的。门控（两个公式中的 `*`）把有效 FFN 维度缩减 2×（减半），使 FFN 比看上去略宽（gate 和 up 投影共同构成完整的 FFN 计算）。

### 2.4 256K token 词表

Gemma 4 使用 256K 的 SentencePiece 词表——比典型 LLM 大 4×（LLaMA 用 32K，Qwen3 用 151K）。这影响 **embedding 表**和 **lm_head** 投影：

```text
Embedding table size:
  Gemma 4 4B:  256K × 2560 hidden_dim × 2 bytes = 1.31 GB (BF16)
               256K × 2560 × 0.5 bytes = 0.33 GB (INT4)

  Compare Qwen3-4B (151K vocab):  151K × 2560 × 2 bytes = 0.77 GB BF16

  Gemma 4 embedding overhead: +0.54 GB BF16 vs Qwen3 at 4B class.
  In INT4, the embedding is often kept in INT8 or FP16 (embeddings quantize poorly).
  Budget ~0.66 GB extra for Gemma 4 4B vs Qwen3-4B due to vocabulary.
```

大词表是 Gemma 广泛多语言覆盖（100+ 种语言）与高效 tokenization 的代价（每个概念用更少 token → 相同 wall-clock 时间内生成更快，这在一定程度上抵消了开销）。

---

## 3. 竞争格局

### 3.1 4B 级别：Gemma 4 对比 Qwen3

| 指标 | Gemma 4 4B | Qwen3-4B |
|-----------|-----------|---------|
| 上下文窗口 | 128K | 32K |
| 128K 时的 KV（batch=1） | **~2.7 GB** | ~9.8 GB（纯 attention） |
| 原生多模态 | 是（SigLIP） | 否（独立模型） |
| 量化稳定性 | QK-norm：高 | QK-norm：有（Qwen3 也加入了） |
| Google AI Edge / LiteRT | **是，一等公民** | 否 |
| 许可证 | Gemma ToS（宽松） | Apache-2.0 |
| Jetson 上的最佳 runtime | llama.cpp、LiteRT、MLC-LLM、TRT-LLM | llama.cpp、MLC-LLM |
| 词表 | 256K | 151K |

**如何选择：** 若部署目标是 Jetson 且使用 Google AI Edge 工具链，选 Gemma 4。若需要 Apache-2.0 许可，或更偏好 Qwen 生态且不需要 128K，Qwen3-4B 同样强。若需要在 Jetson 上用 128K，Gemma 4 明显胜出。

### 3.2 3B/4B 级别：Gemma 4 对比 Llama 3.2

Llama 3.2 3B 是 Meta 在 4B 以下的主导选择。关键差异：
- Llama 3.2 3B 使用**标准 attention**（无 local/global 划分）→ KV 线性增长
- Llama 3.2 1B/3B 有 **128K 上下文**，但 128K 时的全 attention KV 约为 1.2 GB（3B），而 Gemma 4 4B 为 2.7 GB——实际上这里 Llama 3.2 在绝对 KV 上占优，因为它更小
- 但**在 12B–27B 规模上**，Gemma 4 交错 attention 的 KV 优势变得决定性
- **视觉**：Llama 3.2 11B Vision 对比 Gemma 4 4B/12B Vision——大致相当；Gemma 4 的视觉编码器路径更轻

**客观的边缘侧定位：** 在 < 4B 的纯文本场景下，Llama 3.2 3B 和 Qwen3-4B 是强有力的替代方案。Gemma 4 在 12B+ 明显胜出，在任何规模上都赢在 128K 上下文，并且在你需要 Google AI Edge 生态时胜出。


<details>
<summary>English original</summary>

**2.2 QK-Norm — why Gemma 4 is more stable to quantize**

Gemma 4 applies **RMSNorm to Q and K** before the attention dot product:

```python
# Standard attention (no QK-norm):
scores = Q @ K.T / sqrt(head_dim)  # Q, K can be large → scores blow up at long seq

# Gemma 4 QK-norm:
Q_normed = rms_norm(Q, weight=q_scale)   # normalize Q
K_normed = rms_norm(K, weight=k_scale)   # normalize K
scores = Q_normed @ K_normed.T / sqrt(head_dim)  # bounded inputs → stable scores
```

**Why this matters for edge quantization:**

Without QK-norm, Q and K activations can have unbounded magnitude at long contexts — especially in early layers where attention hasn't stabilized. This makes INT8/INT4 quantization of Q and K projections error-prone: a single outlier activation can saturate the per-tensor scale and corrupt the entire tile.

With QK-norm:
- Q and K are L2-normalized → all values bounded in [-1, 1] before the learned scale
- Per-tensor INT8/INT4 quantization works well because the dynamic range is predictable
- The attention logits are similarly bounded → no attention-logit overflow at 128K sequence
- QK-norm also prevents attention entropy collapse (the "attention sink" problem) that plagues long-context deployment

In practice: **Gemma 4 at INT4 loses less accuracy than models without QK-norm**, because the normalization tames the activation outliers that kill quantization quality.

**2.3 GeGLU activation**

Gemma 4 uses **Gated GELU (GeGLU)** in the FFN:

```python
# Standard FFN:
out = W_down( SiLU(W_gate(x)) * W_up(x) )     # SwiGLU (used in Llama/Qwen)

# Gemma 4 FFN:
out = W_down( GELU(W_gate(x)) * W_up(x) )     # GeGLU (GELU instead of SiLU)
```

`GELU(x) = x × Φ(x)` (Gaussian CDF) vs `SiLU(x) = x × sigmoid(x)`. In practice the throughput difference is negligible (both are hardware-fused element-wise ops). The modeling difference matters only for fine-tuning; for inference it is transparent. The gating (the `*` in both formulas) halves the effective FFN dimension by 2×, making the FFN slightly wider than it appears (the gate and up projections together give the full FFN computation).

**2.4 The 256K-token vocabulary**

Gemma 4 uses a 256K SentencePiece vocabulary — 4× larger than typical LLMs (LLaMA uses 32K, Qwen3 uses 151K). This affects the **embedding table** and the **lm_head** projection:

```text
Embedding table size:
  Gemma 4 4B:  256K × 2560 hidden_dim × 2 bytes = 1.31 GB (BF16)
               256K × 2560 × 0.5 bytes = 0.33 GB (INT4)

  Compare Qwen3-4B (151K vocab):  151K × 2560 × 2 bytes = 0.77 GB BF16

  Gemma 4 embedding overhead: +0.54 GB BF16 vs Qwen3 at 4B class.
  In INT4, the embedding is often kept in INT8 or FP16 (embeddings quantize poorly).
  Budget ~0.66 GB extra for Gemma 4 4B vs Qwen3-4B due to vocabulary.
```

The large vocabulary is the cost of Gemma's broad multilingual coverage (100+ languages) and efficient tokenization (fewer tokens per concept → faster generation for the same wall-clock time, which partially offsets the overhead).

---

**3. The competitive position**

**3.1 Gemma 4 vs Qwen3 at 4B class**

| Criterion | Gemma 4 4B | Qwen3-4B |
|-----------|-----------|---------|
| Context window | 128K | 32K |
| KV at 128K (batch=1) | **~2.7 GB** | ~9.8 GB (pure attention) |
| Multimodal native | Yes (SigLIP) | No (separate model) |
| Quantization stability | QK-norm: high | QK-norm: Yes (Qwen3 added this too) |
| Google AI Edge / LiteRT | **Yes, first-class** | No |
| License | Gemma ToS (permissive) | Apache-2.0 |
| Best runtime on Jetson | llama.cpp, LiteRT, MLC-LLM, TRT-LLM | llama.cpp, MLC-LLM |
| Vocabulary | 256K | 151K |

**Which to choose:** If your deployment target is Jetson with Google AI Edge toolchain, Gemma 4. If you need Apache-2.0 licensing or prefer the Qwen ecosystem and don't need 128K, Qwen3-4B is equally strong. If you need 128K on Jetson, Gemma 4 wins decisively.

**3.2 Gemma 4 vs Llama 3.2 at 3B/4B class**

Llama 3.2 3B is the dominant sub-4B choice from Meta. Key differences:
- Llama 3.2 3B uses **standard attention** (no local/global split) → KV grows linearly
- Llama 3.2 1B/3B have **128K context** but the full-attention KV at 128K is ~1.2 GB (3B) vs Gemma 4 4B's 2.7 GB — actually Llama 3.2 wins on absolute KV here because it's smaller
- But **at the 12B–27B scale**, the KV advantage of Gemma 4's interleaved attention becomes decisive
- **Vision**: Llama 3.2 11B Vision vs Gemma 4 4B/12B Vision — roughly comparable; Gemma 4 has a lighter vision encoder path

**The honest edge position:** For pure text at < 4B, Llama 3.2 3B and Qwen3-4B are strong alternatives. Gemma 4 wins clearly at 12B+, and wins on 128K context at every size, and wins when you want the Google AI Edge ecosystem.

</details>

### 3.3 Gemma 4 成为显然之选的场景

1. **Jetson 上的 128K 长上下文**：边缘没有任何其他方案具备这样的 KV 效率
2. **Jetson Orin/Thor 上的多模态**：4B 与 12B 视觉模型是 2025 年 Jetson 上最高效的 VLM
3. **Google AI Edge 部署**（通过 LiteRT 部署到 Android/iOS/嵌入式 Linux）：一等公民级支持
4. **物理 AI + 语言**：既需要视觉理解又需要长上下文对话的机器人——Thor 上的 Gemma 4 12B vision
5. **自投机解码**：用 Gemma 4 1B 作为 4B/12B 目标的 draft 很自然（同一 tokenizer、同一架构族）——见第 04 讲

---

## 4. 硬件适配指南

### 4.1 Jetson AGX Orin (204 GB/s, 64 GB, 275 TOPS INT8)

```text
Model         │ Format    │ Weights │ KV @ 4K ctx, b=4 │ Total   │ Fits? │ ~tok/s
──────────────┼───────────┼─────────┼──────────────────┼─────────┼───────┼───────
Gemma 4 1B   │ INT4      │ 0.5 GB  │ 0.05 GB          │ 0.55 GB │ ✓     │ ~245
Gemma 4 4B   │ INT4      │ 2.2 GB  │ 0.2 GB           │ 2.4 GB  │ ✓     │ ~67
Gemma 4 12B  │ INT4      │ 6.4 GB  │ 0.4 GB           │ 6.8 GB  │ ✓     │ ~22
Gemma 4 27B  │ INT4      │ 13.7 GB │ 0.8 GB           │ 14.5 GB │ ✓     │ ~10
Gemma 4 4B   │ BF16      │ 8.6 GB  │ 0.8 GB           │ 9.4 GB  │ ✓     │ ~17
Gemma 4 12B  │ BF16      │ 25.4 GB │ 1.6 GB           │ 27 GB   │ ✓     │ ~6
```

### 4.2 Jetson AGX Thor (273 GB/s, 128 GB, ~1035 FP8 TFLOPS dense)

```text
Model         │ Format    │ Weights │ KV @ 128K ctx, b=1 │ Total   │ Fits? │ ~tok/s
──────────────┼───────────┼─────────┼────────────────────┼─────────┼───────┼───────
Gemma 4 4B   │ INT4      │ 2.2 GB  │ 2.7 GB             │ 4.9 GB  │ ✓     │ ~127
Gemma 4 12B  │ INT4      │ 6.4 GB  │ 5.1 GB             │ 11.5 GB │ ✓     │ ~43
Gemma 4 27B  │ INT4      │ 13.7 GB │ 10.8 GB            │ 24.5 GB │ ✓     │ ~20
Gemma 4 4B   │ FP8       │ 4.3 GB  │ 2.7 GB             │ 7.0 GB  │ ✓     │ ~64
Gemma 4 12B  │ BF16      │ 25.4 GB │ 5.1 GB             │ 30.5 GB │ ✓     │ ~11
```

**Thor 的关键结论：** 27B 模型以 INT4 装入 Thor，容纳 128 K 上下文后仍有富余——对单板边缘设备上这一上下文长度的 27B 模型而言，这是前所未有的。

---

## 5. 边缘用例框架

2025 年有三类用例推动 Gemma 4 在边缘落地：

**物理 AI / 机器人：** 机器人需要语言、视觉和长记忆。Thor 上的 Gemma 4 12B vision 在一个模型里同时给出这三者：VLM 处理关于环境的视觉提问，128K 窗口保存多步任务历史而不截断，43 tok/s 的吞吐足以支撑实时指令理解。

**汽车 / 自动驾驶：** DRIVE Thor（DRIVE 架构，与 Jetson AGX Thor 同一颗芯片）面向集中式计算。Gemma 4 27B INT4 可作为副驾驶，承担自然语言导航、安全报告与驾驶员沟通——用 128K 承载长途行程的上下文。

**移动端 / 嵌入式 Linux 上的边缘 agent：** Orin Nano 8 GB 板上的 Gemma 4 4B INT4 是真正可用的本地 agent——能处理工具调用、遵循指令、维持对话上下文——完全离线，无需云。

---

## 关键结论

- Gemma 4 的 **1:6 交错 local/global attention** 产生的 KV cache 对 6/7 的 layer 是 O(1)，只有 1/7 是 O(n)——在 4B 规模上，128K 上下文的成本比纯 attention 低 6.6×。
- **结合分组查询注意力 GQA（2:1）**，在 128K 上下文下，相对等效的 MHA 稠密模型，KV 总节省可达 ~13×。
- **QK-norm** 在点积之前对 Q 与 K 做归一化，避免长序列下 attention logit 溢出，并使 INT4/INT8 量化比没有该机制的模型更稳定。
- **带宽上限公式**（`tok/s_max = BW_GB/s ÷ bytes_per_weight_byte`）预测 Gemma 4 1B 在 Orin 上以 ~245 tok/s 运行（INT4），4B 为 ~67，12B 为 ~22。乘以 0.7 得到接近实际的吞吐。
- **竞争定位：** Gemma 4 在 Jetson 上的 128K 长上下文以及 12B–27B 多模态边缘档位胜出。在纯 4B 文本上，Qwen3-4B 与 Llama 3.2 3B 有竞争力。在边缘搭配 Google AI Edge / LiteRT 时，Gemma 4 是唯一选择。

---


<details>
<summary>English original</summary>

**3.3 Where Gemma 4 is the obvious choice**

1. **128K long-context on Jetson**: Nothing else has this KV efficiency at the edge
2. **Multimodal on Jetson Orin/Thor**: The 4B and 12B vision models are the most efficient VLMs for Jetson in 2025
3. **Google AI Edge deployment** (Android/iOS/embedded Linux via LiteRT): First-class support
4. **Physical AI + language**: Robot that needs both vision understanding and long-context dialogue — Gemma 4 12B vision on a Thor
5. **Self-speculative decoding**: Using Gemma 4 1B as draft for 4B/12B targets is natural (same tokenizer, same architecture family) — covered in Lecture 04

---

**4. Hardware fit guide**

**4.1 Jetson AGX Orin (204 GB/s, 64 GB, 275 TOPS INT8)**

```text
Model         │ Format    │ Weights │ KV @ 4K ctx, b=4 │ Total   │ Fits? │ ~tok/s
──────────────┼───────────┼─────────┼──────────────────┼─────────┼───────┼───────
Gemma 4 1B   │ INT4      │ 0.5 GB  │ 0.05 GB          │ 0.55 GB │ ✓     │ ~245
Gemma 4 4B   │ INT4      │ 2.2 GB  │ 0.2 GB           │ 2.4 GB  │ ✓     │ ~67
Gemma 4 12B  │ INT4      │ 6.4 GB  │ 0.4 GB           │ 6.8 GB  │ ✓     │ ~22
Gemma 4 27B  │ INT4      │ 13.7 GB │ 0.8 GB           │ 14.5 GB │ ✓     │ ~10
Gemma 4 4B   │ BF16      │ 8.6 GB  │ 0.8 GB           │ 9.4 GB  │ ✓     │ ~17
Gemma 4 12B  │ BF16      │ 25.4 GB │ 1.6 GB           │ 27 GB   │ ✓     │ ~6
```

**4.2 Jetson AGX Thor (273 GB/s, 128 GB, ~1035 FP8 TFLOPS dense)**

```text
Model         │ Format    │ Weights │ KV @ 128K ctx, b=1 │ Total   │ Fits? │ ~tok/s
──────────────┼───────────┼─────────┼────────────────────┼─────────┼───────┼───────
Gemma 4 4B   │ INT4      │ 2.2 GB  │ 2.7 GB             │ 4.9 GB  │ ✓     │ ~127
Gemma 4 12B  │ INT4      │ 6.4 GB  │ 5.1 GB             │ 11.5 GB │ ✓     │ ~43
Gemma 4 27B  │ INT4      │ 13.7 GB │ 10.8 GB            │ 24.5 GB │ ✓     │ ~20
Gemma 4 4B   │ FP8       │ 4.3 GB  │ 2.7 GB             │ 7.0 GB  │ ✓     │ ~64
Gemma 4 12B  │ BF16      │ 25.4 GB │ 5.1 GB             │ 30.5 GB │ ✓     │ ~11
```

**Key takeaway for Thor:** The 27B model fits in Thor at INT4 with 128 K context and room to spare — this is unprecedented for a 27B model at that context length on a single-board edge device.

---

**5. The edge use-case framing**

Three use-cases drive Gemma 4 at the edge in 2025:

**Physical AI / robotics:** Robots need language AND vision AND long-memory. Gemma 4 12B vision on Thor gives all three in one model: the VLM handles visual questions about the environment, the 128K window holds multi-step task history without truncating, and the 43 tok/s throughput is fast enough for real-time command understanding.

**Automotive / autonomous vehicles:** DRIVE Thor (DRIVE architecture, same silicon as Jetson AGX Thor) targets centralized compute. Gemma 4 27B INT4 as a co-driver for natural language navigation, safety reporting, and driver communication — with 128K for long-trip context.

**Edge agents on mobile / embedded Linux:** Gemma 4 4B INT4 on an Orin Nano 8 GB board is a genuinely capable local agent — handles tool calls, follows instructions, and maintains conversation context — entirely offline, no cloud required.

---

**Key takeaways**

- Gemma 4's **interleaved 1:6 local/global attention** produces a KV cache that is O(1) for 6/7 of layers and O(n) for only 1/7 — making 128K context 6.6× cheaper than pure attention at the 4B scale.
- **Combined with GQA (2:1)**, total KV savings vs a MHA-dense equivalent reach ~13× at 128K context.
- **QK-norm** normalizes Q and K before the dot product, preventing attention-logit overflow at long sequences and making INT4/INT8 quantization more stable than in models without it.
- The **bandwidth-ceiling formula** (`tok/s_max = BW_GB/s ÷ bytes_per_weight_byte`) predicts that Gemma 4 1B runs at ~245 tok/s on Orin (INT4), 4B at ~67, 12B at ~22. Multiply by 0.7 for realistic throughput.
- **Competitive position:** Gemma 4 wins at 128K long-context on Jetson and at the 12B–27B multimodal edge tier. At pure 4B text, Qwen3-4B and Llama 3.2 3B are competitive. At the edge with Google AI Edge / LiteRT, Gemma 4 is the only choice.

---

</details>

## 参考文献

- Google DeepMind Gemma 4 模型卡与技术报告（2025 年 4 月）— [ai.google.dev/gemma/docs/gemma4](https://ai.google.dev/gemma/docs/gemma4)
- NVIDIA Jetson AGX Thor 产品页（273 GB/s、128 GB LPDDR5X）— [developer.nvidia.com/embedded/jetson-agx-thor](https://developer.nvidia.com/embedded/jetson-agx-thor)
- NVIDIA Jetson AGX Orin 开发套件 — [developer.nvidia.com/embedded/jetson-agx-orin-developer-kit](https://developer.nvidia.com/embedded/jetson-agx-orin-developer-kit)
- Gemma 2 Technical Report（Google DeepMind，2024）— Gemma 4 架构的基础 — arXiv:2408.00118
- "GQA: Training Generalized Multi-Query Transformer Models from Multi-Head Checkpoints"（Ainslie 等，2023）— arXiv:2305.13245
- *MLSys Deep Dives — Lecture 01 — 带宽上限方程* — [Lecture-01.md](../../07-方向G-ML系统工程/05-MLSys深度探索/Lecture-01.md)
- *Edge LLM Inference Internals — GEMV decode 与内存墙* — [Lecture-01.md](../03-边缘LLM推理内部机制/Lecture-01.md)

---

## 内容截至 2026-06

Gemma 4 模型家族：1B/4B/12B/27B，2025 年 4 月发布，4B–27B 为 128K 上下文，交错的 1:6 局部/全局 attention，W=1024 滑动窗口，GQA 2:1，QK-norm，GeGLU，256K 词表。Jetson AGX Thor 规格：128 GB LPDDR5X @ 273 GB/s，75–130 W，开发套件 $3,499（2025 年 11 月）。

---

*上一级：[Gemma 4 Edge Deployment](README.md) · 下一节：[Lecture 02 — 量化与格式转换](Lecture-02.md)*


<details>
<summary>English original</summary>

**References**

- Google DeepMind Gemma 4 model card and technical report (April 2025) — [ai.google.dev/gemma/docs/gemma4](https://ai.google.dev/gemma/docs/gemma4)
- NVIDIA Jetson AGX Thor product page (273 GB/s, 128 GB LPDDR5X) — [developer.nvidia.com/embedded/jetson-agx-thor](https://developer.nvidia.com/embedded/jetson-agx-thor)
- NVIDIA Jetson AGX Orin developer kit — [developer.nvidia.com/embedded/jetson-agx-orin-developer-kit](https://developer.nvidia.com/embedded/jetson-agx-orin-developer-kit)
- Gemma 2 Technical Report (Google DeepMind, 2024) — basis for Gemma 4 architecture — arXiv:2408.00118
- "GQA: Training Generalized Multi-Query Transformer Models from Multi-Head Checkpoints" (Ainslie et al., 2023) — arXiv:2305.13245
- *MLSys Deep Dives — Lecture 01 — the bandwidth-ceiling equation* — [Lecture-01.md](../../07-方向G-ML系统工程/05-MLSys深度探索/Lecture-01.md)
- *Edge LLM Inference Internals — GEMV decode and memory wall* — [Lecture-01.md](../03-边缘LLM推理内部机制/Lecture-01.md)

---

**Current as of 2026-06**

Gemma 4 model family: 1B/4B/12B/27B, released April 2025, 128K context for 4B–27B, interleaved 1:6 local/global attention, W=1024 sliding window, GQA 2:1, QK-norm, GeGLU, 256K vocabulary. Jetson AGX Thor spec: 128 GB LPDDR5X @ 273 GB/s, 75–130 W, dev kit $3,499 (Nov 2025).

---

*Up: [Gemma 4 Edge Deployment](README.md) · Next: [Lecture 02 — Quantization and Format Conversion](Lecture-02.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Gemma 4 Edge Deployment/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Gemma%204%20Edge%20Deployment/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
