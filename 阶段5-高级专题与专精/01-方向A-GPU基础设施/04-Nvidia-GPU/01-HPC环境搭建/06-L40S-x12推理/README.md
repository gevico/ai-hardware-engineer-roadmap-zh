---
title: L40S x12 — 推理深度解析
description: L40S x12 — 推理深度解析
---

# L40S x12 — 推理深度解析

NVIDIA L40S 是一款基于 PCIe 的 GPU，面向 AI 推理、图形和企业级工作负载设计。与 H100/H200 SXM 不同，它使用 GDDR6 内存并通过 PCIe 连接——因此在对推理密集型部署中更具成本效益，这类部署中 HBM 的极高带宽并非瓶颈。

## 系统概览

| 属性 | 值 |
|---|---|
| GPU | NVIDIA L40S |
| 数量 | 12 |
| 单 GPU 内存 | 48 GB GDDR6 |
| GPU 总内存 | 576 GB |
| 内存带宽 | 每 GPU 864 GB/s |
| FP8 Tensor Core TFLOPS | 每 GPU ~733 TFLOPS |
| BF16 / FP16 TFLOPS | 每 GPU ~366 TFLOPS（稀疏） |
| TF32 TFLOPS | 每 GPU ~183 TFLOPS |
| GPU 互连 | PCIe 4.0 x16（无 NVLink） |
| 形态规格 | PCIe 全高、双槽 |
| TDP | 每 GPU 350 W |

## L40S vs H200：何时选择 L40S

| 因素 | L40S x12 | H200 x8 |
|---|---|---|
| 总内存 | 576 GB GDDR6 | 1,128 GB HBM3e |
| 内存带宽 | 合计 10.4 TB/s | 合计 38.4 TB/s |
| GPU 互连 | PCIe（无 NVLink） | NVLink 4.0 900 GB/s |
| 成本（约） | ~$60K | ~$400K+ |
| 最佳适用场景 | 高性价比推理 | 训练 + 大模型推理 |
| 整机功耗 | ~4,200 W（12 块 GPU） | ~5,600 W（8 块 GPU） |
| 单模型最大规模 | ~70B（多 GPU） | ~405B（多 GPU） |

**以下情况选择 L40S：**推理吞吐比模型规模更重要、成本受限，或需同时运行多个较小的模型。

## 主题索引

| # | 主题 | 描述 |
|---|---|---|
| 01 | [硬件架构](01-Hardware-Architecture.md) | Ada Lovelace die、GDDR6、PCIe 拓扑、无 NVLink |
| 02 | [推理优化](02-Inference-Optimization.md) | vLLM、TRT-LLM、量化、批处理策略 |
| 03 | [多 GPU 策略](03-Multi-GPU-Strategy.md) | 受 PCIe 约束的并行、流水线并行 vs 张量并行 |
| 04 | [部署指南](04-Deployment-Guide.md) | 多实例部署、模型分片、生产环境搭建 |
| 05 | [benchmark](05-Benchmarks.md) | 吞吐目标、延迟基线、成本/性能对比 |

## 快速导航

- **要跑 7B 模型的推理服务？** → [04-部署指南](04-Deployment-Guide.md) — 单 GPU 推理
- **要跑 70B 模型的推理服务？** → [03-多 GPU 策略](03-Multi-GPU-Strategy.md) — 2 块 GPU，TP=2
- **要最大化吞吐？** → [02-推理优化](02-Inference-Optimization.md)
- **遇到性能瓶颈？** → [05-benchmark](05-Benchmarks.md)


<details>
<summary>English original</summary>

**L40S x12 — Inference Deep Dive**

The NVIDIA L40S is a PCIe-based GPU designed for AI inference, graphics, and enterprise workloads. Unlike H100/H200 SXM, it uses GDDR6 memory and connects via PCIe — making it more cost-effective for inference-heavy deployments where the extreme bandwidth of HBM is not the bottleneck.

**System Snapshot**

| Property | Value |
|---|---|
| GPU | NVIDIA L40S |
| Count | 12 |
| Memory per GPU | 48 GB GDDR6 |
| Total GPU Memory | 576 GB |
| Memory Bandwidth | 864 GB/s per GPU |
| FP8 Tensor Core TFLOPS | ~733 TFLOPS per GPU |
| BF16 / FP16 TFLOPS | ~366 TFLOPS per GPU (sparse) |
| TF32 TFLOPS | ~183 TFLOPS per GPU |
| GPU Interconnect | PCIe 4.0 x16 (no NVLink) |
| Form Factor | PCIe full-height, dual-slot |
| TDP | 350 W per GPU |

**L40S vs H200: When to Choose L40S**

| Factor | L40S x12 | H200 x8 |
|---|---|---|
| Total memory | 576 GB GDDR6 | 1,128 GB HBM3e |
| Memory bandwidth | 10.4 TB/s total | 38.4 TB/s total |
| GPU interconnect | PCIe (no NVLink) | NVLink 4.0 900 GB/s |
| Cost (approx) | ~$60K | ~$400K+ |
| Best for | Cost-efficient inference | Training + large model inference |
| Power/rack | ~4,200 W (12 GPUs) | ~5,600 W (8 GPUs) |
| Max single model | ~70B (multi-GPU) | ~405B (multi-GPU) |

**Choose L40S when:** inference throughput matters more than model size, cost is constrained, or you're running multiple smaller models simultaneously.

**Topic Index**

| # | Topic | Description |
|---|---|---|
| 01 | [Hardware Architecture](01-Hardware-Architecture.md) | Ada Lovelace die, GDDR6, PCIe topology, NVLink absence |
| 02 | [Inference Optimization](02-Inference-Optimization.md) | vLLM, TRT-LLM, quantization, batching strategies |
| 03 | [Multi-GPU Strategy](03-Multi-GPU-Strategy.md) | PCIe-constrained parallelism, pipeline vs tensor parallel |
| 04 | [Deployment Guide](04-Deployment-Guide.md) | Multi-instance deployment, model sharding, production setup |
| 05 | [Benchmarks](05-Benchmarks.md) | Throughput targets, latency baselines, cost/perf comparison |

**Quick Navigation**

- **Serving a 7B model?** → [04-Deployment-Guide](04-Deployment-Guide.md) — single GPU inference
- **Serving a 70B model?** → [03-Multi-GPU-Strategy](03-Multi-GPU-Strategy.md) — 2 GPUs with TP=2
- **Maximizing throughput?** → [02-Inference-Optimization](02-Inference-Optimization.md)
- **Performance bottleneck?** → [05-Benchmarks](05-Benchmarks.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/L40S-x12-Inference/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/L40S-x12-Inference/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
