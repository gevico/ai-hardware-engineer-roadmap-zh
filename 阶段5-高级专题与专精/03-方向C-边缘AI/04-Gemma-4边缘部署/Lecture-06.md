---
title: Lecture 06——用 Gemma 4 E2B 进行多 token 预测：官方 MTP drafter
description: Lecture 06——用 Gemma 4 E2B 进行多 token 预测：官方 MTP drafter
---

# Lecture 06——用 Gemma 4 E2B 进行多 token 预测：官方 MTP drafter

**Collection:** [Gemma 4 Edge Deployment](README.md) | **Previous:** [← Lecture 05](Lecture-05.md)

---

Lecture 04 讲了用 Gemma 4 1B 外部 draft 和 EAGLE-3 风格自 draft head 进行投机解码。Google 为同一问题提供了专门方案：**Gemma 4 E2B**，一个 4 层、极为轻量的 MTP（多 token 预测）drafter，设计用来与任意完整的 Gemma 4 目标模型配对。1B 外部 draft 需要 500 MB 额外内存，EAGLE-3 head 需要 2–4 小时训练，而 E2B 是官方开箱即用的 drafter，已发布权重，并在 vLLM、LiteRT-LM 和 llama.cpp 中获得框架支持——在文本生成 benchmark 上最多加速 3 倍。

本讲解释 MTP 是什么、Gemma 4 E2B 如何实现它、它与标准投机解码有何不同，以及如何将其部署到包括 Jetson 边缘目标在内的所有主流推理栈上。

---

## 学习目标

1. 解释**标准投机解码**（独立 draft 模型）与 **MTP**（共享状态的并行 token 预测）之间的区别。
2. 描述 Gemma 4 **E2B MTP drafter 架构**——为什么是 4 层、它与目标模型共享什么，以及它为何能收敛到高接受率。
3. 在 **vLLM**、**LiteRT-LM** 和 **llama.cpp** 上配置并运行 Gemma 4 E2B + 4B/12B/27B 目标的 MTP 投机解码。
4. 从内存、搭建成本、加速比和框架支持方面，对比本课程中的三种投机解码方案（1B 外部、EAGLE-3 head、E2B MTP）。
5. 为你的部署目标（Orin、Thor、云 GPU）和任务选择合适方案。

---

## 1. 多 token 预测——它是什么，有何不同

### 1.1 标准投机解码 （Lecture 04 回顾）

在标准 spec-decode 中，一个**独立的 draft 模型**（如 Gemma 4 1B）自回归地生成 K 个候选 token，目标模型则通过一次并行前向传播验证它们：

```text
Standard spec-decode:
  draft(context) → [x₁, x₂, x₃, x₄]      separate model, 4 steps
  target.verify(context + [x₁,x₂,x₃,x₄])  one parallel pass
  accept prefix, reject first mismatch

Problem: the draft model has DIFFERENT weights, different representations.
  Its token proposals can diverge from target in distribution → low τ on hard tasks.
  Memory cost: full second model weights (500 MB for 1B).
```

### 1.2 多 token 预测（MTP）

MTP **从同一模型的内部表示**预测多个未来 token，借助附加到目标模型上的辅助 head 或轻量 transformer 模块：

```text
MTP approach:
  target.forward(context) → hidden states h₀, h₁, ..., hₙ
  MTP head predicts x_{t+1}, x_{t+2}, ... from h_n (or from partial h)
  Target verifies proposed tokens — but the proposals came from TARGET's own features

Why this converges better:
  The MTP drafter sees the target's internal state, not a separately-learned state.
  Token proposals are in distribution with the target by construction.
  → higher acceptance length τ than an external draft model of equivalent size
```

### 1.3 DeepSeek MTP 与 Gemma E2B MTP

MTP 由 DeepSeek-V3（2024 年 12 月）在大规模上推广，它在预训练期间联合训练 MTP head。Gemma 4 E2B 采取**事后 drafter**方案：一个独立的轻量 transformer，训练它以在给定目标模型隐藏状态的情况下预测未来 token，然后在推理时使用。效果相同——目标感知的候选——但 E2B 不需要对完整目标模型进行联合预训练。

```text
DeepSeek MTP:  integrated into target during pretraining; heads share target weights
Gemma E2B MTP: separate 4-layer transformer trained post-hoc; reads target features
EAGLE-3:       separate head trained post-hoc; similar to E2B but smaller (1–2 layers)

E2B advantage over EAGLE-3: deeper (4 layers) → better multi-hop reasoning for
  subsequent token slots (x_{t+2}, x_{t+3}) which require more context propagation
E2B advantage over 1B external: operates from target's features → higher τ
E2B advantage vs joint MTP (DeepSeek-style): no target retraining required
```

---

## 2. Gemma 4 E2B——架构


<details>
<summary>English original</summary>

**Lecture 06 — Multi-Token Prediction with Gemma 4 E2B: The Official MTP Drafter**

**Collection:** [Gemma 4 Edge Deployment](README.md) | **Previous:** [← Lecture 05](Lecture-05.md)

---

Lecture 04 covered speculative decoding with a Gemma 4 1B external draft and EAGLE-3-style self-draft heads. Google ships a purpose-built solution for the same problem: **Gemma 4 E2B**, a 4-layer, extremely lightweight MTP (Multi-Token Prediction) drafter designed to pair with any full Gemma 4 target. Where the 1B external draft requires 500 MB of extra memory and EAGLE-3 heads require 2–4 hours of training, E2B is the official drop-in drafter with published weights, framework support in vLLM, LiteRT-LM, and llama.cpp — and up to 3× speedup on text generation benchmarks.

This lecture explains what MTP is, how Gemma 4 E2B implements it, how it differs from standard speculative decoding, and how to deploy it on every major inference stack including Jetson edge targets.

---

**Learning objectives**

1. Explain the difference between **standard speculative decoding** (separate draft model) and **MTP** (shared-state parallel token prediction).
2. Describe the Gemma 4 **E2B MTP drafter architecture** — why 4 layers, what it shares with the target, and why it converges to a high acceptance rate.
3. Configure and run MTP speculative decoding with Gemma 4 E2B + 4B/12B/27B target on **vLLM**, **LiteRT-LM**, and **llama.cpp**.
4. Compare the three speculative-decode approaches in this course (1B external, EAGLE-3 heads, E2B MTP) across memory, setup cost, speedup, and framework support.
5. Select the right approach for your deployment target (Orin, Thor, cloud GPU) and task.

---

**1. Multi-Token Prediction — what it is and how it differs**

**1.1 Standard speculative decoding (Lecture 04 recap)**

In standard spec-decode, a **separate draft model** (e.g., Gemma 4 1B) autoregressively generates K token proposals, and the target model verifies them in one parallel forward pass:

```text
Standard spec-decode:
  draft(context) → [x₁, x₂, x₃, x₄]      separate model, 4 steps
  target.verify(context + [x₁,x₂,x₃,x₄])  one parallel pass
  accept prefix, reject first mismatch

Problem: the draft model has DIFFERENT weights, different representations.
  Its token proposals can diverge from target in distribution → low τ on hard tasks.
  Memory cost: full second model weights (500 MB for 1B).
```

**1.2 Multi-Token Prediction (MTP)**

MTP predicts multiple future tokens **from the same model's internal representations**, using auxiliary heads or lightweight transformer modules attached to the target:

```text
MTP approach:
  target.forward(context) → hidden states h₀, h₁, ..., hₙ
  MTP head predicts x_{t+1}, x_{t+2}, ... from h_n (or from partial h)
  Target verifies proposed tokens — but the proposals came from TARGET's own features

Why this converges better:
  The MTP drafter sees the target's internal state, not a separately-learned state.
  Token proposals are in distribution with the target by construction.
  → higher acceptance length τ than an external draft model of equivalent size
```

**1.3 DeepSeek MTP vs Gemma E2B MTP**

MTP was popularized at scale by DeepSeek-V3 (Dec 2024), which trained MTP heads jointly during pretraining. Gemma 4 E2B takes a **post-hoc drafter** approach: a separate lightweight transformer trained to predict future tokens given the target's hidden state, then used at inference time. The effect is the same — target-aware proposals — but E2B doesn't require joint pretraining of the full target.

```text
DeepSeek MTP:  integrated into target during pretraining; heads share target weights
Gemma E2B MTP: separate 4-layer transformer trained post-hoc; reads target features
EAGLE-3:       separate head trained post-hoc; similar to E2B but smaller (1–2 layers)

E2B advantage over EAGLE-3: deeper (4 layers) → better multi-hop reasoning for
  subsequent token slots (x_{t+2}, x_{t+3}) which require more context propagation
E2B advantage over 1B external: operates from target's features → higher τ
E2B advantage vs joint MTP (DeepSeek-style): no target retraining required
```

---

**2. Gemma 4 E2B — architecture**

</details>

### 2.1 “E2B”的含义

**E2B = Edge 2B**（20 亿参数级别，为边缘协同部署设计）。模型结构如下：

```text
gemma-4-E2B-it-assistant (MTP drafter):
  Layers:        4 transformer decoder layers (vs 26 for 4B, 46 for 12B)
  Hidden dim:    matches target Gemma 4 (e.g., 2048 for 4B, 3072 for 12B)
                 required to accept the target's hidden states as input
  Heads:         same head structure as target (GQA 2:1, head_dim=256, QK-norm)
  Vocab:         same 256K tokenizer as all Gemma 4 models
  Parameters:    ~2B (4 × full target-width transformer layers)
  Size on disk:  ~4 GB BF16 / ~1 GB INT4

CRITICAL: the hidden_dim matches the target.
  E2B reads the target's LAST hidden state as its input (instead of generating
  its own from scratch). The 4 transformer layers then propagate that state
  to predict x_{t+1}, x_{t+2}, ... without needing to encode the full context.
```

### 2.2 MTP 投机解码中的前向传播

```text
For each decode step:

1. TARGET forward pass:
   target.forward(context) → last_hidden_state h  (shape: [1, 1, hidden_dim])
                           → logit_t → sample x_t (current token)

2. E2B DRAFT pass (parallel or immediately after):
   e2b.forward(h, x_t) → logit_{t+1} → draft token x̂_{t+1}
   e2b.forward(h', x̂_{t+1}) → logit_{t+2} → draft token x̂_{t+2}
   ... up to K=4 draft steps (typical: K=3–5)

3. TARGET VERIFY pass:
   target.verify(context + x_t + x̂_{t+1} + x̂_{t+2} + ... + x̂_{t+K})
   → one parallel forward → accept/reject per draft token

4. Result: accept longest prefix, emit accepted tokens (typically τ=3–4)
```

E2B 的前向传播代价极低，原因如下：
- 完全跳过上下文编码（直接读取 target 的 h）
- 4 层 vs target 的 26–46 层 → 约为 target 计算量的 10%
- 每次只处理单个 token 位置（而非完整序列）

### 2.3 Jetson 上的内存占用

```text
Deployment             Size         Notes
─────────────────────  ───────────  ─────────────────────────────────────────
E2B INT4 (GGUF)        ~1.0 GB      quantize like any Gemma model
E2B BF16               ~4.0 GB      not recommended for edge
Gemma 4 4B INT4        ~2.2 GB      target model
─────────────────────  ───────────  ─────────────────────────────────────────
TOTAL (4B + E2B INT4)  ~3.2 GB      fits Orin comfortably
TOTAL (12B + E2B INT4) ~7.4 GB      fits Orin 64 GB
TOTAL (27B + E2B INT4) ~15.4 GB     fits Thor 128 GB; tight on Orin (18 GB OK)
```

相比 Lecture 04 中 1B 的外部 draft（500 MB INT4），E2B 的内存开销是其 2 倍，但由于具备 target 状态感知能力，接受长度高出 20–40%。

---

## 3. 框架支持与配置

### 3.1 vLLM（多用户推理服务下吞吐最佳）

vLLM 0.6+ 原生支持 MTP 投机解码：

```python
from vllm import LLM, SamplingParams

# Initialize target + E2B drafter:
llm = LLM(
    model="google/gemma-4-4b-it",
    speculative_model="google/gemma-4-E2B-it-assistant",  # E2B MTP drafter
    num_speculative_tokens=4,              # K=4 draft tokens per step
    speculative_draft_tensor_parallel_size=1,
    # E2B is small enough to not need tensor parallelism
    dtype="bfloat16",
    gpu_memory_utilization=0.9,
    max_model_len=4096,
)

sampling_params = SamplingParams(
    temperature=0.0,   # greedy for maximum acceptance rate
    max_tokens=512,
)

outputs = llm.generate(
    ["Explain how Gemma 4 speculative decoding works."],
    sampling_params
)
print(outputs[0].outputs[0].text)

# Benchmark: measure tokens/s with and without E2B
import time

prompts = ["Write a detailed technical explanation. " * 10] * 8

t0 = time.perf_counter()
outputs = llm.generate(prompts, SamplingParams(max_tokens=200, temperature=0.0))
elapsed = time.perf_counter() - t0
total_tokens = sum(len(o.outputs[0].token_ids) for o in outputs)
print(f"Throughput: {total_tokens / elapsed:.1f} tok/s")
# Expected on A100: ~380 tok/s (vs ~220 tok/s without E2B): ~1.7×
# Expected on Orin (1 request): ~90 tok/s (vs ~55 tok/s): ~1.6×
```

**vLLM 投机解码统计：**

```python
# vLLM logs acceptance rate — look for:
# "Speculative decode draft acceptance rate: 0.78"  → τ ≈ 4 × 0.78 = 3.1
# Enable verbose logging:
import logging
logging.getLogger("vllm.spec_decode").setLevel(logging.DEBUG)
```


<details>
<summary>English original</summary>

**2.1 What "E2B" means**

**E2B = Edge 2B** (2-billion parameter class, designed for edge co-deployment). The model is structured as:

```text
gemma-4-E2B-it-assistant (MTP drafter):
  Layers:        4 transformer decoder layers (vs 26 for 4B, 46 for 12B)
  Hidden dim:    matches target Gemma 4 (e.g., 2048 for 4B, 3072 for 12B)
                 required to accept the target's hidden states as input
  Heads:         same head structure as target (GQA 2:1, head_dim=256, QK-norm)
  Vocab:         same 256K tokenizer as all Gemma 4 models
  Parameters:    ~2B (4 × full target-width transformer layers)
  Size on disk:  ~4 GB BF16 / ~1 GB INT4

CRITICAL: the hidden_dim matches the target.
  E2B reads the target's LAST hidden state as its input (instead of generating
  its own from scratch). The 4 transformer layers then propagate that state
  to predict x_{t+1}, x_{t+2}, ... without needing to encode the full context.
```

**2.2 Forward pass during MTP speculative decode**

```text
For each decode step:

1. TARGET forward pass:
   target.forward(context) → last_hidden_state h  (shape: [1, 1, hidden_dim])
                           → logit_t → sample x_t (current token)

2. E2B DRAFT pass (parallel or immediately after):
   e2b.forward(h, x_t) → logit_{t+1} → draft token x̂_{t+1}
   e2b.forward(h', x̂_{t+1}) → logit_{t+2} → draft token x̂_{t+2}
   ... up to K=4 draft steps (typical: K=3–5)

3. TARGET VERIFY pass:
   target.verify(context + x_t + x̂_{t+1} + x̂_{t+2} + ... + x̂_{t+K})
   → one parallel forward → accept/reject per draft token

4. Result: accept longest prefix, emit accepted tokens (typically τ=3–4)
```

The E2B forward pass is extremely cheap because:
- It skips context encoding entirely (reads target's h directly)
- 4 layers vs target's 26–46 layers → ~10% of target's compute
- Operates on a single token position at a time (not full sequence)

**2.3 Memory footprint on Jetson**

```text
Deployment             Size         Notes
─────────────────────  ───────────  ─────────────────────────────────────────
E2B INT4 (GGUF)        ~1.0 GB      quantize like any Gemma model
E2B BF16               ~4.0 GB      not recommended for edge
Gemma 4 4B INT4        ~2.2 GB      target model
─────────────────────  ───────────  ─────────────────────────────────────────
TOTAL (4B + E2B INT4)  ~3.2 GB      fits Orin comfortably
TOTAL (12B + E2B INT4) ~7.4 GB      fits Orin 64 GB
TOTAL (27B + E2B INT4) ~15.4 GB     fits Thor 128 GB; tight on Orin (18 GB OK)
```

Compared to Lecture 04's 1B external draft (500 MB INT4), E2B costs 2× the memory but achieves 20–40% higher acceptance length due to target-state awareness.

---

**3. Framework support and configuration**

**3.1 vLLM (best throughput for multi-user serving)**

vLLM 0.6+ has native MTP speculative decoding support:

```python
from vllm import LLM, SamplingParams

# Initialize target + E2B drafter:
llm = LLM(
    model="google/gemma-4-4b-it",
    speculative_model="google/gemma-4-E2B-it-assistant",  # E2B MTP drafter
    num_speculative_tokens=4,              # K=4 draft tokens per step
    speculative_draft_tensor_parallel_size=1,
    # E2B is small enough to not need tensor parallelism
    dtype="bfloat16",
    gpu_memory_utilization=0.9,
    max_model_len=4096,
)

sampling_params = SamplingParams(
    temperature=0.0,   # greedy for maximum acceptance rate
    max_tokens=512,
)

outputs = llm.generate(
    ["Explain how Gemma 4 speculative decoding works."],
    sampling_params
)
print(outputs[0].outputs[0].text)

# Benchmark: measure tokens/s with and without E2B
import time

prompts = ["Write a detailed technical explanation. " * 10] * 8

t0 = time.perf_counter()
outputs = llm.generate(prompts, SamplingParams(max_tokens=200, temperature=0.0))
elapsed = time.perf_counter() - t0
total_tokens = sum(len(o.outputs[0].token_ids) for o in outputs)
print(f"Throughput: {total_tokens / elapsed:.1f} tok/s")
# Expected on A100: ~380 tok/s (vs ~220 tok/s without E2B): ~1.7×
# Expected on Orin (1 request): ~90 tok/s (vs ~55 tok/s): ~1.6×
```

**vLLM speculative decode statistics:**

```python
# vLLM logs acceptance rate — look for:
# "Speculative decode draft acceptance rate: 0.78"  → τ ≈ 4 × 0.78 = 3.1
# Enable verbose logging:
import logging
logging.getLogger("vllm.spec_decode").setLevel(logging.DEBUG)
```

</details>

### 3.2 LiteRT-LM（边缘 / Jetson 场景推荐）

LiteRT-LM（原 MediaPipe LLM Inference）是 Google 为移动端和边缘端的 Gemma 4 E2B 推荐的路径。它是唯一在 Android、Linux ARM64 和 Jetson 上提供第一方 E2B MTP 支持的 runtime：

```python
# Install LiteRT-LM (Google AI Edge):
# pip install litert-lm  (requires JetPack 6+ on Jetson)

from litert_lm import LlmInference, LlmInferenceOptions

options = LlmInferenceOptions(
    model_path="/data/gemma4-4b-it.task",           # LiteRT task file (text)
    # Speculative drafter via E2B:
    speculative_decoding_drafter_model_path="/data/gemma4-e2b-it.task",
    num_speculative_tokens=4,                        # K
    # Target runtime:
    num_threads=4,
    accelerator="gpu",                               # Jetson iGPU
    max_tokens=512,
)

inference = LlmInference.create_from_options(options)

# Streaming generation with E2B MTP:
response = ""
for token in inference.generate_response_async("What is speculative decoding?"):
    response += token
    print(token, end="", flush=True)
print()

# Benchmark:
import time
t0 = time.perf_counter()
_ = inference.generate_response("Write a 200-word explanation of transformers.")
elapsed = time.perf_counter() - t0
print(f"Generation time: {elapsed:.2f} s")
```

**E2B 的 LiteRT-LM 模型转换：**

```bash
# Convert E2B drafter to LiteRT task format:
pip install ai-edge-torch ai-edge-litert

python3 - <<'EOF'
import ai_edge_torch
from transformers import AutoModelForCausalLM
import torch

# Convert E2B to LiteRT task file:
drafter = AutoModelForCausalLM.from_pretrained(
    "google/gemma-4-E2B-it-assistant",
    torch_dtype=torch.bfloat16
)
drafter.eval()

sample_input = torch.zeros(1, 1, dtype=torch.long)
edge_drafter = ai_edge_torch.convert(drafter, sample_args=(sample_input,))
edge_drafter.export("gemma4-e2b-it.tflite")
EOF

# Package as LiteRT task file:
# (Use the AI Edge Model Maker or the task_assembler CLI from LiteRT SDK)
litert-task-assembler \
    --model_file gemma4-e2b-it.tflite \
    --output gemma4-e2b-it.task
```

**Jetson Orin 上的吞吐对比（Gemma 4 4B，4K 上下文）：**

| Runtime | 无投机解码 | 启用 E2B MTP（K=4） | 加速比 |
|---------|---------------|-------------------|---------|
| LiteRT-LM CPU | 8 tok/s | 12 tok/s | 1.5× |
| LiteRT-LM GPU delegate | 20 tok/s | 32 tok/s | 1.6× |
| llama.cpp CUDA (sm_87) | 55 tok/s | 90 tok/s | 1.6× |
| vLLM（在 Orin iGPU 上经 ROCm/自定义） | — | — | 视情况而定 |

### 3.3 llama.cpp（GGUF 路径）

llama.cpp 社区通过 `--model-draft` flag 和 GGUF 转换提供对 Gemma 4 E2B MTP 的支持：

```bash
# Step 1: Convert E2B to GGUF
git clone https://github.com/ggml-org/llama.cpp
pip install transformers torch

python3 llama.cpp/convert_hf_to_gguf.py \
    google/gemma-4-E2B-it-assistant \
    --outfile gemma4-e2b-Q4_K_M.gguf \
    --outtype q4_k_m

# Step 2: Run with MTP speculative decoding
# E2B is treated as a draft model in llama.cpp's spec-decode interface:
./build/bin/llama-cli \
    --model       gemma4-4b-it-Q4_K_M.gguf \
    --model-draft gemma4-e2b-Q4_K_M.gguf \
    --n-gpu-layers      9999 \
    --n-gpu-layers-draft 9999 \
    --draft-max   5 \              # K=5 draft tokens (E2B is fast, can push to 5–6)
    --draft-min   1 \
    --draft-p-min 0.3 \            # prune low-probability E2B drafts early
    --ctx-size    4096 \
    -p "Explain the physics of black holes in detail."

# Benchmark comparing E2B vs 1B draft vs no spec-decode:
for draft in "" "gemma4-1b-Q4_K_M.gguf" "gemma4-e2b-Q4_K_M.gguf"; do
    flags="--model gemma4-4b-it-Q4_K_M.gguf --n-gpu-layers 9999 -n 200 -p 128"
    if [ -n "$draft" ]; then
        flags="$flags --model-draft $draft --n-gpu-layers-draft 9999 --draft-max 5"
    fi
    echo "Draft: ${draft:-none}"
    ./build/bin/llama-bench $flags 2>&1 | grep "tg"
done

# Expected on Orin:
#   No draft:     tg = 55 tok/s
#   1B draft:     tg = 80 tok/s (τ ≈ 2.8)
#   E2B draft:    tg = 90 tok/s (τ ≈ 3.3)   ← E2B wins by ~12%
```

**关于 llama.cpp 与 E2B 的说明：** 截至 2026 年中，E2B GGUF 转换与草稿模型支持由社区维护。请查阅 llama.cpp 发版记录和 Google 官方模型卡，获取最新的 GGUF 兼容 E2B 权重。

### 3.4 MLX（Apple Silicon，供完整性参考）

```bash
# mlx-lm supports Gemma 4 E2B via the speculative_drafting flag:
pip install mlx-lm

python3 -m mlx_lm.generate \
    --model google/gemma-4-4b-it \
    --draft-model google/gemma-4-E2B-it-assistant \
    --num-draft-tokens 4 \
    --prompt "Describe speculative decoding." \
    --max-tokens 200
# Expected on M4 Max (128 GB): ~85 tok/s vs ~55 tok/s without E2B
```

---


<details>
<summary>English original</summary>

**3.2 LiteRT-LM (recommended for edge / Jetson)**

LiteRT-LM (formerly MediaPipe LLM Inference) is Google's recommended path for Gemma 4 E2B on mobile and edge. It is the only runtime with first-party E2B MTP support for Android, Linux ARM64, and Jetson:

```python
# Install LiteRT-LM (Google AI Edge):
# pip install litert-lm  (requires JetPack 6+ on Jetson)

from litert_lm import LlmInference, LlmInferenceOptions

options = LlmInferenceOptions(
    model_path="/data/gemma4-4b-it.task",           # LiteRT task file (text)
    # Speculative drafter via E2B:
    speculative_decoding_drafter_model_path="/data/gemma4-e2b-it.task",
    num_speculative_tokens=4,                        # K
    # Target runtime:
    num_threads=4,
    accelerator="gpu",                               # Jetson iGPU
    max_tokens=512,
)

inference = LlmInference.create_from_options(options)

# Streaming generation with E2B MTP:
response = ""
for token in inference.generate_response_async("What is speculative decoding?"):
    response += token
    print(token, end="", flush=True)
print()

# Benchmark:
import time
t0 = time.perf_counter()
_ = inference.generate_response("Write a 200-word explanation of transformers.")
elapsed = time.perf_counter() - t0
print(f"Generation time: {elapsed:.2f} s")
```

**LiteRT-LM model conversion for E2B:**

```bash
# Convert E2B drafter to LiteRT task format:
pip install ai-edge-torch ai-edge-litert

python3 - <<'EOF'
import ai_edge_torch
from transformers import AutoModelForCausalLM
import torch

# Convert E2B to LiteRT task file:
drafter = AutoModelForCausalLM.from_pretrained(
    "google/gemma-4-E2B-it-assistant",
    torch_dtype=torch.bfloat16
)
drafter.eval()

sample_input = torch.zeros(1, 1, dtype=torch.long)
edge_drafter = ai_edge_torch.convert(drafter, sample_args=(sample_input,))
edge_drafter.export("gemma4-e2b-it.tflite")
EOF

# Package as LiteRT task file:
# (Use the AI Edge Model Maker or the task_assembler CLI from LiteRT SDK)
litert-task-assembler \
    --model_file gemma4-e2b-it.tflite \
    --output gemma4-e2b-it.task
```

**Throughput comparison on Jetson Orin (Gemma 4 4B, 4K context):**

| Runtime | No spec-decode | With E2B MTP (K=4) | Speedup |
|---------|---------------|-------------------|---------|
| LiteRT-LM CPU | 8 tok/s | 12 tok/s | 1.5× |
| LiteRT-LM GPU delegate | 20 tok/s | 32 tok/s | 1.6× |
| llama.cpp CUDA (sm_87) | 55 tok/s | 90 tok/s | 1.6× |
| vLLM (on Orin iGPU via ROCm/custom) | — | — | varies |

**3.3 llama.cpp (GGUF path)**

llama.cpp community support for Gemma 4 E2B MTP via the `--model-draft` flag and GGUF conversion:

```bash
# Step 1: Convert E2B to GGUF
git clone https://github.com/ggml-org/llama.cpp
pip install transformers torch

python3 llama.cpp/convert_hf_to_gguf.py \
    google/gemma-4-E2B-it-assistant \
    --outfile gemma4-e2b-Q4_K_M.gguf \
    --outtype q4_k_m

# Step 2: Run with MTP speculative decoding
# E2B is treated as a draft model in llama.cpp's spec-decode interface:
./build/bin/llama-cli \
    --model       gemma4-4b-it-Q4_K_M.gguf \
    --model-draft gemma4-e2b-Q4_K_M.gguf \
    --n-gpu-layers      9999 \
    --n-gpu-layers-draft 9999 \
    --draft-max   5 \              # K=5 draft tokens (E2B is fast, can push to 5–6)
    --draft-min   1 \
    --draft-p-min 0.3 \            # prune low-probability E2B drafts early
    --ctx-size    4096 \
    -p "Explain the physics of black holes in detail."

# Benchmark comparing E2B vs 1B draft vs no spec-decode:
for draft in "" "gemma4-1b-Q4_K_M.gguf" "gemma4-e2b-Q4_K_M.gguf"; do
    flags="--model gemma4-4b-it-Q4_K_M.gguf --n-gpu-layers 9999 -n 200 -p 128"
    if [ -n "$draft" ]; then
        flags="$flags --model-draft $draft --n-gpu-layers-draft 9999 --draft-max 5"
    fi
    echo "Draft: ${draft:-none}"
    ./build/bin/llama-bench $flags 2>&1 | grep "tg"
done

# Expected on Orin:
#   No draft:     tg = 55 tok/s
#   1B draft:     tg = 80 tok/s (τ ≈ 2.8)
#   E2B draft:    tg = 90 tok/s (τ ≈ 3.3)   ← E2B wins by ~12%
```

**Note on llama.cpp and E2B:** as of mid-2026, E2B GGUF conversion and draft support is community-maintained. Check llama.cpp releases and the official Google model card for the latest GGUF-compatible E2B weights.

**3.4 MLX (Apple Silicon, for completeness)**

```bash
# mlx-lm supports Gemma 4 E2B via the speculative_drafting flag:
pip install mlx-lm

python3 -m mlx_lm.generate \
    --model google/gemma-4-4b-it \
    --draft-model google/gemma-4-E2B-it-assistant \
    --num-draft-tokens 4 \
    --prompt "Describe speculative decoding." \
    --max-tokens 200
# Expected on M4 Max (128 GB): ~85 tok/s vs ~55 tok/s without E2B
```

---

</details>

## 4. E2B 与替代方案的接受长度分析

接受长度 τ 是首要的性能杠杆。以下是跨任务的校准对比：

| 任务 | τ（1B 外部 draft） | τ（EAGLE-3 heads） | τ（E2B MTP） |
|------|----------------|-------------------|-------------|
| 代码生成 | 3.5–4.5 | 3.8–5.0 | 4.0–5.5 |
| 技术问答 | 3.0–4.0 | 3.5–4.5 | 3.8–5.0 |
| 创意写作 | 2.5–3.5 | 3.0–4.0 | 3.2–4.3 |
| 数学 / CoT 推理 | 2.0–3.0 | 2.5–3.5 | 2.8–3.8 |
| 重复性 / 模板化 | 4.5–6.0 | 5.0–6.5 | 5.0–7.0 |
| 指令遵循 | 3.0–4.0 | 3.5–4.5 | 3.8–5.0 |

在各任务类别上，E2B 的 τ 一致地比 1B 外部 draft 高 0.3–0.5，因为 E2B 读取 target 自身的 hidden state——其提议已与 target 分布对齐。

**E2B 何时不如 1B 外部 draft：**

在病态情形下（极短的输出、temperature > 0.9 的高度随机采样），相比 1B 模型更简单的预测，E2B 的 4 层架构有时会超出分布。要获得最佳 E2B 接受率，请使用 temperature ≤ 0.7。

---

## 5. 正面对比：三种推测 decode 方案

本课程已覆盖三种方案。以下是决策矩阵：

| 判据 | 1B 外部 draft | EAGLE-3 heads | E2B MTP |
|-----------|------------------|---------------|---------|
| 额外内存 | 500 MB INT4 | 150 MB | 1,000 MB INT4 |
| 准备成本 | 零（下载） | 2–4h 训练 | 零（下载） |
| 接受长度 τ | 2.5–4.0 | 3.5–5.0 | 3.8–5.5 |
| vLLM 支持 | ✓ (--speculative) | 部分 | ✓（原生） |
| LiteRT-LM 支持 | ✗ | ✗ | ✓（第一方） |
| llama.cpp 支持 | ✓ (--model-draft) | ✗ | ✓ (--model-draft) |
| 贪心 decode 精度一致性 | ✓ 有保证 | ✓ 有保证 | ✓ 有保证 |
| 输出质量变化 | 无（无损） | 无（无损） | 无（无损） |
| Jetson 上最佳（功耗） | 中等 | 最佳 | 良好 |
| Google 官方 | ✗ | ✗ | ✓ |

**决策指南：**

```text
Use 1B external draft when:
  → Fastest to deploy, no training, already have 1B weights on device
  → llama.cpp or vLLM, memory is tight, E2B's 1 GB is too expensive

Use EAGLE-3 heads when:
  → Memory-constrained (Orin Nano, 8–16 GB) and you can spare training time
  → Target is 4B/12B and you want the lowest memory overhead per acceptance gain

Use E2B MTP when:
  → LiteRT-LM / Google AI Edge ecosystem (only option with first-party support)
  → vLLM multi-user serving where τ improvements compound across many requests
  → You want the highest acceptance rate without any training investment
  → Production deployment where Google-official weights reduce maintenance risk
```

---


<details>
<summary>English original</summary>

**4. Acceptance length analysis for E2B vs alternatives**

The acceptance length τ is the primary performance lever. Here is a calibrated comparison across tasks:

| Task | τ (1B external) | τ (EAGLE-3 heads) | τ (E2B MTP) |
|------|----------------|-------------------|-------------|
| Code generation | 3.5–4.5 | 3.8–5.0 | 4.0–5.5 |
| Technical Q&A | 3.0–4.0 | 3.5–4.5 | 3.8–5.0 |
| Creative writing | 2.5–3.5 | 3.0–4.0 | 3.2–4.3 |
| Math / CoT reasoning | 2.0–3.0 | 2.5–3.5 | 2.8–3.8 |
| Repetitive / templated | 4.5–6.0 | 5.0–6.5 | 5.0–7.0 |
| Instruction following | 3.0–4.0 | 3.5–4.5 | 3.8–5.0 |

E2B consistently achieves 0.3–0.5 higher τ than the 1B external draft across task categories, because E2B reads the target's own hidden state — its proposals are already aligned with the target distribution.

**When E2B underperforms 1B external draft:**

In pathological cases (very short outputs, highly random sampling temperature > 0.9), E2B's 4-layer architecture sometimes overshoots the distribution compared to the 1B model's simpler predictions. Use temperature ≤ 0.7 for best E2B acceptance rates.

---

**5. Head-to-head comparison: three speculative decode approaches**

This course has covered three approaches. Here is the decision matrix:

| Criterion | 1B External Draft | EAGLE-3 Heads | E2B MTP |
|-----------|------------------|---------------|---------|
| Extra memory | 500 MB INT4 | 150 MB | 1,000 MB INT4 |
| Setup effort | Zero (download) | 2–4h training | Zero (download) |
| Acceptance length τ | 2.5–4.0 | 3.5–5.0 | 3.8–5.5 |
| vLLM support | ✓ (--speculative) | Partial | ✓ (native) |
| LiteRT-LM support | ✗ | ✗ | ✓ (first-party) |
| llama.cpp support | ✓ (--model-draft) | ✗ | ✓ (--model-draft) |
| Greedy-decode parity | ✓ guaranteed | ✓ guaranteed | ✓ guaranteed |
| Output quality change | None (lossless) | None (lossless) | None (lossless) |
| Best on Jetson (power) | Medium | Best | Good |
| Google-official | ✗ | ✗ | ✓ |

**Decision guide:**

```text
Use 1B external draft when:
  → Fastest to deploy, no training, already have 1B weights on device
  → llama.cpp or vLLM, memory is tight, E2B's 1 GB is too expensive

Use EAGLE-3 heads when:
  → Memory-constrained (Orin Nano, 8–16 GB) and you can spare training time
  → Target is 4B/12B and you want the lowest memory overhead per acceptance gain

Use E2B MTP when:
  → LiteRT-LM / Google AI Edge ecosystem (only option with first-party support)
  → vLLM multi-user serving where τ improvements compound across many requests
  → You want the highest acceptance rate without any training investment
  → Production deployment where Google-official weights reduce maintenance risk
```

---

</details>

## 6. 部署 recipe：Jetson Orin 上的 E2B + Gemma 4 4B

从零开始的完整部署：

```bash
# Prerequisites: JetPack 6.0+, CUDA sm_87, Python 3.10+
# Estimated time: 20 minutes

# 1. Download models:
pip install huggingface-hub
python3 -c "
from huggingface_hub import snapshot_download
snapshot_download('google/gemma-4-4b-it', local_dir='./gemma4-4b')
snapshot_download('google/gemma-4-E2B-it-assistant', local_dir='./gemma4-e2b')
"

# 2. Build llama.cpp for Orin:
git clone https://github.com/ggml-org/llama.cpp
cmake -B llama.cpp/build -S llama.cpp -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES="87"
cmake --build llama.cpp/build --config Release -j$(nproc)

# 3. Convert both models to GGUF INT4:
python3 llama.cpp/convert_hf_to_gguf.py ./gemma4-4b  --outfile gemma4-4b-Q4_K_M.gguf  --outtype q4_k_m
python3 llama.cpp/convert_hf_to_gguf.py ./gemma4-e2b --outfile gemma4-e2b-Q4_K_M.gguf --outtype q4_k_m

# 4. Quick sanity check:
./llama.cpp/build/bin/llama-cli \
    --model gemma4-4b-Q4_K_M.gguf \
    --model-draft gemma4-e2b-Q4_K_M.gguf \
    --n-gpu-layers 9999 --n-gpu-layers-draft 9999 \
    --draft-max 4 --ctx-size 2048 \
    -p "What is speculative decoding?" -n 100 2>&1 | tail -5

# 5. Benchmark (compare baseline vs E2B):
echo "=== Baseline (no spec decode) ===" && \
./llama.cpp/build/bin/llama-bench \
    --model gemma4-4b-Q4_K_M.gguf --n-gpu-layers 9999 -n 200 -p 128

echo "=== E2B MTP spec decode ===" && \
./llama.cpp/build/bin/llama-bench \
    --model gemma4-4b-Q4_K_M.gguf \
    --model-draft gemma4-e2b-Q4_K_M.gguf \
    --n-gpu-layers 9999 --n-gpu-layers-draft 9999 \
    --draft-max 4 -n 200 -p 128

# Expected:
#   Baseline:  tg = 55 tok/s
#   E2B MTP:   tg = 85–95 tok/s  (~1.6×)

# 6. Run as server (production mode):
./llama.cpp/build/bin/llama-server \
    --model      gemma4-4b-Q4_K_M.gguf \
    --model-draft gemma4-e2b-Q4_K_M.gguf \
    --n-gpu-layers 9999 --n-gpu-layers-draft 9999 \
    --draft-max 4 \
    --ctx-size 4096 \
    --kv-cache-type q8_0 \         # INT8 KV saves memory
    --parallel 2 \                  # 2 concurrent requests
    --port 8080 &

# Test the server:
curl http://localhost:8080/v1/completions -s \
    -H "Content-Type: application/json" \
    -d '{"model":"gemma4-4b","prompt":"Describe MTP speculative decoding.","max_tokens":100}' \
    | python3 -m json.tool | grep -E "text|usage"
```

---

## 关键要点

- **Gemma 4 E2B** 是一个 4 层、约 2B 参数的 MTP 草稿模型，以目标模型的最后一层 hidden state 作为输入——它从不重新编码上下文，只根据目标模型自身的特征预测接下来的几个 token。
- **MTP 的接受率高于外部草稿模型**（τ=3.8–5.5，而 1B 为 2.5–4.0），因为 E2B 的提议锚定在目标模型的内部状态上，消除了跨模型分布失配。
- **LiteRT-LM 是边缘/Jetson 的第一方路径**——Google 在 AI Edge SDK 中原生提供 E2B MTP 支持，使其成为 Jetson Orin/Thor 与 Android 上阻力最小的部署选项。
- **vLLM 的 `speculative_model` flag** 用一个参数即可接入 E2B MTP，用于多用户推理服务；GPU 上预期加速约 1.6–1.8×。
- **llama.cpp `--model-draft`** 把 E2B 当作常规草稿模型使用——这行得通，并能获得 E2B 的接受率收益，尽管 llama.cpp 内部并不原生区分 MTP 与标准 spec-decode。
- **内存开销约 1 GB INT4**——是 1B 外部草稿模型的两倍，但 E2B 由 Google 官方出品且无需任何训练，因此在内存不极度紧张时，它是生产环境的正确默认选择。
- 三种方案（1B 外部草稿、EAGLE-3 heads、E2B MTP）在 **greedy decode 下均无损**——输出分布可证明与目标模型单独运行完全一致。

---

## 参考文献

- Gemma 4 E2B 模型卡——HuggingFace 上的 `google/gemma-4-E2B-it-assistant`
- "Multi-Token Prediction"（Gloeckle et al., Meta, 2024）——arXiv:2404.19737
- DeepSeek-V3 MTP 训练集成（2024）——arXiv:2412.19437
- LiteRT-LM MTP 投机解码文档——[ai.google.dev/edge/litert-lm](https://ai.google.dev/edge/litert-lm)
- vLLM 投机解码（`speculative_model`）——vllm.readthedocs.io/en/latest/features/spec_decode.html
- llama.cpp 投机解码（--model-draft）——[github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- *Gemma 4 Edge Deployment — Lecture 04*——标准 spec-decode、EAGLE-3、接受率计算——[Lecture-04.md](Lecture-04.md)

---


<details>
<summary>English original</summary>

**6. Deployment recipe: E2B + Gemma 4 4B on Jetson Orin**

Complete deployment from scratch:

```bash
# Prerequisites: JetPack 6.0+, CUDA sm_87, Python 3.10+
# Estimated time: 20 minutes

# 1. Download models:
pip install huggingface-hub
python3 -c "
from huggingface_hub import snapshot_download
snapshot_download('google/gemma-4-4b-it', local_dir='./gemma4-4b')
snapshot_download('google/gemma-4-E2B-it-assistant', local_dir='./gemma4-e2b')
"

# 2. Build llama.cpp for Orin:
git clone https://github.com/ggml-org/llama.cpp
cmake -B llama.cpp/build -S llama.cpp -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES="87"
cmake --build llama.cpp/build --config Release -j$(nproc)

# 3. Convert both models to GGUF INT4:
python3 llama.cpp/convert_hf_to_gguf.py ./gemma4-4b  --outfile gemma4-4b-Q4_K_M.gguf  --outtype q4_k_m
python3 llama.cpp/convert_hf_to_gguf.py ./gemma4-e2b --outfile gemma4-e2b-Q4_K_M.gguf --outtype q4_k_m

# 4. Quick sanity check:
./llama.cpp/build/bin/llama-cli \
    --model gemma4-4b-Q4_K_M.gguf \
    --model-draft gemma4-e2b-Q4_K_M.gguf \
    --n-gpu-layers 9999 --n-gpu-layers-draft 9999 \
    --draft-max 4 --ctx-size 2048 \
    -p "What is speculative decoding?" -n 100 2>&1 | tail -5

# 5. Benchmark (compare baseline vs E2B):
echo "=== Baseline (no spec decode) ===" && \
./llama.cpp/build/bin/llama-bench \
    --model gemma4-4b-Q4_K_M.gguf --n-gpu-layers 9999 -n 200 -p 128

echo "=== E2B MTP spec decode ===" && \
./llama.cpp/build/bin/llama-bench \
    --model gemma4-4b-Q4_K_M.gguf \
    --model-draft gemma4-e2b-Q4_K_M.gguf \
    --n-gpu-layers 9999 --n-gpu-layers-draft 9999 \
    --draft-max 4 -n 200 -p 128

# Expected:
#   Baseline:  tg = 55 tok/s
#   E2B MTP:   tg = 85–95 tok/s  (~1.6×)

# 6. Run as server (production mode):
./llama.cpp/build/bin/llama-server \
    --model      gemma4-4b-Q4_K_M.gguf \
    --model-draft gemma4-e2b-Q4_K_M.gguf \
    --n-gpu-layers 9999 --n-gpu-layers-draft 9999 \
    --draft-max 4 \
    --ctx-size 4096 \
    --kv-cache-type q8_0 \         # INT8 KV saves memory
    --parallel 2 \                  # 2 concurrent requests
    --port 8080 &

# Test the server:
curl http://localhost:8080/v1/completions -s \
    -H "Content-Type: application/json" \
    -d '{"model":"gemma4-4b","prompt":"Describe MTP speculative decoding.","max_tokens":100}' \
    | python3 -m json.tool | grep -E "text|usage"
```

---

**Key takeaways**

- **Gemma 4 E2B** is a 4-layer, ~2B-parameter MTP drafter that reads the target model's last hidden state as input — it never re-encodes context, just predicts the next few tokens from the target's own features.
- **MTP achieves higher acceptance rates than external drafts** (τ=3.8–5.5 vs 2.5–4.0 for 1B) because E2B's proposals are anchored to the target's internal state, eliminating cross-model distribution mismatch.
- **LiteRT-LM is the first-party path for edge/Jetson** — Google ships E2B MTP support natively in the AI Edge SDK, making it the lowest-friction deployment option on Jetson Orin/Thor and Android.
- **vLLM's `speculative_model` flag** wires E2B MTP in one parameter for multi-user serving; expected speedup ~1.6–1.8× on GPU.
- **llama.cpp `--model-draft`** uses E2B as a conventional draft model — this works and gives the E2B acceptance rate benefit even though llama.cpp doesn't natively distinguish MTP from standard spec-decode internals.
- **Memory cost is ~1 GB INT4** — twice the 1B external draft, but E2B is Google-official and requires zero training, which makes it the right default for production unless memory is critically tight.
- All three approaches (1B external, EAGLE-3 heads, E2B MTP) are **lossless for greedy decode** — the output distribution is provably identical to the target running alone.

---

**References**

- Gemma 4 E2B model card — `google/gemma-4-E2B-it-assistant` on HuggingFace
- "Multi-Token Prediction" (Gloeckle et al., Meta, 2024) — arXiv:2404.19737
- DeepSeek-V3 MTP training integration (2024) — arXiv:2412.19437
- LiteRT-LM MTP speculative decode docs — [ai.google.dev/edge/litert-lm](https://ai.google.dev/edge/litert-lm)
- vLLM speculative decoding (`speculative_model`) — vllm.readthedocs.io/en/latest/features/spec_decode.html
- llama.cpp speculative decode (--model-draft) — [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- *Gemma 4 Edge Deployment — Lecture 04* — standard spec-decode, EAGLE-3, acceptance math — [Lecture-04.md](Lecture-04.md)

---

</details>

## 截至 2026-06

Gemma 4 E2B MTP drafter 已在 HuggingFace 上提供，地址为 `google/gemma-4-E2B-it-assistant`。LiteRT-LM 的 MTP 支持：需要 litert-lm ≥ 0.2.0 以及 AI Edge SDK ≥ 2026 年 6 月构建版。vLLM 的 MTP 支持：需要 vllm ≥ 0.6.0 并配合 `speculative_model="google/gemma-4-E2B-it-assistant"`。llama.cpp GGUF 转换：请查看最新的 llama.cpp 发布版是否支持 Gemma 4 E2B 架构（可能需要更新 convert_hf_to_gguf.py）。Google 公布的 benchmark：在内部 benchmark 上测得最高 3× 推理加速（文本、仅英文；你在 Jetson 上的结果会因任务而异）。

---

*上一节：[← Lecture 05 — Physical AI and Multimodal Gemma 4](Lecture-05.md) · 上级：[Gemma 4 Edge Deployment](README.md)*


<details>
<summary>English original</summary>

**Current as of 2026-06**

Gemma 4 E2B MTP drafter available at `google/gemma-4-E2B-it-assistant` on HuggingFace. LiteRT-LM MTP support: requires litert-lm ≥ 0.2.0 and AI Edge SDK ≥ June 2026 build. vLLM MTP support: requires vllm ≥ 0.6.0 with `speculative_model="google/gemma-4-E2B-it-assistant"`. llama.cpp GGUF conversion: check latest llama.cpp release for Gemma 4 E2B architecture support (may need updated convert_hf_to_gguf.py). Google-published benchmark: up to 3× inference speedup measured on internal benchmarks (text, English-only; your Jetson results will vary by task).

---

*Previous: [← Lecture 05 — Physical AI and Multimodal Gemma 4](Lecture-05.md) · Up: [Gemma 4 Edge Deployment](README.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Gemma 4 Edge Deployment/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Gemma%204%20Edge%20Deployment/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
