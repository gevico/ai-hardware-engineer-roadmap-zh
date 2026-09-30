---
title: 边缘端非接触式多传感器监测 — 项目指南
description: 边缘端非接触式多传感器监测 — 项目指南
---

# 边缘端非接触式多传感器监测 — 项目指南

<div class="course-identity auto-course" style="--course-accent: #7e22ce; --course-accent-rgb: 126, 34, 206;" markdown="1">
<div class="course-identity__icon">NCMS</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入探究 · Jetson 路线</p>
<p class="course-identity__title">边缘端非接触式多传感器监测 — 项目指南的专属课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成演示 · 测量：延迟、内存、功耗、日志</p>
</div>
</div>


> **目标：** 构建一台非接触式监测设备，融合 RGB/Depth 与热成像相机，从热数据中提取微波动信号（0.8–3 Hz），并在边缘设备（如 Raspberry Pi 或 Jetson）上实时运行完整流水线，可选配 IoT 集成（BLE、MQTT）。

---

## 概述（3 句话）

**如何达成目标：** 校准并对齐双相机装置（RGB/Depth + 热成像），使 ROI 能在各流之间以亚像素精度彼此映射；运行基于 EVM 或 FFT 的 DSP 流水线，提取并带通滤波 0.8–3 Hz 频段的热微波动，并在预先录制的同步数据集上验证 SNR。将流水线部署到边缘硬件上，配合优化过的 NumPy/SciPy（或等价实现），使所有处理都在本地实时完成，无需云端卸载。在阶段 2，加入同步音频采集与 IoT（BLE 配网、MQTT 流式传输），实现实时多模态监测。

---


## 1. 设备与传感器

该系统是一台**非接触式监测设备**：在不发生物理接触的情况下远程观测并分析对象。

### 传感器阵列

| 组件 | 作用 |
|----------|------|
| **RGB/Depth 相机** | 彩色图像 + 深度（距离）。用于重建 3D 位置并定义 ROI（如手、脸）。 |
| **热成像相机** | 整幅场景的温度。目标：检测**微小的、局部化的温度波动**，其可能对应生理信号（血流、微动）。 |
| **音频采集** | 与视频/热成像同步，用于多模态分析与事件对齐。 |

- **异构性：** RGB/Depth 与热成像的 **FOV、分辨率和物理对齐方式均不同**。核心任务之一是把一个流上的感兴趣区域映射到另一个流上（例如“RGB 里的这只手” → “热成像里的这些像素”）。

---

## 2. 关键技术挑战

### A. 多相机传感器融合

- **问题：** 两个异构传感器（RGB/Depth + 热成像）具有不同的视场、分辨率和安装方式。
- **目标：** 把 ROI 从一台相机映射到另一台。例如：在 RGB/Depth 中检测到手，并知道热成像图中**精确对应的像素**，从而在正确的区域上做热分析。
- **要求：**
  - **外参校准：** 两个相机坐标系之间的旋转与平移（刚体式或手眼式）。
  - **内参校准：** 每台相机的镜头畸变与内参（例如使用棋盘格或专用标定靶）。
  - **亚像素精度：** 生理热信号十分微弱；对齐必须足够精确，使 ROI 边界与采样保持稳定。

### B. 热数据中的微波动检测

- **目标频段：** **0.8 Hz – 3 Hz** —— 非常微小的、亚像素级的时间变化。可能对应：
  - 脉搏或血流
  - 轻微的肌肉运动
  - 微小的环境或传感器噪声
- **要求：**
  - **欧拉视频放大（EVM）或类似方法：** 放大热视频中的微小时间变化，使其可被测量。
  - **带通滤波：** 将分析限制在 0.8–3 Hz，以抑制 DC 漂移和更高频噪声。
  - **SNR 优化：** 热成像相机噪声较大；提取如此微小的波动需要精心的流水线设计（滤波、加窗、平均，可能还需在 ROI 上做空间池化）。

### C. 边缘计算约束

- **所有处理都必须在本地**小型设备（如 Raspberry Pi、Jetson Nano）上运行。
- 这意味着：
  - **不做云端卸载**来承担重计算。
  - **具备实时能力的**算法（每帧或每缓冲区的延迟有界）。
  - **高效实现：** 优化 NumPy/SciPy（或 C/Cython），使 FFT、滤波和 EVM 在帧预算内完成；如有需要，考虑定点或降精度。

---


<details>
<summary>English original</summary>

**Non-Contact Multi-Sensor Monitoring on Edge — Project Guide**

<div class="course-identity auto-course" style="--course-accent: #7e22ce; --course-accent-rgb: 126, 34, 206;" markdown="1">
<div class="course-identity__icon">NCMS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Non-Contact Multi-Sensor Monitoring on Edge — Project Guide.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Goal:** Build a non-contact monitoring device that fuses RGB/Depth and thermal cameras, extracts micro-fluctuation signals (0.8–3 Hz) from thermal data, and runs the full pipeline in real time on an edge device (e.g. Raspberry Pi or Jetson), with optional IoT integration (BLE, MQTT).

---

**Overview (3 sentences)**

**How to achieve the goal:** Calibrate and align a dual-camera setup (RGB/Depth + thermal) so ROIs can be mapped between streams with sub-pixel accuracy; run an EVM- or FFT-based DSP pipeline to extract and bandpass-filter thermal micro-fluctuations in the 0.8–3 Hz band and validate SNR on pre-recorded, synchronized datasets. Deploy the pipeline on edge hardware with optimized NumPy/SciPy (or equivalent) so all processing runs locally in real time without cloud offload. In Phase 2, add synchronized audio capture and IoT (BLE provisioning, MQTT streaming) for live multi-modal monitoring.

---


**1. Device and Sensors**

The system is a **non-contact monitoring device**: it observes and analyzes the subject remotely without physical contact.

**Sensor array**

| Component | Role |
|----------|------|
| **RGB/Depth camera** | Color images + depth (distance). Used to reconstruct 3D positions and define ROIs (e.g. hand, face). |
| **Thermal camera** | Temperature across the scene. Target: detect **small, localized temperature fluctuations** that may correspond to physiological signals (blood flow, micro-movements). |
| **Audio capture** | Synchronized with video/thermal for multi-modal analysis and event alignment. |

- **Heterogeneity:** RGB/Depth and thermal have **different FOVs, resolutions, and physical alignment**. A core task is mapping a region of interest from one feed to the other (e.g. “this hand in RGB” → “these pixels in thermal”).

---

**2. Key Technical Challenges**

**A. Multi-camera sensor fusion**

- **Problem:** Two heterogeneous sensors (RGB/Depth + thermal) with different fields of view, resolutions, and mounting.
- **Goal:** Map an ROI from one camera to the other. Example: detect a hand in RGB/Depth and know the **exact corresponding pixels** in the thermal image so thermal analysis is done on the right region.
- **Requirements:**
  - **Extrinsic calibration:** Rotation and translation between the two camera frames (rig or hand–eye style).
  - **Intrinsic calibration:** Per-camera lens distortion and intrinsics (e.g. with a checkerboard or dedicated calibration target).
  - **Sub-pixel accuracy:** Physiological thermal signals are subtle; alignment must be precise enough that ROI boundaries and sampling are stable.

**B. Micro-fluctuation detection in thermal data**

- **Target band:** **0.8 Hz – 3 Hz** — very small, sub-pixel-level temporal variations. May correspond to:
  - Pulses or blood flow
  - Minor muscle movements
  - Minute environmental or sensor noise
- **Requirements:**
  - **Eulerian Video Magnification (EVM) or similar:** Amplify small temporal changes in the thermal video so they become measurable.
  - **Bandpass filtering:** Restrict analysis to 0.8–3 Hz to reject DC drift and higher-frequency noise.
  - **SNR optimization:** Thermal cameras are noisy; extracting such small fluctuations needs careful pipeline design (filtering, windowing, averaging, possibly spatial pooling over ROI).

**C. Edge computing constraints**

- **All processing must run locally** on a small device (e.g. Raspberry Pi, Jetson Nano).
- Implications:
  - **No cloud offload** for heavy compute.
  - **Real-time capable** algorithms (bounded latency per frame or per buffer).
  - **Efficient implementations:** Optimize NumPy/SciPy (or C/Cython) so FFT, filtering, and EVM run within the frame budget; consider fixed-point or reduced precision if needed.

---

</details>

## 3. 阶段 1：离线流水线

在迁移到实时硬件之前，先在**预先录制、时间同步**的数据集上验证完整信号链。

1. **输入：** 来自 RGB/Depth 与热成像相机的预先录制、时间同步的视频（可选含音频）。
2. **校准与对齐：**
   - 计算两个相机的内参矩阵与外参矩阵。
   - 实现 ROI 映射：给定 RGB/Depth 中的 ROI（例如来自检测或 3D 结果），投影或 warp 到热成像图像，使热成像中选中同一物理区域。
3. **热成像微波动分析：**
   - 对热成像空间中的每个 ROI，运行时序流水线：
     - EVM（或类似方法）放大微小变化。
     - 带通滤波（0.8–3 Hz）。
     - 可选：FFT 或功率谱密度，以确认频带内有能量。
   - **验证 SNR：** 确保硬件与流水线给出足够的信噪比，以可靠检测目标微信号。
4. **输出：** 干净的频域（或滤波后的时域）数据，证明 0.8–3 Hz 微波动可被提取，且该搭建方案适用于阶段 2。

### 阶段 1 数据集：仅 camera01（Free-Viewpoint RGB-D Video Dataset）

阶段 1 仅使用 Free-Viewpoint RGB-D Video Dataset 中的 **camera01**。这样得到单路、同步的 RGB + depth 流，无需应对多视角的复杂性，即可实现并验证流水线的 RGB/Depth 一侧（校准、ROI 定义、时间对齐）。

**文件（本地）**

| File | Role |
|------|------|
| `Free-Viewpoint-RGB-D-Video-Dataset-main/camera01-rgb.mp4` | RGB 视频（1920×1080，已同步）。 |
| `Free-Viewpoint-RGB-D-Video-Dataset-main/camera01-depth.mp4` | 逐帧 depth 视频（分辨率相同）；depth 以灰度编码；转换为公制深度（见下文）。 |
| `Free-Viewpoint-RGB-D-Video-Dataset-main/Camera Parameters/` | 全部 12 个相机的内参与外参；使用 **camera 1**（COLMAP 相机 ID `1`，或 `paras.txt` 中前 5 行构成的块）。 |

**Camera01 校准**

- **COLMAP：** 在 `Camera Parameters/sparse/cameras.txt` 中，相机 ID **1** 即 camera01（PINHOLE，1920×1080；参数：fx、fy、cx、cy）。逐帧外参位于 `images.txt` / `images.bin`（图像名或 ID 对应 camera01 的相机 ID 1）。
- **paras.txt：** 前 5 行 = camera01：分辨率、K_matrix（fx、fy、cx、cy）、R_matrix（3×3）、world_position（t）。投影：`Xp = K * R * (Xw - t)`。

**深度转换（来自数据集 README）**

Depth 视频帧为 0–255 灰度。按下式转换为公制深度（如 mm 或 m）：

```text
fB = 32504
mindepth, maxdepth = 40, 150
maxdisp, mindisp = fB/mindepth, fB/maxdepth
depth = fB / (gray/255 * (maxdisp - mindisp) + mindisp)
```

对 `camera01-rgb.mp4` 与 `camera01-depth.mp4` 使用相同的帧索引，使 RGB 与 depth 保持同步。

**阶段 1 代码：** 文件夹 [phase1/](01-第一阶段/README.md) 提供 Python 代码，用于**相机校准**（加载 `paras.txt`）、**实时目标检测**（通过 OpenCV 做人脸 + 人体）以及每个检测 ROI 的**深度计算**。运行：`python phase1/run_pipeline.py`（见 [phase1/README.md](01-第一阶段/README.md)）。

**使用 camera01 的阶段 1 工作流**

1. **加载** `camera01-rgb.mp4` 与 `camera01-depth.mp4`；按帧索引对齐。
2. 从 `Camera Parameters` **加载** camera01 内参（必要时含外参）。
3. 在 RGB 中**定义 ROI**（例如通过检测或手动框选的人脸、手）；可选地用 depth 与内参反投影到 3D。
4. **时序流水线：** 之后加入热成像时，需把这些 ROI 映射到热成像图像。仅用 camera01 时，验证 ROI 随时间的稳定性与基于 depth 的掩膜。
5. **热成像：** 本数据集不含；对于 0.8–3 Hz 微波动与 EVM/带通/SNR，改用其他来源或阶段 2 的实时采集。

**数据集摘要（仅 camera01）**

| Aspect | camera01 (this dataset) | Phase 1 use |
|--------|--------------------------|-------------|
| **RGB** | ✓ `camera01-rgb.mp4`，1920×1080 | 单视角 ROI 与对齐 |
| **Depth** | ✓ `camera01-depth.mp4`，由 COLMAP 导出，后处理过 | 3D ROI、尺度、遮挡；用上文公式转换 |
| **Calibration** | ✓ `Camera Parameters/sparse` 或 `paras.txt` 中的 Camera 1 | 内参（以及未来热成像装置的外参） |
| **Thermal** | ✗ 无 | 通过其他数据集或阶段 2 补充 |
| **Citation** | Guo et al., MMSys 2022；另见数据集文件夹中的 README | — |

**来源：** [Free-Viewpoint RGB-D Video Dataset](https://medialab.sjtu.edu.cn/post/free-viewpoint-rgb-d-video-dataset/) (SJTU)；学术用途，非商业。完整数据集有 12 个视角与 14 个序列；本项目阶段 1 仅使用上述 camera01 这一对。

---


<details>
<summary>English original</summary>

**3. Phase 1: Offline Pipeline**

Validate the full signal chain on **pre-recorded, synchronized** datasets before moving to live hardware.

1. **Input:** Pre-recorded, time-synchronized videos from RGB/Depth and thermal cameras (and optionally audio).
2. **Calibration and alignment:**
   - Compute intrinsic and extrinsic matrices for both cameras.
   - Implement ROI mapping: given an ROI in RGB/Depth (e.g. from detection or 3D), project or warp to the thermal image so the same physical region is selected in thermal.
3. **Thermal micro-fluctuation analysis:**
   - For each ROI in thermal space, run a temporal pipeline:
     - EVM (or similar) to amplify small changes.
     - Bandpass filter (0.8–3 Hz).
     - Optional: FFT or power spectral density to confirm energy in band.
   - **Validate SNR:** Ensure the hardware and pipeline yield a sufficient signal-to-noise ratio to reliably detect the target micro-signals.
4. **Output:** Clean frequency-domain (or filtered time-domain) data demonstrating that 0.8–3 Hz micro-fluctuations can be extracted and that the setup is suitable for Phase 2.

**Phase 1 dataset: camera01 only (Free-Viewpoint RGB-D Video Dataset)**

For Phase 1, use **only camera01** from the Free-Viewpoint RGB-D Video Dataset. This gives a single, synchronized RGB + depth stream so you can implement and validate the RGB/Depth side of the pipeline (calibration, ROI definition, temporal alignment) without multi-view complexity.

**Files (local)**

| File | Role |
|------|------|
| `Free-Viewpoint-RGB-D-Video-Dataset-main/camera01-rgb.mp4` | RGB video (1920×1080, synchronized). |
| `Free-Viewpoint-RGB-D-Video-Dataset-main/camera01-depth.mp4` | Per-frame depth video (same resolution); depth encoded as grayscale; convert to metric depth (see below). |
| `Free-Viewpoint-RGB-D-Video-Dataset-main/Camera Parameters/` | Intrinsics and extrinsics for all 12 cameras; use **camera 1** (COLMAP camera ID `1`, or the first 5-line block in `paras.txt`). |

**Camera01 calibration**

- **COLMAP:** In `Camera Parameters/sparse/cameras.txt`, camera ID **1** is camera01 (PINHOLE, 1920×1080; params: fx, fy, cx, cy). Extrinsics per frame are in `images.txt` / `images.bin` (image names or IDs map to camera ID 1 for camera01).
- **paras.txt:** First 5 lines = camera01: resolution, K_matrix (fx, fy, cx, cy), R_matrix (3×3), world_position (t). Projection: `Xp = K * R * (Xw - t)`.

**Depth conversion (from dataset README)**

Depth video frames are grayscale 0–255. Convert to metric depth (e.g. mm or m) with:

```text
fB = 32504
mindepth, maxdepth = 40, 150
maxdisp, mindisp = fB/mindepth, fB/maxdepth
depth = fB / (gray/255 * (maxdisp - mindisp) + mindisp)
```

Use the same frame index for `camera01-rgb.mp4` and `camera01-depth.mp4` so RGB and depth stay synchronized.

**Phase 1 code:** The folder [phase1/](01-第一阶段/README.md) provides Python code for **camera calibration** (load `paras.txt`), **real-time object detection** (face + person via OpenCV), and **depth calculation** per detection ROI. Run: `python phase1/run_pipeline.py` (see [phase1/README.md](01-第一阶段/README.md)).

**Phase 1 workflow with camera01**

1. **Load** `camera01-rgb.mp4` and `camera01-depth.mp4`; align by frame index.
2. **Load** camera01 intrinsics (and extrinsics if needed) from `Camera Parameters`.
3. **Define ROIs** in RGB (e.g. face, hand via detection or manual box); optionally back-project to 3D using depth and intrinsics.
4. **Temporal pipeline:** When you add thermal later, you will map these ROIs to the thermal image. For camera01 alone, validate ROI stability over time and depth-based masking.
5. **Thermal:** Not in this dataset; use another source or Phase 2 live capture for 0.8–3 Hz micro-fluctuation and EVM/bandpass/SNR.

**Dataset summary (camera01 only)**

| Aspect | camera01 (this dataset) | Phase 1 use |
|--------|--------------------------|-------------|
| **RGB** | ✓ `camera01-rgb.mp4`, 1920×1080 | Single-view ROI and alignment |
| **Depth** | ✓ `camera01-depth.mp4`, COLMAP-derived, post-processed | 3D ROI, scale, occlusion; convert with formula above |
| **Calibration** | ✓ Camera 1 in `Camera Parameters/sparse` or `paras.txt` | Intrinsics (and extrinsics for future thermal rig) |
| **Thermal** | ✗ None | Add via other dataset or Phase 2 |
| **Citation** | Guo et al., MMSys 2022; see also README in dataset folder | — |

**Source:** [Free-Viewpoint RGB-D Video Dataset](https://medialab.sjtu.edu.cn/post/free-viewpoint-rgb-d-video-dataset/) (SJTU); academic, non-commercial. Full dataset has 12 views and 14 sequences; for this project, Phase 1 uses only the camera01 pair above.

---

</details>

## 4. 阶段 2：实时与 IoT 集成

若阶段 1 能实现可靠的微信号提取：

1. **部署到真实硬件：** 在边缘设备上实时运行同一套校准与 DSP 流水线（实时 RGB/Depth + thermal 流）。
2. **音频：** 增加同步音频采集，使事件能在视觉、thermal 与声音之间对齐。
3. **IoT 集成：**
   - **BLE 配网：** 通过低功耗蓝牙（BLE）将设备与其他系统（例如手机 app、网关）配对并配置。
   - **MQTT：** 实时流式传输提取出的信号或摘要，用于仪表盘、日志记录或云端备份。
4. **目标：** 一台完全可运行的设备，能够在边缘侧采集并处理多模态信号（RGB、depth、thermal、音频），具备精确对齐与可选的实时流式传输。

---

## 5. 为何先进

- **实时多传感器融合：** 很少有系统能在小型设备上把 RGB/Depth 与 thermal 结合并进行实时、对齐的处理。
- **微信号提取：** 在 0.8–3 Hz 频段内做亚像素级 thermal 波动检测，处于生理学、信号处理与计算机视觉的交汇点。
- **边缘 + IoT：** 在严格的延迟与资源限制下，融合 CV、校准、DSP 与嵌入式/IoT（BLE、MQTT）。

---

## 6. 主要应用

**非接触式婴儿/儿童生命体征监测** —— 本项目的主要目标用例。该类设备（例如 [iBaby Labs i20](https://ibabylabs.com/)) 提供**非接触式呼吸与心率监测**，让看护者能在婴儿睡眠时追踪其生命体征——无需可穿戴设备、夹子或皮肤接触。

- **呼吸率：** 由胸部/腹部微动推断（例如 ROI 上的光流或 RGB/thermal 的细微强度变化）。0.8–3 Hz 频段与 EVM 式放大与呼吸频率相符。
- **心率：** 由**远程 PPG（rPPG）**或类似的光学传感推断：血液搏动引起的肤色或 thermal 特征的微小周期性变化由相机捕获并处理（带通、相位分析），从而得出脉搏。无需接触；低光下配合夜视也可工作。
- **边缘 + AI：** 端侧 NPU 或 CPU 实时运行心率、呼吸与微动的非接触分析；告警（例如安全区域、面部遮挡）与洞察（睡眠质量、情绪线索）可发送到手机 app。BLE/Wi‑Fi 与可选的云端同步支撑家长仪表盘与安心感。
- **为何契合本项目：** 同一套技术栈——多传感器（RGB/Depth + thermal）融合、ROI 对齐、微波动提取（0.8–3 Hz）与边缘部署——直接支撑构建一款婴儿/儿童监测器，实现「无形的照护，可见的安心」：持续监测生命体征，不打扰睡眠，也无需可穿戴设备。

---

## 7. 其他可能的应用

非接触、多传感器（RGB/Depth + thermal）设备在边缘侧提取微波动（0.8–3 Hz）的其他用途：

| 领域 | 应用 |
|--------|-------------|
| **生命体征（成人）** | 从面部或身体 ROI 获取非接触式心率/脉搏与呼吸；适用于护理院、医院或不宜使用皮肤传感器的家庭监测。 |
| **压力与唤醒度** | 面部/身体的 thermal 与细微运动；0.8–3 Hz 频段对应生理节律。驾驶员困倦、职场健康、精神负荷。 |
| **睡眠与休息** | 睡眠期间的非接触呼吸与体动；低光下用 thermal；边缘 + MQTT 供仪表盘或看护者使用。 |
| **跌倒与活动** | RGB/Depth 用于姿态与跌倒检测；thermal 用于黑暗/杂乱环境下的鲁棒性；边缘告警（例如独居老人）。 |
| **工业与安防** | thermal 用于存在检测、过热或异常；RGB/Depth 用于 occupancy 检测与 3D；边缘用于隐私与延迟。 |
| **研究与生理学** | 在实验室/现场研究中同步 RGB、depth、thermal、音频；导出或经 MQTT 供分析。 |
| **无障碍与辅助技术** | 为无法佩戴传感器的用户提供免手操作的生命体征/状态监测；通过 BLE/MQTT 发送到 app 或看护者。 |

---


<details>
<summary>English original</summary>

**4. Phase 2: Live and IoT Integration**

If Phase 1 shows reliable micro-signal extraction:

1. **Deploy on live hardware:** Run the same calibration and DSP pipeline on the edge device in real time (live RGB/Depth + thermal streams).
2. **Audio:** Add synchronized audio capture so events can be aligned across vision, thermal, and sound.
3. **IoT integration:**
   - **BLE provisioning:** Pair and configure the device with other systems (e.g. phone app, gateway) over Bluetooth Low Energy.
   - **MQTT:** Stream extracted signals or summaries in real time for dashboards, logging, or cloud backup.
4. **Goal:** A fully operational device that captures and processes multi-modal signals (RGB, depth, thermal, audio) with precise alignment and optional real-time streaming, all on the edge.

---

**5. Why It’s Advanced**

- **Real-time multi-sensor fusion:** Few systems combine RGB/Depth and thermal with live, aligned processing on small devices.
- **Micro-signal extraction:** Sub-pixel thermal fluctuation detection in the 0.8–3 Hz band is at the intersection of physiology, signal processing, and computer vision.
- **Edge + IoT:** Combines CV, calibration, DSP, and embedded/IoT (BLE, MQTT) under strict latency and resource limits.

---

**6. Main Application**

**Contactless baby / child vital monitoring** — the primary target use case for this project. Devices in this category (e.g. [iBaby Labs i20](https://ibabylabs.com/)) provide **contactless breathing and heart rate monitoring** so caregivers can track a baby’s vitals while they sleep—without wearables, clips, or skin contact.

- **Breathing rate:** Inferred from chest/abdomen micro-motion (e.g. optical flow or subtle intensity changes in RGB/thermal over an ROI). The 0.8–3 Hz band and EVM-style amplification align with respiratory rates.
- **Heart rate:** Inferred from **remote PPG (rPPG)** or analogous optical sensing: tiny, periodic changes in skin tone or thermal signature caused by blood pulsation are captured by the camera and processed (bandpass, phase analysis) to derive pulse. No contact required; works with night vision in low light.
- **Edge + AI:** An on-device NPU or CPU runs real-time contactless analysis of heart rate, breathing, and micro-movements; alerts (e.g. safe zone, face cover) and insights (sleep quality, mood cues) can be sent to a phone app. BLE/Wi‑Fi and optional cloud sync support parental dashboards and peace of mind.
- **Why it fits this project:** The same technical stack—multi-sensor (RGB/Depth + thermal) fusion, ROI alignment, micro-fluctuation extraction (0.8–3 Hz), and edge deployment—directly supports building a baby/child monitor that offers “invisible care, visible peace”: continuous vital monitoring without disrupting sleep or requiring wearables.

---

**7. Other Possible Applications**

Other uses of a non-contact, multi-sensor (RGB/Depth + thermal) device that extracts micro-fluctuations (0.8–3 Hz) on the edge:

| Domain | Application |
|--------|-------------|
| **Vital signs (adult)** | Contactless heart rate / pulse and respiration from facial or body ROIs; care homes, hospitals, or home monitoring where skin sensors are undesirable. |
| **Stress & arousal** | Thermal and subtle motion in face/body; 0.8–3 Hz band for physiological rhythms. Driver drowsiness, workplace wellness, mental load. |
| **Sleep & rest** | Non-contact breathing and movement during sleep; thermal in low light; edge + MQTT for dashboards or caregivers. |
| **Fall & activity** | RGB/Depth for pose and fall detection; thermal for robustness in dark/clutter; edge alerts (e.g. elderly living alone). |
| **Industrial & security** | Thermal for presence, overheating, or anomalies; RGB/Depth for occupancy and 3D; edge for privacy and latency. |
| **Research & physiology** | Lab/field studies with synced RGB, depth, thermal, audio; export or MQTT for analysis. |
| **Accessibility & assistive tech** | Hands-free vital/state monitoring for users who cannot wear sensors; BLE/MQTT to apps or caregivers. |

---

</details>

## 8. 资源

- **阶段 1 RGB/Depth 数据：** 仅使用 **camera01**：`camera01-rgb.mp4` 和 `camera01-depth.mp4` 在 `Free-Viewpoint-RGB-D-Video-Dataset-main/` 中，校准文件在 `Camera Parameters/`。数据集：[Free-Viewpoint RGB-D Video Dataset](https://medialab.sjtu.edu.cn/post/free-viewpoint-rgb-d-video-dataset/)（SJTU）。见 [阶段 1 数据集：仅 camera01](#phase-1-dataset-camera01-only-free-viewpoint-rgb-d-video-dataset)。
- **Eulerian Video Magnification（EVM）** —— 用于放大视频中细微时间变化的原始论文与实现。
- **多相机校准：** OpenCV 相机校准、stereo/rig 校准；thermal–RGB 对齐（如 FLIR/opencv thermal 示例）。
- **带通滤波 / FFT：** SciPy 信号处理（butter、filtfilt、fft、welch PSD），用于 0.8–3 Hz 与 SNR 估计。
- **边缘：** NumPy/SciPy 优化；Raspberry Pi 或 Jetson 性能调优；热点循环可选 Cython 或 C。
- **IoT：** 低功耗蓝牙（BLE）（如 BlueZ、bleak）；MQTT（如 paho-mqtt）用于流传输。

---

*返回 [边缘 AI 优化 —— 项目](../Guide.md)。*


<details>
<summary>English original</summary>

**8. Resources**

- **Phase 1 RGB/Depth data:** Use **camera01 only**: `camera01-rgb.mp4` and `camera01-depth.mp4` in `Free-Viewpoint-RGB-D-Video-Dataset-main/`, with calibration in `Camera Parameters/`. Dataset: [Free-Viewpoint RGB-D Video Dataset](https://medialab.sjtu.edu.cn/post/free-viewpoint-rgb-d-video-dataset/) (SJTU). See [Phase 1 dataset: camera01 only](#phase-1-dataset-camera01-only-free-viewpoint-rgb-d-video-dataset).
- **Eulerian Video Magnification (EVM)** — original paper and implementations for amplifying subtle temporal changes in video.
- **Multi-camera calibration:** OpenCV camera calibration, stereo/rig calibration; thermal–RGB alignment (e.g. FLIR/opencv thermal examples).
- **Bandpass filtering / FFT:** SciPy signal processing (butter, filtfilt, fft, welch PSD) for 0.8–3 Hz and SNR estimation.
- **Edge:** NumPy/SciPy optimization; Raspberry Pi or Jetson performance tuning; optional Cython or C for hot loops.
- **IoT:** BLE (e.g. BlueZ, bleak); MQTT (e.g. paho-mqtt) for streaming.

---

*Back to [Edge AI Optimization — Projects](../Guide.md).*

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/5. ML and AI/non-contact-monitoring-edge/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/5.%20ML%20and%20AI/non-contact-monitoring-edge/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
