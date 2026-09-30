---
title: 第 4 部分 · 第 05 讲 — 融合与激活值量化纪律
description: 第 4 部分 · 第 05 讲 — 融合与激活值量化纪律
---

# 第 4 部分 · 第 05 讲 — 融合与激活值量化纪律

## 概述

案例研究历史上最大的一次单步提升是 **3.54 → 9.94 tok/s** —— 2.77× 的提升，bit-identical，来自**同一个 kernel 中的五个相互独立的缺陷**。不是新算法。是同一个 GEMV 白白浪费机器的五种方式。

本讲讲的就是那个 kernel 及其邻近部分：模型中每一次非专家矩阵乘都要经过的量化投影路径，以及**激活值量化**的纪律 —— 一次性地、正确地决定一个向量在哪里被编码为 int8，以及谁有权读取它。

本讲也是某个具体而反复出现的观察的落脚点。三个各自独立的 PR 都发现某个 kernel 在一块 **4.8 TB/s 的芯片上只跑出约 7 GB/s**，并正确地断定：

> *那不是计算，那是启动开销。*

读完本讲，你应当能够：按 §2 中的五个缺陷审计一个量化 GEMV，判断某个激活值应当被提升还是被融合，以及 —— 最重要的一点 —— 设计一种共享缓冲区优化，使得一旦犯错，结果是*变慢*而不是*出错*。

---

## 1. decode（逐 token 生成阶段）的时间究竟花在哪里

在批大小为 1 的稀疏 MoE（混合专家模型）中，专家权重是*字节数*的大头。但**投影** —— Q/K/V/gate、MLA 的 down/up 投影、router、共享专家、LM head —— 才是 *kernel* 数量的大头，而且它们在 93 个 layer 的每一层上都要跑。

案例研究中的 Q8_0 投影 GEMV（矩阵-向量乘）被描述为*“每一个非专家 K3 投影背后的 kernel，如今也是 decode 的主要开销。”* 这就是应当预期到的形态：在显而易见的内存收益（量化权重）之后，开销转移到围绕那些大矩阵乘的众多小矩阵-向量乘上。

关于该 kernel 的两个结构性事实决定了下面的一切：

```text
   Q8_0 weights + f32 activation:
       the activation must be QUANTIZED before an int8 dot product.
       → somebody has to encode it. once? or once per consumer?

   K3 has FOUR consumers reading the SAME activation on every KDA layer:
       attn_q, attn_k, attn_v, ssm_g   all read s.normed at [qkv, H]
       → four launches over one vector, and four re-quantizations of it.
```

§2–§6 中的所有内容都是那两行代码的结果。

---

## 2. 一个 kernel 中的五个缺陷

[**PR #25**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/25) —— *“Q8_0 投影路径 —— 在 ctx 128k 下 3.54 → 9.94 tok/s，bit-identical。”* 每个缺陷都值得单独研究；下面的顺序大致按微妙程度递增。

### 2.1 一个 34 字节的 struct 让向量化加载失效

```text
   BlockQ8_0 is 34 bytes, alignment 2.
   → qs[] starts at 34·b + 2
   → 4-byte aligned only for ODD b.  never uniformly.

   nvcc cannot widen a scalar qs[j] loop, so it emits
   ONE ld.global.s8 PER ELEMENT.  32 loads where 8 would do.
```

修复方案是 `get_int_b2()`，它用两次**始终对齐的 `uint16` 加载**重建同样的 4 个字节。而说明这就是正确修复的线索是：*“ggml 的 CUDA 路径出于同样的原因也带着同一个辅助函数。”*

> **直接抄走。** 块大小不是 4 的倍数的量化块格式，会静默地让每一个碰它的 kernel 中的每一次向量化加载失效。你无法修改磁盘上的格式，所以修复办法是写一个辅助函数，用你实际拥有的对齐来重建宽字。在 profile 任何东西之前，先检查你的 block struct 的大小和对齐。

### 2.2 量化了激活值，却不用整数运算

量化激活值的路径用 **32 条标量 IMAD** 指令来计算它的 int8 点积 —— *“把量化激活值的意义整个丢掉了。”*

`__dp4a` 用一条指令完成 4 元素的 int8 点积累加。把激活值编码成 int8 的全部理由，就是为了用上它。一条先量化、然后做标量整数乘加的流水线，付了量化的代价，却一点收益都没拿到。

> **检查你的快速路径是否真的用上了它为之存在的那条指令。** 量化只是通往 `dp4a` / IMMA / tensor-core 路径的手段。如果 profile 在量化步骤之后显示标量 IMAD，那这次量化就是纯粹的开销。


<details>
<summary>English original</summary>

**Part 4 · Lecture 05 — Fusion and the Activation-Quantization Discipline**

**Overview**

The single largest step in the case study's history was **3.54 → 9.94 tok/s** — a 2.77× improvement, bit-identical, from **five independent defects in one kernel**. Not a new algorithm. Five ways the same GEMV was leaving the machine on the table.

This lecture is about that kernel and its neighbours: the quantized projection path that every non-expert matrix multiply in the model goes through, and the discipline of **activation quantization** — deciding once, correctly, where a vector gets encoded to int8 and who is allowed to read it.

It is also where a specific and repeated observation lands. Three separate PRs found a kernel achieving **about 7 GB/s on a 4.8 TB/s part** and concluded, correctly:

> *That is not work, it is launch overhead.*

By the end you should be able to audit a quantized GEMV for the five defects in §2, decide whether an activation should be hoisted or fused, and — most importantly — architect a shared-buffer optimization so that a mistake makes it *slow* rather than *wrong*.

---

**1. Where decode's time actually goes**

In a sparse MoE at batch 1, the expert weights are the bulk of the *bytes*. But the **projections** — Q/K/V/gate, the MLA down/up projections, the router, the shared experts, the LM head — are the bulk of the *kernels*, and they run on every one of 93 layers.

The case study's Q8_0 projection GEMV is described as *"the kernel behind every non-expert K3 projection, and now the dominant decode cost."* That is the shape to expect: after the obvious memory win (quantizing weights), the cost migrates to the many small matrix-vector products that surround the big ones.

Two structural facts about that kernel drive everything below:

```text
   Q8_0 weights + f32 activation:
       the activation must be QUANTIZED before an int8 dot product.
       → somebody has to encode it. once? or once per consumer?

   K3 has FOUR consumers reading the SAME activation on every KDA layer:
       attn_q, attn_k, attn_v, ssm_g   all read s.normed at [qkv, H]
       → four launches over one vector, and four re-quantizations of it.
```

Everything in §2–§6 is a consequence of those two lines.

---

**2. Five defects in one kernel**

[**PR #25**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/25) — *"Q8_0 projection path — 3.54 → 9.94 tok/s at ctx 128k, bit-identical."* Each defect is worth studying separately; the ordering below is roughly increasing subtlety.

**2.1 A 34-byte struct defeats vectorized loads**

```text
   BlockQ8_0 is 34 bytes, alignment 2.
   → qs[] starts at 34·b + 2
   → 4-byte aligned only for ODD b.  never uniformly.

   nvcc cannot widen a scalar qs[j] loop, so it emits
   ONE ld.global.s8 PER ELEMENT.  32 loads where 8 would do.
```

The fix is `get_int_b2()`, which rebuilds the same 4 bytes from two **always-aligned `uint16` loads**. And the tell that this is the right fix: *"ggml's CUDA path carries the same helper for the same reason."*

> **Steal this.** A quantization block format with a size that is not a multiple of 4 will silently defeat every vectorized load in every kernel that touches it. You cannot change the on-disk format, so the fix is a helper that reconstructs wide words from the alignment you actually have. Check your block struct's size and alignment before you profile anything.

**2.2 Quantizing the activation and then not using integer arithmetic**

The quantized-activation path was computing its int8 dot product with **32 scalar IMAD** instructions — *"giving up the point of having quantised the activation."*

`__dp4a` performs a 4-element int8 dot-product-accumulate in one instruction. The entire reason to encode activations to int8 is to reach it. A pipeline that quantizes and then does scalar integer multiply-adds has paid the quantization cost and collected none of the benefit.

> **Check that your fast path reaches the instruction it exists for.** Quantization is a means to `dp4a` / IMMA / tensor-core paths. If the profile shows scalar IMAD after a quantize step, the quantization is pure overhead.

</details>

### 2.3 激活值比权重还大

反直觉的那一个，也是 multi-row 存在的原因：

```text
   projection with N = 12288 output rows, K = 7168:

   weight traffic       ~94 MB
   ACTIVATION traffic   ~344 MB     ← re-read once per output row

   the activation is 3.7x the weights.
```

在 batch 1、每个输出行一个 block 时，每个 block 都加载整个激活值。修法是把 **4 行放进一个 block**，让每次激活值加载在这 4 行之间复用——激活值流量削减约 4×。

而让它保持诚实的约束是：只在 **N ≥ 1024** 时分派，因为 multi-row 同时也会*切分 grid*。在 `ssm_beta` 的 N=96 下，每 block 四行会把 grid 降到 **24 blocks**——直接掉进 [Lecture 04](Lecture-04.md)的饥饿。两个方向相反的优化，靠一个阈值来解决，而不是选边站。

> **复用与 occupancy 相互权衡。** 任何“每 block 处理 R 行”的改动都会把 grid 除以 R。它在大 N 下是收益，在小 N 下是回退，所以它需要一个分派条件——而不是一个全局开关。

### 2.4 固定的 block 宽度面对可变的工作量

```text
   BLOCK was 128 threads regardless of available work.
   the loop strides  b < blocks_per_row:

     ssm_f_b   (K=128)  →  124 of 128 threads idle — 96.9% —
                           on all 69 KDA layers
     attn_q_b           →  62.5% idle, on all 24 MLA layers
```

这是 [Lecture 04](Lecture-04.md)的 occupancy 问题出现在 block 内部而不是跨越整个 grid，PR 指出它*正是 PR #7 在 MoE 分派中修掉的同一个缺陷，在这里依然存在，并且因同样的原因存活下来*：

> *空闲线程贡献的是精确的零，所以输出是对的，错的只有 occupancy。*

这句话就是这类 bug 为何长期存在的全部机理。没有任何东西是错的。对以零为主的贡献做归约，得到的是正确答案。没有测试会失败，没有断言会被触发，没有数值漂移——唯一的症状是：你为一个 128 线程的 block 付了钱，却只用了四个线程。

### 2.5 一次激活值上的四次 launch

`attn_q`、`attn_k`、`attn_v` 和 `ssm_g` 在每个 KDA layer 上都以相同 shape 读取同一个 normed hidden state。把它们融合意味着**一次激活值加载喂给 8 个点积**，并去掉**每个 token 207 次 launch**。

这个安全性论证值得记一笔，因为它是*数据流*论证，不是约定：`ssm_g` 可以被提升（hoist）进这一组，理由是*“安全的，因为 `s.normed` 在 `attn_norm` 处写一次，之后在该 block 中再也不被触碰。”*生产者与消费者位于直线代码中，中间没有任何东西。§4 讲的是当这一点不成立时会发生什么。

### 2.6 收益在真正重要的地方站得住

| 上下文 | 前 | 后 | 加速比 |
|---|--:|--:|--:|
| 128 | 3.54 | 8.89 | 2.48× |
| 4k | 3.13 | 8.03 | 2.25× |
| 32k | 3.57 | 9.77 | 2.72× |
| **128k** | **3.54** | **9.94** | **2.77×** |

> *“被计分的那一行是最强的一行，这正是重点：这不是一个在模型真正被使用的场景下就会蒸发掉的短上下文收益。”*

扫遍上下文、并展示收益随深度*增长*，正是区分一个优化与一个 benchmark 产物的东西。对比 [Lecture 03 §5](Lecture-03.md)：一个在 4k 有用、在 128k 消失的改动，发现的是关于你的测试的某件事，而不是你的 engine。

---

## 3. 一个“免费”操作却带着 launch 账单

接下来是在三份 PR、三组数字里反复出现的发现。

[**PR #67**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67)：

```text
   quantize_q8_0:  59,696 launches,  8.7% of GPU time
                   4.9 µs each to move ~36 KB
                   ≈ 7 GB/s   on a 4.8 TB/s part

   "That is not work, it is launch overhead."
```

[**PR #81**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81)，在 ctx 131072 下 profile：

```text
   quantize_q8_0:  61,848 launches,  7.9% of GPU kernel time
                   4.19 µs each to move ~28 KB
                   ≈ 6.7 GB/s  on a 4.8 TB/s part
```

两次独立测量一致表明，全部 GPU 时间中约 8% 花在了一个以**峰值带宽的 0.15%** 运行的 kernel 上。量化一个 28 KB 的向量确实是微不足道的工作——而这正是它不可见的原因。没人会去 profile 那个便宜的操作。

> **诊断方法：用搬动的字节数除以耗时，再与峰值比较。** 一个在极小负载上只达到峰值带宽极小比例的 kernel，做的不是访存工作——它是*在被 launch*。它的成本正比于你调用它的次数，而修法是少调用它几次，不是让它更快。

这是 [Lecture 03](Lecture-03.md)的 launch-bound 诊断被局部化到单个 kernel 上，而且它可以推广：**任何按消费者而非按值调用的小型 helper kernel，都是一个披着带宽外衣的 launch 次数 bug。**


<details>
<summary>English original</summary>

**2.3 The activation is bigger than the weights**

The counterintuitive one, and the reason multi-row exists:

```text
   projection with N = 12288 output rows, K = 7168:

   weight traffic       ~94 MB
   ACTIVATION traffic   ~344 MB     ← re-read once per output row

   the activation is 3.7x the weights.
```

At batch 1 with one block per output row, each block loads the whole activation. The fix puts **4 rows in a block** and reuses each activation load across them — cutting activation traffic ~4×.

And the constraint that keeps it honest: dispatched only for **N ≥ 1024**, because multi-row also *divides the grid*. At `ssm_beta`'s N=96, four rows per block would drop the grid to **24 blocks** — straight into [Lecture 04](Lecture-04.md)'s starvation. Two optimizations pulling in opposite directions, resolved by a threshold rather than by picking a side.

> **Reuse and occupancy trade against each other.** Any "process R rows per block" change divides your grid by R. It is a win at large N and a regression at small N, so it needs a dispatch condition — not a global flag.

**2.4 A fixed block width against variable work**

```text
   BLOCK was 128 threads regardless of available work.
   the loop strides  b < blocks_per_row:

     ssm_f_b   (K=128)  →  124 of 128 threads idle — 96.9% —
                           on all 69 KDA layers
     attn_q_b           →  62.5% idle, on all 24 MLA layers
```

This is [Lecture 04](Lecture-04.md)'s occupancy problem inside a block rather than across a grid, and the PR notes it is *the same defect PR #7 fixed in the MoE dispatch, still present here, surviving for the same reason*:

> *Idle threads contribute exact zeros, so output is correct and only occupancy is wrong.*

That sentence is the whole mechanism of why this class of bug persists. Nothing is wrong. A reduction over mostly-zero contributions gives the right answer. There is no test that fails, no assertion to trip, no numerical drift — the only symptom is that you paid for a 128-thread block and used four threads.

**2.5 Four launches over one activation**

`attn_q`, `attn_k`, `attn_v` and `ssm_g` read the same normed hidden state at the same shape on every KDA layer. Fusing them means **one activation load feeds 8 dot products**, and removes **207 launches per token**.

The safety argument is worth noting because it is a *data-flow* argument, not a convention: `ssm_g` can be hoisted to join the group *"safe because `s.normed` is written once at `attn_norm` and never touched again in the block."* The producer and the consumers are in straight-line code with nothing between them. §4 is about what happens when that is not true.

**2.6 The win holds where it matters**

| context | before | after | speedup |
|---|--:|--:|--:|
| 128 | 3.54 | 8.89 | 2.48× |
| 4k | 3.13 | 8.03 | 2.25× |
| 32k | 3.57 | 9.77 | 2.72× |
| **128k** | **3.54** | **9.94** | **2.77×** |

> *"The scored row is the strongest one, which is the point: this is not a short-context win that evaporates where the model is actually used."*

Sweeping context and showing the win *grows* with depth is what distinguishes an optimization from a benchmark artifact. Compare [Lecture 03 §5](Lecture-03.md): a change that helps at 4k and vanishes at 128k has found something about your test, not your engine.

---

**3. A "free" operation with a launch bill**

Now the recurring finding, in three PRs, with three sets of numbers.

[**PR #67**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67):

```text
   quantize_q8_0:  59,696 launches,  8.7% of GPU time
                   4.9 µs each to move ~36 KB
                   ≈ 7 GB/s   on a 4.8 TB/s part

   "That is not work, it is launch overhead."
```

[**PR #81**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81), profiled at ctx 131072:

```text
   quantize_q8_0:  61,848 launches,  7.9% of GPU kernel time
                   4.19 µs each to move ~28 KB
                   ≈ 6.7 GB/s  on a 4.8 TB/s part
```

Two independent measurements agreeing that ~8% of all GPU time went to a kernel running at **0.15% of peak bandwidth.** Quantizing a 28 KB vector is genuinely trivial work — which is exactly why it was invisible. Nobody profiles the cheap op.

> **The diagnostic: divide bytes moved by time taken, and compare to peak.** A kernel achieving a tiny fraction of peak bandwidth on a tiny payload is not doing memory work — it is *being launched*. Its cost is proportional to how many times you call it, and the fix is to call it fewer times, not to make it faster.

This is [Lecture 03](Lecture-03.md)'s launch-bound diagnosis localized to a single kernel, and it generalizes: **any small helper kernel invoked per-consumer rather than per-value is a launch-count bug wearing a bandwidth costume.**

</details>

### 3.1 修复方案，及其规模

[#81](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81) 对每个*激活值*只量化一次，并把缓冲区交给各个消费者：

```text
   61,848 launches (7.9%)   →   40,104 launches (4.5%)
                                −302 per token per rank

   50.01 → 47.71 ms/token at 128k
   20.00 → 20.96 tok/s        +4.8%,  bit-identical
```

[#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67) 从另一个方向攻击同一浪费 —— *融合*四个消费者，使其只剩一个：

```text
   57.33 → 55.13 ms/token at 128k
   17.45 → 18.14 tok/s        +4.0%,  bit-identical
```

两者都正确，且可以组合。**Hoisting** 消除冗余的*生产者*调用；**融合**消除冗余的*消费者*启动。§4 解释为什么其中之一在架构上更安全。

---

## 4. 传递缓冲区，不要缓存它

这是本讲最重要的一节，因为它讲的是如何做出一个*不可能*悄悄出错的优化。

在本仓库中，此前曾尝试以**缓存**的形式在消费者之间共享量化后的激活值：

```text
   THE FAILED VERSION
     cache the quantized activation inside the projection,
     keyed on x, guarded by "was the scratch last written from
     this pointer?"

   RESULT SHIPPED:   top1 0.0     mean_kld 0.937
                     — every token wrong, while running faster.

   THE BUG:  the guard tracked which POINTER the scratch came from,
             never whether the BYTES behind it still matched.
```

指针同一性守卫不是有效性守卫。同一个缓冲区地址在前向传播的不同位置可以存放不同内容，而且通常确实如此。并且注意来自 [Lecture 10](Lecture-10.md) 的签名：**错误，而且更快。**

成功的版本改变的是*架构*，而不是守卫。三个性质，每一个都堵住一个具体的失败：

| 性质 | 它防止什么 |
|---|---|
| **缓冲区是一个参数。** `k3_quantize_act_f32` 写它，`k3_proj_q8act_f32` 读它，二者之间的窗口是同一个流上的直线代码。 | *"不存在可供后续变更悄悄违反的不变量。"* |
| **`act_q8` 与 `proj_q8` 是相互独立的分配。** 每个未 hoist 的投影仍量化到 `proj_q8` 中。 | 共享同一个缓冲区会让交织的调用覆盖 hoisted 的字节 —— *"正是那个击垮缓存版本的别名。"* |
| **`proj_h` 回退**到完整路径，只要 hoist 未生效。 | **"漏掉的 hoist 只会变慢，绝不会出错。"** |

还有一条，防止两条路径随时间分叉：`k3_proj_ggml_f32` 被重新实现为 `k3_quantize_act_f32` + `k3_proj_q8act_f32`，因此 *"只有一份实现，hoisted 路径与逐调用路径无法渐行渐远。"*

[#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67) 就融合提出了相同的观点，并表述得最好：

> *融合不需要任何人的承诺。激活值不可能在生产者与消费者之间过期，**因为它们就是同一次启动。** 正确性不再依赖于关于周边代码的断言，而这正是复用版本不安全、无法保留的原因。*

> **设计准则。** 当两段代码必须就共享内存的内容达成一致时，按此顺序优先：(1) **让它们成为同一次启动**，这样就不需要一致；(2) **显式传递缓冲区**，让一致体现在签名中；(3) *绝不* **带守卫地缓存**，因为守卫是不变量的代理，而代理会漂移。要这样设计：优化的失效是一次*变慢*，而不是一个错误答案。

最后一句值得强调。"漏掉的 hoist 只会变慢，绝不会出错"是一个需要你*工程化*出来的性质：把快速路径做成对正确路径的可选精化，而不是对它的替换。这与 [Lecture 04](Lecture-04.md) 的条件 `float4` 加载回退到精确标量路径是同一形状。

---


<details>
<summary>English original</summary>

**3.1 The fix, and its size**

[#81](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81) quantizes once per *activation* and hands the buffer to each consumer:

```text
   61,848 launches (7.9%)   →   40,104 launches (4.5%)
                                −302 per token per rank

   50.01 → 47.71 ms/token at 128k
   20.00 → 20.96 tok/s        +4.8%,  bit-identical
```

[#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67) attacks the same waste from the other direction — *fusing* the four consumers so there is only one of them:

```text
   57.33 → 55.13 ms/token at 128k
   17.45 → 18.14 tok/s        +4.0%,  bit-identical
```

Both are correct and they compose. **Hoisting** removes redundant *producer* calls; **fusion** removes redundant *consumer* launches. §4 explains why one of them is architecturally safer.

---

**4. Pass the buffer; do not cache it**

This is the most important section in the lecture, because it is about how to make an optimization that *cannot* be silently wrong.

Sharing a quantized activation between consumers had been attempted in this repository before, as a **cache**:

```text
   THE FAILED VERSION
     cache the quantized activation inside the projection,
     keyed on x, guarded by "was the scratch last written from
     this pointer?"

   RESULT SHIPPED:   top1 0.0     mean_kld 0.937
                     — every token wrong, while running faster.

   THE BUG:  the guard tracked which POINTER the scratch came from,
             never whether the BYTES behind it still matched.
```

A pointer-identity guard is not a validity guard. The same buffer address can hold different contents at different points in a forward pass, and it usually does. And note the signature from [Lecture 10](Lecture-10.md): **wrong, and faster.**

The successful version changes the *architecture*, not the guard. Three properties, and each one closes a specific failure:

| Property | What it prevents |
|---|---|
| **The buffer is a parameter.** `k3_quantize_act_f32` writes it, `k3_proj_q8act_f32` reads it, and the window between them is straight-line code on one stream. | *"There is no invariant for a later change to violate silently."* |
| **`act_q8` is a separate allocation from `proj_q8`.** Every un-hoisted projection still quantizes into `proj_q8`. | Sharing one buffer would let an interleaved call overwrite the hoisted bytes — *"the exact aliasing that broke the cached version."* |
| **`proj_h` falls back** to the full path whenever the hoist did not apply. | **"A missed hoist is slow, never wrong."** |

And one more, which prevents the two paths from diverging over time: `k3_proj_ggml_f32` is reimplemented as `k3_quantize_act_f32` + `k3_proj_q8act_f32`, so *"there is one implementation and the hoisted and per-call paths cannot drift apart."*

[#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67) makes the same point about fusion, and states it best:

> *Fusing needs no promise from anyone. The activation cannot go stale between its producer and its consumer **because they are the same launch.** Correctness stops depending on a claim about the surrounding code, which is what made the reuse version unsafe to keep.*

> **The design rule.** When two pieces of code must agree about the contents of shared memory, prefer — in this order: (1) **make them the same launch** so no agreement is needed; (2) **pass the buffer explicitly** so the agreement is visible in the signature; (3) *never* **cache with a guard**, because the guard is a proxy for the invariant and proxies drift. Design so that a failure of the optimization is a *slowdown*, not a wrong answer.

That last clause deserves emphasis. "A missed hoist is slow, never wrong" is a property you *engineer*, by making the fast path an opt-in refinement of a correct path rather than a replacement for it. It is the same shape as [Lecture 04](Lecture-04.md)'s conditional `float4` load falling back to an exact scalar path.

---

</details>

## 5. 把预测差距当作调试工具

[#81](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81) 的第二个 commit 是整个案例研究中最精彩的方法论，值得花点时间体会。

该改动的第一个版本实测为 **+1.8%**，而*预测*的节省是每 token 每 rank 462 次 launch。实测：**−233**。

多数工程师会收下这 +1.8% 然后继续往前走。而实际做法是：

> *这个差距是在指认 bug，而不是噪声。*

推理如下：

```text
   proj_f32_kernel runs 11,592 times
                 = 161 per token per rank
                 = 69 × 2  +  24 × 1        ← EXACTLY

   ⇒ ssm_f_a and ssm_beta are f32-WEIGHTED and were never
     quantized at all. So on all 69 KDA layers the hoist
     ADDED a launch and removed none — while
     k3_proj_ggml_f32_x4 re-quantized the same bytes independently.

   FIX: give the fused group an x_pre_q8 parameter.
        removes exactly 4,968 = 69 × 8 × 9 further launches.

   +1.8%  →  +4.1%
```


这里用到两种技巧，都很廉价，也都没被充分利用：

**预测一个可计数的量，而不只是一个加速比。**“这里应该能去掉每 token 每 rank 462 次 launch”是可*证伪*的，而“这应该会更快”不是。当计数落到 233 时，就有一个具体的残差——229——需要解释。

**把残差对模型的结构做因式分解。**每 token 每 rank 的 161 恰好分解为 `69 × 2 + 24 × 1` 并非巧合；69 和 24 分别是 KDA 和 MLA 的层数。这一因式分解*就是*诊断结果：每条 KDA layer 两次调用、每条 MLA layer 一次，来自一条从未被量化的路径。

> **照抄这一手。**给计数器加埋点，而不只是给时钟加埋点。然后把计数器的结果对模型的算术做校验——层数、head 数、专家数、rank 数。一个无法干净地分解进你的架构维度的 launch 计数，说明存在一条你没有考虑到的路径。正是这一技巧，把“比我希望的快了一点”从一句耸肩变成一份 bug report。

---

## 6. 融合回归藏在布尔守卫背后

[#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67) 以一个小的恐怖故事开场：

> *在 [#63](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) 中启用 Q8 激活值，**静默地禁用**了每条 KDA layer 上的四路投影融合，因为守卫条件读作 `!ggml_qact_proj && ...`*

一个 feature flag 被打开。别处的 dispatch 守卫读作*“只有在不使用量化激活值时才做融合。”*于是启用新路径就禁用了全部 69 条 KDA layer 上的旧优化——没有报错、没有告警，只是静默地退回四次独立投影，以及对同一个向量做四次重新量化。

这是 [Lecture 04 §5](Lecture-04.md) 的模式换了一副面貌。那里，对一个轴做分片会让由它导出的每个 grid 都缩小。这里，启用一条路径会禁用所有守卫将其排除在外的优化。同一形状：

> **dispatch 条件里的每个 `if (!new_feature)` 都是一个会被你的新特性关掉的融合或快路径。**在启用任何东西之后，都要 `grep` 其 flag 的取反。而在你写这样一条守卫时，留一条注释说明 Q8 对应的写法需要是什么——因为总会有人去启用它。

补救办法是对应的实现：`k3_proj_ggml_f32_x4` 的 Q8 激活值孪生版本，再加上重新启用融合。**+4.0%**，这实际上是在*找回* #63 悄悄花掉的 4.0%。

以及那句诚实的范围声明，它可以防止过度宣称：

> *只有 KDA 的 `q/k/v/g` 这一组符合条件：`k3_proj_ggml_f32_x4` 要求四个相同的 shape。`ssm_f_a`、`ssm_beta` 和 `ssm_f_b` 读取同一个激活值，但处在不同的 `N`，而 MLA 和 MoE 投影保留各自的量化——所以这去掉的是 59,696 次 launch 中的大约**五分之一**，而不是全部。*

融合要求 shape 一致。一个激活值的四个消费者若处在*不同*的输出宽度上，没有更通用的 grouped kernel 就无法共享一次 launch。知道你的修复只覆盖了 20% 的实例，正是这一点告诉你桌上还有更多可拿。

---


<details>
<summary>English original</summary>

**5. The prediction gap as a debugging instrument**

[#81](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81)'s second commit is the finest piece of methodology in the entire case study, and it takes a moment to appreciate.

The first version of the change measured **+1.8%**, against a *predicted* saving of 462 launches per token per rank. Measured: **−233**.

Most engineers would bank the +1.8% and move on. Instead:

> *That gap named the bug rather than being noise.*

The reasoning:

```text
   proj_f32_kernel runs 11,592 times
                 = 161 per token per rank
                 = 69 × 2  +  24 × 1        ← EXACTLY

   ⇒ ssm_f_a and ssm_beta are f32-WEIGHTED and were never
     quantized at all. So on all 69 KDA layers the hoist
     ADDED a launch and removed none — while
     k3_proj_ggml_f32_x4 re-quantized the same bytes independently.

   FIX: give the fused group an x_pre_q8 parameter.
        removes exactly 4,968 = 69 × 8 × 9 further launches.

   +1.8%  →  +4.1%
```

Two techniques here, both cheap and both underused:

**Predict a countable quantity, not just a speedup.** "This should remove 462 launches per token per rank" is *falsifiable* in a way "this should be faster" is not. When the count came in at 233, there was a specific residual — 229 — to explain.

**Factor the residual against the model's structure.** 161 per token per rank decomposing as exactly `69 × 2 + 24 × 1` is not a coincidence; 69 and 24 are the KDA and MLA layer counts. That factorization *is* the diagnosis: two calls per KDA layer, one per MLA layer, from a path that was never quantized.

> **Steal this.** Instrument a counter, not just a clock. Then check the counter against the arithmetic of your model — layers, heads, experts, ranks. A launch count that does not factor cleanly into your architecture's dimensions is telling you about a path you have not accounted for. This is the technique that converts "it's a bit faster than I hoped" from a shrug into a bug report.

---

**6. Fusion regressions hide behind boolean guards**

[#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67) opens with a small horror story:

> *Enabling Q8 activations in [#63](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) **silently disabled** the four-way projection fusion on every KDA layer, because the guard read `!ggml_qact_proj && ...`*

A feature flag was turned on. A dispatch guard elsewhere read *"only fuse if we are NOT using quantized activations."* So enabling the new path disabled the old optimization on all 69 KDA layers — no error, no warning, just a silent return to four separate projections and four re-quantizations of the same vector.

This is the [Lecture 04 §5](Lecture-04.md) pattern in a different guise. There, sharding an axis shrank every grid derived from it. Here, enabling a path disabled every optimization whose guard excluded it. Same shape:

> **Every `if (!new_feature)` in a dispatch condition is a fusion or fast path that your new feature turns off.** After enabling anything, `grep` for negations of its flag. And when you write such a guard, leave a note saying what the Q8 counterpart would need to be — because someone will enable it.

The recovery is the counterpart implementation: `k3_proj_ggml_f32_x4`'s Q8-activation twin, plus re-enabling the fusion. **+4.0%**, which is really *recovering* 4.0% that #63 had quietly spent.

And the honest scope statement, which prevents over-claiming:

> *Only the KDA `q/k/v/g` group qualifies: `k3_proj_ggml_f32_x4` requires four identical shapes. `ssm_f_a`, `ssm_beta` and `ssm_f_b` read the same activation but at different `N`, and the MLA and MoE projections keep their own quantisations — so this removes about **a fifth** of the 59,696 launches, not all of them.*

Fusion requires shape agreement. Four consumers of one activation at *different* output widths cannot share a launch without a more general grouped kernel. Knowing that your fix addresses 20% of the instances is what tells you there is more on the table.

---

</details>

## 7. 串行前导

[**PR #107**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107) 为 §2 的清单增添了第四类，而它是任何 occupancy 指标都显示不出来的：

> *四个 decode kernel 把前导阶段耗在**单个线程遍历数组、且带有相互依赖的全局 load** 上，而 block 的其余部分则在 barrier 处等待。*

grid 没问题。block 没问题。实测 occupancy 看起来也没问题。而在前导阶段持续的整段时间里，一个线程发出一条相互依赖的全局 load 链——每一次都要等前一次算完地址——与此同时 1023 个线程停在 `__syncthreads()` 处。

```text
   thread 0:   load → use → load → use → load → ...   (latency-chained)
   threads 1..1023:   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ waiting at barrier ▓▓▓▓▓▓▓▓▓▓
```

依赖 load 链无法靠更多 warp 来隐藏，因为只有一个 warp 在干活。把前导并行化——让整个 block 协作遍历该数组——就把一条延迟链转化成了带宽问题，而带宽正是硬件擅长的。

该 PR 把这一点与 [Lecture 04 §4](Lecture-04.md) 中已经熟悉的东西配在一起：*"重新调校两个被 **#86 的 F16 latent cache 作废掉的** MLA 常量。"* 先前某个 PR 把 latent cache 的元素大小减半，从而改变了那两个调校常量所依据的算术。又一个被缓存的推导过期了，只是来自另一个方向。

这个 PR 里有一处出入，值得点明而不是粉饰过去：**标题声称在 128k 下 +15.9%，而正文测得的却是 +4.5%**——43.30 → 45.25 tok/s，四次交错 load 在 `main`/branch 之间交替，两个 `main` pass 与两个 branch pass 的结果吻合到 0.05%。bot 自己那一轮测得 41.39 tok/s。这里引用正文的数字，因为它才是附带了方法的那个；同时标出这个差距，因为标题与测量结果不一致恰恰是 [Lecture 02](Lecture-02.md) 说要留意的这类事情。准确率同时*提升*了：平均 KLD **0.00751 → 0.00669**。

同一个 PR 里还有两处收益，各值得写一行，因为二者都是 §6 那个 "dead fast path" 模式换了副面孔。**LM head**——一个 163,840 × 7,168 的投影，在 Q8_0 下是模型里最大的一次单权重读取，达 1.25 GB，约是最大的单层投影的 45 倍——原本**只在 rank 0 上**跑，而其余七块 H200 持有完全相同的 hidden state，却无事可做。把它按 band 划分只是一个指针偏移和一个更小的 `N`，因为每个 rank 本来就已经*持有*这份权重。而 all-reduce 原本在**一次就够的地方付了两次跨 GPU rendezvous**：在 14–42 KB 的载荷下，reduce 在 kernel 所用的时间里只搬动 NVLink 约 3%，所以*"kernel 的开销几乎全在那两次 rendezvous 上，而每个 token 有 185 次。"* 第二个 barrier 的存在只是为了阻止一个快的 rank 在慢的对端仍在读取时覆写共享输入——若改为轮转输入缓冲区而不是做同步，这个 hazard 就消失了（[Lecture 07 §6](Lecture-07.md))。

---


<details>
<summary>English original</summary>

**7. The serial prologue**

[**PR #107**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107) adds a fourth family to §2's list, and it is one that no occupancy metric shows:

> *Four decode kernels spent their prologue in **a single thread walking an array with dependent global loads** while the rest of the block waited at a barrier.*

The grid is fine. The block is fine. Achieved occupancy looks fine. And for the duration of the prologue, one thread issues a chain of dependent global loads — each one waiting on the previous to compute its address — while 1023 threads sit at `__syncthreads()`.

```text
   thread 0:   load → use → load → use → load → ...   (latency-chained)
   threads 1..1023:   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ waiting at barrier ▓▓▓▓▓▓▓▓▓▓
```

A dependent-load chain cannot be hidden by more warps, because there is only one warp doing anything. Parallelizing the prologue — having the whole block cooperatively walk the array — converts a latency chain into a bandwidth problem, which the hardware is good at.

The PR pairs this with something already familiar from [Lecture 04 §4](Lecture-04.md): *"retuning two MLA constants that **#86's F16 latent cache invalidated**."* A previous PR halved the latent cache's element size, which changed the arithmetic that two tuned constants were derived from. Another cached derivation gone stale, from a different direction.

There is a discrepancy in this PR worth naming rather than papering over: **its title claims +15.9% at 128k while its body measures +4.5%** — 43.30 → 45.25 tok/s, four interleaved loads alternating `main`/branch, with the two `main` passes and the two branch passes agreeing to 0.05%. The bot's own round measured 41.39 tok/s. I quote the body's number here because it is the one with a method attached, and flag the gap because a title and a measurement disagreeing is exactly the kind of thing [Lecture 02](Lecture-02.md) says to notice. Accuracy *improved* alongside it: mean KLD **0.00751 → 0.00669**.

Two further wins in the same PR are worth a line each, because both are the "dead fast path" pattern from §6 in a new guise. The **LM head** — a 163,840 × 7,168 projection, at Q8_0 the largest single weight read in the model at 1.25 GB, roughly 45× the biggest per-layer projection — was running **on rank 0 alone** while the other seven H200s held the identical hidden state and nothing to do with it. Banding it was a pointer offset and a smaller `N`, because every rank already *held* the weight. And the all-reduce was paying **two cross-GPU rendezvous where one suffices**: at 14–42 KB payloads the reduce moves ~3% of NVLink in the time the kernel takes, so *"what the kernel costs is almost entirely the two rendezvous, and there are 185 of them per token."* The second barrier existed only to stop a fast rank overwriting the shared input while a slow peer still read it — a hazard that disappears if you rotate input buffers instead of synchronizing ([Lecture 07 §6](Lecture-07.md)).

---

</details>

## 8. 证明一次量化改动是 bit-identical

对量化 GEMV 的改动，恰恰是最可能出现 bit 变化的场合，因此 #25 的验证门槛很高。尤其要注意，其中大部分工作是在节点运行**之前**完成的。

**在 host 上，在消耗节点机时之前：**

```text
   14.8M checks on the packed-load and __dp4a rewrites
   160k rows at EVERY blocks_per_row K3 produces,
        including the 32 / 64 / 65 dispatch boundaries
   6 real projection shapes for multi-row, with a ragged tail
   the fused path vs four separate single-row projections
   — with the warp-shuffle reduction tree MODELLED EXACTLY,
     "since a reduction reorder is where a change like this
      would actually move bits"
```

要照抄的有三件事。**在 dispatch 边界上测试** —— 32/64/65 正是 `blocks_per_row` 分支改变行为的地方，off-by-one bug 就藏在那里。**测试参差的尾部**，因为一次处理 4 行的 multi-row kernel 会遇到不是 4 的倍数的 N。**精确建模归约顺序**，因为在 bit-identity 断言中，归约树是 bit 唯一可能移动的地方。

**在节点上，以字节形式：**

```text
   cmp main.spkl proj.spkl        →  identical
   md5  d4f468195f15e941600690838cf808c7   (both)
   argmax id 1379, logit 13.732111         (both)
   top-1 1.0   mean KLD 0.004045515251179501   (identical, all digits)
   ctest 21/21 on 8x H200
```

以及它之所以 bit-identical 的*原因*，以不变量的形式而非观察结论的形式陈述：

> *每个输出行保持相同的 thread-to-block 跨步方式、相同的 `i` 顺序、相同的每 `i` 四次加法，以及相同的 `block_sum`。**改变的只有 load 宽度、launch 形状，以及哪些 dot product 共享同一个 CUDA block。**"

这才是论证 bit-identity 的正确方式：列举哪些东西被保持不变（累加顺序、归约结构、操作数顺序），以及哪些东西允许改变（字节如何取回、工作如何分组）。如果你的改动触及了第一份清单中的任何一项，它就不是 bit-identical，你就应当说明是什么发生了重新结合（[Lecture 04 §8](Lecture-04.md)）。

还要注意 KLD 检查的锐度：*与 main 完全一致，所有数位*，`0.004045515251179501`。一个在 18 位有效数字上一致的指标，其说服力远强于只在 3 位上一致的一个——而且这是免费的。

---

## 9. 把自己降档的那位贡献者

还有一点来自 [#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67)，其小标题好到难以改进：**“harness 把这个报告为 `L`。它是 `S`，而这个差值不该由我保留。”**

harness 返回了 `pct_over_frontier 13.1, label "L"`，因为 `reference.lock` 中保存的 `frontier_tps 16.06` 是在 [#59](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/59) 合并之前测得的。而 #59 已经在 `main` 中，自身就值约 8.3%。所以报告的 13.1% 是 #59 的增益加上这个 PR 的增益。

该贡献者自己测了 `main`（62.27 → 57.33 ms/token），发现了这个差异，并主张把自己的档位从 `L` *下调* 到 `S`。

这是从贡献者一侧看到的 [Lecture 02](Lecture-02.md) 的前沿陈旧问题，它也提出了一个关于激励设计、值得直说的要点：**这种行为之所以可能，是因为 harness 同时公布前沿和原始测量值，所以贡献者可以自己核算算术。** 一个只输出标签的评分系统，无法被最有条件发现它出错的人纠正。

同一个 PR 还展示了来自 [Lecture 04 §6](Lecture-04.md) 的纪律 —— 通过 `SPARKINFER_K3_FUSE_QKVG=0` 做单二进制 A/B，两套测量工具的结果都报告出来并解释其分歧，还打印每个 rep 的数字。它还报告了 register pressure，作为 fusion 没有牺牲 occupancy 的证据：

```text
   proj_q8_0_q8_0_fused4_kernel<128,4>   REG:46  STACK:0  LOCAL:0  SHARED:20
   "no spill, and half the f32 sibling's register count"
```

> **做 fusion 时，要报告 register pressure。** fusion 的经典失效模式是：把四个 kernel 的活跃状态合并在一起会 spill 到 local memory，而 spill 了的融合 kernel 很容易比四个未融合的 kernel 更慢。`REG` 和 `STACK:0` 就是这没有发生的证据。

---


<details>
<summary>English original</summary>

**8. Proving a quantization change is bit-identical**

A change to a quantized GEMV is exactly where you would expect bits to move, so the verification bar in #25 is high. Note especially that most of it happened **before** the node run.

**On the host, before spending node hours:**

```text
   14.8M checks on the packed-load and __dp4a rewrites
   160k rows at EVERY blocks_per_row K3 produces,
        including the 32 / 64 / 65 dispatch boundaries
   6 real projection shapes for multi-row, with a ragged tail
   the fused path vs four separate single-row projections
   — with the warp-shuffle reduction tree MODELLED EXACTLY,
     "since a reduction reorder is where a change like this
      would actually move bits"
```

Three things to copy. **Test at the dispatch boundaries** — 32/64/65 is where a `blocks_per_row` branch changes behaviour, and off-by-one bugs live there. **Test the ragged tail**, because a multi-row kernel processing 4 rows at a time meets an N that is not a multiple of 4. **Model the reduction order exactly**, because in a bit-identity claim the reduction tree is the only place the bits can move.

**On the node, as bytes:**

```text
   cmp main.spkl proj.spkl        →  identical
   md5  d4f468195f15e941600690838cf808c7   (both)
   argmax id 1379, logit 13.732111         (both)
   top-1 1.0   mean KLD 0.004045515251179501   (identical, all digits)
   ctest 21/21 on 8x H200
```

And the *reason* it is bit-identical, stated as an invariant rather than an observation:

> *Every output row keeps the same thread-to-block striding, the same `i` order, the same four adds per `i`, and the same `block_sum`. **Only load width, launch shape, and which dot products share a CUDA block change.**"

That is the correct way to justify a bit-identity claim: enumerate what is preserved (accumulation order, reduction structure, operand order) and what is allowed to change (how bytes are fetched, how work is grouped). If your change touches anything in the first list, it is not bit-identical, and you should say what reassociates ([Lecture 04 §8](Lecture-04.md)).

Note also the sharpness of the KLD check: *identical to main, all digits*, `0.004045515251179501`. A metric that agrees to 18 significant figures is a much stronger statement than one that agrees to 3 — and it is free.

---

**9. The contributor who downgraded their own tier**

One more thing from [#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67), under a heading that is hard to improve on: **"The harness reports this as `L`. It is `S`, and the difference is not mine to keep."**

The harness returned `pct_over_frontier 13.1, label "L"`, because `reference.lock` held `frontier_tps 16.06` measured before [#59](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/59) merged. #59 was already in `main` and worth ~8.3% on its own. So the reported 13.1% was #59's gain plus this PR's.

The contributor measured `main` themselves (62.27 → 57.33 ms/token), found the discrepancy, and argued their own tier *down* from `L` to `S`.

This is [Lecture 02](Lecture-02.md)'s stale-frontier problem viewed from the contributor's side, and it makes a point about incentive design worth stating plainly: **the reason this behaviour is possible is that the harness publishes both the frontier and the raw measurements, so a contributor can check the arithmetic.** A scoring system that emits only a label cannot be corrected by the person best placed to notice it is wrong.

The same PR also demonstrates the discipline from [Lecture 04 §6](Lecture-04.md) — single-binary A/B via `SPARKINFER_K3_FUSE_QKVG=0`, both instruments reported with their disagreement explained, and per-rep numbers printed. And it reports register pressure as evidence the fusion did not cost occupancy:

```text
   proj_q8_0_q8_0_fused4_kernel<128,4>   REG:46  STACK:0  LOCAL:0  SHARED:20
   "no spill, and half the f32 sibling's register count"
```

> **When you fuse, report register pressure.** Fusion's classic failure mode is that combining four kernels' live state spills to local memory, and a spilled fused kernel can easily be slower than four unfused ones. `REG` and `STACK:0` are the evidence that it did not happen.

---

</details>

## 实验 — 审计你的量化投影路径

1. **检查你的 block 结构体。** 量化 block 的大小与对齐。它是 4 的倍数吗？如果不是，找到或编写 aligned-reconstruction 辅助函数，并从 SASS 或 PTX 确认你的 load 已加宽（§2.1）。
2. **确认你确实走到了整数指令。** 反汇编你量化的内层循环。你看到的是 `DP4A` / `IMMA`，还是标量的 `IMAD`？如果是后者，那么你的量化当前纯属开销（§2.2）。
3. **计算激活值与权重的流量**，针对你最宽的三个投影，在 batch 1、每 block 一行输出下。如果激活值流量占主导，就原型验证 multi-row —— 并找出那个 N，低于它时网格划分的损失大于复用带来的收益（§2.3）。
4. **统计每个「廉价」kernel 的启动次数。** 对每一个，计算 bytes ÷ time 并与峰值带宽比较。在小 payload 下低于峰值约 1% 的任何情况，都是 launch 次数 bug（§3）。
5. **找出你重复的编码。** 哪些值在每个 token 中被量化、归一化或变换了不止一次？对每一个，决定：融合消费者，还是把生产者上提并传递 buffer。为你的选择给出理由（§4）。
6. **先预测一个计数器，再测量它。** 在实现之前，预测每个 token 的 launch 次数变化。然后测量它。**如果差距超过约 10%，在接受这个加速比之前，先把残差对你的 layer/head/expert 数量做因式分解**（§5）。
7. **对你的 feature flag 的取反做 `grep`。** dispatch 路径中的每一个 `if (!feature)`，都是你的 feature 关掉的一项优化（§6）。
8. **按「慢，但绝不出错」来设计架构。** 对于你的 shared-buffer 改动，写下 fast path 被跳过时会发生什么。如果答案不是「它更慢」，就重新设计（§4）。
9. **先在主机上验证。** 在占用硬件之前，对 dispatch 边界和 ragged tail 做穷尽检查，并建模归约顺序（§8）。

通过标准：一处量化投影改动，以 single-binary A/B 测量，带有经过校验的 launch 次数预测、由 `cmp` 支撑的 bit-identity 声明，以及一份书面论证：漏掉 fast path 不会产生错误答案。

---

## 自检

1. 你的量化 block 是 34 字节，对齐为 2。精确解释为什么编译器会为每个元素发出一条字节 load，以及修复方案从什么重建出什么。
2. 一个 kernel 在 4.19 µs 内搬运 28 KB，运行 61,848 次，合计占 GPU 时间的 7.9%。以 4.8 TB/s 的器件为基准，计算它达到的带宽占比。诊断是什么，什么*不是*修复方案？
3. 你每个 block 处理 4 行输出，以复用激活值 load。给出收益、代价和 dispatch 条件 —— 并对 N=96、132-SM GPU 上的一个投影给出算术。
4. 一个 kernel 启动 128 个线程，其中只有 4 个有工作。为什么没有测试失败，又为什么它能挺过一次代码评审？
5. 一位前任工程师以指针为键缓存了量化后的激活值，其保护条件是「scratch 最后一次写入是否来自这个指针？」它带着 `top1 0.0` 以更快的 ms/token 上线了。解释这个 bug，并给出两种能让它不可能发生的架构。
6. 你预测每个 rank 每个 token 减少 462 次 launch，实测为减少 233 次。残差分解为 `69 × 2 + 24 × 1`。这告诉了你什么，你又是怎么知道要尝试这种因式分解的？
7. 启用一条新的量化路径，静默地关掉了 69 个 layer 上的融合。写出你在启用任何 feature flag 之后要运行的 `grep`，以及写这类 guard 时留下的注释。
8. 你把四个投影融合进一个 kernel，结果它*更慢*了。说出你首先检查的东西，以及你要报告的两个数字。
9. 你的 bit-identity 声明覆盖 load 宽度、launch 形状和 block 分组。列出三件你一定没有改动过的事，并指出哪一件最有可能改变了 bit。

---


<details>
<summary>English original</summary>

**Lab — audit your quantized projection path**

1. **Check your block struct.** Size and alignment of your quantization block. Is it a multiple of 4? If not, find or write the aligned-reconstruction helper and confirm from the SASS or PTX that your loads widened (§2.1).
2. **Confirm you reach the integer instruction.** Disassemble your quantized inner loop. Do you see `DP4A` / `IMMA`, or scalar `IMAD`? If the latter, your quantization is currently pure cost (§2.2).
3. **Compute activation vs weight traffic** for your three widest projections, at batch 1, one output row per block. If activation traffic dominates, prototype multi-row — and find the N below which the grid division hurts more than the reuse helps (§2.3).
4. **Count launches of every "cheap" kernel.** For each, compute bytes ÷ time and compare to peak bandwidth. Anything under ~1% of peak on a small payload is a launch-count bug (§3).
5. **Find your repeated encodings.** Which values are quantized, normalized, or transformed more than once per token? For each, decide: fuse the consumers, or hoist the producer and pass the buffer. Justify which (§4).
6. **Predict a counter, then measure it.** Before implementing, predict the change in launch count per token. Measure it. **If the gap is more than ~10%, factor the residual against your layer/head/expert counts before you accept the speedup** (§5).
7. **`grep` for negations of your feature flags.** Every `if (!feature)` in a dispatch path is an optimization your feature disables (§6).
8. **Architect for "slow, never wrong."** For your shared-buffer change, write down what happens if the fast path is skipped. If the answer is anything other than "it is slower," redesign (§4).
9. **Verify on the host first.** Exhaustive checks at dispatch boundaries and ragged tails, with the reduction order modelled, before you book hardware (§8).

Pass criterion: one quantized projection change, measured single-binary A/B, with a launch-count prediction that was checked, a bit-identity claim backed by `cmp`, and a written argument that a missed fast path cannot produce a wrong answer.

---

**Self-check**

1. Your quantization block is 34 bytes with alignment 2. Explain precisely why the compiler emits one byte-load per element, and what the fix reconstructs from what.
2. A kernel moves 28 KB in 4.19 µs and runs 61,848 times, totalling 7.9% of GPU time. Compute its achieved bandwidth as a fraction of a 4.8 TB/s part. What is the diagnosis, and what is *not* the fix?
3. You process 4 output rows per block to reuse the activation load. Give the benefit, the cost, and the dispatch condition — with the arithmetic for a projection at N=96 on a 132-SM GPU.
4. A kernel runs 128 threads where only 4 have work. Why does no test fail, and why has this survived a code review?
5. A previous engineer cached a quantized activation keyed on its pointer, guarded by "was the scratch last written from this pointer?" It shipped `top1 0.0` at a faster ms/token. Explain the bug and give two architectures that make it impossible.
6. You predict −462 launches per token per rank and measure −233. The residual factors as `69 × 2 + 24 × 1`. What does that tell you, and how did you know to try that factorization?
7. Enabling a new quantized path silently disabled a fusion on 69 layers. Write the `grep` you run after enabling any feature flag, and the comment you leave when writing such a guard.
8. You fuse four projections into one kernel and it is *slower*. Name the first thing you check and the two numbers you report.
9. Your bit-identity claim covers load width, launch shape, and block grouping. List three things you must *not* have changed, and say which one is most likely to have moved the bits.

---

</details>

## References

* **The PRs** — [#25](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/25)（五缺陷 Q8_0 投影路径）、[#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67)（融合四个 KDA 投影；失效的缓存；自我降级的 tier）、[#81](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81)（只量化一次；预测差距调试）、[#107](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107)（串行 prologue）。它们的正文是一手来源。
* **`ggml` / llama.cpp CUDA 量化路径** — [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)、`ggml/src/ggml-cuda/` — `get_int_b2` 风格的对齐重建，以及 §2.1 和 §2.2 所参照的 `Q8_0` MMVQ kernel。是整个系列的参考实现。
* **`__dp4a`** — [CUDA C++ Programming Guide § Integer Intrinsics](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)；4 路 int8 点积累加，自 sm_61 起可用。
* **GGUF / K-quant 与 I-quant 格式** — [llama.cpp GGUF spec](https://github.com/ggml-org/llama.cpp/blob/master/docs/gguf.md) — 其块大小造就 §2.1 的那些 block layout。
* **W8A8 量化** — SmoothQuant [arXiv:2211.10438](https://arxiv.org/abs/2211.10438) — 激活值究竟为何要被量化，以及决定量化位置的离群值问题。
* **寄存器压力与 occupancy** — [CUDA C++ Best Practices Guide § Register Pressure](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)；`-Xptxas -v` 给出 §9 报告的 `REG`/`STACK` 数值。

交叉引用：

* [第 1 部分 第 04 讲 — 精度栈](../01-基础/Lecture-04.md) — INT8/W8A8 在 FP16/FP8/FP4 之间所处的位置。
* [第 2 部分 第 03 讲 — 量化 70B 级模型](../02-Hopper稠密计算/Lecture-03.md) — 权重侧量化；本讲是*激活值*侧。
* [第 04 讲 — 启动几何](Lecture-04.md) — §2.3 的复用/occupancy 取舍与 §6 的标志位取反模式的另一种形式。
* [第 10 讲 — 静默出错](Lecture-10.md) — §4 中失效的缓存是其分类法中的一个典型条目。

---

## Current as of 2026-08

8× H200 SXM、`sm_90`、CUDA 12.8+、UD-IQ1_S、tp=8、计分上下文 131,072。数值来自 PR #25 / #67 / #81 / #107；凡有说明处均为单二进制的环境变量门控 A/B。按 GGUF Q8_0 布局，`BlockQ8_0` = 34 bytes / align 2。五缺陷审计、启动账单诊断、pass 而非缓存规则，以及预测差距技术属于可长期保留的内容。

---

## Next

* 下一讲：[第 06 讲 — 128k 下的 attention：按上下文切分、按 head 切分](Lecture-06.md)
* 上一讲：[第 04 讲 — 启动几何：grid、occupancy 与每 token 327 个 norm](Lecture-04.md)
* 上级：[第 4 部分 — 优化一个真实引擎](README.md)


<details>
<summary>English original</summary>

**References**

* **The PRs** — [#25](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/25) (the five-defect Q8_0 projection path), [#67](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/67) (fuse the four KDA projections; the failed cache; the self-downgraded tier), [#81](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81) (quantize once; the prediction-gap debugging), [#107](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107) (the serial prologue). Their bodies are the primary source.
* **`ggml` / llama.cpp CUDA quantized paths** — [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp), `ggml/src/ggml-cuda/` — `get_int_b2`-style aligned reconstruction and the `Q8_0` MMVQ kernels that §2.1 and §2.2 mirror. The reference implementation for this whole family.
* **`__dp4a`** — [CUDA C++ Programming Guide § Integer Intrinsics](https://docs.nvidia.com/cuda/cuda-c-programming-guide/); 4-way int8 dot-product-accumulate, available since sm_61.
* **GGUF / K-quant and I-quant formats** — [llama.cpp GGUF spec](https://github.com/ggml-org/llama.cpp/blob/master/docs/gguf.md) — the block layouts whose sizes create §2.1.
* **W8A8 quantization** — SmoothQuant [arXiv:2211.10438](https://arxiv.org/abs/2211.10438) — why activations get quantized at all, and the outlier problem that decides where.
* **Register pressure and occupancy** — [CUDA C++ Best Practices Guide § Register Pressure](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/); `-Xptxas -v` for the `REG`/`STACK` numbers §9 reports.

Cross-references:

* [Part 1 Lecture 04 — The precision stack](../01-基础/Lecture-04.md) — where INT8/W8A8 sits among FP16/FP8/FP4.
* [Part 2 Lecture 03 — Quantizing 70B-class models](../02-Hopper稠密计算/Lecture-03.md) — the weight-quantization side; this lecture is the *activation* side.
* [Lecture 04 — Launch geometry](Lecture-04.md) — §2.3's reuse/occupancy tradeoff and §6's flag-negation pattern in their other form.
* [Lecture 10 — Silently wrong](Lecture-10.md) — the failed cache in §4 is a canonical entry in its taxonomy.

---

**Current as of 2026-08**

8× H200 SXM, `sm_90`, CUDA 12.8+, UD-IQ1_S, tp=8, scored context 131,072. Numbers from PRs #25 / #67 / #81 / #107; all single-binary env-gated A/B where stated. `BlockQ8_0` = 34 bytes / align 2 per the GGUF Q8_0 layout. The five-defect audit, the launch-bill diagnostic, the pass-don't-cache rule, and the prediction-gap technique are the durable content.

---

**Next**

* Next: [Lecture 06 — Attention at 128k: split over context, split over heads](Lecture-06.md)
* Previous: [Lecture 04 — Launch geometry: grids, occupancy, and 327 norms per token](Lecture-04.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
