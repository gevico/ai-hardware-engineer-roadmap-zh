---
title: Part 2 · 第 06 讲 — Hopper 上的 128K 长上下文
description: Part 2 · 第 06 讲 — Hopper 上的 128K 长上下文
---

# Part 2 · 第 06 讲 — Hopper 上的 128K 长上下文

## 概览

Llama 3.3 70B 与 Qwen 2.5 72B 都对外公布 **128K 上下文窗口**。在 128K 下，推理的成本结构会改变——在 4K 时主导 TPOT 的因素（权重带宽）会被 **KV 带宽**与 **prefill 计算**（prefill：首字前的整段计算）追平甚至反超。**长上下文推理服务是一个独立的工程问题**，有自己的精度 recipe、调度选择和精度一致性门槛。

本讲是 Part 2 的最后一讲，涵盖：

1. 128K 下的成本计算——KV 内存、每个 decode 步（逐 token 生成阶段）的 KV 带宽、prefill FLOPs。
2. **YaRN 上下文扩展**——如何从 32K 基础模型解锁 128K，以及它在推理时的代价。
3. 128K 下的 **chunked prefill**——它解决了什么，又在何处失效。
4. **大规模 FP8 KV cache**——何时可用、精度一致性代价如何、逐 head 的缩放规范。
5. **长系统提示词上的前缀共享**——收益最高的长上下文优化。
6. **长上下文评估**——RULER、needle-in-haystack，以及真正该测什么。
7. Hopper 上 128K 的**生产 recipe**。

学完本讲，应能在 Hopper 上以 128K 上下文部署两个 anchor 模型中的任意一个，用 RULER 的精度一致性数据为精度 recipe 辩护，并交付一份让同事可复现的 benchmark 报告。

---

## 1. 128K 下的成本计算

回顾 Part 1 第 02 讲的 KV cache 计算：

```text
kv_bytes_per_token = 2 × L × num_kv_heads × head_dim × bytes
                   = 2 × 80 × 8 × 128 × 2 (FP16)
                   = 320 KB/token
```

在 128K 上下文下，每个请求：

| 精度 | KV cache 大小 |
|-----------|---------------|
| FP16 KV | 320 KB × 131072 = **42 GB** |
| FP8 KV | 21 GB |
| INT4 KV | 10.5 GB |

这是**单个请求**的用量。batch=8、FP16 KV 时为 336 GB。只有靠**激进分页**才塞得进 4× H100 80G（合计 320 GB）——而且没给权重和激活值留下任何余地。

**在 128K 上下文下，批大小远在受算力限制之前就已受 HBM 限制。**

### 1.1 每个 decode 步的 KV 带宽

每个 decode 步：

```text
kv_read_bytes = 2 × L × num_kv_heads × head_dim × seq_len × bytes
              = kv_bytes_per_token × seq_len
              = 320 KB × 131072
              = 42 GB
```

在 H200（4.8 TB/s HBM3e）上，仅 KV 读取就要耗时：

```text
kv_read_time = 42 GB / 4.8 TB/s ≈ 8.7 ms
```

再加上权重读取（FP16 下 140 GB / 4.8 TB/s 约 30 ms，INT4 下 35 GB 约 7.3 ms）。因此在单张 H200、FP16 KV、INT4 权重下，128K 上下文的 decode：

```text
TPOT ≈ kv_read_time + weight_read_time + compute + overhead
     ≈ 8.7 + 7.3 + small + small
     ≈ 16 ms per token
```

而 4K 上下文时约 8 ms（权重读取相同，KV 项可忽略）。**128K 下 TPOT 大致翻倍**，因为 **KV 带宽成了并列主导的成本项**。

### 1.2 128K 下的 prefill 成本

```text
prefill_flops ≈ 2 × P × prompt_tokens
              = 2 × 70 × 10^9 × 131072
              ≈ 1.83 × 10^16 FLOPs
              = 18.3 PFLOPs
```

在 4× H100、FP16 下（4 × 989 TFLOPs / GPU = 3.96 PFLOPs 总量，按 80% 效率计 = 3.17 PFLOPs 有效）：

```text
prefill_time ≈ 18.3 / 3.17 ≈ 5.8 seconds
```

第一个 token 解码出来之前，prefill 耗时近六秒。**对交互式产品而言，没有 chunked prefill 或前缀缓存就无法上线**。（这里只计权重矩阵乘的 FLOPs——128K 下 prefill 的 attention 项量级相当，见 §1.3，因此要按约 10 s 墙钟时间做预算。）

### 1.3 128K 下的 attention 成本

attention 为 `softmax(Q · K^T) · V`。batch=1、128K 上下文时：

```text
qk_flops = 2 × q_heads × q_seq × kv_seq × head_dim
         = 2 × 64 × 1 × 131072 × 128 (for one decode step)
         ≈ 2.1 × 10^9 FLOPs
```

单次 decode 步的 attention 运算为 2 GFLOPs。**计算上微不足道。** 但 attention 的 *KV 读取*为 42 GB / decode 步（见 §1.1）。因此长上下文下的 attention **纯粹受带宽限制**。

对 prefill（batch=1、prompt=128K），attention 为 O(prompt² × head_dim × heads)，即约 2 × 64 × 131072 × 131072 × 128 = 277 TFLOPs——与 FFN 成本相当。FlashAttention 4 的 O(N) 内存复杂度在此至关重要。

---

## 2. YaRN 上下文扩展

两个模型通过**不同机制**达到 128K。Llama 3.1/3.3 70B 使用 Meta 自有的 `"rope_type": "llama3"` RoPE 频率缩放，加上长上下文继续预训练——128K 窗口在训练时就已固化。Qwen 2.5 72B 属于 YaRN 情形：以 32K 原生上下文训练，并**经 YaRN 扩展到 128K**（[arXiv:2309.00071](https://arxiv.org/abs/2309.00071)），在推理时应用。


<details>
<summary>English original</summary>

**Part 2 · Lecture 06 — Long Context at 128K on Hopper**

**Overview**

Both Llama 3.3 70B and Qwen 2.5 72B publish **128K context windows**. At 128K the cost shape of inference changes — what dominated TPOT at 4K (weight bandwidth) is joined or overtaken by **KV bandwidth** and **prefill compute**. **Long-context serving is a distinct engineering problem** with its own precision recipes, scheduling choices, and parity bars.

This final Part 2 lecture covers:

1. The cost math at 128K — KV memory, KV bandwidth per decode step, prefill FLOPs.
2. **YaRN context extension** — how 128K is unlocked from a base 32K model, and what it costs at inference.
3. **Chunked prefill** at 128K — what it solves and where it breaks.
4. **FP8 KV cache at scale** — when it ships, what parity costs, per-head scaling discipline.
5. **Prefix sharing on long system prompts** — the highest-leverage long-context optimization.
6. **Long-context evaluation** — RULER, needle-in-haystack, what to actually measure.
7. **Production recipes** at 128K on Hopper.

By the end you should be able to deploy either anchor model at 128K context on Hopper, defend the precision recipe with parity numbers from RULER, and ship a benchmark report that lets a teammate reproduce it.

---

**1. The cost math at 128K**

Revisiting the KV cache math from Part 1 Lecture 02:

```text
kv_bytes_per_token = 2 × L × num_kv_heads × head_dim × bytes
                   = 2 × 80 × 8 × 128 × 2 (FP16)
                   = 320 KB/token
```

At 128K context, per request:

| Precision | KV cache size |
|-----------|---------------|
| FP16 KV | 320 KB × 131072 = **42 GB** |
| FP8 KV | 21 GB |
| INT4 KV | 10.5 GB |

This is for **one request**. At batch=8 with FP16 KV: 336 GB. That fits on 4× H100 80G (320 GB total) only by **aggressive paging** — and leaves nothing for weights or activations.

**At 128K context, batch size is HBM-limited far before it is compute-limited.**

**1.1 KV bandwidth per decode step**

At each decode step:

```text
kv_read_bytes = 2 × L × num_kv_heads × head_dim × seq_len × bytes
              = kv_bytes_per_token × seq_len
              = 320 KB × 131072
              = 42 GB
```

On H200 (4.8 TB/s HBM3e), the KV read alone takes:

```text
kv_read_time = 42 GB / 4.8 TB/s ≈ 8.7 ms
```

Plus the weight read (~30 ms at FP16 for 140 GB / 4.8 TB/s, ~7.3 ms at INT4 for 35 GB). So decode at 128K context on a single H200, FP16 KV, INT4 weights:

```text
TPOT ≈ kv_read_time + weight_read_time + compute + overhead
     ≈ 8.7 + 7.3 + small + small
     ≈ 16 ms per token
```

versus ~8 ms at 4K context (same weight read, negligible KV term). **TPOT roughly doubles at 128K** because the **KV bandwidth becomes a co-dominant cost**.

**1.2 Prefill cost at 128K**

```text
prefill_flops ≈ 2 × P × prompt_tokens
              = 2 × 70 × 10^9 × 131072
              ≈ 1.83 × 10^16 FLOPs
              = 18.3 PFLOPs
```

On 4× H100 at FP16 (4 × 989 TFLOPs / GPU = 3.96 PFLOPs aggregate, assuming 80% efficiency = 3.17 PFLOPs effective):

```text
prefill_time ≈ 18.3 / 3.17 ≈ 5.8 seconds
```

Almost six seconds of prefill before the first token decodes. **For an interactive product this is unshippable** without chunked prefill or prefix cache. (This counts only the weight-matmul FLOPs — at 128K the prefill attention term is of comparable size, see §1.3, so budget ~10 s wall-clock.)

**1.3 Attention cost at 128K**

Attention is `softmax(Q · K^T) · V`. At 128K context with batch=1:

```text
qk_flops = 2 × q_heads × q_seq × kv_seq × head_dim
         = 2 × 64 × 1 × 131072 × 128 (for one decode step)
         ≈ 2.1 × 10^9 FLOPs
```

A single decode-step attention op is 2 GFLOPs. **Compute-trivial.** But the *KV read* for attention is 42 GB / decode step (per §1.1). So attention at long context is **purely bandwidth-bound**.

For prefill (batch=1, prompt=128K), attention is O(prompt² × head_dim × heads) which becomes ~2 × 64 × 131072 × 131072 × 128 = 277 TFLOPs — comparable to the FFN cost. FlashAttention 4's O(N) memory complexity is essential here.

---

**2. YaRN context extension**

The two models reach 128K by **different mechanisms**. Llama 3.1/3.3 70B use Meta's own `"rope_type": "llama3"` RoPE frequency scaling plus long-context continued pretraining — the 128K window is baked in at training time. Qwen 2.5 72B is the YaRN case: trained at 32K native context and **extended to 128K via YaRN** ([arXiv:2309.00071](https://arxiv.org/abs/2309.00071)) applied at inference.

</details>

### 2.1 YaRN 做了什么

YaRN **重缩放 RoPE 频率**，使模型能够在更长的范围上做 attention，**而无需从头重新训练**。结果是：在 8K 上训练的模型，只需约几个 epoch 的微调，即可在 128K 上服务。

对 runtime 的影响极小：

* RoPE 矩阵以扩展后的频率重建。除 config 中的 `rope_scaling` 外，无需改动任何代码。
* 无额外的 kernel 开销。
* Qwen 公布的 128K 上下文是经 YaRN 扩展的；对 `max_position_embeddings > base_context`，需确保 runtime 施加正确的缩放。

### 2.2 YaRN 在推理时可能出问题的地方

* **缩放因子错误：** 如果 runtime 施加的 YaRN 缩放与模型微调时所用的不一致，长上下文召回会退变。始终使用来自 `config.json` 的官方 `rope_scaling`。
* **量化交互：** 重缩放后的 RoPE 值数值范围更宽；FP8 KV 的 per-tensor 缩放可能会截断。per-head 缩放可避免此问题。

### 2.3 128K 下的长上下文质量

已公布的 benchmark（RULER、NIAH）：

| 模型 | 4K | 32K | 64K | 128K |
|-------|----|-----|-----|------|
| Llama 3.3 70B | ~96 | ~92 | ~89 | ~80（下降更陡） |
| Qwen 2.5 72B | ~96 | ~93 | ~91 | ~85 |

在已公布的 RULER 数字中，Qwen 2.5 72B 在 128K 下的长上下文表现**总体上强于 Llama 3.3 70B**，不过两者相对各自短上下文的性能都有明显下降。对于依赖 100K+ 精确检索的产品，这是一个**模型选型信号**，与推理工程无关。

---

## 3. 128K 下的 chunked prefill

prefill（首字前的整段计算）成本计算（§1.2）显示，在 4× H100 上做一次 128K prefill 需 5.8 秒。chunked prefill 使其变得可行。

### 3.1 思路

把 128K 的 prefill 切成多个 chunk（例如每个 4K）。连续批处理的每个 "step" 处理一个 chunk。decode（逐 token 生成阶段）请求共享这些 step：

```text
step 1: prefill chunk 1 (4K tokens) + decode steps for other requests
step 2: prefill chunk 2 (4K tokens) + decode steps
...
step 32: prefill chunk 32 (4K tokens) → first token of new request ready
```

prefill 的总墙钟时间不变（约 5.8 秒）。但是：

* 长 prefill 期间，**其他请求的 decode 不会被阻塞**。
* **长 prompt 请求的 TTFT 会增大**，但整体吞吐仍然很高。
* **GPU 利用率保持平稳**，因为每个 step 都混合了计算（chunked prefill）与带宽（decode）。

### 3.2 vLLM 配置

```python
LLM(
    ...,
    enable_chunked_prefill=True,
    max_num_batched_tokens=8192,    # total tokens per step (prefill + decode)
)
```

`max_num_batched_tokens=8192` 表示每个 step 处理 8K token 的工作量，把 prefill chunk 与 decode 行混合在一起。

### 3.3 取舍

* **不使用 chunked prefill：** 长 prefill 会阻塞整个副本约 5 秒。其他用户会看到 TTFT 尖峰。
* **使用 chunked prefill：** 长 prefill 被摊开；其他用户的 TTFT 保持正常，但长 prompt 用户的 TTFT 会延长到约 6.5 秒（由于每 step 开销，墙钟时间更长）。

对聊天 / agent 产品，**chunked prefill 是默认选择**——跨用户的 p99 更好，比单个请求的最快 TTFT 更重要。

对批处理产品，关闭 chunked prefill 可能略快，因为省去了每 step 的开销，可以把每个 step 的全部时间花在一个任务上。

---

## 4. 规模化下的 FP8 KV cache

在 128K 上下文下，FP8 KV（把每请求的 HBM 从 42 GB 降到 21 GB）是实际可用的默认选择。

### 4.1 per-head 缩放的纪律

并非所有 attention head 都有相同的 KV 幅值分布：

* 有些 head 专门处理稀有 token 位置（例如 BOS、代码分隔符）。
* 它们的 KV 值存在离群点，FP8 *per-tensor* 缩放会将其截断，从而在长上下文检索上产生可测量的精度一致性损失。

**per-head 缩放可解决这一问题。** 每个 head 拥有自己的 FP8 scale factor。

vLLM 0.22+：`kv_cache_dtype="fp8_e5m2"`，默认启用 per-head 缩放。
SGLang 0.5+：通过 `--kv-cache-dtype fp8` 实现 per-head FP8 KV。
TRT-LLM：当 `kv_cache_dtype=fp8` 时，per-head FP8 KV 为默认。

### 4.2 128K 下的精度一致性

Llama 3.3 70B 在 64K 和 128K 下使用 FP8 KV：

| 评测 | FP16 KV | FP8 KV per-tensor | FP8 KV per-head |
|------|---------|-------------------|------------------|
| RULER 64K | 89.5 | 86.2（-3.3） | 89.0（-0.5） |
| RULER 128K | 80.1 | 75.3（-4.8） | 79.4（-0.7） |

长上下文下 per-tensor FP8 KV 损失 3-5 个百分点。per-head 损失 0.5-0.7 个百分点。**per-head 是生产环境的 recipe。**

Qwen 2.5 72B 的数字类似；两个模型在 per-head FP8 KV 下表现都很好。

### 4.3 128K 下的 INT4 KV——通常过于激进

128K 下的 INT4 KV 在 RULER 上通常损失 5-10 个百分点。对于模型无需从长上下文中回忆细节的产品（例如短输出摘要）可以接受。对检索类任务不可接受。

**决策规则：** 如果工作负载的价值依赖准确的长上下文检索，就上线 FP8 KV。INT4 KV 只适用于容忍度极高的工作负载或极端的 HBM 压力。

---


<details>
<summary>English original</summary>

**2.1 What YaRN does**

YaRN **rescales RoPE frequencies** so the model can attend over longer ranges **without retraining from scratch**. The result: a model trained at 8K can serve at 128K with only ~few epochs of fine-tuning.

The runtime impact is minimal:

* The RoPE matrix is rebuilt with extended frequencies. No code change beyond `rope_scaling` in config.
* No additional kernel overhead.
* Qwen's published 128K context is YaRN-extended; for `max_position_embeddings > base_context`, ensure the runtime applies the right scaling.

**2.2 Where YaRN can go wrong at inference**

* **Wrong scaling factor:** if the runtime applies a different YaRN scaling than the model was fine-tuned with, long-context recall regresses. Always use the official `rope_scaling` from `config.json`.
* **Quantization interaction:** rescaled RoPE values have a wider numeric range; FP8 KV per-tensor scaling may clip. Per-head scaling avoids this.

**2.3 Long-context quality at 128K**

Published benchmarks (RULER, NIAH):

| Model | 4K | 32K | 64K | 128K |
|-------|----|-----|-----|------|
| Llama 3.3 70B | ~96 | ~92 | ~89 | ~80 (sharper drop) |
| Qwen 2.5 72B | ~96 | ~93 | ~91 | ~85 |

Qwen 2.5 72B's long-context behavior at 128K is **generally stronger than Llama 3.3 70B** in published RULER numbers, though both meaningfully degrade vs. their short-context performance. For products that rely on accurate retrieval at 100K+, this is a **model-selection signal** independent of inference engineering.

---

**3. Chunked prefill at 128K**

The prefill cost calculation (§1.2) showed 5.8 seconds for a 128K prefill on 4× H100. Chunked prefill makes this practical.

**3.1 The idea**

Split the 128K prefill into chunks (e.g., 4K each). Process one chunk per "step" of continuous batching. Decode requests share these steps:

```text
step 1: prefill chunk 1 (4K tokens) + decode steps for other requests
step 2: prefill chunk 2 (4K tokens) + decode steps
...
step 32: prefill chunk 32 (4K tokens) → first token of new request ready
```

Total prefill wall-clock is unchanged (~5.8 seconds). But:

* **Other requests' decode is not blocked** during the long prefill.
* **TTFT for the long-prompt request increases**, but throughput overall stays high.
* **GPU utilization stays smooth** because each step has a mix of compute (chunked prefill) and bandwidth (decodes).

**3.2 vLLM configuration**

```python
LLM(
    ...,
    enable_chunked_prefill=True,
    max_num_batched_tokens=8192,    # total tokens per step (prefill + decode)
)
```

`max_num_batched_tokens=8192` means each step processes 8K tokens of work, mixing prefill chunks with decode rows.

**3.3 The tradeoff**

* **Without chunked prefill:** long prefill blocks the whole replica for ~5 seconds. Other users see a TTFT spike.
* **With chunked prefill:** long prefill is spread; other users' TTFT stays normal, but the long-prompt user's TTFT extends to ~6.5 seconds (longer wall-clock due to per-step overhead).

For chat / agent products, **chunked prefill is the default** — better p99 across users matters more than fastest individual TTFT.

For batch products, disabling chunked prefill can be slightly faster because the per-step overhead is removed and you can spend all of each step on one task.

---

**4. FP8 KV cache at scale**

At 128K context, FP8 KV (cuts HBM from 42 GB → 21 GB per request) is the practical default.

**4.1 Per-head scaling discipline**

Not all attention heads have the same KV magnitude distribution:

* Some heads specialize in rare-token positions (e.g., BOS, code-delimiters).
* Their KV values have outliers that FP8 *per-tensor* scaling clips, producing measurable parity loss on long-context retrieval.

**Per-head scaling fixes this.** Each head gets its own FP8 scale factor.

vLLM 0.22+: `kv_cache_dtype="fp8_e5m2"` with per-head scaling enabled by default.
SGLang 0.5+: per-head FP8 KV via `--kv-cache-dtype fp8`.
TRT-LLM: per-head FP8 KV is the default when `kv_cache_dtype=fp8`.

**4.2 Parity at 128K**

For Llama 3.3 70B with FP8 KV at 64K and 128K:

| Eval | FP16 KV | FP8 KV per-tensor | FP8 KV per-head |
|------|---------|-------------------|------------------|
| RULER 64K | 89.5 | 86.2 (-3.3) | 89.0 (-0.5) |
| RULER 128K | 80.1 | 75.3 (-4.8) | 79.4 (-0.7) |

Per-tensor FP8 KV loses 3-5 pp at long context. Per-head loses 0.5-0.7 pp. **Per-head is the production recipe.**

For Qwen 2.5 72B numbers are similar; both models behave well under per-head FP8 KV.

**4.3 INT4 KV at 128K — usually too aggressive**

INT4 KV at 128K typically loses 5-10 pp on RULER. Acceptable for products where the model doesn't need to recall details from the long context (e.g., short-output summarization). Unacceptable for retrieval-style tasks.

**Decision rule:** if the workload's value depends on accurate long-context retrieval, ship FP8 KV. INT4 KV is only for highly tolerant workloads or extreme HBM pressure.

---

</details>

## 5. 长系统提示词上的前缀共享

收益最高的长上下文优化。多数长上下文产品具备以下之一：

* **长系统提示词**（整个代码库、文档、手册）在所有用户轮次间共享。
* **已存储的对话**，聊天历史不断增长。
* **RAG（检索增强生成）上下文**，按查询检索，在查询之间部分重叠。

对上述三种情况，**前缀缓存可将 prefill（首字前的整段计算）成本降低 70-95%。**

### 5.1 系统提示词场景

100K token 的系统提示词 + 1K 用户轮次：

* 无前缀缓存：prefill 101K token（在 4× H100 上约 5 秒）。
* 有前缀缓存（首次请求之后）：prefill 1K token（约 50 ms）。

对任何共享该系统提示词的后续请求，prefill 快 100×。

### 5.2 对话场景

不断增长的聊天会话，50K token 历史 + 500 token 的新用户轮次：

* 无前缀缓存：每轮 prefill 50.5K。
* 有前缀缓存：每轮 prefill 500（系统提示词 + 此前轮次均已缓存）。

首轮之后每轮快 10-100×。

### 5.3 RAG 场景

RAG 按查询检索 N 个文档。如果文档在查询之间复用，缓存会捕获部分重叠。SGLang 的 RadixAttention 在这里尤其强，因为前缀树匹配文档的部分重叠，而不仅仅是精确匹配的前缀。

### 5.4 缓存驱逐

前缀缓存位于 HBM；在长上下文下，驱逐策略很重要。vLLM 使用 LRU；SGLang 使用带树结构感知的 LRU（让共享前缀保留更久）。

常见的生产调优：将 `kv_cache_block_size` 设置得稍大（例如 32 而不是 16），让 radix tree 在长前缀上获得更好的命中率。代价：驱逐粒度稍大。

---

## 6. 长上下文评估——要测什么

长上下文推理的精度一致性要点：

### 6.1 RULER ([arXiv:2404.06654](https://arxiv.org/abs/2404.06654))

一套受控长度（4K、8K、16K、32K、64K、128K）的合成任务。任务包括：

* 单针大海捞针（在无关文本中找出一条事实）。
* 多针（找出 N 条事实）。
* 变量跟踪（在长文本中跟踪取值）。
* 词频提取。

截至 2026 年中期，RULER 是 **权威的长上下文 benchmark**。

### 6.2 大海捞针（NIAH）

比 RULER 更简单。在 N 个 token 的无关上下文中，将一根针（例如 "the secret password is purple-frog-42"）放在已知位置。查询模型以将其检索出来。

* 绘制准确率随上下文长度 × 针位置变化的曲线。
* 常见“lost in the middle”——开头/结尾准确率更高，中间下降。

### 6.3 为精度一致性验证实际要测什么

对于长上下文产品：

1. **选择 16K、64K、128K 的 RULER**（或产品预期的最长上下文）。
2. **将 FP16 参考 recipe 运行两次，以确定本底噪声。**
3. **运行每个候选 recipe（FP8 KV、INT4 KV、chunked prefill 开/关）。**
4. **按长度计算 Δ。**

规律：**像 MMLU 这样的短上下文评测抓不住长上下文性能退化**。**始终针对长上下文专门验证。**

---

## 7. 128K 下的生产 recipe

针对 Hopper 上 128K 上下文的 Llama 3.3 70B / Qwen 2.5 72B 综合得出：

### 7.1 单卡 H200 141G（低并发）

```text
Weights:       AWQ-INT4 group-128 (35 GB)
Activations:   FP16
KV cache:      FP8 per-head (21 GB per request at 128K)
HBM headroom:  ~85 GB for KV + activations → batch ~3 at 128K, batch ~12 at 32K

Runtime:       vLLM 0.22+ V1
Features:      continuous batching, paged KV v2, prefix cache, chunked prefill (4K chunks)
Speculation:   off (small batch makes it less effective)
```

### 7.2 4× H100 SXM（生产聊天）

```text
Weights:       FP8 (TRT-LLM) or BF16 (vLLM)
Activations:   FP8 / FP16
KV cache:      FP8 per-head
TP:            4
Sequence par.: enabled (long-context activation savings)

Features:      continuous batching, paged KV v2, prefix cache, chunked prefill (8K chunks)
Speculation:   EAGLE-2 / EAGLE-3 if validated for the workload

Expected:      ~600-1000 tok/s/GPU at chat-typical concurrency 32 with 16K mean context
```

### 7.3 8× H200（最大吞吐 / 长上下文）

```text
Weights:       FP16 or FP8
KV cache:      FP8 per-head
TP:            8 (or 4 with DP=2 for throughput)
Sequence par.: enabled

Features:      full stack
Expected:      max concurrent users at 128K context; per-GPU throughput lower than TP=4 but absolute throughput highest
```

---


<details>
<summary>English original</summary>

**5. Prefix sharing on long system prompts**

The highest-leverage long-context optimization. Most long-context products have one of:

* **Long system prompt** (entire codebase, document, manual) shared across all user turns.
* **Stored conversation** with growing chat history.
* **RAG context** retrieved per query, partially overlapping across queries.

For all three, **prefix caching cuts prefill cost by 70-95%.**

**5.1 The system-prompt case**

A 100K-token system prompt + 1K user turn:

* Without prefix cache: prefill 101K tokens (~5 seconds on 4× H100).
* With prefix cache (after first request): prefill 1K tokens (~50 ms).

100× faster prefill for any subsequent request that shares the system prompt.

**5.2 The conversation case**

A growing chat session, 50K tokens of history + 500-token new user turn:

* Without prefix cache: prefill 50.5K each turn.
* With prefix cache: prefill 500 each turn (system prompt + previous turns are cached).

10-100× faster per turn after the first.

**5.3 The RAG case**

RAG retrieves N docs per query. If docs are reused across queries, the cache catches partial overlap. SGLang's RadixAttention is especially strong here because the prefix tree matches partial document overlap, not just exact-match prefixes.

**5.4 Cache eviction**

Prefix cache lives in HBM; eviction policy matters at long context. vLLM uses LRU; SGLang uses LRU with tree-structure awareness (keeps shared prefixes longer).

A common production tuning: set `kv_cache_block_size` slightly larger (e.g., 32 instead of 16) to give the radix tree better hit rates on long prefixes. Trade: slightly larger eviction granularity.

---

**6. Long-context evaluation — what to measure**

The parity story for long-context inference:

**6.1 RULER ([arXiv:2404.06654](https://arxiv.org/abs/2404.06654))**

A suite of synthetic tasks at controlled lengths (4K, 8K, 16K, 32K, 64K, 128K). Tasks:

* Single needle in haystack (find one fact among irrelevant text).
* Multi-needle (find N facts).
* Variable tracking (track values across long text).
* Frequency word extraction.

RULER is the **canonical long-context benchmark** as of mid-2026.

**6.2 Needle in a haystack (NIAH)**

Simpler than RULER. Place a needle (e.g., "the secret password is purple-frog-42") at a known position in N tokens of irrelevant context. Query the model to retrieve it.

* Plot accuracy as a function of context length × needle position.
* Common to see "lost in the middle" — accuracy higher at start/end, dips in the middle.

**6.3 What to actually measure for parity validation**

For a long-context product:

1. **Pick RULER at 16K, 64K, 128K** (or your product's longest expected context).
2. **Run the FP16-reference recipe twice for noise floor.**
3. **Run each candidate recipe (FP8 KV, INT4 KV, chunked prefill on/off).**
4. **Compute Δ per length.**

The pattern: **short-context evals like MMLU don't catch long-context regressions**. **Always validate long-context-specifically.**

---

**7. Production recipes at 128K**

Synthesized for Llama 3.3 70B / Qwen 2.5 72B at 128K context on Hopper:

**7.1 Single H200 141G (low concurrency)**

```text
Weights:       AWQ-INT4 group-128 (35 GB)
Activations:   FP16
KV cache:      FP8 per-head (21 GB per request at 128K)
HBM headroom:  ~85 GB for KV + activations → batch ~3 at 128K, batch ~12 at 32K

Runtime:       vLLM 0.22+ V1
Features:      continuous batching, paged KV v2, prefix cache, chunked prefill (4K chunks)
Speculation:   off (small batch makes it less effective)
```

**7.2 4× H100 SXM (production chat)**

```text
Weights:       FP8 (TRT-LLM) or BF16 (vLLM)
Activations:   FP8 / FP16
KV cache:      FP8 per-head
TP:            4
Sequence par.: enabled (long-context activation savings)

Features:      continuous batching, paged KV v2, prefix cache, chunked prefill (8K chunks)
Speculation:   EAGLE-2 / EAGLE-3 if validated for the workload

Expected:      ~600-1000 tok/s/GPU at chat-typical concurrency 32 with 16K mean context
```

**7.3 8× H200 (max throughput / long context)**

```text
Weights:       FP16 or FP8
KV cache:      FP8 per-head
TP:            8 (or 4 with DP=2 for throughput)
Sequence par.: enabled

Features:      full stack
Expected:      max concurrent users at 128K context; per-GPU throughput lower than TP=4 but absolute throughput highest
```

---

</details>

## 实验 — 两个模型的长上下文 bench

目标：为 Llama 3.3 70B 和 Qwen 2.5 72B 产出一份长上下文 benchmark 报告。

1. **硬件** — 4× H100 或 H200；最好两者都有以便对比。
2. **Runtime** — vLLM 0.22+ V1。
3. **待 bench 的配置** — 对每个模型：
   * FP8 KV，不启用 chunked prefill（prefill 指首字前的整段计算）。
   * FP8 KV，启用 chunked prefill。
   * INT4 KV（如果你想验证它在何处失效）。
4. **工作负载：**
   * RULER，16K、64K、128K。
   * 在 100K token 的 prefill 上测 TTFT，随后接 1K 的用户轮次，分别在启用与不启用 prefix cache 的情况下测量。
5. **吞吐** — 32K 平均上下文下的对话形态持续工作负载，并发 16。

通过标准：你可以把报告交给另一位工程师，他们能据此为 128K 上下文的产品选定精度 recipe + chunked-prefill 配置，并以你的精度一致性数据作为依据。

---

## 自检

1. 在 128K 上下文下，FP16 KV 每个请求为 42 GB。在 4× H100（320 GB HBM）上，使用 INT4 权重（35 GB）加 FP16 KV，你能在 128K 下服务多少个并发请求？用 FP8 KV 呢？
2. RULER 64K 精度一致性在 per-tensor FP8 KV 下损失 3.3 pp，在 per-head FP8 KV 下损失 0.5 pp。为什么 per-head 版本能恢复这么多？
3. 你的 128K TTFT 在不启用 prefix cache 时是 5 秒。启用后降至 80 ms。工作负载的什么特性带来了这一显著改善？
4. 4K chunk 与 16K chunk 的 chunked prefill：对于 90% 短 prompt、10% 128K prompt 的工作负载，哪个的 p99 TTFT 更好？为什么？
5. 对 128K 下的 Llama 3.3 70B 使用 per-head FP8 KV，预测 H200（4.8 TB/s）上的 decode TPOT。写出计算过程。

---

## 参考文献

* YaRN — [arXiv:2309.00071](https://arxiv.org/abs/2309.00071)
* RULER — [arXiv:2404.06654](https://arxiv.org/abs/2404.06654)
* Needle in a haystack（LangChain 实现） — [github.com/gkamradt/LLMTest_NeedleInAHaystack](https://github.com/gkamradt/LLMTest_NeedleInAHaystack)
* StreamingLLM（长上下文窗口 attention） — [arXiv:2309.17453](https://arxiv.org/abs/2309.17453)
* H2O（KV cache 驱逐） — [arXiv:2306.14048](https://arxiv.org/abs/2306.14048)
* MInference（面向长上下文 prefill 的动态稀疏 attention） — [arXiv:2407.02490](https://arxiv.org/abs/2407.02490)
* vLLM 长上下文调优 — [docs.vllm.ai/en/latest/serving/openai_compatible_server.html](https://docs.vllm.ai/en/latest/serving/openai_compatible_server.html)
* SGLang RadixAttention 论文 — [arXiv:2312.07104](https://arxiv.org/abs/2312.07104)

交叉引用：

* [第 1 部分 → 第 02 讲 — KV cache 数学](../01-基础/Lecture-02.md)
* [阶段 5 → GPU 基础设施 → Long-Context-MoE-Foundation-Training → 07 长上下文评估](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/07-长上下文MoE基础训练/07-Long-Context-Evaluation.md)

---

## 内容更新至 2026-06

上下文扩展方案由两个模型团队各自发布（`llama3` Llama 3.3 的 RoPE scaling，Qwen 2.5 的 YaRN）。生产 recipe 为 per-head FP8 KV。chunked prefill 成为 vLLM 0.22+ 中的默认项。当任一模型系列发布本质上全新的长上下文 attention 模式（例如循环状态、混合 SSM）时，需重新更新。

---

## 下一节

* 下一节：[第 07 讲 — 通信层内部](Lecture-07.md) — NCCL、自定义 all-reduce，以及 vLLM communicator 栈
* 上一节：[第 05 讲 — 现代推理服务栈](Lecture-05.md)
* 上一级：[第 2 部分 — Hopper 上的稠密模型](README.md)


<details>
<summary>English original</summary>

**Lab — long-context bench for both models**

Goal: produce a long-context benchmark report for Llama 3.3 70B and Qwen 2.5 72B.

1. **Hardware** — 4× H100 or H200; ideally both for comparison.
2. **Runtime** — vLLM 0.22+ V1.
3. **Configurations to bench** — for each model:
   * FP8 KV, no chunked prefill.
   * FP8 KV, chunked prefill.
   * INT4 KV (if you want to validate where it breaks).
4. **Workloads:**
   * RULER at 16K, 64K, 128K.
   * TTFT measurement on 100K-token prefill, then 1K user turn, with and without prefix cache.
5. **Throughput** — chat-shape continuous workload at 32K mean context, concurrency 16.

Pass criterion: you can hand the report to another engineer and they can pick a precision recipe + chunked-prefill config for a 128K-context product, defended by your parity numbers.

---

**Self-check**

1. At 128K context, FP16 KV is 42 GB per request. On 4× H100 (320 GB HBM), how many concurrent requests can you serve at 128K with INT4 weights (35 GB) and FP16 KV? With FP8 KV?
2. RULER 64K parity loses 3.3 pp with per-tensor FP8 KV and 0.5 pp with per-head FP8 KV. Why does the per-head version recover so much?
3. Your TTFT at 128K is 5 seconds without prefix cache. After enabling, it drops to 80 ms. What property of the workload allowed the dramatic improvement?
4. Chunked prefill at 4K chunks vs 16K chunks: which has better p99 TTFT for a workload of 90% short prompts and 10% 128K prompts? Why?
5. For Llama 3.3 70B with FP8 KV per-head at 128K, predict the decode TPOT on H200 (4.8 TB/s). Show the math.

---

**References**

* YaRN — [arXiv:2309.00071](https://arxiv.org/abs/2309.00071)
* RULER — [arXiv:2404.06654](https://arxiv.org/abs/2404.06654)
* Needle in a haystack (LangChain implementation) — [github.com/gkamradt/LLMTest_NeedleInAHaystack](https://github.com/gkamradt/LLMTest_NeedleInAHaystack)
* StreamingLLM (long-context window attention) — [arXiv:2309.17453](https://arxiv.org/abs/2309.17453)
* H2O (KV cache eviction) — [arXiv:2306.14048](https://arxiv.org/abs/2306.14048)
* MInference (dynamic sparse attention for long-context prefill) — [arXiv:2407.02490](https://arxiv.org/abs/2407.02490)
* vLLM long-context tuning — [docs.vllm.ai/en/latest/serving/openai_compatible_server.html](https://docs.vllm.ai/en/latest/serving/openai_compatible_server.html)
* SGLang RadixAttention paper — [arXiv:2312.07104](https://arxiv.org/abs/2312.07104)

Cross-references:

* [Part 1 → Lecture 02 — KV cache math](../01-基础/Lecture-02.md)
* [Phase 5 → GPU Infrastructure → Long-Context-MoE-Foundation-Training → 07 Long-Context Evaluation](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/07-长上下文MoE基础训练/07-Long-Context-Evaluation.md)

---

**Current as of 2026-06**

Context extension as published by both model teams (`llama3` RoPE scaling for Llama 3.3, YaRN for Qwen 2.5). FP8 KV per-head as the production recipe. Chunked prefill as the default in vLLM 0.22+. Refresh when a fundamentally new long-context attention pattern (e.g., recurrent state, hybrid SSM) ships in either model family.

---

**Next**

* Next: [Lecture 07 — Inside the communication layer](Lecture-07.md) — NCCL, custom all-reduce, and the vLLM communicator stack
* Previous: [Lecture 05 — Modern serving stack](Lecture-05.md)
* Up: [Part 2 — Dense at Hopper](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 2 - Dense at Hopper/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%202%20-%20Dense%20at%20Hopper/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
