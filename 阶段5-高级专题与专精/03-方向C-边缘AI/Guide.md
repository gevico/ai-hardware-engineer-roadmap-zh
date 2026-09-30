---
title: 阶段 5 — 方向 C：边缘 AI
description: 阶段 5 — 方向 C：边缘 AI
---

# 阶段 5 — 方向 C：边缘 AI

<div class="course-identity edge-ai" markdown="1">
<div class="course-identity__icon">EDGE</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 5C · 边缘 AI</p>
<p class="course-identity__title">在真实设备约束下部署 AI：传感器、延迟、内存、功耗、热管理与更新。</p>
<p class="course-identity__meta">产物：边缘 AI 案例研究 · 度量：延迟、功耗、内存、可靠性</p>
</div>
</div>


> 在资源受限的硬件上设计、优化、部署并运维 AI 系统，此时延迟、内存、功耗、热管理、传感器与可靠性都至关重要。

**层级映射：** L1-L5。本方向串联边缘工作负载、模型优化、推理 runtime、嵌入式 Linux、传感器流水线、加速器与产品部署。

**目标岗位：** Edge AI 工程师 · 嵌入式 AI 工程师 · Jetson 工程师 · TinyML 工程师 · 机器人感知工程师 · 边缘推理优化工程师

**前置要求：** [阶段 2 — 嵌入式系统](../../阶段2-嵌入式系统/Guide.md)、[阶段 3 — AI 工作负载](../../阶段3-人工智能/Guide.md)，以及最好具备 [阶段 4 方向 B — NVIDIA Jetson](../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md)。

**后续方向：** [ML 系统工程](../07-方向G-ML系统工程/Guide.md)、[机器人学](../04-方向D-机器人/Guide.md)、[自动驾驶](../05-方向E-自动驾驶/Guide.md)，或 [AI 芯片设计](../06-方向F-AI芯片设计/Guide.md)。

---

## 本方向为何存在

边缘 AI 并非“把云端推理塞进更小的盒子”。这套系统有着不同的约束：

- 内存受限
- 功耗受限
- 热降频
- 传感器时序
- 摄像头/音频预处理
- 本地可靠性要求
- 间歇性网络接入
- 更新与回滚风险
- 硬件专用 runtime 与加速器

核心工程问题是：

```text
Can this model meet the product requirement on this device under real power,
thermal, memory, sensor, and latency constraints?
```

本方向教你用测量数据来回答这个问题。

---

## 课程成果

学完后，你应当能够：

- 为目标设备选择边缘模型架构
- 不靠猜测地压缩与量化模型
- 通过 TensorRT、ONNX Runtime、LiteRT/TFLite、TFLM 或厂商 runtime 部署
- 剖析延迟、内存、吞吐、功耗与热管理
- 构建摄像头、音频与传感器到模型的流水线
- 推理 CPU/GPU/DLA（深度学习加速器）/NPU/DSP 的分区
- 设计 OTA、回滚、遥测与设备群更新流程
- 说明边缘推理何时应留在本地、何时应卸载

---

## 课程地图

<div class="lecture-map" markdown>

| 单元 | 重点 | 产物 |
|------|-------|----------|
| 1 | 边缘约束与平台选型 | 目标设备决策备忘 |
| 2 | 高效模型架构 | 模型对比 benchmark |
| 3 | 压缩与量化 | PTQ/QAT/精度取舍报告 |
| 4 | runtime 部署 | 可复现的 runtime 部署 |
| 5 | TinyML 与 MCU 推理 | 受限内存推理演示 |
| 6 | 传感器流水线 | 摄像头/音频/传感器流水线 benchmark |
| 7 | 功耗、热管理与可靠性 | 长时间稳定性报告 |
| 8 | 设备群与产品运营 | OTA/遥测/回滚设计 |

</div>

---

## 单元 1：边缘约束与平台选型

### 学习

- MCU、MPU、边缘 GPU 与专用加速器的对比
- 延迟、吞吐、内存、功耗、热管理、成本与外壳约束
- TOPS 与有效吞吐的对比
- 模型大小与激活值/KV-cache/runtime 内存的对比
- 传感器带宽与预处理开销
- 纯本地与边缘/云混合部署的对比

### 动手实现

挑选一款目标产品：

- 唤醒词音箱
- 智能摄像头
- 机器人感知模块
- 工业异常检测器
- 本地大语言模型设备
- 电池供电的野生动物相机

编写一份目标设备决策备忘，对至少三个平台进行对比：

- Jetson Orin Nano/NX/AGX
- Raspberry Pi + 加速器
- Coral Edge TPU
- Hailo
- Qualcomm/Android 设备
- STM32/nRF/ESP32 级 MCU

### 度量

- 内存预算
- 延迟目标
- 功耗预算
- 传感器带宽
- 模型大小
- 预期更新节奏

### 交付

一份平台选型备忘，说明为何某款设备比其它方案更契合该工作负载。

---

## 单元 2：高效模型架构

### 学习

- MobileNet、EfficientNet、EfficientDet、EfficientViT、MobileViT、FastViT
- YOLO 系列与实时检测取舍
- 轻量 Transformer
- 关键词识别模型
- 微型分割与姿态模型
- 蒸馏大语言模型与基于适配器的本地模型
- 硬件感知的神经网络架构搜索


<details>
<summary>English original</summary>

**Phase 5 — Track C: Edge AI**

<div class="course-identity edge-ai" markdown="1">
<div class="course-identity__icon">EDGE</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track 5C · Edge AI</p>
<p class="course-identity__title">Deploy AI under real device constraints: sensors, latency, memory, power, thermals, and updates.</p>
<p class="course-identity__meta">Artifact: edge AI case study · Measure: latency, power, memory, reliability</p>
</div>
</div>


> Design, optimize, deploy, and operate AI systems on constrained hardware where latency, memory, power, thermals, sensors, and reliability matter.

**Layer mapping:** L1-L5. This track connects edge workloads, model optimization, inference runtimes, embedded Linux, sensor pipelines, accelerators, and product deployment.

**Role targets:** Edge AI Engineer · Embedded AI Engineer · Jetson Engineer · TinyML Engineer · Robotics Perception Engineer · Edge Inference Optimization Engineer

**Prerequisites:** [Phase 2 — Embedded Systems](../../阶段2-嵌入式系统/Guide.md), [Phase 3 — AI Workloads](../../阶段3-人工智能/Guide.md), and preferably [Phase 4 Track B — NVIDIA Jetson](../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md).

**What comes after:** [ML Systems Engineering](../07-方向G-ML系统工程/Guide.md), [Robotics](../04-方向D-机器人/Guide.md), [Autonomous Vehicles](../05-方向E-自动驾驶/Guide.md), or [AI Chip Design](../06-方向F-AI芯片设计/Guide.md).

---

**Why This Track Exists**

Edge AI is not "cloud inference on a smaller box." The system has different constraints:

- limited memory
- limited power
- thermal throttling
- sensor timing
- camera/audio preprocessing
- local reliability requirements
- intermittent network access
- update and rollback risk
- hardware-specific runtimes and accelerators

The core engineering question is:

```text
Can this model meet the product requirement on this device under real power,
thermal, memory, sensor, and latency constraints?
```

This track teaches you to answer that with measurements.

---

**Course Outcomes**

By the end, you should be able to:

- choose an edge model architecture for a target device
- compress and quantize models without guessing
- deploy through TensorRT, ONNX Runtime, LiteRT/TFLite, TFLM, or a vendor runtime
- profile latency, memory, throughput, power, and thermals
- build camera, audio, and sensor-to-model pipelines
- reason about CPU/GPU/DLA/NPU/DSP partitioning
- design OTA, rollback, telemetry, and fleet update flows
- explain when edge inference should stay local and when it should offload

---

**Course Map**

<div class="lecture-map" markdown>

| Unit | Focus | Artifact |
|------|-------|----------|
| 1 | Edge constraints and platform selection | target-device decision memo |
| 2 | Efficient model architectures | model comparison benchmark |
| 3 | Compression and quantization | PTQ/QAT/precision tradeoff report |
| 4 | Runtime deployment | reproducible runtime deployment |
| 5 | TinyML and MCU inference | constrained-memory inference demo |
| 6 | Sensor pipelines | camera/audio/sensor pipeline benchmark |
| 7 | Power, thermal, and reliability | long-run stability report |
| 8 | Fleet and product operation | OTA/telemetry/rollback design |

</div>

---

**Unit 1: Edge Constraints And Platform Selection**

**Learn**

- MCU versus MPU versus edge GPU versus dedicated accelerator
- latency, throughput, memory, power, thermals, cost, and enclosure constraints
- TOPS versus useful throughput
- model size versus activation/KV-cache/runtime memory
- sensor bandwidth and preprocessing cost
- local-only versus edge/cloud hybrid deployment

**Build It**

Pick one target product:

- wake-word speaker
- smart camera
- robot perception module
- industrial anomaly detector
- local LLM appliance
- battery-powered wildlife camera

Create a target-device decision memo comparing at least three platforms:

- Jetson Orin Nano/NX/AGX
- Raspberry Pi + accelerator
- Coral Edge TPU
- Hailo
- Qualcomm/Android device
- STM32/nRF/ESP32-class MCU

**Measure It**

- memory budget
- latency target
- power budget
- sensor bandwidth
- model size
- expected update cadence

**Ship It**

A platform-selection memo that explains why one device fits the workload better than the alternatives.

---

**Unit 2: Efficient Model Architectures**

**Learn**

- MobileNet, EfficientNet, EfficientDet, EfficientViT, MobileViT, FastViT
- YOLO family and real-time detection tradeoffs
- lightweight transformers
- keyword spotting models
- tiny segmentation and pose models
- distilled LLMs and adapter-based local models
- hardware-aware neural architecture search

</details>

### 构建它

在同一任务与设备上对三个模型家族做 benchmark：

- 检测器：YOLO 变体对比 EfficientDet 风格模型
- 分类器：MobileNetV3 对比 EfficientNet-Lite 对比 FastViT
- 音频：DS-CNN 对比小型 Transformer 或 Conformer
- 本地 LLM：量化小模型变体

### 度量它

- 准确率或任务指标
- 延迟
- 吞吐
- 峰值内存
- 功耗
- 模型大小
- 预处理与后处理开销

### 交付

一份模型选型报告，含准确率/延迟/功耗表和明确建议。

---

## Unit 3：压缩与量化

### 学习

- FP16、BF16、INT8、INT4、混合精度
- 训练后量化
- 量化感知训练
- 校准数据集
- 逐张量与逐通道量化
- 剪枝与结构化稀疏
- 蒸馏
- AWQ、GPTQ、SmoothQuant 及 GGUF 量化家族等 LLM 量化方法
- 准确率恢复与回归测试

### 构建它

让一个模型至少走两条压缩路径：

1. 基线精度
2. PTQ
3. QAT 或校准 INT8
4. 可选的 INT4 或 LLM 量化路径

### 度量它

- 任务准确率前后对比
- 延迟前后对比
- 内存前后对比
- 功耗前后对比
- layer 级回退到更高精度

### 交付

一份量化报告，说明改了什么、什么坏了，以及你会交付哪种精度。

---

## Unit 4：运行时部署

### 学习

- TensorRT engine 构建与校准
- ONNX 导出与图清理
- ONNX Runtime execution providers
- LiteRT/TFLite delegates
- TFLite Micro arena 分配
- Jetson 上的 TensorRT DLA 卸载
- `trtexec`、Nsight Systems 与 runtime 性能剖析
- 部署打包与版本管理

### 构建它

尽可能用两个 runtime 部署同一模型：

- PyTorch eager 基线
- ONNX Runtime
- TensorRT
- LiteRT/TFLite
- TFLite Micro
- 厂商加速器 runtime

### 度量它

- 冷启动
- 热延迟
- 吞吐
- 峰值内存
- 模型加载时间
- engine 构建时间
- CPU/GPU/DLA 利用率

### 交付

可复现的 runtime 部署，包含确切的转换命令、runtime 命令和 benchmark 输出。

---

## Unit 5：TinyML 与 MCU 推理

### 学习

- Cortex-M 级约束
- SRAM 与 flash 预算
- TFLite Micro arena 分配
- CMSIS-NN 与 DSP kernel
- 定点算术
- 占空比调度与常开传感
- MCU OTA 模型更新
- 漂移与本地适应限制

### 构建它

构建一个 MCU 规模的推理 demo：

- 关键词识别
- 基于 IMU 的手势识别
- 振动异常检测
- 低分辨率人体检测
- 环境异常检测

### 度量它

- arena 大小
- flash 大小
- 推理延迟
- 活动功耗与睡眠功耗
- 电池寿命估计
- 假阳性/假阴性表现

### 交付

一个受限内存推理 demo，含固件、模型产物以及功耗或延迟测量结果。

---

## Unit 6：传感器流水线

### 学习

- 摄像头接入：MIPI CSI-2、USB、GigE、V4L2
- ISP（图像信号处理器）流水线：RAW、去马赛克、降噪、色调映射、缩放、色彩转换
- 音频流水线：I2S、ALSA、VAD、关键词识别、ASR
- GStreamer 与 DeepStream
- 多路流推理
- 跟踪与后处理
- 零拷贝路径与缓冲区所有权

### 构建它

构建一条端到端传感器流水线：

- 摄像头 -> 预处理 -> 检测 -> 跟踪 -> 输出
- 麦克风 -> VAD -> 特征提取 -> 关键词/ASR -> 输出
- IMU -> 滤波 -> 模型 -> 异常/事件输出

### 度量它

- 传感器到输出延迟
- 预处理时间
- 推理时间
- 后处理时间
- 丢帧或音频欠载
- 内存拷贝
- CPU/GPU 利用率

### 交付

一份传感器到模型的流水线报告，含延迟细分以及至少一项零拷贝或减少拷贝的改进。

---

## Unit 7：功耗、热管理与可靠性

### 学习

- 热降频
- nvpmodel/jetson_clocks 风格功耗模式
- DVFS
- 电池预算
- 看门狗
- 模型健康检查
- 长时稳定性测试
- 离线行为与恢复

### 构建它

运行一次长时边缘推理测试：

- 固定工作负载
- 真实传感器输入或回放流
- 功耗/热管理日志
- 失败自动重启
- 基础遥测

### 度量它

- 持续延迟
- 持续吞吐
- 温度
- 降频事件
- 功耗
- 内存增长
- 崩溃或重启行为

### 交付

一份稳定性报告，说明设备能持续承受什么，而不只是跑一次 benchmark 能做到什么。

---

## Unit 8：设备群与产品运营

### 学习

- OTA 模型与软件更新
- 回滚与 A/B 槽位
- 设备遥测
- 模型/版本兼容性
- 隐私与本地数据留存
- 边缘/云端路由
- 监控漂移与现场故障
- 系统层面的安全启动与签名产物

### 构建它

为小型设备群设计部署方案：

- 模型打包
- 发布阶段
- 回滚触发条件
- 遥测 schema
- 健康检查
- 故障分诊


<details>
<summary>English original</summary>

**Build It**

Benchmark three model families on the same task and device:

- detector: YOLO variant versus EfficientDet-style model
- classifier: MobileNetV3 versus EfficientNet-Lite versus FastViT
- audio: DS-CNN versus small transformer or conformer
- local LLM: quantized small model variants

**Measure It**

- accuracy or task metric
- latency
- throughput
- peak memory
- power
- model size
- preprocessing and postprocessing cost

**Ship It**

A model-selection report with an accuracy/latency/power table and a clear recommendation.

---

**Unit 3: Compression And Quantization**

**Learn**

- FP16, BF16, INT8, INT4, mixed precision
- post-training quantization
- quantization-aware training
- calibration datasets
- per-tensor versus per-channel quantization
- pruning and structured sparsity
- distillation
- LLM quantization methods such as AWQ, GPTQ, SmoothQuant, and GGUF quant families
- accuracy recovery and regression testing

**Build It**

Take one model through at least two compression paths:

1. baseline precision
2. PTQ
3. QAT or calibrated INT8
4. optional INT4 or LLM quantization path

**Measure It**

- task accuracy before/after
- latency before/after
- memory before/after
- power before/after
- layer-level fallback to higher precision

**Ship It**

A quantization report that explains what changed, what broke, and which precision you would ship.

---

**Unit 4: Runtime Deployment**

**Learn**

- TensorRT engine building and calibration
- ONNX export and graph cleanup
- ONNX Runtime execution providers
- LiteRT/TFLite delegates
- TFLite Micro arena allocation
- TensorRT DLA offload on Jetson
- `trtexec`, Nsight Systems, and runtime profiling
- deployment packaging and versioning

**Build It**

Deploy the same model through two runtimes where possible:

- PyTorch eager baseline
- ONNX Runtime
- TensorRT
- LiteRT/TFLite
- TFLite Micro
- vendor accelerator runtime

**Measure It**

- cold start
- warm latency
- throughput
- peak memory
- model load time
- engine build time
- CPU/GPU/DLA utilization

**Ship It**

A reproducible runtime deployment with exact conversion commands, runtime commands, and benchmark output.

---

**Unit 5: TinyML And MCU Inference**

**Learn**

- Cortex-M class constraints
- SRAM and flash budgeting
- TFLite Micro arena allocation
- CMSIS-NN and DSP kernels
- fixed-point arithmetic
- duty cycling and always-on sensing
- MCU OTA model updates
- drift and local adaptation limits

**Build It**

Build one MCU-scale inference demo:

- keyword spotting
- gesture recognition from IMU
- vibration anomaly detection
- low-resolution person detection
- environmental anomaly detection

**Measure It**

- arena size
- flash size
- inference latency
- active and sleep power
- battery-life estimate
- false positive/false negative behavior

**Ship It**

A constrained-memory inference demo with firmware, model artifact, and power or latency measurements.

---

**Unit 6: Sensor Pipelines**

**Learn**

- camera ingest: MIPI CSI-2, USB, GigE, V4L2
- ISP pipeline: RAW, debayer, denoise, tone map, resize, color convert
- audio pipeline: I2S, ALSA, VAD, keyword spotting, ASR
- GStreamer and DeepStream
- multi-stream inference
- tracking and postprocessing
- zero-copy paths and buffer ownership

**Build It**

Build one end-to-end sensor pipeline:

- camera -> preprocess -> detection -> tracking -> output
- microphone -> VAD -> feature extraction -> keyword/ASR -> output
- IMU -> filtering -> model -> anomaly/event output

**Measure It**

- sensor-to-output latency
- preprocessing time
- inference time
- postprocessing time
- dropped frames or audio underruns
- memory copies
- CPU/GPU utilization

**Ship It**

A sensor-to-model pipeline report with a latency breakdown and at least one zero-copy or copy-reduction improvement.

---

**Unit 7: Power, Thermal, And Reliability**

**Learn**

- thermal throttling
- nvpmodel/jetson_clocks-style power modes
- DVFS
- battery budgeting
- watchdogs
- model health checks
- long-run stability testing
- offline behavior and recovery

**Build It**

Run a long-duration edge inference test:

- fixed workload
- realistic sensor input or replayed stream
- power/thermal logging
- automatic restart on failure
- basic telemetry

**Measure It**

- sustained latency
- sustained throughput
- temperature
- throttling events
- power draw
- memory growth
- crash or restart behavior

**Ship It**

A stability report that says what the device can sustain, not only what it can do for one benchmark run.

---

**Unit 8: Fleet And Product Operation**

**Learn**

- OTA model and software updates
- rollback and A/B slots
- device telemetry
- model/version compatibility
- privacy and local data retention
- edge/cloud routing
- monitoring drift and field failures
- secure boot and signed artifacts at a systems level

**Build It**

Design a deployment plan for a small fleet:

- model packaging
- rollout stages
- rollback trigger
- telemetry schema
- health checks
- failure triage

</details>

### 度量

- 更新时间
- 回滚时间
- 遥测数据量
- 离线恢复行为
- 版本兼容性检查

### 交付

一份边缘 AI 运维计划，另一位工程师可据此把模型安全地交付到设备上。

---

## 精选深度解析

把这些讲座作为 LLM 与无线方向边缘工作的技术核心：

- [边缘 LLM 推理内幕](03-边缘LLM推理内部机制/Lecture-01.md)
- [Qwen 推理优化](05-Qwen推理优化/README.md)
- [Gemma 4 边缘部署](04-Gemma-4边缘部署/README.md) — PDL 流水线、交错 attention KV 计算、speculative decode（逐 token 生成阶段），以及 Jetson Orin/Thor 上的多模态 VLM
- [基于 BFCL 的 agent 工具调度评估](02-智能体工具调度评估BFCL/Lecture-01.md) — 度量量化后的边缘 LLM 是否仍能调用正确的工具
- [AI 驱动的无线通信](01-AI驱动无线通信/Lecture-01.md)

---

## 结业项目

构建一个边缘 AI 系统，包含：

- 真实传感器输入或回放的生产级输入
- 优化后的模型
- runtime 部署
- 延迟与吞吐 benchmark
- 内存报告
- 功耗或热管理报告
- 健康检查
- 更新或回滚方案

优秀的结业项目示例：

- Jetson 多摄像头检测与跟踪系统
- 带内存与热管理控制的本地 LLM runtime
- 带功耗预算的 MCU 关键词识别
- 支持 OTA 模型更新的工业异常检测器
- 多模态机器人感知节点

当另一位工程师能够复现该部署，并理解塑造了该设计的各项约束时，结业项目即告完成。

---

## 达成标准

当你能做到以下几点时，即可宣称具备边缘 AI 专精能力：

- 根据工作负载需求选型硬件
- 量化并部署模型，且给出实测取舍
- 端到端 profile 边缘推理 runtime
- 构建从传感器到模型的流水线
- 解释持续负载下的功耗与热管理行为
- 设计更新、回滚与遥测路径
- 交付可复现的边缘 AI benchmark 或案例研究


<details>
<summary>English original</summary>

**Measure It**

- update time
- rollback time
- telemetry volume
- offline recovery behavior
- version compatibility checks

**Ship It**

An edge AI operations plan that another engineer could use to ship the model to devices safely.

---

**Featured Deep Dives**

Use these lectures as the technical core for LLM and wireless-oriented edge work:

- [Edge LLM Inference Internals](03-边缘LLM推理内部机制/Lecture-01.md)
- [Qwen Inference Optimization](05-Qwen推理优化/README.md)
- [Gemma 4 Edge Deployment](04-Gemma-4边缘部署/README.md) — PDL pipeline, interleaved attention KV math, speculative decode, and multimodal VLM on Jetson Orin/Thor
- [Agent Tool-Dispatch Evaluation with BFCL](02-智能体工具调度评估BFCL/Lecture-01.md) — measure whether a quantized edge LLM still calls the right tool
- [AI-Driven Wireless Communication](01-AI驱动无线通信/Lecture-01.md)

---

**Capstone**

Build an edge AI system that includes:

- real sensor or replayed production-like input
- optimized model
- runtime deployment
- latency and throughput benchmark
- memory report
- power or thermal report
- health checks
- update or rollback plan

Good capstone examples:

- Jetson multi-camera detection and tracking system
- local LLM runtime with memory and thermal controls
- MCU keyword spotter with power budget
- industrial anomaly detector with OTA model updates
- multimodal robot perception node

The capstone is complete when another engineer can reproduce the deployment and understand the constraints that shaped the design.

---

**Exit Criteria**

You are ready to claim edge AI specialization when you can:

- select hardware from workload requirements
- quantize and deploy a model with measured tradeoffs
- profile an edge inference runtime end to end
- build a sensor-to-model pipeline
- explain power and thermal behavior under sustained load
- design update, rollback, and telemetry paths
- ship a reproducible edge AI benchmark or case study

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
