---
title: Jetson 上的 Gemma 4 — 边缘部署、PDL 与物理 AI
description: Jetson 上的 Gemma 4 — 边缘部署、PDL 与物理 AI
---

# Jetson 上的 Gemma 4 — 边缘部署、PDL 与物理 AI

<div class="course-identity edge-ai" markdown="1">
<div class="course-identity__icon">G4E</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 5 · 边缘 AI · 专题课程</p>
<p class="course-identity__title">在 Jetson 与边缘硬件上部署 Google 的 Gemma 4 家族 — 从架构内部到投机解码、PDL runtime 栈与多模态物理 AI 流水线。</p>
<p class="course-identity__meta">产物：在 Jetson 目标上端到端部署 Gemma 4，并实测 tok/s、KV-memory 与 TTFT · 度量：tokens/s、tok/s/W、TTFT、KV-cache GB、准确率 vs BF16 基线</p>
</div>
</div>

> *2025 年边缘 AI 的问题不再是“前沿质量的模型能否在端侧运行？”— Gemma 4 已回答。问题是“它在瓦特、字节与毫秒上要付出多少代价，以及你如何设计流水线来满足预算？”*

Gemma 4（2025 年 4 月，Google DeepMind）是首个同时在 27B 上具备 **前沿竞争力**、又在 1B–4B 上可 **边缘部署** 的开放模型家族 — 并非因为小模型有所妥协，而是因为架构明确针对受限算力做了协同设计。关键在 **交错式局部/全局 attention**：每 7 个 attention layer 中有 6 个是滑动窗口（KV 随上下文 O(1) 增长），使 128 K-token 上下文在 Jetson Orin 上变得可处理。在 Jetson AGX Thor 规模下（128 GB、273 GB/s），INT4 的 27B 模型运行时其 KV 占用比纯 attention 方案小一个数量级。

本课程是面向该部署的工程师手册。它配套三个将贯穿始终的概念：

**PDL — Portable Deployment and Loading** 是本课程用来指代从 Gemma 检查点到 Jetson 上运行服务的端到端流水线的术语：量化校准 → 格式转换 → runtime 编译 → 加载优化 → 推理服务配置。它对应 Google 的 **AI Edge / LiteRT** 生态外加推理优化层（分页 KV、投机解码、由 CUDA-graph 支撑的批处理）。每一讲都是该流水线中的一个阶段。

**Layer 映射：** L1–L6 — 模型架构、量化、编译、runtime、推理服务，以及它们之间的协同设计闭环。

**角色目标：** 边缘 AI 工程师 · 嵌入式 AI 工程师 · Jetson 推理工程师 · 物理 AI 系统工程师 · 端侧 ML 工程师。

**前置要求：**

* [Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md) — GEMV vs GEMM、roofline（性能上界模型）、带宽上限。本课程中的每一个数字都以此模型为依据。
* [Qwen Inference Optimization](../05-Qwen推理优化/README.md) — 量化、KV cache、Jetson 上的 decode（逐 token 生成阶段）优化。Gemma 4 覆盖同一套栈；该系列可补齐任何缺口。
* [MLSys Deep Dives → Lecture 04](../../07-方向G-ML系统工程/05-MLSys深度探索/Lecture-04.md) — SSM/混合架构与 KV cache 理论。Gemma 4 的交错式 attention 遵循同一设计原理。
* 熟悉 Python、`llama.cpp`/`ollama` CLI 与基本的 CUDA 性能剖析。

**配套课程：** [TVM Deep Dives](../../07-方向G-ML系统工程/06-TVM深度探索/README.md)（用于 MLC-LLM 编译）与 [AI Inference Engineer 2026](../../07-方向G-ML系统工程/01-AI推理工程师2026/README.md)（用于同一模型的数据中心侧）。

---

## 为什么 Gemma 4 改变了边缘格局

2025 年年中，另有三个开放模型家族在争夺边缘位置：**Qwen3**（阿里巴巴，在 4B–14B 上表现强劲）、**Phi-4**（微软，14B 稠密）与 **Llama 3.2**（Meta，1B/3B/11B）。Gemma 4 在三个维度上形成差异：

| 差异点 | Gemma 4 | Qwen3 | Phi-4 | Llama 3.2 |
|---|---|---|---|---|
| 交错式局部/全局 attention | **是（1:6）** | 否（纯全局） | 否 | 否 |
| 128K 上下文且 KV 有界 | **是** | 否（KV 增长） | 否 | 128K 但全量 KV |
| 同一家族中多模态 | **1B–27B 全部视觉** | 仅 7B+ | 否 | 仅 11B 视觉 |
| 量化稳定性（QK-norm） | **是** | 是（Qwen3） | 部分 | 否 |
| Google AI Edge / LiteRT 一等支持 | **是** | 否 | 否 | 否 |
| Apache-2.0 / Gemma ToS（生产） | Gemma ToS | Apache-2.0 | MIT | Llama ToS |

**交错式 attention** 是边缘处的架构护城河。在 128 K 上下文下，纯 attention 的 4B 模型需要约 18 GB 的 KV cache；Gemma 4 4B 需要约 3.3 GB — 缩减 5.5×。这就是模型能否装进 Jetson Orin 64 GB 的差别。

---


<details>
<summary>English original</summary>

**Gemma 4 on Jetson — Edge Deployment, PDL, and Physical AI**

<div class="course-identity edge-ai" markdown="1">
<div class="course-identity__icon">G4E</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 5 · Edge AI · Special Course</p>
<p class="course-identity__title">Deploy Google's Gemma 4 family on Jetson and edge hardware — from architecture internals to speculative decoding, PDL runtime stack, and multimodal physical-AI pipelines.</p>
<p class="course-identity__meta">Artifact: end-to-end Gemma 4 deployment with measured tok/s, KV-memory, and TTFT on a Jetson target · Measure: tokens/s, tok/s/W, TTFT, KV-cache GB, accuracy vs BF16 baseline</p>
</div>
</div>

> *The question for edge AI in 2025 is no longer "can a frontier-quality model run on device?" — Gemma 4 answers that. The question is "what does it cost in watts, bytes, and milliseconds, and how do you design the pipeline to meet the budget?"*

Gemma 4 (April 2025, Google DeepMind) is the first open model family that is simultaneously **frontier-competitive** at 27B and **edge-deployable** at 1B–4B — not because the small models are compromised, but because the architecture was explicitly co-designed for constrained compute. The key is **interleaved local/global attention**: 6 of every 7 attention layers are sliding-window (O(1) KV growth with context), making 128 K-token context tractable on a Jetson Orin. At Jetson AGX Thor scale (128 GB, 273 GB/s), the 27B model in INT4 runs with a KV footprint an order of magnitude smaller than a pure-attention equivalent.

This course is the engineer's manual for that deployment. It is paired with three concepts you will hear throughout:

**PDL — Portable Deployment and Loading** is the term this course uses for the end-to-end pipeline from a Gemma checkpoint to a running service on Jetson: quantization calibration → format conversion → runtime compilation → loading optimization → serving configuration. It maps to Google's **AI Edge / LiteRT** ecosystem plus the inference optimization layer (paged KV, speculative decoding, CUDA-graph-backed batching). Every lecture is one stage of this pipeline.

**Layer mapping:** L1–L6 — model architecture, quantization, compilation, runtime, serving, and the co-design loop between them.

**Role targets:** Edge AI Engineer · Embedded AI Engineer · Jetson Inference Engineer · Physical-AI Systems Engineer · On-Device ML Engineer.

**Prerequisites:**

* [Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md) — GEMV vs GEMM, roofline, bandwidth ceiling. Every number in this course is grounded in that model.
* [Qwen Inference Optimization](../05-Qwen推理优化/README.md) — quantization, KV cache, decode optimization on Jetson. Gemma 4 covers the same stack; that series fills any gaps.
* [MLSys Deep Dives → Lecture 04](../../07-方向G-ML系统工程/05-MLSys深度探索/Lecture-04.md) — SSM/hybrid architectures and KV cache theory. Gemma 4's interleaved attention is the same design principle.
* Comfort with Python, `llama.cpp`/`ollama` CLI, and basic CUDA profiling.

**Pairs with:** [TVM Deep Dives](../../07-方向G-ML系统工程/06-TVM深度探索/README.md) (for MLC-LLM compilation) and [AI Inference Engineer 2026](../../07-方向G-ML系统工程/01-AI推理工程师2026/README.md) (for the datacenter side of the same models).

---

**Why Gemma 4 Changes the Edge Landscape**

Three other open-model families were competing for the edge slot in mid-2025: **Qwen3** (Alibaba, strong at 4B–14B), **Phi-4** (Microsoft, 14B dense), and **Llama 3.2** (Meta, 1B/3B/11B). Gemma 4 differentiates on three dimensions:

| Differentiator | Gemma 4 | Qwen3 | Phi-4 | Llama 3.2 |
|---|---|---|---|---|
| Interleaved local/global attention | **Yes (1:6)** | No (pure global) | No | No |
| 128K context with bounded KV | **Yes** | No (KV grows) | No | 128K but full KV |
| Multimodal in same family | **1B–27B all vision** | 7B+ only | No | 11B vision only |
| Quantization stability (QK-norm) | **Yes** | Yes (Qwen3) | Partial | No |
| Google AI Edge / LiteRT first-class | **Yes** | No | No | No |
| Apache-2.0 / Gemma ToS (production) | Gemma ToS | Apache-2.0 | MIT | Llama ToS |

The **interleaved attention** is the architectural moat at the edge. At 128 K context, a pure-attention 4B model needs ~18 GB of KV cache; Gemma 4 4B needs ~3.3 GB — a 5.5× reduction. That is the difference between a model that fits on a Jetson Orin 64 GB and one that doesn't.

---

</details>

## 课程地图（6 讲）

<div class="lecture-map" markdown>

| # | 讲次 | 主线 |
|---|---------|-----------|
| [01](Lecture-01.md) | **为什么在边缘使用 Gemma 4** — 面向边缘工程师的架构：交错 attention、分组查询注意力、QK-norm、带线性 KV 的 128K、模型阵容与竞争定位 | 支持 Gemma 4 的理由 |
| [02](Lecture-02.md) | **量化与格式转换** — Gemma 4 的 GPTQ/AWQ/K-quants、校准、GGUF/LiteRT/ExecuTorch/.pt2/TRT 格式、准确率与速度的取舍 | 从权重到比特 |
| [03](Lecture-03.md) | **PDL Runtime 栈** — LiteRT（Google AI Edge）、llama.cpp、MLC-LLM（TVM Unity）、TensorRT-LLM：选型矩阵，Orin 与 Thor 上的延迟/吞吐概况 | runtime 选择 |
| [04](Lecture-04.md) | **使用 Gemma 4 的投机解码** — 1B 草稿模型 + 4B/12B 目标模型、EAGLE-3 风格的自草稿头、前瞻解码、边缘硬件上的接受长度分析 | 算法层 |
| [05](Lecture-05.md) | **物理 AI 与多模态 Gemma 4** — SigLIP-400M 视觉编码器、Jetson 上的 VLM 流水线、机器人感知用例、延迟预算、综合项目 | 闭环 |
| [06](Lecture-06.md) | **使用 Gemma 4 E2B 的 MTP** — 4 层 E2B MTP 草稿器架构、目标状态感知的提议、在 vLLM / LiteRT-LM / llama.cpp 上部署、E2B vs 1B vs EAGLE-3 决策矩阵 | 官方草稿器 |

</div>

---

## 课程成果

学完后，你应该能够：

* 解释 Gemma 4 的交错 attention 架构为何在长上下文下产生**有界的 KV 占用**，并计算任意 (batch, seq, model) 配置下的精确 KV 内存。
* 为给定 Jetson 目标选择正确的量化格式和 runtime，并根据带宽上限公式预测吞吐。
* 使用至少两种不同 runtime，在 Jetson Orin 或 Thor 上将 Gemma 4 从检查点部署到推理服务，并给出实测 tokens/s 和 TTFT。
* 配置使用 Gemma 4 1B 草稿模型和 4B/12B 目标模型的投机解码，报告接受长度，并验证输出精度一致性。
* 描述多模态 VLM 流水线（SigLIP 编码器 + Gemma 解码器），并将其纳入机器人的延迟预算。

---

## 达成标准

当你能够做到以下事项时，就算完成了本课程：

* 在目标上的任意 Gemma 4 配置中运行 `bandwidth_ceiling(HBM_GBs, model_bytes) → max_tok_per_s`，并解释它触及的是哪一物理上限。
* 生成一张逐级测量的部署表：基线 BF16 → INT8 → INT4 → INT4 + 投机解码，并给出每一级的 tok/s、KV-GB 和 TTFT。
* 看到新发布的边缘模型（任意架构）时，立即问“KV cache 的增长率是多少，在我的目标上下文长度下它是否适合我的内存预算？”— 这就是本课程旨在培养的分析习惯。

---

## 时效性 / 刷新纪律

Gemma 4 于 2025 年 4 月发布。部署生态发展迅速：

* 每讲结束时都会给出一个 **`## Current as of`** 日期，并固定具体版本 / benchmark 数字。
* 厂商报告的数字（Google 的 benchmark 声明、NVIDIA 的 Jetson Thor 吞吐表）会被**明确标注**，并视为教学锚点，而非真实基准。
* runtime 栈（LiteRT 版本、llama.cpp GGUF 支持、MLC-LLM dlight 调度）每月都在变化——始终对照带标签的 release 重新验证。

---

*相关：[Qwen 推理优化](../05-Qwen推理优化/README.md) · [边缘 LLM 推理内幕](../03-边缘LLM推理内部机制/Lecture-01.md) · [MLSys 深度剖析](../../07-方向G-ML系统工程/05-MLSys深度探索/README.md)*


<details>
<summary>English original</summary>

**Course Map (6 lectures)**

<div class="lecture-map" markdown>

| # | Lecture | The thread |
|---|---------|-----------|
| [01](Lecture-01.md) | **Why Gemma 4 at the Edge** — architecture for edge engineers: interleaved attention, GQA, QK-norm, 128K with linear KV, model lineup and competitive positioning | the case for Gemma 4 |
| [02](Lecture-02.md) | **Quantization and Format Conversion** — GPTQ/AWQ/K-quants for Gemma 4, calibration, GGUF/LiteRT/ExecuTorch/.pt2/TRT formats, accuracy vs speed tradeoffs | weights to bits |
| [03](Lecture-03.md) | **The PDL Runtime Stack** — LiteRT (Google AI Edge), llama.cpp, MLC-LLM (TVM Unity), TensorRT-LLM: selection matrix, latency/throughput profiles on Orin and Thor | runtime choice |
| [04](Lecture-04.md) | **Speculative Decoding with Gemma 4** — 1B draft + 4B/12B target, EAGLE-3-style self-draft heads, lookahead decoding, acceptance-length analysis on edge hardware | the algorithm layer |
| [05](Lecture-05.md) | **Physical AI and Multimodal Gemma 4** — SigLIP-400M vision encoder, VLM pipeline on Jetson, robot perception use cases, latency budget, capstone | closing the loop |
| [06](Lecture-06.md) | **MTP with Gemma 4 E2B** — 4-layer E2B MTP drafter architecture, target-state-aware proposals, deployment on vLLM / LiteRT-LM / llama.cpp, E2B vs 1B vs EAGLE-3 decision matrix | the official drafter |

</div>

---

**Course Outcomes**

By the end you should be able to:

* Explain why Gemma 4's interleaved attention architecture produces a **bounded KV footprint** at long context, and compute the exact KV memory for any (batch, seq, model) configuration.
* Select the right quantization format and runtime for a given Jetson target, and predict throughput from the bandwidth-ceiling formula.
* Deploy Gemma 4 from checkpoint to serving on a Jetson Orin or Thor using at least two different runtimes, with measured tokens/s and TTFT.
* Configure speculative decoding with a Gemma 4 1B draft and a 4B/12B target, report the acceptance length, and verify output parity.
* Describe the multimodal VLM pipeline (SigLIP encoder + Gemma decoder) and fit it within a robot's latency budget.

---

**Exit Criteria**

You are done with this course when you can:

* Run `bandwidth_ceiling(HBM_GBs, model_bytes) → max_tok_per_s` on any Gemma 4 configuration on your target and explain which physical bound it's hitting.
* Produce a deployment table with rung-by-rung measurements: baseline BF16 → INT8 → INT4 → INT4 + speculative decode, with tok/s, KV-GB, and TTFT at each rung.
* Look at a new edge model release (any architecture) and immediately ask "what is the KV cache growth rate, and does it fit my memory budget at my target context length?" — the analytical habit this course exists to build.

---

**Currency / Refresh Discipline**

Gemma 4 launched April 2025. The deployment ecosystem is moving fast:

* Every lecture closes with a **`## Current as of`** date and the specific versions / benchmark numbers pinned.
* Vendor-reported numbers (Google's benchmark claims, NVIDIA's Jetson Thor throughput sheets) are **explicitly flagged** and treated as teaching anchors, not ground truth.
* The runtime stack (LiteRT versions, llama.cpp GGUF support, MLC-LLM dlight schedules) changes monthly — always re-verify against the tagged release.

---

*Related: [Qwen Inference Optimization](../05-Qwen推理优化/README.md) · [Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md) · [MLSys Deep Dives](../../07-方向G-ML系统工程/05-MLSys深度探索/README.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Gemma 4 Edge Deployment/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Gemma%204%20Edge%20Deployment/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
