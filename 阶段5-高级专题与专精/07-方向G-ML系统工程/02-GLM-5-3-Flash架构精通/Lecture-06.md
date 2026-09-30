---
title: 模块 06 — DSA 与 KPool：选择性检索
description: 模块 06 — DSA 与 KPool：选择性检索
---

# 模块 06 — DSA 与 KPool：选择性检索

**合集：**[GLM-5.3-Flash 架构精通](README.md) | **上一节：**[← 模块 05](Lecture-05.md) | **下一节：**[模块 07 →](Lecture-07.md)

---

[模块 05](Lecture-05.md) 回答了“每个历史 token 是如何表示的？”本模块回答另一个问题：**这个 query 实际能读取哪些历史位置？** DeepSeek 稀疏 attention（DSA）把廉价的选择机制与其所门控的昂贵 attention 计算分离开——而 GLM-5.3-Flash 的具体实现，即池化索引（pooled indexing），其边界行为很容易出微妙差错。

---

## 学习目标

学完本模块后，你应当能够：

1. 区分 indexer 的维度与主 MLA 头的维度，并解释二者为何不同。
2. 推导 indexer 的选择预算，包括尾部不完整的情形。
3. 解释池化索引可能引发的特定因果可见性 bug，并指出它集中在哪些序列长度上。
4. 推导为何固定的 top-k 选择预算并不意味着常数时间 decode。
5. 解释为何池化是一种选择机制，而非缓存大小的缩减。

---

## 1. Indexer 维度不是主 attention 维度

```text
   Indexer property          Value               Main MLA (Module 05, for contrast)
   ────────────────────      ──────              ────────────────────────────────
   Indexer heads              32                  64 main heads
   Indexer head dimension     128                 256-dim main K/Q/V heads
   Pool width                 4 tokens            (no pooling — token-indexed)
   Main selection budget      2,048 positions     (no cap — full causal history)
   Incomplete-tail handling   enabled             n/a
```

这是两套独立机制，配有两套独立参数。indexer 的职责是在*整个*因果历史上做廉价的近似打分，找出哪些位置值得完整读取；随后主 MLA 路径只对 indexer 选中的位置做昂贵的精确计算。把二者混为一谈——例如以为 indexer 的池宽能说明主 attention 缓存的任何信息——是对该机制最常见的误读，§4 会明确点出这一点。

---

## 2. 选择预算，及其陷阱

indexer 把 token 分组成**大小为 4 的池**，为每个*完整*池构建一个学习得到的、通道加权的表示，对这些池表示打分，并选出得分最高的若干池直至预算上限：

```text
   main selection budget  =  2,048 positions
   pool width             =  4 tokens

   complete pools selectable  =  2,048 / 4  =  512 pools
```

随后，选中的池会被**展开回其原始组成 token 位置**，供主 attention 计算使用——池化只用于让打分变廉价，并不减少主 attention 实际读取的内容。

位于 query 位置上、当前仍不完整的那个池——最多 3 个尚未凑成完整 4 组的 token——可以单独追加为**可见尾部**，因为无论池是否完整，query 总是被允许看到自身及其紧邻的、因果有效的近期上下文：

```text
   max index slots required  =  512 pools × 4 tokens/pool  +  3 tail positions
                              =  2,051 slots
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │  A buffer sized to a hard 2,048 slots — the "selection budget"     │
   │  number everyone quotes — will overflow by up to 3 positions on    │
   │  the incomplete-tail case. Size index buffers to 2,051 (or         │
   │  whatever your pool width and budget actually imply), including   │
   │  room for invalid/padded entries, not to the round headline        │
   │  number.                                                            │
   └────────────────────────────────────────────────────────────────────┘
```

---


<details>
<summary>English original</summary>

**Module 06 — DSA & KPool: Selective Retrieval**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 05](Lecture-05.md) | **Next:** [Module 07 →](Lecture-07.md)

---

[Module 05](Lecture-05.md) answered "how is each historical token represented?" This module answers a different question: **which historical positions does this query actually get to read?** DeepSeek Sparse Attention (DSA) separates a cheap selection mechanism from the expensive attention computation it gates — and GLM-5.3-Flash's specific implementation, pooled indexing, has boundary behavior that is easy to get subtly wrong.

---

**Learning objectives**

By the end of this module you should be able to:

1. Distinguish the indexer's dimensions from the main MLA heads' dimensions, and explain why they differ.
2. Derive the indexer's selection budget, including the incomplete-tail case.
3. Explain the specific causal-visibility bug that pooled indexing makes possible, and name the sequence lengths where it clusters.
4. Derive why a fixed top-k selection budget does not imply constant-time decode.
5. Explain why pooling is a selection mechanism, not a cache-size reduction.

---

**1. Indexer dimensions are not main-attention dimensions**

```text
   Indexer property          Value               Main MLA (Module 05, for contrast)
   ────────────────────      ──────              ────────────────────────────────
   Indexer heads              32                  64 main heads
   Indexer head dimension     128                 256-dim main K/Q/V heads
   Pool width                 4 tokens            (no pooling — token-indexed)
   Main selection budget      2,048 positions     (no cap — full causal history)
   Incomplete-tail handling   enabled             n/a
```

These are two separate mechanisms with two separate parameter sets. The indexer's job is cheap approximate scoring over the *entire* causal history to find which positions are worth reading in full; the main MLA path then does the expensive, exact computation only over the positions the indexer selected. Confusing the two — for instance, assuming the indexer's pool width tells you anything about the main attention cache — is the single most common misreading of this mechanism, and §4 names it explicitly.

---

**2. The selection budget, with its trap**

The indexer groups tokens into **pools of 4**, builds a learned, channel-wise-weighted representation of each *complete* pool, scores those pool representations, and selects the highest-scoring pools up to the budget:

```text
   main selection budget  =  2,048 positions
   pool width             =  4 tokens

   complete pools selectable  =  2,048 / 4  =  512 pools
```

Selected pools are then **expanded back to their original constituent token positions** for the main attention computation — the pooling is used only to make scoring cheap, not to reduce what the main attention actually reads.

The current, still-incomplete pool at the query's position — up to 3 tokens that haven't yet formed a complete group of 4 — can be appended separately as a **visible tail**, since a query is always allowed to see itself and its immediate, causally-valid recent context regardless of pool completion:

```text
   max index slots required  =  512 pools × 4 tokens/pool  +  3 tail positions
                              =  2,051 slots
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │  A buffer sized to a hard 2,048 slots — the "selection budget"     │
   │  number everyone quotes — will overflow by up to 3 positions on    │
   │  the incomplete-tail case. Size index buffers to 2,051 (or         │
   │  whatever your pool width and budget actually imply), including   │
   │  room for invalid/padded entries, not to the round headline        │
   │  number.                                                            │
   └────────────────────────────────────────────────────────────────────┘
```

---

</details>

## 3. 因果性比三角掩码更微妙

普通的因果 attention 需要一条规则：位置 `i` 可以 attend 到位置 `j`，当且仅当 `j ≤ i`。池化索引添加了第二条容易忽略的规则：**一个池只有在它的最后一个组成 token 对查询因果可见后，才能变为可选择的。** 如果允许一个池的表示在其最后一个 token 存在之前影响评分，那么该池表示实际上已经将来自未来的信息泄露到了为更早查询所做的选择决策中。

```text
   pool = tokens [4, 5, 6, 7]     (pool width 4, this is the 2nd pool: positions 4-7)

   a query at position 5 must NOT be able to select this pool's
   representation — the pool isn't "complete" until position 7 exists,
   and position 7 is in this query's future.

   a query at position 8 (or later) MAY select this pool — all four
   of its constituent tokens are now in the past.
```

这正是那种围绕小序列长度和池边界算术聚集的边界条件，尤其是当你将 padding 或打包的多文档批混入其中时：

```text
   Watch lengths:  3, 4, 5, 7, 8, 9

   3  →  a sequence that never completes its first pool at all
         (does the visible-tail path handle this correctly on its own?)
   4  →  exactly one complete pool, zero tail — an off-by-one-prone boundary
   5  →  one complete pool + a 1-token tail
   7  →  one complete pool + a 3-token tail (the MAXIMUM tail size)
   8  →  exactly two complete pools, zero tail — another exact-boundary case
   9  →  two complete pools + a 1-token tail
```

针对该机制的正确性测试套件应当在这些长度的每一个上构造序列——并用填充和打包文档输入重复该练习，其中文档边界可能以不同形式悄无声息地重新引入同一类 off-by-one 错误（跨越文档边界的池不得让一个文档的 token 贡献到另一个文档的选择或 attention 中，正如普通的打包序列 attention 必须防止跨文档泄露一样）。

---

## 4. 池化不是四倍的缓存缩减

这是 §1 中命名的陷阱，作为其自身的规则陈述，因为它值得单独重复：

```text
   pool width 4   ⇏   main latent cache divided by 4
```

池表示的存在**仅仅是为了让 indexer 评分变得廉价**。选中的池在被主 MLA attention（[模块 05](Lecture-05.md)）读取之前，会被扩展回完整的组成 token 位置——主 attention 的每 token 潜在缓存的大小与 [模块 05 §2](Lecture-05.md) 推导出的完全一致，无论 indexer 的池宽度如何。池化和 MLA 潜在缓存是两种不同的数据结构，服务于两种不同的目的，其中任何一个的大小都不由另一个的配置决定。

---

## 5. 固定的 top-k 不意味着恒定的 decode 成本（逐 token 生成阶段）

主 attention 在选择完成后会读取大致固定数量 `K` 的位置——但选择本身并非没有代价，并且即使它 *选择* 的东西不随上下文长度增长，其成本也会随上下文长度扩展。

对于 `T` 个 token 的上下文，decode 成本分解为（至少）三个独立扩展的项：

```text
   F_decode(T)  ≈  F_projections+MoE+KDA                    ← Modules 02–04, roughly context-independent per step
               +  O( H_I · d_I · T/4 )                       ← INDEXER: scores ~T/4 candidate pools, every step
               +  O( H_MLA · K · r )                          ← MAIN ATTENTION: reads only the selected K positions
```

中间项是“固定 top-k 意味着平坦延迟”这一直觉所忽略的：**indexer 在每一个 decode 步骤仍然必须对大约 `T/4` 个候选池进行评分**，即使后续昂贵的主 attention 无论 `T` 如何都只读取有界的 `K` 个位置。随着上下文增长，indexer 评分成本也随之增长——与 `T` 成线性关系，即使主 attention 保持平坦。

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │  A nearly flat decode-latency curve measured across a moderate      │
   │  range of context lengths can be a REAL result — the indexer term   │
   │  may simply be small relative to the other terms at that range.     │
   │  It does NOT prove the whole algorithm is O(1) in context length.   │
   │  Measure the indexer term specifically, at your actual maximum      │
   │  deployed context, before claiming context-independence.            │
   └────────────────────────────────────────────────────────────────────┘
```

对于 prefill（首字前的整段计算），情况更加尖锐：为许多同时查询中的每一个评分一个 *扩展的* 前缀，可能会保留真正随长度二次增长的 indexer 组件——相同的每个查询 `T/4` 个候选的成本，现在乘以 `T` 个查询而不是一个——即使主要的、昂贵的 attention pass 始终保持稀疏。

---


<details>
<summary>English original</summary>

**3. Causality is more delicate than a triangular mask**

Ordinary causal attention needs one rule: position `i` may attend to position `j` only if `j ≤ i`. Pooled indexing adds a second rule that is easy to miss: **a pool may only become selectable once its final constituent token is causally visible to the query.** If a pool's representation is allowed to influence scoring before its last token exists, that pool representation has effectively leaked information from the future into a selection decision made for an earlier query.

```text
   pool = tokens [4, 5, 6, 7]     (pool width 4, this is the 2nd pool: positions 4-7)

   a query at position 5 must NOT be able to select this pool's
   representation — the pool isn't "complete" until position 7 exists,
   and position 7 is in this query's future.

   a query at position 8 (or later) MAY select this pool — all four
   of its constituent tokens are now in the past.
```

This is exactly the kind of boundary condition that clusters around small sequence lengths and pool-boundary arithmetic, particularly once you add padding or packed multi-document batches to the mix:

```text
   Watch lengths:  3, 4, 5, 7, 8, 9

   3  →  a sequence that never completes its first pool at all
         (does the visible-tail path handle this correctly on its own?)
   4  →  exactly one complete pool, zero tail — an off-by-one-prone boundary
   5  →  one complete pool + a 1-token tail
   7  →  one complete pool + a 3-token tail (the MAXIMUM tail size)
   8  →  exactly two complete pools, zero tail — another exact-boundary case
   9  →  two complete pools + a 1-token tail
```

A correctness suite for this mechanism should construct sequences at every one of these lengths — and repeat the exercise with padded and packed-document inputs, where a document boundary can silently reintroduce the same off-by-one class of bug in a different guise (a pool spanning a document boundary must not let one document's tokens contribute to another document's selection or attention, exactly as ordinary packed-sequence attention must prevent cross-document leakage).

---

**4. Pooling is not a fourfold cache reduction**

This is the trap named in §1, stated as its own rule because it is worth repeating on its own:

```text
   pool width 4   ⇏   main latent cache divided by 4
```

The pool representation exists **only to make indexer scoring cheap**. Selected pools are expanded back to full constituent token positions before the main MLA attention ([Module 05](Lecture-05.md)) reads them — the main attention's per-token latent cache is exactly as large as [Module 05 §2](Lecture-05.md) derived, regardless of the indexer's pool width. Pooling and the MLA latent cache are two different data structures serving two different purposes, and neither one's size follows from the other's configuration.

---

**5. Fixed top-k does not mean constant decode cost**

The main attention reads a roughly-fixed number `K` of positions once selection completes — but selection itself is not free, and its cost scales with context length even when the thing it *selects* does not.

For a context of `T` tokens, decode cost decomposes into (at least) three separately-scaling terms:

```text
   F_decode(T)  ≈  F_projections+MoE+KDA                    ← Modules 02–04, roughly context-independent per step
               +  O( H_I · d_I · T/4 )                       ← INDEXER: scores ~T/4 candidate pools, every step
               +  O( H_MLA · K · r )                          ← MAIN ATTENTION: reads only the selected K positions
```

The middle term is the one a "fixed top-k means flat latency" intuition misses: **the indexer must still score approximately `T/4` candidate pools at every decode step**, even though the expensive main attention that follows reads a bounded `K` positions regardless of `T`. As context grows, indexer scoring cost grows with it — linearly in `T`, even while the main attention stays flat.

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │  A nearly flat decode-latency curve measured across a moderate      │
   │  range of context lengths can be a REAL result — the indexer term   │
   │  may simply be small relative to the other terms at that range.     │
   │  It does NOT prove the whole algorithm is O(1) in context length.   │
   │  Measure the indexer term specifically, at your actual maximum      │
   │  deployed context, before claiming context-independence.            │
   └────────────────────────────────────────────────────────────────────┘
```

For prefill, the situation is sharper still: scoring an *expanding* prefix for every one of many simultaneous queries can retain a genuinely quadratic-in-length indexer component — the same `T/4`-candidates-per-query cost, now multiplied across `T` queries instead of one — even though the main, expensive attention pass remains sparse throughout.

---

</details>

## 检查点

现在应能：

1. 说明为何 indexer 的 32 heads × 128 dim 是与 64 个主 heads × 256 dim 相互独立的配置，以及为何将二者混为一谈属于范畴错误。
2. 从 2,048 的预算、pool 宽度 4 和 3-token 尾部推导出 2,051 槽位的上限。
3. 陈述 pool 完成因果性规则，并构造一个能暴露其被违反的测试序列。
4. 列举六个值得测试的边界长度，并解释每个长度探测什么。
5. 用一句话解释为何 pool 宽度不决定主缓存大小。
6. 推导三项 decode（逐 token 生成阶段）代价分解，并解释哪一项打破了「固定 top-k ⇒ O(1)」的假设。

---

## 交付

这是 **[capstone 阶梯](Lecture-12.md) 的 Stage 5**：构建一套 **KPool 边界测试套件**，覆盖：(1) §3 观察清单中的每个序列长度，含与不含 padding；(2) 一个 packed document 批，其中 pool 刻意跨越文档边界，验证在 selection 与 attention 中均无跨文档泄漏；(3) 在一系列上下文长度上测量 indexer 代价，把 `O(H_I d_I T/4)` 项与主 attention 的代价分离，以对照 §5 的线性增长预测与你的实际实现。

---

## 时效性

* **长期有效：** 定义 DeepSeek Sparse Attention 的 selection/attention 分离（通用层面）、pool 完成因果性论证、三项 decode 代价分解。
* **检查点相关：** 32 个 indexer heads、dimension 128、pool 宽度 4、预算 2,048（⇒ 2,051 槽位缓冲区）属于本检查点的配置 —— 若未来修订中其中任何一项发生变化，需重新推导槽位数。

---

**下一章：** [Module 07 — mHC：流形约束的残差流 →](Lecture-07.md)


<details>
<summary>English original</summary>

**Checkpoint**

You should now be able to:

1. State why the indexer's 32 heads × 128 dim are a separate configuration from the 64 main heads × 256 dim, and why conflating them is a category error.
2. Derive the 2,051-slot maximum from a 2,048 budget, pool width 4, and a 3-token tail.
3. State the pool-completion causality rule and construct a test sequence that would expose a violation of it.
4. Name the six boundary lengths worth testing and explain what each one probes.
5. Explain, in one sentence, why pool width does not determine main-cache size.
6. Derive the three-term decode cost decomposition and explain which term breaks a "fixed top-k ⇒ O(1)" assumption.

---

**Ship it**

This is **Stage 5 of the [capstone ladder](Lecture-12.md)**: build a **KPool boundary test suite** covering: (1) every sequence length in §3's watch list, with and without padding; (2) a packed-document batch with a pool deliberately spanning a document boundary, verifying no cross-document leakage in either selection or attention; (3) an indexer-cost measurement across a range of context lengths, isolating the `O(H_I d_I T/4)` term from the main attention's cost, to check the linear-growth prediction from §5 against your actual implementation.

---

**Current as of**

* **Timeless:** the selection/attention separation that defines DeepSeek Sparse Attention generally, the pool-completion causality argument, the three-term decode cost decomposition.
* **Checkpoint-specific:** 32 indexer heads, dimension 128, pool width 4, budget 2,048 (⇒ 2,051-slot buffers) are this checkpoint's configuration — re-derive the slot count if any of these change in a future revision.

---

**Next:** [Module 07 — mHC: Manifold-Constrained Residual Streams →](Lecture-07.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
