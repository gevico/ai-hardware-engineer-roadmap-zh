---
title: Module 03 — KDA I：Delta 规则递推
description: Module 03 — KDA I：Delta 规则递推
---

# Module 03 — KDA I：Delta 规则递推

**合集：** [GLM-5.3-Flash 架构精通](README.md) | **上一模块：** [← Module 02](Lecture-02.md) | **下一模块：** [Module 04 →](Lecture-04.md)

---

Kimi Delta Attention 是本课程要求你掌握得最深入的机制，原因在于结构：它是这样一种机制——实现上稍有偏差，仍然会产出看起来流畅的文本，仍然能通过随手的冒烟测试，但仍然是一个不同的模型。把更新规则做到完全正确——包括两个**不**可交换的运算的先后顺序——就是本模块的全部内容。

---

## 学习目标

学完本模块后，你应当能够：

1. 说明 KDA 的状态矩阵表示什么，以及它明确不是什么。
2. 凭记忆推导五步更新，并把它展开为闭式递推式。
3. 用代数解释为什么衰减与修正这两个运算不能重排顺序。
4. 手算复现一个更新步的数值算例。
5. 解释为什么该状态是按请求的推理服务数据，而不是检查点参数。

---

## 1. 状态矩阵存的是什么

对一个 attention head，定义 query、key、value 向量以及一个不断演化的状态矩阵：

```text
   q_t, k_t  ∈  R^{d_k}          key/query space
   v_t       ∈  R^{d_v}          value space
   S_t       ∈  R^{d_k × d_v}    the state — an associative memory, NOT a list of past (k, v) pairs
```

GLM-5.3-Flash 配置了 **64 个 KDA head、head 维度为 128**，因此每个 head 维护一个 `128 × 128` 状态矩阵。在本模块中，把 `q_t` 和 `k_t` 视为已经带有模型在递推之前施加的任何归一化与 query 缩放——下面的更新就是递推本身，而不是完整的子层（那是 [Module 04](Lecture-04.md) 的任务）。

关键的定位是：`S_t` 是对迄今所见一切内容的**压缩、固定大小的摘要**，并且被持续覆写——与 KV cache 正好相反，后者靠追加增长，从不覆写。这与 Mamba/SSM 架构所做的“循环状态取代不断增长的缓存”是同一种权衡（[MLSys Deep Dives — Lecture 04](../05-MLSys深度探索/Lecture-04.md) 用具体的状态与 KV cache 对比数值推演了这一论证的一般形式）；KDA 是这一家族中一个具体的 delta 规则实例。

---

## 2. 五步更新

设 `D_t = Diag(α_t)` 为**每个 key 通道提供各自的衰减因子**——而不是整个状态共用一个标量衰减，这正是 KDA 区别于更粗糙的门控线性 attention 变体之处。于是一步更新可读作五个具名运算：

```text
   S̃_t  =  D_t · S_{t−1}              (1) FORGET SELECTIVELY — per-channel decay
   v̂_t  =  S̃_tᵀ · k_t                 (2) PREDICT — what the memory currently says v_t should be
   e_t  =  v_t − v̂_t                  (3) CORRECT — the prediction error
   S_t  =  S̃_t + β_t · k_t · e_tᵀ     (4) WRITE — the correction, scaled by a write gate β_t
   o_t  =  S_tᵀ · q_t                 (5) READ — the updated memory, queried
```

把步骤 (2)–(4) 当作一句话来读，因为它是 delta 规则的概念核心：**“我的记忆对这个 key 已经预测出什么，还有哪些有待修正？”** 普通的加性线性 attention 记忆则会做 `S_t = S_{t-1} + k_t v_tᵀ`——盲目地永远累积关联，没有任何机制去察觉或修正过时的关联。KDA 的写入量正比于 *误差*，而不是原始 value，这才使得单个状态矩阵能够跟踪一个变化的世界，而不只是对全部历史取平均。

---


<details>
<summary>English original</summary>

**Module 03 — KDA I: The Delta-Rule Recurrence**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 02](Lecture-02.md) | **Next:** [Module 04 →](Lecture-04.md)

---

Kimi Delta Attention is the mechanism this course asks you to master most deeply, and the reason is structural: it is the one mechanism where a subtly wrong implementation still produces fluent-looking text, still passes a casual smoke test, and is still a different model. Getting the update rule exactly right — including the order of two operations that do **not** commute — is the entire module.

---

**Learning objectives**

By the end of this module you should be able to:

1. State what KDA's state matrix represents, and what it explicitly is not.
2. Derive the five-step update and expand it into the closed-form recurrence, from memory.
3. Explain, using the algebra, why the decay and correction operations cannot be reordered.
4. Reproduce a worked numeric example of one update step by hand.
5. Explain why the state is per-request serving data, not a checkpoint parameter.

---

**1. What the state matrix stores**

For one attention head, define query, key, and value vectors and an evolving state matrix:

```text
   q_t, k_t  ∈  R^{d_k}          key/query space
   v_t       ∈  R^{d_v}          value space
   S_t       ∈  R^{d_k × d_v}    the state — an associative memory, NOT a list of past (k, v) pairs
```

GLM-5.3-Flash configures **64 KDA heads with head dimension 128**, so each head maintains a `128 × 128` state matrix. Throughout this module, treat `q_t` and `k_t` as already carrying whatever normalization and query scaling the model applies before the recurrence — the update below is the recurrence itself, not the full sublayer (that is [Module 04](Lecture-04.md)'s job).

The critical framing: `S_t` is a **compressed, fixed-size summary** of everything seen so far, continuously overwritten — the opposite of a KV cache, which grows by appending and never overwrites. This is the same "recurrent state replaces a growing cache" trade that Mamba/SSM architectures make ([MLSys Deep Dives — Lecture 04](../05-MLSys深度探索/Lecture-04.md) works the general version of this argument with concrete state-vs-KV-cache numbers); KDA is one specific, delta-rule instance of that family.

---

**2. The five-step update**

Let `D_t = Diag(α_t)` supply a **separate decay factor for each key channel** — not one scalar decay for the whole state, which is what distinguishes KDA from a coarser gated linear attention variant. Then one step reads as five named operations:

```text
   S̃_t  =  D_t · S_{t−1}              (1) FORGET SELECTIVELY — per-channel decay
   v̂_t  =  S̃_tᵀ · k_t                 (2) PREDICT — what the memory currently says v_t should be
   e_t  =  v_t − v̂_t                  (3) CORRECT — the prediction error
   S_t  =  S̃_t + β_t · k_t · e_tᵀ     (4) WRITE — the correction, scaled by a write gate β_t
   o_t  =  S_tᵀ · q_t                 (5) READ — the updated memory, queried
```

Read step (2)–(4) as a sentence, because it is the conceptual heart of the delta rule: **"what does my memory already predict for this key, and what remains to be corrected?"** An ordinary additive linear-attention memory would instead do `S_t = S_{t-1} + k_t v_tᵀ` — blindly accumulating associations forever, with no mechanism to notice or fix a stale one. KDA's write is proportional to the *error*, not to the raw value, which is what lets a single state matrix track a changing world instead of just averaging over its entire history.

---

</details>

## 3. 展开为闭式 —— 以及为何顺序重要

把 (1)–(3) 代入 (4) 并完全展开：

```text
   S_t  =  S̃_t + β_t · k_t · e_tᵀ
        =  S̃_t + β_t · k_t · (v_t − v̂_t)ᵀ                         [substitute e_t]
        =  S̃_t + β_t · k_t · v_tᵀ  −  β_t · k_t · v̂_tᵀ             [distribute]
```

现在展开 `v̂_tᵀ`。由于 `v̂_t = S̃_tᵀ k_t`，其转置为 `v̂_tᵀ = k_tᵀ S̃_t` —— 这是标准的乘积转置翻转：

```text
   S_t  =  S̃_t + β_t · k_t · v_tᵀ  −  β_t · k_t · (k_tᵀ S̃_t)
        =  S̃_t  −  β_t · k_t · k_tᵀ · S̃_t  +  β_t · k_t · v_tᵀ
        =  (I − β_t · k_t · k_tᵀ) · S̃_t  +  β_t · k_t · v_tᵀ
```

代入 `S̃_t = D_t · S_{t-1}` 得到闭式：

```text
   ┌──────────────────────────────────────────────────────────────────────┐
   │                                                                        │
   │   S_t  =  (I − β_t · k_t · k_tᵀ) · D_t · S_{t−1}  +  β_t · k_t · v_tᵀ  │
   │                                                                        │
   └──────────────────────────────────────────────────────────────────────┘
```

这就是 KDA 递推，按实现实际施加的顺序写出。下面给出整个推导所要论证的那条警告：

```text
   IN GENERAL:      (I − β k kᵀ) · D     ≠     D · (I − β k kᵀ)

   because k depends on t and D_t is diagonal but NOT a multiple of the identity —
   the two matrices do not commute except in special cases.
```

两个表达式在纸面上几乎一模一样，而交换其顺序的重新实现照样能编译、运行，并且产出一个状态矩阵，它从第一个具有非零 decay 和非零 correction 的 token 起就微妙地错误 —— 该错误会在其后的每一步中静默累积，因为每个 `S_t` 都喂给下一个。这正是 [Module 04](Lecture-04.md) 要求对照 **输出与最终状态两者**（而非仅输出）来验证分块实现的原因：运算顺序的 bug 可能让短序列的输出看起来几乎正确，而携带的状态早已发散。

---

## 4. 一个手算实例

取 decay 关闭（`D_t = I`，故 `S̃_t = S_{t-1}`）、一个 `2×2` 状态，以及：

```text
   S_{t−1}  =  [ 2  0 ]         k_t  =  [ 1 ]         v_t  =  [ 6 ]         β_t = 1/2
              [ 0  4 ]                  [ 0 ]                 [ 1 ]
```

**第 2 步 —— predict。** `v̂_t = S̃_tᵀ k_t`。由于 `S_{t-1}` 是对角阵，其转置等于自身：

```text
   v̂_t  =  [ 2  0 ] [ 1 ]  =  [ 2 ]
           [ 0  4 ] [ 0 ]     [ 0 ]
```

**第 3 步 —— correct。**

```text
   e_t  =  v_t − v̂_t  =  [ 6 ] − [ 2 ]  =  [ 4 ]
                          [ 1 ]   [ 0 ]     [ 1 ]
```

**第 4 步 —— write。** `S_t = S̃_t + β_t · k_t · e_tᵀ`：

```text
   k_t · e_tᵀ  =  [ 1 ] [ 4  1 ]  =  [ 4  1 ]
                  [ 0 ]              [ 0  0 ]

   β_t · k_t · e_tᵀ  =  [ 2   0.5 ]
                        [ 0    0  ]

   S_t  =  [ 2  0 ] + [ 2   0.5 ]  =  [ 4   0.5 ]
           [ 0  4 ]   [ 0    0  ]     [ 0    4  ]
```

```text
   ┌──────────────────────────────────────────────────────────────┐
   │   S_t  =  [ 4   0.5 ]                                        │
   │           [ 0    4  ]                                        │
   │                                                                │
   │   The association touched by k_t moved HALFWAY toward the     │
   │   requested value (4 → nearly the corrected direction, with    │
   │   an off-diagonal term of 0.5 appearing where k_t and e_t      │
   │   interact). The unrelated row (second row) is untouched —     │
   │   k_t = [1, 0] never reads or writes it.                       │
   └──────────────────────────────────────────────────────────────┘
```

在碰真实模型之前，把这个例子原样用代码复现，作为你的第一次正确性检查 —— 如果你的五行实现得不到这个 `S_t`，就不要继续到 [Module 04](Lecture-04.md)。

---


<details>
<summary>English original</summary>

**3. Expanding to the closed form — and why order matters**

Substitute (1)–(3) into (4) and expand fully:

```text
   S_t  =  S̃_t + β_t · k_t · e_tᵀ
        =  S̃_t + β_t · k_t · (v_t − v̂_t)ᵀ                         [substitute e_t]
        =  S̃_t + β_t · k_t · v_tᵀ  −  β_t · k_t · v̂_tᵀ             [distribute]
```

Now expand `v̂_tᵀ`. Since `v̂_t = S̃_tᵀ k_t`, its transpose is `v̂_tᵀ = k_tᵀ S̃_t` — a standard transpose-of-a-product flip:

```text
   S_t  =  S̃_t + β_t · k_t · v_tᵀ  −  β_t · k_t · (k_tᵀ S̃_t)
        =  S̃_t  −  β_t · k_t · k_tᵀ · S̃_t  +  β_t · k_t · v_tᵀ
        =  (I − β_t · k_t · k_tᵀ) · S̃_t  +  β_t · k_t · v_tᵀ
```

Substituting `S̃_t = D_t · S_{t-1}` gives the closed form:

```text
   ┌──────────────────────────────────────────────────────────────────────┐
   │                                                                        │
   │   S_t  =  (I − β_t · k_t · k_tᵀ) · D_t · S_{t−1}  +  β_t · k_t · v_tᵀ  │
   │                                                                        │
   └──────────────────────────────────────────────────────────────────────┘
```

This is the KDA recurrence in the order an implementation actually applies it. Now the warning that this whole derivation exists to justify:

```text
   IN GENERAL:      (I − β k kᵀ) · D     ≠     D · (I − β k kᵀ)

   because k depends on t and D_t is diagonal but NOT a multiple of the identity —
   the two matrices do not commute except in special cases.
```

Both expressions look almost identical on the page, and a re-implementation that swaps their order will compile, run, and produce a state matrix that is subtly wrong from the very first token with nonzero decay and a nonzero correction — an error that compounds silently across every subsequent step, because each `S_t` feeds the next. This is precisely why [Module 04](Lecture-04.md) requires validating a chunked implementation against **both output and final state**, not output alone: an order-of-operations bug can leave short-sequence outputs looking nearly correct while the carried state has already diverged.

---

**4. A worked example, by hand**

Take decay disabled (`D_t = I`, so `S̃_t = S_{t-1}`), a `2×2` state, and:

```text
   S_{t−1}  =  [ 2  0 ]         k_t  =  [ 1 ]         v_t  =  [ 6 ]         β_t = 1/2
              [ 0  4 ]                  [ 0 ]                 [ 1 ]
```

**Step 2 — predict.** `v̂_t = S̃_tᵀ k_t`. Since `S_{t-1}` is diagonal, its transpose equals itself:

```text
   v̂_t  =  [ 2  0 ] [ 1 ]  =  [ 2 ]
           [ 0  4 ] [ 0 ]     [ 0 ]
```

**Step 3 — correct.**

```text
   e_t  =  v_t − v̂_t  =  [ 6 ] − [ 2 ]  =  [ 4 ]
                          [ 1 ]   [ 0 ]     [ 1 ]
```

**Step 4 — write.** `S_t = S̃_t + β_t · k_t · e_tᵀ`:

```text
   k_t · e_tᵀ  =  [ 1 ] [ 4  1 ]  =  [ 4  1 ]
                  [ 0 ]              [ 0  0 ]

   β_t · k_t · e_tᵀ  =  [ 2   0.5 ]
                        [ 0    0  ]

   S_t  =  [ 2  0 ] + [ 2   0.5 ]  =  [ 4   0.5 ]
           [ 0  4 ]   [ 0    0  ]     [ 0    4  ]
```

```text
   ┌──────────────────────────────────────────────────────────────┐
   │   S_t  =  [ 4   0.5 ]                                        │
   │           [ 0    4  ]                                        │
   │                                                                │
   │   The association touched by k_t moved HALFWAY toward the     │
   │   requested value (4 → nearly the corrected direction, with    │
   │   an off-diagonal term of 0.5 appearing where k_t and e_t      │
   │   interact). The unrelated row (second row) is untouched —     │
   │   k_t = [1, 0] never reads or writes it.                       │
   └──────────────────────────────────────────────────────────────┘
```

Reproduce this exact example in code as your first correctness check before touching a real model — if your five-line implementation doesn't produce this `S_t`, do not proceed to [Module 04](Lecture-04.md).

---

</details>

## 5. 状态 vs. checkpoint 权重 —— 一个对推理服务至关重要的区别

```text
   CHECKPOINT WEIGHTS                        RECURRENT STATE  S_t
   ─────────────────────────                 ──────────────────────────────
   W_q, W_k, W_v, decay/write gate params     one S_t PER (request, head, layer)
   fixed after training                       changes every token, every request
   shared across every request                belongs to a specific sequence prefix
   loaded once                                allocated, cached, evicted per-request
```

`S_t` 是**请求作用域内的推理服务状态**，与常规的 KV cache 槽位完全类似 —— 而不是对模型的训练更新。这一区别对任何构建推理服务系统的人都有直接而实际的后果：`S_t` 必须被追踪、在多轮对话的各轮之间被缓存、被正确逐出，并在任何复制或分叉请求的操作（投机回滚、前缀共享、请求迁移）中被正确复制或重建。把它当作「前向传播一返回就无关紧要」的临时暂存空间，是一个迟早会爆发的推理服务 bug —— [Module 08](Lecture-08.md) 正好覆盖了投机解码回滚中的这一失效模式，而 [Module 09](Lecture-09.md) 估算了在完整的多 GPU 部署中该状态所需的 VRAM 预算。

---

## 检查点

你现在应该能够：

1. 说明 `S_t` 表示什么，以及为什么它不是一组过往的 key/value 对。
2. 凭记忆写出五步更新，并将其展开为闭式递推式。
3. 用乘积转置恒等式解释闭式形式究竟从何而来。
4. 陈述非交换性警告，并描述它所预测的一个具体 bug。
5. 在不回看本页的情况下复现那个手算数值示例。
6. 解释为什么 `S_t` 必须被视为按请求划分的推理服务状态，而非临时的暂存内存。

---

## 动手交付

这是**Stage 2，也是 [capstone ladder](Lecture-12.md) 的第一个里程碑**：把五步更新实现为一个小的 FP32 参考实现（NumPy 足够 —— 不用 GPU、不用批处理、只用单头），并精确复现 §4 的手算示例。然后逐步处理一段短的随机序列，并在每一步打印得到的 `S_t`。在这个参考实现存在、且与手算示例逐比特一致（在浮点舍入范围内）之前，不要进入 [Module 04](Lecture-04.md)。

---

## 截至当前的时效性

* **长期有效：** 五步更新、其闭式展开、衰减算子与修正算子的非交换性，以及 delta-rule 的动机（写入误差，而非原始值）—— 这就是源自底层 delta-rule/KDA 形式的 KDA 递推式，此处以对实现友好的运算顺序表达。
* **检查点相关：** 64 个头、head 维度 128（即每个头一个 `128×128` 状态）是该检查点配置的属性。

---

**下一节：** [Module 04 —— KDA II：分块并行与完整子层 →](Lecture-04.md)


<details>
<summary>English original</summary>

**5. State vs. checkpoint weights — a distinction that matters for serving**

```text
   CHECKPOINT WEIGHTS                        RECURRENT STATE  S_t
   ─────────────────────────                 ──────────────────────────────
   W_q, W_k, W_v, decay/write gate params     one S_t PER (request, head, layer)
   fixed after training                       changes every token, every request
   shared across every request                belongs to a specific sequence prefix
   loaded once                                allocated, cached, evicted per-request
```

`S_t` is **request-scoped serving state**, exactly analogous to a conventional KV cache slot — not a training update to the model. This distinction has an immediate, practical consequence for anyone building a serving system: `S_t` must be tracked, cached across a multi-turn conversation's turns, correctly evicted, and correctly copied or reconstructed on any operation that duplicates or forks a request (speculative rollback, prefix sharing, request migration). Treating it as ephemeral scratch space that "doesn't matter once the forward pass returns" is a serving bug waiting to happen — [Module 08](Lecture-08.md) covers exactly this failure mode for speculative decoding rollback, and [Module 09](Lecture-09.md) sizes the VRAM budget this state requires across a full multi-GPU deployment.

---

**Checkpoint**

You should now be able to:

1. State what `S_t` represents and why it is not a list of past key/value pairs.
2. Write the five-step update from memory and expand it into the closed-form recurrence.
3. Explain, using the transpose-of-a-product identity, exactly where the closed form comes from.
4. State the non-commutativity warning and describe a concrete bug it predicts.
5. Reproduce the worked numeric example without referring back to this page.
6. Explain why `S_t` must be treated as per-request serving state rather than transient scratch memory.

---

**Ship it**

This is **Stage 2 and the first milestone of the [capstone ladder](Lecture-12.md)**: implement the five-step update as a small FP32 reference (NumPy is sufficient — no GPU, no batching, one head) and reproduce §4's worked example exactly. Then process a short random sequence step by step and print the resulting `S_t` at each step. Do not proceed to [Module 04](Lecture-04.md) until this reference implementation exists and matches the hand-worked example bit for bit (up to floating-point rounding).

---

**Current as of**

* **Timeless:** the five-step update, its closed-form expansion, the non-commutativity of the decay and correction operators, and the delta-rule motivation (write the error, not the raw value) — this is the KDA recurrence from the underlying delta-rule/KDA formulation, expressed here in an implementation-friendly operation order.
* **Checkpoint-specific:** 64 heads, head dimension 128 (so a `128×128` state per head) are properties of this checkpoint's configuration.

---

**Next:** [Module 04 — KDA II: Chunked Parallelism & the Full Sublayer →](Lecture-04.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
