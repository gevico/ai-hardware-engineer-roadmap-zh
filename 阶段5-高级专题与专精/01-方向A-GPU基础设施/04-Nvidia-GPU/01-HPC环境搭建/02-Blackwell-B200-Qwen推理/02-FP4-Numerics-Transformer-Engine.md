---
title: 第 2 章：FP4 数值格式与面向 Qwen 推理的 Transformer Engine 2
description: 第 2 章：FP4 数值格式与面向 Qwen 推理的 Transformer Engine 2
---

# 第 2 章：FP4 数值格式与面向 Qwen 推理的 Transformer Engine 2

## 概述

Blackwell 的第五代张量核心原生执行带共享 block scale 的 **FP4**、**FP6** 和 **FP8** —— 即 OCP 标准中的 **MX（microscaling）** 格式族。这听起来很晦涩。实际含义是：FP16 下为 145 GB 的 Qwen2.5-72B 模型，**在 MX-FP4 下为 36 GB**，质量退化小到足以投入生产。

第二代 **Transformer Engine** 是让这一切无需手工调校逐张量 scale 即可运作的软件层。它在推理过程中为每个 block 动态选择 scale factor，为每个张量挑选合适的格式（FFN-up 用 FP4，FFN-down 和 V 用 FP6 或 FP8 等），并生成对应的 kernel 变体。

本章涵盖这些格式、该引擎，以及在 B200 上部署 Qwen 时所做的逐张量决策。

读完本章，应当能够：

* 按位布局和 block scale 区分 MX-FP4 / MX-FP6 / MX-FP8。
* 预测每种格式下 Qwen2.5-72B 的显存占用与带宽。
* 挑选类似 Q4_K_M 非对称性的混合精度 recipe（V 与 FFN-down 使用更高精度）。
* 读懂 Transformer Engine 2 校准日志，识别哪些张量回退到更高精度。

---

## 1. OCP Microscaling（MX）格式族

microscaled 格式用一个共享的 scale factor 存储**一个值的 block**。scale 为 FP8/UE8M0（无符号 8 位指数），block size 为 32 个元素。block 内每个元素按该格式的 element type 表示为小位宽 int 或 float。

```
MX block layout (32 elements):

  [ E ]  ← 1 byte shared scale (UE8M0, an 8-bit power of two)
  [ x0 | x1 | x2 | ... | x31 ]   ← 32 element values

The reconstructed value at slot i is:
    value_i = scale × element_i
```

element type 决定格式：

| 格式 | Element type | 元素位宽 | Block size | 有效 bits/value | 范围 |
|---|---|---|---|---|---|
| MX-FP8 (E5M2) | float 5e2m | 8 | 32 | ~8.25 | ±5.7×10⁴ |
| MX-FP8 (E4M3) | float 4e3m | 8 | 32 | ~8.25 | ±448 |
| MX-FP6 (E3M2) | float 3e2m | 6 | 32 | ~6.25 | ±28 |
| MX-FP6 (E2M3) | float 2e3m | 6 | 32 | ~6.25 | ±7.5 |
| MX-FP4 (E2M1) | float 2e1m | 4 | 32 | ~4.25 | ±6 |
| MX-INT8 | signed int | 8 | 32 | ~8.25 | ±127 |

“有效 bits/value”指元素位宽加上按 32 个元素摊分的每 block scale 开销：`bits + 8/32 = bits + 0.25`。

### 1.1 为什么需要 microscaling？

朴素的 INT4 或 FP4 有一个根本问题：训练后的 LLM 中，权重和激活值具有**很宽的动态范围**，但**block 内的局部范围很小**。全局看，一个张量可能跨越 ±50；局部看，连续 32 个权重组成的 block 可能全部落在 ±0.05 内，也可能全部落在 ±5 内。只用一个全局 scale，就会浪费位。

microscaling 让每个 32 元素的 block 拥有自己的 scale，因此每个 block 相对于其局部范围都能获得接近完整的 FP4 精度。代价：每个元素 0.25 bit 的 scale 开销。收益：在相同平均 bit 率下，质量相比均匀量化基线显著更优。

这与 ggml 的 K-quants（Q4_K、Q6_K）是同一个洞见 —— 分 block 的 scale 优于全局 scale —— 但在硬件层面标准化，并由张量核心原生执行。无需在寄存器中做即时反量化；Tensor Core 直接消费 MX 格式的位。

---

## 2. 各格式在 Qwen2.5-72B 上的开销

Qwen2.5-72B-Instruct 的逐张量细分（FP16 基线：约 145 GB）：

| 张量 | FP16 GB | MX-FP8 GB | MX-FP6 GB | MX-FP4 GB |
|---|---|---|---|---|
| `attn_q` (× 80) | 10.7 | 5.5 | 4.2 | 2.8 |
| `attn_k` (× 80) | 1.3 | 0.7 | 0.5 | 0.35 |
| `attn_v` (× 80) | 1.3 | 0.7 | 0.5 | 0.35 |
| `attn_o` (× 80) | 10.7 | 5.5 | 4.2 | 2.8 |
| `ffn_gate` (× 80) | 38.8 | 20.0 | 15.2 | 10.3 |
| `ffn_up` (× 80) | 38.8 | 20.0 | 15.2 | 10.3 |
| `ffn_down` (× 80) | 38.8 | 20.0 | 15.2 | 10.3 |
| Embedding（2 ×） | 4.98 | 2.57 | 1.95 | 1.32 |
| Norm、bias | ~0.05 | ~0.05 | ~0.05 | ~0.05 |
| **总计** | **~145 GB** | **~75 GB** | **~57 GB** | **~38.5 GB** |
| Roofline（性能上界模型）tok/s @ 8 TB/s | ~55 | ~106 | ~140 | **~210** |

带宽 × 格式倍数直接给出 decode（逐 token 生成阶段）tok/s 上限。在短上下文下，**单张 B200 上 MX-FP4 可达约 210 tok/s**（Qwen2.5-72B），与 8×H100 集群在 FP8 下的表现相当。


<details>
<summary>English original</summary>

**Chapter 2: FP4 Numerics and Transformer Engine 2 for Qwen Inference**

**Overview**

Blackwell's 5th-gen tensor cores natively execute **FP4**, **FP6**, and **FP8** with shared block scales — the **MX (microscaling)** family of formats from the OCP standard. That sounds esoteric. In practice it means: a Qwen2.5-72B model that was 145 GB at FP16 is **36 GB at MX-FP4**, with quality degradation small enough to ship in production.

The second-generation **Transformer Engine** is the software layer that makes this work without you hand-tuning per-tensor scales. It dynamically chooses scale factors per block during inference, picks the right format per tensor (FP4 for FFN-up, FP6 or FP8 for FFN-down and V, etc.), and emits the right kernel variants.

This chapter covers the formats, the engine, and the per-tensor decisions you make when deploying Qwen on B200.

By the end you should be able to:

* Distinguish MX-FP4 / MX-FP6 / MX-FP8 by bit layout and block scale.
* Predict Qwen2.5-72B memory and bandwidth for each format.
* Pick a mixed-precision recipe similar to the Q4_K_M asymmetry (V and FFN-down at higher precision).
* Read a Transformer Engine 2 calibration log and identify which tensors fell back to a higher precision.

---

**1. The OCP Microscaling (MX) Format Family**

A microscaled format stores a **block of values** with a single shared scale factor. The scale is FP8/UE8M0 (an unsigned 8-bit exponent), and the block size is 32 elements. Within the block, each element is a small int or float using the format's element type.

```
MX block layout (32 elements):

  [ E ]  ← 1 byte shared scale (UE8M0, an 8-bit power of two)
  [ x0 | x1 | x2 | ... | x31 ]   ← 32 element values

The reconstructed value at slot i is:
    value_i = scale × element_i
```

The element type determines the format:

| Format | Element type | Element bits | Block size | Effective bits/value | Range |
|---|---|---|---|---|---|
| MX-FP8 (E5M2) | float 5e2m | 8 | 32 | ~8.25 | ±5.7×10⁴ |
| MX-FP8 (E4M3) | float 4e3m | 8 | 32 | ~8.25 | ±448 |
| MX-FP6 (E3M2) | float 3e2m | 6 | 32 | ~6.25 | ±28 |
| MX-FP6 (E2M3) | float 2e3m | 6 | 32 | ~6.25 | ±7.5 |
| MX-FP4 (E2M1) | float 2e1m | 4 | 32 | ~4.25 | ±6 |
| MX-INT8 | signed int | 8 | 32 | ~8.25 | ±127 |

The "effective bits/value" is the element bits plus the per-block scale overhead amortized across 32 elements: `bits + 8/32 = bits + 0.25`.

**1.1 Why microscaling at all?**

Plain INT4 or FP4 has a fundamental problem: weights and activations in trained LLMs have **wide dynamic ranges** but **small block-local ranges**. Globally, a tensor might span ±50; locally, a block of 32 consecutive weights might all be in ±0.05 or all in ±5. With a single global scale, you waste bits.

Microscaling gives each block of 32 its own scale, so each block gets nearly full FP4 precision relative to its local range. The cost: 0.25 bits/element of scale overhead. The gain: dramatically better quality at the same average bit-rate vs uniform-quant baselines.

This is the same insight as ggml's K-quants (Q4_K, Q6_K) — block-wise scales beat global scales — but standardized at the hardware level and natively executed by tensor cores. No on-the-fly dequant in registers; the tensor core consumes the MX-format bits directly.

---

**2. What Each Format Costs for Qwen2.5-72B**

Per-tensor breakdown for a Qwen2.5-72B-Instruct (FP16 baseline: ~145 GB):

| Tensor | FP16 GB | MX-FP8 GB | MX-FP6 GB | MX-FP4 GB |
|---|---|---|---|---|
| `attn_q` (× 80) | 10.7 | 5.5 | 4.2 | 2.8 |
| `attn_k` (× 80) | 1.3 | 0.7 | 0.5 | 0.35 |
| `attn_v` (× 80) | 1.3 | 0.7 | 0.5 | 0.35 |
| `attn_o` (× 80) | 10.7 | 5.5 | 4.2 | 2.8 |
| `ffn_gate` (× 80) | 38.8 | 20.0 | 15.2 | 10.3 |
| `ffn_up` (× 80) | 38.8 | 20.0 | 15.2 | 10.3 |
| `ffn_down` (× 80) | 38.8 | 20.0 | 15.2 | 10.3 |
| Embeddings (2 ×) | 4.98 | 2.57 | 1.95 | 1.32 |
| Norms, biases | ~0.05 | ~0.05 | ~0.05 | ~0.05 |
| **Total** | **~145 GB** | **~75 GB** | **~57 GB** | **~38.5 GB** |
| Roofline tok/s @ 8 TB/s | ~55 | ~106 | ~140 | **~210** |

The bandwidth × format multiplier directly gives you the decode tok/s ceiling. **MX-FP4 hits ~210 tok/s on a single B200** for Qwen2.5-72B at short context — competitive with what an 8×H100 cluster delivers at FP8.

</details>

### 2.1 混合精度——非对称 recipe

与 K-quants 一样（Edge AI Qwen 系列第 2 讲），并非每个张量都值得相同的精度。这条经验规则可以干净地迁移到 MX 格式：

| 张量分组 | 推荐格式 | 理由 |
|---|---|---|
| `attn_q`、`attn_k` | MX-FP4 | 后接 softmax——Q/K 的小扰动会被平滑掉 |
| `attn_v` | MX-FP6 或 MX-FP8 | 直接内容通路；对质量敏感 |
| `attn_o` | MX-FP4 | 退化可接受；参数量大 |
| `ffn_gate`、`ffn_up` | MX-FP4 | 最大的张量；带宽收益最大 |
| `ffn_down` | MX-FP6 或 MX-FP8 | 对质量最敏感；会汇入残差 |
| Embeddings | MX-FP6 | 覆盖整个词表；影响每个 token |
| Norms、biases | FP32 | 极小；没有理由不保留 |
| KV cache | MX-FP8（或为保险用 FP16） | 每个 decode（逐 token 生成阶段）出的 token 都要读取；INT8 同样可行 |

这套「除 V、FFN-down 和 embeddings 之外全都用 FP4」的 recipe，是 `Q4_K_M` 在 Blackwell 上的对应方案。显存开销落在 **42–45 GB** 左右，而不是纯 FP4 的 38.5 GB，质量明显更好。相对 FP16 基线的预期 MMLU 下降：<0.5 pts；相对纯 MX-FP4：回升约 0.4 pts。

---

## 3. Transformer Engine 2 流水线

Transformer Engine（TE）是 NVIDIA 的库，负责 FP8/FP4 混合推理的格式选择、scale factor 管理与 kernel 派发。版本 2（感知 Blackwell）自动处理 MX 格式与 per-block 缩放。

### 3.1 TE2 在加载时做什么

```
GGUF/safetensors (FP16) ─►  TE2 quantizer
                              │
                              ├─ Determine per-tensor format from recipe
                              ├─ Run calibration (a few prompts through the model)
                              ├─ Compute initial per-block scales
                              ├─ Pack into MX layout
                              └─ Write Blackwell-resident weights
```

校准 pass 会在数百条 prompt 上收集 per-block 的 max-abs 值，并用它设定初始 scale factor。对于纯权重量化（推理的默认方式），校准**很快**——一两分钟。

### 3.2 TE2 在 runtime 做什么

```
Per layer per token:
  1. Read MX-FP4/FP6/FP8 weights from HBM (5th-gen tensor cores handle dequant internally)
  2. Compute activations in FP16 or BF16 (the high-precision "accumulator" path)
  3. Optionally quantize activations on-the-fly using a learned scale (for KV cache writes)
  4. Detect overflow/underflow and trigger fallback if a block saturates
```

回退的情况是真实存在的：某个 block 的数值在 runtime 暴涨时，会在那一步被透明地提升为 FP8 或 FP16。TE2 的日志会显示提升事件；如果某个张量上出现很多这类事件，说明 recipe 选错了（通常元凶是 V 或 FFN-down 用了 FP4）。

### 3.3 一份典型的 TE2 校准日志

```
[TE2] Calibrating Qwen2.5-72B with recipe='qwen-mx-mixed'
[TE2] Layer 0: q=MX-FP4, k=MX-FP4, v=MX-FP6, o=MX-FP4
[TE2] Layer 0: gate=MX-FP4, up=MX-FP4, down=MX-FP6
[TE2] Layer 12: WARNING — v block 7/128 saturated, promoting to MX-FP8
[TE2] Layer 47: WARNING — down block 23/231 saturated, promoting to MX-FP8
[TE2] Calibration complete: 78 of 800 weight blocks promoted (0.0098%)
[TE2] Final memory: 42.7 GB (vs 36.5 GB pure FP4, vs 145 GB FP16)
[TE2] Bandwidth/token estimate: 42.7 GB → tok/s ceiling 187
```

0.01% 的提升率是健康的。如果提升率超过 1%，说明该 recipe 对这个模型过于激进。

---

## 4. MX 格式下的 KV Cache

KV cache 每个 token 读一次，*每个* token 都是。量化它的收益与读取它的频率成正比。

| KV 格式 | 每 layer 每 token 的字节数（Qwen2.5-72B GQA） | KV @ 32k ctx |
|---|---|---|
| FP16 | 4096 | 10.0 GB |
| MX-FP8 | ~2080 | 5.1 GB |
| MX-FP6 | ~1568 | 3.8 GB |
| MX-FP4 | ~1056 | 2.6 GB |
| 带 per-channel 校准的 INT4 | ~1024 | 2.5 GB |

质量方面：MX-FP8 KV 基本是免费的（Qwen2.5-72B 上困惑度下降 < 0.05）。MX-FP4 KV 在超过 8k 上下文时仍可用，但超过约 32k 后在检索任务上出现退化。安全的生产默认值是 **MX-FP8 KV**——相对 FP16 把带宽减半，且没有可测量的质量代价。

实现细节：KV 是在 RoPE **之后**量化，而不是之前。存储的是旋转后的 K，因此 per-block 的 scale 必须容纳旋转带来的量级。TE2 会自动处理这一点；手写的 KV 量化 runtime 常常弄错，产生难以察觉的长文本生成退化。

---


<details>
<summary>English original</summary>

**2.1 Mixed precision — the asymmetric recipe**

As with K-quants (Lecture 2 of the Edge AI Qwen series), not every tensor deserves the same precision. The empirical rule transfers cleanly to MX formats:

| Tensor group | Recommended format | Reasoning |
|---|---|---|
| `attn_q`, `attn_k` | MX-FP4 | Followed by softmax — small Q/K perturbations get smoothed |
| `attn_v` | MX-FP6 or MX-FP8 | Direct content path; quality-sensitive |
| `attn_o` | MX-FP4 | Acceptable degradation; large parameter count |
| `ffn_gate`, `ffn_up` | MX-FP4 | Largest tensors; biggest bandwidth win |
| `ffn_down` | MX-FP6 or MX-FP8 | Most quality-sensitive; folds into residual |
| Embeddings | MX-FP6 | Vocabulary-wide; affects every token |
| Norms, biases | FP32 | Tiny; no reason not to keep |
| KV cache | MX-FP8 (or FP16 for safety) | Read on every decoded token; INT8 also viable |

This "FP4 everywhere except V, FFN-down, and embeddings" recipe is the Blackwell counterpart of `Q4_K_M`. Memory cost lands around **42–45 GB** instead of a pure-FP4 38.5 GB, with notably better quality. Expected MMLU drop vs FP16 baseline: <0.5 pts; vs pure MX-FP4: ~0.4 pts recovery.

---

**3. The Transformer Engine 2 Pipeline**

Transformer Engine (TE) is the NVIDIA library that owns format selection, scale-factor management, and kernel dispatch for FP8/FP4-mixed inference. Version 2 (Blackwell-aware) handles MX formats and per-block scaling automatically.

**3.1 What TE2 does at load time**

```
GGUF/safetensors (FP16) ─►  TE2 quantizer
                              │
                              ├─ Determine per-tensor format from recipe
                              ├─ Run calibration (a few prompts through the model)
                              ├─ Compute initial per-block scales
                              ├─ Pack into MX layout
                              └─ Write Blackwell-resident weights
```

The calibration pass collects per-block max-abs values across a few hundred prompts, which it uses to set initial scale factors. For pure weight-only quantization (the default for inference) the calibration is **fast** — a minute or two.

**3.2 What TE2 does at runtime**

```
Per layer per token:
  1. Read MX-FP4/FP6/FP8 weights from HBM (5th-gen tensor cores handle dequant internally)
  2. Compute activations in FP16 or BF16 (the high-precision "accumulator" path)
  3. Optionally quantize activations on-the-fly using a learned scale (for KV cache writes)
  4. Detect overflow/underflow and trigger fallback if a block saturates
```

The fallback case is real: a block whose values blow up at runtime gets transparently promoted to FP8 or FP16 for that step. The TE2 log shows promotion events; if you see lots of them on a specific tensor, your recipe is wrong (V or FFN-down at FP4 is the usual culprit).

**3.3 A typical TE2 calibration log**

```
[TE2] Calibrating Qwen2.5-72B with recipe='qwen-mx-mixed'
[TE2] Layer 0: q=MX-FP4, k=MX-FP4, v=MX-FP6, o=MX-FP4
[TE2] Layer 0: gate=MX-FP4, up=MX-FP4, down=MX-FP6
[TE2] Layer 12: WARNING — v block 7/128 saturated, promoting to MX-FP8
[TE2] Layer 47: WARNING — down block 23/231 saturated, promoting to MX-FP8
[TE2] Calibration complete: 78 of 800 weight blocks promoted (0.0098%)
[TE2] Final memory: 42.7 GB (vs 36.5 GB pure FP4, vs 145 GB FP16)
[TE2] Bandwidth/token estimate: 42.7 GB → tok/s ceiling 187
```

The 0.01% promotion rate is healthy. If you see promotion rates above 1%, the recipe is too aggressive for this model.

---

**4. KV Cache in MX Formats**

The KV cache is read once per token, *every* token. Quantizing it pays off proportionally to how often you read it.

| KV format | Bytes per token per layer (Qwen2.5-72B GQA) | KV @ 32k ctx |
|---|---|---|
| FP16 | 4096 | 10.0 GB |
| MX-FP8 | ~2080 | 5.1 GB |
| MX-FP6 | ~1568 | 3.8 GB |
| MX-FP4 | ~1056 | 2.6 GB |
| INT4 with per-channel calibration | ~1024 | 2.5 GB |

Quality-wise: MX-FP8 KV is essentially free (< 0.05 perplexity drop on Qwen2.5-72B). MX-FP4 KV is usable past 8k context but shows degradation in retrieval tasks past ~32k. The safe production default is **MX-FP8 KV** — cuts bandwidth in half vs FP16 with no measurable quality cost.

Implementation detail: KV is quantized **after** RoPE, not before. The rotated K is what gets stored, so the per-block scale must accommodate the rotation's magnitude. TE2 handles this automatically; hand-rolled KV-quant runtimes often get this wrong and produce subtle long-generation degradation.

---

</details>

## 5. 与 Ggml K-Quants 和 AWQ 的对比

MX 格式与你从 Edge 已经熟悉的那些格式相比，表现如何？

| 格式 | Bits/weight（有效） | 硬件原生？ | 每块 scale | 生产支持 |
|---|---|---|---|---|
| Q4_K_M（ggml） | ~4.5 | 否 —— 寄存器内 dequant | 是（256 元素 superblock） | llama.cpp |
| AWQ-INT4 g128 | ~4.25 | Marlin kernel | 每 128 通道 | vLLM、TRT-LLM |
| GPTQ-INT4 g128 | ~4.25 | Marlin kernel | 每 128 通道 | vLLM、exllamav2 |
| **MX-FP4** | **~4.25** | **是 —— 第五代 Tensor Core 原生** | **每 32 元素** | **TRT-LLM 0.20+、TE2** |
| **MX-FP8** | **~8.25** | **是 —— Tensor Core 原生** | **每 32 元素** | **TRT-LLM 0.20+、TE2** |

MX 的决定性差异在于**硬件原生执行**。K-quants 和 AWQ 需要自定义 CUDA kernel，先把 block 反量化到寄存器，再送入矩阵乘。MX 格式由第五代 Tensor Core 的 MMA 指令**直接**消费 —— 不存在软件 dequant 步骤。

这一点重要，原因有二：

1. **带宽效率** —— Tensor Core 从 HBM 读取 MX-FP4 字节，这就是全部 DRAM 开销。没有第二遍。
2. **计算吞吐** —— B200 上 MX-FP4 的峰值吞吐约 2.25 PFLOPS dense。经 Marlin 的 AWQ-INT4 最高只有其约一半，因为该 kernel 必须在 Tensor Core 路径之外做 dequant 工作。

对 Blackwell 上的 Qwen 工作负载，recipe 是：**MX-FP4 配 TE2 混合精度 auto-recipe → MX-FP8 KV cache**。仅在 MX 不受支持的平台（Jetson、AMD）上才用 Q4_K_M。

---

## 6. 质量表现 —— 到底掉了什么？

来自 2026 年初公开的 Qwen2.5-72B-Instruct benchmark：

| 配置 | MMLU | IFEval | GSM8K | HumanEval | MT-Bench |
|---|---|---|---|---|---|
| BF16 基线 | 84.2 | 87.1 | 91.5 | 75.6 | 9.04 |
| MX-FP8（全部） | 84.1 | 86.9 | 91.4 | 75.5 | 9.02 |
| MX-FP6 混合 | 83.9 | 86.5 | 91.0 | 75.1 | 8.98 |
| **MX-FP4 混合（V/down=FP8）** | **83.8** | **86.2** | **90.8** | **74.7** | **8.94** |
| MX-FP4 纯 FP4（不混合） | 83.0 | 84.1 | 89.5 | 73.1 | 8.71 |

混合精度的 MX-FP4 recipe 是最佳平衡点 —— MMLU 差距低于 0.5 分，任何 benchmark 差距低于 1 分，MT-Bench 差距低于 0.1。**最重要的单一参数是把 V 和 FFN-down 保持在 FP6 或 FP8。** 纯 FP4 掉得明显；混合 recipe 已达生产可用。

作为对比，同一模型上 Q4_K_M 掉约 0.4 MMLU、约 1.2 IFEval —— 在相同有效 bit 率下略差于 MX-FP4 混合，且没有硬件原生执行路径。

---

## 7. 部署 —— 从 `transformers` 到 TRT-LLM

Qwen2.5-72B → B200 生产的典型转换流水线：

```bash
# Step 1: download FP16/BF16 weights
huggingface-cli download Qwen/Qwen2.5-72B-Instruct --local-dir ./qwen72b

# Step 2: convert via TRT-LLM's quantizer (uses TE2 internally)
python -m tensorrt_llm.quantization.quantize \
    --model_dir ./qwen72b \
    --output_dir ./qwen72b-mx-fp4 \
    --dtype bf16 \
    --qformat mx_fp4_mixed \
    --calib_dataset openassistant-en-zh \
    --calib_size 256

# Step 3: build the TensorRT engine for B200
trtllm-build --checkpoint_dir ./qwen72b-mx-fp4 \
             --output_dir ./qwen72b-mx-fp4-engine \
             --gemm_plugin mx_fp4 \
             --gpt_attention_plugin auto \
             --max_batch_size 64 \
             --max_input_len 32768 \
             --max_seq_len 65536 \
             --kv_cache_type mx_fp8 \
             --use_paged_context_fmha
```

构建产物是面向 `sm_100`（Blackwell）的 TensorRT engine。它约 42 GB，加载到单颗 B200 die 约需 3 秒。

---

## 8. MX-FP4 不适用的情况

值得了解的真实失效模式：

* **长上下文检索** —— >50k 上下文下的 Needle-in-haystack 开始出现 FP4 退化。对检索密集型工作负载，把 V 和 FFN-down 提升到 FP8。
* **代码生成** —— 纯 FP4 下 HumanEval pass@1 掉约 2 分，混合下掉约 0.5 分。对助手类应用可接受；对高风险代码审查流水线可能不行。
* **多语言** —— 小语种质量下降比英文更多。若服务全球流量，请在多语言混合数据上做校准。
* **长结构化输出**（JSON、工具调用） —— schema 遵循度出现细微退化。一些生产部署因此把 `attn_o` 保持在 FP6。
* **MoE（混合专家模型）活跃专家布线** —— 对 MoE 版 Qwen，router 权重很小且对质量敏感。router 一律保持在 FP16/BF16。

---


<details>
<summary>English original</summary>

**5. Comparison vs Ggml K-Quants and AWQ**

How do MX formats stack up against the formats you already know from Edge?

| Format | Bits/weight (effective) | Hardware-native? | Per-block scale | Production support |
|---|---|---|---|---|
| Q4_K_M (ggml) | ~4.5 | No — dequant in registers | Yes (256-element superblock) | llama.cpp |
| AWQ-INT4 g128 | ~4.25 | Marlin kernel | Per-128 channel | vLLM, TRT-LLM |
| GPTQ-INT4 g128 | ~4.25 | Marlin kernel | Per-128 channel | vLLM, exllamav2 |
| **MX-FP4** | **~4.25** | **Yes — 5th-gen tensor core native** | **Per-32 element** | **TRT-LLM 0.20+, TE2** |
| **MX-FP8** | **~8.25** | **Yes — tensor core native** | **Per-32 element** | **TRT-LLM 0.20+, TE2** |

The decisive differentiator for MX is **hardware-native execution**. K-quants and AWQ require a custom CUDA kernel that dequantizes blocks into registers before feeding the matmul. MX formats are consumed **directly** by the 5th-gen tensor core's MMA instruction — there's no software dequant step.

This matters for two reasons:

1. **Bandwidth efficiency** — the tensor core reads MX-FP4 bytes from HBM, and that's the full DRAM cost. No second pass.
2. **Compute throughput** — peak MX-FP4 throughput on B200 is ~2.25 PFLOPS dense. AWQ-INT4 via Marlin maxes out at ~half that because the kernel has to do dequant work outside the tensor core path.

For Qwen workloads on Blackwell, the recipe is: **MX-FP4 with TE2 mixed-precision auto-recipe → MX-FP8 KV cache**. Use Q4_K_M only on platforms (Jetson, AMD) where MX isn't supported.

---

**6. The Quality Story — What Actually Drops?**

From early-2026 published benchmarks on Qwen2.5-72B-Instruct:

| Config | MMLU | IFEval | GSM8K | HumanEval | MT-Bench |
|---|---|---|---|---|---|
| BF16 baseline | 84.2 | 87.1 | 91.5 | 75.6 | 9.04 |
| MX-FP8 (all) | 84.1 | 86.9 | 91.4 | 75.5 | 9.02 |
| MX-FP6 mixed | 83.9 | 86.5 | 91.0 | 75.1 | 8.98 |
| **MX-FP4 mixed (V/down=FP8)** | **83.8** | **86.2** | **90.8** | **74.7** | **8.94** |
| MX-FP4 pure (no mixing) | 83.0 | 84.1 | 89.5 | 73.1 | 8.71 |

The mixed-precision MX-FP4 recipe is the sweet spot — under 0.5 pts off MMLU, under 1 pt off any benchmark, less than 0.1 off MT-Bench. **The single most important parameter is keeping V and FFN-down at FP6 or FP8.** Pure FP4 drops noticeably; the mixed recipe is production-ready.

For comparison, Q4_K_M on the same model drops ~0.4 MMLU and ~1.2 IFEval — slightly worse than MX-FP4 mixed at the same effective bit-rate, with no hardware-native execution path.

---

**7. Deployment — From `transformers` to TRT-LLM**

A typical conversion pipeline for Qwen2.5-72B → B200 production:

```bash
# Step 1: download FP16/BF16 weights
huggingface-cli download Qwen/Qwen2.5-72B-Instruct --local-dir ./qwen72b

# Step 2: convert via TRT-LLM's quantizer (uses TE2 internally)
python -m tensorrt_llm.quantization.quantize \
    --model_dir ./qwen72b \
    --output_dir ./qwen72b-mx-fp4 \
    --dtype bf16 \
    --qformat mx_fp4_mixed \
    --calib_dataset openassistant-en-zh \
    --calib_size 256

# Step 3: build the TensorRT engine for B200
trtllm-build --checkpoint_dir ./qwen72b-mx-fp4 \
             --output_dir ./qwen72b-mx-fp4-engine \
             --gemm_plugin mx_fp4 \
             --gpt_attention_plugin auto \
             --max_batch_size 64 \
             --max_input_len 32768 \
             --max_seq_len 65536 \
             --kv_cache_type mx_fp8 \
             --use_paged_context_fmha
```

The build produces a TensorRT engine targeting `sm_100` (Blackwell). It ships ~42 GB and loads to one B200 die in ~3 seconds.

---

**8. When MX-FP4 Doesn't Work**

Real failure modes worth knowing:

* **Long-context retrieval** — Needle-in-haystack at >50k context starts showing FP4 degradation. Promote V and FFN-down to FP8 for retrieval-heavy workloads.
* **Code generation** — HumanEval pass@1 drops ~2 pts at pure FP4, ~0.5 pts at mixed. Acceptable for assistants; might not be for high-stakes code review pipelines.
* **Multilingual** — minority-language quality drops more than English. Calibrate on a multilingual mix if you serve global traffic.
* **Long structured output** (JSON, tool calls) — schema adherence degrades subtly. Some production deployments keep `attn_o` at FP6 for this reason.
* **MoE active expert routing** — for MoE Qwen variants, the router weights are tiny and quality-sensitive. Always keep router at FP16/BF16.

---

</details>

## 关键要点

| 要点 | 为何重要 |
|---|---|
| MX-FP4 在 B200 上是硬件原生 —— 无需软件反量化 | 同时具备带宽效率与计算效率 |
| 每 32 元素分块 scale 优于全局 scale | 与 Q4_K_M 相同的洞见，在硬件层面标准化 |
| 混合精度 recipe（V 与 FFN-down 用 FP6/FP8）必不可少 | 纯 FP4 在各 benchmark 上损失约 1 个点；混合精度损失 <0.5 |
| TE2 自动管理 scale 并派发 kernel | 推理工程师很少编写逐张量 scale 代码 |
| MX-FP8 KV cache 在质量上基本免费 | 上下文 >4k 时默认使用 |
| 在非 Blackwell 平台上 AWQ/GPTQ 仍是正确选择 | MX 需要第 5 代 tensor core |
| 质量下降集中在长上下文检索与代码 | 在真实工作负载上验证，而不只是 MMLU |

---

## 资源

* **[OCP Microscaling Formats v1.0 Spec](https://www.opencompute.org/documents/ocp-microscaling-formats-mx-v1-0-spec-final-pdf)：** MX 格式的权威参考。
* **[NVIDIA Transformer Engine documentation](https://docs.nvidia.com/deeplearning/transformer-engine/)：** TE2 API 与 recipe 系统。
* **[TensorRT-LLM Quantization Guide](https://nvidia.github.io/TensorRT-LLM/architecture/quantization.html)：** 端到端 MX-FP4 工作流。
* **["Microscaling Data Formats for Deep Learning" (2023)](https://arxiv.org/abs/2310.10537)：** OCP 标准背后的研究。
* **[Chapter 1 — Blackwell Architecture](01-Blackwell-Architecture.md)：** 硬件基础。
* **[Chapter 3 — Single-B200 Qwen Inference](03-Single-B200-Qwen-Inference.md)：** 在部署中应用这些数值格式。
* **[Qwen Inference Optimization — Lecture 2 (Q4_K_M etc.)](../../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md)：** 边缘量化的对应内容。


<details>
<summary>English original</summary>

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| MX-FP4 is hardware-native on B200 — no software dequant | Bandwidth-efficient and compute-efficient at the same time |
| Per-32-element block scales beat global scales | Same insight as Q4_K_M, standardized at hardware level |
| Mixed-precision recipe (V and FFN-down at FP6/FP8) is essential | Pure FP4 loses ~1 pt across benchmarks; mixed loses <0.5 |
| TE2 manages scales and dispatches kernels automatically | Inference engineers rarely write per-tensor scale code |
| MX-FP8 KV cache is essentially free quality-wise | Use it by default for context >4k |
| AWQ/GPTQ remain the right choice off-Blackwell | MX requires 5th-gen tensor cores |
| Quality regressions concentrate in long-context retrieval and code | Validate on your actual workload, not just MMLU |

---

**Resources**

* **[OCP Microscaling Formats v1.0 Spec](https://www.opencompute.org/documents/ocp-microscaling-formats-mx-v1-0-spec-final-pdf):** The authoritative MX format reference.
* **[NVIDIA Transformer Engine documentation](https://docs.nvidia.com/deeplearning/transformer-engine/):** The TE2 API and recipe system.
* **[TensorRT-LLM Quantization Guide](https://nvidia.github.io/TensorRT-LLM/architecture/quantization.html):** End-to-end MX-FP4 workflow.
* **["Microscaling Data Formats for Deep Learning" (2023)](https://arxiv.org/abs/2310.10537):** The research backing the OCP standard.
* **[Chapter 1 — Blackwell Architecture](01-Blackwell-Architecture.md):** The hardware foundation.
* **[Chapter 3 — Single-B200 Qwen Inference](03-Single-B200-Qwen-Inference.md):** Applying these numerics in deployment.
* **[Qwen Inference Optimization — Lecture 2 (Q4_K_M etc.)](../../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md):** The edge-quantization counterpart.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Blackwell-B200-Qwen-Inference/02-FP4-Numerics-Transformer-Engine.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Blackwell-B200-Qwen-Inference/02-FP4-Numerics-Transformer-Engine.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
