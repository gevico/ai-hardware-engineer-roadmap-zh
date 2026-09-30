---
title: BEVFusion — 鸟瞰图下的相机 + LiDAR 融合
description: BEVFusion — 鸟瞰图下的相机 + LiDAR 融合
---

# BEVFusion — 鸟瞰图下的相机 + LiDAR 融合

<div class="course-identity auto-course" style="--course-accent: #475569; --course-accent-rgb: 71, 85, 105;" markdown="1">
<div class="course-identity__icon">BCLF</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入剖析 · AI 工作负载</p>
<p class="course-identity__title">BEVFusion — 鸟瞰图下的相机 + LiDAR 融合的专题课程标识。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


> **是什么：** 多模态 3D 目标检测，在共享的 **BEV（鸟瞰图）** 特征空间中融合相机图像与 LiDAR 点云。
> **为什么：** 相机看到颜色/纹理；LiDAR 看到精确的深度/几何。在 BEV 中融合能兼得两者之长——准确率高于任一单模态。
> **硬件：** Jetson Orin Nano 8GB（推理），工作站/云端（训练）。

---


## 1. 为什么要做 BEV 融合？

### 早期融合方法的问题

**早期融合（原始数据）：**
```
Camera pixels + LiDAR points → concatenate → model
Problem: very different data formats, hard to align
```

**晚期融合（预测级）：**
```
Camera model → 2D boxes
LiDAR model  → 3D boxes
                → merge/NMS boxes
Problem: each modality's errors compound; no shared features
```

**BEV 融合（BEV 空间中的特征级）：**
```
Camera  → image encoder → lift to BEV features
LiDAR   → point encoder → voxelize to BEV features
                        → concatenate in BEV
                        → unified 3D detection head
```

每种模态都在任何决策做出之前，于特征层面贡献各自的长处。

### 各传感器的贡献

| 传感器   | 优势                              | 短板                              |
|----------|---------------------------------------|---------------------------------------|
| Camera   | 语义丰富（颜色、纹理、类别）| 无深度；受光照影响        |
| LiDAR    | 精确的 3D 几何，测距最远 100m | 无纹理；远处稀疏        |
| **BEV 融合** | **几何 + 语义**        | **更复杂，算力开销更高**      |

### 与单模态的性能对比

在 nuScenes benchmark 上（mAP / NDS）：

```
LiDAR only   (PointPillars):   40.1 / 53.0
Camera only  (BEVDet):         29.8 / 38.8
BEVFusion    (MIT HAN Lab):    68.5 / 71.4   ← +28 mAP over LiDAR alone
```

---

## 2. BEV 空间详解

### 什么是鸟瞰图？

BEV 是一张俯视的 2D 网格，表示 3D 世界中的某个区域：

```
Real world:            BEV grid:
      z (up)           y (forward) ↑
      ↑                │   [car][car]
      │       →        │       [ego]
 ─────┼─────  →    ────┼────────────→ x (right)
 lidar│             │  [pedestrian]
      │             │
      y (forward)   BEV cell = e.g. 0.1m × 0.1m

Range: typically x ∈ [-51.2m, 51.2m], y ∈ [-51.2m, 51.2m], z ∈ [-5m, 3m]
Resolution: 0.1m/voxel → 1024×1024 BEV grid
```

### 为什么要用 BEV？

- **天然适合 3D 检测**：BEV 中物体不会重叠（不像相机视角下远处物体会显得很小）
- **易于从 LiDAR 读取几何**：一次简单的投影就把 Z 压到 BEV 平面上
- **输出对规划友好**：自动驾驶在 XY 平面上规划路线，与 BEV 一致
- **尺度不变**：物体在 BEV 中的大小与距离无关

---

## 3. BEVFusion 架构


<details>
<summary>English original</summary>

**BEVFusion — Camera + LiDAR Fusion in Bird's Eye View**

<div class="course-identity auto-course" style="--course-accent: #475569; --course-accent-rgb: 71, 85, 105;" markdown="1">
<div class="course-identity__icon">BCLF</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for BEVFusion — Camera + LiDAR Fusion in Bird's Eye View.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


> **What:** Multi-modal 3D object detection that fuses camera images and LiDAR point clouds in a shared **Bird's Eye View (BEV)** feature space.
> **Why:** Cameras see color/texture; LiDAR sees accurate depth/geometry. Fusion in BEV gets the best of both — better accuracy than either modality alone.
> **Hardware:** Jetson Orin Nano 8GB (inference), workstation/cloud (training).

---


**1. Why BEV Fusion?**

**The Problem with Earlier Fusion Approaches**

**Early fusion (raw data):**
```
Camera pixels + LiDAR points → concatenate → model
Problem: very different data formats, hard to align
```

**Late fusion (prediction-level):**
```
Camera model → 2D boxes
LiDAR model  → 3D boxes
                → merge/NMS boxes
Problem: each modality's errors compound; no shared features
```

**BEV Fusion (feature-level in BEV space):**
```
Camera  → image encoder → lift to BEV features
LiDAR   → point encoder → voxelize to BEV features
                        → concatenate in BEV
                        → unified 3D detection head
```

Each modality contributes its strengths at the feature level before any decision is made.

**What Each Sensor Contributes**

| Sensor   | Strength                              | Weakness                              |
|----------|---------------------------------------|---------------------------------------|
| Camera   | Rich semantics (color, texture, class)| No depth; affected by lighting        |
| LiDAR    | Precise 3D geometry, range up to 100m | No texture; sparse at distance        |
| **BEV Fusion** | **Geometry + Semantics**        | **More complex, higher compute**      |

**Performance vs. Single Modality**

On nuScenes benchmark (mAP / NDS):

```
LiDAR only   (PointPillars):   40.1 / 53.0
Camera only  (BEVDet):         29.8 / 38.8
BEVFusion    (MIT HAN Lab):    68.5 / 71.4   ← +28 mAP over LiDAR alone
```

---

**2. BEV Space Explained**

**What is Bird's Eye View?**

BEV is a top-down 2D grid that represents a region of the 3D world:

```
Real world:            BEV grid:
      z (up)           y (forward) ↑
      ↑                │   [car][car]
      │       →        │       [ego]
 ─────┼─────  →    ────┼────────────→ x (right)
 lidar│             │  [pedestrian]
      │             │
      y (forward)   BEV cell = e.g. 0.1m × 0.1m

Range: typically x ∈ [-51.2m, 51.2m], y ∈ [-51.2m, 51.2m], z ∈ [-5m, 3m]
Resolution: 0.1m/voxel → 1024×1024 BEV grid
```

**Why BEV?**

- **Natural for 3D detection**: objects don't overlap in BEV (unlike camera view where far objects are small)
- **Easy to read geometry from LiDAR**: a simple projection collapses Z into the BEV plane
- **Planning-friendly output**: self-driving plans routes in the XY plane, same as BEV
- **Scale invariant**: an object is the same size in BEV regardless of distance

---

**3. BEVFusion Architecture**

</details>

### 高层流水线

```
┌─────────────────────────────────────────────────────────────────┐
│  N cameras (e.g., 6 cameras: front, rear, left, right ×2)      │
│  Each: [3, H, W] image                                          │
└─────────────────────┬───────────────────────────────────────────┘
                      │
              [Image Encoder]           e.g., Swin-T or ResNet-50
              backbone + FPN
                      │
              [View Transformer]        Lift-Splat-Shoot
              2D features → 3D BEV     shape: [C, X, Y]
                      │
                      ▼
┌────────────────[BEV Feature Map]──────────────────────────────┐
│   Camera BEV features [C_cam, X, Y]                           │
│          +                                                    │
│   LiDAR BEV features  [C_lidar, X, Y]                        │
│          │                                                    │
│   [Fusion: concat → ConvNet]                                  │
│          ↓                                                    │
│   Fused BEV features  [C_fused, X, Y]                        │
└──────────────────────────┬────────────────────────────────────┘
                           │
                  [Detection Head]      CenterPoint / TransFusion
                           │
                  3D bounding boxes:
                  (x, y, z, w, l, h, heading, class, score)

┌────────────────────────────────────────────────────────────────┐
│  LiDAR point cloud: [N_points, 5]  (x, y, z, intensity, ring) │
└────────────────────────┬───────────────────────────────────────┘
                         │
                [Point Encoder]         VoxelNet or PointPillars
                pillarize + sparse conv
                         │
                LiDAR BEV [C_lidar, X, Y]
```

### 两个主要开源实现

| 仓库                      | 关注点                        | 论文          |
|---------------------------|------------------------------|---------------|
| **MIT HAN Lab BEVFusion** | 相机 + 激光雷达，nuScenes SoTA | ICRA 2023    |
| **NVIDIA BEVFusion**      | 类似，对 TensorRT 友好         | CVPR 2023    |

本指南以 **MIT HAN Lab** 版本为主要参考，并引入 NVIDIA 实现中的 TensorRT 导出优化。

---

## 4. 相机 → BEV：Lift-Splat-Shoot

### 核心问题

相机图像是 2D 投影。要从一个像素得到 3D 位置，就需要深度——但相机并不直接测量深度。

**Lift-Splat-Shoot (LSS)** 的解法是预测每个像素的离散深度分布，再把每个像素在多个候选深度上“lift”到 3D 空间：


<details>
<summary>English original</summary>

**High-Level Pipeline**

```
┌─────────────────────────────────────────────────────────────────┐
│  N cameras (e.g., 6 cameras: front, rear, left, right ×2)      │
│  Each: [3, H, W] image                                          │
└─────────────────────┬───────────────────────────────────────────┘
                      │
              [Image Encoder]           e.g., Swin-T or ResNet-50
              backbone + FPN
                      │
              [View Transformer]        Lift-Splat-Shoot
              2D features → 3D BEV     shape: [C, X, Y]
                      │
                      ▼
┌────────────────[BEV Feature Map]──────────────────────────────┐
│   Camera BEV features [C_cam, X, Y]                           │
│          +                                                    │
│   LiDAR BEV features  [C_lidar, X, Y]                        │
│          │                                                    │
│   [Fusion: concat → ConvNet]                                  │
│          ↓                                                    │
│   Fused BEV features  [C_fused, X, Y]                        │
└──────────────────────────┬────────────────────────────────────┘
                           │
                  [Detection Head]      CenterPoint / TransFusion
                           │
                  3D bounding boxes:
                  (x, y, z, w, l, h, heading, class, score)

┌────────────────────────────────────────────────────────────────┐
│  LiDAR point cloud: [N_points, 5]  (x, y, z, intensity, ring) │
└────────────────────────┬───────────────────────────────────────┘
                         │
                [Point Encoder]         VoxelNet or PointPillars
                pillarize + sparse conv
                         │
                LiDAR BEV [C_lidar, X, Y]
```

**Two Main Open-Source Implementations**

| Repo                      | Focus                        | Paper         |
|---------------------------|------------------------------|---------------|
| **MIT HAN Lab BEVFusion** | Camera + LiDAR, nuScenes SoTA | ICRA 2023    |
| **NVIDIA BEVFusion**      | Similar, TensorRT-friendly    | CVPR 2023    |

This guide uses the **MIT HAN Lab** version as the primary reference, with TensorRT export optimizations from the NVIDIA implementation.

---

**4. Camera → BEV: Lift-Splat-Shoot**

**The Core Problem**

A camera image is a 2D projection. To get 3D position from a pixel, you need depth — but cameras don't measure depth directly.

**Lift-Splat-Shoot (LSS)** solves this by predicting a discrete depth distribution per pixel, then "lifting" each pixel into 3D space at multiple candidate depths:

</details>

### Step 1: Lift (Pixel → 3D Points)

```python
# lss.py — Lift-Splat-Shoot core
import torch
import torch.nn as nn

class LiftSplat(nn.Module):
    def __init__(self, grid_conf, data_aug_conf, outC):
        super().__init__()

        # BEV grid configuration
        self.dx  = torch.tensor(grid_conf['xbound'][2])   # x resolution (m)
        self.bx  = torch.tensor(grid_conf['xbound'][0])   # x start (m)
        self.nx  = int((grid_conf['xbound'][1] - grid_conf['xbound'][0])
                       / grid_conf['xbound'][2])           # x cells

        # Same for y and z
        # ...

        # Depth discretization: D candidate depths
        # e.g., D=41 depths from 1m to 41m
        depth_bins = torch.arange(*grid_conf['dbound'])
        self.D = len(depth_bins)
        self.register_buffer('depth_bins', depth_bins)

        # Image encoder (produces depth distribution + image features)
        self.camencode = CamEncode(self.D, outC, downsample=16)

    def get_geometry(self, rots, trans, intrins, post_rots, post_trans):
        """
        Compute 3D coordinates for every pixel at every candidate depth.

        For each camera pixel (u, v) and depth candidate d:
          1. Pixel (u, v) → normalized camera ray direction
          2. Scale by depth d → 3D point in camera frame
          3. Apply rotation + translation → world frame (ego vehicle frame)

        Output shape: [B, N_cams, D, H, W, 3]
        """
        B, N = trans.shape[:2]

        # Pixel coordinates at each depth
        xs = torch.linspace(0, self.ogfW - 1, self.fW, device=self.depth_bins.device) \
                  .view(1, 1, 1, 1, self.fW).expand(B, N, self.D, self.fH, self.fW)
        ys = torch.linspace(0, self.ogfH - 1, self.fH, device=self.depth_bins.device) \
                  .view(1, 1, 1, self.fH, 1).expand(B, N, self.D, self.fH, self.fW)
        ds = self.depth_bins.view(1, 1, self.D, 1, 1) \
                 .expand(B, N, self.D, self.fH, self.fW)

        # Stack into homogeneous coords: [B, N, D, H, W, 3]
        points = torch.stack([xs * ds, ys * ds, ds], dim=-1)

        # Undo post-augmentation, apply intrinsic inverse, then extrinsic
        # points: camera frame → world frame
        # intrins shape: [B, N, 3, 3]
        points = points - post_trans.view(B, N, 1, 1, 1, 3)
        points = torch.inverse(post_rots).view(B, N, 1, 1, 1, 3, 3).matmul(
                     points.unsqueeze(-1)).squeeze(-1)
        points = torch.cat([points[:, :, :, :, :, :2]
                             / points[:, :, :, :, :, 2:3],
                            torch.ones_like(points[:, :, :, :, :, :1])], -1)
        points = torch.inverse(intrins).view(B, N, 1, 1, 1, 3, 3).matmul(
                     points.unsqueeze(-1)).squeeze(-1)
        points = rots.view(B, N, 1, 1, 1, 3, 3).matmul(points.unsqueeze(-1)).squeeze(-1)
        points += trans.view(B, N, 1, 1, 1, 3)

        return points    # [B, N_cams, D, H, W, 3] in ego frame

    def voxel_pooling(self, geom_feats, x):
        """
        Splat: scatter 3D feature points into BEV grid.

        geom_feats: [B, N, D, H, W, 3] — 3D coordinates per feature
        x:          [B, N, D, H, W, C] — image features at each point
        Output:     [B, C, X_bev, Y_bev] — BEV feature map
        """
        B, N, D, H, W, C = x.shape

        # Convert 3D coordinates to BEV grid indices
        geom_feats = ((geom_feats - (self.bx - self.dx / 2.)) / self.dx).long()

        # Flatten everything: [B*N*D*H*W, C] and matching indices
        x = x.reshape(B * N * D * H * W, C)
        geom_feats = geom_feats.reshape(B * N * D * H * W, 3)

        # Keep only points within BEV bounds
        kept = ((geom_feats[:, 0] >= 0) & (geom_feats[:, 0] < self.nx) &
                (geom_feats[:, 1] >= 0) & (geom_feats[:, 1] < self.ny) &
                (geom_feats[:, 2] >= 0) & (geom_feats[:, 2] < self.nz))
        x = x[kept]
        geom_feats = geom_feats[kept]

        # Sum features at each BEV cell (scatter_add)
        bev = torch.zeros(B, C, self.nx, self.ny, device=x.device)
        flat_idx = (geom_feats[:, 0] * self.ny + geom_feats[:, 1])
        bev.view(B, C, -1).scatter_add_(2, flat_idx.unsqueeze(0).unsqueeze(0)
                                          .expand(B, C, -1), x.T.unsqueeze(0))
        return bev  # [B, C, X_bev, Y_bev]
```

### 第 2 步：深度分布预测

网络为每个像素在深度分箱上预测一个 **D 维 softmax**：

```python
class CamEncode(nn.Module):
    def __init__(self, D, C, downsample):
        super().__init__()
        self.D = D
        self.C = C

        # Backbone (e.g., EfficientNet or ResNet)
        self.trunk = EfficientNet.from_pretrained("efficientnet-b0")

        # Depth + feature head: outputs [D + C] channels per pixel
        # D channels = depth distribution logits
        # C channels = semantic features
        self.depthnet = nn.Conv2d(320 + 112, D + C, kernel_size=1, padding=0)

    def get_depth_dist(self, x):
        return x[:, :self.D].softmax(dim=1)           # [B, D, H, W]

    def get_depth_feat(self, depth, x):
        # Outer product: depth distribution × features → [B, C, D, H, W]
        depth = depth.unsqueeze(1)                     # [B, 1, D, H, W]
        x = x[:, self.D:].unsqueeze(2)                 # [B, C, 1, H, W]
        return depth * x                               # [B, C, D, H, W]

    def forward(self, x):
        x = self.trunk(x)   # backbone features
        x = self.depthnet(x)
        depth = self.get_depth_dist(x)
        return self.get_depth_feat(depth, x)
```

---

## 5. LiDAR（激光雷达）→ BEV（鸟瞰图）：体素化与 Pillar


<details>
<summary>English original</summary>

**Step 2: Depth Distribution Prediction**

The network predicts a **D-dimensional softmax** over depth bins for each pixel:

```python
class CamEncode(nn.Module):
    def __init__(self, D, C, downsample):
        super().__init__()
        self.D = D
        self.C = C

        # Backbone (e.g., EfficientNet or ResNet)
        self.trunk = EfficientNet.from_pretrained("efficientnet-b0")

        # Depth + feature head: outputs [D + C] channels per pixel
        # D channels = depth distribution logits
        # C channels = semantic features
        self.depthnet = nn.Conv2d(320 + 112, D + C, kernel_size=1, padding=0)

    def get_depth_dist(self, x):
        return x[:, :self.D].softmax(dim=1)           # [B, D, H, W]

    def get_depth_feat(self, depth, x):
        # Outer product: depth distribution × features → [B, C, D, H, W]
        depth = depth.unsqueeze(1)                     # [B, 1, D, H, W]
        x = x[:, self.D:].unsqueeze(2)                 # [B, C, 1, H, W]
        return depth * x                               # [B, C, D, H, W]

    def forward(self, x):
        x = self.trunk(x)   # backbone features
        x = self.depthnet(x)
        depth = self.get_depth_dist(x)
        return self.get_depth_feat(depth, x)
```

---

**5. LiDAR → BEV: Voxelization and Pillars**

</details>

### PointPillars（快速，对 Jetson 友好）

PointPillars 将 3D 空间划分为垂直的柱体（pillar），用一个小型 MLP 处理每个 pillar，然后把特征散布到 2D BEV 网格中：

```python
# pointpillars_encoder.py
import torch
import torch.nn as nn

class PillarFeatureNet(nn.Module):
    """
    Encode points within each pillar into a fixed-size feature vector.
    Input:  [N_pillars, max_pts_per_pillar, 9]
            features: x, y, z, intensity, x_c, y_c, z_c, x_p, y_p
            (c = distance from pillar center, p = distance from pillar XY center)
    Output: [N_pillars, C]
    """
    def __init__(self, in_channels=9, out_channels=64):
        super().__init__()
        self.pfn = nn.Sequential(
            nn.Linear(in_channels, out_channels, bias=False),
            nn.BatchNorm1d(out_channels),
            nn.ReLU(),
        )

    def forward(self, pillars, num_points_per_pillar):
        # pillars: [N_pillars, P, 9]
        # Max pooling over points within each pillar
        pillar_feat = self.pfn(pillars.view(-1, pillars.shape[-1]))    # [N*P, C]
        pillar_feat = pillar_feat.view(pillars.shape[0], pillars.shape[1], -1)
        # Mask padding points
        mask = (torch.arange(pillars.shape[1], device=pillars.device)
                < num_points_per_pillar.unsqueeze(1)).float().unsqueeze(2)
        pillar_feat = pillar_feat * mask
        return pillar_feat.max(dim=1).values    # [N_pillars, C]

class PointPillarScatter(nn.Module):
    """
    Scatter pillar features back into 2D BEV grid.
    Input:  pillar_features [N_pillars, C], pillar_coords [N_pillars, 3] (batch, x, y)
    Output: BEV map [B, C, X, Y]
    """
    def __init__(self, nx, ny, C):
        super().__init__()
        self.nx, self.ny, self.C = nx, ny, C

    def forward(self, pillar_features, coords, batch_size):
        # coords: [N_pillars, 4] — batch_idx, z, y, x
        bev = torch.zeros(batch_size, self.C, self.ny, self.nx,
                          device=pillar_features.device)
        for b in range(batch_size):
            mask = coords[:, 0] == b
            feat = pillar_features[mask]                    # [M, C]
            c    = coords[mask]                              # [M, 4]
            bev[b, :, c[:, 2], c[:, 3]] = feat.T           # scatter
        return bev    # [B, C, Y, X]

class PointPillarsEncoder(nn.Module):
    def __init__(self, voxel_size=(0.1, 0.1, 8.0), point_cloud_range=(-51.2, -51.2, -5, 51.2, 51.2, 3)):
        super().__init__()
        self.voxel_size = voxel_size
        self.pc_range   = point_cloud_range

        nx = int((point_cloud_range[3] - point_cloud_range[0]) / voxel_size[0])  # 1024
        ny = int((point_cloud_range[4] - point_cloud_range[1]) / voxel_size[1])  # 1024

        self.pfn     = PillarFeatureNet(in_channels=9, out_channels=64)
        self.scatter = PointPillarScatter(nx, ny, C=64)

        # 2D backbone on BEV feature map
        self.backbone = nn.Sequential(
            nn.Conv2d(64, 128, 3, stride=2, padding=1),
            nn.BatchNorm2d(128), nn.ReLU(),
            nn.Conv2d(128, 128, 3, padding=1),
            nn.BatchNorm2d(128), nn.ReLU(),
        )

    def voxelize(self, points):
        """
        Convert raw point cloud [N, 4] (x,y,z,intensity) into pillars.
        Returns:
          pillars:       [M, max_pts, 9]
          coords:        [M, 4] (batch, z, y, x)
          num_pts:       [M]
        """
        # Implementation uses torch_scatter or custom CUDA kernel
        # in production (MIT BEVFusion uses a custom CUDA pillarization kernel)
        pass

    def forward(self, points, batch_size):
        pillars, coords, num_pts = self.voxelize(points)
        pillar_features = self.pfn(pillars, num_pts)           # [M, 64]
        bev = self.scatter(pillar_features, coords, batch_size)# [B, 64, Y, X]
        return self.backbone(bev)                              # [B, 128, Y/2, X/2]
```

### VoxelNet / 稀疏卷积（更高准确率）

若需要更高准确率（代价是更多计算量），可以先使用稀疏 3D 卷积，再压缩到 BEV：

```
Points → Voxelize [X, Y, Z, C]
       → Sparse 3D Conv (only on occupied voxels)
       → Collapse Z dimension → BEV [X, Y, C*Z]
```

MIT BEVFusion 的 LiDAR 分支使用 **VoxelNet 加稀疏卷积**（通过 `spconv` 库实现）。

---

## 6. BEV 特征融合

### 拼接 + 卷积融合

```python
# bev_fusion.py
import torch
import torch.nn as nn

class BEVFusionLayer(nn.Module):
    """
    Fuse camera BEV features and LiDAR BEV features.

    Camera BEV:  [B, C_cam, X, Y]   — semantic, lower spatial accuracy
    LiDAR BEV:   [B, C_lid, X, Y]   — precise geometry

    Both must have the same spatial resolution (X, Y).
    """
    def __init__(self, C_cam=80, C_lid=128, C_out=256):
        super().__init__()

        # Align feature channels before fusion
        self.cam_proj  = nn.Sequential(
            nn.Conv2d(C_cam, C_out // 2, 1, bias=False),
            nn.BatchNorm2d(C_out // 2), nn.ReLU()
        )
        self.lid_proj  = nn.Sequential(
            nn.Conv2d(C_lid, C_out // 2, 1, bias=False),
            nn.BatchNorm2d(C_out // 2), nn.ReLU()
        )

        # Fusion convolutions
        self.fusion = nn.Sequential(
            nn.Conv2d(C_out, C_out, 3, padding=1, bias=False),
            nn.BatchNorm2d(C_out), nn.ReLU(),
            nn.Conv2d(C_out, C_out, 3, padding=1, bias=False),
            nn.BatchNorm2d(C_out), nn.ReLU(),
        )

    def forward(self, cam_bev, lidar_bev):
        # Project to common channel count
        cam_feat   = self.cam_proj(cam_bev)     # [B, C_out/2, X, Y]
        lidar_feat = self.lid_proj(lidar_bev)   # [B, C_out/2, X, Y]

        # Concatenate along channel axis
        fused = torch.cat([cam_feat, lidar_feat], dim=1)   # [B, C_out, X, Y]
        return self.fusion(fused)                           # [B, C_out, X, Y]
```

### 对齐：处理不同的 BEV 分辨率

Camera BEV（来自 LSS）与 LiDAR（激光雷达）BEV（来自 PointPillars/VoxelNet）可能具有不同的分辨率：

```python
class ResolutionAlignedFusion(nn.Module):
    def __init__(self, C_cam, C_lid, C_out, cam_stride=2):
        super().__init__()
        # Camera BEV is typically lower resolution after FPN downsampling
        # Upsample camera to match LiDAR resolution
        self.cam_upsample = nn.Sequential(
            nn.Upsample(scale_factor=cam_stride, mode='bilinear', align_corners=False),
            nn.Conv2d(C_cam, C_cam, 3, padding=1, bias=False),
            nn.BatchNorm2d(C_cam), nn.ReLU()
        )
        self.fuse = BEVFusionLayer(C_cam, C_lid, C_out)

    def forward(self, cam_bev, lidar_bev):
        cam_bev_aligned = self.cam_upsample(cam_bev)
        return self.fuse(cam_bev_aligned, lidar_bev)
```

---

## 7. 3D 检测头

### CenterPoint Head（用于 BEVFusion）

CenterPoint 在 BEV 中把目标中心预测为高斯热力图，然后回归框属性：

```python
# centerpoint_head.py
import torch
import torch.nn as nn

class CenterPointHead(nn.Module):
    """
    Anchor-free 3D detection head.
    Input:  BEV feature map [B, C, X, Y]
    Output: Per-class heatmaps + regression maps
    """
    def __init__(self, in_channels=256, num_classes=10):
        super().__init__()

        # Heatmap head: predicts center probability per class
        self.heatmap_head = nn.Sequential(
            nn.Conv2d(in_channels, 64, 3, padding=1, bias=False),
            nn.BatchNorm2d(64), nn.ReLU(),
            nn.Conv2d(64, num_classes, 1)    # sigmoid applied in loss
        )

        # Regression heads: offset, height, size, rotation, velocity
        self.offset_head = nn.Sequential(
            nn.Conv2d(in_channels, 64, 3, padding=1, bias=False),
            nn.BatchNorm2d(64), nn.ReLU(),
            nn.Conv2d(64, 2, 1)    # (Δx, Δy) sub-voxel offset
        )
        self.height_head = nn.Sequential(
            nn.Conv2d(in_channels, 64, 3, padding=1, bias=False),
            nn.BatchNorm2d(64), nn.ReLU(),
            nn.Conv2d(64, 1, 1)    # z center
        )
        self.size_head = nn.Sequential(
            nn.Conv2d(in_channels, 64, 3, padding=1, bias=False),
            nn.BatchNorm2d(64), nn.ReLU(),
            nn.Conv2d(64, 3, 1)    # (log w, log l, log h)
        )
        self.rot_head = nn.Sequential(
            nn.Conv2d(in_channels, 64, 3, padding=1, bias=False),
            nn.BatchNorm2d(64), nn.ReLU(),
            nn.Conv2d(64, 2, 1)    # (sin θ, cos θ)
        )

    def forward(self, bev_features):
        return {
            'heatmap': self.heatmap_head(bev_features).sigmoid(),
            'offset':  self.offset_head(bev_features),
            'height':  self.height_head(bev_features),
            'size':    self.size_head(bev_features),
            'rot':     self.rot_head(bev_features),
        }

def decode_predictions(preds, bev_range, voxel_size, score_threshold=0.3, top_k=200):
    """
    Convert raw CenterPoint predictions to 3D bounding boxes.

    Returns list of:
      (x, y, z, w, l, h, heading_rad, class_id, score)
    """
    heatmap = preds['heatmap']   # [B, num_classes, X, Y]
    B, num_cls, X, Y = heatmap.shape

    results = []
    for b in range(B):
        boxes_batch = []
        for cls in range(num_cls):
            heat = heatmap[b, cls]    # [X, Y]

            # Find peaks above threshold
            # Non-maximum suppression via max pooling (peak = local max)
            heat_nms = torch.max_pool2d(heat.unsqueeze(0).unsqueeze(0), 3, 1, 1)
            heat_nms = heat_nms.squeeze()
            peaks = (heat == heat_nms) & (heat > score_threshold)

            ys, xs = peaks.nonzero(as_tuple=True)   # BEV grid coords

            if len(xs) == 0:
                continue

            # Top-K selection
            scores = heat[ys, xs]
            if len(scores) > top_k:
                topk_idx = scores.topk(top_k).indices
                xs, ys, scores = xs[topk_idx], ys[topk_idx], scores[topk_idx]

            # Decode box position from grid to metric coordinates
            offset = preds['offset'][b, :, ys, xs]   # [2, N]
            x_metric = (xs.float() + offset[0]) * voxel_size[0] + bev_range[0]
            y_metric = (ys.float() + offset[1]) * voxel_size[1] + bev_range[1]
            z_metric = preds['height'][b, 0, ys, xs]

            # Box size
            size = preds['size'][b, :, ys, xs].exp()  # [3, N] (w, l, h)

            # Heading
            rot = preds['rot'][b, :, ys, xs]           # [2, N] (sin, cos)
            heading = torch.atan2(rot[0], rot[1])

            for i in range(len(xs)):
                boxes_batch.append({
                    'x': x_metric[i].item(),
                    'y': y_metric[i].item(),
                    'z': z_metric[i].item(),
                    'w': size[0, i].item(),
                    'l': size[1, i].item(),
                    'h': size[2, i].item(),
                    'heading': heading[i].item(),
                    'class': cls,
                    'score': scores[i].item(),
                })
        results.append(boxes_batch)
    return results
```

---

## 8. nuScenes 数据集

nuScenes 是 **BEVFusion 的标准 benchmark**，也是所有现代多模态 3D 检测方法的标准 benchmark。每篇 BEVFusion 论文都在 nuScenes 上报告结果。在训练或评测之前，必须理解它的结构。

### nuScenes 是什么

```
nuScenes — by nuTonomy (acquired by Aptiv / Motional), released 2019
Location:  Boston (USA) + Singapore  ← day/night, rain, different traffic rules
License:   CC BY-NC-SA 4.0 (non-commercial research)
Paper:     Caesar et al., CVPR 2020 — "nuScenes: A Multimodal Dataset for AD"
```

### 传感器套件

nuScenes 采集的是**完整自动驾驶汽车传感器栈**——真实 AV 上会部署的每一个传感器：

```
┌─────────────────────────────────────────────────────────────┐
│                    Vehicle Top View                         │
│                                                             │
│            CAM_FRONT_LEFT  CAM_FRONT  CAM_FRONT_RIGHT       │
│                    ↖           ↑           ↗                │
│                                                             │
│  CAM_BACK_LEFT ←─────────[LIDAR_TOP]─────────→ CAM_BACK_RIGHT │
│                                                             │
│                    ↙           ↓           ↘                │
│                         CAM_BACK                           │
│                                                             │
│  RADAR:  FRONT + FRONT_LEFT + FRONT_RIGHT + BACK_LEFT + BACK_RIGHT │
└─────────────────────────────────────────────────────────────┘
```

| 传感器 | 型号 | 规格 |
|--------|-------|-------|
| **Camera ×6** | Basler acA1600-60gc | 1600×900 px，12 Hz，360° 环视 |
| **激光雷达 ×1** | Velodyne HDL-32E | 32 线，20 Hz，70m 量程，360° |
| **RADAR ×5** | Continental ARS 408-21 | 13 Hz，250m 量程，多普勒速度 |
| **IMU** | Applanix POS LV | 6 轴，100 Hz |
| **GPS** | Applanix POS LV | RTK，厘米级准确率 |

### 数据集统计

| 指标 | 数值 |
|--------|-------|
| 场景 | **1,000**（每个 20 秒） |
| 关键帧 | **40,000**（以 2 Hz 标注 = 每 0.5s 1 个） |
| 相机图像 | **1.4 million**（6 个相机 × 关键帧 + sweeps） |
| LiDAR sweeps | **390,000**（20 Hz × 20s × 1000 个场景） |
| RADAR sweeps | **1.4 million**（5 个 RADAR × 13 Hz） |
| 3D 边界框 | **1.4 million** |
| 检测类别 | **10**（检测任务用） |
| 完整类别集合 | **23**（含稀有与静态目标） |
| 属性 | **8**（可见性、活动状态） |
| 对比 KITTI | 标注多 7×，图像多 100× |

### 数据划分

| 划分 | 场景 | 关键帧 | 用途 |
|-------|--------|-----------|-----|
| **train** | 700 | 28,130 | 训练 |
| **val** | 150 | 6,019 | 开发期间验证 |
| **test** | 150 | 6,008 | 标签隐藏——提交至排行榜 |
| **mini** | 10 | 404 | 入门、调试 |

**从 mini 开始**——它几分钟即可下载完，结构与完整数据集相同，足以在拉取 300 GB 完整数据之前验证流水线是否跑通。

### 检测类别

BEVFusion 在这 **10 个检测类别**上评测（为 23 个完整类别的子集）：

```python
NUSCENES_DETECTION_CLASSES = [
    'car',                   # sedan, SUV, pickup
    'truck',                 # box truck, delivery
    'construction_vehicle',  # crane, bulldozer
    'bus',                   # city bus, tour bus
    'trailer',               # semi trailer (may be detached)
    'barrier',               # jersey barrier, temporary divider
    'motorcycle',
    'bicycle',
    'pedestrian',            # person on foot
    'traffic_cone',
]
```

### 下载

```bash
# 1. Register at https://www.nuscenes.org/ (free academic license)
# 2. Go to "Download" → agree to terms → download via browser or wget

# nuScenes mini (10 scenes, ~4 GB) — START HERE
# From nuscenes.org download page, get the wget command with your token:
wget -O nuScenes-v1.0-mini.tar.bz2 \
  "https://d36yt3mvayqg5m.cloudfront.net/public/v1.0/v1.0-mini.tgz"

# Extract
mkdir -p /data/nuscenes
tar -xjf nuScenes-v1.0-mini.tar.bz2 -C /data/nuscenes/

# nuScenes full (300 GB, split into ~30 blobs) — for real training
# Use the s3 CLI tool provided on nuscenes.org after registration
pip install awscli
# They provide a script: nuScenes-v1.0-data.sh
bash nuScenes-v1.0-data.sh --target /data/nuscenes/
```


<details>
<summary>English original</summary>

**Sensor Suite**

nuScenes captures the **full autonomous vehicle sensor stack** — every sensor you'd deploy on a real AV:

```
┌─────────────────────────────────────────────────────────────┐
│                    Vehicle Top View                         │
│                                                             │
│            CAM_FRONT_LEFT  CAM_FRONT  CAM_FRONT_RIGHT       │
│                    ↖           ↑           ↗                │
│                                                             │
│  CAM_BACK_LEFT ←─────────[LIDAR_TOP]─────────→ CAM_BACK_RIGHT │
│                                                             │
│                    ↙           ↓           ↘                │
│                         CAM_BACK                           │
│                                                             │
│  RADAR:  FRONT + FRONT_LEFT + FRONT_RIGHT + BACK_LEFT + BACK_RIGHT │
└─────────────────────────────────────────────────────────────┘
```

| Sensor | Model | Specs |
|--------|-------|-------|
| **Camera ×6** | Basler acA1600-60gc | 1600×900 px, 12 Hz, 360° surround |
| **LiDAR ×1** | Velodyne HDL-32E | 32 beams, 20 Hz, 70m range, 360° |
| **RADAR ×5** | Continental ARS 408-21 | 13 Hz, 250m range, Doppler velocity |
| **IMU** | Applanix POS LV | 6-axis, 100 Hz |
| **GPS** | Applanix POS LV | RTK, cm-level accuracy |

**Dataset Statistics**

| Metric | Value |
|--------|-------|
| Scenes | **1,000** (each 20 seconds) |
| Keyframes | **40,000** (annotated at 2 Hz = 1 per 0.5s) |
| Camera images | **1.4 million** (6 cameras × keyframes + sweeps) |
| LiDAR sweeps | **390,000** (20 Hz × 20s × 1000 scenes) |
| RADAR sweeps | **1.4 million** (5 radars × 13 Hz) |
| 3D bounding boxes | **1.4 million** |
| Detection classes | **10** (for detection task) |
| Full class set | **23** (including rare and static objects) |
| Attributes | **8** (visibility, activity state) |
| vs KITTI | 7× more annotations, 100× more images |

**Data Splits**

| Split | Scenes | Keyframes | Use |
|-------|--------|-----------|-----|
| **train** | 700 | 28,130 | Training |
| **val** | 150 | 6,019 | Validation during development |
| **test** | 150 | 6,008 | Hidden labels — submit to leaderboard |
| **mini** | 10 | 404 | Getting started, debugging |

**Start with mini** — it downloads in minutes, has the same structure as full, and is enough to verify your pipeline works before committing to the 300 GB full download.

**Detection Classes**

BEVFusion is evaluated on these **10 detection classes** (subset of 23 full classes):

```python
NUSCENES_DETECTION_CLASSES = [
    'car',                   # sedan, SUV, pickup
    'truck',                 # box truck, delivery
    'construction_vehicle',  # crane, bulldozer
    'bus',                   # city bus, tour bus
    'trailer',               # semi trailer (may be detached)
    'barrier',               # jersey barrier, temporary divider
    'motorcycle',
    'bicycle',
    'pedestrian',            # person on foot
    'traffic_cone',
]
```

**Download**

```bash
# 1. Register at https://www.nuscenes.org/ (free academic license)
# 2. Go to "Download" → agree to terms → download via browser or wget

# nuScenes mini (10 scenes, ~4 GB) — START HERE
# From nuscenes.org download page, get the wget command with your token:
wget -O nuScenes-v1.0-mini.tar.bz2 \
  "https://d36yt3mvayqg5m.cloudfront.net/public/v1.0/v1.0-mini.tgz"

# Extract
mkdir -p /data/nuscenes
tar -xjf nuScenes-v1.0-mini.tar.bz2 -C /data/nuscenes/

# nuScenes full (300 GB, split into ~30 blobs) — for real training
# Use the s3 CLI tool provided on nuscenes.org after registration
pip install awscli
# They provide a script: nuScenes-v1.0-data.sh
bash nuScenes-v1.0-data.sh --target /data/nuscenes/
```

</details>

### 磁盘上的数据集结构

```
/data/nuscenes/
├── v1.0-mini/                   ← or v1.0-trainval/ for full dataset
│   ├── scene.json               ← 10 scenes (mini) or 850 (trainval)
│   ├── sample.json              ← keyframes (one every 0.5s)
│   ├── sample_data.json         ← all sensor readings (cameras, LiDAR, radar)
│   ├── sample_annotation.json   ← 3D bounding boxes per keyframe
│   ├── calibrated_sensor.json   ← extrinsics + intrinsics per sensor
│   ├── ego_pose.json            ← vehicle pose at each timestamp
│   ├── instance.json            ← object instances (unique IDs across frames)
│   ├── category.json            ← class taxonomy
│   ├── attribute.json           ← object attributes (moving, stopped, etc.)
│   ├── visibility.json          ← annotation visibility levels
│   ├── log.json                 ← drive metadata (date, location, vehicle)
│   └── map.json                 ← map metadata
├── samples/                     ← keyframe sensor data (annotated)
│   ├── CAM_FRONT/               ← 1600×900 JPEG images
│   ├── CAM_FRONT_LEFT/
│   ├── CAM_FRONT_RIGHT/
│   ├── CAM_BACK/
│   ├── CAM_BACK_LEFT/
│   ├── CAM_BACK_RIGHT/
│   ├── LIDAR_TOP/               ← .bin files (N×5: x,y,z,intensity,ring)
│   └── RADAR_FRONT/             ← .pcd files
├── sweeps/                      ← intermediate frames (not annotated)
│   ├── CAM_FRONT/               ← denser temporal data between keyframes
│   ├── LIDAR_TOP/               ← LiDAR runs at 20 Hz, keyframes at 2 Hz
│   └── ...                      ← so 10 LiDAR sweeps between each keyframe
└── maps/                        ← HD maps as PNG + JSON
    ├── boston-seaport.png
    ├── singapore-onenorth.png
    └── *.json                   ← vectorized road/lane graphs
```

### 数据库模式（关系型）

nuScenes 使用以 JSON 文件形式存储的关系型数据库。理解其中的关联是正确加载数据的关键：

```
scene (20s clip)
  └── sample[] (keyframe every 0.5s, annotated)
        ├── sample_data[] (one per sensor: 6 cameras + LiDAR + 5 radars)
        │     ├── calibrated_sensor → intrinsics + extrinsics (T_sensor_ego)
        │     └── ego_pose          → vehicle pose in world (T_ego_world)
        └── sample_annotation[] (one 3D box per object per keyframe)
              ├── instance → tracks the same object across frames
              ├── category → class name
              └── attribute → moving/stopped/etc.
```

### nuScenes Devkit Python API

```python
from nuscenes.nuscenes import NuScenes

# Initialize — loads all JSON tables into memory (~2 GB RAM for full)
nusc = NuScenes(version='v1.0-mini', dataroot='/data/nuscenes', verbose=True)

# ── Scene ──────────────────────────────────────────────────────────────────
scene = nusc.scene[0]
print(scene['name'])           # 'scene-0061'
print(scene['description'])    # 'Parked truck, motorcycle, ped...'
print(scene['nbr_samples'])    # 39 keyframes in this scene

# ── Sample (keyframe) ───────────────────────────────────────────────────────
first_sample_token = scene['first_sample_token']
sample = nusc.get('sample', first_sample_token)
print(sample['timestamp'])     # Unix microseconds
print(sample.keys())
# dict_keys(['token', 'timestamp', 'prev', 'next', 'scene_token', 'data', 'anns'])

# ── Access Camera Image ─────────────────────────────────────────────────────
cam_token = sample['data']['CAM_FRONT']
cam_data = nusc.get('sample_data', cam_token)
img_path = nusc.get_sample_data_path(cam_token)
# img_path = '/data/nuscenes/samples/CAM_FRONT/n015-...-CAM_FRONT__1531281445162460.jpg'

import cv2
img = cv2.imread(img_path)  # 1600×900

# ── Access LiDAR Point Cloud ────────────────────────────────────────────────
lidar_token = sample['data']['LIDAR_TOP']
lidar_path, boxes, camera_intrinsic = nusc.get_sample_data(lidar_token)
import numpy as np
pts = np.fromfile(lidar_path, dtype=np.float32).reshape(-1, 5)
# pts[:, 0:3] = XYZ, pts[:, 3] = intensity, pts[:, 4] = ring index

# ── Get 3D Annotations ──────────────────────────────────────────────────────
for ann_token in sample['anns']:
    ann = nusc.get('sample_annotation', ann_token)
    print(ann['category_name'])     # 'vehicle.car'
    print(ann['translation'])       # [x, y, z] in ego frame at annotation time
    print(ann['size'])              # [width, length, height] in meters
    print(ann['rotation'])          # quaternion [w, x, y, z]
    print(ann['num_lidar_pts'])     # how many LiDAR points hit this object

# ── Calibrated Sensor (camera intrinsics + extrinsics) ─────────────────────
cam_data = nusc.get('sample_data', sample['data']['CAM_FRONT'])
cs = nusc.get('calibrated_sensor', cam_data['calibrated_sensor_token'])
K = np.array(cs['camera_intrinsic'])      # 3×3 intrinsic matrix
T_cam_ego = np.array(cs['translation'])   # camera position in ego frame
R_cam_ego = cs['rotation']                # quaternion

# ── Ego Pose (vehicle pose in world frame) ──────────────────────────────────
ep = nusc.get('ego_pose', cam_data['ego_pose_token'])
T_ego_world = np.array(ep['translation'])  # [x, y, z] in global map frame
R_ego_world = ep['rotation']               # quaternion

# ── Iterate Through a Scene ─────────────────────────────────────────────────
def iter_scene(nusc, scene):
    """Yield all samples in a scene in temporal order."""
    token = scene['first_sample_token']
    while token:
        sample = nusc.get('sample', token)
        yield sample
        token = sample['next']   # empty string at end

for sample in iter_scene(nusc, scene):
    print(sample['timestamp'])
```

### 用 Devkit 可视化

```python
# Render a sample — annotated BEV + 6 camera images + LiDAR
nusc.render_sample(sample['token'])

# Render just the LiDAR point cloud in BEV with GT boxes
nusc.render_sample_data(sample['data']['LIDAR_TOP'],
                        with_anns=True, axes_limit=50)

# Render camera image with projected LiDAR points + 2D boxes
nusc.render_sample_data(sample['data']['CAM_FRONT'],
                        with_anns=True)

# Render a full scene as video
nusc.render_scene_channel(scene['token'], channel='CAM_FRONT',
                          out_path='scene_front.avi')

# nuScenes-lidarseg: render semantic segmentation labels on point cloud
from nuscenes.eval.lidarseg.utils import colormap_to_colors
nusc.render_pointcloud_in_image(sample['token'],
                                pointsensor_channel='LIDAR_TOP',
                                camera_channel='CAM_FRONT',
                                render_intensity=False)
```

### 坐标系

nuScenes 使用三个坐标系——理解它们对 BEVFusion 至关重要：

```
Global (world) frame:
  Origin: fixed point in the map (city-level)
  Used for: ego_pose, global object tracks, HD maps

Ego (vehicle) frame:
  Origin: rear axle midpoint
  Used for: sample_annotation translations are in THIS frame
  +X: forward, +Y: left, +Z: up

Sensor frame:
  Origin: sensor mounting position
  Used for: raw point cloud data is in THIS frame
  calibrated_sensor gives T_sensor→ego transform

Conversion chain:
  LiDAR points (sensor frame)
    → apply calibrated_sensor.T → ego frame
    → apply ego_pose.T → global frame
    → (subtract target_ego_pose.T) → another ego frame
```

```python
from pyquaternion import Quaternion
import numpy as np

def lidar_to_ego(pts_lidar: np.ndarray, cs_record: dict) -> np.ndarray:
    """Transform LiDAR points from sensor frame to ego frame."""
    R = Quaternion(cs_record['rotation']).rotation_matrix   # 3×3
    t = np.array(cs_record['translation'])                  # (3,)
    return (R @ pts_lidar[:, :3].T).T + t

def ego_to_global(pts_ego: np.ndarray, ep_record: dict) -> np.ndarray:
    """Transform ego-frame points to global frame."""
    R = Quaternion(ep_record['rotation']).rotation_matrix
    t = np.array(ep_record['translation'])
    return (R @ pts_ego.T).T + t
```

### 评估指标（NDS）

BEVFusion 论文报告 **NDS（nuScenes Detection Score）**——一个复合指标：

```
NDS = (1/5) × (5 × mAP + (1 - mATE) + (1 - mASE) + (1 - mAOE) + (1 - mAAE) + (1 - mAVE))

where:
  mAP  = mean Average Precision (matching threshold: BEV center distance, not IoU)
  mATE = mean Average Translation Error  [meters]   ← lower is better
  mASE = mean Average Scale Error        [1 - IoU]  ← lower is better
  mAOE = mean Average Orientation Error  [radians]  ← lower is better
  mAAE = mean Average Attribute Error    [1 - acc]  ← lower is better
  mAVE = mean Average Velocity Error     [m/s]      ← lower is better

Key difference from KITTI:
  nuScenes uses center-distance matching (2D BEV), not 3D IoU.
  Threshold: 0.5m, 1.0m, 2.0m, 4.0m → average over thresholds.
  This makes nearby objects count more (right for AV safety).
```

### BEVFusion 在 nuScenes 上的参考性能

| 方法 | 相机 | 激光雷达 | mAP | NDS |
|--------|---------|-------|-----|-----|
| PointPillars | — | ✓ | 40.1 | 53.0 |
| BEVDet | ✓ (6) | — | 29.8 | 38.8 |
| BEVFusion (MIT) | ✓ (6) | ✓ | **68.5** | **71.4** |
| BEVFusion (NVIDIA) | ✓ (6) | ✓ | 67.9 | 71.0 |

nuScenes val 集。融合相比仅激光雷达提升 +28 mAP，相比仅相机提升 +38 mAP。

---

## 9. 环境搭建与安装

### 克隆与搭建（MIT HAN Lab BEVFusion）

```bash
# On workstation (training) or Jetson (inference)
git clone https://github.com/mit-han-lab/bevfusion.git
cd bevfusion

# Install dependencies
pip3 install torch torchvision --index-url https://download.pytorch.org/whl/cu121
pip3 install mmcv-full==1.7.0 -f https://download.openmmlab.com/mmcv/dist/cu121/torch2.1/index.html
pip3 install mmdet3d==1.1.1
pip3 install nuscenes-devkit

# Build custom CUDA ops
cd mmdet3d && pip3 install -v -e .

# Install spconv (sparse convolution for LiDAR encoder)
pip3 install spconv-cu121   # match your CUDA version

# Install cumm (needed by spconv)
pip3 install cumm-cu121
```

### Orin Nano——纯推理环境搭建

```bash
# On Jetson, skip training deps, use Jetson-optimized wheels
sudo apt-get install -y python3-pip libopenblas-dev

# PyTorch for Jetson (from NVIDIA)
# Check: https://forums.developer.nvidia.com/t/pytorch-for-jetson/72048
# Example for JetPack 6 / CUDA 12.x:
pip3 install torch-*.whl torchvision-*.whl   # from NVIDIA's Jetson PyTorch page

# Minimal inference deps only
pip3 install nuscenes-devkit numpy opencv-python pyyaml

# Pre-built mmcv for Jetson (or build from source)
pip3 install mmcv==2.1.0

# Verify GPU is used
python3 -c "import torch; print(torch.cuda.is_available()); print(torch.cuda.get_device_name(0))"
# Expected: True, NVIDIA Orin
```

### 下载预训练检查点

```bash
# From MIT HAN Lab model zoo
mkdir -p checkpoints

# nuScenes camera+LiDAR fusion checkpoint
wget -O checkpoints/bevfusion-det.pth \
    https://hanlab.mit.edu/projects/bevfusion/files/bevfusion-det.pth

# nuScenes LiDAR-only (for comparison)
wget -O checkpoints/lidaronly.pth \
    https://hanlab.mit.edu/projects/bevfusion/files/lidaronly.pth
```

### nuScenes 数据集结构

```
data/nuscenes/
├── maps/
├── samples/          ← camera images
│   ├── CAM_FRONT/
│   ├── CAM_FRONT_LEFT/
│   ├── CAM_FRONT_RIGHT/
│   ├── CAM_BACK/
│   ├── CAM_BACK_LEFT/
│   └── CAM_BACK_RIGHT/
├── sweeps/           ← LiDAR point clouds
│   └── LIDAR_TOP/
└── v1.0-trainval/    ← annotation JSON files

# Create symlinks expected by BEVFusion
cd bevfusion && mkdir -p data
ln -s /path/to/nuscenes data/nuscenes
```

---

## 10. 训练 BEVFusion


<details>
<summary>English original</summary>

**Coordinate Systems**

nuScenes uses three coordinate frames — understanding them is critical for BEVFusion:

```
Global (world) frame:
  Origin: fixed point in the map (city-level)
  Used for: ego_pose, global object tracks, HD maps

Ego (vehicle) frame:
  Origin: rear axle midpoint
  Used for: sample_annotation translations are in THIS frame
  +X: forward, +Y: left, +Z: up

Sensor frame:
  Origin: sensor mounting position
  Used for: raw point cloud data is in THIS frame
  calibrated_sensor gives T_sensor→ego transform

Conversion chain:
  LiDAR points (sensor frame)
    → apply calibrated_sensor.T → ego frame
    → apply ego_pose.T → global frame
    → (subtract target_ego_pose.T) → another ego frame
```

```python
from pyquaternion import Quaternion
import numpy as np

def lidar_to_ego(pts_lidar: np.ndarray, cs_record: dict) -> np.ndarray:
    """Transform LiDAR points from sensor frame to ego frame."""
    R = Quaternion(cs_record['rotation']).rotation_matrix   # 3×3
    t = np.array(cs_record['translation'])                  # (3,)
    return (R @ pts_lidar[:, :3].T).T + t

def ego_to_global(pts_ego: np.ndarray, ep_record: dict) -> np.ndarray:
    """Transform ego-frame points to global frame."""
    R = Quaternion(ep_record['rotation']).rotation_matrix
    t = np.array(ep_record['translation'])
    return (R @ pts_ego.T).T + t
```

**Evaluation Metrics (NDS)**

BEVFusion papers report **NDS (nuScenes Detection Score)** — a composite metric:

```
NDS = (1/5) × (5 × mAP + (1 - mATE) + (1 - mASE) + (1 - mAOE) + (1 - mAAE) + (1 - mAVE))

where:
  mAP  = mean Average Precision (matching threshold: BEV center distance, not IoU)
  mATE = mean Average Translation Error  [meters]   ← lower is better
  mASE = mean Average Scale Error        [1 - IoU]  ← lower is better
  mAOE = mean Average Orientation Error  [radians]  ← lower is better
  mAAE = mean Average Attribute Error    [1 - acc]  ← lower is better
  mAVE = mean Average Velocity Error     [m/s]      ← lower is better

Key difference from KITTI:
  nuScenes uses center-distance matching (2D BEV), not 3D IoU.
  Threshold: 0.5m, 1.0m, 2.0m, 4.0m → average over thresholds.
  This makes nearby objects count more (right for AV safety).
```

**BEVFusion Reference Performance on nuScenes**

| Method | Cameras | LiDAR | mAP | NDS |
|--------|---------|-------|-----|-----|
| PointPillars | — | ✓ | 40.1 | 53.0 |
| BEVDet | ✓ (6) | — | 29.8 | 38.8 |
| BEVFusion (MIT) | ✓ (6) | ✓ | **68.5** | **71.4** |
| BEVFusion (NVIDIA) | ✓ (6) | ✓ | 67.9 | 71.0 |

nuScenes val set. Fusion gives +28 mAP over LiDAR-only, +38 mAP over camera-only.

---

**9. Setup and Installation**

**Clone and Setup (MIT HAN Lab BEVFusion)**

```bash
# On workstation (training) or Jetson (inference)
git clone https://github.com/mit-han-lab/bevfusion.git
cd bevfusion

# Install dependencies
pip3 install torch torchvision --index-url https://download.pytorch.org/whl/cu121
pip3 install mmcv-full==1.7.0 -f https://download.openmmlab.com/mmcv/dist/cu121/torch2.1/index.html
pip3 install mmdet3d==1.1.1
pip3 install nuscenes-devkit

# Build custom CUDA ops
cd mmdet3d && pip3 install -v -e .

# Install spconv (sparse convolution for LiDAR encoder)
pip3 install spconv-cu121   # match your CUDA version

# Install cumm (needed by spconv)
pip3 install cumm-cu121
```

**Orin Nano — Inference-Only Setup**

```bash
# On Jetson, skip training deps, use Jetson-optimized wheels
sudo apt-get install -y python3-pip libopenblas-dev

# PyTorch for Jetson (from NVIDIA)
# Check: https://forums.developer.nvidia.com/t/pytorch-for-jetson/72048
# Example for JetPack 6 / CUDA 12.x:
pip3 install torch-*.whl torchvision-*.whl   # from NVIDIA's Jetson PyTorch page

# Minimal inference deps only
pip3 install nuscenes-devkit numpy opencv-python pyyaml

# Pre-built mmcv for Jetson (or build from source)
pip3 install mmcv==2.1.0

# Verify GPU is used
python3 -c "import torch; print(torch.cuda.is_available()); print(torch.cuda.get_device_name(0))"
# Expected: True, NVIDIA Orin
```

**Download Pretrained Checkpoint**

```bash
# From MIT HAN Lab model zoo
mkdir -p checkpoints

# nuScenes camera+LiDAR fusion checkpoint
wget -O checkpoints/bevfusion-det.pth \
    https://hanlab.mit.edu/projects/bevfusion/files/bevfusion-det.pth

# nuScenes LiDAR-only (for comparison)
wget -O checkpoints/lidaronly.pth \
    https://hanlab.mit.edu/projects/bevfusion/files/lidaronly.pth
```

**nuScenes Dataset Structure**

```
data/nuscenes/
├── maps/
├── samples/          ← camera images
│   ├── CAM_FRONT/
│   ├── CAM_FRONT_LEFT/
│   ├── CAM_FRONT_RIGHT/
│   ├── CAM_BACK/
│   ├── CAM_BACK_LEFT/
│   └── CAM_BACK_RIGHT/
├── sweeps/           ← LiDAR point clouds
│   └── LIDAR_TOP/
└── v1.0-trainval/    ← annotation JSON files

# Create symlinks expected by BEVFusion
cd bevfusion && mkdir -p data
ln -s /path/to/nuscenes data/nuscenes
```

---

**10. Training BEVFusion**

</details>

### 准备 nuScenes 数据

```bash
# Generate info files (annotations in mmdet3d format)
python3 tools/create_data.py nuscenes \
    --root-path ./data/nuscenes \
    --out-dir ./data/nuscenes \
    --extra-tag nuscenes

# This creates:
# data/nuscenes/nuscenes_infos_train.pkl
# data/nuscenes/nuscenes_infos_val.pkl
```

### 训练配置

```python
# configs/bevfusion/bevfusion-det.yaml (excerpt)
model:
  type: BEVFusion
  encoders:
    camera:
      backbone:
        type: SwinTransformer
        embed_dims: 96
        depths: [2, 2, 6, 2]
        pretrained: checkpoints/swint-nuimages-pretrained.pth
      neck:
        type: FPN
        in_channels: [96, 192, 384, 768]
        out_channels: 256
      vtransform:
        type: DepthLSS
        in_channels: 256
        out_channels: 80
        image_size: [256, 704]          # input image size
        feature_size: [32, 88]          # after backbone downsample
        xbound: [-51.2, 51.2, 0.4]     # BEV x range and resolution
        ybound: [-51.2, 51.2, 0.4]     # BEV y range and resolution
        zbound: [-10.0, 10.0, 20.0]
        dbound: [1.0, 60.0, 0.5]       # depth range: 1m to 60m, 118 bins

    lidar:
      voxelize:
        max_num_points: 10
        point_cloud_range: [-54.0, -54.0, -5.0, 54.0, 54.0, 3.0]
        voxel_size: [0.075, 0.075, 0.2]
      backbone:
        type: SparseEncoder
        in_channels: 5
        sparse_shape: [1440, 1440, 41]

  fuser:
    type: ConvFuser
    in_channels: [80, 256]
    out_channels: 256

  heads:
    object:
      type: TransFusionHead
      num_classes: 10
      hidden_channel: 128
      num_proposals: 200

optimizer:
  type: AdamW
  lr: 2e-4
  weight_decay: 0.01

scheduler:
  type: OneCycleLR
  max_lr: 2e-4
  total_steps: 6019   # 6 epochs × steps_per_epoch
```

### 训练命令

```bash
# Single GPU training (minimum: 24GB VRAM — use workstation or cloud)
python3 tools/train.py configs/bevfusion/bevfusion-det.yaml

# Multi-GPU (recommended: 8× A100 or 4× 3090)
torchrun --nproc_per_node=8 tools/train.py \
    configs/bevfusion/bevfusion-det.yaml \
    --launcher pytorch

# Mixed precision (saves ~40% VRAM)
python3 tools/train.py configs/bevfusion/bevfusion-det.yaml \
    --cfg-options train_cfg.fp16=True

# Monitor training
tensorboard --logdir work_dirs/bevfusion-det/
```

### 评估

```bash
python3 tools/test.py \
    configs/bevfusion/bevfusion-det.yaml \
    checkpoints/bevfusion-det.pth \
    --eval bbox

# Expected output:
# mAP: 68.5
# NDS: 71.4
# Per-class AP:
#   car: 84.3   truck: 56.2   bus: 65.1
#   pedestrian: 82.4   bicycle: 45.3   motorcycle: 56.8
```

---

## 11. 导出到 ONNX 和 TensorRT

### 挑战

BEVFusion 有一些组件难以直接导出：
- 动态尺寸点云（每次扫描的 N_points 可变）
- 稀疏卷积（ONNX 原生不支持）
- Scatter 操作（LSS voxel pooling）

### 策略：将模型拆分为可导出的子图

```
Full BEVFusion
├── Camera Branch
│   ├── Image Backbone + FPN        ← export to ONNX (static input)
│   └── View Transform (LSS)        ← export separately (tricky ops)
├── LiDAR Branch
│   ├── Pillarization               ← custom CUDA kernel (keep as-is)
│   └── BEV Backbone                ← export to ONNX
├── Fusion Layer                    ← export to ONNX
└── Detection Head                  ← export to ONNX
```

### 将相机 Backbone 导出到 ONNX

```python
# export_camera_backbone.py
import torch
from bevfusion.model import BEVFusion

model = BEVFusion.from_pretrained('configs/bevfusion-det.yaml',
                                   'checkpoints/bevfusion-det.pth')
model.eval().cuda()

# Export only the image encoder (backbone + neck, no view transform)
class CameraEncoderOnly(torch.nn.Module):
    def __init__(self, full_model):
        super().__init__()
        self.backbone = full_model.encoders.camera.backbone
        self.neck     = full_model.encoders.camera.neck

    def forward(self, images):
        # images: [N_cams, 3, H, W]
        features = self.backbone(images)
        return self.neck(features)

cam_encoder = CameraEncoderOnly(model).cuda()

# Static input: 6 cameras, 256×704 images
dummy_images = torch.randn(6, 3, 256, 704, device='cuda')

torch.onnx.export(
    cam_encoder,
    dummy_images,
    'camera_backbone.onnx',
    opset_version=17,
    input_names=['images'],
    output_names=['features_0', 'features_1', 'features_2'],
    dynamic_axes={'images': {0: 'num_cameras'}}
)
print("Camera backbone exported")
```

### 导出融合 + 检测头

```python
# export_fusion_head.py
class FusionHead(torch.nn.Module):
    def __init__(self, full_model):
        super().__init__()
        self.fuser = full_model.fuser
        self.head  = full_model.heads['object']

    def forward(self, cam_bev, lidar_bev):
        fused = self.fuser(cam_bev, lidar_bev)
        return self.head(fused)

fusion_head = FusionHead(model).cuda()

# BEV grids: [B, C, X, Y]
cam_bev   = torch.randn(1, 80,  128, 128, device='cuda')
lidar_bev = torch.randn(1, 256, 128, 128, device='cuda')

torch.onnx.export(
    fusion_head,
    (cam_bev, lidar_bev),
    'fusion_head.onnx',
    opset_version=17,
    input_names=['cam_bev', 'lidar_bev'],
    output_names=['heatmap', 'offset', 'height', 'size', 'rot'],
)
print("Fusion+Head exported")
```

### 在 Jetson 上转换为 TensorRT

```bash
# Copy ONNX files to Jetson, then:

# Camera backbone — FP16
trtexec --onnx=camera_backbone.onnx \
        --saveEngine=camera_backbone_fp16.engine \
        --fp16 \
        --verbose 2>&1 | tail -20

# Fusion + detection head — FP16
trtexec --onnx=fusion_head.onnx \
        --saveEngine=fusion_head_fp16.engine \
        --fp16 \
        --verbose 2>&1 | tail -20

# INT8 with calibration (if accuracy target allows)
trtexec --onnx=fusion_head.onnx \
        --saveEngine=fusion_head_int8.engine \
        --int8 --fp16 \
        --calib=fusion_calib.cache
```

### NVIDIA 优化的 BEVFusion TensorRT（推荐用于 Jetson）

NVIDIA 提供了 TensorRT 原生的 BEVFusion 实现，并带有预导出的 engine：

```bash
# Clone NVIDIA's inference-optimized version
git clone https://github.com/NVIDIA-AI-IOT/Lidar_AI_Solution.git
cd Lidar_AI_Solution/CUDA-BEVFusion

# Build (on Jetson, requires JetPack 6 + TensorRT 10)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Download pretrained TRT engines (Jetson Orin optimized)
bash download_model.sh

# Run demo
./bevfusion --camera=demo/camera/ \
            --lidar=demo/lidar/ \
            --model=models/ \
            --output=output/

# This runs ~15 FPS on Orin Nano 8GB with FP16
```

---

## 12. 在 Jetson Orin Nano 上运行


<details>
<summary>English original</summary>

**Export Fusion + Detection Head**

```python
# export_fusion_head.py
class FusionHead(torch.nn.Module):
    def __init__(self, full_model):
        super().__init__()
        self.fuser = full_model.fuser
        self.head  = full_model.heads['object']

    def forward(self, cam_bev, lidar_bev):
        fused = self.fuser(cam_bev, lidar_bev)
        return self.head(fused)

fusion_head = FusionHead(model).cuda()

# BEV grids: [B, C, X, Y]
cam_bev   = torch.randn(1, 80,  128, 128, device='cuda')
lidar_bev = torch.randn(1, 256, 128, 128, device='cuda')

torch.onnx.export(
    fusion_head,
    (cam_bev, lidar_bev),
    'fusion_head.onnx',
    opset_version=17,
    input_names=['cam_bev', 'lidar_bev'],
    output_names=['heatmap', 'offset', 'height', 'size', 'rot'],
)
print("Fusion+Head exported")
```

**Convert to TensorRT on Jetson**

```bash
# Copy ONNX files to Jetson, then:

# Camera backbone — FP16
trtexec --onnx=camera_backbone.onnx \
        --saveEngine=camera_backbone_fp16.engine \
        --fp16 \
        --verbose 2>&1 | tail -20

# Fusion + detection head — FP16
trtexec --onnx=fusion_head.onnx \
        --saveEngine=fusion_head_fp16.engine \
        --fp16 \
        --verbose 2>&1 | tail -20

# INT8 with calibration (if accuracy target allows)
trtexec --onnx=fusion_head.onnx \
        --saveEngine=fusion_head_int8.engine \
        --int8 --fp16 \
        --calib=fusion_calib.cache
```

**NVIDIA-Optimized BEVFusion TensorRT (Recommended for Jetson)**

NVIDIA provides a TensorRT-native BEVFusion implementation with pre-exported engines:

```bash
# Clone NVIDIA's inference-optimized version
git clone https://github.com/NVIDIA-AI-IOT/Lidar_AI_Solution.git
cd Lidar_AI_Solution/CUDA-BEVFusion

# Build (on Jetson, requires JetPack 6 + TensorRT 10)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Download pretrained TRT engines (Jetson Orin optimized)
bash download_model.sh

# Run demo
./bevfusion --camera=demo/camera/ \
            --lidar=demo/lidar/ \
            --model=models/ \
            --output=output/

# This runs ~15 FPS on Orin Nano 8GB with FP16
```

---

**12. Running on Jetson Orin Nano**

</details>

### 推理流水线（Python）

```python
# bevfusion_jetson.py
import numpy as np
import tensorrt as trt
import pycuda.driver as cuda
import pycuda.autoinit
import cv2
import time

class BEVFusionInference:
    def __init__(self, cam_engine_path, fusion_engine_path):
        self.cam_inferencer    = self._load_engine(cam_engine_path)
        self.fusion_inferencer = self._load_engine(fusion_engine_path)

        # BEV configuration
        self.bev_range   = [-51.2, -51.2, -5.0, 51.2, 51.2, 3.0]
        self.voxel_size  = [0.075, 0.075, 0.2]
        self.score_thresh = 0.3

    def _load_engine(self, path):
        with open(path, 'rb') as f:
            runtime = trt.Runtime(trt.Logger(trt.Logger.WARNING))
            engine  = runtime.deserialize_cuda_engine(f.read())
        return engine.create_execution_context()

    def preprocess_cameras(self, images):
        """
        images: list of 6 BGR numpy arrays (one per camera)
        returns: [6, 3, 256, 704] float32 normalized tensor
        """
        processed = []
        mean = np.array([103.530, 116.280, 123.675], dtype=np.float32)
        std  = np.array([57.375,  57.120,  58.395],  dtype=np.float32)

        for img in images:
            img = cv2.resize(img, (704, 256))
            img = img.astype(np.float32)
            img = (img - mean) / std
            img = img.transpose(2, 0, 1)   # HWC → CHW
            processed.append(img)

        return np.stack(processed, axis=0)   # [6, 3, 256, 704]

    def preprocess_lidar(self, points):
        """
        points: numpy [N, 4] (x, y, z, intensity)
        Returns pillars in expected format.
        """
        # Filter to range
        mask = ((points[:, 0] > self.bev_range[0]) &
                (points[:, 0] < self.bev_range[3]) &
                (points[:, 1] > self.bev_range[1]) &
                (points[:, 1] < self.bev_range[4]) &
                (points[:, 2] > self.bev_range[2]) &
                (points[:, 2] < self.bev_range[5]))
        return points[mask]

    def run(self, camera_images, lidar_points):
        t0 = time.perf_counter()

        # Camera preprocessing
        cam_input = self.preprocess_cameras(camera_images)     # [6, 3, 256, 704]

        # Camera backbone inference
        cam_features = self._run_engine(self.cam_inferencer, cam_input)

        t_cam = (time.perf_counter() - t0) * 1000

        # LiDAR preprocessing (voxelization — CUDA kernel or numpy)
        lidar_input = self.preprocess_lidar(lidar_points)

        # LiDAR encoding (pillars → BEV) — handled by custom CUDA kernel
        lidar_bev = self._run_lidar_encoder(lidar_input)

        # Camera view transform (LSS) → camera BEV
        cam_bev = self._run_lss(cam_features)

        # Fusion + detection head
        detections = self._run_engine(self.fusion_inferencer,
                                       [cam_bev, lidar_bev])

        t_total = (time.perf_counter() - t0) * 1000
        print(f"Camera: {t_cam:.1f}ms | Total: {t_total:.1f}ms")

        return self._decode(detections)

    def _decode(self, raw_outputs):
        heatmap = raw_outputs['heatmap']    # [1, 10, X, Y]
        # ... decode centers + boxes (see Section 7) ...
        pass


# Usage
bevfusion = BEVFusionInference(
    'camera_backbone_fp16.engine',
    'fusion_head_fp16.engine'
)

# Load a nuScenes sample for testing
images     = [cv2.imread(f'demo/camera/CAM_FRONT/{i:05d}.jpg') for i in range(6)]
lidar_pts  = np.fromfile('demo/lidar/sample.bin', dtype=np.float32).reshape(-1, 5)

detections = bevfusion.run(images, lidar_pts)
```

### Orin Nano 8GB 上的预期性能

```
Component               FP32     FP16     Notes
─────────────────────────────────────────────────────────────
Camera backbone         340ms    120ms    Swin-T is large
Camera LSS transform     80ms     35ms    scatter op
LiDAR pillarization      15ms     15ms    CUDA kernel, no FP effect
LiDAR sparse encoder     90ms     40ms    spconv ops
Fusion + head            25ms     10ms    2D conv, fastest part
Post-processing          10ms     10ms    NMS, decode
─────────────────────────────────────────────────────────────
Total                   560ms    230ms    → ~4 FPS FP16

With INT8 camera + smaller backbone (ResNet-18 instead of Swin-T):
Total                    —        80ms    → ~12 FPS
```

**实践提示：** 完整的 BEVFusion 计算量很大。在 Orin Nano 8GB 上，有两种现实可行的方案：
1. **缩减相机 backbone** 至 ResNet-18 / MobileNetV2 → 3–5 FPS，准确率略有下降
2. **仅相机或仅激光雷达** 以保证实时，用 BEVFusion 离线建图

---

## 13. ROS2 集成

### BEVFusion ROS2 节点

```python
#!/usr/bin/env python3
# bevfusion_ros2_node.py

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image, PointCloud2
from vision_msgs.msg import Detection3DArray, Detection3D, BoundingBox3D
from geometry_msgs.msg import Pose, Vector3
from std_msgs.msg import Header
import message_filters
from cv_bridge import CvBridge
import sensor_msgs_py.point_cloud2 as pc2
import numpy as np
import time

from bevfusion_jetson import BEVFusionInference

class BEVFusionNode(Node):
    def __init__(self):
        super().__init__('bevfusion_node')

        # Parameters
        self.declare_parameter('cam_engine', 'camera_backbone_fp16.engine')
        self.declare_parameter('fusion_engine', 'fusion_head_fp16.engine')
        self.declare_parameter('score_threshold', 0.35)
        self.declare_parameter('max_age', 0.2)    # seconds — drop detections older than this

        cam_engine    = self.get_parameter('cam_engine').value
        fusion_engine = self.get_parameter('fusion_engine').value
        self.score_thresh = self.get_parameter('score_threshold').value

        # Load model
        self.model = BEVFusionInference(cam_engine, fusion_engine)
        self.bridge = CvBridge()

        # Camera subscribers (6 cameras for nuScenes setup)
        # Adjust topic names to your hardware
        cam_subs = [
            message_filters.Subscriber(self, Image, f'/camera/{name}/image_raw')
            for name in ['front', 'front_left', 'front_right',
                         'back', 'back_left', 'back_right']
        ]
        lidar_sub = message_filters.Subscriber(self, PointCloud2, '/lidar/points')

        # Time-sync all cameras + LiDAR within 50ms
        self.sync = message_filters.ApproximateTimeSynchronizer(
            cam_subs + [lidar_sub],
            queue_size=5,
            slop=0.05
        )
        self.sync.registerCallback(self.fused_callback)

        # Publish 3D detections
        self.det_pub = self.create_publisher(Detection3DArray, '/bevfusion/detections', 10)

        # Class names (nuScenes)
        self.class_names = [
            'car', 'truck', 'construction_vehicle', 'bus', 'trailer',
            'barrier', 'motorcycle', 'bicycle', 'pedestrian', 'traffic_cone'
        ]

        self.get_logger().info('BEVFusion node ready')

    def fused_callback(self, *msgs):
        """Called when all 6 cameras + LiDAR are synchronized"""
        cam_msgs  = msgs[:6]
        lidar_msg = msgs[6]

        # Convert camera messages to numpy
        images = [
            self.bridge.imgmsg_to_cv2(m, desired_encoding='bgr8')
            for m in cam_msgs
        ]

        # Convert LiDAR PointCloud2 to numpy
        pts = np.array(list(pc2.read_points(
            lidar_msg, field_names=('x', 'y', 'z', 'intensity'),
            skip_nans=True
        )), dtype=np.float32)

        if len(pts) < 100:
            self.get_logger().warn('Too few LiDAR points, skipping')
            return

        t0 = time.perf_counter()

        # Run BEVFusion
        detections = self.model.run(images, pts)

        dt = (time.perf_counter() - t0) * 1000
        self.get_logger().info(f'Inference: {dt:.1f}ms | {len(detections)} detections')

        # Publish
        det_array = Detection3DArray()
        det_array.header = lidar_msg.header   # use LiDAR timestamp

        for det in detections:
            if det['score'] < self.score_thresh:
                continue
            d = Detection3D()
            d.header = det_array.header

            # Bounding box in 3D
            d.bbox.center.position.x = det['x']
            d.bbox.center.position.y = det['y']
            d.bbox.center.position.z = det['z']

            # Convert heading to quaternion (rotation around Z)
            import math
            yaw = det['heading']
            d.bbox.center.orientation.z = math.sin(yaw / 2)
            d.bbox.center.orientation.w = math.cos(yaw / 2)

            d.bbox.size.x = det['w']
            d.bbox.size.y = det['l']
            d.bbox.size.z = det['h']

            det_array.detections.append(d)

        self.det_pub.publish(det_array)

def main():
    rclpy.init()
    node = BEVFusionNode()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()
```

### 在 RViz2 中可视化

```bash
# Launch RViz2 with BEVFusion visualization
ros2 launch bevfusion_ros bevfusion_rviz.launch.py

# In another terminal: run the node
ros2 run bevfusion_ros bevfusion_node \
    --ros-args \
    -p cam_engine:=camera_backbone_fp16.engine \
    -p fusion_engine:=fusion_head_fp16.engine \
    -p score_threshold:=0.35

# Subscribe to 3D detections
ros2 topic echo /bevfusion/detections

# Check latency
ros2 topic hz /bevfusion/detections
# Expected on Orin Nano FP16: ~4-12 Hz depending on backbone size
```

---

## 14. 面向 Jetson 的优化

### 策略 1：更轻量的相机骨干网络

把 Swin-Transformer 换成 MobileNetV2——同样的架构，算力需求低得多：

```python
# In bevfusion config, change:
encoders:
  camera:
    backbone:
      type: MobileNetV2          # was SwinTransformer
      out_indices: [1, 2, 4, 6]
      pretrained: checkpoints/mobilenet_v2.pth

# Result on Orin Nano:
# Swin-T camera backbone: 120ms FP16
# MobileNetV2 backbone:    25ms FP16  ← 5× speedup, ~2-3 mAP drop
```

### 策略 2：降低 BEV 分辨率

BEV 分辨率减半（1024→512），融合计算量减少 4 倍：

```yaml
# In config: change xbound/ybound resolution
vtransform:
  xbound: [-51.2, 51.2, 0.8]    # was 0.4 → 2× coarser, 4× fewer cells
  ybound: [-51.2, 51.2, 0.8]

# Result: 4× faster fusion, ~1-2 mAP drop
# Acceptable for robotics (0.8m resolution is fine for navigation)
```

### 策略 3：减少相机数量

在机器人上，可能只有 2–3 个相机，而不是 6 个：

```python
# Single front camera + LiDAR is already useful for forward detection
cameras = ['CAM_FRONT']   # instead of 6 cameras

# Memory: 6 cameras × 256×704 × 3 = 3.2 MB input
#          1 camera  × 256×704 × 3 = 0.5 MB input
# Camera backbone: 6× faster

# Trade-off: lost side and rear coverage
# Solution for robots: front + back only (2 cameras)
```

### 策略 4：相机与 LiDAR（激光雷达）异步

LiDAR 以 10 Hz 扫描，相机以 30 Hz 采集。不要让 LiDAR 处理阻塞相机帧：

```python
class AsyncBEVFusion:
    def __init__(self):
        self.latest_lidar_bev = None
        self.lidar_lock = threading.Lock()
        self.lidar_thread = threading.Thread(target=self._lidar_loop, daemon=True)
        self.lidar_thread.start()

    def _lidar_loop(self):
        """Runs at 10 Hz — update cached LiDAR BEV features"""
        while True:
            pts = get_latest_lidar_scan()
            lidar_bev = run_lidar_encoder(pts)
            with self.lidar_lock:
                self.latest_lidar_bev = lidar_bev

    def run_camera_frame(self, images):
        """Runs at 30 Hz — use cached LiDAR BEV"""
        cam_bev = run_camera_encoder(images)    # fresh every frame

        with self.lidar_lock:
            lidar_bev = self.latest_lidar_bev   # stale by up to 100ms but fast

        return run_fusion_head(cam_bev, lidar_bev)

# Result: effectively 30 FPS camera-driven inference
#         LiDAR updates every 3 camera frames
#         Works well at low speed (<30 km/h where 100ms LiDAR lag is fine)
```

### 策略 5：融合头使用 TensorRT INT8

融合头（ConvNet + CenterPoint）最适合量化：

```bash
# Calibrate on 500 real samples
trtexec --onnx=fusion_head.onnx \
        --saveEngine=fusion_head_int8.engine \
        --int8 --fp16 \
        --calib=fusion_calib.cache \
        --verbose

# Benchmark
trtexec --loadEngine=fusion_head_int8.engine \
        --warmUp=200 --iterations=500 --avgRuns=100

# Typical result:
# FP16 fusion head: 10ms
# INT8 fusion head: 6ms  (keep camera backbone in FP16)
```

### 策略 6：最佳吞吐对应的功耗模式

```bash
# For sustained inference, MAXN mode + jetson_clocks
sudo nvpmodel -m 0              # MAXN: 15W TDP
sudo jetson_clocks              # lock all clocks to max

# Monitor: GPU should stay at ~90%+ utilization
sudo tegrastats --interval 500

# For battery systems (5-10W budget):
sudo nvpmodel -m 1              # 7W mode
# Skip jetson_clocks
# Accept lower FPS in exchange for 2× lower power
```

### 性能汇总表

| 配置                          | FPS（Orin Nano） | mAP  |
|----------------------------------------|-----------------|------|
| 完整 BEVFusion FP32                    | ~1              | 68.5 |
| 完整 BEVFusion FP16                    | ~4              | 68.3 |
| MobileNetV2 + FP16                     | ~10             | 65.0 |
| MobileNetV2 + 更粗的 BEV + FP16      | ~18             | 63.5 |
| MobileNetV2 + INT8 融合 + 异步 LiDAR | ~25           | 62.5 |
| 仅 LiDAR（PointPillars INT8）         | ~35             | 40.1 |

---

## 15. 项目

### 项目 1：nuScenes 上的 BEVFusion 推理
在 nuScenes 验证场景上运行预训练的 BEVFusion 检查点。用 matplotlib 在 BEV 空间可视化检测结果。对比仅相机、仅 LiDAR 与融合三种方式的 mAP。

### 项目 2：面向 Jetson 的轻量级 BEVFusion
把 Swin-Transformer 替换为 MobileNetV2。在 nuScenes mini 上重新训练 6 个 epoch。导出为 TensorRT FP16。目标：Orin Nano 上 >10 FPS，mAP 下降 <5。

### 项目 3：自定义数据集——机器人走廊导航
在机器人平台上用 2 个相机（前 + 后）+ 一个低成本 LiDAR（RPLIDAR S2）采集数据。用 label-studio、CVAT 或 Roboflow 标注。训练 BEVFusion，用于室内走廊中的行人与障碍物检测。


<details>
<summary>English original</summary>

**Strategy 1: Lighter Camera Backbone**

Swap Swin-Transformer for MobileNetV2 — same architecture, much less compute:

```python
# In bevfusion config, change:
encoders:
  camera:
    backbone:
      type: MobileNetV2          # was SwinTransformer
      out_indices: [1, 2, 4, 6]
      pretrained: checkpoints/mobilenet_v2.pth

# Result on Orin Nano:
# Swin-T camera backbone: 120ms FP16
# MobileNetV2 backbone:    25ms FP16  ← 5× speedup, ~2-3 mAP drop
```

**Strategy 2: Reduce BEV Resolution**

Halving the BEV resolution (1024→512) reduces fusion compute by 4×:

```yaml
# In config: change xbound/ybound resolution
vtransform:
  xbound: [-51.2, 51.2, 0.8]    # was 0.4 → 2× coarser, 4× fewer cells
  ybound: [-51.2, 51.2, 0.8]

# Result: 4× faster fusion, ~1-2 mAP drop
# Acceptable for robotics (0.8m resolution is fine for navigation)
```

**Strategy 3: Reduce Number of Cameras**

On a robot, you may only have 2–3 cameras, not 6:

```python
# Single front camera + LiDAR is already useful for forward detection
cameras = ['CAM_FRONT']   # instead of 6 cameras

# Memory: 6 cameras × 256×704 × 3 = 3.2 MB input
#          1 camera  × 256×704 × 3 = 0.5 MB input
# Camera backbone: 6× faster

# Trade-off: lost side and rear coverage
# Solution for robots: front + back only (2 cameras)
```

**Strategy 4: Asynchronous Camera + LiDAR**

LiDAR scans at 10 Hz, cameras at 30 Hz. Don't wait for LiDAR to process a camera frame:

```python
class AsyncBEVFusion:
    def __init__(self):
        self.latest_lidar_bev = None
        self.lidar_lock = threading.Lock()
        self.lidar_thread = threading.Thread(target=self._lidar_loop, daemon=True)
        self.lidar_thread.start()

    def _lidar_loop(self):
        """Runs at 10 Hz — update cached LiDAR BEV features"""
        while True:
            pts = get_latest_lidar_scan()
            lidar_bev = run_lidar_encoder(pts)
            with self.lidar_lock:
                self.latest_lidar_bev = lidar_bev

    def run_camera_frame(self, images):
        """Runs at 30 Hz — use cached LiDAR BEV"""
        cam_bev = run_camera_encoder(images)    # fresh every frame

        with self.lidar_lock:
            lidar_bev = self.latest_lidar_bev   # stale by up to 100ms but fast

        return run_fusion_head(cam_bev, lidar_bev)

# Result: effectively 30 FPS camera-driven inference
#         LiDAR updates every 3 camera frames
#         Works well at low speed (<30 km/h where 100ms LiDAR lag is fine)
```

**Strategy 5: TensorRT INT8 for Fusion Head**

The fusion head (ConvNet + CenterPoint) is the most quantization-friendly:

```bash
# Calibrate on 500 real samples
trtexec --onnx=fusion_head.onnx \
        --saveEngine=fusion_head_int8.engine \
        --int8 --fp16 \
        --calib=fusion_calib.cache \
        --verbose

# Benchmark
trtexec --loadEngine=fusion_head_int8.engine \
        --warmUp=200 --iterations=500 --avgRuns=100

# Typical result:
# FP16 fusion head: 10ms
# INT8 fusion head: 6ms  (keep camera backbone in FP16)
```

**Strategy 6: Power Mode for Best Throughput**

```bash
# For sustained inference, MAXN mode + jetson_clocks
sudo nvpmodel -m 0              # MAXN: 15W TDP
sudo jetson_clocks              # lock all clocks to max

# Monitor: GPU should stay at ~90%+ utilization
sudo tegrastats --interval 500

# For battery systems (5-10W budget):
sudo nvpmodel -m 1              # 7W mode
# Skip jetson_clocks
# Accept lower FPS in exchange for 2× lower power
```

**Performance Summary Table**

| Configuration                          | FPS (Orin Nano) | mAP  |
|----------------------------------------|-----------------|------|
| Full BEVFusion FP32                    | ~1              | 68.5 |
| Full BEVFusion FP16                    | ~4              | 68.3 |
| MobileNetV2 + FP16                     | ~10             | 65.0 |
| MobileNetV2 + coarser BEV + FP16      | ~18             | 63.5 |
| MobileNetV2 + INT8 fusion + async LiDAR | ~25           | 62.5 |
| LiDAR only (PointPillars INT8)         | ~35             | 40.1 |

---

**15. Projects**

**Project 1: BEVFusion Inference on nuScenes**
Run the pretrained BEVFusion checkpoint on a nuScenes validation scene. Visualize detections in BEV space with matplotlib. Compare camera-only vs LiDAR-only vs fusion mAP.

**Project 2: Lightweight BEVFusion for Jetson**
Replace Swin-Transformer with MobileNetV2. Retrain for 6 epochs on nuScenes mini. Export to TensorRT FP16. Target: >10 FPS on Orin Nano with <5 mAP drop.

**Project 3: Custom Dataset — Robot Corridor Navigation**
Collect data with 2 cameras (front + rear) + a low-cost LiDAR (RPLIDAR S2) on a robot platform. Label with label-studio, CVAT, or Roboflow. Train BEVFusion for pedestrian and obstacle detection in indoor corridors.

</details>

### 项目 4：BEVFusion ROS2 集成
完成第 12 节的 ROS2 节点。添加：
- RViz2 可视化，含 3D 边界框
- 按组件记录延迟（camera、LiDAR、融合、head）
- 健康监控（若 FPS < 阈值则发出丢帧告警）

### 项目 5：BEV Occupancy Grid vs BEVFusion
针对同一机器人任务对比两种方案：
- 传统方案：EKF + 2D LiDAR occupancy grid（快、简单）
- 现代方案：BEVFusion 3D 检测（较慢、输出更丰富）
测量：延迟、功耗、检测距离、遮挡处理。

### 项目 6：消融实验
在 nuScenes mini 上训练四个变体：
1. 仅 camera（BEVDet）
2. 仅 LiDAR（PointPillars）
3. 晚期融合（模型各自独立，合并框）
4. BEVFusion（特征级）
绘制每个变体的准确率 vs 算力 vs 内存曲线。针对每种方法的适用场景写出具体建议。

---

## 16. 资源

### 论文
- **"BEVFusion: Multi-Task Multi-Sensor Fusion with Unified Bird's-Eye View Representation"**（MIT HAN Lab，ICRA 2023）：主论文。仔细阅读第 3 节和第 4 节 —— 架构、LSS 与融合都有讲解。
- **"BEVFusion: A Simple and Robust LiDAR-Camera Fusion Framework"**（NVIDIA，NeurIPS 2022）：NVIDIA 版本，带 TensorRT 优化。
- **"Lift, Splat, Shoot: Encoding Images from Arbitrary Camera Rigs by Implicitly Unprojecting to 3D"**（Philion & Fidler，ECCV 2020）：BEVFusion 所用 camera → BEV 变换的基础。先读这篇，再读 BEVFusion 论文。
- **"PointPillars: Fast Encoders for Object Detection from Point Clouds"**（Lang 等，CVPR 2019）：LiDAR 分支的 backbone。
- **"Center-based 3D Object Detection and Tracking"**（CenterPoint，CVPR 2021）：BEVFusion 使用的检测头。

### 代码
- **MIT HAN Lab BEVFusion**：github.com/mit-han-lab/bevfusion
- **NVIDIA CUDA-BEVFusion**（Jetson 优化，原生 TensorRT）：github.com/NVIDIA-AI-IOT/Lidar_AI_Solution/tree/master/CUDA-BEVFusion
- **nuScenes devkit**：github.com/nutonomy/nuscenes-devkit —— 数据加载、评估、可视化

### 数据集
- **nuScenes**：nuscenes.org —— 1000 个场景、6 个 camera、1 个 LiDAR、完整 3D 标注。BEVFusion 的标准 benchmark。
- **nuScenes mini**（免费，10 个场景）：先从这里开始，再下载完整数据集。
- **KITTI**：用于较简单的 camera+LiDAR 任务（仅前向，2D+3D 框）。

### 可视化
```bash
# nuScenes-devkit viewer: render a scene with camera + LiDAR + GT boxes
python3 -c "
from nuscenes.nuscenes import NuScenes
from nuscenes.utils.data_classes import LidarPointCloud, Box
nusc = NuScenes(version='v1.0-mini', dataroot='data/nuscenes', verbose=True)
nusc.render_sample('scene-0001')
"
```

---

*上级：[传感器融合指南](../Guide.md)*
*另见：[4. Sensor Fusion / kalman-filter](../03-kalman-filter/README.md)*


<details>
<summary>English original</summary>

**Project 4: BEVFusion ROS2 Integration**
Complete the ROS2 node from Section 12. Add:
- RViz2 visualization with 3D bounding boxes
- Latency logging per component (camera, LiDAR, fusion, head)
- Health monitoring (drop alert if FPS < threshold)

**Project 5: BEV Occupancy Grid vs BEVFusion**
Compare two approaches for the same robotics task:
- Traditional: EKF + 2D LiDAR occupancy grid (fast, simple)
- Modern: BEVFusion 3D detection (slower, richer output)
Measure: latency, power, detection range, occlusion handling.

**Project 6: Ablation Study**
Train four variants on nuScenes mini:
1. Camera only (BEVDet)
2. LiDAR only (PointPillars)
3. Late fusion (separate models, merge boxes)
4. BEVFusion (feature-level)

Plot accuracy vs compute vs memory for each. Write a concrete recommendation for when each approach is appropriate.

---

**16. Resources**

**Papers**
- **"BEVFusion: Multi-Task Multi-Sensor Fusion with Unified Bird's-Eye View Representation"** (MIT HAN Lab, ICRA 2023): the primary paper. Read Sections 3 and 4 carefully — the architecture, LSS, and fusion are all explained.
- **"BEVFusion: A Simple and Robust LiDAR-Camera Fusion Framework"** (NVIDIA, NeurIPS 2022): the NVIDIA variant with TensorRT optimizations.
- **"Lift, Splat, Shoot: Encoding Images from Arbitrary Camera Rigs by Implicitly Unprojecting to 3D"** (Philion & Fidler, ECCV 2020): the foundation of the camera → BEV transform used in BEVFusion. Read this before the BEVFusion paper.
- **"PointPillars: Fast Encoders for Object Detection from Point Clouds"** (Lang et al., CVPR 2019): the LiDAR branch backbone.
- **"Center-based 3D Object Detection and Tracking"** (CenterPoint, CVPR 2021): the detection head used in BEVFusion.

**Code**
- **MIT HAN Lab BEVFusion**: github.com/mit-han-lab/bevfusion
- **NVIDIA CUDA-BEVFusion** (Jetson-optimized, TensorRT native): github.com/NVIDIA-AI-IOT/Lidar_AI_Solution/tree/master/CUDA-BEVFusion
- **nuScenes devkit**: github.com/nutonomy/nuscenes-devkit — data loading, evaluation, visualization

**Datasets**
- **nuScenes**: nuscenes.org — 1000 scenes, 6 cameras, 1 LiDAR, full 3D annotations. The standard benchmark for BEVFusion.
- **nuScenes mini** (free, 10 scenes): start here before downloading the full dataset.
- **KITTI**: for simpler camera+LiDAR tasks (frontal only, 2D+3D boxes).

**Visualization**
```bash
# nuScenes-devkit viewer: render a scene with camera + LiDAR + GT boxes
python3 -c "
from nuscenes.nuscenes import NuScenes
from nuscenes.utils.data_classes import LidarPointCloud, Box
nusc = NuScenes(version='v1.0-mini', dataroot='data/nuscenes', verbose=True)
nusc.render_sample('scene-0001')
"
```

---

*Up: [Sensor Fusion Guide](../Guide.md)*
*See also: [4. Sensor Fusion / kalman-filter](../03-kalman-filter/README.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/BEVFusion/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/BEVFusion/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
