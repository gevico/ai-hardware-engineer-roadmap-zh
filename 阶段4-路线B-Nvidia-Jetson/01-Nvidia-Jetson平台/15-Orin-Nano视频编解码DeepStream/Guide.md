---
title: Jetson Orin Nano 8GB —— 视频编解码硬件、GStreamer 流水线与 DeepStream SDK
description: Jetson Orin Nano 8GB —— 视频编解码硬件、GStreamer 流水线与 DeepStream SDK
---

# Jetson Orin Nano 8GB —— 视频编解码硬件、GStreamer 流水线与 DeepStream SDK

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">JON8</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · Jetson 专题</p>
<p class="course-identity__title">Jetson Orin Nano 8GB 的专门课程标识——视频编解码硬件、GStreamer 流水线与 DeepStream SDK。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


> **目标平台：** Jetson Orin Nano 8GB Developer Kit（T234 SoC、Ampere 架构 GPU、JetPack 6.x / L4T 36.x）。
>
> **前置要求：** 熟悉 [Orin Nano 内存架构](../07-Orin-Nano内存架构/Guide.md)（NVMM、零拷贝、DMA-BUF）与 [实时推理](../11-Orin-Nano实时推理/Guide.md)（TensorRT engine 构建）。

---


## 1. 引言

### 1.1 边缘 AI 中的视频处理

边缘 AI 部署绝大多数都涉及视频。监控摄像头、自主机器人、工业检测系统与智慧城市基础设施都会产生连续的视频流，这些视频流必须实时解码、分析，且往往还需重新编码。Jetson Orin Nano 8GB 正是为这类工作负载专门打造的：其 T234 SoC 在 Ampere 架构 GPU 之外集成了固定功能视频编解码硬件，使流水线中的帧从摄像头传感器出发，依次经过解码、推理与编码，全程无需 CPU 参与像素级处理。

一个典型的边缘视频分析流水线：

```
  [IP Camera]          [IP Camera]          [USB Camera]
      |                     |                     |
      v                     v                     v
  RTSP Decode (NVDEC)  RTSP Decode (NVDEC)  V4L2 Capture
      |                     |                     |
      +----------+----------+----------+----------+
                 |                     |
                 v                     v
           Stream Muxer          nvvideoconvert
           (batching)            (sys RAM -> NVMM)
                 |                     |
                 +----------+----------+
                            |
                            v
                   TensorRT Inference (GPU)
                            |
                            v
                   Object Tracker (GPU/CPU)
                            |
                   +--------+--------+
                   |                 |
                   v                 v
              OSD Overlay       Metadata Export
              (bboxes)          (Kafka / MQTT)
                   |
                   v
              H.265 Encode (NVENC)
                   |
                   v
              RTSP Server Output
```

### 1.2 硬件加速与软件编解码器

软件编解码器（libx264、libx265、libvpx）在 CPU 核上执行。在 Orin Nano 的六核 Arm Cortex-A78AE 复合体中，单路 1080p H.265 软件编码就会消耗多个核 80-100% 的算力，没有余量留给应用逻辑或推理。

硬件编解码引擎（NVDEC、NVENC、NVJPEG）是 T234 裸片上的专用 ASIC 模块。它们独立于 CPU 和 GPU 运行：

| 指标                    | 软件（libx265）  | 硬件（NVENC H.265） |
|---------------------------|---------------------|------------------------|
| 1080p30 编码 CPU 占用  | 300-400%（3-4 核）| < 5%（控制面）   |
| 1080p30 编码 GPU 占用  | 0%                  | 0%（独立 ASIC）     |
| 1080p30 编码延迟    | 15-40 ms/帧      | 3-8 ms/帧           |
| 1080p30 编码功耗      | 额外 4-6W     | 额外 < 1W        |
| 并发流        | 1-2（受 CPU 限制）   | 4+（受硬件限制）  |

硬件路径把 CPU 解放出来用于控制面逻辑、I/O 与编排。GPU 则完全留给 CUDA kernel 与 TensorRT 推理使用。每编码一帧的功耗仅为软件路径的一小部分，这对 7-15W 的功耗预算至关重要。


<details>
<summary>English original</summary>

**Jetson Orin Nano 8GB -- Video Codec Hardware, GStreamer Pipelines, and DeepStream SDK**

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">JON8</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Jetson Orin Nano 8GB -- Video Codec Hardware, GStreamer Pipelines, and DeepStream SDK.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Target:** Jetson Orin Nano 8GB Developer Kit (T234 SoC, Ampere GPU, JetPack 6.x / L4T 36.x).
>
> **Prerequisites:** Familiarity with the [Orin Nano memory architecture](../07-Orin-Nano内存架构/Guide.md) (NVMM, zero-copy, DMA-BUF) and [real-time inference](../11-Orin-Nano实时推理/Guide.md) (TensorRT engine building).

---


**1. Introduction**

**1.1 Video Processing in Edge AI**

Edge AI deployments overwhelmingly involve video. Surveillance cameras, autonomous robots,
industrial inspection systems, and smart-city infrastructure all generate continuous video
feeds that must be decoded, analyzed, and often re-encoded in real time. The Jetson Orin
Nano 8GB is purpose-built for this class of workload: its T234 SoC integrates fixed-function
video codec hardware alongside an Ampere-architecture GPU, enabling a pipeline where frames
flow from camera sensor through decode, inference, and encode without ever touching the CPU
for pixel-level work.

A representative edge video analytics pipeline:

```
  [IP Camera]          [IP Camera]          [USB Camera]
      |                     |                     |
      v                     v                     v
  RTSP Decode (NVDEC)  RTSP Decode (NVDEC)  V4L2 Capture
      |                     |                     |
      +----------+----------+----------+----------+
                 |                     |
                 v                     v
           Stream Muxer          nvvideoconvert
           (batching)            (sys RAM -> NVMM)
                 |                     |
                 +----------+----------+
                            |
                            v
                   TensorRT Inference (GPU)
                            |
                            v
                   Object Tracker (GPU/CPU)
                            |
                   +--------+--------+
                   |                 |
                   v                 v
              OSD Overlay       Metadata Export
              (bboxes)          (Kafka / MQTT)
                   |
                   v
              H.265 Encode (NVENC)
                   |
                   v
              RTSP Server Output
```

**1.2 Hardware Acceleration vs Software Codecs**

Software codecs (libx264, libx265, libvpx) execute on CPU cores. On the Orin Nano's
six-core Arm Cortex-A78AE complex, a single 1080p H.265 software encode consumes
80-100% of multiple cores, leaving no headroom for application logic or inference.

Hardware codec engines (NVDEC, NVENC, NVJPEG) are dedicated ASIC blocks on the T234 die.
They operate independently of both the CPU and GPU:

| Metric                    | Software (libx265)  | Hardware (NVENC H.265) |
|---------------------------|---------------------|------------------------|
| 1080p30 encode CPU usage  | 300-400% (3-4 cores)| < 5% (control plane)   |
| 1080p30 encode GPU usage  | 0%                  | 0% (separate ASIC)     |
| 1080p30 encode latency    | 15-40 ms/frame      | 3-8 ms/frame           |
| 1080p30 encode power      | 4-6W additional     | < 1W additional        |
| Concurrent streams        | 1-2 (CPU-limited)   | 4+ (hardware-limited)  |

The hardware path frees the CPU for control-plane logic, I/O, and orchestration. The GPU
remains fully available for CUDA kernels and TensorRT inference. Power per encoded frame
is a fraction of the software path, which is critical for 7-15W power budgets.

</details>

### 1.3 T234 SoC 视频引擎概述

T234 包含四个专用于视频与图像处理的硬件引擎：

```
+-------------------------------------------------------------------+
|                        T234 SoC Die                               |
|                                                                   |
|  +--------+  +--------+  +--------+  +--------+  +-------------+ |
|  | NVDEC  |  | NVENC  |  | NVJPEG |  |  VIC   |  | Ampere GPU  | |
|  | (1x)   |  | (1x)   |  | (1x)   |  | (1x)   |  | 1024 CUDA   | |
|  |        |  |        |  |        |  |        |  | cores       | |
|  +---+----+  +---+----+  +---+----+  +---+----+  +------+------+ |
|      |           |           |           |               |        |
|      +-----+-----+-----+----+-----+-----+               |        |
|            |                       |                     |        |
|       +----v-----------------------v---------------------v----+   |
|       |              LPDDR5 Memory Subsystem (8GB)            |   |
|       |              (102.4 GB/s bandwidth, shared)           |   |
|       +-------------------------------------------------------+   |
+-------------------------------------------------------------------+
```

| 引擎 | 全称              | 功能                                    |
|--------|------------------------|---------------------------------------------|
| NVDEC  | 视频解码器          | 固定功能 H.264/H.265/VP9/AV1 解码   |
| NVENC  | 视频编码器          | 固定功能 H.264/H.265 编码            |
| NVJPEG | JPEG 引擎      | 硬件 JPEG 编码与解码               |
| VIC    | 视频图像合成器 | 缩放、色彩转换、旋转、合成 |

所有引擎均通过 SMMU 经 DMA 访问 DRAM。它们输出到 NVMM（NVIDIA Multimedia
Memory）缓冲区，GPU 与 DLA 可零拷贝读取。这是零拷贝流水线的基础，
该流水线在第 14 节中描述。

### 1.4 JetPack 与软件栈版本

| JetPack | L4T    | DeepStream | GStreamer | 主要变更                      |
|---------|--------|------------|-----------|----------------------------------|
| 5.1.2   | 35.4.1 | 6.3        | 1.16      | 首次支持 Orin Nano        |
| 6.0     | 36.3   | 6.4 / 7.0 | 1.20      | AV1 解码、新 tracker、DS 7.0 |
| 6.1     | 36.4   | 7.1        | 1.20      | 性能改进         |

DeepStream 版本必须与 JetPack 严格匹配。跨版本组合会导致
符号解析错误与静默数据损坏。

---

## 2. NVDEC 硬件 —— 视频解码器引擎

### 2.1 解码器模块架构

T234 包含一个 NVDEC 实例。这是一个固定功能硬件模块，接受
压缩码流，并将解码后的帧以 NV12（10-bit 内容为 NV12_10LE）
经 DMA 输出到 DRAM。解码器相对 CPU 与 GPU 均异步运行。

驱动将 NVDEC 暴露为 V4L2 Memory-to-Memory（M2M）设备。用户态流程为：

```
  1. Open /dev/video0 (decoder device)
  2. Set OUTPUT format (compressed: H264, H265, VP9, AV1)
  3. Set CAPTURE format (raw: NV12)
  4. STREAMON on both queues
  5. Queue compressed NAL units to OUTPUT
  6. Dequeue decoded frames from CAPTURE
```

GStreamer 的 `nvv4l2decoder` 封装该 V4L2 接口，并输出 NVMM 缓冲区。

### 2.2 支持的编解码器与 Profile

| 编解码器   | 支持的 Profile                           | 最大位深 | 色度    |
|---------|----------------------------------------------|---------------|-----------|
| H.264   | Baseline、Main、High、High 10                | 10-bit        | 4:2:0     |
| H.265   | Main、Main 10、Main Still Picture             | 10-bit        | 4:2:0     |
| VP9     | Profile 0、Profile 2（10-bit）                | 10-bit        | 4:2:0     |
| AV1     | Main Profile                                  | 10-bit        | 4:2:0     |

AV1 解码需要 JetPack 6.0 或更高版本。10-bit 解码输出 NV12_10LE 格式；
在送入期望 8-bit 的 element 之前，需要经 VIC（nvvideoconvert）做下游转换。

### 2.3 最大分辨率与吞吐

| 编解码器 | 最大分辨率 | 峰值解码吞吐                        |
|-------|----------------|-----------------------------------------------|
| H.264 | 4096 x 4096    | 1x 4K30 或 2x 1080p60 或 8x 720p30           |
| H.265 | 8192 x 8192    | 1x 4K60 或 2x 4K30 或 4x 1080p60             |
| VP9   | 8192 x 8192    | 1x 4K60 或 2x 4K30                            |
| AV1   | 8192 x 4320    | 1x 4K30                                        |

以上为硬件峰值速率。实际吞吐取决于码流复杂度
（高运动 / 高 QP 码流解码更快）、与 GPU 及其他引擎的内存带宽争用，
以及热管理状态（结温超过约 85C 时触发降频）。


<details>
<summary>English original</summary>

**1.3 T234 SoC Video Engine Overview**

The T234 contains four dedicated hardware engines for video and image processing:

```
+-------------------------------------------------------------------+
|                        T234 SoC Die                               |
|                                                                   |
|  +--------+  +--------+  +--------+  +--------+  +-------------+ |
|  | NVDEC  |  | NVENC  |  | NVJPEG |  |  VIC   |  | Ampere GPU  | |
|  | (1x)   |  | (1x)   |  | (1x)   |  | (1x)   |  | 1024 CUDA   | |
|  |        |  |        |  |        |  |        |  | cores       | |
|  +---+----+  +---+----+  +---+----+  +---+----+  +------+------+ |
|      |           |           |           |               |        |
|      +-----+-----+-----+----+-----+-----+               |        |
|            |                       |                     |        |
|       +----v-----------------------v---------------------v----+   |
|       |              LPDDR5 Memory Subsystem (8GB)            |   |
|       |              (102.4 GB/s bandwidth, shared)           |   |
|       +-------------------------------------------------------+   |
+-------------------------------------------------------------------+
```

| Engine | Full Name              | Function                                    |
|--------|------------------------|---------------------------------------------|
| NVDEC  | Video Decoder          | Fixed-function H.264/H.265/VP9/AV1 decode   |
| NVENC  | Video Encoder          | Fixed-function H.264/H.265 encode            |
| NVJPEG | JPEG Engine            | Hardware JPEG encode and decode               |
| VIC    | Video Image Compositor | Scaling, color conversion, rotation, compose |

All engines access DRAM via DMA through the SMMU. They output to NVMM (NVIDIA Multimedia
Memory) buffers that are zero-copy readable by the GPU and DLA. This is the foundation
of the zero-copy pipeline described in Section 14.

**1.4 JetPack and Software Stack Versions**

| JetPack | L4T    | DeepStream | GStreamer | Key Changes                      |
|---------|--------|------------|-----------|----------------------------------|
| 5.1.2   | 35.4.1 | 6.3        | 1.16      | Orin Nano initial support        |
| 6.0     | 36.3   | 6.4 / 7.0 | 1.20      | AV1 decode, new tracker, DS 7.0 |
| 6.1     | 36.4   | 7.1        | 1.20      | Performance improvements         |

Always match DeepStream version to JetPack exactly. Cross-version combinations cause
symbol resolution errors and silent data corruption.

---

**2. NVDEC Hardware -- Video Decoder Engine**

**2.1 Decoder Block Architecture**

The T234 contains one NVDEC instance. This is a fixed-function hardware block that accepts
compressed bitstreams and produces decoded frames in NV12 (or NV12_10LE for 10-bit content)
into DRAM via DMA. The decoder runs asynchronously with respect to both the CPU and GPU.

The driver exposes NVDEC as a V4L2 Memory-to-Memory (M2M) device. The userspace flow:

```
  1. Open /dev/video0 (decoder device)
  2. Set OUTPUT format (compressed: H264, H265, VP9, AV1)
  3. Set CAPTURE format (raw: NV12)
  4. STREAMON on both queues
  5. Queue compressed NAL units to OUTPUT
  6. Dequeue decoded frames from CAPTURE
```

GStreamer's `nvv4l2decoder` wraps this V4L2 interface and outputs NVMM buffers.

**2.2 Supported Codecs and Profiles**

| Codec   | Profiles Supported                           | Max Bit Depth | Chroma    |
|---------|----------------------------------------------|---------------|-----------|
| H.264   | Baseline, Main, High, High 10                | 10-bit        | 4:2:0     |
| H.265   | Main, Main 10, Main Still Picture             | 10-bit        | 4:2:0     |
| VP9     | Profile 0, Profile 2 (10-bit)                | 10-bit        | 4:2:0     |
| AV1     | Main Profile                                  | 10-bit        | 4:2:0     |

AV1 decode requires JetPack 6.0 or later. 10-bit decode outputs NV12_10LE format;
downstream conversion via VIC (nvvideoconvert) is needed before elements expecting 8-bit.

**2.3 Maximum Resolution and Throughput**

| Codec | Max Resolution | Peak Decode Throughput                        |
|-------|----------------|-----------------------------------------------|
| H.264 | 4096 x 4096    | 1x 4K30 or 2x 1080p60 or 8x 720p30           |
| H.265 | 8192 x 8192    | 1x 4K60 or 2x 4K30 or 4x 1080p60             |
| VP9   | 8192 x 8192    | 1x 4K60 or 2x 4K30                            |
| AV1   | 8192 x 4320    | 1x 4K30                                        |

These are hardware peak rates. Actual throughput depends on bitstream complexity (high
motion / high QP streams decode faster), memory bandwidth contention with GPU and other
engines, and thermal state (throttling above ~85C junction temperature).

</details>

### 2.4 并发的 decode 流

单个 NVDEC 引擎在多个并发 decode 会话之间做时间切片。实际限制由总像素吞吐决定，而非固定的流数量：

```
Total pixels/s budget (H.265): ~500 Mpixels/s

Scenario A: 4x 1080p30 H.265  = 4 * 1920 * 1080 * 30 = 249M pixels/s   [OK]
Scenario B: 8x 720p30  H.265  = 8 * 1280 *  720 * 30 = 221M pixels/s   [OK]
Scenario C: 1x 4K60    H.265  = 1 * 3840 * 2160 * 60 = 497M pixels/s   [OK]
Scenario D: 2x 4K60    H.265  = 2 * 3840 * 2160 * 60 = 995M pixels/s   [EXCEEDS]
Scenario E: 1x 4K30 + 4x 720p30 = 249M + 111M        = 360M pixels/s   [OK]
```

当像素预算被超出时，NVDEC 无法保持实时，帧会被丢弃或排队，导致延迟不断增大。

### 2.5 解码器延迟

| 配置                             | 典型 decode 延迟           |
|-----------------------------------|----------------------------|
| H.264 Baseline（无 B 帧）     | 1-2 帧（33-66 ms @30）  |
| H.264 Main/High（含 B 帧）  | 2-4 帧（66-133 ms @30） |
| H.265 Main（无 B 帧）         | 1-3 帧（33-100 ms @30） |
| AV1 Main                         | 2-4 帧（66-133 ms @30） |

对于低延迟应用（机器人、遥操作），产生该流的编码器应配置为不使用 B 帧并使用短 GOP 长度。解码器有一个
`enable-max-performance` 属性用于锁定 NVDEC 时钟：

```bash
gst-launch-1.0 rtspsrc location=rtsp://camera/live latency=0 ! \
  rtph265depay ! h265parse ! nvv4l2decoder enable-max-performance=true ! \
  nvvideoconvert ! nv3dsink sync=false
```

### 2.6 查询 NVDEC 能力

```bash
# List V4L2 video devices (decoder is typically /dev/video0)
ls -la /dev/video*

# Query decoder device capabilities
v4l2-ctl -d /dev/video0 --all

# List supported compressed input formats (OUTPUT queue)
v4l2-ctl -d /dev/video0 --list-formats-out
# Expected output includes: H264, H265, VP9, AV1

# List supported raw output formats (CAPTURE queue)
v4l2-ctl -d /dev/video0 --list-formats
# Expected output includes: NV12, NV12M

# Check current NVDEC clock and utilization
sudo cat /sys/kernel/debug/clk/nvdec/clk_rate
sudo tegrastats --interval 500
# Look for NVDEC% in tegrastats output
```

---

## 3. NVENC 硬件 —— 视频编码引擎

### 3.1 编码器模块架构

T234 包含一个 NVENC 实例。与 NVDEC 一样，这是一个固定功能 ASIC 模块，接受原始帧（NV12、I420、10-bit 的 P010）并产生压缩码流。编码器通过 DMA 工作，独立于 CPU 和 GPU 运行。

驱动将 NVENC 暴露为 V4L2 M2M 设备（通常为 `/dev/video1`）。该流程与解码器类似，但方向相反：原始帧进入 OUTPUT 队列，压缩 NAL 单元从 CAPTURE 队列输出。

### 3.2 支持的编解码器与 Profile

| 编解码器   | 支持的 Profile          | 最大位深 | 色度    |
|---------|-----------------------------|---------------|-----------|
| H.264   | Baseline、Main、High        | 8-bit         | 4:2:0     |
| H.265   | Main、Main 10               | 10-bit        | 4:2:0     |

与 NVDEC 不同，编码器不支持 VP9 或 AV1 编码。这种不对称在边缘 SoC 中很常见 —— decode 支持比 encode 更广泛。

### 3.3 最大分辨率与吞吐

| 编解码器 | 最大编码分辨率 | 峰值编码吞吐                         |
|-------|-----------------------|------------------------------------------------|
| H.264 | 4096 x 4096           | 1x 4K30 或 2x 1080p60 或 4x 1080p30           |
| H.265 | 8192 x 8192           | 1x 4K30 或 2x 1080p60 或 4x 1080p30           |


<details>
<summary>English original</summary>

**2.4 Simultaneous Decode Streams**

The single NVDEC engine time-slices across multiple concurrent decode sessions. The
practical limit is determined by total pixel throughput, not a fixed stream count:

```
Total pixels/s budget (H.265): ~500 Mpixels/s

Scenario A: 4x 1080p30 H.265  = 4 * 1920 * 1080 * 30 = 249M pixels/s   [OK]
Scenario B: 8x 720p30  H.265  = 8 * 1280 *  720 * 30 = 221M pixels/s   [OK]
Scenario C: 1x 4K60    H.265  = 1 * 3840 * 2160 * 60 = 497M pixels/s   [OK]
Scenario D: 2x 4K60    H.265  = 2 * 3840 * 2160 * 60 = 995M pixels/s   [EXCEEDS]
Scenario E: 1x 4K30 + 4x 720p30 = 249M + 111M        = 360M pixels/s   [OK]
```

When the pixel budget is exceeded, NVDEC cannot maintain real-time and frames will be
dropped or queued, causing increasing latency.

**2.5 Decoder Latency**

| Configuration                     | Typical Decode Latency     |
|-----------------------------------|----------------------------|
| H.264 Baseline (no B-frames)     | 1-2 frames (33-66 ms @30)  |
| H.264 Main/High (with B-frames)  | 2-4 frames (66-133 ms @30) |
| H.265 Main (no B-frames)         | 1-3 frames (33-100 ms @30) |
| AV1 Main                         | 2-4 frames (66-133 ms @30) |

For low-latency applications (robotics, teleoperation), the encoder producing the stream
should be configured without B-frames and with short GOP lengths. The decoder has a
`enable-max-performance` property to lock NVDEC clocks:

```bash
gst-launch-1.0 rtspsrc location=rtsp://camera/live latency=0 ! \
  rtph265depay ! h265parse ! nvv4l2decoder enable-max-performance=true ! \
  nvvideoconvert ! nv3dsink sync=false
```

**2.6 Querying NVDEC Capabilities**

```bash
# List V4L2 video devices (decoder is typically /dev/video0)
ls -la /dev/video*

# Query decoder device capabilities
v4l2-ctl -d /dev/video0 --all

# List supported compressed input formats (OUTPUT queue)
v4l2-ctl -d /dev/video0 --list-formats-out
# Expected output includes: H264, H265, VP9, AV1

# List supported raw output formats (CAPTURE queue)
v4l2-ctl -d /dev/video0 --list-formats
# Expected output includes: NV12, NV12M

# Check current NVDEC clock and utilization
sudo cat /sys/kernel/debug/clk/nvdec/clk_rate
sudo tegrastats --interval 500
# Look for NVDEC% in tegrastats output
```

---

**3. NVENC Hardware -- Video Encoder Engine**

**3.1 Encoder Block Architecture**

The T234 contains one NVENC instance. Like NVDEC, this is a fixed-function ASIC block
that accepts raw frames (NV12, I420, P010 for 10-bit) and produces compressed bitstreams.
The encoder operates via DMA and runs independently of the CPU and GPU.

The driver exposes NVENC as a V4L2 M2M device (typically `/dev/video1`). The flow mirrors
the decoder but in reverse: raw frames go into the OUTPUT queue, compressed NAL units come
out of the CAPTURE queue.

**3.2 Supported Codecs and Profiles**

| Codec   | Profiles Supported          | Max Bit Depth | Chroma    |
|---------|-----------------------------|---------------|-----------|
| H.264   | Baseline, Main, High        | 8-bit         | 4:2:0     |
| H.265   | Main, Main 10               | 10-bit        | 4:2:0     |

Unlike NVDEC, the encoder does not support VP9 or AV1 encoding. This asymmetry is
common in edge SoCs -- decode support is broader than encode.

**3.3 Maximum Resolution and Throughput**

| Codec | Max Encode Resolution | Peak Encode Throughput                         |
|-------|-----------------------|------------------------------------------------|
| H.264 | 4096 x 4096           | 1x 4K30 or 2x 1080p60 or 4x 1080p30           |
| H.265 | 8192 x 8192           | 1x 4K30 or 2x 1080p60 or 4x 1080p30           |

</details>

### 3.4 码率控制模式

| 模式 | GStreamer 取值 | 说明                                                |
|------|-----------------|------------------------------------------------------------|
| VBR  | control-rate=0  | 可变比特率。画质随之变化，以命中平均比特率。   |
| CBR  | control-rate=1  | 恒定比特率。最适合固定带宽流式传输。      |
| CQP  | control-rate=2  | 恒定 QP。画质固定，文件大小可变。            |

```bash
# CBR at 4 Mbps
gst-launch-1.0 videotestsrc num-buffers=300 ! \
  'video/x-raw,width=1920,height=1080,framerate=30/1' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 control-rate=1 ! \
  h265parse ! mp4mux ! filesink location=cbr_output.mp4

# VBR with peak 8 Mbps, average 4 Mbps
gst-launch-1.0 videotestsrc num-buffers=300 ! \
  'video/x-raw,width=1920,height=1080,framerate=30/1' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 peak-bitrate=8000000 control-rate=0 ! \
  h265parse ! mp4mux ! filesink location=vbr_output.mp4

# CQP with explicit QP values for I, P, B frames
gst-launch-1.0 videotestsrc num-buffers=300 ! \
  'video/x-raw,width=1920,height=1080,framerate=30/1' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc control-rate=2 quant-i-frames=20 quant-p-frames=23 quant-b-frames=25 ! \
  h265parse ! mp4mux ! filesink location=cqp_output.mp4
```

### 3.5 B 帧支持与 GOP 结构

Orin Nano 上的 NVENC 对 H.264 和 H.265 均支持 B 帧：

```bash
# 2 B-frames, IDR every 30 frames
gst-launch-1.0 videotestsrc num-buffers=300 ! \
  'video/x-raw,width=1920,height=1080,framerate=30/1' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h264enc num-b-frames=2 idrinterval=30 bitrate=4000000 ! \
  h264parse ! mp4mux ! filesink location=bframe_output.mp4
```

GOP 结构示意图：

```
IDR interval=30, B-frames=2:
  I B B P B B P B B P B B P B B P B B P B B P B B P B B P B B [IDR]

IDR interval=30, B-frames=0 (low-latency):
  I P P P P P P P P P P P P P P P P P P P P P P P P P P P P P [IDR]
```

对于低延迟流式传输，将 `num-b-frames=0` 和 `idrinterval=15` 设为该值或更低。

### 3.6 编码器属性参考

| 属性              | 类型    | 默认值 | 说明                                    |
|-----------------------|---------|---------|------------------------------------------------|
| `bitrate`             | uint    | 4000000 | 目标比特率，单位 bit/sec                     |
| `peak-bitrate`        | uint    | 0       | VBR 的峰值比特率（0 = 自动）                |
| `control-rate`        | enum    | 1 (CBR) | 码率控制模式（0=VBR，1=CBR，2=CQP）       |
| `preset-level`        | uint    | 1       | 0=UltraFast，1=Fast，2=Medium，3=Slow，4=HQ   |
| `idrinterval`         | uint    | 256     | IDR 帧之间的帧数                      |
| `iframeinterval`      | uint    | 30      | I 帧之间的帧数                        |
| `num-b-frames`        | uint    | 0       | P 帧之间的 B 帧数量            |
| `insert-sps-pps`      | bool    | false   | 每个 IDR 都插入 SPS/PPS（RTSP 需要）|
| `maxperf-enable`      | bool    | false   | 将 NVENC 时钟锁定到最高频率          |
| `EnableTwopassCBR`    | bool    | false   | 两遍 CBR，以获得更高画质                |
| `insert-vui`          | bool    | false   | 插入 Video Usability Information             |
| `profile`             | enum    | varies  | H264：0=Base，2=Main，4=High；H265：0=Main      |
| `quant-i-frames`      | uint    | 0       | I 帧的 QP（CQP 模式）                    |
| `quant-p-frames`      | uint    | 0       | P 帧的 QP（CQP 模式）                    |
| `quant-b-frames`      | uint    | 0       | B 帧的 QP（CQP 模式）                    |

```bash
# Inspect all encoder properties
gst-inspect-1.0 nvv4l2h265enc
gst-inspect-1.0 nvv4l2h264enc
```

### 3.7 画质调优预设

```bash
# Ultra-low-latency streaming (robotics, teleoperation)
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=1280,height=720,framerate=60/1' ! \
  nvv4l2h264enc preset-level=0 bitrate=3000000 control-rate=1 \
    idrinterval=15 num-b-frames=0 insert-sps-pps=true \
    maxperf-enable=true profile=0 ! \
  h264parse ! rtph264pay ! udpsink host=192.168.1.100 port=5000

# High-quality archival recording
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=3840,height=2160,framerate=30/1' ! \
  nvv4l2h265enc preset-level=4 bitrate=15000000 control-rate=1 \
    EnableTwopassCBR=true idrinterval=60 num-b-frames=2 ! \
  h265parse ! mp4mux ! filesink location=archive.mp4
```

---

## 4. NVJPEG -- 硬件 JPEG 引擎


<details>
<summary>English original</summary>

**3.4 Rate Control Modes**

| Mode | GStreamer Value | Description                                                |
|------|-----------------|------------------------------------------------------------|
| VBR  | control-rate=0  | Variable bitrate. Quality varies to hit average bitrate.   |
| CBR  | control-rate=1  | Constant bitrate. Best for fixed-bandwidth streaming.      |
| CQP  | control-rate=2  | Constant QP. Fixed quality, variable file size.            |

```bash
# CBR at 4 Mbps
gst-launch-1.0 videotestsrc num-buffers=300 ! \
  'video/x-raw,width=1920,height=1080,framerate=30/1' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 control-rate=1 ! \
  h265parse ! mp4mux ! filesink location=cbr_output.mp4

# VBR with peak 8 Mbps, average 4 Mbps
gst-launch-1.0 videotestsrc num-buffers=300 ! \
  'video/x-raw,width=1920,height=1080,framerate=30/1' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 peak-bitrate=8000000 control-rate=0 ! \
  h265parse ! mp4mux ! filesink location=vbr_output.mp4

# CQP with explicit QP values for I, P, B frames
gst-launch-1.0 videotestsrc num-buffers=300 ! \
  'video/x-raw,width=1920,height=1080,framerate=30/1' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc control-rate=2 quant-i-frames=20 quant-p-frames=23 quant-b-frames=25 ! \
  h265parse ! mp4mux ! filesink location=cqp_output.mp4
```

**3.5 B-Frame Support and GOP Structure**

NVENC on the Orin Nano supports B-frames for both H.264 and H.265:

```bash
# 2 B-frames, IDR every 30 frames
gst-launch-1.0 videotestsrc num-buffers=300 ! \
  'video/x-raw,width=1920,height=1080,framerate=30/1' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h264enc num-b-frames=2 idrinterval=30 bitrate=4000000 ! \
  h264parse ! mp4mux ! filesink location=bframe_output.mp4
```

GOP structure visualization:

```
IDR interval=30, B-frames=2:
  I B B P B B P B B P B B P B B P B B P B B P B B P B B P B B [IDR]

IDR interval=30, B-frames=0 (low-latency):
  I P P P P P P P P P P P P P P P P P P P P P P P P P P P P P [IDR]
```

For low-latency streaming, set `num-b-frames=0` and `idrinterval=15` or lower.

**3.6 Encoder Properties Reference**

| Property              | Type    | Default | Description                                    |
|-----------------------|---------|---------|------------------------------------------------|
| `bitrate`             | uint    | 4000000 | Target bitrate in bits/sec                     |
| `peak-bitrate`        | uint    | 0       | Peak bitrate for VBR (0 = auto)                |
| `control-rate`        | enum    | 1 (CBR) | Rate control mode (0=VBR, 1=CBR, 2=CQP)       |
| `preset-level`        | uint    | 1       | 0=UltraFast, 1=Fast, 2=Medium, 3=Slow, 4=HQ   |
| `idrinterval`         | uint    | 256     | Frames between IDR frames                      |
| `iframeinterval`      | uint    | 30      | Frames between I-frames                        |
| `num-b-frames`        | uint    | 0       | Number of B-frames between P-frames            |
| `insert-sps-pps`      | bool    | false   | Insert SPS/PPS with every IDR (needed for RTSP)|
| `maxperf-enable`      | bool    | false   | Lock NVENC clock to maximum frequency          |
| `EnableTwopassCBR`    | bool    | false   | Two-pass CBR for higher quality                |
| `insert-vui`          | bool    | false   | Insert Video Usability Information             |
| `profile`             | enum    | varies  | H264: 0=Base,2=Main,4=High; H265: 0=Main      |
| `quant-i-frames`      | uint    | 0       | QP for I-frames (CQP mode)                    |
| `quant-p-frames`      | uint    | 0       | QP for P-frames (CQP mode)                    |
| `quant-b-frames`      | uint    | 0       | QP for B-frames (CQP mode)                    |

```bash
# Inspect all encoder properties
gst-inspect-1.0 nvv4l2h265enc
gst-inspect-1.0 nvv4l2h264enc
```

**3.7 Quality Tuning Presets**

```bash
# Ultra-low-latency streaming (robotics, teleoperation)
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=1280,height=720,framerate=60/1' ! \
  nvv4l2h264enc preset-level=0 bitrate=3000000 control-rate=1 \
    idrinterval=15 num-b-frames=0 insert-sps-pps=true \
    maxperf-enable=true profile=0 ! \
  h264parse ! rtph264pay ! udpsink host=192.168.1.100 port=5000

# High-quality archival recording
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=3840,height=2160,framerate=30/1' ! \
  nvv4l2h265enc preset-level=4 bitrate=15000000 control-rate=1 \
    EnableTwopassCBR=true idrinterval=60 num-b-frames=2 ! \
  h265parse ! mp4mux ! filesink location=archive.mp4
```

---

**4. NVJPEG -- Hardware JPEG Engine**

</details>

### 4.1 JPEG 硬件模块

T234 集成了一个专用的 JPEG 编码器/解码器硬件模块（NVJPEG）。该引擎完全在硬件中完成 JPEG 压缩与解压缩，独立于 CPU 和 GPU。

### 4.2 规格

| 特性              | 规格                                |
|----------------------|----------------------------------------------|
| 最大编码分辨率 | 32768 x 32768（受内存限制）               |
| 最大解码分辨率 | 32768 x 32768（受内存限制）               |
| 编码吞吐    | ~350-500 Mpixels/s                           |
| 解码吞吐    | ~500 Mpixels/s                               |
| 色度子采样   | 4:2:0, 4:2:2, 4:4:4                         |
| 质量范围    | 1-100（JPEG 质量因子）                  |
| Profile     | 仅 Baseline JPEG（无 progressive/arithmetic）|

### 4.3 性能对比

| 指标              | NVJPEG（硬件）      | libjpeg-turbo（CPU）    |
|---------------------|------------------------|------------------------|
| 1080p 编码时间   | ~1.5 ms                | ~8-12 ms               |
| 4K 编码时间      | ~5 ms                  | ~35-50 ms              |
| CPU 利用率     | 接近零              | 1-2 个核的 100%      |
| 功耗影响    | 极小（< 0.5W）      | 显著（2-4W）     |

### 4.4 快照捕获

```bash
# Single JPEG frame from CSI camera
gst-launch-1.0 nvarguscamerasrc num-buffers=1 ! \
  'video/x-raw(memory:NVMM),width=4032,height=3040,framerate=30/1' ! \
  nvjpegenc quality=95 ! filesink location=snapshot.jpg

# Single JPEG from USB camera
gst-launch-1.0 v4l2src device=/dev/video2 num-buffers=1 ! \
  'video/x-raw,width=1920,height=1080' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvjpegenc quality=90 ! filesink location=snapshot.jpg
```

### 4.5 MJPEG 流传输

```bash
# MJPEG RTP stream from CSI camera
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
  nvjpegenc quality=85 ! rtpjpegpay ! \
  udpsink host=192.168.1.100 port=5000

# Receive and display on client machine
gst-launch-1.0 udpsrc port=5000 ! \
  'application/x-rtp,encoding-name=JPEG' ! rtpjpegdepay ! \
  jpegdec ! videoconvert ! autovideosink
```

### 4.6 用于推理的 JPEG 解码

```bash
# Decode directory of JPEG images for batch inference
gst-launch-1.0 multifilesrc location="images/%05d.jpg" index=0 caps="image/jpeg" ! \
  jpegparse ! nvv4l2decoder mjpeg=1 ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! fakesink

# In Python using jetson.utils
```

```python
import jetson.utils

# Hardware-accelerated JPEG decode
img = jetson.utils.loadImage("input.jpg", format="rgb8")
print(f"Decoded: {img.width}x{img.height}, format={img.format}")

# Hardware-accelerated JPEG encode
jetson.utils.saveImage("output.jpg", img, quality=90)
```

---

## 5. V4L2 编解码接口

### 5.1 设备拓扑

在 Orin Nano 上，V4L2 编解码设备以 Memory-to-Memory（M2M）设备的形式暴露：

```bash
$ ls -la /dev/video*
crw-rw---- 1 root video 81, 0 ... /dev/video0    # NVDEC (decoder)
crw-rw---- 1 root video 81, 1 ... /dev/video1    # NVENC (encoder)
crw-rw---- 1 root video 81, 2 ... /dev/video2    # NVJPEG (JPEG enc/dec)
crw-rw---- 1 root video 81, 3 ... /dev/video3    # USB camera (if connected)
```

注意：实际设备编号可能随 USB 设备和内核配置而变。使用 `v4l2-ctl --list-devices` 识别正确的设备。

```bash
$ v4l2-ctl --list-devices
NVIDIA Tegra Video Decoder (platform:15480000.nvdec):
        /dev/video0

NVIDIA Tegra Video Encoder (platform:15a80000.nvenc):
        /dev/video1

NVIDIA Tegra JPEG Encoder (platform:15380000.nvjpg):
        /dev/video2
```

### 5.2 V4L2 M2M 架构

```
    Userspace Application
         |           ^
    [OUTPUT queue]  [CAPTURE queue]
    (compressed)    (raw NV12)
         |           |
         v           |
    +-----------------------+
    |   V4L2 M2M Driver    |
    |   (nvdec / nvenc)     |
    +-----------------------+
         |           ^
         v           |
    +-----------------------+
    |   NVDEC / NVENC HW   |
    +-----------------------+
```

每个 M2M 设备有两个缓冲区队列：
- **OUTPUT**：应用发送待处理数据的位置（解码时为压缩数据，编码时为原始数据）。
- **CAPTURE**：应用接收已处理数据的位置（解码时为原始数据，编码时为压缩数据）。


<details>
<summary>English original</summary>

**4.1 JPEG Hardware Block**

The T234 integrates a dedicated JPEG encoder/decoder hardware block (NVJPEG). This engine
handles JPEG compression and decompression entirely in hardware, independently of the CPU
and GPU.

**4.2 Specifications**

| Feature              | Specification                                |
|----------------------|----------------------------------------------|
| Max encode resolution| 32768 x 32768 (memory-limited)               |
| Max decode resolution| 32768 x 32768 (memory-limited)               |
| Encode throughput    | ~350-500 Mpixels/s                           |
| Decode throughput    | ~500 Mpixels/s                               |
| Chroma subsampling   | 4:2:0, 4:2:2, 4:4:4                         |
| Quality range        | 1-100 (JPEG quality factor)                  |
| Profile              | Baseline JPEG only (no progressive/arithmetic)|

**4.3 Performance Comparison**

| Metric              | NVJPEG (hardware)      | libjpeg-turbo (CPU)    |
|---------------------|------------------------|------------------------|
| 1080p encode time   | ~1.5 ms                | ~8-12 ms               |
| 4K encode time      | ~5 ms                  | ~35-50 ms              |
| CPU utilization     | Near zero              | 100% of 1-2 cores      |
| Power impact        | Minimal (< 0.5W)      | Significant (2-4W)     |

**4.4 Snapshot Capture**

```bash
# Single JPEG frame from CSI camera
gst-launch-1.0 nvarguscamerasrc num-buffers=1 ! \
  'video/x-raw(memory:NVMM),width=4032,height=3040,framerate=30/1' ! \
  nvjpegenc quality=95 ! filesink location=snapshot.jpg

# Single JPEG from USB camera
gst-launch-1.0 v4l2src device=/dev/video2 num-buffers=1 ! \
  'video/x-raw,width=1920,height=1080' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! \
  nvjpegenc quality=90 ! filesink location=snapshot.jpg
```

**4.5 MJPEG Streaming**

```bash
# MJPEG RTP stream from CSI camera
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
  nvjpegenc quality=85 ! rtpjpegpay ! \
  udpsink host=192.168.1.100 port=5000

# Receive and display on client machine
gst-launch-1.0 udpsrc port=5000 ! \
  'application/x-rtp,encoding-name=JPEG' ! rtpjpegdepay ! \
  jpegdec ! videoconvert ! autovideosink
```

**4.6 JPEG Decode for Inference**

```bash
# Decode directory of JPEG images for batch inference
gst-launch-1.0 multifilesrc location="images/%05d.jpg" index=0 caps="image/jpeg" ! \
  jpegparse ! nvv4l2decoder mjpeg=1 ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! fakesink

# In Python using jetson.utils
```

```python
import jetson.utils

# Hardware-accelerated JPEG decode
img = jetson.utils.loadImage("input.jpg", format="rgb8")
print(f"Decoded: {img.width}x{img.height}, format={img.format}")

# Hardware-accelerated JPEG encode
jetson.utils.saveImage("output.jpg", img, quality=90)
```

---

**5. V4L2 Codec Interface**

**5.1 Device Topology**

On the Orin Nano, the V4L2 codec devices are exposed as Memory-to-Memory (M2M) devices:

```bash
$ ls -la /dev/video*
crw-rw---- 1 root video 81, 0 ... /dev/video0    # NVDEC (decoder)
crw-rw---- 1 root video 81, 1 ... /dev/video1    # NVENC (encoder)
crw-rw---- 1 root video 81, 2 ... /dev/video2    # NVJPEG (JPEG enc/dec)
crw-rw---- 1 root video 81, 3 ... /dev/video3    # USB camera (if connected)
```

Note: Actual device numbers may vary depending on USB devices and kernel configuration.
Use `v4l2-ctl --list-devices` to identify the correct device.

```bash
$ v4l2-ctl --list-devices
NVIDIA Tegra Video Decoder (platform:15480000.nvdec):
        /dev/video0

NVIDIA Tegra Video Encoder (platform:15a80000.nvenc):
        /dev/video1

NVIDIA Tegra JPEG Encoder (platform:15380000.nvjpg):
        /dev/video2
```

**5.2 V4L2 M2M Architecture**

```
    Userspace Application
         |           ^
    [OUTPUT queue]  [CAPTURE queue]
    (compressed)    (raw NV12)
         |           |
         v           |
    +-----------------------+
    |   V4L2 M2M Driver    |
    |   (nvdec / nvenc)     |
    +-----------------------+
         |           ^
         v           |
    +-----------------------+
    |   NVDEC / NVENC HW   |
    +-----------------------+
```

Each M2M device has two buffer queues:
- **OUTPUT**: Where the application sends data to be processed (compressed for decode, raw for encode).
- **CAPTURE**: Where the application receives processed data (raw for decode, compressed for encode).

</details>

### 5.3 Buffer 管理 —— MMAP 与 DMA-BUF

**MMAP buffer：** kernel 分配 buffer 并将其映射到用户空间。

```c
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

// Request MMAP buffers on the CAPTURE queue
struct v4l2_requestbuffers req = {0};
req.count  = 4;
req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
req.memory = V4L2_MEMORY_MMAP;
ioctl(fd, VIDIOC_REQBUFS, &req);

// Map each buffer
for (int i = 0; i < req.count; i++) {
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[1] = {0};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = i;
    buf.m.planes = planes;
    buf.length = 1;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);

    void *ptr = mmap(NULL, planes[0].length,
                     PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd, planes[0].m.mem_offset);
    // Store ptr for later use
}
```

**DMA-BUF buffer：** 硬件引擎之间的零拷贝共享。对于解码帧直接送入 CUDA 或 NVENC 的流水线，这是首选路径。

```c
// Request DMA-BUF export on CAPTURE queue
struct v4l2_requestbuffers req = {0};
req.count  = 4;
req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
req.memory = V4L2_MEMORY_DMABUF;
ioctl(fd, VIDIOC_REQBUFS, &req);

// Export buffer as DMA-BUF fd
struct v4l2_exportbuffer expbuf = {0};
expbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
expbuf.index = 0;
expbuf.plane = 0;
ioctl(fd, VIDIOC_EXPBUF, &expbuf);
int dma_fd = expbuf.fd;
// dma_fd can be imported by CUDA, NVENC, or other DMA-BUF consumers
```

### 5.4 使用 v4l2-ctl 的命令行编解码器用法

```bash
# Query decoder capabilities
v4l2-ctl -d /dev/video0 --info
v4l2-ctl -d /dev/video0 --list-formats-out   # Compressed input formats
v4l2-ctl -d /dev/video0 --list-formats       # Raw output formats

# Query encoder capabilities
v4l2-ctl -d /dev/video1 --info
v4l2-ctl -d /dev/video1 --list-formats-out   # Raw input formats
v4l2-ctl -d /dev/video1 --list-formats       # Compressed output formats

# Set encoder bitrate via V4L2 control
v4l2-ctl -d /dev/video1 --set-ctrl=video_bitrate=4000000
v4l2-ctl -d /dev/video1 --set-ctrl=video_bitrate_mode=1  # CBR

# List all controls for encoder
v4l2-ctl -d /dev/video1 --list-ctrls-menus
```

### 5.5 V4L2 Decode 示例（C）

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define DECODER_DEV "/dev/video0"

int main() {
    int fd = open(DECODER_DEV, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    // Set OUTPUT format (compressed H.265 input)
    struct v4l2_format fmt_out = {0};
    fmt_out.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt_out.fmt.pix_mp.width       = 1920;
    fmt_out.fmt.pix_mp.height      = 1080;
    fmt_out.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H265;
    fmt_out.fmt.pix_mp.num_planes  = 1;
    ioctl(fd, VIDIOC_S_FMT, &fmt_out);

    // Set CAPTURE format (raw NV12 output)
    struct v4l2_format fmt_cap = {0};
    fmt_cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt_cap.fmt.pix_mp.width       = 1920;
    fmt_cap.fmt.pix_mp.height      = 1080;
    fmt_cap.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
    fmt_cap.fmt.pix_mp.num_planes  = 2;
    ioctl(fd, VIDIOC_S_FMT, &fmt_cap);

    // Request buffers, streamon, queue/dequeue loop omitted for brevity
    // See NVIDIA L4T Multimedia API samples: /usr/src/jetson_multimedia_api/

    close(fd);
    return 0;
}
```

完整的 V4L2 编码/解码示例安装在：
JetPack 系统上的 `/usr/src/jetson_multimedia_api/samples/`。

### 5.6 NVIDIA Multimedia API（NvMedia 替代方案）

对于需要比 GStreamer 更精细的控制、但又比裸 V4L2 更有结构的应用，NVIDIA 提供 Jetson Multimedia API：

```bash
# Sample locations on JetPack 6.x
ls /usr/src/jetson_multimedia_api/samples/
# 01_video_encode/   -- V4L2 encode example
# 02_video_decode/   -- V4L2 decode example
# 10_camera_recording/ -- Camera + encode
# 12_camera_v4l2_cuda/  -- Camera + CUDA processing

# Build a sample
cd /usr/src/jetson_multimedia_api/samples/02_video_decode
make
./video_decode H265 --disable-rendering -o decoded.yuv input.h265
```

---

## 6. Jetson 上的 GStreamer —— NVIDIA 加速插件


<details>
<summary>English original</summary>

**5.3 Buffer Management -- MMAP vs DMA-BUF**

**MMAP buffers:** The kernel allocates buffers and maps them into userspace.

```c
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

// Request MMAP buffers on the CAPTURE queue
struct v4l2_requestbuffers req = {0};
req.count  = 4;
req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
req.memory = V4L2_MEMORY_MMAP;
ioctl(fd, VIDIOC_REQBUFS, &req);

// Map each buffer
for (int i = 0; i < req.count; i++) {
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[1] = {0};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = i;
    buf.m.planes = planes;
    buf.length = 1;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);

    void *ptr = mmap(NULL, planes[0].length,
                     PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd, planes[0].m.mem_offset);
    // Store ptr for later use
}
```

**DMA-BUF buffers:** Zero-copy sharing between hardware engines. This is the preferred
path for pipelines where decoded frames feed directly into CUDA or NVENC.

```c
// Request DMA-BUF export on CAPTURE queue
struct v4l2_requestbuffers req = {0};
req.count  = 4;
req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
req.memory = V4L2_MEMORY_DMABUF;
ioctl(fd, VIDIOC_REQBUFS, &req);

// Export buffer as DMA-BUF fd
struct v4l2_exportbuffer expbuf = {0};
expbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
expbuf.index = 0;
expbuf.plane = 0;
ioctl(fd, VIDIOC_EXPBUF, &expbuf);
int dma_fd = expbuf.fd;
// dma_fd can be imported by CUDA, NVENC, or other DMA-BUF consumers
```

**5.4 Command-Line Codec Usage with v4l2-ctl**

```bash
# Query decoder capabilities
v4l2-ctl -d /dev/video0 --info
v4l2-ctl -d /dev/video0 --list-formats-out   # Compressed input formats
v4l2-ctl -d /dev/video0 --list-formats       # Raw output formats

# Query encoder capabilities
v4l2-ctl -d /dev/video1 --info
v4l2-ctl -d /dev/video1 --list-formats-out   # Raw input formats
v4l2-ctl -d /dev/video1 --list-formats       # Compressed output formats

# Set encoder bitrate via V4L2 control
v4l2-ctl -d /dev/video1 --set-ctrl=video_bitrate=4000000
v4l2-ctl -d /dev/video1 --set-ctrl=video_bitrate_mode=1  # CBR

# List all controls for encoder
v4l2-ctl -d /dev/video1 --list-ctrls-menus
```

**5.5 V4L2 Decode Example (C)**

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define DECODER_DEV "/dev/video0"

int main() {
    int fd = open(DECODER_DEV, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    // Set OUTPUT format (compressed H.265 input)
    struct v4l2_format fmt_out = {0};
    fmt_out.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt_out.fmt.pix_mp.width       = 1920;
    fmt_out.fmt.pix_mp.height      = 1080;
    fmt_out.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H265;
    fmt_out.fmt.pix_mp.num_planes  = 1;
    ioctl(fd, VIDIOC_S_FMT, &fmt_out);

    // Set CAPTURE format (raw NV12 output)
    struct v4l2_format fmt_cap = {0};
    fmt_cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt_cap.fmt.pix_mp.width       = 1920;
    fmt_cap.fmt.pix_mp.height      = 1080;
    fmt_cap.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
    fmt_cap.fmt.pix_mp.num_planes  = 2;
    ioctl(fd, VIDIOC_S_FMT, &fmt_cap);

    // Request buffers, streamon, queue/dequeue loop omitted for brevity
    // See NVIDIA L4T Multimedia API samples: /usr/src/jetson_multimedia_api/

    close(fd);
    return 0;
}
```

The full V4L2 encode/decode examples are installed at:
`/usr/src/jetson_multimedia_api/samples/` on JetPack systems.

**5.6 NVIDIA Multimedia API (NvMedia Alternative)**

For applications that need finer control than GStreamer but more structure than raw V4L2,
NVIDIA provides the Jetson Multimedia API:

```bash
# Sample locations on JetPack 6.x
ls /usr/src/jetson_multimedia_api/samples/
# 01_video_encode/   -- V4L2 encode example
# 02_video_decode/   -- V4L2 decode example
# 10_camera_recording/ -- Camera + encode
# 12_camera_v4l2_cuda/  -- Camera + CUDA processing

# Build a sample
cd /usr/src/jetson_multimedia_api/samples/02_video_decode
make
./video_decode H265 --disable-rendering -o decoded.yuv input.h265
```

---

**6. GStreamer on Jetson -- NVIDIA Accelerated Plugins**

</details>

### 6.1 插件概览

NVIDIA 提供硬件加速的 GStreamer 插件，用于替代标准软件 element。这些插件通过 V4L2 驱动与硬件引擎通信，并将数据保存在 NVMM buffer 中，以实现零拷贝吞吐。

| NVIDIA 插件        | 替代对象             | 硬件引擎 | 功能                        |
|----------------------|----------------------|-----------------|---------------------------------|
| `nvv4l2decoder`      | `avdec_h264/h265`   | NVDEC           | 硬件视频解码            |
| `nvv4l2h264enc`      | `x264enc`           | NVENC           | 硬件 H.264 编码            |
| `nvv4l2h265enc`      | `x265enc`           | NVENC           | 硬件 H.265 编码            |
| `nvvideoconvert`     | `videoconvert`       | VIC / GPU       | 色彩/格式转换、缩放 |
| `nvjpegenc`          | `jpegenc`           | NVJPEG          | 硬件 JPEG 编码             |
| `nvjpegdec`          | `jpegdec`           | NVJPEG          | 硬件 JPEG 解码             |
| `nvarguscamerasrc`   | `v4l2src` (CSI)     | ISP             | 通过 libargus 的 CSI 摄像头          |
| `nvv4l2camerasrc`    | `v4l2src` (USB)     | --              | 输出 NVMM 的 USB 摄像头      |
| `nv3dsink`           | `xvimagesink`       | GPU (EGL)       | GPU 加速显示          |
| `nvegltransform`     | --                   | GPU (EGL)       | 用于显示的 EGL transform      |
| `nvstreammux`        | --                   | --              | 批处理多个 stream           |
| `nvdsosd`            | `textoverlay`       | GPU             | 边界框/文本叠加      |
| `nvmultistreamtiler` | --                   | GPU             | N 路 stream 网格合成          |

### 6.2 检查已安装的插件

```bash
# List all NVIDIA GStreamer plugins
gst-inspect-1.0 | grep -i nv

# Detailed info on a specific plugin
gst-inspect-1.0 nvv4l2decoder
gst-inspect-1.0 nvv4l2h265enc
gst-inspect-1.0 nvvideoconvert
gst-inspect-1.0 nvarguscamerasrc

# Verify plugin versions
gst-inspect-1.0 nvv4l2decoder | grep Version
```

### 6.3 内存协商 —— NVMM caps 约定

NVIDIA 插件在 GStreamer caps 中使用 `(memory:NVMM)` 来表示 buffer 位于 NVMM（NVIDIA Multimedia Memory）中 —— 即物理连续、支持 DMA 的内存，所有硬件引擎均可访问，无需 CPU 拷贝。

```
# NVMM buffer (zero-copy between hardware elements)
video/x-raw(memory:NVMM),format=NV12,width=1920,height=1080

# System memory buffer (CPU-accessible, requires copy to reach hardware)
video/x-raw,format=NV12,width=1920,height=1080
```

系统内存与 NVMM 之间的转换始终需要 `nvvideoconvert`：

```
  [v4l2src]                   [nvv4l2decoder]
  (system RAM)                (NVMM)
      |                            |
      v                            v
  nvvideoconvert              directly to
  (sys -> NVMM copy)         nvv4l2h265enc
      |                      (zero-copy)
      v
  nvv4l2h265enc
```

### 6.4 nvvideoconvert 能力

`nvvideoconvert`（在较旧的 JetPack 中也别名为 `nvvidconv`）可处理：

- **色彩空间转换：** NV12 转 RGBA、NV12 转 I420、RGBA 转 NV12 等。
- **分辨率缩放：** 输入到输出的分辨率任意改变。
- **内存域传输：** 系统内存到 NVMM，以及反向。
- **像素格式转换：** 在所有支持的格式之间转换。
- **裁剪与 letterbox：** 通过 caps 协商实现。

```bash
# Scale 4K to 1080p using VIC hardware
gst-launch-1.0 videotestsrc ! \
  'video/x-raw,width=3840,height=2160' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,format=NV12' ! \
  nv3dsink

# Convert NV12 to RGBA for CUDA processing
gst-launch-1.0 ... ! nvv4l2decoder ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! ...
```

### 6.5 用于显示的 nvegltransform

在屏幕上显示 NVMM buffer 时，`nvegltransform` 将 NVMM 转换为 EGLImage 以供渲染。在某些 JetPack 版本上，`nvvideoconvert` 与 `nveglglessink` 之间必须经过这一步：

```bash
gst-launch-1.0 videotestsrc ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=RGBA' ! \
  nvegltransform ! nveglglessink
```

使用 `nv3dsink` 时，无需这一中间步骤 —— `nv3dsink` 直接处理 NVMM。


<details>
<summary>English original</summary>

**6.1 Plugin Overview**

NVIDIA provides hardware-accelerated GStreamer plugins that replace standard software
elements. These plugins communicate with the hardware engines via the V4L2 driver and
keep data in NVMM buffers for zero-copy throughput.

| NVIDIA Plugin        | Replaces             | Hardware Engine | Function                        |
|----------------------|----------------------|-----------------|---------------------------------|
| `nvv4l2decoder`      | `avdec_h264/h265`   | NVDEC           | Hardware video decode            |
| `nvv4l2h264enc`      | `x264enc`           | NVENC           | Hardware H.264 encode            |
| `nvv4l2h265enc`      | `x265enc`           | NVENC           | Hardware H.265 encode            |
| `nvvideoconvert`     | `videoconvert`       | VIC / GPU       | Color/format conversion, scaling |
| `nvjpegenc`          | `jpegenc`           | NVJPEG          | Hardware JPEG encode             |
| `nvjpegdec`          | `jpegdec`           | NVJPEG          | Hardware JPEG decode             |
| `nvarguscamerasrc`   | `v4l2src` (CSI)     | ISP             | CSI camera via libargus          |
| `nvv4l2camerasrc`    | `v4l2src` (USB)     | --              | USB camera with NVMM output      |
| `nv3dsink`           | `xvimagesink`       | GPU (EGL)       | GPU-accelerated display          |
| `nvegltransform`     | --                   | GPU (EGL)       | EGL transform for display        |
| `nvstreammux`        | --                   | --              | Batch multiple streams           |
| `nvdsosd`            | `textoverlay`       | GPU             | Bounding box / text overlay      |
| `nvmultistreamtiler` | --                   | GPU             | N-stream grid composite          |

**6.2 Checking Installed Plugins**

```bash
# List all NVIDIA GStreamer plugins
gst-inspect-1.0 | grep -i nv

# Detailed info on a specific plugin
gst-inspect-1.0 nvv4l2decoder
gst-inspect-1.0 nvv4l2h265enc
gst-inspect-1.0 nvvideoconvert
gst-inspect-1.0 nvarguscamerasrc

# Verify plugin versions
gst-inspect-1.0 nvv4l2decoder | grep Version
```

**6.3 Memory Negotiation -- The NVMM Caps Convention**

NVIDIA plugins use `(memory:NVMM)` in GStreamer caps to indicate that buffers reside
in NVMM (NVIDIA Multimedia Memory) -- physically contiguous, DMA-capable memory
accessible by all hardware engines without CPU copies.

```
# NVMM buffer (zero-copy between hardware elements)
video/x-raw(memory:NVMM),format=NV12,width=1920,height=1080

# System memory buffer (CPU-accessible, requires copy to reach hardware)
video/x-raw,format=NV12,width=1920,height=1080
```

The transition between system memory and NVMM always requires `nvvideoconvert`:

```
  [v4l2src]                   [nvv4l2decoder]
  (system RAM)                (NVMM)
      |                            |
      v                            v
  nvvideoconvert              directly to
  (sys -> NVMM copy)         nvv4l2h265enc
      |                      (zero-copy)
      v
  nvv4l2h265enc
```

**6.4 nvvideoconvert Capabilities**

`nvvideoconvert` (also aliased as `nvvidconv` in older JetPack) handles:

- **Color space conversion:** NV12 to RGBA, NV12 to I420, RGBA to NV12, etc.
- **Resolution scaling:** Arbitrary input-to-output resolution change.
- **Memory domain transfer:** System memory to NVMM and vice versa.
- **Pixel format conversion:** Between all supported formats.
- **Cropping and letterboxing:** Via caps negotiation.

```bash
# Scale 4K to 1080p using VIC hardware
gst-launch-1.0 videotestsrc ! \
  'video/x-raw,width=3840,height=2160' ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,format=NV12' ! \
  nv3dsink

# Convert NV12 to RGBA for CUDA processing
gst-launch-1.0 ... ! nvv4l2decoder ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! ...
```

**6.5 nvegltransform for Display**

When displaying NVMM buffers on screen, `nvegltransform` converts NVMM to EGLImage
for rendering. On some JetPack versions, this is required between `nvvideoconvert`
and `nveglglessink`:

```bash
gst-launch-1.0 videotestsrc ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=RGBA' ! \
  nvegltransform ! nveglglessink
```

With `nv3dsink`, this intermediate step is not needed -- `nv3dsink` handles NVMM
directly.

</details>

### 6.6 插件流水线数据流

```
  nvarguscamerasrc                        v4l2src (USB)
  [NVMM output]                          [sys RAM output]
       |                                       |
       |                                  nvvideoconvert
       |                                  [sys -> NVMM copy]
       |                                       |
       +-------------------+-------------------+
                           |
                    nvv4l2h265enc / nvinfer / nvdsosd
                    [all accept NVMM input]
                           |
                    nvvideoconvert
                    [NVMM, format/scale changes via VIC]
                           |
                    nv3dsink / rtph265pay
                    [display or network output]
```

---

## 7. GStreamer 流水线模式

### 7.1 模式 1：摄像头到编码到流

CSI 摄像头经 ISP（图像信号处理器）采集，硬件编码为 H.265，并通过 RTP/UDP 传输：

```bash
# CSI camera -> H.265 encode -> RTP stream
gst-launch-1.0 nvarguscamerasrc sensor-id=0 ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1,format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 insert-sps-pps=true idrinterval=30 \
    maxperf-enable=true ! \
  h265parse ! rtph265pay config-interval=1 ! \
  udpsink host=192.168.1.100 port=5000 sync=false

# Receive on client
gst-launch-1.0 udpsrc port=5000 ! \
  'application/x-rtp,media=video,encoding-name=H265,payload=96' ! \
  rtph265depay ! h265parse ! avdec_h265 ! videoconvert ! autovideosink
```

USB 摄像头变体（注意 sys 到 NVMM 拷贝所需的 `nvvideoconvert`）：

```bash
# USB camera -> nvvideoconvert -> H.264 encode -> file
gst-launch-1.0 v4l2src device=/dev/video2 ! \
  'video/x-raw,width=1280,height=720,framerate=30/1' ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h264enc bitrate=2000000 insert-sps-pps=true ! \
  h264parse ! mp4mux ! filesink location=usb_recording.mp4
```

### 7.2 模式 2：文件到 decode 到显示

```bash
# MP4 file -> H.265 decode -> display
gst-launch-1.0 filesrc location=video.mp4 ! qtdemux name=d \
  d.video_0 ! h265parse ! nvv4l2decoder enable-max-performance=true ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! \
  nv3dsink sync=true

# With audio (demux both tracks)
gst-launch-1.0 filesrc location=video.mp4 ! qtdemux name=d \
  d.video_0 ! h265parse ! nvv4l2decoder ! nv3dsink \
  d.audio_0 ! aacparse ! avdec_aac ! audioconvert ! alsasink
```

### 7.3 模式 3：decode 到推理到编码

这是核心的边缘 AI 模式 —— decode 一路流，运行推理，叠加结果，
再重新编码输出：

```bash
# File -> decode -> inference (nvinfer) -> overlay -> encode -> file
gst-launch-1.0 filesrc location=traffic.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! \
  m.sink_0 nvstreammux name=m batch-size=1 width=1920 height=1080 ! \
  nvinfer config-file-path=pgie_config.txt ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! \
  nvdsosd ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 ! h265parse ! mp4mux ! \
  filesink location=output_with_detections.mp4
```

### 7.4 模式 4：带 tee 的多流

把单个源拆分为多个输出（显示 + 录制 + 推流）：

```bash
# Camera -> tee -> display + H.265 file + JPEG snapshots
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
  tee name=t \
  t. ! queue ! nv3dsink sync=false \
  t. ! queue ! nvv4l2h265enc bitrate=4000000 ! h265parse ! \
       mp4mux ! filesink location=recording.mp4 \
  t. ! queue ! videorate ! 'video/x-raw(memory:NVMM),framerate=1/5' ! \
       nvjpegenc quality=90 ! multifilesink location="snap_%05d.jpg"
```

### 7.5 模式 5：多源输入

多路 RTSP 摄像头输入单个 DeepStream 流水线：

```bash
gst-launch-1.0 \
  rtspsrc location=rtsp://cam1/live latency=100 ! rtph265depay ! h265parse ! \
    nvv4l2decoder ! m.sink_0 \
  rtspsrc location=rtsp://cam2/live latency=100 ! rtph265depay ! h265parse ! \
    nvv4l2decoder ! m.sink_1 \
  rtspsrc location=rtsp://cam3/live latency=100 ! rtph264depay ! h264parse ! \
    nvv4l2decoder ! m.sink_2 \
  nvstreammux name=m batch-size=3 width=1920 height=1080 \
    batched-push-timeout=40000 live-source=1 ! \
  nvinfer config-file-path=pgie_config.txt ! \
  nvmultistreamtiler rows=2 columns=2 width=1920 height=1080 ! \
  nvvideoconvert ! nvdsosd ! nv3dsink sync=false
```

### 7.6 模式 6：RTSP 输入到 RTSP 输出

RTSP 输入、RTSP 输出的端到端分析流水线：

```bash
# This requires the GStreamer RTSP server library
# Best done via Python (see Section 16) or DeepStream config file:

# deepstream_app config approach:
# [source0]
# enable=1
# type=4
# uri=rtsp://camera.local:554/live
# [sink0]
# enable=1
# type=4
# rtsp-port=8554
# udp-port=5400
# codec=1
# bitrate=4000000
```


<details>
<summary>English original</summary>

**6.6 Plugin Pipeline Data Flow**

```
  nvarguscamerasrc                        v4l2src (USB)
  [NVMM output]                          [sys RAM output]
       |                                       |
       |                                  nvvideoconvert
       |                                  [sys -> NVMM copy]
       |                                       |
       +-------------------+-------------------+
                           |
                    nvv4l2h265enc / nvinfer / nvdsosd
                    [all accept NVMM input]
                           |
                    nvvideoconvert
                    [NVMM, format/scale changes via VIC]
                           |
                    nv3dsink / rtph265pay
                    [display or network output]
```

---

**7. GStreamer Pipeline Patterns**

**7.1 Pattern 1: Camera to Encode to Stream**

CSI camera captured via ISP, hardware-encoded to H.265, and streamed over RTP/UDP:

```bash
# CSI camera -> H.265 encode -> RTP stream
gst-launch-1.0 nvarguscamerasrc sensor-id=0 ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1,format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 insert-sps-pps=true idrinterval=30 \
    maxperf-enable=true ! \
  h265parse ! rtph265pay config-interval=1 ! \
  udpsink host=192.168.1.100 port=5000 sync=false

# Receive on client
gst-launch-1.0 udpsrc port=5000 ! \
  'application/x-rtp,media=video,encoding-name=H265,payload=96' ! \
  rtph265depay ! h265parse ! avdec_h265 ! videoconvert ! autovideosink
```

USB camera variant (note the required `nvvideoconvert` for sys-to-NVMM copy):

```bash
# USB camera -> nvvideoconvert -> H.264 encode -> file
gst-launch-1.0 v4l2src device=/dev/video2 ! \
  'video/x-raw,width=1280,height=720,framerate=30/1' ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h264enc bitrate=2000000 insert-sps-pps=true ! \
  h264parse ! mp4mux ! filesink location=usb_recording.mp4
```

**7.2 Pattern 2: File to Decode to Display**

```bash
# MP4 file -> H.265 decode -> display
gst-launch-1.0 filesrc location=video.mp4 ! qtdemux name=d \
  d.video_0 ! h265parse ! nvv4l2decoder enable-max-performance=true ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! \
  nv3dsink sync=true

# With audio (demux both tracks)
gst-launch-1.0 filesrc location=video.mp4 ! qtdemux name=d \
  d.video_0 ! h265parse ! nvv4l2decoder ! nv3dsink \
  d.audio_0 ! aacparse ! avdec_aac ! audioconvert ! alsasink
```

**7.3 Pattern 3: Decode to Inference to Encode**

This is the core edge AI pattern -- decode a stream, run inference, overlay results,
and re-encode for output:

```bash
# File -> decode -> inference (nvinfer) -> overlay -> encode -> file
gst-launch-1.0 filesrc location=traffic.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! \
  m.sink_0 nvstreammux name=m batch-size=1 width=1920 height=1080 ! \
  nvinfer config-file-path=pgie_config.txt ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! \
  nvdsosd ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 ! h265parse ! mp4mux ! \
  filesink location=output_with_detections.mp4
```

**7.4 Pattern 4: Multi-Stream with tee**

Split a single source into multiple outputs (display + record + stream):

```bash
# Camera -> tee -> display + H.265 file + JPEG snapshots
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
  tee name=t \
  t. ! queue ! nv3dsink sync=false \
  t. ! queue ! nvv4l2h265enc bitrate=4000000 ! h265parse ! \
       mp4mux ! filesink location=recording.mp4 \
  t. ! queue ! videorate ! 'video/x-raw(memory:NVMM),framerate=1/5' ! \
       nvjpegenc quality=90 ! multifilesink location="snap_%05d.jpg"
```

**7.5 Pattern 5: Multi-Source Input**

Multiple RTSP cameras feeding into a single DeepStream pipeline:

```bash
gst-launch-1.0 \
  rtspsrc location=rtsp://cam1/live latency=100 ! rtph265depay ! h265parse ! \
    nvv4l2decoder ! m.sink_0 \
  rtspsrc location=rtsp://cam2/live latency=100 ! rtph265depay ! h265parse ! \
    nvv4l2decoder ! m.sink_1 \
  rtspsrc location=rtsp://cam3/live latency=100 ! rtph264depay ! h264parse ! \
    nvv4l2decoder ! m.sink_2 \
  nvstreammux name=m batch-size=3 width=1920 height=1080 \
    batched-push-timeout=40000 live-source=1 ! \
  nvinfer config-file-path=pgie_config.txt ! \
  nvmultistreamtiler rows=2 columns=2 width=1920 height=1080 ! \
  nvvideoconvert ! nvdsosd ! nv3dsink sync=false
```

**7.6 Pattern 6: RTSP Input to RTSP Output**

End-to-end analytics pipeline with RTSP in and RTSP out:

```bash
# This requires the GStreamer RTSP server library
# Best done via Python (see Section 16) or DeepStream config file:

# deepstream_app config approach:
# [source0]
# enable=1
# type=4
# uri=rtsp://camera.local:554/live
# [sink0]
# enable=1
# type=4
# rtsp-port=8554
# udp-port=5400
# codec=1
# bitrate=4000000
```

</details>

### 7.7 流水线调试辅助：DOT 图

```bash
# Generate pipeline graph for any gst-launch pipeline
export GST_DEBUG_DUMP_DOT_DIR=/tmp/gst-dots
mkdir -p /tmp/gst-dots

gst-launch-1.0 videotestsrc num-buffers=30 ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! nvv4l2h265enc ! fakesink

# Convert to PNG (requires graphviz)
dot -Tpng /tmp/gst-dots/*.dot -o pipeline_graph.png
```

---

## 8. 硬件加速转码

### 8.1 完整的 Decode-Scale-Encode 流水线

从 4K 的 H.264 转码为 1080p 的 H.265，全程在硬件中完成：

```bash
gst-launch-1.0 filesrc location=input_4k.mp4 ! qtdemux ! h264parse ! \
  nvv4l2decoder enable-max-performance=true ! \
  nvvideoconvert ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 preset-level=3 control-rate=1 \
    idrinterval=30 maxperf-enable=true ! \
  h265parse ! mp4mux ! filesink location=output_1080p.mp4
```

数据在硬件引擎间的流转：

```
  File (disk I/O)
       |
       v
  qtdemux (CPU: demux only, no pixel work)
       |
       v
  h264parse (CPU: parse NAL headers only)
       |
       v
  nvv4l2decoder [NVDEC hardware]
  Output: 3840x2160 NV12 in NVMM
       |
       v
  nvvideoconvert [VIC hardware]
  Scale: 3840x2160 -> 1920x1080
  Output: 1920x1080 NV12 in NVMM (zero-copy, same NVMM pool)
       |
       v
  nvv4l2h265enc [NVENC hardware]
  Input: 1920x1080 NV12 from NVMM (zero-copy)
  Output: H.265 bitstream
       |
       v
  h265parse + mp4mux (CPU: mux only)
       |
       v
  filesink (disk I/O)
```

该流水线中 CPU 只参与解复用、解析、复用和 I/O
控制。没有任何 CPU 核会触碰像素数据。

### 8.2 转码过程中的格式转换

```bash
# Transcode with format change: NV12 decode -> RGBA intermediate -> NV12 encode
# Useful when inserting CUDA processing between decode and encode
gst-launch-1.0 filesrc location=input.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! \
  identity name=cuda_tap ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 ! h265parse ! mp4mux ! \
  filesink location=output.mp4
```

### 8.3 分辨率缩放选项

`nvvideoconvert` 支持多种插值方法，由 `interpolation-method` 属性控制：

| 值 | 方法                | 质量   | 速度    |
|-------|-----------------------|-----------|----------|
| 0     | 最近邻      | 最低    | 最快  |
| 1     | 双线性              | 好      | 快     |
| 2     | 5 抽头滤波器          | 较好    | 中等 |
| 3     | 10 抽头滤波器         | 最好      | 最慢 |
| 4     | 智能（自适应）      | 最好      | 中等 |
| 5     | 最优                | 最高   | 最慢 |

```bash
# High-quality downscale with 5-tap filter
gst-launch-1.0 filesrc location=4k.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! \
  nvvideoconvert interpolation-method=2 ! \
  'video/x-raw(memory:NVMM),width=1280,height=720,format=NV12' ! \
  nvv4l2h264enc bitrate=2000000 ! h264parse ! mp4mux ! \
  filesink location=720p.mp4
```

### 8.4 批量转码脚本

```bash
#!/bin/bash
# transcode_directory.sh -- Transcode all MP4s in a directory
INPUT_DIR="$1"
OUTPUT_DIR="$2"
BITRATE="${3:-4000000}"

mkdir -p "$OUTPUT_DIR"

for f in "$INPUT_DIR"/*.mp4; do
    base=$(basename "$f" .mp4)
    echo "Transcoding: $f -> $OUTPUT_DIR/${base}_h265.mp4"

    gst-launch-1.0 -e \
      filesrc location="$f" ! qtdemux ! h264parse ! \
      nvv4l2decoder ! nvvideoconvert ! \
      'video/x-raw(memory:NVMM),width=1920,height=1080,format=NV12' ! \
      nvv4l2h265enc bitrate="$BITRATE" preset-level=3 ! \
      h265parse ! mp4mux ! \
      filesink location="$OUTPUT_DIR/${base}_h265.mp4"
done
echo "Done."
```

### 8.5 转码性能预期

| 转码场景                       | 预期速度（15W） |
|------------------------------------------|----------------------|
| 1080p30 H.264 -> 1080p30 H.265          | 实时（30 fps）   |
| 4K30 H.265 -> 1080p30 H.265             | 实时（30 fps）   |
| 4K30 H.265 -> 4K30 H.265（重编码）    | 实时（30 fps）   |
| 2x 1080p30 并行转码           | 两者均实时       |
| 4x 1080p30 并行转码           | 每个约 20 fps         |

超过 2 路的并行转码将受 NVENC 吞吐限制，因为只有
一个 NVENC 实例，必须分时复用。

---

## 9. DeepStream SDK 概览

### 9.1 DeepStream 是什么

NVIDIA DeepStream SDK 是一个构建在 GStreamer 之上的流式分析框架。它
提供一组专为 AI 视频分析打造的 GStreamer 插件：推理、跟踪、
多流批处理、分析、元数据处理和消息代理。DeepStream 管理
从输入到输出的整条流水线，数据全程保存在 NVMM 缓冲区中。


<details>
<summary>English original</summary>

**7.7 Pipeline Debugging Aid: DOT Graph**

```bash
# Generate pipeline graph for any gst-launch pipeline
export GST_DEBUG_DUMP_DOT_DIR=/tmp/gst-dots
mkdir -p /tmp/gst-dots

gst-launch-1.0 videotestsrc num-buffers=30 ! nvvideoconvert ! \
  'video/x-raw(memory:NVMM),format=NV12' ! nvv4l2h265enc ! fakesink

# Convert to PNG (requires graphviz)
dot -Tpng /tmp/gst-dots/*.dot -o pipeline_graph.png
```

---

**8. Hardware-Accelerated Transcoding**

**8.1 Full Decode-Scale-Encode Pipeline**

Transcoding from H.264 at 4K to H.265 at 1080p, entirely in hardware:

```bash
gst-launch-1.0 filesrc location=input_4k.mp4 ! qtdemux ! h264parse ! \
  nvv4l2decoder enable-max-performance=true ! \
  nvvideoconvert ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 preset-level=3 control-rate=1 \
    idrinterval=30 maxperf-enable=true ! \
  h265parse ! mp4mux ! filesink location=output_1080p.mp4
```

Data flow through hardware engines:

```
  File (disk I/O)
       |
       v
  qtdemux (CPU: demux only, no pixel work)
       |
       v
  h264parse (CPU: parse NAL headers only)
       |
       v
  nvv4l2decoder [NVDEC hardware]
  Output: 3840x2160 NV12 in NVMM
       |
       v
  nvvideoconvert [VIC hardware]
  Scale: 3840x2160 -> 1920x1080
  Output: 1920x1080 NV12 in NVMM (zero-copy, same NVMM pool)
       |
       v
  nvv4l2h265enc [NVENC hardware]
  Input: 1920x1080 NV12 from NVMM (zero-copy)
  Output: H.265 bitstream
       |
       v
  h265parse + mp4mux (CPU: mux only)
       |
       v
  filesink (disk I/O)
```

CPU involvement in this pipeline is limited to demuxing, parsing, muxing, and I/O
control. No CPU core touches pixel data.

**8.2 Format Conversion During Transcode**

```bash
# Transcode with format change: NV12 decode -> RGBA intermediate -> NV12 encode
# Useful when inserting CUDA processing between decode and encode
gst-launch-1.0 filesrc location=input.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=RGBA' ! \
  identity name=cuda_tap ! \
  nvvideoconvert ! 'video/x-raw(memory:NVMM),format=NV12' ! \
  nvv4l2h265enc bitrate=4000000 ! h265parse ! mp4mux ! \
  filesink location=output.mp4
```

**8.3 Resolution Scaling Options**

`nvvideoconvert` supports several interpolation methods controlled by the
`interpolation-method` property:

| Value | Method                | Quality   | Speed    |
|-------|-----------------------|-----------|----------|
| 0     | Nearest neighbor      | Lowest    | Fastest  |
| 1     | Bilinear              | Good      | Fast     |
| 2     | 5-tap filter          | Better    | Moderate |
| 3     | 10-tap filter         | Best      | Slowest  |
| 4     | Smart (adaptive)      | Best      | Moderate |
| 5     | Nicest                | Highest   | Slowest  |

```bash
# High-quality downscale with 5-tap filter
gst-launch-1.0 filesrc location=4k.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! \
  nvvideoconvert interpolation-method=2 ! \
  'video/x-raw(memory:NVMM),width=1280,height=720,format=NV12' ! \
  nvv4l2h264enc bitrate=2000000 ! h264parse ! mp4mux ! \
  filesink location=720p.mp4
```

**8.4 Batch Transcoding Script**

```bash
#!/bin/bash
# transcode_directory.sh -- Transcode all MP4s in a directory
INPUT_DIR="$1"
OUTPUT_DIR="$2"
BITRATE="${3:-4000000}"

mkdir -p "$OUTPUT_DIR"

for f in "$INPUT_DIR"/*.mp4; do
    base=$(basename "$f" .mp4)
    echo "Transcoding: $f -> $OUTPUT_DIR/${base}_h265.mp4"

    gst-launch-1.0 -e \
      filesrc location="$f" ! qtdemux ! h264parse ! \
      nvv4l2decoder ! nvvideoconvert ! \
      'video/x-raw(memory:NVMM),width=1920,height=1080,format=NV12' ! \
      nvv4l2h265enc bitrate="$BITRATE" preset-level=3 ! \
      h265parse ! mp4mux ! \
      filesink location="$OUTPUT_DIR/${base}_h265.mp4"
done
echo "Done."
```

**8.5 Transcode Performance Expectations**

| Transcode Scenario                       | Expected Speed (15W) |
|------------------------------------------|----------------------|
| 1080p30 H.264 -> 1080p30 H.265          | Real-time (30 fps)   |
| 4K30 H.265 -> 1080p30 H.265             | Real-time (30 fps)   |
| 4K30 H.265 -> 4K30 H.265 (re-encode)    | Real-time (30 fps)   |
| 2x 1080p30 parallel transcode           | Both real-time       |
| 4x 1080p30 parallel transcode           | ~20 fps each         |

Parallel transcoding beyond 2 streams will be limited by NVENC throughput, since only
one NVENC instance exists and it must time-slice.

---

**9. DeepStream SDK Overview**

**9.1 What is DeepStream**

NVIDIA DeepStream SDK is a streaming analytics framework built on GStreamer. It provides
a set of GStreamer plugins purpose-built for AI video analytics: inference, tracking,
multi-stream batching, analytics, metadata handling, and message brokering. DeepStream
manages the entire pipeline from input to output, keeping data in NVMM buffers throughout.

</details>

### 9.2 架构

```
+-------------------------------------------------------------------+
|                      DeepStream Application                       |
|  (deepstream-app, Python app, C app, or Triton-based)             |
+-------------------------------------------------------------------+
|                      DeepStream SDK Plugins                       |
|  +-----------+  +----------+  +----------+  +---------+           |
|  |nvstreammux|->| nvinfer  |->|nvtracker |->| nvdsosd |-> Sink    |
|  +-----------+  +----------+  +----------+  +---------+           |
|  | nvurisrc  |  |nvinfersvr|  |nvdsanalyt|  |nvmsgconv|           |
|  +-----------+  +----------+  +----------+  +---------+           |
+-------------------------------------------------------------------+
|                    GStreamer Framework (1.20)                      |
+-------------------------------------------------------------------+
|                  NVIDIA Accelerated Plugins                       |
|  (nvv4l2decoder, nvv4l2h265enc, nvvideoconvert, ...)              |
+-------------------------------------------------------------------+
|                  CUDA / TensorRT / cuDLA                          |
+-------------------------------------------------------------------+
|                  V4L2 Drivers, DMA-BUF, NVMM                     |
+-------------------------------------------------------------------+
|                  T234 Hardware (NVDEC, NVENC, GPU, VIC)            |
+-------------------------------------------------------------------+
```

### 9.3 元数据系统

DeepStream 在数据流经流水线时，为每个 GStreamer buffer 附加元数据。
元数据层级：

```
NvDsBatchMeta (per batch)
  |
  +-- NvDsFrameMeta[] (one per frame in batch)
        |
        +-- source_id, frame_num, buf_pts, ntp_timestamp
        |
        +-- NvDsObjectMeta[] (one per detected object)
        |     |
        |     +-- class_id, object_id (tracker), confidence
        |     +-- rect_params (bbox), text_params (label)
        |     +-- NvDsClassifierMeta[] (secondary classifier results)
        |
        +-- NvDsUserMeta[] (custom application metadata)
        |
        +-- NvDsDisplayMeta[] (OSD drawing commands)
```

该元数据与视频 buffer 一同流动，无需拷贝像素数据。下游
元素（OSD、消息转换器、应用探针）读取并修改元数据，以
实现分析逻辑。

### 9.4 批处理模型

`nvstreammux` 从 N 个输入流中各采集一帧，组成一个批：

```
Stream 0 frame -> +
Stream 1 frame -> +----> Batch (N frames) -> nvinfer (single TensorRT enqueue)
Stream 2 frame -> +
Stream 3 frame -> +

TensorRT processes all N frames in one batched inference call.
```

这比运行 N 次单独的推理调用高效得多。`batch-size`
属性对 `nvstreammux` 和 `nvinfer` 来说，都必须与流的数量匹配。

### 9.5 DeepStream 安装验证

```bash
# Check DeepStream version
deepstream-app --version-all

# Run the reference test application
deepstream-app -c /opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/\
source4_1080p_dec_infer-resnet_tracker_sgie_tiled_display_int8.txt

# List sample configs
ls /opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/

# List sample models
ls /opt/nvidia/deepstream/deepstream/samples/models/
```

---

## 10. DeepStream 流水线构建

### 10.1 关键元素及其作用

| 元素              | 作用                                                        |
|----------------------|-------------------------------------------------------------|
| `nvurisrcbin`        | 统一源：处理文件、RTSP、HTTP、CSI 输入        |
| `nvstreammux`        | 将 N 路流组成单个批 buffer              |
| `nvinfer`            | TensorRT 推理（检测、分类、分割）|
| `nvtracker`          | 多目标跟踪（IOU、NvDCF、DeepSORT）                |
| `nvdsanalytics`      | 越线、感兴趣区域计数、方向检测            |
| `nvdsosd`            | 屏上显示：边界框、文本、线条              |
| `nvmultistreamtiler` | 将 N 路流合成为单个分块输出             |
| `nvmsgconv`          | 将元数据转换为 JSON / protobuf payload                |
| `nvmsgbroker`        | 将 payload 发布到 Kafka、MQTT、AMQP、Azure IoT          |
| `nv3dsink`           | GPU 加速的显示 sink                                |
| `nvrtspoutsinkbin`   | RTSP 服务器输出 sink                                     |


<details>
<summary>English original</summary>

**9.2 Architecture**

```
+-------------------------------------------------------------------+
|                      DeepStream Application                       |
|  (deepstream-app, Python app, C app, or Triton-based)             |
+-------------------------------------------------------------------+
|                      DeepStream SDK Plugins                       |
|  +-----------+  +----------+  +----------+  +---------+           |
|  |nvstreammux|->| nvinfer  |->|nvtracker |->| nvdsosd |-> Sink    |
|  +-----------+  +----------+  +----------+  +---------+           |
|  | nvurisrc  |  |nvinfersvr|  |nvdsanalyt|  |nvmsgconv|           |
|  +-----------+  +----------+  +----------+  +---------+           |
+-------------------------------------------------------------------+
|                    GStreamer Framework (1.20)                      |
+-------------------------------------------------------------------+
|                  NVIDIA Accelerated Plugins                       |
|  (nvv4l2decoder, nvv4l2h265enc, nvvideoconvert, ...)              |
+-------------------------------------------------------------------+
|                  CUDA / TensorRT / cuDLA                          |
+-------------------------------------------------------------------+
|                  V4L2 Drivers, DMA-BUF, NVMM                     |
+-------------------------------------------------------------------+
|                  T234 Hardware (NVDEC, NVENC, GPU, VIC)            |
+-------------------------------------------------------------------+
```

**9.3 Metadata System**

DeepStream attaches metadata to every GStreamer buffer as it flows through the pipeline.
The metadata hierarchy:

```
NvDsBatchMeta (per batch)
  |
  +-- NvDsFrameMeta[] (one per frame in batch)
        |
        +-- source_id, frame_num, buf_pts, ntp_timestamp
        |
        +-- NvDsObjectMeta[] (one per detected object)
        |     |
        |     +-- class_id, object_id (tracker), confidence
        |     +-- rect_params (bbox), text_params (label)
        |     +-- NvDsClassifierMeta[] (secondary classifier results)
        |
        +-- NvDsUserMeta[] (custom application metadata)
        |
        +-- NvDsDisplayMeta[] (OSD drawing commands)
```

This metadata flows alongside the video buffers without copying pixel data. Downstream
elements (OSD, message converter, application probes) read and modify metadata to
implement analytics logic.

**9.4 Batch Processing Model**

`nvstreammux` collects one frame from each of N input streams and forms a batch:

```
Stream 0 frame -> +
Stream 1 frame -> +----> Batch (N frames) -> nvinfer (single TensorRT enqueue)
Stream 2 frame -> +
Stream 3 frame -> +

TensorRT processes all N frames in one batched inference call.
```

This is far more efficient than running N separate inference calls. The `batch-size`
property of both `nvstreammux` and `nvinfer` must match the number of streams.

**9.5 DeepStream Installation Verification**

```bash
# Check DeepStream version
deepstream-app --version-all

# Run the reference test application
deepstream-app -c /opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/\
source4_1080p_dec_infer-resnet_tracker_sgie_tiled_display_int8.txt

# List sample configs
ls /opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/

# List sample models
ls /opt/nvidia/deepstream/deepstream/samples/models/
```

---

**10. DeepStream Pipeline Construction**

**10.1 Key Elements and Their Roles**

| Element              | Role                                                        |
|----------------------|-------------------------------------------------------------|
| `nvurisrcbin`        | Unified source: handles file, RTSP, HTTP, CSI inputs        |
| `nvstreammux`        | Batches N streams into a single batched buffer              |
| `nvinfer`            | TensorRT inference (detection, classification, segmentation)|
| `nvtracker`          | Multi-object tracking (IOU, NvDCF, DeepSORT)                |
| `nvdsanalytics`      | Line crossing, ROI counting, direction detection            |
| `nvdsosd`            | On-screen display: bounding boxes, text, lines              |
| `nvmultistreamtiler` | Composites N streams into a single tiled output             |
| `nvmsgconv`          | Converts metadata to JSON / protobuf payload                |
| `nvmsgbroker`        | Publishes payloads to Kafka, MQTT, AMQP, Azure IoT          |
| `nv3dsink`           | GPU-accelerated display sink                                |
| `nvrtspoutsinkbin`   | RTSP server output sink                                     |

</details>

### 10.2 配置文件格式（deepstream-app）

`deepstream-app` 参考应用使用 INI 风格的配置文件：

```ini
[application]
enable-perf-measurement=1
perf-measurement-interval-sec=5

[tiled-display]
enable=1
rows=2
columns=2
width=1920
height=1080

[source0]
enable=1
type=3                          # 3=URI (file/RTSP), 4=RTSP, 5=CSI
uri=file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h265.mp4
num-sources=1
gpu-id=0
cudadec-memtype=0

[source1]
enable=1
type=4
uri=rtsp://192.168.1.10:554/live
latency=100
num-sources=1

[sink0]
enable=1
type=2                          # 2=EGL (display), 4=RTSP, 5=overlay
sync=0
gpu-id=0

[sink1]
enable=1
type=4                          # RTSP output
rtsp-port=8554
udp-port=5400
codec=1                         # 0=H.264, 1=H.265
bitrate=4000000
enc-type=0                      # 0=hardware encoder

[osd]
enable=1
text-size=15
border-width=2
border-color=0;1;0;1

[streammux]
batch-size=2
batched-push-timeout=40000
width=1920
height=1080
enable-padding=0
live-source=1                   # 1 for RTSP/camera sources

[primary-gie]
enable=1
gpu-id=0
gie-unique-id=1
nvbuf-memory-type=0
config-file=pgie_config.txt

[tracker]
enable=1
tracker-width=640
tracker-height=384
gpu-id=0
ll-lib-file=/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so
ll-config-file=tracker_config.yml
enable-batch-process=1

[secondary-gie0]
enable=1
gpu-id=0
gie-unique-id=2
operate-on-gie-id=1
operate-on-class-ids=0
config-file=sgie_config.txt
```

### 10.3 主推理配置（pgie_config.txt）

```ini
[property]
gpu-id=0
net-scale-factor=0.00392157    # 1/255 for 0-1 normalization
model-engine-file=yolov8n_b4_gpu0_fp16.engine
labelfile-path=labels.txt
batch-size=4                   # Must match streammux batch-size
network-mode=2                 # 0=FP32, 1=INT8, 2=FP16
num-detected-classes=80
interval=0                     # 0=infer every frame, N=skip N frames
gie-unique-id=1
process-mode=1                 # 1=primary (full-frame)
network-type=0                 # 0=detector, 1=classifier, 2=segmentation, 3=instance-seg
cluster-mode=2                 # 2=NMS
maintain-aspect-ratio=1
symmetric-padding=1
workspace-size=1024            # MB, for TensorRT engine building
parse-bbox-func-name=NvDsInferParseYoloV8
custom-lib-path=libnvds_infercustomparser_yolov8.so

[class-attrs-all]
pre-cluster-threshold=0.25
nms-iou-threshold=0.45
topk=300
```

### 10.4 Python Pipeline Construction

```python
#!/usr/bin/env python3
"""DeepStream pipeline: 2-stream detection with tiled display and RTSP output."""

import sys
import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib
import pyds

def osd_sink_pad_buffer_probe(pad, info, u_data):
    """Probe to extract detection metadata from each frame."""
    gst_buffer = info.get_buffer()
    if not gst_buffer:
        return Gst.PadProbeReturn.OK

    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(gst_buffer))
    l_frame = batch_meta.frame_meta_list

    while l_frame is not None:
        try:
            frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)
        except StopIteration:
            break

        num_objects = frame_meta.num_obj_meta
        l_obj = frame_meta.obj_meta_list

        while l_obj is not None:
            try:
                obj_meta = pyds.NvDsObjectMeta.cast(l_obj.data)
            except StopIteration:
                break

            print(f"Source {frame_meta.source_id} | "
                  f"Frame {frame_meta.frame_num} | "
                  f"Class {obj_meta.class_id} | "
                  f"Track ID {obj_meta.object_id} | "
                  f"Confidence {obj_meta.confidence:.2f} | "
                  f"BBox ({obj_meta.rect_params.left:.0f},"
                  f"{obj_meta.rect_params.top:.0f},"
                  f"{obj_meta.rect_params.width:.0f},"
                  f"{obj_meta.rect_params.height:.0f})")

            try:
                l_obj = l_obj.next
            except StopIteration:
                break

        try:
            l_frame = l_frame.next
        except StopIteration:
            break

    return Gst.PadProbeReturn.OK

def main():
    Gst.init(None)
    pipeline = Gst.Pipeline()

    # Sources
    sources = [
        "file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h265.mp4",
        "file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_720p.mp4",
    ]

    streammux = Gst.ElementFactory.make("nvstreammux", "mux")
    streammux.set_property("batch-size", len(sources))
    streammux.set_property("width", 1920)
    streammux.set_property("height", 1080)
    streammux.set_property("batched-push-timeout", 40000)
    pipeline.add(streammux)

    for i, uri in enumerate(sources):
        src = Gst.ElementFactory.make("nvurisrcbin", f"src-{i}")
        src.set_property("uri", uri)
        pipeline.add(src)
        padname = f"sink_{i}"
        sinkpad = streammux.request_pad_simple(padname)
        src.connect("pad-added",
                     lambda src, pad, sink=sinkpad: pad.link(sink))

    # Inference
    pgie = Gst.ElementFactory.make("nvinfer", "pgie")
    pgie.set_property("config-file-path", "pgie_config.txt")
    pipeline.add(pgie)

    # Tracker
    tracker = Gst.ElementFactory.make("nvtracker", "tracker")
    tracker.set_property("tracker-width", 640)
    tracker.set_property("tracker-height", 384)
    tracker.set_property("ll-lib-file",
        "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so")
    tracker.set_property("ll-config-file", "tracker_config.yml")
    pipeline.add(tracker)

    # Tiler
    tiler = Gst.ElementFactory.make("nvmultistreamtiler", "tiler")
    tiler.set_property("rows", 1)
    tiler.set_property("columns", 2)
    tiler.set_property("width", 1920)
    tiler.set_property("height", 540)
    pipeline.add(tiler)

    # OSD
    nvvidconv = Gst.ElementFactory.make("nvvideoconvert", "conv")
    osd = Gst.ElementFactory.make("nvdsosd", "osd")
    pipeline.add(nvvidconv)
    pipeline.add(osd)

    # Sink (display)
    sink = Gst.ElementFactory.make("nv3dsink", "sink")
    sink.set_property("sync", False)
    pipeline.add(sink)

    # Link: mux -> pgie -> tracker -> tiler -> conv -> osd -> sink
    streammux.link(pgie)
    pgie.link(tracker)
    tracker.link(tiler)
    tiler.link(nvvidconv)
    nvvidconv.link(osd)
    osd.link(sink)

    # Add probe on OSD sink pad
    osd_sinkpad = osd.get_static_pad("sink")
    osd_sinkpad.add_probe(Gst.PadProbeType.BUFFER, osd_sink_pad_buffer_probe, None)

    # Run
    pipeline.set_state(Gst.State.PLAYING)
    loop = GLib.MainLoop()

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect("message", lambda bus, msg: (
        loop.quit() if msg.type in (Gst.MessageType.EOS, Gst.MessageType.ERROR)
        else None
    ))

    try:
        loop.run()
    except KeyboardInterrupt:
        pass

    pipeline.set_state(Gst.State.NULL)

if __name__ == "__main__":
    main()
```

### 10.5 C 流水线构建

```c
#include <gst/gst.h>
#include <glib.h>
#include "gstnvdsmeta.h"
#include "nvdsmeta.h"

static GstPadProbeReturn osd_sink_pad_probe(GstPad *pad, GstPadProbeInfo *info,
                                             gpointer user_data) {
    GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER(info);
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    NvDsMetaList *l_frame = batch_meta->frame_meta_list;

    while (l_frame != NULL) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
        NvDsMetaList *l_obj = frame_meta->obj_meta_list;

        while (l_obj != NULL) {
            NvDsObjectMeta *obj = (NvDsObjectMeta *)(l_obj->data);
            g_print("Src %d Frame %d Class %d Track %lu Conf %.2f "
                    "BBox(%.0f,%.0f,%.0f,%.0f)\n",
                    frame_meta->source_id, frame_meta->frame_num,
                    obj->class_id, obj->object_id, obj->confidence,
                    obj->rect_params.left, obj->rect_params.top,
                    obj->rect_params.width, obj->rect_params.height);
            l_obj = l_obj->next;
        }
        l_frame = l_frame->next;
    }
    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GstElement *pipeline  = gst_pipeline_new("ds-pipeline");
    GstElement *source    = gst_element_factory_make("nvurisrcbin",  "src-0");
    GstElement *mux       = gst_element_factory_make("nvstreammux",  "mux");
    GstElement *pgie      = gst_element_factory_make("nvinfer",      "pgie");
    GstElement *tracker   = gst_element_factory_make("nvtracker",    "tracker");
    GstElement *tiler     = gst_element_factory_make("nvmultistreamtiler", "tiler");
    GstElement *conv      = gst_element_factory_make("nvvideoconvert","conv");
    GstElement *osd       = gst_element_factory_make("nvdsosd",      "osd");
    GstElement *sink      = gst_element_factory_make("nv3dsink",     "sink");

    g_object_set(G_OBJECT(source), "uri",
        "file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_1080p_h265.mp4",
        NULL);
    g_object_set(G_OBJECT(mux), "batch-size", 1, "width", 1920,
        "height", 1080, "batched-push-timeout", 40000, NULL);
    g_object_set(G_OBJECT(pgie), "config-file-path", "pgie_config.txt", NULL);
    g_object_set(G_OBJECT(tracker),
        "ll-lib-file",
        "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
        "ll-config-file", "tracker_config.yml",
        "tracker-width", 640, "tracker-height", 384, NULL);
    g_object_set(G_OBJECT(tiler), "rows", 1, "columns", 1,
        "width", 1920, "height", 1080, NULL);
    g_object_set(G_OBJECT(sink), "sync", FALSE, NULL);

    gst_bin_add_many(GST_BIN(pipeline), source, mux, pgie, tracker,
                     tiler, conv, osd, sink, NULL);

    GstPad *srcpad = gst_element_get_static_pad(source, "src");
    GstPad *sinkpad = gst_element_request_pad_simple(mux, "sink_0");
    gst_pad_link(srcpad, sinkpad);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    gst_element_link_many(mux, pgie, tracker, tiler, conv, osd, sink, NULL);

    /* Add probe */
    GstPad *osd_sink_pad = gst_element_get_static_pad(osd, "sink");
    gst_pad_add_probe(osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
                      osd_sink_pad_probe, NULL, NULL);
    gst_object_unref(osd_sink_pad);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
        GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);
    return 0;
}
```

编译：
```bash
gcc -o ds_pipeline ds_pipeline.c \
  $(pkg-config --cflags --libs gstreamer-1.0) \
  -I/opt/nvidia/deepstream/deepstream/sources/includes \
  -L/opt/nvidia/deepstream/deepstream/lib -lnvdsgst_meta -lnvds_meta
```

---

## 11. 使用自定义模型的 DeepStream

### 11.1 使用 TensorRT 引擎

DeepStream 的 `nvinfer` 插件可接受预构建的 TensorRT engine 文件，或在首次运行时
从 ONNX/Caffe/UFF 模型构建它们。生产环境中，务必交付预构建的 engine。

Engine 构建工作流：

```bash
# Build engine from ONNX (do this offline, not at runtime)
/usr/src/tensorrt/bin/trtexec \
  --onnx=yolov8n.onnx \
  --saveEngine=yolov8n_b4_gpu0_fp16.engine \
  --fp16 \
  --minShapes=images:1x3x640x640 \
  --optShapes=images:4x3x640x640 \
  --maxShapes=images:8x3x640x640 \
  --workspace=1024
```

预构建 engine 的配置：

```ini
[property]
model-engine-file=yolov8n_b4_gpu0_fp16.engine
# Do NOT set onnx-file, caffe-model, or uff-file when using pre-built engine
batch-size=4
network-mode=2                  # Must match engine precision
```

### 11.2 自定义输出解析器

使用非标准模型架构（YOLO、自定义检测器）时，必须提供一个自定义解析器库，
将原始张量输出转换为 DeepStream 的
`NvDsInferObjectDetectionInfo` 格式。

**解析器头文件**（`nvds_custom_parser.h`）：

```c
#include "nvdsinfer_custom_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

bool NvDsInferParseYoloV8(
    std::vector<NvDsInferLayerInfo> const &outputLayersInfo,
    NvDsInferNetworkInfo const &networkInfo,
    NvDsInferParseDetectionParams const &detectionParams,
    std::vector<NvDsInferObjectDetectionInfo> &objectList);

#ifdef __cplusplus
}
#endif
```

**解析器实现**（`nvds_custom_parser_yolov8.cpp`）：

```cpp
#include "nvds_custom_parser.h"
#include <cstring>
#include <algorithm>
#include <cmath>

static bool NvDsInferParseYoloV8(
    std::vector<NvDsInferLayerInfo> const &outputLayersInfo,
    NvDsInferNetworkInfo const &networkInfo,
    NvDsInferParseDetectionParams const &detectionParams,
    std::vector<NvDsInferObjectDetectionInfo> &objectList) {

    // YOLOv8 output: [batch, 84, 8400] -> transposed to [batch, 8400, 84]
    // 84 = 4 (bbox: cx, cy, w, h) + 80 (class scores)
    const int num_classes = 80;
    const int num_boxes = 8400;

    const float *output = (const float *)outputLayersInfo[0].buffer;
    float conf_threshold = detectionParams.perClassPreclusterThreshold[0];

    for (int i = 0; i < num_boxes; i++) {
        // Find best class
        int best_class = 0;
        float best_score = 0.0f;
        for (int c = 0; c < num_classes; c++) {
            float score = output[i * (4 + num_classes) + 4 + c];
            if (score > best_score) {
                best_score = score;
                best_class = c;
            }
        }

        if (best_score < conf_threshold) continue;

        // Extract bbox (cx, cy, w, h) normalized to network input size
        float cx = output[i * (4 + num_classes) + 0];
        float cy = output[i * (4 + num_classes) + 1];
        float w  = output[i * (4 + num_classes) + 2];
        float h  = output[i * (4 + num_classes) + 3];

        NvDsInferObjectDetectionInfo obj;
        obj.classId = best_class;
        obj.detectionConfidence = best_score;
        obj.left   = (cx - w / 2.0f);
        obj.top    = (cy - h / 2.0f);
        obj.width  = w;
        obj.height = h;
        objectList.push_back(obj);
    }

    return true;
}

extern "C"
bool NvDsInferParseCustomYoloV8(
    std::vector<NvDsInferLayerInfo> const &outputLayersInfo,
    NvDsInferNetworkInfo const &networkInfo,
    NvDsInferParseDetectionParams const &detectionParams,
    std::vector<NvDsInferObjectDetectionInfo> &objectList) {
    return NvDsInferParseYoloV8(outputLayersInfo, networkInfo,
                                 detectionParams, objectList);
}

CHECK_CUSTOM_PARSE_FUNC_PROTOTYPE(NvDsInferParseCustomYoloV8);
```

构建解析器：

```bash
g++ -shared -o libnvds_infercustomparser_yolov8.so \
  nvds_custom_parser_yolov8.cpp \
  -I/opt/nvidia/deepstream/deepstream/sources/includes \
  -I/usr/include/aarch64-linux-gnu \
  -fPIC -std=c++14
```

### 11.3 输出张量元数据

对于需要访问原始输出张量的模型（分割掩码、embedding、
姿态关键点），启用输出张量元数据：

```ini
[property]
output-tensor-meta=1           # Attach raw tensors to metadata
```

在 Python probe 中访问：

```python
def tensor_probe(pad, info, user_data):
    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(info.get_buffer()))
    l_frame = batch_meta.frame_meta_list

    while l_frame:
        frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)
        l_user = frame_meta.frame_user_meta_list

        while l_user:
            user_meta = pyds.NvDsUserMeta.cast(l_user.data)
            if user_meta.base_meta.meta_type == \
                    pyds.NvDsMetaType.NVDSINFER_TENSOR_OUTPUT_META:
                tensor_meta = pyds.NvDsInferTensorMeta.cast(
                    user_meta.user_meta_data)

                # Access output layers
                for i in range(tensor_meta.num_output_layers):
                    layer = pyds.get_nvds_LayerInfo(tensor_meta, i)
                    print(f"Layer {i}: name={layer.layerName}, "
                          f"dims={layer.inferDims.d[:layer.inferDims.numDims]}")

                    # Get pointer to tensor data
                    ptr = ctypes.cast(
                        pyds.get_ptr(layer.buffer),
                        ctypes.POINTER(ctypes.c_float))
                    # Copy to numpy for processing
                    import numpy as np
                    data = np.ctypeslib.as_array(
                        ptr, shape=layer.inferDims.d[:layer.inferDims.numDims])

            try:
                l_user = l_user.next
            except StopIteration:
                break
        try:
            l_frame = l_frame.next
        except StopIteration:
            break

    return Gst.PadProbeReturn.OK
```

### 11.4 二级分类器

对主检测器输出的裁剪检测框运行次级模型：

```ini
# sgie_vehicle_type_config.txt
[property]
gpu-id=0
net-scale-factor=1.0
model-engine-file=vehicle_type_classifier_b8_gpu0_int8.engine
labelfile-path=vehicle_type_labels.txt
batch-size=8
network-mode=1              # INT8
num-detected-classes=6
gie-unique-id=2
process-mode=2              # Secondary
operate-on-gie-id=1         # Operate on primary detector output
operate-on-class-ids=0      # Only classify class 0 (vehicle) detections
network-type=1              # Classifier
classifier-async-mode=1
input-object-min-width=64
input-object-min-height=64
```

### 11.5 分割模型

```ini
[property]
network-type=2              # Segmentation
segmentation-threshold=0.5
output-tensor-meta=1        # Required for accessing segmentation mask
num-detected-classes=21     # Number of segmentation classes
```

---

## 12. DeepStream 多路流处理

### 12.1 流合流架构

`nvstreammux` 是核心的批处理单元。它从每个源收集一帧，组装成一个 `NvBufSurface` 批，并作为附带 `NvDsBatchMeta` 的单个 GStreamer buffer 向下游推送。

```
  Source 0 (1080p) ---+
                       |
  Source 1 (720p)  ---+---> nvstreammux ---> Single batched buffer
                       |    (scales all to    containing N frames
  Source 2 (4K)    ---+     mux width/height)
                       |
  Source 3 (1080p) ---+

  nvstreammux properties:
    batch-size=4         # Maximum streams to batch
    width=1920           # All frames scaled to this width
    height=1080          # All frames scaled to this height
    batched-push-timeout=40000  # Timeout in microseconds
    live-source=1        # 1 for live RTSP/camera sources
```

### 12.2 每路流的资源分配

每路流消耗以下资源：

| 资源 | 每路流分配 | 8 路流合计 |
|---------------------|-------------------------------------------|--------------------|
| NVDEC 带宽     | 1920*1080*30 = 62M pixels/s               | 497M pixels/s      |
| Decode 缓冲      | 3 个 NV12 buffer * 每个 3 MB                | 72 MB              |
| Mux 缓冲          | 1 个 batch slot（1920*1080 NV12）             | 24 MB              |
| TensorRT workspace  | 在批内共享                       | 200-500 MB         |
| 跟踪器状态       | 每路流约 2 MB（150 个目标）            | 16 MB              |
| 每路流合计    | ~12-15 MB（不含模型权重）       | 96-120 MB          |

在 8 GB 系统总 RAM 下，规划如下：
- 模型权重：50-200 MB（取决于模型）
- 流水线缓冲：120-200 MB（8 路流）
- OS 与系统：1-2 GB
- CUDA 上下文：200-400 MB
- 留给应用：~5-6 GB

### 12.3 性能扩展

Orin Nano 8GB 15W 下的 benchmark（JetPack 6.0、DeepStream 7.0）：

| 模型                | 分辨率 | 流数 | FPS/流 | GPU % | NVDEC % | 备注          |
|----------------------|------------|---------|------------|-------|---------|----------------|
| PeopleNet (ResNet18) | 1080p      | 4       | 30         | 72    | 60      | 宽裕    |
| PeopleNet (ResNet18) | 1080p      | 8       | 15         | 95    | 85      | 受限于 GPU    |
| YOLOv8n INT8         | 1080p      | 4       | 30         | 55    | 60      | 宽裕    |
| YOLOv8n INT8         | 1080p      | 6       | 25         | 80    | 75      | 接近上限     |
| YOLOv8s INT8         | 1080p      | 4       | 20         | 90    | 45      | 受限于 GPU    |
| TrafficCamNet        | 720p       | 8       | 30         | 65    | 50      | 宽裕    |
| TrafficCamNet        | 720p       | 12      | 22         | 88    | 75      | 接近上限     |
| SSD MobileNetV2      | 720p       | 12      | 30         | 58    | 75      | 宽裕    |

经验法则：
- 4 路 1080p30 配轻量 INT8 检测器是宽裕的工作点。
- 1080p 超过 4 路时，降到 720p 或用 `nvinfer` `interval` 跳帧。
- 瓶颈通常是 GPU（推理），而不是 NVDEC（decode）。
- 跟踪器的计算量随被跟踪目标数量增长，而不只是随流数。

### 12.4 更高流数下的跳帧

```ini
# In pgie_config.txt, skip every other frame for inference
# Tracker interpolates positions on skipped frames
[property]
interval=1                    # 0=every frame, 1=skip 1 (infer every 2nd frame)
```

配合 `interval=1` 和良好的跟踪器，感知到的检测质量下降极小，而推理吞吐实际翻倍。


<details>
<summary>English original</summary>

**11.4 Secondary Classifiers**

Run a secondary model on cropped detections from the primary detector:

```ini
# sgie_vehicle_type_config.txt
[property]
gpu-id=0
net-scale-factor=1.0
model-engine-file=vehicle_type_classifier_b8_gpu0_int8.engine
labelfile-path=vehicle_type_labels.txt
batch-size=8
network-mode=1              # INT8
num-detected-classes=6
gie-unique-id=2
process-mode=2              # Secondary
operate-on-gie-id=1         # Operate on primary detector output
operate-on-class-ids=0      # Only classify class 0 (vehicle) detections
network-type=1              # Classifier
classifier-async-mode=1
input-object-min-width=64
input-object-min-height=64
```

**11.5 Segmentation Models**

```ini
[property]
network-type=2              # Segmentation
segmentation-threshold=0.5
output-tensor-meta=1        # Required for accessing segmentation mask
num-detected-classes=21     # Number of segmentation classes
```

---

**12. DeepStream Multi-Stream Processing**

**12.1 Stream Muxing Architecture**

`nvstreammux` is the central batching element. It collects one frame per source,
assembles them into a `NvBufSurface` batch, and pushes them downstream as a single
GStreamer buffer with `NvDsBatchMeta` attached.

```
  Source 0 (1080p) ---+
                       |
  Source 1 (720p)  ---+---> nvstreammux ---> Single batched buffer
                       |    (scales all to    containing N frames
  Source 2 (4K)    ---+     mux width/height)
                       |
  Source 3 (1080p) ---+

  nvstreammux properties:
    batch-size=4         # Maximum streams to batch
    width=1920           # All frames scaled to this width
    height=1080          # All frames scaled to this height
    batched-push-timeout=40000  # Timeout in microseconds
    live-source=1        # 1 for live RTSP/camera sources
```

**12.2 Resource Allocation Per Stream**

Each stream consumes the following resources:

| Resource            | Per-Stream Allocation                     | 8-Stream Total     |
|---------------------|-------------------------------------------|--------------------|
| NVDEC bandwidth     | 1920*1080*30 = 62M pixels/s               | 497M pixels/s      |
| Decode buffers      | 3 NV12 buffers * 3 MB each                | 72 MB              |
| Mux buffer          | 1 batch slot (1920*1080 NV12)             | 24 MB              |
| TensorRT workspace  | Shared across batch                       | 200-500 MB         |
| Tracker state       | ~2 MB per stream (150 targets)            | 16 MB              |
| Total per stream    | ~12-15 MB (excluding model weights)       | 96-120 MB          |

On 8 GB total system RAM, plan for:
- Model weights: 50-200 MB (depending on model)
- Pipeline buffers: 120-200 MB (8 streams)
- OS and system: 1-2 GB
- CUDA context: 200-400 MB
- Remaining for application: ~5-6 GB

**12.3 Performance Scaling**

Benchmarks on Orin Nano 8GB at 15W (JetPack 6.0, DeepStream 7.0):

| Model                | Resolution | Streams | FPS/stream | GPU % | NVDEC % | Notes          |
|----------------------|------------|---------|------------|-------|---------|----------------|
| PeopleNet (ResNet18) | 1080p      | 4       | 30         | 72    | 60      | Comfortable    |
| PeopleNet (ResNet18) | 1080p      | 8       | 15         | 95    | 85      | GPU-limited    |
| YOLOv8n INT8         | 1080p      | 4       | 30         | 55    | 60      | Comfortable    |
| YOLOv8n INT8         | 1080p      | 6       | 25         | 80    | 75      | Near limit     |
| YOLOv8s INT8         | 1080p      | 4       | 20         | 90    | 45      | GPU-limited    |
| TrafficCamNet        | 720p       | 8       | 30         | 65    | 50      | Comfortable    |
| TrafficCamNet        | 720p       | 12      | 22         | 88    | 75      | Near limit     |
| SSD MobileNetV2      | 720p       | 12      | 30         | 58    | 75      | Comfortable    |

Rules of thumb:
- 4x 1080p30 with a lightweight INT8 detector is the comfortable operating point.
- Beyond 4 streams at 1080p, drop to 720p or use `nvinfer` `interval` to skip frames.
- The bottleneck is usually the GPU (inference), not NVDEC (decode).
- Tracker compute scales with number of tracked objects, not just streams.

**12.4 Frame Skipping for Higher Stream Counts**

```ini
# In pgie_config.txt, skip every other frame for inference
# Tracker interpolates positions on skipped frames
[property]
interval=1                    # 0=every frame, 1=skip 1 (infer every 2nd frame)
```

With `interval=1` and a good tracker, perceived detection quality drops minimally
while effectively doubling inference throughput.

</details>

### 12.5 多源配置

```ini
# deepstream-app config for 4 RTSP cameras + 2 file sources
[source0]
enable=1
type=4
uri=rtsp://192.168.1.10:554/stream1
latency=200
num-sources=1
gpu-id=0
cudadec-memtype=0

[source1]
enable=1
type=4
uri=rtsp://192.168.1.11:554/stream1
latency=200
num-sources=1

[source2]
enable=1
type=4
uri=rtsp://192.168.1.12:554/stream1
latency=200
num-sources=1

[source3]
enable=1
type=4
uri=rtsp://192.168.1.13:554/stream1
latency=200
num-sources=1

[source4]
enable=1
type=3
uri=file:///data/videos/test1.mp4
num-sources=1

[source5]
enable=1
type=3
uri=file:///data/videos/test2.mp4
num-sources=1

[streammux]
batch-size=6
width=1920
height=1080
batched-push-timeout=40000
live-source=1
```

### 12.6 动态添加/移除流

DeepStream 支持在 runtime 添加和移除流，而无需重启流水线：

```python
# Add a new source at runtime
def add_source(pipeline, streammux, uri, source_id):
    src = Gst.ElementFactory.make("nvurisrcbin", f"src-{source_id}")
    src.set_property("uri", uri)
    pipeline.add(src)

    sinkpad = streammux.request_pad_simple(f"sink_{source_id}")
    src.connect("pad-added",
                 lambda src, pad, sink=sinkpad: pad.link(sink))
    src.sync_state_with_parent()

# Remove a source at runtime
def remove_source(pipeline, streammux, source_id):
    src = pipeline.get_by_name(f"src-{source_id}")
    sinkpad = streammux.get_static_pad(f"sink_{source_id}")

    src.set_state(Gst.State.NULL)
    pipeline.remove(src)
    streammux.release_request_pad(sinkpad)
```

---

## 13. DeepStream 分析与消息代理

### 13.1 nvdsanalytics 插件

`nvdsanalytics` 插件无需自定义代码即可提供内置分析原语：

- **越线检测：** 统计穿过指定线的目标。
- **基于 ROI 的分析：** 统计指定感兴趣区域内的目标。
- **方向检测：** 判定被跟踪目标的运动方向。
- **拥挤检测：** 当 ROI 内目标数量超过阈值时告警。

### 13.2 分析配置文件

```ini
# nvdsanalytics_config.txt

[property]
enable=1
config-width=1920
config-height=1080

# Line crossing: count vehicles crossing the intersection line
[line-crossing-stream-0]
enable=1
line-crossing-0=600;500;1400;500;    # x1;y1;x2;y2 -- horizontal line
lc-label-0=Intersection_LC
class-id=0                            # Apply to class 0 (vehicle)
extended=0
mode=balanced                         # strict, balanced, loose

# Second line crossing on same stream
line-crossing-1=960;200;960;900;     # Vertical line
lc-label-1=Center_LC
class-id=-1                           # -1 = all classes

# ROI-based counting
[roi-filtering-stream-0]
enable=1
roi-0=200;400;200;800;1000;800;1000;400;   # x1;y1;x2;y2;x3;y3;x4;y4 (polygon)
roi-label-0=Parking_Zone_A
inverse-roi=0                               # 0=count inside, 1=count outside
class-id=0

# Second ROI
roi-1=1100;400;1100;800;1800;800;1800;400;
roi-label-1=Parking_Zone_B
class-id=0

# Overcrowding alert
[overcrowding-stream-0]
enable=1
roi-0=200;400;200;800;1000;800;1000;400;
overcrowding-label-0=Zone_A_Overcrowd
object-threshold=10                          # Alert when > 10 objects
class-id=0

# Direction detection
[direction-detection-stream-0]
enable=1
direction-0=400;500;900;500;               # Reference line
direction-label-0=Northbound
class-id=0
```


<details>
<summary>English original</summary>

**12.5 Multi-Source Configuration**

```ini
# deepstream-app config for 4 RTSP cameras + 2 file sources
[source0]
enable=1
type=4
uri=rtsp://192.168.1.10:554/stream1
latency=200
num-sources=1
gpu-id=0
cudadec-memtype=0

[source1]
enable=1
type=4
uri=rtsp://192.168.1.11:554/stream1
latency=200
num-sources=1

[source2]
enable=1
type=4
uri=rtsp://192.168.1.12:554/stream1
latency=200
num-sources=1

[source3]
enable=1
type=4
uri=rtsp://192.168.1.13:554/stream1
latency=200
num-sources=1

[source4]
enable=1
type=3
uri=file:///data/videos/test1.mp4
num-sources=1

[source5]
enable=1
type=3
uri=file:///data/videos/test2.mp4
num-sources=1

[streammux]
batch-size=6
width=1920
height=1080
batched-push-timeout=40000
live-source=1
```

**12.6 Dynamic Stream Add/Remove**

DeepStream supports adding and removing streams at runtime without restarting the
pipeline:

```python
# Add a new source at runtime
def add_source(pipeline, streammux, uri, source_id):
    src = Gst.ElementFactory.make("nvurisrcbin", f"src-{source_id}")
    src.set_property("uri", uri)
    pipeline.add(src)

    sinkpad = streammux.request_pad_simple(f"sink_{source_id}")
    src.connect("pad-added",
                 lambda src, pad, sink=sinkpad: pad.link(sink))
    src.sync_state_with_parent()

# Remove a source at runtime
def remove_source(pipeline, streammux, source_id):
    src = pipeline.get_by_name(f"src-{source_id}")
    sinkpad = streammux.get_static_pad(f"sink_{source_id}")

    src.set_state(Gst.State.NULL)
    pipeline.remove(src)
    streammux.release_request_pad(sinkpad)
```

---

**13. DeepStream Analytics and Message Brokering**

**13.1 nvdsanalytics Plugin**

The `nvdsanalytics` plugin provides built-in analytics primitives without custom code:

- **Line crossing detection:** Count objects crossing a defined line.
- **ROI-based analytics:** Count objects within a defined region of interest.
- **Direction detection:** Determine movement direction of tracked objects.
- **Overcrowding detection:** Alert when ROI object count exceeds threshold.

**13.2 Analytics Configuration File**

```ini
# nvdsanalytics_config.txt

[property]
enable=1
config-width=1920
config-height=1080

# Line crossing: count vehicles crossing the intersection line
[line-crossing-stream-0]
enable=1
line-crossing-0=600;500;1400;500;    # x1;y1;x2;y2 -- horizontal line
lc-label-0=Intersection_LC
class-id=0                            # Apply to class 0 (vehicle)
extended=0
mode=balanced                         # strict, balanced, loose

# Second line crossing on same stream
line-crossing-1=960;200;960;900;     # Vertical line
lc-label-1=Center_LC
class-id=-1                           # -1 = all classes

# ROI-based counting
[roi-filtering-stream-0]
enable=1
roi-0=200;400;200;800;1000;800;1000;400;   # x1;y1;x2;y2;x3;y3;x4;y4 (polygon)
roi-label-0=Parking_Zone_A
inverse-roi=0                               # 0=count inside, 1=count outside
class-id=0

# Second ROI
roi-1=1100;400;1100;800;1800;800;1800;400;
roi-label-1=Parking_Zone_B
class-id=0

# Overcrowding alert
[overcrowding-stream-0]
enable=1
roi-0=200;400;200;800;1000;800;1000;400;
overcrowding-label-0=Zone_A_Overcrowd
object-threshold=10                          # Alert when > 10 objects
class-id=0

# Direction detection
[direction-detection-stream-0]
enable=1
direction-0=400;500;900;500;               # Reference line
direction-label-0=Northbound
class-id=0
```

</details>

### 13.3 访问 Analytics 元数据

```python
def analytics_probe(pad, info, user_data):
    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(info.get_buffer()))
    l_frame = batch_meta.frame_meta_list

    while l_frame:
        frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)
        l_user = frame_meta.frame_user_meta_list

        while l_user:
            user_meta = pyds.NvDsUserMeta.cast(l_user.data)
            if user_meta.base_meta.meta_type == \
                    pyds.NvDsMetaType.NVDS_ANALYTICS_FRAME_META:
                analytics_meta = pyds.NvDsAnalyticsFrameMeta.cast(
                    user_meta.user_meta_data)

                # Line crossing cumulative counts
                if analytics_meta.objLCCumCnt:
                    print(f"Line crossing counts: "
                          f"{pyds.nvds_analytics_get_lc_cum_cnt(analytics_meta)}")

                # Objects currently in each ROI
                if analytics_meta.objInROIcnt:
                    print(f"Objects in ROI: "
                          f"{pyds.nvds_analytics_get_roi_cnt(analytics_meta)}")

                # Overcrowding status
                if analytics_meta.ocStatus:
                    for label, status in analytics_meta.ocStatus.items():
                        if status:
                            print(f"OVERCROWDING ALERT: {label}")

            try:
                l_user = l_user.next
            except StopIteration:
                break

        # Per-object analytics: direction, LC status
        l_obj = frame_meta.obj_meta_list
        while l_obj:
            obj_meta = pyds.NvDsObjectMeta.cast(l_obj.data)
            l_user_obj = obj_meta.obj_user_meta_list

            while l_user_obj:
                user_meta = pyds.NvDsUserMeta.cast(l_user_obj.data)
                if user_meta.base_meta.meta_type == \
                        pyds.NvDsMetaType.NVDS_ANALYTICS_OBJ_META:
                    obj_analytics = pyds.NvDsAnalyticsObjInfo.cast(
                        user_meta.user_meta_data)

                    if obj_analytics.lcStatus:
                        print(f"Object {obj_meta.object_id} crossed line: "
                              f"{obj_analytics.lcStatus}")
                    if obj_analytics.dirStatus:
                        print(f"Object {obj_meta.object_id} direction: "
                              f"{obj_analytics.dirStatus}")
                    if obj_analytics.roiStatus:
                        print(f"Object {obj_meta.object_id} in ROI: "
                              f"{obj_analytics.roiStatus}")

                try:
                    l_user_obj = l_user_obj.next
                except StopIteration:
                    break

            try:
                l_obj = l_obj.next
            except StopIteration:
                break

        try:
            l_frame = l_frame.next
        except StopIteration:
            break

    return Gst.PadProbeReturn.OK
```

### 13.4 消息代理 —— 将元数据发送至云端

DeepStream 可以使用 `nvmsgconv`（元数据转 payload）和 `nvmsgbroker`（payload 转 broker），将检测元数据发布到外部消息 broker。

支持的协议：

| 协议 | 适配器库                | 使用场景                |
|----------|--------------------------------|-------------------------|
| Kafka    | `libnvds_kafka_proto.so`       | 高吞吐 IoT     |
| MQTT     | `libnvds_mqtt_proto.so`        | 轻量级 IoT         |
| AMQP     | `libnvds_amqp_proto.so`       | 企业消息传递    |
| Azure    | `libnvds_azure_proto.so`       | Azure IoT Hub           |
| Redis    | `libnvds_redis_proto.so`       | 内存数据存储    |


<details>
<summary>English original</summary>

**13.3 Accessing Analytics Metadata**

```python
def analytics_probe(pad, info, user_data):
    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(info.get_buffer()))
    l_frame = batch_meta.frame_meta_list

    while l_frame:
        frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)
        l_user = frame_meta.frame_user_meta_list

        while l_user:
            user_meta = pyds.NvDsUserMeta.cast(l_user.data)
            if user_meta.base_meta.meta_type == \
                    pyds.NvDsMetaType.NVDS_ANALYTICS_FRAME_META:
                analytics_meta = pyds.NvDsAnalyticsFrameMeta.cast(
                    user_meta.user_meta_data)

                # Line crossing cumulative counts
                if analytics_meta.objLCCumCnt:
                    print(f"Line crossing counts: "
                          f"{pyds.nvds_analytics_get_lc_cum_cnt(analytics_meta)}")

                # Objects currently in each ROI
                if analytics_meta.objInROIcnt:
                    print(f"Objects in ROI: "
                          f"{pyds.nvds_analytics_get_roi_cnt(analytics_meta)}")

                # Overcrowding status
                if analytics_meta.ocStatus:
                    for label, status in analytics_meta.ocStatus.items():
                        if status:
                            print(f"OVERCROWDING ALERT: {label}")

            try:
                l_user = l_user.next
            except StopIteration:
                break

        # Per-object analytics: direction, LC status
        l_obj = frame_meta.obj_meta_list
        while l_obj:
            obj_meta = pyds.NvDsObjectMeta.cast(l_obj.data)
            l_user_obj = obj_meta.obj_user_meta_list

            while l_user_obj:
                user_meta = pyds.NvDsUserMeta.cast(l_user_obj.data)
                if user_meta.base_meta.meta_type == \
                        pyds.NvDsMetaType.NVDS_ANALYTICS_OBJ_META:
                    obj_analytics = pyds.NvDsAnalyticsObjInfo.cast(
                        user_meta.user_meta_data)

                    if obj_analytics.lcStatus:
                        print(f"Object {obj_meta.object_id} crossed line: "
                              f"{obj_analytics.lcStatus}")
                    if obj_analytics.dirStatus:
                        print(f"Object {obj_meta.object_id} direction: "
                              f"{obj_analytics.dirStatus}")
                    if obj_analytics.roiStatus:
                        print(f"Object {obj_meta.object_id} in ROI: "
                              f"{obj_analytics.roiStatus}")

                try:
                    l_user_obj = l_user_obj.next
                except StopIteration:
                    break

            try:
                l_obj = l_obj.next
            except StopIteration:
                break

        try:
            l_frame = l_frame.next
        except StopIteration:
            break

    return Gst.PadProbeReturn.OK
```

**13.4 Message Brokering -- Sending Metadata to Cloud**

DeepStream can publish detection metadata to external message brokers using
`nvmsgconv` (metadata to payload) and `nvmsgbroker` (payload to broker).

Supported protocols:

| Protocol | Adapter Library                | Use Case                |
|----------|--------------------------------|-------------------------|
| Kafka    | `libnvds_kafka_proto.so`       | High-throughput IoT     |
| MQTT     | `libnvds_mqtt_proto.so`        | Lightweight IoT         |
| AMQP     | `libnvds_amqp_proto.so`       | Enterprise messaging    |
| Azure    | `libnvds_azure_proto.so`       | Azure IoT Hub           |
| Redis    | `libnvds_redis_proto.so`       | In-memory data store    |

</details>

### 13.5 Kafka 集成

流水线配置：

```ini
# deepstream-app config
[message-consumer0]
enable=0

[message-converter]
enable=1
msg-conv-config=msg_conv_config.txt
msg-conv-payload-type=0           # 0=DeepStream, 1=custom, 256=minimal

[message-broker]
enable=1
msg-broker-proto-lib=/opt/nvidia/deepstream/deepstream/lib/libnvds_kafka_proto.so
msg-broker-conn-str=kafka-broker.local;9092
topic=deepstream-detections
msg-broker-config=kafka_config.txt
```

Kafka 适配器配置（`kafka_config.txt`）：

```ini
[message-broker]
enable=1
broker-proto-lib=/opt/nvidia/deepstream/deepstream/lib/libnvds_kafka_proto.so
broker-conn-str=kafka-broker.local;9092
topic=deepstream-detections

[message-broker-config]
security.protocol=SASL_SSL
sasl.mechanism=PLAIN
sasl.username=api-key
sasl.password=api-secret
```

Python 流水线方式：

```python
# Add message converter and broker to pipeline
msgconv = Gst.ElementFactory.make("nvmsgconv", "msgconv")
msgconv.set_property("config", "msg_conv_config.txt")
msgconv.set_property("payload-type", 0)    # DeepStream schema

msgbroker = Gst.ElementFactory.make("nvmsgbroker", "msgbroker")
msgbroker.set_property("proto-lib",
    "/opt/nvidia/deepstream/deepstream/lib/libnvds_kafka_proto.so")
msgbroker.set_property("conn-str", "kafka-broker.local;9092;deepstream-topic")
msgbroker.set_property("topic", "detections")

# Use tee to branch: one path to display, one to message broker
tee = Gst.ElementFactory.make("tee", "tee")
queue_display = Gst.ElementFactory.make("queue", "q_display")
queue_msg = Gst.ElementFactory.make("queue", "q_msg")

pipeline.add(tee)
pipeline.add(queue_display)
pipeline.add(queue_msg)
pipeline.add(msgconv)
pipeline.add(msgbroker)

# Link: ... -> osd -> tee
#                       |-> queue_display -> sink
#                       |-> queue_msg -> msgconv -> msgbroker
osd.link(tee)
tee.link(queue_display)
queue_display.link(sink)
tee.link(queue_msg)
queue_msg.link(msgconv)
msgconv.link(msgbroker)
```

### 13.6 MQTT 集成

```python
broker = Gst.ElementFactory.make("nvmsgbroker", "broker")
broker.set_property("proto-lib",
    "/opt/nvidia/deepstream/deepstream/lib/libnvds_mqtt_proto.so")
broker.set_property("conn-str", "mqtt-broker.local;1883")
broker.set_property("topic", "edge/detections")
```

### 13.7 默认消息 schema

DeepStream 默认 schema（`payload-type=0`）产出的 JSON 形如：

```json
{
  "@timestamp": "2026-03-05T10:30:00.000Z",
  "sensorId": "sensor-0",
  "objects": [
    {
      "id": "42",
      "bbox": {"topleftx": 100, "toplefty": 200, "bottomrightx": 300, "bottomrighty": 400},
      "classId": 0,
      "label": "person",
      "confidence": 0.92,
      "trackingId": 42,
      "direction": "north",
      "lcStatus": ["Line_A"],
      "roiStatus": ["Zone_1"]
    }
  ],
  "analyticsModule": {
    "lineCrossing": {"Line_A": {"cumCount": {"in": 15, "out": 12}}},
    "roiCount": {"Zone_1": 3}
  }
}
```

### 13.8 自定义 payload 生成

针对应用专属 schema，使用 `payload-type=256`（最小实现）或实现
自定义 payload 生成器：

```python
def custom_payload_probe(pad, info, user_data):
    """Generate custom JSON and publish via external client."""
    import json
    import paho.mqtt.client as mqtt

    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(info.get_buffer()))
    l_frame = batch_meta.frame_meta_list

    events = []
    while l_frame:
        frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)
        l_obj = frame_meta.obj_meta_list

        while l_obj:
            obj = pyds.NvDsObjectMeta.cast(l_obj.data)
            events.append({
                "camera_id": frame_meta.source_id,
                "frame": frame_meta.frame_num,
                "timestamp": frame_meta.buf_pts / 1e9,
                "class": obj.class_id,
                "track_id": obj.object_id,
                "confidence": round(obj.confidence, 3),
                "bbox": [
                    round(obj.rect_params.left),
                    round(obj.rect_params.top),
                    round(obj.rect_params.width),
                    round(obj.rect_params.height)
                ]
            })
            try:
                l_obj = l_obj.next
            except StopIteration:
                break
        try:
            l_frame = l_frame.next
        except StopIteration:
            break

    if events:
        payload = json.dumps({"detections": events})
        # Publish via MQTT (or Kafka, HTTP, etc.)
        user_data["mqtt_client"].publish("edge/custom", payload)

    return Gst.PadProbeReturn.OK
```

---

## 14. 零拷贝视频流水线架构

### 14.1 NvBufSurface 抽象

`NvBufSurface` 是在 Jetson 上贯穿整个 DeepStream/GStreamer 流水线承载视频帧数据的核心数据结构。它把一个或多个视频帧封装为一个批，每帧存储在 NVMM（支持 DMA、物理连续的内存）中。

```c
typedef struct {
    uint32_t batchSize;              // Number of frames in batch
    uint32_t numFilled;              // Number of frames actually filled
    NvBufSurfaceMemType memType;     // NVBUF_MEM_DEFAULT, _CUDA_DEVICE, etc.
    uint64_t gpuId;
    NvBufSurfaceParams *surfaceList; // Array of per-frame parameters
} NvBufSurface;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    NvBufSurfaceColorFormat colorFormat;  // NVBUF_COLOR_FORMAT_NV12, _RGBA, etc.
    NvBufSurfacePlaneParams planeParams;
    int bufferDesc;                       // DMA-BUF fd
    void *dataPtr;                        // Mapped pointer (if mapped)
    uint32_t dataSize;
    void *mappedAddr;                     // CPU-mapped address (after Map)
} NvBufSurfaceParams;
```

### 14.2 零拷贝数据流

核心原则：视频像素数据从不跨越 CPU 与 GPU/硬件地址空间。相反，流水线各元素之间传递的是 `NvBufSurface` 句柄（指向 DMA-BUF 文件描述符的指针）。

```
  NVDEC (hardware)
     |
     | Outputs NvBufSurface with NV12 data in NVMM
     | (DMA-BUF fd in surfaceList[i].bufferDesc)
     |
     v
  nvvideoconvert (VIC hardware)
     |
     | Reads NV12 via DMA-BUF, writes RGBA via DMA-BUF
     | (new NvBufSurface, still in NVMM, no CPU copy)
     |
     v
  nvinfer (GPU - TensorRT)
     |
     | Maps NvBufSurface as CUDA resource via EGLImage/DMA-BUF import
     | cudaGraphicsEGLRegisterImage() or cuGraphicsResourceGetMappedEGLFrame()
     | Runs inference kernel, writes metadata (not pixel data)
     |
     v
  nvdsosd (GPU)
     |
     | Reads NvBufSurface as CUDA surface, draws bboxes/text
     | (modifies pixels in-place in NVMM, no copy)
     |
     v
  nvv4l2h265enc (NVENC hardware)
     |
     | Reads NV12/RGBA from NVMM via DMA-BUF
     | Outputs compressed H.265 bitstream to system memory
     |
     v
  rtph265pay + udpsink (CPU: packetization and network I/O only)
```

CPU `memcpy()` 在任何环节都不接触像素数据。CPU 只做这些工作：
- GStreamer buffer 元数据管理（轻量）
- NAL 单元解析（h264parse/h265parse —— 仅头部字节）
- 封装/解封装（容器格式，不涉及像素数据）
- 网络 I/O（RTP/RTSP 的 sendto/recvfrom）

### 14.3 零拷贝何时失效

当流水线中插入不感知 NVMM 的元素时，零拷贝就会失效：

```
  WRONG (breaks zero-copy):
  nvv4l2decoder ! videoconvert ! nvv4l2h265enc
                  ^^^^^^^^^^^^
                  Standard GStreamer element.
                  Forces: NVMM -> CPU copy -> CPU conversion -> CPU copy -> NVMM
                  Result: 10x slower, massive CPU usage

  RIGHT (maintains zero-copy):
  nvv4l2decoder ! nvvideoconvert ! nvv4l2h265enc
                  ^^^^^^^^^^^^^^^
                  NVIDIA element. Conversion happens in VIC/GPU via DMA-BUF.
                  Result: Zero CPU copies
```

常见的破坏零拷贝的错误：
- 用 `videoconvert` 而非 `nvvideoconvert`
- 用 `videoscale` 而非 `nvvideoconvert`（后者还带缩放）
- 用 `jpegenc` 而非 `nvjpegenc`
- 用 `avdec_h264` 而非 `nvv4l2decoder`
- 插入 `appsink` + `appsrc`，让帧经过 Python/NumPy
- 在 NVMM 元素之间使用带非 NVMM caps 的 `capsfilter`


<details>
<summary>English original</summary>

**14.1 The NvBufSurface Abstraction**

`NvBufSurface` is the central data structure that carries video frame data through
the entire DeepStream/GStreamer pipeline on Jetson. It encapsulates one or more
video frames as a batch, with each frame stored in NVMM (DMA-capable, physically
contiguous memory).

```c
typedef struct {
    uint32_t batchSize;              // Number of frames in batch
    uint32_t numFilled;              // Number of frames actually filled
    NvBufSurfaceMemType memType;     // NVBUF_MEM_DEFAULT, _CUDA_DEVICE, etc.
    uint64_t gpuId;
    NvBufSurfaceParams *surfaceList; // Array of per-frame parameters
} NvBufSurface;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    NvBufSurfaceColorFormat colorFormat;  // NVBUF_COLOR_FORMAT_NV12, _RGBA, etc.
    NvBufSurfacePlaneParams planeParams;
    int bufferDesc;                       // DMA-BUF fd
    void *dataPtr;                        // Mapped pointer (if mapped)
    uint32_t dataSize;
    void *mappedAddr;                     // CPU-mapped address (after Map)
} NvBufSurfaceParams;
```

**14.2 Zero-Copy Data Flow**

The key principle: video pixel data never crosses between CPU and GPU/hardware
address spaces. Instead, `NvBufSurface` handles (pointers to DMA-BUF file
descriptors) are passed between pipeline elements.

```
  NVDEC (hardware)
     |
     | Outputs NvBufSurface with NV12 data in NVMM
     | (DMA-BUF fd in surfaceList[i].bufferDesc)
     |
     v
  nvvideoconvert (VIC hardware)
     |
     | Reads NV12 via DMA-BUF, writes RGBA via DMA-BUF
     | (new NvBufSurface, still in NVMM, no CPU copy)
     |
     v
  nvinfer (GPU - TensorRT)
     |
     | Maps NvBufSurface as CUDA resource via EGLImage/DMA-BUF import
     | cudaGraphicsEGLRegisterImage() or cuGraphicsResourceGetMappedEGLFrame()
     | Runs inference kernel, writes metadata (not pixel data)
     |
     v
  nvdsosd (GPU)
     |
     | Reads NvBufSurface as CUDA surface, draws bboxes/text
     | (modifies pixels in-place in NVMM, no copy)
     |
     v
  nvv4l2h265enc (NVENC hardware)
     |
     | Reads NV12/RGBA from NVMM via DMA-BUF
     | Outputs compressed H.265 bitstream to system memory
     |
     v
  rtph265pay + udpsink (CPU: packetization and network I/O only)
```

At no point does CPU `memcpy()` touch pixel data. The only CPU work is:
- GStreamer buffer metadata management (lightweight)
- NAL unit parsing (h264parse/h265parse -- header bytes only)
- Muxing/demuxing (container format, not pixel data)
- Network I/O (sendto/recvfrom for RTP/RTSP)

**14.3 When Zero-Copy Breaks**

Zero-copy breaks when a non-NVMM-aware element is inserted into the pipeline:

```
  WRONG (breaks zero-copy):
  nvv4l2decoder ! videoconvert ! nvv4l2h265enc
                  ^^^^^^^^^^^^
                  Standard GStreamer element.
                  Forces: NVMM -> CPU copy -> CPU conversion -> CPU copy -> NVMM
                  Result: 10x slower, massive CPU usage

  RIGHT (maintains zero-copy):
  nvv4l2decoder ! nvvideoconvert ! nvv4l2h265enc
                  ^^^^^^^^^^^^^^^
                  NVIDIA element. Conversion happens in VIC/GPU via DMA-BUF.
                  Result: Zero CPU copies
```

Common zero-copy-breaking mistakes:
- Using `videoconvert` instead of `nvvideoconvert`
- Using `videoscale` instead of `nvvideoconvert` (which also scales)
- Using `jpegenc` instead of `nvjpegenc`
- Using `avdec_h264` instead of `nvv4l2decoder`
- Inserting `appsink` + `appsrc` to pass frames through Python/NumPy
- Using `capsfilter` with non-NVMM caps between NVMM elements

</details>

### 14.4 需要时由 CPU 访问

当 CPU 必须访问像素数据时（如自定义 OpenCV 处理），使用
`NvBufSurfaceMap` / `NvBufSurfaceUnMap`：

```c
#include "nvbufsurface.h"

// In a pad probe or custom element
NvBufSurface *surface = /* from GstBuffer map */;

// Map to CPU address space
NvBufSurfaceMap(surface, 0, -1, NVBUF_MAP_READ_WRITE);
NvBufSurfaceSyncForCpu(surface, 0, -1);  // Cache invalidate

// Access pixels
uint8_t *y_plane  = (uint8_t *)surface->surfaceList[0].mappedAddr.addr[0];
uint8_t *uv_plane = (uint8_t *)surface->surfaceList[0].mappedAddr.addr[1];
int pitch = surface->surfaceList[0].planeParams.pitch[0];

// Modify pixels...

NvBufSurfaceSyncForDevice(surface, 0, -1);  // Cache flush
NvBufSurfaceUnMap(surface, 0, -1);
```

在 Python 中配合 OpenCV：

```python
def frame_access_probe(pad, info, user_data):
    import numpy as np
    import cv2

    gst_buffer = info.get_buffer()
    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(gst_buffer))
    l_frame = batch_meta.frame_meta_list

    while l_frame:
        frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)

        # Get NvBufSurface
        n_frame = pyds.get_nvds_buf_surface(hash(gst_buffer),
                                             frame_meta.batch_id)
        # n_frame is now a numpy array (RGBA format)
        # This DOES involve a CPU copy -- use sparingly

        # OpenCV processing
        gray = cv2.cvtColor(n_frame, cv2.COLOR_RGBA2GRAY)
        edges = cv2.Canny(gray, 100, 200)

        try:
            l_frame = l_frame.next
        except StopIteration:
            break

    return Gst.PadProbeReturn.OK
```

### 14.5 DMA-BUF 跨引擎共享

来自 `NvBufSurface` 的 DMA-BUF 文件描述符可与其他子系统共享：

```c
// Import DMA-BUF into CUDA
NvBufSurface *surface = /* from pipeline */;
int dma_fd = surface->surfaceList[0].bufferDesc;

// Create EGLImage from DMA-BUF
EGLImageKHR egl_image = NvEGLImageFromFd(NULL, dma_fd);

// Register with CUDA
cudaGraphicsResource_t cuda_resource;
cudaGraphicsEGLRegisterImage(&cuda_resource, egl_image,
                              cudaGraphicsRegisterFlagsReadOnly);

// Map to get CUDA array
cudaGraphicsMapResources(1, &cuda_resource, 0);
cudaArray_t cuda_array;
cudaGraphicsSubResourceGetMappedArray(&cuda_array, cuda_resource, 0, 0);

// Use cuda_array in CUDA kernels...

cudaGraphicsUnmapResources(1, &cuda_resource, 0);
NvDestroyEGLImage(NULL, egl_image);
```

### 14.6 Buffer Pool 尺寸设定

每个 NVMM buffer 占用的内存与分辨率和格式成正比：

| 分辨率 | 格式 | 字节/帧 | 4 个 buffer | 8 个 buffer |
|------------|--------|-------------|-----------|-----------|
| 720p       | NV12   | 1.38 MB     | 5.5 MB    | 11.1 MB   |
| 1080p      | NV12   | 3.11 MB     | 12.4 MB   | 24.9 MB   |
| 1080p      | RGBA   | 8.29 MB     | 33.2 MB   | 66.4 MB   |
| 4K         | NV12   | 12.4 MB     | 49.8 MB   | 99.5 MB   |

Buffer pool 大小由 GStreamer 自动协商，但可以调优：

```bash
# Control decoder output buffer count
gst-launch-1.0 ... ! nvv4l2decoder num-extra-surfaces=2 ! ...
# Default is typically 4-8 buffers; reducing saves memory but risks starvation
```

---

## 15. 性能剖析

### 15.1 tegrastats —— 系统级监控

`tegrastats` 是监控硬件引擎利用率的首选工具：

```bash
sudo tegrastats --interval 500
```

示例输出：

```
RAM 3200/7620MB (lfb 1234x4MB) SWAP 0/3810MB (cached 0MB)
CPU [40%@1510,35%@1510,22%@1510,18%@1510,10%@1510,8%@1510]
EMC_FREQ 3% GR3D_FREQ 72% NVDEC 55% NVENC 40% NVJPG 0%
VIC_FREQ 0% APE 150 MTS fg 0% bg 0%
TEMP CPU@45C GPU@43C SOC@44.5C tj@45C
VDD_IN 8500mW VDD_CPU_GPU_CV 3200mW VDD_SOC 2100mW
```

关键字段：
- `GR3D_FREQ`：GPU 利用率（推理瓶颈指标）
- `NVDEC`：解码器利用率
- `NVENC`：编码器利用率
- `RAM`：总内存占用
- `TEMP tj`：结温（约 85C 时降频）
- `VDD_IN`：整板功耗

### 15.2 DeepStream 性能测量

启用内置 FPS 上报：

```ini
[application]
enable-perf-measurement=1
perf-measurement-interval-sec=5
```

输出：

```
**PERF:  FPS 0 (Avg)    FPS 1 (Avg)    FPS 2 (Avg)    FPS 3 (Avg)
**PERF:  30.02 (30.01)  30.01 (30.00)  29.98 (29.99)  30.00 (30.00)
```

### 15.3 GStreamer 延迟链路追踪

```bash
# Enable latency tracer (measures per-element latency)
GST_TRACERS="latency(flags=element)" \
GST_DEBUG="GST_TRACER:7" \
gst-launch-1.0 filesrc location=test.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! nvvideoconvert ! nv3dsink 2>&1 | grep -i latency

# Output shows per-element processing time in nanoseconds
# element-latency, element=nvv4l2decoder0, time=3456789
```


<details>
<summary>English original</summary>

**14.4 CPU Access When Needed**

When the CPU must access pixel data (e.g., custom OpenCV processing), use
`NvBufSurfaceMap` / `NvBufSurfaceUnMap`:

```c
#include "nvbufsurface.h"

// In a pad probe or custom element
NvBufSurface *surface = /* from GstBuffer map */;

// Map to CPU address space
NvBufSurfaceMap(surface, 0, -1, NVBUF_MAP_READ_WRITE);
NvBufSurfaceSyncForCpu(surface, 0, -1);  // Cache invalidate

// Access pixels
uint8_t *y_plane  = (uint8_t *)surface->surfaceList[0].mappedAddr.addr[0];
uint8_t *uv_plane = (uint8_t *)surface->surfaceList[0].mappedAddr.addr[1];
int pitch = surface->surfaceList[0].planeParams.pitch[0];

// Modify pixels...

NvBufSurfaceSyncForDevice(surface, 0, -1);  // Cache flush
NvBufSurfaceUnMap(surface, 0, -1);
```

In Python with OpenCV:

```python
def frame_access_probe(pad, info, user_data):
    import numpy as np
    import cv2

    gst_buffer = info.get_buffer()
    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(gst_buffer))
    l_frame = batch_meta.frame_meta_list

    while l_frame:
        frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)

        # Get NvBufSurface
        n_frame = pyds.get_nvds_buf_surface(hash(gst_buffer),
                                             frame_meta.batch_id)
        # n_frame is now a numpy array (RGBA format)
        # This DOES involve a CPU copy -- use sparingly

        # OpenCV processing
        gray = cv2.cvtColor(n_frame, cv2.COLOR_RGBA2GRAY)
        edges = cv2.Canny(gray, 100, 200)

        try:
            l_frame = l_frame.next
        except StopIteration:
            break

    return Gst.PadProbeReturn.OK
```

**14.5 DMA-BUF Inter-Engine Sharing**

The DMA-BUF file descriptor from `NvBufSurface` can be shared with other subsystems:

```c
// Import DMA-BUF into CUDA
NvBufSurface *surface = /* from pipeline */;
int dma_fd = surface->surfaceList[0].bufferDesc;

// Create EGLImage from DMA-BUF
EGLImageKHR egl_image = NvEGLImageFromFd(NULL, dma_fd);

// Register with CUDA
cudaGraphicsResource_t cuda_resource;
cudaGraphicsEGLRegisterImage(&cuda_resource, egl_image,
                              cudaGraphicsRegisterFlagsReadOnly);

// Map to get CUDA array
cudaGraphicsMapResources(1, &cuda_resource, 0);
cudaArray_t cuda_array;
cudaGraphicsSubResourceGetMappedArray(&cuda_array, cuda_resource, 0, 0);

// Use cuda_array in CUDA kernels...

cudaGraphicsUnmapResources(1, &cuda_resource, 0);
NvDestroyEGLImage(NULL, egl_image);
```

**14.6 Buffer Pool Sizing**

Each NVMM buffer consumes memory proportional to resolution and format:

| Resolution | Format | Bytes/frame | 4 buffers | 8 buffers |
|------------|--------|-------------|-----------|-----------|
| 720p       | NV12   | 1.38 MB     | 5.5 MB    | 11.1 MB   |
| 1080p      | NV12   | 3.11 MB     | 12.4 MB   | 24.9 MB   |
| 1080p      | RGBA   | 8.29 MB     | 33.2 MB   | 66.4 MB   |
| 4K         | NV12   | 12.4 MB     | 49.8 MB   | 99.5 MB   |

Buffer pool sizes are negotiated automatically by GStreamer, but can be tuned:

```bash
# Control decoder output buffer count
gst-launch-1.0 ... ! nvv4l2decoder num-extra-surfaces=2 ! ...
# Default is typically 4-8 buffers; reducing saves memory but risks starvation
```

---

**15. Performance Profiling**

**15.1 tegrastats -- System-Level Monitoring**

`tegrastats` is the primary tool for monitoring hardware engine utilization:

```bash
sudo tegrastats --interval 500
```

Sample output:

```
RAM 3200/7620MB (lfb 1234x4MB) SWAP 0/3810MB (cached 0MB)
CPU [40%@1510,35%@1510,22%@1510,18%@1510,10%@1510,8%@1510]
EMC_FREQ 3% GR3D_FREQ 72% NVDEC 55% NVENC 40% NVJPG 0%
VIC_FREQ 0% APE 150 MTS fg 0% bg 0%
TEMP CPU@45C GPU@43C SOC@44.5C tj@45C
VDD_IN 8500mW VDD_CPU_GPU_CV 3200mW VDD_SOC 2100mW
```

Key fields:
- `GR3D_FREQ`: GPU utilization (inference bottleneck indicator)
- `NVDEC`: Decoder utilization
- `NVENC`: Encoder utilization
- `RAM`: Total memory consumption
- `TEMP tj`: Junction temperature (throttle at ~85C)
- `VDD_IN`: Total board power

**15.2 DeepStream Performance Measurement**

Enable built-in FPS reporting:

```ini
[application]
enable-perf-measurement=1
perf-measurement-interval-sec=5
```

Output:

```
**PERF:  FPS 0 (Avg)    FPS 1 (Avg)    FPS 2 (Avg)    FPS 3 (Avg)
**PERF:  30.02 (30.01)  30.01 (30.00)  29.98 (29.99)  30.00 (30.00)
```

**15.3 GStreamer Latency Tracing**

```bash
# Enable latency tracer (measures per-element latency)
GST_TRACERS="latency(flags=element)" \
GST_DEBUG="GST_TRACER:7" \
gst-launch-1.0 filesrc location=test.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! nvvideoconvert ! nv3dsink 2>&1 | grep -i latency

# Output shows per-element processing time in nanoseconds
# element-latency, element=nvv4l2decoder0, time=3456789
```

</details>

### 15.4 流水线 DOT 图

可视化协商后的流水线，包括 caps 与缓冲池：

```bash
export GST_DEBUG_DUMP_DOT_DIR=/tmp/gst-dots
mkdir -p /tmp/gst-dots

# Run pipeline (DOT files generated at state changes)
gst-launch-1.0 ... your pipeline ...

# Convert to PNG
sudo apt-get install graphviz
dot -Tpng /tmp/gst-dots/0.00.00.*PLAYING*.dot -o pipeline.png

# The DOT graph shows:
# - Element names and types
# - Negotiated caps on each link
# - Buffer pool configurations
# - Queue levels
```

### 15.5 NVIDIA Nsight Systems 性能剖析

用于深入分析 CUDA kernel、NVDEC/NVENC 时序与内存传输：

```bash
# Profile a DeepStream application
nsys profile \
  --trace=cuda,nvtx,nvmedia,osrt \
  --output=ds_profile \
  --duration=30 \
  deepstream-app -c config.txt

# Profile a GStreamer pipeline
nsys profile \
  --trace=cuda,nvtx,osrt \
  --output=gst_profile \
  gst-launch-1.0 ... your pipeline ...

# View results
nsys-ui ds_profile.nsys-rep
```

在 Nsight Systems GUI 中，重点关注：
- **CUDA kernel timeline：** 识别推理 kernel 的耗时与间隙。
- **NVDEC 行：** 显示 decode 操作与空闲时间。
- **NVENC 行：** 显示 encode 操作。
- **内存传输：** 出现任何意外的 `cudaMemcpy` 都表明零拷贝已失效。
- **CPU 线程活动：** CPU 占用高说明路径中存在软件处理。

### 15.6 用 GST_DEBUG 做插件级调试

```bash
# Debug levels: 0=none, 1=ERROR, 2=WARNING, 3=FIXME, 4=INFO, 5=DEBUG, 6=LOG, 7=TRACE

# Debug specific plugins
GST_DEBUG="nvv4l2decoder:5,nvv4l2h265enc:4,nvinfer:4" \
  gst-launch-1.0 ... pipeline ...

# Debug caps negotiation
GST_DEBUG="GST_CAPS:5" gst-launch-1.0 ... pipeline ...

# Debug buffer flow
GST_DEBUG="GST_BUFFER:5" gst-launch-1.0 ... pipeline ...

# Write debug output to file
GST_DEBUG="*:3" GST_DEBUG_FILE=/tmp/gst_debug.log \
  gst-launch-1.0 ... pipeline ...
```

### 15.7 瓶颈定位方法

```
Step 1: Run tegrastats during pipeline execution.

Step 2: Identify the constrained resource:
  - GPU > 90%        -> Inference is the bottleneck
  - NVDEC > 90%      -> Decode is the bottleneck (too many streams)
  - NVENC > 90%      -> Encode is the bottleneck
  - CPU > 80% total  -> CPU processing in pipeline (software element?)
  - RAM near limit   -> Memory pressure, possible swap thrashing

Step 3: Address the bottleneck:
  GPU-limited:
    - Reduce model size (YOLOv8n instead of YOLOv8s)
    - Use INT8 quantization
    - Increase nvinfer interval (skip frames)
    - Reduce input resolution to inference

  NVDEC-limited:
    - Reduce number of streams
    - Lower stream resolution at the source
    - Use more efficient codec (H.265 vs H.264)

  NVENC-limited:
    - Reduce output resolution or framerate
    - Use lower preset-level
    - Encode fewer output streams

  CPU-limited:
    - Replace software GStreamer elements with NVIDIA equivalents
    - Move CPU processing to GPU (CUDA kernels)
    - Reduce metadata processing frequency
```

### 15.8 性能剖析脚本

```bash
#!/bin/bash
# profile_pipeline.sh -- Capture tegrastats during pipeline run
DURATION=${1:-60}
CONFIG=${2:-"config.txt"}
OUTPUT_DIR="profile_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_DIR"

# Start tegrastats logging in background
sudo tegrastats --interval 200 --logfile "$OUTPUT_DIR/tegrastats.log" &
TEGRA_PID=$!

# Run DeepStream with perf enabled
timeout "$DURATION" deepstream-app -c "$CONFIG" 2>&1 | \
  tee "$OUTPUT_DIR/deepstream.log"

# Stop tegrastats
sudo kill $TEGRA_PID 2>/dev/null

# Parse tegrastats for GPU/NVDEC/NVENC utilization
echo "=== GPU Utilization ==="
grep -oP 'GR3D_FREQ \K[0-9]+' "$OUTPUT_DIR/tegrastats.log" | \
  awk '{sum+=$1; n++} END {print "Avg:", sum/n "%", "Max:", max}'

echo "=== NVDEC Utilization ==="
grep -oP 'NVDEC \K[0-9]+' "$OUTPUT_DIR/tegrastats.log" | \
  awk '{sum+=$1; n++} END {print "Avg:", sum/n "%"}'

echo "=== Power ==="
grep -oP 'VDD_IN \K[0-9]+' "$OUTPUT_DIR/tegrastats.log" | \
  awk '{sum+=$1; n++} END {print "Avg:", sum/n "mW"}'

echo "Results saved to $OUTPUT_DIR/"
```

---

## 16. 生产部署


<details>
<summary>English original</summary>

**15.4 Pipeline DOT Graphs**

Visualize the negotiated pipeline including caps and buffer pools:

```bash
export GST_DEBUG_DUMP_DOT_DIR=/tmp/gst-dots
mkdir -p /tmp/gst-dots

# Run pipeline (DOT files generated at state changes)
gst-launch-1.0 ... your pipeline ...

# Convert to PNG
sudo apt-get install graphviz
dot -Tpng /tmp/gst-dots/0.00.00.*PLAYING*.dot -o pipeline.png

# The DOT graph shows:
# - Element names and types
# - Negotiated caps on each link
# - Buffer pool configurations
# - Queue levels
```

**15.5 NVIDIA Nsight Systems Profiling**

For deep analysis of CUDA kernels, NVDEC/NVENC timing, and memory transfers:

```bash
# Profile a DeepStream application
nsys profile \
  --trace=cuda,nvtx,nvmedia,osrt \
  --output=ds_profile \
  --duration=30 \
  deepstream-app -c config.txt

# Profile a GStreamer pipeline
nsys profile \
  --trace=cuda,nvtx,osrt \
  --output=gst_profile \
  gst-launch-1.0 ... your pipeline ...

# View results
nsys-ui ds_profile.nsys-rep
```

In the Nsight Systems GUI, look for:
- **CUDA kernel timeline:** Identify inference kernel duration and gaps.
- **NVDEC row:** Shows decode operations and idle time.
- **NVENC row:** Shows encode operations.
- **Memory transfers:** Any unexpected `cudaMemcpy` indicates broken zero-copy.
- **CPU thread activity:** High CPU usage suggests software processing in the path.

**15.6 GST_DEBUG for Plugin-Level Debugging**

```bash
# Debug levels: 0=none, 1=ERROR, 2=WARNING, 3=FIXME, 4=INFO, 5=DEBUG, 6=LOG, 7=TRACE

# Debug specific plugins
GST_DEBUG="nvv4l2decoder:5,nvv4l2h265enc:4,nvinfer:4" \
  gst-launch-1.0 ... pipeline ...

# Debug caps negotiation
GST_DEBUG="GST_CAPS:5" gst-launch-1.0 ... pipeline ...

# Debug buffer flow
GST_DEBUG="GST_BUFFER:5" gst-launch-1.0 ... pipeline ...

# Write debug output to file
GST_DEBUG="*:3" GST_DEBUG_FILE=/tmp/gst_debug.log \
  gst-launch-1.0 ... pipeline ...
```

**15.7 Bottleneck Identification Methodology**

```
Step 1: Run tegrastats during pipeline execution.

Step 2: Identify the constrained resource:
  - GPU > 90%        -> Inference is the bottleneck
  - NVDEC > 90%      -> Decode is the bottleneck (too many streams)
  - NVENC > 90%      -> Encode is the bottleneck
  - CPU > 80% total  -> CPU processing in pipeline (software element?)
  - RAM near limit   -> Memory pressure, possible swap thrashing

Step 3: Address the bottleneck:
  GPU-limited:
    - Reduce model size (YOLOv8n instead of YOLOv8s)
    - Use INT8 quantization
    - Increase nvinfer interval (skip frames)
    - Reduce input resolution to inference

  NVDEC-limited:
    - Reduce number of streams
    - Lower stream resolution at the source
    - Use more efficient codec (H.265 vs H.264)

  NVENC-limited:
    - Reduce output resolution or framerate
    - Use lower preset-level
    - Encode fewer output streams

  CPU-limited:
    - Replace software GStreamer elements with NVIDIA equivalents
    - Move CPU processing to GPU (CUDA kernels)
    - Reduce metadata processing frequency
```

**15.8 Profiling Script**

```bash
#!/bin/bash
# profile_pipeline.sh -- Capture tegrastats during pipeline run
DURATION=${1:-60}
CONFIG=${2:-"config.txt"}
OUTPUT_DIR="profile_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_DIR"

# Start tegrastats logging in background
sudo tegrastats --interval 200 --logfile "$OUTPUT_DIR/tegrastats.log" &
TEGRA_PID=$!

# Run DeepStream with perf enabled
timeout "$DURATION" deepstream-app -c "$CONFIG" 2>&1 | \
  tee "$OUTPUT_DIR/deepstream.log"

# Stop tegrastats
sudo kill $TEGRA_PID 2>/dev/null

# Parse tegrastats for GPU/NVDEC/NVENC utilization
echo "=== GPU Utilization ==="
grep -oP 'GR3D_FREQ \K[0-9]+' "$OUTPUT_DIR/tegrastats.log" | \
  awk '{sum+=$1; n++} END {print "Avg:", sum/n "%", "Max:", max}'

echo "=== NVDEC Utilization ==="
grep -oP 'NVDEC \K[0-9]+' "$OUTPUT_DIR/tegrastats.log" | \
  awk '{sum+=$1; n++} END {print "Avg:", sum/n "%"}'

echo "=== Power ==="
grep -oP 'VDD_IN \K[0-9]+' "$OUTPUT_DIR/tegrastats.log" | \
  awk '{sum+=$1; n++} END {print "Avg:", sum/n "mW"}'

echo "Results saved to $OUTPUT_DIR/"
```

---

**16. Production Deployment**

</details>

### 16.1 RTSP Server 输出

最常见的生产输出是 RTSP server，客户端（VMS、NVR、浏览器）可连接并拉取分析后的视频流。

**GStreamer RTSP Server（Python）：**

```python
#!/usr/bin/env python3
"""RTSP server serving DeepStream-analyzed video."""

import gi
gi.require_version('Gst', '1.0')
gi.require_version('GstRtspServer', '1.0')
from gi.repository import Gst, GstRtspServer, GLib

Gst.init(None)

class DeepStreamRTSPServer:
    def __init__(self, port=8554, mount_point="/live"):
        self.server = GstRtspServer.RTSPServer.new()
        self.server.set_service(str(port))

        factory = GstRtspServer.RTSPMediaFactory.new()
        factory.set_launch(
            '( nvarguscamerasrc sensor-id=0 ! '
            'video/x-raw(memory:NVMM),width=1920,height=1080,'
            'framerate=30/1,format=NV12 ! '
            'nvvideoconvert ! video/x-raw(memory:NVMM),format=NV12 ! '
            'nvv4l2h265enc bitrate=4000000 preset-level=1 '
            'insert-sps-pps=true idrinterval=30 '
            'maxperf-enable=true ! '
            'h265parse ! rtph265pay name=pay0 pt=96 )'
        )
        factory.set_shared(True)

        mounts = self.server.get_mount_points()
        mounts.add_factory(mount_point, factory)
        self.server.attach(None)

        print(f"RTSP server running at rtsp://localhost:{port}{mount_point}")

    def run(self):
        loop = GLib.MainLoop()
        loop.run()

if __name__ == "__main__":
    server = DeepStreamRTSPServer(port=8554, mount_point="/live")
    server.run()
```

**DeepStream 配置文件方式：**

```ini
[sink0]
enable=1
type=4                          # RTSP output
rtsp-port=8554
udp-port=5400
codec=1                         # 0=H.264, 1=H.265
bitrate=4000000
enc-type=0                      # 0=hardware encoder
sync=0

[sink1]
enable=0                        # Disable display sink for headless
type=2
```

**客户端连接：**

```bash
# VLC
vlc rtsp://jetson-ip:8554/live

# GStreamer client
gst-launch-1.0 rtspsrc location=rtsp://jetson-ip:8554/live latency=100 ! \
  rtph265depay ! h265parse ! avdec_h265 ! videoconvert ! autovideosink

# FFmpeg
ffplay -rtsp_transport tcp rtsp://jetson-ip:8554/live
```

### 16.2 录制到磁盘

**带分片轮转的连续录制：**

```bash
# Record to segmented MP4 files (5-minute segments)
gst-launch-1.0 nvarguscamerasrc ! \
  'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
  nvv4l2h265enc bitrate=4000000 ! h265parse ! \
  splitmuxsink location="recording_%05d.mp4" \
    max-size-time=300000000000 \
    muxer-factory=mp4mux
```

**用于录制的 DeepStream 配置：**

```ini
[sink2]
enable=1
type=3                          # File output
container=1                     # 1=MP4
codec=1                         # 1=H.265
enc-type=0                      # 0=hardware encoder
bitrate=4000000
output-file=recording.mp4
source-id=0                     # Record specific source
```

**带元数据 sidecar 的录制（用于取证复查）：**

```python
import json
import time
from pathlib import Path

class MetadataRecorder:
    """Records detection metadata alongside video for later review."""

    def __init__(self, output_dir):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.current_file = None
        self.segment_start = None

    def new_segment(self, video_filename):
        if self.current_file:
            self.current_file.close()
        meta_filename = video_filename.replace('.mp4', '.jsonl')
        self.current_file = open(self.output_dir / meta_filename, 'w')
        self.segment_start = time.time()

    def record_detection(self, frame_num, timestamp, detections):
        record = {
            "frame": frame_num,
            "pts": timestamp,
            "wall_clock": time.time(),
            "detections": detections
        }
        self.current_file.write(json.dumps(record) + '\n')

    def close(self):
        if self.current_file:
            self.current_file.close()
```

### 16.3 错误恢复与流水线状态管理

生产环境中的流水线必须能优雅地处理错误，无需人工干预。

**带自动恢复的总线监视：**

```python
import time
import threading

class ResilientPipeline:
    """Pipeline wrapper with automatic error recovery."""

    def __init__(self, pipeline, max_retries=10, retry_delay=5):
        self.pipeline = pipeline
        self.max_retries = max_retries
        self.retry_delay = retry_delay
        self.retry_count = 0
        self.running = True

        # Install bus watch
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect("message::error", self._on_error)
        bus.connect("message::eos", self._on_eos)
        bus.connect("message::state-changed", self._on_state_changed)

    def _on_error(self, bus, message):
        err, debug = message.parse_error()
        element = message.src.get_name()
        print(f"ERROR from {element}: {err.message}")
        print(f"Debug: {debug}")

        if self.retry_count < self.max_retries:
            self.retry_count += 1
            print(f"Attempting recovery ({self.retry_count}/{self.max_retries})...")
            self._restart()
        else:
            print("Max retries exceeded. Shutting down.")
            self.running = False

    def _on_eos(self, bus, message):
        print("End of stream. Restarting for loop playback...")
        self._restart()

    def _on_state_changed(self, bus, message):
        if message.src == self.pipeline:
            old, new, pending = message.parse_state_changed()
            if new == Gst.State.PLAYING:
                self.retry_count = 0  # Reset on successful play

    def _restart(self):
        self.pipeline.set_state(Gst.State.NULL)
        time.sleep(self.retry_delay)
        ret = self.pipeline.set_state(Gst.State.PLAYING)
        if ret == Gst.StateChangeReturn.FAILURE:
            print("Failed to restart pipeline")

    def start(self):
        self.pipeline.set_state(Gst.State.PLAYING)

    def stop(self):
        self.running = False
        self.pipeline.send_event(Gst.Event.new_eos())
        time.sleep(2)
        self.pipeline.set_state(Gst.State.NULL)
```

### 16.4 RTSP 源重连

```ini
# DeepStream config: auto-reconnect RTSP sources
[source0]
enable=1
type=4
uri=rtsp://camera.local:554/live
rtsp-reconnect-interval-sec=5      # Reconnect every 5 seconds on failure
latency=200
num-sources=1
```

如需通过程序控制：

```python
def handle_rtsp_source_error(src_element, source_id):
    """Handle RTSP source disconnection and reconnect."""
    print(f"Source {source_id} disconnected. Reconnecting...")
    src_element.set_state(Gst.State.NULL)
    time.sleep(3)
    src_element.set_state(Gst.State.PLAYING)
```

### 16.5 长时间运行稳定性

必须 24/7 运行的流水线需考虑的事项：

**内存泄漏防护：**

```python
# Periodically check for buffer leaks
def monitor_memory():
    import psutil
    process = psutil.Process()
    initial_rss = process.memory_info().rss

    while True:
        current_rss = process.memory_info().rss
        delta_mb = (current_rss - initial_rss) / (1024 * 1024)

        if delta_mb > 500:  # 500 MB growth threshold
            print(f"WARNING: Memory grew by {delta_mb:.0f} MB. "
                  f"Possible leak. Current RSS: "
                  f"{current_rss / (1024*1024):.0f} MB")

        time.sleep(60)
```

**热管理：**

```bash
# Monitor thermal zone temperatures
cat /sys/class/thermal/thermal_zone*/temp
# Values are in millidegrees Celsius (45000 = 45.0C)

# Set fan to maximum for 24/7 operation
sudo sh -c 'echo 255 > /sys/devices/pwm-fan/target_pwm'
```

```python
def check_thermal():
    """Read SoC junction temperature and throttle if needed."""
    with open('/sys/class/thermal/thermal_zone0/temp') as f:
        temp_mc = int(f.read().strip())
    temp_c = temp_mc / 1000.0

    if temp_c > 85:
        print(f"THERMAL WARNING: {temp_c}C -- throttling pipeline")
        # Reduce inference interval or drop streams
        return True
    return False
```

**看门狗定时器：**

```python
class PipelineWatchdog:
    """Restarts pipeline if no frames received within timeout."""

    def __init__(self, pipeline, timeout_sec=30):
        self.pipeline = pipeline
        self.timeout = timeout_sec
        self.last_frame_time = time.time()
        self.lock = threading.Lock()

    def frame_received(self):
        with self.lock:
            self.last_frame_time = time.time()

    def monitor(self):
        while True:
            with self.lock:
                elapsed = time.time() - self.last_frame_time

            if elapsed > self.timeout:
                print(f"WATCHDOG: No frames for {elapsed:.0f}s. Restarting...")
                self.pipeline.set_state(Gst.State.NULL)
                time.sleep(5)
                self.pipeline.set_state(Gst.State.PLAYING)
                with self.lock:
                    self.last_frame_time = time.time()

            time.sleep(5)
```

### 16.6 用于自动启动的 systemd 服务

```ini
# /etc/systemd/system/deepstream-analytics.service
[Unit]
Description=DeepStream Video Analytics Pipeline
After=network-online.target nvpmodel.service
Wants=network-online.target

[Service]
Type=simple
User=root
Environment="DISPLAY=:0"
Environment="GST_DEBUG=2"
ExecStartPre=/usr/bin/nvpmodel -m 0
ExecStartPre=/usr/bin/jetson_clocks
ExecStart=/usr/bin/deepstream-app -c /opt/analytics/config.txt
Restart=always
RestartSec=10
WatchdogSec=120
StandardOutput=journal
StandardError=journal
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable deepstream-analytics
sudo systemctl start deepstream-analytics
sudo journalctl -u deepstream-analytics -f   # View logs
```

### 16.7 生产部署检查清单

| 项                            | 操作                                              |
|---------------------------------|-----------------------------------------------------|
| 电源模式                      | `sudo nvpmodel -m 0`（最大 15W）                     |
| 时钟锁定                   | `sudo jetson_clocks`（最高频率）                     |
| TensorRT engine                | 预构建，不在 runtime 构建                     |
| 风扇控制                     | 24/7 设为最大（`echo 255 > target_pwm`）       |
| RTSP 重连                  | `rtsp-reconnect-interval-sec=5`                     |
| 错误恢复                  | Bus watch 与自动重启                         |
| 看门狗                        | 帧超时检测                             |
| 热管理监控                      | 85C 告警，90C 降频或关机           |
| 内存监控                   | 跟踪 RSS 增长，增长 > 500 MB 时告警          |
| 日志轮转                    | 对 GST_DEBUG 和应用日志使用 `logrotate`              |
| Systemd 服务                 | 自动启动，失败时重启                      |
| 健康端点                 | 上报流水线状态的 HTTP 端点              |
| 指标导出                  | 用 Prometheus/Grafana 采集 FPS、延迟、温度    |

---

## 17. 常见问题与调试


<details>
<summary>English original</summary>

**16.5 Long-Running Stability**

Considerations for pipelines that must run 24/7:

**Memory leak prevention:**

```python
# Periodically check for buffer leaks
def monitor_memory():
    import psutil
    process = psutil.Process()
    initial_rss = process.memory_info().rss

    while True:
        current_rss = process.memory_info().rss
        delta_mb = (current_rss - initial_rss) / (1024 * 1024)

        if delta_mb > 500:  # 500 MB growth threshold
            print(f"WARNING: Memory grew by {delta_mb:.0f} MB. "
                  f"Possible leak. Current RSS: "
                  f"{current_rss / (1024*1024):.0f} MB")

        time.sleep(60)
```

**Thermal management:**

```bash
# Monitor thermal zone temperatures
cat /sys/class/thermal/thermal_zone*/temp
# Values are in millidegrees Celsius (45000 = 45.0C)

# Set fan to maximum for 24/7 operation
sudo sh -c 'echo 255 > /sys/devices/pwm-fan/target_pwm'
```

```python
def check_thermal():
    """Read SoC junction temperature and throttle if needed."""
    with open('/sys/class/thermal/thermal_zone0/temp') as f:
        temp_mc = int(f.read().strip())
    temp_c = temp_mc / 1000.0

    if temp_c > 85:
        print(f"THERMAL WARNING: {temp_c}C -- throttling pipeline")
        # Reduce inference interval or drop streams
        return True
    return False
```

**Watchdog timer:**

```python
class PipelineWatchdog:
    """Restarts pipeline if no frames received within timeout."""

    def __init__(self, pipeline, timeout_sec=30):
        self.pipeline = pipeline
        self.timeout = timeout_sec
        self.last_frame_time = time.time()
        self.lock = threading.Lock()

    def frame_received(self):
        with self.lock:
            self.last_frame_time = time.time()

    def monitor(self):
        while True:
            with self.lock:
                elapsed = time.time() - self.last_frame_time

            if elapsed > self.timeout:
                print(f"WATCHDOG: No frames for {elapsed:.0f}s. Restarting...")
                self.pipeline.set_state(Gst.State.NULL)
                time.sleep(5)
                self.pipeline.set_state(Gst.State.PLAYING)
                with self.lock:
                    self.last_frame_time = time.time()

            time.sleep(5)
```

**16.6 Systemd Service for Auto-Start**

```ini
# /etc/systemd/system/deepstream-analytics.service
[Unit]
Description=DeepStream Video Analytics Pipeline
After=network-online.target nvpmodel.service
Wants=network-online.target

[Service]
Type=simple
User=root
Environment="DISPLAY=:0"
Environment="GST_DEBUG=2"
ExecStartPre=/usr/bin/nvpmodel -m 0
ExecStartPre=/usr/bin/jetson_clocks
ExecStart=/usr/bin/deepstream-app -c /opt/analytics/config.txt
Restart=always
RestartSec=10
WatchdogSec=120
StandardOutput=journal
StandardError=journal
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable deepstream-analytics
sudo systemctl start deepstream-analytics
sudo journalctl -u deepstream-analytics -f   # View logs
```

**16.7 Production Deployment Checklist**

| Item                            | Action                                              |
|---------------------------------|-----------------------------------------------------|
| Power mode                      | `sudo nvpmodel -m 0` (15W max)                     |
| Clock locking                   | `sudo jetson_clocks` (max freq)                     |
| TensorRT engines                | Pre-built, not built at runtime                     |
| Fan control                     | Set to max for 24/7 (`echo 255 > target_pwm`)       |
| RTSP reconnect                  | `rtsp-reconnect-interval-sec=5`                     |
| Error recovery                  | Bus watch with auto-restart                         |
| Watchdog                        | Frame timeout detection                             |
| Thermal monitoring              | Alert at 85C, throttle or shutdown at 90C           |
| Memory monitoring               | Track RSS growth, alert on > 500 MB growth          |
| Log rotation                    | `logrotate` for GST_DEBUG and app logs              |
| Systemd service                 | Auto-start, restart on failure                      |
| Health endpoint                 | HTTP endpoint reporting pipeline status              |
| Metrics export                  | Prometheus/Grafana for FPS, latency, temperature    |

---

**17. Common Issues and Debugging**

</details>

### 17.1 流水线协商错误

**症状：** `not-negotiated (-4)` 错误或 `Internal data stream error`。

```
ERROR from element nvv4l2decoder0: Internal data stream error.
Debug info: gstbasesrc.c(3127): gst_base_src_loop():
  streaming stopped, reason not-negotiated (-4)
```

**常见原因与修复方法：**

| 原因                                     | 修复方法                                      |
|------------------------------------------|----------------------------------------------|
| decoder 前缺少 parser                    | 添加 `h264parse` 或 `h265parse`                 |
| caps 格式错误                            | 检查 caps filter 中的 `(memory:NVMM)`        |
| 像素格式不兼容                           | 在元素之间插入 `nvvideoconvert`              |
| 分辨率超过 codec 上限                    | 降低分辨率                                    |
| 缺少 demuxer                             | 为 MP4 添加 `qtdemux`，为 MKV 添加 `matroskademux` |

**调试方法：**

```bash
# Dump negotiation details
GST_DEBUG="GST_CAPS:5,nvv4l2decoder:5" gst-launch-1.0 ... 2>&1 | head -200

# Check what caps each element supports
gst-inspect-1.0 nvv4l2decoder   # Look at "Pad Templates" section
```

### 17.2 缓冲区饥饿与 underrun

**症状：** 播放卡顿、丢帧、延迟升高。

```
WARNING from element nvv4l2decoder0: Output buffer pool has no free buffers
```

**修复方法：**

```bash
# Increase decoder output buffer pool
gst-launch-1.0 ... ! nvv4l2decoder num-extra-surfaces=4 ! ...

# Add queue elements with larger buffers
gst-launch-1.0 ... ! queue max-size-buffers=10 max-size-time=0 max-size-bytes=0 ! ...

# For live sources, increase latency tolerance
gst-launch-1.0 rtspsrc location=... latency=500 ! ...
```

### 17.3 codec 限制与变通方法

| 限制                                | 变通方法                                          |
|-------------------------------------|---------------------------------------------------|
| 不支持 VP9/AV1 编码                  | 为存储/流媒体转码为 H.265                         |
| 不支持 H.264 10-bit 编码             | 10-bit 编码改用 H.265 Main10                      |
| 不支持渐进式 JPEG（NVJPEG）          | 渐进式 JPEG 改用 libjpeg-turbo                    |
| 单个 NVDEC 实例                     | 按像素预算限制并发 decode 流数量                  |
| 单个 NVENC 实例                     | 编码分时复用；减少编码流数量                      |
| 不支持隔行视频编码                    | 先用 `nvvideoconvert` 去隔行                        |

### 17.4 内存泄漏

**检测方法：**

```bash
# Monitor RSS growth over time
watch -n 5 'ps -o pid,rss,vsz,comm -p $(pgrep deepstream-app)'

# Use valgrind (slow, use on development machine, not production)
valgrind --leak-check=full --show-leak-kinds=all \
  deepstream-app -c config.txt 2>&1 | tee valgrind.log

# GStreamer's built-in leak tracer
GST_TRACERS="leaks(check-refs=true)" gst-launch-1.0 ... pipeline ...
```

**常见泄漏来源：**

| 来源                                | 修复方法                                         |
|-------------------------------------|-------------------------------------------------|
| GstBuffer 引用未释放                 | 确保每条代码路径中都执行 `gst_buffer_unref()`  |
| pad probe 泄漏                      | 流水线停止时移除 probe                            |
| Python pyds 元数据引用               | 使用 `try/except StopIteration` 模式                       |
| NvBufSurface 未 unmap               | 始终将 `Map` 与 `UnMap` 配对                   |
| 动态 src pad 未释放                  | source 移除时释放 request pad                     |

### 17.5 NVDEC/NVENC 错误

**症状：** decoder 或 encoder 起初正常，之后返回错误。

```
ERROR from element nvv4l2decoder0: Could not decode stream.
```

**诊断步骤：**

```bash
# Check if codec device is available
ls -la /dev/video0   # decoder
ls -la /dev/video1   # encoder

# Check for other processes holding the codec
sudo fuser /dev/video0
sudo fuser /dev/video1

# Check kernel log for hardware errors
dmesg | grep -i -E "nvdec|nvenc|tegra-video|fault"

# Verify clock status
sudo cat /sys/kernel/debug/clk/nvdec/clk_rate
sudo cat /sys/kernel/debug/clk/msenc/clk_rate

# Reset video engines (last resort)
sudo systemctl restart nvargus-daemon
```


<details>
<summary>English original</summary>

**17.1 Pipeline Negotiation Errors**

**Symptom:** `not-negotiated (-4)` error or `Internal data stream error`.

```
ERROR from element nvv4l2decoder0: Internal data stream error.
Debug info: gstbasesrc.c(3127): gst_base_src_loop():
  streaming stopped, reason not-negotiated (-4)
```

**Common causes and fixes:**

| Cause                                    | Fix                                          |
|------------------------------------------|----------------------------------------------|
| Missing parser before decoder            | Add `h264parse` or `h265parse`               |
| Wrong caps format                        | Check `(memory:NVMM)` in caps filter         |
| Incompatible pixel format                | Insert `nvvideoconvert` between elements     |
| Resolution exceeds codec maximum         | Reduce resolution                            |
| Missing demuxer                          | Add `qtdemux` for MP4 or `matroskademux` for MKV |

**Debugging approach:**

```bash
# Dump negotiation details
GST_DEBUG="GST_CAPS:5,nvv4l2decoder:5" gst-launch-1.0 ... 2>&1 | head -200

# Check what caps each element supports
gst-inspect-1.0 nvv4l2decoder   # Look at "Pad Templates" section
```

**17.2 Buffer Starvation and Underruns**

**Symptom:** Choppy playback, frame drops, increasing latency.

```
WARNING from element nvv4l2decoder0: Output buffer pool has no free buffers
```

**Fixes:**

```bash
# Increase decoder output buffer pool
gst-launch-1.0 ... ! nvv4l2decoder num-extra-surfaces=4 ! ...

# Add queue elements with larger buffers
gst-launch-1.0 ... ! queue max-size-buffers=10 max-size-time=0 max-size-bytes=0 ! ...

# For live sources, increase latency tolerance
gst-launch-1.0 rtspsrc location=... latency=500 ! ...
```

**17.3 Codec Limitations and Workarounds**

| Limitation                          | Workaround                                        |
|-------------------------------------|---------------------------------------------------|
| No VP9/AV1 encode                   | Transcode to H.265 for storage/streaming          |
| No H.264 10-bit encode              | Use H.265 Main10 for 10-bit encode                |
| No progressive JPEG (NVJPEG)        | Use libjpeg-turbo for progressive JPEG             |
| Single NVDEC instance               | Limit concurrent decode streams to pixel budget    |
| Single NVENC instance               | Time-slice encode; reduce encode streams           |
| No interlaced video encode           | Deinterlace with `nvvideoconvert` first            |

**17.4 Memory Leaks**

**Detection:**

```bash
# Monitor RSS growth over time
watch -n 5 'ps -o pid,rss,vsz,comm -p $(pgrep deepstream-app)'

# Use valgrind (slow, use on development machine, not production)
valgrind --leak-check=full --show-leak-kinds=all \
  deepstream-app -c config.txt 2>&1 | tee valgrind.log

# GStreamer's built-in leak tracer
GST_TRACERS="leaks(check-refs=true)" gst-launch-1.0 ... pipeline ...
```

**Common leak sources:**

| Source                              | Fix                                             |
|-------------------------------------|-------------------------------------------------|
| Unreleased GstBuffer refs           | Ensure `gst_buffer_unref()` in every code path  |
| Leaked pad probes                   | Remove probes when pipeline stops                |
| Python pyds metadata refs           | Use `try/except StopIteration` pattern           |
| NvBufSurface not unmapped           | Always pair `Map` with `UnMap`                   |
| Dynamic source pads not released    | Release request pads on source removal           |

**17.5 NVDEC/NVENC Errors**

**Symptom:** Decoder or encoder returns error after working initially.

```
ERROR from element nvv4l2decoder0: Could not decode stream.
```

**Diagnostic steps:**

```bash
# Check if codec device is available
ls -la /dev/video0   # decoder
ls -la /dev/video1   # encoder

# Check for other processes holding the codec
sudo fuser /dev/video0
sudo fuser /dev/video1

# Check kernel log for hardware errors
dmesg | grep -i -E "nvdec|nvenc|tegra-video|fault"

# Verify clock status
sudo cat /sys/kernel/debug/clk/nvdec/clk_rate
sudo cat /sys/kernel/debug/clk/msenc/clk_rate

# Reset video engines (last resort)
sudo systemctl restart nvargus-daemon
```

</details>

### 17.6 DeepStream 模型加载失败

**症状：** `nvinfer` 无法加载模型或构建 engine。

```
ERROR: Failed to create engine from model file
```

**修复方法：**

```bash
# Verify engine file matches GPU architecture
# Orin Nano = SM 8.7 (compute capability)
# Engine built on different GPU will not load

# Rebuild engine for this platform
/usr/src/tensorrt/bin/trtexec --onnx=model.onnx \
  --saveEngine=model_orin_fp16.engine --fp16

# Check engine compatibility
python3 -c "
import tensorrt as trt
logger = trt.Logger(trt.Logger.INFO)
runtime = trt.Runtime(logger)
with open('model.engine', 'rb') as f:
    engine = runtime.deserialize_cuda_engine(f.read())
    if engine is None:
        print('Engine failed to load -- likely wrong platform')
    else:
        print(f'Engine loaded: {engine.num_bindings} bindings')
"

# Common causes:
# 1. Engine built on x86 (SM 8.6) but running on Orin (SM 8.7)
# 2. TensorRT version mismatch between build and runtime
# 3. Batch size in config exceeds engine max batch
# 4. Insufficient workspace memory
```

### 17.7 GST_DEBUG 用法参考

```bash
# Environment variables for GStreamer debugging

# Set global debug level
export GST_DEBUG=3           # WARNING and above for all

# Set per-element debug level (comma-separated)
export GST_DEBUG="nvv4l2decoder:5,nvinfer:4,nvstreammux:3"

# Write to file instead of stderr
export GST_DEBUG_FILE=/tmp/gst_debug.log

# Enable color output (useful for terminal)
export GST_DEBUG_COLOR_MODE=on

# Dump pipeline graphs
export GST_DEBUG_DUMP_DOT_DIR=/tmp/gst-dots

# Debug categories for NVIDIA plugins:
# nvv4l2decoder:N  -- Hardware decoder
# nvv4l2h264enc:N  -- H.264 encoder
# nvv4l2h265enc:N  -- H.265 encoder
# nvvideoconvert:N -- Video format converter
# nvinfer:N        -- TensorRT inference
# nvtracker:N      -- Object tracker
# nvstreammux:N    -- Stream muxer
# nvdsosd:N        -- On-screen display
# nvmsgconv:N      -- Message converter
# nvmsgbroker:N    -- Message broker

# Example: debug a failing decode pipeline
GST_DEBUG="GST_CAPS:4,nvv4l2decoder:6,GST_BUFFER:4" \
  gst-launch-1.0 filesrc location=test.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! nvvideoconvert ! nv3dsink 2>&1 | tee debug.log
```

### 17.8 常见错误消息及解决方法

| 错误消息                                                     | 可能原因                               | 解决方法                                       |
|--------------------------------------------------------------|----------------------------------------|------------------------------------------------|
| `nvbufsurface: mapping of buffer failed`                     | NVMM 内存不足                          | 减少 buffer pool 或流数量                      |
| `Failed to create NvBufSurface`                              | 并发分配过多                           | 减小批大小或分辨率                             |
| `nvv4l2decoder: capture plane deque buffer failed`           | NVDEC 过载                             | 减少 decode 流数                               |
| `Error in NvBufSurfTransformAsync`                           | VIC 队列已满                           | 在 nvvideoconvert 前添加 queue                 |
| `Pipeline doesn't want to PREROLL`                          | element 缺失或 link 断开               | 检查所有 link；使用 DOT 图                     |
| `no element "nvv4l2decoder"`                                | 插件未安装                             | 核验 JetPack 安装；`gst-inspect-1.0 nvv4l2decoder`                        |
| `Could not initialize nvbufsurface`                          | 显示服务器未运行（headless）           | 使用 fakesink 或设置 `DISPLAY=:0`                      |
| `ERROR from nvinfer: TensorRT inference failed`              | engine/模型错误                        | 为该平台重新构建 engine                        |
| `nvstreammux: Failed to get batch`                           | 没有 source 产生帧                     | 检查 source 连通性与状态                       |
| `RTSP connection timed out`                                  | 网络或摄像头问题                       | 设置 `rtsp-reconnect-interval-sec=5`                                      |

### 17.9 性能随时间下降

如果连续运行数小时或数天后性能下降：

```bash
# Check for thermal throttling
cat /sys/devices/virtual/thermal/thermal_zone*/temp
# Junction temp > 85000 means throttling is likely active

# Check for memory fragmentation
cat /proc/buddyinfo
free -h

# Check for swap usage (severe performance hit)
swapon --show
cat /proc/meminfo | grep Swap

# Check if clocks dropped from max
sudo cat /sys/kernel/debug/clk/gpcclk/clk_rate  # GPU clock
sudo cat /sys/kernel/debug/clk/nvdec/clk_rate    # NVDEC clock

# Re-lock clocks (may be lost after thermal throttle)
sudo jetson_clocks

# Check for zombie processes consuming resources
ps aux | grep -i -E "defunct|zombie"

# Check open file descriptors (RTSP connections can leak fds)
ls /proc/$(pgrep deepstream-app)/fd | wc -l
```


<details>
<summary>English original</summary>

**17.6 DeepStream Model Loading Failures**

**Symptom:** `nvinfer` fails to load model or build engine.

```
ERROR: Failed to create engine from model file
```

**Fixes:**

```bash
# Verify engine file matches GPU architecture
# Orin Nano = SM 8.7 (compute capability)
# Engine built on different GPU will not load

# Rebuild engine for this platform
/usr/src/tensorrt/bin/trtexec --onnx=model.onnx \
  --saveEngine=model_orin_fp16.engine --fp16

# Check engine compatibility
python3 -c "
import tensorrt as trt
logger = trt.Logger(trt.Logger.INFO)
runtime = trt.Runtime(logger)
with open('model.engine', 'rb') as f:
    engine = runtime.deserialize_cuda_engine(f.read())
    if engine is None:
        print('Engine failed to load -- likely wrong platform')
    else:
        print(f'Engine loaded: {engine.num_bindings} bindings')
"

# Common causes:
# 1. Engine built on x86 (SM 8.6) but running on Orin (SM 8.7)
# 2. TensorRT version mismatch between build and runtime
# 3. Batch size in config exceeds engine max batch
# 4. Insufficient workspace memory
```

**17.7 GST_DEBUG Usage Reference**

```bash
# Environment variables for GStreamer debugging

# Set global debug level
export GST_DEBUG=3           # WARNING and above for all

# Set per-element debug level (comma-separated)
export GST_DEBUG="nvv4l2decoder:5,nvinfer:4,nvstreammux:3"

# Write to file instead of stderr
export GST_DEBUG_FILE=/tmp/gst_debug.log

# Enable color output (useful for terminal)
export GST_DEBUG_COLOR_MODE=on

# Dump pipeline graphs
export GST_DEBUG_DUMP_DOT_DIR=/tmp/gst-dots

# Debug categories for NVIDIA plugins:
# nvv4l2decoder:N  -- Hardware decoder
# nvv4l2h264enc:N  -- H.264 encoder
# nvv4l2h265enc:N  -- H.265 encoder
# nvvideoconvert:N -- Video format converter
# nvinfer:N        -- TensorRT inference
# nvtracker:N      -- Object tracker
# nvstreammux:N    -- Stream muxer
# nvdsosd:N        -- On-screen display
# nvmsgconv:N      -- Message converter
# nvmsgbroker:N    -- Message broker

# Example: debug a failing decode pipeline
GST_DEBUG="GST_CAPS:4,nvv4l2decoder:6,GST_BUFFER:4" \
  gst-launch-1.0 filesrc location=test.mp4 ! qtdemux ! h265parse ! \
  nvv4l2decoder ! nvvideoconvert ! nv3dsink 2>&1 | tee debug.log
```

**17.8 Common Error Messages and Solutions**

| Error Message                                                | Likely Cause                           | Solution                                       |
|--------------------------------------------------------------|----------------------------------------|------------------------------------------------|
| `nvbufsurface: mapping of buffer failed`                     | Out of NVMM memory                     | Reduce buffer pools or stream count            |
| `Failed to create NvBufSurface`                              | Too many concurrent allocations        | Reduce batch size or resolution                |
| `nvv4l2decoder: capture plane deque buffer failed`           | NVDEC overloaded                       | Reduce decode streams                          |
| `Error in NvBufSurfTransformAsync`                           | VIC queue full                         | Add queue before nvvideoconvert                |
| `Pipeline doesn't want to PREROLL`                          | Missing element or broken link         | Check all links; use DOT graph                 |
| `no element "nvv4l2decoder"`                                | Plugin not installed                   | Verify JetPack install; `gst-inspect-1.0 nvv4l2decoder` |
| `Could not initialize nvbufsurface`                          | Display server not running (headless)  | Use fakesink or set `DISPLAY=:0`               |
| `ERROR from nvinfer: TensorRT inference failed`              | Engine/model error                     | Rebuild engine for this platform               |
| `nvstreammux: Failed to get batch`                           | No sources producing frames            | Check source connectivity and state            |
| `RTSP connection timed out`                                  | Network or camera issue                | Set `rtsp-reconnect-interval-sec=5`            |

**17.9 Performance Degradation Over Time**

If performance degrades after hours or days of continuous operation:

```bash
# Check for thermal throttling
cat /sys/devices/virtual/thermal/thermal_zone*/temp
# Junction temp > 85000 means throttling is likely active

# Check for memory fragmentation
cat /proc/buddyinfo
free -h

# Check for swap usage (severe performance hit)
swapon --show
cat /proc/meminfo | grep Swap

# Check if clocks dropped from max
sudo cat /sys/kernel/debug/clk/gpcclk/clk_rate  # GPU clock
sudo cat /sys/kernel/debug/clk/nvdec/clk_rate    # NVDEC clock

# Re-lock clocks (may be lost after thermal throttle)
sudo jetson_clocks

# Check for zombie processes consuming resources
ps aux | grep -i -E "defunct|zombie"

# Check open file descriptors (RTSP connections can leak fds)
ls /proc/$(pgrep deepstream-app)/fd | wc -l
```

</details>

### 17.10 快速诊断检查清单

```
1. Pipeline fails to start:
   [ ] Check GST_DEBUG output for first ERROR
   [ ] Verify all elements exist: gst-inspect-1.0 <element>
   [ ] Dump DOT graph at READY state
   [ ] Check device permissions: ls -la /dev/video*
   [ ] Verify JetPack version matches DeepStream version

2. Pipeline starts but no video output:
   [ ] Check source connectivity (RTSP reachable? File exists?)
   [ ] Verify display server running (for nv3dsink)
   [ ] Check nvstreammux batch-size matches source count
   [ ] Try with fakesink to isolate display issues

3. Low FPS / poor performance:
   [ ] Run tegrastats -- which engine is saturated?
   [ ] Check nvinfer interval setting
   [ ] Verify hardware encoder/decoder (not software fallback)
   [ ] Check power mode: nvpmodel -q
   [ ] Check clocks: sudo jetson_clocks --show

4. Pipeline crashes after running:
   [ ] Check dmesg for GPU/NVDEC faults
   [ ] Monitor memory with tegrastats -- OOM?
   [ ] Check thermal -- throttling or shutdown?
   [ ] Enable core dumps: ulimit -c unlimited
   [ ] Run with GST_DEBUG=3 and check last messages before crash
```

---

## 参考文献

- NVIDIA Jetson Orin Nano Data Sheet -- https://developer.nvidia.com/embedded/jetson-orin-nano
- Jetson Linux Multimedia API Reference -- https://docs.nvidia.com/jetson/archives/l4t-multimedia/index.html
- NVIDIA Accelerated GStreamer Plugins -- https://docs.nvidia.com/jetson/archives/l4t-multimedia/group__gstreamer__plugins.html
- DeepStream SDK Developer Guide -- https://docs.nvidia.com/metropolis/deepstream/dev-guide/
- DeepStream Python Apps Repository -- https://github.com/NVIDIA-AI-IOT/deepstream_python_apps
- NvBufSurface API Reference -- https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_NvBufSurface.html
- TensorRT Developer Guide -- https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/
- GStreamer Documentation -- https://gstreamer.freedesktop.org/documentation/
- NVIDIA Nsight Systems -- https://docs.nvidia.com/nsight-systems/
- Jetson Power and Performance -- https://docs.nvidia.com/jetson/archives/r36.3/DeveloperGuide/SD/PlatformPowerAndPerformance.html
- V4L2 API Specification -- https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html


<details>
<summary>English original</summary>

**17.10 Quick Diagnostic Checklist**

```
1. Pipeline fails to start:
   [ ] Check GST_DEBUG output for first ERROR
   [ ] Verify all elements exist: gst-inspect-1.0 <element>
   [ ] Dump DOT graph at READY state
   [ ] Check device permissions: ls -la /dev/video*
   [ ] Verify JetPack version matches DeepStream version

2. Pipeline starts but no video output:
   [ ] Check source connectivity (RTSP reachable? File exists?)
   [ ] Verify display server running (for nv3dsink)
   [ ] Check nvstreammux batch-size matches source count
   [ ] Try with fakesink to isolate display issues

3. Low FPS / poor performance:
   [ ] Run tegrastats -- which engine is saturated?
   [ ] Check nvinfer interval setting
   [ ] Verify hardware encoder/decoder (not software fallback)
   [ ] Check power mode: nvpmodel -q
   [ ] Check clocks: sudo jetson_clocks --show

4. Pipeline crashes after running:
   [ ] Check dmesg for GPU/NVDEC faults
   [ ] Monitor memory with tegrastats -- OOM?
   [ ] Check thermal -- throttling or shutdown?
   [ ] Enable core dumps: ulimit -c unlimited
   [ ] Run with GST_DEBUG=3 and check last messages before crash
```

---

**References**

- NVIDIA Jetson Orin Nano Data Sheet -- https://developer.nvidia.com/embedded/jetson-orin-nano
- Jetson Linux Multimedia API Reference -- https://docs.nvidia.com/jetson/archives/l4t-multimedia/index.html
- NVIDIA Accelerated GStreamer Plugins -- https://docs.nvidia.com/jetson/archives/l4t-multimedia/group__gstreamer__plugins.html
- DeepStream SDK Developer Guide -- https://docs.nvidia.com/metropolis/deepstream/dev-guide/
- DeepStream Python Apps Repository -- https://github.com/NVIDIA-AI-IOT/deepstream_python_apps
- NvBufSurface API Reference -- https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_NvBufSurface.html
- TensorRT Developer Guide -- https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/
- GStreamer Documentation -- https://gstreamer.freedesktop.org/documentation/
- NVIDIA Nsight Systems -- https://docs.nvidia.com/nsight-systems/
- Jetson Power and Performance -- https://docs.nvidia.com/jetson/archives/r36.3/DeveloperGuide/SD/PlatformPowerAndPerformance.html
- V4L2 API Specification -- https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/1. Nvidia Jetson Platform/Orin-Nano-Video-Codec-DeepStream/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/1.%20Nvidia%20Jetson%20Platform/Orin-Nano-Video-Codec-DeepStream/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
