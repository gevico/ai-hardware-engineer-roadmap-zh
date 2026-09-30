---
title: Part 3 · Lecture 05 —— 生产环境 MoE 推理服务：MTP 推测、受限 decode、成本模型
description: Part 3 · Lecture 05 —— 生产环境 MoE 推理服务：MTP 推测、受限 decode、成本模型
---

# Part 3 · Lecture 05 —— 生产环境 MoE 推理服务：MTP 推测、受限 decode、成本模型

## 概述

这是本课程的**顶点项目**。前面几讲 Part 3 确立了架构（Lecture 01）、芯片（Lecture 02）、并行（Lecture 03）与推理服务拓扑（Lecture 04）。本讲将它们整合为一个面向 DeepSeek V3.1 与 Qwen3-MoE 235B-A22B、运行在 Blackwell 上的**可辩护、可交付生产的推理部署**，并配有一套其他工程师可以复现的 `$/MTok` 成本模型。

主题：

1. **MTP 作为 DeepSeek 的原生推测**——配置、接受率、吞吐。
2. **面向 Qwen3-MoE 的 EAGLE-3**——带草稿模型的非原生推测路径。
3. **受限解码**——MoE 规模下的 XGrammar / Outlines，工具调用场景。
4. **生产环境技术栈**——NVL72 上每个模型的完整配置。
5. **成本模型**——`$/MTok` 推导，每项精度 / EP / 分离式部署选择能换来什么。
6. **顶点 benchmark**——你的最终仓库必须包含的报告。

学完本讲，你应当能够在 Blackwell 簇上把任一 MoE 模型部署到生产规模的流量上，用精度一致性与吞吐数字为每一项 recipe 选择辩护，并为业务产出一份 `$/MTok` 成本辩护。

---

## 1. MTP —— DeepSeek V3.1 的原生推测

DeepSeek V3 引入了多 token 预测（MTP）作为训练期目标。在推理时，模型**每次前向传播输出 k+1 个 logit 预测**，而非只有一个。

### 1.1 推理流程

```text
Standard autoregressive decode:
  step 1: emit token N+1
  step 2: emit token N+2
  step 3: emit token N+3
  → 3 forward passes for 3 tokens

MTP decode (k=3):
  step 1: emit token N+1, predict N+2 and N+3
  Verification: accept N+1 (always, it's the standard output)
                accept N+2 if its prediction matches what step 2 would have produced
                accept N+3 if both N+2 accepted AND N+3 matches
  → 1 forward pass for up to 3 tokens
```

模型的 MTP 头产生这些预测。**验证逻辑**（预测的 token 是否匹配标准输出）位于推理 runtime 中。**接受率决定有效加速比**。

### 1.2 接受率

DeepSeek V3.1 的真实数据：

* Chat：token N+2 上 65-75%；N+3 上 50-65%。
* 代码生成：N+2 上 55-65%（更难预测）。
* JSON / 工具调用：80-90%（结构高度可预测）。

多样化流量下，每次的平均有效 token 数：约 1.8-2.2。

### 1.3 配置

vLLM 0.22+：

```python
LLM(
    model="deepseek-ai/DeepSeek-V3.1",
    speculative_config={
        "method": "deepseek_mtp",
        "num_speculative_tokens": 3,
    },
    ...
)
```

SGLang：

```bash
python -m sglang.launch_server \
    --model-path deepseek-ai/DeepSeek-V3.1 \
    --speculative-algorithm DeepseekMTP \
    --speculative-num-steps 3 \
    ...
```

### 1.4 吞吐影响

对于在 16× B200 NVL72 分区上以 FP4 运行的 DeepSeek V3.1，chat 工作负载：

| Decoding | 吞吐 tok/s/replica |
|----------|--------------------------|
| 贪心自回归 | ~6,500 |
| MTP k=2 | ~10,000 (1.5×) |
| MTP k=3 | ~13,000 (2.0×) |

MTP 是 DeepSeek 部署栈上**单项最大的吞吐收益**。

---

## 2. 面向 Qwen3-MoE 的 EAGLE-3

Qwen3-MoE 235B-A22B 没有原生 MTP。2026 年的生产推测路径是 **EAGLE-3**（[arXiv:2503.01840](https://arxiv.org/abs/2503.01840)），配一个单独训练或蒸馏出的草稿模型。

### 2.1 EAGLE-3 机制

与独立的草稿 LLM 不同，EAGLE-3 训练一个**轻量级推测头**，置于目标模型之上。该头**复用目标的 hidden states**，预测接下来 k 个 token。

* 无需独立草稿模型的 HBM 开销。
* 各类别接受率 75-85%。
* 2.5-3.5× 有效 decode 吞吐。

针对 Qwen3-MoE 235B-A22B，SGLang 社区发布了专为该模型训练的 EAGLE-3 头。

### 2.2 配置

SGLang：

```bash
python -m sglang.launch_server \
    --model-path Qwen/Qwen3-235B-A22B \
    --speculative-algorithm EAGLE3 \
    --speculative-draft-model-path Qwen/Qwen3-235B-A22B-EAGLE3 \
    --speculative-num-steps 3 \
    --speculative-eagle-topk 1 \
    ...
```

### 2.3 吞吐影响

对于在 8× B200 上以 FP4 运行的 Qwen3-MoE 235B-A22B，chat 工作负载：

| Decoding | 吞吐 tok/s/replica |
|----------|--------------------------|
| 贪心自回归 | ~4,500 |
| EAGLE-3 k=3 | ~10,500 (2.3×) |

与 DeepSeek + MTP 在相近的有效加速比下相当。

---

## 3. MoE 规模下的受限解码

结构化输出（工具调用、JSON、代码）要求模型输出**特定形态的序列**。**受限解码**使用语法（或 regex / FSM）将采样器限制在**仅合法 token** 上。


<details>
<summary>English original</summary>

**Part 3 · Lecture 05 — Production MoE Serving: MTP Speculation, Constrained Decode, Cost Model**

**Overview**

This is the **capstone of the course**. The previous Part 3 lectures established the architecture (Lecture 01), the silicon (Lecture 02), the parallelism (Lecture 03), and the serving topology (Lecture 04). This lecture brings them together into a **defended, production-shippable inference deployment** for DeepSeek V3.1 and Qwen3-MoE 235B-A22B on Blackwell, with a `$/MTok` cost model that another engineer can reproduce.

Topics:

1. **MTP as native speculation** for DeepSeek — configuration, acceptance rate, throughput.
2. **EAGLE-3 for Qwen3-MoE** — the non-native speculation path with draft model.
3. **Constrained decoding** — XGrammar / Outlines at MoE scale, tool-call use cases.
4. **The production stack** — full configuration for each model on NVL72.
5. **Cost model** — `$/MTok` derivation, what each precision / EP / disaggregation choice buys.
6. **Capstone benchmark** — the report your final repo must contain.

By the end you should be able to deploy either MoE model to production-shape traffic on a Blackwell cluster, defend every recipe choice with parity and throughput numbers, and produce a `$/MTok` cost defense for the business.

---

**1. MTP — native speculation for DeepSeek V3.1**

DeepSeek V3 introduced multi-token prediction (MTP) as a training-time objective. At inference time, the model emits **k+1 logit predictions per forward pass**, not just one.

**1.1 The inference flow**

```text
Standard autoregressive decode:
  step 1: emit token N+1
  step 2: emit token N+2
  step 3: emit token N+3
  → 3 forward passes for 3 tokens

MTP decode (k=3):
  step 1: emit token N+1, predict N+2 and N+3
  Verification: accept N+1 (always, it's the standard output)
                accept N+2 if its prediction matches what step 2 would have produced
                accept N+3 if both N+2 accepted AND N+3 matches
  → 1 forward pass for up to 3 tokens
```

The model's MTP heads produce the predictions. The **verification logic** (does the predicted token match the standard output) lives in the inference runtime. The **acceptance rate determines the effective speedup**.

**1.2 Acceptance rates**

Real-world for DeepSeek V3.1:

* Chat: 65-75% on token N+2; 50-65% on N+3.
* Code generation: 55-65% on N+2 (harder to predict).
* JSON / tool-call: 80-90% (highly predictable structure).

Average effective tokens per pass: ~1.8-2.2 across diverse traffic.

**1.3 Configuration**

vLLM 0.22+:

```python
LLM(
    model="deepseek-ai/DeepSeek-V3.1",
    speculative_config={
        "method": "deepseek_mtp",
        "num_speculative_tokens": 3,
    },
    ...
)
```

SGLang:

```bash
python -m sglang.launch_server \
    --model-path deepseek-ai/DeepSeek-V3.1 \
    --speculative-algorithm DeepseekMTP \
    --speculative-num-steps 3 \
    ...
```

**1.4 Throughput impact**

For DeepSeek V3.1 at FP4 on 16× B200 NVL72 partition, chat workload:

| Decoding | Throughput tok/s/replica |
|----------|--------------------------|
| Greedy autoregressive | ~6,500 |
| MTP k=2 | ~10,000 (1.5×) |
| MTP k=3 | ~13,000 (2.0×) |

MTP is the **largest single throughput win** on DeepSeek's deployment stack.

---

**2. EAGLE-3 for Qwen3-MoE**

Qwen3-MoE 235B-A22B has no native MTP. The 2026 production speculation path is **EAGLE-3** ([arXiv:2503.01840](https://arxiv.org/abs/2503.01840)) with a separately-trained or distilled draft model.

**2.1 EAGLE-3 mechanics**

Unlike a separate draft LLM, EAGLE-3 trains a **lightweight speculation head** that sits on top of the target model. The head **reuses the target's hidden states** and predicts next-k tokens.

* No separate draft model HBM cost.
* Acceptance rates 75-85% across categories.
* 2.5-3.5× effective decode throughput.

For Qwen3-MoE 235B-A22B, an EAGLE-3 head trained specifically for this model is published by the SGLang community.

**2.2 Configuration**

SGLang:

```bash
python -m sglang.launch_server \
    --model-path Qwen/Qwen3-235B-A22B \
    --speculative-algorithm EAGLE3 \
    --speculative-draft-model-path Qwen/Qwen3-235B-A22B-EAGLE3 \
    --speculative-num-steps 3 \
    --speculative-eagle-topk 1 \
    ...
```

**2.3 Throughput impact**

For Qwen3-MoE 235B-A22B at FP4 on 8× B200, chat workload:

| Decoding | Throughput tok/s/replica |
|----------|--------------------------|
| Greedy autoregressive | ~4,500 |
| EAGLE-3 k=3 | ~10,500 (2.3×) |

Comparable to DeepSeek + MTP at a similar effective speedup ratio.

---

**3. Constrained decoding at MoE scale**

Structured output (tool calls, JSON, code) requires the model to emit **specifically-shaped sequences**. **Constrained decoding** uses a grammar (or regex / FSM) to restrict the sampler to **legal tokens only**.

</details>

### 3.1 XGrammar vs Outlines

| 库 | 做法 | 所在位置 |
|---------|----------|----------------|
| XGrammar | 把语法编译成 FSM，在 logit 层应用 | SGLang（原生）、vLLM（集成） |
| Outlines | 正则 / Pydantic，对复杂语法较慢 | vLLM、llama.cpp |

**XGrammar 是 2026 年年中最快的生产级选项**。SGLang 原生自带。

### 3.2 MoE FP4 下的 BFCL 精度一致性

[BFCL 评估讲座](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md)框架对 MoE 推理的适用方式与对 dense 完全一致。在 MoE FP4 下，工具调用准确率就是决定 FP4 能否发布的精度一致性门槛。

对于 DeepSeek V3.1：

| 精度 | BFCL（简单） | BFCL（多轮） | 相对 BF16 的 Δ |
|-----------|---------------|-------------------|-----------|
| BF16 参考 | 88.5 | 79.2 | — |
| FP8 | 88.1 | 78.7 | -0.4 / -0.5 |
| FP4 | 87.0 | 77.4 | -1.5 / -1.8 |
| FP4 + 语法约束解码 | 88.2 | 78.9 | -0.3 / -0.3 |

语法约束解码**在结构化输出工作负载上挽回了 FP4 大部分精度一致性损失**。这就是 FP4 下 agent 产品的 recipe。

### 3.3 配置

SGLang：

```python
from sglang import RuntimeEndpoint, ChatTemplate

# Define schema
schema = {
    "type": "object",
    "properties": {
        "tool": {"type": "string", "enum": ["home_control", "search", "memory"]},
        "args": {"type": "object"},
    },
    "required": ["tool", "args"],
}

# Request with structured output
response = client.chat.completions.create(
    model=endpoint,
    messages=...,
    response_format={"type": "json_schema", "json_schema": {"schema": schema}},
)
```

XGrammar 在 logit 采样层生效。对于格式良好的语法，吞吐代价极小（比无约束解码慢约 5%）。

---

## 4. 生产栈

综合第 3 部分的全部内容：

### 4.1 DeepSeek V3.1 生产 recipe

```text
Hardware:           NVL72 partition, 16× B200 (TP=2 × EP=8)
Weights:            FP4 (MX-FP4 via TE2)
Activations:        FP8
KV cache:           BF16 (MLA-compressed; small, no precision drop needed)
Attention:          MLA-aware kernels (SGLang or vLLM 0.22+)
Speculation:        MTP k=3
Serving:            SGLang 0.5+ V1
Features:           continuous batching, paged KV, RadixAttention prefix cache,
                    chunked prefill (8K chunks), XGrammar (if agent workload)
Disaggregation:     enabled if cluster has > 16 GPUs available; SGLang P/D mode

Expected throughput: ~13,000 tok/s/replica (with MTP)
Expected $/MTok:    ~$1.9-2.0 raw replica cost (16× B200 @ ~$5.50/GPU-hr — derived in §5.2)
```

### 4.2 Qwen3-MoE 235B-A22B 生产 recipe

```text
Hardware:           8× B200 (EP=8) for single-replica; 16-32× for multi-replica
Weights:            FP4 (MX-FP4)
Activations:        FP8
KV cache:           FP8 per-head (GQA, 4 KV heads, 94 layers — meaningful at long context)
Speculation:        EAGLE-3 k=3 (with the model-specific draft head)
Serving:            SGLang 0.5+ V1
Features:           continuous batching, paged KV, prefix cache, chunked prefill,
                    XGrammar
Disaggregation:     usually not worth it at this scale; ship colocated

Expected throughput: ~10,500 tok/s/replica
Expected $/MTok:    ~$1.1-1.2 raw replica cost (8× B200 @ ~$5.50/GPU-hr)
```

---

## 5. 成本模型

推导 `$/MTok` 是**最终的 exit-criterion 交付物**。

### 5.1 公式

```text
$/MTok = (replica_cost_per_hour × 10^6) / (3600 × output_tokens_per_sec)
```

输入：

* **replica_cost_per_hour** — GPU 成本 × 一个副本中的 GPU 数量 + 摊销的基础设施成本（网络、存储、调度器）。
* **output_tokens_per_sec** — 工作负载典型并发下实测的吞吐。

### 5.2 GB200 NVL72 成本模型

2026 年云上价格的大致水平：

| GPU 类别 | $/hour（单块 GPU） |
|-----------|------------------|
| H100 SXM | ~$2.50 |
| H200 SXM | ~$3.50 |
| B200 SXM | ~$5.50 |
| B300 SXM | ~$7.00 |
| GB200（每块 Blackwell GPU） | ~$5.50（在超大规模云上与 B200 SXM 相近） |

DeepSeek V3.1 的 16 卡副本：

* 硬件：16 × $5.50 = $88/hour
* 网络 + 调度器：~$2/hour
* 合计：~$90/hour

若吞吐为 13,000 tok/s/副本（启用 MTP）：

```text
$/MTok = ($90 × 10^6) / (3600 × 13,000)
       = $90,000,000 / 46,800,000
       ≈ $1.92 per million tokens
```

现在来对比：DeepSeek V3.1 API 公布的价格（$/MTok) is ~$0.30 输入与 ~$1.10 输出，随供应商而异）——**低于**这个单副本裸算的估算值。这一差距说明，真实部署的利用率和批处理规模远高于本例的假设，此外还有供应商规模的经济性（prefix caching、跨副本的流量混合、承诺用量硬件定价）。这个 $1.92 只是教学锚点，并非优化后集群的真实水平。

**推理工程师的辩护要点：**给出满利用率下的裸 $/MTok。产品团队会再乘上开销。


<details>
<summary>English original</summary>

**3.1 XGrammar vs Outlines**

| Library | Approach | Where it lives |
|---------|----------|----------------|
| XGrammar | compile grammar to FSM, apply at logit level | SGLang (native), vLLM (integration) |
| Outlines | regex / Pydantic, slower for complex grammars | vLLM, llama.cpp |

**XGrammar is the fastest production option** in mid-2026. SGLang ships it natively.

**3.2 BFCL parity at MoE FP4**

The [BFCL evaluation lecture](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) framework applies to MoE inference exactly as to dense. At MoE FP4, tool-call accuracy is the parity gate that decides if FP4 ships.

For DeepSeek V3.1:

| Precision | BFCL (simple) | BFCL (multi-turn) | Δ vs BF16 |
|-----------|---------------|-------------------|-----------|
| BF16 reference | 88.5 | 79.2 | — |
| FP8 | 88.1 | 78.7 | -0.4 / -0.5 |
| FP4 | 87.0 | 77.4 | -1.5 / -1.8 |
| FP4 + grammar-constrained decoding | 88.2 | 78.9 | -0.3 / -0.3 |

Grammar-constrained decoding **recovers most of the FP4 parity loss on structured-output workloads**. This is the recipe for agent products at FP4.

**3.3 Configuration**

SGLang:

```python
from sglang import RuntimeEndpoint, ChatTemplate

# Define schema
schema = {
    "type": "object",
    "properties": {
        "tool": {"type": "string", "enum": ["home_control", "search", "memory"]},
        "args": {"type": "object"},
    },
    "required": ["tool", "args"],
}

# Request with structured output
response = client.chat.completions.create(
    model=endpoint,
    messages=...,
    response_format={"type": "json_schema", "json_schema": {"schema": schema}},
)
```

XGrammar applies at the logit-sampling level. Throughput cost is minimal (~5% slower than unconstrained decoding) for well-formed grammars.

---

**4. The production stack**

Synthesizing everything from Part 3:

**4.1 DeepSeek V3.1 production recipe**

```text
Hardware:           NVL72 partition, 16× B200 (TP=2 × EP=8)
Weights:            FP4 (MX-FP4 via TE2)
Activations:        FP8
KV cache:           BF16 (MLA-compressed; small, no precision drop needed)
Attention:          MLA-aware kernels (SGLang or vLLM 0.22+)
Speculation:        MTP k=3
Serving:            SGLang 0.5+ V1
Features:           continuous batching, paged KV, RadixAttention prefix cache,
                    chunked prefill (8K chunks), XGrammar (if agent workload)
Disaggregation:     enabled if cluster has > 16 GPUs available; SGLang P/D mode

Expected throughput: ~13,000 tok/s/replica (with MTP)
Expected $/MTok:    ~$1.9-2.0 raw replica cost (16× B200 @ ~$5.50/GPU-hr — derived in §5.2)
```

**4.2 Qwen3-MoE 235B-A22B production recipe**

```text
Hardware:           8× B200 (EP=8) for single-replica; 16-32× for multi-replica
Weights:            FP4 (MX-FP4)
Activations:        FP8
KV cache:           FP8 per-head (GQA, 4 KV heads, 94 layers — meaningful at long context)
Speculation:        EAGLE-3 k=3 (with the model-specific draft head)
Serving:            SGLang 0.5+ V1
Features:           continuous batching, paged KV, prefix cache, chunked prefill,
                    XGrammar
Disaggregation:     usually not worth it at this scale; ship colocated

Expected throughput: ~10,500 tok/s/replica
Expected $/MTok:    ~$1.1-1.2 raw replica cost (8× B200 @ ~$5.50/GPU-hr)
```

---

**5. The cost model**

Deriving `$/MTok` is the **final exit-criterion deliverable**.

**5.1 The formula**

```text
$/MTok = (replica_cost_per_hour × 10^6) / (3600 × output_tokens_per_sec)
```

Inputs:

* **replica_cost_per_hour** — GPU cost × number of GPUs in a replica + amortized infrastructure (network, storage, scheduler).
* **output_tokens_per_sec** — measured throughput at the workload's typical concurrency.

**5.2 GB200 NVL72 cost model**

Approximate 2026 cloud rates:

| GPU class | $/hour (one GPU) |
|-----------|------------------|
| H100 SXM | ~$2.50 |
| H200 SXM | ~$3.50 |
| B200 SXM | ~$5.50 |
| B300 SXM | ~$7.00 |
| GB200 (per Blackwell GPU) | ~$5.50 (similar to B200 SXM on hyperscalers) |

A 16-GPU replica of DeepSeek V3.1:

* Hardware: 16 × $5.50 = $88/hour
* Network + scheduler: ~$2/hour
* Total: ~$90/hour

If throughput is 13,000 tok/s/replica (with MTP):

```text
$/MTok = ($90 × 10^6) / (3600 × 13,000)
       = $90,000,000 / 46,800,000
       ≈ $1.92 per million tokens
```

Now compare: the published rate for DeepSeek V3.1 API ($/MTok) is ~$0.30 input and ~$1.10 output (varies by provider) — **below** this raw single-replica estimate. That gap tells you real deployments run at much higher utilization and batching than this example assumes, on top of provider-scale economics (prefix caching, traffic mixing across replicas, committed-hardware pricing). The $1.92 is a teaching anchor, not the truth of an optimized fleet.

**For the inference engineer's defense:** show the raw $/MTok at full utilization. The product team multiplies by overhead.

</details>

### 5.3 什么在驱动成本模型

| 杠杆 | 对 $/MTok 的影响 | 范围 |
|-------|------------------|-------|
| FP4 vs FP8 | -30 到 -40% | 权重精度下降 |
| MTP / EAGLE | -40 到 -55% | 有效吞吐倍增 |
| 分离式部署（NVL72 域内） | -20 到 -35% | 各阶段的硬件优化 |
| 并发 16 → 64 | -50 到 -65% | 批成本摊销 |
| 并发 64 → 256 | -10 到 -20% | 收益递减 |
| EP=8 → EP=16 | -10 到 -20% | 单 GPU 压力更小 |

一个朴素部署（FP8 + 贪心解码 + 并发 16 + EP=2）的成本可能达到 $4-6/MTok (greedy alone drops the replica to ~6,500 tok/s, or ~$3.85/MTok；FP8 和低并发会把它进一步推高）。同一模型在 FP4 + MTP + EP=8 + 并发 64 下可做到 $2 或更低。**这套优化组合把成本模型拉动了 2-3×。**

---

## 6. Capstone benchmark —— 你的 repo 必须包含什么

本课程的**最终交付物**。你的 benchmark repo 包含：

### 6.1 可复现性层

* 确切的硬件（GPU SKU、驱动、CUDA、cuDNN、FA、TE 版本）。
* 确切的 runtime 版本与 commit hash。
* 来自 Hugging Face 的确切模型 hash。
* 校准数据 manifest（若使用 AWQ）。

### 6.2 精度一致性报告

对每个模型 × 精度 recipe：

* 参考噪声底（FP16/BF16，两个 seed）。
* 候选实现在工作负载评测集上的 Δ。
* 若精度一致性超出预算，做失效模式分析。

### 6.3 吞吐表

对每个（模型 × runtime × 精度 × EP × 并发）单元：

* TTFT p50/p95/p99。
* TPOT p50/p95/p99。
* 吞吐 tok/s/replica 与 tok/s/GPU。
* 按 §5 成本模型计算的有效 $/MTok。

### 6.4 Profile

至少一份 Nsight Systems profile，覆盖：

* TP 全规约主导区间（第 2 部分）。
* EP all-to-all 主导区间（第 3 部分）。
* 在飞的 MTP / EAGLE 推测。

### 6.5 叙述

一份 3 页的 markdown 总结，说明：

* 工作负载与 SLO 目标。
* 每个（模型, 硬件）组合的 recipe 选择。
* $/MTok 的论证。
* 若集群规模翻倍，你会改什么。

这份叙述正是资深工程师在批准部署前要审阅的内容。**它就是 capstone。**

---

## Lab —— 产出 capstone 报告

目标：为 DeepSeek V3.1 或 Qwen3-MoE 235B-A22B 之一产出最终的 capstone benchmark 报告。

1. **选定一个模型**和一种工作负载类别（chat 或 agent）。
2. **定义 SLO** —— TTFT、TPOT、吞吐目标。
3. **硬件** —— 你能拿到的资源（8× B200、NVL72 分区等）。
4. **构建矩阵** —— 至少四种配置，覆盖不同的精度 × EP × 推测选择。
5. **逐一 benchmark**，并做精度一致性验证。
6. **计算每个配置的 $/MTok**。
7. **撰写叙述** —— 推荐一个可上线的 recipe，并给出经得起推敲的数字。

通过标准：另一位工程师能 clone 该 repo、运行 `make bench`，并在 ±10% 内复现你的数字。

---

## 自检

1. MTP 为 DeepSeek V3.1 带来 2× 的 decode（逐 token 生成阶段）加速，且不额外占用 HBM。EAGLE-3 为 Qwen3-MoE 带来 2.3× 加速，代价是少量 EAGLE head 的 HBM 开销。即使已有 MTP-DeepSeek，为什么在 code-gen 工作负载上仍可能选 EAGLE-3？
2. 你的 XGrammar 约束解码使 TPOT 增加 5%。BFCL 准确率在 FP4 下提升 4 pp。对 agent 产品而言，这个取舍能上线吗？
3. 在 NVL72 单副本 DeepSeek V3.1、EP=64、FP4 加 MTP 的配置下，预测你的 $/MTok if the per-GPU cost is $5.50/小时，且吞吐为 20,000 tok/s/replica。
4. 产品团队希望 chat 产品达到 0.5s 的 TTFT 和 30ms 的 TPOT。你上线哪个模型 + recipe：DeepSeek V3.1 NVL72 还是 Qwen3-MoE 235B-A22B 8× B200？用两句话论证。
5. 成本模型显示 FP4 省 35%、MTP 省 50%、EP=16 省 15%。为什么总节省不是 100%？

---

## 参考文献

* DeepSeek V3 MTP —— [arXiv:2412.19437](https://arxiv.org/abs/2412.19437)
* "Multi-token Prediction" —— [arXiv:2404.19737](https://arxiv.org/abs/2404.19737)
* EAGLE-3 —— [arXiv:2503.01840](https://arxiv.org/abs/2503.01840)
* XGrammar —— [arXiv:2411.15100](https://arxiv.org/abs/2411.15100)
* Outlines —— [github.com/dottxt-ai/outlines](https://github.com/dottxt-ai/outlines)
* BFCL —— [gorilla.cs.berkeley.edu/leaderboard.html](https://gorilla.cs.berkeley.edu/leaderboard.html)
* SGLang DeepSeek 推理服务指南 —— [sgl-project.github.io](https://sgl-project.github.io/)
* DeepSeek V3.1 官方推理指南 —— [github.com/deepseek-ai/DeepSeek-V3](https://github.com/deepseek-ai/DeepSeek-V3)

交叉引用：

* [阶段 5 → 边缘 AI → 用 BFCL 做 Agent 工具分发评估](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md)
* [第 2 部分 → 第 05 讲 —— 现代推理服务栈](../02-Hopper稠密计算/Lecture-05.md)
* [阶段 5 → ML Systems Engineering Guide → Capstone 选项](../../Guide.md)

---


<details>
<summary>English original</summary>

**5.3 What moves the cost model**

| Lever | Effect on $/MTok | Range |
|-------|------------------|-------|
| FP4 vs FP8 | -30 to -40% | precision drop on weights |
| MTP / EAGLE | -40 to -55% | effective throughput multiplier |
| Disaggregation (NVL72 in-domain) | -20 to -35% | hardware optimization per phase |
| Concurrency 16 → 64 | -50 to -65% | batch amortization |
| Concurrency 64 → 256 | -10 to -20% | diminishing returns |
| EP=8 → EP=16 | -10 to -20% | less per-GPU pressure |

A naive deployment at FP8 + greedy decoding + concurrency 16 + EP=2 might cost $4-6/MTok (greedy alone drops the replica to ~6,500 tok/s, or ~$3.85/MTok; FP8 and the low concurrency push it further). The same model at FP4 + MTP + EP=8 + concurrency 64 hits $2 or less. **The optimization stack moves the cost model by 2-3×.**

---

**6. The capstone benchmark — what your repo must contain**

The **final deliverable** of this course. Your benchmark repo contains:

**6.1 Reproducibility layer**

* Exact hardware (GPU SKU, driver, CUDA, cuDNN, FA, TE versions).
* Exact runtime versions and commit hashes.
* Exact model hashes from Hugging Face.
* Calibration data manifests (if AWQ used).

**6.2 Parity reports**

For each model × precision recipe:

* Reference noise floor (FP16/BF16 across two seeds).
* Candidate Δ on the workload's eval set.
* Failure-mode analysis if parity exceeded budget.

**6.3 Throughput tables**

For each (model × runtime × precision × EP × concurrency) cell:

* TTFT p50/p95/p99.
* TPOT p50/p95/p99.
* Throughput tok/s/replica and tok/s/GPU.
* Effective $/MTok at the cost model from §5.

**6.4 Profiles**

At least one Nsight Systems profile for:

* TP all-reduce dominated regime (Part 2).
* EP all-to-all dominated regime (Part 3).
* MTP / EAGLE speculation in flight.

**6.5 The narrative**

A 3-page markdown summary explaining:

* The workload and SLO targets.
* The recipe choice for each (model, hardware) pair.
* The $/MTok defense.
* What you would change if the cluster scale doubled.

This narrative is what a senior engineer reviews before signing off on the deployment. **It is the capstone.**

---

**Lab — produce the capstone report**

Goal: the final capstone benchmark report for either DeepSeek V3.1 or Qwen3-MoE 235B-A22B.

1. **Pick one model** and one workload class (chat or agent).
2. **Define the SLO** — TTFT, TPOT, throughput targets.
3. **Hardware** — what you have access to (8× B200, NVL72 partition, etc.).
4. **Build matrix** — at least four configurations covering different precision × EP × speculation choices.
5. **Bench each** with parity validation.
6. **Compute $/MTok** for each.
7. **Write the narrative** — recommend a ship recipe with defended numbers.

Pass criterion: another engineer can clone the repo, run `make bench`, and reproduce your numbers within ±10%.

---

**Self-check**

1. MTP delivers 2× decode speedup for DeepSeek V3.1 with no extra HBM. EAGLE-3 delivers 2.3× speedup for Qwen3-MoE with a small EAGLE-head HBM cost. Why might you still pick EAGLE-3 for a code-gen workload even if MTP-DeepSeek is available?
2. Your XGrammar-constrained decoding adds 5% to TPOT. Your BFCL accuracy improves 4 pp at FP4. Does this trade ship for an agent product?
3. At NVL72 single-replica DeepSeek V3.1 EP=64 FP4 with MTP, predict your $/MTok if the per-GPU cost is $5.50/hour and throughput is 20,000 tok/s/replica.
4. A product team wants 0.5s TTFT and 30ms TPOT for a chat product. Which model + recipe do you ship: DeepSeek V3.1 NVL72 or Qwen3-MoE 235B-A22B 8× B200? Defend in two sentences.
5. The cost model says FP4 saves 35%, MTP saves 50%, EP=16 saves 15%. Why is the total savings not 100%?

---

**References**

* DeepSeek V3 MTP — [arXiv:2412.19437](https://arxiv.org/abs/2412.19437)
* "Multi-token Prediction" — [arXiv:2404.19737](https://arxiv.org/abs/2404.19737)
* EAGLE-3 — [arXiv:2503.01840](https://arxiv.org/abs/2503.01840)
* XGrammar — [arXiv:2411.15100](https://arxiv.org/abs/2411.15100)
* Outlines — [github.com/dottxt-ai/outlines](https://github.com/dottxt-ai/outlines)
* BFCL — [gorilla.cs.berkeley.edu/leaderboard.html](https://gorilla.cs.berkeley.edu/leaderboard.html)
* SGLang DeepSeek serving guide — [sgl-project.github.io](https://sgl-project.github.io/)
* DeepSeek V3.1 official inference guide — [github.com/deepseek-ai/DeepSeek-V3](https://github.com/deepseek-ai/DeepSeek-V3)

Cross-references:

* [Phase 5 → Edge AI → Agent Tool-Dispatch Evaluation with BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md)
* [Part 2 → Lecture 05 — Modern serving stack](../02-Hopper稠密计算/Lecture-05.md)
* [Phase 5 → ML Systems Engineering Guide → Capstone Options](../../Guide.md)

---

</details>

## 截至 2026-06

MTP 与 EAGLE-3 作为 2025–2026 的规范推测解码路径。XGrammar 作为标准的约束解码库。SGLang 作为 MoE（混合专家模型）的生产级推理服务 runtime。NVL72 成本模型基于当前云定价。当出现新的推测方法或定价显著变动时刷新。

---

## 第 3 部分结束 — 课程结束

恭喜。你已完成 AI Inference Engineer 2026。

证明就是那个 benchmark repo。如果另一位工程师能运行它并复现你的数据，你就是这门课程旨在培养的资深推理工程师。

* 返回：[AI Inference Engineer 2026 — Overview](../README.md)
* 上级：[阶段 5 → Track G — ML Systems Engineering](../../Guide.md)
* 刷新日志：[REFRESH-LOG.md](../REFRESH-LOG.md)


<details>
<summary>English original</summary>

**Current as of 2026-06**

MTP and EAGLE-3 as canonical 2025–2026 speculation paths. XGrammar as the standard constrained-decode library. SGLang as the production serving runtime for MoE. NVL72 cost model from current cloud pricing. Refresh when new speculation methods land or when pricing shifts significantly.

---

**End of Part 3 — End of Course**

Congratulations. You have completed AI Inference Engineer 2026.

The proof is the benchmark repo. If another engineer can run it and reproduce your numbers, you are the senior inference engineer the course was designed to produce.

* Back to: [AI Inference Engineer 2026 — Overview](../README.md)
* Up: [Phase 5 → Track G — ML Systems Engineering](../../Guide.md)
* Refresh Log: [REFRESH-LOG.md](../REFRESH-LOG.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 3 - MoE at Blackwell/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%203%20-%20MoE%20at%20Blackwell/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
