---
title: 模块 07 — mHC：流形约束残差流
description: 模块 07 — mHC：流形约束残差流
---

# 模块 07 — mHC：流形约束残差流

**合集：** [GLM-5.3-Flash 架构精通](README.md) | **上一节：** [← 模块 06](Lecture-06.md) | **下一节：** [模块 08 →](Lecture-08.md)

---

到目前为止，每个机制改变的都是*计算什么*——哪些专家运行、记住什么、读取什么。mHC（流形约束 Hyper-Connections）改变的是另一件事：**信息如何在子层之间流动。** 它最常被误读为会倍增模型的有效深度或宽度，本模块的存在就是为了精确说明它两者都没有做到。

---

## 学习目标

在本模块结束时，你应该能够：

1. 用 collapse/compute/mix 形式写出四流残差更新。
2. 陈述混合矩阵上的双随机约束，并解释它稳定了什么。
3. 复现一个小型数值 Sinkhorn 归一化示例，并解释为什么在实践中结果只是近似双随机。
4. 精确解释为什么四条残差流不意味着四个 attention 模块，以及为什么 45 个 layer 不会变成 45/4 个有效 layer。

---

## 1. 从一条残差流到四条

普通的 Transformer 残差子层是一个单一的累加和：

```text
   x'  =  x  +  F( Norm(x) )
```

使用 `n = 4` 条并行残差流时，网络中任意位置处的每 token 状态不是向量，而是一个小矩阵：

```text
   X  ∈  R^{4 × 4096}          — four parallel copies of the residual, side by side
```

描述每个子层（attention 或前馈）发生什么的一种有用方式是三个步骤——**collapse、compute、mix**：

```text
   x̂  =  Xᵀ · a                          COLLAPSE: combine the 4 streams into ONE vector
                                           before the expensive sublayer runs

   u  =  F( Norm(x̂) )                    COMPUTE: run the actual sublayer (KDA / MLA / FFN)
                                           ONCE, on the collapsed vector — not four times

   X'  =  R · X  +  b · uᵀ               MIX: distribute u back across the streams (b),
                                           and mix the streams among themselves (R)
```

`a` 和 `b` 是学习到的组合/分配向量；`R` 是学习到的残差混合矩阵。三者都可以依赖于输入状态——这正是让连接模式成为*学习到的*而非固定的原因，也是 Hyper-Connections 风格架构相对于普通残差和所做的泛化。

```text
   ┌─────────────────────────────────────────────────────────────────┐
   │   THE EXPENSIVE SUBLAYER (attention or FFN) RUNS ONCE PER LAYER,  │
   │   ON A COLLAPSED VECTOR — regardless of how many residual         │
   │   streams exist. Widening the residual state does not widen       │
   │   how many times the costly computation executes.                 │
   └─────────────────────────────────────────────────────────────────┘
```

---

## 2. 流形约束

“流形约束”特指施加在 `R`（残差混合矩阵）上的约束：它被推向**双随机**——非负元素、每行和为 1、每列和为 1：

```text
   R · 1  ≈  1              (every row sums to ~1 — each output stream is a weighted
                              AVERAGE of input streams, not an uncontrolled blend)

   1ᵀ · R  ≈  1ᵀ             (every column sums to ~1 — each input stream's
                              contribution is conserved across outputs, not
                              silently amplified or discarded)
```

双随机混合矩阵不能让残差流的整体幅度仅由*混合*步骤就发生爆炸或坍缩——无论具体权重如何学习，混合过程中质量都是守恒的。这是一个稳定性属性，而不是准确率声明：**它专门约束残差混合路径。它并不证明网络中其他位置的每个操作都是非扩张的**，而且它对 attention 或前馈计算 `F` 本身没有任何说明，后者仍然可以任意放大或缩小其输入。


<details>
<summary>English original</summary>

**Module 07 — mHC: Manifold-Constrained Residual Streams**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 06](Lecture-06.md) | **Next:** [Module 08 →](Lecture-08.md)

---

Every mechanism so far changed *what gets computed* — which experts run, what gets remembered, what gets read. mHC (manifold-constrained Hyper-Connections) changes something different: **how information flows between sublayers.** It is the mechanism most often misread as multiplying the model's effective depth or width, and this module exists to make precise why it does neither.

---

**Learning objectives**

By the end of this module you should be able to:

1. Write the four-stream residual update in collapse/compute/mix form.
2. State the doubly-stochastic constraint on the mixing matrix and explain what it stabilizes.
3. Reproduce a small numeric Sinkhorn-normalization example and explain why the result is only approximately doubly stochastic in practice.
4. Explain precisely why four residual streams does not mean four attention modules, and why 45 layers does not become 45/4 effective layers.

---

**1. From one residual stream to four**

An ordinary transformer residual sublayer is a single running sum:

```text
   x'  =  x  +  F( Norm(x) )
```

With `n = 4` parallel residual streams, the per-token state at any point in the network is not a vector but a small matrix:

```text
   X  ∈  R^{4 × 4096}          — four parallel copies of the residual, side by side
```

A useful way to describe what happens at each sublayer (attention or feed-forward) is three steps — **collapse, compute, mix**:

```text
   x̂  =  Xᵀ · a                          COLLAPSE: combine the 4 streams into ONE vector
                                           before the expensive sublayer runs

   u  =  F( Norm(x̂) )                    COMPUTE: run the actual sublayer (KDA / MLA / FFN)
                                           ONCE, on the collapsed vector — not four times

   X'  =  R · X  +  b · uᵀ               MIX: distribute u back across the streams (b),
                                           and mix the streams among themselves (R)
```

`a` and `b` are learned combination/distribution vectors; `R` is a learned residual-mixing matrix. All three can depend on the incoming state — this is what makes the connection pattern *learned* rather than fixed, which is the generalization Hyper-Connections-style architectures make over a plain residual sum.

```text
   ┌─────────────────────────────────────────────────────────────────┐
   │   THE EXPENSIVE SUBLAYER (attention or FFN) RUNS ONCE PER LAYER,  │
   │   ON A COLLAPSED VECTOR — regardless of how many residual         │
   │   streams exist. Widening the residual state does not widen       │
   │   how many times the costly computation executes.                 │
   └─────────────────────────────────────────────────────────────────┘
```

---

**2. The manifold constraint**

"Manifold-constrained" refers specifically to a constraint placed on `R`, the residual-mixing matrix: it is pushed toward being **doubly stochastic** — nonnegative entries, every row summing to 1, every column summing to 1:

```text
   R · 1  ≈  1              (every row sums to ~1 — each output stream is a weighted
                              AVERAGE of input streams, not an uncontrolled blend)

   1ᵀ · R  ≈  1ᵀ             (every column sums to ~1 — each input stream's
                              contribution is conserved across outputs, not
                              silently amplified or discarded)
```

A doubly stochastic mixing matrix cannot let the residual stream's overall magnitude explode or collapse purely from the *mixing* step — mass is conserved across the mix regardless of how the specific weights are learned. This is a stability property, not an accuracy claim: **it constrains the residual mixing path specifically. It does not prove every operation elsewhere in the network is non-expansive**, and it says nothing about the attention or feed-forward computation `F` itself, which can still amplify or shrink its input arbitrarily.

</details>

### 达成约束：Sinkhorn 归一化

把任意矩阵推向双随机的标准做法，是交替进行行归一化与列归一化（Sinkhorn–Knopp）。下面是一个小型的通用示例——不是本检查点实际的混合矩阵，只是机制本身，起点是任意的 3×3 矩阵：

```text
   START (arbitrary, nonnegative):        AFTER 20 ROUND-TRIPS (row, then col):
      3.0  1.0  0.5                          0.645  0.252  0.103
      0.5  2.0  1.0                          0.131  0.616  0.252
      1.0  0.5  3.0                          0.224  0.131  0.645

                                           row sums: [1.000, 1.000, 1.000]
                                           col sums: [1.000, 1.000, 1.000]
```

```text
   AFTER ONLY 2 ROUND-TRIPS (what a fixed, small iteration budget looks like):
      0.645  0.251  0.104
      0.133  0.619  0.255
      0.222  0.130  0.641

   row sums: [1.000, 1.007, 0.993]    ←  close, but NOT exactly 1
   col sums: [1.000, 1.000, 1.000]
```

```text
   ┌─────────────────────────────────────────────────────────────────┐
   │   A real implementation runs a FIXED, SMALL number of Sinkhorn   │
   │   iterations (for speed) and adds numerical stabilizers.          │
   │   Treat the result as APPROXIMATELY doubly stochastic — the       │
   │   "≈" in R·1 ≈ 1 is load-bearing, not decorative. Do not          │
   │   implement or test this as an exact symbolic projection.         │
   └─────────────────────────────────────────────────────────────────┘
```

对正确性测试而言，这意味着：检查行和与列和在一个明确给出的容差内 *接近* 1，该容差与所用的实际迭代次数挂钩，而不是要求它们精确等于 1——针对这一机制做精确相等测试，会在完全正确的代码上失败。

---

## 3. mHC 不做的两件事

这两点都直接源自 §1 中的 collapse-compute-mix 结构，且都值得单独提出作为更正，因为它们正是这一机制最常被过度解读的地方：

```text
   FOUR RESIDUAL STREAMS  ≠  FOUR ATTENTION/FFN MODULES

     The collapse step (x̂ = Xᵀa) reduces four streams to ONE vector
     BEFORE the sublayer runs. F(Norm(x̂)) executes once. There are not
     four parallel KDA computations or four parallel MoE dispatches
     happening because there are four streams — there is one of each,
     per layer, exactly as in a single-stream model.


   45 DECODER LAYERS  ≠  45/4  EFFECTIVE SERIAL LAYERS

     mHC changes CONNECTIVITY between sublayers — how information from
     the residual streams is combined going in and redistributed coming
     out. It does not shorten the DEPENDENCY CHAIN: layer 12's output
     still depends on layer 11's output, which still depends on layer
     10's, all the way down. Nothing about having four streams lets you
     skip layers or run them out of order. The serial depth is 45,
     full stop.
```

它留给你的实际成本模型是：更宽的残差 **激活值**（每个位置的残差内存变为 4 倍，因为你要携带的是 `X ∈ R^{4×4096}`，而不是单个 4096-vector）就是 mHC 的全部代价，换来的是可学习的 collapse/distribute/mix 行为的灵活性。它换来的是 sublayer 之间更丰富的信息流动；它换不来沿 decoder 串行深度上的免费并行，也不会让那些昂贵的 sublayer 多执行几次。

---

## 4. 这对性能剖析与实现意味着什么

基于 §1–3，真实实现在每个 sublayer 周围新增的 mHC 专属执行面，恰好是三个小操作：

```text
   mHC collapse + normalization   (X → x̂)
   [ the sublayer itself: KDA, MLA, or FFN — Modules 02–06 ]
   mHC residual mixing            (u → X')
```

这些 collapse 与 mixing 操作相对便宜——是与 `a`、`b`、`R` 做的小型矩阵-向量乘——相对于它们所包围的 sublayer 而言。但「单次操作相对便宜」与「在 45 个 layer × 每 layer 2 个 sublayer × 每 sublayer 2 次 mHC 操作的总量上可忽略」是两种不同的主张，而这正是 [Module 10](Lecture-10.md) 作为一项具体性能剖析假设所标记的那一类成本：来自大量廉价操作的 **small-kernel 启动开销与冗余激活值流量**，即便其中没有任何一个单独看起来昂贵，累积起来也可能形成真实的成本。不要假设 mHC 的开销可忽略；要在这一具体实现上测量它，就像你测量其他任何东西一样。

---


<details>
<summary>English original</summary>

**Reaching the constraint: Sinkhorn normalization**

The standard way to push an arbitrary matrix toward doubly-stochastic is alternating row and column normalization (Sinkhorn–Knopp). A small, generic illustration — not this checkpoint's actual mixing matrix, just the mechanism, on an arbitrary 3×3 starting point:

```text
   START (arbitrary, nonnegative):        AFTER 20 ROUND-TRIPS (row, then col):
      3.0  1.0  0.5                          0.645  0.252  0.103
      0.5  2.0  1.0                          0.131  0.616  0.252
      1.0  0.5  3.0                          0.224  0.131  0.645

                                           row sums: [1.000, 1.000, 1.000]
                                           col sums: [1.000, 1.000, 1.000]
```

```text
   AFTER ONLY 2 ROUND-TRIPS (what a fixed, small iteration budget looks like):
      0.645  0.251  0.104
      0.133  0.619  0.255
      0.222  0.130  0.641

   row sums: [1.000, 1.007, 0.993]    ←  close, but NOT exactly 1
   col sums: [1.000, 1.000, 1.000]
```

```text
   ┌─────────────────────────────────────────────────────────────────┐
   │   A real implementation runs a FIXED, SMALL number of Sinkhorn   │
   │   iterations (for speed) and adds numerical stabilizers.          │
   │   Treat the result as APPROXIMATELY doubly stochastic — the       │
   │   "≈" in R·1 ≈ 1 is load-bearing, not decorative. Do not          │
   │   implement or test this as an exact symbolic projection.         │
   └─────────────────────────────────────────────────────────────────┘
```

For a correctness test, this means: check row and column sums are *close to* 1 within a stated tolerance tied to the actual iteration count used, not that they equal 1 exactly — an exact-equality test against this mechanism will fail on entirely correct code.

---

**3. Two things mHC does not do**

Both follow directly from the collapse-compute-mix structure in §1, and both are worth stating as standalone corrections because they are the most common way this mechanism gets over-interpreted:

```text
   FOUR RESIDUAL STREAMS  ≠  FOUR ATTENTION/FFN MODULES

     The collapse step (x̂ = Xᵀa) reduces four streams to ONE vector
     BEFORE the sublayer runs. F(Norm(x̂)) executes once. There are not
     four parallel KDA computations or four parallel MoE dispatches
     happening because there are four streams — there is one of each,
     per layer, exactly as in a single-stream model.


   45 DECODER LAYERS  ≠  45/4  EFFECTIVE SERIAL LAYERS

     mHC changes CONNECTIVITY between sublayers — how information from
     the residual streams is combined going in and redistributed coming
     out. It does not shorten the DEPENDENCY CHAIN: layer 12's output
     still depends on layer 11's output, which still depends on layer
     10's, all the way down. Nothing about having four streams lets you
     skip layers or run them out of order. The serial depth is 45,
     full stop.
```

The practical cost model this leaves you with: wider residual **activations** (4× the per-position residual memory, since you're carrying `X ∈ R^{4×4096}` instead of a single 4096-vector) is the entire price of mHC, in exchange for the flexibility of learned collapse/distribute/mix behavior. It buys richer information flow between sublayers; it does not buy free parallelism across the decoder's serial depth, and it does not multiply how many times the expensive sublayers execute.

---

**4. What this means for profiling and implementation**

Given §1–3, the mHC-specific execution surface a real implementation adds around every sublayer is exactly three small operations:

```text
   mHC collapse + normalization   (X → x̂)
   [ the sublayer itself: KDA, MLA, or FFN — Modules 02–06 ]
   mHC residual mixing            (u → X')
```

These collapse and mixing operations are comparatively cheap — small matrix-vector products against `a`, `b`, and `R` — relative to the sublayers they surround. But "comparatively cheap per operation" and "negligible in aggregate across 45 layers × 2 sublayers per layer × 2 mHC operations per sublayer" are different claims, and this is exactly the class of cost [Module 10](Lecture-10.md) flags as a specific profiling hypothesis: **small-kernel launch overhead and redundant activation traffic** from many cheap operations can accumulate into a real cost even when no single one of them shows up as expensive in isolation. Do not assume mHC's overhead is negligible; measure it, on this specific implementation, the same way you would measure anything else.

---

</details>

## 检查点

现在应能做到：

1. 凭记忆写出 4 流残差更新的 collapse/compute/mix 方程。
2. 陈述施加在 `R` 上的双随机约束，并解释它稳定了什么、又不保证什么。
3. 复现一个小型 Sinkhorn 归一化示例，并解释为什么固定的小迭代次数只能得到近似的行/列和。
4. 给出本模块旨在传授的、针对「四个流」和「45 层」的两句话更正。
5. 说出 [Module 10](Lecture-10.md) 中针对 mHC 的性能剖析假设，并解释为什么即便每个单独的 mHC 操作都很廉价，该假设依然成立。

---

## 交付

这是 **[capstone ladder](Lecture-12.md) 的 Stage 6** 的另一半（与 [Module 02](Lecture-02.md) 的 router 审计配对）。构建一个 **mHC 流布局与混合测试**：在玩具级多流残差上实现 collapse/compute/mix 更新，验证 `R` 的行和与列和落在你的实现实际的 Sinkhorn 迭代容差之内（而非精确的 1.0），并验证子层函数 `F` 无论流数是多少都恰好每层被调用一次——这是对 §3 中「四个流 ≠ 四个模块」这一说法的直接、可执行检验。

---

## 时效性

* **不随时间变化：** 多流残差的 collapse/compute/mix 分解，Sinkhorn 归一化机制及其在有限迭代预算下近似（而非精确）的收敛性，以及 §3 中的深度/宽度更正——这些源自任何双随机约束的多流残差设计的结构，与具体检查点无关。
* **与检查点相关：** 流数（4）以及 mHC 前/后处理在每个子层周围的确切布局，属于该检查点配置的属性。

---

**下一篇：** [Module 08 — 视觉、MTP 与混合推理服务状态 →](Lecture-08.md)


<details>
<summary>English original</summary>

**Checkpoint**

You should now be able to:

1. Write the collapse/compute/mix equations for a 4-stream residual update from memory.
2. State the doubly-stochastic constraint on `R` and explain what it stabilizes versus what it does not guarantee.
3. Reproduce a small Sinkhorn-normalization example and explain why a fixed small iteration count yields only approximate row/column sums.
4. Give the two-sentence correction for "four streams" and "45 layers" that this module exists to teach.
5. Name the mHC-specific profiling hypothesis from [Module 10](Lecture-10.md) and explain why it applies even though each individual mHC operation is cheap.

---

**Ship it**

This is the other half of **Stage 6 of the [capstone ladder](Lecture-12.md)** (paired with [Module 02](Lecture-02.md)'s router audit). Build an **mHC stream-layout and mixing test**: implement the collapse/compute/mix update on a toy multi-stream residual, verify `R`'s row and column sums land within your implementation's actual Sinkhorn-iteration tolerance (not exact 1.0), and verify that the sublayer function `F` is invoked exactly once per layer regardless of stream count — a direct, executable check against the "four streams ≠ four modules" claim in §3.

---

**Current as of**

* **Timeless:** the collapse/compute/mix decomposition of a multi-stream residual, the Sinkhorn-normalization mechanism and its approximate (not exact) convergence under a finite iteration budget, and the depth/width corrections in §3 — these follow from the structure of any doubly-stochastic-constrained multi-stream residual design, independent of a specific checkpoint.
* **Checkpoint-specific:** the stream count (4) and the exact placement of mHC pre/post-processing around each sublayer are properties of this checkpoint's configuration.

---

**Next:** [Module 08 — Vision, MTP, and Hybrid Serving State →](Lecture-08.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
