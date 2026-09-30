---
title: 第 6 章：Qwen 在 Blackwell 上的生产推理服务
description: 第 6 章：Qwen 在 Blackwell 上的生产推理服务
---

# 第 6 章：Qwen 在 Blackwell 上的生产推理服务

## 概述

你有一台 HGX B200 机器。你有一个 benchmark 表现良好的 Qwen2.5-72B-MX-FP4-mixed engine。现在你需要以 p99 < 500 ms 服务一千个并发用户，并且不会每周二凌晨 3 点被一次告警叫醒。本章是生产工程层——runtime 选型、批处理、可观测性、容量规划、成本经济学，以及只有在真实负载下才会出现的失效模式。

Qwen Inference Optimization 系列的生产推理服务讲座（Edge AI / Lecture 05）中的大部分内容在 Blackwell 上仍然适用。本章聚焦于从 H100/H200 迁移到 B200 时**变化**的部分——以及那些没有变化但值得重新验证的部分。

读完后你应当能够：

* 针对给定工作负载，在 TRT-LLM、vLLM（Blackwell 后端）、SGLang 与 LMDeploy 之间做出选择。
* 搭建可观测性，捕获 Blackwell 特有的失效模式（kernel 回退到 FP8、NVLink-5 降级、Grace 内存路径）。
* 为 Qwen2.5-72B 聊天产品按真实的峰值系数做容量规划。
* 量化相对 H100/H200 的成本经济学——并找出 Hopper 仍然胜出的工作负载。

---

## 1. 2026 年年中的生产 runtime 选型

| Runtime | Blackwell 支持 | 最擅长 | 注意事项 |
|---|---|---|---|
| **TensorRT-LLM 0.20+** | 一等支持，经 NVIDIA 测试 | 最高原始吞吐，MX-FP4 成熟，完整集成 Triton-Inference-Server | 仅限 NVIDIA；构建流程更重 |
| **vLLM（Blackwell 后端）** | 2026 年 Q1 合入 | OpenAI-API 推理服务，广泛的模型支持，AWQ/GPTQ 回退 | MX-FP4 路径更年轻；在边界情况上成熟度较低 |
| **B200 上的 SGLang** | 积极移植中，2026 年 Q2 | 程序化生成、结构化输出、前缀缓存 | 团队更小，发布节奏更慢 |
| **LMDeploy / TurboMind** | 2026 年底的 Blackwell 移植 | Qwen 专用优化，InternLM 团队主导 | 在 Blackwell 上仍落后于 TRT-LLM/vLLM |
| **自研（CUTLASS 4 + persistent kernel）** | 自己动手 | 研究、新型量化格式、模型架构 | 除非现成 runtime 不够用，否则不要 |

对于 2026 年年中生产环境中单台 B200 或 HGX B200 机器上的 Qwen2.5-72B：**TRT-LLM 0.20+** 是默认选择。vLLM 在大多数工作负载上都有竞争力，并且在 Kubernetes 原生环境中更易部署。对于前缀缓存较重的工作负载（带长共享系统提示词的 RAG，即检索增强生成），SGLang 胜出。

---


<details>
<summary>English original</summary>

**Chapter 6: Production Serving of Qwen on Blackwell**

**Overview**

You have an HGX B200 box. You have a Qwen2.5-72B-MX-FP4-mixed engine that benchmarks well. Now you need to serve a thousand concurrent users at p99 < 500 ms and not have a 3 a.m. page every Tuesday. This chapter is the production engineering layer — runtime selection, batching, observability, capacity planning, cost economics, and the failure modes that show up only under real load.

Most of what's in the production-serving lecture of the Qwen Inference Optimization series (Edge AI / Lecture 05) still applies on Blackwell. This chapter focuses on what **changes** when you move from H100/H200 to B200 — and what doesn't change but is worth re-validating.

By the end you should be able to:

* Choose between TRT-LLM, vLLM (Blackwell backend), SGLang, and LMDeploy for a given workload.
* Set up observability that catches Blackwell-specific failure modes (kernel fallback to FP8, NVLink-5 degradation, Grace memory paths).
* Plan capacity for a Qwen2.5-72B chat product with realistic peak factors.
* Quantify the cost economics vs H100/H200 — and identify the workloads where Hopper still wins.

---

**1. Production Runtime Choices, Mid-2026**

| Runtime | Blackwell support | Best at | Caveat |
|---|---|---|---|
| **TensorRT-LLM 0.20+** | First-class, NVIDIA-tested | Highest raw throughput, MX-FP4 mature, full Triton-Inference-Server integration | NVIDIA-only; build flow is heavier |
| **vLLM (Blackwell backend)** | Merged Q1 2026 | OpenAI-API serving, broad model support, AWQ/GPTQ fallbacks | MX-FP4 path is younger; less mature on edge cases |
| **SGLang on B200** | Active port, Q2 2026 | Programmatic generation, structured outputs, prefix caching | Smaller team, slower release cadence |
| **LMDeploy / TurboMind** | Late-2026 Blackwell port | Qwen-specific optimizations, InternLM-team focus | Catching up to TRT-LLM/vLLM on Blackwell |
| **Custom (CUTLASS 4 + persistent kernels)** | Roll-your-own | Research, novel quant formats, model archs | Don't unless stock runtimes fall short |

For Qwen2.5-72B on a single B200 or HGX B200 box in mid-2026 production: **TRT-LLM 0.20+** is the default choice. vLLM is competitive on most workloads and easier to deploy in a Kubernetes-native shop. SGLang wins for prefix-cache-heavy workloads (RAG with long shared system prompts).

---

</details>

## 2. 部署模式 — Triton Inference Server + TRT-LLM

参考 NVIDIA 栈：

```
┌──────────────────────────────────────────────────────────┐
│   Triton Inference Server (port 8000)                    │
│   ┌────────────────────────────────────────────────┐    │
│   │  TRT-LLM backend                               │    │
│   │   - Continuous batching                        │    │
│   │   - Paged KV cache                             │    │
│   │   - Chunked prefill                            │    │
│   │   - Speculative decoding (EAGLE-2)             │    │
│   │   - Streaming responses                        │    │
│   └──────────────────┬─────────────────────────────┘    │
│                      │                                   │
│                      ▼                                   │
│        Qwen2.5-72B-MX-FP4 engine                         │
│        (sm_100, persistent kernels)                      │
│                      │                                   │
│                      ▼                                   │
│             B200 (or HGX B200 × N)                       │
└──────────────────────────────────────────────────────────┘
                      │
                      │ REST/gRPC
                      ▼
              Application layer
              (OpenAI-compatible API)
```

部署清单（Docker Compose 摘录）：

```yaml
services:
  triton:
    image: nvcr.io/nvidia/tritonserver:25.06-trtllm-python-py3
    runtime: nvidia
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: 8                    # HGX B200 full board
              capabilities: [gpu]
    environment:
      - NCCL_P2P_LEVEL=NVL
      - NCCL_DEBUG=WARN
      - CUDA_VISIBLE_DEVICES=0,1,2,3,4,5,6,7
      - LD_LIBRARY_PATH=/opt/tritonserver/backends/tensorrtllm
    volumes:
      - ./qwen72b-mx-fp4-engine-tp8:/models/qwen
    command: >
      tritonserver
        --model-repository=/models
        --http-port=8000
        --grpc-port=8001
        --log-verbose=1
        --strict-readiness=false
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8000/v2/health/ready"]
      interval: 30s
      timeout: 10s
      retries: 3
```

该部署的预期基线数据（2026 年中）：

| 指标 | HGX B200, TP=8 |
|---|---|
| 单流 decode（逐 token 生成阶段） | ~620 tok/s |
| 批=32 聚合 decode | ~14,000 tok/s |
| 批=128 聚合 decode | ~32,000 tok/s |
| TTFT @ 2k prompt，B=8 | 25 ms |
| TTFT @ 32k prompt（分块） | 180 ms |
| 饱和时 HBM 利用率 | ~90% |
| GPU 计算利用率 | 70-85% |
| 饱和时单 GPU 功耗 | 950-1000 W |
| 整机功耗 | 8-9 kW |

---

## 3. 可观测性 — 关注什么

Edge AI / Qwen / 第 05 讲中的看板仍然适用。Blackwell 新增的内容：

### 3.1 Blackwell 专属指标

* **MX 格式 kernel 命中率** — 命中 MX-FP4 路径的 GEMM（矩阵-矩阵乘）调用占比，相对于 FP8 回退路径。TRT-LLM 通过 `kernel_dispatch_fp4_count` 与 `kernel_dispatch_fp8_count` 暴露该指标。若 FP8 回退超过 1%，说明 recipe 过于激进。
* **Transformer Engine 2 提升事件** — 当某个 block 的量化在 runtime 被提升时（溢出检测），TE2 会记录日志。计数器：`te2_block_promotion_count`。健康值：稳态负载下 < 100/小时。
* **NVLink-5 链路健康** — 每块 GPU 的 `nvidia-smi nvlink --errors -l 0` 应显示零 CRC 错误。此处出错会使受影响的 GPU 吞吐静默下降 20–50%。
* **Grace 一致性内存访问比例**（GB200/NVL72）— 经 NVLink-C2C 从 B200 到 Grace 的内存读占比。高于约 10% 说明 KV 溢出占主导，值得重新调优。
* **TMA-2 多播利用率** — Nsight 指标 `tma_multicast_loads_per_sec`。在 attention kernel 中应较高；若缺失说明 FA-3 实际未启用。

### 3.2 Prometheus 抓取

面向 Triton + TRT-LLM Blackwell 部署的最小 Prometheus 查询集：

```promql
# Latency
histogram_quantile(0.50, rate(triton_inference_compute_input_duration_us_bucket[1m]))
histogram_quantile(0.95, rate(triton_inference_request_duration_us_bucket[1m]))

# Throughput
rate(trtllm_decoded_tokens_total[1m])

# KV cache health
trtllm_kv_cache_used_blocks / trtllm_kv_cache_total_blocks

# GPU
DCGM_FI_DEV_GPU_UTIL
DCGM_FI_DEV_MEM_COPY_UTIL
DCGM_FI_DEV_POWER_USAGE
DCGM_FI_DEV_NVLINK_BANDWIDTH_L0   # NVLink utilization

# Blackwell-specific
trtllm_mx_fp4_kernel_dispatches_total
trtllm_te2_promotion_events_total
trtllm_tma2_multicast_loads_total
```


<details>
<summary>English original</summary>

**2. Deployment Pattern — Triton Inference Server + TRT-LLM**

The reference NVIDIA stack:

```
┌──────────────────────────────────────────────────────────┐
│   Triton Inference Server (port 8000)                    │
│   ┌────────────────────────────────────────────────┐    │
│   │  TRT-LLM backend                               │    │
│   │   - Continuous batching                        │    │
│   │   - Paged KV cache                             │    │
│   │   - Chunked prefill                            │    │
│   │   - Speculative decoding (EAGLE-2)             │    │
│   │   - Streaming responses                        │    │
│   └──────────────────┬─────────────────────────────┘    │
│                      │                                   │
│                      ▼                                   │
│        Qwen2.5-72B-MX-FP4 engine                         │
│        (sm_100, persistent kernels)                      │
│                      │                                   │
│                      ▼                                   │
│             B200 (or HGX B200 × N)                       │
└──────────────────────────────────────────────────────────┘
                      │
                      │ REST/gRPC
                      ▼
              Application layer
              (OpenAI-compatible API)
```

Deployment manifest (Docker Compose excerpt):

```yaml
services:
  triton:
    image: nvcr.io/nvidia/tritonserver:25.06-trtllm-python-py3
    runtime: nvidia
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: 8                    # HGX B200 full board
              capabilities: [gpu]
    environment:
      - NCCL_P2P_LEVEL=NVL
      - NCCL_DEBUG=WARN
      - CUDA_VISIBLE_DEVICES=0,1,2,3,4,5,6,7
      - LD_LIBRARY_PATH=/opt/tritonserver/backends/tensorrtllm
    volumes:
      - ./qwen72b-mx-fp4-engine-tp8:/models/qwen
    command: >
      tritonserver
        --model-repository=/models
        --http-port=8000
        --grpc-port=8001
        --log-verbose=1
        --strict-readiness=false
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8000/v2/health/ready"]
      interval: 30s
      timeout: 10s
      retries: 3
```

Expected baseline numbers from this deployment (mid-2026):

| Metric | HGX B200, TP=8 |
|---|---|
| Single-stream decode | ~620 tok/s |
| Batch=32 aggregate decode | ~14,000 tok/s |
| Batch=128 aggregate decode | ~32,000 tok/s |
| TTFT @ 2k prompt, B=8 | 25 ms |
| TTFT @ 32k prompt (chunked) | 180 ms |
| HBM utilization at saturation | ~90% |
| GPU compute utilization | 70-85% |
| Power per GPU at saturation | 950-1000 W |
| Total box power | 8-9 kW |

---

**3. Observability — What to Watch**

The dashboards from the Edge AI / Qwen / Lecture 05 still apply. What's added for Blackwell:

**3.1 Blackwell-specific metrics**

* **MX format kernel hit rate** — fraction of GEMM calls hitting the MX-FP4 path vs the FP8 fallback path. TRT-LLM exposes this via `kernel_dispatch_fp4_count` vs `kernel_dispatch_fp8_count`. If FP8 fallbacks exceed 1%, your recipe is too aggressive.
* **Transformer Engine 2 promotion events** — TE2 logs when a block's quant gets promoted at runtime (overflow detection). Counter: `te2_block_promotion_count`. Healthy: < 100/hour under steady load.
* **NVLink-5 link health** — `nvidia-smi nvlink --errors -l 0` for each GPU should show zero CRC errors. Errors here silently degrade throughput by 20–50% for the affected GPU.
* **Grace coherent memory access ratio** (GB200/NVL72) — fraction of memory reads that crossed the NVLink-C2C from B200 to Grace. Above ~10% suggests KV spill is dominant and worth re-tuning.
* **TMA-2 multicast utilization** — Nsight metric `tma_multicast_loads_per_sec`. Should be high in attention kernels; absence means FA-3 isn't actually active.

**3.2 The Prometheus scrape**

A minimal Prometheus query set for a Triton + TRT-LLM Blackwell deployment:

```promql
# Latency
histogram_quantile(0.50, rate(triton_inference_compute_input_duration_us_bucket[1m]))
histogram_quantile(0.95, rate(triton_inference_request_duration_us_bucket[1m]))

# Throughput
rate(trtllm_decoded_tokens_total[1m])

# KV cache health
trtllm_kv_cache_used_blocks / trtllm_kv_cache_total_blocks

# GPU
DCGM_FI_DEV_GPU_UTIL
DCGM_FI_DEV_MEM_COPY_UTIL
DCGM_FI_DEV_POWER_USAGE
DCGM_FI_DEV_NVLINK_BANDWIDTH_L0   # NVLink utilization

# Blackwell-specific
trtllm_mx_fp4_kernel_dispatches_total
trtllm_te2_promotion_events_total
trtllm_tma2_multicast_loads_total
```

</details>

### 3.3 生产环境中会遇到的失效模式

| 症状 | 原因 | 修复 |
|---|---|---|
| Tok/s 一夜之间下降 30%，代码未改 | 驱动更新重置了 kernel 缓存；首批请求走冷路径 | 在部署钩子中加入预热步骤 |
| 某块 GPU 利用率 60%，其余为 80% | 该 GPU 链路上出现 NVLink-5 CRC 错误 | 重新插拔或 RMA；重新均衡分片 |
| MX-FP4 命中率 < 80% | 引擎构建时未加 `--gemm_plugin mx_fp4` | 用正确的编译选项重新构建 |
| layer 47 上 TE2 promotion 事件激增 | 某个 tensor 的激活值分布漂移 | 用覆盖面更广的数据集重新校准 |
| Grace 内存访问占比随时间上升 | Grace 层的 KV cache 碎片化 | 重启推理服务实例；排查分页策略 |
| TTFT p99 尖峰与长 prompt 相关 | 未启用 chunked prefill，或 chunk 过小 | 增大 chunk 大小或启用该项 |
| 部署后冷启动 TTFT >10s | TensorRT graph 从零编译 | 缓存编译好的引擎；启动时预加载 |
| 尚有余量时特定并发下仍 OOM | 激活值工作集尖峰，paged-KV 竞争 | 降低 max_num_tokens；提高空闲比例 |

---

## 4. 容量规划 —— 完整算例

与 Edge AI / Qwen / Lecture 05 示例相同的场景，针对 Blackwell 重新推导：

**场景：**
- 每日活跃用户 5,000（为原示例的 5 倍）。
- 每用户每天 50 轮。
- 平均 300 输入 + 400 输出 token。
- 40% 走 Qwen2.5-72B 级别的云端（60% 在边缘上免费处理）。

**每日云端负载：**
```
Queries:     5000 × 50 × 0.40 = 100,000 / day
Output tok:  100,000 × 400    = 40 M / day
Avg tok/s:   40 M / 86,400    ≈ 460 tok/s
Peak tok/s (5× peak factor):  ~2,300 tok/s
```

**选型方案：**

| 平台 | p95 SLA 下的容量 | 所需机器数 | Capex | 年 OPEX（租用） |
|---|---|---|---|---|
| HGX H100（FP8，TP=8） | 持续约 5,000 tok/s | 1 | 约 $250k | ~$170k |
| HGX H200（FP8，TP=8） | 持续约 7,000 tok/s | 1 | 约 $280k | ~$190k |
| **HGX B200（MX-FP4，TP=8）** | **持续约 15,000 tok/s** | **1** | **约 $280k** | **~$220k** |
| 单块 B200（intra-TP=2） | 持续约 3,000 tok/s | 1 | 约 $45k | ~$45k |

对于 2,300 tok/s 的峰值，实际有几种可行方案：

* **单块 B200** 配 intra-package TP=2 即可满足峰值，并留有约 25% 余量。若不需要冗余，成本优势遥遥领先。
* **HGX H100/H200** 同样可行，但余量更小，且容量上限近在眼前。
* **HGX B200** 对该负载属于过剩，但提供 6× 的增长余量 —— 若预期 12 个月内用户增长 5 倍，选它。

为冗余/故障切换，务必成对部署：例如 **2 × 单块 B200** 做主备，或 **1 台 HGX B200 + 1 块备用 B200** 实现优雅降级。生产环境不要跑单实例。

---

## 5. 成本经济学 —— Blackwell 胜在哪里，Hopper 存续在哪里

每百万 token 服务成本才是与生产相关的指标。2026 年中的数据（租用云容量，全成本）：

| 平台 | 每 1M 输出 token 成本，batch-32 稳态 |
|---|---|
| 单块 H100 SXM | $0.85 |
| HGX H100 8 卡，TP=8 | $0.34 |
| HGX H200 8 卡，TP=8 | $0.28 |
| **单块 B200** | **$0.30** |
| **HGX B200 8 卡，TP=8** | **$0.18** |
| OpenAI gpt-4.1-mini API 标价 | $0.60 |

HGX B200 8 卡的结果比 HGX H200 好约 2×，比 H100 好约 3×，**且与托管模型的 API 标价相比具备竞争力**。正是这一成本经济学拐点，推动了 2026 年市场向 Blackwell 的快速转移。

### 5.1 Hopper 在成本上仍然胜出的场景

* **不使用 FP4 的工作负载** —— 若评测显示 MX-FP4 出现无法容忍的回退，你只能在 B200 上跑 FP8。成本优势缩小到相对 H200 约 1.4× —— 仍然可观，但不再压倒性。
* **低并发的 latency-tier-0** —— 在单用户推理服务下，单 GPU 成本占主导，Hopper 更低的芯片价格胜出。单块 H100 做 chat 的成本约为单块 B200 的一半。
* **既有集群的摊销** —— 若 Hopper 集群已付款，按折旧继续跑的边际成本很小。扩容时更新，而非整体替换。
* **合规/合同** —— 部分采购流程被锁定到特定 SKU。迁移周期以季度计，而非以周计。

---


<details>
<summary>English original</summary>

**3.3 Failure modes you'll see in production**

| Symptom | Cause | Fix |
|---|---|---|
| Tok/s drops 30% overnight without code change | Driver update reset kernel cache; first req cold-paths | Warm-up step in deploy hook |
| Specific GPU shows 60% util while others 80% | NVLink-5 CRC errors on that GPU's links | Re-seat or RMA; rebalance shard |
| MX-FP4 hit rate < 80% | Engine built without `--gemm_plugin mx_fp4` | Rebuild with correct flags |
| TE2 promotion events spike on layer 47 | Activation distribution drift on a tensor | Re-calibrate with broader dataset |
| Grace memory access ratio climbs over time | KV cache fragmentation in Grace tier | Restart serving instance; investigate paging policy |
| TTFT p99 spikes correlate with long prompts | Chunked prefill not enabled or too small | Increase chunk size or enable |
| Cold-start TTFT >10s after deploy | TensorRT graph compile from scratch | Cache compiled engine; preload at boot |
| OOM at certain concurrency despite headroom | Activation working set spike, paged-KV race | Lower max_num_tokens; raise free fraction |

---

**4. Capacity Planning — Worked Example**

Same scenario as the Edge AI / Qwen / Lecture 05 example, re-derived for Blackwell:

**Scenario:**
- 5,000 daily active users (5× the original example).
- 50 turns/user/day.
- 300 input + 400 output tokens average.
- 40% to Qwen2.5-72B-class cloud (60% on edge for free).

**Daily cloud load:**
```
Queries:     5000 × 50 × 0.40 = 100,000 / day
Output tok:  100,000 × 400    = 40 M / day
Avg tok/s:   40 M / 86,400    ≈ 460 tok/s
Peak tok/s (5× peak factor):  ~2,300 tok/s
```

**Sizing options:**

| Platform | Capacity at p95 SLA | Boxes needed | Capex | Yearly OPEX (rented) |
|---|---|---|---|---|
| HGX H100 (FP8, TP=8) | ~5,000 tok/s sustained | 1 | ~$250k | ~$170k |
| HGX H200 (FP8, TP=8) | ~7,000 tok/s sustained | 1 | ~$280k | ~$190k |
| **HGX B200 (MX-FP4, TP=8)** | **~15,000 tok/s sustained** | **1** | **~$280k** | **~$220k** |
| Single B200 (intra-TP=2) | ~3,000 tok/s sustained | 1 | ~$45k | ~$45k |

For a 2,300 tok/s peak you actually have several viable options:

* **Single B200** with intra-package TP=2 fits the peak with ~25% headroom. Cheapest by a wide margin if you don't need redundancy.
* **HGX H100/H200** also works but with less headroom and a near-future capacity ceiling.
* **HGX B200** is overkill for this load but gives you 6× growth headroom — pick this if you expect to 5× users in 12 months.

For redundancy/failover, always pair: e.g., **2 × single B200** in active/passive, or **1 HGX B200 + 1 spare B200** for graceful degradation. Don't run production single-instance.

---

**5. Cost Economics — Where Blackwell Wins, Where Hopper Persists**

Per-million-tokens-served cost is the production-relevant metric. Mid-2026 numbers (rented cloud capacity, fully-loaded):

| Platform | Cost per 1M output tokens, batch-32 steady state |
|---|---|
| Single H100 SXM | $0.85 |
| HGX H100 8-GPU, TP=8 | $0.34 |
| HGX H200 8-GPU, TP=8 | $0.28 |
| **Single B200** | **$0.30** |
| **HGX B200 8-GPU, TP=8** | **$0.18** |
| OpenAI gpt-4.1-mini API list price | $0.60 |

The HGX B200 8-GPU result is ~2× better than HGX H200, ~3× better than H100, **and competitive with API list prices for hosted models**. This is the cost-economics inflection point that drove the rapid market shift to Blackwell through 2026.

**5.1 When Hopper still wins on cost**

* **Workloads that don't use FP4** — if your eval shows MX-FP4 regression you can't tolerate, you're running FP8 on B200. The cost advantage shrinks to ~1.4× over H200 — still meaningful, but less commanding.
* **Latency-tier-0 with low concurrency** — at single-user serving, the per-GPU cost dominates and Hopper's lower chip price wins. Single H100 chat costs about half what single B200 chat costs.
* **Existing fleet write-off** — if you already paid for a Hopper fleet, the marginal cost of running it through depreciation is small. Refresh on capacity-add, not on full-replace.
* **Compliance/contracts** — some procurement processes are locked to specific SKUs. Migration timelines are quarters, not weeks.

---

</details>

## 6. 生产检查清单

在把 Qwen 部署到 Blackwell 上承接真实流量之前：

- [ ] 已安装 CUDA 13.x 驱动，并通过 `cuda-compute-capability=10.0` 验证。
- [ ] 已安装 TRT-LLM 0.20+；`pip list | grep tensorrt_llm` 显示预期版本。
- [ ] 引擎以 `--gemm_plugin mx_fp4 --kv_cache_type mx_fp8` 构建（通过 cuobjdump 验证）。
- [ ] FlashAttention-3 kernel 已启用（用 Nsight 或 kernel 反汇编验证）。
- [ ] 已设置 NCCL `NCCL_P2P_LEVEL=NVL`；`nvidia-smi topo -m` 显示所有两两之间均为 NV18。
- [ ] 所有 GPU 上 `nvidia-smi nvlink --errors` 无异常。
- [ ] 校准集与部署领域匹配（chat / code / multilingual）。
- [ ] 已建立评测基线：MMLU 与 BF16 相差 0.5 分以内，IFEval 相差 1.5 分以内。
- [ ] 长上下文评测（>50k 的 needle-in-haystack）通过。
- [ ] 健康检查端点有响应：`/v2/health/ready`。
- [ ] Prometheus 抓取 `triton_*` 与 `DCGM_FI_DEV_*` 指标。
- [ ] Grafana 仪表盘包含：TTFT p50/p95/p99、ITL p50/p95、KV occupancy、MX-FP4 命中率、NVLink 错误、GPU 功耗。
- [ ] 已接好告警：TTFT p95 > SLA、GPU 功耗 < 800W（提示利用率偏低）、MX-FP4 命中率 < 80%、NVLink 错误 > 0。
- [ ] 已按预期峰值的 1.5× 做容量测试。
- [ ] 已记录故障转移方案：HGX 板上某块 GPU 失效时会发生什么。
- [ ] 部署 hook 中有预热步骤：启动后发送 N 个请求以填充 kernel/引擎缓存。
- [ ] 成本监控：每日跟踪 tokens-served / $。

---

## 7. 未来 12 个月 — 推测

2026 年年中到 2027 年年中，这一领域会发生什么变化：

* **B300 / Blackwell Ultra** 大概率规模上市。支持相同 MX 格式，内存略快，HBM 容量更大（288 GB+）。在多数部署中可直接替换 B200。
* **FP6 成熟** — 目前 FP4-mixed 是最佳平衡点；随着 kernel 成熟，FP6-pure 可能成为生产默认。相比 FP6-mixed 内存再降约 25%，质量损失近乎为零。
* **更好的投机解码** — EAGLE-3 移植到 Blackwell；吞吐倍率大概率落在 2-3× 区间。
* **量化模型的跨架构可移植性** — 目前通过仿真在 Hopper 上跑 MX-FP4 量化模型很慢；这一差距将弥合。
* **MoE 推理服务优化** — 对 Qwen3-MoE 及类似模型，Grace LPDDR 专家池模式趋于成熟，把专家推理服务的成本降低 2-3×。
* **NVL576（多机架）** — 前沿客户通过后续 NVLink 世代堆叠 NVL72 机架。万亿参数推理规模化成为常态。

---

## 关键要点

| 要点 | 为什么重要 |
|---|---|
| TRT-LLM 0.20+ 是 2026 年年中 B200 上 Qwen 的生产默认 | MX-FP4 路径成熟、Triton-server 集成、NVIDIA 已测试 |
| HGX B200 8-GPU 以同等吞吐取代 4–6 台 HGX H100 机器 | 集群更新的拐点 |
| MX-FP4 命中率是一线生产指标 | 低于 80% 意味着没拿到 Blackwell 的优势 |
| NVLink-5 CRC 错误会悄然拉低吞吐 | 加入告警；检查 `nvidia-smi nvlink --errors` |
| Blackwell 的每 token 成本比 H200 低约 2×，比 H100 低约 3× | 驱动 2026 年全年的市场转向 |
| 在许多工作负载上，单块 B200 在成本上胜过 Hopper，且无多 GPU 开销 | 若单芯片够用，不要默认选 HGX 级机器 |
| Hopper 在按资本开支摊薄的集群更新和 FP4 不兼容工作负载上仍占优 | 迁移是季度级项目，不是周级项目 |
| 容量规划没有变 — 先测量，再定规模 | 与 Hopper 相同的 TTFT/ITL/KV/util 指标 |

---

## 资源

* **[TensorRT-LLM 0.20 Release Notes](https://github.com/NVIDIA/TensorRT-LLM/releases)：** Blackwell 支持与 MX-FP4。
* **[Triton Inference Server User Guide](https://docs.nvidia.com/deeplearning/triton-inference-server/)：** 前端与编排。
* **[NVIDIA DCGM exporter](https://github.com/NVIDIA/dcgm-exporter)：** GPU 指标的 Prometheus exporter。
* **[vLLM Blackwell backend tracking issue](https://github.com/vllm-project/vllm)：** vLLM Blackwell 功能的状态。
* **[SGLang documentation](https://sgl-project.github.io/)：** B200 上的结构化生成。
* **[LMDeploy / TurboMind](https://github.com/InternLM/lmdeploy)：** 面向 Qwen 优化的推理引擎。
* **[Phase 5 — Edge AI / Qwen Inference Optimization / Lecture 05](../../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-05.md)：** 原始的生产推理服务手册。
* **[Chapter 1 — Blackwell Architecture](01-Blackwell-Architecture.md)：** 这一切的起点。


<details>
<summary>English original</summary>

**6. The Production Checklist**

Before shipping Qwen on Blackwell to real traffic:

- [ ] CUDA 13.x driver installed, verified `cuda-compute-capability=10.0`.
- [ ] TRT-LLM 0.20+ installed; `pip list | grep tensorrt_llm` shows expected version.
- [ ] Engine built with `--gemm_plugin mx_fp4 --kv_cache_type mx_fp8` (verify via cuobjdump).
- [ ] FlashAttention-3 kernels active (verify Nsight or kernel disasm).
- [ ] NCCL `NCCL_P2P_LEVEL=NVL` set; `nvidia-smi topo -m` shows NV18 between all pairs.
- [ ] `nvidia-smi nvlink --errors` clean on all GPUs.
- [ ] Calibration set matches deployment domain (chat / code / multilingual).
- [ ] Eval baseline established: MMLU within 0.5 pt of BF16, IFEval within 1.5 pt.
- [ ] Long-context eval (needle-in-haystack at >50k) passes.
- [ ] Health endpoints respond: `/v2/health/ready`.
- [ ] Prometheus scraping `triton_*` and `DCGM_FI_DEV_*` metrics.
- [ ] Grafana dashboards include: TTFT p50/p95/p99, ITL p50/p95, KV occupancy, MX-FP4 hit rate, NVLink errors, GPU power.
- [ ] Alerts wired: TTFT p95 > SLA, GPU power < 800W (suggests low utilization), MX-FP4 hit rate < 80%, NVLink errors > 0.
- [ ] Capacity tested at 1.5× expected peak.
- [ ] Failover plan documented: what happens if one GPU fails on the HGX board.
- [ ] Warm-up step in deploy hook: send N requests after start to populate kernel/engine caches.
- [ ] Cost monitoring: tokens-served / $ tracked daily.

---

**7. The Next 12 Months — Speculative**

What changes between mid-2026 and mid-2027 in this space:

* **B300 / Blackwell Ultra** likely lands in volume. Same MX format support, marginally faster memory, more HBM capacity (288 GB+). Drop-in for B200 in most deployments.
* **FP6 maturity** — currently FP4-mixed is the sweet spot; FP6-pure may become the production default as kernels mature. Cuts memory ~25% vs FP6-mixed at near-zero quality loss.
* **Better speculative decoding** — EAGLE-3 ports to Blackwell; throughput multipliers in the 2-3× range likely.
* **Cross-arch portability of quantized models** — running an MX-FP4 quantized model on Hopper via emulation is slow today; this gap closes.
* **MoE serving optimization** — for Qwen3-MoE and similar, the Grace LPDDR expert pool pattern matures, dropping the cost of expert serving by 2-3×.
* **NVL576 (multi-rack)** — frontier customers stack NVL72 racks via additional NVLink generations. Trillion-parameter inference at scale becomes routine.

---

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| TRT-LLM 0.20+ is the production default for Qwen on B200 in mid-2026 | Mature MX-FP4 path, Triton-server integration, NVIDIA-tested |
| HGX B200 8-GPU replaces 4–6 HGX H100 boxes for the same throughput | The fleet refresh inflection point |
| MX-FP4 hit rate is a top-tier production metric | Below 80% means you're not getting the Blackwell advantage |
| NVLink-5 CRC errors silently degrade throughput | Add to your alerts; check `nvidia-smi nvlink --errors` |
| Blackwell wins per-token cost ~2× over H200, ~3× over H100 | Drives the market shift through 2026 |
| Single B200 beats Hopper on cost for many workloads with no multi-GPU overhead | Don't default to HGX-class boxes if a single chip suffices |
| Hopper still wins on capex-amortized fleet refresh and FP4-incompatible workloads | Migration is a quarter-scale project, not a week-scale one |
| Capacity planning hasn't changed — measure, then size | Same TTFT/ITL/KV/util metrics as Hopper |

---

**Resources**

* **[TensorRT-LLM 0.20 Release Notes](https://github.com/NVIDIA/TensorRT-LLM/releases):** Blackwell support and MX-FP4.
* **[Triton Inference Server User Guide](https://docs.nvidia.com/deeplearning/triton-inference-server/):** Front-end and orchestration.
* **[NVIDIA DCGM exporter](https://github.com/NVIDIA/dcgm-exporter):** Prometheus exporter for GPU metrics.
* **[vLLM Blackwell backend tracking issue](https://github.com/vllm-project/vllm):** Status of vLLM Blackwell features.
* **[SGLang documentation](https://sgl-project.github.io/):** Structured generation on B200.
* **[LMDeploy / TurboMind](https://github.com/InternLM/lmdeploy):** Qwen-optimized inference engine.
* **[Phase 5 — Edge AI / Qwen Inference Optimization / Lecture 05](../../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-05.md):** The original production-serving playbook.
* **[Chapter 1 — Blackwell Architecture](01-Blackwell-Architecture.md):** Where this all starts.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Blackwell-B200-Qwen-Inference/06-Production-Serving-on-Blackwell.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Blackwell-B200-Qwen-Inference/06-Production-Serving-on-Blackwell.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
