---
title: Module 01 — 为什么长上下文很难
description: Module 01 — 为什么长上下文很难
---

# Module 01 — 为什么长上下文很难

**父模块：** [长上下文 MoE 基础训练](README.md)

**一句话目的：** 建立扩展直觉，讲清楚究竟哪些计算与内存开销随上下文长度增长、交叉点出现在哪里，以及为什么「把上下文窗口做大」只是这个问题中容易的那一半。

**前置要求：** Transformer 基础。强烈建议先看 FlashAttention 课程的第 1 讲（roofline（性能上界模型）/ IO model）。

**产物：** 一张表，比较 `N ∈ {4K, 32K, 128K, 1M}` 下一个真实模型配置中 attention 与 MLP 的逐层计算量和逐层激活值内存，再加一段结论，指出各区间的主导开销。

---

## 为什么重要

长上下文是**两个截然不同的问题**，人们常常把它们混为一谈：

1. **让模型跑起来**：在长序列长度下不耗尽内存或时间。
2. **让模型真正用上**长上下文，而不是把某个位置之后的 token 当作背景噪声。

系统层面的工作覆盖问题 (1)。问题 (2) 需要位置编码的选择、训练课程、评估和数据 — 见模块 03、06 和 07。如果不能先在目标长度上实际训练，就无法解决问题 (2)。

---

## 心智模型

### 逐层计算量扩展

对一个 Transformer block，hidden size 为 `H`、head 数为 `H_q`、head dim 为 `D`、序列长度为 `N`、MLP 扩展系数为 `4H`：

| 组件 | 每 token FLOPs | 随 N 的扩展方式 |
|-----------|------------------|------------------|
| QKV 投影 | `~6 H²` | 线性 |
| Attention `QKᵀ` + `PV` | `~4 N · D · H_q = 4 N · H`（带 `H = H_q · D`） | **每 token 线性**，**每序列二次** |
| Attention softmax | `~5 N` | 每 token 线性 |
| 输出投影 | `~2 H²` | 线性 |
| MLP up + down（dense） | `~16 H²` | 线性 |

对于长度为 `N` 的**一个序列**，单层总计算量：

```
Attention :  ~ 4 N² · H        (the N² term)
MLP       :  ~ 24 N · H²       (linear in N, quadratic in H)
```

交叉点：当 `4N² H > 24 N H²` 时 attention 超过 MLP，即 `N > 6 H`。对于 `H = 4096`，即 `N > 24576`。对于 `H = 8192`，为 `N > 49152`。于是：

- 4K 上下文：MLP 主导。常规的「增大 H」直觉成立。
- 32K+ 上下文、`H = 4096` 时：attention 主导计算。
- 1M 上下文：attention 占据绝对主导；MLP 开销只是舍入误差。

### 逐层激活值内存扩展

每层、每序列为反向传播存储的激活值：

| 组件 | 字节数（bf16） | 扩展方式 |
|-----------|---------------|-----------|
| Q、K、V 激活值 | `6 N H` | 随 N 线性 |
| Attention scores（不使用 FlashAttention） | 每个 head `2 N²`，再求和 | **二次** |
| Attention 输出 | `2 N H` | 线性 |
| MLP 中间结果 | `2 N · 4H = 8 N H` | 线性 |

`N²` 的 attention scores 矩阵才是真正致命的。FlashAttention 通过在线计算 softmax，把这一项从实际分配的显存中消掉（见 FlashAttention 课程）。使用 FlashAttention 后，attention 激活值内存变为 `O(N H)` — 线性而非二次。

但激活值仍主要由 `N H` ×（层数）主导。对于 `N = 128K, H = 8192, L = 80, bf16`，即每序列 `128_000 · 8192 · 80 · 2 ≈ 168 GB` — 若不使用序列并行或激活值重计算，这个量已经大到存不下。

### KV cache 扩展（推理侧，此处为完整性而提及）

推理时每 token 的 KV 大小：`2 · H_kv · D · 2 bytes`（K + V，bf16）。对于 32K 上下文、`H_kv = 8, D = 128`：每层每序列 `2 · 8 · 128 · 32000 · 2 = ~130 MB`。80 层累计约为每序列 10 GB — 这正是你在 72B 推理工作中看到的数字。

### 上下文增长时需要改变什么

| 区间 | 瓶颈 | 所需技术 |
|--------|------------|---------------------|
| 4K | MLP 计算 | TP、ZeRO、混合精度 |
| 32K | Attention 计算开始主导 | FlashAttention |
| 128K | 激活值内存、attention 计算 | + 激活值重计算、序列并行 |
| 1M | Attention 激活值、通信 | + 上下文并行（ring/striped）、更长的流水线、非常谨慎的重叠 |

长上下文不是单一技术，而是一套技术栈，其中每一层都在不同的 N 下才成为必需。

### 准确率问题（「可用」的上下文）

与系统无关的另一条轴：一个能**接受** 128K token 的模型，往往只**用上**前 ~16K 和最后 ~2K，而忽略中间部分。很多「lost in the middle」论文都记录了这一点。解决办法不在 kernel 里 — 而在：

- 位置编码的泛化（模块 03）。
- 持续预训练时的长度课程（模块 06）。
- 需要远距离证据的训练数据（模块 06）。
- 能逐位置衡量检索效果的诚实评估（模块 07）。

长上下文工作的两半 — 系统和准确率 — 必须一起解决。一个物理上支持 1M token、却无法检索出位置 500K 处事实的模型毫无用处。

---

## 动手构建


<details>
<summary>English original</summary>

**Module 01 — Why Long Context Is Hard**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Build the scaling intuition that explains exactly which compute and memory costs grow with context length, where the crossovers happen, and why "make the context window bigger" is the easy half of the problem.

**Prerequisites:** Transformer fundamentals. The FlashAttention course's Lecture 1 (roofline / IO model) is strongly recommended.

**Artifact:** A table comparing per-layer compute and per-layer activation memory for attention vs MLP at `N ∈ {4K, 32K, 128K, 1M}` for one realistic model config, plus a one-paragraph conclusion identifying the dominant cost in each regime.

---

**Why it matters**

Long context is **two distinct problems** that people often conflate:

1. **Making the model run** at long sequence length without running out of memory or time.
2. **Making the model actually use** the long context, instead of treating tokens past some position as background noise.

The systems work covers problem (1). Problem (2) requires positional encoding choices, training curriculum, evaluation, and data — covered in modules 03, 06, and 07. You cannot solve problem (2) without first being able to physically train at the target length.

---

**Mental model**

**Per-layer compute scaling**

For a transformer block with hidden size `H`, number of heads `H_q`, head dim `D`, sequence length `N`, MLP expansion `4H`:

| Component | FLOPs per token | Scales with N as |
|-----------|------------------|------------------|
| QKV projection | `~6 H²` | linear |
| Attention `QKᵀ` + `PV` | `~4 N · D · H_q = 4 N · H` (with `H = H_q · D`) | **linear per token**, **quadratic per sequence** |
| Attention softmax | `~5 N` | linear per token |
| Output projection | `~2 H²` | linear |
| MLP up + down (dense) | `~16 H²` | linear |

Per **sequence** of length `N`, total per-layer compute:

```
Attention :  ~ 4 N² · H        (the N² term)
MLP       :  ~ 24 N · H²       (linear in N, quadratic in H)
```

Crossover: attention overtakes MLP when `4N² H > 24 N H²`, i.e. `N > 6 H`. For `H = 4096`, that is `N > 24576`. For `H = 8192`, `N > 49152`. So:

- At 4K context: MLP dominates. Standard "scale H" intuition holds.
- At 32K+ context with `H = 4096`: attention dominates compute.
- At 1M context: attention is overwhelmingly dominant; MLP cost is rounding error.

**Per-layer activation memory scaling**

Activations stored for the backward pass per layer per sequence:

| Component | Bytes (bf16) | Scales as |
|-----------|---------------|-----------|
| Q, K, V activations | `6 N H` | linear in N |
| Attention scores (without FlashAttention) | `2 N²` per head, summed | **quadratic** |
| Attention output | `2 N H` | linear |
| MLP intermediate | `2 N · 4H = 8 N H` | linear |

The `N²` attention scores matrix is what kills you. FlashAttention removes this term from materialized memory by computing the softmax online (see the FlashAttention course). With FlashAttention, attention activation memory becomes `O(N H)` — linear, not quadratic.

But activations are still dominated by `N H` × (number of layers). For `N = 128K, H = 8192, L = 80, bf16`, that is `128_000 · 8192 · 80 · 2 ≈ 168 GB` per sequence — already too large to keep without sequence parallel or activation recomputation.

**KV cache scaling (inference side, mentioned here for completeness)**

Per-token KV size at inference: `2 · H_kv · D · 2 bytes` (K + V, bf16). For 32K context with `H_kv = 8, D = 128`: `2 · 8 · 128 · 32000 · 2 = ~130 MB` per layer per sequence. Across 80 layers that's ~10 GB per sequence — what you saw with the 72B inference work.

**What needs to change as context grows**

| Regime | Bottleneck | Required techniques |
|--------|------------|---------------------|
| 4K | MLP compute | TP, ZeRO, mixed precision |
| 32K | Attention compute starts to dominate | FlashAttention |
| 128K | Activation memory, attention compute | + activation recomputation, sequence parallel |
| 1M | Attention activation, communication | + context parallel (ring/striped), longer pipeline, very careful overlap |

Long context is not one technique; it is a stack where each layer is necessary at a different N.

**The accuracy problem ("usable" context)**

A separate axis from systems: a model that **accepts** 128K tokens often **uses** the first ~16K and the last ~2K and ignores the middle. This is documented in many "lost in the middle" papers. The fix is not in the kernel — it is in:

- Position encoding generalization (Module 03).
- Length curriculum during continual pretraining (Module 06).
- Training data that requires distant evidence (Module 06).
- Honest evaluation that measures position-by-position retrieval (Module 07).

The two halves of long-context work — systems and accuracy — must be solved together. A model that physically supports 1M tokens but cannot retrieve a fact at position 500K is useless.

---

**Build it**

</details>

### 计算与激活值表

```python
# scaling_table.py
def stats(N, H, L, H_kv, D, bf16=2):
    attn_flops_per_seq = 4 * N * N * H * L
    mlp_flops_per_seq  = 24 * N * H * H * L

    flash_attn_act = (6 * N * H + 2 * N * H) * L * bf16    # Q,K,V + O
    mlp_act        = 8 * N * H * L * bf16
    naive_attn_extra = (2 * N * N) * (H // D) * L * bf16    # the N^2 score per head

    return dict(
        N=N,
        attn_TFLOPs=attn_flops_per_seq / 1e12,
        mlp_TFLOPs=mlp_flops_per_seq / 1e12,
        attn_dom=attn_flops_per_seq > mlp_flops_per_seq,
        flash_act_GB=flash_attn_act / 1e9,
        mlp_act_GB=mlp_act / 1e9,
        naive_extra_GB=naive_attn_extra / 1e9,
    )

# Example: 8B-class model
H, L, H_kv, D = 4096, 32, 8, 128
for N in [4096, 32768, 131072, 1_000_000]:
    s = stats(N, H, L, H_kv, D)
    print(s)

# 72B-class model
H, L, H_kv, D = 8192, 80, 8, 128
for N in [4096, 32768, 131072]:
    s = stats(N, H, L, H_kv, D)
    print(s)
```

保存该表。对每个模型尺寸，写一句话：“从 MLP 主导到 attention 主导的交叉出现在 N ≈ X。”对每一行，写出占主导的激活值内存类别。

### 现实检验

把激活值内存乘以你的批大小和流水线级数。对一次真实的中期训练运行（`batch = 1024 sequences, micro_batch_size = 2, pipeline_parallel = 4`），每个流水线级的激活值很容易就达到数百 GB。记下每一行在哪里首次超过你每块 GPU 可用的 HBM —— 那就是 regime 表中位于你右侧的那项技术（Module 02 / 09）成为必需之处。

---

## 在真实技术栈中使用

NVIDIA 的 [MoE Long-Context Training 技能](https://docs.nvidia.com/nemo/megatron-bridge/nightly/skills/perf-techniques/moe-long-context/SKILL.html) 为 Megatron Bridge 给出了具体的配置表：对不同模型尺寸，在每个 N 下需要什么样的上下文并行大小、激活值重计算、FP8 和 offload 组合。打开它。把每个条目对应到你上表的一行。凡是它与你的粗略估算不一致之处，就是有东西需要更仔细阅读。

Meta 的 [Effective Long-Context Scaling](https://arxiv.org/abs/2309.16039) 是持续预训练把基础模型从 4K 扩展到 32K 的经典范例。该论文中的系统层面经验（数据配比、课程学习、RoPE base 变更）直接汇入模块 03 和 06。

---

## 度量

- 每行的 attention 与 MLP TFLOPs 之比。
- 每行的激活值内存，单位为 GB，按（FlashAttention 激活值、MLP 激活值、naive-attn 额外开销）拆分。
- 每行的“你首先需要加的技术”（重计算、序列并行、上下文并行、offload）。

至少对一组 (N, H, L) 组合，计算你在自己硬件上预期的 wall-clock：`total_FLOPs / (per_GPU_TFLOPs · num_GPUs · 0.5)`（0.5 是现实的利用率）。这个数字告诉你一次训练是几小时还是几周。

---

## 交付

在你的 `lcm-course/` 工作目录中：

1. `scaling_table.py` 及其 `scaling_table.csv` 输出，至少针对两个模型尺寸。
2. `bottleneck_notes.md`，每个 regime 一段，说明哪种开销占主导、你会首先采用哪项技术。
3. 用一段话，用你自己的语言说明“物理上支持 N 个 token”这一轴与“实际使用 N 个 token”这一轴如何区分开。

这三项是后续一切内容的框架。

---

## 相关页面

- [模块 02 —— 长上下文 attention 机制](02-Long-Context-Attention.md)
- [FlashAttention 课程 —— 第 1 讲](../../../../../阶段4-路线C-深度学习推理优化/02-内核工程/01-FlashAttention课程/第01讲-注意力瓶颈与roofline.md)
- NeMo Megatron Bridge：<https://docs.nvidia.com/nemo/megatron-bridge/nightly/>


<details>
<summary>English original</summary>

**Compute & activation table**

```python
# scaling_table.py
def stats(N, H, L, H_kv, D, bf16=2):
    attn_flops_per_seq = 4 * N * N * H * L
    mlp_flops_per_seq  = 24 * N * H * H * L

    flash_attn_act = (6 * N * H + 2 * N * H) * L * bf16    # Q,K,V + O
    mlp_act        = 8 * N * H * L * bf16
    naive_attn_extra = (2 * N * N) * (H // D) * L * bf16    # the N^2 score per head

    return dict(
        N=N,
        attn_TFLOPs=attn_flops_per_seq / 1e12,
        mlp_TFLOPs=mlp_flops_per_seq / 1e12,
        attn_dom=attn_flops_per_seq > mlp_flops_per_seq,
        flash_act_GB=flash_attn_act / 1e9,
        mlp_act_GB=mlp_act / 1e9,
        naive_extra_GB=naive_attn_extra / 1e9,
    )

# Example: 8B-class model
H, L, H_kv, D = 4096, 32, 8, 128
for N in [4096, 32768, 131072, 1_000_000]:
    s = stats(N, H, L, H_kv, D)
    print(s)

# 72B-class model
H, L, H_kv, D = 8192, 80, 8, 128
for N in [4096, 32768, 131072]:
    s = stats(N, H, L, H_kv, D)
    print(s)
```

Save the table. For each model size, write one sentence: "Crossover from MLP-dominated to attention-dominated happens at N ≈ X." For each row, write the dominant activation memory category.

**Reality check**

Multiply the activation memory by your batch size and pipeline stage count. For a realistic mid-training run (`batch = 1024 sequences, micro_batch_size = 2, pipeline_parallel = 4`) you can quickly land at hundreds of GB of activations per stage. Note where each row first exceeds your available HBM per GPU — that's the point at which the technique to your right in the regime table (Module 02 / 09) becomes mandatory.

---

**Use it in the real stack**

NVIDIA's [MoE Long-Context Training skill](https://docs.nvidia.com/nemo/megatron-bridge/nightly/skills/perf-techniques/moe-long-context/SKILL.html) gives concrete config tables for Megatron Bridge: what context-parallel size, activation recomputation, FP8, and offload combinations are needed at each N for different model sizes. Open it. Match each entry to a row of your table above. Where it disagrees with your back-of-envelope, you found something to read more carefully.

Meta's [Effective Long-Context Scaling](https://arxiv.org/abs/2309.16039) is the canonical example of continual pretraining to extend a base model from 4K to 32K. The systems lessons in that paper (data mix, curriculum, RoPE base change) feed directly into modules 03 and 06.

---

**Measure it**

- Per-row attention vs MLP TFLOPs ratio.
- Per-row activation memory in GB, broken down by (FlashAttention activations, MLP activations, naive-attn extra).
- Per-row "first technique you need to add" (recomputation, sequence parallel, context parallel, offload).

For at least one (N, H, L) combination, compute the wall-clock you would expect on your hardware: `total_FLOPs / (per_GPU_TFLOPs · num_GPUs · 0.5)` (the 0.5 is realistic utilization). This number tells you whether a training run is hours or weeks.

---

**Ship it**

In your `lcm-course/` working dir:

1. `scaling_table.py` and its `scaling_table.csv` output for at least two model sizes.
2. `bottleneck_notes.md` with one paragraph per regime explaining which cost dominates and which technique you would reach for first.
3. A one-paragraph statement of how the "physically supports N tokens" axis is separate from the "actually uses N tokens" axis, in your own words.

These three are the framing for everything that follows.

---

**Related pages**

- [Module 02 — Long-context attention mechanics](02-Long-Context-Attention.md)
- [FlashAttention Course — Lecture 1](../../../../../阶段4-路线C-深度学习推理优化/02-内核工程/01-FlashAttention课程/第01讲-注意力瓶颈与roofline.md)
- NeMo Megatron Bridge: <https://docs.nvidia.com/nemo/megatron-bridge/nightly/>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/01-Long-Context-Bottlenecks.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/01-Long-Context-Bottlenecks.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
