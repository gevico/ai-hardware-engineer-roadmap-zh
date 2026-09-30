---
title: 第 3 讲：面向机器人的高级感知与 AI
description: 第 3 讲：面向机器人的高级感知与 AI
---

# 第 3 讲：面向机器人的高级感知与 AI

## 概述

本讲包含**三个部分**：

* **Part A — 应用感知、跟踪与控制（ROS 2）：** 用标准消息、估计器与中间件，把从**像素**到**执行**的闭环打通——这正是许多**应用研究 / 感知工程师**岗位所要求的。
* **Part B — 高级感知：** 面向操作与导航的深度学习 **2D/3D** 感知、**语义**、**VIO** 与**多模态**传感。
* **Part C — 机器人学习：** **RL**、**模仿学习**、**基础模型**与**全身 / 足式**控制——学习技术栈如何位于经典 ROS 2 技术栈的**上方**或**旁侧**。

**读完本讲后，你应该能够：**

* 画出 detector → tracker → planner/controller 的 **ROS 2 图**，并配上 **`vision_msgs`** 与 **TF2**。
* 在系统层面解释多目标跟踪中的 **Kalman 预测 + 关联**。
* 说明学习式深度**何时**失效、而**几何方法**（双目、structure-from-motion）仍然重要。
* 描述用于 **RL** 策略的 **sim-to-real** 手段（随机化、延迟、校准）。
* 把 **LLM / VLM** 规划器定位为 **ROS 2** 原语之上的**高层**监督者。

---

## Part A — 应用感知、跟踪与控制（以 ROS 2 为重点）

本部分把 **detection → tracking → estimation → actuation** 映射到 **ROS 2** 原语：node、topic、QoS、launch、`ros2 bag`，以及通往**非 ROS** 服务的 bridge。

### A.1 ROS 图中的检测

**目标：**把**相机**或 **LiDAR（激光雷达）**数据流变成供下游 node 使用的**稳定、带类型的消息**。

| 组件 | 作用 |
|-------|------|
| `sensor_msgs/Image` | 原始图像（常用 `image_transport` 压缩） |
| `cv_bridge` | 在 ROS 图像 ↔ OpenCV 之间转换，不出现拷贝错误 |
| `vision_msgs/Detection2D` / `Detection3D` | 标准 bounding box + class id；用 **message_filters** 做同步 |

**部署路径：**在 PyTorch 中训练 → 导出 **ONNX** → 在 Jetson 上跑 **TensorRT** → 一个只做推理并发布结果的轻量 ROS 2 node。

### A.2 多目标跟踪（MOT）

**核心循环：**用 **Kalman**（或匀速）模型对每条 track 做**预测** → 把 detection 与 track **关联**（Hungarian / IoU / Mahalanobis gating）→ **新建** / **删除** track。

**学习辅助**的 tracker（如 **ByteTrack** 风格）在 detection 抖动时提升**关联**的鲁棒性。在 ROS 2 中，发布 **track ID** 和 **marker** 给 RViz2，使调试可视化。

**路线图深入：**[阶段 3 — Multi-Object Tracking guide](../../../阶段3-人工智能/03-硬件与边缘AI/02-传感器融合/04-multi-object-tracking/Guide.md)（Kalman + 关联分配 + ROS 2 模式）。

### A.3 状态估计与 `robot_localization`

与第 1 讲相同的数学，应用于**感知驱动**的系统：融合 **wheel odom**、**IMU**、**visual odometry** 或 **GPS**（室外）。**TF2** 必须保持**一致**——错误的 `odom` → `base_link` 会同时破坏跟踪与 Nav2。

### A.4 语义层与行为树

Nav2 已经在用 **BehaviorTree.CPP**。对于**语义**目标（“只在标注的地面上行驶”），把 **segmentation** 结果送入 **costmap plugin** 或按类别标签分支的 **BT condition**。

### A.5 控制与视觉伺服

**视觉伺服：**把**图像特征**（点、线）调节到期望位置；**控制律**输出 **twist** 或**关节速度**。始终通过 **TF2**（`geometry_msgs`）**变换** setpoint，使控制器在**正确的坐标系**下工作。

**空中 / PX4：**使用 [PX4 ↔ ROS 2](https://docs.px4.io/main/en/ros2/user_guide.html)（micro-ROS / uXRCE-DDS），而不是重新造 autopilot；ROS 2 提供 **mission**、**offboard** setpoint 或**感知**钩子。

### A.6 单目几何

**单目**系统缺少绝对尺度；**IMU** 或**已知物体尺寸**提供尺度。**VIO** 包（ORB-SLAM3、OpenVINS、Kimera-VIO）暴露 **ROS** 接口——融合时要将其输出视为**有噪声**且**有速率限制**。

### A.7 仿真与 sim-to-real

* **Gazebo** + [`ros_gz`](https://github.com/gazebosim/ros_gz)：地面机器人、传感器。
* **PX4 SITL：**飞行前验证的空中技术栈。

**Sim-to-real：**在声称硬件就绪之前，先在仿真中注入**延迟**、**噪声**与**错误校准**。

### A.8 服务集成（FastAPI、NATS）

**模式：**ROS 2 负责**准实时**的感知与控制；**FastAPI** 暴露 **REST/WebSocket** 供看板使用；**NATS** 把**事件**扇出到分析系统。用小型 node 做**桥接**；**不要**用阻塞式 HTTP 饿死 DDS 线程。

**QoS：**[DDS tuning](https://docs.ros.org/en/humble/Concepts/About-Quality-of-Service-Settings.html)，针对传感数据流与命令的对比。

### A.9 GPU、Docker、GStreamer

在 Docker 中为 GPU node 使用 **NVIDIA Container Toolkit**。当 `v4l2` 不足以应对**相机接入**时，使用 **`gscam`** 或**流水线垫片**。


<details>
<summary>English original</summary>

**Lecture 3: Advanced Perception and AI for Robotics**

**Overview**

This lecture has **three parts**:

* **Part A — Applied perception, tracking, and control (ROS 2):** Closing the loop from **pixels** to **actuation** with standard messages, estimators, and middleware—what many **applied research / perception engineer** roles require.
* **Part B — Advanced perception:** Deep learning **2D/3D** perception, **semantics**, **VIO**, and **multi-modal** sensing for manipulation and navigation.
* **Part C — Robot learning:** **RL**, **imitation**, **foundation models**, and **whole-body / legged** control—how learning stacks sit **above** or **beside** classical ROS 2 stacks.

**By the end of this lecture you should be able to:**

* Sketch a **ROS 2 graph** for detector → tracker → planner/controller, with **`vision_msgs`** and **TF2**.
* Explain **Kalman prediction + association** for multi-object tracking at a systems level.
* Name **when** learned depth fails and **geometry** (stereo, structure-from-motion) still matters.
* Describe **sim-to-real** levers (randomization, latency, calibration) for **RL** policies.
* Place **LLM / VLM** planners as **high-level** supervisors over **ROS 2** primitives.

---

**Part A — Applied perception, tracking, and control (ROS 2 focus)**

This block maps **detection → tracking → estimation → actuation** to **ROS 2** primitives: nodes, topics, QoS, launch, `ros2 bag`, and bridges to **non-ROS** services.

**A.1 Detection in the ROS graph**

**Goal:** Turn a **camera** or **LiDAR** stream into **stable, typed messages** for downstream nodes.

| Piece | Role |
|-------|------|
| `sensor_msgs/Image` | Raw image (often compressed with `image_transport`) |
| `cv_bridge` | Convert ROS images ↔ OpenCV without copy mistakes |
| `vision_msgs/Detection2D` / `Detection3D` | Standard bounding boxes + class ids; use **message_filters** for sync |

**Deployment path:** Train in PyTorch → export **ONNX** → **TensorRT** on Jetson → thin ROS 2 node that only runs inference and publishes.

**A.2 Multi-object tracking (MOT)**

**Core loop:** **Predict** each track with a **Kalman** (or constant-velocity) model → **associate** detections to tracks (Hungarian / IoU / Mahalanobis gating) → **create** / **delete** tracks.

**Learning-assisted** trackers (e.g. **ByteTrack**-style) add **association** robustness when detections flicker. In ROS 2, publish **track IDs** and **markers** for RViz2 so debugging is visual.

**Roadmap deep dive:** [Phase 3 — Multi-Object Tracking guide](../../../阶段3-人工智能/03-硬件与边缘AI/02-传感器融合/04-multi-object-tracking/Guide.md) (Kalman + assignment + ROS 2 patterns).

**A.3 State estimation and `robot_localization`**

Same mathematics as Lecture 1, applied to **perception-driven** systems: fuse **wheel odom**, **IMU**, **visual odometry**, or **GPS** (outdoor). **TF2** must stay **consistent**—a wrong `odom` → `base_link` corrupts both tracking and Nav2.

**A.4 Semantic layer and behavior trees**

Nav2 already uses **BehaviorTree.CPP**. For **semantic** goals (“only drive on labeled floor”), feed **segmentation** into **costmap plugins** or **BT conditions** that branch on class labels.

**A.5 Control and visual servoing**

**Visual servoing:** Regulate **image features** (points, lines) to desired positions; **control law** outputs **twist** or **joint velocities**. Always **transform** setpoints through **TF2** (`geometry_msgs`) so the controller operates in the **correct frame**.

**Aerial / PX4:** Use [PX4 ↔ ROS 2](https://docs.px4.io/main/en/ros2/user_guide.html) (micro-ROS / uXRCE-DDS) rather than reinventing the autopilot; ROS 2 supplies **missions**, **offboard** setpoints, or **perception** hooks.

**A.6 Monocular geometry**

**Monocular** systems lack absolute scale; **IMU** or **known object size** provides scale. **VIO** packages (ORB-SLAM3, OpenVINS, Kimera-VIO) expose **ROS** interfaces—treat outputs as **noisy** and **rate-limited** for fusion.

**A.7 Simulation and sim-to-real**

* **Gazebo** + [`ros_gz`](https://github.com/gazebosim/ros_gz): ground robots, sensors.
* **PX4 SITL:** aerial stacks before flight.

**Sim-to-real:** Inject **latency**, **noise**, and **misp calibration** in sim before claiming hardware readiness.

**A.8 Service integration (FastAPI, NATS)**

**Pattern:** ROS 2 owns **real-time-ish** sensing and control; **FastAPI** exposes **REST/WebSocket** for dashboards; **NATS** fans out **events** to analytics. **Bridge** with small nodes; **do not** starve the DDS thread with blocking HTTP.

**QoS:** [DDS tuning](https://docs.ros.org/en/humble/Concepts/About-Quality-of-Service-Settings.html) for sensor streams vs commands.

**A.9 GPU, Docker, GStreamer**

**NVIDIA Container Toolkit** for GPU nodes in Docker. **`gscam`** or **pipeline shims** when `v4l2` is not enough for **camera ingest**.

</details>

### 推荐课程（Part A）

* [卡尔曼滤波](https://app.theconstruct.ai/courses/kalman-filters-52/)
* [ROS 2 感知](https://app.theconstruct.ai/courses/ros-2-perception-in-5-days-239/)
* [ROS 2 行为树](https://app.theconstruct.ai/courses/behavior-trees-for-ros2-131/)
* [ROS 2 控制框架](https://app.theconstruct.ai/courses/ros-2-control-framework-jazzy-404/)
* [用 ROS 编程无人机](https://app.theconstruct.ai/courses/programming-drones-with-ros-24/)
* [自动驾驶汽车的状态估计与定位](https://www.coursera.org/learn/state-estimation-localization-self-driving-cars)（Coursera）
* ETH [自主移动机器人](https://www.edx.org/learn/autonomous-robotics/eth-zurich-autonomous-mobile-robots)（edX）
* ETH RSL [机器人编程（ROS）](https://rsl.ethz.ch/education-students/lectures/ros.html)

### 规范链接（Part A）

* [ROS 2 Humble 教程](https://docs.ros.org/en/humble/Tutorials.html) · [QoS](https://docs.ros.org/en/humble/Concepts/About-Quality-of-Service-Settings.html) · [`ros2 bag`](https://docs.ros.org/en/humble/Tutorials/Beginner-CLI-Tools/Recording-And-Playing-Back-Data/Recording-And-Playing-Back-Data.html)
* [Nav2](https://navigation.ros.org/) · [Nav2 中的行为树](https://navigation.ros.org/behavior_trees/index.html)
* [ros2_control](https://control.ros.org/)
* [`vision_msgs`](https://github.com/ros-perception/vision_msgs) · [cv_bridge](https://docs.ros.org/en/humble/Tutorials/Advanced/Cvbridge/Cvbridge.html)
* Barfoot, *State Estimation for Robotics* — [资源页](http://asrl.utias.utoronto.ca/~tdb/bib.html)
* [Gazebo](https://gazebosim.org/docs) · [ros_gz](https://github.com/gazebosim/ros_gz)
* [PX4 ROS 2](https://docs.px4.io/main/en/ros2/user_guide.html) · [MAVSDK](https://mavsdk.mavlink.io/)
* [Docker + ROS 2](https://github.com/osrf/docker_images) · [Docker 操作指南](https://docs.ros.org/en/humble/How-To-Guides/Run-2-nodes-in-single-or-separate-docker-containers.html)
* [FastAPI](https://fastapi.tiangolo.com/) · [NATS](https://docs.nats.io/)

### Part A 如何与其他讲次衔接

| 主题 | 位置 |
|-------|--------|
| Nav2、SLAM、`robot_localization` | [第 1 讲 — 高级 ROS](../02-高级机器人操作系统/Lecture-01.md)、[第 2 讲 — 工业](../03-工业与嵌入式机器人/Lecture-01.md) |
| 多机器人 | [第 4 讲 — 多机器人](../04-多机器人与集群机器人/Lecture-01.md) |

### 项目（Part A）

1. **检测器 → 跟踪器 → RViz2：** `vision_msgs` + 轨迹标记 + `ros2 bag`。
2. **PX4 SITL 或 Gazebo + Nav2：** 比较仿真与硬件上的**命令延迟**。
3. **桥接：** ROS 2 状态 → **FastAPI** + **NATS** 事件；测量端到端延迟。

---

## Part B — 高级感知与 AI（扩展方向）

### B.1 基于深度学习的感知

* **2D 检测：** YOLO 系列、RT-DETR 等——针对 Jetson（TensorRT）上的**延迟**做优化。
* **6D 位姿：** FoundationPose、DenseFusion 风格的方法用于**抓取**——输出必须经 **TF** 送入 **MoveIt 2**，并做碰撞感知的规划。
* **3D 点云：** 经典几何用 **Open3D** / **PCL**；结构化场景中的学习式分割用 **PointNet++** / **voxel** 网络。

### B.2 VIO / SLAM

**VIO** 将 **IMU**（高频率、有偏置）与**相机**（频率较低、信息丰富）融合。失效模式：**运动模糊**、**卷帘快门**、**无纹理**区域。只要可能，应始终与**轮式里程计**或**动捕**对比。

### B.3 语义与场景图

**语义分割**（例如可行驶区域与障碍物）为 **Nav2** 的 costmap 提供输入。**3D 场景图**挂接**物体**与**关系**，用于**任务规划**与 **HRI**（“左边桌子上的杯子”）。

**开放词表**检测器（Grounding DINO、基于 CLIP 的）可减少重训练，但需要在你的机器人上验证**延迟**与 **grounding**。

### B.4 触觉与多模态感知

**触觉**阵列估计**滑动**与**接触**；与**视觉**融合有助于**手内**操作。**音频**可标记**碰撞**或**电机**异常——应视其为发给**监督器**的**异步**线索，而非硬实时控制，除非经过验证。

### 资源（Part B）

* [Open3D](http://www.open3d.org/docs/)
* Siciliano et al., *Robotics: Modelling, Planning and Control*
* Berkeley Robot Sensing（BRS）系列工作（论文）

### 项目（Part B）

* **6D 位姿 + 抓取：** Jetson Orin + 桌面物体 + MoveIt 2。
* **VIO benchmark：** ORB-SLAM3 与真值对比。
* **开放词表拾取放置：** 语言 → 检测 → 抓取。

---

## Part C — 机器人学习与自主行为


<details>
<summary>English original</summary>

**Recommended courses (Part A)**

* [Kalman Filters](https://app.theconstruct.ai/courses/kalman-filters-52/)
* [ROS 2 Perception](https://app.theconstruct.ai/courses/ros-2-perception-in-5-days-239/)
* [Behavior Trees for ROS 2](https://app.theconstruct.ai/courses/behavior-trees-for-ros2-131/)
* [ROS 2 Control Framework](https://app.theconstruct.ai/courses/ros-2-control-framework-jazzy-404/)
* [Programming Drones with ROS](https://app.theconstruct.ai/courses/programming-drones-with-ros-24/)
* [State Estimation and Localization for Self-Driving Cars](https://www.coursera.org/learn/state-estimation-localization-self-driving-cars) (Coursera)
* ETH [Autonomous Mobile Robots](https://www.edx.org/learn/autonomous-robotics/eth-zurich-autonomous-mobile-robots) (edX)
* ETH RSL [Programming for Robotics (ROS)](https://rsl.ethz.ch/education-students/lectures/ros.html)

**Canonical links (Part A)**

* [ROS 2 Humble tutorials](https://docs.ros.org/en/humble/Tutorials.html) · [QoS](https://docs.ros.org/en/humble/Concepts/About-Quality-of-Service-Settings.html) · [`ros2 bag`](https://docs.ros.org/en/humble/Tutorials/Beginner-CLI-Tools/Recording-And-Playing-Back-Data/Recording-And-Playing-Back-Data.html)
* [Nav2](https://navigation.ros.org/) · [Behavior trees in Nav2](https://navigation.ros.org/behavior_trees/index.html)
* [ros2_control](https://control.ros.org/)
* [`vision_msgs`](https://github.com/ros-perception/vision_msgs) · [cv_bridge](https://docs.ros.org/en/humble/Tutorials/Advanced/Cvbridge/Cvbridge.html)
* Barfoot, *State Estimation for Robotics* — [resource page](http://asrl.utias.utoronto.ca/~tdb/bib.html)
* [Gazebo](https://gazebosim.org/docs) · [ros_gz](https://github.com/gazebosim/ros_gz)
* [PX4 ROS 2](https://docs.px4.io/main/en/ros2/user_guide.html) · [MAVSDK](https://mavsdk.mavlink.io/)
* [Docker + ROS 2](https://github.com/osrf/docker_images) · [Docker how-to](https://docs.ros.org/en/humble/How-To-Guides/Run-2-nodes-in-single-or-separate-docker-containers.html)
* [FastAPI](https://fastapi.tiangolo.com/) · [NATS](https://docs.nats.io/)

**How Part A connects to other lectures**

| Topic | Where |
|-------|--------|
| Nav2, SLAM, `robot_localization` | [Lecture 1 — Advanced ROS](../02-高级机器人操作系统/Lecture-01.md), [Lecture 2 — Industrial](../03-工业与嵌入式机器人/Lecture-01.md) |
| Multi-robot | [Lecture 4 — Multi-Robot](../04-多机器人与集群机器人/Lecture-01.md) |

**Projects (Part A)**

1. **Detector → tracker → RViz2:** `vision_msgs` + track markers + `ros2 bag`.
2. **PX4 SITL or Gazebo + Nav2:** Compare **command latency** sim vs hardware.
3. **Bridge:** ROS 2 state → **FastAPI** + **NATS** events; measure end-to-end delay.

---

**Part B — Advanced perception and AI (expanded track)**

**B.1 Deep learning–based perception**

* **2D detection:** YOLO-family, RT-DETR, etc.—optimize for **latency** on Jetson (TensorRT).
* **6D pose:** FoundationPose, DenseFusion-style methods for **grasp**—outputs must feed **MoveIt 2** via **TF** and collision-aware planning.
* **3D point clouds:** **Open3D** / **PCL** for classical geometry; **PointNet++** / **voxel** nets for learned segmentation in structured scenes.

**B.2 VIO / SLAM**

**VIO** fuses **IMU** (high rate, biased) with **camera** (lower rate, rich). Failure modes: **motion blur**, **rolling shutter**, **textureless** regions. Always compare against **wheel odometry** or **mocap** when possible.

**B.3 Semantics and scene graphs**

**Semantic segmentation** (e.g. drivable vs obstacle) feeds **Nav2** costmaps. **3D scene graphs** attach **objects** and **relations** for **task planning** and **HRI** (“the cup on the left table”).

**Open-vocabulary** detectors (Grounding DINO, CLIP-based) reduce retraining but require **latency** and **grounding** validation on your robot.

**B.4 Tactile and multi-modal sensing**

**Tactile** arrays estimate **slip** and **contact**; fusion with **vision** helps **in-hand** manipulation. **Audio** can flag **collision** or **motor** anomalies—treat as **asynchronous** cues to **supervisors**, not hard real-time control unless validated.

**Resources (Part B)**

* [Open3D](http://www.open3d.org/docs/)
* Siciliano et al., *Robotics: Modelling, Planning and Control*
* Berkeley Robot Sensing (BRS) line of work (papers)

**Projects (Part B)**

* **6D pose + grasp:** Jetson Orin + table-top objects + MoveIt 2.
* **VIO benchmark:** ORB-SLAM3 vs ground truth.
* **Open-vocabulary pick-and-place:** language → detection → grasp.

---

**Part C — Robot learning and autonomous behaviors**

</details>

### C.1 强化学习

**Sim-to-real：** 随机化**动力学**、**摩擦**、**传感器噪声**、**延迟**；**域随机化**可降低对单一模拟器构建版本的**过拟合**。

**算法：** PPO 与 SAC 是连续控制的常用选择；**TD-MPC** 和**基于模型**的变体在某些配置下样本效率更高。

**框架：** Stable-Baselines3、RLlib、面向 GPU 密集训练的 **Isaac Lab**。

### C.2 模仿学习与离线 RL

**行为克隆**在**分布外**很脆弱；**DAgger** 通过混合专家数据与策略数据来降低**协变量偏移**。**Diffusion Policy** 输出**平滑**的多模态动作轨迹。

### C.3 机器人基础模型

**视觉-语言-动作（VLA）** 模型旨在把**图像 + 语言**映射为**动作**。在部署中，**大语言模型**常充当**高层规划器**，调用 **ROS 2** 技能（navigate、pick、place）——用**可执行**检查**验证**每一步。

### C.4 全身与足式控制

**WBC** 在约束（接触、COM）下协调**多自由度**。**足式**系统把**基于模型**的方法（MPC、WBC）与 **RL** 策略结合起来。**富接触**操作使用**混合**力/位置控制。

### 资源（Part C）

* [Isaac Lab](https://docs.omniverse.nvidia.com/isaacsim/latest/isaac_lab_tutorials/index.html)
* Sutton & Barto, *Reinforcement Learning: An Introduction*
* [Lerobot](https://github.com/huggingface/lerobot) (Hugging Face)

### 项目（Part C）

* **Sim-to-real 运动控制：** Isaac Sim → 真实四足机器人（记录迁移过程）。
* **Diffusion policy：** 遥操作演示 → 训练 → 在机械臂上评测。
* **大语言模型任务规划器：** 大语言模型输出**技能序列**，经由 ROS 2 action/service 执行。

---

## 自检（全讲）

**Part A：**（1）相比自定义 float 数组 topic，`vision_msgs` 能带来什么？（2）说出一个把 **FastAPI** 挡在**关键** DDS 回调路径之外的理由。

**Part B：** 在户外**高速**场景下，**单目深度**何时会失效？

**Part C：** 没有 **DAgger** 时，**行为克隆**的**一个**失效模式是什么？

---

## 本路线图的后续内容

* 上一节：[工业与嵌入式机器人](../03-工业与嵌入式机器人/Lecture-01.md)
* 下一节：[多机器人系统与集群机器人](../04-多机器人与集群机器人/Lecture-01.md)


<details>
<summary>English original</summary>

**C.1 Reinforcement learning**

**Sim-to-real:** Randomize **dynamics**, **friction**, **sensor noise**, **latency**; **domain randomization** reduces **overfitting** to one simulator build.

**Algorithms:** PPO and SAC are common for continuous control; **TD-MPC** and **model-based** variants sample-efficiently in some setups.

**Frameworks:** Stable-Baselines3, RLlib, **Isaac Lab** for GPU-heavy training.

**C.2 Imitation and offline RL**

**Behavior cloning** is fragile **out of distribution**; **DAgger** reduces **covariate shift** by mixing expert and policy data. **Diffusion Policy** outputs **smooth** multi-modal action trajectories.

**C.3 Foundation models for robotics**

**Vision-language-action (VLA)** models aim to map **images + language** to **actions**. In deployment, **LLMs** often act as **high-level planners** that call **ROS 2** skills (navigate, pick, place)—**verify** each step with **executable** checks.

**C.4 Whole-body and legged control**

**WBC** coordinates **many DoF** under constraints (contacts, COM). **Legged** systems blend **model-based** (MPC, WBC) with **RL** policies. **Contact-rich** manipulation uses **hybrid** force/position control.

**Resources (Part C)**

* [Isaac Lab](https://docs.omniverse.nvidia.com/isaacsim/latest/isaac_lab_tutorials/index.html)
* Sutton & Barto, *Reinforcement Learning: An Introduction*
* [Lerobot](https://github.com/huggingface/lerobot) (Hugging Face)

**Projects (Part C)**

* **Sim-to-real locomotion:** Isaac Sim → real quadruped (document transfer).
* **Diffusion policy:** Teleop demos → train → evaluate on arm.
* **LLM task planner:** LLM outputs **skill sequence** executed via ROS 2 actions/services.

---

**Self-check (whole lecture)**

**Part A:** (1) What does `vision_msgs` buy you vs a custom float array topic? (2) Name one reason to keep **FastAPI** out of the **critical** DDS callback path.

**Part B:** When does **monocular depth** fail outdoors at **high speed**?

**Part C:** What is **one** failure mode of **behavior cloning** without **DAgger**?

---

**Next in this roadmap**

* Previous: [Industrial and Embedded Robotics](../03-工业与嵌入式机器人/Lecture-01.md)
* Next: [Multi-Robot Systems and Swarm Robotics](../04-多机器人与集群机器人/Lecture-01.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track D - Robotics/Advanced Perception and AI for Robotics/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20D%20-%20Robotics/Advanced%20Perception%20and%20AI%20for%20Robotics/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
