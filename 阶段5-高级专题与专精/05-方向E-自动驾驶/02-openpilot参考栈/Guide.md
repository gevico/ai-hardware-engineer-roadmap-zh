---
title: 模块 2 — openpilot Reference Stack
description: 模块 2 — openpilot Reference Stack
---

# 模块 2 — openpilot Reference Stack

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">MORS</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入钻研 · 专业化</p>
<p class="course-identity__title">模块 2 — openpilot Reference Stack 的专业化课程标识。</p>
<p class="course-identity__meta">产物：专业化案例研究 · 衡量指标：性能、可靠性、岗位匹配度</p>
</div>
</div>


**父级：** [阶段 5 — 自动驾驶](../Guide.md)

**时间：** 3–4 个月

**前置要求：** 模块 1（基础）、阶段 4 方向 B（Jetson — CUDA、Linux BSP、设备树）。

---

## 为什么选 openpilot

[openpilot](https://github.com/commaai/openpilot) 是 comma.ai 的开源 ADAS — 已量产部署、文档完善，并使用 tinygrad 做推理。它是公开可得的最佳端到端参考，用于研究自动驾驶系统究竟如何工作：从摄像头原始像素一直到 CAN bus 转向指令。

---

## 1. 架构总览

* **端到端流水线：**
    * `camerad` → `modeld` → `plannerd` → `controlsd` → `pandad` → CAN bus → 车辆执行器。
    * 各进程通过 **cereal**（基于 msgpack 的 IPC）和 **VisionIpc**（共享内存零拷贝帧）通信。

* **硬件：**
    * comma 3X / comma four：Snapdragon 845/8 Gen 2，3 个摄像头（广角道路、道路、驾驶员），GPS，IMU。
    * **panda**：用于车辆通信的 CAN-to-USB 接口。

* **软件层：**
    * **AGNOS**：面向 comma 设备的定制 Linux 发行版（上游内核的 fork）。
    * **openpilot**：Python/C++ 应用层 — 感知、规划、控制。
    * **tinygrad**：神经网络模型的推理 runtime。

* **[流程图](flow-diagram.md)** — 从摄像头输入到 CAN 执行的数据流详图，含进程到源码的映射。

**项目：**
* 克隆 openpilot。通过阅读 `selfdrive/` 源码，追踪从 `camerad` 到 `pandad` 的数据流。记录各进程之间的 IPC 消息。
* 使用 comma 的 replay 工具或 CARLA 集成，在仿真中运行 openpilot。

---

## 2. 摄像头流水线（camerad）

> **深入钻研：** [camerad 指南](02-camerad/Guide.md)

* **传感器硬件：** OX03C10 / OS04C10 图像传感器，Qualcomm Spectra ISP。
* **采集流程：** Sensor RAW → CSI → IFE（demosaic、CCM、gamma）→ BPS → YUV NV12。
* **自动曝光（AE）：** 软件算法，3 帧延迟，类 PI 控制环，夜间行驶时采用 DC 增益迟滞。
* **VisionIpc：** 共享内存 IPC，用于从 `camerad` 到 `modeld` 的零拷贝帧传输。
* **V4L2 集成：** 使用 Linux Video4Linux2 API 进行摄像头设备控制、request manager、缓冲区流转。

**项目：**
* 阅读 `system/camerad/` 源码。追踪单帧从 sensor DQBUF 到 VisionIpc publish 的全过程。
* 修改 AE 目标灰阶比例。观察不同光照条件下对曝光的影响。

---

## 3. AGNOS 操作系统

> **深入钻研：** [AGNOS + OS 课程](01-agnos/Guide.md)

* **AGNOS 是什么：** Linux 内核（`agnos-kernel-sdm845`）的 fork + 面向 comma 设备的定制构建系统（`agnos-builder`）。
* **关键内核定制：** 摄像头驱动（V4L2/media）、SDM845 设备树、SPI（面向 panda 的 CAN-over-SPI）、热管理/电源管理、用于进程隔离的 cgroups。
* **与阶段 1 操作系统课程的对应关系：** 阶段 1 的全部 26 讲操作系统课程都在 AGNOS 指南中对应到了具体的内核路径和 openpilot 用例。

**项目：**
* 克隆 `agnos-kernel-sdm845`。按照 AGNOS 指南，追踪阶段 1 的操作系统概念（进程、中断、调度、设备树）在量产 ADAS 内核中如何体现。
* 找出 3 个摄像头传感器对应的设备树节点。理解 CSI lane 和时钟如何配置。

---

## 4. 感知（modeld）

* **模型架构：** 端到端神经网络，输入多摄像头画面，输出车道线、前车、位姿、规划以及期望路径。
* **Warp 矩阵：** 推理前施加基于校准的图像 warp，以归一化摄像头视角。
* **tinygrad 推理：** 模型在 Snapdragon GPU（Adreno）上通过 tinygrad 运行 — 见 [模块 3](../03-tinygrad推理/Guide.md)。
* **输出：** `modelV2` cereal 消息 — 车道线、道路边缘、位姿、规划轨迹、动作（油门/刹车/转向）、前向碰撞预警（FCW）。

**项目：**
* 阅读 `selfdrive/modeld/`。追踪模型输入准备（帧 + warp）经 tinygrad 推理到 modelV2 输出的全过程。
* 在行驶回放过程中记录 modelV2 输出。可视化预测的车道线和前车位置。

---


<details>
<summary>English original</summary>

**Module 2 — openpilot Reference Stack**

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">MORS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for Module 2 — openpilot Reference Stack.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [Phase 5 — Autonomous Driving](../Guide.md)

**Time:** 3–4 months

**Prerequisites:** Module 1 (Fundamentals), Phase 4 Track B (Jetson — CUDA, Linux BSP, device trees).

---

**Why openpilot**

[openpilot](https://github.com/commaai/openpilot) is comma.ai's open-source ADAS — production-deployed, well-documented, and using tinygrad for inference. It's the best publicly available end-to-end reference for studying how an autonomous driving system actually works: from raw camera pixels to CAN bus steering commands.

---

**1. Architecture Overview**

* **End-to-end pipeline:**
    * `camerad` → `modeld` → `plannerd` → `controlsd` → `pandad` → CAN bus → vehicle actuators.
    * Each process communicates via **cereal** (msgpack-based IPC) and **VisionIpc** (shared-memory zero-copy frames).

* **Hardware:**
    * comma 3X / comma four: Snapdragon 845/8 Gen 2, 3 cameras (wide road, road, driver), GPS, IMU.
    * **panda**: CAN-to-USB interface for vehicle communication.

* **Software layers:**
    * **AGNOS**: Custom Linux distro (fork of upstream kernel) for comma devices.
    * **openpilot**: Python/C++ application layer — perception, planning, control.
    * **tinygrad**: Inference runtime for neural network models.

* **[Flow Diagram](flow-diagram.md)** — Detailed data flow from camera input to CAN actuation with process-to-source mapping.

**Projects:**
* Clone openpilot. Trace the data flow from `camerad` to `pandad` by reading `selfdrive/` source. Document the IPC messages between each process.
* Run openpilot in simulation using comma's replay tools or CARLA integration.

---

**2. Camera Pipeline (camerad)**

> **Deep dive:** [camerad Guide](02-camerad/Guide.md)

* **Sensor hardware:** OX03C10 / OS04C10 image sensors, Qualcomm Spectra ISP.
* **Capture flow:** Sensor RAW → CSI → IFE (demosaic, CCM, gamma) → BPS → YUV NV12.
* **Auto exposure (AE):** Software algorithm with 3-frame latency, PI-like control loop, DC gain hysteresis for night driving.
* **VisionIpc:** Shared-memory IPC for zero-copy frame transport from `camerad` to `modeld`.
* **V4L2 integration:** Linux Video4Linux2 API for camera device control, request manager, buffer flow.

**Projects:**
* Read `system/camerad/` source. Trace a single frame from sensor DQBUF to VisionIpc publish.
* Modify the AE target grey fraction. Observe the effect on exposure in different lighting conditions.

---

**3. AGNOS Operating System**

> **Deep dive:** [AGNOS + OS Course](01-agnos/Guide.md)

* **What AGNOS is:** Fork of Linux kernel (`agnos-kernel-sdm845`) + custom build system (`agnos-builder`) for comma devices.
* **Key kernel customizations:** Camera drivers (V4L2/media), device tree for SDM845, SPI (CAN-over-SPI for panda), thermal/power management, cgroups for process isolation.
* **Maps to Phase 1 OS lectures:** All 26 OS lectures from Phase 1 are mapped to specific kernel paths and openpilot use cases in the AGNOS guide.

**Projects:**
* Clone `agnos-kernel-sdm845`. Follow the AGNOS guide to trace how Phase 1 OS concepts (processes, interrupts, scheduling, device tree) manifest in a production ADAS kernel.
* Identify the device tree nodes for the 3 camera sensors. Understand how CSI lanes and clocks are configured.

---

**4. Perception (modeld)**

* **Model architecture:** End-to-end neural network taking multi-camera input, outputting lanes, lead vehicles, pose, plan, and desired path.
* **Warp matrix:** Calibration-based image warping applied before inference to normalize camera viewpoint.
* **tinygrad inference:** Models run through tinygrad on Snapdragon GPU (Adreno) — see [Module 3](../03-tinygrad推理/Guide.md).
* **Outputs:** `modelV2` cereal message — lanes, road edges, pose, plan trajectory, action (gas/brake/steer), forward collision warning (FCW).

**Projects:**
* Read `selfdrive/modeld/`. Trace model input preparation (frame + warp) through tinygrad inference to modelV2 output.
* Log modelV2 outputs during a drive replay. Visualize predicted lanes and lead vehicle positions.

---

</details>

## 5. 规划与控制

* **plannerd：**
    * `LongitudinalPlanner`：速度曲线、跟车距离、走走停停。
    * `LaneDepartureWarning`：横向安全监控。
    * 输入：modelV2（感知）、radarState、carState。

* **controlsd：**
    * `LatControl`：用于转向的横向 PID/INDI/力矩控制器。
    * `LongControl`：用于油门/刹车的纵向 PID。
    * 输出：`carControl` cereal 消息 → `CarInterface` → CAN 指令。

* **pandad + CAN：**
    * `pandad` 通过 panda 设备把 carControl 转换为原始 CAN 消息。
    * 车辆特定的 `CarInterface` 实现按厂商/车型处理 DBC 编码。

**项目：**
* 追踪一条转向指令：从 plannerd 的期望路径出发，经 controlsd 的横向控制器，直到最终的 CAN 消息。记录该控制回路。
* 将 openpilot 的横向控制（基于力矩）与 Module 1 中的 Stanley 控制器进行对比。

---

## 6. Fork 与贡献

* **Fork 工作流：** fork openpilot，搭建开发环境，构建并测试。
* **车辆移植：** 新增一款车的支持——CAN DBC 逆向工程、`CarInterface` 实现、指纹识别。
* **社区：** 活跃的 Discord、社区 fork、面向贡献的悬赏计划。

**项目：**
* fork openpilot。做一处小改动（例如调整某个 UI 元素或调参）。构建、在仿真中测试，并提交 PR。
* （进阶）研究你自己车辆的 CAN DBC。实现基本的解析与执行。

---

## 资源

| 资源 | URL |
|----------|-----|
| openpilot 源码 | https://github.com/commaai/openpilot |
| agnos-kernel-sdm845 | https://github.com/commaai/agnos-kernel-sdm845 |
| agnos-builder | https://github.com/commaai/agnos-builder |
| comma.ai 博客 | https://blog.comma.ai/ |
| openpilot 社区文档 | https://github.com/commaai/openpilot/wiki |

---

## 下一章

→ **[Module 3 — tinygrad for Inference](../03-tinygrad推理/Guide.md)** — 深入剖析为 openpilot 的感知提供算力的推理引擎。


<details>
<summary>English original</summary>

**5. Planning and Control**

* **plannerd:**
    * `LongitudinalPlanner`: speed profile, following distance, stop-and-go.
    * `LaneDepartureWarning`: lateral safety monitoring.
    * Inputs: modelV2 (perception), radarState, carState.

* **controlsd:**
    * `LatControl`: lateral PID/INDI/torque controller for steering.
    * `LongControl`: longitudinal PID for gas/brake.
    * Outputs: `carControl` cereal message → `CarInterface` → CAN commands.

* **pandad + CAN:**
    * `pandad` translates carControl into raw CAN messages via the panda device.
    * Vehicle-specific `CarInterface` implementations handle DBC encoding per make/model.

**Projects:**
* Trace a steering command from plannerd's desired path through controlsd's lateral controller to the final CAN message. Document the control loop.
* Compare openpilot's lateral control (torque-based) with the Stanley controller from Module 1.

---

**6. Forking and Contributing**

* **Fork workflow:** Fork openpilot, set up development environment, build and test.
* **Vehicle porting:** Add support for a new vehicle — CAN DBC reverse engineering, `CarInterface` implementation, fingerprinting.
* **Community:** Active Discord, community forks, bounty programs for contributions.

**Projects:**
* Fork openpilot. Make a small change (e.g., adjust a UI element or tuning parameter). Build, test in simulation, and submit a PR.
* (Advanced) Study the CAN DBC for your vehicle. Implement basic parsing and actuation.

---

**Resources**

| Resource | URL |
|----------|-----|
| openpilot source | https://github.com/commaai/openpilot |
| agnos-kernel-sdm845 | https://github.com/commaai/agnos-kernel-sdm845 |
| agnos-builder | https://github.com/commaai/agnos-builder |
| comma.ai Blog | https://blog.comma.ai/ |
| openpilot community docs | https://github.com/commaai/openpilot/wiki |

---

**Next**

→ **[Module 3 — tinygrad for Inference](../03-tinygrad推理/Guide.md)** — Deep dive into the inference engine that powers openpilot's perception.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/2. openpilot Reference Stack/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/2.%20openpilot%20Reference%20Stack/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
