---
title: 分布式 AI 互连：vLLM、PyTorch、UCX 与 UCC
description: 分布式 AI 互连：vLLM、PyTorch、UCX 与 UCC
---

# 分布式 AI 互连：vLLM、PyTorch、UCX 与 UCC

<div class="course-identity auto-course" style="--course-accent: #ca8a04; --course-accent-rgb: 202, 138, 4;" markdown="1">
<div class="course-identity__icon">DAIV</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 专业化</p>
<p class="course-identity__title">Distributed AI Interconnects: vLLM, PyTorch, UCX, and UCC 的专业化课程标识。</p>
<p class="course-identity__meta">产物：专业化案例研究 · 度量：性能、可靠性、角色匹配</p>
</div>
</div>


**阶段：** 5 - 高级主题与专业化  
**方向：** GPU Infrastructure  
**级别：** 高级基础设施 / 分布式推理系统

---

## 为什么有这门课程

现代 AI 推理服务不再仅限于：

```text
one model
one GPU
one process
```

大型推理系统往往变成：

```text
vLLM
  -> PyTorch distributed process groups
  -> collective communication backend
  -> NCCL / RCCL / UCC
  -> UCX / InfiniBand / RoCE / TCP / shared memory / NVLink
  -> physical topology
```

难点不只是启动一个模型服务器。

难点在于搞清楚：当物理拓扑与逻辑分布式拓扑不一致时会发生什么。

本课程聚焦一个实际问题：

> 如果有三个节点按 `A <-> B <-> C` 排列，而 `A` 无法直接到达 `C`，那么 vLLM、PyTorch、UCX 和 UCC 实际上会做什么？

简短回答：

> vLLM 和 PyTorch 不会魔法般地绕过损坏或不可路由的 fabric。它们创建逻辑进程组并调用通信操作。所选后端与底层网络必须提供所需的可达性。UCC 可以提供集合通信抽象并选择算法/传输层，但它不是针对不可达拓扑的魔法应用层路由器。

---

## 学习目标

完成本课程后，你应该能够：

1. 解释 vLLM 编排、PyTorch 进程组、集合通信库、传输层与物理 fabric 之间的区别。
2. 理解为什么张量并行推理比流水线并行推理需要强得多的通信能力。
3. 解释当分布式作业假定全 rank 可达、而真实网络只是一条链时会发生什么。
4. 区分 NCCL/RCCL、UCX 和 UCC。
5. 设计拓扑感知的 vLLM 部署方案。
6. 利用日志、拓扑检查和小型集合通信测试来调试分布式推理挂起。
7. 判断何时使用流水线并行、张量并行、数据并行或独立副本。
8. 为定制或非标准互连制定研究计划。

---

## 1. 栈的心智模型

先使用这张图。

```text
Application
  vLLM API server / offline engine

Distributed execution runtime
  Ray or multiprocessing

Framework communication layer
  PyTorch torch.distributed ProcessGroup

Collective / communication backend
  NCCL, RCCL, Gloo, UCC, MPI, XCCL

Transport / fabric abstraction
  UCX/UCP, InfiniBand verbs, RoCE, TCP, shared memory, CUDA IPC, ROCm IPC

Physical links
  NVLink, NVSwitch, PCIe, NICs, cables, switches, subnets
```

每一层都有不同的职责。

| 层 | 职责 | 不负责什么 |
|---|---|---|
| vLLM | 以批处理、KV cache、张量/流水线/数据并行执行来为 LLM 提供推理服务 | 修复损坏的网络可达性 |
| Ray | 放置 worker 并管理分布式 Python 任务 | 保证高性能 GPU 集合通信 |
| PyTorch distributed | 创建 rank、进程组并调用集合通信 | 自动发明 fabric 布线 |
| NCCL/RCCL | 面向 NVIDIA/AMD 类加速器栈的 GPU 集合通信 | 让断开的对端可达 |
| UCC | 带多种传输层的统一集合通信 API | 取代拓扑设计 |
| UCX | 面向 RDMA、TCP、共享内存、GPU 内存的传输抽象 | 决定应用的并行策略 |
| Fabric | 设备之间真实的数据包/事务 | 理解你的模型图 |

---


<details>
<summary>English original</summary>

**Distributed AI Interconnects: vLLM, PyTorch, UCX, and UCC**

<div class="course-identity auto-course" style="--course-accent: #ca8a04; --course-accent-rgb: 202, 138, 4;" markdown="1">
<div class="course-identity__icon">DAIV</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for Distributed AI Interconnects: vLLM, PyTorch, UCX, and UCC.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Phase:** 5 - Advanced Topics and Specialization  
**Track:** GPU Infrastructure  
**Level:** Advanced infrastructure / distributed inference systems

---

**Why this course exists**

Modern AI serving is no longer only:

```text
one model
one GPU
one process
```

Large inference systems often become:

```text
vLLM
  -> PyTorch distributed process groups
  -> collective communication backend
  -> NCCL / RCCL / UCC
  -> UCX / InfiniBand / RoCE / TCP / shared memory / NVLink
  -> physical topology
```

The hard part is not just starting a model server.

The hard part is knowing what happens when the physical topology does not match the logical distributed topology.

This course focuses on a practical question:

> If we have three nodes arranged like `A <-> B <-> C`, and `A` cannot reach `C` directly, what do vLLM, PyTorch, UCX, and UCC actually do?

Short answer:

> vLLM and PyTorch do not magically route around a broken or non-routable fabric. They create logical process groups and invoke communication operations. The selected backend and the underlying network must provide the required reachability. UCC can provide a collective abstraction and choose algorithms/transports, but it is not a magic application-layer router for an unreachable topology.

---

**Learning objectives**

By the end of this course, you should be able to:

1. Explain the difference between vLLM orchestration, PyTorch process groups, collective libraries, transport layers, and physical fabric.
2. Understand why tensor parallel inference needs much stronger communication than pipeline parallel inference.
3. Explain what happens when a distributed job assumes full rank reachability but the real network is only a chain.
4. Distinguish NCCL/RCCL, UCX, and UCC.
5. Design a topology-aware vLLM deployment plan.
6. Debug distributed inference hangs using logs, topology checks, and small collective tests.
7. Decide when to use pipeline parallelism, tensor parallelism, data parallelism, or separate replicas.
8. Build a research plan for custom or non-standard interconnects.

---

**1. Stack mental model**

Use this map first.

```text
Application
  vLLM API server / offline engine

Distributed execution runtime
  Ray or multiprocessing

Framework communication layer
  PyTorch torch.distributed ProcessGroup

Collective / communication backend
  NCCL, RCCL, Gloo, UCC, MPI, XCCL

Transport / fabric abstraction
  UCX/UCP, InfiniBand verbs, RoCE, TCP, shared memory, CUDA IPC, ROCm IPC

Physical links
  NVLink, NVSwitch, PCIe, NICs, cables, switches, subnets
```

Each layer has a different job.

| Layer | Responsibility | What It Does Not Do |
|---|---|---|
| vLLM | Serve LLMs with batching, KV cache, tensor/pipeline/data parallel execution | Fix broken network reachability |
| Ray | Place workers and manage distributed Python tasks | Guarantee high-performance GPU collectives |
| PyTorch distributed | Create ranks, process groups, and call collectives | Automatically invent fabric routing |
| NCCL/RCCL | GPU collectives for NVIDIA/AMD-like accelerator stacks | Make disconnected peers reachable |
| UCC | Unified collective API with multiple transport layers | Replace topology design |
| UCX | Transport abstraction for RDMA, TCP, shared memory, GPU memory | Decide application parallelism strategy |
| Fabric | Actual packets/transactions between devices | Understand your model graph |

---

</details>

## 2. vLLM 用分布式通信做什么

vLLM 支持使用张量并行和流水线并行的分布式推理。

重要的区别在于：

```text
Tensor parallelism:
  split one layer across devices
  frequent collectives inside model execution

Pipeline parallelism:
  split layers across stages
  activation traffic mostly between adjacent stages
```

对于张量并行，vLLM 会调用如下通信操作：

```text
all-reduce
all-gather
reduce-scatter
gather
send / recv
```

这意味着张量并行组中的所有 rank 都必须通过所选后端正确通信。

对于流水线并行，模型的 layer 被划分成多个 stage：

```text
node A: layers 0-15
node B: layers 16-31
node C: layers 32-47
```

前向路径天然是链式的：

```text
A -> B -> C
```

这就是为什么流水线并行往往更容易适配弱互连或非均匀互连。

但有一个警告：

> 即使数据搬运大多发生在相邻节点之间，runtime 仍可能需要全局控制面通信，用于启动、rendezvous、健康检查、元数据、barrier 以及 process group 的建立

所以，“流水线并行适合链式拓扑”并不意味着“集群中端点 rank 之间可以完全没有路由”。

---

## 3. `A <-> B <-> C` 拓扑问题

假设：

```text
A can reach B
B can reach A and C
C can reach B
A cannot reach C directly
C cannot reach A directly
```

共有三种不同的情况。

### 情况 1：通过 B 存在 IP 路由

如果 `A` 能在网络层通过 `B` 路由到 `C`，那么从应用角度看，`A` 和 `C` 就是可达的。

这条路径是间接的，但可达：

```text
A -> B -> C
```

这在功能上可能可用。

但性能可能很差，因为所有端点流量都要争抢中间的节点/链路。

预期症状：

- 高延迟
- all-reduce 带宽降低
- B 成为瓶颈
- 集合通信比拓扑图所暗示的更慢
- 负载下尾延迟升高

### 情况 2：从 A 到 C 不存在 IP/RDMA 路由

如果 `A` 确实无法寻址或连接到 `C`，大多数分布式框架会在初始化或第一次集合通信时失败或挂起。

典型故障点：

- Ray 无法组成健康的集群
- PyTorch `init_process_group()` 无法完成
- NCCL/RCCL communicator 初始化挂起或报错
- 第一次 all-reduce 或 all-gather 卡住
- UCX 端点建立失败

关键答案是：

> 这不是 vLLM 在模型层能解决的问题

vLLM 假定分布式 runtime 和通信后端能够连通所需的 rank。

### 情况 3：自定义应用有意只使用相邻通信

如果构建的 runtime 只发送：

```text
A -> B
B -> C
```

而从不要求：

```text
A <-> C
```

那么线型拓扑可以工作。

但这需要谨慎设计：

- 不能有跨全部三个 rank 的全局 all-reduce
- 不能有跨全部三个 rank 的 all-gather
- 不能有跨全部三个 rank 的 all-to-all
- 不能使用假定任意两两可达的 process group 后端
- 需要显式路由或分阶段转发逻辑

这是一个自定义分布式系统，而不是普通的“把 tensor_parallel_size=3 一设就行”的部署。

---

## 4. 直接答案：这是 UCC 胶水层的问题吗？

通常不是。

UCC 是集合通信的 API 和库。

它可以选择集合通信算法和传输层，并且可以视构建和 runtime 而定，使用 UCX/UCP、NCCL、SHARP、CUDA 或 HIP 等组件。

但 UCC 不能替代 fabric 的可达性。

可以这样理解 UCC：

```text
"I need all-reduce across these ranks.
Which collective implementation should I use?"
```

而不是：

```text
"A and C cannot talk.
Please invent a safe, high-performance routed network through B."
```

如果底层传输能够路由流量，UCC 就可能用上它。

如果底层传输无法在所需 rank 之间建立端点或搬运数据，UCC 本身无法让任务变得正确。

---

## 5. UCX 与 UCC

这两个名字很容易混淆。

| 组件 | 简要含义 | 使用示例 |
|---|---|---|
| UCX | 传输框架 | 在 InfiniBand、RoCE、TCP、共享内存、CUDA、ROCm 上搬运数据 |
| UCC | 集合通信库 | 通过所选传输层实现 all-reduce、广播、all-gather、all-to-all |

UCX 提供通信原语和传输选择。

UCC 提供集合通信操作。

关系：

```text
UCC collective
  -> may use UCP transport layer
  -> UCP comes from UCX
  -> UCX uses RDMA/TCP/shared memory/GPU transports
```

因此：

```text
UCX = how bytes move
UCC = how collective algorithms are expressed and selected
```

---


<details>
<summary>English original</summary>

**2. What vLLM uses distributed communication for**

vLLM supports distributed inference using tensor parallelism and pipeline parallelism.

The important difference:

```text
Tensor parallelism:
  split one layer across devices
  frequent collectives inside model execution

Pipeline parallelism:
  split layers across stages
  activation traffic mostly between adjacent stages
```

For tensor parallelism, vLLM calls communication operations such as:

```text
all-reduce
all-gather
reduce-scatter
gather
send / recv
```

That means all ranks in the tensor-parallel group must communicate correctly through the selected backend.

For pipeline parallelism, the model layers are partitioned into stages:

```text
node A: layers 0-15
node B: layers 16-31
node C: layers 32-47
```

The forward path is naturally chain-like:

```text
A -> B -> C
```

That is why pipeline parallelism is often easier to fit onto weak or non-uniform interconnects.

But there is a warning:

> even if data movement is mostly adjacent, the runtime may still need global control-plane communication for startup, rendezvous, health checks, metadata, barriers, and process-group setup

So "pipeline parallelism fits a chain" does not mean "the cluster can have no route between endpoint ranks at all."

---

**3. The `A <-> B <-> C` topology problem**

Assume:

```text
A can reach B
B can reach A and C
C can reach B
A cannot reach C directly
C cannot reach A directly
```

There are three different cases.

**Case 1: IP routing exists through B**

If `A` can route to `C` through `B` at the network layer, then from the application perspective `A` and `C` are reachable.

The path is indirect, but reachable:

```text
A -> B -> C
```

This may work functionally.

But performance may be poor because all endpoint traffic competes for the middle node/link.

Expected symptoms:

- high latency
- lower all-reduce bandwidth
- B becomes a bottleneck
- collectives are slower than topology diagrams suggest
- tail latency increases under load

**Case 2: no IP/RDMA route exists from A to C**

If `A` truly cannot address or connect to `C`, most distributed frameworks will fail or hang during initialization or first collective.

Typical failure points:

- Ray cannot form a healthy cluster
- PyTorch `init_process_group()` cannot complete
- NCCL/RCCL communicator initialization hangs or errors
- first all-reduce or all-gather stalls
- UCX endpoint setup fails

The important answer:

> this is not something vLLM fixes at the model layer

vLLM assumes the distributed runtime and communication backend can connect the required ranks.

**Case 3: custom application intentionally uses only adjacent communication**

If you build a custom runtime that only sends:

```text
A -> B
B -> C
```

and never requires:

```text
A <-> C
```

then a line topology can work.

But this requires careful design:

- no global all-reduce across all three ranks
- no all-gather across all three ranks
- no all-to-all across all three ranks
- no process-group backend that assumes full pairwise reachability
- explicit routing or staged forwarding logic

That is a custom distributed system, not a normal "just set tensor_parallel_size=3" deployment.

---

**4. The direct answer: is this a UCC glue-layer problem?**

Usually, no.

UCC is a collective communication API and library.

It can select collective algorithms and transport layers, and it can use components such as UCX/UCP, NCCL, SHARP, CUDA, or HIP depending on the build and runtime.

But UCC is not a substitute for fabric reachability.

Think of UCC like this:

```text
"I need all-reduce across these ranks.
Which collective implementation should I use?"
```

Not:

```text
"A and C cannot talk.
Please invent a safe, high-performance routed network through B."
```

If the underlying transport can route traffic, UCC may use it.

If the underlying transport cannot establish endpoints or move data between required ranks, UCC cannot make the job correct by itself.

---

**5. UCX versus UCC**

These names are easy to confuse.

| Component | Simple Meaning | Example Use |
|---|---|---|
| UCX | Transport framework | Move data over InfiniBand, RoCE, TCP, shared memory, CUDA, ROCm |
| UCC | Collective communication library | Implement all-reduce, broadcast, all-gather, all-to-all through selected transport layers |

UCX provides communication primitives and transport selection.

UCC provides collective operations.

Relationship:

```text
UCC collective
  -> may use UCP transport layer
  -> UCP comes from UCX
  -> UCX uses RDMA/TCP/shared memory/GPU transports
```

So:

```text
UCX = how bytes move
UCC = how collective algorithms are expressed and selected
```

---

</details>

## 6. 该技术栈中的 PyTorch 分布式

PyTorch 分布式会创建一个进程组。

一个进程组包含：

- `rank`
- `world_size`
- 后端
- rendezvous 方法
- 集合通信操作

示例：

```python
import torch
import torch.distributed as dist

dist.init_process_group(
    backend="nccl",
    init_method="env://",
)

x = torch.ones(1, device="cuda")
dist.all_reduce(x)
```

对于 CUDA GPU，PyTorch 通常使用 NCCL。

PyTorch 还提供其他后端，文档中列出的后端包括 Gloo、NCCL、UCC、MPI、XCCL、FAKE 以及注册的第三方后端。在当前 PyTorch 文档中，UCC 后端被标记为实验性。

实际影响：

> 如果 PyTorch 的进程组建立或一次小的 all-reduce 失败，那么建立在它之上的 vLLM 分布式推理服务就不会稳定

在归咎于模型服务器之前，始终先用微型测试调试后端。

---

## 7. 为什么张量并行对拓扑敏感

张量并行会拆分 layer 的计算。

示例：

```text
linear layer weight matrix
  split across ranks
  each rank computes partial output
  ranks synchronize partial results
```

这种同步会产生集合通信。

对于一个张量并行组，其 rank 为：

```text
[A, B, C]
```

后端可能需要等价于以下形式的通信模式：

```text
A <-> B
B <-> C
C <-> A
```

即使算法基于 ring、每次只向邻居发送分块，ring 通常也需要一个闭合环路。

一条线不是 ring。

```text
Line:
  A <-> B <-> C

Ring:
  A <-> B <-> C <-> A
```

如果不存在 `C <-> A` 路径，则该集合通信要么需要：

- 经由 B 的网络层路由
- 另一种显式支持该拓扑的集合通信算法
- 自定义通信插件/runtime
- 另一种并行策略

---

## 8. 流水线并行是首个该测试的变通方案

对于较弱的链式拓扑，流水线并行通常是第一个要评估的推理服务策略。

示例：

```bash
vllm serve <model> \
  --tensor-parallel-size 1 \
  --pipeline-parallel-size 3 \
  --distributed-executor-backend ray
```

思路：

```text
A: early layers
B: middle layers
C: late layers
```

与张量并行相比，这减少了全 rank 的集合通信。

取舍：

- 更适合链式拓扑
- 层内同步更少
- 可能存在流水线气泡
- 短 prompt 或低并发时调度更难
- 仍然需要可用的簇控制面通信
- 端点故障可能使整条流水线停摆

当模型太大、单个节点放不下，而互连又不足以支撑繁重的张量并行集合通信时，使用流水线并行。

---

## 9. 数据并行副本可避开链式问题

另一种实用的答案是不把一个模型拆分到全部三个节点上。

而是：

```text
A: full model replica
B: full model replica
C: full model replica
```

然后在 HTTP/负载均衡层对请求做路由。

这就是数据并行推理服务。

优点：

- 常规推理无需跨节点 GPU 集合通信
- 运维模型最简单
- 故障隔离更好
- 延迟可预测

缺点：

- 每个节点都必须容纳完整模型
- 对超大模型内存效率低
- 没有任何单个请求会用到全部节点

对于生产推理，副本往往优于脆弱的模型并行试验，除非模型规模迫使必须做分片。

---

## 10. NVLink 澄清

NVLink 不是通用的多节点路由器。

存在几种不同的情况：

```text
single server with direct NVLink:
  GPU-to-GPU peer path inside one NVLink domain

server with NVSwitch:
  many GPUs connected through switch fabric inside the system

multi-node NVIDIA systems:
  require specific NVLink Switch / networking architecture

ordinary Ethernet or InfiniBand cluster:
  inter-node communication normally uses NICs, RDMA, TCP, NCCL net plugins, UCX, etc.
```

不要假设：

```text
"NVLink exists somewhere, therefore every node can peer-to-peer every other node."
```

始终检查实际拓扑。

对于 NVIDIA 系统，从以下开始：

```bash
nvidia-smi topo -m
```

网络可达性：

```bash
ip route
ping <peer>
nc -vz <peer> <port>
```

RDMA：

```bash
ibv_devinfo
ib_write_bw
ib_read_bw
```

NCCL：

```bash
NCCL_DEBUG=INFO NCCL_DEBUG_SUBSYS=INIT,GRAPH,TUNING ...
```

---

## 11. 调试阶梯

不要从 vLLM 开始。

从最底层开始。

### 步骤 1：物理层与 IP 可达性

检查每一对：

```text
A -> B
B -> A
B -> C
C -> B
A -> C
C -> A
```

如果预期 `A <-> C` 能通过路由工作，就要加以验证。

### 步骤 2：传输测试

UCX：

```bash
ucx_info -d
ucx_perftest <peer> -t tag_bw
```

RDMA：

```bash
ibv_devinfo
ib_write_bw <peer>
```

TCP 回退：

```bash
iperf3 -s
iperf3 -c <peer>
```

### 步骤 3：集合通信测试

NCCL：

```bash
all_reduce_perf -b 8 -e 1G -f 2 -g 1
```

PyTorch：

```bash
torchrun --nnodes=3 --nproc-per-node=1 \
  --rdzv_backend=c10d \
  --rdzv_endpoint=<head>:29500 \
  test_allreduce.py
```


<details>
<summary>English original</summary>

**6. PyTorch distributed in this stack**

PyTorch distributed creates a process group.

A process group has:

- `rank`
- `world_size`
- backend
- rendezvous method
- collective operations

Example:

```python
import torch
import torch.distributed as dist

dist.init_process_group(
    backend="nccl",
    init_method="env://",
)

x = torch.ones(1, device="cuda")
dist.all_reduce(x)
```

For CUDA GPUs, PyTorch commonly uses NCCL.

PyTorch also exposes other backends, and the documentation lists backends including Gloo, NCCL, UCC, MPI, XCCL, FAKE, and registered third-party backends. The UCC backend is marked experimental in current PyTorch documentation.

Practical implication:

> if PyTorch process-group setup or a small all-reduce fails, vLLM distributed serving will not be stable above it

Always debug the backend with tiny tests before blaming the model server.

---

**7. Why tensor parallelism is topology-sensitive**

Tensor parallelism splits layer math.

Example:

```text
linear layer weight matrix
  split across ranks
  each rank computes partial output
  ranks synchronize partial results
```

That synchronization creates collectives.

For a tensor-parallel group with ranks:

```text
[A, B, C]
```

the backend may need communication patterns equivalent to:

```text
A <-> B
B <-> C
C <-> A
```

Even if the algorithm is ring-based and sends only neighbor chunks at a time, the ring normally needs a closed cycle.

A line is not a ring.

```text
Line:
  A <-> B <-> C

Ring:
  A <-> B <-> C <-> A
```

If no `C <-> A` path exists, the collective either needs:

- network-layer routing through B
- a different collective algorithm that explicitly supports the topology
- a custom communication plugin/runtime
- a different parallelism strategy

---

**8. Pipeline parallelism is the first workaround to test**

For a weak chain topology, pipeline parallelism is usually the first serving strategy to evaluate.

Example:

```bash
vllm serve <model> \
  --tensor-parallel-size 1 \
  --pipeline-parallel-size 3 \
  --distributed-executor-backend ray
```

The idea:

```text
A: early layers
B: middle layers
C: late layers
```

This reduces all-rank collectives compared with tensor parallelism.

Tradeoffs:

- better fit for chain topology
- lower intra-layer synchronization
- possible pipeline bubbles
- harder scheduling for short prompts or low concurrency
- still requires working cluster control-plane communication
- endpoint failure can stall the whole pipeline

Use pipeline parallelism when the model is too large for one node but the interconnect is not strong enough for heavy tensor parallel collectives.

---

**9. Data parallel replicas avoid the chain problem**

Another practical answer is not to split one model across all three nodes.

Instead:

```text
A: full model replica
B: full model replica
C: full model replica
```

Then route requests at the HTTP/load-balancer layer.

This is data parallel serving.

Pros:

- no cross-node GPU collectives for normal inference
- easiest operational model
- failure isolation is better
- latency is predictable

Cons:

- each node must fit the full model
- memory inefficient for very large models
- no single request uses all nodes

For production inference, replicas are often better than fragile model-parallel experiments unless the model size forces sharding.

---

**10. NVLink clarification**

NVLink is not a universal multi-node router.

There are several different situations:

```text
single server with direct NVLink:
  GPU-to-GPU peer path inside one NVLink domain

server with NVSwitch:
  many GPUs connected through switch fabric inside the system

multi-node NVIDIA systems:
  require specific NVLink Switch / networking architecture

ordinary Ethernet or InfiniBand cluster:
  inter-node communication normally uses NICs, RDMA, TCP, NCCL net plugins, UCX, etc.
```

Do not assume:

```text
"NVLink exists somewhere, therefore every node can peer-to-peer every other node."
```

Always inspect the actual topology.

For NVIDIA systems, start with:

```bash
nvidia-smi topo -m
```

For network reachability:

```bash
ip route
ping <peer>
nc -vz <peer> <port>
```

For RDMA:

```bash
ibv_devinfo
ib_write_bw
ib_read_bw
```

For NCCL:

```bash
NCCL_DEBUG=INFO NCCL_DEBUG_SUBSYS=INIT,GRAPH,TUNING ...
```

---

**11. Debugging ladder**

Do not start with vLLM.

Start at the bottom.

**Step 1: physical and IP reachability**

Check each pair:

```text
A -> B
B -> A
B -> C
C -> B
A -> C
C -> A
```

If `A <-> C` is expected to work through routing, prove it.

**Step 2: transport tests**

For UCX:

```bash
ucx_info -d
ucx_perftest <peer> -t tag_bw
```

For RDMA:

```bash
ibv_devinfo
ib_write_bw <peer>
```

For TCP fallback:

```bash
iperf3 -s
iperf3 -c <peer>
```

**Step 3: collective tests**

For NCCL:

```bash
all_reduce_perf -b 8 -e 1G -f 2 -g 1
```

For PyTorch:

```bash
torchrun --nnodes=3 --nproc-per-node=1 \
  --rdzv_backend=c10d \
  --rdzv_endpoint=<head>:29500 \
  test_allreduce.py
```

</details>

### 步骤 4：vLLM 小模型

只有在集合通信可用之后，再用小模型测试 vLLM。

先使用尽可能小的拓扑：

```text
1 node
2 nodes
3 nodes
```

然后对比：

```text
TP only
PP only
TP + PP
replicas
```

---

## 12. 最小 PyTorch 全规约测试

在运行大模型之前先做这个。

```python
# test_allreduce.py
import os
import socket

import torch
import torch.distributed as dist


def main():
    dist.init_process_group(backend=os.environ.get("BACKEND", "nccl"))

    rank = dist.get_rank()
    world = dist.get_world_size()
    host = socket.gethostname()

    device = torch.device(f"cuda:{rank % torch.cuda.device_count()}")
    torch.cuda.set_device(device)

    x = torch.ones(1, device=device) * (rank + 1)
    dist.all_reduce(x, op=dist.ReduceOp.SUM)
    torch.cuda.synchronize()

    expected = world * (world + 1) / 2
    print(f"rank={rank} host={host} value={x.item()} expected={expected}")

    dist.destroy_process_group()


if __name__ == "__main__":
    main()
```

运行：

```bash
BACKEND=nccl torchrun \
  --nnodes=3 \
  --nproc-per-node=1 \
  --rdzv_backend=c10d \
  --rdzv_endpoint=<head-node-ip>:29500 \
  test_allreduce.py
```

如果这一步卡住，那问题首先不在 vLLM。

---

## 13. 如何选择并行策略

使用下面的决策表。

| 约束 | 首先尝试的策略 | 原因 |
|---|---|---|
| 模型能装进单张 GPU/单个节点 | 数据并行副本 | 避免分布式集合通信 |
| 模型能装进单个节点但需要多张 GPU | 节点内张量并行 | NVLink/NVSwitch/PCIe 比多节点更容易 |
| 模型装不进单个节点 | 跨节点流水线并行 | 通信压力低于跨节点 TP |
| full-mesh InfiniBand/RoCE 强 | 张量并行或 TP+PP | 网络结构能支撑集合通信 |
| 仅有链式拓扑 | 流水线并行或自定义 runtime | 尽量避开全 rank 集合通信 |
| 异构节点 | 按能力等级划分副本 | 避免同步不匹配的硬件 |
| 研究自定义互连 | 从 microbenchmark 入手 | 先验证传输层，再做模型推理服务 |

---

## 14. 常见失效模式

### 失效：Ray 能看到节点，vLLM 仍然卡住

Ray 的布局还不够。

Ray 可能成功拉起 worker，但之后在 GPU 通信阶段 NCCL/RCCL/UCC 失败。

排查：

```text
Ray cluster status
  -> PyTorch all-reduce
  -> NCCL/RCCL logs
  -> vLLM
```

### 失效：TCP 可用但 RDMA 失败

这说明对所选的传输方式而言，仅有基础 IP 可达性是不够的。

检查：

- RDMA 驱动版本
- RoCE 的 GID index
- 无损以太网配置
- MTU 一致性
- 防火墙规则
- 容器的设备访问
- GPU-NIC 的 PCIe 局部性

### 失效：两个节点可用，三个节点卡住

这通常意味着拓扑或 rank 对之间的可达性不匹配。

逐一检查每一对。

不要假设：

```text
A <-> B works
B <-> C works
therefore A <-> C works
```

### 失效：性能远低于预期带宽

可能原因：

- 流量经过瓶颈节点
- 选错 NIC
- 未启用 GPUDirect RDMA
- 回退到 TCP
- PCIe NUMA 不匹配
- 集合通信针对该拓扑使用了不佳的 ring/tree
- 张量并行跨慢速链路

---

## 15. 研究项目：拓扑感知的分布式推理

一个好的研究项目是：

> 构建一个拓扑感知的 vLLM 推理服务方案，根据实测链路带宽和可达性来选择 TP、PP、副本或自定义通信。

输入：

```text
nodes
GPUs per node
GPU memory
GPU-GPU bandwidth matrix
NIC-NIC bandwidth matrix
reachability graph
model size
KV-cache budget
target latency
target throughput
```

输出：

```text
recommended serving layout
  data parallel replicas
  tensor parallel groups
  pipeline stages
  node placement
  expected bottleneck links
  required backend settings
```

示例图：

```text
Nodes:
  A, B, C

Reachability:
  A-B: 100 Gb/s
  B-C: 100 Gb/s
  A-C: none

Recommendation:
  avoid TP group [A, B, C]
  test PP stages [A, B, C]
  if global runtime requires A-C reachability, add routing/switching or use replicas
```

---

## 16. 实践实验计划

### Lab 1：后端冒烟测试

用以下配置运行 PyTorch 全规约脚本：

```text
world_size = 2
world_size = 3
backend = nccl
backend = gloo
backend = ucc, if built and available
```

记录：

- 成功/失败
- 初始化时间
- 全规约耗时
- 报错信息
- 选中的接口

### Lab 2：vLLM 布局实验

用以下配置运行小模型：

```text
TP=1, PP=1
TP=2, PP=1
TP=1, PP=2
TP=1, PP=3
```

记录：

- 启动是否成功
- 首 token 延迟
- 输出 token 延迟
- 并发下的吞吐
- GPU 利用率
- 网络吞吐


<details>
<summary>English original</summary>

**Step 4: vLLM small model**

Only after collectives work, test vLLM with a small model.

Use the smallest possible topology first:

```text
1 node
2 nodes
3 nodes
```

Then compare:

```text
TP only
PP only
TP + PP
replicas
```

---

**12. Minimal PyTorch all-reduce test**

Use this before running a large model.

```python
# test_allreduce.py
import os
import socket

import torch
import torch.distributed as dist


def main():
    dist.init_process_group(backend=os.environ.get("BACKEND", "nccl"))

    rank = dist.get_rank()
    world = dist.get_world_size()
    host = socket.gethostname()

    device = torch.device(f"cuda:{rank % torch.cuda.device_count()}")
    torch.cuda.set_device(device)

    x = torch.ones(1, device=device) * (rank + 1)
    dist.all_reduce(x, op=dist.ReduceOp.SUM)
    torch.cuda.synchronize()

    expected = world * (world + 1) / 2
    print(f"rank={rank} host={host} value={x.item()} expected={expected}")

    dist.destroy_process_group()


if __name__ == "__main__":
    main()
```

Run:

```bash
BACKEND=nccl torchrun \
  --nnodes=3 \
  --nproc-per-node=1 \
  --rdzv_backend=c10d \
  --rdzv_endpoint=<head-node-ip>:29500 \
  test_allreduce.py
```

If this hangs, vLLM is not the first problem.

---

**13. How to choose a parallelism strategy**

Use this decision table.

| Constraint | First Strategy to Try | Why |
|---|---|---|
| Model fits on one GPU/node | Data parallel replicas | Avoid distributed collectives |
| Model fits on one node but needs multiple GPUs | Tensor parallel within node | NVLink/NVSwitch/PCIe is easier than multi-node |
| Model does not fit on one node | Pipeline parallel across nodes | Lower communication pressure than cross-node TP |
| Strong full-mesh InfiniBand/RoCE | Tensor parallel or TP+PP | Fabric can support collectives |
| Chain topology only | Pipeline parallel or custom runtime | Avoid all-rank collectives where possible |
| Heterogeneous nodes | Replicas by capability class | Avoid synchronizing mismatched hardware |
| Researching custom interconnect | Start with microbenchmarks | Prove transport before model serving |

---

**14. Common failure modes**

**Failure: Ray sees nodes, vLLM still hangs**

Ray placement is not enough.

Ray may launch workers successfully while NCCL/RCCL/UCC fails later during GPU communication.

Debug:

```text
Ray cluster status
  -> PyTorch all-reduce
  -> NCCL/RCCL logs
  -> vLLM
```

**Failure: TCP works but RDMA fails**

This means basic IP reachability is not enough for the chosen transport.

Check:

- RDMA driver versions
- GID index for RoCE
- lossless Ethernet configuration
- MTU consistency
- firewall rules
- container device access
- GPU-NIC PCIe locality

**Failure: two nodes work, three nodes hang**

This often indicates topology or rank-pair reachability mismatch.

Check every pair.

Do not assume:

```text
A <-> B works
B <-> C works
therefore A <-> C works
```

**Failure: performance is far below expected bandwidth**

Likely causes:

- traffic routed through a bottleneck node
- wrong NIC selected
- no GPUDirect RDMA
- TCP fallback
- PCIe NUMA mismatch
- collectives using a poor ring/tree for the topology
- tensor parallel across slow links

---

**15. Research project: topology-aware distributed inference**

A good research project is:

> Build a topology-aware vLLM serving plan that chooses TP, PP, replicas, or custom communication based on measured link bandwidth and reachability.

Inputs:

```text
nodes
GPUs per node
GPU memory
GPU-GPU bandwidth matrix
NIC-NIC bandwidth matrix
reachability graph
model size
KV-cache budget
target latency
target throughput
```

Output:

```text
recommended serving layout
  data parallel replicas
  tensor parallel groups
  pipeline stages
  node placement
  expected bottleneck links
  required backend settings
```

Example graph:

```text
Nodes:
  A, B, C

Reachability:
  A-B: 100 Gb/s
  B-C: 100 Gb/s
  A-C: none

Recommendation:
  avoid TP group [A, B, C]
  test PP stages [A, B, C]
  if global runtime requires A-C reachability, add routing/switching or use replicas
```

---

**16. Practical lab plan**

**Lab 1: backend smoke test**

Run the PyTorch all-reduce script with:

```text
world_size = 2
world_size = 3
backend = nccl
backend = gloo
backend = ucc, if built and available
```

Record:

- success/failure
- time to initialize
- time to all-reduce
- error messages
- selected interfaces

**Lab 2: vLLM placement experiment**

Run a small model with:

```text
TP=1, PP=1
TP=2, PP=1
TP=1, PP=2
TP=1, PP=3
```

Record:

- startup success
- first-token latency
- output-token latency
- throughput under concurrency
- GPU utilization
- network throughput

</details>

### Lab 3：拓扑故障注入

阻断两个端点节点之间的直接流量。

观察：

- Ray 行为
- PyTorch 进程组行为
- 集合通信行为
- vLLM 启动行为
- 错误是显式报出，还是表现为挂起

目标不是让它失败。

目标是弄清协议栈在哪里检测到拓扑不匹配。

---

## 关键要点

- vLLM 是模型推理服务层，不是网络路由器。
- Ray 负责放置 worker，但 GPU 集合通信仍依赖通信后端和 fabric。
- PyTorch 进程组调用后端集合通信；它们假定后端能触达所需的 rank。
- UCX 在 RDMA、TCP、共享内存、CUDA、ROCm 等传输方式之上搬运字节。
- UCC 表达集合通信，且可使用多个传输层，但它不会神奇地修复不可达的 peer。
- 张量并行对拓扑的敏感度远高于流水线并行。
- 链式拓扑可以容纳流水线阶段，但常规分布式 runtime 可能仍需要全局可达性。
- 调试 vLLM 之前，始终先验证可达性、传输和集合通信。

---

## 参考资料

- vLLM 并行与扩展：[https://github.com/vllm-project/vllm/blob/main/docs/serving/parallelism_scaling.md](https://github.com/vllm-project/vllm/blob/main/docs/serving/parallelism_scaling.md)
- vLLM 分布式 API 参考：[https://docs.vllm.ai/en/stable/api/vllm/distributed/](https://docs.vllm.ai/en/stable/api/vllm/distributed/)
- PyTorch 分布式通信包：[https://docs.pytorch.org/docs/2.11/distributed.html](https://docs.pytorch.org/docs/2.11/distributed.html)
- UCX 主要特性：[https://openucx.readthedocs.io/en/master/ucx_features.html](https://openucx.readthedocs.io/en/master/ucx_features.html)
- UCC API 与文档：[https://openucx.github.io/ucc/](https://openucx.github.io/ucc/)
- UCC 规范：[https://openucx.github.io/ucc/api/v1.4.4/html/index.html](https://openucx.github.io/ucc/api/v1.4.4/html/index.html)
- UCC 源码仓库：[https://github.com/openucx/ucc](https://github.com/openucx/ucc)
- NVIDIA NCCL 文档：[https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/)


<details>
<summary>English original</summary>

**Lab 3: topology failure injection**

Block direct traffic between two endpoint nodes.

Observe:

- Ray behavior
- PyTorch process group behavior
- collective behavior
- vLLM startup behavior
- whether errors are explicit or hang-like

The goal is not to make it fail.

The goal is to learn where the stack detects the topology mismatch.

---

**Key takeaways**

- vLLM is the model serving layer, not the network router.
- Ray places workers, but GPU collectives still depend on the communication backend and fabric.
- PyTorch process groups call backend collectives; they assume the backend can reach required ranks.
- UCX moves bytes across transports such as RDMA, TCP, shared memory, CUDA, and ROCm.
- UCC expresses collective communication and can use multiple transport layers, but it does not magically fix unreachable peers.
- Tensor parallelism is much more topology-sensitive than pipeline parallelism.
- A chain topology can fit pipeline stages, but normal distributed runtimes may still need global reachability.
- Always prove reachability, transport, and collectives before debugging vLLM.

---

**References**

- vLLM parallelism and scaling: [https://github.com/vllm-project/vllm/blob/main/docs/serving/parallelism_scaling.md](https://github.com/vllm-project/vllm/blob/main/docs/serving/parallelism_scaling.md)
- vLLM distributed API reference: [https://docs.vllm.ai/en/stable/api/vllm/distributed/](https://docs.vllm.ai/en/stable/api/vllm/distributed/)
- PyTorch distributed communication package: [https://docs.pytorch.org/docs/2.11/distributed.html](https://docs.pytorch.org/docs/2.11/distributed.html)
- UCX main features: [https://openucx.readthedocs.io/en/master/ucx_features.html](https://openucx.readthedocs.io/en/master/ucx_features.html)
- UCC API and documentation: [https://openucx.github.io/ucc/](https://openucx.github.io/ucc/)
- UCC specification: [https://openucx.github.io/ucc/api/v1.4.4/html/index.html](https://openucx.github.io/ucc/api/v1.4.4/html/index.html)
- UCC source repository: [https://github.com/openucx/ucc](https://github.com/openucx/ucc)
- NVIDIA NCCL documentation: [https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Distributed AI Interconnects/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Distributed%20AI%20Interconnects/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
