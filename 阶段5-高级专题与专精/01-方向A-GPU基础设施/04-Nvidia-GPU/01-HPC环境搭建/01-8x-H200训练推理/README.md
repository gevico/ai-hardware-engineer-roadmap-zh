---
title: 8x H200 GPU — 训练与推理深度剖析
description: 8x H200 GPU — 训练与推理深度剖析
---

# 8x H200 GPU — 训练与推理深度剖析

NVIDIA H200 SXM5 是当前面向 AI 工作负载的旗舰 GPU，配备 141 GB HBM3e 内存，带宽 4.8 TB/s。由 8 块 GPU 组成、搭载 NVLink 4.0/NVSwitch 的 SXM 节点，是大模型训练与高吞吐推理的行业标准构建单元。

## 系统概览

| 属性 | 值 |
|---|---|
| GPU | NVIDIA H200 SXM5 |
| 数量 | 8 |
| 每 GPU 内存 | 141 GB HBM3e |
| GPU 总内存 | 1,128 GB（1.1 TB） |
| 内存带宽 | 每 GPU 4.8 TB/s |
| FP8 Tensor Core TFLOPS | 每 GPU 约 3,958 TFLOPS |
| BF16 Tensor Core TFLOPS | 每 GPU 约 1,979 TFLOPS |
| GPU 互连 | NVLink 4.0（900 GB/s 双向） |
| NVSwitch | 第三代（全互联） |
| 主机互连 | PCIe 5.0 / CXL |
| 形态 | SXM5 底板 |

## 主题索引

| # | 主题 | 说明 |
|---|---|---|
| 01 | [硬件架构](01-Hardware-Architecture.md) | 芯片设计、HBM3e、NVLink 4.0、NVSwitch 拓扑 |
| 02 | [训练配置](02-Training-Setup.md) | 分布式训练、3D 并行、FSDP、DeepSpeed |
| 03 | [推理配置](03-Inference-Setup.md) | 张量并行推理、vLLM、TensorRT-LLM |
| 04 | [内存管理](04-Memory-Management.md) | KV cache、paged attention、内存池化 |
| 05 | [性能优化](05-Performance-Optimization.md) | 性能剖析、roofline（性能上界模型）、kernel 调优、CUDA Graphs |
| 06 | [Benchmark 与验证](06-Benchmarks-and-Validation.md) | MFU、MBU、延迟、吞吐目标 |

## 快速导航

- **要训练 70B 模型？** → 从 [02-Training-Setup](02-Training-Setup.md) 开始
- **要做推理服务？** → 从 [03-Inference-Setup](03-Inference-Setup.md) 开始
- **GPU 内存 OOM？** → 参见 [04-Memory-Management](04-Memory-Management.md)
- **GPU 利用率偏低？** → 参见 [05-Performance-Optimization](05-Performance-Optimization.md)


<details>
<summary>English original</summary>

**8x H200 GPU — Training & Inference Deep Dive**

The NVIDIA H200 SXM5 is the current flagship GPU for AI workloads, featuring 141 GB HBM3e memory at 4.8 TB/s bandwidth. An 8-GPU SXM node with NVLink 4.0/NVSwitch is the industry-standard building block for large model training and high-throughput inference.

**System Snapshot**

| Property | Value |
|---|---|
| GPU | NVIDIA H200 SXM5 |
| Count | 8 |
| Memory per GPU | 141 GB HBM3e |
| Total GPU Memory | 1,128 GB (1.1 TB) |
| Memory Bandwidth | 4.8 TB/s per GPU |
| FP8 Tensor Core TFLOPS | ~3,958 TFLOPS per GPU |
| BF16 Tensor Core TFLOPS | ~1,979 TFLOPS per GPU |
| GPU Interconnect | NVLink 4.0 (900 GB/s bidirectional) |
| NVSwitch | 3rd Gen (full mesh) |
| Host Interconnect | PCIe 5.0 / CXL |
| Form Factor | SXM5 baseboard |

**Topic Index**

| # | Topic | Description |
|---|---|---|
| 01 | [Hardware Architecture](01-Hardware-Architecture.md) | Chip design, HBM3e, NVLink 4.0, NVSwitch topology |
| 02 | [Training Setup](02-Training-Setup.md) | Distributed training, 3D parallelism, FSDP, DeepSpeed |
| 03 | [Inference Setup](03-Inference-Setup.md) | Tensor parallel inference, vLLM, TensorRT-LLM |
| 04 | [Memory Management](04-Memory-Management.md) | KV cache, paged attention, memory pooling |
| 05 | [Performance Optimization](05-Performance-Optimization.md) | Profiling, roofline, kernel tuning, CUDA Graphs |
| 06 | [Benchmarks & Validation](06-Benchmarks-and-Validation.md) | MFU, MBU, latency, throughput targets |

**Quick Navigation**

- **Training a 70B model?** → Start with [02-Training-Setup](02-Training-Setup.md)
- **Inference serving?** → Start with [03-Inference-Setup](03-Inference-Setup.md)
- **GPU memory OOM?** → See [04-Memory-Management](04-Memory-Management.md)
- **Low GPU utilization?** → See [05-Performance-Optimization](05-Performance-Optimization.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/8x-H200-Training-Inference/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/8x-H200-Training-Inference/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
