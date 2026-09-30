---
title: Module 01 — 确切的模型
description: Module 01 — 确切的模型
---

# Module 01 — 确切的模型

**合集：** [GLM-5.3-Flash Architecture Mastery](README.md) | **上一篇：** [← 课程索引](README.md) | **下一篇：** [模块 02 →](Lecture-02.md)

---

在研究任何机制之前，先把你实际研究的检查点固定下来。GLM-5.3-Flash 不是层数更少的 GLM-5.3，也不是某个更大同门模型的量化缩水版——Z.ai 把它描述为一种**重新设计的架构**，将线性 attention 与稀疏 attention 同流形约束的超连接结合起来，并作为独立模型训练。本课程后续的一切都取决于本模块中的数字是否正确，因此请把它当作需要记住的 ground truth，而不是可以略读的背景。

---

## 学习目标

学完本模块后，你应当能够：

1. 凭记忆背出该检查点的配置：hidden width、layer 数、attention schedule、MoE shape、residual stream 数。
2. 仅凭 layer 数重建 attention schedule 以及 dense/MoE FFN 的划分。
3. 追踪一个 token 走完整个执行路径，并指出五种机制各自位于其中哪个位置。
4. 解释为什么这个模型是真正的架构重新设计，而不是某个已有模型的变体。
5. 说明哪些公开数字属于检查点事实，哪些属于依赖部署的说法。

---

## 1. 公开的配置

```text
   Model class            Glm5NextForConditionalGeneration
   Parameters             ≈320B total, ≈18B active per token
   Language hidden width  4,096
   Decoder depth          45 layers
   Attention mixture      34 KDA layers + 11 sparse MLA/DSA layers
   Feed-forward mixture   first 3 layers dense; remaining 42 layers MoE
   Routed experts         288 per MoE layer, top-8 selected
   Shared experts         1 per MoE layer (always active)
   Residual streams       4, via mHC
   Configured max context 1,048,576 tokens  (2^20)
```

有两点必须立刻指出，因为它们会在整个课程中反复出现：

* **“18B active”是每 token 的计算量估计，而非内存数字。** MoE 路由器选择某个 token 由哪些专家 *运行*；它并不减少为服务一个 batch 中任意 token 而必须 *常驻* 的专家权重数量。[模块 02](Lecture-02.md) 会把这一点说清楚。
* **“1,048,576 tokens”是配置的上限，而不是部署保证。** 某个具体的 8-GPU 部署是否真能分配并正确服务这一长度，是一个内存与正确性问题，本课程在[模块 09](Lecture-09.md) 和 [11](Lecture-11.md) 中给出答案——这不是配置文件本身能决定的事。

---

## 2. 自己重建 attention schedule

该 schedule 以一个重复块的形式给出：

```text
   [ KDA, KDA, KDA, DSA/MLA ]  ×11   =  44 layers
                                 +1 KDA   =  45 layers  (matches decoder depth)

   KDA layers total        =  3×11 + 1  =  34   ✓ matches "34 KDA layers"
   DSA/MLA layers total    =  1×11      =  11   ✓ matches "11 sparse MLA/DSA layers"
```

这件事值得亲手做一遍，而不是相信文字描述：在这个架构里，有许多地方公开的汇总数字（“34 KDA layers”）可以从更简单的重复结构中 *推导* 出来，这只是第一处；而能推导出来，才能让你日后抓住别人复现实现中的错误。

前馈部分的划分同理：

```text
   45 layers total  −  3 dense  =  42 MoE layers    ✓ matches "remaining 42 MoE"
```

每个 decoder layer 都既有一个 attention 子层（KDA 或 DSA/MLA）**又**有一个前馈子层（dense 或 MoE）——这是两条独立的轴，而不是一个合并的 schedule。layer 的位置以简单阈值决定其前馈类型（`layer_idx < 3` → dense），并以上面的重复模式决定其 attention 类型。无论两者各自使用哪种变体，mHC 都以相同方式包裹这两个子层。

---


<details>
<summary>English original</summary>

**Module 01 — The Exact Model**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Course index](README.md) | **Next:** [Module 02 →](Lecture-02.md)

---

Before studying any mechanism, fix the checkpoint you are actually studying. GLM-5.3-Flash is not GLM-5.3 run with fewer layers, nor a quantized shrink of a larger sibling — Z.ai describes it as a **redesigned architecture** combining linear and sparse attention with manifold-constrained hyper-connections, trained as its own model. Everything downstream in this course depends on the numbers in this module being right, so treat this as ground truth to memorize, not background to skim.

---

**Learning objectives**

By the end of this module you should be able to:

1. Recite the checkpoint's configuration from memory: hidden width, layer count, attention schedule, MoE shape, residual stream count.
2. Reconstruct the attention schedule and the dense/MoE FFN split from the layer count alone.
3. Trace one token through the full execution path and name where each of the five mechanisms sits in it.
4. Explain why this model is a genuine architectural redesign rather than a variant of an existing one.
5. State which published numbers are checkpoint facts versus deployment-dependent claims.

---

**1. The published configuration**

```text
   Model class            Glm5NextForConditionalGeneration
   Parameters             ≈320B total, ≈18B active per token
   Language hidden width  4,096
   Decoder depth          45 layers
   Attention mixture      34 KDA layers + 11 sparse MLA/DSA layers
   Feed-forward mixture   first 3 layers dense; remaining 42 layers MoE
   Routed experts         288 per MoE layer, top-8 selected
   Shared experts         1 per MoE layer (always active)
   Residual streams       4, via mHC
   Configured max context 1,048,576 tokens  (2^20)
```

Two things to flag immediately, because they recur throughout the course:

* **"18B active" is a per-token compute estimate, not a memory figure.** The MoE router selects which experts *run* for a token; it does not shrink how many expert weights must be *resident* to serve any token in a batch. [Module 02](Lecture-02.md) makes this precise.
* **"1,048,576 tokens" is a configured ceiling, not a deployment guarantee.** Whether a specific 8-GPU deployment can actually allocate and correctly serve that length is a memory-and-correctness question this course answers in [Modules 09](Lecture-09.md) and [11](Lecture-11.md) — not something the config file settles by itself.

---

**2. Reconstruct the attention schedule yourself**

The schedule is stated as a repeating block:

```text
   [ KDA, KDA, KDA, DSA/MLA ]  ×11   =  44 layers
                                 +1 KDA   =  45 layers  (matches decoder depth)

   KDA layers total        =  3×11 + 1  =  34   ✓ matches "34 KDA layers"
   DSA/MLA layers total    =  1×11      =  11   ✓ matches "11 sparse MLA/DSA layers"
```

This is worth doing by hand rather than trusting the prose: it is the first of many places in this architecture where a published summary number ("34 KDA layers") is *derivable* from a simpler repeating structure, and being able to derive it is what lets you catch an error in someone else's re-implementation later.

The feed-forward split works the same way:

```text
   45 layers total  −  3 dense  =  42 MoE layers    ✓ matches "remaining 42 MoE"
```

Every decoder layer has both an attention sublayer (KDA or DSA/MLA) **and** a feed-forward sublayer (dense or MoE) — these are two independent axes, not one combined schedule. A layer's position determines its feed-forward type by simple threshold (`layer_idx < 3` → dense) and its attention type by the repeating pattern above. mHC wraps both sublayers identically, regardless of which variant of either is in use.

---

</details>

## 3. 一个 token 穿过模型

```text
   Text tokens ──▶ embeddings ─────────────────────────────────┐
                                                                │
   Images ──▶ vision encoder ──▶ language-width features ──────┤
                                                                ▼
                                                    Four residual streams
                                                                │
                            ┌───────────────────────────────────┐
                            │  mHC collapse + normalization      │
                            │  KDA  or  sparse MLA (+ DSA select) │
                            │  mHC residual mixing                │
                            │                                     │
                            │  mHC collapse + normalization      │
                            │  Dense FFN  or  MoE                 │
                            │  mHC residual mixing                │
                            └───────────────────────────────────┘
                                          × 45 layers
                                                                │
                                                    Final stream contraction
                                                                │
                                                    Normalization + LM head
                                                                │
                                                      Next-token logits
```

这是一张**原生模型数据流的概念图**，而不是一份逐项启动的 GPU kernel 清单 —— 真实实现会在这张图上做大量融合、重排与批处理，而 [Module 10](Lecture-10.md) 才是你学会思考这一差距的地方。但图中的每个方框都对应着某样东西，你必须能在检查点代码中说出它的名字并定位到它：

| 方框 | 机制 | 模块 |
|---|---|---|
| vision encoder → 语言宽度特征 | 多模态投影 | [08](Lecture-08.md) |
| mHC 坍缩 / 混合 | 流形约束超连接 | [07](Lecture-07.md) |
| KDA | 循环线性 attention | [03](Lecture-03.md), [04](Lecture-04.md) |
| 稀疏 MLA（+ DSA select） | 压缩潜在 attention + 选择性检索 | [05](Lecture-05.md), [06](Lecture-06.md) |
| Dense FFN / MoE | 稀疏参数激活 | [02](Lecture-02.md) |

---

## 4. 为什么是“重新设计”，而不是“蒸馏”

人们很容易把混合模型理解为“拿一个现有的 dense 或 MoE（混合专家模型）Transformer，把 attention 换成一个更便宜的”。有两个事实反对这种理解，两者都关系到你如何对待本课程后续内容：

**第一，这些机制是协同设计的，而不是外挂上去的。** KDA 的通道级衰减门与 MLA 的潜在宽度，是和 DSA 池化宽度（4 个 token）与 mHC 流数量（4）一起选定的 —— 它们不是独立的超参数，你不可能在不动其他机制有效容量的情况下自由改动其中之一。孤立地研究单个机制（出于教学目的，本课程正是逐模块这样做的）是一种简化，一旦你学到 [Module 09](Lecture-09.md) 的完整内存模型——在那里这五个机制在同一个预算里相互作用——就应该有意识地撤销这种简化。

**第二，训练目标产生的是真正不同的权重，而不是一个更大模型权重的剪枝子集。** KDA 层的衰减门和 DSA 索引器的池化投影，这些参数只对*这个*架构才有意义 —— 底下并不存在一个“GLM-5.3 减去这些层”的检查点。实际上，这意味着你可能在剪枝或蒸馏模型上采用的技术（例如“恢复被移除层的权重以还原行为”）在这里并不适用。如果 GLM-5.3-Flash 的行为与 GLM-5.3 出现分歧，解决办法不是“把移除的东西放回去” —— 没有什么可放回去的。

---

## 5. 本模块*不*确立什么

要分清配置文件与架构描述给了你什么，以及什么需要推导或测量：

```text
   FROM THE CONFIG (this module)          REQUIRES DERIVATION (later modules)
   ───────────────────────────────        ────────────────────────────────────
   layer count, hidden width              routed-expert parameter total   (02)
   attention/FFN schedule                 per-token bytes moved by KDA    (04)
   expert count, top-k                    MLA cache size at a given ctx   (05)
   residual stream count                  indexer slot budget            (06)
   configured max context                 whether 1M context FITS on 8 GPUs (09)
```

右列就是本课程的其余部分。右列的内容单靠左列是猜不出来的 —— 全都必须推导，而且必须正确地推导，这就是为什么先有 [Modules 02–07](Lecture-02.md)，然后 [Module 09](Lecture-09.md) 中的内存模型才被允许使用它们的任何结果。

---


<details>
<summary>English original</summary>

**3. One token through the model**

```text
   Text tokens ──▶ embeddings ─────────────────────────────────┐
                                                                │
   Images ──▶ vision encoder ──▶ language-width features ──────┤
                                                                ▼
                                                    Four residual streams
                                                                │
                            ┌───────────────────────────────────┐
                            │  mHC collapse + normalization      │
                            │  KDA  or  sparse MLA (+ DSA select) │
                            │  mHC residual mixing                │
                            │                                     │
                            │  mHC collapse + normalization      │
                            │  Dense FFN  or  MoE                 │
                            │  mHC residual mixing                │
                            └───────────────────────────────────┘
                                          × 45 layers
                                                                │
                                                    Final stream contraction
                                                                │
                                                    Normalization + LM head
                                                                │
                                                      Next-token logits
```

This is a **conceptual map of the native model flow**, not a list of separately launched GPU kernels — a real implementation fuses, reorders, and batches heavily across this diagram, and [Module 10](Lecture-10.md) is where you learn to reason about that gap. But every box in this diagram corresponds to something you must be able to name and locate in checkpoint code:

| Box | Mechanism | Module |
|---|---|---|
| vision encoder → language-width features | multimodal projection | [08](Lecture-08.md) |
| mHC collapse / mixing | manifold-constrained hyper-connections | [07](Lecture-07.md) |
| KDA | recurrent linear attention | [03](Lecture-03.md), [04](Lecture-04.md) |
| sparse MLA (+ DSA select) | compressed latent attention + selective retrieval | [05](Lecture-05.md), [06](Lecture-06.md) |
| Dense FFN / MoE | sparse parameter activation | [02](Lecture-02.md) |

---

**4. Why "redesigned," not "distilled"**

It is tempting to read a hybrid model as "take an existing dense or MoE transformer and swap in a cheaper attention." Two facts argue against that reading, and both matter for how you approach the rest of this course:

**First, the mechanisms are co-designed, not bolted on.** KDA's channel-wise decay gate and MLA's latent width were chosen together with the DSA pooling width (4 tokens) and the mHC stream count (4) — these are not independent hyperparameters you could vary freely without touching the others' effective capacity. Studying one mechanism in isolation (which this course does, module by module, for pedagogical reasons) is a simplification you should consciously undo once you reach [Module 09](Lecture-09.md)'s full memory model, where all five interact in one budget.

**Second, the training objective produced genuinely different weights, not a pruned subset of a larger model's weights.** A KDA layer's decay gates and a DSA indexer's pooling projections are parameters that only make sense for *this* architecture — there is no "GLM-5.3 minus these layers" checkpoint hiding underneath. Practically, this means techniques you might reach for on a pruned or distilled model (e.g., "restore the removed layer's weights to recover behavior") do not apply here. If GLM-5.3-Flash's behavior diverges from GLM-5.3's, the fix is not "put back what was removed" — there is nothing to put back.

---

**5. What this module does *not* establish**

Be precise about what a config file and an architecture description give you, versus what requires derivation or measurement:

```text
   FROM THE CONFIG (this module)          REQUIRES DERIVATION (later modules)
   ───────────────────────────────        ────────────────────────────────────
   layer count, hidden width              routed-expert parameter total   (02)
   attention/FFN schedule                 per-token bytes moved by KDA    (04)
   expert count, top-k                    MLA cache size at a given ctx   (05)
   residual stream count                  indexer slot budget            (06)
   configured max context                 whether 1M context FITS on 8 GPUs (09)
```

The right-hand column is the rest of this course. Nothing there is guessable from the left-hand column alone — it all has to be derived, and derived correctly, which is why [Modules 02–07](Lecture-02.md) exist before the memory model in [Module 09](Lecture-09.md) is allowed to use any of their results.

---

</details>

## 检查点

你现在应该能够：

1. 凭记忆背出配置表的全部八行。
2. 仅凭重复块结构推导出 `34 KDA + 11 DSA/MLA = 45` 与 `42 MoE + 3 dense = 45`。
3. 指出 execution trace 图中的每一个方框，并说出本课程的哪个模块对其做了讲解。
4. 用一句话解释为什么「distilled from GLM-5.3」是错误的心智模型。
5. 把关于该模型的五条示例论断归类为「检查点事实」「需要推导」与「依赖部署」。

---

## 交付

构建一份**检查点到代码的张量映射**：对配置表的每一行，给出确切的 config 字段名、其取值，以及参考实现中消费该字段的模块/类。这是 [capstone ladder](Lecture-12.md) 的第 1 阶段，本课程后续每个模块都假定你已完成它——你会不断用到它。

---

## 时效说明

* **检查点特定：**§1 中的每个数字都是已发布的 GLM-5.3-Flash 配置的属性，截至本文写作时有效。未来的修订可能改变其中任何一个——在信任任何下游内容之前，请针对新配置重跑 §2 的推导。
* **长期有效：**「协同设计的混合架构」与「剪枝/蒸馏变体」之间的区别，以及把配置事实与推导性论断、依赖部署的论断区分开来的纪律。

---

**下一节：**[Module 02 — MoE（混合专家模型）：容量、工作与流量 →](Lecture-02.md)


<details>
<summary>English original</summary>

**Checkpoint**

You should now be able to:

1. Recite all eight rows of the configuration table from memory.
2. Derive `34 KDA + 11 DSA/MLA = 45` and `42 MoE + 3 dense = 45` from the repeating-block structure alone.
3. Point to each box in the execution-trace diagram and name which module of this course explains it.
4. Explain, in one sentence, why "distilled from GLM-5.3" is the wrong mental model.
5. Sort five example claims about this model into "checkpoint fact" versus "requires derivation" versus "deployment-dependent."

---

**Ship it**

Build a **checkpoint-to-code tensor map**: for each row of the configuration table, the exact config field name, its value, and the module/class in the reference implementation where it is consumed. This is Stage 1 of the [capstone ladder](Lecture-12.md), and every later module in this course assumes you have it — you will be reaching for it constantly.

---

**Current as of**

* **Checkpoint-specific:** every number in §1 is a property of the released GLM-5.3-Flash configuration, current as of this writing. A future revision can change any of them — re-run §2's derivation against the new config before trusting anything downstream.
* **Timeless:** the distinction between "co-designed hybrid architecture" and "pruned/distilled variant," and the discipline of separating config facts from derived and deployment-dependent claims.

---

**Next:** [Module 02 — MoE: Capacity, Work, and Traffic →](Lecture-02.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
