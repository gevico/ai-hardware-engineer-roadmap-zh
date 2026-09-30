---
title: 第 4 部分 · 第 02 讲 — 记分板：一个无法被操纵的 benchmark
description: 第 4 部分 · 第 02 讲 — 记分板：一个无法被操纵的 benchmark
---

# 第 4 部分 · 第 02 讲 — 记分板：一个无法被操纵的 benchmark

## 概述

这是大多数优化课程都会跳过的一讲，而它恰恰决定了其余工作是否算数。

案例仓库里大约有 **四十个已合并的 pull request，其唯一目的就是修正度量**。不是修正引擎 —— 而是修正*尺子*。每一个都堵住了一种具体的途径：让某个数字看上去比真实情况更好。这个比例并不说明 harness（agent 运行时框架）造得差；它说明的是：当一个 benchmark 被置于真实的激励压力之下，且失效被记录下来而不是被悄悄补上时，会发生什么。

读完这一讲，你应该能为自己的工作负载设计一道性能门禁，让它扛得住三类不同的对手：**把自己骗过去的诚实工程师**、**针对指标做优化的敌对贡献者**，以及**硬件本身的非确定性**。这三类需要不同的防御手段，把它们混为一谈，正是大多数内部 benchmark 沦为摆设的原因。

---

## 1. 为什么记分板要先行

直觉上的顺序是：先把东西造出来，再去测量它。这个顺序会失败，原因与懒惰无关。

**benchmark 就是一份规格说明，规定了你会去优化什么。** 每一小时的工程注意力都流向板上的那个数字。如果数字错了，注意力也就错了 —— 而且你无法从数字本身发现这一点，因为数字只会往上走。

案例研究中最典型的例子来自 [第 01 讲](Lecture-01.md)：

```text
   the harness measured        ctx 64
   the slot was named          KIMI_K3_H200X8_IQ1S_SPARKINFER_128
   the contribution guide said 128k
   the hardware saw            64

                        ctx 64      ctx 131,072
   llama.cpp             18.32          18.44
   sparkinfer            10.34           1.00      <- the real gap
                         1.8x            18x
```


数周的 kernel 工作，是在没人会跑的上下文下被评分的。引擎没有说谎，其他人也没有 —— 三个产物互相不一致，而其中没有任何一个是权威的。[PR #51](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/51)（“*在 128k 下评分，这才是本仓库实际瞄准的上下文*”）修好了它，前沿随即从 9.63 *下降到* 1.00，因为它开始测量真正的东西了。

> **修好 harness 时前沿反而下降，说明 harness 起作用了。** 如果修正一次度量从不让你失去一个你喜欢的数字，那你就没有修正度量。

先建记分板还有第二个更微妙的原因：**你无法事后给一架梯子打分。** 仓库之所以能逐行审计自己的历史，只是因为在每一行发生时就把它封存了。事后凭记忆和重跑来重建 46 级台阶，在你已经不再持有的租用硬件上，是做不到的。

---

## 2. 门禁顺序 —— 正确性*先于*速度

这个 harness 中最重要的结构性决策，就是两项检查的先后顺序。

```text
   kimi_k3_eval.sh
     │
     ├─ 1. ACCURACY GATE   top-1 ≥ 0.95   AND   mean KL ≤ 0.05
     │      │                     against the captured reference,
     │      │                     identical weights, identical token ids
     │      └─ fail → REJECT.  the speed number is never even reported.
     │
     ├─ 2. SIGNIFICANCE GATE   gain > 2% of the current frontier
     │      └─ fail → label `none`  (inside measurement noise)
     │
     └─ 3. TIER   min(delta / llama_ref, delta / frontier)  →  xs | s | m | l | xl
```


规则说得很直白：*侵蚀精度一致性的加速不算加速。* 一个更快但改变了模型输出的 kernel，价值是 **零**，而 harness 的执行方式是直接拒绝计算 tier，而不是在某条曲线上拿准确率与速度做权衡。

这一点对推理引擎的重要性，几乎超过其他任何软件，因为 —— 正如 [第 10 讲](Lecture-10.md) 详细展开的那样 —— 坏掉的推理 kernel，其自然失效模式是**流畅、看上去合理、但错误的输出**。没有任何崩溃会拦住你。如果速度门禁先跑，数据损坏会被读成一次胜利，而你扫描中最快的配置就是坏得最厉害的那个。这在这里并非假设：[第 10 讲 §3](Lecture-10.md) 讲到一个 PR，它测出 **169.72 tok/s**，原因是两个现存的缺陷让它读错了行。


<details>
<summary>English original</summary>

**Part 4 · Lecture 02 — The Scoreboard: A Benchmark That Cannot Be Gamed**

**Overview**

This is the lecture most optimization courses skip, and it is the one that decides whether the rest of the work counted.

The case-study repository has roughly **forty merged pull requests whose only purpose was to fix the measurement**. Not the engine — the *ruler*. Each closed one specific way a number could look better than reality. That ratio is not a sign of a badly-built harness; it is what happens when a benchmark is put under real incentive pressure and the failures are recorded instead of quietly patched.

By the end of this lecture you should be able to design a performance gate for your own workload that survives three distinct adversaries: **an honest engineer fooling themselves**, **a hostile contributor optimizing for the metric**, and **the hardware itself being non-deterministic**. Those need different defenses, and confusing them is why most internal benchmarks are decorative.

---

**1. Why the scoreboard comes first**

The intuitive order is: build the thing, then measure it. That order fails for a reason that has nothing to do with laziness.

**A benchmark is a specification of what you will optimize.** Every hour of engineering attention flows toward the number on the board. If the number is wrong, the attention is wrong — and you will not find out from the number, because the number will be going up.

The case study's canonical instance, from [Lecture 01](Lecture-01.md):

```text
   the harness measured        ctx 64
   the slot was named          KIMI_K3_H200X8_IQ1S_SPARKINFER_128
   the contribution guide said 128k
   the hardware saw            64

                        ctx 64      ctx 131,072
   llama.cpp             18.32          18.44
   sparkinfer            10.34           1.00      <- the real gap
                         1.8x            18x
```

Weeks of kernel work were graded at a context nobody runs. The engine was not lying and neither was anyone else — three artifacts disagreed and none of them was authoritative. [PR #51](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/51) ("*score at 128k, the context this repo actually targets*") fixed it, and the frontier promptly *dropped* from 9.63 to 1.00 because it had started measuring the real thing.

> **A frontier that drops when you fix the harness is the harness working.** If correcting a measurement never costs you a number you liked, you have not corrected a measurement.

There is a second, subtler reason to build the scoreboard first: **you cannot retroactively grade a ladder.** The repo can audit its own history row by row only because each row was sealed when it happened. Reconstructing 46 rungs afterwards from memory and re-runs is not possible on rented hardware you no longer hold.

---

**2. Gate order — correctness *before* speed**

The single most important structural decision in this harness is the order of two checks.

```text
   kimi_k3_eval.sh
     │
     ├─ 1. ACCURACY GATE   top-1 ≥ 0.95   AND   mean KL ≤ 0.05
     │      │                     against the captured reference,
     │      │                     identical weights, identical token ids
     │      └─ fail → REJECT.  the speed number is never even reported.
     │
     ├─ 2. SIGNIFICANCE GATE   gain > 2% of the current frontier
     │      └─ fail → label `none`  (inside measurement noise)
     │
     └─ 3. TIER   min(delta / llama_ref, delta / frontier)  →  xs | s | m | l | xl
```

The rule stated plainly: *a speedup that erodes parity is not a speedup.* A faster kernel that changes the model's output is worth **zero**, and the harness enforces that by refusing to compute a tier at all rather than by trading accuracy against speed on some curve.

This matters more for inference engines than for almost any other software, because — as [Lecture 10](Lecture-10.md) develops at length — the natural failure mode of a broken inference kernel is **fluent, plausible, wrong output**. There is no crash to stop you. If the speed gate runs first, corruption reads as a win, and the fastest configuration in your sweep is the most broken one. That is not hypothetical here: [Lecture 10 §3](Lecture-10.md) covers a PR that measured **169.72 tok/s** because two live defects made it read the wrong rows.

</details>

### 2.1 没有第二份实现时，「正确性」意味着什么

不存在可供对照的 Kimi K3 独立重实现，而目标量化相对全精度的*自身* top-1 只有 90.4%。因此绝对的质量数字在这里没有意义。正确性的定义被刻意收窄，且足够诚实：

> **在相同权重、相同 token id 下与参考引擎一致。** 仅此而已。

这一定义带来实实在在的后果，repo 选择记录下来而不是藏起来：

* **两个 reference-server flag 是强制项，不是调参项。** `--no-context-shift`，因为 K3 是混合循环架构，llama.cpp 无法对其做 context-shift —— 没有它，长 eval 会中途挂掉。以及 `--no-jinja`，因为 gate 提交的是原始 token id，而 chat template 会前置一些候选模型从未见过的 token。
* **残余 KL 是已知且被接受的。** 平均 KLD 比*同一*实现两次运行应有的 1e-5 门槛高出约 400×，原因已定位：K3 保留 f32 激活值，而 ggml 会在量化 mat-vec 之前把它们量化。给贡献者的指示完全正确 —— *不要把它当 bug 去追查；也不要让它变得更糟。*
* **gate 无法在评分所用的上下文长度上运行。** 在 131,072 token 上抓取参考值代价高得不可接受，因此精度一致性只在较浅深度上评分。未测试的区域被明确讲了出来（§7）。

### 2.2 Top-1 与 KL 各司其职

有个值得内化的细节，因为它能推广到你搭建的任何精度一致性 gate：

| | 是什么 | 行为 |
|---|---|---|
| **top-1 ≥ 0.95** | 每个探测点在单行 logit 上的 `argmax_ref == argmax_ours` | 实际上是个 **boolean**。任意 (0, 1] 内的门槛表现完全一致。封存日志里全部 48 个 top-1 值都正好是 `1.0`。 |
| **mean KL ≤ 0.05** | 对全部 163,840 个 vocab 条目求和 | 是**有梯度**的 gate。在 argmax 翻转之前很早就开始变化。已合并的运行停在 **0.004–0.008**。 |

所以这两者并不冗余：top-1 回答*决策是否改变*，KL 回答*分布漂移了多远*，而给出预警的是 KL。已合并的运行比门槛低一个数量级，这正是健康余量该有的样子 —— 也正是 [PR #147](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/147) 能把 top-1 门槛提到 0.95 而不弄坏任何东西的原因。

这套机制在 [Logprobs, Perplexity & KL Divergence — Lecture 05](../../04-Logprobs-困惑度与KL散度/Lecture-05.md) 中从第一性原理推导而来。如果觉得这个配对很随意，先读那篇。

### 2.3 按最差深度评分，而不是取平均

gate 探测**七个上下文深度** —— 4、128、256、512、1024、2048、4096 —— 它们是同一篇文档的嵌套前缀，并取**最差**值而非平均值。

在 [PR #116](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/116) 之前，它只是单个 4-token prompt，基于一个 next-token 分布评分。那样的 gate 无法区分「正确」与「只在 KV cache 近乎为空时才正确」—— 而这恰恰是 KV cache、attention、routing 或 LM-head 改动会引入的那类 bug。

贡献指南明确写出两点后果：

* 只要有一个深度出现回退，即使其他所有深度都完美，gate 也会失败。
* 此外，精度一致性还会**按深度与同一轮测得的 `main` 做棘轮式收紧**（[PR #83](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/83)）—— 所以即使在合法范围内，漂移也是可见的。

接着是对这套棘轮机制的修正，这是关于过度收紧的一课：[PRs #124](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/124) 和 [#126](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/126) 确立了**低于 0.05 门槛时，比值永远不算回退。** 把 KL 从 0.001 推到 0.004 的改动是 4× 的「更差」，但仍在门槛以内四十倍。把它标记出来只会训练贡献者忽略这个标记。最终规则：`accuracy-regression` 只在某个深度既 ≥2× main **且**达到或超过门槛时才适用 —— 而此时绝对检查本来就会拒绝。

> **直接抄走。** 棘轮式 gate 需要一个绝对下限，低于它比值一概忽略，否则它产生的噪声会与你做得有多好成正比。

---


<details>
<summary>English original</summary>

**2.1 What "correctness" means when there is no second implementation**

There is no independent reimplementation of Kimi K3 to check against, and the target quantization's *own* top-1 against full precision is only 90.4%. So absolute quality numbers are meaningless here. Correctness is defined narrowly and honestly:

> **Agreement with the reference engine on identical weights and identical token ids.** Nothing else.

That definition has real consequences the repo documents rather than hides:

* **Two reference-server flags are mandatory, not tuning.** `--no-context-shift`, because K3 is a hybrid recurrent architecture that llama.cpp cannot context-shift — a long eval dies mid-run without it. And `--no-jinja`, because the gate posts raw token ids and a chat template would prepend tokens the candidate never saw.
* **The residual KL is known and accepted.** Mean KLD sits ~400× above the 1e-5 bar you would expect from two runs of the *same* implementation, from an identified cause: K3 keeps f32 activations where ggml quantizes them before a quantized mat-vec. The instruction to contributors is exactly right — *do not go hunting it as a bug; do not make it worse.*
* **The gate cannot run at the scored context.** Capturing a reference at 131,072 tokens is prohibitively expensive, so parity is graded at short depths. The untested region is stated out loud (§7).

**2.2 Top-1 and KL do different jobs**

A subtlety worth internalizing, because it generalizes to any parity gate you build:

| | What it is | Behaviour |
|---|---|---|
| **top-1 ≥ 0.95** | `argmax_ref == argmax_ours` on one logit row per probe | Effectively **boolean**. Any bar in (0, 1] behaves identically. All 48 top-1 values in the sealed log are exactly `1.0`. |
| **mean KL ≤ 0.05** | Sum over all 163,840 vocab entries | The **graded** gate. Moves long before an argmax flips. Merged runs sit at **0.004–0.008**. |

So the pair is not redundant: top-1 says *did the decision change*, KL says *how far is the distribution drifting*, and KL is the one that gives you warning. Merged runs sitting an order of magnitude under the bar is what a healthy margin looks like — and is why [PR #147](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/147) could raise the top-1 bar to 0.95 without breaking anything.

This is the mechanism developed from first principles in [Logprobs, Perplexity & KL Divergence — Lecture 05](../../04-Logprobs-困惑度与KL散度/Lecture-05.md). If the pairing feels arbitrary, read that first.

**2.3 Grade the worst depth, not the average**

The gate probes **seven context depths** — 4, 128, 256, 512, 1024, 2048, 4096 — as nested prefixes of one document, and takes the **worst**, not the mean.

Until [PR #116](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/116) it was a single 4-token prompt graded on one next-token distribution. That gate could not distinguish "correct" from "correct only while the KV cache is nearly empty" — which is precisely the bug class that a KV-cache, attention, routing, or LM-head change introduces.

Two consequences the contribution guide spells out:

* A regression at one depth fails the gate even if every other depth is perfect.
* Parity is additionally **ratcheted against `main` measured in the same round, per depth** ([PR #83](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/83)) — so drift is visible even while it is legal.

And then the correction to that ratchet, which is a good lesson in over-tightening: [PRs #124](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/124) and [#126](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/126) established that **below the 0.05 bar, a ratio is never a regression.** A change that moves KL from 0.001 to 0.004 is 4× "worse" and still forty times inside the bar. Flagging it trains contributors to ignore the flag. The final rule: `accuracy-regression` applies only when a depth is both ≥2× main **and** at or over the bar — at which point the absolute check rejects anyway.

> **Steal this.** A ratcheting gate needs an absolute floor below which ratios are ignored, or it generates noise proportional to how good you have become.

---

</details>

## 3. 在不会静止的硬件上测量速度

正确性是确定性的：固定的权重、固定的输入、greedy decode（逐 token 生成阶段）⇒ 精确，靠重跑来验证。速度则不是。时钟会漂移，散热条件会变化，机器各不相同。因此这道关卡的两半需要完全不同的信任模型，仓库把它们明确分开。

以下技术，每一项都可追溯到促成它的那次失败：

**把两个构建交错跑在同一台机器上。** `main` 与 PR 在同一节点上构建并跑 benchmark，交错进行，按 **同机 delta** 计分，这样机器与机器之间的硬件差异就被抵消了。差异有多大？仓库记录到，换机后 `main` 的读数为 **18.14 pinned vs 18.88 measured** —— 几个百分点，比一个 `S` 档位还大。

**绝不跨机对比。** llama.cpp 的 pin 原本是 **16.7026**，是在另一台机器上跑的单次 rep。修正为 **18.4435** —— 在为每个 PR 打分的机器上取 3 次 rep 的中位数（[PR #101](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/101)）。`reference.lock` 中的说明准确描述了当时发生的事：*“任何人的实测速度都没有变；错的是那把尺子。”* 由于档位基准是 `delta / llama_ref`，把它抬高让每个档位都缩小了约 9.4%。

**单次 rep 算不上一次测量。** 同一份 lock 文件记录了一条“随深度 −8% 的 decode 衰减”，结果证明是*测量假象* —— 在另一台机器上的一次 rep（`llama-bench -r 1`）。在打分机器上用 3 次 rep 重新测量后，llama.cpp 随深度基本保持平坦（18.32 → 18.44）。这个假象一直在说好话：它让参考实现看起来随上下文退化，而真正断崖式下跌的是候选实现。

**预热页缓存。** [PR #92](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/92) —— *“预热页缓存，这样前沿就不会在冷机器上被测量。”* 从冷磁盘加载 553 GiB 与从热缓存加载相比绝非小效应，而且哪个构建先跑，这笔账就由它吃掉。

**用固定版本的编译器从头重建。** [PR #93](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/93)。复用构建目录意味着你有一部分测量的是上一次运行的产物。

**wall clock 是上界，不是读数。** [PR #61](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/61)，并由 [#68](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/68) 和 [#121](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/121) 细化，后者把余量按观测到的 jitter 来定。总 wall time 可以*证伪*一个上报的 tok/s —— 声称的工作量超过已用时间所能允许的，就是不可能的 —— 但它不能*成为*读数，因为它包含加载、预热和收尾。

**把不稳定节点与性能回归区分开。** [PRs #118](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/118) 和 [#82](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/82)：一台繁忙或垂死的机器必须触发*带诊断的重试*，而不是判为一轮失败，也不是记成一次回归。否则基础设施噪声就会作为工程历史进入阶梯，未来每个读者都得猜哪些行是真的。

---

## 4. 档位函数，以及它为什么有两个分母

过了 2% 显著性关卡之后，标签是：

```text
   tier_basis = min( delta / llama_ref ,  delta / frontier )

   xs  < 3.5%        s  3.5–6%        m  6–10%        l  10–18%        xl  > 18%
```

取两个基准中**更差**的那个，意味着档位**绝不会超过实际测得的加速比**。这两项在相反的区间里起作用，而这就是整个设计：

| 区间 | 哪一项更小 | 它防止什么 |
|---|---|---|
| 前沿 **低于** 参考实现 | `delta / llama_ref` | 不成熟的引擎从低垂的果实里铸出一个个 `xl`。把 1 tok/s 翻倍是 100% 的增益，却只有参考实现的约 5%。 |
| 前沿 **超过** 参考实现 | `delta / frontier` | 领先之后档位变得廉价。领先 2.2× 时，`xl` 要实打实付出 **比 `main` 高 18%** 的代价 —— 而不是已经击败的参考实现 `main` 的 18%。 |

它所取代的那种失败很有教益。档位得分过去被限制在实测增益的*两倍* —— 当引擎落后时这看不出来，可一旦前沿超过参考实现的 2×，它就变成了唯一的规则，**`xl` 要付出 9% 的代价**。[PR #122](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/122) 把得分上限设为实测加速比，并**依据密封的回执对整个阶梯重新打分。** 能够正确地做这种回溯性重打分，就是为每一轮封存所换来的回报。


<details>
<summary>English original</summary>

**3. Measuring speed on hardware that will not sit still**

Correctness is deterministic: fixed weights, fixed inputs, greedy decode ⇒ exact, and you verify by re-running. Speed is not. Clocks drift, thermals vary, boxes differ. The two halves of the gate therefore need entirely different trust models, and the repo separates them explicitly.

The techniques, each traceable to the failure that motivated it:

**Interleave both builds on the same box.** `main` and the PR are built and benched on the same node, interleaved, and scored as a **same-box delta**, so box-to-box hardware variance cancels. How much variance? The repo records `main` reading **18.14 pinned vs 18.88 measured** when the box changed — several percent, which is larger than an `S` tier.

**Never cross-compare boxes.** The llama.cpp pin was originally **16.7026**, a single rep on a different machine. Corrected to **18.4435** — 3-rep median on the box that scores every PR ([PR #101](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/101)). The note in `reference.lock` is exact about what happened: *"Nothing about anyone's measured speed changed; the yardstick was wrong."* Because the tier basis is `delta / llama_ref`, raising it made every tier ~9.4% smaller.

**A single rep is not a measurement.** The same lock file records a "−8% decode falloff with depth" that turned out to be *a measurement artefact* — one rep (`llama-bench -r 1`) on a different box. Re-measured with 3 reps on the scoring box, llama.cpp holds essentially flat with depth (18.32 → 18.44). The artefact had been flattering: it made the reference look like it degraded with context when it was the candidate that fell off a cliff.

**Warm the page cache.** [PR #92](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/92) — "*warm the page cache so the frontier is not measured on a cold box*." Loading 553 GiB from cold disk versus warm cache is not a small effect, and whichever build runs first eats it.

**Rebuild from scratch with a pinned compiler.** [PR #93](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/93). A reused build directory means you are partly measuring the previous run's artifacts.

**Wall clock is a bound, not the reading.** [PR #61](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/61), refined by [#68](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/68) and [#121](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/121) which sized the margin to the observed jitter. Total wall time can *falsify* a reported tok/s — a claim implying more work than the elapsed time allows is impossible — but it cannot *be* the reading, because it includes load, warmup, and teardown.

**Distinguish a flaky node from a regression.** [PRs #118](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/118) and [#82](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/82): a busy or dying box must cause a *retry with a diagnosis*, not a failed round and not a recorded regression. Otherwise infrastructure noise enters the ladder as engineering history, and every future reader has to guess which rows are real.

---

**4. The tier function, and why it has two denominators**

Past the 2% significance gate, the label is:

```text
   tier_basis = min( delta / llama_ref ,  delta / frontier )

   xs  < 3.5%        s  3.5–6%        m  6–10%        l  10–18%        xl  > 18%
```

Taking the **worse** of two bases means the tier **can never exceed the speedup actually measured**. The two terms bind in opposite regimes, and that is the entire design:

| Regime | Which term is smaller | What it prevents |
|---|---|---|
| Frontier **below** the reference | `delta / llama_ref` | An immature engine minting `xl`s from low-hanging fruit. Doubling 1 tok/s is a 100% gain and ~5% of the reference. |
| Frontier **past** the reference | `delta / frontier` | Cheap tiers once you lead. At 2.2× ahead, `xl` costs a real **18% over `main`** — not 18% of a reference `main` already beat. |

The failure this replaced is instructive. Tier credit used to be capped at *twice* the measured gain — invisible while the engine was behind, and then, once the frontier passed 2× the reference, it became the whole rule and **`xl` was costing 9%**. [PR #122](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/122) capped credit at the measured speedup and **re-scored the entire ladder from the sealed receipts.** Being able to do that retroactive re-score, correctly, is the payoff for sealing every round.

</details>

### 4.1 何时关闭分母

当计分指标换到 prefill（首字前的整段计算）后（[Lecture 09](Lecture-09.md)），llama 锚点被**关闭**，tier 就只剩 `delta / frontier`。这个推理值得仔细跟进，因为它是一个真正不显然的建模要点。

tier 分桶是*相对于参考值的比例*。llama.cpp 的两个数字**相差 7.8×**——18.44 decode（逐 token 生成阶段）对 143.88 prefill。所以如果继续开着锚点，一档 tier 在 prefill 上的代价会比在 decode 上**高 2.7×**：一个 `l` 需要比 frontier 高出 +27.1%，而不是 +10.0%。

> prefill 的工作本身没有任何地方难 2.7×。那个数字来自 llama.cpp 的形态，不是来自我们的。

还有第二个原因：当时 llama.cpp 对 prompt 做*批处理*，而 SparkInfer 逐个 token 地走，所以 `delta / 143.88` 衡量的收益对应的是**一个尚未构建的特性**，而不是 PR 里的工作。锚点计划在两个引擎做同样的事情之后重新开启——到那时它才重新名副其实。

`pct_of_llama` 仍然在每次运行中被记录；它不再充当 tier 的依据，但仍是目标。verdict JSON 里**两者都有**——`pct_over_frontier`（诚实测得的加速比）、`pct_of_llama`（tier 依据）和 `scored_context`（是哪个 context 挣来的）。

---

## 5. frontier——一个有归属、有来源的数字

frontier 是在 `main` 上由封存轮次测出的最佳数字。四个属性，每一个都是挣来的：

**只升不降。** 这样一台慢机器无法把它压低，从而为后面所有人凭空造出 tier。

**每轮重新测量，不信任 pin**（[PR #50](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/50)）。pin 是某人对 `main` 做过一次的断言。

**绝不手工填写。** `check_reference_lock.py` 是一个必需的 CI job：`0` 表示“未测量”，始终允许，但**任何非零 baseline 都必须能追溯到同一 node *和* 同一 context 的、已提交的测量 JSON**。该仓库对此的论证是其文档中关于这个主题最犀利的一句话：

> *在下游看来，手工填写的 baseline 与实测的 baseline 无法区分。*

**按名称限定 quant 和 node。** 是 `KIMI_K3_H200X8_IQ1S_LLAMA_128K`，不是 `LLAMA_128K`。三个里程碑 node 在同一 context 下跑同一 quant，所以单个共享 slot 会悄悄把某个里程碑的参考值覆盖成另一个的（[PR #24](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/24) 修复了读取未限定 slot 的问题）。

### 5.1 陈旧 pin 的教训，两次——以及没人预料到的不对称

一个陈旧的 frontier 造成了两个*不同*的问题，而两者之间的差异才是真正的教训。

**作为 tier 依据时，偏低且陈旧是自纠正的。** 它会按差距大小给下一个 PR 打高分，下一轮重新测量就会修正。烦人，但有界。

**作为回归护栏时，偏低且陈旧是危险的。** decode 护栏的下限是 pin 值之下 1%。slot 读数是 **46.48**，而 `main` 实际跑到约 56.8——所以它会放行 decode 一路跌到 **46.02**，即 **19% 的回归**，同时还欢快地报告护栏通过。[PR #134](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/134) 正是出于这个原因手工把它设为 56.8，并记录了这么做的风险（slot 只升不降，所以若 `main` 真的测到下限之下，在有人介入之前每个 PR 都过不了护栏）。

> **同一个数字扮演两种角色，就需要两种不同的陈旧容忍度。** 依据想要的是*最新*；护栏想要的是*朝安全方向保守*。弄清楚你的属于哪一种。

**而偏低且陈旧作为已发布目标时代价高昂。** prefill frontier 被手工种子设为 40.35，并归到了错误的 commit 上。bot 每轮总是测量真实的 frontier，所以没有任何评分错误——但三个 PR 针对*已发布的* 40.35 做优化，报告了 +25.4%、+24.5%、+5.3%，而相对真实的 53.02 其实是 **−4.6%、−5.3%、−19.8%**。只有那个自己测量了 `main` 的 PR 落在了与 bot 相差 0.23% 以内。[PR #140](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/140) 和 [#146](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/146) 让陈旧的 pin *自己说出来*，由此得出的规则很短：

> 种子 frontier 是一句关于 `main` 的、没人测量过的断言。如果实在避免不了，就测量**当前 head**，并说明是哪个 commit。


<details>
<summary>English original</summary>

**4.1 When to switch the denominator off**

When the scored metric moved to prefill ([Lecture 09](Lecture-09.md)), the llama anchor was **disabled** and the tier became `delta / frontier` alone. The reasoning is worth following closely, because it is a genuinely non-obvious modelling point.

The tier buckets are *fractions of the reference*. llama.cpp's two numbers are **7.8× apart** — 18.44 decode versus 143.88 prefill. So leaving the anchor on would make a tier cost **2.7× more on prefill than on decode**: an `l` would need +27.1% over the frontier instead of +10.0%.

> Nothing about prefill work is 2.7× harder. That number came from llama.cpp's shape, not from ours.

And a second reason: at that point llama.cpp *batched* the prompt and SparkInfer walked it token by token, so `delta / 143.88` sized a gain against **a feature that had not been built** rather than against the work in the PR. The anchor is scheduled to flip back on once both engines are doing the same thing — at which point it means what it says again.

`pct_of_llama` is still recorded on every run; it stopped being the tier basis, not the target. The verdict JSON carries **both** — `pct_over_frontier` (the honest measured speedup), `pct_of_llama` (the tier basis), and `scored_context` (which context earned it).

---

**5. The frontier — a number with an owner and a provenance**

The frontier is the best figure measured on `main` by a sealed round. Four properties, each earned:

**Raise-only.** So a slow box cannot deflate it and mint tiers for everyone behind it.

**Re-measured every round, not trusted from a pin** ([PR #50](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/50)). A pin is a claim about `main` that someone made once.

**Never hand-filled.** `check_reference_lock.py` is a required CI job: `0` means "not measured" and is always allowed, but **any non-zero baseline must trace to a committed measurement JSON** for the same node *and* context. The repo's justification is the sharpest sentence in its docs on this subject:

> *Downstream, a hand-filled baseline is indistinguishable from a measured one.*

**Quant- and node-qualified by name.** `KIMI_K3_H200X8_IQ1S_LLAMA_128K`, not `LLAMA_128K`. The three milestone nodes run the same quant at the same context, so a single shared slot would silently overwrite one milestone's reference with another's ([PR #24](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/24) fixed reading the unqualified slot).

**5.1 The stale-pin lesson, twice — and the asymmetry nobody expects**

A stale frontier caused two *different* problems, and the difference between them is the actual lesson.

**As a tier basis, stale-low is self-correcting.** It over-scores the next PR by whatever the gap is, and the next round re-measures and fixes it. Annoying, bounded.

**As a regression guard, stale-low is dangerous.** The decode guard's floor is 1% under the pinned value. The slot read **46.48** while `main` actually did ~56.8 — so it would have admitted a decode collapse to **46.02**, a **19% regression**, while cheerfully reporting that the guard passed. [PR #134](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/134) set it to 56.8 by hand for exactly this reason, and documented the risk of doing so (the slot is raise-only, so if `main` really measures under the floor, every PR fails the guard until a human intervenes).

> **The same number in two roles needs two different staleness tolerances.** A basis wants to be *current*; a guard wants to be *conservative in the safe direction*. Work out which yours is.

**And stale-low as a published target is expensive.** The prefill frontier was hand-seeded at 40.35 and attributed to the wrong commit. The bot always measured the real frontier each round, so nothing was mis-scored — but three PRs optimized against the *published* 40.35 and reported +25.4%, +24.5%, +5.3%, which against the real 53.02 were **−4.6%, −5.3%, −19.8%**. Only the one PR that measured `main` itself landed within 0.23% of the bot. [PRs #140](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/140) and [#146](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/146) made a stale pin *say so*, and the rule that came out of it is short:

> A seeded frontier is a claim about `main` that nobody measured. If one is unavoidable, measure the **current head**, and say which commit.

</details>

### 5.2 实测 vs 已证实

在撰写本文时，prefill（首字前的整段计算）前沿 pin 保持在 **69.02**，而引擎在该节点上可证实达到 **99.68** —— 同一台机器、同一份权重，输出与逐 token 路径逐 bit 一致。该 pin 被刻意 *不* 上调，因为没有密封轮次对它做过实测。

这一区分正是台账的全部意义所在：**tier 基准不会因日志无法向你展示的数字而变动。** 该仓库还记录了这一后果，而不是事后才发现 —— 当 `main` 快于其自身的 pin 时，claim gate 会暂时 *宽松*，这被刻意接受为相较于未证实的 pin 而言更小的失败。

---


<details>
<summary>English original</summary>

**5.2 Measured vs attested**

At the time of writing, the prefill frontier pin holds at **69.02** while the engine demonstrably does **99.68** on the node — same box, same weights, output bit-identical to the per-token path. The pin is deliberately *not* raised, because no sealed round has measured it.

That distinction is the whole point of a ledger: **the tier basis does not move on a number the log cannot show you.** The repo also records the consequence rather than discovering it later — with `main` faster than its own pin, the claim gate is temporarily *lenient*, and that is accepted deliberately as the lesser failure versus an unattested pin.

---

</details>

## 6. 十二个漏洞，以及堵住它们的 PR

按被利用的东西分组。这里的每一条都是真实合并的修复；标题直接引用，是因为它们比转述更准确。

**候选者不能给自己打分。**

| PR | 堵住了什么 |
|---|---|
| [#56](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/56) | "*stop the PR's own binary deciding its tier*" |
| [#41](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/41) | "*grade with the harness from `main`, not the PR's copy*" |
| [#18](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/18) | "*guard the scoring function, not just its outputs*" |
| [#35](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/35) | "*pin the tier basis to trusted `main` and bind the result to the PR*" |

一般原则：**被测量之物不能提供测量装置的任何一个部件。** 在本仓库中，这一点靠结构强制执行 —— `sensitive-paths-guard` 是一个 *required status check*，覆盖 `label.py`（测量链路）、`reference.lock`（准确率门禁）和 `bench/results/`。它运行在 `pull_request_target` 上，因此对 fork PR 同样适用，且无法通过修改自己分支里的 workflow 来关闭。欢迎改进 harness（agent 运行时框架）—— 请走 issue，而不是提交一个会给自己打分的 PR。

**裁决必须被重新推导，而不是被上报。** 节点运行会上报 `/eval RESULT_JSON {…}`；workflow 则依据上报的测量结果、以及 *受保护分支上* 的 `reference.lock` **重新计算** tier，而不是依据 payload（[#32](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/32) closed "answer key, verdict overwrite, dead validator"）。贴出的 label 不能决定 payout。

**任何声称都必须有数字支撑。**

| PR | 堵住了什么 |
|---|---|
| [#139](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/139) | "*a ticked box must be backed by a measured number*" |
| [#132](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/132) | "*require a prefill claim and a decode-guard claim, not just a node tick*" |
| [#141](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/141) | "*skip a PR whose claimed prefill cannot beat the frontier*" — 不要在算术上就不可能的事情上消耗节点机时 |
| [#44](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/44) | "*stop the bot claiming success it never verified*" |

**有些数字是不可能的。** [PR #130](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/130) 加入了 **合理性上限** —— 声称结果超过参考值 5× 的，会被判为不可信而拒绝记录。在看到损坏的 kernel 报出惊人数字之前，你会觉得这条规则很奇怪。这个上限不是对可达性能的陈述；它陈述的是：**好到不真实的结果应当被 *核查*，而不是被记账。** [Lecture 10](Lecture-10.md) 里有它抓到的案例。

**调度也是测量的一部分。** [#91](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/91) 先服务最老的 PR，而不是最新的。[#85](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/85) 绝不合并没能超过 frontier 的 PR。[#99](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/99) 给相互冲突的 PR 打 label，而不是静默跳过它们。[#79](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/79) 不再为无法计分的 PR 预约节点 —— 也不再拒绝可以计分的那些。[#62](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/62) 堵住先 draft 后 ready 的规避路径。

还有一条让边际计分得以自洽的约束：**每个贡献者只能有一个 open PR。** 同一作者的两个 open PR 会同时对照同一个基线测量，而另一个 PR 正要改变这个基线 —— 只有它们一次只落地一个，边际增益这个数字才有意义。

**原创性同样是一个测量问题。** `copycat-guard` 通过包含关系，对每个 diff 与已合并历史做指纹比对，后果分档如下：

| 包含率 | 动作 |
|---|---|
| ≥ 90% | 加入 denylist + 关闭 |
| 80–90% | 评论 + 自动关闭 |
| 70–80% | 打 label 供 **semantic review** —— 从不关闭，从不阻塞 |
| < 70% | 忽略 |

70–80% 这一档之所以存在，是因为单靠重叠度无法区分一个改了名的抄袭者，和一个碰巧改到同一个热点函数的独立贡献者。这种不对称被明确写出：*漏放一个抄袭者，代价是一个 PR 的 emission；误封一个真实贡献者，代价是其访问权限被永久剥夺。* 你的假阳性容忍度应当按假阳性的代价来定，而不是按你对检测器的信心来定。

---


<details>
<summary>English original</summary>

**6. Twelve holes, and the PRs that closed them**

Grouped by what was being exploited. Every one of these is a real merged fix; the titles are quoted because they are better than a paraphrase.

**The candidate must not grade itself.**

| PR | What it closed |
|---|---|
| [#56](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/56) | "*stop the PR's own binary deciding its tier*" |
| [#41](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/41) | "*grade with the harness from `main`, not the PR's copy*" |
| [#18](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/18) | "*guard the scoring function, not just its outputs*" |
| [#35](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/35) | "*pin the tier basis to trusted `main` and bind the result to the PR*" |

The general principle: **the thing being measured cannot supply any part of the measuring apparatus.** In this repo that is enforced structurally — `sensitive-paths-guard` is a *required status check* covering `label.py`, the measurement chain, `reference.lock`, the accuracy gate, and `bench/results/`. It runs on `pull_request_target`, so it applies to fork PRs and cannot be disabled by editing the workflow in your own branch. Harness improvements are welcome — via an issue, not a PR that would score itself.

**The verdict must be re-derived, not reported.** A node run posts `/eval RESULT_JSON {…}`; the workflow **recomputes** the tier from the reported measurements and from `reference.lock` *on the protected branch*, not from the payload ([#32](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/32) closed "answer key, verdict overwrite, dead validator"). A posted label cannot set a payout.

**A claim must be backed by a number.**

| PR | What it closed |
|---|---|
| [#139](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/139) | "*a ticked box must be backed by a measured number*" |
| [#132](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/132) | "*require a prefill claim and a decode-guard claim, not just a node tick*" |
| [#141](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/141) | "*skip a PR whose claimed prefill cannot beat the frontier*" — do not spend node hours on an arithmetic impossibility |
| [#44](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/44) | "*stop the bot claiming success it never verified*" |

**Some numbers are impossible.** [PR #130](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/130) added a **plausibility ceiling** — a claimed result above 5× the reference is rejected as implausible rather than recorded. This feels like an odd thing to need until you have seen a corrupted kernel post a spectacular number. The ceiling is not a statement about what is achievable; it is a statement that **a result too good to be true should be *checked*, not banked.** [Lecture 10](Lecture-10.md) has the case it caught.

**Scheduling is part of the measurement.** [#91](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/91) serve the oldest PR first, not the newest. [#85](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/85) never merge a PR that did not beat the frontier. [#99](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/99) label conflicting PRs instead of skipping them silently. [#79](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/79) stop booking the node for PRs that cannot be scored — and stop refusing the ones that can. [#62](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/62) close the draft-then-ready evasion.

And the constraint that makes marginal scoring coherent at all: **one open PR per contributor.** Two open PRs from one author are both measured against a baseline the other is about to move — the marginal-gain number only means something if they land one at a time.

**Originality is a measurement problem too.** `copycat-guard` fingerprints every diff against merged history by containment, with tiered consequences:

| containment | action |
|---|---|
| ≥ 90% | denylist + close |
| 80–90% | comment + auto-close |
| 70–80% | label for **semantic review** — never closes, never blocks |
| < 70% | ignored |

The 70–80% band exists because overlap alone cannot separate a renamed copy from an independent contributor who touched the same hot function. The asymmetry is stated explicitly: *a missed copycat costs one PR's emission; a false block costs a real contributor their access permanently.* Size your false-positive tolerance to the cost of the false positive, not to your confidence in the detector.

---

</details>

## 7. 把不对称性记录下来——以及诚实的边界

这个 harness（agent 运行时框架）里最可信的东西不是什么防御。而是它把*没有*证明的东西写下来的那些地方。

**两侧的测量方式并不相同，而这一点就写在 lock file 里。** llama.cpp 会 prefill（首字前的整段计算）131,072 个真实 token（`llama-bench -d`）。SparkInfer 没有 prefill 路径——一次真正的填充是约 10 小时的串行 `forward_token` 调用——因此它按 131,072 分配缓存，把它保持**置零**，然后 seek 位置。

给出的理由是具体的：**decode（逐 token 生成阶段）的代价与数据无关**——无论表项是激活值还是零，MLA 归约都是稠密的——所以*时序*是可信的。*正确性*则不是，这正是准确率门控单独在短上下文下针对真实 capture 运行、而绝不在 128k 下运行的原因。

接着是防止滥用这条捷径的防御：一个把 `--seek` 打桩的 PR 会让 ctx-64 decode 按 128k 计分，差不多白拿 10×。因此 harness **拒绝任何 bench 未声明其所执行 seek 的运行。**

> **把这个模式抄走。** 任何测量捷径旁边都要写三样东西：(1) 它让什么变得可信，(2) 它*没有*让什么变得可信，(3) 阻止它被滥用的断言。只有 (1) 的捷径就是一个洞。

**明示的局限，精神上等同逐字照录：**

* *“4096 并不是给你计分的那个 131,072。”* 精度一致性门控未测试的区域是“4k 之后的一切”——开启 opt-in 深度后，就是 32k 之后的一切。既不是“什么都没有”，也不是“一切”。
* receipt 校验被一个**未设置**的仓库变量门控，而 workflow 每次运行都会打印一条通知说明这一点。在它开启之前，某个 tier 只能证明算术是在可信输入上重新计算的；**它不能证明测量确实发生过。**
* 深层精度一致性档位（8192/16384/32768）已存在并已提交，但默认关闭，因为 32768 每个被测 build 要花 812 s，而 4096 只需 136 s——深度 pass 要通过 decode 路径一次一个 token 地跑完。代价是明说的；这个取舍是选择，不是疏忽。
* `--merge-admin` 可以在所述的边界内让一项改动在无人阅读的情况下落地。仓库有文档说明，*因为贡献者有权知道什么能合并自己的工作。*

速度的远程证明还存在一个真正的结构性局限，项目的 trust 文档把它讲了出来而不是掩盖过去：**benchmark 数字没有密码学证明。** 正确性是确定性的，可以重跑或封存在 TEE 中；速度不是，它只能靠容差范围内的*复现与共识*变得可信。声称更多，就是不诚实的做法。

---

## 8. 检查清单

从上面四十来项修复中提炼而来。每一行都可追溯到某个真正出过问题的地方。

**顺序与范围**

1. 正确性门控**先于**速度门控运行，一旦失败就完全压制速度数字。
2. 在整个 sweep 上评判**最差**情况，而不是平均值。
3. 棘轮式比较有一个**绝对下限**，低于它比值一律忽略。
4. **计分配置就是你发布的那个配置**——验证 harness 确实能到达它。

**隔离**

5. 候选方**不提供**测量装置的**任何部分**。用必需检查来强制，而不是靠惯例。
6. 判定结论要**重新推导**自可信输入上的测量，绝不按上报值直接采信。
7. 两侧都从零构建，同一台机器，**交错运行**，固定编译器，预热缓存，记录时钟。

**来源可溯**

8. 每个非零基线都**可追溯到一次已提交的测量**。零表示“未测量”，永远合法。
9. 基线的**命名**要涵盖其取值所依赖的每个轴——node、quant、context。
10. frontier 只会**上抬**，且每轮都重新测量。
11. 已失效的 pin 要**自己说出来**。
12. 区分**实测**与**远程证明**，并且只让基点在后者的基础上移动。

**对手与现实**

13. 主张必须**有数字支撑**，不可能的主张要在耗费 node 机时之前被拒绝。
14. 基础设施故障要产生**带诊断的重试**，绝不记录成性能回退。
15. 每条测量捷径都要记录：它让什么变得可信、没有让什么变得可信，以及防止滥用的断言。
16. 写下该门控**没有证明**什么。


<details>
<summary>English original</summary>

**7. Document the asymmetries — and the honest boundaries**

The most credible thing in this harness is not a defense. It is the set of places where it writes down what it *does not* prove.

**The two sides are not measured the same way, and that is in the lock file.** llama.cpp prefills 131,072 real tokens (`llama-bench -d`). SparkInfer had no prefill path — a genuine fill is ~10 hours of sequential `forward_token` calls — so it allocates the cache at 131,072, leaves it **zeroed**, and seeks position.

The justification is specific: **decode cost is data-independent** — the MLA reduction is dense whether the entries are activations or zeros — so the *timing* is faithful. The *correctness* is not, which is exactly why the accuracy gate runs separately at short context against a real capture, and never at 128k.

Then the defense against abusing that shortcut: a PR that stubbed `--seek` would score ctx-64 decode as 128k, roughly a free 10×. So the harness **refuses any run where the bench did not announce the seek it performed.**

> **Steal this pattern.** Any measurement shortcut needs three things written next to it: (1) what it makes faithful, (2) what it does *not*, and (3) the assertion that stops it being abused. A shortcut with only (1) is a hole.

**Stated limits, verbatim in spirit:**

* *"4096 is not the 131,072 you are scored at."* The parity gate's untested region is "everything past 4k" — with the opt-in depths on, past 32k. Not "nothing", and not "everything".
* Receipt verification is gated behind a repository variable that **is not set**, and the workflow prints a notice saying so on every run. Until it is on, a tier proves the arithmetic was recomputed on trusted inputs; **it does not prove the measurement happened.**
* Deep parity depths (8192/16384/32768) exist and are committed but default off, because 32768 costs 812 s per measured build against 136 s for 4096 — the deep pass runs through the decode path one token at a time. The cost is stated; the tradeoff is a choice, not an oversight.
* `--merge-admin` can land a change with no human reading it, within stated bounds. Documented, the repo says, "*because a contributor is entitled to know what can merge their work.*"

There is also a genuine structural limit on speed attestation, which the project's trust document states rather than papers over: **there is no cryptographic proof of a benchmark number.** Correctness is deterministic and can be re-run or sealed in a TEE; speed is not, and is made trustworthy by *reproduction and consensus* within a tolerance. Claiming more than that would be the dishonest option.

---

**8. The checklist**

Distilled from the forty-odd fixes above. Each line is traceable to something that actually went wrong.

**Order and scope**

1. Correctness gate runs **before** the speed gate, and a failure suppresses the speed number entirely.
2. Grade the **worst** case across a sweep, not the average.
3. A ratcheting comparison has an **absolute floor** below which ratios are ignored.
4. The **scored configuration is the one you ship** — verify the harness actually reaches it.

**Isolation**

5. The candidate supplies **no part** of the measuring apparatus. Enforce with a required check, not a convention.
6. The verdict is **re-derived** from measurements on trusted inputs, never accepted as reported.
7. Both sides built from scratch, same box, **interleaved**, pinned compiler, warm cache, recorded clocks.

**Provenance**

8. Every non-zero baseline **traces to a committed measurement**. Zero means "not measured" and is always legal.
9. Baselines are **named** for every axis their value depends on — node, quant, context.
10. The frontier is **raise-only** and re-measured each round.
11. A pin that has gone stale **says so**.
12. Distinguish **measured** from **attested**, and let the basis move only on the latter.

**Adversaries and reality**

13. Claims must be **backed by numbers**, and impossible claims rejected before they cost node hours.
14. Infrastructure failure produces a **retry with a diagnosis**, never a recorded regression.
15. Every measurement shortcut is documented with what it makes faithful, what it does not, and the assertion preventing abuse.
16. Write down what the gate **does not prove.**

---

</details>

## 实验 — 在优化任何东西之前，先搭好记分牌

目标：一个你愿意拿去接受审计的 harness（agent 运行时框架）。本实验不做 engine 改动。

1. **固定参考实现。** 仓库、commit、权重 hash、精确命令行，放进一个已提交的文件里。如果它位于别人的仓库中，就加上一条断言，在它移动时报错。
2. **抓取一份精度一致性参考**，覆盖 ≥5 个嵌套的上下文深度。把 logits 或它们的 hash 提交进去。
3. **写门禁**，顺序如下：精度一致性（最差深度，绝对门槛 + 带下限的棘轮）→ 显著性（前沿的某个百分比，按你实测的噪声定大小）→ tier。
4. **先测你的噪声底。** 把 `main` 与自身对比跑 5 次以上，交错进行。这个离散度*就是*你的显著性阈值；如果你选了 2% 而离散度是 4%，你的门禁就只是摆设。
5. **加一道来源检查。** 一个 CI job，只要有任何非零基线无法追溯到一次已提交的测量就失败。然后试着绕过它，把你发现的问题修掉。
6. **故意制造一处损坏。** 弄坏一个 kernel，让输出是错的但看起来合理，并确认门禁在报告速度*之前*就拒绝了它。如果它先报告速度，你的顺序就错了。
7. **写 `LIMITS.md`。** 列出你的门禁无法证明的三件事。要具体写明未测试的区域。

通过标准：一位同事读完你的 harness 后，能说出它*最*脆弱的测量是哪一项 —— 而这正是你已经在 `LIMITS.md` 里记录过的漏洞。

---

## 自检

1. 你的门禁在报告速度之前先按精度一致性拒绝。有贡献者argues这浪费节点机时，因为大多数 PR 反正都能通过精度一致性。用三句话回答他。
2. 你的前沿陈旧偏低 15%。如果它是 (a) tier 的依据，(b) 一个下限比它低 1% 的回归守卫，分别给出后果。哪个更糟，为什么？
3. Tier = `min(delta/reference, delta/frontier)`。计算 `xl` 阈值（>18%）对应的绝对 tok/s：前沿为 10 tok/s 时，以及前沿为 50 tok/s 时，参考值为 18.44。每种情况下哪一项起约束作用？
4. 你必须在 128k 下测 decode（逐 token 生成阶段），但承担不起用真实 token 做 128k 的 prefill（首字前的整段计算）。描述这个捷径、使它对于*时序*是忠实的那个性质、它为什么对*正确性*不忠实，以及那条阻止别人用桩替代它的断言。
5. 一个 PR 报告比前沿快 5.2×。你的上限是 5×，所以它以不可信为由被拒。作者坚称是真的。你接下来具体做什么 —— 什么情况下你会改变看法？
6. 你的准确率棘轮会标记每一个把 KL 从 0.001 移到 0.003 的 PR。两周后没人再看这个标记。诊断设计错误，并用一条规则修复它。
7. 为什么「把边际收益对照已合并的前沿来打分」会逻辑上推出「每位贡献者只能有一个开着的 PR」？

---

## 参考

* **SparkInfer-K3 测量链** —— [`CONTRIBUTING.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/CONTRIBUTING.md)（门禁顺序、tier 区间、prefill 锚点的推理），[`bench/scripts/reference.lock`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/bench/scripts/reference.lock)（固定的基线与本讲引用的评注），[`EVAL-TRUST.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/EVAL-TRUST.md)（确定性 vs 非确定性的信任划分）。
* **那些加固 PR** —— 标记为 `eval:*` 的 pull request，以及 `gittensor-ai-lab/sparkinfer-k3` 上的 `eval:`/`fix(eval):` 系列](https://github.com/gittensor-ai-lab/sparkinfer-k3/pulls?q=is%3Apr+is%3Aclosed)。按顺序读上二十个，是能替代亲自犯这些错误的最佳方案。
* **作为量化门禁的 KL 散度与 top-1 一致率** —— llama.cpp 的 `--kl-divergence` / `--kl-divergence-base`；推导见 [Logprobs, Perplexity & KL Divergence — Lecture 05](../../04-Logprobs-困惑度与KL散度/Lecture-05.md)。
* **"Which Quantization Should I Use?"** —— [arXiv:2601.14277](https://arxiv.org/abs/2601.14277) —— 系统性的发现：内在指标（PPL、平均 KLD）*必要但不充分*；对 §2 中狭隘的正确性定义是一则有用的告诫。
* **Goodhart 定律**，其原始形式：当一个度量变成目标，它就不再是好的度量。§6 里的每个 PR 都是一个实例。

交叉引用：

* [阶段 5 → ML Systems Engineering Guide → Stage 0: Measurement Discipline](../../Guide.md) —— 本讲就是为这个阶段写的现场报告。
* [Part 1 Lecture 01 — The 2026 inference engineer's mental model](../01-基础/Lecture-01.md) —— 本文假定的 TTFT / TPOT / 吞吐定义。

---


<details>
<summary>English original</summary>

**Lab — build the scoreboard before you optimize anything**

Goal: a harness you would be willing to have audited. No engine changes in this lab.

1. **Pin the reference.** Repo, commit, weights hash, exact command line, in one committed file. If it lives in someone else's repository, add the assertion that fails when it moves.
2. **Capture a parity reference** at ≥5 nested context depths. Commit the logits or a hash of them.
3. **Write the gate**, in this order: parity (worst depth, absolute bar + ratchet-with-floor) → significance (a % of your frontier, sized to your measured noise) → tier.
4. **Measure your noise floor first.** Run `main` against itself 5+ times, interleaved. The spread *is* your significance threshold; if you picked 2% and your spread is 4%, your gate is decorative.
5. **Add the provenance check.** A CI job that fails if any non-zero baseline does not trace to a committed measurement. Then try to cheat it and fix what you find.
6. **Make one deliberate corruption.** Break a kernel so output is wrong but plausible, and confirm the gate rejects it *before* reporting speed. If it reports speed first, your order is wrong.
7. **Write `LIMITS.md`.** Three things your gate does not prove. Be specific about the untested region.

Pass criterion: a colleague can read your harness and name the measurement it is *most* vulnerable to — and it is a hole you already documented in `LIMITS.md`.

---

**Self-check**

1. Your gate rejects on parity before reporting speed. A contributor argues this wastes node hours, since most PRs pass parity anyway. Answer them in three sentences.
2. Your frontier is stale-low by 15%. Give the consequence if it is (a) the tier basis, (b) a regression guard whose floor is 1% below it. Which is worse and why?
3. Tier = `min(delta/reference, delta/frontier)`. Compute the `xl` threshold (>18%) in absolute tok/s for a frontier at 10 tok/s and again at 50 tok/s, with the reference at 18.44. Which term binds in each case?
4. You must measure decode at 128k but cannot afford to prefill 128k of real tokens. Describe the shortcut, the property that makes it faithful for *timing*, why it is not faithful for *correctness*, and the assertion that stops someone stubbing it.
5. A PR reports 5.2× over the frontier. Your ceiling is 5×, so it is rejected as implausible. The author insists it is real. What exactly do you do next — and what would change your mind?
6. Your accuracy ratchet flags every PR that moves KL from 0.001 to 0.003. After two weeks nobody reads the flag. Diagnose the design error and fix it in one rule.
7. Why does "one open PR per contributor" follow logically from scoring marginal gains against a merged frontier?

---

**References**

* **SparkInfer-K3 measurement chain** — [`CONTRIBUTING.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/CONTRIBUTING.md) (gate order, tier bands, the prefill-anchor reasoning), [`bench/scripts/reference.lock`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/bench/scripts/reference.lock) (pinned baselines and the commentary this lecture quotes), [`EVAL-TRUST.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/EVAL-TRUST.md) (the deterministic-vs-non-deterministic trust split).
* **The hardening PRs** — [pull requests labeled `eval:*` and the `eval:`/`fix(eval):` series](https://github.com/gittensor-ai-lab/sparkinfer-k3/pulls?q=is%3Apr+is%3Aclosed) on `gittensor-ai-lab/sparkinfer-k3`. Reading twenty of them in order is the best available substitute for making the mistakes yourself.
* **KL divergence and top-1 agreement as a quantization gate** — llama.cpp's `--kl-divergence` / `--kl-divergence-base`; derived in [Logprobs, Perplexity & KL Divergence — Lecture 05](../../04-Logprobs-困惑度与KL散度/Lecture-05.md).
* **"Which Quantization Should I Use?"** — [arXiv:2601.14277](https://arxiv.org/abs/2601.14277) — the systematic finding that intrinsic metrics (PPL, mean KLD) are *necessary but not sufficient*; a useful caution on §2's narrow definition of correctness.
* **Goodhart's law**, in its original form: when a measure becomes a target, it ceases to be a good measure. Every PR in §6 is an instance.

Cross-references:

* [Phase 5 → ML Systems Engineering Guide → Stage 0: Measurement Discipline](../../Guide.md) — the stage this lecture is the field report for.
* [Part 1 Lecture 01 — The 2026 inference engineer's mental model](../01-基础/Lecture-01.md) — TTFT / TPOT / throughput definitions assumed here.

---

</details>

## 截至 2026-08

SparkInfer-K3 于 `7689cc7`。Gate：top-1 ≥ 0.95、mean KL ≤ 0.05、七个 depth、worst-of。显著性为 frontier 的 2%。档位 `xs` <3.5% / `s` 3.5–6% / `m` 6–10% / `l` 10–18% / `xl` >18%。计分指标为 prefill @ 32k、llama anchor 禁用；decode @ 128k 作为 1% 回归守卫。receipt verification 关闭（`REQUIRE_EVAL_RECEIPT` 未设置）。这里的*规则*才是持久内容；阈值只是一个项目的 calibration。

---

## Next

* 下一讲：[Lecture 03 — Diagnosis: launch-bound, bandwidth-bound, or comm-bound?](Lecture-03.md)
* 上一讲：[Lecture 01 — The workload, the baseline, and the ladder](Lecture-01.md)
* 上级：[Part 4 — Optimizing a Real Engine](README.md)


<details>
<summary>English original</summary>

**Current as of 2026-08**

SparkInfer-K3 at `7689cc7`. Gate: top-1 ≥ 0.95, mean KL ≤ 0.05, seven depths, worst-of. Significance 2% of frontier. Tiers `xs` <3.5% / `s` 3.5–6% / `m` 6–10% / `l` 10–18% / `xl` >18%. Scored metric prefill @ 32k with the llama anchor disabled; decode @ 128k as a 1% regression guard. Receipt verification off (`REQUIRE_EVAL_RECEIPT` unset). The *rules* are the durable content here; the thresholds are one project's calibration.

---

**Next**

* Next: [Lecture 03 — Diagnosis: launch-bound, bandwidth-bound, or comm-bound?](Lecture-03.md)
* Previous: [Lecture 01 — The workload, the baseline, and the ladder](Lecture-01.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
