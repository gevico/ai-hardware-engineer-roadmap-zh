---
title: 05 — 推理 runtime 与部署目标
description: 05 — 推理 runtime 与部署目标
---

# 05 — 推理 runtime 与部署目标

<div class="course-identity auto-course" style="--course-accent: #7c3aed; --course-accent-rgb: 124, 58, 237;" markdown="1">
<div class="course-identity__icon">IRDT</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · 编译器方向</p>
<p class="course-identity__title">05 的专门课程标识 — 推理 runtime 与部署目标。</p>
<p class="course-identity__meta">产物：编译器/推理优化 · 度量：算子数量、内存、延迟</p>
</div>
</div>


**顺序：** 第五。在图、kernel、编译器与量化（01–04）之后，你在类生产环境中部署并度量。

**岗位目标：** DL 推理优化工程师 · **MTS Kernels**（部署、生产可靠性、可度量的结果）。

---

## 为什么它排在第五

你的 kernel 与优化只有在 runtime 中正确运行并满足延迟/吞吐目标时才有意义。本单元覆盖主要推理 runtime，以及如何度量和比较它们。

---

## 1. Runtime

* **TensorRT** — Engine 构建、插件、动态形状、DLA（深度学习加速器）。你的 kernel 与图优化如何在 engine 中体现。
* **ONNX Runtime** — 执行 provider（CUDA、TensorRT、OpenVINO）。图优化与 provider 选择。
* **Triton Inference Server** — 批处理、模型并发、指标。推理服务多个模型与动态批处理。

---

## 2. TensorRT-LLM — 生产级大语言模型推理

[**TensorRT-LLM**](https://github.com/NVIDIA/TensorRT-LLM) 是 NVIDIA 的开源库，专为在 NVIDIA GPU 上进行大语言模型推理而构建。它通过大语言模型专用优化扩展 TensorRT，这些优化是通用 runtime 无法匹敌的。如果你在 NVIDIA 硬件上部署大语言模型，这就是生产路径。

### 为什么需要 TensorRT-LLM

标准 TensorRT 能很好地处理视觉和小模型，但大语言模型有独特挑战：
- **KV-cache 管理** — 随上下文长度线性增长，必须分页并在请求间复用。
- **自回归解码** — 每个 token 依赖所有先前 token；批处理很复杂。
- **多 GPU 推理服务** — 无法放入单个 GPU 的模型需要张量并行/流水线并行。
- **混合工作负载** — prefill（首字前的整段计算，算力受限）与 decode（逐 token 生成阶段，带宽受限）阶段有相反的瓶颈。

TensorRT-LLM 通过一个 Python API 解决所有这些问题，该 API 将大语言模型编译为带大语言模型专用 runtime 特性的优化 TensorRT engine。

### 核心特性

* **In-flight batching（连续批处理）：**
    * 请求到达时即进行批处理 — 不要等待组成完整批。新请求加入，而其他请求正处于生成中。
    * 通过跨请求混合 prefill 与 decode 阶段，最大化 GPU 利用率。

* **Paged KV-cache：**
    * 受 vLLM 的 PagedAttention 启发 — 以固定大小块分配 KV-cache，而非按序列连续分配。
    * 消除内存碎片；支持对更多并发序列进行推理服务。

* **量化：**
    * FP8（Hopper+）、INT8（SmoothQuant）、INT4（AWQ、GPTQ）— 全都在 GEMM（矩阵-矩阵乘）kernel 中融合反量化。
    * Blackwell 上的 FP4，用于最大吞吐。

* **张量并行与流水线并行：**
    * 将模型拆分到多个 GPU：张量并行（layer 内拆分）或流水线并行（跨 layer 拆分）。
    * 基于 NCCL 的通信，与计算重叠。

* **投机解码：**
    * 草稿模型生成候选 token；主模型在一次前向传播中验证。
    * 降低首 token 时间与整体延迟。

* **定制 Hopper/Blackwell kernel：**
    * 基于 CUTLASS 的 GEMM kernel，针对每代 GPU 优化。
    * warp 特化、persistent kernel、Transformer Engine 集成。

* **CUDA Graphs：**
    * 将 decode 循环捕获为 CUDA graph — 消除每 token 的 kernel 启动开销。

### 构建与部署工作流

```bash
# Install
pip install tensorrt-llm

# Step 1: Convert model checkpoint to TRT-LLM format
python convert_checkpoint.py \
    --model_dir ./llama-3-8b \
    --output_dir ./trt_ckpt \
    --dtype float16 \
    --tp_size 2           # tensor parallel across 2 GPUs

# Step 2: Build TRT-LLM engine
trtllm-build \
    --checkpoint_dir ./trt_ckpt \
    --output_dir ./trt_engine \
    --gemm_plugin float16 \
    --max_batch_size 64 \
    --max_input_len 2048 \
    --max_seq_len 4096 \
    --paged_kv_cache enable \
    --use_fused_mlp enable

# Step 3: Run inference
python run.py \
    --engine_dir ./trt_engine \
    --tokenizer_dir ./llama-3-8b \
    --input_text "Explain how a systolic array works"

# Step 4: Serve with Triton Inference Server
# TRT-LLM integrates with Triton via the TRT-LLM backend
# → in-flight batching, streaming, multi-model serving
```


<details>
<summary>English original</summary>

**05 — Inference Runtimes & Deployment Targets**

<div class="course-identity auto-course" style="--course-accent: #7c3aed; --course-accent-rgb: 124, 58, 237;" markdown="1">
<div class="course-identity__icon">IRDT</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Compiler Track</p>
<p class="course-identity__title">Specialized course identity for 05 — Inference Runtimes & Deployment Targets.</p>
<p class="course-identity__meta">Artifact: compiler/inference optimization · Measure: op count, memory, latency</p>
</div>
</div>


**Order:** Fifth. After graph, kernels, compiler, and quantization (01–04), you deploy and measure in production-like settings.

**Role target:** DL Inference Optimization Engineer · **MTS Kernels** (deployment, production reliability, measurable outcomes).

---

**Why this comes fifth**

Your kernels and optimizations only matter if they run correctly in a runtime and meet latency/throughput goals. This unit covers the main inference runtimes and how to measure and compare them.

---

**1. Runtimes**

* **TensorRT** — Engine build, plugins, dynamic shapes, DLA. How your kernels and graph optimizations show up in the engine.
* **ONNX Runtime** — Execution providers (CUDA, TensorRT, OpenVINO). Graph optimizations and provider selection.
* **Triton Inference Server** — Batching, model concurrency, metrics. Serving multiple models and dynamic batching.

---

**2. TensorRT-LLM — Production LLM Inference**

[**TensorRT-LLM**](https://github.com/NVIDIA/TensorRT-LLM) is NVIDIA's open-source library purpose-built for LLM inference on NVIDIA GPUs. It extends TensorRT with LLM-specific optimizations that general-purpose runtimes cannot match. If you deploy large language models on NVIDIA hardware, this is the production path.

**Why TensorRT-LLM exists**

Standard TensorRT handles vision and small models well, but LLMs have unique challenges:
- **KV-cache management** — grows linearly with context length, must be paged and reused across requests.
- **Autoregressive decoding** — each token depends on all previous tokens; batching is complex.
- **Multi-GPU serving** — models that don't fit on one GPU need tensor/pipeline parallelism.
- **Mixed workloads** — prefill (compute-bound) and decode (memory-bound) phases have opposite bottlenecks.

TensorRT-LLM solves all of these with a Python API that compiles LLMs into optimized TensorRT engines with LLM-specific runtime features.

**Core features**

* **In-flight batching (continuous batching):**
    * Batch requests as they arrive — don't wait for a full batch. New requests join while others are mid-generation.
    * Maximizes GPU utilization by mixing prefill and decode phases across requests.

* **Paged KV-cache:**
    * Inspired by vLLM's PagedAttention — allocates KV-cache in fixed-size blocks, not contiguous per-sequence.
    * Eliminates memory fragmentation; enables serving more concurrent sequences.

* **Quantization:**
    * FP8 (Hopper+), INT8 (SmoothQuant), INT4 (AWQ, GPTQ) — all with fused dequantize in GEMM kernels.
    * FP4 on Blackwell for maximum throughput.

* **Tensor parallelism and pipeline parallelism:**
    * Split model across GPUs: tensor parallel (split within layers) or pipeline parallel (split across layers).
    * NCCL-based communication, overlapped with compute.

* **Speculative decoding:**
    * Draft model generates candidate tokens; main model verifies in one forward pass.
    * Reduces time-to-first-token and overall latency.

* **Custom Hopper/Blackwell kernels:**
    * CUTLASS-based GEMM kernels optimized for each GPU generation.
    * Warp specialization, persistent kernels, Transformer Engine integration.

* **CUDA Graphs:**
    * Captures the decode loop as a CUDA graph — eliminates per-token kernel launch overhead.

**Build and deploy workflow**

```bash
# Install
pip install tensorrt-llm

# Step 1: Convert model checkpoint to TRT-LLM format
python convert_checkpoint.py \
    --model_dir ./llama-3-8b \
    --output_dir ./trt_ckpt \
    --dtype float16 \
    --tp_size 2           # tensor parallel across 2 GPUs

# Step 2: Build TRT-LLM engine
trtllm-build \
    --checkpoint_dir ./trt_ckpt \
    --output_dir ./trt_engine \
    --gemm_plugin float16 \
    --max_batch_size 64 \
    --max_input_len 2048 \
    --max_seq_len 4096 \
    --paged_kv_cache enable \
    --use_fused_mlp enable

# Step 3: Run inference
python run.py \
    --engine_dir ./trt_engine \
    --tokenizer_dir ./llama-3-8b \
    --input_text "Explain how a systolic array works"

# Step 4: Serve with Triton Inference Server
# TRT-LLM integrates with Triton via the TRT-LLM backend
# → in-flight batching, streaming, multi-model serving
```

</details>

### TensorRT-LLM vs vLLM

| | TensorRT-LLM | vLLM |
|---|---|---|
| **方法** | 将模型编译为优化后的 engine（提前编译） | 用 PyTorch + 自定义 CUDA kernel 做 JIT |
| **性能** | NVIDIA GPU 上吞吐最高（定制的 Hopper/Blackwell kernel） | 很好；峰值略低但迭代更快 |
| **量化** | FP8、INT8、INT4、FP4，配合融合 kernel | 通过外部库支持 GPTQ、AWQ、FP8 |
| **多 GPU** | 通过 NCCL 实现 TP + PP | 通过 NCCL 实现 TP |
| **部署复杂度** | 较高——需要 build 步骤 | 较低——加载即可对外服务 |
| **模型支持** | 主流 LLM（Llama、Mistral、GPT、Falcon 等） | 通过 HuggingFace 支持更广泛的模型 |
| **硬件** | 仅 NVIDIA | NVIDIA + AMD（ROCm） |
| **适用场景** | NVIDIA 上生产环境的最高吞吐 | 快速原型验证、AMD 支持、灵活性 |

### 需要内化的关键概念

* **Prefill 与 decode 阶段** — prefill（首字前的整段计算）一次处理完整个 prompt（算力受限、算术强度高）。decode（逐 token 生成阶段）每次生成一个 token（带宽受限、算术强度低）。TRT-LLM 用不同的 kernel 策略对两者分别优化。
* **KV-cache 容量估算** — 7B 模型在 FP16、4096 context 下：每条序列约 2 GB KV-cache。使用 paged KV-cache 时，80 GB 的 H100 可服务约 30 条并发序列。理解这套算术很关键。
* **Engine 构建的取舍** — `max_batch_size`、`max_input_len`、`max_seq_len` 在构建时就固化进 engine。越大 = 预留内存越多，可并存的 engine 越少。按实际工作负载确定大小。

---

## 3. 可度量的结果

* **延迟** — p50、p99；要测什么（单请求、批）。对 LLM：首 token 时延（TTFT）和 token 间延迟（ITL）。
* **吞吐** — QPS、tokens/s；批大小与并发如何影响它。对 LLM：所有并发请求合计的输出 tokens/s。
* **内存占用** — GPU/系统内存峰值；批处理与精度的影响。对 LLM：模型权重 + KV-cache + 激活值内存。
* **方法论** — 可复现的 benchmark；A/B 对比（例如 kernel 改动前后，或 TensorRT-LLM vs vLLM）。

---

## 资源

* [TensorRT Best Practices](https://docs.nvidia.com/deeplearning/tensorrt/best-practices/)
* [TensorRT-LLM GitHub](https://github.com/NVIDIA/TensorRT-LLM) — 源码、示例、模型支持矩阵。
* [TensorRT-LLM 文档](https://nvidia.github.io/TensorRT-LLM/) — 构建、部署与优化指南。
* [vLLM](https://github.com/vllm-project/vllm) — 用于对比的另一种 LLM 推理服务引擎。
* [Triton Inference Server](https://github.com/triton-inference-server/server) — 以 TRT-LLM 为后端的生产级推理服务。
* [MLPerf Inference](https://mlcommons.org/benchmarks/inference/) — 参考 benchmark 与方法论。

---

## 项目

1. **Runtime 对比** — 在相同硬件上用 ONNX Runtime 和 TensorRT 部署同一个模型。比较延迟与吞吐；记录配置与测量方法。
2. **Triton server** — 搭建一个带动态批处理的最小 Triton server。测量 QPS 随批大小的变化，并记录批处理如何影响延迟与吞吐。
3. **Benchmark 报告** — 针对一个模型和一个 runtime，产出一页 benchmark 报告：延迟（p50/p99）、吞吐、内存，以及确切环境（GPU、驱动、runtime 版本）。
4. **TensorRT-LLM engine 构建** — 为 Llama-3-8B 构建 FP16 与 INT8 量化的 TensorRT-LLM engine。测量 tokens/s、TTFT 和内存占用。与相同硬件上的 vLLM 对比。
5. **多 GPU LLM 推理服务** — 使用 TensorRT-LLM，以张量并行把一个 70B 模型部署到 2 张以上 GPU 上。测量扩展效率（每 GPU 的 tokens/s），并与单 GPU 跑更小模型对比。
6. **TRT-LLM + Triton** — 启用 in-flight batching，把 TensorRT-LLM engine 部署在 Triton Inference Server 之后。用并发客户端做负载测试，测量负载下的 p50/p99 延迟。

---

## 下一步

→ **[06 — tinygrad 深入剖析](../06-tinygrad深入解析/Guide.md)**（可选）— 动手实践编译器/kernel 接口：IR、调度器、后端，以及添加一个简单优化。


<details>
<summary>English original</summary>

**TensorRT-LLM vs vLLM**

| | TensorRT-LLM | vLLM |
|---|---|---|
| **Approach** | Compile model to optimized engine (ahead-of-time) | JIT with PyTorch + custom CUDA kernels |
| **Performance** | Highest throughput on NVIDIA GPUs (custom Hopper/Blackwell kernels) | Very good; slightly lower peak but faster iteration |
| **Quantization** | FP8, INT8, INT4, FP4 with fused kernels | GPTQ, AWQ, FP8 via external libraries |
| **Multi-GPU** | TP + PP via NCCL | TP via NCCL |
| **Setup complexity** | Higher — build step required | Lower — load and serve |
| **Model support** | Major LLMs (Llama, Mistral, GPT, Falcon, etc.) | Broader model support via HuggingFace |
| **Hardware** | NVIDIA only | NVIDIA + AMD (ROCm) |
| **Best for** | Maximum throughput in production on NVIDIA | Rapid prototyping, AMD support, flexibility |

**Key concepts to internalize**

* **Prefill vs decode phases** — Prefill processes the entire prompt in one pass (compute-bound, high arithmetic intensity). Decode generates one token at a time (memory-bound, low arithmetic intensity). TRT-LLM optimizes both with different kernel strategies.
* **KV-cache sizing** — For a 7B model at FP16 with 4096 context: ~2 GB KV-cache per sequence. With paged KV-cache, 80 GB H100 can serve ~30 concurrent sequences. Understanding this math is essential.
* **Engine build trade-offs** — `max_batch_size`, `max_input_len`, `max_seq_len` are baked into the engine. Larger = more memory reserved, fewer concurrent engines. Size for your actual workload.

---

**3. Measurable outcomes**

* **Latency** — p50, p99; what to measure (single request, batch). For LLMs: time-to-first-token (TTFT) and inter-token latency (ITL).
* **Throughput** — QPS, tokens/s; how batch size and concurrency affect it. For LLMs: output tokens/s across all concurrent requests.
* **Memory footprint** — Peak GPU/system memory; impact of batching and precision. For LLMs: model weights + KV-cache + activation memory.
* **Methodology** — Reproducible benchmarks; A/B comparison (e.g. before/after kernel change, or TensorRT-LLM vs vLLM).

---

**Resources**

* [TensorRT Best Practices](https://docs.nvidia.com/deeplearning/tensorrt/best-practices/)
* [TensorRT-LLM GitHub](https://github.com/NVIDIA/TensorRT-LLM) — Source, examples, model support matrix.
* [TensorRT-LLM Documentation](https://nvidia.github.io/TensorRT-LLM/) — Build, deploy, and optimize guides.
* [vLLM](https://github.com/vllm-project/vllm) — Alternative LLM serving engine for comparison.
* [Triton Inference Server](https://github.com/triton-inference-server/server) — Production serving with TRT-LLM backend.
* [MLPerf Inference](https://mlcommons.org/benchmarks/inference/) — Reference benchmarks and methodology.

---

**Projects**

1. **Runtime comparison** — Deploy the same model with ONNX Runtime and TensorRT (same hardware). Compare latency and throughput; document configuration and measurement method.
2. **Triton server** — Set up a minimal Triton server with dynamic batching. Measure QPS vs batch size and document how batching affects latency and throughput.
3. **Benchmark report** — For one model and one runtime, produce a one-page benchmark report: latency (p50/p99), throughput, memory, and exact environment (GPU, driver, runtime version).
4. **TensorRT-LLM engine build** — Build a TensorRT-LLM engine for Llama-3-8B with FP16 and INT8 quantization. Measure tokens/s, TTFT, and memory usage. Compare with vLLM on the same hardware.
5. **Multi-GPU LLM serving** — Deploy a 70B model across 2+ GPUs with tensor parallelism using TensorRT-LLM. Measure scaling efficiency (tokens/s per GPU) vs single-GPU with a smaller model.
6. **TRT-LLM + Triton** — Deploy TensorRT-LLM engine behind Triton Inference Server with in-flight batching enabled. Load test with concurrent clients and measure p50/p99 latency under load.

---

**Next**

→ **[06 — tinygrad Deep Dive](../06-tinygrad深入解析/Guide.md)** (optional) — Hands-on compiler/kernel interface: IR, scheduler, backends, and adding a simple optimization.

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/05 - Inference Runtimes and Deployment/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/05%20-%20Inference%20Runtimes%20and%20Deployment/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
