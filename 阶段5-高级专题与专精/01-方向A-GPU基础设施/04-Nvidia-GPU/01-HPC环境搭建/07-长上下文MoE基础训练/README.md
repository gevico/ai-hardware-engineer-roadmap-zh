---
title: 长上下文 MoE（混合专家模型）基础模型训练
description: 长上下文 MoE（混合专家模型）基础模型训练
---

# 长上下文 MoE（混合专家模型）基础模型训练

**上级：** [HPC Setup](../Guide.md)（高性能计算）

**形式：** 10 个模块。理论 → 系统机制 → 实验产物。每个模块交付一项可度量的输出（配置 diff、性能分析器 trace、训练运行日志、benchmark 表或 eval CSV）。

**本课程为何存在。** 多数“训练大模型”教程止步于单节点微调。多数“长上下文”教程止步于 attention kernel。本课程覆盖规模化训练**长上下文、混合专家基础模型**真正需要的东西——attention 数学、位置编码、专家路由、分布式并行、自适应数据与诚实评估构成的联合系统。到课程结束时，你能够读懂 NVIDIA Megatron / NeMo Bridge 代码，设计出适配自身硬件的 context parallel + expert parallel mesh，调试 router collapse，运行一项不被困惑度蒙蔽的长上下文评估，并说清模型在训练装置之外为何有用、为何无用。

---

## 学完之后你能做到什么

- 推导长序列长度下 attention 与 MLP 的内存 + 计算伸缩，并预测二者分别在何处成为主导开销。
- 构建长度课程，让 4K 预训练的模型真正有用地达到 256K 上下文，而不只是达到名义最大长度。
- 针对目标上下文长度选择位置编码策略（RoPE base scaling、YaRN、position interpolation），并给出具体取舍。
- 搭建带 top-k 路由、capacity factor 调优、负载均衡损失与 router z-loss 的稀疏 MoE FFN 块；调试 router collapse。
- 为 8x 或 64x H200 集群规划训练 mesh，组合数据并行、张量并行、流水线并行、序列并行、上下文并行与专家并行。
- 构建自适应数据流水线，把评估的失效模式转化为下一轮训练样本。
- 运行真实的长上下文评估（RULER、LongBench、带干扰项的 needle-in-haystack、multi-hop、codebase reasoning），并如实报告结果。
- 严谨论证模型是普遍表现好，还是只在狭窄的测试分布上好。

---

## 前置要求

- 阶段 4 方向 B（Jetson、CUDA 基础、TensorRT）。
- 阶段 4 方向 C 第 01–02 单元（图优化、kernel 工程）。
- [FlashAttention 系统 / kernel 课程](../../../../../阶段4-路线C-深度学习推理优化/02-内核工程/01-FlashAttention课程/Guide.md)（本课程依赖 IO/roofline（性能上界模型）+ online-softmax 的心智模型）。
- HPC Setup [NCCL Deep Dive](../08-NCCL深度探索/README.md) 与 [8× H200 训练/推理](../01-8x-H200训练推理/README.md) 模块。
- 至少能实际使用一台 8× H100/H200 节点。真实实验假定可访问多节点；较小的实验在单台 8-GPU 节点上运行。

---

## 课程大纲

| # | 模块 | 实验产物 |
|---|--------|--------------|
| 01 | [为什么长上下文很难](01-Long-Context-Bottlenecks.md) | 跨 `N ∈ {4K, 32K, 128K, 1M}` 的内存 + 计算伸缩表；找出 attention 压过 MLP 的交叉点 |
| 02 | [长上下文 attention 机制](02-Long-Context-Attention.md) | 单节点上的 FlashAttention + 上下文并行 benchmark；激活值显存随序列长度的曲线 |
| 03 | [长上下文的位置编码](03-Position-Encoding.md) | 在 32K → 128K 下比较 RoPE base scaling vs YaRN vs position interpolation 的逐位置检索准确率曲线 |
| 04 | [MoE 基础](04-MoE-Fundamentals.md) | PyTorch 中的 top-k MoE FFN 层；展示 aux loss 权重对专家利用率影响的消融 |
| 05 | [MoE 系统与基础设施](05-MoE-Systems-Infrastructure.md) | all-to-all dispatch 微基准；带 token 丢弃率的 capacity factor 扫描 |
| 06 | [自适应数据流水线](06-Adaptive-Data-Pipelines.md) | 一次闭环迭代：失效分析 → 定向数据 → 重新训练 → 重新评估 |
| 07 | [长上下文评估](07-Long-Context-Evaluation.md) | RULER + LongBench harness，产出按长度、按任务的细分结果 |
| 08 | [长上下文 + MoE 结合](08-Combining-LongContext-and-MoE.md) | mesh 布局决策表；一个真实配置的通信开销拆解 |
| 09 | [分布式训练基础设施](09-Distributed-Training-Infrastructure.md) | 带检查点、恢复与容错测试的可运行多节点训练 |
| 10 | [从实验到通用模型](10-General-Purpose-Model.md) | 在**训练分布之外**的任务上与开源基线对比的 harness |

---


<details>
<summary>English original</summary>

**Long-Context MoE Foundation Model Training**

**Parent:** [HPC Setup](../Guide.md)

**Format:** 10 modules. Theory → systems mechanics → lab artifact. Each module ships a measurable output (config diff, profiler trace, training run log, benchmark table, or eval CSV).

**Why this course exists.** Most "train a big model" tutorials stop at single-node fine-tuning. Most "long context" tutorials stop at the attention kernel. This course covers what is actually required to train a **long-context, Mixture-of-Experts foundation model** at scale — the joint system of attention math, position encoding, expert routing, distributed parallelism, adaptive data, and honest evaluation. By the end you can read NVIDIA Megatron / NeMo Bridge code, design a context-parallel + expert-parallel mesh that fits your hardware, debug a router collapse, run a long-context evaluation that is not gamed by perplexity, and explain why your model is or is not useful outside the training rig.

---

**What you will be able to do at the end**

- Derive the memory + compute scaling of attention vs MLP at long sequence lengths and predict where each becomes the dominant cost.
- Build a length curriculum that takes a 4K-pretrained model to 256K-context usefully, not just to nominal max length.
- Pick a positional encoding strategy (RoPE base scaling, YaRN, position interpolation) for a target context length with concrete trade-offs.
- Set up a sparse MoE FFN block with top-k routing, capacity-factor tuning, load-balancing loss, and router z-loss; debug router collapse.
- Lay out a training mesh combining data, tensor, pipeline, sequence, context, and expert parallelism for an 8x or 64x H200 cluster.
- Build an adaptive data pipeline that turns evaluation failure modes into next-iteration training examples.
- Run real long-context evaluations (RULER, LongBench, needle-in-haystack with distractors, multi-hop, codebase reasoning) and report results honestly.
- Argue rigorously about whether your model is good in general or good only on a narrow test distribution.

---

**Prerequisites**

- Phase 4 Track B (Jetson, CUDA basics, TensorRT).
- Phase 4 Track C Units 01–02 (graph optimization, kernel engineering).
- [FlashAttention systems / kernel course](../../../../../阶段4-路线C-深度学习推理优化/02-内核工程/01-FlashAttention课程/Guide.md) (this course depends on the IO/roofline + online-softmax mental model).
- HPC Setup [NCCL Deep Dive](../08-NCCL深度探索/README.md) and [8× H200 Training/Inference](../01-8x-H200训练推理/README.md) modules.
- Working access to at least one 8× H100/H200 node. Real labs assume multi-node access; the smaller labs run on a single 8-GPU node.

---

**Syllabus**

| # | Module | Lab artifact |
|---|--------|--------------|
| 01 | [Why long context is hard](01-Long-Context-Bottlenecks.md) | Memory + compute scaling table across `N ∈ {4K, 32K, 128K, 1M}`; identify the crossover where attention dominates MLP |
| 02 | [Long-context attention mechanics](02-Long-Context-Attention.md) | FlashAttention + context-parallel benchmark on one node; activation-memory plot vs sequence length |
| 03 | [Positional encoding for long context](03-Position-Encoding.md) | Per-position retrieval-accuracy plot comparing RoPE base scaling vs YaRN vs position interpolation at 32K → 128K |
| 04 | [MoE fundamentals](04-MoE-Fundamentals.md) | Top-k MoE FFN layer in PyTorch; ablation showing aux-loss-weight effect on expert utilization |
| 05 | [MoE systems and infrastructure](05-MoE-Systems-Infrastructure.md) | All-to-all dispatch micro-benchmark; capacity-factor sweep with dropped-token rate |
| 06 | [Adaptive data pipelines](06-Adaptive-Data-Pipelines.md) | One closed-loop iteration: failure analysis → targeted data → retrain → re-eval |
| 07 | [Long-context evaluation](07-Long-Context-Evaluation.md) | RULER + LongBench harness producing per-length, per-task breakdown |
| 08 | [Combining long-context + MoE](08-Combining-LongContext-and-MoE.md) | Mesh-layout decision table; communication-cost breakdown for one realistic config |
| 09 | [Distributed training infrastructure](09-Distributed-Training-Infrastructure.md) | Working multi-node training run with checkpoint, resume, and fault-tolerance test |
| 10 | [From experiment to general-purpose model](10-General-Purpose-Model.md) | Comparison harness against an open-source baseline on tasks **outside** your training distribution |

---

</details>

## 如何使用本课程

- 按顺序完成各模块。每个模块都假设你已掌握上一模块的系统原语。
- 配合本课程阅读 NVIDIA 的 Megatron / NeMo Bridge MoE（混合专家模型）+ 长上下文文档。本课程只是导览，上游代码才是最终依据。
- 把所有训练配置、评测 CSV 和性能分析器 trace 都放在同一个 `lcm-course/` 工作目录下，这样 capstone（模块 10）就能与更早的运行结果做 diff。
- 评估是一等产物。对模型或数据的每一次改动，都必须搭配一个评测 delta——而不只是一条 loss 曲线。

---

## 核心资料

- NeMo Megatron Bridge — 长上下文 + MoE 训练 recipe：<https://docs.nvidia.com/nemo/megatron-bridge/nightly/>
- Megatron-LM（核心分布式训练原语）：<https://github.com/NVIDIA/Megatron-LM>
- DeepSpeed MoE：<https://www.deepspeed.ai/tutorials/mixture-of-experts/>
- FlashAttention：<https://github.com/Dao-AILab/flash-attention>
- Effective Long-Context Scaling of Foundation Models（Meta）：<https://arxiv.org/abs/2309.16039>
- YaRN：<https://arxiv.org/abs/2309.00071>
- RULER 长上下文 benchmark：<https://github.com/hsiehjackson/RULER>
- LongBench：<https://github.com/THUDM/LongBench>
- ICML 2025 Long Context Foundation Models workshop：<https://longcontextfm.github.io/>

---

## 角色映射

- **MTS 分布式训练 / 训练基础设施工程师** — 技能直接对口。capstone 产物采用招聘经理读得懂的形式。
- **MTS Kernel / DL 推理优化** — 依赖 FlashAttention 课程；本课程在其基础上扩展了训练侧的关注点（重计算、ZeRO、上下文并行）。
- **ML 研究工程师（长上下文、agent、代码库模型）** — 模块 03、06、07、10 直接对应研究-工程闭环。
- **AI 基础设施架构师** — 模块 05、08、09 涵盖你将负责的 mesh 设计与多节点决策。

---

## 本课程不是什么

- 不是一门讲新型长上下文架构的研究课程。本课程采用经过验证的、已在生产环境使用的设计（rotary scaling、top-k MoE、ring/上下文并行），聚焦于如何把它们落地上线。
- 不是一份泛泛的“训练大语言模型”教程。每个模块都假设你已理解 Transformer 训练基础，并且正在扩展到单节点做不到的规模。
- 不会穷尽 MoE 的各种变体。本课程聚焦 token-choice top-k routing；提到 expert-choice、hash routing 和 soft MoE，只是为了指出下一步该读什么。


<details>
<summary>English original</summary>

**How to use this course**

- Do the modules in order. Each one assumes the systems primitives from the previous one.
- Read NVIDIA's Megatron / NeMo Bridge MoE + long-context docs alongside the course. The course is a tour guide; the upstream code is the ground truth.
- Keep all training configs, eval CSVs, and profiler traces in a single `lcm-course/` working directory so the capstone (Module 10) can diff against earlier runs.
- Evaluation is a first-class artifact. Every change to the model or data must be paired with an eval delta — not just a loss curve.

---

**Core sources**

- NeMo Megatron Bridge — long-context + MoE training recipes: <https://docs.nvidia.com/nemo/megatron-bridge/nightly/>
- Megatron-LM (core distributed training primitives): <https://github.com/NVIDIA/Megatron-LM>
- DeepSpeed MoE: <https://www.deepspeed.ai/tutorials/mixture-of-experts/>
- FlashAttention: <https://github.com/Dao-AILab/flash-attention>
- Effective Long-Context Scaling of Foundation Models (Meta): <https://arxiv.org/abs/2309.16039>
- YaRN: <https://arxiv.org/abs/2309.00071>
- RULER long-context benchmark: <https://github.com/hsiehjackson/RULER>
- LongBench: <https://github.com/THUDM/LongBench>
- ICML 2025 Long Context Foundation Models workshop: <https://longcontextfm.github.io/>

---

**Role mapping**

- **MTS Distributed Training / Training Infrastructure Engineer** — direct skill match. Capstone artifact is in the form a hiring manager can read.
- **MTS Kernels / DL Inference Optimization** — depends on the FlashAttention course; this course extends it with the training-side concerns (recomputation, ZeRO, context parallel).
- **ML Research Engineer (long-context, agents, codebase models)** — Modules 03, 06, 07, 10 map directly to the research-engineering loop.
- **AI Infrastructure Architect** — Modules 05, 08, 09 cover the mesh-design and multi-node decisions you will own.

---

**What this course is not**

- Not a research course on novel long-context architectures. We use proven, in-production designs (rotary scaling, top-k MoE, ring/context parallel) and focus on how to ship them.
- Not a generic "train an LLM" tutorial. Every module assumes you already understand transformer training basics and are scaling beyond what a single node can do.
- Not exhaustive on MoE variants. We focus on token-choice top-k routing; we mention expert-choice, hash routing, and soft MoE only to point at where to read next.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
