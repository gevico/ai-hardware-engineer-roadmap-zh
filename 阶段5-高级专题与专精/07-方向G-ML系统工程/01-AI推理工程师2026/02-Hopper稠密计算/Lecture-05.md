---
title: 第 2 部分 · 第 05 讲——现代推理服务栈：连续批处理、Paged KV、前缀缓存、投机解码
description: 第 2 部分 · 第 05 讲——现代推理服务栈：连续批处理、Paged KV、前缀缓存、投机解码
---

# 第 2 部分 · 第 05 讲——现代推理服务栈：连续批处理、Paged KV、前缀缓存、投机解码

## 概述

Hopper 上的 70B 稠密模型，经 TP 切分并由 AWQ-INT4 或 FP8 量化后，会给你 *单副本峰值*。要构建生产级推理服务系统，你需要四项现代推理服务特性，它们把大语言模型推理从研究演示变成了产业：

1. **连续批处理**——动态地在同一批中混合 prefill（首字前的整段计算）与 decode（逐 token 生成阶段），每一步重新组合。
2. **PagedAttention v2**——分页、块级 KV cache 管理，消除 HBM 碎片。
3. **前缀缓存 / RadixAttention**——在具有重叠前缀的请求之间共享 KV 条目。
4. **投机解码**——通过草稿模型在每次前向传播中生成多个 token。

每一项在发布时（2023–2025）都独立将一项主要指标提升了 **2–10×**。它们共同定义了 **2026 推理服务基线**。不使用它们的部署，正以本可达到的 **10–50× 成本** 交付。

本讲将每项应用于 4–8× H100/H200 上的 Llama 3.3 70B 和 Qwen 2.5 72B，并给出具体数字与 runtime 配置。

读完后，你应能为任一锚点模型启用每项特性，预测指标变化，并用测量验证。

---

## 1. 连续批处理

2023 年的突破。心智模型：

### 1.1 静态批处理解决不佳的问题

在连续批处理之前，一个请求作为自包含作业被派发：

```text
batch of 8 requests arrives → forward pass → all 8 finish (or pad) → next batch starts
```

问题：

* 如果请求 1 生成 50 个 token，而请求 8 生成 500 个 token，批会等待请求 8——请求 1 闲置在批底部。
* 吞吐受最长运行请求限制。
* GPU 利用率低——频繁重建。

### 1.2 连续批处理重构问题

每个请求被分解为 **步骤**。每一步，调度器选择哪些序列参与批：

```text
step t:    batch = [req1.t=8, req2.t=12, req3.t=0(prefill), req4.t=5, ...]
step t+1:  req1 finishes → batch = [req2.t=13, req3.t=1, req4.t=6, req5.t=0(prefill), ...]
step t+2:  req5 prefill finishes → batch = [req2.t=14, req3.t=2, req4.t=7, req5.t=1, ...]
```

* 一旦有槽位空出，新请求就加入批。
* 完成的请求立即离开。
* 吞吐持续接近单批峰值。

### 1.3 混合 prefill 与 decode

微妙难点在于：**prefill 算力受限，decode 带宽受限**。在同一步中混合二者可能拖住批。

vLLM 0.22+ V1 使用 **chunked prefill**——prefill 被切成与一个 decode 批行大小相同的块。prefill 变成“不过是另一个 decode 形状的步骤”。这使批保持均衡，并让 GPU 处于 **高利用率**。

### 1.4 吞吐影响

对于 4× H100 上的 Llama 3.3 70B，混合聊天流量（平均 prompt 1024，平均 output 256，并发 32）：

| 调度 | 吞吐 tok/s/GPU | 备注 |
|------------|----------------------|-------|
| 静态批处理 | ~140 | 受长尾请求拖累 |
| 连续批处理 | ~300 | 提升 2.1× |
| 连续批处理 + chunked prefill | ~360 | 批组成平滑再带来 20% |

连续批处理在 vLLM、SGLang 和 TRT-LLM 中默认开启。没有充分理由禁用它。

---

## 2. PagedAttention v2——块级 KV 内存

第二个突破。心智模型：

### 2.1 KV 内存碎片问题

在分页之前，每个请求 **连续** 预留 KV 内存。如果分配了 4096 个 token 的 KV，而请求在第 312 个 token 完成，其余部分（3784 个 token × 320 KB ≈ 1.2 GB）在请求释放前一直 **被浪费**。

在许多请求之间，HBM 看起来像瑞士奶酪：满是空洞。**有效容量可能只有物理容量的 50%。**

### 2.2 PagedAttention v1

每个请求的 KV 被分割为固定大小的 **块**（例如每块 16 个 token）。随着序列增长，块从池中分配。

* 无碎片——每个块要么被完全使用，要么可用。
* 用于 KV 的有效 HBM 容量：物理容量的 95–98%（从 50–70% 下降）。
* 成本：间接块寻址带来约 3% 的 kernel 开销。

### 2.3 PagedAttention v2

在 vLLM 0.6+ 中加入。改进：

* 更小的块（4 或 8 个 token），以实现更细粒度分配。
* attention kernel 中更好的缓存利用率。
* 更低开销。

对于 128K 上下文、batch=8 下的 Llama 3.3 70B / Qwen 2.5 72B：

| 内存模式 | 有效 KV 容量 | 128K 下最大并发请求数 |
|-------------|----------------------|----------------------------------|
| 连续 | ~50% | ~3 |
| PagedAttention v1 | ~95% | ~6 |
| PagedAttention v2 | ~97% | ~6（分配更平滑） |

**没有分页，Hopper 上的长上下文推理服务不切实际。** vLLM、SGLang、TRT-LLM 和 llama.cpp 都实现了 paged KV。


<details>
<summary>English original</summary>

**Part 2 · Lecture 05 — The Modern Serving Stack: Continuous Batching, Paged KV, Prefix Cache, Speculation**

**Overview**

A 70B dense model on Hopper, partitioned by TP and quantized by AWQ-INT4 or FP8, will give you the *single-replica peak*. To get to a production-grade serving system you need the four modern serving features that turned LLM inference from a research demo into an industry:

1. **Continuous batching** — dynamically mix prefill and decode in one batch, recomposed every step.
2. **PagedAttention v2** — paged, block-level KV cache management that eliminates HBM fragmentation.
3. **Prefix caching / RadixAttention** — share KV entries across requests with overlapping prefixes.
4. **Speculative decoding** — generate multiple tokens per forward pass via a draft model.

Each one independently moved a major metric by **2–10×** when it shipped (2023–2025). Together they define the **2026 serving baseline**. A deployment that doesn't use them is shipping at **10–50× the cost** it could.

This lecture applies each to Llama 3.3 70B and Qwen 2.5 72B on 4–8× H100/H200, with concrete numbers and the runtime configs.

By the end you should be able to enable each feature for either anchor model, predict the metric movement, and verify with measurement.

---

**1. Continuous batching**

The breakthrough of 2023. The mental model:

**1.1 The problem static batching solves badly**

Before continuous batching, a request was dispatched as a self-contained job:

```text
batch of 8 requests arrives → forward pass → all 8 finish (or pad) → next batch starts
```

Problems:

* If request 1 generates 50 tokens and request 8 generates 500 tokens, the batch waits for request 8 — request 1 sits at the bottom of the batch idle.
* Throughput is bounded by the longest-running request.
* GPU utilization is poor — frequent rebuilds.

**1.2 Continuous batching reframes the problem**

Each request is decomposed into **steps**. At each step, the scheduler picks which sequences participate in the batch:

```text
step t:    batch = [req1.t=8, req2.t=12, req3.t=0(prefill), req4.t=5, ...]
step t+1:  req1 finishes → batch = [req2.t=13, req3.t=1, req4.t=6, req5.t=0(prefill), ...]
step t+2:  req5 prefill finishes → batch = [req2.t=14, req3.t=2, req4.t=7, req5.t=1, ...]
```

* New requests join the batch as soon as a slot opens.
* Finished requests leave immediately.
* Throughput approaches the single-batch peak continuously.

**1.3 Mixing prefill and decode**

The subtle hard part: **prefill is compute-bound, decode is memory-bound**. Mixing them in the same step can stall the batch.

vLLM 0.22+ V1 uses **chunked prefill** — prefill is split into chunks the size of a decode batch row. Prefill becomes "just another decode-shaped step." This keeps the batch balanced and the GPU at **high utilization**.

**1.4 Throughput impact**

For Llama 3.3 70B on 4× H100 with mixed chat traffic (mean prompt 1024, mean output 256, concurrency 32):

| Scheduling | Throughput tok/s/GPU | Notes |
|------------|----------------------|-------|
| Static batch | ~140 | dominated by stragglers |
| Continuous batching | ~300 | 2.1× improvement |
| Continuous + chunked prefill | ~360 | another 20% from smooth batch composition |

Continuous batching is on by default in vLLM, SGLang, and TRT-LLM. There is no good reason to disable it.

---

**2. PagedAttention v2 — block-level KV memory**

The second breakthrough. The mental model:

**2.1 The KV memory fragmentation problem**

Before paging, each request reserved KV memory **contiguously**. If you allocated 4096 tokens of KV and the request finished at token 312, the rest (3784 tokens × 320 KB ≈ 1.2 GB) was **wasted** until the request released.

Across many requests, HBM looks Swiss-cheese: full of holes. **Effective capacity might be 50% of physical.**

**2.2 PagedAttention v1**

Each request's KV is split into fixed-size **blocks** (e.g., 16 tokens per block). Blocks are allocated from a pool as the sequence grows.

* No fragmentation — every block is fully used or available.
* Effective HBM capacity for KV: 95–98% of physical (down from 50–70%).
* Cost: ~3% kernel overhead from indirect block addressing.

**2.3 PagedAttention v2**

Added in vLLM 0.6+. Improvements:

* Smaller blocks (4 or 8 tokens) for finer-grained allocation.
* Better cache utilization in the attention kernel.
* Lower overhead.

For Llama 3.3 70B / Qwen 2.5 72B at 128K context with batch=8:

| Memory mode | Effective KV capacity | Max concurrent requests at 128K |
|-------------|----------------------|----------------------------------|
| Contiguous | ~50% | ~3 |
| PagedAttention v1 | ~95% | ~6 |
| PagedAttention v2 | ~97% | ~6 (smoother allocation) |

**Without paging, long-context serving on Hopper is not practical.** vLLM, SGLang, TRT-LLM, and llama.cpp all implement paged KV.

</details>

### 2.4 共享 kernel 层 — FlashInfer

2025 年值得注意的转变：paged-attention（以及采样） kernel 越来越多地*不是*各个 runtime 定制的 CUDA。**FlashInfer**（[arXiv:2501.01005](https://arxiv.org/abs/2501.01005), MLSys 2025; Apache-2.0, `flashinfer-ai`]）是一个开源、JIT 编译的 attention + 采样 **kernel engine**，vLLM、SGLang、TensorRT-LLM 和 MLC-LLM 都基于它构建。它提供：

* **Paged / ragged attention kernel**，用于 prefill（首字前的整段计算） *和* decode（逐 token 生成阶段），作用于 block-sparse（page-table） KV 布局 — 即那些让 PagedAttention 变快的 kernel，包括上面提到的间接块寻址。
* **无序 top-k / top-p 采样 kernel**（采样步骤，而不仅是 attention）。
* **可定制、JIT 编译的 attention 变体**（不同的 mask、RoPE 形状、head 几何、FP8 KV），无需为每种情况手写 CUDA。

这对你为何重要：kernel 级别的改进（新的 attention 变体、Blackwell 调优路径）会同时惠及*多个* runtime，而“选用哪个 attention 后端”变成了一个需要固定和性能剖析的配置/版本旋钮 — 而不是黑盒。当你设置 `VLLM_ATTENTION_BACKEND=FLASHINFER` 或 SGLang 的 `--attention-backend flashinfer` 时，你选择的就是这个引擎。

---

## 3. 前缀缓存 — RadixAttention 及其同类

2024 年的突破。许多请求**共享前缀** — 系统提示词、RAG（检索增强生成）检索、对话历史。为这些前缀重新计算 KV 是**纯粹的浪费**。

### 3.1 简单前缀缓存（vLLM）

请求前缀被哈希。如果哈希值与缓存中已有的 KV 块序列匹配，则复用缓存的 KV。请求只需为其*新* token 计算 KV。

* 对于具有 1000 token 系统提示词和 100 token 用户轮次的聊天产品：前缀缓存命中可节省 90% 的 prefill 成本。
* 对于每个查询嵌入 5 篇检索文档的 RAG 产品：命中率取决于检索重叠程度，会有所变化。

### 3.2 RadixAttention（SGLang）

更通用的结构：一棵由前缀 token 索引的 **radix tree**。任意两个请求之间的任何公共前缀都能命中缓存，而不仅仅是系统提示词。

```text
        ┌── "What is..."  (5 requests share this 7-token prefix)
        │
"You are a helpful   ┤
 assistant. Respond  │
 in English. "       └── "Translate..." (3 requests)

Both branches share the system prompt KV — only the branch-specific tokens
needed fresh prefill.
```

* **在 RAG、agent、多轮聊天中获胜**，其中前缀会分支到许多请求。
* **收益因子：** 在高前缀重叠工作负载下，prefill 吞吐提升 2-10×。

### 3.3 可衡量的影响

对于客户支持聊天产品（长系统提示词 + 工具目录）上的 Qwen 2.5 72B：

| 前缀缓存 | 首 token 时延 | Prefill 成本 |
|--------------|------|--------------|
| 关闭 | 480 ms | 完整 |
| vLLM 哈希前缀缓存 | 90 ms | 完整的 19% |
| SGLang RadixAttention | 85 ms | 完整的 18% |

对于单段系统提示词，影响较小（首 token 时延改善约 30-50%）。对于**长系统提示词 + 工具目录**（典型的 agent / 聊天），影响**巨大**。

### 3.4 配置

vLLM：

```python
LLM(model="...", enable_prefix_caching=True)
```

SGLang：通过 RadixAttention 默认开启。

TRT-LLM 1.3+：在 runtime 中通过 `--use_paged_context_fmha`、`kv_cache_reuse=True` 支持。

---

## 4. 投机解码

2024–2025 年在 decode 吞吐上的突破。心智模型：

### 4.1 思想

一个小型 **draft model** 提出接下来的 k 个 token。完整的 **target model** 在一次前向传播中验证所有 k 个 token：

```text
draft model: emits 5 candidate tokens (cheap, fast)
target model: prefills "previous tokens + 5 candidates" and gets logits for each position
              accept tokens 1..n where draft prediction matches target's argmax
              reject from token n+1
```

* 最佳情况：draft 正确预测全部 5 个 token → 在 1 次 target 前向传播中生成 5 个 token。
* 最差情况：draft 在 token 1 预测错误 → 1 个 token（无收益）。
* 配对良好的 draft+target 的平均值：**接受率 60-80%**，带来 2-4× 的有效吞吐。

### 4.2 Draft model 选择

对于 Llama 3.3 70B target：

* **Llama 3.2 1B Instruct** 作为 draft — 小 70×，约 0.5 ms/token 的 draft 延迟，接受率约 65%。
* **Llama 3.2 3B Instruct** 作为 draft — 小 23×，约 1.5 ms/token，接受率约 75%。

对于 Qwen 2.5 72B target：

* **Qwen 2.5 1.5B Instruct** 作为 draft — 接受率约 75%。
* **Qwen 2.5 3B Instruct** 作为 draft — 接受率约 80%。

规律：draft 与 target 来自**同一家族**。**跨家族 drafting**（Llama draft，Qwen target）会损失接受率。


<details>
<summary>English original</summary>

**2.4 The shared kernel layer — FlashInfer**

A 2025 shift worth knowing: the paged-attention (and sampling) kernels are increasingly *not* each runtime's bespoke CUDA. **FlashInfer** ([arXiv:2501.01005](https://arxiv.org/abs/2501.01005), MLSys 2025; Apache-2.0, `flashinfer-ai`) is an open-source, JIT-compiled attention + sampling **kernel engine** that vLLM, SGLang, TensorRT-LLM, and MLC-LLM all build on. It provides:

* **Paged / ragged attention kernels** for prefill *and* decode over block-sparse (page-table) KV layouts — i.e., the kernels that make PagedAttention fast, including the indirect block addressing referenced above.
* **Sorting-free top-k / top-p sampling kernels** (the sampling step, not just attention).
* **Customizable, JIT-compiled attention variants** (different masks, RoPE shapes, head geometries, FP8 KV) without hand-writing CUDA per case.

Why this matters to you: a kernel-level win (a new attention variant, a Blackwell-tuned path) lands across *multiple* runtimes at once, and "which attention backend" becomes a config/version knob to pin and profile — not a black box. When you set `VLLM_ATTENTION_BACKEND=FLASHINFER` or SGLang's `--attention-backend flashinfer`, this is the engine you are selecting.

---

**3. Prefix caching — RadixAttention and friends**

The 2024 breakthrough. Many requests **share prefixes** — system prompts, RAG retrievals, conversation history. Recomputing KV for these prefixes is **pure waste**.

**3.1 Simple prefix cache (vLLM)**

A request prefix is hashed. If the hash matches an existing KV block sequence in the cache, the cached KV is reused. The request only needs to compute KV for its *new* tokens.

* For a chat product with 1000-token system prompts and 100-token user turns: prefix cache hit saves 90% of prefill cost.
* For a RAG product where each query embeds 5 retrieved docs: variable hit rate depending on retrieval overlap.

**3.2 RadixAttention (SGLang)**

A more general structure: a **radix tree** indexed by prefix tokens. Any common prefix between any two requests hits the cache, not just the system prompt.

```text
        ┌── "What is..."  (5 requests share this 7-token prefix)
        │
"You are a helpful   ┤
 assistant. Respond  │
 in English. "       └── "Translate..." (3 requests)

Both branches share the system prompt KV — only the branch-specific tokens
needed fresh prefill.
```

* **Wins for RAG, agent, multi-turn chat** where prefixes branch into many requests.
* **Win factor:** 2-10× prefill throughput at high prefix-overlap workloads.

**3.3 Measurable impact**

For Qwen 2.5 72B on a customer-support chat product (long system prompt + tool catalog):

| Prefix cache | TTFT | Prefill cost |
|--------------|------|--------------|
| Off | 480 ms | full |
| vLLM hash prefix cache | 90 ms | 19% of full |
| SGLang RadixAttention | 85 ms | 18% of full |

For a one-paragraph system prompt the impact is smaller (~30-50% TTFT improvement). For **long system prompts + tool catalogs** (typical agent / chat) the impact is **dramatic**.

**3.4 Configuration**

vLLM:

```python
LLM(model="...", enable_prefix_caching=True)
```

SGLang: on by default via RadixAttention.

TRT-LLM 1.3+: supported via `--use_paged_context_fmha`, `kv_cache_reuse=True` at runtime.

---

**4. Speculative decoding**

The 2024–2025 breakthrough on decode throughput. The mental model:

**4.1 The idea**

A small **draft model** proposes the next k tokens. The full **target model** verifies all k in one forward pass:

```text
draft model: emits 5 candidate tokens (cheap, fast)
target model: prefills "previous tokens + 5 candidates" and gets logits for each position
              accept tokens 1..n where draft prediction matches target's argmax
              reject from token n+1
```

* Best case: draft predicts all 5 correctly → 5 tokens generated in 1 target forward pass.
* Worst case: draft mispredicts token 1 → 1 token (no win).
* Average for well-paired draft+target: **acceptance rate 60-80%**, giving 2-4× effective throughput.

**4.2 Draft model choice**

For Llama 3.3 70B target:

* **Llama 3.2 1B Instruct** as draft — 70× smaller, ~0.5 ms/token draft latency, acceptance ~65%.
* **Llama 3.2 3B Instruct** as draft — 23× smaller, ~1.5 ms/token, acceptance ~75%.

For Qwen 2.5 72B target:

* **Qwen 2.5 1.5B Instruct** as draft — ~75% acceptance.
* **Qwen 2.5 3B Instruct** as draft — ~80% acceptance.

The pattern: draft from the **same family** as target. **Cross-family drafting** (Llama draft, Qwen target) loses acceptance.

</details>

### 4.3 EAGLE / EAGLE-2 / EAGLE-3

[EAGLE](https://arxiv.org/abs/2401.15077) 及其后继版本使用在目标模型分布上训练的*学习式*推测头。EAGLE-3（[arXiv:2503.01840](https://arxiv.org/abs/2503.01840)）是 2025 年的 state-of-the-art：

* 各类别的接受率为 75-85%。
* 无独立的 draft 模型 —— 推测头与目标模型融合。
* 对话工作负载的有效 decode（逐 token 生成阶段）吞吐提升 2.5-3.5×。

截至 2026 年年中，vLLM 与 SGLang 均支持 EAGLE-2 / EAGLE-3。

### 4.4 推测何时有害

* **批大小极低**（1-2 个请求）：推测增加延迟却省不了多少，因为验证很短。
* **decode 极短**（输出 <10 token）：draft 成本摊薄得很差。
* **高接受率情形**属于例外；对于低接受率工作负载（高度创造性的任务、非英文代码），推测可能只有非推测的 0.7×。

**按工作负载验证。** 对话场景 2.5× 的加速未必能迁移到输出 JSON 的 agent。

### 4.5 吞吐影响

Llama 3.3 70B 在 4× H100 FP8 对话工作负载下（并发 32，prompt 1024，输出 256）：

| decode 方式 | 吞吐 tok/s/GPU |
|----------|----------------------|
| 贪心自回归 | ~580 |
| Llama 3.2 1B draft + Llama 3.3 70B target | ~870（1.5×） |
| EAGLE-3 推测头 | ~1450（2.5×） |

在 Hopper 上，对于对话工作负载，推测是**最大的“免费”decode 优化**。

---

## 5. 四特性叠加

在 4× H100 上对 Llama 3.3 70B FP8 对话工作负载组合启用：

```text
baseline (vLLM 0.22, no advanced features):       ~250 tok/s/GPU
+ continuous batching:                           ~580 tok/s/GPU  (already on by default)
+ PagedAttention v2:                             ~610 tok/s/GPU  (3-5% from less fragmentation)
+ prefix cache:                                  ~700 tok/s/GPU  (15-20% for long system prompts)
+ EAGLE-3 speculation:                           ~1450 tok/s/GPU (2.07× from speculation)
```

叠加后的组合约为无特性基线的 6×。**这就是相比朴素部署的推理工程优势。**

---

## 6. 配置速查表 —— vLLM

```python
from vllm import LLM, SamplingParams

llm = LLM(
    model="meta-llama/Llama-3.3-70B-Instruct",
    tensor_parallel_size=4,
    dtype="float16",
    
    # continuous batching: on by default in V1, no flag needed
    
    # PagedAttention v2: on by default
    block_size=16,                     # default; smaller = finer paging
    
    # prefix cache: enable explicitly
    enable_prefix_caching=True,
    
    # speculation: pick one
    speculative_model="meta-llama/Llama-3.2-1B-Instruct",
    num_speculative_tokens=5,
    # OR EAGLE-3 once landed:
    # speculative_config={"method": "eagle3", ...}
    
    # chunked prefill (recommended for long prompts):
    enable_chunked_prefill=True,
    max_num_batched_tokens=8192,
    
    # KV quantization (optional):
    kv_cache_dtype="fp8_e5m2",
    
    # serving config:
    gpu_memory_utilization=0.92,
    max_num_seqs=128,
)
```

对于 Qwen 2.5 72B，替换模型路径并将 `Qwen/Qwen2.5-1.5B-Instruct` 用作 draft。

---

## 7. 启用顺序

一个务实的顺序：

1. **连续批处理 + PagedAttention v2** —— 默认已开启；用 profile 验证。
2. **Prefix caching** —— 单项 TTFT 收益最大；启用后实测。
3. **Chunked prefill**（首字前的整段计算） —— prompt 超过 2K token 时启用；实测对 TTFT 的影响。
4. **FP8 KV cache** —— 处于长上下文或大批场景时启用；实测精度一致性。
5. **推测** —— 放在最后，因为接受率的测量要求栈中其余部分已稳定。先用 draft 模型测量接受率；工作负载 profile 完成后切换到 EAGLE-3。

---

## Lab —— 逐项启用特性，测量每个指标的变化

目标：在固定工作负载上隔离每个特性对吞吐与 TTFT 的贡献。

1. **基线** —— 4× H100 上的 Llama 3.3 70B FP8，vLLM 0.22，除关闭各特性外为默认配置。
2. **加入连续批处理 + PagedAttention** —— 测量吞吐增量。
3. **加入 prefix cache** —— 在含 1500 token 系统提示词 + 100 token 用户轮次的工作负载上测量 TTFT 增量。
4. **加入 chunked prefill** —— 在 prompt 为 8K token 的工作负载上测量 TTFT 增量。
5. **加入 FP8 KV cache** —— 测量节省的 HBM，在 32K 上于 RULER 验证精度一致性。
6. **加入 1B 级别的 draft 模型** —— 测量吞吐增量，测量接受率。
7. **绘制**所有特性累计后的吞吐曲线。

通过标准：你有一张图，展示每个特性的独立贡献与累计结果。每项贡献都由一次测量支撑。

---


<details>
<summary>English original</summary>

**4.3 EAGLE / EAGLE-2 / EAGLE-3**

[EAGLE](https://arxiv.org/abs/2401.15077) and successors use a *learned* speculation head trained on the target model's distribution. EAGLE-3 ([arXiv:2503.01840](https://arxiv.org/abs/2503.01840)) is the 2025 state-of-the-art:

* Acceptance rates 75-85% across categories.
* No separate draft model — the head is fused with the target.
* 2.5-3.5× effective decode throughput for chat workloads.

vLLM and SGLang both support EAGLE-2 / EAGLE-3 as of mid-2026.

**4.4 When speculation hurts**

* **Very low batch** (1-2 requests): speculation adds latency without saving much because verification is short.
* **Very short decodes** (<10 tokens output): the draft cost amortizes poorly.
* **High-acceptance regimes** are exceptional; for low-acceptance workloads (very creative tasks, non-English code), speculation can be 0.7× of non-speculative.

**Validate per workload.** A 2.5× speedup on chat may not transfer to a JSON-emitting agent.

**4.5 Throughput impact**

For Llama 3.3 70B on 4× H100 FP8 chat workload (concurrency 32, prompt 1024, output 256):

| Decoding | Throughput tok/s/GPU |
|----------|----------------------|
| Greedy autoregressive | ~580 |
| Llama 3.2 1B draft + Llama 3.3 70B target | ~870 (1.5×) |
| EAGLE-3 head | ~1450 (2.5×) |

Speculation is the **biggest "free" decode optimization** on Hopper for chat workloads.

---

**5. The four-feature stack**

Combined for Llama 3.3 70B FP8 on 4× H100, chat workload:

```text
baseline (vLLM 0.22, no advanced features):       ~250 tok/s/GPU
+ continuous batching:                           ~580 tok/s/GPU  (already on by default)
+ PagedAttention v2:                             ~610 tok/s/GPU  (3-5% from less fragmentation)
+ prefix cache:                                  ~700 tok/s/GPU  (15-20% for long system prompts)
+ EAGLE-3 speculation:                           ~1450 tok/s/GPU (2.07× from speculation)
```

The composed stack is ~6× the no-feature baseline. **This is the inference-engineering edge over a naive deployment.**

---

**6. Configuration cheat sheet — vLLM**

```python
from vllm import LLM, SamplingParams

llm = LLM(
    model="meta-llama/Llama-3.3-70B-Instruct",
    tensor_parallel_size=4,
    dtype="float16",
    
    # continuous batching: on by default in V1, no flag needed
    
    # PagedAttention v2: on by default
    block_size=16,                     # default; smaller = finer paging
    
    # prefix cache: enable explicitly
    enable_prefix_caching=True,
    
    # speculation: pick one
    speculative_model="meta-llama/Llama-3.2-1B-Instruct",
    num_speculative_tokens=5,
    # OR EAGLE-3 once landed:
    # speculative_config={"method": "eagle3", ...}
    
    # chunked prefill (recommended for long prompts):
    enable_chunked_prefill=True,
    max_num_batched_tokens=8192,
    
    # KV quantization (optional):
    kv_cache_dtype="fp8_e5m2",
    
    # serving config:
    gpu_memory_utilization=0.92,
    max_num_seqs=128,
)
```

For Qwen 2.5 72B replace the model path and use `Qwen/Qwen2.5-1.5B-Instruct` as the draft.

---

**7. Order to enable**

A pragmatic order:

1. **Continuous batching + PagedAttention v2** — already on by default; verify with a profile.
2. **Prefix caching** — biggest single TTFT win; enable, measure.
3. **Chunked prefill** — enable if your prompts are >2K tokens; measure TTFT impact.
4. **FP8 KV cache** — if at long context or large batch; measure parity.
5. **Speculation** — last because acceptance-rate measurement requires the rest of the stack to be stable. Measure acceptance with a draft model first; switch to EAGLE-3 once the workload is profiled.

---

**Lab — enable each feature, measure each metric movement**

Goal: isolate each feature's contribution to throughput and TTFT on a fixed workload.

1. **Baseline** — Llama 3.3 70B FP8 on 4× H100, vLLM 0.22, default config except features-off.
2. **Add continuous batching + PagedAttention** — measure throughput delta.
3. **Add prefix cache** — measure TTFT delta on a workload with 1500-token system prompt + 100-token user turns.
4. **Add chunked prefill** — measure TTFT delta on a workload with 8K-token prompts.
5. **Add FP8 KV cache** — measure HBM saved, validate parity on RULER at 32K.
6. **Add a 1B-class draft model** — measure throughput delta, measure acceptance rate.
7. **Plot** the cumulative throughput across all features.

Pass criterion: you have a chart that shows each feature's individual contribution and the cumulative result. Each contribution is defended by a measurement.

---

</details>

## 自查

1. 同事测了 prefix caching，报告吞吐提升 2×。你怀疑该测试有偏。什么样的工作负载特性会放大 prefix cache 的收益？
2. EAGLE-3 在 chat 上的接受率是 70%，在 tool-call agent 工作负载上是 45%。为什么 agent 场景的接受率可能更低？这对是否把 EAGLE-3 上线到 agent 产品有何启示？
3. PagedAttention v2 与 v1：在什么样的工作负载下，你能可测量地观察到 v2 的优势？
4. 连续批处理会混合 prefill 与 decode。对于平均 prompt 2048、平均输出 16 的工作负载（agent 形态），你会启用 chunked prefill 吗？为什么？
5. 你的 draft model 是 Llama 3.2 1B，target 是 Qwen 2.5 72B，接受率为 35%。第一个实验是什么？

---

## 参考文献

* vLLM PagedAttention 论文 — [arXiv:2309.06180](https://arxiv.org/abs/2309.06180)
* SGLang RadixAttention 论文 — [arXiv:2312.07104](https://arxiv.org/abs/2312.07104)
* FlashInfer（attention/sampling kernel 引擎）— [arXiv:2501.01005](https://arxiv.org/abs/2501.01005)（MLSys 2025）· [github.com/flashinfer-ai/flashinfer](https://github.com/flashinfer-ai/flashinfer)
* 投机解码原始论文 — [arXiv:2211.17192](https://arxiv.org/abs/2211.17192)
* EAGLE — [arXiv:2401.15077](https://arxiv.org/abs/2401.15077)
* EAGLE-2 — [arXiv:2406.16858](https://arxiv.org/abs/2406.16858)
* EAGLE-3 — [arXiv:2503.01840](https://arxiv.org/abs/2503.01840)
* Medusa heads — [arXiv:2401.10774](https://arxiv.org/abs/2401.10774)
* DistServe（P/D 分离，作对照）— [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)
* "Orca: A Distributed Serving System for Transformer-Based Generative Models" — OSDI 2022 — 连续批处理思想的 vLLM 之前源头

交叉引用：

* [Part 1 → Lecture 05 — Runtime landscape](../01-基础/Lecture-05.md)
* [阶段 5 → 边缘 AI → Agent Tool-Dispatch Evaluation with BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) — 投机解码的精度一致性门控

---

## 更新至 2026-06

特性锁定为：vLLM 0.22 V1（连续批处理、PagedAttention v2、prefix cache、支持 EAGLE-2/3）、SGLang 0.5（RadixAttention）、TRT-LLM 1.3（in-flight batching、KV 复用）。当 EAGLE-4 或后续版本落地，或出现一种根本性的新调度方法（后连续批处理时代）时，刷新本节。

---

## 后续

* 下一篇：[Lecture 06 — 128K 长上下文在 Hopper 上](Lecture-06.md)
* 上一篇：[Lecture 04 — 8× H100/H200 上的张量并行](Lecture-04.md)
* 上级：[Part 2 — Hopper 上的 Dense](README.md)


<details>
<summary>English original</summary>

**Self-check**

1. A teammate measures prefix caching and reports a 2× throughput improvement. You suspect the test is biased. What workload property would inflate prefix-cache gains?
2. EAGLE-3 acceptance is 70% on chat. On a tool-call agent workload it's 45%. Why might agent acceptance be lower, and what does that suggest about whether to ship EAGLE-3 for the agent product?
3. PagedAttention v2 vs v1: under what workload would you measurably notice the v2 win?
4. Continuous batching mixes prefill and decode. For a workload with mean prompt 2048 and mean output 16 (agent shape), would you enable chunked prefill? Why?
5. Your draft model is Llama 3.2 1B and target is Qwen 2.5 72B. Acceptance is 35%. What is the first experiment?

---

**References**

* vLLM PagedAttention paper — [arXiv:2309.06180](https://arxiv.org/abs/2309.06180)
* SGLang RadixAttention paper — [arXiv:2312.07104](https://arxiv.org/abs/2312.07104)
* FlashInfer (attention/sampling kernel engine) — [arXiv:2501.01005](https://arxiv.org/abs/2501.01005) (MLSys 2025) · [github.com/flashinfer-ai/flashinfer](https://github.com/flashinfer-ai/flashinfer)
* Speculative Decoding original — [arXiv:2211.17192](https://arxiv.org/abs/2211.17192)
* EAGLE — [arXiv:2401.15077](https://arxiv.org/abs/2401.15077)
* EAGLE-2 — [arXiv:2406.16858](https://arxiv.org/abs/2406.16858)
* EAGLE-3 — [arXiv:2503.01840](https://arxiv.org/abs/2503.01840)
* Medusa heads — [arXiv:2401.10774](https://arxiv.org/abs/2401.10774)
* DistServe (P/D disaggregation, contrast) — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)
* "Orca: A Distributed Serving System for Transformer-Based Generative Models" — OSDI 2022 — pre-vLLM origin of continuous batching ideas

Cross-references:

* [Part 1 → Lecture 05 — Runtime landscape](../01-基础/Lecture-05.md)
* [Phase 5 → Edge AI → Agent Tool-Dispatch Evaluation with BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) — parity gating for speculation

---

**Current as of 2026-06**

Features pinned: vLLM 0.22 V1 (continuous batching, PagedAttention v2, prefix cache, EAGLE-2/3 supported), SGLang 0.5 (RadixAttention), TRT-LLM 1.3 (in-flight batching, KV reuse). Refresh when EAGLE-4 or successor lands, or when a fundamentally new scheduling method (post-continuous-batching) ships.

---

**Next**

* Next: [Lecture 06 — Long context at 128K on Hopper](Lecture-06.md)
* Previous: [Lecture 04 — Tensor parallelism on 8× H100/H200](Lecture-04.md)
* Up: [Part 2 — Dense at Hopper](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 2 - Dense at Hopper/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%202%20-%20Dense%20at%20Hopper/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
