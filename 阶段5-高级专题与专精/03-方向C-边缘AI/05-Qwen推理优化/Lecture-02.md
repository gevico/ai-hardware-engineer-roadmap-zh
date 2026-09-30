---
title: 第 2 讲：把 Qwen3-4B 量化到 Q4 —— AWQ、GPTQ、K-Quants 与每权重字节数的取舍
description: 第 2 讲：把 Qwen3-4B 量化到 Q4 —— AWQ、GPTQ、K-Quants 与每权重字节数的取舍
---

# 第 2 讲：把 Qwen3-4B 量化到 Q4 —— AWQ、GPTQ、K-Quants 与每权重字节数的取舍

## 概述

BF16 下的 Qwen3-4B 在磁盘上占 **~7.6 GB**。Orin Nano 8 GB 在扣除 OS、CUDA 上下文、KV cache 和临时缓冲之后，大约只剩 4 GB 可用 DRAM。模型放不下。要么压缩它，要么就跑不起来。

本讲是专门针对 Qwen3-4B 的**推理量化**实战手册：哪些格式可用、Qwen 特有的权重统计特性如何影响选型、Q4_K_M 凭什么坐稳默认位置，以及 AWQ 与 GPTQ 的做法差异在 4B 这个规模上为何重要。本讲**不**涉及训练量化（QAT）——只讨论面向推理的训练后、仅权重量化。

读完本讲，你应当能够：

* 对 Qwen3-4B 的任意量化选择，算出其磁盘占用与 DRAM 占用。
* 在 Q4_0、Q4_K_M、Q5_K_M、AWQ-4bit、GPTQ-4bit 之间，按目标 tok/s 与质量做取舍。
* 跑一轮校准 pass 并完成验证，而不必在评测上耗掉一周。
* 解释在 K-quant 家族中，为什么 V 与 FFN-down 会被升级到 Q6_K。

---

## 1. 为什么要量化：带宽这笔账

Orin Nano 上的 4B 级模型：

| 格式 | 每权重比特数 | 磁盘 | DRAM（权重） | Roofline tok/s @ 50 GB/s |
|---|---|---|---|---|
| FP32 | 32 | 15.3 GB | 放不下 | — |
| BF16 / FP16 | 16 | 7.6 GB | 放不下 | — |
| Q8_0 | ~8.5 | 4.1 GB | 4.1 GB（紧张） | ~12 |
| Q6_K | ~6.6 | 3.2 GB | 3.2 GB | ~15 |
| Q5_K_M | ~5.6 | 2.7 GB | 2.7 GB | ~18 |
| **Q4_K_M** | **~4.6** | **2.4 GB** | **2.4 GB** | **~21** |
| Q4_0 | ~4.5 | 2.3 GB | 2.3 GB | ~21 |
| Q3_K_M | ~3.9 | 2.1 GB | 2.1 GB | ~24 |
| Q2_K | ~3.0 | 1.7 GB | 1.7 GB | ~29 |
| IQ2_XS | ~2.4 | 1.4 GB | 1.4 GB | ~35 |

roofline（性能上界模型）为 `bandwidth / bytes_per_token`。对 4B 级模型，低于 ~Q4 质量会快速下滑；高于 Q5 则是在为收益递减的困惑度而多付带宽。**Q4_K_M 是最佳平衡点**——也正是你的日志中 JLLM 所运行的格式。

这笔账很残酷，但也很清楚：每权重每减少一个比特，在 Orin Nano 上就能换来约 1 tok/s。事情的关键全在这里。

---

## 2. 量化格式大观

在 Qwen3-4B 部署中，三大格式家族占据主导：

### 2.1 ggml K-quants（Q*_K_M 家族）

两级分块布局：

```
Superblock: 256 weights
  ├── shared FP16 scale (s_super)
  ├── shared FP16 min   (m_super)
  └── 16 sub-blocks of 16 weights each
        ├── small per-sub-block scale (4–6 bits, packed)
        └── small per-sub-block min  (4–6 bits, packed)
              └── 16 quantized weights (3, 4, 5, or 6 bits each)
```

其中 “K” 代表按子块进行 **K-means** 式的最优 scale 选择——并非真正的 K-means，而是一种最小化块级重建误差的数值优化。

`_M` 后缀表示**逐矩阵混合精度**：关键张量（FFN-down、V projection、最靠近残差的那一半 attention）会被提到更高的比特率。标准的 Qwen3-4B-Q4_K_M 布局——正是你的 JLLM 日志所显示的：

```
q=Q4_K  k=Q4_K  v=Q6_K  o=Q4_K  gate=Q4_K  up=Q4_K  down=Q6_K
```

V 与 FFN-down 用 Q6_K，是因为经验上这些张量对质量影响最大。仅把 FFN-down 量化到 Q4，在 Qwen3-4B 上就要付出约 0.3 个困惑度的代价；仅量化 V 则约 0.15。这两个张量用 Q6 带来的带宽代价很小（它们在参数总量中只占少数），因此混合 recipe 在两个维度上都占优。

### 2.2 AWQ —— 激活感知权重量化

AWQ 的洞见在于：**有离群值的是激活值，而不是权重**。因此在量化权重时，应当更谨慎地保护那些会与大数值激活值相乘的维度。

算法一段话讲完：

1. 在校准集上收集激活统计：按输入通道求 `|act_i|`，在数百个样本上取平均。
2. 对每个权重矩阵 `W`，求一个逐通道的 **scale 向量** `s`，使得等价计算 `y = (x / s) · (W · diag(s))` 把数值幅度从“对离群值敏感”的通道重新分配到 scale 中。
3. 把缩放后的 `W · diag(s)` 量化到 4-bit。scale 以 FP16 随权重一同保存。

输出是一个 4-bit 权重矩阵，外加逐通道 scale。在 Qwen 上，AWQ-4bit 在 **7B 以上**的大多数评测中都稳定优于 GPTQ-4bit 和 Q4_K_M，而在 4B 规模下优势更小（有时打平）。该格式比 ggml K-quants 更 GPU 友好，因为其 kernel 是一个干净的融合反量化 matmul，无需子块记账。

### 2.3 GPTQ —— 最优脑量化

GPTQ 拿一个校准集，**逐列**量化权重，并更新其余列以补偿刚量化那一列引入的量化误差。更新量用到该层 MSE 损失关于权重的 **Hessian 逆**。

GPTQ-4bit 的质量接近 AWQ。该格式在 kernel 派发上更难处理（逐组 scale、非对称零点），但生态支持极好——vLLM、TGI、exllamav2 与 Marlin kernel 都原生支持 GPTQ。


<details>
<summary>English original</summary>

**Lecture 2: Quantizing Qwen3-4B to Q4 — AWQ, GPTQ, K-Quants, and the Bytes-per-Weight Trade**

**Overview**

Qwen3-4B at BF16 is **~7.6 GB** on disk. An Orin Nano 8 GB has roughly 4 GB of free DRAM after the OS, CUDA context, KV cache, and scratch. The model doesn't fit. Either you shrink it or you don't run it.

This lecture is the **inference-quantization** playbook for Qwen3-4B specifically: which formats work, how Qwen's particular weight statistics shape the choice, where Q4_K_M earned its default-status, and what AWQ and GPTQ do differently that matters at this scale. We do **not** cover training quantization (QAT) — only post-training, weight-only quantization for inference.

By the end you should be able to:

* Compute on-disk and DRAM footprint for any quant choice on Qwen3-4B.
* Pick between Q4_0, Q4_K_M, Q5_K_M, AWQ-4bit, GPTQ-4bit for a target tok/s and quality.
* Run a calibration pass and validate without burning a week on eval.
* Explain why V and FFN-down are upgraded to Q6_K in the K-quant family.

---

**1. Why Quantize: The Bandwidth Math**

A 4B-class model on Orin Nano:

| Format | Bits/weight | Disk | DRAM (weights) | Roofline tok/s @ 50 GB/s |
|---|---|---|---|---|
| FP32 | 32 | 15.3 GB | doesn't fit | — |
| BF16 / FP16 | 16 | 7.6 GB | doesn't fit | — |
| Q8_0 | ~8.5 | 4.1 GB | 4.1 GB (tight) | ~12 |
| Q6_K | ~6.6 | 3.2 GB | 3.2 GB | ~15 |
| Q5_K_M | ~5.6 | 2.7 GB | 2.7 GB | ~18 |
| **Q4_K_M** | **~4.6** | **2.4 GB** | **2.4 GB** | **~21** |
| Q4_0 | ~4.5 | 2.3 GB | 2.3 GB | ~21 |
| Q3_K_M | ~3.9 | 2.1 GB | 2.1 GB | ~24 |
| Q2_K | ~3.0 | 1.7 GB | 1.7 GB | ~29 |
| IQ2_XS | ~2.4 | 1.4 GB | 1.4 GB | ~35 |

The roofline is `bandwidth / bytes_per_token`. Below ~Q4 the quality drops fast for a 4B-class model; above Q5 you're paying bandwidth for diminishing perplexity gains. **Q4_K_M is the sweet spot** — and it's also what JLLM was running in your log.

The math is brutal but clarifying: every bit per weight you remove buys you ~1 tok/s on Orin Nano. That's the entire game.

---

**2. The Quantization Format Zoo**

Three families dominate Qwen3-4B deployment:

**2.1 ggml K-quants (Q*_K_M family)**

Two-level block layout:

```
Superblock: 256 weights
  ├── shared FP16 scale (s_super)
  ├── shared FP16 min   (m_super)
  └── 16 sub-blocks of 16 weights each
        ├── small per-sub-block scale (4–6 bits, packed)
        └── small per-sub-block min  (4–6 bits, packed)
              └── 16 quantized weights (3, 4, 5, or 6 bits each)
```

The "K" stands for **K-means**-style optimal scale selection per sub-block — not a real K-means, but a numerical optimization that minimizes block-level reconstruction error.

`_M` suffix means **mixed precision per matrix**: critical tensors (FFN-down, V projection, the half of attention closest to the residual) get bumped up to a higher bit-rate. The standard Qwen3-4B-Q4_K_M layout — exactly what your JLLM log showed:

```
q=Q4_K  k=Q4_K  v=Q6_K  o=Q4_K  gate=Q4_K  up=Q4_K  down=Q6_K
```

V and FFN-down get Q6_K because empirically those tensors carry the most quality. Quantizing FFN-down to Q4 alone costs ~0.3 perplexity points on Qwen3-4B; quantizing only V costs ~0.15. The bandwidth penalty of Q6 for those two tensors is small (they're a minority of the parameter count), so the mixed recipe wins on both axes.

**2.2 AWQ — Activation-aware Weight Quantization**

AWQ's insight: **the activations have outliers**, not the weights. So when you quantize weights, you should protect the dimensions that get multiplied by large-magnitude activations more carefully.

Algorithm in one paragraph:

1. Collect activation statistics on a calibration set: `|act_i|` per input channel, averaged across a few hundred samples.
2. For each weight matrix `W`, find a per-channel **scale vector** `s` such that the equivalent computation `y = (x / s) · (W · diag(s))` redistributes magnitude from "outlier-sensitive" channels into the scale.
3. Quantize the scaled `W · diag(s)` to 4-bit. The scales travel as FP16 alongside.

Output is a 4-bit weight matrix plus per-channel scales. On Qwen, AWQ-4bit reliably beats both GPTQ-4bit and Q4_K_M on most evals **at 7B+**, with a smaller margin (sometimes a tie) at the 4B scale. The format is more GPU-friendly than ggml K-quants because the kernel is a clean fused-dequant matmul without sub-block bookkeeping.

**2.3 GPTQ — Optimal Brain Quantization**

GPTQ takes a calibration set and quantizes weights **column by column**, updating remaining columns to compensate for quantization error in the column just quantized. The update uses the **inverse Hessian** of the layer's MSE loss with respect to weights.

GPTQ-4bit gives near-AWQ quality. The format is uglier to kernel-dispatch on (group-wise scales, asymmetric zero points) but enjoys excellent ecosystem support — vLLM, TGI, exllamav2, and Marlin kernels all consume GPTQ natively.

</details>

### 2.4 Qwen3-4B 的横向对比

假设是「Q4 家族」。以下数字近似取自公开的 Qwen3-4B benchmark（2026 年 5 月，MMLU + IFEval 综合，你的结果会有所不同）：

| 格式 | 有效 bpw | 相对 BF16 的 MMLU 下降 | 磁盘 | 最佳 runtime |
|---|---|---|---|---|
| BF16（参考） | 16 | — | 7.6 GB | vLLM, transformers |
| Q8_0 | 8.5 | < 0.05 | 4.1 GB | llama.cpp |
| Q6_K | 6.6 | < 0.1 | 3.2 GB | llama.cpp |
| Q5_K_M | 5.6 | 0.1–0.2 | 2.7 GB | llama.cpp |
| **Q4_K_M** | **4.6** | **0.3–0.5** | **2.4 GB** | **llama.cpp, JLLM** |
| AWQ-int4 (g128) | 4.25 | 0.2–0.4 | 2.3 GB | vLLM, TRT-LLM, SGLang |
| GPTQ-int4 (g128) | 4.25 | 0.3–0.5 | 2.3 GB | vLLM, exllamav2, Marlin |
| IQ4_XS | 4.25 | 0.4–0.7 | 2.2 GB | llama.cpp |
| Q3_K_M | 3.9 | 0.8–1.2 | 2.1 GB | llama.cpp |
| Q2_K | 3.0 | 2.5–3.5 | 1.7 GB | llama.cpp（勉强可用） |

Q4 以下，指令遵循的退化比聚合指标所显示的更快 —— Q2 的 Qwen3-4B 技术上仍会「作答」，但经常漏掉多步指令中的部分内容。

---

## 3. 为什么 V 和 FFN-Down 会被升级

经验法则：**误差会沿 residual stream 累积的张量，对质量最为敏感**。

```
attention block:    x ── norm ── QKV ── attn ── O ── ⊕ ──► x'
                                                     │
                              residual goes here ────┘

FFN block:          x' ── norm ── gate/up ── SwiGLU ── down ── ⊕ ──► x''
                                                                │
                                          residual goes here ───┘
```

注入到 `O` 和 `down` 的误差会直接加到 residual 上，并被带到之后的每一个 layer。`Q` 和 `gate` 处的误差会被下游非线性（softmax、SiLU）部分「吸收」—— 不过说「吸收」是抬举了，它只是没那么灾难性而已。

为 V 选择 Q6_K 的理由略有不同：V 是 attention 真正的内容通路。Q 和 K 只决定**看哪里**；V 才是**你拿到什么**。有噪声的 V 对输出的毒害比有噪声的 Q 或 K 更直接（Q、K 后面跟着 softmax —— attention score 的小扰动只是 attention 权重的小扰动）。

这些如今已是开放权重社区里众所周知的规则，在 Llama、Mistral、Phi、Gemma —— 以及 Qwen 的 `*_K_M` 量化配置中都能看到同样的模式（升级 V 和 FFN-down）。

---

## 4. 实用工作流

视 runtime 不同有两条路径：

### 4.1 路径 A — 面向 JLLM、llama.cpp、MLC 的 llama.cpp / K-quants

```bash
# Starting from HF safetensors:
git lfs clone https://huggingface.co/Qwen/Qwen3-4B-Instruct
cd Qwen3-4B-Instruct

# Step 1: convert to GGUF FP16 (intermediate)
python -m llama_cpp.convert_hf_to_gguf . \
    --outfile qwen3-4b-fp16.gguf \
    --outtype f16

# Step 2: quantize to Q4_K_M
./quantize qwen3-4b-fp16.gguf qwen3-4b-q4_k_m.gguf Q4_K_M

# Optional: with importance matrix for slightly better quality
./imatrix -m qwen3-4b-fp16.gguf -f calibration.txt -o qwen3.imatrix
./quantize --imatrix qwen3.imatrix qwen3-4b-fp16.gguf qwen3-4b-q4_k_m.gguf Q4_K_M
```

中间产物 FP16 GGUF 约 7.6 GB；最终 Q4_K_M 约 2.4 GB。如果打算尝试其他量化档位，就把 FP16 留着 —— 重新量化很快（现代 CPU 上约 30 s）；从 HF safetensors 重新转换则很慢且占磁盘。

### 4.2 路径 B — 面向 vLLM / TRT-LLM / SGLang 的 AWQ

```bash
pip install autoawq

python -c "
from awq import AutoAWQForCausalLM
from transformers import AutoTokenizer
model_path = 'Qwen/Qwen3-4B-Instruct'
quant_path = './qwen3-4b-awq'

tokenizer = AutoTokenizer.from_pretrained(model_path, trust_remote_code=True)
model = AutoAWQForCausalLM.from_pretrained(model_path, trust_remote_code=True, safetensors=True)

quant_config = {
    'zero_point': True,
    'q_group_size': 128,
    'w_bit': 4,
    'version': 'GEMM',  # 'GEMM' for vLLM; 'GEMV' for inference-only kernels
}
model.quantize(tokenizer, quant_config=quant_config)
model.save_quantized(quant_path)
tokenizer.save_pretrained(quant_path)
"
```

校准默认使用 128 个 pile/c4 样本。对 Qwen 来说，**与领域匹配的校准集明显更好** —— 如果下游是对话，就用对话风格的数据校准；如果是代码，就用代码校准。HuggingFace 上公开的 Qwen-AWQ 发布版通常使用对话风格的校准混合数据。

benchmark 中的 `g128` 记法表示 group size 128 —— 量化 scale 沿一行在 128 个权重间共享。更小的 group（32、64）以存储开销换取质量。

---


<details>
<summary>English original</summary>

**2.4 Side-by-side for Qwen3-4B**

Assume "Q4 family". Approximate numbers from public Qwen3-4B benchmarks (May 2026, MMLU + IFEval composite, your mileage will vary):

| Format | Effective bpw | MMLU drop vs BF16 | Disk | Best runtime |
|---|---|---|---|---|
| BF16 (reference) | 16 | — | 7.6 GB | vLLM, transformers |
| Q8_0 | 8.5 | < 0.05 | 4.1 GB | llama.cpp |
| Q6_K | 6.6 | < 0.1 | 3.2 GB | llama.cpp |
| Q5_K_M | 5.6 | 0.1–0.2 | 2.7 GB | llama.cpp |
| **Q4_K_M** | **4.6** | **0.3–0.5** | **2.4 GB** | **llama.cpp, JLLM** |
| AWQ-int4 (g128) | 4.25 | 0.2–0.4 | 2.3 GB | vLLM, TRT-LLM, SGLang |
| GPTQ-int4 (g128) | 4.25 | 0.3–0.5 | 2.3 GB | vLLM, exllamav2, Marlin |
| IQ4_XS | 4.25 | 0.4–0.7 | 2.2 GB | llama.cpp |
| Q3_K_M | 3.9 | 0.8–1.2 | 2.1 GB | llama.cpp |
| Q2_K | 3.0 | 2.5–3.5 | 1.7 GB | llama.cpp (usable barely) |

Below Q4, instruction following degrades faster than aggregate metrics suggest — Qwen3-4B at Q2 will technically "answer" but routinely drops parts of multi-step instructions.

---

**3. Why V and FFN-Down Get Upgraded**

The empirical rule: **the tensors whose error compounds along the residual stream are the most quality-sensitive**.

```
attention block:    x ── norm ── QKV ── attn ── O ── ⊕ ──► x'
                                                     │
                              residual goes here ────┘

FFN block:          x' ── norm ── gate/up ── SwiGLU ── down ── ⊕ ──► x''
                                                                │
                                          residual goes here ───┘
```

Error injected at `O` and `down` is added directly to the residual and carried through every subsequent layer. Error at `Q` and `gate` is partially "absorbed" by downstream nonlinearities (softmax, SiLU) — though "absorbed" is generous, it's just less catastrophic.

The Q6_K choice for V comes from a slightly different argument: V is the actual content path of attention. Q and K only determine **where** to look; V is **what** you get. Noisy V poisons the output more directly than noisy Q or K (which are followed by softmax — small perturbations in attention scores are small perturbations in attention weights).

These are now well-known rules in the open-weights community and you see the same pattern (V and FFN-down upgraded) in `*_K_M` quant configurations across Llama, Mistral, Phi, Gemma — and Qwen.

---

**4. The Practical Workflow**

Two paths depending on runtime:

**4.1 Path A — llama.cpp / K-quants for JLLM, llama.cpp, MLC**

```bash
# Starting from HF safetensors:
git lfs clone https://huggingface.co/Qwen/Qwen3-4B-Instruct
cd Qwen3-4B-Instruct

# Step 1: convert to GGUF FP16 (intermediate)
python -m llama_cpp.convert_hf_to_gguf . \
    --outfile qwen3-4b-fp16.gguf \
    --outtype f16

# Step 2: quantize to Q4_K_M
./quantize qwen3-4b-fp16.gguf qwen3-4b-q4_k_m.gguf Q4_K_M

# Optional: with importance matrix for slightly better quality
./imatrix -m qwen3-4b-fp16.gguf -f calibration.txt -o qwen3.imatrix
./quantize --imatrix qwen3.imatrix qwen3-4b-fp16.gguf qwen3-4b-q4_k_m.gguf Q4_K_M
```

The intermediate FP16 GGUF is ~7.6 GB; the final Q4_K_M is ~2.4 GB. Keep the FP16 around if you plan to try other quant levels — re-quantizing is fast (~30 s on a modern CPU); re-converting from HF safetensors is slow and disk-heavy.

**4.2 Path B — AWQ for vLLM / TRT-LLM / SGLang**

```bash
pip install autoawq

python -c "
from awq import AutoAWQForCausalLM
from transformers import AutoTokenizer
model_path = 'Qwen/Qwen3-4B-Instruct'
quant_path = './qwen3-4b-awq'

tokenizer = AutoTokenizer.from_pretrained(model_path, trust_remote_code=True)
model = AutoAWQForCausalLM.from_pretrained(model_path, trust_remote_code=True, safetensors=True)

quant_config = {
    'zero_point': True,
    'q_group_size': 128,
    'w_bit': 4,
    'version': 'GEMM',  # 'GEMM' for vLLM; 'GEMV' for inference-only kernels
}
model.quantize(tokenizer, quant_config=quant_config)
model.save_quantized(quant_path)
tokenizer.save_pretrained(quant_path)
"
```

Calibration uses 128 samples of pile/c4 by default. For Qwen, a **domain-matched calibration set is meaningfully better** — if your downstream is chat, calibrate on chat-style data; if it's code, calibrate on code. Public Qwen-AWQ releases on HuggingFace typically use a chat-style calibration mix.

The `g128` notation in benchmarks means group size 128 — quantization scales are shared across 128 weights along a row. Smaller groups (32, 64) buy quality at storage cost.

---

</details>

## 5. 校准集 —— 90 秒的决策

校准集是 AWQ/GPTQ/imatrix 用来测量 **激活统计量** 的语料。经验法则：

| 目标 | 校准集 |
|---|---|
| 通用聊天助手 | OpenAssistant + ShareGPT 混合（约 512 条样本 × 2 k token） |
| 代码助手 | Stack-V2 子集，挑你关心的语言 |
| 多语言（中文 + 英文） | mC4 + Wikipedia-ZH + Wikipedia-EN |
| 长上下文 | Books-3 或同人小说，各切成约 8 k |
| 领域专用（医疗、法律等） | 该领域的公开语料 |

常见的坑：

* **太小**（<32 条样本）—— 校准会过拟合到少数激活模式。
* **太窄** —— 只在英文上校准会明显损害中文表现。
* **太短** —— 短于约 256 token 的样本无法检验长程 attention。
* **校准数据里带 `<think>` 块** —— 对 Qwen3，先决定你要模型擅长 thinking-mode 还是 chat-mode，再据此挑校准数据。混合用也行，但会被占多数的那一类主导。

---

## 6. 验证量化结果

两项廉价检查，一项昂贵检查：

### 6.1 健全性检查（60 秒）

```python
from llama_cpp import Llama

m = Llama(model_path="qwen3-4b-q4_k_m.gguf",
          n_ctx=2048, n_gpu_layers=99)

for prompt in [
    "Write a one-sentence summary of quantum entanglement.",
    "用一句话解释量子纠缠。",
    "def fibonacci(n):",
    "Solve: if x + 3 = 7, what is x?",
]:
    out = m(prompt, max_tokens=64, temperature=0.0)
    print(out["choices"][0]["text"])
```

如果模型输出胡言乱语、自我重复、在回答中途切换语言，或对无害提示词拒答 —— 说明有地方坏了（通常是 tokenizer 或 chat-template 的 bug，有时是 RoPE 布局，偶尔是量化文件损坏）。

### 6.2 困惑度扫描（10 分钟）

```bash
./perplexity -m qwen3-4b-q4_k_m.gguf -f wikitext-2-raw/wiki.test.raw -c 2048
```

与 FP16 基线对比。在 wiki 文本上，Q4_K_M 的困惑度通常落在 FP16 的 +0.05 到 +0.15 之间。偏差更大就说明有问题 —— 通常是校准没做好，或者你不小心把 LM head 也量化了。

### 6.3 下游评测（1–6 GPU 小时）

`lm-eval-harness`，配一套 Qwen 友好的测试套件：

```bash
lm-eval --model gguf \
    --model_args pretrained=qwen3-4b-q4_k_m.gguf \
    --tasks mmlu,ifeval,humaneval,gsm8k \
    --batch_size 1 \
    --num_fewshot 0
```

Q4_K_M Qwen3-4B 相对 BF16 的目标：

* MMLU：下降 < 0.5 pts
* IFEval（指令遵循）：下降 < 1.5 pts（这是对 Q4 最敏感的指标）
* GSM8K（数学）：下降 < 2 pts
* HumanEval（代码）：下降 < 1 pt

如果 IFEval 下降 > 3 pts，说明校准集要么太小要么太窄 —— 换更宽的混合重新跑。

---

## 7. 存储布局 —— Qwen3-4B-Q4_K_M 的 GGUF

GGUF 文件是单个 blob，结构如下：

```
┌─────────────────────────────────────┐
│ Magic "GGUF" + version (4 bytes)    │
├─────────────────────────────────────┤
│ Tensor count (8 bytes)              │
│ Metadata KV count (8 bytes)         │
├─────────────────────────────────────┤
│ Metadata KV table                   │
│   - general.name = "Qwen3 4B …"     │
│   - general.architecture = "qwen3"  │
│   - qwen3.context_length = 40960    │
│   - qwen3.embedding_length = 2560   │
│   - qwen3.block_count = 36          │
│   - qwen3.attention.head_count = 32 │
│   - qwen3.attention.head_count_kv=8 │
│   - qwen3.feed_forward_length = 6912│
│   - qwen3.rope.freq_base = 1000000  │
│   - tokenizer.ggml.tokens = [...]   │
│   - tokenizer.ggml.merges = [...]   │
│   - ...                             │
├─────────────────────────────────────┤
│ Tensor info table (name, dtype,     │
│   shape, offset for each tensor)    │
├─────────────────────────────────────┤
│ Padding to alignment                │
├─────────────────────────────────────┤
│ Tensor data — raw quantized blocks  │
│   token_embd (Q6_K block stream)    │
│   blk.0.attn_norm (F32)             │
│   blk.0.attn_q (Q4_K block stream)  │
│   blk.0.attn_q.bias (F32)           │
│   blk.0.attn_k (Q4_K block stream)  │
│   …                                 │
└─────────────────────────────────────┘
```

对推理工程而言，有两点关键：

1. **所有张量数据在 header 之后连续存放。** 这正是 `mmap` 可行的原因。runtime 把文件映射一次，然后让 GPU 指向它需要的偏移量。
2. **块边界是对齐的。** 对 Q4_K，K-quant 的块为 256 个权重 = 144 字节。runtime 可以读取单个块（一次 DRAM 事务），并在寄存器中反量化。

---


<details>
<summary>English original</summary>

**5. Calibration Set — The 90-Second Decision**

The calibration set is the corpus AWQ/GPTQ/imatrix use to measure **activation statistics**. Rules of thumb:

| Goal | Calibration set |
|---|---|
| General chat assistant | OpenAssistant + ShareGPT mix (~512 samples × 2 k tokens) |
| Code assistant | Stack-V2 subset, languages you care about |
| Multilingual (Chinese + English) | mC4 + Wikipedia-ZH + Wikipedia-EN |
| Long context | Books-3 or fan-fiction sliced to ~8 k each |
| Domain-specific (medical, legal, etc.) | Public corpora in that domain |

Common pitfalls:

* **Too small** (<32 samples) — calibration overfits to handful of activation patterns.
* **Too narrow** — calibrating only on English breaks Chinese performance noticeably.
* **Too short** — samples shorter than ~256 tokens don't exercise long-range attention.
* **Calibrating with `<think>` blocks included** — for Qwen3, decide whether you want the model to be good at thinking-mode or chat-mode and pick the calibration data accordingly. Mixed works but is dominated by the majority.

---

**6. Validating the Quant**

Two cheap checks, one expensive one:

**6.1 Sanity (60 seconds)**

```python
from llama_cpp import Llama

m = Llama(model_path="qwen3-4b-q4_k_m.gguf",
          n_ctx=2048, n_gpu_layers=99)

for prompt in [
    "Write a one-sentence summary of quantum entanglement.",
    "用一句话解释量子纠缠。",
    "def fibonacci(n):",
    "Solve: if x + 3 = 7, what is x?",
]:
    out = m(prompt, max_tokens=64, temperature=0.0)
    print(out["choices"][0]["text"])
```

If the model produces gibberish, repeats itself, switches language mid-response, or refuses on benign prompts — something is broken (often a tokenizer or chat-template bug, sometimes RoPE layout, occasionally a corrupt quant).

**6.2 Perplexity sweep (10 minutes)**

```bash
./perplexity -m qwen3-4b-q4_k_m.gguf -f wikitext-2-raw/wiki.test.raw -c 2048
```

Compare against the FP16 baseline. Q4_K_M typically lands within +0.05 to +0.15 perplexity of FP16 on wiki text. Anything bigger means something is off — usually the calibration was bad or you accidentally quantized the LM head.

**6.3 Downstream eval (1–6 GPU-hours)**

`lm-eval-harness` with a Qwen-friendly suite:

```bash
lm-eval --model gguf \
    --model_args pretrained=qwen3-4b-q4_k_m.gguf \
    --tasks mmlu,ifeval,humaneval,gsm8k \
    --batch_size 1 \
    --num_fewshot 0
```

Targets for Q4_K_M Qwen3-4B vs BF16:

* MMLU: drop < 0.5 pts
* IFEval (instruction following): drop < 1.5 pts (this is the most sensitive metric for Q4)
* GSM8K (math): drop < 2 pts
* HumanEval (code): drop < 1 pt

If IFEval drops > 3 pts, your calibration set was either too small or too narrow — re-run with a broader mix.

---

**7. Storage Layout — GGUF for Qwen3-4B-Q4_K_M**

The GGUF file is a single blob with this structure:

```
┌─────────────────────────────────────┐
│ Magic "GGUF" + version (4 bytes)    │
├─────────────────────────────────────┤
│ Tensor count (8 bytes)              │
│ Metadata KV count (8 bytes)         │
├─────────────────────────────────────┤
│ Metadata KV table                   │
│   - general.name = "Qwen3 4B …"     │
│   - general.architecture = "qwen3"  │
│   - qwen3.context_length = 40960    │
│   - qwen3.embedding_length = 2560   │
│   - qwen3.block_count = 36          │
│   - qwen3.attention.head_count = 32 │
│   - qwen3.attention.head_count_kv=8 │
│   - qwen3.feed_forward_length = 6912│
│   - qwen3.rope.freq_base = 1000000  │
│   - tokenizer.ggml.tokens = [...]   │
│   - tokenizer.ggml.merges = [...]   │
│   - ...                             │
├─────────────────────────────────────┤
│ Tensor info table (name, dtype,     │
│   shape, offset for each tensor)    │
├─────────────────────────────────────┤
│ Padding to alignment                │
├─────────────────────────────────────┤
│ Tensor data — raw quantized blocks  │
│   token_embd (Q6_K block stream)    │
│   blk.0.attn_norm (F32)             │
│   blk.0.attn_q (Q4_K block stream)  │
│   blk.0.attn_q.bias (F32)           │
│   blk.0.attn_k (Q4_K block stream)  │
│   …                                 │
└─────────────────────────────────────┘
```

Two things matter for inference engineering:

1. **All tensor data is contiguous after the header.** This is what makes `mmap` viable. The runtime maps the file once and points the GPU at the offsets it needs.
2. **Block boundaries are aligned.** K-quant blocks are 256 weights = 144 bytes for Q4_K. The runtime can read a single block (one DRAM transaction) and dequant it in registers.

---

</details>

## 8. 非对称内存：把对的张量放到对的位置

在带统一内存的 Orin Nano 上，你什么都不用做——CPU 和 GPU 看到的是同一块 DRAM。在独立 GPU 机器（RTX、L40S 等）上，runtime 决定把哪些张量拷到 VRAM。启发式规则：按每个 decode（逐 token 生成阶段）读取字节数的顺序拷贝。

对 Qwen3-4B-Q4_K_M，按优先级排序：

1. **全部 Transformer block 权重**（~2.0 GB）——每 decode 一个 token 时，每一层都会访问它们。必须放在 VRAM。
2. **输出 embedding（与 token_embd 绑定）**（Q6_K 下约 390 MB）——每生成一个 token 都要为 LM head 的 GEMV（矩阵-向量乘）读一次。必须放在 VRAM。
3. **Norm 权重**（合计约 600 KB）——很小，留在 VRAM；每次 layer norm 都要用到。
4. **Tokenizer 与元数据**——CPU。
5. **KV cache**——分配在 VRAM 中，与权重分开。

对 8 GB 的 GPU 来说这很轻松，根本不需要 offload。只有当 VRAM < ~3 GB 时（部分 Jetson Nano 配置、部分嵌入式 Tegra），这个决策才有意义。

---

## 9. 质量与带宽的前沿——选一次，然后接受它

一张与部署目标挂钩的决策表：

| 部署场景 | 推荐量化 | 原因 |
|---|---|---|
| Jetson Orin Nano 8 GB 聊天助手 | Q4_K_M | 最佳平衡点——可达 20+ tok/s，MMLU 下降 < 0.5 |
| Jetson Orin Nano 8 GB 代码 copilot | Q5_K_M 或 AWQ-int4 | 代码对 logit 精度更敏感 |
| Jetson Orin NX 16 GB | Q5_K_M 或 Q6_K | 余量更大，瓶颈仍是带宽 |
| Raspberry Pi 5 + AI HAT（Hailo） | Q4_K_M（Hailo 路径） | Hailo 的工具链支持 K-quant |
| 独立 GPU（RTX 4090、L40S）跑 batch=1 | AWQ-int4 或 GPTQ-int4 | 工具链更好，有 Marlin kernel |
| 独立 GPU 跑大 batch | AWQ-int4 + FP16 KV | 带宽压力较小，吞吐由 KV 主导 |
| 追求最高质量、愿意付出带宽 | Q6_K | 近乎无损，省心的默认选项 |
| 激进压体积（如浏览器内推理） | IQ4_XS 或 Q3_K_M | 接受约 1 个点的评测下降 |

一旦你发布某个量化选择，在用户眼里模型**就等同于那个量化版本**。重新量化在运维上代价很低，但会积累**评测债**——即使“有效位宽相同”，你更换格式时用户仍会察觉行为的细微变化。

---

## 动手练习

1. **画出 bytes/token 曲线。** 取 Qwen3-4B，量化到 Q4_0、Q4_K_M、Q5_K_M、Q6_K、Q8_0（保留 FP16 GGUF，多次量化）。在 Orin Nano 上测每个版本的 tok/s（先运行 `jetson_clocks`！）。画 tok/s 对有效 bytes/weight 的曲线。确认线性关系。找出 kernel 效率偏低的离群点。

2. **Orin 上的 AWQ 与 K-quant 对比。** 用 CUDA 构建 llama.cpp，运行 Qwen3-4B-Q4_K_M。再在同一台 Orin 上构建 vLLM（或 MLC-LLM）（在 Orin AGX 上可行；在 Orin Nano 上偏紧），运行 Qwen3-4B-AWQ。比较 tok/s、峰值内存，以及 temperature 0 下的 IFEval 分数。

3. **重要性矩阵实验。** 对 Qwen3-4B-Q4_K_M 量化两次：一次使用来自校准集的 `--imatrix`，一次不使用。比较 wikitext 困惑度。计算困惑度差值——这就是该位宽下校准这一步的价值。

4. **FFN-down 敏感度。** 修改 llama.cpp 的量化逻辑，让 FFN-down 保持 FP16，其余全部量化到 Q4_K。再做一次：FFN-down 用 Q4_K，其余用 FP16。测两者的困惑度。确认 FFN-down 受损更严重。

5. **校准集消融。** 在 Qwen3-4B 上跑两次 AWQ 流水线：一次用默认混合校准，一次**只用**中文文本。用英文 MMLU 和一个中文评测（CMMLU）测试产出的量化模型。量化单语校准带来的双语代价。

6. **为真实产品选 Q。** 给一个假设的边缘产品定规格：“BOM 200 美元，4 GB DRAM，必须做到 15 tok/s，必须礼貌，必须支持英语 + 西班牙语。”选一个量化方案。用 200 字说明理由，引用 §9 的表格和你实测的困惑度数值。

---

## 关键要点

| 要点 | 为什么重要 |
|---|---|
| 每减少 1 bit/weight，在 Orin Nano 上大约换来 1 tok/s | 带宽受限的 decode 与 bits/weight 完全线性 |
| Q4_K_M 成为默认是有原因的 | 该规模下每字节质量最优；混合精度已内建 |
| V 和 FFN-down 值得比 Q/K/O/gate/up 分到更多 bit | 它们的误差会沿残差流累积 |
| 在 4B 规模上 AWQ 工具链胜出、质量持平 | 如果绑死 vLLM/TRT-LLM，直接用 AWQ 即可 |
| 校准集必须匹配部署领域 | 单语校准会悄悄破坏双语质量 |
| 至少用困惑度 + IFEval 验证 | 聚合分数会掩盖指令遵循能力的退化 |
| GGUF 张量数据是连续的；围绕 `mmap` 设计你的 loader | 零拷贝、对 page cache 友好、可安全重启 |

---


<details>
<summary>English original</summary>

**8. Asymmetric Memory: Stage the Right Tensors in the Right Place**

On Orin Nano with unified memory, you don't have to do anything — CPU and GPU see the same DRAM. On a discrete-GPU box (RTX, L40S, etc.), the runtime decides which tensors to copy to VRAM. Heuristic: copy in order of bytes-read-per-decode.

For Qwen3-4B-Q4_K_M, in priority order:

1. **All transformer-block weights** (~2.0 GB) — they're hit on every layer of every decoded token. Must be in VRAM.
2. **Output embedding (tied to token_embd)** (~390 MB at Q6_K) — read once per generated token for the LM head GEMV. Must be in VRAM.
3. **Norm weights** (~600 KB total) — tiny, keep in VRAM; they go through every layer norm.
4. **Tokenizer & metadata** — CPU.
5. **KV cache** — allocated in VRAM, separate from weights.

For an 8 GB GPU this is trivial; you'd never offload. The decision only matters for boxes where VRAM is < ~3 GB (some Jetson Nano configurations, some embedded Tegra).

---

**9. The Quality-Bandwidth Frontier — Choose Once, Live with It**

A decision table tied to deployment goals:

| Deployment | Recommended quant | Why |
|---|---|---|
| Jetson Orin Nano 8 GB chat assistant | Q4_K_M | Sweet spot — 20+ tok/s achievable, < 0.5 MMLU drop |
| Jetson Orin Nano 8 GB code copilot | Q5_K_M or AWQ-int4 | Code is more sensitive to logit precision |
| Jetson Orin NX 16 GB | Q5_K_M or Q6_K | More headroom, bandwidth still the wall |
| Raspberry Pi 5 + AI HAT (Hailo) | Q4_K_M (Hailo path) | Hailo's tooling supports K-quants |
| Discrete GPU (RTX 4090, L40S) for batch=1 | AWQ-int4 or GPTQ-int4 | Better tooling, Marlin kernels |
| Discrete GPU for high batch | AWQ-int4 with FP16 KV | Bandwidth less of a worry, throughput dominated by KV |
| Maximum quality, willing to pay bandwidth | Q6_K | Near-lossless, easy default |
| Aggressive size goal (e.g., browser inference) | IQ4_XS or Q3_K_M | Accept ~1-pt eval drop |

Once you ship a quant choice, the model **becomes that quant** in your users' eyes. Re-quantizing is cheap operationally but creates **eval debt** — users will notice subtle behavior changes when you swap formats, even at "the same effective bit-rate".

---

**Hands-On Exercises**

1. **Build the bytes/token plot.** Take Qwen3-4B and quantize it to Q4_0, Q4_K_M, Q5_K_M, Q6_K, Q8_0 (you'll keep the FP16 GGUF and quantize multiple times). Measure tok/s for each on Orin Nano (run `jetson_clocks` first!). Plot tok/s vs effective bytes/weight. Confirm linearity. Identify the kernel-inefficiency outliers.

2. **AWQ vs K-quant on Orin.** Build llama.cpp with CUDA, run Qwen3-4B-Q4_K_M. Then build vLLM (or MLC-LLM) on the same Orin (works on Orin AGX; tight on Orin Nano), run Qwen3-4B-AWQ. Compare tok/s, peak memory, and the IFEval scores at temperature 0.

3. **Importance-matrix experiment.** Quantize Qwen3-4B-Q4_K_M twice: once with `--imatrix` from a calibration set, once without. Compare wikitext perplexity. Compute the perplexity delta — that's the value of the calibration step at this bit-rate.

4. **FFN-down sensitivity.** Patch llama.cpp's quantization to keep FFN-down at FP16 while quantizing everything else to Q4_K. Repeat with FFN-down at Q4_K and everything else FP16. Measure perplexity for both. Confirm FFN-down hurts more.

5. **Calibration-set ablation.** Run the AWQ pipeline twice on Qwen3-4B: once with the default mixed calibration, once with **only** Chinese text. Test the resulting quants on English MMLU and a Chinese eval (CMMLU). Quantify the bilingual cost of monolingual calibration.

6. **Pick the Q for a real product.** Spec a hypothetical edge product: "$200 BOM, 4 GB DRAM, must do 15 tok/s, must be polite, must handle English+Spanish." Choose a quant. Justify in 200 words referencing the table in §9 and your measured perplexity numbers.

---

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| Every bit/weight you remove buys ~1 tok/s on Orin Nano | Bandwidth-bound decode is dead linear in bits/weight |
| Q4_K_M is the default for a reason | Best quality/byte at this scale; mixed-precision baked in |
| V and FFN-down deserve more bits than Q/K/O/gate/up | Their error compounds along the residual stream |
| AWQ beats K-quants on tooling, ties on quality at 4B | If you're vLLM/TRT-LLM-bound, just use AWQ |
| Calibration set must match deployment domain | Monolingual calibration silently breaks bilingual quality |
| Validate with at least perplexity + IFEval | Aggregate scores can mask instruction-following regression |
| GGUF tensor data is contiguous; design your loader around `mmap` | Zero-copy, page-cache-friendly, restart-safe |

---

</details>

## Resources

* **[ggml quantization formats — Justine Tunney](https://justine.lol/matmul/):** 关于 K-quant kernel 设计与反量化技巧的最佳读物。
* **[AWQ paper (2023, latest revision 2024)](https://arxiv.org/abs/2306.00978):** 奠基性论文，易读。
* **[GPTQ paper](https://arxiv.org/abs/2210.17323):** Layer-wise OBQ —— 稍旧但仍相关。
* **[AutoAWQ](https://github.com/casper-hansen/AutoAWQ):** AWQ 参考实现，带 HF 集成。
* **[AutoGPTQ](https://github.com/AutoGPTQ/AutoGPTQ):** GPTQ 参考实现。
* **[llama.cpp quantize tool](https://github.com/ggerganov/llama.cpp/blob/master/examples/quantize/README.md):** K-quant 转换。
* **[Hugging Face — Qwen3-4B-Instruct-GGUF (community)](https://huggingface.co/Qwen/Qwen3-4B-Instruct-GGUF):** 预量化权重，用于与本地转换结果对比。
* **[lm-eval-harness](https://github.com/EleutherAI/lm-evaluation-harness):** 标准 eval 框架。
* **[Marlin GPTQ kernel](https://github.com/IST-DASLab/marlin):** 为 GPTQ 优化的 4-bit GEMM —— vLLM 使用。


<details>
<summary>English original</summary>

**Resources**

* **[ggml quantization formats — Justine Tunney](https://justine.lol/matmul/):** Best read on K-quant kernel design and dequant tricks.
* **[AWQ paper (2023, latest revision 2024)](https://arxiv.org/abs/2306.00978):** Foundational paper, easy to read.
* **[GPTQ paper](https://arxiv.org/abs/2210.17323):** Layer-wise OBQ — slightly older but still relevant.
* **[AutoAWQ](https://github.com/casper-hansen/AutoAWQ):** Reference AWQ implementation with HF integration.
* **[AutoGPTQ](https://github.com/AutoGPTQ/AutoGPTQ):** Reference GPTQ implementation.
* **[llama.cpp quantize tool](https://github.com/ggerganov/llama.cpp/blob/master/examples/quantize/README.md):** K-quant conversion.
* **[Hugging Face — Qwen3-4B-Instruct-GGUF (community)](https://huggingface.co/Qwen/Qwen3-4B-Instruct-GGUF):** Pre-quantized weights to compare against your local conversion.
* **[lm-eval-harness](https://github.com/EleutherAI/lm-evaluation-harness):** The standard eval framework.
* **[Marlin GPTQ kernel](https://github.com/IST-DASLab/marlin):** Optimized 4-bit GEMM for GPTQ — used by vLLM.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Qwen Inference Optimization/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Qwen%20Inference%20Optimization/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
