---
title: 对数概率、困惑度与 KL 散度 — LLM 推理的信息论
description: 对数概率、困惑度与 KL 散度 — LLM 推理的信息论
---

# 对数概率、困惑度与 KL 散度 — LLM 推理的信息论

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">−log q</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 5 · ML 系统工程 · 基础课程</p>
<p class="course-identity__title">每个 LLM 系统工程师都必须能流利读出的三个数 — 模型给出的对数概率、给模型打分的困惑度，以及衡量一个量化、蒸馏或对齐后的模型相对原始模型漂移了多远的 KL 散度。</p>
<p class="course-identity__meta">产物：一个量化评分器，报告 PPL delta、平均 KLD 与 top-token 一致率 · 度量：bits-per-token、困惑度比值、D_KL、token 概率 RMS</p>
</div>
</div>

> *对数概率是模型说的话。困惑度是它有多惊讶。KL 散度是你更便宜的那个模型偏离真模型有多远。它们不是三个主题 — 它们是同一个等式的三种读法。*

LLM 推理中每一个严肃的决策，都是关于一个你无法直接看到的概率分布的决策。INT4 量化是把模型弄坏了，还是只磕了一下？蒸馏出的 1B 对 27B 教师模型是否忠实？投机的 draft 模型是否离目标模型足够近，值得一跑？RLHF 是否把 policy 推得离基座模型太远？这四个问题你都用同一组三件仪器来回答 — **对数概率**、**困惑度**和 **KL 散度** — 而这三者都被同一个恒等式约束：

```text
    H(p, q)        =        H(p)         +        D_KL(p ‖ q)
  cross-entropy           entropy                  KL divergence
  the loss you             the irreducible          the EXCESS — the part
  actually train on        floor (data's own        quantization, distillation,
  (mean −log q)            uncertainty)             and RLHF spend their lives fighting
        │                                                  │
        └──────────  Perplexity = exp(H(p, q))  ───────────┘
                     the metric you report
```

`p` 是你信任的分布 — 数据、教师模型、FP16 参考、你不希望偏离的 policy。`q` 是你负担得起的分布 — 你的模型、学生模型、INT4 量化模型、调优后的 policy。交叉熵是你在训练时最小化的量。熵是你永远打不破的下界。**KL 散度是两者之间的差距 — 那部分可避免的浪费 — 而本阶段几乎每一项系统技术，都是一场缩小某个特定 KL 的行动。** 本课程让这个恒等式变成第二天性，然后用最后一讲展示它如何驱动你已经在阶段 5 其他地方见过的量化、蒸馏、对齐与投机解码机制。

**层映射：** 位于 kernel、编译器与架构层*之下*的测量层 — MLSys 工程师用来判断某项优化是否保住了模型所读的仪表。

**目标岗位：** ML 系统工程师 · AI 推理工程师 · 模型压缩工程师 · 评估/质量工程师 · 研究工程师（对齐/蒸馏）。

**前置要求：**

* 高中以上的概率知识（概率分布、期望、log 函数），并能轻松阅读 Python/PyTorch。
* [Edge LLM Inference Internals — Lecture 01](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) 讲 roofline（性能上界模型），以及为什么 decode（逐 token 生成阶段）是带宽受限 — 这里的"硬件视角"提示框假设你读过它。
* 有帮助但非必需：[Practical Machine Learning (CS329P) — Lecture 10 (Model Compression)](../../../阶段3-人工智能/04-智能体AI与ML工程/02-ML工程与MLOps/01-实用机器学习CS329P/Lecture-10.md)，它使用这些指标但不做推导。本课程做的就是推导。

**配套课程：** [MLSys Deep Dives](../05-MLSys深度探索/README.md)（投机解码与量化在其中作为系统出现），以及 [Gemma 4 Edge Deployment](../../03-方向C-边缘AI/04-Gemma-4边缘部署/README.md) 课程（困惑度与接受长度在其中于真实硬件上测量）。

---


<details>
<summary>English original</summary>

**Logprobs, Perplexity & KL Divergence — The Information Theory of LLM Inference**

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">−log q</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 5 · ML Systems Engineering · Foundations Course</p>
<p class="course-identity__title">The three numbers every LLM systems engineer must read fluently — the log-probability a model assigns, the perplexity that grades it, and the KL divergence that measures how far a quantized, distilled, or aligned model has drifted from the original.</p>
<p class="course-identity__meta">Artifact: a quantization grader that reports PPL delta, mean KLD, and top-token agreement · Measure: bits-per-token, perplexity ratio, D_KL, token-probability RMS</p>
</div>
</div>

> *Logprobs are what the model says. Perplexity is how surprised it was. KL divergence is how far your cheaper model strayed from the real one. They are not three topics — they are three readings of one equation.*

Every serious decision in LLM inference is a decision about a probability distribution you cannot see directly. Did INT4 quantization break the model or just dent it? Is the distilled 1B faithful to the 27B teacher? Is the speculative draft close enough to the target to be worth running? Did RLHF push the policy too far from the base model? You answer all four with the same three instruments — **log-probabilities**, **perplexity**, and **KL divergence** — and all three are bound by a single identity:

```text
    H(p, q)        =        H(p)         +        D_KL(p ‖ q)
  cross-entropy           entropy                  KL divergence
  the loss you             the irreducible          the EXCESS — the part
  actually train on        floor (data's own        quantization, distillation,
  (mean −log q)            uncertainty)             and RLHF spend their lives fighting
        │                                                  │
        └──────────  Perplexity = exp(H(p, q))  ───────────┘
                     the metric you report
```

`p` is the distribution you trust — the data, the teacher, the FP16 reference, the policy you don't want to leave. `q` is the distribution you can afford — your model, the student, the INT4 quant, the tuned policy. Cross-entropy is what you minimize when you train. Entropy is the floor you can never beat. **KL divergence is the gap between them — the avoidable waste — and almost every systems technique in this phase is a campaign to shrink one specific KL.** This course makes that identity second nature, then spends its last lecture showing it driving the quantization, distillation, alignment, and speculative-decoding machinery you already met elsewhere in Phase 5.

**Layer mapping:** the measurement layer that sits *under* the kernel, compiler, and architecture layers — the instrumentation an MLSys engineer reads to know whether any optimization preserved the model.

**Role targets:** ML Systems Engineer · AI Inference Engineer · Model-Compression Engineer · Evaluation/Quality Engineer · Research Engineer (alignment/distillation).

**Prerequisites:**

* High-school-plus probability (a probability distribution, expectation, the log function) and comfort reading Python/PyTorch.
* [Edge LLM Inference Internals — Lecture 01](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) for the roofline and why decode is memory-bound — the "Hardware lens" callouts here assume it.
* Helpful but not required: [Practical Machine Learning (CS329P) — Lecture 10 (Model Compression)](../../../阶段3-人工智能/04-智能体AI与ML工程/02-ML工程与MLOps/01-实用机器学习CS329P/Lecture-10.md), which uses these metrics without deriving them. This course is the derivation.

**Pairs with:** [MLSys Deep Dives](../05-MLSys深度探索/README.md) (where speculative decoding and quantization appear as systems) and the [Gemma 4 Edge Deployment](../../03-方向C-边缘AI/04-Gemma-4边缘部署/README.md) course (where perplexity and acceptance length are measured on real hardware).

---

</details>

## 这门课为何这样组织

五讲把主恒等式从原材料一路推到回报：

1. **Logprobs** —— 原材料。下游的一切都由 `log q(token)` 搭建，因此必须确切知道它是什么、为什么它活在对数空间，以及如何从任何 runtime 中把它提取出来。
2. **熵与交叉熵** —— 目标函数。交叉熵*就是*训练损失；熵是它追逐的下界。这一讲赚下恒等式左边的两项。
3. **困惑度** —— 度量。交叉熵的 `exp`、有效分支因子，以及让大多数跨模型比较失效的 tokenizer 陷阱。
4. **KL 散度** —— 差距。第三项，被证明等于交叉熵减熵，连同它的不对称性、它的 forward/reverse 两种性格，以及它作为通用的「与我信任的模型之间的距离」的角色。
5. **应用** —— 回报。把恒等式放出去用在量化评级、知识蒸馏、RLHF 的 KL 惩罚项和投机解码的接受规则上，最后以一个 capstone 量化评级器收尾。

---

## 课程地图（5 讲）

<div class="lecture-map" markdown>

| # | 讲 | 主线 |
|---|---------|-----------|
| [01](Lecture-01.md) | **Logprobs —— 语言模型实际输出的是什么** —— logits → softmax → 对数概率，为何用对数空间（可加性、稳定性、log-sum-exp）、序列 logprob、temperature，以及从 OpenAI / HF / vLLM / llama.cpp 提取 logprobs | 原材料 |
| [02](Lecture-02.md) | **熵、交叉熵与 NLL** —— Shannon 熵、作为训练损失的交叉熵、bits 与 nats、teacher forcing，以及初见 `H(p,q) = H(p) + D_KL` | 目标函数 |
| [03](Lecture-03.md) | **困惑度 —— 困惑的指数** —— `PPL = exp(mean NLL)`、有效分支因子、bits-per-byte、tokenizer 比较陷阱、滑窗 PPL，以及困惑度作为标准的量化度量 | 度量 |
| [04](Lecture-04.md) | **KL 散度 —— 两个分布之间的差距** —— `D_KL(p‖q)`、Gibbs 不等式、不对称性、forward 与 reverse KL、Jensen–Shannon，以及 `D_KL = H(p,q) − H(p)` 的证明 | 差距 |
| [05](Lecture-05.md) | **这一切最终落在哪里** —— llama.cpp `--kl-divergence` 量化评级、知识蒸馏、RLHF/PPO/GRPO 的 KL 惩罚项、投机解码的接受规则，以及一个 capstone 量化评级器 | 回报 |

</div>

---

## 课程产出

学完后你应当能够：

* 从任何 runtime 提取并解读 **token 与序列 logprobs**，并解释为什么做这些算术的正确场所是对数空间（而非概率空间）。
* 从零推导 **交叉熵 = 熵 + KL 散度**，解释每一项在物理上意味着什么 —— 以及为什么交叉熵永远不可能优于熵。
* 用滑窗正确计算 **困惑度**，在 tokenizer 不同时报告 **bits-per-byte**，并解释为什么跨两个 tokenizer 直接比较原始 PPL 毫无意义。
* 计算两个 next-token 分布之间的 **KL 散度**，区分 forward KL 与 reverse KL，并说出某项技术（MLE、蒸馏、变分推断、RL）实际最小化的是哪一个。
* 按 llama.cpp 的方式给一个**量化**评级 —— PPL 比值、平均 KLD、top-token 一致率、token 概率 RMS —— 并解释为什么单看 PPL 是「粗糙的」，而 KL/一致率与感知质量的相关性更好。
* 在知识蒸馏、RLHF 的 KL 惩罚项和投机解码的接受规则中认出**同一个 KL 项** —— 并解释为什么投机解码是*无损的*。

---

## 时效性 / 更新纪律

这里的数学是定论（Shannon 1948；Kullback–Leibler 1951）。会动的是**工具与实践**：

* 量化评估一节（第 05 讲）跟踪当前实践 —— llama.cpp 的 `--kl-divergence` 模式，以及 **2026 年 1 月**的发现（[arXiv 2601.14277](https://arxiv.org/abs/2601.14277)）：量化*格式*比名义位宽更重要，且内在指标（PPL、KLD）是必要的但不充分 —— 你仍要在下游 benchmark 上验证。
* RLHF 的目标在演进（PPO → DPO → GRPO）；**KL 项是不变量**，而本课程教的正是这一点。
* 每讲都以一条 **`## Current as of`** 说明收尾，标出哪些是不变的数学、哪些是 2026 年的工具。

---


<details>
<summary>English original</summary>

**Why this course is structured the way it is**

The five lectures walk the master identity from its raw material to its payoff:

1. **Logprobs** — the raw material. Everything downstream is built from `log q(token)`, so you must know exactly what it is, why it lives in log-space, and how to extract it from any runtime.
2. **Entropy & cross-entropy** — the objective. Cross-entropy *is* the training loss; entropy is the floor it's chasing. This lecture earns the left two terms of the identity.
3. **Perplexity** — the metric. `exp` of cross-entropy, the effective branching factor, and the tokenizer trap that voids most cross-model comparisons.
4. **KL divergence** — the gap. The third term, proven equal to cross-entropy minus entropy, with its asymmetry, its forward/reverse personalities, and its role as the universal "distance from the model I trust."
5. **Applications** — the payoff. The identity turned loose on quantization grading, knowledge distillation, RLHF's KL penalty, and the speculative-decoding acceptance rule, ending in a capstone quant grader.

---

**Course Map (5 lectures)**

<div class="lecture-map" markdown>

| # | Lecture | The thread |
|---|---------|-----------|
| [01](Lecture-01.md) | **Logprobs — what a language model actually emits** — logits → softmax → log-probabilities, why log-space (additivity, stability, log-sum-exp), sequence logprob, temperature, and extracting logprobs from OpenAI / HF / vLLM / llama.cpp | the raw material |
| [02](Lecture-02.md) | **Entropy, Cross-Entropy & NLL** — Shannon entropy, cross-entropy as the training loss, bits vs nats, teacher forcing, and the first sight of `H(p,q) = H(p) + D_KL` | the objective |
| [03](Lecture-03.md) | **Perplexity — the exponential of confusion** — `PPL = exp(mean NLL)`, effective branching factor, bits-per-byte, the tokenizer-comparison trap, sliding-window PPL, and perplexity as the canonical quantization metric | the metric |
| [04](Lecture-04.md) | **KL Divergence — the gap between two distributions** — `D_KL(p‖q)`, Gibbs' inequality, asymmetry, forward vs reverse KL, Jensen–Shannon, and the proof that `D_KL = H(p,q) − H(p)` | the gap |
| [05](Lecture-05.md) | **Where it all lands** — llama.cpp `--kl-divergence` quant grading, knowledge distillation, the RLHF/PPO/GRPO KL penalty, the speculative-decoding acceptance rule, and a capstone quant grader | the payoff |

</div>

---

**Course Outcomes**

By the end you should be able to:

* Extract and interpret **token and sequence logprobs** from any runtime, and explain why log-space (not probability-space) is the right place to do the arithmetic.
* Derive **cross-entropy = entropy + KL divergence** from scratch and explain what each term means physically — and why cross-entropy can never beat entropy.
* Compute **perplexity** correctly with a sliding window, report **bits-per-byte** when tokenizers differ, and explain why a raw PPL comparison across two tokenizers is meaningless.
* Compute **KL divergence** between two next-token distributions, distinguish forward from reverse KL, and say which one a given technique (MLE, distillation, variational inference, RL) actually minimizes.
* Grade a **quantization** the way llama.cpp does — PPL ratio, mean KLD, top-token agreement, token-probability RMS — and explain why PPL alone is "rough" and KL/agreement correlate better with perceived quality.
* Recognize the **same KL term** inside knowledge distillation, the RLHF KL penalty, and the speculative-decoding acceptance rule — and explain why speculative decoding is *lossless*.

---

**Currency / Refresh Discipline**

The mathematics here is settled (Shannon 1948; Kullback–Leibler 1951). What moves is the **tooling and practice**:

* The quantization-evaluation section (Lecture 05) tracks current practice — llama.cpp's `--kl-divergence` mode and the **January 2026** finding ([arXiv 2601.14277](https://arxiv.org/abs/2601.14277)) that quantization *format* matters more than nominal bit-width, and that intrinsic metrics (PPL, KLD) are necessary but not sufficient — you still validate on downstream benchmarks.
* RLHF objectives evolve (PPO → DPO → GRPO); the **KL term is the invariant**, and that is what this course teaches.
* Every lecture closes with a **`## Current as of`** note marking what is timeless math versus 2026 tooling.

---

</details>

## 达成标准

当你拿到一个**刚量化完的模型**，不查任何资料就能做到下面这些事，这门课就算学完了：

* 用正确的滑动窗口计算它在 WikiText-2 上的困惑度，以及它的 bits-per-byte。
* 计算它相对 FP16 参考模型的平均 KL 散度和 top-1 token 一致率。
* 把这三个数字放在一起读，给出一个站得住脚的判断——*可以直接发，还是量化搞坏了什么*——并说出哪个下游测试能证实它。
* 指着 `H(p,q) = H(p) + D_KL(p‖q)` 图，准确说明你的量化动了哪一项。

如果你能背出这些定义，却不能用它们给一个量化结果打分，那你手里只有记号。这门课的重点是判断。

---

*相关：[MLSys 深度解析](../05-MLSys深度探索/README.md) · [Gemma 4 边缘部署](../../03-方向C-边缘AI/04-Gemma-4边缘部署/README.md) · [实用机器学习（CS329P）— 模型压缩](../../../阶段3-人工智能/04-智能体AI与ML工程/02-ML工程与MLOps/01-实用机器学习CS329P/Lecture-10.md) · [阶段 5 — ML 系统工程指南](../Guide.md)*


<details>
<summary>English original</summary>

**Exit Criteria**

You are done with this course when you can take a **freshly quantized model** and, without looking anything up:

* Compute its perplexity on WikiText-2 with a correct sliding window, and its bits-per-byte.
* Compute mean KL divergence and top-1 token agreement against the FP16 reference.
* Read those three numbers together and give a defensible verdict — *ship it, or the quant broke something* — and say which downstream test would confirm it.
* Point at the `H(p,q) = H(p) + D_KL(p‖q)` diagram and explain exactly which term your quantization moved.

If you can quote the definitions but can't grade a quant with them, you have notation. The point of this course is the verdict.

---

*Related: [MLSys Deep Dives](../05-MLSys深度探索/README.md) · [Gemma 4 Edge Deployment](../../03-方向C-边缘AI/04-Gemma-4边缘部署/README.md) · [Practical Machine Learning (CS329P) — Model Compression](../../../阶段3-人工智能/04-智能体AI与ML工程/02-ML工程与MLOps/01-实用机器学习CS329P/Lecture-10.md) · [Phase 5 — ML Systems Engineering Guide](../Guide.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Logprobs, Perplexity and KL Divergence/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Logprobs%2C%20Perplexity%20and%20KL%20Divergence/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
