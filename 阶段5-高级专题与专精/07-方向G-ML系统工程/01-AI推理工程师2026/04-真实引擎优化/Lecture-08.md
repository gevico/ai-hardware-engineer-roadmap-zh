---
title: 第 4 部分 · 第 08 讲 — Graph 常驻 Decode：永久消灭启动开销
description: 第 4 部分 · 第 08 讲 — Graph 常驻 Decode：永久消灭启动开销
---

# 第 4 部分 · 第 08 讲 — Graph 常驻 Decode：永久消灭启动开销

## 概览

爬到阶梯的这一级时，引擎已经完成了第 04 讲到第 07 讲的工作：grid 尺寸已定，projection 已融合，attention 拆成三路，experts 沿两个轴分片。而 **每层约 30 次 kernel 启动 × 93 层 ≈ 每个 token 2790 次启动** 仍由 host 逐个发出，每个 rank 都是如此。

在 21 tok/s 下，这是每 token 47 ms、每次启动约 17 µs 的预算——还算宽裕。在 26 tok/s 下是 38 ms 和约 14 µs。启动开销不会随优化而缩小；它只是在一个更小的数字里占更大比例，直到它*就是*那个数字。

答案是停止启动：把整个 decode 步骤捕获为 **每个 rank 一张 CUDA graph** 并重放。[PR #89](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/89) 这样做了，换来 **+22.7%**，并捕获了一张 **每个 rank 4,257 个节点** 的 graph。

但最重要的后果不是那 22.7%，而是 **graph 捕获改变了哪些其他优化还值得做**——一个此前被正确否决的 kernel 拆分，在捕获之后立刻成了收益。

读完之后，你应该能让一个 decode 步骤可被捕获、正确地测试一张 graph（只重放一次什么都证明不了），并识别出某项优化的价值取决于启动机制而非算术本身的情形。

---

## 1. 捕获换来了什么——以及没人提及的另一半

CUDA graph 把一串 kernel 启动、内存操作及其依赖记录一次，然后用单次提交重放整段内容。启动开销变成 graph 构建成本，只付一次。

实测结果来自一个五次配对重复的 A/B，每次重复交替顺序，全部跑在 **同一份二进制** 上，各特性由环境变量开关控制：

```text
   everything off  (GRAPH=0, QACT_HOIST=0)        48.60 ms    sem 0.55
   graph on, single-kernel combine                39.68 ms
   graph on, split combine  (the PR)              38.30 ms

   47.1 → 38.37 ms/token   ·   21.24 → 26.06 tok/s   ·   +22.7%
   captured: 4257 nodes/rank, 185 collectives/token, mla splits=32
```

现在看 `sem` 列——即标准误——因为它承载着一个不会出现在 CUDA 文档里的发现：

> *「全部关闭」那一组 [...] 是噪声较大的一个（**eager 模式带有 host 侧的方差，而捕获会把它去掉**）——因此 sem 为 0.55，而各捕获组为 0.02。*

**捕获把运行间方差削减了约 27 倍。** 说清楚之后这就很好理解：在 eager 模式下，每个 token 的计时都包含 host 调度、驱动工作和 CPU 争用，这些都会波动。而重放一张 graph 只是对一个固定计划的单次提交——host 几乎不参与，也就没剩多少可波动的了。

两个实际后果：

* **Graph 捕获是测量上的改进，而不只是性能上的改进。** 噪声底降低，意味着更小的真实收益也变得可测量。本案例研究后面那些 3–5% 的结果，正是因为引擎 graph 常驻才*得以*被检测到。
* **它改变了「p99 延迟」的含义。** 在受启动开销约束的 decode 循环里，尾延迟很大程度上是 host 侧的抖动。把 host 从内层循环里移走，对尾部的压缩远大于对均值的移动。

[PR #86](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/86) 从另一侧报告了同样的现象，而且戏剧性得多：

> *已观察到该模型上的 **eager 基线跨会话波动约 15%**，而优化后的那一侧保持稳定，因此从两次独立运行中取出的差值不可比较。交替测量把两侧固定在同一台机器的同一状态上。*

eager 那一侧 15% 的会话间漂移比大多数梯级都大。这就是为什么本部分的每个 PR 都在*同一个会话内*交替测量两侧，而不是背靠背地测——也是为什么拿未捕获的基线做跨时间对比是件坏事。

---


<details>
<summary>English original</summary>

**Part 4 · Lecture 08 — Graph-Resident Decode: Killing the Launch Bill for Good**

**Overview**

By this point in the ladder the engine had done the work of Lectures 04 through 07: the grids were sized, the projections fused, attention split three ways, the experts sharded on two axes. And **~30 kernel launches per layer × 93 layers ≈ 2790 launches per token** were still being issued from the host, one at a time, per rank.

At 21 tok/s that is 47 ms per token and ~17 µs of budget per launch — comfortable. At 26 tok/s it is 38 ms and ~14 µs. The launch bill does not shrink as you optimize; it becomes a larger fraction of a smaller number, until it *is* the number.

The answer is to stop launching: capture the whole decode step as **one CUDA graph per rank** and replay it. [PR #89](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/89) did that for **+22.7%**, and captured a graph of **4,257 nodes per rank**.

But the most important consequence is not the 22.7%. It is that **graph capture changes which other optimizations are worth doing** — and one kernel split that had been correctly rejected before became a win immediately after.

By the end you should be able to make a decode step capturable, test a graph correctly (one replay proves nothing), and recognize when an optimization's value depends on the launch mechanism rather than on the arithmetic.

---

**1. What capture buys — and the half nobody mentions**

A CUDA graph records a sequence of kernel launches, memory operations, and their dependencies once, then replays the whole thing with a single submission. Launch overhead becomes graph-construction cost, paid once.

The measured result, in a five-paired-rep A/B with order alternated per rep, all on **one binary** with the features env-gated:

```text
   everything off  (GRAPH=0, QACT_HOIST=0)        48.60 ms    sem 0.55
   graph on, single-kernel combine                39.68 ms
   graph on, split combine  (the PR)              38.30 ms

   47.1 → 38.37 ms/token   ·   21.24 → 26.06 tok/s   ·   +22.7%
   captured: 4257 nodes/rank, 185 collectives/token, mla splits=32
```

Now look at the `sem` column — the standard error — because it carries the finding that does not appear in the CUDA documentation:

> *The "everything off" arm [...] is the noisy one (**eager mode carries host-side variance that capture removes**) — hence the sem of 0.55 against 0.02 for the captured arms.*

**Capture cut the run-to-run variance by ~27×.** That makes sense once stated: in eager mode every token's timing includes host scheduling, driver work, and CPU contention, all of which vary. A replayed graph is one submission of a fixed plan — the host is barely involved, so there is little left to vary.

Two practical consequences:

* **Graph capture is a measurement improvement, not only a performance one.** Your noise floor drops, which means smaller genuine wins become measurable. Some of the case study's later 3–5% results are only detectable *because* the engine is graph-resident.
* **It changes what "p99 latency" means.** Tail latency in a launch-bound decode loop is substantially host-side jitter. Removing the host from the inner loop compresses the tail more than it moves the mean.

[PR #86](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/86) reports the same phenomenon from the other side, and much more dramatically:

> *the **eager baseline on this model has been observed to move ~15% across sessions** while the optimised side stays stable, so a delta taken from two separate passes is not comparable. Interleaving pins both arms to the same box state.*

A 15% session-to-session drift on the eager arm is larger than most tiers. It is why every PR in this part interleaves arms *within one session* rather than measuring back to back — and why an un-captured baseline is a bad thing to compare against across time.

---

</details>

## 2. 什么会阻断 capture：在 host 上被冻结的值

一张 graph 记录的是 **capture 时刻仍然存活的值**。任何在 host 上算出的地址、大小或计数，都会变成烧进 graph 的常量 —— 而 replay 永远使用 capture 到的那个常量。

有四个这样的值在这里阻断了 capture。每一个都是同类问题的模板：

**由 token 位置算出的指针。** MLA 的 K-cache 行地址 `cache + position × key_length` 在 host 上算出，并被喂给了两个 `cudaMemcpyAsync` 调用。

> *Replay 会永远重写 **同一行。**"*

修复办法：由一个 kernel 从 **驻留在 device 上** 的 `d_pos` 推导出行号，并且 *"它以相同的顺序搬运相同的字节。"* 通用手法是 —— **把会变化的值提升到 device memory，并在 device 上计算地址。**

**以传值方式传入的大小。** attention 的上下文长度原本是一个 kernel 参数。修复依赖于对其使用方式的一个观察：*"`n_ctx` 在三个 MLA kernel 里始终只是一个循环上界，所以它们改为读取 `*d_pos + 1`。没有任何算术发生变化。"* 只用作循环上界的值，可以在 kernel 启动时从内存读取；而用于决定 *grid* 大小的值不行（见下文）。

**状态更新本身。** 位置自增原本在 host 侧。现在它运行在 capture 到的区域 *内部*：

> *正是这一行，让一次 replay 变成 **一个不同的 token**。*

这就是常驻 graph 的自回归 decode（逐 token 生成阶段）的要害。一个以相同方式 replay 的 graph 会产生相同的结果 —— 这对一个 decode 步骤来说毫无用处。每次 replay 必须恰好有一处改变，而带来这一改变的自增必须位于 graph *内部*，作用于 device 状态。

**一个决定 grid 的值，无法被提升。** `splits` 决定 grid 的大小，并选择运行哪个 kernel。

> *graph 这两者都无法改变，所以它仍然是 host 侧的决定，并且 **当 plan 移动时会重新 capture graph。** 驱动的检查与 launcher 调用的是同一个 `k3_mla_decode_plan()`，因此探测结果不会偏离真正启动的东西。*

这就是 graph capture 诚实的边界：**grid 维度与 kernel 身份是结构性的。** 它们无法在单个 graph 内随数据变化。答案是维护一小组 graph，在 plan 变化时重新 capture —— 而使其安全的纪律是：决定 *是否要重新 capture* 的代码与决定 *要启动什么* 的代码调用同一个函数。"哪个 plan 适用" 的两份独立实现，就是一次注定发生的分歧。

> **审计。** 在 capture 之前，列出每一个由 host 算出、并最终到达 launch 的值：指针、大小、计数、循环上界、grid 维度。对每一个做出决定 —— 提升到 device memory，或者把它变成一个 capture key。任何遗漏都会变成常量，其症状是一个只在第一个 token 上正确的模型。

---

## 3. 在 capture 内部非法的操作

与冻结的值分开来看，有些 CUDA 操作在 capture 期间根本不能发生 —— 同步拷贝和内存分配就在其中。有两个藏在惰性初始化路径里：

```text
   ensure_iq1s_tables() / ensure_iq2xs_tables()
       upload lattice tables via SYNCHRONOUS cudaMemcpyToSymbol
   k3_mla_split_scratch()
       cudaMallocs when capacity is short
```


两者都是典型的惰性初始化：首次使用时做一次，然后永久缓存。两者在 eager 模式下都不可见。还要注意它们是在 *哪里* 触发的：

> *两者都在 IQ1_S 下的 **第一个 MoE layer** 上触发 —— 所以 capture 干净地记录完了前面的 dense layer，却在 layer 1 上死掉，而 `cudaGetLastError` 把责任归给了下一次 launch 的 MoE dispatch。*

两条教训，而第二条来自 [Lecture 06 §5.3](Lecture-06.md)：CUDA 错误具有 **粘滞性**，所以失败被归咎于随后运行的任何东西。上报的位置并不是出错的位置。

修复办法是在 init 阶段把两者都预热 —— 然后还有一条值得采纳的工程判断：

> *现在 capture 失败会 **禁用 capture 并以 eager 方式重新发起该 token**，而不是让这次运行失败。*

graph capture 是一种优化。capture 失败应该只让你损失性能，而不是正确性或可用性。这与 [Lecture 05 §4](Lecture-05.md) 的 "slow, never wrong" 是同一原则，只是应用在执行策略这一层面上。

> **抄走这一招。** 在 capture 之前审计惰性初始化：首次使用时上传的表、按需分配、缓存的描述符创建、memoized 的 handle。在启动时把这一切都预热。然后让 capture 失败优雅地降级到 eager，并记录下来。

---


<details>
<summary>English original</summary>

**2. What blocks capture: host values that get frozen**

A graph records **the values that were live at capture time**. Any address, size, or count computed on the host becomes a constant baked into the graph — and replay uses the captured constant forever.

Four such values blocked capture here. Each one is a template for the class:

**A pointer computed from the token position.** The MLA K-cache row address `cache + position × key_length` was computed on the host and fed to two `cudaMemcpyAsync` calls.

> *Replay would rewrite **one row forever.**"*

The fix: a kernel derives the row from a **device-resident** `d_pos`, and *"it moves the same bytes in the same order."* The general move — **promote the varying value to device memory and compute the address on the device.**

**A size passed by value.** The attention context length was a kernel argument. The fix relies on an observation about how it is used: *"`n_ctx` is only ever a loop bound in the three MLA kernels, so they read `*d_pos + 1`. No arithmetic changed."* A value used only as a loop bound can be read from memory at kernel start; a value used to size a *grid* cannot (see below).

**The state update itself.** The position increment was host-side. It now runs *inside* the captured region:

> *the single line that makes a replay a **different token**.*

That is the crux of graph-resident autoregressive decode. A graph replayed identically produces an identical result — which for a decode step is useless. Exactly one thing must change per replay, and the increment that changes it has to be *inside* the graph, operating on device state.

**A value that decides the grid, which cannot be promoted.** `splits` sizes the grid and selects which kernel runs.

> *A graph can change neither, so it stays a host decision and **the graph is re-captured when the plan moves.** The driver's check and the launcher call the same `k3_mla_decode_plan()`, so a probe cannot drift from what launches.*

This is the honest boundary of graph capture: **grid dimensions and kernel identity are structural.** They cannot be data-dependent within one graph. The answer is a small set of graphs, re-captured when the plan changes — and the discipline that makes it safe is that the code deciding *whether to re-capture* and the code deciding *what to launch* call the same function. Two separate implementations of "which plan applies" is a divergence waiting to happen.

> **The audit.** Before capturing, list every host-computed value that reaches a launch: pointers, sizes, counts, loop bounds, grid dimensions. For each, decide — promote to device memory, or make it a capture key. Anything you miss becomes a constant, and the symptom is a model that is correct on the first token.

---

**3. Operations that are illegal inside a capture**

Separately from frozen values, some CUDA operations cannot occur during capture at all — synchronous copies and allocations among them. Two were hiding in lazy initialization paths:

```text
   ensure_iq1s_tables() / ensure_iq2xs_tables()
       upload lattice tables via SYNCHRONOUS cudaMemcpyToSymbol
   k3_mla_split_scratch()
       cudaMallocs when capacity is short
```

Both are classic lazy-init: do it on first use, cache it forever. Both are invisible in eager mode. And note *where* they fired:

> *both firing on the **first MoE layer** under IQ1_S — so capture recorded the leading dense layer cleanly and died at layer 1, while `cudaGetLastError` blamed the MoE dispatch from the next launch.*

Two lessons, and the second is from [Lecture 06 §5.3](Lecture-06.md): CUDA errors are **sticky**, so the failure was attributed to whatever ran next. The reported location was not the fault.

The fix is pre-warming both at init — and then a piece of engineering judgment worth adopting:

> *A capture failure now **disables capture and re-issues the token eagerly**, rather than failing the run.*

Graph capture is an optimization. A failure to capture should cost you performance, not correctness or availability. This is the same "slow, never wrong" principle as [Lecture 05 §4](Lecture-05.md), applied at the level of an execution strategy.

> **Steal this.** Audit for lazy initialization before capturing: first-use table uploads, on-demand allocations, cached descriptor creation, memoized handles. Pre-warm all of it at startup. Then make capture failure a graceful downgrade to eager, and log it.

---

</details>

## 4. 测试 graph：一次 replay 证明不了什么

对任何即将做这项工作的人，[#89](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/89) 里最重要的那句话是：

> *在短上下文下，MLA plan 为 `splits=1` 时，logits 在 `SPARKINFER_K3_GRAPH=1` 与 `=0` 之间是**逐字节相同**的，跨越 **capture 加 7 次 replay** —— **一次 replay 证明不了什么，因为这里的失效模式在第 1 次 replay 时是正确的，从第 2 次 replay 起就错了。**"*

考虑 §2 里的 frozen-pointer bug。capture 时地址是正确的 —— 它是针对当前位置算出来的。第一次 replay 完全复现了 capture 所做的，而那是正确的。从第二次 replay 起，它又把同一行写了一遍，KV cache 便悄无声息地不再推进。

```text
   capture   → correct  (the value was live)
   replay 1  → correct  (identical to capture)
   replay 2  → WRONG    (the frozen value is now stale)
   replay 3+ → WRONG, and the output is still fluent
```

capture 并 replay 一次的测试会通过。生成两个 token 的测试也会通过。**你至少需要三次，越多越好** —— #89 用的是 capture 加 7 次。

还要注意是什么让它成为强有力的检查：**与未 capture 的路径逐字节相同**，而不是“接近”。graph replay 用相同的参数执行相同的 kernel，因此：

> *只要不是完全一致，那就是 bug，而不是 drift。*

准确率数值在全精度上吻合：`mean_kld` **0.005910210209380935**，被描述为*“与 main 的值每一位都相同。”*

---

## 5. 避免了错误结论的 A/A 对照

这是本案例研究里最精彩的调试轶事，作者特意把它写进来，就是为了不让下一个人重蹈覆辙：

> ***在 131072 上做 byte-compare 不是有效的检查，我想让下一个人省掉这段弯路。** `--seek` 推进了位置却没有填充 KV cache，所以 `splits=32` 路径读到约 131k 行**未初始化内存**，同一条 arm 的两次运行彼此都对不上。我据此得出结论说 capture 在 128k 上坏了，**而那时我还没跑 `off`-vs-`off` 对照，那个对照会表明这个测试毫无意义。**"*

事情是这样一连串发生的：在 128k 上跑 byte-compare，看到不一致，断定 graph capture 在长上下文下坏了，然后开始调试一个根本不存在的 bug。真正把它解决掉的，是拿*同一套配置与自身对比* —— 结果发现它自己也不一致。

```text
   graph ON  vs  graph OFF   at 128k  →  MISMATCH   "capture is broken!"
   graph OFF vs  graph OFF   at 128k  →  MISMATCH   ...the TEST is broken.
```

这就是 [Lecture 06 §3.2](Lecture-06.md) 里的 A/A 对照，只是在做另一件事。在那里它确立的是噪声本底；在这里它确立的是，**测量本身没有区分能力。** 同样的手法，当一次比较出现意外失败时，它应该是你*首先*去跑的，而不是最后。

根本原因本身也值得单独记住：基于 seek 的 benchmark 让 KV cache 保持**清零且未填充**（[Lecture 02 §7](Lecture-02.md)）。对*计时*来说这是合理的，因为 MLA 归约是稠密的、与数据无关。对*逐位比较*来说则毫无意义，因为 kernel 读取的是未初始化内存，其内容在多次运行之间不同。一种对某类测量有效、对另一类无效的捷径 —— 这正是该 repo 坚持每条捷径都要写明自己让什么变得忠实、又没让什么变得忠实的原因。

> **当一次比较产生意外结果时，先拿它与自身跑一遍，再去相信它。** 先 A/A，再 A/B。

---


<details>
<summary>English original</summary>

**4. Testing a graph: one replay proves nothing**

The single most important line in [#89](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/89) for anyone about to do this work:

> *At short context, where the MLA plan is `splits=1`, the logits are **byte-identical** between `SPARKINFER_K3_GRAPH=1` and `=0` across **capture plus 7 replays** — **one replay proves nothing, because the failure mode here is correct on replay 1 and wrong from replay 2.**"*

Consider the frozen-pointer bug from §2. At capture time the address is correct — it was computed for the current position. The first replay reproduces exactly what capture did, which was right. From the second replay onward it writes the same row again, and the KV cache silently stops advancing.

```text
   capture   → correct  (the value was live)
   replay 1  → correct  (identical to capture)
   replay 2  → WRONG    (the frozen value is now stale)
   replay 3+ → WRONG, and the output is still fluent
```

A test that captures and replays once passes. So does a test that generates two tokens. **You need at least three, and more is better** — #89 used capture plus seven.

And note what makes it a strong check: **byte-identical against the un-captured path**, not "close." Graph replay executes the identical kernels with identical parameters, so:

> *anything but an exact match would be a bug rather than drift.*

Graph capture is one of the few optimizations where bit-identity is not an aspiration but a *definition*. If replay differs from eager, something is stale.

The accuracy figure agrees to full precision: `mean_kld` **0.005910210209380935**, described as *"main's value to every digit."*

---

**5. The A/A control that prevented a wrong conclusion**

This is the best debugging anecdote in the case study, and the author included it specifically so nobody repeats it:

> ***A byte-compare at 131072 is not a valid check, and I want to save the next person the detour.** `--seek` advances the position without filling the KV cache, so the `splits=32` path reads ~131k rows of **uninitialised memory** and two runs of the **same** arm do not match each other. I concluded from that comparison that capture was broken at 128k, **before running the `off`-vs-`off` control that shows the test is meaningless.**"*

The chain of events: run a byte-compare at 128k, see a mismatch, conclude graph capture is broken at long context, start debugging a bug that does not exist. The thing that resolved it was running the *same configuration against itself* — and finding that it also mismatched.

```text
   graph ON  vs  graph OFF   at 128k  →  MISMATCH   "capture is broken!"
   graph OFF vs  graph OFF   at 128k  →  MISMATCH   ...the TEST is broken.
```

This is the A/A control from [Lecture 06 §3.2](Lecture-06.md) doing different work. There it established a noise floor; here it establishes that **the measurement itself has no discriminating power.** Same technique, and it should be the *first* thing you run when a comparison fails unexpectedly, not the last.

The underlying cause is worth remembering on its own: the seek-based benchmark leaves the KV cache **zeroed and unfilled** ([Lecture 02 §7](Lecture-02.md)). That is legitimate for *timing*, because the MLA reduction is dense and data-independent. It is meaningless for *bitwise comparison*, because the kernel reads uninitialized memory whose contents differ between runs. A shortcut that is valid for one kind of measurement and invalid for another — exactly why the repo insists every shortcut document what it makes faithful and what it does not.

> **When a comparison produces an unexpected result, run it against itself before you believe it.** A/A first, then A/B.

---

</details>

## 6. Capture 改变哪些优化值得做

以下是本讲的概念要点。

`mla_decode_combine_kernel` 合并了各 slice 的 attention 局部分量，*并*把合并后的 latent 经 `wv_b` 投影出去，全部在 `grid = n_head` 个 block 内完成。在 attention 按 head 分片的情况下，一个 rank 拥有 12 个 head —— **在 132-SM 的芯片上只占 12 个 block，约为整机的 9%**，而这个 kernel 每个 head 要读取 256 KB 的 `wv_b`。它的 thread 0 还要串行遍历 `O(splits)` 来构建 rescale 权重，同时 block 内其余线程都在 barrier 处等待（[Lecture 05 §7](Lecture-05.md) 的串行序言）。

修复方法就是把它一分为二，让两半都能填满 grid：

```text
   merge     grid (n_head,  8)   writes the normalised latent to per-device scratch
   project   grid (n_head, 16)   each block owns a slice of v_dim and reads only
                                 its own wv_b rows → no redundant weight traffic

   12 blocks  →  96 and 192.
```

接下来是真正关键的一句话：

> ***多出来的这个 kernel，恰恰就是此前单 kernel 形式之所以正确的原因。*** *每个 MLA layer 多一次 launch，每个 token 就多 24 次 launch，在 93 层的规模下这不是小事。**一旦 decode（逐 token 生成阶段）被 capture 成一个 CUDA graph，launch 就是 graph 里一个已固化的节点，这件事也就不再有影响。** 这是同一分支中 capture 的下游结果，而不是一项独立的改动。*

单 kernel 形式并不是错误。它在**旧的 launch 经济学下是正确的**，而那些经济学一旦改变，它就立刻变成错的。单独测量其效果：**39.68 → 38.30 ms，+3.5%**，配合 `sem 0.02`、`n=5`、`t=69.0`。

这可以推广成一条规则，它会重新框定很多性能工作：

> **Graph capture 把一次 kernel launch 的代价从 ~5 µs 变成 ~0。** 凡是过去因为「它会多一次 launch」而被否决的优化，都应该重新考虑。融合与拆分是相反的方向，哪一个胜出取决于 launch 机制 —— 而不取决于算术本身。

具体来说，一旦进入 graph 常驻状态：为提高 occupancy 而拆分 kernel 变得廉价；用专用变体替代带分支的通用 kernel 变得廉价；为改善 grid 形状而增加小 kernel 也变得廉价。反过来说，**融合的价值比过去降低了**，因为融合当初买到的一部分收益就是消除 launch —— 而 graph 已经把这份收益买下了。

还要注意这次拆分本身的纪律：*「两个阶段的求和顺序都没有改变 —— slice 求和仍按 `i` 升序进行，投影点积仍以步长 32 跨 warp 遍历 `r` —— 因此它与被它替换的 kernel 逐 bit 一致，而不是对它重新做了一次舍入。」* 只要在接缝处保持累加顺序，把一个 kernel 拆成两个并不一定要改动任何 bit（[Lecture 04 §7](Lecture-04.md)）。

---


<details>
<summary>English original</summary>

**6. Capture changes which optimizations are worth doing**

Here is the conceptual payload of the lecture.

`mla_decode_combine_kernel` merged the per-slice attention partials *and* projected the merged latent through `wv_b`, all in `grid = n_head` blocks. With attention head-sharded a rank owns 12 heads — **12 blocks on a 132-SM part, ~9% of the machine**, for a kernel reading 256 KB of `wv_b` per head. Its thread 0 also walked `O(splits)` serially to build the rescale weights while the rest of the block waited at a barrier ([Lecture 05 §7](Lecture-05.md)'s serial prologue).

The fix is to split it in two so both halves fill the grid:

```text
   merge     grid (n_head,  8)   writes the normalised latent to per-device scratch
   project   grid (n_head, 16)   each block owns a slice of v_dim and reads only
                                 its own wv_b rows → no redundant weight traffic

   12 blocks  →  96 and 192.
```

And now the sentence that matters:

> ***The extra kernel is precisely why the single-kernel form was right before.*** *A second launch per MLA layer is 24 more launches per token, and at 93 layers that mattered. **It stops mattering once the decode is captured as one CUDA graph, where the launch is a baked graph node.** This is downstream of the capture in this same branch, not an independent change.*

The single-kernel form was not a mistake. It was **correct under the old launch economics** and became wrong the moment those economics changed. Measured on its own: **39.68 → 38.30 ms, +3.5%**, with `sem 0.02`, `n=5`, `t=69.0`.

This generalizes into a rule that reframes a lot of performance work:

> **Graph capture changes the price of a kernel launch from ~5 µs to ~0.** Every optimization you previously rejected because "it adds a launch" should be reconsidered. Fusion and fission are opposites, and which one wins depends on the launch mechanism — not on the arithmetic.

Concretely, once you are graph-resident: splitting kernels to improve occupancy becomes cheap; specialized variants instead of branchy general kernels become cheap; extra small kernels that improve grid shape become cheap. Conversely, **fusion is worth less than it was**, because part of what fusion was buying was launch elimination — and the graph already bought that.

And note the discipline in the split itself: *"Summation order is unchanged in both stages — the slice sum still runs `i` ascending, the projection dot still strides `r` by 32 across the warp — so this is bit-identical to the kernel it replaces rather than a re-rounding of it."* Splitting a kernel in two does not have to move bits, if you preserve the accumulation order across the seam ([Lecture 04 §7](Lecture-04.md)).

---

</details>

## 7. 重叠、PDL 与留一法归因

[**PR #90**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/90) — *"五条 decode（逐 token 生成阶段）快速路径与程序化启动"* — **48.69 → 37.75 ms/token、20.54 → 26.49 tok/s、+29.0%。**

六项改动，*"各自位于独立的翻译单元中、由各自的开关控制，作为同一个二进制来测量。"* 而这里的归因方式不是累积阶梯，而是**留一法**：

| arm | ms/token | tok/s | factor 价值 |
|---|--:|--:|--:|
| base（全部开关关闭） | 47.48 | 21.06 | — |
| **全部开启** | **39.44** | **25.35** | — |
| `SPARKINFER_K3_PDL=0` | 44.06 | 22.70 | **PDL 4.62 ms** |
| `SPARKINFER_K3_KDA_IP=0` | 42.10 | 23.75 | KDA step 2.66 ms |
| `SPARKINFER_K3_PROJ_1BAR=0` | 39.90 | 25.06 | projections 0.46 ms |

留一法衡量的是每个 factor 在其余全部 factor 都存在时的**边际**价值，而这正是你真正想知道的——累积阶梯会把共同收益全记在你恰好最先应用的那项改动上。而且 base arm 是对照现实验证过的：*"`base` 与单独测量的 main 相差在 1% 以内（47.48 vs 46.93），**正因如此，这些 delta 才名副其实。**"* 一个关掉全部开关、却无法复现真实 `main` 构建的 arm 不能算基线。

**PDL 是最大的单项 factor，为 4.62 ms**——约占该 token 的 10%。Programmatic Dependent Launch 是 Hopper（`sm_90`）的一项特性，它让依赖 kernel 能在其前驱仍在排空时就开始自己的 prologue——把 block 调度到 SM 上、分配寄存器、缺页调入首批指令；而后继 kernel 中的 `cudaGridDependencySynchronize()` 正是它等待前驱写入变为可见的位置。

关键的一点是，它**并非** graph capture 的替代品，PR 用大写专门强调了这一点。二者针对的是同一处缺口的两半：

```text
   GRAPH CAPTURE  removes the HOST cost of submitting a launch
   PDL            removes the DEVICE-side gap between two kernels
                  that are already submitted
   → they stack.
```

一个 K3 token 在 47 ms 内于每个 rank 上要发起约 4,000 个依赖 kernel——约 **12 µs/kernel，而按其字节量这点工作量只该值一两个。**graph 消除的是提交成本；PDL 消除的是 spin-up 成本。这正是 [PR #115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115) 特意把加宽的 norm 保留在 PDL 路径上的原因（[Lecture 04 §2](Lecture-04.md)）——一项选择绕开发射基础设施的优化，会把它的收益又还回去。

有一条安全提示值得带进你自己的代码：该 sync 调用*"必须由每个线程在前驱所写内存的**任何**读取之前执行"*，而把它放得太晚*"会形成数据竞争，产生看似合理的错误数字，而不是崩溃。"* 让它具备可审查性的纪律在于位置——每个 kernel 都把它作为第一条语句调用，于是这个顺序主张就是一行代码的性质，而不是一场关于哪些 load 与哪些 write 互为别名的论证。而且因为这个调用**在 kernel 非程序化启动时是 no-op**，带有它的 kernel 在以常规方式启动时字节完全相同——这正是让 `SPARKINFER_K3_PDL=0` 成为同一个二进制上的诚实 A/B、而不是两个不同程序的原因。

[PR #114](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/114)——*"在每个 decode layer 内部重叠相互独立的工作"*，**41.31 → 47.00 tok/s**——是这个家族的第三个成员：在一个 layer 内部找出没有数据依赖的操作，让它们在各自的流上并发运行。graph capture、PDL 与 layer 内重叠是三种面向同一浪费的机制——**机器在本可以做别的事情的间隙里空转。**

### 7.1 每个 factor 一个测试二进制

> *每个 factor 一个测试二进制，这样删掉某个 factor 时，它自己的测试也随之删除。`k3_kda_step_cpu_test` **不需要 device**——这是唯一能在 fork 的 PR 上运行的覆盖。GPU 测试使用 K3 的**真实** dims，而 `kimi_k3_numeric_test` 缩减后的 dims（`kv_lora` 64、`key_length` 80）永远到不了这些尺寸。*

有三个值得照搬的做法。**每个 factor 一个测试**意味着回退一项改动只会移除它自己的测试，不会留下孤立的断言。**无 device 的测试**是唯一能在不可信 fork 的 PR 上于 CI 中运行的东西——所以最可移植的覆盖应该针对你最复杂的逻辑。而**缩减的测试维度不会走到真实代码路径**：在 `kv_lora 64` 上的数值测试永远到不了 512 宽 latent 所产生的那些 shape，这就是 GPU 测试即便成本更高也要用真实维度的原因。

---


<details>
<summary>English original</summary>

**7. Overlap, PDL, and leave-one-out attribution**

[**PR #90**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/90) — *"five decode fast paths and programmatic launch"* — **48.69 → 37.75 ms/token, 20.54 → 26.49 tok/s, +29.0%.**

Six changes, *"each in its own translation unit behind its own toggle, measured as one binary."* And rather than a cumulative ladder, the attribution is **leave-one-out**:

| arm | ms/token | tok/s | factor worth |
|---|--:|--:|--:|
| base (every toggle off) | 47.48 | 21.06 | — |
| **all on** | **39.44** | **25.35** | — |
| `SPARKINFER_K3_PDL=0` | 44.06 | 22.70 | **PDL 4.62 ms** |
| `SPARKINFER_K3_KDA_IP=0` | 42.10 | 23.75 | KDA step 2.66 ms |
| `SPARKINFER_K3_PROJ_1BAR=0` | 39.90 | 25.06 | projections 0.46 ms |

Leave-one-out measures each factor's **marginal** value in the presence of all the others, which is what you actually want to know — a cumulative ladder credits whichever change you happened to apply first with all the shared benefit. And the base arm is validated against reality: *"`base` lands within 1% of main measured separately (47.48 vs 46.93), **which is what makes these deltas mean what they say.**"* An all-toggles-off arm that does not reproduce a real `main` build is not a baseline.

**PDL is the largest single factor at 4.62 ms** — roughly 10% of the token. Programmatic Dependent Launch is a Hopper (`sm_90`) feature that lets a dependent kernel begin its prologue — scheduling blocks onto SMs, allocating registers, faulting in its first instructions — while its predecessor is still draining; `cudaGridDependencySynchronize()` inside the successor is where it waits for the predecessor's writes to become visible.

The crucial point is that it is **not** a substitute for graph capture, and the PR says so in capitals. The two attack different halves of the same gap:

```text
   GRAPH CAPTURE  removes the HOST cost of submitting a launch
   PDL            removes the DEVICE-side gap between two kernels
                  that are already submitted
   → they stack.
```

A K3 token issues roughly 4,000 dependent kernels per rank in 47 ms — about **12 µs per kernel for work whose bytes justify one or two.** Graphs take the submission cost out; PDL takes the spin-up cost out. This is why [PR #115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115) was careful to keep its widened norm on the PDL path ([Lecture 04 §2](Lecture-04.md)) — an optimization that opts out of the launch infrastructure gives back what it gains.

One safety note worth carrying into your own code: the sync call *"MUST be executed by every thread before ANY read of memory the predecessor wrote,"* and placing it too late *"is a data race that produces plausible wrong numbers, not a crash."* The discipline that makes it reviewable is placement — every kernel calls it as its first statement, so the ordering claim is a one-line property rather than an argument about which loads alias what. And because the call is a **no-op unless the kernel is launched programmatically**, a kernel carrying it is byte-identical when launched the ordinary way — which is exactly what makes `SPARKINFER_K3_PDL=0` an honest A/B on one binary rather than two different programs.

[PR #114](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/114) — *"overlap independent work inside each decode layer"*, **41.31 → 47.00 tok/s** — is the third member of this family: within a layer, find operations with no data dependency and let them run concurrently on separate streams. Graph capture, PDL, and intra-layer overlap are three mechanisms aimed at the same waste — **the machine being idle between things it could have been doing.**

**7.1 One test binary per factor**

> *One test binary per factor, so a dropped factor takes its own test with it. `k3_kda_step_cpu_test` needs **no device** — the only coverage that runs on a fork's PR. The GPU tests use K3's **real** dims, which `kimi_k3_numeric_test`'s shrunk dims (`kv_lora` 64, `key_length` 80) never reach.*

Three practices worth copying. **Test-per-factor** means reverting one change removes exactly its own test, with no orphaned assertions. **A device-free test** is the only thing that can run in CI on an untrusted fork's PR — so the most portable coverage should target your most intricate logic. And **shrunk test dimensions do not exercise real code paths**: a numeric test at `kv_lora 64` never reaches the shapes that a 512-wide latent produces, which is why the GPU tests use the real dimensions even though they cost more.

---

</details>

## 8. 哪些归约与顺序无关

[#90](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/90) 提升了准确率，其解释堪称一堂小型大师课：

> *`mean_kld` **为 main 的 0.687×**，`top1` 两者均为 1.0 [...] 这是构造使然：**KDA 归约变成了一棵树，而不再是长度为 128 的顺序链**，CPU 模型测得这棵树相对 float64 为 **1.13e-7**，而顺序调度测得 **2.41e-7**。其余一切逐位一致，由 `memcmp` 校验 —— **包括量化器，它的扫描是对幅值取最大值，因此与顺序无关。**"*

两个不同的要点：

**树形归约比链式归约更准确**，其道理与 [Lecture 06 §2.1](Lecture-06.md) 中把 attention 沿上下文切分提升了准确率相同：顺序累加的误差按 `O(n)` 增长，成对累加则大致按 `O(log n)` 增长。为并行而重构结构，会作为副作用改善数值表现。该论断由两种调度各自的 *float64 参考测量* 支撑，而非凭空断言。

**知道哪些运算与顺序无关，就知道哪些重排是免费的。** 量化器的扫描是 **对幅值取最大值** —— 而 `max` 在实数域 *以及* 浮点域上都严格满足结合律与交换律。因此对它的重排在构造上就是逐位一致的，`memcmp` 也证实了这点。对比之下，浮点 *加法* 两者都不满足。

> **抄走这条。** 在重排一个归约之前，先给算子分类。`max`、`min` 与位运算是严格结合的 —— 随意重排，放心声称逐位一致。浮点的 `+` 与 `×` 则不是 —— 重排会改变比特，必须明说，并按容差检查。仅此一条区分，就能解决大多数「我能声称逐位一致吗？」的问题。

---

## 9. 比参考实现更精确是一种负担

[**PR #86**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/86) —— 五项 decode（逐 token 生成阶段）路径改动，**33.67 → 35.99 tok/s（+6.9%）** —— 把 MLA 的 latent KV cache 从 **F32 收窄到 F16**。在 128k 下把缓存的字节数减半是很大的带宽收益，而 [PR #107](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107) 后来不得不重新调优的两个常量中，有一个正是被这次改动作废的（[Lecture 05 §7](Lecture-05.md)）。

有趣的部分是准确率论证。三项测量：

| 对比 | 平均 KLD | top-1 |
|---|--:|--:|
| `main` vs 参考捕获 | 5.146e-03 | 100.00 % |
| **本 PR** vs 参考捕获 | **7.513e-03** | 100.00 % |
| 本 PR vs `main` | 4.600e-03 | 100.00 % |

本 PR 距参考反而比 `main` *更远*。而解释颠覆了直觉：

> *这种偏差源于 F16 缓存，它让结果 **向** 参考靠近，而不是远离参考 —— **llama.cpp 的默认 `type_k` 是 F16，所以 `main` 携带的 KV 精度比它被拿来对比的那个实现更高。**"*

该引擎把缓存保持在 F32，而参考实现保持在 F16。由于此处的正确性被定义为 *与参考一致*（[Lecture 02 §2.1](Lecture-02.md)），多出来的那份精度反倒成了 *不一致* 的来源。对齐参考的精度会减少真正要紧的那种偏差，尽管它降低了绝对精度。

> **当你的正确性判据是与参考实现一致时，「比参考更准确」就是你所受评分指标中的一处缺陷。** 要刻意决定：你追求的是对数学的忠实，还是对参考的忠实 —— 它们是两个不同的目标，而其中只有一个才是你的门槛。

同一个 PR 还带来两个习惯：

**隔离你自己的贡献。** *「`main` 自身在同一节点上对同一 refdata 的 0.0051，才是用来对照这一切的基线 —— 这里暂存的权重与 `hello.spkl` 捕获时所依据的快照并非逐位一致，因此 **两条臂都带着这个偏移。** 真正能隔离出本 PR 的数字，是第三行的 0.0046。」* 当两条臂共享同一个系统性偏移时，A 到 B 的对比是唯一干净的信号 —— 与 [Lecture 07 §6.1](Lecture-07.md) 的三方逻辑相同。

**工具打印 FAIL 并不一定意味着门槛没过。** *「注意 `compare_logits.py` 对全部三行都打印 `FAIL`：它自己的标杆是 1e-5，那是 **同一** 实现两次运行之间的容差。它不是准确率门槛，而它 **对 refdata 同样失败 `main`。**」* 工具内置的阈值是为另一个问题设计的。清楚你的哪个工具 *才* 是门槛、以及每个工具默认标杆的含义，既能避免虚惊，也能避免虚假的信心。

还有一处是 [Lecture 07 §5.1](Lecture-07.md) 中死快速路径模式的又一例：该 PR 的改动 2 是 *「一行布线修复，而非新的 kernel 工作。`k3_proj_q8_multirow_1bar` 及其 `x_pre_q8` 参数都已存在；**在提升后的路径上没有任何东西能到达它们。**」* 又一个被合并、正确、却不可达的优化。

---


<details>
<summary>English original</summary>

**8. Which reductions are order-independent**

[#90](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/90) improved accuracy, and the explanation is a small masterclass:

> *`mean_kld` **0.687× main's**, `top1` 1.0 on both [...] That is by construction: **the KDA reduction becomes a tree instead of a 128-long sequential chain**, and the CPU model measures the tree at **1.13e-7** against float64 where the sequential schedule measures **2.41e-7**. Everything else is bit-identical, checked by `memcmp` — **including the quantiser, whose scan is a max over magnitudes and therefore order-independent.**"*

Two distinct ideas:

**A tree reduction is more accurate than a chain**, for the same reason splitting attention over context improved accuracy in [Lecture 06 §2.1](Lecture-06.md): error grows as `O(n)` for sequential accumulation and roughly `O(log n)` for pairwise. Restructuring for parallelism improves numerics as a side effect. The claim is backed by a *float64 reference measurement* of both schedules, not asserted.

**Knowing which operations are order-independent tells you which reorderings are free.** The quantizer's scan is a **max over magnitudes** — and `max` is associative and commutative over the reals *and* over floats, exactly. So reordering it is bit-identical by construction, and `memcmp` confirms it. Compare floating-point *addition*, which is neither.

> **Steal this.** Before reordering a reduction, classify the operator. `max`, `min`, and bitwise ops are exactly associative — reorder freely, claim bit-identity. Floating-point `+` and `×` are not — reordering changes bits, and you must say so and check on tolerance. This one distinction resolves most "can I claim bit-identical?" questions.

---

**9. Being more precise than your reference is a liability**

[**PR #86**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/86) — five decode-path changes, **33.67 → 35.99 tok/s (+6.9%)** — narrows the MLA latent KV cache from **F32 to F16**. Halving the cache's bytes at 128k is a large bandwidth win, and one of the two constants [PR #107](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107) later had to re-tune was invalidated by exactly this change ([Lecture 05 §7](Lecture-05.md)).

The interesting part is the accuracy argument. Three measurements:

| comparison | mean KLD | top-1 |
|---|--:|--:|
| `main` vs reference capture | 5.146e-03 | 100.00 % |
| **this PR** vs reference capture | **7.513e-03** | 100.00 % |
| this PR vs `main` | 4.600e-03 | 100.00 % |

The PR is *further* from the reference than `main`. And the explanation inverts the intuition:

> *The divergence is the F16 cache, which moves **toward** the reference rather than away from it — **llama.cpp's default `type_k` is F16, so `main` was carrying more KV precision than the implementation it is scored against.**"*

The engine was keeping the cache in F32 while the reference kept it in F16. Since correctness here is defined as *agreement with the reference* ([Lecture 02 §2.1](Lecture-02.md)), the extra precision was a source of *disagreement*. Matching the reference's precision reduces the divergence that matters even though it reduces the absolute precision.

> **When your correctness criterion is agreement with a reference implementation, "more accurate than the reference" is a defect in the metric you are graded on.** Decide deliberately whether you are chasing fidelity to the mathematics or fidelity to the reference — they are different targets, and only one of them is your gate.

Two more habits from the same PR:

**Isolate your own contribution.** *"`main`'s own 0.0051 against the same refdata on this node is the baseline to read this against — the weights staged here are not bit-identical to the snapshot `hello.spkl` was captured from, so **both arms carry that offset.** The number that isolates this PR is the 0.0046 in the third row."* When both arms share a systematic offset, the A-to-B comparison is the only clean signal — the same three-way logic as [Lecture 07 §6.1](Lecture-07.md).

**A tool printing FAIL is not necessarily a gate failing.** *"Note `compare_logits.py` prints `FAIL` for all three rows: its own bar is 1e-5, the tolerance for two runs of the **same** implementation. It is not the accuracy gate, and it **fails `main` against refdata too.**"* A tool's built-in threshold was designed for a different question. Knowing which of your tools *is* the gate, and what each one's default bar means, prevents both false alarms and false confidence.

And one more instance of [Lecture 07 §5.1](Lecture-07.md)'s dead-fast-path pattern: change 2 in that PR is *"a one-line routing fix rather than new kernel work. `k3_proj_q8_multirow_1bar` and its `x_pre_q8` parameter both already existed; **nothing reached them on the hoisted path.**"* Another optimization that was merged, correct, and unreachable.

---

</details>

## Lab — 让 decode step 常驻 graph

1. **数一数你的 launch 次数。** 每层 kernel 数 × 层数 × rank 数。用你的 step 时间除以它。若每次 launch 的预算低于约 10 µs，capture 就值得做（§Overview）。
2. **审计 host 上算出来的值。** 列出每一个到达 launch、且在 host 上计算的指针、大小、计数、循环边界和 grid 维度。逐一分类：*提升到 device*，或 *capture key*。不要漏掉循环边界（§2）。
3. **找出状态更新。** 确定每次 replay 必然不同的那一项，把它移进被 capture 的区域，并作用于 device memory（§2）。
4. **审计 lazy initialization。** grep `ensure_*`、`get_or_create`、首次使用的分配、缓存的 handle。在 init 时全部预热（§3）。
5. **做 capture，并让失败可优雅降级。** capture 失败时，禁用 capture、以 eager 方式重新下发并记录日志。测试方法是故意留下一个 lazy init 不预热（§3）。
6. **用 ≥3 次 replay 做测试**，与 eager 路径做逐字节比较。通过故意冻结一个指针，确认只做单次 replay 的测试本来也会通过（§4）。
7. **跑 A/A 对照。** 在相信任何不一致之前，先把某个配置与它自身比较。如果这也对不上，那是你的测试坏了，不是你的代码坏了（§5）。
8. **测量方差，而不只是均值。** 报告两组的标准误。预期 capture 那一组紧密得多（§1）。
9. **重新审视你否决掉的优化。** 列出每一个因为「它会多一次 launch」而被否决的改动。在 graph 常驻的前提下重新评估每一项，并对最有希望的做测量（§6）。
10. **做 leave-one-out 归因。** 如果你落地了多个因素，就发布一张 leave-one-out 表，并把你全关的那一组对照真实的 baseline 构建做验证（§7）。

通过标准：一个被 capture 的 decode step，带有节点数统计、≥3 次 replay 的逐字节一致性测试、测试套件里的 A/A 对照、两组的标准误，以及一项此前被否决的 fission 在 capture 下重新测量。

---

## 自检

1. 你 capture 的 graph 第一个 token 正确，但从第三个起输出流畅却错误。说出这个 bug 的类别，以及两个最可能的具体成因。
2. 为什么 position 的递增必须位于被 capture 的区域之内？这对 position 必须存放在哪里意味着什么？
3. `n_ctx` 被用作循环边界；`splits` 决定 grid 的大小。其中一个可以提升到 device memory，另一个不行。解释两者的区别，并给出后者的应对策略。
4. Graph capture 把你的标准误从 0.55 降到 0.02 ms。给出对今后如何运行 benchmark 套件的两个影响。
5. 一次 lazy 的表上传在第一个 MoE 层触发并破坏了 capture，但报错却指向 MoE dispatch。解释其机制（§3，以及 [Lecture 06 §5.3](Lecture-06.md)）。
6. 你对 *同一个* build 在 128k 下的两次运行做逐字节比较，结果不同。在下任何结论之前，你该检查什么？考虑到 benchmark 是如何到达 128k 的，最可能的成因是什么？
7. 一个单 kernel 设计原本是正确的；后来你 capture 了 decode step，把它拆成两个反而成了收益。解释原因，并说出另外两类价值以同样方式发生变化的优化。
8. 以下哪些可以重排顺序却仍声称 bit-identical：f32 的求和、对幅值取 max、f32 的乘积、按位 OR？逐一给出理由。
9. 你的引擎把 KV cache 存为 F32；参考实现存为 F16。你保留的精度越高，你的精度一致性指标反而越 *差*。解释原因，并说明你会怎么做。
10. `compare_logits.py` 对你的 build、对 `main`、以及对参考实现自比都打印 FAIL。发生了什么？你应当得出什么结论？

---


<details>
<summary>English original</summary>

**Lab — make your decode step graph-resident**

1. **Count your launches.** Kernels per layer × layers × ranks. Divide your step time by it. If the per-launch budget is under ~10 µs, capture is worth doing (§Overview).
2. **Audit host-computed values.** List every pointer, size, count, loop bound, and grid dimension that reaches a launch and is computed on the host. Classify each: *promote to device*, or *capture key*. Do not skip loop bounds (§2).
3. **Find the state update.** Identify the one thing that must differ per replay, and move it inside the captured region, operating on device memory (§2).
4. **Audit lazy initialization.** Grep for `ensure_*`, `get_or_create`, first-use allocations, cached handles. Pre-warm all of it at init (§3).
5. **Capture, and make failure graceful.** On capture failure, disable capture, re-issue eagerly, and log. Test it by deliberately leaving one lazy init un-warmed (§3).
6. **Test with ≥3 replays**, byte-compared against the eager path. Confirm that a test with a single replay would have passed by deliberately freezing a pointer (§4).
7. **Run the A/A control.** Before believing any mismatch, compare a configuration against itself. If that mismatches, your test is broken, not your code (§5).
8. **Measure the variance, not just the mean.** Report the standard error of both arms. Expect the captured arm to be far tighter (§1).
9. **Revisit your rejected optimizations.** List every change you declined because "it adds a launch." Re-evaluate each under graph residency and measure the most promising (§6).
10. **Attribute leave-one-out.** If you land several factors, publish a leave-one-out table and validate your all-off arm against a real baseline build (§7).

Pass criterion: a captured decode step with a node count, a ≥3-replay byte-identity test, an A/A control in your test suite, standard errors for both arms, and one previously-rejected fission re-measured under capture.

---

**Self-check**

1. Your captured graph produces a correct first token and fluent-but-wrong output from the third. Name the bug class and the two most likely specific causes.
2. Why does the position increment have to be inside the captured region, and what does that imply about where the position must live?
3. `n_ctx` is used as a loop bound; `splits` sizes the grid. One can be promoted to device memory and one cannot. Explain the difference and give the strategy for the second.
4. Graph capture cut your standard error from 0.55 to 0.02 ms. Give two consequences for how you run your benchmark suite from now on.
5. A lazy table upload fires on the first MoE layer and breaks capture, but the error names the MoE dispatch. Explain the mechanism (§3, and [Lecture 06 §5.3](Lecture-06.md)).
6. You byte-compare two runs of the *same* build at 128k and they differ. Before concluding anything, what do you check, and what is the likely cause given how the benchmark reaches 128k?
7. A single-kernel design was correct; then you captured the decode step and splitting it into two became a win. Explain, and name two other optimization classes whose value changes the same way.
8. Which of these can you reorder and still claim bit-identical: a sum of f32, a max over magnitudes, a product of f32, a bitwise OR? Justify each.
9. Your engine keeps the KV cache in F32; the reference keeps it in F16. Your parity metric gets *worse* the more precision you keep. Explain, and say what you would do.
10. `compare_logits.py` prints FAIL for your build, for `main`, and for the reference against itself. What is happening, and what should you conclude?

---

</details>

## 参考文献

* **这些 PR**——[#89](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/89)（按 rank 的 CUDA graphs + split MLA combine；捕获阻塞点；A/A 弯路）、[#90](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/90)（PDL 与五条快速路径；留一归因；树形归约）、[#86](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/86)（F16 latent cache；精度即负担的论证；会话漂移）、[#114](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/114)（层内重叠）。它们的主体是主要来源。
* **CUDA Graphs**——[CUDA C++ Programming Guide § CUDA Graphs](https://docs.nvidia.com/cuda/cuda-c-programming-guide/#cuda-graphs) 与 [Getting Started with CUDA Graphs](https://developer.nvidia.com/blog/cuda-graphs/)——捕获语义、流捕获，以及捕获期间非法操作的清单（§3）。
* **Programmatic Dependent Launch**——[CUDA C++ Programming Guide § Programmatic Dependent Launch and Synchronization](https://docs.nvidia.com/cuda/cuda-c-programming-guide/#programmatic-dependent-launch-and-synchronization)——§7 中价值 4.62 ms 的 `sm_90` 特性。
* **生产级 LLM 推理服务中的 CUDA graphs**——vLLM 的 graph capture（`--enforce-eager` 将其禁用）与 TensorRT-LLM 的 CUDA-graph decode 路径；§1 的生产等价物。见 [Part 1 Lecture 05](../01-基础/Lecture-05.md)。
* **Megakernels / 常驻 runtime**——本讲的逻辑终点：与其捕获许多 kernel，不如运行*一个*常驻 kernel。见 [MLSys Deep Dives Lecture 03](../../05-MLSys深度探索/Lecture-03.md) 关于 TileRT 与 megakernel 方案。
* **成对求和 vs 顺序求和的误差**——Higham，*Accuracy and Stability of Numerical Algorithms*，第 4 章——§8 背后的 `O(n)` → `O(log n)` 结果。

交叉引用：

* [Lecture 03 §3——诊断](Lecture-03.md)——launch 账单的算术，以及约束上限为何移动。
* [Lecture 04 §6——Launch geometry](Lecture-04.md)——此处每个 PR 都使用的单二进制、由环境变量门控的 A/B 纪律；§7 的 `k3_pdl_launch`。
* [Lecture 05 §7——Fusion 与激活值量化](Lecture-05.md)——§6 的 combine kernel 也曾有的串行前奏，以及 #107 的常量被 §9 的 F16 cache 作废。
* [Lecture 06 §3.2——128k 下的 attention](Lecture-06.md)——A/A 对照的另一重角色，以及 §6 改进其 combine 的那个 split。
* [Lecture 10 §4.4——静默出错](Lecture-10.md)——粘滞 CUDA 错误与静默未发生的工作。

---

## 截至 2026-08 的现状

8× H200 SXM，`sm_90`，132 SMs，CUDA 12.8+，UD-IQ1_S，tp=8，评分上下文 131,072。捕获的 decode 步：4,257 nodes/rank，185 次集合通信/token，`mla splits=32`。PDL 在 47.48 ms 的 token 中价值 4.62 ms。Graph 臂的标准误为 0.02 ms，eager 为 0.55 ms。捕获审计、≥3 次重放测试、先 A/A 再相信的规则，以及「捕获改变了 launch 的价格」这一重构，是持久的内容。

---

## 下一篇

* 下一篇：[Lecture 09——你遗忘的阶段：batched prefill](Lecture-09.md)
* 上一篇：[Lecture 07——分片 896 个 expert，以及随之而来的 Amdahl 陷阱](Lecture-07.md)
* 上级：[Part 4——优化一个真实引擎](README.md)


<details>
<summary>English original</summary>

**References**

* **The PRs** — [#89](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/89) (per-rank CUDA graphs + split MLA combine; the capture blockers; the A/A detour), [#90](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/90) (PDL and five fast paths; leave-one-out attribution; the tree reduction), [#86](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/86) (F16 latent cache; the precision-as-liability argument; session drift), [#114](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/114) (intra-layer overlap). Their bodies are the primary source.
* **CUDA Graphs** — [CUDA C++ Programming Guide § CUDA Graphs](https://docs.nvidia.com/cuda/cuda-c-programming-guide/#cuda-graphs) and [Getting Started with CUDA Graphs](https://developer.nvidia.com/blog/cuda-graphs/) — capture semantics, stream capture, and the list of operations illegal during capture (§3).
* **Programmatic Dependent Launch** — [CUDA C++ Programming Guide § Programmatic Dependent Launch and Synchronization](https://docs.nvidia.com/cuda/cuda-c-programming-guide/#programmatic-dependent-launch-and-synchronization) — the `sm_90` feature worth 4.62 ms in §7.
* **CUDA graphs in production LLM serving** — vLLM's graph capture (`--enforce-eager` disables it) and TensorRT-LLM's CUDA-graph decode path; the production equivalents of §1. See [Part 1 Lecture 05](../01-基础/Lecture-05.md).
* **Megakernels / persistent runtimes** — the logical endpoint of this lecture: instead of capturing many kernels, run *one* resident kernel. See [MLSys Deep Dives Lecture 03](../../05-MLSys深度探索/Lecture-03.md) on TileRT and the megakernel approach.
* **Pairwise vs sequential summation error** — Higham, *Accuracy and Stability of Numerical Algorithms*, ch. 4 — the `O(n)` → `O(log n)` result behind §8.

Cross-references:

* [Lecture 03 §3 — Diagnosis](Lecture-03.md) — the launch-bill arithmetic, and why the binding ceiling moves.
* [Lecture 04 §6 — Launch geometry](Lecture-04.md) — the single-binary env-gated A/B discipline every PR here uses; §7's `k3_pdl_launch`.
* [Lecture 05 §7 — Fusion and activation quantization](Lecture-05.md) — the serial prologue that §6's combine kernel also had, and #107's constants invalidated by §9's F16 cache.
* [Lecture 06 §3.2 — Attention at 128k](Lecture-06.md) — the A/A control in its other role, and the split whose combine §6 improves.
* [Lecture 10 §4.4 — Silently wrong](Lecture-10.md) — sticky CUDA errors and work that silently does not happen.

---

**Current as of 2026-08**

8× H200 SXM, `sm_90`, 132 SMs, CUDA 12.8+, UD-IQ1_S, tp=8, scored context 131,072. Captured decode step: 4,257 nodes/rank, 185 collectives/token, `mla splits=32`. PDL worth 4.62 ms of a 47.48 ms token. Graph-arm standard error 0.02 ms against 0.55 ms eager. The capture audit, the ≥3-replay test, the A/A-before-believing rule, and the "capture changes the price of a launch" reframing are the durable content.

---

**Next**

* Next: [Lecture 09 — The phase you forgot: batched prefill](Lecture-09.md)
* Previous: [Lecture 07 — Sharding 896 experts, and the Amdahl trap that followed](Lecture-07.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
