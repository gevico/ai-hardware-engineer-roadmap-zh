---
title: 阶段 5 — 路线 D：机器人（12-18 个月）
description: 阶段 5 — 路线 D：机器人（12-18 个月）
---

# 阶段 5 — 路线 D：机器人（12-18 个月）

<div class="course-identity robotics" markdown="1">
<div class="course-identity__icon">ROS</div>
<div markdown="1">
<p class="course-identity__eyebrow">路线 5D · 机器人</p>
<p class="course-identity__title">整合感知、规划、ROS 2、实时约束与具身 AI 系统。</p>
<p class="course-identity__meta">产物：机器人子系统 demo · 度量：loop rate、延迟、安全性、鲁棒性</p>
</div>
</div>


本阶段组织为 **四条讲座路线**（每条都有自己的文件夹和 `Lecture-01.md`）。从 **高级机器人操作系统** 开始，然后按顺序推进，或按主题跳读。

| # | 路线 | 文件夹 / 讲座 |
|---|--------|------------------|
| 1 | **高级机器人操作系统**（ROS / ROS 2、Nav2、SLAM、MoveIt 2） | [Advanced Robot Operating System/Lecture-01.md](02-高级机器人操作系统/Lecture-01.md) |
| 2 | **工业与嵌入式机器人**（ROS-I、嵌入式、Docker、Gazebo / Isaac Sim） | [Industrial and Embedded Robotics/Lecture-01.md](03-工业与嵌入式机器人/Lecture-01.md) |
| 3 | **面向机器人的高级感知与 AI**（应用型 ROS 2 感知环路、深度学习感知、机器人学习） | [Advanced Perception and AI for Robotics/Lecture-01.md](01-机器人高级感知与AI/Lecture-01.md) |
| 4 | **多机器人系统与集群机器人** | [Multi-Robot Systems and Swarm Robotics/Lecture-01.md](04-多机器人与集群机器人/Lecture-01.md) |
| ★ | **特别课程 — VLA（视觉-语言-动作模型）优化与动作精度一致性 harness**（在真实机器人上部署并验证 Vision-Language-Action 策略） | [VLA Optimization and Action-Parity Harness/README.md](05-VLA优化与动作对齐测试/README.md) |

每个 `Lecture-01.md` 都是一堂 **完整讲座**：学习目标、逐步讲解的概念章节、必要时给出的表格/图、**自检**问题，以及指向上一/下一路线的链接。

**扩展时间线（18-36 个月）：** 讲座 3–4 包含 **扩展** 主题，这些主题在本文件较早版本中曾标记为“阶段 2（大幅扩展）”（深度感知、机器人学习、多机器人 / 集群）。所有这些内容现在都放在路线 3 和 4 的 **Lecture-01.md** 中。

---

**推荐课程（线上）**


<details>
<summary>English original</summary>

**Phase 5 — Track D: Robotics (12-18 months)**

<div class="course-identity robotics" markdown="1">
<div class="course-identity__icon">ROS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track 5D · Robotics</p>
<p class="course-identity__title">Integrate perception, planning, ROS 2, real-time constraints, and embodied AI systems.</p>
<p class="course-identity__meta">Artifact: robot subsystem demo · Measure: loop rate, latency, safety, robustness</p>
</div>
</div>


This phase is organized into **four lecture tracks** (each has its own folder and `Lecture-01.md`). Start with **Advanced Robot Operating System**, then proceed in order or jump by topic.

| # | Track | Folder / lecture |
|---|--------|------------------|
| 1 | **Advanced Robot Operating System** (ROS / ROS 2, Nav2, SLAM, MoveIt 2) | [Advanced Robot Operating System/Lecture-01.md](02-高级机器人操作系统/Lecture-01.md) |
| 2 | **Industrial and Embedded Robotics** (ROS-I, embedded, Docker, Gazebo / Isaac Sim) | [Industrial and Embedded Robotics/Lecture-01.md](03-工业与嵌入式机器人/Lecture-01.md) |
| 3 | **Advanced Perception and AI for Robotics** (applied ROS 2 perception loop, deep learning perception, robot learning) | [Advanced Perception and AI for Robotics/Lecture-01.md](01-机器人高级感知与AI/Lecture-01.md) |
| 4 | **Multi-Robot Systems and Swarm Robotics** | [Multi-Robot Systems and Swarm Robotics/Lecture-01.md](04-多机器人与集群机器人/Lecture-01.md) |
| ★ | **Special course — VLA Optimization and Action-Parity Harness** (deploy and validate Vision-Language-Action policies on real robots) | [VLA Optimization and Action-Parity Harness/README.md](05-VLA优化与动作对齐测试/README.md) |

Each `Lecture-01.md` is a **full lecture**: learning objectives, worked conceptual sections, tables/diagrams where helpful, **self-check** questions, and links to the next/previous track.

**Extended timeline (18-36 months):** Lectures 3–4 include the **expanded** topics that were previously labeled “Phase 2 (Significantly Expanded)” in older versions of this file (deep perception, robot learning, multi-robot / swarm). All of that material now lives in **Lecture-01.md** for tracks 3 and 4.

---

**Recommended courses (online)**

</details>

Use these **structured courses** alongside each lecture’s **Recommended courses** section. Official ROS 2 and vendor docs stay the source of truth; courses help when you want guided projects, browser-based simulators (e.g. The Construct), or university-style theory. Full catalog (The Construct): [Robotics & ROS courses](https://www.theconstruct.ai/robotigniteacademy_learnros/ros-courses-library/).

* **§1 — ROS 2 核心、Nav2、MoveIt (对应 [Lecture 1](02-高级机器人操作系统/Lecture-01.md))**
    * [ROS 2 Basics in Python](https://app.theconstruct.ai/courses/ros2-basics-in-5-days-v2-python-268/) 或 [ROS 2 Basics in C++](https://app.theconstruct.ai/courses/ros2-basics-in-5-days-c-325/) — 先选定一门语言，之后再加另一门。
    * [TF ROS 2](https://app.theconstruct.ai/courses/tf-ros2-217/) — 坐标系与 `robot_state_publisher` 模式。
    * [Intermediate ROS 2](https://app.theconstruct.ai/courses/intermediate-ros2-113/) — launch、参数、QoS、lifecycle。
    * [ROS 2 Navigation](https://app.theconstruct.ai/courses/ros2-navigation-galactic-109/) 与 [Advanced ROS 2 Navigation](https://app.theconstruct.ai/courses/advanced-ros2-navigation-116/) — Nav2 风格的栈。
    * [ROS 2 Manipulation Basics](https://app.theconstruct.ai/courses/ros2-manipulation-basics-81/) 或 [ROS 2 Manipulation & Perception](https://app.theconstruct.ai/courses/ros2-manipulation-perception-master-103/) — MoveIt 2 风格的操作。
    * [Robotics Specialization](https://www.coursera.org/specializations/robotics)（University of Pennsylvania）— 空中机器人、规划、感知、机动性、capstone；数学与控制方面很强（作业常用 MATLAB；不针对特定 ROS 版本）。


<details>
<summary>English original</summary>

Use these **structured courses** alongside each lecture’s **Recommended courses** section. Official ROS 2 and vendor docs stay the source of truth; courses help when you want guided projects, browser-based simulators (e.g. The Construct), or university-style theory. Full catalog (The Construct): [Robotics & ROS courses](https://www.theconstruct.ai/robotigniteacademy_learnros/ros-courses-library/).

* **§1 — ROS 2 core, Nav2, MoveIt (maps to [Lecture 1](02-高级机器人操作系统/Lecture-01.md))**
    * [ROS 2 Basics in Python](https://app.theconstruct.ai/courses/ros2-basics-in-5-days-v2-python-268/) or [ROS 2 Basics in C++](https://app.theconstruct.ai/courses/ros2-basics-in-5-days-c-325/) — pick one language first, then add the other.
    * [TF ROS 2](https://app.theconstruct.ai/courses/tf-ros2-217/) — frames and `robot_state_publisher` patterns.
    * [Intermediate ROS 2](https://app.theconstruct.ai/courses/intermediate-ros2-113/) — launch, parameters, QoS, lifecycle.
    * [ROS 2 Navigation](https://app.theconstruct.ai/courses/ros2-navigation-galactic-109/) and [Advanced ROS 2 Navigation](https://app.theconstruct.ai/courses/advanced-ros2-navigation-116/) — Nav2-style stacks.
    * [ROS 2 Manipulation Basics](https://app.theconstruct.ai/courses/ros2-manipulation-basics-81/) or [ROS 2 Manipulation & Perception](https://app.theconstruct.ai/courses/ros2-manipulation-perception-master-103/) — MoveIt 2–style manipulation.
    * [Robotics Specialization](https://www.coursera.org/specializations/robotics) (University of Pennsylvania) — aerial robotics, planning, perception, mobility, capstone; strong on math and control (often MATLAB in assignments; not ROS-version-specific).

</details>

* **§2 — 仿真、嵌入式 Linux、Docker、工业场景（对应 [Lecture 2](03-工业与嵌入式机器人/Lecture-01.md)）**
    * [Gazebo Sim 入门（配合 ROS 2）](https://app.theconstruct.ai/courses/introduction-to-gazebo-ignition-with-ros2-170/) 与 [Mastering Gazebo Simulator](https://app.theconstruct.ai/courses/mastering-gazebo-simulator-78/) — Gazebo Sim + worlds/models。
    * [面向机器人的 Docker](https://app.theconstruct.ai/courses/docker-basics-for-robotics-114/) — 容器化开发与部署。
    * [面向机器人的 Linux](https://app.theconstruct.ai/courses/linux-for-robotics-noetic-185/) — shell、权限、工作流（标题中是 ROS 1；技能可直接迁移）。
    * [面向机器人的 C++](https://app.theconstruct.ai/courses/c-for-robotics-59/) / [面向机器人的 Python 3](https://app.theconstruct.ai/courses/python-3-for-robotics-58/) — ROS 2 节点之前的语言准备。
    * NVIDIA [Isaac Sim 文档](https://docs.omniverse.nvidia.com/isaacsim/latest/index.html) — 教程；在与 ROS 2 工作对接时参见 [ROS 2 教程](https://docs.omniverse.nvidia.com/isaacsim/latest/ros2_tutorials/index.html)。


<details>
<summary>English original</summary>

* **§2 — Simulation, embedded Linux, Docker, industrial context (maps to [Lecture 2](03-工业与嵌入式机器人/Lecture-01.md))**
    * [Introduction to Gazebo Sim with ROS 2](https://app.theconstruct.ai/courses/introduction-to-gazebo-ignition-with-ros2-170/) and [Mastering Gazebo Simulator](https://app.theconstruct.ai/courses/mastering-gazebo-simulator-78/) — Gazebo Sim + worlds/models.
    * [Docker for Robotics](https://app.theconstruct.ai/courses/docker-basics-for-robotics-114/) — containerized dev and deploy.
    * [Linux for Robotics](https://app.theconstruct.ai/courses/linux-for-robotics-noetic-185/) — shell, permissions, workflows (ROS 1 in the title; skills transfer directly).
    * [C++ for Robotics](https://app.theconstruct.ai/courses/c-for-robotics-59/) / [Python 3 for Robotics](https://app.theconstruct.ai/courses/python-3-for-robotics-58/) — language prep before ROS 2 nodes.
    * Nvidia [Isaac Sim documentation](https://docs.omniverse.nvidia.com/isaacsim/latest/index.html) — tutorials; see [ROS 2 tutorials](https://docs.omniverse.nvidia.com/isaacsim/latest/ros2_tutorials/index.html) when bridging to ROS 2 work.

</details>

* **§3 — 应用感知、跟踪、控制、无人机（对应 [Lecture 3](01-机器人高级感知与AI/Lecture-01.md)，Part A）**
    * [Kalman Filters](https://app.theconstruct.ai/courses/kalman-filters-52/) — 与移动机器人相关的估计直觉。
    * [ROS 2 Perception](https://app.theconstruct.ai/courses/ros-2-perception-in-5-days-239/) — ROS 2 中的传感器与感知栈。
    * [Behavior Trees for ROS 2](https://app.theconstruct.ai/courses/behavior-trees-for-ros2-131/) — 与 Nav2 / 高层行为相关。
    * [ROS 2 Control Framework](https://app.theconstruct.ai/courses/ros-2-control-framework-jazzy-404/) — `ros2_control` 与硬件接口。
    * [Programming Drones with ROS](https://app.theconstruct.ai/courses/programming-drones-with-ros-24/) — 补充文档中的 PX4/MAVLink 自学内容。
    * [State Estimation and Localization for Self-Driving Cars](https://www.coursera.org/learn/state-estimation-localization-self-driving-cars) (Coursera) — Kalman、EKF、UKF、粒子滤波（可迁移到跟踪与融合）。
    * ETH Zürich [Autonomous Mobile Robots](https://www.edx.org/learn/autonomous-robotics/eth-zurich-autonomous-mobile-robots) (edX) — 运动学、定位、建图、规划（理论；可配合 ROS 2 项目）。
    * ETH RSL [Programming for Robotics (ROS)](https://rsl.ethz.ch/education-students/lectures/ros.html) — 官方 ROS 2 课程材料（大纲/阅读材料免费；校内课程；可作为教学大纲配合自己的实验）。


<details>
<summary>English original</summary>

* **§3 — Applied perception, tracking, control, drones (maps to [Lecture 3](01-机器人高级感知与AI/Lecture-01.md), Part A)**
    * [Kalman Filters](https://app.theconstruct.ai/courses/kalman-filters-52/) — estimation intuition tied to mobile robots.
    * [ROS 2 Perception](https://app.theconstruct.ai/courses/ros-2-perception-in-5-days-239/) — sensors and perception stack in ROS 2.
    * [Behavior Trees for ROS 2](https://app.theconstruct.ai/courses/behavior-trees-for-ros2-131/) — ties to Nav2 / high-level behaviors.
    * [ROS 2 Control Framework](https://app.theconstruct.ai/courses/ros-2-control-framework-jazzy-404/) — `ros2_control` and hardware interfaces.
    * [Programming Drones with ROS](https://app.theconstruct.ai/courses/programming-drones-with-ros-24/) — complements PX4/MAVLink self-study from docs.
    * [State Estimation and Localization for Self-Driving Cars](https://www.coursera.org/learn/state-estimation-localization-self-driving-cars) (Coursera) — Kalman, EKF, UKF, particle filters (transferable to tracking and fusion).
    * ETH Zürich [Autonomous Mobile Robots](https://www.edx.org/learn/autonomous-robotics/eth-zurich-autonomous-mobile-robots) (edX) — kinematics, localization, mapping, planning (theory; pair with ROS 2 projects).
    * ETH RSL [Programming for Robotics (ROS)](https://rsl.ethz.ch/education-students/lectures/ros.html) — official ROS 2 lecture materials (free outline/readings; on-campus course; use as a syllabus alongside your own labs).

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track D - Robotics/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20D%20-%20Robotics/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
