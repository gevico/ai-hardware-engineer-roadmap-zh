---
title: 模块 05 — MLA：压缩逐 token 历史
description: 模块 05 — MLA：压缩逐 token 历史
---

# 模块 05 — MLA：压缩逐 token 历史

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 04](Lecture-04.md) | **Next:** [Module 06 →](Lecture-06.md)

---

KDA 把整段历史压缩成一个固定大小的状态矩阵，完全丢弃 token 身份。MLA —— 用于本 checkpoint 的 11 个稀疏 attention layer —— 则不同：它让历史保持 **token 索引**（你依然可以问「token 47 长什么样？」），但把每个 token 必须存储的内容缩小一个数量级。本模块推导这一缩小量，以及让 attention 直接在压缩形式上运算、且无需重新展开的代数技巧。

---

## 学习目标

学完本模块后，你应当能够：

1. 说明 MLA 缓存的是什么，而非展开后的逐 head key 与 value，以及为什么。
2. 从 checkpoint 的维度出发，推导全展开缓存与所缓存 latent 之间的内存比例。
3. 推导让 attention 直接读取 latent 的吸收恒等式，并准确说明它改变了什么、没有改变什么。
4. 解释为什么本 checkpoint 的 NoPE 配置不会让模型 order-blind。
5. 解释为什么最快的代数形式在 prefill（首字前的整段计算）与 decode（逐 token 生成阶段）之间不同。

---

## 1. latent 表示

MLA 不缓存每个 head 展开后的 key 与 value 向量，而是把每个 token 投影到一个共享的低秩 latent：

```text
   c_t  =  RMSNorm( W_D · x_t )              c_t  ∈  R^r          (the cached quantity)
```

随后，逐 head 的 key 与 value 由逐 head 上投影矩阵**按需从 latent 重建**：

```text
   k_{t,h}  =  U_h^K · c_t
   v_{t,h}  =  U_h^V · c_t
```

要点在于：被缓存、并沿序列持久保存的是 `c_t`。`k_{t,h}` 与 `v_{t,h}` 在需要时*由*它计算得出，而非直接存储。本 checkpoint 配置了 **512 的 KV latent 宽度**、**64 个主 attention head**、以及 **256 维的主 key/query 与 value head**，其中主 MLA 路径配置为 **NoPE**（主 query/key 上的旋转维度为零 —— §4 解释为什么这并不意味着它听起来的那回事）。

---

## 2. 内存优势推导

比较全展开的 BF16 缓存所需与共享 latent 实际所需 —— 按每个 token、每个 layer。

**全展开**（假设情形 —— 这*并非*实际被缓存的内容）：64 个 head，每个存储一个 256 维 key 和一个 256 维 value，每项 2 字节（BF16）：

```text
   64 heads × (256 + 256) dims × 2 bytes  =  65,536 bytes / token / layer
```

**共享 latent**（实际被缓存的内容）：512 维 latent，2 字节：

```text
   512 × 2  =  1,024 bytes / token / layer
```

```text
   ┌────────────────────────────────────────────────────────────────┐
   │                     65,536 / 1,024  =  64×                      │
   │                                                                    │
   │   This is a 64× reduction for THESE specific logical              │
   │   representations, on THIS checkpoint's dimensions. It is NOT     │
   │   a claim that total model memory or end-to-end inference speed   │
   │   improves by 64× — weights, the DSA indexer, workspace buffers,   │
   │   and communication overhead are all separate terms that don't    │
   │   shrink because this one term did.                                │
   └────────────────────────────────────────────────────────────────┘
```

[模块 09](Lecture-09.md) 使用这个精确的 512 宽、BF16、逐 layer 的数值，来构建覆盖全部 11 个 MLA layer 与给定上下文长度的完整每请求缓存预算 —— 本模块只确立逐 token、逐 layer 的单位。


<details>
<summary>English original</summary>

**Module 05 — MLA: Compressing Per-Token History**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 04](Lecture-04.md) | **Next:** [Module 06 →](Lecture-06.md)

---

KDA compresses an entire history into one fixed-size state matrix, discarding token identity entirely. MLA — used in this checkpoint's 11 sparse attention layers — does something different: it keeps history **token-indexed** (you can still ask "what did token 47 look like?"), but shrinks what has to be stored per token by an order of magnitude. This module derives that shrinkage, and the algebraic trick that lets attention operate directly on the compressed form without ever re-expanding it.

---

**Learning objectives**

By the end of this module you should be able to:

1. State what MLA caches instead of expanded per-head keys and values, and why.
2. Derive the memory ratio between a fully-expanded cache and the cached latent, from the checkpoint's dimensions.
3. Derive the absorption identity that lets attention read the latent directly, and state exactly what it does and does not change.
4. Explain why this checkpoint's NoPE configuration does not make the model order-blind.
5. Explain why the fastest algebraic form differs between prefill and decode.

---

**1. The latent representation**

Rather than caching every head's expanded key and value vectors, MLA projects each token down to one shared low-rank latent:

```text
   c_t  =  RMSNorm( W_D · x_t )              c_t  ∈  R^r          (the cached quantity)
```

Per-head keys and values are then **reconstructed from the latent, on demand**, by per-head up-projection matrices:

```text
   k_{t,h}  =  U_h^K · c_t
   v_{t,h}  =  U_h^V · c_t
```

The point: `c_t` is what gets cached and persisted across the sequence. `k_{t,h}` and `v_{t,h}` are computed *from* it when needed, rather than stored directly. This checkpoint configures a **KV latent width of 512**, **64 main attention heads**, and **256-dimensional main key/query and value heads**, with the main MLA path configured as **NoPE** (zero rotary dimensions on the main query/key — §4 explains why this doesn't mean what it sounds like).

---

**2. The memory advantage, derived**

Compare what a fully-expanded BF16 cache would need against what the shared latent actually needs, per token, per layer.

**Fully expanded** (hypothetical — this is *not* what gets cached): 64 heads, each storing a 256-dim key and a 256-dim value, at 2 bytes (BF16):

```text
   64 heads × (256 + 256) dims × 2 bytes  =  65,536 bytes / token / layer
```

**Shared latent** (what actually gets cached): 512-dim latent at 2 bytes:

```text
   512 × 2  =  1,024 bytes / token / layer
```

```text
   ┌────────────────────────────────────────────────────────────────┐
   │                     65,536 / 1,024  =  64×                      │
   │                                                                    │
   │   This is a 64× reduction for THESE specific logical              │
   │   representations, on THIS checkpoint's dimensions. It is NOT     │
   │   a claim that total model memory or end-to-end inference speed   │
   │   improves by 64× — weights, the DSA indexer, workspace buffers,   │
   │   and communication overhead are all separate terms that don't    │
   │   shrink because this one term did.                                │
   └────────────────────────────────────────────────────────────────┘
```

[Module 09](Lecture-09.md) uses this exact 512-wide, BF16, per-layer figure to build the full per-request cache budget across all 11 MLA layers and a stated context length — this module only establishes the per-token, per-layer unit.

---

</details>

## 3. 避免重新展开历史的代数

如果计算 attention 仍需在每一步从缓存的 latent 中物化出每个历史 token 的全尺寸 `k_{t,h}` 和 `v_{t,h}`，§2 的显存收益便毫无价值——那只是用更小的 cache 换取更大的重算量。MLA 用一次代数重排避免了这一点。

**对于 score。** 与一个缓存的历史 token `j` 做 query-key 点积：

```text
   q_hᵀ · k_{j,h}   =   q_hᵀ · (U_h^K c_j)                      [substitute k_{j,h}]
                    =   (q_hᵀ U_h^K) · c_j                       [associativity]
                    =   ( (U_h^K)ᵀ q_h )ᵀ · c_j                  [transpose identity]
```

定义 `q̃_h = (U_h^K)ᵀ q_h`——**每个 query、每个 head 计算一次**，与你要打分的是哪个历史 token `j` 无关。于是针对 cache 的每个 score 都变成与缓存 latent 的直接点积：

```text
   q_hᵀ · k_{j,h}   =   q̃_hᵀ · c_j            ← operates DIRECTLY on the cached latent
                                                  no k_{j,h} ever materialized
```

**对于输出。** 对 value 的 attention 加权求和具有相同的结构，而线性性允许把固定矩阵因子提到历史求和之外：

```text
   Σ_j  p_j · v_{j,h}   =   Σ_j  p_j · (U_h^V c_j)                 [substitute v_{j,h}]
                        =   U_h^V · ( Σ_j  p_j · c_j )              [U_h^V doesn't depend on j — factors out]
```

因此加权累加在 **latent 空间**中进行（对历史累加 `p_j · c_j`），只有*最终*累加结果才通过 `U_h^V` 展开——只展开一次，而不是每个历史 token 展开一次。

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │   Neither derivation requires ever reconstructing a historical    │
   │   token's full-size key or value. Scores read the latent          │
   │   directly via a pre-transformed query; the value sum accumulates │
   │   in latent space and expands exactly once, at the end.            │
   └──────────────────────────────────────────────────────────────────┘
```

### 这套代数绝不能改变的一件事

```text
   Rewriting HOW a dot product is computed does not authorize
   rewriting WHAT it's compared against.

   The softmax temperature / attention scale is a property of the
   ORIGINAL q·k formulation. Moving to the absorbed (latent-space)
   form must reproduce IDENTICAL score values — not merely
   proportional ones — or you have changed the model's attention
   distribution while believing you only changed its data layout.
```

这正是 [Module 11](Lecture-11.md) 的正确性矩阵所命名的「expanded-versus-absorbed attention 等价性」这一类 bug，它值得一个专门的数值测试，而不是一个假设：在完全相同的输入上计算两种形式，要求 score 在浮点容差内一致，而不仅仅是产出看起来相似的下游文本。

---

## 4. NoPE 并不意味着对顺序无感

这个检查点的主 MLA 路径使用 **NoPE**——主 query/key 上的旋转位置维度为零。很容易把它读成「模型在这些 layer 里分辨不出 token 顺序」，这是错的，原因有两个，且相互独立：

```text
   1. GLM-5.3-Flash is a HYBRID model. 34 of its 45 layers are KDA, and
      KDA's recurrence is INHERENTLY order-sensitive — S_t depends on the
      exact sequence of updates that produced it. A model with 34
      order-sensitive layers is not rendered order-blind by 11 layers
      that individually lack rotary position encoding.

   2. Within the MLA layers themselves, CAUSAL MASKING already restricts
      which positions a query can attend to (only j ≤ t). That alone
      encodes a coarse notion of order — "before vs. after" — even
      without a positional embedding encoding exact relative distance.
```

NoPE *确实*移除的，具体来说，是位于 causal masking 本就允许的范围*之内*的细粒度相对距离信息——即 RoPE 否则会直接注入点积的那类信号。这对某个给定的下游任务是否重要，是一个关于*这个特定* layer 在混合栈中所扮演角色的实证问题，而不是仅凭旋转编码的有无就能得出的结论。

---


<details>
<summary>English original</summary>

**3. The algebra that avoids re-expanding history**

The memory win in §2 would be worthless if computing attention still required materializing every historical token's full-size `k_{t,h}` and `v_{t,h}` from the cached latents at every step — you'd be trading a smaller cache for a larger recompute. MLA avoids this with an algebraic reshuffling.

**For the score.** A query-key dot product against a cached historical token `j`:

```text
   q_hᵀ · k_{j,h}   =   q_hᵀ · (U_h^K c_j)                      [substitute k_{j,h}]
                    =   (q_hᵀ U_h^K) · c_j                       [associativity]
                    =   ( (U_h^K)ᵀ q_h )ᵀ · c_j                  [transpose identity]
```

Define `q̃_h = (U_h^K)ᵀ q_h` — computed **once per query, per head**, independent of which historical token `j` you're scoring against. Then every score against the cache becomes a direct dot product with the cached latent:

```text
   q_hᵀ · k_{j,h}   =   q̃_hᵀ · c_j            ← operates DIRECTLY on the cached latent
                                                  no k_{j,h} ever materialized
```

**For the output.** The attention-weighted sum over values has the same structure, and linearity lets the fixed matrix factor outside the sum over history:

```text
   Σ_j  p_j · v_{j,h}   =   Σ_j  p_j · (U_h^V c_j)                 [substitute v_{j,h}]
                        =   U_h^V · ( Σ_j  p_j · c_j )              [U_h^V doesn't depend on j — factors out]
```

So the weighted accumulation happens **in latent space** (summing `p_j · c_j` over history), and only the *final* accumulated result gets expanded through `U_h^V` — once, not once per historical token.

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │   Neither derivation requires ever reconstructing a historical    │
   │   token's full-size key or value. Scores read the latent          │
   │   directly via a pre-transformed query; the value sum accumulates │
   │   in latent space and expands exactly once, at the end.            │
   └──────────────────────────────────────────────────────────────────┘
```

**The one thing this algebra must never change**

```text
   Rewriting HOW a dot product is computed does not authorize
   rewriting WHAT it's compared against.

   The softmax temperature / attention scale is a property of the
   ORIGINAL q·k formulation. Moving to the absorbed (latent-space)
   form must reproduce IDENTICAL score values — not merely
   proportional ones — or you have changed the model's attention
   distribution while believing you only changed its data layout.
```

This is exactly the class of bug [Module 11](Lecture-11.md)'s correctness matrix names as "expanded-versus-absorbed attention equivalence," and it deserves a dedicated numerical test, not an assumption: compute both forms on identical inputs and require the scores to match to floating-point tolerance, not merely to produce similar-looking downstream text.

---

**4. NoPE does not mean order-blind**

This checkpoint's main MLA path uses **NoPE** — zero rotary position dimensions on the main query/key. It is tempting to read this as "the model can't tell token order in these layers," which is wrong, for two independent reasons:

```text
   1. GLM-5.3-Flash is a HYBRID model. 34 of its 45 layers are KDA, and
      KDA's recurrence is INHERENTLY order-sensitive — S_t depends on the
      exact sequence of updates that produced it. A model with 34
      order-sensitive layers is not rendered order-blind by 11 layers
      that individually lack rotary position encoding.

   2. Within the MLA layers themselves, CAUSAL MASKING already restricts
      which positions a query can attend to (only j ≤ t). That alone
      encodes a coarse notion of order — "before vs. after" — even
      without a positional embedding encoding exact relative distance.
```

What NoPE *does* remove, specifically, is fine-grained relative-distance information *within* what causal masking already permits — the kind of signal RoPE would otherwise inject directly into the dot product. Whether that matters for a given downstream task is an empirical question about *this specific* layer's role in the hybrid stack, not something you can conclude from the presence or absence of rotary encoding alone.

---

</details>

## 5. Prefill（首字前的整段计算）与 decode（逐 token 生成阶段）可能需要不同的代数形式

两个事实并不能自动决定第三个：无论是内存论证（§2）还是代数等价性（§3），都不能告诉你下面两种计算策略中哪一种在你的硬件上、在你的批大小下最快：

```text
   NAIVE (per-head expansion)     :  expand k_{j,h}, v_{j,h} for every historical
                                      token, run ordinary multi-head attention
   ABSORBED (latent-space, §3)   :  keep everything in the r-dimensional latent
                                      space, expand only the final accumulated result
```

```text
   PREFILL   :  many queries share the same historical keys/values.
                Expanding once and reusing across queries can make the
                naive form's larger per-step matmuls a good FIT for
                tensor cores — this is a COMPUTE-heavy regime
                (see Hardware-Aware LLM Quantization — Module 01
                 for the general compute-vs-bandwidth framing).

   DECODE    :  one query at a time, against a cache that dominates
                memory traffic. The absorbed form's smaller cache
                directly reduces the dominant cost — this is a
                BANDWIDTH-heavy regime, and it is where the 64×
                reduction from §2 pays off most directly.
```

**仅靠数 FLOPs 来选择路径并不可靠**——正确的选择取决于哪种资源（计算还是带宽）才是真正受限的那个，这正是 roofline（性能上界模型）区分，它支配着 [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) 课程中的每一个优化决策。[Module 10](Lecture-10.md) 会回到这一点，把它当作一条性能剖析假设，而不是一条要背下来的规则：先测量你处在哪个 regime，再假定哪种代数形式胜出。

---

## 检查点

你现在应该能够：

1. 说出 MLA 缓存什么（`c_t`），以及它按需重建什么（`k_{t,h}`、`v_{t,h}`）。
2. 不看资料，从本检查点的维度推导出 64× 的内存比例。
3. 推导 `q̃_h = (U_h^K)ᵀ q_h`，并解释为什么它让打分跳过逐 token 的 key 展开。
4. 推导 latent 空间的值累加恒等式，并解释为什么 `U_h^V` 可以提到求和之外。
5. 解释为什么主 MLA 路径中的 NoPE 不会让整个模型失去顺序感知。
6. 解释为什么 prefill 和 decode 可以合理地偏好同一个 attention 的不同代数形式。

---

## 交付

这是 **[capstone ladder](Lecture-12.md) 的第 4 阶段**：搭建一个 **MLA 等价性实验室**——在缓存 latent 上分别实现 attention 的朴素形式（逐 head 展开）与吸收形式（latent 空间），验证两者在相同输入下的分数与输出在浮点容差内一致，然后测量两种形式在 decode 形态的工作负载（批大小 1、缓存持续增长）和 prefill 形态的工作负载（大量 query、共享历史）下的墙钟时间与内存流量。报告哪种形式在哪种 regime 下胜出，以及这是否符合 §5 的预测。

---

## 截至目前的时效性

* **永不过时：** latent 缓存的思想以及 score/value 吸收代数——这是来自 DeepSeek-MLA 系列工作的联合低秩压缩与“把上投影吸收进 query/output”技术，在此应用于本检查点的具体维度。
* **检查点相关：** KV latent 宽度 512、64 个主 head、主 K/Q/V head 为 256 维、主路径使用 NoPE——在复用这些常量之前，先对照实际的检查点配置核实。

---

**接下来：** [Module 06 — DSA & KPool: Selective Retrieval →](Lecture-06.md)


<details>
<summary>English original</summary>

**5. Prefill and decode may want different algebra**

Two facts do not automatically settle a third: neither the memory argument (§2) nor the algebraic equivalence (§3) tells you which of the following two computational strategies is fastest on your hardware, at your batch size:

```text
   NAIVE (per-head expansion)     :  expand k_{j,h}, v_{j,h} for every historical
                                      token, run ordinary multi-head attention
   ABSORBED (latent-space, §3)   :  keep everything in the r-dimensional latent
                                      space, expand only the final accumulated result
```

```text
   PREFILL   :  many queries share the same historical keys/values.
                Expanding once and reusing across queries can make the
                naive form's larger per-step matmuls a good FIT for
                tensor cores — this is a COMPUTE-heavy regime
                (see Hardware-Aware LLM Quantization — Module 01
                 for the general compute-vs-bandwidth framing).

   DECODE    :  one query at a time, against a cache that dominates
                memory traffic. The absorbed form's smaller cache
                directly reduces the dominant cost — this is a
                BANDWIDTH-heavy regime, and it is where the 64×
                reduction from §2 pays off most directly.
```

**Selecting a path by counting FLOPs alone is unreliable** — the right choice depends on which resource (compute or bandwidth) is actually binding, exactly the roofline distinction that governs every optimization decision in the [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) course. [Module 10](Lecture-10.md) returns to this as a profiling hypothesis, not a rule to memorize: measure which regime you're in before assuming which algebraic form wins.

---

**Checkpoint**

You should now be able to:

1. State what MLA caches (`c_t`) versus what it reconstructs on demand (`k_{t,h}`, `v_{t,h}`).
2. Derive the 64× memory ratio from this checkpoint's dimensions without looking it up.
3. Derive `q̃_h = (U_h^K)ᵀ q_h` and explain why it lets scoring skip per-token key expansion.
4. Derive the latent-space value-accumulation identity and explain why `U_h^V` factors outside the sum.
5. Explain why NoPE in the main MLA path does not make the whole model order-blind.
6. Explain why prefill and decode can legitimately prefer different algebraic forms of the same attention.

---

**Ship it**

This is **Stage 4 of the [capstone ladder](Lecture-12.md)**: build an **MLA equivalence laboratory** — implement both the naive (per-head expansion) and absorbed (latent-space) forms of attention over cached latents, verify their scores and outputs match to floating-point tolerance on identical inputs, then measure wall-clock and memory traffic for both forms at a decode-shaped workload (batch 1, growing cache) and a prefill-shaped workload (many queries, shared history). Report which form wins in which regime, and whether that matches the §5 prediction.

---

**Current as of**

* **Timeless:** the latent-caching idea and the score/value absorption algebra — this is the joint low-rank compression and "absorb the up-projection into the query/output" technique from the DeepSeek-MLA line of work, applied here to this checkpoint's specific dimensions.
* **Checkpoint-specific:** KV latent width 512, 64 main heads, 256-dim main K/Q/V heads, NoPE on the main path — verify against the actual checkpoint config before reusing these constants.

---

**Next:** [Module 06 — DSA & KPool: Selective Retrieval →](Lecture-06.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
