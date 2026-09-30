---
title: 性能
description: 性能
---

# 性能

## 硬件规格（Orin Nano Super）

| 规格 | 值 |
|------|-------|
| GPU TOPS (INT8) | 67 |
| DLA（深度学习加速器）TOPS (INT8) | ~10 |
| 总 TOPS | ~77 |
| 内存 | 8 GB LPDDR5 |
| 带宽 | 102 GB/s |
| CUDA 核心 | 1024（16 SM × 64） |
| 张量核心 | 32 |
| 功耗模式 | 7W / 10W / 15W / 25W |
| 拐点 | 0.66 OP/byte |

## 为什么大语言模型 decode（逐 token 生成阶段）受带宽限制

大语言模型自回归 decode 每次生成一个 token。每个 token 都需要从 DRAM 读取**整个**权重矩阵：

```
Llama 3.2 3B INT4 — one decode step:

  Weight read:    1.5 GB from DRAM
  Compute:        6 GFLOP (3B × 2 ops)
  Time to read:   1.5 GB / 102 GB/s = 14.7 ms
  Time to compute: 6 GFLOP / 67 TOPS = 0.09 ms

  → 99.4% of time is reading weights
  → Compute utilization: 0.6%
  → Theoretical max: ~68 tokens/sec (bandwidth-limited)
```

模型越小 = 需读取的字节越少 = tokens/sec 越多。量化直接转化为吞吐。

## 预期性能

### TinyLlama 1.1B（Q4_K_M，669 MB）— 测试模型

| 指标 | 25W 模式 | 15W 模式 |
|--------|----------|----------|
| Prompt 评测（512 tok） | ~200 tok/s | ~120 tok/s |
| Decode（128 tok） | ~65 tok/s | ~40 tok/s |
| 峰值内存 | ~2.5 GB | ~2.5 GB |
| 峰值温度 | <60°C | <55°C |

### Llama 3.2 3B（Q4_K_M，1.8 GB）— 目标模型

| 指标 | 25W 模式 | 15W 模式 |
|--------|----------|----------|
| Prompt 评测（512 tok） | ~65 tok/s | ~40 tok/s |
| Decode（128 tok） | ~25 tok/s | ~15 tok/s |
| 峰值内存 | ~4 GB | ~4 GB |
| 峰值温度 | <70°C | <60°C |

### Phi-4 Mini 3.8B（Q4_K_M，2.3 GB）— 压力测试

| 指标 | 25W 模式 |
|--------|----------|
| Prompt 评测（512 tok） | ~50 tok/s |
| Decode（128 tok） | ~20 tok/s |
| 峰值内存 | ~4.5 GB |

*所有数值均为估计值。实际性能取决于热管理设计、上下文长度和 prompt 内容。*

## 性能剖析

### Nsight Systems — 时间线剖析

```bash
./scripts/profile.sh models/tinyllama.gguf
# Creates: profile_YYYYMMDD_HHMMSS.nsys-rep
```

显示：
- 逐个 kernel 的时间线
- 内存传输时序
- CPU-GPU 同步点
- kernel 启动开销

### 需关注的关键指标

```bash
nsys stats profile.nsys-rep
```

预期输出：
```
Kernel                              Time%    Calls
────────────────────────────────────────────────────
jllm::gemv_q4_kernel               38.2%    312
jllm::flash_attention_decode_kernel 28.1%    156
jllm::fused_rmsnorm_residual_kernel 11.4%    312
jllm::swiglu_kernel                  7.8%    156
jllm::rope_kernel                    4.2%    312
jllm::vec_add_kernel                 3.1%    312
other                                7.2%    ...
```

### Benchmark 脚本

```bash
./scripts/bench.sh models/tinyllama.gguf
```

记录：
- 系统状态（功耗模式、GPU 频率、RAM、温度）
- 短生成（128 tokens）：tok/s
- 长生成（256 tokens）：tok/s
- 推理期间的内存占用（每秒 RSS）
- 推理期间的温度

## 优化目标

### 级别 1：融合 kernel（已完成）

| 操作 | 未融合 | 融合后 | 节省 |
|-----------|---------------|-------------|--------|
| RMSNorm + 残差 | 3 个 kernel，6 次 DRAM 操作 | 1 个 kernel，3 次 DRAM 操作 | 2× |
| SwiGLU | 2 个 kernel，4 次 DRAM 操作 | 1 个 kernel，2 次 DRAM 操作 | 2× |
| 反量化 + GEMV（矩阵-向量乘） | 2 个 kernel，2× 带宽 | 1 个 kernel，1× 带宽 | 3.5× |

### 级别 2：分块大小调优（已完成）

| 参数 | 桌面 GPU（H100） | Orin Nano | 差异原因 |
|-----------|-------------------|-----------|---------------|
| GEMV 线程块 | 256 线程 | 128 线程 | SM 更少，occupancy 压力更小 |
| attention 分块 | 128 KV token | 64 KV token | 48 KB 共享内存（而非 228 KB） |
| GEMM（矩阵-矩阵乘）分块 | 128×128 | 64×64 | 每个 SM 的共享内存更少 |

### 级别 3：CUDA Graphs（已实现）

把 decode 步骤捕获成一张图，用单个 `cudaGraphLaunch()` 重放。
每步可从 kernel 启动开销中省下约 1 ms。

### 级别 4：未来优化

| 优化项 | 预期收益 | 工作量 |
|-------------|---------------|--------|
| 用于 prefill（首字前的整段计算）的 Tensor Core WMMA | prefill 速度提升 2–3× | 2 天 |
| 持久化 kernel（常驻 SM） | decode 提升 10–20% | 3 天 |
| 自定义内存分配器（绕过 CUDA） | 开销减少 5% | 2 天 |
| INT4 KV cache（不仅是 INT8） | 上下文 2× | 1 天 |
| 带草稿模型的投机解码 | 1.5–3× tok/s | 3 天 |

## Roofline（性能上界模型）分析

```
TFLOPS
  │
  │                    ──── 67 TOPS peak (INT8)
  │                 ╱
  │              ╱
  │           ╱
  │        ╱  ← slope = 102 GB/s bandwidth
  │     ╱
  │  ╱
  └───────────────── Arithmetic Intensity (OP/byte)
        ↑
  ridge = 0.66

  LLM decode AI ≈ 0.5 OP/byte → severely left of ridge → bandwidth-bound
  Prefill GEMM AI ≈ 50+ OP/byte → right of ridge → compute-bound
```

所有 decode 优化都必须降低带宽（量化、融合、缓存）。
prefill 优化则应最大化 Tensor Core 利用率。


<details>
<summary>English original</summary>

**Performance**

**Hardware Specs (Orin Nano Super)**

| Spec | Value |
|------|-------|
| GPU TOPS (INT8) | 67 |
| DLA TOPS (INT8) | ~10 |
| Total TOPS | ~77 |
| Memory | 8 GB LPDDR5 |
| Bandwidth | 102 GB/s |
| CUDA cores | 1024 (16 SMs × 64) |
| Tensor Cores | 32 |
| Power modes | 7W / 10W / 15W / 25W |
| Ridge point | 0.66 OP/byte |

**Why LLM Decode is Bandwidth-Bound**

LLM autoregressive decode generates one token at a time. Each token requires reading the **entire** weight matrix from DRAM:

```
Llama 3.2 3B INT4 — one decode step:

  Weight read:    1.5 GB from DRAM
  Compute:        6 GFLOP (3B × 2 ops)
  Time to read:   1.5 GB / 102 GB/s = 14.7 ms
  Time to compute: 6 GFLOP / 67 TOPS = 0.09 ms

  → 99.4% of time is reading weights
  → Compute utilization: 0.6%
  → Theoretical max: ~68 tokens/sec (bandwidth-limited)
```

Smaller model = fewer bytes to read = more tokens/sec. Quantization directly translates to throughput.

**Expected Performance**

**TinyLlama 1.1B (Q4_K_M, 669 MB) — Test Model**

| Metric | 25W mode | 15W mode |
|--------|----------|----------|
| Prompt eval (512 tok) | ~200 tok/s | ~120 tok/s |
| Decode (128 tok) | ~65 tok/s | ~40 tok/s |
| Peak memory | ~2.5 GB | ~2.5 GB |
| Peak temperature | <60°C | <55°C |

**Llama 3.2 3B (Q4_K_M, 1.8 GB) — Target Model**

| Metric | 25W mode | 15W mode |
|--------|----------|----------|
| Prompt eval (512 tok) | ~65 tok/s | ~40 tok/s |
| Decode (128 tok) | ~25 tok/s | ~15 tok/s |
| Peak memory | ~4 GB | ~4 GB |
| Peak temperature | <70°C | <60°C |

**Phi-4 Mini 3.8B (Q4_K_M, 2.3 GB) — Stress Test**

| Metric | 25W mode |
|--------|----------|
| Prompt eval (512 tok) | ~50 tok/s |
| Decode (128 tok) | ~20 tok/s |
| Peak memory | ~4.5 GB |

*All values are estimates. Actual performance depends on thermal design, context length, and prompt content.*

**Profiling**

**Nsight Systems — Timeline Profile**

```bash
./scripts/profile.sh models/tinyllama.gguf
# Creates: profile_YYYYMMDD_HHMMSS.nsys-rep
```

Shows:
- Kernel-by-kernel timeline
- Memory transfer timing
- CPU-GPU synchronization points
- Kernel launch overhead

**Key Metrics to Watch**

```bash
nsys stats profile.nsys-rep
```

Expected output:
```
Kernel                              Time%    Calls
────────────────────────────────────────────────────
jllm::gemv_q4_kernel               38.2%    312
jllm::flash_attention_decode_kernel 28.1%    156
jllm::fused_rmsnorm_residual_kernel 11.4%    312
jllm::swiglu_kernel                  7.8%    156
jllm::rope_kernel                    4.2%    312
jllm::vec_add_kernel                 3.1%    312
other                                7.2%    ...
```

**Benchmark Script**

```bash
./scripts/bench.sh models/tinyllama.gguf
```

Records:
- System state (power mode, GPU freq, RAM, temperature)
- Short generation (128 tokens): tok/s
- Long generation (256 tokens): tok/s
- Memory profile during inference (RSS every second)
- Temperature during inference

**Optimization Targets**

**Level 1: Fused Kernels (Done)**

| Operation | Without fusion | With fusion | Saving |
|-----------|---------------|-------------|--------|
| RMSNorm + residual | 3 kernels, 6 DRAM ops | 1 kernel, 3 DRAM ops | 2× |
| SwiGLU | 2 kernels, 4 DRAM ops | 1 kernel, 2 DRAM ops | 2× |
| Dequant + GEMV | 2 kernels, 2× bandwidth | 1 kernel, 1× bandwidth | 3.5× |

**Level 2: Tile Size Tuning (Done)**

| Parameter | Desktop GPU (H100) | Orin Nano | Why different |
|-----------|-------------------|-----------|---------------|
| GEMV block | 256 threads | 128 threads | Fewer SMs, less occupancy pressure |
| Attention tile | 128 KV tokens | 64 KV tokens | 48 KB shared (not 228 KB) |
| GEMM tile | 128×128 | 64×64 | Less shared memory per SM |

**Level 3: CUDA Graphs (Implemented)**

Captures decode step as a graph, replays with single `cudaGraphLaunch()`.
Saves ~1 ms per step from kernel launch overhead.

**Level 4: Future Optimizations**

| Optimization | Expected gain | Effort |
|-------------|---------------|--------|
| Tensor Core WMMA for prefill | 2–3× prefill speed | 2 days |
| Persistent kernels (stay resident on SM) | 10–20% decode | 3 days |
| Custom memory allocator (bypass CUDA) | 5% less overhead | 2 days |
| INT4 KV cache (not just INT8) | 2× more context | 1 day |
| Speculative decoding with draft model | 1.5–3× tok/s | 3 days |

**Roofline Analysis**

```
TFLOPS
  │
  │                    ──── 67 TOPS peak (INT8)
  │                 ╱
  │              ╱
  │           ╱
  │        ╱  ← slope = 102 GB/s bandwidth
  │     ╱
  │  ╱
  └───────────────── Arithmetic Intensity (OP/byte)
        ↑
  ridge = 0.66

  LLM decode AI ≈ 0.5 OP/byte → severely left of ridge → bandwidth-bound
  Prefill GEMM AI ≈ 50+ OP/byte → right of ridge → compute-bound
```

All decode optimizations must reduce bandwidth (quantization, fusion, caching).
Prefill optimizations should maximize Tensor Core utilization.

</details>

## 能效

| 模型 | Tokens/sec | 功耗 (W) | Tokens/Joule |
|-------|-----------|-----------|-------------|
| TinyLlama 1.1B @ 25W | ~65 | 25 | 2.6 |
| TinyLlama 1.1B @ 7W | ~20 | 7 | 2.9 |
| Llama 3.2 3B @ 25W | ~25 | 25 | 1.0 |
| Llama 3.2 3B @ 7W | ~8 | 7 | 1.1 |

*7W 模式能效更高（tokens/joule），尽管速度更慢。*


<details>
<summary>English original</summary>

**Power Efficiency**

| Model | Tokens/sec | Power (W) | Tokens/Joule |
|-------|-----------|-----------|-------------|
| TinyLlama 1.1B @ 25W | ~65 | 25 | 2.6 |
| TinyLlama 1.1B @ 7W | ~20 | 7 | 2.9 |
| Llama 3.2 3B @ 25W | ~25 | 25 | 1.0 |
| Llama 3.2 3B @ 7W | ~8 | 7 | 1.1 |

*7W mode is more power-efficient (tokens/joule) despite being slower.*

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/performance.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/performance.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
