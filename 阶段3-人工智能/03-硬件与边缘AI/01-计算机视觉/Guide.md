---
title: 计算机视觉 —— 完整指南
description: 计算机视觉 —— 完整指南
---

# 计算机视觉 —— 完整指南

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">CVCG</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度解析 · AI 工作负载</p>
<p class="course-identity__title">《计算机视觉 —— 完整指南》的专用课程标识。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


> **目标：** 从图像处理基础到现代深度学习架构，全面理解计算机视觉，并掌握标注工具、数据集格式以及边缘硬件部署方面的实用技能。

---


## 1. 什么是计算机视觉？

计算机视觉让机器具备解读和理解来自世界的视觉信息的能力 —— 图像、视频、深度图、点云。

```
Input: raw pixels / point cloud / depth frame
         ↓
  Preprocessing (resize, normalize, augment)
         ↓
  Feature extraction (learned or hand-crafted)
         ↓
  Task-specific head (detection / segmentation / depth)
         ↓
Output: bounding boxes / masks / pose / 3D structure
```

### 核心任务

| 任务 | 输入 | 输出 | 示例 |
|------|-------|--------|---------|
| 分类 | 图像 | 类别标签 | “这是一只猫” |
| 目标检测 | 图像 | 框 + 标签 | “车位于 (x1,y1,x2,y2)” |
| 语义分割 | 图像 | 逐像素类别 | 每个像素 = 道路/车/天空 |
| 实例分割 | 图像 | 逐对象掩码 | 每辆车有自己的掩码 |
| 深度估计 | RGB | 深度图 | 每个像素的距离 |
| 姿态估计 | 图像 | 关键点 | 一个人的骨架 |
| 3D 检测 | 点云 | 3D 框 | LiDAR 目标检测 |
| 光流 | 视频 | 运动场 | 帧之间的像素位移 |

### 计算机视觉为何困难

```
Challenges:
  Viewpoint variation    — same object, different camera angle
  Illumination           — shadows, overexposure, nighttime
  Occlusion              — object partly hidden by another
  Scale variation        — same object at different distances
  Intra-class variation  — all chairs look different
  Background clutter     — object blends into background
  Deformable objects     — humans, animals change shape
```

---

## 2. 图像处理基础

### 2.0 ISP 流水线 —— 从光子到像素

初学者以为：“相机给出图像。”现实是：相机给出的是原始物理量 —— 图像由 ISP 构建。

神经网络处理的每一张图像都是由 ISP（图像信号处理器）制造出来的。传感器并不输出 RGB。它输出的是 10/12/14-bit 位深下单通道光子计数的 Bayer 马赛克。ISP 流水线把这一原始物理量转换成稳定、色彩正确的图像。每个阶段都因物理原因而存在，而且任何阶段做错了都会破坏你的模型。

这些是 ADAS、嵌入式视觉、Jetson 和相机系统的核心知识。

#### 完整的 RAW 到 NV12 流水线

```
Photons → Lens → Sensor (CCD/CMOS with Bayer CFA)
                        │
                        │  raw Bayer mosaic (10/12/14 bit)
                        ▼
              ┌───────────────────────────┐
              │  1. RAW Bayer             │  sensor output — NOT an image yet
              │  2. Linearization         │  fix sensor non-linearity
              │  3. White Balance         │  correct for lighting color
              │  4. Demosaic              │  Bayer → full RGB
              │  5. Color Correction (CCM)│  sensor RGB → standard sRGB
              │  6. Brightness / Contrast │  tone mapping, gamma
              │  7. Lens Shading          │  fix vignetting (dark edges)
              │  8. NV12 output           │  YUV for GPU / neural net
              └───────────────────────────┘
```

---


<details>
<summary>English original</summary>

**Computer Vision — Complete Guide**

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">CVCG</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for Computer Vision — Complete Guide.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


> **Goal:** Build a comprehensive understanding of computer vision from image processing fundamentals through modern deep learning architectures, with practical skills in annotation tools, dataset formats, and deployment on edge hardware.

---


**1. What is Computer Vision?**

Computer vision gives machines the ability to interpret and understand visual information from the world — images, videos, depth maps, point clouds.

```
Input: raw pixels / point cloud / depth frame
         ↓
  Preprocessing (resize, normalize, augment)
         ↓
  Feature extraction (learned or hand-crafted)
         ↓
  Task-specific head (detection / segmentation / depth)
         ↓
Output: bounding boxes / masks / pose / 3D structure
```

**Core Tasks**

| Task | Input | Output | Example |
|------|-------|--------|---------|
| Classification | Image | Class label | "this is a cat" |
| Object Detection | Image | Boxes + labels | "car at (x1,y1,x2,y2)" |
| Semantic Segmentation | Image | Per-pixel class | every pixel = road/car/sky |
| Instance Segmentation | Image | Per-object mask | each car gets its own mask |
| Depth Estimation | RGB | Depth map | distance per pixel |
| Pose Estimation | Image | Keypoints | skeleton of a person |
| 3D Detection | Point Cloud | 3D boxes | LiDAR object detection |
| Optical Flow | Video | Motion field | pixel displacement between frames |

**Why Computer Vision is Hard**

```
Challenges:
  Viewpoint variation    — same object, different camera angle
  Illumination           — shadows, overexposure, nighttime
  Occlusion              — object partly hidden by another
  Scale variation        — same object at different distances
  Intra-class variation  — all chairs look different
  Background clutter     — object blends into background
  Deformable objects     — humans, animals change shape
```

---

**2. Image Processing Fundamentals**

**2.0 ISP Pipeline — From Photons to Pixels**

Beginners think: "the camera gives an image." Reality: the camera gives raw physics — the ISP builds the image.

Every image a neural network processes was manufactured by an Image Signal Processor. The sensor does not output RGB. It outputs a Bayer mosaic of single-channel photon counts at 10/12/14-bit depth. The ISP pipeline transforms this raw physics into a stable, color-correct image. Every stage exists for a physical reason, and every stage can break your model if done wrong.

This is core knowledge for ADAS, embedded vision, Jetson, and camera systems.

**Full RAW-to-NV12 Pipeline**

```
Photons → Lens → Sensor (CCD/CMOS with Bayer CFA)
                        │
                        │  raw Bayer mosaic (10/12/14 bit)
                        ▼
              ┌───────────────────────────┐
              │  1. RAW Bayer             │  sensor output — NOT an image yet
              │  2. Linearization         │  fix sensor non-linearity
              │  3. White Balance         │  correct for lighting color
              │  4. Demosaic              │  Bayer → full RGB
              │  5. Color Correction (CCM)│  sensor RGB → standard sRGB
              │  6. Brightness / Contrast │  tone mapping, gamma
              │  7. Lens Shading          │  fix vignetting (dark edges)
              │  8. NV12 output           │  YUV for GPU / neural net
              └───────────────────────────┘
```

---

</details>

#### 阶段 1：RAW Bayer（Sensor 输出）

Sensor 并不采集 RGB。

它采集的是 **Bayer mosaic pattern**——每个物理像素前面都有一个滤色片，只让一种颜色通过。

```
Bayer RGGB pattern (most common):

  col 0   col 1   col 2   col 3
  ┌───────┬───────┬───────┬───────┐
  │   R   │   G   │   R   │   G   │  row 0
  ├───────┼───────┼───────┼───────┤
  │   G   │   B   │   G   │   B   │  row 1
  ├───────┼───────┼───────┼───────┤
  │   R   │   G   │   R   │   G   │  row 2
  ├───────┼───────┼───────┼───────┤
  │   G   │   B   │   G   │   B   │  row 3
  └───────┴───────┴───────┴───────┘

Each pixel records ONLY ONE color channel.
50% green — human vision is most sensitive to green, so more green pixels
            maximizes signal-to-noise ratio.
25% red, 25% blue.
```

为什么这样设计？

- 硬件简单——每个光电二极管一个滤色片，无需分光器
- 带宽更小——每像素 1 个通道，而不是 3 个
- 全行业通用——Bayer pattern 发明于 1976 年，至今仍是标准

问题在于：

- 这并非一张可用的图像
- 神经网络无法直接摄取 raw Bayer（结构不对，单通道）
- 每一个下游阶段的存在，都是为了把它变成可用的东西

```
CCD vs CMOS:

  CCD  — Charge Coupled Device
         Shifts charge row-by-row to a single ADC at the edge
         Global shutter (all pixels exposed simultaneously — no rolling artifacts)
         Higher power, mostly legacy (replaced by CMOS)

  CMOS — Complementary Metal-Oxide-Semiconductor
         Each pixel has its own amplifier + ADC (column-parallel readout)
         Rolling shutter (rows read sequentially — fast motion causes skew)
         Lower power, faster, dominant in ALL modern cameras

ADAS reference sensors:
  AR0233AT (ON Semi)    — 2.1 MP, HDR, automotive qualified, RGGB
  OV8856  (OmniVision)  — 8 MP, openpilot comma 3 sensor, RGGB
  IMX390  (Sony)        — 2.3 MP, HDR, LED flicker mitigation, ADAS standard
  IMX728  (Sony)        — 8.3 MP, latest generation ADAS, stacked BSI
```

---

#### 阶段 2：线性化

Sensor 是非线性器件。从 ADC 输出的原始像素值，并不与打到该像素上的光子数成正比。

```
Why non-linear?

  Photodiode response      — charge accumulates non-linearly near saturation
  ADC transfer function    — analog-to-digital converter has its own curve
  Sensor gain stages       — programmable gain amplifiers introduce distortion

What linearization does:

  Apply a correction curve (measured during factory calibration)
  so that:  output_value = k × (number of photons)

  After linearization: double the light → double the pixel value
```

为什么这很重要：

- 后续所有数学运算（白平衡、颜色校正、降噪）都假定光是线性的
- 不做线性化：颜色错、亮度错、曝光控制失效
- 校正曲线是每个 sensor 型号特有的（存放在 sensor 固件或 ISP 配置中）

```
Before linearization:
  200 photons → ADC reads 1000
  400 photons → ADC reads 1700    ← should be 2000 if linear

After linearization:
  200 photons → corrected to 1000
  400 photons → corrected to 2000  ← now proportional
```

---


<details>
<summary>English original</summary>

**Stage 1: RAW Bayer (Sensor Output)**

The sensor does NOT capture RGB.

It captures a **Bayer mosaic pattern** — each physical pixel has a color filter in front of it that passes only one color.

```
Bayer RGGB pattern (most common):

  col 0   col 1   col 2   col 3
  ┌───────┬───────┬───────┬───────┐
  │   R   │   G   │   R   │   G   │  row 0
  ├───────┼───────┼───────┼───────┤
  │   G   │   B   │   G   │   B   │  row 1
  ├───────┼───────┼───────┼───────┤
  │   R   │   G   │   R   │   G   │  row 2
  ├───────┼───────┼───────┼───────┤
  │   G   │   B   │   G   │   B   │  row 3
  └───────┴───────┴───────┴───────┘

Each pixel records ONLY ONE color channel.
50% green — human vision is most sensitive to green, so more green pixels
            maximizes signal-to-noise ratio.
25% red, 25% blue.
```

Why this design?

- Hardware simplicity — one filter per photodiode, no beam splitters
- Smaller bandwidth — 1 channel per pixel instead of 3
- Standard across the industry — Bayer pattern invented 1976, still universal

The problem:

- This is NOT a usable image
- A neural network cannot ingest raw Bayer directly (wrong structure, single-channel)
- Every downstream stage exists to turn this into something usable

```
CCD vs CMOS:

  CCD  — Charge Coupled Device
         Shifts charge row-by-row to a single ADC at the edge
         Global shutter (all pixels exposed simultaneously — no rolling artifacts)
         Higher power, mostly legacy (replaced by CMOS)

  CMOS — Complementary Metal-Oxide-Semiconductor
         Each pixel has its own amplifier + ADC (column-parallel readout)
         Rolling shutter (rows read sequentially — fast motion causes skew)
         Lower power, faster, dominant in ALL modern cameras

ADAS reference sensors:
  AR0233AT (ON Semi)    — 2.1 MP, HDR, automotive qualified, RGGB
  OV8856  (OmniVision)  — 8 MP, openpilot comma 3 sensor, RGGB
  IMX390  (Sony)        — 2.3 MP, HDR, LED flicker mitigation, ADAS standard
  IMX728  (Sony)        — 8.3 MP, latest generation ADAS, stacked BSI
```

---

**Stage 2: Linearization**

Sensors are non-linear devices. The raw pixel value coming out of the ADC is NOT proportional to the number of photons that hit the pixel.

```
Why non-linear?

  Photodiode response      — charge accumulates non-linearly near saturation
  ADC transfer function    — analog-to-digital converter has its own curve
  Sensor gain stages       — programmable gain amplifiers introduce distortion

What linearization does:

  Apply a correction curve (measured during factory calibration)
  so that:  output_value = k × (number of photons)

  After linearization: double the light → double the pixel value
```

Why this matters:

- ALL subsequent math (white balance, color correction, noise reduction) assumes linear light
- Without linearization: colors are wrong, brightness is wrong, exposure control breaks
- The correction curve is specific to each sensor model (stored in sensor firmware or ISP config)

```
Before linearization:
  200 photons → ADC reads 1000
  400 photons → ADC reads 1700    ← should be 2000 if linear

After linearization:
  200 photons → corrected to 1000
  400 photons → corrected to 2000  ← now proportional
```

---

</details>

#### 阶段 3：白平衡（对 ADAS 至关重要）

真实世界的光照并非中性。不同光源发出不同色温的光，会改变场景中一切物体的颜色。

```
Color temperature of common illuminants:

  1800 K    candle flame            very warm / orange
  2700 K    tungsten bulb           warm yellow
  4000 K    fluorescent tube        neutral / slightly green
  5500 K    direct sunlight (D55)   daylight standard
  6500 K    overcast sky (D65)      cool / slightly blue — sRGB reference
 10000 K    blue sky / deep shade   very cool / blue
```

传感器的 R、G、B 通道对每种光源的响应各不相同。钨丝灯下红色通道过强；阴影下蓝色通道过强。

白平衡通过用增益系数缩放每个通道来校正这一点：

```
How it works:

  Measure the scene illuminant (auto white balance algorithms):
    - Gray World:    assume average scene color should be neutral gray
    - White Patch:   assume brightest region is white
    - Illuminant estimation: statistical model of light source

  Apply per-channel gain:
    R_corrected = R_raw × gain_R
    G_corrected = G_raw × gain_G     (usually gain_G = 1.0, reference channel)
    B_corrected = B_raw × gain_B

  Goal: a white object appears as (R=G=B) regardless of lighting
```

为什么这对 ADAS 至关重要：

- 行车记录仪面对不断变化的光源：阳光、阴影、隧道、LED 尾灯、路灯
- 糟糕的 AWB 会导致帧间颜色漂移
- 在 D65（阴天日光）图像上训练的神经网络，当 ISP 输出偏橙色的隧道帧时会误分类物体
- 颜色漂移 → 类别混淆 → 漏检

```
ADAS scenario:

  Highway → tunnel → exit:
    Frame 1: outdoor sunlight (5500K) → correct colors
    Frame 2: tunnel sodium lamps (2000K) → everything orange
    Frame 3: tunnel exit (mixed) → half orange, half blue

  Without good AWB: the neural net sees three different "worlds"
  With good AWB:    all three frames have consistent color → stable predictions
```

---

#### 阶段 4：去马赛克（最重要的 ISP 阶段）

raw Bayer 图像的颜色信息不完整。每个像素只记录了 ONE 个通道（R、G 或 B）。去马赛克为每个像素重建缺失的两个通道。

```
Before demosaic:                  After demosaic:

  pixel (2,3) has only G value    pixel (2,3) has (R_interpolated,
                                                    G_actual,
                                                    B_interpolated)

How interpolation works (simplified — bilinear):

  To estimate the missing Red value at a Green pixel:
    R_estimated = average of neighboring R pixels (above, below, left, right)

  To estimate the missing Blue value at a Green pixel:
    B_estimated = average of neighboring B pixels (diagonal neighbors)
```

去马赛克算法的质量参差不齐：

```
Method           Speed      Quality    Artifacts
──────────────────────────────────────────────────────────
Bilinear         fastest    poor       zipper edges, moiré, false color
VNG              medium     good       mild color fringing at sharp edges
AHD / EA         slower     very good  edge-aware — interpolates ALONG edges,
                                       not across them
Neural network   GPU        excellent  learned from ground-truth RGB pairs
```

为什么质量影响巨大：

- 糟糕的去马赛克会在锐利边缘产生 **zipper artifacts**（交替出现的彩色边纹）
- 这些伪影在 CNN 看来像真实特征
- 摩尔纹（虚假的周期性图案）会触发误检
- 每个下游 CV 任务都会继承去马赛克误差——它们会永久传播下去

```
ADAS tradeoff:

  Accuracy:  want AHD or neural-net demosaic (best edges)
  Speed:     need real-time (30 fps minimum)
  Solution:  hardware ISP does demosaic in dedicated silicon at pixel rate
             — no CPU/GPU cycles consumed, runs at sensor speed

Hardware ISP demosaicers:
  NVIDIA Jetson:    built into VI/ISP unit (not user-configurable)
  Qualcomm Spectra: multi-pass with chroma correction
  ARM Mali-C71:     proprietary edge-directed algorithm
```

---


<details>
<summary>English original</summary>

**Stage 3: White Balance (Critical for ADAS)**

Real-world lighting is not neutral. Different light sources emit different color temperatures, which shifts the color of everything in the scene.

```
Color temperature of common illuminants:

  1800 K    candle flame            very warm / orange
  2700 K    tungsten bulb           warm yellow
  4000 K    fluorescent tube        neutral / slightly green
  5500 K    direct sunlight (D55)   daylight standard
  6500 K    overcast sky (D65)      cool / slightly blue — sRGB reference
 10000 K    blue sky / deep shade   very cool / blue
```

The sensor's R, G, B channels respond differently to each illuminant. Under tungsten light, red channel is too strong; under shade, blue is too strong.

White balance corrects this by scaling each channel with a gain factor:

```
How it works:

  Measure the scene illuminant (auto white balance algorithms):
    - Gray World:    assume average scene color should be neutral gray
    - White Patch:   assume brightest region is white
    - Illuminant estimation: statistical model of light source

  Apply per-channel gain:
    R_corrected = R_raw × gain_R
    G_corrected = G_raw × gain_G     (usually gain_G = 1.0, reference channel)
    B_corrected = B_raw × gain_B

  Goal: a white object appears as (R=G=B) regardless of lighting
```

Why this is critical for ADAS:

- A dashcam sees constantly changing illuminants: sun, shadows, tunnels, LED taillights, streetlights
- Bad AWB causes color shifts between frames
- A neural net trained on D65 (overcast daylight) images will misclassify objects when the ISP outputs orange-tinted tunnel frames
- Color shift → class confusion → missed detections

```
ADAS scenario:

  Highway → tunnel → exit:
    Frame 1: outdoor sunlight (5500K) → correct colors
    Frame 2: tunnel sodium lamps (2000K) → everything orange
    Frame 3: tunnel exit (mixed) → half orange, half blue

  Without good AWB: the neural net sees three different "worlds"
  With good AWB:    all three frames have consistent color → stable predictions
```

---

**Stage 4: Demosaic (Most Important ISP Stage)**

The raw Bayer image has incomplete color information. Each pixel recorded only ONE channel (R, G, or B). Demosaicing reconstructs the missing two channels for every pixel.

```
Before demosaic:                  After demosaic:

  pixel (2,3) has only G value    pixel (2,3) has (R_interpolated,
                                                    G_actual,
                                                    B_interpolated)

How interpolation works (simplified — bilinear):

  To estimate the missing Red value at a Green pixel:
    R_estimated = average of neighboring R pixels (above, below, left, right)

  To estimate the missing Blue value at a Green pixel:
    B_estimated = average of neighboring B pixels (diagonal neighbors)
```

Demosaicing algorithms range in quality:

```
Method           Speed      Quality    Artifacts
──────────────────────────────────────────────────────────
Bilinear         fastest    poor       zipper edges, moiré, false color
VNG              medium     good       mild color fringing at sharp edges
AHD / EA         slower     very good  edge-aware — interpolates ALONG edges,
                                       not across them
Neural network   GPU        excellent  learned from ground-truth RGB pairs
```

Why quality matters enormously:

- Bad demosaicing creates **zipper artifacts** at sharp edges (alternating color fringes)
- These artifacts look like real features to a CNN
- Moiré patterns (false periodic patterns) trigger false detections
- Every downstream CV task inherits demosaic errors — they propagate forever

```
ADAS tradeoff:

  Accuracy:  want AHD or neural-net demosaic (best edges)
  Speed:     need real-time (30 fps minimum)
  Solution:  hardware ISP does demosaic in dedicated silicon at pixel rate
             — no CPU/GPU cycles consumed, runs at sensor speed

Hardware ISP demosaicers:
  NVIDIA Jetson:    built into VI/ISP unit (not user-configurable)
  Qualcomm Spectra: multi-pass with chroma correction
  ARM Mali-C71:     proprietary edge-directed algorithm
```

---

</details>

#### 阶段 5：色彩空间校正（CCM）

传感器的滤色片并非理想的 R、G、B。每种传感器型号都有独特的光谱响应 —— 红色滤光片会透过部分绿光，蓝色滤光片会漏过部分红光，等等。

**Color Correction Matrix (CCM)** 将传感器原生 RGB 变换到标准色彩空间（sRGB）。

```
What the CCM does:

  [R_sRGB]     [  1.86  -0.64  -0.21 ]   [R_sensor]
  [G_sRGB]  =  [ -0.25   1.52  -0.26 ] × [G_sensor]
  [B_sRGB]     [  0.04  -0.57   1.53 ]   [B_sensor]

  This 3×3 matrix corrects:
    - Spectral cross-talk between filter channels
    - Sensitivity differences between R/G/B photodiodes
    - Maps sensor-specific colors to device-independent standard

The full color chain:
  Sensor RGB → CCM → CIE XYZ → matrix → linear sRGB → gamma curve → sRGB
  (device-      (device-          (physical     (perceptual /
   dependent)    independent)      light values)  display-ready)
```

对神经网络的意义：

- 在 sRGB 图像上训练的 NN 学到的是特定的颜色统计特性
- 若相机的 CCM 与训练数据不同，颜色分布就会偏移
- 颜色偏移 → 颜色敏感任务（红绿灯、车道颜色、刹车灯）上准确率下降
- 解决方案：要么校准 CCM 使其匹配训练分布，要么用多样化的 ISP 输出做训练数据增强

---

#### 阶段 6：亮度与对比度控制

色彩校正之后，图像处于线性光下 —— 但线性光在人眼看来、以及在用 gamma 校正图像训练的模型看来都不对。

```
Why we need this stage:

  Linear light reality:
    - Most of the useful scene detail is compressed into a tiny range
    - Dark areas look too dark, bright areas look too bright
    - Real scenes have 120+ dB dynamic range (HDR sensors)
    - Display/JPEG supports only ~48 dB (8 bits)

What this stage does:

  1. Tone mapping (HDR → SDR)
     Compress the enormous dynamic range into 8 bits
     Preserve detail in both shadows and highlights
     Critical for dashcams: headlights + dark road in same frame

  2. Gamma correction
     Apply power curve: output = input^(1/2.2)
     Redistributes bit depth to match human perception
     Without gamma: dark regions have too few distinct values

  3. Contrast adjustment
     Local or global contrast enhancement
     Prevents losing lane markings in shadows
     Prevents saturating sky / headlight regions
```

ADAS 特有的关注点：

```
  Problem: driving toward sunset
    - Sky: massively overexposed (saturated white)
    - Road: deeply underexposed (near black)
    - Lane markings: invisible in both regions

  ISP tone mapper must:
    - Compress sky brightness so headlights don't bloom
    - Lift road shadows so lane markings remain visible
    - Do this consistently frame-to-frame (no flickering)

  Bad tone mapping → lost lane markings → lateral control fails
  Good tone mapping → neural net sees lanes in all conditions
```

---

#### 阶段 7：镜头阴影 / 渐晕校正

每个镜头在光学上都不完美。光线向图像边缘逐渐衰减 —— 中心比四角更亮。这称为**渐晕**。

```
Why it happens:

  - Cos⁴ law: light intensity drops as cos⁴(angle) from optical axis
  - At 30° off-axis: brightness drops to ~56% of center
  - At 45° off-axis: brightness drops to ~25% of center
  - Worse with wide-angle lenses (ADAS cameras are typically 60–120° FOV)

What the correction does:

  Apply a per-pixel gain map (measured during lens calibration):
    pixel_corrected = pixel_raw × gain_map(x, y)

  The gain map is:
    - ~1.0 at center (no correction needed)
    - ~1.5–2.5 at edges (amplify to compensate for falloff)
    - Unique per lens + sensor combination
    - Stored in ISP firmware
```

对 ADAS 的意义：

```
  Without lens shading correction:
    - Objects at image edges appear darker than center
    - A pedestrian in the left corner looks different from same pedestrian at center
    - Segmentation models produce inconsistent masks at image borders
    - Lane detection fails at peripheral lanes (they appear too dim)

  Especially critical for:
    - Wide-angle cameras (openpilot wide cam: ~120° FOV → severe vignetting)
    - Multi-camera stitching (brightness mismatch at overlap zones)
    - Surround-view systems (4+ cameras with edge overlap)
```

---


<details>
<summary>English original</summary>

**Stage 5: Color Space Correction (CCM)**

Sensor color filters are NOT ideal R, G, B. Each sensor model has a unique spectral response — the red filter passes some green, the blue filter leaks some red, etc.

The **Color Correction Matrix (CCM)** transforms sensor-native RGB into a standard color space (sRGB).

```
What the CCM does:

  [R_sRGB]     [  1.86  -0.64  -0.21 ]   [R_sensor]
  [G_sRGB]  =  [ -0.25   1.52  -0.26 ] × [G_sensor]
  [B_sRGB]     [  0.04  -0.57   1.53 ]   [B_sensor]

  This 3×3 matrix corrects:
    - Spectral cross-talk between filter channels
    - Sensitivity differences between R/G/B photodiodes
    - Maps sensor-specific colors to device-independent standard

The full color chain:
  Sensor RGB → CCM → CIE XYZ → matrix → linear sRGB → gamma curve → sRGB
  (device-      (device-          (physical     (perceptual /
   dependent)    independent)      light values)  display-ready)
```

Why this matters for neural networks:

- NNs trained on sRGB images learn specific color statistics
- If your camera's CCM differs from training data, color distribution shifts
- Shifted colors → degraded accuracy on color-sensitive tasks (traffic light, lane color, brake lights)
- Solution: either calibrate CCM to match training distribution, or augment training with diverse ISP outputs

---

**Stage 6: Brightness and Contrast Control**

After color correction, the image is in linear light — but linear light looks wrong to human eyes and to models trained on gamma-corrected images.

```
Why we need this stage:

  Linear light reality:
    - Most of the useful scene detail is compressed into a tiny range
    - Dark areas look too dark, bright areas look too bright
    - Real scenes have 120+ dB dynamic range (HDR sensors)
    - Display/JPEG supports only ~48 dB (8 bits)

What this stage does:

  1. Tone mapping (HDR → SDR)
     Compress the enormous dynamic range into 8 bits
     Preserve detail in both shadows and highlights
     Critical for dashcams: headlights + dark road in same frame

  2. Gamma correction
     Apply power curve: output = input^(1/2.2)
     Redistributes bit depth to match human perception
     Without gamma: dark regions have too few distinct values

  3. Contrast adjustment
     Local or global contrast enhancement
     Prevents losing lane markings in shadows
     Prevents saturating sky / headlight regions
```

ADAS-specific concerns:

```
  Problem: driving toward sunset
    - Sky: massively overexposed (saturated white)
    - Road: deeply underexposed (near black)
    - Lane markings: invisible in both regions

  ISP tone mapper must:
    - Compress sky brightness so headlights don't bloom
    - Lift road shadows so lane markings remain visible
    - Do this consistently frame-to-frame (no flickering)

  Bad tone mapping → lost lane markings → lateral control fails
  Good tone mapping → neural net sees lanes in all conditions
```

---

**Stage 7: Lens Shading / Vignetting Correction**

Every lens is optically imperfect. Light falls off toward the edges of the image — the center is brighter than the corners. This is called **vignetting**.

```
Why it happens:

  - Cos⁴ law: light intensity drops as cos⁴(angle) from optical axis
  - At 30° off-axis: brightness drops to ~56% of center
  - At 45° off-axis: brightness drops to ~25% of center
  - Worse with wide-angle lenses (ADAS cameras are typically 60–120° FOV)

What the correction does:

  Apply a per-pixel gain map (measured during lens calibration):
    pixel_corrected = pixel_raw × gain_map(x, y)

  The gain map is:
    - ~1.0 at center (no correction needed)
    - ~1.5–2.5 at edges (amplify to compensate for falloff)
    - Unique per lens + sensor combination
    - Stored in ISP firmware
```

Why it matters for ADAS:

```
  Without lens shading correction:
    - Objects at image edges appear darker than center
    - A pedestrian in the left corner looks different from same pedestrian at center
    - Segmentation models produce inconsistent masks at image borders
    - Lane detection fails at peripheral lanes (they appear too dim)

  Especially critical for:
    - Wide-angle cameras (openpilot wide cam: ~120° FOV → severe vignetting)
    - Multi-camera stitching (brightness mismatch at overlap zones)
    - Surround-view systems (4+ cameras with edge overlap)
```

---

</details>

#### 阶段 8：NV12 输出（最终格式）

ISP 的最后一步将处理后的 RGB 图像转换为 **NV12**（一种 YUV 4:2:0 格式），供 GPU、神经网络和视频编码器下游使用。

```
What is NV12?

  Y plane:   full resolution luminance
             every pixel has its own brightness value
             size: H × W bytes

  UV plane:  half-resolution chrominance (interleaved U, V)
             shared between 2×2 pixel blocks
             size: H/2 × W bytes

  Total size: H × W × 1.5 bytes (vs H × W × 3 for RGB)
             → 50% smaller than RGB

NV12 memory layout for a 4×4 image:

  Y plane (4×4):        UV plane (2×4, interleaved):
  ┌───┬───┬───┬───┐     ┌───┬───┬───┬───┐
  │Y00│Y01│Y02│Y03│     │U00│V00│U01│V01│  ← shared by top 2 rows
  ├───┼───┼───┼───┤     ├───┼───┼───┼───┤
  │Y10│Y11│Y12│Y13│     │U10│V10│U11│V11│  ← shared by bottom 2 rows
  ├───┼───┼───┼───┤     └───┴───┴───┴───┘
  │Y20│Y21│Y22│Y23│
  ├───┼───┼───┼───┤
  │Y30│Y31│Y32│Y33│
  └───┴───┴───┴───┘
```

为何用 NV12 而不是 RGB？

```
  Bandwidth:      NV12 is 50% smaller → saves memory bus bandwidth
  GPU friendly:   NVIDIA hardware texture units read NV12 natively
  ISP native:     hardware ISPs output NV12 directly (no conversion needed)
  Neural net:     openpilot supercombo takes YUV input directly
  Video encode:   H.264/H.265 encoders expect YUV420 input
  Cache friendly:  Y plane accessed sequentially → good cache behavior

  RGB is expensive:
    3 bytes per pixel, 3× the bandwidth
    Conversion from NV12→RGB costs CPU/GPU cycles
    Most inference pipelines avoid RGB entirely
```

```
Typical ADAS SoC data path:

  Sensor → MIPI CSI-2 → Hardware ISP → NV12 in DRAM
                                         │
                                         ├──→ GPU/DLA (inference)
                                         ├──→ H.265 encoder (recording)
                                         └──→ Display (preview)

  The ISP output goes everywhere. No CPU touches it.
  This is why ISP quality directly determines system quality.
```

---

#### 每个阶段为何存在（总结）

每个阶段都修正一个特定的物理现实。跳过任何一个，图像都会以特定且可预测的方式劣化：

```
  Stage                Fixes                        If wrong, neural net sees...
  ──────────────────────────────────────────────────────────────────────────────
  Linearization        sensor physics               wrong brightness relationships
  White Balance        lighting color variation      color-shifted objects (blue cars, yellow roads)
  Demosaic             Bayer sampling limitation     zipper edges, moiré false patterns
  CCM                  sensor spectral mismatch      wrong colors (red looks orange)
  Brightness/Contrast  scene dynamic range           crushed shadows, blown highlights
  Lens Shading         optical brightness falloff    dark edges, inconsistent features
  NV12 conversion      compute efficiency            (correct by construction)
```

#### ADAS / 嵌入式 AI 视角

神经网络需要：

- 帧间亮度稳定
- 不同光照条件下颜色稳定
- 对比度稳定（无闪烁曝光）
- 暗区噪声最小

ISP 保证这些特性。它是感知的 **输入契约**。

```
  Bad ISP + good neural net  →  unreliable perception  ✗
  Good ISP + good neural net →  robust perception      ✓

  The ISP is not optional. It is the foundation.
```

#### Jetson / 嵌入式关联

在每个 ADAS SoC 上，ISP 都是专用硬件模块——而非软件。

```
  Platform              ISP Block            Notes
  ─────────────────────────────────────────────────────────────
  NVIDIA Jetson Orin    VI/ISP unit          12-bit, HDR, up to 16 cameras,
                                             Bayer → NV12, accessed via libargus
  Qualcomm Snapdragon   Spectra ISP          14-bit, triple ISP (3 cameras),
                                             HDR, multi-frame noise reduction
  Texas Instruments     VPAC                 12-bit, lens distortion correction,
    TDA4                                     DMA to C7x DSP
  Xilinx Zynq (KV260)  Soft ISP on PL       implement in HLS — full control
  Mobileye EyeQ        Integrated ISP       proprietary, not configurable

  Jetson data path:
    Sensor → MIPI CSI-2 → VI → ISP → NV12 → NVMM buffer
                                              │
                                              ├──→ CUDA / TensorRT (inference)
                                              ├──→ NVENC (H.265 recording)
                                              └──→ nvvidconv (display)

  Same physics, dedicated silicon. Zero CPU involvement.
```

---


<details>
<summary>English original</summary>

**Stage 8: NV12 Output (Final Format)**

The ISP's final step converts the processed RGB image into **NV12** (a YUV 4:2:0 format) for downstream consumption by GPU, neural network, and video encoder.

```
What is NV12?

  Y plane:   full resolution luminance
             every pixel has its own brightness value
             size: H × W bytes

  UV plane:  half-resolution chrominance (interleaved U, V)
             shared between 2×2 pixel blocks
             size: H/2 × W bytes

  Total size: H × W × 1.5 bytes (vs H × W × 3 for RGB)
             → 50% smaller than RGB

NV12 memory layout for a 4×4 image:

  Y plane (4×4):        UV plane (2×4, interleaved):
  ┌───┬───┬───┬───┐     ┌───┬───┬───┬───┐
  │Y00│Y01│Y02│Y03│     │U00│V00│U01│V01│  ← shared by top 2 rows
  ├───┼───┼───┼───┤     ├───┼───┼───┼───┤
  │Y10│Y11│Y12│Y13│     │U10│V10│U11│V11│  ← shared by bottom 2 rows
  ├───┼───┼───┼───┤     └───┴───┴───┴───┘
  │Y20│Y21│Y22│Y23│
  ├───┼───┼───┼───┤
  │Y30│Y31│Y32│Y33│
  └───┴───┴───┴───┘
```

Why NV12 and not RGB?

```
  Bandwidth:      NV12 is 50% smaller → saves memory bus bandwidth
  GPU friendly:   NVIDIA hardware texture units read NV12 natively
  ISP native:     hardware ISPs output NV12 directly (no conversion needed)
  Neural net:     openpilot supercombo takes YUV input directly
  Video encode:   H.264/H.265 encoders expect YUV420 input
  Cache friendly:  Y plane accessed sequentially → good cache behavior

  RGB is expensive:
    3 bytes per pixel, 3× the bandwidth
    Conversion from NV12→RGB costs CPU/GPU cycles
    Most inference pipelines avoid RGB entirely
```

```
Typical ADAS SoC data path:

  Sensor → MIPI CSI-2 → Hardware ISP → NV12 in DRAM
                                         │
                                         ├──→ GPU/DLA (inference)
                                         ├──→ H.265 encoder (recording)
                                         └──→ Display (preview)

  The ISP output goes everywhere. No CPU touches it.
  This is why ISP quality directly determines system quality.
```

---

**Why Each Stage Exists (Summary)**

Every stage corrects a specific physical reality. Skip any one and the image degrades in a specific, predictable way:

```
  Stage                Fixes                        If wrong, neural net sees...
  ──────────────────────────────────────────────────────────────────────────────
  Linearization        sensor physics               wrong brightness relationships
  White Balance        lighting color variation      color-shifted objects (blue cars, yellow roads)
  Demosaic             Bayer sampling limitation     zipper edges, moiré false patterns
  CCM                  sensor spectral mismatch      wrong colors (red looks orange)
  Brightness/Contrast  scene dynamic range           crushed shadows, blown highlights
  Lens Shading         optical brightness falloff    dark edges, inconsistent features
  NV12 conversion      compute efficiency            (correct by construction)
```

**ADAS / Embedded AI Perspective**

Neural networks require:

- Stable brightness frame-to-frame
- Stable colors across lighting conditions
- Stable contrast (no flickering exposure)
- Minimal noise in dark regions

The ISP guarantees these properties. It is the **input contract** for perception.

```
  Bad ISP + good neural net  →  unreliable perception  ✗
  Good ISP + good neural net →  robust perception      ✓

  The ISP is not optional. It is the foundation.
```

**Jetson / Embedded Connection**

On every ADAS SoC, the ISP is a dedicated hardware block — not software.

```
  Platform              ISP Block            Notes
  ─────────────────────────────────────────────────────────────
  NVIDIA Jetson Orin    VI/ISP unit          12-bit, HDR, up to 16 cameras,
                                             Bayer → NV12, accessed via libargus
  Qualcomm Snapdragon   Spectra ISP          14-bit, triple ISP (3 cameras),
                                             HDR, multi-frame noise reduction
  Texas Instruments     VPAC                 12-bit, lens distortion correction,
    TDA4                                     DMA to C7x DSP
  Xilinx Zynq (KV260)  Soft ISP on PL       implement in HLS — full control
  Mobileye EyeQ        Integrated ISP       proprietary, not configurable

  Jetson data path:
    Sensor → MIPI CSI-2 → VI → ISP → NV12 → NVMM buffer
                                              │
                                              ├──→ CUDA / TensorRT (inference)
                                              ├──→ NVENC (H.265 recording)
                                              └──→ nvvidconv (display)

  Same physics, dedicated silicon. Zero CPU involvement.
```

---

</details>

### 2.1 颜色空间

```python
import cv2
import numpy as np

img_bgr = cv2.imread('image.jpg')           # OpenCV reads as BGR

img_rgb  = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
img_gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)
img_hsv  = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2HSV)
img_lab  = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2LAB)

# HSV is useful for color-based segmentation (hue is color, S=saturation, V=brightness)
# LAB is perceptually uniform — good for distance-based color similarity
# Gray reduces to 1 channel — reduces computation for non-color tasks
```

### 2.2 滤波

#### 空间滤波器

```python
# Gaussian blur — smooths noise
blurred = cv2.GaussianBlur(img_gray, ksize=(5, 5), sigmaX=1.5)

# Median blur — removes salt-and-pepper noise (edge-preserving)
median = cv2.medianBlur(img_gray, ksize=5)

# Bilateral filter — edge-preserving smoothing (keeps sharp edges, blurs flat regions)
bilateral = cv2.bilateralFilter(img_bgr, d=9, sigmaColor=75, sigmaSpace=75)

# Sharpening via custom kernel
kernel_sharpen = np.array([[ 0, -1,  0],
                            [-1,  5, -1],
                            [ 0, -1,  0]])
sharpened = cv2.filter2D(img_bgr, -1, kernel_sharpen)
```

#### 自适应滤波

```python
# Adaptive threshold — threshold varies per region (handles uneven lighting)
adaptive_thresh = cv2.adaptiveThreshold(
    img_gray,
    maxValue=255,
    adaptiveMethod=cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
    thresholdType=cv2.THRESH_BINARY,
    blockSize=11,   # neighborhood size (must be odd)
    C=2             # constant subtracted from mean
)

# CLAHE — Contrast Limited Adaptive Histogram Equalization
clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
enhanced = clahe.apply(img_gray)
```

#### 频域滤波

```python
import numpy as np

def frequency_domain_filter(img_gray: np.ndarray, low_pass: bool = True,
                             radius: int = 30) -> np.ndarray:
    """Apply low-pass or high-pass filter in frequency domain."""
    rows, cols = img_gray.shape
    crow, ccol = rows // 2, cols // 2

    # Fourier transform + shift DC to center
    dft = np.fft.fft2(img_gray.astype(np.float32))
    dft_shift = np.fft.fftshift(dft)

    # Create circular mask
    Y, X = np.ogrid[:rows, :cols]
    dist = np.sqrt((X - ccol)**2 + (Y - crow)**2)
    mask = (dist <= radius).astype(np.float32)
    if not low_pass:
        mask = 1 - mask   # invert for high-pass

    # Apply mask, inverse FFT
    filtered = np.fft.ifftshift(dft_shift * mask)
    result = np.fft.ifft2(filtered)
    return np.abs(result).clip(0, 255).astype(np.uint8)

# Low-pass: removes high-freq noise (blurring effect)
# High-pass: keeps edges only (sharpening effect)
```

#### 小波变换

```python
# pip install PyWavelets
import pywt

def wavelet_denoise(img_gray: np.ndarray, wavelet: str = 'db1',
                    level: int = 3, threshold: float = 20.0) -> np.ndarray:
    """Denoise image using wavelet soft-thresholding."""
    coeffs = pywt.wavedec2(img_gray.astype(float), wavelet, level=level)

    # Soft-threshold all detail coefficients (not approximation)
    new_coeffs = [coeffs[0]]  # keep approximation
    for detail_tuple in coeffs[1:]:
        new_coeffs.append(tuple(
            pywt.threshold(d, threshold, mode='soft') for d in detail_tuple
        ))

    denoised = pywt.waverec2(new_coeffs, wavelet)
    return denoised.clip(0, 255).astype(np.uint8)
```

### 2.3 边缘检测

```python
# Canny edge detector — gradient magnitude + non-max suppression + hysteresis
edges = cv2.Canny(img_gray, threshold1=50, threshold2=150)

# Sobel — gradient in X and Y separately
sobel_x = cv2.Sobel(img_gray, cv2.CV_64F, dx=1, dy=0, ksize=3)
sobel_y = cv2.Sobel(img_gray, cv2.CV_64F, dx=0, dy=1, ksize=3)
magnitude = np.sqrt(sobel_x**2 + sobel_y**2)

# Laplacian — second derivative (finds blobs + edges)
laplacian = cv2.Laplacian(img_gray, cv2.CV_64F)
```

### 2.4 形态学操作

```python
kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))

erosion  = cv2.erode(binary_img, kernel, iterations=1)   # shrinks white regions
dilation = cv2.dilate(binary_img, kernel, iterations=1)  # grows white regions
opening  = cv2.morphologyEx(binary_img, cv2.MORPH_OPEN, kernel)   # erosion→dilation (removes noise)
closing  = cv2.morphologyEx(binary_img, cv2.MORPH_CLOSE, kernel)  # dilation→erosion (fills holes)
gradient = cv2.morphologyEx(binary_img, cv2.MORPH_GRADIENT, kernel)  # dilation−erosion = outline
```

---

## 3. 特征提取

### 3.1 经典描述子

```python
# ── ORB (fast, free, rotation/scale invariant) ──────────────────────────────
orb = cv2.ORB_create(nfeatures=500)
keypoints, descriptors = orb.detectAndCompute(img_gray, mask=None)
# descriptors: (N, 32) uint8 — binary descriptor

# ── SIFT (scale/rotation invariant, patent expired 2020) ────────────────────
sift = cv2.SIFT_create()
kp, des = sift.detectAndCompute(img_gray, None)
# des: (N, 128) float32

# ── Feature Matching ─────────────────────────────────────────────────────────
bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)   # for ORB
matches = bf.match(des1, des2)
matches = sorted(matches, key=lambda x: x.distance)

# FLANN matcher (faster for large descriptor sets)
FLANN_INDEX_LSH = 6
index_params = dict(algorithm=FLANN_INDEX_LSH, table_number=6, key_size=12)
search_params = dict(checks=50)
flann = cv2.FlannBasedMatcher(index_params, search_params)
matches = flann.knnMatch(des1, des2, k=2)

# Lowe's ratio test — filter bad matches
good = [m for m, n in matches if m.distance < 0.75 * n.distance]
```

### 3.2 纹理特征

```python
from skimage.feature import local_binary_pattern, graycomatrix, graycoprops

# LBP — Local Binary Patterns (rotation-invariant texture)
radius = 3
n_points = 8 * radius
lbp = local_binary_pattern(img_gray, n_points, radius, method='uniform')
hist, _ = np.histogram(lbp.ravel(), bins=n_points + 2,
                       range=(0, n_points + 2), density=True)

# GLCM — Gray-Level Co-occurrence Matrix
glcm = graycomatrix(img_gray, distances=[1, 3], angles=[0, np.pi/4, np.pi/2],
                    levels=256, symmetric=True, normed=True)
contrast    = graycoprops(glcm, 'contrast').mean()
homogeneity = graycoprops(glcm, 'homogeneity').mean()
energy      = graycoprops(glcm, 'energy').mean()
correlation = graycoprops(glcm, 'correlation').mean()
```

### 3.3 HOG — 方向梯度直方图

```python
from skimage.feature import hog

features, hog_image = hog(
    img_gray,
    orientations=9,
    pixels_per_cell=(8, 8),
    cells_per_block=(2, 2),
    visualize=True,
    channel_axis=None
)
# features: 1D vector describing gradient orientation histogram
# Classic use: pedestrian detection with SVM classifier
```

---

## 4. 图像分割

### 4.1 基于阈值

```python
# Global Otsu threshold — automatic threshold selection
_, otsu = cv2.threshold(img_gray, 0, 255,
                        cv2.THRESH_BINARY + cv2.THRESH_OTSU)
```

### 4.2 分水岭

```python
def watershed_segment(img_bgr: np.ndarray) -> np.ndarray:
    """Segment touching objects using Watershed algorithm."""
    gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)
    _, thresh = cv2.threshold(gray, 0, 255,
                              cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)

    # Remove noise
    kernel = np.ones((3, 3), np.uint8)
    opening = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel, iterations=2)

    # Sure background (dilated)
    sure_bg = cv2.dilate(opening, kernel, iterations=3)

    # Sure foreground (via distance transform)
    dist = cv2.distanceTransform(opening, cv2.DIST_L2, 5)
    _, sure_fg = cv2.threshold(dist, 0.7 * dist.max(), 255, 0)
    sure_fg = sure_fg.astype(np.uint8)

    # Unknown region
    unknown = cv2.subtract(sure_bg, sure_fg)

    # Markers for watershed
    _, markers = cv2.connectedComponents(sure_fg)
    markers += 1
    markers[unknown == 255] = 0

    markers = cv2.watershed(img_bgr, markers)
    img_bgr[markers == -1] = [0, 0, 255]   # boundaries in red
    return markers
```

### 4.3 语义分割

语义分割为图像中的每个像素分配一个类别标签。与目标检测（边界框）或实例分割（逐对象掩码）不同，语义分割把同一类别的所有实例视为一个区域。

```
Input:  RGB image (H, W, 3)
Output: label map  (H, W)    — each pixel = class index (0=background, 1=road, 2=car …)
```

#### 架构概览

```
FCN (2015)           — first fully-convolutional net; bilinear upsampling
SegNet (2016)        — encoder-decoder with pooling indices for upsampling
U-Net (2015)         — encoder-decoder with skip connections; standard for medical/satellite
PSPNet (2017)        — pyramid pooling module captures multi-scale context
DeepLab v3+ (2018)   — atrous (dilated) convolutions + ASPP + decoder; PASCAL/Cityscapes SOTA
SegFormer (2021)     — transformer encoder + lightweight MLP decoder; strong on ADE20K
Mask2Former (2022)   — unified architecture for semantic/instance/panoptic
```

#### 架构细节：DeepLab v3+

```
Input image
    ↓
Encoder (ResNet/MobileNet backbone with dilated conv)
    ↓
ASPP — Atrous Spatial Pyramid Pooling
  ├── 1×1 conv
  ├── dilated conv rate=6
  ├── dilated conv rate=12
  ├── dilated conv rate=18
  └── global average pooling
  → concatenate → 1×1 conv → features (H/16, W/16, 256)
    ↓
Decoder
  ├── bilinear upsample ×4
  ├── concat with low-level encoder features (H/4 resolution)
  └── 3×3 conv → 1×1 conv → (H/4, W/4, num_classes)
    ↓
Bilinear upsample ×4 → logits (H, W, num_classes)
    ↓
argmax → per-pixel class label
```

#### segmentation_models_pytorch — 标准库

```bash
pip install segmentation-models-pytorch
```

```python
import segmentation_models_pytorch as smp
import torch
import torch.nn as nn

# ── Build model ──────────────────────────────────────────────────────────────
model = smp.DeepLabV3Plus(
    encoder_name='resnet50',        # backbone: resnet18/34/50, efficientnet-b*, mit_b*
    encoder_weights='imagenet',     # pretrained weights
    in_channels=3,
    classes=19,                     # Cityscapes has 19 evaluation classes
)

# Other architectures (same API):
# smp.Unet(...)         — classic U-Net (best for small datasets)
# smp.FPN(...)          — Feature Pyramid Network decoder
# smp.PSPNet(...)       — Pyramid Scene Parsing
# smp.SegFormer(...)    — transformer encoder + MLP decoder

# ── Loss functions ────────────────────────────────────────────────────────────
# smp provides standard losses that work with logits (no sigmoid/softmax needed)
dice_loss = smp.losses.DiceLoss(mode='multiclass')
ce_loss   = nn.CrossEntropyLoss(ignore_index=255)   # 255 = void/ignore label

def combined_loss(logits, targets):
    return 0.5 * ce_loss(logits, targets) + 0.5 * dice_loss(logits, targets)

# ── Training step ─────────────────────────────────────────────────────────────
optimizer = torch.optim.AdamW(model.parameters(), lr=1e-4, weight_decay=1e-4)
scheduler = torch.optim.lr_scheduler.PolynomialLR(optimizer, total_iters=100, power=0.9)

model.train()
for images, masks in train_loader:
    images = images.cuda()   # (B, 3, H, W) float32
    masks  = masks.cuda()    # (B, H, W) long  — class indices

    logits = model(images)   # (B, num_classes, H, W)
    loss   = combined_loss(logits, masks)

    optimizer.zero_grad()
    loss.backward()
    optimizer.step()
scheduler.step()

# ── Inference ─────────────────────────────────────────────────────────────────
model.eval()
with torch.no_grad():
    logits = model(images)                  # (B, C, H, W)
    probs  = torch.softmax(logits, dim=1)   # (B, C, H, W)
    preds  = probs.argmax(dim=1)            # (B, H, W)
```

#### 指标：mIoU 与像素准确率

mIoU（mean Intersection-over-Union）是标准指标。它按类别计算，再取平均。

```python
import torch

def compute_miou(preds: torch.Tensor, targets: torch.Tensor,
                 num_classes: int, ignore_index: int = 255) -> dict:
    """
    Compute per-class IoU and mIoU.

    Args:
        preds:   (B, H, W) long — predicted class per pixel
        targets: (B, H, W) long — ground truth class per pixel
    Returns:
        dict with 'miou', 'per_class_iou', 'pixel_acc'
    """
    mask = targets != ignore_index
    preds   = preds[mask]
    targets = targets[mask]

    per_class_iou = []
    for cls in range(num_classes):
        pred_cls   = preds == cls
        target_cls = targets == cls

        intersection = (pred_cls & target_cls).sum().float()
        union        = (pred_cls | target_cls).sum().float()

        if union == 0:
            per_class_iou.append(float('nan'))   # class not present
        else:
            per_class_iou.append((intersection / union).item())

    valid_iou = [v for v in per_class_iou if not torch.isnan(torch.tensor(v))]
    miou = sum(valid_iou) / len(valid_iou) if valid_iou else 0.0
    pixel_acc = (preds == targets).float().mean().item()

    return {
        'miou': miou,
        'per_class_iou': per_class_iou,
        'pixel_acc': pixel_acc,
    }

# Usage
metrics = compute_miou(preds, targets, num_classes=19)
print(f"mIoU: {metrics['miou']:.4f}  |  Pixel Acc: {metrics['pixel_acc']:.4f}")
```

```
Common mIoU benchmarks:
  Cityscapes val:  DeepLabV3+(R101) = 81.3%
                   SegFormer(B5)    = 84.0%
  ADE20K val:      SegFormer(B5)    = 51.8%
  PASCAL VOC:      DeepLabV3+       = 89.0%
```

#### YOLOv8-seg — 单模型中的语义 + 实例

```python
from ultralytics import YOLO
import numpy as np
import cv2

# YOLOv8-seg does instance segmentation — masks per detected object
# For ADAS road segmentation, use a class-specific semantic projection
model = YOLO('yolov8n-seg.pt')   # n/s/m/l/x variants

results = model('frame.jpg', conf=0.4)

for r in results:
    if r.masks is not None:
        # masks.data: (N_instances, H, W) float32, values 0–1
        masks = r.masks.data.cpu().numpy()
        classes = r.boxes.cls.cpu().numpy().astype(int)

        # Build semantic map by projecting instance masks
        H, W = r.orig_shape
        semantic_map = np.zeros((H, W), dtype=np.uint8)
        for mask, cls_id in zip(masks, classes):
            binary = (cv2.resize(mask, (W, H)) > 0.5)
            semantic_map[binary] = cls_id + 1   # 0 = background

# Fine-tune on custom segmentation dataset
model = YOLO('yolov8n-seg.pt')
model.train(
    data='seg_dataset.yaml',   # same format as detection, but images have masks
    epochs=100,
    imgsz=640,
    batch=8,
    device='cuda',
)
```

#### 分割训练的数据集格式（YOLO）

```yaml
# seg_dataset.yaml
path: /data/road_seg
train: images/train
val:   images/val

nc: 5
names:
  0: road
  1: sidewalk
  2: car
  3: person
  4: vegetation
```

```
# Polygon mask label (YOLO seg format)
# labels/frame001.txt
# class_id  x1 y1  x2 y2  x3 y3  ...  (normalized polygon vertices)
0  0.10 0.95  0.45 0.55  0.90 0.55  0.95 0.95   ← road polygon
2  0.30 0.40  0.45 0.40  0.45 0.70  0.30 0.70   ← car bounding polygon
```

#### 基于 Cityscapes 类别的 ADAS 道路分割

```python
import torch
import cv2
import numpy as np
import segmentation_models_pytorch as smp

# Cityscapes 19-class palette (RGB)
CITYSCAPES_PALETTE = np.array([
    [128, 64, 128],   # 0  road
    [244, 35, 232],   # 1  sidewalk
    [ 70, 70, 70],   # 2  building
    [102, 102, 156],  # 3  wall
    [190, 153, 153],  # 4  fence
    [153, 153, 153],  # 5  pole
    [250, 170,  30],  # 6  traffic light
    [220, 220,   0],  # 7  traffic sign
    [107, 142,  35],  # 8  vegetation
    [152, 251, 152],  # 9  terrain
    [ 70, 130, 180],  # 10 sky
    [220,  20,  60],  # 11 person
    [255,   0,   0],  # 12 rider
    [  0,   0, 142],  # 13 car
    [  0,   0,  70],  # 14 truck
    [  0,  60, 100],  # 15 bus
    [  0,  80, 100],  # 16 train
    [  0,   0, 230],  # 17 motorcycle
    [119,  11,  32],  # 18 bicycle
], dtype=np.uint8)

CITYSCAPES_MEAN = np.array([0.485, 0.456, 0.406])
CITYSCAPES_STD  = np.array([0.229, 0.224, 0.225])


def preprocess_frame(frame_bgr: np.ndarray, size=(1024, 512)) -> torch.Tensor:
    """Resize and normalize a BGR frame for Cityscapes-trained model."""
    img = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
    img = cv2.resize(img, size)
    img = img.astype(np.float32) / 255.0
    img = (img - CITYSCAPES_MEAN) / CITYSCAPES_STD
    return torch.from_numpy(img.transpose(2, 0, 1)).unsqueeze(0).float()


def colorize(label_map: np.ndarray) -> np.ndarray:
    """Map (H, W) class indices → (H, W, 3) RGB color image."""
    color = np.zeros((*label_map.shape, 3), dtype=np.uint8)
    for cls_id, rgb in enumerate(CITYSCAPES_PALETTE):
        color[label_map == cls_id] = rgb
    return color


def segment_frame(model, frame_bgr: np.ndarray, device='cuda') -> np.ndarray:
    """Run full segmentation pipeline on one frame. Returns color overlay."""
    H, W = frame_bgr.shape[:2]
    inp = preprocess_frame(frame_bgr).to(device)

    with torch.no_grad():
        logits = model(inp)                        # (1, 19, h, w)
        pred   = logits.argmax(1).squeeze().cpu().numpy()  # (h, w)

    # Upsample prediction to original resolution
    pred_full = cv2.resize(pred.astype(np.uint8), (W, H),
                           interpolation=cv2.INTER_NEAREST)
    color_mask = colorize(pred_full)

    # Blend with original frame
    overlay = cv2.addWeighted(frame_bgr, 0.5,
                              cv2.cvtColor(color_mask, cv2.COLOR_RGB2BGR), 0.5, 0)
    return overlay, pred_full


# Load Cityscapes pretrained model
model = smp.DeepLabV3Plus(
    encoder_name='resnet50',
    encoder_weights='imagenet',
    classes=19,
).cuda().eval()

# Run on video
cap = cv2.VideoCapture('dashcam.mp4')
while True:
    ret, frame = cap.read()
    if not ret:
        break
    overlay, pred = segment_frame(model, frame)
    road_pixels = (pred == 0).sum()
    total_pixels = pred.size
    print(f"Road coverage: {100 * road_pixels / total_pixels:.1f}%")
    cv2.imshow('Semantic Segmentation', overlay)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break
```

#### 快速对比：语义 vs 实例 vs 全景

```
Semantic segmentation:
  Output: (H, W) label map — every pixel has one class
  All cars = same "car" region
  Models: DeepLab, SegFormer, FCN

Instance segmentation:
  Output: N masks, one per object — same class objects get separate masks
  Car A mask, Car B mask, Car C mask
  Models: Mask R-CNN, YOLOv8-seg, SOLO, SOLOv2

Panoptic segmentation:
  Output: combines both — stuff (road, sky) as semantic, things (car, person) as instances
  Complete scene understanding
  Models: Panoptic FPN, Mask2Former, DETR (panoptic head)

ADAS typically needs:
  Drivable area      → semantic (road class)
  Obstacle avoidance → instance (each car/pedestrian separately)
  Full understanding → panoptic
```

### 4.4 使用 Mask R-CNN 的实例分割（PyTorch）

```python
import torchvision
import torch

model = torchvision.models.detection.maskrcnn_resnet50_fpn(pretrained=True)
model.eval()

from torchvision.transforms.functional import to_tensor
img_tensor = to_tensor(img_rgb).unsqueeze(0)

with torch.no_grad():
    predictions = model(img_tensor)[0]

# predictions keys: 'boxes', 'labels', 'scores', 'masks'
for i, score in enumerate(predictions['scores']):
    if score > 0.5:
        box  = predictions['boxes'][i].int().numpy()    # [x1, y1, x2, y2]
        mask = predictions['masks'][i, 0].numpy() > 0.5  # bool mask (H, W)
        label = predictions['labels'][i].item()
```

---

## 5. 目标检测

### 5.1 检测范式

```
Two-stage (high accuracy, slower):
  Region proposals → classify each proposal
  Examples: Faster R-CNN, Mask R-CNN

One-stage (real-time, slightly lower accuracy):
  Grid/anchor predictions in single pass
  Examples: YOLO, SSD, RetinaNet, FCOS

Anchor-free (modern, simpler):
  No pre-defined anchor boxes
  Examples: CenterNet, FCOS, RT-DETR
```

### 5.2 YOLO — You Only Look Once

```python
# YOLOv8 — Ultralytics (easiest modern YOLO)
# pip install ultralytics

from ultralytics import YOLO

# Load pretrained model
model = YOLO('yolov8n.pt')   # nano (fastest), also: s, m, l, x

# Inference
results = model('image.jpg', conf=0.4, iou=0.45)

for r in results:
    for box in r.boxes:
        xyxy  = box.xyxy[0].numpy()   # [x1, y1, x2, y2]
        conf  = float(box.conf)
        cls   = int(box.cls)
        label = model.names[cls]
        print(f"{label}: {conf:.2f} at {xyxy}")

# Fine-tune on custom dataset
model.train(
    data='custom_dataset.yaml',
    epochs=100,
    imgsz=640,
    batch=16,
    device='cuda',
    patience=20,        # early stopping
    optimizer='AdamW',
    lr0=0.001
)

# Export to ONNX / TensorRT
model.export(format='onnx', opset=12, simplify=True)
model.export(format='engine', device=0, half=True)  # TensorRT FP16
```

### 5.3 自定义 YOLO 数据集 YAML

```yaml
# custom_dataset.yaml
path: /data/my_dataset      # root path
train: images/train
val: images/val
test: images/test

nc: 3                       # number of classes
names:
  0: forklift
  1: person
  2: pallet
```

### 5.4 检测指标

| 指标 | 定义 |
|--------|-----------|
| **IoU** | 交并比 — 框重叠质量 |
| **精度** | TP / (TP + FP) — 所有检测结果中正确的比例 |
| **召回率** | TP / (TP + FN) — 所有 GT 目标中被找到的比例 |
| **AP** | Precision-Recall 曲线下面积（每类） |
| **mAP@50** | IoU 阈值 0.5 下的平均 AP |
| **mAP@50:95** | 在 IoU 0.5–0.95 上取平均的 AP（COCO 标准） |

```python
# Evaluate with Ultralytics
metrics = model.val(data='custom_dataset.yaml')
print(metrics.box.map)      # mAP@50:95
print(metrics.box.map50)    # mAP@50
print(metrics.box.mp)       # mean precision
print(metrics.box.mr)       # mean recall
```

---

## 6. 目标跟踪

### 6.1 OpenCV 中的经典跟踪器

```python
# Create tracker — options: CSRT (accurate), KCF (fast), MIL
tracker = cv2.TrackerCSRT_create()

# Initialize with first frame + bounding box
ret, frame = cap.read()
bbox = cv2.selectROI('Select Object', frame, fromCenter=False)
tracker.init(frame, bbox)

while True:
    ret, frame = cap.read()
    ok, bbox = tracker.update(frame)
    if ok:
        x, y, w, h = [int(v) for v in bbox]
        cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
    else:
        cv2.putText(frame, 'Tracking failure', (50, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.75, (0, 0, 255), 2)
```


<details>
<summary>English original</summary>

**Quick Comparison: Semantic vs Instance vs Panoptic**

```
Semantic segmentation:
  Output: (H, W) label map — every pixel has one class
  All cars = same "car" region
  Models: DeepLab, SegFormer, FCN

Instance segmentation:
  Output: N masks, one per object — same class objects get separate masks
  Car A mask, Car B mask, Car C mask
  Models: Mask R-CNN, YOLOv8-seg, SOLO, SOLOv2

Panoptic segmentation:
  Output: combines both — stuff (road, sky) as semantic, things (car, person) as instances
  Complete scene understanding
  Models: Panoptic FPN, Mask2Former, DETR (panoptic head)

ADAS typically needs:
  Drivable area      → semantic (road class)
  Obstacle avoidance → instance (each car/pedestrian separately)
  Full understanding → panoptic
```

**4.4 Instance Segmentation with Mask R-CNN (PyTorch)**

```python
import torchvision
import torch

model = torchvision.models.detection.maskrcnn_resnet50_fpn(pretrained=True)
model.eval()

from torchvision.transforms.functional import to_tensor
img_tensor = to_tensor(img_rgb).unsqueeze(0)

with torch.no_grad():
    predictions = model(img_tensor)[0]

# predictions keys: 'boxes', 'labels', 'scores', 'masks'
for i, score in enumerate(predictions['scores']):
    if score > 0.5:
        box  = predictions['boxes'][i].int().numpy()    # [x1, y1, x2, y2]
        mask = predictions['masks'][i, 0].numpy() > 0.5  # bool mask (H, W)
        label = predictions['labels'][i].item()
```

---

**5. Object Detection**

**5.1 Detection Paradigms**

```
Two-stage (high accuracy, slower):
  Region proposals → classify each proposal
  Examples: Faster R-CNN, Mask R-CNN

One-stage (real-time, slightly lower accuracy):
  Grid/anchor predictions in single pass
  Examples: YOLO, SSD, RetinaNet, FCOS

Anchor-free (modern, simpler):
  No pre-defined anchor boxes
  Examples: CenterNet, FCOS, RT-DETR
```

**5.2 YOLO — You Only Look Once**

```python
# YOLOv8 — Ultralytics (easiest modern YOLO)
# pip install ultralytics

from ultralytics import YOLO

# Load pretrained model
model = YOLO('yolov8n.pt')   # nano (fastest), also: s, m, l, x

# Inference
results = model('image.jpg', conf=0.4, iou=0.45)

for r in results:
    for box in r.boxes:
        xyxy  = box.xyxy[0].numpy()   # [x1, y1, x2, y2]
        conf  = float(box.conf)
        cls   = int(box.cls)
        label = model.names[cls]
        print(f"{label}: {conf:.2f} at {xyxy}")

# Fine-tune on custom dataset
model.train(
    data='custom_dataset.yaml',
    epochs=100,
    imgsz=640,
    batch=16,
    device='cuda',
    patience=20,        # early stopping
    optimizer='AdamW',
    lr0=0.001
)

# Export to ONNX / TensorRT
model.export(format='onnx', opset=12, simplify=True)
model.export(format='engine', device=0, half=True)  # TensorRT FP16
```

**5.3 Custom YOLO Dataset YAML**

```yaml
# custom_dataset.yaml
path: /data/my_dataset      # root path
train: images/train
val: images/val
test: images/test

nc: 3                       # number of classes
names:
  0: forklift
  1: person
  2: pallet
```

**5.4 Detection Metrics**

| Metric | Definition |
|--------|-----------|
| **IoU** | Intersection over Union — box overlap quality |
| **Precision** | TP / (TP + FP) — of all detections, how many are correct |
| **Recall** | TP / (TP + FN) — of all GT objects, how many were found |
| **AP** | Area under Precision-Recall curve (per class) |
| **mAP@50** | Mean AP at IoU threshold 0.5 |
| **mAP@50:95** | Mean AP averaged over IoU 0.5–0.95 (COCO standard) |

```python
# Evaluate with Ultralytics
metrics = model.val(data='custom_dataset.yaml')
print(metrics.box.map)      # mAP@50:95
print(metrics.box.map50)    # mAP@50
print(metrics.box.mp)       # mean precision
print(metrics.box.mr)       # mean recall
```

---

**6. Object Tracking**

**6.1 Classic Trackers in OpenCV**

```python
# Create tracker — options: CSRT (accurate), KCF (fast), MIL
tracker = cv2.TrackerCSRT_create()

# Initialize with first frame + bounding box
ret, frame = cap.read()
bbox = cv2.selectROI('Select Object', frame, fromCenter=False)
tracker.init(frame, bbox)

while True:
    ret, frame = cap.read()
    ok, bbox = tracker.update(frame)
    if ok:
        x, y, w, h = [int(v) for v in bbox]
        cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
    else:
        cv2.putText(frame, 'Tracking failure', (50, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.75, (0, 0, 255), 2)
```

</details>

### 6.2 ByteTrack — 多目标跟踪

ByteTrack 是当前实时 MOT 的标准方案（可与任意检测器搭配）。

```bash
pip install lap  # linear assignment
pip install git+https://github.com/ifzhang/ByteTrack.git
```

```python
from ultralytics import YOLO

# Ultralytics has ByteTrack built-in
model = YOLO('yolov8n.pt')

# Track across video frames
results = model.track(
    source='video.mp4',
    tracker='bytetrack.yaml',
    persist=True,       # keep track IDs between frames
    conf=0.3,
    iou=0.5
)

for r in results:
    if r.boxes.id is not None:
        track_ids = r.boxes.id.int().tolist()
        boxes = r.boxes.xyxy.tolist()
        for tid, box in zip(track_ids, boxes):
            print(f"Track {tid}: {box}")
```

### 6.3 光流

```python
# Lucas-Kanade sparse optical flow — tracks specific points
feature_params = dict(maxCorners=100, qualityLevel=0.3,
                      minDistance=7, blockSize=7)
lk_params = dict(winSize=(15, 15), maxLevel=2,
                 criteria=(cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 10, 0.03))

ret, old_frame = cap.read()
old_gray = cv2.cvtColor(old_frame, cv2.COLOR_BGR2GRAY)
p0 = cv2.goodFeaturesToTrack(old_gray, mask=None, **feature_params)

while True:
    ret, frame = cap.read()
    frame_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    p1, st, err = cv2.calcOpticalFlowPyrLK(old_gray, frame_gray, p0, None, **lk_params)
    good_new = p1[st == 1]
    good_old = p0[st == 1]
    old_gray = frame_gray.copy()
    p0 = good_new.reshape(-1, 1, 2)

# Farneback dense optical flow — flow for every pixel
flow = cv2.calcOpticalFlowFarneback(
    old_gray, frame_gray, None,
    pyr_scale=0.5, levels=3, winsize=15,
    iterations=3, poly_n=5, poly_sigma=1.2, flags=0
)
mag, ang = cv2.cartToPolar(flow[..., 0], flow[..., 1])
```

---

## 7. 3D 视觉

### 7.1 相机模型与校准

```python
# Checkerboard calibration
import cv2
import numpy as np

CHECKERBOARD = (9, 6)   # inner corners
criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

objp = np.zeros((CHECKERBOARD[0] * CHECKERBOARD[1], 3), np.float32)
objp[:, :2] = np.mgrid[0:CHECKERBOARD[0], 0:CHECKERBOARD[1]].T.reshape(-1, 2)

objpoints, imgpoints = [], []
for fname in calibration_images:
    img = cv2.imread(fname)
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    ret, corners = cv2.findChessboardCorners(gray, CHECKERBOARD, None)
    if ret:
        corners2 = cv2.cornerSubPix(gray, corners, (11,11), (-1,-1), criteria)
        objpoints.append(objp)
        imgpoints.append(corners2)

ret, K, dist, rvecs, tvecs = cv2.calibrateCamera(
    objpoints, imgpoints, gray.shape[::-1], None, None
)
# K: 3×3 intrinsic matrix [[fx,0,cx],[0,fy,cy],[0,0,1]]
# dist: [k1,k2,p1,p2,k3] distortion coefficients

# Undistort an image
h, w = img.shape[:2]
newK, roi = cv2.getOptimalNewCameraMatrix(K, dist, (w,h), 1, (w,h))
undistorted = cv2.undistort(img, K, dist, None, newK)
```

### 7.2 立体视觉 — 从两个相机获得深度

```python
# After calibrating both cameras separately, stereo calibrate
ret, K1, D1, K2, D2, R, T, E, F = cv2.stereoCalibrate(
    obj_pts, img_pts_left, img_pts_right,
    K1, D1, K2, D2, gray.shape[::-1],
    flags=cv2.CALIB_FIX_INTRINSIC
)

# Rectify — make epipolar lines horizontal
R1, R2, P1, P2, Q, roi1, roi2 = cv2.stereoRectify(
    K1, D1, K2, D2, gray.shape[::-1], R, T
)

# Compute disparity map
stereo = cv2.StereoSGBM_create(
    minDisparity=0, numDisparities=128, blockSize=11,
    P1=8*3*11**2, P2=32*3*11**2,
    disp12MaxDiff=1, uniquenessRatio=15,
    speckleWindowSize=100, speckleRange=32
)
disparity = stereo.compute(left_rect, right_rect).astype(np.float32) / 16.0

# Convert disparity to depth
# depth = (baseline × focal_length) / disparity
baseline = np.linalg.norm(T)          # meters between cameras
depth = (baseline * K1[0, 0]) / (disparity + 1e-6)

# Or use the Q matrix from stereoRectify
points_3D = cv2.reprojectImageTo3D(disparity, Q)  # (H, W, 3) XYZ per pixel
```

### 7.3 位姿估计（solvePnP）

```python
# Given 3D object points and their 2D image projections, find camera pose
# object_points: (N, 3) float32 — 3D coordinates in object frame
# image_points:  (N, 2) float32 — corresponding 2D pixel coordinates

success, rvec, tvec = cv2.solvePnP(
    object_points, image_points, K, dist,
    flags=cv2.SOLVEPNP_ITERATIVE
)

# rvec: rotation vector (Rodrigues format) → convert to matrix
R, _ = cv2.Rodrigues(rvec)   # 3×3 rotation matrix
# tvec: translation vector (camera-to-object)

# Project 3D points back to image
projected, _ = cv2.projectPoints(object_points, rvec, tvec, K, dist)

# Draw axes on object
cv2.drawFrameAxes(img, K, dist, rvec, tvec, length=0.05)
```

### 7.4 多摄像头 ADAS：openpilot 近距 + 宽视场摄像头

openpilot（comma.ai）是开源 ADAS 参考栈。它使用两个前向摄像头，二者视场角刻意不同——这一设计决策揭示了 ADAS 感知中的根本权衡。

#### 为何用两个摄像头？

```
Single camera problem:
  Wide FOV (120°) → low focal length → poor resolution at distance
                  → lane lines 50m ahead are only a few pixels wide
                  → lead vehicle at 80m is too small to detect reliably

  Narrow FOV (60°) → high focal length → good long-range resolution
                   → misses adjacent lanes and near-field cut-ins
                   → blind to pedestrians at crossings

Solution: run both simultaneously, fuse their outputs per task
```

```
comma 3 / 3X camera layout:
  ┌─────────────────────────────────────────────┐
  │         windshield mount (top center)        │
  │                                              │
  │  [Wide road cam]   [Road cam]   [Driver cam] │
  │   ~120° HFOV       ~60° HFOV   (inward)     │
  │   1.71 mm lens     2.2 mm lens               │
  └─────────────────────────────────────────────┘
```

#### 摄像头规格

| 属性 | Road（Narrow） | Wide Road |
|----------|--------------|-----------|
| 传感器 | OV8856 | OV8856 |
| 焦距（约） | 2.2 mm | 1.71 mm |
| HFOV | ~60° | ~120° |
| 原生分辨率 | 1928×1208 | 1928×1208 |
| 模型输入裁剪 | 1152×1152 | 1152×1152 |
| 为模型缩放 | 512×256 → 128×256 | 512×256 → 128×256 |
| 主要用途 | 前车、车道几何、远距离 | 相邻车道、近场、更宽视野 |

#### 各摄像头的内参矩阵

```python
import numpy as np

# comma 3 road camera intrinsics (approximate — calibrated per device)
# K = [[fx,  0, cx],
#       [ 0, fy, cy],
#       [ 0,  0,  1]]

K_road = np.array([
    [2648.0,    0.0,  964.0],
    [   0.0, 2648.0,  604.0],
    [   0.0,    0.0,    1.0],
], dtype=np.float64)

K_wide = np.array([
    [1036.0,    0.0,  964.0],
    [   0.0, 1036.0,  604.0],
    [   0.0,    0.0,    1.0],
], dtype=np.float64)

# Focal lengths: road (2648 px) vs wide (1036 px)
# Lower fx/fy = shorter focal length = wider field of view
# Both cameras at same image center (cx=964, cy=604) assuming no lens offset

def pixel_to_ray(u: float, v: float, K: np.ndarray) -> np.ndarray:
    """Convert pixel (u, v) to unit direction ray in camera frame."""
    fx, fy = K[0, 0], K[1, 1]
    cx, cy = K[0, 2], K[1, 2]
    ray = np.array([(u - cx) / fx, (v - cy) / fy, 1.0])
    return ray / np.linalg.norm(ray)

# Road camera: pixel at image center-right (u=1200, v=604)
ray_road = pixel_to_ray(1200, 604, K_road)
angle_road = np.degrees(np.arctan2(ray_road[0], ray_road[2]))
print(f"Road cam angle: {angle_road:.1f}°")   # ~8.7° off-center

# Wide camera: same pixel
ray_wide = pixel_to_ray(1200, 604, K_wide)
angle_wide = np.degrees(np.arctan2(ray_wide[0], ray_wide[2]))
print(f"Wide cam angle: {angle_wide:.1f}°")   # ~22.5° off-center
```


<details>
<summary>English original</summary>

**7.4 Multi-Camera ADAS: openpilot Near + Wide Camera**

openpilot (comma.ai) is the reference open-source ADAS stack. It uses two forward-facing cameras with deliberately different fields of view — a design decision that reveals the fundamental trade-off in ADAS perception.

**Why Two Cameras?**

```
Single camera problem:
  Wide FOV (120°) → low focal length → poor resolution at distance
                  → lane lines 50m ahead are only a few pixels wide
                  → lead vehicle at 80m is too small to detect reliably

  Narrow FOV (60°) → high focal length → good long-range resolution
                   → misses adjacent lanes and near-field cut-ins
                   → blind to pedestrians at crossings

Solution: run both simultaneously, fuse their outputs per task
```

```
comma 3 / 3X camera layout:
  ┌─────────────────────────────────────────────┐
  │         windshield mount (top center)        │
  │                                              │
  │  [Wide road cam]   [Road cam]   [Driver cam] │
  │   ~120° HFOV       ~60° HFOV   (inward)     │
  │   1.71 mm lens     2.2 mm lens               │
  └─────────────────────────────────────────────┘
```

**Camera Specifications**

| Property | Road (Narrow) | Wide Road |
|----------|--------------|-----------|
| Sensor | OV8856 | OV8856 |
| Focal length (approx) | 2.2 mm | 1.71 mm |
| HFOV | ~60° | ~120° |
| Native resolution | 1928×1208 | 1928×1208 |
| Model input crop | 1152×1152 | 1152×1152 |
| Resized for model | 512×256 → 128×256 | 512×256 → 128×256 |
| Primary use | Lead vehicle, lane geometry, long range | Adjacent lanes, near-field, wider view |

**Intrinsic Matrix for Each Camera**

```python
import numpy as np

# comma 3 road camera intrinsics (approximate — calibrated per device)
# K = [[fx,  0, cx],
#       [ 0, fy, cy],
#       [ 0,  0,  1]]

K_road = np.array([
    [2648.0,    0.0,  964.0],
    [   0.0, 2648.0,  604.0],
    [   0.0,    0.0,    1.0],
], dtype=np.float64)

K_wide = np.array([
    [1036.0,    0.0,  964.0],
    [   0.0, 1036.0,  604.0],
    [   0.0,    0.0,    1.0],
], dtype=np.float64)

# Focal lengths: road (2648 px) vs wide (1036 px)
# Lower fx/fy = shorter focal length = wider field of view
# Both cameras at same image center (cx=964, cy=604) assuming no lens offset

def pixel_to_ray(u: float, v: float, K: np.ndarray) -> np.ndarray:
    """Convert pixel (u, v) to unit direction ray in camera frame."""
    fx, fy = K[0, 0], K[1, 1]
    cx, cy = K[0, 2], K[1, 2]
    ray = np.array([(u - cx) / fx, (v - cy) / fy, 1.0])
    return ray / np.linalg.norm(ray)

# Road camera: pixel at image center-right (u=1200, v=604)
ray_road = pixel_to_ray(1200, 604, K_road)
angle_road = np.degrees(np.arctan2(ray_road[0], ray_road[2]))
print(f"Road cam angle: {angle_road:.1f}°")   # ~8.7° off-center

# Wide camera: same pixel
ray_wide = pixel_to_ray(1200, 604, K_wide)
angle_wide = np.degrees(np.arctan2(ray_wide[0], ray_wide[2]))
print(f"Wide cam angle: {angle_wide:.1f}°")   # ~22.5° off-center
```

</details>

#### YUV420 帧打包 — supercombo 输入

supercombo 模型接受形状为 `(1, 12, 128, 256)` 的 `input_imgs`。这 12 个通道编码两个连续的 YUV420 帧（用于运动估计的时间上下文）。

```
YUV420 memory layout for one frame (H=128, W=256):
  Y  plane: 128 × 256 = 32768 bytes  (full luma)
  U  plane:  64 × 128 =  8192 bytes  (half-res chroma)
  V  plane:  64 × 128 =  8192 bytes  (half-res chroma)
  Total: 49152 bytes per frame

Packed into 6 channels at (128, 256):
  ch 0: Y[0::2, 0::2]   — even rows, even cols  (64×128, tiled to 128×256)
  ch 1: Y[0::2, 1::2]   — even rows, odd cols
  ch 2: Y[1::2, 0::2]   — odd rows, even cols
  ch 3: Y[1::2, 1::2]   — odd rows, odd cols
  ch 4: U resized to 128×256 via nearest-neighbor
  ch 5: V resized to 128×256 via nearest-neighbor

Two frames (t and t-1) → 6 × 2 = 12 channels
```

```python
import numpy as np
import cv2


def yuv420_to_6ch(yuv420_frame: np.ndarray, out_h: int = 128, out_w: int = 256) -> np.ndarray:
    """
    Convert a YUV420 frame to the 6-channel format used by supercombo.

    Args:
        yuv420_frame: raw YUV420 bytes as (H*3//2, W) uint8 array,
                      OR (H + H//2, W) array (standard cv2 YUV420 layout).
                      For comma 3: H=886 (after crop), W=1152.
        out_h, out_w: target spatial size (128, 256 for supercombo)

    Returns:
        (6, out_h, out_w) float32 array, values in [0, 1]
    """
    h_full = yuv420_frame.shape[0] * 2 // 3
    w_full = yuv420_frame.shape[1]

    # Split Y, U, V planes
    Y = yuv420_frame[:h_full, :]                                  # (H, W)
    uv = yuv420_frame[h_full:, :].reshape(h_full // 2, w_full)
    U = uv[:h_full // 4, :]                                      # (H/2, W/2) after reshape
    V = uv[h_full // 4:, :]

    # Resize Y to target
    Y_resized = cv2.resize(Y, (out_w, out_h), interpolation=cv2.INTER_AREA)

    # Sub-sample Y into 4 interleaved channels
    ch0 = Y_resized[0::2, 0::2]   # (out_h//2, out_w//2)
    ch1 = Y_resized[0::2, 1::2]
    ch2 = Y_resized[1::2, 0::2]
    ch3 = Y_resized[1::2, 1::2]

    # Resize to full (out_h, out_w) via repeat
    ch0 = np.repeat(np.repeat(ch0, 2, axis=0), 2, axis=1)
    ch1 = np.repeat(np.repeat(ch1, 2, axis=0), 2, axis=1)
    ch2 = np.repeat(np.repeat(ch2, 2, axis=0), 2, axis=1)
    ch3 = np.repeat(np.repeat(ch3, 2, axis=0), 2, axis=1)

    # Resize U and V to (out_h, out_w)
    ch4 = cv2.resize(U, (out_w, out_h), interpolation=cv2.INTER_NEAREST)
    ch5 = cv2.resize(V, (out_w, out_h), interpolation=cv2.INTER_NEAREST)

    channels = np.stack([ch0, ch1, ch2, ch3, ch4, ch5], axis=0)  # (6, H, W) uint8
    return channels.astype(np.float32) / 255.0


def build_supercombo_input(frame_t: np.ndarray, frame_t1: np.ndarray) -> np.ndarray:
    """
    Stack two consecutive YUV420 frames into supercombo input_imgs.

    Args:
        frame_t:  current frame YUV420 raw array
        frame_t1: previous frame YUV420 raw array

    Returns:
        (1, 12, 128, 256) float32 — ready for supercombo inference
    """
    ch_t  = yuv420_to_6ch(frame_t)    # (6, 128, 256)
    ch_t1 = yuv420_to_6ch(frame_t1)   # (6, 128, 256)
    stacked = np.concatenate([ch_t, ch_t1], axis=0)   # (12, 128, 256)
    return stacked[np.newaxis]                         # (1, 12, 128, 256)


# Load two consecutive dashcam frames (BGR → YUV420)
frame_bgr_0 = cv2.imread('frame_000.jpg')
frame_bgr_1 = cv2.imread('frame_001.jpg')

yuv_0 = cv2.cvtColor(frame_bgr_0, cv2.COLOR_BGR2YUV_I420)
yuv_1 = cv2.cvtColor(frame_bgr_1, cv2.COLOR_BGR2YUV_I420)

input_imgs = build_supercombo_input(yuv_0, yuv_1)
print(input_imgs.shape)  # (1, 12, 128, 256)
print(input_imgs.min(), input_imgs.max())  # 0.0  1.0
```

#### Road 与 Wide：按任务路由

```
Task                     Primary camera       Reason
──────────────────────────────────────────────────────────────────────
Lane detection (far)     Road (narrow)        High focal length → lane lines at 50m are wider
Lead vehicle distance    Road (narrow)        Accurate size → distance estimation
Adjacent lane cuts       Wide                 FOV covers 2 lanes either side
Near-field pedestrians   Wide                 Pedestrians at <10m outside road cam FOV
Traffic sign reading     Road (narrow)        Signs need text resolution at distance
Blind spot monitoring    Wide                 ~120° catches what road cam misses at sides
Ego-motion estimation    Road (narrow)        Stable horizon → cleaner optical flow
```

openpilot 的 supercombo 模型**只接收 road 摄像头**的帧作为 `input_imgs`。wide 摄像头接入单独的路径（例如驾驶员监控、广角目标检测）。两路输出在 `controlsd` 中融合——横向控制器使用源自 road 摄像头的车道预测，而广角检测结果更新障碍物地图。

#### 在相机坐标系之间投影

```python
import numpy as np


def project_road_to_wide(
    pts_road_cam: np.ndarray,   # (N, 3) 3D points in road camera frame
    R_r2w: np.ndarray,          # (3, 3) rotation: road cam → wide cam
    t_r2w: np.ndarray,          # (3,) translation: road cam → wide cam
    K_wide: np.ndarray,         # (3, 3) wide cam intrinsics
    D_wide: np.ndarray = None,  # (5,) wide cam distortion (optional)
) -> np.ndarray:
    """
    Project 3D points seen in road camera frame into wide camera pixel coordinates.
    Used to cross-validate detections across cameras.

    Returns:
        (N, 2) pixel coordinates in wide camera image
    """
    # Transform points to wide camera frame
    pts_wide_cam = (R_r2w @ pts_road_cam.T).T + t_r2w   # (N, 3)

    # Project to wide image plane
    if D_wide is not None:
        pts_2d, _ = cv2.projectPoints(
            pts_wide_cam.astype(np.float64),
            np.zeros(3), np.zeros(3),   # identity (already in cam frame)
            K_wide, D_wide
        )
        return pts_2d.squeeze()   # (N, 2)
    else:
        # Simple pinhole projection (no distortion)
        z = pts_wide_cam[:, 2:3].clip(0.01, None)
        xy = pts_wide_cam[:, :2] / z
        uv = (K_wide[:2, :2] @ xy.T + K_wide[:2, 2:3]).T
        return uv   # (N, 2)


# Example: project lead vehicle center from road cam 3D into wide cam 2D
# Lead vehicle detected at 30m ahead, ~0m lateral, ~1.5m height
lead_vehicle_3d = np.array([[30.0, 0.0, 1.5]])   # (N, 3) in road cam frame

# Extrinsic: wide cam is ~5mm to the left, same orientation (approx)
R_r2w = np.eye(3)   # cameras are nearly parallel
t_r2w = np.array([-0.005, 0.0, 0.0])   # 5mm lateral offset

uv_in_wide = project_road_to_wide(lead_vehicle_3d, R_r2w, t_r2w, K_wide)
print(f"Lead vehicle projects to wide cam pixel: {uv_in_wide}")
```

#### 相机到地面的单应性（平坦路面假设）

```python
def camera_to_ground_homography(K: np.ndarray,
                                 camera_height_m: float = 1.22,
                                 pitch_deg: float = 0.0) -> np.ndarray:
    """
    Compute homography mapping image pixels → ground plane (Z=0) coordinates.

    Assumes flat road. Used for:
      - Lane width estimation in meters
      - Lead vehicle distance from pixel height
      - Free-space estimation

    Args:
        K:                camera intrinsics
        camera_height_m:  camera height above road (comma 3: ~1.22m)
        pitch_deg:        downward pitch of camera (positive = looking down)

    Returns:
        H: (3, 3) homography — pixel (u,v,1) → ground (X, Y, 1) in meters
    """
    pitch = np.radians(pitch_deg)
    # Rotation: camera tilted downward by pitch
    R = np.array([
        [1,           0,            0],
        [0,  np.cos(pitch), -np.sin(pitch)],
        [0,  np.sin(pitch),  np.cos(pitch)],
    ])
    t = np.array([0, -camera_height_m, 0])   # camera above ground

    # Ground plane normal in world: [0, 1, 0], d = 0
    # Homography from image to ground (standard derivation)
    # P = K [R | t], solve for intersection with Y=0 plane
    P = K @ np.hstack([R, t[:, None]])
    # Columns 0, 2, 3 of P (drop Y column since Y=0 on ground)
    H = P[:, [0, 2, 3]]
    return np.linalg.inv(H)


def pixel_to_ground(u: float, v: float, H: np.ndarray):
    """Convert image pixel to ground plane coordinates (meters from camera)."""
    p = H @ np.array([u, v, 1.0])
    return p[0] / p[2], p[1] / p[2]   # (X_meters, Z_meters)


H_road = camera_to_ground_homography(K_road, camera_height_m=1.22, pitch_deg=1.5)
H_wide = camera_to_ground_homography(K_wide, camera_height_m=1.22, pitch_deg=1.5)

# Where does bottom-center of image touch the road in road cam?
x, z = pixel_to_ground(964, 1100, H_road)
print(f"Road cam bottom-center → ({x:.2f}m lateral, {z:.2f}m ahead)")

# Same pixel in wide cam
x_w, z_w = pixel_to_ground(964, 1100, H_wide)
print(f"Wide cam bottom-center → ({x_w:.2f}m lateral, {z_w:.2f}m ahead)")
```

#### openpilot 中的自动曝光（AE）——摄像头如何决定亮度

自动曝光是决定每一帧应该多亮的控制回路。它位于 ISP 之上——它告诉传感器曝光多长时间、施加多少增益，然后 ISP 处理传感器捕捉到的内容。

在 openpilot 中，AE 不交给传感器内置逻辑。openpilot 在软件中运行自己的 AE 算法，因为默认的摄像头 AE 优化的是「好看的照片」——但 ADAS 需要一种根本不同的曝光策略。

```
Why default AE fails for ADAS:

  Phone / consumer camera AE:
    Goal: make the image look pleasing to a human
    Strategy: expose for the center-weighted average brightness
    Result: faces look good, sky is white, shadows are crushed

  ADAS AE:
    Goal: maximize information for the neural network
    Strategy: preserve detail in BOTH road AND sky simultaneously
    Result: may look "ugly" to humans but neural net sees everything

  Specific failure cases with consumer AE:
    - Driving toward sunset: AE exposes for bright sky → road is black → lane lines invisible
    - Tunnel entry: AE averages bright exterior + dark tunnel → both look bad
    - Night with headlights: AE exposes for headlight → everything else is black
    - Snow: AE underexposes (thinks scene is too bright) → gray snow, dark objects
```

```
The exposure triangle — three variables the AE controller adjusts:

  1. Exposure time (shutter speed)
     How long the sensor collects photons per frame
     Longer = brighter but motion blur increases
     ADAS constraint: must stay below 1/framerate (33 ms at 30 fps)
     Typical ADAS: 1–20 ms

  2. Analog gain (ISO)
     Amplify the sensor signal before ADC
     Higher gain = brighter but noise increases proportionally
     ADAS constraint: keep gain low when possible (noise hurts NN accuracy)
     Typical range: ×1 (ISO 100) to ×16 (ISO 1600)

  3. Digital gain (applied after ADC)
     Software multiplication of pixel values
     Same as analog gain but applied digitally — amplifies both signal AND quantization noise
     Used only when analog gain is maxed out (nighttime)
     Last resort — worst quality

  Priority order (openpilot strategy):
    First:  increase exposure time (up to motion-blur limit)
    Second: increase analog gain (up to noise limit)
    Last:   increase digital gain (emergency only)
```

openpilot 的 AE 如何工作：


<details>
<summary>English original</summary>

**Auto Exposure (AE) in openpilot — How the Camera Decides Brightness**

Auto Exposure is the control loop that decides how bright each frame should be. It sits above the ISP — it tells the sensor how long to expose and how much gain to apply, then the ISP processes whatever the sensor captures.

In openpilot, AE is not left to the sensor's built-in logic. openpilot runs its own AE algorithm in software because the default camera AE optimizes for "nice-looking photos" — but ADAS needs a fundamentally different exposure strategy.

```
Why default AE fails for ADAS:

  Phone / consumer camera AE:
    Goal: make the image look pleasing to a human
    Strategy: expose for the center-weighted average brightness
    Result: faces look good, sky is white, shadows are crushed

  ADAS AE:
    Goal: maximize information for the neural network
    Strategy: preserve detail in BOTH road AND sky simultaneously
    Result: may look "ugly" to humans but neural net sees everything

  Specific failure cases with consumer AE:
    - Driving toward sunset: AE exposes for bright sky → road is black → lane lines invisible
    - Tunnel entry: AE averages bright exterior + dark tunnel → both look bad
    - Night with headlights: AE exposes for headlight → everything else is black
    - Snow: AE underexposes (thinks scene is too bright) → gray snow, dark objects
```

```
The exposure triangle — three variables the AE controller adjusts:

  1. Exposure time (shutter speed)
     How long the sensor collects photons per frame
     Longer = brighter but motion blur increases
     ADAS constraint: must stay below 1/framerate (33 ms at 30 fps)
     Typical ADAS: 1–20 ms

  2. Analog gain (ISO)
     Amplify the sensor signal before ADC
     Higher gain = brighter but noise increases proportionally
     ADAS constraint: keep gain low when possible (noise hurts NN accuracy)
     Typical range: ×1 (ISO 100) to ×16 (ISO 1600)

  3. Digital gain (applied after ADC)
     Software multiplication of pixel values
     Same as analog gain but applied digitally — amplifies both signal AND quantization noise
     Used only when analog gain is maxed out (nighttime)
     Last resort — worst quality

  Priority order (openpilot strategy):
    First:  increase exposure time (up to motion-blur limit)
    Second: increase analog gain (up to noise limit)
    Last:   increase digital gain (emergency only)
```

How openpilot's AE works:

</details>

```
The AE control loop runs every frame:

  1. Capture frame with current exposure settings
                ↓
  2. Compute frame statistics
     - Mean luminance of the frame (Y channel average)
     - Luminance histogram (distribution of brightness values)
     - Region-of-interest weighting (road region matters more than sky)
                ↓
  3. Compare to target brightness
     - Target is NOT "middle gray" (unlike phone cameras)
     - Target is tuned per camera:
         Road cam:  optimize for road surface visibility
         Wide cam:  optimize for near-field obstacle visibility
                ↓
  4. Compute exposure adjustment
     - If frame is too dark:  increase exposure time, then gain
     - If frame is too bright: decrease exposure time first
     - Use a PID-like controller (not bang-bang) to avoid oscillation
     - Rate-limit changes to prevent flickering between frames
                ↓
  5. Send new exposure settings to sensor via I2C/MIPI
     - Takes effect 2–3 frames later (sensor pipeline delay)
     - AE must predict where brightness is going, not just react
                ↓
  6. Next frame arrives → repeat from step 1
```

```
Key AE concepts:

  Metering regions:
    Consumer camera: center-weighted or face-priority
    ADAS camera:     lower-half-weighted (road is more important than sky)

    openpilot uses the bottom 60% of the frame for metering:
    ┌──────────────────────────────┐
    │          sky / trees         │  ← low weight (don't expose for this)
    │                              │
    ├──────────────────────────────┤
    │      road / lanes / cars     │  ← HIGH weight (expose for this)
    │      obstacles / signs       │
    └──────────────────────────────┘

    This means: even if the sky is overexposed (white), the road surface
    has correct brightness → lane lines are visible → NN succeeds

  Target luminance:
    Not a fixed value — adapts to scene conditions
    Bright daylight: target = high (use full dynamic range)
    Night:           target = lower (accept darker image to keep noise down)
    Tunnel:          ramp quickly (don't wait for slow exponential adaptation)

  Temporal smoothing:
    AE changes must be gradual — sudden brightness jumps confuse the NN
    openpilot applies exponential moving average to exposure settings
    Typical time constant: ~0.5 seconds (15 frames at 30 fps)
    Exception: tunnel entry/exit → faster adaptation allowed
```

```
HDR and AE — solving the dynamic range problem:

  Real driving scenes often have extreme contrast:
    - Sunlit road: 100,000 lux
    - Shadow under bridge: 500 lux
    - Headlight beam: 1,000,000+ lux
    - Dark lane markings in shadow: 200 lux

  Single exposure cannot capture all of this.

  HDR sensors (IMX390, AR0233AT) solve this with DOL-HDR:
    DOL = Digital Overlap

    How DOL-HDR works:
      Sensor captures TWO exposures per frame:
        Long exposure  (20 ms) → good for dark regions
        Short exposure (1 ms)  → good for bright regions

      ISP merges them:
        For each pixel:
          if short_exposure pixel is not saturated → use it (bright region)
          else → use long_exposure pixel (dark region)

      Result: single frame with much wider dynamic range
      Neural net sees both road surface AND headlights in same image

  openpilot on comma 3:
    OV8856 does NOT have DOL-HDR
    Relies entirely on software AE to choose the best single exposure
    This is why AE quality matters even more on non-HDR sensors
```

```
AE failure modes in ADAS and how openpilot handles them:

  Failure                    Cause                     Solution
  ──────────────────────────────────────────────────────────────────────
  Flickering exposure        AE oscillates between     PID controller with
                             two brightness targets    damping + rate limiting

  Tunnel blindness           sudden dark→bright or     fast-ramp mode when
                             bright→dark transition    luminance delta > threshold

  Headlight bloom            oncoming headlights       lower-half metering ignores
                             saturate AE               top-of-frame bright sources

  Snow blindness             high reflectance scene    histogram-based target
                             causes underexposure      (don't target mean, target
                                                       percentile)

  LED flicker                traffic lights / signs    exposure time must NOT be
                             modulated at 50/60 Hz     a multiple of 1/100 or 1/120 s
                             → can appear OFF in       (sensor anti-flicker mode)
                             some frames

  Sunrise/sunset glare       direct sun in camera      nothing AE can fully fix —
                             saturates entire region   need physical sun visor or
                                                       polarizing filter
```

```
AE and the neural network — the contract:

  The NN was trained on images with a specific brightness distribution.

  If the AE produces frames that are:
    Too dark    → NN sees shadows as obstacles, misses lane markings
    Too bright  → NN sees saturated white where objects should be
    Flickering  → NN confidence oscillates, control becomes jerky
    Inconsistent between cameras → road cam and wide cam disagree on scene

  openpilot's AE goal:
    Produce frames where the road region has mean luminance ≈ 100–120
    (on a 0–255 scale) regardless of weather, time of day, or lighting

    This is the input contract between camera and neural network.
    Break it → perception degrades → vehicle control degrades.
```

---

#### openpilot 模型输入总结

```
supercombo inputs:
  input_imgs:             (1, 12, 128, 256)  ← 2 consecutive road cam YUV420 frames
  desire:                 (1, 8)             ← one-hot: lane change L/R, keep, turn L/R
  traffic_convention:     (1, 2)             ← [RHD, LHD] traffic direction
  lateral_control_params: (1, 2)             ← [v_ego, roll]
  nav_features:           (1, 64)            ← map/route embedding
  nav_instructions:       (1, 150)           ← turn-by-turn instruction vector

supercombo output:
  outputs: flat float32 vector (~6504 values)
  Parsed by openpilot modeldata.py into:
    lead:         lead vehicle position, velocity, acceleration
    path:         lane line polynomials (4 points × 33 time steps)
    desire_state: predicted driver intent
    meta:         model confidence, disengagement probability
    pose:         ego velocity and orientation

Wide camera (separate model / separate input stream):
  Feeds wide-angle obstacle detection (pedestrians, cyclists at ±60° off-center)
  Output merged in controlsd with supercombo lead predictions
```

---

## 8. 高级深度学习架构

### 8.1 Vision Transformer (ViT)

```python
# ViT splits image into fixed patches, treats each as a "token"
# Self-attention captures global relationships between any two patches

from transformers import ViTForImageClassification, ViTImageProcessor
import torch

processor = ViTImageProcessor.from_pretrained('google/vit-base-patch16-224')
model = ViTForImageClassification.from_pretrained('google/vit-base-patch16-224')
model.eval()

from PIL import Image
img = Image.open('image.jpg')
inputs = processor(images=img, return_tensors='pt')

with torch.no_grad():
    logits = model(**inputs).logits
pred_class = logits.argmax(-1).item()
print(model.config.id2label[pred_class])
```

### 8.2 DETR — Detection Transformer

```python
from transformers import DetrImageProcessor, DetrForObjectDetection
import torch
from PIL import Image

processor = DetrImageProcessor.from_pretrained('facebook/detr-resnet-50')
model = DetrForObjectDetection.from_pretrained('facebook/detr-resnet-50')

img = Image.open('image.jpg')
inputs = processor(images=img, return_tensors='pt')

with torch.no_grad():
    outputs = model(**inputs)

# Post-process to get boxes and labels
target_sizes = torch.tensor([img.size[::-1]])
results = processor.post_process_object_detection(
    outputs, threshold=0.9, target_sizes=target_sizes)[0]

for score, label, box in zip(results['scores'], results['labels'], results['boxes']):
    print(f"{model.config.id2label[label.item()]}: {score:.2f} {box.tolist()}")
```

### 8.3 RT-DETR — Real-Time Detection Transformer

RT-DETR 兼具 Transformer 的准确率与 YOLO 级速度（可在 GPU 上实时运行）。

```python
from ultralytics import RTDETR

model = RTDETR('rtdetr-l.pt')   # large variant
results = model.predict('image.jpg', conf=0.4)
model.export(format='engine')   # TensorRT export
```

### 8.4 SAM — Segment Anything Model

```python
# pip install segment-anything
from segment_anything import sam_model_registry, SamPredictor
import numpy as np

sam = sam_model_registry['vit_b'](checkpoint='sam_vit_b_01ec64.pth')
predictor = SamPredictor(sam)

predictor.set_image(img_rgb)

# Prompt with point (x, y) + foreground/background label
input_point = np.array([[500, 375]])
input_label = np.array([1])   # 1=foreground, 0=background

masks, scores, logits = predictor.predict(
    point_coords=input_point,
    point_labels=input_label,
    multimask_output=True,  # returns 3 masks (small/medium/large)
)
best_mask = masks[np.argmax(scores)]   # (H, W) bool
```

---

## 9. OpenCV — 从核心到进阶

### 9.1 DNN 模块 — 无需 PyTorch/TF 运行模型

```python
# Load any ONNX model
net = cv2.dnn.readNetFromONNX('yolov8n.onnx')

# Use CUDA backend on GPU
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_CUDA)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CUDA)

# Or use OpenVINO for Intel CPUs
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_INFERENCE_ENGINE)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

# Preprocess image into blob
blob = cv2.dnn.blobFromImage(
    img_bgr,
    scalefactor=1.0/255,
    size=(640, 640),
    mean=(0, 0, 0),
    swapRB=True,        # BGR→RGB
    crop=False
)
net.setInput(blob)
outputs = net.forward(net.getUnconnectedOutLayersNames())
```

### 9.2 CUDA 加速

```python
# Check CUDA support at runtime
print(cv2.cuda.getCudaEnabledDeviceCount())   # 0 if no CUDA build

# Upload mat to GPU
gpu_mat = cv2.cuda_GpuMat()
gpu_mat.upload(img_gray)

# GPU operations
gpu_blurred = cv2.cuda.createGaussianFilter(
    cv2.CV_8UC1, cv2.CV_8UC1, (5, 5), 0
).apply(gpu_mat)

gpu_edges = cv2.cuda.createCannyEdgeDetector(50, 150).detect(gpu_mat)

# Download back to CPU
result = gpu_edges.download()
```

### 9.3 用 GStreamer 构建视频流水线

```python
# CSI camera on Jetson (zero-copy path)
pipeline = (
    "nvarguscamerasrc sensor-id=0 ! "
    "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! "
    "nvvidconv flip-method=0 ! "
    "video/x-raw, format=BGRx ! "
    "videoconvert ! video/x-raw, format=BGR ! appsink"
)
cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)

# RTSP stream input
rtsp_pipeline = "rtspsrc location=rtsp://192.168.1.50/stream ! decodebin ! videoconvert ! appsink"
cap = cv2.VideoCapture(rtsp_pipeline, cv2.CAP_GSTREAMER)
```

### 9.4 G-API — 面向流水线的 Graph API

G-API 允许把视觉流水线定义为计算图，从而实现自动优化与后端切换。

```python
import cv2 as cv

# Define the graph
g_in  = cv.GMat()
g_gray = cv.gapi.BGR2Gray(g_in)
g_blur = cv.gapi.gaussianBlur(g_gray, (5, 5), 0)
g_edges = cv.gapi.Canny(g_blur, 50, 150)

# Compile
pipeline = cv.GComputation(cv.GIn(g_in), cv.GOut(g_edges))
compiled = pipeline.compileStreaming()

compiled.setSource(cv.gin(cv.gapi.wip.make_capture_src(0)))
compiled.start()

while True:
    ok, (edges,) = compiled.pull()
    if not ok:
        break
    cv2.imshow('Edges', edges)
```

---

## 10. 标注工具

数据标注是监督学习的基础。标注的质量与格式直接决定模型性能。选择正确的工具取决于规模、团队人数和输出格式要求。

### 概览对比

| 工具 | 类型 | 价格 | AI 辅助 | 格式 | 适用场景 |
|------|------|-------|-----------|---------|----------|
| **X-AnyLabeling** | 桌面端 | 免费 | ✓ GPU | COCO, VOC, YOLO, DOTA, MOT, MASK | AI 辅助的单张/批量标注 |
| **CVAT** | Web / 自托管 | 免费 | ✓ | Pascal VOC, COCO, YOLO, Datumaro | 团队协作、视频跟踪 |
| **LabelImg** | 桌面端 | 免费 | ✗ | Pascal VOC, YOLO | 单人快速标注 |
| **RectLabel** | 桌面端（macOS） | 付费 | ✓ | COCO, VOC, YOLO | macOS 用户、快速多边形标注 |
| **VGG VIA** | 浏览器 | 免费 | ✗ | JSON, CSV | 轻量、无需安装 |
| **Labelbox** | 云 SaaS | 免费增值 | ✓ | COCO, VOC, YOLO + 自定义 | 企业级规模、团队管理 |
| **Roboflow** | 云 | 免费增值 | ✓ | YOLO, COCO, VOC, TFRecord, TensorFlow | 数据集版本管理、增强、训练与部署、边缘导出 |
| **COCO Annotator** | Web / 自托管 | 免费 | ✓（有限） | COCO JSON | COCO 格式分割数据集 |

---

### 10.1 X-AnyLabeling

X-AnyLabeling 是功能最丰富的开源标注工具——它直接内嵌 AI 推理引擎，可对图像和视频一键预测。

**GitHub:** [github.com/CVHub520/X-AnyLabeling](https://github.com/CVHub520/X-AnyLabeling)

#### 核心特性

```
AI-Assisted Annotation:
  Embedded models: YOLOv5/v8/v9/v10, SAM, GroundingDINO, DINO,
                   RT-DETR, CLIP, DepthAnything, and more
  GPU acceleration via ONNX Runtime with CUDA EP
  Single-frame prediction → manually correct → next frame
  Batch prediction → annotate entire folder in one click

Annotation Types:
  Polygons                  (fine-grained segmentation)
  Rectangles                (bounding boxes)
  Rotated boxes             (DOTA / aerial imagery)
  Circles
  Lines / polylines
  Points / keypoints

Format Support:
  Import:  COCO, VOC, YOLO, DOTA, MOT, MASK, LabelMe
  Export:  COCO, VOC, YOLO, DOTA, MOT, MASK, LabelMe, ODVG

Video Support:
  Frame-by-frame annotation
  Auto-tracking using embedded trackers
  Batch inference on video files
```

#### 安装

```bash
# Via pip (recommended)
pip install anylabeling

# With GPU support (ONNX Runtime CUDA)
pip install anylabeling[gpu]

# Or via release binary:
# Download from github.com/CVHub520/X-AnyLabeling/releases
# Available: .exe (Windows), .AppImage (Linux), .dmg (macOS)

# Run
anylabeling
```

#### 工作流

```
1. Open X-AnyLabeling
2. File → Open Dir → select your image folder
3. AI Models → select a model (e.g., YOLOv8n, SAM)
   - First use: model downloads automatically
4. Predict:
   - Single frame: Ctrl+M or click "Run Model"
   - Batch: Tools → Auto Labeling → Run on All Images
5. Review predictions, correct mistakes manually
6. File → Export → choose format (COCO / YOLO / VOC)
```

#### X-AnyLabeling 的导出格式

```bash
# YOLO format export example output:
# labels/image001.txt
# 0 0.512 0.334 0.241 0.189     ← class cx cy w h (normalized 0–1)

# COCO format export:
# annotations.json — single file with all boxes + images list

# VOC format export:
# Annotations/image001.xml — one XML per image

# DOTA format (rotated boxes for aerial imagery):
# labels/image001.txt
# 100 200 150 200 150 250 100 250 vehicle 0   ← 4 corner points + class + difficult
```

---


<details>
<summary>English original</summary>

**9.3 Video Pipeline with GStreamer**

```python
# CSI camera on Jetson (zero-copy path)
pipeline = (
    "nvarguscamerasrc sensor-id=0 ! "
    "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! "
    "nvvidconv flip-method=0 ! "
    "video/x-raw, format=BGRx ! "
    "videoconvert ! video/x-raw, format=BGR ! appsink"
)
cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)

# RTSP stream input
rtsp_pipeline = "rtspsrc location=rtsp://192.168.1.50/stream ! decodebin ! videoconvert ! appsink"
cap = cv2.VideoCapture(rtsp_pipeline, cv2.CAP_GSTREAMER)
```

**9.4 G-API — Graph API for Pipelines**

G-API lets you define your vision pipeline as a computation graph, enabling automatic optimization and backend switching.

```python
import cv2 as cv

# Define the graph
g_in  = cv.GMat()
g_gray = cv.gapi.BGR2Gray(g_in)
g_blur = cv.gapi.gaussianBlur(g_gray, (5, 5), 0)
g_edges = cv.gapi.Canny(g_blur, 50, 150)

# Compile
pipeline = cv.GComputation(cv.GIn(g_in), cv.GOut(g_edges))
compiled = pipeline.compileStreaming()

compiled.setSource(cv.gin(cv.gapi.wip.make_capture_src(0)))
compiled.start()

while True:
    ok, (edges,) = compiled.pull()
    if not ok:
        break
    cv2.imshow('Edges', edges)
```

---

**10. Annotation Tools**

Data annotation is the foundation of supervised learning. The quality and format of your annotations directly determines model performance. Choosing the right tool depends on your scale, team size, and output format requirements.

**Overview Comparison**

| Tool | Type | Price | AI-Assist | Formats | Best for |
|------|------|-------|-----------|---------|----------|
| **X-AnyLabeling** | Desktop | Free | ✓ GPU | COCO, VOC, YOLO, DOTA, MOT, MASK | AI-assisted single/batch annotation |
| **CVAT** | Web / Self-hosted | Free | ✓ | Pascal VOC, COCO, YOLO, Datumaro | Team collaboration, video tracking |
| **LabelImg** | Desktop | Free | ✗ | Pascal VOC, YOLO | Quick single-annotator labeling |
| **RectLabel** | Desktop (macOS) | Paid | ✓ | COCO, VOC, YOLO | macOS users, fast polygon annotation |
| **VGG VIA** | Browser | Free | ✗ | JSON, CSV | Lightweight, no install needed |
| **Labelbox** | Cloud SaaS | Freemium | ✓ | COCO, VOC, YOLO + custom | Enterprise scale, team management |
| **Roboflow** | Cloud | Freemium | ✓ | YOLO, COCO, VOC, TFRecord, TensorFlow | Dataset versioning, augmentation, train & deploy, edge export |
| **COCO Annotator** | Web / Self-hosted | Free | ✓ (limited) | COCO JSON | COCO-format segmentation datasets |

---

**10.1 X-AnyLabeling**

X-AnyLabeling is the most feature-rich open-source annotation tool — it embeds AI inference engines directly, enabling one-click predictions on images and videos.

**GitHub:** [github.com/CVHub520/X-AnyLabeling](https://github.com/CVHub520/X-AnyLabeling)

**Key Features**

```
AI-Assisted Annotation:
  Embedded models: YOLOv5/v8/v9/v10, SAM, GroundingDINO, DINO,
                   RT-DETR, CLIP, DepthAnything, and more
  GPU acceleration via ONNX Runtime with CUDA EP
  Single-frame prediction → manually correct → next frame
  Batch prediction → annotate entire folder in one click

Annotation Types:
  Polygons                  (fine-grained segmentation)
  Rectangles                (bounding boxes)
  Rotated boxes             (DOTA / aerial imagery)
  Circles
  Lines / polylines
  Points / keypoints

Format Support:
  Import:  COCO, VOC, YOLO, DOTA, MOT, MASK, LabelMe
  Export:  COCO, VOC, YOLO, DOTA, MOT, MASK, LabelMe, ODVG

Video Support:
  Frame-by-frame annotation
  Auto-tracking using embedded trackers
  Batch inference on video files
```

**Installation**

```bash
# Via pip (recommended)
pip install anylabeling

# With GPU support (ONNX Runtime CUDA)
pip install anylabeling[gpu]

# Or via release binary:
# Download from github.com/CVHub520/X-AnyLabeling/releases
# Available: .exe (Windows), .AppImage (Linux), .dmg (macOS)

# Run
anylabeling
```

**Workflow**

```
1. Open X-AnyLabeling
2. File → Open Dir → select your image folder
3. AI Models → select a model (e.g., YOLOv8n, SAM)
   - First use: model downloads automatically
4. Predict:
   - Single frame: Ctrl+M or click "Run Model"
   - Batch: Tools → Auto Labeling → Run on All Images
5. Review predictions, correct mistakes manually
6. File → Export → choose format (COCO / YOLO / VOC)
```

**Export Formats from X-AnyLabeling**

```bash
# YOLO format export example output:
# labels/image001.txt
# 0 0.512 0.334 0.241 0.189     ← class cx cy w h (normalized 0–1)

# COCO format export:
# annotations.json — single file with all boxes + images list

# VOC format export:
# Annotations/image001.xml — one XML per image

# DOTA format (rotated boxes for aerial imagery):
# labels/image001.txt
# 100 200 150 200 150 250 100 250 vehicle 0   ← 4 corner points + class + difficult
```

---

</details>

### 10.2 CVAT

CVAT（Computer Vision Annotation Tool）是业界标准的开源标注平台，由 Intel 开发，被各大 ML 团队大规模使用。

**GitHub：** [github.com/cvat-ai/cvat](https://github.com/cvat-ai/cvat)
**托管版本：** [app.cvat.ai](https://app.cvat.ai)

#### 主要特性

```
Annotation Types:
  Bounding boxes (2D + 3D)
  Polygons / polylines / points
  Masks (brush tool, superpixel segmentation)
  Keypoints + skeletons (pose estimation)
  Cuboids (3D objects in 2D images)
  LiDAR point cloud annotation (3D bboxes)

Video Annotation:
  Frame-by-frame OR annotate keyframes → interpolation fills gaps
  Semi-automatic tracking (SiamMask, OpenCV trackers)
  Object track IDs across frames (MOT format)

AI Assistance:
  SAM (Segment Anything) integration — click → mask
  Detection models via Nuclio serverless functions
  Interactors: DEXTR, f-BRS for polygon assistance

Collaboration:
  Role-based access: annotator / reviewer / owner
  Task assignment across multiple annotators
  Review workflow with accept/reject per annotation

Format Support:
  Export: Pascal VOC, COCO JSON, YOLO, TFRecord, MOT,
          Cityscapes, KITTI, LFW, Wider Face, VGGFace2
  Import: All of the above
```

#### 自托管部署

```bash
# Clone and start with Docker Compose
git clone https://github.com/cvat-ai/cvat.git
cd cvat

# Start all services
docker compose up -d

# Create admin user
docker exec -it cvat_server python manage.py createsuperuser

# Access at http://localhost:8080
```

#### CVAT Python SDK — 程序化标注

```python
# pip install cvat-sdk

from cvat_sdk import make_client
from cvat_sdk.models import TaskWriteRequest, DataRequest

with make_client(host='localhost', credentials=('user', 'password')) as client:
    # Create task
    task = client.tasks.create(TaskWriteRequest(
        name='My Detection Task',
        labels=[
            {'name': 'car',    'color': '#ff0000'},
            {'name': 'person', 'color': '#00ff00'},
        ]
    ))

    # Upload images
    task.upload_data(DataRequest(
        image_quality=95,
        server_files=['path/to/images/']
    ))

    # Export annotations
    task.export_dataset(
        format_name='COCO 1.0',
        filename='annotations.zip',
        include_images=False
    )
```

#### 从 CVAT 导出 — YOLO 格式结构

```
cvat_export/
├── obj_train_data/
│   ├── image001.txt       ← YOLO label file
│   ├── image002.txt
│   └── ...
├── obj.data               ← paths config
├── obj.names              ← class names, one per line
└── train.txt              ← list of image paths
```

---

### 10.3 LabelImg

LabelImg 是经典的轻量级单人标注工具 — 简单、久经考验、支持广泛。

**GitHub：** [github.com/HumanSignal/labelImg](https://github.com/HumanSignal/labelImg)

#### 安装

```bash
pip install labelImg

# Or from source
git clone https://github.com/HumanSignal/labelImg.git
cd labelImg
pip install pyqt5 lxml
python labelImg.py
```

#### 常用快捷键

| 按键 | 操作 |
|-----|--------|
| `W` | 创建矩形框 |
| `D` | 下一张图像 |
| `A` | 上一张图像 |
| `Ctrl+S` | 保存标注 |
| `Ctrl+R` | 更改保存目录 |
| `Del` | 删除选中的框 |

#### 工作流

```
1. Open Dir → select image folder
2. Change Save Dir → set label output folder
3. Toggle format: PascalVOC or YOLO (bottom-left of window)
4. Press W → draw box → enter class name → confirm
5. Ctrl+S → save → D (next image) → repeat
6. Result:
   YOLO:  labels/image001.txt   (class cx cy w h normalized)
   VOC:   Annotations/image001.xml
```

#### Pascal VOC XML 格式

```xml
<!-- Annotations/image001.xml -->
<annotation>
  <folder>images</folder>
  <filename>image001.jpg</filename>
  <size>
    <width>1920</width><height>1080</height><depth>3</depth>
  </size>
  <object>
    <name>car</name>
    <difficult>0</difficult>
    <bndbox>
      <xmin>452</xmin><ymin>78</ymin>
      <xmax>929</xmax><ymax>534</ymax>
    </bndbox>
  </object>
</annotation>
```

---

### 10.4 RectLabel

RectLabel 是 macOS 上精致的商业标注工具，专注于为单人标注者提供速度。

**网站：** [rectlabel.com](https://rectlabel.com)

#### 主要特性

```
Annotation Types:
  Bounding boxes
  Polygons
  Polylines
  Points
  Keypoints with skeleton templates
  Oriented bounding boxes

Unique Capabilities:
  Core ML integration — annotate with Apple Neural Engine
  Pre-labeling via Core ML detection models
  Tracking between video frames
  Label history / auto-complete class names
  Attribute support (color, material, truncated, occluded)

Export Formats:
  COCO JSON
  Pascal VOC XML
  YOLO
  CreateML JSON
  CSV

macOS Features:
  Native SwiftUI app (fast, no Electron overhead)
  Drag-and-drop image/video import
  Full keyboard shortcut customization
```


<details>
<summary>English original</summary>

**10.2 CVAT**

CVAT (Computer Vision Annotation Tool) is the industry-standard open-source annotation platform, developed by Intel and used at scale by major ML teams.

**GitHub:** [github.com/cvat-ai/cvat](https://github.com/cvat-ai/cvat)
**Hosted version:** [app.cvat.ai](https://app.cvat.ai)

**Key Features**

```
Annotation Types:
  Bounding boxes (2D + 3D)
  Polygons / polylines / points
  Masks (brush tool, superpixel segmentation)
  Keypoints + skeletons (pose estimation)
  Cuboids (3D objects in 2D images)
  LiDAR point cloud annotation (3D bboxes)

Video Annotation:
  Frame-by-frame OR annotate keyframes → interpolation fills gaps
  Semi-automatic tracking (SiamMask, OpenCV trackers)
  Object track IDs across frames (MOT format)

AI Assistance:
  SAM (Segment Anything) integration — click → mask
  Detection models via Nuclio serverless functions
  Interactors: DEXTR, f-BRS for polygon assistance

Collaboration:
  Role-based access: annotator / reviewer / owner
  Task assignment across multiple annotators
  Review workflow with accept/reject per annotation

Format Support:
  Export: Pascal VOC, COCO JSON, YOLO, TFRecord, MOT,
          Cityscapes, KITTI, LFW, Wider Face, VGGFace2
  Import: All of the above
```

**Self-Hosted Setup**

```bash
# Clone and start with Docker Compose
git clone https://github.com/cvat-ai/cvat.git
cd cvat

# Start all services
docker compose up -d

# Create admin user
docker exec -it cvat_server python manage.py createsuperuser

# Access at http://localhost:8080
```

**CVAT Python SDK — Programmatic Annotation**

```python
# pip install cvat-sdk

from cvat_sdk import make_client
from cvat_sdk.models import TaskWriteRequest, DataRequest

with make_client(host='localhost', credentials=('user', 'password')) as client:
    # Create task
    task = client.tasks.create(TaskWriteRequest(
        name='My Detection Task',
        labels=[
            {'name': 'car',    'color': '#ff0000'},
            {'name': 'person', 'color': '#00ff00'},
        ]
    ))

    # Upload images
    task.upload_data(DataRequest(
        image_quality=95,
        server_files=['path/to/images/']
    ))

    # Export annotations
    task.export_dataset(
        format_name='COCO 1.0',
        filename='annotations.zip',
        include_images=False
    )
```

**Export from CVAT — YOLO Format Structure**

```
cvat_export/
├── obj_train_data/
│   ├── image001.txt       ← YOLO label file
│   ├── image002.txt
│   └── ...
├── obj.data               ← paths config
├── obj.names              ← class names, one per line
└── train.txt              ← list of image paths
```

---

**10.3 LabelImg**

LabelImg is the classic, lightweight, single-annotator tool — simple, battle-tested, and universally supported.

**GitHub:** [github.com/HumanSignal/labelImg](https://github.com/HumanSignal/labelImg)

**Installation**

```bash
pip install labelImg

# Or from source
git clone https://github.com/HumanSignal/labelImg.git
cd labelImg
pip install pyqt5 lxml
python labelImg.py
```

**Key Shortcuts**

| Key | Action |
|-----|--------|
| `W` | Create rectangle box |
| `D` | Next image |
| `A` | Previous image |
| `Ctrl+S` | Save annotation |
| `Ctrl+R` | Change save directory |
| `Del` | Delete selected box |

**Workflow**

```
1. Open Dir → select image folder
2. Change Save Dir → set label output folder
3. Toggle format: PascalVOC or YOLO (bottom-left of window)
4. Press W → draw box → enter class name → confirm
5. Ctrl+S → save → D (next image) → repeat
6. Result:
   YOLO:  labels/image001.txt   (class cx cy w h normalized)
   VOC:   Annotations/image001.xml
```

**Pascal VOC XML Format**

```xml
<!-- Annotations/image001.xml -->
<annotation>
  <folder>images</folder>
  <filename>image001.jpg</filename>
  <size>
    <width>1920</width><height>1080</height><depth>3</depth>
  </size>
  <object>
    <name>car</name>
    <difficult>0</difficult>
    <bndbox>
      <xmin>452</xmin><ymin>78</ymin>
      <xmax>929</xmax><ymax>534</ymax>
    </bndbox>
  </object>
</annotation>
```

---

**10.4 RectLabel**

RectLabel is a polished commercial annotation tool for macOS, focused on speed for individual annotators.

**Website:** [rectlabel.com](https://rectlabel.com)

**Key Features**

```
Annotation Types:
  Bounding boxes
  Polygons
  Polylines
  Points
  Keypoints with skeleton templates
  Oriented bounding boxes

Unique Capabilities:
  Core ML integration — annotate with Apple Neural Engine
  Pre-labeling via Core ML detection models
  Tracking between video frames
  Label history / auto-complete class names
  Attribute support (color, material, truncated, occluded)

Export Formats:
  COCO JSON
  Pascal VOC XML
  YOLO
  CreateML JSON
  CSV

macOS Features:
  Native SwiftUI app (fast, no Electron overhead)
  Drag-and-drop image/video import
  Full keyboard shortcut customization
```

</details>

#### 何时使用 RectLabel

```
✓ Solo annotator on macOS
✓ Need fast Core ML pre-labeling (uses Apple Silicon Neural Engine)
✓ Dataset has oriented/rotated objects (satellite, aerial)
✓ Need keypoint/skeleton annotation (sports, medical)

✗ Team collaboration (single user license)
✗ Linux/Windows (macOS only)
✗ Free budget required
```

---

### 10.5 VGG Image Annotator (VIA)

VIA 是一款免安装的浏览器端标注工具——只需打开一个 HTML 文件即可开始标注。由 Oxford VGG 开发。

**网站：** [robots.ox.ac.uk/~vgg/software/via/](https://www.robots.ox.ac.uk/~vgg/software/via/)
**GitHub：** [github.com/ox-vgg/via](https://github.com/ox-vgg/via)

#### 核心特性

```
No installation:
  Single HTML file — open in any browser, works offline
  Load local images directly (no server upload needed)
  All data stays on your machine

Annotation Types:
  Bounding boxes
  Polygons
  Polylines
  Points
  Circles / ellipses

Attributes:
  Define custom attributes per region (class, color, quality, etc.)
  Checkbox / radio / text / number attribute types

Export Formats:
  VIA JSON (native format)
  CSV (one row per annotation)
  COCO JSON (via VIA3 version)
```

#### 用法

```
1. Download via.html from robots.ox.ac.uk/~vgg/software/via/
2. Open in browser (no server needed)
3. Add Files → select images from your disk
4. Define attributes (e.g., "class": type=radio, options=car,person,bike)
5. Draw regions: select shape → draw → set attribute values
6. Annotations → Export → JSON or CSV
```

#### 用 Python 解析 VIA JSON

```python
import json

with open('via_annotations.json') as f:
    via_data = json.load(f)

for file_key, file_data in via_data['_via_img_metadata'].items():
    filename = file_data['filename']
    for region in file_data['regions']:
        shape = region['shape_attributes']
        attrs = region['region_attributes']
        class_name = attrs.get('class', 'unknown')

        if shape['name'] == 'rect':
            x, y = shape['x'], shape['y']
            w, h = shape['width'], shape['height']
            print(f"{filename}: {class_name} box ({x},{y}) {w}×{h}")
        elif shape['name'] == 'polygon':
            xs = shape['all_points_x']
            ys = shape['all_points_y']
            print(f"{filename}: {class_name} polygon with {len(xs)} points")
```

---

### 10.6 Labelbox

Labelbox 是面向大规模数据标注的企业级云平台——支持协作、由 API 驱动，并与 MLOps 流水线集成。

**网站：** [labelbox.com](https://labelbox.com)

#### 核心特性

```
Scale:
  Manage thousands of annotators
  Built-in quality control (review workflows, consensus)
  Nested task queues with priority

Automation:
  Model-assisted labeling (MAL): run your model, human corrects
  DINO, SAM integration for pre-labeling
  Active learning: prioritize uncertain/valuable examples

API-First:
  Full REST + GraphQL API
  Python SDK for programmatic dataset management
  Webhooks for real-time events

Export Formats:
  COCO, Pascal VOC, YOLO
  Custom format via API
  Direct integration: HuggingFace, Roboflow, AWS SageMaker

Pricing:
  Free: 5 users, 1 project, limited storage
  Team: $0.06–0.12/label
  Enterprise: custom pricing
```

#### Labelbox Python SDK

```python
# pip install labelbox

import labelbox as lb

client = lb.Client(api_key='YOUR_API_KEY')

# Create dataset
dataset = client.create_dataset(name='MyDetectionDataset')

# Upload images from URLs
dataset.create_data_rows([
    {'row_data': 'https://example.com/image001.jpg', 'global_key': 'img001'},
    {'row_data': 'https://example.com/image002.jpg', 'global_key': 'img002'},
])

# Create labeling project
ontology_builder = lb.OntologyBuilder(
    tools=[
        lb.Tool(tool=lb.Tool.Type.BOUNDING_BOX, name='car'),
        lb.Tool(tool=lb.Tool.Type.BOUNDING_BOX, name='person'),
        lb.Tool(tool=lb.Tool.Type.POLYGON, name='road'),
    ]
)
ontology = client.create_ontology('Detection Ontology', ontology_builder.asdict())

project = client.create_project(
    name='Warehouse Detection',
    media_type=lb.MediaType.Image
)
project.setup_editor(ontology)
project.create_batch('batch-1', dataset.export_data_rows(), 5)

# Export completed labels
export_task = project.export(params={
    'data_row_details': True,
    'metadata_fields': True,
    'attachments': False,
    'project_details': True,
    'performance_details': True,
    'label_details': True
})
export_task.wait_till_done()

for row in export_task.get_buffered_stream():
    label = row.json
    for annotation in label.get('projects', {}).get(project.uid, {}).get('labels', []):
        for obj in annotation.get('annotations', {}).get('objects', []):
            print(obj['value'], obj['bounding_box'])
```

---


<details>
<summary>English original</summary>

**When to Use RectLabel**

```
✓ Solo annotator on macOS
✓ Need fast Core ML pre-labeling (uses Apple Silicon Neural Engine)
✓ Dataset has oriented/rotated objects (satellite, aerial)
✓ Need keypoint/skeleton annotation (sports, medical)

✗ Team collaboration (single user license)
✗ Linux/Windows (macOS only)
✗ Free budget required
```

---

**10.5 VGG Image Annotator (VIA)**

VIA is a zero-install browser-based annotator — just open an HTML file and start labeling. Developed by Oxford VGG.

**Website:** [robots.ox.ac.uk/~vgg/software/via/](https://www.robots.ox.ac.uk/~vgg/software/via/)
**GitHub:** [github.com/ox-vgg/via](https://github.com/ox-vgg/via)

**Key Features**

```
No installation:
  Single HTML file — open in any browser, works offline
  Load local images directly (no server upload needed)
  All data stays on your machine

Annotation Types:
  Bounding boxes
  Polygons
  Polylines
  Points
  Circles / ellipses

Attributes:
  Define custom attributes per region (class, color, quality, etc.)
  Checkbox / radio / text / number attribute types

Export Formats:
  VIA JSON (native format)
  CSV (one row per annotation)
  COCO JSON (via VIA3 version)
```

**Usage**

```
1. Download via.html from robots.ox.ac.uk/~vgg/software/via/
2. Open in browser (no server needed)
3. Add Files → select images from your disk
4. Define attributes (e.g., "class": type=radio, options=car,person,bike)
5. Draw regions: select shape → draw → set attribute values
6. Annotations → Export → JSON or CSV
```

**Parse VIA JSON in Python**

```python
import json

with open('via_annotations.json') as f:
    via_data = json.load(f)

for file_key, file_data in via_data['_via_img_metadata'].items():
    filename = file_data['filename']
    for region in file_data['regions']:
        shape = region['shape_attributes']
        attrs = region['region_attributes']
        class_name = attrs.get('class', 'unknown')

        if shape['name'] == 'rect':
            x, y = shape['x'], shape['y']
            w, h = shape['width'], shape['height']
            print(f"{filename}: {class_name} box ({x},{y}) {w}×{h}")
        elif shape['name'] == 'polygon':
            xs = shape['all_points_x']
            ys = shape['all_points_y']
            print(f"{filename}: {class_name} polygon with {len(xs)} points")
```

---

**10.6 Labelbox**

Labelbox is an enterprise cloud platform for large-scale data labeling — collaborative, API-driven, and integrated with MLOps pipelines.

**Website:** [labelbox.com](https://labelbox.com)

**Key Features**

```
Scale:
  Manage thousands of annotators
  Built-in quality control (review workflows, consensus)
  Nested task queues with priority

Automation:
  Model-assisted labeling (MAL): run your model, human corrects
  DINO, SAM integration for pre-labeling
  Active learning: prioritize uncertain/valuable examples

API-First:
  Full REST + GraphQL API
  Python SDK for programmatic dataset management
  Webhooks for real-time events

Export Formats:
  COCO, Pascal VOC, YOLO
  Custom format via API
  Direct integration: HuggingFace, Roboflow, AWS SageMaker

Pricing:
  Free: 5 users, 1 project, limited storage
  Team: $0.06–0.12/label
  Enterprise: custom pricing
```

**Labelbox Python SDK**

```python
# pip install labelbox

import labelbox as lb

client = lb.Client(api_key='YOUR_API_KEY')

# Create dataset
dataset = client.create_dataset(name='MyDetectionDataset')

# Upload images from URLs
dataset.create_data_rows([
    {'row_data': 'https://example.com/image001.jpg', 'global_key': 'img001'},
    {'row_data': 'https://example.com/image002.jpg', 'global_key': 'img002'},
])

# Create labeling project
ontology_builder = lb.OntologyBuilder(
    tools=[
        lb.Tool(tool=lb.Tool.Type.BOUNDING_BOX, name='car'),
        lb.Tool(tool=lb.Tool.Type.BOUNDING_BOX, name='person'),
        lb.Tool(tool=lb.Tool.Type.POLYGON, name='road'),
    ]
)
ontology = client.create_ontology('Detection Ontology', ontology_builder.asdict())

project = client.create_project(
    name='Warehouse Detection',
    media_type=lb.MediaType.Image
)
project.setup_editor(ontology)
project.create_batch('batch-1', dataset.export_data_rows(), 5)

# Export completed labels
export_task = project.export(params={
    'data_row_details': True,
    'metadata_fields': True,
    'attachments': False,
    'project_details': True,
    'performance_details': True,
    'label_details': True
})
export_task.wait_till_done()

for row in export_task.get_buffered_stream():
    label = row.json
    for annotation in label.get('projects', {}).get(project.uid, {}).get('labels', []):
        for obj in annotation.get('annotations', {}).get('objects', []):
            print(obj['value'], obj['bounding_box'])
```

---

</details>

### 10.7 Roboflow

Roboflow 是一个用于构建计算机视觉数据集与流水线的云平台——标注、增强、版本管理，并导出为 YOLO/COCO/VOC，然后在云端训练，或导出以进行本地/边缘部署（Jetson、TensorRT、ONNX、TFLite）。

**网站：** [roboflow.com](https://roboflow.com)

#### 主要特性

```
Dataset Management:
  Upload images/video; annotate in browser (boxes, polygons, keypoints)
  Dataset versioning — track changes and augmentations per version
  Train/valid/test split with one click
  Public dataset catalog (COCO, Open Images, etc.) and custom uploads

Augmentation & Preprocessing:
  Built-in augmentations (flip, rotate, brightness, mosaic, etc.)
  Auto-orientation and resize for target model input
  Generate new versions with different augmentations without re-labeling

Export & Deployment:
  Export: YOLO (v5/v8/v11), COCO, Pascal VOC, TFRecord, TensorFlow, Create ML
  Train in Roboflow (YOLOv8, etc.) or download dataset for local training
  Deploy: Roboflow API, ONNX, TensorRT, TFLite, Core ML, browser (JavaScript)
  Edge: direct export for Jetson, Raspberry Pi, and embedded targets

API & Integrations:
  Python SDK (roboflow) for upload, annotation, version, and inference
  REST API for pipelines and MLOps
  Integrates with Labelbox, CVAT (import/export), and major frameworks
```

#### 何时使用 Roboflow

- 希望有一个统一的地方来管理数据集版本、做增强，并导出为 YOLO/COCO 以用于训练和边缘部署。
- 需要快速迭代：标注 → 增强 → 导出 → 训练（本地或云端）→ 部署到 Jetson/边缘。
- 比起自托管 CVAT 或自行管理原始文件和脚本，更倾向于托管式流水线。

#### 快速上手（Python）

```python
# pip install roboflow

from roboflow import Roboflow

rf = Roboflow(api_key="YOUR_API_KEY")
project = rf.workspace("my-workspace").project("my-project")
dataset = project.version(1).download("yolov8")   # or "coco", "voc", etc.

# Inference with hosted model
model = project.version(1).model
pred = model.predict("image.jpg")
print(pred.json())
```

---

### 10.8 COCO Annotator

COCO Annotator 是一款专为 COCO 数据集格式构建的开源 Web 标注工具——原生支持分割掩码。

**GitHub：** [github.com/jsbroks/coco-annotator](https://github.com/jsbroks/coco-annotator)

#### 主要特性

```
Annotation Types:
  Bounding boxes
  Segmentation masks (polygon)
  Keypoints

COCO-Native:
  Stores directly in COCO JSON format
  Supports supercategory hierarchy
  Proper instance segmentation with "iscrowd" flag

Smart Tools:
  Magic Wand (color-based region fill)
  Superpixel-based selection (SLIC segments)
  Polygon simplification

Collaboration:
  User accounts with dataset permissions
  Multi-user concurrent annotation
  Export at any time (live COCO JSON)
```

#### Docker 配置

```bash
git clone https://github.com/jsbroks/coco-annotator.git
cd coco-annotator

# Start with Docker Compose (includes MongoDB + Flask + Vue frontend)
docker-compose up -d

# Access at http://localhost:5000
# Default credentials: admin / admin

# Mount your images directory
# Edit docker-compose.yml:
# volumes:
#   - /path/to/your/images:/datasets
```

#### 导出并加载 COCO JSON

```python
import json

# Load COCO format annotations
with open('annotations.json') as f:
    coco = json.load(f)

# COCO structure:
# {
#   "images":      [...],   ← image metadata (id, file_name, width, height)
#   "annotations": [...],   ← boxes/masks (image_id, category_id, bbox, segmentation, area)
#   "categories":  [...],   ← class list (id, name, supercategory)
# }

# Build lookup maps
cat_map = {c['id']: c['name'] for c in coco['categories']}
img_map = {i['id']: i['file_name'] for i in coco['images']}

# Iterate annotations
for ann in coco['annotations']:
    img_name = img_map[ann['image_id']]
    class_name = cat_map[ann['category_id']]
    x, y, w, h = ann['bbox']   # COCO bbox: x_min, y_min, width, height
    area = ann['area']
    segmentation = ann.get('segmentation', [])   # list of polygon vertex lists
    is_crowd = ann.get('iscrowd', 0)             # 1=RLE mask, 0=polygon

    print(f"{img_name}: {class_name} at [{x:.0f},{y:.0f},{x+w:.0f},{y+h:.0f}]")
```

---

### 10.9 标注工作流最佳实践

#### 质量控制

```python
# Check annotation quality: count boxes per image, flag outliers
import json
from collections import Counter

with open('annotations.json') as f:
    coco = json.load(f)

boxes_per_image = Counter()
for ann in coco['annotations']:
    boxes_per_image[ann['image_id']] += 1

# Flag images with too few or too many boxes
avg = sum(boxes_per_image.values()) / len(boxes_per_image)
for img_id, count in boxes_per_image.items():
    if count < avg * 0.2 or count > avg * 3:
        img = next(i for i in coco['images'] if i['id'] == img_id)
        print(f"REVIEW: {img['file_name']} has {count} boxes (avg={avg:.1f})")
```


<details>
<summary>English original</summary>

**10.7 Roboflow**

Roboflow is a cloud platform for building computer vision datasets and pipelines — annotate, augment, version, and export to YOLO/COCO/VOC, then train in the cloud or export for local/edge deployment (Jetson, TensorRT, ONNX, TFLite).

**Website:** [roboflow.com](https://roboflow.com)

**Key Features**

```
Dataset Management:
  Upload images/video; annotate in browser (boxes, polygons, keypoints)
  Dataset versioning — track changes and augmentations per version
  Train/valid/test split with one click
  Public dataset catalog (COCO, Open Images, etc.) and custom uploads

Augmentation & Preprocessing:
  Built-in augmentations (flip, rotate, brightness, mosaic, etc.)
  Auto-orientation and resize for target model input
  Generate new versions with different augmentations without re-labeling

Export & Deployment:
  Export: YOLO (v5/v8/v11), COCO, Pascal VOC, TFRecord, TensorFlow, Create ML
  Train in Roboflow (YOLOv8, etc.) or download dataset for local training
  Deploy: Roboflow API, ONNX, TensorRT, TFLite, Core ML, browser (JavaScript)
  Edge: direct export for Jetson, Raspberry Pi, and embedded targets

API & Integrations:
  Python SDK (roboflow) for upload, annotation, version, and inference
  REST API for pipelines and MLOps
  Integrates with Labelbox, CVAT (import/export), and major frameworks
```

**When to Use Roboflow**

- You want one place to version datasets, augment, and export to YOLO/COCO for training and edge deployment.
- You need quick iteration: annotate → augment → export → train (locally or in cloud) → deploy to Jetson/edge.
- You prefer a managed pipeline over self-hosting CVAT or managing raw files and scripts.

**Quick Start (Python)**

```python
# pip install roboflow

from roboflow import Roboflow

rf = Roboflow(api_key="YOUR_API_KEY")
project = rf.workspace("my-workspace").project("my-project")
dataset = project.version(1).download("yolov8")   # or "coco", "voc", etc.

# Inference with hosted model
model = project.version(1).model
pred = model.predict("image.jpg")
print(pred.json())
```

---

**10.8 COCO Annotator**

COCO Annotator is an open-source web-based annotation tool built specifically for the COCO dataset format — supports segmentation masks natively.

**GitHub:** [github.com/jsbroks/coco-annotator](https://github.com/jsbroks/coco-annotator)

**Key Features**

```
Annotation Types:
  Bounding boxes
  Segmentation masks (polygon)
  Keypoints

COCO-Native:
  Stores directly in COCO JSON format
  Supports supercategory hierarchy
  Proper instance segmentation with "iscrowd" flag

Smart Tools:
  Magic Wand (color-based region fill)
  Superpixel-based selection (SLIC segments)
  Polygon simplification

Collaboration:
  User accounts with dataset permissions
  Multi-user concurrent annotation
  Export at any time (live COCO JSON)
```

**Docker Setup**

```bash
git clone https://github.com/jsbroks/coco-annotator.git
cd coco-annotator

# Start with Docker Compose (includes MongoDB + Flask + Vue frontend)
docker-compose up -d

# Access at http://localhost:5000
# Default credentials: admin / admin

# Mount your images directory
# Edit docker-compose.yml:
# volumes:
#   - /path/to/your/images:/datasets
```

**Export and Load COCO JSON**

```python
import json

# Load COCO format annotations
with open('annotations.json') as f:
    coco = json.load(f)

# COCO structure:
# {
#   "images":      [...],   ← image metadata (id, file_name, width, height)
#   "annotations": [...],   ← boxes/masks (image_id, category_id, bbox, segmentation, area)
#   "categories":  [...],   ← class list (id, name, supercategory)
# }

# Build lookup maps
cat_map = {c['id']: c['name'] for c in coco['categories']}
img_map = {i['id']: i['file_name'] for i in coco['images']}

# Iterate annotations
for ann in coco['annotations']:
    img_name = img_map[ann['image_id']]
    class_name = cat_map[ann['category_id']]
    x, y, w, h = ann['bbox']   # COCO bbox: x_min, y_min, width, height
    area = ann['area']
    segmentation = ann.get('segmentation', [])   # list of polygon vertex lists
    is_crowd = ann.get('iscrowd', 0)             # 1=RLE mask, 0=polygon

    print(f"{img_name}: {class_name} at [{x:.0f},{y:.0f},{x+w:.0f},{y+h:.0f}]")
```

---

**10.9 Annotation Workflow Best Practices**

**Quality Control**

```python
# Check annotation quality: count boxes per image, flag outliers
import json
from collections import Counter

with open('annotations.json') as f:
    coco = json.load(f)

boxes_per_image = Counter()
for ann in coco['annotations']:
    boxes_per_image[ann['image_id']] += 1

# Flag images with too few or too many boxes
avg = sum(boxes_per_image.values()) / len(boxes_per_image)
for img_id, count in boxes_per_image.items():
    if count < avg * 0.2 or count > avg * 3:
        img = next(i for i in coco['images'] if i['id'] == img_id)
        print(f"REVIEW: {img['file_name']} has {count} boxes (avg={avg:.1f})")
```

</details>

#### 数据集划分

```python
import random
from pathlib import Path
import shutil

def split_dataset(image_dir: str, label_dir: str,
                  train=0.7, val=0.2, test=0.1, seed=42):
    """Split YOLO-format dataset into train/val/test."""
    images = list(Path(image_dir).glob('*.jpg')) + \
             list(Path(image_dir).glob('*.png'))
    random.seed(seed)
    random.shuffle(images)

    n = len(images)
    splits = {
        'train': images[:int(n*train)],
        'val':   images[int(n*train):int(n*(train+val))],
        'test':  images[int(n*(train+val)):],
    }

    for split_name, imgs in splits.items():
        img_out = Path(f'dataset/{split_name}/images')
        lbl_out = Path(f'dataset/{split_name}/labels')
        img_out.mkdir(parents=True, exist_ok=True)
        lbl_out.mkdir(parents=True, exist_ok=True)

        for img_path in imgs:
            lbl_path = Path(label_dir) / (img_path.stem + '.txt')
            shutil.copy(img_path, img_out / img_path.name)
            if lbl_path.exists():
                shutil.copy(lbl_path, lbl_out / lbl_path.name)

    print(f"Split: train={len(splits['train'])}, "
          f"val={len(splits['val'])}, test={len(splits['test'])}")

split_dataset('images', 'labels')
```

---

## 11. 数据集格式

### 11.1 COCO JSON

最完整的格式——支持检测、分割、关键点、图像描述。

```json
{
  "info": {"description": "My dataset", "version": "1.0", "year": 2025},
  "licenses": [],
  "categories": [
    {"id": 1, "name": "car",    "supercategory": "vehicle"},
    {"id": 2, "name": "person", "supercategory": "human"}
  ],
  "images": [
    {"id": 1, "file_name": "image001.jpg", "width": 1920, "height": 1080}
  ],
  "annotations": [
    {
      "id": 1,
      "image_id": 1,
      "category_id": 1,
      "bbox": [452, 78, 477, 456],
      "area": 217332,
      "segmentation": [[452,78, 929,78, 929,534, 452,534]],
      "iscrowd": 0
    }
  ]
}
```

### 11.2 YOLO 格式

每张图像一个 `.txt` 文件。类别索引从 0 开始。所有值归一化到 0–1。

```
# labels/image001.txt
# class_id  cx      cy      width   height
0            0.5130  0.2833  0.2474  0.4222
1            0.1250  0.5370  0.0625  0.1667
```

```python
# Convert YOLO label to pixel coordinates
def yolo_to_pixel(cx, cy, w, h, img_w, img_h):
    x1 = int((cx - w/2) * img_w)
    y1 = int((cy - h/2) * img_h)
    x2 = int((cx + w/2) * img_w)
    y2 = int((cy + h/2) * img_h)
    return x1, y1, x2, y2
```

### 11.3 Pascal VOC XML

```xml
<annotation>
  <folder>images</folder>
  <filename>image001.jpg</filename>
  <size><width>1920</width><height>1080</height><depth>3</depth></size>
  <object>
    <name>car</name>
    <pose>Unspecified</pose>
    <truncated>0</truncated>
    <difficult>0</difficult>
    <bndbox><xmin>452</xmin><ymin>78</ymin><xmax>929</xmax><ymax>534</ymax></bndbox>
  </object>
</annotation>
```

### 11.4 MOT 格式（多目标跟踪）

```
# mot_labels/image001.txt
# frame_id, track_id, x, y, w, h, confidence, class, visibility
1, 1, 452, 78, 477, 456, 1.0, 1, 1.0
1, 2, 120, 200,  60, 180, 0.9, 2, 0.8
```

### 11.5 格式转换工具

```python
# Roboflow — free web tool for format conversion
# Or use supervision library:
# pip install supervision

import supervision as sv

# Load COCO
ds = sv.DetectionDataset.from_coco(
    images_directory_path='images/',
    annotations_path='annotations.json'
)

# Save as YOLO
ds.as_yolo(
    images_directory_path='yolo/images/',
    annotations_directory_path='yolo/labels/',
    data_yaml_path='yolo/data.yaml'
)

# Save as Pascal VOC
ds.as_pascal_voc(
    images_directory_path='voc/images/',
    annotations_directory_path='voc/Annotations/'
)
```

---

## 12. 模型训练流水线

### 端到端工作流

```
1. Collect images
      ↓
2. Annotate (X-AnyLabeling / CVAT)
      ↓
3. Export in target format (YOLO / COCO)
      ↓
4. Split dataset (70/20/10 train/val/test)
      ↓
5. Augment (Albumentations / YOLOv8 built-in)
      ↓
6. Train (YOLOv8 / TAO Toolkit / custom PyTorch)
      ↓
7. Evaluate (mAP@50, mAP@50:95, confusion matrix)
      ↓
8. Optimize (TensorRT / tinygrad quantization)
      ↓
9. Deploy (Jetson / DeepStream / ROS2)
```

### 用 Albumentations 做增强

```python
import albumentations as A
from albumentations.pytorch import ToTensorV2

train_transform = A.Compose([
    A.RandomResizedCrop(640, 640, scale=(0.5, 1.0)),
    A.HorizontalFlip(p=0.5),
    A.ColorJitter(brightness=0.4, contrast=0.4, saturation=0.4, hue=0.1, p=0.8),
    A.GaussNoise(var_limit=(10, 50), p=0.3),
    A.MotionBlur(blur_limit=5, p=0.2),
    A.RandomRain(p=0.1),
    A.Normalize(mean=(0.485, 0.456, 0.406), std=(0.229, 0.224, 0.225)),
    ToTensorV2(),
], bbox_params=A.BboxParams(format='yolo', label_fields=['class_labels']))
```

---

## 13. 项目

### Project 1：从标注到部署的完整流水线

构建一个仓库安全检测器（人、叉车、安全帽），端到端完成。

```
1. Collect 500 images from warehouse video
2. Annotate with X-AnyLabeling (YOLOv8 pre-label → human correction)
3. Export as YOLO format, split 70/20/10
4. Fine-tune YOLOv8n for 100 epochs
5. Evaluate: target mAP@50 > 85%
6. Export to TensorRT INT8 for Jetson
7. Run at 30+ FPS on Jetson Orin Nano with ROS2 publisher

Deliverable: GitHub repo + annotated dataset on Roboflow Universe
```

### Project 2：多摄像头行人重识别

在 FOV 不重叠的 4 路摄像头间跟踪同一个人。

```
Components:
  4 cameras → YOLOv8 person detection
           → Re-ID embedding (OSNet, MobileNetV2 backbone)
           → Hungarian algorithm cross-camera matching
           → Redis store for active identities

Annotation: label 1000 person crops per camera with CVAT
Training: contrastive loss on person pairs (same/different identity)
```

### Project 3：基于旋转框的航拍目标检测

用有向边界框在卫星影像中检测车辆与建筑。

```
Dataset: DOTA dataset (aerial images, 15 categories, rotated boxes)
Tool: X-AnyLabeling DOTA export
Model: YOLOv8-OBB (oriented bounding box variant)
Metric: mAP@50 with rotated IoU

uv run ultralytics train model=yolov8n-obb.pt data=dota.yaml epochs=100
```

### Project 4：实时姿态估计

```python
# YOLOv8 pose — 17 keypoints (COCO skeleton)
from ultralytics import YOLO

model = YOLO('yolov8n-pose.pt')
results = model('video.mp4', stream=True)

for r in results:
    if r.keypoints is not None:
        kpts = r.keypoints.xy.numpy()   # (N_persons, 17, 2)
        # kpts[0, 0] = nose, [0, 5] = left shoulder, [0, 11] = left hip
        for person_kpts in kpts:
            nose = person_kpts[0]
            left_shoulder = person_kpts[5]
            right_shoulder = person_kpts[6]
            # Compute angle, detect fall, measure posture
```

### Project 5：基于 PointNet 的 3D 场景理解

```python
# pip install torch-geometric
import torch
import torch.nn as nn

class PointNet(nn.Module):
    """Classify a point cloud into one of N classes."""
    def __init__(self, num_classes: int):
        super().__init__()
        self.mlp1 = nn.Sequential(
            nn.Conv1d(3, 64, 1), nn.BatchNorm1d(64), nn.ReLU(),
            nn.Conv1d(64, 128, 1), nn.BatchNorm1d(128), nn.ReLU(),
            nn.Conv1d(128, 1024, 1), nn.BatchNorm1d(1024), nn.ReLU(),
        )
        self.fc = nn.Sequential(
            nn.Linear(1024, 512), nn.ReLU(), nn.Dropout(0.3),
            nn.Linear(512, 256), nn.ReLU(), nn.Dropout(0.3),
            nn.Linear(256, num_classes)
        )

    def forward(self, x):  # x: (B, 3, N)
        x = self.mlp1(x)
        x = x.max(dim=-1).values   # global max pooling: (B, 1024)
        return self.fc(x)
```

---

## 14. 资源

### 基础

- **《Computer Vision: Algorithms and Applications》** — Richard Szeliski（在线免费）：最完整的计算机视觉教材，从针孔相机到神经网络无所不包
- **《Programming Computer Vision with Python》** — Jan Erik Solem：实用的 OpenCV 项目
- **CS231n：Convolutional Neural Networks for Visual Recognition**（Stanford，免费）：最经典的卷积神经网络课程，讲义质量极佳
- **《Deep Learning for Vision Systems》** — Mohamed Elgendy：CNN 架构与实用代码

### 论文

| 主题 | 论文 |
|-------|-------|
| YOLO 原始论文 | Redmon et al., "You Only Look Once" (CVPR 2016) |
| YOLOv8 | Jocher et al., Ultralytics YOLOv8 (2023) |
| Faster R-CNN | Ren et al., "Faster R-CNN" (NeurIPS 2015) |
| ViT | Dosovitskiy et al., "An Image is Worth 16×16 Words" (ICLR 2021) |
| DETR | Carion et al., "End-to-End Object Detection with Transformers" (ECCV 2020) |
| SAM | Kirillov et al., "Segment Anything" (ICCV 2023) |
| DeepLab v3+ | Chen et al., "Encoder-Decoder with Atrous Separable Convolution" (ECCV 2018) |
| SegFormer | Xie et al., "SegFormer: Simple and Efficient Design for Semantic Segmentation" (NeurIPS 2021) |
| Mask2Former | Cheng et al., "Masked-attention Mask Transformer" (CVPR 2022) |
| BEVFusion | Liu et al., "BEVFusion" (ICRA 2023) |
| ByteTrack | Zhang et al., "ByteTrack" (ECCV 2022) |

### 工具与库

- **OpenCV** — docs.opencv.org — 核心库
- **Halide** — [github.com/halide/Halide](https://github.com/halide/Halide) — 用于快速、可移植数据并行图像处理的语言（CPU、GPU、FPGA）
- **Ultralytics YOLOv8** — docs.ultralytics.com — 现代检测/分割/姿态
- **Albumentations** — albumentations.ai — 数据增强
- **Supervision** — supervision.roboflow.com — 检测工具、可视化
- **Roboflow** — roboflow.com — 数据集托管、格式转换、数据增强


<details>
<summary>English original</summary>

**Project 1: Full Annotation-to-Deployment Pipeline**

Build a warehouse safety detector (person, forklift, hard hat) end-to-end.

```
1. Collect 500 images from warehouse video
2. Annotate with X-AnyLabeling (YOLOv8 pre-label → human correction)
3. Export as YOLO format, split 70/20/10
4. Fine-tune YOLOv8n for 100 epochs
5. Evaluate: target mAP@50 > 85%
6. Export to TensorRT INT8 for Jetson
7. Run at 30+ FPS on Jetson Orin Nano with ROS2 publisher

Deliverable: GitHub repo + annotated dataset on Roboflow Universe
```

**Project 2: Multi-Camera People Re-Identification**

Track the same person across 4 cameras without overlapping FOVs.

```
Components:
  4 cameras → YOLOv8 person detection
           → Re-ID embedding (OSNet, MobileNetV2 backbone)
           → Hungarian algorithm cross-camera matching
           → Redis store for active identities

Annotation: label 1000 person crops per camera with CVAT
Training: contrastive loss on person pairs (same/different identity)
```

**Project 3: Aerial Object Detection with Rotated Boxes**

Detect vehicles and buildings in satellite imagery using oriented bounding boxes.

```
Dataset: DOTA dataset (aerial images, 15 categories, rotated boxes)
Tool: X-AnyLabeling DOTA export
Model: YOLOv8-OBB (oriented bounding box variant)
Metric: mAP@50 with rotated IoU

uv run ultralytics train model=yolov8n-obb.pt data=dota.yaml epochs=100
```

**Project 4: Real-Time Pose Estimation**

```python
# YOLOv8 pose — 17 keypoints (COCO skeleton)
from ultralytics import YOLO

model = YOLO('yolov8n-pose.pt')
results = model('video.mp4', stream=True)

for r in results:
    if r.keypoints is not None:
        kpts = r.keypoints.xy.numpy()   # (N_persons, 17, 2)
        # kpts[0, 0] = nose, [0, 5] = left shoulder, [0, 11] = left hip
        for person_kpts in kpts:
            nose = person_kpts[0]
            left_shoulder = person_kpts[5]
            right_shoulder = person_kpts[6]
            # Compute angle, detect fall, measure posture
```

**Project 5: 3D Scene Understanding with PointNet**

```python
# pip install torch-geometric
import torch
import torch.nn as nn

class PointNet(nn.Module):
    """Classify a point cloud into one of N classes."""
    def __init__(self, num_classes: int):
        super().__init__()
        self.mlp1 = nn.Sequential(
            nn.Conv1d(3, 64, 1), nn.BatchNorm1d(64), nn.ReLU(),
            nn.Conv1d(64, 128, 1), nn.BatchNorm1d(128), nn.ReLU(),
            nn.Conv1d(128, 1024, 1), nn.BatchNorm1d(1024), nn.ReLU(),
        )
        self.fc = nn.Sequential(
            nn.Linear(1024, 512), nn.ReLU(), nn.Dropout(0.3),
            nn.Linear(512, 256), nn.ReLU(), nn.Dropout(0.3),
            nn.Linear(256, num_classes)
        )

    def forward(self, x):  # x: (B, 3, N)
        x = self.mlp1(x)
        x = x.max(dim=-1).values   # global max pooling: (B, 1024)
        return self.fc(x)
```

---

**14. Resources**

**Foundational**

- **"Computer Vision: Algorithms and Applications"** — Richard Szeliski (free online): the most complete CV textbook, covers everything from pinhole cameras to neural nets
- **"Programming Computer Vision with Python"** — Jan Erik Solem: practical OpenCV projects
- **CS231n: Convolutional Neural Networks for Visual Recognition** (Stanford, free): the canonical CNN course, lecture notes are excellent
- **"Deep Learning for Vision Systems"** — Mohamed Elgendy: CNN architectures with practical code

**Papers**

| Topic | Paper |
|-------|-------|
| YOLO original | Redmon et al., "You Only Look Once" (CVPR 2016) |
| YOLOv8 | Jocher et al., Ultralytics YOLOv8 (2023) |
| Faster R-CNN | Ren et al., "Faster R-CNN" (NeurIPS 2015) |
| ViT | Dosovitskiy et al., "An Image is Worth 16×16 Words" (ICLR 2021) |
| DETR | Carion et al., "End-to-End Object Detection with Transformers" (ECCV 2020) |
| SAM | Kirillov et al., "Segment Anything" (ICCV 2023) |
| DeepLab v3+ | Chen et al., "Encoder-Decoder with Atrous Separable Convolution" (ECCV 2018) |
| SegFormer | Xie et al., "SegFormer: Simple and Efficient Design for Semantic Segmentation" (NeurIPS 2021) |
| Mask2Former | Cheng et al., "Masked-attention Mask Transformer" (CVPR 2022) |
| BEVFusion | Liu et al., "BEVFusion" (ICRA 2023) |
| ByteTrack | Zhang et al., "ByteTrack" (ECCV 2022) |

**Tools and Libraries**

- **OpenCV** — docs.opencv.org — core library
- **Halide** — [github.com/halide/Halide](https://github.com/halide/Halide) — language for fast, portable data-parallel image processing (CPU, GPU, FPGA)
- **Ultralytics YOLOv8** — docs.ultralytics.com — modern detection/segmentation/pose
- **Albumentations** — albumentations.ai — augmentation
- **Supervision** — supervision.roboflow.com — detection utilities, visualization
- **Roboflow** — roboflow.com — dataset hosting, format conversion, augmentation

</details>

### 标注工具汇总

| 工具 | 链接 |
|------|------|
| X-AnyLabeling | github.com/CVHub520/X-AnyLabeling |
| CVAT | app.cvat.ai or github.com/cvat-ai/cvat |
| LabelImg | github.com/HumanSignal/labelImg |
| RectLabel | rectlabel.com |
| VGG VIA | robots.ox.ac.uk/~vgg/software/via |
| Labelbox | labelbox.com |
| Roboflow | roboflow.com |
| COCO Annotator | github.com/jsbroks/coco-annotator |

### 会议

- **CVPR** — IEEE/CVF Conference on Computer Vision and Pattern Recognition
- **ICCV** — International Conference on Computer Vision
- **ECCV** — European Conference on Computer Vision
- **NeurIPS** — ML/AI（计算机视觉方向强）

---

*上一篇：[阶段 3 — 人工智能](../../Guide.md)*


<details>
<summary>English original</summary>

**Annotation Tools Summary**

| Tool | Link |
|------|------|
| X-AnyLabeling | github.com/CVHub520/X-AnyLabeling |
| CVAT | app.cvat.ai or github.com/cvat-ai/cvat |
| LabelImg | github.com/HumanSignal/labelImg |
| RectLabel | rectlabel.com |
| VGG VIA | robots.ox.ac.uk/~vgg/software/via |
| Labelbox | labelbox.com |
| Roboflow | roboflow.com |
| COCO Annotator | github.com/jsbroks/coco-annotator |

**Conferences**

- **CVPR** — IEEE/CVF Conference on Computer Vision and Pattern Recognition
- **ICCV** — International Conference on Computer Vision
- **ECCV** — European Conference on Computer Vision
- **NeurIPS** — ML/AI (strong CV track)

---

*Previous: [Phase 3 — Artificial Intelligence](../../Guide.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/3. Computer Vision/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/3.%20Computer%20Vision/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
