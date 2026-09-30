---
title: Part 4 · Lecture 04 — 启动几何：Grid、Occupancy 与每 token 327 次 Norm
description: Part 4 · Lecture 04 — 启动几何：Grid、Occupancy 与每 token 327 次 Norm
---

# Part 4 · Lecture 04 — 启动几何：Grid、Occupancy 与每 token 327 次 Norm

## Overview

本案例研究中收益最高的优化并不是更好的算法，而是**同一套算术，用不同的形状启动。**

H200 有 **132 个 streaming multiprocessor**。以 12 个 block 启动的 kernel 只用了它的 9%。以 1 个 block 启动的 kernel 只用 0.8%。在 batch-1 decode（逐 token 生成阶段）中，每一条天然并行轴都坍缩成尺寸 1，此时启动极小的 grid 并不是什么罕见的错误——它是写一个正确 kernel 所得到的*默认*结果。

本讲中的三个 PR，分别带来 +4.0%、+6.7% 和一次 21% 的跃升，它们之间没有任何新数学。一个加宽了 block。一个增加了 grid 轴。一个把硬编码常量替换成了推导式。它们之所以值得讲一讲，在于找到它们的*推理过程*，以及证明它们除速度外什么都没改变的那份严谨。

读完你应当能够：看一个 kernel 的启动配置，就说出它是否在饿死机器；在 profile 中区分饿死与争用；并证明一次启动形状的改动是 bit 完全一致的，而不仅仅是落在容差之内。

---

## 1. 没人做的那道算术

两个数，相乘。整个诊断就这么多。

```text
   H200 (sm_90):  132 SMs

   grid    1 block   →   0.8%  of SMs have work
   grid   12 blocks  →   9.1%
   grid   48 blocks  →  36.4%
   grid  128 blocks  →  97.0%   (but a tail: 132 would be one wave)
   grid  256 blocks  →  two waves, each ~full
```

之所以没人例行做这件事，是因为**症状看起来不像形状问题。** 一个饿死的 kernel 表现为实测带宽低、FLOPs 低、耗时比工作量应得的长——恰好就是带宽受限 kernel 的特征。每一个吞吐指标都同意你受带宽限制。它们没有一个提到 GPU 有 91% 处于空闲。

有区分度的测量是 grid 维度对照 SM 数量，而没有任何吞吐计数器会报告这一点。你得亲自去看。

### 1.1 为什么 decode 在结构上就容易这样

来自 [Lecture 03 §4.1](Lecture-03.md)：batch-1 decode 产生的张量，其序列维度是 **1**。batch、sequence、tile——训练 kernel 用来并行的每一条轴——都没了。剩下的是 heads、hidden width 和 experts。

所以本讲以及 [Lecture 06](Lecture-06.md) 中反复出现的修法，是同一个动作的三个名字：**在工作负载的天然并行轴耗尽之处，造出一条并行轴。** 按 context 切。按 value tile 切。按 expert group × FFN band 切。把 block 加宽，直到宽度本身成为并行度。

---

## 2. 屏障是对的；宽度是错的

[**PR #115**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115) —— *"widen the last single-block launch — 327 norms/token were running on 128 threads."*

发现是：`rms_norm_f32` 以 **一个 128 个标量线程的单个 block** 运行，处理宽度最高达 7168——而且它**每个 token 运行 327 次**。

接下来才是让它成为好教学案例的部分。天真地读会得出"一个 block 就是 bug，把它并行化"。这种读法是错的，PR 自己也这么说：

> *One block is correct (the mean must complete before any element is scaled). What was wrong is the width.*

RMS norm 需要在任何元素被缩放之前，对整行做一次完整归约。在单个 block 内那是 `__syncthreads()`；跨 block 则需要全局屏障或两趟 kernel。**单个 block 是正确的结构。** 缺陷在于，当行有 7168 个元素时，block 却只有 128 个线程宽。

这个修法作用在 **in-flight 字节数**上，而不是 block 数量上：

```text
   BEFORE   128 scalar threads          →  ~512 B in flight
   AFTER    block sized to the width,
            float4 loads where legal    →  1024 threads, ~16 KB in flight
                                           at hidden 7168

   the grid is still 1 block. the barrier is untouched.
   what changed is how much memory the block has outstanding.
```


<details>
<summary>English original</summary>

**Part 4 · Lecture 04 — Launch Geometry: Grids, Occupancy, and 327 Norms per Token**

**Overview**

The cheapest wins in this case study were not better algorithms. They were **the same arithmetic, launched in a different shape.**

An H200 has **132 streaming multiprocessors**. A kernel launched with 12 blocks uses 9% of it. A kernel launched with one block uses 0.8%. In batch-1 decode, where every natural parallel axis has collapsed to size 1, launching tiny grids is not an unusual mistake — it is the *default* outcome of writing a kernel that is correct.

Three PRs in this lecture, worth +4.0%, +6.7%, and a 21% step, contain no new mathematics between them. One widened a block. One added a grid axis. One replaced a hardcoded constant with a derivation. What makes them worth a lecture is the *reasoning* that found them and the discipline that proved they changed nothing but speed.

By the end you should be able to look at a kernel's launch configuration and say whether it is starving the machine, distinguish starvation from contention in a profile, and prove a launch-shape change is bit-identical rather than merely passing a tolerance.

---

**1. The arithmetic nobody does**

Two numbers, multiplied. That is the whole diagnostic.

```text
   H200 (sm_90):  132 SMs

   grid    1 block   →   0.8%  of SMs have work
   grid   12 blocks  →   9.1%
   grid   48 blocks  →  36.4%
   grid  128 blocks  →  97.0%   (but a tail: 132 would be one wave)
   grid  256 blocks  →  two waves, each ~full
```

The reason this is not done routinely is that **the symptom does not look like a shape problem.** A starved kernel shows low achieved bandwidth, low FLOPs, and a duration longer than the work justifies — exactly the fingerprint of a memory-bound kernel. Every throughput metric agrees that you are bandwidth-limited. None of them mentions that 91% of the GPU is idle.

The distinguishing measurement is grid dimensions against SM count, which no throughput counter reports. You have to go and look.

**1.1 Why decode is structurally prone to this**

From [Lecture 03 §4.1](Lecture-03.md): batch-1 decode produces tensors whose sequence dimension is **1**. Batch, sequence, and tile — every axis a training kernel parallelizes over — are gone. What remains is heads, hidden width, and experts.

So the recurring fix in this lecture and [Lecture 06](Lecture-06.md) is one move under three names: **manufacture a parallel axis where the workload's natural axes ran out.** Split over context. Split over value tiles. Split over expert groups × FFN bands. Widen the block until the width itself is the parallelism.

---

**2. The barrier was right; the width was wrong**

[**PR #115**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115) — *"widen the last single-block launch — 327 norms/token were running on 128 threads."*

The finding: `rms_norm_f32` ran as **a single block of 128 scalar threads** over widths up to 7168 — and it ran **327 times per token**.

Now the part that makes this a good teaching case. A naive reading is "one block is a bug, parallelize it." That reading is wrong, and the PR says so:

> *One block is correct (the mean must complete before any element is scaled). What was wrong is the width.*

RMS norm needs a full reduction over the row before any element can be scaled. Within a single block that is a `__syncthreads()`; across blocks it would need a global barrier or a two-pass kernel. **One block is the right structure.** The defect was that the block was 128 threads wide when the row was 7168 elements.

The fix works on **bytes in flight**, not on block count:

```text
   BEFORE   128 scalar threads          →  ~512 B in flight
   AFTER    block sized to the width,
            float4 loads where legal    →  1024 threads, ~16 KB in flight
                                           at hidden 7168

   the grid is still 1 block. the barrier is untouched.
   what changed is how much memory the block has outstanding.
```

</details>

### 2.1 被刻意*没有*加宽的那一半

下面这个细节让本例成为极好的教学案例，而且很容易被忽略：**平方和仍然正好在 128 个线程上运行。** 只有它之后的逐元素 apply 被加宽了。kernel 自己的注释解释了原因，值得仔细读：

```c
float acc = 0.0f;
if (threadIdx.x < 128) {                              /* the REDUCTION stays narrow */
    for (int d = (int)threadIdx.x; d < n; d += 128) acc += x[d] * x[d];
}
const float ss  = block_sum<BLOCK>(acc, shm);
const float inv = rsqrtf(ss / (float)n + eps);        /* then the APPLY goes wide */
```

> 改变有多少线程参与 `ss`，就会改变平方和的 float32 **结合顺序**，从而改变 `inv`，从而让**每一个输出元素**产生约 1e-7 的相对变化——足以把相对 `main` 的 KL 推过 2× 棘轮，同时仍能通过绝对 KL 门槛。

这并非假设：该分支的早先一次测量**正是因为这一原因**被贴上 `accuracy-regression` 标签（ctx2048 处为 main KL 的 4.63×）。最终版本在结合顺序不再有影响的那一行把 kernel 拆成两段：

```text
   the REDUCTION  → association-sensitive → must keep the same thread count
   the APPLY      → elementwise, once `inv` is fixed → widen freely, byte-identical
```

两个支撑性事实使这次拆分完全 bit-identical。超过 128 的空闲线程对 `block_sum` 贡献 `0`，而 `x + 0` 是 IEEE 恒等式——因此跨 1024 个线程的更长 warp 树求和产生的比特与 4-warp 版本相同。而在 `BLOCK == 128` 处没有向量化路径时，代码仍走*原始* kernel 而非特化版本，因此评审者可以一行就校验等价性，而不必去信任它。

> **在归约与逐元素 apply 的边界处拆分 kernel。** 归约的宽度是其数值特性的一部分；apply 的宽度则不是。加宽后者能在输出字节完全相同的前提下拿到带宽收益，而加宽前者是一种准确率改动，必须为之辩护。这与 [Lecture 06 §2.1](Lecture-06.md) 以及 [Lecture 08 §8](Lecture-08.md) 中的结合顺序论证相同，这里用它来决定*不优化哪里*。

还有两个值得效仿的工程细节：

* **向量路径是有条件的，标量路径是精确的。** 仅当 `n % 4 == 0` 且三个指针均为 16-byte 对齐时才使用 `float4`。回退路径不是降级的近似——它就是原始的精确路径。一条只在*某些时候*合法的快速路径，必须回退到正确性，而不是回退到「差不多就行」。
* **它留在既有的 launch 机制上。** launch 经过 `k3_pdl_launch`，使该 kernel 保持在 decode 链（逐 token 生成阶段）其余部分所使用的 programmatic-dependent-launch 路径上（[Lecture 08](Lecture-08.md)）。一项悄悄绕开引擎 launch 基础设施的优化，会把收益又还回去。

### 2.2 同样的思路两次都拿到 `none`

这项改动此前以 [#71](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/71) 提交过，并测量了三次：

| 轮次 | tok/s | top-1 | KL | 相对前沿 % | tier |
|---|--:|--:|--:|--:|---|
| 1 | 16.25 | 1.0 | 0.0065 | +1.2% | `eval:none` |
| 2 | 17.53 | 1.0 | 0.0035 | +0.4% | `eval:none` |
| 3 | 26.57 | 1.0 | 0.0026 | +1.8% | `eval:none` |

三次真实、正确、正向的测量——全部落在 2% 显著性门槛之内，全部得分为零。只有在以 #115 针对另一条前沿重新提交后，这项改动才拿到一个 tier。

两条教训，都来自 [Lecture 02](Lecture-02.md)，值得看着它们落在一个具体的 diff 上：**一次胜利是相对某个状态测得的，而不是一劳永逸**，并且**显著性门槛是一条绝对的 tok/s 线，会随着你的改进而移动。** 还要注意 tok/s 这一列意味着什么：该分支在第 1 轮测得 16.25，在第 3 轮测得 26.57，却始终未能超过前沿 2%——也就是说，在这个 PR 挂着未合期间，`main` 本身大约快了 63%，而这同一个 diff 的绝对贡献全程都落在门槛之内。

---

## 3. 饥饿，以及如何把它与争用区分开

[**PR #77**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77) — *「对 KDA decode 步骤做 value 分块 —— 132 个 SM 上只用 12 个 block（+6.7%，bit-identical）。」*

这个发现，陈述得异常精确：

```text
   kda_decode_step's grid IS n_head.

   #63 head-sharded KDA, taking n_head from 96 to 12 at tp=8.
   → 12 blocks on a 132-SM H200 = 9% of the device,
     consuming 11.8% of ALL GPU kernel time.
```


<details>
<summary>English original</summary>

**2.1 The half that was deliberately *not* widened**

Here is the detail that makes this a great teaching case, and it is easy to miss: **the sum of squares still runs on exactly 128 threads.** Only the elementwise apply after it was widened. The kernel's own comment explains why, and it is worth reading closely:

```c
float acc = 0.0f;
if (threadIdx.x < 128) {                              /* the REDUCTION stays narrow */
    for (int d = (int)threadIdx.x; d < n; d += 128) acc += x[d] * x[d];
}
const float ss  = block_sum<BLOCK>(acc, shm);
const float inv = rsqrtf(ss / (float)n + eps);        /* then the APPLY goes wide */
```

> Changing how many threads contribute to `ss` changes the float32 **association** of the sum of squares, which changes `inv`, which changes **every output element** by ~1e-7 relative — enough to push KL vs `main` over the 2× ratchet while still clearing the absolute KL bar.

And this is not hypothetical: an earlier measurement of this branch drew an **`accuracy-regression` label for exactly that reason** (ctx2048 at 4.63× main's KL). The final version splits the kernel at the line where association stops mattering:

```text
   the REDUCTION  → association-sensitive → must keep the same thread count
   the APPLY      → elementwise, once `inv` is fixed → widen freely, byte-identical
```

Two supporting facts make the split exactly bit-identical. Idle threads above 128 contribute `0` to `block_sum`, and `x + 0` is an IEEE identity — so the longer warp-tree sum over 1024 threads produces the same bits as the 4-warp one. And at `BLOCK == 128` with no vectorized path the code stays on the *original* kernel rather than the specialization, so a reviewer can check the equivalence in one line instead of trusting it.

> **Split a kernel at the boundary between its reduction and its elementwise apply.** The reduction's width is part of its numerics; the apply's is not. Widening the second gives you the bandwidth win with byte-identical output, while widening the first is an accuracy change you would have to defend. This is the same association argument as [Lecture 06 §2.1](Lecture-06.md) and [Lecture 08 §8](Lecture-08.md), used here to decide *where not to optimize*.

Two more details of craftsmanship worth copying:

* **The vector path is conditional and the scalar path is exact.** `float4` is used only when `n % 4 == 0` and all three pointers are 16-byte aligned. The fallback is not a degraded approximation — it is the original exact path. A fast path that is only *sometimes* legal must fail into correctness, not into "close enough."
* **It stays on the existing launch mechanism.** Launches go through `k3_pdl_launch`, keeping the kernel on the programmatic-dependent-launch path the rest of the decode chain uses ([Lecture 08](Lecture-08.md)). An optimization that quietly opts out of the engine's launch infrastructure gives back what it gains.

**2.2 The same idea scored `none` twice**

This change was submitted before as [#71](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/71) and measured three times:

| round | tok/s | top-1 | KL | % over frontier | tier |
|---|--:|--:|--:|--:|---|
| 1 | 16.25 | 1.0 | 0.0065 | +1.2% | `eval:none` |
| 2 | 17.53 | 1.0 | 0.0035 | +0.4% | `eval:none` |
| 3 | 26.57 | 1.0 | 0.0026 | +1.8% | `eval:none` |

Three real, correct, positive measurements — all inside the 2% significance gate, all scoring zero. The change only earned a tier when it was resubmitted as #115 against a different frontier.

Two lessons, both from [Lecture 02](Lecture-02.md) and worth seeing land on a specific diff: **a win is measured against a state, not for all time**, and **the significance gate is an absolute tok/s bar that moves as you improve.** Notice also what the tok/s column implies: the branch measured 16.25 in round 1 and 26.57 in round 3 while never clearing 2% over the frontier — so `main` itself got roughly 63% faster underneath this PR while it sat open, and the same diff's absolute contribution stayed inside the gate the whole time.

---

**3. Starvation, and how to tell it from contention**

[**PR #77**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77) — *"value-tile the KDA decode step — 12 blocks on 132 SMs (+6.7%, bit-identical)."*

The finding, stated with unusual precision:

```text
   kda_decode_step's grid IS n_head.

   #63 head-sharded KDA, taking n_head from 96 to 12 at tp=8.
   → 12 blocks on a 132-SM H200 = 9% of the device,
     consuming 11.8% of ALL GPU kernel time.
```

</details>

### 3.1 识别 starvation 的诊断

这是整个 PR 里最可迁移的一段，值得先原样引用，再逐句拆解：

> *在 ctx 131072 下做性能剖析，这个 kernel 每次 launch 搬运约 3.1 MB，**耗时 87 µs，而带宽下界约 0.9 µs**，**标准差 982 ns**。方差这么紧、与带宽相差 95×，既不是争用，也不是算术——这是 block 的 starvation。*

推理链条：

```text
   1.  bytes moved ÷ device bandwidth  =  0.9 µs   ← what it SHOULD cost
   2.  measured                        =  87 µs    ← 95× off
   3.  stddev                          =  982 ns   ← 1.1% of the mean

   contention would be NOISY   (other work interfering → high variance)
   arithmetic would be VISIBLE (FLOPs would account for the time)
   a 95× gap that is REPEATABLE TO 1% is a structural property
       of the launch, not of the machine's state.

   ⇒ the blocks are not there.
```

**方差才是判别依据。** 一个 kernel 慢，是因为它在和其他工作抢资源，那它慢得 *不稳定*。一个 kernel 慢，是因为它始终只占住设备的 9%，那它慢得像节拍器一样规律。把标准差和均值一起报出来，才能做出这种区分——而大多数性能剖析的文章都漏了这一点。

> **抄走这条。** 永远报出离散程度，而不只是均值。`87 µs ± 1 µs` 和 `87 µs ± 30 µs` 结论数字一样，却是两种不同的诊断。

### 3.2 造出这条轴，并证明它合法

这个修复在 **value 分块** 之上又加了一条 grid 轴，把 block 数从 12 变成 48。为什么这样做是允许的——这套论证才是要钻研的部分，因为只有当工作确实可分解时，“加一条 grid 轴”才是安全的：

> *每一步都是 **列局部** 的：对状态列 `j` 而言，decay、`sk[j] = Σᵢ S[i][j]·k[i]`、`d[j]`、rank-1 更新以及 `o[j]` 都只碰列 `j`，再加上 per-head 的 `k`/`q`/`g` 向量，而这些向量是只读且共享的。因此各列可以拆到不同 block 上，**不需要额外的归约，也不会有重复的状态流量**——一个 block 只读它自己那 `BV` 行 j-major buffer。*

让拆分变便宜的有三个性质，这里三个全占：

| 性质 | 为什么重要 | 本例 |
|---|---|---|
| 无跨 block 归约 | 否则就要付一次 combine pass | 各列相互独立 |
| 无重复读取 | 否则流量 × 拆分数 | 每个 block 只读自己的行 |
| 布局不变 | 否则就要付一次转置 | `S[i][j]` 在 `s[j*D+i]` 下仍然成立 |

对比 [Lecture 06](Lecture-06.md) 里的 *上下文* 拆分：它确实需要一次 combine pass——而且仍然值得。弄清自己属于哪一类拆分，就知道该期待什么。

**这个分块宽度不是调出来的——是引用来的。** `BV = 32` 正是参考实现在这个 kernel 上用的值：FLA 的 `fused_recurrent_gated_delta_rule`——vLLM 为 decode 发布的就是这个——用 `BV = 32` launch `grid = (cdiv(V, BV) · N · HV)`。

> **在开始扫参之前，先查参考实现是怎么做的。** 一条引用比跑一轮调参便宜，也比你在自己硬件上试出来的数字更通用。如果你的值跟参考实现的不一致，这很有意思，也值得弄明白——但起点要用它的值。

**同一个 PR 里还有第二项可独立拆出的收益。** pass 1 把 `S·exp(g)` 存到 global memory，pass 2 再把它读回来；现在 pass 2 改为重新计算这个乘积。这样就省掉了 **每次 launch 一次完整的状态写入——占该 kernel 全局流量的四分之一** ——外加每个 chunk 的一个 staging 循环和一个 barrier。用重算一个便宜的乘积来避免一趟 HBM 往返，是带宽受限取舍最纯粹的形式。

最终结果：在计分的 128k 上下文下 **52.96 → 49.63 ms/token**，**18.88 → 20.15 tok/s**，**+6.7%**，输出逐 bit 一致。

---


<details>
<summary>English original</summary>

**3.1 The diagnostic that identifies starvation**

This is the most transferable paragraph in the PR, and it deserves to be quoted and then unpacked:

> *Profiled at ctx 131072, the kernel moves ~3.1 MB per launch and takes **87 µs against a ~0.9 µs bandwidth floor**, with a **982 ns stddev**. Tight variance and a 95× gap to bandwidth is not contention and not arithmetic — it is starvation of blocks.*

The inference chain:

```text
   1.  bytes moved ÷ device bandwidth  =  0.9 µs   ← what it SHOULD cost
   2.  measured                        =  87 µs    ← 95× off
   3.  stddev                          =  982 ns   ← 1.1% of the mean

   contention would be NOISY   (other work interfering → high variance)
   arithmetic would be VISIBLE (FLOPs would account for the time)
   a 95× gap that is REPEATABLE TO 1% is a structural property
       of the launch, not of the machine's state.

   ⇒ the blocks are not there.
```

**Variance is the discriminator.** A kernel that is slow because it is fighting other work is slow *inconsistently*. A kernel that is slow because it only ever occupies 9% of the device is slow with metronome regularity. Reporting a standard deviation alongside a mean is what makes that distinction available — and most profiling writeups omit it.

> **Steal this.** Always report the spread, not just the mean. `87 µs ± 1 µs` and `87 µs ± 30 µs` are different diagnoses with the same headline.

**3.2 Manufacturing the axis, and proving it is legal**

The fix adds a second grid axis over **value tiles**, taking 12 blocks to 48. The justification for why this is allowed is the part to study, because "add a grid axis" is only safe if the work actually decomposes:

> *Every step is **column-local**: for state column `j`, the decay, `sk[j] = Σᵢ S[i][j]·k[i]`, `d[j]`, the rank-1 update and `o[j]` all touch only column `j` plus the per-head `k`/`q`/`g` vectors, which are read-only and shared. So columns split across blocks with **no extra reduction and no duplicated state traffic** — a block reads exactly its own `BV` rows of the j-major buffer.*

Three properties make a split cheap, and this one has all three:

| Property | Why it matters | Here |
|---|---|---|
| No cross-block reduction | Otherwise you pay a combine pass | Columns are independent |
| No duplicated reads | Otherwise traffic × number of splits | Each block reads its own rows |
| Layout unchanged | Otherwise you pay a transpose | `S[i][j]` at `s[j*D+i]` still holds |

Compare with the *context* split in [Lecture 06](Lecture-06.md), which does require a combine pass — and is still worth it. Knowing which kind of split you have tells you what to expect.

**The tile width was not tuned — it was cited.** `BV = 32` is what the reference implementations use for exactly this kernel: FLA's `fused_recurrent_gated_delta_rule`, which vLLM ships for decode, launches `grid = (cdiv(V, BV) · N · HV)` with `BV = 32`.

> **Look up what the reference implementation does before sweeping.** A citation is cheaper than a tuning run and generalizes better than a number you found on your own hardware. If your value disagrees with the reference's, that is interesting and worth understanding — but start from theirs.

**A second, separable win in the same PR.** Pass 1 stored `S·exp(g)` to global memory and pass 2 re-read it; now pass 2 recomputes the product instead. That removes **a full write of the state per launch — a quarter of the kernel's global traffic** — plus one staging loop and one barrier per chunk. Recomputing a cheap product to avoid a round trip through HBM is the memory-bound trade in its purest form.

Net result: **52.96 → 49.63 ms/token** at the scored 128k context, **18.88 → 20.15 tok/s**, **+6.7%**, output bit-identical.

---

</details>

## 4. 推导常量，不要写死

[**PR #73**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/73) —— *「推导出 MLA slice 下限，使 head 分片无法让 grid 搁浅。」*

MLA decode kernel（逐 token 生成阶段）有一个**固定的 1024-token slice 下限**——一个硬编码的最小 slice 尺寸。写的时候完全合理。后来 head 分片改变了 head 数量，这一相互作用让 grid 落到了相邻常量中那个 occupancy 目标之下。

修复方案从 shape 推导出这个下限。结果：

```text
   grid at ctx 131,072:   128 blocks  →  256 blocks   (on a 132-SM part)
   measured against the kMlaBlocksPerSm = 2 target beside it

   main      54.53 ms/token   18.34 tok/s
   this PR   52.44 ms/token   19.07 tok/s      +4.0%
```


注意目标：`kMlaBlocksPerSm = 2`，所以 264 个 block 就是「每个 SM 两个」，而 256 基本达到了。**每个 SM 两个 block 而非一个**是刻意为之——它让一个 block 的内存延迟与另一个 block 的计算重叠，这是每个 SM 只驻留一个 block 做不到的。

> **硬编码的 launch 常量是一份被缓存的推导结果，而缓存会过期。** 任何正确取值依赖于 `n_heads`、`tp_size`、`head_dim` 或上下文长度的常量，都应该由这些量计算得出，并在旁边写明它所瞄准的 occupancy 目标。写下*意图*（`kMlaBlocksPerSm = 2`），再推导出数字。

---

## 5. 每一次优化都在埋下下一个 occupancy bug

本讲最重要的结构性洞见是 #63、#73 与 #77 之间的链条，PR 作者直言不讳地指出了它：

> *「那次回归是我的责任。**#63 正是为这一失效预留了 MLA split cap（`kMlaSplitBudget = 96 · kMlaMaxSplits`）**——head 分片导致 grid 塌缩——却把三个函数之外的 KDA kernel 留在了每个 head 一个 block 的状态。#73 后来改进了 MLA 那一侧；没有人看过这一个。」*

顺序如下：

```text
   #63   head-shard both attention bands       →  n_head 96 → 12 at tp=8
         (an eval:xl win — see Lecture 06)
         author ANTICIPATED grid collapse and budgeted the MLA split cap for it

   #73   MLA slice floor derived from shape    →  the MLA side, fixed properly

   #77   KDA decode grid IS n_head             →  12 blocks. three functions
                                                  away from the fix, unnoticed
```


作者预见到了确切的失效模式，为一个 kernel 做了防护，却漏掉了几个函数之外的兄弟 kernel。这不是粗心；这就是降低并行度的改动*会*造成的结果。

> **任何把一个并行轴跨 rank 切分的改动，都会缩小由该轴推导出的每一个 grid。** 对一个维度分片之后，要审计**每一个** grid 提及该维度的 kernel。不是你想的那个——是全部。`grep` 是针对维度，不是针对 kernel。

还有一条关于归因的推论：#63 被评为 `xl` 是正确的。它测得的端到端收益是真实的。它同时也制造了一个潜伏的 −6.7%，又花了两个 PR 才找回来。这两个事实都成立，而只记录前者的阶梯与其说是在撒谎，不如说是不完整。这就是为什么本案例研究的逐 PR receipts 与其 frontier-on-`main` 测量是两组独立的数字（[Lecture 01 §5.1](Lecture-01.md)）。

---

## 6. 一个二进制，两条对照臂

本讲的每个 PR 都用了同一套测量技术，值得全盘采用。

每个改动都藏在一个 **runtime 环境开关**之后，所以「改前」与「改后」是*同一个编译出来的二进制*，只是 flag 不同：

```text
   #77:   off  = SPARKINFER_K3_KDA_VT=0    →  main's launch geometry
          vt   = SPARKINFER_K3_KDA_VT=1    →  the new grid

          "Both arms are the same binary [...] so nothing but the grid
           differs and no rebuild separates them."
```


这样做消除的是：build flag 漂移、编译器版本差异、驱动状态、链接顺序效应，以及整类「我们重新构建了，结果别的东西也变了」。这些都不是假设——它们正是 [Lecture 02](Lecture-02.md) 记录的那个 PR 不得不*用固定版本的编译器从头重建每一次评测*的原因。

[**PR #127**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/127) 把它推到了终点。它打包了十二个改动，每个都单独加开关，并发布了**完整的 restore set**，可在同一个二进制上还原出 `main` 的确切行为：

```text
   SPARKINFER_K3_MOE_WEPS=0 IQ1S_PACK=0 MLA_PVT=0 B2WIDE=0
   COLL_BLOCK=512 WARP_TARGET=1792 ROUTER_REG=0 HEAD_1BAR=0
   RES_FUSE=0 RMSG=0 RMSU=0 COLL_CAP=36 FUSED4_ROWS=4 MLA_KLEN=0
```


> *「每个改动都在 runtime 加开关；restore set [...] 可在同一个二进制上原地还原出 `main` 的确切行为，**无需 revert**。」*

最后那句才是运维上的收益。如果那十二个改动中有任何一个在生产环境中造成了回归，缓解手段就是一个环境变量——而不是 revert、重新构建、再重新部署。


<details>
<summary>English original</summary>

**4. Derive the constant; do not pin it**

[**PR #73**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/73) — *"derive the MLA slice floor so head-sharding cannot strand the grid."*

The MLA decode kernel had a **flat 1024-token slice floor** — a hardcoded minimum slice size. Perfectly reasonable when written. Then head-sharding changed the head count, and the interaction stranded the grid below the occupancy target sitting in the adjacent constant.

The fix derives the floor from the shape. The result:

```text
   grid at ctx 131,072:   128 blocks  →  256 blocks   (on a 132-SM part)
   measured against the kMlaBlocksPerSm = 2 target beside it

   main      54.53 ms/token   18.34 tok/s
   this PR   52.44 ms/token   19.07 tok/s      +4.0%
```

Note the target: `kMlaBlocksPerSm = 2`, so 264 blocks is "two per SM" and 256 is essentially there. **Two blocks per SM rather than one** is deliberate — it lets one block's memory latency overlap with another's compute, which a single resident block per SM cannot do.

> **A hardcoded launch constant is a cached derivation, and caches go stale.** Any constant whose correct value depends on `n_heads`, `tp_size`, `head_dim`, or context length should be computed from those, with the occupancy target it is aiming at named next to it. Write the *intent* (`kMlaBlocksPerSm = 2`) and derive the number.

---

**5. Each optimization plants the next occupancy bug**

The most important structural insight in this lecture is the chain between #63, #73, and #77, and the PR author names it plainly:

> *"That regression is mine. **#63 budgeted the MLA split cap (`kMlaSplitBudget = 96 · kMlaMaxSplits`) for precisely this failure** — head-sharding collapsing a grid — and left the KDA kernel three functions away at one block per head. #73 has since refined the MLA side; nobody had looked at this one."*

The sequence:

```text
   #63   head-shard both attention bands       →  n_head 96 → 12 at tp=8
         (an eval:xl win — see Lecture 06)
         author ANTICIPATED grid collapse and budgeted the MLA split cap for it

   #73   MLA slice floor derived from shape    →  the MLA side, fixed properly

   #77   KDA decode grid IS n_head             →  12 blocks. three functions
                                                  away from the fix, unnoticed
```

The author foresaw the exact failure mode, defended one kernel against it, and missed the sibling kernel a few functions away. This is not carelessness; it is what parallelism-reducing changes *do*.

> **Any change that divides a parallel axis across ranks reduces every grid derived from that axis.** After sharding a dimension, audit **every** kernel whose grid mentions it. Not the one you were thinking about — all of them. `grep` for the dimension, not for the kernel.

And a corollary about attribution: #63 was correctly scored `xl`. Its measured end-to-end win was real. It also created a latent −6.7% that took two more PRs to recover. Both facts are true, and a ladder that records only the first is not lying so much as incomplete. This is why the case study's per-PR receipts and its frontier-on-`main` measurements are separate numbers ([Lecture 01 §5.1](Lecture-01.md)).

---

**6. One binary, two arms**

Every PR in this lecture used the same measurement technique, and it is worth adopting wholesale.

Each change is behind a **runtime environment gate**, so "before" and "after" are the *same compiled binary* with a different flag:

```text
   #77:   off  = SPARKINFER_K3_KDA_VT=0    →  main's launch geometry
          vt   = SPARKINFER_K3_KDA_VT=1    →  the new grid

          "Both arms are the same binary [...] so nothing but the grid
           differs and no rebuild separates them."
```

What this eliminates: build-flag drift, compiler version differences, driver state, link-order effects, and the whole class of "we rebuilt and something else changed." Those are not hypothetical — they are why [Lecture 02](Lecture-02.md) records a PR that had to *rebuild every eval from scratch with a pinned compiler.*

[**PR #127**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/127) takes it to its conclusion. It bundles a dozen changes, each individually gated, and publishes the **complete restore set** that returns `main`'s exact behaviour on one binary:

```text
   SPARKINFER_K3_MOE_WEPS=0 IQ1S_PACK=0 MLA_PVT=0 B2WIDE=0
   COLL_BLOCK=512 WARP_TARGET=1792 ROUTER_REG=0 HEAD_1BAR=0
   RES_FUSE=0 RMSG=0 RMSU=0 COLL_CAP=36 FUSED4_ROWS=4 MLA_KLEN=0
```

> *"Every change is gated at runtime; the restore set [...] returns `main`'s exact behaviour in place on one binary, **without a revert**."*

That last clause is the operational payoff. If any of those twelve changes turns out to regress something in production, the mitigation is an environment variable — not a revert, a rebuild, and a redeploy.

</details>

### 6.1 ABBA 交错，以及报告每次重复的数值

#127 的测量纪律是另一半：

```text
   A_this   16.88 / 16.87 / 16.88   →  16.88 ms   (59.26 tok/s)
   B_main   20.46 / 20.50 / 20.46   →  20.46 ms   (48.87 tok/s)
```

交错 A/B/B/A，三对，**每次重复的数值单独打印**。公布每次重复的数值而不是均值，读者就能自己看到离散程度，也正是这一点让 §3.1 的方差论证能被别人复核。

### 6.2 两台仪器必然不一致 —— 每次比较只选一台

#77 报告了同一改动用两种方式测得的结果，并解释差异而不是隐藏它：

| 仪器 | 改动前 | 改动后 | 差值 |
|---|--:|--:|--:|
| `kimi_k3_eval.sh`（计分仪器） | 18.88 | 20.15 | **+6.7%** |
| `kimi_k3_tp_bench`，136 tokens，3 次重复 | 18.86 | 20.03 | **+6.2%** |

> *“我的 32-token bench 和 harness（agent 运行时框架）在绝对速度上不一致 —— harness 分别计时 8-token 和 136-token 的运行并取边际差分，约 7 s 的持续负载对约 1.6 s —— 所以只有当两条臂使用同一台仪器时，A/B 才有意义。”*

**两个差值都成立；两个绝对值彼此不可比。** 规则是：差值只有在一台仪器内部才有意义，而决定档位的那台仪器才是重要的那台。把两者都报出来，并说明它们为何不同，严格优于只挑那个好看的数字。

同一 PR 还标记了它*无法*解决的一处问题 —— 与 eval bot 运行结果的模型指纹不一致（`8c3b548967ff7583` vs `4128f23a2dcea136`），理由是权重几乎肯定完全相同，因为 `mean_kld` 能复现到全部 18 位数字。随后写道：*“我没有证明这一点，与其把它从一份引用节点数字的 PR 里略去，不如把它标记出来。”* 这才是性能声明中出现无法解释的观测时正确的处理方式。

---

## 7. 用字节证明逐位一致

这三个 PR 中有两个声称**逐位一致**。这是个很强的声明，需要很强的校验。

**校验手段是 `cmp`，而不是容差。** 来自 #77：

> *“在 `SPARKINFER_K3_KDA_VT=1` 和 `=0` 下，`cmp` 对 logits dump 的报告是**字节完全相同** [...] `kimi_k3_numeric_test` 种入一个随机非零状态，并把 `out` 和该状态都对照 float64 参考做检查 [...] **但这是容差检查，所以逐位一致的声明所依赖的是上面的 `cmp`，而不是这个测试。**”*

在 `1e-6` 下通过的数值测试只能告诉你这个改动是*可接受的*，不能告诉你它是*完全相同的*。如果你声称逐位一致，就去比较字节。

#127 在两个深度上做了同样的事，并以逐深度表格的形式报告：

```text
   ctx128: IDENTICAL   ctx256: IDENTICAL   ctx512: IDENTICAL
   ctx1024: IDENTICAL  ctx2048: IDENTICAL  ctx4096: IDENTICAL

   and at 2x the deepest graded depth, 8192 real ids, full 93 layers:
   BIT-IDENTICAL: default build == all-gates-off, one binary
```

### 7.1 FMA 陷阱

这是本篇中最精细的细节，来自 #77 的写消除。值得读两遍。

原来的 pass 1 会把乘积存入共享内存；新版本把它直接向后传递。该 PR 用的是 `__fmul_rn` 而不是 `*`：

> *旧的 pass 1 把乘积存入共享内存，这迫使它被具现化为一个**舍入后的 f32**，而直接喂给一个加法会让 `nvcc`（默认 `--fmad=true`）**把它收缩成一个 `fma` 并跳过那次舍入**。这会在通过套件里每一项容差检查的同时，挪动最低的几个比特。*

拆开来看：

```text
   OLD:  tmp = a * b;        // rounded to f32, because it went to shared
         ...
         out = tmp + c;      // add of a ROUNDED product

   NAIVE NEW:  out = a * b + c;
                     ^^^^^^^^^ nvcc contracts to fma(a, b, c)
                               → the product is NOT rounded first
                               → different last bits

   CORRECT NEW:  out = __fmul_rn(a, b) + c;
                       ^^^^^^^^^^ forces the round-to-nearest f32 product,
                                  reproducing the old rounding exactly
```

Fused multiply-add *更* 准确 —— 它保留了完整的中间精度。但“更准确”不等于“完全相同”，而逐位一致的声明是一个关于*复现原先那次舍入*的声明，包括复现它的损失。

> **当你消除一次经由内存的往返时，你可能也消除了一次舍入。** 去掉一次存储就去掉了一次具现化，编译器随后会乐于跨过这道接缝把算术收缩起来。如果你想要原来的比特，就强制使用原来的舍入 —— `__fmul_rn`、`__fadd_rn`，或作用域限于该文件的 `-fmad=false`。

这也最清楚地说明了为什么容差检查无法管住逐位一致：FMA 收缩会挪动最低的几个比特，并且*改善*准确率。它会在让该声明变为假的同时通过套件里的每一项数值测试。

---


<details>
<summary>English original</summary>

**6.1 ABBA interleaving, and reporting per-rep numbers**

#127's measurement discipline is the other half:

```text
   A_this   16.88 / 16.87 / 16.88   →  16.88 ms   (59.26 tok/s)
   B_main   20.46 / 20.50 / 20.46   →  20.46 ms   (48.87 tok/s)
```

Interleaved A/B/B/A, three pairs, **individual reps printed**. Publishing the reps rather than the mean lets a reader see the spread themselves and is what makes §3.1's variance argument checkable by someone else.

**6.2 Two instruments will disagree — pick one per comparison**

#77 reports the same change measured two ways, and explains the discrepancy rather than hiding it:

| instrument | before | after | delta |
|---|--:|--:|--:|
| `kimi_k3_eval.sh` (the scoring instrument) | 18.88 | 20.15 | **+6.7%** |
| `kimi_k3_tp_bench`, 136 tokens, 3 reps | 18.86 | 20.03 | **+6.2%** |

> *"My 32-token bench and the harness disagree on absolute speed — the harness times 8- and 136-token runs and takes the marginal differential, ~7 s of sustained load against ~1.6 s — so an A/B is only meaningful when both arms use one instrument."*

**Both deltas are valid; neither absolute number is comparable to the other's.** The rule: a delta is only meaningful within a single instrument, and the instrument that decides the tier is the one that matters. Reporting both, with the reason they differ, is strictly better than picking the flattering one.

The same PR also flags something it *could not* resolve — a model fingerprint mismatch against the eval bot's runs (`8c3b548967ff7583` vs `4128f23a2dcea136`), with the reasoning that the weights are almost certainly identical because `mean_kld` reproduces to all 18 digits. Then: *"I have not proven that, and would rather flag it than leave it out of a PR quoting node numbers."* That is the correct handling of an unexplained observation in a performance claim.

---

**7. Proving bit-identity as bytes**

Two of these three PRs claim **bit-identical**. That is a strong claim and it needs a strong check.

**The check is `cmp`, not a tolerance.** From #77:

> *`cmp` of the logits dumps under `SPARKINFER_K3_KDA_VT=1` and `=0` reports **byte-identical** [...] `kimi_k3_numeric_test` seeds a random non-zero state and checks both `out` and the state against a float64 reference [...] **But it is a tolerance check, so the `cmp` above — not the test — is what the bit-identical claim rests on.**"

A numeric test that passes at `1e-6` tells you the change is *acceptable*. It cannot tell you it is *identical*. If you claim bit-identity, compare bytes.

#127 does the same at two depths, and reports it as a per-depth table:

```text
   ctx128: IDENTICAL   ctx256: IDENTICAL   ctx512: IDENTICAL
   ctx1024: IDENTICAL  ctx2048: IDENTICAL  ctx4096: IDENTICAL

   and at 2x the deepest graded depth, 8192 real ids, full 93 layers:
   BIT-IDENTICAL: default build == all-gates-off, one binary
```

**7.1 The FMA trap**

The finest detail in this lecture, from #77's write-elision. It is worth reading twice.

Pass 1 used to store a product to shared memory; the new version feeds it onward directly. The PR uses `__fmul_rn` rather than `*`:

> *The old pass 1 stored the product to shared, which forced it to be materialised as a **rounded f32**, and feeding an add directly would let `nvcc` (`--fmad=true` by default) **contract it into an `fma` and skip that rounding**. That would move the last bits while passing every tolerance check in the suite.*

Unpacked:

```text
   OLD:  tmp = a * b;        // rounded to f32, because it went to shared
         ...
         out = tmp + c;      // add of a ROUNDED product

   NAIVE NEW:  out = a * b + c;
                     ^^^^^^^^^ nvcc contracts to fma(a, b, c)
                               → the product is NOT rounded first
                               → different last bits

   CORRECT NEW:  out = __fmul_rn(a, b) + c;
                       ^^^^^^^^^^ forces the round-to-nearest f32 product,
                                  reproducing the old rounding exactly
```

Fused multiply-add is *more* accurate — it keeps the full intermediate precision. But "more accurate" is not "identical," and a bit-identity claim is a claim about *reproducing the previous rounding*, including its losses.

> **When you eliminate a round trip through memory, you may also be eliminating a rounding.** Removing a store removes a materialization, and the compiler will then happily contract the arithmetic across the seam. If you want the old bits, force the old rounding — `__fmul_rn`, `__fadd_rn`, or `-fmad=false` scoped to the file.

This is also the clearest possible illustration of why a tolerance check cannot police bit-identity: an FMA contraction moves the last bits and *improves* accuracy. It would pass every numeric test in the suite while making the claim false.

---

</details>

## 8. 当 launch shape 变更并非无代价

并非每次 reshape 都是 bit 完全一致的，#73 就是一个诚实的反例。

> *并非 bit 完全一致：**提高 split 数量会重新结合 online-softmax 的合并**，split 路径一贯如此。*

把 attention 切分到更多切片，会改变各 partial softmax 结果合并的*顺序*。浮点加法不满足结合律，因此不同的 split 数量会得到不同的末位 bit。这是 split-K/split-context attention 的固有性质，而非缺陷（[Lecture 06](Lecture-06.md)）。

然后是整段里最有价值的一句，因为它讲的是这道 gate 的局限：

> *"端到端准确率 gate 看不到这一变更——它评分的是一段短参考 prompt，而 split 只在超过 `kMlaSplitMinCtx` 时才启用——所以**这道 gate 通过与否在这里都不构成证据。**"*

parity gate 跑在 ≤4096 token 上；split 路径只在长上下文才启用。所以这道 gate 对这一变更*保持沉默*——作者如实说明，而不是拿一个绿灯 gate 当验证。对比 [Lecture 02 §7](Lecture-02.md) 中明说的局限：*"4096 并不是你被打分的那个 131,072。"* 这里就有一次具体的变更，正好落在这个盲区里。

> **只有当 gate 真正跑到你改动的代码时，它通过才构成证据。** 在引用绿色 CI 之前，先问哪一道 gate 真的走到了新路径。如果一道都没有，就如实说明。

#127 以相反的方式处理类似情况——靠*构造*而非靠测量。它的 expert-weight 阈值会丢弃那些重归一化后 router 权重低于 `8e-2` 的 routed-expert 贡献，但仅在 position 16384 之后生效：

* 在 16k 以下，gate 把它**关掉**，因此在 parity 套件评分的每个深度上它都不改变任何字节。
* 浅层深度阈值化**经测量已被证明有害**——ctx 2048 无 gate 时 KL 1.1e-1——并且是靠构造排除而非靠约定排除。
* 超过 16k 后，它只作用于 router 自身权重分布被展平后的尾部，而输出的 norm 会吸收该尺度。

> *"gate 是质量守卫，不是优化手段。"*

对于你无法完全验证的近似，这才是正确的形态：**把它限定在你手上有论据的区间内，在一切可测量的地方证明它是惰性的，并记录下促使你给它设界的那次测量。**

---

## 实验 — 审计你的 launch geometry

1. **导出每个 kernel 的 grid 和 block。** 给你的 launch 加埋点，或从性能分析器里读取。产出一张表：kernel、grid、block、每 token 调用次数、每 token 总耗时。
2. **加上 occupancy 列。** `blocks ÷ SMs`。把低于 50% 的全部标出来。
3. **按浪费的 SM-time 排序**——`time_per_token × (1 − occupancy)`。这是按*可回收*时间排序，而不是按时长，顺序会和你的性能分析器给出的不同。
4. **对你最大的那个问题 kernel，做 #77 那套计算。** 搬运字节数 ÷ 设备带宽 = 下限。实测时间。比值。**以及 ≥10 次重复的标准差。** 方差小且比值大意味着饥饿；方差大意味着争用。
5. **找一个轴。** 对那个 kernel，找出一个可以切分的维度，要求没有跨 block 归约、没有重复读取、没有 layout 变更。如果三者都不成立，你面对的就是一个真正更难的问题——说明是哪一个不成立。
6. **在 runtime 层面加开关。** 把新 geometry 放在一个环境变量后面实现，让两条臂共用同一个二进制。做 ABBA 交错测量，三对，并公布每一次单独的重复。
7. **证明你所做的正确性声明。** 如果是 bit 完全一致：在不止一个深度上 `cmp` 输出字节。如果不是：说明是什么被重新结合了，并说明你的 gate 能否看到这一变更。
8. **对你的 sharded 维度做 `grep`。** 对并行度所切分的每一个维度，找出所有 grid 中提到它的 kernel（§5）。报告这份清单，即便其中有些没问题也要报。

通过标准：提交一张按可回收 SM-time 给 kernel 排序的表，一个用单二进制 ABBA 测过的 reshape 后 kernel，以及一份由正确类型的检查支撑的正确性声明。

---


<details>
<summary>English original</summary>

**8. When a launch-shape change is not free**

Not every reshape is bit-identical, and #73 is the honest counter-example.

> *Not bit-identical: **raising the split count reassociates the online-softmax combine**, as the split path has always done.*

Splitting attention over more slices changes the *order* in which partial softmax results are combined. Floating-point addition is not associative, so a different split count gives different last bits. That is inherent to split-K/split-context attention, not a defect ([Lecture 06](Lecture-06.md)).

And then the most valuable sentence, because it is about the limits of the gate:

> *"The end-to-end accuracy gate cannot see this change — it scores a short reference prompt, and splitting only engages above `kMlaSplitMinCtx` — so **that gate passing is not evidence either way here.**"*

The parity gate runs at ≤4096 tokens; the split path engages only at long context. So the gate is *silent* on this change — and the author says so rather than presenting a green gate as validation. Compare [Lecture 02 §7](Lecture-02.md)'s stated limit: *"4096 is not the 131,072 you are scored at."* Here is a specific change that lands squarely in that blind spot.

> **A passing gate is only evidence if the gate exercises the code you changed.** Before citing green CI, ask which of your gates actually reached the new path. If none did, say so.

#127 handles a similar situation the opposite way — by *construction* rather than by measurement. Its expert-weight threshold drops routed-expert contributions whose renormalized router weight is below `8e-2`, but only past position 16384:

* Below 16k the gate holds it **off**, so it is byte-inert at every depth the parity suite grades.
* Shallow-depth thresholding **was measured and found harmful** — KL 1.1e-1 at ctx 2048 ungated — and is excluded by construction rather than by convention.
* Past 16k it acts only on the flattened tail of the router's own weight distribution, and the output norm absorbs the scale.

> *"The gate is a quality guard, not an optimisation."*

That is the right shape for an approximation you cannot fully validate: **bound it to the regime where you have an argument, prove it inert everywhere you can measure, and document the measurement that made you bound it.**

---

**Lab — audit your launch geometry**

1. **Dump every kernel's grid and block.** Instrument your launches or read them from a profiler. Produce a table: kernel, grid, block, calls per token, total time per token.
2. **Add the occupancy column.** `blocks ÷ SMs`. Flag everything under 50%.
3. **Sort by wasted SM-time** — `time_per_token × (1 − occupancy)`. This ranks by *recoverable* time, not by duration, and the order will differ from your profiler's.
4. **For your top offender, do the #77 calculation.** Bytes moved ÷ device bandwidth = floor. Measured time. Ratio. **And the standard deviation over ≥10 reps.** Tight variance plus a large ratio means starvation; wide variance means contention.
5. **Find an axis.** For that kernel, identify a dimension you can split with no cross-block reduction, no duplicated reads, and no layout change. If all three fail, you have a genuinely harder problem — say which one failed.
6. **Gate it at runtime.** Implement the new geometry behind an environment variable so both arms are one binary. Measure ABBA-interleaved, three pairs, and publish the individual reps.
7. **Prove the correctness claim you are making.** If bit-identical: `cmp` the output bytes, at more than one depth. If not: say what reassociates, and state whether your gate can even see the change.
8. **`grep` for your sharded dimensions.** For every dimension your parallelism divides, find every kernel whose grid mentions it (§5). Report the list, even the ones that are fine.

Pass criterion: a committed table ranking your kernels by recoverable SM-time, one reshaped kernel measured single-binary ABBA, and a correctness claim backed by the right kind of check.

---

</details>

## 自检

1. 一个 kernel 耗时 87 µs，而带宽表明应为 0.9 µs，十次重复的标准差为 982 ns。给出诊断，以及你已排除的两个备选诊断，并说明各自的理由。
2. RMS norm 需要全行归约，因此结构上需要一个 block。给出两种不增加 block 的加速方法，并说明各自提高了什么。
3. 在同一个 kernel 中，归约保持在 128 个线程上，而 apply 展宽到 1024。解释为什么展宽归约会带来准确率变化，追踪从 `ss` 到输出的传播，并给出使展宽后的 apply 完全位一致的两个事实。
4. 你的团队将 attention head 按 8 路分片，以获得大的端到端收益。写出你随后立即运行的审计，并说明你为何 `grep`。
5. 你移除了一次对共享内存的存储，并将乘积直接送入一个加法。你的容差测试仍然通过，但 `cmp` 报告了不同的字节。发生了什么，单 token 修复是什么？
6. `kMlaBlocksPerSm = 2` 而不是 1。为什么你会以每个 SM 两个驻留 block 为目标，而不是一个？
7. 一个 PR 声称位完全一致，并引用了一个在 `1e-6` 通过的数字测试。写出两句话的评审意见。
8. 你的准确率门禁是绿的，但你改动的代码路径只在超过 16k 上下文时才生效，而门禁在 4k 下运行。你能从绿色门禁得出什么结论？
9. 一个正确、正向、测量良好的优化三次得到 `none`，然后在没有代码改动的情况下得到 `s`。解释原因，并说明在此期间你如何处理该分支。

---

## 参考文献

* **三个 PR** — [#115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115)（展宽单 block norm）、[#77](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77)（对 KDA decode 步骤做 value 分块）、[#73](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/73)（推导 MLA slice floor），加上 [#127](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/127)（小 grid 展宽 + restore 集合）以及 [#71](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/71)（与 #115 相同的思路，评分 `none`）。它们的正文是本讲的主要来源。
* **Flash Linear Attention (FLA)** — [github.com/fla-org/flash-linear-attention](https://github.com/fla-org/flash-linear-attention) — `fused_recurrent_gated_delta_rule`，#77 引用而非调优的参考启动形状（`BV = 32`）。已在 vLLM 中发布，用于 gated-delta decode。
* **Gated DeltaNet** — [arXiv:2412.06464](https://arxiv.org/abs/2412.06464) — 其列局部性使 #77 的拆分合法的递推。
* **CUDA occupancy** — [CUDA C++ Best Practices Guide § Occupancy](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/#occupancy)；用于计算每个 SM 可达到的 block 数的 Occupancy Calculator API。
* **FMA contraction** — [CUDA C++ Programming Guide § Mathematical Functions](https://docs.nvidia.com/cuda/cuda-c-programming-guide/#mathematical-functions-appendix) 以及 `--fmad` / `__fmul_rn` / `__fadd_rn` intrinsics。§7.1 背后的机制。
* **“What Every Computer Scientist Should Know About Floating-Point Arithmetic”** — Goldberg，[ACM Computing Surveys 1991](https://dl.acm.org/doi/10.1145/103162.103163) — 非结合性，§8 重结合的根源。

交叉引用：

* [Lecture 03 — Diagnosis](Lecture-03.md) — 上下文中的 occupancy 上限，以及为什么约束上限会移动。
* [Lecture 06 — Attention at 128k](Lecture-06.md) — 上下文拆分，它*确实*需要 combine pass，以及 #63 为 §5 做铺垫的 head 分片。
* [Lecture 08 — Graph-resident decode](Lecture-08.md) — `k3_pdl_launch` 以及 #115 保持使用的启动基础设施。
* [Lecture 10 — Silently wrong](Lecture-10.md) — 为什么 §7 中 `cmp` 与容差的区分并非吹毛求疵。

---

## 截至 2026-08

8× H200 SXM，`sm_90`，132 SMs，CUDA 12.8+。测量来自 PR #73 / #77 / #115 / #127，在评分所用的 131,072-token 上下文、UD-IQ1_S、tp=8 下；全部为单二进制、由环境变量门控的 A/B。`BV = 32` 按 FLA 的 gated-delta decode kernel。occupancy 算术、starvation-vs-contention 判别器，以及位一致性纪律才是持久内容。

---

## 下一讲

* 下一讲：[Lecture 05 — Fusion and the activation-quantization discipline](Lecture-05.md)
* 上一讲：[Lecture 03 — Diagnosis: launch-bound, bandwidth-bound, or comm-bound?](Lecture-03.md)
* 上级：[Part 4 — Optimizing a Real Engine](README.md)


<details>
<summary>English original</summary>

**Self-check**

1. A kernel takes 87 µs where bandwidth says 0.9 µs, with a 982 ns standard deviation over ten reps. Name the diagnosis and the two alternatives you have ruled out, with the reasoning for each.
2. RMS norm needs a full-row reduction, so one block is structurally required. Give two ways to make it faster that do not add blocks, and say what each raises.
3. In that same kernel, the reduction stays on 128 threads while the apply widens to 1024. Explain why widening the reduction would be an accuracy change, trace the propagation from `ss` to the output, and give the two facts that make the widened apply exactly bit-identical.
4. Your team shards attention heads 8 ways for a large end-to-end win. Write the audit you run immediately afterwards, and say what you `grep` for.
5. You remove a store to shared memory and feed the product straight into an add. Your tolerance test still passes but `cmp` reports differing bytes. What happened, and what is the one-token fix?
6. `kMlaBlocksPerSm = 2` rather than 1. Why would you target two resident blocks per SM instead of one?
7. A PR claims bit-identical and cites a numeric test passing at `1e-6`. Write the two-sentence review comment.
8. Your accuracy gate is green, but the code path you changed only engages above 16k context and the gate runs at 4k. What may you conclude from the green gate?
9. A correct, positive, well-measured optimization scores `none` three times and then `s` with no code change. Explain, and say what you do with the branch in the meantime.

---

**References**

* **The three PRs** — [#115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115) (widen the single-block norm), [#77](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77) (value-tile the KDA decode step), [#73](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/73) (derive the MLA slice floor), plus [#127](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/127) (small-grid widening + the restore set) and [#71](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/71) (the same idea as #115, scored `none`). Their bodies are the primary source for this lecture.
* **Flash Linear Attention (FLA)** — [github.com/fla-org/flash-linear-attention](https://github.com/fla-org/flash-linear-attention) — `fused_recurrent_gated_delta_rule`, the reference launch shape (`BV = 32`) that #77 cites rather than tunes. Shipped in vLLM for gated-delta decode.
* **Gated DeltaNet** — [arXiv:2412.06464](https://arxiv.org/abs/2412.06464) — the recurrence whose column-locality makes #77's split legal.
* **CUDA occupancy** — [CUDA C++ Best Practices Guide § Occupancy](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/#occupancy); the Occupancy Calculator API for computing achievable blocks per SM.
* **FMA contraction** — [CUDA C++ Programming Guide § Mathematical Functions](https://docs.nvidia.com/cuda/cuda-c-programming-guide/#mathematical-functions-appendix) and the `--fmad` / `__fmul_rn` / `__fadd_rn` intrinsics. The mechanism behind §7.1.
* **"What Every Computer Scientist Should Know About Floating-Point Arithmetic"** — Goldberg, [ACM Computing Surveys 1991](https://dl.acm.org/doi/10.1145/103162.103163) — non-associativity, the root of §8's reassociation.

Cross-references:

* [Lecture 03 — Diagnosis](Lecture-03.md) — the occupancy ceiling in context, and why the binding ceiling moves.
* [Lecture 06 — Attention at 128k](Lecture-06.md) — the context split, which *does* need a combine pass, and #63's head-sharding that set up §5.
* [Lecture 08 — Graph-resident decode](Lecture-08.md) — `k3_pdl_launch` and the launch infrastructure #115 stays on.
* [Lecture 10 — Silently wrong](Lecture-10.md) — why the `cmp`-versus-tolerance distinction in §7 is not pedantry.

---

**Current as of 2026-08**

8× H200 SXM, `sm_90`, 132 SMs, CUDA 12.8+. Measurements from PRs #73 / #77 / #115 / #127 at the scored 131,072-token context, UD-IQ1_S, tp=8; all single-binary env-gated A/B. `BV = 32` per FLA's gated-delta decode kernel. The occupancy arithmetic, the starvation-vs-contention discriminator, and the bit-identity discipline are the durable content.

---

**Next**

* Next: [Lecture 05 — Fusion and the activation-quantization discipline](Lecture-05.md)
* Previous: [Lecture 03 — Diagnosis: launch-bound, bandwidth-bound, or comm-bound?](Lecture-03.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
