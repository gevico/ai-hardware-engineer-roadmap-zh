---
title: 模块 4B — ML 工程与 MLOps
description: 模块 4B — ML 工程与 MLOps
---

# 模块 4B — ML 工程与 MLOps

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">M4ME</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · AI 工作负载</p>
<p class="course-identity__title">模块 4B 的专用课程标识 — ML 工程与 MLOps。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


**父级：** [阶段 3 — 人工智能](../../Guide.md) · 方向 B

> *构建并运营 ML 训练/推理流水线 — 模型生命周期、数据流水线、推理服务基础设施。*

**前置要求：** 模块 2（框架 — 熟练使用 PyTorch），模块 3B（智能体化 AI — 理解大语言模型工作负载）。

**目标岗位：** 机器学习工程师 · MLOps 工程师 · AI/ML 工程师 · ML 平台工程师

---

> **★ 精选深度剖析课程：** [**Practical Machine Learning (CS329P)**](01-实用机器学习CS329P/README.md) — 实践者实际会遇到的完整 ML 生命周期，改编自 Stanford CS329P（Mu Li & Alex Smola），并为 2026 更新：数据采集/标注、不会说谎的验证、分布偏移检测、调优，以及面向硬件的 [模型压缩讲座](01-实用机器学习CS329P/Lecture-10.md)。11 讲。从 [概述](01-实用机器学习CS329P/README.md) 开始。

---

## 为什么这对 AI 硬件很重要

ML 工程师定义硬件必须支持的 **训练与推理服务工作负载**：
- 分布式训练（数据/模型/流水线并行）→ L3：NCCL、多 GPU runtime
- 模型推理服务（延迟 SLA、吞吐目标）→ L1a：TensorRT、Triton、vLLM
- 实验跟踪与模型版本管理 → 使用 GPU 簇的基础设施（阶段 5A）
- 数据流水线 → 驱动 GPUDirect Storage 的 I/O 带宽需求（阶段 5B）

---

## 1. 训练流水线

* **数据流水线：** PyTorch DataLoader、NVIDIA DALI、流式数据集
* **分布式训练：** DDP（Data Distributed Parallel）、FSDP（Fully Sharded）、DeepSpeed
* **混合精度：** `torch.cuda.amp`、BF16、FP8 训练
* **检查点保存：** 模型保存、从检查点恢复、弹性训练
* **超参数调优：** Optuna、Ray Tune、网格/随机/贝叶斯搜索

**项目：**
1. 用 DDP 在 2+ GPU 上训练模型。测量扩展效率。
2. 加入混合精度训练。比较 FP32 与 BF16 的训练速度和准确率。

---

## 2. 实验跟踪与模型注册表

* **实验跟踪：** MLflow、Weights & Biases（W&B）、Neptune
* **模型注册表：** 版本管理、预发、生产晋级
* **数据集版本管理：** DVC、LakeFS
* **可复现性：** 环境跟踪、种子管理、确定性的训练

**项目：**
1. 为一次训练运行设置 MLflow 跟踪。记录超参数、指标和产物。
2. 在 MLflow 中注册模型。创建预发 → 生产晋级工作流。

---

## 3. 模型推理服务与推理基础设施

* **推理服务框架：** Triton Inference Server、TorchServe、vLLM、TensorRT-LLM
* **批处理：** 静态批处理、动态批处理、连续批处理（用于大语言模型）
* **扩展：** 水平扩展（副本）、垂直扩展（更大 GPU）、针对大模型的模型并行
* **监控：** 延迟百分位数（p50/p99）、吞吐（QPS）、错误率、GPU 利用率
* **策略网关：** 请求过滤、审核 sidecar、认证、速率限制、安全 trace
* **A/B 测试：** 金丝雀部署、影子模式、流量拆分

**项目：**
1. 在 Triton 上部署模型并启用动态批处理。进行负载测试并测量 p50/p99 延迟。
2. 在 vLLM 上部署大语言模型。比较不同批大小和量化级别的吞吐。
3. 在推理服务栈前插入审核或策略网关，并测量延迟取舍。

---

## 4. 面向模型的 MLOps 与 CI/CD

* **CI/CD：** 用于模型训练、测试、部署的 GitHub Actions / GitLab CI
* **容器编排：** Docker、Kubernetes、NVIDIA GPU Operator
* **模型测试：** 预处理的单元测试、推理的集成测试、数据漂移检测
* **安全回归测试：** jailbreak 套件、屏蔽主题测试、提示攻击回放、策略漂移检查
* **特征存储：** Feast、Tecton — 管理特征，以保证训练与推理服务的一致性
* **编排：** Kubeflow Pipelines、Apache Airflow、Prefect

**项目：**
1. 构建 CI/CD 流水线：push → 训练 → 评测 → 若改进 → 部署到 Triton。
2. 使用 NVIDIA GPU Operator 搭建支持 GPU 的 Kubernetes。部署模型推理服务工作负载。

---


<details>
<summary>English original</summary>

**Module 4B — ML Engineering & MLOps**

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">M4ME</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for Module 4B — ML Engineering & MLOps.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


**Parent:** [Phase 3 — Artificial Intelligence](../../Guide.md) · Track B

> *Build and operate ML training/inference pipelines — model lifecycle, data pipelines, serving infrastructure.*

**Prerequisites:** Module 2 (Frameworks — PyTorch fluency), Module 3B (Agentic AI — understand LLM workloads).

**Role targets:** Machine Learning Engineer · MLOps Engineer · AI/ML Engineer · ML Platform Engineer

---

> **★ Featured deep-dive course:** [**Practical Machine Learning (CS329P)**](01-实用机器学习CS329P/README.md) — the full ML lifecycle a practitioner actually meets, adapted from Stanford CS329P (Mu Li & Alex Smola) and refreshed for 2026: data acquisition/labeling, validation that doesn't lie, distribution-shift detection, tuning, and a hardware-facing [model-compression lecture](01-实用机器学习CS329P/Lecture-10.md). 11 lectures. Start with the [Overview](01-实用机器学习CS329P/README.md).

---

**Why This Matters for AI Hardware**

ML engineers define the **training and serving workloads** that hardware must support:
- Distributed training (data/model/pipeline parallelism) → L3: NCCL, multi-GPU runtime
- Model serving (latency SLAs, throughput targets) → L1a: TensorRT, Triton, vLLM
- Experiment tracking and model versioning → infrastructure that uses GPU clusters (Phase 5A)
- Data pipelines → I/O bandwidth requirements that drive GPUDirect Storage (Phase 5B)

---

**1. Training Pipelines**

* **Data pipelines:** PyTorch DataLoader, NVIDIA DALI, streaming datasets
* **Distributed training:** DDP (Data Distributed Parallel), FSDP (Fully Sharded), DeepSpeed
* **Mixed precision:** `torch.cuda.amp`, BF16, FP8 training
* **Checkpointing:** model saving, resume from checkpoint, elastic training
* **Hyperparameter tuning:** Optuna, Ray Tune, grid/random/Bayesian search

**Projects:**
1. Train a model with DDP across 2+ GPUs. Measure scaling efficiency.
2. Add mixed precision training. Compare FP32 vs BF16 training speed and accuracy.

---

**2. Experiment Tracking & Model Registry**

* **Experiment tracking:** MLflow, Weights & Biases (W&B), Neptune
* **Model registry:** versioning, staging, production promotion
* **Dataset versioning:** DVC, LakeFS
* **Reproducibility:** environment tracking, seed management, deterministic training

**Projects:**
1. Set up MLflow tracking for a training run. Log hyperparameters, metrics, and artifacts.
2. Register a model in MLflow. Create a staging → production promotion workflow.

---

**3. Model Serving & Inference Infrastructure**

* **Serving frameworks:** Triton Inference Server, TorchServe, vLLM, TensorRT-LLM
* **Batching:** static batching, dynamic batching, continuous batching (for LLMs)
* **Scaling:** horizontal (replicas), vertical (larger GPU), model parallelism for large models
* **Monitoring:** latency percentiles (p50/p99), throughput (QPS), error rates, GPU utilization
* **Policy gateways:** request filtering, moderation sidecars, auth, rate limits, safety traces
* **A/B testing:** canary deployments, shadow mode, traffic splitting

**Projects:**
1. Deploy a model on Triton with dynamic batching. Load test and measure p50/p99 latency.
2. Deploy an LLM on vLLM. Compare throughput with different batch sizes and quantization levels.
3. Insert a moderation or policy gateway in front of a serving stack and measure the latency tradeoff.

---

**4. MLOps & CI/CD for Models**

* **CI/CD:** GitHub Actions / GitLab CI for model training, testing, deployment
* **Container orchestration:** Docker, Kubernetes, NVIDIA GPU Operator
* **Model testing:** unit tests for preprocessing, integration tests for inference, data drift detection
* **Safety regression testing:** jailbreak suites, blocked-topic tests, prompt-attack replay, policy drift checks
* **Feature stores:** Feast, Tecton — manage features for training and serving consistency
* **Orchestration:** Kubeflow Pipelines, Apache Airflow, Prefect

**Projects:**
1. Build a CI/CD pipeline: on push → train → evaluate → if improved → deploy to Triton.
2. Set up GPU-enabled Kubernetes with NVIDIA GPU Operator. Deploy a model serving workload.

---

</details>

## 资源

| 资源 | 涵盖内容 |
|----------|---------------|
| [MLflow Documentation](https://mlflow.org/docs/latest/) | 实验跟踪、模型注册表 |
| [Triton Inference Server](https://github.com/triton-inference-server/server) | 生产环境模型推理服务 |
| [vLLM](https://github.com/vllm-project/vllm) | 大语言模型推理服务引擎 |
| [DeepSpeed](https://github.com/microsoft/DeepSpeed) | 分布式训练 |
| *Designing Machine Learning Systems* (Huyen) | 机器学习系统设计 |

---

## 下一步

→ [**Module 5B — LLM Application Development**](../03-LLM应用开发/Guide.md)


<details>
<summary>English original</summary>

**Resources**

| Resource | What it covers |
|----------|---------------|
| [MLflow Documentation](https://mlflow.org/docs/latest/) | Experiment tracking, model registry |
| [Triton Inference Server](https://github.com/triton-inference-server/server) | Production model serving |
| [vLLM](https://github.com/vllm-project/vllm) | LLM serving engine |
| [DeepSpeed](https://github.com/microsoft/DeepSpeed) | Distributed training |
| *Designing Machine Learning Systems* (Huyen) | ML systems design |

---

**Next**

→ [**Module 5B — LLM Application Development**](../03-LLM应用开发/Guide.md)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
