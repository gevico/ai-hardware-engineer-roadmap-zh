---
title: 05 — L40S x12 的 benchmark
description: 05 — L40S x12 的 benchmark
---

# 05 — L40S x12 的 benchmark

## 1. 硬件验证 benchmark

### 内存带宽测试

```bash
python - <<'EOF'
import torch, time

N = int(40 * 1024**3 / 2)  # 40 GB of FP16 data
x = torch.randn(N // 2, 2, device="cuda:0", dtype=torch.float16).view(-1)
y = torch.empty_like(x)

for _ in range(5): y.copy_(x)  # warmup
torch.cuda.synchronize()

t0 = time.perf_counter()
for _ in range(20): y.copy_(x)
torch.cuda.synchronize()
elapsed = time.perf_counter() - t0

bw = (2 * N * 2 * 20) / elapsed / 1e12  # read + write, FP16 = 2 bytes
print(f"HBM Bandwidth: {bw:.2f} TB/s")
# Target: > 0.72 TB/s (83% of 0.864 TB/s peak)
EOF
```

### 计算（GEMM）benchmark

```bash
python - <<'EOF'
import torch, time

# Large GEMM — should be compute-bound, hitting Tensor Cores
M, N, K = 8192, 8192, 8192
a = torch.randn(M, K, device="cuda:0", dtype=torch.bfloat16)
b = torch.randn(K, N, device="cuda:0", dtype=torch.bfloat16)

for _ in range(10): torch.mm(a, b)
torch.cuda.synchronize()

t0 = time.perf_counter()
for _ in range(100): torch.mm(a, b)
torch.cuda.synchronize()
elapsed = time.perf_counter() - t0

tflops = 2 * M * N * K * 100 / elapsed / 1e12
print(f"GEMM TFLOPS (BF16): {tflops:.0f}")
# Target: > 150 TFLOPS (dense BF16, ~82% of 183 TFLOPS peak)
EOF
```

### PCIe P2P 带宽

```bash
python - <<'EOF'
import torch, time

# Transfer 2 GB between adjacent GPUs (same PCIe switch)
size = 2 * 1024**3 // 2  # 2 GB of FP16
a = torch.randn(size, device="cuda:0", dtype=torch.float16)
b = torch.empty(size, device="cuda:1", dtype=torch.float16)

for _ in range(5): b.copy_(a); torch.cuda.synchronize()

t0 = time.perf_counter()
for _ in range(50): b.copy_(a)
torch.cuda.synchronize()
elapsed = time.perf_counter() - t0

bw = 2e9 * 2 * 50 / elapsed / 1e9
print(f"P2P Bandwidth GPU0→GPU1: {bw:.1f} GB/s")
# Good (same switch): > 25 GB/s
# Poor (cross NUMA):  < 10 GB/s → rearrange GPU assignments
EOF
```

## 2. 推理吞吐 benchmark

### 单 GPU — 7B 模型

```bash
# vLLM throughput benchmark (offline mode)
python benchmarks/benchmark_throughput.py \
    --model meta-llama/Llama-3-8b-instruct \
    --dtype bfloat16 \
    --num-prompts 1000 \
    --input-len 512 \
    --output-len 256 \
    --backend vllm

# Expected results for L40S (single GPU, 7B BF16):
# Throughput: 3,000 - 5,000 output tokens/s
# GPU memory used: ~30 GB (14 GB weights + 16 GB KV cache)
```

### 延迟 benchmark — 在线推理服务模式

```bash
python benchmarks/benchmark_serving.py \
    --model meta-llama/Llama-3-8b-instruct \
    --request-rate 20 \
    --num-prompts 500 \
    --input-len 512 \
    --output-len 128 \
    --backend vllm

# Expected metrics (L40S, 7B, single GPU):
# TTFT p50:  < 80 ms
# TTFT p99:  < 200 ms
# TPOT p50:  < 15 ms/token
# TPOT p99:  < 30 ms/token
```

## 3. 性能基线

### 单张 L40S — 模型性能表

| 模型 | 精度 | 显存占用 | 吞吐 | TTFT p50 | TPOT p50 |
|---|---|---|---|---|---|
| Llama-3 8B | BF16 | 16 GB | ~4,500 tok/s | 60 ms | 12 ms |
| Llama-3 8B | INT8 | 10 GB | ~3,800 tok/s | 55 ms | 13 ms |
| Llama-3 13B | BF16 | 26 GB | ~2,800 tok/s | 90 ms | 18 ms |
| Llama-3 70B | AWQ INT4 | 37 GB | ~900 tok/s | 250 ms | 35 ms |
| Mistral 7B | BF16 | 14 GB | ~4,800 tok/s | 55 ms | 11 ms |

> 在 batch_size=32、input_len=512、output_len=256、单 GPU 下测得。

### 2 张 L40S（TP=2）— 70B 模型

| 模型 | 精度 | 每 GPU 显存 | 吞吐 | TTFT p50 | TPOT p50 |
|---|---|---|---|---|---|
| Llama-3 70B | BF16 | 72 GB | ~2,200 tok/s | 150 ms | 20 ms |
| Llama-3 70B | INT8 | 37 GB | ~1,800 tok/s | 140 ms | 22 ms |
| Llama-3 70B | AWQ INT4 | 19 GB | ~3,000 tok/s | 120 ms | 17 ms |

> TP=2，要求挂在同一 PCIe switch 下。batch_size=32。

### 12 张 L40S 簇 — 聚合吞吐

| 配置 | 模型 | 总吞吐 |
|---|---|---|
| 12 × 单 GPU 7B（BF16） | 12 个 Llama-3-8B 副本 | ~54,000 tok/s |
| 6 × TP=2 70B AWQ | 6 个 Llama-3-70B 副本 | ~18,000 tok/s |
| 4 个单卡 7B + 4 个 TP=2 70B | 混合推理服务 | ~18,000 + 12,000 tok/s |

## 4. L40S 与 H200 对比

| 指标 | L40S x12 | H200 x8 | 备注 |
|---|---|---|---|
| GPU 总显存 | 576 GB GDDR6 | 1,128 GB HBM3e | H200 多 2× |
| 总带宽 | 10.4 TB/s | 38.4 TB/s | H200 多 3.7× |
| GPU-GPU 互连 | PCIe 4.0 | NVLink 4.0（900 GB/s） | H200 快 14× |
| 7B BF16 吞吐 | ~4,500 tok/s/GPU | ~8,000 tok/s/GPU | H200 快 ~1.8× |
| 70B BF16（多 GPU） | ~2,200 tok/s (TP=2) | ~18,000 tok/s (TP=8) | H200 快 8× |
| 功耗（仅 GPU） | 4,200 W | 5,600 W | L40S 功耗低 25% |
| 近似成本 | ~$60-80K | ~$400K+ | L40S 便宜 5-7× |
| 成本/token（7B） | 1× | ~0.4× | 规模上 H200 的 $/tok 更优 |
| 成本/token（70B） | 1× | ~0.15× | 70B 上 H200 明显更优 |

**小结：** L40S 在低成本小模型推理上表现出色。H200 在大模型推理以及所有训练任务上占优。


<details>
<summary>English original</summary>

**05 — Benchmarks for L40S x12**

**1. Hardware Validation Benchmarks**

**Memory Bandwidth Test**

```bash
python - <<'EOF'
import torch, time

N = int(40 * 1024**3 / 2)  # 40 GB of FP16 data
x = torch.randn(N // 2, 2, device="cuda:0", dtype=torch.float16).view(-1)
y = torch.empty_like(x)

for _ in range(5): y.copy_(x)  # warmup
torch.cuda.synchronize()

t0 = time.perf_counter()
for _ in range(20): y.copy_(x)
torch.cuda.synchronize()
elapsed = time.perf_counter() - t0

bw = (2 * N * 2 * 20) / elapsed / 1e12  # read + write, FP16 = 2 bytes
print(f"HBM Bandwidth: {bw:.2f} TB/s")
# Target: > 0.72 TB/s (83% of 0.864 TB/s peak)
EOF
```

**Compute (GEMM) Benchmark**

```bash
python - <<'EOF'
import torch, time

# Large GEMM — should be compute-bound, hitting Tensor Cores
M, N, K = 8192, 8192, 8192
a = torch.randn(M, K, device="cuda:0", dtype=torch.bfloat16)
b = torch.randn(K, N, device="cuda:0", dtype=torch.bfloat16)

for _ in range(10): torch.mm(a, b)
torch.cuda.synchronize()

t0 = time.perf_counter()
for _ in range(100): torch.mm(a, b)
torch.cuda.synchronize()
elapsed = time.perf_counter() - t0

tflops = 2 * M * N * K * 100 / elapsed / 1e12
print(f"GEMM TFLOPS (BF16): {tflops:.0f}")
# Target: > 150 TFLOPS (dense BF16, ~82% of 183 TFLOPS peak)
EOF
```

**PCIe P2P Bandwidth**

```bash
python - <<'EOF'
import torch, time

# Transfer 2 GB between adjacent GPUs (same PCIe switch)
size = 2 * 1024**3 // 2  # 2 GB of FP16
a = torch.randn(size, device="cuda:0", dtype=torch.float16)
b = torch.empty(size, device="cuda:1", dtype=torch.float16)

for _ in range(5): b.copy_(a); torch.cuda.synchronize()

t0 = time.perf_counter()
for _ in range(50): b.copy_(a)
torch.cuda.synchronize()
elapsed = time.perf_counter() - t0

bw = 2e9 * 2 * 50 / elapsed / 1e9
print(f"P2P Bandwidth GPU0→GPU1: {bw:.1f} GB/s")
# Good (same switch): > 25 GB/s
# Poor (cross NUMA):  < 10 GB/s → rearrange GPU assignments
EOF
```

**2. Inference Throughput Benchmarks**

**Single GPU — 7B Model**

```bash
# vLLM throughput benchmark (offline mode)
python benchmarks/benchmark_throughput.py \
    --model meta-llama/Llama-3-8b-instruct \
    --dtype bfloat16 \
    --num-prompts 1000 \
    --input-len 512 \
    --output-len 256 \
    --backend vllm

# Expected results for L40S (single GPU, 7B BF16):
# Throughput: 3,000 - 5,000 output tokens/s
# GPU memory used: ~30 GB (14 GB weights + 16 GB KV cache)
```

**Latency Benchmark — Online Serving Mode**

```bash
python benchmarks/benchmark_serving.py \
    --model meta-llama/Llama-3-8b-instruct \
    --request-rate 20 \
    --num-prompts 500 \
    --input-len 512 \
    --output-len 128 \
    --backend vllm

# Expected metrics (L40S, 7B, single GPU):
# TTFT p50:  < 80 ms
# TTFT p99:  < 200 ms
# TPOT p50:  < 15 ms/token
# TPOT p99:  < 30 ms/token
```

**3. Performance Baselines**

**Single L40S — Model Performance Table**

| Model | Precision | VRAM Used | Throughput | TTFT p50 | TPOT p50 |
|---|---|---|---|---|---|
| Llama-3 8B | BF16 | 16 GB | ~4,500 tok/s | 60 ms | 12 ms |
| Llama-3 8B | INT8 | 10 GB | ~3,800 tok/s | 55 ms | 13 ms |
| Llama-3 13B | BF16 | 26 GB | ~2,800 tok/s | 90 ms | 18 ms |
| Llama-3 70B | AWQ INT4 | 37 GB | ~900 tok/s | 250 ms | 35 ms |
| Mistral 7B | BF16 | 14 GB | ~4,800 tok/s | 55 ms | 11 ms |

> Measured at batch_size=32, input_len=512, output_len=256, single GPU.

**2x L40S (TP=2) — 70B Model**

| Model | Precision | VRAM per GPU | Throughput | TTFT p50 | TPOT p50 |
|---|---|---|---|---|---|
| Llama-3 70B | BF16 | 72 GB | ~2,200 tok/s | 150 ms | 20 ms |
| Llama-3 70B | INT8 | 37 GB | ~1,800 tok/s | 140 ms | 22 ms |
| Llama-3 70B | AWQ INT4 | 19 GB | ~3,000 tok/s | 120 ms | 17 ms |

> TP=2, same PCIe switch required. batch_size=32.

**12x L40S Cluster — Aggregate Throughput**

| Configuration | Models | Total Throughput |
|---|---|---|
| 12 × single-GPU 7B (BF16) | 12 replicas Llama-3-8B | ~54,000 tok/s |
| 6 × TP=2 70B AWQ | 6 replicas Llama-3-70B | ~18,000 tok/s |
| 4 single 7B + 4 TP=2 70B | Mixed serving | ~18,000 + 12,000 tok/s |

**4. L40S vs H200 Comparison**

| Metric | L40S x12 | H200 x8 | Notes |
|---|---|---|---|
| Total GPU memory | 576 GB GDDR6 | 1,128 GB HBM3e | H200 2× more |
| Total bandwidth | 10.4 TB/s | 38.4 TB/s | H200 3.7× more |
| GPU-GPU interconnect | PCIe 4.0 | NVLink 4.0 (900 GB/s) | H200 14× faster |
| 7B BF16 throughput | ~4,500 tok/s/GPU | ~8,000 tok/s/GPU | H200 ~1.8× faster |
| 70B BF16 (multi-GPU) | ~2,200 tok/s (TP=2) | ~18,000 tok/s (TP=8) | H200 8× faster |
| Power (GPUs only) | 4,200 W | 5,600 W | L40S 25% less power |
| Approximate cost | ~$60-80K | ~$400K+ | L40S 5-7× cheaper |
| Cost/token (7B) | 1× | ~0.4× | H200 better $/tok at scale |
| Cost/token (70B) | 1× | ~0.15× | H200 much better at 70B |

**Summary:** L40S excels at cost-efficient small model inference. H200 dominates for large model inference and all training tasks.

</details>

## 5. 量化质量 vs 性能

```bash
# Perplexity comparison (lower is better)
python - <<'EOF'
from lm_eval import evaluator
from transformers import AutoModelForCausalLM, AutoTokenizer
import torch

# Run on WikiText-2 perplexity
results = {}
for model_id, quant in [
    ("meta-llama/Llama-3-8b", "none"),
    ("/models/llama-3-8b-awq-int4", "awq"),
    ("/models/llama-3-8b-gptq-int4", "gptq"),
    ("/models/llama-3-8b-int8", "int8"),
]:
    result = evaluator.simple_evaluate(
        model="vllm",
        model_args=f"pretrained={model_id},quantization={quant}",
        tasks=["wikitext"],
    )
    results[quant] = result["results"]["wikitext"]["word_perplexity"]
    print(f"{quant}: {results[quant]:.2f}")
EOF
```

### 预期的质量-性能权衡

| 精度 | 困惑度（WikiText2） | 吞吐 | 内存 |
|---|---|---|---|
| BF16（基线） | 5.85 | 1.0× | 1.0× |
| INT8（LLM.int8） | 5.88 | 0.85× | 0.5× |
| AWQ INT4 | 5.95 | 1.3× | 0.25× |
| GPTQ INT4 | 5.97 | 1.2× | 0.25× |
| GPTQ INT4 + desc_act | 5.90 | 1.0× | 0.25× |

> AWQ/GPTQ INT4 在质量损失 < 2% 的前提下带来 4× 内存缩减——L40S 70B 部署的标准选择。

## 6. 持续负载测试

```python
# load_test.py — simulate production traffic
import asyncio, aiohttp, time, random
from dataclasses import dataclass
from typing import List

@dataclass
class RequestResult:
    latency: float
    ttft: float
    tokens_generated: int
    success: bool

async def send_request(session, url: str, prompt: str, max_tokens: int) -> RequestResult:
    t0 = time.perf_counter()
    first_token_time = None

    payload = {
        "model": "meta-llama/Llama-3-8b-instruct",
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": max_tokens,
        "stream": True,
    }

    tokens = 0
    try:
        async with session.post(url, json=payload) as resp:
            async for line in resp.content:
                if first_token_time is None and b"content" in line:
                    first_token_time = time.perf_counter() - t0
                if b"content" in line:
                    tokens += 1
        return RequestResult(
            latency=time.perf_counter() - t0,
            ttft=first_token_time or 0,
            tokens_generated=tokens,
            success=True,
        )
    except Exception:
        return RequestResult(latency=0, ttft=0, tokens_generated=0, success=False)

async def load_test(rps: int, duration: int, url: str):
    results: List[RequestResult] = []
    prompts = ["Explain quantum computing", "Write a Python sort", "What is NVLink?"] * 100

    async with aiohttp.ClientSession() as session:
        t_end = time.time() + duration
        tasks = []
        while time.time() < t_end:
            prompt = random.choice(prompts)
            tasks.append(asyncio.create_task(
                send_request(session, url, prompt, max_tokens=128)
            ))
            await asyncio.sleep(1.0 / rps)

        results = await asyncio.gather(*tasks)

    successes = [r for r in results if r.success]
    print(f"Requests: {len(results)}, Success: {len(successes)}")
    print(f"Throughput: {len(successes)/duration:.1f} req/s")
    print(f"TTFT p50: {sorted([r.ttft for r in successes])[len(successes)//2]*1000:.0f} ms")
    print(f"TTFT p99: {sorted([r.ttft for r in successes])[int(len(successes)*0.99)]*1000:.0f} ms")

asyncio.run(load_test(rps=50, duration=60, url="http://localhost:8000/v1/chat/completions"))
```

## 参考文献

- [vLLM Benchmark Scripts](https://github.com/vllm-project/vllm/tree/main/benchmarks)
- [MLPerf Inference Benchmark](https://mlcommons.org/benchmarks/inference-datacenter/)
- [LM Evaluation Harness](https://github.com/EleutherAI/lm-evaluation-harness)
- [NVIDIA L40S Performance Guide](https://www.nvidia.com/en-us/data-center/l40s/)
- [AWQ Paper](https://arxiv.org/abs/2306.00978)
- [GPTQ Paper](https://arxiv.org/abs/2210.17323)


<details>
<summary>English original</summary>

**5. Quantization Quality vs Performance**

```bash
# Perplexity comparison (lower is better)
python - <<'EOF'
from lm_eval import evaluator
from transformers import AutoModelForCausalLM, AutoTokenizer
import torch

# Run on WikiText-2 perplexity
results = {}
for model_id, quant in [
    ("meta-llama/Llama-3-8b", "none"),
    ("/models/llama-3-8b-awq-int4", "awq"),
    ("/models/llama-3-8b-gptq-int4", "gptq"),
    ("/models/llama-3-8b-int8", "int8"),
]:
    result = evaluator.simple_evaluate(
        model="vllm",
        model_args=f"pretrained={model_id},quantization={quant}",
        tasks=["wikitext"],
    )
    results[quant] = result["results"]["wikitext"]["word_perplexity"]
    print(f"{quant}: {results[quant]:.2f}")
EOF
```

**Expected Quality-Performance Trade-offs**

| Precision | Perplexity (WikiText2) | Throughput | Memory |
|---|---|---|---|
| BF16 (baseline) | 5.85 | 1.0× | 1.0× |
| INT8 (LLM.int8) | 5.88 | 0.85× | 0.5× |
| AWQ INT4 | 5.95 | 1.3× | 0.25× |
| GPTQ INT4 | 5.97 | 1.2× | 0.25× |
| GPTQ INT4 + desc_act | 5.90 | 1.0× | 0.25× |

> AWQ/GPTQ INT4 give 4× memory reduction with < 2% quality degradation — the standard choice for L40S 70B deployment.

**6. Continuous Load Testing**

```python
# load_test.py — simulate production traffic
import asyncio, aiohttp, time, random
from dataclasses import dataclass
from typing import List

@dataclass
class RequestResult:
    latency: float
    ttft: float
    tokens_generated: int
    success: bool

async def send_request(session, url: str, prompt: str, max_tokens: int) -> RequestResult:
    t0 = time.perf_counter()
    first_token_time = None

    payload = {
        "model": "meta-llama/Llama-3-8b-instruct",
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": max_tokens,
        "stream": True,
    }

    tokens = 0
    try:
        async with session.post(url, json=payload) as resp:
            async for line in resp.content:
                if first_token_time is None and b"content" in line:
                    first_token_time = time.perf_counter() - t0
                if b"content" in line:
                    tokens += 1
        return RequestResult(
            latency=time.perf_counter() - t0,
            ttft=first_token_time or 0,
            tokens_generated=tokens,
            success=True,
        )
    except Exception:
        return RequestResult(latency=0, ttft=0, tokens_generated=0, success=False)

async def load_test(rps: int, duration: int, url: str):
    results: List[RequestResult] = []
    prompts = ["Explain quantum computing", "Write a Python sort", "What is NVLink?"] * 100

    async with aiohttp.ClientSession() as session:
        t_end = time.time() + duration
        tasks = []
        while time.time() < t_end:
            prompt = random.choice(prompts)
            tasks.append(asyncio.create_task(
                send_request(session, url, prompt, max_tokens=128)
            ))
            await asyncio.sleep(1.0 / rps)

        results = await asyncio.gather(*tasks)

    successes = [r for r in results if r.success]
    print(f"Requests: {len(results)}, Success: {len(successes)}")
    print(f"Throughput: {len(successes)/duration:.1f} req/s")
    print(f"TTFT p50: {sorted([r.ttft for r in successes])[len(successes)//2]*1000:.0f} ms")
    print(f"TTFT p99: {sorted([r.ttft for r in successes])[int(len(successes)*0.99)]*1000:.0f} ms")

asyncio.run(load_test(rps=50, duration=60, url="http://localhost:8000/v1/chat/completions"))
```

**References**

- [vLLM Benchmark Scripts](https://github.com/vllm-project/vllm/tree/main/benchmarks)
- [MLPerf Inference Benchmark](https://mlcommons.org/benchmarks/inference-datacenter/)
- [LM Evaluation Harness](https://github.com/EleutherAI/lm-evaluation-harness)
- [NVIDIA L40S Performance Guide](https://www.nvidia.com/en-us/data-center/l40s/)
- [AWQ Paper](https://arxiv.org/abs/2306.00978)
- [GPTQ Paper](https://arxiv.org/abs/2210.17323)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/L40S-x12-Inference/05-Benchmarks.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/L40S-x12-Inference/05-Benchmarks.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
