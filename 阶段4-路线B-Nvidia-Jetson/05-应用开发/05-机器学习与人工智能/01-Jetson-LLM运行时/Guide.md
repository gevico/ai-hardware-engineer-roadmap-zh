---
title: Jetson LLM Runtime — 内存优先推理引擎
description: Jetson LLM Runtime — 内存优先推理引擎
---

# Jetson LLM Runtime — 内存优先推理引擎

<div class="course-identity auto-course" style="--course-accent: #65a30d; --course-accent-rgb: 101, 163, 13;" markdown="1">
<div class="course-identity__icon">JLRM</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · Jetson 专题</p>
<p class="course-identity__title">面向 Jetson LLM Runtime — 内存优先推理引擎的专属课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 测量：延迟、内存、功耗、日志</p>
</div>
</div>


**父级：** [机器学习与 AI](../Guide.md)

> **构建一个 Jetson 原生的 LLM runtime，把内存当作首要约束。** 它不是 llama.cpp 的 fork，而是一个从头设计的引擎：面向 8 GB 统一内存，配备针对 Orin 调优的 CUDA kernel、功耗感知推理，以及零分配的 decode（逐 token 生成阶段）。

**源代码：** [`Projects/jetson-llm-runtime/README.md`](../../../../项目/01-Jetson-LLM运行时/README.md)
**测试指南：** [`TESTING.md`](../../../../项目/01-Jetson-LLM运行时/TESTING.md)

---

## 为什么需要它

| Runtime | 擅长 | 在 Jetson 8 GB 上的短板 |
|---------|---------|-------------------|
| **llama.cpp** | 可移植、易用 | kernel 通用，无内存感知，未集成功耗 |
| **TensorRT-LLM** | 速度极致 | 构建流水线繁重，为数据中心设计 |
| **Ollama** | 一条命令的体验 | 封装 llama.cpp，增加开销 |
| **jetson-llm** | 内存优先、Orin 原生 | 需要你自己构建与维护 |

空白在于：**现有 runtime 都没有围绕 8 GB 统一内存这一约束来设计。**

---

## 架构

```
┌──────────────────────────────────────────────────────┐
│                   jetson-llm                          │
│                                                       │
│  Serving Layer                                        │
│    POST /v1/chat/completions | GET /health            │
│                                                       │
│  Engine                                               │
│    GGUF load → tokenize → prefill → decode → sample  │
│    transformer_layer() × N per token                  │
│    OOM guard + thermal check per token                │
│                                                       │
│  CUDA Kernels (SM 8.7 only)                           │
│    gemv_q4 | fused_rmsnorm | flash_attn | rope        │
│    swiglu | softmax | fp16↔int8                       │
│                                                       │
│  Memory Manager                                       │
│    MemoryBudget | OOMGuard | KVCachePool | ScratchPool│
│                                                       │
│  Jetson HAL                                           │
│    PowerState | ThermalState | LiveStats               │
└──────────────────────────────────────────────────────┘
```

---

## 实现状态

**30 个文件、4,200+ 行代码。所有组件均已实现。**

| layer | 组件 | 状态 |
|-------|-----------|--------|
| **内存** | 预算跟踪器、OOM 防护、分级 KV cache（pinned + overflow）、scratch bump 分配器 | ✅ 已实现 + 已测试 |
| **Jetson HAL** | 功耗模式读取、热区 + 自适应退避、系统探测、实时统计 | ✅ 已实现 + 已测试 |
| **CUDA kernel** | gemv_q4（INT4 反量化融合）、fused_rmsnorm、flash_attention_decode、rope、softmax、fp16↔int8、swiglu | ✅ 已实现 + 5 项正确性测试 |
| **引擎** | GGUF 配置解析器、tensor 信息解析器、权重映射、tokenizer（GGUF 词表）、Transformer 前向传播（每 layer 12 个算子）、采样（top-k/top-p/temp） | ✅ 已实现 |
| **CLI** | 交互式对话、单次 prompt、verbose 模式、OOM 预检查 | ✅ 已实现 |
| **服务端** | 兼容 OpenAI 的 /v1/chat/completions、/health、/v1/models | ✅ 已实现 |
| **脚本** | setup_jetson.sh、bench.sh、profile.sh | ✅ 就绪 |
| **测试** | test_memory（3）、test_kernels（5）、test_model_load（8） | ✅ 就绪 |

### 所有 Bug 已修复（✅）

代码库中 8 个已知 bug 全部修复：

| # | Bug | 修复 |
|---|-----|-----|
| 1 | GGUF offset 计算错误 | 通过 `gguf_scalar_size()` helper 获取精确类型大小 |
| 2 | 残差未串联 | 在 attention 与 FFN 之间加入 `vec_add()` kernel |
| 3 | memcpy 方向错误 | `cudaMemcpyDefault`（统一内存与独立内存均适用） |
| 4 | 缺少 include | 添加 `#include <sys/mman.h>` |
| 5 | CUDA graph 为空 | 完整捕获 graph，涵盖所有 Transformer layer |
| 6 | attention 累加器出错 | 在 shared memory 中按维度 `s_out[head_dim]` |
| 7 | 缺少 FP32 logits | 添加 `fp16_to_fp32()` GPU 转换 kernel |
| 8 | tokenizer 慢 O(V×L) | 哈希表 `token_to_id_` + 最长匹配优先 |

**代码已就绪，可在 Jetson 硬件上构建与测试。**

---


<details>
<summary>English original</summary>

**Jetson LLM Runtime — Memory-First Inference Engine**

<div class="course-identity auto-course" style="--course-accent: #65a30d; --course-accent-rgb: 101, 163, 13;" markdown="1">
<div class="course-identity__icon">JLRM</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Jetson LLM Runtime — Memory-First Inference Engine.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


**Parent:** [ML and AI](../Guide.md)

> **Build a Jetson-native LLM runtime that treats memory as the primary constraint.** Not a fork of llama.cpp — a ground-up engine designed for 8 GB unified memory, with Orin-tuned CUDA kernels, power-aware inference, and zero-allocation decode.

**Source code:** [`Projects/jetson-llm-runtime/README.md`](../../../../项目/01-Jetson-LLM运行时/README.md)
**Testing guide:** [`TESTING.md`](../../../../项目/01-Jetson-LLM运行时/TESTING.md)

---

**Why This Exists**

| Runtime | Good at | Bad on Jetson 8 GB |
|---------|---------|-------------------|
| **llama.cpp** | Portable, easy | Generic kernels, no memory awareness, no power integration |
| **TensorRT-LLM** | Maximum speed | Heavy build pipeline, designed for datacenter |
| **Ollama** | One-command UX | Wraps llama.cpp, adds overhead |
| **jetson-llm** | Memory-first, Orin-native | You build and maintain it |

The gap: **no existing runtime is designed around the 8 GB unified memory constraint.**

---

**Architecture**

```
┌──────────────────────────────────────────────────────┐
│                   jetson-llm                          │
│                                                       │
│  Serving Layer                                        │
│    POST /v1/chat/completions | GET /health            │
│                                                       │
│  Engine                                               │
│    GGUF load → tokenize → prefill → decode → sample  │
│    transformer_layer() × N per token                  │
│    OOM guard + thermal check per token                │
│                                                       │
│  CUDA Kernels (SM 8.7 only)                           │
│    gemv_q4 | fused_rmsnorm | flash_attn | rope        │
│    swiglu | softmax | fp16↔int8                       │
│                                                       │
│  Memory Manager                                       │
│    MemoryBudget | OOMGuard | KVCachePool | ScratchPool│
│                                                       │
│  Jetson HAL                                           │
│    PowerState | ThermalState | LiveStats               │
└──────────────────────────────────────────────────────┘
```

---

**Implementation Status**

**4,200+ lines across 30 files. All components implemented.**

| Layer | Components | Status |
|-------|-----------|--------|
| **Memory** | Budget tracker, OOM guard, tiered KV cache (pinned + overflow), scratch bump allocator | ✅ Implemented + tested |
| **Jetson HAL** | Power mode reader, thermal zones + adaptive backoff, system probe, live stats | ✅ Implemented + tested |
| **CUDA Kernels** | gemv_q4 (INT4 dequant-fused), fused_rmsnorm, flash_attention_decode, rope, softmax, fp16↔int8, swiglu | ✅ Implemented + 5 correctness tests |
| **Engine** | GGUF config parser, tensor info parser, weight mapping, tokenizer (GGUF vocab), transformer forward pass (12 ops/layer), sampling (top-k/top-p/temp) | ✅ Implemented |
| **CLI** | Interactive chat, single prompt, verbose mode, OOM pre-check | ✅ Implemented |
| **Server** | OpenAI-compatible /v1/chat/completions, /health, /v1/models | ✅ Implemented |
| **Scripts** | setup_jetson.sh, bench.sh, profile.sh | ✅ Ready |
| **Tests** | test_memory (3), test_kernels (5), test_model_load (8) | ✅ Ready |

**All Bugs Fixed (✅)**

All 8 known bugs have been fixed in the codebase:

| # | Bug | Fix |
|---|-----|-----|
| 1 | GGUF offset miscalculation | Exact type sizes via `gguf_scalar_size()` helper |
| 2 | Residual not chained | Added `vec_add()` kernel between attention and FFN |
| 3 | Wrong memcpy direction | `cudaMemcpyDefault` (works for unified + discrete) |
| 4 | Missing include | Added `#include <sys/mman.h>` |
| 5 | Empty CUDA graph | Full graph capture with all transformer layers |
| 6 | Broken attention accumulator | Per-dimension `s_out[head_dim]` in shared memory |
| 7 | No FP32 logits | Added `fp16_to_fp32()` GPU conversion kernel |
| 8 | Slow tokenizer O(V×L) | Hash map `token_to_id_` + longest-match-first |

**Code is ready to build and test on Jetson hardware.**

---

</details>

## 里程碑路线图

```
v0.1 — First Tokens
  ✅ All 8 bugs fixed
  ○ Build on Jetson, test_model_load passes
  ○ Generate coherent text

v0.2 — Benchmark Baseline
  ○ bench.sh produces tok/s numbers
  ○ Compare against stock llama.cpp
  ○ Fix tokenizer performance (#8)

v0.3 — Performance Target
  ○ >20% faster than llama.cpp on decode
  ○ CUDA graph for decode loop
  ○ Streaming SSE in server

v0.4 — Production Ready
  ○ 24-hour stability test
  ○ Chat template support
  ○ systemd auto-start
  ○ Documented performance table across models
```

---

## 设计原则

### 1. 内存优先

每项决策都围绕 8 GB 统一内存预算做优化：

- **MemoryBudget** 追踪每一 MB 的去向（OS、CMA、CUDA、模型、KV、scratch）
- **OOMGuard** 在每次扩展 KV cache 前检查 `/proc/meminfo`
- **KVCachePool** 使用 pinned memory（快），溢出部分用 unpinned（在统一内存上仍是零拷贝）
- **ScratchPool** bump 分配器——推理期间零 `malloc`/`free`
- 上下文长度在模型加载后根据剩余内存自动计算

### 2. 仅限 Orin

不为 x86、独立 GPU 或桌面硬件保留代码路径：

- `CMAKE_CUDA_ARCHITECTURES="87"`——仅 SM 8.7
- 分块尺寸针对 48 KB 共享内存调优（而非 H100 的 164 KB）
- block 大小 128 线程（4 个 warp——在 16 个 SM 上有良好的 occupancy）
- INT4 反量化融合进 GEMV（带宽比 FP16 少 3.5×）
- sysfs 路径为 Jetson 专有（`/sys/devices/17000000.ga10b/`）

### 3. 功耗/热管理感知

- 读取 nvpmodel 功耗状态（7W / 10W / 15W / 25W）
- 热管理监控与自适应退避（80°C → 85°C → 90°C → 95°C）
- 出现 OOM 风险或极端高温时优雅停止生成

### 4. 统一内存上的零拷贝

Jetson 的 CPU 与 GPU 共享同一 DRAM——充分利用这一点：

- 模型权重：mmap + `cudaHostRegister`（GPU 直接读取 mmap 的文件）
- KV cache：`cudaMallocHost`（CPU 与 GPU 均可访问，无需拷贝）
- 旧 KV 条目的“CPU offload”只是分配类型的改变，而非物理拷贝

---

## 关键文件

| 文件 | 用途 |
|------|---------|
| `include/jllm.h` | Orin 常量（16 个 SM、48 KB 共享内存、102 GB/s、0.66 ridge point） |
| `include/jllm_memory.h` | MemoryBudget、OOMGuard、KVCachePool、ScratchPool |
| `include/jllm_kernels.h` | kernel API，带 Orin 最优分块/block 尺寸 |
| `src/kernels/gemv_q4.cu` | INT4 反量化融合 GEMV——占 decode（逐 token 生成阶段）时间的 38% |
| `src/kernels/attention.cu` | Flash attention，带 online softmax、GQA、INT8 KV |
| `src/engine/decode.cpp` | Transformer 前向传播（每 layer 12 个算子）+ 生成循环 |
| `src/engine/model.cpp` | GGUF 解析器 + mmap + 张量名→指针映射 |
| `src/jetson/thermal.cpp` | 热管理 zone 读取器 + 自适应退避调度 |

---

## 如何测试

从 **TinyLlama 1.1B Q4_K_M**（669 MB）开始——体积小、速度快、架构与目标一致：

```bash
# Build
./scripts/setup_jetson.sh

# Test without model
./build/test_memory
./build/test_kernels

# Test with model
./build/test_model_load model.gguf
./build/jetson-llm -m model.gguf -p "What is 2+2?" -n 32

# Benchmark
./scripts/bench.sh model.gguf
```

完整测试指南：[`TESTING.md`](../../../../项目/01-Jetson-LLM运行时/TESTING.md)

---

## 资源

| 资源 | 用途 |
|----------|----------|
| [源代码](../../../../项目/01-Jetson-LLM运行时/README.md) | 实际实现 |
| [TESTING.md](../../../../项目/01-Jetson-LLM运行时/TESTING.md) | 10 步测试指南 |
| [Orin Nano 内存架构](../../../01-Nvidia-Jetson平台/07-Orin-Nano内存架构/Guide.md) | 统一内存深入剖析 |
| [Jetson 上的 LLM 优化](../02-Jetson-LLM优化/Guide.md) | 量化、模型选择、FlashAttention |
| [llama.cpp](https://github.com/ggerganov/llama.cpp) | GGUF runtime 参考实现 |
| [GGML CUDA kernels](https://github.com/ggerganov/llama.cpp/tree/master/ggml/src/ggml-cuda) | kernel 参考实现 |


<details>
<summary>English original</summary>

**Milestone Roadmap**

```
v0.1 — First Tokens
  ✅ All 8 bugs fixed
  ○ Build on Jetson, test_model_load passes
  ○ Generate coherent text

v0.2 — Benchmark Baseline
  ○ bench.sh produces tok/s numbers
  ○ Compare against stock llama.cpp
  ○ Fix tokenizer performance (#8)

v0.3 — Performance Target
  ○ >20% faster than llama.cpp on decode
  ○ CUDA graph for decode loop
  ○ Streaming SSE in server

v0.4 — Production Ready
  ○ 24-hour stability test
  ○ Chat template support
  ○ systemd auto-start
  ○ Documented performance table across models
```

---

**Design Principles**

**1. Memory-First**

Every decision optimizes for the 8 GB unified memory budget:

- **MemoryBudget** tracks where every MB goes (OS, CMA, CUDA, model, KV, scratch)
- **OOMGuard** checks `/proc/meminfo` before every KV cache extension
- **KVCachePool** uses pinned memory (fast) with unpinned overflow (still zero-copy on unified mem)
- **ScratchPool** bump allocator — zero `malloc`/`free` during inference
- Context length auto-calculated from remaining memory after model load

**2. Orin-Only**

No code paths for x86, discrete GPUs, or desktop hardware:

- `CMAKE_CUDA_ARCHITECTURES="87"` — SM 8.7 only
- Tile sizes tuned for 48 KB shared memory (not 164 KB like H100)
- Block size 128 threads (4 warps — good occupancy on 16 SMs)
- INT4 dequant fused into GEMV (3.5× less bandwidth than FP16)
- sysfs paths are Jetson-specific (`/sys/devices/17000000.ga10b/`)

**3. Power/Thermal Aware**

- Reads nvpmodel power state (7W / 10W / 15W / 25W)
- Thermal monitoring with adaptive backoff (80°C → 85°C → 90°C → 95°C)
- Generation stops gracefully on OOM risk or extreme heat

**4. Zero-Copy on Unified Memory**

Jetson's CPU and GPU share the same DRAM — exploit this:

- Model weights: mmap + `cudaHostRegister` (GPU reads mmap'd file directly)
- KV cache: `cudaMallocHost` (both CPU and GPU access without copy)
- "CPU offload" of old KV entries is just an allocation type change, not a physical copy

---

**Key Files**

| File | Purpose |
|------|---------|
| `include/jllm.h` | Orin constants (16 SMs, 48 KB shared, 102 GB/s, 0.66 ridge point) |
| `include/jllm_memory.h` | MemoryBudget, OOMGuard, KVCachePool, ScratchPool |
| `include/jllm_kernels.h` | Kernel API with Orin-optimal tile/block sizes |
| `src/kernels/gemv_q4.cu` | INT4 dequant-fused GEMV — 38% of decode time |
| `src/kernels/attention.cu` | Flash attention with online softmax, GQA, INT8 KV |
| `src/engine/decode.cpp` | Transformer forward pass (12 ops/layer) + generation loop |
| `src/engine/model.cpp` | GGUF parser + mmap + tensor name→pointer mapping |
| `src/jetson/thermal.cpp` | Thermal zone reader + adaptive backoff schedule |

---

**How to Test**

Start with **TinyLlama 1.1B Q4_K_M** (669 MB) — small, fast, same architecture as target:

```bash
# Build
./scripts/setup_jetson.sh

# Test without model
./build/test_memory
./build/test_kernels

# Test with model
./build/test_model_load model.gguf
./build/jetson-llm -m model.gguf -p "What is 2+2?" -n 32

# Benchmark
./scripts/bench.sh model.gguf
```

Full testing guide: [`TESTING.md`](../../../../项目/01-Jetson-LLM运行时/TESTING.md)

---

**Resources**

| Resource | What for |
|----------|----------|
| [Source code](../../../../项目/01-Jetson-LLM运行时/README.md) | The actual implementation |
| [TESTING.md](../../../../项目/01-Jetson-LLM运行时/TESTING.md) | 10-step testing guide |
| [Orin Nano Memory Architecture](../../../01-Nvidia-Jetson平台/07-Orin-Nano内存架构/Guide.md) | Unified memory deep dive |
| [LLM Optimization on Jetson](../02-Jetson-LLM优化/Guide.md) | Quantization, model selection, FlashAttention |
| [llama.cpp](https://github.com/ggerganov/llama.cpp) | Reference GGUF runtime |
| [GGML CUDA kernels](https://github.com/ggerganov/llama.cpp/tree/master/ggml/src/ggml-cuda) | Reference kernel implementations |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/5. ML and AI/jetson-llm-runtime/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/5.%20ML%20and%20AI/jetson-llm-runtime/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
