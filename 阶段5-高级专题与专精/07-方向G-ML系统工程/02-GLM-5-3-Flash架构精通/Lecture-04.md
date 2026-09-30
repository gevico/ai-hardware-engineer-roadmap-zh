---
title: Module 04 — KDA II：分块并行与完整子层
description: Module 04 — KDA II：分块并行与完整子层
---

# Module 04 — KDA II：分块并行与完整子层

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 03](Lecture-03.md) | **Next:** [Module 05 →](Lecture-05.md)

---

[Module 03](Lecture-03.md) 正确推导出了该递推式，但用 Python 逐 token 循环实现它在真实硬件上毫无用处：prefill（首字前的整段计算）一次给你成百上千个 token，为了满足串行递推而丢弃这种并行性，等于扔掉了 GPU 几乎全部的吞吐。本模块覆盖一个生产级 KDA 实现所需的、仅靠递推式无法提供的两样东西：一种将其并行化的方法，以及子层中围绕它的一切。

---

## 学习目标

学完本模块后，你应当能够：

1. 推导出使分块/并行 KDA 成为可能的仿射复合恒等式。
2. 解释为什么 decode（逐 token 生成阶段）与 prefill 对同一个递推式需要结构上不同的 kernel。
3. 列出 KDA 子层中除状态更新之外的每一个组件，并解释为什么仅对递推式做 benchmark 会低估 layer 延迟。
4. 陈述分块实现相对于递推参考必须满足的两部分正确性不变量。

---

## 1. 递推式具有仿射形式

[Module 03](Lecture-03.md) 的闭式解：

```text
   S_t  =  (I − β_t k_t k_tᵀ) D_t · S_{t−1}  +  β_t k_t v_tᵀ
```

是一般仿射更新的一例：

```text
   S_t  =  A_t · S_{t−1}  +  B_t

   where   A_t  =  (I − β_t k_t k_tᵀ) D_t          B_t  =  β_t k_t v_tᵀ
```

仿射更新可以**复合**。把第 `t` 步的更新代入第 `t+1` 步的更新：

```text
   S_{t+1}  =  A_{t+1} · S_t  +  B_{t+1}
            =  A_{t+1} · (A_t · S_{t−1} + B_t)  +  B_{t+1}
            =  (A_{t+1} A_t) · S_{t−1}  +  (A_{t+1} B_t + B_{t+1})
```

连续两步坍缩为单次仿射更新，其转移矩阵 `A_{t+1}A_t` 是**复合**的，偏移 `A_{t+1}B_t + B_{t+1}` 也是**复合**的。你完全可以对一整块 `L` 个 token 继续这样做：整块对输入状态的作用归结为一个复合的 `A` 和一个复合的 `B`，其计算与该块开始*之前*的状态无关。正是这种独立性使块级并行成为可能 —— 你可以在各块跨序列位置并行处理的同时，计算每块的复合转移，然后再施加（开销很小的、串行的）块到块复合。

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │  This explains WHY parallel formulations of the delta rule exist.  │
   │  It does NOT mean a good kernel should materialize the dense        │
   │  d_k × d_k matrix A_t for every token and multiply them out         │
   │  explicitly — that throws away the diagonal (D_t) and rank-one      │
   │  ((I − β k kᵀ)) structure that makes each A_t cheap to apply in     │
   │  the first place. A real chunked kernel keeps A_t and B_t in their  │
   │  structured (diagonal + rank-one) form throughout the composition. │
   └────────────────────────────────────────────────────────────────────┘
```

---


<details>
<summary>English original</summary>

**Module 04 — KDA II: Chunked Parallelism & the Full Sublayer**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 03](Lecture-03.md) | **Next:** [Module 05 →](Lecture-05.md)

---

[Module 03](Lecture-03.md) derived the recurrence correctly, but a token-by-token Python loop implementing it would be useless on real hardware: prefill hands you hundreds or thousands of tokens at once, and discarding that parallelism to satisfy a serial recurrence throws away nearly all of the GPU's throughput. This module covers the two things a production KDA implementation needs that the recurrence alone does not give you: a way to parallelize it, and everything in the sublayer that surrounds it.

---

**Learning objectives**

By the end of this module you should be able to:

1. Derive the affine-composition identity that makes chunked/parallel KDA possible.
2. Explain why decode and prefill need structurally different kernels for the same recurrence.
3. List every component of the KDA sublayer beyond the state update, and explain why benchmarking the recurrence alone understates layer latency.
4. State the two-part correctness invariant a chunked implementation must satisfy against the recurrent reference.

---

**1. The recurrence has affine form**

[Module 03](Lecture-03.md)'s closed form:

```text
   S_t  =  (I − β_t k_t k_tᵀ) D_t · S_{t−1}  +  β_t k_t v_tᵀ
```

is an instance of the general affine update:

```text
   S_t  =  A_t · S_{t−1}  +  B_t

   where   A_t  =  (I − β_t k_t k_tᵀ) D_t          B_t  =  β_t k_t v_tᵀ
```

Affine updates **compose**. Substitute the update for step `t` into the update for step `t+1`:

```text
   S_{t+1}  =  A_{t+1} · S_t  +  B_{t+1}
            =  A_{t+1} · (A_t · S_{t−1} + B_t)  +  B_{t+1}
            =  (A_{t+1} A_t) · S_{t−1}  +  (A_{t+1} B_t + B_{t+1})
```

Two consecutive steps collapse into a single affine update with a **composed** transition matrix `A_{t+1}A_t` and a **composed** offset `A_{t+1}B_t + B_{t+1}`. Nothing stops you from continuing this for an entire chunk of `L` tokens: the whole chunk's effect on the incoming state reduces to one composed `A` and one composed `B`, computable independently of the state you had *before* the chunk started. That independence is exactly what makes chunk-level parallelism possible — you can compute each chunk's composed transition while chunks are processed in parallel across sequence positions, then apply the (cheap, sequential) chunk-to-chunk composition afterward.

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │  This explains WHY parallel formulations of the delta rule exist.  │
   │  It does NOT mean a good kernel should materialize the dense        │
   │  d_k × d_k matrix A_t for every token and multiply them out         │
   │  explicitly — that throws away the diagonal (D_t) and rank-one      │
   │  ((I − β k kᵀ)) structure that makes each A_t cheap to apply in     │
   │  the first place. A real chunked kernel keeps A_t and B_t in their  │
   │  structured (diagonal + rank-one) form throughout the composition. │
   └────────────────────────────────────────────────────────────────────┘
```

---

</details>

## 2. 为什么 decode（逐 token 生成阶段）与 prefill（首字前的整段计算）需要不同的 kernel

仿射复合技巧告诉你并行性是*在数学上可用的*；它并没有告诉你 decode 就应该用上它。

```text
   DECODE                                    PREFILL
   ────────────────────────────              ────────────────────────────
   ONE new token arrives at a time            HUNDREDS/THOUSANDS of tokens
                                               arrive at once (the prompt)
   the incoming state S_{t-1} is already      no state exists yet for
   sitting in memory, ready to use            most of the sequence — it
                                               has to be BUILT, in order
   a single fused recurrent step is           a serial token-by-token loop
   the natural, cheap operation                discards nearly all available
                                               parallelism across the L
                                               prompt tokens
   → FUSED RECURRENT KERNEL                   → CHUNKED KERNEL using §1's
                                                 composition to process
                                                 blocks of tokens in parallel,
                                                 then compose block results
                                                 sequentially
```

这与支配普通 Transformer attention 的 prefill/decode 非对称性完全一致——prefill 面向吞吐，可利用跨位置的并行性；decode 面向延迟，受制于必须按序产生的状态——只不过这里是把它施加到循环机制而非二次型机制上。该模型的推理服务系统需要 **两类** KDA kernel，按请求阶段选取，正如它需要为 DSA/MLA layer 提供独立的 prefill 与 decode 路径一样。

---

## 3. 子层比循环本身更大

[Module 03](Lecture-03.md) 中的循环是数学核心，但实际执行的 KDA 子层在其外还包裹了若干额外操作：

```text
   x  ──▶  Q/K/V projections
       ──▶  short causal convolution  (on Q, K, and/or V — a small local mixing step)
       ──▶  Q/K normalization
       ──▶  learned decay gate   →  produces α_t (and hence D_t)
       ──▶  learned write gate   →  produces β_t
       ──▶  THE RECURRENCE  (Module 03)
       ──▶  gated normalization on the output
       ──▶  output projection
       ──▶  back into the residual stream (via mHC — Module 07)
```

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │   Do not benchmark only the state update and call that            │
   │   "KDA layer latency." The projections, convolution, and gating   │
   │   surrounding the recurrence can account for a substantial        │
   │   share of a KDA sublayer's execution time even when the           │
   │   recurrence itself is efficiently implemented.                    │
   └──────────────────────────────────────────────────────────────────┘
```

这对 [Module 10](Lecture-10.md) 的性能剖析准则有直接影响：当你着手优化“KDA”时，首先得确定这八个阶段中*究竟哪一个*在消耗你想要压缩的时间预算。如果真正主导子层延迟的是短卷积或门控投影，那么循环 kernel 融合得再漂亮也毫无收益。

---


<details>
<summary>English original</summary>

**2. Why decode and prefill need different kernels**

The affine-composition trick tells you parallelism is *mathematically available*; it does not tell you decode should use it.

```text
   DECODE                                    PREFILL
   ────────────────────────────              ────────────────────────────
   ONE new token arrives at a time            HUNDREDS/THOUSANDS of tokens
                                               arrive at once (the prompt)
   the incoming state S_{t-1} is already      no state exists yet for
   sitting in memory, ready to use            most of the sequence — it
                                               has to be BUILT, in order
   a single fused recurrent step is           a serial token-by-token loop
   the natural, cheap operation                discards nearly all available
                                               parallelism across the L
                                               prompt tokens
   → FUSED RECURRENT KERNEL                   → CHUNKED KERNEL using §1's
                                                 composition to process
                                                 blocks of tokens in parallel,
                                                 then compose block results
                                                 sequentially
```

This is the same prefill/decode asymmetry that governs ordinary transformer attention — prefill is throughput-oriented and can exploit parallelism across positions, decode is latency-oriented and bottlenecked on state that must be produced in order — applied to a recurrent mechanism instead of a quadratic one. A serving system for this model needs **both** kernel families for KDA, selected by request phase, exactly as it needs separate prefill and decode paths for the DSA/MLA layers.

---

**3. The sublayer is bigger than the recurrence**

The recurrence in [Module 03](Lecture-03.md) is the mathematical core, but the executed KDA sublayer wraps it in several additional operations:

```text
   x  ──▶  Q/K/V projections
       ──▶  short causal convolution  (on Q, K, and/or V — a small local mixing step)
       ──▶  Q/K normalization
       ──▶  learned decay gate   →  produces α_t (and hence D_t)
       ──▶  learned write gate   →  produces β_t
       ──▶  THE RECURRENCE  (Module 03)
       ──▶  gated normalization on the output
       ──▶  output projection
       ──▶  back into the residual stream (via mHC — Module 07)
```

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │   Do not benchmark only the state update and call that            │
   │   "KDA layer latency." The projections, convolution, and gating   │
   │   surrounding the recurrence can account for a substantial        │
   │   share of a KDA sublayer's execution time even when the           │
   │   recurrence itself is efficiently implemented.                    │
   └──────────────────────────────────────────────────────────────────┘
```

This has a direct consequence for [Module 10](Lecture-10.md)'s profiling discipline: when you set out to optimize "KDA," first determine *which* of these eight stages is actually consuming the time budget you're trying to reduce. A beautifully fused recurrent kernel delivers nothing if the short convolution or the gating projections are what's actually dominating the sublayer's latency.

---

</details>

## 4. 正确性不变量

由于 decode（逐 token 生成阶段）与 prefill（首字前的整段计算）针对*同一*数学递推使用结构不同的 kernel，且真实推理服务会混用两者（一段 prefix 在 prefill 中处理一次，随后在 decode 中逐 token 继续，有时还会分块重新处理以支持重试或投机回滚），分块实现的正确性门槛高于「某个例子上数值看起来接近」：

```text
   REQUIRED:  recurrent execution  ≡  chunked execution

              on BOTH of:

              (a)  the output sequence  o_1, o_2, ..., o_L
              (b)  the FINAL STATE  S_L

              agreeing within your chosen numerical tolerance.
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   Matching outputs on one short prompt is NOT sufficient.           │
   │                                                                      │
   │   An order-of-operations bug (Module 03 §3) or an off-by-one in     │
   │   chunk boundary handling can produce outputs that look correct     │
   │   over a short window while the carried STATE has already            │
   │   diverged — and that divergence only becomes visible several       │
   │   tokens later, or after the state crosses a chunk boundary, or      │
   │   after it is checkpointed and resumed in a later request turn.     │
   └────────────────────────────────────────────────────────────────────┘
```

具体的测试设计，展开 [Module 03](Lecture-03.md) 的单步参考实现：

```python
def test_recurrent_vs_chunked(seq_len, chunk_sizes, tol=1e-4):
    x = random_sequence(seq_len)

    ref_outputs, ref_state = run_recurrent(x)              # token-by-token, Module 03's update

    for chunk_size in chunk_sizes:
        chunked_outputs, chunked_state = run_chunked(x, chunk_size)

        assert allclose(chunked_outputs, ref_outputs, tol), \
            f"OUTPUT mismatch at chunk_size={chunk_size}"
        assert allclose(chunked_state, ref_state, tol), \
            f"FINAL STATE mismatch at chunk_size={chunk_size}"   # ← the check most tests skip

        # also verify the STATE agrees at every chunk boundary, not just at the end —
        # this is what catches a boundary bug that happens to cancel out by seq_len
        for boundary in range(chunk_size, seq_len, chunk_size):
            assert allclose(chunked_state_at(boundary), ref_state_at(boundary), tol)
```

在能**与不能**整除 `seq_len` 的 chunk size 上分别运行——只正确处理完整 chunk、对末尾不完整 chunk 处理错误的实现，是一种常见且容易漏掉的失效模式；同样的边界纪律，你在 [Module 06](Lecture-06.md) 中处理 DSA 的不完整尾部 pooling 时还会再次需要。

---

## 检查点

现在应当能够：

1. 从仿射形式推导出 `S_{t+1} = (A_{t+1}A_t)S_{t-1} + (A_{t+1}B_t + B_{t+1})`，并解释它带来了什么。
2. 解释为什么把稠密转移矩阵显式物化会违背组合技巧的初衷。
3. 说出 KDA 子层在递推本身之外的八个阶段。
4. 陈述两部分（输出 + 最终状态）的正确性不变量，并设计一个能捕获边界处理 bug 的测试。

---

## 交付

这是 **[capstone ladder](Lecture-12.md) 的 Stage 3**：用 §1 中的仿射组合为 Module 03 的参考实现扩展一条分块/并行执行路径，然后在上文 `test_recurrent_vs_chunked` 设计中至少在三种 chunk size 上运行，其中包括不能整除测试序列长度的那种。报告输出、最终状态以及每个 chunk 边界处的状态是否一致——而不只是最终状态。

---

## 内容时效

* **不随时间变化：** 仿射组合恒等式及其为何能支持 chunk 并行执行，prefill/decode 的 kernel 家族区分，两部分正确性不变量。
* **与检查点相关：** §3 中确切的阶段列表（哪些 normalization、哪些 gate、卷积 kernel 宽度）应与参考实现实际的 KDA 子层核对——这里给出的顺序只是概念形态，并不保证每个版本中的精确算子顺序。

---

**Next:** [Module 05 — MLA：压缩逐 token 历史 →](Lecture-05.md)


<details>
<summary>English original</summary>

**4. The correctness invariant**

Because decode and prefill use structurally different kernels for the *same* mathematical recurrence, and because real serving mixes both (a prefix processed once during prefill, continued token-by-token during decode, sometimes re-processed in chunks for retries or speculative rollback), a chunked implementation's correctness bar is higher than "the numbers look close on one example":

```text
   REQUIRED:  recurrent execution  ≡  chunked execution

              on BOTH of:

              (a)  the output sequence  o_1, o_2, ..., o_L
              (b)  the FINAL STATE  S_L

              agreeing within your chosen numerical tolerance.
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   Matching outputs on one short prompt is NOT sufficient.           │
   │                                                                      │
   │   An order-of-operations bug (Module 03 §3) or an off-by-one in     │
   │   chunk boundary handling can produce outputs that look correct     │
   │   over a short window while the carried STATE has already            │
   │   diverged — and that divergence only becomes visible several       │
   │   tokens later, or after the state crosses a chunk boundary, or      │
   │   after it is checkpointed and resumed in a later request turn.     │
   └────────────────────────────────────────────────────────────────────┘
```

The concrete test design, expanding on [Module 03](Lecture-03.md)'s single-step reference:

```python
def test_recurrent_vs_chunked(seq_len, chunk_sizes, tol=1e-4):
    x = random_sequence(seq_len)

    ref_outputs, ref_state = run_recurrent(x)              # token-by-token, Module 03's update

    for chunk_size in chunk_sizes:
        chunked_outputs, chunked_state = run_chunked(x, chunk_size)

        assert allclose(chunked_outputs, ref_outputs, tol), \
            f"OUTPUT mismatch at chunk_size={chunk_size}"
        assert allclose(chunked_state, ref_state, tol), \
            f"FINAL STATE mismatch at chunk_size={chunk_size}"   # ← the check most tests skip

        # also verify the STATE agrees at every chunk boundary, not just at the end —
        # this is what catches a boundary bug that happens to cancel out by seq_len
        for boundary in range(chunk_size, seq_len, chunk_size):
            assert allclose(chunked_state_at(boundary), ref_state_at(boundary), tol)
```

Run this across chunk sizes that do **and do not** evenly divide `seq_len` — an implementation that only handles full chunks correctly and mishandles the trailing partial chunk is a common, easy-to-miss failure mode, and the same boundary discipline you will need again for DSA's incomplete-tail pooling in [Module 06](Lecture-06.md).

---

**Checkpoint**

You should now be able to:

1. Derive `S_{t+1} = (A_{t+1}A_t)S_{t-1} + (A_{t+1}B_t + B_{t+1})` from the affine form and explain what it enables.
2. Explain why materializing dense transition matrices would defeat the purpose of the composition trick.
3. Name all eight stages of the KDA sublayer beyond the recurrence itself.
4. State the two-part (output + final state) correctness invariant and design a test that catches a boundary-handling bug.

---

**Ship it**

This is **Stage 3 of the [capstone ladder](Lecture-12.md)**: extend Module 03's reference implementation with a chunked/parallel execution path using the affine composition from §1, then run the `test_recurrent_vs_chunked` design above across at least three chunk sizes, including ones that do not evenly divide your test sequence length. Report agreement on outputs, final state, and state at every chunk boundary — not final state alone.

---

**Current as of**

* **Timeless:** the affine-composition identity and why it enables chunk-parallel execution, the prefill/decode kernel-family distinction, the two-part correctness invariant.
* **Checkpoint-specific:** the exact stage list in §3 (which normalizations, which gates, convolution kernel width) should be verified against the reference implementation's actual KDA sublayer — the ordering shown here is the conceptual shape, not a guarantee of the precise operation sequence in every revision.

---

**Next:** [Module 05 — MLA: Compressing Per-Token History →](Lecture-05.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
