---
title: 01 — H200 硬件架构
description: 01 — H200 硬件架构
---

# 01 — H200 硬件架构

## 1. 裸片与工艺

- **GPU 裸片：** GH100（Hopper 架构，TSMC 4N）
- **晶体管：** 800 亿
- **SM：** 132 个流式多处理器
- **CUDA 核心：** 16,896
- **Tensor Core：** 第 4 代（FP8、FP16、BF16、TF32、INT8、INT4）
- **FP8 峰值：** ~3,958 TFLOPS（稠密）
- **BF16 峰值：** ~1,979 TFLOPS（稠密）
- **TF32 峰值：** ~989 TFLOPS

H200 使用与 H100 相同的 GH100 裸片，但用 HBM3e 堆叠替换了 HBM3，以获得更高容量与带宽。

## 2. HBM3e 内存子系统

| 属性 | H100 SXM5 | H200 SXM5 |
|---|---|---|
| 内存类型 | HBM3 | HBM3e |
| 容量 | 80 GB | 141 GB |
| 带宽 | 3.35 TB/s | 4.8 TB/s |
| 堆叠数 | 5 | 6 |

### HBM3e 为何对 AI 重要

- 长上下文大语言模型推理可用更大的 KV cache（内存中可容纳 128K+ token）
- 更大的模型分片 → 更少的流水线级数 → 更低的流水线气泡开销
- 带宽提升 43% → 带宽受限算子（attention、embedding 查表）运行更快

### 内存访问最佳实践

```python
# Profile actual HBM bandwidth utilization
import torch
torch.backends.cuda.matmul.allow_tf32 = True
torch.backends.cudnn.allow_tf32 = True

# Always use BF16 for training (numerically stable, uses Tensor Cores)
model = model.to(torch.bfloat16)

# Pin CPU buffers for async H2D/D2H transfers
buffer = torch.zeros(size, pin_memory=True)
```

## 3. NVLink 4.0 与第三代 NVSwitch

### NVLink 4.0 规格

- **每 GPU 链路：** 18 条 NVLink 4.0 通道
- **单链路带宽：** 50 GB/s 双向
- **GPU 间总带宽：** 每 GPU 900 GB/s 双向

### 第三代 NVSwitch（8 GPU 节点拓扑）

一个 8 GPU SXM5 节点使用 **四颗 NVSwitch 3.0 芯片**，构成全 all-to-all 网状互联：

```
GPU0 ──┐
GPU1 ──┤
GPU2 ──┤   NVSwitch 0   ←→   NVSwitch 1
GPU3 ──┤         ↕               ↕
GPU4 ──┤   NVSwitch 2   ←→   NVSwitch 3
GPU5 ──┤
GPU6 ──┤
GPU7 ──┘

Every GPU has direct full-bandwidth path to every other GPU.
No multi-hop penalty unlike ring or tree topologies.
```

### 全互联为何重要

- 跨 8 个 GPU 的 all-reduce 保持 **900 GB/s 满带宽**（没有瓶颈 GPU）
- attention head 的张量并行 all-reduce 在典型规模下约 1 µs 完成
- 用于流水线并行的点对点 P2P 传输无损

### 实践中验证 NVLink

```bash
# Check NVLink topology
nvidia-smi topo -m

# Monitor NVLink traffic per GPU
nvidia-smi dmon -s u -d 1

# NCCL topology detection
NCCL_DEBUG=INFO torchrun --nproc_per_node=8 your_script.py 2>&1 | grep "NCCL"
```

## 4. SXM5 基板与主机连接

- **PCIe：** 每 GPU PCIe 5.0 x16（在 Grace-Hopper 上经 NVLink C2C 桥接到 CPU）
- **NVMe：** 经 PCIe 5.0 直连本地 NVMe 的 GPUDirect Storage 路径
- **热管理：** 液冷 SXM5 模块；GPU 结温目标 < 83°C

### CPU-GPU 内存（Grace Hopper 超级芯片版本）

在 GH200（Grace + H200）上，CPU 与 GPU 共享 **统一的 900 GB/s NVLink-C2C** 互联 —— CPU 的 LPDDR5x 与 GPU 的 HBM3e 处于同一地址空间。这消除了 CPU-GPU 数据搬运的 PCIe 瓶颈。

## 5. 功耗与散热

| 属性 | 值 |
|---|---|
| 单 H200 TDP | 700 W |
| 8 GPU 节点 TDP | ~5,600 W（仅 GPU） |
| 散热方式 | 直接液冷（DLC） |
| 冷却液入口温度 | 建议 ≤ 45°C |

机架 PDU 至少按 **每节点 7.5 kW** 设计（计入 CPU、NVSwitch、网络设备）。

## 6. 面向 AI 的关键架构特性

### Transformer Engine

H200 内置 **Transformer Engine**，可逐层自动选择 FP8 或 BF16 精度：

```python
# PyTorch + Transformer Engine (TE)
import transformer_engine.pytorch as te

# Replace standard Linear with TE Linear — auto FP8
layer = te.Linear(in_features, out_features, bias=True)

# TE handles scaling factors, amax history, and E4M3/E5M2 selection
```

### 基于 NVSwitch 的网内计算

NVSwitch 3.0 支持 **SHARP（Scalable Hierarchical Aggregation and Reduction Protocol）** 网内归约 —— all-reduce 操作部分在交换芯片内完成，减少 GPU 用于通信的周期。

## 参考文献

- [H200 数据手册](https://www.nvidia.com/en-us/data-center/h200/)
- [Hopper 架构白皮书](https://resources.nvidia.com/en-us-tensor-core/gtc22-whitepaper-hopper)
- [NVLink 4.0 技术博客](https://developer.nvidia.com/blog/nvidia-hopper-architecture-in-depth/)
- [Transformer Engine 文档](https://docs.nvidia.com/deeplearning/transformer-engine/user-guide/)


<details>
<summary>English original</summary>

**01 — H200 Hardware Architecture**

**1. Die and Process**

- **GPU die:** GH100 (Hopper architecture, TSMC 4N)
- **Transistors:** 80 billion
- **SMs:** 132 Streaming Multiprocessors
- **CUDA cores:** 16,896
- **Tensor Cores:** 4th generation (FP8, FP16, BF16, TF32, INT8, INT4)
- **FP8 peak:** ~3,958 TFLOPS (dense)
- **BF16 peak:** ~1,979 TFLOPS (dense)
- **TF32 peak:** ~989 TFLOPS

The H200 uses the same GH100 die as H100 but replaces HBM3 with HBM3e stacks for higher capacity and bandwidth.

**2. HBM3e Memory Subsystem**

| Property | H100 SXM5 | H200 SXM5 |
|---|---|---|
| Memory type | HBM3 | HBM3e |
| Capacity | 80 GB | 141 GB |
| Bandwidth | 3.35 TB/s | 4.8 TB/s |
| Stacks | 5 | 6 |

**Why HBM3e Matters for AI**

- Larger KV caches for long-context LLM inference (fit 128K+ tokens in memory)
- Bigger model shards → fewer pipeline stages → less pipeline bubble overhead
- 43% more bandwidth → memory-bound ops (attention, embedding lookups) run faster

**Memory Access Best Practices**

```python
# Profile actual HBM bandwidth utilization
import torch
torch.backends.cuda.matmul.allow_tf32 = True
torch.backends.cudnn.allow_tf32 = True

# Always use BF16 for training (numerically stable, uses Tensor Cores)
model = model.to(torch.bfloat16)

# Pin CPU buffers for async H2D/D2H transfers
buffer = torch.zeros(size, pin_memory=True)
```

**3. NVLink 4.0 and NVSwitch 3rd Gen**

**NVLink 4.0 Specs**

- **Links per GPU:** 18 NVLink 4.0 lanes
- **Bandwidth per link:** 50 GB/s bidirectional
- **Total GPU-to-GPU bandwidth:** 900 GB/s bidirectional per GPU

**NVSwitch 3rd Gen (8-GPU Node Topology)**

An 8-GPU SXM5 node uses **four NVSwitch 3.0 chips** forming a full all-to-all mesh:

```
GPU0 ──┐
GPU1 ──┤
GPU2 ──┤   NVSwitch 0   ←→   NVSwitch 1
GPU3 ──┤         ↕               ↕
GPU4 ──┤   NVSwitch 2   ←→   NVSwitch 3
GPU5 ──┤
GPU6 ──┤
GPU7 ──┘

Every GPU has direct full-bandwidth path to every other GPU.
No multi-hop penalty unlike ring or tree topologies.
```

**Why Full Mesh Matters**

- All-reduce across 8 GPUs stays at **full 900 GB/s** (no bottleneck GPUs)
- Tensor parallel all-reduce for attention heads completes in ~1 µs for typical sizes
- Point-to-point P2P transfers for pipeline parallelism are lossless

**Verifying NVLink in Practice**

```bash
# Check NVLink topology
nvidia-smi topo -m

# Monitor NVLink traffic per GPU
nvidia-smi dmon -s u -d 1

# NCCL topology detection
NCCL_DEBUG=INFO torchrun --nproc_per_node=8 your_script.py 2>&1 | grep "NCCL"
```

**4. SXM5 Baseboard and Host Connectivity**

- **PCIe:** PCIe 5.0 x16 per GPU (via NVLink C2C bridge to CPU on Grace-Hopper)
- **NVMe:** Direct GPUDirect Storage paths to local NVMe over PCIe 5.0
- **Thermal:** Liquid-cooled SXM5 module; GPU junction temperature target < 83°C

**CPU-GPU Memory (Grace Hopper Superchip variant)**

On GH200 (Grace + H200), CPU and GPU share a **unified 900 GB/s NVLink-C2C** fabric — CPU LPDDR5x and GPU HBM3e appear in the same address space. This eliminates PCIe bottlenecks for CPU-GPU data movement.

**5. Power and Cooling**

| Property | Value |
|---|---|
| TDP per H200 | 700 W |
| 8-GPU node TDP | ~5,600 W (GPUs only) |
| Cooling method | Direct liquid cooling (DLC) |
| Inlet coolant temp | ≤ 45°C recommended |

Design your rack PDU for at least **7.5 kW per node** (accounting for CPUs, NVSwitches, networking).

**6. Key Architectural Features for AI**

**Transformer Engine**

The H200 includes a **Transformer Engine** that automatically selects FP8 or BF16 precision per layer:

```python
# PyTorch + Transformer Engine (TE)
import transformer_engine.pytorch as te

# Replace standard Linear with TE Linear — auto FP8
layer = te.Linear(in_features, out_features, bias=True)

# TE handles scaling factors, amax history, and E4M3/E5M2 selection
```

**In-Network Computing via NVSwitch**

NVSwitch 3.0 supports **SHARP (Scalable Hierarchical Aggregation and Reduction Protocol)** in-network reductions — all-reduce operations are partially computed inside the switch fabric, reducing GPU cycles spent on communication.

**References**

- [H200 Datasheet](https://www.nvidia.com/en-us/data-center/h200/)
- [Hopper Architecture Whitepaper](https://resources.nvidia.com/en-us-tensor-core/gtc22-whitepaper-hopper)
- [NVLink 4.0 Technical Blog](https://developer.nvidia.com/blog/nvidia-hopper-architecture-in-depth/)
- [Transformer Engine Documentation](https://docs.nvidia.com/deeplearning/transformer-engine/user-guide/)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/8x-H200-Training-Inference/01-Hardware-Architecture.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/8x-H200-Training-Inference/01-Hardware-Architecture.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
