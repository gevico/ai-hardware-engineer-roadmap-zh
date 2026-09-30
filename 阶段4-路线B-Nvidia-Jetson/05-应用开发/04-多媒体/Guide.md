---
title: 多媒体
description: 多媒体
---

# 多媒体

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">MUL</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · Jetson 方向</p>
<p class="course-identity__title">Multimedia 的专属课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成演示 · 测量：延迟、内存、功耗、日志</p>
</div>
</div>


**阶段 4 — 方向 B — 模块 5.4** · 应用开发

> **重点：** 在 **Jetson Orin Nano 8GB** 上构建硬件加速的多媒体流水线——音频播放/采集、摄像头集成（USB 与 CSI）、GStreamer 视频流水线、显示输出，以及使用 NVIDIA 的 NVENC/NVDEC 引擎进行视频编码/decode。

**Hub：** [5. 应用开发](../Guide.md)
**音频专题深入解析：** [Jetson Audio Setup and Development](02-Jetson音频设置与开发/Guide.md)
**音频应用：** [ESP32-LyraT I2S Microphone Capture on Jetson Orin Nano](01-ESP32-LyraT-I2S麦克风Jetson-Orin-Nano/Guide.md)

---


## 1. 音频（Linux）

Jetson 上的音频使用 ALSA（kernel 驱动）配合 PulseAudio 或 PipeWire（用户态）。

若需以下各项的完整 Jetson 专属模型：

- `APE` vs `HDA`
- 40-pin 排针 `I2S2`
- 自定义 codec 的设备树 bring-up（上电点亮/调通）
- `amixer` 经 `ADMAIF` / `I2S` 的布线
- DAPM 与 ASoC 调试

参见专题指南：[Jetson Audio Setup and Development](02-Jetson音频设置与开发/Guide.md)。

### 查看音频设备

```bash
# List ALSA playback devices
aplay -l

# List ALSA capture devices
arecord -l

# List PulseAudio sinks/sources
pactl list short sinks
pactl list short sources
```

### 播放与采集

```bash
# Play a WAV file
aplay test.wav

# Record 5 seconds of audio
arecord -d 5 -f cd -t wav recording.wav

# Adjust volume
amixer set Master 80%
```

### 自定义载板上的音频

Orin Nano SoM 提供 I2S 音频接口。载板需要一颗 **音频 codec**（如 Realtek ALC5640、TI TLV320AIC），通过 I2S + I2C 控制连接。在设备树中配置：

```dts
sound {
    compatible = "nvidia,tegra-audio-t234";
    nvidia,audio-codec = <&codec>;
    nvidia,i2s-controller = <&i2s1>;
};
```

> **深入解析：** 完整的 Jetson 音频栈、NVIDIA 官方 ASoC 模型、40-pin 排针 pinmux 以及自定义 codec 的 bring-up 流程，参见 [Jetson Audio Setup and Development](02-Jetson音频设置与开发/Guide.md)。

---

## 2. 蓝牙音频配置文件

### A2DP（向蓝牙音箱输出立体声音频）

```bash
bluetoothctl
  power on
  scan on
  pair <speaker-MAC>
  connect <speaker-MAC>

# Route audio to Bluetooth
pactl set-default-sink bluez_sink.<MAC-with-underscores>.a2dp_sink
aplay test.wav   # plays through BT speaker
```

### HFP（免提，双向）

```bash
# Enable HFP in PulseAudio
# /etc/pulse/default.pa: load-module module-bluetooth-policy
# Restart PulseAudio
pulseaudio --kill && pulseaudio --start
```

---

## 3. GStreamer — 音频/视频流水线

GStreamer 是 Jetson 上的标准媒体框架。NVIDIA 提供硬件加速插件（`nvv4l2decoder`、`nvv4l2h264enc`、`nvarguscamerasrc`、`nv3dsink`）。

### 安装与验证

```bash
# GStreamer should be pre-installed in JetPack
gst-inspect-1.0 --version

# List NVIDIA plugins
gst-inspect-1.0 | grep nv
```

### 基本流水线

```bash
# Test video (color bars)
gst-launch-1.0 videotestsrc ! autovideosink

# Test audio (sine wave)
gst-launch-1.0 audiotestsrc ! autoaudiosink

# Play a video file (HW decoded)
gst-launch-1.0 filesrc location=video.mp4 ! \
    qtdemux ! h264parse ! nvv4l2decoder ! nv3dsink
```

---

## 4. 视频编码与播放（GStreamer）

### 硬件加速 decode

```bash
# H.264 decode
gst-launch-1.0 filesrc location=input.mp4 ! \
    qtdemux ! h264parse ! nvv4l2decoder ! nv3dsink

# H.265/HEVC decode
gst-launch-1.0 filesrc location=input.mp4 ! \
    qtdemux ! h265parse ! nvv4l2decoder ! nv3dsink
```

### 硬件加速 encode

```bash
# Camera → H.264 encode → file
gst-launch-1.0 nvarguscamerasrc ! \
    'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
    nvv4l2h264enc bitrate=8000000 ! h264parse ! mp4mux ! \
    filesink location=output.mp4

# Camera → RTSP stream (for remote viewing)
# Use the NVIDIA DeepStream or GStreamer RTSP server
```

### Orin Nano 上的 codec 支持

| Codec | Decode | Encode | 最大分辨率 |
|-------|--------|--------|---------------|
| **H.264** | HW | HW | 4K@60 decode，4K@30 encode |
| **H.265 (HEVC)** | HW | HW | 4K@60 decode，4K@30 encode |
| **VP9** | HW | — | 4K@60 decode |
| **AV1** | HW | — | 4K@60 decode |
| **JPEG** | HW | HW | — |

---


<details>
<summary>English original</summary>

**Multimedia**

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">MUL</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Multimedia.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


**Phase 4 — Track B — Module 5.4** · Application Development

> **Focus:** Build hardware-accelerated multimedia pipelines on the **Jetson Orin Nano 8GB** — audio playback/capture, camera integration (USB and CSI), GStreamer video pipelines, display output, and video encode/decode using NVIDIA's NVENC/NVDEC engines.

**Hub:** [5. Application Development](../Guide.md)
**Dedicated audio deep dive:** [Jetson Audio Setup and Development](02-Jetson音频设置与开发/Guide.md)
**Audio application:** [ESP32-LyraT I2S Microphone Capture on Jetson Orin Nano](01-ESP32-LyraT-I2S麦克风Jetson-Orin-Nano/Guide.md)

---


**1. Audio (Linux)**

Audio on Jetson uses ALSA (kernel driver) with PulseAudio or PipeWire (userspace).

If you want the full Jetson-specific model for:

- `APE` vs `HDA`
- 40-pin header `I2S2`
- device tree bring-up for custom codecs
- `amixer` routing through `ADMAIF` / `I2S`
- DAPM and ASoC debugging

use the dedicated guide: [Jetson Audio Setup and Development](02-Jetson音频设置与开发/Guide.md).

**Check audio devices**

```bash
# List ALSA playback devices
aplay -l

# List ALSA capture devices
arecord -l

# List PulseAudio sinks/sources
pactl list short sinks
pactl list short sources
```

**Playback and capture**

```bash
# Play a WAV file
aplay test.wav

# Record 5 seconds of audio
arecord -d 5 -f cd -t wav recording.wav

# Adjust volume
amixer set Master 80%
```

**Audio on custom carriers**

The Orin Nano SoM provides I2S audio interfaces. Your carrier board needs an **audio codec** (e.g., Realtek ALC5640, TI TLV320AIC) connected via I2S + I2C control. Configure in device tree:

```dts
sound {
    compatible = "nvidia,tegra-audio-t234";
    nvidia,audio-codec = <&codec>;
    nvidia,i2s-controller = <&i2s1>;
};
```

> **Deep dive:** for the full Jetson audio stack, official NVIDIA ASoC model, 40-pin header pinmux, and custom codec bring-up flow, see [Jetson Audio Setup and Development](02-Jetson音频设置与开发/Guide.md).

---

**2. Bluetooth audio profiles**

**A2DP (stereo audio to BT speaker)**

```bash
bluetoothctl
  power on
  scan on
  pair <speaker-MAC>
  connect <speaker-MAC>

# Route audio to Bluetooth
pactl set-default-sink bluez_sink.<MAC-with-underscores>.a2dp_sink
aplay test.wav   # plays through BT speaker
```

**HFP (hands-free, bidirectional)**

```bash
# Enable HFP in PulseAudio
# /etc/pulse/default.pa: load-module module-bluetooth-policy
# Restart PulseAudio
pulseaudio --kill && pulseaudio --start
```

---

**3. GStreamer — audio/video pipelines**

GStreamer is the standard media framework on Jetson. NVIDIA provides hardware-accelerated plugins (`nvv4l2decoder`, `nvv4l2h264enc`, `nvarguscamerasrc`, `nv3dsink`).

**Install and verify**

```bash
# GStreamer should be pre-installed in JetPack
gst-inspect-1.0 --version

# List NVIDIA plugins
gst-inspect-1.0 | grep nv
```

**Basic pipelines**

```bash
# Test video (color bars)
gst-launch-1.0 videotestsrc ! autovideosink

# Test audio (sine wave)
gst-launch-1.0 audiotestsrc ! autoaudiosink

# Play a video file (HW decoded)
gst-launch-1.0 filesrc location=video.mp4 ! \
    qtdemux ! h264parse ! nvv4l2decoder ! nv3dsink
```

---

**4. Video encoding and playback (GStreamer)**

**Hardware-accelerated decode**

```bash
# H.264 decode
gst-launch-1.0 filesrc location=input.mp4 ! \
    qtdemux ! h264parse ! nvv4l2decoder ! nv3dsink

# H.265/HEVC decode
gst-launch-1.0 filesrc location=input.mp4 ! \
    qtdemux ! h265parse ! nvv4l2decoder ! nv3dsink
```

**Hardware-accelerated encode**

```bash
# Camera → H.264 encode → file
gst-launch-1.0 nvarguscamerasrc ! \
    'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
    nvv4l2h264enc bitrate=8000000 ! h264parse ! mp4mux ! \
    filesink location=output.mp4

# Camera → RTSP stream (for remote viewing)
# Use the NVIDIA DeepStream or GStreamer RTSP server
```

**Codec support on Orin Nano**

| Codec | Decode | Encode | Max resolution |
|-------|--------|--------|---------------|
| **H.264** | HW | HW | 4K@60 decode, 4K@30 encode |
| **H.265 (HEVC)** | HW | HW | 4K@60 decode, 4K@30 encode |
| **VP9** | HW | — | 4K@60 decode |
| **AV1** | HW | — | 4K@60 decode |
| **JPEG** | HW | HW | — |

---

</details>

## 5. USB 摄像头 / 网络摄像头（UVC）

任何兼容 UVC 的 USB 摄像头开箱即用。

```bash
# List video devices
v4l2-ctl --list-devices

# Check supported formats
v4l2-ctl -d /dev/video0 --list-formats-ext

# Capture a frame
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=MJPG \
    --stream-mmap --stream-count=1 --stream-to=frame.mjpg

# GStreamer pipeline (USB webcam → display)
gst-launch-1.0 v4l2src device=/dev/video0 ! \
    'video/x-raw,width=1280,height=720,framerate=30/1' ! \
    videoconvert ! autovideosink
```

---

## 6. CSI 摄像头（MIPI）

CSI 摄像头通过载板上的 MIPI CSI-2 接口连接，经由 NVIDIA 的 **Argus** 摄像头框架访问。

### 使用 nvarguscamerasrc（GStreamer）

```bash
# Preview from CSI camera 0
gst-launch-1.0 nvarguscamerasrc sensor-id=0 ! \
    'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
    nvvidconv ! nv3dsink

# Capture to JPEG
gst-launch-1.0 nvarguscamerasrc sensor-id=0 num-buffers=1 ! \
    'video/x-raw(memory:NVMM),width=1920,height=1080' ! \
    nvjpegenc ! filesink location=capture.jpg
```

### 直接使用 V4L2

```bash
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080 \
    --set-ctrl bypass_mode=0 --stream-mmap --stream-count=10
```

### 摄像头设备树配置

CSI 摄像头需要设备树条目，指定 sensor 驱动、I2C 地址、MIPI lane 和像素格式。IMX219 的示例：

```dts
cam0: imx219@10 {
    compatible = "sony,imx219";
    reg = <0x10>;
    clocks = <&bpmp TEGRA234_CLK_EXTPERIPH1>;

    mode0 {
        mclk_khz = "24000";
        num_lanes = "2";
        tegra_sinterface = "serial_a";
        active_w = "3264";
        active_h = "2464";
        pixel_t = "bayer_rggb";
    };
};
```

> **深入阅读：** 完整的摄像头 ISP、sensor bring-up（上电点亮/调通）以及多摄像头配置见 [Orin Nano Camera ISP Sensor Bring-Up](../../01-Nvidia-Jetson平台/02-Orin-Nano摄像头ISP传感器启动/Guide.md)。

---

## 7. 显示输出与分辨率

### HDMI/DisplayPort

```bash
# List outputs and modes
xrandr

# Set specific mode
xrandr --output HDMI-0 --mode 1920x1080 --rate 60

# Force a custom mode
cvt 1280 800 60
xrandr --newmode "1280x800_60" ...
xrandr --addmode HDMI-0 "1280x800_60"
xrandr --output HDMI-0 --mode "1280x800_60"
```

### headless 运行

对于没有显示器的产品，在设备树中禁用显示输出可加快启动并释放资源：

```bash
# Add to kernel command line
video=efifb:off
```

或者从载板设备树中移除与显示相关的节点。

---

## 8. Framebuffer 与 DRM/KMS

### DRM/KMS（现代方式）

```bash
# List DRM devices
ls /dev/dri/

# Check connected displays
sudo cat /sys/class/drm/card*/status

# Use modetest for direct display testing
sudo modetest -M nvidia-drm -s <connector>@<crtc>:<mode>
```

### Framebuffer（传统方式）

```bash
# Check framebuffer info
fbset -i

# Write test pattern
sudo apt install fbset
cat /dev/urandom > /dev/fb0   # random noise
```

---

## 9. 项目

- **多摄像头查看器：** 使用 GStreamer `nvcompositor` 在 HDMI 输出上并排显示 2 路 CSI 摄像头画面。
- **录像机：** 构建 GStreamer 流水线，将 CSI 摄像头的 H.264 视频录制到 NVMe，并叠加时间戳，由 GPIO 按键触发。
- **音频对讲：** 使用 GStreamer RTP 经以太网在两台 Jetson 设备之间传输音频。
- **ESP32-LyraT I2S 麦克风采集：** 将 LyraT 板用作 I2S 麦克风前端，通过 Jetson Orin Nano 40-pin `I2S2` 通路采集原始音频。
- **RTSP 摄像头服务器：** 将 CSI 摄像头画面作为 RTSP 流提供，可从任意网络设备的 VLC 访问。

---

## 10. 资源

| 资源 | 描述 |
|----------|-------------|
| **NVIDIA Multimedia API** | Jetson 多媒体框架文档（Argus、V4L2、GStreamer） |
| [Jetson Audio Setup and Development](02-Jetson音频设置与开发/Guide.md) | Jetson 专用的 ALSA、ASoC、AHUB、`APE`、设备树、引脚复用、codec bring-up |
| [ESP32-LyraT I2S Microphone Capture](01-ESP32-LyraT-I2S麦克风Jetson-Orin-Nano/Guide.md) | 以 LyraT 作为音频前端的 Jetson 40-pin `I2S2` 采集应用 |
| **GStreamer documentation** (gstreamer.freedesktop.org) | 流水线语法、插件参考 |
| **NVIDIA GStreamer plugins** | `nvarguscamerasrc`, `nvv4l2decoder`, `nvv4l2h264enc`, `nv3dsink` |
| **ALSA project** (alsa-project.org) | Advanced Linux Sound Architecture 文档 |
| [Orin Nano Camera ISP](../../01-Nvidia-Jetson平台/02-Orin-Nano摄像头ISP传感器启动/Guide.md) | CSI sensor bring-up、ISP 流水线、多摄像头（Module 1 深入阅读） |
| [Orin Nano Video Codec DeepStream](../../01-Nvidia-Jetson平台/15-Orin-Nano视频编解码DeepStream/Guide.md) | 硬件 codec 细节、DeepStream 集成（Module 1 深入阅读） |


<details>
<summary>English original</summary>

**5. USB cameras / webcams (UVC)**

Any UVC-compliant USB camera works out of the box.

```bash
# List video devices
v4l2-ctl --list-devices

# Check supported formats
v4l2-ctl -d /dev/video0 --list-formats-ext

# Capture a frame
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=MJPG \
    --stream-mmap --stream-count=1 --stream-to=frame.mjpg

# GStreamer pipeline (USB webcam → display)
gst-launch-1.0 v4l2src device=/dev/video0 ! \
    'video/x-raw,width=1280,height=720,framerate=30/1' ! \
    videoconvert ! autovideosink
```

---

**6. CSI cameras (MIPI)**

CSI cameras connect via the MIPI CSI-2 interface on the carrier board and are accessed through NVIDIA's **Argus** camera framework.

**Using nvarguscamerasrc (GStreamer)**

```bash
# Preview from CSI camera 0
gst-launch-1.0 nvarguscamerasrc sensor-id=0 ! \
    'video/x-raw(memory:NVMM),width=1920,height=1080,framerate=30/1' ! \
    nvvidconv ! nv3dsink

# Capture to JPEG
gst-launch-1.0 nvarguscamerasrc sensor-id=0 num-buffers=1 ! \
    'video/x-raw(memory:NVMM),width=1920,height=1080' ! \
    nvjpegenc ! filesink location=capture.jpg
```

**Using V4L2 directly**

```bash
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080 \
    --set-ctrl bypass_mode=0 --stream-mmap --stream-count=10
```

**Camera device tree configuration**

CSI cameras require device tree entries specifying the sensor driver, I2C address, MIPI lanes, and pixel format. Example for IMX219:

```dts
cam0: imx219@10 {
    compatible = "sony,imx219";
    reg = <0x10>;
    clocks = <&bpmp TEGRA234_CLK_EXTPERIPH1>;

    mode0 {
        mclk_khz = "24000";
        num_lanes = "2";
        tegra_sinterface = "serial_a";
        active_w = "3264";
        active_h = "2464";
        pixel_t = "bayer_rggb";
    };
};
```

> **Deep dive:** For full camera ISP, sensor bring-up, and multi-camera configurations see [Orin Nano Camera ISP Sensor Bring-Up](../../01-Nvidia-Jetson平台/02-Orin-Nano摄像头ISP传感器启动/Guide.md).

---

**7. Display output and resolution**

**HDMI/DisplayPort**

```bash
# List outputs and modes
xrandr

# Set specific mode
xrandr --output HDMI-0 --mode 1920x1080 --rate 60

# Force a custom mode
cvt 1280 800 60
xrandr --newmode "1280x800_60" ...
xrandr --addmode HDMI-0 "1280x800_60"
xrandr --output HDMI-0 --mode "1280x800_60"
```

**Headless operation**

For products without a display, disable display output in the device tree to speed up boot and free resources:

```bash
# Add to kernel command line
video=efifb:off
```

Or remove display-related nodes from the carrier device tree.

---

**8. Framebuffer and DRM/KMS**

**DRM/KMS (modern approach)**

```bash
# List DRM devices
ls /dev/dri/

# Check connected displays
sudo cat /sys/class/drm/card*/status

# Use modetest for direct display testing
sudo modetest -M nvidia-drm -s <connector>@<crtc>:<mode>
```

**Framebuffer (legacy)**

```bash
# Check framebuffer info
fbset -i

# Write test pattern
sudo apt install fbset
cat /dev/urandom > /dev/fb0   # random noise
```

---

**9. Projects**

- **Multi-camera viewer:** Display 2 CSI cameras side-by-side using GStreamer `nvcompositor` on HDMI output.
- **Video recorder:** Build a GStreamer pipeline that records H.264 video from a CSI camera to NVMe with timestamp overlay, triggered by GPIO button press.
- **Audio intercom:** Stream audio between two Jetson devices using GStreamer RTP over Ethernet.
- **ESP32-LyraT I2S mic capture:** Use a LyraT board as an I2S microphone frontend and capture raw audio through the Jetson Orin Nano 40-pin `I2S2` path.
- **RTSP camera server:** Serve a CSI camera feed as an RTSP stream accessible from VLC on any network device.

---

**10. Resources**

| Resource | Description |
|----------|-------------|
| **NVIDIA Multimedia API** | Jetson multimedia framework documentation (Argus, V4L2, GStreamer) |
| [Jetson Audio Setup and Development](02-Jetson音频设置与开发/Guide.md) | Jetson-specific ALSA, ASoC, AHUB, `APE`, device tree, pinmux, codec bring-up |
| [ESP32-LyraT I2S Microphone Capture](01-ESP32-LyraT-I2S麦克风Jetson-Orin-Nano/Guide.md) | Focused Jetson 40-pin `I2S2` capture application using LyraT as an audio frontend |
| **GStreamer documentation** (gstreamer.freedesktop.org) | Pipeline syntax, plugin reference |
| **NVIDIA GStreamer plugins** | `nvarguscamerasrc`, `nvv4l2decoder`, `nvv4l2h264enc`, `nv3dsink` |
| **ALSA project** (alsa-project.org) | Advanced Linux Sound Architecture documentation |
| [Orin Nano Camera ISP](../../01-Nvidia-Jetson平台/02-Orin-Nano摄像头ISP传感器启动/Guide.md) | CSI sensor bring-up, ISP pipeline, multi-camera (Module 1 deep dive) |
| [Orin Nano Video Codec DeepStream](../../01-Nvidia-Jetson平台/15-Orin-Nano视频编解码DeepStream/Guide.md) | Hardware codec details, DeepStream integration (Module 1 deep dive) |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/4. Multimedia/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/4.%20Multimedia/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
