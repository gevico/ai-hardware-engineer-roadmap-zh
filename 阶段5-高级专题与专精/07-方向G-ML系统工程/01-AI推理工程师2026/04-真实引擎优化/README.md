---
title: 第 4 部分 — 优化一个真实引擎
description: 第 4 部分 — 优化一个真实引擎
---

# 第 4 部分 — 优化一个真实引擎

第 1–3 部分讲的是整个 stack。本部分讲的是一个引擎、一个模型、一个节点，以及 **96 个 pull request** —— 每一条论断都是有人在租来的硬件上测出来的数字，每一个数字都能追溯到产生它的那个 diff。

锚点是 **[SparkInfer-K3](https://github.com/gittensor-ai-lab/sparkinfer-k3)**（MIT）：一个从零开始写的 **Kimi K3** CUDA 推理引擎 —— 总计 2.8T 参数、93 层、**896 个路由专家**、**KDA + MLA** 混合 attention 栈、1M token 上下文 —— 在**单台 8× H200 节点**上以 `UD-IQ1_S` 运行（553 GiB 权重）。

它之所以是有用的案例研究，原因很具体：**它起步很差，而且是在公开场合，并且记录被保留了下来。**

| 8× H200 · UD-IQ1_S · 同一台机器，同一份权重 | llama.cpp | SparkInfer-K3 | |
|---|--:|--:|--:|
| **decode @ 128k**（逐 token 生成阶段），首次测量（2026-08-01） | 18.44 tok/s | **1.01 tok/s** | *落后* 18× |
| **decode @ 128k**，当前 | 18.44 tok/s | **60.17 tok/s** | **领先 3.26×** |
| **prefill @ 32k**（首字前的整段计算），批处理之前 | 143.88 tok/s | 40.35 tok/s | 落后 3.57× |
| **prefill @ 32k**，当前 | 143.88 tok/s | **99.68 tok/s** | 落后 1.44× |

在从未变过的硬件上，大约六周里做出 **60×** 的 decode 提升。没有新芯片、没有新模型、没有降低精度 —— 表格两端用的是同一份权重、同一台机器。而中间发生的一切，正是本部分的主题。

## 为什么用案例研究，以及为什么是这一个

优化类的文章通常都是预先洗过的。论文只报胜利；博客只报胜利；release notes 只报胜利。被删掉的恰恰是你真正需要的那部分：哪次测量是错的、哪个“speedup”其实是个 bug、哪六周花在了后来发现根本不重要的阶段上。

这个仓库把这一切都留了下来，留在工程史通常能存活下来的地方 —— `reference.lock` 注释、revert commit、`eval:none` 标签，以及一份把撤回的数字和真实数字并列记录的 changelog。下面三个例子说明这对读者意味着什么：

* 一个 PR 测出 **169.72 tok/s** 的 prefill，结果被拒了。两个实际存在的缺陷使它读错了行，而读错行反而*更快*。同一改动的诚实数字是 **98.80**。（[Lecture 10](Lecture-10.md)）
* 项目最初几周在上下文 **64** 下给 decode 打分，而自己的文档却声称 128k，原因是 benchmark harness（agent 运行时框架）把 `max_ctx=64` 写死了。在 64 下它落后 llama.cpp 1.8×；在 128k 下它落后 **18×**。好几周的优化都瞄准了一个没人会跑的上下文。（[Lecture 02](Lecture-02.md)）
* 一个 **3× MoE**（混合专家模型）加速反而让 tensor-parallel 扩展性*更差* —— 2.44× → ~1.1× —— 因为它留下的复制式 attention 变成了全部的串行项。Amdahl 定律，还带着凭证。（[Lecture 07](Lecture-07.md)）

这些都不在论文里。而这三件事，恰恰决定了你自己那一个月的优化到底有没有价值。

## 本部分有什么不同

第 1–3 部分钉住的是*教学锚点* —— 代表性硬件上的代表性数字，按节奏刷新。第 4 部分钉住的是*一段被测量过的历史*。这改变了你应该怎么读它：

| | 第 1–3 部分 | 第 4 部分 |
|---|---|---|
| 数字 | 代表性的，会刷新 | 一个节点、一种量化、一个引擎 —— 与 commit 一起冻结 |
| 来源 | model card、论文、厂商文档 | pull request、封存的凭证、`reference.lock` |
| 失效模式 | 被描述 | *被诊断出来，并附上修复它们的 diff* |
| 对读者的要求 | 理解机制 | 在自己的工作负载上复现推理过程 |

技术是可以迁移的。数字不行 —— 它们属于 `sm_90` 上以 IQ1_S 运行的 Kimi K3，把它们引用到任何其他工作负载上，正是 [Lecture 02](Lecture-02.md) 所讲的那个错误。


<details>
<summary>English original</summary>

**Part 4 — Optimizing a Real Engine**

Parts 1–3 taught the stack. This part is one engine, one model, one node, and **96 pull requests** — every claim a number somebody measured on rented hardware, every number traceable to the diff that produced it.

The anchor is **[SparkInfer-K3](https://github.com/gittensor-ai-lab/sparkinfer-k3)** (MIT): a from-scratch CUDA inference engine for **Kimi K3** — 2.8T total parameters, 93 layers, **896 routed experts**, a hybrid **KDA + MLA** attention stack, 1M-token context — running on a **single 8× H200 node** at `UD-IQ1_S` (553 GiB of weights).

It is a useful case study for one specific reason: **it started badly, in public, and the record was kept.**

| 8× H200 · UD-IQ1_S · same box, same weights | llama.cpp | SparkInfer-K3 | |
|---|--:|--:|--:|
| **decode @ 128k**, first measurement (2026-08-01) | 18.44 tok/s | **1.01 tok/s** | 18× *behind* |
| **decode @ 128k**, current | 18.44 tok/s | **60.17 tok/s** | **3.26× ahead** |
| **prefill @ 32k**, before batching | 143.88 tok/s | 40.35 tok/s | 3.57× behind |
| **prefill @ 32k**, current | 143.88 tok/s | **99.68 tok/s** | 1.44× behind |

A **60×** decode improvement, in about six weeks, on hardware that never changed. No new silicon, no new model, no lowered precision — the weights and the box are the same at both ends of that table. Everything in between is the subject of this part.

**Why a case study, and why this one**

Optimization writing usually arrives pre-laundered. The paper reports the win; the blog post reports the win; the release notes report the win. What gets deleted is the part you actually need: which measurement was wrong, which "speedup" was a bug, which six weeks went into the phase that turned out not to matter.

This repository kept all of it, in the places where engineering history normally survives — `reference.lock` comments, revert commits, `eval:none` labels, and a changelog that records retracted numbers alongside real ones. Three examples of what that buys a reader:

* A PR measured **169.72 tok/s** prefill and was rejected. Two live defects made it read the wrong rows, and reading the wrong rows was *faster*. The honest number for the same change was **98.80**. ([Lecture 10](Lecture-10.md))
* The project scored decode at context **64** for its first weeks while its own docs claimed 128k, because a benchmark harness hardcoded `max_ctx=64`. At 64 it was 1.8× behind llama.cpp; at 128k it was **18×** behind. Weeks of optimization were aimed at a context nobody runs. ([Lecture 02](Lecture-02.md))
* A **3× MoE speedup** made tensor-parallel scaling *worse* — 2.44× → ~1.1× — because the replicated attention it left behind became the entire serial term. Amdahl's law, with receipts. ([Lecture 07](Lecture-07.md))

None of those are in a paper. All three are the kind of thing that decides whether your own optimization month was worth anything.

**What is different about this part**

Parts 1–3 pin *teaching anchors* — representative numbers for representative hardware, refreshed on a cadence. Part 4 pins *one measured history*. That changes how you should read it:

| | Parts 1–3 | Part 4 |
|---|---|---|
| Numbers | representative, refreshed | one node, one quant, one engine — frozen with the commits |
| Sourcing | model cards, papers, vendor docs | pull requests, sealed receipts, `reference.lock` |
| Failure modes | described | *diagnosed, with the diff that fixed them* |
| Ask of the reader | understand the mechanism | reproduce the reasoning on your own workload |

The techniques transfer. The numbers do not — they belong to Kimi K3 on `sm_90` at IQ1_S, and quoting them for any other workload is exactly the mistake [Lecture 02](Lecture-02.md) is about.

</details>

## Lectures

<div class="lecture-map" markdown>

| # | 标题 | 核心问题 |
|---|-------|---------------|
| 01 | [工作负载、基线与阶梯](Lecture-01.md) | 单节点上 2.8T 的真实代价是多少，60× 又是一级一级怎么实现的？ |
| 02 | [记分牌 —— 一个无法被钻空子的 benchmark](Lecture-02.md) | 如何在优化*之前*就把测量搭好，让数字在激励面前依然站得住？ |
| 03 | [诊断 —— 受限于 launch、带宽还是通信？](Lecture-03.md) | 引擎距离 roofline（性能上界模型）差 13×。四道上限中哪一道在起约束，怎么证明？ |
| 04 | [Launch geometry —— grid、occupancy 与每 token 327 个 norm](Lecture-04.md) | 为什么板上最便宜的收益都跟 *grid 形状*有关，而不是算术？ |
| 05 | [融合与激活值量化纪律](Lecture-05.md) | 量化激活值什么时候划算，如何在不破坏 bit 一致性的前提下做融合？ |
| 06 | [128k 下的 attention —— 按上下文切分、按 head 切分](Lecture-06.md) | 为什么 decode（逐 token 生成阶段）随深度掉了 90%，而 llama.cpp 纹丝不动？ |
| 07 | [切分 896 个专家 —— 以及随之而来的 Amdahl 陷阱](Lecture-07.md) | 集合通信的开销去了哪里，为什么一次 3× 的 MoE（混合专家模型）收益反而摧毁了 TP 扩展性？ |
| 08 | [图内常驻的 decode —— 一劳永逸地干掉 launch 账单](Lecture-08.md) | 当每 layer 约 30 次 kernel 启动 × 93 layer 不再是启动时，还剩下什么？ |
| 09 | [你忘掉的那个阶段 —— 批处理 prefill（首字前的整段计算）](Lecture-09.md) | 如何在 prompt 摄入每 token 只跑一次前向的情况下，把六周时间花在 decode 上？ |
| 10 | [静默地出错 —— 推理引擎独有的失效模式](Lecture-10.md) | 对于那些能产出流畅文本的 bug，以及那些其实是数据损坏的“加速”，你打算怎么办？ |

</div>

这个顺序是你希望*动手*的顺序，而不是项目发现这些事的顺序。第 01–03 讲是搭建与诊断；04–08 讲是四类优化，大致按实现成本递增排列；09–10 讲是两项纪律，决定前面那些是否算数。

## 前置要求

* **[第 1 部分第 03 讲 —— Roofline、带宽与存储层次](../01-基础/Lecture-03.md)** —— 这里的第 03 讲从头到尾是一个 roofline 论证，并假定你能读懂一份。
* **[第 2 部分第 04 讲 —— 8× Hopper 上的张量并行](../02-Hopper稠密计算/Lecture-04.md)** 与 **[第 07 讲 —— 通信层内部](../02-Hopper稠密计算/Lecture-07.md)** —— 这里的第 07 讲是两者的 MoE 变体，带实测的集合通信延迟。
* **[第 3 部分第 01 讲 —— 现代 MoE 剖析](../03-Blackwell-MoE/Lecture-01.md)** 与 **[第 03 讲 —— 专家并行](../03-Blackwell-MoE/Lecture-03.md)** —— 用于 MLA 与专家路由。
* **[对数概率、困惑度与 KL 散度](../../04-Logprobs-困惑度与KL散度/README.md)** —— 本部分的准确率门禁是一道 mean-KL 与 top-1 一致率的门槛。那门课推导了这两个量。
* 能舒服地读 CUDA C++。跟着论证走不需要你写 kernel，但 diff 是 CUDA，有意思的那些都直接引出来了。

## 第 4 部分你要交付什么

第 4 部分的产物刻意*不是*“重新实现 SparkInfer-K3”，而是把这套纪律应用到你自己的一个工作负载上：

1. **先有记分牌。** 针对你自己的一个模型 + runtime + 硬件目标：一个锁定的参考、一个同机交错的 harness（agent 运行时框架）、一道在速度门禁*之前*跑的正确性门禁，以及一个只允许上调的前沿文件。在这套东西存在之前，不做任何优化。（[第 02 讲](Lecture-02.md)）
2. **一份诊断，每道上限配一个数字。** 带宽上限、launch 开销估计、集合通信开销占 step 时间的百分比、实测 occupancy 与峰值的对比。说明是哪一道在起约束，以及你预测改动它能换来什么。（[第 03 讲](Lecture-03.md)）
3. **一个优化阶梯。** 至少五项改动，每项一个独立 commit，每项都有同机测得的 before → after，每项都标上它自己的档位 —— 包括那些测出来是 `none` 的。`none` 那些行才是重点；没有失败项的阶梯是被编辑过的阶梯。
4. **一份正确性日志。** 每项改动在你参考上的最差深度 KL 与 top-1。任何声称 bit 一致性的改动，都已证明是 bit 一致的。（[第 10 讲](Lecture-10.md)）
5. **一份复盘。** 一页：你*以为*卡住的是哪道上限，实际卡住的是哪道，最大的一笔单项收益是什么，以及起步时你会换个什么测法。引用你自己的 `none` 与回滚。

产物阶梯目标：**Level 4–5** —— 一份带原始数据的测量报告，外加一个别的工程师能直接跑的可复用 harness。


<details>
<summary>English original</summary>

**Lectures**

<div class="lecture-map" markdown>

| # | Title | Core question |
|---|-------|---------------|
| 01 | [The workload, the baseline, and the ladder](Lecture-01.md) | What does 2.8T on one node actually cost, and what did 60× look like rung by rung? |
| 02 | [The scoreboard — a benchmark that cannot be gamed](Lecture-02.md) | How do you build the measurement *before* the optimization, so the numbers survive contact with incentives? |
| 03 | [Diagnosis — launch-bound, bandwidth-bound, or comm-bound?](Lecture-03.md) | The engine is 13× off roofline. Which of the four ceilings is binding, and how do you prove it? |
| 04 | [Launch geometry — grids, occupancy, and 327 norms per token](Lecture-04.md) | Why were the cheapest wins on the board about *grid shape*, not arithmetic? |
| 05 | [Fusion and the activation-quantization discipline](Lecture-05.md) | When does quantizing an activation pay, and how do you fuse without breaking bit-identity? |
| 06 | [Attention at 128k — split over context, split over heads](Lecture-06.md) | Why did decode fall 90% with depth while llama.cpp stayed flat? |
| 07 | [Sharding 896 experts — and the Amdahl trap that followed](Lecture-07.md) | Where does the collective go, and why did a 3× MoE win destroy TP scaling? |
| 08 | [Graph-resident decode — killing the launch bill for good](Lecture-08.md) | What is left when ~30 kernel launches per layer × 93 layers stop being launches? |
| 09 | [The phase you forgot — batched prefill](Lecture-09.md) | How do you spend six weeks on decode while prompt ingestion runs one forward per token? |
| 10 | [Silently wrong — the failure mode unique to inference engines](Lecture-10.md) | What do you do about bugs that produce fluent text, and "speedups" that are corruption? |

</div>

The order is the order you would want to *work* in, not the order the project discovered things in. Lectures 01–03 are setup and diagnosis; 04–08 are the four optimization families, roughly in increasing cost-to-implement; 09–10 are the two disciplines that decide whether any of it counted.

**Prerequisites**

* **[Part 1 Lecture 03 — Roofline, bandwidth, and the memory hierarchy](../01-基础/Lecture-03.md)** — Lecture 03 here is a roofline argument end to end and assumes you can read one.
* **[Part 2 Lecture 04 — Tensor parallelism on 8× Hopper](../02-Hopper稠密计算/Lecture-04.md)** and **[Lecture 07 — Inside the communication layer](../02-Hopper稠密计算/Lecture-07.md)** — Lecture 07 here is the MoE variant of both, with measured collective latencies.
* **[Part 3 Lecture 01 — Anatomy of a modern MoE](../03-Blackwell-MoE/Lecture-01.md)** and **[Lecture 03 — Expert parallelism](../03-Blackwell-MoE/Lecture-03.md)** — for MLA and expert routing.
* **[Logprobs, Perplexity & KL Divergence](../../04-Logprobs-困惑度与KL散度/README.md)** — the accuracy gate in this part is a mean-KL and top-1-agreement bar. That course derives both.
* Comfort reading CUDA C++. You will not write kernels to follow the argument, but the diffs are CUDA and the interesting ones are quoted.

**What you ship from Part 4**

Part 4's artifact is deliberately *not* "reimplement SparkInfer-K3." It is the discipline, applied to a workload you own:

1. **A scoreboard first.** For one model + runtime + hardware target of yours: a pinned reference, a same-box interleaved harness, a correctness gate that runs *before* the speed gate, and a frontier file that is raise-only. No optimization until this exists. ([Lecture 02](Lecture-02.md))
2. **A diagnosis, with a number per ceiling.** Bandwidth ceiling, launch-overhead estimate, collective cost as a percentage of step time, achieved-vs-peak occupancy. State which one binds and what you predict changing it will buy. ([Lecture 03](Lecture-03.md))
3. **An optimization ladder.** At least five changes, each its own commit, each with a before → after measured on the same box, each labeled with its own tier — including the ones that measured `none`. The `none` rows are the point; a ladder with no failures is a ladder that was edited.
4. **A correctness log.** Every change's worst-depth KL and top-1 against your reference. Any change claiming bit-identity, proved bit-identical. ([Lecture 10](Lecture-10.md))
5. **A retrospective.** One page: which ceiling you *thought* was binding, which actually was, what the biggest single win was, and what you would have measured differently at the start. Cite your own `none`s and reverts.

Artifact ladder target: **Level 4–5** — a measurement report with raw data plus a reusable harness another engineer can run.

</details>

## 达成标准

当你能够做到以下各项时，Part 4 就算完成了：

* 拿到任何一条优化声明——一个 PR、一篇论文、一张厂商幻灯片——在讨论技术本身之前，先说出**测量可能美化自己的三种方式**。
* 看一份 decode 剖析，判断约束上限是带宽、启动开销、occupancy 还是集合通信——并说出能一锤定音的*那一个*测量。
* 解释为什么某个阶段 3× 的提升反而可能让端到端扩展变差，并用 Amdahl 定律算出交叉点。
* 从第一性原理出发，为张量并行 MoE 层中的一个 reduce 点辩护——包括为什么把它往后挪两个 op 会让输出乘以 `tp_size` 且不会崩溃。
* 描述三种会产生**流畅、貌似合理但错误**输出的推理引擎 bug，以及能捕获每一种的断言。
* 针对你自己的产物，说明其中哪些你测出的收益，即使有人带着敌意来审计，你依然会相信。

如果这六项你都能做到，本部分的可迁移内容就已经落地。如果你只能背出 SparkInfer-K3 的数字，那就还没落地——那些数字是 2026-08 的往事，只对应一个模型、一台机器，它们唯一的职责是让推理过程变得具体。


<details>
<summary>English original</summary>

**Exit criteria**

You are done with Part 4 when you can:

* Take any optimization claim — a PR, a paper, a vendor slide — and name **three ways the measurement could be flattering itself** before you argue about the technique.
* Look at a decode profile and say whether the binding ceiling is bandwidth, launch overhead, occupancy, or collectives — and name the *one* measurement that would settle it.
* Explain why a 3× improvement to one phase can make end-to-end scaling worse, and compute the crossover from Amdahl's law.
* Defend a reduce point in a tensor-parallel MoE layer from first principles — including why moving it two ops later multiplies your output by `tp_size` and does not crash.
* Describe three inference-engine bugs that produce **fluent, plausible, wrong** output, and the assertion that catches each one.
* State, for your own artifact, which of your measured wins you would still believe if someone hostile audited it.

If you can do all six, the transferable content of this part has landed. If you can only recite the SparkInfer-K3 numbers, it has not — those numbers are 2026-08 history for one model on one box, and their only job is to make the reasoning concrete.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
