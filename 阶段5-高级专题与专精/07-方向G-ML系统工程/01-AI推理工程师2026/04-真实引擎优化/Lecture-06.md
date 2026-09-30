---
title: Part 4 · Lecture 06 — 128k 下的 attention：按上下文拆分，按 head 拆分
description: Part 4 · Lecture 06 — 128k 下的 attention：按上下文拆分，按 head 拆分
---

# Part 4 · Lecture 06 — 128k 下的 attention：按上下文拆分，按 head 拆分

## 概览

18× 的差距就在这里。

在上下文 64 时，引擎落后参考实现 1.8×。在 131,072 时，它 **落后 18×**——1.01 tok/s 对 18.44——而参考实现的速率在整个区间上基本 *持平*。所有这些发散都来自一个 kernel：`attn_mla` 在 128k 下消耗了 **decode（逐 token 生成阶段）的 69.2%**，而且其结构决定了它无法随深度扩展。

三个 pull request 将它从 1.01 提升到 15.93 tok/s——**15.8×**——而 attention 的数学没有任何改变。每个都找到了不同的并行化轴：

```text
   #49   split over CONTEXT   →  990.98 → 220.94 ms/token   4.49×
   #57   batch HEADS per block →  220.5  → 110.4  ms/token   2.00×
   #63   shard heads over RANKS → 108.4  →  62.8  ms/token   1.73×
```

到结尾时，你应能从 online-softmax 递推推导出 split-attention 的合并，依据寄存器压力而非扫参来选择分块宽度，并且——区分本案例研究的那部分——*测试并测量一条你的正确性门禁在结构上无法触达的代码路径。*

---

## 1. 无法随深度扩展的形态

原始 `mla_decode_attn_kernel` 为 **96 个 head 中的每一个分配了单个线程块，串行遍历全部 131,072 个 token。**

```text
   grid = n_head = 96 blocks    on a 132-SM H200
                                → 36 SMs receive NO BLOCK AT ALL
   96 blocks × 256 threads      = 24,576 threads
   each block walks 131,072 KV positions, serially

   time per layer ∝ n_ctx,  with 27% of the GPU idle throughout
```

两个独立缺陷叠加。**Occupancy**：96 个 block 放在 132 个 SM 上，而 96 的任何排布都填不满 132。**串行深度**：每个 block 的 runtime 都随上下文线性增长，因为序列维度是被串行走过而非并行化。

参考引擎没有这个问题，因为它保留了 *压缩的* MLA 缓存（`kv_lora` 512，f16），以及一个在深度上并行化的 kernel。因此曲线持平。这正是 [Lecture 03 §5](Lecture-03.md) 说要寻找的那种发散：**你的候选实现的成本会沿着参考实现不会增长的那个维度增长。**

### 1.1 为这个 bug 命名的数字

[**PR #57**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/57) 用一句话点出了底层浪费，而且这是我见过的对 decode 阶段 MLA 的最佳表述：

> *MLA **就是** MQA——96 个 query head 在同一个共享 latent KV cache 上做 attention——但 decode 时每个 head 都有自己的 block，所以在 128k 下 **每一层都把同一个 302 MB 缓存流式读取了 96 次。**"*

MLA 将 KV 压缩为每个位置一个共享 latent（[Part 3 Lecture 01](../03-Blackwell-MoE/Lecture-01.md) 的 §1）。这种压缩正是该架构的全部卖点——而每个 head 一个 block 的 kernel 把它丢掉了，因为 96 个 block 各自独立读取整个共享缓存。架构提供了复用；启动几何却拒绝了它。

以及来自 `nsys` 的支撑算术：

```text
   mla_decode_attn_split   56.8%  = 124 ms   (5.165 ms × 24 MLA layers)
   proj_q8_0_multirow      17.0%
   proj_q8_0_fused4         8.9%

   main at ctx 64 is 98.7 ms/token
   ⇒ ~124 ms of the 221 ms token IS the cache walk.
```

从长上下文的 token 时间中减去短上下文的 token 时间，就分离出依赖深度的项。这是一种只需两次测量的诊断，不需要性能分析器，而且当曲线出现斜率时，它应当是你做的第一件事。


<details>
<summary>English original</summary>

**Part 4 · Lecture 06 — Attention at 128k: Split Over Context, Split Over Heads**

**Overview**

This is where the 18× gap lived.

At context 64 the engine was 1.8× behind the reference. At 131,072 it was **18× behind** — 1.01 tok/s against 18.44 — and the reference's rate was essentially *flat* across that whole range. All of that divergence was one kernel: `attn_mla` consumed **69.2% of decode at 128k**, and it was structured in a way that could not scale with depth.

Three pull requests took it from 1.01 to 15.93 tok/s — **15.8×** — with no change to the mathematics of attention. Each one found a different axis to parallelize:

```text
   #49   split over CONTEXT   →  990.98 → 220.94 ms/token   4.49×
   #57   batch HEADS per block →  220.5  → 110.4  ms/token   2.00×
   #63   shard heads over RANKS → 108.4  →  62.8  ms/token   1.73×
```

By the end you should be able to derive the split-attention combine from the online-softmax recurrence, choose a tile width from register pressure rather than by sweeping, and — the part that distinguishes this case study — *test and measure a code path your correctness gate structurally cannot reach.*

---

**1. The shape that cannot scale with depth**

The original `mla_decode_attn_kernel` gave **each of 96 heads a single thread block that walked all 131,072 tokens serially.**

```text
   grid = n_head = 96 blocks    on a 132-SM H200
                                → 36 SMs receive NO BLOCK AT ALL
   96 blocks × 256 threads      = 24,576 threads
   each block walks 131,072 KV positions, serially

   time per layer ∝ n_ctx,  with 27% of the GPU idle throughout
```

Two independent defects stacked. **Occupancy**: 96 blocks on 132 SMs, and no arrangement of 96 fills 132. **Serial depth**: every block's runtime grows linearly with context, because the sequence dimension is walked rather than parallelized.

The reference engine did not have this problem because it kept a *compressed* MLA cache (`kv_lora` 512, f16) and a kernel that parallelizes over depth. Hence the flat curve. This is precisely the divergence [Lecture 03 §5](Lecture-03.md) says to look for: **your candidate's cost grows with a dimension the reference's does not.**

**1.1 The number that names the bug**

[**PR #57**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/57) states the underlying waste in one sentence, and it is the best framing of MLA-at-decode I have seen:

> *MLA **is** MQA — 96 query heads attend over one shared latent KV cache — but decode gave each head its own block, so at 128k **every layer streamed the same 302 MB cache 96 times.**"*

MLA compresses KV into a single shared latent per position (§1 of [Part 3 Lecture 01](../03-Blackwell-MoE/Lecture-01.md)). That compression is the architecture's whole selling point — and a kernel with one block per head throws it away, because 96 blocks each read the entire shared cache independently. The architecture provided the reuse; the launch geometry declined it.

And the supporting arithmetic, from `nsys`:

```text
   mla_decode_attn_split   56.8%  = 124 ms   (5.165 ms × 24 MLA layers)
   proj_q8_0_multirow      17.0%
   proj_q8_0_fused4         8.9%

   main at ctx 64 is 98.7 ms/token
   ⇒ ~124 ms of the 221 ms token IS the cache walk.
```

Subtracting the short-context token time from the long-context token time isolates the depth-dependent term. That is a two-measurement diagnosis requiring no profiler, and it should be the first thing you do when a curve slopes.

---

</details>

## 2. 沿上下文拆分

[**PR #49**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) — *“沿上下文拆分 MLA decode（128k 时 4.49×）+ launch 失败时让 forward 失败。”*

这一招就是 **Flash-Decoding**：从 decode（逐 token 生成阶段）时唯一够大的那个维度 —— KV cache 本身 —— 制造并行度。

```text
   grid:  n_head   →   n_head × splits
```

每个 block 对上下文的一段**连续**切片执行同样的 online softmax，并写出一个部分结果 `(m, l, latent)`。一个 combine kernel 把它们合并起来：

```text
   m   = max_i  m_i
   l   = Σ_i    l_i  · exp(m_i − m)
   acc = Σ_i    acc_i · exp(m_i − m)
```

这就是 FlashAttention 的 online-softmax 重缩放，只不过作用在*跨 block* 上，而不是 block 内的跨分块。每个部分结果自带自己的 running max `m_i` 和归一化因子 `l_i`；合并时先把每个部分结果重缩放到共同的 max，再求和。除浮点重结合带来的差异外，它是精确的。

该 PR 中有六个实现决策，每一个都值得点名，因为每一个都堵住了一类真实的失败：

**用连续切片，不用跨步切片。** *“用连续而非跨步，好让每个 block 遍历 cache 时对预取器保持顺序访问。”* 跨步分配能带来完美的负载均衡，却会毁掉局部性。在批大小为 1、cache 为 302 MB 时，局部性胜出。

**输出投影挪进 combine。** `wv_b` 需要*合并后*的 latent，所以不能按切片分别跑。判断哪个下游算子是合并结果的函数 —— 而不是每个部分结果的函数 —— 正是 split-K 里最容易做错的一环。

**低于阈值就不拆分。** 只在高于 `kMlaSplitMinCtx`（4096）时才拆：*“低于它时，combine pass 和额外的 global 往返开销超过所换来的并行度，而不拆分的 kernel 仍是数值测试所锚定的那一个。”* 拆分多出一次 kernel 启动和一次经 global memory 的往返。在短上下文下，这些开销超过所获得的并行度。

**每设备独立的 scratch，而非一份全局分配。** 它防止的这个 bug 值得全文引用，因为这是一个*张量并行*的陷阱，而不是 attention 的陷阱：*“一个静态指针由最先到达的 rank 分配，随后被另外七个 rank 在并不属于它的设备上解引用。”* 八个 rank，八块设备，一个 `static` 指针 —— 却有七个 rank 在读属于另一块设备的内存。

**空切片必须是中性的，而不是 `NaN`。** 当 `n_ctx < splits` 时，有些切片没有工作可做。它们把 `l = 0, m = -1e30` 设为特定值，使其 `exp(m_i − m)` 贡献为零。若未初始化，`exp(-inf - -inf)` 会给出 `NaN`，而合并中的一个 `NaN` 就会毒化整个 token。

**shared memory 是 `O(key_length + kv_lora + tile)`，绝不能是 `O(n_ctx)`。** 这一点很关键，因为 [PR #33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33) 刚修掉一个 kernel *“按上下文长度分配动态 shared memory”* 的 bug —— 这正是*“MLA decode attention 在上下文超过约 11.7k 后静默地停止启动”*的原因。每个 block 的 shared memory 有硬上限；任何随上下文增长的分配，都存在一个会让启动失败的上下文长度。参见 §5.3 和 [Lecture 10 §4.4](Lecture-10.md)。

### 2.1 拆分反而能*提升*准确率

拆分路径是刻意**不**保证逐位一致的 —— 对每切片的部分结果求和会重结合各项。在 128k、全部 8 个 rank 上实测：

```text
   mean KLD         1.333795e-10     (threshold < 1e-5)
   top-1 agreement  100.0000 %
   top-5 overlap    100.0000 %
   RMS dp           0.000001 %

   base argmax 378 @ 10.563942
   PR   argmax 378 @ 10.563975
        "a 5th-decimal difference, the signature of summation
         reassociation, five orders of magnitude inside tolerance"
```

然后是令人愉快的意外：

> *它在 n_ctx=20000 时恰好**准确了约 4×**（relL2 3.734e-07 对 1.607e-06），因为**按切片的部分结果实际上就是成对求和**。*

把 131,072 项顺序累加进一个 f32 累加器，舍入误差大致按 `O(n)` 累积。把它们拆成 64 个独立的部分结果求和再合并，是一个两级树 —— 误差项会降到趋近 `O(log n)`。**为并行而拆分，顺带白得成对求和。**

> **当你拆分一个长归约时，预期准确率会提升，而不是下降。** 如果它下降了，说明你的 combine 写错了。这是对任何 split-K 实现都有用的 sanity check，它也颠覆了“非逐位一致”就等于“更差”的直觉。

---

## 3. #49 中的测量功夫

这个 PR 里的三项技巧，价值超过那项优化本身。


<details>
<summary>English original</summary>

**2. Split over context**

[**PR #49**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) — *"split MLA decode over context (4.49× at 128k) + fail the forward on a failed launch."*

The move is **Flash-Decoding**: manufacture parallelism from the one dimension that is large at decode — the KV cache itself.

```text
   grid:  n_head   →   n_head × splits
```

Each block runs the same online softmax over a **contiguous** slice of the context and writes a partial `(m, l, latent)`. A combine kernel merges them:

```text
   m   = max_i  m_i
   l   = Σ_i    l_i  · exp(m_i − m)
   acc = Σ_i    acc_i · exp(m_i − m)
```

This is the online-softmax rescaling of FlashAttention, applied *across blocks* rather than across tiles within a block. Each partial carries its own running max `m_i` and normalizer `l_i`; the merge rescales every partial to a common maximum before summing. It is exact up to floating-point reassociation.

Six implementation decisions in that PR, each worth naming because each closes a real failure:

**Contiguous slices, not strided.** *"Contiguous rather than strided so each block's cache walk stays sequential for the prefetcher."* Strided assignment would give perfect load balance and destroy locality. At batch 1 over a 302 MB cache, locality wins.

**The output projection moves into the combine.** `wv_b` needs the *merged* latent, so it cannot run per-slice. Recognizing which downstream op is a function of the combined result — rather than of each partial — is the part of split-K that is easy to get wrong.

**Gate the split below a threshold.** Split only above `kMlaSplitMinCtx` (4096): *"below it the combine pass and extra global round trip cost more than the parallelism buys, and the un-split kernel stays the one the numeric test pins."* A split adds a kernel launch and a round trip through global memory. At short context those exceed the parallelism gained.

**Per-device scratch, not one global allocation.** The bug this prevents is worth quoting in full because it is a *tensor-parallel* trap, not an attention one: *"a single static pointer is allocated by whichever rank arrives first and then dereferenced by the other seven on devices it does not belong to."* Eight ranks, eight devices, one `static` pointer — and seven ranks reading memory that belongs to another device.

**Empty slices must be neutral, not `NaN`.** When `n_ctx < splits`, some slices have no work. They set `l = 0, m = -1e30` so their `exp(m_i − m)` contributes zero. Left uninitialized, `exp(-inf - -inf)` gives `NaN`, and one `NaN` in the merge poisons the token.

**Shared memory is `O(key_length + kv_lora + tile)`, never `O(n_ctx)`.** This matters because [PR #33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33) had just fixed a bug where the kernel *"sized dynamic shared memory by context length"* — which is why *"MLA decode attention silently stops launching past ~11.7k context."* Shared memory per block has a hard cap; any allocation that scales with context has a context at which the launch fails. See §5.3 and [Lecture 10 §4.4](Lecture-10.md).

**2.1 Splitting can *improve* accuracy**

The split path is deliberately **not** bit-identical — summing per-slice partials reassociates the terms. Measured at 128k across all 8 ranks:

```text
   mean KLD         1.333795e-10     (threshold < 1e-5)
   top-1 agreement  100.0000 %
   top-5 overlap    100.0000 %
   RMS dp           0.000001 %

   base argmax 378 @ 10.563942
   PR   argmax 378 @ 10.563975
        "a 5th-decimal difference, the signature of summation
         reassociation, five orders of magnitude inside tolerance"
```

And then the pleasant surprise:

> *It happens to land **~4× more accurate** at n_ctx=20000 (relL2 3.734e-07 vs 1.607e-06), because **per-slice partials are effectively pairwise summation**.*

Summing 131,072 terms sequentially into one f32 accumulator accumulates rounding error roughly as `O(n)`. Summing them in 64 independent partials and then combining is a two-level tree — the error term drops toward `O(log n)`. **Splitting for parallelism gives you pairwise summation for free.**

> **When you split a long reduction, expect accuracy to improve, not degrade.** If it degrades, your combine is wrong. This is a useful sanity check on any split-K implementation, and it inverts the instinct that "not bit-identical" means "worse."

---

**3. The measurement craft in #49**

Three techniques from this PR that are worth more than the optimization.

</details>

### 3.1 一个对照阶段

该 profile 呈现出一个 PR *并未触及*的阶段：

| 阶段 | 之前 | 之后 | |
|---|--:|--:|---|
| `attn_mla` | 64912.89 ms (69.2%) | 9607.90 ms (28.8%) | **减少 6.76×** |
| `ffn_moe` | 23378.38 ms (24.9%) | 20993.16 ms (63.0%) | ~10%（来自另一个 PR） |
| `attn_kda` | 2655.21 ms (2.8%) | 2657.01 ms (8.0%) | **2655 → 2657：未变** |
| 总计 | 93740.70 ms | 33333.80 ms | |

> *"**`attn_kda` 是对照。** 本 PR 不触及它，而在一组独立的构建与运行中它只变动了 0.07%。**一个阶段下降 6.8×，而它的邻居纹丝不动，这正是把真实效果与 harness 假象区分开的东西。**"*

如果这次改进源自重新构建、驱动变更或机器漂移，那么*所有*阶段都会变动。一个相邻阶段在独立的构建与运行间复现到 0.07%，这就确立了 harness 是稳定的，且效果是局部的。

> **直接抄走。** 每一次 profile 对比都应点名一个你未触及的阶段并把它报告出来。这不花什么成本，却是「这个阶段变快了」和「这次运行变快了」之间的差别。

还要注意表中关于百分比所显示的东西：`ffn_moe` 从占 decode（逐 token 生成阶段）的 24.9% 变成 **63.0%**，而绝对时间却*快了*约 10%。百分比是相对一个不断缩小的总量而言的比值。**永远要把绝对时间与占比一起报告**，否则你会把一项固定成本读成一次性能回退。

### 3.2 用被门控关闭的路径测量你的噪声底

上下文扫描包含 PR 无法影响的若干行，作者把它们当作一件测量仪器来用：

```text
   ctx 128    97.58 → 95.46 ms    +2.2%
   ctx 4k    124.07 → 122.68 ms   +1.1%
   ctx 8k    151.72 → 124.02 ms   +22%    ← the split first pays
   ctx 128k  990.98 → 220.94 ms   +349%
```

> *"把 128 和 4k 这两行读作中性，而不是读作胜利。低于 `kMlaSplitMinCtx` 时 split 路径被门控关闭，**两个构建运行的是同一份代码**，因此 ctx 128 处的 2.2% 是对本节点逐次运行波动的直接测量。这把噪声底定在接近 2%，也就使得 4k 行（+1.1%）**不构成结果。** 值得计分的结论是 8k 和 128k，它们分别是该底的 9× 和 200×。"*

这是一个确实优雅的技巧。一条被门控关闭的代码路径给你一个**嵌入在 A/B test 内部的 A/A test**：同样的二进制、同样的命令、同样的机器、完全相同的代码，因此任何差异都是纯粹的测量噪声。你在产生结果的那同一次运行中、在同一硬件上、在同一时刻，得到你的噪声底。

随后，同一套纪律被*反过来*用在作者自己的改动上。launch guard 的代价测得为 141.35 → 142.54 ms，约 0.8%——低于 2% 的底：

> *"所以诚实的说法是「无可测代价」，而不是「0.8% 的代价」。"*

小于噪声底的数字不是一个小数字。它*不是一个数字*。把它报成 0.8% 会夸大该测量的精度，而且是在让作者显得严谨的那个方向上夸大——这仍然是夸大。

### 3.3 工具不可用时如何推理

`ncu` 在该节点上不可用（`ERR_NVGPUCTRPERM`——经典的、被锁死的性能计数器权限）。因此作者无法直接区分关于 691.6 ms 的两种假设：

```text
   H1  bandwidth-bound:  blocks drift apart and re-read DRAM
   H2  latency-bound:    blocks stay in lockstep, L2 absorbs the reuse

   → "I targeted the deficiency BOTH models agree on:
      ~9% occupancy with 36 SMs idle."

   → the 4.49× result settles it:
      "a bandwidth-bound kernel could not respond to pure
       parallelism like this."
```

两步走：**先做那些竞争假设都同意会有帮助的事**，然后**让结果在它们之间做出判别。** 一个受带宽限制的 kernel 不会因为更多的 block 就快 4.5×——字节就是字节。仅靠并行度就得到 4.5×，这本身就是该 kernel 受延迟和 occupancy 限制的证据。

> **缺少性能分析器不等于调查被卡住。** 找出每一个候选解释都预测会有帮助的那个干预手段，去做，然后用响应的幅度来识别哪个解释是对的。

---


<details>
<summary>English original</summary>

**3.1 A control phase**

The profile is presented with a phase that the PR *does not touch*:

| phase | before | after | |
|---|--:|--:|---|
| `attn_mla` | 64912.89 ms (69.2%) | 9607.90 ms (28.8%) | **6.76× less** |
| `ffn_moe` | 23378.38 ms (24.9%) | 20993.16 ms (63.0%) | ~10% (from another PR) |
| `attn_kda` | 2655.21 ms (2.8%) | 2657.01 ms (8.0%) | **2655 → 2657: unchanged** |
| total | 93740.70 ms | 33333.80 ms | |

> *"**`attn_kda` is the control.** This PR does not touch it, and across a separate build and run it moved by 0.07%. **One phase falling 6.8× while its neighbour sits still is what distinguishes a real effect from a harness artefact.**"*

If a rebuild, a driver change, or box drift were responsible for the improvement, *everything* would move. A neighbouring phase that reproduces to 0.07% across separate builds and runs establishes that the harness is stable and the effect is localized.

> **Steal this.** Every profile comparison should name a phase you did not touch and report it. It costs nothing and it is the difference between "this phase got faster" and "this run was faster."

Note also what the table shows about percentages: `ffn_moe` went from 24.9% to **63.0%** of decode while getting ~10% *faster* in absolute terms. Percentages are ratios against a shrinking total. **Always report absolute times alongside shares**, or you will read a fixed cost as a regression.

**3.2 Measure your noise floor with a gated-off path**

The context sweep includes rows the PR cannot affect, and the author uses them as an instrument:

```text
   ctx 128    97.58 → 95.46 ms    +2.2%
   ctx 4k    124.07 → 122.68 ms   +1.1%
   ctx 8k    151.72 → 124.02 ms   +22%    ← the split first pays
   ctx 128k  990.98 → 220.94 ms   +349%
```

> *"Read the 128 and 4k rows as neutral, not as wins. Below `kMlaSplitMinCtx` the split path is gated off and **both builds run the same code**, so the 2.2% at ctx 128 is a direct measurement of this node's run-to-run spread. That puts the noise floor near 2%, which makes the 4k row (+1.1%) **not a result.** The claims worth scoring are 8k and 128k, which are 9× and 200× that floor."*

This is a genuinely elegant trick. A gated-off code path gives you an **A/A test embedded inside your A/B test**: same binary, same command, same box, identical code, so any difference is pure measurement noise. You get your noise floor from the same run that produces your result, on the same hardware, at the same moment.

Then the same discipline is applied *against* the author's own change. The launch guard's cost measured 141.35 → 142.54 ms, ~0.8% — below the 2% floor:

> *"so the honest claim is 'no measurable cost', not '0.8% cost'."*

A number smaller than your noise floor is not a small number. It is *no number*. Reporting it as 0.8% would overstate the precision of the measurement in the direction that makes the author look careful — which is still overstating it.

**3.3 Reasoning when the tool is unavailable**

`ncu` was not available on the node (`ERR_NVGPUCTRPERM` — the classic locked-down performance-counter permission). So the author could not directly distinguish two hypotheses for the 691.6 ms:

```text
   H1  bandwidth-bound:  blocks drift apart and re-read DRAM
   H2  latency-bound:    blocks stay in lockstep, L2 absorbs the reuse

   → "I targeted the deficiency BOTH models agree on:
      ~9% occupancy with 36 SMs idle."

   → the 4.49× result settles it:
      "a bandwidth-bound kernel could not respond to pure
       parallelism like this."
```

Two moves: **act on what competing hypotheses agree about**, and **let the outcome discriminate between them.** A bandwidth-limited kernel does not get 4.5× faster from more blocks — the bytes are the bytes. Getting 4.5× from parallelism alone is itself the evidence that the kernel was latency- and occupancy-limited.

> **A missing profiler is not a blocked investigation.** Find the intervention every candidate explanation predicts will help, do it, and use the magnitude of the response to identify which explanation was right.

---

</details>

## 4. 每 block 的 batch heads，并在寄存器墙前停下

[**PR #57**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/57) 攻的是*另一半*：302 MB 缓存每层被流式读取 96 次。按 **每 block 12 个 head** 做批处理，意味着对缓存的一次遍历可服务 12 个 head。

这与 [Lecture 05 §2.3](Lecture-05.md) 里的 reuse-versus-occupancy 张力相同，而该 PR 用整个案例研究中最清晰的数据解决了它：

| heads/block | ms/token @ 128k | registers | blocks/SM |
|---|--:|--:|--:|
| 8 | 141.1 | 96 | 2 |
| **12** | **133.9** | 125 | 2 |
| 16 | 149.9 | 148 | **1** |

> *为什么每 block 是 12 个 head 而不是更多：16 再次**把流量减半，却输给了 occupancy**。*

每 block 16 个 head 每层读缓存 6 次而非 8 次——内存流量严格更少——却**慢了 12%**，因为每线程 148 个寄存器意味着每个 SM 只驻留**一个 block**，而非两个。每个 SM 只有一个 block 时，就没有东西可与它的内存延迟重叠。

这就是寄存器压力悬崖，而且它是*阶跃函数*，不是渐变。Occupancy 是 `floor(registers_per_SM / registers_per_block)`；跨过一个阈值会一步把驻留 block 数减半。扫过 8 → 12 → 16 并把寄存器数与 timing 一并报告，才让这一机制变得可见而非神秘。

> **照搬这一条。** 调 tile 或 batch 宽度时，在每个 timing 旁边报告**寄存器数量和 blocks-per-SM**。最优点几乎总是停留在当前 occupancy 阶跃同一侧的最大 tile——而且你可以从 `-Xptxas -v` 算出阶跃在哪，而不必靠扫参去发现它。

### 4.1 对叠加变更做归因

#57 打包了若干变更，并公布了增量归因，在同一台机器上测得、交错进行：

```text
   main                                                        221.4 ms
   + MLA heads batched 12/block, 4 tokens per staged-q pass    133.9
   + attn_res_mix device-wide, KDA state staged, slices = SMs   125.6
   + Q8_0 projections ROWS 16/8/4, fused4 2 → 4                 110.4
```

单一个「220 → 110，2×」的标题是不可评审的。这张表说明了哪项变更换来了什么，评审者可以就其中任何一项提出质疑，后来的工程师也能在回退某一项时不用猜。当你不得不打包时——[PR #49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) 解释了 one-open-PR 规则有时会迫使你这么做——那就公布这个阶梯。

---

## 5. 把 heads 分片到各 rank

[**PR #63**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) —— *"head-shard both attention bands, budget the split cap, default the Q8 projection path."* **128k 下 108.4 → 62.8 ms/token：9.23 → 15.93 tok/s，+72.6%。**

观察到的问题：attention 在**全部八个 rank 上冗余运行。**分片策略把 896 个 expert 分带（band）到各 rank，但其余一切都复制，于是全部 8 个 GPU 都计算了全部 96 个 head，扔掉了 7/8 的工作。对 24 个 MLA layer *和* 69 个 KDA layer 做 head 分片，使每个 rank 只计算自己的 head 带，代价是每层多一次 all-reduce（此配置下每 token 185 次集合通信，原来为 92 次）。

这正是 [Lecture 03 §6](Lecture-03.md) 的 Amdahl 故事得到答案之处：复制的 attention 就是那个串行项，而 head 分片把它并行化了。[Lecture 07](Lecture-07.md) 覆盖集合通信这一侧。

### 5.1 分片既消除了工作，*也*消除了并行度

#63 清单里的第四项是本讲中最具迁移价值的一句话：

> *在 `n_head × splits` 上为 MLA split cap 做预算，因为 head 分片在 132-SM 的芯片上**把 attention 网格塌缩到 64 个 block**。值 **−8%**，且容易被忽略：**head 分片既消除了工作，*也*消除了并行度。**"*

Head 分片把 `n_head` 除以 `tp_size`。Attention 网格是 `n_head × splits`。所以这个既把每 rank 工作量削减 8× 的变更，也把网格削减了 8×——直接退回到 [Lecture 04 §1](Lecture-04.md) 的 starvation。作者预见到了这一点，把 split cap 的预算做在*乘积* `n_head × splits` 上而不是仅 `splits` 上，从而把那 8% 找补了回来。

```text
   BEFORE shard:  96 heads × S splits   →  plenty of blocks
   AFTER  shard:  12 heads × S splits   →  64 blocks. starved.
   FIX: budget the cap on the PRODUCT, so S grows as n_head shrinks.
```

> **任何会除一个并行轴的变更，都必须重新推导由它算出的每一个网格。**不是「检查你改的那个 kernel」——而是重新推导那个*预算*，让常量自适应，而不是需要被重新调优。然后，如 [Lecture 04 §5](Lecture-04.md) 所示，审计同族 kernel：同一个分片把 KDA decode 网格塌缩到 12 个 block，而这一点直到隔着三个函数的 [PR #77](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77) 才被发现。


<details>
<summary>English original</summary>

**4. Batch heads per block, and stop at the register wall**

[**PR #57**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/57) attacks the *other* half: the 302 MB cache being streamed 96 times per layer. Batching **12 heads per block** means one pass over the cache serves 12 heads.

This is the same reuse-versus-occupancy tension as [Lecture 05 §2.3](Lecture-05.md), and the PR resolves it with the clearest data in the entire case study:

| heads/block | ms/token @ 128k | registers | blocks/SM |
|---|--:|--:|--:|
| 8 | 141.1 | 96 | 2 |
| **12** | **133.9** | 125 | 2 |
| 16 | 149.9 | 148 | **1** |

> *why 12 heads per block and not more: 16 **halves traffic again and loses to occupancy**.*

Sixteen heads per block reads the cache 6 times per layer instead of 8 — strictly less memory traffic — and is **12% slower**, because 148 registers per thread means only **one block resident per SM** instead of two. With one block per SM there is nothing to overlap its memory latency against.

This is the register-pressure cliff, and it is a *step function*, not a gradient. Occupancy is `floor(registers_per_SM / registers_per_block)`; crossing a threshold halves your resident blocks in one step. Sweeping 8 → 12 → 16 and reporting registers alongside the timing is what makes the mechanism visible rather than mysterious.

> **Steal this.** When tuning a tile or batch width, report **register count and blocks-per-SM** next to each timing. The optimum is almost always the largest tile that stays on the current side of an occupancy step — and you can compute where that step is from `-Xptxas -v` instead of discovering it by sweeping.

**4.1 Attributing a stacked change**

#57 bundles several changes and publishes the incremental attribution, measured on one box, interleaved:

```text
   main                                                        221.4 ms
   + MLA heads batched 12/block, 4 tokens per staged-q pass    133.9
   + attn_res_mix device-wide, KDA state staged, slices = SMs   125.6
   + Q8_0 projections ROWS 16/8/4, fused4 2 → 4                 110.4
```

A single "220 → 110, 2×" headline is unreviewable. This table says which change bought what, so a reviewer can question any one of them, and a later engineer can revert one without guessing. When you must bundle — and [PR #49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) explains that the one-open-PR rule sometimes forces it — publish the ladder.

---

**5. Shard the heads across ranks**

[**PR #63**](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) — *"head-shard both attention bands, budget the split cap, default the Q8 projection path."* **108.4 → 62.8 ms/token at 128k: 9.23 → 15.93 tok/s, +72.6%.**

The observation: attention was running **redundantly on all eight ranks.** The shard policy bands the 896 experts across ranks but replicates everything else, so all 8 GPUs computed all 96 heads and threw away 7/8 of the work. Head-sharding the 24 MLA layers *and* the 69 KDA layers makes each rank compute its own head band, at the cost of an additional all-reduce per layer (185 collectives per token in this configuration, up from 92).

This is the [Lecture 03 §6](Lecture-03.md) Amdahl story getting its answer: replicated attention was the serial term, and head-sharding is what parallelized it. [Lecture 07](Lecture-07.md) covers the collective side.

**5.1 The shard removes the work *and* the parallelism**

The fourth item in #63's list is the most transferable sentence in this lecture:

> *Budget the MLA split cap on `n_head × splits`, because head-sharding **collapses the attention grid to 64 blocks** on a 132-SM part. Worth **−8%** and easy to miss: **the head-shard removes the work *and* the parallelism.**"*

Head-sharding divides `n_head` by `tp_size`. The attention grid is `n_head × splits`. So the same change that cut the work per rank by 8× also cut the grid by 8× — straight back into [Lecture 04 §1](Lecture-04.md)'s starvation. The author anticipated it, budgeted the split cap on the *product* `n_head × splits` rather than on `splits` alone, and recovered the 8%.

```text
   BEFORE shard:  96 heads × S splits   →  plenty of blocks
   AFTER  shard:  12 heads × S splits   →  64 blocks. starved.
   FIX: budget the cap on the PRODUCT, so S grows as n_head shrinks.
```

> **Any change that divides a parallel axis must re-derive every grid computed from it.** Not "check the kernel you changed" — re-derive the *budget*, so the constant adapts instead of needing to be re-tuned. And then, as [Lecture 04 §5](Lecture-04.md) shows, audit the sibling kernels: this same shard collapsed the KDA decode grid to 12 blocks, and that went unnoticed until [PR #77](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77) three functions away.

</details>

### 5.2 把保守的行标为保守

```text
   ctx     128     512     4k     32k     128k
   before  13.58   13.59   9.84   11.82    9.23
   after   16.42   16.48  11.31   14.46   15.93
```

> *“128k 那一行是全栈（两个 attention band）。128/512/4k/32k 那几行是在 MLA-only 构建上扫出来的，因此是**保守**的——KDA band 不在其中。”*

较短的那几行*低估*了这一变化。把它说出来，与通常的直觉相反，而正是它让表格的其余部分可信。评审者若发现某个数字在悄悄美化，就会不相信全部数字；评审者若发现某个数字被标注为“这比真实值低，因为我的测量方式如此”，就会相信其余的数字。

### 5.3 launch guard，以及它为什么该和 attention 改动放在一起

#49 打包进了一个每阶段一行的 `cudaGetLastError()` 轮询——每 layer 3 次，每 token 约 279 次，而 launch 有约 2,300 次——因为 **k3 的 launcher 中有 18 个返回 `void`，而前向路径上没有任何地方轮询错误。** 后果是：

> *一次失败的 launch 会把上一层的数据留在被复用的 scratch 里，模型继续输出**流畅但错误的输出**。*

那正是 [Lecture 10 §4.4](Lecture-10.md) 所属的那一类，而它之所以被放进一个 *attention* PR，是因为 attention kernel 正是 launch 配置最有可能超限的地方：共享内存的大小由上下文决定，grid 的大小由 head 数乘以 split 数决定，而两者都会随引擎演进变化。[PR #33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33) 刚刚修好了其中一个实例——共享内存按上下文长度分配，超过约 11.7k 就静默失败。#49 修的是*这一类。

而这个 guard 立刻证明了自己的价值：它在引入它的那个 PR 里就抓到了一个 bug——即 §2 里的跨设备 scratch 指针：

```text
   [tp] ncclAllReduce(f32) rank 0: unhandled cuda error
   [tp] ncclGroupEnd: unhandled cuda error
   [k3-tp] all-reduce failed at layer 33
```

> *NCCL 的集合通信内部报出的错误，指认了一个本身没问题的 layer，指向的位置离真正出错的 kernel 十万八千里。*

CUDA 错误是**黏性的**：一次 launch 抛出的错误，会由*下一个*做检查的 API 调用返回。没有按阶段轮询时，第一个要检查的是集合通信——于是第 5 层一个坏掉的 attention kernel 会被报成第 33 层的 all-reduce 失败。按阶段轮询把一个误导性的错误变成一个有定位的错误。

> **在一长串未检查的异步 launch 中，错误会在第一个检查点浮出水面，而不是在故障点。** 频繁轮询的价值不在检测——而在*定位*。

---


<details>
<summary>English original</summary>

**5.2 Report conservative rows as conservative**

```text
   ctx     128     512     4k     32k     128k
   before  13.58   13.59   9.84   11.82    9.23
   after   16.42   16.48  11.31   14.46   15.93
```

> *"The 128k row is the full stack (both attention bands). The 128/512/4k/32k rows were swept on the MLA-only build and are therefore **conservative** — the KDA band is not in them."*

The shorter rows *understate* the change. Saying so is the opposite of the usual instinct, and it is what makes the rest of the table credible. A reviewer who finds one number quietly flattering discounts all of them; a reviewer who finds one number labelled "this is lower than reality because of how I measured it" believes the rest.

**5.3 The launch guard, and why it belongs with an attention change**

#49 bundled a one-line-per-phase `cudaGetLastError()` poll — 3 per layer, ~279 per token against ~2,300 launches — because **18 of the k3 launchers return `void` and nothing on the forward path polled for errors.** The consequence:

> *a failed launch leaves the previous layer's data in reused scratch and the model keeps emitting **fluent, wrong output**.*

That is [Lecture 10 §4.4](Lecture-10.md)'s family, and the reason it sits in an *attention* PR is that attention kernels are where launch configurations are most likely to exceed a limit: shared memory sized from context, grids sized from head count times splits, and both changing as the engine evolves. [PR #33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33) had just fixed one instance — shared memory sized by context length, failing silently past ~11.7k. #49 fixed the *class*.

And the guard immediately earned its place, catching a bug in the very PR that introduced it — the cross-device scratch pointer from §2:

```text
   [tp] ncclAllReduce(f32) rank 0: unhandled cuda error
   [tp] ncclGroupEnd: unhandled cuda error
   [k3-tp] all-reduce failed at layer 33
```

> *An error inside NCCL's collective, naming a layer with nothing wrong with it, pointing nowhere near the faulting kernel.*

CUDA errors are **sticky**: an error raised by one launch is returned by the *next* API call that checks. Without per-phase polling, the first thing to check is the collective — so a broken attention kernel at layer 5 is reported as an all-reduce failure at layer 33. Per-phase polling converts a misleading error into a located one.

> **In a long chain of unchecked async launches, the error surfaces at the first checkpoint, not the fault.** The value of frequent polling is not detection — it is *localization*.

---

</details>

## 6. 测试一条 gate 到不了的路径

本讲反复出现的结构性问题：split 路径与批处理路径只在 `kMlaSplitMinCtx` = 4096 以上才生效，而正确性 gate 跑在 ≤4096。**gate 看不到这些 PR 改动的代码。**

三个 PR 都以三种不同且互补的方式应对了它。

**#49 —— 自己跑一次精度一致性检查，在深度上跑，并说明原因。**

> *“现有两个 gate 都不覆盖 split 路径：`kimi_k3_numeric_test` 是单设备的，而 `kimi_k3_eval.sh` 打分的是 `bench/refdata/hello.ids`，一个永远到不了 `kMlaSplitMinCtx` 的短 prompt，因此走的仍是未拆分的路径。**两者都会在这个 PR 上通过，却一点都没测到它。**”*

于是作者在 128k 上跨全部 8 个 rank 跑了 `compare_logits.py`——即 §2.1 里的数字。认识到你的绿色 gate 与你的改动*毫不相关*，并把缺失的检查补上，这就是全部功夫所在。

**#57 —— 把盲区用作有针对性的逐位一致性检查。**

> *“gate 跑在 n_ctx 4 上，低于 `kMlaSplitMinCtx`，所以它走的是和 main 相同的 per-head kernel——**这正是它之所以构成对 projection、`attn_res_mix` 和 KDA 改动的逐位一致性检查的原因，这些改动确实都在那里执行。**”*

同一个 PR 还改到了一些*确实*会在短上下文下执行的东西。所以 gate 报告 `mean_kld` 与 `main` **到最后一位都相等**（`0.0040455027115537685`），就是一份真正的逐位一致性证明——只不过是对它覆盖到的那个子集而言。精确地知道 gate 验证了你哪些改动，并恰好只声称这么多，比无视 gate 或过度声称都更好。

**#57 和 #63 —— 扩展测试，去够到那条够不到的路径。**

```text
   kimi_k3_numeric_test   35 cases, 0 failures.
     adds ctx 20000 and 12289 at 12 heads — the batched path,
     "otherwise unreachable on a device, it needs ctx > kMlaSplitMinCtx"
     and 20000 at 8 heads — the fallback

   cpu_reference_test
     models the batched + context-split SCHEDULE against a float64
     two-pass reference at K3's real dims (576 / 512 / 128)
     PLUS a NEGATIVE CONTROL that the slice merge needs its
          exp(m_i − m) rescale
```

这里有三点。**在真实维度上的 CPU 参考实现**让你不必用 20,000 token 的 GPU 显存，就能测试一个需要 20,000 token 上下文的 schedule——建模的是 *schedule*，不是 kernel。**在边界值上测试**（12289 刚越过 12288）能抓到 split 算术里的 off-by-one。而**负向对照**是大多数测试套件都缺的细节：一个*故意去掉* `exp(m_i − m)` rescale 并断言结果变错的检查。没有它，一个通过的测试之所以通过，可能只是因为在该测试形状下 rescale 本就多余——而你不会知道你的测试根本没有效力。

> **一个从未失败过的测试，没有被证明是有效的。** 加一个故意破坏不变量的负向对照，并断言测试能抓到它。这就是 [Lecture 10 §5](Lecture-10.md) 的“assert your assertions ran”，在单元测试层面的版本。

#63 的第五项是同一种直觉：*“针对 wide combine 的 CPU 参考测试，**这是端到端准确率 gate 在结构上无法覆盖的。**”*

---

## 7. 阶梯

```text
   decode @ 131,072, 8× H200, UD-IQ1_S, tp=8

   1.01  ──#49──▶  4.53  ──#57──▶  9.06  ──#63──▶  15.93   tok/s
         split         batch heads      shard heads
         context       12/block         across ranks
         4.49×         2.00×            1.73×

   llama.cpp on the same box: 18.44
   ⇒ three PRs took 5.5% of the reference to 86% of it.
```

三个 PR，15.8×，没有一行新的 attention 数学。每一个都找到了不同的可并行维度——上下文、块内的 head、跨 rank 的 head——因为该工作负载的自然轴（decode 时的序列轴）大小为 1。

这就是本讲的教训，也是 [Lecture 04](Lecture-04.md) 在更困难设定下的同一条教训：**在 batch 1 下，性能工作就是寻找一条轴。**


<details>
<summary>English original</summary>

**6. Testing a path your gate cannot reach**

The recurring structural problem in this lecture: the split and batched paths engage only above `kMlaSplitMinCtx` = 4096, and the correctness gate runs at ≤4096. **The gate cannot see the code these PRs changed.**

All three PRs address it, in three different and complementary ways.

**#49 — run your own parity check, at depth, and say why.**

> *"Neither existing gate covers the split path: `kimi_k3_numeric_test` is single-device, and `kimi_k3_eval.sh` scores `bench/refdata/hello.ids`, a short prompt that never reaches `kMlaSplitMinCtx` and therefore exercises the un-split path. **Both would pass this PR while testing none of it.**"*

So the author ran `compare_logits.py` at 128k across all 8 ranks — the numbers in §2.1. Recognizing that your green gates are *irrelevant* to your change, and building the missing check, is the whole discipline.

**#57 — use the blind spot as a targeted bit-identity check.**

> *"The gate runs at n_ctx 4, below `kMlaSplitMinCtx`, so it takes the same per-head kernel main takes — **which is what makes it a bit-identity check on the projection, `attn_res_mix` and KDA changes, all of which do run there.**"*

The same PR touched changes that *do* execute at short context. So the gate reporting `mean_kld` equal to `main` **to the last digit** (`0.0040455027115537685`) is a genuine bit-identity proof — of the subset it reaches. Knowing precisely which of your changes a gate validates, and claiming exactly that much, is better than either ignoring the gate or over-claiming it.

**#57 and #63 — extend the tests to reach the unreachable path.**

```text
   kimi_k3_numeric_test   35 cases, 0 failures.
     adds ctx 20000 and 12289 at 12 heads — the batched path,
     "otherwise unreachable on a device, it needs ctx > kMlaSplitMinCtx"
     and 20000 at 8 heads — the fallback

   cpu_reference_test
     models the batched + context-split SCHEDULE against a float64
     two-pass reference at K3's real dims (576 / 512 / 128)
     PLUS a NEGATIVE CONTROL that the slice merge needs its
          exp(m_i − m) rescale
```

Three things here. **A CPU reference at the real dimensions** lets you test a schedule that needs 20,000 tokens of context without 20,000 tokens of GPU memory — modelling the *schedule*, not the kernel. **Testing at the boundary values** (12289 is just past 12288) catches the off-by-one in the split arithmetic. And the **negative control** is the detail most test suites lack: a check that *deliberately removes* the `exp(m_i − m)` rescale and asserts the result is now wrong. Without it, a test that passes might be passing because the rescale is unnecessary at the tested shape — and you would not know your test has no power.

> **A test that has never failed has not been shown to work.** Add a negative control that breaks the invariant on purpose and assert the test catches it. This is [Lecture 10 §5](Lecture-10.md)'s "assert your assertions ran," at the unit-test level.

#63's fifth item is the same instinct: *"a CPU-reference test for a wide combine, **which the end-to-end accuracy gate structurally cannot reach.**"*

---

**7. The ladder**

```text
   decode @ 131,072, 8× H200, UD-IQ1_S, tp=8

   1.01  ──#49──▶  4.53  ──#57──▶  9.06  ──#63──▶  15.93   tok/s
         split         batch heads      shard heads
         context       12/block         across ranks
         4.49×         2.00×            1.73×

   llama.cpp on the same box: 18.44
   ⇒ three PRs took 5.5% of the reference to 86% of it.
```

Three PRs, 15.8×, and not one line of new attention mathematics. Every one of them found a different dimension to parallelize over — context, heads-within-a-block, heads-across-ranks — because the workload's natural axis (sequence, at decode) is size 1.

That is the lesson of the lecture, and it is the same one as [Lecture 04](Lecture-04.md) in a harder setting: **at batch 1, performance work is the search for an axis.**

---

</details>

## 实验 — 攻击你自己的深度曲线

1. **画出曲线。** 对你的引擎和你的参考实现，测量 ≥5 个上下文深度下的 decode tok/s（decode 即逐 token 生成阶段），深度范围从短上下文一直覆盖到你的上线上下文。如果你的曲线是斜的而他们的不是，你就遇到了本讲的问题。
2. **隔离与深度相关的项。** `time(long) − time(short)` 是随深度增长的开销。它占你每个 token 成本的多少比例？（§1.1）
3. **读一读 attention 网格。** `blocks` 与 SM 数量的对比，以及是否有某个 block 的循环 trip count 是 `O(n_ctx)`。任何一项单独出现都是缺陷；两者同现，就是本讲的主题。
4. **找到你的 A/A 测试。** 找出一个被 gate 关掉的路径，或一个低于你阈值的上下文，使两条 arm 跑的是完全相同的代码。测 10 次并报告波动范围——**那就是你的噪声下限**，低于它的任何结果都不算结果。（§3.2）
5. **指定一个对照阶段。** 选一个你的改动不可能触及的阶段。报告它改动前后的值。如果它的变化超过你的噪声下限，就停下来修 harness（agent 运行时框架）。（§3.1）
6. **实现上下文切分。** 连续切片、部分 `(m, l, acc)`、重缩放合并、最小上下文 gate、每设备 scratch，以及中性的空切片。验证准确率*提升*——如果下降，说明你的合并写错了。（§2）
7. **扫描分块宽度并记录寄存器。** 对每 block 的 head 数或行数，列表记录时间、寄存器数量和 blocks/SM。找到 occupancy 的台阶，并停在它正下方。（§4）
8. **检查你的 gate 覆盖到哪里。** 对每项改动，说明你的正确性 gate 是否会执行被修改的路径。对每个「否」，补上缺失的检查——并加一个**阴性对照**，证明该检查确实有检出能力。（§6）
9. **按阶段轮询 launch 错误。** 数一数你的 `void` launcher 有多少个。加一个按阶段的 `cudaGetLastError()`，并确认一次故意超限的 launch 现在会在正确的阶段被报出来。（§5.3）

通过标准：两个引擎都提交深度曲线，从 A/A 路径测出噪声下限，一次 attention 切分并证明准确率提升，一张包含寄存器数量的分块宽度表，以及一个能触达你的 gate 无法触达路径的测试。

---

## 自检

1. 一个 kernel 在 132-SM 的 GPU 上启动 96 个 block，每个 block 串行遍历 `n_ctx` 个位置。说出这两个彼此独立的缺陷，并指出哪一个解释了深度曲线的*斜率*。
2. MLA 把 KV 压缩成每个位置一个共享 latent。解释 one-block-per-head 的 kernel 如何把这一优势变成 96× 的代价。
3. 从 online-softmax 递推式推导 split-attention 的合并。为什么 output projection 必须在合并*之后*运行，而不是按切片分别运行？
4. 你的切分路径不是 bit 一致的，且在长上下文下比未切分版本*更*准确。解释原因，并说明如果它更不准确意味着什么。
5. 在一个你的新代码路径被 gate 关掉的上下文上，你测到 +2.2%。你测到的是什么？这对同一张表里别处 +1.1% 的结果意味着什么？
6. `ncu` 不可用。你有两个假设——带宽受限和延迟受限。描述在两种假设下都成立的干预措施，以及其结果如何告诉你哪一个是对的。
7. 每 block 16 个 head 比 12 个 head 读缓存更少，却慢了 12%。给出机理，以及你为证明它而报告的两个数字。
8. Head 分片把每个 rank 的 attention 工作量降低 8×，收益却不到 8×。给出两个原因，其中一个不是集合通信。
9. 第 5 层一次错误的 attention launch 产生的 CUDA 错误，被报告为第 33 层的 all-reduce 失败。解释其机理与修复方法。
10. 你为 split-merge 不变量写的测试自写出来就一直在通过。描述一个补充项，它能告诉你这个测试究竟有没有可能失败。


<details>
<summary>English original</summary>

**Lab — attack your own depth curve**

1. **Plot the curve.** Decode tok/s at ≥5 context depths spanning short to your shipping context, for your engine and your reference. If your curve slopes and theirs does not, you have this lecture's problem.
2. **Isolate the depth-dependent term.** `time(long) − time(short)` is the cost that scales with depth. What fraction of your token is it? (§1.1)
3. **Read the attention grid.** `blocks` vs SM count, and whether any block's loop trip count is `O(n_ctx)`. Either alone is a defect; together they are this lecture.
4. **Find your A/A test.** Identify a gated-off path or a context below your thresholds where both arms run identical code. Measure it 10× and report the spread — **that is your noise floor**, and any result under it is not a result. (§3.2)
5. **Name a control phase.** Pick a phase your change cannot touch. Report it before and after. If it moves more than your noise floor, stop and fix the harness. (§3.1)
6. **Implement the context split.** Contiguous slices, partial `(m, l, acc)`, the rescaling combine, a minimum-context gate, per-device scratch, and neutral empty slices. Verify accuracy *improves* — if it degrades, your combine is wrong. (§2)
7. **Sweep your tile width with registers.** For heads- or rows-per-block, tabulate time, register count, and blocks/SM. Find the occupancy step and sit just below it. (§4)
8. **Check what your gate reaches.** For each change, state whether your correctness gate executes the modified path. For every "no," build the missing check — and add a **negative control** proving the check has power. (§6)
9. **Poll for launch errors per phase.** Count your `void` launchers. Add a per-phase `cudaGetLastError()` and confirm a deliberately over-sized launch is now reported at the right phase. (§5.3)

Pass criterion: a committed depth curve for both engines, a measured noise floor from an A/A path, one attention split with accuracy shown to improve, a tile-width table including register counts, and a test that reaches a path your gate cannot.

---

**Self-check**

1. A kernel launches 96 blocks, each walking `n_ctx` positions serially, on a 132-SM GPU. Name the two independent defects and say which one explains the *slope* of the depth curve.
2. MLA compresses KV to one shared latent per position. Explain how a one-block-per-head kernel converts that advantage into a 96× penalty.
3. Derive the split-attention combine from the online-softmax recurrence. Why must the output projection run *after* the merge rather than per slice?
4. Your split path is not bit-identical and is *more* accurate than the unsplit version at long context. Explain, and say what it would mean if it were less accurate.
5. You measure +2.2% at a context where your new code path is gated off. What have you measured, and what does it imply about a +1.1% result elsewhere in the same table?
6. `ncu` is unavailable. You have two hypotheses — bandwidth-bound and latency-bound. Describe the intervention that is justified under both, and how its outcome tells you which was right.
7. 16 heads per block reads the cache less than 12 and is 12% slower. Give the mechanism and the two numbers you would report to prove it.
8. Head-sharding cuts per-rank attention work 8× and yields less than 8×. Give two reasons, one of which is not the collective.
9. A CUDA error from a bad attention launch at layer 5 is reported as an all-reduce failure at layer 33. Explain the mechanism and the fix.
10. Your test for a split-merge invariant has passed since it was written. Describe the one addition that would tell you whether it can fail at all.

---

</details>

## 参考文献

* **这些 PR** — [#33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33)（MLA decode（逐 token 生成阶段）在约 11.7k 之后静默停止启动）、[#49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49)（按上下文切分 + 启动守卫）、[#57](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/57)（每 block 成批处理 head）、[#63](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63)（两个 band 都做 head 分片）、[#73](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/73) / [#77](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77)（#63 造成的 grid 塌缩）。这些 PR 的正文是主要来源。
* **Flash-Decoding** — [PyTorch 博客，2023 年 10 月](https://pytorch.org/blog/flash-decoding/) — 把 KV 维度切分到多个 SM 上，再合并。#49 实现的就是这项技术。
* **FlashAttention** — [arXiv:2205.14135](https://arxiv.org/abs/2205.14135) 与 **FlashAttention-2** [arXiv:2307.08691](https://arxiv.org/abs/2307.08691) — §2 的 combine 将其推广到跨 block 的那套 online-softmax 重缩放。
* **Online softmax** — Milakov & Gimelshein，[arXiv:1805.02867](https://arxiv.org/abs/1805.02867) — `(m, l)` 的 running-max 形式。
* **MLA（多头潜在注意力）** — DeepSeek V2 [arXiv:2405.04434](https://arxiv.org/abs/2405.04434)、V3 [arXiv:2412.19437](https://arxiv.org/abs/2412.19437) — §1.1 讨论的正是这套压缩 KV 设计的复用。
* **FlashInfer** — [arXiv:2501.01005](https://arxiv.org/abs/2501.01005)（MLSys 2025）— 带负载均衡切分调度的生产级 attention 引擎；在能用的地方，就用它代替自己写。
* **成对求和误差界** — Higham，*Accuracy and Stability of Numerical Algorithms*，第 4 章 — §2.1 背后的 `O(n)` → `O(log n)` 结论。

交叉引用：

* [Part 2 Lecture 06 — Hopper 上 128K 的长上下文](../02-Hopper稠密计算/Lecture-06.md) — KV 扩展与 chunked prefill（首字前的整段计算），生产环境中的表述框架。
* [Part 3 Lecture 01 — 现代 MoE（混合专家模型）剖析](../03-Blackwell-MoE/Lecture-01.md) — MLA 的 KV 压缩机制。
* [Lecture 04 — 启动几何](Lecture-04.md) — §5.1 造成的 grid 塌缩，以及 occupancy 算术。
* [Lecture 07 — 分片 896 个专家](Lecture-07.md) — §5 的 head 分片带来的集合通信开销。
* [Lecture 10 — 静默出错](Lecture-10.md) — §5.3 的静默启动问题族。

---

## 截至 2026-08

8× H200 SXM，`sm_90`，132 个 SM，CUDA 12.8+，UD-IQ1_S，tp=8，评分的上下文 131,072。`kMlaSplitMinCtx` = 4096；125 个寄存器下每 block 12 个 head，每 SM 2 个 block。数字来自 PR #49 / #57 / #63；凡有说明处，均为单二进制、用环境变量门控的 A/B。轴搜索的框架、A/A 噪声底、对照阶段，以及负对照测试，是长期有效的内容。

---

## 下一节

* 下一篇：[Lecture 07 — 分片 896 个专家，以及随之而来的 Amdahl 陷阱](Lecture-07.md)
* 上一篇：[Lecture 05 — 融合与激活值量化纪律](Lecture-05.md)
* 返回上级：[Part 4 — 优化一个真实引擎](README.md)


<details>
<summary>English original</summary>

**References**

* **The PRs** — [#33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33) (MLA decode silently stops launching past ~11.7k), [#49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) (split over context + the launch guard), [#57](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/57) (batch heads per block), [#63](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) (head-shard both bands), [#73](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/73) / [#77](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/77) (the grid collapses #63 created). Their bodies are the primary source.
* **Flash-Decoding** — [PyTorch blog, Oct 2023](https://pytorch.org/blog/flash-decoding/) — split the KV dimension across SMs, then combine. The technique #49 implements.
* **FlashAttention** — [arXiv:2205.14135](https://arxiv.org/abs/2205.14135) and **FlashAttention-2** [arXiv:2307.08691](https://arxiv.org/abs/2307.08691) — the online-softmax rescaling that §2's combine generalizes across blocks.
* **Online softmax** — Milakov & Gimelshein, [arXiv:1805.02867](https://arxiv.org/abs/1805.02867) — the `(m, l)` running-max formulation.
* **MLA (Multi-head Latent Attention)** — DeepSeek V2 [arXiv:2405.04434](https://arxiv.org/abs/2405.04434), V3 [arXiv:2412.19437](https://arxiv.org/abs/2412.19437) — the compressed-KV design whose reuse §1.1 is about.
* **FlashInfer** — [arXiv:2501.01005](https://arxiv.org/abs/2501.01005) (MLSys 2025) — the production attention engine with load-balanced split scheduling; what you use instead of writing this yourself, where you can.
* **Pairwise summation error bounds** — Higham, *Accuracy and Stability of Numerical Algorithms*, ch. 4 — the `O(n)` → `O(log n)` result behind §2.1.

Cross-references:

* [Part 2 Lecture 06 — Long context at 128K on Hopper](../02-Hopper稠密计算/Lecture-06.md) — KV scaling and chunked prefill, the production framing.
* [Part 3 Lecture 01 — Anatomy of a modern MoE](../03-Blackwell-MoE/Lecture-01.md) — MLA's KV-compression mechanism.
* [Lecture 04 — Launch geometry](Lecture-04.md) — the grid collapses §5.1 creates, and the occupancy arithmetic.
* [Lecture 07 — Sharding 896 experts](Lecture-07.md) — the collective cost of §5's head-sharding.
* [Lecture 10 — Silently wrong](Lecture-10.md) — §5.3's silent-launch family.

---

**Current as of 2026-08**

8× H200 SXM, `sm_90`, 132 SMs, CUDA 12.8+, UD-IQ1_S, tp=8, scored context 131,072. `kMlaSplitMinCtx` = 4096; 12 heads/block at 125 registers, 2 blocks/SM. Numbers from PRs #49 / #57 / #63; all single-binary env-gated A/B where stated. The axis-search framing, the A/A noise floor, the control phase, and the negative-control test are the durable content.

---

**Next**

* Next: [Lecture 07 — Sharding 896 experts, and the Amdahl trap that followed](Lecture-07.md)
* Previous: [Lecture 05 — Fusion and the activation-quantization discipline](Lecture-05.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
