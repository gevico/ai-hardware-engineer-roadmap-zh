---
title: 模块 4 — 高级感知与预测
description: 模块 4 — 高级感知与预测
---

# 模块 4 — 高级感知与预测

<div class="course-identity auto-course" style="--course-accent: #0d9488; --course-accent-rgb: 13, 148, 136;" markdown="1">
<div class="course-identity__icon">MAPA</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度探索 · 专业化</p>
<p class="course-identity__title">模块 4 — 高级感知与预测的专业化课程标识。</p>
<p class="course-identity__meta">产物：专业化案例研究 · 度量：性能、可靠性、角色匹配</p>
</div>
</div>


**父级：** [阶段 5 — 自动驾驶](../Guide.md)

**时间：** 6–12 个月

**前置要求：** 模块 1–3，阶段 3（传感器融合 — 卡尔曼滤波、多传感器数学）。

---

## 为什么这是高级内容

模块 1–3 教你理解并使用单摄像头、生产部署的 ADAS（openpilot）。本模块更进一步：多传感器感知、BEV（鸟瞰图）架构、轨迹预测、高精地图和仿真 — L3+ 自动驾驶的研究与工程前沿。

---

## 1. 高级传感器硬件

* **LiDAR（激光雷达）：**
    * 工作原理：机械旋转（Velodyne）、固态（Livox、Innoviz）、FMCW（Aeva、Luminar）。
    * 权衡：距离、分辨率、FoV、延迟、成本。
    * 点云表示：原始点、体素、距离图像、柱体。

* **雷达：**
    * 汽车雷达：77 GHz FMCW、距离-多普勒处理、角分辨率。
    * 4D 成像雷达：仰角 + 方位角 + 距离 + 速度。
    * 雷达优势：全天候、直接测速、远距离。

* **摄像头系统：**
    * 传感器类型：CCD vs CMOS、全局 vs 卷帘快门。
    * 光学：FoV、焦距、光圈、HDR 技术。
    * 汽车 ISP 流水线（与模块 2 camerad 的联系）。

---

## 2. 多传感器校准

* **内参校准：**
    * 相机内参（焦距、主点、畸变）使用棋盘格或 ArUco 图案，配合 OpenCV 或 Kalibr。

* **外参校准：**
    * 传感器间的空间变换：摄像头-LiDAR、摄像头-雷达、LiDAR-IMU。
    * 基于靶标（棋盘格、ArUco）和无靶标方法（LI-Calib、ACSC）。

* **时间校准：**
    * 跨硬件时钟对齐传感器时间戳。
    * PPS 信号、IEEE 1588 PTP、观测事件的互相关。
    * 时间不对齐对融合准确率的影响。

**项目：**
* 构建一个校准靶标，并使用 Kalibr 或 ACSC 校准摄像头-LiDAR 对。通过将 LiDAR 点投影到摄像头图像上进行验证。
* 实现雷达-摄像头融合流水线：将雷达检测与摄像头边界框关联，进行速度增强检测。

---

## 3. BEV（鸟瞰图）感知

* **BEV Transformer：**
    * BEVFormer：基于 attention 的跨视图特征提升，从透视视图到 BEV。
    * BEVFusion：在 BEV 空间中进行多模态（摄像头 + LiDAR）融合。
    * Tesla 的 Occupancy Network：从摄像头进行密集 3D 体素预测。

* **3D 占用预测：**
    * 基于体素的占用（Occ3D、SurroundOcc）作为显式目标检测的替代方案。
    * 将环境表示为占用/空闲体素的密集 3D 网格。

* **时序融合：**
    * 使用循环架构或长时程时序 attention 纳入时序信息。
    * 跨帧跟踪隐式目标状态，无需显式跟踪。

**项目：**
* 在 nuScenes 上训练 BEVFusion 风格的模型，用于摄像头+LiDAR 3D 目标检测。与仅 LiDAR 的基线进行比较。

---

## 4. 轨迹预测

* **多模态预测：**
    * TNT、MTR、Wayformer：预测多条可能的未来轨迹及其概率分数。
    * 为什么是多模态：车辆在路口可能直行、左转或右转。

* **交互建模：**
    * 社会力模型、图神经网络（GRIP、HYPER）。
    * 基于 Transformer 的交互编码器，用于 agent-agent 推理。

* **地图条件预测：**
    * 纳入车道几何和交通规则。
    * VectorNet、MapTR 风格的地图编码。
    * 将预测约束为物理上合理、沿车道行驶的轨迹。

**项目：**
* 在 Waymo Open Motion Dataset 上实现 TNT 或 MTR。评估 minADE/minFDE。可视化多模态预测。

---

## 5. 高精地图与在线建图

* **高精地图组件：**
    * 图层：车道几何、道路标线、交通标志、交通信号灯、3D 地标。
    * 格式：OpenDRIVE、Lanelet2。

* **在线地图构建：**
    * MapTR、BeMapNet：从传感器数据实时预测高精地图。
    * 减少对预建地图的依赖，支持未测绘区域。

* **基于地图的定位：**
    * 点云匹配：NDT、ICP。
    * 摄像头-地图匹配，用于在高精地图内精确定位。

**项目：**
* 在 CARLA 中部署基于 MapTR 的在线地图系统。将在线地图质量与真值高精地图进行比较。

---


<details>
<summary>English original</summary>

**Module 4 — Advanced Perception and Prediction**

<div class="course-identity auto-course" style="--course-accent: #0d9488; --course-accent-rgb: 13, 148, 136;" markdown="1">
<div class="course-identity__icon">MAPA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for Module 4 — Advanced Perception and Prediction.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [Phase 5 — Autonomous Driving](../Guide.md)

**Time:** 6–12 months

**Prerequisites:** Modules 1–3, Phase 3 (Sensor Fusion — Kalman filtering, multi-sensor math).

---

**Why this is advanced**

Modules 1–3 teach you to understand and work with a single-camera, production-deployed ADAS (openpilot). This module goes beyond: multi-sensor perception, BEV architectures, trajectory prediction, HD maps, and simulation — the research and engineering frontier for L3+ autonomy.

---

**1. Advanced Sensor Hardware**

* **LiDAR:**
    * Working principles: mechanical spinning (Velodyne), solid-state (Livox, Innoviz), FMCW (Aeva, Luminar).
    * Trade-offs: range, resolution, FoV, latency, cost.
    * Point cloud representations: raw points, voxels, range images, pillars.

* **Radar:**
    * Automotive radar: 77 GHz FMCW, range-Doppler processing, angular resolution.
    * 4D imaging radar: elevation + azimuth + range + velocity.
    * Radar advantages: all-weather, direct velocity measurement, long range.

* **Camera systems:**
    * Sensor types: CCD vs CMOS, global vs rolling shutter.
    * Optics: FoV, focal length, aperture, HDR techniques.
    * ISP pipelines for automotive (connection to Module 2 camerad).

---

**2. Multi-Sensor Calibration**

* **Intrinsic calibration:**
    * Camera intrinsics (focal length, principal point, distortion) using checkerboard or ArUco patterns with OpenCV or Kalibr.

* **Extrinsic calibration:**
    * Spatial transforms between sensors: camera-LiDAR, camera-radar, LiDAR-IMU.
    * Target-based (checkerboard, ArUco) and targetless methods (LI-Calib, ACSC).

* **Temporal calibration:**
    * Aligning sensor timestamps across hardware clocks.
    * PPS signals, IEEE 1588 PTP, cross-correlation of observed events.
    * Impact of time misalignment on fusion accuracy.

**Projects:**
* Build a calibration target and calibrate a camera-LiDAR pair using Kalibr or ACSC. Verify by projecting LiDAR points onto camera image.
* Implement a radar-camera fusion pipeline: associate radar detections with camera bounding boxes for velocity-augmented detection.

---

**3. BEV (Bird's-Eye View) Perception**

* **BEV transformers:**
    * BEVFormer: attention-based cross-view feature lifting from perspective to BEV.
    * BEVFusion: multi-modal (camera + LiDAR) fusion in BEV space.
    * Tesla's Occupancy Network: dense 3D voxel prediction from cameras.

* **3D occupancy prediction:**
    * Voxel-based occupancy (Occ3D, SurroundOcc) as alternative to explicit object detection.
    * Represent environment as dense 3D grid of occupied/free voxels.

* **Temporal fusion:**
    * Incorporate temporal information using recurrent architectures or long-range temporal attention.
    * Track implicit object states across frames without explicit tracking.

**Projects:**
* Train a BEVFusion-style model on nuScenes for camera+LiDAR 3D object detection. Compare with LiDAR-only baseline.

---

**4. Trajectory Prediction**

* **Multi-modal prediction:**
    * TNT, MTR, Wayformer: predict multiple plausible future trajectories with probability scores.
    * Why multi-modal: a vehicle at an intersection might go straight, turn left, or turn right.

* **Interaction modeling:**
    * Social force models, graph neural networks (GRIP, HYPER).
    * Transformer-based interaction encoders for agent-agent reasoning.

* **Map-conditioned prediction:**
    * Incorporate lane geometry and traffic rules.
    * VectorNet, MapTR-style map encoding.
    * Constraining predictions to physically plausible, lane-following trajectories.

**Projects:**
* Implement TNT or MTR on the Waymo Open Motion Dataset. Evaluate minADE/minFDE. Visualize multi-modal predictions.

---

**5. HD Maps and Online Mapping**

* **HD map components:**
    * Layers: lane geometry, road markings, traffic signs, traffic lights, 3D landmarks.
    * Formats: OpenDRIVE, Lanelet2.

* **Online map building:**
    * MapTR, BeMapNet: predict HD map from sensor data in real-time.
    * Reduces dependence on pre-built maps, supports uncharted areas.

* **Map-based localization:**
    * Point cloud matching: NDT, ICP.
    * Camera-map matching for precise positioning within HD map.

**Projects:**
* Deploy a MapTR-based online map system in CARLA. Compare online map quality against ground-truth HD map.

---

</details>

## 6. Sensor Simulation and Synthetic Data

* **合成数据生成：**
    * CARLA、SUMO+CARLA、NVIDIA DRIVE Sim，用于生成带 ground-truth 标签的 photorealistic 训练数据。
    * 标签：bounding boxes、semantic masks、depth、radar detections。

* **传感器模型仿真：**
    * LiDAR 光线投射、radar 多径、相机噪声模型。
    * 缩小训练数据中的 sim-to-real gap。

* **场景生成：**
    * Corner cases：雾、雨、夜间、遮挡、near-miss 事件。
    * 真实驾驶中无法采集的罕见或不安全场景。

**Projects：**
* 在 CARLA 中生成 10,000 帧合成数据集，覆盖不同天气、光照和交通密度。分别用合成数据与真实数据训练 3D 检测器并对比。

---

## Resources

| Resource | Why |
|----------|-----|
| [nuScenes](https://www.nuscenes.org/) | 带 3D 标注和 HD map 的多模态 AV 数据集 |
| [Waymo Open Dataset](https://waymo.com/open/) | 带运动预测 benchmark 的大规模数据集 |
| BEVFormer / BEVFusion papers | BEV 感知的基础架构 |
| [Kalibr](https://github.com/ethz-asl/kalibr) | 多传感器 calibration 工具包 |
| [CARLA](https://carla.org/) | 传感器仿真与合成数据 |

---

## Next

→ **[Module 5 — Safety Standards and Deployment](../05-安全标准与部署/Guide.md)** — ISO 26262、SOTIF、V2X、HIL 测试与生产部署。


<details>
<summary>English original</summary>

**6. Sensor Simulation and Synthetic Data**

* **Synthetic data generation:**
    * CARLA, SUMO+CARLA, NVIDIA DRIVE Sim for photorealistic training data with ground-truth labels.
    * Labels: bounding boxes, semantic masks, depth, radar detections.

* **Sensor model simulation:**
    * LiDAR ray casting, radar multipath, camera noise models.
    * Reducing sim-to-real gap in training data.

* **Scenario generation:**
    * Corner cases: fog, rain, night, occlusions, near-miss events.
    * Rare or unsafe scenarios that cannot be collected in real driving.

**Projects:**
* Generate a 10,000-frame synthetic dataset in CARLA with varying weather, lighting, and traffic density. Train a 3D detector on synthetic vs. real data and compare.

---

**Resources**

| Resource | Why |
|----------|-----|
| [nuScenes](https://www.nuscenes.org/) | Multi-modal AV dataset with 3D annotations and HD maps |
| [Waymo Open Dataset](https://waymo.com/open/) | Large-scale dataset with motion prediction benchmarks |
| BEVFormer / BEVFusion papers | Foundational BEV perception architectures |
| [Kalibr](https://github.com/ethz-asl/kalibr) | Multi-sensor calibration toolkit |
| [CARLA](https://carla.org/) | Sensor simulation and synthetic data |

---

**Next**

→ **[Module 5 — Safety Standards and Deployment](../05-安全标准与部署/Guide.md)** — ISO 26262, SOTIF, V2X, HIL testing, and production deployment.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/4. Advanced Perception and Prediction/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/4.%20Advanced%20Perception%20and%20Prediction/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
