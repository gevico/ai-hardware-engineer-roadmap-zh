---
title: Lecture 06 - 让 decode 变快：投机解码、DFlash 与 Flash Kernel
description: Lecture 06 - 让 decode 变快：投机解码、DFlash 与 Flash Kernel
---

# Lecture 06 - 让 decode 变快：投机解码、DFlash 与 Flash Kernel

**合集：** [MLSys Deep Dives](README.md) | **上一篇：** [← Lecture 05](Lecture-05.md) | **下一篇：** [Lecture 07](Lecture-07.md)

---

kernel 已经变快（Lec 2–3），模型已经变精简（Lec 4–5）。本讲攻克最后一个、也是最顽固的成本：**自回归 decode（逐 token 生成阶段）是串行的且带宽受限**，没有任何 kernel 能让一个根本串行的循环并行化。2023–2026 年的突破来自算法 —— **投机解码** —— 一族技巧，每次遍历权重可产出 *多个* token，且**输出可证明与普通 decode 相同**。

这是栈中回报最密集的 layer：投机解码通常能带来 2–6× 的加速且质量损失为 *零*，而 2026 年的前沿（DFlash，MiMo + TileRT 的 1000-tok/s 里程碑）则更进一步。本讲追溯算法谱系、让验证步骤变廉价的 attention kernel（FlashAttention-3、FlashInfer），以及处于其中大部分工作中心的研究组 —— **Together AI**。

---

## 学习目标

在本讲结束时，你应该能够：

1. 解释为什么 decode 是**带宽受限**的，以及为什么验证 K 个候选 token 的内存流量与生成一个 token 大致相同。
2. 追溯投机解码的谱系：**draft model → Medusa → Hydra → Sequoia → Lookahead → EAGLE-1/2/3**，以及每个方法新增了什么。
3. 描述 **EAGLE-3**（直接 token 预测，“训练时测试”）与 **DFlash**（块扩散 drafting）以及它们为何胜过前代方法。
4. 将**接受长度**作为主导指标，并将其与加速比关联。
5. 将 **FlashAttention-3、FlashInfer、Flash-Decoding** 定位为让 verify/attention 步骤变快的 kernel。
6. 将 **MiMo + TileRT** 的 1000-tok/s 结果解读为一个 *栈*（量化 + DFlash + megakernel），并定位 **Together AI** 在整个 layer 上的研究。

---

## 1. 为什么 decode 卡住了 —— 以及出路

回顾 Lecture 1：**prefill（首字前的整段计算）是算力受限，decode 是带宽受限。** 生成一个 token 需要从 HBM 流式读取*整个*权重集（并遍历整个 KV cache），却只做极少量的矩阵乘。Tensor Core 大部分时间处于空闲；瓶颈是带宽。

```text
   one decode step (generate 1 token):
   ┌────────────────────────────────────────────────────────┐
   │  stream ALL weights + KV from HBM  ───────────────────► │  ← dominates (memory bandwidth)
   │  matmuls (Tensor Cores ~idle)                           │  ← tiny (compute)
   │  emit 1 token                                           │
   └────────────────────────────────────────────────────────┘
```

下面是解锁一切的关键观察：**验证 K 个候选 token 的内存流量与生成一个 token 基本相同** —— 仍然是对权重的一次遍历。因此，如果你能廉价地*猜测*下 K 个 token，然后在一次遍历中*验证*它们，就能将主要内存开销分摊到多个 token 上：

```text
   speculative decoding:
   ① a cheap DRAFT proposes K tokens ahead
   ② the big TARGET verifies all K in ONE forward pass
   ③ accept the longest prefix the target agrees with;  re-draft from there
   ⇒ same HBM traffic per pass, up to ~K tokens out  →  more tokens per memory pass
   ⇒ OUTPUT IS PROVABLY IDENTICAL to normal target decoding (lossless)
```

最后一行正是投机解码特别之处：它是**质量上的免费午餐** —— 分布相同，速度更快。“可证明相同”是一个强有力的主张，因此这里是证明的引擎 —— **接受规则**（修改的拒绝采样），本讲每个方法都继承它：

```text
   for each drafted token x, with draft probability q(x) and target probability p(x):
       accept x with probability  min(1, p(x) / q(x))
       on the first rejection:    resample from  norm( max(0, p − q) )   and stop there
   ⇒ the token that comes out — accepted or resampled — is distributed EXACTLY as p,
     for ANY draft q. (greedy is the easy case: accept while draft matches target argmax.)
```

梳理直觉：当 draft *过度*提议某个 token（`q > p`）时，接受概率被 `p/q` 削减；当它*提议不足*（`q < p`）时，该 token 总是被接受，*并且*剩余概率质量（`p − q`）正是拒绝-重采样分布所恢复的。两种效应精确抵消到 `p`。工程师关心的后果：**糟糕的 draft 只会损失速度（低接受率），绝不会损失正确性** —— 这就是为什么整个研究竞赛都在让 draft *更便宜且更准确*（更高接受率），从而让更多 K 个 token 通过验证。

---


<details>
<summary>English original</summary>

**Lecture 06 - Making Decode Fast: Speculative Decoding, DFlash, and Flash Kernels**

**Collection:** [MLSys Deep Dives](README.md) | **Previous:** [← Lecture 05](Lecture-05.md) | **Next:** [Lecture 07](Lecture-07.md)

---

We have made the kernels fast (Lec 2–3) and the model lean (Lec 4–5). This lecture attacks the last and most stubborn cost: **autoregressive decode is sequential and memory-bound**, and no kernel makes a fundamentally serial loop parallel. The breakthrough of 2023–2026 was algorithmic — **speculative decoding** — a family of tricks that emit *multiple* tokens per pass over the weights, **with provably identical output** to normal decoding.

This is the densest-payoff layer in the stack: speculative decoding routinely delivers 2–6× with *zero* quality loss, and the 2026 frontier (DFlash, the MiMo + TileRT 1000-tok/s milestone) pushes further. We trace the algorithm lineage, the attention kernels that make the verify step cheap (FlashAttention-3, FlashInfer), and the research group at the center of most of it — **Together AI**.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why decode is **memory-bound**, and why verifying K candidate tokens costs ~the same memory traffic as generating one.
2. Trace the speculative-decoding lineage: **draft model → Medusa → Hydra → Sequoia → Lookahead → EAGLE-1/2/3**, and what each added.
3. Describe **EAGLE-3** (direct-token prediction, "training-time test") and **DFlash** (block-diffusion drafting) and why they beat predecessors.
4. Use **acceptance length** as the governing metric and relate it to speedup.
5. Place **FlashAttention-3, FlashInfer, Flash-Decoding** as the kernels that make the verify/attention step fast.
6. Read the **MiMo + TileRT** 1000-tok/s result as a *stack* (quantization + DFlash + megakernel), and locate **Together AI**'s research across the whole layer.

---

**1. Why decode is stuck — and the way out**

Recall from Lecture 1: **prefill is compute-bound, decode is memory-bound.** Generating one token requires streaming the *entire* weight set (and traversing the whole KV cache) from HBM, to do a tiny amount of matmul. The Tensor Cores sit mostly idle; the bottleneck is bandwidth.

```text
   one decode step (generate 1 token):
   ┌────────────────────────────────────────────────────────┐
   │  stream ALL weights + KV from HBM  ───────────────────► │  ← dominates (memory bandwidth)
   │  matmuls (Tensor Cores ~idle)                           │  ← tiny (compute)
   │  emit 1 token                                           │
   └────────────────────────────────────────────────────────┘
```

Here is the key observation that unlocks everything: **verifying K candidate tokens costs essentially the same memory traffic as generating one** — it's still one pass over the weights. So if you can *guess* the next K tokens cheaply and then *verify* them all in a single pass, you amortize the dominant memory cost across multiple tokens:

```text
   speculative decoding:
   ① a cheap DRAFT proposes K tokens ahead
   ② the big TARGET verifies all K in ONE forward pass
   ③ accept the longest prefix the target agrees with;  re-draft from there
   ⇒ same HBM traffic per pass, up to ~K tokens out  →  more tokens per memory pass
   ⇒ OUTPUT IS PROVABLY IDENTICAL to normal target decoding (lossless)
```

That last line is what makes speculative decoding special: it is a **free lunch in quality** — same distribution, more speed. "Provably identical" is a strong claim, so here is the proof's engine — the **acceptance rule** (modified rejection sampling), which every method in this lecture inherits:

```text
   for each drafted token x, with draft probability q(x) and target probability p(x):
       accept x with probability  min(1, p(x) / q(x))
       on the first rejection:    resample from  norm( max(0, p − q) )   and stop there
   ⇒ the token that comes out — accepted or resampled — is distributed EXACTLY as p,
     for ANY draft q. (greedy is the easy case: accept while draft matches target argmax.)
```

Walk the intuition: where the draft *over*-proposes a token (`q > p`), acceptance is thinned by `p/q`; where it *under*-proposes (`q < p`), the token is always accepted *and* the leftover probability mass (`p − q`) is exactly what the rejection-resample distribution restores. The two effects cancel to `p` precisely. The consequence an engineer cares about: **a bad draft costs you speed (low acceptance), never correctness** — which is why the entire research race is about making the draft *cheaper and more accurate* (higher acceptance), so more of the K tokens survive verification.

---

</details>

## 2. 主导指标：接受长度

在梳理谱系之前，先确定指标，因为每种方法都以它来衡量。verify 步骤会提出候选 token 的序列（或树）；目标模型接受**最长有效前缀**。每步接受的平均数量就是 **接受长度** τ。

```text
   one verify cycle:  draft K tokens, verify once, keep τ on average

                         τ                  c  =  draft cost per token
   speedup   ≈      ─────────                     ─────────────────────
                     1 + K·c                      target cost per step

   τ = 4, K = 5, c = 0.05 (EAGLE-style head)  →  4 / 1.25  =  3.2×
   τ = 4, K = 5, c = 0.5  (a chunky draft LM) →  4 / 3.5   =  1.14×   ← same τ, draft ate the win
```

这两行就是整个设计空间：**相同的接受长度，价值可能是 3×，也可能一文不值，取决于 draft 的成本。** 因此，方法获胜的方式是**提高 τ**（更好的 draft → 接受更多）或**降低 c**（更便宜地产生猜测）——而 §3 中的谱系正是同时推动这两者的历史。EAGLE-3 报告的 τ 提升可转化为约 3–6.5×；DFlash 通过 block drafting 将 τ 推得更高。当你对 spec decode（其中 decode 为逐 token 生成阶段）做 benchmark 时，**τ 是你报告的数字**——它是墙钟加速比背后的机制层面的真相。

---

## 3. 谱系

每种方法都是对 “如何廉价地产生好的候选 token？” 的不同回答。

```text
   DRAFT MODEL     a small LM drafts K tokens; the big model verifies K in one pass
        │          (simple, but you must train/host a matched small model)
   MEDUSA          add lightweight DECODING HEADS to the frozen base; tree-attention
        │          over candidates. no separate model.            [Tri Dao co-author → Together]
   HYDRA           make the heads SEQUENTIALLY DEPENDENT (each conditions on prior
        │          drafted tokens) → ~1.3× over Medusa.
   SEQUOIA         find the OPTIMAL token-tree topology by dynamic programming;
        │          temperature-robust; HARDWARE-AWARE tree sizing.  [Together co-author]
   LOOKAHEAD       draft-FREE: break the sequential dependency with Jacobi iteration,
        │          generate & verify n-grams in parallel. no aux model.   [Hao AI Lab]
   EAGLE-1         autoregress at the FEATURE level (predict the target's 2nd-to-top
        │          hidden feature, reuse its LM head). ~2.7–3.5×.   [Li et al.]
   EAGLE-2         add a DYNAMIC, context-aware draft TREE (keep high-confidence branches).
        │
   EAGLE-3         drop feature-prediction → DIRECT token prediction; fuse multi-layer
        │          features via "TRAINING-TIME TEST". ~3–6.5×, +20–40% over EAGLE-2,
        │          and the speedup now SCALES WITH TRAINING DATA. the de-facto baseline.
   DFLASH          a small BLOCK-DIFFUSION draft predicts a WHOLE BLOCK in one pass.
                   2–3× over EAGLE-3 on synchronous requests; >6× overall; lossless.
```

资深工程师会记住的几个要点：

* **Self-drafting 优于独立 draft。** Medusa/EAGLE 将 draft 附加到 *base model*（heads 或轻量特征头）上，不再需要训练和托管一个单独的、良好匹配的小模型——大幅简化了运维。
* **树优于链。** 提出候选的 *树* 并用 tree-attention 验证（Medusa → EAGLE-2 → Sequoia）会提高 τ，因为你在一次 verify pass 中对多个合理的延续进行对冲。Sequoia 使树变得 *最优且硬件感知*。
* **EAGLE-3 的 “training-time test”** 是一个微妙而重要的思想：在 *相同的* 多步自回归条件下训练 draft head，使其与推理时面临的条件一致，从而让训练和测试分布匹配。正是这一点让 EAGLE-3 能在 EAGLE-1/2 陷入平台期的情况下，随着更多数据持续改进。

---


<details>
<summary>English original</summary>

**2. The governing metric: acceptance length**

Before the lineage, fix the metric, because every method is measured by it. The verify step proposes a sequence (or tree) of candidate tokens; the target accepts the **longest valid prefix**. The average number accepted per step is the **acceptance length** τ.

```text
   one verify cycle:  draft K tokens, verify once, keep τ on average

                         τ                  c  =  draft cost per token
   speedup   ≈      ─────────                     ─────────────────────
                     1 + K·c                      target cost per step

   τ = 4, K = 5, c = 0.05 (EAGLE-style head)  →  4 / 1.25  =  3.2×
   τ = 4, K = 5, c = 0.5  (a chunky draft LM) →  4 / 3.5   =  1.14×   ← same τ, draft ate the win
```

The two rows are the whole design space: **the same acceptance length is worth 3× or worth nothing depending on what the draft costs.** So a method wins by **raising τ** (better drafts → more accepted) or **lowering c** (cheaper to produce the guesses) — and the lineage in §3 is exactly the history of pushing both at once. EAGLE-3 reports τ improvements that translate to ~3–6.5×; DFlash pushes τ higher still with block drafting. When you benchmark spec decode, **τ is the number you report** — it's the mechanism-level truth behind the wall-clock speedup.

---

**3. The lineage**

Every method is a different answer to "how do I produce good candidate tokens cheaply?"

```text
   DRAFT MODEL     a small LM drafts K tokens; the big model verifies K in one pass
        │          (simple, but you must train/host a matched small model)
   MEDUSA          add lightweight DECODING HEADS to the frozen base; tree-attention
        │          over candidates. no separate model.            [Tri Dao co-author → Together]
   HYDRA           make the heads SEQUENTIALLY DEPENDENT (each conditions on prior
        │          drafted tokens) → ~1.3× over Medusa.
   SEQUOIA         find the OPTIMAL token-tree topology by dynamic programming;
        │          temperature-robust; HARDWARE-AWARE tree sizing.  [Together co-author]
   LOOKAHEAD       draft-FREE: break the sequential dependency with Jacobi iteration,
        │          generate & verify n-grams in parallel. no aux model.   [Hao AI Lab]
   EAGLE-1         autoregress at the FEATURE level (predict the target's 2nd-to-top
        │          hidden feature, reuse its LM head). ~2.7–3.5×.   [Li et al.]
   EAGLE-2         add a DYNAMIC, context-aware draft TREE (keep high-confidence branches).
        │
   EAGLE-3         drop feature-prediction → DIRECT token prediction; fuse multi-layer
        │          features via "TRAINING-TIME TEST". ~3–6.5×, +20–40% over EAGLE-2,
        │          and the speedup now SCALES WITH TRAINING DATA. the de-facto baseline.
   DFLASH          a small BLOCK-DIFFUSION draft predicts a WHOLE BLOCK in one pass.
                   2–3× over EAGLE-3 on synchronous requests; >6× overall; lossless.
```

A few points a senior engineer holds onto:

* **Self-drafting beat separate drafts.** Medusa/EAGLE attach the draft to the *base model* (heads or a light feature head), removing the need to train and host a separate, well-matched small model — a big operational simplification.
* **Trees beat chains.** Proposing a *tree* of candidates and verifying it with tree-attention (Medusa → EAGLE-2 → Sequoia) raises τ, because you hedge across multiple plausible continuations in one verify pass. Sequoia made the tree *optimal and hardware-aware*.
* **EAGLE-3's "training-time test"** is the subtle, important idea: train the draft head under the *same* multi-step autoregressive condition it faces at inference, so train and test distributions match. This is what let EAGLE-3 keep improving with more data where EAGLE-1/2 plateaued.

---

</details>

## 4. DFlash — 一次草拟整个 block

**DFlash**（你可能见过的 “dfalsh”/“dflash”；arXiv 2602.06036，Z-Lab，2026）是当前的前沿，也是一种真正不同的 draft 机制。它不用逐 token 草拟的自回归 head，而是用一个小型 **block-diffusion** draft 模型，在**单次前向传播中预测整个 block 的 token**——对 verifier 的 hidden states 加 mask embeddings 做非因果 attention，并以 target 的特征为条件。

```text
   EAGLE-3 draft:  token → token → token → ...   (sequential head, K small passes)
   DFlash draft:   [ ▢ ▢ ▢ ▢ ▢ ▢ ] → fill the WHOLE masked block in ONE pass (diffusion)
                   → bigger blocks drafted cheaper → higher τ, fewer draft passes
```

据报告：在同步请求上**比 EAGLE-3 快 2–3×**，**整体 >6×**，仍然**无损**，并且已集成进 **vLLM 的 “speculators”** 框架（有已发布的 checkpoint）。它也是 §6 中 MiMo 吞吐结果里的投机解码组件。要点：draft 机制仍在积极改进——block-diffusion drafting 是 2026 年的 state of the art，“什么是最好的 draft？”是一个活跃的研究前沿，而非已有定论的问题。

---

## 5. kernel 层：让 verify 步骤开销更低

投机解码是*算法*；但 **attention kernel** 仍必须快速跑完 verify pass。三个 Flash 家族 kernel（都与 Tri Dao / Together 关系密切）让这一步变得高效——这是与 spec decode 不同的轴线，叠加在其之上：

* **FlashAttention-3**（2024 年 7 月）—— Hopper 专用的 attention 重写：**warp specialization** 让计算与异步 Tensor-Core + TMA 数据搬运重叠、matmul/softmax 交错流水，并支持 **FP8**。结果：**比 FA-2 快 1.5–2.0×**，FP16 最高 **~740 TFLOP/s（约 75% H100 利用率）**，FP8 最高 **~1.2 PFLOP/s**，数值误差比基线 FP8 低 2.6×。正是这个 kernel 让 attention 本身在 Hopper 上接近峰值。
* **FlashInfer**（MLSys 2025 **Best Paper**）—— 不是单个 kernel，而是一个**面向推理服务的 attention 引擎/库**：处理 **KV-cache 存储异构性**（block-sparse + 可组合格式）、**JIT 编译的可定制 attention 模板**，以及与 CUDA Graphs 兼容的**负载均衡调度**。它被 **vLLM、SGLang 和 MLC-Engine** 采用，现已获 NVIDIA 支持（即 Lecture 3 中的上游化），并报告相比编译器后端**降低 29–69% 的 inter-token 延迟**。2026 年你做 attention 推理服务时，底下很可能就是 FlashInfer。
* **Flash-Decoding**（2023 年 10 月）—— **长上下文、小批**场景下的 decode 阶段（逐 token 生成阶段）技巧，此时 query 长度为 1，朴素的 FlashAttention 只用到 GPU 的 <1%。它**把 attention 的归约沿 KV（序列）维度并行化**——将 keys/values 拆分到各 SM 上，再做一次小的最终合并——从而做到 **64K+ token 下延迟近乎恒定**，端到端最高 **~8×**（在长序列 decode 场景下相比 FA 最高可达 ~50×）。

心智模型：**spec decode 减少你需要的内存 pass 次数；Flash kernel 降低每次 pass 的开销。**两者相乘。一个推理服务栈会*同时*运行 FlashInfer/FA-3 kernel *和* EAGLE-3/DFlash draft。

---


<details>
<summary>English original</summary>

**4. DFlash — drafting a whole block at once**

**DFlash** (the "dfalsh"/"dflash" you may have seen; arXiv 2602.06036, Z-Lab, 2026) is the current frontier and a genuinely different draft mechanism. Instead of an autoregressive head that drafts token-by-token, DFlash uses a small **block-diffusion** draft model that predicts an **entire block of tokens in a single forward pass** — non-causal attention over the verifier's hidden states plus mask embeddings, conditioned on the target's features.

```text
   EAGLE-3 draft:  token → token → token → ...   (sequential head, K small passes)
   DFlash draft:   [ ▢ ▢ ▢ ▢ ▢ ▢ ] → fill the WHOLE masked block in ONE pass (diffusion)
                   → bigger blocks drafted cheaper → higher τ, fewer draft passes
```

Reported: **2–3× larger speedups than EAGLE-3 on synchronous requests**, **>6× overall**, still **lossless**, and it's integrated into **vLLM's "speculators"** framework (with a published checkpoint). It is also the speculative-decoding component inside the MiMo throughput result in §6. The takeaway: the draft mechanism is still actively improving — block-diffusion drafting is the 2026 state of the art, and "what's the best draft?" is a live research front, not a settled question.

---

**5. The kernel layer: making the verify step cheap**

Speculative decoding is the *algorithm*; the **attention kernel** still has to run the verify pass fast. Three Flash-family kernels (all closely tied to Tri Dao / Together) make that step efficient — a different axis from spec decode, stacked on top of it:

* **FlashAttention-3** (Jul 2024) — the Hopper-specific attention rewrite: **warp specialization** to overlap compute with async Tensor-Core + TMA data movement, interleaved matmul/softmax pipelining, and **FP8** support. Results: **1.5–2.0× over FA-2**, FP16 up to **~740 TFLOP/s (~75% H100 utilization)**, FP8 up to **~1.2 PFLOP/s** with 2.6× lower numerical error than baseline FP8. This is the kernel that makes attention itself near-peak on Hopper.
* **FlashInfer** (MLSys 2025 **Best Paper**) — not a single kernel but a **serving-oriented attention engine/library**: handles **KV-cache storage heterogeneity** (block-sparse + composable formats), **JIT-compiled customizable attention templates**, and **load-balanced scheduling** compatible with CUDA Graphs. It's adopted by **vLLM, SGLang, and MLC-Engine**, is now NVIDIA-backed (the upstreaming from Lecture 3), and reports **29–69% inter-token-latency reductions** vs compiler backends. When you serve attention in 2026, FlashInfer is very likely underneath.
* **Flash-Decoding** (Oct 2023) — the decode-phase trick for **long context, small batch**, where query length is 1 and plain FlashAttention uses <1% of the GPU. It **parallelizes the attention reduction across the KV (sequence) dimension** — splitting keys/values across SMs, then a small final combine — giving **near-constant latency to 64K+ tokens** and up to **~8× end-to-end** (and up to ~50× vs FA in the long-seq decode regime).

The mental model: **spec decode reduces *how many* memory passes you need; Flash kernels reduce *how expensive each pass* is.** They multiply. A serving stack runs FlashInfer/FA-3 kernels *and* EAGLE-3/DFlash drafting *together*.

---

</details>

## 6. 案例研究：MiMo + TileRT 突破 1000 tokens/s

把整门课串起来的 2026 年结果：**小米 MiMo 团队 + TileRT runtime 把 1 万亿参数的 MoE（混合专家模型）模型推过了约 1000 tokens/s**（峰值约 1200），而且是在**单个标准 8-GPU 通用节点**上——没有特殊芯片。它值得拆解，因为它*堆叠了这门课的每一层*：

```text
   ① ARCHITECTURE   MiMo-V2.5-Pro: a 1T-param MoE (Lec 5), MTP-friendly, sparse-active
   ② QUANTIZATION   MXFP4 on the MoE expert layers (rest higher precision) → bytes ↓ (Lec 5/7)
   ③ SPEC DECODE    DFlash block-diffusion drafting (§4) → tokens/memory-pass ↑
                    (reported acceptance lengths ~6.30 coding / 5.56 math / 4.29 agent)
   ④ RUNTIME        TileRT persistent MEGAKERNEL (Lec 3): warp-specialized, GPU-resident,
                    overlaps compute/IO/comm, µs-scale overhead → kills the gaps
   ─────────────────────────────────────────────────────────────────────────────────
   ⇒ ~1000–1200 tok/s on a 1T model, one 8-GPU node.  priced ~3× the standard rate for ~10× speed.
```

没有哪一个单独的技巧做到了这件事。架构（稀疏 MoE）+ 精度（MXFP4）+ 算法（DFlash）+ runtime（TileRT megakernel）**叠加**在一起。这就是这门课的核心论点落到了实处：各层是一个协同设计的系统，收益会相乘。

> **时效 / 来源提示。** 该结果由小米 / TileRT 于约 **2026-06-08** 发布，科技媒体报道过；其中的数字（约 1200 tok/s、接受长度、专家层用 MXFP4、“8-GPU 通用节点”——未说明具体 GPU 型号）**由厂商报告，尚未被独立复现**。应视其为前沿数据点，而非已有定论的 benchmark。在设计评审中引用前需重新核实。

---

## 7. Together AI——贯穿这一整层的研究主线

如果说有一个组织处于这一讲的核心，那就是 **Together AI**，主要通过 **Tri Dao**（其首席科学家）及合作者。他们的工作组合*就是*现代 decode（逐 token 生成阶段）加速栈：

| Layer | Together 相关工作 |
|---|---|
| Attention kernel | **FlashAttention** (1/2/3)、**Flash-Decoding**——IO 感知的精确 attention |
| 架构 | **Mamba**（经由 Dao；第 4 讲）——SSM（状态空间模型）这条线 |
| 投机解码 | **Medusa**（Dao 为共同作者）、**Sequoia**（Together 为共同作者） |
| 多 agent 推理 | **Mixture-of-Agents (MoA)**——弱提议者 + 一个聚合器；据报告，用开放模型取得 **65.1% AlpacaEval LC，超过 GPT-4o 的 57.5%** |
| 推理服务 | **Together Inference Engine**——Blackwell 上的生产栈 |

需要理解的定位：Together 以**完整纵向栈著称——从 CUDA attention kernel，经由高效架构，经由投机解码，直到推理服务引擎及其经济性。** 他们 Inference Engine 的营销说法（例如**相比 TensorRT-LLM 提升 +31% TPS**、**饱和时 TTFT 约改善 2×**、Blackwell 上“**每 token 成本最多低 10×**”、在某个编码 agent benchmark 上**比 Claude Opus 便宜 76%**）都是**特定工作负载上的厂商 benchmark**——有方向性参考价值，但不是中立的第三方数字，应按此对待。不过其研究（FlashAttention、Mamba、Medusa、Sequoia）是基础性的，且可独立验证。

---

## 8. 动手 / 实测

在真实的推理服务栈中启用投机解码，测量三个关键数字。

```python
from vllm import LLM, SamplingParams

# EAGLE-3 speculative decoding in vLLM (API surface evolves across versions — check your vLLM)
llm = LLM(
    model="meta-llama/Llama-3.1-8B-Instruct",
    speculative_config={
        "method": "eagle3",
        "model": "yuhuili/EAGLE3-LLaMA3.1-Instruct-8B",
        "num_speculative_tokens": 5,
    },
)
# simplest baseline to compare against: draft-free n-gram speculation
#   speculative_config={"method": "ngram", "num_speculative_tokens": 4}
```

相对于无投机解码的基线进行测量：

```text
   1. CORRECTNESS:   outputs identical to no-spec (greedy) — spec decode is LOSSLESS; verify it
   2. ACCEPTANCE τ:  mean tokens accepted per verify step (vLLM reports this)
   3. SPEEDUP:       tokens/s with spec ÷ tokens/s without  →  recompute $/Mtok (Lecture 1)
```

像“τ = 3.2、2.4× tokens/s、输出完全一致、`$/Mtok` $0.56 → $0.23”这样的结果，就是整讲浓缩成一行：**每次访存产出更多 token、质量相同、成本更低。** 如果 τ 偏低，说明你的 draft 与工作负载不匹配——换一种 draft 方法（EAGLE-3 对比 ngram 对比 DFlash），或换一个与领域匹配的 draft。

---


<details>
<summary>English original</summary>

**6. Case study: MiMo + TileRT past 1000 tokens/s**

The 2026 result that stitches this whole course together: **Xiaomi's MiMo team + the TileRT runtime pushed a 1-trillion-parameter MoE model past ~1000 tokens/s** (peaks ~1200) on **a single standard 8-GPU commodity node** — no exotic silicon. It is worth dissecting because it is *every layer of this course stacked*:

```text
   ① ARCHITECTURE   MiMo-V2.5-Pro: a 1T-param MoE (Lec 5), MTP-friendly, sparse-active
   ② QUANTIZATION   MXFP4 on the MoE expert layers (rest higher precision) → bytes ↓ (Lec 5/7)
   ③ SPEC DECODE    DFlash block-diffusion drafting (§4) → tokens/memory-pass ↑
                    (reported acceptance lengths ~6.30 coding / 5.56 math / 4.29 agent)
   ④ RUNTIME        TileRT persistent MEGAKERNEL (Lec 3): warp-specialized, GPU-resident,
                    overlaps compute/IO/comm, µs-scale overhead → kills the gaps
   ─────────────────────────────────────────────────────────────────────────────────
   ⇒ ~1000–1200 tok/s on a 1T model, one 8-GPU node.  priced ~3× the standard rate for ~10× speed.
```

No single trick did it. Architecture (sparse MoE) + precision (MXFP4) + algorithm (DFlash) + runtime (TileRT megakernel) **compounded**. That is the thesis of this course made concrete: the layers are one co-designed system, and the wins multiply.

> **Currency / sourcing flag.** This result was announced ~**2026-06-08** by Xiaomi/TileRT and covered by tech press; the figures (~1200 tok/s, acceptance lengths, MXFP4-on-experts, "8-GPU commodity node" — exact GPU model unspecified) are **vendor-reported and not yet independently reproduced**. Treat as a leading-edge data point, not a settled benchmark. Re-verify before quoting in a design review.

---

**7. Together AI — the research thread through this whole layer**

If one organization sits at the center of this lecture, it is **Together AI**, largely through **Tri Dao** (its Chief Scientist) and collaborators. Their portfolio *is* the modern decode-acceleration stack:

| Layer | Together-linked work |
|---|---|
| Attention kernels | **FlashAttention** (1/2/3), **Flash-Decoding** — IO-aware exact attention |
| Architecture | **Mamba** (via Dao; Lec 4) — the SSM line |
| Speculative decode | **Medusa** (Dao co-author), **Sequoia** (Together co-author) |
| Multi-agent inference | **Mixture-of-Agents (MoA)** — weak proposers + an aggregator; reported **65.1% AlpacaEval LC, beating GPT-4o's 57.5%** with open models |
| Serving | **Together Inference Engine** — production stack on Blackwell |

The positioning to understand: Together is known for **the full vertical — from the CUDA attention kernel, through efficient architectures, through speculative decoding, to the serving engine and its economics.** Their Inference-Engine marketing claims (e.g. **+31% TPS vs TensorRT-LLM**, **~2× better TTFT at saturation**, "**up to 10× lower cost per token**" on Blackwell, **76% cheaper than Claude Opus** on a coding-agent benchmark) are **vendor benchmarks on specific workloads** — directionally informative, not neutral third-party numbers, and you should treat them as such. The research, though (FlashAttention, Mamba, Medusa, Sequoia), is foundational and independently verifiable.

---

**8. Hands-on / Measure it**

Enable speculative decoding in a real serving stack and measure the three numbers that matter.

```python
from vllm import LLM, SamplingParams

# EAGLE-3 speculative decoding in vLLM (API surface evolves across versions — check your vLLM)
llm = LLM(
    model="meta-llama/Llama-3.1-8B-Instruct",
    speculative_config={
        "method": "eagle3",
        "model": "yuhuili/EAGLE3-LLaMA3.1-Instruct-8B",
        "num_speculative_tokens": 5,
    },
)
# simplest baseline to compare against: draft-free n-gram speculation
#   speculative_config={"method": "ngram", "num_speculative_tokens": 4}
```

Measure, against a no-spec baseline:

```text
   1. CORRECTNESS:   outputs identical to no-spec (greedy) — spec decode is LOSSLESS; verify it
   2. ACCEPTANCE τ:  mean tokens accepted per verify step (vLLM reports this)
   3. SPEEDUP:       tokens/s with spec ÷ tokens/s without  →  recompute $/Mtok (Lecture 1)
```

A result like "τ = 3.2, 2.4× tokens/s, identical outputs, `$/Mtok` $0.56 → $0.23" is the whole lecture in one line: **more tokens per memory pass, same quality, lower cost.** If τ is low, your draft is poorly matched to the workload — try a different draft method (EAGLE-3 vs ngram vs DFlash) or a domain-matched draft.

---

</details>

## 9. Mini-lab

1. **基线：** 不做投机，直接对模型做推理服务。记录 tokens/s、TTFT、TPOT、`$/Mtok`。
2. **投机解码：** 启用 **EAGLE-3**（以及单独启用 **ngram**）投机。记录 τ、tokens/s，并确认输出与贪心基线完全一致。计算新的 `$/Mtok`。
3. **工作负载敏感性：** 在两个工作负载上测量 τ（例如代码 vs 开放式对话）。解释 τ 为何不同（可预测文本 → 更高的接受率）。
4. **（进阶）叠加：** 如果你的技术栈支持，确认 attention 后端是 FlashInfer/FA-3，并推理投机解码（更少 pass）与 Flash kernel（更便宜的 pass）如何叠加。

交付物：跨两个工作负载的一张 `{baseline, ngram, EAGLE-3}` × `{τ, tokens/s, TPOT, $/Mtok, outputs-identical?}` 表，外加一段文字，说明 τ 为何变化、以及 *无损* 加速带来了多少 `$/Mtok`。「无损成本降低」是 MLSys 工程师能在评审里给出的最站得住脚的成果——这个实验就能产出一个。

---

## 关键要点

- Decode 是 **带宽受限** 的：每个 token 都要从 HBM 流式读取全部权重/KV。**验证 K 个候选的代价约等于一次 pass**，因此投机解码每个内存 pass 能产出更多 token——而且是 **无损** 的（可证明输出一致）。
- 核心指标是 **接受长度 τ**：方法要么提高 τ（更好的草稿），要么降低草稿成本。要报告 τ，而不只是墙钟时间。
- 谱系：**draft model → Medusa → Hydra → Sequoia → Lookahead → EAGLE-1/2/3**。自草稿胜过独立草稿；树胜过链；**EAGLE-3**（direct-token + “训练时测试”，约 3–6.5×）是事实上的基线。
- **DFlash** 通过 **一次 pass 草拟整个块**（块扩散），比 EAGLE-3 快 2–3×，已在 vLLM speculators 中——是 2026 年的前沿草稿方法。
- **Flash kernel** 让验证/attention 步骤变便宜：**FlashAttention-3**（约 75% H100 利用率，FP8）、**FlashInfer**（推理服务 attention 引擎，MLSys'25 最佳论文，已进 vLLM/SGLang）、**Flash-Decoding**（长上下文 decode）。投机解码减少 *pass 的数量*；Flash kernel 减少 *每个 pass 的成本*——二者相乘。
- **MiMo + TileRT** 的约 1000-tok/s 结果 = 架构（稀疏 MoE）+ MXFP4 + **DFlash** + **TileRT megakernel** 叠加——本课程的「各层叠加」命题，落到实处（厂商报告，2026 年 6 月）。
- **Together AI**（经由 Tri Dao）支撑了整层——FlashAttention、Mamba、Medusa、Sequoia、MoA、Together Inference Engine——研究是基础性的，推理服务引擎的数字是厂商级的。

---

## 参考文献

- Li et al., "EAGLE-3," arXiv 2503.01840 · repo [https://github.com/SafeAILab/EAGLE](https://github.com/SafeAILab/EAGLE)
- Cai et al., "Medusa," arXiv 2401.10774: [https://arxiv.org/abs/2401.10774](https://arxiv.org/abs/2401.10774)
- Chen et al., "Sequoia," arXiv 2402.12374: [https://arxiv.org/abs/2402.12374](https://arxiv.org/abs/2402.12374)
- "DFlash: Block Diffusion for Flash Speculative Decoding," arXiv 2602.06036 · vLLM speculators [https://docs.vllm.ai/projects/speculators/](https://docs.vllm.ai/projects/speculators/)
- Shah et al., "FlashAttention-3," arXiv 2407.08608: [https://arxiv.org/abs/2407.08608](https://arxiv.org/abs/2407.08608)
- FlashInfer (MLSys 2025 Best Paper): [https://github.com/flashinfer-ai/flashinfer](https://github.com/flashinfer-ai/flashinfer)
- Flash-Decoding: [https://crfm.stanford.edu/2023/10/12/flashdecoding.html](https://crfm.stanford.edu/2023/10/12/flashdecoding.html)
- Together AI, Mixture-of-Agents, arXiv 2406.04692: [https://arxiv.org/abs/2406.04692](https://arxiv.org/abs/2406.04692)
- Xiaomi MiMo + TileRT 1000-tok/s announcement (vendor, Jun 2026): [https://github.com/tile-ai/TileRT](https://github.com/tile-ai/TileRT)

---

## 截至

2026-06。锁定：EAGLE-3（2025，事实上的基线）、DFlash（arXiv 2602.06036，已在 vLLM speculators）、FlashAttention-3（2024 年 7 月，Hopper）、FlashInfer（MLSys 2025 最佳论文）。**MiMo + TileRT 约 1000–1200 tok/s（约 2026-06-08 公布）以及 Together Inference Engine 的数字是厂商报告的，未经独立复现**——正文中已标注。vLLM `speculative_config` API 会随版本演进；请对照你的版本核实。

---

*下一讲：[Lecture 07 — 边缘与物理 AI 前沿](Lecture-07.md)*


<details>
<summary>English original</summary>

**9. Mini-lab**

1. **Baseline:** serve a model with no speculation. Record tokens/s, TTFT, TPOT, `$/Mtok`.
2. **Spec decode:** enable **EAGLE-3** (and, separately, **ngram**) speculation. Record τ, tokens/s, and confirm outputs are identical to greedy baseline. Compute the new `$/Mtok`.
3. **Workload sensitivity:** measure τ on two workloads (e.g. code vs open-ended chat). Explain why τ differs (predictable text → higher acceptance).
4. **(Stretch) stack it:** if your stack supports it, confirm FlashInfer/FA-3 is the attention backend, and reason about how spec decode (fewer passes) and the Flash kernel (cheaper passes) compound.

Deliverable: a `{baseline, ngram, EAGLE-3}` × `{τ, tokens/s, TPOT, $/Mtok, outputs-identical?}` table across two workloads, plus a paragraph on why τ moved and how much `$/Mtok` the *lossless* speedup bought. "Lossless cost reduction" is the most defensible win an MLSys engineer can put in a review — this lab produces one.

---

**Key takeaways**

- Decode is **memory-bound**: each token streams all weights/KV from HBM. **Verifying K candidates costs ~one pass**, so speculative decoding emits more tokens per memory pass — **losslessly** (provably identical output).
- The governing metric is **acceptance length τ**: methods win by raising τ (better drafts) or lowering draft cost. Report τ, not just wall-clock.
- Lineage: **draft model → Medusa → Hydra → Sequoia → Lookahead → EAGLE-1/2/3**. Self-drafting beat separate drafts; trees beat chains; **EAGLE-3** (direct-token + "training-time test", ~3–6.5×) is the de-facto baseline.
- **DFlash** drafts a **whole block in one pass** (block-diffusion), 2–3× over EAGLE-3, in vLLM speculators — the 2026 frontier draft.
- **Flash kernels** make the verify/attention step cheap: **FlashAttention-3** (~75% H100 util, FP8), **FlashInfer** (serving attention engine, MLSys'25 best paper, in vLLM/SGLang), **Flash-Decoding** (long-context decode). Spec decode cuts *how many* passes; Flash kernels cut *cost per pass* — they multiply.
- The **MiMo + TileRT** ~1000-tok/s result = architecture (sparse MoE) + MXFP4 + **DFlash** + **TileRT megakernel** stacked — the course's "layers compound" thesis, made concrete (vendor-reported, June 2026).
- **Together AI** (via Tri Dao) anchors the whole layer — FlashAttention, Mamba, Medusa, Sequoia, MoA, the Together Inference Engine — research foundational, serving-engine numbers vendor-grade.

---

**References**

- Li et al., "EAGLE-3," arXiv 2503.01840 · repo [https://github.com/SafeAILab/EAGLE](https://github.com/SafeAILab/EAGLE)
- Cai et al., "Medusa," arXiv 2401.10774: [https://arxiv.org/abs/2401.10774](https://arxiv.org/abs/2401.10774)
- Chen et al., "Sequoia," arXiv 2402.12374: [https://arxiv.org/abs/2402.12374](https://arxiv.org/abs/2402.12374)
- "DFlash: Block Diffusion for Flash Speculative Decoding," arXiv 2602.06036 · vLLM speculators [https://docs.vllm.ai/projects/speculators/](https://docs.vllm.ai/projects/speculators/)
- Shah et al., "FlashAttention-3," arXiv 2407.08608: [https://arxiv.org/abs/2407.08608](https://arxiv.org/abs/2407.08608)
- FlashInfer (MLSys 2025 Best Paper): [https://github.com/flashinfer-ai/flashinfer](https://github.com/flashinfer-ai/flashinfer)
- Flash-Decoding: [https://crfm.stanford.edu/2023/10/12/flashdecoding.html](https://crfm.stanford.edu/2023/10/12/flashdecoding.html)
- Together AI, Mixture-of-Agents, arXiv 2406.04692: [https://arxiv.org/abs/2406.04692](https://arxiv.org/abs/2406.04692)
- Xiaomi MiMo + TileRT 1000-tok/s announcement (vendor, Jun 2026): [https://github.com/tile-ai/TileRT](https://github.com/tile-ai/TileRT)

---

**Current as of**

2026-06. Pins: EAGLE-3 (2025, de-facto baseline), DFlash (arXiv 2602.06036, in vLLM speculators), FlashAttention-3 (Jul 2024, Hopper), FlashInfer (MLSys 2025 best paper). **MiMo + TileRT ~1000–1200 tok/s (announced ~2026-06-08) and Together Inference Engine numbers are vendor-reported, not independently reproduced** — flagged in-text. vLLM `speculative_config` API evolves across releases; verify against your version.

---

*Next: [Lecture 07 — The edge & physical-AI frontier](Lecture-07.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/MLSys Deep Dives/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/MLSys%20Deep%20Dives/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
