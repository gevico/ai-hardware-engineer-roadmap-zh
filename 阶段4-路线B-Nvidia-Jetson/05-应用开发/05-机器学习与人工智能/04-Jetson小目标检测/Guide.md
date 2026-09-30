---
title: Jetson 上的小目标检测 — 项目指南
description: Jetson 上的小目标检测 — 项目指南
---

# Jetson 上的小目标检测 — 项目指南

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">SODO</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度解析 · Jetson 赛道</p>
<p class="course-identity__title">Jetson 上的小目标检测 — 项目指南的专项课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


> **目标：** 在 NVIDIA Jetson Orin Nano 上，使用 **VisDrone2019-DET** 与成熟的最佳实践，为实时视觉后端解决小目标检测问题。交付一个可经 DeepStream + TensorRT 部署的训练后模型，具备可接受的准确率与延迟。

**如何达成目标（概览）：** 在 VisDrone2019-DET 上训练一个小目标感知的检测器（例如 YOLOv8，或 SPD-YOLOv8 / LPAE-YOLOv8 这类变体），采用更高的输入分辨率（832–960）、多尺度融合、对超高分辨率帧可选的分块，以及调优后的置信度/NMS。将模型导出为 TensorRT（FP16 或 INT8），并集成为 Jetson Orin Nano 上现有 DeepStream + GStreamer + FastAPI 流水线中的主检测器。对 4K 或高分辨率无人机视频，使用带重叠 patch 的分块推理（如 SAHI）并合并检测结果，使小目标在每个分块中保留足够像素，同时在边缘设备上平衡延迟与吞吐。

---


## 1. 项目概览

### 问题

- **平台：** NVIDIA Jetson Orin Nano；技术栈：DeepStream、GStreamer、FastAPI。
- **任务：** 改进主目标检测与可选的二级分类；减少误检；处理**极小目标**（帧中仅几个像素）。
- **约束：** 实时流水线，内存与算力受限；生产级代码质量。

### 方法

| 阶段 | 内容 |
|-------|--------|
| **数据** | 使用 VisDrone2019-DET（无人机视角、小目标、公开 benchmark）。从官方来源下载，将标注转换为 YOLO 格式。 |
| **模型** | YOLOv8（或改进变体），针对小目标做调整：多尺度特征、更高输入分辨率、anchor/损失调优。 |
| **训练** | 从 COCO 预训练权重做迁移学习；在 VisDrone 上微调；针对尺度/光照/运动的增强。 |
| **部署** | 导出为 ONNX → TensorRT（FP16/INT8）；集成为主检测器接入 DeepStream；针对小目标调优置信度/NMS。 |

### Demo 输出（小目标检测）

该流水线输出带 bounding box 的检测结果，以及可选的分割 mask。下图展示了高角度 VisDrone 风格帧上的**分块推理（SAHI）**：夜间拥挤的户外场景，包含大量小实例。模型检测出**人**（红色）、**自行车**（橙色）以及其他类别的多尺度目标，包括场景中的极小目标。

![小目标检测 demo 输出 — 拥挤场景上的 SAHI 分块推理](../../../../Assets/images/0000066_01097_d_0000002_sahi.png)

*示例：在密集、多尺度场景上给出 bounding box 与分割 mask；可用于验证小目标召回率与分块（SAHI）行为。*

---

## 2. 数据集：VisDrone2019-DET

### 来源与许可

- **仓库：** [VisDrone/VisDrone-Dataset](https://github.com/VisDrone/VisDrone-Dataset)（GitHub）。
- **挑战赛 / 说明：** [Vision Meets Drones (VisDrone)](https://aiskyeye.com/) — ICCV 2019。
- **注册：** 部分划分（如 test-dev）可能需要在 [aiskyeye.com](http://www.aiskyeye.com/views/register) 注册。

### 划分与规模

| 划分 | 图像数 | 规模（约） | 用途 |
|-------|--------|----------------|------|
| **Train** | 6,471 | ~1.44 GB | 训练 + 可选的验证留出集 |
| **Val** | 548 | ~0.77 GB | 验证 / 早停 |
| **Test-dev** | 1,610 | ~0.56 GB | 评估（标注未公开） |

- **实例总数：** 260 万+ bounding box；其中很多是小目标（无人机视角）。

### 下载

**方案 A — 官方（推荐）**  
- 按照 [VisDrone-Dataset](https://github.com/VisDrone/VisDrone-Dataset) README 与下载链接操作（通常是 Google Drive，或注册后的挑战赛站点）。  
- **VisDrone2019-DET-train：** 即训练图像 + 标注。  
- **VisDrone2019-DET-val：** 验证图像 + 标注。

**方案 B — 其他镜像**  
- [dataset-ninja/vis-drone-2019-det](https://github.com/dataset-ninja/vis-drone-2019-det)（见 `DOWNLOAD.md`）可能列出 train/val 的直接链接（如 Google Drive）。

**方案 C — Ultralytics（自动下载）**  
- 若使用 Ultralytics YOLOv8，[VisDrone 数据集卡片](https://docs.ultralytics.com/datasets/detect/visdrone/) 可在使用内置的 `visdrone.yaml` 时驱动自动下载（在支持的情况下）。


<details>
<summary>English original</summary>

**Small Object Detection on Jetson — Project Guide**

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">SODO</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Small Object Detection on Jetson — Project Guide.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Goal:** Solve small-object detection for a real-time vision backend on NVIDIA Jetson Orin Nano using **VisDrone2019-DET** and established best practices. Deliver a trained model deployable via DeepStream + TensorRT with acceptable accuracy and latency.

**How to achieve the goal (overview):** Train a small-object–aware detector (e.g. YOLOv8 or a variant like SPD-YOLOv8 / LPAE-YOLOv8) on VisDrone2019-DET using higher input resolution (832–960), multi-scale fusion, optional tiling for very high-res frames, and tuned confidence/NMS. Export the model to TensorRT (FP16 or INT8) and integrate it as the primary detector in your existing DeepStream + GStreamer + FastAPI pipeline on Jetson Orin Nano. For 4K or high-resolution drone footage, use tiled inference (e.g. SAHI) with overlapping patches and merge detections so small targets retain enough pixels per tile while balancing latency and throughput on the edge device.

---


**1. Project Overview**

**Problem**

- **Platform:** NVIDIA Jetson Orin Nano; stack: DeepStream, GStreamer, FastAPI.
- **Task:** Improve primary object detection and optional secondary classification; reduce false positives; handle **very small targets** (few pixels in frame).
- **Constraints:** Real-time pipeline, limited memory and compute; production code quality.

**Approach**

| Phase | Content |
|-------|--------|
| **Data** | Use VisDrone2019-DET (drone-view, small objects, public benchmark). Download from official source, convert annotations to YOLO format. |
| **Model** | YOLOv8 (or improved variant) with small-object–oriented tweaks: multi-scale features, higher input resolution, anchor/loss tuning. |
| **Training** | Transfer learning from COCO-pretrained weights; fine-tune on VisDrone; augmentation for scale/lighting/motion. |
| **Deploy** | Export to ONNX → TensorRT (FP16/INT8); integrate as primary detector in DeepStream; tune confidence/NMS for small objects. |

**Demo output (small object detection)**

The pipeline produces detections with bounding boxes and optional segmentation masks. The image below shows **tiled inference (SAHI)** on a high-angle VisDrone-style frame: a crowded outdoor scene at night with many small instances. The model detects **people** (red), **bicycles** (orange), and other classes across scale, including very small targets in the scene.

![Small object detection demo output — SAHI tiled inference on crowded scene](../../../../Assets/images/0000066_01097_d_0000002_sahi.png)

*Example: bounding boxes and segmentation masks on a dense, multi-scale scene; useful for validating small-object recall and tiling (SAHI) behavior.*

---

**2. Dataset: VisDrone2019-DET**

**Source and License**

- **Repository:** [VisDrone/VisDrone-Dataset](https://github.com/VisDrone/VisDrone-Dataset) (GitHub).
- **Challenge / description:** [Vision Meets Drones (VisDrone)](https://aiskyeye.com/) — ICCV 2019.
- **Registration:** Some splits (e.g. test-dev) may require registration at [aiskyeye.com](http://www.aiskyeye.com/views/register).

**Splits and Size**

| Split | Images | Size (approx.) | Use |
|-------|--------|----------------|------|
| **Train** | 6,471 | ~1.44 GB | Training + optional validation holdout |
| **Val** | 548 | ~0.77 GB | Validation / early stopping |
| **Test-dev** | 1,610 | ~0.56 GB | Evaluation (labels not public) |

- **Total instances:** 2.6+ million bounding boxes; many are small (drone perspective).

**Download**

**Option A — Official (recommended)**  
- Follow [VisDrone-Dataset](https://github.com/VisDrone/VisDrone-Dataset) README and download links (often Google Drive or the challenge site after registration).  
- **VisDrone2019-DET-train:** e.g. training images + annotations.  
- **VisDrone2019-DET-val:** validation images + annotations.

**Option B — Alternative mirrors**  
- [dataset-ninja/vis-drone-2019-det](https://github.com/dataset-ninja/vis-drone-2019-det) (see `DOWNLOAD.md`) may list direct links (e.g. Google Drive) for train/val.

**Option C — Ultralytics (auto-download)**  
- If using Ultralytics YOLOv8, the [VisDrone dataset card](https://docs.ultralytics.com/datasets/detect/visdrone/) can drive automatic download when using the built-in `visdrone.yaml` (where supported).

</details>

### 目录布局（原始 VisDrone2019-DET）

下载后，通常会得到：

```
VisDrone2019-DET/
├── images/
│   ├── train/          # 6471 images
│   ├── val/            # 548 images
│   └── test-dev/       # 1610 images (optional)
└── annotations/
    ├── train/          # one .txt per image, same base name
    ├── val/
    └── test-dev/
```

每个标注文件与一张图像一一对应（例如 `0000001_00000_d_0000001.jpg` → `0000001_00000_d_0000001.txt`）。

---

## 3. 标注格式与转换为 YOLO

### VisDrone 真值格式（每行）

每个目标一行；8 个以逗号分隔的字段：

```
<x>,<y>,<width>,<height>,<score>,<object_category>,<truncation>,<occlusion>
```

| 字段 | 含义 |
|-------|--------|
| `x`, `y` | 边界框左上角（像素）。|
| `width`, `height` | 框尺寸（像素）。|
| `score` | 在真值中：`1` = 参与评估，`0` = 忽略。|
| `object_category` | 类别索引（见下文）。|
| `truncation` | 0 = 无，1 = 部分（1–50% 在画面外）。|
| `occlusion` | 0 = 无，1 = 部分（1–50%），2 = 严重（50–100%）。|

### 目标类别（DET）

| 索引 | 类别 |
|-------|--------|
| 0 | ignored regions |
| 1 | pedestrian |
| 2 | people |
| 3 | bicycle |
| 4 | car |
| 5 | van |
| 6 | truck |
| 7 | tricycle |
| 8 | awning-tricycle |
| 9 | bus |
| 10 | motor |
| 11 | others |

对于**检测训练**，通常使用 **1–10**（10 个类别）。索引 `0` 和 `11` 在评估中被忽略；可以跳过它们，或者如有需要把 `others` 映射为单个 “other” 类别。

### 转换为 YOLO 格式

YOLO 每张图像期望一个 `.txt`，每行格式为：`class_id x_center y_center width height`（相对于图像尺寸归一化到 0–1）。

- **类别：** 把 VisDrone 类别映射到 0–9：`yolo_cls = visdrone_cls - 1`（即 pedestrian=0，people=1，…，motor=9）。视任务需要，可选地合并 pedestrian/people 或去掉 “others”。
- **框：** 从像素单位的 `(x_tl, y_tl, w, h)` 转换为归一化的 `(x_center, y_center, w, h)`：
  - `x_center = (x + w/2) / img_width`
  - `y_center = (y + h/2) / img_height`
  - `w_norm = w / img_width`, `h_norm = h / img_height`
- **过滤：** 若不希望用它们训练，忽略 `score == 0` 的行或类别为 `0` / `11` 的行。

**参考实现：**

- Ultralytics：[yolov5/data/VisDrone.yaml](https://github.com/ultralytics/yolov5/blob/master/data/VisDrone.yaml)（旧版 YOLOv5；转换逻辑在 dataset loader 中）。
- [adityatandon/VisDrone2YOLO](https://github.com/adityatandon/VisDrone2YOLO)：独立的转换脚本与预转换好的标签。

### 转换后的 YOLO 风格目录结构

转换完成后，使用与 Ultralytics 兼容的目录结构（例如 YOLOv8）：

```
VisDrone/
├── images/
│   ├── train/
│   └── val/
├── labels/
│   ├── train/   # .txt per image, same base name
│   └── val/
└── data.yaml   # dataset config (see below)
```

**示例 `data.yaml`：**

```yaml
path: /path/to/VisDrone
train: images/train
val: images/val
# test: images/test-dev  # optional

nc: 10
names: ['pedestrian', 'people', 'bicycle', 'car', 'van', 'truck', 'tricycle', 'awning-tricycle', 'bus', 'motor']
```

---

## 4. 小目标检测的最佳方法（无人机 / UAV）

以下是在航拍/无人机图像中对小目标的成熟改进方法；可据此选择架构与训练设置。

### 模型系列：YOLOv8

- **默认选择：** YOLOv8n/s/m（nano/small/medium）——速度/准确率取舍良好，且易于导出到 TensorRT/DeepStream。
- **理由：** 在 VisDrone 上基线表现强；Ultralytics 支持完善（train、export、validation）；TensorRT 与 DeepStream 对 YOLO 有支持。

### 架构 / 训练改进（概要）

| 技术 | 作用 |
|-----------|------|
| **多尺度特征融合** | 为小目标增加额外检测头或 FPN 式融合；避免小目标在深层步长中丢失。|
| **更高的输入分辨率** | 例如延迟允许时在推理/训练中采用 640→960 或 1280；对极小目标至关重要。|
| **面向小目标的层** | 某些变体去掉最大目标检测头，增加一个额外的小目标检测头（例如 LPAE-YOLOv8）。|
| **SPD（space-to-depth）** | 用 SPD 模块替换 stride-2 的 conv/pool，以保留小目标信息（例如 SPD-YOLOv8）。|
| **Anchor / 损失调优** | 针对小框调整 anchor 尺度；使用对小目标有利的损失（例如 Wise-IoU、EfficiCIoU、MPDIoU）。|
| **轻量 attention** | 轻量 attention（例如 LPAE-YOLOv8 中的）以在不大幅增加延迟的前提下改善特征加权。|
| **分块（slicing / tiled inference）** | 把高分辨率图像切成重叠的 patch；对每个分块分别检测并合并结果，使小目标保有足够的像素。|

以下各小节说明每种技术，以及如何应用它们达成项目目标。

---


<details>
<summary>English original</summary>

**Directory Layout (raw VisDrone2019-DET)**

After downloading, you typically have:

```
VisDrone2019-DET/
├── images/
│   ├── train/          # 6471 images
│   ├── val/            # 548 images
│   └── test-dev/       # 1610 images (optional)
└── annotations/
    ├── train/          # one .txt per image, same base name
    ├── val/
    └── test-dev/
```

Each annotation file corresponds 1:1 to an image (e.g. `0000001_00000_d_0000001.jpg` → `0000001_00000_d_0000001.txt`).

---

**3. Annotation Format and Conversion to YOLO**

**VisDrone Ground-Truth Format (per line)**

One line per object; 8 comma-separated fields:

```
<x>,<y>,<width>,<height>,<score>,<object_category>,<truncation>,<occlusion>
```

| Field | Meaning |
|-------|--------|
| `x`, `y` | Top-left corner of bounding box (pixels). |
| `width`, `height` | Box size (pixels). |
| `score` | In ground truth: `1` = used in evaluation, `0` = ignored. |
| `object_category` | Class index (see below). |
| `truncation` | 0 = none, 1 = partial (1–50% outside frame). |
| `occlusion` | 0 = none, 1 = partial (1–50%), 2 = heavy (50–100%). |

**Object Categories (DET)**

| Index | Class |
|-------|--------|
| 0 | ignored regions |
| 1 | pedestrian |
| 2 | people |
| 3 | bicycle |
| 4 | car |
| 5 | van |
| 6 | truck |
| 7 | tricycle |
| 8 | awning-tricycle |
| 9 | bus |
| 10 | motor |
| 11 | others |

For **detection training** we usually use **1–10** (10 classes). Index `0` and `11` are ignored in evaluation; you can skip them or map `others` to a single “other” class if desired.

**Conversion to YOLO Format**

YOLO expects one `.txt` per image, each line: `class_id x_center y_center width height` (normalized 0–1 relative to image size).

- **Class:** Map VisDrone category to 0–9: `yolo_cls = visdrone_cls - 1` (so pedestrian=0, people=1, …, motor=9). Optionally merge pedestrian/people or drop “others” depending on task.
- **Box:** Convert from `(x_tl, y_tl, w, h)` in pixels to normalized `(x_center, y_center, w, h)`:
  - `x_center = (x + w/2) / img_width`
  - `y_center = (y + h/2) / img_height`
  - `w_norm = w / img_width`, `h_norm = h / img_height`
- **Filter:** Ignore rows with `score == 0` or category `0` / `11` if you do not want to train on them.

**Reference implementations:**

- Ultralytics: [yolov5/data/VisDrone.yaml](https://github.com/ultralytics/yolov5/blob/master/data/VisDrone.yaml) (legacy YOLOv5; conversion logic in dataset loader).
- [adityatandon/VisDrone2YOLO](https://github.com/adityatandon/VisDrone2YOLO): standalone conversion script and pre-converted labels.

**Resulting YOLO-Style Layout**

After conversion, use a layout compatible with Ultralytics (e.g. YOLOv8):

```
VisDrone/
├── images/
│   ├── train/
│   └── val/
├── labels/
│   ├── train/   # .txt per image, same base name
│   └── val/
└── data.yaml   # dataset config (see below)
```

**Example `data.yaml`:**

```yaml
path: /path/to/VisDrone
train: images/train
val: images/val
# test: images/test-dev  # optional

nc: 10
names: ['pedestrian', 'people', 'bicycle', 'car', 'van', 'truck', 'tricycle', 'awning-tricycle', 'bus', 'motor']
```

---

**4. Best Methods for Small-Object Detection (Drone / UAV)**

These are established improvements for small objects in aerial/drone imagery; use them to pick architecture and training settings.

**Model Family: YOLOv8**

- **Default choice:** YOLOv8n/s/m (nano/small/medium) — good speed/accuracy tradeoff and easy export to TensorRT/DeepStream.
- **Why:** Strong baseline on VisDrone; well supported by Ultralytics (train, export, validation); TensorRT and DeepStream support for YOLO.

**Architectural / Training Improvements (summary)**

| Technique | Role |
|-----------|------|
| **Multi-scale feature fusion** | Extra detection heads or FPN-style fusion for small objects; avoid losing small instances in deep strides. |
| **Higher input resolution** | e.g. 640→960 or 1280 for inference/training when latency allows; critical for very small targets. |
| **Small-object–specific layers** | Some variants remove the largest-object head and add an additional small-object head (e.g. LPAE-YOLOv8). |
| **SPD (space-to-depth)** | Replace stride-2 conv/pool with SPD modules to retain information for small objects (e.g. SPD-YOLOv8). |
| **Anchor / loss tuning** | Adjust anchor scales for small boxes; use losses that help small objects (e.g. Wise-IoU, EfficiCIoU, MPDIoU). |
| **Lightweight attention** | Lightweight attention (e.g. in LPAE-YOLOv8) for better feature weighting without blowing up latency. |
| **Tiling (slicing / tiled inference)** | Split high-resolution images into overlapping patches; run detection per tile and merge results so small objects keep enough pixels. |

The following subsections explain each technique and how to apply it to reach the project goal.

---

</details>

#### 1. 多尺度特征融合——小目标检测中的做法与缘由

**深入理解它是什么：**  
在 YOLO 这类现代目标检测器中，输入图像会经过骨干网络，该网络通过卷积层与池化层**逐步降低图像的空间分辨率**（称为“下采样”，步长如 8、16 或 32）。这一过程有助于提取更高层的特征（如目标类别），但同时也意味着**微小目标可能丢失**——想象一辆 12×12 像素的汽车被压缩成单个“cell”，或在深层中彻底消失。

**多尺度特征融合**通过**组合多个分辨率下的特征**直接解决这一问题。它不是只处理深层（低分辨率）特征，而是同时收集并合并两者：
- **浅层高分辨率特征：** 有利于*精确的空间细节*（边缘、角点、微小斑块）
- **深层低分辨率特征：** 有利于*语义含义*（“这是一辆车”）

这种融合通常借助称为 **FPN（Feature Pyramid Network）** 或其变体（如 PANet）的结构完成，它们使用：
- **自顶向下路径：** 对深层特征上采样，并通过横向（侧边）连接与更早层的特征合并
- **自底向上路径（可选）：** 将信息再向下传递，用语义信息细化高分辨率特征

**为什么这对小目标重要？**
- 小目标（尤其在无人机或拥挤场景中）可能只占几个像素，因此若只使用最深层，微小目标就会消失。
- 通过合并浅层（细节）与深层（语义）特征，每个检测头都能获得“两全其美”的效果。小目标检测头获得精细细节，大目标检测头获得语义上下文。
- **在实践中，多尺度融合意味着模型保留了对微小信号的敏感度，而不仅是对大而容易的目标！**

**分步讲解：技术上如何实现**


<details>
<summary>English original</summary>

**1. Multi-Scale Feature Fusion — How and Why for Small Object Detection**

**What It Is, in Depth:**  
In modern object detectors like YOLO, the input image is passed through a backbone network that **progressively reduces the image’s spatial resolution** via convolutional and pooling layers (called "downsampling," with strides like 8, 16, or 32). While this process helps extract higher-level features (like object types), it also means **tiny objects can be lost**—imagine a 12×12 pixel car being squished down to a single “cell” or vanishing entirely in deep layers.

**Multi-scale feature fusion** directly addresses this by **combining features at multiple resolutions**. Instead of only processing deep (low-res) features, it collects and merges both:
- **Shallow, high-resolution features:** Good for *precise spatial detail* (edges, corners, tiny blobs)
- **Deep, low-resolution features:** Good for *semantic meaning* (“this is a car”)

This fusion is usually done using structures called **FPN (Feature Pyramid Network)** or its variants (like PANet), which use:
- **Top-down pathways:** Upsample deep features and merge with earlier layer features through lateral (side) connections
- **Bottom-up pathways (optional):** Pass information back down, to refine high-res features with semantic info

**Why Is This Important for Small Objects?**
- Small objects (especially in drones or crowded scenes) might only take up a few pixels, so if you only use the deepest layers, tiny targets will disappear.
- By merging shallow (detailed) and deep (semantic) features, each detection head gets “the best of both worlds.” Small-object heads get fine detail, and large-object heads get semantic context.
- **Practically, multi-scale fusion means your model retains the sensitivity to tiny signals, not just large and easy targets!**

**Step-by-Step: How to Do This Technically**

</details>

1. **使用 YOLOv8（开箱即用）：**
   - **无需改代码：** Ultralytics YOLOv8 已经内置 **PANet 风格的 FPN**，即默认就会融合多尺度特征。
   - 共有 3 个检测头（P3–P5），因此模型可以覆盖一定的尺寸范围。
   - *应该做什么：* 把精力放在**数据集和数据增强**上——避免使用可能把小目标裁掉的增强设置（例如过度裁剪或缩放）。把数据中的小目标实例保留下来！
   - 训练时，**把 `imgsz` 参数设得足够大**（参见“更高的输入分辨率”），让小目标在浅层特征图上可见。

2. **使用增强架构（获得更好效果）：**
   - 对许多航拍/无人机数据集而言，默认的 YOLO 检测头可能不够。一些进阶研究增加了：
     - 一个分辨率更高的**额外检测头**（P2，即输入的 1/4 步长）
     - 这个额外检测头连接到更浅的层（更靠近输入），专门用于*极*小目标
   - **如何实现：**
     - **方案 1：** 找一个已经实现 P2 检测头的公开 YOLOv8 变体（例如 **LPAE-YOLOv8**），参考其 repo 了解用法。
     - **方案 2：** 自定义代码（进阶）：fork [Ultralytics YOLOv8 repo](https://github.com/ultralytics/ultralytics)，修改模型代码加入一条 P2 特征图分支（通常放在 backbone 的第二个 block 之后），添加对应的检测头，并针对小框尺寸更新 anchor 设置。在 loss 分配阶段，你可能还想把小目标的 ground truth 专门分配给这个 P2 输出。
     - **提示：** 新检测头的权重随机初始化，其余部分加载 COCO 预训练权重。再在你的数据集上微调。
     - **参考：** 网络结构图与消融实验见论文 *“Improved YOLOv8 for Small Object Detection in Aerial Images”*。


<details>
<summary>English original</summary>

1. **With YOLOv8 (Out-of-the-Box):**
   - **No code changes needed:** Ultralytics YOLOv8 already has a **PANet-style FPN**, meaning it fuses multi-scale features by default.
   - There are 3 detection heads (P3–P5), so the model can handle a range of sizes.
   - *What you should do:* Focus on your **dataset and data augmentations**—avoid augmentation settings (like excessive cropping or resizing) that might exclude small objects. Keep the small instances in your data!
   - When training, **set the `imgsz` parameter large enough** (see “Higher input resolution”) to make small objects visible on early feature maps.

2. **With Enhanced Architectures (For Even Better Results):**
   - For many aerial/drone datasets, the default YOLO heads may not be enough. Some advanced research adds:
     - An **extra detection head** at even higher resolution (P2, or 1/4 stride of input)
     - This extra head is dedicated to *very* small objects by connecting to shallower layers (closer to the input)
   - **How to implement:**
     - **Option 1:** Find a public YOLOv8 variant (like **LPAE-YOLOv8**) that already implements a P2 head and see their repo for usage.
     - **Option 2:** Custom code (advanced): Fork the [Ultralytics YOLOv8 repo](https://github.com/ultralytics/ultralytics), modify the model code to add a P2 feature map branch (often after the second block in the backbone), add a corresponding detection head, and update the anchor settings for small box sizes. You may also want to assign small-object ground truths specifically to this P2 output during loss assignment.
     - **Tip:** Initialize the weights for your new head randomly, but load COCO-pretrained weights for the rest. Fine-tune on your dataset.
     - **Reference:** See the paper *“Improved YOLOv8 for Small Object Detection in Aerial Images”* for network diagrams and ablation studies.

</details>

3. **多尺度训练（必做，补充架构）：**
   - 即使模型支持多尺度检测，你也希望模型对目标尺寸变化具有鲁棒性。
   - **如何：**  
     - 多尺度训练在范围内随机缩放输入图像（例如基础尺寸的 ±50%）。Ultralytics YOLO 默认执行此操作。
     - 要更关注小目标：**设置更大的基础图像尺寸**（`imgsz`）。这确保即使按尺度下限训练，小目标 *仍然足够大*，网络能从中学习。
     - **命令示例：**  
       ```bash
       yolo detect train data=VisDrone/data.yaml model=yolov8s.pt imgsz=832  # or imgsz=960 for very small objects
       ```
     - **最佳实践：** 留意内存使用！更大的图像尺寸会增加 VRAM 需求——如果出现 OOM（“out of memory”）错误，请减小批大小。

**典型工作流回顾 / 检查清单**

- [ ] 选择你的 YOLO 变体（原版 YOLOv8，或小目标增强变体）
- [ ] 调整训练命令，使用更大的 `imgsz`（例如 832 或 960）
- [ ] 验证数据加载/增强 *不* 排除/裁剪小目标
- [ ] 为了研究或前沿性能，考虑修改模型架构以添加 P2 head（阅读论文 / 使用公开的小目标 YOLO 变体）
- [ ] 训练后，始终 *专门* 检查小目标类别的性能 — 测量小框的平均精度，而不仅仅是整体 mAP。

**关键工具：**
- [Ultralytics YOLOv8 repo](https://github.com/ultralytics/ultralytics): 用于训练、定制模型
- 论文/LPAE-YOLOv8 仓库：用于 P2 head 实现
- Netron：可视化训练好的 ONNX 或 PyTorch 模型，以确认网络结构，验证额外的 head 是否存在


---


<details>
<summary>English original</summary>

3. **Multi-Scale Training (Must-Do, Complements Architecture):**
   - Even if your model supports multi-scale detection, you want the model to be robust to object size variations.
   - **How:**  
     - Multi-scale training randomly resizes input images within a range (e.g. ±50% of base size). Ultralytics YOLO does this by default.
     - To focus more on small objects: **Set a larger base image size** (`imgsz`). This ensures that, even when training at the lower end of the scale, small objects *remain large enough* for the network to learn from.
     - **Command example:**  
       ```bash
       yolo detect train data=VisDrone/data.yaml model=yolov8s.pt imgsz=832  # or imgsz=960 for very small objects
       ```
     - **Best practice:** Watch memory usage! Larger image sizes increase VRAM needs—reduce batch size if you get OOM (“out of memory”) errors.

**Typical Workflow Recap / Checklist**

- [ ] Pick your YOLO variant (vanilla YOLOv8, or a small-object–enhanced variant)
- [ ] Adjust your training command to use a larger `imgsz` (e.g. 832 or 960)
- [ ] Verify your data loading/augmentation does *not* exclude/crop small objects
- [ ] For research or cutting-edge performance, consider modifying the model architecture to add P2 head (read papers / use public small-object YOLO variants)
- [ ] After training, always check performance *specifically* for small object categories — measure average precision for small boxes, not just overall mAP.

**Key Tools:**
- [Ultralytics YOLOv8 repo](https://github.com/ultralytics/ultralytics): for training, customizing models
- Papers/LPAE-YOLOv8 repos: for P2 head implementations
- Netron: visualize your trained ONNX or PyTorch model to confirm network structure, verify extra heads exist


---

</details>

#### 2. 更高的输入分辨率

**它是什么：**  
输入尺寸是送入网络的图像的高度/宽度（以像素为单位）（例如 640×640）。**更高分辨率**（例如 960×960 或 1280×1280）意味着每个目标拥有更多像素，因此一个“微小”目标会占据更多网格单元，并获得更强的特征响应。

**为什么对小目标有帮助：**  
- 在 640×640 下，一个 10×10 像素的目标约占图像的 ~0.15%，并且可能映射到一个或两个网格单元。  
- 在 1280×1280 下，同一目标按绝对占比约为图像的 ~0.06%，但在特征图中（逐 stride 比较）具有 **4× 的像素数**，因此检测器有更多信号用于分类和回归。

**如何实现：**

- **训练：** 训练时使用更大的 `imgsz`。取舍：需要更多 VRAM 且训练更慢；如有需要，减小批大小。

  ```bash
  # Baseline
  yolo detect train data=VisDrone/data.yaml model=yolov8s.pt imgsz=640 batch=16 epochs=100

  # Better for small objects (expect ~1.5–2× VRAM, reduce batch)
  yolo detect train data=VisDrone/data.yaml model=yolov8s.pt imgsz=960 batch=8 epochs=100
  # or imgsz=832 batch=12 as a compromise
  ```

- **推理：** 推理时使用相同（或略低）的分辨率，使行为与训练一致。在 Jetson 上，960 或 1280 会增加延迟；先试 832，如果准确率仍不足，再试 960。

  ```bash
  yolo detect val model=best.pt data=data.yaml imgsz=960
  ```

- **经验法则：** 对于“非常小”的目标（例如完整图像中 &lt;32×32 px），当延迟预算允许时，训练和部署优先选择 **832–960**；对于较大的小目标，**640** 可以接受。

---

#### 3. 小目标专用层（小目标的额外检测头）

**它是什么：**  
标准 YOLOv8 在输入尺寸的 1/8、1/16 和 1/32 处有三个检测头。1/8 检测头已经是“最小 stride”，负责相对较小的目标，但在无人机数据中，许多目标甚至更小。**小目标专用**设计会增加一个 **更高分辨率的额外检测头**（例如 1/4 stride），有时会 **移除或降低最大目标检测头的权重**（1/32），以平衡计算量并聚焦小目标。

**为什么对小目标有帮助：**  
- 专用的高分辨率检测头会增加能够“看到”微小目标并为其回归边界框的网格单元数量。  
- 将容量从“非常大的目标”检测头转移到“非常小的目标”检测头，符合无人机数据集的分布（大量小目标，少量巨大目标）。

**如何实现：**

- **使用已发布变体：** 例如 **LPAE-YOLOv8** 增加一个小目标检测层并移除大目标层；它还使用轻量级 attention 模块（见下文）。克隆仓库，遵循其训练说明，并照常导出到 ONNX/TensorRT。
- **DIY（进阶）：** 修改 Ultralytics 代码库（或 fork）中的 YOLOv8 检测头：从 backbone 添加一个 P2（1/4）分支，为该尺度添加第四个检测头，并为其分配小 anchor。然后从预训练 backbone 开始训练（例如加载 COCO 权重并随机初始化新检测头）。

---

#### 4. SPD（space-to-depth）

**它是什么：**  
**Space-to-depth** 是将空间维度重排到通道维度：SPD 不是使用会 **丢弃** 信息的 stride-2 卷积或池化，而是 **重排** 像素，使 2×2（或 4×4）邻域变为额外通道。有效空间尺寸会减小（例如缩小 2×），但 **没有信息被丢弃**；随后的 conv 再混合这些通道。

**为什么对小目标有帮助：**  
- Stride-2 卷积和池化会对特征图进行 **下采样**。微小目标（在特征图中为 1–2 像素）可能在一个 stride 中消失。  
- SPD 将所有像素保留在“深度”维度中，因此小结构仍然存在；网络可以学习利用它们。这在空间分辨率仍然较高的 **早期 backbone** 中尤其有用。

**如何实现：**

- **使用 SPD-YOLOv8（或类似方法）：** 将 backbone 中前几个 stride-2 操作替换为 SPD + conv。论文/仓库中已有实现，例如 “SPD-YOLOv8: small-size object detection model of UAV imagery”。克隆模型代码，在 VisDrone 上训练，然后导出到 ONNX/TensorRT（确保自定义 SPD op 受支持，或转换为标准 op 序列）。
- **概念（用于实现）：**  
  - 输入特征 `H×W×C`。  
  - 切分为 2×2 patch → reshape 为 `(H/2)×(W/2)×(4*C)`。  
  - Conv 4*C → C。  
  结果：与 stride-2 相同的空间下采样，但没有池化造成的信息损失。

---


<details>
<summary>English original</summary>

**2. Higher input resolution**

**What it is:**  
Input size is the height/width (in pixels) of the image fed to the network (e.g. 640×640). **Higher resolution** (e.g. 960×960 or 1280×1280) means more pixels per object, so a “tiny” object occupies more cells and gets a stronger feature response.

**Why it helps small objects:**  
- At 640×640, a 10×10 pixel object is ~0.15% of the image and may map to one or two grid cells.  
- At 1280×1280, the same object is ~0.06% of the image in absolute terms but has **4× more pixels** in the feature maps (stride-for-stride), so the detector has more signal to classify and regress.

**How to achieve it:**

- **Training:** Use a larger `imgsz` when training. Tradeoff: more VRAM and slower training; reduce batch size if needed.

  ```bash
  # Baseline
  yolo detect train data=VisDrone/data.yaml model=yolov8s.pt imgsz=640 batch=16 epochs=100

  # Better for small objects (expect ~1.5–2× VRAM, reduce batch)
  yolo detect train data=VisDrone/data.yaml model=yolov8s.pt imgsz=960 batch=8 epochs=100
  # or imgsz=832 batch=12 as a compromise
  ```

- **Inference:** Use the same (or slightly lower) resolution at inference so behaviour matches training. On Jetson, 960 or 1280 increases latency; try 832 first, then 960 if accuracy is still insufficient.

  ```bash
  yolo detect val model=best.pt data=data.yaml imgsz=960
  ```

- **Rule of thumb:** For “very small” targets (e.g. &lt;32×32 px in full image), prefer **832–960** for training and deployment when the latency budget allows; **640** is acceptable for larger small objects.

---

**3. Small-object–specific layers (extra head for small objects)**

**What it is:**  
Standard YOLOv8 has three detection heads at 1/8, 1/16, and 1/32 of the input size. The 1/8 head is already the “smallest stride” and handles relatively small objects, but in drone data many targets are even smaller. **Small-object–specific** designs add an **additional head at higher resolution** (e.g. 1/4 stride) and sometimes **remove or downweight the largest-object head** (1/32) to balance compute and focus on small instances.

**Why it helps small objects:**  
- A dedicated high-resolution head increases the number of grid cells that can “see” tiny objects and regress boxes for them.  
- Shifting capacity from “very large object” head to “very small object” head matches the distribution of drone datasets (many small, few huge).

**How to achieve it:**

- **Use a published variant:** e.g. **LPAE-YOLOv8** adds a small-object detection layer and removes the large-object layer; it also uses a lightweight attention module (see below). Clone the repo, follow their training instructions, and export to ONNX/TensorRT as usual.
- **DIY (advanced):** Modify the YOLOv8 head in the Ultralytics codebase (or a fork): add a P2 (1/4) branch from the backbone, add a fourth detection head for that scale, and assign small anchors to it. Then train from a pretrained backbone (e.g. load COCO weights and init the new head randomly).

---

**4. SPD (space-to-depth)**

**What it is:**  
**Space-to-depth** is a rearrangement of spatial dimensions into channels: instead of a stride-2 convolution or pooling that **discards** information, SPD **reorders** pixels so that a 2×2 (or 4×4) neighbourhood becomes extra channels. The effective spatial size is reduced (e.g. 2× smaller) but **no information is thrown away**; a following conv then mixes these channels.

**Why it helps small objects:**  
- Stride-2 convs and pooling **subsample** the feature map. Tiny objects (1–2 pixels in a feature map) can vanish in one stride.  
- SPD keeps all pixels in the “depth” dimension, so small structures are still present; the network can learn to use them. This is especially useful in the **early backbone** where spatial resolution is still high.

**How to achieve it:**

- **Use SPD-YOLOv8 (or similar):** Replace the first few stride-2 operations in the backbone with SPD + conv. Implementations are available in papers/repos such as “SPD-YOLOv8: small-size object detection model of UAV imagery”. Clone the model code, train on VisDrone, then export to ONNX/TensorRT (ensure the custom SPD op is supported or converted to a standard op sequence).
- **Concept (for implementation):**  
  - Input feature `H×W×C`.  
  - Split into 2×2 patches → reshape to `(H/2)×(W/2)×(4*C)`.  
  - Conv 4*C → C.  
  Result: same spatial downsampling as stride-2, but no information loss from pooling.

---

</details>

#### 5. Anchor / loss 调优

**是什么：**  
YOLOv8 是 **anchor-free** 的（使用 anchor-free 回归 head）。这里的 “Anchor tuning” 指：(1) 确保各 head 的 **有效感受野 / 尺度范围** 与目标尺寸分布相匹配，(2) 使用能更公平地对待小框的 **loss function**（例如为极小框提供更好的梯度，或采用尺度不变的度量）。

**为什么能帮助小目标：**  
- 标准 IoU 与 L1 loss 容易被大目标主导；小框的绝对误差小，可能只得到很弱的梯度。  
- **Wise-IoU**、**EfficiCIoU**、**MPDIoU**（及类似方法）加入动态加权或形状项，使小框与大框对 loss 的贡献更均衡，从而获得更好的回归表现。

**如何实现：**

- **Wise-IoU (WIoU)：** 常见于改进 YOLOv8 的论文。它把默认的 box loss 换成一种对“简单”样本（例如大框、高 IoU）降权、把学习集中在困难样本（往往是小目标）上的版本。实现方式是 fork Ultralytics 并替换 head 中的 box loss，或者使用已经包含 WIoU 的第三方 YOLOv8 代码库。
- **EfficiCIoU / MPDIoU：** 改进小目标回归的 IoU 变体（例如考虑对角线长度、最小点距离）。思路相同：在训练循环中用新公式替换 box 回归 loss。
- **无需改代码的实用做法：** 通过 **augmentation**（尺度抖动、小实例 copy-paste）提高批内 **小目标出现比例**，让 optimizer 见到更多小框样本；这可部分补偿 loss 的不均衡。

---

#### 6. 轻量级 attention

**是什么：**  
**Attention** 模块（例如 channel attention、spatial attention，或两者兼有）对特征重新加权，使网络聚焦于重要区域和通道。**Lightweight** 版本（例如 squeeze-and-excitation、CBAM，或小型 MLP）只增加有限的参数与计算量，因此适合边缘部署。

**为什么能帮助小目标：**  
- 在杂乱的无人机场景中，小目标要与背景和大目标争夺特征容量。Attention 让网络 **强调** 与小目标对应的通道和空间位置。  
- 轻量级 attention 避免了完整 self-attention 的高开销，同时仍能改善特征加权。

**如何实现：**

- **使用已包含该模块的变体：** 例如 **LPAE-YOLOv8** 使用了 “ACMConv”（adaptive channel modulation）风格的模块。在 VisDrone 上训练该模型并导出；确认自定义层在 ONNX/TensorRT 中受支持（通常标准 conv + sigmoid/mul 即可）。
- **加到原版 YOLOv8 上（进阶）：** 在 backbone 之后或 neck 之前（例如 C2f 中）插入轻量 SE 或 CBAM 块。重新训练；预期延迟小幅上升。优先选择 **channel-only**（SE）或 kernel 极小的 **spatial** attention，以保持 Jetson 上推理够快。

---


<details>
<summary>English original</summary>

**5. Anchor / loss tuning**

**What it is:**  
YOLOv8 is **anchor-free** (uses anchor-free regression heads). “Anchor tuning” here means: (1) ensuring the **effective receptive fields / scale ranges** of the heads match your object size distribution, and (2) using **loss functions** that treat small boxes more fairly (e.g. better gradient for tiny boxes, or scale-invariant metrics).

**Why it helps small objects:**  
- Standard IoU and L1 loss can be dominated by large objects; small boxes have small absolute errors and may get weak gradients.  
- **Wise-IoU**, **EfficiCIoU**, **MPDIoU** (and similar) add dynamic weighting or shape terms so that small and large boxes contribute more equally to the loss and get better regression behaviour.

**How to achieve it:**

- **Wise-IoU (WIoU):** Often used in improved-YOLOv8 papers. Replaces the default box loss with a version that down-weights “easy” (e.g. large, high-IoU) examples and focuses learning on hard ones (often small). Implement by forking Ultralytics and swapping the box loss in the head, or use a third-party YOLOv8 codebase that already includes WIoU.
- **EfficiCIoU / MPDIoU:** Alternative IoU variants that improve small-object regression (e.g. consider diagonal length, minimal point distance). Same idea: replace the box regression loss in the training loop with the new formulation.
- **Practical step without code change:** Increase **small-object presence** in the batch via **augmentation** (scale jitter, copy-paste of small instances) so the optimizer sees more small-box examples; this partially compensates for loss imbalance.

---

**6. Lightweight attention**

**What it is:**  
**Attention** modules (e.g. channel attention, spatial attention, or both) reweight features so the network focuses on important regions and channels. **Lightweight** versions (e.g. squeeze-and-excitation, CBAM, or small MLPs) add limited parameters and compute so they are suitable for edge deployment.

**Why it helps small objects:**  
- In cluttered drone scenes, small objects compete with background and large objects for feature capacity. Attention lets the network **emphasize** channels and spatial locations that correspond to small targets.  
- Lightweight attention avoids the heavy cost of full self-attention while still improving feature weighting.

**How to achieve it:**

- **Use a variant that includes it:** e.g. **LPAE-YOLOv8** uses an “ACMConv” (adaptive channel modulation) style module. Train their model on VisDrone and export; ensure the custom layer is supported in ONNX/TensorRT (usually standard conv + sigmoid/mul is fine).
- **Add to vanilla YOLOv8 (advanced):** Insert a lightweight SE or CBAM block after the backbone or before the neck (e.g. in C2f). Retrain; expect a small latency increase. Prefer **channel-only** (SE) or very small kernel **spatial** attention to keep inference fast on Jetson.

---

</details>

#### 7. 分块（切片 / 分块推理）

**是什么：**  
**分块**（也称**切片**或**滑动窗口推理**）将高分辨率图像切分为更小的、通常**重叠**的 patch（分块）。检测器在每个分块上以模型原生输入尺寸（例如 640×640）运行，因此整图中的小目标在分块内部以更大的有效尺度呈现。推理时，所有分块的检测结果被**合并**，并变换回原始图像坐标（对重叠区域执行 NMS 以去除重复）。

**为何有助于小目标：**  
- 在 640×640 的单次 pass 中，4K 帧被大幅下采样，微小目标可能缩到几个像素甚至更少。  
- 通过分块（例如带重叠的 640×640 窗口），每个分块都处于完整模型分辨率；在下采样整图中只有 2×2 像素的小目标，在某个分块内可能达到 10×10 或更大，从而可被检测到。  
- 重叠可确保靠近分块边界的目标不会被切成两半而漏检；它们至少在某个分块中完整出现。

**关键要点：**

| Aspect | Recommendation |
|--------|----------------|
| **重叠分块** | 使用重叠（例如 **25%** 或 50%），使分块边缘的目标至少在某个分块中被完整包含。可减少边界截断与漏检。 |
| **推理流程** | 在每个分块上运行模型 → 得到分块坐标下的框 → 将框映射到整图坐标 → 对所有检测结果运行 NMS，合并来自重叠分块的重复项。 |
| **工具** | **SAHI**（Slicing Aided Hyper Inference）是配合 YOLO 及其他检测器进行分块推理的常用库。也可用自定义脚本切分图像、运行推理并合并。 |
| **性能** | 分块能提升高分辨率图像的检测准确率，但**增加计算与内存**：每张图大量分块意味着大量前向传播。在 Jetson 上需权衡分块尺寸、重叠与吞吐。 |

**何时使用：**  
分块对**航拍影像**、**无人机视频**和**医学影像**尤为有用，这些场景图像很大（例如 1920×1080、4K）且小细节至关重要。当单帧下采样会使目标小到模型无法处理时，就应使用分块。

**如何实现：**

- **SAHI（Slicing Aided Hyper Inference）：**  
  安装 `sahi`，然后用它切分图像（或视频帧），在每个切片上运行 YOLO（或其他）模型，并合并结果。支持重叠比例、切片尺寸，以及合并后的 NMS。

  ```bash
  pip install sahi
  ```

  Example pattern（概念）：定义切片尺寸与重叠，用你的模型和图像运行 `get_sliced_prediction()`（或等效调用）；SAHI 返回原始图像坐标下合并后的检测结果。参见上方的 [demo 输出](#demo-output-small-object-detection) 图像，了解 SAHI 在拥挤、多尺度场景下的输出示例。

- **自定义流水线：**  
  1. 将图像切分为分块（例如 640×640，stride = 640×(1 − overlap)）。  
  2. 在每个分块上运行检测器；收集框。  
  3. 用分块左上角偏移框坐标。  
  4. 对合并后的列表运行 NMS（IoU 阈值例如 0.5），丢弃来自重叠分块的重复项。

- **训练：**  
  也可以在分块上**训练**（例如将高分辨率图像裁剪为重叠 patch，并标注或把标签映射到分块）。这样模型始终看到“放大”后的内容；推理时采用相同的分块策略。

**Jetson 注意事项：**  
分块会使每帧的推理次数成倍增加。对于实时视频，按需减少分块数量/尺寸或降低重叠，或在准确率比延迟更重要时，把分块留给关键帧 / 离线处理。

---

### 达成目标的建议应用顺序

1. **快速见效（不改模型）：**  
   **提高输入分辨率**（以 832 或 960 训练与推理）+ **多尺度训练**（Ultralytics 默认启用）+ **置信度/NMS 调参**（见 §8）。在 VisDrone val 上验证 mAP；若小类 mAP 可接受且 Jetson 上的延迟可接受，到此为止。

2. **下一步（更优的小目标 mAP）：**  
   尝试**已发表的小目标变体**：**SPD-YOLOv8**（backbone 中加入 SPD）或 **LPAE-YOLOv8**（额外的小目标 head + 轻量 attention）。用相同的数据流水线在 VisDrone 上训练；对比 mAP 与推理速度，与 vanilla YOLOv8s 比较。

3. **若需在不损失召回的情况下减少误报：**  
   训练变体时留意 **anchor/loss 调参**（例如 Wise-IoU）；部署后调 **置信度与 NMS**（见 §8）。

4. **Jetson 部署：**  
   优先选用 **YOLOv8n 或 YOLOv8s**（或等效变体）+ **FP16 或 INT8** TensorRT。若 960 太慢，可用 **832** 作为折中分辨率；记录分辨率 vs 准确率 vs 延迟的取舍。

5. **超高分辨率或 4K 无人机视频：**  
   使用**分块**（例如 **SAHI** 或自定义切片）：重叠分块（例如 25% 重叠），逐分块运行检测器，在整图坐标下合并并 NMS。在 Jetson 上权衡分块数量/重叠与实时吞吐。


<details>
<summary>English original</summary>

**7. Tiling (slicing / tiled inference)**

**What it is:**  
**Tiling** (also called **slicing** or **sliding-window inference**) divides high-resolution images into smaller, often **overlapping** patches (tiles). The detector runs on each tile at native model input size (e.g. 640×640), so small objects in the full image are seen at a larger effective scale inside the tile. At inference, detections from all tiles are **merged** and transformed back to the original image coordinates (with NMS across overlapping regions to remove duplicates).

**Why it helps small objects:**  
- In a single pass at 640×640, a 4K frame is heavily downsampled and tiny objects can shrink to a handful of pixels or less.  
- By tiling (e.g. 640×640 windows with overlap), each tile is at full model resolution; a small object that would be 2×2 pixels in a downsampled full image may be 10×10 or more inside one tile, making it detectable.  
- Overlap ensures objects near tile boundaries are not cut in half and missed; they appear fully in at least one tile.

**Key aspects:**

| Aspect | Recommendation |
|--------|----------------|
| **Overlapping tiles** | Use overlap (e.g. **25%** or 50%) so objects at tile edges are fully contained in at least one tile. Reduces boundary cut-off and missed detections. |
| **Inference flow** | Run the model on each tile → get boxes in tile coordinates → map boxes to full-image coordinates → run NMS over all detections to merge duplicates from overlapping tiles. |
| **Tools** | **SAHI** (Slicing Aided Hyper Inference) is a popular library for tiled inference with YOLO and other detectors. Custom scripts can also slice images, run inference, and merge. |
| **Performance** | Tiling improves detection accuracy on high-res imagery but **increases compute and memory**: many tiles per image mean many forward passes. On Jetson, balance tile size, overlap, and throughput. |

**When to use:**  
Tiling is especially useful for **aerial imagery**, **drone footage**, and **medical imaging**, where images are large (e.g. 1920×1080, 4K) and small details are critical. Use it when single-frame downsampling would make your targets too small for the model.

**How to achieve it:**

- **SAHI (Slicing Aided Hyper Inference):**  
  Install `sahi`, then use it to slice the image (or video frame), run your YOLO (or other) model on each slice, and merge results. Supports overlap ratio, slice size, and NMS after merge.

  ```bash
  pip install sahi
  ```

  Example pattern (concept): define slice dimensions and overlap, run `get_sliced_prediction()` (or equivalent) with your model and image; SAHI returns merged detections in original image coordinates. See the [demo output](#demo-output-small-object-detection) image above for an example of SAHI output on a crowded, multi-scale scene.

- **Custom pipeline:**  
  1. Split image into tiles (e.g. 640×640, stride = 640×(1 − overlap)).  
  2. Run detector on each tile; collect boxes.  
  3. Offset box coordinates by tile top-left.  
  4. Run NMS on the combined list (IoU threshold e.g. 0.5) to drop duplicates from overlapping tiles.

- **Training:**  
  You can also **train** on tiles (e.g. crop high-res images into overlapping patches and annotate or map labels to tiles). That way the model always sees “zoomed” content; at inference, use the same tiling strategy.

**Jetson note:**  
Tiling multiplies inference count per frame. For real-time video, use fewer/smaller tiles or lower overlap if needed, or reserve tiling for key frames / offline processing when accuracy matters more than latency.

---

**Suggested application order to achieve the goal**

1. **Quick wins (no model change):**  
   **Higher input resolution** (train and infer at 832 or 960) + **multi-scale training** (default in Ultralytics) + **confidence/NMS tuning** (see §8). Validate mAP on VisDrone val; if small-class mAP is acceptable and latency on Jetson is fine, stop here.

2. **Next step (better small-object mAP):**  
   Try a **published small-object variant**: **SPD-YOLOv8** (SPD in backbone) or **LPAE-YOLOv8** (extra small-object head + lightweight attention). Train on VisDrone with the same data pipeline; compare mAP and inference speed vs vanilla YOLOv8s.

3. **If you need to reduce false positives without losing recall:**  
   Keep **anchor/loss tuning** in mind (e.g. Wise-IoU) when training a variant; tune **confidence and NMS** after deployment (see §8).

4. **Jetson deployment:**  
   Prefer **YOLOv8n or YOLOv8s** (or equivalent variant) + **FP16 or INT8** TensorRT. Use **832** as a compromise resolution if 960 is too slow; document the resolution vs accuracy vs latency tradeoff.

5. **Very high-res or 4K drone footage:**  
   Use **tiling** (e.g. **SAHI** or custom slicing): overlapping tiles (e.g. 25% overlap), run detector per tile, merge and NMS in full-image coordinates. Trade off tile count/overlap vs real-time throughput on Jetson.

</details>

### 数据增强（对小目标很重要）

- **尺度：** 多尺度训练（如 0.5–1.5× 或类似范围），让模型在不同分辨率下见到小目标。
- **Mosaic / MixUp：** 提升鲁棒性；需谨慎使用，避免把小实例过度模糊。
- **运动模糊、亮度/对比度：** 模拟困难的无人机条件。
- **Copy-paste（可选）：** 把小实例粘贴到其他图像上，以提高小目标密度。

---

## 5. 训练流水线

### 环境

- Python 3.8+；支持 CUDA 的 GPU（在 Jetson 上训练可行但很慢；训练时优先用桌面 GPU 或云端）。
- 安装：`ultralytics`（YOLOv8），可选安装带 CUDA 的 PyTorch。

```bash
pip install ultralytics
# or from source for latest
```

### 步骤

1. **准备数据：** 下载 VisDrone2019-DET 的 train/val；把标注转换成 YOLO 格式；按上文创建 `data.yaml`。
2. **训练：** 从 COCO 预训练的 YOLOv8 做迁移学习：

   ```bash
   yolo detect train data=path/to/VisDrone/data.yaml model=yolov8s.pt epochs=100 imgsz=640 batch=16
   ```

   按需调整 `imgsz`（例如 832 以获得更多小目标信号）、`batch` 和 `epochs`。对 Jetson，`yolov8n.pt` 或 `yolov8s.pt` 是典型取值。
3. **验证：**  
   `yolo detect val model=runs/detect/train/weights/best.pt data=path/to/VisDrone/data.yaml`
4. **调优：** 若小目标类别的 mAP 偏低，可提高分辨率、增加增强，或尝试面向小目标的变体（见 [Resources](#10-resources)）。

### 置信度与 NMS（用于部署）

- **置信度阈值：** 调低（如 0.2–0.35）可提升小目标的召回率，代价是更多误检；在验证集上调。
- **NMS IoU：** 略微提高 IoU（如 0.5–0.6）有助于合并小实例上的重复框；用你的指标验证。

---

## 6. 导出与 TensorRT 优化

### 先导出到 ONNX 再转 TensorRT

```bash
# From best.pt
yolo export model=runs/detect/train/weights/best.pt format=onnx simplify=True
yolo export model=runs/detect/train/weights/best.pt format=engine device=0 half=True  # FP16 on Jetson
# INT8 (recommended on Jetson; provide calibration data if needed)
yolo export model=runs/detect/train/weights/best.pt format=engine device=0 int8=True data=path/to/VisDrone/data.yaml
```

或使用 `trtexec` 获得更多控制（批大小、workspace、精度）。

### 优化建议（Jetson Orin Nano）

- **FP16：** 速度与准确率权衡下的默认选择。
- **INT8：** 吞吐最佳；用 VisDrone（或你的部署域）的代表性子集做校准，以限制准确率下降。
- **输入尺寸：** 与训练保持一致（如 640 或 832）；更大尺寸可提升小目标检测，但会增加延迟。
- **DLA：** 对受支持的 layer，卸载到 DLA 可释放 GPU 去做其他任务；对 GPU、DLA、GPU+DLA 分别做 benchmark。

导出后验证 mAP（或你的指标）：如果你的框架支持，用 `.engine` 模型运行同一套验证脚本。

---

## 7. 在 Jetson 上集成 DeepStream

### 模型的角色

- VisDrone 训练出的模型在现有 DeepStream + GStreamer + FastAPI 流水线中充当**主检测器**。
- DeepStream 的 `nvinfer`（primary GIE）加载 TensorRT engine；对解码后的帧跑推理；可选地对裁剪出的检测结果跑一个二级分类器（secondary GIE）。

### 配置

- 采用主 [ML and AI — DeepStream](../Guide.md) 指南中的同一模式：
  - **Engine：** 你生成的 `.engine`（来自 VisDrone 训练的 YOLOv8）。
  - **标签：** 10 个类别（或你保留的任意数量）；在配置中给出标签文件路径。
  - **预处理：** 输入尺寸（如 640 或 832），归一化方式与训练一致。
  - **后处理：** 置信度阈值与 NMS（如 `nms-iou-threshold`、`cluster-mode`）针对小目标调优。

### 减少误检

- 略微提高置信度阈值，和/或收紧 NMS。
- 可选：加一个轻量二级分类器（例如作用于裁剪出的 patch），对类别重新打分或过滤。
- 时序一致性：使用跟踪（如 DeepStream tracker），并要求检测在连续若干帧上持续存在后，才暴露给 API。

### 小目标/快速移动目标的稳定性

- 用内置跟踪器（如 IOU 或 NvDCF）跨帧关联检测。
- 可选：在把结果返回 FastAPI 之前，做短窗口内的时序平滑或投票。

---

## 8. 调优置信度与 NMS

后处理有两个主要旋钮：**置信度阈值**（保留哪些检测）和 **NMS IoU 阈值**（以多大力度合并重叠框）。两者都会影响召回率、精确率以及在小目标上的表现。


<details>
<summary>English original</summary>

**Data Augmentation (important for small objects)**

- **Scale:** Multi-scale training (e.g. 0.5–1.5× or similar) so the model sees small objects at different resolutions.
- **Mosaic / MixUp:** Improves robustness; use with care to avoid over-blurring small instances.
- **Motion blur, brightness/contrast:** Simulate challenging drone conditions.
- **Copy-paste (optional):** Paste small instances onto other images to increase small-object density.

---

**5. Training Pipeline**

**Environment**

- Python 3.8+; CUDA-capable GPU (training on Jetson is possible but slow; prefer a desktop GPU or cloud for training).
- Install: `ultralytics` (YOLOv8), and optionally PyTorch with CUDA.

```bash
pip install ultralytics
# or from source for latest
```

**Steps**

1. **Prepare data:** Download VisDrone2019-DET train/val; convert annotations to YOLO format; create `data.yaml` as above.
2. **Train:** Transfer learning from COCO-pretrained YOLOv8:

   ```bash
   yolo detect train data=path/to/VisDrone/data.yaml model=yolov8s.pt epochs=100 imgsz=640 batch=16
   ```

   Adjust `imgsz` (e.g. 832 for more small-object signal), `batch`, and `epochs` as needed. For Jetson, `yolov8n.pt` or `yolov8s.pt` are typical.
3. **Validate:**  
   `yolo detect val model=runs/detect/train/weights/best.pt data=path/to/VisDrone/data.yaml`
4. **Tune:** If small-class mAP is low, increase resolution, add augmentations, or try a small-object–oriented variant (see [Resources](#10-resources)).

**Confidence and NMS (for deployment)**

- **Confidence threshold:** Lower (e.g. 0.2–0.35) can improve recall on small objects at the cost of more false positives; tune on val set.
- **NMS IoU:** Slightly higher IoU (e.g. 0.5–0.6) can help merge duplicate boxes on small instances; validate with your metric.

---

**6. Export and TensorRT Optimization**

**Export to ONNX then TensorRT**

```bash
# From best.pt
yolo export model=runs/detect/train/weights/best.pt format=onnx simplify=True
yolo export model=runs/detect/train/weights/best.pt format=engine device=0 half=True  # FP16 on Jetson
# INT8 (recommended on Jetson; provide calibration data if needed)
yolo export model=runs/detect/train/weights/best.pt format=engine device=0 int8=True data=path/to/VisDrone/data.yaml
```

Or use `trtexec` for more control (batch size, workspace, precision).

**Optimization Tips (Jetson Orin Nano)**

- **FP16:** Default choice for speed vs accuracy.
- **INT8:** Best throughput; calibrate with a representative subset of VisDrone (or your deployment domain) to limit accuracy drop.
- **Input size:** Match training (e.g. 640 or 832); larger size improves small-object detection but increases latency.
- **DLA:** For supported layers, offloading to DLA can free GPU for other tasks; benchmark GPU vs DLA vs GPU+DLA.

Validate mAP (or your metric) after export: run the same validation script with the `.engine` model if your framework supports it.

---

**7. DeepStream Integration on Jetson**

**Role of the Model**

- The VisDrone-trained model serves as the **primary detector** in the existing DeepStream + GStreamer + FastAPI pipeline.
- DeepStream’s `nvinfer` (primary GIE) loads the TensorRT engine; run inference on decoded frames; optionally run a secondary classifier (secondary GIE) on cropped detections.

**Configuration**

- Use the same pattern as in the main [ML and AI — DeepStream](../Guide.md) guide:
  - **Engine:** Your generated `.engine` (from VisDrone-trained YOLOv8).
  - **Labels:** 10 classes (or however many you kept); label file path in config.
  - **Preprocessing:** Input size (e.g. 640 or 832), normalize to match training.
  - **Post-processing:** Confidence threshold and NMS (e.g. `nms-iou-threshold`, `cluster-mode`) tuned for small objects.

**Reducing False Positives**

- Slightly raise confidence threshold and/or tighten NMS.
- Optional: add a lightweight secondary classifier (e.g. on cropped patches) to re-score or filter classes.
- Temporal consistency: use tracking (e.g. DeepStream tracker) and require detections to persist over several frames before exposing them to the API.

**Stability for Small / Fast-Moving Targets**

- Associate detections across frames with the built-in tracker (e.g. IOU or NvDCF).
- Optional: temporal smoothing or voting over a short window before returning results to FastAPI.

---

**8. Tuning Confidence and NMS**

Post-processing has two main knobs: **confidence threshold** (which detections to keep) and **NMS IoU threshold** (how aggressively to merge overlapping boxes). Both affect recall, precision, and behaviour on small objects.

</details>

### 每个参数的作用

| 参数 | 作用 |
|-----------|--------|
| **置信度阈值** | 分数低于该阈值的检测框被丢弃。**更低** → 检测更多（召回更高，假阳性更多）。**更高** → 检测更少但更可信（精度更高，可能漏掉小目标/模糊目标）。 |
| **NMS IoU 阈值** | 当两个框的 IoU 超过该值时，分数较低的那个被移除。**更低**的 IoU → 合并更激进（重复框更少，但可能把邻近的小目标过度合并）。**更高**的 IoU → 保留更多重叠框（对密集小目标更好，但重复检测更多）。 |

### 小目标为什么需要不同的调参

- **小目标**的原始置信度通常低于大目标（像素更少，上下文更少）。置信度阈值设得高（如 0.5）会抹掉大部分小目标检测。
- **密集的小实例**（如人群、停放的车辆）会产生大量重叠框；用默认 IoU（如 0.45）做 NMS 可能过度抑制。略**高**一些的 NMS IoU（如 0.5–0.6）能保留更多不同的小框。
- 背景或噪声上的**假阳性**分数往往处于中低区间；提高置信度有帮助，但太高会损害小目标的召回。

### 推荐的调参流程

1. **先固定 NMS（可选但有用）。**  
   用一个典型的置信度（如 0.25），在**验证集**上扫描 NMS IoU（如 0.4、0.45、0.5、0.55、0.6）。计算 mAP@0.5（如果有，还有 mAP@0.5:0.95）。选出让 mAP 最好、或对你的小目标类别取舍最佳的 IoU。
2. **扫描置信度。**  
   在 NMS 固定的前提下，扫描置信度（如 0.15、0.2、0.25、0.3、0.35、0.4、0.5），再次测量 mAP，必要时再测每个类别的精度/召回。
3. **选择工作点。**  
   - 偏向**召回**（如后接下游分类器或人工复核）：置信度取低（0.2–0.3）。  
   - 偏向**精度**（误报更少）：置信度取高（0.35–0.5）。  
   - 专门针对小目标时，从 **conf ≈ 0.25、NMS IoU ≈ 0.5** 起步，再逐步调整。

### 在哪里设置

**YOLOv8（验证/推理）：**

```bash
# Validation with custom conf and IoU
yolo detect val model=best.pt data=data.yaml conf=0.25 iou=0.5

# Predict: pass at inference
yolo detect predict model=best.pt source=images/ conf=0.25 iou=0.5
```

在 Python 中：

```python
from ultralytics import YOLO
model = YOLO("best.pt")
model.val(data="data.yaml", conf=0.25, iou=0.5)
# or predict
model.predict(source="images/", conf=0.25, iou=0.5)
```

**DeepStream（nvinfer）：**  
在配置文件中（如 `config_infer_primary_*.txt` 或 app config）：

- **置信度：** `threshold` 或 `conf-threshold`（取决于自定义解析器；通常为 0–1）。
- **NMS：** `nms-iou-threshold`（或类似项）。设为你选定的同一个值（如 0.5）。

如果你的流水线使用自定义后处理器，就把相同的 `conf` 和 `iou` 传给它，使行为与验证时一致。

### 快速参考

| 目标 | 置信度 | NMS IoU |
|------|------------|---------|
| 最大化召回（小目标） | 0.20–0.30 | 0.50–0.60 |
| 均衡 | 0.25–0.35 | 0.45–0.55 |
| 减少假阳性 | 0.35–0.50 | 0.45–0.50 |

改动任一参数后，务必在验证集上重新检查 mAP（以及可选的每类精度/召回）。

---

## 9. 端到端检查清单

- [ ] 从 [VisDrone/VisDrone-Dataset](https://github.com/VisDrone/VisDrone-Dataset) 下载 VisDrone2019-DET 的训练集和验证集（或使用关联的镜像）。
- [ ] 把标注转换为 YOLO 格式；创建 `data.yaml`，其中 `path`、`train`、`val`、`nc`、`names` 要正确。
- [ ] 从 COCO 预训练权重出发训练 YOLOv8（n/s/m）；使用多尺度以及对小目标友好的数据增强。
- [ ] 验证 mAP（总体和每类）；针对小目标召回与假阳性之间的取舍调优 confidence/NMS。
- [ ] 导出为 TensorRT（FP16 或 INT8）；在 Jetson 上验证准确率。
- [ ] 把 engine 集成到 DeepStream primary GIE；设置批大小、输入尺寸和标签。
- [ ] 在 DeepStream 配置中调优 confidence 和 NMS；如有需要，加入跟踪和可选的二级分类。
- [ ] 运行完整流水线（视频接入 → 检测 → 可选分类 → 推流/录制）；测量延迟和质量；记录分辨率与速度的取舍。
- [ ] 若使用高分辨率或 4K 输入：考虑使用**分块**（如 SAHI）并让分块之间相互重叠；合并检测结果并跑 NMS；在 Jetson 上调优分块尺寸/重叠与吞吐之间的取舍。

---

## 10. 资源

### 数据集

- [VisDrone/VisDrone-Dataset](https://github.com/VisDrone/VisDrone-Dataset) — 官方仓库与相关链接。
- [VisDrone 数据集（Ultralytics）](https://docs.ultralytics.com/datasets/detect/visdrone/) — 格式与 YOLO 用法。
- [VisDrone2YOLO](https://github.com/adityatandon/VisDrone2YOLO) — 转换脚本和 YOLO 格式标注。
- [YOLOv5 VisDrone.yaml](https://github.com/ultralytics/yolov5/blob/master/data/VisDrone.yaml) — 路径与转换的参考。


<details>
<summary>English original</summary>

**What each parameter does**

| Parameter | Effect |
|-----------|--------|
| **Confidence threshold** | Detections with score below this are dropped. **Lower** → more detections (higher recall, more false positives). **Higher** → fewer, more confident detections (higher precision, may miss small/ambiguous objects). |
| **NMS IoU threshold** | When two boxes overlap with IoU above this value, the lower-scoring one is removed. **Lower** IoU → more aggressive merging (fewer duplicates, can over-merge nearby small objects). **Higher** IoU → keep more overlapping boxes (better for dense small objects, but more duplicate detections). |

**Why small objects need different tuning**

- **Small objects** often get lower raw confidence than large ones (fewer pixels, less context). A high confidence threshold (e.g. 0.5) can wipe out most small detections.
- **Dense small instances** (e.g. crowds, parked cars) can produce many overlapping boxes; NMS with default IoU (e.g. 0.45) may over-suppress. Slightly **higher** NMS IoU (e.g. 0.5–0.6) keeps more distinct small boxes.
- **False positives** on background or noise tend to have mid–low scores; raising confidence helps, but too high hurts small-object recall.

**Recommended tuning flow**

1. **Fix NMS first (optional but useful).**  
   Use a typical confidence (e.g. 0.25) and sweep NMS IoU (e.g. 0.4, 0.45, 0.5, 0.55, 0.6) on the **validation set**. Compute mAP@0.5 (and mAP@0.5:0.95 if available). Pick the IoU that gives the best mAP or best tradeoff for your small classes.
2. **Sweep confidence.**  
   With NMS fixed, sweep confidence (e.g. 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.5) and again measure mAP and, if needed, precision/recall per class.
3. **Choose operating point.**  
   - Prefer **recall** (e.g. downstream classifier or human review): lower confidence (0.2–0.3).  
   - Prefer **precision** (fewer false alarms): higher confidence (0.35–0.5).  
   - For small objects specifically, start with **conf ≈ 0.25, NMS IoU ≈ 0.5** and adjust from there.

**Where to set them**

**YOLOv8 (validation / inference):**

```bash
# Validation with custom conf and IoU
yolo detect val model=best.pt data=data.yaml conf=0.25 iou=0.5

# Predict: pass at inference
yolo detect predict model=best.pt source=images/ conf=0.25 iou=0.5
```

In Python:

```python
from ultralytics import YOLO
model = YOLO("best.pt")
model.val(data="data.yaml", conf=0.25, iou=0.5)
# or predict
model.predict(source="images/", conf=0.25, iou=0.5)
```

**DeepStream (nvinfer):**  
In the config (e.g. `config_infer_primary_*.txt` or app config):

- **Confidence:** `threshold` or `conf-threshold` (depends on custom parser; often 0–1).
- **NMS:** `nms-iou-threshold` (or similar). Set to the same value you chose (e.g. 0.5).

If your pipeline uses a custom post-processor, pass the same `conf` and `iou` there so behaviour matches validation.

**Quick reference**

| Goal | Confidence | NMS IoU |
|------|------------|---------|
| Maximize recall (small objects) | 0.20–0.30 | 0.50–0.60 |
| Balanced | 0.25–0.35 | 0.45–0.55 |
| Reduce false positives | 0.35–0.50 | 0.45–0.50 |

Always re-check mAP (and optional per-class precision/recall) on the val set after changing either parameter.

---

**9. End-to-End Checklist**

- [ ] Download VisDrone2019-DET train and val from [VisDrone/VisDrone-Dataset](https://github.com/VisDrone/VisDrone-Dataset) (or linked mirrors).
- [ ] Convert annotations to YOLO format; create `data.yaml` with correct `path`, `train`, `val`, `nc`, `names`.
- [ ] Train YOLOv8 (n/s/m) from COCO pretrained; use multi-scale and small-object–friendly augmentation.
- [ ] Validate mAP (overall and per-class); tune confidence/NMS for small-object recall vs false positives.
- [ ] Export to TensorRT (FP16 or INT8); verify accuracy on Jetson.
- [ ] Integrate engine into DeepStream primary GIE; set batch size, input size, and labels.
- [ ] Tune confidence and NMS in DeepStream config; add tracking and optional secondary classification if needed.
- [ ] Run full pipeline (video ingest → detection → optional classification → streaming/recording); measure latency and quality; document resolution vs speed tradeoffs.
- [ ] If using high-res or 4K input: consider **tiling** (e.g. SAHI) with overlapping tiles; merge detections and run NMS; tune tile size/overlap vs throughput on Jetson.

---

**10. Resources**

**Dataset**

- [VisDrone/VisDrone-Dataset](https://github.com/VisDrone/VisDrone-Dataset) — official repo and links.
- [VisDrone dataset (Ultralytics)](https://docs.ultralytics.com/datasets/detect/visdrone/) — format and YOLO usage.
- [VisDrone2YOLO](https://github.com/adityatandon/VisDrone2YOLO) — conversion script and YOLO-format labels.
- [YOLOv5 VisDrone.yaml](https://github.com/ultralytics/yolov5/blob/master/data/VisDrone.yaml) — reference for paths and conversion.

</details>

### 小目标检测（Drone / UAV）

- 针对航拍图像中小目标检测的改进 YOLOv8（多尺度特征、Wise-IoU）——例如面向 VisDrone 的 IEEE/Springer 变体。
- **SPD-YOLOv8** —— SPD-Conv、MPDIoU；在 VisDrone 上提升 mAP。
- **LPAE-YOLOv8** —— 轻量化；LSE-Head、小目标 layer、自适应 attention；VisDrone benchmark。
- **RLRD-YOLO** —— 面向 UAV 小目标检测的改进 YOLOv8。

### 分块 / 切片（高分辨率与小目标）

- **SAHI（Slicing Aided Hyper Inference）** —— [GitHub](https://github.com/obss/sahi)：在切片/重叠的分块上运行检测并合并结果；支持 YOLO 及其他后端。
- **Microsoft Learn** —— 重叠分块（如 25% 重叠），使边界处的目标被完整捕获。
- **“How to Use SAHI Tiling Windows to Detect Small Objects”**（Nicolai Nielsen，YouTube）—— 用 YOLO 做实际的分块推理。
- **“Improving Small Object Detection in 4K Drone Footage Using Tiling”**（Hailo Community）—— 面向航拍/无人机与边缘部署的分块。
- **CVF / IEEE** —— 高分辨率图像上小目标检测的分块；准确率与算力之间的取舍。

### 部署

- 主指南：[Edge AI Optimization](../../Guide.md) —— TensorRT、DeepStream、Jetson 性能剖析。
- [NVIDIA DeepStream SDK](https://developer.nvidia.com/deepstream-sdk) —— 流水线与 nvinfer 配置。
- [NVIDIA TAO Toolkit](../05-TAO工具包/Guide.md) —— 可选：用 TAO 训练/适配并导出到 TensorRT/DeepStream。

---

*返回 [ML and AI — Projects](../Guide.md)。*


<details>
<summary>English original</summary>

**Small-Object Detection (Drone / UAV)**

- Improved YOLOv8 for small object detection in aerial images (multi-scale features, Wise-IoU) — e.g. IEEE/Springer variants targeting VisDrone.
- **SPD-YOLOv8** — SPD-Conv, MPDIoU; improved mAP on VisDrone.
- **LPAE-YOLOv8** — lightweight; LSE-Head, small-object layer, adaptive attention; VisDrone benchmarks.
- **RLRD-YOLO** — improved YOLOv8 for UAV small-object detection.

**Tiling / Slicing (high-resolution and small objects)**

- **SAHI (Slicing Aided Hyper Inference)** — [GitHub](https://github.com/obss/sahi): run detection on sliced/overlapping tiles and merge results; supports YOLO and other backends.
- **Microsoft Learn** — overlapping tiles (e.g. 25% overlap) so objects at boundaries are fully captured.
- **“How to Use SAHI Tiling Windows to Detect Small Objects”** (Nicolai Nielsen, YouTube) — practical tiled inference with YOLO.
- **“Improving Small Object Detection in 4K Drone Footage Using Tiling”** (Hailo Community) — tiling for aerial/drone and edge deployment.
- **CVF / IEEE** — tiling for small object detection on high-resolution images; tradeoff between accuracy and compute.

**Deployment**

- Main guide: [Edge AI Optimization](../../Guide.md) — TensorRT, DeepStream, Jetson profiling.
- [NVIDIA DeepStream SDK](https://developer.nvidia.com/deepstream-sdk) — pipeline and nvinfer config.
- [NVIDIA TAO Toolkit](../05-TAO工具包/Guide.md) — optional: train/adapt with TAO and export to TensorRT/DeepStream.

---

*Back to [ML and AI — Projects](../Guide.md).*

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/5. ML and AI/small-object-detection-jetson/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/5.%20ML%20and%20AI/small-object-detection-jetson/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
