---
title: 边缘 AI
description: 边缘 AI
---

# 边缘 AI

<div class="course-identity auto-course" style="--course-accent: #ca8a04; --course-accent-rgb: 202, 138, 4;" markdown="1">
<div class="course-identity__icon">EA</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · AI 工作负载</p>
<p class="course-identity__title">边缘 AI 的专门课程标识。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


**阶段 3 — 人工智能**（独立主题）。学习模型**在哪里**运行于端侧以及**为什么**；在 **[神经网络](../../01-神经网络/Guide.md)** 中学习它们**计算什么**。

> **目标：** 梳理边缘栈——延迟、隐私、功耗层级，以及训练 → 优化 → 部署流水线——让阶段 4（Xilinx 或 Jetson）与各专业化方向都有清晰的上下文。

**前置：** [阶段 1 §4 — C++ 与并行计算](../../../阶段1-基础知识/04-C++与并行计算/Guide.md) · **配套：** [神经网络](../../01-神经网络/Guide.md)（MLP、CNN、训练、tinygrad） · **下一步（部署深度）：** [阶段 4 方向 B — Jetson](../../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md)

---


## 1. 定义

**边缘 AI** = 在**设备本地**（即“边缘”）运行 AI 算法，而不是把数据发送到远程云服务器。

```
Traditional Cloud AI:
  Device → [internet] → Cloud Server (GPU farm) → [internet] → Result
  Latency: 50–300ms   Privacy risk   Needs connectivity

Edge AI:
  Device → Local Chip (CPU/GPU/NPU/FPGA) → Result
  Latency: <1ms       Data stays local   Works offline
```

---

## 2. 边缘 AI 为何存在

| 云端 AI 的问题        | 边缘 AI 的解决方案                        |
|------------------------------|-----------------------------------------|
| 网络延迟（~100ms）     | 亚毫秒级本地推理         |
| 带宽成本（视频数据）  | 只发送结果，不发送原始数据         |
| 隐私（人脸/语音/医疗） | 数据从不离开设备            |
| 可靠性（无互联网）    | 完全离线工作                     |
| 规模化的云成本          | 一次性硬件成本                  |

---

## 3. 边缘 AI 在哪里运行（层级）

```
Tier 1 — Microcontrollers (MCU):
  STM32, Arduino, RP2040
  RAM: 256KB–512KB
  Power: <1W
  Use: keyword spotting, gesture detection

Tier 2 — Embedded Linux SBCs:
  Raspberry Pi, BeagleBone
  RAM: 1–8GB
  Power: 2–10W
  Use: image classification, object detection

Tier 3 — AI Accelerator SoCs:
  Nvidia Jetson, Google Coral (TPU), Apple Neural Engine
  RAM: 4–64GB
  Power: 5–30W
  Use: real-time video inference, NLP, robotics

Tier 4 — Edge Servers:
  FPGA + GPU combinations, industrial PCs
  Power: 50–300W
  Use: factory automation, autonomous vehicles
```

---

## 4. 边缘 AI 流水线

```
1. Train model on a powerful workstation/cloud (large data, many epochs)
2. Optimize model for edge (quantization, pruning, distillation)
3. Convert model to edge runtime format (ONNX, TensorRT, TFLite)
4. Deploy to edge device
5. Run inference locally in real-time
```

步骤 1 建立在 **[神经网络](../../01-神经网络/Guide.md)** 的基础上。步骤 2–5 在**阶段 4 方向 B**（[机器学习与 AI](../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/05-机器学习与人工智能/Guide.md)）中展开，对于定制芯片，则在**阶段 4 方向 A**和**阶段 5 — AI 芯片设计**中展开。

---

## 5. 这如何契合路线图

| 你想要… | 从这里开始 | 然后 |
|-----------|------------|------|
| 对张量、反向传播、CNN 的直觉 | [神经网络](../../01-神经网络/Guide.md) | tinygrad 动手实践，[micrograd](../../02-深度学习框架/02-micrograd/Guide.md) |
| 产品与部署上下文（本指南） | 浏览上面的层级 + 流水线 | 阶段 4 的 Jetson 或 FPGA 方向 |
| 视觉预处理与传统 CV | [计算机视觉](../01-计算机视觉/Guide.md) | 阶段 4 的感知流水线 |
| 多传感器校准、跟踪、BEV 融合 | [传感器融合](../02-传感器融合/Guide.md) | 阶段 4 的 Jetson + [ROS2](../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/06-ROS-2/Guide.md) 用于集成 |

**枢纽：** [阶段 3 — 人工智能](../../Guide.md)


<details>
<summary>English original</summary>

**Edge AI**

<div class="course-identity auto-course" style="--course-accent: #ca8a04; --course-accent-rgb: 202, 138, 4;" markdown="1">
<div class="course-identity__icon">EA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for Edge AI.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


**Phase 3 — Artificial Intelligence** (standalone topic). Learn **where** and **why** models run on-device; learn **what** they compute in **[Neural Networks](../../01-神经网络/Guide.md)**.

> **Goal:** Map the edge stack—latency, privacy, power tiers, and the train → optimize → deploy pipeline—so Phase 4 (Xilinx or Jetson) and specialization tracks have clear context.

**Previous:** [Phase 1 §4 — C++ and Parallel Computing](../../../阶段1-基础知识/04-C++与并行计算/Guide.md) · **Companion:** [Neural Networks](../../01-神经网络/Guide.md) (MLPs, CNNs, training, tinygrad) · **Next (deployment depth):** [Phase 4 Track B — Jetson](../../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md)

---


**1. Definition**

**Edge AI** = running AI algorithms **locally on a device** (the "edge") instead of sending data to a remote cloud server.

```
Traditional Cloud AI:
  Device → [internet] → Cloud Server (GPU farm) → [internet] → Result
  Latency: 50–300ms   Privacy risk   Needs connectivity

Edge AI:
  Device → Local Chip (CPU/GPU/NPU/FPGA) → Result
  Latency: <1ms       Data stays local   Works offline
```

---

**2. Why edge AI exists**

| Problem with Cloud AI        | Edge AI Solution                        |
|------------------------------|-----------------------------------------|
| Network latency (~100ms)     | Sub-millisecond local inference         |
| Bandwidth cost (video data)  | Only send results, not raw data         |
| Privacy (face/voice/medical) | Data never leaves the device            |
| Reliability (no internet)    | Works fully offline                     |
| Cloud cost at scale          | One-time hardware cost                  |

---

**3. Where edge AI runs (tiers)**

```
Tier 1 — Microcontrollers (MCU):
  STM32, Arduino, RP2040
  RAM: 256KB–512KB
  Power: <1W
  Use: keyword spotting, gesture detection

Tier 2 — Embedded Linux SBCs:
  Raspberry Pi, BeagleBone
  RAM: 1–8GB
  Power: 2–10W
  Use: image classification, object detection

Tier 3 — AI Accelerator SoCs:
  Nvidia Jetson, Google Coral (TPU), Apple Neural Engine
  RAM: 4–64GB
  Power: 5–30W
  Use: real-time video inference, NLP, robotics

Tier 4 — Edge Servers:
  FPGA + GPU combinations, industrial PCs
  Power: 50–300W
  Use: factory automation, autonomous vehicles
```

---

**4. The edge AI pipeline**

```
1. Train model on a powerful workstation/cloud (large data, many epochs)
2. Optimize model for edge (quantization, pruning, distillation)
3. Convert model to edge runtime format (ONNX, TensorRT, TFLite)
4. Deploy to edge device
5. Run inference locally in real-time
```

Step 1 is grounded in **[Neural Networks](../../01-神经网络/Guide.md)**. Steps 2–5 are expanded in **Phase 4 Track B** ([ML and AI](../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/05-机器学习与人工智能/Guide.md)) and, for custom silicon, **Phase 4 Track A** and **Phase 5 — AI Chip Design**.

---

**5. How this fits the roadmap**

| You want… | Start here | Then |
|-----------|------------|------|
| Intuition for tensors, backprop, CNNs | [Neural Networks](../../01-神经网络/Guide.md) | tinygrad hands-on, [micrograd](../../02-深度学习框架/02-micrograd/Guide.md) |
| Product and deployment context (this guide) | Skim tiers + pipeline above | Phase 4 Jetson or FPGA track |
| Vision preprocessing and classical CV | [Computer Vision](../01-计算机视觉/Guide.md) | Phase 4 perception pipelines |
| Multi-sensor calibration, tracking, BEV fusion | [Sensor Fusion](../02-传感器融合/Guide.md) | Phase 4 Jetson + [ROS2](../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/06-ROS-2/Guide.md) for integration |

**Hub:** [Phase 3 — Artificial Intelligence](../../Guide.md)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/6. Edge AI and Model Optimization/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/6.%20Edge%20AI%20and%20Model%20Optimization/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
