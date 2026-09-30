---
title: Part 4 · Lecture 03 — 诊断：受 launch 限制、受带宽限制，还是受通信限制？
description: Part 4 · Lecture 03 — 诊断：受 launch 限制、受带宽限制，还是受通信限制？
---

# Part 4 · Lecture 03 — 诊断：受 launch 限制、受带宽限制，还是受通信限制？

## 概述

你已经有了一块记分板（[Lecture 02](Lecture-02.md)），也有了一个很难看的数字（[Lecture 01](Lecture-01.md)）。下一个问题会决定你接下来的一整个月：**哪个上界在起约束作用？**

多 GPU decode（逐 token 生成阶段）循环中有四个候选者，而它们需要的工作完全不同：

```text
   BANDWIDTH   you are reading weights as fast as HBM allows.
               → reduce bytes per token. quantize, compress the cache, read less.

   OCCUPANCY   the kernels are running, but on a fraction of the machine.
               → change grid geometry, split work, widen blocks.

   LAUNCH      the GPU is idle between kernels more than it is busy.
               → fuse, capture graphs, make the step device-resident.

   COMM        the collectives dominate the step.
               → change the algorithm, the payload, or where the reduce sits.
```

靠猜的代价是几周时间。案例研究自己的诊断结论是：集合通信——在 93 层 8 GPU 模型中*看起来*最贵的那个东西——只占 **token 的约 2%**，真正的开销是每层约 30 个未融合 kernel 的 launch 开销。把这个搞反，就意味着要花一个月去优化 NCCL。

读完之后，你应当能够拿到一份 decode profile，说出起约束作用的上界，为每个候选者附上一个数字，再加上那个能证伪你结论的*唯一*一项测量。

---

## 1. 从你无法突破的那个上界开始

带宽上界是这四者中唯一属于定律而非症状的一个，所以先讲它。来自 [Part 1 Lecture 03](../01-基础/Lecture-03.md)：

```text
   tokens/s  ≤  aggregate HBM bandwidth  ÷  bytes read per token
```

对于 batch 1 的 MoE（混合专家模型），「每 token 读取的字节数」有三部分，而常见的错误是只算第一部分：

```text
   bytes/token  =  ACTIVE expert weights          (top-k of N, not all N)
                +  every replicated/dense weight   (attention, norms, LM head,
                                                    shared experts, router)
                +  the attention state you touch   (KV cache at depth,
                                                    or recurrent state)
```

按案例研究的 shape 演算一遍，以展示方法：

```text
   weights          553 GiB for 2.8T params   →  ~0.21 bytes/param  (~1.7 bits)
   active params    ~50B  (top-16 of 896 routed + 2 shared + dense)
   active bytes     50e9 × 0.21             ≈  10.6 GB per token
   8× H200 HBM3e    ~4.8 TB/s per card       →  ~38 TB/s aggregate

   naive ceiling    38e12 / 10.6e9          ≈  3600 tok/s
```

这个数字作为目标毫无用处，作为诊断却极其有用。引擎开始时测得 **3.55 tok/s**，结束时测得 **60.17 tok/s**。比带宽上界低三个数量级，意味着**带宽不是约束，再怎么量化也没用**。有别的东西在白白糟蹋这台机器。

> **roofline（性能上界模型）计算最有价值的产出往往是「不是这个」。** 你低了 1000 倍的那个上界不算上界；它证明你看错了资源。

那个朴素上界在 batch 1 下不可达，原因有三，值得了解，免得你去追它：

* **聚合带宽假设 8 张卡在同时读取有用的字节。** 在专家分片下，每个 rank 上约 2 个活跃专家只是一次很小的读取；这个 rank 读完就等。
* **batch 1 没有复用。** 每个权重字节读出来只服务一个 token。这是 GEMV（矩阵-向量乘）regime —— 算术强度 ≈ 1 —— 此时你无法把一次读取摊销到一个分块的工作上。
* **一连串小 kernel 永远达不到峰值带宽**，因为每个 kernel 都要花开头几微秒爬升、结尾几微秒排空。

仓库自己对它实际所处位置的总结是：**「profile 受 launch/occupancy 限制，距离 roofline 差 13 倍。」** 四个候选者中的两个被并列点名，因为它们很难分开 —— 见 §4。

---


<details>
<summary>English original</summary>

**Part 4 · Lecture 03 — Diagnosis: Launch-Bound, Bandwidth-Bound, or Comm-Bound?**

**Overview**

You have a scoreboard ([Lecture 02](Lecture-02.md)) and a number that is bad ([Lecture 01](Lecture-01.md)). The next question decides your entire month: **which ceiling is binding?**

There are four candidates in a multi-GPU decode loop, and they demand completely different work:

```text
   BANDWIDTH   you are reading weights as fast as HBM allows.
               → reduce bytes per token. quantize, compress the cache, read less.

   OCCUPANCY   the kernels are running, but on a fraction of the machine.
               → change grid geometry, split work, widen blocks.

   LAUNCH      the GPU is idle between kernels more than it is busy.
               → fuse, capture graphs, make the step device-resident.

   COMM        the collectives dominate the step.
               → change the algorithm, the payload, or where the reduce sits.
```

Guessing costs weeks. The case study's own diagnosis was that the collective — the thing that *looks* most expensive in a 93-layer 8-GPU model — accounted for about **2% of the token**, and the real cost was launch overhead in ~30 unfused kernels per layer. Getting that backwards would have meant optimizing NCCL for a month.

By the end you should be able to take a decode profile and name the binding ceiling with a number attached to each candidate, plus the *one* measurement that would falsify your conclusion.

---

**1. Start with the ceiling you cannot beat**

The bandwidth ceiling is the only one of the four that is a law rather than a symptom, so it goes first. From [Part 1 Lecture 03](../01-基础/Lecture-03.md):

```text
   tokens/s  ≤  aggregate HBM bandwidth  ÷  bytes read per token
```

For an MoE at batch 1, "bytes read per token" has three parts, and the mistake is counting only the first:

```text
   bytes/token  =  ACTIVE expert weights          (top-k of N, not all N)
                +  every replicated/dense weight   (attention, norms, LM head,
                                                    shared experts, router)
                +  the attention state you touch   (KV cache at depth,
                                                    or recurrent state)
```

Worked for the case study's shape, to show the method:

```text
   weights          553 GiB for 2.8T params   →  ~0.21 bytes/param  (~1.7 bits)
   active params    ~50B  (top-16 of 896 routed + 2 shared + dense)
   active bytes     50e9 × 0.21             ≈  10.6 GB per token
   8× H200 HBM3e    ~4.8 TB/s per card       →  ~38 TB/s aggregate

   naive ceiling    38e12 / 10.6e9          ≈  3600 tok/s
```

That number is useless as a target and extremely useful as a diagnosis. The engine measured **3.55 tok/s** at the start and **60.17 tok/s** at the end. Three orders of magnitude below a bandwidth ceiling means **bandwidth is not the constraint and no amount of quantization will help.** Something else is throwing the machine away.

> **The most valuable output of a roofline calculation is often "not this one."** A ceiling you are 1000× below is not a ceiling; it is proof you are looking at the wrong resource.

Three reasons that naive ceiling is unreachable at batch 1, worth knowing so you do not chase it:

* **Aggregate bandwidth assumes all 8 cards are reading useful bytes simultaneously.** With expert sharding, the ~2 active experts per rank are a small read; the rank finishes and waits.
* **Batch 1 has no reuse.** Every weight byte is read to serve one token. This is the GEMV regime — arithmetic intensity ≈ 1 — where you cannot amortize a read across a tile of work.
* **A stream of small kernels never reaches peak bandwidth**, because each one spends its opening microseconds ramping and its closing microseconds draining.

The repo's own summary of where it actually sat: **"the profile is launch/occupancy-bound at 13× off roofline."** Two of the four candidates, named together, because they are hard to separate — §4.

---

</details>

## 2. 算清集合通信的代价，因为所有人都假设它就是问题所在

一个 93 层模型跑在 8 张 GPU 上，*听起来*就是通信受限的。先测量，再相信。本案例研究用的这套算术，就是一个可复用的模板：

```text
   MEASURED, ShardPolicy::ExpertsOnly, K3, tp_size 8:

     1 collective per MoE layer × 92 MoE layers   =  92 all-reduces per token
     expert_latent 3584 × 4 bytes (f32)           =  14 KiB per collective
     58.7 µs per call, 8× H200, NCCL              =  ~5.4 ms/token of collective

     token time at the time of measurement        =  281.6 ms  (3.55 tok/s)
     collective share                             ≈  2%
```

结论：**不是瓶颈。**该 repo 的结论是——*“每层约 30 个未融合 kernel 的启动开销才是。”*

这个计算里有四处很容易算错，每一处都是一课。

**集合通信的次数要从代码里数，而不是看图。**文档最初写的数字是每 token **186** 次——93 层每层两次，这是 [Part 2 Lecture 04](../02-Hopper稠密计算/Lecture-04.md) 里的标准张量并行数值。真实数字是 **92**，因为这种分片策略*复制了 attention*（所以没有 attention 的规约），而且最前面的 dense 层没有 expert dispatch。通信估算差了 2 倍，只因照搬教科书上的数字，而没去看前向传播。

**把载荷宽度算对。**规约宽度是 **3584**，不是 7168——routed experts 位于降投影后的 latent 空间。比按 `hidden_size` 预测的字节数少一半。

**搞清 dtype，也要搞清为什么。**不管你按 `3584 × 4 B`（f32）还是按 `7168 × 2 B`（bf16）算，14 KiB 碰巧都一样——*数字相同，原因不同*，而这类巧合恰恰最容易掩盖错误。K3 刻意让 residual stream 跑 f32，如果把它送进 bf16 的 all-reduce，每个 layer 边界都会被截断到约 8 位尾数，从而破坏执行器的数值行为。

**在这种规模下你是延迟受限的，所以要报 µs/call，而不是 GB/s。**实测的伸缩性就是证据：**数据量放大 512 倍，耗时只增加 1.45 倍**（14 KiB → 7 MiB）。在 14 KiB 上谈 GB/s，说明不了硬件的任何事，只能说明你的固定开销。验证工具之所以按每次调用报微秒数，就是这个原因。

> **直接抄。**对任何小于约 1 MB 的集合通信，有用的单位是每次调用的延迟，有用的优化目标是*调用次数和 barrier 机制*——而不是带宽。

---

## 3. 估算启动开销的账单

这是大多数人从未量化过的天花板，而在本案例研究中，它两次成为答案。

数一数：**每层约 30 次未融合 kernel 启动 × 93 层 ≈ 每 token 2790 次启动**，按 rank 计——而 driver 从单个 host 线程向全部 8 个 rank 下发。

现在拿它和项目历史上两个时点的 token 预算对比：

```text
   at 3.55 tok/s   →  281.6 ms/token  ÷  2790  =  ~101 µs per kernel
                      launch overhead (~3–5 µs) is ~4% of that.
                      the KERNELS are slow. fix kernels.

   at 60.17 tok/s  →   16.6 ms/token  ÷  2790  =  ~6 µs per kernel
                      launch overhead is now most of the budget.
                      the LAUNCHES are the work. fuse, or capture a graph.
```

这张表就是本讲中最重要的一个观点：

> **约束天花板会随优化而移动。**诊断是有保质期的。在 3.55 tok/s 时的正确答案（“kernel 太慢”）在 60 tok/s 时就是错误答案，而一个只诊断一次、然后执行六周的团队，会在后半程对着错误的天花板使劲。

这就是为什么 [Lecture 08](Lecture-08.md)——CUDA graphs 与 device-resident decode（逐 token 生成阶段）——放在这一部分的靠后位置，而不是靠前。当每个 kernel 要跑 101 µs 时，graph capture 毫无收益。等到每个 kernel 只跑几微秒时，它带来了 **+22.7%**。同样的改动、同样的代码，价值不同，完全取决于引擎其余部分推进到了哪一步。

### 3.1 如何低成本地区分启动受限与 kernel 慢

在伸手去拿 profiler 之前：

| 信号 | 读数 |
|---|---|
| kernel 时长之和 ≪ 墙钟 step 时间 | **间隙。**启动/同步受限。 |
| kernel 时长之和 ≈ 墙钟 step 时间 | kernel 就是成本。去看 occupancy 和带宽。 |
| 缩小某个 kernel 的工作量，step 时间几乎不变 | 该 kernel 由启动或延迟主导，而非计算主导。 |
| step 时间随 layer 的*数量*增长，而非每层的工作量 | 每层的固定成本——启动、同步或集合通信。 |
| 去掉一个 `cudaDeviceSynchronize()` 会改变这个数字 | 你测的是 stall，而不是吞吐。 |

用 Nsight Systems 的话说：**时间线上 kernel 之间的空白就是测量结果。**工程师会本能地去看最宽的那根条形；在一个启动受限的 decode 循环里，答案在条形之间的空隙里。

---


<details>
<summary>English original</summary>

**2. Price the collective, because everyone assumes it is the problem**

A 93-layer model on 8 GPUs *sounds* comm-bound. Measure it before believing it. The case study's arithmetic, which is a template you can reuse:

```text
   MEASURED, ShardPolicy::ExpertsOnly, K3, tp_size 8:

     1 collective per MoE layer × 92 MoE layers   =  92 all-reduces per token
     expert_latent 3584 × 4 bytes (f32)           =  14 KiB per collective
     58.7 µs per call, 8× H200, NCCL              =  ~5.4 ms/token of collective

     token time at the time of measurement        =  281.6 ms  (3.55 tok/s)
     collective share                             ≈  2%
```

Verdict: **not the bottleneck.** The repo's conclusion — *"Launch overhead in the ~30 unfused kernels per layer is."*

Four things in that calculation are easy to get wrong, and each is a lesson.

**Count the collectives from the code, not the diagram.** The doc originally quoted **186** per token — two per layer across 93 layers, the standard tensor-parallel figure from [Part 2 Lecture 04](../02-Hopper稠密计算/Lecture-04.md). The real number is **92**, because this shard policy *replicates attention* (so there is no attention reduce) and the leading dense layer has no expert dispatch. A 2× error in your comm estimate, from reading a textbook figure instead of the forward pass.

**Get the payload width right.** The reduce is **3584** wide, not 7168 — the routed experts live in a down-projected latent space. Half the bytes you would predict from `hidden_size`.

**Know your dtype and why.** 14 KiB happens to be the same whether you compute `3584 × 4 B` (f32) or `7168 × 2 B` (bf16) — *the same number for a different reason*, which is exactly the kind of coincidence that hides an error. K3 runs an f32 residual stream deliberately, and routing it through a bf16 all-reduce would truncate to ~8 mantissa bits at every layer boundary, undoing the executor's numerics.

**At these sizes you are latency-bound, so report µs/call, not GB/s.** The measured scaling is the proof: **512× the data costs 1.45× the time** (14 KiB → 7 MiB). A GB/s figure at 14 KiB tells you nothing about the hardware and everything about your fixed overhead. The validation tool reports microseconds per call for this reason.

> **Steal this.** For any collective under ~1 MB, the useful unit is latency per call, and the useful optimization target is *the number of calls and the barrier mechanism* — not bandwidth.

---

**3. Estimate the launch bill**

This is the ceiling most people never quantify, and in this case study it was the answer twice.

The count: **~30 unfused kernel launches per layer × 93 layers ≈ 2790 launches per token**, per rank — and the driver issues all 8 ranks from a single host thread.

Now compare against the token budget at two points in the project's history:

```text
   at 3.55 tok/s   →  281.6 ms/token  ÷  2790  =  ~101 µs per kernel
                      launch overhead (~3–5 µs) is ~4% of that.
                      the KERNELS are slow. fix kernels.

   at 60.17 tok/s  →   16.6 ms/token  ÷  2790  =  ~6 µs per kernel
                      launch overhead is now most of the budget.
                      the LAUNCHES are the work. fuse, or capture a graph.
```

That table is the single most important idea in this lecture:

> **The binding ceiling moves as you optimize.** A diagnosis has a shelf life. The correct answer at 3.55 tok/s ("the kernels are slow") is the wrong answer at 60 tok/s, and a team that diagnoses once and executes for six weeks will spend the back half of those weeks on the wrong ceiling.

This is why [Lecture 08](Lecture-08.md) — CUDA graphs and device-resident decode — is late in this part rather than early. Graph capture buys nothing when each kernel runs for 101 µs. It bought **+22.7%** once each kernel ran for a few microseconds. Same change, same code, different value, entirely because of where the rest of the engine had got to.

**3.1 How to tell launch-bound from slow-kernel, cheaply**

Before reaching for a profiler:

| Signal | Reading |
|---|---|
| Sum of kernel durations ≪ wall-clock step time | **Gaps.** Launch/sync-bound. |
| Sum of kernel durations ≈ wall-clock step time | Kernels are the cost. Look at occupancy and bandwidth. |
| Step time barely changes when you shrink a kernel's work | That kernel is launch- or latency-dominated, not compute-dominated. |
| Step time scales with the *number* of layers, not the work per layer | Per-layer fixed cost — launches, syncs, or collectives. |
| A `cudaDeviceSynchronize()` removal changes the number | You were measuring stalls, not throughput. |

In Nsight Systems terms: **the whitespace between kernels on the timeline is the measurement.** Engineers instinctively look at the widest bar; in a launch-bound decode loop the answer is the gaps between the bars.

---

</details>

## 4. Occupancy —— 隐藏于“kernel 慢”之内的天花板

kernel 慢，可能是因为它做的事很多，也可能是因为它在 GPU 的一小片上只做了一点点事。后一种在 decode（逐 token 生成阶段）中要常见得多，而且在你发现它时，也难堪得多。

这个算术很简单，却几乎没人去做：

```text
   H200 (sm_90):  132 SMs

   grid of  12 blocks  →   9% of the SMs have work.  91% idle.
   grid of   1 block   →  0.8%.
   grid of 132+ blocks →  every SM has something, and the tail matters
```

案例研究中的两个真实发现，二者的价值都超出其体量所暗示的程度：

* 一个 norm kernel，**每个 token 调用 327 次、跑在 128 个 thread 上** —— 单个 block。占整机 0.8%，每个 token 来 327 次。（[#115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115)，[Lecture 04](Lecture-04.md)）
* KDA decode 步骤**在 132 个 SM 上只跑 12 个 block** —— 9% occupancy，通过 value 分块拓宽 grid 得以修复。（[#77](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77)）

二者都不是算法上的洞察。二者都是 *shape* 问题：工作量本来就在那里，只是 grid 没有把它暴露出来。而且关键在于，**在 naive profile 下二者看起来都像带宽问题** —— 实测带宽低、FLOPs 低、kernel 耗时超出应有水平。能区分它们的测量量是 grid 尺寸与 SM 数量之比，而这是任何吞吐指标都显示不出来的。

### 4.1 为什么 decode 在结构上容易出这个问题

批大小为 1 的 decode 所产生的张量，其 **sequence 维度为 1**。训练 kernel 所依赖的每一条并行轴 —— 批、sequence、分块 —— 都已塌缩。剩下的只有 heads、hidden dimension 和 experts。如果 kernel 按 heads 并行，而模型有 96 个，你就得到 96 个 block，在一张 132 个 SM 的卡上，GPU 永远填不满。

所以 [Lectures 04](Lecture-04.md) 和 [06](Lecture-06.md) 中反复出现的修复方法是**再找一条可切的轴**：按 context 切（Flash-Decoding 的思路）、按 value 分块切、按 expert group × FFN band 切。每一种都是同一个动作 —— *在工作负载天然的轴用尽之处，人为制造并行度。*

---

## 5. 诊断随 context 深度而变

一个 profile 不够，因为配比会随序列长度变化。案例研究中最清晰的一组数据：

| 8× H200、UD-IQ1_S、相同权重 | ctx 64 | ctx 131,072 | 变化 |
|---|--:|--:|--:|
| llama.cpp | 18.32 | 18.44 | ~持平 |
| SparkInfer-K3，最初测得 | 10.34 | **1.00** | **−90%** |

同一个引擎、同一份权重、同一台机器。一个数字比参考实现落后 1.8×；另一个落后 18×。**它们是对不同瓶颈的诊断。**

在 ctx 64 时 KV cache 几乎为空，因此每 token 的权重读取占主导，引擎看起来只是没优化好。在 131,072 时，沿深度方向的 attention 归约占主导 —— 参考实现之所以能保持持平，是因为它保存的是*压缩过的* MLA cache（`kv_lora` 512, f16），而候选实现要在**每个 token 的 576 个 f32 值上做归约，且其 kernel 每个 head 只有一个 block。**

一句话里有三个各自独立的缺陷，而只有在长深度下才看得到：cache 未压缩（带宽）、它是 f32（带宽）、kernel 每个 head 只有一个 block（occupancy）。

> **在你实际部署的深度上做 profile。** 曲线持平是一种主张，而不是默认状态。如果你的候选实现随 context 变长而退化、而参考实现没有，那么这个差距*就是*你的诊断 —— 而且它在短 context 下不可见。

对你自己的 harness（agent 运行时框架）而言，推论是：扫遍 context，把两个引擎都画出来。分歧的*形状*会告诉你深度消耗的是哪种资源。[Lecture 06](Lecture-06.md) 就是修复这个问题之后的样子。

---


<details>
<summary>English original</summary>

**4. Occupancy — the ceiling hidden inside "the kernel is slow"**

A kernel can be slow because it is doing a lot, or because it is doing a little on a sliver of the GPU. The second is far more common in decode, and far more embarrassing when you find it.

The arithmetic is elementary and almost never done:

```text
   H200 (sm_90):  132 SMs

   grid of  12 blocks  →   9% of the SMs have work.  91% idle.
   grid of   1 block   →  0.8%.
   grid of 132+ blocks →  every SM has something, and the tail matters
```

Two of the case study's real findings, both worth more than their size suggests:

* A norm kernel with **327 invocations per token running on 128 threads** — a single block. 0.8% of the machine, 327 times a token. ([#115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115), [Lecture 04](Lecture-04.md))
* The KDA decode step running **12 blocks on 132 SMs** — 9% occupancy, fixed by value-tiling to widen the grid. ([#77](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77))

Neither is an algorithmic insight. Both are *shape* problems: the work was there, the grid did not expose it. And critically, **both look like bandwidth problems in a naive profile** — low achieved bandwidth, low FLOPs, kernel taking longer than it should. The distinguishing measurement is grid dimensions against SM count, which no throughput metric shows you.

**4.1 Why decode is structurally prone to this**

Batch-1 decode produces tensors with a **sequence dimension of 1**. Every parallelization axis a training kernel relies on — batch, sequence, tile — has collapsed. What is left is heads, hidden dimension, and experts. If a kernel parallelizes over heads and the model has 96, you get 96 blocks and a permanently underfilled GPU on a 132-SM card.

So the recurring fix in [Lectures 04](Lecture-04.md) and [06](Lecture-06.md) is **finding another axis to split**: split over context (Flash-Decoding's idea), split over value tiles, split over expert groups × FFN bands. Every one of those is the same move — *manufacture parallelism where the workload's natural axes ran out.*

---

**5. The diagnosis changes with context depth**

One profile is not enough, because the mix moves with sequence length. The case study's clearest data:

| 8× H200, UD-IQ1_S, same weights | ctx 64 | ctx 131,072 | change |
|---|--:|--:|--:|
| llama.cpp | 18.32 | 18.44 | ~flat |
| SparkInfer-K3, as first measured | 10.34 | **1.00** | **−90%** |

Same engine, same weights, same box. One number is 1.8× behind the reference; the other is 18× behind. **They are diagnoses of different bottlenecks.**

At ctx 64 the KV cache is nearly empty, so per-token weight reads dominate and the engine looks merely unoptimized. At 131,072 the attention reduction over depth dominates — and the reference held flat because it keeps a *compressed* MLA cache (`kv_lora` 512, f16) while the candidate reduced over **576 f32 values per token in a kernel with one block per head.**

Three separate defects in one sentence, and you can only see them at depth: the cache is uncompressed (bandwidth), it is f32 (bandwidth), and the kernel has one block per head (occupancy).

> **Profile at the depth you ship.** A flat curve is a claim, not a default. If your candidate degrades with context and your reference does not, the gap *is* your diagnosis — and it is invisible at short context.

The corollary for your own harness: sweep context and plot both engines. The *shape* of the divergence tells you which resource depth consumes. [Lecture 06](Lecture-06.md) is what fixing this one looked like.

---

</details>

## 6. Amdahl 定律，附凭证

最后一个诊断陷阱，是那种在*赢下之后*才逮住优秀工程师的陷阱：**优化一个阶段会改变哪个阶段是瓶颈，并可能让原本有效的并行化策略失效。**

案例研究里的实例异常干净。分片策略 `ExpertsOnly` 把 896 个路由专家（553 GiB 中的 531 GiB）分布到 8 个 GPU 上，并**复制其余一切，包括 attention。** 那是正确的第一刀 —— 它拿下了几乎全部显存收益，每层只需要一次 collective 而不是两次，并且让 forward 在每个 rank 上都以完整维度运行，无需 per-rank 的 shape threading。

随后 MoE dispatch 快了约 3×：

| | MoE 加速前 | 加速后 |
|---|---:|---:|
| tp=1, 16 layers | 196.65 ms/token | **60.66** |
| tp=8, 16 layers | 80.61 ms/token | **54.33** |
| **tp=8 vs tp=1 加速比** | **2.44×** | **~1.1×** |

MoE 快了 3×，而 **tensor-parallel scaling 从 2.44× 崩到 1.1×。** 没有任何东西退化 —— `tp=8` 从 80.61 降到 54.33 ms/token，是实打实的 1.48× 收益。但被复制的 attention 没有变快，也没有被分片，于是它成了整个串行项。

Amdahl 定律说的正是这件事，而这些数字让你可以直接把串行占比读出来：

```text
   speedup(N) = 1 / ( s + (1-s)/N )      s = serial fraction

   before:  2.44× at N=8   →   s ≈ 0.33   (a third of the token was serial)
   after:   1.10× at N=8   →   s ≈ 0.90   (nine tenths is now serial)
```

并行部分缩小了 3×；串行部分没有动；所以它的*占比*从三分之一涨到十分之九。在 attention 被分片之前，之后每加一块 GPU 都几乎买不到任何东西。

两条可迁移的结论：

1. **任何一次显著收益之后，都要重新推导你的串行占比。** 优化之前测得的 scaling 数字在优化之后不再有效。发布一个过期的 TP-scaling 数字，就是团队最后买了一堆毫无用处的 GPU 的原因。
2. **下一个优化由新的串行占比选定，而不是由旧计划选定。** 该 repo 自己的路线图说得很直白：*“Attention 现在是整个串行项 —— `ShardPolicy::Full` 是主要杠杆。”*

还有第三条，更微妙：`ShardPolicy::Full` 是*已声明且刻意未启用*的，因为 loader 会对权重做分片，而 executor 仍按完整宽度索引它们 —— 读过一个 slice 的末尾。知道下一步该做什么，和知道它现在还不安全，是两种不同的状态，把它们混为一谈就会交付静默损坏。[Lecture 07](Lecture-07.md) 和 [Lecture 10](Lecture-10.md).

---

## 7. 诊断顺序

从最便宜的做起。当你为每个候选都拿到一个数字、并且其中一个明显就是约束项时，停下。

```text
   1.  BANDWIDTH CEILING           (arithmetic, no GPU)
       bytes/token → tok/s bound.  How far below are you?
       ≫10× below  → not bandwidth. keep going.

   2.  COLLECTIVE COST             (one microbenchmark)
       calls/token × µs/call.  What % of the step?
       <10%        → not comm. keep going.

   3.  LAUNCH BILL                 (count kernels, divide)
       launches/token vs step time.  µs available per kernel?
       <10 µs      → launch-bound. fuse or capture.

   4.  OCCUPANCY                   (read grid dims)
       blocks per launch vs SM count. Which kernels are <25%?
       any hot kernel with a tiny grid → occupancy. reshape it.

   5.  DEPTH SWEEP                 (repeat 1–4 at your real context)
       does your curve diverge from the reference's with depth?

   6.  SERIAL FRACTION             (after every win, re-derive)
       measure at N=1 and N=max. solve Amdahl for s.
```

步骤 1–4 花掉一个下午，前两步不用租任何硬件。在案例研究中，它们会依次给出：*不是带宽*（低了 1000×）、*不是 comm*（~2%）、*launch-bound*（在目标速率下约 6 µs/kernel）、*以及若干 occupancy 不到 10% 的热点 kernel* —— 这实际上就是 Lecture 04 到 08 的全部内容。

### 7.1 先写下来规则

在改代码之前，先立下一个预测：**哪个天花板在起约束，你要改什么，预期提升多少。** 然后再去测。

这不是走过场。它是唯一能发现你对*某件事为什么有效*判断错误的方法。一个按预期原因带来预期 10% 的改动，让你对这台机器多懂一点；一个以你未曾预料的原因带来 10% 的改动，则是一个你正要拿来做归纳的巧合。案例研究里两类都有，而只有封存的凭证能让任何人在事后把它们区分开。

---


<details>
<summary>English original</summary>

**6. Amdahl's law, with receipts**

The last diagnostic trap is the one that catches good engineers *after* a win: **optimizing one phase changes which phase is the bottleneck, and can make a parallelization strategy that used to work stop working.**

The case study's instance is unusually clean. The shard policy `ExpertsOnly` bands the 896 routed experts (531 of 553 GiB) across 8 GPUs and **replicates everything else, including attention.** That was the right first cut — it captures essentially all of the memory win, needs one collective per layer instead of two, and lets the forward run at full dimensions on every rank with no per-rank shape threading.

Then the MoE dispatch got ~3× faster:

| | before the MoE speedup | after |
|---|---:|---:|
| tp=1, 16 layers | 196.65 ms/token | **60.66** |
| tp=8, 16 layers | 80.61 ms/token | **54.33** |
| **tp=8 vs tp=1 speedup** | **2.44×** | **~1.1×** |

The MoE got 3× faster and **tensor-parallel scaling collapsed from 2.44× to 1.1×.** Nothing regressed — `tp=8` went from 80.61 to 54.33 ms/token, a real 1.48× win. But the replicated attention did not get faster and did not get sharded, so it became the entire serial term.

Amdahl's law says exactly this, and the numbers let you read the serial fraction off directly:

```text
   speedup(N) = 1 / ( s + (1-s)/N )      s = serial fraction

   before:  2.44× at N=8   →   s ≈ 0.33   (a third of the token was serial)
   after:   1.10× at N=8   →   s ≈ 0.90   (nine tenths is now serial)
```

The parallel part shrank by 3×; the serial part did not move; so its *share* went from a third to nine tenths. Every subsequent GPU you add buys ~nothing until attention is sharded.

Two transferable conclusions:

1. **After any significant win, re-derive your serial fraction.** A scaling number measured before an optimization is not valid after it. Publishing a stale TP-scaling figure is how a team ends up buying GPUs that do nothing.
2. **The next optimization is chosen by the new serial fraction, not by the old plan.** The repo's own roadmap says so plainly: *"Attention is now the whole serial term — `ShardPolicy::Full` is the main lever."*

And a third, subtler one: `ShardPolicy::Full` is *declared and deliberately not enabled*, because the loader would shard weights the executor still indexes at full width — reading past the end of a slice. Knowing the right next move and knowing it is not yet safe are different states, and conflating them ships silent corruption. [Lecture 07](Lecture-07.md) and [Lecture 10](Lecture-10.md).

---

**7. The diagnostic sequence**

Cheapest first. Stop when you have a number for every candidate and one of them is obviously binding.

```text
   1.  BANDWIDTH CEILING           (arithmetic, no GPU)
       bytes/token → tok/s bound.  How far below are you?
       ≫10× below  → not bandwidth. keep going.

   2.  COLLECTIVE COST             (one microbenchmark)
       calls/token × µs/call.  What % of the step?
       <10%        → not comm. keep going.

   3.  LAUNCH BILL                 (count kernels, divide)
       launches/token vs step time.  µs available per kernel?
       <10 µs      → launch-bound. fuse or capture.

   4.  OCCUPANCY                   (read grid dims)
       blocks per launch vs SM count. Which kernels are <25%?
       any hot kernel with a tiny grid → occupancy. reshape it.

   5.  DEPTH SWEEP                 (repeat 1–4 at your real context)
       does your curve diverge from the reference's with depth?

   6.  SERIAL FRACTION             (after every win, re-derive)
       measure at N=1 and N=max. solve Amdahl for s.
```

Steps 1–4 cost an afternoon and no rented hardware for the first two. In the case study they would have produced, in order: *not bandwidth* (1000× below), *not comm* (~2%), *launch-bound* (~6 µs/kernel at the target rate), *and several hot kernels under 10% occupancy* — which is, in fact, the whole content of Lectures 04 through 08.

**7.1 The write-it-down rule**

Before you change code, commit a prediction: **which ceiling binds, what you will change, and how much you expect.** Then measure.

This is not ceremony. It is the only way to detect that you were wrong about *why* something worked. A change that delivers the predicted 10% for the predicted reason teaches you something about the machine; a change that delivers 10% for a reason you did not anticipate is a coincidence you are about to generalize from. The case study has both kinds, and only the sealed receipts let anyone tell them apart afterwards.

---

</details>

## Lab — 动手之前先诊断

延续 [Lecture 01](Lecture-01.md) 的 lab 中的 `SHAPE.md`，并使用 [Lecture 02](Lecture-02.md) 的 harness（agent 运行时框架）。

1. **评估你的预测。** 把你在 Lecture 01 预测的带宽上限与实测 tok/s 比较。给出比值。若差距在 2× 以内，说明你受带宽限制，Lecture 05 和 06 就是你要处理的部分；若低了 10× 或更多，继续。
2. **为你的集合通信定价。** 按每个 token 统计全规约次数，*要从前向传播中数*，而不是从图上数。在你的真实 payload 大小上对其中一个做 microbenchmark。报告 µs/call 和占 step time 的百分比。还要报告 payload 放大 8× 和 64× 时的延迟——如果时间几乎不动，你就是延迟受限，优化目标是调用次数。
3. **数清你的启动次数。** 每 layer 的 kernel 数 × layer 数。用 step time 除以它。报告每个 kernel 可用的 µs。
4. **列出每个 kernel 的 grid。** 按（时间 × 空闲 SM 数）排序。报告前五名，给出 `blocks` 与你 SM 数的对比。
5. **扫描上下文。** 至少四个深度，从短的覆盖到你的上线上下文，你的 engine 和你的 reference 两者都要。把两者都画出来。用一句话描述二者的分歧。
6. **推导你的串行占比。** 在你最低和最高的并行度下测量；对 `s` 求解 Amdahl 定律。若无法在 N=1 下运行，就说明这一点并给出界限。
7. **提交 `DIAGNOSIS.md`**，写明瓶颈上限、支撑四个候选原因各自的数字、你对第一项改动的预期收益，以及**那一个能证明你错了的测量。**

通过标准：有人读过 `DIAGNOSIS.md` 后，能不问问题就复述你的瓶颈上限和你的证伪测试。

---

## Self-check

1. 你的带宽上限说 3600 tok/s；你实测 3.55。按你投入精力的顺序列出四个候选解释，以及能排除每个候选的最省成本的测量。
2. 一次集合通信在 14 KiB 时耗时 58.7 µs，在 7 MiB 时耗时 85 µs。它处于哪个区间，你应该报告什么单位，优化目标是什么？
3. 你数出每个 token 有 2790 次 kernel 启动，step 为 16.6 ms。这是启动受限吗？给出算式，并说明你必须对每次启动开销做的假设。
4. 两个 kernel 各耗时 400 µs。一个跑 132 个 block，另一个跑 12 个。你先动哪个，为什么，写代码之前你会先测什么来确认？
5. 你的 engine 在上下文 512 时落后 reference 1.8×，在 128k 时落后 18×。你的 attention cache 的哪一项架构特性最可能解释这一分歧？
6. 你对原本占 token 时间 70% 的那个阶段上线了 3× 的改进。计算你新的串行占比，以及 8 路并行下新的预期加速比。现在价值最高的下一项改动是什么？
7. 你的 TP 扩展性幻灯片写着 8 GPU 时 2.4×；它是在两个月前、四次优化合入之前测的。论证这张幻灯片如今是一种负担，而不只是过时。

---

## References

* **Roofline model** — Williams, Waterman, Patterson，*"Roofline: An Insightful Visual Performance Model"*（[CACM 2009](https://dl.acm.org/doi/10.1145/1498765.1498785)）。原始论文；§1 的方法就是把它用在 bytes-per-token 上。
* **Amdahl's law** — Amdahl，*"Validity of the single processor approach…"*（AFIPS 1967）。§6 就是一个带实测数字的教科书例子。
* **Flash-Decoding** — [PyTorch blog, Oct 2023](https://pytorch.org/blog/flash-decoding/)——§4.1 所描述、[Lecture 06](Lecture-06.md) 所应用的那个经典的“按上下文切分来制造并行度”的做法。
* **NVIDIA Nsight Systems** — [docs.nvidia.com/nsight-systems](https://docs.nvidia.com/nsight-systems/)——§3.1 用的工具。空白就是测量结果。
* **CUDA occupancy** — [CUDA C++ Best Practices Guide § Occupancy](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/#occupancy) 与 Occupancy Calculator API。
* **SparkInfer-K3** — [`docs/tensor-parallel.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/docs/tensor-parallel.md) 是 §2 的集合通信算术和 §6 的 Amdahl 表的来源。

Cross-references:

* [Part 1 Lecture 03 — Roofline、带宽与存储层次](../01-基础/Lecture-03.md)——§1 里的上限。
* [Part 2 Lecture 04 — 8× Hopper 上的张量并行](../02-Hopper稠密计算/Lecture-04.md)——§2 所纠正的“每层两次全规约”这个数字就出自这里。
* [Part 2 Lecture 07 — 通信层内部](../02-Hopper稠密计算/Lecture-07.md)——小消息延迟，即 §2 落入的区间。

---

## 截至 2026-08 有效

测量来自 8× H200 SXM 上的 SparkInfer-K3（`sm_90`，132 SM，~4.8 TB/s/卡），UD-IQ1_S，CUDA 12.8+，NCCL。集合通信数据来自 `tp_allreduce_check`；occupancy 结论来自 PR #77 / #115；Amdahl 表来自 `docs/tensor-parallel.md`。§7 中的*顺序*才是能长期保留的内容。

---


<details>
<summary>English original</summary>

**Lab — diagnose before you touch anything**

Continues the `SHAPE.md` from [Lecture 01](Lecture-01.md)'s lab, and uses the harness from [Lecture 02](Lecture-02.md).

1. **Grade your prediction.** Compare the bandwidth ceiling you predicted in Lecture 01 to your measured tok/s. State the ratio. If you are within 2×, you are bandwidth-bound and Lectures 05 and 06 are your part; if you are 10× or more below, continue.
2. **Price your collectives.** Count all-reduces per token *from the forward pass*, not from a diagram. Microbenchmark one at your real payload size. Report µs/call and percentage of step time. Also report the latency at 8× and 64× the payload — if time barely moves, you are latency-bound and the target is call count.
3. **Count your launches.** Kernels per layer × layers. Divide your step time by it. Report µs available per kernel.
4. **List every kernel's grid.** Sort by (time × idle SMs). Report the top five with `blocks` vs your SM count.
5. **Sweep context.** At least four depths spanning short to your shipping context, both your engine and your reference. Plot both. Describe the divergence in one sentence.
6. **Derive your serial fraction.** Measure at your lowest and highest parallelism degree; solve Amdahl for `s`. If you cannot run at N=1, say so and bound it.
7. **Commit `DIAGNOSIS.md`** naming the binding ceiling, the number supporting each of the four candidates, your predicted win for the first change, and **the one measurement that would prove you wrong.**

Pass criterion: someone reads `DIAGNOSIS.md` and can restate your binding ceiling and your falsification test without asking a question.

---

**Self-check**

1. Your bandwidth ceiling says 3600 tok/s; you measure 3.55. List the four candidate explanations in the order you would spend effort on them, and the cheapest measurement that eliminates each.
2. A collective takes 58.7 µs at 14 KiB and 85 µs at 7 MiB. What regime is it in, what unit should you report, and what is the optimization target?
3. You count 2790 kernel launches per token and a 16.6 ms step. Is this launch-bound? Show the arithmetic and state the assumption you had to make about per-launch overhead.
4. Two kernels each take 400 µs. One runs 132 blocks, the other runs 12. Which do you attack first, why, and what would you measure to confirm before writing code?
5. Your engine is 1.8× behind the reference at context 512 and 18× behind at 128k. What single architectural property of your attention cache most likely explains the divergence?
6. You ship a 3× improvement to the phase that was 70% of your token. Compute your new serial fraction and your new expected speedup at 8-way parallelism. What is now the highest-value next change?
7. Your TP-scaling slide says 2.4× at 8 GPUs; it was measured two months and four merged optimizations ago. Argue that the slide is now a liability rather than merely out of date.

---

**References**

* **Roofline model** — Williams, Waterman, Patterson, *"Roofline: An Insightful Visual Performance Model"* ([CACM 2009](https://dl.acm.org/doi/10.1145/1498765.1498785)). The original; §1's method is this applied to bytes-per-token.
* **Amdahl's law** — Amdahl, *"Validity of the single processor approach…"* (AFIPS 1967). §6 is a textbook instance with measured numbers.
* **Flash-Decoding** — [PyTorch blog, Oct 2023](https://pytorch.org/blog/flash-decoding/) — the canonical "manufacture parallelism by splitting over context" move that §4.1 describes and [Lecture 06](Lecture-06.md) applies.
* **NVIDIA Nsight Systems** — [docs.nvidia.com/nsight-systems](https://docs.nvidia.com/nsight-systems/) — the tool for §3.1. The whitespace is the measurement.
* **CUDA occupancy** — [CUDA C++ Best Practices Guide § Occupancy](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/#occupancy) and the Occupancy Calculator API.
* **SparkInfer-K3** — [`docs/tensor-parallel.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/docs/tensor-parallel.md) is the source for §2's collective arithmetic and §6's Amdahl table.

Cross-references:

* [Part 1 Lecture 03 — Roofline, bandwidth, and the memory hierarchy](../01-基础/Lecture-03.md) — the ceiling in §1.
* [Part 2 Lecture 04 — Tensor parallelism on 8× Hopper](../02-Hopper稠密计算/Lecture-04.md) — where the "two all-reduces per layer" figure §2 corrects comes from.
* [Part 2 Lecture 07 — Inside the communication layer](../02-Hopper稠密计算/Lecture-07.md) — small-message latency, the regime §2 lands in.

---

**Current as of 2026-08**

Measurements from SparkInfer-K3 on 8× H200 SXM (`sm_90`, 132 SMs, ~4.8 TB/s/card), UD-IQ1_S, CUDA 12.8+, NCCL. Collective figures from `tp_allreduce_check`; occupancy findings from PRs #77 / #115; Amdahl table from `docs/tensor-parallel.md`. The *sequence* in §7 is the durable content.

---

</details>

## Next

* Next: [Lecture 04 — Launch geometry: grids, occupancy, and 327 norms per token](Lecture-04.md)
* Previous: [Lecture 02 — The scoreboard: a benchmark that cannot be gamed](Lecture-02.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)


<details>
<summary>English original</summary>

**Next**

* Next: [Lecture 04 — Launch geometry: grids, occupancy, and 327 norms per token](Lecture-04.md)
* Previous: [Lecture 02 — The scoreboard: a benchmark that cannot be gamed](Lecture-02.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
