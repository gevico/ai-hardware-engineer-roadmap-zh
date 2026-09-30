---
title: 第 4 部分 · 第 07 讲 —— 分片 896 个专家，以及随之而来的 Amdahl 陷阱
description: 第 4 部分 · 第 07 讲 —— 分片 896 个专家，以及随之而来的 Amdahl 陷阱
---

# 第 4 部分 · 第 07 讲 —— 分片 896 个专家，以及随之而来的 Amdahl 陷阱

## 概述

896 个被路由的专家，top-16，分布在 8 个 GPU 上。最直观的划分 —— 每个 rank 112 个完整专家 —— 是正确的，*平均而言*是均衡的，却**让 MoE layer 关键路径上约 43% 的潜力白白流失**，原因纯粹是概率，而非工程。

本讲覆盖让张量并行 MoE layer 变快的四个决策：

1. **规约放在哪里** —— 由代数决定，而非由一张图决定。
2. **它有多大代价** —— 实测得出，且远低于所有人的假设。
3. **如何分片** —— 以及为什么关键路径是一个*最大值*，而不是均值。
4. **哪个集合通信会真正执行** —— 以及那被一个未设置的环境变量挡住数周的 7.2%。

再加上案例研究中最干净的 Amdahl 演示：一次**让张量并行扩展性变差的 3× MoE 加速**，2.44× → ~1.1×。

读完之后，你应当能够从第一性原理出发为集合通信定位，计算 top-*k* 路由方案的期望关键路径，并基于 balls-in-bins 论证而非参数扫描来决定分片几何。

---

## 1. 规约放在哪里是一个代数问题

从这里开始，因为搞错它是无声的（[第 10 讲 §4.3](Lecture-10.md)），而其余一切都依赖于此。

被路由的专家是按专家分片的，因此每个 rank 的 dispatch 累加器持有该 token 的 top-16 上的一个**部分和** —— 它持有这 16 个中归它所有的那个子集，其余位置为零。全规约把八个部分和变成单个持有全部 896 个专家的 GPU 会算出的结果。

唯一的问题是*在哪里*。而答案是唯一确定的：

```text
   the next op after the expert dispatch is  ffn_routed_norm,
   an RMS norm.  and:

        rms_norm( Σ partial )   ≠   Σ rms_norm( partial )

   RMS norm is NOT LINEAR, so the cross-rank sum must complete
   BEFORE it.
```

> **集合通信的位置由部分和下游的第一个非线性算子决定。**每一个线性算子 —— 缩放、加残差、再做一次矩阵乘 —— 都与求和可交换，放在哪一侧都可以。第一个非线性算子则不行，规约必须落在那里。

把它往后移两个算子、越过 `routed_up`，会同时出两处问题：你跳过了专家所需的那次规约，**并且** —— 因为 `routed_norm`、`routed_up` 和共享专家都是复制的，所以每个 rank 本就已经持有完整的张量 —— 你对一个完整张量做了规约，于是**把 FFN 输出乘上了 `tp_size`**。

把 FFN 输出乘以 8 不会崩溃。它产出的文本依旧通顺。

### 1.1 载荷比你猜的要窄

```text
   the reduce is 3584 wide,  NOT 7168.
```

被路由的专家位于一个**降维投影后的 latent 空间** —— `expert_latent_length` 3584，而 `hidden_size` 是 7168。按 hidden 宽度来估算集合通信的大小，会预测出实际搬运字节数的两倍。这与那个让每个专家 GEMM 都错 2× 的陷阱如出一辙 —— 只要你按 hidden 来估算（[第 01 讲 §1.1](Lecture-01.md)）。

在 f32 下，每次集合通信是 `3584 × 4 = 14 KiB`。注意 `7168 × 2`（bf16）也*同样*是 14 KiB —— 相同的数字，不同的原因，而这恰恰是那种会掩盖错误的巧合。引擎刻意跑 f32：把 f32 残差流送经 bf16 全规约，会在**每一个 layer 边界**截断到约 8 位尾数，使执行器的数值处理前功尽弃。

### 1.2 从前向传播数集合通信的次数

```text
   attention REPLICATED (ExpertsOnly):
       1 MoE reduce × 92 MoE layers                 =  92 per token
       (93 layers, minus the leading dense block, which has no
        expert dispatch to reduce)

   attention HEAD-SHARDED (after PR #63, Lecture 06 §5):
       92 MoE reduces  +  93 attention reduces      = 185 per token
```

来自 [第 2 部分 第 04 讲](../02-Hopper稠密计算/Lecture-04.md) 的教科书数字 —— 每 layer 两次全规约 —— 只对完全分片的模型成立。哪个数字适用于*你*，取决于你的分片策略；而案例研究自己的文档记录中，曾在正确答案为 92 的情况下引用过 186。

这个计数是**断言出来的**，而不是靠肉眼数出来的：*“少一次规约会留下专家的部分和；多一次规约会把一个完整张量乘以 `tp_size`。两者都不会崩溃，所以这个计数靠断言而非肉眼。”*本讲的每个 PR 都在其 bench 输出中、在每条 arm 上报告 `collectives=185`。这样你才能发现某次悄悄改变了通信模式的改动。

---


<details>
<summary>English original</summary>

**Part 4 · Lecture 07 — Sharding 896 Experts, and the Amdahl Trap That Followed**

**Overview**

896 routed experts, top-16, across 8 GPUs. The obvious partition — 112 whole experts per rank — is correct, balanced *on average*, and **leaves ~43% of the MoE layer's critical path on the table**, for a reason that is pure probability rather than engineering.

This lecture covers the four decisions that make a tensor-parallel MoE layer fast:

1. **Where the reduce goes** — determined by algebra, not by a diagram.
2. **What it costs** — measured, and much less than everyone assumes.
3. **How to shard** — and why the critical path is a *maximum*, not a mean.
4. **Which collective runs** — and the 7.2% that sat behind an unset environment variable for weeks.

Plus the case study's cleanest Amdahl demonstration: a **3× MoE speedup that made tensor-parallel scaling worse**, 2.44× → ~1.1×.

By the end you should be able to place a collective from first principles, compute the expected critical path of a top-*k* routing scheme, and decide a sharding geometry from a balls-in-bins argument rather than a sweep.

---

**1. Where the reduce goes is an algebra question**

Start here, because getting it wrong is silent ([Lecture 10 §4.3](Lecture-10.md)) and everything else depends on it.

The routed experts are expert-sharded, so each rank's dispatch accumulator holds a **partial sum** over the token's top-16 — it has whatever subset of those 16 it owns, and zero for the rest. The all-reduce turns eight partial sums into what one GPU holding all 896 experts would have computed.

The only question is *where*. And the answer is forced:

```text
   the next op after the expert dispatch is  ffn_routed_norm,
   an RMS norm.  and:

        rms_norm( Σ partial )   ≠   Σ rms_norm( partial )

   RMS norm is NOT LINEAR, so the cross-rank sum must complete
   BEFORE it.
```

> **A collective's position is determined by the first non-linearity downstream of the partial sum.** Every linear op — scaling, adding a residual, a further matmul — commutes with the sum and can happen on either side. The first non-linearity cannot, and that is where the reduce must land.

Move it two ops later, past `routed_up`, and two things go wrong simultaneously: you skip the reduce the experts needed, **and** — because `routed_norm`, `routed_up` and the shared experts are all replicated, so every rank already holds the complete tensor — you reduce a complete tensor and **multiply the FFN output by `tp_size`**.

Multiplying an FFN output by 8 does not crash. It produces fluent text.

**1.1 The payload is narrower than you would guess**

```text
   the reduce is 3584 wide,  NOT 7168.
```

The routed experts live in a **down-projected latent space** — `expert_latent_length` 3584 against `hidden_size` 7168. Sizing the collective off hidden width predicts twice the bytes that actually move. This is the same trap that makes every expert GEMM wrong by 2× if you size it off hidden ([Lecture 01 §1.1](Lecture-01.md)).

At f32 that is `3584 × 4 = 14 KiB` per collective. Note that `7168 × 2` (bf16) is *also* 14 KiB — the same number for a different reason, which is exactly the sort of coincidence that hides an error. The engine runs f32 deliberately: routing an f32 residual stream through a bf16 all-reduce would truncate to ~8 mantissa bits **at every layer boundary**, undoing the executor's numerics.

**1.2 Count the collectives from the forward pass**

```text
   attention REPLICATED (ExpertsOnly):
       1 MoE reduce × 92 MoE layers                 =  92 per token
       (93 layers, minus the leading dense block, which has no
        expert dispatch to reduce)

   attention HEAD-SHARDED (after PR #63, Lecture 06 §5):
       92 MoE reduces  +  93 attention reduces      = 185 per token
```

The textbook figure from [Part 2 Lecture 04](../02-Hopper稠密计算/Lecture-04.md) — two all-reduces per layer — is right only for a fully-sharded model. Which number applies to *you* depends on your shard policy, and the case study's own docs record having quoted 186 when the answer was 92.

The count is **asserted**, not eyeballed: *"A missing reduce leaves a partial expert sum; an extra one multiplies a complete tensor by `tp_size`. Neither crashes, so the count is asserted rather than eyeballed."* Every PR in this lecture reports `collectives=185` in its bench output, on every arm. That is how you notice a change that silently altered the communication pattern.

---

</details>

## 2. 集合通信的代价 —— 优化前先测量

8 张 GPU 上的 93 层模型*听起来*像是 comm-bound。事实并非如此。

```text
   MEASURED, ExpertsOnly, tp_size 8, 8× H200, NCCL:

     92 all-reduces/token × 14 KiB × 58.7 µs/call  =  ~5.4 ms/token
     token time at the time of measurement          =  281.6 ms
     collective share                               ≈  2%
```

结论：不是瓶颈。而且所处 regime 与占比同样重要：

```text
   512× the data costs 1.45× the time     (14 KiB → 7 MiB)
   ⇒ LATENCY-bound, not bandwidth-bound.
   ⇒ report µs/call, not GB/s.
   ⇒ optimize the CALL COUNT and the BARRIER, not the bytes.
```

这正是验证工具报告的是每次调用的微秒数的原因，也是 §7 的改进针对的是**barrier 机制**而不是带宽的原因。对任何小于 ~1 MB 的集合通信，GB/s 都是没有意义的单位 —— 它反映的是固定开销，而不是 fabric。

---

## 3. Amdahl 陷阱

shard 策略 `ExpertsOnly` 把 896 个 routed experts（553 GiB 中的 531 GiB）划成 band，并**复制其余一切，包括 attention**。这是正确的第一刀：基本拿到了全部内存收益，每层一次集合通信而不是两次，而且前向传播中任何地方都不需要按 rank 做 shape threading。

之后 MoE dispatch 快了约 3×：

| | MoE 加速前 | 加速后 |
|---|---:|---:|
| tp=1, 16 layers | 196.65 ms/token | **60.66** |
| tp=8, 16 layers | 80.61 ms/token | **54.33** |
| **tp=8 vs tp=1** | **2.44×** | **~1.1×** |

没有任何东西退化 —— `tp=8` 从 80.61 提升到 54.33 ms/token，是实打实的 1.48×。但被复制的 attention 既没有变快，也没有被 shard，于是它成了**整个串行项**。对串行占比解 Amdahl：

```text
   speedup(N) = 1 / ( s + (1−s)/N )

   before:  2.44× at N=8  →  s ≈ 0.33
   after:   1.10× at N=8  →  s ≈ 0.90
```

并行部分缩小了 3×；串行部分没有动；它的*占比*从三分之一涨到十分之九。现在再加 GPU 几乎买不到任何东西。

两个结论，第二个是团队最容易搞错的：

**每一次显著收益之后，都要重新推导你的串行占比。** 在某个优化之前测出的扩展性数字，在优化之后不再有效。发布一个过期的 TP 扩展性数据，就是一个组织买来一堆毫无用处的 GPU 的过程。

**下一个优化由新的串行占比决定，而不是由旧计划决定。** 仓库的路线图说得很直接 —— *"Attention is now the whole serial term"* —— 而 [PR #63](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) 对两个 attention band 都做了 head-shard，收益 **+72.6%**（[Lecture 06 §5](Lecture-06.md)）。这就是在你识别出串行项之后，修好它该有的样子。

还有第三点，更安静：`ShardPolicy::Full` —— 在 loader 里 shard attention 的*权重*，而不只是 shard 计算 —— 已被声明，但**刻意不启用**，因为 loader 会去 shard 那些 executor 仍按完整宽度索引的权重，从而读越切片的末尾。知道下一步该做什么，和知道它现在还不安全，是两种不同的状态，把它们混为一谈就会发布静默损坏。

---

## 4. 关键路径是最大值，不是均值

现在是本讲最好的一段推理，来自 [**PR #96**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96)。

whole-expert sharding 在纸面上看完美均衡：896 experts ÷ 8 ranks = 每 rank 112 个，而 top-16 意味着每个 rank *期望*拥有该 token 活跃 experts 中的 2 个。均匀。公平。

但同步的 layer 不会等平均的那个 rank。**它等的是最忙的那个。**

```text
   top_k = 16 active experts, thrown into 8 ranks
     → balls in bins:  16 balls, 8 bins

   expected count in a given bin      =  2.0
   expected count in the BUSIEST bin  ≈  4.2      ← this is what you wait for

   critical-path expert-rows = 4.20 × 3072 rows = 12,902
```

2.0 与 4.2 之间的差距是 **2.1×**，而且它不是靠重排分配就能修好的负载均衡 bug。它是多项分布的最大值。球少而箱子多时，最大值远高于均值，而 layer 的代价由最大值决定。

> **直接抄走。** 对任何在 *N* 个 rank 上做 top-*k* routing 的情况，你的同步关键路径由 `E[max bin]` 支配，而不是 `k/N`。当 *k* 很小时，两者相差两倍或更多。在得出你的 expert 划分是均衡的结论之前，先把它算出来 —— 解析地算，或者仿真算。


<details>
<summary>English original</summary>

**2. What the collective costs — measure before optimizing it**

A 93-layer model on 8 GPUs *sounds* comm-bound. It was not.

```text
   MEASURED, ExpertsOnly, tp_size 8, 8× H200, NCCL:

     92 all-reduces/token × 14 KiB × 58.7 µs/call  =  ~5.4 ms/token
     token time at the time of measurement          =  281.6 ms
     collective share                               ≈  2%
```

Verdict: not the bottleneck. And the regime matters as much as the share:

```text
   512× the data costs 1.45× the time     (14 KiB → 7 MiB)
   ⇒ LATENCY-bound, not bandwidth-bound.
   ⇒ report µs/call, not GB/s.
   ⇒ optimize the CALL COUNT and the BARRIER, not the bytes.
```

This is why the validation tool reports microseconds per call, and why §7's improvements are about *barrier mechanism* rather than bandwidth. For any collective under ~1 MB, GB/s is a meaningless unit — it tells you about your fixed overhead, not your fabric.

---

**3. The Amdahl trap**

The shard policy `ExpertsOnly` bands the 896 routed experts (531 of 553 GiB) and **replicates everything else, including attention**. That was the right first cut: essentially all of the memory win, one collective per layer instead of two, and no per-rank shape threading anywhere in the forward pass.

Then the MoE dispatch got ~3× faster:

| | before the MoE speedup | after |
|---|---:|---:|
| tp=1, 16 layers | 196.65 ms/token | **60.66** |
| tp=8, 16 layers | 80.61 ms/token | **54.33** |
| **tp=8 vs tp=1** | **2.44×** | **~1.1×** |

Nothing regressed — `tp=8` improved from 80.61 to 54.33 ms/token, a real 1.48×. But the replicated attention did not get faster and did not get sharded, so it became the **entire serial term**. Solving Amdahl for the serial fraction:

```text
   speedup(N) = 1 / ( s + (1−s)/N )

   before:  2.44× at N=8  →  s ≈ 0.33
   after:   1.10× at N=8  →  s ≈ 0.90
```

The parallel part shrank 3×; the serial part did not move; its *share* went from a third to nine tenths. Every additional GPU now buys almost nothing.

Two conclusions, and the second is the one teams get wrong:

**Re-derive your serial fraction after every significant win.** A scaling number measured before an optimization is not valid after it. Publishing a stale TP-scaling figure is how an organization buys GPUs that do nothing.

**The next optimization is chosen by the new serial fraction, not by the old plan.** The repo's roadmap said so directly — *"Attention is now the whole serial term"* — and [PR #63](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) head-sharded both attention bands for **+72.6%** ([Lecture 06 §5](Lecture-06.md)). That is what fixing the serial term looks like once you have identified it.

A third, quieter point: `ShardPolicy::Full` — sharding the attention *weights* in the loader, not just the compute — is declared and **deliberately not enabled**, because the loader would shard weights the executor still indexes at full width, reading past the end of a slice. Knowing the right next move and knowing it is not yet safe are different states, and conflating them ships silent corruption.

---

**4. The critical path is a maximum, not a mean**

Now the best piece of reasoning in this lecture, from [**PR #96**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96).

Whole-expert sharding looks perfectly balanced on paper: 896 experts ÷ 8 ranks = 112 each, and top-16 means each rank owns an *expected* 2 of the token's active experts. Even. Fair.

But a synchronous layer does not wait for the average rank. **It waits for the busiest one.**

```text
   top_k = 16 active experts, thrown into 8 ranks
     → balls in bins:  16 balls, 8 bins

   expected count in a given bin      =  2.0
   expected count in the BUSIEST bin  ≈  4.2      ← this is what you wait for

   critical-path expert-rows = 4.20 × 3072 rows = 12,902
```

The gap between 2.0 and 4.2 is a factor of **2.1×**, and it is not a load-balancing bug you can fix by shuffling the assignment. It is the max of a multinomial. With few balls and many bins, the maximum is far above the mean, and the layer's cost is set by the maximum.

> **Steal this.** For any top-*k* routing over *N* ranks, your synchronous critical path is governed by `E[max bin]`, not `k/N`. With small *k*, those differ by a factor of two or more. Compute it — analytically or by simulation — before you conclude your expert partition is balanced.

</details>

### 4.1 修复方案：用一个完美均衡的轴，换掉严重失衡的轴

关键在于，**FFN 宽度是一个固定维度。** 无论布线如何，每个 expert 永远恰好有 3072 个 FFN 行。沿这条轴切分在构造上就是*完美*均衡的 —— 其中完全不含随机性。

于是沿**两个**轴分片：expert group × FFN band。

```text
   eg = 2:  each rank owns  448 of 896 experts
                       AND  768 of each expert's 3072 FFN rows
            → the SAME weight bytes per rank as before

   fewer, fatter expert groups → worse expert balance:
       16 balls in 2 bins  →  E[busiest] ≈ 9.57   (up from 4.2)

   but each active expert is now only 768 rows, not 3072:

   critical-path expert-rows   before  4.20 × 3072 = 12,902
                               after   9.57 ×  768 =   7,350
                                                     (0.57×)
```

你刻意让*随机*轴变得更差 —— 预期最忙从 4.2 → 9.57 —— 因为你在一条**无方差**的轴上把损失拿了回来。每个 rank 的总字节数不变；只有关键路径缩短了 43%。

实测：**128k 下 33.42 → 29.81 ms/token，decode（逐 token 生成阶段）提升 12.1%。**

> **一般原则。** 当某个 partition 的不均衡来自随机性时，去找一条确定性的第二轴，把并行度挪到它上面。用一个固定维度换掉随机维度，即使会拉长平均值，也能缩短关键路径。

这是 [Part 3 Lecture 03](../03-Blackwell-MoE/Lecture-03.md) 中 grouped-GEMM（矩阵-矩阵乘）/ all-to-all 机制的单节点亲戚 —— 两者回答的都是同一个问题：“布线不均衡，而同步层要为最坏情况买单。”

### 4.2 为什么它不需要改动 kernel

让这件事代价如此之低的实现细节值得一说，因为它本身就说明了从一开始该怎样写 MoE kernel：

> *没有 `kernels/` 文件改动。`moe_gate_up_situ_kernel` 已经用 `gate_exps + (e·ffn + j)·blocks_per_row` 和 `moe_down_combine_kernel` `down_exps + (e·latent + o)·blocks_per_row` 做索引；**把该 rank 的 768 行 band 作为 `ffn` 传入，寻址其 packed buffer 的方式与 3072 寻址整块时完全一致。** 只有 loader 的 packing 变了。*

由于 kernel 把 FFN 宽度当作*参数*而非常量，沿这条轴分片只是改变 loader 写入的内容以及传入的那个数字。若 kernel 里写死 `#define FFN 3072`，就必须重写。

而正确性论证是类比代码库中已有的东西：

> *shared expert 已经这样分片：gate/up 按行分片，`situ` 是逐元素的并保持 band 不变，`down` 在同一 band 上按列分片，留下一个全宽 partial，由已有的 expert all-reduce 求和。**没有新的 collective，其宽度和次数也不变** —— 仍是 185 collectives/token。*

这正是 [Part 2 Lecture 04](../02-Hopper稠密计算/Lecture-04.md) 中“先行后列”的组合方式：对 up-projection 按行分片，使每个 rank 产生自己那份 band 的中间结果，把逐元素激活值留在 band 内，再对 down-projection 按列分片，使每个 rank 产生一个全宽 partial，由*已有的* reduce 求和。FFN band 搭在了一个本来就存在的 collective 上。

---

## 5. 默认行为必须就是你所宣称的东西

[**PR #96**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96) 的第一版把 2-D 分片放在一个 opt-in 环境变量后面。结果是：

> *那是个错误：**评测会构建代码树并跑 bench，但它不设置环境变量**，所以它测的是整 expert 路径，并如实地给这个 PR `eval:none` 打了比 frontier 高 0.1% 的分。**harness（agent 运行时框架）够不到的性能改动，就不是性能改动。**”*

这与 [Lecture 04 §6](Lecture-04.md) 中的单二进制 A/B 技术直接冲突，而解决方式很重要：**先用开关把它 gated 起来，以便测量，然后让新行为成为默认，把开关留给旧行为。** `SPARKINFER_K3_MOE_2D=1` 现在恢复整 expert 分片 —— 开关仍然存在，但它选择的是*旧*路径。

成为默认强制带来了两件 opt-in 不需要的事：

* **承受不了它的 shape 也必须能加载。** 拒绝加载会把一个调优默认变成可移植性回归。所以：*显式请求 → 硬报错；替你选的 → 回退到整 expert 并在 stderr 上说明。* 这是安全的，因为维度校验器在写入任何东西*之前*运行，整 expert 的 band 保持完好。
* **默认策略由一个测试钉住**，*“因为行为默认若悄悄漂移，就会重新分片每个部署的 expert。”*

> **两条规则。** 显式请求的配置如果做不到，就应当**响亮地失败**；代表用户选择的配置则应当**安静地降级并说明**。而任何值得存在的默认行为，都值得配一个它一变就失败的测试。


<details>
<summary>English original</summary>

**4.1 The fix: trade a badly-balanced axis for a perfectly-balanced one**

The insight is that **the FFN width is a fixed dimension.** Every expert has exactly 3072 FFN rows, always, regardless of routing. Splitting on that axis is *perfectly* balanced by construction — there is no randomness in it at all.

So shard on **two** axes: expert groups × FFN band.

```text
   eg = 2:  each rank owns  448 of 896 experts
                       AND  768 of each expert's 3072 FFN rows
            → the SAME weight bytes per rank as before

   fewer, fatter expert groups → worse expert balance:
       16 balls in 2 bins  →  E[busiest] ≈ 9.57   (up from 4.2)

   but each active expert is now only 768 rows, not 3072:

   critical-path expert-rows   before  4.20 × 3072 = 12,902
                               after   9.57 ×  768 =   7,350
                                                     (0.57×)
```

You deliberately make the *random* axis worse — 4.2 → 9.57 expected busiest — because you get the loss back on an axis with **no variance**. Total bytes per rank are unchanged; only the critical path shortens, by 43%.

Measured: **33.42 → 29.81 ms/token at 128k, +12.1% decode.**

> **The general principle.** When a partition's imbalance comes from randomness, look for a second axis that is deterministic, and move parallelism onto it. Trading a stochastic dimension for a fixed one shortens the critical path even when it lengthens the average.

This is the single-node relative of the grouped-GEMM / all-to-all machinery in [Part 3 Lecture 03](../03-Blackwell-MoE/Lecture-03.md) — both are answers to "routing is uneven, and synchronous layers pay for the worst case."

**4.2 Why it needed no kernel changes**

The implementation detail that makes this cheap is worth noting, because it says something about how to write MoE kernels in the first place:

> *No `kernels/` file changes. `moe_gate_up_situ_kernel` already indexes `gate_exps + (e·ffn + j)·blocks_per_row` and `moe_down_combine_kernel` `down_exps + (e·latent + o)·blocks_per_row`; **passing the rank's 768-row band as `ffn` addresses its packed buffer exactly as 3072 addressed the whole one.** Only the loader's packing changes.*

Because the kernels took the FFN width as a *parameter* rather than a constant, sharding that axis is a change to what the loader writes and what number gets passed in. A kernel with `#define FFN 3072` would have required a rewrite.

And the correctness argument is by analogy to something already in the codebase:

> *The shared expert already shards this way: gate/up row-shard, `situ` is elementwise and preserves the band, `down` col-shards over the same band leaving a full-width partial that the existing expert all-reduce sums. **No new collective and no change to its width or count** — still 185 collectives/token.*

That is the row-then-col composition from [Part 2 Lecture 04](../02-Hopper稠密计算/Lecture-04.md): row-shard the up-projection so each rank produces its own band of intermediates, keep the elementwise activation inside the band, then col-shard the down-projection so each rank produces a full-width partial that the *existing* reduce sums. The FFN band rides on a collective that was already there.

---

**5. The default must be the thing you claim**

[**PR #96**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96)'s first revision put 2-D sharding behind an opt-in environment variable. The result:

> *That was a mistake: **the eval builds the tree and runs the bench, it does not set environment variables**, so it measured the whole-expert path and correctly scored the PR `eval:none` at 0.1% over frontier. **A perf change the harness cannot reach is not a perf change.**"*

This is in direct tension with the single-binary A/B technique from [Lecture 04 §6](Lecture-04.md), and the resolution is important: **gate it so you can measure it, then make the new behaviour the default and keep the gate for the old one.** `SPARKINFER_K3_MOE_2D=1` now restores whole-expert sharding — the gate still exists, but it selects the *old* path.

Being a default forced two things opt-in did not need:

* **A shape that cannot take it must still load.** Refusing would turn a tuning default into a portability regression. So: *explicit request → hard error; chosen-for-you → fall back to whole-expert and say so on stderr.* Safe because the dimension validator runs *before* it writes anything, leaving the whole-expert band intact.
* **The default policy is pinned by a test**, *"since a behavioural default that drifts silently re-shards every deployment's experts."*

> **The two rules.** An explicitly requested configuration should **fail loudly** if impossible; a configuration chosen on the user's behalf should **degrade quietly and say so**. And any behavioural default worth having is worth a test that fails when it changes.

</details>

### 5.1 藏在一个未设置变量后面的 7.2%

[**PR #74**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/74) 是这一类失败最纯粹的实例，也是仓库中每行价值最高的改动之一——约 45 行，没有新增代码路径。

[PR #59](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/59) 构建了一个快速集合通信，并在 eval 节点上测量了两种配置：

| build | ms/token | tok/s |
|---|--:|--:|
| #59, NCCL backend | 58.77 / 57.18 | 17.02–17.49 |
| **#59, peer one-shot** | **54.52 / 54.48** | **18.35** |

相差 7.2%。然后：

> *紧随其后的 eval 轮次把前沿记录在 **17.46 tok/s**——也就是 NCCL 的数字。**#59 构建的快速路径，自合并那天起就一直待在一个测量链路里没有任何东西会去设置的环境变量后面。**"*

未设置 `SPARKINFER_TP_BACKEND` 就意味着 NCCL。benchmark 脚本和 eval bot 里都没有设置它。于是这个快速集合通信被合并、被验证、被测量——却从未在生产环境或任何计分轮次中运行过一次。#74 让未设置意味着 **auto**：当所有对等对之间都存在 peer access 时用 peer-oneshot，否则用 NCCL。

还要注意作者关于证据的说法：

> *这个 PR 的论断没有一处来自我的测量——它是 #59 自己在固定节点上的数字，并与公开账本交叉核对过。这个 PR 改变的只是：默认值落到了胜出的那一种配置上。*

一个 PR，全部内容就是改一个默认值，全部理由都来自别人已经封存的测量结果，并与公开日志交叉核对。这之所以可能，只因为账本存在（[Lecture 02](Lecture-02.md)）。

> **拿你的测量来审查你的默认值。** 对每个性能 flag，都要问：*我的 benchmark 所跑的配置，与我的用户所跑的配置一致吗？* 只有你的 flag 能到达的优化，等于没有人拥有的优化。

还有值得照搬的克制：**multimem 被有意排除在 auto 集合之外**，因为它在硬件上仍未验证，而 peer-oneshot 有实测的前后对比、背后还有一个验证工具。“Auto”应当在你有*证据*的选项之间做选择，而不是在存在的选项之间做选择。

---

## 6. 更快的集合通信：实际改变了什么

[**PR #59**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/59) —— **16.06 → 18.35 tok/s，+14.3%** —— 打包了三件事，而它们之间的拆分会说明问题：

| build | ms/token | tok/s | vs main |
|---|--:|--:|--:|
| main | 62.24 / 62.29 | 16.06 | — |
| this PR, **NCCL** backend | 58.77 / 57.18 | 17.25 | −6.9% |
| this PR, **peer one-shot** | 54.52 / 54.48 | **18.35** | −12.5% |

中间那一行是*除*集合通信算法之外的一切所带来的价值——shared-expert banding 与 staging 拷贝的消除。最后一行再加上 barrier 机制。两者都报告出来，才能把这两份贡献分离开。

**“peer one-shot” 改变了什么。** 每个 rank 直接读取所有 peer，并以 f32 求和，配一个 **kernel 内 flag barrier**——每个 rank 一个 kernel，没有 host event，没有跨 stream 的 graph 边。对比 NCCL 的 ring，在 14 KiB 下，赢的不是带宽，而是那个 *barrier*。既然 §2 已经确认这个区间是延迟受限的，barrier 机制就是唯一还剩的优化点。（这与 [Part 2 Lecture 07](../02-Hopper稠密计算/Lecture-07.md) 讲的是同一套小消息 custom-all-reduce 的故事，代码在 Apache-2.0 下归属于 vLLM 的 `custom_all_reduce`。）

**为什么必须把 f32 做出来。** 快速后端只支持 bf16，所以 K3 的 f32 residual stream 回退到了 NCCL——快速路径存在，却对这个模型不可达。而且回退是*提早*协商的：`make_collective(..., need_f32=true)` 会在**20 分钟的权重加载之前**就把只支持 bf16 的后端降级，而不是等到第一次集合通信时才失败。

**Mode B，以及为什么 staging 不是选项。** NCCL 原地规约调用者自己的 buffer；快速后端做不到——只有绑定 multicast 或注册为 peer 的分配才能支撑它们的 load，因此 buffer 必须属于集合通信本身。把这件事藏在一次拷入、拷出调用者 buffer 的拷贝之后，会让每次集合通信多出两次 14 KiB 的设备到设备拷贝——**每个 token 多 372 次拷贝**。所以 in-place API 在 Mode-B 后端上 *returns false*，而不是悄悄做 staging；这样接错线的 forward 会立刻失败，而不必每个 token 付 186 次那样的代价。

> **当一个优化需要不同的调用约定时，就把这个约定暴露出来。** 用拷贝把它糊过去，代价可能超过优化本身省下的收益；而悄悄回退到慢的形态，比硬报错更糟。


<details>
<summary>English original</summary>

**5.1 The 7.2% that sat behind an unset variable**

[**PR #74**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/74) is the purest instance of this failure, and one of the highest-value-per-line changes in the repository — about 45 lines, no new code paths.

[PR #59](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/59) had built a fast collective and measured both arms on the eval node:

| build | ms/token | tok/s |
|---|--:|--:|
| #59, NCCL backend | 58.77 / 57.18 | 17.02–17.49 |
| **#59, peer one-shot** | **54.52 / 54.48** | **18.35** |

7.2% apart. And then:

> *The eval round that followed recorded the frontier at **17.46 tok/s** — the NCCL number. **The fast path #59 built has been sitting behind an environment variable nothing in the measurement chain sets, since the day it merged.**"*

An unset `SPARKINFER_TP_BACKEND` meant NCCL. Nothing in the benchmark scripts or the eval bot set it. So the fast collective was merged, validated, measured — and never once ran in production or in any scored round. #74 makes unset mean **auto**: peer-oneshot when peer access exists across all pairs, NCCL otherwise.

Note also what the author says about the evidence:

> *Nothing about this PR's claim is my measurement — it is #59's own numbers on the pinned node, cross-checked against the public ledger. What this PR changes is only that the default reaches the arm that won.*

A PR whose entire content is changing a default, justified entirely by someone else's already-sealed measurements, cross-checked against the public log. That is only possible because the ledger exists ([Lecture 02](Lecture-02.md)).

> **Audit your defaults against your measurements.** For every performance flag, ask: *does the configuration my benchmark runs match the configuration my users run?* An optimization only your flags reach is an optimization nobody has.

And the restraint worth copying: **multimem is deliberately left out of the auto set**, because it remains unvalidated on hardware while peer-oneshot has a measured before/after and a validation tool behind it. "Auto" should select among options you have *evidence* for, not among options that exist.

---

**6. Faster collectives: what actually changed**

[**PR #59**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/59) — **16.06 → 18.35 tok/s, +14.3%** — bundles three things, and the split between them is instructive:

| build | ms/token | tok/s | vs main |
|---|--:|--:|--:|
| main | 62.24 / 62.29 | 16.06 | — |
| this PR, **NCCL** backend | 58.77 / 57.18 | 17.25 | −6.9% |
| this PR, **peer one-shot** | 54.52 / 54.48 | **18.35** | −12.5% |

The middle row is the value of everything *except* the collective algorithm — the shared-expert banding and the staging-copy elimination. The bottom row adds the barrier mechanism. Reporting both isolates the two contributions.

**What "peer one-shot" changes.** Every rank reads all peers directly and sums in f32, with an **in-kernel flag barrier** — one kernel per rank, no host events, no cross-stream graph edges. Against NCCL's ring, at 14 KiB, the win is not bandwidth; it is the *barrier*. Since §2 established this regime is latency-bound, the barrier mechanism is the only thing left to optimize. (This is the same small-message custom-all-reduce story as [Part 2 Lecture 07](../02-Hopper稠密计算/Lecture-07.md), and the code is attributed to vLLM's `custom_all_reduce` under Apache-2.0.)

**Why f32 had to be built.** The fast backends were bf16-only, so K3's f32 residual stream fell back to NCCL — the fast path existed and was unreachable for this model. And the fallback is negotiated *early*: `make_collective(..., need_f32=true)` downgrades a bf16-only backend **before the 20-minute weight load**, rather than failing at the first collective.

**Mode B, and why staging is not an option.** NCCL reduces the caller's own buffer in place; the fast backends cannot — only multicast-bound or peer-registered allocations can back their loads, so the buffer must belong to the collective. Hiding that behind a copy into and out of a caller buffer would add two 14 KiB device-to-device copies per collective — **372 extra copies per token**. So the in-place API *returns false* on a Mode-B backend rather than silently staging, making a mis-wired forward fail immediately instead of paying that cost 186 times a token.

> **When an optimization requires a different calling convention, expose the convention.** Papering over it with copies can cost more than the optimization saves, and a silent fallback to the slow shape is worse than a hard error.

</details>

### 6.1 重结合，以及它朝哪个方向移动

#59 与 #96 都明确**不是 bit 级完全一致**，且都精确说明了重结合发生在何处：

* **#59：** *"对共享专家做 banding，把一次 6144 项收缩替换为八个 768 项的部分和并由集合通信求和，f32 reduce 随之重结合。**实测效果是朝参考靠拢，而非远离。**"* 相对 llama.cpp 的平均 KLD 为 **8.075e-03 → 3.867e-03** —— 2× 的*改善*，原因正是 [Lecture 06 §2.1](Lecture-06.md) 所述的成对求和。
* **#96：** 做的是三方比较，而非两方：

```text
                                   mean KLD    top-1     top-5
   whole-expert  vs reference      4.063e-03   100.00 %  100.00 %
   2-D default   vs reference      5.146e-03   100.00 %  100.00 %
   2-D  vs  whole-expert directly  3.255e-03   100.00 %  100.00 %
```

而它的解读，堪称统计诚实性的典范：

> *在这里，2-D 路径与参考的距离比 whole-expert 略远；**而在上一个基线上它反而略近。**两个差异都小于二者各自与参考的差距，这才是诚实的解读：重结合求和使 logits 的移动量约等于既有量化噪声的量级，方向则取决于 prompt 恰好落在哪一侧。*

作者得到的结果略微偏向*旧*路径，如实报告了它，指出它在更早的基线上走向相反，并得出结论：差异属于噪声量级，而不是替任何一方造势。**直接的 A 与 B 比较**（3.255e-03）使这一论证成为可能：若两个候选彼此之间的差异小于各自与参考的差异，那么二者都谈不上更接近。

> **拿走这条。** 比较两个近似时，要测量全部三个距离：A 到参考、B 到参考，以及 **A 到 B**。没有第三个，就分不清真实差异与重采样噪声。

---

## 7. 验证一个 shard：看守恒，而非计数

[#96](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96) 附带 **7,150 个 CPU 检查** —— 5,117 个针对 shard 数学、1,740 个针对权重规划、293 个针对驻留 —— 全程无需 GPU。其中有一个是本案例研究中最出色的测试：

> *驻留测试增加了一个**字节级 2-D 守恒用例**：它在字节映射上重放每个 rank 的拷贝描述符，并要求**每个字节恰好被覆盖一次**，因为**字节*计数*检查会放过这样一个 stride：它读取的字节数量正确，读到的位置却完全错误。**"*

这一句就是全部洞见所在。stride 被转置的 2-D 跨步拷贝，搬运的字节数量分毫不差，来源位置却完全错误。任何校验*总量*的测试都会放过它。只有校验*哪些字节*的测试才能抓住它 —— 而 [Lecture 10 §2](Lecture-10.md) 就是一个真实上线过、而且跑得很快的转置 stride bug。

该测试还覆盖了**拒绝（refusal）**的情形，而分片约束正栖身于此：

```text
   · group count must divide BOTH tp_size AND n_experts
   · the FFN shard must be a whole number of quant blocks
        768 = 3 × 256   ✓
   · a refusal must leave the ShardDims UNTOUCHED
        (the fallback path in §5 depends on this)
```

量化块约束是 [Part 2 Lecture 03 §5.4](../02-Hopper稠密计算/Lecture-03.md) 中 FP8-block/TP-alignment 这个坑的一般形式：**量化块不能跨 rank 拆分**，因此每个 shard 边界都必须是块边界。任何作用于量化权重的分片方案都继承这一点，它在性能开始约束你之前，就已经约束了你的 group 数量。

而最后一行 —— *拒绝必须不触碰状态* —— 是一条事务性属性。只有当校验器无法在拒绝之前把配置写一半时，§5 的静默回退行为才是安全的。

---


<details>
<summary>English original</summary>

**6.1 Reassociation, and which direction it moves**

Both #59 and #96 are explicitly **not bit-identical**, and both explain exactly what reassociates:

* **#59:** *"banding the shared expert replaces one 6144-term contraction with eight 768-term partials summed by the collective, and the f32 reduce reassociates. **The measured effect is toward the reference, not away from it.**"* Mean KLD vs llama.cpp **8.075e-03 → 3.867e-03** — a 2× *improvement*, for the pairwise-summation reason from [Lecture 06 §2.1](Lecture-06.md).
* **#96:** three-way comparison rather than two:

```text
                                   mean KLD    top-1     top-5
   whole-expert  vs reference      4.063e-03   100.00 %  100.00 %
   2-D default   vs reference      5.146e-03   100.00 %  100.00 %
   2-D  vs  whole-expert directly  3.255e-03   100.00 %  100.00 %
```

And the reading, which is a model of statistical honesty:

> *The 2-D path sits slightly further from the reference than whole-expert here; **on the previous base it sat slightly closer.** Both differences are smaller than the gap either one has to the reference, which is the honest reading: re-associating the sum moves the logits by about the size of the existing quantisation noise, in whichever direction the prompt happens to fall.*

The author had a result that mildly favoured the *old* path, reported it, noted it had gone the other way on a previous base, and concluded the difference is noise-scale rather than spinning either sign. The **direct A-vs-B comparison** (3.255e-03) is what makes that argument possible: if the two candidates differ from each other by less than either differs from the reference, neither is meaningfully closer.

> **Steal this.** When comparing two approximations, measure all three distances: A-to-reference, B-to-reference, and **A-to-B**. Without the third you cannot tell a real difference from resampling noise.

---

**7. Verifying a shard: conservation, not counting**

[#96](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96) ships **7,150 CPU checks** — 5,117 on shard math, 1,740 on the weight plan, 293 on residency — all GPU-free. And one of them is the best test in the case study:

> *The residency test adds a **byte-level 2-D conservation case**: it replays every rank's copy descriptor over a byte map and requires **each byte to be covered exactly once**, since **a byte *count* check would pass a stride that reads the wrong bytes in the right quantity.**"*

That clause is the whole insight. A 2-D strided copy with transposed strides moves exactly the right number of bytes from exactly the wrong places. Any test that verifies *totals* passes it. Only a test that verifies *which bytes* catches it — and [Lecture 10 §2](Lecture-10.md) is a real transposed-stride bug that shipped and was fast.

The test also covers the **refusals**, which is where sharding constraints live:

```text
   · group count must divide BOTH tp_size AND n_experts
   · the FFN shard must be a whole number of quant blocks
        768 = 3 × 256   ✓
   · a refusal must leave the ShardDims UNTOUCHED
        (the fallback path in §5 depends on this)
```

The quant-block constraint is the general form of the FP8-block/TP-alignment footgun from [Part 2 Lecture 03 §5.4](../02-Hopper稠密计算/Lecture-03.md): **you cannot split a quantization block across ranks**, so every shard boundary must be a block boundary. Any sharding scheme on quantized weights inherits this, and it constrains your group counts before performance does.

And the last line — *a refusal must leave the state untouched* — is a transactional property. §5's quiet-fallback behaviour is only safe if the validator cannot half-write a configuration before rejecting it.

---

</details>

## 8. 披露一个你未修复的回归

[#96](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96) 的结尾章节是「如何交付一个不完美的改动」的范本。

**代价：** 模型加载 **~22 s → ~31 s**。`down` slice 是一个 small-pitch 的 `cudaMemcpy2D` —— IQ1_S 下 1.6M 行、每行约 150 B —— 尽管字节数相同，其效率仍低于连续的 host 到 device 传输。*“它在计时区间之外，也不影响评分，但它是一个真实的回归。”*

**尝试过的修复，经过测量后被否决：**

> *本 PR 的一个早期修订版声称，host 侧重打包能把它找回来。**我实现了并做了测量，结果更糟：48.3 s 对 30.9 s。** gather 是每个 rank 1.6M 次单线程的小 `memcpy`，首次触碰 mmap 的 page-cache 页，外加对字节的第二遍完整遍历 —— **驱动自身的 strided walk 胜过它。**”*

**这个负面结果记录在哪里：**

> *已回退，数字和原因都**记录在调用点**，这样下一个评估那行代码的人就不必再花同一个下午在上面。*

在别人会想改的那一行上留一条注释，里面装着说明不该这么改的测量结果。这比把同样的信息留在一个已关闭的 PR 里更有价值，因为那正是下一位工程师会去看的地方。

**以及该结论的边界：**

> *我没有在 gather 下重跑 logit 比较，所以那里的“正确”是**由构造保证**的 —— 它把相同的源区间复制进相同的布局 —— **而非由测量保证。** [...] 多线程 gather 也许仍然会赢，但我没有测量过，就不会这么宣称。*

一段话里三个习惯：区分由构造保证的正确与由测量保证的正确，披露未修复的回归而不是指望没人去看加载时间，以及拒绝对一个未尝试过的优化做猜测。

另需注意，这个回归**位于评分区间之外**。省略它本来很容易。之所以要写进去，是因为对任何部署它的人来说加载时间都是真实的，而一个没有覆盖某项内容的评分指标，并不能让那项内容变成免费。

### 8.1 同一个 PR 中的另外两个汇报习惯

**给出统计量，而不只是均值。** 五组 ABBA 交错配对，第一组作为 warm-up 丢弃：

```text
   whole-expert   33.43 33.44 33.36 33.43 33.43   mean 33.418  sd 0.033
   2-D default    29.82 29.83 29.80 29.80 29.80   mean 29.810  sd 0.014

   delta −3.608 ms/token   (−10.80% latency, +12.10% throughput)
   Welch t = 226;  the arms do not overlap
                   (base min 33.36 > 2-D max 29.83)
```

对读者来说，不重叠的陈述才是有用的：*快的那一臂最差的一次运行，优于慢的那一臂最好的一次运行。* 不需要统计学学位。

**拒绝一个好看的分母。** 该机器自测 `main` 为 29.92，而记录在案的前沿是 26.09：

> *引用“较前沿 +28%”会把一个并非由本次改动造成的机器/build 差异记到这次改动头上。站得住脚的数字是同机 A/B：**+12.1%**。*

再说百分比与绝对值：较早的一个修订版在更旧的基线上测得同一改动为 38.31 → 34.76。*“绝对节省基本不变，约 3.6 ms，因为 MoE（混合专家模型）的不均衡是一笔 #90 未触及的**固定开销**；百分比上升是因为 token 的其余部分变快了。”* **一笔针对固定开销的修复，会在其他一切都在变快时收获不断上升的百分比** —— 在你下结论说自己的改动变好了之前，这一点值得弄明白。

---


<details>
<summary>English original</summary>

**8. Disclosing a regression you did not fix**

[#96](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96)'s closing section is a model of how to ship an imperfect change.

**The cost:** model load goes **~22 s → ~31 s**. The `down` slice is a small-pitch `cudaMemcpy2D` — 1.6M rows of ~150 B at IQ1_S — which is less efficient than a contiguous host-to-device transfer despite an identical byte count. *"Outside the timed region and it does not touch the score, but it is a real regression."*

**The attempted fix, measured and rejected:**

> *An earlier revision of this PR promised a host-side repack would recover it. **I implemented that and measured it, and it is worse: 48.3 s against 30.9 s.** The gather is 1.6M single-threaded small `memcpy`s per rank, first-touching mmap'd page-cache pages, plus a second full pass over the bytes — **the driver's own strided walk beats it.**"*

**Where the negative result was recorded:**

> *Reverted, with the numbers and the reason **recorded at the call site** so the next person sizing up that line does not spend the same afternoon on it.*

A comment at the line someone will want to change, containing the measurement that says not to. That is worth more than the same information in a closed PR, because it is where the next engineer will be looking.

**And the boundary of the claim:**

> *I did not re-run the logit comparison under the gather, so "correct" there is **by construction** — it copies the same source ranges into the same layout — **not by measurement.** [...] A threaded gather might still win, but I have not measured it and will not claim it.*

Three habits in one paragraph: distinguishing correct-by-construction from correct-by-measurement, disclosing an unfixed regression rather than hoping nobody looks at load time, and declining to speculate about an optimization not attempted.

Note also that the regression is **outside the scored region**. It would have been easy to omit. The reason to include it is that load time is real to whoever deploys this, and a scoring metric that does not cover something does not make it free.

**8.1 Two more reporting habits from the same PR**

**Statistics, not just means.** Five ABBA-interleaved pairs, first discarded as warm-up:

```text
   whole-expert   33.43 33.44 33.36 33.43 33.43   mean 33.418  sd 0.033
   2-D default    29.82 29.83 29.80 29.80 29.80   mean 29.810  sd 0.014

   delta −3.608 ms/token   (−10.80% latency, +12.10% throughput)
   Welch t = 226;  the arms do not overlap
                   (base min 33.36 > 2-D max 29.83)
```

The non-overlap statement is the useful one for a reader: *the worst run of the fast arm beat the best run of the slow arm.* No statistics degree required.

**Declining a flattering denominator.** The box measured `main` itself at 29.92 against a recorded frontier of 26.09:

> *quoting "+28% over frontier" would be crediting this change with a box/build difference it did not cause. The defensible number is the same-box A/B: **+12.1%**.*

And on percentages versus absolutes: an earlier revision measured the same change at 38.31 → 34.76 on an older base. *"The absolute saving is essentially unchanged at ~3.6 ms because the MoE imbalance is a **fixed cost** that #90 did not touch; the percentage grew because the rest of the token got faster."* **A fixed-cost fix earns a rising percentage as everything else improves** — which is worth understanding before you conclude your change got better.

---

</details>

## Lab — 分片 MoE（混合专家模型）layer 并论证其几何布局

1. **推导你的归约点。** 按顺序写出你的 MoE layer 的算子。找到部分和之后的第一个非线性。把集合通信放在那里，并说明代数理由。然后说明如果把它往后移一个算子会发生什么。
2. **计数并断言。** 每个 token 的集合通信次数，以及 payload 宽度——来自前向传播，而不是图。对计数加一个断言，并在 bench 输出中打印它。
3. **估算代价。** 在你的真实 payload 下做微基准测试，并在其 8× 和 64× 下也做。如果耗时几乎不变，你就是受延迟限制：报告 µs/call，并以调用次数和 barrier 为目标。
4. **计算 `E[max bin]`。** 对于跨 *N* 个 rank 的 top-*k*，用解析法或仿真。与 `k/N` 比较。该比值就是你正在付出的不均衡（§4）。
5. **找到一条确定性的轴。** 是否存在一个固定维度——FFN 宽度、head dim、hidden——你可以分片它，而不是分片专家，或与专家一起分片？两种方式都按 `E[max bin] × rows_per_expert` 计算关键路径（§4.1）。
6. **检查你的量化块约束。** 你的候选分片宽度能否拆成整数个量化块？在从中优化之前，先枚举合法宽度。
7. **编写守恒测试。** 在字节映射上重放每个 rank 的拷贝描述符；断言每个字节被覆盖 **恰好一次**。然后故意转置两个步长，并确认测试失败——字节计数测试则不会（§7）。
8. **审计你的默认值。** 对每个性能 flag，检查你的 benchmark 和生产入口点是否设置它。报告任何二者不一致的 flag（§5.1）。
9. **测量你的串行比例，两次。** 在 N=1 和 N=max 处，在你的改动前后。每次按 Amdahl 解出 `s`。如果 `s` 上升，说明下一个杠杆是什么（§3）。
10. **报告三个 KLD。** A 对参考、B 对参考，以及 A 对 B。只得出第三个所支持的结论（§6.1）。

通过标准：一个已确定的分片决策，由 `E[max bin]` 计算论证；一个你亲眼看到其失败的字节守恒测试；一次默认值审计；以及改动前后的串行比例。

---

## 自检

1. 你的专家分发先产生部分和，然后缩放，然后 RMS norm，然后矩阵乘。全规约可以放在哪里，又绝不能放在哪里？给出代数。
2. 你把归约移到 up-projection 之后。没有崩溃，文本也流畅。输出被乘以什么，为什么会出现那个特定因子？
3. 你的模型 hidden 为 7168，expert latent 为 3584。你根据 hidden 来给集合通信定尺寸。你的带宽估计错了几倍，能从时序上看出来吗？
4. Top-8 路由跨 4 个 rank。计算 `k/N` 并估计 `E[max bin]`。不均衡因子是多少，它会让同步 layer 付出什么代价？
5. 解释为什么故意恶化专家均衡（用 2 组而不是 8 组）可以缩短关键路径。给出 3072 行 FFN 的算术。
6. 一个集合通信在 14 KiB 下耗时 58.7 µs，在 7 MiB 下耗时 85 µs。你应该优化什么，应该报告什么单位？
7. 一个已合并、已测量、已验证的快速集合通信从未在任何计分轮次中运行。给出其机制，以及本可在第一天就抓住它的一行审计。
8. 你的分片测试验证每个 rank 拷贝了正确数量的字节。描述一个它抓不到的 bug，以及能抓住它的测试。
9. 对某个阶段的 3× 改进让你的 8-GPU 扩展性变差。计算改进前后的串行比例，并指出接下来该修什么。
10. 近似 A 与参考相差 4.063e-03，B 为 5.146e-03，A 与 B 相差 3.255e-03。哪个更准确，可辩护的结论是什么？

---


<details>
<summary>English original</summary>

**Lab — shard an MoE layer and defend the geometry**

1. **Derive your reduce point.** Write out your MoE layer's ops in order. Find the first non-linearity after the partial sum. Place the collective there and state the algebraic reason. Then state what would happen if you moved it one op later.
2. **Count and assert.** Collectives per token, and payload width — from the forward pass, not a diagram. Add an assertion on the count and print it in your bench output.
3. **Price it.** Microbenchmark at your real payload, and at 8× and 64× it. If time barely moves, you are latency-bound: report µs/call and target the call count and barrier.
4. **Compute `E[max bin]`.** For your top-*k* over *N* ranks, analytically or by simulation. Compare to `k/N`. That ratio is the imbalance you are paying (§4).
5. **Find a deterministic axis.** Is there a fixed dimension — FFN width, head dim, hidden — you could shard instead of or alongside experts? Compute the critical path both ways as `E[max bin] × rows_per_expert` (§4.1).
6. **Check your quant-block constraint.** Does your candidate shard width divide into whole quantization blocks? Enumerate the legal widths before optimizing among them.
7. **Write the conservation test.** Replay every rank's copy descriptor over a byte map; assert each byte is covered **exactly once**. Then deliberately transpose two strides and confirm the test fails — a byte-count test would not (§7).
8. **Audit your defaults.** For every performance flag, check whether your benchmark and your production entry point set it. Report any flag where they differ (§5.1).
9. **Measure your serial fraction, twice.** At N=1 and N=max, before and after your change. Solve Amdahl for `s` each time. If `s` rose, say what the next lever is (§3).
10. **Report the three KLDs.** A-to-reference, B-to-reference, and A-to-B. Conclude only what the third supports (§6.1).

Pass criterion: a committed sharding decision justified by an `E[max bin]` calculation, a byte-conservation test you have watched fail, a defaults audit, and before/after serial fractions.

---

**Self-check**

1. Your expert dispatch produces a partial sum, then a scale, then an RMS norm, then a matmul. Where can the all-reduce go, and where must it not? Give the algebra.
2. You move the reduce past the up-projection. Nothing crashes and the text is fluent. What is the output multiplied by, and why does that specific factor appear?
3. Your model has hidden 7168 and expert latent 3584. You size the collective off hidden. By what factor is your bandwidth estimate wrong, and would you notice from the timing?
4. Top-8 routing over 4 ranks. Compute `k/N` and estimate `E[max bin]`. What is the imbalance factor, and what does it cost a synchronous layer?
5. Explain why deliberately worsening expert balance (2 groups instead of 8) can shorten the critical path. Give the arithmetic with a 3072-row FFN.
6. A collective takes 58.7 µs at 14 KiB and 85 µs at 7 MiB. What should you optimize, and what unit should you report?
7. A merged, measured, validated fast collective never ran in any scored round. Give the mechanism and the one-line audit that would have caught it on day one.
8. Your shard test verifies each rank copies the correct number of bytes. Describe a bug it cannot catch, and the test that can.
9. A 3× improvement to a phase makes your 8-GPU scaling worse. Compute the serial fraction before and after, and name what to fix next.
10. Approximation A is 4.063e-03 from the reference, B is 5.146e-03, and A-to-B is 3.255e-03. Which is more accurate, and what is the defensible claim?

---

</details>

## References

* **The PRs** — [#59](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/59)（f32 peer one-shot，Mode B，shared-expert banding）、[#74](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/74)（make unset 表示 auto）、[#96](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96)（2-D MoE 分片；balls-in-bins 论证；字节守恒测试；已披露的负载回归），外加 [#107](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107)（one-rendezvous all-reduce）。[`docs/tensor-parallel.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/docs/tensor-parallel.md) 是 §1–§3 的来源。
* **Megatron-LM tensor parallelism** — [arXiv:1909.08053](https://arxiv.org/abs/1909.08053) — §4.2 所依赖的 row-then-col 组合。
* **vLLM custom all-reduce** — [github.com/vllm-project/vllm](https://github.com/vllm-project/vllm)、`csrc/custom_all_reduce.cuh`（Apache-2.0）——§6 的 peer one-shot 所改编的 `Signal` 布局、双计数器 barrier 与 packed FP32 reduce，署名保留在仓库的 `NOTICE` 中。
* **NCCL** — [docs.nvidia.com/deeplearning/nccl](https://docs.nvidia.com/deeplearning/nccl/) — ring 与 tree all-reduce，以及自定义 kernel 胜出的小消息区间。
* **NVLink SHARP / multimem** — [NVIDIA NVLink Switch](https://www.nvidia.com/en-us/data-center/nvlink/) — 网络内归约；§5.1 刻意排除的后端。
* **Balls into bins / maximum load** — Raab & Steger，*"Balls into Bins — A Simple and Tight Analysis"*（[RANDOM 1998](https://link.springer.com/chapter/10.1007/3-540-49543-6_13)）——§4 背后的最大负载结论。
* **DeepEP** — [github.com/deepseek-ai/DeepEP](https://github.com/deepseek-ai/DeepEP) — 簇规模下的专家并行通信；§4.1 的多节点对应物。

Cross-references:

* [Part 2 Lecture 04 — Tensor parallelism on 8× Hopper](../02-Hopper稠密计算/Lecture-04.md) — row/col 分片，以及 §1.2 所纠正的「每 layer 两次 all-reduce」数字。
* [Part 2 Lecture 07 — Inside the communication layer](../02-Hopper稠密计算/Lecture-07.md) — §6 所实现的小消息自定义 all-reduce。
* [Part 3 Lecture 03 — Expert parallelism and the gating hot path](../03-Blackwell-MoE/Lecture-03.md) — 针对同一不均衡在簇规模下的 all-to-all 方案。
* [Lecture 03 §6 — Diagnosis](Lecture-03.md) — 处于诊断语境中的 Amdahl 表。
* [Lecture 06 §5 — Head-shard the bands](Lecture-06.md) — §3 串行项的修复。
* [Lecture 10 §4.2–4.3 — Silently wrong](Lecture-10.md) — 作为一类故障的分区与集合通信放置 bug。

---

## Current as of 2026-08

8× H200 SXM、`sm_90`、CUDA 12.8+、NCCL、UD-IQ1_S、tp=8、计分上下文 131,072。896 experts / top-16 / expert latent 3584 / expert FFN 3072；`tp_size ≥ 4` 处默认采用 2-D 分片，配合 `eg=2`（每 rank 448 experts × 768 FFN rows，768 = 3 × 256 量化块）。attention head 分片时每 token 185 次集合通信；replicated 时 92 次。reduce 放置代数、`E[max bin]` 论证、默认值审计与字节守恒测试是持久内容。

---

## Next

* Next: [Lecture 08 — Graph-resident decode: killing the launch bill for good](Lecture-08.md)
* Previous: [Lecture 06 — Attention at 128k: split over context, split over heads](Lecture-06.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)


<details>
<summary>English original</summary>

**References**

* **The PRs** — [#59](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/59) (f32 peer one-shot, Mode B, shared-expert banding), [#74](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/74) (make unset mean auto), [#96](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96) (2-D MoE sharding; the balls-in-bins argument; the byte-conservation test; the disclosed load regression), plus [#107](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107) (one-rendezvous all-reduce). [`docs/tensor-parallel.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/docs/tensor-parallel.md) is the source for §1–§3.
* **Megatron-LM tensor parallelism** — [arXiv:1909.08053](https://arxiv.org/abs/1909.08053) — the row-then-col composition §4.2 relies on.
* **vLLM custom all-reduce** — [github.com/vllm-project/vllm](https://github.com/vllm-project/vllm), `csrc/custom_all_reduce.cuh` (Apache-2.0) — the `Signal` layout, dual-counter barrier and packed FP32 reduce that §6's peer one-shot adapts, with attribution retained in the repo's `NOTICE`.
* **NCCL** — [docs.nvidia.com/deeplearning/nccl](https://docs.nvidia.com/deeplearning/nccl/) — ring and tree all-reduce, and the small-message regime where a custom kernel wins.
* **NVLink SHARP / multimem** — [NVIDIA NVLink Switch](https://www.nvidia.com/en-us/data-center/nvlink/) — in-network reduction; §5.1's deliberately-excluded backend.
* **Balls into bins / maximum load** — Raab & Steger, *"Balls into Bins — A Simple and Tight Analysis"* ([RANDOM 1998](https://link.springer.com/chapter/10.1007/3-540-49543-6_13)) — the maximum-load result behind §4.
* **DeepEP** — [github.com/deepseek-ai/DeepEP](https://github.com/deepseek-ai/DeepEP) — expert-parallel communication at cluster scale; the multi-node relative of §4.1.

Cross-references:

* [Part 2 Lecture 04 — Tensor parallelism on 8× Hopper](../02-Hopper稠密计算/Lecture-04.md) — row/col sharding and the "two all-reduces per layer" figure §1.2 corrects.
* [Part 2 Lecture 07 — Inside the communication layer](../02-Hopper稠密计算/Lecture-07.md) — the small-message custom all-reduce §6 implements.
* [Part 3 Lecture 03 — Expert parallelism and the gating hot path](../03-Blackwell-MoE/Lecture-03.md) — the all-to-all answer to the same imbalance, at cluster scale.
* [Lecture 03 §6 — Diagnosis](Lecture-03.md) — the Amdahl table in its diagnostic context.
* [Lecture 06 §5 — Head-shard the bands](Lecture-06.md) — the fix for §3's serial term.
* [Lecture 10 §4.2–4.3 — Silently wrong](Lecture-10.md) — partition and collective-placement bugs as a failure family.

---

**Current as of 2026-08**

8× H200 SXM, `sm_90`, CUDA 12.8+, NCCL, UD-IQ1_S, tp=8, scored context 131,072. 896 experts / top-16 / expert latent 3584 / expert FFN 3072; 2-D sharding default at `tp_size ≥ 4` with `eg=2` (448 experts × 768 FFN rows per rank, 768 = 3 × 256 quant blocks). 185 collectives/token with attention head-sharded; 92 with it replicated. The reduce-placement algebra, the `E[max bin]` argument, the defaults audit, and byte-conservation testing are the durable content.

---

**Next**

* Next: [Lecture 08 — Graph-resident decode: killing the launch bill for good](Lecture-08.md)
* Previous: [Lecture 06 — Attention at 128k: split over context, split over heads](Lecture-06.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
