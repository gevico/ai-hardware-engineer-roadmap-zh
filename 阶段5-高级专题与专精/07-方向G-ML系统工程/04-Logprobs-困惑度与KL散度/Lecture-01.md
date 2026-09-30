---
title: Lecture 01 - Logprobs：语言模型实际输出什么
description: Lecture 01 - Logprobs：语言模型实际输出什么
---

# Lecture 01 - Logprobs：语言模型实际输出什么

**合集：** [Logprobs、困惑度与 KL 散度](README.md) | **上一篇：** [← 课程目录](README.md) | **下一篇：** [Lecture 02](Lecture-02.md)

---

语言模型并不输出文本。它每一步输出的是整个词表上的一个概率分布——而你从该分布中为实际出现的那个 token 读出的那个数，就是**对数概率**，即 `log q(x_t | x_<t)`。采样、贪心 argmax、beam search、终端里显示的文本——全都是施加在该分布之上的下游装饰。logprob 是模型自己的看法，用它自己的记账单位表达，发生在上述任何装饰运行之前。

这是整门课的原子。交叉熵（Lecture 02）是这些数的均值。困惑度（Lecture 03）是该均值的指数。KL 散度（Lecture 04）是其中两个数的加权差。量化评分、蒸馏损失、RLHF 的 KL 惩罚、投机解码的接受/拒绝检验（Lecture 05）——上述每一个都是逐 token logprob 的聚合。如果你能流畅地读 logprob，本课程其余部分只是记账；如果不能，后面每个指标都是一个黑盒，你只能凭信念相信它。

所以本讲只把一件事做透：带你从 LM head 产生的原始 logit 向量出发，经过 softmax 和对数，走到你能从 OpenAI、Hugging Face、vLLM 或 llama.cpp 中取出的那个数——并解释为什么模型、kernel 和你的评测 harness（agent 运行时框架）都更愿意在 log 空间而非概率空间中做算术。

---

## 学习目标

学完本讲后，你应当能够：

1. 追踪 LM head 流水线 `logits → softmax → probabilities → log → logprobs`，并解释为什么 `log_softmax` 把最后三步融合成一个数值稳定的 kernel。
2. 从两条相互独立的理由出发论证 log 空间——**可加性**（序列联合 logprob = 各 token logprob 之和）与**数值稳定性**（长序列的概率会让浮点数下溢；其对数不会）——并说明 log-sum-exp 技巧。
3. 区分 **token logprob** 与 **sequence logprob**，并在比较不同长度的候选时应用**长度归一化**（均值 / 逐 token logprob）。
4. 预测**温度**对 logprob 和熵的影响，包括 `T → 0`（argmax）与 `T → ∞`（均匀分布）两个极限。
5. 从 OpenAI、Hugging Face Transformers、vLLM 和 llama.cpp 中提取 logprob，并解释 `top_logprobs` 截断会让你付出什么代价。
6. 说出消费 logprob 的各类系统——置信度、排序/best-of-n、引导式解码、评估、幻觉信号——并把 LM head 正确地放到推理 roofline（性能上界模型）上。

---


<details>
<summary>English original</summary>

**Lecture 01 - Logprobs: What a Language Model Actually Emits**

**Collection:** [Logprobs, Perplexity & KL Divergence](README.md) | **Previous:** [← Course index](README.md) | **Next:** [Lecture 02](Lecture-02.md)

---

A language model does not emit text. It emits, at every step, a probability distribution over its entire vocabulary — and the single number you read off that distribution for the token that actually appeared is the **log-probability**, the `log q(x_t | x_<t)`. Sampling, greedy argmax, beam search, the text in your terminal — all of it is downstream cosmetics applied to that distribution. The logprob is the model's own opinion, in its own currency, before any of those cosmetics run.

This is the atom of the entire course. Cross-entropy (Lecture 02) is the mean of these numbers. Perplexity (Lecture 03) is the exponential of that mean. KL divergence (Lecture 04) is a weighted difference of two of them. Quantization grading, distillation loss, the RLHF KL penalty, the speculative-decoding accept/reject test (Lecture 05) — every one of those is an aggregation of per-token logprobs. If you read logprobs fluently, the rest of the course is bookkeeping; if you don't, every later metric is a black box you can only trust on faith.

So this lecture does one thing exhaustively: it takes you from the raw logit vector the LM head produces, through softmax and the log, to the number you can extract from OpenAI, Hugging Face, vLLM, or llama.cpp — and it explains why the model, the kernels, and your evaluation harness all prefer to do their arithmetic in log-space rather than probability-space.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. Trace the LM-head pipeline `logits → softmax → probabilities → log → logprobs`, and explain why `log_softmax` fuses the last three steps into one numerically stable kernel.
2. Justify log-space on two independent grounds — **additivity** (joint sequence logprob = sum of token logprobs) and **numerical stability** (a long-sequence probability underflows float; its log does not) — and state the log-sum-exp trick.
3. Distinguish a **token logprob** from a **sequence logprob**, and apply **length normalization** (mean / per-token logprob) when comparing candidates of different lengths.
4. Predict the effect of **temperature** on logprobs and entropy, including the `T → 0` (argmax) and `T → ∞` (uniform) limits.
5. Extract logprobs from OpenAI, Hugging Face Transformers, vLLM, and llama.cpp, and explain what `top_logprobs` truncation costs you.
6. Name the systems that consume logprobs — confidence, ranking/best-of-n, guided decoding, evaluation, hallucination signals — and place the LM head correctly on the inference roofline.

---

</details>

## 1. 从 logits 到 logprobs

Transformer 的主体为当前位置产出一个单独的隐藏向量 `h ∈ R^{d}`，其中 `d` 是模型维度（Llama-3-8B 为 4096，Qwen2.5-7B 为 3584）。**LM head**——通常就是一个权重矩阵 `W_U ∈ R^{V × d}`，其中 `V` 是词表大小——把该隐藏向量投影成词表中每个词条对应一个实数：

```text
logits z = W_U · h          z ∈ R^{V}
```

`z` 就是 **logit 向量**。它的各分量是无界实数（可正可负）；它们*不是*概率，求和后也不等于任何特定值。要把它们变成词表上的一个分布 `q(·)`，需施加 **softmax**：

```text
                 exp(z_i)
q_i = softmax(z)_i = ─────────────         (i indexes the vocabulary)
                    Σ_j exp(z_j)
```

分母 `Z = Σ_j exp(z_j)` 是**配分函数** / 归一化因子；它保证 `Σ_i q_i = 1` 且 `q_i > 0`。现在取对数，就得到词条 `i` 的 **logprob**：

```text
log q_i = z_i − log Σ_j exp(z_j) = z_i − log Z
```

最后一行就是全部关键，值得盯住看：**logprob 不过是该 token 自身的 logit 减去一个共享的归一化因子。** 给定步中每个 token 减去的都是*同一个* `log Z`。计算中昂贵、覆盖整个分布的那部分就是这个标量；其余都只是该 token 的原始得分。

先算 `softmax` 再算 `log` 既浪费又数值脆弱（先指数、再归一化、然后取对数——三次丢失精度的机会）。因此每个框架都提供融合的 **`log_softmax`**，直接计算 `z_i − log Z`：

```text
log_softmax(z)_i = z_i − logsumexp(z)
logsumexp(z)     = log Σ_j exp(z_j)
```

下表列出了本课程后续要反复摆弄的四个量。

| 量 | 符号 | 形状 | 范围 | 求和为 |
|---|---|---|---|---|
| 隐藏状态 | `h` | `[d]` | 无界 | — |
| Logits | `z = W_U h` | `[V]` | 无界 | — |
| 概率 | `q = softmax(z)` | `[V]` | `(0, 1)` | `1` |
| Logprobs | `log q = log_softmax(z)` | `[V]` | `(−∞, 0]` | — |

注意 logprob 的范围：它总是 `≤ 0`，因为概率是 `≤ 1`，而某个 `≤ 1` 的东西的 `log` 就是 `≤ 0`。logprob 为 `0` 意味着概率 `1`（确定性）；logprob 非常负则意味着模型认为该 token 几乎不可能。

---

## 2. 为什么用对数空间：可加性、稳定性、log-sum-exp

模型、损失函数以及你的评测 harness（agent 运行时框架）都活在对数空间，有两个彼此独立的原因。任一条单独成立就足以支持这么做；两条合在一起，就让在概率空间里做运算成了新手才会犯的错误。

**原因 1 —— 可加性。** 语言模型用链式法则对序列概率做分解，而相邻 token 的概率是**相乘**的：

```text
q(x_1 ... x_T) = Π_{t=1}^{T} q(x_t | x_<t)
```

取对数后，乘积变成**求和**——这是本课程中最有用的一个恒等式：

```text
log q(x_1 ... x_T) = Σ_{t=1}^{T} log q(x_t | x_<t)
```

联合**序列 logprob 就是各 token logprob 之和。** 求和开销小、可微分且稳定；许多小数的乘积则三者皆无。所有「给这个序列打分」的操作——重排序、best-of-n、beam search——在对数空间里都是一次加法。

**原因 2 —— 数值稳定性 / 下溢。** 每个 token 的概率通常都很小（一个平均水平的 token 大约在 `q ≈ 0.05` 附近）。把一百个这样的概率乘起来，你就会深深跌到 IEEE 浮点的下限之下：

```text
typical per-token prob ≈ 0.05
100-token sequence prob ≈ 0.05^100 ≈ 1e-130   →  underflows to 0.0 in float32/float16
same sequence in log-space:
  Σ log(0.05) = 100 × (−3.0) = −300.0          →  a perfectly ordinary float
```

float32 在大约低于 `1e-38` 时下溢为零；float16 则在大约低于 `6e-5` 时下溢。一个真实句子的概率根本无法用数字表示——但它的 **logprob 大约在 −100 到 −300，平淡无奇。** 对数空间不只是让运算更整洁；它是这个量*得以存在*的唯一空间。

**log-sum-exp 技巧。** 朴素地计算 `log Z = log Σ_j exp(z_j)` 也会溢出：只要有某个 logit 很大（比如 `z_j = 90`），`exp(90) ≈ 1.2e39` 就会超出 float32 的最大值（`~3.4e38`）。解决办法是在取指数之前先减去最大 logit `m = max_j z_j`，之后再把它加回来：

```text
logsumexp(z) = m + log Σ_j exp(z_j − m),    where m = max_j z_j
```

平移后的每个指数 `z_j − m` 都是 `≤ 0`，所以每个 `exp(...)` 都落在 `(0, 1]` 内——不会溢出——而最大的那一项恰好是 `1`，因此整个和也不会下溢为零。这个恒等式是精确的（`m` 在代数上被消掉），它也正是 `torch.logsumexp`、`F.log_softmax` 以及每个 CUDA softmax kernel 内部实际做的事。你永远不会手动去调用它，但当融合 softmax kernel 出现在性能分析器上时，这正是它在保护的运算。

---


<details>
<summary>English original</summary>

**1. From logits to logprobs**

A transformer's body produces, for the current position, a single hidden vector `h ∈ R^{d}` where `d` is the model dimension (4096 for Llama-3-8B, 3584 for Qwen2.5-7B). The **LM head** — usually a single weight matrix `W_U ∈ R^{V × d}`, where `V` is the vocabulary size — projects that hidden vector to one real number per vocabulary entry:

```text
logits z = W_U · h          z ∈ R^{V}
```

`z` is the **logit vector**. Its entries are unbounded real numbers (positive and negative); they are *not* probabilities and do not sum to anything in particular. To turn them into a distribution `q(·)` over the vocab, apply the **softmax**:

```text
                 exp(z_i)
q_i = softmax(z)_i = ─────────────         (i indexes the vocabulary)
                    Σ_j exp(z_j)
```

The denominator `Z = Σ_j exp(z_j)` is the **partition function** / normalizer; it forces `Σ_i q_i = 1` and `q_i > 0`. Now take the log to get the **logprob** of entry `i`:

```text
log q_i = z_i − log Σ_j exp(z_j) = z_i − log Z
```

That last line is the whole game and worth staring at: **a logprob is just the token's own logit minus one shared normalizer.** Every token at a given step subtracts the *same* `log Z`. The expensive, distribution-wide part of the computation is that single scalar; everything else is the token's raw score.

Doing it as `softmax` then `log` is wasteful and numerically fragile (you exponentiate, normalize, then take a log — three chances to lose precision). Every framework therefore offers a fused **`log_softmax`** that computes `z_i − log Z` directly:

```text
log_softmax(z)_i = z_i − logsumexp(z)
logsumexp(z)     = log Σ_j exp(z_j)
```

The table below names the four quantities you will be juggling for the rest of the course.

| Quantity | Symbol | Shape | Range | Sums to |
|---|---|---|---|---|
| Hidden state | `h` | `[d]` | unbounded | — |
| Logits | `z = W_U h` | `[V]` | unbounded | — |
| Probabilities | `q = softmax(z)` | `[V]` | `(0, 1)` | `1` |
| Logprobs | `log q = log_softmax(z)` | `[V]` | `(−∞, 0]` | — |

Note the range of a logprob: it is `≤ 0` always, because a probability is `≤ 1` and `log` of something `≤ 1` is `≤ 0`. A logprob of `0` means probability `1` (certainty); a very negative logprob means a token the model considered nearly impossible.

---

**2. Why log-space: additivity, stability, log-sum-exp**

There are two independent reasons the model, the loss function, and your eval harness all live in log-space. Either one alone would justify it; together they make probability-space arithmetic a beginner's mistake.

**Reason 1 — additivity.** A language model factorizes the probability of a sequence by the chain rule, and probabilities of successive tokens **multiply**:

```text
q(x_1 ... x_T) = Π_{t=1}^{T} q(x_t | x_<t)
```

Take the log and the product becomes a **sum** — this is the single most useful identity in the course:

```text
log q(x_1 ... x_T) = Σ_{t=1}^{T} log q(x_t | x_<t)
```

The joint **sequence logprob is just the sum of the per-token logprobs.** Sums are cheap, differentiable, and stable; products of many small numbers are none of those. Every "score this sequence" operation — re-ranking, best-of-n, beam search — is an addition in log-space.

**Reason 2 — numerical stability / underflow.** Per-token probabilities are routinely small (an average token might sit around `q ≈ 0.05`). Multiply a hundred of them and you are deep below the floor of IEEE float:

```text
typical per-token prob ≈ 0.05
100-token sequence prob ≈ 0.05^100 ≈ 1e-130   →  underflows to 0.0 in float32/float16
same sequence in log-space:
  Σ log(0.05) = 100 × (−3.0) = −300.0          →  a perfectly ordinary float
```

float32 underflows to zero below roughly `1e-38`; float16 below roughly `6e-5`. A realistic sentence's probability is unrepresentable as a number — but its **logprob, around −100 to −300, is mundane.** Log-space does not merely tidy the arithmetic; it is the only space in which the quantity *exists*.

**The log-sum-exp trick.** Computing `log Z = log Σ_j exp(z_j)` naively also overflows: if any logit is large (say `z_j = 90`), `exp(90) ≈ 1.2e39` blows past float32's max (`~3.4e38`). The fix is to subtract the max logit `m = max_j z_j` before exponentiating, then add it back:

```text
logsumexp(z) = m + log Σ_j exp(z_j − m),    where m = max_j z_j
```

Every shifted exponent `z_j − m` is `≤ 0`, so every `exp(...)` is in `(0, 1]` — no overflow — and the largest term is exactly `1`, so no underflow-to-zero of the whole sum either. This identity is exact (the `m` cancels algebraically), and it is what is actually inside `torch.logsumexp`, `F.log_softmax`, and every CUDA softmax kernel. You will never call it by hand, but when a fused softmax kernel shows up on a profiler, this is the arithmetic it is protecting.

---

</details>

## 3. token logprob 与 sequence logprob，以及长度归一化

在脑中区分两个量：

| 术语 | 定义 | 典型量级 |
|---|---|---|
| token logprob | `ℓ_t = log q(x_t \| x_<t)` — 一个数，对应一个已实现的 token | `−0.01` 到 `−15` |
| sequence logprob | `L = Σ_{t=1}^{T} ℓ_t` — 对生成的 token 求和 | `−10` 到 `−several hundred` |

sequence logprob 有一个内置陷阱：**它随长度单调非增。** 每多一个 token 就增加一个 `≤ 0` 项，因此更长的序列几乎总是比更短的序列有更负（更低）的总 logprob——*无论质量如何*。如果按原始 sequence logprob 对候选生成进行排序，你基本上是在按简短程度排序。

当候选长度不同时，解决办法是**长度归一化**——除以 token 数量得到**平均（每 token）logprob**：

```text
mean logprob  =  L / T  =  (1/T) Σ_{t=1}^{T} log q(x_t | x_<t)
```

这个每 token 平均值正好是模型分配给该序列的**交叉熵**的负值——通往第 02 讲的桥梁——而其负值的指数是**困惑度**（第 03 讲）。所以“平均 logprob”、“负 NLL”和“−log PPL”是同一个数的三个名称；你都会遇到。

何时使用哪个：

- **比较固定长度的续写**（例如，等长的多项选择答案 token，或在两个模型下对同一目标字符串打分）：原始 sequence logprob 就可以且正确。
- **比较不同长度的自由生成**（best-of-n 采样、束搜索假设）：归一化，否则短的假设仅凭长度就会胜出。束搜索实现会暴露一个 `length_penalty` 指数 `α` 并除以 `T^α` 正是为了调节这一点；`α = 1` 是纯平均 logprob，`α = 0` 是原始和。

```python
import torch.nn.functional as F

# token_logprobs: 1-D tensor of per-token logprobs for ONE candidate
seq_logprob  = token_logprobs.sum().item()              # ranks short candidates higher
mean_logprob = token_logprobs.mean().item()             # length-fair; = -NLL
length_penalized = seq_logprob / (len(token_logprobs) ** 0.7)   # beam-style, alpha=0.7
```

---

## 4. 温度：在读取分布之前重塑它

**温度** `T` 在 softmax *之前* 重新缩放 logits，将每个 logit 除以 `T`：

```text
q_i(T) = softmax(z / T)_i = exp(z_i / T) / Σ_j exp(z_j / T)
```

其效果是拉伸或压缩 logits 之间的差距，从而锐化或平坦化得到的分布：

| 区间 | 对分布的影响 | 对熵的影响 | 对 logprob 的影响 |
|---|---|---|---|
| `T < 1` (例如 0.7) | **锐化**——差距扩大，顶部 token 占主导 | 熵 ↓ | 顶部 token logprob ↑（趋于 0），尾部 ↓ |
| `T = 1` | 模型的原始分布 | 原始 | 原始 logprob |
| `T > 1` (例如 1.5) | **平坦化**——差距缩小，质量扩散 | 熵 ↑ | 顶部 token logprob ↓，尾部 ↑ |
| `T → 0⁺` | **argmax**——所有质量集中在单个最大 logit 上 | 熵 → 0 | 顶部 logprob → 0，所有其他 → −∞ |
| `T → ∞` | 在词表上**均匀** | 熵 → `log V`（最大） | 每个 logprob → `−log V` |

```text
logits z = [4.0, 2.0, 1.0]   (3-way toy vocab)

T = 0.5 : q = [0.980, 0.018, 0.002]   sharper, near-argmax
T = 1.0 : q = [0.844, 0.114, 0.042]   native
T = 2.0 : q = [0.629, 0.231, 0.140]   flatter, toward uniform
```

有两则警告对本课程后续内容很重要。第一，**温度是你读取分布的方式的属性，而不是模型的属性。** logits 是固定的；`T` 是采样时的旋钮。第二——也是人们会忘记的一点——**当你计算用于评估、困惑度或 KL 的 logprob 时，你几乎总是想要 `T = 1`**，即模型的真实分布。报告在 `T = 0.7` 下计算的困惑度是范畴错误：你测量了一个模型从未声称的分布。这同样适用于两个模型之间的 KL——在 `T = 1` 下比较它们，否则数字没有意义。温度属于生成；信息论指标属于未受影响的分布。

---


<details>
<summary>English original</summary>

**3. Token logprob vs sequence logprob, and length normalization**

Keep two quantities mentally distinct:

| Term | Definition | Typical magnitude |
|---|---|---|
| Token logprob | `ℓ_t = log q(x_t \| x_<t)` — one number, for one realized token | `−0.01` to `−15` |
| Sequence logprob | `L = Σ_{t=1}^{T} ℓ_t` — the sum over the generated tokens | `−10` to `−several hundred` |

The sequence logprob has a built-in trap: **it is monotonically non-increasing in length.** Every additional token adds another `≤ 0` term, so a longer sequence almost always has a more-negative (lower) total logprob than a shorter one — *regardless of quality*. If you rank candidate generations by raw sequence logprob, you are mostly ranking by brevity.

The fix when candidates differ in length is **length normalization** — divide by the token count to get the **mean (per-token) logprob**:

```text
mean logprob  =  L / T  =  (1/T) Σ_{t=1}^{T} log q(x_t | x_<t)
```

This per-token average is exactly the negative of the **cross-entropy** the model assigns to the sequence — the bridge to Lecture 02 — and its exponential of the negative is **perplexity** (Lecture 03). So "mean logprob," "negative NLL," and "−log PPL" are three names for one number; you will meet all three.

When to use which:

- **Comparing fixed-length continuations** (e.g., multiple-choice answer tokens of equal length, or scoring the same target string under two models): raw sequence logprob is fine and correct.
- **Comparing free-form generations of different lengths** (best-of-n sampling, beam hypotheses): normalize, or short hypotheses win on length alone. Beam search implementations expose a `length_penalty` exponent `α` and divide by `T^α` precisely to tune this; `α = 1` is plain mean-logprob, `α = 0` is raw sum.

```python
import torch.nn.functional as F

# token_logprobs: 1-D tensor of per-token logprobs for ONE candidate
seq_logprob  = token_logprobs.sum().item()              # ranks short candidates higher
mean_logprob = token_logprobs.mean().item()             # length-fair; = -NLL
length_penalized = seq_logprob / (len(token_logprobs) ** 0.7)   # beam-style, alpha=0.7
```

---

**4. Temperature: reshaping the distribution before you read it**

**Temperature** `T` rescales the logits *before* the softmax, dividing every logit by `T`:

```text
q_i(T) = softmax(z / T)_i = exp(z_i / T) / Σ_j exp(z_j / T)
```

The effect is to stretch or compress the gaps between logits, which sharpens or flattens the resulting distribution:

| Regime | Effect on distribution | Effect on entropy | Effect on logprobs |
|---|---|---|---|
| `T < 1` (e.g. 0.7) | **sharpens** — gaps widen, top token dominates | entropy ↓ | top-token logprob ↑ (toward 0), tail ↓ |
| `T = 1` | the model's native distribution | native | native logprobs |
| `T > 1` (e.g. 1.5) | **flattens** — gaps shrink, mass spreads | entropy ↑ | top-token logprob ↓, tail ↑ |
| `T → 0⁺` | **argmax** — all mass on the single largest logit | entropy → 0 | top logprob → 0, all others → −∞ |
| `T → ∞` | **uniform** over the vocab | entropy → `log V` (maximal) | every logprob → `−log V` |

```text
logits z = [4.0, 2.0, 1.0]   (3-way toy vocab)

T = 0.5 : q = [0.980, 0.018, 0.002]   sharper, near-argmax
T = 1.0 : q = [0.844, 0.114, 0.042]   native
T = 2.0 : q = [0.629, 0.231, 0.140]   flatter, toward uniform
```

Two warnings that matter for the rest of the course. First, **temperature is a property of how you read the distribution, not of the model.** The logits are fixed; `T` is a sampling-time knob. Second — and this is the one people forget — **when you compute logprobs for evaluation, perplexity, or KL, you almost always want `T = 1`**, the model's true distribution. Reporting perplexity computed at `T = 0.7` is a category error: you measured a distribution the model never claimed. The same applies to KL between two models — compare them both at `T = 1` or the number is meaningless. Temperature belongs to generation; the information-theoretic metrics belong to the untouched distribution.

---

</details>

## 5. 在实践中获取 logprobs

数学在哪里都一样；API 则不然。以下是全景。

| Runtime | 如何获取 logprobs | 截断行为 | 备注 |
|---|---|---|---|
| **OpenAI API** | `logprobs=True`，chat completions 上可选 `top_logprobs=k`（k ≤ 20） | 只返回所选 token 的 logprob 加上 top-`k` 备选 —— **不是完整词表** | 无法据此重建完整分布或精确的归一化因子 |
| **HF Transformers** | `out = model(input_ids); F.log_softmax(out.logits, dim=-1)`，然后对 target ids 做 `gather` | 无 —— **完整**的 `[seq, V]` logits/logprobs 都在内存中 | 参考实现；其他一切皆为其近似 |
| **vLLM** | 采样参数 `logprobs=k`（生成的 token）与 `prompt_logprobs=k`（prompt token） | 每位置 top-`k`，与 OpenAI 相同；`k` 可配置 | `prompt_logprobs` 就是在推理服务速度下对固定字符串打分/评测的方式 |
| **llama.cpp** | server 的 `n_probs` / `logprobs` 字段；CLI 用 `--logits-all` 保留*每个*位置的 logits（不只是最后一个） | 每 token top-`k` probs；`--logits-all` 可启用全序列打分（困惑度） | `--logits-all` 正是 `llama-perplexity` 工具所需要的 |

那张表里最重要的一条区分：**托管 API 为了节省带宽会截断到 top-`k`。** Llama-3 的 128k 词表上完整 logprob 向量是 128k 个 float32 值 = **每 token 512 KB**；Gemma 的 256k 词表翻倍到 **每 token 1 MB**。为每个生成的 token 传输这些数据，会远超文本负载本身，所以 OpenAI/vLLM 只回传 top ~20。这足以判断置信度和排序，但**不足以**计算精确的跨模型 KL 散度（Lecture 04），后者需要完整分布 —— 一旦你试图通过 API 用一个模型给另一个模型打分，就会反复撞上这一约束。

当你掌控模型时，就按参考做法来 —— 完整的 `log_softmax`，然后取出实际出现的 token。下面是在 HF 因果 LM 下对某字符串的 **序列 logprob** 做的一次完整、可运行的计算，且正确处理了 off-by-one 移位：

```python
import torch, torch.nn.functional as F
from transformers import AutoModelForCausalLM, AutoTokenizer

name = "meta-llama/Meta-Llama-3-8B"           # V = 128,256
tok  = AutoTokenizer.from_pretrained(name)
model = AutoModelForCausalLM.from_pretrained(name, torch_dtype=torch.float16,
                                             device_map="auto").eval()

text = "The capital of France is Paris."
ids  = tok(text, return_tensors="pt").input_ids.to(model.device)   # [1, T]

with torch.no_grad():
    logits = model(ids).logits                 # [1, T, V]

# Position t predicts token t+1, so align logits[:, :-1] with targets ids[:, 1:].
logprobs = F.log_softmax(logits[:, :-1, :].float(), dim=-1)         # [1, T-1, V]
targets  = ids[:, 1:]                                               # [1, T-1]
token_lp = logprobs.gather(-1, targets.unsqueeze(-1)).squeeze(-1)   # [1, T-1]

seq_logprob  = token_lp.sum().item()           # additivity: sum of token logprobs
mean_logprob = token_lp.mean().item()          # = -NLL  (Lecture 02)
ppl          = torch.exp(-token_lp.mean()).item()   # = perplexity (Lecture 03)

print(f"sequence logprob = {seq_logprob:.3f} nats")
print(f"mean logprob/tok = {mean_logprob:.3f}   PPL = {ppl:.2f}")
for t, lp in zip(targets[0].tolist(), token_lp[0].tolist()):
    print(f"  {tok.decode([t])!r:>12} : {lp:+.3f}")
```

这段代码里有三处至关重要，几乎人人都栽过：

- **移位。** `logits[:, :-1]` 对齐 `ids[:, 1:]`。位置 `t` 处的 logit 是*对* token `t+1` 的预测；错开一位，所有数字都成了垃圾。同一移位定义了 Lecture 02 中的 teacher forcing。
- **在 `log_softmax` 之前先 `.float()`。** 前向传播为提速以 float16 运行，但 `logsumexp` 在 128k 个条目上会累积舍入误差；把归一化因子升到 float32，否则 logprobs 会漂移 `~0.1` nat —— 足以改变一次困惑度比较。
- **用 `gather`，而非 argmax。** 你要的是字符串中*实际出现*的 token 的 logprob，而不是模型偏好的 token。打分针对的是实际出现的序列；这正是 logprob 的意义所在。

---


<details>
<summary>English original</summary>

**5. Getting logprobs in practice**

The math is identical everywhere; the APIs are not. Here is the landscape.

| Runtime | How you get logprobs | Truncation behavior | Notes |
|---|---|---|---|
| **OpenAI API** | `logprobs=True`, optional `top_logprobs=k` (k ≤ 20) on chat completions | returns only the chosen token's logprob plus top-`k` alternatives — **not the full vocab** | you cannot reconstruct the full distribution or an exact normalizer from this |
| **HF Transformers** | `out = model(input_ids); F.log_softmax(out.logits, dim=-1)`, then `gather` the target ids | none — you hold the **full** `[seq, V]` logits/logprobs in memory | the reference implementation; everything else is an approximation of this |
| **vLLM** | sampling param `logprobs=k` (generated tokens) and `prompt_logprobs=k` (prompt tokens) | top-`k` per position, like OpenAI; `k` configurable | `prompt_logprobs` is how you score/evaluate a fixed string at serving speed |
| **llama.cpp** | server `n_probs` / `logprobs` field; CLI `--logits-all` to keep logits for *every* position (not just the last) | top-`k` probs per token; `--logits-all` enables full-sequence scoring (perplexity) | `--logits-all` is what the `llama-perplexity` tool needs |

The single most important distinction in that table: **hosted APIs truncate to top-`k` to save bandwidth.** A full logprob vector over Llama-3's 128k vocab is 128k float32 values = **512 KB per token**; Gemma's 256k vocab doubles that to **1 MB per token**. Streaming that for every generated token would dwarf the text payload, so OpenAI/vLLM hand back only the top ~20. That is enough for confidence and ranking, but it is **not** enough to compute an exact cross-model KL divergence (Lecture 04), which needs the whole distribution — a recurring constraint you will hit the moment you try to grade one model against another over an API.

When you control the model, do it the reference way — full `log_softmax`, then gather the realized token. Here is a complete, runnable computation of the **sequence logprob** of a string under a HF causal LM, with the off-by-one shift handled correctly:

```python
import torch, torch.nn.functional as F
from transformers import AutoModelForCausalLM, AutoTokenizer

name = "meta-llama/Meta-Llama-3-8B"           # V = 128,256
tok  = AutoTokenizer.from_pretrained(name)
model = AutoModelForCausalLM.from_pretrained(name, torch_dtype=torch.float16,
                                             device_map="auto").eval()

text = "The capital of France is Paris."
ids  = tok(text, return_tensors="pt").input_ids.to(model.device)   # [1, T]

with torch.no_grad():
    logits = model(ids).logits                 # [1, T, V]

# Position t predicts token t+1, so align logits[:, :-1] with targets ids[:, 1:].
logprobs = F.log_softmax(logits[:, :-1, :].float(), dim=-1)         # [1, T-1, V]
targets  = ids[:, 1:]                                               # [1, T-1]
token_lp = logprobs.gather(-1, targets.unsqueeze(-1)).squeeze(-1)   # [1, T-1]

seq_logprob  = token_lp.sum().item()           # additivity: sum of token logprobs
mean_logprob = token_lp.mean().item()          # = -NLL  (Lecture 02)
ppl          = torch.exp(-token_lp.mean()).item()   # = perplexity (Lecture 03)

print(f"sequence logprob = {seq_logprob:.3f} nats")
print(f"mean logprob/tok = {mean_logprob:.3f}   PPL = {ppl:.2f}")
for t, lp in zip(targets[0].tolist(), token_lp[0].tolist()):
    print(f"  {tok.decode([t])!r:>12} : {lp:+.3f}")
```

Three things in that snippet are load-bearing and trip up nearly everyone:

- **The shift.** `logits[:, :-1]` against `ids[:, 1:]`. The logit at position `t` is the prediction *of* token `t+1`; misalign by one and every number is garbage. This same shift defines teacher forcing in Lecture 02.
- **`.float()` before `log_softmax`.** The forward pass runs in float16 for speed, but `logsumexp` over 128k entries accumulates rounding error; upcast to float32 for the normalizer or your logprobs drift by `~0.1` nat — enough to move a perplexity comparison.
- **`gather`, not argmax.** You want the logprob of the token that *actually appeared* in the string, not of the model's preferred token. Scoring is about the realized sequence; that is the whole point of a logprob.

---

</details>

## 6. logprobs 能驱动什么

本讲之后的一切，都是对 logprobs 的汇总手段。主要的使用方：

- **置信度 / 不确定性。** 被选中 token 的 logprob（或它的 `exp`，即概率）就是模型自陈的置信度。低置信度，或每步熵 `H = −Σ q_i log q_i` 偏高，标记出模型正在猜的位置——直接用于选择性预测与弃答。
- **候选排序 / 打分。** 重排序、**best-of-n**、beam search 和 self-consistency 都用（长度归一化的）序列 logprob 给整条序列打分，并保留最优者。其算术就是 §2 的可加性恒等式。
- **受限 / 引导解码。** 语法与 schema 约束的解码器（JSON 模式、regex、函数调用 schema）靠 **masking logits** 工作——在 softmax *之前*把被禁止 token 的 logits 设为 `−∞`，使它们的概率恰为 `0`，归一化项再把概率质量重新分配到合法集合上。
- **评估。** 多选 benchmark（MMLU 风格）常用模型赋予各选项 token 的 logprob 给每个选项打分，并取 argmax 选项——不生成，只用 logprobs。这比采样答案更快、方差更低。
- **幻觉 / 忠实度信号。** 生成中途 token logprob 骤降，或熵突然尖峰，是一个可用的（尽管有噪声的）信号，说明模型已离开有支撑的根据——这是若干基于不确定性的幻觉检测器的基础。

而本课程贯穿始终的主线是：**困惑度（第 03 讲）是平均负 logprob 的 `exp`，KL 散度（第 04 讲）是两个模型之间 logprob 之差的期望。** 二者都是由你刚学会提取的逐 token 数值聚合而成。掌握了原子，分子自会拼装成形。

> **硬件视角：** LM head 是一个 `[d × V]` GEMM——对 Llama-3-8B 而言是 `4096 × 128,256`，约 **仅 unembedding 就有 525M 参数**，而在 decode（逐 token 生成阶段）时它以 GEMV 运行（`h` 是单个 `[d]` 向量）。输出是完整的 `[V]` logits 向量——Llama-3 为 128k 个值，**Gemma 2/3 为 256k**——这正是超大词表模型要在 head 处付出真实带宽代价的原因，也是融合 `log_softmax` 以及（训练时）分块 cross-entropy kernel 存在的原因：避免把完整的 `[seq, V]` float32 张量实体化。提取*完整* logprobs 意味着每一步都要把那整个向量从设备上读下来；`top_logprobs` 的存在正是为了截断它，省下每 token 512 KB–1 MB 的传输。如果 “GEMV”、“bandwidth-bound”、“roofline”（性能上界模型）尚未成为条件反射，请阅读前置要求：[Edge LLM Inference Internals — Lecture 01](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md)。

> **2026 更新：** logprobs 不再是调试时的新奇玩意——它们是生产信号。评测 harness（agent 运行时框架）用它们给多选题打分；护栏与 routing 层读取 token 置信度与熵，以决定何时升级到更大的模型或拒绝；基于不确定性的幻觉检测器以 logprob 下降为触发条件。与此同时，访问权限正在收紧：若干托管推理模型完全限制或省略 logprob 输出（推理 trace 是隐藏的，其 logprobs 亦然），而结构化输出 / 引导解码路径在你看到之前就已 **mask logits**——所以你读回的 logprobs 已经反映了约束掩码，而非模型未被约束时的判断。当你需要精确的跨模型 KL 时，仍然需要完整的词表分布，也就是说需要一个你自己托管的模型；API 的 top-20 做不到。

---

## 更新截至

2026 年 6 月。本讲的数学是永恒的：softmax、`log_softmax`、log-sum-exp 技巧、序列 logprob 的可加性，以及温度极限（`T→0` 对应 argmax，`T→∞` 对应均匀分布）都已尘埃落定，十年后读来仍一字不差。会变的是工具链与访问策略。词表规模在缓慢攀升（Llama-3 为 128k，Gemma 为 256k，如今 256k 对多语言模型已是常态），这持续抬高了全向量 logprob 提取的带宽成本。API 表面在漂移——OpenAI 的 `top_logprobs` 上限、vLLM 的 `prompt_logprobs`、llama.cpp 的 `--logits-all` 在撰写时均为现状，但托管服务商越来越多地把 logprob 访问挡在更高档位之后，或对隐藏推理的模型不予提供。把公式当作承重结构，在依赖 §5 的 API 那一列之前，先对照服务商的最新文档重新核对。


<details>
<summary>English original</summary>

**6. What logprobs power**

Everything past this lecture is a way of summarizing logprobs. The major consumers:

- **Confidence / uncertainty.** The chosen token's logprob (or `exp` of it, the probability) is the model's stated confidence. Low confidence, or high per-step entropy `H = −Σ q_i log q_i`, flags spots where the model is guessing — used directly in selective prediction and abstention.
- **Candidate ranking / scoring.** Re-ranking, **best-of-n**, beam search, and self-consistency all score whole sequences by (length-normalized) sequence logprob and keep the best. The arithmetic is the additivity identity from §2.
- **Constrained / guided decoding.** Grammar- and schema-constrained decoders (JSON mode, regex, function-call schemas) work by **masking logits** — setting disallowed tokens' logits to `−∞` *before* softmax, so their probability is exactly `0` and the normalizer redistributes mass over the legal set.
- **Evaluation.** Multiple-choice benchmarks (MMLU-style) often score each option by the logprob the model assigns to its tokens and pick the argmax option — no generation, just logprobs. This is faster and lower-variance than sampling answers.
- **Hallucination / faithfulness signals.** A sharp drop in token logprob, or a spike in entropy, mid-generation is a usable (if noisy) signal that the model has left supported ground — the basis of several uncertainty-based hallucination detectors.

And the through-line of this course: **perplexity (Lecture 03) is `exp` of the mean negative logprob, and KL divergence (Lecture 04) is an expectation of a difference of logprobs between two models.** Both are aggregations of exactly the per-token numbers you just learned to extract. Master the atom and the molecules assemble themselves.

> **Hardware lens:** the LM head is a `[d × V]` GEMM — for Llama-3-8B that is `4096 × 128,256`, about **525M parameters in the unembedding alone**, and at decode it runs as a GEMV (`h` is a single `[d]` vector). The output is the full `[V]` logit vector — 128k values for Llama-3, **256k for Gemma 2/3** — which is why models with huge vocabularies pay a real bandwidth tax at the head, and why fused `log_softmax` and (during training) chunked cross-entropy kernels exist to avoid materializing the full `[seq, V]` float32 tensor. Extracting *full* logprobs means reading that entire vector off the device every step; `top_logprobs` exists precisely to truncate it and save the 512 KB–1 MB-per-token transfer. If "GEMV," "bandwidth-bound," and "roofline" are not yet reflexes, read the prerequisite: [Edge LLM Inference Internals — Lecture 01](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md).

> **2026 update:** logprobs are no longer a debugging curiosity — they are production signal. Eval harnesses score multiple-choice with them; guardrail and routing layers read token confidence and entropy to decide when to escalate to a larger model or refuse; uncertainty-based hallucination detectors key off logprob drops. At the same time access is tightening: several hosted reasoning models restrict or omit logprob output entirely (the reasoning trace is hidden, and so are its logprobs), and structured-output / guided-decoding paths **mask logits** before you ever see them — so the logprobs you read back already reflect the constraint mask, not the model's unconstrained opinion. When you need an exact cross-model KL, you still need the full vocab distribution, which means a model you host yourself; the API's top-20 will not do it.

---

**Current as of**

June 2026. The mathematics of this lecture is permanent: softmax, `log_softmax`, the log-sum-exp trick, additivity of sequence logprobs, and the temperature limits (`T→0` argmax, `T→∞` uniform) are settled and will read identically in a decade. What moves is the tooling and the access policy. Vocabulary sizes have crept up (Llama-3 at 128k, Gemma at 256k, with 256k now common for multilingual models), which steadily raises the bandwidth cost of full-vector logprob extraction. API surfaces drift — OpenAI's `top_logprobs` cap, vLLM's `prompt_logprobs`, and llama.cpp's `--logits-all` are current as written, but hosted providers increasingly gate logprob access behind tiers or withhold it for hidden-reasoning models. Treat the equations as load-bearing and re-check the API column of §5 against current provider docs before you depend on it.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Logprobs, Perplexity and KL Divergence/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Logprobs%2C%20Perplexity%20and%20KL%20Divergence/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
