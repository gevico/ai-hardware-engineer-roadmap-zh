---
title: 模块 11——正确性即架构精通
description: 模块 11——正确性即架构精通
---

# 模块 11——正确性即架构精通

**合集：** [GLM-5.3-Flash Architecture Mastery](README.md) | **上一模块：** [← 模块 10](Lecture-10.md) | **下一模块：** [模块 12 →](Lecture-12.md)

---

本课程的每个模块都点出过一种看似合理、实则错误的机制实现方式——router 的修正偏置、KDA 的算子顺序、MLA 的 softmax scale、DSA 的 pool 补全因果性、mHC 的近似双随机容差、混合回滚状态。本模块把它们归拢为一条准则：**先定义一项改动必须保持的契约，再测量它是否够快。**

---

## 学习目标

学完本模块后，你应该能够：

1. 区分同一检查点下的 kernel 替换与新的量化：它们是两类不同的实验，需要两种不同的评估。
2. 为该架构构建完整的逐机制正确性测试矩阵。
3. 陈述 prefix/chunk/continuation 不变量，并解释它能捕获哪一类逐 token 输出比较漏掉的 bug。
4. 在 benchmark 之前先约定数值契约，并解释为什么“看起来合理”与“必须逐位相同”通常都是错误的门槛。

---

## 1. 两类不同的实验，两种不同的评估

```text
   SAME-CHECKPOINT KERNEL REPLACEMENT           NEW QUANTIZATION / NEW PRECISION
   ──────────────────────────────────           ──────────────────────────────────
   same weights, same math, different            different NUMBERS — a deliberate,
   CODE PATH computing it                        bounded change to the computation

   compare against the EXISTING computation       needs everything the left column
   with a numerical-tolerance check                needs, PLUS model-quality evaluation
   (Module 04's output+state agreement,            against the higher-precision reference
   Module 05's expanded-vs-absorbed check)          (Hardware-Aware LLM Quantization —
                                                     Module 08's KL/acceptance-length gate)
```

把它们当成同一种测试是一种常见的捷径，其失效模式很具体：一套只检查“这段文本看起来是否合理”的 kernel 替换测试，会欣然放过一个代数上错误的改动（softmax scale 写错、算子顺序颠倒），只要这种错误足够细微、不至于破坏流畅性——而这恰恰是本课程反复指认为危险的那类 bug，因为输出流畅并不证明实现正确。

---

## 2. 完整的正确性矩阵

| 领域 | 必需的测试 |
|---|---|
| **MoE**（混合专家模型） | 选中的专家 ID 与参考一致；混合权重使用 *原始 sigmoid score*，而非 score 加 bias（[模块 02 §2](Lecture-02.md)）；gate clamp 仅设上界，up-projection clamp 为双侧（[模块 02 §3](Lecture-02.md)） |
| **KDA** | 循环执行与分块执行在**输出与最终状态**两方面都一致（[模块 04 §4](Lecture-04.md)）；算子顺序匹配 `(I − βkkᵀ)D`，而非 `D(I − βkkᵀ)`（[模块 03 §3](Lecture-03.md)） |
| **KPool** | pool 边界长度（3、4、5、7、8、9——[模块 06 §3](Lecture-06.md)）与不完整尾部处理都能给出正确的选择；缓冲区按 2,051 个 slot 定尺寸，而非 2,048 |
| **Padding / packing** | KDA 的循环、MLA 的缓存、DSA 的池化在 packed sequence 边界处均无跨文档状态泄漏 |
| **MLA** | 在相同输入下，展开形式与吸收形式的 attention 产生完全相同的分数（而不只是成比例的分数）——softmax scale 不得悄悄偏移（[模块 05 §3](Lecture-05.md)） |
| **mHC** | 流布局、collapse/mix 方向与归一化顺序均与参考一致；`R` 的行/列和落在实际使用的 Sinkhorn 迭代容差之内，而非精确的 1.0（[模块 07 §2](Lecture-07.md)） |
| **推理服务状态** | prefix cache 命中、请求重排序、分支、取消与投机回滚，都能一致地对齐 KDA 状态、卷积状态、MLA 缓存和 DSA 元数据（[模块 08 §3](Lecture-08.md)） |
| **精度** | 覆盖长上下文累积行为、极端 gate 值（±10 的 clamp——[模块 02 §3](Lecture-02.md)）以及近乎并列的布线分数，而不只是典型情形的输入 |

这张表的每一行都直接回溯到本课程前文某处具体的推导——这个矩阵不是通用检查清单，它是真正推导过每个机制、而非仅仅读过之后自然得出的结果。

---


<details>
<summary>English original</summary>

**Module 11 — Correctness as Architecture Mastery**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 10](Lecture-10.md) | **Next:** [Module 12 →](Lecture-12.md)

---

Every module in this course has flagged a specific way to implement its mechanism plausibly and wrong — the router's correction bias, the KDA operator ordering, MLA's softmax scale, DSA's pool-completion causality, mHC's approximate doubly-stochastic tolerance, hybrid rollback state. This module collects them into one discipline: **define the contract a change must preserve before you measure whether it's fast.**

---

**Learning objectives**

By the end of this module you should be able to:

1. Distinguish a same-checkpoint kernel replacement from a new quantization as two different kinds of experiment with two different required evaluations.
2. Build the full per-mechanism correctness test matrix for this architecture.
3. State the prefix/chunk/continuation invariant and explain what class of bug it catches that per-token output comparison misses.
4. Specify a numerical contract before benchmarking, and explain why both "looks reasonable" and "must be bitwise identical" are usually the wrong bar.

---

**1. Two different experiments, two different evaluations**

```text
   SAME-CHECKPOINT KERNEL REPLACEMENT           NEW QUANTIZATION / NEW PRECISION
   ──────────────────────────────────           ──────────────────────────────────
   same weights, same math, different            different NUMBERS — a deliberate,
   CODE PATH computing it                        bounded change to the computation

   compare against the EXISTING computation       needs everything the left column
   with a numerical-tolerance check                needs, PLUS model-quality evaluation
   (Module 04's output+state agreement,            against the higher-precision reference
   Module 05's expanded-vs-absorbed check)          (Hardware-Aware LLM Quantization —
                                                     Module 08's KL/acceptance-length gate)
```

Treating these as one kind of test is a common shortcut with a specific failure mode: a kernel-replacement test suite that only checks "does this look like reasonable text" will happily pass a change that is algebraically wrong (wrong softmax scale, swapped operator order) as long as the wrongness is subtle enough not to break fluency — exactly the class of bug this course has repeatedly named as the dangerous one, because fluent output is not evidence of a correct implementation.

---

**2. The full correctness matrix**

| Area | Required tests |
|---|---|
| **MoE** | Selected expert IDs match reference; mixture weights use the *raw sigmoid score*, not score-plus-bias ([Module 02 §2](Lecture-02.md)); gate clamp is upper-only, up-projection clamp is two-sided ([Module 02 §3](Lecture-02.md)) |
| **KDA** | Recurrent and chunked execution agree on **both output and final state** ([Module 04 §4](Lecture-04.md)); operator order matches `(I − βkkᵀ)D`, not `D(I − βkkᵀ)` ([Module 03 §3](Lecture-03.md)) |
| **KPool** | Pool-boundary lengths (3, 4, 5, 7, 8, 9 — [Module 06 §3](Lecture-06.md)) and incomplete-tail handling produce correct selections; buffers sized to 2,051 slots, not 2,048 |
| **Padding / packing** | No cross-document state leakage in KDA's recurrence, MLA's cache, or DSA's pooling across packed-sequence boundaries |
| **MLA** | Expanded-form and absorbed-form attention produce identical scores (not merely proportional ones) on identical inputs — the softmax scale must not silently move ([Module 05 §3](Lecture-05.md)) |
| **mHC** | Stream layout, collapse/mix orientation, and normalization order match reference; `R`'s row/column sums fall within the *actual* Sinkhorn-iteration tolerance used, not exact 1.0 ([Module 07 §2](Lecture-07.md)) |
| **Serving state** | Prefix-cache hits, request reordering, branching, cancellation, and speculative rollback all reconcile KDA state, convolution state, MLA cache, and DSA metadata consistently ([Module 08 §3](Lecture-08.md)) |
| **Precision** | Long-context accumulation behavior, extreme-gate values (the ±10 clamps — [Module 02 §3](Lecture-02.md)), and near-tied routing scores are covered, not just typical-case inputs |

Every row of this table is a direct callback to a specific derivation earlier in the course — this matrix is not a generic checklist, it is what falls out of having actually derived each mechanism instead of only reading about it.

---

</details>

## 3. 可推广到所有机制的不变量

无论 prefix 是*如何*被处理的，都应满足同一个性质：

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   Processing the same prefix in ONE PREFILL PASS, in MULTIPLE       │
   │   CHUNKS, or through a CACHED CONTINUATION (prefix reuse across      │
   │   turns) should lead to EQUIVALENT continuation state and logits,    │
   │   within a stated tolerance.                                         │
   └────────────────────────────────────────────────────────────────────┘
```

这一条不变量一次性涵盖了本课程中多个针对具体机制的检查：

```text
   ONE PREFILL vs. CHUNKED PREFILL       →  tests KDA's chunk composition (Module 04)
                                             and DSA's pool-boundary handling (Module 06)

   CACHED CONTINUATION vs. FULL REPLAY   →  tests whether prefix-cache reuse correctly
                                             restored EVERY piece of hybrid state (Module 08),
                                             not just the token-indexed MLA cache

   ANY OF THE ABOVE, POST-ROLLBACK       →  tests the rollback reconciliation itself
                                             (Module 08 §3) against a from-scratch run
```

围绕这一条不变量构建的测试套件，在 [Module 06 §3](Lecture-06.md) 的边界长度上、以及 [Module 08 §3](Lecture-08.md) 的状态清单上加以执行，就能捕获本课程作为单独的、机制特有的风险所列出的大部分 bug —— 因为它们中的多数，在底层都是同一个失效：**某块状态没能挺过一条本应与参考路径等价的代码路径。**

---

## 4. 先定契约，再做 benchmark

两个极端都不是正确的默认做法：

```text
   "the output looks reasonable"     →  too weak. Catches nothing this course has
                                          flagged — a wrong router weighting, a
                                          swapped operator order, and a mis-scaled
                                          softmax can ALL still look reasonable.

   "must be bitwise identical"       →  too strong, for the wrong reason. Algebraically
                                          EQUIVALENT floating-point reductions (e.g.
                                          summing in a different order, Module 05's
                                          expanded vs. absorbed forms) are not
                                          bitwise identical and should not be demanded
                                          to be — that bar rejects correct code.
```

正确的做法是**在运行比较之前就写明容差和度量指标**，并与你所测试的变更类型相匹配：

```text
   kernel replacement, same checkpoint   :  numerical tolerance on outputs AND state
                                             (Module 04 §4's exact test design)

   new quantization / precision change   :  KL divergence + top-1 agreement against
                                             the higher-precision reference, exactly as
                                             Hardware-Aware LLM Quantization — Module 08
                                             specifies, PLUS this course's state-agreement
                                             checks — a quantization change to this
                                             architecture needs BOTH evaluations, not
                                             either one alone
```

在*看到结果之前*就锁定契约，才能使随后的比较成为一次实验，而不是一次自我合理化 —— 这正是 [Hardware-Aware LLM Quantization — Module 12](../03-硬件感知LLM量化/Lecture-12.md) 总体主张的纪律，这里应用到一个模型上：它的“还能不能正常工作”这一问题涉及五个独立机制，每个机制都要对该问题给出自己的答案。

---

## 检查点

现在你应该能够：

1. 解释为什么 kernel 替换测试和量化评估测试是两种不同的实验，需要不同的证据。
2. 复述正确性矩阵的八个领域，并针对每一个，说出它旨在捕获的具体失效模式。
3. 陈述 prefix/chunk/continuation 不变量，并解释它涵盖了哪三种测试场景。
4. 解释为什么“看起来合理”和“逐位一致”通常都是错误的标准，并说明两者之间应该是什么。

---


<details>
<summary>English original</summary>

**3. The invariant that generalizes across every mechanism**

One property should hold regardless of *how* a prefix was processed:

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   Processing the same prefix in ONE PREFILL PASS, in MULTIPLE       │
   │   CHUNKS, or through a CACHED CONTINUATION (prefix reuse across      │
   │   turns) should lead to EQUIVALENT continuation state and logits,    │
   │   within a stated tolerance.                                         │
   └────────────────────────────────────────────────────────────────────┘
```

This single invariant subsumes several of this course's mechanism-specific checks at once:

```text
   ONE PREFILL vs. CHUNKED PREFILL       →  tests KDA's chunk composition (Module 04)
                                             and DSA's pool-boundary handling (Module 06)

   CACHED CONTINUATION vs. FULL REPLAY   →  tests whether prefix-cache reuse correctly
                                             restored EVERY piece of hybrid state (Module 08),
                                             not just the token-indexed MLA cache

   ANY OF THE ABOVE, POST-ROLLBACK       →  tests the rollback reconciliation itself
                                             (Module 08 §3) against a from-scratch run
```

A test suite built around this one invariant, exercised at the boundary lengths from [Module 06 §3](Lecture-06.md) and across the state inventory from [Module 08 §3](Lecture-08.md), catches a large fraction of the bugs this course has named as individual, mechanism-specific risks — because most of them are, underneath, the same failure: **some piece of state didn't survive a code path that was supposed to be equivalent to the reference path.**

---

**4. Specify the contract before you benchmark**

Neither extreme is the right default:

```text
   "the output looks reasonable"     →  too weak. Catches nothing this course has
                                          flagged — a wrong router weighting, a
                                          swapped operator order, and a mis-scaled
                                          softmax can ALL still look reasonable.

   "must be bitwise identical"       →  too strong, for the wrong reason. Algebraically
                                          EQUIVALENT floating-point reductions (e.g.
                                          summing in a different order, Module 05's
                                          expanded vs. absorbed forms) are not
                                          bitwise identical and should not be demanded
                                          to be — that bar rejects correct code.
```

The right move is to **state the tolerance and the metric before running the comparison**, matched to what kind of change you're testing:

```text
   kernel replacement, same checkpoint   :  numerical tolerance on outputs AND state
                                             (Module 04 §4's exact test design)

   new quantization / precision change   :  KL divergence + top-1 agreement against
                                             the higher-precision reference, exactly as
                                             Hardware-Aware LLM Quantization — Module 08
                                             specifies, PLUS this course's state-agreement
                                             checks — a quantization change to this
                                             architecture needs BOTH evaluations, not
                                             either one alone
```

Committing to the contract *before* seeing results is what makes the subsequent comparison an experiment rather than a rationalization — precisely the discipline [Hardware-Aware LLM Quantization — Module 12](../03-硬件感知LLM量化/Lecture-12.md) argues for generally, applied here to a model where "does it still work" has five separate mechanisms that each need their own answer to that question.

---

**Checkpoint**

You should now be able to:

1. Explain why a kernel-replacement test and a quantization-evaluation test are different experiments requiring different evidence.
2. Recite the correctness matrix's eight areas and, for each, name the specific failure mode it exists to catch.
3. State the prefix/chunk/continuation invariant and explain which three testing scenarios it subsumes.
4. Explain why both "looks reasonable" and "bitwise identical" are usually the wrong bar, and state what belongs between them.

---

</details>

## 交付

把**完整正确性矩阵构建成可执行的测试套件**——§2 表格的每一行对应一个测试组——并加入来自 §3 的 prefix/chunk/continuation 不变量，作为横切测试针对每个机制的 state 运行，而不只是 MLA 的。对每个测试，都要在运行*之前*写下容差与度量指标，遵循 §4 的纪律。本套件正是 [Module 12](Lecture-12.md) 的 capstone 用来把关每一项优化的依据——不先通过它，任何东西都过不了本课程的纪律。

---

## 时效说明

* **不随时间变化：** kernel 替换与量化之间的区分、正确性矩阵（每一行都源自更早某个模块的具体发现）、prefix/chunk/continuation 不变量，以及在 benchmark 之前先写明数值契约的纪律。
* **与检查点相关：** 矩阵中引用的具体 clamp 值、容差阈值与边界长度都可追溯到本检查点的配置——若要把该矩阵应用到其他修订版本，请从对应的更早模块重新推导这些值。

---

**接下来：** [Module 12 — Capstone: The KDA-First Mastery Ladder →](Lecture-12.md)


<details>
<summary>English original</summary>

**Ship it**

Build the **full correctness matrix as an executable test suite** — one test group per row of §2's table — and add the prefix/chunk/continuation invariant from §3 as a cross-cutting test run against every mechanism's state, not just MLA's. For each test, write down the tolerance and metric *before* running it, per §4's discipline. This suite is what [Module 12](Lecture-12.md)'s capstone gates every optimization against — nothing ships past this course's discipline without passing it first.

---

**Current as of**

* **Timeless:** the kernel-replacement-vs-quantization distinction, the correctness matrix (each row derived from an earlier module's specific finding), the prefix/chunk/continuation invariant, and the discipline of specifying a numerical contract before benchmarking.
* **Checkpoint-specific:** the exact clamp values, tolerance thresholds, and boundary lengths cited in the matrix trace back to this checkpoint's configuration — re-derive them from the corresponding earlier module if you are applying this matrix to a different revision.

---

**Next:** [Module 12 — Capstone: The KDA-First Mastery Ladder →](Lecture-12.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-11.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-11.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
