---
title: Qwen 推理优化 — 5 讲系列
description: Qwen 推理优化 — 5 讲系列
---

# Qwen 推理优化 — 5 讲系列

一套动手实操、硬件优先的系列，讲如何从两个恰好覆盖部署谱系两端的特定 Qwen 模型中拿到真实吞吐：

* **Qwen3-4B-Instruct (Q4_K_M)** — 边缘目标，运行在 Jetson Orin Nano 8 GB 上。
* **Qwen2.5-72B-Instruct (FP16)** — 数据中心目标，需要多 GPU。

同一架构家族，硬件工程完全不同。本系列先讲两个端点，再通过跨模型策略（投机解码、边缘↔云端路由）把它们统一起来。

**范围：** 仅限推理。训练与微调不在范围内。

| 讲次 | 标题 | 重点 |
|---|---|---|
| 01 | Qwen 架构深入剖析 | 配置、形状、分组查询注意力、旋转位置编码（RoPE）、SwiGLU、tokenizer |
| 02 | 把 Qwen3-4B 量化到 Q4 | AWQ、GPTQ、K-quants、校准、GGUF 布局 |
| 03 | Jetson 上的 decode（逐 token 生成阶段）优化 | GEMV（矩阵-向量乘）链、KV cache、融合、CUDA Graphs、INT8 KV |
| 04 | Qwen2.5-72B 多 GPU FP16 | TP/PP、NCCL 热路径、paged attention、YaRN |
| 05 | 跨模型与生产环境推理服务 | 投机解码、vLLM/TRT-LLM、可观测性、混合 |
| 06 | 批 GEMM（矩阵-矩阵乘）vs 普通 GEMM | cuBLAS API 形式、布局、张量核心、位精确可复现性 |

**前置要求：**

* 阶段 5 — 边缘 AI — [边缘大语言模型推理内幕](../03-边缘LLM推理内部机制/Lecture-01.md)（GEMV vs GEMM，roofline（性能上界模型））
* 阶段 4 方向 B — [Jetson 实时推理](../../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/11-Orin-Nano实时推理/Guide.md)
* 阶段 4 方向 C — [量化](../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md)


<details>
<summary>English original</summary>

**Qwen Inference Optimization — 5-Lecture Series**

A hands-on, hardware-first series on getting real throughput out of two specific Qwen models that bracket the deployment spectrum:

* **Qwen3-4B-Instruct (Q4_K_M)** — edge target, runs on Jetson Orin Nano 8 GB.
* **Qwen2.5-72B-Instruct (FP16)** — datacenter target, needs multi-GPU.

Same architecture family, completely different hardware engineering. The series teaches both endpoints, then unifies them through cross-model strategies (speculative decoding, edge↔cloud routing).

**Scope:** inference only. Training and fine-tuning are out of scope.

| Lecture | Title | Focus |
|---|---|---|
| 01 | Qwen Architecture Deep Dive | Config, shapes, GQA, RoPE, SwiGLU, tokenizer |
| 02 | Quantizing Qwen3-4B to Q4 | AWQ, GPTQ, K-quants, calibration, GGUF layout |
| 03 | Decode Optimization on Jetson | GEMV chain, KV cache, fusion, CUDA Graphs, INT8 KV |
| 04 | Qwen2.5-72B Multi-GPU FP16 | TP/PP, NCCL hot path, paged attention, YaRN |
| 05 | Cross-Model & Production Serving | Speculative decoding, vLLM/TRT-LLM, observability, hybrid |
| 06 | Batched GEMM vs Normal GEMM | cuBLAS API forms, layout, tensor cores, bit-exact reproducibility |

**Prerequisites:**

* Phase 5 — Edge AI — [Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md) (GEMV vs GEMM, roofline)
* Phase 4 Track B — [Jetson Real-Time Inference](../../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/11-Orin-Nano实时推理/Guide.md)
* Phase 4 Track C — [Quantization](../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Qwen Inference Optimization/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Qwen%20Inference%20Optimization/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
