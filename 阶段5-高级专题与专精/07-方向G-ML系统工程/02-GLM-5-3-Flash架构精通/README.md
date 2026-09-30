---
title: GLM-5.3-Flash 架构精通
description: GLM-5.3-Flash 架构精通
---

# GLM-5.3-Flash 架构精通

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">Sₜ</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 5 · ML 系统工程 · 首席工程师深度剖析</p>
<p class="course-identity__title">推导计算过程、预测内存流量、找出真正的推理服务瓶颈，并在不悄悄改变模型的前提下修改实现。</p>
<p class="course-identity__meta">产物：一个经过验证的 per-GPU 内存模型 + 在 8× RTX 5090 部署上的一项以正确性为门控的优化 · 度量：按机制统计的 bytes/token、逐算子 roofline（性能上界模型）、跨执行路径的输出/状态一致性</p>
</div>
</div>

> *看懂一张架构图并不是目标。能够从五行代码重建递推、预测它搬运的字节数，并证明你的 kernel 没有悄悄改变模型——这才是目标。*

GLM-5.3-Flash 并不是层数更少或精度更低的 GLM-5.3。Z.ai 描述的是一套**重新设计的混合架构**：稀疏参数激活（MoE，混合专家模型）、循环序列记忆（KDA）、压缩的逐 token 历史（MLA）、选择性历史检索（DSA/KPool），以及流形约束的多流残差（mHC）——五种机制，各自针对不同的开销，组合进同一个 decoder。把其中任意两种当作可互换，或假定一种涵盖了另一种，是误读这个模型最常见的方式。

**目标部署：**一套 8× RTX 5090（PCIe Gen4，无 P2P）系统，以推理服务方式运行 NVFP4 检查点——贯穿模块 09–12 的 `sparkinfer-frontier` 案例研究。
**层级：**首席 AI 工程师。本课程假定你已能读懂 CUDA 性能分析器并推导 roofline；这两者都不再重新讲授。

**层级映射：**L4–L7——从模型数学到多 GPU 推理服务。它直接位于 [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md)（本检查点*就是*一次 NVFP4 部署）之上，之下则没有任何内容——这就是 Track G 其余工具所服务的架构。

**目标岗位：**首席/Staff 推理工程师 · ML 系统工程师（混合架构）· GPU runtime 工程师 · 模型推理服务正确性工程师

---

## 前置要求

| 前置要求 | 为什么需要它 |
|---|---|
| [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) | 本检查点以 NVFP4 方式提供推理服务。这里的模块 09 直接复用该课程的 bits-per-weight 计算——本课程不再重新推导块缩放。 |
| [AI Inference Engineer 2026 — Part 3](../01-AI推理工程师2026/03-Blackwell-MoE/README.md) | 通用的 MoE/MLA 推理服务模式（专家并行、prefill（首字前的整段计算）/decode（逐 token 生成阶段）分离）。本课程深入某一个具体的混合模型；Part 3 提供通用的推理服务词汇。 |
| [Logprobs, Perplexity & KL Divergence](../04-Logprobs-困惑度与KL散度/README.md) | 模块 11 的正确性矩阵依赖 KL/一致性评分，来区分「代数等价」与「模型被悄悄改变」。 |
| 线性代数熟练度 | 模块 03 的递推推导是矩阵代数，不是空谈。你应当能在不用计算机的情况下熟练展开 `(I − βkkᵀ)D`。 |
| CUDA + roofline 熟练度 | 模块 10 假定你已经知道带宽受限 kernel 在 Nsight Compute 里长什么样。 |

**搭配：**[MLSys Deep Dives — Lecture 04](../05-MLSys深度探索/Lecture-04.md)（Mamba/SSM 的状态 vs KV 缓存经济性——与 KDA 在此提出的「循环状态取代不断增长的缓存」是同一个论证）以及 [AI Inference Engineer 2026 — Part 4](../01-AI推理工程师2026/04-真实引擎优化/README.md)（同样的「先度量后优化」纪律，只是换了一个真实的 engine）。

---

## 五种机制，各自独立

这条唯一规则能避免对本架构的大多数误读：

```text
   MoE   decides WHICH PARAMETERS execute           (capacity vs. arithmetic vs. traffic)
   KDA   compresses SEQUENCE HISTORY into fixed state (a memory, not a growing list)
   MLA   compresses the REPRESENTATION per token      (still token-indexed, just smaller)
   DSA   decides WHICH POSITIONS get read             (selection, separate from representation)
   mHC   changes HOW INFORMATION FLOWS between sublayers (connectivity, not computation)
```

它们互补，而非可互换。mHC 的四条残差流并不会把 45 层的串行深度除以四。Prefill/decode 分离是一项*部署*决策，不是第六种机制。本课程的每一个模块，都是为了让这五种机制中的某一种足够精确，使你无法把它与相邻的机制混淆。

---


<details>
<summary>English original</summary>

**GLM-5.3-Flash Architecture Mastery**

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">Sₜ</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 5 · ML Systems Engineering · Principal-Engineer Deep Dive</p>
<p class="course-identity__title">Derive the computations, predict the memory traffic, find the real serving bottleneck, and modify the implementation without silently changing the model.</p>
<p class="course-identity__meta">Artifact: a validated per-GPU memory model + one correctness-gated optimization on an 8× RTX 5090 deployment · Measure: bytes/token by mechanism, per-operator roofline, output/state agreement across execution paths</p>
</div>
</div>

> *Understanding an architecture diagram is not the goal. Being able to rebuild the recurrence from five lines, predict the bytes it moves, and prove your kernel didn't quietly change the model — that is the goal.*

GLM-5.3-Flash is not GLM-5.3 with fewer layers or lower precision. Z.ai describes a **redesigned hybrid architecture**: sparse parameter activation (MoE), recurrent sequence memory (KDA), compressed per-token history (MLA), selective historical retrieval (DSA/KPool), and manifold-constrained multi-stream residuals (mHC) — five mechanisms, each attacking a different cost, composed into one decoder. Treating any two of them as interchangeable, or assuming one subsumes another, is the single most common way to misread this model.

**Target deployment:** an 8× RTX 5090 (PCIe Gen4, no P2P) system serving the NVFP4 checkpoint — the `sparkinfer-frontier` case study threaded through Modules 09–12.
**Level:** principal AI engineer. This course assumes you can already read a CUDA profiler and derive a roofline; it does not re-teach either.

**Layer mapping:** L4–L7 — model mathematics through multi-GPU serving. It sits directly on top of [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) (this checkpoint *is* an NVFP4 deployment) and below nothing — this is the architecture the rest of Track G's tooling serves.

**Role targets:** Principal/Staff Inference Engineer · ML Systems Engineer (hybrid architectures) · GPU Runtime Engineer · Model-Serving Correctness Engineer

---

**Prerequisites**

| Prerequisite | Why you need it |
|---|---|
| [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) | This checkpoint is served in NVFP4. Module 09 here reuses that course's bits-per-weight arithmetic directly — this course does not re-derive block scaling. |
| [AI Inference Engineer 2026 — Part 3](../01-AI推理工程师2026/03-Blackwell-MoE/README.md) | General MoE/MLA serving patterns (expert parallelism, disaggregated prefill/decode). This course goes deeper on one specific hybrid model; Part 3 gives you the general serving vocabulary. |
| [Logprobs, Perplexity & KL Divergence](../04-Logprobs-困惑度与KL散度/README.md) | Module 11's correctness matrix leans on KL/agreement grading to distinguish "algebraically equivalent" from "silently different model." |
| Linear algebra fluency | Module 03's recurrence derivation is matrix algebra, not hand-waving. You should be comfortable multiplying out `(I − βkkᵀ)D` without a computer. |
| CUDA + roofline fluency | Module 10 assumes you already know what a memory-bound kernel looks like in Nsight Compute. |

**Pairs with:** [MLSys Deep Dives — Lecture 04](../05-MLSys深度探索/Lecture-04.md) (Mamba/SSM state-vs-KV-cache economics — the same "recurrent state replaces a growing cache" argument KDA makes here) and [AI Inference Engineer 2026 — Part 4](../01-AI推理工程师2026/04-真实引擎优化/README.md) (the same measure-before-you-optimize discipline, on a different real engine).

---

**The five mechanisms, kept separate**

The single rule that prevents most misreadings of this architecture:

```text
   MoE   decides WHICH PARAMETERS execute           (capacity vs. arithmetic vs. traffic)
   KDA   compresses SEQUENCE HISTORY into fixed state (a memory, not a growing list)
   MLA   compresses the REPRESENTATION per token      (still token-indexed, just smaller)
   DSA   decides WHICH POSITIONS get read             (selection, separate from representation)
   mHC   changes HOW INFORMATION FLOWS between sublayers (connectivity, not computation)
```

They are complementary, not interchangeable. mHC's four residual streams do not divide the 45-layer serial depth by four. Prefill/decode disaggregation is a *deployment* decision, not a sixth mechanism. Every module in this course exists to make one of these five precise enough that you cannot confuse it with its neighbors.

---

</details>

## 课程地图（12 个模块）

<div class="lecture-map" markdown>

| # | 模块 | 主线 |
|---|--------|-----------|
| [01](Lecture-01.md) | **精确模型** — checkpoint 配置、执行 trace、为何这是一个新的混合模型而不是蒸馏得到的 GLM-5.3 | 在推导任何结论之前先确立 ground truth |
| [02](Lecture-02.md) | **MoE：容量、工作与流量** — 带 correction bias 的 sigmoid router、clamped SwiGLU 专家、304.4B 参数的推导 | 三个常被混为一谈的量 |
| [03](Lecture-03.md) | **KDA I：Delta 规则递推** — 五步更新、加框的闭式形式、一个手算数值示例、state 与 checkpoint 权重之别 | 这一块最先、最深入地掌握 |
| [04](Lecture-04.md) | **KDA II：分块并行与完整子层** — 支持并行训练的仿射复合论证、为何 prefill 需要的 kernel 与 decode 不同、递推没告诉你的全部内容 | 递推不等于整个 layer |
| [05](Lecture-05.md) | **MLA：压缩逐 token 历史** — 联合 latent、避免重新展开历史的吸收代数、64× 缓存比、为何 NoPE 不是对顺序不敏感 | 仍按 token 索引，只是更小 |
| [06](Lecture-06.md) | **DSA 与 KPool：选择性检索** — 池化索引、2,051 个 slot 的预算、池边界处的因果性、为何固定 top-k 不是 O(1) | 池化不是删除 |
| [07](Lecture-07.md) | **mHC：流形约束的残差流** — 双随机混合矩阵、Sinkhorn 归一化、为何四条流不是四个 attention 模块 | 是连接性，不是计算 |
| [08](Lecture-08.md) | **视觉、MTP 与混合推理服务状态** — 多模态路径、为何投机回滚需要恢复递推状态*以及*卷积状态*以及* latent 状态 | 独立的子系统，独立的计时器 |
| [09](Lecture-09.md) | **8-GPU 显存模型** — 权重存储下界、KDA/MLA 的每请求预算、张量并行的复制陷阱、完整的每 GPU 方程 | 不要把所有项都除以 8 |
| [10](Lecture-10.md) | **Kernel Roofline 与推理服务决策** — 逐算子的下界模型、按区域划分的 profiling 假设表、disaggregation 的完整状态交接要求 | 是实验，不是预设结论 |
| [11](Lecture-11.md) | **把正确性作为架构掌握度** — 完整测试矩阵、prefix/chunk/continuation 不变量、在 benchmark 之前先规定数值契约 | 没有声明契约的加速不算结果 |
| [12](Lecture-12.md) | **结课项目：以 KDA 为先的掌握阶梯** — 从一个 KDA head 到经验证的端到端优化，共八级阶段交付物 | 是构建顺序，不是阅读顺序 |

</div>

---

## 课程成果

学完之后你应当能够：

* 读懂 checkpoint 配置并定位每一个维度 —— `d_model`、KDA head 数量/维度、MLA latent 宽度、indexer 池宽度、MoE 专家数量/top-k、mHC 流数量 —— 无需参照任何图示。
* 推导 MoE router 的实际分数（**选择**用 sigmoid + correction bias，**加权**用重新归一化后的原始 sigmoid 分数），并解释这一区分为何对正确性至关重要。
* 由五个具名操作写出 KDA delta 规则更新，将其展开为闭式 `(I − βkkᵀ)D·S + βkvᵀ`，并手算复现一个示例。
* 解释为何分块/并行的 KDA 实现必须在**输出与最终状态**两方面都与递推参考实现一致 —— 而不只是在某一个 prompt 上的输出。
* 推导 MLA 的吸收代数（`q̃ = (Uᴷ)ᵀq`），并准确说明它对 softmax 改变了什么、没有改变什么。
* 计算 DSA 的 indexer slot 预算，包含不完整尾块的情形，并指出因果性 bug 集中出现的上下文长度（3、4、5、7、8、9）。
* 说明为何 mHC 的残差混合不降低串行深度，以及为何“四条流”是连接性的改变，而不是四个 Transformer。
* 为一个张量并行的混合模型构建完整的每 GPU 显存方程，且**不**天真地把每一项都除以 TP 度。
* 在测量加速比*之前*先规定数值正确性契约，并指出能捕获 KV 长度检查所遗漏的回滚 bug 的不变量。

---

## 本课程不是什么

* 不是通用的 MoE/MLA 推理服务课程 —— [AI Inference Engineer 2026 — Part 3](../01-AI推理工程师2026/03-Blackwell-MoE/README.md) 已泛化地覆盖了那部分内容。
* 不是对 NVFP4 的重新推导 —— [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) 已经做过；本课程只是引用它。
* 并非声称此处的每个数字都经过独立复测。归因于已发布 checkpoint 配置或 `sparkinfer-frontier` 仓库的数字均按来源引用，而不是作为本课程所做的测量呈现 —— 这一区分贯穿全篇且至关重要，尤其是在模块 09。

---



---


<details>
<summary>English original</summary>

**Course Map (12 modules)**

<div class="lecture-map" markdown>

| # | Module | The thread |
|---|--------|-----------|
| [01](Lecture-01.md) | **The Exact Model** — checkpoint configuration, the execution trace, why this is a new hybrid model rather than a distilled GLM-5.3 | establish ground truth before deriving anything |
| [02](Lecture-02.md) | **MoE: Capacity, Work, and Traffic** — the sigmoid router with correction bias, clamped SwiGLU experts, and the 304.4B-parameter derivation | three quantities people conflate |
| [03](Lecture-03.md) | **KDA I: The Delta-Rule Recurrence** — the five-step update, the boxed closed form, a worked numeric example, state vs. checkpoint weights | master this most deeply first |
| [04](Lecture-04.md) | **KDA II: Chunked Parallelism & the Full Sublayer** — the affine-composition argument for parallel training, why prefill needs a different kernel than decode, everything the recurrence doesn't tell you | the recurrence is not the whole layer |
| [05](Lecture-05.md) | **MLA: Compressing Per-Token History** — the joint latent, the absorption algebra that avoids re-expanding history, the 64× cache ratio, why NoPE isn't order-blindness | still token-indexed, just smaller |
| [06](Lecture-06.md) | **DSA & KPool: Selective Retrieval** — pooled indexing, the 2,051-slot budget, causality at pool boundaries, why fixed top-k isn't O(1) | pooling is not deletion |
| [07](Lecture-07.md) | **mHC: Manifold-Constrained Residual Streams** — the doubly-stochastic mixing matrix, Sinkhorn normalization, why four streams isn't four attention modules | connectivity, not computation |
| [08](Lecture-08.md) | **Vision, MTP, and Hybrid Serving State** — the multimodal path, why speculative rollback needs to restore recurrent *and* convolution *and* latent state | separate subsystems, separate timers |
| [09](Lecture-09.md) | **The 8-GPU Memory Model** — weight-storage lower bounds, KDA/MLA per-request budgets, the tensor-parallel replication trap, the full per-GPU equation | don't divide everything by 8 |
| [10](Lecture-10.md) | **Kernel Roofline & Serving Decisions** — the per-operator lower-bound model, a profiling hypothesis table by region, disaggregation's full state-handoff requirement | experiments, not predetermined conclusions |
| [11](Lecture-11.md) | **Correctness as Architecture Mastery** — the full test matrix, the prefix/chunk/continuation invariant, specifying the numerical contract before benchmarking | a speedup with no stated contract isn't a result |
| [12](Lecture-12.md) | **Capstone: The KDA-First Mastery Ladder** — eight staged deliverables from one KDA head to a validated end-to-end optimization | build order, not reading order |

</div>

---

**Course Outcomes**

By the end you should be able to:

* Read the checkpoint config and place every dimension — `d_model`, KDA head count/dim, MLA latent width, indexer pool width, MoE expert count/top-k, mHC stream count — without consulting a diagram.
* Derive the MoE router's actual score (sigmoid + correction bias for **selection**, raw sigmoid scores renormalized for **weighting**) and explain why that split matters for correctness.
* Write the KDA delta-rule update from the five named operations, expand it into the closed-form `(I − βkkᵀ)D·S + βkvᵀ`, and reproduce a worked example by hand.
* Explain why a chunked/parallel KDA implementation must match both **output and final state** against the recurrent reference — not just outputs on one prompt.
* Derive MLA's absorption algebra (`q̃ = (Uᴷ)ᵀq`) and state exactly what it does and does not change about the softmax.
* Compute DSA's indexer slot budget including the incomplete-tail case, and name the context lengths (3, 4, 5, 7, 8, 9) where causality bugs cluster.
* State why mHC's residual mixing does not reduce serial depth, and why "four streams" is a connectivity change, not four transformers.
* Build a full per-GPU memory equation for a tensor-parallel hybrid model that does **not** naively divide every term by the TP degree.
* Specify a numerical correctness contract *before* measuring a speedup, and name the invariant that catches a rollback bug a KV-length check would miss.

---

**What this course is not**

* Not a general MoE/MLA serving course — [AI Inference Engineer 2026 — Part 3](../01-AI推理工程师2026/03-Blackwell-MoE/README.md) covers that ground generically.
* Not a re-derivation of NVFP4 — [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) already did that; this course cites it.
* Not a claim that every number here was independently re-measured. Figures attributed to the published checkpoint configuration or to the `sparkinfer-frontier` repository are cited as such, not presented as measurements taken for this course — the distinction is load-bearing throughout, especially in Module 09.

---

</details>

## 时效性 / 刷新纪律

* **不随时间变化：** delta-rule 代数、MLA absorption 恒等式、roofline 下界模型（性能上界模型）、正确性契约纪律。
* **随检查点变化：** 模块 01 配置表中的每一个维度、304.4B/18B 参数切分、indexer 预算。如果未来发布 GLM-5.x-Flash 修订版，在信任其下游任何内容之前，先对照其配置重新核验模块 01 —— 后续每个模块的算术都依赖这些数字。
* **随 runtime 变化：** 哪个推理服务栈（SGLang、vLLM、TensorRT-LLM）具备融合 KDA kernel、chunked-prefill 支持（prefill：首字前的整段计算），以及针对这一特定混合 state 形状的 disaggregation。模块 10 是刷新入口。
* 每个模块都以一条 **`## Current as of`** 说明收尾，把已确定的数学与依赖检查点、依赖工具链的事实区分开。

---

## 达成标准

当你仅凭检查点配置、不查阅任何资料就能做到以下各点时，即告完成：

* 根据层数重建 attention 调度（`[KDA,KDA,KDA,DSA/MLA]×11 + KDA`）以及 dense/MoE FFN 切分。
* 推导出每个专家的参数数量与路由专家总数，误差在取整范围内。
* 用五行写出 KDA 更新，并准确展开为闭式解。
* 针对五种机制中的任意一种，说明它降低了哪项开销，以及它*不*影响其余四种中的哪些。
* 针对给定的上下文长度和张量并行度，逐项构建每 GPU 显存方程，并正确标注每一项是分片、复制、请求相关还是瞬态。
* 说出一个仅凭“输出看起来合理”就判定 benchmark 通过时会漏掉的数值不变量。

如果你能背出五种机制的名称，却推导不出其中任何一个更新方程，那你只掌握了词汇。这门课的重点是推导。

---

*相关：[Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) · [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) · [MLSys Deep Dives](../05-MLSys深度探索/README.md) · [阶段 5 — ML Systems Engineering Guide](../Guide.md)*


<details>
<summary>English original</summary>

**Currency / Refresh Discipline**

* **Timeless:** the delta-rule algebra, the MLA absorption identity, the roofline lower-bound model, the correctness-contract discipline.
* **Moves with the checkpoint:** every dimension in Module 01's config table, the 304.4B/18B parameter split, the indexer budget. If a future GLM-5.x-Flash revision ships, re-verify Module 01 against its config before trusting anything downstream of it — every later module's arithmetic depends on those numbers.
* **Moves with the runtime:** which serving stack (SGLang, vLLM, TensorRT-LLM) has a fused KDA kernel, chunked-prefill support, and disaggregation for this specific hybrid state shape. Module 10 is the refresh surface.
* Every module closes with a **`## Current as of`** note separating settled math from checkpoint- and tooling-specific facts.

---

**Exit Criteria**

You are done when you can take the checkpoint config alone and, without looking anything up:

* Reconstruct the attention schedule (`[KDA,KDA,KDA,DSA/MLA]×11 + KDA`) and the dense/MoE FFN split from the layer count.
* Derive the per-expert parameter count and the routed-expert total to within rounding.
* Write out the KDA update in five lines and expand it to the closed form without error.
* State, for any one of the five mechanisms, which cost it reduces and which of the other four it does *not* affect.
* Build the per-GPU memory equation for a stated context length and tensor-parallel degree, term by term, correctly marking each as sharded, replicated, request-dependent, or transient.
* Name a numerical invariant that a benchmark passing on "the output looks reasonable" would miss.

If you can recite the five mechanism names but cannot derive any of their update equations, you have vocabulary. The point of this course is the derivation.

---

*Related: [Hardware-Aware LLM Quantization](../03-硬件感知LLM量化/README.md) · [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) · [MLSys Deep Dives](../05-MLSys深度探索/README.md) · [Phase 5 — ML Systems Engineering Guide](../Guide.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
