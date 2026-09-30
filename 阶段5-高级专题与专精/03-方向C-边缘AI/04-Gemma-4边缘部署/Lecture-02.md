---
title: 第 02 讲 — 量化与格式转换：让 Gemma 4 用到正确的位宽
description: 第 02 讲 — 量化与格式转换：让 Gemma 4 用到正确的位宽
---

# 第 02 讲 — 量化与格式转换：让 Gemma 4 用到正确的位宽

**合集：** [Gemma 4 边缘部署](README.md) | **上一讲：** [← 第 01 讲](Lecture-01.md) | **下一讲：** [第 03 讲 →](Lecture-03.md)

---

Gemma 4 的架构为边缘部署而设计，但原始检查点仍是 BF16 —— 4B 模型为 8.6 GB。为满足第 01 讲的 runtime 目标（4B 为 2.2 GB INT4，12B 为 6.4 GB），需要从 BF16 转到正确的量化格式，**且不能出现不可接受的准确率下降**。本讲覆盖完整量化链路：为什么 Gemma 4 比大多数模型量化得更干净、具体算法（GPTQ、AWQ、K-quants）、校准策略，以及从量化检查点到部署栈所消费 runtime 格式的格式转换步骤。

---

## 学习目标

1. 解释为什么 Gemma 4 的 **QK-norm 与 GeGLU** 架构与没有它们的模型相比能降低量化误差。
2. 针对给定的目标 runtime 和准确率要求，在 **GPTQ、AWQ 与 K-quants（GGUF）** 之间做选择。
3. 为 Gemma 4 的 128K 上下文窗口构建合适的校准数据集，并运行一次校准 pass。
4. 将量化后的 Gemma 4 检查点转换为 **GGUF**（llama.cpp）、**TFLite FlatBuffer / LiteRT 格式**，并理解 ExecuTorch `.pte` 与 TRT engine 路径。
5. 解读量化误差指标（`perplexity Δ`、`kv_cache_max_quant_error`、下游任务上的准确率），并为边缘部署设置验收阈值。

---

## 1. 为什么 Gemma 4 量化得干净

在选择算法之前，先理解为什么 Gemma 4 是一个好的量化目标。

### 1.1 毁掉量化的两种病态现象

大多数 INT4/INT8 量化退化来自两种现象：

```text
Pathology 1: ACTIVATION OUTLIERS
  In some layers, a small fraction of activation values are 10–100× larger than
  the rest (often channel-specific). A per-tensor scale sized to the outlier
  means every normal value loses ~7 bits of effective precision.

  Example (without protection): activation range = [-0.1, 47.3]
    per-tensor scale = 47.3 / 127 ≈ 0.37 (for INT8)
    value 0.1 quantizes to round(0.1 / 0.37) = round(0.27) = 0 → pure noise

Pathology 2: ATTENTION LOGIT OVERFLOW
  At long sequence lengths, Q @ K^T can produce very large logits. If Q or K
  values grow with context (as in pre-norm attention without QK-norm), the
  logit distribution shifts — and a fixed per-tensor scale chosen during
  calibration on short sequences is wrong at deployment time on long sequences.
```

**Gemma 4 的答案：**

```text
QK-norm (for Pathology 2):
  Q = rms_norm(Q, g_q)   → Q values ∈ [-g_q_max, +g_q_max], bounded and predictable
  K = rms_norm(K, g_k)   → same for K
  The attention logit scale is then determined by g_q × g_k / sqrt(head_dim),
  which is constant across context lengths. Calibration on 512 tokens is valid
  for 128K tokens — no distribution shift.

GeGLU gating (for Pathology 1, partial):
  out = GELU(W_gate(x)) * W_up(x)
  The GELU gate saturates large positive/negative values to near-zero/near-one.
  This soft clipping reduces extreme activation values in the gate path,
  partially suppressing outliers in the FFN activation tensor.
```

结果：在标准 benchmark 上，采用 Q4_K_M（带 K-quants 的 GGUF 4-bit）的 Gemma 4 通常损失 **< 0.3 困惑度点**，而没有这些架构保护的模型为 0.5–1.5。对于 INT8，退化通常无法测量。

### 1.2 仍然会出什么问题

尽管有这些保护，仍存在两种量化失效模式：

1. **Embedding 表量化**：256K × 2560 的 embedding 很大（BF16 下 1.3 GB）。对 embedding 做 INT4 量化是有损的，因为每个 embedding 向量很短（2560 个值）—— 按行量化可行，但按通道（这本应是理想方案）需要宽 embedding。大多数 runtime 即使其余部分为 INT4，也会把 embedding 保持为 INT8 或 BF16。

2. **First and last layers**：Layer 0 输入投影和最后的 lm_head 投影始终更敏感。大多数量化流水线对这些 layer 跳过 INT4，将它们保持为 INT8 或 BF16。为这些更高精度的 layer 预留约 50–100 MB。


<details>
<summary>English original</summary>

**Lecture 02 — Quantization and Format Conversion: Getting Gemma 4 to the Right Bits**

**Collection:** [Gemma 4 Edge Deployment](README.md) | **Previous:** [← Lecture 01](Lecture-01.md) | **Next:** [Lecture 03 →](Lecture-03.md)

---

Gemma 4's architecture is designed for edge deployment, but the raw checkpoint is still BF16 — 8.6 GB for the 4B model. To fit the runtime targets from Lecture 01 (2.2 GB INT4 for 4B, 6.4 GB for 12B), you need to get from BF16 to the right quantized format **without unacceptable accuracy degradation**. This lecture covers the full quantization chain: why Gemma 4 quantizes more cleanly than most models, the specific algorithms (GPTQ, AWQ, K-quants), calibration strategies, and the format conversion step from a quantized checkpoint to the runtime format your deployment stack consumes.

---

**Learning objectives**

1. Explain why Gemma 4's **QK-norm and GeGLU** architecture reduces quantization error vs models without them.
2. Choose between **GPTQ, AWQ, and K-quants (GGUF)** for a given target runtime and accuracy requirement.
3. Build a calibration dataset appropriate for Gemma 4's 128K-context window and run a calibration pass.
4. Convert a quantized Gemma 4 checkpoint to **GGUF** (llama.cpp), **TFLite FlatBuffer / LiteRT format**, and understand the ExecuTorch `.pte` and TRT engine paths.
5. Interpret quantization error metrics (`perplexity Δ`, `kv_cache_max_quant_error`, accuracy on downstream tasks) and set acceptance thresholds for edge deployment.

---

**1. Why Gemma 4 quantizes cleanly**

Before choosing an algorithm, understand why Gemma 4 is a good quantization target.

**1.1 The two pathologies that kill quantization**

Most INT4/INT8 quantization degradation comes from two phenomena:

```text
Pathology 1: ACTIVATION OUTLIERS
  In some layers, a small fraction of activation values are 10–100× larger than
  the rest (often channel-specific). A per-tensor scale sized to the outlier
  means every normal value loses ~7 bits of effective precision.

  Example (without protection): activation range = [-0.1, 47.3]
    per-tensor scale = 47.3 / 127 ≈ 0.37 (for INT8)
    value 0.1 quantizes to round(0.1 / 0.37) = round(0.27) = 0 → pure noise

Pathology 2: ATTENTION LOGIT OVERFLOW
  At long sequence lengths, Q @ K^T can produce very large logits. If Q or K
  values grow with context (as in pre-norm attention without QK-norm), the
  logit distribution shifts — and a fixed per-tensor scale chosen during
  calibration on short sequences is wrong at deployment time on long sequences.
```

**Gemma 4's answers:**

```text
QK-norm (for Pathology 2):
  Q = rms_norm(Q, g_q)   → Q values ∈ [-g_q_max, +g_q_max], bounded and predictable
  K = rms_norm(K, g_k)   → same for K
  The attention logit scale is then determined by g_q × g_k / sqrt(head_dim),
  which is constant across context lengths. Calibration on 512 tokens is valid
  for 128K tokens — no distribution shift.

GeGLU gating (for Pathology 1, partial):
  out = GELU(W_gate(x)) * W_up(x)
  The GELU gate saturates large positive/negative values to near-zero/near-one.
  This soft clipping reduces extreme activation values in the gate path,
  partially suppressing outliers in the FFN activation tensor.
```

The result: Gemma 4 at Q4_K_M (GGUF 4-bit with K-quants) typically loses **< 0.3 perplexity points** on standard benchmarks, compared to 0.5–1.5 for models without these architectural protections. For INT8, the degradation is often immeasurable.

**1.2 What still goes wrong**

Despite the protections, two quantization failure modes remain:

1. **Embedding table quantization**: The 256K × 2560 embedding is large (1.3 GB BF16). INT4 quantization of embeddings is lossy because each embedding vector is short (2560 values) — per-row quantization is viable but per-channel (which would be ideal) requires wide embeddings. Most runtimes keep the embedding in INT8 or BF16 even when the rest is INT4.

2. **First and last layers**: Layer 0 input projection and the final lm_head projection always have higher sensitivity. Most quantization pipelines skip INT4 for these and keep them in INT8 or BF16. Budget ~50–100 MB for these layers at higher precision.

---

</details>

## 2. GPTQ——用于 TensorRT-LLM 和 vLLM 路径

**GPTQ**（Frantar 等，2022）是 Transformer 推理引擎中权重-only INT4 量化的标准。它利用校准数据中的二阶 Hessian 信息，独立地量化每一权重行。

```python
# GPTQ for Gemma 4 4B using AutoGPTQ:
from transformers import AutoModelForCausalLM, AutoTokenizer
from auto_gptq import AutoGPTQForCausalLM, BaseQuantizeConfig

# Load Gemma 4 4B from HuggingFace:
model_name = "google/gemma-4-4b-it"
tokenizer  = AutoTokenizer.from_pretrained(model_name)
model      = AutoModelForCausalLM.from_pretrained(model_name, device_map="cpu", torch_dtype="bfloat16")

# GPTQ config: 4-bit, group_size 128 (recommended for Gemma 4):
quant_config = BaseQuantizeConfig(
    bits=4,
    group_size=128,         # 128 rows share one scale — balance precision vs overhead
    desc_act=False,         # disable act-order reordering (not needed with QK-norm)
    damp_percent=0.1,       # Hessian damping for numerical stability
    static_groups=False,
)

# Calibration dataset: use diverse multilingual data matching 128K context capability
# Keep calibration sequences SHORT (512–2048 tokens) to match most deployment scenarios:
def get_calibration_data(tokenizer, n_samples=128, seq_len=2048):
    from datasets import load_dataset
    data = load_dataset("allenai/c4", "en", split="validation", streaming=True)
    samples = []
    for sample in data:
        tokens = tokenizer(sample["text"], max_length=seq_len, truncation=True,
                           return_tensors="pt")
        samples.append(tokens["input_ids"])
        if len(samples) >= n_samples: break
    return samples

calib_data = get_calibration_data(tokenizer, n_samples=128, seq_len=2048)

# Run GPTQ quantization (requires 1× A100 or 4× A10 for 4B, more for 12B+):
quantized_model = AutoGPTQForCausalLM.from_pretrained(
    model_name, quant_config, calib_data=calib_data
)
quantized_model.save_quantized("gemma4-4b-gptq-int4")
```

**Gemma 4 的校准注意事项：**

- **不要在 128K 上下文下校准**，除非你的部署始终是 128K。在最大上下文下校准会使缩放因子偏向罕见的长上下文分布。应按照你典型部署长度进行校准（聊天场景为 512–2048 token）。
- 使用**多样化数据**：Gemma 4 的 256K 词表覆盖 100+ 种语言。若你的部署是单语的，就用该语言校准，以获得最佳的逐语言准确率。若为多语言，则使用 C4-multilingual 或 mC4。
- **128 个样本 × 2048 token** 是标准下限。更多样本可降低方差，但超过 512 后收益递减。

**GPTQ 输出：** 一个 `model.safetensors`，内含 INT4 打包权重（当 `group_size=128` 时，2 个 FP16 值共享一个 32 位字）。由 TensorRT-LLM 与 vLLM 原生消费。

---

## 3. AWQ——用于 MLC-LLM 和激活感知路径

**AWQ**（Lin 等，2023）是 GPTQ 的替代方案，它通过在量化前对**显著通道**（激活幅值高的通道）进行缩放来保护它们：

```python
from awq import AutoAWQForCausalLM

model = AutoAWQForCausalLM.from_pretrained("google/gemma-4-4b-it")
tokenizer = AutoTokenizer.from_pretrained("google/gemma-4-4b-it")

# AWQ needs activation statistics — collect with calibration:
model.quantize(
    tokenizer,
    quant_config={
        "zero_point": True,     # use zero_point offset (better for asymmetric data)
        "q_group_size": 128,    # same group size as GPTQ
        "w_bit": 4,             # INT4 weights
        "version": "GEMM",      # optimized for GEMM kernels vs "GEMV" for small batch
    }
)
model.save_quantized("gemma4-4b-awq-int4", safetensors=True)
```

**Gemma 4 上 GPTQ 与 AWQ 对比：**

| 判据 | GPTQ | AWQ |
|-----------|------|-----|
| INT4 下的准确率 | 略好（使用 Hessian） | 略差但校准快 |
| 校准时间（4B） | A100 上约 15 分钟 | A100 上约 5 分钟 |
| Runtime 支持 | TRT-LLM、vLLM、llama.cpp（经由 GGUF 转换） | MLC-LLM 原生、vLLM、llama.cpp |
| 激活感知的通道缩放 | 否 | **是**（对离群模型是关键优势） |
| 最适合 Gemma 4？ | **是**（QK-norm 抑制离群值，Hessian 胜出） | 当存在离群值时表现良好；对 Gemma 4 需求较少 |

对 Gemma 4 而言，**GPTQ 更受青睐**，因为 QK-norm 已抑制了 AWQ 的激活感知缩放所针对的离群通道。没有离群值时，GPTQ 的二阶信息能给出更干净的量化。

---

## 4. K-quants（GGUF）——用于 Jetson 上的 llama.cpp

**K-quants** 是 llama.cpp 的混合精度量化格式，以 **GGUF** 文件格式发布。它们根据各层的敏感度，以不同的位宽量化不同的层。


<details>
<summary>English original</summary>

**2. GPTQ — for TensorRT-LLM and vLLM paths**

**GPTQ** (Frantar et al., 2022) is the standard for weight-only INT4 quantization for transformer inference engines. It quantizes each weight row independently using second-order Hessian information from calibration data.

```python
# GPTQ for Gemma 4 4B using AutoGPTQ:
from transformers import AutoModelForCausalLM, AutoTokenizer
from auto_gptq import AutoGPTQForCausalLM, BaseQuantizeConfig

# Load Gemma 4 4B from HuggingFace:
model_name = "google/gemma-4-4b-it"
tokenizer  = AutoTokenizer.from_pretrained(model_name)
model      = AutoModelForCausalLM.from_pretrained(model_name, device_map="cpu", torch_dtype="bfloat16")

# GPTQ config: 4-bit, group_size 128 (recommended for Gemma 4):
quant_config = BaseQuantizeConfig(
    bits=4,
    group_size=128,         # 128 rows share one scale — balance precision vs overhead
    desc_act=False,         # disable act-order reordering (not needed with QK-norm)
    damp_percent=0.1,       # Hessian damping for numerical stability
    static_groups=False,
)

# Calibration dataset: use diverse multilingual data matching 128K context capability
# Keep calibration sequences SHORT (512–2048 tokens) to match most deployment scenarios:
def get_calibration_data(tokenizer, n_samples=128, seq_len=2048):
    from datasets import load_dataset
    data = load_dataset("allenai/c4", "en", split="validation", streaming=True)
    samples = []
    for sample in data:
        tokens = tokenizer(sample["text"], max_length=seq_len, truncation=True,
                           return_tensors="pt")
        samples.append(tokens["input_ids"])
        if len(samples) >= n_samples: break
    return samples

calib_data = get_calibration_data(tokenizer, n_samples=128, seq_len=2048)

# Run GPTQ quantization (requires 1× A100 or 4× A10 for 4B, more for 12B+):
quantized_model = AutoGPTQForCausalLM.from_pretrained(
    model_name, quant_config, calib_data=calib_data
)
quantized_model.save_quantized("gemma4-4b-gptq-int4")
```

**Calibration notes for Gemma 4:**

- **Do NOT calibrate at 128K context** unless your deployment is always 128K. Calibrating at the max context biases the scales toward the rare long-context distribution. Calibrate at your typical deployment length (512–2048 tokens for chat).
- Use **diverse data**: Gemma 4's 256K vocab covers 100+ languages. If your deployment is monolingual, calibrate on that language to get the best per-language accuracy. If multilingual, use C4-multilingual or mC4.
- **128 samples × 2048 tokens** is the standard minimum. More samples reduce variance but exhibit diminishing returns past 512.

**GPTQ output:** a `model.safetensors` with INT4 packed weights (2 FP16 values share one 32-bit word when `group_size=128`). Consumed by TensorRT-LLM and vLLM natively.

---

**3. AWQ — for MLC-LLM and the activation-aware path**

**AWQ** (Lin et al., 2023) is an alternative to GPTQ that protects **salient channels** (those with high activation magnitude) by scaling them before quantization:

```python
from awq import AutoAWQForCausalLM

model = AutoAWQForCausalLM.from_pretrained("google/gemma-4-4b-it")
tokenizer = AutoTokenizer.from_pretrained("google/gemma-4-4b-it")

# AWQ needs activation statistics — collect with calibration:
model.quantize(
    tokenizer,
    quant_config={
        "zero_point": True,     # use zero_point offset (better for asymmetric data)
        "q_group_size": 128,    # same group size as GPTQ
        "w_bit": 4,             # INT4 weights
        "version": "GEMM",      # optimized for GEMM kernels vs "GEMV" for small batch
    }
)
model.save_quantized("gemma4-4b-awq-int4", safetensors=True)
```

**GPTQ vs AWQ for Gemma 4:**

| Criterion | GPTQ | AWQ |
|-----------|------|-----|
| Accuracy at INT4 | Slightly better (uses Hessian) | Slightly worse but fast calibration |
| Calibration time (4B) | ~15 min on A100 | ~5 min on A100 |
| Runtime support | TRT-LLM, vLLM, llama.cpp (via GGUF convert) | MLC-LLM native, vLLM, llama.cpp |
| Activation-aware channel scaling | No | **Yes** (key advantage for outlier models) |
| Best for Gemma 4? | **Yes** (QK-norm suppresses outliers, Hessian wins) | Good when outliers exist; less needed for Gemma 4 |

For Gemma 4, **GPTQ is preferred** because QK-norm already suppresses the outlier channels that AWQ's activation-aware scaling targets. Without outliers, GPTQ's second-order information gives cleaner quantization.

---

**4. K-quants (GGUF) — for llama.cpp on Jetson**

**K-quants** are llama.cpp's mixed-precision quantization format, shipped in the **GGUF** file format. They quantize different layers at different bit widths depending on their sensitivity.

</details>

### 4.1 面向 Gemma 4 的 GGUF K-quant 格式

| 格式 | 平均位宽 | 策略 | 大小（4B） | 困惑度 Δ |
|--------|----------|----------|-----------|--------------|
| Q2_K | 2.63 | 极致激进 | ~1.6 GB | ~3.5 pp |
| Q4_K_S | 4.37 | 4-bit，小分块 | ~2.5 GB | ~0.4 pp |
| **Q4_K_M** | **4.85** | **4-bit，中等（推荐）** | **~2.8 GB** | **~0.2 pp** |
| Q5_K_M | 5.68 | 5-bit，中等 | ~3.3 GB | ~0.1 pp |
| Q6_K | 6.56 | 6-bit 分块 | ~3.8 GB | ~0.05 pp |
| Q8_0 | 8.5 | 8-bit，快速 | ~4.6 GB | ~0.01 pp |

**Q4_K_M 是 Gemma 4 在 Jetson Orin 上的标准推荐**——它能轻松装下所有模型尺寸，同时在标准 benchmark 上只损失 < 0.2 个困惑度点。

### 4.2 将 Gemma 4 转换为 GGUF

```bash
# Step 1: Clone llama.cpp (ensure Gemma 4 support — check tag ≥ b3500):
git clone https://github.com/ggml-org/llama.cpp && cd llama.cpp

# Step 2: Install conversion deps:
pip install -r requirements.txt

# Step 3: Convert HuggingFace Gemma 4 to GGUF (F16 first):
python convert_hf_to_gguf.py \
    /path/to/gemma-4-4b-it \
    --outfile gemma4-4b-f16.gguf \
    --outtype f16

# Step 4: Quantize to Q4_K_M (this is fast, runs on CPU, no GPU needed):
./build/bin/llama-quantize gemma4-4b-f16.gguf gemma4-4b-Q4_K_M.gguf Q4_K_M

# Step 5: Verify the result:
./build/bin/llama-perplexity -m gemma4-4b-Q4_K_M.gguf \
    -f wikitext-2-raw/wiki.test.raw --ctx 2048
```

**Gemma 4 专属说明：** 确保 `convert_hf_to_gguf.py` 脚本带有 Gemma 4 tokenizer 处理逻辑。必须能识别 Gemma 4 的 `256K` 词表，以及 `config.json` 中的 `model_type: "gemma4"` 标识符。若使用较旧的 llama.cpp，检查是否已具备 `models/gemma4` 架构支持。

### 4.3 Imatrix 量化——相同位宽下更好的准确率

`imatrix`（importance matrix）是 llama.cpp 的校准感知量化：

```bash
# Build an importance matrix (calibration pass, ~30 min on CPU for 4B):
./build/bin/llama-imatrix \
    -m gemma4-4b-f16.gguf \
    -f calibration_data.txt \    # 512 × 2048-token text samples
    -o gemma4-4b.imatrix \
    --ctx 2048 -b 512

# Quantize with imatrix (lower perplexity than standard K-quant at same bits):
./build/bin/llama-quantize \
    --imatrix gemma4-4b.imatrix \
    gemma4-4b-f16.gguf \
    gemma4-4b-Q4_K_M_imatrix.gguf \
    Q4_K_M

# Result: Q4_K_M with imatrix typically ≈ Q5_K_M accuracy at Q4_K_M size
```

---

## 5. LiteRT 格式（Google AI Edge）——PDL 原生路径

**LiteRT**（Google AI Edge，前身是 TensorFlow Lite）是 Google 的端侧推理 runtime。针对 Gemma 4，Google 通过 **AI Edge Torch** 库提供了直接的导出路径：

```python
# pip install ai-edge-torch

import ai_edge_torch
from transformers import AutoModelForCausalLM, AutoTokenizer
import torch

# Load Gemma 4 4B:
model_id = "google/gemma-4-4b-it"
model = AutoModelForCausalLM.from_pretrained(model_id, torch_dtype=torch.bfloat16)
model.eval()

# Export to LiteRT FlatBuffer (.tflite):
sample_input = torch.ones((1, 128), dtype=torch.long)  # (batch, seq_len)
edge_model = ai_edge_torch.convert(
    model,
    sample_args=(sample_input,),
    quant_config=ai_edge_torch.quantize.pt2e_quantizer.PT2EQuantizerConfig(
        is_per_channel=True,            # per-channel INT8 for weights
        is_symmetric=True,
    )
)
edge_model.export("gemma4-4b-int8.tflite")
```

**在 LiteRT 中使用 INT4（AI Edge 的 Gemma 专属路径）：**

Google 通过 Kaggle Models 提供了 Gemma 4 的预量化 LiteRT 变体。它们是 INT4/INT8 混合精度的 FlatBuffers，其中 embedding 表以 INT8 保留：

```bash
# Download via kaggle CLI (requires Kaggle API key + Gemma 4 terms acceptance):
kaggle models instances versions download \
    google/gemma/tfLite/gemma-4-4b-it-int4/1 \
    --untar
# → gemma4-4b-it-int4.tflite (~2.4 GB)
```

**Jetson 上 LiteRT 与 GGUF 的对比：**

| 评判维度 | LiteRT (.tflite) | GGUF (llama.cpp) |
|-----------|-----------------|-----------------|
| 针对 Android/嵌入式优化 | **是（主要目标）** | 否（桌面/服务器优先） |
| Jetson 上的 CUDA 后端 | 有限（通过 GPU delegate） | **是（CUDA，完整）** |
| Google AI Edge 生态 | **是（一等公民）** | 否 |
| 面向 Jetson 的 kernel 优化 | 基础 | **完整的 CUDA kernel** |
| 最适合 Jetson CUDA | 否 | **是** |
| 最适合 Jetson DLA / ARM | **是** | 否 |

**建议：** 对于带 CUDA 后端的 Jetson（Orin/Thor），为获得峰值吞吐应使用 **GGUF + llama.cpp** 或 **TRT-LLM**。当部署到 Jetson 的 DLA 加速器、ARM CPU，或把同一模型交叉部署到 Android/iOS 时，使用 **LiteRT**。

---


<details>
<summary>English original</summary>

**4.1 GGUF K-quant formats for Gemma 4**

| Format | Avg bits | Strategy | Size (4B) | Perplexity Δ |
|--------|----------|----------|-----------|--------------|
| Q2_K | 2.63 | Ultra-aggressive | ~1.6 GB | ~3.5 pp |
| Q4_K_S | 4.37 | 4-bit, small block | ~2.5 GB | ~0.4 pp |
| **Q4_K_M** | **4.85** | **4-bit, medium (recommended)** | **~2.8 GB** | **~0.2 pp** |
| Q5_K_M | 5.68 | 5-bit, medium | ~3.3 GB | ~0.1 pp |
| Q6_K | 6.56 | 6-bit blocks | ~3.8 GB | ~0.05 pp |
| Q8_0 | 8.5 | 8-bit, fast | ~4.6 GB | ~0.01 pp |

**Q4_K_M is the standard recommendation for Gemma 4 on Jetson Orin** — it fits all model sizes comfortably while losing < 0.2 perplexity points on standard benchmarks.

**4.2 Converting Gemma 4 to GGUF**

```bash
# Step 1: Clone llama.cpp (ensure Gemma 4 support — check tag ≥ b3500):
git clone https://github.com/ggml-org/llama.cpp && cd llama.cpp

# Step 2: Install conversion deps:
pip install -r requirements.txt

# Step 3: Convert HuggingFace Gemma 4 to GGUF (F16 first):
python convert_hf_to_gguf.py \
    /path/to/gemma-4-4b-it \
    --outfile gemma4-4b-f16.gguf \
    --outtype f16

# Step 4: Quantize to Q4_K_M (this is fast, runs on CPU, no GPU needed):
./build/bin/llama-quantize gemma4-4b-f16.gguf gemma4-4b-Q4_K_M.gguf Q4_K_M

# Step 5: Verify the result:
./build/bin/llama-perplexity -m gemma4-4b-Q4_K_M.gguf \
    -f wikitext-2-raw/wiki.test.raw --ctx 2048
```

**Gemma 4-specific note:** Ensure the `convert_hf_to_gguf.py` script has a Gemma 4 tokenizer handler. Gemma 4's `256K` vocabulary and the `model_type: "gemma4"` identifier in `config.json` must be recognized. If using an older llama.cpp, check that `models/gemma4` architecture support is present.

**4.3 Imatrix quantization — better accuracy at the same bit count**

`imatrix` (importance matrix) is llama.cpp's calibration-aware quantization:

```bash
# Build an importance matrix (calibration pass, ~30 min on CPU for 4B):
./build/bin/llama-imatrix \
    -m gemma4-4b-f16.gguf \
    -f calibration_data.txt \    # 512 × 2048-token text samples
    -o gemma4-4b.imatrix \
    --ctx 2048 -b 512

# Quantize with imatrix (lower perplexity than standard K-quant at same bits):
./build/bin/llama-quantize \
    --imatrix gemma4-4b.imatrix \
    gemma4-4b-f16.gguf \
    gemma4-4b-Q4_K_M_imatrix.gguf \
    Q4_K_M

# Result: Q4_K_M with imatrix typically ≈ Q5_K_M accuracy at Q4_K_M size
```

---

**5. LiteRT format (Google AI Edge) — the PDL-native path**

**LiteRT** (Google AI Edge, formerly TensorFlow Lite) is Google's on-device inference runtime. For Gemma 4 specifically, Google provides a direct export path via the **AI Edge Torch** library:

```python
# pip install ai-edge-torch

import ai_edge_torch
from transformers import AutoModelForCausalLM, AutoTokenizer
import torch

# Load Gemma 4 4B:
model_id = "google/gemma-4-4b-it"
model = AutoModelForCausalLM.from_pretrained(model_id, torch_dtype=torch.bfloat16)
model.eval()

# Export to LiteRT FlatBuffer (.tflite):
sample_input = torch.ones((1, 128), dtype=torch.long)  # (batch, seq_len)
edge_model = ai_edge_torch.convert(
    model,
    sample_args=(sample_input,),
    quant_config=ai_edge_torch.quantize.pt2e_quantizer.PT2EQuantizerConfig(
        is_per_channel=True,            # per-channel INT8 for weights
        is_symmetric=True,
    )
)
edge_model.export("gemma4-4b-int8.tflite")
```

**For INT4 with LiteRT (AI Edge Gemma-specific path):**

Google provides pre-quantized LiteRT variants for Gemma 4 via Kaggle Models. These are INT4/INT8 mixed-precision FlatBuffers with the embedding table preserved in INT8:

```bash
# Download via kaggle CLI (requires Kaggle API key + Gemma 4 terms acceptance):
kaggle models instances versions download \
    google/gemma/tfLite/gemma-4-4b-it-int4/1 \
    --untar
# → gemma4-4b-it-int4.tflite (~2.4 GB)
```

**LiteRT vs GGUF on Jetson:**

| Criterion | LiteRT (.tflite) | GGUF (llama.cpp) |
|-----------|-----------------|-----------------|
| Optimized for Android/embedded | **Yes (primary target)** | No (desktop/server-first) |
| CUDA backend on Jetson | Limited (via GPU delegate) | **Yes (CUDA, full)** |
| Google AI Edge ecosystem | **Yes (first-class)** | No |
| Kernel optimization for Jetson | Basic | **Full CUDA kernels** |
| Best for Jetson CUDA | No | **Yes** |
| Best for Jetson DLA / ARM | **Yes** | No |

**Recommendation:** For Jetson with CUDA backend (Orin/Thor), use **GGUF + llama.cpp** or **TRT-LLM** for peak throughput. Use **LiteRT** when deploying to Jetson's DLA accelerator, ARM CPU, or cross-deploying the same model to Android/iOS.

---

</details>

## 6. ExecuTorch `.pte` 格式——Meta 面向 Gemma 4 的边缘路径

ExecuTorch（Meta，2024）将 PyTorch 模型导出为面向边缘设备的可移植 `.pte` 格式。Google 添加了 Gemma 支持：

```python
# export_gemma4_et.py  (requires executorch nightly)
import torch
from executorch.exir import to_edge
from executorch.backends.xnnpack.partition.xnnpack_partitioner import XnnpackPartitioner
from transformers import AutoModelForCausalLM

model = AutoModelForCausalLM.from_pretrained(
    "google/gemma-4-4b-it", torch_dtype=torch.float32
)
model.eval()

# Export with XNNPACK backend (optimized for ARM NEON / Jetson CPU fallback):
example_inputs = (torch.ones(1, 64, dtype=torch.long),)
edge_program = to_edge(
    torch.export.export(model, example_inputs),
    compile_config=torch.backends.xnnpack.XnnpackPartitioner()
)
et_program = edge_program.to_executorch()

with open("gemma4-4b.pte", "wb") as f:
    f.write(et_program.buffer)
```

当你需要**在 ARM CPU + XNNPACK 上运行同一个模型**、横跨 Android、iOS 和嵌入式 Linux 而无需各自单独的编译步骤时，ExecuTorch `.pte` 最为有用。在带 CUDA 的 Jetson 上，TRT-LLM 或 llama.cpp 的性能会显著优于 ExecuTorch。

---

## 7. 格式选择指南

```text
DECISION TREE for Gemma 4 format choice on Jetson:

  Target: Jetson with CUDA (Orin / Thor)
  └─ Need peak throughput? → TensorRT-LLM engine (build from GPTQ safetensors)
  └─ Need cross-platform + portability? → GGUF Q4_K_M (llama.cpp)
  └─ Need Google AI Edge ecosystem integration? → LiteRT .tflite (via AI Edge)
  └─ Need TVM compilation with MLC-LLM? → AWQ safetensors → mlc_llm convert

  Target: Jetson DLA / ARM CPU only
  └─ Google AI Edge first choice → LiteRT .tflite (GPU delegate or CPU)
  └─ XNNPACK on ARM → ExecuTorch .pte

  Target: Jetson + Android/iOS same model
  └─ LiteRT .tflite (runs everywhere in the Google AI Edge ecosystem)

  Target: low-latency single-GPU cloud (Orin Orin server cluster)
  └─ GPTQ INT4 + TensorRT-LLM
```

---

## 8. 准确率验收标准

部署量化后的 Gemma 4 模型前，先设定明确的验收阈值：

```text
Metric          │ INT8       │ Q4_K_M     │ Q4_K_M imatrix │ Q2_K
────────────────┼────────────┼────────────┼────────────────┼──────────
Perplexity Δ   │ < 0.1 pp   │ < 0.25 pp  │ < 0.15 pp      │ < 4 pp
MMLU accuracy Δ│ < 0.3%     │ < 1.5%     │ < 0.8%         │ < 5%
GSM8K accuracy Δ│ < 0.5%    │ < 2.0%     │ < 1.0%         │ unacceptable
HumanEval Δ    │ < 0.5%     │ < 2.0%     │ < 1.0%         │ unacceptable
```

在交付前，针对你的部署模型跑这些 benchmark：

```bash
# lm-evaluation-harness (fast perplexity + task eval):
lm_eval --model gguf --model_args pretrained=gemma4-4b-Q4_K_M.gguf \
        --tasks mmlu,gsm8k,hellaswag \
        --num_fewshot 5 --batch_size 4

# If MMLU degrades > 1.5% from BF16 baseline: try Q5_K_M or imatrix.
# If latency is acceptable with Q5_K_M: use that instead of Q4_K_M.
```

---

## 关键要点

- Gemma 4 **量化得很干净**，因为 QK-norm 消除了 attention-logit 离群值（长序列下量化退化的主要元凶），而 GeGLU 部分抑制了 FFN 激活值离群值。
- **Q4_K_M**（GGUF）是 llama.cpp 在 Jetson 上的标准推荐：退化 < 0.2 pp，权重相比 BF16 压缩约 45%。加入 **imatrix** 校准可在不增加体积的情况下挽回约 0.05 pp。
- **GPTQ**（group_size=128，128 个校准样本）是在为 TensorRT-LLM 或 vLLM 提供模型时，最适合 Gemma 4 的算法——当离群值已被抑制时，二阶 Hessian 占优。
- **LiteRT / Google AI Edge** 是部署到 DLA、ARM 或跨平台（从一个 `.tflite` 出发，Jetson → Android → 嵌入式 Linux）时的首选路径。对于 Jetson 上的 CUDA，GGUF 或 TRT-LLM 胜出。
- 即使在 INT4 部署中，也要把 **embedding 表保持在 INT8**——256K 词表使得 INT4 embedding 行太短，难以实现低误差量化。
- 设定明确的**准确率验收阈值**（Δ 困惑度、MMLU、GSM8K）并在部署前测量——一个在下游任务上失败的量化模型不算部署。

---

## 参考文献

- GPTQ 论文（Frantar 等，2022）—— arXiv:2210.17323
- AWQ 论文（Lin 等，2023）—— arXiv:2306.00978
- llama.cpp GGUF 格式与 K-quants 文档 — [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- AI Edge Torch（Google 面向 Gemma → LiteRT 的导出路径）— [github.com/google-ai-edge/ai-edge-torch](https://github.com/google-ai-edge/ai-edge-torch)
- Google AI Edge LiteRT 文档 — [ai.google.dev/edge/litert](https://ai.google.dev/edge/litert)
- ExecuTorch（Meta）— [github.com/pytorch/executorch](https://github.com/pytorch/executorch)
- Kaggle Models — Gemma 4 LiteRT INT4 变体 — [kaggle.com/models/google/gemma](https://www.kaggle.com/models/google/gemma)

---


<details>
<summary>English original</summary>

**6. ExecuTorch `.pte` format — Meta's edge path for Gemma 4**

ExecuTorch (Meta, 2024) exports PyTorch models to a portable `.pte` format for edge devices. Google added Gemma support:

```python
# export_gemma4_et.py  (requires executorch nightly)
import torch
from executorch.exir import to_edge
from executorch.backends.xnnpack.partition.xnnpack_partitioner import XnnpackPartitioner
from transformers import AutoModelForCausalLM

model = AutoModelForCausalLM.from_pretrained(
    "google/gemma-4-4b-it", torch_dtype=torch.float32
)
model.eval()

# Export with XNNPACK backend (optimized for ARM NEON / Jetson CPU fallback):
example_inputs = (torch.ones(1, 64, dtype=torch.long),)
edge_program = to_edge(
    torch.export.export(model, example_inputs),
    compile_config=torch.backends.xnnpack.XnnpackPartitioner()
)
et_program = edge_program.to_executorch()

with open("gemma4-4b.pte", "wb") as f:
    f.write(et_program.buffer)
```

ExecuTorch `.pte` is most useful when you need to run **the same model on ARM CPU + XNNPACK** across Android, iOS, and embedded Linux without separate compile steps. On Jetson with CUDA, TRT-LLM or llama.cpp will outperform ExecuTorch significantly.

---

**7. Format selection guide**

```text
DECISION TREE for Gemma 4 format choice on Jetson:

  Target: Jetson with CUDA (Orin / Thor)
  └─ Need peak throughput? → TensorRT-LLM engine (build from GPTQ safetensors)
  └─ Need cross-platform + portability? → GGUF Q4_K_M (llama.cpp)
  └─ Need Google AI Edge ecosystem integration? → LiteRT .tflite (via AI Edge)
  └─ Need TVM compilation with MLC-LLM? → AWQ safetensors → mlc_llm convert

  Target: Jetson DLA / ARM CPU only
  └─ Google AI Edge first choice → LiteRT .tflite (GPU delegate or CPU)
  └─ XNNPACK on ARM → ExecuTorch .pte

  Target: Jetson + Android/iOS same model
  └─ LiteRT .tflite (runs everywhere in the Google AI Edge ecosystem)

  Target: low-latency single-GPU cloud (Orin Orin server cluster)
  └─ GPTQ INT4 + TensorRT-LLM
```

---

**8. Accuracy acceptance criteria**

Before deploying a quantized Gemma 4 model, set explicit acceptance thresholds:

```text
Metric          │ INT8       │ Q4_K_M     │ Q4_K_M imatrix │ Q2_K
────────────────┼────────────┼────────────┼────────────────┼──────────
Perplexity Δ   │ < 0.1 pp   │ < 0.25 pp  │ < 0.15 pp      │ < 4 pp
MMLU accuracy Δ│ < 0.3%     │ < 1.5%     │ < 0.8%         │ < 5%
GSM8K accuracy Δ│ < 0.5%    │ < 2.0%     │ < 1.0%         │ unacceptable
HumanEval Δ    │ < 0.5%     │ < 2.0%     │ < 1.0%         │ unacceptable
```

Run these benchmarks against your deployment model before shipping:

```bash
# lm-evaluation-harness (fast perplexity + task eval):
lm_eval --model gguf --model_args pretrained=gemma4-4b-Q4_K_M.gguf \
        --tasks mmlu,gsm8k,hellaswag \
        --num_fewshot 5 --batch_size 4

# If MMLU degrades > 1.5% from BF16 baseline: try Q5_K_M or imatrix.
# If latency is acceptable with Q5_K_M: use that instead of Q4_K_M.
```

---

**Key takeaways**

- Gemma 4 **quantizes cleanly** because QK-norm removes attention-logit outliers (the main culprit for quantization degradation at long sequences) and GeGLU partially suppresses FFN activation outliers.
- **Q4_K_M** (GGUF) is the standard recommendation for llama.cpp on Jetson: < 0.2 pp degradation, ~45% weight compression vs BF16. Add **imatrix** calibration to recover ~0.05 pp at no size cost.
- **GPTQ** (group_size=128, 128 calibration samples) is the best algorithm for Gemma 4 when feeding TensorRT-LLM or vLLM — second-order Hessian wins when outliers are already suppressed.
- **LiteRT / Google AI Edge** is the first-class path when deploying to DLA, ARM, or cross-platform (Jetson → Android → embedded Linux from one `.tflite`). For CUDA-on-Jetson, GGUF or TRT-LLM wins.
- Keep the **embedding table at INT8** even in INT4 deployments — the 256K vocab makes INT4 embedding rows too short for low-error quantization.
- Set explicit **accuracy acceptance thresholds** (Δ perplexity, MMLU, GSM8K) and measure before deploying — a quantized model that fails your downstream task is not a deployment.

---

**References**

- GPTQ paper (Frantar et al., 2022) — arXiv:2210.17323
- AWQ paper (Lin et al., 2023) — arXiv:2306.00978
- llama.cpp GGUF format and K-quants documentation — [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- AI Edge Torch (Google's export path for Gemma → LiteRT) — [github.com/google-ai-edge/ai-edge-torch](https://github.com/google-ai-edge/ai-edge-torch)
- Google AI Edge LiteRT documentation — [ai.google.dev/edge/litert](https://ai.google.dev/edge/litert)
- ExecuTorch (Meta) — [github.com/pytorch/executorch](https://github.com/pytorch/executorch)
- Kaggle Models — Gemma 4 LiteRT INT4 variants — [kaggle.com/models/google/gemma](https://www.kaggle.com/models/google/gemma)

---

</details>

## 截至 2026-06

llama.cpp tag b3500+ 以支持 Gemma 4 GGUF；AI Edge Torch 0.3+；LiteRT 2.16+；AutoGPTQ 0.7+；AutoAWQ 0.2+。Gemma 4 4B/12B 推荐 GPTQ group_size=128；27B 用 group_size=64（更大规模下粒度更细）。运行前务必确认 llama.cpp 对 Gemma 4 tokenizer 的支持——256K 词表需要在 `convert_hf_to_gguf.py` 中显式处理模型类型。

---

*Previous: [← Lecture 01](Lecture-01.md) · Up: [Gemma 4 Edge Deployment](README.md) · Next: [Lecture 03 — The PDL Runtime Stack](Lecture-03.md)*


<details>
<summary>English original</summary>

**Current as of 2026-06**

llama.cpp tag b3500+ for Gemma 4 GGUF support; AI Edge Torch 0.3+; LiteRT 2.16+; AutoGPTQ 0.7+; AutoAWQ 0.2+. GPTQ group_size=128 recommended for Gemma 4 4B/12B; group_size=64 for 27B (finer granularity at larger scale). Always verify llama.cpp Gemma 4 tokenizer support before running — the 256K vocab requires explicit model type handling in `convert_hf_to_gguf.py`.

---

*Previous: [← Lecture 01](Lecture-01.md) · Up: [Gemma 4 Edge Deployment](README.md) · Next: [Lecture 03 — The PDL Runtime Stack](Lecture-03.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Gemma 4 Edge Deployment/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Gemma%204%20Edge%20Deployment/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
