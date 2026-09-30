---
title: 第 05 讲 - 一切归于此：量化、蒸馏、RLHF 与投机解码
description: 第 05 讲 - 一切归于此：量化、蒸馏、RLHF 与投机解码
---

# 第 05 讲 - 一切归于此：量化、蒸馏、RLHF 与投机解码

**合集：** [对数概率、困惑度与 KL 散度](README.md) | **上一讲：** [← 第 04 讲](Lecture-04.md) | **下一讲：** [课程索引](README.md)

---

前四讲构建出了同一个等式。第 02 讲挣得了 `H(p, q) = H(p) + D_KL(p ‖ q)`；第 03 讲把它包进 `PPL = exp(H(p, q))`；第 04 讲证明了第三项是 `D_KL(p ‖ q) = E_p[log p − log q] ≥ 0`，不对称，是本可避免的浪费。本讲把这个恒等式放开了用。这个论断很强，值得直白说出：**每一种让模型更便宜、更小、更安全或更快的重大系统技术，都是一场把某一个特定 KL 压小的战役**——或者说，当这个 KL 被*构造性地固定*时，把它读作那道决定更便宜的模型能否出货的闸门。

把这一众角色过一遍。量化评分问的是 *INT4 偏离 FP16 有多远*——那就是 `D_KL(p_fp16 ‖ q_quant)`，llama.cpp 会替你把它打印出来。知识蒸馏*训练*一个学生模型在某个温度下最小化 `D_KL(p_teacher ‖ q_student)`。RLHF 不只是最大化奖励——它最大化奖励**减去**一条 KL 缰绳 `β·D_KL(π_θ ‖ π_ref)`，这条缰绳阻止策略跑偏、陷入奖励劫持的胡言乱语，而这条缰绳从 PPO 到 DPO 到 GRPO 原封不动地延续下来。投机解码看起来不一样——它可证明是*无损*的，KL 见鬼去吧——但接受规则是一次对数概率比的检验，而它带来的加速恰恰取决于 draft 到 target 的差距有多小。一个等式，四台机器。

对资深工程师而言，贯穿始终的主线是：**`p` 永远是你信任的分布，`q` 永远是你负担得起的分布。**数据、教师、FP16 参考、参考策略、目标模型——这些是 `p`。学生、量化模型、调优后的策略、草稿模型——这些是 `q`。读下面每一节时都问一句：*哪个是 `p`，哪个是 `q`，这个 KL 是在被最小化还是在被测量？*

---

## 学习目标

本讲结束时，你应当能够：

1. 针对这四种技术中的每一种，说清**哪个分布是 `p`，哪个是 `q`，以及该 KL 是在被最小化（一个训练目标）还是在被测量（一道质量闸门）**。
2. 像 llama.cpp 的 `--kl-divergence` 模式那样为一次量化评分——**均值/中位数 `D_KL`、token 概率 RMS、top-1/top-k 一致率以及 PPL 比值**——并解释为什么单看 PPL 太“粗糙”。
3. 引用**2026 年的发现**：量化的*格式*比名义位宽更重要，且内在指标是必要但**不充分**的，并把它转化为一条出货/不出货的规则。
4. 把知识蒸馏推导为**温度 `T` 下的 forward-KL 匹配**，解释“暗知识”与 `T²` 梯度缩放，并说明为什么 forward KL 让学生模型*模式覆盖*（mode-covering）。
5. 说明 **RLHF 的 KL 惩罚项是不变量**，贯穿 PPO → DPO → GRPO，并说明 DPO 的闭式解正是 KL 正则化奖励目标的精确最优解。
6. 证明**投机解码中被接受的样本，其分布与 target `p` 完全一致**，并把接受长度与 draft–target 的 KL 差距联系起来。
7. 构建一个**收官量化评分器**：在 WikiText-2 上对比 FP16 参考与量化模型，报告完整的指标面板，外加一个带阈值的 `verdict()`。

---


<details>
<summary>English original</summary>

**Lecture 05 - Where It All Lands: Quantization, Distillation, RLHF & Speculative Decoding**

**Collection:** [Logprobs, Perplexity & KL Divergence](README.md) | **Previous:** [← Lecture 04](Lecture-04.md) | **Next:** [Course index](README.md)

---

Four lectures built one equation. Lecture 02 earned `H(p, q) = H(p) + D_KL(p ‖ q)`; Lecture 03 wrapped it in `PPL = exp(H(p, q))`; Lecture 04 proved the third term is `D_KL(p ‖ q) = E_p[log p − log q] ≥ 0`, asymmetric, the avoidable waste. This lecture turns the identity loose. The claim is strong and worth stating plainly: **every major systems technique for making a model cheaper, smaller, safer, or faster is a campaign to shrink one specific KL** — or, when the KL is held *fixed by construction*, to read it as the gate that decides whether the cheaper model ships.

Walk the cast. Quantization grading asks *how far did INT4 drift from FP16* — that is `D_KL(p_fp16 ‖ q_quant)`, and llama.cpp will print it for you. Knowledge distillation *trains* a student to minimize `D_KL(p_teacher ‖ q_student)` at temperature. RLHF doesn't just maximize reward — it maximizes reward **minus** a KL leash `β·D_KL(π_θ ‖ π_ref)` that stops the policy from wandering off into reward-hacked gibberish, and that same leash survives unchanged from PPO to DPO to GRPO. Speculative decoding looks different — it is provably *lossless*, KL be damned — but the acceptance rule is a log-probability-ratio test, and the speedup it delivers is governed by exactly how small the draft-to-target gap is. One equation, four machines.

The throughline for a senior engineer: **`p` is always the distribution you trust, `q` is always the distribution you can afford.** Data, teacher, FP16 reference, reference policy, target model — those are `p`. Student, quant, tuned policy, draft model — those are `q`. Read every section below by asking *which `p`, which `q`, and is the KL being minimized or measured?*

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. State, for each of the four techniques, **which distribution is `p`, which is `q`, and whether the KL is being minimized (a training objective) or measured (a quality gate)**.
2. Grade a quantization the way llama.cpp's `--kl-divergence` mode does — **mean/median `D_KL`, token-probability RMS, top-1/top-k agreement, and PPL ratio** — and explain why PPL alone is "rough."
3. Cite the **2026 finding** that quantization *format* matters more than nominal bit-width and that intrinsic metrics are necessary but **not sufficient**, and translate it into a ship/no-ship rule.
4. Derive knowledge distillation as **forward-KL matching at temperature `T`**, explain "dark knowledge" and the `T²` gradient scaling, and say why forward KL makes the student *mode-covering*.
5. Show that the **RLHF KL penalty is the invariant** across PPO → DPO → GRPO, and that DPO's closed form is the exact optimum of the KL-regularized reward objective.
6. Prove that **speculative decoding's accepted samples are distributed exactly as the target `p`**, and relate acceptance length to the draft-target KL gap.
7. Build a **capstone quant grader**: FP16 reference vs quantized model over WikiText-2, reporting the full metric panel plus a thresholded `verdict()`.

---

</details>

## 1. 作为工具包的恒等式

整个课程归结为一行，你应当能盲写出来：

```text
   H(p, q)   =   H(p)   +   D_KL(p ‖ q)          and          PPL = exp(H(p, q))
   ───────       ────       ────────────
   what q pays   the floor   the avoidable waste — what every
   per token     (p's own    technique below either MINIMIZES
   (mean −log q) entropy)    (training) or MEASURES (a gate)
```

`p` 是参考/可信的（数据、教师、FP16、参考策略、目标模型）；`q` 是近似（学生、quant、调优策略、草稿模型）。该恒等式在这里很重要，因为它告诉你 *你实际在移动什么*。当 quant 的困惑度上升时，该恒等式表明增加 **完全** 在 `D_KL(p_fp16 ‖ q_quant)` 中 —— `H(p)` 是数据的属性，无法改变。因此，困惑度差值和 KL 是 *同一种测量* 的两种解读方式；唯一的问题是哪一个与用户注意到的内容相关性更好。（来自第 2 节的剧透：KL 和 top-1 一致率确实如此。）

这是本讲座剩余部分的概览 —— 四种技术，各占一列：

| 技术 | `p` (可信) | `q` (廉价) | KL | 最小化还是测量？ |
|-----------|---------------|-------------|--------|------------------------|
| 量化评分 | FP16 logits | INT4/INT8 logits | `D_KL(p_fp16 ‖ q_quant)` | **测量**（一个门控） |
| 知识蒸馏 | 教师 | 学生 | `D_KL(p_teacher ‖ q_student)` 在 `T` | **最小化**（损失） |
| RLHF / 对齐 | 参考策略 `π_ref` | 调优策略 `π_θ` | `D_KL(π_θ ‖ π_ref)` | **约束**（一条拴绳） |
| 投机解码 | 目标模型 `p` | 草稿模型 `q` | `p`/`q` 差距（类 KL） | **两者都不是** —— 无损；差距决定 *速度* |

> **硬件视角：** 本讲座中的每个指标都是一个 **在东西发布之前的门控** —— 一个 quant、一个蒸馏学生、一个调优策略、一个草稿头。关键的是，计算这个门控是一个 **前向传播**，而不是一次训练运行：你将权重在评测集上流式传输一次，并从输出中读取对数概率。这比完整下游 benchmark 套件 *几个数量级* 便宜，这就是为什么这些内在数值是第一道过滤器 —— 它们在你在 MMLU/GSM8K/IFEval 上花费 GPU 天之前，就淘汰掉明显损坏的候选者。

---

## 2. 量化评分 —— 最深的门控

量化用更低精度的格式（INT8、INT4、llama.cpp 中的 K-quants 和 I-quants）替换 FP16 权重，以缩小内存占用和带宽 —— 这是带宽受限的 decode（逐 token 生成阶段）中的主要成本。每次的问题都是：**它是破坏了模型，还是仅仅使它轻微受损？** 直觉是在两者上计算困惑度并比较。这有效，但它 *粗糙*，而理解 *为什么* 是本课程的核心。

### 2.1 为什么困惑度比率只是一个代理

困惑度是一个 **在整个语料库上平均的单个标量**。它将每个位置上的整个下一个 token 分布压缩成一个数字（在 *留出 token* 上的平均 NLL 的 `exp`）。两个失效模式隐藏在几乎相等的困惑度中：

- quant 可能 **平均正确但尾部错误** —— 它让 top token 的概率大致正确（因此实际下一个 token 的 NLL 几乎不变），同时严重破坏分布的 *其余部分*。困惑度从不关注它放在未出现 token 上的概率质量。
- quant 可能 **在不同位置交换相互抵消的错误** —— 这里过度自信，那里自信不足 —— 均值未变，而每个 token 的分布明显退化。

KL 散度不会以这种方式压缩：`D_KL(p_fp16 ‖ q_quant)` 是在 **每个 token 的完整分布** 上计算的，然后取平均。它能看到尾部。这就是 llama.cpp 添加 KL 模式的全部原因。


<details>
<summary>English original</summary>

**1. The identity as a toolkit**

The whole course collapses to one line you should be able to write blind:

```text
   H(p, q)   =   H(p)   +   D_KL(p ‖ q)          and          PPL = exp(H(p, q))
   ───────       ────       ────────────
   what q pays   the floor   the avoidable waste — what every
   per token     (p's own    technique below either MINIMIZES
   (mean −log q) entropy)    (training) or MEASURES (a gate)
```

`p` is reference/trusted (data, teacher, FP16, reference policy, target model); `q` is the approximation (student, quant, tuned policy, draft model). The identity matters here because it tells you *what you are actually moving*. When a quant's PPL rises, the identity says the increase is **entirely** in `D_KL(p_fp16 ‖ q_quant)` — `H(p)` is a property of the data and cannot change. So a PPL delta and a KL are the *same measurement* read two ways; the only question is which one correlates better with what a user notices. (Spoiler from §2: the KL and top-1 agreement do.)

Here is the map for the rest of the lecture — four techniques, one column each:

| Technique | `p` (trusted) | `q` (cheap) | The KL | Minimized or measured? |
|-----------|---------------|-------------|--------|------------------------|
| Quantization grading | FP16 logits | INT4/INT8 logits | `D_KL(p_fp16 ‖ q_quant)` | **measured** (a gate) |
| Knowledge distillation | teacher | student | `D_KL(p_teacher ‖ q_student)` at `T` | **minimized** (the loss) |
| RLHF / alignment | reference policy `π_ref` | tuned policy `π_θ` | `D_KL(π_θ ‖ π_ref)` | **constrained** (a leash) |
| Speculative decoding | target model `p` | draft model `q` | the `p`/`q` gap (KL-like) | **neither** — lossless; gap sets *speed* |

> **Hardware lens:** every metric in this lecture is a **gate before something ships** — a quant, a distilled student, a tuned policy, a draft head. Crucially, computing the gate is a **forward pass**, not a training run: you stream the weights once over an eval set and read logprobs off the output. That is *orders of magnitude* cheaper than the full downstream benchmark suite, which is why these intrinsic numbers are the first filter — they kill the obviously-broken candidates before you spend GPU-days on MMLU/GSM8K/IFEval.

---

**2. Quantization grading — the deepest gate**

Quantization replaces FP16 weights with a lower-precision format (INT8, INT4, the K-quants and I-quants in llama.cpp) to shrink memory footprint and bandwidth — the dominant cost in memory-bound decode. The question every time: **did it break the model, or just dent it?** The instinct is to compute perplexity on both and compare. That works, but it is *rough*, and understanding *why* is the heart of this course.

**2.1 Why PPL ratio is only a proxy**

PPL is a **single scalar averaged over the whole corpus**. It collapses the entire next-token distribution at every position into one number (`exp` of mean NLL on the *held-out token*). Two failure modes hide inside a near-equal PPL:

- The quant can be **right on average but wrong in the tails** — it keeps the top token's probability about right (so NLL on the actual next token barely moves) while badly mangling the *rest* of the distribution. PPL never looks at the mass it put on tokens that didn't occur.
- The quant can **trade errors that cancel** across positions — over-confident here, under-confident there — leaving the mean untouched while the per-token distribution is visibly degraded.

KL divergence does not collapse this way: `D_KL(p_fp16 ‖ q_quant)` is computed over the **full distribution at every token**, then averaged. It sees the tails. That is the whole reason llama.cpp added a KL mode.

</details>

### 2.2 `llama.cpp --kl-divergence` 报告什么

工作流分两趟。先用 `--kl-divergence-base` 在一个语料（按惯例是 WikiText-2）上跑 **FP16 参考模型**，把它的逐 token logits 转储到文件。再用 `--kl-divergence` 跑**量化**模型，指向那个基准文件；它流式读取量化模型的 logits，与缓存的 FP16 分布逐 token 对齐，并报告：

| 指标 | 定义 | 说明什么 |
|--------|-----------|-------------------|
| **均值 `D_KL(p_fp16 ‖ q_quant)`** | 逐 token `Σ_v p_fp16(v)·log(p_fp16(v)/q_quant(v))` 的平均 | 整体分布漂移；核心数字 |
| **中位数 `D_KL`** | 逐 token KL 的第 50 百分位 | 典型情况下的漂移，对少数病态 token 稳健 |
| **最大值 / 99 百分位 `D_KL`** | 最差的逐 token KL | 抓住平均值尚可、却在罕见 token 上灾难性失败的量化 |
| **RMS Δ token-prob** | 实际采样到的 token 上的 `sqrt(E[(q_quant(x) − p_fp16(x))²])` | **量化注入到 token 概率上的高斯噪声标准差** —— 一个可直接解释的“概率抖得有多厉害” |
| **Top-1 一致率 %** | `argmax q_quant == argmax p_fp16` 的 token 占比 | 贪心解码选中*同一* token 的频率 —— 与感知质量高度相关 |
| **Top-k 一致率 %** | FP16 top-1 落在量化模型 top-k 内的占比 | 更宽松的一致性；采样时有用，贪心时用处不大 |
| **PPL 比值** | `PPL(quant) / PPL(fp16)` | 经典的代理指标；保留它，但别单独信它 |

RMS 这套视角才是要内化的：量化在一阶近似下就是**加在 logits 上的加性噪声**，而 RMS Δ token-prob 字面上就是该噪声经 softmax 传播后的标准差。RMS Δ 为 0.002 的量化是在耳语；0.02 的就是在吼。

### 2.3 2026 年的发现 —— 格式重于位宽，内在指标必要但不充分

本轮周期决定性的实证结果是 **arXiv 2601.14277，《Which Quantization Should I Use? A Systematic Evaluation of llama.cpp Quantization on Llama-3.1-8B》**（2026 年 1 月）。每位 MLSys 工程师都应记住两个发现：

1. **格式比标称位宽更重要。** 设计良好的 ~4-bit 格式（K-quants/I-quants，采用重要性加权、混合精度的分块布局）能*击败*标称位宽*更高*的朴素舍入格式。“4-bit”不是质量等级；**打包方案**才是。不要按名字里的数字给量化方法排名。
2. **内在指标必要但不充分。** 两个 PPL 几乎相同——甚至平均 KLD 也几乎相同——的量化模型，可以**在推理与指令遵循 benchmark 上出现可测量的分歧**（GSM8K、MMLU-Pro、IFEval）。分布层面的门禁能抓住严重损坏；它**不能**证明多步推理仍然存活。0.3% 的 PPL 上升可能掩盖真实的 GSM8K 回退，因为失败藏在思维链深处*少数*高杠杆 token 上，被语料均值淹没。（关于这些下游评测*具体是什么* —— 从 HumanEval 到 SWE-bench、Terminal-Bench、MCPMark 的编码/agent 能力阶梯 —— 见 [Agentic AI · Lecture 23 §8](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/Lecture-23.md)。）

由此得出的实用经验法则：

```text
   RANK quants by:  mean D_KL(p_fp16 ‖ q_quant)  and  top-1 agreement %
                    (these track perceived quality better than the PPL delta)
   GATE on intrinsic:  reject anything with mean KLD or top-1 agreement out of band
   CONFIRM on downstream:  the survivors MUST pass a reasoning/instruction eval
                           (PPL/KLD necessary, NOT sufficient)
```

> **2026 更新：** 共识已固化为两句口号。**(a) “格式重于位宽”** —— 要选量化的*方案*（K-quant/I-quant、重要性加权、混合精度分块），而不是标签上的位数；好的 4-bit 格式打败粗糙的 5-bit 格式。**(b) “KLD 重于 PPL，内在指标重于没有，下游重于内在”** —— 平均 `D_KL` 与 top-1 一致率同感知质量的相关性优于 PPL 差值，但*所有*内在指标都是必要不充分的：一个量化模型可以 PPL 和 KLD 都对得上，却悄悄丢掉一个推理 benchmark，所以内在指标面板是**快速筛子**，下游评测才是**判决**。（arXiv 2601.14277。）

---

## 3. 知识蒸馏 —— 最小化 teacher-student KL

蒸馏（Hinton, Vinyals & Dean, 2015）训练一个小的 **student** `q` 去模仿大的 **teacher** `p`。student 不单从硬 one-hot 标签学习；它从 teacher 的**完整软分布**学习，后者每个样本承载的信息多得多。


<details>
<summary>English original</summary>

**2.2 What `llama.cpp --kl-divergence` reports**

The workflow is two passes. First, run the **FP16 reference** over a corpus (WikiText-2 by convention) with `--kl-divergence-base` to dump its per-token logits to a file. Then run the **quantized** model with `--kl-divergence`, pointing at that base file; it streams the quant's logits, aligns them token-for-token with the cached FP16 distribution, and reports:

| Metric | Definition | What it tells you |
|--------|-----------|-------------------|
| **Mean `D_KL(p_fp16 ‖ q_quant)`** | average over tokens of `Σ_v p_fp16(v)·log(p_fp16(v)/q_quant(v))` | overall distributional drift; the headline number |
| **Median `D_KL`** | the 50th-percentile per-token KL | typical-case drift, robust to a few pathological tokens |
| **Max / 99th-pct `D_KL`** | worst per-token KL | catches a quant that is fine on average but catastrophic on rare tokens |
| **RMS Δ token-prob** | `sqrt(E[(q_quant(x) − p_fp16(x))²])` on the realized token | **the std of the Gaussian noise quantization injects onto token probabilities** — a directly interpretable "how jittery did probs get" |
| **Top-1 agreement %** | fraction of tokens where `argmax q_quant == argmax p_fp16` | how often greedy decoding picks the *same* token — tracks perceived quality closely |
| **Top-k agreement %** | fraction where the FP16 top-1 is within the quant's top-k | softer agreement; useful when sampling, not greedy |
| **PPL ratio** | `PPL(quant) / PPL(fp16)` | the classic proxy; keep it, but don't trust it alone |

The RMS framing is the one to internalize: quantization is, to first order, **additive noise on the logits**, and the RMS Δ token-prob is literally the standard deviation of that noise after it propagates through softmax. A quant with RMS Δ of 0.002 is whispering; one at 0.02 is shouting.

**2.3 The 2026 finding — format over bit-width, intrinsic-necessary-not-sufficient**

The decisive empirical result of this cycle is **arXiv 2601.14277, "Which Quantization Should I Use? A Systematic Evaluation of llama.cpp Quantization on Llama-3.1-8B"** (January 2026). Two findings every MLSys engineer should carry:

1. **Format matters more than nominal bit-width.** A well-designed ~4-bit format (the K-quants/I-quants with their importance-weighted, mixed-precision block layouts) can *beat* a naively-rounded format at a *higher* nominal bit count. "4-bit" is not a quality tier; the **packing scheme** is. Do not rank quants by the number in their name.
2. **Intrinsic metrics are necessary but NOT sufficient.** Two quants with near-identical PPL — even near-identical mean KLD — can **diverge measurably on reasoning and instruction-following benchmarks** (GSM8K, MMLU-Pro, IFEval). The distributional gate catches gross breakage; it does **not** certify that multi-step reasoning survived. A 0.3% PPL bump can hide a real GSM8K regression because the failure lives in a *few* high-leverage tokens deep in a chain of thought, which the corpus mean drowns out. (For *what* those downstream evals are — the coding/agent capability ladder from HumanEval through SWE-bench, Terminal-Bench, and MCPMark — see [Agentic AI · Lecture 23 §8](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/Lecture-23.md).)

The practical rule of thumb that falls out:

```text
   RANK quants by:  mean D_KL(p_fp16 ‖ q_quant)  and  top-1 agreement %
                    (these track perceived quality better than the PPL delta)
   GATE on intrinsic:  reject anything with mean KLD or top-1 agreement out of band
   CONFIRM on downstream:  the survivors MUST pass a reasoning/instruction eval
                           (PPL/KLD necessary, NOT sufficient)
```

> **2026 update:** the consensus has hardened around two slogans. **(a) "Format over bit-width"** — choose the quantization *scheme* (K-quant/I-quant, importance-weighted, mixed-precision blocks), not the bit count on the label; a good 4-bit format beats a sloppy 5-bit one. **(b) "KLD over PPL, intrinsic over nothing, downstream over intrinsic"** — mean `D_KL` and top-1 agreement correlate with perceived quality better than the PPL delta, but *all* intrinsic metrics are necessary-not-sufficient: a quant can match PPL and KLD yet quietly lose a reasoning benchmark, so the intrinsic panel is the **fast filter**, and a downstream eval is the **verdict**. (arXiv 2601.14277.)

---

**3. Knowledge distillation — minimizing the teacher-student KL**

Distillation (Hinton, Vinyals & Dean, 2015) trains a small **student** `q` to imitate a large **teacher** `p`. The student doesn't learn from the hard one-hot labels alone; it learns from the teacher's **full soft distribution**, which carries far more information per example.

</details>

### 3.1 目标是在温度 `T` 下的 forward KL

用温度 `T` 软化两个分布（在 softmax 之前把 logits 除以 `T`），然后匹配：

```text
   L_distill  =  D_KL( p_teacher^(T)  ‖  q_student^(T) )
              =  Σ_v  p_T(v) · [ log p_T(v) − log q_T(v) ]

   where  p_T(v) = softmax(z_teacher / T)[v],   q_T(v) = softmax(z_student / T)[v]
```

这就是 **forward KL**——第一个位置放的是 `p`（教师）。由于 `D_KL(p ‖ q) = H(p, q) − H(p)` 以及 `H(p)`（教师的熵）在学生训练期间是固定的，最小化这个 KL 等同于最小化**学生相对教师软标签的交叉熵**——与 Lecture 02 中完全相同的 `H(p, q)`，只不过现在由教师扮演「数据」的角色。

### 3.2 Dark knowledge

软目标之所以胜过硬标签，原因在于 **“dark knowledge”**：教师分配给**错误**类别的*相对*概率。一个说「dog 0.9, wolf 0.08, cat 0.0002, car 1e-9」的教师，已经告诉学生这张图像更像 wolf 而远不像 cat，也完全不像 car——这是一种丰富的相似性结构，而 one-hot 标签 `dog=1` 会把它彻底抹掉。温度 `T > 1` *放大*了这一信号：它把分布压平，将极小的 logits 抬升到一个其比值能携带梯度的区间。对大语言模型而言，对应物是完整的 next-token 分布：教师对*合理续写*的排序就是 dark knowledge，而匹配这一排序，正是蒸馏模型的能力之所以远超其参数数量所暗示水平的原因。

### 3.3 关于 `T²` 的梯度缩放注记

当你用 `T` 软化 logits 时，软目标交叉熵关于学生 logits 的梯度按 `1/T²` 缩放。因此，若把蒸馏与硬标签损失混合使用，就要**把蒸馏项乘以 `T²`**，以使两个梯度在不同温度下处于可比的量级：

```text
   L = α · T² · D_KL(p_T ‖ q_T)   +   (1 − α) · CE(hard_label, q_{T=1})
       └──────── soft, scaled ────┘       └──── hard, unscaled ────┘
```

忘掉 `T²`，那么随着你提高 `T`，蒸馏信号会悄悄变小——而这恰恰是你希望它更重要的时候。

### 3.4 Forward KL → mode-covering

由 Lecture 04：**forward KL `D_KL(p ‖ q)` 是 mode-covering（zero-avoiding）的。** 只要教师 `p` 在哪里分配了概率质量，若学生 `q(v) → 0`，则 `p(v)·log(p(v)/q(v))` 这一项就会爆炸，因此学生*被迫*在教师有概率的地方都保留概率——它**在教师的概率质量上做平滑**，而不是塌缩到单个最可能的续写上。这通常正是你希望生成式学生具备的（覆盖、多样性、校准的尾部），而它与交换参数顺序后得到的 reverse-KL、mode-*seeking* 行为正好相反——这一对比会成为 §4 的全部内容。

关于工程实践——数据流水线、中间特征匹配、在给定压缩预算下何时蒸馏优于剪枝或量化——参见 [Practical Machine Learning (CS329P) — Lecture 10](../../../阶段3-人工智能/04-智能体AI与ML工程/02-ML工程与MLOps/01-实用机器学习CS329P/Lecture-10.md) 中的压缩章节，它*使用*了这个 KL 而没有推导它。本节就是它所回指的那段推导。

---

## 4. RLHF / alignment — 奖励减去一条 KL 缰绳

基于人类反馈的强化学习微调一个策略 `π_θ`，以最大化学习到的奖励 `r(x, y)`。若不加约束，优化器就会**reward-hack**：它会找到在不完美奖励模型下得分很高的退化输出——重复、谄媚、利用性短语，最终变成乱码——并使分布塌缩。解决办法是给参考（pre-RLHF）策略 `π_ref` 加一条 **KL 缰绳**：

```text
   maximize_θ   E_{x, y∼π_θ}[ r(x, y) ]   −   β · D_KL( π_θ(·|x)  ‖  π_ref(·|x) )
                └──── chase reward ────┘        └──── but don't stray from the trusted model ────┘
```

这里 `p = π_ref`（可信），`q = π_θ`（可以承受其移动的策略）。`β` 决定缰绳的长度：`β` 大时，`π_θ` 紧贴 `π_ref`（安全，保守）；`β` 小时则任其游走（能力强，风险高）。KL 项正是防止 reward hacking、mode collapse 以及缓慢漂移到分布外文本的东西——它是**那个让微调后的模型仍可辨认地是同一个模型的正则项。**


<details>
<summary>English original</summary>

**3.1 The objective is forward KL at temperature `T`**

Soften both distributions with temperature `T` (divide logits by `T` before softmax), then match:

```text
   L_distill  =  D_KL( p_teacher^(T)  ‖  q_student^(T) )
              =  Σ_v  p_T(v) · [ log p_T(v) − log q_T(v) ]

   where  p_T(v) = softmax(z_teacher / T)[v],   q_T(v) = softmax(z_student / T)[v]
```

This is **forward KL** — `p` (teacher) in the first slot. Because `D_KL(p ‖ q) = H(p, q) − H(p)` and `H(p)` (the teacher's entropy) is fixed during student training, minimizing this KL is identical to minimizing the **cross-entropy of the student against the teacher's soft labels** — the exact same `H(p, q)` from Lecture 02, now with the teacher playing the role of "the data."

**3.2 Dark knowledge**

The reason soft targets beat hard labels is **"dark knowledge"**: the *relative* probabilities the teacher assigns to the **wrong** classes. A teacher that says "dog 0.9, wolf 0.08, cat 0.0002, car 1e-9" has told the student that this image is *much* more wolf-like than cat-like and nothing like a car — a rich similarity structure that a one-hot label `dog=1` annihilates. Temperature `T > 1` *amplifies* this signal by flattening the distribution, pulling the tiny logits up into a range where their ratios carry gradient. For LLMs the analogue is the full next-token distribution: the teacher's ranking of *plausible continuations* is the dark knowledge, and matching it is why a distilled model can feel far more capable than its parameter count suggests.

**3.3 The `T²` gradient-scaling note**

When you soften logits by `T`, the gradient of the soft-target cross-entropy w.r.t. the student logits scales like `1/T²`. So if you blend distillation with a hard-label loss, you **multiply the distillation term by `T²`** to keep the two gradients on a comparable scale across temperatures:

```text
   L = α · T² · D_KL(p_T ‖ q_T)   +   (1 − α) · CE(hard_label, q_{T=1})
       └──────── soft, scaled ────┘       └──── hard, unscaled ────┘
```

Forget the `T²` and your distillation signal silently shrinks as you raise `T`, exactly when you wanted it to matter more.

**3.4 Forward KL → mode-covering**

From Lecture 04: **forward KL `D_KL(p ‖ q)` is mode-covering (zero-avoiding).** Wherever the teacher `p` puts mass, the term `p(v)·log(p(v)/q(v))` blows up if the student `q(v) → 0`, so the student is *forced* to keep probability everywhere the teacher does — it **smooths over the teacher's mass** rather than collapsing onto the single most likely continuation. That is usually what you want from a generative student (coverage, diversity, calibrated tails), and it is the opposite of the reverse-KL, mode-*seeking* behavior you would get if you swapped the arguments — a contrast that becomes the entire story in §4.

For the engineering practice — data pipelines, intermediate-feature matching, when distillation beats pruning or quantization for a given compression budget — see the compression treatment in [Practical Machine Learning (CS329P) — Lecture 10](../../../阶段3-人工智能/04-智能体AI与ML工程/02-ML工程与MLOps/01-实用机器学习CS329P/Lecture-10.md), which *uses* this KL without deriving it. This section is the derivation it points back to.

---

**4. RLHF / alignment — reward minus a KL leash**

Reinforcement learning from human feedback fine-tunes a policy `π_θ` to maximize a learned reward `r(x, y)`. Left unconstrained, the optimizer **reward-hacks**: it finds degenerate outputs that score high under the imperfect reward model — repetition, sycophancy, exploit phrases, eventually gibberish — and collapses the distribution. The fix is a **KL leash** to the reference (pre-RLHF) policy `π_ref`:

```text
   maximize_θ   E_{x, y∼π_θ}[ r(x, y) ]   −   β · D_KL( π_θ(·|x)  ‖  π_ref(·|x) )
                └──── chase reward ────┘        └──── but don't stray from the trusted model ────┘
```

Here `p = π_ref` (trusted), `q = π_θ` (the policy you can afford to move). `β` sets the leash length: large `β` keeps `π_θ` glued to `π_ref` (safe, timid); small `β` lets it roam (capable, risky). The KL term is what prevents reward hacking, mode collapse, and the slow drift into off-distribution text — it is the **regularizer that keeps the tuned model recognizably the same model.**

</details>

### 4.1 DPO 正是这一目标的闭式最优解

优雅的结果（Rafailov et al., 2023）：**KL 正则化的奖励目标存在闭式最优策略**，将其反解即可完全跳过 RL 循环。最优解是被指数化奖励 *重加权* 的参考策略：

```text
   π*(y|x)  =  (1/Z(x)) · π_ref(y|x) · exp( r(x, y) / β )         ← optimum of the KL-leashed objective

   ⇒ solve for the implicit reward:   r(x, y)  =  β · log( π*(y|x) / π_ref(y|x) )  +  β·log Z(x)
```

把该隐式奖励代入 Bradley–Terry 偏好模型，配分函数 `Z(x)` 相消，剩下的就是 **Direct Preference Optimization**——一种在偏好对 `(y_win, y_lose)` 上的简单分类损失，直接训练策略。DPO 与 RLHF 并非不同的目标；它就是 *同一个 KL 正则化奖励目标的闭式解*。KL 牵引在其推导本身中就已内置——它就是位于损失之中的 `β·log(π_θ/π_ref)` 项。

### 4.2 KL 项是 PPO → DPO → GRPO 全程的不变量

算法不断更替；牵引不变。

| 方法 | 变化之处 | KL 项 |
|--------|--------------|-------------|
| **PPO**（RLHF 经典） | 在线 RL、学习到的奖励模型、价值网络 | 每 token 奖励中的 **显式** `−β·D_KL(π_θ ‖ π_ref)` |
| **DPO** | 去掉 RL 循环；在偏好对上离线训练 | **隐式**——内置于闭式 `β·log(π_θ/π_ref)` 中 |
| **GRPO**（DeepSeek） | 去掉价值网络；组相对优势来自采样得到的完成结果 | 目标中保留 **显式** `D_KL(π_θ ‖ π_ref)` 项 |

GRPO 值得注意：在剥离价值函数 critic（这一重大简化支撑了 DeepSeek-R1 的推理结果）时，它 **保留了显式 KL 项**。这就是线索。过去三年每一次重新设计——在线还是离线、有无 critic——不变的都是对 `π_ref` 的牵引。**KL 惩罚是对齐的不变量**，它正是本课程恒等式的第三项，如今扮演裁判而非评分者。

---

## 5. 投机解码——为什么它是 *无损* 的

投机解码是其中的异类：它涉及 draft 模型 `q` 与 target 模型 `p`，二者之间的差距是 KL 式的，然而 **其输出分布可证明与 target 的完全相同**。没有近似，没有质量损失——只有速度。原因如下。

### 5.1 接受规则就是一次对数概率比值检验

廉价的 **draft** `q` 提议一个 token `x`。昂贵的 **target** `p` 为它打分。接受或修正：

```text
   draft proposes x ~ q(·)
   accept x   with probability   min(1, p(x) / q(x))
   on reject: resample x' ~ norm( max(0, p − q) )      ← the normalized residual
```

接受概率 `min(1, p(x)/q(x))` 是 **概率之比**——也就是对数概率之差 `log p(x) − log q(x)`，以零为阈值。本课程一直在读的正是这个量。精妙之处在于 **拒绝时的修正**：拒绝时并不回退到 target 的完整分布（那会把 draft 已经猜对的质量重复计算）——而是从 `max(0, p − q)` 重新采样，即 target 想要、而 draft *提议不足* 的那部分质量，经重新归一化。

### 5.2 证明被接受的样本分布恰好是 `p`

```text
   P(output = x) = P(accept on the draft) + P(reject, then resample to x)

   ① draft proposes x and it is accepted:
        q(x) · min(1, p(x)/q(x))  =  min(q(x), p(x))

   ② draft proposes ANY token, it is rejected, then the residual resamples to x:
        P(reject) · norm(max(0, p−q))[x]
        P(reject) = Σ_y q(y)·(1 − min(1, p(y)/q(y))) = Σ_y (q(y) − min(q(y),p(y))) = Σ_y max(0, q(y)−p(y))
        the residual max(0, p−q) sums to  Σ_y max(0, p(y)−q(y))  = the SAME total (both equal 1 − Σ min(p,q))
        ⇒ the P(reject) factor and the residual's normalizer CANCEL, leaving exactly  max(0, p(x) − q(x))

   total:  min(q(x), p(x)) + max(0, p(x) − q(x))  =  p(x)        ∎   (for ANY draft q)
```

两种情况由代数恒等式 `min(a,b) + max(0, b−a) = b` 求和得到 `p(x)`。draft `q` **完全相消**——在结果中不出现。这就是「无损」的形式化内容：对 *任意* draft，无论好坏，输出 token 的分布都恰好与 target 相同。糟糕的 draft 从不会污染输出；它只是浪费算力。


<details>
<summary>English original</summary>

**4.1 DPO is the closed-form optimum of exactly this objective**

The elegant result (Rafailov et al., 2023): the **KL-regularized reward objective has a closed-form optimal policy**, and inverting it lets you skip the RL loop entirely. The optimum is the reference policy *reweighted* by exponentiated reward:

```text
   π*(y|x)  =  (1/Z(x)) · π_ref(y|x) · exp( r(x, y) / β )         ← optimum of the KL-leashed objective

   ⇒ solve for the implicit reward:   r(x, y)  =  β · log( π*(y|x) / π_ref(y|x) )  +  β·log Z(x)
```

Substitute that implicit reward into the Bradley–Terry preference model and the partition function `Z(x)` cancels, leaving **Direct Preference Optimization** — a simple classification loss on preference pairs `(y_win, y_lose)` that trains the policy directly. DPO is not a different objective from RLHF; it is the *same KL-regularized reward objective solved in closed form*. The KL leash is baked into its very derivation — it is the `β·log(π_θ/π_ref)` term sitting inside the loss.

**4.2 The KL term is the invariant across PPO → DPO → GRPO**

The algorithms churn; the leash does not.

| Method | What changed | The KL term |
|--------|--------------|-------------|
| **PPO** (RLHF classic) | online RL, learned reward model, value network | **explicit** `−β·D_KL(π_θ ‖ π_ref)` in the per-token reward |
| **DPO** | drops the RL loop; offline on preference pairs | **implicit** — baked into the closed-form `β·log(π_θ/π_ref)` |
| **GRPO** (DeepSeek) | drops the value network; group-relative advantage from sampled completions | **explicit** `D_KL(π_θ ‖ π_ref)` term retained in the objective |

GRPO is the one to note: in stripping away the value-function critic (a major simplification that powered the DeepSeek-R1 reasoning results), it **kept the explicit KL term**. That is the tell. Across every redesign of the last three years — online vs offline, with critic or without — the constant is the leash to `π_ref`. **The KL penalty is the invariant of alignment**, and it is exactly the third term of this course's identity, now playing referee instead of grader.

---

**5. Speculative decoding — why it's *lossless***

Speculative decoding is the odd one out: it involves a draft model `q` and a target model `p`, the gap between them is KL-like, and yet **the output distribution is provably identical to the target's**. No approximation, no quality loss — only speed. Here is why.

**5.1 The acceptance rule is a logprob-ratio test**

A cheap **draft** `q` proposes a token `x`. The expensive **target** `p` scores it. Accept or repair:

```text
   draft proposes x ~ q(·)
   accept x   with probability   min(1, p(x) / q(x))
   on reject: resample x' ~ norm( max(0, p − q) )      ← the normalized residual
```

The accept probability `min(1, p(x)/q(x))` is a **ratio of probabilities** — i.e., a difference of logprobs, `log p(x) − log q(x)`, thresholded at zero. This course has been about reading exactly that quantity. The genius is the **repair on rejection**: when you reject, you don't fall back to the target's full distribution (that would double-count the mass the draft already got right) — you resample from `max(0, p − q)`, the mass the target wanted that the draft *under*-proposed, renormalized.

**5.2 Proof that accepted samples are distributed exactly as `p`**

```text
   P(output = x) = P(accept on the draft) + P(reject, then resample to x)

   ① draft proposes x and it is accepted:
        q(x) · min(1, p(x)/q(x))  =  min(q(x), p(x))

   ② draft proposes ANY token, it is rejected, then the residual resamples to x:
        P(reject) · norm(max(0, p−q))[x]
        P(reject) = Σ_y q(y)·(1 − min(1, p(y)/q(y))) = Σ_y (q(y) − min(q(y),p(y))) = Σ_y max(0, q(y)−p(y))
        the residual max(0, p−q) sums to  Σ_y max(0, p(y)−q(y))  = the SAME total (both equal 1 − Σ min(p,q))
        ⇒ the P(reject) factor and the residual's normalizer CANCEL, leaving exactly  max(0, p(x) − q(x))

   total:  min(q(x), p(x)) + max(0, p(x) − q(x))  =  p(x)        ∎   (for ANY draft q)
```

The two cases sum to `p(x)` by the algebraic identity `min(a,b) + max(0, b−a) = b`. The draft `q` **cancels out completely** — it appears nowhere in the result. That is the formal content of "lossless": for *any* draft, good or garbage, the emitted token is distributed exactly as the target. A bad draft never corrupts the output; it only wastes work.

</details>

### 5.3 类 KL 差距决定的是*速度*，而非质量

那么 draft 质量能换来什么？**接受率**，以及由此而来的**期望接受长度**（每次 target 前向传播所产出的 token）。`q` 与 `p` 越接近，`min(1, p/q) = 1` 就越常发生，token 也就越能存活 —— **draft 对齐得越好 → 接受率越高 → 每次内存 pass 产出更多 token → 加速比越大**。起决定作用的差距，恰恰就是 draft 与 target 之间的散度：`q` 与 `p` 匹配之处，接受率高；`q` 背离之处，就要承受拒绝与重新 draft。这是本讲中唯一一处 KL **既不被最小化、也不作为门限被度量** —— 它是*吞吐*的杠杆，而质量已由上面的证明钉死在 target 上。

这是从算法视角看到的、你曾在 [MLSys Deep Dives — Lecture 06](../05-MLSys深度探索/Lecture-06.md) 中作为 *systems* 技术遇到的东西（EAGLE-3、DFlash、verify-kernel 栈，以及作为支配度量的接受长度），也是你在 [Gemma 4 Edge Deployment — Lecture 06](../../03-方向C-边缘AI/04-Gemma-4边缘部署/Lecture-06.md) MTP-drafter 讲中作为 *端侧测量* 遇到的东西，那里接受长度是在真实芯片上 profile 出来的。同一条接受规则，三个高度：这里是数学，那里是系统，那里是硬件。

> **硬件视角：** draft 模型本身就是一个 `q`，可以在 *发布之前* 用 §2 的工具给它打分 —— 在一份语料上跑 FP16 参考实现和 draft，读取逐 token 的 KL/一致度。与 target 的 top-1 一致度高的 draft，就是接受长度高的 draft，而接受长度就是吞吐。draft 的质量和它的加速比是 *同一次测量*（即 `p`/`q` 差距），所以你在 §6 中搭建的打分器兼作 draft 质量预测器 —— 而且一如既往，target 的 logits **只缓存一次**，在你评测的每个候选 draft 之间复用。

---


<details>
<summary>English original</summary>

**5.3 The KL-like gap sets the *speed*, not the quality**

So what does the draft quality buy you? **Acceptance rate**, and therefore **expected accept-length** (tokens emitted per target forward pass). The closer `q` is to `p`, the more often `min(1, p/q) = 1` and the token survives — a **better-aligned draft → higher acceptance → more tokens per memory pass → more speedup**. The governing gap is exactly a divergence between draft and target: where `q` matches `p`, acceptance is high; where `q` diverges, you eat rejections and re-drafts. This is the one place in the lecture where the KL is **neither minimized nor measured as a gate** — it is the lever on *throughput*, with quality nailed to the target by the proof above.

This is the algorithm's-eye view of what you met as a *systems* technique in [MLSys Deep Dives — Lecture 06](../05-MLSys深度探索/Lecture-06.md) (EAGLE-3, DFlash, the verify-kernel stack, and acceptance length as the governing metric), and as an *on-device measurement* in the [Gemma 4 Edge Deployment — Lecture 06](../../03-方向C-边缘AI/04-Gemma-4边缘部署/Lecture-06.md) MTP-drafter lecture, where acceptance length is profiled on real silicon. Same acceptance rule, three altitudes: the math here, the systems there, the hardware there.

> **Hardware lens:** the draft model is itself a `q` you can grade with §2's tools *before* you ship it — run the FP16 reference and the draft over a corpus and read the per-token KL/agreement. A draft with high top-1 agreement to the target is a draft with high acceptance length, which is throughput. The draft's quality and its speedup are the *same measurement* (the `p`/`q` gap), so the grader you build in §6 doubles as a draft-quality predictor — and, as always, the target's logits are **cached once** and reused across every candidate draft you evaluate.

---

</details>

## 6. 综合实战 —— 构建量化评分器

把整章串起来。加载 FP16 **参考**模型与**量化**模型，让两者都在 WikiText-2 上跑一遍，汇报 §2 的面板，外加一个带阈值的判定。采用贴近实际的 Hugging Face + PyTorch；唯一的微妙之处是正确的滑动窗口（第 03 讲），以及缓存参考模型的 logits。


<details>
<summary>English original</summary>

**6. Capstone — build a quant grader**

Tie it together. Load an FP16 **reference** and a **quantized** model, run both over WikiText-2, and report the panel from §2 plus a thresholded verdict. Realistic Hugging Face + PyTorch; the only subtlety is a correct sliding window (Lecture 03) and caching the reference logits.

</details>

```python
import torch, torch.nn.functional as F
from transformers import AutoModelForCausalLM, AutoTokenizer
from datasets import load_dataset

REF_ID   = "meta-llama/Llama-3.1-8B"          # p  — FP16 reference (trusted)
QUANT_ID = "path/to/llama-3.1-8b-q4_k_m"      # q  — quantized model (cheap)
STRIDE, WINDOW = 512, 2048                      # sliding-window PPL (Lecture 03)

def load(model_id, dtype):
    tok = AutoTokenizer.from_pretrained(model_id)
    m   = AutoModelForCausalLM.from_pretrained(model_id, torch_dtype=dtype,
                                               device_map="cuda").eval()
    return tok, m

@torch.no_grad()
def grade(ref_id=REF_ID, quant_id=QUANT_ID):
    tok, ref   = load(ref_id, torch.float16)
    _,   quant = load(quant_id, torch.float16)   # quantized weights, fp16 compute
    text = "\n\n".join(load_dataset("wikitext", "wikitext-2-raw-v1", split="test")["text"])
    ids  = tok(text, return_tensors="pt").input_ids.cuda()

    nll_ref = nll_q = kl_sum = rms_sq = 0.0
    top1_hits = n_tok = 0
    prev = 0
    for begin in range(0, ids.size(1), STRIDE):
        end = min(begin + WINDOW, ids.size(1))
        trg_len = end - prev                       # only score the NEW tokens (no double count)
        chunk   = ids[:, begin:end]
        labels  = chunk.clone(); labels[:, :-trg_len] = -100

        # --- one forward pass each; the reference logits are computed once here and reused ---
        lr = ref(chunk).logits[0, :-1].float()     # p over the vocab, per position
        lq = quant(chunk).logits[0, :-1].float()   # q over the vocab, per position
        tgt = chunk[0, 1:]                          # the realized next tokens
        mask = labels[0, 1:] != -100                # positions we actually score

        logp = F.log_softmax(lr[mask], dim=-1)      # log p
        logq = F.log_softmax(lq[mask], dim=-1)      # log q
        p, q = logp.exp(), logq.exp()
        gold = tgt[mask]

        # cross-entropies on the realized token -> the two PPLs
        nll_ref += -logp[torch.arange(gold.numel()), gold].sum().item()
        nll_q   += -logq[torch.arange(gold.numel()), gold].sum().item()
        # mean D_KL(p_fp16 || q_quant): full-distribution, summed over vocab then tokens
        kl_sum  += (p * (logp - logq)).sum().item()
        # RMS token-prob change on the realized token (the Gaussian-noise std)
        rms_sq  += ((q[torch.arange(gold.numel()), gold]
                     - p[torch.arange(gold.numel()), gold]) ** 2).sum().item()
        # top-1 agreement: same argmax?
        top1_hits += (lr[mask].argmax(-1) == lq[mask].argmax(-1)).sum().item()
        n_tok     += gold.numel()
        prev = end
        if end == ids.size(1): break

    import math
    return {
        "ppl_fp16":      math.exp(nll_ref / n_tok),
        "ppl_quant":     math.exp(nll_q   / n_tok),
        "ppl_ratio":     math.exp(nll_q / n_tok) / math.exp(nll_ref / n_tok),
        "mean_kld":      kl_sum / n_tok,                  # nats; D_KL(p_fp16 || q_quant)
        "top1_agree_pct": 100.0 * top1_hits / n_tok,
        "rms_dprob":     math.sqrt(rms_sq / n_tok),       # std of injected prob noise
    }

def verdict(m, kld_ship=0.02, kld_warn=0.10, top1_ship=99.0, ppl_warn=1.01):
    """Intrinsic gate only — necessary, NOT sufficient (see 2026 finding)."""
    if m["mean_kld"] <= kld_ship and m["top1_agree_pct"] >= top1_ship \
                                  and m["ppl_ratio"] <= ppl_warn:
        return "SHIP (intrinsic clean) — still confirm on a reasoning/instruction eval"
    if m["mean_kld"] >= kld_warn or m["top1_agree_pct"] < 95.0:
        return "REJECT — quant broke the distribution (high KLD / low top-1 agreement)"
    return "INVESTIGATE — borderline; run GSM8K/MMLU-Pro/IFEval before deciding"

if __name__ == "__main__":
    m = grade()
    for k, v in m.items():
        print(f"{k:>16}: {v:.4f}")
    print("verdict:", verdict(m))
```

阈值是刻意保守的起点（`mean_kld`，单位 nats；按模型族分别调优）。注意 `verdict()` **不**声称的一点：一个 "SHIP" 是 *intrinsic* 的 pass，它明确要求你去下游确认——这正是 §2.3 的教训被编码进代码。Grader 是快速过滤器；reasoning 评测才是裁决。

> **硬件视角：** 整个 grader 不过就是 **几百 KB 文本上的两次前向传播**——在一块 GPU 上只要几分钟——而跑完整的 benchmark 套件要花 GPU-*天*。正是这种不对称性，使得 intrinsic grading 成为每一个严肃的量化或 drafting 流水线的第一道关卡：把 FP16 参考 logits 缓存一次，用它们廉价地扫过每一个候选 quant/draft，只把幸存者升级到昂贵的下游评测。

---


<details>
<summary>English original</summary>

The thresholds are deliberately conservative starting points (`mean_kld` in nats; tune per model family). Note what `verdict()` does **not** claim: a "SHIP" is an *intrinsic* pass that explicitly tells you to confirm downstream — the §2.3 lesson encoded in code. The grader is the fast filter; the reasoning eval is the verdict.

> **Hardware lens:** this entire grader is **two forward passes over a few hundred KB of text** — minutes on one GPU — versus GPU-*days* for a full benchmark suite. That asymmetry is why intrinsic grading is the first gate in every serious quantization or drafting pipeline: cache the FP16 reference logits once, sweep every candidate quant/draft against them cheaply, and only escalate the survivors to the expensive downstream evals.

---

</details>

## 截至

**2026 年 6 月。** 数学已经确定（Shannon 1948；Kullback–Leibler 1951；Hinton et al. 2015；Rafailov et al. 2023）。真正 *当前* 的是量化评估实践，并且它在这一轮已经大幅收敛：

- **2026 年量化评估发现（头条）。** 根据 **arXiv 2601.14277**（“Which Quantization Should I Use? A Systematic Evaluation of llama.cpp Quantization on Llama-3.1-8B”，2026 年 1 月）：**（1）格式胜过标称位宽**——按打包方案（K-quant/I-quant、重要性加权、混合精度分块）对量化版本排序，绝不按名称中的数字排序；一个好的 4-bit 格式胜过草率的更高位宽格式。**（2）内在指标必要但不充分**——具有近乎相同 PPL *和* 近乎相同平均 KLD 的量化版本在推理/指令 benchmark 上仍可能分化，因此内在指标面板是快速筛选器，而下游评估才是裁决。现在，该领域的共识是在内在门控上采用 **“KLD over PPL”**：平均 `D_KL(p_fp16 ‖ q_quant)` 和 top-1 一致率比 PPL 差值更能跟踪感知质量。
- **工具。** `llama.cpp` 的 `--kl-divergence` / `--kl-divergence-base` flags 报告完整面板（平均/中位数/最大 KLD、RMS Δ token-prob、top-1/top-k 一致率、PPL 比率）；flag 名称和输出格式随版本漂移——请对照你的构建进行验证。
- **对齐。** PPO → DPO → GRPO 继续更迭，但 **对 `π_ref` 的 KL 约束是不变量**；GRPO（DeepSeek）在舍弃价值网络后显著保留了一个 *显式* KL 项。DPO 仍是同一 KL 正则化奖励目标的闭式最优解。
- **投机解码。** 无损性是一个定理，而非趋势——它不会过时。前沿（EAGLE-3、DFlash 以及 MiMo + TileRT 吞吐里程碑）旨在将 draft-target 差距拉 *低*，以提高接受长度；这些吞吐数字由厂商报告，并在 [MLSys Deep Dives — Lecture 06](../05-MLSys深度探索/Lecture-06.md) 中跟踪。


<details>
<summary>English original</summary>

**Current as of**

**June 2026.** The mathematics is settled (Shannon 1948; Kullback–Leibler 1951; Hinton et al. 2015; Rafailov et al. 2023). What is *current* is the quantization-evaluation practice, and it has converged hard this cycle:

- **The 2026 quant-eval findings (the headline).** Per **arXiv 2601.14277** ("Which Quantization Should I Use? A Systematic Evaluation of llama.cpp Quantization on Llama-3.1-8B," Jan 2026): **(1) format beats nominal bit-width** — rank quants by their packing scheme (K-quant/I-quant, importance-weighted, mixed-precision blocks), never by the number in the name; a good 4-bit format outperforms a sloppy higher-bit one. **(2) intrinsic metrics are necessary but not sufficient** — quants with near-equal PPL *and* near-equal mean KLD can still diverge on reasoning/instruction benchmarks, so the intrinsic panel is the fast filter and a downstream eval is the verdict. The field consensus is now **"KLD over PPL"** for the intrinsic gate: mean `D_KL(p_fp16 ‖ q_quant)` and top-1 agreement track perceived quality better than the PPL delta.
- **Tooling.** `llama.cpp`'s `--kl-divergence` / `--kl-divergence-base` flags report the full panel (mean/median/max KLD, RMS Δ token-prob, top-1/top-k agreement, PPL ratio); flag names and output formatting drift across releases — verify against your build.
- **Alignment.** PPO → DPO → GRPO continue to churn, but the **KL leash to `π_ref` is the invariant**; GRPO (DeepSeek) notably retains an *explicit* KL term after dropping the value network. DPO remains the closed-form optimum of the same KL-regularized reward objective.
- **Speculative decoding.** Losslessness is a theorem, not a trend — it does not age. The frontier (EAGLE-3, DFlash, and the MiMo + TileRT throughput milestone) is about driving the draft-target gap *down* to raise acceptance length; those throughput numbers are vendor-reported and tracked in [MLSys Deep Dives — Lecture 06](../05-MLSys深度探索/Lecture-06.md).

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Logprobs, Perplexity and KL Divergence/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Logprobs%2C%20Perplexity%20and%20KL%20Divergence/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
