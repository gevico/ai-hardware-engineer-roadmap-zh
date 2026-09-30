---
title: 02 — L40S 上的推理优化
description: 02 — L40S 上的推理优化
---

# 02 — L40S 上的推理优化

## 1. 为什么 L40S 推理与 H200 不同

| 约束 | 影响 | 缓解措施 |
|---|---|---|
| 864 GB/s 对 4.8 TB/s HBM | decode（逐 token 生成阶段）的带宽受限程度高 5× | 加大批大小、量化 |
| GPU 间走 PCIe x16 | 全规约慢 14× | 最小化 TP 度、使用流水线并行 |
| 每 GPU 48 GB | 模型分片更小 | 更激进的量化（INT4） |
| 无 NVLink | 张量并行延迟高 | 多 GPU 优先用流水线并行 |
| FP8（无 TE 硬件缩放） | 需手工量化 | 离线使用 GPTQ/AWQ |

## 2. 量化：对 L40S 至关重要

量化在 L40S 上比在 H200 上更重要，因为：
1. 每 GPU 内存更小 → 更大的模型需要更强的压缩
2. 内存带宽更低 → 量化算子可改善带宽受限场景的性能

### GPTQ（训练后量化）

```bash
# Install AutoGPTQ
pip install auto-gptq

# Quantize Llama-3 70B to INT4 (GPTQ)
python - <<'EOF'
from transformers import AutoTokenizer
from auto_gptq import AutoGPTQForCausalLM, BaseQuantizeConfig

model_name = "meta-llama/Llama-3-70b"
tokenizer = AutoTokenizer.from_pretrained(model_name)

quantize_config = BaseQuantizeConfig(
    bits=4,               # INT4 quantization
    group_size=128,       # quantization group size (128 is standard)
    damp_percent=0.01,
    desc_act=True,        # activation ordering (better quality)
)

model = AutoGPTQForCausalLM.from_pretrained(
    model_name,
    quantize_config=quantize_config,
    device_map="auto",
)

# Calibration data (128 samples, 2048 tokens each)
examples = [tokenizer("calibration text " * 200, return_tensors="pt")]
model.quantize(examples)
model.save_quantized("/models/llama-3-70b-gptq-int4")
EOF
```

INT4 GPTQ 的内存节省：
- FP16：140 GB（70B 模型）
- INT4：约 35 GB（70B 模型）→ 能装进 **1 张 L40S！**（受 KV cache 限制）

### AWQ（激活感知权重量化）

```bash
pip install autoawq

python - <<'EOF'
from awq import AutoAWQForCausalLM
from transformers import AutoTokenizer

model_path = "meta-llama/Llama-3-70b"
quant_path = "/models/llama-3-70b-awq-int4"

model = AutoAWQForCausalLM.from_pretrained(model_path, device_map="cuda:0")
tokenizer = AutoTokenizer.from_pretrained(model_path)

quant_config = {
    "zero_point": True,     # zero-point quantization (better quality)
    "q_group_size": 128,    # group size
    "w_bit": 4,             # INT4
    "version": "GEMM",      # GEMM or GEMV kernel
}

model.quantize(tokenizer, quant_config=quant_config)
model.save_quantized(quant_path)
tokenizer.save_pretrained(quant_path)
EOF
```

AWQ 与 GPTQ 对比：
- AWQ：困惑度略好，推理更快（优化过的 GEMM（矩阵-矩阵乘）kernel）
- GPTQ：控制力更强，`desc_act=True` 质量最佳
- 两者：相比 FP16 内存减少约 4×

### 面向 L40S 的 FP8 静态量化

```python
# For L40S, use offline FP8 quantization (no hardware TE scaling)
from transformers import AutoModelForCausalLM
import torch

model = AutoModelForCausalLM.from_pretrained(
    "meta-llama/Llama-3-8b",
    torch_dtype=torch.float8_e4m3fn,  # requires PyTorch 2.1+
    device_map="cuda:0",
)
# Note: FP8 on Ada gives ~1.4× speedup vs FP16 (vs ~2× on Hopper with TE)
```

### 量化决策指南

| 模型规模 | L40S 策略 | 所需 GPU 数 | 备注 |
|---|---|---|---|
| 7B | FP16 或 BF16 | 1 | 14 GB，快，无精度损失 |
| 13B | FP16 | 1 | 26 GB，配小 KV cache 可装下 |
| 34B | INT8 或 AWQ INT4 | 1-2 | INT8：34 GB（1 张 GPU），INT4：17 GB |
| 70B | AWQ/GPTQ INT4 | 1-2 | INT4：35 GB（1 张 GPU），最大上下文受限 |
| 180B | GPTQ INT4 | 4-5 | 共 90 GB，需多 GPU |

## 3. L40S 的 vLLM 配置

```python
from vllm import LLM, SamplingParams

# Single GPU, 7B model — standard deployment
llm = LLM(
    model="meta-llama/Llama-3-8b-instruct",
    dtype="bfloat16",
    max_model_len=8192,
    gpu_memory_utilization=0.90,
    max_num_seqs=128,
)

# Single GPU, 70B INT4 AWQ — fits on 1 L40S
llm = LLM(
    model="/models/llama-3-70b-awq-int4",
    quantization="awq",
    dtype="float16",
    max_model_len=4096,             # limit context due to 48 GB constraint
    gpu_memory_utilization=0.85,    # leave room for KV cache
    max_num_seqs=64,
)

# Multi-GPU, 70B BF16 — 2 L40S with TP=2
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",
    tensor_parallel_size=2,         # PCIe limited; keep TP low
    dtype="bfloat16",
    max_model_len=8192,
    gpu_memory_utilization=0.90,
)
```


<details>
<summary>English original</summary>

**02 — Inference Optimization on L40S**

**1. Why L40S Inference is Different from H200**

| Constraint | Impact | Mitigation |
|---|---|---|
| 864 GB/s vs 4.8 TB/s HBM | Decode is 5× more memory-bound | Larger batches, quantization |
| PCIe x16 for GPU-GPU | All-reduce is 14× slower | Minimize TP degree, use pipeline parallel |
| 48 GB per GPU | Smaller model shards | More aggressive quantization (INT4) |
| No NVLink | High latency tensor parallel | Prefer pipeline parallelism for multi-GPU |
| FP8 (no TE hardware scaling) | Manual quantization required | Use GPTQ/AWQ offline |

**2. Quantization: Essential for L40S**

Quantization is more important on L40S than H200 because:
1. Smaller per-GPU memory → larger models need more compression
2. Lower memory bandwidth → quantized ops improve memory-bound performance

**GPTQ (Post-Training Quantization)**

```bash
# Install AutoGPTQ
pip install auto-gptq

# Quantize Llama-3 70B to INT4 (GPTQ)
python - <<'EOF'
from transformers import AutoTokenizer
from auto_gptq import AutoGPTQForCausalLM, BaseQuantizeConfig

model_name = "meta-llama/Llama-3-70b"
tokenizer = AutoTokenizer.from_pretrained(model_name)

quantize_config = BaseQuantizeConfig(
    bits=4,               # INT4 quantization
    group_size=128,       # quantization group size (128 is standard)
    damp_percent=0.01,
    desc_act=True,        # activation ordering (better quality)
)

model = AutoGPTQForCausalLM.from_pretrained(
    model_name,
    quantize_config=quantize_config,
    device_map="auto",
)

# Calibration data (128 samples, 2048 tokens each)
examples = [tokenizer("calibration text " * 200, return_tensors="pt")]
model.quantize(examples)
model.save_quantized("/models/llama-3-70b-gptq-int4")
EOF
```

INT4 GPTQ memory savings:
- FP16: 140 GB (70B model)
- INT4: ~35 GB (70B model) → fits on **1 L40S!** (with KV cache limits)

**AWQ (Activation-Aware Weight Quantization)**

```bash
pip install autoawq

python - <<'EOF'
from awq import AutoAWQForCausalLM
from transformers import AutoTokenizer

model_path = "meta-llama/Llama-3-70b"
quant_path = "/models/llama-3-70b-awq-int4"

model = AutoAWQForCausalLM.from_pretrained(model_path, device_map="cuda:0")
tokenizer = AutoTokenizer.from_pretrained(model_path)

quant_config = {
    "zero_point": True,     # zero-point quantization (better quality)
    "q_group_size": 128,    # group size
    "w_bit": 4,             # INT4
    "version": "GEMM",      # GEMM or GEMV kernel
}

model.quantize(tokenizer, quant_config=quant_config)
model.save_quantized(quant_path)
tokenizer.save_pretrained(quant_path)
EOF
```

AWQ vs GPTQ comparison:
- AWQ: slightly better perplexity, faster inference (optimized GEMM kernels)
- GPTQ: more control, `desc_act=True` gives best quality
- Both: ~4× memory reduction vs FP16

**FP8 Static Quantization for L40S**

```python
# For L40S, use offline FP8 quantization (no hardware TE scaling)
from transformers import AutoModelForCausalLM
import torch

model = AutoModelForCausalLM.from_pretrained(
    "meta-llama/Llama-3-8b",
    torch_dtype=torch.float8_e4m3fn,  # requires PyTorch 2.1+
    device_map="cuda:0",
)
# Note: FP8 on Ada gives ~1.4× speedup vs FP16 (vs ~2× on Hopper with TE)
```

**Quantization Decision Guide**

| Model Size | L40S Strategy | GPUs Needed | Notes |
|---|---|---|---|
| 7B | FP16 or BF16 | 1 | 14 GB, fast, no quality loss |
| 13B | FP16 | 1 | 26 GB, fits with small KV cache |
| 34B | INT8 or AWQ INT4 | 1-2 | INT8: 34 GB (1 GPU), INT4: 17 GB |
| 70B | AWQ/GPTQ INT4 | 1-2 | INT4: 35 GB (1 GPU), max context limited |
| 180B | GPTQ INT4 | 4-5 | 90 GB total, need multi-GPU |

**3. vLLM Configuration for L40S**

```python
from vllm import LLM, SamplingParams

# Single GPU, 7B model — standard deployment
llm = LLM(
    model="meta-llama/Llama-3-8b-instruct",
    dtype="bfloat16",
    max_model_len=8192,
    gpu_memory_utilization=0.90,
    max_num_seqs=128,
)

# Single GPU, 70B INT4 AWQ — fits on 1 L40S
llm = LLM(
    model="/models/llama-3-70b-awq-int4",
    quantization="awq",
    dtype="float16",
    max_model_len=4096,             # limit context due to 48 GB constraint
    gpu_memory_utilization=0.85,    # leave room for KV cache
    max_num_seqs=64,
)

# Multi-GPU, 70B BF16 — 2 L40S with TP=2
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",
    tensor_parallel_size=2,         # PCIe limited; keep TP low
    dtype="bfloat16",
    max_model_len=8192,
    gpu_memory_utilization=0.90,
)
```

</details>

### 面向 PCIe 系统的 vLLM 调优

```bash
# L40S-specific vLLM launch flags
python -m vllm.entrypoints.openai.api_server \
    --model meta-llama/Llama-3-8b-instruct \
    --dtype bfloat16 \
    --max-model-len 8192 \
    --gpu-memory-utilization 0.90 \
    --max-num-seqs 256 \
    --max-num-batched-tokens 32768 \
    --block-size 16 \
    --port 8000

# For GPTQ/AWQ quantized models
python -m vllm.entrypoints.openai.api_server \
    --model /models/llama-3-70b-awq \
    --quantization awq \
    --max-model-len 4096 \
    --gpu-memory-utilization 0.85 \
    --max-num-seqs 128
```

## 4. 连续批处理策略

### L40S 的最优批大小

与适合大 batch size 的 H200 不同，L40S 的内存约束更紧：

```
7B model on L40S (48 GB total):
  Weights (BF16): 14 GB
  CUDA reserved:  ~2 GB
  Available KV:   ~32 GB

KV cache per token (Llama-3 8B, FP16):
  2 × 32 layers × 8 kv-heads × 128 head-dim × 2 bytes = 131 KB/token

Max concurrent tokens at BS=256, seq=256:
  256 × 256 = 65,536 tokens × 131 KB = ~8.6 GB → fits ✓

Sweet spot for L40S 7B: batch_size=128-256
```

```python
# Benchmark batch sizes to find throughput peak
import torch, time
from transformers import AutoModelForCausalLM, AutoTokenizer

model = AutoModelForCausalLM.from_pretrained(
    "meta-llama/Llama-3-8b",
    torch_dtype=torch.bfloat16,
    device_map="cuda:0",
)
model.eval()

for batch_size in [1, 4, 16, 32, 64, 128]:
    input_ids = torch.randint(0, 32000, (batch_size, 128), device="cuda:0")

    with torch.no_grad(), torch.autocast("cuda", dtype=torch.bfloat16):
        for _ in range(3): model(input_ids)  # warmup
        t0 = time.perf_counter()
        for _ in range(20): model(input_ids)
        torch.cuda.synchronize()
        elapsed = time.perf_counter() - t0

    tps = batch_size * 128 * 20 / elapsed
    print(f"BS={batch_size:4d}: {tps:8.0f} tokens/s")
```

## 5. 投机解码

投机解码在 L40S 上尤其有效，因为 decode（逐 token 生成阶段）高度受限于内存带宽：

```python
# L40S speculative decoding setup
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",   # target model (2 GPUs, BF16)
    speculative_model="meta-llama/Llama-3-8b-instruct",  # draft on 1 GPU
    num_speculative_tokens=5,
    tensor_parallel_size=2,
)
```

替代方案：使用极小的 draft model（< 1B）可获得更大加速：

```python
llm = LLM(
    model="meta-llama/Llama-3-8b-instruct",
    speculative_model="TinyLlama/TinyLlama-1.1B-Chat-v1.0",
    num_speculative_tokens=6,
    speculative_max_model_len=4096,
)
# Typical speedup: 1.5-2.5x on L40S (memory-bound decode benefits most)
```

## 6. KV cache 量化

在 L40S（48 GB）上，KV cache 压缩对长上下文至关重要：

```python
# vLLM with FP8 KV cache
llm = LLM(
    model="meta-llama/Llama-3-8b-instruct",
    kv_cache_dtype="fp8",       # cuts KV cache memory by 50%
    max_model_len=32768,        # now supports 32K context on single L40S
)

# FP8 KV cache impact on L40S 7B:
# FP16 KV: 131 KB/token  → 32K context needs 4.2 GB (max ~240 batch sequences at 128 tokens)
# FP8 KV:  66 KB/token   → 32K context needs 2.1 GB (nearly 2× more sequences)
```

## 7. Flash Attention 2（L40S）

L40S 支持 Flash Attention 2（不支持 FA3，后者为 Hopper 专属）：

```bash
pip install flash-attn --no-build-isolation
```

```python
# Flash Attention 2 is automatic in PyTorch ≥ 2.2 via SDPA
import torch
# This automatically uses FA2 on L40S (Ada)
out = torch.nn.functional.scaled_dot_product_attention(q, k, v, is_causal=True)

# Memory savings: O(N) vs O(N²) for attention map
# Speed: 2-4× faster than naive attention for seq_len > 1024
```

## 8. Triton Inference Server 配置

面向 12 块 L40S GPU 的生产级多模型部署：

```bash
# Model repository structure
model_repo/
├── llama-3-8b/
│   ├── config.pbtxt
│   └── 1/
│       └── model.py
├── llama-3-70b-awq/
│   ├── config.pbtxt
│   └── 1/
│       └── model.py
└── ensemble/
    └── config.pbtxt

# config.pbtxt for vLLM backend on Triton
name: "llama-3-8b"
backend: "vllm"
max_batch_size: 256

model_transaction_policy {
  decoupled: true  # streaming
}

instance_group [
  {
    count: 1
    kind: KIND_GPU
    gpus: [0]  # assign to GPU 0
  }
]

parameters {
  key: "model"
  value: { string_value: "meta-llama/Llama-3-8b-instruct" }
}
parameters {
  key: "dtype"
  value: { string_value: "bfloat16" }
}
```

```bash
# Launch Triton with 12 L40S GPUs
tritonserver \
    --model-repository=/model_repo \
    --backend-config=vllm,cmdline_args="--max-num-seqs 256" \
    --http-port 8000 \
    --grpc-port 8001 \
    --log-verbose 1
```


<details>
<summary>English original</summary>

**vLLM Tuning for PCIe Systems**

```bash
# L40S-specific vLLM launch flags
python -m vllm.entrypoints.openai.api_server \
    --model meta-llama/Llama-3-8b-instruct \
    --dtype bfloat16 \
    --max-model-len 8192 \
    --gpu-memory-utilization 0.90 \
    --max-num-seqs 256 \
    --max-num-batched-tokens 32768 \
    --block-size 16 \
    --port 8000

# For GPTQ/AWQ quantized models
python -m vllm.entrypoints.openai.api_server \
    --model /models/llama-3-70b-awq \
    --quantization awq \
    --max-model-len 4096 \
    --gpu-memory-utilization 0.85 \
    --max-num-seqs 128
```

**4. Continuous Batching Strategies**

**L40S Optimal Batch Size**

Unlike H200 where large batch sizes are preferable, L40S has tighter memory constraints:

```
7B model on L40S (48 GB total):
  Weights (BF16): 14 GB
  CUDA reserved:  ~2 GB
  Available KV:   ~32 GB

KV cache per token (Llama-3 8B, FP16):
  2 × 32 layers × 8 kv-heads × 128 head-dim × 2 bytes = 131 KB/token

Max concurrent tokens at BS=256, seq=256:
  256 × 256 = 65,536 tokens × 131 KB = ~8.6 GB → fits ✓

Sweet spot for L40S 7B: batch_size=128-256
```

```python
# Benchmark batch sizes to find throughput peak
import torch, time
from transformers import AutoModelForCausalLM, AutoTokenizer

model = AutoModelForCausalLM.from_pretrained(
    "meta-llama/Llama-3-8b",
    torch_dtype=torch.bfloat16,
    device_map="cuda:0",
)
model.eval()

for batch_size in [1, 4, 16, 32, 64, 128]:
    input_ids = torch.randint(0, 32000, (batch_size, 128), device="cuda:0")

    with torch.no_grad(), torch.autocast("cuda", dtype=torch.bfloat16):
        for _ in range(3): model(input_ids)  # warmup
        t0 = time.perf_counter()
        for _ in range(20): model(input_ids)
        torch.cuda.synchronize()
        elapsed = time.perf_counter() - t0

    tps = batch_size * 128 * 20 / elapsed
    print(f"BS={batch_size:4d}: {tps:8.0f} tokens/s")
```

**5. Speculative Decoding**

Speculative decoding is especially effective on L40S because the decode step is highly memory-bandwidth bound:

```python
# L40S speculative decoding setup
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",   # target model (2 GPUs, BF16)
    speculative_model="meta-llama/Llama-3-8b-instruct",  # draft on 1 GPU
    num_speculative_tokens=5,
    tensor_parallel_size=2,
)
```

Alternative: use a tiny draft model (< 1B) for even larger speedups:

```python
llm = LLM(
    model="meta-llama/Llama-3-8b-instruct",
    speculative_model="TinyLlama/TinyLlama-1.1B-Chat-v1.0",
    num_speculative_tokens=6,
    speculative_max_model_len=4096,
)
# Typical speedup: 1.5-2.5x on L40S (memory-bound decode benefits most)
```

**6. KV Cache Quantization**

On L40S (48 GB), KV cache compression is critical for long contexts:

```python
# vLLM with FP8 KV cache
llm = LLM(
    model="meta-llama/Llama-3-8b-instruct",
    kv_cache_dtype="fp8",       # cuts KV cache memory by 50%
    max_model_len=32768,        # now supports 32K context on single L40S
)

# FP8 KV cache impact on L40S 7B:
# FP16 KV: 131 KB/token  → 32K context needs 4.2 GB (max ~240 batch sequences at 128 tokens)
# FP8 KV:  66 KB/token   → 32K context needs 2.1 GB (nearly 2× more sequences)
```

**7. Flash Attention 2 (L40S)**

L40S supports Flash Attention 2 (not FA3, which is Hopper-specific):

```bash
pip install flash-attn --no-build-isolation
```

```python
# Flash Attention 2 is automatic in PyTorch ≥ 2.2 via SDPA
import torch
# This automatically uses FA2 on L40S (Ada)
out = torch.nn.functional.scaled_dot_product_attention(q, k, v, is_causal=True)

# Memory savings: O(N) vs O(N²) for attention map
# Speed: 2-4× faster than naive attention for seq_len > 1024
```

**8. Triton Inference Server Setup**

For production multi-model deployment across 12 L40S GPUs:

```bash
# Model repository structure
model_repo/
├── llama-3-8b/
│   ├── config.pbtxt
│   └── 1/
│       └── model.py
├── llama-3-70b-awq/
│   ├── config.pbtxt
│   └── 1/
│       └── model.py
└── ensemble/
    └── config.pbtxt

# config.pbtxt for vLLM backend on Triton
name: "llama-3-8b"
backend: "vllm"
max_batch_size: 256

model_transaction_policy {
  decoupled: true  # streaming
}

instance_group [
  {
    count: 1
    kind: KIND_GPU
    gpus: [0]  # assign to GPU 0
  }
]

parameters {
  key: "model"
  value: { string_value: "meta-llama/Llama-3-8b-instruct" }
}
parameters {
  key: "dtype"
  value: { string_value: "bfloat16" }
}
```

```bash
# Launch Triton with 12 L40S GPUs
tritonserver \
    --model-repository=/model_repo \
    --backend-config=vllm,cmdline_args="--max-num-seqs 256" \
    --http-port 8000 \
    --grpc-port 8001 \
    --log-verbose 1
```

</details>

## 参考文献

- [AutoGPTQ](https://github.com/PanQiWei/AutoGPTQ)
- [AutoAWQ](https://github.com/casper-hansen/AutoAWQ)
- [vLLM 量化指南](https://docs.vllm.ai/en/latest/quantization/auto_awq.html)
- [FlashAttention-2 论文](https://arxiv.org/abs/2307.08691)
- [Triton Inference Server + vLLM](https://github.com/triton-inference-server/vllm_backend)
- [投机解码综述](https://arxiv.org/abs/2401.07851)


<details>
<summary>English original</summary>

**References**

- [AutoGPTQ](https://github.com/PanQiWei/AutoGPTQ)
- [AutoAWQ](https://github.com/casper-hansen/AutoAWQ)
- [vLLM Quantization Guide](https://docs.vllm.ai/en/latest/quantization/auto_awq.html)
- [FlashAttention-2 Paper](https://arxiv.org/abs/2307.08691)
- [Triton Inference Server + vLLM](https://github.com/triton-inference-server/vllm_backend)
- [Speculative Decoding Survey](https://arxiv.org/abs/2401.07851)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/L40S-x12-Inference/02-Inference-Optimization.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/L40S-x12-Inference/02-Inference-Optimization.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
