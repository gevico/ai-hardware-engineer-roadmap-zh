---
title: GPUDirect Storage (GDS) — 深入剖析
description: GPUDirect Storage (GDS) — 深入剖析
---

# GPUDirect Storage (GDS) — 深入剖析

GPUDirect Storage (GDS) 将 CPU 从 GPU 到存储的数据通路中移除。数据不再走 GPU → PCIe → CPU RAM → PCIe → NVMe 这条路径，GDS 在 GPU HBM 与 NVMe/网络存储之间建立一条 **直接 DMA 通路** —— CPU 复合体被彻底绕过。

## GDS 为何存在

```
Without GDS (traditional path):
  NVMe → PCIe → CPU DRAM (bounce buffer) → PCIe → GPU HBM
  CPU must be awake and involved for every I/O
  CPU DRAM becomes the bottleneck (~50 GB/s DRAM bandwidth shared)

With GDS (direct path):
  NVMe → PCIe → GPU HBM  (direct DMA)
  No CPU bounce buffer
  No CPU involvement in the data path
  Limited only by PCIe bandwidth and NVMe throughput
```

## 参考配置（Western Digital Technical Brief）

本节基于 WD OpenFlex Data24 + NVIDIA GDS 验证环境，该环境代表了一个真实的生产 GDS 部署：

| 组件 | 规格 |
|---|---|
| CPU | Dual Intel Xeon Gold 6348, 26C @ 2.60 GHz |
| RAM | 512 GiB |
| GPU | 4× NVIDIA A100 80 GB PCIe |
| NIC | 6× ConnectX-7（CX-6 也测试过）|
| CUDA | 12.2.1 |
| GDS | 2.17.3 |
| libcufile | 1.7.1.12 |
| OS | RHEL 9: 5.14.0-70.70.1.el9_0 |
| OFED | Mellanox OFED 5.8-3.0.7.0 |
| Nvidia 驱动 | 535.86.10 |
| 以太网交换机 | NVIDIA SN3700，32 端口 200 Gb（Spectrum 2）|
| 存储 | WD OpenFlex Data24 3200 Series |
| 存储带宽 | 理论 75 GB/s（6 × 100 Gb/s 前端）|

## 主题索引

| # | 主题 | 描述 |
|---|---|---|
| 01 | [架构与数据通路](01-Architecture-and-Data-Path.md) | GDS 如何工作、PCIe 拓扑、NUMA 绑定 |
| 02 | [硬件搭建与配置](02-Hardware-Setup.md) | WD brief 中的参考配置、PCIe 布局布线、NIC 布局 |
| 03 | [软件栈与安装](03-Software-Stack.md) | GDS 安装、libcufile、验证、版本要求 |
| 04 | [libcufile 编程 API](04-libcufile-API.md) | cuFile API —— 读、写、注册缓冲区、异步 I/O |
| 05 | [性能调优](05-Performance-Tuning.md) | 对齐、缓冲区注册、多流、benchmark 测试 |
| 06 | [解耦式存储（OpenFlex + RapidFlex）](06-Disaggregated-Storage.md) | 基于 RDMA/RoCE 的 NVMe-oF、WD OpenFlex、线性横向扩展 |

## 快速导航

- **第一次搭建 GDS？** → [03-Software-Stack](03-Software-Stack.md)
- **要写支持 GDS 的代码？** → [04-libcufile-API](04-libcufile-API.md)
- **存储带宽太低？** → [05-Performance-Tuning](05-Performance-Tuning.md)
- **解耦式 NVMe-oF？** → [06-Disaggregated-Storage](06-Disaggregated-Storage.md)


<details>
<summary>English original</summary>

**GPUDirect Storage (GDS) — Deep Dive**

GPUDirect Storage (GDS) eliminates the CPU from the GPU-to-storage data path. Instead of data traveling GPU → PCIe → CPU RAM → PCIe → NVMe, GDS creates a **direct DMA path** between GPU HBM and NVMe/network storage — the CPU complex is bypassed entirely.

**Why GDS Exists**

```
Without GDS (traditional path):
  NVMe → PCIe → CPU DRAM (bounce buffer) → PCIe → GPU HBM
  CPU must be awake and involved for every I/O
  CPU DRAM becomes the bottleneck (~50 GB/s DRAM bandwidth shared)

With GDS (direct path):
  NVMe → PCIe → GPU HBM  (direct DMA)
  No CPU bounce buffer
  No CPU involvement in the data path
  Limited only by PCIe bandwidth and NVMe throughput
```

**Reference Configuration (Western Digital Technical Brief)**

This section is based on the WD OpenFlex Data24 + NVIDIA GDS validation setup, which represents a real production GDS deployment:

| Component | Specification |
|---|---|
| CPU | Dual Intel Xeon Gold 6348, 26C @ 2.60 GHz |
| RAM | 512 GiB |
| GPU | 4× NVIDIA A100 80 GB PCIe |
| NIC | 6× ConnectX-7 (CX-6 also tested) |
| CUDA | 12.2.1 |
| GDS | 2.17.3 |
| libcufile | 1.7.1.12 |
| OS | RHEL 9: 5.14.0-70.70.1.el9_0 |
| OFED | Mellanox OFED 5.8-3.0.7.0 |
| Nvidia Driver | 535.86.10 |
| Ethernet Switch | NVIDIA SN3700, 32-port 200 Gb (Spectrum 2) |
| Storage | WD OpenFlex Data24 3200 Series |
| Storage Bandwidth | 75 GB/s theoretical (6 × 100 Gb/s frontend) |

**Topic Index**

| # | Topic | Description |
|---|---|---|
| 01 | [Architecture & Data Path](01-Architecture-and-Data-Path.md) | How GDS works, PCIe topology, NUMA pinning |
| 02 | [Hardware Setup & Configuration](02-Hardware-Setup.md) | Reference config from WD brief, PCIe layout, NIC placement |
| 03 | [Software Stack & Installation](03-Software-Stack.md) | GDS install, libcufile, verification, version requirements |
| 04 | [libcufile Programming API](04-libcufile-API.md) | cuFile API — read, write, register buffers, async I/O |
| 05 | [Performance Tuning](05-Performance-Tuning.md) | Alignment, buffer registration, multi-stream, benchmarking |
| 06 | [Disaggregated Storage (OpenFlex + RapidFlex)](06-Disaggregated-Storage.md) | NVMe-oF over RDMA/RoCE, WD OpenFlex, linear scale-out |

**Quick Navigation**

- **First time setting up GDS?** → [03-Software-Stack](03-Software-Stack.md)
- **Writing GDS-enabled code?** → [04-libcufile-API](04-libcufile-API.md)
- **Storage bandwidth too low?** → [05-Performance-Tuning](05-Performance-Tuning.md)
- **Disaggregated NVMe-oF?** → [06-Disaggregated-Storage](06-Disaggregated-Storage.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/GPUDirect-Storage/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/GPUDirect-Storage/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
