---
title: 第 2 讲：工业与嵌入式机器人
description: 第 2 讲：工业与嵌入式机器人
---

# 第 2 讲：工业与嵌入式机器人

## 概述

工业与嵌入式机器人，正是 **ROS 2 原型** 与 **工厂车间**、**现场部署** 和 **资源限制** 相遇的地方。本讲关注 **接口**（对接 PLC 与 MES）、**可复现性**（Docker、镜像）、**仿真保真度**（Gazebo 对比 Isaac Sim），以及当错过截止时间就意味着安全故障或零件报废时的 **实时** 行为。

**学完本讲，你应当能够：**

* 把 **OPC UA**、**Modbus** 和 **EtherCAT** 放到自动化栈中的正确位置，并为新建产线与改造产线各选一个默认方案。
* 解释 **ROS-I** 为何存在，以及它与“在笔记本上做研究的 ROS 2”有何不同。
* 勾勒一条 **部署** 路径：开发容器 → 机器人上的 Jetson 镜像 → OTA 更新（概念层面）。
* 在感知密集型和 RL 密集型项目中，对比 **Gazebo / Gazebo Sim** 与 **Isaac Sim** 的工作流。
* 列出 **协作机器人（cobot）** 的主要安全思路（速度、间距、力限制），以及 ISO/TS 15066 所处的位置。

---

## 推荐课程（本方向）

* [Introduction to Gazebo Sim with ROS 2](https://app.theconstruct.ai/courses/introduction-to-gazebo-ignition-with-ros2-170/) 和 [Mastering Gazebo Simulator](https://app.theconstruct.ai/courses/mastering-gazebo-simulator-78/) — Gazebo Sim + worlds/models。
* [Docker for Robotics](https://app.theconstruct.ai/courses/docker-basics-for-robotics-114/) — 容器化开发与部署。
* [Linux for Robotics](https://app.theconstruct.ai/courses/linux-for-robotics-noetic-185/) — shell、权限、工作流（标题里写的是 ROS 1；技能可直接迁移）。
* [C++ for Robotics](https://app.theconstruct.ai/courses/c-for-robotics-59/) / [Python 3 for Robotics](https://app.theconstruct.ai/courses/python-3-for-robotics-58/) — 在写 ROS 2 节点之前的语言准备。
* Nvidia [Isaac Sim documentation](https://docs.omniverse.nvidia.com/isaacsim/latest/index.html) — 教程；桥接到 ROS 2 工作时参见 [ROS 2 tutorials](https://docs.omniverse.nvidia.com/isaacsim/latest/ros2_tutorials/index.html)。

---

## 1. 工业自动化与 ROS-Industrial

### 1.1 ROS-Industrial 增加了什么

**ROS-Industrial（ROS-I）** 软件包把 **工业机械臂**、**夹爪** 和 **工位布局** 桥接到 ROS 生态：驱动、校准，以及适合 **结构化环境**（工位、夹具、传送带）的运动模式。实践中，要把 **MoveIt 2** / 轨迹执行与 **厂商控制器** 和 **安全 PLC** 结合起来，后两者掌管急停与安全光幕。

### 1.2 与工厂对话

| 技术 | 优势 | 典型角色 |
|------------|----------|--------------|
| **OPC UA** | 信息模型丰富、安全性、pub/sub | MES ↔ 机器人 / SCADA，现代产线 |
| **Modbus TCP** | 简单、无处不在 | 老旧 PLC、传感器、快速集成 |
| **EtherCAT** | 确定性的、周期性 I/O | 紧耦合运动 + I/O；高端机械臂中常见 |

**设计模式：** ROS 2 节点负责 **感知与运动意图**；**PLC 或安全继电器** 执行 **硬停机** 与 **互锁**。不要把“我能写一个 Modbus 线圈”与“我通过了安全认证”混为一谈。

### 1.3 协作机器人（cobot）

协作机器人用 **负载与速度** 换取 **力限制** 运行和更简单的防护——但 **风险评估** 依然必不可少。**ISO/TS 15066** 等标准给出了相对人的 **速度间距** 与 **力** 限制。当人进入某个区域时，软件必须遵守 **降速模式**（通常由安全等级输入硬接线触发）。

---

## 2. 嵌入式部署

### 2.1 目标平台

| 平台 | 出现场景 |
|----------|------------------|
| **NVIDIA Jetson** | GPU 感知、TensorRT、边缘多摄像头 |
| **ARM SBC**（如 Raspberry Pi 级别） | 轻量 I/O、桥接、教学 |
| **x86 迷你主机** | 开发，以及部分散热余量充足的生产工位 |

在嵌入式上跑 ROS 2，意味着你要关注 **CPU 预算**、**内存**、**存储磨损**，以及持续推理过程中的 **热降频**。

### 2.2 实时 Linux

标准 Linux 采用 **尽力而为** 的调度。对于 **伺服级关节控制** 或 **紧耦合 I/O**，团队会使用 **PREEMPT_RT** 内核或专用运动控制器。ROS 2 可以运行在 RT 内核上，但要获得 **确定性**，仍需谨慎设置线程优先级、**内存锁定** 和 **isolcpus**。经验法则：**硬实时环路** 往往不在 ROS 内，而在厂商控制器中；ROS 以更低的速率提供 **设定点**。

### 2.3 Docker 与可复现性

**机器人项目为什么用 Docker**

* 笔记本、CI 和机器人上使用同一个镜像。
* 固定 Nav2 / 感知栈的 **发行版 + 依赖**。

**注意事项**

* 容器内要用 CUDA / TensorRT，需要 GPU 和 **NVIDIA Container Toolkit**。
* **设备节点**（`/dev/video*`、CAN、GPIO）必须显式透传。
* 跨容器的 DDS **网络** 需要显式配置。

模式：**一个**“robot runtime”镜像；用 git tag 打版本；在镜像旁记录 **内核 + JetPack**（或宿主 OS）。

---


<details>
<summary>English original</summary>

**Lecture 2: Industrial and Embedded Robotics**

**Overview**

Industrial and embedded robotics is where **ROS 2 prototypes** meet **factory floors**, **field deployment**, and **resource limits**. This lecture is about **interfaces** (to PLCs and MES), **repeatability** (Docker, images), **simulation fidelity** (Gazebo vs Isaac Sim), and **real-time** behavior when a missed deadline means a safety fault or scrapped part.

**By the end of this lecture you should be able to:**

* Place **OPC UA**, **Modbus**, and **EtherCAT** in the automation stack and choose a default for greenfield vs retrofit.
* Explain why **ROS-I** exists and how it differs from “research ROS 2 on a laptop.”
* Sketch a **deployment** path: dev container → Jetson image on the robot → OTA updates (conceptually).
* Compare **Gazebo / Gazebo Sim** workflows with **Isaac Sim** for perception-heavy and RL-heavy projects.
* List the main **cobot** safety ideas (speed, separation, force limiting) and where ISO/TS 15066 fits.

---

**Recommended courses (this track)**

* [Introduction to Gazebo Sim with ROS 2](https://app.theconstruct.ai/courses/introduction-to-gazebo-ignition-with-ros2-170/) and [Mastering Gazebo Simulator](https://app.theconstruct.ai/courses/mastering-gazebo-simulator-78/) — Gazebo Sim + worlds/models.
* [Docker for Robotics](https://app.theconstruct.ai/courses/docker-basics-for-robotics-114/) — containerized dev and deploy.
* [Linux for Robotics](https://app.theconstruct.ai/courses/linux-for-robotics-noetic-185/) — shell, permissions, workflows (ROS 1 in the title; skills transfer directly).
* [C++ for Robotics](https://app.theconstruct.ai/courses/c-for-robotics-59/) / [Python 3 for Robotics](https://app.theconstruct.ai/courses/python-3-for-robotics-58/) — language prep before ROS 2 nodes.
* Nvidia [Isaac Sim documentation](https://docs.omniverse.nvidia.com/isaacsim/latest/index.html) — tutorials; see [ROS 2 tutorials](https://docs.omniverse.nvidia.com/isaacsim/latest/ros2_tutorials/index.html) when bridging to ROS 2 work.

---

**1. Industrial automation and ROS-Industrial**

**1.1 What ROS-Industrial adds**

**ROS-Industrial (ROS-I)** packages bridge **industrial arms**, **grippers**, and **cell layouts** to the ROS ecosystem: drivers, calibration, and motion patterns suitable for **structured environments** (cells, fixtures, conveyors). In practice you combine **MoveIt 2** / trajectory execution with **vendor controllers** and **safety PLCs** that own emergency stops and light curtains.

**1.2 Talking to the factory**

| Technology | Strength | Typical role |
|------------|----------|--------------|
| **OPC UA** | Rich information model, security, pub/sub | MES ↔ robot / SCADA, modern lines |
| **Modbus TCP** | Simple, ubiquitous | Legacy PLCs, sensors, quick integration |
| **EtherCAT** | Deterministic, cyclic I/O | Tight motion + I/O; common in high-end arms |

**Design pattern:** ROS 2 nodes handle **perception and motion intent**; a **PLC or safety relay** enforces **hard stops** and **interlocks**. Do not confuse “I can send a Modbus coil” with “I am certified safe.”

**1.3 Collaborative robots (cobots)**

Cobots trade **payload and speed** for **force-limited** operation and simpler guarding—but **risk assessment** is still required. Standards such as **ISO/TS 15066** inform **speed separation** and **force** limits relative to humans. Software must respect **reduced mode** when a person enters a zone (often wired from safety-rated inputs).

---

**2. Embedded deployment**

**2.1 Targets**

| Platform | When it shows up |
|----------|------------------|
| **NVIDIA Jetson** | GPU perception, TensorRT, multi-camera on edge |
| **ARM SBCs** (e.g. Raspberry Pi class) | Lightweight I/O, bridges, teaching |
| **x86 mini-PCs** | Development, some production cells with thermal headroom |

ROS 2 on embedded means you care about **CPU budget**, **memory**, **storage wear**, and **thermal throttling** during sustained inference.

**2.2 Real-time Linux**

Standard Linux is **best-effort** scheduling. For **servo-grade joint control** or **tight I/O**, teams use **PREEMPT_RT** kernels or dedicated motion controllers. ROS 2 can run on RT kernels, but **determinism** still requires careful thread priority, **memory locking**, and **isolcpus**. Rule of thumb: **hard real-time loops** often live outside ROS in a vendor controller; ROS supplies **setpoints** at a slower rate.

**2.3 Docker and reproducibility**

**Why Docker for robotics**

* Same image on laptop, CI, and robot.
* Pin **distro + dependencies** for Nav2 / perception stacks.

**Caveats**

* GPU and **NVIDIA Container Toolkit** for CUDA / TensorRT inside containers.
* **Device nodes** (`/dev/video*`, CAN, GPIO) must be passed through deliberately.
* **Networking** for DDS across containers needs explicit configuration.

Pattern: **one** “robot runtime” image; version with git tags; document **kernel + JetPack** (or host OS) beside the image.

---

</details>

## 3. Simulation and testing

### 3.1 URDF, SDF, and models

* **URDF：** 树形结构的机器人描述；广泛用于 ROS。
* **SDF：** **Gazebo Sim** 中的世界与模型格式；在某些工作流中支持更多物理特性。

**仿真差距**通常来自**错误的惯性**、**错误的摩擦**或**缺失的齿隙**——调到足以捕获**集成 bug**即可，不必与现实精确到微米。

### 3.2 Gazebo / Gazebo Sim + ROS 2

用 **`ros_gz`** 桥接 **Gazebo Sim** 与 ROS 2 topic。适合 **Nav2** bring-up（上电点亮/调通）、**传感器**原型验证和 **CI** 冒烟测试。

### 3.3 Isaac Sim

**Isaac Sim** 面向**高保真渲染**、**传感器仿真**、**合成数据**，以及用于 RL 的 **Isaac Lab**。[ROS 2 桥](https://docs.omniverse.nvidia.com/isaacsim/latest/ros2_tutorials/index.html)连接到与你在 [Lecture 1](../02-高级机器人操作系统/Lecture-01.md) 中学到的相同的图模式，但编写环境是**基于 USD** 的且重度依赖 GPU。

| Need | Often choose |
|------|----------------|
| Nav2 + LiDAR 激光雷达 bring-up、轻量 CI | Gazebo Sim |
| 照片级真实数据、RL、数字孪生 | Isaac Sim |

---

## 4. Sim-to-real 检查清单

1. **时钟：** 仿真时间与墙上时钟通过 ROS 2 time 对齐（`use_sim_time`）。
2. **传感器延迟：** 在信任调参之前，先在感知流水线中加入真实的**延迟**。
3. **校准：** 相机的内参/外参；用于融合的**LiDAR 激光雷达–相机**外参。
4. **动力学：** 针对操作力验证**质量/惯性**的量级。

---

## 5. Projects（来自本路线图）

* **Jetson 部署：** 在 Jetson 上运行 Nav2 或感知 + `ros2_control` 栈；测量持续负载下的 **CPU/GPU** 与**热管理**。
* **Sim-to-real：** 在 Gazebo Sim 中实现一个功能（例如避障）→ 在硬件上运行相同的 launch 图；记录**哪里出问题了**（TF、QoS、校准）。

---

## 6. Self-check

1. 即使 ROS 2 规划了所有运动，为什么 **PLC** 仍可能掌管 E-stop？
2. 如果未映射 `/dev`，举出一个 **Docker** 对**相机**节点有风险的原因。
3. 什么时候你会为项目选择 **Isaac Sim** 而非 **Gazebo Sim**？

---

## Resources

* **结构化课程：** 见上文的 **Recommended courses**；[主 Robotics Application 指南](../Guide.md)列出了所有学习轨道。
* **ROS-Industrial：** [rosindustrial.org](https://rosindustrial.org/) 以及各发行版专属教程。
* **Gazebo：** [gazebosim.org/docs](https://gazebosim.org/docs)
* **Isaac Sim：** [docs.omniverse.nvidia.com/isaacsim](https://docs.omniverse.nvidia.com/isaacsim/latest/index.html)

---

## Next in this roadmap

* Previous: [Advanced Robot Operating System](../02-高级机器人操作系统/Lecture-01.md)
* Next: [Advanced Perception and AI for Robotics](../01-机器人高级感知与AI/Lecture-01.md)


<details>
<summary>English original</summary>

**3. Simulation and testing**

**3.1 URDF, SDF, and models**

* **URDF:** Tree-structured robot description; widely used with ROS.
* **SDF:** World and model format in **Gazebo Sim**; supports more physics features in some workflows.

A **simulation gap** often comes from **wrong inertia**, **wrong friction**, or **missing backlash**—tune enough to catch **integration bugs**, not to match reality to the micron.

**3.2 Gazebo / Gazebo Sim + ROS 2**

Use **`ros_gz`** bridge to connect **Gazebo Sim** and ROS 2 topics. Good for **Nav2** bring-up, **sensor** prototyping, and **CI** smoke tests.

**3.3 Isaac Sim**

**Isaac Sim** targets **high-fidelity rendering**, **sensor simulation**, **synthetic data**, and **Isaac Lab** for RL. The [ROS 2 bridge](https://docs.omniverse.nvidia.com/isaacsim/latest/ros2_tutorials/index.html) connects to the same graph patterns you learned in [Lecture 1](../02-高级机器人操作系统/Lecture-01.md), but the authoring environment is **USD-based** and GPU-heavy.

| Need | Often choose |
|------|----------------|
| Nav2 + LiDAR bring-up, lightweight CI | Gazebo Sim |
| Photoreal data, RL, digital twin | Isaac Sim |

---

**4. Sim-to-real checklist**

1. **Clock:** Sim time vs wall clock aligned with ROS 2 time (`use_sim_time`).
2. **Sensor delay:** Add realistic **latency** in perception pipelines before trusting tuning.
3. **Calibration:** Intrinsics/extrinsics for cameras; **LiDAR–camera** extrinsics for fusion.
4. **Dynamics:** Validate **mass/inertia** order-of-magnitude for manipulation forces.

---

**5. Projects (from this roadmap)**

* **Jetson deployment:** Run Nav2 or a perception + `ros2_control` stack on Jetson; measure **CPU/GPU** and **thermal** under sustained load.
* **Sim-to-real:** One feature (e.g. obstacle avoidance) in Gazebo Sim → same launch graph on hardware; document **what broke** (TF, QoS, calibration).

---

**6. Self-check**

1. Why might a **PLC** still own E-stop even if ROS 2 plans all motions?
2. Name one reason **Docker** is risky for **camera** nodes if `/dev` is not mapped.
3. When would you prefer **Isaac Sim** over **Gazebo Sim** for a project?

---

**Resources**

* **Structured courses:** See **Recommended courses** above; the [main Robotics Application guide](../Guide.md) lists all tracks.
* **ROS-Industrial:** [rosindustrial.org](https://rosindustrial.org/) and distro-specific tutorials.
* **Gazebo:** [gazebosim.org/docs](https://gazebosim.org/docs)
* **Isaac Sim:** [docs.omniverse.nvidia.com/isaacsim](https://docs.omniverse.nvidia.com/isaacsim/latest/index.html)

---

**Next in this roadmap**

* Previous: [Advanced Robot Operating System](../02-高级机器人操作系统/Lecture-01.md)
* Next: [Advanced Perception and AI for Robotics](../01-机器人高级感知与AI/Lecture-01.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track D - Robotics/Industrial and Embedded Robotics/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20D%20-%20Robotics/Industrial%20and%20Embedded%20Robotics/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
