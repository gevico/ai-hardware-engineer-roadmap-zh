---
title: 第 1 部分 — AI 推理基础 / MLSys
description: 第 1 部分 — AI 推理基础 / MLSys
---

# 第 1 部分 — AI 推理基础 / MLSys

本课程的心智模型层。五讲按顺序构建每个 AI 推理工程师在接触特定模型或硬件目标之前都需要掌握的四件事：

1. **角色是什么** — 四种推理形态、关键指标、诊断流程。
2. **Transformer 实际执行什么** — prefill（首字前的整段计算）与 decode（逐 token 生成阶段）、作为承重结构的 KV cache、三种受限模式（算力受限 / 内存受限 / 调度器受限）。
3. **硬件做什么** — roofline（性能上界模型）、带宽、存储层次，以及营销包装的规格实际能换来什么。
4. **2026 年的精度意味着什么** — FP16 → FP8 → FP4 → INT4，以及让量化保持诚实的精度一致性验证纪律。
5. **存在哪些 runtime 以及为什么** — vLLM、SGLang、TensorRT-LLM、llama.cpp、MLX，以及用于在它们之间进行选择的工作负载矩阵。

到第 1 部分结束时，读者应能打开一个自己从未听说过的模型的模型卡，并预测——在由数学支撑的误差范围内——其 KV cache 的开销将是多少、其 decode 带宽上限位于何处、它能容忍哪一档精度下限，以及哪个 runtime 才是正确的起点。随后，模型相关的部分（第 2 部分 dense / 第 3 部分 MoE（混合专家模型））会分别用两个具体的锚点对来加深该心智模型。

## 讲座

<div class="lecture-map" markdown>

| # | 标题 | 它回答的核心问题 |
|---|-------|--------------------------|
| 01 | [2026 年推理工程师的心智模型](Lecture-01.md) | 这个角色日复一日*做*什么，哪些指标决定工作做得好不好？ |
| 02 | [Transformer 执行 — 从 token 到 bit](Lecture-02.md) | 生成一个 token 时 GPU 上实际运行什么，以及为什么 decode 是带宽受限的问题？ |
| 03 | [Roofline、带宽与存储层次](Lecture-03.md) | 哪些硬件规格项会影响哪个指标，哪些只是噪声？ |
| 04 | [精度栈 — FP16 → FP8 → FP4 → INT4](Lecture-04.md) | 每个精度下限的代价是什么，每个又能换来什么，以及我们如何*知道*精度一致性？ |
| 05 | [runtime 全景 — vLLM、SGLang、TensorRT-LLM、llama.cpp、MLX](Lecture-05.md) | 给定一个工作负载 + 硬件 + SLO，我们从哪个 runtime 开始 — 以及为什么？ |

</div>

## 第 1 部分要交付什么

* 一个可运行的 benchmark 模板仓库，能够针对一个模型启动一个 runtime，并输出首 token 时延（TTFT）/ TPOT / 吞吐 / 峰值内存。
* 一份针对某个模型卡（由你选择）的简短书面分析，推导其 KV cache 开销、主导成本区间，以及推荐的 runtime + 精度起点。
* 一张针对一个目标 GPU + 一个特定 kernel 的 roofline 图，展示算术强度 vs 达到的带宽。

你将在第 2 部分和第 3 部分中一直使用这个模板仓库。

## 达成标准

当你能做到以下事项时，就准备好进入第 2 部分：

* 在白板上画出 Transformer 前向传播中 prefill 与 decode 的划分，并标出在 batch=1 时哪个阶段是算力受限、哪个阶段是带宽受限。
* 从任意模型的 `config.json` 计算其 KV cache 的每 token 字节数公式。
* 说明已发布模型能容忍哪一档精度下限而无需从头重新验证 — 以及不能容忍哪一档。
* 用两句话论证，对于给定工作负载，为什么你会选择 vLLM 而不是 TensorRT-LLM（或反之）。

如果其中任何一项还不牢固，在继续之前重读对应的讲座。第 2 部分假定你已经掌握全部四项。


<details>
<summary>English original</summary>

**Part 1 — Fundamentals of AI Inference / MLSys**

The mental model layer of the course. Five lectures that build, in order, the four things every AI inference engineer needs before touching a specific model or hardware target:

1. **What the role is** — the four inference shapes, the metrics that matter, the diagnostic flow.
2. **What a transformer actually executes** — prefill vs decode, the KV cache as the load-bearing structure, the three regimes (compute / memory / scheduler bound).
3. **What the hardware does** — roofline, bandwidth, the memory hierarchy, what the marketing-shaped specs actually buy.
4. **What precision means in 2026** — FP16 → FP8 → FP4 → INT4 and the parity validation discipline that keeps quantization honest.
5. **What runtimes exist and why** — vLLM, SGLang, TensorRT-LLM, llama.cpp, MLX, and the workload matrix for picking between them.

By the end of Part 1, a reader should be able to open a model card for a model they have never heard of, and predict — within a margin defended by the math — what its KV cache will cost, where its decode bandwidth ceiling sits, which precision floor it will tolerate, and which runtime is the right starting point. The model-specific parts (Part 2 dense / Part 3 MoE) then deepen that mental model with two concrete anchor pairs each.

**Lectures**

<div class="lecture-map" markdown>

| # | Title | Core question it answers |
|---|-------|--------------------------|
| 01 | [The 2026 inference engineer's mental model](Lecture-01.md) | What does the role *do*, day to day, and what metrics decide whether the work was good? |
| 02 | [Transformer execution — from tokens to bits](Lecture-02.md) | What actually runs on the GPU when a token is generated, and why decode is the bandwidth-bound problem? |
| 03 | [Roofline, bandwidth, and the memory hierarchy](Lecture-03.md) | Which hardware spec lines move which metric, and which are noise? |
| 04 | [The precision stack — FP16 → FP8 → FP4 → INT4](Lecture-04.md) | What does each precision floor cost, what does each one buy, and how do we *know* parity? |
| 05 | [The runtime landscape — vLLM, SGLang, TensorRT-LLM, llama.cpp, MLX](Lecture-05.md) | Given a workload + hardware + SLO, which runtime do we start with — and why? |

</div>

**What you ship from Part 1**

* A worked benchmark template repo that can boot one runtime against one model and emit TTFT / TPOT / throughput / peak memory.
* A short written analysis of one model card (your choice) that derives its KV-cache cost, dominant-cost regime, and a recommended runtime + precision starting point.
* A roofline plot for one target GPU + one specific kernel showing arithmetic intensity vs achieved bandwidth.

You will use this template repo throughout Parts 2 and 3.

**Exit criteria**

You are ready for Part 2 when you can:

* Sketch the prefill-vs-decode split of a transformer forward pass on a whiteboard and label which stage is compute-bound and which is bandwidth-bound at batch=1.
* Compute the KV cache bytes-per-token formula for any model from its `config.json`.
* State which precision floor a published model can tolerate without re-validating from scratch — and which it cannot.
* Defend, in two sentences, why you would pick vLLM over TensorRT-LLM (or vice versa) for a given workload.

If any of these is shaky, re-read the matching lecture before moving on. Part 2 assumes you have all four.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 1 - Fundamentals/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%201%20-%20Fundamentals/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
