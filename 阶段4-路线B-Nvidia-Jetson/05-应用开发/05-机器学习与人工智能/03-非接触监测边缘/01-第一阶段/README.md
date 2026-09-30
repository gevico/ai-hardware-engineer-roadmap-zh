---
title: 阶段 1：相机校准与带深度的实时目标检测
description: 阶段 1：相机校准与带深度的实时目标检测
---

# 阶段 1：相机校准与带深度的实时目标检测

用于非接触式监测项目的流水线，使用 [Free-Viewpoint RGB-D Video Dataset](https://medialab.sjtu.edu.cn/post/free-viewpoint-rgb-d-video-dataset/)（SJTU）中的 **camera01**。

## 功能

- **相机校准：** 从 `Camera Parameters/paras.txt` 加载 camera01（索引 0）的内参（和外参）。
- **深度：** 使用数据集公式将灰度深度帧转换为度量深度（米）。
- **检测：** 通过 OpenCV 在 RGB 流上运行人脸（Haar）和人体（HOG）检测（无需额外模型文件）。
- **每个 ROI 的深度：** 对每个检测边界框，计算中位深度并叠加到图像上。

## 环境准备

```bash
cd "Phase 4 - Track B - Nvidia Jetson/5. Edge AI Optimization/non-contact-monitoring-edge"
pip install -r phase1/requirements.txt
```

确保数据集存在：

- `Free-Viewpoint-RGB-D-Video-Dataset-main/camera01-rgb.mp4`
- `Free-Viewpoint-RGB-D-Video-Dataset-main/camera01-depth.mp4`
- `Free-Viewpoint-RGB-D-Video-Dataset-main/Camera Parameters/paras.txt`

## 运行

在 `non-contact-monitoring-edge` 目录下：

```bash
python phase1/run_pipeline.py
```

或从 `phase1`：

```bash
cd phase1
python run_pipeline.py --dataset-dir ../Free-Viewpoint-RGB-D-Video-Dataset-main
```

### 选项

| 选项 | 说明 |
|--------|-------------|
| `--dataset-dir PATH` | 数据集根目录（默认：`../Free-Viewpoint-RGB-D-Video-Dataset-main`） |
| `--camera-index N` | paras.txt 中的相机索引（0 = camera01） |
| `--no-person` | 仅运行人脸检测（更快） |
| `--no-display` | 无 GUI（例如 headless）；仍会处理并打印进度 |
| `--out PATH` | 写出输出视频（例如 `phase1_out.mp4`） |
| `--max-frames N` | 最多处理 N 帧（0 = 全部） |

## 模块概览

| 模块 | 作用 |
|--------|------|
| `calibration.py` | 解析 `paras.txt`，暴露 K、R、t；世界到图像的投影。 |
| `depth_utils.py` | 灰度 → 度量深度；边界框内的中位深度。 |
| `detection.py` | 人脸（Haar）和人体（HOG）检测器；避免重复人体/人脸框的复合逻辑。 |
| `run_pipeline.py` | 主程序：加载校准，读取 RGB + 深度，检测，计算每个框的深度，可视化。 |

## 引用

使用数据集时，请引用：Guo S, Zhou K, Hu J, et al. A new free viewpoint video dataset and DIBR benchmark. *Proceedings of the 13th ACM Multimedia Systems Conference*. 2022: 265–271.


<details>
<summary>English original</summary>

**Phase 1: Camera Calibration and Real-Time Object Detection with Depth**

Pipeline for the non-contact monitoring project using **camera01** from the [Free-Viewpoint RGB-D Video Dataset](https://medialab.sjtu.edu.cn/post/free-viewpoint-rgb-d-video-dataset/) (SJTU).

**What it does**

- **Camera calibration:** Loads intrinsics (and extrinsics) from `Camera Parameters/paras.txt` for camera01 (index 0).
- **Depth:** Converts grayscale depth frames to metric depth (meters) using the dataset formula.
- **Detection:** Runs face (Haar) and person (HOG) detection on the RGB stream via OpenCV (no extra model files).
- **Depth per ROI:** For each detection bounding box, computes median depth and overlays it on the image.

**Setup**

```bash
cd "Phase 4 - Track B - Nvidia Jetson/5. Edge AI Optimization/non-contact-monitoring-edge"
pip install -r phase1/requirements.txt
```

Ensure the dataset is present:

- `Free-Viewpoint-RGB-D-Video-Dataset-main/camera01-rgb.mp4`
- `Free-Viewpoint-RGB-D-Video-Dataset-main/camera01-depth.mp4`
- `Free-Viewpoint-RGB-D-Video-Dataset-main/Camera Parameters/paras.txt`

**Run**

From the `non-contact-monitoring-edge` directory:

```bash
python phase1/run_pipeline.py
```

Or from `phase1`:

```bash
cd phase1
python run_pipeline.py --dataset-dir ../Free-Viewpoint-RGB-D-Video-Dataset-main
```

**Options**

| Option | Description |
|--------|-------------|
| `--dataset-dir PATH` | Root of the dataset (default: `../Free-Viewpoint-RGB-D-Video-Dataset-main`) |
| `--camera-index N` | Camera index in paras.txt (0 = camera01) |
| `--no-person` | Only run face detection (faster) |
| `--no-display` | No GUI (e.g. headless); still processes and prints progress |
| `--out PATH` | Write output video (e.g. `phase1_out.mp4`) |
| `--max-frames N` | Process at most N frames (0 = all) |

**Module overview**

| Module | Role |
|--------|------|
| `calibration.py` | Parse `paras.txt`, expose K, R, t; world-to-image projection. |
| `depth_utils.py` | Grayscale → metric depth; median depth in a bounding box. |
| `detection.py` | Face (Haar) and person (HOG) detectors; composite that avoids duplicate person/face boxes. |
| `run_pipeline.py` | Main: load calibration, read RGB + depth, detect, compute depth per box, visualize. |

**Citation**

When using the dataset, cite: Guo S, Zhou K, Hu J, et al. A new free viewpoint video dataset and DIBR benchmark. *Proceedings of the 13th ACM Multimedia Systems Conference*. 2022: 265–271.

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/5. ML and AI/non-contact-monitoring-edge/phase1/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/5.%20ML%20and%20AI/non-contact-monitoring-edge/phase1/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
