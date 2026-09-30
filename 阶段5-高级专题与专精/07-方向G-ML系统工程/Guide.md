---
title: 阶段 5 — 方向 G：机器学习系统工程
description: 阶段 5 — 方向 G：机器学习系统工程
---

# 阶段 5 — 方向 G：机器学习系统工程

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">SYS</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 5G · 机器学习系统工程</p>
<p class="course-identity__title">构建支撑规模化 AI 的 runtime、调度器、kernel、训练系统和推理服务基础设施。</p>
<p class="course-identity__meta">产物：MLSys benchmark/runtime · 度量：延迟、内存、通信、利用率</p>
</div>
</div>


> 构建让现代 AI 工作负载可靠地训练与推理的 runtime、分布式系统、kernel、调度器和基础设施。

**layer 映射：** L3-L8。本方向串联模型数学、CUDA kernel、runtime 调度、分布式通信、簇编排、编译器/runtime 工作、推理服务系统和可观测性。

**岗位目标：** ML Systems Engineer · AI Infrastructure Engineer · Inference Systems Engineer · Training Systems Engineer · GPU Runtime Engineer · Edge AI Runtime Engineer

**前置要求：** [操作系统](../../阶段1-基础知识/03-操作系统/Guide.md)、[C++ 与并行计算](../../阶段1-基础知识/04-C++与并行计算/Guide.md)、[神经网络](../../阶段3-人工智能/01-神经网络/Guide.md)、[深度学习框架](../../阶段3-人工智能/02-深度学习框架/Guide.md)、一条阶段 4 部署路径，以及足以运行和调试 GPU benchmark 的 [GPU 基础设施](../01-方向A-GPU基础设施/Guide.md)。

**后续内容：** 一个公开的系统产物：推理 runtime、分布式训练 runbook、CUDA/Triton kernel benchmark 套件、调度器原型、编译器/runtime demo，或带可复现测量的边缘 MLSys 案例研究。

---

## 课程契约

这不是一门把 ML 框架当黑盒用的课程，而是一门讲框架之下那些系统的课程。

到课程结束时，你应当能够：

- 沿着 tensor 形状、内存流量、kernel 启动和通信调用，把 transformer 完整 trace 一遍
- 分离 prefill（首字前的整段计算）、decode（逐 token 生成阶段）、训练前向传播、反向传播、优化器状态、KV cache 和调度器开销
- 构建一个小型推理 runtime，具备批处理、流式、取消和背压
- 运行 DDP/FSDP/ZeRO 训练实验并做性能分析，而不是只读相关材料
- 编写并 benchmark 至少一个 transformer 路径中用到的 CUDA 或 Triton kernel
- 解释一个工作负载何时是算力受限、带宽受限、通信受限或调度器受限
- 把性能分析器的 trace、计数器和日志当作默认的事实来源
- 交付一个可复现的 benchmark，别的工程师拿来就能跑

重点不是成为一个通用的模型训练者，而是成为这样的工程师：知道模型为什么慢、贵、不稳定、利用率低或难以部署。

---

## 为什么这属于 AI 硬件路线图

只有软件栈能喂饱硬件，硬件才有用。

MLSys 正是工作负载现实与硬件现实交汇之处：

```text
model architecture
  -> framework graph
  -> runtime scheduler
  -> memory planner
  -> kernels
  -> collectives
  -> cluster fabric
  -> observability
  -> production behavior
```

若想设计加速器、边缘 AI 平台或 AI 基础设施，就必须理解工作负载实际要求机器做什么：

- KV cache 消耗多少字节
- attention 如何随序列长度变化
- 为什么 decode 常常变成内存带宽受限
- 为什么训练会卡在全规约或激活值内存上
- 为什么多一个同步点就能摧毁扩展性
- 为什么调度器能让一个快的 kernel 看起来慢
- 为什么拓扑不匹配会搞垮分布式推理

MLSys 这门学科所做的，就是找出这些约束、测量它们，并改动系统，让硬件更频繁地做有用功。

---


<details>
<summary>English original</summary>

**Phase 5 — Track G: ML Systems Engineering**

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">SYS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track 5G · ML Systems Engineering</p>
<p class="course-identity__title">Build the runtimes, schedulers, kernels, training systems, and serving infrastructure behind AI at scale.</p>
<p class="course-identity__meta">Artifact: MLSys benchmark/runtime · Measure: latency, memory, communication, utilization</p>
</div>
</div>


> Build the runtimes, distributed systems, kernels, schedulers, and infrastructure that make modern AI workloads train and serve reliably.

**Layer mapping:** L3-L8. This track connects model math, CUDA kernels, runtime scheduling, distributed communication, cluster orchestration, compiler/runtime work, serving systems, and observability.

**Role targets:** ML Systems Engineer · AI Infrastructure Engineer · Inference Systems Engineer · Training Systems Engineer · GPU Runtime Engineer · Edge AI Runtime Engineer

**Prerequisites:** [Operating Systems](../../阶段1-基础知识/03-操作系统/Guide.md), [C++ and Parallel Computing](../../阶段1-基础知识/04-C++与并行计算/Guide.md), [Neural Networks](../../阶段3-人工智能/01-神经网络/Guide.md), [Deep Learning Frameworks](../../阶段3-人工智能/02-深度学习框架/Guide.md), one Phase 4 deployment path, and enough [GPU Infrastructure](../01-方向A-GPU基础设施/Guide.md) to run and debug GPU benchmarks.

**What comes after:** a public systems artifact: inference runtime, distributed training runbook, CUDA/Triton kernel benchmark suite, scheduler prototype, compiler/runtime demo, or edge MLSys case study with reproducible measurements.

---

**Course Contract**

This is not a course about using ML frameworks as black boxes. It is a course about the systems below those frameworks.

By the end, you should be able to:

- trace a transformer through tensor shapes, memory traffic, kernel launches, and communication calls
- separate prefill, decode, training forward pass, backward pass, optimizer state, KV cache, and scheduler overhead
- build a small inference runtime with batching, streaming, cancellation, and backpressure
- run and profile DDP/FSDP/ZeRO training experiments instead of only reading about them
- write and benchmark at least one CUDA or Triton kernel used by a transformer path
- explain when a workload is compute-bound, memory-bound, communication-bound, or scheduler-bound
- use profiler traces, counters, and logs as the default source of truth
- ship a reproducible benchmark that another engineer can run

The point is not to become a generic model trainer. The point is to become the engineer who knows why a model is slow, expensive, unstable, underutilized, or hard to deploy.

---

**Why This Belongs In An AI Hardware Roadmap**

Hardware is only useful when the software stack can feed it.

MLSys is where workload reality meets hardware reality:

```text
model architecture
  -> framework graph
  -> runtime scheduler
  -> memory planner
  -> kernels
  -> collectives
  -> cluster fabric
  -> observability
  -> production behavior
```

If you want to design accelerators, edge AI platforms, or AI infrastructure, you need to understand what the workload actually asks the machine to do:

- how many bytes the KV cache consumes
- how attention changes with sequence length
- why decode often becomes memory-bandwidth limited
- why training stalls on all-reduce or activation memory
- why one extra synchronization point can destroy scaling
- why a scheduler can make a fast kernel look slow
- why a topology mismatch can break distributed inference

MLSys is the discipline of finding those constraints, measuring them, and changing the system so the hardware does useful work more often.

---

</details>

## 核心心智模型

大多数 MLSys 工作都在降低五种成本之一：

```text
T_total = T_compute + T_memory + T_communication + T_synchronization + T_scheduling
```

推理：

```text
T_latency = T_prefill + T_decode + T_queueing + T_communication + T_streaming
```

训练：

```text
T_step = T_forward + T_backward + T_optimizer + T_allreduce + T_checkpoint + T_sync
```

内存：

```text
M_training = M_weights + M_activations + M_gradients + M_optimizer + M_workspace
M_inference = M_weights + M_kv_cache + M_workspace + M_batch_state
```

分布式扩展：

```text
efficiency = useful_compute_time / wall_clock_time
```

好的 MLSys 工程始于假设，终于测量：

```text
observe -> isolate -> change one thing -> benchmark -> profile -> explain -> repeat
```

避免「这个更快」这类含糊说法，改用具体的论断：

- 在 32 并发请求下，p95 TTFT 从 420 ms 降到 260 ms
- KV-cache 压缩后，decode（逐 token 生成阶段）吞吐从 38 提升到 51 tok/s
- bucket 调优后，all-reduce 时间占 step 时间的比例从 31% 降到 18%
- activation checkpointing 后，训练峰值内存下降 42%
- GPU 利用率提升，是因为修好了队列饥饿，而不是因为改了 kernel

---

## 课程地图

<div class="lecture-map" markdown>

| 阶段 | 关注点 | 核心产物 |
|-------|-------|---------------|
| 0 | 测量纪律 | benchmark harness（agent 运行时框架）与性能剖析模板 |
| 1 | 系统 runtime 基础 | 带背压的类异步推理服务器 |
| 2 | Transformer 执行内部机制 | Transformer 形状、内存与吞吐报告 |
| 3 | GPU kernel 与 CUDA 性能 | 带 roofline（性能上界模型）注记的 CUDA/Triton kernel benchmark |
| 4 | 推理服务系统 | 连续批处理或分页 KV-cache 原型 |
| 5 | 分布式训练系统 | DDP/FSDP/ZeRO benchmark 与瓶颈报告 |
| 6 | AI 基础设施与编排 | 可复现的集群/runbook 与故障恢复 |
| 7 | 编译器与 runtime 层 | 图 lowering、融合或内存规划 demo |
| 8 | 研究与源码循环 | 论文复现或源码深入解析 |

</div>

推荐顺序：

1. 如果已经在做边缘推理 runtime：Stage 0 -> 1 -> 2 -> 3 -> 5。
2. 如果想做 AI 基础设施方向：Stage 0 -> 1 -> 4 -> 5 -> 6。
3. 如果想做编译器/runtime 方向：Stage 0 -> 2 -> 3 -> 7 -> 8。
4. 如果想做边缘 MLSys：Stage 0 -> 1 -> 2 -> 3 -> 4，再按需加入 Stage 5。

---

## Stage 0：测量纪律

### 为什么重要

没有测量的 MLSys 工作会变成工具观光。在更换 runtime 或 kernel 之前，先养成收集可比较数字的习惯。性能数字（延迟、吞吐）告诉你系统跑得*多快*；**质量**数字——logprobs、困惑度、KL 散度——告诉你优化是否*保住了模型*。一个悄悄把困惑度抬高 15% 的 2× 加速是退步，不是胜利。

> **★ 配套课程 — 质量测量：** [**Logprobs、Perplexity 与 KL 散度**](04-Logprobs-困惑度与KL散度/README.md) 是本阶段的基础课程。它推导了 `cross-entropy = entropy + KL`、`perplexity = exp(cross-entropy)`，并展示 llama.cpp 如何用 [mean KLD 与 top-token 一致率](04-Logprobs-困惑度与KL散度/Lecture-05.md) 给一次量化打分——正是 Stage 0 所关注的「这次优化有没有把模型搞坏？」这道关口。5 讲。

### 学习内容

- 延迟分位数：p50、p95、p99
- 吞吐：requests/sec、tokens/sec、samples/sec、tokens/sec/GPU
- GPU 计数器：occupancy、内存带宽、tensor core 利用率、SM 活跃度
- 内存指标：峰值分配量、碎片、KV-cache 块、激活值内存
- **质量指标：困惑度 / bits-per-byte、相对 FP16 参考的 KL 散度、top-1 token 一致率**（见上方配套课程）
- 分布式指标：集合通信时间、带宽、GPU 空闲时间、rank 倾斜
- 可靠性指标：错误率、重试率、检查点恢复时间、故障节点恢复

### 动手构建

写一个小的 benchmark harness，能重复运行同一工作负载并输出：

- 使用的命令行
- 硬件与驱动摘要
- git commit hash
- 模型或合成工作负载配置
- 并发度或批大小
- warmup 次数与实测迭代次数
- CSV 或 JSON 结果文件

### 用在真实技术栈中

后续每个阶段都用这个 harness。不要手工把 benchmark 数字抄进笔记，用脚本生成它们。

### 测量指标

你的 harness 应报告：

- 平均延迟与分位延迟
- 吞吐
- 峰值内存
- 若可得，CPU 与 GPU 利用率
- 性能分析器 trace 路径

### 交付物

交付 `bench/`、`results/`、`reports/` 目录，其中包含一个可复现的基线。基线可以是合成的，但必须可重复运行。

---

## Stage 1：系统 runtime 基础


<details>
<summary>English original</summary>

**The Core Mental Model**

Most MLSys work reduces one of five costs:

```text
T_total = T_compute + T_memory + T_communication + T_synchronization + T_scheduling
```

For inference:

```text
T_latency = T_prefill + T_decode + T_queueing + T_communication + T_streaming
```

For training:

```text
T_step = T_forward + T_backward + T_optimizer + T_allreduce + T_checkpoint + T_sync
```

For memory:

```text
M_training = M_weights + M_activations + M_gradients + M_optimizer + M_workspace
M_inference = M_weights + M_kv_cache + M_workspace + M_batch_state
```

For distributed scaling:

```text
efficiency = useful_compute_time / wall_clock_time
```

Good MLSys engineering starts with a hypothesis and ends with measurement:

```text
observe -> isolate -> change one thing -> benchmark -> profile -> explain -> repeat
```

Avoid vague claims like "this is faster." Use concrete claims:

- p95 TTFT dropped from 420 ms to 260 ms at 32 concurrent requests
- decode throughput increased from 38 to 51 tok/s after KV-cache compaction
- all-reduce time fell from 31 percent to 18 percent of step time after bucket tuning
- peak training memory fell by 42 percent after activation checkpointing
- GPU utilization improved because queue starvation was fixed, not because kernels changed

---

**Course Map**

<div class="lecture-map" markdown>

| Stage | Focus | Core artifact |
|-------|-------|---------------|
| 0 | Measurement discipline | benchmark harness and profiling template |
| 1 | Systems runtime foundations | async inference-like server with backpressure |
| 2 | Transformer execution internals | transformer shape, memory, and throughput report |
| 3 | GPU kernels and CUDA performance | CUDA/Triton kernel benchmark with roofline notes |
| 4 | Inference serving systems | continuous batching or paged KV-cache prototype |
| 5 | Distributed training systems | DDP/FSDP/ZeRO benchmark and bottleneck report |
| 6 | AI infrastructure and orchestration | reproducible cluster/runbook with failure recovery |
| 7 | Compiler and runtime layer | graph lowering, fusion, or memory-planning demo |
| 8 | Research and source-code loop | paper reproduction or source-code deep dive |

</div>

Recommended order:

1. If you already build edge inference runtimes: Stage 0 -> 1 -> 2 -> 3 -> 5.
2. If you want AI infrastructure roles: Stage 0 -> 1 -> 4 -> 5 -> 6.
3. If you want compiler/runtime roles: Stage 0 -> 2 -> 3 -> 7 -> 8.
4. If you want edge MLSys: Stage 0 -> 1 -> 2 -> 3 -> 4, then add Stage 5 selectively.

---

**Stage 0: Measurement Discipline**

**Why It Matters**

MLSys work without measurement turns into tool tourism. Before changing runtimes or kernels, build the habit of collecting comparable numbers. Performance numbers (latency, throughput) tell you how *fast* the system runs; **quality** numbers — logprobs, perplexity, KL divergence — tell you whether your optimization *preserved the model*. A 2× speedup that quietly raises perplexity 15% is a regression, not a win.

> **★ Companion course — quality measurement:** [**Logprobs, Perplexity & KL Divergence**](04-Logprobs-困惑度与KL散度/README.md) is the foundations course for this stage. It derives `cross-entropy = entropy + KL`, `perplexity = exp(cross-entropy)`, and shows how llama.cpp grades a quantization with [mean KLD and top-token agreement](04-Logprobs-困惑度与KL散度/Lecture-05.md) — exactly the "did the optimization break the model?" gate Stage 0 is about. 5 lectures.

**Learn**

- latency percentiles: p50, p95, p99
- throughput: requests/sec, tokens/sec, samples/sec, tokens/sec/GPU
- GPU counters: occupancy, memory bandwidth, tensor-core utilization, SM activity
- memory metrics: peak allocated, fragmentation, KV-cache blocks, activation memory
- **quality metrics: perplexity / bits-per-byte, KL divergence vs. the FP16 reference, top-1 token agreement** (see companion course above)
- distributed metrics: collective time, bandwidth, GPU idle time, rank skew
- reliability metrics: error rate, retry rate, checkpoint restore time, failed-node recovery

**Build It**

Create a small benchmark harness that can run the same workload repeatedly and emit:

- command line used
- hardware and driver summary
- git commit hash
- model or synthetic workload config
- concurrency level or batch size
- warmup count and measured iteration count
- CSV or JSON result file

**Use It In The Real Stack**

Use the harness for every later stage. Do not hand-copy benchmark numbers into notes. Generate them from scripts.

**Measure It**

Your harness should report:

- mean and percentile latency
- throughput
- peak memory
- CPU and GPU utilization if available
- profiler trace path

**Ship It**

Ship `bench/`, `results/`, and `reports/` directories with one reproducible baseline. The baseline can be synthetic, but it must be rerunnable.

---

**Stage 1: Systems Runtime Foundations**

</details>

### 为什么重要

推理或训练服务仍然是一个分布式 Linux 程序。它排队工作、搬运字节、调度线程、管理内存、处理故障，并上报遥测数据。

### 学习

- Linux 进程、线程、信号、cgroups、namespaces 与文件系统
- 内存映射、缺页、大页、pinned memory、NUMA 与零拷贝路径
- 并发：线程、锁、原子操作、队列、work stealing、取消与背压
- 网络：TCP、HTTP 流式传输、gRPC、`epoll`、`io_uring` 与 RDMA 概念
- CPU 性能：缓存局部性、SIMD 基础、`perf`、火焰图与 tracing
- 生产 runtime 基础：结构化日志、指标、trace、健康检查与优雅关闭

语言：

- Python：用于集成 ML 生态
- C++：用于 runtime 与 CUDA 集成
- Rust：在契合项目时用于基础设施与系统组件

### 构建

先构建一个推理形态的服务器，不接真实模型：

1. 接收带 prompt 长度、max tokens、优先级与截止时间的请求。
2. 将请求放入调度器队列。
3. 每隔几毫秒把兼容的请求组成批。
4. 向客户端流式返回伪造的 token。
5. 支持取消。
6. 当队列或内存预算超限时施加背压。

然后添加第二个组件：

- 带零拷贝请求解析的 tokenizer runtime，或
- 带显式分配与 shape 跟踪的 mini 张量 runtime，或
- 用于请求状态与类 KV cache 块的内存 arena。

### 在真实技术栈中使用

把你的设计与 vLLM、SGLang、Ray Serve、Triton Inference Server 中控制面的职责做对比。关注 runtime 调度什么，以及 GPU kernel 实际执行什么。

### 度量

- 并发不断提升下的 p50/p95/p99 延迟
- 队列等待时间与执行时间之比
- 不同批处理窗口下的吞吐
- 分配次数与峰值 RSS
- CPU 利用率、锁竞争与上下文切换
- 取消延迟

### 交付

交付一个异步 runtime，附带负载生成器、dashboard 或指标端点，以及一份简短报告，说明批处理何时有帮助、何时会损害尾延迟。

---

## 阶段 2：Transformer 执行内部机制

### 为什么重要

MLSys 工程师不需要发明每一种模型架构，但必须理解自己所服务或训练的模型的计算流与内存流。

Transformer 推理流程：

```text
tokens -> embeddings -> attention(Q, K, V) -> MLP -> residual -> logits -> sampler
```

Transformer 训练流程：

```text
forward -> loss -> backward -> gradients -> optimizer step -> updated weights
```

Attention：

```text
Attention(Q, K, V) = softmax((Q * K^T) / sqrt(d_k)) * V
```

### 学习

- embedding、attention、MLP、残差路径、RMSNorm/layer norm、logits 与采样
- QKV 投影、grouped-query attention、RoPE、ALiBi 风格的位置处理与 KV cache
- prefill（首字前的整段计算）与 decode（逐 token 生成阶段）
- 激活值内存与梯度流
- 优化器状态内存
- 混合精度、loss scaling 与梯度累积
- 批处理、投机解码、量化与长上下文行为

### 构建

构建一条既能训练又能推理的小型 Transformer 路径：

1. 在每个主要操作处打印张量 shape。
2. 跟踪训练期间的激活值内存。
3. 跟踪推理期间的 KV cache 内存。
4. 分开统计 prefill 与 decode 的时序。
5. 加入一个最小采样器。
6. 加入梯度累积与混合精度。

### 在真实技术栈中使用

把你这个玩具实现映射到 PyTorch 模块与生产 runtime 概念上：

| 概念 | 训练系统的关注点 | 推理系统的关注点 |
|---------|-------------------------|--------------------------|
| 激活值 | 内存与重计算 | 通常不保留 |
| 梯度 | all-reduce/reduce-scatter | 不存在 |
| 优化器状态 | 常比权重更大 | 不存在 |
| KV cache | 常规训练中不是核心 | 推理服务内存压力的主要来源 |
| 批大小 | 吞吐与收敛性 | 吞吐与延迟 |
| 序列长度 | 激活值与 attention 开销 | KV cache 与 prefill 开销 |

### 度量

- prefill 与 decode 的 tokens/sec
- 训练期间的 samples/sec 或 tokens/sec
- 按 layer 统计的激活值内存
- 每 token 的 KV cache 字节数
- 批大小与序列长度的影响
- 不同精度模式下的数值漂移

### 交付

交付一份 Transformer 执行报告，用图或表给出 shape 流、内存流与吞吐。至少包含一个你在度量中发现的意外瓶颈。

---

## 阶段 3：GPU kernel 与 CUDA 性能

### 为什么重要

这里正是 MLSys 显出硬件形态之处。你需要足够的 GPU 知识，才能判断瓶颈究竟来自带宽、启动开销、occupancy、同步，还是 tensor core 利用率。

存储层次：

```text
HBM -> L2 cache -> SM shared memory -> registers
```


<details>
<summary>English original</summary>

**Why It Matters**

An inference or training service is still a distributed Linux program. It queues work, moves bytes, schedules threads, manages memory, handles failure, and emits telemetry.

**Learn**

- Linux processes, threads, signals, cgroups, namespaces, and filesystems
- memory mapping, page faults, huge pages, pinned memory, NUMA, and zero-copy paths
- concurrency with threads, locks, atomics, queues, work stealing, cancellation, and backpressure
- networking with TCP, HTTP streaming, gRPC, `epoll`, `io_uring`, and RDMA concepts
- CPU performance: cache locality, SIMD basics, `perf`, flamegraphs, and tracing
- production runtime basics: structured logs, metrics, traces, health checks, and graceful shutdown

Languages:

- Python for ML ecosystem integration
- C++ for runtime and CUDA integration
- Rust for infrastructure and systems components when it fits the project

**Build It**

Build an inference-shaped server without a real model first:

1. Accept requests with prompt length, max tokens, priority, and deadline.
2. Put requests into a scheduler queue.
3. Batch compatible requests every few milliseconds.
4. Stream fake tokens back to clients.
5. Support cancellation.
6. Apply backpressure when queues or memory budgets are exceeded.

Then add a second component:

- tokenizer runtime with zero-copy request parsing, or
- mini tensor runtime with explicit allocation and shape tracking, or
- memory arena for request state and KV-cache-like blocks.

**Use It In The Real Stack**

Compare your design to the control-plane responsibilities in vLLM, SGLang, Ray Serve, and Triton Inference Server. Focus on what the runtime schedules and what the GPU kernels actually execute.

**Measure It**

- p50/p95/p99 latency under increasing concurrency
- queue wait time versus execution time
- throughput under different batching windows
- allocation count and peak RSS
- CPU utilization, lock contention, and context switches
- cancellation latency

**Ship It**

Ship an async runtime with a load generator, dashboard or metrics endpoint, and a short report explaining when batching helps and when it hurts tail latency.

---

**Stage 2: Transformer Execution Internals**

**Why It Matters**

MLSys engineers do not need to invent every model architecture, but they must understand the compute and memory flow of the models they serve or train.

Transformer inference flow:

```text
tokens -> embeddings -> attention(Q, K, V) -> MLP -> residual -> logits -> sampler
```

Transformer training flow:

```text
forward -> loss -> backward -> gradients -> optimizer step -> updated weights
```

Attention:

```text
Attention(Q, K, V) = softmax((Q * K^T) / sqrt(d_k)) * V
```

**Learn**

- embeddings, attention, MLP, residual paths, RMSNorm/layer norm, logits, and sampling
- QKV projection, grouped-query attention, RoPE, ALiBi-style position handling, and KV cache
- prefill versus decode
- activation memory and gradient flow
- optimizer state memory
- mixed precision, loss scaling, and gradient accumulation
- batching, speculative decoding, quantization, and long-context behavior

**Build It**

Build a small transformer path that can run both training and inference:

1. Print tensor shapes at every major operation.
2. Track activation memory during training.
3. Track KV-cache memory during inference.
4. Separate prefill and decode timing.
5. Add a minimal sampler.
6. Add gradient accumulation and mixed precision.

**Use It In The Real Stack**

Map your toy implementation to PyTorch modules and to production runtime concepts:

| Concept | Training system concern | Inference system concern |
|---------|-------------------------|--------------------------|
| activations | memory and recomputation | usually not retained |
| gradients | all-reduce/reduce-scatter | not present |
| optimizer state | often larger than weights | not present |
| KV cache | not central in normal training | primary serving memory pressure |
| batch size | throughput and convergence | throughput and latency |
| sequence length | activation and attention cost | KV cache and prefill cost |

**Measure It**

- tokens/sec for prefill and decode
- samples/sec or tokens/sec during training
- activation memory by layer
- KV-cache bytes per token
- effect of batch size and sequence length
- numerical drift across precision modes

**Ship It**

Ship a transformer execution report with diagrams or tables for shape flow, memory flow, and throughput. Include at least one surprising bottleneck you found from measurement.

---

**Stage 3: GPU Kernels And CUDA Performance**

**Why It Matters**

This is where MLSys becomes hardware-shaped. You need enough GPU knowledge to know whether a bottleneck is bandwidth, launch overhead, occupancy, synchronization, or tensor-core utilization.

Memory hierarchy:

```text
HBM -> L2 cache -> SM shared memory -> registers
```

</details>

### 学习

- CUDA grid、block、warp、流、event 与同步
- warp 执行、发散、occupancy、访存合并与 bank 冲突
- 共享内存、寄存器、L2 行为与 HBM 带宽
- Tensor Core 与矩阵分块
- 归约、scan、softmax、归一化与矩阵乘 kernel
- kernel 启动开销、CUDA graph、persistent kernel 与 kernel 融合
- 使用 Nsight Systems、Nsight Compute 与简单 CUDA event 的 profiler 工作流

### 动手实现

构建一条小型 kernel 阶梯：

1. 向量加基线
2. 归约 kernel
3. LayerNorm 或 RMSNorm
4. 分块矩阵乘
5. 融合的 RMSNorm + 残差，或融合的 bias + 激活函数
6. 其中一个 kernel 的 Triton 版本

然后加一个 Transformer 形状的 benchmark：

- 在可行时比较朴素 attention、分块 attention 与库 attention
- 比较单 kernel 与融合路径
- 在适用时比较逐次启动执行与 CUDA graph 捕获

### 在真实技术栈中使用

带着一个问题读源码：这段代码消除了哪些内存流量？

研读：

- CUTLASS：分块 GEMM（矩阵-矩阵乘）与基于模板的 kernel 结构
- FlashAttention：attention 内存流量削减
- TensorRT-LLM kernel：生产级 LLM 推理路径
- vLLM 或 SGLang：调度器/runtime 与 kernel 的交互
- llama.cpp CUDA 路径：更小、更易读的推理 kernel

### 度量

- 实测带宽
- 实测 FLOP/s
- occupancy
- 全局内存事务数
- 共享内存 bank 冲突
- Tensor Core 利用率
- kernel 启动次数
- 相对参考实现的数值误差

### 交付

交付一套 kernel benchmark 套件，包含前后对比数字、profiler 截图或导出的报告，以及每个 kernel 的简短 roofline（性能上界模型）风格说明。

---

## 阶段 4：推理服务系统

### 为什么重要

推理服务是 kernel 成为产品级基础设施的地方。runtime 必须控制排队、内存、流式输出、公平性、过载、放置与可观测性。

推理服务延迟大致可分解为：

```text
T_latency = T_queue + T_prefill + T_decode + T_stream + T_network
```

吞吐通常受限于：

```text
min(compute capacity, memory bandwidth, KV-cache capacity, scheduler efficiency)
```

### 学习

- prefill（首字前的整段计算）与 decode（逐 token 生成阶段）的调度
- 批处理与连续批处理
- 请求准入与过载控制
- 流式响应与取消
- 分页 KV cache 与 block 分配
- 前缀缓存与 prompt 共享
- 投机解码
- 张量并行与流水线并行推理
- 自动扩缩容、放置、健康检查与摘流逻辑
- 可观测性：TTFT、token 间延迟、队列深度、活跃序列数、KV block 数、tokens/sec/GPU

### 动手实现

分层构建一个推理服务原型：

1. 带截止时间与优先级的请求队列
2. 连续批处理调度器
3. KV cache block 分配器
4. 流式 token 输出
5. 取消与驱逐路径
6. 基于内存预算的准入控制
7. metrics 端点

可选进阶内容：

- 带 draft 模型与 target 模型的投机解码路径
- 按队列深度与健康状态选择副本的分布式路由器
- 带显式通信调用的张量并行玩具 runtime

### 在真实技术栈中使用

研读：

- vLLM：PagedAttention、连续批处理与推理服务抽象
- SGLang：结构化生成的 runtime 思路
- TensorRT-LLM：优化后的 NVIDIA 推理路径
- Triton Inference Server：生产级模型推理服务模式
- Ray Serve：分布式服务编排
- llama.cpp：本地与边缘推理的约束

### 度量

- p50/p95/p99 首 token 时间
- p50/p95/p99 token 间延迟
- requests/sec 与 tokens/sec/GPU
- prefill 吞吐与 decode 吞吐对比
- KV cache 利用率与碎片
- 活跃序列数
- 调度器开销
- 过载下的尾延迟

### 交付

交付一份推理服务报告，能够回答：

- 低并发下的瓶颈是什么？
- 高并发下的瓶颈是什么？
- 每个活跃请求的 KV cache 占用多少内存？
- 批处理在何时提升吞吐却损害延迟？
- 系统过载时会做什么？

---

## 阶段 5：分布式训练系统

### 为什么重要

训练系统教授的是仅靠推理无法覆盖的那部分 MLSys：梯度同步、激活值内存、优化器状态切分、检查点、数据加载与故障恢复。

基本更新：

```text
theta_next = theta - learning_rate * gradient(loss, theta)
```

单步时间：

```text
T_step = T_forward + T_backward + T_optimizer + T_communication + T_sync
```

训练内存：

```text
M_total = M_weights + M_activations + M_gradients + M_optimizer + M_workspace
```


<details>
<summary>English original</summary>

**Learn**

- CUDA grids, blocks, warps, streams, events, and synchronization
- warp execution, divergence, occupancy, memory coalescing, and bank conflicts
- shared memory, registers, L2 behavior, and HBM bandwidth
- tensor cores and matrix tiling
- reductions, scans, softmax, normalization, and matmul kernels
- kernel launch overhead, CUDA graphs, persistent kernels, and kernel fusion
- profiler workflow with Nsight Systems, Nsight Compute, and simple CUDA events

**Build It**

Build a small kernel ladder:

1. vector add baseline
2. reduction kernel
3. layer norm or RMSNorm
4. tiled matrix multiply
5. fused RMSNorm + residual or fused bias + activation
6. Triton version of one kernel

Then add a transformer-shaped benchmark:

- compare naive attention, tiled attention, and library attention where possible
- compare single kernel versus fused path
- compare launch-by-launch execution versus CUDA graph capture when applicable

**Use It In The Real Stack**

Read source with one question in mind: what memory traffic did this code remove?

Study:

- CUTLASS for tiled GEMM and template-based kernel structure
- FlashAttention for attention memory traffic reduction
- TensorRT-LLM kernels for production LLM inference paths
- vLLM or SGLang for scheduler/runtime interaction with kernels
- llama.cpp CUDA paths for smaller, readable inference kernels

**Measure It**

- achieved bandwidth
- achieved FLOP/s
- occupancy
- global memory transactions
- shared-memory bank conflicts
- tensor-core utilization
- kernel launch count
- numerical error versus reference implementation

**Ship It**

Ship a kernel benchmark suite with before/after numbers, profiler screenshots or exported reports, and a short roofline-style explanation for each kernel.

---

**Stage 4: Inference Serving Systems**

**Why It Matters**

Serving is where kernels become product infrastructure. The runtime must control queueing, memory, streaming, fairness, overload, placement, and observability.

Serving latency decomposes roughly into:

```text
T_latency = T_queue + T_prefill + T_decode + T_stream + T_network
```

Throughput is often constrained by:

```text
min(compute capacity, memory bandwidth, KV-cache capacity, scheduler efficiency)
```

**Learn**

- prefill versus decode scheduling
- batching and continuous batching
- request admission and overload control
- streaming responses and cancellation
- paged KV cache and block allocation
- prefix caching and prompt sharing
- speculative decoding
- tensor-parallel and pipeline-parallel inference
- autoscaling, placement, health checks, and drain logic
- observability: TTFT, inter-token latency, queue depth, active sequences, KV blocks, tokens/sec/GPU

**Build It**

Build a serving prototype in layers:

1. request queue with deadlines and priorities
2. continuous batching scheduler
3. KV-cache block allocator
4. streaming token output
5. cancellation and eviction path
6. admission control based on memory budget
7. metrics endpoint

Optional advanced additions:

- speculative decoding path with draft and target model
- distributed router that selects replicas by queue depth and health
- tensor-parallel toy runtime with explicit communication calls

**Use It In The Real Stack**

Study:

- vLLM for PagedAttention, continuous batching, and serving abstractions
- SGLang for structured generation runtime ideas
- TensorRT-LLM for optimized NVIDIA inference paths
- Triton Inference Server for production model serving patterns
- Ray Serve for distributed service orchestration
- llama.cpp for local and edge inference constraints

**Measure It**

- p50/p95/p99 time-to-first-token
- p50/p95/p99 inter-token latency
- requests/sec and tokens/sec/GPU
- prefill throughput versus decode throughput
- KV-cache utilization and fragmentation
- active sequence count
- scheduler overhead
- tail latency under overload

**Ship It**

Ship a serving report that can answer:

- What is the bottleneck at low concurrency?
- What is the bottleneck at high concurrency?
- How much memory does the KV cache consume per active request?
- When does batching improve throughput but hurt latency?
- What does the system do when overloaded?

---

**Stage 5: Distributed Training Systems**

**Why It Matters**

Training systems teach the part of MLSys that inference alone does not: gradient synchronization, activation memory, optimizer-state partitioning, checkpointing, data loading, and failure recovery.

Basic update:

```text
theta_next = theta - learning_rate * gradient(loss, theta)
```

Step time:

```text
T_step = T_forward + T_backward + T_optimizer + T_communication + T_sync
```

Training memory:

```text
M_total = M_weights + M_activations + M_gradients + M_optimizer + M_workspace
```

</details>

### 学习

- autograd、激活值内存与重计算
- 混合精度、梯度缩放与梯度累积
- 优化器状态内存与分片
- 数据并行、张量并行、流水线并行、序列并行与专家并行
- DDP、FSDP、ZeRO 与 DTensor/DeviceMesh 概念
- 全规约、reduce-scatter、all-gather、广播与 point-to-point send/recv
- NCCL 拓扑、NVLink/NVSwitch、PCIe、InfiniBand、RoCE 与 RDMA 概念
- 检查点、弹性恢复、rank 失效与重启行为

### 动手构建

按以下顺序构建训练进阶路线：

1. 单 GPU Transformer 训练循环
2. 带激活值与优化器状态的内存 profile
3. 在 2 个或更多 GPU 上跑 DDP
4. 在同一模型上跑 FSDP 或 ZeRO
5. 激活值检查点实验
6. 分布式检查点保存与恢复
7. NCCL 调试/profile 运行

如果本地只有一块 GPU，分布式那一步就用租来的 GPU。产物比拥有集群更重要。

### 在真实技术栈中运用

研读：

- PyTorch Distributed：进程组、DDP、FSDP、DTensor 与 DeviceMesh
- DeepSpeed：ZeRO 与优化器/内存切分
- Megatron-LM：张量并行、流水线并行与序列并行模式
- Ray Train：作业编排与分布式训练的易用性
- Slurm 或 Kubernetes：调度真实 GPU 作业

### 度量

- 每 GPU 的 samples/sec 或 tokens/sec
- step 耗时拆解
- 扩展效率
- 全规约/reduce-scatter 耗时
- GPU 空闲时间
- data-loader 停滞时间
- 激活值内存
- 优化器状态内存
- 检查点写入与恢复耗时

### 交付

交付一份训练系统报告，比较单 GPU、DDP 与 FSDP/ZeRO。包含 profiler trace，并清楚说明每次运行中哪项开销占主导。

---

## Stage 6：AI 基础设施与编排

### 为什么重要

MLSys 不会止步于 kernel 和框架。真实系统还需要调度、部署、隔离、存储、监控、发布与恢复。

### 学习

- 用 Slurm、Kubernetes、Ray 或更小的自研调度器做 GPU 调度
- 放置约束：GPU 类型、内存、拓扑、MIG、NUMA 与网络可达性
- 容器镜像、驱动兼容性、CUDA runtime 兼容性与可复现环境
- 数据流水线吞吐与存储局部性
- 检查点存储、产物版本管理与恢复路径
- 自动扩缩容与准入控制
- 多租户隔离与配额策略
- 指标、链路追踪、日志、告警与 SLO
- 故障调试：挂起、OOM、坏节点、慢链路、时钟降频与版本偏移

### 动手构建

构建一份小而真实的 runbook：

1. 为训练和推理服务定义容器镜像
2. 在本地或单节点上跑一个 benchmark 作业
3. 通过调度器跑同一个 benchmark
4. 收集日志、指标与 profiler trace
5. 模拟一次故障：OOM、进程被杀、worker 丢失、检查点失败或配置错误
6. 记录恢复路径

### 在真实技术栈中运用

把基础设施与工作负载绑定：

- 训练作业需要检查点/重启与高效的数据加载
- 推理服务需要健康检查、摘流与过载行为
- 多 GPU 作业需要拓扑感知的放置
- 边缘系统需要在部署方案中考虑热管理、功耗与存储约束

### 度量

- 作业启动时间
- 镜像大小与冷启动时间
- GPU 分配效率
- 失败作业恢复时间
- 检查点恢复时间
- data-loader 吞吐
- 发布期间的服务可用性

### 交付

交付一份运维级 runbook，包含确切的命令、配置、预期指标和故障模式表。

---

## Stage 7：编译器与 runtime 层

### 为什么重要

编译器/runtime 的工作把模型图与硬件执行连接起来。高层算子正是在这里变成融合 kernel、内存规划和后端专属代码。

编译器路径：

```text
framework graph -> IR -> graph rewrite -> fused operators -> lowered kernels -> runtime execution
```

### 学习

- PyTorch 的图捕获、导出与编译路径
- 图优化：常量折叠、死代码消除、布局变更与算子融合
- 内存规划与缓冲区复用
- MLIR 方言与 pass
- TVM 调度与自动调优概念
- XLA 图编译
- TensorRT 图优化
- 用 Triton 语言编写自定义 kernel
- 重写后图的正确性测试

### 动手构建

选做一项：

- 融合两个简单张量算子，并测量启动次数的减少
- 为 RMSNorm、softmax 或小型矩阵乘写一个 Triton kernel
- 把一个玩具张量算子经 MLIR lower
- 对一个小模型片段，比较 TensorRT 输出与 eager PyTorch
- 为固定图构建静态内存规划器


<details>
<summary>English original</summary>

**Learn**

- autograd, activation memory, and recomputation
- mixed precision, gradient scaling, and gradient accumulation
- optimizer state memory and sharding
- data parallelism, tensor parallelism, pipeline parallelism, sequence parallelism, and expert parallelism
- DDP, FSDP, ZeRO, and DTensor/DeviceMesh concepts
- all-reduce, reduce-scatter, all-gather, broadcast, and point-to-point send/recv
- NCCL topology, NVLink/NVSwitch, PCIe, InfiniBand, RoCE, and RDMA concepts
- checkpointing, elastic recovery, rank failure, and restart behavior

**Build It**

Build the training progression in this order:

1. single-GPU transformer training loop
2. memory profile with activations and optimizer states
3. DDP run on 2 or more GPUs
4. FSDP or ZeRO run on the same model
5. activation checkpointing experiment
6. distributed checkpoint save and restore
7. NCCL debug/profile run

If you only have one GPU locally, use rented GPUs for the distributed step. The artifact matters more than owning the cluster.

**Use It In The Real Stack**

Study:

- PyTorch Distributed for process groups, DDP, FSDP, DTensor, and DeviceMesh
- DeepSpeed for ZeRO and optimizer/memory partitioning
- Megatron-LM for tensor, pipeline, and sequence parallelism patterns
- Ray Train for job orchestration and distributed training ergonomics
- Slurm or Kubernetes for scheduling real GPU jobs

**Measure It**

- samples/sec or tokens/sec per GPU
- step time breakdown
- scaling efficiency
- all-reduce/reduce-scatter time
- GPU idle time
- data-loader stall time
- activation memory
- optimizer-state memory
- checkpoint write and restore time

**Ship It**

Ship a training-systems report comparing single GPU, DDP, and FSDP/ZeRO. Include profiler traces and a clear explanation of which cost dominated each run.

---

**Stage 6: AI Infrastructure And Orchestration**

**Why It Matters**

MLSys does not stop at kernels and frameworks. Real systems need scheduling, deployment, isolation, storage, monitoring, rollout, and recovery.

**Learn**

- GPU scheduling with Slurm, Kubernetes, Ray, or a smaller custom scheduler
- placement constraints: GPU type, memory, topology, MIG, NUMA, and network reachability
- container images, driver compatibility, CUDA runtime compatibility, and reproducible environments
- data pipeline throughput and storage locality
- checkpoint storage, artifact versioning, and restore paths
- autoscaling and admission control
- multi-tenant isolation and quota policies
- metrics, tracing, logging, alerts, and SLOs
- incident debugging: hangs, OOM, bad nodes, slow links, clock throttling, and version skew

**Build It**

Build a small but realistic runbook:

1. define a container image for training and serving
2. run a benchmark job locally or on one node
3. run the same benchmark through a scheduler
4. collect logs, metrics, and profiler traces
5. simulate one failure: OOM, killed process, lost worker, failed checkpoint, or bad config
6. document the recovery path

**Use It In The Real Stack**

Tie the infrastructure to the workload:

- training jobs need checkpoint/restart and efficient data loading
- inference services need health checks, draining, and overload behavior
- multi-GPU jobs need topology-aware placement
- edge systems need thermal, power, and storage constraints in the deployment plan

**Measure It**

- job startup time
- image size and cold-start time
- GPU allocation efficiency
- failed-job recovery time
- checkpoint restore time
- data-loader throughput
- service availability during rollout

**Ship It**

Ship an operations-grade runbook with exact commands, configs, expected metrics, and a failure-mode table.

---

**Stage 7: Compiler And Runtime Layer**

**Why It Matters**

Compiler/runtime work connects model graphs to hardware execution. It is where high-level operations become fused kernels, memory plans, and backend-specific code.

Compiler path:

```text
framework graph -> IR -> graph rewrite -> fused operators -> lowered kernels -> runtime execution
```

**Learn**

- PyTorch graph capture, export, and compile paths
- graph optimization: constant folding, dead-code elimination, layout changes, and operator fusion
- memory planning and buffer reuse
- MLIR dialects and passes
- TVM schedules and auto-tuning concepts
- XLA graph compilation
- TensorRT graph optimization
- Triton language for custom kernels
- correctness testing across rewritten graphs

**Build It**

Pick one:

- fuse two simple tensor ops and measure launch-count reduction
- write a Triton kernel for RMSNorm, softmax, or a small matmul
- lower a toy tensor op through MLIR
- compare TensorRT output against eager PyTorch for a small model fragment
- build a static memory planner for a fixed graph

</details>

### 在真实技术栈中使用

把这个阶段接回硬件：

- 融合减少内存流量与启动开销
- layout 变化会让 kernel 变快或变慢
- 动态 shape 增加 runtime 复杂度
- 量化同时改变图结构与 kernel 选择
- 只有当数值质量和部署约束仍能满足时，编译器的收益才算真实

### 度量它

- 前后算子数量
- kernel 启动次数
- 内存流量
- 延迟与吞吐
- 编译时间
- 峰值内存
- 相对参考实现的数值误差

### 交付它

交付一个编译器/runtime 产物，附带前后对比 benchmark 与正确性测试。

---

## 阶段 8：研究与源码循环

### 为什么重要

在资深 MLSys 层面，论文和生产代码都是工程决策的输入。目标不是收集论文。目标是把论文转化为度量与设计选择。

研究循环：

```text
read -> implement or reproduce -> benchmark -> profile -> compare -> write findings
```

### 读

优先关注系统类会议和偏系统的 ML 工作：

- MLSys
- OSDI
- NSDI
- ASPLOS
- SOSP
- NeurIPS 的系统、效率与基础设施论文

### 研读源码

采用这样的阅读路径：

1. 找出热路径。
2. 找到调度器或 runtime 的边界。
3. 找到内存分配与缓存策略。
4. 找到通信调用。
5. 找到 kernel 启动路径。
6. 复现一个小 benchmark。
7. 改一个设置，度量其影响。

好的源码目标：

| 领域 | 系统 |
|------|---------|
| 推理 | vLLM、SGLang、llama.cpp、TensorRT-LLM、Triton Inference Server |
| 分布式训练 | PyTorch Distributed、DeepSpeed、Megatron-LM、Horovod |
| 基础设施 | Ray、Ray Serve、Ray Train、Kubernetes、Slurm、Kubeflow |
| kernel | FlashAttention、CUTLASS、xFormers、FlashInfer |
| 编译器/runtime | Triton language、TVM、MLIR、XLA、TensorRT |
| 边缘 | Jetson Linux、TensorRT、Holoscan、ONNX Runtime、llama.cpp |

### 交付它

每篇论文或每次源码研读都应产出一个产物：

- 复现
- benchmark
- 实现笔记
- 图
- 性能分析器 trace
- bug 报告
- 小补丁
- 明确的负面结果

---

## 6 个月训练系统里程碑

如果当前强项是推理/runtime 工作，这就是最好的下一个里程碑。保持偏系统。

| 月份 | 关注点 | 产物 |
|-------|-------|----------|
| 1 | Transformer 训练内部机制、autograd、激活值内存 | 带内存 profile 的单 GPU 训练循环 |
| 2 | 混合精度、梯度累积、优化器状态 | 各精度模式下的吞吐与内存报告 |
| 3 | DDP 与 NCCL 基础 | 2-8 GPU DDP benchmark，含 step 时间拆解 |
| 4 | FSDP/ZeRO 与检查点 | 内存扩展性对比与恢复测试 |
| 5 | DeepSpeed 或 Megatron-LM 内部机制 | 针对一个真实模型配置的带注释运行手册 |
| 6 | 自定义优化 | 融合 kernel、调度器改进、检查点改进或通信调优 |

最低可接受的产出：

- 一个 repo
- 一个模型配置
- 三种训练模式
- 性能分析器 trace
- 内存表格
- 扩展性图表
- 书面瓶颈分析

不要止步于“能跑起来”。当你能解释它为什么能扩展、或为什么扩展不上去时，这个里程碑才算完成。

---


<details>
<summary>English original</summary>

**Use It In The Real Stack**

Connect this stage back to hardware:

- fusion reduces memory traffic and launch overhead
- layout changes can make kernels faster or slower
- dynamic shapes increase runtime complexity
- quantization changes both graph structure and kernel selection
- compiler wins are only real if numerical quality and deployment constraints survive

**Measure It**

- operator count before/after
- kernel launch count
- memory traffic
- latency and throughput
- compile time
- peak memory
- numerical error versus reference

**Ship It**

Ship a compiler/runtime artifact with a before/after benchmark and correctness tests.

---

**Stage 8: Research And Source-Code Loop**

**Why It Matters**

At senior MLSys level, papers and production code become inputs to engineering decisions. The goal is not to collect papers. The goal is to turn papers into measurements and design choices.

Research loop:

```text
read -> implement or reproduce -> benchmark -> profile -> compare -> write findings
```

**Read**

Prioritize systems venues and systems-heavy ML work:

- MLSys
- OSDI
- NSDI
- ASPLOS
- SOSP
- NeurIPS systems, efficiency, and infrastructure papers

**Study Source Code**

Use this reading pattern:

1. Identify the hot path.
2. Find the scheduler or runtime boundary.
3. Find memory allocation and cache policy.
4. Find communication calls.
5. Find the kernel launch path.
6. Reproduce a small benchmark.
7. Change one setting and measure the effect.

Good source-code targets:

| Area | Systems |
|------|---------|
| Inference | vLLM, SGLang, llama.cpp, TensorRT-LLM, Triton Inference Server |
| Distributed training | PyTorch Distributed, DeepSpeed, Megatron-LM, Horovod |
| Infrastructure | Ray, Ray Serve, Ray Train, Kubernetes, Slurm, Kubeflow |
| Kernels | FlashAttention, CUTLASS, xFormers, FlashInfer |
| Compiler/runtime | Triton language, TVM, MLIR, XLA, TensorRT |
| Edge | Jetson Linux, TensorRT, Holoscan, ONNX Runtime, llama.cpp |

**Ship It**

Every paper or source-code study should produce one artifact:

- reproduction
- benchmark
- implementation note
- diagram
- profiler trace
- bug report
- small patch
- clear negative result

---

**The 6-Month Training Systems Milestone**

If your current strength is inference/runtime work, this is the best next milestone. Keep it systems-heavy.

| Month | Focus | Artifact |
|-------|-------|----------|
| 1 | transformer training internals, autograd, activation memory | single-GPU training loop with memory profile |
| 2 | mixed precision, gradient accumulation, optimizer states | throughput and memory report across precision modes |
| 3 | DDP and NCCL basics | 2-8 GPU DDP benchmark with step-time breakdown |
| 4 | FSDP/ZeRO and checkpointing | memory scaling comparison and restore test |
| 5 | DeepSpeed or Megatron-LM internals | annotated runbook for one realistic model config |
| 6 | custom optimization | fused kernel, scheduler improvement, checkpoint improvement, or communication tuning |

Minimum acceptable output:

- one repo
- one model config
- three training modes
- profiler traces
- memory tables
- scaling chart
- written bottleneck analysis

Do not stop at "it runs." The milestone is complete when you can explain why it scales or fails to scale.

---

</details>

## 精选专题课程

- [**AI Inference Engineer 2026**](01-AI推理工程师2026/README.md) — 现代生产推理栈的四部分深度剖析：
  - **第 1 部分 — 基础**（5 讲）：心智模型、transformer 执行、roofline（性能上界模型）、FP16→FP8→FP4→INT4 精度栈、runtime 全景
  - **第 2 部分 — Hopper 上的 Dense**（7 讲）：Llama 3.3 70B ↔ Qwen 2.5 72B 在 H100/H200 上并排对比、量化 recipe（含 LLaMA-3-70B W8A8 异常）、张量并行、现代推理服务栈、128K 上下文，以及分布式通信层
  - **第 3 部分 — Blackwell 上的 MoE**（5 讲）：DeepSeek V3.1 + Qwen3-MoE 235B-A22B 在 B200/GB200 NVL72 上、MLA + MTP、EP all-to-all、prefill（首字前的整段计算）/decode（逐 token 生成阶段）分离、完整的 `$/MTok` 成本模型
  - **第 4 部分 — 优化一个真实引擎**（10 讲）：一个**实测案例研究**。[SparkInfer-K3](https://github.com/gittensor-ai-lab/sparkinfer-k3) 在单台 8× H200 节点上运行 Kimi K3（2.8T、896 experts、混合 MLA + KDA），经约 96 个 PR，在 128k 下 decode 从 **1.01 → 60.17 tok/s**，而同一台机器上的 llama.cpp 为 18.44。构建一个无法被作弊的记分板；诊断四大约束中哪一个在起决定作用；launch geometry、融合 + 激活值量化、split-context/split-head attention、专家分片与 Amdahl 陷阱、CUDA-graph 常驻；被遗忘的阶段（prefill，六周内每 token 一次前向）；以及 bug 让 benchmark *变好* 的那种失效模式。

锚定于最新的开放权重模型与当前 runtime 版本，并附一份 [refresh log](01-AI推理工程师2026/REFRESH-LOG.md) 追踪每一次版本固定。


<details>
<summary>English original</summary>

**Featured Special Course**

- [**AI Inference Engineer 2026**](01-AI推理工程师2026/README.md) — four-part deep dive on the modern production inference stack:
  - **Part 1 — Fundamentals** (5 lectures): the mental model, transformer execution, roofline, the FP16→FP8→FP4→INT4 precision stack, the runtime landscape
  - **Part 2 — Dense at Hopper** (7 lectures): Llama 3.3 70B ↔ Qwen 2.5 72B side-by-side on H100/H200, quantization recipes including the LLaMA-3-70B W8A8 anomaly, tensor parallelism, the modern serving stack, 128K context, and the distributed communication layer
  - **Part 3 — MoE at Blackwell** (5 lectures): DeepSeek V3.1 + Qwen3-MoE 235B-A22B on B200/GB200 NVL72, MLA + MTP, EP all-to-all, disaggregated prefill/decode, full `$/MTok` cost model
  - **Part 4 — Optimizing a Real Engine** (10 lectures): a **measured case study**. [SparkInfer-K3](https://github.com/gittensor-ai-lab/sparkinfer-k3) running Kimi K3 (2.8T, 896 experts, hybrid MLA + KDA) on one 8× H200 node, **1.01 → 60.17 tok/s** decode at 128k across ~96 PRs, against llama.cpp at 18.44 on the same box. Building a scoreboard that cannot be gamed; diagnosing which of the four ceilings binds; launch geometry, fusion + activation quantization, split-context/split-head attention, expert sharding and the Amdahl trap, CUDA-graph residency; the forgotten phase (prefill, one forward per token for six weeks); and the failure mode where a bug makes the benchmark *better*.

Anchored on the most recent open-weights models and current runtime versions, with a [refresh log](01-AI推理工程师2026/REFRESH-LOG.md) that tracks every version pin.

</details>

- [**MLSys Deep Dives**](05-MLSys深度探索/README.md) —— 7 讲、面向资深工程师的巡览，把 **2026 年机器学习系统全景** 讲成一个彼此关联、成本驱动的系统：推理经济学（tokens/s、TOK/$、perf/watt）；kernel 语言的爆发（Triton、CUTLASS/cuTile、ThunderKittens、TileLang）；编译器与 runtime（TVM、Mojo/MAX、TensorRT-LLM、TileRT megakernel runtime）；后 Transformer 架构（Mamba/SSMs 与混合浪潮 —— Nemotron-H、Jamba、Falcon-H1、MiniMax）；把 2026 前沿模型当作系统产物来读（Qwen3、Llama Nemotron Ultra 253B、Xiaomi MiMo、DeepSeek V3/R1、MoE（混合专家模型）+ MLA（多头潜在注意力）+ MTP（多 token 预测））；推理加速（投机解码 —— EAGLE-3、DFlash —— 与 Flash kernel，以及 Together AI 的研究脉络）；以及边缘 / 物理 AI 前沿（1000-TOPS 硬件 vs 1000-tok/s 里程碑，外加一个实测的优化阶梯压轴项目）。

- [**GLM-5.3-Flash 架构精研**](02-GLM-5-3-Flash架构精通/README.md) —— 12 模块的主任工程师深度钻研，聚焦一个混合检查点（总参数约 320B / 激活约 18B，4,096 hidden，45 层：34 个 KDA + 11 个稀疏 MLA/DSA，42 个 MoE 层、288 个专家，4 流 mHC），面向 8× RTX 5090 NVFP4 部署。每个机制都从其更新方程推导，而不是从架构图看：MoE router 的“选择”与“加权”分离，以及带 clamp 的 SwiGLU；**KDA delta-rule 递推** `S_t = (I − βkkᵀ)D·S_{t−1} + βkvᵀ` 手工推演，外加使其可 chunk 并行的仿射复合技巧；**MLA 的吸收代数**（`q̃ = (Uᴷ)ᵀq`）如何支撑其每 token 64× 的 cache 缩减；**DSA 的池化 indexer** 及其 2,051 个 slot 的预算，以及池边界处的因果性 bug；**mHC 的** 双随机残差混合（四条流 ≠ 四个 attention 模块，45 层 ≠ 45/4 的有效深度）；为什么投机解码（decode，逐 token 生成阶段）的 rollback 必须恢复循环*和*卷积状态，而不只是截断 KV cache；会打破朴素“除以 8”显存预算的张量并行 **MLA 复制陷阱**；以及围绕同一条不变量构建的完整正确性矩阵 —— 一次 prefill（首字前的整段计算）、chunked prefill 与带 cache 的续算，必须达到等价的状态和 logits。压轴项目：从手工推导的单个 KDA head，到一次经过验证、受正确性门控的优化，共八级的构建阶梯。


<details>
<summary>English original</summary>

- [**MLSys Deep Dives**](05-MLSys深度探索/README.md) — a 7-lecture senior-engineer tour of the **2026 machine-learning-systems landscape** as one connected, cost-driven system: the economics of inference (tokens/s, TOK/$, perf/watt); the kernel-language explosion (Triton, CUTLASS/cuTile, ThunderKittens, TileLang); compilers and runtimes (TVM, Mojo/MAX, TensorRT-LLM, the TileRT megakernel runtime); post-transformer architectures (Mamba/SSMs and the hybrid wave — Nemotron-H, Jamba, Falcon-H1, MiniMax); the 2026 frontier models read as systems artifacts (Qwen3, Llama Nemotron Ultra 253B, Xiaomi MiMo, DeepSeek V3/R1, MoE + MLA + MTP); inference acceleration (speculative decoding — EAGLE-3, DFlash — and Flash kernels, with Together AI's research line); and the edge / physical-AI frontier (1000-TOPS hardware vs the 1000-tok/s milestone, plus a measured optimization-ladder capstone).

- [**GLM-5.3-Flash Architecture Mastery**](02-GLM-5-3-Flash架构精通/README.md) — a 12-module principal-engineer deep dive on one hybrid checkpoint (~320B total / ~18B active, 4,096 hidden, 45 layers: 34 KDA + 11 sparse MLA/DSA, 42 MoE layers of 288 experts, 4-stream mHC), targeting an 8× RTX 5090 NVFP4 deployment. Derives every mechanism from its update equation rather than its diagram: the MoE router's selection-vs-weighting split and clamped SwiGLU; the **KDA delta-rule recurrence** `S_t = (I − βkkᵀ)D·S_{t−1} + βkvᵀ` worked by hand, plus the affine-composition trick that makes it chunk-parallelizable; **MLA's absorption algebra** (`q̃ = (Uᴷ)ᵀq`) behind its 64× per-token cache reduction; **DSA's pooled indexer** and its 2,051-slot budget with causality bugs at pool boundaries; **mHC's** doubly-stochastic residual mixing (four streams ≠ four attention modules, 45 layers ≠ 45/4 effective depth); why speculative-decode rollback must restore recurrent *and* convolution state, not just truncate a KV cache; the tensor-parallel **MLA replication trap** that breaks a naive "divide by 8" memory budget; and a full correctness matrix built around one invariant — one prefill, chunked prefill, and cached continuation must reach equivalent state and logits. Capstone: an eight-stage build ladder from one hand-derived KDA head to one validated, correctness-gated optimization.

</details>

- [**Hardware-Aware LLM Quantization & Inference Optimization**](03-硬件感知LLM量化/README.md) — 一门 12 模块 + capstone 的研究-工程课程，回答一个问题：**该从哪些 tensor 里、用哪种方法去掉哪些 bit，才能在不改变模型行为的前提下拿到真实的 tok/s？** 面向 NVIDIA Blackwell / RTX 5090（`sm_120`），带一个实测的约 27 B NVFP4 + MTP 案例研究。推理物理与 `tok/s ≈ BW_eff / B_token` 上限；NVFP4 数学（E2M1 + block-16 + E4M3 scale = **4.5** bit/权重，而不是 4）；`sm_120` 原生加速什么，以及为什么 3-bit *比* 4-bit *更慢*；把常驻字节与流式字节分开的逐 token 字节账本；按误差模式选择的校准（AWQ / GPTQ / SmoothQuant）；激活值离群点与 attention sink；**softmax 放大**证明——Q/K 误差会被放大，而 MLP/O 误差随 `η/√K` 平均下去；用 KL 做行为评分，以及恒等式 **acceptance rate = 1 − TV(p, q)**；KV/权重流量交叉点与 262 K 上下文可行性；**接受税**（一个实测案例：字节收益的 84 % 被以接受率损失的形式还了回去）；以及把精度分配当作多重选择背包问题——它仅靠重新分配，就能找到**同样吞吐下不到一半的行为损伤**。

- [**TVM Deep Dives**](06-TVM深度探索/README.md) — 一门 5 讲的资深工程师课程，讲 Apache TVM 编译器栈：编译流程（Relax、TensorIR、统一的 `IRModule`）、手工调度 kernel、自动调优（AutoTVM → Ansor → MetaSchedule）、Relax 图优化（动态 shape、融合、面向自定义加速器的 BYOC），以及交付（runtime、microTVM、MLC-LLM）。把模型直接编译到硬件，调优它，并击败框架基线——用数字证明。

---


<details>
<summary>English original</summary>

- [**Hardware-Aware LLM Quantization & Inference Optimization**](03-硬件感知LLM量化/README.md) — a 12-module + capstone research-engineering course answering one question: **which bits should I remove, from which tensors, using which method, to gain real tok/s without changing model behavior?** Targets NVIDIA Blackwell / RTX 5090 (`sm_120`) with a measured ~27 B NVFP4 + MTP case study. Inference physics and the `tok/s ≈ BW_eff / B_token` ceiling; NVFP4 mathematics (E2M1 + block-16 + E4M3 scale = **4.5** bits/weight, not 4); what `sm_120` accelerates natively and why 3-bit is *slower* than 4-bit; the per-token byte ledger that separates resident bytes from streamed bytes; calibration chosen by error mode (AWQ / GPTQ / SmoothQuant); activation outliers and attention sinks; the **softmax-amplification** proof that Q/K error amplifies while MLP/O error averages down as `η/√K`; behavior grading via KL and the identity **acceptance rate = 1 − TV(p, q)**; KV/weight traffic crossover and 262 K-context feasibility; the **acceptance tax** (a measured case where 84 % of a byte win was paid back as lost acceptance); and precision allocation as a multiple-choice knapsack — which finds **the same throughput at less than half the behavioral damage**, by reallocation alone.

- [**TVM Deep Dives**](06-TVM深度探索/README.md) — a 5-lecture senior-engineer course on the Apache TVM compiler stack: the compilation flow (Relax, TensorIR, the unified `IRModule`), hand-scheduling kernels, auto-tuning (AutoTVM → Ansor → MetaSchedule), Relax graph optimization (dynamic shapes, fusion, BYOC for custom accelerators), and shipping (runtime, microTVM, MLC-LLM). Compile a model to the metal, tune it, and beat the framework baseline — with the numbers to prove it.

---

</details>

## 边缘 MLSys 专精方向

对本路线图而言，最强的细分方向是 **边缘 MLSys + 推理 Runtime 工程**。

它结合了：

- Jetson 和嵌入式 Linux
- 本地 AI 与机器人推理
- 低功耗部署
- 内存高效的推理服务
- 多模态 runtime 工作
- 调度器设计
- CUDA/TensorRT 优化
- Rust/C++ runtime 工程
- MLIR/Triton 编译器路径
- 受限设备上的可观测性

优秀的边缘 MLSys 项目：

- 带连续批处理和 KV cache 核算的 Jetson 大语言模型推理服务 runtime
- 低内存 LoRA 或适配器训练实验
- 带检查点恢复的边缘模型适配流水线
- 面向摄像头、音频和文本工作负载的多模态调度器
- 带可观测性和过载控制的本地/私有 AI 设备 runtime
- Orin 与桌面 GPU 上的 CUDA kernel 优化报告
- 在功耗限制下改变批/并发度的热感知推理调度器

边缘细分方向很有价值，因为它把通常分散在不同工程师身上的技能结合起来：嵌入式 Linux、GPU 优化、AI 推理、runtime 工程和生产部署。

---

## 顶点项目选项

选择一个。好的顶点项目要足够聚焦以便完成，又足够深入以证明系统能力。

### 选项 A：边缘推理 Runtime

构建一个面向 Jetson 的 runtime，包含：

- tokenizer 路径
- 请求调度器
- 连续批处理
- KV cache 核算
- 流式输出
- 指标端点
- Nsight profile
- 功耗与热管理说明

成功标准：

- 可复现的 benchmark
- p50/p95 首 token 时延和 token 间延迟
- 至少三个并发级别下的 tokens/sec
- 权重、KV cache 和 workspace 的内存报告
- 记录过载行为

### 选项 B：分布式训练系统报告

构建一个训练 benchmark 套件，包含：

- 单 GPU 基线
- DDP 运行
- FSDP 或 ZeRO 运行
- 激活值检查点实验
- 检查点恢复测试
- 性能分析器 trace

成功标准：

- 每 GPU 的 tokens/sec 或 samples/sec
- step 耗时拆解
- 扩展效率
- 内存对比
- 通信成本分析
- 一条具体调优建议

### 选项 C：编译器/Kernel Runtime 演示

构建一个模型片段优化，包含：

- 参考 PyTorch 实现
- 自定义 CUDA 或 Triton kernel
- 图融合或 lowering 路径
- 正确性测试
- benchmark harness

成功标准：

- 优化前后的延迟
- kernel 启动次数
- 内存流量估算
- 数值误差表
- 说明该优化何时不再有帮助

---

## 作品集标准

一份强的 MLSys 作品集产物包含：

- 架构图
- 确切的硬件和软件版本
- 可复现的环境搭建命令
- benchmark harness
- 性能分析器 trace
- 原始结果文件
- 汇总图表
- 瓶颈分析
- 失效模式
- 下一步优化假设

弱的产物：

```text
I used vLLM and it was faster.
```

强的产物：

```text
Continuous batching improved throughput from 410 to 690 tok/s at 64 concurrent
requests, but p95 TTFT increased from 380 ms to 610 ms. The profiler shows
prefill bursts starving decode, so the next experiment limits prefill tokens per
scheduling iteration.
```

---

## 职业定位

这条路线支持的头衔包括：

- ML 系统工程师
- AI 基础设施工程师
- 推理系统工程师
- 训练系统工程师
- GPU Runtime 工程师
- 边缘 AI Runtime 工程师
- 大语言模型 Runtime 优化工程师

强定位：

```text
ML Systems Engineer | GPU Runtime Optimization | CUDA | TensorRT-LLM |
Distributed Inference | Edge AI Infrastructure | Jetson | MLIR | C++ | Rust
```

或：

```text
Inference Systems Engineer | LLM Runtime Optimization | CUDA Kernels |
Tensor Parallelism | Edge AI | Jetson | TensorRT-LLM | ML Systems
```

头衔不如公开证明重要。发布 benchmark 图、延迟剖析、内存报告、架构图、性能分析器 trace 和小型 runtime 组件。

---

## 官方参考

优先使用官方文档和一手来源：

- [PyTorch Distributed 概述](https://docs.pytorch.org/tutorials/beginner/dist_overview.html)
- [vLLM 文档](https://docs.vllm.ai/)
- [NVIDIA TensorRT-LLM 文档](https://docs.nvidia.com/tensorrt-llm/)
- [NVIDIA CUDA C++ 最佳实践指南](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)
- [NVIDIA NCCL 文档](https://docs.nvidia.com/deeplearning/nccl/)
- [DeepSpeed ZeRO 文档](https://deepspeed.readthedocs.io/en/stable/zero3.html)
- [Ray Train 文档](https://docs.ray.io/en/latest/train/overview.html)
- [NVIDIA Megatron-LM](https://github.com/NVIDIA/Megatron-LM)
- [Triton 语言文档](https://triton-lang.org/main/index.html)
- [MLIR 文档](https://mlir.llvm.org/docs/)
- [Apache TVM 文档](https://tvm.apache.org/docs/)
- [Triton Inference Server 文档](https://docs.nvidia.com/deeplearning/triton-inference-server/)

---


<details>
<summary>English original</summary>

**Edge MLSys Specialization**

For this roadmap, the strongest niche is **Edge MLSys + Inference Runtime Engineering**.

This combines:

- Jetson and embedded Linux
- local AI and robotics inference
- low-power deployment
- memory-efficient serving
- multimodal runtime work
- scheduler design
- CUDA/TensorRT optimization
- Rust/C++ runtime engineering
- MLIR/Triton compiler paths
- observability on constrained devices

Good edge MLSys projects:

- Jetson LLM serving runtime with continuous batching and KV-cache accounting
- low-memory LoRA or adapter-training experiment
- edge model adaptation pipeline with checkpoint recovery
- multimodal scheduler for camera, audio, and text workloads
- local/private AI appliance runtime with observability and overload control
- CUDA kernel optimization report on Orin versus desktop GPU
- thermal-aware inference scheduler that changes batch/concurrency under power limits

The edge niche is valuable because it joins skills that are usually split across different engineers: embedded Linux, GPU optimization, AI inference, runtime engineering, and production deployment.

---

**Capstone Options**

Choose one. A good capstone is narrow enough to finish and deep enough to prove systems ability.

**Option A: Edge Inference Runtime**

Build a Jetson-focused runtime with:

- tokenizer path
- request scheduler
- continuous batching
- KV-cache accounting
- streaming output
- metrics endpoint
- Nsight profile
- power and thermal notes

Success criteria:

- reproducible benchmark
- p50/p95 TTFT and inter-token latency
- tokens/sec under at least three concurrency levels
- memory report for weights, KV cache, and workspace
- overload behavior documented

**Option B: Distributed Training Systems Report**

Build a training benchmark suite with:

- single-GPU baseline
- DDP run
- FSDP or ZeRO run
- activation checkpointing experiment
- checkpoint restore test
- profiler traces

Success criteria:

- tokens/sec or samples/sec per GPU
- step-time breakdown
- scaling efficiency
- memory comparison
- communication-cost analysis
- one concrete tuning recommendation

**Option C: Compiler/Kernel Runtime Demo**

Build a model-fragment optimization with:

- reference PyTorch implementation
- custom CUDA or Triton kernel
- graph fusion or lowering path
- correctness tests
- benchmark harness

Success criteria:

- before/after latency
- kernel launch count
- memory traffic estimate
- numerical error table
- explanation of when the optimization stops helping

---

**Portfolio Standard**

A strong MLSys portfolio artifact includes:

- architecture diagram
- exact hardware and software versions
- reproducible setup commands
- benchmark harness
- profiler traces
- raw result files
- summary charts
- bottleneck analysis
- failure modes
- next optimization hypothesis

Weak artifact:

```text
I used vLLM and it was faster.
```

Strong artifact:

```text
Continuous batching improved throughput from 410 to 690 tok/s at 64 concurrent
requests, but p95 TTFT increased from 380 ms to 610 ms. The profiler shows
prefill bursts starving decode, so the next experiment limits prefill tokens per
scheduling iteration.
```

---

**Career Positioning**

This track supports titles like:

- ML Systems Engineer
- AI Infrastructure Engineer
- Inference Systems Engineer
- Training Systems Engineer
- GPU Runtime Engineer
- Edge AI Runtime Engineer
- LLM Runtime Optimization Engineer

Strong positioning:

```text
ML Systems Engineer | GPU Runtime Optimization | CUDA | TensorRT-LLM |
Distributed Inference | Edge AI Infrastructure | Jetson | MLIR | C++ | Rust
```

or:

```text
Inference Systems Engineer | LLM Runtime Optimization | CUDA Kernels |
Tensor Parallelism | Edge AI | Jetson | TensorRT-LLM | ML Systems
```

The title matters less than public proof. Publish benchmark graphs, latency profiles, memory reports, architecture diagrams, profiler traces, and small runtime components.

---

**Official References**

Use official docs and primary sources first:

- [PyTorch Distributed Overview](https://docs.pytorch.org/tutorials/beginner/dist_overview.html)
- [vLLM Documentation](https://docs.vllm.ai/)
- [NVIDIA TensorRT-LLM Documentation](https://docs.nvidia.com/tensorrt-llm/)
- [NVIDIA CUDA C++ Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)
- [NVIDIA NCCL Documentation](https://docs.nvidia.com/deeplearning/nccl/)
- [DeepSpeed ZeRO Documentation](https://deepspeed.readthedocs.io/en/stable/zero3.html)
- [Ray Train Documentation](https://docs.ray.io/en/latest/train/overview.html)
- [NVIDIA Megatron-LM](https://github.com/NVIDIA/Megatron-LM)
- [Triton Language Documentation](https://triton-lang.org/main/index.html)
- [MLIR Documentation](https://mlir.llvm.org/docs/)
- [Apache TVM Documentation](https://tvm.apache.org/docs/)
- [Triton Inference Server Documentation](https://docs.nvidia.com/deeplearning/triton-inference-server/)

---

</details>

## 达成标准

当你能做到以下各点时，即可声称具备 MLSys 能力：

- 把 transformer 训练与推理解释为 shape、memory、kernel 与通信流
- 在提出优化方案之前先对工作负载做 profile
- 编写并 benchmark 至少一个自定义 GPU kernel
- 调试受内存、通信或同步限制的分布式训练任务
- 解释连续批处理与 paged KV cache 如何影响推理服务吞吐与尾延迟
- 把 runtime 决策与硬件约束关联起来
- 用日志、指标与恢复步骤运维一个小型训练或推理系统
- 交付一个可复现的 benchmark 产物

结果不是一张证书，而是一批系统性的工作，证明你能让 AI 工作负载跑得更快、更便宜、更可靠。


<details>
<summary>English original</summary>

**Exit Criteria**

You are ready to claim MLSys competency when you can:

- explain transformer training and inference as shape, memory, kernel, and communication flows
- profile a workload before proposing an optimization
- write and benchmark at least one custom GPU kernel
- debug a distributed training run limited by memory, communication, or synchronization
- explain how continuous batching and paged KV cache affect serving throughput and tail latency
- connect runtime decisions to hardware constraints
- operate a small training or inference system with logs, metrics, and recovery steps
- ship a reproducible benchmark artifact

The outcome is not a certificate. It is a body of systems work that proves you can make AI workloads run faster, cheaper, and more reliably.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
