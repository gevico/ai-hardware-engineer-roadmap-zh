---
title: jetson-llm
description: jetson-llm
---

# jetson-llm

面向 NVIDIA Jetson Orin 的内存优先 LLM 推理 runtime。

**目标硬件：** Orin Nano Super 8 GB（SM 8.7，102 GB/s，67 TOPS）
**不支持：** x86、独立 GPU、Windows、macOS — 仅限 Jetson。

---

## 🚀 生产实现：`GeniePod/genie-ai-runtime` v1.0.0

可运行、经硬件验证的 runtime —— 作为 GeniePod 本地家庭 AI 栈的一部分处于活跃开发中 —— 位于：

### 👉 **[`GeniePod/genie-ai-runtime`](https://github.com/GeniePod/genie-ai-runtime)** &nbsp;·&nbsp; [release v1.0.0](https://github.com/GeniePod/genie-ai-runtime/releases/tag/v1.0.0)

本文件夹是**最初的框架 / 脚手架**，生产实现正由此 fork 而来。它记录了初始架构与首批 token 的 bring-up（上电点亮/调通）计划。生产仓库才是后续每一次 kernel 优化、persistent-KV 工作与生产加固发生的地方。

### v1.0.0 已在 Jetson Orin Nano Super 8 GB 上验证（Qwen3-4B-Q4_K_M，25 W MAXN SUPER）

| 工作负载 | 数值 |
| -------- | ------ |
| Prefill（首字前的整段计算，33-tok 冷启动） | **38.0 tok/s** |
| Decode（逐 token 生成阶段） | **9.9 tok/s** |
| 冷启动 TTFT | **877 ms** |
| 热轮次 TTFT（persistent KV，67 % prefix） | **444 ms** |
| KV pool 内存 @ 1024 ctx | **74 MB**（默认 INT8） |
| 模型加载 — 冷（NVMe） | **30 s**（79 MB/s） |
| 模型加载 — 热（pagecache） | **1.3 s**（1.8 GB/s） |
| 对比 `llama-bench pp18 = 17.97 ± 0.65 tok/s` | **prefill +115 %** |

在整个优化路径上，输出与 FP16 参考保持合理一致（源自 tensor-core `mma.sync` 的 FP16-ULP 有界漂移；源自 per-(layer, pos, kv_head) KV 量化的 INT8 精度下限漂移）。

### 生产仓库在此脚手架之外新增的内容

- **Paths A → I**（编号的优化伞形任务），每个都带阶段计划、Jetson 上逐 PR 的验证结果表，以及回滚纪律。完整叙述见生产仓库中的 [`ROADMAP.md`](https://github.com/GeniePod/genie-ai-runtime/blob/main/ROADMAP.md)。
- **Tensor-core MMQ Q4_K prefill GEMM（矩阵-矩阵乘）**（Path E）—— multi-warp 协作反量化，`mma.sync.aligned.m16n8k16` 于 SM 8.7。较标量路径 prefill +147 %。
- **uint32-load decode GEMV（矩阵-向量乘）**（Path C）—— 四字节合并权重加载 + 残差融合输出。decode +21 %。
- **Persistent KV cache**（Path F）—— 按会话保存 / 恢复，带最长前缀匹配、模型指纹校验、1 GB 上限的 LRU 淘汰。热轮次 TTFT 下降 48 %。
- **INT8 KV cache**（Path I，默认）—— 按 (layer, pos, kv_head) 做 absmax 缩放。1024 ctx 下 144 MB → 74 MB，输出合理一致。
- **兼容 OpenAI 的 HTTP 服务器** —— 基于 cpp-httplib + nlohmann/json（与 llama.cpp 的 server 所用库相同）构建，SSE 流式输出，Qwen3 推理拆分到 `reasoning_content`，systemd unit + 安装器。可选编译（`-DJLLM_BUILD_SERVER=ON`）；引擎默认以可嵌入库形式发布。
- **生产加固** —— `--version` flag、冷/热加载计时、OOM 防护防止崩溃、用于 1000+ token × 100 次迭代运行的稳定性浸泡 harness（agent 运行时框架）。

### 行之有效的模式

alpha 阶段沉淀的三个模式，可很好地迁移到其他推理引擎项目（若你在做类似工作，值得借鉴）：

1. **基于 Path 的伞形 issue，分阶段推进。** 每个编号 Path = 一个 GitHub 伞形 issue，带阶段表、预先列明风险，以及逐阶段的小 PR，每个 PR 在合并前都在 Jetson 上发布验证结果评论。这让负面结果（Path G 的 no-op 尝试、Path C 的 split-K 死胡同、Path E 的 E2 microbenchmark）易于吸收与回滚。
2. **以「合理一致，而非逐字节一致」作为质量门槛。** 一旦 `mma.sync` 重排了浮点累加，逐字节一致就不再可能。定义「FP16-ULP 有界漂移、在规范 prompt 上字符等价」解锁了整条 tensor-core 路径。同样的妥协也促成了 INT8 KV。
3. **诚实的性能重新基线。** 每个 release 都在同一天、用相同的 prompt 与机器状态重新测量上一 release 的数字。从不用跨天的数字做对比 —— 环境噪声曾坑过一次，此后便将这条纪律固化下来。

---

## 为什么这很重要（以及为什么通用 runtime 不适用）

现有 runtime 并非为与语音 STT、TTS、降噪以及 Home Assistant 容器共享的 8 GB 统一内存而设计：

- **llama.cpp** —— 可移植但 CUDA kernel 通用，无 Jetson 内存感知。生产仓库意在 GenieClaw 内部替换掉的 runtime。
- **TensorRT-LLM** —— 快但面向数据中心形态（A100/H100），对 Orin Nano 的 iGPU 预算而言太重。
- **jetson-llm / `genie-ai-runtime`** —— 内存优先、功耗感知、为 Orin SM 8.7 调优的 CUDA kernel、预分配的 KV/scratch 池、单一二进制、单一 GGUF、可与 `whisper-server` 和 `genie-core` 共存的单一共享内存预算。


<details>
<summary>English original</summary>

**jetson-llm**

Memory-first LLM inference runtime for NVIDIA Jetson Orin.

**Target hardware:** Orin Nano Super 8 GB (SM 8.7, 102 GB/s, 67 TOPS)
**Not supported:** x86, discrete GPUs, Windows, macOS — Jetson only.

---

**🚀 Production implementation: `GeniePod/genie-ai-runtime` v1.0.0**

The working, hardware-validated runtime — under active development as part of the GeniePod local home-AI stack — lives at:

**👉 **[`GeniePod/genie-ai-runtime`](https://github.com/GeniePod/genie-ai-runtime)** &nbsp;·&nbsp; [release v1.0.0](https://github.com/GeniePod/genie-ai-runtime/releases/tag/v1.0.0)**

This folder is the **original framework / scaffold** from which the production implementation was forked. It documents the initial architecture and the first-tokens bring-up plan. The production repo is where every subsequent kernel optimization, persistent-KV work, and production-hardening step happened.

**v1.0.0 verified on Jetson Orin Nano Super 8 GB (Qwen3-4B-Q4_K_M, 25 W MAXN SUPER)**

| Workload | Number |
| -------- | ------ |
| Prefill (33-tok cold) | **38.0 tok/s** |
| Decode | **9.9 tok/s** |
| Cold TTFT | **877 ms** |
| Warm-turn TTFT (persistent KV, 67 % prefix) | **444 ms** |
| KV pool memory @ 1024 ctx | **74 MB** (INT8 default) |
| Model load — cold (NVMe) | **30 s** at 79 MB/s |
| Model load — warm (pagecache) | **1.3 s** at 1.8 GB/s |
| vs `llama-bench pp18 = 17.97 ± 0.65 tok/s` | **+115 % prefill** |

Output stays sensibly-identical to FP16 reference across the entire optimization path (FP16-ULP-bounded drift from tensor-core `mma.sync`; INT8-precision-floor drift from per-(layer, pos, kv_head) KV quantization).

**What the production repo adds beyond this scaffold**

- **Paths A → I** (numbered optimization umbrellas), each with phase plans, per-PR verified-result tables on Jetson, and rollback discipline. See [`ROADMAP.md`](https://github.com/GeniePod/genie-ai-runtime/blob/main/ROADMAP.md) in the production repo for the full narrative.
- **Tensor-core MMQ Q4_K prefill GEMM** (Path E) — multi-warp cooperative dequant, `mma.sync.aligned.m16n8k16` on SM 8.7. +147 % prefill over the scalar path.
- **uint32-load decode GEMV** (Path C) — quad-byte coalesced weight loads + residual-fused output. +21 % decode.
- **Persistent KV cache** (Path F) — per-conversation save / hydrate with longest-prefix match, model fingerprint validation, LRU eviction at 1 GB cap. Warm-turn TTFT drops 48 %.
- **INT8 KV cache** (Path I, default) — per-(layer, pos, kv_head) absmax-scaled. 144 MB → 74 MB at 1024 ctx, sensibly-identical output.
- **OpenAI-compatible HTTP server** — built on cpp-httplib + nlohmann/json (the same libs llama.cpp's server uses), SSE streaming, Qwen3 reasoning split into `reasoning_content`, systemd unit + installer. Opt-in build (`-DJLLM_BUILD_SERVER=ON`); the engine ships as an embeddable library by default.
- **Production hardening** — `--version` flag, cold/warm load timing, OOM-guard prevents crashes, stability soak harness for 1000+ token × 100 iteration runs.

**Patterns that worked**

Three patterns from the alpha-track that travel well to other inference-engine projects (worth stealing if you're doing similar work):

1. **Path-based umbrella issues with staged phases.** Every numbered Path = one GitHub umbrella issue with a phase table, risks named up front, and small per-phase PRs that each posted a verified-result comment on Jetson before merge. Made negative results (Path G's no-op attempts, Path C's split-K dead end, Path E's E2 microbenchmark) easy to absorb and roll back.
2. **"Sensibly-identical, not byte-identical" as the quality bar.** Once `mma.sync` reordered float accumulation, byte equality was off the table. Defining "FP16-ULP-bounded drift, character-equivalent on the canonical prompt" unlocked the whole tensor-core path. Same compromise enabled INT8 KV.
3. **Honest perf re-baselines.** Every release re-measured the prior release's number same-day with the same prompt and machine state. We never compared cross-day numbers — environmental noise had bitten us once and we baked the discipline in after.

---

**Why this matters (and why generic runtimes don't fit)**

Existing runtimes are not designed for 8 GB unified memory shared with voice STT, TTS, denoise, and a Home Assistant container:

- **llama.cpp** — portable but generic CUDA kernels, no Jetson memory awareness. The runtime the production repo aims to replace inside GenieClaw.
- **TensorRT-LLM** — fast but datacenter-shaped (A100/H100), too heavy for Orin Nano's iGPU budget.
- **jetson-llm / `genie-ai-runtime`** — memory-first, power-aware, Orin SM 8.7-tuned CUDA kernels, pre-allocated KV/scratch pools, single binary, single GGUF, single shared-memory budget that fits alongside `whisper-server` and `genie-core`.

</details>

## 架构（按最初脚手架搭建 —— 当前状态见 production repo）

```
┌──────────────────────────────────────────────────────┐
│                   jetson-llm                          │
│                                                       │
│  Serving Layer (OpenAI-compatible REST API)           │
│    POST /v1/chat/completions | GET /health            │
│                                                       │
│  Engine (GGUF load → prefill → decode → sample)      │
│    transformer_layer() × N_layers per token           │
│    Memory guard + thermal check per token             │
│                                                       │
│  CUDA Kernels (SM 8.7 tuned)                          │
│    gemv_q4 | fused_rmsnorm | flash_attn | rope        │
│    swiglu | softmax | fp16↔int8                       │
│                                                       │
│  Memory Manager                                       │
│    MemoryBudget | OOMGuard | KVCachePool | ScratchPool│
│                                                       │
│  Jetson HAL                                           │
│    PowerState | ThermalState | LiveStats | JetsonInfo │
└──────────────────────────────────────────────────────┘
```

production repo 在此基础上扩展了：
- tensor-core MMQ Q4_K prefill（首字前的整段计算）GEMM（脚手架中不存在的新 kernel）
- 持久化 KV cache 模块（`src/persistence/`）
- 基于 cpp-httplib + nlohmann/json 的 server（替换原先的 raw-sockets server）
- `scripts/soak.sh` 与 `scripts/bench_load.sh`，用于稳定性 + 加载时校验

## 如何使用本文件夹

| 你想做什么… | 去这里 |
| ------------ | ------- |
| 运行、构建或为实际实现做贡献 | **[`GeniePod/genie-ai-runtime`](https://github.com/GeniePod/genie-ai-runtime)** |
| 阅读 v1.0.0 发布说明 | [Release v1.0.0](https://github.com/GeniePod/genie-ai-runtime/releases/tag/v1.0.0) |
| 阅读 alpha 线叙事（Path A→I、尝试过什么、失败过什么） | [`ROADMAP.md`（production repo）](https://github.com/GeniePod/genie-ai-runtime/blob/main/ROADMAP.md) |
| 阅读各版本已验证的数据 | [`CHANGELOG.md`（production repo）](https://github.com/GeniePod/genie-ai-runtime/blob/main/CHANGELOG.md) |
| 阅读 HTTP server 参考 | [`docs/server.md`（production repo）](https://github.com/GeniePod/genie-ai-runtime/blob/main/docs/server.md) |
| 阅读最初的 first-tokens 路线图（本文件夹的规划文档） | [`ROADMAP.md`](ROADMAP.md) |
| 阅读最初的测试计划 | [`TESTING.md`](TESTING.md) |
| 查看最初脚手架搭建的模块 | [`src/`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/src), [`include/`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/include), [`tests/`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/tests), [`scripts/`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/scripts) |

## 最初的脚手架（保留作起始框架）

本 README 的其余部分记录的是该框架最初为 AI Hardware Engineer Roadmap 练习搭建脚手架时的样子。下文全部描述的是 **初始** 模块布局、已完成组件，以及 pre-v0.1 bring-up（上电点亮/调通）期间的 bug 跟踪记录 —— 在此保留，作为该路线图的历史背景。production repo 早已远远超出这一阶段。

### 已完成（截至最初脚手架，✅）

| 组件 | 文件 | 行数 | 状态 |
|-----------|-------|-------|--------|
| **内存管理器** | budget.cpp, kv_cache.cpp, pool.cpp | ~300 | ✅ 已实现，已测试 |
| **Jetson HAL** | power.cpp, thermal.cpp, sysinfo.cpp | ~250 | ✅ 读取 sysfs，已测试 |
| **CUDA kernel** | 6 个 .cu 文件 | ~500 | ✅ 已实现，5 项正确性测试通过 |
| **GGUF 配置解析器** | model.cpp (load_gguf_config) | ~80 | ✅ 读取模型架构 |
| **GGUF tensor 解析器** | model.cpp (parse_tensor_infos) | ~150 | ✅ 解析 tensor 名称/形状/偏移 |
| **权重映射** | model.cpp (load_and_map_weights) | ~80 | ✅ 映射 tensor 名称 → 结构体指针 |
| **tokenizer** | tokenizer.cpp | ~160 | ✅ 读取 GGUF 词表，encode/decode |
| **采样** | sample.cpp | ~120 | ✅ Top-k, top-p, temperature, repeat penalty |
| **Transformer 前向** | decode.cpp (transformer_layer) | ~100 | ✅ 串联每个 layer 的 12 个算子 |
| **decode 循环（逐 token 生成阶段）** | decode.cpp (generate) | ~80 | ✅ prefill + decode + 流式输出 |
| **CLI** | main.cpp | ~120 | ✅ 交互式 + 单 prompt + OOM 预检查 |
| **HTTP server** | http_server.cpp, main_server.cpp | ~250 | ✅ /health, /v1/chat/completions, /v1/models |
| **脚本** | setup, bench, profile | ~180 | ✅ 首次安装配置、benchmark、nsys 性能剖析 |
| **测试** | test_memory, test_kernels, test_model_load | ~280 | ✅ 内存、5 项 kernel 测试、8 项模型加载测试 |
| **合计** | **30 个文件** | **~4,200** | （脚手架；当前约 10 k LOC 见 production repo） |


<details>
<summary>English original</summary>

**Architecture (as originally scaffolded — see production repo for current state)**

```
┌──────────────────────────────────────────────────────┐
│                   jetson-llm                          │
│                                                       │
│  Serving Layer (OpenAI-compatible REST API)           │
│    POST /v1/chat/completions | GET /health            │
│                                                       │
│  Engine (GGUF load → prefill → decode → sample)      │
│    transformer_layer() × N_layers per token           │
│    Memory guard + thermal check per token             │
│                                                       │
│  CUDA Kernels (SM 8.7 tuned)                          │
│    gemv_q4 | fused_rmsnorm | flash_attn | rope        │
│    swiglu | softmax | fp16↔int8                       │
│                                                       │
│  Memory Manager                                       │
│    MemoryBudget | OOMGuard | KVCachePool | ScratchPool│
│                                                       │
│  Jetson HAL                                           │
│    PowerState | ThermalState | LiveStats | JetsonInfo │
└──────────────────────────────────────────────────────┘
```

The production repo extends this with:
- Tensor-core MMQ Q4_K prefill GEMM (a new kernel that didn't exist in the scaffold)
- Persistent KV cache module (`src/persistence/`)
- cpp-httplib + nlohmann/json-based server (replacing the original raw-sockets server)
- `scripts/soak.sh` and `scripts/bench_load.sh` for stability + load-time validation

**How to use this folder**

| You want to… | Go here |
| ------------ | ------- |
| Run, build, or contribute to the actual implementation | **[`GeniePod/genie-ai-runtime`](https://github.com/GeniePod/genie-ai-runtime)** |
| Read v1.0.0 release notes | [Release v1.0.0](https://github.com/GeniePod/genie-ai-runtime/releases/tag/v1.0.0) |
| Read the alpha-track narrative (Paths A→I, what we tried, what failed) | [`ROADMAP.md` (production repo)](https://github.com/GeniePod/genie-ai-runtime/blob/main/ROADMAP.md) |
| Read per-release verified numbers | [`CHANGELOG.md` (production repo)](https://github.com/GeniePod/genie-ai-runtime/blob/main/CHANGELOG.md) |
| Read the HTTP server reference | [`docs/server.md` (production repo)](https://github.com/GeniePod/genie-ai-runtime/blob/main/docs/server.md) |
| Read the original first-tokens roadmap (this folder's planning doc) | [`ROADMAP.md`](ROADMAP.md) |
| Read the original test plan | [`TESTING.md`](TESTING.md) |
| See the original scaffolded modules | [`src/`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/src), [`include/`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/include), [`tests/`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/tests), [`scripts/`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/scripts) |

**Original scaffold (preserved as the starting framework)**

The rest of this README documents the framework as it was originally scaffolded for the AI Hardware Engineer Roadmap exercise. Everything below describes the **initial** module layout, completed components, and bug-tracking notes from the pre-v0.1 bring-up — preserved here as historical context for the roadmap. The production repo has moved well past it.

**Completed (as of initial scaffold, ✅)**

| Component | Files | Lines | Status |
|-----------|-------|-------|--------|
| **Memory Manager** | budget.cpp, kv_cache.cpp, pool.cpp | ~300 | ✅ Implemented, tested |
| **Jetson HAL** | power.cpp, thermal.cpp, sysinfo.cpp | ~250 | ✅ Reads sysfs, tested |
| **CUDA Kernels** | 6 .cu files | ~500 | ✅ Implemented, 5 correctness tests pass |
| **GGUF Config Parser** | model.cpp (load_gguf_config) | ~80 | ✅ Reads model architecture |
| **GGUF Tensor Parser** | model.cpp (parse_tensor_infos) | ~150 | ✅ Parses tensor name/shape/offset |
| **Weight Mapping** | model.cpp (load_and_map_weights) | ~80 | ✅ Maps tensor names → struct pointers |
| **Tokenizer** | tokenizer.cpp | ~160 | ✅ Reads GGUF vocab, encode/decode |
| **Sampling** | sample.cpp | ~120 | ✅ Top-k, top-p, temperature, repeat penalty |
| **Transformer Forward** | decode.cpp (transformer_layer) | ~100 | ✅ Wires all 12 ops per layer |
| **Decode Loop** | decode.cpp (generate) | ~80 | ✅ Prefill + decode + streaming |
| **CLI** | main.cpp | ~120 | ✅ Interactive + single prompt + OOM pre-check |
| **HTTP Server** | http_server.cpp, main_server.cpp | ~250 | ✅ /health, /v1/chat/completions, /v1/models |
| **Scripts** | setup, bench, profile | ~180 | ✅ First-time setup, benchmark, nsys profiling |
| **Tests** | test_memory, test_kernels, test_model_load | ~280 | ✅ Memory, 5 kernel tests, 8 model load tests |
| **Total** | **30 files** | **~4,200** | (scaffold; see production repo for current ~10 k LOC) |

</details>

### bring-up（上电点亮/调通）期间修复的初始 bug（✅，历史）

| # | Bug | 修复措施 |
|---|-----|-------------|
| 1 | GGUF KV 跳过时 offset 计算错误 | 借助 `gguf_scalar_size()` helper 按精确的 GGUF 类型大小重写；标量数组一次性 `fseek` 跳过 |
| 2 | 残差连接未串联 | 新增 `vec_add()` kernel：`x2 = x + attn_proj`，随后 `x = x2 + ffn_out` |
| 3 | embedding memcpy 方向错误 | 改为 `cudaMemcpyDefault`（host mmap 与 device memory 均可用） |
| 4 | 缺少 `<sys/mman.h>` include | 补上 `#include <sys/mman.h>` |
| 5 | CUDA Graph body 为空 | 实现完整的 graph capture：所有 Transformer layer + 最终 norm + logit projection |
| 6 | attention 累加器 `acc[d%4]` | 替换为 shared memory 中按维度划分的 `s_out[head_dim]` |
| 7 | logits 为 FP16，无 FP32 | 新增 `fp16_to_fp32()` GPU kernel；在 D2H 拷贝前于 device 上完成转换 |
| 8 | tokenizer O(V×L) 扫描 | 新增 `token_to_id_` hash map + `max_token_len_`，实现 O(max_len) 最长匹配 |

production repo 中的 Path A → I 工作针对的是完全不同的一类问题（kernel 架构、内存布局、规模化下的正确性/性能取舍）。

### 最初的 v0.1 → v0.4 里程碑路线图（✅ 已交付 + 已扩展）

```
v0.1 — First Tokens (DONE → see alpha.2 in production repo)
  ✅ All 8 bring-up bugs fixed
  ✅ Build on Jetson (cmake + make)
  ✅ test_model_load passes with Qwen3-4B Q4_K_M
  ✅ Generate coherent text

v0.2 — Benchmark Baseline (DONE → alpha.2 baseline established)
  ✅ bench.sh produces tok/s numbers
  ✅ Compared against llama-bench (17.97 tok/s pp18 baseline)

v0.3 — Performance Target (DONE → exceeded in alpha.8)
  ✅ >20% faster than stock llama.cpp on decode (decode: parity; prefill: +115%)
  ✅ CUDA graph replay verified working
  □ Memory-stable over 1000+ tokens (soak harness shipped, full run pending — issue #4)

v0.4 — Production Ready (DONE → v1.0.0)
  ✅ Chat template support + SSE streaming
  ✅ Multi-turn conversation (Path F persistent KV)
  ✅ Documented performance table across releases (CHANGELOG.md alpha.2 → v1.0.0)
  □ 24-hour stability test (issue #7, scheduled post-v1.0)
```

## 许可证

MIT — 与 production repo 一致，这是有意为之。runtime 属于基础设施，其他项目应能以低成本嵌入。


<details>
<summary>English original</summary>

**Initial bugs that were fixed during bring-up (✅, historical)**

| # | Bug | Fix applied |
|---|-----|-------------|
| 1 | GGUF KV skip miscalculated offsets | Rewrote with exact GGUF type sizes via `gguf_scalar_size()` helper; arrays of scalars skip in one `fseek` |
| 2 | Residual connection not chained | Added `vec_add()` kernel: `x2 = x + attn_proj`, then `x = x2 + ffn_out` |
| 3 | Embedding memcpy wrong direction | Changed to `cudaMemcpyDefault` (works for both host mmap and device memory) |
| 4 | Missing `<sys/mman.h>` include | Added `#include <sys/mman.h>` |
| 5 | CUDA graph body empty | Implemented full graph capture: all transformer layers + final norm + logit projection |
| 6 | Attention accumulator `acc[d%4]` | Replaced with per-dimension `s_out[head_dim]` in shared memory |
| 7 | FP16 logits, no FP32 | Added `fp16_to_fp32()` GPU kernel; convert on device before D2H copy |
| 8 | Tokenizer O(V×L) scan | Added `token_to_id_` hash map + `max_token_len_` for O(max_len) longest-match |

The Path A → I work in the production repo addresses an entirely different class of issues (kernel architecture, memory layout, correctness/perf tradeoffs at scale).

**Original v0.1 → v0.4 milestone roadmap (✅ delivered + extended)**

```
v0.1 — First Tokens (DONE → see alpha.2 in production repo)
  ✅ All 8 bring-up bugs fixed
  ✅ Build on Jetson (cmake + make)
  ✅ test_model_load passes with Qwen3-4B Q4_K_M
  ✅ Generate coherent text

v0.2 — Benchmark Baseline (DONE → alpha.2 baseline established)
  ✅ bench.sh produces tok/s numbers
  ✅ Compared against llama-bench (17.97 tok/s pp18 baseline)

v0.3 — Performance Target (DONE → exceeded in alpha.8)
  ✅ >20% faster than stock llama.cpp on decode (decode: parity; prefill: +115%)
  ✅ CUDA graph replay verified working
  □ Memory-stable over 1000+ tokens (soak harness shipped, full run pending — issue #4)

v0.4 — Production Ready (DONE → v1.0.0)
  ✅ Chat template support + SSE streaming
  ✅ Multi-turn conversation (Path F persistent KV)
  ✅ Documented performance table across releases (CHANGELOG.md alpha.2 → v1.0.0)
  □ 24-hour stability test (issue #7, scheduled post-v1.0)
```

**License**

MIT — same as the production repo, on purpose. The runtime is infrastructure that other projects should be able to embed cheaply.

</details>

---

> 原文：[`Projects/jetson-llm-runtime/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
