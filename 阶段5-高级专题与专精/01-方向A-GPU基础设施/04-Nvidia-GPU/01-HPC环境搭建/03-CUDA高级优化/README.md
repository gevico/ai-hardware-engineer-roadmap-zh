---
title: CUDA 高级优化 — 深入解析
description: CUDA 高级优化 — 深入解析
---

# CUDA 高级优化 — 深入解析

NVIDIA、OpenAI 与 Meta 的 GPU 工程师用这五项技术把推理与高性能计算（HPC）kernel 推到硬件极限。它们远超基础 CUDA 编程 —— 正是它们区分了「能跑」的 GPU kernel 与「生产级」的 GPU kernel。

## 为什么这些技术重要

```
Naive CUDA kernel:         ~30% of hardware peak  (common)
With kernel fusion:        ~50% of hardware peak
With CUDA Graphs:          +10-30% latency reduction
With cooperative groups:   enables algorithms impossible without them
With persistent kernels:   near-zero kernel launch overhead
With warp specialization:  ~70-85% of hardware peak  (elite)
```

每个 LLM 推理引擎（TensorRT-LLM、vLLM、FasterTransformer、FlashAttention）都用到了全部五项。

## 主题索引

| # | 主题 | 解决的问题 |
|---|---|---|
| 01 | [CUDA Graphs](01-CUDA-Graphs.md) | 小批大小下 CPU 启动开销拖垮延迟 |
| 02 | [Cooperative Groups](02-Cooperative-Groups.md) | 线程块边界限制了同步的灵活性 |
| 03 | [Persistent Kernels](03-Persistent-Kernels.md) | 反复的 kernel 启动浪费 SM 建立时间 |
| 04 | [Kernel Fusion](04-Kernel-Fusion.md) | 分离的 kernel 把 HBM 带宽浪费在中间结果上 |
| 05 | [Warp Specialization](05-Warp-Specialization.md) | kernel 内部的计算延迟与访存延迟没有重叠 |

## 它们之间的关系

```
CUDA Graphs          → reduces CPU↔GPU interface overhead
Cooperative Groups   → enables flexible intra-kernel synchronization
Persistent Kernels   → eliminates kernel launch overhead entirely
Kernel Fusion        → reduces HBM round-trips between operations
Warp Specialization  → overlaps compute and memory within a single kernel

Combined (e.g. FlashAttention-3):
  Persistent kernel + warp specialization + cooperative groups
  → 90%+ of H200 BF16 peak on attention kernels
```

## 快速导航

- **LLM 推理延迟太高？** → [01-CUDA-Graphs](01-CUDA-Graphs.md)
- **在写自定义归约/扫描？** → [02-Cooperative-Groups](02-Cooperative-Groups.md)
- **在 profile 里能看到 kernel 启动开销？** → [03-Persistent-Kernels](03-Persistent-Kernels.md)
- **GPU 内存带宽瓶颈？** → [04-Kernel-Fusion](04-Kernel-Fusion.md)
- **想写 FlashAttention 风格的 kernel？** → [05-Warp-Specialization](05-Warp-Specialization.md)


<details>
<summary>English original</summary>

**CUDA Advanced Optimization — Deep Dive**

Five techniques used by GPU engineers at NVIDIA, OpenAI, and Meta to push inference and HPC kernels to hardware limits. These go far beyond basic CUDA programming — they are what separates a "working" GPU kernel from a "production" one.

**Why These Techniques Matter**

```
Naive CUDA kernel:         ~30% of hardware peak  (common)
With kernel fusion:        ~50% of hardware peak
With CUDA Graphs:          +10-30% latency reduction
With cooperative groups:   enables algorithms impossible without them
With persistent kernels:   near-zero kernel launch overhead
With warp specialization:  ~70-85% of hardware peak  (elite)
```

Every LLM inference engine (TensorRT-LLM, vLLM, FasterTransformer, FlashAttention) uses all five.

**Topic Index**

| # | Topic | What It Solves |
|---|---|---|
| 01 | [CUDA Graphs](01-CUDA-Graphs.md) | CPU launch overhead kills latency at small batch sizes |
| 02 | [Cooperative Groups](02-Cooperative-Groups.md) | Thread block boundary limits synchronization flexibility |
| 03 | [Persistent Kernels](03-Persistent-Kernels.md) | Repeated kernel launches waste SM setup time |
| 04 | [Kernel Fusion](04-Kernel-Fusion.md) | Separate kernels waste HBM bandwidth on intermediate results |
| 05 | [Warp Specialization](05-Warp-Specialization.md) | Compute and memory latency are not overlapped inside a kernel |

**How They Relate**

```
CUDA Graphs          → reduces CPU↔GPU interface overhead
Cooperative Groups   → enables flexible intra-kernel synchronization
Persistent Kernels   → eliminates kernel launch overhead entirely
Kernel Fusion        → reduces HBM round-trips between operations
Warp Specialization  → overlaps compute and memory within a single kernel

Combined (e.g. FlashAttention-3):
  Persistent kernel + warp specialization + cooperative groups
  → 90%+ of H200 BF16 peak on attention kernels
```

**Quick Navigation**

- **LLM inference latency too high?** → [01-CUDA-Graphs](01-CUDA-Graphs.md)
- **Writing a custom reduction/scan?** → [02-Cooperative-Groups](02-Cooperative-Groups.md)
- **Kernel launch overhead visible in profile?** → [03-Persistent-Kernels](03-Persistent-Kernels.md)
- **GPU memory bandwidth bottleneck?** → [04-Kernel-Fusion](04-Kernel-Fusion.md)
- **Want to write FlashAttention-style kernels?** → [05-Warp-Specialization](05-Warp-Specialization.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/CUDA-Advanced-Optimization/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/CUDA-Advanced-Optimization/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
