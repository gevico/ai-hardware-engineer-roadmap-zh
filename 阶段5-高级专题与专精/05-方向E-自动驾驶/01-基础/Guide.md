---
title: 模块 1 — 自动驾驶基础
description: 模块 1 — 自动驾驶基础
---

# 模块 1 — 自动驾驶基础

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">MADF</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 专业化</p>
<p class="course-identity__title">模块 1 —— 自动驾驶基础的专业化课程标识。</p>
<p class="course-identity__meta">产物：专业化案例研究 · 衡量：性能、可靠性、岗位匹配度</p>
</div>
</div>


**父级：** [阶段 5 — 自动驾驶](../Guide.md)

**时间：** 3–4 个月

**前置要求：** 阶段 3（计算机视觉、传感器融合）、阶段 1 §4（C++/CUDA）。

---

## 为什么先学这一模块

在学习 openpilot 代码库或高级研究之前，需要先掌握基础词汇：自动驾驶车辆如何感知、规划与控制。本模块涵盖每位 ADAS 工程师日常都会用到的算法与理论。

---

## 1. 面向驾驶的计算机视觉

* **车道线检测：**
    * 传统方法：Hough 变换、滑动窗口、IPM（逆透视变换）。
    * 深度学习：LaneNet、SCNN、U-Net 做车道线分割。
    * 输出：车道边界的折线或多项式系数。

* **目标检测：**
    * 2D：YOLO 系列、CenterNet、FCOS —— 车辆、行人、骑行者的边界框。
    * 3D：PointPillars、SECOND（激光雷达）；FCOS3D、DETR3D（单目相机）。
    * BEV（鸟瞰图）：将 2D 相机特征提升到 BEV 空间，以统一做 3D 检测。

* **语义分割：**
    * 可行驶区域、车道线、路缘、障碍物。
    * 模型：DeepLab、SegFormer、轻量级边缘变体。
    * 全景分割：结合语义 + 实例，实现完整的场景理解。

* **深度估计：**
    * 单目深度（MiDaS、DepthAnything）—— 在没有激光雷达可用时有用。
    * 立体匹配：经典块匹配、深度立体（AANet、RAFT-Stereo）。

**项目：**
* 在行车记录仪视频上分别用 Hough 变换和 U-Net 实现车道线检测。比较在弯道、阴影和夜间条件下的鲁棒性。
* 在 KITTI 或 nuScenes 上训练 YOLO 模型做 2D 车辆检测。在 Jetson 上测量 mAP 和推理 FPS。

---

## 2. 规划与决策

* **行为规划：**
    * 高层决策：车道保持、换道、汇入、转向、停车。
    * 有限状态机（FSM）与行为树。
    * 基于规则 vs. 基于学习的行为规划。

* **运动规划：**
    * 路径规划：A*、RRT、RRT*、混合 A*（用于非完整约束车辆）。
    * 轨迹优化：在避免碰撞的同时最小化 jerk/加速度。
    * Frenet 坐标系：将规划分解为纵向（速度）与横向（车道偏移）分量。
    * Lattice 规划器：离散化轨迹空间，用代价函数评估候选轨迹。

* **预测：**
    * 恒定速度 / 恒定转向率模型（基线）。
    * 基于学习：TNT、LaneGCN、MTR —— 多模态轨迹预测。
    * 交互感知：用图神经网络建模 agent 与 agent 之间的交互。
    * 地图条件：利用车道几何与交通规则约束预测。

**项目：**
* 在带障碍物的 2D 栅格中实现 RRT* 路径规划。扩展到自行车模型以满足非完整约束。
* 构建一个简单的行为 FSM：CRUISE → FOLLOW → LANE_CHANGE → STOP。在带交通流的 CARLA 中测试。

---

## 3. 自动驾驶车辆控制理论

* **车辆动力学：**
    * 自行车模型：前/后轴、侧偏角、横摆角速度。
    * 轮胎模型：线性侧偏刚度、Pacejka 魔术公式（概述）。
    * 车辆状态估计：融合 IMU + 轮式里程计 + GPS 做定位。

* **横向控制：**
    * **Stanley 控制器：** 横向跟踪误差 + 航向误差，用于早期 AV 和 openpilot。
    * **Pure pursuit：** 使用前视点的几何路径跟踪。
    * **MPC（模型预测控制）：** 在一个时域上优化一串转向指令，并受动力学约束。

* **纵向控制：**
    * PID 做速度跟踪（自适应巡航控制）。
    * MPC 做速度 + 跟车距离联合控制。
    * 限 jerk 的曲线，保证乘客舒适性。

* **横纵向联合控制：**
    * LQR（线性二次调节器）用于带速度控制的路径跟踪。
    * 级联 MPC：横向/纵向 MPC 分离并协调。

**项目：**
* 在 CARLA 中实现 Stanley 控制器做车道跟随。针对不同速度整定增益。
* 实现基于 PID 的自适应巡航控制（ACC）：保持目标速度、对前车减速、走停。在 CARLA 中测试。
* （进阶）实现一个简单的 MPC 做横纵向联合控制。对比与 Stanley + PID 的平滑性和跟踪误差。

---


<details>
<summary>English original</summary>

**Module 1 — Autonomous Driving Fundamentals**

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">MADF</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for Module 1 — Autonomous Driving Fundamentals.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [Phase 5 — Autonomous Driving](../Guide.md)

**Time:** 3–4 months

**Prerequisites:** Phase 3 (Computer Vision, Sensor Fusion), Phase 1 §4 (C++/CUDA).

---

**Why this comes first**

Before studying openpilot's codebase or advanced research, you need the foundational vocabulary: how autonomous vehicles perceive, plan, and control. This module covers the algorithms and theory that every ADAS engineer uses daily.

---

**1. Computer Vision for Driving**

* **Lane detection:**
    * Traditional: Hough transform, sliding window, IPM (inverse perspective mapping).
    * Deep learning: LaneNet, SCNN, U-Net for lane segmentation.
    * Output: polylines or polynomial coefficients for lane boundaries.

* **Object detection:**
    * 2D: YOLO family, CenterNet, FCOS — bounding boxes for vehicles, pedestrians, cyclists.
    * 3D: PointPillars, SECOND (LiDAR); FCOS3D, DETR3D (monocular camera).
    * Bird's Eye View (BEV): lifting 2D camera features to BEV space for unified 3D detection.

* **Semantic segmentation:**
    * Drivable area, lane markings, curbs, obstacles.
    * Models: DeepLab, SegFormer, lightweight edge variants.
    * Panoptic segmentation: combining semantic + instance for complete scene understanding.

* **Depth estimation:**
    * Monocular depth (MiDaS, DepthAnything) — useful when no LiDAR is available.
    * Stereo matching: classic block matching, deep stereo (AANet, RAFT-Stereo).

**Projects:**
* Implement lane detection on a dashcam video using both Hough transform and a U-Net. Compare robustness in curves, shadows, and night.
* Train a YOLO model on KITTI or nuScenes for 2D vehicle detection. Measure mAP and inference FPS on Jetson.

---

**2. Planning and Decision Making**

* **Behavior planning:**
    * High-level decisions: lane keep, lane change, merge, turn, stop.
    * Finite state machines (FSM) and behavior trees.
    * Rule-based vs. learning-based behavior planning.

* **Motion planning:**
    * Path planning: A*, RRT, RRT*, hybrid A* (for non-holonomic vehicles).
    * Trajectory optimization: minimize jerk/acceleration while avoiding collisions.
    * Frenet frame: decompose planning into longitudinal (speed) and lateral (lane offset) components.
    * Lattice planners: discretize trajectory space, evaluate candidates against cost functions.

* **Prediction:**
    * Constant velocity / constant turn-rate models (baseline).
    * Learning-based: TNT, LaneGCN, MTR — multi-modal trajectory prediction.
    * Interaction-aware: graph neural networks for modeling agent-agent interactions.
    * Map-conditioned: use lane geometry and traffic rules to constrain predictions.

**Projects:**
* Implement RRT* for path planning in a 2D grid with obstacles. Extend to a bicycle model for non-holonomic constraints.
* Build a simple behavior FSM: CRUISE → FOLLOW → LANE_CHANGE → STOP. Test in CARLA with traffic.

---

**3. Control Theory for Autonomous Vehicles**

* **Vehicle dynamics:**
    * Bicycle model: front/rear axle, slip angle, yaw rate.
    * Tire models: linear cornering stiffness, Pacejka magic formula (overview).
    * Vehicle state estimation: combine IMU + wheel odometry + GPS for localization.

* **Lateral control:**
    * **Stanley controller:** cross-track error + heading error, used in early AV and openpilot.
    * **Pure pursuit:** geometric path following using a lookahead point.
    * **MPC (Model Predictive Control):** optimize a sequence of steering commands over a horizon, subject to dynamics constraints.

* **Longitudinal control:**
    * PID for speed tracking (adaptive cruise control).
    * MPC for combined speed + following distance.
    * Jerk-limited profiles for passenger comfort.

* **Combined lateral + longitudinal:**
    * LQR (Linear Quadratic Regulator) for path tracking with speed control.
    * Cascaded MPC: separate lateral/longitudinal MPC with coordination.

**Projects:**
* Implement a Stanley controller for lane following in CARLA. Tune gains for different speeds.
* Implement PID-based ACC (Adaptive Cruise Control): maintain target speed, slow for lead vehicle, stop-and-go. Test in CARLA.
* (Advanced) Implement a simple MPC for combined lateral + longitudinal control. Compare smoothness and tracking error vs Stanley + PID.

---

</details>

## 资源

| 资源 | 理由 |
|----------|-----|
| [CARLA Simulator](https://carla.org/) | 接触真实硬件之前，先在仿真中测试所有算法 |
| *Autonomous Driving in the Real World*（Shaoshan Liu 等） | 全面的 AV 教科书 |
| [Apollo Auto](https://github.com/ApolloAuto/apollo) | 用于架构对比的参考全栈 AV |
| [KITTI](http://www.cvlibs.net/datasets/kitti/) / [nuScenes](https://www.nuscenes.org/) | Benchmark 数据集 |
| 阶段 3 — 计算机视觉、传感器融合 | 本路线图中的前置模块 |

---

## 下一步

→ **[模块 2 — openpilot Reference Stack](../02-openpilot参考栈/Guide.md)** — 端到端研究一个真实、已量产部署的 ADAS。


<details>
<summary>English original</summary>

**Resources**

| Resource | Why |
|----------|-----|
| [CARLA Simulator](https://carla.org/) | Test all algorithms in simulation before touching real hardware |
| *Autonomous Driving in the Real World* (Shaoshan Liu et al.) | Comprehensive AV textbook |
| [Apollo Auto](https://github.com/ApolloAuto/apollo) | Reference full-stack AV for architectural comparison |
| [KITTI](http://www.cvlibs.net/datasets/kitti/) / [nuScenes](https://www.nuscenes.org/) | Benchmark datasets |
| Phase 3 — Computer Vision, Sensor Fusion | Prerequisite modules in this roadmap |

---

**Next**

→ **[Module 2 — openpilot Reference Stack](../02-openpilot参考栈/Guide.md)** — Study a real, production-deployed ADAS end-to-end.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/1. Fundamentals/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/1.%20Fundamentals/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
