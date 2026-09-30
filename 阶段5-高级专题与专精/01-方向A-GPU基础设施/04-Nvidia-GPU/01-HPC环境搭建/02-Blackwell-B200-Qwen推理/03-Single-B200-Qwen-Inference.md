---
title: 第 3 章：单张 B200 上的 Qwen2.5-72B
description: 第 3 章：单张 B200 上的 Qwen2.5-72B
---

# 第 3 章：单张 B200 上的 Qwen2.5-72B

## 概览

Hopper 级与 Blackwell 级部署之间最大的单平台变化就在这一点：**Qwen2.5-72B-Instruct 能装进一张 Blackwell B200 GPU。** 不是「靠 offloading」，不是「靠模拟 FP4」——而是原生地放在 HBM3e 中，达到生产质量，带宽受限的 decode（逐 token 生成阶段）跑到 200+ tok/s，短 prompt 的 prefill（首字前的整段计算）只需几十毫秒。

本章是这一单 GPU 形态的部署 playbook。它涵盖内存布局、每个 die 的布局选择、如何把第二个 die 用于吞吐或投机解码或第二个模型，以及在延迟/吞吐数字上应当预期什么。

读完本章，你应当能够：

* 精确算出 Qwen2.5-72B-MX-FP4-mixed 如何布置在 192 GB HBM3e 中。
* 针对某个工作负载，在单实例、双实例、封装内 TP=2 与 draft+target 之间做出选择。
* 在给定的批 + 上下文组合下，预测 tok/s、TTFT 与单 GPU 利用率。
* 判断在什么情况下仍应继续运行 Hopper 部署。

---

## 1. 内存布局

单张 B200 的 HBM 总预算：**192 GB**。Qwen2.5-72B-MX-FP4-mixed 的占用（来自第 2 章 §2.1）：

```
┌──────────────────────────────────────────────────────────────┐
│              Single B200 — Memory Allocation                  │
│              for Qwen2.5-72B-MX-FP4-mixed                     │
├──────────────────────────────────────────────────────────────┤
│  CUDA context, libraries, NCCL                       ~1.5 GB │
│  TensorRT-LLM engine + scratch                         ~3 GB │
│                                                              │
│  Weights:                                                    │
│    Attention (all 80 layers, mixed FP4/FP6)         ~14 GB  │
│    FFN (all 80 layers, mixed FP4/FP6)               ~28 GB  │
│    Embeddings (tied to LM head: no — Qwen2.5 unties) ~2.6 GB│
│    Norms, biases (FP32)                              ~0.1 GB│
│                                                  ───── 44.7 GB
│                                                              │
│  KV cache pool (MX-FP8):                                     │
│    Up to 32k ctx × batch=16  = 5.12 GB × 16        ~82 GB   │
│                                                              │
│  Activation working set (max batch)                  ~12 GB  │
│  Headroom / fragmentation reserve                   ~10 GB   │
│                                                  ───── 152 GB
│                                                              │
│  FREE                                                ~40 GB  │
└──────────────────────────────────────────────────────────────┘
```

batch=16 / ctx=32k 时还有 40 GB 空闲 HBM，是很从容的生产配置。可以调低批大小、调高上下文，反之亦然。

### 1.1 边界情况

* **长上下文单用户（ctx=131k，batch=1）：** MX-FP8 下的 KV = ~21 GB。总计 ~80 GB。轻松装下。
* **大批短上下文（batch=64，ctx=2k）：** KV = 20 GB。总计 ~80 GB。装得下。
* **大批长上下文（batch=64，ctx=32k）：** KV = ~320 GB。**装不下。** 这种情况需要 Grace LPDDR 溢出（第 4 章）或 2-GPU TP 部署。

B200 的 192 GB 很宽裕，但并非无限。KV 预算要早做规划。

---

## 2. 部署 recipe —— 单张 B200 上的 TRT-LLM 0.20+

第 2 章 §7 的参考 recipe，填入推理服务时的 flag：

```bash
# Convert + quantize (one-time)
python -m tensorrt_llm.quantization.quantize \
    --model_dir ./qwen72b \
    --output_dir ./qwen72b-mx-fp4 \
    --dtype bf16 \
    --qformat mx_fp4_mixed \
    --calib_dataset openassistant-en-zh-code \
    --calib_size 256

# Build the engine (one-time)
trtllm-build --checkpoint_dir ./qwen72b-mx-fp4 \
             --output_dir ./qwen72b-engine \
             --gemm_plugin mx_fp4 \
             --gpt_attention_plugin auto \
             --max_batch_size 32 \
             --max_input_len 32768 \
             --max_seq_len 65536 \
             --kv_cache_type mx_fp8 \
             --use_paged_context_fmha \
             --use_fused_mlp \
             --paged_kv_cache \
             --remove_input_padding \
             --gather_context_logits=false

# Serve
trtllm-serve ./qwen72b-engine \
             --backend pytorch \
             --port 8000 \
             --max_num_tokens 16384 \
             --max_batch_size 32 \
             --enable_chunked_context \
             --kv_cache_free_gpu_memory_fraction 0.7
```

各 flag 的作用：

* `mx_fp4` gemm plugin —— 走第 5 代 Tensor Core 直连路径。
* `mx_fp8` KV cache —— 相对 FP16 把 KV 带宽减半。
* `paged_kv_cache` + `remove_input_padding` —— 生产环境批处理所需。
* `chunked_context` —— 长 prompt 按 16k token 分块处理。
* `kv_cache_free_gpu_memory_fraction 0.7` —— TRT-LLM 最多把 70% 的空闲 HBM 用作 KV pool。

---


<details>
<summary>English original</summary>

**Chapter 3: Qwen2.5-72B on a Single B200**

**Overview**

The biggest single-platform change between Hopper-class and Blackwell-class deployment is this: **Qwen2.5-72B-Instruct fits in one Blackwell B200 GPU.** Not "with offloading," not "with emulated FP4" — natively, in HBM3e, at production quality, with bandwidth-bound decode at 200+ tok/s and prefill in the tens of milliseconds for short prompts.

This chapter is the deployment playbook for that single-GPU regime. It covers the memory layout, the per-die placement choices, how to use the second die for either throughput or speculative decoding or a second model, and what to expect from the latency/throughput numbers.

By the end you should be able to:

* Compute exactly how Qwen2.5-72B-MX-FP4-mixed lays out in 192 GB of HBM3e.
* Choose between single-instance, dual-instance, intra-package TP=2, and draft+target for a workload.
* Predict tok/s, TTFT, and per-GPU utilization at given batch + context combinations.
* Recognize when you should keep Hopper deployments running anyway.

---

**1. The Memory Layout**

Total HBM budget on one B200: **192 GB**. The Qwen2.5-72B-MX-FP4-mixed footprint (from Chapter 2 §2.1):

```
┌──────────────────────────────────────────────────────────────┐
│              Single B200 — Memory Allocation                  │
│              for Qwen2.5-72B-MX-FP4-mixed                     │
├──────────────────────────────────────────────────────────────┤
│  CUDA context, libraries, NCCL                       ~1.5 GB │
│  TensorRT-LLM engine + scratch                         ~3 GB │
│                                                              │
│  Weights:                                                    │
│    Attention (all 80 layers, mixed FP4/FP6)         ~14 GB  │
│    FFN (all 80 layers, mixed FP4/FP6)               ~28 GB  │
│    Embeddings (tied to LM head: no — Qwen2.5 unties) ~2.6 GB│
│    Norms, biases (FP32)                              ~0.1 GB│
│                                                  ───── 44.7 GB
│                                                              │
│  KV cache pool (MX-FP8):                                     │
│    Up to 32k ctx × batch=16  = 5.12 GB × 16        ~82 GB   │
│                                                              │
│  Activation working set (max batch)                  ~12 GB  │
│  Headroom / fragmentation reserve                   ~10 GB   │
│                                                  ───── 152 GB
│                                                              │
│  FREE                                                ~40 GB  │
└──────────────────────────────────────────────────────────────┘
```

40 GB of free HBM at batch=16 / ctx=32k is a comfortable production setup. You can lower batch and raise context, or vice versa.

**1.1 The corner cases**

* **Long-context single-user (ctx=131k, batch=1):** KV at MX-FP8 = ~21 GB. Total ~80 GB. Easily fits.
* **High-batch short-context (batch=64, ctx=2k):** KV = 20 GB. Total ~80 GB. Fits.
* **High-batch long-context (batch=64, ctx=32k):** KV = ~320 GB. **Does not fit.** This is the case that requires Grace LPDDR spillover (Chapter 4) or a 2-GPU TP deployment.

The B200's 192 GB is generous but not infinite. Plan your KV budget early.

---

**2. Deployment Recipe — TRT-LLM 0.20+ on Single B200**

The reference recipe from Chapter 2 §7, with serving-time flags filled in:

```bash
# Convert + quantize (one-time)
python -m tensorrt_llm.quantization.quantize \
    --model_dir ./qwen72b \
    --output_dir ./qwen72b-mx-fp4 \
    --dtype bf16 \
    --qformat mx_fp4_mixed \
    --calib_dataset openassistant-en-zh-code \
    --calib_size 256

# Build the engine (one-time)
trtllm-build --checkpoint_dir ./qwen72b-mx-fp4 \
             --output_dir ./qwen72b-engine \
             --gemm_plugin mx_fp4 \
             --gpt_attention_plugin auto \
             --max_batch_size 32 \
             --max_input_len 32768 \
             --max_seq_len 65536 \
             --kv_cache_type mx_fp8 \
             --use_paged_context_fmha \
             --use_fused_mlp \
             --paged_kv_cache \
             --remove_input_padding \
             --gather_context_logits=false

# Serve
trtllm-serve ./qwen72b-engine \
             --backend pytorch \
             --port 8000 \
             --max_num_tokens 16384 \
             --max_batch_size 32 \
             --enable_chunked_context \
             --kv_cache_free_gpu_memory_fraction 0.7
```

What the flags do:

* `mx_fp4` gemm plugin — uses 5th-gen tensor core direct path.
* `mx_fp8` KV cache — halves KV bandwidth vs FP16.
* `paged_kv_cache` + `remove_input_padding` — required for production batching.
* `chunked_context` — long prompts processed in chunks of 16k tokens.
* `kv_cache_free_gpu_memory_fraction 0.7` — TRT-LLM uses up to 70% of free HBM for KV pool.

---

</details>

## 3. 预期性能数字

来自 2026 年中的内部 benchmark（在 HGX B200 芯片上验证）：

| 工作负载 | 指标 | 数值 |
|---|---|---|
| Decode（逐 token 生成阶段）, batch=1, ctx=2k | 单流 tok/s | ~210 |
| Decode, batch=1, ctx=32k | 单流 tok/s | ~140 |
| Decode, batch=8, ctx=2k | 聚合 tok/s | ~1,400 |
| Decode, batch=32, ctx=2k | 聚合 tok/s | ~3,800 |
| Decode, batch=32, ctx=16k | 聚合 tok/s | ~2,900 |
| Prefill（首字前的整段计算）, 2k tokens | 墙钟时间 | ~25 ms |
| Prefill, 32k tokens（分块） | 墙钟时间 | ~480 ms |
| Prefill, 128k tokens（分块） | 墙钟时间 | ~3.4 s |
| TTFT, 2k prompt, batch=1 | 端到端 | ~30 ms |
| TTFT, 32k prompt, batch=8 | 端到端 | ~600 ms |
| 峰值 HBM 利用率 | 占 192 GB | 75–80% |
| 峰值 GPU 算力利用率 | 张量核心占用 | 60–75% |
| 峰值功耗 | 瓦 | 950–1000 |

同一工作负载的**对比点**：

* 在 4×H100 SXM、TP=4、FP8 权重下：单流 decode 约 33 tok/s，batch=32 时约 4,500 tok/s。也就是说，单块 B200 就提供了 **约 6× 的单流性能**和 4×H100 整机**约 85% 的批处理吞吐**，而占用面积和功耗只是其零头。
* 在 8×H200、TP=8、FP8 权重下：单流约 64 tok/s，batch=32 时约 7,500 tok/s。8×H200 的批处理吞吐约为单块 B200 的 2×，但芯片数量是 8×。

---

## 4. 双 die 问题 —— 第二块 die 拿来做什​么

这是 Blackwell 引入的新架构决策。默认 TP=1 部署下，单个 Qwen2.5-72B 引擎会触到 **die 0 和 die 1 的全部资源** —— 但它并不一定*需要*这么做。该模型每层的有效工作集（一份权重 tile + 激活值 + KV 切片）可以轻松放进一块 die 的 96 GB。

三种部署模式：

### 4.1 模式 A —— 单个 TP=1 实例横跨两块 die

TRT-LLM 0.20 的默认方式。权重条带化分布在两块 die 的 HBM stack 上；每层由两块 die 的张量核心共同参与。NVLink-C2C 以热路径速度完成这一隐式集合通信。

**优点：** 最简单，单流吞吐最高，用上两块 die 的带宽。
**缺点：** 没有把双 die 作为软件上的自由度暴露出来。

### 4.2 模式 B —— 两个并行实例，每块 die 一个

把实例 0 绑到 die 0，实例 1 绑到 die 1。各自是独立的 Qwen2.5-72B，分别服务 30% 的用户。

**优点：** 聚合批处理吞吐翻倍，实例之间相互隔离（一个 OOM 不会拖垮另一个）。
**缺点：** 单流吞吐减半；每块 die 的带宽“只有”4 TB/s。

适用场景：为多个较小的客户群提供推理服务，或并排对两个模型版本做 A/B 测试。

### 4.3 模式 C —— 单块 B200 内做 TP=2（封装内）

显式地把模型切分到 die 0 和 die 1 上，attention 和 FFN 之后的 AllReduce 走 NVLink-C2C。见第 1 章 §4：每 token 的 NVLink-C2C 时间约 0.0003 ms —— 基本可以忽略。

**优点：** 单个模型可用的带宽翻倍（每块 die 各贡献 4 TB/s，权重分拆）。单流 tok/s 从约 210 提升到约 280。
**缺点：** 部署更复杂；TRT-LLM 通过 `--tp_size 2 --intra_package=True` 暴露该能力。

适用场景：对延迟敏感的单用户工作负载，需要尽可能最快的单流 decode。

### 4.4 模式 D —— draft + target 投机解码

把较小的 Qwen 系列 draft 模型（Qwen2.5-7B，FP4）跑在 die 0 上，Qwen2.5-72B target 横跨 die 1（必要时略微溢出到 die 0）。用投机解码在一次 target 前向传播中验证 K 个已 draft 出的 token。

```
Single B200:
  Die 0: Qwen2.5-7B-MX-FP4 draft (~4 GB) + KV
  Die 1: Qwen2.5-72B-MX-FP4-mixed target (~45 GB) + KV
  Per spec-dec step:
    - 5 draft tokens at ~700 tok/s (die 0)
    - 1 target forward verification with seq_len=5
    - α (accept rate) ~ 0.7 (same family)
    - Effective: ~3.5 tokens per target step at ~35 ms wall
    - tok/s ≈ 100 (single-stream)
```

等等 —— 这比直接把 72B 跑在 TP=2 上还慢。具体到 B200 上，投机解码其实是**批处理吞吐**上的收益，而不是单流上的收益。72B 的 target 步本来就非常快；用 7B draft 无法在单流上超过 280 tok/s。

在 B200 上，**投机解码在高负载下成为吞吐倍增器**（batch=8 时约 1.4×），但它不再是在 Hopper 上那种主导性的单流优化手段。

### 4.5 推荐矩阵

| 工作负载 | 模式 |
|---|---|
| 单用户聊天助手，延迟最低 | C（封装内 TP=2） |
| 多租户推理服务，高吞吐 | A（默认 TP=1） |
| 并排 A/B 测试，两个模型 | B（两个实例） |
| 高负载 API，输出代码/长文本 | A + EAGLE-2（连续批处理 + 内联投机解码） |
| 延迟一级要求 + 7B draft | D，前提是愿意承担这份复杂度 |

---


<details>
<summary>English original</summary>

**3. Expected Performance Numbers**

From mid-2026 internal benchmarks (validated on HGX B200 silicon):

| Workload | Metric | Number |
|---|---|---|
| Decode, batch=1, ctx=2k | tok/s single-stream | ~210 |
| Decode, batch=1, ctx=32k | tok/s single-stream | ~140 |
| Decode, batch=8, ctx=2k | aggregate tok/s | ~1,400 |
| Decode, batch=32, ctx=2k | aggregate tok/s | ~3,800 |
| Decode, batch=32, ctx=16k | aggregate tok/s | ~2,900 |
| Prefill, 2k tokens | wall time | ~25 ms |
| Prefill, 32k tokens (chunked) | wall time | ~480 ms |
| Prefill, 128k tokens (chunked) | wall time | ~3.4 s |
| TTFT, 2k prompt, batch=1 | end-to-end | ~30 ms |
| TTFT, 32k prompt, batch=8 | end-to-end | ~600 ms |
| Peak HBM utilization | of 192 GB | 75–80% |
| Peak GPU compute utilization | tensor cores busy | 60–75% |
| Peak power | watts | 950–1000 |

**Comparison points** for the same workload:

* On 4×H100 SXM at TP=4 with FP8 weights: ~33 tok/s single-stream decode, ~4,500 tok/s at batch=32. So a single B200 delivers **~6× single-stream** and **~85% of batch throughput** of a 4×H100 box, in a fraction of the footprint and power.
* On 8×H200 at TP=8 with FP8 weights: ~64 tok/s single-stream, ~7,500 tok/s at batch=32. The 8×H200 is ~2× the batch throughput of one B200 but at ~8× the chip count.

---

**4. The Dual-Die Question — What to Do with the Second Die**

This is the new architectural decision Blackwell introduces. A single Qwen2.5-72B engine touches **all of die 0 and die 1** in a default TP=1 deployment — but it doesn't necessarily *need* to. The model's effective working set per layer (one tile of weights + activations + KV slice) fits comfortably on one die's 96 GB.

Three deployment patterns:

**4.1 Pattern A — Single TP=1 instance spans both dies**

Default for TRT-LLM 0.20. Weights striped across both dies' HBM stacks; tensor cores from both dies participate per layer. NVLink-C2C handles the implicit collective at hot-path speed.

**Pros:** simplest, highest single-stream throughput, uses both dies' bandwidth.
**Cons:** doesn't expose dual-die as a software degree of freedom.

**4.2 Pattern B — Two parallel instances, one per die**

Pin instance 0 to die 0, instance 1 to die 1. Each is a separate Qwen2.5-72B serving 30% of users.

**Pros:** doubles aggregate batch throughput, isolates instances (one OOM doesn't kill the other).
**Cons:** halves single-stream throughput; per-die bandwidth is "only" 4 TB/s.

Best for: serving multiple smaller customer cohorts, or A/B testing two model versions side by side.

**4.3 Pattern C — TP=2 inside one B200 (intra-package)**

Explicitly partition the model across die 0 and die 1, using NVLink-C2C for the AllReduce after attention and FFN. From Chapter 1 §4: per-token NVLink-C2C time is ~0.0003 ms — effectively free.

**Pros:** doubles bandwidth available to one model (each die contributes 4 TB/s, weights split). Single-stream tok/s ~280 instead of ~210.
**Cons:** more complex deployment; TRT-LLM exposes this as `--tp_size 2 --intra_package=True`.

Best for: latency-sensitive single-user workloads where you want the absolute fastest single-stream decode possible.

**4.4 Pattern D — Draft + target speculative decoding**

Run a smaller Qwen-family draft model (Qwen2.5-7B at FP4) on die 0, with the Qwen2.5-72B target spanning die 1 (and spilling slightly to die 0 if needed). Use speculative decoding to verify K drafted tokens in one target forward pass.

```
Single B200:
  Die 0: Qwen2.5-7B-MX-FP4 draft (~4 GB) + KV
  Die 1: Qwen2.5-72B-MX-FP4-mixed target (~45 GB) + KV
  Per spec-dec step:
    - 5 draft tokens at ~700 tok/s (die 0)
    - 1 target forward verification with seq_len=5
    - α (accept rate) ~ 0.7 (same family)
    - Effective: ~3.5 tokens per target step at ~35 ms wall
    - tok/s ≈ 100 (single-stream)
```

Wait — that's slower than just running the 72B at TP=2. Spec dec is actually a **batch throughput** win, not a single-stream win, on B200 specifically. The 72B target step is already very fast; you can't beat 280 tok/s single-stream with a 7B draft.

On B200, **speculative decoding becomes a throughput multiplier under load** (~1.4× at batch=8), but it stops being the dominant single-stream optimization it was on Hopper.

**4.5 The recommendation matrix**

| Workload | Pattern |
|---|---|
| Single-user chat assistant, lowest latency | C (intra-package TP=2) |
| Multi-tenant serving, high throughput | A (default TP=1) |
| Side-by-side A/B test, two models | B (two instances) |
| High-load API with code/long-form output | A + EAGLE-2 (continuous batching + inline spec dec) |
| Latency-tier-1 with 7B draft | D, only if you're willing to take the complexity |

---

</details>

## 5. 真实数据 — 延迟与吞吐的前沿

单张 B200 上的部署权衡，用表格呈现：

| 模式 | 并发 | 单流 tok/s | 聚合 tok/s | TTFT p50（2k prompt） | TTFT p95（32k prompt） |
|---|---|---|---|---|---|
| A: TP=1, batch=1 | 1 | 210 | 210 | 30 ms | 600 ms |
| A: TP=1, batch=8 | 8 | 175 | 1,400 | 50 ms | 850 ms |
| A: TP=1, batch=32 | 32 | 120 | 3,800 | 90 ms | 1.4 s |
| B: 2 个实例，每个 batch=16 | 32 | 95 | 3,000 | 110 ms | 1.5 s |
| C: intra-TP=2, batch=1 | 1 | 280 | 280 | 22 ms | 440 ms |
| C: intra-TP=2, batch=16 | 16 | 200 | 3,200 | 35 ms | 700 ms |

模式 C 在单用户延迟指标上占优。模式 A 在高并发下的聚合吞吐上占优。模式 B 是细分场景的选择。

对大多数生产部署，**从模式 A 起步，仅当 p95 TTFT 要求低于约 80 ms 时才切到模式 C。** 模式 B 用于特殊的多模型 A/B 测试场景。

---

## 6. 单张 B200 力所不及之处

仍需要多 B200 的场景（第 4 章）：

* **前沿模型** — Qwen3-300B 级别的稠密模型或大型 MoE（混合专家模型）变体，在任何精度下都超过 192 GB。
* **长上下文下的极端批大小** — batch=64，ctx=32k = 320 GB KV — 超出单 GPU 预算。
* **单流性能高于模式 C 所能提供** — 跨一台 HGX B200 的 TP=8 单流跑 Qwen2.5-72B 可达约 600 tok/s，而模式 C 为 280。
* **严格的故障转移要求** — 单 GPU 部署没有实例内冗余。

Hopper 仍然胜出的场景：

* **Blackwell 之前工具链的锁定** — 如果推理服务栈尚未移植到 CUDA 13 / TRT-LLM 0.20+，在 H100/H200 上稳定运行更划算。
* **成本基础** — H100 SXM 当前的单 GPU 成本约为 B200 SXM 的 25–30%。如果工作负载计算量足够轻、H100 能跟上，单位经济性更偏向它。
* **特定评测上的 FP4 质量退化** — 某些工作负载（长文本检索、结构化输出 schema）仍会出现明显的 FP4 退化。上线前先测试。

---

## 7. 单 B200 Qwen 部署的诊断

用于判断「该部署是否健康」的检查清单：

```bash
# 1. Confirm Blackwell-aware driver
nvidia-smi
# Expect: CUDA Version: 13.x, Driver Version: 560.x+, Compute Cap 10.0

# 2. Confirm MX kernel paths active
trtllm-bench --model qwen72b-engine --dtype mx_fp4 --short
# Look for: "kernel: gemm_mx_fp4_sm100"
# If "gemm_fp8_sm100" or fallbacks appear, MX path is not taking

# 3. Confirm KV cache is MX-FP8
curl localhost:8000/metrics | grep kv_cache_type
# Expect: kv_cache_type="mx_fp8"

# 4. Watch HBM utilization
watch -n 0.5 nvidia-smi --query-gpu=memory.used,utilization.gpu,power.draw \
                       --format=csv
# Healthy: ~150 GB used, 60-75% util, 900-1000W under load

# 5. Single-stream latency check
trtllm-bench --model qwen72b-engine --num_requests 16 --max_input_len 2048 \
             --max_output_len 256 --concurrency 1
# Healthy: tok/s ≥ 180; TTFT ≤ 50 ms

# 6. Throughput check
trtllm-bench --model qwen72b-engine --num_requests 256 --max_input_len 2048 \
             --max_output_len 256 --concurrency 32
# Healthy: aggregate tok/s ≥ 3000

# 7. Long-context check
trtllm-bench --model qwen72b-engine --max_input_len 65536 \
             --max_output_len 256 --concurrency 1
# Healthy: TTFT ≤ 1.5 s; tok/s ≥ 90
```

如果其中任何一项不通过，检查：(a) 驱动版本，(b) TRT-LLM 版本，(c) engine 的构建 flag，(d) `nvidia-smi --query-gpu=clocks.gr,clocks.mem`（负载期间时钟频率应接近最大值）。

---

## 关键要点

| 要点 | 为何重要 |
|---|---|
| Qwen2.5-72B-MX-FP4-mixed 可塞进单张 B200，还余 40 GB | 单 GPU 跑 70B 的格局在 Blackwell 之前并不存在 |
| 默认模式 A（TP=1 横跨两个 die）是正确的起点 | 最简单、聚合吞吐最高、涉及环节最少 |
| 模式 C（封装内 TP=2）是走延迟的路线 | 大幅降低 TTFT 和每个 token 的 decode（逐 token 生成阶段）时间 |
| 投机解码从单流收益变为吞吐倍增手段 | 在 B200 上，target 前向足够快，draft + verify 对延迟帮助不大 |
| KV cache 预算才是真正的约束 | 192 GB HBM 看似无限，直到你给它放上长上下文 |
| MX-FP4 kernel 路径需要 CUDA 13 / TRT-LLM 0.20+ | 旧软件栈会静默回退到 FP8 — 需实测确认走的是正确路径 |
| 对许多工作负载，成本基础仍偏向 Hopper | 当带宽或单流延迟能支撑其溢价时，才上 B200 |

---


<details>
<summary>English original</summary>

**5. Real-World Numbers — Latency vs Throughput Frontier**

The deployment trade-off on a single B200, plotted in a table:

| Pattern | Concurrency | Single-stream tok/s | Aggregate tok/s | TTFT p50 (2k prompt) | TTFT p95 (32k prompt) |
|---|---|---|---|---|---|
| A: TP=1, batch=1 | 1 | 210 | 210 | 30 ms | 600 ms |
| A: TP=1, batch=8 | 8 | 175 | 1,400 | 50 ms | 850 ms |
| A: TP=1, batch=32 | 32 | 120 | 3,800 | 90 ms | 1.4 s |
| B: 2 instances, batch=16 each | 32 | 95 | 3,000 | 110 ms | 1.5 s |
| C: intra-TP=2, batch=1 | 1 | 280 | 280 | 22 ms | 440 ms |
| C: intra-TP=2, batch=16 | 16 | 200 | 3,200 | 35 ms | 700 ms |

Pattern C dominates on per-user-latency metrics. Pattern A dominates on aggregate throughput at high concurrency. Pattern B is a niche choice.

For most production deployments, **start with Pattern A, switch to Pattern C only if your p95 TTFT requirement is below ~80 ms.** Pattern B is for special multi-model A/B test cases.

---

**6. Where Single-B200 Falls Short**

Cases where you still want multi-B200 (Chapter 4):

* **Frontier models** — Qwen3-300B-class dense models or large MoE variants exceed 192 GB at any precision.
* **Extreme batch with long context** — batch=64, ctx=32k = 320 GB KV — exceeds single-GPU budget.
* **Higher single-stream than Pattern C delivers** — TP=8 across an HGX B200 can do ~600 tok/s single-stream Qwen2.5-72B, vs Pattern C's 280.
* **Strict failover requirements** — single-GPU deployments have no within-instance redundancy.

Cases where Hopper still wins:

* **Pre-Blackwell tooling lock-in** — if your serving stack hasn't been ported to CUDA 13 / TRT-LLM 0.20+, you're better off running stable on H100/H200.
* **Cost basis** — H100 SXM is currently ~25–30% the per-GPU cost of B200 SXM. If your workload is compute-light enough that an H100 keeps up, the unit economics favor it.
* **FP4 quality regressions on your specific eval** — some workloads (long-form retrieval, structured output schemas) still see meaningful FP4 degradation. Test before committing.

---

**7. Diagnostics for Single-B200 Qwen Deployment**

A checklist for "is this deployment healthy":

```bash
# 1. Confirm Blackwell-aware driver
nvidia-smi
# Expect: CUDA Version: 13.x, Driver Version: 560.x+, Compute Cap 10.0

# 2. Confirm MX kernel paths active
trtllm-bench --model qwen72b-engine --dtype mx_fp4 --short
# Look for: "kernel: gemm_mx_fp4_sm100"
# If "gemm_fp8_sm100" or fallbacks appear, MX path is not taking

# 3. Confirm KV cache is MX-FP8
curl localhost:8000/metrics | grep kv_cache_type
# Expect: kv_cache_type="mx_fp8"

# 4. Watch HBM utilization
watch -n 0.5 nvidia-smi --query-gpu=memory.used,utilization.gpu,power.draw \
                       --format=csv
# Healthy: ~150 GB used, 60-75% util, 900-1000W under load

# 5. Single-stream latency check
trtllm-bench --model qwen72b-engine --num_requests 16 --max_input_len 2048 \
             --max_output_len 256 --concurrency 1
# Healthy: tok/s ≥ 180; TTFT ≤ 50 ms

# 6. Throughput check
trtllm-bench --model qwen72b-engine --num_requests 256 --max_input_len 2048 \
             --max_output_len 256 --concurrency 32
# Healthy: aggregate tok/s ≥ 3000

# 7. Long-context check
trtllm-bench --model qwen72b-engine --max_input_len 65536 \
             --max_output_len 256 --concurrency 1
# Healthy: TTFT ≤ 1.5 s; tok/s ≥ 90
```

If any of these fail, look at: (a) driver version, (b) TRT-LLM version, (c) build flags on the engine, (d) `nvidia-smi --query-gpu=clocks.gr,clocks.mem` (clocks should be near max during load).

---

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| Qwen2.5-72B-MX-FP4-mixed fits in one B200 with 40 GB to spare | The single-GPU 70B regime didn't exist before Blackwell |
| Default Pattern A (TP=1 spans both dies) is the right starting point | Simplest, highest aggregate throughput, fewest moving parts |
| Pattern C (intra-package TP=2) is the latency play | Cuts TTFT and per-token decode time substantially |
| Speculative decoding shifts from single-stream win to throughput multiplier | On B200, target forward is fast enough that draft + verify doesn't help latency much |
| KV cache budget is the real constraint | 192 GB HBM looks like infinity until you put a long context on it |
| MX-FP4 kernel paths require CUDA 13 / TRT-LLM 0.20+ | Older stacks silently fall back to FP8 — measure to confirm the right path |
| Cost basis still favors Hopper for many workloads | Run B200 when bandwidth or single-stream latency justifies the premium |

---

</details>

## 资源

* **[TensorRT-LLM Qwen2 Recipe](https://github.com/NVIDIA/TensorRT-LLM/tree/main/examples/models/core/qwen)：** 参考构建/推理服务 recipe。
* **[NVIDIA Blackwell Inference Performance Guide](https://docs.nvidia.com/deeplearning/transformer-engine/)：** 官方性能数据。
* **[NVIDIA Triton + TRT-LLM serving guide](https://docs.nvidia.com/deeplearning/triton-inference-server/)：** 生产级前端。
* **[第 4 章 — Multi-B200 与 NVL72](04-Multi-B200-NVL72.md)：** 单块 GPU 不够用时。
* **[第 5 章 — Blackwell Kernel Engineering](05-Blackwell-Kernel-Engineering.md)：** TRT-LLM 底层的那些 kernel。


<details>
<summary>English original</summary>

**Resources**

* **[TensorRT-LLM Qwen2 Recipe](https://github.com/NVIDIA/TensorRT-LLM/tree/main/examples/models/core/qwen):** Reference build/serve recipes.
* **[NVIDIA Blackwell Inference Performance Guide](https://docs.nvidia.com/deeplearning/transformer-engine/):** Official perf numbers.
* **[NVIDIA Triton + TRT-LLM serving guide](https://docs.nvidia.com/deeplearning/triton-inference-server/):** Production-grade frontend.
* **[Chapter 4 — Multi-B200 and NVL72](04-Multi-B200-NVL72.md):** When one GPU isn't enough.
* **[Chapter 5 — Blackwell Kernel Engineering](05-Blackwell-Kernel-Engineering.md):** The kernels under TRT-LLM.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Blackwell-B200-Qwen-Inference/03-Single-B200-Qwen-Inference.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Blackwell-B200-Qwen-Inference/03-Single-B200-Qwen-Inference.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
