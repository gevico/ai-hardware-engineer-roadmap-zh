---
title: ROS 2
description: ROS 2
---

# ROS 2

<div class="course-identity auto-course" style="--course-accent: #ca8a04; --course-accent-rgb: 202, 138, 4;" markdown="1">
<div class="course-identity__icon">ROS</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入探索 · Jetson Track</p>
<p class="course-identity__title">ROS 2 的专门课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成演示 · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


**阶段 4 — 方向 B — 模块 5.6** · 应用开发

> **重点：** 从基础到高级导航、实时安全，以及 **Jetson Orin Nano** 边缘部署，全面掌握 **ROS 2**——让机器人软件达到生产级、确定性的、GPU 加速。

**枢纽：** [5. 应用开发](../Guide.md)

---


## 1. ROS 2 基础

### ROS 2 架构

- **节点、话题、服务、动作：** 掌握 ROS 2 核心通信范式。理解发布-订阅（话题）、请求-响应（服务）以及长时间运行的任务（动作）。
- **DDS（数据分发服务）：** 了解 ROS 2 如何将 DDS 用作中间件。理解用于可靠、实时或尽力而为通信的服务质量（QoS）设置。
- **工作空间与包结构：** 搭建 ROS 2 工作空间，创建包，并用 colcon 构建系统组织机器人软件。

### 用 ROS 2 编程

- **Python 和 C++ 客户端：** 用 Python 和 C++ 编写 ROS 2 节点。理解 `rclpy` 和 `rclcpp` 客户端库。
- **Launch 文件与参数：** 创建 launch 文件以启动多个节点，并为不同部署配置参数。
- **生命周期节点：** 探索托管节点，用于生产系统中的状态受控启动与关闭。

---

## 2. 机器人导航与控制

### 导航栈（Nav2）

- **代价地图与路径规划：** 学习代价地图生成、全局规划器（NavFn、Smac Planner）以及用于避障的局部规划器。
- **恢复行为：** 当机器人卡住或丢失路径时实现恢复行为。
- **多机器人协同：** 探索用 Nav2 在共享环境中协调多个机器人。

### 传感器集成

- **相机与 LiDAR（激光雷达）驱动：** 将相机和 LiDAR 传感器与 ROS 2 集成。使用 `sensor_msgs` 和点云处理。
- **TF2 与机器人状态：** 掌握用 TF2 进行坐标变换和机器人状态发布（关节状态、里程计）。

---

## 3. Jetson 上的边缘部署

### Jetson Orin Nano 上的 ROS 2

- **交叉编译与原生构建：** 为 Jetson 构建 ROS 2 包。针对 ARM 架构和 GPU 加速进行优化。
- **实时性能：** 调优 ROS 2 和 DDS，以在嵌入式硬件上实现低延迟、确定性的行为。
- **容器化：** 使用 Docker / ROS 2 容器在 Jetson 上实现可复现部署。

### AI 与 ROS 2 集成

- **TensorRT 与 ROS 2：** 在 ROS 2 节点中运行 TensorRT 优化的模型，用于感知（目标检测、分割）。
- **基于 ROS 2 的传感器融合：** 在 ROS 2 流水线中融合相机、IMU 和其他传感器数据，实现稳健的感知。

---

## 4. 高级 ROS 2 开发

### 自定义接口与中间件

- **自定义消息与服务：** 设计特定领域的 ROS 2 消息类型（`.msg`、`.srv`、`.action`），配以合适的字段类型和序列化。理解接口变更如何影响下游包。
- **DDS QoS 调优：** 配置 DDS 服务质量（QoS）策略——可靠性（RELIABLE 与 BEST_EFFORT）、持久性、历史深度、截止时间和生存期——以匹配应用对延迟、带宽和可靠性的要求。
- **自定义 DDS 中间件：** 替换 DDS 实现（FastDDS、CycloneDDS、Connext DDS），并针对特定用例（实时、高吞吐或低功耗嵌入式系统）调优中间件配置。

### ROS 2 执行器与并发

- **单线程与多线程执行器：** 理解执行器选择对回调调度、延迟和线程安全的影响。实现回调组以实现细粒度的并发控制。
- **自定义执行器：** 针对专门的调度需求编写自定义执行器——基于优先级的执行、时间触发回调，或实时系统中受 WCET 约束的执行。
- **进程内通信：** 为运行在同一进程中的节点启用零拷贝进程内通信，大幅降低高带宽数据（相机、LiDAR）的延迟和内存拷贝。


<details>
<summary>English original</summary>

**ROS 2**

<div class="course-identity auto-course" style="--course-accent: #ca8a04; --course-accent-rgb: 202, 138, 4;" markdown="1">
<div class="course-identity__icon">ROS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for ROS 2.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


**Phase 4 — Track B — Module 5.6** · Application Development

> **Focus:** Master **ROS 2** from fundamentals through advanced navigation, real-time safety, and **Jetson Orin Nano** edge deployment—so your robot software is production-grade, deterministic, and GPU-accelerated.

**Hub:** [5. Application Development](../Guide.md)

---


**1. ROS 2 fundamentals**

**ROS 2 architecture**

- **Nodes, Topics, Services, Actions:** Master the core ROS 2 communication paradigms. Understand publish-subscribe (topics), request-response (services), and long-running tasks (actions).
- **DDS (Data Distribution Service):** Learn how ROS 2 uses DDS as its middleware. Understand quality of service (QoS) settings for reliable, real-time, or best-effort communication.
- **Workspace and Package Structure:** Set up ROS 2 workspaces, create packages, and organize your robot software with the colcon build system.

**Programming with ROS 2**

- **Python and C++ Clients:** Write ROS 2 nodes in both Python and C++. Understand the `rclpy` and `rclcpp` client libraries.
- **Launch Files and Parameters:** Create launch files to start multiple nodes and configure parameters for different deployments.
- **Lifecycle Nodes:** Explore managed nodes for state-controlled startup and shutdown in production systems.

---

**2. Robot navigation and control**

**Navigation Stack (Nav2)**

- **Costmaps and Path Planning:** Learn costmap generation, global planners (NavFn, Smac Planner), and local planners for obstacle avoidance.
- **Recovery Behaviors:** Implement recovery behaviors when the robot gets stuck or loses its path.
- **Multi-Robot Coordination:** Explore Nav2 for coordinating multiple robots in shared environments.

**Sensor integration**

- **Camera and LiDAR Drivers:** Integrate camera and LiDAR sensors with ROS 2. Use `sensor_msgs` and point cloud processing.
- **TF2 and Robot State:** Master TF2 for coordinate transforms and robot state publishing (joint states, odometry).

---

**3. Edge deployment on Jetson**

**ROS 2 on Jetson Orin Nano**

- **Cross-Compilation and Native Build:** Build ROS 2 packages for Jetson. Optimize for ARM architecture and GPU acceleration.
- **Real-Time Performance:** Tune ROS 2 and DDS for low-latency, deterministic behavior on embedded hardware.
- **Containerization:** Use Docker / ROS 2 containers for reproducible deployment on Jetson.

**AI and ROS 2 integration**

- **TensorRT and ROS 2:** Run TensorRT-optimized models in ROS 2 nodes for perception (object detection, segmentation).
- **Sensor Fusion with ROS 2:** Combine camera, IMU, and other sensor data in ROS 2 pipelines for robust perception.

---

**4. Advanced ROS 2 development**

**Custom interfaces and middleware**

- **Custom Messages and Services:** Design domain-specific ROS 2 message types (`.msg`, `.srv`, `.action`) with appropriate field types and serialization. Understand how interface changes affect downstream packages.
- **DDS QoS Tuning:** Configure DDS Quality of Service (QoS) policies—reliability (RELIABLE vs. BEST_EFFORT), durability, history depth, deadline, and lifespan—to match application requirements for latency, bandwidth, and reliability.
- **Custom DDS Middleware:** Swap DDS implementations (FastDDS, CycloneDDS, Connext DDS) and tune middleware configuration for specific use cases (real-time, high throughput, or low-power embedded systems).

**ROS 2 executors and concurrency**

- **Single-Threaded vs. Multi-Threaded Executors:** Understand the implications of executor choice on callback scheduling, latency, and thread safety. Implement callback groups for fine-grained concurrency control.
- **Custom Executors:** Write custom executors for specialized scheduling requirements—priority-based execution, time-triggered callbacks, or WCET-bounded execution for real-time systems.
- **Intra-Process Communication:** Enable zero-copy intra-process communication for nodes running in the same process, dramatically reducing latency and memory copies for high-bandwidth data (cameras, LiDAR).

</details>

### ROS 2 组件架构

- **可组合节点：** 将 ROS 2 节点重构为可组合组件，使其能运行在共享进程（组件容器）中，以降低开销并实现进程内通信。
- **受管生命周期节点：** 实现生命周期节点（`rclcpp_lifecycle`）用于生产级状态管理——configure、activate、deactivate 和 cleanup 转换，以实现确定性的启动与关闭。
- **基于插件的架构：** 用 `pluginlib` 定义插件接口（例如用于规划器、控制器、滤波器），并在 runtime 动态加载实现，无需重新编译。

---

## 5. 高级导航、规划与行为

### Nav2 深入剖析

- **自定义规划器与控制器：** 实现 Nav2 插件接口，创建自定义全局规划器（例如带运动学约束的 hybrid A*）与局部控制器（例如 MPPI——Model Predictive Path Integral）。
- **行为树：** 掌握 BehaviorTree.CPP 与 Nav2 的行为树集成。为复杂的自主行为设计分层行为树——探索、对接、多目标导航。
- **动态障碍物避让：** 使用激光雷达或摄像头检测结果，把动态障碍物 layer 集成到 costmap 中。用速度障碍法（VO/RVO）实现预测式避障。

### SLAM 与定位

- **高级 SLAM：** 超越 2D SLAM，使用 LIO-SAM、LOAM 或 RTAB-Map 等包配合激光雷达或 RGB-D 相机做 3D SLAM。理解回环检测与位姿图优化。
- **多会话与多地图：** 实现多会话 SLAM，以跨断电周期持久建图。使用地图合并包合并来自多个会话或多台机器人的地图。
- **语义 SLAM：** 将几何 SLAM 与语义理解结合——把目标检测集成到地图中，用于语义地点识别与定向导航。

### 任务与使命规划

- **基于 BT 的使命规划：** 用行为树实现高层使命规划——逐房间清扫、巡检路线、带取放序列的物品配送。
- **多机器人系统中的任务分配：** 为多机器人系统实现基于拍卖或基于优化的任务分配。用 ROS 2 service 或 action server 进行任务分派与状态跟踪。
- **ROS 2 与 PlanSys2：** 用 PlanSys2（ROS 2 中基于 PDDL 的规划）做带前置条件与效果的形式化任务规划，从而对机器人能力与世界状态进行高层推理。

---

## 6. 实时与安全关键的 ROS 2

### 配合 ROS 2 的实时 Linux

- **PREEMPT_RT 内核：** 构建并配置带 PREEMPT_RT 补丁的 Linux 内核，实现完全可抢占执行。用 `cyclictest` 测量 ROS 2 控制环的延迟改善。
- **CPU 隔离与 IRQ 亲和性：** 用 `isolcpus` 为实时 ROS 2 节点隔离 CPU 核，并配置 IRQ 亲和性，避免中断在实时核上引发抖动。
- **内存锁定：** 用 `mlockall()` 锁定进程内存页，避免缺页异常在实时节点中造成延迟尖峰。

### 面向安全关键应用的 ROS 2

- **ROS 2 安全认证：** 理解 ROS 2 安全认证的格局——Apex.AI 的 Apex.OS（ASIL-D 认证）、通过安全认证的 DDS，以及形式化验证方法。
- **看门狗与故障检测：** 实现系统级看门狗节点，监控 topic 心跳、检测静默节点失效并触发安全状态行为。
- **确定性执行：** 以确定性时序设计 ROS 2 系统——固定频率发布者、deadline QoS 与定时器驱动的执行，以支持 WCET 分析。

### ROS 2 测试与 CI/CD

- **ROS 2 测试框架：** 用 `ros_testing` 配合 `launch_pytest` 为 ROS 2 节点编写单元测试。用参数化 launch 文件测试节点在各种条件下的行为。
- **集成测试：** 设计集成测试，启动完整的多节点系统并验证端到端行为（例如从感知到控制的流水线）。
- **配合 ROS 2 的 CI/CD：** 搭建 GitHub Actions 或 Jenkins 流水线，对 ROS 2 包做自动构建、lint 与测试。与 ROS 2 Industrial CI 集成以实现跨平台构建。

---

## 7. 项目

### 基础

- **用 Nav2 实现机器人导航：** 构建一台自主移动机器人，用 Nav2 在已知环境中导航，包含 costmap 与路径规划。
- **多机器人系统：** 创建包含多台机器人（真实或仿真）的系统，通过 ROS 2 topic 与 service 协同。
- **边缘 AI 机器人：** 在 Jetson Orin Nano 上部署基于 ROS 2 的机器人，采用 TensorRT 加速的感知与 Nav2 导航。


<details>
<summary>English original</summary>

**ROS 2 component architecture**

- **Composable Nodes:** Refactor ROS 2 nodes as composable components that can run in a shared process (component container) for lower overhead and intra-process communication.
- **Managed Lifecycle Nodes:** Implement lifecycle nodes (`rclcpp_lifecycle`) for production-grade state management—configure, activate, deactivate, and cleanup transitions for deterministic startup and shutdown.
- **Plugin-Based Architecture:** Use `pluginlib` to define plugin interfaces (e.g., for planners, controllers, filters) and dynamically load implementations at runtime without recompilation.

---

**5. Advanced navigation, planning, and behavior**

**Nav2 deep dive**

- **Custom Planners and Controllers:** Implement Nav2 plugin interfaces to create custom global planners (e.g., hybrid A* with kinematic constraints) and local controllers (e.g., MPPI — Model Predictive Path Integral).
- **Behavior Trees:** Master BehaviorTree.CPP and Nav2's behavior tree integration. Design hierarchical behavior trees for complex autonomous behaviors—exploration, docking, multi-goal navigation.
- **Dynamic Obstacle Avoidance:** Integrate dynamic obstacle layers into costmaps using LiDAR or camera detections. Implement predictive obstacle avoidance with velocity obstacles (VO/RVO).

**SLAM and localization**

- **Advanced SLAM:** Go beyond 2D SLAM to 3D SLAM using packages like LIO-SAM, LOAM, or RTAB-Map with LiDAR or RGB-D cameras. Understand loop closure detection and pose graph optimization.
- **Multi-Session and Multi-Map:** Implement multi-session SLAM for persistent mapping across power cycles. Merge maps from multiple sessions or robots using map merging packages.
- **Semantic SLAM:** Combine geometric SLAM with semantic understanding—object detection integrated into the map for semantic place recognition and targeted navigation.

**Task and mission planning**

- **BT-Based Mission Planning:** Use behavior trees to implement high-level mission planning—room-by-room cleaning, inspection routes, item delivery with pick-up and drop-off sequences.
- **Task Allocation in Multi-Robot Systems:** Implement auction-based or optimization-based task allocation for multi-robot systems. Use ROS 2 services or action servers for task assignment and status tracking.
- **ROS 2 with PlanSys2:** Use PlanSys2 (PDDL-based planning in ROS 2) for formal task planning with preconditions and effects, enabling high-level reasoning about robot capabilities and world state.

---

**6. Real-time and safety-critical ROS 2**

**Real-time Linux with ROS 2**

- **PREEMPT_RT Kernel:** Build and configure a Linux kernel with the PREEMPT_RT patch for fully preemptible execution. Measure latency improvements for ROS 2 control loops with `cyclictest`.
- **CPU Isolation and IRQ Affinity:** Isolate CPU cores for real-time ROS 2 nodes using `isolcpus` and configure IRQ affinity to prevent interrupt-induced jitter on real-time cores.
- **Memory Locking:** Use `mlockall()` to lock process memory pages, preventing page faults from causing latency spikes in real-time nodes.

**ROS 2 for safety-critical applications**

- **ROS 2 Safety Certification:** Understand the landscape of safety certification for ROS 2—Apex.AI's Apex.OS (ASIL-D certified), safety-certified DDS, and formal verification approaches.
- **Watchdog and Fault Detection:** Implement system-level watchdog nodes that monitor topic heartbeats, detect silent node failures, and trigger safe-state behaviors.
- **Deterministic Execution:** Design ROS 2 systems with deterministic timing—fixed-rate publishers, deadline QoS, and timer-driven execution to enable WCET analysis.

**ROS 2 testing and CI/CD**

- **ROS 2 Test Framework:** Write unit tests for ROS 2 nodes using `ros_testing` with `launch_pytest`. Test node behavior under various conditions using parameterized launch files.
- **Integration Testing:** Design integration tests that spin up complete multi-node systems and validate end-to-end behavior (e.g., a perception-to-control pipeline).
- **CI/CD with ROS 2:** Set up GitHub Actions or Jenkins pipelines for automated building, linting, and testing of ROS 2 packages. Integrate with ROS 2 Industrial CI for cross-platform builds.

---

**7. Projects**

**Fundamentals**

- **Robot Navigation with Nav2:** Build an autonomous mobile robot that navigates in a known environment using Nav2, with costmaps and path planning.
- **Multi-Robot System:** Create a system with multiple robots (real or simulated) that coordinate via ROS 2 topics and services.
- **Edge AI Robot:** Deploy a ROS 2-based robot on Jetson Orin Nano with TensorRT-accelerated perception and Nav2 for navigation.

</details>

### 进阶

- **可组合节点流水线：** 将多节点感知流水线（相机驱动 → 预处理 → 检测器 → 跟踪器）重构为单容器内采用进程内通信的可组合节点。
- **实时执行器：** 为控制环节点实现基于优先级的自定义执行器，测量有实时调度与无实时调度下的回调抖动。
- **QoS Benchmark：** 在 CPU 负载下，对比高频传感器 topic 上 RELIABLE 与 BEST_EFFORT QoS 的延迟与丢包。

### 导航与规划

- **自定义 Nav2 控制器插件：** 将 MPPI 控制器实现为 Nav2 控制器插件。以 DWB 为对照，benchmark 轨迹质量与避障表现。
- **3D LiDAR（激光雷达）SLAM：** 在 Jetson Orin Nano 上用 3D 激光雷达运行 LIO-SAM 或 LOAM，构建带回环闭合的室内环境 3D 地图。
- **多机器人任务分配：** 为 Gazebo 中的 3 个仿真机器人实现集中式任务分配器，通过 ROS 2 action 客户端下发导航目标并跟踪完成情况。

### 实时与安全

- **实时控制环：** 在 PREEMPT_RT 内核上的 ROS 2 节点中实现 1 kHz 控制环。测量并记录有 CPU 隔离与无 CPU 隔离下的抖动。
- **容错机器人系统：** 构建带看门狗节点的 ROS 2 系统，该节点检测传感器节点故障，并使机器人转入安全停止行为。
- **完整 CI/CD 流水线：** 为 ROS 2 package 搭建 GitHub Actions 流水线，在 Docker 容器中执行 colcon build、单元测试与集成测试。

---

## 8. 资源

| 主题 | 资源 |
|-------|----------|
| **官方文档** | [ROS 2 Documentation](https://docs.ros.org/) — 教程与 API 参考 |
| **Nav2** | [Navigation 2](https://navigation.ros.org/) — 导航栈指南 |
| **NVIDIA + ROS 2** | 在 Jetson 平台上运行 ROS 2 的 NVIDIA 指南 |
| **ROS 2 设计** | [ROS 2 Design](https://design.ros2.org/) — 架构决策、DDS 集成、QoS 依据 |
| **实时 ROS 2** | "A Systematic Approach to Real-Time ROS 2"（Apex.AI）— RT 系统上的确定性 ROS 2 |
| **FastDDS** | eProsima FastDDS 配置参考（ROS 2 默认中间件） |
| **Nav2 理论** | *Behavior Trees in Robotics and AI* — Colledanchise 与 Ögren |
| **SLAM 理论** | *Probabilistic Robotics* — Thrun、Burgard 与 Fox |
| **测试** | `ros_testing` — 用于节点单元测试与集成测试的官方 ROS 2 测试框架 |


<details>
<summary>English original</summary>

**Advanced**

- **Composable Node Pipeline:** Refactor a multi-node perception pipeline (camera driver → preprocessing → detector → tracker) into composable nodes in a single container with intra-process communication.
- **Real-Time Executor:** Implement a priority-based custom executor for a control loop node and measure callback jitter with and without real-time scheduling.
- **QoS Benchmark:** Compare latency and packet loss for RELIABLE vs. BEST_EFFORT QoS on a high-frequency sensor topic under CPU load.

**Navigation and planning**

- **Custom Nav2 Controller Plugin:** Implement an MPPI controller as a Nav2 controller plugin. Benchmark trajectory quality and obstacle avoidance against DWB.
- **3D LiDAR SLAM:** Run LIO-SAM or LOAM on a Jetson Orin Nano with a 3D LiDAR, building a 3D map of an indoor environment with loop closure.
- **Multi-Robot Task Allocation:** Implement a centralized task allocator for 3 simulated robots in Gazebo, assigning navigation goals via ROS 2 action clients and tracking completion.

**Real-time and safety**

- **Real-Time Control Loop:** Implement a 1 kHz control loop in a ROS 2 node on a PREEMPT_RT kernel. Measure and document jitter with and without CPU isolation.
- **Fault-Tolerant Robot System:** Build a ROS 2 system with a watchdog node that detects sensor node failures and transitions the robot to a safe stop behavior.
- **Full CI/CD Pipeline:** Set up a GitHub Actions pipeline for a ROS 2 package with colcon build, unit tests, and integration tests running in a Docker container.

---

**8. Resources**

| Topic | Resource |
|-------|----------|
| **Official docs** | [ROS 2 Documentation](https://docs.ros.org/) — tutorials and API references |
| **Nav2** | [Navigation 2](https://navigation.ros.org/) — navigation stack guide |
| **NVIDIA + ROS 2** | NVIDIA guides for running ROS 2 on Jetson platforms |
| **ROS 2 design** | [ROS 2 Design](https://design.ros2.org/) — architecture decisions, DDS integration, QoS rationale |
| **Real-time ROS 2** | "A Systematic Approach to Real-Time ROS 2" (Apex.AI) — deterministic ROS 2 on RT systems |
| **FastDDS** | eProsima FastDDS configuration reference (default ROS 2 middleware) |
| **Nav2 theory** | *Behavior Trees in Robotics and AI* — Colledanchise and Ögren |
| **SLAM theory** | *Probabilistic Robotics* — Thrun, Burgard, and Fox |
| **Testing** | `ros_testing` — official ROS 2 testing framework for node unit and integration tests |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/6. ROS2/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/6.%20ROS2/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
