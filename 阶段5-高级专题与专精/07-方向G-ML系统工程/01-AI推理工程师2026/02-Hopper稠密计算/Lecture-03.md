---
title: 第 2 部分 · Lecture 03 —— 量化 Llama 3.3 70B 与 Qwen 2.5 72B
description: 第 2 部分 · Lecture 03 —— 量化 Llama 3.3 70B 与 Qwen 2.5 72B
---

# 第 2 部分 · Lecture 03 —— 量化 Llama 3.3 70B 与 Qwen 2.5 72B

## 概述

第 1 部分 Lecture 04 在概念层面介绍了精度栈和主要量化方法。本讲将其应用于 Hopper 硬件上的两个锚定模型，并产出具体、可论证的 recipe。

Llama 3.3 70B ↔ Qwen 2.5 72B 这一对在此处格外有用，因为：

* 二者架构相同（Lecture 01），因此适用相同方法。
* 二者在一处已发表的异常上不同（arXiv:2408.15301）——Llama-3-70B 的激活值离群敏感性——因此可以走一遍真实的「此方法在此处有效、在彼处无效」案例。
* 二者部署广泛，因此主要 benchmark 充足。

本讲涵盖：

1. 2026 年各模型出货的精度 recipe。
2. **Llama 3.3 70B 与 Qwen 2.5 72B 上的 AWQ** —— 校准数据选择、分组大小、验证。
3. **GPTQ vs AWQ** —— 各自何时胜出及原因。
4. **Llama-3-70B 的 W8A8 异常** —— 是什么，QuaRot/SpinQuant 如何修复。
5. **Hopper 上的 FP8** —— TensorRT-LLM 的路径及如何验证精度一致性。
6. **KV cache 量化** —— FP8 KV 逐头缩放，何时 INT4 KV 不安全。
7. **完整精度一致性验证方法论** —— 评测集、预算、失效模式。

到本讲结束时，应能为任一模型在 H100 或 H200 上产出一份有据可依、精度一致性已验证的精度 recipe，将其记录成文并发布。

---

## 1. 2026 年生产 recipe

起点，按精度激进程度递增排列：

| Recipe | Llama 3.3 70B | Qwen 2.5 72B | 硬件适配 |
|--------|---------------|----------------|--------------|
| **安全基线** | FP16/BF16 weights/activations/KV | 同左 | 4-8× H100（TP）或 1× H200 |
| **FP8 吞吐** | FP8 E4M3 weights+activations，FP16 KV（TRT-LLM） | 同左 | 1× H200 或 2× H100 NVL |
| **FP8 全量** | FP8 weights+activations+E5M2 KV | 同左 | 1× H200（最佳单卡） |
| **AWQ-INT4 主流** | AWQ-INT4 group-128 weights，FP16 activations，FP16 KV | 同左（已验证） | 1× H100 或 1× H200（高批） |
| **AWQ-INT4 + FP8 KV** | AWQ-INT4 weights，FP16 activations，FP8 KV | 同左 | 1× H200 长上下文 |
| **W4A8（进阶）** | QuaRot/SpinQuant W4A8 | QuaRot/SpinQuant W4A8 | 1× H100/H200 |
| **W4A4（研究）** | SpinQuant W4A4（精度一致性视 workload 待定） | 同左 | Hopper 边缘；Blackwell 原生（第 3 部分） |

2026 年生产环境中为这两个模型出货的 recipe 通常是以下之一：**FP8 全量（TRT-LLM，最大吞吐）** 或 **AWQ-INT4（最大成本效率）**。其余 recipe 是边缘处的取舍。

---

## 2. Llama 3.3 70B 与 Qwen 2.5 72B 上的 AWQ

AWQ（[arXiv:2306.00978](https://arxiv.org/abs/2306.00978)）是 INT4 的默认起点。流程：

```text
1. Load FP16/BF16 reference model
2. Pick calibration data — 256–1024 prompts representative of deployment
3. For each Linear layer:
   a. Compute per-channel activation magnitudes from calibration data
   b. Identify top ~1% channels with largest activation
   c. Compute per-channel scale factor s such that:
      W' = W × s    (the important channels become larger in weight space)
      X' = X / s    (and proportionally smaller in activation space)
   d. Quantize W' to INT4 with group-128 scaling
   e. Keep scale factor for the dequant kernel
4. Output a quantized model + scale factors
```

关键洞察：**预缩放权重将「重要」行移入 INT4 可表示范围的高幅值区域**，此处量化更精细。激活值侧吸收逆缩放而不损失精度，因为激活值本来就以 FP16 计算。

### 2.1 校准数据选择

AWQ 中最被低估的决策。校准集应匹配**部署分布**：

* **英文聊天产品：** 来自 WikiText 或自有产品日志的 512+ prompt。
* **多语言产品（Qwen 2.5 72B + 中文）** ：按与生产流量相同的比例纳入中文 prompt。仅用英文校准数据校准 Qwen 2.5 72B，会在中文评测上产生明显更差的精度一致性。
* **agent / 工具调用产品：** 纳入工具调用示例。不纳入这些示例进行校准，会产出一种对稀有动作 token 系统性不利的权重量化——正是 [BFCL lecture](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) 指出的 bug。

**规则：** 如果无法论证校准集的代表性，就无法论证量化。

### 2.2 分组大小

`group_size=128` 是默认值。其含义是每 128 个连续权重共享一个缩放因子。

* 更小的分组（64 或 32）→ 更细粒度的缩放、精度一致性略好、存储略大且 kernel 略慢。
* 更大的分组（256，「逐通道」）→ 更粗、更快、精度一致性更低。

对于 70B 级模型，**group=128 是通用默认值**。仅当某个特定维度无法通过精度一致性时才降到 64。


<details>
<summary>English original</summary>

**Part 2 · Lecture 03 — Quantizing Llama 3.3 70B and Qwen 2.5 72B**

**Overview**

Part 1 Lecture 04 introduced the precision stack and the major quantization methods at the concept level. This lecture applies them to the two anchor models on Hopper hardware and produces concrete, defended recipes.

The Llama 3.3 70B ↔ Qwen 2.5 72B pair is uniquely useful here because:

* They share architecture (Lecture 01) so the same methods apply.
* They differ in one published anomaly (arXiv:2408.15301) — the Llama-3-70B activation-outlier sensitivity — so we get to walk a real "this method works here but not there" case.
* Both are widely-deployed, so primary benchmarks are abundant.

This lecture covers:

1. The precision recipes that ship for each model in 2026.
2. **AWQ on Llama 3.3 70B and Qwen 2.5 72B** — calibration data choice, group size, validation.
3. **GPTQ vs AWQ** — when each wins and why.
4. **The Llama-3-70B W8A8 anomaly** — what it is, how QuaRot/SpinQuant fix it.
5. **FP8 on Hopper** — TensorRT-LLM's path and how to validate parity.
6. **KV cache quantization** — FP8 KV per-head scaling, when INT4 KV is unsafe.
7. **The full parity validation methodology** — eval sets, budgets, failure modes.

By the end you should be able to produce a defended, parity-verified precision recipe for either model on H100 or H200, document it, and ship.

---

**1. The 2026 production recipes**

Starting points, in order of increasing precision aggression:

| Recipe | Llama 3.3 70B | Qwen 2.5 72B | Hardware fit |
|--------|---------------|----------------|--------------|
| **Safe baseline** | FP16/BF16 weights/activations/KV | same | 4-8× H100 (TP) or 1× H200 |
| **FP8 throughput** | FP8 E4M3 weights+activations, FP16 KV (TRT-LLM) | same | 1× H200 or 2× H100 NVL |
| **FP8 full** | FP8 weights+activations+E5M2 KV | same | 1× H200 (best single-GPU) |
| **AWQ-INT4 mainstream** | AWQ-INT4 group-128 weights, FP16 activations, FP16 KV | same (validated) | 1× H100 or 1× H200 (high batch) |
| **AWQ-INT4 + FP8 KV** | AWQ-INT4 weights, FP16 activations, FP8 KV | same | 1× H200 long context |
| **W4A8 (advanced)** | QuaRot/SpinQuant W4A8 | QuaRot/SpinQuant W4A8 | 1× H100/H200 |
| **W4A4 (research)** | SpinQuant W4A4 (parity TBD per workload) | same | Hopper edge; Blackwell native (Part 3) |

The recipes that ship in 2026 production for these two models are usually one of: **FP8 full (TRT-LLM, max throughput)** or **AWQ-INT4 (max cost-efficiency)**. The remaining recipes are tradeoffs at the margins.

---

**2. AWQ on Llama 3.3 70B and Qwen 2.5 72B**

AWQ ([arXiv:2306.00978](https://arxiv.org/abs/2306.00978)) is the default starting point for INT4. The pipeline:

```text
1. Load FP16/BF16 reference model
2. Pick calibration data — 256–1024 prompts representative of deployment
3. For each Linear layer:
   a. Compute per-channel activation magnitudes from calibration data
   b. Identify top ~1% channels with largest activation
   c. Compute per-channel scale factor s such that:
      W' = W × s    (the important channels become larger in weight space)
      X' = X / s    (and proportionally smaller in activation space)
   d. Quantize W' to INT4 with group-128 scaling
   e. Keep scale factor for the dequant kernel
4. Output a quantized model + scale factors
```

The key insight: **pre-scaling weights moves "important" rows into the high-magnitude region** of INT4's representable range, where quantization is finer. The activation side absorbs the inverse scale without precision loss because activations are computed at FP16 anyway.

**2.1 Calibration data choice**

The most-underrated decision in AWQ. The calibration set should match the **deployment distribution**:

* **English chat product:** 512+ prompts from WikiText or your own product logs.
* **Multilingual product (Qwen 2.5 72B + Chinese)** : include Chinese-language prompts at the same proportion as production traffic. Calibrating Qwen 2.5 72B on only English calibration data produces measurably worse parity on Chinese eval.
* **Agent / tool-use product:** include tool-call examples. Calibrating without them produces a weight quant that systematically biases against rare action tokens — exactly the bug the [BFCL lecture](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) calls out.

**Rule:** if you cannot defend the calibration set's representativeness, you cannot defend the quant.

**2.2 Group size**

`group_size=128` is the default. It means every 128 contiguous weights share one scale factor.

* Smaller groups (64 or 32) → finer-grained scaling, slightly better parity, slightly larger storage and slower kernels.
* Larger groups (256, "channel-wise") → coarser, faster, lower parity.

For 70B-class models, **group=128 is the universal default**. Drop to 64 only if a specific axis fails parity.

</details>

### 2.3 Llama 3.3 70B 上的 AWQ —— 典型结果

| Recipe | MMLU | HumanEval | BFCL（simple） | Δ vs FP16 |
|--------|------|-----------|----------------|-----------|
| FP16 参考 | 82.0 | 73.8 | 88.2 | — |
| AWQ-INT4 group-128，英文校准 | 81.2 | 72.5 | 86.9 | -0.8 / -1.3 / -1.3 |
| AWQ-INT4 group-128，混合校准 | 81.5 | 72.9 | 87.4 | -0.5 / -0.9 / -0.8 |

数值为近似值；请在自己的实验室复现。**混合校准 recipe 是可用于生产发布的那一个。**

### 2.4 Qwen 2.5 72B 上的 AWQ —— 典型结果

| Recipe | MMLU | CEval（中文） | HumanEval | Δ vs FP16 |
|--------|------|------------------|-----------|-----------|
| FP16 参考 | 86.1 | 83.9 | 87.2 | — |
| AWQ-INT4 group-128，仅英文校准 | 85.3 | 81.5 | 86.6 | -0.8 / **-2.4** / -0.6 |
| AWQ-INT4 group-128，英文+中文校准 | 85.6 | 83.1 | 86.8 | -0.5 / -0.8 / -0.4 |

仅英文校准**在中文评测上损失 2.4 pp** —— 远超任何合理的精度一致性预算。**这是生产中 Qwen 模型上最常见的量化错误：** 开发者使用手头现成的英文校准数据，然后发布一个在他们特意选择 Qwen 的那个语言上**悄无声息地表现不佳**的模型。

---

## 3. GPTQ 对比 AWQ

[GPTQ](https://arxiv.org/abs/2210.17323) 是更早的方法，使用二阶误差度量（OBS）一次量化一个权重。

**差异：**

| 属性 | GPTQ | AWQ |
|----------|------|-----|
| 方法 | OBS 误差最小化 | 激活感知缩放 |
| 逐通道敏感度处理 | 隐式 | 显式 |
| 校准数据敏感度 | 中 | 高（对劣质校准更敏感） |
| agent 工作负载上的精度一致性 | ~ | 通常好 0.3–1 pp |
| 翻译 / 多语言上的精度一致性 | ~ | ~ |
| 推理 kernel | 相同（Marlin / 标准 INT4 GEMM） | 相同 |
| 量化时间 | 慢（70B 需数小时） | 相近 |

**建议：**

* 对于 7B–70B 稠密 LLM，默认用 **AWQ**。若在某个特定维度上精度一致性不达标，再试 GPTQ 作为兜底。
* 对于存在严重离群值的模型（激活量化路径下的 Llama-3-70B 系列，见 §4），单靠 AWQ 或 GPTQ 都不够 —— 使用 QuaRot 或 SpinQuant。

---

## 4. Llama-3-70B 的 W8A8 异常

[arXiv:2408.15301](https://arxiv.org/abs/2408.15301) 记录了 Llama-3-70B 级别模型在逐通道 W8A8 量化下的一种特定失效模式。其架构上的怪癖（由于架构未变，同样适用于 Llama 3.3 70B）是：

* 后段 layer 中少量 MLP 通道的激活值存在**比主体大 50–100× 的离群值**。
* 标准的逐张量激活量化会严重削掉这些值，造成 MMLU 下降 3–5 pp。
* 逐通道缩放有帮助，但仍留下 2–3 pp 的差距。

### 4.1 Llama 3.3 70B 的规避方案

按复杂度递增排列：

1. **保持在 W4A16** —— AWQ-INT4 权重配 FP16 激活值。彻底绕开激活量化问题。**大多数生产部署就是这么做的。**
2. **用 FP8 代替 INT8** —— TRT-LLM 的 FP8 激活值是 E4M3（±448），所以原始范围并不是救星：浮点数的指数间隔网格对小量级的主体部分保持细分辨率，同时又能覆盖离群值，而 INT8 的均匀网格必须二者舍一。TensorRT-LLM 在 Llama 3.3 70B 上的 FP8 路径没有可测量的精度一致性问题。
3. **QuaRot W4A8** —— Hadamard 旋转以不变的方式把离群值摊到各通道上。精度一致性得以恢复。
4. **SpinQuant W4A8** —— 可学习的旋转，精度一致性略优于 QuaRot，但流水线复杂度更高。

**决策树：**

```
Need INT8 activation quant for any reason? ─► QuaRot or SpinQuant
                                              │
Want max throughput on Hopper? ──────────► TRT-LLM FP8 (no anomaly)
                                              │
Want max cost-efficiency? ───────────────► AWQ-INT4 weights + FP16 activations
```

### 4.2 该异常是否适用于 Qwen 2.5 72B？

同一篇论文对 Qwen 模型做了 benchmark，发现它们**对 W8A8 的耐受性优于 Llama-3** —— 没有同等严重的离群值模式。因此**在 Qwen 2.5 72B 上用 W8A8 是可行的**；在 Llama 3.3 70B 上用 W8A8，**不做旋转就不行**。

**这是 Llama 与 Qwen 对比中得到的、最具体的架构相关量化洞见。** 两个模型在 config.json 上看起来很相似，但 Llama-3 有这么一个古怪特性，改变了哪些精度路径可行。

---

## 5. Hopper 上的 FP8 —— TensorRT-LLM 路径

要在 Hopper 上获得最大吞吐，**FP8 权重 + FP8 激活值**就是那个 recipe。成熟的路径是 **TensorRT-LLM**。


<details>
<summary>English original</summary>

**2.3 AWQ on Llama 3.3 70B — typical result**

| Recipe | MMLU | HumanEval | BFCL (simple) | Δ vs FP16 |
|--------|------|-----------|----------------|-----------|
| FP16 reference | 82.0 | 73.8 | 88.2 | — |
| AWQ-INT4 group-128, English calibration | 81.2 | 72.5 | 86.9 | -0.8 / -1.3 / -1.3 |
| AWQ-INT4 group-128, mixed calibration | 81.5 | 72.9 | 87.4 | -0.5 / -0.9 / -0.8 |

Numbers approximate; replicate in your lab. **The mixed-calibration recipe is the production-shippable one.**

**2.4 AWQ on Qwen 2.5 72B — typical result**

| Recipe | MMLU | CEval (Chinese) | HumanEval | Δ vs FP16 |
|--------|------|------------------|-----------|-----------|
| FP16 reference | 86.1 | 83.9 | 87.2 | — |
| AWQ-INT4 group-128, English-only calibration | 85.3 | 81.5 | 86.6 | -0.8 / **-2.4** / -0.6 |
| AWQ-INT4 group-128, English+Chinese calibration | 85.6 | 83.1 | 86.8 | -0.5 / -0.8 / -0.4 |

The English-only calibration **loses 2.4 pp on Chinese eval** — well outside any reasonable parity budget. **This is the most common quantization mistake on Qwen models in production:** developers use English calibration data because it's what's available, and ship a model that **silently underperforms** on the language they specifically picked Qwen for.

---

**3. GPTQ vs AWQ**

[GPTQ](https://arxiv.org/abs/2210.17323) is the older method, quantizing one weight at a time using a second-order error metric (OBS).

**Differences:**

| Property | GPTQ | AWQ |
|----------|------|-----|
| Method | OBS error minimization | Activation-aware scaling |
| Per-channel sensitivity handling | implicit | explicit |
| Calibration data sensitivity | medium | high (more sensitive to bad calibration) |
| Parity on agent workloads | ~ | typically 0.3–1 pp better |
| Parity on translation / multilingual | ~ | ~ |
| Inference kernel | identical (Marlin / standard INT4 GEMM) | identical |
| Quantization time | slow (hours for 70B) | similar |

**Recommendation:**

* Default to **AWQ** for 7B–70B dense LLMs. If parity fails on a specific axis, try GPTQ as a fallback.
* For models with severe outliers (Llama-3-70B family for activation-quant paths, see §4), neither AWQ nor GPTQ alone is sufficient — use QuaRot or SpinQuant.

---

**4. The Llama-3-70B W8A8 anomaly**

[arXiv:2408.15301](https://arxiv.org/abs/2408.15301) documented a specific failure mode of Llama-3-70B-class models under per-channel W8A8 quantization. The architectural quirk (which carries over to Llama 3.3 70B since the architecture is unchanged) is:

* A small number of MLP channels in late layers have activations with **outliers 50–100× larger than the bulk**.
* Standard per-tensor activation quantization clips these severely, producing a 3–5 pp MMLU drop.
* Per-channel scaling helps but still leaves a 2–3 pp gap.

**4.1 Workarounds for Llama 3.3 70B**

In order of increasing complexity:

1. **Stay at W4A16** — AWQ-INT4 weights with FP16 activations. Dodges the activation-quantization problem entirely. **This is what most production deployments do.**
2. **Use FP8 instead of INT8** — TRT-LLM's FP8 activations are E4M3 (±448), so raw range is not the savior: floating point's exponentially-spaced grid keeps fine resolution for the small-magnitude bulk while still reaching the outliers, where INT8's uniform grid must sacrifice one for the other. TensorRT-LLM's FP8 path on Llama 3.3 70B has no measurable parity issue.
3. **QuaRot W4A8** — Hadamard rotation invariantly spreads the outliers across channels. Parity recovers.
4. **SpinQuant W4A8** — learnable rotation, slightly better parity than QuaRot at higher pipeline complexity.

**Decision tree:**

```
Need INT8 activation quant for any reason? ─► QuaRot or SpinQuant
                                              │
Want max throughput on Hopper? ──────────► TRT-LLM FP8 (no anomaly)
                                              │
Want max cost-efficiency? ───────────────► AWQ-INT4 weights + FP16 activations
```

**4.2 Does the anomaly apply to Qwen 2.5 72B?**

The same paper benchmarked Qwen models and found they **tolerate W8A8 better than Llama-3** — no equivalent severe outlier pattern. So **W8A8 on Qwen 2.5 72B is viable**; W8A8 on Llama 3.3 70B is **not without rotation**.

**This is the most concrete architecture-specific quantization insight from the Llama vs Qwen comparison.** Both models look similar in their config.json, but Llama-3 has this one quirky property that changes which precision paths are practical.

---

**5. FP8 on Hopper — the TensorRT-LLM path**

For maximum throughput on Hopper, **FP8 weights + FP8 activations** is the recipe. The mature path is **TensorRT-LLM**.

</details>

### 5.1 TRT-LLM 构建

```bash
# Quantize the model
python examples/llama/quantize.py \
    --model_dir /path/to/llama-3.3-70b \
    --output_dir ./fp8_checkpoint \
    --dtype float16 \
    --qformat fp8 \
    --kv_cache_dtype fp8 \
    --calib_size 512

# Build the engine
trtllm-build \
    --checkpoint_dir ./fp8_checkpoint \
    --output_dir ./engine \
    --gemm_plugin float16 \
    --use_fp8_context_fmha enable \
    --max_input_len 8192 \
    --max_output_len 4096 \
    --max_batch_size 64 \
    --tp_size 4
```

engine plan 是针对选定的（精度、批范围、序列长度范围、TP size）预先计算好的。改动其中任何一项都需要重新构建。

### 5.2 Hopper 上的预期吞吐

4× H100 SXM（TP=4）、Llama 3.3 70B：

| 精度 | TPOT (batch=1) | TPOT (batch=64) | 吞吐 tok/s/GPU |
|-----------|----------------|------------------|----------------------|
| BF16 (vLLM) | ~38 ms | ~24 ms | ~660 tok/s/GPU |
| FP8 (TRT-LLM) | ~22 ms | ~13 ms | ~1200 tok/s/GPU |

FP8 带来约 1.7–1.8× 的吞吐提升。MMLU 上的精度一致性下降约 0.2 pp（完全在预算之内）。

Qwen 2.5 72B 的数据与之相近，但相同批下慢约 5%，原因是模型略大。

### 5.3 vLLM 0.22+ 的 FP8 路径

到 2026 年，vLLM 的 FP8 实现**正在逼近 TRT-LLM**。吞吐约为 **TRT-LLM 的 90%**，而部署摩擦只有约 50%（无需 engine 编译步骤）。对迭代速度关键的 chat 产品而言，**vLLM FP8 往往是务实之选**。

### 5.4 FP8 block-scaling × 张量并行的对齐陷阱

这是生产环境里的坑，团队第一次把 FP8 与 TP 结合时就会踩到，因此在进入 Lecture 04 之前值得内化。

现代 FP8 权重量化是 **block-scaled** 的（每个权重块一个独立 scale——例如 128×128——这正是 FP8 保持准确率的原因）。scale 网格与张量的维度绑定：权重列维度 `N` 必须是整数个块，即**`N` 能被 block size 整除**。

张量并行把这些相同的维度切分到多个 GPU 上。当把一个输出维度为 `N` 的权重切分到 `TP` 个 GPU 上时，每个 GPU 得到 `N / TP`。**如果 `N / TP` 不是 FP8 block size 的倍数，engine 会拒绝加载**，并报出类似 *"output size not divisible by block size."* 的错误。

具体来说，Qwen 2.5 72B 的 FFN 中间维度是 `29568 = 128 × 231`，而 `231 = 3 × 7 × 11`——因此 `29568 / TP` 只有在 `TP ∈ {1, 3, 7, 11, …}` 时才是 128 的倍数，**而非**大家首先会去尝试的 2 的幂 `{2, 4, 8}`。在这个模型上，block-scaled FP8 + TP=8 开箱即会加载失败。

修复方案，按优先级排序：

1. **选择能让每个被切分维度都保持块对齐的 TP**——这通常意味着从 TP=8 降到 TP=4（若仍不能整除，则用 TP=2）。更小的 TP 会损失一些吞吐余量（Lecture 04 §4），但这是最干净的修复。
2. **使用会对出问题的维度做 padding 的框架构建**，将其补齐到下一个块倍数（vLLM 与 TRT-LLM 正越来越多地自动这么做；请确认你的版本）。
3. **粗化或改变 scaling 的粒度**（用 per-channel/per-tensor 取代 per-block）——以少量准确率换取对齐上的自由度。

教训是：**采用 block-scaled FP8 后，张量并行大小不再是可随意调节的性能旋钮——它受算术约束。** 先验证加载再做 benchmark，并把调整 TP 视为 FP8 加载失败的首选修复手段。

---

## 6. KV cache 量化

在权重之后，KV cache 是**第二个精度决策**，在长上下文下尤其如此。

### 6.1 FP8 KV cache

FP8 E5M2（范围 ±57000）是 KV 的安全默认选择。

* **把 KV 字节数减半**——在 128K 上下文下，每个请求从 42 GB 降到 21 GB。
* 在 decode（逐 token 生成阶段）期间**把 KV 读取带宽减半**——在长上下文下意义显著。
* **精度一致性下降：** 若使用 per-head scaling，在多数评测上约 0.1–0.3 pp。

**Per-head scaling** 很关键。Per-tensor scaling 会截断离群 head（稀有 token 专门化的 head）。vLLM 0.22+、SGLang 0.5+ 与 TRT-LLM 都支持 per-head FP8 KV。

### 6.2 INT4 KV cache

激进。把 KV 缩减 4×。

* 允许 4× 更长的上下文，或每个 HBM 承载 4× 的并发请求。
* **精度一致性损失：** 在长上下文评测（RULER、needle-in-haystack）上为 1–3 pp。在短上下文下的 chat/agent 场景通常没问题。
* **务必在产品实际使用的上下文长度上验证。** 短上下文的精度一致性无法预测长上下文的行为。

对 128K 上下文下的 Llama 3.3 70B / Qwen 2.5 72B 而言，**FP8 KV 是生产默认**。INT4 KV 则是**研究模式选项**。

---

## 7. 完整的精度一致性验证方法

把 Part 1 Lecture 04 的纪律落到实操：

### 7.1 固定参考实现

对 Llama 3.3 70B：BF16 权重、FP16 KV、官方 tokenizer、固定 seed 列表。
对 Qwen 2.5 72B：相同。

对权重取 hash。该 hash 写入 bench 报告。任何量化候选都以此精确参考为判据。


<details>
<summary>English original</summary>

**5.1 The TRT-LLM build**

```bash
# Quantize the model
python examples/llama/quantize.py \
    --model_dir /path/to/llama-3.3-70b \
    --output_dir ./fp8_checkpoint \
    --dtype float16 \
    --qformat fp8 \
    --kv_cache_dtype fp8 \
    --calib_size 512

# Build the engine
trtllm-build \
    --checkpoint_dir ./fp8_checkpoint \
    --output_dir ./engine \
    --gemm_plugin float16 \
    --use_fp8_context_fmha enable \
    --max_input_len 8192 \
    --max_output_len 4096 \
    --max_batch_size 64 \
    --tp_size 4
```

The engine plan is precomputed for the chosen (precision, batch range, sequence length range, TP size). Changing any of these requires a rebuild.

**5.2 Expected throughput on Hopper**

On 4× H100 SXM (TP=4), Llama 3.3 70B:

| Precision | TPOT (batch=1) | TPOT (batch=64) | Throughput tok/s/GPU |
|-----------|----------------|------------------|----------------------|
| BF16 (vLLM) | ~38 ms | ~24 ms | ~660 tok/s/GPU |
| FP8 (TRT-LLM) | ~22 ms | ~13 ms | ~1200 tok/s/GPU |

FP8 ≈ 1.7–1.8× throughput improvement. Parity drop on MMLU: ~0.2 pp (well within budget).

For Qwen 2.5 72B numbers are similar but ~5% slower at the same batch due to the slightly larger model.

**5.3 vLLM 0.22+ FP8 path**

vLLM's FP8 implementation is **approaching TRT-LLM** in 2026. Roughly **90% of TRT-LLM throughput** for ~50% of the deployment friction (no engine compile step). For chat products where iteration matters, **vLLM FP8 is often the practical pick**.

**5.4 The FP8 block-scaling × tensor-parallel alignment trap**

This is the production footgun that catches teams the first time they combine FP8 with TP, so it is worth internalizing before Lecture 04.

Modern FP8 weight quantization is **block-scaled** (a separate scale per block of weights — e.g., 128×128 — which is what keeps FP8 accurate). The scale grid is tied to the tensor's dimensions: a weight column dimension `N` must be a whole number of blocks, i.e., **`N` divisible by the block size**.

Tensor parallelism slices those same dimensions across GPUs. When you shard a weight of output dim `N` across `TP` GPUs, each GPU gets `N / TP`. **If `N / TP` is not a multiple of the FP8 block size, the engine refuses to load** with an error like *"output size not divisible by block size."*

Concretely, the FFN intermediate of Qwen 2.5 72B is `29568 = 128 × 231`, and `231 = 3 × 7 × 11` — so `29568 / TP` stays a multiple of 128 only for `TP ∈ {1, 3, 7, 11, …}`, **not** for the powers of two `{2, 4, 8}` everyone reaches for first. Block-scaled FP8 + TP=8 on this model will fail to load out of the box.

The fixes, in order of preference:

1. **Pick a TP that keeps every sharded dimension block-aligned** — often this means dropping from TP=8 to TP=4 (and if that still doesn't divide, TP=2). Smaller TP costs some throughput headroom (Lecture 04 §4) but is the cleanest fix.
2. **Use a framework build that pads** the offending dimension up to the next block multiple (vLLM and TRT-LLM increasingly do this automatically; check your version).
3. **Coarsen or change the scaling granularity** (per-channel/per-tensor instead of per-block) — trades a little accuracy for alignment freedom.

The lesson: **with block-scaled FP8, your tensor-parallel size is no longer a free performance knob — it is constrained by arithmetic.** Verify load before you benchmark, and treat a TP change as a first-line fix for FP8 load failures.

---

**6. KV cache quantization**

The KV cache is the **second precision decision** after weights, especially at long context.

**6.1 FP8 KV cache**

FP8 E5M2 (range ±57000) is the safe default for KV.

* **Cuts KV bytes in half** — at 128K context this is 21 GB instead of 42 GB per request.
* **Halves KV read bandwidth** during decode — meaningful at long context.
* **Parity drop:** ~0.1–0.3 pp on most evals if per-head scaling is used.

**Per-head scaling** matters. Per-tensor scaling can clip outlier heads (rare-token specialization heads). vLLM 0.22+, SGLang 0.5+, and TRT-LLM all support per-head FP8 KV.

**6.2 INT4 KV cache**

Aggressive. Cuts KV by 4×.

* Allows 4× longer context or 4× more concurrent requests per HBM.
* **Parity loss:** 1–3 pp on long-context evals (RULER, needle-in-haystack). On chat/agent at short context it's usually fine.
* **Always validate at the actual context length the product will use.** Short-context parity does not predict long-context behavior.

For Llama 3.3 70B / Qwen 2.5 72B at 128K context, **FP8 KV is the production default**. INT4 KV is a **research-mode option**.

---

**7. The full parity validation methodology**

The discipline from Part 1 Lecture 04, applied:

**7.1 Pin the reference**

For Llama 3.3 70B: BF16 weights, FP16 KV, official tokenizer, fixed seed list.
For Qwen 2.5 72B: same.

Hash the weights. The hash goes in the bench report. Any quant candidate is judged against this exact reference.

</details>

### 7.2 固定评测集

按工作负载类别：

* **聊天：** MMLU（500 题子集，固定种子）、GSM8K（200 题）、若与代码相关则为 HumanEval。
* **多语言（Qwen）：** CEval、MMLU-CN、FLORES（如涉及翻译）。
* **Agent:** [BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) 在你所服务的类别上（simple、multiple、irrelevance、multi-turn）。
* **长上下文：** RULER 在 16K / 64K / 128K。

### 7.3 候选运行

1. 应用量化。
2. 重新运行相同评测。按类别计算 Δ。
3. 检查失效项：采样 20 个新失效项，加以分类。

### 7.4 精度一致性预算

对于生产聊天产品中的 Llama 3.3 70B 和 Qwen 2.5 72B，一个可辩护的预算：

| 指标 | 下限（参考） | 相对参考的预算 |
|--------|-------------------|----------------|
| MMLU | 82.0 / 86.1 | -1.0 pp |
| HumanEval | 73.8 / 87.2 | -1.5 pp |
| BFCL (simple) | 88.2 / ~88 | -2.0 pp |
| BFCL (irrelevance) | ~80 / ~80 | -2.0 pp |
| RULER 64K | ~89.5 / ~91 | -1.5 pp |
| 分语言评测（Qwen 中文） | 83.9 | -1.5 pp |

这些预算需 *按工作负载论证*，而非通用。代码产品无法承受 HumanEval -1.5 pp；对这类产品应收紧到 -0.5。非多语言产品可忽略 CEval。

### 7.5 这些模型特定的失效模式

* **Llama 3.3 70B W8A8 → MMLU 下降 3 pp。** 切换到 W4A16 或 QuaRot。
* **Qwen 2.5 72B AWQ-INT4 仅用英文校准 → CEval 下降 2 pp。** 修复校准数据。
* **任一模型在 64K+ 上下文使用 INT4 KV → RULER 下降 3-5 pp。** 切换到 FP8 KV。
* **FP8 KV 按张量 → BFCL 在稀有动作 token 上下降 1 pp。** 切换到逐 head 的 FP8 KV。

这四种已有充分记录且可恢复。任何在预算内无法恢复的情况 → 重新选择精度 recipe。

---

## 实验 — 为两个模型产出可辩护的 recipe

目标：为 Llama 3.3 70B 和 Qwen 2.5 72B 各自交付一个带有经精度一致性验证数字的精度 recipe。上限两天。

1. **参考运行** — 在固定评测集上为两者建立 BF16 / FP16 基线。各两个种子 → 噪声下限。
2. **候选 A — AWQ-INT4 group-128**，使用适当校准（Llama 用英文，Qwen 用英文+中文）。重新运行评测。
3. **候选 B — 通过 TRT-LLM 的 FP8 权重+激活值**。重新运行评测。
4. **候选 C — AWQ-INT4 + FP8 KV**，64K 上下文。重新运行评测（仅限长上下文相关评测）。
5. **仅对 Llama 3.3 70B — 候选 D — 尝试带 QuaRot 的 W8A8。** 与 W4A16 基线比较。
6. **为每个模型产出一份 recipe 报告**：
   * 精度 recipe。
   * 按评测类别的 Δ 相对参考（标记精度一致性预算）。
   * 每个 knob 的吞吐改进（TPOT、吞吐 tok/s/GPU）。
   * 推荐的发布 recipe，附两句话理由。

通过标准：另一位工程师阅读报告后，认同该发布 recipe 是所述工作负载类别的正确取舍。

---

## 自检

1. 一名队友想在 H100 上以 W8A8 部署 Llama 3.3 70B 以获得最大吞吐。用两句话支持或反对，并引用具体 arXiv 结果。
2. 为什么 Qwen 2.5 72B 上使用仅英文校准的 AWQ 会特别在 CEval 上失败？梳理链路：校准 → 权重缩放 → 保护哪些通道 → 哪些语言的 token 使用这些通道。
3. 对于 128K 上下文的 Llama 3.3 70B，FP8 权重 + FP8 KV 会使总 HBM 达到多少（单请求，无批处理）？能放进一张 H200 吗？
4. 你的 AWQ-INT4 Llama 3.3 70B 通过了 MMLU 和 HumanEval，但 BFCL（simple）下降 2.5 pp。最可能的失效模式是什么？修复它的第一个实验是什么？
5. 对于中文聊天产品，在 (a) H100 上的 AWQ-INT4 Qwen 2.5 72B 与 (b) H200 上的 FP8 Llama 3.3 70B 之间，按成本你会选哪个？按质量呢？请具体说明取舍。

---


<details>
<summary>English original</summary>

**7.2 Pin the eval set**

Per workload class:

* **Chat:** MMLU (subset of 500 questions, fixed seed), GSM8K (200 problems), HumanEval if code-relevant.
* **Multilingual (Qwen):** CEval, MMLU-CN, FLORES (translation if relevant).
* **Agent:** [BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) at the categories you serve (simple, multiple, irrelevance, multi-turn).
* **Long context:** RULER at 16K / 64K / 128K.

**7.3 The candidate run**

1. Apply quantization.
2. Re-run identical eval. Compute Δ per category.
3. Inspect failures: sample 20 newly-failing items, categorize.

**7.4 The parity budget**

For Llama 3.3 70B and Qwen 2.5 72B in a production chat product, a defensible budget:

| Metric | Floor (reference) | Budget vs ref |
|--------|-------------------|----------------|
| MMLU | 82.0 / 86.1 | -1.0 pp |
| HumanEval | 73.8 / 87.2 | -1.5 pp |
| BFCL (simple) | 88.2 / ~88 | -2.0 pp |
| BFCL (irrelevance) | ~80 / ~80 | -2.0 pp |
| RULER 64K | ~89.5 / ~91 | -1.5 pp |
| Per-language eval (Qwen Chinese) | 83.9 | -1.5 pp |

Budgets are *workload-defended*, not generic. A code-product cannot afford -1.5 pp HumanEval; for them tighten to -0.5. A non-multilingual product can ignore CEval.

**7.5 Failure modes specific to these models**

* **Llama 3.3 70B W8A8 → MMLU drops 3 pp.** Switch to W4A16 or QuaRot.
* **Qwen 2.5 72B AWQ-INT4 with English-only calibration → CEval drops 2 pp.** Fix calibration data.
* **Either at INT4 KV at 64K+ context → RULER drops 3-5 pp.** Switch to FP8 KV.
* **FP8 KV per-tensor → BFCL drops 1 pp on rare-action tokens.** Switch to per-head FP8 KV.

These four are well-documented and recoverable. Anything you cannot recover within budget → re-pick precision recipe.

---

**Lab — produce defended recipes for both models**

Goal: ship a precision recipe with parity-validated numbers for each of Llama 3.3 70B and Qwen 2.5 72B. Cap of two days.

1. **Reference runs** — BF16 / FP16 baseline for both on fixed eval set. Two seeds each → noise floor.
2. **Candidate A — AWQ-INT4 group-128** with appropriate calibration (English for Llama, English+Chinese for Qwen). Re-run eval.
3. **Candidate B — FP8 weights+activations via TRT-LLM**. Re-run eval.
4. **Candidate C — AWQ-INT4 + FP8 KV** at 64K context. Re-run eval (long-context-relevant evals only).
5. **For Llama 3.3 70B only — Candidate D — try W8A8 with QuaRot.** Compare with W4A16 baseline.
6. **Produce a recipe report** per model:
   * Precision recipe.
   * Per-eval-category Δ vs reference (with parity budget marked).
   * Per-knob throughput improvement (TPOT, throughput tok/s/GPU).
   * Recommended ship recipe with two-sentence justification.

Pass criterion: another engineer reading the report agrees the ship recipe is the right tradeoff for the stated workload class.

---

**Self-check**

1. A teammate wants to deploy Llama 3.3 70B at W8A8 on H100 for max throughput. Defend or reject in two sentences, citing the specific arXiv result.
2. Why does AWQ on Qwen 2.5 72B with English-only calibration fail on CEval specifically? Walk the chain: calibration → weight scaling → which channels are protected → which language's tokens use those channels.
3. For Llama 3.3 70B at 128K context, FP8 weights + FP8 KV would put you at what total HBM (one request, no batching)? Will it fit on one H200?
4. Your AWQ-INT4 Llama 3.3 70B passes MMLU and HumanEval but BFCL (simple) drops 2.5 pp. What's the most likely failure mode and what is the first experiment to fix it?
5. For a Chinese-language chat product, between (a) AWQ-INT4 Qwen 2.5 72B at H100 and (b) FP8 Llama 3.3 70B at H200, which would you pick on cost? On quality? Be specific about the tradeoffs.

---

</details>

## 参考资料

* AWQ —— [arXiv:2306.00978](https://arxiv.org/abs/2306.00978)
* GPTQ —— [arXiv:2210.17323](https://arxiv.org/abs/2210.17323)
* "The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization" —— [arXiv:2408.15301](https://arxiv.org/abs/2408.15301)
* QuaRot —— [arXiv:2404.00456](https://arxiv.org/abs/2404.00456)
* SpinQuant —— [arXiv:2405.16406](https://arxiv.org/abs/2405.16406)
* TensorRT-LLM FP8 documentation —— [nvidia.github.io/TensorRT-LLM/](https://nvidia.github.io/TensorRT-LLM/)
* AutoAWQ —— [github.com/casper-hansen/AutoAWQ](https://github.com/casper-hansen/AutoAWQ)
* MMLU benchmark —— [github.com/hendrycks/test](https://github.com/hendrycks/test)
* HumanEval —— [github.com/openai/human-eval](https://github.com/openai/human-eval)
* RULER 长上下文评估 —— [arXiv:2404.06654](https://arxiv.org/abs/2404.06654)

交叉引用：

* [第 1 部分 → 第 04 讲 —— 精度栈](../01-基础/Lecture-04.md)
* [阶段 5 → 边缘 AI → Qwen 推理优化 → 第 02 讲 —— 量化 Qwen3-4B](../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md)
* [阶段 5 → 边缘 AI → 用 BFCL 做 agent 工具分发评估](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) —— agent 工作负载的精度一致性

---

## 截至 2026-06

AWQ、GPTQ、QuaRot、SpinQuant 是锁定的方法。Hopper 上的 FP8（E4M3 / E5M2），经由 TE / TRT-LLM。当某个方法发布，并在这些特定模型上给出显著优于 AWQ 的精度一致性时，或当 Hopper 上的 FP4 路径落地时（预计不会有——那属于 Blackwell），就刷新本条。

---

## 下一篇

* 下一篇：[第 04 讲 —— 单节点多 GPU 推理服务（TP）](Lecture-04.md)
* 上一篇：[第 02 讲 —— Hopper 硬件故事](Lecture-02.md)
* 上级：[第 2 部分 —— Hopper 上的稠密](README.md)


<details>
<summary>English original</summary>

**References**

* AWQ — [arXiv:2306.00978](https://arxiv.org/abs/2306.00978)
* GPTQ — [arXiv:2210.17323](https://arxiv.org/abs/2210.17323)
* "The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization" — [arXiv:2408.15301](https://arxiv.org/abs/2408.15301)
* QuaRot — [arXiv:2404.00456](https://arxiv.org/abs/2404.00456)
* SpinQuant — [arXiv:2405.16406](https://arxiv.org/abs/2405.16406)
* TensorRT-LLM FP8 documentation — [nvidia.github.io/TensorRT-LLM/](https://nvidia.github.io/TensorRT-LLM/)
* AutoAWQ — [github.com/casper-hansen/AutoAWQ](https://github.com/casper-hansen/AutoAWQ)
* MMLU benchmark — [github.com/hendrycks/test](https://github.com/hendrycks/test)
* HumanEval — [github.com/openai/human-eval](https://github.com/openai/human-eval)
* RULER long-context eval — [arXiv:2404.06654](https://arxiv.org/abs/2404.06654)

Cross-references:

* [Part 1 → Lecture 04 — The precision stack](../01-基础/Lecture-04.md)
* [Phase 5 → Edge AI → Qwen Inference Optimization → Lecture 02 — Quantizing Qwen3-4B](../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md)
* [Phase 5 → Edge AI → Agent Tool-Dispatch Evaluation with BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) — parity for agent workloads

---

**Current as of 2026-06**

AWQ, GPTQ, QuaRot, SpinQuant are the pinned methods. FP8 (E4M3 / E5M2) on Hopper via TE / TRT-LLM. Refresh when a method ships with materially better-than-AWQ parity on these specific models, or when FP4-on-Hopper paths land (none expected — that's Blackwell).

---

**Next**

* Next: [Lecture 04 — Single-node multi-GPU serving (TP)](Lecture-04.md)
* Previous: [Lecture 02 — Hopper hardware story](Lecture-02.md)
* Up: [Part 2 — Dense at Hopper](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 2 - Dense at Hopper/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%202%20-%20Dense%20at%20Hopper/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
