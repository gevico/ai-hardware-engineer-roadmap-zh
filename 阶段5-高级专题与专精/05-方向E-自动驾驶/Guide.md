---
title: 阶段 5 —— 方向 E：自动驾驶汽车
description: 阶段 5 —— 方向 E：自动驾驶汽车
---

# 阶段 5 —— 方向 E：自动驾驶汽车

<div class="course-identity autonomous-vehicles" markdown="1">
<div class="course-identity__icon">AV</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 5E · 自动驾驶汽车</p>
<p class="course-identity__title">学习量产自动驾驶栈、感知、预测、安全、部署与车辆调试。</p>
<p class="course-identity__meta">产物：AV 栈分析 · 度量：传感器时序、模型延迟、安全约束</p>
</div>
</div>


**时间线：**12–24 个月（模块 1–3）；含进阶模块（4–5）为 24–48 个月。模块 6 为可选工具。

**前置要求：**阶段 3（**计算机视觉**、**传感器融合**）、阶段 4 方向 B —— Jetson（CUDA、TensorRT、边缘部署）。阶段 4 方向 C 推荐用于 tinygrad 编译器上下文。

**岗位目标：**ADAS 软件工程师 · 自动驾驶感知工程师 · 运动规划工程师 · 功能安全工程师 · AV 系统工程师

---

## 概述

本方向以 **[openpilot](https://github.com/commaai/openpilot)**（comma.ai）作为主要参考实现 —— 一个开源、已量产部署的 ADAS，端侧推理运行 tinygrad。你端到端地研究一个真实系统：摄像头采集、神经网络感知、规划、控制与 CAN 执行。

本方向从基础出发，经过 openpilot 代码库，再进入进阶感知研究与安全/部署标准。

---

## 模块地图

| # | 模块 | 学习内容 | 时长 |
|---|--------|---------------|------|
| **1** | [基础](01-基础/Guide.md) | 面向驾驶的 CV、规划算法、控制理论、车辆动力学 | 3–4 个月 |
| **2** | [openpilot 参考栈](02-openpilot参考栈/Guide.md) | 完整 openpilot 架构：摄像头流水线、AGNOS kernel、数据流、fork | 3–4 个月 |
| **3** | [用于推理的 tinygrad](03-tinygrad推理/Guide.md) | tinygrad 内部机制：lazy eval、3 种 op 类型、编译器流水线、后端、自定义 op | 3–4 个月 |
| **4** | [进阶感知与预测](04-高级感知与预测/Guide.md) | 传感器（激光雷达、雷达）、校准、BEV（鸟瞰图）感知、轨迹预测、HD 地图、仿真 | 6–12 个月 |
| **5** | [安全标准与部署](05-安全标准与部署/Guide.md) | ISO 26262、SOTIF、V2X、HIL 测试、影子模式、基于场景的验证 | 3–6 个月 |
| **6** | [Lauterbach TRACE32 调试](06-Lauterbach-TRACE32调试/Guide.md) | 汽车 ECU 的在线调试与 trace（可选专业工具） | 2–3 个月 |

---

## 推荐顺序

```
Module 1 (Fundamentals)
    ↓
Module 2 (openpilot) ←→ Module 3 (tinygrad)   [study in parallel or interleaved]
    ↓
Module 4 (Advanced Perception)
    ↓
Module 5 (Safety & Deployment)
    ↓
Module 6 (Lauterbach — optional, for automotive ECU roles)
```

模块 2 与模块 3 相互强化：openpilot 是系统上下文，tinygrad 是其中的推理引擎。两者一起学习。

---

## 全程使用的参考项目

| 项目 | 模块 1 | 模块 2 | 模块 3 | 模块 4 |
|---------|----------|----------|----------|----------|
| **[openpilot](https://github.com/commaai/openpilot)** | 车道/目标检测上下文 | 全栈：摄像头→ISP（图像信号处理器）→modeld→规划→CAN | modeld 内部的推理引擎 | 真实感知工作负载 |
| **[tinygrad](https://github.com/tinygrad/tinygrad)** | — | openpilot 模型的 runtime | 完整编译器 + 后端学习 | 加速器的自定义后端 |
| **[CARLA](https://carla.org/)** | 规划算法的仿真 | 在仿真中测试 openpilot | — | 合成数据、传感器模型 |

---

## 关键资源

| 资源 | URL |
|----------|-----|
| openpilot | https://github.com/commaai/openpilot |
| tinygrad | https://github.com/tinygrad/tinygrad |
| CARLA Simulator | https://carla.org/ |
| comma.ai Blog | https://blog.comma.ai/ |
| nuScenes Dataset | https://www.nuscenes.org/ |
| Waymo Open Dataset | https://waymo.com/open/ |


<details>
<summary>English original</summary>

**Phase 5 — Track E: Autonomous Vehicles**

<div class="course-identity autonomous-vehicles" markdown="1">
<div class="course-identity__icon">AV</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track 5E · Autonomous Vehicles</p>
<p class="course-identity__title">Study production autonomy stacks, perception, prediction, safety, deployment, and vehicle debugging.</p>
<p class="course-identity__meta">Artifact: AV stack analysis · Measure: sensor timing, model latency, safety constraints</p>
</div>
</div>


**Timeline:** 12–24 months (modules 1–3); 24–48 months with advanced modules (4–5). Module 6 is optional tooling.

**Prerequisites:** Phase 3 (**Computer Vision**, **Sensor Fusion**), Phase 4 Track B — Jetson (CUDA, TensorRT, edge deployment). Phase 4 Track C recommended for tinygrad compiler context.

**Role targets:** ADAS Software Engineer · Autonomous Driving Perception Engineer · Motion Planning Engineer · Functional Safety Engineer · AV Systems Engineer

---

**Overview**

This track uses **[openpilot](https://github.com/commaai/openpilot)** (comma.ai) as the primary reference implementation — an open-source, production-deployed ADAS that runs tinygrad for on-device inference. You study a real system end-to-end: camera capture, neural network perception, planning, control, and CAN actuation.

The track progresses from fundamentals through the openpilot codebase, then into advanced perception research and safety/deployment standards.

---

**Module Map**

| # | Module | What you learn | Time |
|---|--------|---------------|------|
| **1** | [Fundamentals](01-基础/Guide.md) | CV for driving, planning algorithms, control theory, vehicle dynamics | 3–4 months |
| **2** | [openpilot Reference Stack](02-openpilot参考栈/Guide.md) | Full openpilot architecture: camera pipeline, AGNOS kernel, data flow, forking | 3–4 months |
| **3** | [tinygrad for Inference](03-tinygrad推理/Guide.md) | tinygrad internals: lazy eval, 3 op types, compiler pipeline, backends, custom ops | 3–4 months |
| **4** | [Advanced Perception and Prediction](04-高级感知与预测/Guide.md) | Sensors (LiDAR, radar), calibration, BEV perception, trajectory prediction, HD maps, simulation | 6–12 months |
| **5** | [Safety Standards and Deployment](05-安全标准与部署/Guide.md) | ISO 26262, SOTIF, V2X, HIL testing, shadow mode, scenario-based validation | 3–6 months |
| **6** | [Lauterbach TRACE32 Debug](06-Lauterbach-TRACE32调试/Guide.md) | In-circuit debug and trace for automotive ECUs (optional professional tooling) | 2–3 months |

---

**Recommended Order**

```
Module 1 (Fundamentals)
    ↓
Module 2 (openpilot) ←→ Module 3 (tinygrad)   [study in parallel or interleaved]
    ↓
Module 4 (Advanced Perception)
    ↓
Module 5 (Safety & Deployment)
    ↓
Module 6 (Lauterbach — optional, for automotive ECU roles)
```

Modules 2 and 3 reinforce each other: openpilot is the system context, tinygrad is the inference engine inside it. Study them together.

---

**Reference Projects Used Throughout**

| Project | Module 1 | Module 2 | Module 3 | Module 4 |
|---------|----------|----------|----------|----------|
| **[openpilot](https://github.com/commaai/openpilot)** | Lane/object detection context | Full stack: camera→ISP→modeld→planning→CAN | Inference engine inside modeld | Real perception workloads |
| **[tinygrad](https://github.com/tinygrad/tinygrad)** | — | Runtime for openpilot models | Full compiler + backend study | Custom backend for accelerators |
| **[CARLA](https://carla.org/)** | Simulation for planning algorithms | Test openpilot in simulation | — | Synthetic data, sensor models |

---

**Key Resources**

| Resource | URL |
|----------|-----|
| openpilot | https://github.com/commaai/openpilot |
| tinygrad | https://github.com/tinygrad/tinygrad |
| CARLA Simulator | https://carla.org/ |
| comma.ai Blog | https://blog.comma.ai/ |
| nuScenes Dataset | https://www.nuscenes.org/ |
| Waymo Open Dataset | https://waymo.com/open/ |

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
