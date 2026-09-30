---
title: Part 4 · Lecture 09 — 你遗忘的阶段：批处理 prefill
description: Part 4 · Lecture 09 — 你遗忘的阶段：批处理 prefill
---

# Part 4 · Lecture 09 — 你遗忘的阶段：批处理 prefill

## 概述

在大约六周里，案例研究引擎**完全没有 prefill（首字前的整段计算）路径**。prompt 的每一个 token 都要走单 token 的 decode（逐 token 生成阶段）步骤。摄入 32,768 个 token 需要 **812.2 秒**的墙钟时间。

没有人是不知道才遗漏它。项目自己的路线图里白纸黑字写着它是「下一步，按顺序」的第 2 项。真正发生的事更有意思，也更常见：**记分板衡量的是 decode，于是工程资源也流向了 decode**，落后参考实现 3.57× 的那个阶段始终无人触碰，而最终领先 3.26× 的那个阶段却拿到了十七轮的关注。

然后评分指标变了，在**两天**之内，prefill 从 40.35 提升到 99.68 tok/s —— 一步 2.47×，也是项目历史上最大的单次变化。

到本讲结束时，你应当能够 (1) 解释为什么 prefill 和 decode 是共享 kernel 的两个不同优化问题，(2) 推导为什么对 prompt 做批处理在 dense 模型上值一个数量级，在 sparse 模型上则少得多，(3) 识别那种组织层面的失效模式：你的指标在悄悄决定你的路线图。

---

## 1. 两个阶段，两种状态

摘自 [Part 1 Lecture 02](../01-基础/Lecture-02.md)，用本讲需要的术语重述：

```text
   PREFILL   process N prompt tokens.  All N are known up front.
             → parallel over the sequence.  GEMM.  arithmetic intensity ~ N.
             → COMPUTE-bound.  a weight tile is read once, used N times.

   DECODE    produce token t+1 given t.  Inherently serial.
             → sequence dimension is 1.  GEMV.  arithmetic intensity ~ 1.
             → MEMORY-bound.  a weight tile is read once, used once.
```


这里要紧的推论是：**prefill 的收益来自批处理，而且它是免费的，因为不涉及任何近似。** 你没有拿准确率做交换，也没有改变数学 —— 对于结果在两种做法下都相同的同一个计算，你只是把每个权重分块读取一次，而不是 *N* 次。

一个逐 token 遍历 prompt 的引擎，是在用 decode 的工作做 *N* 遍来完成 prefill 的任务。按构造，它的 prefill 吞吐就等于它的 decode 吞吐。案例研究测到的正是这一点：

```text
   main @ a169ff4, 32,768 prompt tokens:   812.2 s   =   40.35 tok/s
   decode on the same build:                          ~40   tok/s

   "Ingesting is not faster than generating,
    which is the whole opportunity."
```


当你的 prefill 和 decode 数字是同一个数时，你并没有测到两件事。你把一件事测了两遍，而缺失的那个特性，大约就值你没用上的那个批宽。

### 1.1 参考实现的形态告诉你什么是可能的

| 8× H200、UD-IQ1_S，权重相同 | llama.cpp | SparkInfer-K3（之前） | |
|---|--:|--:|---|
| decode @ 128k | 18.44 | 56.8 → 60.17 | **领先 3.08–3.26×** |
| prefill @ 32k | **143.88** ± 0.23 | 40.35 | **落后 3.57×** |

把那张表当作诊断来看，而不是记分板。在相同的硬件和权重下，参考实现在 prefill 上比 decode 快 **7.8×**（143.88 vs 18.44）。这个比值就是一个能用的批处理 prefill 的标志。候选实现的比值是 ~1.0。这两个比值之间的差距*就是*那个缺失的特性，而且在任何人写代码之前它就已经被量化了。

还要注意参考实现的误差棒：**±0.23 tok/s、±0.16%**，基于 3 次重复。prefill 是比 decode 安静得多的测量 —— 它是算力受限的，且运行时间足够长，能把调度噪声平均掉。这对 [Lecture 02](Lecture-02.md) 的显著性门槛有实际影响：你的噪声底不是整个引擎统一的一个数，而一个对 decode 已经很宽容的 2% 门槛，对 prefill 可能就太松了。

---


<details>
<summary>English original</summary>

**Part 4 · Lecture 09 — The Phase You Forgot: Batched Prefill**

**Overview**

For roughly six weeks, the case-study engine had **no prefill path at all**. Every token of a prompt went through the single-token decode step. Ingesting 32,768 tokens took **812.2 seconds** of wall clock.

Nobody had missed it in the sense of not knowing. It is written down in the project's own roadmap as item 2 of "next, in order." What happened is more interesting and much more common: **the scoreboard measured decode, so the engineering went to decode**, and the phase that was 3.57× behind the reference sat untouched while the phase that ended up 3.26× ahead got seventeen rounds of attention.

Then the scored metric changed, and in **two days** prefill went from 40.35 to 99.68 tok/s — a 2.47× step, and the largest single change in the project's history.

By the end of this lecture you should be able to (1) explain why prefill and decode are different optimization problems that share kernels, (2) derive why batching the prompt is worth an order of magnitude on a dense model and much less on a sparse one, and (3) recognize the organizational failure mode where your metric quietly decides your roadmap.

---

**1. Two phases, two regimes**

From [Part 1 Lecture 02](../01-基础/Lecture-02.md), restated in the terms this lecture needs:

```text
   PREFILL   process N prompt tokens.  All N are known up front.
             → parallel over the sequence.  GEMM.  arithmetic intensity ~ N.
             → COMPUTE-bound.  a weight tile is read once, used N times.

   DECODE    produce token t+1 given t.  Inherently serial.
             → sequence dimension is 1.  GEMV.  arithmetic intensity ~ 1.
             → MEMORY-bound.  a weight tile is read once, used once.
```

The consequence that matters here: **prefill's win comes from batching, and it is free in the sense that no approximation is involved.** You are not trading accuracy or changing the math — you are reading each weight tile once instead of *N* times for a computation whose result is identical either way.

An engine that walks the prompt token by token is doing decode's work *N* times to accomplish prefill's job. Its prefill throughput is, by construction, its decode throughput. That is exactly what the case study measured:

```text
   main @ a169ff4, 32,768 prompt tokens:   812.2 s   =   40.35 tok/s
   decode on the same build:                          ~40   tok/s

   "Ingesting is not faster than generating,
    which is the whole opportunity."
```

When your prefill and decode numbers are the same number, you have not measured two things. You have measured one thing twice, and the missing feature is worth roughly the batch width you are not using.

**1.1 The reference's shape tells you what is possible**

| 8× H200, UD-IQ1_S, same weights | llama.cpp | SparkInfer-K3 (before) | |
|---|--:|--:|---|
| decode @ 128k | 18.44 | 56.8 → 60.17 | **3.08–3.26× ahead** |
| prefill @ 32k | **143.88** ± 0.23 | 40.35 | **3.57× behind** |

Read that table as a diagnosis rather than a scoreboard. The reference is **7.8× faster at prefill than at decode** on the same hardware and weights (143.88 vs 18.44). That ratio is the signature of a working batched prefill. The candidate's ratio was ~1.0. The gap between those two ratios *is* the missing feature, and it was quantified before anyone wrote the code.

Note also the reference's error bar: **±0.23 tok/s, ±0.16%**, over 3 reps. Prefill is a much quieter measurement than decode — it is compute-bound and runs long enough to average out scheduling noise. That has a practical consequence for [Lecture 02](Lecture-02.md)'s significance gate: your noise floor is not one number for the whole engine, and a 2% gate that is generous for decode may be loose for prefill.

---

</details>

## 2. 变换：把 layer 循环移到 token 循环之外

整个结构性改动，仓库自己的描述是：

> *"layer 循环现在位于 token 循环之外，因此一块 token 一起流过每个 kernel，一个权重分块对整块只读一次，而不是每个 token 读一次。"*

用伪代码表示，改动前后如下：

```text
   BEFORE — per-token walk                 AFTER — chunked / tiled prefill
   ────────────────────────                ──────────────────────────────
   for tok in prompt:                      for chunk in prompt.chunks(C):
       for layer in 0..93:                     for layer in 0..93:
           attn(tok, layer)                        attn(chunk, layer)     # C rows
           moe(tok, layer)                         moe(chunk, layer)      # C rows
                                                   # one all-reduce for the chunk

   weight reads:  93 × K × N               weight reads:  93 × K × N/C
   launches:      93 × ~30 × N             launches:      93 × ~30 × N/C
   collectives:   92 × N                   collectives:   92 × N/C
```

三项开销以相同的因子 `C` 下降，值得把它们分开来看，因为三者的响应方式不同：

* **权重流量**下降 `C`，*但仅针对块内每个 token 都会访问的权重。* §4 讨论为什么在 MoE 上这个限定条件就是事情的全部。
* **启动次数**无条件下降 `C`。鉴于 [Lecture 03](Lecture-03.md) 发现该引擎受 launch 限制，仅这一点就已很重要——大小为 32 的块去掉了 prompt 的 launch 账单中的 31/32。
* **集合通信次数**无条件下降 `C`，且每次集合通信的宽度变为 `C` 倍。既然 [Lecture 03 §2](Lecture-03.md) 已确认这些操作受延迟限制（payload 增至 512 倍，耗时仅 1.45 倍），把大量窄 reduce 换成少量宽 reduce 几乎是纯收益。

### 2.1 演化脉络：四个 PR

这一改动不是单个 commit 落地的，其顺序很有启发性，因为每一步攻的是这三项开销中的不同一项：

按合并顺序——即 **#133 → #144 → #136**，由 frontier commit 确认（`59.59 → 66.62` 在 #144 的合并处 `23949db6`，随后 `66.62 → 69.02` 在 #136 的 `2a6c66f9`）：

| PR | 层级 | 做了什么 | 攻击点 |
|---|---|---|---|
| [#133](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/133) | `l` | 为 prefill 填充 MLA slice（**+11.1% @32k**），在 int8 张量核心上跑 IQ1_S | attention grid + 算术 |
| [#144](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/144) | `l` | **phase-major** 分块 prefill — 59.04 → 63.80 tok/s @32k | 图节点 + 集合通信 |
| [#136](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/136) | `s` | 对分块的 projection 和 MoE 做批处理 — 63.28 → 69.19 | 一个 phase *内部*的算术 |
| [#148](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/148) | — | 对 prompt 摄入做批处理 — M=1 GEMV 变成 M=B GEMM | 权重读取摊薄 |

这个顺序很重要，因为四个 PR 是**依次施加的三个不同维度**，而不是对同一件事的四次尝试：

* **#144 批处理的是调度。** *Phase-major* 意味着分块内的每个 token 都完成同一个 phase——先全部 attention，再一次 reduce，再全部 FFN——而不是每个 token 走完整个 layer。kernel 相同、算术相同、M=1 的 shape 也相同；变的只有发射顺序。而它之所以划算，原因不在延迟：集合通信只占 18.8 ms 中约 0.01 ms，所以把它们批处理看起来毫无价值。在 CUDA graph 捕获下，真正的瓶颈成本是**图的大小**——捕获在每 token 路径的 3,308 节点图上值 1.93×，而在 T=16 分块的 61,258 节点图上只值 1.56×。**节点才是硬通货**，每个 token 的 185 次集合通信，无论耗时多少，都值得删掉。
* **#136 批处理的是每个 phase 内部的算术**——projection、router、expert dispatch——于是 M=1 的 GEMV 终于变成了 M=T。这正是 #144 作为前置要求所指向的事。
* **#148 摊薄了权重读取**，这是另一种、也是更大的效果，§4 讲的正是它。它自己的 commit 划出了区别：分块驱动*"批处理了集合通信，但仍然按 token 流式读取权重。"*

#144 这次重排的合法性论证值得保留，因为任何调度改动都用得上它：一个 layer 内部唯一的跨 token 依赖沿 **recurrent** 轴分布——KDA 卷积状态与 MLA KV 行——而两者都由 **attention** 消费，attention 仍严格按 token 顺序执行。token *t+1* 的 attention 不读取 token *t* 的 FFN 输出；那条数据流去的是下一个 *layer*。**只要所有跨 token 依赖都落在你保持有序的那条轴上，重排就是合法的。**


<details>
<summary>English original</summary>

**2. The transformation: move the layer loop outside the token loop**

The entire structural change, in the repo's own description:

> *"The layer loop now sits outside the token loop, so a chunk of tokens goes through each kernel together and a weight tile is read once for the chunk instead of once per token."*

In pseudocode, the before and after:

```text
   BEFORE — per-token walk                 AFTER — chunked / tiled prefill
   ────────────────────────                ──────────────────────────────
   for tok in prompt:                      for chunk in prompt.chunks(C):
       for layer in 0..93:                     for layer in 0..93:
           attn(tok, layer)                        attn(chunk, layer)     # C rows
           moe(tok, layer)                         moe(chunk, layer)      # C rows
                                                   # one all-reduce for the chunk

   weight reads:  93 × K × N               weight reads:  93 × K × N/C
   launches:      93 × ~30 × N             launches:      93 × ~30 × N/C
   collectives:   92 × N                   collectives:   92 × N/C
```

Three costs drop by the same factor `C`, and it is worth separating them because they respond differently:

* **Weight traffic** falls by `C` *only for weights every token in the chunk touches.* §4 is about why that qualifier is the whole story on an MoE.
* **Launch count** falls by `C` unconditionally. Given [Lecture 03](Lecture-03.md)'s finding that this engine was launch-bound, this alone is significant — a chunk of 32 removes 31/32 of the launch bill for the prompt.
* **Collective count** falls by `C` unconditionally, and each collective gets `C` times wider. Since [Lecture 03 §2](Lecture-03.md) established these are latency-bound (512× the payload for 1.45× the time), trading many narrow reduces for few wide ones is close to pure profit.

**2.1 The lineage, in four PRs**

The change did not land in one commit, and the sequence is instructive because each step attacked a different one of those three costs:

In merge order — which is **#133 → #144 → #136**, confirmed by the frontier commits (`59.59 → 66.62` at #144's merge `23949db6`, then `66.62 → 69.02` at #136's `2a6c66f9`):

| PR | Tier | What it did | Attacks |
|---|---|---|---|
| [#133](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/133) | `l` | MLA slice fill for prefill (**+11.1% @32k**), IQ1_S on the int8 tensor cores | attention grid + arithmetic |
| [#144](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/144) | `l` | **phase-major** tile prefill — 59.04 → 63.80 tok/s @32k | graph nodes + collectives |
| [#136](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/136) | `s` | batch the tile's projections and MoE — 63.28 → 69.19 | the arithmetic *inside* a phase |
| [#148](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/148) | — | batch prompt ingestion — M=1 GEMV becomes M=B GEMM | weight-read amortization |

That order matters, because the four PRs are **three different axes applied in sequence**, not four attempts at one thing:

* **#144 batches the schedule.** *Phase-major* means every token in the tile completes one phase — all the attention, then one reduce, then all the FFN — instead of each token walking the whole layer. Same kernels, same arithmetic, same M=1 shapes; only the issue order changes. And the reason it pays is not latency: collectives cost ~0.01 ms of an 18.8 ms token, so batching them looks worthless. Under CUDA-graph capture the binding cost is **graph size** — capture is worth 1.93× on the per-token path's 3,308-node graph and only 1.56× on a T=16 tile's 61,258-node one. **Nodes are the currency**, and 185 collectives per token are worth deleting whatever they cost in time.
* **#136 batches the arithmetic inside each phase** — projections, router, expert dispatch — so the M=1 GEMVs finally become M=T. This is what #144 was a prerequisite for.
* **#148 amortizes the weight reads**, which is the different and larger effect §4 is about. Its own commit draws the distinction: the tile driver *"batches the collectives but still streams the weights per token."*

The legality argument for #144's reorder is worth keeping, because it is the one you will need for any schedule change: the only cross-token dependencies inside a layer run along the **recurrent** axis — the KDA convolution state and the MLA KV rows — and both are consumed by **attention**, which still runs strictly in token order. Nothing in token *t+1*'s attention reads token *t*'s FFN output; that flows to the next *layer*. **A reorder is legal exactly when every cross-token dependency lives on an axis you keep ordered.**

</details>

### 2.2 layer-major 驱动是前置要求，不是收益

关于顺序为何重要的最清晰证据，就是那个试图跳过它的 PR。[#145](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/145) —— #148 的直接祖先 —— 先单独做了 layer-major chunked 驱动，并测得：

```text
   128 tokens, 8× H200, same binary and session:

   token loop (main)      55.61 tok/s
   chunked, B = 1         56.78          ← slightly faster
   chunked, B = 8         24.78          ← 2.2× SLOWER
   chunked, B = 32        25.69
```

B=1 胜过那个循环，因为 1.25 GB 的 LM-head 投影每 *prompt* 只跑一次，而不是每个 token 跑一次。而之后批处理让它急剧变差，原因有两点，PR 把这两点作为测量结果而非谜团列出：

* **chunked 路径没有捕获 CUDA graph**，而仅捕获这一项在 prefill（首字前的整段计算）上就值约 1.8×。一个 chunk 必须先把这个挣回来，才能挣到别的。
* **collectives 其实还没有真正批处理。** 在 init 时把它们按 B 倍定尺寸，导致 *weight load* 在 layer 68 失败，因为该 collective 所拥有的 buffer 是 peer-mapped，并按 rank 按 slot 复制 —— 于是 reduce 退化为把 payload 切片到现有容量，结果就是**每次调用一个 token。** collective 次数与 token 循环相同，另外还多出两次 device-to-device 拷贝。

> **一项能解锁某类优化的重构，本身并不是优化。** 把它作为带自身成本的前置要求来报告，并保持 `B=1` 与它所替换的路径逐 bit 一致 —— 正是这种等价性，才让该驱动成为可用的对照，而不是一次无法验证的重写。#148 展示的是同一个驱动 *在* graph capture 与真正批处理的 collectives *之后*值多少：**+43.6%**，而非 −55%。

---

## 3. 同一个改动，三个诚实的比值

有一个微妙之处，会在你第一次汇报批处理收益时咬你一口。同一个 PR 用三种方式测过，三个数字都正确：

| 对比 | 后 | 后 | 比值 |
|---|--:|--:|--:|
| 同一二进制，对比其自身的 per-token 遍历 | 69.41 | 99.68 | **1.44×** |
| 该 PR 的头条数字，对比其 **pre-rebase 父提交** | 56.53 | 98.80 | **1.75×** |
| 对比最后发布的 pre-batching 前沿 | 40.35 | 99.68 | **2.47×** |

这里没有任何粉饰。它们回答三个不同的问题：

* **1.44×** —— *在其余一切固定不变时，chunking 买到了什么？* 这是对改动本身最干净的归因，因为唯一的变量就是代码路径。这是 kernel 工程师应当关心的数字。
* **1.75×** —— *这个分支在被写下时测到的是什么？* 它的父提交得分为 56.53；#136 之后合入，把 pin 移到 69.02，所以这个 delta 是相对**一个已不复存在的 `main`**。作者正是这么说的，而没有把它当作当前值重述：*“我宁愿这么说，也不愿重述一个我没测过的 delta。”*
* **2.47×** —— *prefill 比这条工作线开始之前好了多少？* 这是 release note 该用的数字，用来归因单个 diff 则不对。

值得注意的是那张表里 *没有* 的东西：**一个评测层级。** #148 合入时完全没有任何 `eval:*` 标签。它唯一被打分的轮次是一次 **REJECT**，169.72 tok/s（[Lecture 10 §3](Lecture-10.md)）—— 而那个诚实的 99.68 从未被 sealed，这正是被 pin 住的前沿刻意停在 69.02 的原因（[Lecture 02 §5.2](Lecture-02.md)）。

> **说明你的分母，并说明它何时过期。** 说“快 2.47×”而不带“相比 2026-08-05 发布的 40.35 前沿”，那不是论断，只是一种情绪。而一个相对此后已被取代的父提交测得的 delta，必须把这一点讲出来 —— 算术在被取用时是对的，但它并不是对今天的陈述。

---


<details>
<summary>English original</summary>

**2.2 A layer-major driver is a prerequisite, not a win**

The clearest evidence for why the sequence matters is the PR that tried to skip it. [#145](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/145) — the direct ancestor of #148 — built the layer-major chunked driver first, on its own, and measured:

```text
   128 tokens, 8× H200, same binary and session:

   token loop (main)      55.61 tok/s
   chunked, B = 1         56.78          ← slightly faster
   chunked, B = 8         24.78          ← 2.2× SLOWER
   chunked, B = 32        25.69
```

B=1 beats the loop, because the 1.25 GB LM-head projection runs once per *prompt* instead of once per token. And then batching makes it dramatically worse, for two reasons the PR names as measurements rather than mysteries:

* **The chunked path did not capture a CUDA graph**, and capture alone is worth ~1.8× on prefill. A chunk has to earn that back before it earns anything.
* **The collectives were not actually batched yet.** Sizing them B-fold at init made the *weight load* fail at layer 68, because the collective's owned buffers are peer-mapped and replicated per rank per slot — so the reduce fell back to slicing the payload to existing capacity, which works out to **one token per call.** Same collective count as the token loop, plus two extra device-to-device copies.

> **A restructuring that unlocks a class of optimization is not itself an optimization.** Report it as a prerequisite with its own cost, and keep `B=1` bit-identical to the path it replaces — that equivalence is what makes the driver a usable control rather than an unverifiable rewrite. #148 is what the same driver is worth *after* graph capture and genuinely batched collectives: **+43.6%** instead of −55%.

---

**3. The same change, three honest ratios**

Here is a subtlety that will bite you the first time you report a batching win. The same PR was measured three ways, and all three numbers are correct:

| Comparison | Before | After | Ratio |
|---|--:|--:|--:|
| Same binary, against its own per-token walk | 69.41 | 99.68 | **1.44×** |
| The PR's headline, against its **pre-rebase parent** | 56.53 | 98.80 | **1.75×** |
| Against the last published pre-batching frontier | 40.35 | 99.68 | **2.47×** |

Nothing is being spun. They answer three different questions:

* **1.44×** — *what does chunking buy, holding everything else fixed?* The cleanest attribution of the change itself, because the only variable is the code path. This is the number a kernel engineer should care about.
* **1.75×** — *what did the branch measure when it was written?* Its parent scored 56.53; #136 merged afterwards and moved the pin to 69.02, so this delta is against **a `main` that no longer exists.** The author says exactly that rather than restating it as current: *"I would rather say so than restate a delta I did not measure."*
* **2.47×** — *how much better is prefill than it was before this line of work started?* The right number for a release note, wrong for attributing a single diff.

Worth noting what is *not* in that table: **an eval tier.** #148 merged with no `eval:*` label at all. Its only scored round was a **REJECT** at 169.72 tok/s ([Lecture 10 §3](Lecture-10.md)) — and the honest 99.68 has never been sealed, which is why the pinned frontier deliberately holds at 69.02 ([Lecture 02 §5.2](Lecture-02.md)).

> **State your denominator, and say when it has expired.** "2.47× faster" without "than the 40.35 published frontier of 2026-08-05" is not a claim, it is a mood. And a delta measured against a parent that has since been superseded needs that said out loud — the arithmetic was right when it was taken and is not a statement about today.

---

</details>

## 4. 为什么是 2.47× 而不是 32× —— 稀疏性与批处理相争

如果一个包含 `C` 个 token 的分块把每个权重分块只读一次而不是 `C` 次，为什么实测收益只是一个小倍数，而不是接近 `C` 的数值？

对 **dense** 模型来说，它几乎就是 `C`（直到撞上算力 roofline）。对 **稀疏 MoE** 来说则不是，原因是路由发散：

```text
   dense layer, chunk of C tokens:
       every token needs the SAME weights.
       reads amortize perfectly.  traffic ÷ C.

   MoE layer, top-16 of 896, chunk of C tokens:
       token 1 → experts {a, b, c, ... }      16 of 896
       token 2 → experts {d, e, f, ... }      probably mostly different
       ...
       the chunk touches  UNION of all tokens' top-16
                       ≈ min(16·C, 896)  distinct experts

       at C = 32:  up to 512 distinct experts for 32 tokens.
       each expert's weights read once, used ~1 time.
       expert traffic barely amortizes at all.
```

所以在该架构上，分块分摊掉的是 **dense 且重复** 的部分 —— attention 投影、norm、router、2 个共享专家、LM head —— 却基本分摊不了 **896 个被路由的专家**，后者占 553 GiB 中的 531 GiB。这正是为什么 2.47× 落在这里，而 dense 模型会高出许多。

有两个独立的证据说明这才是真正的约束，而不是一个听起来合理的故事：

**该项目自己的下一步工作恰恰瞄准这两处。** 撰写本文时尚未完成的工作是 [#154](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/154) "*面向 batched prefill 的 chunk-native causal MLA attention*"，以及 [#155](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/155) "*在 scored chunk MoE 上的 CSR + grouped IQ1_S MMA*"。第二项正是本分析所预测的修复：**按专家把分块内的 token 分组**，这样当你确实要为读取某个专家的权重付出代价时，就能把这些权重花在分块内所有路由到该专家的 token 上，并把路由表达为稀疏（CSR）结构，而不是逐 token 的循环。

**这就是专家并行在规模化时解决的同一个问题。** [Part 3 Lecture 03](../03-Blackwell-MoE/Lecture-03.md) 的 all-to-all —— 把所有发往专家 *e* 的 token 汇聚到持有 *e* 的 rank 上，跑一次 grouped GEMM，再 scatter 回来 —— 就是中间隔着一层网络的按专家分组。单节点的分块版本形状相同，只是没有这层网络。**正因如此，grouped GEMM 才是 MoE 的典范原语**，也正是它把一批路由发散的 token 重新变回 dense 工作。

> **一般性教训。** 批处理把一次权重读取分摊到 *共享* 它的那些 token 上。任何让 token 需要不同权重的稀疏性，都会按比例削弱批处理的价值。在稀疏模型上，"batch it" 不是设计的终点 —— 它只是 "现在按它们共享的东西分组" 的前置准备。

---


<details>
<summary>English original</summary>

**4. Why 2.47× and not 32× — sparsity fights batching**

If a chunk of `C` tokens reads each weight tile once instead of `C` times, why is the measured win a small multiple rather than something close to `C`?

For a **dense** model, it very nearly is `C` (until you hit the compute roofline). For a **sparse MoE**, it is not, and the reason is routing divergence:

```text
   dense layer, chunk of C tokens:
       every token needs the SAME weights.
       reads amortize perfectly.  traffic ÷ C.

   MoE layer, top-16 of 896, chunk of C tokens:
       token 1 → experts {a, b, c, ... }      16 of 896
       token 2 → experts {d, e, f, ... }      probably mostly different
       ...
       the chunk touches  UNION of all tokens' top-16
                       ≈ min(16·C, 896)  distinct experts

       at C = 32:  up to 512 distinct experts for 32 tokens.
       each expert's weights read once, used ~1 time.
       expert traffic barely amortizes at all.
```

So on this architecture, chunking amortizes the **dense and replicated** parts — attention projections, norms, router, the 2 shared experts, the LM head — and largely fails to amortize the **896 routed experts**, which are 531 of the 553 GiB. That is precisely why a 2.47× lands where a dense model would show much more.

Two independent confirmations that this is the real constraint rather than a plausible story:

**The project's own next steps target exactly these two places.** The open work at the time of writing is [#154](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/154) "*chunk-native causal MLA attention for batched prefill*" and [#155](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/155) "*CSR + grouped IQ1_S MMA on scored chunk MoE*". The second is the fix this analysis predicts: **group the chunk's tokens by expert** so that when you do pay to read an expert's weights, you spend them on every token in the chunk that routed there, and express the routing as a sparse (CSR) structure rather than a per-token loop.

**It is the same problem expert parallelism solves at scale.** [Part 3 Lecture 03](../03-Blackwell-MoE/Lecture-03.md)'s all-to-all — gather every token destined for expert *e* onto the rank holding *e*, run one grouped GEMM, scatter back — is grouping-by-expert with a network in the middle. The single-node chunked version has the same shape without the network. **Grouped GEMM is the canonical MoE primitive for exactly this reason**, and it is what turns a chunk of divergently-routed tokens back into dense work.

> **The general lesson.** Batching amortizes a weight read across the tokens that *share* it. Any sparsity that makes tokens need different weights reduces batching's value in direct proportion. On a sparse model, "batch it" is not the end of the design — it is the setup for "now group by what they share."

---

</details>

## 5. 组织层面的失败：你的指标就是你的路线图

现在讲可以推广到 kernel 之外的部分。

prefill（首字前的整段计算）是已知缺失项，白纸黑字记着，且落后参考实现 3.57×。它整整六周无人触碰。背后的机制不是疏忽——而是**计分板衡量的是 128k 下的 decode（逐 token 生成阶段），奖励支付的是 128k 下的 decode，因此每个贡献者最理性的动作都是 128k 下的 decode。** 十七项前沿改进投入到了一个本就领先的阶段。

解决办法是换指标，贡献指南里记录的推理完全正确：

> sparkinfer 在那个指标上落后 llama.cpp 18× 时，给 decode 计分是对的。现在它领先 3.08×，而无人触碰的差距在 ingestion。

[PR #131](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/131) 把 prefill @ 32k 定为 tier 基准，并把 decode @ 128k 降级为**回归防线**。几天之内，工作就转向了。同样的贡献者、同样的仓库、同样的激励*结构*——不同的分母。

三个可迁移的要点：

**指标就是一种资源分配策略。** 你给什么计分，就能得到什么，代价是所有你没计分的东西。如果某个已知差距没有推进，先检查是否有人因推进它而获得回报，再下结论说它难。

**边际收益下降时就轮换指标。** 不是按日程，也不是因为旧指标变错了——128k 下的 decode 是个非常好的指标，只是基本已被收割干净。触发条件是*“我们与参考实现之间剩余的最大差距在哪里”*，这个问题可以用 [Lecture 03](Lecture-03.md) 中的 sweep 按季度回答。

**降级，而不是删除。** decode 并没有停止被测量；它变成了带 **1% 下限**的防线。这一点很重要，因为这两个阶段共享 kernel：

> *“decode 和 prefill 共享 kernel。对 prompt 做批处理会带动 decode。1% 防线限定了幅度，它是一种拒绝而非一个 tier——靠让出 decode 换来的 prefill 收益并没有让引擎前进，只是把工作挪了个地方。”*

这是关于单指标优化为何危险的最锋利的表述。没有这道防线，最容易拿到的第一个 prefill 收益就会是靠花掉 decode 换来的，而阶梯会记录下进步，引擎却原地不动。

### 5.1 证明耦合关系的那次事故

prefill 和 decode 共享一切的最强证据来自一个错误，它记录在 [Lecture 02 §5.1](Lecture-02.md) 中，从这个角度看值得再回访一次。

prefill 前沿被手工播种为 **40.35**，并归因到了错误的 commit。等到它被写入时，三个 decode PR（[#114](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/114)、[#115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115)、[#127](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/127)）已经合并。因为 prefill 跑的是与 decode *同一条单 token 路径*，那些 decode 的收益也抬高了 prefill——第一轮真正测量 prefill 得到的是 **53.02**，即**低估 31%**。

于是：三个 decode 优化带来了 31% 的 prefill 提升，没人要求也没人注意到，因为没有任何东西在测量 prefill。接着三个 prefill PR 针对过时的 40.35 做优化，报告了 **+25.4%、+24.5%、+5.3%**——这些数字以真实的 53.02 重新基准化后，变成 **−4.6%、−5.3%、−19.8%**。对照 `main` 的实际状态，这些所谓的收益全都是回归。唯一一个实测 `main` 自身的 PR，落点在 bot 的 **0.23%** 以内。

没有任何东西被错*计分*——bot 每一轮都测量真实前沿并据此计分，所以一个声称被夸大的 PR 仍然可以被修正并获得正当的 tier。过时锚点真正的代价是**三个贡献者的工程方向**：他们针对一个比现实低 31% 的目标调优，而他们自己结果的正负号直到某一轮跑完之前都对他们隐藏着。

由此得出两条规则，成本都很低：

1. **测量你真正所站的那个头部。** 不是发布出来的数字，也不是上周的数字。唯一这么做的那个 PR，也是唯一声称站得住脚的。
2. **当两个阶段共享一条代码路径时，对任一方的优化都会同时移动两者——方向不定。** 所以两者每一轮都需要一个数字，即使只有一个能拿到 tier。

---


<details>
<summary>English original</summary>

**5. The organizational failure: your metric is your roadmap**

Now the part that generalizes beyond kernels.

Prefill was known-missing, written down, and 3.57× behind the reference. It stayed untouched for six weeks. The mechanism is not negligence — it is that **the scoreboard measured decode at 128k, the reward paid for decode at 128k, and so every contributor's rational move was decode at 128k.** Seventeen frontier advances went into a phase that was already winning.

The fix was to change the metric, and the reasoning recorded in the contribution guide is exactly right:

> Decode was the right thing to score while sparkinfer was 18× behind llama.cpp there. It is now 3.08× ahead, and the untouched gap is ingestion.

[PR #131](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/131) made prefill @ 32k the tier basis and demoted decode @ 128k to a **regression guard**. Within days, the work moved. Same contributors, same repo, same incentive *structure* — different denominator.

Three transferable points:

**A metric is a resource-allocation policy.** Whatever you score is what you get, at the expense of everything you do not score. If a known gap is not moving, check whether anyone is paid to move it before concluding it is hard.

**Rotate the metric when the marginal return drops.** Not on a schedule, and not because the old metric became wrong — decode at 128k was a perfectly good metric that had simply been mostly harvested. The trigger is *"where is the largest remaining gap between us and the reference,"* which is a question you can answer quarterly with the sweep from [Lecture 03](Lecture-03.md).

**Demote, do not delete.** Decode did not stop being measured; it became a guard with a **1% floor**. This matters because the two phases share kernels:

> *"Decode and prefill share kernels. Batching the prompt will move decode. The 1% guard bounds how far, and it is a refusal rather than a tier — a prefill gain bought by giving decode back has not moved the engine forward, it has moved work around."*

That is the sharpest available statement of why single-metric optimization is dangerous. Without the guard, the first easy prefill win would have been to spend decode, and the ladder would have recorded progress while the engine stood still.

**5.1 The accident that proves the coupling**

The strongest evidence that prefill and decode shared everything came from a mistake, described in [Lecture 02 §5.1](Lecture-02.md) and worth the second visit from this angle.

The prefill frontier was hand-seeded at **40.35** and attributed to the wrong commit. By the time it was written, three decode PRs ([#114](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/114), [#115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115), [#127](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/127)) had merged. Because prefill ran the *same single-token path* as decode, those decode wins had lifted prefill too — the first round to actually measure prefill found **53.02**, a **31% understatement**.

So: three decode optimizations delivered a 31% prefill improvement that nobody had asked for or noticed, because nothing was measuring prefill. And then three prefill PRs optimized against the stale 40.35 and reported **+25.4%, +24.5%, +5.3%** — figures that, re-based on the real 53.02, become **−4.6%, −5.3%, −19.8%**. Every one of those claimed wins was, against the actual state of `main`, a regression. The one PR that measured `main` itself landed within **0.23%** of the bot.

Nothing was mis-*scored* — the bot measured the real frontier every round and scored against it, so a PR whose claim was inflated could still be revised and earn a legitimate tier. What the stale pin cost was **three contributors' engineering direction**: they were tuning against a target 31% below reality, and the sign of their own results was hidden from them until a round ran.

Two rules fall out, and they are cheap:

1. **Measure the head you are actually starting from.** Not the published number, not last week's number. The single PR that did this was the only one whose claim survived.
2. **When two phases share a code path, an optimization to either moves both — in whichever direction.** So both need a number every round, even if only one earns a tier.

---

</details>

## 6. 生产 runtime 里的 batched prefill 长什么样

案例研究是从零构建的，为的是一个别的工具都加载不了的模型。如果你用的是主流 runtime，同一套机制会以不同的名字出现，这套词汇值得对照一下。

| 这里的概念 | 生产中的名字 | 出现在哪里 |
|---|---|---|
| 把一块 prompt token 一起送过 kernel | **batched prefill** | 每个推理服务 runtime |
| 把长 prompt 切成固定大小的片段 | **chunked prefill** | vLLM `--enable-chunked-prefill`、SGLang 默认 |
| 在一个批里混入 prompt 分块与 decode 步骤 | **continuous batching / mixed batching** | vLLM、SGLang、TRT-LLM |
| 在 GEMM 之前按路由到的 expert 给 token 分组 | **grouped GEMM / expert grouping** | DeepEP、CUTLASS grouped GEMM |
| 把 prefill 和 decode 跑在各自独立的硬件上 | **P/D disaggregation** | Mooncake、Splitwise、DistServe |

生产环境里存在 chunked prefill（首字前的整段计算）的原因不是吞吐，而是 **延迟公平性**：一个 32k 的 prompt 独占 GPU，会卡住每一个并发的 decode（逐 token 生成阶段），所以 prompt 被切成块，与 decode 步骤交错执行。这就是 [Part 2 Lecture 05](../02-Hopper稠密计算/Lecture-05.md) 和 [Part 2 Lecture 06](../02-Hopper稠密计算/Lecture-06.md)。

值得指出案例研究*没有*做什么，因为这能厘清范围：它是单流引擎，所以没有调度器、没有连续批处理，也没有请求队列。它的“批处理”针对的是一个 prompt 的 token，而不是并发的请求。这是正确的第一个目标——你调度不了一个还没实现的阶段——但这也意味着本讲的数字是单流数字，完全说明不了推理服务的吞吐。当 [Part 3 Lecture 04](../03-Blackwell-MoE/Lecture-04.md) 讨论 P/D disaggregation 时，它预设两个阶段都已经能跑。

---

## 7. 落到了哪里，还剩下什么

```text
   prefill @ 32k, 8× H200, UD-IQ1_S

      40.35  ──▶  53.02  ──▶  59.59  ──▶  66.62  ──▶  69.02  ──▶  99.68
      (per-token walk, lifted by decode wins)          │            │
                                                   last sealed   batched,
                                                                 unsealed

      llama.cpp:  143.88                          →  1.44× ahead of us
```

剩下的差距不再是*“他们做批处理，我们不做。”*两个引擎都做批处理。剩下的是**批处理自身的效率**——用 chunk 原生的 attention 而不是切片填充，用分组的 expert GEMM 而不是在 chunk 内逐 token 分发（§4）。

这是一个好得多的处境，也值得作为工程判断明确说出来：**“缺一个功能”与“我们这版功能差 1.44×”之间的差别，就是路线图条目与调优问题之间的差别。**前者以周为单位估算，且有未知的未知；后者以 profile 为单位估算。

---

## 实验——找出你自己被遗忘的阶段

1. **测量每一个阶段，而不只是你优化的那个阶段。**针对你的工作负载：TTFT、真实 prompt 长度下的 prefill tok/s、真实上下文下的 decode tok/s，以及——如果你做并发推理服务——目标 batch 下的吞吐。四个数字，同一份构建，同一台机器。
2. **算出参照实现的阶段比值和你自己的。**`prefill_tok_s / decode_tok_s` 两个引擎都算。参照比值远高于你的，说明你在某处漏了批处理。两个比值都要报告。
3. **找出你相对参照最差的比值。**哪个阶段你落后最多？那就是你的候选，不管你一直在做的是哪一个。
4. **如果任何地方存在逐 token 的路径，估算分块能带来的收益。**数一数 chunk 里*所有* token 共享的权重字节，与只有部分 token 共享的字节。共享的比例给出了摊销的上界——这就是把 §4 的算术用在你自己的模型上。
5. **在优化之前先加一道护栏。**挑一个你*不*优化的阶段，为它加一条回归下限（比当前低 1–2%）。然后故意让该阶段退化，验证护栏会触发。
6. **拿到收益后重新推导。**再测一遍那四个数字。你没优化的那个阶段动了吗？动了多少，朝哪个方向？这就是把 §5.1 用在你自己的代码上。

通过标准：你的产物包含一张列出两个引擎比值的阶段表，以及一道你*亲眼看到触发过*的护栏——而不是你相信它会触发。

---


<details>
<summary>English original</summary>

**6. What batched prefill looks like in production runtimes**

The case study built this from scratch for a model nothing else could load. If you are working in a mainstream runtime, the same mechanics appear with different names, and the vocabulary is worth mapping.

| Concept here | Production name | Where |
|---|---|---|
| Chunk of prompt tokens through the kernels together | **batched prefill** | every serving runtime |
| Splitting a long prompt into fixed-size pieces | **chunked prefill** | vLLM `--enable-chunked-prefill`, SGLang default |
| Mixing prompt chunks with decode steps in one batch | **continuous batching / mixed batching** | vLLM, SGLang, TRT-LLM |
| Group tokens by routed expert before the GEMM | **grouped GEMM / expert grouping** | DeepEP, CUTLASS grouped GEMM |
| Run prefill and decode on separate hardware | **P/D disaggregation** | Mooncake, Splitwise, DistServe |

The reason chunked prefill exists in production is not throughput but **latency fairness**: a 32k prompt monopolizing the GPU stalls every concurrent decode, so the prompt is broken into chunks and interleaved with decode steps. That is [Part 2 Lecture 05](../02-Hopper稠密计算/Lecture-05.md) and [Part 2 Lecture 06](../02-Hopper稠密计算/Lecture-06.md).

Worth noting what the case study is *not* doing, because it clarifies scope: it is a single-stream engine, so it has no scheduler, no continuous batching, and no request queue. Its "batching" is over the tokens of one prompt, not over concurrent requests. That is the right first target — you cannot schedule a phase you have not implemented — but it means the numbers in this lecture are single-stream numbers and do not speak to serving throughput at all. When [Part 3 Lecture 04](../03-Blackwell-MoE/Lecture-04.md) discusses P/D disaggregation, it presumes both phases already work.

---

**7. Where it landed, and what is left**

```text
   prefill @ 32k, 8× H200, UD-IQ1_S

      40.35  ──▶  53.02  ──▶  59.59  ──▶  66.62  ──▶  69.02  ──▶  99.68
      (per-token walk, lifted by decode wins)          │            │
                                                   last sealed   batched,
                                                                 unsealed

      llama.cpp:  143.88                          →  1.44× ahead of us
```

The remaining gap is no longer *"they batch and we do not."* Both engines batch. What is left is **the batching's own efficiency** — chunk-native attention rather than a slice fill, and grouped expert GEMMs rather than per-token dispatch inside the chunk (§4).

That is a much better position to be in, and worth saying explicitly as a matter of engineering judgment: **the difference between "missing a feature" and "our version of the feature is 1.44× off" is the difference between a roadmap item and a tuning problem.** The first is estimated in weeks and has unknown unknowns; the second is estimated in profiles.

---

**Lab — find your own forgotten phase**

1. **Measure every phase, not the one you optimize.** For your workload: TTFT, prefill tok/s at a realistic prompt length, decode tok/s at a realistic context, and — if you serve concurrently — throughput at your target batch. Four numbers, same build, same box.
2. **Compute your reference's phase ratio and yours.** `prefill_tok_s / decode_tok_s` for both engines. A reference ratio far above yours means you are missing batching somewhere. Report both ratios.
3. **Find your worst ratio-to-reference.** Which phase are you furthest behind on? That is your candidate, regardless of which one you have been working on.
4. **If you have a per-token path anywhere, estimate the chunked win.** Count weight bytes that *all* tokens in a chunk share versus bytes that only some share. The shared fraction bounds your amortization — this is §4's arithmetic on your model.
5. **Add a guard before you optimize.** Pick the phase you are *not* optimizing and add a regression floor for it (1–2% below current). Then verify the guard fires by deliberately regressing that phase.
6. **Re-derive after the win.** Measure all four numbers again. Did the phase you were not optimizing move? By how much, and in which direction? This is §5.1 on your own code.

Pass criterion: your artifact contains a phase table with both engines' ratios, and a guard that you have *watched fire* — not one you believe would fire.

---

</details>

## 自检

1. 你的引擎报告 prefill（首字前的整段计算）42 tok/s 和 decode（逐 token 生成阶段）41 tok/s。由此能立刻得出的唯一结论是什么，预期修复的规模有多大？
2. 一台参考引擎在同一台机器、同一份权重上做到 143.88 的 prefill 和 18.44 的 decode。7.8× 的比值说明它的实现是怎样的，而接近 1.0 的比值又会告诉你什么？
3. 你在稠密模型上以 C=32 切分 prefill，看到约 20×；在 256 选 8 的 MoE（混合专家模型）上看到 3×。用一段话解释这一差异，然后指出能补回大部分差距的优化。
4. 同一个 PR 被诚实地描述成 1.44×、1.75× 和 2.47×。给出每个数字各自回答的问题，并说明哪一个该进分级表、哪一个该进发版说明。
5. 你的团队在某个没人负责的阶段存在已知的 3.5× 差距。在断定它很难之前，你该检查什么？
6. 你把 decode 从分级依据降格为 1% 的回归护栏。有贡献者提交了一个 prefill 收益，代价是 0.8% 的 decode。它通过了。为双方各作论证，然后说明你实际会怎么做。
7. 三个 decode PR 把 prefill 提升了 31%，一周内没人注意到。能发现它的最小监控埋点是什么，成本又是多少？

---

## References

* **SparkInfer-K3 prefill 工作** — [#133](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/133), [#136](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/136), [#144](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/144), [#148](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/148)；[#131](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/131) 和 [#132](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/132) 中的指标变更；[`bench/scripts/reference.lock`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/bench/scripts/reference.lock) 和 [`CONTRIBUTING.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/CONTRIBUTING.md) 中的陈旧 seed 事后复盘。
* **Chunked prefill（Sarathi）** — [arXiv:2308.16369](https://arxiv.org/abs/2308.16369) 和 **Sarathi-Serve** [arXiv:2403.02310](https://arxiv.org/abs/2403.02310) —— 把 prefill 切分以与 decode 交错的源头，以及无停顿批处理的论证。
* **Orca（连续批处理）** — [OSDI '22](https://www.usenix.org/conference/osdi22/presentation/yu) —— 迭代级调度，混合批的想法。
* **vLLM chunked prefill** — [docs.vllm.ai](https://docs.vllm.ai/en/latest/) —— 生产环境的旋钮。
* **DistServe** — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670) —— prefill 与 decode 需要不同资源这一论点，被推到了极致。
* **CUTLASS grouped GEMM（矩阵-矩阵乘）** — [github.com/NVIDIA/cutlass](https://github.com/NVIDIA/cutlass) —— §4 认定为解决一个 chunk 内布线发散问题的原语。

交叉引用：

* [第 1 部分 第 02 讲 —— Transformer 执行：从 token 到位](../01-基础/Lecture-02.md) —— prefill/decode 的切分。
* [第 2 部分 第 05 讲 —— 现代推理服务栈](../02-Hopper稠密计算/Lecture-05.md) 和 [第 06 讲 —— 128K 长上下文](../02-Hopper稠密计算/Lecture-06.md) —— chunked prefill 作为一种调度技术。
* [第 3 部分 第 03 讲 —— 专家并行与 gating 热路径](../03-Blackwell-MoE/Lecture-03.md) —— 按专家分组，在簇规模上。
* [第 3 部分 第 04 讲 —— 分离式 prefill/decode](../03-Blackwell-MoE/Lecture-04.md) —— 两个阶段都能跑通之后该做的事。

---

## 截至 2026-08 的现状

SparkInfer-K3 位于 `7689cc7`。Prefill 99.68 tok/s @ 32k（未封存；固定前沿 69.02），对照 llama.cpp 143.88 ± 0.23。Decode 护栏下限 56.232。Chunk 原生的 MLA（多头潜在注意力）（[#154](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/154)）和分组 chunk MoE（[#155](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/155)）在撰写本文时仍处于开放状态。§1.1 中的阶段比值诊断和 §4 中的稀疏性论证是经久的内容。

---

## 下一讲

* 下一讲：[第 10 讲 —— 静默出错：推理引擎独有的失效模式](Lecture-10.md)
* 上一讲：[第 08 讲 —— 图驻留的 decode](Lecture-08.md)
* 上级：[第 4 部分 —— 优化一个真实引擎](README.md)


<details>
<summary>English original</summary>

**Self-check**

1. Your engine reports prefill 42 tok/s and decode 41 tok/s. What single conclusion follows immediately, and what is the expected size of the fix?
2. A reference engine does 143.88 prefill and 18.44 decode on the same box and weights. What does the 7.8× ratio tell you about its implementation, and what would a ratio near 1.0 tell you?
3. You chunk prefill at C=32 on a dense model and see ~20×; on a top-8-of-256 MoE you see 3×. Explain the difference in one paragraph, then name the optimization that recovers most of the gap.
4. The same PR is honestly described as a 1.44×, a 1.75×, and a 2.47×. Give the question each answers, and say which belongs in a tier and which in a release note.
5. Your team has a known 3.5× gap in a phase nobody is working on. Before concluding it is hard, what do you check?
6. You demote decode from tier basis to a 1% regression guard. A contributor submits a prefill win that costs 0.8% of decode. It passes. Argue both sides, then state what you would actually do.
7. Three decode PRs improved prefill by 31% and nobody noticed for a week. What is the minimum instrumentation that would have caught it, and what would it have cost?

---

**References**

* **SparkInfer-K3 prefill work** — [#133](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/133), [#136](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/136), [#144](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/144), [#148](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/148); the metric change in [#131](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/131) and [#132](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/132); the stale-seed post-mortem in [`bench/scripts/reference.lock`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/bench/scripts/reference.lock) and [`CONTRIBUTING.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/CONTRIBUTING.md).
* **Chunked prefill (Sarathi)** — [arXiv:2308.16369](https://arxiv.org/abs/2308.16369) and **Sarathi-Serve** [arXiv:2403.02310](https://arxiv.org/abs/2403.02310) — the origin of splitting prefill to interleave with decode, and the stall-free-batching argument.
* **Orca (continuous batching)** — [OSDI '22](https://www.usenix.org/conference/osdi22/presentation/yu) — iteration-level scheduling, the mixed-batch idea.
* **vLLM chunked prefill** — [docs.vllm.ai](https://docs.vllm.ai/en/latest/) — the production knob.
* **DistServe** — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670) — the argument that prefill and decode want different resources, taken to its conclusion.
* **CUTLASS grouped GEMM** — [github.com/NVIDIA/cutlass](https://github.com/NVIDIA/cutlass) — the primitive §4 identifies as the fix for divergent routing in a chunk.

Cross-references:

* [Part 1 Lecture 02 — Transformer execution: from tokens to bits](../01-基础/Lecture-02.md) — the prefill/decode split.
* [Part 2 Lecture 05 — Modern serving stack](../02-Hopper稠密计算/Lecture-05.md) and [Lecture 06 — Long context at 128K](../02-Hopper稠密计算/Lecture-06.md) — chunked prefill as a scheduling technique.
* [Part 3 Lecture 03 — Expert parallelism and the gating hot path](../03-Blackwell-MoE/Lecture-03.md) — grouping by expert, at cluster scale.
* [Part 3 Lecture 04 — Disaggregated prefill/decode](../03-Blackwell-MoE/Lecture-04.md) — what you do once both phases work.

---

**Current as of 2026-08**

SparkInfer-K3 at `7689cc7`. Prefill 99.68 tok/s @ 32k (unsealed; pinned frontier 69.02) against llama.cpp 143.88 ± 0.23. Decode guard floor 56.232. Chunk-native MLA ([#154](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/154)) and grouped chunk MoE ([#155](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/155)) open at the time of writing. The phase-ratio diagnostic in §1.1 and the sparsity argument in §4 are the durable content.

---

**Next**

* Next: [Lecture 10 — Silently wrong: the failure mode unique to inference engines](Lecture-10.md)
* Previous: [Lecture 08 — Graph-resident decode](Lecture-08.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-09.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-09.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
