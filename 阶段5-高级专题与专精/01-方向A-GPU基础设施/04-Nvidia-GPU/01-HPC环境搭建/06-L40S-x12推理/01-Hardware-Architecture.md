---
title: 01 — L40S 硬件架构
description: 01 — L40S 硬件架构
---

# 01 — L40S 硬件架构

## 1. 裸片与制程

- **GPU 裸片：** AD102（Ada Lovelace 架构，TSMC 4N）
- **同款裸片：** RTX 4090（消费级），但热管理/功耗/固件配置不同
- **晶体管数：** 763 亿
- **SM：** 142 个 Streaming Multiprocessor
- **CUDA 核心：** 18,176
- **Tensor Core：** 第 4 代（FP8、FP16、BF16、TF32、INT8、INT4）
- **FP8 TFLOPS：** ~733 TFLOPS（稀疏），~366 TFLOPS（稠密）
- **BF16 TFLOPS：** ~366 TFLOPS（稀疏），~183 TFLOPS（稠密）
- **TF32 TFLOPS：** ~183 TFLOPS（稠密）

> L40S 与 L40（无 FP8）的区别在于增加了 FP8 Tensor Core 支持，因此适合当前一代大语言模型推理。

## 2. GDDR6 内存子系统

| 属性 | L40S | A10（上一代） | A100 PCIe |
|---|---|---|---|
| 内存类型 | GDDR6 | GDDR6 | HBM2e |
| 容量 | 48 GB | 24 GB | 80 GB |
| 带宽 | 864 GB/s | 600 GB/s | 1,935 GB/s |
| 接口 | 384-bit | 384-bit | 5120-bit |

### GDDR6 vs HBM：关键差异

```
GDDR6 (L40S):
  + Cheaper to manufacture (standard PCB stacking)
  + Higher capacity per dollar
  + Good bandwidth for inference (memory-bound decode)
  − ~5× less bandwidth than HBM3e
  − PCIe attachment (shared CPU-GPU bandwidth)
  − No on-package NVLink possible

HBM3e (H200):
  + Extreme bandwidth (4.8 TB/s per GPU)
  + On-package with NVSwitch for GPU-GPU transfers
  − Expensive (specialized packaging)
  − Fixed capacity tiers (80 GB, 141 GB)
```

对于 LLM **decode**（逐 token 生成阶段，即吞吐瓶颈），内存带宽是关键指标。L40S 达到 864 GB/s，而 H200 为 4.8 TB/s——对于带宽受限工作负载，这一差距十分显著。

### 12 块 L40S 的内存容量规划

```
Total GPU memory: 12 × 48 GB = 576 GB

Model weight allocation (FP16):
  7B   model: 14 GB  → fits on 1 GPU (34 GB free for KV cache)
  13B  model: 26 GB  → fits on 1 GPU (22 GB free for KV cache)
  34B  model: 68 GB  → needs 2 GPUs (14 GB/GPU free)
  70B  model: 140 GB → needs 3-4 GPUs (depends on KV cache needs)
  180B model: 360 GB → needs 8-10 GPUs
```

## 3. PCIe 拓扑（无 NVLink）

L40S 使用 PCIe 4.0 x16 作为其唯一的主机互连和 GPU 间互连。这是需要理解的最重要的架构约束。

### PCIe 带宽 vs NVLink

```
PCIe 4.0 x16:   ~32 GB/s per direction (bidirectional: 64 GB/s)
NVLink 4.0:      900 GB/s bidirectional per GPU

Ratio: NVLink is 14× faster for GPU-to-GPU communication.
```

### 12-GPU PCIe 拓扑（典型服务器）

```
CPU 0 (socket 0)              CPU 1 (socket 1)
   |                               |
PCIe Root Complex 0          PCIe Root Complex 1
   |          |                |           |
Switch 0   Switch 1        Switch 2    Switch 3
  / \        / \              / \          / \
GPU0 GPU1  GPU2 GPU3       GPU4 GPU5   GPU6 GPU7
                                       |     |
                                      GPU8  GPU9
                                      GPU10 GPU11
```

### 验证 PCIe 拓扑

```bash
# Show full topology including NUMA and PCIe relationship
nvidia-smi topo -m

# Output (simplified):
#        GPU0 GPU1 GPU2 GPU3 ... CPU Affinity
# GPU0    X    SYS  SYS  SYS ...   0-15
# GPU1   SYS   X   SYS  SYS ...   0-15
# ...
# SYS = traverses PCIe through CPU NUMA node (highest latency)
# NODE = traverses PCIe within same NUMA node (medium latency)
# PHB = traverses PCIe host bridge (low latency)
# PXB = traverses PCIe switch (lowest latency, like NVLink)

# Measure actual P2P bandwidth
python -c "
import torch
a = torch.randn(1024*1024*256, device='cuda:0', dtype=torch.float16)  # 512 MB
b = torch.empty_like(a).to('cuda:1')
import time
for _ in range(5): b.copy_(a)
torch.cuda.synchronize()
t0 = time.perf_counter()
for _ in range(100): b.copy_(a)
torch.cuda.synchronize()
bw = 512e6 * 100 / (time.perf_counter() - t0) / 1e9
print(f'P2P bandwidth GPU0→GPU1: {bw:.1f} GB/s')
# Expected: 24-30 GB/s (PCIe 4.0, direct switch)
# Poor result: < 10 GB/s (traverses NUMA boundary)
"
```

## 4. L40S PCIe 外形尺寸优势

### 机架密度

```
2U server (typical):
  4 × L40S @ 350W = 1,400W total

4U server (dense GPU):
  8 × L40S @ 350W = 2,800W total

For 12 GPUs:
  Option A: 3 × 4U servers (4 GPUs each), cross-server via InfiniBand
  Option B: 1 × 6U or 8U super-dense chassis
  Option C: 2U + 4U combination

H100/H200 SXM reference:
  DGX H100: 8 GPUs, 10.2U, 10.2kW
  L40S equivalent 8 GPUs: ~4U, ~2.8kW
```

### 灵活性

- L40S 可在任意 PCIe 4.0 服务器中运行（无需 SXM 底板）
- 可与 CPU、FPGA 或网卡混插于同一机箱
- 标准供电接口（PCIe 16-pin，600W 线缆）
- 可单独更换（无需更换 SXM 模块）

## 5. 推理关键特性

### NVENC / NVDEC（媒体引擎）

每块 L40S 包含 2× NVENC + 2× NVDEC——与处理视频的多模态推理流水线相关。


<details>
<summary>English original</summary>

**01 — L40S Hardware Architecture**

**1. Die and Process**

- **GPU die:** AD102 (Ada Lovelace architecture, TSMC 4N)
- **Same die as:** RTX 4090 (consumer), but different thermal/power/firmware profile
- **Transistors:** 76.3 billion
- **SMs:** 142 Streaming Multiprocessors
- **CUDA cores:** 18,176
- **Tensor Cores:** 4th generation (FP8, FP16, BF16, TF32, INT8, INT4)
- **FP8 TFLOPS:** ~733 TFLOPS (sparse), ~366 TFLOPS (dense)
- **BF16 TFLOPS:** ~366 TFLOPS (sparse), ~183 TFLOPS (dense)
- **TF32 TFLOPS:** ~183 TFLOPS (dense)

> The L40S is distinguished from L40 (no FP8) by adding FP8 Tensor Core support, making it suitable for current-generation LLM inference.

**2. GDDR6 Memory Subsystem**

| Property | L40S | A10 (prev gen) | A100 PCIe |
|---|---|---|---|
| Memory type | GDDR6 | GDDR6 | HBM2e |
| Capacity | 48 GB | 24 GB | 80 GB |
| Bandwidth | 864 GB/s | 600 GB/s | 1,935 GB/s |
| Interface | 384-bit | 384-bit | 5120-bit |

**GDDR6 vs HBM: Key Differences**

```
GDDR6 (L40S):
  + Cheaper to manufacture (standard PCB stacking)
  + Higher capacity per dollar
  + Good bandwidth for inference (memory-bound decode)
  − ~5× less bandwidth than HBM3e
  − PCIe attachment (shared CPU-GPU bandwidth)
  − No on-package NVLink possible

HBM3e (H200):
  + Extreme bandwidth (4.8 TB/s per GPU)
  + On-package with NVSwitch for GPU-GPU transfers
  − Expensive (specialized packaging)
  − Fixed capacity tiers (80 GB, 141 GB)
```

For LLM **decode** (the throughput bottleneck), memory bandwidth is the critical metric. L40S achieves 864 GB/s vs H200's 4.8 TB/s — the gap is significant for memory-bound workloads.

**Memory Capacity Planning for 12x L40S**

```
Total GPU memory: 12 × 48 GB = 576 GB

Model weight allocation (FP16):
  7B   model: 14 GB  → fits on 1 GPU (34 GB free for KV cache)
  13B  model: 26 GB  → fits on 1 GPU (22 GB free for KV cache)
  34B  model: 68 GB  → needs 2 GPUs (14 GB/GPU free)
  70B  model: 140 GB → needs 3-4 GPUs (depends on KV cache needs)
  180B model: 360 GB → needs 8-10 GPUs
```

**3. PCIe Topology (No NVLink)**

The L40S uses PCIe 4.0 x16 as its only host and GPU-to-GPU interconnect. This is the most important architectural constraint to understand.

**PCIe Bandwidth vs NVLink**

```
PCIe 4.0 x16:   ~32 GB/s per direction (bidirectional: 64 GB/s)
NVLink 4.0:      900 GB/s bidirectional per GPU

Ratio: NVLink is 14× faster for GPU-to-GPU communication.
```

**12-GPU PCIe Topology (Typical Server)**

```
CPU 0 (socket 0)              CPU 1 (socket 1)
   |                               |
PCIe Root Complex 0          PCIe Root Complex 1
   |          |                |           |
Switch 0   Switch 1        Switch 2    Switch 3
  / \        / \              / \          / \
GPU0 GPU1  GPU2 GPU3       GPU4 GPU5   GPU6 GPU7
                                       |     |
                                      GPU8  GPU9
                                      GPU10 GPU11
```

**Verifying PCIe Topology**

```bash
# Show full topology including NUMA and PCIe relationship
nvidia-smi topo -m

# Output (simplified):
#        GPU0 GPU1 GPU2 GPU3 ... CPU Affinity
# GPU0    X    SYS  SYS  SYS ...   0-15
# GPU1   SYS   X   SYS  SYS ...   0-15
# ...
# SYS = traverses PCIe through CPU NUMA node (highest latency)
# NODE = traverses PCIe within same NUMA node (medium latency)
# PHB = traverses PCIe host bridge (low latency)
# PXB = traverses PCIe switch (lowest latency, like NVLink)

# Measure actual P2P bandwidth
python -c "
import torch
a = torch.randn(1024*1024*256, device='cuda:0', dtype=torch.float16)  # 512 MB
b = torch.empty_like(a).to('cuda:1')
import time
for _ in range(5): b.copy_(a)
torch.cuda.synchronize()
t0 = time.perf_counter()
for _ in range(100): b.copy_(a)
torch.cuda.synchronize()
bw = 512e6 * 100 / (time.perf_counter() - t0) / 1e9
print(f'P2P bandwidth GPU0→GPU1: {bw:.1f} GB/s')
# Expected: 24-30 GB/s (PCIe 4.0, direct switch)
# Poor result: < 10 GB/s (traverses NUMA boundary)
"
```

**4. L40S PCIe Form Factor Advantages**

**Rack Density**

```
2U server (typical):
  4 × L40S @ 350W = 1,400W total

4U server (dense GPU):
  8 × L40S @ 350W = 2,800W total

For 12 GPUs:
  Option A: 3 × 4U servers (4 GPUs each), cross-server via InfiniBand
  Option B: 1 × 6U or 8U super-dense chassis
  Option C: 2U + 4U combination

H100/H200 SXM reference:
  DGX H100: 8 GPUs, 10.2U, 10.2kW
  L40S equivalent 8 GPUs: ~4U, ~2.8kW
```

**Flexibility**

- L40S can run in any PCIe 4.0 server (no SXM baseboard needed)
- Mix with CPUs, FPGAs, or networking cards in same chassis
- Standard power connectors (PCIe 16-pin, 600W cable)
- Replaceable individually (no SXM module replacement)

**5. Key Features for Inference**

**NVENC / NVDEC (Media Engines)**

L40S includes 2× NVENC + 2× NVDEC per GPU — relevant for multimodal inference pipelines processing video.

</details>

### Ada Lovelace 着色器执行重排序（SER）

SER 动态重排序着色器工作负载以提升 occupancy——主要用于图形。对于计算/AI，适用标准 CUDA 调度。

### ADA FP8 vs Hopper FP8

```
Ada (L40S) FP8:    FP8 Tensor Cores, E4M3 and E5M2 formats
Hopper (H200) FP8: FP8 + Transformer Engine for automated scaling
                   + hardware-accelerated amax tracking

For L40S: FP8 quantization must be done offline (PTQ)
          No hardware delayed scaling support
          Use GPTQ/AWQ for post-training quantization instead
```

## 6. 功耗与散热

| 属性 | 值 |
|---|---|
| 单块 L40S TDP | 350 W |
| 12-GPU 系统 TDP | ~4,200 W（GPU） |
| 散热 | 风冷（被动散热器 + 服务器风扇） |
| 所需风量 | 前后向，建议 200+ CFM |
| PCIe 供电接口 | 16-pin ATX 3.0（可支持 600W） |

### 热管理

```bash
# Monitor GPU temperatures and fan speed
nvidia-smi dmon -s pucvt -d 5 -i 0,1,2,3,4,5,6,7,8,9,10,11

# Set power limit (if thermal throttling occurs)
sudo nvidia-smi -pl 300 -i 0  # reduce to 300W for GPU 0

# Check throttling reasons
nvidia-smi -q -d PERFORMANCE | grep "Reason"
# "Active: Yes" under "SW Thermal Slowdown" means throttling
```

## 参考文献

- [L40S 数据手册](https://www.nvidia.com/en-us/data-center/l40s/)
- [Ada Lovelace 架构白皮书](https://images.nvidia.com/akamai/marketing/documents/Ada-GPU-Architecture-Overview.pdf)
- [PCIe 4.0 规范](https://pcisig.com/pcie-4.0)
- [NVIDIA L40S 部署指南](https://docs.nvidia.com/deeplearning/performance/dl-performance-gpu-background/index.html)


<details>
<summary>English original</summary>

**Ada Lovelace Shader Execution Reordering (SER)**

SER dynamically reorders shader workloads to improve occupancy — primarily useful for graphics. For compute/AI, standard CUDA scheduling applies.

**ADA FP8 vs Hopper FP8**

```
Ada (L40S) FP8:    FP8 Tensor Cores, E4M3 and E5M2 formats
Hopper (H200) FP8: FP8 + Transformer Engine for automated scaling
                   + hardware-accelerated amax tracking

For L40S: FP8 quantization must be done offline (PTQ)
          No hardware delayed scaling support
          Use GPTQ/AWQ for post-training quantization instead
```

**6. Power and Cooling**

| Property | Value |
|---|---|
| TDP per L40S | 350 W |
| 12-GPU system TDP | ~4,200 W (GPUs) |
| Cooling | Air-cooled (passive heatsink + server fans) |
| Required airflow | Front-to-back, 200+ CFM recommended |
| PCIe power connector | 16-pin ATX 3.0 (600W capable) |

**Thermal Management**

```bash
# Monitor GPU temperatures and fan speed
nvidia-smi dmon -s pucvt -d 5 -i 0,1,2,3,4,5,6,7,8,9,10,11

# Set power limit (if thermal throttling occurs)
sudo nvidia-smi -pl 300 -i 0  # reduce to 300W for GPU 0

# Check throttling reasons
nvidia-smi -q -d PERFORMANCE | grep "Reason"
# "Active: Yes" under "SW Thermal Slowdown" means throttling
```

**References**

- [L40S Datasheet](https://www.nvidia.com/en-us/data-center/l40s/)
- [Ada Lovelace Architecture Whitepaper](https://images.nvidia.com/akamai/marketing/documents/Ada-GPU-Architecture-Overview.pdf)
- [PCIe 4.0 Specification](https://pcisig.com/pcie-4.0)
- [NVIDIA L40S Deployment Guide](https://docs.nvidia.com/deeplearning/performance/dl-performance-gpu-background/index.html)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/L40S-x12-Inference/01-Hardware-Architecture.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/L40S-x12-Inference/01-Hardware-Architecture.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
