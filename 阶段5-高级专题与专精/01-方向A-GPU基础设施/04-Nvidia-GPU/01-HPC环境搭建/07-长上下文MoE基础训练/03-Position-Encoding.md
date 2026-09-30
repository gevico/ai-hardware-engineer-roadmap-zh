---
title: Module 03 —— 长上下文的位置编码
description: Module 03 —— 长上下文的位置编码
---

# Module 03 —— 长上下文的位置编码

**Parent:** [长上下文 MoE 基础训练](README.md)

**一句话目标：** 选择并配置一种位置编码方案（带 base scaling 的 RoPE、YaRN、位置插值、ALiBi），使在某个上下文长度上训练的模型无需从头重训即可泛化到长得多的上下文长度。

**前置要求：** Module 02。熟悉旋转位置编码（RoPE）。

**产物：** 一张逐位置的检索准确率曲线图，对比至少三种位置编码策略，模型从 4K 扩展到 32K（或 32K → 128K），在 needle-in-haystack 风格的任务上评估。

---

## 为什么重要

在 4K 上训练的模型，技术上可以在 32K 上评估——矩阵乘能跑通。但超过训练长度后，attention 输出通常是垃圾，因为位置编码并非为外推而设计。选对扩展方案，决定了你需要一次完整重训（昂贵）还是一次短期的继续预训练（便宜）。这是少数几个改对一个架构旋钮就能省下数周算力的领域之一。

---

## 心智模型

### 一段话讲清 RoPE

旋转位置编码用一个与位置相关的矩阵 `R(pos, θ_k)` 旋转 Q 和 K 向量，其中 `θ_k = base^(-2k/D)`。点积 `Q(pos_q)ᵀ K(pos_k)` 于是只依赖于相对偏移 `pos_q − pos_k`。`base`（通常为 10000）控制高频旋转周期的缓慢程度。

### 为什么朴素的 RoPE 在扩展时会失效

RoPE 让每个维度以不同的旋转频率编码。在远超训练长度的位置上，**高频**维度会完成许多模型从未见过的旋转——旋转模式落在训练分布之外。模型在这些位置上的 attention 分数会退化。

具体来说：在 `base = 10000` 且模型训练于 `N = 4096` 的情况下，位置 `pos = 100_000` 产生的点积与模型在预训练中见过的任何东西都毫不相似。

### 三种扩展策略

#### 1. RoPE base scaling

把 `base` 从 `10000` 增大到大约 `500000` 或 `1M`，再短暂继续预训练。base 越高 = 旋转越慢 = 高频维度在每个位置上循环得更少，因此位置 100K 处的旋转模式仍与模型在例如 32K 的继续预训练中见过的相似。

这是最简单的方法。Llama 3.1 在其长上下文变体中使用了 `base = 500000`。在训练长度约 10× 以内效果良好。

#### 2. 位置插值（PI）

不用位置 `pos`，而用 `pos · (N_train / N_extended)`。于是训练于 4K、评估于 32K 的模型使用缩放后的位置 `pos / 8`。旋转模式仍留在训练分布之内，只是被压缩了。

便宜，在中等扩展（2–4×）下无需重训即可工作。每个位置的精度会损失（分辨率下降）。

#### 3. YaRN（Yet another RoPE extensioN）

混合方案：只对**低频**维度施加位置插值（它们需要），高频维度保持不变（PI 会损害那里的分辨率）。加入温度校正以补偿分布偏移。

在最小化继续预训练的前提下，从 32K 扩展到 128K+ 时，经验上这是最好的单一扩展方法。Mistral、DeepSeek、若干 Qwen 长上下文变体都用了它。

#### 4. NTK-aware / 动态 NTK 缩放

YaRN 的概念先驱。从神经正切核（neural tangent kernel）的视角看待 RoPE，调整 base 或位置以保留高频信息。如今一般更偏好 YaRN 而非朴素的 NTK。

### ALiBi（Attention with Linear Biases）

另一种思路：彻底放弃位置编码，给 attention 分数加上逐 head 的线性偏置 `-m_h · |i − j|`。由于该偏置在任意位置都有良好定义，它能自然外推。MPT 和 BloombergGPT 用过它。在现代长上下文模型中不太流行，因为它比 RoPE 更约束 attention 模式。

### 为什么「训练到 128K」≠「在 128K 上有用」

即便位置编码正确，模型也必须**见过含有丰富长程依赖信息的训练数据**。如果你的继续预训练数据只是把互不相关的随机文档拼接起来填满 32K，模型就没有动力真正使用超过比如 1K 的位置。位置编码让长上下文成为可能；数据和 curriculum（Module 06、07）让长上下文变得有用。

---

## 构建

### 环境准备

取一个小型公开模型（例如 Llama-3-8B base 或 Qwen2.5-7B base——任何带 RoPE 的都行）。搭一套评估 harness（agent 运行时框架），在多个位置、多个总上下文长度上跑 needle-in-haystack。


<details>
<summary>English original</summary>

**Module 03 — Positional Encoding for Long Context**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Choose and configure a positional encoding scheme (RoPE with base scaling, YaRN, position interpolation, ALiBi) that lets a model trained at one context length generalize to a much longer one without retraining from scratch.

**Prerequisites:** Module 02. Familiarity with rotary position embeddings (RoPE).

**Artifact:** A per-position retrieval-accuracy plot comparing at least three position-encoding strategies on a model extended from 4K to 32K (or 32K → 128K), evaluated on a needle-in-haystack-style task.

---

**Why it matters**

A model trained at 4K can technically be evaluated at 32K — the matmuls work. But the attention outputs are usually garbage past the training length because the position encoding was not designed to extrapolate. Choosing the right extension scheme determines whether you need a full retrain (expensive) or a short continual pretraining (cheap). This is one of the few areas where the right architectural knob saves weeks of compute.

---

**Mental model**

**RoPE in one paragraph**

Rotary position embedding rotates the Q and K vectors by a position-dependent matrix `R(pos, θ_k)` where `θ_k = base^(-2k/D)`. The dot product `Q(pos_q)ᵀ K(pos_k)` then depends only on the relative offset `pos_q − pos_k`. The `base` (usually 10000) controls how slowly the high-frequency rotations cycle.

**Why naive RoPE breaks at extension**

RoPE encodes each dimension at a different rotational frequency. At positions far beyond the training length, the **high-frequency** dimensions complete many rotations the model has never seen — the rotation pattern is outside the training distribution. The model's attention scores at those positions degenerate.

Concretely: with `base = 10000` and a model trained at `N = 4096`, position `pos = 100_000` produces dot products that look nothing like anything the model saw in pretraining.

**Three extension strategies**

**1. RoPE base scaling**

Increase the `base` from `10000` to something like `500000` or `1M` and continue pretraining briefly. Higher base = slower rotations = the high-frequency dimensions cycle less per position, so the rotational patterns at position 100K still resemble what the model saw during continual pretraining at e.g. 32K.

This is the simplest method. Llama 3.1 used `base = 500000` for its long-context variants. Works well up to ~10× the training length.

**2. Position interpolation (PI)**

Instead of using position `pos`, use `pos · (N_train / N_extended)`. So a model trained at 4K, evaluated at 32K, uses scaled positions `pos / 8`. The rotational pattern stays within the training distribution, just compressed.

Cheap, works without retraining for modest extensions (2–4×). Loses precision per position (resolution drops).

**3. YaRN (Yet another RoPE extensioN)**

Hybrid scheme: apply position interpolation only to the **low-frequency** dimensions (which need it) and leave high-frequency dimensions unchanged (where PI would damage resolution). Adds a temperature correction to compensate for distribution shift.

Empirically the best single extension method for going from 32K to 128K+ with minimal continual pretraining. Used by Mistral, DeepSeek, several Qwen long-context variants.

**4. NTK-aware / dynamic NTK scaling**

Conceptual ancestor of YaRN. Treats RoPE through the lens of the neural tangent kernel and adjusts base or position to preserve high-frequency information. YaRN is generally preferred over plain NTK now.

**ALiBi (Attention with Linear Biases)**

Different approach: drop positional encoding entirely, add a per-head linear bias `-m_h · |i − j|` to attention scores. Naturally extrapolates because the bias is well-defined at any position. MPT and BloombergGPT used this. Less popular in modern long-context models because it constrains the attention pattern more than RoPE.

**Why "trained 128K" ≠ "useful at 128K"**

Even with a correct position encoding, the model must have **seen training data with informative long-range dependencies**. If your continual pretraining data has random unrelated documents concatenated to fill 32K, the model has no incentive to actually use positions past, say, 1K. The position encoding makes long context possible; the data and curriculum (Modules 06, 07) make it useful.

---

**Build it**

**Setup**

Take a small public model (e.g. Llama-3-8B base or a Qwen2.5-7B base — anything with RoPE). Set up an evaluation harness that runs needle-in-haystack at multiple positions and multiple total context lengths.

</details>

### 扫描

对每种扩展策略：

- **基线**：模型保持原样，不做扩展。
- **RoPE base scaling**：在 `N = 32K` 上用 `rope_theta = 500_000` 继续预训练约 1B tokens。（若是快速课程实验，可跳过继续预训练，直接用新的 base 评测——质量会更差，但对比很有启发性。）
- **Position interpolation**：评测时设置 `pos = pos * (N_train / N_eval)`。无需训练。
- **YaRN**：思路相同，但改用 YaRN 公式。参考实现：<https://github.com/jquesnelle/yarn>。

在 needle-in-haystack 上对每种策略评测，条件为：

- 插入位置：`{1K, 4K, 8K, 16K, 24K, 31K}`（针对 32K 上下文）。
- 总上下文：`{8K, 16K, 32K}`。
- 可复用模板：<https://github.com/gkamradt/LLMTest_NeedleInAHaystack>。

### 绘图

```
y = retrieval accuracy (0..1)
x = needle position
lines = (baseline, base-scaling, PI, YaRN)
panels = per total-context value
```

应当看到：

- 基线在超出训练长度后崩溃。
- Position interpolation 在适度扩展时有效，在大幅扩展时退化。
- YaRN 在整个范围内保持准确率。

如果能为 RoPE base scaling 跑一个小规模的继续预训练任务（约 1B tokens），其效果应当追平或超过 YaRN。

---

## 在实际技术栈中使用

`transformers` 库通过 config 中的 `rope_scaling` 字段暴露 RoPE scaling。具体配置：

- Llama 3.1 long-context：`rope_theta = 500000.0` 已固化进模型。
- 使用 YaRN 的 Qwen2.5：`rope_scaling = {"type": "yarn", "factor": 4.0, "original_max_position_embeddings": 32768}`。
- Mistral 7B 32K：相同的 YaRN 配置。

当下游工作采用这些模型之一时，配置会告诉你已经应用了哪种扩展策略。不要再叠加第二种——否则会进一步破坏旋转模式。

对于自己的训练任务，对应的 Megatron-LM flags：

```
--position-embedding-type rope
--rotary-base 500000
--rotary-percent 1.0
--rotary-seq-len-interpolation-factor 1.0   # = no PI; raise for PI
```

Megatron 中的 YaRN 在较新的 fork 里通过自定义 `--rotary-base-strategy yarn` 支持（请查阅当前的 main）。

---

## 度量

对每个（策略，位置，上下文长度）行：

- 检索准确率（二值：找到 needle / 未找到）。
- 按（策略，上下文长度）对多个 needle 聚合——报告均值和最差位置。
- 在留出的长文档上计算困惑度（确认整体没有坏掉）。

绘制**逐位置**准确率。单一平均值会掩盖 “lost in the middle”——即上下文中部 50% 的位置系统性地比两端更差这一失效模式。

---

## 交付

放入 `lcm-course/`：

1. `position_encoding_sweep.csv` 和 `position_encoding_sweep.png`——逐位置图。
2. `rope_extension_notes.md`——就 RoPE base scaling、PI、YaRN、ALiBi 各写一段，说明各自修复的失效模式。
3. 一份简短建议：从 4K → 32K、32K → 128K、128K → 1M 分别选哪种方案，每个选择给一条理由。

---

## 相关页面

- [Module 02 — 长上下文 attention 机制](02-Long-Context-Attention.md)
- [Module 06 — 自适应数据流水线](06-Adaptive-Data-Pipelines.md)
- [Module 07 — 长上下文评估](07-Long-Context-Evaluation.md)
- YaRN 论文：<https://arxiv.org/abs/2309.00071>
- “Lost in the Middle” 论文：<https://arxiv.org/abs/2307.03172>
- Effective Long-Context Scaling（Meta）：<https://arxiv.org/abs/2309.16039>


<details>
<summary>English original</summary>

**Sweep**

For each extension strategy:

- **Baseline**: model as-is, no extension.
- **RoPE base scaling**: continual-pretrain ~1B tokens at `N = 32K` with `rope_theta = 500_000`. (For a quick course lab, skip continual pretraining and just evaluate with the new base — quality will be worse but the comparison is instructive.)
- **Position interpolation**: at eval time set `pos = pos * (N_train / N_eval)`. No training.
- **YaRN**: same idea but with the YaRN formula. Reference implementation: <https://github.com/jquesnelle/yarn>.

Evaluate each on needle-in-haystack at:

- Insertion positions: `{1K, 4K, 8K, 16K, 24K, 31K}` (for 32K context).
- Total context: `{8K, 16K, 32K}`.
- Reusable templates: <https://github.com/gkamradt/LLMTest_NeedleInAHaystack>.

**Plot**

```
y = retrieval accuracy (0..1)
x = needle position
lines = (baseline, base-scaling, PI, YaRN)
panels = per total-context value
```

You should see:

- Baseline collapsing past the training length.
- Position interpolation working at modest extension, degrading at large extension.
- YaRN holding accuracy across the range.

If you can run a small continual-pretraining job (~1B tokens) for RoPE base scaling, that should match or beat YaRN.

---

**Use it in the real stack**

The `transformers` library exposes RoPE scaling via the `rope_scaling` field in the config. Concrete configs:

- Llama 3.1 long-context: `rope_theta = 500000.0` baked into the model.
- Qwen2.5 with YaRN: `rope_scaling = {"type": "yarn", "factor": 4.0, "original_max_position_embeddings": 32768}`.
- Mistral 7B 32K: same YaRN config.

When you adopt one of these models for downstream work, the config tells you what extension strategy is already applied. Do not stack a second one on top — you will break the rotation pattern further.

For your own training runs, the corresponding Megatron-LM flags:

```
--position-embedding-type rope
--rotary-base 500000
--rotary-percent 1.0
--rotary-seq-len-interpolation-factor 1.0   # = no PI; raise for PI
```

YaRN in Megatron is supported via a custom `--rotary-base-strategy yarn` in newer forks (check current main).

---

**Measure it**

Per (strategy, position, context length) row:

- Retrieval accuracy (binary: needle found / not).
- Aggregate across needles per (strategy, context length) — report mean and worst-position.
- Perplexity on a held-out long document (sanity that nothing is broken globally).

Plot **per-position** accuracy. A single average can hide "lost in the middle" — the failure mode where positions in the middle 50% of the context are systematically worse than the edges.

---

**Ship it**

Drop into `lcm-course/`:

1. `position_encoding_sweep.csv` and `position_encoding_sweep.png` — the per-position plot.
2. `rope_extension_notes.md` — one paragraph each on RoPE base scaling, PI, YaRN, ALiBi, with the failure mode each fixes.
3. A short recommendation: which scheme you would pick to go from 4K → 32K, 32K → 128K, and 128K → 1M, with one reason per choice.

---

**Related pages**

- [Module 02 — Long-context attention mechanics](02-Long-Context-Attention.md)
- [Module 06 — Adaptive data pipelines](06-Adaptive-Data-Pipelines.md)
- [Module 07 — Long-context evaluation](07-Long-Context-Evaluation.md)
- YaRN paper: <https://arxiv.org/abs/2309.00071>
- "Lost in the Middle" paper: <https://arxiv.org/abs/2307.03172>
- Effective Long-Context Scaling (Meta): <https://arxiv.org/abs/2309.16039>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/03-Position-Encoding.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/03-Position-Encoding.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
