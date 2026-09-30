---
title: 03 — 8x H200 推理环境搭建
description: 03 — 8x H200 推理环境搭建
---

# 03 — 8x H200 推理环境搭建

## 1. 8x H200 上的推理规模规划

单节点 HBM3e 总容量 1,128 GB，可服务超大模型：

| 模型 | 权重 (FP16) | KV cache (128K ctx, BS=32) | 能否放入 8 张 GPU？ |
|---|---|---|---|
| Llama-3 8B | 16 GB | ~32 GB | 是（1 张 GPU） |
| Llama-3 70B | 140 GB | ~256 GB | 是（4–8 张 GPU） |
| Llama-3 405B | 810 GB | ~512 GB | 是（8 张 GPU，吃紧） |
| Mixtral 8×22B | 281 GB | ~196 GB | 是（4 张 GPU） |

> **KV cache 估算：** 2 × layers × heads × head_dim × seq_len × batch × 2 bytes (FP16)

## 2. 推理引擎概览

| 引擎 | 最佳使用场景 | 关键特性 |
|---|---|---|
| **vLLM** | 在线推理服务，高并发 | PagedAttention，连续批处理 |
| **TensorRT-LLM** | 最大吞吐，NVIDIA 优化 | INT4/FP8 kernel，连续批处理 |
| **SGLang** | 多轮对话，结构化生成 | RadixAttention 实现 KV cache 共享 |
| **Triton Inference Server** | 生产部署，ensemble | gRPC/HTTP，动态批处理 |

## 3. vLLM 部署

vLLM 是 H200 推理最常用的开源引擎。

### 安装

```bash
pip install vllm  # requires CUDA 12.x, PyTorch 2.x
```

### 单节点 8 GPU 推理服务（张量并行）

```python
from vllm import LLM, SamplingParams

# TP=8: split the model across all 8 H200s
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",
    tensor_parallel_size=8,
    dtype="bfloat16",           # or "float16" or "auto"
    max_model_len=131072,       # 128K context
    gpu_memory_utilization=0.90,  # leave 10% headroom
    enforce_eager=False,        # use CUDA graphs (faster)
    max_num_seqs=256,           # max concurrent sequences
)

params = SamplingParams(temperature=0.7, top_p=0.9, max_tokens=512)
outputs = llm.generate(["Explain NVLink in one paragraph:"], params)
print(outputs[0].outputs[0].text)
```

### OpenAI 兼容 API 服务

```bash
python -m vllm.entrypoints.openai.api_server \
    --model meta-llama/Llama-3-70b-instruct \
    --tensor-parallel-size 8 \
    --max-model-len 131072 \
    --gpu-memory-utilization 0.90 \
    --max-num-seqs 512 \
    --dtype bfloat16 \
    --host 0.0.0.0 \
    --port 8000
```

```bash
# Test endpoint
curl http://localhost:8000/v1/completions \
  -H "Content-Type: application/json" \
  -d '{"model": "meta-llama/Llama-3-70b-instruct", "prompt": "Hello", "max_tokens": 50}'
```

### vLLM PagedAttention 内部机制

```
Traditional KV cache:            PagedAttention:
[seq1: 2048 tokens reserved]     [Block 0: 16 tokens][Block 1: 16 tokens]...
[seq2: 2048 tokens reserved]     [Block 5: 16 tokens][Block 8: 16 tokens]...
[seq3: 2048 tokens reserved]     Blocks shared via reference counting

→ Memory waste: 30-50%           → Near-zero fragmentation
                                 → 2-4× more sequences in memory
```

## 4. 用 TensorRT-LLM 实现最大性能

TensorRT-LLM 将模型编译为带定制 Hopper kernel 的优化引擎。

### 构建与运行

```bash
# Build TRT-LLM engine for Llama-3 70B, FP8, TP=8
python examples/llama/convert_checkpoint.py \
    --model_dir /models/llama-3-70b \
    --output_dir /engines/llama-3-70b-fp8 \
    --dtype bfloat16 \
    --use_fp8_rowwise \      # FP8 quantization for Hopper
    --tp_size 8

trtllm-build \
    --checkpoint_dir /engines/llama-3-70b-fp8 \
    --output_dir /engines/llama-3-70b-trt \
    --gemm_plugin bfloat16 \
    --use_paged_context_fmha enable \
    --max_batch_size 256 \
    --max_input_len 8192 \
    --max_output_len 2048 \
    --workers 8
```

### TRT-LLM 的连续批处理

```python
from tensorrt_llm.runtime import ModelRunner

runner = ModelRunner.from_dir(
    engine_dir="/engines/llama-3-70b-trt",
    rank=0,
    max_output_len=2048,
)

# Inflight batching: new requests join mid-flight without waiting
results = runner.generate(
    batch_input_ids=input_ids,
    streaming=True,
    max_new_tokens=512,
)
```

## 5. FP8 量化（H200 专属）

H200 Tensor Core 原生计算 FP8（E4M3 与 E5M2），吞吐约为 BF16 的 2×。

```python
# Using Transformer Engine FP8 for inference
import transformer_engine.pytorch as te
from transformer_engine.common.recipe import Format, DelayedScaling

fp8_recipe = DelayedScaling(
    margin=0,
    interval=1,
    fp8_format=Format.HYBRID,     # E4M3 for forward, E5M2 for backward
    amax_history_len=16,
    amax_compute_algo="max",
)

with te.fp8_autocast(enabled=True, fp8_recipe=fp8_recipe):
    output = model(input_ids)
```

### H200 上的 FP8 吞吐增益

| 精度 | 吞吐（tokens/s，Llama-70B，BS=64） |
|---|---|
| BF16 | ~12,000 |
| FP8 (TRT-LLM) | ~22,000 |
| INT4 AWQ | ~28,000（精度取舍） |


<details>
<summary>English original</summary>

**03 — Inference Setup on 8x H200**

**1. Inference Sizing on 8x H200**

With 1,128 GB total HBM3e, a single node can serve very large models:

| Model | Weights (FP16) | KV Cache (128K ctx, BS=32) | Fits in 8 GPUs? |
|---|---|---|---|
| Llama-3 8B | 16 GB | ~32 GB | Yes (1 GPU) |
| Llama-3 70B | 140 GB | ~256 GB | Yes (4–8 GPUs) |
| Llama-3 405B | 810 GB | ~512 GB | Yes (8 GPUs, tight) |
| Mixtral 8×22B | 281 GB | ~196 GB | Yes (4 GPUs) |

> **KV cache estimate:** 2 × layers × heads × head_dim × seq_len × batch × 2 bytes (FP16)

**2. Inference Engines Overview**

| Engine | Best Use Case | Key Feature |
|---|---|---|
| **vLLM** | Online serving, high concurrency | PagedAttention, continuous batching |
| **TensorRT-LLM** | Maximum throughput, NVIDIA optimized | INT4/FP8 kernels, inflight batching |
| **SGLang** | Multi-turn, structured generation | RadixAttention for KV cache sharing |
| **Triton Inference Server** | Production deployment, ensemble | gRPC/HTTP, dynamic batching |

**3. vLLM Deployment**

vLLM is the most common open-source engine for H200 inference.

**Installation**

```bash
pip install vllm  # requires CUDA 12.x, PyTorch 2.x
```

**Single-Node 8-GPU Serving (Tensor Parallel)**

```python
from vllm import LLM, SamplingParams

# TP=8: split the model across all 8 H200s
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",
    tensor_parallel_size=8,
    dtype="bfloat16",           # or "float16" or "auto"
    max_model_len=131072,       # 128K context
    gpu_memory_utilization=0.90,  # leave 10% headroom
    enforce_eager=False,        # use CUDA graphs (faster)
    max_num_seqs=256,           # max concurrent sequences
)

params = SamplingParams(temperature=0.7, top_p=0.9, max_tokens=512)
outputs = llm.generate(["Explain NVLink in one paragraph:"], params)
print(outputs[0].outputs[0].text)
```

**OpenAI-Compatible API Server**

```bash
python -m vllm.entrypoints.openai.api_server \
    --model meta-llama/Llama-3-70b-instruct \
    --tensor-parallel-size 8 \
    --max-model-len 131072 \
    --gpu-memory-utilization 0.90 \
    --max-num-seqs 512 \
    --dtype bfloat16 \
    --host 0.0.0.0 \
    --port 8000
```

```bash
# Test endpoint
curl http://localhost:8000/v1/completions \
  -H "Content-Type: application/json" \
  -d '{"model": "meta-llama/Llama-3-70b-instruct", "prompt": "Hello", "max_tokens": 50}'
```

**vLLM PagedAttention Internals**

```
Traditional KV cache:            PagedAttention:
[seq1: 2048 tokens reserved]     [Block 0: 16 tokens][Block 1: 16 tokens]...
[seq2: 2048 tokens reserved]     [Block 5: 16 tokens][Block 8: 16 tokens]...
[seq3: 2048 tokens reserved]     Blocks shared via reference counting

→ Memory waste: 30-50%           → Near-zero fragmentation
                                 → 2-4× more sequences in memory
```

**4. TensorRT-LLM for Maximum Performance**

TensorRT-LLM compiles models into optimized engines with custom Hopper kernels.

**Build and Run**

```bash
# Build TRT-LLM engine for Llama-3 70B, FP8, TP=8
python examples/llama/convert_checkpoint.py \
    --model_dir /models/llama-3-70b \
    --output_dir /engines/llama-3-70b-fp8 \
    --dtype bfloat16 \
    --use_fp8_rowwise \      # FP8 quantization for Hopper
    --tp_size 8

trtllm-build \
    --checkpoint_dir /engines/llama-3-70b-fp8 \
    --output_dir /engines/llama-3-70b-trt \
    --gemm_plugin bfloat16 \
    --use_paged_context_fmha enable \
    --max_batch_size 256 \
    --max_input_len 8192 \
    --max_output_len 2048 \
    --workers 8
```

**Inflight Batching with TRT-LLM**

```python
from tensorrt_llm.runtime import ModelRunner

runner = ModelRunner.from_dir(
    engine_dir="/engines/llama-3-70b-trt",
    rank=0,
    max_output_len=2048,
)

# Inflight batching: new requests join mid-flight without waiting
results = runner.generate(
    batch_input_ids=input_ids,
    streaming=True,
    max_new_tokens=512,
)
```

**5. FP8 Quantization (H200 Specific)**

H200 Tensor Cores natively compute FP8 (E4M3 and E5M2), offering ~2× throughput over BF16.

```python
# Using Transformer Engine FP8 for inference
import transformer_engine.pytorch as te
from transformer_engine.common.recipe import Format, DelayedScaling

fp8_recipe = DelayedScaling(
    margin=0,
    interval=1,
    fp8_format=Format.HYBRID,     # E4M3 for forward, E5M2 for backward
    amax_history_len=16,
    amax_compute_algo="max",
)

with te.fp8_autocast(enabled=True, fp8_recipe=fp8_recipe):
    output = model(input_ids)
```

**FP8 Throughput Gains on H200**

| Precision | Throughput (tokens/s, Llama-70B, BS=64) |
|---|---|
| BF16 | ~12,000 |
| FP8 (TRT-LLM) | ~22,000 |
| INT4 AWQ | ~28,000 (quality trade-off) |

</details>

## 6. 连续批处理与调度

```
Without continuous batching:
  Request A: [==== 512 tokens ====] → batch finishes → accept B
  Request B:                          [==== 200 tokens ====]
  GPU sits idle between batches

With continuous batching:
  Iteration 1: [A token 1][B token 1][C token 1]
  Iteration 2: [A token 2][B token 2][C token 2]
  ...when A finishes: [D token 1] immediately joins
  GPU is always processing maximum sequences
```

vLLM、TRT-LLM 和 SGLang 都实现了这一机制。吞吐提升：**相较静态批处理提升 2–8×**。

## 7. 投机解码

投机解码用一个小的“draft”模型提出 token，再由大模型并行验证：

```python
# vLLM speculative decoding
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",
    speculative_model="meta-llama/Llama-3-8b-instruct",  # draft model
    num_speculative_tokens=5,    # predict 5 tokens per draft step
    tensor_parallel_size=8,
)
```

典型加速：在贪心/低温采样（代码、结构化输出）下为 **1.5–2.5×**。

## 8. KV cache 量化

对长上下文工作负载，KV cache 是显存瓶颈：

```python
# vLLM FP8 KV cache (H200 compatible)
llm = LLM(
    model="...",
    kv_cache_dtype="fp8",        # quantize KV cache to FP8
    tensor_parallel_size=8,
    max_model_len=131072,        # now fits ~4× more tokens
)
```

KV cache 量化带来的显存节省：
- FP16 KV：100% 显存
- FP8 KV：~50% 显存 → 可并发的长上下文请求数提升 2×

## 9. 推理 benchmark

```bash
# vLLM benchmark
python benchmarks/benchmark_throughput.py \
    --model meta-llama/Llama-3-70b-instruct \
    --tensor-parallel-size 8 \
    --num-prompts 1000 \
    --input-len 512 \
    --output-len 256 \
    --dtype bfloat16

# Key metrics to track:
# - Throughput: tokens/s (output tokens)
# - TTFT: Time To First Token (prefill latency)
# - TPOT: Time Per Output Token (decode latency)
# - ITL: Inter-Token Latency
```

### H200 预期基线（Llama-3 70B，TP=8）

| 指标 | 目标 |
|---|---|
| TTFT（512 输入 token） | < 100 ms |
| TPOT（decode，逐 token 生成阶段） | < 20 ms/token |
| 吞吐（BS=64） | > 15,000 tokens/s |

## 参考文献

- [vLLM Documentation](https://docs.vllm.ai/)
- [TensorRT-LLM Repository](https://github.com/NVIDIA/TensorRT-LLM)
- [SGLang Repository](https://github.com/sgl-project/sglang)
- [PagedAttention Paper](https://arxiv.org/abs/2309.06180)
- [Speculative Decoding Paper](https://arxiv.org/abs/2211.17192)
- [FP8 Formats for Deep Learning](https://arxiv.org/abs/2209.05433)


<details>
<summary>English original</summary>

**6. Continuous Batching and Scheduling**

```
Without continuous batching:
  Request A: [==== 512 tokens ====] → batch finishes → accept B
  Request B:                          [==== 200 tokens ====]
  GPU sits idle between batches

With continuous batching:
  Iteration 1: [A token 1][B token 1][C token 1]
  Iteration 2: [A token 2][B token 2][C token 2]
  ...when A finishes: [D token 1] immediately joins
  GPU is always processing maximum sequences
```

vLLM, TRT-LLM, and SGLang all implement this. Throughput improvement: **2–8× over static batching**.

**7. Speculative Decoding**

Speculative decoding uses a small "draft" model to propose tokens, then verifies them with the large model in parallel:

```python
# vLLM speculative decoding
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",
    speculative_model="meta-llama/Llama-3-8b-instruct",  # draft model
    num_speculative_tokens=5,    # predict 5 tokens per draft step
    tensor_parallel_size=8,
)
```

Typical speedup: **1.5–2.5×** for greedy/low-temperature sampling (code, structured output).

**8. KV Cache Quantization**

For long-context workloads, KV cache is the memory bottleneck:

```python
# vLLM FP8 KV cache (H200 compatible)
llm = LLM(
    model="...",
    kv_cache_dtype="fp8",        # quantize KV cache to FP8
    tensor_parallel_size=8,
    max_model_len=131072,        # now fits ~4× more tokens
)
```

Memory savings with KV cache quant:
- FP16 KV: 100% memory
- FP8 KV: ~50% memory → 2× more concurrent long-context requests

**9. Benchmarking Inference**

```bash
# vLLM benchmark
python benchmarks/benchmark_throughput.py \
    --model meta-llama/Llama-3-70b-instruct \
    --tensor-parallel-size 8 \
    --num-prompts 1000 \
    --input-len 512 \
    --output-len 256 \
    --dtype bfloat16

# Key metrics to track:
# - Throughput: tokens/s (output tokens)
# - TTFT: Time To First Token (prefill latency)
# - TPOT: Time Per Output Token (decode latency)
# - ITL: Inter-Token Latency
```

**H200 Expected Baselines (Llama-3 70B, TP=8)**

| Metric | Target |
|---|---|
| TTFT (512 input tokens) | < 100 ms |
| TPOT (decode) | < 20 ms/token |
| Throughput (BS=64) | > 15,000 tokens/s |

**References**

- [vLLM Documentation](https://docs.vllm.ai/)
- [TensorRT-LLM Repository](https://github.com/NVIDIA/TensorRT-LLM)
- [SGLang Repository](https://github.com/sgl-project/sglang)
- [PagedAttention Paper](https://arxiv.org/abs/2309.06180)
- [Speculative Decoding Paper](https://arxiv.org/abs/2211.17192)
- [FP8 Formats for Deep Learning](https://arxiv.org/abs/2209.05433)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/8x-H200-Training-Inference/03-Inference-Setup.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/8x-H200-Training-Inference/03-Inference-Setup.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
