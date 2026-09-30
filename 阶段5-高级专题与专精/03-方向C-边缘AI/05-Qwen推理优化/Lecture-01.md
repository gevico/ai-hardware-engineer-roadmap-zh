---
title: 第 1 讲：Qwen 架构深度剖析 — Qwen3-4B 与 Qwen2.5-72B 并排对比
description: 第 1 讲：Qwen 架构深度剖析 — Qwen3-4B 与 Qwen2.5-72B 并排对比
---

# 第 1 讲：Qwen 架构深度剖析 — Qwen3-4B 与 Qwen2.5-72B 并排对比

## 概述

优化 Qwen 推理之前，需要知道权重里究竟有什么。本讲完整走一遍两个具体版本的架构：

* **Qwen3-4B-Instruct** — Alibaba 2026 年的 small-instruct 模型。大多数人实际会在边缘上运行的尺寸。
* **Qwen2.5-72B-Instruct** — 2024 年的大型 instruct 主力，仍是部署最多的 70B 级开放权重模型。

两者都是同一家族中的 decoder-only 因果 Transformer。它们共享设计选择（GQA、RoPE-NeoX、SwiGLU FFN、RMSNorm、词表 151 936 的 BPE），但在规模、embedding 绑定和旋转配置上不同，这些差异会改变你的推理 recipe。

到本讲结束时，你应该能够：

* 将 Qwen GGUF / safetensors 文件中的每个字节映射到前向传播中的特定角色。
* 仅凭 `config.json` 计算参数数量和每个张量的内存占用。
* 解释为什么 Qwen3-4B 绑定 embedding 而 Qwen2.5-72B 不绑定 — 以及这在 LM head 上代价是什么。
* 读取 GEMV trace，并仅凭 M 和 K 识别 layer/block/projection。

---

## 1. 家族谱系（简版）

```
Qwen 1.5  ──► Qwen2  ──► Qwen2.5  ──► Qwen3
 (2023)     (2024)      (2024)       (2026)
   │          │            │            │
   │          │            │            ├─ "thinking" mode (CoT toggleable)
   │          │            │            ├─ better tool use
   │          │            ├─ YaRN extended context (128 k)
   │          │            ├─ improved post-training
   │          ├─ GQA standard, dual rope-base for short/long ctx
   │          │
   ├─ original release, MHA only
```

我们涵盖的两个模型都是 **decoder-only**、**causal**、**通过 RoPE 实现双向位置编码**、**为批处理左填充**、**使用 ChatML 风格模板**（`<|im_start|>role\n…<|im_end|>`）。

---

## 2. 驱动一切的配置

每个模型的 `config.json`：

| 字段 | Qwen3-4B-Instruct | Qwen2.5-72B-Instruct |
|---|---|---|
| `hidden_size` (d_model) | 2560 | 8192 |
| `intermediate_size` (FFN) | 6912 | 29568 |
| `num_hidden_layers` | 36 | 80 |
| `num_attention_heads` | 32 | 64 |
| `num_key_value_heads` | 8 (GQA group=4) | 8 (GQA group=8) |
| `head_dim` | 128 | 128 |
| `vocab_size` | 151 936 | 152 064 |
| `max_position_embeddings` | 40 960 (262 144 w/ YaRN) | 32 768 (131 072 w/ YaRN) |
| `rope_theta` | 1 000 000 | 1 000 000 |
| `rope_scaling` | YaRN (rel) | YaRN (rel) |
| `tie_word_embeddings` | **true** | **false** |
| `rms_norm_eps` | 1e-6 | 1e-6 |
| `torch_dtype`（发布版） | BF16 | BF16 |

这十二个数字决定了你在 GEMV trace 中会看到的每一个形状。

### 2.1 手动推导形状

给定配置，每个张量的形状都是机械推导的：

| 张量 | Qwen3-4B | Qwen2.5-72B | 数学 |
|---|---|---|---|
| `token_embd.weight` | 151936 × 2560 | 152064 × 8192 | `vocab × d` |
| 每层：`attn_norm.weight` | 2560 | 8192 | `d` |
| `attn_q.weight` (W_Q) | 2560 × 4096 | 8192 × 8192 | `d × (n_heads · head_dim)` |
| `attn_k.weight` (W_K) | 2560 × 1024 | 8192 × 1024 | `d × (n_kv_heads · head_dim)` |
| `attn_v.weight` (W_V) | 2560 × 1024 | 8192 × 1024 | 与 K 相同（GQA） |
| `attn_q.bias` | 4096 | 8192 | Qwen 保留 QKV 偏置 |
| `attn_k.bias` | 1024 | 1024 |  |
| `attn_v.bias` | 1024 | 1024 |  |
| `attn_o.weight` (W_O) | 4096 × 2560 | 8192 × 8192 | `(n_heads · head_dim) × d` |
| `ffn_norm.weight` | 2560 | 8192 | `d` |
| `ffn_gate.weight` (W_g) | 2560 × 6912 | 8192 × 29568 | `d × intermediate` |
| `ffn_up.weight` (W_u) | 2560 × 6912 | 8192 × 29568 | `d × intermediate` |
| `ffn_down.weight` (W_d) | 6912 × 2560 | 29568 × 8192 | `intermediate × d` |
| `output_norm.weight` | 2560 | 8192 | `d` |
| `output.weight` (LM head) | **绑定** | 152064 × 8192 | （只有 Qwen2.5-72B 有单独的） |


<details>
<summary>English original</summary>

**Lecture 1: Qwen Architecture Deep Dive — Qwen3-4B and Qwen2.5-72B Side by Side**

**Overview**

Before you can optimize Qwen inference, you need to know what's actually in the weights. This lecture is a complete walk of the architecture for two specific releases:

* **Qwen3-4B-Instruct** — Alibaba's 2026 small-instruct model. The size most people will actually run on edge.
* **Qwen2.5-72B-Instruct** — the 2024 large-instruct workhorse, still the most-deployed 70B-class open-weight model.

Both are decoder-only causal transformers in the same family. They share design choices (GQA, RoPE-NeoX, SwiGLU FFN, RMSNorm, BPE with 151 936 vocab) but differ in scale, embedding tying, and rotary configuration in ways that change your inference recipe.

By the end you should be able to:

* Map every byte in a Qwen GGUF / safetensors file to a specific role in the forward pass.
* Compute the parameter count and per-tensor memory footprint from `config.json` alone.
* Explain why Qwen3-4B ties embeddings and Qwen2.5-72B does not — and what that costs at the LM head.
* Read a GEMV trace and identify the layer/block/projection from M and K alone.

---

**1. Family Lineage (the short version)**

```
Qwen 1.5  ──► Qwen2  ──► Qwen2.5  ──► Qwen3
 (2023)     (2024)      (2024)       (2026)
   │          │            │            │
   │          │            │            ├─ "thinking" mode (CoT toggleable)
   │          │            │            ├─ better tool use
   │          │            ├─ YaRN extended context (128 k)
   │          │            ├─ improved post-training
   │          ├─ GQA standard, dual rope-base for short/long ctx
   │          │
   ├─ original release, MHA only
```

Both models we cover are **decoder-only**, **causal**, **bidirectional-positional via RoPE**, **left-padded for batch**, **uses ChatML-style template** (`<|im_start|>role\n…<|im_end|>`).

---

**2. The Config That Drives Everything**

`config.json` for each model:

| Field | Qwen3-4B-Instruct | Qwen2.5-72B-Instruct |
|---|---|---|
| `hidden_size` (d_model) | 2560 | 8192 |
| `intermediate_size` (FFN) | 6912 | 29568 |
| `num_hidden_layers` | 36 | 80 |
| `num_attention_heads` | 32 | 64 |
| `num_key_value_heads` | 8 (GQA group=4) | 8 (GQA group=8) |
| `head_dim` | 128 | 128 |
| `vocab_size` | 151 936 | 152 064 |
| `max_position_embeddings` | 40 960 (262 144 w/ YaRN) | 32 768 (131 072 w/ YaRN) |
| `rope_theta` | 1 000 000 | 1 000 000 |
| `rope_scaling` | YaRN (rel) | YaRN (rel) |
| `tie_word_embeddings` | **true** | **false** |
| `rms_norm_eps` | 1e-6 | 1e-6 |
| `torch_dtype` (release) | BF16 | BF16 |

These twelve numbers determine every shape you'll see in a GEMV trace.

**2.1 Deriving shapes by hand**

Given the config, every tensor's shape is mechanical:

| Tensor | Qwen3-4B | Qwen2.5-72B | Math |
|---|---|---|---|
| `token_embd.weight` | 151936 × 2560 | 152064 × 8192 | `vocab × d` |
| Per layer: `attn_norm.weight` | 2560 | 8192 | `d` |
| `attn_q.weight` (W_Q) | 2560 × 4096 | 8192 × 8192 | `d × (n_heads · head_dim)` |
| `attn_k.weight` (W_K) | 2560 × 1024 | 8192 × 1024 | `d × (n_kv_heads · head_dim)` |
| `attn_v.weight` (W_V) | 2560 × 1024 | 8192 × 1024 | same as K (GQA) |
| `attn_q.bias` | 4096 | 8192 | Qwen keeps QKV bias |
| `attn_k.bias` | 1024 | 1024 |  |
| `attn_v.bias` | 1024 | 1024 |  |
| `attn_o.weight` (W_O) | 4096 × 2560 | 8192 × 8192 | `(n_heads · head_dim) × d` |
| `ffn_norm.weight` | 2560 | 8192 | `d` |
| `ffn_gate.weight` (W_g) | 2560 × 6912 | 8192 × 29568 | `d × intermediate` |
| `ffn_up.weight` (W_u) | 2560 × 6912 | 8192 × 29568 | `d × intermediate` |
| `ffn_down.weight` (W_d) | 6912 × 2560 | 29568 × 8192 | `intermediate × d` |
| `output_norm.weight` | 2560 | 8192 | `d` |
| `output.weight` (LM head) | **tied** | 152064 × 8192 | (only Qwen2.5-72B has a separate one) |

</details>

### 2.2 参数量合理性检查

对于 Qwen3-4B：

```
Embeddings (tied):       151 936 · 2560                   = 389  M
Per layer attention:     2 · 2560·4096 + 2·2560·1024      ≈   26 M
  +  bias                4096 + 2·1024                    =   ~ 6 K (negligible)
Per layer FFN:           3 · 2560 · 6912                  ≈   53 M
Per layer norms:         2 · 2560                         ≈    5 K
Per layer total:                                          ≈   79 M
× 36 layers:                                              ≈ 2.84 B
+ final norm:                                             negligible

Total: 389 M (embed) + 2.84 B (layers) ≈ 3.23 B params
Marketing name "4B" — close enough, depending on bias and norm accounting.
```

对于 Qwen2.5-72B：

```
Embeddings (untied — 2 copies):  2 · 152 064 · 8192     ≈ 2.49 B
Per layer attention:             8192·8192 + 2·8192·1024
                                 + 8192·8192            ≈ 151  M
Per layer FFN:                   3 · 8192 · 29 568      ≈ 727  M
Per layer total:                                        ≈ 878  M
× 80 layers:                                            ≈ 70.2 B
Total:                                                  ≈ 72.7 B params ✓
```

embedding 对 72B 模型的巨大贡献（2.49 B 参数，约占总量的 3.4%）正是在该规模下 **不绑定** 它们是合理选择的原因——但这也是一些你能在 HuggingFace 上找到的定制 Qwen2.5-72B 微调模型事后绑定它们以节省 1.24 B LM head 权重参数的原因。

---

## 3. Attention Block：两者都使用分组查询注意力，压力不同

两个模型都使用 **分组查询注意力**，均有 8 个 KV 头。变化的是 Q 头数。

### 3.1 Qwen3-4B-Instruct

```
n_heads     = 32
n_kv_heads  = 8
group_size  = 32 / 8 = 4    ← every 4 Q heads share 1 K, 1 V
head_dim    = 128
```

每 token 的 KV cache：

```
2 (K and V) · 8 heads · 128 head_dim · 2 bytes (FP16)
 = 4096 bytes per token per layer
 × 36 layers
 = 147 456 bytes per token
 × 4096 ctx = 576 MB
```

### 3.2 Qwen2.5-72B-Instruct

```
n_heads     = 64
n_kv_heads  = 8
group_size  = 64 / 8 = 8    ← every 8 Q heads share 1 K, 1 V
head_dim    = 128
```

每 token 的 KV cache：

```
2 · 8 · 128 · 2 = 4096 bytes per token per layer
× 80 layers     = 327 680 bytes per token
× 32 768 ctx    = 10.0 GB
× 131 072 ctx   = 40.0 GB  ← long-context regime, single-stream
```

72B 更宽的 Q（64 头）会增加计算开销，但其每 token 的 KV 仅比 4B 的 **大 2.2×**，因为两者都使用 8 个 KV 头。这正是分组查询注意力的全部意义：无论 Q 头数多少，都限制 KV cache。

### 3.3 attention block 流程（两个模型完全相同）

```
x[d]
 │
 ├─► RMSNorm(eps=1e-6)
 │      │
 │      ├─► [W_Q + b_Q] → q[n_heads · head_dim]
 │      ├─► [W_K + b_K] → k[n_kv_heads · head_dim]
 │      └─► [W_V + b_V] → v[n_kv_heads · head_dim]
 │
 │     RoPE(q, k)   ← rotary with theta=1e6, NeoX layout
 │
 │     Append k,v to KV cache
 │
 │     attn_out = softmax(q · Kᵀ / √head_dim) · V
 │     (Q has 4× or 8× more heads than K/V — heads in the same group
 │      attend to the same K/V, then concatenate independently)
 │
 │     attn_out → [W_O] → o[d]
 ▼
x + o   (residual)
```

你会忘记一次并后悔的 Qwen 特有细节：**Qwen 保留 QKV bias**（`b_Q, b_K, b_V`）。Llama 和 Mistral 不保留。在导入时剥离 bias 的 runtime 会静默破坏 Qwen 模型。验证你的 loader。

---

## 4. RoPE —— 旋转位置编码（详细版）

位置编码是模型获知 token 顺序的方式。Qwen 使用 **RoPE**（旋转位置编码）——与 Llama、Mistral、Phi、Gemma 同属一个家族。本节会很长，因为 RoPE 正是最细微的 Qwen 特有推理 bug 的所在之处，也是所有长上下文技术（YaRN、NTK-aware、Dual-Chunk）接入之处。


<details>
<summary>English original</summary>

**2.2 Parameter count sanity check**

For Qwen3-4B:

```
Embeddings (tied):       151 936 · 2560                   = 389  M
Per layer attention:     2 · 2560·4096 + 2·2560·1024      ≈   26 M
  +  bias                4096 + 2·1024                    =   ~ 6 K (negligible)
Per layer FFN:           3 · 2560 · 6912                  ≈   53 M
Per layer norms:         2 · 2560                         ≈    5 K
Per layer total:                                          ≈   79 M
× 36 layers:                                              ≈ 2.84 B
+ final norm:                                             negligible

Total: 389 M (embed) + 2.84 B (layers) ≈ 3.23 B params
Marketing name "4B" — close enough, depending on bias and norm accounting.
```

For Qwen2.5-72B:

```
Embeddings (untied — 2 copies):  2 · 152 064 · 8192     ≈ 2.49 B
Per layer attention:             8192·8192 + 2·8192·1024
                                 + 8192·8192            ≈ 151  M
Per layer FFN:                   3 · 8192 · 29 568      ≈ 727  M
Per layer total:                                        ≈ 878  M
× 80 layers:                                            ≈ 70.2 B
Total:                                                  ≈ 72.7 B params ✓
```

The huge embedding contribution to the 72B model (2.49 B params, ~3.4% of total) is exactly why **not tying** them is a defensible choice at that scale — but it's also why some custom Qwen2.5-72B fine-tunes you'll find on HuggingFace tie them after-the-fact to save 1.24 B params of LM head weight.

---

**3. Attention Block: GQA in Both, Different Pressure**

Both models use **Grouped-Query Attention** with 8 KV heads. What changes is the Q-head count.

**3.1 Qwen3-4B-Instruct**

```
n_heads     = 32
n_kv_heads  = 8
group_size  = 32 / 8 = 4    ← every 4 Q heads share 1 K, 1 V
head_dim    = 128
```

KV cache per token:

```
2 (K and V) · 8 heads · 128 head_dim · 2 bytes (FP16)
 = 4096 bytes per token per layer
 × 36 layers
 = 147 456 bytes per token
 × 4096 ctx = 576 MB
```

**3.2 Qwen2.5-72B-Instruct**

```
n_heads     = 64
n_kv_heads  = 8
group_size  = 64 / 8 = 8    ← every 8 Q heads share 1 K, 1 V
head_dim    = 128
```

KV cache per token:

```
2 · 8 · 128 · 2 = 4096 bytes per token per layer
× 80 layers     = 327 680 bytes per token
× 32 768 ctx    = 10.0 GB
× 131 072 ctx   = 40.0 GB  ← long-context regime, single-stream
```

The 72B's wider Q (64 heads) costs you compute, but its KV per token is only **2.2× larger** than the 4B's because both use 8 KV heads. That's the whole point of GQA: bound the KV cache regardless of Q-head count.

**3.3 The attention block flow (identical in both models)**

```
x[d]
 │
 ├─► RMSNorm(eps=1e-6)
 │      │
 │      ├─► [W_Q + b_Q] → q[n_heads · head_dim]
 │      ├─► [W_K + b_K] → k[n_kv_heads · head_dim]
 │      └─► [W_V + b_V] → v[n_kv_heads · head_dim]
 │
 │     RoPE(q, k)   ← rotary with theta=1e6, NeoX layout
 │
 │     Append k,v to KV cache
 │
 │     attn_out = softmax(q · Kᵀ / √head_dim) · V
 │     (Q has 4× or 8× more heads than K/V — heads in the same group
 │      attend to the same K/V, then concatenate independently)
 │
 │     attn_out → [W_O] → o[d]
 ▼
x + o   (residual)
```

The Qwen-specific detail you'll forget once and regret: **Qwen keeps QKV bias** (`b_Q, b_K, b_V`). Llama and Mistral don't. Runtimes that strip bias on import will silently break Qwen models. Verify your loader.

---

**4. RoPE — Rotary Position Embedding (the long version)**

Position encoding is how the model knows token order. Qwen uses **RoPE** (Rotary Position Embedding) — same family as Llama, Mistral, Phi, Gemma. This section gets long because RoPE is where the subtlest Qwen-specific inference bugs live, and where every long-context technique (YaRN, NTK-aware, Dual-Chunk) plugs in.

</details>

### 4.1 为什么用旋转而非加法

更早的架构（原始 Transformer、BERT）会给每个 token embedding 加上一个学习得到的位置向量：

```
x'_i = x_i + p_i        # element-wise add a position vector
```

这对推理有两个问题：

1. `p_i` 向量是按位置逐个学出来的 —— 长于训练上下文的序列没有定义好的行为。
2. 位置信息混入 residual stream，并被包括 FFN 在内的每个下游算子消费 —— 白白把带宽花在那些唯一职责只是说明「我在哪」的维度上。

RoPE 反其道而行：**只旋转 Q 和 K**，原地进行，旋转角与位置成正比。**二维旋转矩阵**就是中学里那个：

```
R(mθ) = [ cos(mθ)  -sin(mθ) ]
        [ sin(mθ)   cos(mθ) ]
```

让它成立的代数性质是：

```
⟨ R(mθ)·Q_m , R(nθ)·K_n ⟩  =  ⟨ Q_m , R((n - m)θ)·K_n ⟩
```

内积（也就是 attention 所用的东西）只依赖**相对**位置 `(n − m)`。模型看到的从来不是绝对坐标，只有相对几何。

对推理工程师的实际影响：

* 位置信息从不进入 FFN —— Q 和 K 是唯一被触碰的张量。
* 把上下文扩展到训练长度之外，「只不过」是为未见过的位置选对旋转角。
* 被缓存的是**旋转后**的 K，不是原始 K。RoPE 发生在 KV 写入**之前**。

### 4.2 RoPE 插在哪里

```
hidden_state ──► RMSNorm ──┬─► q_proj ──► Q ──┐
                            ├─► k_proj ──► K ──┼─► RoPE(Q, K, pos)
                            └─► v_proj ──► V   │           │
                                               │           ▼
                                               │   write rotated K and V
                                               │   to KV cache at pos
                                               │
                              attention(Q,     ▼
                                        K_cache, V_cache) → o_proj
```

RoPE kernel 做的是：

1. 读取每个 head 的 Q 和 K（形状 `[n_heads, head_dim]` 和 `[n_kv_heads, head_dim]`）。
2. 读取为当前位置预先算好的 `cos[pos]` 和 `sin[pos]` 切片。
3. 在 reg 中施加逐对旋转。
4. 写回 Q 和 K，通常原地写回。

看看你的 `rope_kernel` 在 JLLM 构建里的 ptxas 行：

```
Used 15 registers, used 0 barriers, 389 bytes cmem[0]
```

这是典型的 embarrassingly parallel 特征 —— 每个 (head × pair) 元素一个 thread，不用 shared memory，不用同步。该 kernel 在 Q+K 流量上是带宽受限的，而这部分流量相比紧接其前的 QKV projection 权重读取小得多。

### 4.3 成对布局 —— NeoX vs 原始版

旋转施加在 head 维度上成**对**的坐标上。有两种约定：

```
"Original" (Llama-1, GPT-NeoX research code): pairs are interleaved
   indices: [0, 1, 2, 3, …, d-2, d-1]
   pair (0,1), (2,3), (4,5), …, (d-2, d-1)

"NeoX-style" (Qwen, Llama-2/3, Mistral, Phi, Gemma): pairs split halves
   indices: [0, 1, …, d/2-1] | [d/2, d/2+1, …, d-1]
   pair (0, d/2), (1, d/2+1), (2, d/2+2), …, (d/2-1, d-1)
```

如果你的 kernel 是按原始布局写的，却拿它去跑 Qwen 模型，数学照样算，**不会报任何错**，前 ~5–10 个生成的 token 看起来也还合理，因为位置 0–10 的角度很小、破坏也小。再往后，模型就跑飞了。这就是那个经典的 **「通过单元测试、挂在集成测试」** 的 RoPE bug。

验证方法是检查你的 kernel 把 `head_dim` 中的哪两个元素乘上了同一个 `cos`。如果它们相邻（下标 `2i` 和 `2i+1`），那你用的就是原始布局 —— 对 Qwen 来说是错的。

### 4.4 角度 —— 为什么是 `rope_theta = 1 000 000`

每个 pair 下标 `i ∈ [0, head_dim/2)` 的旋转频率为：

```
θ_i = base ^ (-2i / head_dim)
```

其中 `base` 是来自 `config.json` 的 `rope_theta`。Qwen 用的是 `base = 1e6`，**不是** Llama-1 的 `10 000`。

| `base` | 最慢的 pair `i=0` | 最快的 pair `i = head_dim/2 - 1`，head_dim=128 |
|---|---|---|
| 10 000 | 1.0 rad/pos | ~1e-4 rad/pos |
| 1 000 000 | 1.0 rad/pos | ~1e-6 rad/pos |

更大的 base **把频率拉得更开**。高频 pair（`i` 小）转得快 —— 它们编码细粒度的局部顺序。低频 pair（`i` 大）转得慢 —— 它们编码粗粒度的「远处」顺序。

取 `base = 1e6` 时，最慢的 pair 在整个 32 k 训练上下文里几乎不旋转。正是**这一设计选择让 Qwen2.5 和 Qwen3 无需重新训练就能扩展到 131 k token** —— 慢通道是在它们几乎不变的状态下训练出来的，所以把它们稍微外推仍处于分布内。

Llama-1 用 `base = 10 000` 时，慢通道早就用尽了 —— 这就是为什么 Llama-1 不「动手术」就做不了长上下文。


<details>
<summary>English original</summary>

**4.1 Why rotation instead of addition**

Older architectures (original Transformer, BERT) added a learned position vector to each token embedding:

```
x'_i = x_i + p_i        # element-wise add a position vector
```

Two problems for inference:

1. The `p_i` vectors are learned per-position — sequences longer than the training context have no defined behavior.
2. Position information mixes into the residual stream and is consumed by every downstream op including FFN — wasting bandwidth on dimensions whose only job is to say "where am I."

RoPE does the opposite: **rotate Q and K only**, in place, by an angle proportional to position. The **2D rotation matrix** is the high school one:

```
R(mθ) = [ cos(mθ)  -sin(mθ) ]
        [ sin(mθ)   cos(mθ) ]
```

The algebraic property that makes this work:

```
⟨ R(mθ)·Q_m , R(nθ)·K_n ⟩  =  ⟨ Q_m , R((n - m)θ)·K_n ⟩
```

The inner product (which is what attention uses) depends only on the **relative** position `(n − m)`. The model never sees absolute coordinates, only relative geometry.

Practical consequences for an inference engineer:

* Position info never enters the FFN — Q and K are the only tensors touched.
* Extending context past training is "just" picking the right rotation angles for unseen positions.
* The **rotated** K is what gets cached, not the raw K. RoPE happens **before** KV insertion.

**4.2 Where RoPE plugs in**

```
hidden_state ──► RMSNorm ──┬─► q_proj ──► Q ──┐
                            ├─► k_proj ──► K ──┼─► RoPE(Q, K, pos)
                            └─► v_proj ──► V   │           │
                                               │           ▼
                                               │   write rotated K and V
                                               │   to KV cache at pos
                                               │
                              attention(Q,     ▼
                                        K_cache, V_cache) → o_proj
```

The RoPE kernel does:

1. Read the per-head Q and K (shape `[n_heads, head_dim]` and `[n_kv_heads, head_dim]`).
2. Read the precomputed `cos[pos]` and `sin[pos]` slices for the current position.
3. Apply the per-pair rotation in registers.
4. Write Q and K back, usually in place.

Look at your `rope_kernel`'s ptxas line from the JLLM build:

```
Used 15 registers, used 0 barriers, 389 bytes cmem[0]
```

That's an embarrassingly parallel signature — one thread per (head × pair) element, no shared memory, no synchronization. The kernel is bandwidth-bound on Q+K traffic, which is tiny compared to the QKV projection's weight read that just preceded it.

**4.3 Pair layout — NeoX vs original**

The rotation is applied to **pairs** of coordinates along the head dimension. Two conventions:

```
"Original" (Llama-1, GPT-NeoX research code): pairs are interleaved
   indices: [0, 1, 2, 3, …, d-2, d-1]
   pair (0,1), (2,3), (4,5), …, (d-2, d-1)

"NeoX-style" (Qwen, Llama-2/3, Mistral, Phi, Gemma): pairs split halves
   indices: [0, 1, …, d/2-1] | [d/2, d/2+1, …, d-1]
   pair (0, d/2), (1, d/2+1), (2, d/2+2), …, (d/2-1, d-1)
```

If your kernel was written for the original layout and you point it at a Qwen model, the math runs, **no error fires**, and the first ~5–10 generated tokens look plausible because position 0–10 angles are tiny and the corruption is small. Past that, the model goes off the rails. This is the canonical **"passes unit tests, fails integration tests"** RoPE bug.

Verify by inspecting which two elements of `head_dim` your kernel multiplies by the same `cos`. If they are adjacent (indices `2i` and `2i+1`), you've got the original layout — wrong for Qwen.

**4.4 The angle — why `rope_theta = 1 000 000`**

Each pair index `i ∈ [0, head_dim/2)` rotates at frequency:

```
θ_i = base ^ (-2i / head_dim)
```

where `base` is `rope_theta` from `config.json`. Qwen uses `base = 1e6`, **not** Llama-1's `10 000`.

| `base` | Slowest pair `i=0` | Fastest pair `i = head_dim/2 - 1`, head_dim=128 |
|---|---|---|
| 10 000 | 1.0 rad/pos | ~1e-4 rad/pos |
| 1 000 000 | 1.0 rad/pos | ~1e-6 rad/pos |

The larger base **spreads the frequencies further apart**. High-frequency pairs (small `i`) rotate fast — they encode fine-grained local order. Low-frequency pairs (large `i`) rotate slowly — they encode coarse "far away" order.

With `base = 1e6`, the slowest pair barely rotates over the entire 32 k training context. This is **the design choice that lets Qwen2.5 and Qwen3 extend to 131 k tokens without retraining** — the slow channels were trained in a regime where they almost don't change, so extending them slightly stays in-distribution.

Llama-1 with `base = 10 000` exhausts the slow channels much earlier — which is why Llama-1 couldn't do long context without surgery.

</details>

### 4.5 PyTorch 参考实现（NeoX 布局，匹配 Qwen）

```python
import torch

def precompute_cos_sin(head_dim: int, max_pos: int, base: float,
                       device, dtype=torch.float32):
    """Build cos/sin tables once at startup. Tables are tiny (max_pos x head_dim/2)."""
    inv_freq = 1.0 / (base ** (torch.arange(0, head_dim, 2,
                                            device=device, dtype=torch.float32)
                               / head_dim))                 # [head_dim/2]
    positions = torch.arange(max_pos, device=device, dtype=torch.float32)
    freqs = torch.outer(positions, inv_freq)                # [max_pos, head_dim/2]
    return freqs.cos().to(dtype), freqs.sin().to(dtype)


def rope_neox(x, cos, sin, position_ids):
    """
    x:            [B, n_heads, seq_len, head_dim]
    cos, sin:     [max_pos, head_dim/2]
    position_ids: [B, seq_len]
    """
    half = x.shape[-1] // 2
    x1 = x[..., :half]      # first half of head_dim
    x2 = x[..., half:]      # second half
    cos_p = cos[position_ids].unsqueeze(1)   # [B, 1, seq_len, head_dim/2]
    sin_p = sin[position_ids].unsqueeze(1)
    rot1 = x1 * cos_p - x2 * sin_p
    rot2 = x2 * cos_p + x1 * sin_p
    return torch.cat([rot1, rot2], dim=-1)


# In the attention block:
q = rope_neox(q, cos, sin, pos_ids)
k = rope_neox(k, cos, sin, pos_ids)
# V is NOT rotated — only Q and K.
```

具体到推理：

* `cos/sin` 表针对 `max_pos = max_position_embeddings` 预先计算一次，存放在常量或只读内存中。很小——在 `head_dim=128` 和 `max_pos=131072` 下合计约 16 MB，FP16。
* 在位置 `n` 处进行 **decode**（逐 token 生成阶段）时，只需索引单行 `cos[n], sin[n]`——无需跨批进行广播，当然也无需重新计算。
* 旋转后的 K 会被追加到 KV cache。如果 runtime 缓存**旋转前**的 K，则每个 attention 步骤都会在整个前缀上重新计算旋转位置编码（RoPE）——悄无声息的 O(N²) 爆炸。

### 4.6 无需重新训练扩展上下文

当超出训练时的上下文（Qwen2.5-72B 为 32 k，Qwen3-4B 原生为 40 k）时，慢频率开始产生模型在训练期间从未见过的旋转。如果不加校正，困惑度会在边界之外爆炸。有几种技术可以**仅在推理时**修复这个问题，无需微调：

#### 4.6.1 位置插值（PI）
将所有位置索引按 `1/k` 缩放，使模型实际看到压缩后的位置。机械上可行；但会损失精度，因为每一对都得到相同的缩放——包括原本已经正常的快对。用于早期 Llama-2 长上下文调优；未用于 Qwen。

#### 4.6.2 动态 NTK-Aware 旋转位置编码（RoPE）
推理时，**随着序列超出训练上下文，将 `rope_theta` 向上缩放**：

```
base_effective = base · ( k · seq_len / orig_max - (k - 1) ) ^ (head_dim / (head_dim - 2))
```

技巧在于：高频对（在大量旋转上训练过）保持原有行为；低频对获得拉伸后的有效范围。用于早期 Qwen 和 Qwen2 长上下文变体。“动态”部分意味着缩放取决于实际输入长度，而非固定因子。

#### 4.6.3 YaRN——Qwen2.5 / Qwen3 的生产技术
YaRN 从两方面改进 NTK-aware：

* **逐频率策略。** 快对使用普通外推（不缩放）。慢对使用插值（重新缩放）。中间对平滑混合。边界由配置中的 `beta_fast` 和 `beta_slow` 控制。
* **Attention 温度校正。** 重新缩放频率时，attention logits 的方差会变化——softmax 变得更尖锐或更平坦。YaRN 将 attention 分数乘以 `1/√t(s)`，其中 `t(s)` 取决于缩放因子。补偿熵漂移。

该系列中的两个模型都附带 YaRN 配置：

```json
"rope_scaling": {
  "type": "yarn",
  "factor": 4.0,
  "original_max_position_embeddings": 32768,
  "attention_factor": 0.1,
  "beta_fast": 32,
  "beta_slow": 1
}
```

runtime 在启动时读取这些配置，调整 `cos/sin` 预计算，并将 attention 缩放折叠进 softmax。**每个 token 零开销**——只要连接正确，就能免费获得长上下文。

#### 4.6.4 Dual-Chunk RoPE（面向 100 k+）
对于实验性长上下文 Qwen 变体和 “Qwen-Long” 构建，即使 YaRN 也会在超过约 100 k 后开始退化。Dual-Chunk attention 将计算拆分为：

* **局部块**——在 N 个 token 的窗口内，应用标准旋转 RoPE。
* **全局路径**——跨块 attention 使用**重新缩放**的角度来压缩绝对范围，使长距离关系保持在分布内。

这实现为自定义 attention kernel，而非纯 RoPE 补丁。截至 2026 年中，LMDeploy/TurboMind 为 Qwen-LongContext 提供该实现；主线 vLLM 和 TRT-LLM 则依赖 YaRN 服务于生产级 Qwen2.5/Qwen3 产品线。


<details>
<summary>English original</summary>

**4.5 PyTorch reference (NeoX layout, matches Qwen)**

```python
import torch

def precompute_cos_sin(head_dim: int, max_pos: int, base: float,
                       device, dtype=torch.float32):
    """Build cos/sin tables once at startup. Tables are tiny (max_pos x head_dim/2)."""
    inv_freq = 1.0 / (base ** (torch.arange(0, head_dim, 2,
                                            device=device, dtype=torch.float32)
                               / head_dim))                 # [head_dim/2]
    positions = torch.arange(max_pos, device=device, dtype=torch.float32)
    freqs = torch.outer(positions, inv_freq)                # [max_pos, head_dim/2]
    return freqs.cos().to(dtype), freqs.sin().to(dtype)


def rope_neox(x, cos, sin, position_ids):
    """
    x:            [B, n_heads, seq_len, head_dim]
    cos, sin:     [max_pos, head_dim/2]
    position_ids: [B, seq_len]
    """
    half = x.shape[-1] // 2
    x1 = x[..., :half]      # first half of head_dim
    x2 = x[..., half:]      # second half
    cos_p = cos[position_ids].unsqueeze(1)   # [B, 1, seq_len, head_dim/2]
    sin_p = sin[position_ids].unsqueeze(1)
    rot1 = x1 * cos_p - x2 * sin_p
    rot2 = x2 * cos_p + x1 * sin_p
    return torch.cat([rot1, rot2], dim=-1)


# In the attention block:
q = rope_neox(q, cos, sin, pos_ids)
k = rope_neox(k, cos, sin, pos_ids)
# V is NOT rotated — only Q and K.
```

For inference specifically:

* The `cos/sin` tables are precomputed once for `max_pos = max_position_embeddings` and live in constant or read-only memory. Tiny — at `head_dim=128` and `max_pos=131072` they total ~16 MB combined, FP16.
* During **decode** at position `n`, you index a single row `cos[n], sin[n]` — no broadcast across batch needed, and certainly no recomputation.
* The rotated K is what gets appended to the KV cache. If your runtime caches **pre-rotation** K, every attention step recomputes RoPE on the entire prefix — silent O(N²) blow-up.

**4.6 Extending context without retraining**

When you push past the trained context (32 k for Qwen2.5-72B, 40 k for Qwen3-4B native), the slow frequencies start producing rotations the model never saw during training. Without correction, perplexity explodes past the boundary. Several techniques fix this **at inference time only**, no fine-tuning required:

**4.6.1 Position Interpolation (PI)**
Scale all position indices by `1/k` so the model effectively sees compressed positions. Works mechanically; loses precision because every pair gets the same scaling — including the fast pairs that were already fine. Used in early Llama-2 long-context tunes; not used in Qwen.

**4.6.2 Dynamic NTK-Aware RoPE**
At inference, **scale `rope_theta` upward as the sequence grows past training context**:

```
base_effective = base · ( k · seq_len / orig_max - (k - 1) ) ^ (head_dim / (head_dim - 2))
```

The trick: high-frequency pairs (which were trained on many rotations) keep behavior; low-frequency pairs get a stretched effective range. Used in early Qwen and Qwen2 long-context variants. The "dynamic" part means the scaling depends on actual input length, not a fixed factor.

**4.6.3 YaRN — the production technique for Qwen2.5 / Qwen3**
YaRN refines NTK-aware in two ways:

* **Per-frequency policy.** Fast pairs use plain extrapolation (no scaling). Slow pairs use interpolation (rescale). Mid pairs blend smoothly. The boundaries are controlled by `beta_fast` and `beta_slow` in config.
* **Attention temperature correction.** When you rescale frequencies, the variance of attention logits changes — the softmax gets sharper or flatter. YaRN multiplies attention scores by `1/√t(s)` where `t(s)` depends on the scaling factor. Compensates for the entropy drift.

Both models in this series ship YaRN configuration:

```json
"rope_scaling": {
  "type": "yarn",
  "factor": 4.0,
  "original_max_position_embeddings": 32768,
  "attention_factor": 0.1,
  "beta_fast": 32,
  "beta_slow": 1
}
```

The runtime reads these at startup, adjusts the `cos/sin` precompute, and folds the attention scale into the softmax. **Zero per-token cost** — you get long context for free as long as you wire it correctly.

**4.6.4 Dual-Chunk RoPE (for 100 k+)**
For the experimental long-context Qwen variants and "Qwen-Long" builds, even YaRN starts to degrade past ~100 k. Dual-Chunk attention splits the computation into:

* **Local chunks** — within a window of N tokens, standard rotated RoPE applies.
* **Global path** — cross-chunk attention uses **rescaled** angles that compress the absolute range, so long-distance relationships stay in-distribution.

This is implemented as a custom attention kernel, not a pure RoPE patch. As of mid-2026, LMDeploy/TurboMind ships it for Qwen-LongContext; mainline vLLM and TRT-LLM rely on YaRN for the production Qwen2.5/Qwen3 lineup.

</details>

### 4.7 `config.json` 速查表

为推理接入 RoPE 时真正要看的字段：

| 字段 | 用途 | Qwen3-4B-Instruct | Qwen2.5-72B-Instruct |
|---|---|---|---|
| `rope_theta` | 基频 `θ_base` | 1 000 000 | 1 000 000 |
| `rope_scaling.type` | 缩放策略 | `yarn` | `yarn` |
| `rope_scaling.factor` | 上下文倍数 | 4–8 | 4 |
| `rope_scaling.original_max_position_embeddings` | 原生训练上下文 | 32 768 | 32 768 |
| `rope_scaling.beta_fast` / `beta_slow` | YaRN 频率边界 | 32 / 1 | 32 / 1 |
| `max_position_embeddings` | 缩放后允许的最大上下文 | 40 960–262 144 | 131 072 |
| `head_dim` | 决定 `head_dim/2` 个旋转对 | 128 | 128 |

### 4.8 每个推理 runtime 都必须通过的四项检查

1. **配对布局是 NeoX** —— 配对是 `(i, i + head_dim/2)`，不是 `(2i, 2i+1)`。
2. **`rope_scaling` 会被读取并应用** —— 而不只是 `rope_theta`。如果你的 runtime 只认 `rope_theta`，模型能跑到 32 k，超过就崩。
3. **`cos/sin` 表覆盖完整的扩展范围** —— 为 `max_position_embeddings` 预计算，而不是 `original_max_position_embeddings`。
4. **旋转发生在 KV 写入之前** —— 缓存存的是旋转后的 K。如果在 trace 中看到 `kv_append` *先于* `rope`，说明 runtime 的顺序搞错了。

一个 30 秒的集成测试：喂入约 50 000 token 的 prompt，要求补全 20 个 token。如果语法正确且切题，YaRN 就接对了。如果在生成约 10 个 token 之后开始输出乱码，说明 RoPE 缩放坏了。

### 4.9 GEMV trace 中的 RoPE

layer 0 上单次 decode（逐 token 生成阶段）步骤正确插桩的 trace 如下：

```
[GEMV-GPU #0] type=12 M=4096 K=2560     ← Q projection
[GEMV-GPU #1] type=12 M=1024 K=2560     ← K projection
[GEMV-GPU #2] type=14 M=1024 K=2560     ← V projection
[rope] applied at position N             ← rotates Q and K in place
[kv_append] cached K, V at position N    ← cached K is post-rotation
[attention] flash_attention_decode       ← attends over [0..N]
```

如果在 trace 中完全看不到 RoPE，要么是 kernel 本身不吭声（大多数如此），要么是它已被折叠进 QKV-projection kernel 作为融合的 epilogue（vLLM 就是这么做的 —— `qkv_proj_with_rope_kernel`）。把 RoPE 融合进 QKV 可以省下一次 launch 和一次 Q+K 的内存读写 —— 收益不大，但 kernel 现成的话就是白拿。

---

## 5. FFN：非对称尺寸的 SwiGLU

两个模型都使用 **SwiGLU** FFN 变体：

```
ffn(x) = down( silu(gate(x)) ⊙ up(x) )
```

代码形式：

```python
def ffn(x, W_gate, W_up, W_down):
    g = x @ W_gate                  # [d] → [intermediate]
    u = x @ W_up                    # [d] → [intermediate]
    h = torch.nn.functional.silu(g) * u
    return h @ W_down               # [intermediate] → [d]
```

**每层三个 GEMV（矩阵-向量乘）**，中间维度为 **2.7× d_model**（Qwen3-4B：2560 → 6912；Qwen2.5-72B：8192 → 29568）。该比例取决于一个选择：让 FFN 与 attention 的参数量之比接近 2:1。

按参数量算，FFN 是两个 block 中**更大**的那个：

| | Qwen3-4B 每层 | Qwen2.5-72B 每层 |
|---|---|---|
| Attention 参数 | 26 M | 151 M |
| FFN 参数       | 53 M | 727 M |
| FFN / Attention  | 2.0× | 4.8× |

两点结论：
1. 72B 比 4B **更加 FFN 主导**。规模越大，优化 FFN 越重要。
2. FFN-down 的量化比其他任何 tensor 都更伤质量（这在各个大语言模型家族中都被经验证实）。Q6_K 或 per-tensor-skip 是标准 recipe。

---

## 6. 归一化与残差结构

两个模型都使用：

* **Pre-norm RMSNorm**（不是 LayerNorm，也不是 Post-norm）。
* `eps = 1e-6`。
* **仅权重**，norm 上没有 bias。

融合的 residual+RMSNorm kernel 模式（JLLM 称之为 `fused_rmsnorm_residual_kernel`）是标准实现：读 residual stream、累加方差、缩放、写回。把它做成一个 kernel 而不是三个，在带宽受限的硬件上约快 3×。

```
y = x · scale / sqrt(mean(x²) + eps) · w
```

每层有 2 个 norm，外加一个最终输出 norm：

```
Total norms in Qwen3-4B:  2 · 36 + 1 = 73 norm tensors
Total norms in Qwen2.5-72B: 2 · 80 + 1 = 161 norm tensors
```

Norm 权重很小（每个约 `d` 个 float）—— 即使模型是 FP16/INT4，也要以 FP32 存储。FP16 norm 权重带来的数值噪声底不可忽视。

---


<details>
<summary>English original</summary>

**4.7 The `config.json` cheat sheet**

The fields you actually look at when wiring up RoPE for inference:

| Field | Purpose | Qwen3-4B-Instruct | Qwen2.5-72B-Instruct |
|---|---|---|---|
| `rope_theta` | Base frequency `θ_base` | 1 000 000 | 1 000 000 |
| `rope_scaling.type` | Scaling strategy | `yarn` | `yarn` |
| `rope_scaling.factor` | Context multiplier | 4–8 | 4 |
| `rope_scaling.original_max_position_embeddings` | Native training context | 32 768 | 32 768 |
| `rope_scaling.beta_fast` / `beta_slow` | YaRN frequency boundaries | 32 / 1 | 32 / 1 |
| `max_position_embeddings` | Max allowed context after scaling | 40 960–262 144 | 131 072 |
| `head_dim` | Determines `head_dim/2` rotated pairs | 128 | 128 |

**4.8 The four checks every inference runtime must pass**

1. **Pair layout is NeoX** — pairs are `(i, i + head_dim/2)`, not `(2i, 2i+1)`.
2. **`rope_scaling` is read and applied** — not just `rope_theta`. If your runtime only honors `rope_theta`, you get a model that runs at 32 k and breaks past that.
3. **`cos/sin` tables cover the full extended range** — precompute for `max_position_embeddings`, not `original_max_position_embeddings`.
4. **Rotation happens before KV insertion** — the cache stores rotated K. If you see `kv_append` *before* `rope` in a trace, the runtime has the order wrong.

A 30-second integration test: feed a ~50 000-token prompt and ask for a 20-token completion. If grammatical and on-topic, YaRN is wired correctly. If it produces gibberish past ~10 generated tokens, RoPE scaling is broken.

**4.9 RoPE in the GEMV trace**

A correctly-instrumented trace for a single decode step on layer 0 shows:

```
[GEMV-GPU #0] type=12 M=4096 K=2560     ← Q projection
[GEMV-GPU #1] type=12 M=1024 K=2560     ← K projection
[GEMV-GPU #2] type=14 M=1024 K=2560     ← V projection
[rope] applied at position N             ← rotates Q and K in place
[kv_append] cached K, V at position N    ← cached K is post-rotation
[attention] flash_attention_decode       ← attends over [0..N]
```

If you can't see RoPE in your trace at all, either the kernel is silent (most are) or it's been folded into the QKV-projection kernel as a fused epilogue (vLLM does this — `qkv_proj_with_rope_kernel`). Fusing RoPE into QKV saves one launch and one Q+K pass through memory — small win, but free if you have the kernel.

---

**5. FFN: SwiGLU with Asymmetric Sizing**

Both models use the **SwiGLU** FFN variant:

```
ffn(x) = down( silu(gate(x)) ⊙ up(x) )
```

In code:

```python
def ffn(x, W_gate, W_up, W_down):
    g = x @ W_gate                  # [d] → [intermediate]
    u = x @ W_up                    # [d] → [intermediate]
    h = torch.nn.functional.silu(g) * u
    return h @ W_down               # [intermediate] → [d]
```

**Three GEMVs per layer**, intermediate dimension is **2.7× d_model** (Qwen3-4B: 2560 → 6912; Qwen2.5-72B: 8192 → 29568). That ratio is fixed by the choice to keep FFN-vs-attention parameter balance close to 2:1.

The FFN is the **larger** of the two blocks by parameter count:

| | Qwen3-4B per layer | Qwen2.5-72B per layer |
|---|---|---|
| Attention params | 26 M | 151 M |
| FFN params       | 53 M | 727 M |
| FFN / Attention  | 2.0× | 4.8× |

Two takeaways:
1. The 72B is **even more FFN-dominated** than the 4B. Optimizing FFN matters more at scale.
2. Quantization of FFN-down hurts quality more than any other tensor (this is empirically true across LLM families). Q6_K or per-tensor-skip is the standard recipe.

---

**6. Normalization and Residual Structure**

Both models use:

* **Pre-norm RMSNorm** (not LayerNorm, not Post-norm).
* `eps = 1e-6`.
* **Weight only**, no bias on the norm.

The fused residual+RMSNorm kernel pattern (which JLLM names `fused_rmsnorm_residual_kernel`) is the standard implementation: read residual stream, accumulate variance, scale, write. Doing this as one kernel rather than three is ~3× faster on bandwidth-bound hardware.

```
y = x · scale / sqrt(mean(x²) + eps) · w
```

There are 2 norms per layer plus a final output norm:

```
Total norms in Qwen3-4B:  2 · 36 + 1 = 73 norm tensors
Total norms in Qwen2.5-72B: 2 · 80 + 1 = 161 norm tensors
```

Norm weights are tiny (~`d` floats each) — store FP32 even when the model is FP16/INT4. The numerical noise floor from FP16 norm weights is non-trivial.

---

</details>

## 7. Tokenizer

两个模型使用同一族 tokenizer——Qwen 的 BPE，构建在 `tiktoken` 风格的 byte-level 编码之上：

| | Qwen3-4B | Qwen2.5-72B |
|---|---|---|
| `vocab_size` | 151 936 | 152 064 |
| 编码 | BPE, byte-level | BPE, byte-level |
| 多语言 | 是（大量 CJK + Latin） | 是 |
| 特殊 token | `<\|im_start\|>` = 151 644、`<\|im_end\|>` = 151 645 等 | 相同 ID |

相比 Llama（32 k），词表极其庞大，由此带来两个后果：

1. **LM head 巨大。** 对 embedding 共享的 Qwen3-4B，这笔开销只付一次（在 `token_embd` 中）。对 embedding 不共享的 Qwen2.5-72B，最后的 GEMV（矩阵-向量乘）是 `8192 × 152 064 = 1.25 B` 参数——在 FP16 下，单次 GEMV 就要**每 token 读取 2.5 GB 权重**。在 ~2 TB/s 的 A100 80GB 上，仅 LM head 就要 ~1.25 ms。

2. **Token 效率。** 一句典型英文，该 tokenizer 编码出的 token 数比 Llama 的 tokenizer 少 ~30%，中文少 ~50%。这直接提升了翻译类工作负载上感知到的 tok/s。跨模型家族比较时，同口径（apples-to-apples）的 benchmark 必须把这一点计入。

### 7.1 对话模板

```
<|im_start|>system
You are a helpful assistant.<|im_end|>
<|im_start|>user
Hello, this is testing.<|im_end|>
<|im_start|>assistant
<think>

</think>

Hello! It seems like ...<|im_end|>
```

Qwen3 引入了 `<think>…</think>` 块——runtime 可显示或隐藏的可见 CoT。对推理优化而言，这在结构上没有任何改变；它仍然是一条 token 流。但这确实意味着，有相当比例的 decode（逐 token 生成阶段）出的 token 可能落在 `<think>` 块内——终端用户看不到，却消耗同样的带宽。

---

## 8. 把 Qwen 张量映射到 GGUF / safetensors 名称

调试 runtime trace 时需要对齐名称。映射关系：

| HuggingFace safetensors | GGUF | 作用 |
|---|---|---|
| `model.embed_tokens.weight` | `token_embd.weight` | 输入 embedding |
| `model.layers.N.input_layernorm.weight` | `blk.N.attn_norm.weight` | attention 前的 norm |
| `model.layers.N.self_attn.q_proj.weight` | `blk.N.attn_q.weight` | W_Q |
| `model.layers.N.self_attn.q_proj.bias` | `blk.N.attn_q.bias` | b_Q（Qwen 有这些） |
| `model.layers.N.self_attn.k_proj.weight` | `blk.N.attn_k.weight` | W_K |
| `model.layers.N.self_attn.v_proj.weight` | `blk.N.attn_v.weight` | W_V |
| `model.layers.N.self_attn.o_proj.weight` | `blk.N.attn_output.weight` | W_O |
| `model.layers.N.post_attention_layernorm.weight` | `blk.N.ffn_norm.weight` | FFN 前的 norm |
| `model.layers.N.mlp.gate_proj.weight` | `blk.N.ffn_gate.weight` | W_g |
| `model.layers.N.mlp.up_proj.weight` | `blk.N.ffn_up.weight` | W_u |
| `model.layers.N.mlp.down_proj.weight` | `blk.N.ffn_down.weight` | W_d |
| `model.norm.weight` | `output_norm.weight` | 最终 norm |
| `lm_head.weight` | `output.weight`（共享时不存在） | LM head |

张量条目计数：

* Qwen3-4B（共享）：`1 + (2 + 4 + 4 + 4 - 1) · 36 + 1 = 1 + 13 · 36 + 1 = 470`？实际数量取决于 bias 是否单独存储，以及共享的 LM head 是否出现。JLLM trace 显示 Qwen3-4B-AWQ 有 **398 个 tensor**——QKV 上的 bias（多出 3·36 = 108）使这个数字高于 Llama 风格的模型。

* Qwen2.5-72B FP16 不共享：~`2 + 13 · 80 + 1 = 1043` 个 tensor，磁盘上 ~145 GB。

---

## 9. 最终对比

| | Qwen3-4B-Instruct (Q4_K_M) | Qwen2.5-72B-Instruct (FP16) |
|---|---|---|
| 磁盘大小 | ~2.4 GB | ~145 GB |
| 每 token 激活参数 | 4 B | 72 B |
| 每次 decode 每权重实际读取字节数 | ~0.6（Q4_K 平均） | 2（FP16） |
| 每 token 字节数（权重） | 2.4 GB | 145 GB |
| Orin Nano 上的 roofline（性能上界模型）tok/s（50 GB/s） | ~20（理论值） | n/a（放不下） |
| 8×H100 SXM 上的 roofline tok/s（每卡 ~3.35 TB/s，TP=8 → 有效 26.8 TB/s） | n/a | ~185 |
| 每 token KV（8 KV head · 128 head_dim · 2 layers·side） | 147 KB / token | 320 KB / token |
| 最大上下文下的 KV | 576 MB (4 k) / 9.4 GB (64 k) | 10 GB (32 k) / 40 GB (131 k) |
| Embedding 是否共享？ | 是（省 389 MB） | 否（1.24 B LM head 参数） |
| 运行平台 | Jetson Orin Nano 8GB、M2 Mac、iPhone Pro NPU | 4–8 × A100/H100、8 × L40S |

下一讲从差异最大之处讲起——量化，只有 4B 会经历它。

---


<details>
<summary>English original</summary>

**7. Tokenizer**

Both models use the same family of tokenizers — Qwen's BPE built on top of `tiktoken`-style byte-level encoding:

| | Qwen3-4B | Qwen2.5-72B |
|---|---|---|
| `vocab_size` | 151 936 | 152 064 |
| Encoding | BPE, byte-level | BPE, byte-level |
| Multilingual | Yes (heavy CJK + Latin) | Yes |
| Special tokens | `<\|im_start\|>` = 151 644, `<\|im_end\|>` = 151 645, etc. | same IDs |

The vocab is enormous compared to Llama (32 k), which has two consequences:

1. **LM head is huge.** For Qwen3-4B with tied embeddings, you pay it once (in `token_embd`). For Qwen2.5-72B with untied, the final GEMV is `8192 × 152 064 = 1.25 B` parameters — at FP16 that's a single GEMV reading **2.5 GB of weights per token**. On an A100 80GB at ~2 TB/s, that's ~1.25 ms just for the LM head.

2. **Token efficiency.** A typical English sentence is ~30% fewer tokens than Llama's tokenizer encodes it as, ~50% fewer for Chinese. This directly improves perceived tok/s on translated workloads. Your apples-to-apples benchmarks must account for this when comparing across families.

**7.1 Chat template**

```
<|im_start|>system
You are a helpful assistant.<|im_end|>
<|im_start|>user
Hello, this is testing.<|im_end|>
<|im_start|>assistant
<think>

</think>

Hello! It seems like ...<|im_end|>
```

Qwen3 introduces the `<think>…</think>` block — visible CoT that runtime can show or hide. For inference optimization this changes nothing structurally; it's still a token stream. But it does mean a non-trivial fraction of decoded tokens may be inside the `<think>` block — invisible to the end user but consuming the same bandwidth.

---

**8. Mapping Qwen Tensors to GGUF / safetensors Names**

When debugging a runtime trace you need to match names. The mapping:

| HuggingFace safetensors | GGUF | Role |
|---|---|---|
| `model.embed_tokens.weight` | `token_embd.weight` | input embedding |
| `model.layers.N.input_layernorm.weight` | `blk.N.attn_norm.weight` | pre-attention norm |
| `model.layers.N.self_attn.q_proj.weight` | `blk.N.attn_q.weight` | W_Q |
| `model.layers.N.self_attn.q_proj.bias` | `blk.N.attn_q.bias` | b_Q (Qwen has these) |
| `model.layers.N.self_attn.k_proj.weight` | `blk.N.attn_k.weight` | W_K |
| `model.layers.N.self_attn.v_proj.weight` | `blk.N.attn_v.weight` | W_V |
| `model.layers.N.self_attn.o_proj.weight` | `blk.N.attn_output.weight` | W_O |
| `model.layers.N.post_attention_layernorm.weight` | `blk.N.ffn_norm.weight` | pre-FFN norm |
| `model.layers.N.mlp.gate_proj.weight` | `blk.N.ffn_gate.weight` | W_g |
| `model.layers.N.mlp.up_proj.weight` | `blk.N.ffn_up.weight` | W_u |
| `model.layers.N.mlp.down_proj.weight` | `blk.N.ffn_down.weight` | W_d |
| `model.norm.weight` | `output_norm.weight` | final norm |
| `lm_head.weight` | `output.weight` (absent if tied) | LM head |

Counting tensor entries:

* Qwen3-4B (tied): `1 + (2 + 4 + 4 + 4 - 1) · 36 + 1 = 1 + 13 · 36 + 1 = 470`? In practice the count depends on whether biases are stored separately and whether tied LM head appears. The JLLM trace shows **398 tensors** for Qwen3-4B-AWQ — biases on QKV (3·36 = 108 extra) push that up vs Llama-style models.

* Qwen2.5-72B FP16 untied: ~`2 + 13 · 80 + 1 = 1043` tensors, ~145 GB on disk.

---

**9. Final Comparison**

| | Qwen3-4B-Instruct (Q4_K_M) | Qwen2.5-72B-Instruct (FP16) |
|---|---|---|
| On-disk size | ~2.4 GB | ~145 GB |
| Active params per token | 4 B | 72 B |
| Effective bytes/weight read per decode | ~0.6 (Q4_K average) | 2 (FP16) |
| Bytes/token (weights) | 2.4 GB | 145 GB |
| Roofline tok/s on Orin Nano (50 GB/s) | ~20 (theoretical) | n/a (doesn't fit) |
| Roofline tok/s on 8×H100 SXM (~3.35 TB/s each, TP=8 → effective 26.8 TB/s) | n/a | ~185 |
| KV per token (8 KV heads · 128 head_dim · 2 layers·side) | 147 KB / token | 320 KB / token |
| KV at max context | 576 MB (4 k) / 9.4 GB (64 k) | 10 GB (32 k) / 40 GB (131 k) |
| Embeddings tied? | Yes (saves 389 MB) | No (1.24 B LM head params) |
| Where it runs | Jetson Orin Nano 8GB, M2 Mac, iPhone Pro NPUs | 4–8 × A100/H100, 8 × L40S |

The next lecture starts where the difference is biggest — quantization, which only the 4B undergoes.

---

</details>

## 动手练习

1. **验证形状。** 从 HuggingFace 下载 Qwen3-4B-Instruct。打开 `config.json`，按 §2.1 的公式算出每个张量的形状，然后用 `transformers` 加载并断言每个 `param.shape` 都匹配。任何对不上的地方，要么是 runtime 的 bug，要么是你读 config 的 bug。

2. **参数量核对。** 只凭 `config.json` 算出 Qwen2.5-72B-Instruct 的总参数量。与真实加载得到的 `sum(p.numel() for p in model.parameters())` 对比。两者误差应在 0.1% 以内；若不是，说明你把 norm 或 bias 数错了。

3. **trace ↔ 张量映射。** 取上一讲的 JLLM 日志（`[GEMV-GPU #0] type=12 M=4096 K=2560`），依据 `config.json` 证明这**必然**是 `blk.0` 的 Q 投影。对 #1 和 #2 重复同样的推导。然后预测 `blk.0` 中接下来的 4 个 GEMV 应该是什么（形状和类型）。

4. **KV cache 计算器。** 写一个小 Python 工具，输入 `(n_layers, n_kv_heads, head_dim, kv_dtype, ctx)`，返回 KV cache 的字节数。用它画出 Qwen3-4B 与 Qwen2.5-72B 在 1 k 到 128 k token 范围内 KV 大小随上下文长度变化的曲线。标出 Orin Nano、L40S 48GB、A100 80GB、H100 80GB 的 GPU 显存预算。

5. **RoPE 变体检查。** 选一个 runtime（llama.cpp、MLC 或你自己写的），通过检查确认它的 `rope_kernel` 对 Qwen 用的是 NeoX（split）布局。最快的验证方式：把它实际施加的旋转矩阵 dump 出来，与两种顺序下的 `cos(theta_i) ± sin(theta_i)` 模式分别对比。

6. **YaRN 集成测试。** 取 Qwen3-4B-Instruct，喂给它一份 50 000 token 的文档，在接近 token 45 000 处埋一个独一无二的事实（“the password is `puffin-spruce-47`”）。最后问模型：“What was the password?” 若 YaRN 接得正确，模型能取回它；若不对，答案是乱码或拒答。分别在启用和禁用 `--rope-scaling yarn` 的情况下跑一遍，确认 runtime 里到底哪个开关起作用。

7. **从零实现 RoPE。** 照抄 §4.5 的 PyTorch 参考实现。加载 Qwen3-4B，用你的实现替换 `transformers` 内置的 RoPE。跑几个 prompt，确认输出与参考实现逐 bit 一致。然后故意换成原始布局的配对（用 `(2i, 2i+1)` 而不是 `(i, i+d/2)`），观察前 ~10 个 token 看着还算合理，之后输出就崩掉。

8. **逐频率的 YaRN 检查。** 画出 `pos ∈ [0, 32k, 100k, 131k]` 和配对索引 `i ∈ [0, 16, 32, 48, 63]` 的 `cos[pos]` 表值。施加 YaRN 后，快配对应与不加 YaRN 时相同；慢配对应呈现平滑插值。若不是这样，说明 YaRN 的逐频率策略没有被应用。

---

## 关键要点

| 要点 | 为什么重要 |
|---|---|
| `config.json` 中的十二个数字决定每个张量的形状 | 不加载模型就能预测 trace 里的每一个 GEMV |
| GQA 决定 KV cache 的上界，与 Q head 数量无关 | KV 随 `n_kv_heads · n_layers` 增长，而非 `n_heads` |
| Qwen 保留 QKV bias —— Llama/Mistral 不保留 | 剥掉 bias 的 loader 会悄无声息地弄坏 Qwen |
| RoPE-NeoX 布局（配对是拆分的前后半，而非交错） | 布局错误 = 生成约 10 个 token 后悄然损坏 |
| `rope_theta = 1e6` 拉开频率间隔，为长上下文留出余量 | 唯一一个无需重训练就能撑起 131 k 上下文的配置值 |
| 支撑 131 k 上下文的是 YaRN，而不是裸的 `rope_theta` | 只读取 `rope_theta` 的 runtime 会悄无声息地弄坏长输入 |
| cache 存的是旋转后的 K，不是原始 K | RoPE 必须在 `kv_append` 之前执行，否则 attention 会悄无声息地算错 |
| Qwen3-4B 上 embedding 绑定，Qwen2.5-72B 上不绑定 | LM head 是 72B 中最大的 GEMV 之一；在 4B 中很便宜 |
| FFN 在参数量上比 attention 占比更大 | 优化 FFN 路径比优化 attention 收益更大 |
| `<think>` 块消耗与可见输出相同的带宽 | 实际部署中的 decode 预算必须包含隐藏的 CoT |

---


<details>
<summary>English original</summary>

**Hands-On Exercises**

1. **Verify the shapes.** Download Qwen3-4B-Instruct from HuggingFace. Open `config.json`, compute every per-tensor shape from the formulas in §2.1, then load with `transformers` and assert each `param.shape` matches. Anything that doesn't match is either a runtime bug or your config-reading bug.

2. **Param-count sanity.** Compute the total parameter count for Qwen2.5-72B-Instruct from `config.json` alone. Compare with `sum(p.numel() for p in model.parameters())` from a real load. They should agree within 0.1%; if not, you've miscounted norms or biases.

3. **Trace ↔ tensor mapping.** Take the JLLM log from the previous lecture (`[GEMV-GPU #0] type=12 M=4096 K=2560`) and prove from `config.json` that this **must** be the Q projection of `blk.0`. Repeat for #1 and #2. Then predict what the next 4 GEMVs in `blk.0` should be (shapes and types).

4. **KV cache calculator.** Build a small Python utility that takes `(n_layers, n_kv_heads, head_dim, kv_dtype, ctx)` and returns KV cache bytes. Use it to plot KV size vs context length for both Qwen3-4B and Qwen2.5-72B from 1 k to 128 k tokens. Mark the GPU-memory budget for Orin Nano, L40S 48GB, A100 80GB, H100 80GB.

5. **RoPE flavor check.** Pick a runtime (llama.cpp, MLC, or your own) and confirm via inspection that its `rope_kernel` uses the NeoX (split) layout for Qwen. The fastest verification: dump the rotation matrix it applies and compare against `cos(theta_i) ± sin(theta_i)` patterns in either order.

6. **YaRN integration test.** Take Qwen3-4B-Instruct and feed it a 50 000-token document with a single distinctive fact buried near token 45 000 ("the password is `puffin-spruce-47`"). Ask the model at the end: "What was the password?" If YaRN is wired correctly the model retrieves it; if not, the answer is gibberish or a refusal. Run this with `--rope-scaling yarn` enabled and disabled to confirm which knob in your runtime matters.

7. **Implement RoPE from scratch.** Copy the §4.5 PyTorch reference. Load Qwen3-4B and replace `transformers`'s built-in RoPE with your implementation. Run a few prompts and confirm bit-identical output to the reference implementation. Then deliberately swap to original-layout pairs (`(2i, 2i+1)` instead of `(i, i+d/2)`) and observe the first ~10 tokens look reasonable before output collapses.

8. **Per-frequency YaRN inspection.** Plot the `cos[pos]` table values for `pos ∈ [0, 32k, 100k, 131k]` and pair indices `i ∈ [0, 16, 32, 48, 63]`. With YaRN applied, fast pairs should look the same as without YaRN; slow pairs should show smooth interpolation. If they don't, YaRN's per-frequency policy isn't being applied.

---

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| Twelve numbers in `config.json` determine every tensor shape | You can predict every GEMV in a trace without loading the model |
| GQA bounds the KV cache regardless of Q-head count | KV scales with `n_kv_heads · n_layers`, not `n_heads` |
| Qwen keeps QKV biases — Llama/Mistral don't | Loaders that strip bias silently break Qwen |
| RoPE-NeoX layout (pairs are split halves, not interleaved) | Wrong layout = silent corruption past ~10 generated tokens |
| `rope_theta = 1e6` spreads frequencies for long-context headroom | The single config value that makes 131 k context possible without retraining |
| YaRN, not raw `rope_theta`, is what enables 131 k context | Runtimes that only read `rope_theta` silently break long inputs |
| Cache stores rotated K, not raw K | RoPE must run before `kv_append`, or attention is silently wrong |
| Tied embeddings on Qwen3-4B, untied on Qwen2.5-72B | LM head is one of the largest GEMVs in the 72B; cheap in 4B |
| FFN dominates parameters more than attention | Optimizing FFN paths buys more than optimizing attention |
| `<think>` blocks consume the same bandwidth as visible output | Real-world decode budgets must include hidden CoT |

---

</details>

## 资源

* **[Qwen3 Technical Report (2026)](https://arxiv.org/abs/2505.09388):** 官方架构与后训练说明。
* **[Qwen2.5 Technical Report (2024)](https://arxiv.org/abs/2412.15115):** 包含全部配置的 72B 论文。
* **[Hugging Face — Qwen/Qwen3-4B-Instruct](https://huggingface.co/Qwen/Qwen3-4B-Instruct-2507):** 权重、tokenizer、生成配置。
* **[Hugging Face — Qwen/Qwen2.5-72B-Instruct](https://huggingface.co/Qwen/Qwen2.5-72B-Instruct):** 权重与 `config.json`。
* **[RoFormer: Enhanced Transformer with Rotary Position Embedding (Su et al., 2021)](https://arxiv.org/abs/2104.09864):** RoPE 原始论文。
* **[YaRN: Efficient Context Window Extension](https://arxiv.org/abs/2309.00071):** 两个模型长上下文行为背后的数学原理。
* **["Scaling Laws of RoPE-based Extrapolation" (NTK-aware analysis)](https://arxiv.org/abs/2310.05209):** 为什么 `rope_theta` 重要，以及 dynamic-NTK 如何工作。
* **["Dual Chunk Attention" (Qwen-LongContext team, 2024)](https://arxiv.org/abs/2402.17463):** Qwen-Long 各变体中使用的 100 k+ 上下文技术。
* **[Hugging Face Transformers — `modeling_qwen2.py`](https://github.com/huggingface/transformers/blob/main/src/transformers/models/qwen2/modeling_qwen2.py):** 生产级 RoPE-NeoX 参考实现 (`apply_rotary_pos_emb`)。
* **["GQA: Training Generalized Multi-Query Transformer Models"](https://arxiv.org/abs/2305.13245):** GQA 原始论文。
* **[llama.cpp GGUF format spec](https://github.com/ggerganov/ggml/blob/master/docs/gguf.md):** 张量命名与磁盘布局。
* **[阶段 5 — Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md):** 前置要求 roofline（性能上界模型）讲座。


<details>
<summary>English original</summary>

**Resources**

* **[Qwen3 Technical Report (2026)](https://arxiv.org/abs/2505.09388):** Official architecture and post-training description.
* **[Qwen2.5 Technical Report (2024)](https://arxiv.org/abs/2412.15115):** The 72B paper with all configs.
* **[Hugging Face — Qwen/Qwen3-4B-Instruct](https://huggingface.co/Qwen/Qwen3-4B-Instruct-2507):** Weights, tokenizer, generation config.
* **[Hugging Face — Qwen/Qwen2.5-72B-Instruct](https://huggingface.co/Qwen/Qwen2.5-72B-Instruct):** Weights and `config.json`.
* **[RoFormer: Enhanced Transformer with Rotary Position Embedding (Su et al., 2021)](https://arxiv.org/abs/2104.09864):** The original RoPE paper.
* **[YaRN: Efficient Context Window Extension](https://arxiv.org/abs/2309.00071):** The math behind both models' long-context behavior.
* **["Scaling Laws of RoPE-based Extrapolation" (NTK-aware analysis)](https://arxiv.org/abs/2310.05209):** Why `rope_theta` matters and how dynamic-NTK works.
* **["Dual Chunk Attention" (Qwen-LongContext team, 2024)](https://arxiv.org/abs/2402.17463):** The 100 k+ context technique used in Qwen-Long variants.
* **[Hugging Face Transformers — `modeling_qwen2.py`](https://github.com/huggingface/transformers/blob/main/src/transformers/models/qwen2/modeling_qwen2.py):** Production-grade RoPE-NeoX reference (`apply_rotary_pos_emb`).
* **["GQA: Training Generalized Multi-Query Transformer Models"](https://arxiv.org/abs/2305.13245):** Original GQA paper.
* **[llama.cpp GGUF format spec](https://github.com/ggerganov/ggml/blob/master/docs/gguf.md):** Tensor naming and on-disk layout.
* **[Phase 5 — Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md):** The prerequisite roofline lecture.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Qwen Inference Optimization/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Qwen%20Inference%20Optimization/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
