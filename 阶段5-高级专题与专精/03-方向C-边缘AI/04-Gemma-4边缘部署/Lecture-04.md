---
title: 第 04 讲 — 面向 Gemma 4 的投机解码：draft-verify、self-draft 与边缘接受长度问题
description: 第 04 讲 — 面向 Gemma 4 的投机解码：draft-verify、self-draft 与边缘接受长度问题
---

# 第 04 讲 — 面向 Gemma 4 的投机解码：draft-verify、self-draft 与边缘接受长度问题

**合集：** [Gemma 4 边缘部署](README.md) | **上一讲：** [← 第 03 讲](Lecture-03.md) | **下一讲：** [第 05 讲 →](Lecture-05.md)

---

投机解码是边缘 LLM 部署中 ROI 最高的单一算法。在 Orin 上批大小=1 时，仅凭带宽，Gemma 4 4B 就能跑到约 50–65 tok/s——但 decode（逐 token 生成阶段）是带宽受限的，而每个带宽受限的步骤只产生一个 token。投机解码打破了这一点：以低成本 draft K 个 token，在一次 pass 中全部验证，接受最长匹配前缀。如果接受长度 τ 平均为 3，那么每次受带宽限制的内存 pass 可多获得 3× 的 token——免费，且输出无损。

本讲结合 Gemma 4 的特定架构解释投机解码机制，构建 1B→4B draft-verify 对，介绍面向 Gemma 的 EAGLE-3 风格 self-draft head，并推导你用来预测 spec-decode 是否会在你的 Jetson 目标平台上带来收益所需的接受长度算式。

---

## 学习目标

1. 解释**投机解码接受规则**，以及为何它能为任意 draft 模型保证无损输出。
2. 在给定 τ（接受长度）与 draft/target 成本比 c 时，计算投机解码的**理论加速比**。
3. 为 Gemma 4 4B/12B target 配置 **Gemma 4 1B 作为 draft 模型**，并理解同一系列为何适合 self-speculation。
4. 描述 **EAGLE-3 风格的 self-draft head**，并说明为何 Gemma 4 的架构使其尤为有效。
5. 在 Jetson 上使用 llama.cpp speculative mode 运行投机解码，测量 τ，并验证输出精度一致性。

---

## 1. 为何投机解码在边缘更重要

回顾第 01 讲中的带宽上限：

```text
decode ceiling = BW / weight_bytes
  Orin (204 GB/s):  Gemma 4 4B INT4 → 204 / 2.2 ≈ 93 tok/s  (ceiling)
  Actual (0.65×):                                    ≈ 60 tok/s (realistic)

With speculative decode (τ = 3, same bandwidth pass):
  tokens per memory pass = τ = 3
  effective tok/s ceiling = 3 × 93 = 279 tok/s  (same BW, 3× tokens)
  Actual (0.65×):                = 3 × 60 ≈ 180 tok/s
```

算式是精确的：投机解码在每周期消耗**相同内存带宽**的同时，将有效吞吐乘以 τ。每次 HBM pass 的 token 数增加；每秒字节速率保持不变。这是对带宽上限的直接倍增，而不是绕开它的效率改进。

**为何它在边缘比在数据中心影响更大：**

在数据中心（H100，3.35 TB/s，批大小 > 32），对于大批大小，decode 常常接近算力受限——投机解码的收益更小。在 Jetson（204–273 GB/s，批大小=1–4）上，decode 在每个批大小下都强烈带宽受限——投机解码的收益最大，因为每个 token 都在支付完整的 HBM 访问成本，没有任何可分摊的对象。

---


<details>
<summary>English original</summary>

**Lecture 04 — Speculative Decoding with Gemma 4: Draft-Verify, Self-Draft, and the Edge Acceptance-Length Problem**

**Collection:** [Gemma 4 Edge Deployment](README.md) | **Previous:** [← Lecture 03](Lecture-03.md) | **Next:** [Lecture 05 →](Lecture-05.md)

---

Speculative decoding is the highest-ROI single algorithm for edge LLM deployment. At batch=1 on an Orin, Gemma 4 4B runs at ~50–65 tok/s from bandwidth alone — but decode is memory-bound, and every bandwidth-bound step emits just one token. Speculative decoding breaks this: draft K tokens cheaply, verify them all in one pass, accept the longest matching prefix. If the acceptance length τ averages 3, you get 3× more tokens per bandwidth-limited memory pass — for free, with lossless output.

This lecture explains the speculative-decoding mechanism in the context of Gemma 4's specific architecture, builds the 1B→4B draft-verify pair, covers EAGLE-3-style self-draft heads for Gemma, and derives the acceptance-length arithmetic you need to predict whether spec-decode will help on your Jetson target.

---

**Learning objectives**

1. Explain the **speculative decoding acceptance rule** and why it guarantees lossless output for any draft model.
2. Compute the **theoretical speedup** from speculative decode given τ (acceptance length) and the draft/target cost ratio c.
3. Configure **Gemma 4 1B as a draft model** for Gemma 4 4B/12B targets and understand why the same family is ideal for self-speculation.
4. Describe **EAGLE-3-style self-draft heads** and why Gemma 4's architecture makes them especially effective.
5. Run speculative decoding on Jetson using llama.cpp speculative mode, measure τ, and verify output parity.

---

**1. Why speculative decoding matters more at the edge**

Recall from Lecture 01 the bandwidth ceiling:

```text
decode ceiling = BW / weight_bytes
  Orin (204 GB/s):  Gemma 4 4B INT4 → 204 / 2.2 ≈ 93 tok/s  (ceiling)
  Actual (0.65×):                                    ≈ 60 tok/s (realistic)

With speculative decode (τ = 3, same bandwidth pass):
  tokens per memory pass = τ = 3
  effective tok/s ceiling = 3 × 93 = 279 tok/s  (same BW, 3× tokens)
  Actual (0.65×):                = 3 × 60 ≈ 180 tok/s
```

The math is exact: speculative decode multiplies effective throughput by τ while consuming **the same memory bandwidth per cycle**. The token count per HBM pass goes up; the bytes-per-second rate stays fixed. This is a direct multiplier on the bandwidth ceiling, not an efficiency improvement around it.

**Why it's more impactful at the edge than in the datacenter:**

In a datacenter (H100, 3.35 TB/s, batch > 32), decode is often near-compute-bound for large batches — the gain from speculative decode is smaller. On Jetson (204–273 GB/s, batch=1–4), decode is aggressively memory-bound at every batch size — the gain from speculative decode is maximal because every token is paying the full HBM access cost with nothing to amortize over.

---

</details>

## 2. 接受规则——为什么它是无损的

投机解码机制（Leviathan et al., 2023；Chen et al., 2023）：

```text
SETUP:
  - Draft model q (small, cheap) — e.g., Gemma 4 1B
  - Target model p (large, accurate) — e.g., Gemma 4 4B

ALGORITHM (for each decode step):
  1. Draft: use q to autoregressively propose tokens x₁, x₂, ..., xₖ
  2. Verify: run p ONCE on the context + all K proposed tokens (ONE forward pass)
             p produces probability distributions P(xᵢ | context, x₁...xᵢ₋₁) for each
  3. Accept/reject (per-token, left to right):
     for i = 1..K:
       accept xᵢ with probability min(1, p(xᵢ) / q(xᵢ))
       on first rejection: resample from normalize(max(0, p - q)) and stop
  4. Accepted tokens (plus the resampled token) are output

GUARANTEE:
  The output distribution is EXACTLY p — identical to running p autoregressively.
  Proof sketch: accepted tokens pass through unmodified (probability min(1, p/q) ×
  appearing with q probability = p probability). Rejected tokens resample from the
  gap (p - q). Expected distribution = p exactly.

WHY THIS ENABLES SPEEDUP:
  Step 2 (one p-forward-pass over K+1 positions) costs ≈ the same as one standard
  decode step (one p-forward-pass over 1 position) because both are memory-bandwidth
  bound: you stream the same weights, you get K+1 logits instead of 1.
  Cost per bandwidth pass: ~same. Tokens produced: τ on average (τ = average accepted).
  Speedup: approximately τ / (1 + K × c)  where c = draft cost / target cost.
```

**加速比公式：**

```text
speedup ≈ τ / (1 + K × c)

where:
  τ = average number of tokens accepted per speculative step (acceptance length)
  K = number of tokens drafted per step
  c = cost of drafting one token / cost of one target decode step

For Gemma 4 1B → 4B on Orin (INT4):
  Draft cost (1B, INT4): 204 GB/s ÷ 0.5 GB weights ≈ 408 tok/s → 1/408 s/tok
  Target cost (4B, INT4): 204 GB/s ÷ 2.2 GB weights ≈ 93 tok/s  → 1/93 s/tok
  c = (1/93) / (1/408) × (0.5/2.2) ≈ 0.5/2.2 ≈ 0.23
       (draft costs 23% of target per token)

speedup at τ=3, K=5, c=0.23:
  speedup = 3 / (1 + 5 × 0.23) = 3 / 2.15 ≈ 1.4×

speedup at τ=4, K=6, c=0.23:
  speedup = 4 / (1 + 6 × 0.23) = 4 / 2.38 ≈ 1.7×

speedup at τ=2 (bad draft), K=5, c=0.23:
  speedup = 2 / (1 + 5 × 0.23) = 2 / 2.15 ≈ 0.93×   → SLOWER! don't use.
```

**关键洞察：** 投机 decode 仅在 τ > (1 + K×c) 时才有帮助。如果 draft 质量差（τ 低），增加 draft 开销反而会损失吞吐。在采用投机 decode 之前先测量 τ。

---

## 3. Gemma 4 1B 作为 4B/12B 的 draft——为什么这是理想组合

### 3.1 同系列优势

用 Gemma 4 1B 作为 Gemma 4 4B/12B 的 draft 是天然的搭配，因为：

```text
TOKENIZER PARITY:
  Both use the same 256K SentencePiece tokenizer.
  Draft tokens are valid target tokens — no conversion, no vocabulary mapping.
  (Cross-family speculation requires vocabulary alignment shims, losing accuracy)

ARCHITECTURE PARITY:
  Same positional encoding (RoPE), same context window, same interleaved attention.
  The 1B draft's hidden states align with the 4B target's early layers —
  useful for feature-level speculative decoding (see §4).

DISTRIBUTION SIMILARITY:
  Both are Gemma 4 IT (instruction-tuned). The 1B model learned from the same
  instruction distribution as 4B — its token proposals are a compressed version
  of 4B's outputs, not a different distribution.
```

### 3.2 1B + 4B 在 Orin 上共存的内存布局

```text
Gemma 4 1B INT4:   0.5 GB weights + ~0.05 GB KV = 0.55 GB
Gemma 4 4B INT4:   2.2 GB weights + ~0.2 GB KV  = 2.4 GB
Total co-resident: ~3.0 GB  (easily fits in Orin's 64 GB)

vs Gemma 4 12B INT4 alone: 6.4 GB (no spec decode needed if it fits)
```

两个模型可以同时驻留在 Jetson Orin 的统一内存中，没有内存压力。1B 模型使 4B 部署的内存占用增加约 23%。


<details>
<summary>English original</summary>

**2. The acceptance rule — why it's lossless**

The speculative decoding mechanism (Leviathan et al., 2023; Chen et al., 2023):

```text
SETUP:
  - Draft model q (small, cheap) — e.g., Gemma 4 1B
  - Target model p (large, accurate) — e.g., Gemma 4 4B

ALGORITHM (for each decode step):
  1. Draft: use q to autoregressively propose tokens x₁, x₂, ..., xₖ
  2. Verify: run p ONCE on the context + all K proposed tokens (ONE forward pass)
             p produces probability distributions P(xᵢ | context, x₁...xᵢ₋₁) for each
  3. Accept/reject (per-token, left to right):
     for i = 1..K:
       accept xᵢ with probability min(1, p(xᵢ) / q(xᵢ))
       on first rejection: resample from normalize(max(0, p - q)) and stop
  4. Accepted tokens (plus the resampled token) are output

GUARANTEE:
  The output distribution is EXACTLY p — identical to running p autoregressively.
  Proof sketch: accepted tokens pass through unmodified (probability min(1, p/q) ×
  appearing with q probability = p probability). Rejected tokens resample from the
  gap (p - q). Expected distribution = p exactly.

WHY THIS ENABLES SPEEDUP:
  Step 2 (one p-forward-pass over K+1 positions) costs ≈ the same as one standard
  decode step (one p-forward-pass over 1 position) because both are memory-bandwidth
  bound: you stream the same weights, you get K+1 logits instead of 1.
  Cost per bandwidth pass: ~same. Tokens produced: τ on average (τ = average accepted).
  Speedup: approximately τ / (1 + K × c)  where c = draft cost / target cost.
```

**The speedup formula:**

```text
speedup ≈ τ / (1 + K × c)

where:
  τ = average number of tokens accepted per speculative step (acceptance length)
  K = number of tokens drafted per step
  c = cost of drafting one token / cost of one target decode step

For Gemma 4 1B → 4B on Orin (INT4):
  Draft cost (1B, INT4): 204 GB/s ÷ 0.5 GB weights ≈ 408 tok/s → 1/408 s/tok
  Target cost (4B, INT4): 204 GB/s ÷ 2.2 GB weights ≈ 93 tok/s  → 1/93 s/tok
  c = (1/93) / (1/408) × (0.5/2.2) ≈ 0.5/2.2 ≈ 0.23
       (draft costs 23% of target per token)

speedup at τ=3, K=5, c=0.23:
  speedup = 3 / (1 + 5 × 0.23) = 3 / 2.15 ≈ 1.4×

speedup at τ=4, K=6, c=0.23:
  speedup = 4 / (1 + 6 × 0.23) = 4 / 2.38 ≈ 1.7×

speedup at τ=2 (bad draft), K=5, c=0.23:
  speedup = 2 / (1 + 5 × 0.23) = 2 / 2.15 ≈ 0.93×   → SLOWER! don't use.
```

**The critical insight:** speculative decode helps only when τ > (1 + K×c). If the draft quality is poor (low τ), you can actually lose throughput by adding draft overhead. Measure τ before committing to speculative decode.

---

**3. Gemma 4 1B as draft for 4B/12B — why it's the ideal pair**

**3.1 Same-family advantages**

Using Gemma 4 1B as draft for Gemma 4 4B/12B is the natural pairing because:

```text
TOKENIZER PARITY:
  Both use the same 256K SentencePiece tokenizer.
  Draft tokens are valid target tokens — no conversion, no vocabulary mapping.
  (Cross-family speculation requires vocabulary alignment shims, losing accuracy)

ARCHITECTURE PARITY:
  Same positional encoding (RoPE), same context window, same interleaved attention.
  The 1B draft's hidden states align with the 4B target's early layers —
  useful for feature-level speculative decoding (see §4).

DISTRIBUTION SIMILARITY:
  Both are Gemma 4 IT (instruction-tuned). The 1B model learned from the same
  instruction distribution as 4B — its token proposals are a compressed version
  of 4B's outputs, not a different distribution.
```

**3.2 Memory layout for 1B + 4B co-residence on Orin**

```text
Gemma 4 1B INT4:   0.5 GB weights + ~0.05 GB KV = 0.55 GB
Gemma 4 4B INT4:   2.2 GB weights + ~0.2 GB KV  = 2.4 GB
Total co-resident: ~3.0 GB  (easily fits in Orin's 64 GB)

vs Gemma 4 12B INT4 alone: 6.4 GB (no spec decode needed if it fits)
```

Both models can live in Jetson Orin's unified memory simultaneously with no memory pressure. The 1B model adds ~23% to the memory footprint of the 4B deployment.

</details>

### 3.3 在 llama.cpp 中配置 draft-target 推测

```bash
# llama.cpp speculative decoding (--model-draft):
./build/bin/llama-cli \
    --model              gemma4-4b-Q4_K_M.gguf \    # TARGET: 4B
    --model-draft        gemma4-1b-Q4_K_M.gguf \    # DRAFT: 1B
    --n-gpu-layers       9999 \                      # all layers to GPU (target)
    --n-gpu-layers-draft 9999 \                      # all layers to GPU (draft)
    --draft-max          8 \                         # K=8 draft tokens per step
    --draft-min          1 \                         # minimum draft tokens
    --draft-p-min        0.4 \                       # prune low-prob drafts early
    --ctx-size           4096 \
    -p "Describe how a robot picks up an object."

# Benchmarking spec decode throughput:
./build/bin/llama-bench \
    --model              gemma4-4b-Q4_K_M.gguf \
    --model-draft        gemma4-1b-Q4_K_M.gguf \
    --n-gpu-layers       9999 \
    --n-gpu-layers-draft 9999 \
    --draft-max          8 \
    -n 200 -p 128

# Expected output on Orin:
#   Without spec decode: pp = 180 tok/s, tg = 55 tok/s
#   With spec decode:    pp = 175 tok/s, tg = 80–90 tok/s (τ ≈ 2.5–3)
#   Speedup: ~1.5–1.6×
```

### 3.4 接受长度测量

```bash
# llama.cpp outputs draft acceptance statistics:
# Look for lines like:
# draft acceptance: 67.3% (τ ≈ 2.7 average)

# To measure τ explicitly:
./build/bin/llama-cli \
    --model gemma4-4b-Q4_K_M.gguf \
    --model-draft gemma4-1b-Q4_K_M.gguf \
    --n-gpu-layers 9999 --n-gpu-layers-draft 9999 \
    --draft-max 8 \
    -p "$(python3 -c "print('Write a long technical explanation. ' * 5)")" \
    -n 1000 2>&1 | grep -E "accepted|draft"
```

**Gemma 4 1B→4B 的典型接受长度：**

| 任务类型 | 接受长度 τ | 加速比（K=6，c=0.23） |
|-----------|-------------------|----------------------|
| 代码生成 | ~3.5–4.5 | 1.6–1.9× |
| 技术问答 | ~3.0–4.0 | 1.4–1.7× |
| 创意写作 | ~2.5–3.5 | 1.2–1.5× |
| 数学推理 | ~2.0–3.0 | 0.9–1.4× |
| 重复性输出 | ~4.5–6.0 | 1.8–2.3× |

代码与技术文本是接受长度最高的任务，因为其 token 分布窄且可预测（关键词、语法 token 会重复出现）。创意文本与推理文本的分布更宽，因此 τ 更低。

---

## 4. 面向 Gemma 4 的 EAGLE-3 风格自草稿头

### 4.1 从外部草稿到自草稿

外部草稿（1B 模型）需要在内存中额外放置一个模型，并引入模型间同步开销。**自草稿**方法在目标模型上直接添加轻量级草稿头，从而省去单独的模型：

```text
External draft (1B→4B):                 Self-draft (EAGLE-3-style):
  1B forward pass → K token proposals     Target model runs with attached draft heads
  4B forward pass → verify K tokens       Draft heads predict next tokens from existing
  Separate memory: 0.5 GB extra           hidden states — no separate model needed
  Synchronization overhead: yes           Overhead: ~5–10% of target's weight size
```


<details>
<summary>English original</summary>

**3.3 Configuring draft-target speculation in llama.cpp**

```bash
# llama.cpp speculative decoding (--model-draft):
./build/bin/llama-cli \
    --model              gemma4-4b-Q4_K_M.gguf \    # TARGET: 4B
    --model-draft        gemma4-1b-Q4_K_M.gguf \    # DRAFT: 1B
    --n-gpu-layers       9999 \                      # all layers to GPU (target)
    --n-gpu-layers-draft 9999 \                      # all layers to GPU (draft)
    --draft-max          8 \                         # K=8 draft tokens per step
    --draft-min          1 \                         # minimum draft tokens
    --draft-p-min        0.4 \                       # prune low-prob drafts early
    --ctx-size           4096 \
    -p "Describe how a robot picks up an object."

# Benchmarking spec decode throughput:
./build/bin/llama-bench \
    --model              gemma4-4b-Q4_K_M.gguf \
    --model-draft        gemma4-1b-Q4_K_M.gguf \
    --n-gpu-layers       9999 \
    --n-gpu-layers-draft 9999 \
    --draft-max          8 \
    -n 200 -p 128

# Expected output on Orin:
#   Without spec decode: pp = 180 tok/s, tg = 55 tok/s
#   With spec decode:    pp = 175 tok/s, tg = 80–90 tok/s (τ ≈ 2.5–3)
#   Speedup: ~1.5–1.6×
```

**3.4 Acceptance length measurement**

```bash
# llama.cpp outputs draft acceptance statistics:
# Look for lines like:
# draft acceptance: 67.3% (τ ≈ 2.7 average)

# To measure τ explicitly:
./build/bin/llama-cli \
    --model gemma4-4b-Q4_K_M.gguf \
    --model-draft gemma4-1b-Q4_K_M.gguf \
    --n-gpu-layers 9999 --n-gpu-layers-draft 9999 \
    --draft-max 8 \
    -p "$(python3 -c "print('Write a long technical explanation. ' * 5)")" \
    -n 1000 2>&1 | grep -E "accepted|draft"
```

**Typical acceptance lengths for Gemma 4 1B→4B:**

| Task type | Acceptance length τ | Speedup (K=6, c=0.23) |
|-----------|-------------------|----------------------|
| Code generation | ~3.5–4.5 | 1.6–1.9× |
| Technical Q&A | ~3.0–4.0 | 1.4–1.7× |
| Creative writing | ~2.5–3.5 | 1.2–1.5× |
| Math reasoning | ~2.0–3.0 | 0.9–1.4× |
| Repetitive outputs | ~4.5–6.0 | 1.8–2.3× |

Code and technical text are the highest-acceptance tasks because the token distribution is narrow and predictable (keywords, syntax tokens repeat). Creative and reasoning text has wider distributions, reducing τ.

---

**4. EAGLE-3-style self-draft heads for Gemma 4**

**4.1 From external draft to self-draft**

External draft (1B model) requires a separate model in memory and introduces inter-model synchronization overhead. **Self-draft** methods add lightweight draft heads directly to the target model, eliminating the separate model:

```text
External draft (1B→4B):                 Self-draft (EAGLE-3-style):
  1B forward pass → K token proposals     Target model runs with attached draft heads
  4B forward pass → verify K tokens       Draft heads predict next tokens from existing
  Separate memory: 0.5 GB extra           hidden states — no separate model needed
  Synchronization overhead: yes           Overhead: ~5–10% of target's weight size
```

</details>

### 4.2 EAGLE-3 机制

EAGLE-3（Li et al., 2025）是最先进的自推测（self-draft）方法。核心思路：不再预测特征（EAGLE-1/2 的做法），EAGLE-3 直接用目标模型多个 layer 的特征预测 **token logits**：

```python
# Conceptual EAGLE-3 for Gemma 4:
class EAGLE3DraftHead(nn.Module):
    def __init__(self, config):
        super().__init__()
        # Fuse early + late features from Gemma 4 transformer:
        # Uses hidden states from layer L/4 and L/2 as early signals
        # plus the full hidden state from the last layer
        self.feature_fuser = nn.Linear(
            config.hidden_size * 3,  # 3 feature vectors concatenated
            config.hidden_size
        )
        self.small_transformer = GemmaDecoderLayer(config, reduced_heads=True)
        # Directly reuse Gemma 4's lm_head (tied embeddings):
        # output logits without adding a new projection

    def forward(self, hidden_early, hidden_mid, hidden_final, kv_cache=None):
        # Fuse multi-layer features:
        fused = self.feature_fuser(
            torch.cat([hidden_early, hidden_mid, hidden_final], dim=-1)
        )
        # One lightweight transformer layer for autoregressive draft:
        draft_hidden = self.small_transformer(fused, kv_cache=kv_cache)
        # Reuse target's lm_head to get logits:
        draft_logits = lm_head(draft_hidden)
        return draft_logits
```

**为什么 Gemma 4 特别适合 EAGLE-3：**

1. **QK-norm 稳定中间隐藏状态**：保护 attention 分数的 QK 归一化也意味着 layer 之间的隐藏状态更稳定、更可预测——对 draft head 来说是更好的特征。
2. **交错 attention 形成天然的检查点**：global-attention layer 之后的隐藏状态，比 local-attention layer 之后的语义更丰富。EAGLE-3 可以瞄准这些 global-attention 检查点来获取最佳的 draft 特征。
3. **hidden_dim × head_dim 保持一致**：Gemma 4 在所有尺寸下 head_dim=256，意味着为 4B 训练的 draft head 只需极小改动即可重新加权（知识蒸馏）用于 12B。

### 4.3 为 Gemma 4 训练 EAGLE-3 head

训练 EAGLE-3 head 需要：
- 一个 Gemma 4 4B 目标模型（冻结）
- 100K–1M token 的训练数据（与部署同分布）
- 单张 A100 上 2–4 小时（4B；12B 按比例线性放大）

```python
# Train EAGLE-3 draft heads (simplified training loop):
import torch
from transformers import AutoModelForCausalLM

target = AutoModelForCausalLM.from_pretrained("google/gemma-4-4b-it")
target.eval()  # frozen

draft_head = EAGLE3DraftHead(target.config)

optimizer = torch.optim.AdamW(draft_head.parameters(), lr=1e-4)

for batch in training_data:
    input_ids = batch["input_ids"].cuda()

    with torch.no_grad():
        # Run target once, collect hidden states at 3 layers:
        outputs = target(input_ids, output_hidden_states=True)
        hidden_early = outputs.hidden_states[len(outputs.hidden_states) // 4]
        hidden_mid   = outputs.hidden_states[len(outputs.hidden_states) // 2]
        hidden_final = outputs.hidden_states[-1]
        target_logits = outputs.logits

    # Draft head predicts next tokens from multi-layer features:
    draft_logits = draft_head(hidden_early, hidden_mid, hidden_final)

    # Training objective: match target's token distribution
    # Cross-entropy loss on next-token prediction:
    loss = F.cross_entropy(
        draft_logits[:, :-1].reshape(-1, target.config.vocab_size),
        input_ids[:, 1:].reshape(-1)
    )

    optimizer.zero_grad()
    loss.backward()
    optimizer.step()
```

**Gemma 4 4B EAGLE-3 head 的预期结果：**

在指令遵循数据上训练后：
- 接受长度 τ ≈ 3.5–5.0（1B 外部 draft 为 2.5–3.5）
- 内存开销：约 150 MB（1B 模型为 500 MB）
- Orin 上的加速：1.7–2.3×（1B draft 为 1.4–1.7×）

只要愿意投入训练这些 head，自推测方法在内存效率和接受长度上都胜出。

---


<details>
<summary>English original</summary>

**4.2 EAGLE-3 mechanism**

EAGLE-3 (Li et al., 2025) is the state-of-the-art self-draft method. Key idea: instead of predicting features (EAGLE-1/2's approach), EAGLE-3 directly predicts **token logits** using features from multiple layers of the target:

```python
# Conceptual EAGLE-3 for Gemma 4:
class EAGLE3DraftHead(nn.Module):
    def __init__(self, config):
        super().__init__()
        # Fuse early + late features from Gemma 4 transformer:
        # Uses hidden states from layer L/4 and L/2 as early signals
        # plus the full hidden state from the last layer
        self.feature_fuser = nn.Linear(
            config.hidden_size * 3,  # 3 feature vectors concatenated
            config.hidden_size
        )
        self.small_transformer = GemmaDecoderLayer(config, reduced_heads=True)
        # Directly reuse Gemma 4's lm_head (tied embeddings):
        # output logits without adding a new projection

    def forward(self, hidden_early, hidden_mid, hidden_final, kv_cache=None):
        # Fuse multi-layer features:
        fused = self.feature_fuser(
            torch.cat([hidden_early, hidden_mid, hidden_final], dim=-1)
        )
        # One lightweight transformer layer for autoregressive draft:
        draft_hidden = self.small_transformer(fused, kv_cache=kv_cache)
        # Reuse target's lm_head to get logits:
        draft_logits = lm_head(draft_hidden)
        return draft_logits
```

**Why Gemma 4 is especially suited to EAGLE-3:**

1. **QK-norm stabilizes intermediate hidden states**: the QK normalization that protects attention scores also means the hidden states between layers are more stable and predictable — better features for the draft head.
2. **Interleaved attention creates natural checkpoints**: the hidden state after a global-attention layer is a semantically richer feature than after a local-attention layer. EAGLE-3 can target these global-attention checkpoints for the best draft features.
3. **Consistent hidden_dim × head_dim**: Gemma 4's head_dim=256 across all sizes means a draft head trained for 4B can be re-weighted (knowledge distillation) for 12B with minimal architecture changes.

**4.3 Training EAGLE-3 heads for Gemma 4**

Training EAGLE-3 heads requires:
- A Gemma 4 4B target (frozen)
- 100K–1M tokens of training data (same distribution as deployment)
- 2–4 hours on one A100 (for 4B; scale linearly for 12B)

```python
# Train EAGLE-3 draft heads (simplified training loop):
import torch
from transformers import AutoModelForCausalLM

target = AutoModelForCausalLM.from_pretrained("google/gemma-4-4b-it")
target.eval()  # frozen

draft_head = EAGLE3DraftHead(target.config)

optimizer = torch.optim.AdamW(draft_head.parameters(), lr=1e-4)

for batch in training_data:
    input_ids = batch["input_ids"].cuda()

    with torch.no_grad():
        # Run target once, collect hidden states at 3 layers:
        outputs = target(input_ids, output_hidden_states=True)
        hidden_early = outputs.hidden_states[len(outputs.hidden_states) // 4]
        hidden_mid   = outputs.hidden_states[len(outputs.hidden_states) // 2]
        hidden_final = outputs.hidden_states[-1]
        target_logits = outputs.logits

    # Draft head predicts next tokens from multi-layer features:
    draft_logits = draft_head(hidden_early, hidden_mid, hidden_final)

    # Training objective: match target's token distribution
    # Cross-entropy loss on next-token prediction:
    loss = F.cross_entropy(
        draft_logits[:, :-1].reshape(-1, target.config.vocab_size),
        input_ids[:, 1:].reshape(-1)
    )

    optimizer.zero_grad()
    loss.backward()
    optimizer.step()
```

**Expected results for Gemma 4 4B EAGLE-3 heads:**

After training on instruction-following data:
- Acceptance length τ ≈ 3.5–5.0 (vs 2.5–3.5 for 1B external draft)
- Memory overhead: ~150 MB (vs 500 MB for 1B model)
- Speedup on Orin: 1.7–2.3× (vs 1.4–1.7× for 1B draft)

The self-draft approach wins on both memory efficiency and acceptance length when you invest in training the heads.

---

</details>

## 5. Lookahead decoding — 面向 Gemma 4 的无 draft 推测

**Lookahead decoding**（Fu et al., 2024）无需任何辅助模型即可生成 draft token，方法是针对 n-gram 序列做 Jacobi 迭代：

```text
LOOKAHEAD IDEA:
  Standard decode: generate one token at a time (sequential)
  Lookahead: maintain a "lookahead cache" of W candidate tokens,
             verify them in parallel with the target model
             — no draft model needed, no extra weights

MECHANISM (W=4 lookahead, N=5-gram):
  Step 1: propose W tokens simultaneously (random or from n-gram cache)
  Step 2: verify with target in one pass (like speculative decode)
  Step 3: accept longest verified n-gram, extend context, refill lookahead

WHY IT HELPS ON GEMMA 4:
  - No external model memory (0 overhead for draft)
  - Gemma 4's 128K context + sliding-window local attention makes the
    n-gram cache viable: long histories improve n-gram match rates
  - Works well for Gemma 4's verbose, consistent technical outputs
```

```python
# Lookahead decoding via llama.cpp (requires --lookup-cache-static flag):
./build/bin/llama-cli \
    --model gemma4-4b-Q4_K_M.gguf \
    --n-gpu-layers 9999 \
    --lookup-cache-static lookup_cache.bin \  # pre-computed n-gram cache
    --ctx-size 32768 \
    -p "Explain the Gemma 4 attention mechanism in detail."
```

**Orin 上 Gemma 4 的 Lookahead 与 draft model 对比：**

| 方法 | 额外内存 | τ（典型） | 最佳任务 |
|--------|-------------|------------|----------|
| 无投机 decode | 0 | 1.0 | — |
| Gemma 4 1B draft | 500 MB | 2.5–4.0 | 对话、代码 |
| EAGLE-3 heads | 150 MB | 3.5–5.0 | 对话、代码、推理 |
| Lookahead (W=4) | ~50 MB 缓存 | 1.5–2.5 | 重复性输出 |
| Lookahead + 1B | 550 MB | 3.0–4.5 | 两者兼优 |

**建议：** 在 Orin 上以 Gemma 4 4B 做生产部署时：
- **部署最快**：Gemma 4 1B 外部 draft（无需任何训练）
- **质量/内存比最优**：EAGLE-3 heads（需 2–4 小时训练）
- **零成本基线**：lookahead decoding（无需额外权重，对重复性输出有帮助）

---

## 6. 输出精度一致性验证

投机解码在理论上无损。实践中，浮点运算的非结合性会引入微小数值差异，并逐级放大。务必验证：

```python
import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

model_id = "google/gemma-4-4b-it"
tokenizer = AutoModelForCausalLM.from_pretrained(model_id)
model_base = AutoModelForCausalLM.from_pretrained(model_id)

# 1. Standard greedy decode (reference):
reference_outputs = model_base.generate(
    inputs, max_new_tokens=200, do_sample=False, temperature=1.0
)

# 2. With speculative decode (implementation-specific):
spec_outputs = run_speculative_decode(...)

# 3. Verify token-level parity:
assert torch.all(reference_outputs == spec_outputs), \
    f"Parity failure at token {(reference_outputs != spec_outputs).nonzero()[0]}"
```

**配合投机解码时，贪心 decode 可证明是确定性的** —— 接受规则保证对贪心（argmax）采样完全等价。当 temperature > 0（随机采样）时，输出分布相同，但单次样本不同（这是预期且正确的）。

---

## 关键要点

- **投机 decode（逐 token 生成阶段）加速比 = τ / (1 + K×c)**，其中 τ 是平均接受 token 数，K 是 draft 数量，c 是 draft/target 成本比。对 Orin 上的 Gemma 4 1B→4B（c≈0.23），需 τ > 1.23 才能打平 —— 典型 τ=2.5–4 可带来 1.4–1.7× 加速。
- **Gemma 4 的同族配对**（1B→4B 或 1B→12B）最为理想：tokenizer 相同、架构对齐、IT 分布一致。无需词表映射，也不存在分布失配。
- **1B 与 4B 可同时放进 Orin**（INT4 合计 3.0 GB），并为 KV cache 留出充足空间 —— 在受限硬件上投机 decode 在内存上可行。
- **EAGLE-3 风格的自 draft head** 优于外部 1B draft（τ=3.5–5.0 对 2.5–3.5），内存开销仅为后者的 1/3，但需要 2–4 小时的训练投入。
- **Lookahead decoding** 不需要任何额外权重，在 Gemma 4 输出上可达 τ=1.5–2.5 —— 对重复性输出是白捡的 30–60% 收益，且不增加部署复杂度。
- **务必验证精度一致性**（贪心：完全一致；采样：分布等价），再宣布某次投机 decode 部署达到生产可用。

---


<details>
<summary>English original</summary>

**5. Lookahead decoding — draft-free speculation for Gemma 4**

**Lookahead decoding** (Fu et al., 2024) generates draft tokens without any auxiliary model, using Jacobi iteration over n-gram sequences:

```text
LOOKAHEAD IDEA:
  Standard decode: generate one token at a time (sequential)
  Lookahead: maintain a "lookahead cache" of W candidate tokens,
             verify them in parallel with the target model
             — no draft model needed, no extra weights

MECHANISM (W=4 lookahead, N=5-gram):
  Step 1: propose W tokens simultaneously (random or from n-gram cache)
  Step 2: verify with target in one pass (like speculative decode)
  Step 3: accept longest verified n-gram, extend context, refill lookahead

WHY IT HELPS ON GEMMA 4:
  - No external model memory (0 overhead for draft)
  - Gemma 4's 128K context + sliding-window local attention makes the
    n-gram cache viable: long histories improve n-gram match rates
  - Works well for Gemma 4's verbose, consistent technical outputs
```

```python
# Lookahead decoding via llama.cpp (requires --lookup-cache-static flag):
./build/bin/llama-cli \
    --model gemma4-4b-Q4_K_M.gguf \
    --n-gpu-layers 9999 \
    --lookup-cache-static lookup_cache.bin \  # pre-computed n-gram cache
    --ctx-size 32768 \
    -p "Explain the Gemma 4 attention mechanism in detail."
```

**Lookahead vs draft-model for Gemma 4 on Orin:**

| Method | Extra memory | τ (typical) | Best task |
|--------|-------------|------------|----------|
| No spec decode | 0 | 1.0 | — |
| Gemma 4 1B draft | 500 MB | 2.5–4.0 | Chat, code |
| EAGLE-3 heads | 150 MB | 3.5–5.0 | Chat, code, reasoning |
| Lookahead (W=4) | ~50 MB cache | 1.5–2.5 | Repetitive outputs |
| Lookahead + 1B | 550 MB | 3.0–4.5 | Best of both |

**Recommendation:** For production on Orin with Gemma 4 4B:
- **Fastest to deploy**: Gemma 4 1B external draft (zero training required)
- **Best quality/memory ratio**: EAGLE-3 heads (requires 2–4h training)
- **Zero-cost baseline**: lookahead decoding (no extra weights, helps on repetitive outputs)

---

**6. Output parity verification**

Speculative decoding is theoretically lossless. In practice, floating-point non-associativity can introduce tiny numerical differences that cascade. Always verify:

```python
import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

model_id = "google/gemma-4-4b-it"
tokenizer = AutoModelForCausalLM.from_pretrained(model_id)
model_base = AutoModelForCausalLM.from_pretrained(model_id)

# 1. Standard greedy decode (reference):
reference_outputs = model_base.generate(
    inputs, max_new_tokens=200, do_sample=False, temperature=1.0
)

# 2. With speculative decode (implementation-specific):
spec_outputs = run_speculative_decode(...)

# 3. Verify token-level parity:
assert torch.all(reference_outputs == spec_outputs), \
    f"Parity failure at token {(reference_outputs != spec_outputs).nonzero()[0]}"
```

**Greedy decode is provably deterministic with speculative decoding** — the acceptance rule guarantees exact equivalence for greedy (argmax) sampling. For temperature > 0 (stochastic sampling), the output distributions are identical but individual samples differ (which is expected and correct).

---

**Key takeaways**

- **Speculative decode speedup = τ / (1 + K×c)** where τ is the average accepted tokens, K is draft count, and c is draft/target cost ratio. For Gemma 4 1B→4B on Orin (c≈0.23), you need τ > 1.23 to break even — typical τ=2.5–4 gives 1.4–1.7× speedup.
- **Gemma 4's same-family pairing** (1B→4B or 1B→12B) is ideal: identical tokenizer, aligned architecture, same IT distribution. No vocabulary mapping, no distribution mismatch.
- **Both 1B and 4B fit on Orin simultaneously** (3.0 GB total INT4) with ample room for KV cache — spec decode is memory-viable on constrained hardware.
- **EAGLE-3-style self-draft heads** outperform the external 1B draft (τ=3.5–5.0 vs 2.5–3.5) at 3× lower memory overhead, but require 2–4h training investment.
- **Lookahead decoding** requires zero extra weights and gives τ=1.5–2.5 on Gemma 4 outputs — a free 30–60% gain for repetitive outputs with no deployment complexity.
- **Always verify parity** (greedy: exact; sampled: distribution-equivalent) before declaring a speculative decode deployment production-ready.

---

</details>

## 参考文献

- "Fast Inference from Transformers via Speculative Decoding"（Leviathan et al., 2023）— arXiv:2211.17192
- "Accelerating Large Language Model Decoding with Speculative Sampling"（Chen et al., 2023）— arXiv:2302.01318
- EAGLE-3 论文（Li et al., 2025）— arXiv：查看 [github.com/SafeAILab/EAGLE](https://github.com/SafeAILab/EAGLE)
- "Lookahead Decoding"（Fu et al., 2024）— arXiv:2402.02057
- llama.cpp 投机解码（--model-draft, --draft-max）— [github.com/ggml-org/llama.cpp/blob/master/examples/speculative/](https://github.com/ggml-org/llama.cpp)
- *MLSys Deep Dives — Lecture 06 — Making Decode Fast* — 完整的投机解码演进脉络 — [Lecture-06.md](../../07-方向G-ML系统工程/05-MLSys深度探索/Lecture-06.md)

---

## 截至 2026-06 的最新状态

Gemma 4 1B 模型可在 HuggingFace 的 `google/gemma-4-1b-it` 获取。llama.cpp `--model-draft` 投机解码已确认可用于 GGUF Gemma 4 模型对。EAGLE-3 Gemma 4 官方 head：Gemma 4 权重请查看 SafeAILab/EAGLE 仓库（可能需要社区训练）。llama.cpp 中的 Lookahead decoding 通过 `--lookup-cache-static` 实现。

---

*上一讲：[← Lecture 03](Lecture-03.md) · 上级：[Gemma 4 边缘部署](README.md) · 下一讲：[Lecture 05 — Physical AI 与多模态 Gemma 4](Lecture-05.md)*


<details>
<summary>English original</summary>

**References**

- "Fast Inference from Transformers via Speculative Decoding" (Leviathan et al., 2023) — arXiv:2211.17192
- "Accelerating Large Language Model Decoding with Speculative Sampling" (Chen et al., 2023) — arXiv:2302.01318
- EAGLE-3 paper (Li et al., 2025) — arXiv: check [github.com/SafeAILab/EAGLE](https://github.com/SafeAILab/EAGLE)
- "Lookahead Decoding" (Fu et al., 2024) — arXiv:2402.02057
- llama.cpp speculative decoding (--model-draft, --draft-max) — [github.com/ggml-org/llama.cpp/blob/master/examples/speculative/](https://github.com/ggml-org/llama.cpp)
- *MLSys Deep Dives — Lecture 06 — Making Decode Fast* — full speculative decode lineage — [Lecture-06.md](../../07-方向G-ML系统工程/05-MLSys深度探索/Lecture-06.md)

---

**Current as of 2026-06**

Gemma 4 1B model available at `google/gemma-4-1b-it` on HuggingFace. llama.cpp `--model-draft` speculative decode confirmed working for GGUF Gemma 4 pairs. EAGLE-3 Gemma 4 official heads: check SafeAILab/EAGLE repo for Gemma 4 weights (may require community training). Lookahead decoding in llama.cpp via `--lookup-cache-static`.

---

*Previous: [← Lecture 03](Lecture-03.md) · Up: [Gemma 4 Edge Deployment](README.md) · Next: [Lecture 05 — Physical AI and Multimodal Gemma 4](Lecture-05.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Gemma 4 Edge Deployment/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Gemma%204%20Edge%20Deployment/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
