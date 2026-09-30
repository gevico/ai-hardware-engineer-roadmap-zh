---
title: 架构
description: 架构
---

# 架构

## 概览

jetson-llm 是一个内存优先的 LLM 推理 runtime，专为目标 NVIDIA Jetson Orin 打造。它从 GGUF 格式文件运行自回归 Transformer 模型（Llama、Phi、Gemma、Qwen 等）。

```
User prompt (text)
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│                        CLI / HTTP Server                     │
│  main.cpp: parse args, probe system, pre-check memory       │
│  http_server.cpp: /v1/chat/completions, /health, /v1/models │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                          Engine                              │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐  │
│  │ Tokenizer│  │  Prefill │  │  Decode  │  │  Sampling  │  │
│  │ encode() │→ │ N layers │→ │ 1 token  │→ │ top-k/p    │  │
│  │ decode() │  │ per tok  │  │ per step │  │ temperature│  │
│  └──────────┘  └──────────┘  └──────────┘  └────────────┘  │
│                       │              │                        │
│              ┌────────┴──────────────┘                        │
│              ▼                                                │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │            transformer_layer() × N_layers               │ │
│  │                                                         │ │
│  │  RMSNorm → Q/K/V proj → RoPE → KV store → Attention    │ │
│  │  → Output proj → Residual → RMSNorm → Gate/Up proj     │ │
│  │  → SwiGLU → Down proj → Residual                       │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                     CUDA Kernels (SM 8.7)                    │
│                                                              │
│  gemv_q4        INT4 dequant-fused GEMV (38% of decode)     │
│  attention      Flash attention decode, online softmax       │
│  fused_norm     RMSNorm + residual add (2× less traffic)    │
│  rope           Rotary position embedding (fused Q+K)        │
│  convert        FP16↔INT8 + SwiGLU activation               │
│  softmax        Numerically stable logit softmax             │
│  vec_add        Residual connection add                      │
│  fp16_to_fp32   Logit conversion for sampling                │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                     Memory Manager                           │
│                                                              │
│  MemoryBudget   Track every MB (OS, CMA, CUDA, model, KV)   │
│  OOMGuard       Check /proc/meminfo before every KV extend   │
│  KVCachePool    Pinned fast pool + unpinned overflow          │
│  ScratchPool    Bump allocator, zero malloc during inference  │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                      Jetson HAL                              │
│                                                              │
│  PowerState     nvpmodel query (7W/10W/15W/25W)              │
│  ThermalState   Thermal zones, adaptive backoff              │
│  JetsonInfo     One-time system probe (L4T, CUDA, SMs, RAM) │
│  LiveStats      tegrastats-style real-time metrics           │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│              Jetson Orin Nano Super Hardware                  │
│                                                              │
│  1024 CUDA cores (16 SMs × 64)  │  102 GB/s LPDDR5         │
│  32 Tensor Cores                  │  8 GB unified memory     │
│  67 TOPS INT8 (GPU)              │  7–25W power modes       │
│  ~10 TOPS INT8 (DLA)             │  SM 8.7 (Ampere)         │
└─────────────────────────────────────────────────────────────┘
```

## 设计原则

### 1. 内存优先

在 Jetson 上，CPU 与 GPU 共享同一块 8 GB LPDDR5。模型占用的每一个字节，都是 KV cache、OS 或相机不可用的字节。runtime 追踪每一笔分配。

- `MemoryBudget` 在推理开始前就知道每一个 MB 的去向
- 上下文长度由模型加载后的剩余内存自动计算
- `OOMGuard` 在每次 KV cache 扩展前检查真实的 `/proc/meminfo`
- 内存压力下生成会优雅停止（不崩溃，不触发 OOM killer）


<details>
<summary>English original</summary>

**Architecture**

**Overview**

jetson-llm is a memory-first LLM inference runtime targeting NVIDIA Jetson Orin exclusively. It runs autoregressive transformer models (Llama, Phi, Gemma, Qwen, etc.) from GGUF format files.

```
User prompt (text)
      │
      ▼
┌─────────────────────────────────────────────────────────────┐
│                        CLI / HTTP Server                     │
│  main.cpp: parse args, probe system, pre-check memory       │
│  http_server.cpp: /v1/chat/completions, /health, /v1/models │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                          Engine                              │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐  │
│  │ Tokenizer│  │  Prefill │  │  Decode  │  │  Sampling  │  │
│  │ encode() │→ │ N layers │→ │ 1 token  │→ │ top-k/p    │  │
│  │ decode() │  │ per tok  │  │ per step │  │ temperature│  │
│  └──────────┘  └──────────┘  └──────────┘  └────────────┘  │
│                       │              │                        │
│              ┌────────┴──────────────┘                        │
│              ▼                                                │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │            transformer_layer() × N_layers               │ │
│  │                                                         │ │
│  │  RMSNorm → Q/K/V proj → RoPE → KV store → Attention    │ │
│  │  → Output proj → Residual → RMSNorm → Gate/Up proj     │ │
│  │  → SwiGLU → Down proj → Residual                       │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                     CUDA Kernels (SM 8.7)                    │
│                                                              │
│  gemv_q4        INT4 dequant-fused GEMV (38% of decode)     │
│  attention      Flash attention decode, online softmax       │
│  fused_norm     RMSNorm + residual add (2× less traffic)    │
│  rope           Rotary position embedding (fused Q+K)        │
│  convert        FP16↔INT8 + SwiGLU activation               │
│  softmax        Numerically stable logit softmax             │
│  vec_add        Residual connection add                      │
│  fp16_to_fp32   Logit conversion for sampling                │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                     Memory Manager                           │
│                                                              │
│  MemoryBudget   Track every MB (OS, CMA, CUDA, model, KV)   │
│  OOMGuard       Check /proc/meminfo before every KV extend   │
│  KVCachePool    Pinned fast pool + unpinned overflow          │
│  ScratchPool    Bump allocator, zero malloc during inference  │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                      Jetson HAL                              │
│                                                              │
│  PowerState     nvpmodel query (7W/10W/15W/25W)              │
│  ThermalState   Thermal zones, adaptive backoff              │
│  JetsonInfo     One-time system probe (L4T, CUDA, SMs, RAM) │
│  LiveStats      tegrastats-style real-time metrics           │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│              Jetson Orin Nano Super Hardware                  │
│                                                              │
│  1024 CUDA cores (16 SMs × 64)  │  102 GB/s LPDDR5         │
│  32 Tensor Cores                  │  8 GB unified memory     │
│  67 TOPS INT8 (GPU)              │  7–25W power modes       │
│  ~10 TOPS INT8 (DLA)             │  SM 8.7 (Ampere)         │
└─────────────────────────────────────────────────────────────┘
```

**Design Principles**

**1. Memory-First**

On Jetson, CPU and GPU share the same 8 GB LPDDR5. Every byte for the model is a byte not available for KV cache, OS, or camera. The runtime tracks every allocation.

- `MemoryBudget` knows where every MB goes before inference starts
- Context length is auto-calculated from remaining memory after model load
- `OOMGuard` checks real `/proc/meminfo` before every KV cache extension
- Generation stops gracefully on memory pressure (no crash, no OOM killer)

</details>

### 2. 仅限 Orin

没有针对 x86、桌面 GPU 或多 GPU 的代码路径。所有常量、分块大小和线程块都针对 SM 8.7、48 KB 共享内存和 16 个 SM 调优。

### 3. 推理期间零分配

所有内存都在加载时预分配：
- 模型权重：mmap'd + pinned（一次分配）
- KV cache：预分配池（一次分配）
- Scratch 缓冲区：bump allocator，后备内存预分配（一次分配）
- decode 循环期间没有 `malloc`、`new`、`cudaMalloc`

### 4. 统一内存的利用

Jetson 的 CPU 和 GPU 共享同一 DRAM。runtime 利用了这一点：
- 模型权重：`mmap` + `cudaHostRegister` → GPU 直接读取文件
- KV cache：`cudaMallocHost` → CPU 和 GPU 均可访问，无需拷贝
- KV 溢出：普通 `malloc` → GPU 通过页错误读取（较慢但可用）
- CPU 与 GPU 之间共享数据不使用 `cudaMemcpy`

### 5. 功耗/热管理感知

runtime 会适应 Jetson 的功耗约束：
- 启动时读取 `nvpmodel` 状态
- 生成过程中监控热区
- 在硬件热降频触发之前退让（插入延迟）
- 在 health 端点中上报功耗模式、温度和利用率

## 数据流 — 一个 token

```
1. Engine receives last generated token ID
2. Embedding lookup: tok_embd[token_id] → hidden state x (cudaMemcpy from mmap)
3. For each layer (0 to N-1):
   a. RMSNorm(x) → normed
   b. Q = gemv_q4(W_q, normed)
   c. K = gemv_q4(W_k, normed)
   d. V = gemv_q4(W_v, normed)
   e. RoPE(Q, K, position)
   f. Store K,V into KV cache pool (INT8 quantized if enabled)
   g. attn_out = flash_attention(Q, K_cache, V_cache)
   h. attn_proj = gemv_q4(W_o, attn_out)
   i. x2 = x + attn_proj                    ← first residual
   j. normed2 = RMSNorm(x2)
   k. gate = gemv_q4(W_gate, normed2)
   l. up = gemv_q4(W_up, normed2)
   m. swiglu_out = silu(gate) × up
   n. ffn_out = gemv_q4(W_down, swiglu_out)
   o. x = x2 + ffn_out                      ← second residual
4. Final RMSNorm(x) → normed
5. logits = gemv_q4(W_output, normed)        → FP16
6. fp16_to_fp32(logits)                      → FP32
7. cudaMemcpy logits to CPU
8. sample_token(logits, top_k, top_p, temperature)
9. Return token ID, call streaming callback
```

## 文件组织

```
include/
  jllm.h            Master header + Orin constants
  jllm_memory.h     MemoryBudget, OOMGuard, KVCachePool, ScratchPool
  jllm_jetson.h     PowerState, ThermalState, LiveStats, JetsonInfo
  jllm_kernels.h    Kernel API declarations + SM 8.7 tile/block constants
  jllm_engine.h     ModelConfig, LayerWeights, ModelWeights, Tokenizer, Engine

src/memory/         Memory management (budget, KV cache, scratch pool)
src/jetson/         Hardware abstraction (power, thermal, sysinfo)
src/kernels/        CUDA kernels (all .cu files, SM 8.7 only)
src/engine/         Model loading, forward pass, sampling, tokenizer
src/server/         HTTP API server
src/main.cpp        CLI entry point
src/main_server.cpp Server entry point

scripts/            Setup, benchmarking, profiling
tests/              Unit and integration tests
docs/               This documentation
```


<details>
<summary>English original</summary>

**2. Orin-Only**

No code paths for x86, desktop GPUs, or multi-GPU. Every constant, tile size, and thread block is tuned for SM 8.7 with 48 KB shared memory and 16 SMs.

**3. Zero Allocation During Inference**

All memory is pre-allocated at load time:
- Model weights: mmap'd + pinned (one allocation)
- KV cache: pre-allocated pool (one allocation)
- Scratch buffers: bump allocator with pre-allocated backing (one allocation)
- No `malloc`, `new`, `cudaMalloc` during the decode loop

**4. Unified Memory Exploitation**

Jetson's CPU and GPU share the same DRAM. The runtime exploits this:
- Model weights: `mmap` + `cudaHostRegister` → GPU reads file directly
- KV cache: `cudaMallocHost` → both CPU and GPU access without copy
- KV overflow: regular `malloc` → GPU reads via page faults (slower but works)
- No `cudaMemcpy` for data sharing between CPU and GPU

**5. Power/Thermal Awareness**

The runtime adapts to Jetson's power constraints:
- Reads `nvpmodel` state at startup
- Monitors thermal zones during generation
- Backs off (inserts delays) before hardware thermal throttling triggers
- Reports power mode, temperature, and utilization in health endpoint

**Data Flow — One Token**

```
1. Engine receives last generated token ID
2. Embedding lookup: tok_embd[token_id] → hidden state x (cudaMemcpy from mmap)
3. For each layer (0 to N-1):
   a. RMSNorm(x) → normed
   b. Q = gemv_q4(W_q, normed)
   c. K = gemv_q4(W_k, normed)
   d. V = gemv_q4(W_v, normed)
   e. RoPE(Q, K, position)
   f. Store K,V into KV cache pool (INT8 quantized if enabled)
   g. attn_out = flash_attention(Q, K_cache, V_cache)
   h. attn_proj = gemv_q4(W_o, attn_out)
   i. x2 = x + attn_proj                    ← first residual
   j. normed2 = RMSNorm(x2)
   k. gate = gemv_q4(W_gate, normed2)
   l. up = gemv_q4(W_up, normed2)
   m. swiglu_out = silu(gate) × up
   n. ffn_out = gemv_q4(W_down, swiglu_out)
   o. x = x2 + ffn_out                      ← second residual
4. Final RMSNorm(x) → normed
5. logits = gemv_q4(W_output, normed)        → FP16
6. fp16_to_fp32(logits)                      → FP32
7. cudaMemcpy logits to CPU
8. sample_token(logits, top_k, top_p, temperature)
9. Return token ID, call streaming callback
```

**File Organization**

```
include/
  jllm.h            Master header + Orin constants
  jllm_memory.h     MemoryBudget, OOMGuard, KVCachePool, ScratchPool
  jllm_jetson.h     PowerState, ThermalState, LiveStats, JetsonInfo
  jllm_kernels.h    Kernel API declarations + SM 8.7 tile/block constants
  jllm_engine.h     ModelConfig, LayerWeights, ModelWeights, Tokenizer, Engine

src/memory/         Memory management (budget, KV cache, scratch pool)
src/jetson/         Hardware abstraction (power, thermal, sysinfo)
src/kernels/        CUDA kernels (all .cu files, SM 8.7 only)
src/engine/         Model loading, forward pass, sampling, tokenizer
src/server/         HTTP API server
src/main.cpp        CLI entry point
src/main_server.cpp Server entry point

scripts/            Setup, benchmarking, profiling
tests/              Unit and integration tests
docs/               This documentation
```

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/architecture.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/architecture.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
