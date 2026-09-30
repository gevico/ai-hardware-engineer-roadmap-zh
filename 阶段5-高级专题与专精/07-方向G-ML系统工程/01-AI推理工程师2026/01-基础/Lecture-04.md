---
title: Part 1 · Lecture 04 — 精度栈：FP16 → FP8 → FP4 → INT4
description: Part 1 · Lecture 04 — 精度栈：FP16 → FP8 → FP4 → INT4
---

# Part 1 · Lecture 04 — 精度栈：FP16 → FP8 → FP4 → INT4

## 概览

如果 roofline（性能上界模型；Lecture 03）是 GPU 的*静态*上限，那么**精度就是决定 kernel 落在 roofline 上哪个位置的杠杆。** 把权重精度砍半，从 HBM 读取的字节数就减半，算术强度翻倍；而且——如果该 kernel 原本受带宽限制——吞吐大致翻倍。

问题在于：每一次精度下调都是**一次潜在的精度一致性下滑**。**量化**是这样一门工程学科：把精度下限压下去，却不把准确率一起带走。不做**精度一致性门禁**就量化的工程师，交付的是事故。

本讲涵盖：

1. 2026 年的精度格局——每种格式实际存储什么，以及它在哪些硬件上原生落地。
2. 量化的三条轴线——权重、激活值、KV cache——以及为什么它们是三个不同的决策。
3. 主要的仅权重方法——AWQ、GPTQ、QuaRot、SpinQuant——以及如何选择。
4. 已知异常——包括 Part 2 将重新审视的 Llama-3-70B W8A8 敏感性问题（arXiv:2408.15301）。
5. 精度一致性验证方法论——该测什么、该定多少预算、什么绝不能让步。

读完本讲，你应当能够面对一个模型 + 工作负载 + 硬件目标，写出一份精度 recipe（例如，“AWQ-INT4 权重、FP16 激活值、FP8 KV”），并为每一项给出有依据的精度一致性预算。

---

## 1. 2026 年的精度格局

三年内精度下限下调了两次。当前的技术栈：

| 格式 | 位数 | 范围 / 尾数 | 所在位置 |
|--------|------|------------------|----------------|
| FP32 | 32 | 完整 IEEE 754 | 训练参考，从不部署 |
| TF32 | 19 | 8 位指数、10 位尾数 | Ampere 架构及以上训练，推理中很少用 |
| BF16 | 16 | 8 位指数、7 位尾数 | 默认训练精度，推理中常用 |
| FP16 | 16 | 5 位指数、10 位尾数 | FP8 之前推理的主力 |
| **FP8 E4M3** | 8 | 4 位指数、3 位尾数 | Hopper + Blackwell 原生，FP8 权重/激活值的主力 |
| **FP8 E5M2** | 8 | 5 位指数、2 位尾数 | Hopper + Blackwell，常用于梯度 / KV |
| **FP6 (E3M2, E2M3)** | 6 | 视格式而定 | Blackwell 支持，实践中较少用 |
| **FP4 (E2M1, MX-FP4)** | 4 | 2 位指数、1 位尾数 | Blackwell 原生，微缩放 |
| INT8 | 8 | 有符号整数 | 通用，需校准；传统量化 |
| INT4 | 4 | 有符号整数 | 仅权重的主流（AWQ / GPTQ / GGUF） |

2025–2026 年的关键变化：

* 在 kernel 已成熟的 Hopper 及更新硬件上，**FP8 成为新的默认激活值精度**。大多数旗舰推理服务部署（TensorRT-LLM、vLLM 0.22+）都优先交付 FP8。
* Blackwell 上的 **FP4 原生运算**（Transformer Engine 2）使 FP8 吞吐翻倍。微缩放格式（MX-FP4）附带按块的缩放因子，从而保住动态范围。
* **在成本敏感的推理服务中，仅权重的 INT4 仍是主流量化方式**——AWQ、GPTQ 和 GGUF-IQ-quants 都在此列。INT4 权重配 FP16（或 FP8）激活值，是在 1–4 张 GPU 上跑 70B 级模型的实用 recipe。
* **KV cache 量化**在 FP8 上越来越常见（有时也做带校准的 INT4），因为在长上下文下 KV cache 主导了 HBM 占用。

### 1.1 硬件支持门槛

一种精度格式只有在 GPU 为其提供**原生张量核心**时才有用。矩阵如下：

| 格式 | Ampere 架构（A100、RTX 3000） | Hopper (H100/H200) | Ada (L40S, RTX 4000) | Blackwell (B200, RTX 5000) |
|--------|--------------------------|--------------------|----------------------|----------------------------|
| FP16/BF16 | ✓ | ✓ | ✓ | ✓ |
| FP8 | ✗（模拟） | ✓ TE | ✓ TE | ✓ TE2 |
| FP6 | ✗ | ✗ | ✗ | ✓ |
| FP4 | ✗ | ✗ | ✗ | ✓ TE2 |
| INT8 (DP4A) | ✓ | ✓ | ✓ | ✓ |
| INT4 仅权重 | 权重以低精度存储，以更高精度计算 | 相同 | 相同 | 相同 |

**INT4 仅权重是一种软件模式**，而非硬件操作：权重以每元素 4 位存储，在共享内存中反量化为 FP16（或 FP8），再以该更高精度计算。不存在原生 INT4 张量核心。这就是为什么 AWQ-INT4 + FP16 激活值能在 A100 以来的任何现代 GPU 上运行。

**FP8 和 FP4 *确实*是硬件操作**——Hopper / Blackwell 的张量核心在这些精度上原生执行矩阵乘。性能收益来自真实硅片，而不是软件技巧。

---

## 2. 量化的三条轴线——权重、激活值、KV cache

这是**三个彼此独立的工程决策**，各自对应三种不同的精度一致性代价。

### 2.1 权重

量化权重会削减：

* HBM 容量按比例下降（FP16 → INT4：缩小 4×）。
* decode（逐 token 生成阶段）读取时的 HBM 带宽（主要开销）也按同样比例下降。

敏感度是**逐 layer、逐通道**的——某些 attention head 或 MLP 行对量化的容忍度很差。方法选择（AWQ / GPTQ / QuaRot）主要就是如何优雅地处理这些敏感行。


<details>
<summary>English original</summary>

**Part 1 · Lecture 04 — The Precision Stack: FP16 → FP8 → FP4 → INT4**

**Overview**

If the roofline (Lecture 03) is the *static* ceiling of a GPU, **precision is the lever that moves where on the roofline a kernel sits.** Cutting weight precision in half cuts the bytes read from HBM in half, doubles arithmetic intensity, and — if the kernel was bandwidth-bound — roughly doubles throughput.

The catch: every precision drop is a **potential parity drop**. **Quantization** is the engineering discipline of taking a precision floor down without taking accuracy with it. The engineer who quantizes without a **parity gate** is shipping incidents.

This lecture covers:

1. The 2026 precision landscape — what each format actually stores and where it ships natively.
2. The three quantization axes — weights, activations, KV cache — and why they're three different decisions.
3. The major weight-only methods — AWQ, GPTQ, QuaRot, SpinQuant — and how to pick.
4. The known anomalies — including the Llama-3-70B W8A8 sensitivity (arXiv:2408.15301) that Part 2 will revisit.
5. The parity validation methodology — what to measure, what budget to set, what to never trade away.

By the end you should be able to look at a model + workload + hardware target and write down a precision recipe (e.g., "AWQ-INT4 weights, FP16 activations, FP8 KV") with a defended parity budget for each.

---

**1. The 2026 precision landscape**

The precision floor has dropped twice in three years. The current stack:

| Format | Bits | Range / mantissa | Where it lives |
|--------|------|------------------|----------------|
| FP32 | 32 | full IEEE 754 | training reference, never deployed |
| TF32 | 19 | 8-bit exp, 10-bit mantissa | Ampere+ training, rarely inference |
| BF16 | 16 | 8-bit exp, 7-bit mantissa | default training precision, common inference |
| FP16 | 16 | 5-bit exp, 10-bit mantissa | inference workhorse pre-FP8 |
| **FP8 E4M3** | 8 | 4-bit exp, 3-bit mantissa | Hopper + Blackwell native, primary FP8 weight/activation |
| **FP8 E5M2** | 8 | 5-bit exp, 2-bit mantissa | Hopper + Blackwell, often used for gradients / KV |
| **FP6 (E3M2, E2M3)** | 6 | various | Blackwell support, less common in practice |
| **FP4 (E2M1, MX-FP4)** | 4 | 2-bit exp, 1-bit mantissa | Blackwell native, microscaled |
| INT8 | 8 | signed integer | universal, calibrated; legacy quantization |
| INT4 | 4 | signed integer | weight-only mainstream (AWQ / GPTQ / GGUF) |

The key 2025–2026 shifts:

* **FP8 is the new default activation precision** on Hopper-and-newer hardware where the kernels are mature. Most flagship inference deployments (TensorRT-LLM, vLLM 0.22+) ship FP8 first.
* **FP4 native arithmetic on Blackwell** (Transformer Engine 2) doubles FP8 throughput. Microscaling format (MX-FP4) attaches per-block scale factors so the dynamic range is preserved.
* **INT4 weight-only remains the dominant quantization for cost-sensitive serving** — AWQ, GPTQ, and GGUF-IQ-quants all live here. INT4 weights at FP16 (or FP8) activations is the practical recipe for 70B-class on 1–4 GPUs.
* **KV cache quantization** is increasingly common at FP8 (and sometimes INT4 with calibration) because the KV cache dominates HBM at long context.

**1.1 The hardware support gate**

A precision format is only useful if the GPU has **native tensor cores** for it. The matrix:

| Format | Ampere (A100, RTX 3000) | Hopper (H100/H200) | Ada (L40S, RTX 4000) | Blackwell (B200, RTX 5000) |
|--------|--------------------------|--------------------|----------------------|----------------------------|
| FP16/BF16 | ✓ | ✓ | ✓ | ✓ |
| FP8 | ✗ (emulated) | ✓ TE | ✓ TE | ✓ TE2 |
| FP6 | ✗ | ✗ | ✗ | ✓ |
| FP4 | ✗ | ✗ | ✗ | ✓ TE2 |
| INT8 (DP4A) | ✓ | ✓ | ✓ | ✓ |
| INT4 weight-only | weights stored low-precision, computed at higher | same | same | same |

**INT4 weight-only is a software pattern**, not a hardware operation: the weights are stored at 4 bits per element, dequantized to FP16 (or FP8) in shared memory, and computed at that higher precision. There is no native INT4 tensor core. This is why AWQ-INT4 + FP16 activations works on any modern GPU back to A100.

**FP8 and FP4 *are* hardware operations** — Hopper / Blackwell tensor cores execute matmul natively at those precisions. The performance win is real silicon, not a software trick.

---

**2. The three quantization axes — weights, activations, KV cache**

These are **three separate engineering decisions** with three separate parity costs.

**2.1 Weights**

Quantizing weights cuts:

* HBM capacity by the ratio (FP16 → INT4: 4× smaller).
* HBM bandwidth on decode read (the dominant cost) by the same ratio.

Sensitivity is **per-layer and per-channel** — some attention heads or MLP rows tolerate quantization poorly. Method choice (AWQ / GPTQ / QuaRot) is mostly about handling the sensitive rows gracefully.

</details>

### 2.2 Activations

量化激活值可减少：

* 层间中间结果的 HBM 流量（收益不大 —— 激活值并非主要流量）。
* 若格式有硬件支持，还可提升 Tensor Core 吞吐（在 Hopper 上 FP8 相对 FP16 翻倍）。

敏感度是**逐张量或逐 token** 的 —— 激活值中的离群值（少数大值）会造成裁剪，并向外传播。方法（SmoothQuant、OmniQuant、QuaRot）的思路是把激活值离群值*重新分配*到权重中，或施加不变旋转。

**仅权重量化 INT4 + FP16 激活值是安全默认。** 在 Hopper / Blackwell 上，一旦精度一致性通过验证，FP8 权重 + FP8 激活值是更高性能的 recipe。

### 2.3 KV cache

量化 KV 可减少：

* KV cache 占用的 HBM 容量（可支持更长上下文或更大的批）。
* 每个 decode 步骤（逐 token 生成阶段）的 HBM 带宽（与上下文长度成正比）。

敏感度是**逐 head 与逐 position** 的 —— INT4 的 KV cache 量化通常需要逐 block 校准。FP8 KV 在逐张量或逐 head 缩放下通常是安全的。

注意 KV 的带宽开销**随上下文线性增长**。**在 128K 上下文下，KV cache 的读取常常比权重的读取带来更大的 HBM 开销。** 这就是为什么即便权重停留在 INT4，FP8 KV 在长上下文推理服务中也日益成为标准做法。

### 2.4 The three-axis recipe table

| 工作负载 | 权重 | 激活值 | KV cache | 备注 |
|----------|---------|-------------|----------|-------|
| Chat, 70B, 16K 上下文, H200 | INT4 (AWQ) | FP16 | FP16 | 安全默认 |
| Chat, 70B, 128K 上下文, H200 | INT4 (AWQ) | FP16 | FP8 | KV 压力迫使提高精度 |
| Chat, 70B, 128K 上下文, B200 | FP4 (MX-FP4) | FP4 | FP8 | 完整 Blackwell 栈 |
| 批 / 离线, 70B, H100 | FP8 | FP8 | FP8 | 吞吐优化 |
| 边缘, 4B, Jetson Orin Nano | INT4 (AWQ 或 Q4_K_M) | FP16 | FP16 | 内存紧张，KV 小 |
| Agent (BFCL 门控), 7B–70B | INT4 (AWQ) | FP16 | FP16 → FP8（精度一致性 OK 后） | 工具调用准确率是精度一致性门槛 |

recipe 只是起点。每个单元格在发布前都必须按 §5 做精度一致性验证。

---

## 3. 主流的仅权重量化方法

### 3.1 GPTQ —— 通过 OBS 实现训练后 INT4

[GPTQ](https://arxiv.org/abs/2210.17323) (2023) 使用二阶误差度量（Optimal Brain Surgeon）一次量化一层。它是经典的“首个实用 INT4”方法。

优点：

* 成熟；已随 `auto-gptq`、`vLLM`、`TensorRT-LLM`、`llama.cpp` 发布。
* 在没有严重离群行为的稠密模型上，精度一致性可预期。

缺点：

* 未显式处理激活值离群值；在激活值存在大幅离群通道的模型上质量下降。
* 可能对校准集的选择敏感。

### 3.2 AWQ —— 激活感知权重量化

[AWQ](https://arxiv.org/abs/2306.00978) (2023) 观察到一小部分权重通道（约 1%）对应激活值幅值较大的通道，并通过逐通道缩放保护它们。其余部分以 INT4 group-128 量化。

优点：

* 在 agent／工具调用工作负载上，精度一致性通常优于 GPTQ（[阶段 5 → 边缘 AI → BFCL Lecture](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) 中的讨论展示了这一点）。
* 推理快：其诀窍是在标准 INT4 矩阵乘 kernel 之上加的一层软件技巧。

缺点：

* 需要与部署分布匹配的校准数据。
* 对激活值量化没有帮助（它是仅权重量化方法）。

**当前稠密 LLM 上仅权重量化 INT4 的默认选择。** 第 2 部分第 03 讲会逐步走查 Llama 3.3 70B 与 Qwen 2.5 72B 上的 AWQ 流水线。

### 3.3 QuaRot —— 旋转不变量化

[QuaRot](https://arxiv.org/abs/2404.00456) (2024) 对模型施加不变旋转（Hadamard 矩阵），使激活值离群值均匀分散到所有通道。旋转之后，即便 W8A8（或 W4A8）量化在以往棘手的模型上也变得可行。

优点：

* 能处理 Llama-3-70B 这类在 W8A8 下单独用 GPTQ 和 AWQ 难以应对的模型（见 §4 中的异常）。
* 可启用低精度激活值，而不只是权重 —— 这在 Hopper FP8 / Blackwell FP4 上很重要。

缺点：

* 侵入性更强 —— 需要修改模型图以插入旋转矩阵。
* 较新，截至 2026 年中生态覆盖较少。

### 3.4 SpinQuant —— 可学习旋转

[SpinQuant](https://arxiv.org/abs/2405.16406) (2024) 扩展了 QuaRot，在校准集上*学习*旋转矩阵，而不是使用固定的 Hadamard。在相同精度下，通常比 QuaRot 取得更好的精度一致性。

优点：

* 在存在严重离群值的模型上，W4A4 的精度一致性为同类最佳。

缺点：

* 需要一个学习步骤；不是纯粹的训练后量化。
* 生态支持比 AWQ/GPTQ 薄弱。


<details>
<summary>English original</summary>

**2.2 Activations**

Quantizing activations cuts:

* Intermediate HBM traffic between layers (small win — activations are not the dominant traffic).
* Tensor core throughput if the format has hardware support (FP8 doubles vs FP16 on Hopper).

Sensitivity is **per-tensor or per-token** — outliers in the activations (a few large values) cause clipping that propagates. Methods (SmoothQuant, OmniQuant, QuaRot) work by *redistributing* activation outliers into the weights or by applying invariant rotations.

**Weight-only INT4 + FP16 activations is the safe default.** FP8 weights + FP8 activations is the higher-performance recipe on Hopper / Blackwell once parity is validated.

**2.3 KV cache**

Quantizing KV cuts:

* HBM capacity of the KV cache (allows longer context or larger batches).
* HBM bandwidth on every decode step (proportional to context length).

Sensitivity is **per-head and per-position** — KV cache quantization at INT4 typically requires per-block calibration. FP8 KV is usually safe with per-tensor or per-head scaling.

Note the bandwidth cost of KV **grows linearly with context**. **At 128K context the KV cache read is often a larger HBM cost than the weight read.** This is why FP8 KV is increasingly standard for long-context serving even when weights stay at INT4.

**2.4 The three-axis recipe table**

| Workload | Weights | Activations | KV cache | Notes |
|----------|---------|-------------|----------|-------|
| Chat, 70B, 16K context, H200 | INT4 (AWQ) | FP16 | FP16 | safe default |
| Chat, 70B, 128K context, H200 | INT4 (AWQ) | FP16 | FP8 | KV pressure forces precision |
| Chat, 70B, 128K context, B200 | FP4 (MX-FP4) | FP4 | FP8 | full Blackwell stack |
| Batch / offline, 70B, H100 | FP8 | FP8 | FP8 | throughput optimization |
| Edge, 4B, Jetson Orin Nano | INT4 (AWQ or Q4_K_M) | FP16 | FP16 | memory tight, KV small |
| Agent (BFCL-gated), 7B–70B | INT4 (AWQ) | FP16 | FP16 → FP8 once parity OK | tool-call accuracy is the parity bar |

The recipe is a starting point. Every cell must be parity-verified per §5 before shipping.

---

**3. The major weight-only quantization methods**

**3.1 GPTQ — Post-training INT4 via OBS**

[GPTQ](https://arxiv.org/abs/2210.17323) (2023) quantizes one layer at a time using a second-order error metric (Optimal Brain Surgeon). It is the canonical "first practical INT4" method.

Strengths:

* Mature; ships in `auto-gptq`, `vLLM`, `TensorRT-LLM`, `llama.cpp`.
* Predictable parity on dense models that don't have severe outlier behavior.

Weaknesses:

* No explicit handling of activation outliers; quality degrades on models where activations have large outlier channels.
* Can be sensitive to calibration set choice.

**3.2 AWQ — Activation-aware Weight Quantization**

[AWQ](https://arxiv.org/abs/2306.00978) (2023) observes that a small set of weight channels (~1%) corresponds to channels with large activation magnitudes, and protects them by per-channel scaling. Quantizes the rest at INT4 group-128.

Strengths:

* Generally better parity than GPTQ on agent/tool-use workloads (the BFCL discussion in [Phase 5 → Edge AI → BFCL Lecture](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) shows this).
* Fast inference: the trick is a software trick on top of standard INT4 matmul kernels.

Weaknesses:

* Requires calibration data that matches the deployment distribution.
* Doesn't help with activation quantization (it's a weight-only method).

**The current default for weight-only INT4 on dense LLMs.** Part 2 Lecture 03 walks the AWQ pipeline on Llama 3.3 70B and Qwen 2.5 72B step by step.

**3.3 QuaRot — Rotation-invariant quantization**

[QuaRot](https://arxiv.org/abs/2404.00456) (2024) applies an invariant rotation (Hadamard matrices) to the model so that activation outliers spread across all channels evenly. After rotation, even W8A8 (or W4A8) quantization becomes tractable on previously-difficult models.

Strengths:

* Handles models like Llama-3-70B that GPTQ and AWQ alone struggle with under W8A8 (see the anomaly in §4).
* Enables low-precision activations, not just weights — important on Hopper FP8 / Blackwell FP4.

Weaknesses:

* More invasive — requires modifying the model graph to insert the rotation matrices.
* Newer, less ecosystem coverage as of mid-2026.

**3.4 SpinQuant — Learnable rotations**

[SpinQuant](https://arxiv.org/abs/2405.16406) (2024) extends QuaRot by *learning* the rotation matrix on a calibration set rather than using a fixed Hadamard. Often achieves better parity than QuaRot at the same precision.

Strengths:

* Best-in-class parity for W4A4 on models with severe outliers.

Weaknesses:

* Requires a learning step; not pure post-training.
* Ecosystem support thinner than AWQ/GPTQ.

</details>

### 3.5 SmoothQuant — 激活值到权重的迁移

[SmoothQuant](https://arxiv.org/abs/2211.10438)（2022）通过把激活值离群点迁移到权重上将其「平滑」，从而实现 W8A8。在概念上是 QuaRot 的前身。

优势：

* TensorRT-LLM 与 DeepSpeed 支持良好。
* 适合 INT8/INT8。

劣势：

* 在更低精度下不如 QuaRot/SpinQuant 有效。

### 3.6 GGUF / K-quants / IQ-quants

llama.cpp 生态自带一族以 GGUF 格式存储的量化方案。常用的几种：

* **Q4_K_M** — 有效 4.5 bits/weight；事实上的边缘默认选项。
* **Q5_K_M** — 5.5 bits/weight；精度一致性更好，文件更大。
* **Q3_K_S / Q3_K_M** — 3-bit，激进；对生产环境通常太有损。
* **IQ4_XS / IQ3_M** — 更新的 "Improved Quants"，带重要性矩阵校准；相同位深下精度一致性更好。

GGUF/K-quants 是 weight-only 的，在数学层面与 AWQ/GPTQ 没有本质区别——区别在于工具链、格式，以及与 llama.cpp runtime 的集成。当要交付到 llama.cpp 部署时使用它们。

### 3.7 方法选型指南

| 约束 | 方法 |
|------------|--------|
| 服务器，dense 7B–70B，INT4，精度一致性已验证 | **AWQ**（从这里开始） |
| 服务器，dense 70B+，INT4，agent 工作负载 | 用 BFCL 校准的 **AWQ** |
| 服务器，低精度激活值（W4A8 / W4A4） | **QuaRot** 或 **SpinQuant** |
| 服务器，FP8 权重与激活值 | **FP8 原生**（NVIDIA TE） |
| 边缘（llama.cpp / Jetson） | **Q4_K_M** GGUF 或经 TRT-LLM 的 **AWQ-INT4** |
| Blackwell，完整 FP4 栈 | **TE2 原生 FP4** + per-block scaling |
| 激进：W3 / W2 | 非做不可就用 **SpinQuant + rotation**；通常该重新考虑模型规模 |

---

## 4. 已知异常

有两个具体异常值得了解：

### 4.1 Llama-3-70B 的 W8A8 敏感性

论文 *"The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization"*（[arXiv:2408.15301](https://arxiv.org/abs/2408.15301)）表明，Llama-3-70B（进而包括架构完全相同的 3.3-70B）在特定 MLP 通道中存在异常严重的激活值离群点。标准 SmoothQuant W8A8 在 MMLU 上产生 2–4 pp 的下降；W4A8 更差。

修复方法：使用 **QuaRot 或 SpinQuant**（基于旋转）——它们能正确处理离群点。或者停留在 **W4A16**（AWQ-INT4 权重 + FP16 激活值），这完全绕开了激活值量化问题。

Part 2 Lecture 03 会用 Llama 3.3 70B 上的具体数字讲解这一异常。

### 4.2 INT4 下 KV cache 的衰减

用 per-tensor scaling 把 KV cache 量化到 INT4，在长上下文任务（RULER、needle-in-haystack）上通常损失几个百分点。per-head 或 per-block scaling 能挽回其中的大部分。**务必在产品实际使用的上下文长度上验证 KV 量化**——短上下文的精度一致性并不意味着长上下文的精度一致性。

### 4.3 稀有 token 专门化 head 上的 FP8 KV

某些模型中有少量 attention head 专门处理稀有 token 位置（BOS、code-tokens）。FP8 per-tensor scaling 会把这些截断。per-head scaling 可修复。这是 2025 年年中至 2026 年 vLLM 与 SGLang 的 FP8 KV 实现中的已知问题。

---

## 5. 精度一致性验证方法论

**不可妥协的纪律**。每一次精度下调都需要一道关卡。

### 5.1 精度一致性契约

在任何精度下调之前：

1. **固定参考实现。** FP16/BF16 部署、确切的 tokenizer、确切的 prompt、确切的 seed 列表。对权重做哈希。这就是契约。
2. **固定评测集。** 一个与你的工作负载类别匹配的、固定且公开的评测集：
   * Chat：MMLU 子集（200 题）、GSM8K 子集、代码则用 HumanEval。
   * Agent：[BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md)，选你实际服务的类别。
   * 长上下文：RULER（4K → 128K）、needle-in-haystack。
   * Embedding：MTEB 子集。
3. **用不同 seed 跑两遍参考实现**——两次运行之间的方差就是你的噪声底。
4. **设定精度一致性预算** ≤ 3× 噪声底，并与产品可接受的下降幅度取交集。

### 5.2 运行候选方案

1. 隔离地应用精度下调（一次只动一个旋钮）。
2. 用同一评测集重新运行。
3. 按类别计算 Δ。超出预算的类别会阻塞该 recipe。
4. 检查失败样本：哪些以前通过的问题 / 请求现在失败了？抽样 20 个读一读。通常可以根据失败模式判断是哪个精度轴造成的（权重 vs 激活值 vs KV）。


<details>
<summary>English original</summary>

**3.5 SmoothQuant — Activation-to-weight migration**

[SmoothQuant](https://arxiv.org/abs/2211.10438) (2022) "smooths" activation outliers by migrating them to weights, enabling W8A8. Predecessor to QuaRot in concept.

Strengths:

* Well-supported in TensorRT-LLM and DeepSpeed.
* Good for INT8/INT8.

Weaknesses:

* Less effective than QuaRot/SpinQuant at lower precisions.

**3.6 GGUF / K-quants / IQ-quants**

The llama.cpp ecosystem ships its own family of quantizations stored in the GGUF format. The widely-used ones:

* **Q4_K_M** — 4.5 bits/weight effective; the de facto edge default.
* **Q5_K_M** — 5.5 bits/weight; better parity, larger files.
* **Q3_K_S / Q3_K_M** — 3-bit, aggressive; usually too lossy for production.
* **IQ4_XS / IQ3_M** — newer "Improved Quants" with importance-matrix calibration; better parity at the same bit depth.

GGUF/K-quants are weight-only and don't fundamentally differ from AWQ/GPTQ at the math level — they differ in tooling, format, and integration with the llama.cpp runtime. Use them when shipping into llama.cpp deployments.

**3.7 Method-picking guide**

| Constraint | Method |
|------------|--------|
| Server, dense 7B–70B, INT4, validated parity | **AWQ** (start here) |
| Server, dense 70B+, INT4, agent workload | **AWQ** with BFCL calibration |
| Server, low-precision activations (W4A8 / W4A4) | **QuaRot** or **SpinQuant** |
| Server, FP8 weights and activations | **FP8 native** (NVIDIA TE) |
| Edge (llama.cpp / Jetson) | **Q4_K_M** GGUF or **AWQ-INT4** via TRT-LLM |
| Blackwell, full FP4 stack | **TE2 native FP4** + per-block scaling |
| Aggressive: W3 / W2 | **SpinQuant + rotation** if you must; usually re-think the model size |

---

**4. The known anomalies**

Two specific anomalies worth knowing:

**4.1 Llama-3-70B W8A8 sensitivity**

The paper *"The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization"* ([arXiv:2408.15301](https://arxiv.org/abs/2408.15301)) showed that Llama-3-70B (and by extension 3.3-70B, which is architecturally identical) has unusually severe activation outliers in specific MLP channels. Standard SmoothQuant W8A8 produces a 2–4 pp drop on MMLU; W4A8 is worse.

The fix: use **QuaRot or SpinQuant** (rotation-based) — they handle the outliers correctly. Or stay at **W4A16** (AWQ-INT4 weights + FP16 activations), which dodges the activation-quantization problem entirely.

Part 2 Lecture 03 walks through this anomaly with concrete numbers on Llama 3.3 70B.

**4.2 KV cache rolloff at INT4**

KV cache quantization to INT4 with per-tensor scaling typically loses several percentage points on long-context tasks (RULER, needle-in-haystack). Per-head or per-block scaling recovers most of it. **Always validate KV quantization at the actual context length the product will use** — short-context parity does not imply long-context parity.

**4.3 FP8 KV on heads with rare-token specialization**

A small number of attention heads in some models specialize in rare-token positions (BOS, code-tokens). FP8 per-tensor scaling clips these. Per-head scaling fixes it. This is a known issue in mid-2025–2026 vLLM and SGLang FP8 KV implementations.

---

**5. The parity validation methodology**

The **non-negotiable discipline**. Every precision drop needs a gate.

**5.1 The parity contract**

Before any precision drop:

1. **Pin the reference.** The FP16/BF16 deployment, exact tokenizer, exact prompts, exact seed list. Hash the weights. This is the contract.
2. **Pin the eval set.** A fixed, public eval that matches your workload class:
   * Chat: MMLU subset (200 questions), GSM8K subset, HumanEval if code.
   * Agent: [BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) at the categories you serve.
   * Long context: RULER (4K → 128K), needle-in-haystack.
   * Embedding: MTEB subset.
3. **Run reference twice with different seeds** — the variance between runs is your noise floor.
4. **Set a parity budget** ≤ 3× the noise floor, intersected with the product-acceptable drop.

**5.2 Run the candidate**

1. Apply the precision drop in isolation (one knob at a time).
2. Re-run the same eval set.
3. Compute Δ per category. Categories that exceed budget block the recipe.
4. Inspect failures: which questions / requests now fail that used to pass? Sample 20 and read them. Often you can predict which precision axis is responsible (weights vs activations vs KV) by the failure pattern.

</details>

### 5.3 四种常见的精度一致性失效模式

| 症状 | 疑似轴 | 修复 |
|---------|--------------|-----|
| 工具调用准确率下降，MMLU 稳定 | 权重量化影响了稀有词表投影 | 逐通道权重缩放，校准数据包含工具使用样例 |
| 长上下文召回下降，短上下文正常 | KV cache 精度 | 逐 head KV 缩放，或保持 FP8 KV |
| 代码生成退化，对话正常 | MLP 中的激活值量化 | 改为仅权重量化，或使用 QuaRot |
| 特定 prompt 上出现随机尖峰 | 离群通道 | QuaRot / SpinQuant |

### 5.4 首要法则

**绝不要用工作负载定义指标上的精度一致性下降，去换取另一个指标上的吞吐提升。**

如果你的产品是 agent，BFCL 掉了 4 pp 换来 2× 的吞吐提升，这个 recipe 出厂就是事故。换一个 recipe。[VLA action-parity harness（agent 运行时框架）](../../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) 的纪律同样适用于 LLM——在精度一致性得到验证之前，任何优化都只是假设。

---

## 实验 — 量化 Qwen3-4B 并验证精度一致性

目标：在 Lectures 01–03 的 benchmark harness 基础上，扩展出一套量化精度一致性门控。

1. **参考运行** — BF16（官方发布的精度）下的 Qwen3-4B Instruct。跑一个小评测集（MMLU-200，如果有的话再加上 BFCL-50）。记录数值 + 两个 seed 之间的方差。
2. **候选 A — AWQ-INT4** 权重，FP16 激活值，FP16 KV。用 `auto-awq` 配合 256 条与部署分布匹配的校准 prompt 进行量化。重新跑评测。按类别计算 Δ。判断：是否在预算内？
3. **候选 B — FP8** 权重与激活值（TensorRT-LLM 或 vLLM 0.22+ 的 FP8 路径，如果你有 Hopper）。重新跑评测。
4. **候选 C — AWQ-INT4 权重，FP8 KV cache。** 重新跑评测，若适用则特别关注长上下文。
5. **产出一份精度一致性报告** — 每个候选一个 CSV，一份汇总 markdown。

通过标准：至少有一个候选在你定义的评测集上通过精度一致性预算，并且你已写下在什么产品约束下会发布哪一个。

---

## 自检

1. 你要在单张 A100 上部署一个 7B 模型（没有 FP8 张量核心）。最安全的起步精度 recipe 是什么？它拿不到哪些 FP8 在 H100 上能给你的东西？
2. 同事给你看一个 benchmark：W8A8 量化的 Llama 3 70B 比 FP16 快 1.6×，但 MMLU 掉了 3 pp。你坚持改用 QuaRot W4A8。用两句话为这个选择辩护。
3. 你在 Blackwell 上把模型量化到 FP4，工具调用准确率（BFCL）掉了 5 pp，而 MMLU 稳定。三个量化轴中哪个最可能是元凶，确认它的第一个实验是什么？
4. 你在 FP16 参考上测了两次精度一致性，得到 MMLU = 78.2 和 78.6。某个候选得 76.8。这在合理的精度一致性预算之内还是之外？写出计算过程。
5. 在 Hopper 上，针对并发 32 的对话工作负载，你要在 (a) AWQ-INT4 权重 + FP16 激活值与 (b) FP8 权重 + FP8 激活值之间选择。哪个的 decode（逐 token 生成阶段）吞吐上限更高，为什么？

---

## 参考文献

* GPTQ — [arXiv:2210.17323](https://arxiv.org/abs/2210.17323)
* AWQ — [arXiv:2306.00978](https://arxiv.org/abs/2306.00978)
* SmoothQuant — [arXiv:2211.10438](https://arxiv.org/abs/2211.10438)
* QuaRot — [arXiv:2404.00456](https://arxiv.org/abs/2404.00456)
* SpinQuant — [arXiv:2405.16406](https://arxiv.org/abs/2405.16406)
* OmniQuant — [arXiv:2308.13137](https://arxiv.org/abs/2308.13137)
* "The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization" — [arXiv:2408.15301](https://arxiv.org/abs/2408.15301)
* NVIDIA Transformer Engine 文档 — [docs.nvidia.com/deeplearning/transformer-engine/](https://docs.nvidia.com/deeplearning/transformer-engine/)
* MX（microscaling）FP4 / FP6 规范 — Open Compute Project，2023+ — [opencompute.org](https://www.opencompute.org/)
* llama.cpp GGUF / IQ-quants 参考 — [github.com/ggml-org/llama.cpp/wiki](https://github.com/ggml-org/llama.cpp/wiki)

交叉引用：

* [阶段 5 → 边缘 AI → Qwen Inference Optimization → Lecture 02 — Quantizing Qwen3-4B to Q4](../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md)
* [阶段 5 → 边缘 AI → Agent Tool-Dispatch Evaluation with BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) — 精度一致性门控
* [阶段 4 → 方向 C → Quantization](../../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md) — 编译器侧基础

---

## 截至 2026-06 的最新情况

截至 2026 年中的方法清单：INT4/W4A8 用 AWQ、GPTQ、QuaRot、SpinQuant。Hopper/Blackwell 上用 FP8（E4M3、E5M2）。Blackwell 上用 FP4（MX-FP4）。当出现新的量化方法在 agent 工作负载上精度一致性优于 AWQ，或 FP6 / FP3 在硬件上落地时，更新本清单。

---


<details>
<summary>English original</summary>

**5.3 The four common parity failure modes**

| Symptom | Suspect axis | Fix |
|---------|--------------|-----|
| Tool-call accuracy drops, MMLU stable | Weight quantization touched rare-vocabulary projections | Per-channel weight scaling, calibration data with tool-use examples |
| Long-context recall drops, short-context fine | KV cache precision | Per-head KV scaling, or stay at FP8 KV |
| Code generation regresses, chat fine | Activation quantization in MLP | Switch to weight-only, or use QuaRot |
| Random spikes on specific prompts | Outlier channels | QuaRot / SpinQuant |

**5.4 The cardinal rule**

**Never trade a parity drop in the workload-defining metric for a throughput gain in a different metric.**

If your product is an agent and BFCL drops 4 pp for a 2× throughput gain, the recipe ships incidents. Pick a different recipe. The discipline of the [VLA action-parity harness](../../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) applies to LLMs too — every optimization is hypothetical until parity is verified.

---

**Lab — quantize Qwen3-4B and validate parity**

Goal: extend the benchmark harness from Lectures 01–03 with a quantization parity gate.

1. **Reference run** — Qwen3-4B Instruct at BF16 (the published precision). Run a small eval set (MMLU-200, BFCL-50 if you have it). Record numbers + variance across two seeds.
2. **Candidate A — AWQ-INT4** weights, FP16 activations, FP16 KV. Quantize using `auto-awq` with 256 calibration prompts that match your deployment distribution. Re-run eval. Compute Δ per category. Decide: within budget?
3. **Candidate B — FP8** weights and activations (TensorRT-LLM or vLLM 0.22+ FP8 path, if you have Hopper). Re-run eval.
4. **Candidate C — AWQ-INT4 weights, FP8 KV cache.** Re-run eval, especially at long context if applicable.
5. **Produce a parity report** — one CSV per candidate, one summary markdown.

Pass criterion: at least one candidate passes the parity budget on your defined eval set, and you have written down which one would ship under what product constraints.

---

**Self-check**

1. You are deploying a 7B model on a single A100 (no FP8 tensor cores). What is the safest precision recipe to start with? What does it not buy you that FP8 would on H100?
2. A teammate shows you a benchmark where W8A8 quantized Llama 3 70B is 1.6× faster than FP16 but loses 3 pp on MMLU. You insist on QuaRot W4A8 instead. Defend the choice in two sentences.
3. You quantize a model to FP4 on Blackwell and tool-call accuracy (BFCL) drops 5 pp while MMLU is stable. Which of the three quantization axes is the likely culprit, and what is the first experiment to confirm?
4. You measure parity twice on the FP16 reference and get MMLU = 78.2 and 78.6. A candidate scores 76.8. Is this within or outside a reasonable parity budget? Show the math.
5. On Hopper, you are choosing between (a) AWQ-INT4 weights + FP16 activations and (b) FP8 weights + FP8 activations for a chat workload at concurrency 32. Which has the higher decode throughput ceiling, and why?

---

**References**

* GPTQ — [arXiv:2210.17323](https://arxiv.org/abs/2210.17323)
* AWQ — [arXiv:2306.00978](https://arxiv.org/abs/2306.00978)
* SmoothQuant — [arXiv:2211.10438](https://arxiv.org/abs/2211.10438)
* QuaRot — [arXiv:2404.00456](https://arxiv.org/abs/2404.00456)
* SpinQuant — [arXiv:2405.16406](https://arxiv.org/abs/2405.16406)
* OmniQuant — [arXiv:2308.13137](https://arxiv.org/abs/2308.13137)
* "The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization" — [arXiv:2408.15301](https://arxiv.org/abs/2408.15301)
* NVIDIA Transformer Engine documentation — [docs.nvidia.com/deeplearning/transformer-engine/](https://docs.nvidia.com/deeplearning/transformer-engine/)
* MX (microscaling) FP4 / FP6 specification — Open Compute Project, 2023+ — [opencompute.org](https://www.opencompute.org/)
* llama.cpp GGUF / IQ-quants reference — [github.com/ggml-org/llama.cpp/wiki](https://github.com/ggml-org/llama.cpp/wiki)

Cross-references:

* [Phase 5 → Edge AI → Qwen Inference Optimization → Lecture 02 — Quantizing Qwen3-4B to Q4](../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md)
* [Phase 5 → Edge AI → Agent Tool-Dispatch Evaluation with BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) — parity gating
* [Phase 4 → Track C → Quantization](../../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md) — compiler-side foundation

---

**Current as of 2026-06**

Methods pinned as of mid-2026: AWQ, GPTQ, QuaRot, SpinQuant for INT4/W4A8. FP8 (E4M3, E5M2) on Hopper/Blackwell. FP4 (MX-FP4) on Blackwell. Refresh when a new quant method ships with better-than-AWQ parity on agent workloads, or when FP6 / FP3 lands in hardware.

---

</details>

## Next

* Next: [Lecture 05 — The runtime landscape](Lecture-05.md)
* Previous: [Lecture 03 — Roofline, bandwidth, and the memory hierarchy](Lecture-03.md)
* Up: [Part 1 — Fundamentals](README.md)


<details>
<summary>English original</summary>

**Next**

* Next: [Lecture 05 — The runtime landscape](Lecture-05.md)
* Previous: [Lecture 03 — Roofline, bandwidth, and the memory hierarchy](Lecture-03.md)
* Up: [Part 1 — Fundamentals](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 1 - Fundamentals/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%201%20-%20Fundamentals/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
