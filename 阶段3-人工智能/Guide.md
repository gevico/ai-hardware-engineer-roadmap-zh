---
title: 阶段 3：人工智能 —— 你的硬件必须运行的工作负载
description: 阶段 3：人工智能 —— 你的硬件必须运行的工作负载
---

# 阶段 3：人工智能 —— 你的硬件必须运行的工作负载

<div class="course-identity ai-workloads" markdown="1">
<div class="course-identity__icon">AI</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 3 · AI 工作负载</p>
<p class="course-identity__title">了解你的硬件实际必须运行的模型、框架与应用模式。</p>
<p class="course-identity__meta">产物：工作负载画像 · 度量：计算、内存、精度、延迟</p>
</div>
</div>


> *在设计硬件之前，你必须深入理解它所加速的软件。*

**层映射：** **L1**（应用与框架）—— 整个阶段都在教你 AI 芯片计算什么。

**目标岗位：** ML 推理工程师 · 边缘 AI 工程师 · 智能体化 AI 工程师 · ML 工程师 · AI 编译器工程师

**前置要求：** 阶段 1（数字基础）、阶段 2（嵌入式系统）。

**后续内容：** 阶段 4 方向 A（FPGA）、方向 B（Jetson）、方向 C（ML 编译器）。

---

## 为什么需要这个阶段

8 层栈中的每一个决策都由工作负载需求驱动。如果跳过这个阶段，你会在不了解硬件需要运行什么的情况下设计硬件。阶段 3 给你工作负载直觉，它影响着下游的每一个硬件决策。

---

## 结构：核心 + 两个方向

**模块 1–2** 对所有人都是必修。之后你选择 **方向 A**、**方向 B**，或两者都选。

```
Module 1: Neural Networks          ← mandatory (what accelerators compute)
Module 2: Deep Learning Frameworks ← mandatory (micrograd, PyTorch, tinygrad)
        ↓                    ↓
   Track A                Track B
   Hardware &             Agentic AI &
   Edge AI                ML Engineering
        ↓                    ↓
   Phase 4               Phase 4 or
   (FPGA/Jetson/          Phase 5
    Compiler)             (HPC/GenAI)
```

---

## 核心模块（必修）

| # | 模块 | 你将学到什么 | 为什么对硬件重要 |
|---|--------|---------------|---------------------------|
| **1** | [神经网络](01-神经网络/Guide.md) | 多层感知机、卷积神经网络、训练、反向传播、损失函数 | 加速器计算什么 —— 张量、矩阵乘、激活值 |
| **2** | [深度学习框架](02-深度学习框架/Guide.md) | [micrograd](02-深度学习框架/02-micrograd/Guide.md) → [PyTorch](02-深度学习框架/01-PyTorch/Guide.md) → [tinygrad](02-深度学习框架/03-tinygrad/Guide.md)：autograd、算子、编译器流水线 | 软件如何产生工作负载 —— 模型与硬件之间的接口 |

---

## 方向 A —— 硬件与边缘 AI

> *面向走向阶段 4（FPGA、Jetson、ML 编译器）与阶段 5（自动驾驶汽车、AI 芯片设计）的工程师。*

本方向讲授驱动边缘推理硬件的**感知与部署工作负载**。

| # | 模块 | 你将学到什么 | 通向 |
|---|--------|---------------|----------|
| **3** | [计算机视觉](03-硬件与边缘AI/01-计算机视觉/Guide.md) | 图像处理、检测、分割、3D 视觉、OpenCV | 阶段 4A（FPGA 视觉）、阶段 5E（自动驾驶感知） |
| **4** | [传感器融合](03-硬件与边缘AI/02-传感器融合/Guide.md) | 相机/激光雷达/IMU、卡尔曼滤波、BEVFusion、MOT | 阶段 4B（Jetson + ROS2）、阶段 5E（自动驾驶） |
| **5** | [语音 AI](03-硬件与边缘AI/03-语音AI/Guide.md) | STT（Whisper）、TTS（VITS/Piper）、VAD、关键词识别、噪声抑制 | 阶段 4A（FPGA 音频 DSP）、阶段 4B（Jetson 语音流水线） |
| **6** | [边缘 AI 与模型优化](03-硬件与边缘AI/04-边缘AI与模型优化/Guide.md) | 量化、剪枝、知识蒸馏、部署流水线 | 阶段 4（通往所有硬件方向的桥梁） |

**构建：** OpenCV 检测、传感器校准、INT8 量化、tinygrad 端侧推理、Jetson 上的 Whisper、边缘语音流水线（VAD→STT→TTS）。

---


<details>
<summary>English original</summary>

**Phase 3: Artificial Intelligence — The Workloads Your Hardware Must Run**

<div class="course-identity ai-workloads" markdown="1">
<div class="course-identity__icon">AI</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 3 · AI Workloads</p>
<p class="course-identity__title">Learn the models, frameworks, and application patterns your hardware must actually run.</p>
<p class="course-identity__meta">Artifact: workload profile · Measure: compute, memory, precision, latency</p>
</div>
</div>


> *Before you design hardware, you must deeply understand the software it accelerates.*

**Layer mapping:** **L1** (Application & Framework) — this entire phase teaches you what AI chips compute.

**Role targets:** ML Inference Engineer · Edge AI Engineer · Agentic AI Engineer · ML Engineer · AI Compiler Engineer

**Prerequisites:** Phase 1 (Digital Foundations), Phase 2 (Embedded Systems).

**What comes after:** Phase 4 Track A (FPGA), Track B (Jetson), Track C (ML Compiler).

---

**Why This Phase Exists**

Every decision in the 8-layer stack is driven by workload requirements. If you skip this phase, you'll design hardware without knowing what it needs to run. Phase 3 gives you the workload intuition that informs every hardware decision downstream.

---

**Structure: Core + Two Tracks**

**Modules 1–2** are mandatory for everyone. Then you choose **Track A**, **Track B**, or both.

```
Module 1: Neural Networks          ← mandatory (what accelerators compute)
Module 2: Deep Learning Frameworks ← mandatory (micrograd, PyTorch, tinygrad)
        ↓                    ↓
   Track A                Track B
   Hardware &             Agentic AI &
   Edge AI                ML Engineering
        ↓                    ↓
   Phase 4               Phase 4 or
   (FPGA/Jetson/          Phase 5
    Compiler)             (HPC/GenAI)
```

---

**Core Modules (Mandatory)**

| # | Module | What you learn | Why it matters for hardware |
|---|--------|---------------|---------------------------|
| **1** | [Neural Networks](01-神经网络/Guide.md) | MLPs, CNNs, training, backpropagation, loss functions | What accelerators compute — tensors, matmul, activations |
| **2** | [Deep Learning Frameworks](02-深度学习框架/Guide.md) | [micrograd](02-深度学习框架/02-micrograd/Guide.md) → [PyTorch](02-深度学习框架/01-PyTorch/Guide.md) → [tinygrad](02-深度学习框架/03-tinygrad/Guide.md): autograd, ops, compiler pipeline | How software generates workloads — the interface between models and hardware |

---

**Track A — Hardware & Edge AI**

> *For engineers heading to Phase 4 (FPGA, Jetson, ML Compiler) and Phase 5 (Autonomous Vehicles, AI Chip Design).*

This track teaches the **perception and deployment workloads** that drive edge inference hardware.

| # | Module | What you learn | Leads to |
|---|--------|---------------|----------|
| **3** | [Computer Vision](03-硬件与边缘AI/01-计算机视觉/Guide.md) | Image processing, detection, segmentation, 3D vision, OpenCV | Phase 4A (FPGA vision), Phase 5E (AV perception) |
| **4** | [Sensor Fusion](03-硬件与边缘AI/02-传感器融合/Guide.md) | Camera/LiDAR/IMU, Kalman filtering, BEVFusion, MOT | Phase 4B (Jetson + ROS2), Phase 5E (AV) |
| **5** | [Voice AI](03-硬件与边缘AI/03-语音AI/Guide.md) | STT (Whisper), TTS (VITS/Piper), VAD, keyword spotting, noise suppression | Phase 4A (FPGA audio DSP), Phase 4B (Jetson voice pipeline) |
| **6** | [Edge AI & Model Optimization](03-硬件与边缘AI/04-边缘AI与模型优化/Guide.md) | Quantization, pruning, knowledge distillation, deployment pipeline | Phase 4 (bridge to all hardware tracks) |

**Build:** OpenCV detection, sensor calibration, INT8 quantization, tinygrad on-device inference, Whisper on Jetson, edge voice pipeline (VAD→STT→TTS).

---

</details>

## 方向 B — 智能体化 AI 与 ML 工程

> *面向进入阶段 5（HPC、GPU 基础设施）的工程师，或是构建 AI 应用、由这些应用产生你的硬件所服务的推理需求的工程师。*

本方向讲的是**构建在现代推理系统之上的 AI 应用、基础设施与安全工作负载**——芯片市场的需求侧。

| # | 模块 | 你会学到什么 | 通向何处 |
|---|--------|---------------|----------|
| **3** | [Agentic AI 与 GenAI](04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) | LLM agent、RAG 流水线、工具调用、多步推理、提示注入基础、编码 agent 与 GenAI 产品 | 阶段 5A/B（GPU 基础设施、HPC） |
| **4** | [ML 工程与 MLOps](04-智能体AI与ML工程/02-ML工程与MLOps/Guide.md) | 训练流水线、实验追踪、模型推理服务、可观测性、面向模型的 CI/CD | 阶段 5A/B（HPC、分布式训练） |
| **5** | [LLM 应用开发](04-智能体AI与ML工程/03-LLM应用开发/Guide.md) | 提示工程、[Qwen3.5-4B Unsloth 微调](04-智能体AI与ML工程/03-LLM应用开发/01-Qwen3-5-4B-Unsloth微调/Guide.md)、RAG 架构、评估、护栏、审核、PII 控制、生产部署 | L1d/L1e 岗位（岗位数量最多） |

**动手做：** 带向量检索的 RAG 流水线、具备工具调用的 agent、终端或 CI 中的编码 agent、用 Unsloth 微调 `Qwen/Qwen3.5-4B-Base`、把模型部署在 Triton/vLLM 之后、带审核与脱敏的安全提示网关。

---

## 相关结构：方向 B 中的安全 LLM 系统

在方向 B 里，安全不是附带话题，而是应用架构的一部分。

- **模块 3B** 讲威胁模型：提示注入、工具滥用、文档攻击，以及不安全的检索上下文。
- **模块 4B** 讲运行模型：策略网关、日志、链路追踪、发布控制，以及生产监控。
- **模块 5B** 讲执行模型：内容审核、越狱检测、禁答话题、PII 脱敏、输出校验，以及回退行为。

采用纵深防御流水线：

```text
User input
  -> Unicode normalization / sanitization
  -> blocklists or denied-topic checks
  -> moderation and jailbreak / prompt-injection classifiers
  -> model with hardened system prompt and tool constraints
  -> output moderation, validation, and PII redaction
  -> logging, review, and threshold tuning
```

具体策略取决于产品。有些系统拦截有害或违法请求，另一些还会拦截应用不允许的话题，例如政治劝服、受监管的咨询建议，或涉及竞品的敏感内容。关键在于让策略的执行显式且可度量。

---

## 为什么分两个方向？

| | 方向 A（硬件与边缘 AI） | 方向 B（智能体化 AI 与 ML 工程） |
|---|---|---|
| **目标** | 理解跑在你芯片上的工作负载 | 理解产生推理需求的工作负载 |
| **关注点** | 感知、传感器、优化、部署 | LLM、agent、训练流水线、推理服务 |
| **硬件关联** | 直接——你部署在 FPGA/Jetson/NPU 上 | 间接——你产生的是芯片所承载的流量 |
| **就业市场** | L1a、L1b、L1c 岗位（约 4,500/月） | L1d、L1e 岗位（约 15,000/月） |
| **远程比例** | 10–15%（需要接触硬件） | 20–25%（基于云端/API） |
| **阶段 4 路径** | 方向 A → B → C（全是硬件） | 方向 C（编译器），或直接进入阶段 5 |

**两个都做？** 如果时间允许，方向 A → 方向 B 能覆盖完整的 L1。多数偏硬件的工程师先做方向 A，再按需补上方向 B 的内容。

---

## 本阶段如何与整个栈衔接

| 你学到的东西 | 它如何影响硬件设计 |
|---------------|-------------------------------|
| 神经网络中的矩阵乘 | L5：脉动阵列尺寸、dataflow 策略 |
| Conv2D、attention、pooling 算子 | L2：编译器必须融合与分块的对象 |
| 量化（INT8、FP8） | L6：PE 设计中的精度支持 |
| LLM 推理（KV-cache、批处理） | L5：存储层次、HBM 带宽需求 |
| 模型计算图 | L2：图 IR 表示、融合机会 |
| 大规模训练（分布式） | L3：NCCL、多 GPU runtime |
| 审核、护栏与提示防护 | L3/L4：推理服务拓扑、延迟预算、sidecar 服务与策略网关 |

---


<details>
<summary>English original</summary>

**Track B — Agentic AI & ML Engineering**

> *For engineers heading to Phase 5 (HPC, GPU Infrastructure) or building AI applications that generate the inference demand your hardware serves.*

This track teaches the **AI application, infrastructure, and safety workloads** that sit on top of modern inference systems — the demand side of the chip market.

| # | Module | What you learn | Leads to |
|---|--------|---------------|----------|
| **3** | [Agentic AI & GenAI](04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) | LLM agents, RAG pipelines, tool use, multi-step reasoning, prompt-injection basics, coding agents, and GenAI products | Phase 5A/B (GPU Infrastructure, HPC) |
| **4** | [ML Engineering & MLOps](04-智能体AI与ML工程/02-ML工程与MLOps/Guide.md) | Training pipelines, experiment tracking, model serving, observability, CI/CD for models | Phase 5A/B (HPC, distributed training) |
| **5** | [LLM Application Development](04-智能体AI与ML工程/03-LLM应用开发/Guide.md) | Prompt engineering, [Qwen3.5-4B Unsloth fine-tuning](04-智能体AI与ML工程/03-LLM应用开发/01-Qwen3-5-4B-Unsloth微调/Guide.md), RAG architecture, evaluation, guardrails, moderation, PII controls, production deployment | L1d/L1e roles (highest job volume) |

**Build:** RAG pipeline with vector search, agent with tool calling, coding agent in terminal or CI, fine-tune `Qwen/Qwen3.5-4B-Base` with Unsloth, deploy model behind Triton/vLLM, secure prompt gateway with moderation and redaction.

---

**Relevant Structure: Safe LLM Systems in Track B**

For Track B, safety is not a side note. It is part of the application architecture.

- **Module 3B** teaches the threat model: prompt injection, tool misuse, document attacks, and unsafe retrieval context.
- **Module 4B** teaches the operating model: policy gateways, logs, tracing, rollout controls, and production monitoring.
- **Module 5B** teaches the enforcement model: content moderation, jailbreak detection, denied topics, PII redaction, output validation, and fallback behavior.

Use a defense-in-depth pipeline:

```text
User input
  -> Unicode normalization / sanitization
  -> blocklists or denied-topic checks
  -> moderation and jailbreak / prompt-injection classifiers
  -> model with hardened system prompt and tool constraints
  -> output moderation, validation, and PII redaction
  -> logging, review, and threshold tuning
```

The exact policy depends on the product. Some systems block harmful or illegal requests. Others also block application-disallowed topics such as political persuasion, regulated advice, or competitor-sensitive content. The important point is to make policy enforcement explicit and measurable.

---

**Why Two Tracks?**

| | Track A (Hardware & Edge AI) | Track B (Agentic AI & ML Eng) |
|---|---|---|
| **Goal** | Understand workloads that run on your chip | Understand workloads that create inference demand |
| **Focus** | Perception, sensors, optimization, deployment | LLMs, agents, training pipelines, serving |
| **Hardware connection** | Direct — you deploy on FPGA/Jetson/NPU | Indirect — you generate the traffic the chip serves |
| **Job market** | L1a, L1b, L1c roles (~4,500/month) | L1d, L1e roles (~15,000/month) |
| **Remote %** | 10–15% (hardware access needed) | 20–25% (cloud/API-based) |
| **Phase 4 path** | Track A → B → C (all hardware) | Track C (compiler) or Phase 5 directly |

**Do both?** If you have time, Track A → Track B gives you full L1 coverage. Most hardware-focused engineers do Track A first, then add Track B topics as needed.

---

**How This Phase Connects to the Stack**

| What you learn | How it informs hardware design |
|---------------|-------------------------------|
| Matrix multiply in neural networks | L5: systolic array dimensions, dataflow strategy |
| Conv2D, attention, pooling ops | L2: what the compiler must fuse and tile |
| Quantization (INT8, FP8) | L6: precision support in PE design |
| LLM inference (KV-cache, batching) | L5: memory hierarchy, HBM bandwidth requirements |
| Model computational graphs | L2: graph IR representation, fusion opportunities |
| Training at scale (distributed) | L3: NCCL, multi-GPU runtime |
| Moderation, guardrails, and prompt shielding | L3/L4: serving topology, latency budget, sidecar services, and policy gateways |

---

</details>

## 应产出的内容

到本阶段结束时，你至少应当有：

- 核心模块中的一份模型实现或工作负载直觉产物
- 来自方向 A 或方向 B 的一份面向部署或优化的产物
- 一份把工作负载行为与内存、延迟、吞吐或精度等硬件约束联系起来的简短总结

示例包括量化对比、性能剖析笔记、tinygrad trace、RAG 延迟分析或传感器流水线 benchmark。

---

## 达成标准

当你能做到以下各点时，就准备好进入阶段 4：

- 解释目标工作负载实际计算了什么
- 判断一个工作负载究竟受限于计算、内存、批处理、上下文长度还是部署约束
- 把模型结构与编译器、runtime 或硬件层面的影响联系起来
- 拿出至少一份产物，证明你不只是读过关于该工作负载的资料

---

## 补充资源

- [CMU AI Courses Reference](CMU-AI-Courses.md)
- [Maxime Labonne 的 LLM Course](https://github.com/mlabonne/llm-course) — 免费的 LLM 路线图，附带 Colab notebook，覆盖 LLM 基础、微调、量化、RAG、评估与部署。可作为方向 B 的动手配套材料。
- [LLM Visualization (bbycroft.net)](https://bbycroft.net/llm) — 交互式 3D 漫游 GPT 式模型：每个张量、每个矩阵乘、attention、layer norm、多层感知机、softmax、输出投影。是把「我读过 Transformer 的资料」变成「我能看清每个张量用来干什么」的最快途径。可配合 [Neural Networks → Transformer Fundamentals](01-神经网络/01-Transformer基础/Lecture-01.md) 使用。

---

## 下一步

→ [**阶段 4 方向 A — Xilinx FPGA**](../阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md) · [**阶段 4 方向 B — Jetson**](../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md) · [**阶段 4 方向 C — ML Compiler**](../阶段4-路线C-深度学习推理优化/Guide.md)


<details>
<summary>English original</summary>

**What You Should Produce**

By the end of this phase, you should have at least:

- one model-implementation or workload-intuition artifact from the core modules
- one deployment- or optimization-oriented artifact from Track A or Track B
- one short write-up connecting workload behavior to hardware constraints such as memory, latency, throughput, or precision

Examples include a quantization comparison, a profiling note, a tinygrad trace, a RAG latency analysis, or a sensor-pipeline benchmark.

---

**Exit Criteria**

You are ready for Phase 4 when you can:

- explain what your target workloads actually compute
- identify whether a workload is dominated by compute, memory, batching, context length, or deployment constraints
- connect model structure to compiler, runtime, or hardware implications
- show at least one artifact that proves you did more than read about the workload

---

**Additional Resources**

- [CMU AI Courses Reference](CMU-AI-Courses.md)
- [Maxime Labonne's LLM Course](https://github.com/mlabonne/llm-course) — free LLM roadmap with Colab notebooks covering LLM fundamentals, fine-tuning, quantization, RAG, evaluation, and deployment. Use it as a hands-on companion for Track B.
- [LLM Visualization (bbycroft.net)](https://bbycroft.net/llm) — interactive 3D walk through a GPT-style model: every tensor, every matmul, attention, layer norm, MLP, softmax, output projection. The fastest way to convert "I read about transformers" into "I can see what each tensor is for." Pair with [Neural Networks → Transformer Fundamentals](01-神经网络/01-Transformer基础/Lecture-01.md).

---

**Next**

→ [**Phase 4 Track A — Xilinx FPGA**](../阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md) · [**Phase 4 Track B — Jetson**](../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md) · [**Phase 4 Track C — ML Compiler**](../阶段4-路线C-深度学习推理优化/Guide.md)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
