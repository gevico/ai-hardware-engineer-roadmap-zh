---
title: 03 — L40S x12 的多 GPU 策略
description: 03 — L40S x12 的多 GPU 策略
---

# 03 — L40S x12 的多 GPU 策略

## 1. 多 GPU 推理的 PCIe 瓶颈

没有 NVLink 时，所有 GPU 间通信都要经过 PCIe fabric。这从根本上改变了并行的决策。

### 通信延迟对比

```
NVLink 4.0 (H200):
  All-reduce 1 GB across 8 GPUs: ~2 ms
  Latency per hop: ~1 µs

PCIe 4.0 (L40S):
  All-reduce 1 GB across 2 GPUs (same switch): ~30 ms
  All-reduce 1 GB across 8 GPUs (cross NUMA): ~60-100 ms
  Latency per hop: ~5-15 µs
```

对使用 TP 的 transformer 推理，all-reduce 每层发生**两次**（QKV 投影之后和输出投影之后）。在 PCIe 上跑 80 层、TP=8 时，通信可能压过计算。

### L40S 的经验法则

```
TP=1  (no tensor parallel):
  Zero communication overhead. Best for small models (≤ 48 GB).

TP=2  (2 GPUs, same PCIe switch):
  ~30-50 ms communication per all-reduce.
  Acceptable for 70B models where compute > communication.

TP=4  (may cross NUMA):
  Communication overhead often exceeds compute benefit.
  Only worthwhile if model > 2×48 = 96 GB in memory.

TP=8+ on PCIe:
  Not recommended for L40S. Communication bottleneck dominates.
  Use pipeline parallelism or separate model replicas instead.
```

## 2. 12x L40S 的部署模式

### 模式 A：12 个独立的单 GPU 实例

适用：≤ 48 GB 的模型（7B BF16、13B BF16、70B INT4）

```
GPU 0  → Model replica 1 (e.g., Llama-3-8B)  → handles requests 0-N
GPU 1  → Model replica 2                       → handles requests 0-N
...
GPU 11 → Model replica 12                      → handles requests 0-N

Load balancer → round-robin across 12 replicas
```

```python
# Launch 12 independent vLLM instances on ports 8000-8011
for i in range(12):
    subprocess.Popen([
        "python", "-m", "vllm.entrypoints.openai.api_server",
        "--model", "meta-llama/Llama-3-8b-instruct",
        "--gpu-ids", str(i),
        "--port", str(8000 + i),
    ])

# nginx.conf load balancer
upstream vllm_cluster {
    least_conn;
    server localhost:8000;
    server localhost:8001;
    # ... up to localhost:8011
}
```

**吞吐：** 12× 单 GPU 吞吐，无通信开销。

### 模式 B：6 × TP=2 实例

适用：48-96 GB 的模型（70B BF16 需要 140 GB → 在 2 张 GPU 上用 INT8 做到 70 GB）

```
GPU 0+1   → Model replica 1 (TP=2, same PCIe switch)
GPU 2+3   → Model replica 2 (TP=2, same PCIe switch)
GPU 4+5   → Model replica 3 (TP=2)
GPU 6+7   → Model replica 4 (TP=2)
GPU 8+9   → Model replica 5 (TP=2)
GPU 10+11 → Model replica 6 (TP=2)

6 replicas × 70B INT8 = 6× the throughput of a single 2-GPU instance
```

```bash
# 6 vLLM instances, each with TP=2
for i in 0 1 2 3 4 5; do
    CUDA_VISIBLE_DEVICES=$((i*2)),$((i*2+1)) \
    python -m vllm.entrypoints.openai.api_server \
        --model meta-llama/Llama-3-70b-instruct \
        --quantization awq \
        --tensor-parallel-size 2 \
        --port $((8000 + i)) &
done
```

**确保成对的 GPU 共用同一个 PCIe switch** — 用 `nvidia-smi topo -m` 来验证。

### 模式 C：3 × TP=4 实例

适用：180B 模型（INT4 下每个分片需要约 90 GB）

```
GPU 0-3   → Model replica 1 (TP=4)
GPU 4-7   → Model replica 2 (TP=4)
GPU 8-11  → Model replica 3 (TP=4)
```

```bash
CUDA_VISIBLE_DEVICES=0,1,2,3 \
python -m vllm.entrypoints.openai.api_server \
    --model /models/llama-70b-awq-int4 \
    --tensor-parallel-size 4 \
    --port 8000
```

**警告：** PCIe 上的 TP=4 通信密集。用 Nsight Systems 监控，确认 GPU 利用率 > 50%。

### 模式 D：流水线并行（PP）—— 对 PCIe 优于 TP

流水线并行把模型 layer 切分到多张 GPU 上，而不是切分张量。通信只是 stage 之间的激活值（每个 layer 边界一次），不是 all-reduce。

```
Pipeline Parallel (PP=4):
  GPU 0: Layers 0-19   (processes batch, sends activations to GPU 1)
  GPU 1: Layers 20-39  (receives, processes, sends to GPU 2)
  GPU 2: Layers 40-59  (receives, processes, sends to GPU 3)
  GPU 3: Layers 60-79  (produces output)

Communication: activation tensor (batch × seq × hidden) = much smaller than gradients
```

```python
# vLLM pipeline parallel (experimental in recent versions)
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",
    tensor_parallel_size=1,
    pipeline_parallel_size=4,  # split layers across 4 GPUs
    max_model_len=4096,
)

# TRT-LLM has mature PP support
trtllm-build \
    --checkpoint_dir /ckpt \
    --output_dir /engine \
    --pp_size 4 \
    --tp_size 1 \
    --workers 4
```

### PP 通信分析

```
Activation per pipeline boundary:
  batch=32, seq=512, hidden=8192 (70B Llama)
  = 32 × 512 × 8192 × 2 bytes = 268 MB

PCIe 4.0 transfer time: 268 MB / 32 GB/s = ~8 ms
Compute per stage (20 layers): ~50 ms (estimated)

Overlap possible with micro-batching (1F1B schedule):
  Fill pipeline with 4 micro-batches → GPU bubble = 3/7 ≈ 43%
  Effective utilization: ~57% (acceptable for inference)
```


<details>
<summary>English original</summary>

**03 — Multi-GPU Strategy for L40S x12**

**1. The PCIe Bottleneck for Multi-GPU Inference**

Without NVLink, all GPU-to-GPU communication passes through the PCIe fabric. This fundamentally changes parallelism decisions.

**Communication Latency Comparison**

```
NVLink 4.0 (H200):
  All-reduce 1 GB across 8 GPUs: ~2 ms
  Latency per hop: ~1 µs

PCIe 4.0 (L40S):
  All-reduce 1 GB across 2 GPUs (same switch): ~30 ms
  All-reduce 1 GB across 8 GPUs (cross NUMA): ~60-100 ms
  Latency per hop: ~5-15 µs
```

For transformer inference with TP, the all-reduce happens **twice per layer** (after QKV projection and after output projection). With 80 layers and TP=8 on PCIe, communication can dominate over compute.

**Rule of Thumb for L40S**

```
TP=1  (no tensor parallel):
  Zero communication overhead. Best for small models (≤ 48 GB).

TP=2  (2 GPUs, same PCIe switch):
  ~30-50 ms communication per all-reduce.
  Acceptable for 70B models where compute > communication.

TP=4  (may cross NUMA):
  Communication overhead often exceeds compute benefit.
  Only worthwhile if model > 2×48 = 96 GB in memory.

TP=8+ on PCIe:
  Not recommended for L40S. Communication bottleneck dominates.
  Use pipeline parallelism or separate model replicas instead.
```

**2. Deployment Patterns for 12x L40S**

**Pattern A: 12 Independent Single-GPU Instances**

Best for: models ≤ 48 GB (7B BF16, 13B BF16, 70B INT4)

```
GPU 0  → Model replica 1 (e.g., Llama-3-8B)  → handles requests 0-N
GPU 1  → Model replica 2                       → handles requests 0-N
...
GPU 11 → Model replica 12                      → handles requests 0-N

Load balancer → round-robin across 12 replicas
```

```python
# Launch 12 independent vLLM instances on ports 8000-8011
for i in range(12):
    subprocess.Popen([
        "python", "-m", "vllm.entrypoints.openai.api_server",
        "--model", "meta-llama/Llama-3-8b-instruct",
        "--gpu-ids", str(i),
        "--port", str(8000 + i),
    ])

# nginx.conf load balancer
upstream vllm_cluster {
    least_conn;
    server localhost:8000;
    server localhost:8001;
    # ... up to localhost:8011
}
```

**Throughput:** 12× single-GPU throughput, no communication overhead.

**Pattern B: 6 × TP=2 Instances**

Best for: models 48-96 GB (70B BF16 needs 140 GB → use INT8 at 70 GB on 2 GPUs)

```
GPU 0+1   → Model replica 1 (TP=2, same PCIe switch)
GPU 2+3   → Model replica 2 (TP=2, same PCIe switch)
GPU 4+5   → Model replica 3 (TP=2)
GPU 6+7   → Model replica 4 (TP=2)
GPU 8+9   → Model replica 5 (TP=2)
GPU 10+11 → Model replica 6 (TP=2)

6 replicas × 70B INT8 = 6× the throughput of a single 2-GPU instance
```

```bash
# 6 vLLM instances, each with TP=2
for i in 0 1 2 3 4 5; do
    CUDA_VISIBLE_DEVICES=$((i*2)),$((i*2+1)) \
    python -m vllm.entrypoints.openai.api_server \
        --model meta-llama/Llama-3-70b-instruct \
        --quantization awq \
        --tensor-parallel-size 2 \
        --port $((8000 + i)) &
done
```

**Ensure GPU pairs share a PCIe switch** — use `nvidia-smi topo -m` to verify.

**Pattern C: 3 × TP=4 Instances**

Best for: 180B models (need ~90 GB per shard in INT4)

```
GPU 0-3   → Model replica 1 (TP=4)
GPU 4-7   → Model replica 2 (TP=4)
GPU 8-11  → Model replica 3 (TP=4)
```

```bash
CUDA_VISIBLE_DEVICES=0,1,2,3 \
python -m vllm.entrypoints.openai.api_server \
    --model /models/llama-70b-awq-int4 \
    --tensor-parallel-size 4 \
    --port 8000
```

**Warning:** TP=4 on PCIe is communication-heavy. Monitor with Nsight Systems to verify GPU utilization is > 50%.

**Pattern D: Pipeline Parallelism (PP) — Better than TP for PCIe**

Pipeline parallelism splits model layers across GPUs rather than splitting tensors. Communication is only activations between stages (once per layer boundary), not all-reduce.

```
Pipeline Parallel (PP=4):
  GPU 0: Layers 0-19   (processes batch, sends activations to GPU 1)
  GPU 1: Layers 20-39  (receives, processes, sends to GPU 2)
  GPU 2: Layers 40-59  (receives, processes, sends to GPU 3)
  GPU 3: Layers 60-79  (produces output)

Communication: activation tensor (batch × seq × hidden) = much smaller than gradients
```

```python
# vLLM pipeline parallel (experimental in recent versions)
llm = LLM(
    model="meta-llama/Llama-3-70b-instruct",
    tensor_parallel_size=1,
    pipeline_parallel_size=4,  # split layers across 4 GPUs
    max_model_len=4096,
)

# TRT-LLM has mature PP support
trtllm-build \
    --checkpoint_dir /ckpt \
    --output_dir /engine \
    --pp_size 4 \
    --tp_size 1 \
    --workers 4
```

**PP Communication Analysis**

```
Activation per pipeline boundary:
  batch=32, seq=512, hidden=8192 (70B Llama)
  = 32 × 512 × 8192 × 2 bytes = 268 MB

PCIe 4.0 transfer time: 268 MB / 32 GB/s = ~8 ms
Compute per stage (20 layers): ~50 ms (estimated)

Overlap possible with micro-batching (1F1B schedule):
  Fill pipeline with 4 micro-batches → GPU bubble = 3/7 ≈ 43%
  Effective utilization: ~57% (acceptable for inference)
```

</details>

## 3. 多节点 L40S（InfiniBand）

对于跨多台服务器、搭载 L40S GPU 的部署，InfiniBand 提供快速的服务器间通信：

```
Server 0: GPU 0-7   (L40S x8, InfiniBand HDR 200 Gb/s)
Server 1: GPU 8-15  (L40S x8, InfiniBand HDR 200 Gb/s)
...
Server N: GPU N*8 to N*8+7

Total: N servers × 8 GPUs = scalable cluster
```

```bash
# NCCL with InfiniBand (enables GPUDirect RDMA)
export NCCL_IB_HCA=mlx5_0           # InfiniBand HCA device
export NCCL_IB_GID_INDEX=3          # RoCE v2 GID
export NCCL_NET_GDR_LEVEL=5         # GPUDirect RDMA level
export NCCL_IB_TC=106               # Traffic class for priority
export NCCL_IB_SL=0                 # Service level

# Launch multi-node with torchrun
torchrun \
    --nnodes=2 \
    --nproc_per_node=8 \
    --rdzv_backend=c10d \
    --rdzv_endpoint=server0:29500 \
    inference_server.py
```

### RDMA 带宽 vs PCIe

```
InfiniBand HDR 200 Gb/s with GPUDirect RDMA:
  Effective GPU-to-GPU (across servers): ~20 GB/s (bidirectional)

PCIe 4.0 x16 (intra-server):
  Effective GPU-to-GPU (local switch): ~30 GB/s

Conclusion: InfiniBand is competitive with PCIe for cross-server communication.
Use TP=2 per server (PCIe), PP=N across servers (InfiniBand).
```

## 4. 负载均衡策略

### 请求路由器（Python 示例）

```python
import asyncio, aiohttp, random
from typing import List

class L40SLoadBalancer:
    def __init__(self, endpoints: List[str]):
        self.endpoints = endpoints
        self.request_counts = {ep: 0 for ep in endpoints}
        self.sessions = None

    async def route(self, request: dict) -> dict:
        # Least-connections routing
        endpoint = min(self.request_counts, key=self.request_counts.get)
        self.request_counts[endpoint] += 1
        try:
            async with self.sessions.post(
                f"{endpoint}/v1/completions", json=request
            ) as resp:
                return await resp.json()
        finally:
            self.request_counts[endpoint] -= 1

    def get_routing_stats(self):
        return {ep: count for ep, count in self.request_counts.items()}

# Usage
balancer = L40SLoadBalancer([
    "http://localhost:8000",  # GPU 0 — Llama-3-8B
    "http://localhost:8001",  # GPU 1 — Llama-3-8B
    # ... up to 12 replicas
])
```

### 模型感知布线

```python
# Route based on model size request
ROUTING_TABLE = {
    "llama-3-8b":   ["http://localhost:8000", ...],  # GPUs 0-3 (single GPU)
    "llama-3-70b":  ["http://localhost:8004", ...],  # GPUs 4-7 (2-GPU TP=2)
    "llama-3-405b": ["http://localhost:8008"],        # GPUs 8-11 (4-GPU TP=4)
}

async def smart_route(model_name: str, request: dict) -> dict:
    endpoints = ROUTING_TABLE.get(model_name, ROUTING_TABLE["llama-3-8b"])
    endpoint = random.choice(endpoints)  # or least-connections
    async with aiohttp.ClientSession() as session:
        async with session.post(f"{endpoint}/v1/completions", json=request) as r:
            return await r.json()
```

## 5. GPU 亲和性与 NUMA 绑定

将进程绑定到正确的 NUMA 节点，以避免跨 NUMA 的 PCIe 流量：

```bash
# Find NUMA node for each GPU
nvidia-smi topo -m | head -20
# GPU0 → NUMA node 0, GPU4 → NUMA node 1 (example)

# Bind process to correct NUMA node + CPU cores
numactl --cpunodebind=0 --membind=0 \
    CUDA_VISIBLE_DEVICES=0,1,2,3 \
    python -m vllm.entrypoints.openai.api_server \
        --model ... --tensor-parallel-size 4 --port 8000

numactl --cpunodebind=1 --membind=1 \
    CUDA_VISIBLE_DEVICES=4,5,6,7 \
    python -m vllm.entrypoints.openai.api_server \
        --model ... --tensor-parallel-size 4 --port 8001
```

## 参考文献

- [vLLM 分布式推理](https://docs.vllm.ai/en/latest/serving/distributed_serving.html)
- [TensorRT-LLM 流水线并行](https://github.com/NVIDIA/TensorRT-LLM/blob/main/docs/source/architecture/pipeline-parallelism.md)
- [NCCL 性能调优](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/usage/collectives.html)
- [GPUDirect RDMA](https://docs.nvidia.com/cuda/gpudirect-rdma/)
- [NUMA 与 PCIe 拓扑](https://www.kernel.org/doc/html/latest/admin-guide/mm/numa_memory_policy.html)


<details>
<summary>English original</summary>

**3. Multi-Node L40S (InfiniBand)**

For deployments spanning multiple servers with L40S GPUs, InfiniBand provides fast inter-server communication:

```
Server 0: GPU 0-7   (L40S x8, InfiniBand HDR 200 Gb/s)
Server 1: GPU 8-15  (L40S x8, InfiniBand HDR 200 Gb/s)
...
Server N: GPU N*8 to N*8+7

Total: N servers × 8 GPUs = scalable cluster
```

```bash
# NCCL with InfiniBand (enables GPUDirect RDMA)
export NCCL_IB_HCA=mlx5_0           # InfiniBand HCA device
export NCCL_IB_GID_INDEX=3          # RoCE v2 GID
export NCCL_NET_GDR_LEVEL=5         # GPUDirect RDMA level
export NCCL_IB_TC=106               # Traffic class for priority
export NCCL_IB_SL=0                 # Service level

# Launch multi-node with torchrun
torchrun \
    --nnodes=2 \
    --nproc_per_node=8 \
    --rdzv_backend=c10d \
    --rdzv_endpoint=server0:29500 \
    inference_server.py
```

**RDMA Bandwidth vs PCIe**

```
InfiniBand HDR 200 Gb/s with GPUDirect RDMA:
  Effective GPU-to-GPU (across servers): ~20 GB/s (bidirectional)

PCIe 4.0 x16 (intra-server):
  Effective GPU-to-GPU (local switch): ~30 GB/s

Conclusion: InfiniBand is competitive with PCIe for cross-server communication.
Use TP=2 per server (PCIe), PP=N across servers (InfiniBand).
```

**4. Load Balancing Strategies**

**Request Router (Python Example)**

```python
import asyncio, aiohttp, random
from typing import List

class L40SLoadBalancer:
    def __init__(self, endpoints: List[str]):
        self.endpoints = endpoints
        self.request_counts = {ep: 0 for ep in endpoints}
        self.sessions = None

    async def route(self, request: dict) -> dict:
        # Least-connections routing
        endpoint = min(self.request_counts, key=self.request_counts.get)
        self.request_counts[endpoint] += 1
        try:
            async with self.sessions.post(
                f"{endpoint}/v1/completions", json=request
            ) as resp:
                return await resp.json()
        finally:
            self.request_counts[endpoint] -= 1

    def get_routing_stats(self):
        return {ep: count for ep, count in self.request_counts.items()}

# Usage
balancer = L40SLoadBalancer([
    "http://localhost:8000",  # GPU 0 — Llama-3-8B
    "http://localhost:8001",  # GPU 1 — Llama-3-8B
    # ... up to 12 replicas
])
```

**Model-Aware Routing**

```python
# Route based on model size request
ROUTING_TABLE = {
    "llama-3-8b":   ["http://localhost:8000", ...],  # GPUs 0-3 (single GPU)
    "llama-3-70b":  ["http://localhost:8004", ...],  # GPUs 4-7 (2-GPU TP=2)
    "llama-3-405b": ["http://localhost:8008"],        # GPUs 8-11 (4-GPU TP=4)
}

async def smart_route(model_name: str, request: dict) -> dict:
    endpoints = ROUTING_TABLE.get(model_name, ROUTING_TABLE["llama-3-8b"])
    endpoint = random.choice(endpoints)  # or least-connections
    async with aiohttp.ClientSession() as session:
        async with session.post(f"{endpoint}/v1/completions", json=request) as r:
            return await r.json()
```

**5. GPU Affinity and NUMA Binding**

Bind processes to correct NUMA node to avoid cross-NUMA PCIe traffic:

```bash
# Find NUMA node for each GPU
nvidia-smi topo -m | head -20
# GPU0 → NUMA node 0, GPU4 → NUMA node 1 (example)

# Bind process to correct NUMA node + CPU cores
numactl --cpunodebind=0 --membind=0 \
    CUDA_VISIBLE_DEVICES=0,1,2,3 \
    python -m vllm.entrypoints.openai.api_server \
        --model ... --tensor-parallel-size 4 --port 8000

numactl --cpunodebind=1 --membind=1 \
    CUDA_VISIBLE_DEVICES=4,5,6,7 \
    python -m vllm.entrypoints.openai.api_server \
        --model ... --tensor-parallel-size 4 --port 8001
```

**References**

- [vLLM Distributed Inference](https://docs.vllm.ai/en/latest/serving/distributed_serving.html)
- [TensorRT-LLM Pipeline Parallelism](https://github.com/NVIDIA/TensorRT-LLM/blob/main/docs/source/architecture/pipeline-parallelism.md)
- [NCCL Performance Tuning](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/usage/collectives.html)
- [GPUDirect RDMA](https://docs.nvidia.com/cuda/gpudirect-rdma/)
- [NUMA and PCIe Topology](https://www.kernel.org/doc/html/latest/admin-guide/mm/numa_memory_policy.html)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/L40S-x12-Inference/03-Multi-GPU-Strategy.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/L40S-x12-Inference/03-Multi-GPU-Strategy.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
