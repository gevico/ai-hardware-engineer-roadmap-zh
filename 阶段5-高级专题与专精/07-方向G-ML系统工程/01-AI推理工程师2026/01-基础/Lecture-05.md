---
title: Part 1 · 第 05 讲 — runtime 格局（2026）
description: Part 1 · 第 05 讲 — runtime 格局（2026）
---

# Part 1 · 第 05 讲 — runtime 格局（2026）

## 概述

**runtime** 是模型文件与 GPU 之间的层。它拥有：

* 将请求转化为 kernel 启动的调度器。
* 决定 attention 使用 FlashAttention 4 还是更旧实现的 kernel 选择。
* 拥有 KV cache 的内存管理器。
* 使用（或未能使用）硬件上 FP8 / FP4 / INT4 路径的量化集成。
* 栈的其余部分与之通信的 API 接口面。

选择正确的 runtime 是选择模型之后的 **单一最大工程决策**。**错误选择会使吞吐损失 2–5×**，阻碍功能（paged KV、前缀缓存、FP8），或限制可针对的硬件。正确选择意味着大部分困难的 kernel 和调度器工作已经完成。

本讲涵盖 2026 年的格局：

1. **vLLM** — 开源主力，2026 年采用 V1 引擎。
2. **SGLang** — RadixAttention + EP，结构化输出 + MoE（混合专家模型）推理服务的领导者。
3. **TensorRT-LLM** — NVIDIA 的最大吞吐路径，原生支持 FP8 和 FP4。
4. **llama.cpp** — 边缘 / 桌面 / CPU+GPU 通用 runtime。
5. **MLX** — Apple Silicon 原生。
6. **LMDeploy 及其他** — TurboMind kernel，值得了解的次级玩家。

对每个：它擅长什么、不擅长什么、何时选择它。

到本讲结束时，你应拥有针对任意工作负载 × 硬件组合的 runtime 决策矩阵，以及一个可辩护的起点，让你在接到新项目后一小时内就能在其上做原型。

---

## 1. vLLM — 开源主力

**项目：** [github.com/vllm-project/vllm](https://github.com/vllm-project/vllm)
**固定版本（本讲）：** 0.22.x，以 **V1 引擎** 作为默认路径。

### 1.1 它做什么

* **连续批处理** — 核心特性；在一个批中任意混合 prefill（首字前的整段计算）和 decode（逐 token 生成阶段），每个 step 重新组合。
* **PagedAttention v2** — 块级 KV 内存管理，消除此前限制批大小的内存碎片。
* **前缀缓存** — 共享的系统提示词和对话前缀自动命中缓存。
* **多 LoRA 推理服务** — 多个适配器可在单个副本中共享一个基础模型。
* **多 GPU** — 张量并行（NCCL）以及（在 V1 中）更好的流水线 + EP 支持。
* **OpenAI 兼容 API** — 可直接替换面向 OpenAI 的 REST 形态的客户端。
* **量化** — AWQ、GPTQ、FP8（Marlin kernel）、GGUF（通过实验性 kernel）。
* **投机解码** — 支持 draft model、Medusa、EAGLE 系列。

### 1.2 V0 → V1 引擎

vLLM 的 V0 引擎（2023 → 2025 年初）累积了复杂性。**V1 引擎**（2025 年起）是重写版，它：

* 清理调度器-kernel 边界。
* 改进 decode 上的 CUDA Graph 集成。
* 更好的多模态支持（视觉编码器内联）。
* 降低 Python 开销 — 对短 decode 有意义。

**除非有特定理由（尚未迁移的第三方插件），否则始终运行 V1**。V0 相关内容应视为考古。

### 1.3 它不擅长什么

* 分离式 prefill / decode（Mooncake 风格）截至 2026 年中仍在成熟中；不是 V1 的强项。
* 一些前沿模型架构比 SGLang 落地更晚（SGLang 团队通常率先获得 DeepSeek / Qwen3 支持）。
* 当二者都能在 Hopper 上使用 FP8 时，TensorRT-LLM 在原始单副本吞吐上仍胜过它。

### 1.4 何时选择 vLLM

* 使用多个模型或 LoRA 的 OpenAI-API 兼容推理服务。
* Hopper 或更早硬件，混合工作负载（聊天 + agent + 偶发批处理）。
* 你想要最广泛的社区支持和最大的集成生态。
* 你需要多模态（视觉）内联推理服务。

**大多数生产部署的默认起点。**

---

## 2. SGLang — RadixAttention + EP

**项目：** [github.com/sgl-project/sglang](https://github.com/sgl-project/sglang)
**固定版本：** 0.5.x.

### 2.1 它做什么

* **RadixAttention** — 基于前缀 token 的基数树，让*任意*共享前缀（不仅是系统提示词）命中缓存。这是任何开放 runtime 中最激进的前缀缓存架构。
* **专家并行（EP）** — 对 MoE 模型的一等支持。SGLang 是服务 DeepSeek V3 / Qwen3-MoE 的开源参考。
* **结构化输出** — XGrammar 和 Outlines 集成；工具调用语法强制很快。
* **投机解码** — EAGLE-2、EAGLE-3、用于 DeepSeek 的 MTP。
* **多 GPU** — TP、PP、EP 组合，包括当前版本中的分离式 P/D。
* **OpenAI 兼容 API**，具有用于结构化输出的扩展端点。

### 2.2 它不擅长什么

* 社区比 vLLM 小；集成生态更薄弱。
* 文档滞后于功能；依赖项目的示例 notebook。
* 多模态支持存在，但不如 vLLM 成熟。


<details>
<summary>English original</summary>

**Part 1 · Lecture 05 — The Runtime Landscape (2026)**

**Overview**

The **runtime** is the layer between the model file and the GPU. It owns:

* The scheduler that turns requests into kernel launches.
* The kernel selection that decides whether attention is FlashAttention 4 or something older.
* The memory manager that owns the KV cache.
* The quantization integration that uses (or fails to use) the FP8 / FP4 / INT4 path on the hardware.
* The API surface that the rest of the stack talks to.

Picking the right runtime is the **single largest engineering decision** after picking the model. A **wrong choice costs 2–5× in throughput**, blocks features (paged KV, prefix cache, FP8), or limits the hardware you can target. A right choice means most of the hard kernel and scheduler work is already done.

This lecture covers the 2026 landscape:

1. **vLLM** — the open-source workhorse, V1 engine in 2026.
2. **SGLang** — RadixAttention + EP, the structured-output + MoE serving leader.
3. **TensorRT-LLM** — NVIDIA's max-throughput path, FP8 and FP4 native.
4. **llama.cpp** — the edge / desktop / CPU+GPU universal runtime.
5. **MLX** — Apple Silicon native.
6. **LMDeploy and others** — TurboMind kernels, secondary players worth knowing.

For each: what it does well, what it does badly, when to pick it.

By the end you should have a runtime decision matrix for any workload × hardware combination, and a defended starting point you can prototype on within an hour of receiving a new project.

---

**1. vLLM — the open-source workhorse**

**Project:** [github.com/vllm-project/vllm](https://github.com/vllm-project/vllm)
**Pinned version (this lecture):** 0.22.x with the **V1 engine** as the default path.

**1.1 What it does**

* **Continuous batching** — the headline feature; arbitrary mixing of prefill and decode in one batch, recomposed every step.
* **PagedAttention v2** — block-level KV memory management, eliminates the memory fragmentation that previously capped batch sizes.
* **Prefix cache** — shared system prompts and conversation prefixes hit the cache automatically.
* **Multi-LoRA serving** — many adapters can share one base model in a single replica.
* **Multi-GPU** — tensor parallelism (NCCL) and (in V1) better pipeline + EP support.
* **OpenAI-compatible API** — drop-in for clients that target OpenAI's REST shape.
* **Quantization** — AWQ, GPTQ, FP8 (Marlin kernels), GGUF (via experimental kernels).
* **Speculative decoding** — draft model, Medusa, EAGLE families supported.

**1.2 V0 → V1 engine**

vLLM's V0 engine (2023 → early 2025) accumulated complexity. The **V1 engine** (2025-onwards) is a rewrite that:

* Cleans up the scheduler-kernel boundary.
* Improves CUDA Graph integration on decode.
* Better multi-modal support (vision encoders inline).
* Lowers Python overhead — meaningful for short decodes.

**Always run V1 unless you have a specific reason** (a third-party plugin that hasn't migrated). V0 references should be treated as archaeology.

**1.3 What it does badly**

* Disaggregated prefill / decode (Mooncake-style) is still maturing as of mid-2026; not the V1 strong suit.
* Some bleeding-edge model architectures land later than in SGLang (the SGLang team often gets DeepSeek / Qwen3 support first).
* TensorRT-LLM still outperforms it on raw single-replica throughput when both can use FP8 on Hopper.

**1.4 When to pick vLLM**

* OpenAI-API-compatible serving with multiple models or LoRAs.
* Hopper or earlier hardware with mixed workloads (chat + agent + occasional batch).
* You want the broadest community support and the largest ecosystem of integrations.
* You need multi-modal (vision) inline serving.

**Default starting point for most production deployments.**

---

**2. SGLang — RadixAttention + EP**

**Project:** [github.com/sgl-project/sglang](https://github.com/sgl-project/sglang)
**Pinned version:** 0.5.x.

**2.1 What it does**

* **RadixAttention** — a radix tree over prefix tokens that lets *any* shared prefix (not just system prompts) hit the cache. This is the most aggressive prefix-cache architecture in any open runtime.
* **Expert parallelism (EP)** — first-class support for MoE models. SGLang is the open-source reference for serving DeepSeek V3 / Qwen3-MoE.
* **Structured output** — XGrammar and Outlines integrations; tool-call grammar enforcement is fast.
* **Speculative decoding** — EAGLE-2, EAGLE-3, MTP for DeepSeek.
* **Multi-GPU** — TP, PP, EP combinations including disaggregated P/D in current releases.
* **OpenAI-compatible API** with extended endpoints for structured output.

**2.2 What it does badly**

* Smaller community than vLLM; ecosystem of integrations is thinner.
* Documentation lags features; relies on the project's example notebooks.
* Multi-modal support exists but is less mature than vLLM's.

</details>

### 2.3 何时选择 SGLang

* **MoE 推理服务**（混合专家模型）—— DeepSeek V3.1 / Qwen3-MoE 规模化 → 首选 SGLang。
* **Agent / 工具使用工作负载** —— XGrammar 集成很快；RadixAttention 有助于处理系统提示词中重复的工具目录。
* **RAG 工作负载**（检索增强生成）—— 跨请求大量前缀共享；RadixAttention 是结构上的优势。
* **长提示词批处理** —— 跨多个请求共享系统指令的前缀。

**第 3 部分中 MoE 的默认起点。**

---

## 3. TensorRT-LLM — NVIDIA 的最大吞吐路径

**项目：** [github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)
**锁定版本：** 1.3.x。

### 3.1 它能做什么

* **TensorRT engine 编译** —— 模型图编译为硬件特定的 engine plan；在相同 Hopper GPU 上，峰值单副本吞吐通常比 vLLM 快 1.5–2×。
* **FP8 原生** —— 一等 FP8 支持（E4M3 权重、E5M2 KV），是各 runtime 中最成熟的 FP8 路径。
* **FP4 / Blackwell** —— TE2 FP4 路径在 1.x 版本中落地；Blackwell 的规范推理路径很可能将是 TRT-LLM。
* **在途批处理** —— NVIDIA 对连续批处理的等价机制。
* **Triton Inference Server 集成** —— 生产推理服务编排。
* **所有 NVIDIA kernel** —— FlashAttention 4、Hopper TMA + WGMMA、Blackwell tcgen05（UMMA），全部手工调优。

### 3.2 它做不好什么

* **Engine 编译摩擦** —— 每个模型 + 精度 + batch-size bucket 都需要编译好的 engine plan。迭代很慢。
* **厂商锁定** —— 仅 NVIDIA 硬件。没有 AMD，没有 Apple，没有边缘 ARM。
* **配置复杂** —— 大量可调参数；专家级文档；没有 NVIDIA 解决方案架构师帮助时，部署并非易事。
* **支持新模型架构更慢** —— TRT-LLM 的发布周期常常比开源模型晚数周。

### 3.3 何时选择 TensorRT-LLM

* **最大单副本吞吐**，在 Hopper / Blackwell 上，模型固定。
* **生产部署** —— 其中 engine plan 可编译一次并服务数月。
* **FP8 或 FP4 原生**为必需，且模型稳定。
* **仅 NVIDIA 技术栈**可接受。

**在 Hopper / Blackwell 上，当模型稳定后，对成本敏感的批处理 / 高吞吐聊天工作负载的默认选择。**

---

## 4. llama.cpp — 通用边缘 / 桌面

**项目：** [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
**锁定版本：** post-2026-04 构建。

### 4.1 它能做什么

* **到处都能运行** —— CPU（AVX2/AVX512/NEON）、CUDA、ROCm、Metal（Apple）、Vulkan、OpenCL。跨桌面、服务器和边缘的单一二进制文件。
* **GGUF 格式** —— 单文件模型包，内嵌 tokenizer + 配置 + 量化。
* **K-quants 和 IQ-quants** —— 定制量化格式（Q4_K_M、IQ4_XS、IQ3_S 等）——精度选项最丰富。
* **投机解码** —— 支持 draft-model 推测。
* **Server 模式** —— `llama-server` 暴露 HTTP/JSON API（OpenAI 兼容子集）。
* **小模型成熟** —— Qwen3-4B、Llama 3.2-1B、Phi-4-mini 等。

### 4.2 它做不好什么

* **规模化吞吐**在 Hopper 上远低于 vLLM/SGLang —— llama.cpp 是为“一个用户，一台机器”设计的，不是“100 个并发用户，一个集群”。
* **没有 vLLM 意义上的 paged KV** —— 其内存模型是按会话。
* **在 Hopper 级 GPU 上没有值得使用的张量并行** —— TP-1 是实用模式。

### 4.3 何时选择 llama.cpp

* **边缘** —— Jetson Orin Nano、Raspberry Pi 5、Mac mini、单 GPU 桌面部署。
* **Mac 开发** —— Apple Silicon 上的 CPU 路径非常出色；用于 Apple 部署时，改用 MLX（§5）。
* **仅 CPU 部署** —— `llama.cpp` 是唯一成熟的选项。
* **捆绑模型 + tokenizer** 工作流，其中 GGUF 分发简化了运维。

**任何按用户、按机器、边缘或桌面场景的默认选择。**

---

## 5. MLX — Apple Silicon 原生

**项目：** [github.com/ml-explore/mlx](https://github.com/ml-explore/mlx)
**锁定版本：** 0.31.x。

### 5.1 它能做什么

* **Apple Silicon 原生** —— 在适当情况下使用统一内存 + Apple GPU + Neural Engine。
* **类 NumPy API** —— 对 PyTorch 用户很熟悉。
* **MLX-LM** —— 独立包，提供大语言模型推理工具、量化（4-bit / 8-bit）、生成循环。
* **M 系列上的性能** —— 在 M4 Max / M3 Ultra 上，MLX 在大多数精度下优于 llama.cpp 的 Metal 后端，因为它更激进地利用 Apple GPU。

### 5.2 它做不好什么

* **仅限 Apple** —— 在 NVIDIA / AMD 上无用。
* **社区比 llama.cpp 小** —— 可用的模型转换更少。
* **Server 模式很简陋** —— 面向单用户推理。

### 5.3 何时选择 MLX

* **Apple Silicon 部署** —— Mac Studio、MacBook Pro、Mac mini 上部署的 AI 设备。
* **Mac 上的本地开发** —— 配合高保真推理（相对于 llama.cpp 的 Metal 路径）。
* **单用户、高端 Mac** 场景，希望为 70B 模型使用 64 GB 统一内存。

---


<details>
<summary>English original</summary>

**2.3 When to pick SGLang**

* **MoE serving** — DeepSeek V3.1 / Qwen3-MoE at scale → SGLang first.
* **Agent / tool-use workloads** — XGrammar integration is fast; RadixAttention helps with repeated tool catalogs in system prompts.
* **RAG workloads** — heavy prefix sharing across requests; RadixAttention is the structural win.
* **Long-prompt batch processing** — prefix sharing on system instructions across many requests.

**Default starting point for MoE in Part 3.**

---

**3. TensorRT-LLM — NVIDIA's max-throughput path**

**Project:** [github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)
**Pinned version:** 1.3.x.

**3.1 What it does**

* **TensorRT engine compilation** — model graph compiled into a hardware-specific engine plan; usually 1.5–2× faster than vLLM at peak single-replica throughput on the same Hopper GPU.
* **FP8 native** — first-class FP8 support (E4M3 weights, E5M2 KV), the most mature FP8 path among the runtimes.
* **FP4 / Blackwell** — TE2 FP4 path landed in the 1.x releases; the canonical Blackwell inference path will probably be TRT-LLM.
* **In-flight batching** — NVIDIA's continuous-batching equivalent.
* **Triton Inference Server integration** — production serving orchestration.
* **All NVIDIA kernels** — FlashAttention 4, Hopper TMA + WGMMA, Blackwell tcgen05 (UMMA), all hand-tuned.

**3.2 What it does badly**

* **Engine compilation friction** — each model + precision + batch-size-bucket needs a compiled engine plan. Iteration is slow.
* **Vendor lock-in** — NVIDIA hardware only. No AMD, no Apple, no edge ARM.
* **Configuration complexity** — many knobs; expert-level documentation; non-trivial to deploy without an NVIDIA solution architect's help.
* **Slower to support new model architectures** — TRT-LLM's release cycle often trails the open-source models by weeks.

**3.3 When to pick TensorRT-LLM**

* **Maximum single-replica throughput** on Hopper / Blackwell, model fixed.
* **Production deployment** where the engine plan can be compiled once and served for months.
* **FP8 or FP4 native** is required and the model is stable.
* **NVIDIA-only stack** is acceptable.

**Default for the cost-sensitive batch / high-throughput chat workloads on Hopper / Blackwell once the model has stabilized.**

---

**4. llama.cpp — universal edge / desktop**

**Project:** [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
**Pinned version:** post-2026-04 builds.

**4.1 What it does**

* **Runs everywhere** — CPU (AVX2/AVX512/NEON), CUDA, ROCm, Metal (Apple), Vulkan, OpenCL. Single binary across desktop, server, and edge.
* **GGUF format** — single-file model bundles with embedded tokenizer + config + quantization.
* **K-quants and IQ-quants** — bespoke quantization formats (Q4_K_M, IQ4_XS, IQ3_S, etc.) — most diverse precision options.
* **Speculative decoding** — draft-model speculation supported.
* **Server mode** — `llama-server` exposes a HTTP/JSON API (OpenAI-compatible subset).
* **Mature for small models** — Qwen3-4B, Llama 3.2-1B, Phi-4-mini etc.

**4.2 What it does badly**

* **Throughput at scale** is far below vLLM/SGLang on Hopper — llama.cpp is designed for "one user, one machine," not "100 concurrent users, one cluster."
* **No paged KV in the vLLM sense** — its memory model is per-session.
* **No tensor parallelism worth using on Hopper-class GPUs** — TP-1 is the practical mode.

**4.3 When to pick llama.cpp**

* **Edge** — Jetson Orin Nano, Raspberry Pi 5, Mac mini, single-GPU desktop deployments.
* **Mac development** — the CPU path on Apple Silicon is excellent; for Apple deployment use MLX (§5) instead.
* **CPU-only deployments** — `llama.cpp` is the only mature option.
* **Bundled model + tokenizer** workflow where GGUF distribution simplifies operations.

**Default for any per-user, per-machine, edge or desktop scenario.**

---

**5. MLX — Apple Silicon native**

**Project:** [github.com/ml-explore/mlx](https://github.com/ml-explore/mlx)
**Pinned version:** 0.31.x.

**5.1 What it does**

* **Apple Silicon native** — uses unified memory + Apple GPU + Neural Engine where appropriate.
* **NumPy-like API** — familiar to PyTorch users.
* **MLX-LM** — separate package with LLM inference utilities, quantization (4-bit / 8-bit), generation loop.
* **Performance on M-series** — on M4 Max / M3 Ultra, MLX outperforms llama.cpp Metal backend at most precisions because it uses the Apple GPU more aggressively.

**5.2 What it does badly**

* **Apple-only** — useless on NVIDIA / AMD.
* **Smaller community than llama.cpp** — fewer model conversions available.
* **Server mode is minimal** — geared at single-user inference.

**5.3 When to pick MLX**

* **Apple Silicon deployment** — Mac Studio, MacBook Pro, deployed AI appliance on Mac mini.
* **Local dev on Mac** with high-fidelity inference (vs llama.cpp's Metal path).
* **Single-user, high-end Mac** scenario where you want to use 64 GB of unified memory for a 70B model.

---

</details>

## 6. 第二梯队

值得了解，很少作为首选：

* **LMDeploy**（[github.com/InternLM/lmdeploy](https://github.com/InternLM/lmdeploy)） —— InternLM 的 runtime，带 TurboMind kernel。在中文工作负载上表现强，与 InternLM 模型系列集成良好。在单张 A100 / H100 上有时比 vLLM 更快。
* **Hugging Face TGI**（[github.com/huggingface/text-generation-inference](https://github.com/huggingface/text-generation-inference)） —— 生产质量，但功能迭代速度已放缓；vLLM 已吃掉 TGI 的大部分午餐。
* **Triton Inference Server**（[github.com/triton-inference-server/server](https://github.com/triton-inference-server/server)） —— NVIDIA 的编排层。承载 TensorRT-LLM、vLLM 或其他任何东西。它本身不是大语言模型 runtime；而是*部署*层。
* **Ray Serve / Ray LLM** —— Anyscale 的分布式推理服务层。用于跨副本布线、多区域部署。

---

## 7. 决策矩阵

| 工作负载 | 硬件 | 首选 | 次选 | 备注 |
|----------|----------|------------|-------------|-------|
| 对话，稠密 70B，OpenAI-API 推理服务 | H100/H200，8× | **vLLM** | TensorRT-LLM | 混合工作负载选 vLLM；仅追求吞吐选 TRT-LLM |
| 对话，MoE（混合专家模型）200B+ | H200 / B200，8×+ | **SGLang** | vLLM | SGLang 的 EP 支持成熟 |
| 带结构化输出的 agent / tool-use | 任意 | **SGLang** | vLLM | XGrammar 集成快 |
| 批处理 / 离线 embedding | H100 | TensorRT-LLM | vLLM | TRT-LLM 吞吐占优 |
| 批处理 / 离线大语言模型 | H200 / B200 | **TensorRT-LLM** | vLLM | engine 编译有回报 |
| 大量前缀共享的 RAG（检索增强生成） | H100 / H200 | **SGLang** | vLLM | RadixAttention 占优 |
| 边缘 / 单用户 | Jetson、Mac、RPi | **llama.cpp** | MLX（Mac） | llama.cpp 通用 |
| Apple Silicon | M 系列 | **MLX** | llama.cpp | MLX 在 Apple GPU 上更快 |
| 多模态视觉 | H100 / H200 | **vLLM** | SGLang | vLLM 的多模态支持最成熟 |
| Blackwell 上的 MoE | B200 / GB200 | **SGLang** | TensorRT-LLM | SGLang EP + FP4 路径 |

起点——不是永恒真理。硬件或模型变化时重新 benchmark。

---

## 8. runtime 层的形态

所有五个 runtime 都有相同的架构骨架：

```text
HTTP / gRPC API
       │
       ▼
   scheduler  ──►  pending request queue
       │              │
       ▼              ▼
   model graph   ──►  batched forward pass
       │              │
       ▼              ▼
    kernels      ──►  attention, FFN, sampling
       │              │
       ▼              ▼
  KV manager    ──►  paged KV / radix tree / per-session
       │
       ▼
    HBM / fabric
```

差异在于：

* **调度器** —— vLLM 和 SGLang 有连续批处理；llama.cpp 是按会话；TRT-LLM 有连续批处理。
* **KV 管理器** —— vLLM 的 PagedAttention v2 vs SGLang 的 RadixAttention vs llama.cpp 的按会话。
* **Kernel** —— vLLM 使用 Triton + C++；SGLang 使用自己的 kernel；TRT-LLM 使用 NVIDIA kernel；llama.cpp 使用 GGML kernel。
* **量化** —— 每个都有自己的 AWQ / GPTQ / FP8 / GGUF 集成。

资深工程师能读懂任何 runtime 的源码，并定位每个模块。**初级工程师学一个 runtime；资深工程师学*这些模块***，并将其应用到交给他们的任何 runtime。

---

## Lab —— 在同一个模型上 benchmark 三个 runtime

目标：在相同机器、相同模型下，对三个 runtime 做对比。

1. **选择一个模型** —— Qwen3-4B Instruct（AWQ-INT4 或原始 BF16，二者均可）。
2. **选择三个 runtime** —— vLLM（V1）、llama.cpp（post-2026-04），以及以下之一：SGLang 或 TensorRT-LLM。
3. **相同硬件**、相同 warmup、相同 prompt 集。
4. **跑 benchmark**：
   * 对话形态：prompt 长度 512，输出 128，并发 1、8、32。
   * agent 形态：prompt 256，输出 16，并发 16（低延迟）。
5. **报告** TTFT、TPOT、吞吐、峰值 HBM，并给出一次 run 的性能分析器 trace。
6. **按形态选出胜者**，并用一段话论证。

通过标准：报告位于你的 benchmark repo。任何读它的人都能看出*为什么*某个 runtime 在某种形态下胜出。

---

## 自检

1. 你接到一个项目：在 8× H200 上为 agent 产品提供 Qwen3-MoE 235B-A22B 推理服务。你会从哪个 runtime 开始，在投入前检查什么？
2. 队友坚持在一个对话产品中使用 TensorRT-LLM，而该产品的模型每周都会发布一个新 fine-tune。用两句话支持或否定该选择。
3. 你要在 Jetson Orin Nano Super 8 GB 上交付一个大语言模型设备。选哪个 runtime、哪个模型规模、哪种量化？为什么？
4. SGLang 的 RadixAttention 在 RAG 上占优。为什么 RAG 比对话受益更明显？
5. 你的 benchmark 显示，在 H200 上单副本以 FP8 跑 Llama 3.3 70B 时，vLLM 为 920 tok/s，TRT-LLM 为 1430 tok/s。你仍然交付 vLLM。给出一个工程理由，说明这样做是对的。

---


<details>
<summary>English original</summary>

**6. The secondary tier**

Worth knowing, rarely the first pick:

* **LMDeploy** ([github.com/InternLM/lmdeploy](https://github.com/InternLM/lmdeploy)) — InternLM's runtime with TurboMind kernels. Strong on Chinese-language workloads, integrates well with the InternLM model family. Sometimes faster than vLLM on a single A100 / H100.
* **Hugging Face TGI** ([github.com/huggingface/text-generation-inference](https://github.com/huggingface/text-generation-inference)) — production-quality but feature pace has slowed; vLLM has eaten most of TGI's lunch.
* **Triton Inference Server** ([github.com/triton-inference-server/server](https://github.com/triton-inference-server/server)) — NVIDIA's orchestration layer. Hosts TensorRT-LLM, vLLM, or anything else. Not an LLM runtime itself; the *deployment* layer.
* **Ray Serve / Ray LLM** — Anyscale's distributed serving layer. Used for routing across replicas, multi-region deployments.

---

**7. The decision matrix**

| Workload | Hardware | First pick | Second pick | Notes |
|----------|----------|------------|-------------|-------|
| Chat, dense 70B, OpenAI-API serving | H100/H200, 8× | **vLLM** | TensorRT-LLM | vLLM if mixed workloads; TRT-LLM if throughput-only |
| Chat, MoE 200B+ | H200 / B200, 8×+ | **SGLang** | vLLM | SGLang's EP support is mature |
| Agent / tool-use with structured output | any | **SGLang** | vLLM | XGrammar integration is fast |
| Batch / offline embedding | H100 | TensorRT-LLM | vLLM | TRT-LLM throughput wins |
| Batch / offline LLM | H200 / B200 | **TensorRT-LLM** | vLLM | engine compile pays off |
| RAG with heavy prefix sharing | H100 / H200 | **SGLang** | vLLM | RadixAttention wins |
| Edge / single user | Jetson, Mac, RPi | **llama.cpp** | MLX (Mac) | llama.cpp is universal |
| Apple Silicon | M-series | **MLX** | llama.cpp | MLX is faster on Apple GPU |
| Multi-modal vision | H100 / H200 | **vLLM** | SGLang | vLLM's multi-modal support is most mature |
| MoE on Blackwell | B200 / GB200 | **SGLang** | TensorRT-LLM | SGLang EP + FP4 path |

Starting point — not eternal truth. Re-bench when your hardware or model changes.

---

**8. The shape of the runtime layer**

All five runtimes have the same architectural skeleton:

```text
HTTP / gRPC API
       │
       ▼
   scheduler  ──►  pending request queue
       │              │
       ▼              ▼
   model graph   ──►  batched forward pass
       │              │
       ▼              ▼
    kernels      ──►  attention, FFN, sampling
       │              │
       ▼              ▼
  KV manager    ──►  paged KV / radix tree / per-session
       │
       ▼
    HBM / fabric
```

Where they differ:

* **Scheduler** — vLLM and SGLang have continuous batching; llama.cpp is per-session; TRT-LLM has in-flight batching.
* **KV manager** — vLLM PagedAttention v2 vs SGLang RadixAttention vs llama.cpp per-session.
* **Kernels** — vLLM uses Triton + C++; SGLang uses its own kernels; TRT-LLM uses NVIDIA kernels; llama.cpp uses GGML kernels.
* **Quantization** — each has its own integration of AWQ / GPTQ / FP8 / GGUF.

A senior engineer can read any runtime's source and locate each box. **Junior engineers learn one runtime; seniors learn *the boxes*** and apply them to whichever runtime they're handed.

---

**Lab — bench three runtimes on one model**

Goal: produce a same-machine, same-model comparison across three runtimes.

1. **Pick one model** — Qwen3-4B Instruct (AWQ-INT4 or original BF16, both available).
2. **Pick three runtimes** — vLLM (V1), llama.cpp (post-2026-04), and one of: SGLang or TensorRT-LLM.
3. **Same hardware**, same warmup, same prompt set.
4. **Bench**:
   * Chat shape: prompt length 512, output 128, concurrency 1, 8, 32.
   * Agent shape: prompt 256, output 16, concurrency 16 (low-latency).
5. **Report** TTFT, TPOT, throughput, peak HBM, with profiler traces for one run.
6. **Pick a winner** per shape and defend in one paragraph.

Pass criterion: the report is in your benchmark repo. Anyone reading it can see *why* one runtime won at one shape.

---

**Self-check**

1. You receive a project: serve Qwen3-MoE 235B-A22B for an agent product on 8× H200. What runtime do you start with, and what do you check before committing?
2. A teammate insists on TensorRT-LLM for a chat product with a model that ships a new fine-tune every week. Defend or reject the choice in two sentences.
3. You are shipping an LLM appliance on Jetson Orin Nano Super 8 GB. Which runtime, which model size, which quantization? Why?
4. SGLang's RadixAttention wins on RAG. Why specifically does RAG benefit more than chat?
5. Your benchmark shows vLLM at 920 tok/s and TRT-LLM at 1430 tok/s on Llama 3.3 70B at FP8 on H200, single replica. You ship vLLM anyway. Give one engineering reason where this is the right call.

---

</details>

## 参考文献

* vLLM — [docs.vllm.ai](https://docs.vllm.ai) · GitHub [vllm-project/vllm](https://github.com/vllm-project/vllm)
* SGLang — [sgl-project.github.io](https://sgl-project.github.io/) · GitHub [sgl-project/sglang](https://github.com/sgl-project/sglang)
* TensorRT-LLM — [nvidia.github.io/TensorRT-LLM/](https://nvidia.github.io/TensorRT-LLM/) · GitHub [NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)
* llama.cpp — GitHub [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
* MLX — [ml-explore.github.io/mlx/](https://ml-explore.github.io/mlx/build/html/index.html) · GitHub [ml-explore/mlx](https://github.com/ml-explore/mlx)
* LMDeploy — GitHub [InternLM/lmdeploy](https://github.com/InternLM/lmdeploy)
* HF TGI — GitHub [huggingface/text-generation-inference](https://github.com/huggingface/text-generation-inference)
* Triton Inference Server — [docs.nvidia.com/deeplearning/triton-inference-server/](https://docs.nvidia.com/deeplearning/triton-inference-server/)

交叉引用：

* [阶段 5 → 边缘 AI → Qwen 推理优化](../../../03-方向C-边缘AI/05-Qwen推理优化/README.md) — 边缘端的 runtime + 模型实操
* [阶段 5 → ML Systems Engineering Guide → Stage 4 Inference Serving Systems](../../Guide.md)

---

## 截至 2026-06

版本锁定：vLLM 0.22.x V1、SGLang 0.5.x、TensorRT-LLM 1.3.x、llama.cpp post-2026-04、MLX 0.31.x。当 vLLM V1 作为无条件默认版本发布，或某个 runtime 发布破坏性 API 变更，或 TRT-LLM 的 Blackwell FP4 路径推出稳定版本时，更新本文。

---

## 下一章

* 下一章：[第 2 部分 — Hopper 上的 Dense](../02-Hopper稠密计算/README.md) — 具体的 Llama 3.3 70B ↔ Qwen 2.5 72B 实操
* 上一章：[Lecture 04 — 精度栈](Lecture-04.md)
* 返回：[第 1 部分 — 基础](README.md)


<details>
<summary>English original</summary>

**References**

* vLLM — [docs.vllm.ai](https://docs.vllm.ai) · GitHub [vllm-project/vllm](https://github.com/vllm-project/vllm)
* SGLang — [sgl-project.github.io](https://sgl-project.github.io/) · GitHub [sgl-project/sglang](https://github.com/sgl-project/sglang)
* TensorRT-LLM — [nvidia.github.io/TensorRT-LLM/](https://nvidia.github.io/TensorRT-LLM/) · GitHub [NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)
* llama.cpp — GitHub [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
* MLX — [ml-explore.github.io/mlx/](https://ml-explore.github.io/mlx/build/html/index.html) · GitHub [ml-explore/mlx](https://github.com/ml-explore/mlx)
* LMDeploy — GitHub [InternLM/lmdeploy](https://github.com/InternLM/lmdeploy)
* HF TGI — GitHub [huggingface/text-generation-inference](https://github.com/huggingface/text-generation-inference)
* Triton Inference Server — [docs.nvidia.com/deeplearning/triton-inference-server/](https://docs.nvidia.com/deeplearning/triton-inference-server/)

Cross-references:

* [Phase 5 → Edge AI → Qwen Inference Optimization](../../../03-方向C-边缘AI/05-Qwen推理优化/README.md) — runtime + model walkthroughs on edge
* [Phase 5 → ML Systems Engineering Guide → Stage 4 Inference Serving Systems](../../Guide.md)

---

**Current as of 2026-06**

Versions pinned: vLLM 0.22.x V1, SGLang 0.5.x, TensorRT-LLM 1.3.x, llama.cpp post-2026-04, MLX 0.31.x. Update when vLLM V1 ships as the unconditional default, or when a runtime ships a breaking API change, or when TRT-LLM's Blackwell FP4 path lands a stable release.

---

**Next**

* Next: [Part 2 — Dense at Hopper](../02-Hopper稠密计算/README.md) — concrete Llama 3.3 70B ↔ Qwen 2.5 72B walkthroughs
* Previous: [Lecture 04 — The precision stack](Lecture-04.md)
* Up: [Part 1 — Fundamentals](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 1 - Fundamentals/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%201%20-%20Fundamentals/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
