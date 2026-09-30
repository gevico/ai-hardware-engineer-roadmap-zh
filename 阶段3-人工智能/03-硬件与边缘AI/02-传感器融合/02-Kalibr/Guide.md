---
title: Kalibr — 面向自动驾驶汽车的多传感器校准
description: Kalibr — 面向自动驾驶汽车的多传感器校准
---

# Kalibr — 面向自动驾驶汽车的多传感器校准

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">KMSC</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · AI 工作负载</p>
<p class="course-identity__title">Kalibr 的专用课程标识 —— 面向自动驾驶汽车的多传感器校准。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


> **来源：** [ethz-asl/kalibr](https://github.com/ethz-asl/kalibr) — ETH Zurich Autonomous Systems Lab
> **解决什么：** 相机内参、多相机外参、camera-IMU 空间+时间校准、卷帘快门校准 —— 全部通过批连续时间轨迹优化完成。
> **为何重要：** 每个下游系统（BEVFusion、SLAM、openpilot、VIO）都依赖正确的校准。camera-IMU 系统中 1° 的外参旋转误差会导致里程计漂移约为行驶距离的 1.7%。

---


## 1. 为何校准至关重要

### 校准误差的真实代价

所有多传感器融合都依赖两件事被精确已知：

```
Intrinsic calibration:  how does this camera map 3D rays to 2D pixels?
                        errors here: warped images, wrong depth estimates

Extrinsic calibration:  what is the rigid transform between sensor A and sensor B?
                        errors here: LiDAR points project to wrong pixels,
                        IMU integration drifts, fusion degrades or diverges
```

### 具体误差影响

```
Camera intrinsic error (1 pixel reprojection):
  At 20m depth: ~4cm lateral position error per pixel
  In BEVFusion: camera features misaligned with LiDAR → fusion hurts accuracy

Camera-LiDAR extrinsic error (1° rotation):
  Point at 30m range: 30 × tan(1°) = 52cm lateral shift
  BEVFusion camera BEV features miss LiDAR BEV features by ~52cm

Camera-IMU temporal error (1ms time offset):
  At 1 m/s IMU excitation: 1mm position error per ms
  In VIO at aggressive motion: position error accumulates every step
  In SLAM: loop closures fail because frames don't align

Camera-IMU spatial error (1cm translation):
  Gyroscope correction for camera motion uses wrong lever arm
  In VIO: accelerometer-induced rotation errors
```

### 自动驾驶汽车的校准链

```
Step 1: Calibrate each camera intrinsically (separate step)
Step 2: Calibrate multi-camera rig extrinsics (camera 0 ← → camera 1 ... N)
Step 3: Calibrate camera-IMU (spatial + temporal)
Step 4: Calibrate LiDAR-camera (target-based or targetless)
Step 5: Calibrate radar-camera (if applicable)
Step 6: Validate full system — project LiDAR onto camera image

Kalibr handles steps 1, 2, 3 with a single unified framework.
Steps 4–5 need separate tools (see Section 12).
```

---

## 2. Kalibr 架构

### 连续时间轨迹优化

与 OpenCV 的单帧内参校准不同，Kalibr 采用**连续时间批优化**：

```
Traditional (OpenCV):
  N static images → extract corners → minimize reprojection error per image
  Works well for intrinsics, not suitable for camera-IMU (no IMU model)

Kalibr:
  Video sequence → B-spline trajectory fitting → unified cost function
  Cost = reprojection error + IMU pre-integration error + temporal alignment error

Why B-spline?
  IMU measures continuous motion: ω(t), a(t)
  B-spline provides continuous position p(t), orientation q(t)
  Can compute p'(t), p''(t) analytically for any timestamp t
  → Compare B-spline acceleration with IMU accelerometer reading directly
```

### Kalibr 联合估计的内容

```
Camera intrinsics:  [fu, fv, cu, cv, distortion params]
Camera extrinsics:  T_cam1_cam0  (4×4 transform from cam0 to cam1 frame)
Camera-IMU:         T_imu_cam    (4×4 transform) + time_offset (seconds)
IMU intrinsics:     scale, misalignment, bias (if --imu-models is specified)
```

---

## 3. 校准靶标


<details>
<summary>English original</summary>

**Kalibr — Multi-Sensor Calibration for Autonomous Vehicles**

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">KMSC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for Kalibr — Multi-Sensor Calibration for Autonomous Vehicles.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


> **Source:** [ethz-asl/kalibr](https://github.com/ethz-asl/kalibr) — ETH Zurich Autonomous Systems Lab
> **What it solves:** Camera intrinsics, multi-camera extrinsics, camera-IMU spatial+temporal calibration, rolling-shutter calibration — all through batch continuous-time trajectory optimization.
> **Why it matters:** Every downstream system (BEVFusion, SLAM, openpilot, VIO) depends on correct calibration. A 1° extrinsic rotation error in a camera-IMU system causes odometry drift of ~1.7% of distance traveled.

---


**1. Why Calibration is Critical**

**What Calibration Errors Actually Cost**

All multi-sensor fusion depends on two things being known precisely:

```
Intrinsic calibration:  how does this camera map 3D rays to 2D pixels?
                        errors here: warped images, wrong depth estimates

Extrinsic calibration:  what is the rigid transform between sensor A and sensor B?
                        errors here: LiDAR points project to wrong pixels,
                        IMU integration drifts, fusion degrades or diverges
```

**Concrete Error Impact**

```
Camera intrinsic error (1 pixel reprojection):
  At 20m depth: ~4cm lateral position error per pixel
  In BEVFusion: camera features misaligned with LiDAR → fusion hurts accuracy

Camera-LiDAR extrinsic error (1° rotation):
  Point at 30m range: 30 × tan(1°) = 52cm lateral shift
  BEVFusion camera BEV features miss LiDAR BEV features by ~52cm

Camera-IMU temporal error (1ms time offset):
  At 1 m/s IMU excitation: 1mm position error per ms
  In VIO at aggressive motion: position error accumulates every step
  In SLAM: loop closures fail because frames don't align

Camera-IMU spatial error (1cm translation):
  Gyroscope correction for camera motion uses wrong lever arm
  In VIO: accelerometer-induced rotation errors
```

**The Calibration Chain for an Autonomous Vehicle**

```
Step 1: Calibrate each camera intrinsically (separate step)
Step 2: Calibrate multi-camera rig extrinsics (camera 0 ← → camera 1 ... N)
Step 3: Calibrate camera-IMU (spatial + temporal)
Step 4: Calibrate LiDAR-camera (target-based or targetless)
Step 5: Calibrate radar-camera (if applicable)
Step 6: Validate full system — project LiDAR onto camera image

Kalibr handles steps 1, 2, 3 with a single unified framework.
Steps 4–5 need separate tools (see Section 12).
```

---

**2. Kalibr Architecture**

**Continuous-Time Trajectory Optimization**

Unlike OpenCV's single-frame intrinsic calibration, Kalibr uses **continuous-time batch optimization**:

```
Traditional (OpenCV):
  N static images → extract corners → minimize reprojection error per image
  Works well for intrinsics, not suitable for camera-IMU (no IMU model)

Kalibr:
  Video sequence → B-spline trajectory fitting → unified cost function
  Cost = reprojection error + IMU pre-integration error + temporal alignment error

Why B-spline?
  IMU measures continuous motion: ω(t), a(t)
  B-spline provides continuous position p(t), orientation q(t)
  Can compute p'(t), p''(t) analytically for any timestamp t
  → Compare B-spline acceleration with IMU accelerometer reading directly
```

**What Kalibr Estimates Jointly**

```
Camera intrinsics:  [fu, fv, cu, cv, distortion params]
Camera extrinsics:  T_cam1_cam0  (4×4 transform from cam0 to cam1 frame)
Camera-IMU:         T_imu_cam    (4×4 transform) + time_offset (seconds)
IMU intrinsics:     scale, misalignment, bias (if --imu-models is specified)
```

---

**3. Calibration Targets**

</details>

### A. AprilGrid（强烈推荐）

AprilGrid 使用基准标记（每个标记有唯一 ID），从而支持：
- 部分可见 —— 标定板无需完整出现在画面中
- 无位姿歧义 —— 唯一 ID 可消除棋盘格的 180° 歧义
- 运动模糊下检测依然鲁棒

```yaml
# aprilgrid_6x6.yaml
target_type: 'aprilgrid'
tagCols:     6          # tags across
tagRows:     6          # tags down
tagSize:     0.088      # tag outer edge length in meters — MEASURE AFTER PRINTING
tagSpacing:  0.3        # ratio: gap_between_tags / tagSize
```

```bash
# Generate the PDF target
kalibr_create_target_pdf \
    --type apriltag \
    --nx 6 \
    --ny 6 \
    --tsize 0.088 \
    --tspace 0.3 \
    --output aprilgrid_6x6.pdf

# Print at 100% scale (no scaling/fit to page)
# Glue to a flat, rigid board — aluminum composite or 10mm forex
# ALWAYS re-measure tagSize after printing with calipers
# Printer scale error of 2% → 2% error in metric scale → scale error in depth
```

### B. 棋盘格（更简单，鲁棒性较差）

标准棋盘格。便宜且易于制作，但：
- 要求每一帧中都完整可见
- 存在 180° 位姿歧义（可借助 Kalibr 的初始化缓解，但可靠性较低）
- 仅对全局快门相机能可靠工作

```yaml
# checkerboard_7x6.yaml
target_type: 'checkerboard'
targetCols:         7     # internal corner count, horizontal
targetRows:         6     # internal corner count, vertical
rowSpacingMeters:   0.03  # square size in meters
colSpacingMeters:   0.03
```

### C. Circlegrid（很少使用）

```yaml
target_type: 'circlegrid'
targetCols:   6
targetRows:   7
spacingMeters: 0.02
asymmetricGrid: False
```

### 物理标定板制作

```
For AprilGrid:
  Paper:    Print → laminate → glue to 5mm aluminum composite
  Foam:     Print → glue to 10mm foam board (lighter, ok for indoor)
  Size:     At least 40×40cm for cameras at 0.5–2m distance

Critical measurements after printing:
  1. Measure tagSize with digital calipers at 4+ locations → average
  2. Measure tagSpacing ratio: (gap between tags) / tagSize
  3. Update your YAML before calibration — wrong measurements = wrong scale

White border:
  Leave at least one tag-width of white space around the entire grid
  Without it, corner detection fails at board edges
```

---

## 4. 相机模型与畸变模型

Kalibr 通过 `projection-distortion` 指定模型。示例：`pinhole-radtan`。

### 投影模型

| 模型  | CLI 名称 | 参数        | 适用场景                              |
|--------|----------|-------------------|---------------------------------------|
| Pinhole | `pinhole` | fu, fv, cu, cv  | 标准相机，FoV < ~120°        |
| Omnidirectional | `omni` | xi, fu, fv, cu, cv | 鱼眼 / 折反射（FoV ~180°） |
| Double Sphere | `ds` | xi, alpha, fu, fv, cu, cv | 广角鱼眼（FoV ~195°） |
| Extended Unified | `eucm` | alpha, beta, fu, fv, cu, cv | 另一种鱼眼模型 |

### 畸变模型

| 模型    | CLI 名称 | 参数          | 适用场景                           |
|----------|----------|---------------------|------------------------------------|
| Radial-Tangential | `radtan` | k1, k2, r1, r2 | 标准镜头，FoV < 120°    |
| Equidistant | `equi` | k1, k2, k3, k4  | 鱼眼镜头（广角首选） |
| Field-of-View | `fov` | w                | 简单的单参数鱼眼模型  |
| None     | `none`   | —                   | 理想 / 仿真相机            |

### 如何选择模型

```
USB webcam (60–90° FoV):     pinhole-radtan
DSLR / action cam (90–120°): pinhole-radtan or pinhole-equi
Wide-angle / GoPro (120–180°): pinhole-equi
Fisheye surround cam (>180°): omni-radtan or ds-none
Jetson CSI camera (IMX219):  pinhole-radtan
Intel RealSense D435i:
  RGB: pinhole-radtan
  IR stereo: pinhole-radtan
```

---

## 5. 安装

### 方法 1：Docker（推荐 —— 适用于任意 Ubuntu）

```bash
# Pull Kalibr Docker image
docker pull ghcr.io/ethz-asl/kalibr:latest

# Run interactively with shared folder for data/results
docker run -it \
    --volume /path/to/your/data:/data \
    --volume /tmp/.X11-unix:/tmp/.X11-unix \
    --env DISPLAY=$DISPLAY \
    ghcr.io/ethz-asl/kalibr:latest

# Inside container, Kalibr commands are available:
kalibr_calibrate_cameras --help
kalibr_calibrate_imu_camera --help
kalibr_create_target_pdf --help
```


<details>
<summary>English original</summary>

**A. AprilGrid (Strongly Recommended)**

AprilGrid uses fiducial markers (each with a unique ID), enabling:
- Partial visibility — the board doesn't need to be fully in frame
- No pose ambiguity — unique ID resolves the 180° checkerboard ambiguity
- Robust detection under motion blur

```yaml
# aprilgrid_6x6.yaml
target_type: 'aprilgrid'
tagCols:     6          # tags across
tagRows:     6          # tags down
tagSize:     0.088      # tag outer edge length in meters — MEASURE AFTER PRINTING
tagSpacing:  0.3        # ratio: gap_between_tags / tagSize
```

```bash
# Generate the PDF target
kalibr_create_target_pdf \
    --type apriltag \
    --nx 6 \
    --ny 6 \
    --tsize 0.088 \
    --tspace 0.3 \
    --output aprilgrid_6x6.pdf

# Print at 100% scale (no scaling/fit to page)
# Glue to a flat, rigid board — aluminum composite or 10mm forex
# ALWAYS re-measure tagSize after printing with calipers
# Printer scale error of 2% → 2% error in metric scale → scale error in depth
```

**B. Checkerboard (Simpler, Less Robust)**

Standard chessboard. Cheap and easy to make but:
- Requires full visibility in every frame
- Has 180° pose ambiguity (mitigated by Kalibr's init, but less reliable)
- Only works for global-shutter cameras reliably

```yaml
# checkerboard_7x6.yaml
target_type: 'checkerboard'
targetCols:         7     # internal corner count, horizontal
targetRows:         6     # internal corner count, vertical
rowSpacingMeters:   0.03  # square size in meters
colSpacingMeters:   0.03
```

**C. Circlegrid (Rarely Used)**

```yaml
target_type: 'circlegrid'
targetCols:   6
targetRows:   7
spacingMeters: 0.02
asymmetricGrid: False
```

**Physical Target Construction**

```
For AprilGrid:
  Paper:    Print → laminate → glue to 5mm aluminum composite
  Foam:     Print → glue to 10mm foam board (lighter, ok for indoor)
  Size:     At least 40×40cm for cameras at 0.5–2m distance

Critical measurements after printing:
  1. Measure tagSize with digital calipers at 4+ locations → average
  2. Measure tagSpacing ratio: (gap between tags) / tagSize
  3. Update your YAML before calibration — wrong measurements = wrong scale

White border:
  Leave at least one tag-width of white space around the entire grid
  Without it, corner detection fails at board edges
```

---

**4. Camera Models and Distortion Models**

Kalibr specifies the model as `projection-distortion`. Example: `pinhole-radtan`.

**Projection Models**

| Model  | CLI name | Parameters        | Use Case                              |
|--------|----------|-------------------|---------------------------------------|
| Pinhole | `pinhole` | fu, fv, cu, cv  | Standard cameras, FoV < ~120°        |
| Omnidirectional | `omni` | xi, fu, fv, cu, cv | Fisheye / catadioptric (FoV ~180°) |
| Double Sphere | `ds` | xi, alpha, fu, fv, cu, cv | Wide-angle fisheye (FoV ~195°) |
| Extended Unified | `eucm` | alpha, beta, fu, fv, cu, cv | Alternative fisheye model |

**Distortion Models**

| Model    | CLI name | Parameters          | Use Case                           |
|----------|----------|---------------------|------------------------------------|
| Radial-Tangential | `radtan` | k1, k2, r1, r2 | Standard lens, FoV < 120°    |
| Equidistant | `equi` | k1, k2, k3, k4  | Fisheye lens (preferred for wide) |
| Field-of-View | `fov` | w                | Simple single-param fisheye model  |
| None     | `none`   | —                   | Ideal/simulated cameras            |

**Choosing Your Model**

```
USB webcam (60–90° FoV):     pinhole-radtan
DSLR / action cam (90–120°): pinhole-radtan or pinhole-equi
Wide-angle / GoPro (120–180°): pinhole-equi
Fisheye surround cam (>180°): omni-radtan or ds-none
Jetson CSI camera (IMX219):  pinhole-radtan
Intel RealSense D435i:
  RGB: pinhole-radtan
  IR stereo: pinhole-radtan
```

---

**5. Installation**

**Method 1: Docker (Recommended — Works on Any Ubuntu)**

```bash
# Pull Kalibr Docker image
docker pull ghcr.io/ethz-asl/kalibr:latest

# Run interactively with shared folder for data/results
docker run -it \
    --volume /path/to/your/data:/data \
    --volume /tmp/.X11-unix:/tmp/.X11-unix \
    --env DISPLAY=$DISPLAY \
    ghcr.io/ethz-asl/kalibr:latest

# Inside container, Kalibr commands are available:
kalibr_calibrate_cameras --help
kalibr_calibrate_imu_camera --help
kalibr_create_target_pdf --help
```

</details>

### 方法 2：从源码构建（ROS2 Humble / Ubuntu 22.04）

```bash
# Install ROS2 Humble first (see Jetson guide)
sudo apt-get install -y \
    ros-humble-cv-bridge \
    ros-humble-image-transport \
    python3-catkin-tools \
    libopencv-dev \
    libsuitesparse-dev

# Create a catkin workspace (Kalibr still uses catkin, not colcon)
mkdir -p ~/kalibr_ws/src
cd ~/kalibr_ws/src
git clone https://github.com/ethz-asl/kalibr.git

cd ~/kalibr_ws
catkin build -DCMAKE_BUILD_TYPE=Release -j4
# Build takes 10–20 minutes

# Source the workspace
echo "source ~/kalibr_ws/devel/setup.bash" >> ~/.bashrc
source ~/.bashrc

# Verify
kalibr_calibrate_cameras --help
```

### 方法 3：在 Jetson Orin Nano 上构建

```bash
# Same as Method 2, but reduce parallel jobs to avoid OOM
catkin build -DCMAKE_BUILD_TYPE=Release -j2

# Kalibr calibration itself is CPU-intensive
# Run calibration on desktop/laptop, not on Jetson
# Only deploy calibration RESULTS (yaml files) to Jetson
```

### 验证安装

```bash
# Check all tools are available
kalibr_calibrate_cameras --help
kalibr_calibrate_imu_camera --help
kalibr_create_target_pdf --help
kalibr_bagcreater --help
kalibr_bag_extractor --help

# Create test target PDF
kalibr_create_target_pdf \
    --type apriltag --nx 6 --ny 6 \
    --tsize 0.088 --tspace 0.3
# Should produce: target.pdf
```

---

## 6. IMU 噪声模型 —— Allan 方差

在进行相机-IMU 校准之前，需要先拿到 IMU 噪声参数：陀螺仪与加速度计各自的 **noise density** 与 **bias random walk**。这些参数刻画了你手上这块具体 IMU 的噪声有多大。

### 这些参数的含义

```
Gyroscope noise density (gyr_n):   [rad/s/√Hz]
  White noise on angular rate measurement
  Higher → less trust in gyro in short-term

Gyroscope bias random walk (gyr_w): [rad/s²/√Hz]
  How fast the gyro bias drifts over time
  Higher → bias must be re-estimated more often

Accelerometer noise density (acc_n): [m/s²/√Hz]
  White noise on acceleration measurement

Accelerometer bias random walk (acc_w): [m/s³/√Hz]
  How fast accelerometer bias drifts

These directly scale the IMU terms in Kalibr's cost function.
Wrong values → poor calibration convergence or wrong result.
```

### Allan 方差法（正确做法）

```bash
# Step 1: Record IMU at rest for 2+ hours (the longer the better)
# Mount IMU on a vibration-isolated surface, do NOT touch it

# ROS2: record IMU topic
ros2 bag record -o imu_static_2h /imu/data

# Convert to ROS1 bag (allan_variance_ros uses ROS1)
# Or use the ROS2-compatible fork: github.com/ori-drs/allan_variance_ros

# Step 2: Install allan_variance_ros
cd ~/kalibr_ws/src
git clone https://github.com/ori-drs/allan_variance_ros.git
catkin build allan_variance_ros

# Step 3: Run Allan variance computation
rosrun allan_variance_ros allan_variance \
    [path_to_bag] \
    [imu_topic]   \
    [output_dir]

# Step 4: Plot Allan deviation to find noise floor and bias instability
rosrun allan_variance_ros plot_allan_variance \
    [output_dir]/allan_variance.csv
```

### 如何读 Allan 偏差图

```
Allan Deviation σ(τ)
    │
    │     Angle Random Walk (ARW)
    │      slope = -0.5
10⁻³│    \
    │     \____________________  ← Bias Instability (minimum point)
    │                            ← read gyr_w here (slope = +0.5)
10⁻⁵│
    └─────────────────────────────→ τ (seconds, log scale)
         1    10   100  1000

gyr_n  = σ at τ = 1s  (read from slope -0.5 line)
gyr_w  = σ at minimum × √(2*ln(2)/π)  (conversion factor)

For accelerometer: same plot, different units (m/s² vs rad/s)
```

### IMU YAML 文件示例

```yaml
# imu_mpu6050.yaml  — cheap IMU (MPU6050 on Jetson GPIO)
rostopic:     /imu/data
update_rate:  100       # Hz

accelerometer_noise_density:  0.0087  # [m/s^2/sqrt(Hz)]
accelerometer_random_walk:    0.00043 # [m/s^3/sqrt(Hz)]
gyroscope_noise_density:      0.0015  # [rad/s/sqrt(Hz)]
gyroscope_random_walk:        0.000019 # [rad/s^2/sqrt(Hz)]
```

```yaml
# imu_realsense_d435i.yaml — Intel RealSense D435i built-in IMU
rostopic:     /camera/imu
update_rate:  400       # Hz

accelerometer_noise_density:  0.0028  # factory spec
accelerometer_random_walk:    0.0003
gyroscope_noise_density:      0.00016
gyroscope_random_walk:        0.0000022
```

```yaml
# imu_xsens_mti30.yaml — high-end tactical IMU
rostopic:     /imu/data
update_rate:  400

accelerometer_noise_density:  0.002
accelerometer_random_walk:    0.00003
gyroscope_noise_density:      0.00003
gyroscope_random_walk:        0.000001
```

### 粗略估算（不使用 Allan Variance）

如果无法执行 2 小时静态测试，就使用厂商数据手册：

```python
# Convert datasheet specs to Kalibr units
# Datasheet: gyro noise = 0.005 °/s/√Hz

import math

gyro_noise_deg = 0.005    # °/s/√Hz from datasheet
gyr_n = gyro_noise_deg * math.pi / 180   # → rad/s/√Hz = 8.7e-5

# Datasheet: gyro bias instability = 2 °/hr
gyro_bias_deg_hr = 2.0
# bias random walk ≈ bias_instability / sqrt(3600) (rough approximation)
gyr_w = (gyro_bias_deg_hr / 3600) * math.pi / 180 / math.sqrt(3600)

print(f"gyr_n: {gyr_n:.2e}")
print(f"gyr_w: {gyr_w:.2e}")
# Use these as starting points, then tune if calibration fails to converge
```

---

## 7. 相机内参校准

### 数据采集

```bash
# Method A: OpenCV (simplest for intrinsics alone)
# Record a ROS bag while moving the target in front of the fixed camera

# Recommended motion pattern:
# 1. Fill the frame at different distances (close, medium, far)
# 2. Tilt the target left/right/up/down ~30° from camera axis
# 3. Move target to all corners of the frame
# 4. Include frames where target is partially visible (AprilGrid only)
# Duration: 2–3 minutes at slow, deliberate motion
# Frame rate: ~4 Hz (Kalibr --bag-freq flag sub-samples to this)

# Record with ROS2
ros2 bag record -o camera_calib /camera/image_raw

# Convert ROS2 bag to ROS1 format (Kalibr requires ROS1 bag)
# Install ros2bag_to_rosbag converter:
pip3 install rosbags
rosbags-convert camera_calib/ --dst camera_calib.bag
```

### 运行内参校准

```bash
# Single camera, pinhole-radtan model
kalibr_calibrate_cameras \
    --bag       camera_calib.bag \
    --topics    /camera/image_raw \
    --models    pinhole-radtan \
    --target    aprilgrid_6x6.yaml \
    --bag-freq  4.0 \
    --show-extraction

# --show-extraction: opens a window showing detected corners — check it!
# --bag-freq 4.0: sub-sample bag to 4 Hz (reduces redundant frames)

# Two cameras (stereo rig)
kalibr_calibrate_cameras \
    --bag       stereo_calib.bag \
    --topics    /cam0/image_raw /cam1/image_raw \
    --models    pinhole-radtan pinhole-radtan \
    --target    aprilgrid_6x6.yaml \
    --bag-freq  4.0

# Fisheye camera
kalibr_calibrate_cameras \
    --bag       fisheye_calib.bag \
    --topics    /camera/image_raw \
    --models    pinhole-equi \
    --target    aprilgrid_6x6.yaml \
    --bag-freq  4.0
```

### 输出文件

```
results-cam-camera_calib.txt     ← human-readable summary
report-cam-camera_calib.pdf      ← plots: corners, reprojection errors
camchain-camera_calib.yaml       ← machine-readable, input to next steps
```

### 读取 camchain.yaml（仅内参）

```yaml
# camchain-camera_calib.yaml
cam0:
  camera_model: pinhole
  intrinsics: [461.629, 460.152, 362.680, 246.049]
  #            fu       fv       cu       cv      (pixels)

  distortion_model: radtan
  distortion_coeffs: [-0.27695, 0.06712, 0.00100, 0.00020]
  #                   k1        k2        r1        r2

  resolution: [752, 480]
  #            width height

  T_cn_cnm1:
  # Only present for cam1, cam2... (extrinsic to previous camera)
  # cam0 is the reference (identity transform)
```

### 重投影误差质量判据

```
Reprojection error = distance between detected corner and projected corner

Good:    mean < 0.5 pixels, max < 1.5 pixels
OK:      mean < 1.0 pixel
Poor:    mean > 1.0 pixel → re-collect data or check target measurements

Common causes of high reprojection error:
  1. Wrong tagSize in YAML (re-measure with calipers)
  2. Motion blur (slow down, use faster shutter)
  3. Non-flat target (re-mount on rigid board)
  4. Too few poses / all poses at same distance
  5. Defocus / poor lighting
```

---

## 8. 多相机外参校准

### 多相机校准的要求

- **所有相机必须在足够多的帧中同时看到标靶**
- 如果使用中间相机作为桥接，相机之间无需有重叠 FoV：
  ```
  cam0 ↔ cam1 ↔ cam2   (cam0 and cam2 never see target together)
  Kalibr chains: T_cam2_cam0 = T_cam2_cam1 × T_cam1_cam0
  ```

### 多相机的数据采集

```bash
# Record all cameras synchronized
ros2 bag record -o multicam_calib \
    /cam0/image_raw \
    /cam1/image_raw \
    /cam2/image_raw \
    /cam3/image_raw   # surround-view setup

# Motion pattern for multi-camera:
# Move the TARGET (not the camera rig) in the shared FoV region
# Ensure each pair of adjacent cameras sees the target together
# Duration: 3–5 minutes

# Convert to ROS1 bag
rosbags-convert multicam_calib/ --dst multicam_calib.bag
```


<details>
<summary>English original</summary>

**Rough Estimates (Without Allan Variance)**

If you cannot run 2-hour static test, use manufacturer datasheets:

```python
# Convert datasheet specs to Kalibr units
# Datasheet: gyro noise = 0.005 °/s/√Hz

import math

gyro_noise_deg = 0.005    # °/s/√Hz from datasheet
gyr_n = gyro_noise_deg * math.pi / 180   # → rad/s/√Hz = 8.7e-5

# Datasheet: gyro bias instability = 2 °/hr
gyro_bias_deg_hr = 2.0
# bias random walk ≈ bias_instability / sqrt(3600) (rough approximation)
gyr_w = (gyro_bias_deg_hr / 3600) * math.pi / 180 / math.sqrt(3600)

print(f"gyr_n: {gyr_n:.2e}")
print(f"gyr_w: {gyr_w:.2e}")
# Use these as starting points, then tune if calibration fails to converge
```

---

**7. Camera Intrinsic Calibration**

**Data Collection**

```bash
# Method A: OpenCV (simplest for intrinsics alone)
# Record a ROS bag while moving the target in front of the fixed camera

# Recommended motion pattern:
# 1. Fill the frame at different distances (close, medium, far)
# 2. Tilt the target left/right/up/down ~30° from camera axis
# 3. Move target to all corners of the frame
# 4. Include frames where target is partially visible (AprilGrid only)
# Duration: 2–3 minutes at slow, deliberate motion
# Frame rate: ~4 Hz (Kalibr --bag-freq flag sub-samples to this)

# Record with ROS2
ros2 bag record -o camera_calib /camera/image_raw

# Convert ROS2 bag to ROS1 format (Kalibr requires ROS1 bag)
# Install ros2bag_to_rosbag converter:
pip3 install rosbags
rosbags-convert camera_calib/ --dst camera_calib.bag
```

**Run Intrinsic Calibration**

```bash
# Single camera, pinhole-radtan model
kalibr_calibrate_cameras \
    --bag       camera_calib.bag \
    --topics    /camera/image_raw \
    --models    pinhole-radtan \
    --target    aprilgrid_6x6.yaml \
    --bag-freq  4.0 \
    --show-extraction

# --show-extraction: opens a window showing detected corners — check it!
# --bag-freq 4.0: sub-sample bag to 4 Hz (reduces redundant frames)

# Two cameras (stereo rig)
kalibr_calibrate_cameras \
    --bag       stereo_calib.bag \
    --topics    /cam0/image_raw /cam1/image_raw \
    --models    pinhole-radtan pinhole-radtan \
    --target    aprilgrid_6x6.yaml \
    --bag-freq  4.0

# Fisheye camera
kalibr_calibrate_cameras \
    --bag       fisheye_calib.bag \
    --topics    /camera/image_raw \
    --models    pinhole-equi \
    --target    aprilgrid_6x6.yaml \
    --bag-freq  4.0
```

**Output Files**

```
results-cam-camera_calib.txt     ← human-readable summary
report-cam-camera_calib.pdf      ← plots: corners, reprojection errors
camchain-camera_calib.yaml       ← machine-readable, input to next steps
```

**Reading camchain.yaml (Intrinsics Only)**

```yaml
# camchain-camera_calib.yaml
cam0:
  camera_model: pinhole
  intrinsics: [461.629, 460.152, 362.680, 246.049]
  #            fu       fv       cu       cv      (pixels)

  distortion_model: radtan
  distortion_coeffs: [-0.27695, 0.06712, 0.00100, 0.00020]
  #                   k1        k2        r1        r2

  resolution: [752, 480]
  #            width height

  T_cn_cnm1:
  # Only present for cam1, cam2... (extrinsic to previous camera)
  # cam0 is the reference (identity transform)
```

**Reprojection Error Quality Criteria**

```
Reprojection error = distance between detected corner and projected corner

Good:    mean < 0.5 pixels, max < 1.5 pixels
OK:      mean < 1.0 pixel
Poor:    mean > 1.0 pixel → re-collect data or check target measurements

Common causes of high reprojection error:
  1. Wrong tagSize in YAML (re-measure with calipers)
  2. Motion blur (slow down, use faster shutter)
  3. Non-flat target (re-mount on rigid board)
  4. Too few poses / all poses at same distance
  5. Defocus / poor lighting
```

---

**8. Multi-Camera Extrinsic Calibration**

**Requirements for Multi-Camera Calibration**

- **All cameras must see the target simultaneously** in enough frames
- Cameras do not need overlapping FoV IF you use intermediate cameras as bridges:
  ```
  cam0 ↔ cam1 ↔ cam2   (cam0 and cam2 never see target together)
  Kalibr chains: T_cam2_cam0 = T_cam2_cam1 × T_cam1_cam0
  ```

**Data Collection for Multi-Camera**

```bash
# Record all cameras synchronized
ros2 bag record -o multicam_calib \
    /cam0/image_raw \
    /cam1/image_raw \
    /cam2/image_raw \
    /cam3/image_raw   # surround-view setup

# Motion pattern for multi-camera:
# Move the TARGET (not the camera rig) in the shared FoV region
# Ensure each pair of adjacent cameras sees the target together
# Duration: 3–5 minutes

# Convert to ROS1 bag
rosbags-convert multicam_calib/ --dst multicam_calib.bag
```

</details>

### 运行多相机校准

```bash
# 4-camera surround view (front, left, rear, right)
kalibr_calibrate_cameras \
    --bag       multicam_calib.bag \
    --topics    /cam0/image_raw \
                /cam1/image_raw \
                /cam2/image_raw \
                /cam3/image_raw \
    --models    pinhole-equi \
                pinhole-equi \
                pinhole-equi \
                pinhole-equi \
    --target    aprilgrid_6x6.yaml \
    --bag-freq  4.0 \
    --dont-show-report

# Output: camchain-multicam_calib.yaml with T_cn_cnm1 for each camera pair
```

### 多相机系统的 camchain.yaml

```yaml
# camchain-multicam_calib.yaml
cam0:
  camera_model: pinhole
  intrinsics: [fu0, fv0, cu0, cv0]
  distortion_model: equi
  distortion_coeffs: [k1, k2, k3, k4]
  resolution: [1280, 720]
  T_cn_cnm1:   # identity (cam0 is reference frame)
  - [1, 0, 0, 0]
  - [0, 1, 0, 0]
  - [0, 0, 1, 0]
  - [0, 0, 0, 1]

cam1:   # mounted to the left of cam0
  camera_model: pinhole
  intrinsics: [fu1, fv1, cu1, cv1]
  distortion_model: equi
  distortion_coeffs: [k1, k2, k3, k4]
  resolution: [1280, 720]
  T_cn_cnm1:   # T_cam1_cam0: transform from cam0 frame to cam1 frame
  - [ 0.9998, -0.0098,  0.0181,  0.1205]  # rotation + translation
  - [ 0.0102,  0.9999, -0.0076, -0.0032]
  - [-0.0180,  0.0078,  0.9998, -0.0015]
  - [ 0.0000,  0.0000,  0.0000,  1.0000]

cam2:   # T_cam2_cam1
  ...

cam3:   # T_cam3_cam2
  ...
```

### 把 T_cn_cnm1 转换成你需要的变换

```python
# calibration_utils.py
import numpy as np

def load_camchain(yaml_path):
    import yaml
    with open(yaml_path) as f:
        chain = yaml.safe_load(f)
    return chain

def get_T_cam_cam0(chain, cam_idx):
    """
    Get T_camN_cam0: the transform that maps a point in cam0 frame
    to camN frame.  (= T_camN_camN-1 @ ... @ T_cam1_cam0)
    """
    T = np.eye(4)
    for i in range(1, cam_idx + 1):
        T_step = np.array(chain[f'cam{i}']['T_cn_cnm1'])
        T = T_step @ T
    return T

# Example: get transform from cam0 to cam3
chain = load_camchain('camchain-multicam.yaml')
T_cam3_cam0 = get_T_cam_cam0(chain, 3)

# Transform a 3D point from cam0 to cam3 frame
point_cam0 = np.array([1.0, 0.5, 3.0, 1.0])   # homogeneous
point_cam3 = T_cam3_cam0 @ point_cam0
print(f"Point in cam3 frame: {point_cam3[:3]}")
```

---

## 9. 相机-IMU 校准

对于使用 Visual-Inertial Odometry（VIO）、SLAM，或任何融合相机与 IMU 数据的系统，这是自动驾驶汽车最重要的一项校准。

Kalibr 估计：
- **T_imu_cam** — 刚体变换：相机在 IMU 坐标系中的位置与姿态
- **time_offset** — 相机曝光与 IMU 时间戳之间的时间延迟（可达 ±10ms）

### 数据采集 — 关键细节

```bash
# Setup:
# 1. Mount camera + IMU rigidly together — no flex, no vibration
# 2. Fix the AprilGrid target to a WALL or rigid stand
# 3. Move the camera-IMU assembly in front of the fixed target

# Motion requirements (ALL must be excited):
# Translation: X, Y, Z axis independently
# Rotation:    Roll, Pitch, Yaw independently
# Fast enough to excite IMU, slow enough to avoid blur

# Recommended motion script (2–3 minutes):
# Phase 1: slow rotations (roll 45°, pitch 45°, yaw 90°) — 30 seconds
# Phase 2: slow translations (±20cm in X, Y, Z) — 30 seconds
# Phase 3: figure-8 pattern combining rotation + translation — 60 seconds
# Phase 4: random vigorous movement — 30 seconds

# Camera rate: 20 Hz
# IMU rate: 200 Hz minimum (400 Hz preferred)

# Important: LOW MOTION BLUR
# Use short exposure time: < 1/500s
# Good lighting: LED panel or natural light
# Avoid fluorescent lights (50/60 Hz flicker)

# Record all topics
ros2 bag record -o cam_imu_calib \
    /camera/image_raw \
    /imu/data

# Convert to ROS1 bag
rosbags-convert cam_imu_calib/ --dst cam_imu_calib.bag

# Verify bag contents
rosbag info cam_imu_calib.bag
# Check: camera at ~20 Hz, IMU at ~200 Hz, no gaps in IMU stream
```

### 验证 IMU 频率

```bash
# Check IMU topic rate in ROS1 bag
rosbag play cam_imu_calib.bag &
rostopic hz /imu/data
# Must be stable at your declared update_rate (200 or 400 Hz)
# Irregular IMU timestamps are a common cause of calibration failure

# Check timestamp alignment (rough — within a few seconds is fine)
python3 -c "
import rosbag
bag = rosbag.Bag('cam_imu_calib.bag')
cam_times = [t.to_sec() for _, _, t in bag.read_messages('/camera/image_raw')]
imu_times = [t.to_sec() for _, _, t in bag.read_messages('/imu/data')]
print(f'Camera: {len(cam_times)} frames, {cam_times[-1]-cam_times[0]:.1f}s')
print(f'IMU:    {len(imu_times)} samples, {imu_times[-1]-imu_times[0]:.1f}s')
print(f'IMU rate: {len(imu_times)/(imu_times[-1]-imu_times[0]):.1f} Hz')
bag.close()
"
```


<details>
<summary>English original</summary>

**Run Multi-Camera Calibration**

```bash
# 4-camera surround view (front, left, rear, right)
kalibr_calibrate_cameras \
    --bag       multicam_calib.bag \
    --topics    /cam0/image_raw \
                /cam1/image_raw \
                /cam2/image_raw \
                /cam3/image_raw \
    --models    pinhole-equi \
                pinhole-equi \
                pinhole-equi \
                pinhole-equi \
    --target    aprilgrid_6x6.yaml \
    --bag-freq  4.0 \
    --dont-show-report

# Output: camchain-multicam_calib.yaml with T_cn_cnm1 for each camera pair
```

**camchain.yaml for Multi-Camera Rig**

```yaml
# camchain-multicam_calib.yaml
cam0:
  camera_model: pinhole
  intrinsics: [fu0, fv0, cu0, cv0]
  distortion_model: equi
  distortion_coeffs: [k1, k2, k3, k4]
  resolution: [1280, 720]
  T_cn_cnm1:   # identity (cam0 is reference frame)
  - [1, 0, 0, 0]
  - [0, 1, 0, 0]
  - [0, 0, 1, 0]
  - [0, 0, 0, 1]

cam1:   # mounted to the left of cam0
  camera_model: pinhole
  intrinsics: [fu1, fv1, cu1, cv1]
  distortion_model: equi
  distortion_coeffs: [k1, k2, k3, k4]
  resolution: [1280, 720]
  T_cn_cnm1:   # T_cam1_cam0: transform from cam0 frame to cam1 frame
  - [ 0.9998, -0.0098,  0.0181,  0.1205]  # rotation + translation
  - [ 0.0102,  0.9999, -0.0076, -0.0032]
  - [-0.0180,  0.0078,  0.9998, -0.0015]
  - [ 0.0000,  0.0000,  0.0000,  1.0000]

cam2:   # T_cam2_cam1
  ...

cam3:   # T_cam3_cam2
  ...
```

**Converting T_cn_cnm1 to the Transform You Need**

```python
# calibration_utils.py
import numpy as np

def load_camchain(yaml_path):
    import yaml
    with open(yaml_path) as f:
        chain = yaml.safe_load(f)
    return chain

def get_T_cam_cam0(chain, cam_idx):
    """
    Get T_camN_cam0: the transform that maps a point in cam0 frame
    to camN frame.  (= T_camN_camN-1 @ ... @ T_cam1_cam0)
    """
    T = np.eye(4)
    for i in range(1, cam_idx + 1):
        T_step = np.array(chain[f'cam{i}']['T_cn_cnm1'])
        T = T_step @ T
    return T

# Example: get transform from cam0 to cam3
chain = load_camchain('camchain-multicam.yaml')
T_cam3_cam0 = get_T_cam_cam0(chain, 3)

# Transform a 3D point from cam0 to cam3 frame
point_cam0 = np.array([1.0, 0.5, 3.0, 1.0])   # homogeneous
point_cam3 = T_cam3_cam0 @ point_cam0
print(f"Point in cam3 frame: {point_cam3[:3]}")
```

---

**9. Camera-IMU Calibration**

This is the most important calibration for autonomous vehicles using Visual-Inertial Odometry (VIO), SLAM, or any system that combines camera and IMU data.

Kalibr estimates:
- **T_imu_cam** — rigid body transform: position and orientation of camera in IMU frame
- **time_offset** — time delay between camera exposure and IMU timestamp (can be ±10ms)

**Data Collection — Critical Details**

```bash
# Setup:
# 1. Mount camera + IMU rigidly together — no flex, no vibration
# 2. Fix the AprilGrid target to a WALL or rigid stand
# 3. Move the camera-IMU assembly in front of the fixed target

# Motion requirements (ALL must be excited):
# Translation: X, Y, Z axis independently
# Rotation:    Roll, Pitch, Yaw independently
# Fast enough to excite IMU, slow enough to avoid blur

# Recommended motion script (2–3 minutes):
# Phase 1: slow rotations (roll 45°, pitch 45°, yaw 90°) — 30 seconds
# Phase 2: slow translations (±20cm in X, Y, Z) — 30 seconds
# Phase 3: figure-8 pattern combining rotation + translation — 60 seconds
# Phase 4: random vigorous movement — 30 seconds

# Camera rate: 20 Hz
# IMU rate: 200 Hz minimum (400 Hz preferred)

# Important: LOW MOTION BLUR
# Use short exposure time: < 1/500s
# Good lighting: LED panel or natural light
# Avoid fluorescent lights (50/60 Hz flicker)

# Record all topics
ros2 bag record -o cam_imu_calib \
    /camera/image_raw \
    /imu/data

# Convert to ROS1 bag
rosbags-convert cam_imu_calib/ --dst cam_imu_calib.bag

# Verify bag contents
rosbag info cam_imu_calib.bag
# Check: camera at ~20 Hz, IMU at ~200 Hz, no gaps in IMU stream
```

**Verify IMU Frequency**

```bash
# Check IMU topic rate in ROS1 bag
rosbag play cam_imu_calib.bag &
rostopic hz /imu/data
# Must be stable at your declared update_rate (200 or 400 Hz)
# Irregular IMU timestamps are a common cause of calibration failure

# Check timestamp alignment (rough — within a few seconds is fine)
python3 -c "
import rosbag
bag = rosbag.Bag('cam_imu_calib.bag')
cam_times = [t.to_sec() for _, _, t in bag.read_messages('/camera/image_raw')]
imu_times = [t.to_sec() for _, _, t in bag.read_messages('/imu/data')]
print(f'Camera: {len(cam_times)} frames, {cam_times[-1]-cam_times[0]:.1f}s')
print(f'IMU:    {len(imu_times)} samples, {imu_times[-1]-imu_times[0]:.1f}s')
print(f'IMU rate: {len(imu_times)/(imu_times[-1]-imu_times[0]):.1f} Hz')
bag.close()
"
```

</details>

### 运行 Camera-IMU 校准

```bash
# Step 1: camera intrinsics must exist first
# Use camchain from Section 7 as input

# Run calibration
kalibr_calibrate_imu_camera \
    --bag     cam_imu_calib.bag \
    --cam     camchain-camera_calib.yaml \
    --imu     imu_mpu6050.yaml \
    --target  aprilgrid_6x6.yaml \
    --bag-freq 20.0 \
    --time-calibration

# --time-calibration: estimate time offset between camera and IMU (important!)
# --show-extraction: visualize corner detection (add for debugging)
# Remove --time-calibration if IMU and camera are hardware-synchronized (e.g., via trigger)

# For multiple cameras + IMU
kalibr_calibrate_imu_camera \
    --bag     cam_imu_calib.bag \
    --cam     camchain-multicam.yaml \
    --imu     imu.yaml \
    --target  aprilgrid_6x6.yaml \
    --bag-freq 20.0 \
    --time-calibration
```

### Camera-IMU 校准输出

```yaml
# camchain-imucam-cam_imu_calib.yaml
cam0:
  camera_model: pinhole
  intrinsics: [461.629, 460.152, 362.680, 246.049]
  distortion_model: radtan
  distortion_coeffs: [-0.277, 0.067, 0.001, 0.000]
  resolution: [752, 480]

  T_cam_imu:             # ← THE KEY RESULT
  - [ 0.01479, -0.99977,  0.01553, -0.00288]   # rotation matrix | translation
  - [ 0.99985,  0.01502, -0.00891, -0.07294]   #                  | (meters)
  - [ 0.00914,  0.01540,  0.99984, -0.00745]
  - [ 0.00000,  0.00000,  0.00000,  1.00000]

  timeshift_cam_imu: -0.0040   # seconds: IMU is 4ms ahead of camera timestamp
  #                              Positive = camera leads IMU
  #                              Negative = IMU leads camera
```

### 理解 T_cam_imu

```
T_cam_imu transforms a point expressed in IMU frame to camera frame:

  p_cam = T_cam_imu × p_imu

To go the other direction (point in camera frame → IMU frame):
  T_imu_cam = inv(T_cam_imu)
  p_imu = T_imu_cam × p_cam

In practice, you need:
  - VIO (VINS-Mono, OKVIS, ORB-SLAM3): T_cam_imu directly
  - BEVFusion: needs T_cam_lidar AND T_imu_lidar (from LiDAR-IMU calib)
  - ROS2 TF tree: publish T_cam_imu as a static transform
```

```python
# Use the calibration result
import numpy as np
import yaml

def load_cam_imu_calib(yaml_path, cam_name='cam0'):
    with open(yaml_path) as f:
        data = yaml.safe_load(f)
    cam = data[cam_name]
    T_cam_imu = np.array(cam['T_cam_imu'])
    timeshift  = cam.get('timeshift_cam_imu', 0.0)
    intrinsics = cam['intrinsics']   # [fu, fv, cu, cv]
    dist_coeffs = cam['distortion_coeffs']
    return T_cam_imu, timeshift, intrinsics, dist_coeffs

T_cam_imu, dt, K_params, dist = load_cam_imu_calib(
    'camchain-imucam-cam_imu_calib.yaml'
)
T_imu_cam = np.linalg.inv(T_cam_imu)

print(f"Camera-IMU rotation:\n{T_cam_imu[:3,:3]}")
print(f"Camera-IMU translation: {T_cam_imu[:3,3]*100:.2f} cm")
print(f"Time offset: {dt*1000:.2f} ms (camera relative to IMU)")
```

---

## 10. 卷帘快门校准

许多相机（USB 摄像头、手机摄像头、部分车载摄像头）使用卷帘快门 —— 每一行的曝光时刻略有不同。这在运动过程中会造成“果冻效应”。

Kalibr 可以估计**读出时间**（第一行到最后一行曝光之间的时间）。

```bash
# Rolling shutter calibration requires a MOVING TARGET approach
# (opposite to global shutter: you move the camera, not the target)
# Actually: both methods work — just need sufficient motion

kalibr_calibrate_cameras \
    --bag       rolling_calib.bag \
    --topics    /camera/image_raw \
    --models    pinhole-radtan \
    --target    aprilgrid_6x6.yaml \
    --approx-synced \
    --bag-freq  4.0

# For rolling shutter, add:
# --camera-models rs-pinhole-radtan   (rs = rolling shutter)
# (not all Kalibr versions support this — check your version)
```

---

## 11. 读取并验证结果

### 重投影误差分析

```python
# parse_kalibr_report.py
# Parse the results text file for quick quality check

import re

def parse_results(results_txt):
    with open(results_txt) as f:
        text = f.read()

    # Extract reprojection errors per camera
    errors = {}
    for match in re.finditer(
        r'cam(\d+).*?Reprojection error.*?mean:\s*([\d.]+).*?max:\s*([\d.]+)',
        text, re.DOTALL
    ):
        cam_id = int(match.group(1))
        errors[f'cam{cam_id}'] = {
            'mean': float(match.group(2)),
            'max':  float(match.group(3))
        }

    return errors

errors = parse_results('results-cam-calib.txt')
for cam, e in errors.items():
    status = '✓' if e['mean'] < 0.5 else ('⚠' if e['mean'] < 1.0 else '✗')
    print(f"{status} {cam}: mean={e['mean']:.3f}px  max={e['max']:.3f}px")
```

### 视觉验证：将 LiDAR 投影到相机

```python
# validate_cam_lidar.py
import numpy as np
import cv2

def project_lidar_to_camera(points_lidar, T_cam_lidar, K, dist_coeffs, img_shape):
    """
    Project LiDAR points onto camera image using calibration results.
    Visual check: LiDAR points should land on the correct objects in the image.
    """
    N = points_lidar.shape[0]
    pts_h = np.hstack([points_lidar[:, :3], np.ones((N, 1))])  # [N, 4]
    pts_cam = (T_cam_lidar @ pts_h.T).T                          # [N, 4] cam frame

    # Keep only points in front of camera
    mask = pts_cam[:, 2] > 0.1
    pts_cam = pts_cam[mask]
    depths  = pts_cam[:, 2]

    # Project using camera intrinsics
    fu, fv, cu, cv = K
    u = (pts_cam[:, 0] / depths) * fu + cu
    v = (pts_cam[:, 1] / depths) * fv + cv

    # Keep only points within image bounds
    H, W = img_shape[:2]
    in_frame = (u >= 0) & (u < W) & (v >= 0) & (v < H)
    u, v, depths = u[in_frame], v[in_frame], depths[in_frame]

    return u.astype(int), v.astype(int), depths

# Load calibration
K_params = [461.629, 460.152, 362.680, 246.049]  # fu, fv, cu, cv
T_cam_lidar = np.array([...])  # from your LiDAR-camera extrinsic calibration

# Load image and point cloud
img    = cv2.imread('test_frame.png')
points = np.fromfile('test_scan.bin', dtype=np.float32).reshape(-1, 4)

u, v, depths = project_lidar_to_camera(points, T_cam_lidar, K_params, None, img.shape)

# Color points by depth (near=red, far=blue)
depth_norm = np.clip(depths / 50.0, 0, 1)
for i in range(len(u)):
    color = (int(255*(1-depth_norm[i])), 0, int(255*depth_norm[i]))
    cv2.circle(img, (u[i], v[i]), 2, color, -1)

cv2.imshow('LiDAR on Camera — validate calibration', img)
cv2.waitKey(0)

# GOOD: LiDAR points align with object edges in image
# BAD:  LiDAR points float above/below objects → wrong extrinsics
```

### 将校准结果发布为 ROS2 静态变换

```python
# publish_calibration_tf.py
import rclpy
from rclpy.node import Node
from tf2_ros import StaticTransformBroadcaster
from geometry_msgs.msg import TransformStamped
import numpy as np
import yaml
from scipy.spatial.transform import Rotation

class CalibrationPublisher(Node):
    def __init__(self, camchain_path):
        super().__init__('calibration_publisher')
        self.tf_broadcaster = StaticTransformBroadcaster(self)

        with open(camchain_path) as f:
            chain = yaml.safe_load(f)

        transforms = []

        # Publish camera-IMU transform
        for cam_name, cam_data in chain.items():
            if 'T_cam_imu' not in cam_data:
                continue

            T = np.array(cam_data['T_cam_imu'])
            T_imu_cam = np.linalg.inv(T)

            ts = TransformStamped()
            ts.header.stamp = self.get_clock().now().to_msg()
            ts.header.frame_id = 'imu'
            ts.child_frame_id  = cam_name

            # Translation
            ts.transform.translation.x = T_imu_cam[0, 3]
            ts.transform.translation.y = T_imu_cam[1, 3]
            ts.transform.translation.z = T_imu_cam[2, 3]

            # Rotation → quaternion
            R = Rotation.from_matrix(T_imu_cam[:3, :3])
            q = R.as_quat()   # [x, y, z, w]
            ts.transform.rotation.x = q[0]
            ts.transform.rotation.y = q[1]
            ts.transform.rotation.z = q[2]
            ts.transform.rotation.w = q[3]

            transforms.append(ts)

        self.tf_broadcaster.sendTransform(transforms)
        self.get_logger().info(f'Published {len(transforms)} static calibration transforms')

def main():
    rclpy.init()
    node = CalibrationPublisher('camchain-imucam-calib.yaml')
    rclpy.spin(node)
    rclpy.shutdown()
```

---

## 12. 在下游系统中使用校准

### BEVFusion（相机-LiDAR 融合）

BEVFusion 需要相机内参和相机到 LiDAR 的外参。LiDAR-相机外参并非直接来自 Kalibr（Kalibr 不校准 LiDAR），而是由 Kalibr 的相机-IMU 结果与 LiDAR-IMU 结果组合得到：

```python
# T_cam_lidar = T_cam_imu @ T_imu_lidar
T_cam_imu   = load_kalibr_cam_imu('camchain-imucam.yaml')
T_lidar_imu = load_lidar_imu_calib('lidar_imu_calib.yaml')  # from LI-Calib or similar

T_imu_lidar = np.linalg.inv(T_lidar_imu)
T_cam_lidar = T_cam_imu @ T_imu_lidar

# In BEVFusion config:
data:
  cameras:
    CAM_FRONT:
      camera_model: pinhole
      intrinsics:
        fx: 461.629
        fy: 460.152
        cx: 362.680
        cy: 246.049
      distortion: [-0.277, 0.067, 0.001, 0.000]
      extrinsic:  # T_cam_lidar as 4×4
        - [R00, R01, R02, tx]
        - [R10, R11, R12, ty]
        - [R20, R21, R22, tz]
        - [0,   0,   0,   1 ]
```

### OpenVINS / VINS-Mono（视觉惯性里程计）

Kalibr 的输出可直接映射为 VIO 配置：

```yaml
# openvins config.yaml
camera_intrinsics:
    - [461.629, 460.152, 362.680, 246.049]   # fu, fv, cu, cv

camera_distortion_coeffs:
    - [-0.277, 0.067, 0.001, 0.000]

T_imu_cam0:              # use T_imu_cam (inverse of Kalibr's T_cam_imu)
    rows: 4
    cols: 4
    data: [R00, R01, R02, tx,
           R10, R11, R12, ty,
           R20, R21, R22, tz,
           0,   0,   0,   1]

timeshift_cam_imu: -0.004   # copy from Kalibr output
```

### ORB-SLAM3（单目/双目/IMU）

```yaml
# ORB_SLAM3/config/EuRoC.yaml
Camera.type: "PinHole"
Camera.fx: 461.629
Camera.fy: 460.152
Camera.cx: 362.680
Camera.cy: 246.049
Camera.k1: -0.277
Camera.k2:  0.067
Camera.p1:  0.001
Camera.p2:  0.000

# IMU-Camera extrinsic (T_bc = T_cam_imu in Kalibr notation)
Tbc: !!opencv-matrix
   rows: 4
   cols: 4
   dt: f
   data: [R00, R01, R02, tx, R10, R11, R12, ty, R20, R21, R22, tz, 0, 0, 0, 1]

IMU.NoiseGyro:  0.00016     # gyr_n from imu.yaml
IMU.NoiseAcc:   0.0028      # acc_n
IMU.GyroWalk:   0.0000022   # gyr_w
IMU.AccWalk:    0.0003      # acc_w
IMU.Frequency:  400
```

### Openpilot / comma.ai

Openpilot 的感知模型使用相机校准。内参矩阵嵌入在模型的 normalization layer 中，但对于自定义硬件，需通过以下方式提供：

```python
# In openpilot params (simplified)
from common.params import Params
import json, numpy as np

K = np.array([
    [461.629,   0,     362.680],
    [  0,     460.152, 246.049],
    [  0,       0,       1    ]
])

Params().put('CalibrationParams', json.dumps({
    'rpyCalib': [0.0, 0.0, 0.0],   # roll, pitch, yaw offset from Kalibr extrinsic
    'wideCameraOnly': False,
}))
```

---

## 13. 常见陷阱与调试

### 陷阱 1：YAML 中的 tagSize 错误

```
Symptom:  Calibration succeeds but depth estimates are systematically off
          LiDAR points project onto wrong locations in image
Cause:    tagSize in YAML does not match the PRINTED target size
Fix:      Re-measure with digital calipers AFTER printing
          Printers scale to fit page → actual size ≠ intended size
```

### 陷阱 2：图像模糊

```
Symptom:  "Could not find enough corners" or high reprojection error
Cause:    Motion blur, out-of-focus, or dark corners
Fix:      - Use shorter exposure (< 1/500s)
          - Add more lighting (LED panel)
          - Move target more slowly
          - Check focus: target must be sharp at the calibration distances
```

### 陷阱 3：IMU 时间戳抖动

```
Symptom:  Camera-IMU calibration fails to converge
          "IMU integration error is very large"
Cause:    Irregular IMU timestamps (USB latency, poor driver)
Fix:      - Check with: rostopic hz /imu/data (should be stable ±1%)
          - Use hardware-timestamped IMU (SPI/I2C directly, not USB)
          - Add --time-calibration flag (Kalibr estimates the offset)
          - If jitter > 1ms: fix the IMU driver or hardware first
```

### 陷阱 4：Camera-IMU 运动不足

```
Symptom:  Large uncertainty on T_imu_cam rotation
          "Warning: observability is poor"
Cause:    Motion did not excite all 6 DOF
Fix:      - Record a new bag with deliberate roll/pitch/yaw rotations
          - Each axis must be rotated at least 45°
          - Add fast, sharp jerks (excites accelerometer)
          - Motion must be fast enough to generate non-trivial IMU signal
```

### 陷阱 5：相机 FoV 不重叠

```
Symptom:  "No common observations between cam0 and cam2"
Cause:    Cameras don't share enough target visibility
Fix:      - Use intermediate cameras as bridges
          - Record while overlapping FoV region of adjacent camera pairs
          - Reduce --bag-freq to include more diverse frames
```

### 陷阱 6：多次运行结果不一致

```
Symptom:  T_cam_imu differs by >5° between two calibration runs
Cause:    Insufficient data, or target/sensor moved between recording
Fix:      - Collect longer bags (3+ minutes for camera-IMU)
          - Do not change camera focus or IMU mounting between sessions
          - Use --show-extraction to verify corners are detected consistently
          - Average results of 3 calibration runs (take median quaternion)
```


<details>
<summary>English original</summary>

**OpenVINS / VINS-Mono (Visual-Inertial Odometry)**

Kalibr output maps directly to VIO config:

```yaml
# openvins config.yaml
camera_intrinsics:
    - [461.629, 460.152, 362.680, 246.049]   # fu, fv, cu, cv

camera_distortion_coeffs:
    - [-0.277, 0.067, 0.001, 0.000]

T_imu_cam0:              # use T_imu_cam (inverse of Kalibr's T_cam_imu)
    rows: 4
    cols: 4
    data: [R00, R01, R02, tx,
           R10, R11, R12, ty,
           R20, R21, R22, tz,
           0,   0,   0,   1]

timeshift_cam_imu: -0.004   # copy from Kalibr output
```

**ORB-SLAM3 (Monocular/Stereo/IMU)**

```yaml
# ORB_SLAM3/config/EuRoC.yaml
Camera.type: "PinHole"
Camera.fx: 461.629
Camera.fy: 460.152
Camera.cx: 362.680
Camera.cy: 246.049
Camera.k1: -0.277
Camera.k2:  0.067
Camera.p1:  0.001
Camera.p2:  0.000

# IMU-Camera extrinsic (T_bc = T_cam_imu in Kalibr notation)
Tbc: !!opencv-matrix
   rows: 4
   cols: 4
   dt: f
   data: [R00, R01, R02, tx, R10, R11, R12, ty, R20, R21, R22, tz, 0, 0, 0, 1]

IMU.NoiseGyro:  0.00016     # gyr_n from imu.yaml
IMU.NoiseAcc:   0.0028      # acc_n
IMU.GyroWalk:   0.0000022   # gyr_w
IMU.AccWalk:    0.0003      # acc_w
IMU.Frequency:  400
```

**Openpilot / comma.ai**

Openpilot uses camera calibration for its perception model. The intrinsic matrix is embedded in the model's normalization layer, but for custom hardware you provide it via:

```python
# In openpilot params (simplified)
from common.params import Params
import json, numpy as np

K = np.array([
    [461.629,   0,     362.680],
    [  0,     460.152, 246.049],
    [  0,       0,       1    ]
])

Params().put('CalibrationParams', json.dumps({
    'rpyCalib': [0.0, 0.0, 0.0],   # roll, pitch, yaw offset from Kalibr extrinsic
    'wideCameraOnly': False,
}))
```

---

**13. Common Pitfalls and Debugging**

**Pitfall 1: Wrong tagSize in YAML**

```
Symptom:  Calibration succeeds but depth estimates are systematically off
          LiDAR points project onto wrong locations in image
Cause:    tagSize in YAML does not match the PRINTED target size
Fix:      Re-measure with digital calipers AFTER printing
          Printers scale to fit page → actual size ≠ intended size
```

**Pitfall 2: Blurry Images**

```
Symptom:  "Could not find enough corners" or high reprojection error
Cause:    Motion blur, out-of-focus, or dark corners
Fix:      - Use shorter exposure (< 1/500s)
          - Add more lighting (LED panel)
          - Move target more slowly
          - Check focus: target must be sharp at the calibration distances
```

**Pitfall 3: IMU Timestamp Jitter**

```
Symptom:  Camera-IMU calibration fails to converge
          "IMU integration error is very large"
Cause:    Irregular IMU timestamps (USB latency, poor driver)
Fix:      - Check with: rostopic hz /imu/data (should be stable ±1%)
          - Use hardware-timestamped IMU (SPI/I2C directly, not USB)
          - Add --time-calibration flag (Kalibr estimates the offset)
          - If jitter > 1ms: fix the IMU driver or hardware first
```

**Pitfall 4: Insufficient Motion for Camera-IMU**

```
Symptom:  Large uncertainty on T_imu_cam rotation
          "Warning: observability is poor"
Cause:    Motion did not excite all 6 DOF
Fix:      - Record a new bag with deliberate roll/pitch/yaw rotations
          - Each axis must be rotated at least 45°
          - Add fast, sharp jerks (excites accelerometer)
          - Motion must be fast enough to generate non-trivial IMU signal
```

**Pitfall 5: Non-Overlapping Camera FoVs**

```
Symptom:  "No common observations between cam0 and cam2"
Cause:    Cameras don't share enough target visibility
Fix:      - Use intermediate cameras as bridges
          - Record while overlapping FoV region of adjacent camera pairs
          - Reduce --bag-freq to include more diverse frames
```

**Pitfall 6: Inconsistent Results Between Runs**

```
Symptom:  T_cam_imu differs by >5° between two calibration runs
Cause:    Insufficient data, or target/sensor moved between recording
Fix:      - Collect longer bags (3+ minutes for camera-IMU)
          - Do not change camera focus or IMU mounting between sessions
          - Use --show-extraction to verify corners are detected consistently
          - Average results of 3 calibration runs (take median quaternion)
```

</details>

### 调试检查清单

```
Before recording:
  □ tagSize measured with calipers and updated in YAML
  □ Target mounted flat on rigid surface
  □ Camera in focus at the working distance
  □ IMU frequency verified (rostopic hz)
  □ Good lighting, no motion blur at planned speeds

During recording:
  □ All cameras seeing target in every frame (for extrinsic)
  □ IMU and camera recording simultaneously
  □ Sufficient motion diversity (all 6 DOF for camera-IMU)
  □ No gaps or drops in IMU stream

After calibration:
  □ Reprojection error < 0.5px (intrinsic)
  □ Report PDF reviewed — corner detections look correct
  □ T_cam_imu makes physical sense (matches physical measurement roughly)
  □ Timeshift within expected range (|dt| < 50ms for software-triggered systems)
  □ Validate by projecting LiDAR on camera image
```

---

## 14. 项目

### 项目 1：完整校准链
从带有 CSI 摄像头（IMX219）和 MPU6050 IMU 的 Jetson Orin Nano 开始：
1. 录制 2 小时 IMU 静态 bag → 计算 Allan 方差 → 填充 `imu.yaml`
2. 打印 6×6 AprilGrid 并贴到泡沫板上，测量 tagSize
3. 录制相机内参 bag → 运行 `kalibr_calibrate_cameras`
4. 录制 camera-IMU bag → 运行 `kalibr_calibrate_imu_camera`
5. 验证：重投影误差 < 0.5px，时间偏移 < 30ms

### 项目 2：双目相机校准
校准一对双目相机（两个 CSI 摄像头或 USB 网络摄像头）。将结果与 OpenCV 的 `stereoCalibrate` 比较。用卡尺测量基线（两相机之间的距离），并验证 Kalibr 估计的平移量在 2mm 以内吻合。

### 项目 3：RealSense D435i 校准
D435i 内置 IMU、RGB 相机和 IR 双目相机。用 Kalibr 校准 RGB-IMU 对。与 Intel 的出厂校准值（存于设备上）比较。量化差异。

### 项目 4：校准质量 vs 数据时长
录制三个 bag：1 分钟、3 分钟、5 分钟。对每个分别运行 camera-IMU 校准。绘图：估计的 T_cam_imu 不确定度 vs 录制时长。找出达到目标不确定度所需的最少数据量。

### 项目 5：集成到 BEVFusion
用 Kalibr 的结果为自定义 camera+LiDAR 设备配置 BEVFusion。运行可视化验证（将 LiDAR 投影到相机）。比较 BEVFusion 的检测准确率与：
- 默认/单位外参（未校准）
- 粗略手工测量
- Kalibr 校准结果

### 项目 6：校准验证工具
编写一个 Python 脚本，实现：
1. 加载 `camchain-imucam.yaml`
2. 加载一个短的验证 ROS bag（与校准 bag 不同）
3. 在相机帧中检测 AprilGrid 角点
4. 通过 T_cam_imu 投影检测到的角点 3D 位置
5. 在留出数据上计算重投影误差
6. 以可配置的阈值报告通过/失败

---

## 15. 资源

### 官方
- **Kalibr GitHub** — github.com/ethz-asl/kalibr：源代码、Docker、issues
- **Kalibr Wiki** — github.com/ethz-asl/kalibr/wiki：全部文档页面
- **Kalibr 论文：**“A Toolbox for Easily Calibrating Onboard Cameras”（Furgale 等，IROS 2013）
- **Camera-IMU 论文：**“Unified Temporal and Spatial Calibration for Multi-Sensor Systems”（Furgale 等，IROS 2013）

### Allan 方差
- **allan_variance_ros** — github.com/ori-drs/allan_variance_ros：用于 IMU 噪声特性刻画的 ROS 包
- **IEEE Std 952-1997**：IMU 特性刻画中 Allan 方差的正式定义

### 相关校准工具
- **LI-Calib** — github.com/APRIL-ZJU/LI-Calib：LiDAR-IMU 校准（将其结果与 Kalibr 的 camera-IMU 结果配合，得到完整的 camera-LiDAR-IMU 链）
- **ACSC** — github.com/HViktorTsoi/ACSC：自动 camera-LiDAR 空间校准
- **targetless_calibration** — github.com/OpenCalib/JointCalib：利用自然场景特征的无标靶 camera-LiDAR 校准
- **OpenCV calibrateCamera** — docs.opencv.org：更简单的单相机内参校准（用于快速检查；生产环境用 Kalibr）

### 视觉惯性系统（使用 Kalibr 校准的场合）
- **OpenVINS** — github.com/rpng/open_vins：生产级 VIO，Kalibr 集成文档出色
- **VINS-Mono** — github.com/HKUST-Aerial-Robotics/VINS-Mono：单目 VIO
- **ORB-SLAM3** — github.com/UZ-SLAMLab/ORB_SLAM3：双目/单目/IMU SLAM

---

*Up: [Sensor Fusion Guide](../Guide.md)*
*另见：[BEVFusion](../01-BEVFusion/Guide.md) — 使用 Kalibr 的相机内参和外参*
*另见：[多目标跟踪](../04-multi-object-tracking/Guide.md)*


<details>
<summary>English original</summary>

**Debugging Checklist**

```
Before recording:
  □ tagSize measured with calipers and updated in YAML
  □ Target mounted flat on rigid surface
  □ Camera in focus at the working distance
  □ IMU frequency verified (rostopic hz)
  □ Good lighting, no motion blur at planned speeds

During recording:
  □ All cameras seeing target in every frame (for extrinsic)
  □ IMU and camera recording simultaneously
  □ Sufficient motion diversity (all 6 DOF for camera-IMU)
  □ No gaps or drops in IMU stream

After calibration:
  □ Reprojection error < 0.5px (intrinsic)
  □ Report PDF reviewed — corner detections look correct
  □ T_cam_imu makes physical sense (matches physical measurement roughly)
  □ Timeshift within expected range (|dt| < 50ms for software-triggered systems)
  □ Validate by projecting LiDAR on camera image
```

---

**14. Projects**

**Project 1: Full Calibration Chain**
Starting from a Jetson Orin Nano with a CSI camera (IMX219) and an MPU6050 IMU:
1. Record 2-hour IMU static bag → compute Allan variance → populate `imu.yaml`
2. Print and mount a 6×6 AprilGrid on foam board, measure tagSize
3. Record camera intrinsic bag → run `kalibr_calibrate_cameras`
4. Record camera-IMU bag → run `kalibr_calibrate_imu_camera`
5. Validate: reprojection error < 0.5px, timeshift < 30ms

**Project 2: Stereo Camera Calibration**
Calibrate a stereo pair (two CSI cameras or USB webcams). Compare results with OpenCV's `stereoCalibrate`. Measure the baseline (distance between cameras) with calipers and verify Kalibr's estimated translation matches within 2mm.

**Project 3: RealSense D435i Calibration**
The D435i has a built-in IMU, RGB camera, and IR stereo cameras. Calibrate the RGB-IMU pair with Kalibr. Compare with Intel's factory calibration values (stored on device). Quantify the difference.

**Project 4: Calibration Quality vs Data Duration**
Record three bags: 1 minute, 3 minutes, 5 minutes. Run camera-IMU calibration on each. Plot: estimated T_cam_imu uncertainty vs recording duration. Find the minimum data needed for your target uncertainty.

**Project 5: Integrate into BEVFusion**
Use Kalibr results to configure BEVFusion for your custom camera+LiDAR rig. Run the visual validation (project LiDAR onto camera). Compare BEVFusion detection accuracy with:
- Default/identity extrinsic (no calibration)
- Rough manual measurement
- Kalibr calibrated result

**Project 6: Calibration Validation Tooling**
Write a Python script that:
1. Loads a `camchain-imucam.yaml`
2. Loads a short validation ROS bag (different from calibration bag)
3. Detects AprilGrid corners in camera frames
4. Projects the detected corner 3D positions through T_cam_imu
5. Computes reprojection error on held-out data
6. Reports pass/fail with a configurable threshold

---

**15. Resources**

**Official**
- **Kalibr GitHub** — github.com/ethz-asl/kalibr: source code, Docker, issues
- **Kalibr Wiki** — github.com/ethz-asl/kalibr/wiki: all documentation pages
- **Kalibr Paper:** "A Toolbox for Easily Calibrating Onboard Cameras" (Furgale et al., IROS 2013)
- **Camera-IMU Paper:** "Unified Temporal and Spatial Calibration for Multi-Sensor Systems" (Furgale et al., IROS 2013)

**Allan Variance**
- **allan_variance_ros** — github.com/ori-drs/allan_variance_ros: ROS package for IMU noise characterization
- **IEEE Std 952-1997**: formal definition of Allan Variance for IMU characterization

**Related Calibration Tools**
- **LI-Calib** — github.com/APRIL-ZJU/LI-Calib: LiDAR-IMU calibration (use results with Kalibr camera-IMU to get full camera-LiDAR-IMU chain)
- **ACSC** — github.com/HViktorTsoi/ACSC: automatic camera-LiDAR spatial calibration
- **targetless_calibration** — github.com/OpenCalib/JointCalib: targetless camera-LiDAR calibration using natural scene features
- **OpenCV calibrateCamera** — docs.opencv.org: simpler single-camera intrinsic calibration (use for quick checks; Kalibr for production)

**Visual-Inertial Systems (where Kalibr calibration is used)**
- **OpenVINS** — github.com/rpng/open_vins: production-quality VIO, excellent Kalibr integration docs
- **VINS-Mono** — github.com/HKUST-Aerial-Robotics/VINS-Mono: monocular VIO
- **ORB-SLAM3** — github.com/UZ-SLAMLab/ORB_SLAM3: stereo/mono/IMU SLAM

---

*Up: [Sensor Fusion Guide](../Guide.md)*
*See also: [BEVFusion](../01-BEVFusion/Guide.md) — uses Kalibr camera intrinsics and extrinsics*
*See also: [Multi-Object Tracking](../04-multi-object-tracking/Guide.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/Kalibr/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/Kalibr/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
