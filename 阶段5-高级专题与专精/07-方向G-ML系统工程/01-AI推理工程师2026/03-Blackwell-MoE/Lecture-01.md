---
title: Part 3 · 第 01 讲 —— 现代 MoE 剖析：DeepSeek V3.1 与 Qwen3-MoE 235B-A22B
description: Part 3 · 第 01 讲 —— 现代 MoE 剖析：DeepSeek V3.1 与 Qwen3-MoE 235B-A22B
---

# Part 3 · 第 01 讲 —— 现代 MoE 剖析：DeepSeek V3.1 与 Qwen3-MoE 235B-A22B

## 概览

混合专家模型（MoE）以一种非常具体的方式改变了推理的经济性：**总参数与激活参数变成两个不同的数字**，而 runtime 的职责就是把第二个数字用好。

在 Hopper 上，一个稠密的 72B 模型每 token 要做 72B 参数的工作量。一个 235B-A22B MoE 每 token 占用的内存相当于 235B 参数，但每 token 的计算量只有 22B —— 因为**门控网络把每个 token 路由到一小部分专家**。**HBM 压力看起来像 235B 模型**；**FLOPs 看起来像 22B 模型**。这大幅改变了带宽/计算比：在相同的激活参数规模下，MoE decode（逐 token 生成阶段）比稠密 decode *更*受带宽约束。

本讲并排梳理这两个锚点模型：

* **DeepSeek V3.1** —— 671B 总参数 / 37B 激活，MLA（多头潜在注意力）attention，MTP（多 token 预测）head，256+1 个专家，top-8 路由。
* **Qwen3-MoE 235B-A22B** —— 235B 总参数 / 22B 激活，GQA（分组查询注意力）attention，128 个专家，top-8 路由。

主题：

1. 共有的 MoE 骨架 —— 每个现代 MoE 都会交付的部分。
2. **MLA** —— DeepSeek 的 KV 压缩，2025 年最具辨识度的架构选择。
3. **MTP** —— DeepSeek 原生的多 token 预测，内建在模型里的免费投机。
4. 专家层 —— 数量、尺寸、top-k 路由、共享专家。
5. 用具体数字看这两个模型 —— config、总参数、激活参数、每 token 的 KV、decode 带宽上限。
6. 相比稠密模型（Part 2 第 01 讲），推理图有什么变化。
7. 为什么 MoE 的推理经济性不同 —— 以及 runtime 必须做哪些不同的处理。

读完后，你应当能读懂任一模型的 config，并预测它在 Blackwell 上的推理成本形态 —— 总 HBM 占用、每 token decode 带宽、每 token 计算量，以及 runtime 必须在哪些地方做出与稠密部署不同的选择。

---

## 1. 共有的 MoE 骨架

DeepSeek V3.1 与 Qwen3-MoE 235B-A22B 都实现了同一套标准的 MoE 模块。Transformer 层只在 FFN 上有变化：

```text
input hidden states (h)
       │
       ▼
   RMSNorm
       │
       ▼
   Attention block   (MLA for DeepSeek; GQA for Qwen3-MoE)
       │
       ▼
   residual add
       │
       ▼
   RMSNorm
       │
       ▼
   ┌─────────────── MoE FFN ─────────────────┐
   │  gating network: g(h) → expert scores   │
   │  topk(scores, k=8) → selected experts   │
   │  for each selected expert e:            │
   │      out_e = expert_e(h)                │
   │  output = sum_e (gate_score_e × out_e)  │
   │  (+ shared expert output, DeepSeek)     │
   └─────────────────────────────────────────┘
       │
       ▼
   residual add → next layer
```

计算模式：

* **门控网络**是一个小的 Linear 层（hidden → num_experts），每一步都会运行。它很廉价（FLOPs 可忽略），但每一层都**引入一次 token 路由决策**。
* **每个 token 从 N 个专家中激活 k 个** —— DeepSeek 是 256 个中取 top-8，Qwen3-MoE 是 128 个中取 top-8。因此每 token 触及的专家容量为 8/256 ≈ 3.1%，Qwen3-MoE 则是 8/128 = 6.25%。
* **共享专家**（仅 DeepSeek）是一条小的 Linear 通路，对*每一个* token 都运行。它吸收所有 token 都需要的“通用知识”。
* **输出**是所选专家输出的加权和。

“激活”在参数计数中的含义是：**只有该 token 实际触及的专家**才计入每 token 的计算量。其余 248 个（或 120 个）专家**待在 HBM 里，但这一步不贡献任何计算**。

---

## 2. MLA —— 多头潜在注意力（DeepSeek）

2024–2025 年最具辨识度的架构选择。**相比完整的 MHA，MLA 把 KV cache 压缩约 15×；相比 70B 级模型典型的 GQA 配置（§2.2），压缩约 4×**，这是长上下文下单项收益最大的推理优化。

### 2.1 思路

标准 GQA 每 token 存储 `K, V ∈ R^(L × h_kv × head_dim)` —— 显式的 key/value 向量。MLA 则每 token 存储一个小的**潜在向量** `c_kv ∈ R^(L × d_c)`，其中 `d_c << h_kv × head_dim`。

在 attention 计算时，K 和 V *由 latent 重建*：

```text
K_t = W_uK · c_t      (W_uK: d_c → h_kv × head_dim, low-rank up-projection)
V_t = W_uV · c_t
```

latent `c_t` 就是实际存储的缓存。K 和 V 在每个 decode 步骤中即时重算。这是一个**内存/计算取舍**：用更多计算（每层每步一次小矩阵乘）换取**大幅减少的 KV 内存**。

有一个细节：**旋转位置编码（RoPE）不能搭在 latent 里**。RoPE 对每个 key 施加与位置相关的旋转，而该旋转与低秩上投影 `W_uK` 不可交换 —— 把位置折进压缩后的 latent，就意味着每来一个新的 query 位置都要对缓存值重新旋转。DeepSeek 的解法是一个**解耦的 rotary key**：每 token 一个小的 64 维 key（`qk_rope_head_dim = 64`），旋转一次后与 latent 一起缓存。latent 保持不含位置；由 rotary key 承载位置。


<details>
<summary>English original</summary>

**Part 3 · Lecture 01 — Anatomy of a Modern MoE: DeepSeek V3.1 and Qwen3-MoE 235B-A22B**

**Overview**

Mixture-of-Experts changes the inference economics in one specific way: **total parameters and active parameters become two different numbers**, and the runtime's job is to use the second one well.

A dense 72B model on Hopper does 72B params of work per token. A 235B-A22B MoE does 235B of params *worth of memory* but only 22B of *compute per token* — because the **gating network routes each token to a small fraction of experts**. The **HBM pressure looks like a 235B model**; the **FLOPs look like a 22B model**. This shifts the bandwidth/compute ratio sharply: MoE decode is *even more* bandwidth-bound than dense decode at the same active-param scale.

This lecture walks the two anchor models side by side:

* **DeepSeek V3.1** — 671B total / 37B active, MLA attention, MTP head, 256+1 experts, top-8 routing.
* **Qwen3-MoE 235B-A22B** — 235B total / 22B active, GQA attention, 128 experts, top-8 routing.

Topics:

1. The shared MoE skeleton — what every modern MoE ships.
2. **MLA** — DeepSeek's KV compression, the most distinctive 2025 architectural choice.
3. **MTP** — DeepSeek's native multi-token prediction, free speculation built into the model.
4. The expert layer — count, sizing, top-k routing, shared experts.
5. The two models in concrete numbers — config, total params, active params, KV per token, decode bandwidth ceiling.
6. What changes in the inference graph compared to dense (Part 2 Lecture 01).
7. Why MoE inference economics differ — and what the runtime has to do differently.

By the end you should be able to read either model's config and predict its inference cost shape on Blackwell — total HBM footprint, per-token decode bandwidth, per-token compute, and where the runtime has to choose differently from a dense deployment.

---

**1. The shared MoE skeleton**

Both DeepSeek V3.1 and Qwen3-MoE 235B-A22B implement the same canonical mixture-of-experts block. The transformer layer changes only in the FFN:

```text
input hidden states (h)
       │
       ▼
   RMSNorm
       │
       ▼
   Attention block   (MLA for DeepSeek; GQA for Qwen3-MoE)
       │
       ▼
   residual add
       │
       ▼
   RMSNorm
       │
       ▼
   ┌─────────────── MoE FFN ─────────────────┐
   │  gating network: g(h) → expert scores   │
   │  topk(scores, k=8) → selected experts   │
   │  for each selected expert e:            │
   │      out_e = expert_e(h)                │
   │  output = sum_e (gate_score_e × out_e)  │
   │  (+ shared expert output, DeepSeek)     │
   └─────────────────────────────────────────┘
       │
       ▼
   residual add → next layer
```

The compute pattern:

* **The gating network** is a small Linear layer (hidden → num_experts) that runs every step. It is cheap (FLOPs negligible) but **introduces a token-routing decision** every layer.
* **Each token activates k experts** out of N — top-8 of 256 for DeepSeek, top-8 of 128 for Qwen3-MoE. So 8/256 ≈ 3.1% of expert capacity is touched per token, or 8/128 = 6.25% for Qwen3-MoE.
* **The shared expert** (DeepSeek only) is a small Linear path that runs for *every* token. It absorbs the "common knowledge" that all tokens need.
* **The output** is a weighted sum of the selected experts' outputs.

What "active" means in the param count: **only the experts the token actually touched** count toward the per-token compute. The other 248 (or 120) experts **sit in HBM but contribute nothing** this step.

---

**2. MLA — Multi-head Latent Attention (DeepSeek)**

The most distinctive 2024–2025 architectural choice. **MLA compresses the KV cache by ~15× compared to full MHA, and ~4× compared to the GQA configs typical of 70B-class models (§2.2)**, which is the single biggest inference win at long context.

**2.1 The idea**

Standard GQA stores `K, V ∈ R^(L × h_kv × head_dim)` per token — the explicit key/value vectors. MLA instead stores a small **latent vector** `c_kv ∈ R^(L × d_c)` per token, where `d_c << h_kv × head_dim`.

At attention time, K and V are *reconstructed from the latent*:

```text
K_t = W_uK · c_t      (W_uK: d_c → h_kv × head_dim, low-rank up-projection)
V_t = W_uV · c_t
```

The latent `c_t` is the stored cache. K and V are recomputed on-the-fly each decode step. This is a **memory/compute tradeoff**: more compute (a small matmul per layer per step) for **dramatically less KV memory**.

One subtlety: **the rotary embedding cannot ride inside the latent**. RoPE applies a position-dependent rotation to each key, and that rotation does not commute with the low-rank up-projection `W_uK` — folding position into the compressed latent would mean re-rotating the cached value for every new query position. DeepSeek's fix is a **decoupled rotary key**: a small 64-dim key per token (`qk_rope_head_dim = 64`), rotated once and cached alongside the latent. The latent stays position-free; the rotary key carries the position.

</details>

### 2.2 每 token 的 KV 字节数 — MLA vs GQA

对于 DeepSeek V3 / V3.1：

| 规格 | 取值 |
|------|-------|
| `kv_lora_rank` (d_c) | 512 |
| `qk_rope_head_dim` (decoupled rotary key) | 64 |
| `num_hidden_layers` | 61 |
| `bytes_per_element` | 2 (FP16/BF16) |

```text
mla_kv_bytes_per_token = num_layers × (d_c + qk_rope_head_dim) × bytes
                       = 61 × (512 + 64) × 2
                       = 70,272 bytes
                       ≈ 69 KB / token
```

对于一个假设的 64 head、8 个 KV head、head dim 128、61 层的 GQA 模型：

```text
gqa_kv_bytes_per_token = 2 (K + V) × 61 × 8 × 128 × 2
                       = 249,856 bytes
                       ≈ 244 KB / token
```

MLA 比等效 GQA **小约 3.5×** —— 而 DeepSeek V3 的**层数（61）比其稠密竞争对手更多**，因此绝对节省量更大。

在 128K 上下文下，DeepSeek V3.1 每个请求需要 **约 9 GB 的 KV cache**，而等效 GQA 需要约 30 GB+。正因如此，**无需激进的 KV 量化，long-context 推理服务也能实际可用**。

### 2.3 MLA 的代价

* 每个 decode step 额外的矩阵乘：`W_uK · c_t` 与 `W_uV · c_t`。每 layer 每 step 两次小矩阵乘。
* 在 Blackwell 上以 FP4 运行时，这些开销很低 —— 其计算量可以轻松塞进 FFN 矩阵乘与 attention 之间的空闲周期。
* runtime 需要能感知 MLA 的 kernel —— 采用 MLA 形状的 FlashAttention 4，或 SGLang 的 DeepSeek 路径中的专用 kernel。

截至 2026 年年中，vLLM、SGLang 和 TensorRT-LLM 均已支持 MLA，其中 SGLang 的 DeepSeek 专用 kernel 最为成熟。

---

## 3. MTP — 多 token 预测（DeepSeek）

DeepSeek V3 将 MTP 作为*训练期*目标引入：预测接下来 *k* 个 token，而不只是下一个。在推理时，模型已学会同时给出位置 +1、+2、…、+k 的预测。

### 3.1 它如何改变推理

```text
Standard decode step:           emits one token
With MTP active:                emits up to k tokens, with confidence scores

Verification:
  At step t, target model has emitted tokens t+1, t+2, t+3 (k=3)
  Continue from t+3 unless any of them is rejected by sampling logic
```

这是内置于模型中的**原生投机解码** —— 无需单独的 draft model。

对于 DeepSeek V3.1：

* 接受率（粗略）：k=3 时 60-80%（依赖样本）。
* 有效吞吐：无额外 draft 开销的情况下，decode 提速 1.6-2.5×。
* 可在 vLLM 0.22+（`speculative_config={"method": "deepseek_mtp"}`）和 SGLang 中运行。

### 3.2 这对推理工程意味着什么

MTP 使 DeepSeek 在 Blackwell 上的 decode 吞吐，大致相当于一个比其激活参数量所暗示的规模小 2× 的稠密模型。**在成本不变的情况下，带 MTP 的 DeepSeek V3.1 所服务的 token 数约为 37B 稠密模型的 3×。** 正是这一算式，让 DeepSeek 的 $/MTok 经济性在 2025-2026 年足以与闭源旗舰模型竞争。

### 3.3 Qwen3-MoE 没有原生 MTP

Qwen3-MoE 235B-A22B **并非用 MTP 目标训练**。它使用**一个轻量的 EAGLE-3 head（或 Medusa heads）**，复用目标模型的 hidden states。接受率相近（k=4 时 70-80%），但工程层不同：单独训练的 head、单独的 runtime 路径、单独的微调。

这是本课程中最清晰的「架构选择 → 推理 recipe」对照之一。

---

## 4. 专家层

### 4.1 DeepSeek V3.1 专家配置

来自 `config.json`：

| 字段 | 取值 |
|-------|-------|
| `n_routed_experts` | 256 |
| `n_shared_experts` | 1 |
| `num_experts_per_tok` | 8 |
| `moe_intermediate_size` | 2048（每个 expert FFN） |
| `routed_scaling_factor` | 2.5 |
| `first_k_dense_replace` | 3（前 3 层为稠密，不用 MoE） |

因此：

* 共 61 层；前 3 层是标准稠密 FFN，其余 58 层使用 MoE。
* 每个 MoE 层有 256 + 1 = 257 个 expert。
* 每个 token 路由到 256 个 routed expert 中的 8 个，外加 1 个 shared expert。
* 每个 expert 是一个 SwiGLU FFN，intermediate size 为 2048（远小于 DeepSeek V3 的稠密 FFN —— 后者宽 18432，约大 9×）。

每个 MoE 层的参数量：

```text
Per expert: 3 × hidden × moe_intermediate (gate + up + down)
          = 3 × 7168 × 2048
          = 44M params
Per layer (routed): 256 × 44M = 11.2B
Per layer (shared): 1 × 44M = 44M
Per layer (gating): hidden × num_experts = 7168 × 256 = 1.8M
Per layer total: ~11.3B

58 MoE layers + 3 dense layers + attention layers + embed/LM head
≈ 671B total params
```

每 token 激活参数量：

```text
Per token, per MoE layer:
  Attention: ~110M (MLA-shaped attention)
  8 routed experts × 44M + 1 shared × 44M + gating = ~352M + ~44M + ~2M ≈ ~398M
  Per-layer active: ~510M

58 MoE layers × 510M + 3 dense layers × dense-FFN-size + attention + embed
≈ 37B active params per token
```

与已公布的“37B active”相符。


<details>
<summary>English original</summary>

**2.2 KV bytes per token — MLA vs GQA**

For DeepSeek V3 / V3.1:

| Spec | Value |
|------|-------|
| `kv_lora_rank` (d_c) | 512 |
| `qk_rope_head_dim` (decoupled rotary key) | 64 |
| `num_hidden_layers` | 61 |
| `bytes_per_element` | 2 (FP16/BF16) |

```text
mla_kv_bytes_per_token = num_layers × (d_c + qk_rope_head_dim) × bytes
                       = 61 × (512 + 64) × 2
                       = 70,272 bytes
                       ≈ 69 KB / token
```

For a hypothetical 64-head, 8-KV-head, head-dim-128 GQA model with 61 layers:

```text
gqa_kv_bytes_per_token = 2 (K + V) × 61 × 8 × 128 × 2
                       = 249,856 bytes
                       ≈ 244 KB / token
```

MLA is **~3.5× smaller** than equivalent GQA — and DeepSeek V3 has **more layers (61) than its dense competitors** so the absolute savings are larger.

At 128K context, DeepSeek V3.1 needs **~9 GB of KV cache per request**, versus ~30 GB+ for a GQA equivalent. This is what makes **long-context serving practical without aggressive KV quantization**.

**2.3 What MLA costs**

* Extra matmul per decode step: `W_uK · c_t` and `W_uV · c_t`. Two small matmuls per layer per step.
* On Blackwell at FP4, these are cheap — the compute fits comfortably in the spare cycles between the FFN matmul and the attention.
* The runtime needs MLA-aware kernels — FlashAttention 4 with the MLA shape, or specialized kernels in SGLang's DeepSeek path.

vLLM, SGLang, and TensorRT-LLM all support MLA as of mid-2026, with SGLang having the most mature DeepSeek-specific kernels.

---

**3. MTP — Multi-Token Prediction (DeepSeek)**

DeepSeek V3 introduced MTP as a *training-time* objective: predict the next *k* tokens, not just the next one. At inference time, the model has learned to also emit predictions for positions +1, +2, ..., +k.

**3.1 How it changes inference**

```text
Standard decode step:           emits one token
With MTP active:                emits up to k tokens, with confidence scores

Verification:
  At step t, target model has emitted tokens t+1, t+2, t+3 (k=3)
  Continue from t+3 unless any of them is rejected by sampling logic
```

This is **native speculative decoding** built into the model — no separate draft model needed.

For DeepSeek V3.1:

* Acceptance rate (rough): 60-80% at k=3 (sample-dependent).
* Effective throughput: 1.6-2.5× decode speedup with no additional draft cost.
* Works in vLLM 0.22+ (`speculative_config={"method": "deepseek_mtp"}`) and SGLang.

**3.2 Why this matters for inference engineering**

MTP makes DeepSeek's decode throughput on Blackwell roughly equivalent to a dense model 2× smaller than its active param count would suggest. **At constant cost, DeepSeek V3.1 with MTP serves ~3× more tokens than a 37B dense model would.** That math is what makes DeepSeek's $/MTok economics competitive with closed flagship models in 2025-2026.

**3.3 Qwen3-MoE has no native MTP**

Qwen3-MoE 235B-A22B was **not trained with the MTP objective**. It uses **a lightweight EAGLE-3 head (or Medusa heads)** that reuses the target model's hidden states. Acceptance rates are similar (70-80% at k=4) but the engineering layer is different: a separately trained head, separate runtime path, separate fine-tuning.

This is one of the cleanest "architectural choice → inference recipe" contrasts in the course.

---

**4. The expert layer**

**4.1 DeepSeek V3.1 expert configuration**

From `config.json`:

| Field | Value |
|-------|-------|
| `n_routed_experts` | 256 |
| `n_shared_experts` | 1 |
| `num_experts_per_tok` | 8 |
| `moe_intermediate_size` | 2048 (per expert FFN) |
| `routed_scaling_factor` | 2.5 |
| `first_k_dense_replace` | 3 (first 3 layers are dense, no MoE) |

So:

* 61 total layers; first 3 are standard dense FFN, the remaining 58 use MoE.
* Each MoE layer has 256 + 1 = 257 experts.
* Each token routes to 8 of 256 routed experts plus the 1 shared expert.
* Each expert is a SwiGLU FFN with intermediate size 2048 (much smaller than DeepSeek V3's dense FFN, which is 18432 wide — ~9× larger).

Param count per MoE layer:

```text
Per expert: 3 × hidden × moe_intermediate (gate + up + down)
          = 3 × 7168 × 2048
          = 44M params
Per layer (routed): 256 × 44M = 11.2B
Per layer (shared): 1 × 44M = 44M
Per layer (gating): hidden × num_experts = 7168 × 256 = 1.8M
Per layer total: ~11.3B

58 MoE layers + 3 dense layers + attention layers + embed/LM head
≈ 671B total params
```

Per-token active params:

```text
Per token, per MoE layer:
  Attention: ~110M (MLA-shaped attention)
  8 routed experts × 44M + 1 shared × 44M + gating = ~352M + ~44M + ~2M ≈ ~398M
  Per-layer active: ~510M

58 MoE layers × 510M + 3 dense layers × dense-FFN-size + attention + embed
≈ 37B active params per token
```

Which matches the published "37B active." 

</details>

### 4.2 Qwen3-MoE 235B-A22B 专家配置

来自 `config.json`：

| 字段 | 值 |
|-------|-------|
| `num_experts` | 128 |
| `num_experts_per_tok` | 8 |
| `moe_intermediate_size` | 1536 |
| （无共享专家） | — |

因此：

* 所有层都是 MoE（无 first-k-dense-replace）。
* 每层 128 个专家，top-8 路由。
* 每个专家的 FFN 中间维度 1536。
* 无共享专家。

结果：总参数约 235B，每 token 激活约 22B。

### 4.3 这对 runtime 意味着什么

runtime 必须：

* **把所有专家常驻在 HBM 中** —— 两个模型都需要将其全部专家权重常驻在整个集群上。
* **让每个 token 经过门控网络** —— 计算量小，但逐层逐 token 都要做。
* **把 token 搬到选中的专家上** —— 当专家分布在多块 GPU 上时，这就是 all-to-all 通信问题（Lecture 03）。
* **只计算激活专家的 FFN** —— DeepSeek 为 256 选 8（3.1%），Qwen3-MoE 为 128 选 8（6.25%）。

稠密模型的心智模型很简单：「1 块 GPU，全部权重」。MoE 则是「所有专家无处不在，或者把它们分区，并在 runtime 路由 token」。**复杂度从计算密度转移到通信与调度。**

---

## 5. 两个模型的具体数字

### 5.1 DeepSeek V3.1

| 规格 | 值 |
|------|-------|
| 总参数 | 671B |
| 激活参数（每 token） | 37B |
| 层数 | 61（3 稠密 + 58 MoE） |
| 隐藏维度 | 7168 |
| Attention | MLA，128 个 head（Q），`q_lora_rank=1536`，`kv_lora_rank=512` |
| 词表 | 129,280 |
| 上下文 | 128K（通过 YaRN 扩展） |
| 每 token 的 KV 字节数（FP16） | ~69 KB |
| 原生推测解码 | MTP |
| HBM 总量（BF16） | ~1.4 TB |
| HBM 总量（FP8） | ~700 GB |
| HBM 总量（FP4） | ~350 GB |

### 5.2 Qwen3-MoE 235B-A22B

| 规格 | 值 |
|------|-------|
| 总参数 | 235B |
| 激活参数（每 token） | 22B |
| 层数 | 94 |
| 隐藏维度 | 4096 |
| Attention | GQA，64 个 Q head，4 个 KV head（依据 Qwen3 发布） |
| 词表 | 151,936 |
| 上下文 | 256K（Instruct-2507） |
| 每 token 的 KV 字节数（FP16） | 2 × 94 × 4 × 128 × 2 ≈ 192 KB |
| 原生推测解码 | 无（使用 EAGLE-3 / Medusa） |
| HBM 总量（BF16） | ~470 GB |
| HBM 总量（FP8） | ~235 GB |
| HBM 总量（FP4） | ~118 GB |

### 5.3 并排对比

| 属性 | DeepSeek V3.1 | Qwen3-MoE 235B-A22B |
|----------|---------------|----------------------|
| 总参数 / 激活参数 | 671B / 37B | 235B / 22B |
| 激活参数占比 | 5.5% | 9.4% |
| Attention | MLA（压缩 KV） | GQA（4 个 KV head） |
| 每 token 的 KV 字节数 | ~69 KB | ~192 KB |
| 层数 | 61 | 94 |
| 原生推测解码 | MTP（有） | 无（外部 EAGLE-3） |
| 专家 | 256 + 1 共享 | 128 |
| FP4 下的 HBM | ~350 GB | ~118 GB |

DeepSeek 总参数更多但 KV 经过压缩；Qwen 总参数更少但每 token 的 KV 更多。在两者之间做选择取决于：

* 长上下文优先 → DeepSeek 的 MLA 胜出。
* HBM 受限的部署 → Qwen3-MoE 235B-A22B 更容易装下。
* 需要开箱即用的 MTP 推测解码 → DeepSeek。
* 多语言覆盖（尤其是中文） → 两者都有竞争力；Qwen3 在中文 benchmark 上略强。

---

## 6. 推理图相对稠密模型的变化

与 Part 2 的稠密模型相比，推理图新增了：

### 6.1 每层新增的阶段

```text
Dense block FFN:                       MoE block FFN:
  RMSNorm                                RMSNorm
  gate matmul + up matmul                gating linear → top-k
  SwiGLU                                 token → expert assignment
  down matmul                            (if EP) all-to-all dispatch
  residual                               per-expert FFN computation (only selected)
                                         (if EP) all-to-all combine
                                         weighted sum of expert outputs
                                         (+ shared expert path, DeepSeek)
                                         residual
```

新增的阶段：

1. **门控计算** —— 计算量小，但逐 token 逐层进行。
2. **token 路由** —— 把每个 token 分配给对应的专家，构建 dispatch 表。
3. **（仅 EP）** 用 all-to-all 通信把 token 搬到专家处再搬回。
4. **逐专家 FFN** —— FFN 计算与稠密模型相同，但每个专家只处理一部分 token。
5. **输出合并** —— 对各专家输出加权求和。
6. **（MTP）** 多 token 预测头每步产生 k+1 个 logit 预测。

### 6.2 新的瓶颈候选项

* **门控计算** —— 通常很小，但在 Hopper 级硬件上、大 batch 时可能变得可观。
* **all-to-all 通信** —— 最主要的新瓶颈（Lecture 03）。
* **专家负载不均** —— 如果某些专家拿到的 token 远多于其他专家，最慢的专家决定该步耗时。
* **KV cache attention** —— 对 DeepSeek MLA，上投影计算量增加；对 Qwen3-MoE，则是标准 GQA attention 开销。

---


<details>
<summary>English original</summary>

**4.2 Qwen3-MoE 235B-A22B expert configuration**

From `config.json`:

| Field | Value |
|-------|-------|
| `num_experts` | 128 |
| `num_experts_per_tok` | 8 |
| `moe_intermediate_size` | 1536 |
| (no shared experts) | — |

So:

* All layers are MoE (no first-k-dense-replace).
* 128 experts per layer, top-8 routing.
* Per-expert FFN intermediate 1536.
* No shared expert.

The result: ~235B total params, ~22B active per token.

**4.3 What this means for the runtime**

The runtime has to:

* **Hold all experts in HBM** — both models need their full expert weights resident across the cluster.
* **Route every token through the gating network** — small but per-layer per-token.
* **Move tokens to their selected experts** — this is the all-to-all communication problem when experts are partitioned across GPUs (Lecture 03).
* **Compute only the active experts' FFN** — 8 of 256 (3.1%) for DeepSeek, 8 of 128 (6.25%) for Qwen3-MoE.

A dense model has a simple "1 GPU, all the weights" mental model. An MoE has "all experts everywhere or partition them, and route tokens at runtime." **The complexity moves from compute density to communication and scheduling.**

---

**5. The two models in concrete numbers**

**5.1 DeepSeek V3.1**

| Spec | Value |
|------|-------|
| Total params | 671B |
| Active params (per token) | 37B |
| Layers | 61 (3 dense + 58 MoE) |
| Hidden size | 7168 |
| Attention | MLA, 128 heads (Q), `q_lora_rank=1536`, `kv_lora_rank=512` |
| Vocab | 129,280 |
| Context | 128K (extended via YaRN) |
| KV bytes/token (FP16) | ~69 KB |
| Native speculation | MTP |
| Total HBM (BF16) | ~1.4 TB |
| Total HBM (FP8) | ~700 GB |
| Total HBM (FP4) | ~350 GB |

**5.2 Qwen3-MoE 235B-A22B**

| Spec | Value |
|------|-------|
| Total params | 235B |
| Active params (per token) | 22B |
| Layers | 94 |
| Hidden size | 4096 |
| Attention | GQA, 64 Q heads, 4 KV heads (per Qwen3 release) |
| Vocab | 151,936 |
| Context | 256K (Instruct-2507) |
| KV bytes/token (FP16) | 2 × 94 × 4 × 128 × 2 ≈ 192 KB |
| Native speculation | None (use EAGLE-3 / Medusa) |
| Total HBM (BF16) | ~470 GB |
| Total HBM (FP8) | ~235 GB |
| Total HBM (FP4) | ~118 GB |

**5.3 Side by side**

| Property | DeepSeek V3.1 | Qwen3-MoE 235B-A22B |
|----------|---------------|----------------------|
| Total / active params | 671B / 37B | 235B / 22B |
| Active param ratio | 5.5% | 9.4% |
| Attention | MLA (compressed KV) | GQA (4 KV heads) |
| KV bytes/token | ~69 KB | ~192 KB |
| Layers | 61 | 94 |
| Native speculation | MTP (yes) | none (EAGLE-3 external) |
| Experts | 256 + 1 shared | 128 |
| HBM at FP4 | ~350 GB | ~118 GB |

DeepSeek has more total params but compressed KV; Qwen has fewer total but more KV per token. Choosing one over the other depends on:

* Long-context priorities → DeepSeek MLA wins.
* HBM-constrained deployment → Qwen3-MoE 235B-A22B fits more easily.
* Need MTP speculation out of the box → DeepSeek.
* Multilingual coverage (Chinese especially) → both competitive; Qwen3 slightly stronger on Chinese benchmarks.

---

**6. Inference graph changes vs dense**

Compared to Part 2's dense models, the inference graph adds:

**6.1 New stages per layer**

```text
Dense block FFN:                       MoE block FFN:
  RMSNorm                                RMSNorm
  gate matmul + up matmul                gating linear → top-k
  SwiGLU                                 token → expert assignment
  down matmul                            (if EP) all-to-all dispatch
  residual                               per-expert FFN computation (only selected)
                                         (if EP) all-to-all combine
                                         weighted sum of expert outputs
                                         (+ shared expert path, DeepSeek)
                                         residual
```

The extra stages:

1. **Gating computation** — small but per-token per-layer.
2. **Token routing** — assigns each token to its experts, builds the dispatch table.
3. **(EP-only)** All-to-all communication to move tokens to their experts and back.
4. **Per-expert FFN** — same FFN math as dense, but only on a subset of tokens per expert.
5. **Output combination** — weighted sum of expert outputs.
6. **(MTP)** Multi-token head produces k+1 logit predictions per step.

**6.2 The new bottleneck candidates**

* **Gating compute** — usually small but can become measurable on Hopper-class hardware at high batch sizes.
* **All-to-all communication** — the dominant new bottleneck (Lecture 03).
* **Expert load imbalance** — if some experts get many more tokens than others, the slowest expert sets the step time.
* **KV cache attention** — for DeepSeek MLA, the up-projection compute adds; for Qwen3-MoE, the standard GQA attention cost.

---

</details>

## 7. 为什么 MoE（混合专家模型）经济性不同

一个简化的成本模型：

```text
$/MTok ≈ (replica_cost_per_hour × hours_per_MTok)
       = (replica_cost) / (output_tokens/sec × 3600 / 10^6)
```

对于稠密模型（第 2 部分）：模型每个 token 必须执行 P × 2 FLOPs，其中 P 是模型规模，decode（逐 token 生成阶段）时主要从 HBM 读取。

对于 MoE：模型必须在 HBM 中 *持有* P_total，但每个 token 只 *执行* P_active × 2 FLOPs。decode 带宽仍要承担激活参数权重的开销，但 HBM 成本高得多。

### 7.1 带宽计算

在单个 B200 上以 FP4 对 DeepSeek V3.1 进行 batch=1 的 decode（仅作假设，忽略它放不下）：

```text
bytes read per decode step:
  Active weights: 37B × 0.5 bytes (FP4) = 18.5 GB
  KV cache (128K, FP16 MLA): 9 GB
  Total: ~27.5 GB

Decode time on B200 (8 TB/s HBM):
  ≈ 27.5 GB / 8 TB/s ≈ 3.4 ms / token
  ≈ ~300 tokens/sec at batch=1 (theoretical ceiling)
```

在 GB200 NVL72 上采用恰当批处理的实践中：

* MoE 推理服务受益于批处理，因为跨 token 专家布线摊销了 all-to-all 成本。
* 每个 GPU 的带宽上限比稠密更严格，因为持有的总 HBM 很高。
* GB200 NVL72 上 SGLang 的实测：DeepSeek V3.1 FP4 在并发 64 下约 600-1000 tok/s/GPU。

### 7.2 每百万 token 成本（$/MTok）对比

核心数字（2026 年中大致范围；在实验室复现）：

| 模型 | 硬件 | 激活 | 吞吐（tok/s/GPU） | $/MTok |
|-------|----------|--------|------------------------|--------|
| Llama 3.3 70B FP8 | 4× H100 | 70B | ~580 | ~$1.20 |
| Qwen 2.5 72B FP8 | 4× H100 | 72B | ~550 | ~$1.26 |
| Qwen3-MoE 235B-A22B FP4 | 8× B200 | 22B | ~1200 | ~$1.27 |
| DeepSeek V3.1 FP4 + MTP | 16× B200 (NVL72) | 37B | ~1500（MTP 有效） | ~$1.02 |

**在 Blackwell 上，MoE 是成本经济性的赢家**，一旦具备该簇规模——但它 **要求该簇规模**。对于单副本部署，**稠密 Hopper 仍然胜出**。

---

## 实验 — 推导两种配置并生成并排成本表

目标：用支持 MoE 的成本建模扩展 benchmark 仓库。限时一天。

1. 从 Hugging Face 官方仓库**下载两个 `config.json` 文件**。
2. **以编程方式计算**每个模型的：
   * 总参数、每 token 激活参数。
   * 每 token 的 KV cache 字节数（DeepSeek 考虑 MLA，Qwen3-MoE 考虑 GQA）。
   * BF16、FP8、FP4 下的 HBM 占用（权重 + 128K 时的 KV，batch=1）。
   * B200 单 GPU 上的 decode 带宽上限（假设权重放得下，batch=1）。
3. **生成对比表**，包含两种模型、三种精度。
4. **预测**在什么批大小下，decode 工作负载从带宽受限转为算力受限（使用第 1 部分第 03 讲的 ridge-point 数学，B200 上 FP4 上限约 1125 FLOPs/byte）。
5. **选择**每个模型的 HBM 可行部署形态（单 B200、2× B200、4× B200、8× B200 或 NVL72），采用 FP4 + FP8 KV，并发目标为 16、64、256。

通过标准：另一位工程师能根据公开配置复现该报告，并且你的预测与第 02-05 讲中的实测数字一致。

---

## 自测

1. MLA 将 DeepSeek V3.1 的每 token KV 从约 244 KB（假设的 GQA 等效）降至约 69 KB。在 128K 上下文下，一个 B200（192 GB）的 KV cache 预算中能容纳多少并发请求，相比假设的 GQA 又如何？
2. MTP 为 DeepSeek 提供原生推测，达到约 2× 的有效 decode 吞吐。为什么它特别无法在不重新训练的情况下迁移到 Qwen3-MoE？
3. 队友提议用 8× H200 而非 8× B200 为 DeepSeek V3.1 提供推理服务，以节省成本。不实际运行，预测吞吐下降幅度。瓶颈是什么？
4. 为什么 Qwen3-MoE 235B-A22B 有 94 个 layer，而 DeepSeek V3.1 只有 61 个，但 DeepSeek 的激活参数（37B）却 *高于* Qwen3 的（22B）？
5. 对于 batch=8 的长上下文（128K）聊天产品，哪个模型在 FP4 下的总 HBM 占用更大？给出计算过程。

---


<details>
<summary>English original</summary>

**7. Why MoE economics differ**

A simplified cost model:

```text
$/MTok ≈ (replica_cost_per_hour × hours_per_MTok)
       = (replica_cost) / (output_tokens/sec × 3600 / 10^6)
```

For dense (Part 2): the model has to do P × 2 FLOPs per token, where P is the model size, mostly read from HBM at decode time.

For MoE: the model has to *hold* P_total in HBM but only *do* P_active × 2 FLOPs per token. Decode bandwidth still pays for the active params' weights, but the HBM cost is much higher.

**7.1 The bandwidth math**

Decode at batch=1 for DeepSeek V3.1 at FP4 on one B200 (just hypothetically, ignoring it doesn't fit):

```text
bytes read per decode step:
  Active weights: 37B × 0.5 bytes (FP4) = 18.5 GB
  KV cache (128K, FP16 MLA): 9 GB
  Total: ~27.5 GB

Decode time on B200 (8 TB/s HBM):
  ≈ 27.5 GB / 8 TB/s ≈ 3.4 ms / token
  ≈ ~300 tokens/sec at batch=1 (theoretical ceiling)
```

In practice on GB200 NVL72 with proper batching:

* MoE serving wins from batching because the cross-token expert routing amortizes the all-to-all cost.
* Bandwidth ceiling per GPU is more strict than dense because total HBM held is high.
* Real measurements at SGLang on GB200 NVL72: ~600-1000 tok/s/GPU at concurrency 64 for DeepSeek V3.1 FP4.

**7.2 The $/MTok comparison**

The headline numbers (mid-2026 ballpark; replicate in your lab):

| Model | Hardware | Active | Throughput (tok/s/GPU) | $/MTok |
|-------|----------|--------|------------------------|--------|
| Llama 3.3 70B FP8 | 4× H100 | 70B | ~580 | ~$1.20 |
| Qwen 2.5 72B FP8 | 4× H100 | 72B | ~550 | ~$1.26 |
| Qwen3-MoE 235B-A22B FP4 | 8× B200 | 22B | ~1200 | ~$1.27 |
| DeepSeek V3.1 FP4 + MTP | 16× B200 (NVL72) | 37B | ~1500 (effective with MTP) | ~$1.02 |

**MoE on Blackwell is the cost-economics winner** once the cluster scale is available — but it **requires the cluster scale**. For a single-replica deployment, **dense Hopper still wins**.

---

**Lab — derive both configs and produce a side-by-side cost table**

Goal: extend the benchmark repo with MoE-aware cost modeling. Cap of one day.

1. **Download both `config.json` files** from official Hugging Face repos.
2. **Compute, programmatically**, for each model:
   * Total params, active params per token.
   * Per-token KV cache bytes (MLA-aware for DeepSeek, GQA-aware for Qwen3-MoE).
   * HBM footprint at BF16, FP8, FP4 (weights + KV at 128K, batch=1).
   * Decode bandwidth ceiling on B200 single GPU (assuming weights fit, batch=1).
3. **Render a comparison table** with both models, three precisions.
4. **Predict** at what batch size the decode workload moves from bandwidth-bound to compute-bound (using Part 1 Lecture 03 ridge-point math, FP4 ceiling ~1125 FLOPs/byte on B200).
5. **Pick** an HBM-feasible deployment shape for each model (single B200, 2× B200, 4× B200, 8× B200, or NVL72) at FP4 + FP8 KV with concurrency targets of 16, 64, 256.

Pass criterion: the report can be reproduced by another engineer from public configs, and your predictions match measured numbers in Lectures 02-05.

---

**Self-check**

1. MLA reduces DeepSeek V3.1's per-token KV from ~244 KB (hypothetical GQA equivalent) to ~69 KB. At 128K context, how many concurrent requests fit in the KV cache budget of one B200 (192 GB) versus the GQA hypothetical?
2. MTP gives DeepSeek native speculation at ~2× effective decode throughput. Why is it specifically not transferable to Qwen3-MoE without retraining?
3. A teammate proposes serving DeepSeek V3.1 on 8× H200 instead of 8× B200 to save cost. Without running it, predict the throughput drop. What's the bottleneck?
4. Why does Qwen3-MoE 235B-A22B have 94 layers and DeepSeek V3.1 only 61, but DeepSeek's active params (37B) is *higher* than Qwen3's (22B)?
5. For a long-context (128K) chat product at batch=8, which model has the larger total HBM footprint at FP4? Show the math.

---

</details>

## 参考文献

* DeepSeek V3 技术报告 — [arXiv:2412.19437](https://arxiv.org/abs/2412.19437)
* DeepSeek V3.1 发布说明 — [github.com/deepseek-ai/DeepSeek-V3](https://github.com/deepseek-ai/DeepSeek-V3)
* DeepSeek V3.1 模型卡 — [huggingface.co/deepseek-ai/DeepSeek-V3.1](https://huggingface.co/deepseek-ai/DeepSeek-V3.1)
* MLA — 见 DeepSeek V2 论文 [arXiv:2405.04434](https://arxiv.org/abs/2405.04434)
* DeepSeek-MoE 论文 — [arXiv:2401.06066](https://arxiv.org/abs/2401.06066)
* 多 token 预测 — [arXiv:2404.19737](https://arxiv.org/abs/2404.19737)
* Qwen3 技术报告 — [qwenlm.github.io/blog/qwen3/](https://qwenlm.github.io/blog/qwen3/)
* Qwen3-MoE 235B-A22B 模型卡 — [huggingface.co/Qwen/Qwen3-235B-A22B](https://huggingface.co/Qwen/Qwen3-235B-A22B)（模型名与发布时一致）
* “Mixture of Experts Explained” — [huggingface.co/blog/moe](https://huggingface.co/blog/moe) — 易读的入门读物
* Switch Transformers — [arXiv:2101.03961](https://arxiv.org/abs/2101.03961) — MoE 的奠基论文

交叉引用：

* [第 2 部分 → 第 01 讲 — 70B 级稠密模型的解剖](../02-Hopper稠密计算/Lecture-01.md) — 用于稠密与 MoE 的直接对比
* [阶段 5 → GPU Infrastructure → Long-Context-MoE-Foundation-Training → 04 MoE Fundamentals](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/07-长上下文MoE基础训练/04-MoE-Fundamentals.md) — MoE 的训练侧视角

---

## 截至 2026-06

配置固定自官方模型卡：DeepSeek V3.1（2025-08）与 Qwen3-235B-A22B-Instruct-2507（2025-07）。待 DeepSeek V4 或 Qwen4-MoE 发布，或某个架构选择不同的竞争 MoE 系列成为生产标准时，再行更新。

---

## 接下来

* 下一篇：[第 02 讲 — Blackwell 硬件故事](Lecture-02.md)
* 上级：[第 3 部分 — Blackwell 上的 MoE](README.md)


<details>
<summary>English original</summary>

**References**

* DeepSeek V3 technical report — [arXiv:2412.19437](https://arxiv.org/abs/2412.19437)
* DeepSeek V3.1 release notes — [github.com/deepseek-ai/DeepSeek-V3](https://github.com/deepseek-ai/DeepSeek-V3)
* DeepSeek V3.1 model card — [huggingface.co/deepseek-ai/DeepSeek-V3.1](https://huggingface.co/deepseek-ai/DeepSeek-V3.1)
* MLA — described in DeepSeek V2 paper [arXiv:2405.04434](https://arxiv.org/abs/2405.04434)
* DeepSeek-MoE paper — [arXiv:2401.06066](https://arxiv.org/abs/2401.06066)
* Multi-token prediction — [arXiv:2404.19737](https://arxiv.org/abs/2404.19737)
* Qwen3 technical report — [qwenlm.github.io/blog/qwen3/](https://qwenlm.github.io/blog/qwen3/)
* Qwen3-MoE 235B-A22B model card — [huggingface.co/Qwen/Qwen3-235B-A22B](https://huggingface.co/Qwen/Qwen3-235B-A22B) (model name as released)
* "Mixture of Experts Explained" — [huggingface.co/blog/moe](https://huggingface.co/blog/moe) — accessible primer
* Switch Transformers — [arXiv:2101.03961](https://arxiv.org/abs/2101.03961) — the foundational MoE paper

Cross-references:

* [Part 2 → Lecture 01 — Anatomy of a 70B-class dense model](../02-Hopper稠密计算/Lecture-01.md) — for direct dense-vs-MoE comparison
* [Phase 5 → GPU Infrastructure → Long-Context-MoE-Foundation-Training → 04 MoE Fundamentals](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/07-长上下文MoE基础训练/04-MoE-Fundamentals.md) — training-side perspective on MoE

---

**Current as of 2026-06**

Configs pinned from the official model cards: DeepSeek V3.1 (2025-08) and Qwen3-235B-A22B-Instruct-2507 (2025-07). Refresh when DeepSeek V4 or Qwen4-MoE ships, or when a competing MoE family with different architectural choices becomes the production standard.

---

**Next**

* Next: [Lecture 02 — Blackwell hardware story](Lecture-02.md)
* Up: [Part 3 — MoE at Blackwell](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 3 - MoE at Blackwell/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%203%20-%20MoE%20at%20Blackwell/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
