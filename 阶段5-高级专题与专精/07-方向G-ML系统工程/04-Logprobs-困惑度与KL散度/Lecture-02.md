---
title: 第 02 讲 - 熵、交叉熵与负对数似然
description: 第 02 讲 - 熵、交叉熵与负对数似然
---

# 第 02 讲 - 熵、交叉熵与负对数似然

**合集：** [对数概率、困惑度与 KL 散度](README.md) | **上一篇：** [← 第 01 讲](Lecture-01.md) | **下一篇：** [第 03 讲](Lecture-03.md)

---

训练日志里滚动而过的那个数字 —— `loss: 2.041`、`loss: 1.998`、`loss: 1.973` —— 并不是泛泛的「误差」。它是一个有名字、有单位、有物理下限的具体量：它是数据分布与你的模型之间的**交叉熵**，以 **nats** 为度量单位。第 01 讲给了你原材料 `log q(x_t | x_<t)`，即模型分配给每个 token 的对数概率。本讲要赢得的，是那个把这些 logprobs 聚合成优化器所追逐的单一标量的 loss。

读完本讲，你将能同时用三种方式解读这个标量：数据施加给模型的**期望惊讶度**（交叉熵）；一个永远无法移除的部分（数据自身的**熵**）与一个可以移除的部分（**KL gap**）之和；以及第 03 讲将要汇报的困惑度底下的那个指数。主恒等式 `H(p,q) = H(p) + D_KL(p ‖ q)` 是整门课的脊柱，而本讲正是它不再是一条定义、开始成为 loss 曲线进入平台期时你盯着看的那个东西的地方。

全文使用 nats（自然对数），因为 PyTorch 的 `cross_entropy` 返回的就是它，日志打印的也是它。Bits（`log2`）只在编码解释使其成为自然单位的地方出现；换算是一个常数因子 `1 nat = 1/ln 2 ≈ 1.4427 bits`，每次切换都会明确说明。

---

## 学习目标

读完本讲，你应当能够：

* 将**惊讶度** `−log p(x)` 与**熵** `H(p) = E_p[−log p]` 定义为期望惊讶度，并解释为什么均匀分布使熵最大、尖峰分布使熵最小。
* 定义**交叉熵** `H(p,q) = −Σ p log q`，给出其最优编码解释，并证明 `H(p,q) ≥ H(p)` 恒成立。
* 说明在 next-token 预测中数据分布是 one-hot，因此交叉熵 loss **退化为平均负对数似然** `mean_t[−log q(x_t | x_<t)]` —— 也就是你日志里的那个数字。
* 陈述并解释**主恒等式** `H(p,q) = H(p) + D_KL(p ‖ q)`，并指出熵就是 loss 永远无法低于的**下限**。
* 把以 nats 计的 loss 换算成困惑度，并从不可约误差与可消除误差的角度解读训练/评测曲线。

---

## 1. 惊讶度与熵

从单个事件说起。若某个结果 `x` 的概率为 `p(x)`，其**惊讶度**（或称*信息量*）为

```text
  surprise(x) = −log p(x)
```

这是唯一一个（在对数的底数意义下）对独立事件可加、且随概率单调递减的函数：确定事件（`p = 1`）的惊讶度为零，不可能事件的惊讶度为无穷，而两个独立事件带来的惊讶度等于各自惊讶度之和（因为 `−log(p·p') = −log p − log p'`）。对数的底数决定单位：`log2` 给出 **bits**，自然 `log` 给出 **nats**。

**熵**是分布的*期望*惊讶度 —— 即从 `p` 中抽样时，平均而言你应当预期有多惊讶：

```text
  H(p) = E_p[−log p] = − Σ_x p(x) log p(x)
```

它是信源的**不可约不确定性**：每样本平均接收到的信息量，也是任何编码每符号最少能用多少 nats 的下限（Shannon 信源编码定理）。两个极端情形可以锚定直觉：

* **均匀分布使熵最大。** 当有 `N` 个等概率结果时 `p(x) = 1/N`，故 `H = −Σ (1/N) log(1/N) = log N`。没有什么比「所有结果等概率」更不确定；这是 `N` 个符号上任何分布所能具有的最大熵。
* **尖峰分布的熵很低。** 若某个结果的概率接近 1、其余接近 0，几乎每次抽样都是预期结果，平均惊讶度便接近 0。确定性的信源（某个符号上 `p = 1`）有 `H = 0`。

一个手算的小例子可以固定单位。取 4 符号字母表 `{a, b, c, d}`：

| 分布 | p(a) | p(b) | p(c) | p(d) | H (nats) | H (bits) |
|---|---|---|---|---|---|---|
| 均匀 | 0.25 | 0.25 | 0.25 | 0.25 | `ln 4 ≈ 1.386` | `log2 4 = 2.000` |
| 尖峰 | 0.97 | 0.01 | 0.01 | 0.01 | `≈ 0.168` | `≈ 0.242` |
| 确定性 | 1.00 | 0.00 | 0.00 | 0.00 | `0.000` | `0.000` |

对均匀那一行，每个符号携带 `−ln 0.25 = 1.386` nats 的惊讶度，且因为它们等概率，期望值同为 `1.386` nats —— 恰好是 `log N`。对尖峰那一行，占主导的符号只携带 `−ln 0.97 ≈ 0.030` nats，却有 97% 的概率被抽到，把平均值拉低到 `0.168`。以 bits 来读，均匀分布需要整整 2 bits/symbol；尖峰分布只需要它的四分之一。**熵回答的是「这个信源每符号从根本上要花多少 nats」** —— 任何模型，无论多好，都不可能用更少的量来描述它。

---


<details>
<summary>English original</summary>

**Lecture 02 - Entropy, Cross-Entropy & Negative Log-Likelihood**

**Collection:** [Logprobs, Perplexity & KL Divergence](README.md) | **Previous:** [← Lecture 01](Lecture-01.md) | **Next:** [Lecture 03](Lecture-03.md)

---

The number scrolling past in your training logs — `loss: 2.041`, `loss: 1.998`, `loss: 1.973` — is not a generic "error." It is a specific quantity with a name, a unit, and a physical floor: it is the **cross-entropy** between the data distribution and your model, measured in **nats**. Lecture 01 gave you the raw material, `log q(x_t | x_<t)`, the log-probability the model assigns to each token. This lecture earns the loss that aggregates those logprobs into the single scalar your optimizer chases.

By the end you will be able to read that scalar three ways at once: as the **expected surprise** the data inflicts on your model (cross-entropy), as the sum of a part you can never remove (the data's own **entropy**) and a part you can (the **KL gap**), and as the exponent under the perplexity you will report in Lecture 03. The master identity `H(p,q) = H(p) + D_KL(p ‖ q)` is the spine of this entire course, and this lecture is where it stops being a definition and starts being the thing you stare at when a loss curve plateaus.

We work in nats (natural log) throughout, because that is what PyTorch's `cross_entropy` returns and what your logs print. Bits (`log2`) appear only where the coding interpretation makes them the natural unit; the conversion is a constant factor `1 nat = 1/ln 2 ≈ 1.4427 bits`, and we will be explicit every time we switch.

---

**Learning objectives**

By the end of this lecture you should be able to:

* Define **surprise** `−log p(x)` and **entropy** `H(p) = E_p[−log p]` as expected surprise, and explain why the uniform distribution maximizes entropy and a peaked distribution minimizes it.
* Define **cross-entropy** `H(p,q) = −Σ p log q`, give its optimal-coding interpretation, and prove that `H(p,q) ≥ H(p)` always.
* Show that for next-token prediction the data distribution is one-hot, so the cross-entropy loss **collapses to the mean negative log-likelihood** `mean_t[−log q(x_t | x_<t)]` — the number in your logs.
* State and explain the **master identity** `H(p,q) = H(p) + D_KL(p ‖ q)`, and identify entropy as the **floor** the loss can never beat.
* Convert a loss in nats to perplexity, and read a training/eval curve in terms of irreducible vs. closable error.

---

**1. Surprise and entropy**

Start with a single event. If an outcome `x` has probability `p(x)`, its **surprise** (or *information content*) is

```text
  surprise(x) = −log p(x)
```

This is the only function (up to the base of the log) that is additive over independent events and monotone decreasing in probability: a certain event (`p = 1`) carries zero surprise, an impossible one carries infinite surprise, and two independent events surprise you by the sum of their individual surprises (because `−log(p·p') = −log p − log p'`). The base of the log sets the unit: `log2` gives **bits**, natural `log` gives **nats**.

**Entropy** is the *expected* surprise of a distribution — how surprised you should expect to be, on average, by a draw from `p`:

```text
  H(p) = E_p[−log p] = − Σ_x p(x) log p(x)
```

It is the **irreducible uncertainty** of the source: the average information you receive per sample, and the floor on how few nats per symbol any code can use (Shannon's source-coding theorem). Two limiting cases anchor the intuition:

* **Uniform distribution maximizes entropy.** With `N` equally likely outcomes, `p(x) = 1/N`, so `H = −Σ (1/N) log(1/N) = log N`. Nothing is more uncertain than "all outcomes equally likely"; this is the maximum entropy any distribution over `N` symbols can have.
* **A peaked distribution has low entropy.** If one outcome has probability near 1 and the rest near 0, almost every draw is the expected one, so the average surprise is near 0. A deterministic source (`p = 1` on one symbol) has `H = 0`.

A worked tiny example fixes the units. Take a 4-symbol alphabet `{a, b, c, d}`:

| Distribution | p(a) | p(b) | p(c) | p(d) | H (nats) | H (bits) |
|---|---|---|---|---|---|---|
| Uniform | 0.25 | 0.25 | 0.25 | 0.25 | `ln 4 ≈ 1.386` | `log2 4 = 2.000` |
| Peaked | 0.97 | 0.01 | 0.01 | 0.01 | `≈ 0.168` | `≈ 0.242` |
| Deterministic | 1.00 | 0.00 | 0.00 | 0.00 | `0.000` | `0.000` |

For the uniform row, every symbol carries `−ln 0.25 = 1.386` nats of surprise, and since they are equally likely the expectation is the same `1.386` nats — exactly `log N`. For the peaked row, the dominant symbol carries only `−ln 0.97 ≈ 0.030` nats and is drawn 97% of the time, dragging the average down to `0.168`. Read in bits, the uniform distribution needs a full 2 bits/symbol; the peaked one needs a quarter of that. **Entropy is the answer to "how many nats per symbol does this source fundamentally cost?"** — and no model, however good, can describe it for less.

---

</details>

## 2. 交叉熵

熵假设你知道真实分布 `p` 并为它构造了最优编码。**交叉熵**问的是一个更现实的问题：你为*你的模型* `q` 构造了编码，但符号实际上来自 `p`。现在你每个符号要付出多少 nats？

```text
  H(p, q) = E_p[−log q] = − Σ_x p(x) log q(x)
```


**编码解释。** 对 `q` 的最优前缀码给符号 `x` 分配一个长度为 `−log q(x)` nats 的码字（这就是概率为 `q` 的信源的 Kraft–Shannon 最优长度）。如果符号真的来自 `q`，期望长度会是 `H(q)`。但它们来自 `p`，所以期望描述长度是 `Σ_x p(x) · [−log q(x)] = H(p, q)`。你是在为一个调校到*错误*分布的编码付费。凡是 `q` 低估了 `p` 视为常见的符号之处，你就得经常付出长码字；凡是 `q` 高估了稀有符号之处，你就把短码字浪费在极少发生的事件上。

**为什么 `H(p, q) ≥ H(p)` 总是成立。** 交叉熵永远不可能优于熵：最好的编码就是与真实分布相匹配的那个。缺口恰好是 KL 散度（第 04 讲通过 Gibbs 不等式证明 `H(p,q) − H(p) = D_KL(p ‖ q) ≥ 0`），但这个界本身是直观的——你无法用比信源自身熵更少的 nats 来描述一个信源，所以为任何 `q ≠ p` 编码只会付出更多。等式成立**当且仅当**在 `p` 有支撑的每一处都成立 `q = p`。正是这一不等式使训练存在下界，§4 会把它精确化。

一个具体的解读：让 `p` 在 `{a,b,c,d}` 上均匀分布（`H(p) = 1.386` nats），但假设模型 `q` 自信地错了，即 `q = (0.7, 0.1, 0.1, 0.1)`。那么

```text
  H(p, q) = −Σ p log q
          = −0.25 (ln 0.7 + ln 0.1 + ln 0.1 + ln 0.1)
          = −0.25 (−0.357 − 2.303 − 2.303 − 2.303)
          ≈ 1.816 nats
```


你付出的是 `1.816` nats，而不是 `1.386` 的下界——净浪费 `0.430` nats，这恰好就是 `D_KL(p ‖ q)`。模型对 `a` 的过度自信，会在 `b`、`c` 或 `d` 真正出现时让你付出代价。

---


<details>
<summary>English original</summary>

**2. Cross-entropy**

Entropy assumes you know the true distribution `p` and built the optimal code for it. **Cross-entropy** asks the realistic question: you built your code for *your model* `q`, but the symbols actually arrive from `p`. How many nats per symbol do you pay now?

```text
  H(p, q) = E_p[−log q] = − Σ_x p(x) log q(x)
```

**The coding interpretation.** An optimal prefix code for `q` assigns symbol `x` a codeword of length `−log q(x)` nats (this is the Kraft–Shannon optimal length for a source whose probabilities are `q`). If symbols truly came from `q`, the expected length would be `H(q)`. But they come from `p`, so the expected description length is `Σ_x p(x) · [−log q(x)] = H(p, q)`. You are paying for a code tuned to the *wrong* distribution. Every place where `q` underestimates a symbol that `p` makes common, you pay a long codeword often; every place `q` overestimates a rare symbol, you waste short codewords on events that rarely happen.

**Why `H(p, q) ≥ H(p)` always.** The cross-entropy can never beat the entropy: the best possible code is the one matched to the true distribution. The shortfall is exactly the KL divergence (Lecture 04 proves `H(p,q) − H(p) = D_KL(p ‖ q) ≥ 0` via Gibbs' inequality), but the bound itself is intuitive — you cannot describe a source for fewer nats than its own entropy, so coding for any `q ≠ p` can only cost more. Equality holds **iff** `q = p` everywhere `p` has support. This single inequality is why training has a floor, which §4 makes precise.

A concrete reading: keep `p` uniform over `{a,b,c,d}` (`H(p) = 1.386` nats) but suppose the model `q` is confidently wrong, `q = (0.7, 0.1, 0.1, 0.1)`. Then

```text
  H(p, q) = −Σ p log q
          = −0.25 (ln 0.7 + ln 0.1 + ln 0.1 + ln 0.1)
          = −0.25 (−0.357 − 2.303 − 2.303 − 2.303)
          ≈ 1.816 nats
```

You pay `1.816` nats instead of the `1.386` floor — `0.430` nats of pure waste, which is precisely `D_KL(p ‖ q)`. The model's overconfidence in `a` costs you every time `b`, `c`, or `d` actually shows up.

---

</details>

## 3. 训练损失就是交叉熵

下面这一步，让本讲对系统工作有了实际意义。在语言建模中，从来不存在词表上的 soft 数据分布 `p`；对给定位置 `t`，数据集只是*包含*一个真实的下一 token，记为 `y_t`。因此该位置上的经验数据分布是一个 **one-hot**：`p(y_t) = 1`，而对其他每一个 token `x` 则为 `p(x) = 0`。

把一个 one-hot `p` 代入交叉熵，求和随之坍缩 —— 除真实 token 处的那一项外，每一项都被乘以零：

```text
  H(p, q) = − Σ_x p(x) log q(x)
          = − 1 · log q(y_t)              ← only the true-token term survives
          = − log q(y_t | x_<t)           ← the NLL of the true token
```

所以在单个位置上，交叉熵**就是**实际下一 token 的负对数似然 —— 仅此而已。对批中每个位置取平均，就得到优化器所最小化的那个标量：

```text
  loss = mean_t [ − log q(y_t | x_<t) ]   = empirical cross-entropy = mean NLL
```

这就是为什么 “交叉熵损失”“负对数似然” 和 “语言建模损失” 是同一个数值的三个名字。（注意 one-hot 是*经验* `p`；某个位置上的*真实* `p` 通常是 soft 的 —— 许多 token 都可能合理地接下去 —— 而这一差距正是 §4 中的熵所刻画的。Lecture 05 的蒸馏则用一个 teacher 的 soft `p` 替换该 one-hot。）

**Teacher forcing 与移位标签。** 训练时，模型在预测位置 `t` 时以*真实*前缀 `x_<t` 为条件，而不是以它自己过去的采样为条件 —— 这就是 **teacher forcing**。从机制上说，它意味着目标就是输入**偏移一位**：`logits[:, :-1]` 预测 `labels[:, 1:]`。padding 与 prompt token 通过 ignore index 排除，因此它们对均值的贡献为零。每个位置都并行地与实际紧随其后的那一个 token 打分。

下面这个 PyTorch 恒等式就是 §3 的代码形态 —— 对 logits 做 `F.cross_entropy` *恰好*等于在真实 token 处收集到的 `−log_softmax` 的均值：

```python
import torch
import torch.nn.functional as F

torch.manual_seed(0)
B, T, V = 2, 5, 32000            # batch, seq, vocab
logits = torch.randn(B, T, V)
labels = torch.randint(0, V, (B, T))

# --- Shifted labels (teacher forcing): predict token t+1 from tokens <= t ---
shift_logits = logits[:, :-1, :].reshape(-1, V)   # [(B*(T-1)), V]
shift_labels = labels[:, 1:].reshape(-1)          # [(B*(T-1))]

# Path A: the library's fused cross-entropy (mean reduction, in nats)
loss_builtin = F.cross_entropy(shift_logits, shift_labels)

# Path B: cross-entropy spelled out = mean of gathered negative log-softmax
logq = F.log_softmax(shift_logits, dim=-1)                 # log q over vocab
nll  = -logq.gather(1, shift_labels[:, None]).squeeze(1)   # -log q(true token)
loss_manual = nll.mean()

print(loss_builtin.item(), loss_manual.item())   # identical to fp precision
assert torch.allclose(loss_builtin, loss_manual, atol=1e-6)
```

`F.cross_entropy` 内部融合了 `log_softmax` + `nll_loss`；它从不要求你先做 softmax（先 softmax 再取 `log` 在数值上更差 —— 见 Lecture 01 的 log-sum-exp）。两条路径在浮点精度内一致，因为它们*就是*同一个计算。

> **硬件视角：** 计算这个损失会实体化一个 `[batch × seq × vocab]` 的 logit 张量，以及一个同样大的 `log_softmax` —— 对 256k 词表、`batch·seq = 8192` 的模型，仅 logits *一项*就是 `8192 × 256000 × 2 bytes ≈ 4.2 GB`（以 `bf16` 计），往往是整个训练步中最大的单个激活值，也是激活内存与 HBM 带宽的主要来源。解决办法是永不实体化完整张量：融合的 / **分块交叉熵** kernel（Liger-Kernel 的 fused linear+CE、**cut-cross-entropy**、FlashCE）在词表上逐块计算损失及其梯度，只保留滚动归约结果。128k–256k 的词表（Llama 3、Gemma、Qwen）使这成为一阶成本，而不是一个脚注。

---


<details>
<summary>English original</summary>

**3. The training loss IS cross-entropy**

Here is the move that makes this lecture matter for systems work. In language modeling we never have a soft data distribution `p` over the vocabulary; for a given position `t`, the dataset simply *contains* one actual next token, call it `y_t`. The empirical data distribution at that position is therefore a **one-hot**: `p(y_t) = 1` and `p(x) = 0` for every other token `x`.

Plug a one-hot `p` into cross-entropy and the sum collapses — every term is multiplied by zero except the one at the true token:

```text
  H(p, q) = − Σ_x p(x) log q(x)
          = − 1 · log q(y_t)              ← only the true-token term survives
          = − log q(y_t | x_<t)           ← the NLL of the true token
```

So at a single position the cross-entropy **is** the negative log-likelihood of the actual next token — nothing more. Averaging over every position in the batch gives the scalar your optimizer minimizes:

```text
  loss = mean_t [ − log q(y_t | x_<t) ]   = empirical cross-entropy = mean NLL
```

This is why "cross-entropy loss," "negative log-likelihood," and "the language-modeling loss" are three names for the same number. (Note the one-hot is the *empirical* `p`; the *true* `p` for a position is generally soft — many tokens could plausibly continue — and that gap is what entropy in §4 captures. Distillation, Lecture 05, replaces the one-hot with a teacher's soft `p`.)

**Teacher forcing and shifted labels.** During training the model predicts position `t` while conditioning on the *ground-truth* prefix `x_<t` rather than its own past samples — this is **teacher forcing**. Mechanically it means the targets are the inputs **shifted by one**: `logits[:, :-1]` predicts `labels[:, 1:]`. Padding and prompt tokens are excluded with an ignore index so they contribute zero to the mean. Every position is scored in parallel against the single token that actually followed.

The PyTorch identity below is the whole §3 in code — `F.cross_entropy` on logits is *exactly* the mean of the gathered `−log_softmax` at the true tokens:

```python
import torch
import torch.nn.functional as F

torch.manual_seed(0)
B, T, V = 2, 5, 32000            # batch, seq, vocab
logits = torch.randn(B, T, V)
labels = torch.randint(0, V, (B, T))

# --- Shifted labels (teacher forcing): predict token t+1 from tokens <= t ---
shift_logits = logits[:, :-1, :].reshape(-1, V)   # [(B*(T-1)), V]
shift_labels = labels[:, 1:].reshape(-1)          # [(B*(T-1))]

# Path A: the library's fused cross-entropy (mean reduction, in nats)
loss_builtin = F.cross_entropy(shift_logits, shift_labels)

# Path B: cross-entropy spelled out = mean of gathered negative log-softmax
logq = F.log_softmax(shift_logits, dim=-1)                 # log q over vocab
nll  = -logq.gather(1, shift_labels[:, None]).squeeze(1)   # -log q(true token)
loss_manual = nll.mean()

print(loss_builtin.item(), loss_manual.item())   # identical to fp precision
assert torch.allclose(loss_builtin, loss_manual, atol=1e-6)
```

`F.cross_entropy` internally fuses `log_softmax` + `nll_loss`; it never asks you to softmax first (doing so and then taking `log` is numerically worse — see Lecture 01's log-sum-exp). The two paths agree to floating-point precision because they *are* the same computation.

> **Hardware lens:** computing this loss materializes a `[batch × seq × vocab]` logit tensor and an equally large `log_softmax` — for a 256k-vocab model at `batch·seq = 8192`, that is `8192 × 256000 × 2 bytes ≈ 4.2 GB` in `bf16` for the logits *alone*, frequently the single largest activation in the whole training step and a major source of activation memory and HBM bandwidth. The fix is to never materialize the full tensor: fused / **chunked cross-entropy** kernels (Liger-Kernel's fused linear+CE, **cut-cross-entropy**, FlashCE) compute the loss and its gradient block-by-block over the vocabulary, keeping only running reductions. Vocabularies of 128k–256k (Llama 3, Gemma, Qwen) make this a first-order cost, not a footnote.

---

</details>

## 4. 主恒等式

以上所有内容都汇聚到一个等式上。交叉熵恰好分解为熵加上 KL 散度：

```text
  H(p, q)  =  H(p)  +  D_KL(p ‖ q)
   ▲           ▲          ▲
   │           │          └─ the gap: avoidable error the model CAN close
   │           └──────────── irreducible entropy of the data: the FLOOR
   └──────────────────────── the loss you actually train (mean −log q)
```

这个推导只有一行——把 KL 里的 log 拆开，就能认出这两块（`D_KL ≥ 0` 的完整 Gibbs 不等式证明在第 04 讲）：

```text
  D_KL(p ‖ q) = Σ p log(p/q) = Σ p log p − Σ p log q = −H(p) + H(p, q)
  ⇒  H(p, q) = H(p) + D_KL(p ‖ q),     with  D_KL(p ‖ q) ≥ 0.
```

从物理上读：**你的损失是数据自身的熵，再加上你的模型与数据之间的距离。** 训练永远只能移动第二项。无论多少优化、多少规模、多少数据，都无法把损失压到 `H(p)` 以下——那就是**地板**，是语言中真实存在的歧义（"The capital of France is" 后面合法地可以跟很多 token）。当损失曲线变平时，你看着的是 `D_KL(p ‖ q) → 0`，而 `H(p)` 在下面纹丝不动。损失达到 `= H(p)` 的模型会是*完美*的——它会精确地给出数据真实的条件下概率——而 `D_KL(p ‖ q) = 0` 是到达那里的唯一途径。

这就把课程中每一个下游技术都重新表述为针对某一个特定 KL 项的战役。量化会引入一个你希望它小的 `D_KL(p_fp16 ‖ q_int4)`（第 05 讲）；蒸馏直接最小化 `D_KL(p_teacher ‖ q_student)`（第 05 讲）；RLHF 惩罚项约束 `D_KL(q_tuned ‖ q_base)` 以阻止策略漂移。恒等式是同一个；变的只是那两个分布。

---

## 5. 读懂训练 / 评测曲线

你的损失单位是 **nats per token**。最有用的一个条件反射是取它的指数：因为 `PPL = exp(H(p, q)) = exp(mean NLL)`（第 03 讲完整推导），损失和困惑度是同一信息的两种单位。

| Loss（nats） | `PPL = e^loss` | 对 LLM 的非正式解读 |
|---|---|---|
| `0.0` | `1.0` | 完美 / 退化——只有在数据是确定性的时候才可能 |
| `1.0` | `≈ 2.72` | 对开放域文本低得不可信；怀疑有泄漏或数据过于平凡 |
| `2.0` | `≈ 7.39` | 在留出的通用文本上属于强现代 LLM 的水平 |
| `3.0` | `≈ 20.1` | 更小或训练不足的模型 |
| `5.0` | `≈ 148` | 对小词表而言接近随机；有问题 |
| `ln V` | `V` | 在词表上均匀分布——未训练模型的健全性检查 |

损失 **2.0 nats ↔ PPL ≈ 7.4**：模型平均而言的不确定程度，相当于在约 7.4 个等可能的下一个 token 中均匀选择（第 03 讲的"有效分支因子"）。最后一行是你手头最廉价的诊断手段——刚初始化的模型应该打印出损失 `≈ ln V`（例如 `ln 128000 ≈ 11.76`），因为未训练的 softmax 大致是均匀的；如果你的第一步远低于 `ln V`，说明标签在泄漏；远高于它，则 logits 或 loss masking 有问题。

有两条报告约定要分清：

* **按 token 与按序列。** 打印出的损失几乎总是**按 token** 的平均值（`reduction='mean'` 除以 token 数）。*按序列*的求和（`reduction='sum'` 再除以 batch）对序列长度敏感，用于比较不同 `seq_len` 的多次运行时是错误的做法。比较两条曲线之前一定要确认分母；仅仅因为用了更短序列而得到的"更低损失"只是一种产物。
* **"好"是什么样子是相对的。** 绝对损失只有在*固定的 tokenizer 和数据集*下才有意义。tokenizer 不同的两个模型会把同一段文本铺成不同数量的 token，因此它们的按 token 损失不可比——解决办法是 **bits-per-byte**，即归一化到底层的 UTF-8 字节（第 03 讲）。在此之前，把原始交叉熵视为仅*在同一 tokenizer 家族内*可比。

> **2026 更新：** 融合 / 分块的交叉熵如今已是大词表训练的默认路径，而不是你要费劲去找的优化手段——Liger-Kernel 的 fused-linear-cross-entropy 和 **cut-cross-entropy**（它从头到尾都不必把 `[tokens × vocab]` 的 logit 矩阵具象化）已进入主流训练栈，在 128k–256k 词表下通常能把峰值激活内存削掉数 GB，并使损失张量不再是序列长度的瓶颈。在报告方面，**bits-per-byte**（把损失转成 `log2` 再除以每 token 字节数）已成为与 tokenizer 无关的比较的首选指标；原始按 token 的 nats 越来越被视为内部训练信号，而非跨模型指标（前向引用第 03 讲）。

---


<details>
<summary>English original</summary>

**4. The master identity**

Everything above converges on one equation. Cross-entropy decomposes exactly into entropy plus KL divergence:

```text
  H(p, q)  =  H(p)  +  D_KL(p ‖ q)
   ▲           ▲          ▲
   │           │          └─ the gap: avoidable error the model CAN close
   │           └──────────── irreducible entropy of the data: the FLOOR
   └──────────────────────── the loss you actually train (mean −log q)
```

The algebra is one line — split the log inside KL and recognize the two pieces (the full Gibbs-inequality proof that `D_KL ≥ 0` is Lecture 04):

```text
  D_KL(p ‖ q) = Σ p log(p/q) = Σ p log p − Σ p log q = −H(p) + H(p, q)
  ⇒  H(p, q) = H(p) + D_KL(p ‖ q),     with  D_KL(p ‖ q) ≥ 0.
```

Read physically: **your loss is the data's own entropy plus the distance from your model to the data.** Training can only ever move the second term. No amount of optimization, scale, or data drives the loss below `H(p)` — that is the **floor**, the genuine ambiguity in language (many tokens can legitimately follow "The capital of France is"). When a loss curve flattens, you are watching `D_KL(p ‖ q) → 0` while `H(p)` sits underneath unmoved. A model that reached loss `= H(p)` would be *perfect* — it would assign exactly the data's true conditional probabilities — and `D_KL(p ‖ q) = 0` is the only way to get there.

This reframes every downstream technique in the course as a campaign against one specific KL term. Quantization adds a `D_KL(p_fp16 ‖ q_int4)` you want small (Lecture 05); distillation minimizes `D_KL(p_teacher ‖ q_student)` directly (Lecture 05); the RLHF penalty bounds `D_KL(q_tuned ‖ q_base)` to stop the policy drifting. The identity is the same; only the two distributions change.

---

**5. Reading a training / eval curve**

Your loss is in **nats per token**. The single most useful reflex is to exponentiate it: because `PPL = exp(H(p, q)) = exp(mean NLL)` (derived in full in Lecture 03), the loss and the perplexity are the same information in two units.

| Loss (nats) | `PPL = e^loss` | Informal reading for an LLM |
|---|---|---|
| `0.0` | `1.0` | perfect / degenerate — only if the data were deterministic |
| `1.0` | `≈ 2.72` | implausibly low for open-domain text; suspect a leak or trivial data |
| `2.0` | `≈ 7.39` | strong modern LLM territory on held-out general text |
| `3.0` | `≈ 20.1` | a smaller or undertrained model |
| `5.0` | `≈ 148` | near-random for a small vocab; something is wrong |
| `ln V` | `V` | uniform over the vocab — the untrained-model sanity check |

A loss of **2.0 nats ↔ PPL ≈ 7.4**: the model is, on average, as uncertain as if it were choosing uniformly among ~7.4 equally likely next tokens (the "effective branching factor" of Lecture 03). The last row is the cheapest diagnostic you have — a freshly initialized model should print loss `≈ ln V` (e.g. `ln 128000 ≈ 11.76`), because an untrained softmax is roughly uniform; if your first step is far below `ln V`, your labels are leaking; far above it and your logits or loss masking are broken.

Two reporting conventions to keep straight:

* **Per-token vs. per-sequence.** The printed loss is almost always the **per-token** mean (`reduction='mean'` divides by the token count). A *per-sequence* sum (`reduction='sum'` then divide by batch) is sensitive to sequence length and is the wrong thing to compare across runs with different `seq_len`. Always confirm the denominator before comparing two curves; a "lower loss" that merely used shorter sequences is an artifact.
* **What "good" looks like is relative.** Absolute loss is only meaningful against a *fixed tokenizer and dataset*. Two models with different tokenizers spread the same text across different numbers of tokens, so their per-token losses are not comparable — the fix is **bits-per-byte**, normalizing to the underlying UTF-8 bytes (Lecture 03). Until then, treat raw cross-entropy as comparable only *within* a tokenizer family.

> **2026 update:** fused / chunked cross-entropy is now the default path for large-vocab training rather than an optimization you reach for — Liger-Kernel's fused-linear-cross-entropy and **cut-cross-entropy** (which avoids ever materializing the `[tokens × vocab]` logit matrix) ship in mainstream training stacks, routinely cutting peak activation memory by multiple GB at 128k–256k vocab and removing the loss tensor as a sequence-length bottleneck. On the reporting side, **bits-per-byte** (loss converted to `log2` and divided by bytes per token) has become the preferred headline for tokenizer-independent comparison; raw per-token nats are increasingly treated as an internal training signal rather than a cross-model metric (forward-ref Lecture 03).

---

</details>

## 更新日期

2026 年 6 月。本讲义的数学部分是恒久的：surprise、熵、交叉熵、主恒等式 `H(p,q) = H(p) + D_KL(p ‖ q)`，以及交叉熵在 one-hot 目标下坍缩为平均 NLL —— 这些都是 Shannon-1948 的结果，任何框架的修订都不会触动；自第一个神经语言模型以来，你日志里的 loss 含义一直就是如此。会变的是**工具链**：哪个 fused/chunked 交叉熵 kernel 最快（Liger-Kernel、cut-cross-entropy、FlashCE 及其后继者），fused-linear+CE 的边界在给定框架中落在哪里，以及团队是以 nats、bits-per-token 还是 bits-per-byte 来作为其数字的主打口径。把恒等式当作基岩，把 kernel 名称和汇报单位当作每次刷新时需要重新核对的部分。


<details>
<summary>English original</summary>

**Current as of**

June 2026. The mathematics of this lecture is permanent: surprise, entropy, cross-entropy, the master identity `H(p,q) = H(p) + D_KL(p ‖ q)`, and the collapse of cross-entropy to mean NLL under a one-hot target are Shannon-1948 results that no framework revision will touch — the loss in your logs has meant exactly this since the first neural language model. What moves is the **tooling**: which fused/chunked cross-entropy kernel is fastest (Liger-Kernel, cut-cross-entropy, FlashCE and their successors), where the fused-linear+CE boundary lands in a given framework, and whether teams headline their numbers in nats, bits-per-token, or bits-per-byte. Treat the identity as bedrock and the kernel names and reporting unit as the parts to re-check on each refresh.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Logprobs, Perplexity and KL Divergence/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Logprobs%2C%20Perplexity%20and%20KL%20Divergence/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
