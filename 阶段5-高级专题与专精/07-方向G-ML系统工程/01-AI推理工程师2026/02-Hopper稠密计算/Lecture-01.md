---
title: 第 2 部分 · 第 01 讲 — 70B 级稠密模型剖析：Llama 3.3 70B vs Qwen 2.5 72B
description: 第 2 部分 · 第 01 讲 — 70B 级稠密模型剖析：Llama 3.3 70B vs Qwen 2.5 72B
---

# 第 2 部分 · 第 01 讲 — 70B 级稠密模型剖析：Llama 3.3 70B vs Qwen 2.5 72B

## Overview

两个模型，同一个架构家族，两种生产部署——本讲将并列对比从模型卡级别的概览，深入到推理图的张量形状以及各阶段的具体成本数字。

Llama 3.3 70B（Meta，2024-12）和 Qwen 2.5 72B（Alibaba，2024-09）都是**稠密 decoder-only Transformer**，它们共享：

* 80 个 Transformer layer
* 分组查询注意力（GQA），64 个 query head 和 8 个 KV head（head_dim 128）
* 旋转位置编码（RoPE）、RMSNorm、SwiGLU FFN
* 128K 上下文窗口（通过不同路线达成：Llama 3.3 使用 Meta 的 `llama3` RoPE 频率缩放，Qwen 2.5 使用 YaRN）

事实上，它们在**维度上几乎相同**——相同的 hidden size（8192）、相同的 80 个 layer、相同的 GQA 几何。它们在三个较小的方面不同，而这些方面对推理工程很重要：

1. **词表 / tokenizer** — Llama 3.3 使用源自 tiktoken 的 BPE，词表约 128K；Qwen 2.5 使用自己的 152K BPE，针对多语言（尤其是中文）内容优化。更大的 embed + LM-head 矩阵为 70B 与 72B 的差距增加约 0.4B 参数（大部分差距来自约宽 3% 的 FFN，约 1.8B——见 §5），并且在中文上 tokenization 效率相差约 20–30%。
2. **QKV 偏置** — Qwen 在 Q、K、V 投影上保留 bias 项；Llama 无 bias。内存占用极小，对长上下文外推行为影响适中。
3. **FFN 宽度** — Qwen 的中间尺寸*略*大（29568 vs Llama 的 28672，约 3%）。真实但次要——不是你在二手资料中会看到的“Qwen 宽 50%”（这些资料错误引用 Qwen 为 12288 hidden / 49152 FFN；§2.1 说明为何这无法通过粗略估算检查）。

本讲涵盖：

1. 共享架构——每个现代稠密大语言模型在 2025–2026 年都会交付的内容。
2. 四个差异及其推理成本影响。
3. 每个 token 的 KV cache 成本，基于两者的 `config.json` 推导。
4. 每个 layer 的张量形状——确切地哪些内容会被矩阵乘。
5. 参数核算——70B 和 72B 标签从何而来，以及它们隐藏了什么。
6. 实际影响——runtime 选择（vLLM / SGLang / TRT-LLM）、量化，以及部署形态在两者之间如何变化。

到本文结束时，你应该能够阅读任一模型的 `config.json`，并用具体数字预测每个前向传播步骤在 HBM 字节和 FLOPs 上的成本。

---

> 🧠 **在 3D 中查看。** 本讲的架构按比例渲染在 [**LLM Inference Visualizer**](https://github.com/ai-hpc/llm-inference-viz) —— 在 Qwen 2.5 72B 和 Llama 3.3 70B 之间切换，悬停每个阶段（embedding → RMSNorm → GQA → RoPE → softmax → SwiGLU），并观察 roofline（性能上界模型）标记每个 op 是内存受限还是算力受限。

---

## 1. 共享架构

两个模型都实现了相同的 2024+ 规范 decoder-only 设计：

```text
input tokens (vocab → embeddings)
       │
       ▼
   ┌─────────────────────────────────────────────────┐
   │ for layer in 1..80:                             │
   │   ┌──── attention block ──────────────────────┐ │
   │   │  RMSNorm                                  │ │
   │   │  Q, K, V projections (GQA: 64Q, 8KV)      │ │
   │   │  RoPE on Q and K                          │ │
   │   │  scaled-dot-product attention (masked)    │ │
   │   │  output projection                        │ │
   │   │  residual add                             │ │
   │   └───────────────────────────────────────────┘ │
   │   ┌──── feed-forward block ───────────────────┐ │
   │   │  RMSNorm                                  │ │
   │   │  gate projection, up projection           │ │
   │   │  SwiGLU (silu(gate) * up)                 │ │
   │   │  down projection                          │ │
   │   │  residual add                             │ │
   │   └───────────────────────────────────────────┘ │
   └─────────────────────────────────────────────────┘
       │
       ▼
   RMSNorm + LM head (output projection to vocab)
       │
       ▼
   logits → sampler → next token
```

两者：

* 使用 **分组查询注意力（GQA）**（8 个 KV head 在 64 个 query head 间共享 → 分组大小为 8）。
* 使用 **旋转位置编码（RoPE）** 进行位置编码，并通过不同路线扩展到 128K——Llama 3.3 通过 Meta 的 `"rope_type": "llama3"` 频率缩放以及长上下文继续预训练；Qwen 2.5 通过在 32K 原生窗口上于推理时应用 YaRN。
* 使用 **RMSNorm**，epsilon ≈ 1e-6 / 1e-5。
* 在 FFN 中使用 **SwiGLU**（gate、up、down 投影）。
* 通过 RoPE 具有双向位置性，并为 decode（逐 token 生成阶段）使用 **因果掩码**。
* LM head 与输入 embedding 绑定或不绑定——Llama 3.3 70B **不**绑定（单独的输出矩阵），Qwen 2.5 72B 也**不**绑定。（4B 级 Qwen3 *确实*绑定，但 72B 不绑定。）

这就是**如今每个 7B–72B 稠密大语言模型都会交付的**架构。掌握它 = 可移植。

---

## 1.1 RMSNorm — 它实际做什么

每个 block 应用两次（attention 前、多层感知机前），因此在 80 个 layer 中共 160 次。值得彻底理解，而不只是叫出名字。


<details>
<summary>English original</summary>

**Part 2 · Lecture 01 — Anatomy of a 70B-Class Dense Model: Llama 3.3 70B vs Qwen 2.5 72B**

**Overview**

Two models, one architecture family, two production deployments — this lecture takes the side-by-side comparison from a model-card-level summary down to inference-graph tensor shapes and concrete cost numbers per stage.

Both Llama 3.3 70B (Meta, 2024-12) and Qwen 2.5 72B (Alibaba, 2024-09) are **dense decoder-only transformers** that share:

* 80 transformer layers
* GQA with 64 query heads and 8 KV heads (head_dim 128)
* RoPE positional encoding, RMSNorm, SwiGLU FFN
* 128K context window (reached by different routes: Meta's `llama3` RoPE frequency scaling for Llama 3.3, YaRN for Qwen 2.5)

They are, in fact, **dimensionally almost identical** — same hidden size (8192), same 80 layers, same GQA geometry. They differ in three smaller places that matter for inference engineering:

1. **Vocabulary / tokenizer** — Llama 3.3 uses a tiktoken-derived BPE at ~128K vocab; Qwen 2.5 uses its own 152K BPE optimized for multilingual (especially Chinese) content. The bigger embed + LM-head matrices add ≈0.4B params to the 70B-vs-72B gap (most of the gap is the ~3% wider FFN, ≈1.8B — see §5), and tokenization efficiency differs by ~20–30% on Chinese.
2. **QKV bias** — Qwen keeps the bias terms on Q, K, V projections; Llama is bias-free. Tiny memory footprint, modest impact on long-context extrapolation behavior.
3. **FFN width** — Qwen's intermediate size is *slightly* larger (29568 vs Llama's 28672, ~3%). Real, but minor — not the "Qwen is 50% wider" you'll see in secondary sources (which misquote Qwen as 12288 hidden / 49152 FFN; §2.1 shows why that fails a back-of-envelope check).

This lecture covers:

1. The shared architecture — what every modern dense LLM ships in 2025–2026.
2. The four differences and their inference-cost impact.
3. KV cache cost per token, derived from `config.json` for both.
4. Tensor shapes per layer — exactly what gets matmul'd.
5. Parameter accounting — where the 70B and 72B labels come from and what they hide.
6. Practical impact — how runtime picks (vLLM / SGLang / TRT-LLM), quantization, and deployment shape change between the two.

By the end you should be able to read the `config.json` for either model and predict, in concrete numbers, what each forward pass step costs in HBM bytes and FLOPs.

---

> 🧠 **See it in 3D.** This lecture's architecture is rendered to scale in the [**LLM Inference Visualizer**](https://github.com/ai-hpc/llm-inference-viz) — switch between Qwen 2.5 72B and Llama 3.3 70B, hover each stage (embedding → RMSNorm → GQA → RoPE → softmax → SwiGLU), and watch the roofline mark every op memory- or compute-bound.

---

**1. The shared architecture**

Both models implement the same canonical 2024+ decoder-only design:

```text
input tokens (vocab → embeddings)
       │
       ▼
   ┌─────────────────────────────────────────────────┐
   │ for layer in 1..80:                             │
   │   ┌──── attention block ──────────────────────┐ │
   │   │  RMSNorm                                  │ │
   │   │  Q, K, V projections (GQA: 64Q, 8KV)      │ │
   │   │  RoPE on Q and K                          │ │
   │   │  scaled-dot-product attention (masked)    │ │
   │   │  output projection                        │ │
   │   │  residual add                             │ │
   │   └───────────────────────────────────────────┘ │
   │   ┌──── feed-forward block ───────────────────┐ │
   │   │  RMSNorm                                  │ │
   │   │  gate projection, up projection           │ │
   │   │  SwiGLU (silu(gate) * up)                 │ │
   │   │  down projection                          │ │
   │   │  residual add                             │ │
   │   └───────────────────────────────────────────┘ │
   └─────────────────────────────────────────────────┘
       │
       ▼
   RMSNorm + LM head (output projection to vocab)
       │
       ▼
   logits → sampler → next token
```

Both:

* Use **GQA** (8 KV heads shared across 64 query heads → group size 8).
* Use **RoPE** for position encoding, extended to 128K by different routes — Llama 3.3 via Meta's `"rope_type": "llama3"` frequency scaling plus long-context continued pretraining; Qwen 2.5 via YaRN applied at inference over a 32K native window.
* Use **RMSNorm** with epsilon ≈ 1e-6 / 1e-5.
* Use **SwiGLU** in the FFN (gate, up, down projections).
* Are bidirectional-positional via RoPE, **causally masked** for decoding.
* Tie or do not tie the LM head with input embeddings — Llama 3.3 70B does **not** tie (separate output matrix), Qwen 2.5 72B does **not** tie either. (The 4B-class Qwen3 *does* tie, but 72B does not.)

This is the architecture **every 7B–72B dense LLM ships today**. Mastering it = portable.

---

**1.1 RMSNorm — what it actually does**

Applied twice per block (pre-attention, pre-MLP), so 160 times across 80 layers. Worth understanding cold, not just naming.

</details>

### 它解决的问题

神经网络在逐层传递时，产生的激活值尺度会发生漂移：

```text
Layer 1 output: small values (~0.1)
Layer 40 output: large values (~50)
Layer 80 output: exploding or vanishing
```

不做归一化，**梯度会爆炸或消失**，深层堆叠训练失败。

### 公式

```text
RMSNorm(x) = γ ⊙ (x / RMS(x))

where RMS(x) = sqrt( (1/d) · Σ xᵢ² )
```

### 直觉：RMS = “典型幅值”

取 `x = [2, -2, 1, -1]`：

```text
Step 1 — square (removes sign): [4, 4, 1, 1]
Step 2 — average:               (4+4+1+1)/4 = 2.5
Step 3 — square root:           √2.5 ≈ 1.58
```

为什么平方？因为直接求平均时正负值会相互抵消：

```text
[+5, -5] → average = 0  ❌  (looks empty; the signal is actually strong)
[+5, -5] → RMS     = 5  ✔  (captures the true energy)
```

RMS 度量的是**信号能量**，而非中心。

### 两步操作

**步骤 A —— 度量尺度：**除以 RMS → 向量此后具有稳定的单位幅值。

**步骤 B —— 重新缩放（可学习）：**乘以学习到的权重 `γ` → 由模型决定每层该有多“响”。

```text
Before:  x = [3, -4]    → RMS = √((9+16)/2) ≈ 3.54  (magnitude uncontrolled)
After:   x / RMS ≈ [0.85, -1.13]                     (stable ~unit magnitude)
With γ:  γ ⊙ [0.85, -1.13]                           (model-learned scale)
```

> **类比：**把每个 token 向量看作音频信号。RMS = 音量表。RMSNorm = 自动增益控制。无论该层多“响”，它都把信号维持在稳定、可学习的响度上。

### 为什么不用 LayerNorm？

LayerNorm 还会减去均值（对分布做中心化）。实践中，对 Transformer 的激活值而言，中心化只增加计算量，收益并不稳定 —— 尺度远比中心重要。RMSNorm 去掉了中心化：

```text
LayerNorm:  center (subtract mean) + scale
RMSNorm:    scale only
```

结果：对深层 decoder，**更简单、约快 10%、质量基本一致**。所有现代稠密 LLM（Llama、Qwen、Mistral、Gemma）都因此采用 RMSNorm。

### 这对推理工程意味着什么

* **开销：**逐元素运算，在 decode（逐 token 生成阶段）时受带宽限制（读一次激活值，写一次）。FLOPs 可忽略 —— 约 0.5 FLOP/B，远低于 roofline（性能上界模型）的拐点。每 block 两次调用 × 80 blocks = 每 token 160 次调用，但每次都很便宜。
* **ε (epsilon)：**Qwen 2.5 72B 为 `1e-6`，Llama 3.3 70B 为 `1e-5`。加到分母上，防止激活值接近零时除零。实践中对推理无影响。
* **γ 权重：**每次 RMSNorm 调用 1 个含 `d_model` 个 float 的向量 × 每 block 2 次 × 80 blocks = 160 × 8192 ≈ 1.3M 参数 —— 相对 72B 只是极小一部分，但 decode 时每个都必须从 HBM 加载。

---

## 2. 四个差异

### 2.1 宽度 —— 几乎相同（以及一个警示故事）

| 模型 | hidden (d) | intermediate (d_ff) | d_ff / d |
|-------|-----------|---------------------|----------|
| Llama 3.3 70B | 8192 | 28672 | 3.5 |
| Qwen 2.5 72B | 8192 | 29568 | 3.6 |

hidden 维度相同；Qwen 的 FFN 只宽约 3%。因此逐层 decode 开销（batch=1）几乎相等：

| 开销 | Llama 3.3 70B | Qwen 2.5 72B | 比值 |
|------|----------------|----------------|-------|
| FFN gate+up+down 的 HBM 读取 | 3 × d × d_ff × bytes ≈ 3 × 8192 × 28672 × 2 ≈ 1.41 GB FP16 | 3 × 8192 × 29568 × 2 ≈ 1.45 GB FP16 | Qwen 1.03× |
| FFN 矩阵乘 FLOPs（每 token） | 6 × d × d_ff ≈ 1.41 GFLOP | 6 × 8192 × 29568 ≈ 1.45 GFLOP | Qwen 1.03× |
| Attention QKVO 投影 HBM | ≈ 302 MB | ≈ 302 MB | 1.0× |

> ⚠️ **常见误引。**许多二手资料把 Qwen 2.5 72B 列为 **12288 hidden / 49152 FFN**，暗示它比 Llama “宽 50%”。这是错的 —— 12288 是 GPT-3 的宽度，不是 Qwen 的。最快识破它的方法：12288 hidden 配 49152 FFN、共 80 层，参数量会达到 **约 160B+**，而不是 72B。当配置与机器标注的参数量对不上时，不要相信该配置。§4 直接从官方 `config.json` 推导出真实形状。

### 2.2 Attention head 几何 —— 完全相同

| 模型 | num_q_heads | num_kv_heads | head_dim |
|-------|------------|--------------|----------|
| Llama 3.3 70B | 64 | 8 | 128 |
| Qwen 2.5 72B | 64 | 8 | 128 |

完全相同。这意味着：

* 两个模型的**每 token KV cache 完全相同**（FP16 下 320 KB/token，见第 1 部分第 02 讲）。
* **每 token 的 attention 计算完全相同** —— attention 矩阵乘取决于 (h_q, head_dim) 形状的 Q × K^T，两者一致。

对长上下文工作负载，两个模型的 **KV 显存压力相同**。差异在于 **FFN 开销**。


<details>
<summary>English original</summary>

**The problem it solves**

Neural networks produce activations that drift in scale as they pass through layers:

```text
Layer 1 output: small values (~0.1)
Layer 40 output: large values (~50)
Layer 80 output: exploding or vanishing
```

Without normalization, **gradients explode or vanish** and training fails for deep stacks.

**The formula**

```text
RMSNorm(x) = γ ⊙ (x / RMS(x))

where RMS(x) = sqrt( (1/d) · Σ xᵢ² )
```

**Intuition: RMS = "typical magnitude"**

Take `x = [2, -2, 1, -1]`:

```text
Step 1 — square (removes sign): [4, 4, 1, 1]
Step 2 — average:               (4+4+1+1)/4 = 2.5
Step 3 — square root:           √2.5 ≈ 1.58
```

Why square? Because positive and negative values cancel if you average directly:

```text
[+5, -5] → average = 0  ❌  (looks empty; the signal is actually strong)
[+5, -5] → RMS     = 5  ✔  (captures the true energy)
```

RMS measures **signal energy**, not center.

**The two-step operation**

**Step A — measure scale:** divide by RMS → vector now has stable unit magnitude.

**Step B — re-scale (learned):** multiply by learned weight `γ` → the model decides how loud each layer should be.

```text
Before:  x = [3, -4]    → RMS = √((9+16)/2) ≈ 3.54  (magnitude uncontrolled)
After:   x / RMS ≈ [0.85, -1.13]                     (stable ~unit magnitude)
With γ:  γ ⊙ [0.85, -1.13]                           (model-learned scale)
```

> **Analogy:** think of each token vector as an audio signal. RMS = volume meter. RMSNorm = automatic gain control. No matter how loud the layer gets, it keeps the signal at a stable, learnable loudness.

**Why not LayerNorm?**

LayerNorm also subtracts the mean (centers the distribution). In practice, centering adds computation without consistent benefit for transformer activations — the scale matters far more than the center. RMSNorm drops the centering:

```text
LayerNorm:  center (subtract mean) + scale
RMSNorm:    scale only
```

Result: **simpler, ~10% faster, essentially the same quality** for deep decoders. All modern dense LLMs (Llama, Qwen, Mistral, Gemma) use RMSNorm for this reason.

**What this means for inference engineering**

* **Cost:** elementwise, bandwidth-bound at decode (reads activations once, writes once). Negligible FLOPs — ~0.5 FLOP/B, well below the roofline ridge. Two calls per block × 80 blocks = 160 calls per token, but each is cheap.
* **ε (epsilon):** `1e-6` for Qwen 2.5 72B, `1e-5` for Llama 3.3 70B. Adds to the denominator to prevent division by zero when activations are near-zero. Inference-irrelevant in practice.
* **γ weights:** 1 vector of `d_model` floats per RMSNorm call × 2 per block × 80 blocks = 160 × 8192 ≈ 1.3M params — tiny fraction of 72B, but each must be loaded from HBM at decode.

---

**2. The four differences**

**2.1 Width — nearly identical (and a cautionary tale)**

| Model | hidden (d) | intermediate (d_ff) | d_ff / d |
|-------|-----------|---------------------|----------|
| Llama 3.3 70B | 8192 | 28672 | 3.5 |
| Qwen 2.5 72B | 8192 | 29568 | 3.6 |

Same hidden dimension; Qwen's FFN is only ~3% wider. Per-layer decode costs (batch=1) are therefore nearly equal:

| Cost | Llama 3.3 70B | Qwen 2.5 72B | Ratio |
|------|----------------|----------------|-------|
| FFN gate+up+down HBM read | 3 × d × d_ff × bytes ≈ 3 × 8192 × 28672 × 2 ≈ 1.41 GB FP16 | 3 × 8192 × 29568 × 2 ≈ 1.45 GB FP16 | Qwen 1.03× |
| FFN matmul FLOPs (per token) | 6 × d × d_ff ≈ 1.41 GFLOP | 6 × 8192 × 29568 ≈ 1.45 GFLOP | Qwen 1.03× |
| Attention QKVO proj HBM | ≈ 302 MB | ≈ 302 MB | 1.0× |

> ⚠️ **Common misquote.** Many secondary sources list Qwen 2.5 72B as **12288 hidden / 49152 FFN**, implying it is "50% wider" than Llama. That is wrong — 12288 is GPT-3's width, not Qwen's. The fastest way to catch it: 12288 hidden with a 49152 FFN across 80 layers would weigh in at **~160B+ parameters**, not 72B. When a config doesn't reconcile with the parameter count on the box, distrust the config. §4 derives the real shapes straight from the official `config.json`.

**2.2 Attention head geometry — identical**

| Model | num_q_heads | num_kv_heads | head_dim |
|-------|------------|--------------|----------|
| Llama 3.3 70B | 64 | 8 | 128 |
| Qwen 2.5 72B | 64 | 8 | 128 |

Identical. This means:

* **KV cache per token is identical** between the two models (320 KB/token at FP16, per Part 1 Lecture 02).
* **Attention computation per token is identical** — the attention matmul depends on Q × K^T at (h_q, head_dim) shape, same for both.

For a long-context workload, the two models have the **same KV memory pressure**. The differentiator is the **FFN cost**.

</details>

### 2.3 QKV 偏置

| 模型 | Q 偏置 | K 偏置 | V 偏置 |
|-------|--------|--------|--------|
| Llama 3.3 70B | 无 | 无 | 无 |
| Qwen 2.5 72B | 有 | 有 | 有 |

内存开销：8192 + 1024 + 1024 = 10,240 个 float × 80 层 ≈ 819K float ≈ FP16 下 1.6 MB。可忽略。

推理开销：每个矩阵乘多一次加法。在现代硬件上可忽略。

**为什么重要？** 这是 Qwen 做的一项很小的架构承诺，因为该团队观察到 QKV 投影中的偏置项有助于 long-context 的外推行为。开销基本为零，所以就这么带上了。对推理工程师而言，它只是一个一行的配置开关（HF transformers 中的 `use_bias` 或 `attention_bias`）。

### 2.4 词表与 tokenizer

| 模型 | 词表大小 | Tokenizer 基座 |
|-------|-----------|----------------|
| Llama 3.3 70B | 128,256 | 源自 tiktoken 的 BPE |
| Qwen 2.5 72B | 152,064 | Qwen BPE（多语言 + 代码优化） |

对推理有影响的差异：

* **Embedding 矩阵大小：** Llama 128256 × 8192 ≈ 1.05B 参数；Qwen 152064 × 8192 ≈ 1.25B 参数（embed 与 LM head 各约 2.5 GB FP16，不共享权重）。Qwen 更大的词表为 72B 与 70B 的参数差贡献了约 0.4B——这是实打实的，但大头来自宽了约 3% 的 FFN（≈1.8B；见 §5）。
* **LM head 矩阵：** 与 embedding 尺寸相同（不共享权重）。
* **token 化效率**——对给定文本：
  * 英文：Llama 的 tokenizer 比 Qwen 的高效约 5%。
  * 中文：Qwen 的 tokenizer 高效约 25–30%。
  * 代码：Qwen 的高效约 10%。
* **对部署延迟而言：** 同样的中文输入在 Qwen 下产生更少 token → 更少 decode 步数（逐 token 生成阶段）→ 相同回复内容下 wall-clock 更低。这在中文产品中是实打实的收益。

---

## 3. 每 token 的 KV cache，由配置推导

资深工程师会在白板上推导这个。两个模型：

```text
kv_bytes_per_token = 2 × L × num_kv_heads × head_dim × bytes
                   = 2 × 80 × 8 × 128 × 2 (FP16)
                   = 327,680 bytes
                   ≈ 320 KB / token
```

| 上下文 | KV cache @ FP16 | @ FP8 | @ INT4 |
|---------|------------------|-------|--------|
| 4,096 | 1.3 GB | 0.65 GB | 0.33 GB |
| 32,768 | 10.5 GB | 5.25 GB | 2.6 GB |
| 131,072 | 42 GB | 21 GB | 10.5 GB |

**每请求。** 两个模型此项相同。**long-context 推理服务会迫使你选择 FP8 KV**，无论你从这一对模型里选哪一个。

---

## 4. 每层张量形状——究竟哪些会被矩阵乘

对于一次前向传播（单 token，decode batch=1）：

### 4.1 Llama 3.3 70B

| 张量 | 形状 | FP16 大小 |
|--------|-------|-----------|
| `attn_q.weight` | (8192, 8192) | 134 MB |
| `attn_k.weight` | (8192, 1024) | 17 MB |
| `attn_v.weight` | (8192, 1024) | 17 MB |
| `attn_o.weight` | (8192, 8192) | 134 MB |
| `ffn_gate.weight` | (8192, 28672) | 470 MB |
| `ffn_up.weight` | (8192, 28672) | 470 MB |
| `ffn_down.weight` | (28672, 8192) | 470 MB |
| 每层合计 | — | **~1.7 GB** |
| × 80 层 | — | **~136 GB FP16** |
| + embed + LM head | 128256 × 8192 × 2 × 2 | **+ 4 GB** |
| **合计** | | **~140 GB FP16** |


<details>
<summary>English original</summary>

**2.3 QKV bias**

| Model | Q bias | K bias | V bias |
|-------|--------|--------|--------|
| Llama 3.3 70B | absent | absent | absent |
| Qwen 2.5 72B | present | present | present |

Memory cost: 8192 + 1024 + 1024 = 10,240 floats × 80 layers ≈ 819K floats ≈ 1.6 MB at FP16. Negligible.

Inference cost: one extra add per matmul. Negligible on modern hardware.

**Why does it matter?** It is a small architectural commitment Qwen made because the team observed that bias terms in QKV projections help long-context extrapolation behavior. The cost is essentially zero, so it ships. For an inference engineer it is a one-line config flag (`use_bias` or `attention_bias` in HF transformers).

**2.4 Vocabulary and tokenizer**

| Model | Vocab size | Tokenizer base |
|-------|-----------|----------------|
| Llama 3.3 70B | 128,256 | tiktoken-derived BPE |
| Qwen 2.5 72B | 152,064 | Qwen BPE (multilingual + code optimized) |

Differences with inference impact:

* **Embedding matrix size:** Llama 128256 × 8192 ≈ 1.05B params; Qwen 152064 × 8192 ≈ 1.25B params (≈ 2.5 GB FP16 each for embed and LM head, untied). Qwen's larger vocab adds ≈0.4B params to the 72B-vs-70B parameter gap — real, but the ~3% wider FFN contributes the bulk (≈1.8B; see §5).
* **LM head matrix:** same sizes as embeddings (untied).
* **Tokenization efficiency** — for a given text:
  * English: Llama's tokenizer is ~5% more efficient than Qwen's.
  * Chinese: Qwen's tokenizer is ~25–30% more efficient.
  * Code: Qwen's is ~10% more efficient.
* **For deployment latency:** the same Chinese input produces fewer tokens with Qwen → fewer decode steps → lower wall-clock for the same response content. This is a real win in Chinese-language products.

---

**3. KV cache per token, derived from config**

A senior engineer derives this on a whiteboard. Both models:

```text
kv_bytes_per_token = 2 × L × num_kv_heads × head_dim × bytes
                   = 2 × 80 × 8 × 128 × 2 (FP16)
                   = 327,680 bytes
                   ≈ 320 KB / token
```

| Context | KV cache @ FP16 | @ FP8 | @ INT4 |
|---------|------------------|-------|--------|
| 4,096   | 1.3 GB          | 0.65 GB | 0.33 GB |
| 32,768  | 10.5 GB         | 5.25 GB | 2.6 GB  |
| 131,072 | 42 GB           | 21 GB   | 10.5 GB |

**Per request.** This is the same for both models. **Long-context serving forces the FP8-KV decision** regardless of which model you pick from this pair.

---

**4. Tensor shapes per layer — exactly what gets matmul'd**

For a forward pass step (single token, decode batch=1):

**4.1 Llama 3.3 70B**

| Tensor | Shape | Size FP16 |
|--------|-------|-----------|
| `attn_q.weight` | (8192, 8192) | 134 MB |
| `attn_k.weight` | (8192, 1024) | 17 MB |
| `attn_v.weight` | (8192, 1024) | 17 MB |
| `attn_o.weight` | (8192, 8192) | 134 MB |
| `ffn_gate.weight` | (8192, 28672) | 470 MB |
| `ffn_up.weight` | (8192, 28672) | 470 MB |
| `ffn_down.weight` | (28672, 8192) | 470 MB |
| Per-layer total | — | **~1.7 GB** |
| × 80 layers | — | **~136 GB FP16** |
| + embed + LM head | 128256 × 8192 × 2 × 2 | **+ 4 GB** |
| **Total** | | **~140 GB FP16** |

</details>

### 4.2 Qwen 2.5 72B

来自官方 `config.json`（`hidden_size: 8192`、`intermediate_size: 29568`）。注意 `attn_q` 投影到 `num_q_heads × head_dim = 64 × 128 = 8192`，K/V 投影到 `8 × 128 = 1024`：

| Tensor | Shape | Size FP16 |
|--------|-------|-----------|
| `attn_q.weight` | (8192, 8192) | 134 MB |
| `attn_k.weight` | (8192, 1024) | 17 MB |
| `attn_v.weight` | (8192, 1024) | 17 MB |
| `attn_o.weight` | (8192, 8192) | 134 MB |
| `ffn_gate.weight` | (8192, 29568) | 485 MB |
| `ffn_up.weight` | (8192, 29568) | 485 MB |
| `ffn_down.weight` | (29568, 8192) | 485 MB |
| QKV biases | (8192 + 1024 + 1024) × 2 | ~25 KB |
| Per-layer total | — | **~1.76 GB** |
| × 80 layers | — | **~141 GB FP16** |
| + embed + LM head | 152064 × 8192 × 2 × 2 | **+ 5 GB** |
| **Total** | | **~146 GB FP16** |

每 layer 占用与 Llama 3.3 70B（~1.7 GB）几乎相同——FFN 只大了约 3%。72B 与 70B 的全部差距于是来自稍宽的 FFN（每 layer ~3%）+ 更大的词表（152K 对 128K → embed + LM head 多约 1 GB）+ 可忽略的 QKV 偏置。

> 🔍 **这个数能对上——12288 那个传言对不上。** ~1.76 GB/layer × 80 + ~5 GB embedding ≈ **146 GB ≈ 73B 参数 × 2 B**，与标称的 "72B" 相符。12288 / 49152 这个数字会给出 ~328 GB（~164B 参数）——而这种不吻合恰恰就是破绽。**永远从公开的 `config.json` 推导，再对照标称参数数量做合理性检查。** 第三方总结把宽度搞错的频率高得惊人。

**要点：** Llama 3.3 70B 与 Qwen 2.5 72B 在*架构上几乎相同*，都是 dense decoder。二者真正与推理相关的差异是：

* **Tokenizer 效率** — Qwen 处理中文 / 代码的 token 数更少 → 相同内容需要更少的 decode 步（最实际的收益）。
* **词表大小** — Qwen 的 152K 对 Llama 的 128K，使 embed + LM head 多约 1 GB。
* **FFN 宽度与 QKV 偏置** — 都真实存在但可忽略（每 layer ~3%；~2 MB）。
* **训练数据 / 后训练质量** — 主导性的*行为*差异，在推理计算图上不可见。

---

## 5. 参数核算——70B 与 72B 从何而来

用修正后的数字做一次快速合理性检查。

### Llama 3.3 70B

```text
Per-layer attention (Q + K + V + O):
  Q: 8192 × 8192 = 67M
  K: 8192 × 1024 = 8.4M
  V: 8192 × 1024 = 8.4M
  O: 8192 × 8192 = 67M
  Attention total: ~151M

Per-layer FFN (gate + up + down):
  gate: 8192 × 28672 = 235M
  up:   8192 × 28672 = 235M
  down: 28672 × 8192 = 235M
  FFN total: ~705M

Per-layer total: ~856M
× 80 layers: ~68.5B

Embeddings + LM head: 128256 × 8192 × 2 = ~2.1B (untied)
RMSNorm: ~negligible

Total: ~70.6B
```

在取整范围内与 "70B" 相符。

### Qwen 2.5 72B

```text
Per-layer (slightly larger):
  Attention same: ~151M
  FFN: 8192 × 29568 × 3 = ~727M
  Per-layer: ~878M
× 80 layers: ~70.2B

Embeddings + LM head: 152064 × 8192 × 2 = ~2.5B
QKV biases: ~2 MB (negligible at param count)

Total: ~72.7B
```

在取整范围内与 "72B" 相符。

两个标称之间约 2B 的参数差异主要来自宽了约 3% 的 FFN（每 layer 多 ≈22M × 80 ≈ 1.8B）；更大的词表在 embed + LM head 上增加 ≈0.4B。就架构而言，对推理工程可视为几乎相同。

---

## 6. 实际影响——部署这两者之间有何变化

### 6.1 Runtime 选择

* 截至 2026 年中期，两者在 **vLLM**、**SGLang**、**TensorRT-LLM**、**llama.cpp** 中都作为一等模型得到支持。
* 两者在 runtime 特有行为上没有实质性差异。
* `transformers` 配置差异：Qwen 用 `attention_bias=True`，两者都用 `tie_word_embeddings=False`。

### 6.2 量化

* **AWQ-INT4 在两者上都表现良好。** 校准集应匹配部署的语言分布。
* **arXiv:2408.15301 的 W8A8 异常适用于 Llama 3.3 70B。** 按同一篇论文的测量，Qwen 2.5 72B 对 W8A8 更耐受。
* 对 W4A4 / FP4，两者都能从 QuaRot 或 SpinQuant 中受益。Lecture 03 会展开讲。

### 6.3 常见硬件上的部署形态

| Hardware | Llama 3.3 70B | Qwen 2.5 72B | Notes |
|----------|----------------|----------------|-------|
| 1× H100 80G | 仅 INT4，紧张 | 仅 INT4，非常紧张 | KV cache 压力限制 batch |
| 1× H200 141G | FP8 小 batch，INT4 带 batch | 同上 | H200 是单 GPU 70B 级的最佳选择 |
| 2× H100 NVL (TP=2) | FP8 带 batch | 同上 | FP8 下 ~35 GB/GPU，加上 KV 也宽裕 |
| 4× H100 80G (TP=4) | 原生 FP16/BF16 | 原生 FP16/BF16 | 35–36B/GPU，宽裕 |
| 8× H100/H200 (TP=8) | FP16、大 batch、长上下文 | 同上 | 最大吞吐的生产环境最优配置 |

对 32K 上下文的 Llama 3.3 70B，2026 年 2× H100 NVL 上 FP8 权重 + FP8 KV 是高性价比的 recipe。Qwen 2.5 72B 同样受益于该 recipe，没有意外（因词表而内存略多）。


<details>
<summary>English original</summary>

**4.2 Qwen 2.5 72B**

From the official `config.json` (`hidden_size: 8192`, `intermediate_size: 29568`). Note `attn_q` projects to `num_q_heads × head_dim = 64 × 128 = 8192`, and K/V to `8 × 128 = 1024`:

| Tensor | Shape | Size FP16 |
|--------|-------|-----------|
| `attn_q.weight` | (8192, 8192) | 134 MB |
| `attn_k.weight` | (8192, 1024) | 17 MB |
| `attn_v.weight` | (8192, 1024) | 17 MB |
| `attn_o.weight` | (8192, 8192) | 134 MB |
| `ffn_gate.weight` | (8192, 29568) | 485 MB |
| `ffn_up.weight` | (8192, 29568) | 485 MB |
| `ffn_down.weight` | (29568, 8192) | 485 MB |
| QKV biases | (8192 + 1024 + 1024) × 2 | ~25 KB |
| Per-layer total | — | **~1.76 GB** |
| × 80 layers | — | **~141 GB FP16** |
| + embed + LM head | 152064 × 8192 × 2 × 2 | **+ 5 GB** |
| **Total** | | **~146 GB FP16** |

Almost the same per-layer footprint as Llama 3.3 70B (~1.7 GB) — the FFN is just ~3% larger. The whole 72B-vs-70B gap is then a slightly wider FFN (~3% per layer) + a larger vocab (152K vs 128K → ~1 GB more in embed + LM head) + negligible QKV biases.

> 🔍 **This reconciles — the 12288 myth didn't.** ~1.76 GB/layer × 80 + ~5 GB embeddings ≈ **146 GB ≈ 73B params × 2 B**, matching the "72B" on the box. The 12288 / 49152 figure would have given ~328 GB (~164B params) — and that mismatch is exactly the tell. **Always derive from the published `config.json`, then sanity-check against the advertised parameter count.** Third-party summaries get widths wrong surprisingly often.

**Takeaway:** Llama 3.3 70B and Qwen 2.5 72B are *architecturally near-identical* dense decoders. Their real inference-relevant differences are:

* **Tokenizer efficiency** — fewer tokens for Chinese / code with Qwen → fewer decode steps for the same content (the biggest practical win).
* **Vocab size** — Qwen's 152K vs Llama's 128K adds ~1 GB to embed + LM head.
* **FFN width and QKV biases** — both real but negligible (~3% per layer; ~2 MB).
* **Training data / post-training quality** — the dominant *behavioral* difference, invisible to the inference graph.

---

**5. Parameter accounting — where 70B and 72B come from**

Quick sanity check using the corrected numbers.

**Llama 3.3 70B**

```text
Per-layer attention (Q + K + V + O):
  Q: 8192 × 8192 = 67M
  K: 8192 × 1024 = 8.4M
  V: 8192 × 1024 = 8.4M
  O: 8192 × 8192 = 67M
  Attention total: ~151M

Per-layer FFN (gate + up + down):
  gate: 8192 × 28672 = 235M
  up:   8192 × 28672 = 235M
  down: 28672 × 8192 = 235M
  FFN total: ~705M

Per-layer total: ~856M
× 80 layers: ~68.5B

Embeddings + LM head: 128256 × 8192 × 2 = ~2.1B (untied)
RMSNorm: ~negligible

Total: ~70.6B
```

Matches "70B" within rounding.

**Qwen 2.5 72B**

```text
Per-layer (slightly larger):
  Attention same: ~151M
  FFN: 8192 × 29568 × 3 = ~727M
  Per-layer: ~878M
× 80 layers: ~70.2B

Embeddings + LM head: 152064 × 8192 × 2 = ~2.5B
QKV biases: ~2 MB (negligible at param count)

Total: ~72.7B
```

Matches "72B" within rounding.

The ~2B parameter difference between the two labels comes mostly from the ~3% wider FFN (≈22M more per layer × 80 ≈ 1.8B); the larger vocab adds ≈0.4B in embed + LM head. Architecturally, treat them as nearly identical for inference engineering purposes.

---

**6. Practical impact — what changes between deploying these two**

**6.1 Runtime picks**

* Both supported as first-class models in **vLLM**, **SGLang**, **TensorRT-LLM**, **llama.cpp** as of mid-2026.
* No runtime-specific behavior differs meaningfully between the two.
* `transformers` config differences: `attention_bias=True` for Qwen, `tie_word_embeddings=False` for both.

**6.2 Quantization**

* **AWQ-INT4 works well on both.** Calibration sets should match the deployment language distribution.
* **The arXiv:2408.15301 W8A8 anomaly applies to Llama 3.3 70B.** Qwen 2.5 72B is more W8A8-tolerant by the same paper's measurements.
* For W4A4 / FP4, both benefit from QuaRot or SpinQuant. We will walk this in Lecture 03.

**6.3 Deployment shape on common hardware**

| Hardware | Llama 3.3 70B | Qwen 2.5 72B | Notes |
|----------|----------------|----------------|-------|
| 1× H100 80G | INT4 only, tight | INT4 only, very tight | KV cache pressure limits batch |
| 1× H200 141G | FP8 with small batch, INT4 with batch | Same | H200 is the sweet spot for single-GPU 70B-class |
| 2× H100 NVL (TP=2) | FP8 with batch | Same | ~35 GB/GPU at FP8 fits comfortably with KV |
| 4× H100 80G (TP=4) | FP16/BF16 native | FP16/BF16 native | 35–36B/GPU, comfortable |
| 8× H100/H200 (TP=8) | FP16, large batch, long context | Same | production sweet spot for max throughput |

For Llama 3.3 70B at 32K context, FP8 weights + FP8 KV on 2× H100 NVL is the cost-effective recipe in 2026. Qwen 2.5 72B benefits from the same recipe with no surprises (slightly more memory due to vocab).

</details>

### 6.4 Tokenizer 驱动的成本差异

对纯英文聊天产品，两个模型在相同 recipe 下的 **$/MTok 几乎完全相同**。对中文产品，Qwen 2.5 72B 对同一回复生成的 token 少 **~25%** ——意味着*有效* $/MTok 低约 25%。在很多产品场景下，这是两者之间**最大的工程差异**。

---

## Lab —— 从磁盘推导出两份 config 并产出并排成本报告

目标：在你的 benchmark 仓库中产出一份含并排成本数字的 Markdown 报告。

1. 从官方 Hugging Face 仓库**下载两份 `config.json` 文件**。
2. **以程序方式计算：**
   * FP16、FP8、INT4 下的每层权重内存。
   * 参数总数（校验是否与 70B / 72B 标签一致）。
   * FP16、FP8、INT4 下每 token 的 KV 字节数。
   * Embedding + LM head 内存。
3. **渲染一张对比表**，涵盖两个模型、全部三种精度。
4. **预测**四种场景下的总 HBM：(batch=1, ctx=4K) / (batch=1, ctx=128K) / (batch=16, ctx=4K) / (batch=16, ctx=32K)。
5. **判断**每种场景会强制要求哪种硬件 × 精度 recipe。写下每种场景的推理依据。

通过标准：另一位工程师能依据相同 config 复现你的报告，且预测值与实测数字吻合（Lecture 02 将在真实 H100/H200 上验证）。

---

## 自检

1. W8A8 Llama-3-70B 异常（arXiv:2408.15301）已有充分记录。同一异常是否可能适用于 Qwen 2.5 72B？基于你目前对二者架构相似性的了解，为什么适用或不适用？
2. 同事提议在 4× H100 80G 上以 FP16 部署 Qwen 2.5 72B，用于英文聊天产品。不实际运行的情况下：能装下吗？给出 KV cache + 权重内存的算式。
3. 对 4× H100 上的中文聊天产品，你会选 Llama 3.3 70B INT4 还是 Qwen 2.5 72B INT4？用两句话结合 tokenizer 效率说明理由。
4. 两个模型共享相同的 KV head 结构（8 个 KV head × head_dim 128）。在 batch=64、context=8K、FP8 KV 下，你为 KV cache 单独预留的最小 HBM 是多少？
5. 原版 Qwen 2.5 72B 的 `hidden_size=8192`，并非某些二手来源所引用的 12288。对于从非一手来源读取产品规格的推理工程师，这个教训是什么？

---

## 参考文献

* Llama 3.3 70B 模型卡 —— [huggingface.co/meta-llama/Llama-3.3-70B-Instruct](https://huggingface.co/meta-llama/Llama-3.3-70B-Instruct)
* Qwen 2.5 72B 模型卡 —— [huggingface.co/Qwen/Qwen2.5-72B-Instruct](https://huggingface.co/Qwen/Qwen2.5-72B-Instruct)
* Qwen 2.5 技术报告 —— [arXiv:2412.15115](https://arxiv.org/abs/2412.15115)
* "The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization" —— [arXiv:2408.15301](https://arxiv.org/abs/2408.15301)
* GQA 论文 —— [arXiv:2305.13245](https://arxiv.org/abs/2305.13245)
* RoPE 论文 —— [arXiv:2104.09864](https://arxiv.org/abs/2104.09864)
* YaRN —— [arXiv:2309.00071](https://arxiv.org/abs/2309.00071) —— Qwen 2.5 采用的上下文扩展方法（原生 32K → 推理时 128K）；Llama 3.1/3.3 则改用 Meta 的 `"rope_type": "llama3"` 频率缩放 + 长上下文继续预训练

交叉引用：

* [Part 1 → Lecture 02 —— Transformer 执行](../01-基础/Lecture-02.md)
* [阶段 5 → 边缘 AI → Qwen 推理优化 → Lecture 01 —— 架构深度剖析](../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-01.md) —— Qwen 4B/72B 并排对比（侧重点不同，材料相关）

---

## 截至 2026-06 有效

Config 锁定自撰写时的官方 Hugging Face 模型卡。若 Meta 或阿里巴巴发布任一模型的 v2 / 小版本更新且带有架构变更，请刷新。

---

## 下一步

* 下一步：[Lecture 02 —— Hopper 硬件故事](Lecture-02.md)
* 上级：[Part 2 —— Hopper 上的稠密模型](README.md)


<details>
<summary>English original</summary>

**6.4 Tokenizer-driven cost difference**

For an English-only chat product the two models have **nearly identical $/MTok** at the same recipe. For a Chinese-language product Qwen 2.5 72B emits **~25% fewer tokens** for the same response — meaning the *effective* $/MTok is ~25% lower. This is the **largest engineering difference** between the two for many product contexts.

---

**Lab — derive both configs from disk and produce a side-by-side cost report**

Goal: a Markdown report in your benchmark repo with side-by-side cost numbers.

1. **Download both `config.json` files** from the official Hugging Face repos.
2. **Compute, programmatically:**
   * Per-layer weight memory at FP16, FP8, INT4.
   * Total parameter count (verify matches 70B / 72B labels).
   * KV bytes per token at FP16, FP8, INT4.
   * Embedding + LM head memory.
3. **Render a comparison table** with both models, all three precisions.
4. **Predict** total HBM at four scenarios: (batch=1, ctx=4K) / (batch=1, ctx=128K) / (batch=16, ctx=4K) / (batch=16, ctx=32K).
5. **Decide** which hardware × precision recipe each scenario forces. Write down the reasoning for each.

Pass criterion: your report can be reproduced by another engineer from the same configs, and the predictions match measured numbers (Lecture 02 will validate them on real H100/H200).

---

**Self-check**

1. The W8A8 Llama-3-70B anomaly (arXiv:2408.15301) is well-documented. Does the same anomaly likely apply to Qwen 2.5 72B? Why or why not, given what you now know about the architectural similarities?
2. A teammate proposes deploying Qwen 2.5 72B FP16 on 4× H100 80G for an English-language chat product. Without running it: does it fit? Show the KV cache + weight memory math.
3. For a Chinese-language chat product at 4× H100, would you pick Llama 3.3 70B INT4 or Qwen 2.5 72B INT4? Justify in two sentences using tokenizer efficiency.
4. Both models share the same KV head structure (8 KV heads × head_dim 128). What is the minimum HBM you would budget for KV cache alone at batch=64, context=8K, FP8 KV?
5. The vanilla Qwen 2.5 72B has `hidden_size=8192`, not the 12288 some secondary sources cite. What is the lesson for an inference engineer reading product specs from non-primary sources?

---

**References**

* Llama 3.3 70B model card — [huggingface.co/meta-llama/Llama-3.3-70B-Instruct](https://huggingface.co/meta-llama/Llama-3.3-70B-Instruct)
* Qwen 2.5 72B model card — [huggingface.co/Qwen/Qwen2.5-72B-Instruct](https://huggingface.co/Qwen/Qwen2.5-72B-Instruct)
* Qwen 2.5 technical report — [arXiv:2412.15115](https://arxiv.org/abs/2412.15115)
* "The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization" — [arXiv:2408.15301](https://arxiv.org/abs/2408.15301)
* GQA paper — [arXiv:2305.13245](https://arxiv.org/abs/2305.13245)
* RoPE paper — [arXiv:2104.09864](https://arxiv.org/abs/2104.09864)
* YaRN — [arXiv:2309.00071](https://arxiv.org/abs/2309.00071) — context extension method used by Qwen 2.5 (32K native → 128K at inference); Llama 3.1/3.3 instead use Meta's `"rope_type": "llama3"` frequency scaling + long-context continued pretraining

Cross-references:

* [Part 1 → Lecture 02 — Transformer execution](../01-基础/Lecture-02.md)
* [Phase 5 → Edge AI → Qwen Inference Optimization → Lecture 01 — Architecture Deep Dive](../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-01.md) — Qwen 4B/72B side-by-side (different focus, related material)

---

**Current as of 2026-06**

Configs pinned from the official Hugging Face cards at the time of writing. Refresh if Meta or Alibaba publishes a v2 / point release of either model with architectural changes.

---

**Next**

* Next: [Lecture 02 — Hopper hardware story](Lecture-02.md)
* Up: [Part 2 — Dense at Hopper](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 2 - Dense at Hopper/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%202%20-%20Dense%20at%20Hopper/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
