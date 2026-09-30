---
title: 结业项目 — KDA 优先的精通阶梯
description: 结业项目 — KDA 优先的精通阶梯
---

# 结业项目 — KDA 优先的精通阶梯

**合集：** [GLM-5.3-Flash Architecture Mastery](README.md) | **上一篇：** [← 模块 11](Lecture-11.md) | **下一篇：** [课程索引](README.md)

---

十一个模块产出了推导、演算示例与逐机制的测试设计。本结业项目把它们转成一份**构建顺序** — 八个分阶段交付物，每一个都是下一个的前置要求，最终落在真实部署上的一次经验证的优化。这架阶梯刻意不从"整个 320B 模型"开始，而是从**一个 KDA head** 开始。

---

## 1. 为什么先从一个 KDA head 开始，而不是整个模型

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   Start with ONE KDA head, not the entire 320B-parameter model.     │
   └────────────────────────────────────────────────────────────────────┘
```

本课程中其他每一个机制（MoE（混合专家模型）布线、MLA 的吸收代数、DSA 的池化）复杂在它的*簿记*上 — 众多专家、众多 head、众多池化位置 — 但一旦你看清它，每一个单独运算都相对简单。KDA 恰恰相反：**一个 head 的更新是一个五行的递推式，你可以手算**（[模块 03 §4](Lecture-03.md)），但把它的算子顺序搞错，会产生一个在多步上静默发散、而非直接崩溃的状态。它同时是最容易为它构建首个参考实现的机制，也是最容易在细微处、悄无声息地搞错的机制。正是这一组合，使它成为培养最初严谨习惯的正确起点 — 先在这里养成，再同时用到五个机制上。

一旦你能准确解释前缀之后必须保存什么、乘法顺序为什么重要、以及这个递推式如何变成一个高效的 GPU 计算，你就拥有了整门课程一直在铺垫的基础 — 剩下的七个阶段，就是把同样的严谨向外施加到架构的其余部分。

---

## 2. 八个阶段

<div class="lecture-map" markdown>

| 阶段 | 交付物 | 体现精通之处 | 构建于 |
|---|---|---|---|
| 1 | 架构审计 | 一份 checkpoint 到代码的张量映射 — 你能定位每一个主要维度、状态与投影 | [模块 01](Lecture-01.md) |
| 2 | KDA 参考实现 | 一个小型 FP32 递推实现 — 你能推导每一次更新并解释其顺序 | [模块 03](Lecture-03.md) |
| 3 | KDA 执行 | 递推/分块对比 — 输出与最终状态在边界处一致 | [模块 04](Lecture-04.md) |
| 4 | MLA 实验室 | 展开与潜空间 attention — 你能证明等价性并测量访存流量差异 | [模块 05](Lecture-05.md) |
| 5 | KPool 实验室 | 池化、选择、尾部与 mask 测试 — 边界与 padding 用例正确 | [模块 06](Lecture-06.md) |
| 6 | MoE + mHC 审计 | 路由器与残差流测试 — 你保持选择、加权、钳位与流语义 | [模块 02](Lecture-02.md) + [07](Lecture-07.md) |
| 7 | 八 GPU 成本模型 | 预测与实测的内存和延迟对比 — 你解释差异而非掩盖它们 | [模块 09](Lecture-09.md) |
| 8 | 优化研究 | 一项已验证的端到端改进 — 加速比能在重复运行与正确性门禁下成立 | 模块 [10](Lecture-10.md) + [11](Lecture-11.md) |

</div>

注意这架阶梯的形状：**阶段 1–6 完全关乎正确性** — 逐个机制地构建参考实现并证明等价性。只有阶段 7–8 才涉及性能，而且阶段 8 明确以阶段 6 的正确性测试套件先通过为门禁。这个顺序是刻意的，也呼应了课程的核心论点：你无法负责任地优化一个你尚未证明自己能正确实现的机制，而本课程中的每一个模块，都是为了让这种证明能一次针对一个具体机制发生。

---


<details>
<summary>English original</summary>

**Capstone — The KDA-First Mastery Ladder**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 11](Lecture-11.md) | **Next:** [Course index](README.md)

---

Eleven modules produced derivations, worked examples, and per-mechanism test designs. This capstone turns them into a **build order** — eight staged deliverables, each one a prerequisite for the next, ending in one validated optimization on a real deployment. The ladder deliberately does not start with "the entire 320B model." It starts with **one KDA head.**

---

**1. Why start with one KDA head, not the whole model**

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   Start with ONE KDA head, not the entire 320B-parameter model.     │
   └────────────────────────────────────────────────────────────────────┘
```

Every other mechanism in this course (MoE routing, MLA's absorption algebra, DSA's pooling) is complex in its *bookkeeping* — many experts, many heads, many pooled positions — but each individual operation is comparatively simple once you see it clearly. KDA is the opposite: **one head's update is a five-line recurrence you can compute by hand** ([Module 03 §4](Lecture-03.md)), but getting its operator ordering wrong produces a state that silently diverges over many steps rather than crashing outright. It is simultaneously the easiest mechanism to build a first reference implementation for and the easiest one to get subtly, invisibly wrong. That combination is exactly why it is the right place to build your first habits of rigor before applying them to five mechanisms at once.

Once you can explain exactly what must be saved after a prefix, why the multiplication order matters, and how the recurrence becomes an efficient GPU computation, you have the foundation this entire course was building toward — and the remaining seven stages are that same rigor, applied outward to the rest of the architecture.

---

**2. The eight stages**

<div class="lecture-map" markdown>

| Stage | Deliverable | What demonstrates mastery | Built in |
|---|---|---|---|
| 1 | Architecture audit | A checkpoint-to-code tensor map — you can locate every major dimension, state, and projection | [Module 01](Lecture-01.md) |
| 2 | KDA reference | A small FP32 recurrent implementation — you can derive every update and explain its order | [Module 03](Lecture-03.md) |
| 3 | KDA execution | Recurrent/chunked comparison — outputs and final states agree across boundaries | [Module 04](Lecture-04.md) |
| 4 | MLA laboratory | Expanded and latent-space attention — you can prove equivalence and measure the traffic difference | [Module 05](Lecture-05.md) |
| 5 | KPool laboratory | Pooling, selection, tail, and mask tests — boundary and padding cases are correct | [Module 06](Lecture-06.md) |
| 6 | MoE + mHC audit | Router and residual-flow tests — you preserve selection, weighting, clamping, and stream semantics | [Modules 02](Lecture-02.md) + [07](Lecture-07.md) |
| 7 | Eight-GPU cost model | Predicted versus measured memory and latency — you explain discrepancies instead of hiding them | [Module 09](Lecture-09.md) |
| 8 | Optimization study | One validated end-to-end improvement — the speedup survives repeated runs and a correctness gate | Modules [10](Lecture-10.md) + [11](Lecture-11.md) |

</div>

Notice the shape of the ladder: **stages 1–6 are entirely about correctness** — building reference implementations and proving equivalence, mechanism by mechanism. Only stages 7–8 touch performance at all, and stage 8 is explicitly gated on stage 6's correctness suite passing first. This ordering is deliberate and mirrors the course's central argument: you cannot responsibly optimize a mechanism you have not first proven you can implement correctly, and every module in this course exists to make that proof possible for one specific mechanism at a time.

---

</details>

## 3. 来源导航，按顺序

对每个阶段，按以下顺序从一手来源向外展开：

```text
   1. the checkpoint configuration itself     — ground truth for every dimension (Module 01)
   2. the Transformers reference implementation — the canonical, if not fastest, semantics
   3. the serving implementation in your fork  — what actually runs in production,
                                                  including every fused/reordered
                                                  optimization Module 10 warned you
                                                  the conceptual diagram doesn't show
   4. the KDA and mHC papers                   — for WHY the mathematics has this
                                                  particular structure, not as a
                                                  substitute for reading the actual
                                                  checkpoint implementation
```

这个顺序很重要：先读论文并假设检查点与论文完全一致，就是“generic SwiGLU”替换掉“clamped SwiGLU”（[Module 02 §3](Lecture-02.md)）在某人脑中的方式。论文解释意图；检查点及其参考实现才是实际契约。

---

## 4. 阶段 8 详解：优化研究

阶段 1–7 产出理解与成本模型。阶段 8 才是这种理解体现价值之处—并且它有自己的一套内在纪律，直接借鉴自 [Module 11](Lecture-11.md) 和 [Hardware-Aware LLM Quantization — Module 12](../03-硬件感知LLM量化/Lecture-12.md)：

```text
   1.  PICK ONE HYPOTHESIS from Module 10 §2's table — not "make it faster,"
       but something falsifiable: "the KDA decode kernel is register-spill-bound
       at head dimension 128," or "MoE dispatch is launch-bound below batch 8."

   2.  PREDICT the outcome using the relevant module's roofline or cost model
       BEFORE writing the optimization.

   3.  SPECIFY THE CORRECTNESS CONTRACT before benchmarking (Module 11 §4) —
       which of the Module 11 §2 test-matrix rows this change must still pass,
       and at what numerical tolerance.

   4.  IMPLEMENT the change.

   5.  RUN THE FULL Module 11 correctness suite. A speedup that fails
       the suite is not a result — it is a different, unvalidated model
       that happens to run faster.

   6.  MEASURE repeatedly, on a machine in a known state, and report
       the result whether or not it matches your stage-2 prediction.
       A wrong prediction that you can explain is worth more than a
       correct one you can't.
```

**一个加速比只有在通过第 5 步后，才能在这一流程中存活。** 这一原则与 [Module 11](Lecture-11.md) 构建其整个正确性矩阵所围绕的原则相同，只是被用作最终门禁而非事后补丁：该架构上的性能工作，不是在 benchmark 数字提升时就算完成，而是在 benchmark 数字提升 *且* 阶段 1–6 中每个机制特定的正确性测试仍然通过时，才算完成。

---


<details>
<summary>English original</summary>

**3. Source navigation, in order**

For each stage, work outward from primary sources in this order:

```text
   1. the checkpoint configuration itself     — ground truth for every dimension (Module 01)
   2. the Transformers reference implementation — the canonical, if not fastest, semantics
   3. the serving implementation in your fork  — what actually runs in production,
                                                  including every fused/reordered
                                                  optimization Module 10 warned you
                                                  the conceptual diagram doesn't show
   4. the KDA and mHC papers                   — for WHY the mathematics has this
                                                  particular structure, not as a
                                                  substitute for reading the actual
                                                  checkpoint implementation
```

That ordering matters: reading a paper first and assuming the checkpoint matches it exactly is how "generic SwiGLU" replaces "clamped SwiGLU" ([Module 02 §3](Lecture-02.md)) in someone's mental model. Papers explain intent; the checkpoint and its reference implementation are the actual contract.

---

**4. Stage 8, in detail: the optimization study**

Stages 1–7 produce understanding and a cost model. Stage 8 is where that understanding earns its keep — and it has its own internal discipline, borrowed directly from [Module 11](Lecture-11.md) and from [Hardware-Aware LLM Quantization — Module 12](../03-硬件感知LLM量化/Lecture-12.md):

```text
   1.  PICK ONE HYPOTHESIS from Module 10 §2's table — not "make it faster,"
       but something falsifiable: "the KDA decode kernel is register-spill-bound
       at head dimension 128," or "MoE dispatch is launch-bound below batch 8."

   2.  PREDICT the outcome using the relevant module's roofline or cost model
       BEFORE writing the optimization.

   3.  SPECIFY THE CORRECTNESS CONTRACT before benchmarking (Module 11 §4) —
       which of the Module 11 §2 test-matrix rows this change must still pass,
       and at what numerical tolerance.

   4.  IMPLEMENT the change.

   5.  RUN THE FULL Module 11 correctness suite. A speedup that fails
       the suite is not a result — it is a different, unvalidated model
       that happens to run faster.

   6.  MEASURE repeatedly, on a machine in a known state, and report
       the result whether or not it matches your stage-2 prediction.
       A wrong prediction that you can explain is worth more than a
       correct one you can't.
```

**A speedup survives this process only if it passes step 5.** This is the same principle [Module 11](Lecture-11.md) built its entire correctness matrix around, applied as the final gate rather than an afterthought: performance work on this architecture is not complete when the benchmark number improves, it is complete when the benchmark number improves *and* every mechanism-specific correctness test from stages 1–6 still passes.

---

</details>

## 5. 在这条阶梯的终点，“精通”意味着什么

当你能拿到检查点配置，在不再翻阅本课程的情况下做到以下各点，本课程即算完成：

* 仅凭 layer 数量就能重建 attention 调度与 FFN 切分（[Module 01](Lecture-01.md)）。
* 推导 router 的选择与加权之分，以及 clamped-SwiGLU 的非对称性，并说明缺少其中任何一项会引发的具体 bug（[Module 02](Lecture-02.md)）。
* 凭记忆写出 KDA 的五步更新，将其展开为闭式形式，并解释为什么衰减算子与修正算子不可交换（[Module 03](Lecture-03.md)）。
* 解释为什么对同一个递推，decode（逐 token 生成阶段）与 prefill（首字前的整段计算）需要不同的 KDA kernel，并说出该 sublayer 中每一个非递推组成部分（[Module 04](Lecture-04.md)）。
* 推导 MLA 的吸收代数与 64× 缓存比，并解释为什么 NoPE 不会让模型对顺序视而不见（[Module 05](Lecture-05.md)）。
* 计算 DSA 的 indexer slot 预算（含不完整的尾部），并说出值得测试的边界长度（[Module 06](Lecture-06.md)）。
* 解释为什么四条 mHC 残差流不等于四个 attention 模块，45 层也不等于 45/4 个有效层（[Module 07](Lecture-07.md)）。
* 针对这一特定的混合架构，列举一次推测式回滚必须对齐的每一份状态（[Module 08](Lecture-08.md)）。
* 构建一份单 GPU 内存预算，且不把每一项都天真地除以张量并行度（[Module 09](Lecture-09.md)）。
* 为模型的七个不同区域各给出一个可证伪的性能剖析假设，并知道暴露的与重叠的跨 GPU 通信开销之间的区别（[Module 10](Lecture-10.md)）。
* 在 benchmark 之前先规定数值正确性契约，并说出那个能抓住回滚或 chunking bug 的不变量 —— 这类 bug 仅靠逐 token 输出比对本会漏掉（[Module 11](Lecture-11.md)）。

如果你能背出五个机制的名字 —— MoE（混合专家模型）、KDA、MLA、DSA、mHC —— 却无法从检查点配置里的各维度推导出其中任何一个的更新方程，那你只拥有词汇。这条阶梯存在的目的，就是把那些词汇转化为一种能力：安全地修改这个模型，而不在无声中改变它。

---

## 时效说明

* **不随时间变化：** 八阶段的构建顺序，以及阶段 8 所依据的“先正确性、后性能”准则。
* **与检查点和部署相关：** 阶段 7 的成本模型，以及贯穿本课程的案例研究（8× RTX 5090、PCIe Gen4、无 P2P、NVFP4、`sparkinfer-frontier`），都绑定于这一具体部署 —— 而阶梯本身（阶段 1–6，以及阶段 8 的准则）原样适用于任何混合架构：稀疏 MoE、循环式线性 attention、压缩潜在 attention、选择性 retrieval、多流残差，无论其具体维度最后是多少。

---

*课程结束。[← 返回课程索引](README.md) · [阶段 5 — ML Systems Engineering Guide](../Guide.md)*


<details>
<summary>English original</summary>

**5. What "mastery" means at the end of this ladder**

You have completed this course when you can take the checkpoint configuration and, without consulting this course again:

* Reconstruct the attention schedule and FFN split from the layer count alone ([Module 01](Lecture-01.md)).
* Derive the router's selection-versus-weighting split and the clamped-SwiGLU asymmetry, and explain the specific bug each one's absence would cause ([Module 02](Lecture-02.md)).
* Write the KDA five-step update from memory, expand it to closed form, and explain why the decay and correction operators don't commute ([Module 03](Lecture-03.md)).
* Explain why decode and prefill need different KDA kernels for the same recurrence, and name every non-recurrence component of the sublayer ([Module 04](Lecture-04.md)).
* Derive MLA's absorption algebra and the 64× cache ratio, and explain why NoPE doesn't make the model order-blind ([Module 05](Lecture-05.md)).
* Compute DSA's indexer slot budget including the incomplete tail, and name the boundary lengths worth testing ([Module 06](Lecture-06.md)).
* Explain why four mHC residual streams is not four attention modules and 45 layers is not 45/4 effective layers ([Module 07](Lecture-07.md)).
* Enumerate every piece of state a speculative rollback must reconcile for this specific hybrid architecture ([Module 08](Lecture-08.md)).
* Build a per-GPU memory budget that does not naively divide every term by the tensor-parallel degree ([Module 09](Lecture-09.md)).
* State a falsifiable profiling hypothesis for each of seven distinct regions of the model, and know the difference between an exposed and an overlapped cross-GPU communication cost ([Module 10](Lecture-10.md)).
* Specify a numerical correctness contract before benchmarking, and name the invariant that catches a rollback or chunking bug that per-token output comparison alone would miss ([Module 11](Lecture-11.md)).

If you can recite the five mechanism names — MoE, KDA, MLA, DSA, mHC — but cannot derive any one of their update equations from the dimensions in the checkpoint config, you have vocabulary. The ladder exists to convert that vocabulary into the ability to safely modify this model without silently changing it.

---

**Current as of**

* **Timeless:** the eight-stage build order and the correctness-before-performance discipline underlying stage 8.
* **Checkpoint- and deployment-specific:** stage 7's cost model and the case study threaded through this course (8× RTX 5090, PCIe Gen4, no P2P, NVFP4, `sparkinfer-frontier`) are tied to this specific deployment — the ladder itself (stages 1–6, and the discipline of stage 8) applies unchanged to any hybrid architecture combining sparse MoE, recurrent linear attention, compressed latent attention, selective retrieval, and multi-stream residuals, whatever its specific dimensions turn out to be.

---

*Course complete. [← Back to the course index](README.md) · [Phase 5 — ML Systems Engineering Guide](../Guide.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-12.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-12.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
