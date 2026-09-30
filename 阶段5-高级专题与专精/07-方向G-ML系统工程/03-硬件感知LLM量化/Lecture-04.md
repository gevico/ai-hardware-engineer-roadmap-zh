---
title: 模块 04 — 模型解剖：找出真正关键的字节
description: 模块 04 — 模型解剖：找出真正关键的字节
---

# 模块 04 — 模型解剖：找出真正关键的字节

**合集：** [面向硬件的 LLM 量化](README.md) | **上一模块：** [← 模块 03](Lecture-03.md) | **下一模块：** [模块 05 →](Lecture-05.md)

---

[模块 01](Lecture-01.md) 曾指出，检查点大小与 per-token 流量是两个不同的量。本模块构建区分二者的工具——**per-token 字节账本**——再用它从四个已公开的数字反推整个模型的架构。

本模块把框架变成算术，让你一个下午就能在任意检查点上跑一遍。

---

## 学习目标

学完本模块，你应能：

1. 把检查点中的每个张量归入四种流量类别之一。
2. 构建 per-token 字节账本，并据此预测 batch-1 decode（逐 token 生成阶段）吞吐。
3. 从字节清单中重建模型的 hidden dimension、词表规模和参数构成。
4. 解释为何启用推测（speculation）或多模态时账本会变化。
5. 在不跑实验的情况下找出检查点中机会最大的张量。

---

## 1. 四种流量类别

LLM 检查点中的每个张量都恰好属于以下之一：

```text
   ┌─────────────────────────────────────────────────────────────────────────┐
   │ CLASS A — STREAMED                                                       │
   │   Read in full, every forward pass. THE decode critical path.            │
   │   → MLP weights, attention projections, lm_head, per-layer norms         │
   │   Optimization target: YES. This is where tok/s lives.                   │
   ├─────────────────────────────────────────────────────────────────────────┤
   │ CLASS B — GATHERED                                                        │
   │   Resident in full, indexed sparsely. Capacity cost, ~no traffic cost.   │
   │   → input embedding table                                                 │
   │   Optimization target: only if VRAM-constrained.                          │
   ├─────────────────────────────────────────────────────────────────────────┤
   │ CLASS C — CONDITIONAL                                                     │
   │   Traffic depends on SERVING CONFIG, not on the checkpoint.               │
   │   → vision/audio towers, MTP or draft heads, LoRA adapters                │
   │   Optimization target: depends. Ledger it twice — enabled and disabled.  │
   ├─────────────────────────────────────────────────────────────────────────┤
   │ CLASS D — STATE                                                           │
   │   Not in the checkpoint at all. Grows with context and batch.             │
   │   → KV cache, workspace, activation buffers                               │
   │   Optimization target: dominant at long context (Module 09).             │
   └─────────────────────────────────────────────────────────────────────────┘
```


账本能防止的错误，是因为某个 B 类或 C 类张量体积大就把它当作 A 类。**大小完全不决定类别归属。**

---


<details>
<summary>English original</summary>

**Module 04 — Model Anatomy: Finding the Bytes That Matter**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 03](Lecture-03.md) | **Next:** [Module 05 →](Lecture-05.md)

---

[Module 01](Lecture-01.md) claimed that checkpoint size and per-token traffic are different quantities. This module builds the instrument that separates them — the **per-token byte ledger** — and then uses it to reverse-engineer an entire model's architecture from four published numbers.

This is the module that turns the framework into arithmetic you can run on any checkpoint in an afternoon.

---

**Learning objectives**

By the end of this module you should be able to:

1. Classify every tensor in a checkpoint into one of four traffic classes.
2. Build a per-token byte ledger and predict batch-1 decode throughput from it.
3. Reconstruct a model's hidden dimension, vocabulary, and parameter split from its byte inventory.
4. Explain why the ledger changes when speculation or multimodality is enabled.
5. Identify the highest-opportunity tensor in a checkpoint without running an experiment.

---

**1. The four traffic classes**

Every tensor in an LLM checkpoint belongs to exactly one of these:

```text
   ┌─────────────────────────────────────────────────────────────────────────┐
   │ CLASS A — STREAMED                                                       │
   │   Read in full, every forward pass. THE decode critical path.            │
   │   → MLP weights, attention projections, lm_head, per-layer norms         │
   │   Optimization target: YES. This is where tok/s lives.                   │
   ├─────────────────────────────────────────────────────────────────────────┤
   │ CLASS B — GATHERED                                                        │
   │   Resident in full, indexed sparsely. Capacity cost, ~no traffic cost.   │
   │   → input embedding table                                                 │
   │   Optimization target: only if VRAM-constrained.                          │
   ├─────────────────────────────────────────────────────────────────────────┤
   │ CLASS C — CONDITIONAL                                                     │
   │   Traffic depends on SERVING CONFIG, not on the checkpoint.               │
   │   → vision/audio towers, MTP or draft heads, LoRA adapters                │
   │   Optimization target: depends. Ledger it twice — enabled and disabled.  │
   ├─────────────────────────────────────────────────────────────────────────┤
   │ CLASS D — STATE                                                           │
   │   Not in the checkpoint at all. Grows with context and batch.             │
   │   → KV cache, workspace, activation buffers                               │
   │   Optimization target: dominant at long context (Module 09).             │
   └─────────────────────────────────────────────────────────────────────────┘
```

The mistake the ledger prevents is treating a Class B or C tensor as though it were Class A because it is large. **Size determines class membership not at all.**

---

</details>

## 2. 从字节重建模型

以案例研究的检查点为例。只用已公开的事实，别的一概不用：

```text
   resident weights      :  18.80 GiB
   BF16 remaining        :   6.91 GB  (33.6 % of checkpoint)
   of which:  embeddings :   2.54 GB
              lm_head    :   2.54 GB
              vision     :   0.92 GB
              MTP head   :   0.85 GB
   decode throughput     :  81.6 tok/s
```

**步骤 1 —— 归一化单位。** `18.80 GiB × 1024³ / 10⁹ = 20.19 GB`.

**步骤 2 —— 余量就是量化后的主体。**

```text
   20.19 GB (total)  −  6.91 GB (BF16)  =  13.28 GB  in NVFP4
```

**步骤 3 —— 把字节换算成参数**，使用 [Module 02](Lecture-02.md) 的 bits-per-weight（NVFP4 = 0.5625 B/param，BF16 = 2 B/param）：

| 分组 | 字节 | ÷ B/param | **参数** |
|---|---:|---:|---:|
| Transformer 主体（NVFP4） | 13.28 GB | 0.5625 | **23.61 B** |
| Embeddings（BF16） | 2.54 GB | 2.0 | 1.270 B |
| lm_head（BF16） | 2.54 GB | 2.0 | 1.270 B |
| Vision tower（BF16） | 0.92 GB | 2.0 | 0.460 B |
| MTP head（BF16） | 0.85 GB | 2.0 | 0.425 B |
| Norms / 杂项 | 0.06 GB | 2.0 | 0.030 B |
| **总计** | **20.19 GB** | | **27.06 B** |

**27.06 B 参数。** 仅凭字节数，该检查点就能重建出一个 27 B 模型，误差在 0.2 % 以内。内部自洽正是这本账正确的证明。

**步骤 4 —— 推断架构。** Embeddings 与 `lm_head` 大小相同，所以该模型的输入/输出 embedding 是 **untied** 的，且各自为 `vocab × d_model = 1.270 B`。求解 plausible hidden size：

```text
   d_model = 8192   →  vocab ≈ 155,000     ← consistent with a ~152–156 K Qwen-family tokenizer
   d_model = 5120   →  vocab ≈ 248,000     ← implausibly large
   d_model = 4096   →  vocab ≈ 310,000     ← implausible
```

所以：**`d_model ≈ 8192`，`vocab ≈ 155 K`，untied embedding，Transformer 主体中约 23.6 B 参数。** 全部由四个数字和 bits-per-weight 表推导而来。

> 这是一项真正有用的技能。你会不断遇到无法查看 config 的检查点（竞品发布的模型、量化产物、只能通过 API 看到的服务）。字节清单能告诉你所需的大部分信息。

---

## 3. 每 token 字节账

现在看真正能预测吞吐的那本账。**纯文本 decode（逐 token 生成阶段），关闭推测，短上下文：**

| 张量分组 | 字节 | 类别 | 每 token 读取 | 原因 |
|---|---:|---|---:|---|
| Transformer 主体 | 13.28 GB | **A** | **13.28 GB** | 每个权重都参与 |
| lm_head | 2.54 GB | **A** | **2.54 GB** | 全词表投影 |
| Norms / 杂项 | 0.06 GB | **A** | **0.06 GB** | 每 layer 都有，极小但需流式读取 |
| Embeddings | 2.54 GB | B | **~16 KB** | 一行：`8192 × 2 B` |
| Vision tower | 0.92 GB | C | **0** | 请求中没有图像 |
| MTP head | 0.85 GB | C | **0** | 已关闭推测 |
| KV cache | — | D | 短 ctx 下很小 | Module 09 |
| | | | | |
| **`B_token`** | | | **≈ 15.88 GB** | |

```text
   resident  20.19 GB   ─────▶   B_token  15.88 GB
                                  │
                    4.31 GB (21 %) of the checkpoint is NEVER
                    fetched during a text-only decode step
```

**用实测值验证：**

```text
   BW_achieved  =  15.88 GB × 81.6 tok/s  =  1296 GB/s  =  72.3 % of 1792 GB/s
```

与使用常驻字节的朴素计算对比：

```text
   naive :  20.19 GB × 81.6  =  1647 GB/s  =  91.9 %   ← wrong; charges for 4.31 GB never read
   real  :  15.88 GB × 81.6  =  1296 GB/s  =  72.3 %   ← correct
```

工程结论截然相反：

| 如果利用率是… | 那么瓶颈约束是… | 下一步动作是… |
|---|---|---|
| 92 % | 字节 | 进一步量化 |
| **72 %** | **kernel / launch / scheduling** | **先 profile kernel** |

```text
   headroom at CONSTANT bytes:
        1650 GB/s (92 % of peak)  ÷  15.88 GB  =  103.9 tok/s
        measured                                =   81.6 tok/s
                                                   ─────────────
        available without removing one bit      =  +22.3 tok/s  (+27 %)
```

**每秒 22 个 token 的差距在 kernel 效率上，而不在比特上。** 这正是这本账的价值所在——它扭转了整个优化方向。

---


<details>
<summary>English original</summary>

**2. Reconstructing a model from its bytes**

Take the case-study checkpoint. Published facts, and nothing else:

```text
   resident weights      :  18.80 GiB
   BF16 remaining        :   6.91 GB  (33.6 % of checkpoint)
   of which:  embeddings :   2.54 GB
              lm_head    :   2.54 GB
              vision     :   0.92 GB
              MTP head   :   0.85 GB
   decode throughput     :  81.6 tok/s
```

**Step 1 — normalize units.** `18.80 GiB × 1024³ / 10⁹ = 20.19 GB`.

**Step 2 — the residual is the quantized body.**

```text
   20.19 GB (total)  −  6.91 GB (BF16)  =  13.28 GB  in NVFP4
```

**Step 3 — convert bytes to parameters** using bits-per-weight from [Module 02](Lecture-02.md) (NVFP4 = 0.5625 B/param, BF16 = 2 B/param):

| Group | Bytes | ÷ B/param | **Parameters** |
|---|---:|---:|---:|
| Transformer body (NVFP4) | 13.28 GB | 0.5625 | **23.61 B** |
| Embeddings (BF16) | 2.54 GB | 2.0 | 1.270 B |
| lm_head (BF16) | 2.54 GB | 2.0 | 1.270 B |
| Vision tower (BF16) | 0.92 GB | 2.0 | 0.460 B |
| MTP head (BF16) | 0.85 GB | 2.0 | 0.425 B |
| Norms / misc | 0.06 GB | 2.0 | 0.030 B |
| **Total** | **20.19 GB** | | **27.06 B** |

**27.06 B parameters.** The checkpoint reconstructs to a 27 B model to within 0.2 %, from byte counts alone. The internal consistency is the proof that the ledger is correct.

**Step 4 — infer the architecture.** Embeddings and `lm_head` are the same size, so the model has **untied** input/output embeddings, and each is `vocab × d_model = 1.270 B`. Solving for plausible hidden sizes:

```text
   d_model = 8192   →  vocab ≈ 155,000     ← consistent with a ~152–156 K Qwen-family tokenizer
   d_model = 5120   →  vocab ≈ 248,000     ← implausibly large
   d_model = 4096   →  vocab ≈ 310,000     ← implausible
```

So: **`d_model ≈ 8192`, `vocab ≈ 155 K`, untied embeddings, ~23.6 B params in the transformer body.** All of it derived from four numbers and the bits-per-weight table.

> This is a genuinely useful skill. You will constantly face checkpoints whose configs you cannot inspect (a competitor's release, a quantized artifact, a service you only see through an API). The byte inventory tells you most of what you need.

---

**3. The per-token byte ledger**

Now the ledger that actually predicts throughput. **Text-only decode, speculation disabled, short context:**

| Tensor group | Bytes | Class | Read per token | Why |
|---|---:|---|---:|---|
| Transformer body | 13.28 GB | **A** | **13.28 GB** | every weight participates |
| lm_head | 2.54 GB | **A** | **2.54 GB** | full vocab projection |
| Norms / misc | 0.06 GB | **A** | **0.06 GB** | per-layer, tiny but streamed |
| Embeddings | 2.54 GB | B | **~16 KB** | one row: `8192 × 2 B` |
| Vision tower | 0.92 GB | C | **0** | no image in the request |
| MTP head | 0.85 GB | C | **0** | speculation disabled |
| KV cache | — | D | small at short ctx | Module 09 |
| | | | | |
| **`B_token`** | | | **≈ 15.88 GB** | |

```text
   resident  20.19 GB   ─────▶   B_token  15.88 GB
                                  │
                    4.31 GB (21 %) of the checkpoint is NEVER
                    fetched during a text-only decode step
```

**Validate against the measurement:**

```text
   BW_achieved  =  15.88 GB × 81.6 tok/s  =  1296 GB/s  =  72.3 % of 1792 GB/s
```

Compare with the naive calculation that uses resident bytes:

```text
   naive :  20.19 GB × 81.6  =  1647 GB/s  =  91.9 %   ← wrong; charges for 4.31 GB never read
   real  :  15.88 GB × 81.6  =  1296 GB/s  =  72.3 %   ← correct
```

The engineering conclusions are opposite:

| If utilization is… | Then the binding constraint is… | And the next move is… |
|---|---|---|
| 92 % | bytes | quantize more |
| **72 %** | **kernel / launch / scheduling** | **profile the kernels first** |

```text
   headroom at CONSTANT bytes:
        1650 GB/s (92 % of peak)  ÷  15.88 GB  =  103.9 tok/s
        measured                                =   81.6 tok/s
                                                   ─────────────
        available without removing one bit      =  +22.3 tok/s  (+27 %)
```

**Twenty-two tokens per second are sitting in kernel efficiency, not in bits.** That is the ledger earning its keep — it redirected the entire optimization effort.

---

</details>

## 4. 目标排序

账本建好后，来自 [Module 01](Lecture-01.md) 的机会框架就成了算术。每个候选的上限由它在 `B_token` 中所占的份额决定：

| 候选 | 流量 | 在 `B_token` 中的占比 | 原生路径？ | 最大可能增益 |
|---|---:|---:|---|---:|
| **`lm_head` BF16 → FP8** | 2.54 GB | **16.0 %** | 是 | **+8.7 %** |
| `lm_head` BF16 → NVFP4 | 2.54 GB | 16.0 % | 是 | +13.4 % |
| Embeddings BF16 → FP8 | 16 KB | 0.0001 % | 是 | **+0.0 %** |
| Vision BF16 → NVFP4 | 0 | 0 % | 是 | **+0.0 %** |
| Body NVFP4 → INT3 | 13.28 GB | 83.6 % | **否** | 负增益（Module 03） |

把排名第一的候选代入吞吐方程，并将达成带宽固定在实测的 1296 GB/s：

```text
   lm_head 2.54 GB → 1.27 GB (FP8)     B_token: 15.88 → 14.61 GB
        1296 / 14.61  =  88.7 tok/s          (+8.7 %)

   lm_head 2.54 GB → 0.71 GB (NVFP4)   B_token: 15.88 → 14.05 GB
        1296 / 14.05  =  92.2 tok/s          (+13.0 %)
```

而真正说明问题的对照是：

```text
   Embeddings are the SAME SIZE as lm_head (2.54 GB each).
   Quantizing embeddings :  +0.0 %  throughput,  −1.27 GB VRAM
   Quantizing lm_head    :  +8.7 %  throughput,  −1.27 GB VRAM

   Identical tensors. Identical capacity win. ~150,000× difference in traffic.
```

如果要从这门课里带走一张表，就带走这一张。

---

## 5. 配置变了，账本也跟着变

同一个检查点在不同的推理服务配置下会有**不同的账本**。这就是 Class C 存在的原因。

### 启用推测时（DSpark 构建）

实测：**接受长度 2.886 时达到 155.75 tok/s。** 目标模型现在每 *个被接受的组* 运行一次，而不是每个 token 运行一次：

```text
   target forward passes/s  =  155.75 / 2.886  =  53.97 passes/s
```

每次目标 pass 都会流式读取完整的 `B_token`，而 MTP head 每个周期运行 `K` 次以产生 draft：

| Draft 深度 `K` | 流量（GB/s） | 占峰值 % |
|---:|---:|---:|
| 2 | 946 | 52.8 % |
| 3 | 991 | 55.3 % |
| 4 | 1037 | 57.9 % |

```text
   traffic  =  53.97 × ( 15.88  +  K × 0.85 )   GB/s
                        └ target ┘   └ MTP ┘
```

**启用推测后，带宽利用率从 72 % *降* 到约 55 %。** 这不是性能回退——这正是推测所做的事：把一次权重读取摊薄到约 2.9 个已输出的 token 上。但它带来一个尖锐的后果：

> 在推测配置下，模型**不再受带宽约束。** 它处在峰值的约 55 % 处，这意味着继续做权重量化会**收益递减**，而主导的杠杆变成了**接受长度**（[Module 10](Lecture-10.md)）和 **kernel/启动效率**（[Module 03](Lecture-03.md)）。

这正是账本能得出、直觉得不出的一类结论。同样的模型、同样的权重、同样的 GPU——而正确的优化策略会随推测是否开启而反转。

### 请求中带图像时

在 prefill（首字前的整段计算）过程中，视觉塔从 Class C 移到 Class A：`+0.92 GB` 的流量，每张图像一次，而不是每个 token 一次。它从不进入 decode（逐 token 生成阶段）账本。**量化视觉塔能改善图像 prefill 延迟和 VRAM，但从不改善 tok/s**——正如 [Module 01](Lecture-01.md) 所断言的那样。

---


<details>
<summary>English original</summary>

**4. Ranking the targets**

With the ledger built, the opportunity framework from [Module 01](Lecture-01.md) becomes arithmetic. Each candidate's ceiling is bounded by its share of `B_token`:

| Candidate | Traffic | Share of `B_token` | Native path? | Max possible gain |
|---|---:|---:|---|---:|
| **`lm_head` BF16 → FP8** | 2.54 GB | **16.0 %** | yes | **+8.7 %** |
| `lm_head` BF16 → NVFP4 | 2.54 GB | 16.0 % | yes | +13.4 % |
| Embeddings BF16 → FP8 | 16 KB | 0.0001 % | yes | **+0.0 %** |
| Vision BF16 → NVFP4 | 0 | 0 % | yes | **+0.0 %** |
| Body NVFP4 → INT3 | 13.28 GB | 83.6 % | **no** | negative (Module 03) |

Working the top candidate through the throughput equation, holding achieved bandwidth at the measured 1296 GB/s:

```text
   lm_head 2.54 GB → 1.27 GB (FP8)     B_token: 15.88 → 14.61 GB
        1296 / 14.61  =  88.7 tok/s          (+8.7 %)

   lm_head 2.54 GB → 0.71 GB (NVFP4)   B_token: 15.88 → 14.05 GB
        1296 / 14.05  =  92.2 tok/s          (+13.0 %)
```

And the contrast that makes the point:

```text
   Embeddings are the SAME SIZE as lm_head (2.54 GB each).
   Quantizing embeddings :  +0.0 %  throughput,  −1.27 GB VRAM
   Quantizing lm_head    :  +8.7 %  throughput,  −1.27 GB VRAM

   Identical tensors. Identical capacity win. ~150,000× difference in traffic.
```

If you take one table from this course, take that one.

---

**5. The ledger changes when the config changes**

The same checkpoint has **different ledgers** under different serving configurations. This is why Class C exists.

**With speculation enabled (the DSpark build)**

Measured: **155.75 tok/s at acceptance length 2.886.** The target model now runs once per *accepted group*, not once per token:

```text
   target forward passes/s  =  155.75 / 2.886  =  53.97 passes/s
```

Each target pass streams the full `B_token`, and the MTP head runs `K` times per cycle to produce the draft:

| Draft depth `K` | Traffic (GB/s) | % of peak |
|---:|---:|---:|
| 2 | 946 | 52.8 % |
| 3 | 991 | 55.3 % |
| 4 | 1037 | 57.9 % |

```text
   traffic  =  53.97 × ( 15.88  +  K × 0.85 )   GB/s
                        └ target ┘   └ MTP ┘
```

**Bandwidth utilization *drops* from 72 % to ~55 % when speculation is enabled.** That is not a regression — it is exactly what speculation does: it amortizes one weight-read across ~2.9 emitted tokens. But it has a sharp consequence:

> In the speculative configuration the model is **no longer bandwidth-bound.** It sits at ~55 % of peak, which means further weight quantization has **diminishing returns**, and the dominant levers become **acceptance length** ([Module 10](Lecture-10.md)) and **kernel/launch efficiency** ([Module 03](Lecture-03.md)).

This is the kind of conclusion the ledger produces and intuition does not. The same model, same weights, same GPU — and the correct optimization strategy inverts depending on whether speculation is on.

**With an image in the request**

The vision tower moves from Class C to Class A for the prefill pass: `+0.92 GB` of traffic, once per image, not per token. It never enters the decode ledger. **Quantizing the vision tower improves image-prefill latency and VRAM, and never improves tok/s** — precisely as [Module 01](Lecture-01.md) claimed.

---

</details>

## 6. 构建分析器

手工算一次的账本只是笔记。能重复生成的账本才是工具。

```python
"""Per-token byte ledger for a safetensors checkpoint."""
import json, re
from collections import defaultdict
from safetensors import safe_open

BYTES_PER_PARAM = {  # see Module 02
    "BF16": 2.0, "F16": 2.0, "F32": 4.0,
    "F8_E4M3": 1.0, "F8_E5M2": 1.0,
    "NVFP4": 0.5625,   # 4 bits + 8-bit E4M3 scale per 16 elements
    "MXFP4": 0.5312,   # 4 bits + 8-bit E8M0 scale per 32 elements
}

# Class A = streamed every token. Class B/C/D never enter B_token by default.
CLASS_RULES = [
    (r"vision|visual|image_encoder",           "C_vision"),
    (r"mtp|draft|eagle|medusa",                "C_speculative"),
    (r"embed_tokens|wte|tok_embeddings",       "B_gathered"),
    (r"lm_head|output\.weight",                "A_streamed"),
    (r"layers\.\d+\.",                         "A_streamed"),
    (r"norm|ln_f",                             "A_streamed"),
]

def classify(name: str) -> str:
    for pattern, cls in CLASS_RULES:
        if re.search(pattern, name):
            return cls
    return "A_streamed"          # conservative default: assume it is on the hot path

def ledger(path: str, d_model: int, spec_enabled=False, draft_depth=0):
    by_class = defaultdict(float)
    with safe_open(path, framework="pt") as f:
        for name in f.keys():
            sl = f.get_slice(name)
            n = 1
            for d in sl.get_shape():
                n *= d
            bpp = BYTES_PER_PARAM.get(sl.get_dtype(), 2.0)
            by_class[classify(name)] += n * bpp

    resident = sum(by_class.values())
    b_token  = by_class["A_streamed"] + d_model * 2      # + one gathered embedding row
    if spec_enabled:
        b_token += by_class["C_speculative"] * draft_depth

    return {
        "resident_GB":  resident / 1e9,
        "resident_GiB": resident / 1024**3,
        "B_token_GB":   b_token / 1e9,
        "dead_weight_GB": (resident - by_class["A_streamed"]) / 1e9,
        "by_class_GB":  {k: v / 1e9 for k, v in sorted(by_class.items())},
    }

def predict_tps(b_token_GB: float, peak_BW_GBs=1792, efficiency=0.92):
    return peak_BW_GBs * efficiency / b_token_GB

def achieved_BW(b_token_GB: float, measured_tps: float):
    return b_token_GB * measured_tps          # GB/s — compare to peak, NOT to resident×tps
```

运行它，然后运行真正重要的对比：

```text
   predicted_tps  =  predict_tps(B_token)              ← what physics allows
   measured_tps   =  benchmark()                       ← what you get
   gap            =  predicted / measured

   gap ≈ 1.0   →  you are at the bandwidth wall. Quantize.
   gap > 1.2   →  you are NOT bandwidth-bound. Profile kernels first.
```

对于案例研究：`predicted = 103.9`、`measured = 81.6`、**`gap = 1.27`**。对 kernel 做性能剖析。

---

## 检查点

现在你应该能够：

1. 说出四个流量类别，并把任意张量归入其中一类。
2. 从字节清单重建模型的参数量和隐藏维度。
3. 构建 `B_token` 账本并正确计算实测带宽。
4. 解释为什么 embedding 和 `lm_head` —— 大小相同 —— 在流量上相差五个数量级。
5. 解释为什么启用推测 *降低* 带宽利用率，并且 *改变了哪个优化才是正确的*。
6. 用 `predicted/measured` 差距来决定是量化还是做性能剖析。

---

## 交付

需要你运行的检查点：分析器脚本、账本表（全部四个类别）、带差距的预测值与实测值对比，以及 **一句话结论，点明你的下一步行动**。如果差距高于约 1.2，结论必须是"性能剖析 kernel"，而不是"量化" —— 等你回来时，本课程余下的内容依然在这里。

---

## 时效说明

* **长期有效：** 四个流量类别、账本方法、字节到参数的重建、差距启发式。
* **2026 案例研究固定值：** 27.06 B 重建、`B_token ≈ 15.88 GB`、81.6 tok/s 下 72.3 % 实测带宽，以及推测下约 55 %，均来自 [课程索引](README.md) 中链接的已发布 NVFP4/DSpark 构建。
* **注意：** `BYTES_PER_PARAM` 必须跟踪你的 runtime 实际输出的格式。4-bit 格式的 Safetensors dtype 字符串在不同导出器之间各不相同 —— 请对照你自己的检查点核实，不要轻信表格。

---

**下一节：** [模块 05 —— 校准 →](Lecture-05.md)


<details>
<summary>English original</summary>

**6. Build the analyzer**

A ledger you compute by hand once is a note. A ledger you can regenerate is a tool.

```python
"""Per-token byte ledger for a safetensors checkpoint."""
import json, re
from collections import defaultdict
from safetensors import safe_open

BYTES_PER_PARAM = {  # see Module 02
    "BF16": 2.0, "F16": 2.0, "F32": 4.0,
    "F8_E4M3": 1.0, "F8_E5M2": 1.0,
    "NVFP4": 0.5625,   # 4 bits + 8-bit E4M3 scale per 16 elements
    "MXFP4": 0.5312,   # 4 bits + 8-bit E8M0 scale per 32 elements
}

# Class A = streamed every token. Class B/C/D never enter B_token by default.
CLASS_RULES = [
    (r"vision|visual|image_encoder",           "C_vision"),
    (r"mtp|draft|eagle|medusa",                "C_speculative"),
    (r"embed_tokens|wte|tok_embeddings",       "B_gathered"),
    (r"lm_head|output\.weight",                "A_streamed"),
    (r"layers\.\d+\.",                         "A_streamed"),
    (r"norm|ln_f",                             "A_streamed"),
]

def classify(name: str) -> str:
    for pattern, cls in CLASS_RULES:
        if re.search(pattern, name):
            return cls
    return "A_streamed"          # conservative default: assume it is on the hot path

def ledger(path: str, d_model: int, spec_enabled=False, draft_depth=0):
    by_class = defaultdict(float)
    with safe_open(path, framework="pt") as f:
        for name in f.keys():
            sl = f.get_slice(name)
            n = 1
            for d in sl.get_shape():
                n *= d
            bpp = BYTES_PER_PARAM.get(sl.get_dtype(), 2.0)
            by_class[classify(name)] += n * bpp

    resident = sum(by_class.values())
    b_token  = by_class["A_streamed"] + d_model * 2      # + one gathered embedding row
    if spec_enabled:
        b_token += by_class["C_speculative"] * draft_depth

    return {
        "resident_GB":  resident / 1e9,
        "resident_GiB": resident / 1024**3,
        "B_token_GB":   b_token / 1e9,
        "dead_weight_GB": (resident - by_class["A_streamed"]) / 1e9,
        "by_class_GB":  {k: v / 1e9 for k, v in sorted(by_class.items())},
    }

def predict_tps(b_token_GB: float, peak_BW_GBs=1792, efficiency=0.92):
    return peak_BW_GBs * efficiency / b_token_GB

def achieved_BW(b_token_GB: float, measured_tps: float):
    return b_token_GB * measured_tps          # GB/s — compare to peak, NOT to resident×tps
```

Run it, then run the comparison that matters:

```text
   predicted_tps  =  predict_tps(B_token)              ← what physics allows
   measured_tps   =  benchmark()                       ← what you get
   gap            =  predicted / measured

   gap ≈ 1.0   →  you are at the bandwidth wall. Quantize.
   gap > 1.2   →  you are NOT bandwidth-bound. Profile kernels first.
```

For the case study: `predicted = 103.9`, `measured = 81.6`, **`gap = 1.27`**. Profile the kernels.

---

**Checkpoint**

You should now be able to:

1. Name the four traffic classes and place any tensor into one.
2. Reconstruct a model's parameter count and hidden dimension from a byte inventory.
3. Build a `B_token` ledger and compute achieved bandwidth correctly.
4. Explain why embeddings and `lm_head` — identical in size — differ by five orders of magnitude in traffic.
5. Explain why enabling speculation *lowers* bandwidth utilization and *changes which optimization is correct*.
6. Use the `predicted/measured` gap to decide between quantizing and profiling.

---

**Ship it**

For a checkpoint you run: the analyzer script, the ledger table (all four classes), the predicted-vs-measured comparison with the gap, and **a one-sentence verdict naming your next action**. If the gap is above ~1.2, the verdict must be "profile kernels", not "quantize" — and the rest of this course will still be here when you come back.

---

**Current as of**

* **Timeless:** the four traffic classes, the ledger method, byte-to-parameter reconstruction, the gap heuristic.
* **2026 case-study pins:** the 27.06 B reconstruction, `B_token ≈ 15.88 GB`, 72.3 % achieved bandwidth at 81.6 tok/s, and ~55 % under speculation are derived from the published NVFP4/DSpark builds linked in the [course index](README.md).
* **Note:** `BYTES_PER_PARAM` must track the formats your runtime emits. Safetensors dtype strings for 4-bit formats vary between exporters — verify against your own checkpoint rather than trusting the table.

---

**Next:** [Module 05 — Calibration →](Lecture-05.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
