---
title: camerad — openpilot 摄像头流水线
description: camerad — openpilot 摄像头流水线
---

# camerad — openpilot 摄像头流水线

<div class="course-identity auto-course" style="--course-accent: #ea580c; --course-accent-rgb: 234, 88, 12;" markdown="1">
<div class="course-identity__icon">COCP</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 专项</p>
<p class="course-identity__title">camerad — openpilot 摄像头流水线的专项课程标识。</p>
<p class="course-identity__meta">产物：专项案例研究 · 度量：性能、可靠性、角色匹配</p>
</div>
</div>


> **目标：** 理解 openpilot 如何采集、处理摄像头帧并将其交付给感知栈。camerad 是流水线中的第一个进程：raw sensor → ISP → VisionIpc → modeld。

---


## 1. 概述

**camerad** 是一个原生 C++ 进程，它：

- 从最多 **三路摄像头**（广角路况、路况、驾驶员）采集帧
- 将其送入 **Qualcomm Spectra ISP**（图像信号处理器）处理
- 通过 **VisionIpc**（共享内存 IPC）发布 **YUV 帧**
- 通过 cereal 消息发布 **FrameData**（元数据）
- 实现 **自动曝光（AE）** 以适应光照

**位置：** `openpilot/system/camerad/`

**入口点：** `main.cc` → `camerad_thread()`（绑定到 CPU 核 6）

---

## 2. 架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              camerad                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│  Sensor (I2C)     │  CSI/MIPI    │  IFE (Image Front End)  │  BPS (optional)  │
│  OX03C10/OS04C10 │  → RAW       │  Demosaic, CCM, Gamma   │  Downscale        │
│                  │              │  Vignetting correction  │  (driver cam)     │
└─────────────────────────────────────────────────────────────────────────────┘
         │                                    │
         ▼                                    ▼
   Exposure control                    YUV output
   (AE algorithm)                      VisionIpc buffers
         │                                    │
         └────────────────┬───────────────────┘
                          ▼
                   FrameData (cereal)
                   frame_id, timestamps, gain, exposure
```

**三路摄像头（comma 3X）：**

| 摄像头 | 流 | 角色 | 焦距 |
|--------|--------|------|--------------|
| **广角路况** | `VISION_STREAM_WIDE_ROAD` | 宽视场，外设 | 1.71 mm |
| **路况** | `VISION_STREAM_ROAD` | 主驾驶视野 | 8.0 mm |
| **驾驶员** | `VISION_STREAM_DRIVER` | 驾驶员监控（DMS） | 1.71 mm |

---

## 3. 摄像头配置

定义于 `cameras/hw.h`：

```cpp
// Wide: fisheye, peripheral
WIDE_ROAD_CAMERA_CONFIG = {
  .camera_num = 0,
  .stream_type = VISION_STREAM_WIDE_ROAD,
  .focal_len = 1.71,
  .publish_name = "wideRoadCameraState",
  .output_type = ISP_IFE_PROCESSED,
};

// Road: main forward-facing, narrow FoV
ROAD_CAMERA_CONFIG = {
  .camera_num = 1,
  .stream_type = VISION_STREAM_ROAD,
  .focal_len = 8.0,
  .publish_name = "roadCameraState",
  .vignetting_correction = true,
  .output_type = ISP_IFE_PROCESSED,
};

// Driver: cabin-facing
DRIVER_CAMERA_CONFIG = {
  .camera_num = 2,
  .stream_type = VISION_STREAM_DRIVER,
  .focal_len = 1.71,
  .publish_name = "driverCameraState",
  .output_type = ISP_BPS_PROCESSED,  // BPS for extra processing
};
```

**Python 内参**（用于 modeld warp、校准）：`common/transformations/camera.py`

- 路况：1928×1208，焦距 2648 px（OX）或 1344×760，焦距约 1142 px（OS）
- 广角：1928×1208，焦距 567 px
- 驾驶员：与广角相同

---

## 4. 硬件栈

### 传感器

| 传感器 | 设备 | 分辨率 | 备注 |
|--------|--------|------------|-------|
| **OX03C10** | comma 3X（tici/tizi） | 1928×1208 | OmniVision，3 MP |
| **OS04C10** | comma 3X（mici） | 2688×1520 | OmniVision，4 MP |
| **AR0231** | 旧版（neo） | 1164×874 | Aptina |

**传感器接口：** I2C 用于寄存器控制（曝光、增益、初始化）。MIPI CSI 用于图像数据。

**文件：** `sensors/ox03c10.cc`、`sensors/os04c10.cc`、`sensors/sensor.h`

### Qualcomm Spectra ISP

- **IFE**（Image Front End）：去马赛克、色彩校正（CCM）、gamma、暗角校正
- **BPS**（Bayer Processing Segment）：用于驾驶员摄像头（额外的降采样/处理）
- **CDM**（Camera Data Mover）：DMA、缓冲区管理
- **CSIPHY**：MIPI CSI 物理层

**内核：** Linux V4L2（Video4Linux2），`CAM_REQ_MGR`（Request Manager）用于帧同步。

### V4L2：Linux 内核摄像头 API

**V4L2**（Video4Linux2）是 Linux 内核用于视频采集、输出和编解码的标准 API。camerad 使用 V4L2 驱动 Qualcomm Spectra ISP 并接收处理后的帧。


<details>
<summary>English original</summary>

**camerad — Openpilot Camera Pipeline**

<div class="course-identity auto-course" style="--course-accent: #ea580c; --course-accent-rgb: 234, 88, 12;" markdown="1">
<div class="course-identity__icon">COCP</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for camerad — Openpilot Camera Pipeline.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


> **Goal:** Understand how openpilot captures, processes, and delivers camera frames to the perception stack. camerad is the first process in the pipeline: raw sensor → ISP → VisionIpc → modeld.

---


**1. Overview**

**camerad** is a native C++ process that:

- Captures frames from up to **three cameras** (wide road, road, driver)
- Runs them through the **Qualcomm Spectra ISP** (Image Signal Processor)
- Publishes **YUV frames** via **VisionIpc** (shared memory IPC)
- Publishes **FrameData** (metadata) via cereal messaging
- Implements **auto exposure (AE)** to adapt to lighting

**Location:** `openpilot/system/camerad/`

**Entry point:** `main.cc` → `camerad_thread()` (pinned to CPU core 6)

---

**2. Architecture**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              camerad                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│  Sensor (I2C)     │  CSI/MIPI    │  IFE (Image Front End)  │  BPS (optional)  │
│  OX03C10/OS04C10 │  → RAW       │  Demosaic, CCM, Gamma   │  Downscale        │
│                  │              │  Vignetting correction  │  (driver cam)     │
└─────────────────────────────────────────────────────────────────────────────┘
         │                                    │
         ▼                                    ▼
   Exposure control                    YUV output
   (AE algorithm)                      VisionIpc buffers
         │                                    │
         └────────────────┬───────────────────┘
                          ▼
                   FrameData (cereal)
                   frame_id, timestamps, gain, exposure
```

**Three cameras (comma 3X):**

| Camera | Stream | Role | Focal length |
|--------|--------|------|--------------|
| **Wide road** | `VISION_STREAM_WIDE_ROAD` | Wide FoV, peripheral | 1.71 mm |
| **Road** | `VISION_STREAM_ROAD` | Main driving view | 8.0 mm |
| **Driver** | `VISION_STREAM_DRIVER` | Driver monitoring (DMS) | 1.71 mm |

---

**3. Camera Configuration**

Defined in `cameras/hw.h`:

```cpp
// Wide: fisheye, peripheral
WIDE_ROAD_CAMERA_CONFIG = {
  .camera_num = 0,
  .stream_type = VISION_STREAM_WIDE_ROAD,
  .focal_len = 1.71,
  .publish_name = "wideRoadCameraState",
  .output_type = ISP_IFE_PROCESSED,
};

// Road: main forward-facing, narrow FoV
ROAD_CAMERA_CONFIG = {
  .camera_num = 1,
  .stream_type = VISION_STREAM_ROAD,
  .focal_len = 8.0,
  .publish_name = "roadCameraState",
  .vignetting_correction = true,
  .output_type = ISP_IFE_PROCESSED,
};

// Driver: cabin-facing
DRIVER_CAMERA_CONFIG = {
  .camera_num = 2,
  .stream_type = VISION_STREAM_DRIVER,
  .focal_len = 1.71,
  .publish_name = "driverCameraState",
  .output_type = ISP_BPS_PROCESSED,  // BPS for extra processing
};
```

**Python intrinsics** (for modeld warp, calibration): `common/transformations/camera.py`

- Road: 1928×1208, focal 2648 px (OX) or 1344×760, focal ~1142 px (OS)
- Wide: 1928×1208, focal 567 px
- Driver: same as wide

---

**4. Hardware Stack**

**Sensors**

| Sensor | Device | Resolution | Notes |
|--------|--------|------------|-------|
| **OX03C10** | comma 3X (tici/tizi) | 1928×1208 | OmniVision, 3 MP |
| **OS04C10** | comma 3X (mici) | 2688×1520 | OmniVision, 4 MP |
| **AR0231** | Legacy (neo) | 1164×874 | Aptina |

**Sensor interface:** I2C for register control (exposure, gain, init). MIPI CSI for image data.

**Files:** `sensors/ox03c10.cc`, `sensors/os04c10.cc`, `sensors/sensor.h`

**Qualcomm Spectra ISP**

- **IFE** (Image Front End): demosaic, color correction (CCM), gamma, vignetting
- **BPS** (Bayer Processing Segment): used for driver camera (extra downscale/processing)
- **CDM** (Camera Data Mover): DMA, buffer management
- **CSIPHY**: MIPI CSI physical layer

**Kernel:** Linux V4L2 (Video4Linux2), `CAM_REQ_MGR` (Request Manager) for frame synchronization.

**V4L2: Linux Kernel Camera API**

**V4L2** (Video4Linux2) is the standard Linux kernel API for video capture, output, and codecs. camerad uses V4L2 to drive the Qualcomm Spectra ISP and receive processed frames.

</details>

#### V4L2 提供了什么

| 概念 | 说明 |
|---------|-------------|
| **设备节点** | `/dev/video0`、`/dev/video1`、… — 每个采集/输出设备一个。camerad 为 request manager 打开 `/dev/video0`（sync 设备）。 |
| **Media Controller** | 对流水线建模：sensor → CSI → IFE → BPS → video node。用于发现并连接各实体。 |
| **Request API** | 一个 *request* = 一帧通过流水线。sensor 采集、IFE、BPS 都绑定到同一个 request 以实现同步。 |
| **Buffer 流程** | `VIDIOC_QBUF` 入队空 buffer → 硬件填充 → `VIDIOC_DQBUF` 出队已填充的 buffer。 |

#### camerad 的 V4L2 流程

```
1. Open /dev/video0 (sync/request manager device)
2. Media Controller: discover sensor, IFE, BPS entities; configure links
3. VIDIOC_REQBUFS: allocate capture buffers (YUV output)
4. VIDIOC_QUERYBUF: get buffer addresses for mmap()
5. VIDIOC_QBUF: enqueue buffers into the pipeline
6. VIDIOC_STREAMON: start streaming
7. poll(fd, POLLPRI): wait for frame-done event
8. VIDIOC_DQEVENT: dequeue event (frame completed)
9. processFrame() → YUV ready → sendFrameToVipc()
10. VIDIOC_QBUF: re-enqueue buffer for next frame
```

#### camerad 使用的关键 ioctl

| ioctl | 用途 |
|-------|---------|
| `VIDIOC_REQBUFS` | 分配 buffer 队列（memory-mapped 或 DMA） |
| `VIDIOC_QUERYBUF` | 获取 buffer 信息（offset、length）以供 mmap |
| `VIDIOC_QBUF` | 为采集入队 buffer |
| `VIDIOC_DQBUF` | 出队已填充的 buffer（也可由事件通知完成） |
| `VIDIOC_DQEVENT` | 出队异步事件（如 frame done）— 与 Request API 配合使用 |
| `VIDIOC_STREAMON` / `VIDIOC_STREAMOFF` | 启动/停止 streaming |
| `VIDIOC_S_FMT` | 设置像素格式（如 NV12） |
| `VIDIOC_S_PARM` | 设置帧率 |

#### 事件驱动模型

camerad 使用**事件驱动**采集，而非阻塞式的 `DQBUF`：

- `poll(fd, POLLPRI)` — 等待 frame-done 事件可用
- `VIDIOC_DQEVENT` — 出队事件；payload 指明哪个 request/frame 已完成
- 处理该帧、重新入队 buffer，然后再次 poll

当有事件待处理时，`POLLPRI`（priority/exceptional condition）被置位。这避免了忙等待，并能与 Request API 干净地集成。

#### CAM_REQ_MGR（Qualcomm Request Manager）

在 Qualcomm 平台上，**CAM_REQ_MGR** 是一个 kernel 组件，负责：

- **同步** sensor 采集与 ISP（IFE、BPS）— 一个 request 将它们绑定在一起
- **排队 request** — 每个 request 携带整条流水线的 buffer 和控制信息
- **通知完成** — 流水线完成一帧时，会向用户态排队一个事件

没有 Request API 时，sensor 与 ISP 可能失去同步（例如给某一帧应用了错误的曝光）。Request API 保证：*这个* sensor 帧 → *这个* IFE 配置 → *这个* 输出 buffer。

#### 设备布局（Qualcomm Spectra）

| 设备 | 角色 |
|--------|------|
| `/dev/video0` | 同步/request manager — camerad 在此 poll frame-done 事件 |
| `/dev/media0` | Media controller — 流水线拓扑（sensor ↔ IFE ↔ BPS） |
| Subdev（如 `/dev/v4l-subdev*`） | sensor、IFE、BPS 作为独立实体；通过 media controller 配置 |

#### 调试 V4L2

```bash
# List video devices and capabilities
v4l2-ctl --list-devices

# List supported formats
v4l2-ctl -d /dev/video0 --list-formats-ext

# Media controller: show pipeline
media-ctl -d /dev/media0 -p
```

**参考资料：** [V4L2 API (kernel.org)](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html), [VIDIOC_QBUF/VIDIOC_DQBUF](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/vidioc-qbuf.html), [V4L2 poll()](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/func-poll.html)

---

## 5. ISP 流水线

```
Sensor (RAW Bayer) → CSI → IFE → [BPS for driver] → YUV (NV12)
```

**输出格式：** NV12（Y plane + interleaved UV plane）。vision/ML 的标准格式。

**Buffer 流程：**

1. `SpectraMaster` 初始化 `/dev/video0`、sync、ISP、ICP
2. `SpectraCamera` 每路 camera：sensor init、IFE config、BPS config（仅驱动）、link devices
3. `CameraBuf` 分配 raw（可选）和 YUV buffer
4. `VisionIpcServer` 为消费者（modeld 等）创建共享内存 buffer
5. V4L2 帧事件到达时：`handle_camera_event` → `processFrame` → `sendFrameToVipc` + 发布 FrameData

---


<details>
<summary>English original</summary>

**What V4L2 Provides**

| Concept | Description |
|---------|-------------|
| **Device nodes** | `/dev/video0`, `/dev/video1`, … — one per capture/output device. camerad opens `/dev/video0` (sync device) for the request manager. |
| **Media Controller** | Models the pipeline: sensor → CSI → IFE → BPS → video node. Used to discover and link entities. |
| **Request API** | One *request* = one frame through the pipeline. Sensor capture, IFE, BPS all tied to the same request for synchronization. |
| **Buffer flow** | `VIDIOC_QBUF` enqueue empty buffer → hardware fills it → `VIDIOC_DQBUF` dequeue filled buffer. |

**camerad's V4L2 Flow**

```
1. Open /dev/video0 (sync/request manager device)
2. Media Controller: discover sensor, IFE, BPS entities; configure links
3. VIDIOC_REQBUFS: allocate capture buffers (YUV output)
4. VIDIOC_QUERYBUF: get buffer addresses for mmap()
5. VIDIOC_QBUF: enqueue buffers into the pipeline
6. VIDIOC_STREAMON: start streaming
7. poll(fd, POLLPRI): wait for frame-done event
8. VIDIOC_DQEVENT: dequeue event (frame completed)
9. processFrame() → YUV ready → sendFrameToVipc()
10. VIDIOC_QBUF: re-enqueue buffer for next frame
```

**Key ioctls Used by camerad**

| ioctl | Purpose |
|-------|---------|
| `VIDIOC_REQBUFS` | Allocate buffer queue (memory-mapped or DMA) |
| `VIDIOC_QUERYBUF` | Get buffer info (offset, length) for mmap |
| `VIDIOC_QBUF` | Enqueue buffer for capture |
| `VIDIOC_DQBUF` | Dequeue filled buffer (alternatively, events can signal completion) |
| `VIDIOC_DQEVENT` | Dequeue async event (e.g. frame done) — used with Request API |
| `VIDIOC_STREAMON` / `VIDIOC_STREAMOFF` | Start/stop streaming |
| `VIDIOC_S_FMT` | Set pixel format (e.g. NV12) |
| `VIDIOC_S_PARM` | Set frame rate |

**Event-Driven Model**

camerad uses **event-driven** capture, not blocking `DQBUF`:

- `poll(fd, POLLPRI)` — wait until a frame-done event is available
- `VIDIOC_DQEVENT` — dequeue the event; payload indicates which request/frame completed
- Process the frame, re-enqueue buffers, then poll again

`POLLPRI` (priority/exceptional condition) is set when an event is pending. This avoids busy-waiting and integrates cleanly with the Request API.

**CAM_REQ_MGR (Qualcomm Request Manager)**

On Qualcomm platforms, **CAM_REQ_MGR** is a kernel component that:

- **Synchronizes** sensor capture with ISP (IFE, BPS) — one request ties them together
- **Queues requests** — each request carries buffers and controls for the whole pipeline
- **Signals completion** — when the pipeline finishes a frame, an event is queued for userspace

Without the Request API, sensor and ISP could run out of sync (e.g. wrong exposure applied to a frame). The Request API guarantees: *this* sensor frame → *this* IFE config → *this* output buffer.

**Device Layout (Qualcomm Spectra)**

| Device | Role |
|--------|------|
| `/dev/video0` | Sync/request manager — camerad polls here for frame-done events |
| `/dev/media0` | Media controller — pipeline topology (sensor ↔ IFE ↔ BPS) |
| Subdevs (e.g. `/dev/v4l-subdev*`) | Sensor, IFE, BPS as separate entities; configured via media controller |

**Debugging V4L2**

```bash
# List video devices and capabilities
v4l2-ctl --list-devices

# List supported formats
v4l2-ctl -d /dev/video0 --list-formats-ext

# Media controller: show pipeline
media-ctl -d /dev/media0 -p
```

**References:** [V4L2 API (kernel.org)](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html), [VIDIOC_QBUF/VIDIOC_DQBUF](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/vidioc-qbuf.html), [V4L2 poll()](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/func-poll.html)

---

**5. ISP Pipeline**

```
Sensor (RAW Bayer) → CSI → IFE → [BPS for driver] → YUV (NV12)
```

**Output format:** NV12 (Y plane + interleaved UV plane). Standard format for vision/ML.

**Buffer flow:**

1. `SpectraMaster` initializes `/dev/video0`, sync, ISP, ICP
2. `SpectraCamera` per camera: sensor init, IFE config, BPS config (driver only), link devices
3. `CameraBuf` allocates raw (optional) and YUV buffers
4. `VisionIpcServer` creates shared-memory buffers for consumers (modeld, etc.)
5. On V4L2 frame event: `handle_camera_event` → `processFrame` → `sendFrameToVipc` + publish FrameData

---

</details>

## 6. 自动曝光 (AE)

camerad 实现**软件 AE**（不使用 sensor AEC）。目标：让目标区域保持在约 12.5% 灰（中位亮度）。

**算法**（`camera_qcom2.cc`）：

1. **测量：**`calculate_exposure_value()` 在 AE 矩形内对亮度分箱，返回中位灰度比例
2. **目标：**`target_grey_fraction` ≈ 0.125，按场景亮度缩放（越暗 → 目标越低）
3. **控制环：**类 PI 更新，约 3 帧延迟（sensor 寄存器缓冲）
4. **优化器：**对 `(exposure_time, analog_gain)` 暴力搜索，以最小化 `getExposureScore`（EV 误差）
5. **DC 增益：**低光下用高转换增益；加迟滞以避免闪烁

**AE 矩形**（每路相机，像素坐标）：

- Wide：`{96, 400, 1734, 524}` —— 帧的下部
- Road：`{96, 160, 1734, 986}` —— 帧的大部分
- Driver：`{96, 242, 1736, 906}` —— 人脸区域

**曝光参数：**`exposure_time`（µs）、`analog_gain`、`dc_gain_enabled`。经 I2C 发送给 sensor。

---

## 7. VisionIpc

**VisionIpc** = 面向视频帧的共享内存 IPC。避免拷贝；modeld 直接从共享缓冲区读取。

**服务端（camerad）：**

```cpp
VisionIpcServer v("camerad");
v.create_buffers_with_sizes(stream_type, VIPC_BUFFER_COUNT, width, height, yuv_size, stride, uv_offset);
// ...
vipc_server->send(cur_yuv_buf, &extra);  // on each frame
```

**客户端（modeld）：**

```python
from msgq.visionipc import VisionIpcClient, VisionStreamType
vipc = VisionIpcClient("camerad", 0, VisionStreamType.VISION_STREAM_ROAD)
# vipc.recv() → frame buffer
```

**流类型：**`VISION_STREAM_WIDE_ROAD`、`VISION_STREAM_ROAD`、`VISION_STREAM_DRIVER`

**缓冲区数量：**18（`VIPC_BUFFER_COUNT`）。允许生产者/消费者以不同速率运行。

---

## 8. 数据流

```
V4L2 poll(POLLPRI) on video0
    │
    ▼
VIDIOC_DQEVENT (frame done)
    │
    ├─► handle_camera_event()
    │       │
    │       ├─► processFrame() → YUV ready
    │       │
    │       ├─► sendFrameToVipc() → VisionIpc send
    │       │
    │       ├─► calculate_exposure_value() → grey_frac
    │       │
    │       ├─► set_camera_exposure() → I2C exposure registers
    │       │
    │       └─► pm->send("roadCameraState", FrameData)
    │
    └─► (next frame)
```

**FrameData**（cereal）：`frameId`、`timestampSof`、`timestampEof`、`integLines`、`gain`、`measuredGreyFraction`、`targetGreyFraction`、`exposureValPercent`、`sensor`、`processingTime`。

---


<details>
<summary>English original</summary>

**6. Auto Exposure (AE)**

camerad implements **software AE** (no sensor AEC). Goal: keep a target region at ~12.5% grey (median luminance).

**Algorithm** (`camera_qcom2.cc`):

1. **Measure:** `calculate_exposure_value()` bins luminance in an AE rectangle, returns median grey fraction
2. **Target:** `target_grey_fraction` ≈ 0.125, scaled by scene brightness (darker → lower target)
3. **Control loop:** PI-like update with ~3-frame latency (sensor register buffering)
4. **Optimizer:** Brute-force over `(exposure_time, analog_gain)` to minimize `getExposureScore` (EV error)
5. **DC gain:** High conversion gain for low light; hysteresis to avoid flicker

**AE rectangles** (per camera, in pixel coords):

- Wide: `{96, 400, 1734, 524}` — lower part of frame
- Road: `{96, 160, 1734, 986}` — most of frame
- Driver: `{96, 242, 1736, 906}` — face region

**Exposure params:** `exposure_time` (µs), `analog_gain`, `dc_gain_enabled`. Sent to sensor via I2C.

---

**7. VisionIpc**

**VisionIpc** = shared-memory IPC for video frames. Avoids copies; modeld reads directly from shared buffers.

**Server (camerad):**

```cpp
VisionIpcServer v("camerad");
v.create_buffers_with_sizes(stream_type, VIPC_BUFFER_COUNT, width, height, yuv_size, stride, uv_offset);
// ...
vipc_server->send(cur_yuv_buf, &extra);  // on each frame
```

**Client (modeld):**

```python
from msgq.visionipc import VisionIpcClient, VisionStreamType
vipc = VisionIpcClient("camerad", 0, VisionStreamType.VISION_STREAM_ROAD)
# vipc.recv() → frame buffer
```

**Stream types:** `VISION_STREAM_WIDE_ROAD`, `VISION_STREAM_ROAD`, `VISION_STREAM_DRIVER`

**Buffer count:** 18 (`VIPC_BUFFER_COUNT`). Allows producer/consumer to run at different rates.

---

**8. Data Flow**

```
V4L2 poll(POLLPRI) on video0
    │
    ▼
VIDIOC_DQEVENT (frame done)
    │
    ├─► handle_camera_event()
    │       │
    │       ├─► processFrame() → YUV ready
    │       │
    │       ├─► sendFrameToVipc() → VisionIpc send
    │       │
    │       ├─► calculate_exposure_value() → grey_frac
    │       │
    │       ├─► set_camera_exposure() → I2C exposure registers
    │       │
    │       └─► pm->send("roadCameraState", FrameData)
    │
    └─► (next frame)
```

**FrameData** (cereal): `frameId`, `timestampSof`, `timestampEof`, `integLines`, `gain`, `measuredGreyFraction`, `targetGreyFraction`, `exposureValPercent`, `sensor`, `processingTime`.

---

</details>

## 9. 源码地图

**本地路径（本路线图中）：** Autonomous Driving 文件夹下的 `openpilot/system/camerad/`。

| 组件                     | 路径                                                    | 含义 / 职责（含文件名含义）                                                                                     |
|-------------------------------|---------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| 主循环                     | `system/camerad/main.cc`                               | 主入口点；运行 `main()` 并将进程绑定到 CPU/核心；启动 camerad 主线程。                                               |
| 相机状态、AE（qcom2：Qualcomm Gen2） | `system/camerad/cameras/camera_qcom2.cc`               | 相机状态逻辑与自动曝光（AE）；"qcom2" 指 Qualcomm 第二代 ISP 平台（用于 Snapdragon SoC）。管理这些 ISP 的流水线与事件处理。 |
| 相机缓冲区、VIPC 发送      | `system/camerad/cameras/camera_common.cc`              | 相机环形缓冲区管理与 Vision IPC（进程间通信）数据发送。"common" 表示共享/通用的相机逻辑（不特定于平台或 sensor）。     |
| ISP（IFE、BPS、CDM：图像信号处理器、Bayer 处理、Camera Data Mover） | `system/camerad/cameras/spectra.cc`, `cdm.cc`          | Qualcomm "Spectra" 是 ISP 子系统。IFE = Image Front End（采集/初始处理），BPS = Bayer Processing Segment，CDM = Camera Data Mover（DMA/硬件卸载管理）。这些文件负责 ISP 的初始化与运行。 |
| Sensor 驱动                | `system/camerad/sensors/ox03c10.cc`, `os04c10.cc`      | 特定于 sensor 的驱动：`ox03c10` 与 `os04c10` 是图像 sensor 型号名（OmniVision OX03C10 & OS04C10）。每个文件包含初始化、I2C 寄存器访问与 sensor 设置。 |
| 相机配置（hw：hardware）  | `system/camerad/cameras/hw.h`                          | 相机硬件与配置头文件。"hw" 代表 "hardware"：定义所用不同相机的类型、能力与常量。                          |
| 内参（Python）           | `common/transformations/camera.py`                     | 相机内参/外参及变换、投影与校正逻辑，用 Python 编写。用于几何相机模型数学计算。 |
| VisionIpc                     | `msgq/visionipc/`                                      | VisionIPC（进程间通信）框架目录：共享内存与消息传递（server/client），用于进程间传输图像/帧数据。        |

---

## 10. 代码走读 —— 每一部分

使用 `../openpilot/system/camerad/` 处的**本地 openpilot 克隆**（相对于本指南）。按以下顺序阅读：

### 入口点

| 文件 | 作用 |
|------|---------------|
| **main.cc** | `main()` → 将进程绑定到 CPU 6 → 调用 `camerad_thread()`。无 RT 优先级（使用 isolcpus）。 |

### 配置

| 文件 | 作用 |
|------|---------------|
| **cameras/hw.h** | `CameraConfig` struct：`camera_num`、`stream_type`、`focal_len`、`publish_name`、`output_type`（IFE vs BPS）。定义 `WIDE_ROAD_CAMERA_CONFIG`、`ROAD_CAMERA_CONFIG`、`DRIVER_CAMERA_CONFIG`、`ALL_CAMERA_CONFIGS`。 |

### 核心循环与 AE（Qualcomm）

| 文件 | 作用 |
|------|---------------|
| **cameras/camera_qcom2.cc** | `camerad_thread()`：初始化 `SpectraMaster`，按配置创建 `CameraState`，启动 sensor，**轮询** `video0_fd` 以完成 `POLLPRI` → `VIDIOC_DQEVENT` → `handle_camera_event()` → `sendState()`。`CameraState`：保存 `SpectraCamera`、曝光参数、AE rect。`set_camera_exposure()`：类 PI 控制，在 (exp_t, gain_idx) 上暴力搜索，DC gain 滞回，通过 `sensors_i2c()` 进行 I2C。`set_exposure_rect()`：每个相机（wide/road/driver）的 AE 矩形。 |

### 缓冲区与 VIPC

| 文件 | 作用 |
|------|---------------|
| **cameras/camera_common.h** | `CameraBuf`：`vipc_server`、`stream_type`、`cur_buf_idx`、`cur_frame_data`、`cur_yuv_buf`、`camera_bufs_raw`。`FrameMetadata`：frame_id、request_id、timestamps。声明 `camerad_thread()`、`calculate_exposure_value()`、`get_raw_frame_image()`、`open_v4l_by_name_and_index()`。 |
| **cameras/camera_common.cc** | `CameraBuf::init()`：分配 raw 缓冲区（若为 BPS），创建 VIPC 缓冲区。`sendFrameToVipc()`：获取 YUV buf，设置 `VisionIpcBufExtra`，调用 `vipc_server->send()`。`calculate_exposure_value()`：在 `ae_xywh` 中对 Y 亮度分箱，返回 median/256。`open_v4l_by_name_and_index()`：扫描 `/sys/class/video4linux/` 查找 subdev 名称，打开 `/dev/v4l-subdevN`。 |


<details>
<summary>English original</summary>

**9. Source Map**

**Local path (in this roadmap):** `openpilot/system/camerad/` under the Autonomous Driving folder.

| Component                     | Path                                                    | Meaning / Responsibility (including file name meaning)                                                                                     |
|-------------------------------|---------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| Main loop                     | `system/camerad/main.cc`                               | Main entry point; runs `main()` and pins process to CPU/core; launches camerad main thread.                                               |
| Camera state, AE (qcom2: Qualcomm Gen2) | `system/camerad/cameras/camera_qcom2.cc`               | Camera state logic and auto-exposure (AE); "qcom2" refers to Qualcomm 2nd-Gen ISP platform (used in Snapdragon SoCs). Manages pipeline and event handling for these ISPs. |
| Camera buffer, VIPC send      | `system/camerad/cameras/camera_common.cc`              | Camera buffer ring management and Vision IPC (Interprocess Communication) data sending. "common" means shared/common camera logic (not platform- or sensor-specific).     |
| ISP (IFE, BPS, CDM: Image Signal Processor, Bayer processing, Camera Data Mover) | `system/camerad/cameras/spectra.cc`, `cdm.cc`          | Qualcomm "Spectra" is the ISP subsystem. IFE = Image Front End (captures/initial process), BPS = Bayer Processing Segment, CDM = Camera Data Mover (DMA/HW offload management). These files initialize and operate the ISP. |
| Sensor drivers                | `system/camerad/sensors/ox03c10.cc`, `os04c10.cc`      | Sensor-specific drivers: `ox03c10` and `os04c10` are image sensor model names (OmniVision OX03C10 & OS04C10). Each file contains initialization, I2C register access, and sensor settings. |
| Camera config (hw: hardware)  | `system/camerad/cameras/hw.h`                          | Camera hardware and configuration header. "hw" stands for "hardware": defines types, capabilities, and constants for different cameras used.                          |
| Intrinsics (Python)           | `common/transformations/camera.py`                     | Camera intrinsic/extrinsic parameters and transformations, projection and rectification logic, written in Python. Used for geometric camera model math. |
| VisionIpc                     | `msgq/visionipc/`                                      | VisionIPC (Inter-Process Communication) framework directory: shared memory and message-passing (server/client) for image/frame data transport between processes.        |

---

**10. Code Walkthrough — Every Piece**

Use the **local openpilot clone** at `../openpilot/system/camerad/` (relative to this Guide). Read in this order:

**Entry Point**

| File | What it does |
|------|---------------|
| **main.cc** | `main()` → pins process to CPU 6 → calls `camerad_thread()`. No RT priority (isolcpus used). |

**Configuration**

| File | What it does |
|------|---------------|
| **cameras/hw.h** | `CameraConfig` struct: `camera_num`, `stream_type`, `focal_len`, `publish_name`, `output_type` (IFE vs BPS). Defines `WIDE_ROAD_CAMERA_CONFIG`, `ROAD_CAMERA_CONFIG`, `DRIVER_CAMERA_CONFIG`, `ALL_CAMERA_CONFIGS`. |

**Core Loop & AE (Qualcomm)**

| File | What it does |
|------|---------------|
| **cameras/camera_qcom2.cc** | `camerad_thread()`: init `SpectraMaster`, create `CameraState` per config, start sensors, **poll** `video0_fd` for `POLLPRI` → `VIDIOC_DQEVENT` → `handle_camera_event()` → `sendState()`. `CameraState`: holds `SpectraCamera`, exposure params, AE rect. `set_camera_exposure()`: PI-like control, brute-force over (exp_t, gain_idx), DC gain hysteresis, I2C via `sensors_i2c()`. `set_exposure_rect()`: AE rectangles per camera (wide/road/driver). |

**Buffer & VIPC**

| File | What it does |
|------|---------------|
| **cameras/camera_common.h** | `CameraBuf`: `vipc_server`, `stream_type`, `cur_buf_idx`, `cur_frame_data`, `cur_yuv_buf`, `camera_bufs_raw`. `FrameMetadata`: frame_id, request_id, timestamps. Declares `camerad_thread()`, `calculate_exposure_value()`, `get_raw_frame_image()`, `open_v4l_by_name_and_index()`. |
| **cameras/camera_common.cc** | `CameraBuf::init()`: allocates raw buffers (if BPS), creates VIPC buffers. `sendFrameToVipc()`: gets YUV buf, sets `VisionIpcBufExtra`, calls `vipc_server->send()`. `calculate_exposure_value()`: bins Y luminance in `ae_xywh`, returns median/256. `open_v4l_by_name_and_index()`: scans `/sys/class/video4linux/` for subdev name, opens `/dev/v4l-subdevN`. |

</details>

### ISP (Spectra)

| 文件 | 作用 |
|------|---------------|
| **cameras/spectra.h** | `SpectraMaster`：`video0_fd`、`cam_sync_fd`、`isp_fd`、`icp_fd`、`MemoryManager`。`SpectraCamera`：sensor、IFE/BPS 配置、`handle_camera_event()`、`camera_open()`、`sensors_init/start/i2c()`、`config_ife()`、`config_bps()`、`enqueue_frame()`。`SpectraBuf`：mmap 的 DMA buffer。 |
| **cameras/spectra.cc** | `SpectraMaster::init()`：打开 `/dev/video0`、sync、ISP、ICP。`SpectraCamera`：sensor 探测、IFE/BPS 配置、关联设备、request 队列。`handle_camera_event()`：校验事件、调用 `processFrame()`、重新入队。CDM（Camera Data Mover）配置。 |
| **cameras/cdm.cc**、**cdm.h** | IFE/BPS 的 CDM 程序：DMA 描述符、striping。 |
| **cameras/ife.h** | IFE（Image Front End）寄存器布局、LUT。 |
| **cameras/bps_blobs.h** | BPS（Bayer Processing Segment）二进制 blob / 配置。 |
| **cameras/nv12_info.h**、**nv12_info.py** | 不同分辨率的 NV12 布局（stride、uv_offset）。 |

### 传感器

| 文件 | 作用 |
|------|---------------|
| **sensors/sensor.h** | `SensorInfo`：帧尺寸、曝光上限、模拟增益、DC gain、CCM、gamma LUT、线性化、渐晕。`OX03C10`、`OS04C10` 子类：`getExposureRegisters()`、`getExposureScore()`、`getSlaveAddress()`。 |
| **sensors/ox03c10.cc** | OX03C10 初始化、曝光寄存器写入、I2C 从机地址。 |
| **sensors/ox03c10_registers.h** | OX03C10 的寄存器地址。 |
| **sensors/os04c10.cc** | OS04C10 初始化、`ife_downscale_configure()`、曝光。 |
| **sensors/os04c10_registers.h** | OS04C10 的寄存器地址。 |

### 其他

| 文件 | 作用 |
|------|---------------|
| **snapshot.py** | 抓取一帧的工具（用于调试）。 |
| **SConscript** | camerad 的构建规则。 |
| **test/** | `test_camerad.py`、`debug.sh`、`stress_restart.sh`、`test_ae_gray.cc` —— 测试与调试脚本。 |

### 建议阅读顺序

1. **main.cc** —— 入口
2. **hw.h** —— 配置
3. **camera_common.h** + **camera_common.cc** —— buffer、VIPC、AE 测量
4. **sensor.h** —— sensor 接口
5. **camera_qcom2.cc** —— 主循环、AE 控制、`camerad_thread()`
6. **spectra.h** + **spectra.cc** —— ISP 初始化、`handle_camera_event`、`processFrame`
7. **ox03c10.cc** / **os04c10.cc** —— 传感器相关的初始化与曝光

---

## 11. 关键概念

| 概念 | 说明 |
|---------|-------------|
| **NV12** | YUV 4:2:0：Y 平面全分辨率，UV 交织半分辨率。常见于 ISP 输出与 ML。 |
| **VisionIpc** | 通过共享内存实现零拷贝帧共享。服务端创建 buffer；客户端按流类型订阅。 |
| **FrameData** | 带帧元数据的 Cereal 消息。用于同步（frame_id、时间戳）与 AE 调试。 |
| **ISP** | 图像信号处理器。将 RAW Bayer → demosaic → 颜色校正 → gamma → YUV。 |
| **AE** | 自动曝光。软件环路：测量亮度 → 计算目标 EV → 设置 sensor 曝光/增益。 |
| **V4L2** | Video4Linux2 —— Linux 内核视频采集 API。设备节点（`/dev/video*`）、ioctl（QBUF/DQBUF）、Request API。 |
| **Request Manager** | V4L2 CAM_REQ_MGR：把 sensor 采集绑定到 ISP 流水线。一个 request = 一帧通过流水线。 |
| **QBUF/DQBUF** | V4L2 buffer 交换：QBUF 入队空 buffer，DQBUF 出队已填充 buffer。 |

---

## 延伸阅读

- **本地 openpilot camerad：** `../openpilot/system/camerad/`（相对于本指南）
- **V4L2 API：** [kernel.org V4L2 文档](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html) —— 设备节点、ioctl、buffer 流、Request API
- **追踪流水线：** 从 `camera_qcom2.cc` 中的 `camerad_thread()` 开始，沿 `handle_camera_event` → `processFrame` → `sendFrameToVipc` 跟进
- **modeld 消费：** `selfdrive/modeld/modeld.py` —— `VisionIpcClient` 用于 `VISION_STREAM_ROAD`（若使用 wide 则含 wide）
- **校准：** `common/transformations/camera.py`、`get_warp_matrix`，用于模型输入 warp


<details>
<summary>English original</summary>

**ISP (Spectra)**

| File | What it does |
|------|---------------|
| **cameras/spectra.h** | `SpectraMaster`: `video0_fd`, `cam_sync_fd`, `isp_fd`, `icp_fd`, `MemoryManager`. `SpectraCamera`: sensor, IFE/BPS config, `handle_camera_event()`, `camera_open()`, `sensors_init/start/i2c()`, `config_ife()`, `config_bps()`, `enqueue_frame()`. `SpectraBuf`: mmap'd DMA buffer. |
| **cameras/spectra.cc** | `SpectraMaster::init()`: opens `/dev/video0`, sync, ISP, ICP. `SpectraCamera`: sensor probe, IFE/BPS setup, link devices, request queue. `handle_camera_event()`: validates event, calls `processFrame()`, re-enqueues. CDM (Camera Data Mover) setup. |
| **cameras/cdm.cc**, **cdm.h** | CDM programs for IFE/BPS: DMA descriptors, striping. |
| **cameras/ife.h** | IFE (Image Front End) register layouts, LUTs. |
| **cameras/bps_blobs.h** | BPS (Bayer Processing Segment) binary blobs / config. |
| **cameras/nv12_info.h**, **nv12_info.py** | NV12 layout (stride, uv_offset) for different resolutions. |

**Sensors**

| File | What it does |
|------|---------------|
| **sensors/sensor.h** | `SensorInfo`: frame dimensions, exposure limits, analog gains, DC gain, CCM, gamma LUT, linearization, vignetting. `OX03C10`, `OS04C10` subclasses: `getExposureRegisters()`, `getExposureScore()`, `getSlaveAddress()`. |
| **sensors/ox03c10.cc** | OX03C10 init, exposure register writes, I2C slave addr. |
| **sensors/ox03c10_registers.h** | Register addresses for OX03C10. |
| **sensors/os04c10.cc** | OS04C10 init, `ife_downscale_configure()`, exposure. |
| **sensors/os04c10_registers.h** | Register addresses for OS04C10. |

**Other**

| File | What it does |
|------|---------------|
| **snapshot.py** | Utility to capture a frame (for debugging). |
| **SConscript** | Build rules for camerad. |
| **test/** | `test_camerad.py`, `debug.sh`, `stress_restart.sh`, `test_ae_gray.cc` — tests and debug scripts. |

**Reading Order (Suggested)**

1. **main.cc** — entry
2. **hw.h** — config
3. **camera_common.h** + **camera_common.cc** — buffers, VIPC, AE measurement
4. **sensor.h** — sensor interface
5. **camera_qcom2.cc** — main loop, AE control, `camerad_thread()`
6. **spectra.h** + **spectra.cc** — ISP init, `handle_camera_event`, `processFrame`
7. **ox03c10.cc** / **os04c10.cc** — sensor-specific init and exposure

---

**11. Key Concepts**

| Concept | Description |
|---------|-------------|
| **NV12** | YUV 4:2:0: Y plane full res, UV interleaved half res. Common for ISP output and ML. |
| **VisionIpc** | Zero-copy frame sharing via shared memory. Server creates buffers; clients subscribe by stream type. |
| **FrameData** | Cereal message with frame metadata. Used for sync (frame_id, timestamps) and AE debugging. |
| **ISP** | Image Signal Processor. Converts RAW Bayer → demosaic → color correct → gamma → YUV. |
| **AE** | Auto exposure. Software loop: measure luminance → compute desired EV → set sensor exposure/gain. |
| **V4L2** | Video4Linux2 — Linux kernel API for video capture. Device nodes (`/dev/video*`), ioctls (QBUF/DQBUF), Request API. |
| **Request Manager** | V4L2 CAM_REQ_MGR: ties sensor capture to ISP pipeline. One request = one frame through the pipeline. |
| **QBUF/DQBUF** | V4L2 buffer exchange: QBUF enqueue empty buffer, DQBUF dequeue filled buffer. |

---

**Further Reading**

- **Local openpilot camerad:** `../openpilot/system/camerad/` (relative to this Guide)
- **V4L2 API:** [kernel.org V4L2 documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html) — device nodes, ioctls, buffer flow, Request API
- **Trace the pipeline:** Start at `camerad_thread()` in `camera_qcom2.cc`, follow `handle_camera_event` → `processFrame` → `sendFrameToVipc`
- **modeld consumption:** `selfdrive/modeld/modeld.py` — `VisionIpcClient` for `VISION_STREAM_ROAD` (and wide if used)
- **Calibration:** `common/transformations/camera.py`, `get_warp_matrix` for model input warp

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/2. openpilot Reference Stack/camerad/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/2.%20openpilot%20Reference%20Stack/camerad/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
