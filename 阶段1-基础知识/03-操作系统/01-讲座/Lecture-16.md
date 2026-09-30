---
title: 第 16 讲：NUMA 拓扑与高性能计算（HPC）内存优化
description: 第 16 讲：NUMA 拓扑与高性能计算（HPC）内存优化
---

# 第 16 讲：NUMA 拓扑与高性能计算（HPC）内存优化

## 概述

多 socket 服务器是大规模 AI 训练和高吞吐推理的标准平台。这类机器并不把所有 RAM 一视同仁：物理上挂在 socket 0 的 CPU 上的内存，从 socket 0 访问很快，从 socket 1 访问就很慢。这种非一致内存访问 —— **NUMA** —— 意味着数据存放在物理 RAM 的哪个位置，与数据本身是什么同样重要。心智模型是地理：本地内存像桌上的文件，远端内存像另一栋办公楼里的文件 —— 数据相同，但取回它要久得多。对 AI 硬件工程师而言，有没有 NUMA 意识，决定了多 GPU 训练是打满内存带宽，还是有一半时间在等跨 socket 数据传输。把 NUMA 布局做对，往往就是达到理论带宽与默默承受 2× 降速之间的差别。

---

## NUMA 架构

**NUMA**（Non-Uniform Memory Access）是多 socket 服务器的内存拓扑。每个 CPU socket（**NUMA 节点**）都有一组直连的 DRAM，能以**本地延迟和全带宽**访问。访问挂在其他 socket 上的 DRAM，需要穿过 socket 互连（Intel QPI/UPI、AMD Infinity Fabric、IBM X-Bus），从而带来额外延迟并降低带宽。

```
2-Socket NUMA Server Topology

Socket 0 (NUMA Node 0)          Socket 1 (NUMA Node 1)
┌─────────────────────┐         ┌─────────────────────┐
│  CPU cores 0–23     │         │  CPU cores 24–47    │
│  L3 Cache (60MB)    │         │  L3 Cache (60MB)    │
│  Memory Controller  │         │  Memory Controller  │
└────────┬────────────┘         └────────┬────────────┘
         │                               │
    ┌────▼────┐   QPI/UPI link      ┌────▼────┐
    │ DDR5    │ <─────────────────> │ DDR5    │
    │ (192GB) │   ~100 GB/s         │ (192GB) │
    │ ~80 ns  │   ~150-200 ns       │ ~80 ns  │
    │ local   │   cross-socket      │ local   │
    └─────────┘                     └─────────┘
         │                               │
    ┌────▼────┐                     ┌────▼────┐
    │ GPU0    │                     │ GPU2    │
    │ GPU1    │                     │ GPU3    │
    │ (PCIe)  │                     │ (PCIe)  │
    └─────────┘                     └─────────┘
```

| 访问类型 | 延迟 | 带宽（DDR5 双 socket Xeon） |
|-------------|---------|-------------------------------|
| 本地 DRAM | ~80 ns | 每 socket ~200 GB/s |
| 远端 DRAM（1 跳 QPI） | ~150–200 ns | ~100 GB/s |
| 远端 DRAM（2 跳，4 socket） | ~250+ ns | ~60 GB/s |

运行在 socket 0 上的进程访问分配在 socket 1 上的数据时，每次缓存未命中都要付出**远端惩罚** —— 延迟是本地访问的 2× 到 3×，带宽只有一半。

> **关键洞察：** NUMA 惩罚是无声的。没有错误、没有警告、没有可见的失败 —— 程序只是跑得更慢。访问模型权重时带宽减半，直接意味着矩阵乘法时间翻倍。`numastat -p` 就是让这个隐形问题显形的工具。

### 拓扑发现

在优化 NUMA 布局之前，需要先了解这台具体机器的拓扑：

```bash
numactl --hardware       # nodes, CPUs per node, memory per node, distance matrix
lstopo                   # graphical topology: sockets, cores, L3 cache, NUMA nodes, PCIe devices
numastat                 # per-node allocation hit/miss counters (system-wide)
cat /sys/devices/system/node/node0/distance   # raw NUMA distance factors
```

**距离矩阵**：本地访问为 10（归一化）；远端 1 跳通常为 20–40；2 跳为 60–80。

距离为 20 意味着跨 socket 访问的**延迟约为本地访问的 2×**。这个数字可以直接预测不具备 NUMA 意识的工作负载的性能损失。

---

## Linux NUMA 内存策略

Linux 提供了若干分配策略，控制新页面从哪个 NUMA 节点分配。理解默认行为至关重要，因为它对 AI 工作负载来说往往是错的。


<details>
<summary>English original</summary>

**Lecture 16: NUMA Topology & HPC Memory Optimization**

**Overview**

Multi-socket servers are the standard platform for large-scale AI training and high-throughput inference. These machines do not treat all RAM as equal: memory physically attached to the CPU on socket 0 is fast to access from socket 0, but slow to access from socket 1. This non-uniform memory access — **NUMA** — means that where data lives in physical RAM is as important as what the data is. The mental model is geography: local memory is like a file on your desk, remote memory is like a file in another office building — same data, but retrieving it takes much longer. For an AI hardware engineer, NUMA awareness determines whether multi-GPU training saturates memory bandwidth or spends half its time waiting for cross-socket data transfers. Getting NUMA placement right is often the difference between achieving theoretical bandwidth and suffering a silent 2× slowdown.

---

**NUMA Architecture**

**NUMA** (Non-Uniform Memory Access) is the memory topology of multi-socket servers. Each CPU socket (**NUMA node**) has a directly attached DRAM bank accessible at **local latency and full bandwidth**. Accessing DRAM attached to a different socket requires traversing the socket interconnect (Intel QPI/UPI, AMD Infinity Fabric, IBM X-Bus), incurring additional latency and reduced bandwidth.

```
2-Socket NUMA Server Topology

Socket 0 (NUMA Node 0)          Socket 1 (NUMA Node 1)
┌─────────────────────┐         ┌─────────────────────┐
│  CPU cores 0–23     │         │  CPU cores 24–47    │
│  L3 Cache (60MB)    │         │  L3 Cache (60MB)    │
│  Memory Controller  │         │  Memory Controller  │
└────────┬────────────┘         └────────┬────────────┘
         │                               │
    ┌────▼────┐   QPI/UPI link      ┌────▼────┐
    │ DDR5    │ <─────────────────> │ DDR5    │
    │ (192GB) │   ~100 GB/s         │ (192GB) │
    │ ~80 ns  │   ~150-200 ns       │ ~80 ns  │
    │ local   │   cross-socket      │ local   │
    └─────────┘                     └─────────┘
         │                               │
    ┌────▼────┐                     ┌────▼────┐
    │ GPU0    │                     │ GPU2    │
    │ GPU1    │                     │ GPU3    │
    │ (PCIe)  │                     │ (PCIe)  │
    └─────────┘                     └─────────┘
```

| Access type | Latency | Bandwidth (DDR5 2-socket Xeon) |
|-------------|---------|-------------------------------|
| Local DRAM | ~80 ns | ~200 GB/s per socket |
| Remote DRAM (1 QPI hop) | ~150–200 ns | ~100 GB/s |
| Remote DRAM (2 hops, 4-socket) | ~250+ ns | ~60 GB/s |

A process running on socket 0 that accesses data allocated on socket 1 pays the **remote penalty** on every cache miss — 2× to 3× the latency and half the bandwidth of local access.

> **Key Insight:** NUMA penalties are silent. There is no error, no warning, no visible failure — the program just runs slower. A 2× bandwidth reduction on model weight access directly translates to 2× longer matrix multiply time. `numastat -p` is the tool that makes this invisible problem visible.

**Topology Discovery**

Before optimizing NUMA placement, you need to understand the topology of the specific machine:

```bash
numactl --hardware       # nodes, CPUs per node, memory per node, distance matrix
lstopo                   # graphical topology: sockets, cores, L3 cache, NUMA nodes, PCIe devices
numastat                 # per-node allocation hit/miss counters (system-wide)
cat /sys/devices/system/node/node0/distance   # raw NUMA distance factors
```

**Distance matrix**: local access is 10 (normalized); remote 1-hop typically 20–40; 2-hop 60–80.

A distance of 20 means cross-socket access has roughly **2× the latency** of local access. This number directly predicts performance loss for NUMA-unaware workloads.

---

**Linux NUMA Memory Policies**

Linux provides several allocation policies that control which NUMA node a new page is allocated from. Understanding the default behavior is critical because it is often wrong for AI workloads.

</details>

### 默认策略：首次触碰

kernel 会在首个**触发缺页**（访问）该页的 CPU 所属的 NUMA node 上分配该页。当**初始化线程就是消费线程**时，这种做法是高效的。当 node 0 上的主线程初始化了一个之后仅由 node 1 上的线程消费的数据结构时，这就成了问题。

```
First-Touch Bug Pattern (common in AI frameworks):

Main thread (node 0):                Worker thread (node 1):
  weights = malloc(14GB)               [waiting for weights]
  memset(weights, 0, 14GB)  ←── pages allocated on node 0
  [signals worker thread]
                                       [begins inference]
                                       [reads weights]
                                           ↓
                                       Every cache miss crosses QPI!
                                       Effective bandwidth halved.
```

AI 工作负载的关键模式：
```c
/* Wrong: main thread (node 0) initializes, worker (node 1) consumes */
float *weights = malloc(model_size);
memset(weights, 0, model_size);   /* allocated on node 0 */

/* Correct: initialize on the thread that will use the data */
/* Pin worker thread to node 1, then fault the pages */
numa_run_on_node(1);
numa_set_membind(node1_mask);
float *weights = malloc(model_size);
memset(weights, 0, model_size);   /* allocated on node 1 */
```

> **常见陷阱：** 运行在 node 0 上的 PyTorch DataLoader worker 会在 node 0 的页上初始化权重张量，即使 GPU 和推理 worker 位于 node 1。这会在进程的整个生命周期内造成持续且隐性的带宽损失。务必在首次 `memset` 或张量填充之前，把初始化线程绑定到正确的 node 上。

### 内存策略类型

| 策略 | 模式常量 | 行为 |
|--------|--------------|----------|
| 默认（首次触碰） | `MPOL_DEFAULT` | 继承自父进程；优先本地 node |
| 绑定 | `MPOL_BIND` | 严格只从指定 node 分配；空间不足则失败 |
| 优先 | `MPOL_PREFERRED` | 优先使用指定 node；必要时回退到其他 node |
| 交错 | `MPOL_INTERLEAVE` | 在指定 node 间轮转分配页 |

### 系统调用

```c
/* Per-process policy */
set_mempolicy(MPOL_INTERLEAVE, &all_nodes_mask, max_node);

/* Per-VMA policy (overrides process policy for this address range) */
mbind(addr, len, MPOL_BIND, &node0_mask, max_node, MPOL_MF_MOVE);
```

`MPOL_MF_MOVE`：立即把 VMA 中已有的页迁移到目标 node。

带 `MPOL_MF_MOVE` 的 `mbind()` 调用非常强大：它会把**已分配的页**迁移到目标 node。这样就能**不重启进程**即修正首次触碰的放置错误。

---

## numactl：命令行策略控制

`numactl` 无需修改源代码即可对进程施加 NUMA 策略。它是快速实验和生产部署的首选工具：

```bash
# Bind process to CPUs and memory of node 0 (GPU on node 0 socket)
numactl --cpunodebind=0 --membind=0 ./inference_server

# Interleave allocations across all nodes (bandwidth-bound all-reduce)
numactl --interleave=all ./allreduce_benchmark

# Prefer node 1 but allow fallback (mixed workload)
numactl --preferred=1 ./data_loader

# Interleave across nodes 0 and 1 only
numactl --interleave=0,1 ./matmul_benchmark
```

同时使用 `--cpunodebind` 和 `--membind` 可确保 CPU 执行与内存分配都发生在**同一个 node** 上——把计算与数据放在一起，避免跨 socket 传输。

---

## libnuma API（编程控制）

对于需要在 runtime 感知拓扑的应用，`libnuma` 提供了对 NUMA 策略的编程访问接口：

```c
#include <numa.h>

int node = numa_node_of_cpu(sched_getcpu());   /* which node am I on? */
void *p = numa_alloc_onnode(size, node);       /* allocate on specific node */
numa_free(p, size);                            /* release */

numa_bind(nodemask);                           /* bind calling thread's CPU+memory */
numa_set_interleave_mask(&all_nodes);          /* interleave for future allocs */
numa_set_membind(&node_mask);                  /* memory-only bind */
```

`numa_node_of_cpu(sched_getcpu())` 模式相当于在 runtime 询问“我当前的 CPU core 位于哪个 NUMA node？”——在分配大缓冲区之前使用它，以确保本地布局。


<details>
<summary>English original</summary>

**Default: First-Touch**

The kernel allocates a page on the NUMA node of the CPU that first **faults** (accesses) it. This is efficient when the **initializing thread is the consuming thread**. It becomes a problem when a main thread on node 0 initializes a data structure later consumed exclusively by threads on node 1.

```
First-Touch Bug Pattern (common in AI frameworks):

Main thread (node 0):                Worker thread (node 1):
  weights = malloc(14GB)               [waiting for weights]
  memset(weights, 0, 14GB)  ←── pages allocated on node 0
  [signals worker thread]
                                       [begins inference]
                                       [reads weights]
                                           ↓
                                       Every cache miss crosses QPI!
                                       Effective bandwidth halved.
```

Critical pattern for AI workloads:
```c
/* Wrong: main thread (node 0) initializes, worker (node 1) consumes */
float *weights = malloc(model_size);
memset(weights, 0, model_size);   /* allocated on node 0 */

/* Correct: initialize on the thread that will use the data */
/* Pin worker thread to node 1, then fault the pages */
numa_run_on_node(1);
numa_set_membind(node1_mask);
float *weights = malloc(model_size);
memset(weights, 0, model_size);   /* allocated on node 1 */
```

> **Common Pitfall:** PyTorch DataLoader workers that run on node 0 initialize weight tensors on node 0's pages, even if the GPU and inference workers are on node 1. This creates a persistent, silent bandwidth penalty for the lifetime of the process. Always pin the initializing thread to the correct node before the first `memset` or tensor fill.

**Memory Policy Types**

| Policy | Mode constant | Behavior |
|--------|--------------|----------|
| Default (first-touch) | `MPOL_DEFAULT` | Inherit from parent; local node preferred |
| Bind | `MPOL_BIND` | Strictly allocate only from named nodes; fail if full |
| Preferred | `MPOL_PREFERRED` | Prefer named node; fall back to others if needed |
| Interleave | `MPOL_INTERLEAVE` | Round-robin pages across named nodes |

**System Calls**

```c
/* Per-process policy */
set_mempolicy(MPOL_INTERLEAVE, &all_nodes_mask, max_node);

/* Per-VMA policy (overrides process policy for this address range) */
mbind(addr, len, MPOL_BIND, &node0_mask, max_node, MPOL_MF_MOVE);
```

`MPOL_MF_MOVE`: migrate existing pages in the VMA to the target node immediately.

The `mbind()` call with `MPOL_MF_MOVE` is powerful: it moves **already-allocated pages** to the target node. This allows correcting a first-touch placement bug **without restarting the process**.

---

**numactl: Command-Line Policy Control**

`numactl` applies NUMA policies to a process without modifying its source code. It is the primary tool for quick experiments and production deployments:

```bash
# Bind process to CPUs and memory of node 0 (GPU on node 0 socket)
numactl --cpunodebind=0 --membind=0 ./inference_server

# Interleave allocations across all nodes (bandwidth-bound all-reduce)
numactl --interleave=all ./allreduce_benchmark

# Prefer node 1 but allow fallback (mixed workload)
numactl --preferred=1 ./data_loader

# Interleave across nodes 0 and 1 only
numactl --interleave=0,1 ./matmul_benchmark
```

Using `--cpunodebind` and `--membind` together ensures both CPU execution and memory allocation happen on the **same node** — collocating compute and data to avoid cross-socket transfers.

---

**libnuma API (Programmatic Control)**

For applications that need runtime topology awareness, `libnuma` provides programmatic access to NUMA policies:

```c
#include <numa.h>

int node = numa_node_of_cpu(sched_getcpu());   /* which node am I on? */
void *p = numa_alloc_onnode(size, node);       /* allocate on specific node */
numa_free(p, size);                            /* release */

numa_bind(nodemask);                           /* bind calling thread's CPU+memory */
numa_set_interleave_mask(&all_nodes);          /* interleave for future allocs */
numa_set_membind(&node_mask);                  /* memory-only bind */
```

The `numa_node_of_cpu(sched_getcpu())` pattern is the runtime equivalent of asking "which NUMA node is my current CPU core on?" — use it before allocating large buffers to ensure local placement.

---

</details>

## AutoNUMA 均衡

内核的 **自动 NUMA 均衡器** 会周期性扫描进程页表，临时清除 PTE 中的 present 位，再重新触发缺页以观察哪个 CPU 访问哪些页。**"热"页会迁移** 到访问它的 CPU 所在的 NUMA 节点。

```
AutoNUMA mechanism:
1. Kernel scanner removes Present bit from PTEs (makes pages "not present")
2. Next access to that page causes a page fault
3. Kernel records: "CPU on node X faulted page Y"
4. If page Y is on node Z ≠ X, kernel migrates it to node X
5. Result: hot pages move to the node where they are accessed most
```

- 启用/禁用：`echo 1 > /proc/sys/kernel/numa_balancing`
- 开销：扫描约占 1% CPU；迁移会短暂阻塞触发缺页的线程
- 对工作集稳定、可局部化的长时运行进程有益
- 对延迟敏感的推理有害：不可预测的迁移突发会造成尾延迟尖峰

**建议**：在实时和延迟敏感的推理服务器上禁用 AutoNUMA（内核参数 `numa_balancing=0`）。改用显式的 `mbind`/`numactl`。

> **常见陷阱：** AutoNUMA 默认启用。在延迟敏感的推理服务器上，后台页扫描器会引发周期性的 TLB shootdown（用于清除 Present 位）和迁移停顿（页在节点间移动期间）。这些会在 p99/p999 推理延迟直方图中表现为不可预测的尾延迟尖峰。禁用 AutoNUMA，并显式设置布局。

---

## 多 GPU NUMA 亲和性

GPU 拓扑与 NUMA 拓扑直接相互作用。在 2 路服务器中，GPU 卡连接到**某一个 socket 的 PCIe root complex**。来自远端 socket 的每次 CPU 到 GPU 内存传输都要跨越 socket 互连，每个 cache line 增加 **60–100 ns 的延迟**。

```bash
nvidia-smi topo -m    # GPU-GPU and CPU-GPU interconnect topology
```

2 路服务器输出示例：
```
        GPU0  GPU1  GPU2  GPU3  CPU Affinity
GPU0     X    NV4   SYS   SYS   0-23
GPU1    NV4    X    SYS   SYS   0-23
GPU2    SYS   SYS    X    NV4   24-47
GPU3    SYS   SYS   NV4    X    24-47
```

`SYS` = 穿越 QPI/UPI（慢）；`NV4` = NVLink 4（快）。将推理进程绑定到与目标 GPU 同处的 CPU 亲和范围，以避免 `SYS` 跨越。

`SYS` 链路跨越 QPI/UPI 互连。对于 GPU0/GPU1，使用 CPU 核 0–23（node 0）。对于 GPU2/GPU3，使用 CPU 核 24–47（node 1）。混用这些会造成跨 socket 的 PCIe 流量，使有效 H2D/D2H 传输带宽减半。

GPU0 的正确绑定：
```bash
numactl --cpunodebind=0 --membind=0 ./inference_server --gpu=0
```

> **关键洞见：** `nvidia-smi topo -m` 是在新的多 GPU 服务器上要运行的第一个命令。拓扑矩阵会确切告诉你每个 GPU 应绑定哪些 CPU 核和内存。这一个配置决定就能让 H2D 传输的有效内存带宽翻倍或减半。

---

## HPC（高性能计算）内存优化模式

在建立起 NUMA 概念之后，根据工作负载是受延迟约束还是受带宽约束，有两种主要的优化模式。

### 绑定（受延迟约束的推理）

将模型权重的全部分配和推理进程固定到**与 GPU 同处的 NUMA 节点**。使用被固定的工作线程做 first-touch，或在分配后使用 `mbind(MPOL_BIND)`。

```bash
echo 256 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
numactl --cpunodebind=0 --membind=0 ./inferenced
```

`MPOL_BIND` 策略确保：如果 node 0 内存耗尽，分配会失败，而不是静默回退到 node 1。这避免了内存压力下出现意外的性能下降。

### 交错（受带宽约束的训练）

对于超出单节点内存带宽的 GEMM 操作，**将权重矩阵交错分布到所有节点**。聚合带宽 = N × 单节点带宽。

```bash
numactl --interleave=all ./training_process
```

交错会以 round-robin 方式把页分散到所有节点。每节点 200 GB/s 的 4 节点系统在交错下可提供约 800 GB/s 的聚合带宽——而单节点只有 200 GB/s。对于瓶颈在内存吞吐的分布式训练 all-reduce 阶段，这是正确的模式。

> **关键洞见：** 绑定与交错是相反的两极。绑定为延迟敏感型工作负载最大化局部性（每次访问都命中本地 DRAM）。交错为受吞吐约束的工作负载最大化总带宽（所有 DRAM bank 都出力）。为工作负载类型选错模式会让性能减半。


<details>
<summary>English original</summary>

**AutoNUMA Balancing**

The kernel's **automatic NUMA balancer** periodically scans process page tables, temporarily removes present bits from PTEs, and re-faults pages to observe which CPU accesses which pages. **"Hot" pages migrate** to the NUMA node of the accessing CPU.

```
AutoNUMA mechanism:
1. Kernel scanner removes Present bit from PTEs (makes pages "not present")
2. Next access to that page causes a page fault
3. Kernel records: "CPU on node X faulted page Y"
4. If page Y is on node Z ≠ X, kernel migrates it to node X
5. Result: hot pages move to the node where they are accessed most
```

- Enable/disable: `echo 1 > /proc/sys/kernel/numa_balancing`
- Overhead: ~1% CPU for scanning; migration stalls the faulting thread briefly
- Beneficial for long-running processes with stable, localizable working sets
- Harmful for latency-sensitive inference: unpredictable migration bursts cause tail latency spikes

**Recommendation**: disable AutoNUMA (`numa_balancing=0` in kernel parameters) on real-time and latency-sensitive inference servers. Use explicit `mbind`/`numactl` instead.

> **Common Pitfall:** AutoNUMA is enabled by default. On a latency-sensitive inference server, the background page scanner causes periodic TLB shootdowns (to clear Present bits) and migration stalls (while pages move between nodes). These appear as unpredictable tail latency spikes in p99/p999 inference latency histograms. Disable AutoNUMA and set placement explicitly.

---

**Multi-GPU NUMA Affinity**

GPU topology interacts directly with NUMA topology. In a 2-socket server, GPU cards are connected to **one socket's PCIe root complex**. Every CPU-to-GPU memory transfer from a remote socket crosses the socket interconnect, adding **60–100 ns of latency** per cache line.

```bash
nvidia-smi topo -m    # GPU-GPU and CPU-GPU interconnect topology
```

Example 2-socket output:
```
        GPU0  GPU1  GPU2  GPU3  CPU Affinity
GPU0     X    NV4   SYS   SYS   0-23
GPU1    NV4    X    SYS   SYS   0-23
GPU2    SYS   SYS    X    NV4   24-47
GPU3    SYS   SYS   NV4    X    24-47
```

`SYS` = traverses QPI/UPI (slow); `NV4` = NVLink 4 (fast). Bind the inference process to the CPU affinity range collocated with the target GPU to avoid `SYS` crossings.

The `SYS` links cross the QPI/UPI interconnect. For GPU0/GPU1, use CPU cores 0–23 (node 0). For GPU2/GPU3, use CPU cores 24–47 (node 1). Mixing these creates cross-socket PCIe traffic that halves effective H2D/D2H transfer bandwidth.

Correct binding for GPU0:
```bash
numactl --cpunodebind=0 --membind=0 ./inference_server --gpu=0
```

> **Key Insight:** `nvidia-smi topo -m` is the first command to run on a new multi-GPU server. The topology matrix tells you exactly which CPU cores and memory to bind to each GPU. This single configuration decision can double or halve effective memory bandwidth for H2D transfers.

---

**HPC Memory Optimization Patterns**

With the NUMA concepts in place, there are two primary optimization patterns depending on whether the workload is latency-bound or bandwidth-bound.

**Binding (Latency-Bound Inference)**

Pin all model weight allocations and the inference process to the **NUMA node local to the GPU**. Use first-touch by the pinned worker thread, or `mbind(MPOL_BIND)` after allocation.

```bash
echo 256 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
numactl --cpunodebind=0 --membind=0 ./inferenced
```

The `MPOL_BIND` policy ensures that if node 0 is out of memory, the allocation fails rather than silently falling back to node 1. This prevents surprise performance degradation under memory pressure.

**Interleaving (Bandwidth-Bound Training)**

For GEMM operations that exceed single-node memory bandwidth, **interleave the weight matrix across all nodes**. Aggregate bandwidth = N × per-node bandwidth.

```bash
numactl --interleave=all ./training_process
```

Interleaving spreads pages round-robin across all nodes. A 4-node system with 200 GB/s per node delivers ~800 GB/s aggregate bandwidth with interleaving — versus 200 GB/s from a single node. This is the correct mode for distributed training all-reduce passes where the bottleneck is memory throughput.

> **Key Insight:** Binding and interleaving are opposites. Binding maximizes locality for latency-sensitive workloads (each access hits local DRAM). Interleaving maximizes total bandwidth for throughput-bound workloads (all DRAM banks contribute). Choosing the wrong mode for the workload type can cut performance in half.

</details>

### 大页 + NUMA

为模型权重缓冲区按 NUMA 节点预分配大页。HugeTLBFS 通过 `/sys/devices/system/node/nodeN/hugepages/` 支持按节点分配。配合 `madvise(MADV_WILLNEED)`，在推理开始前对本地页面做预缺页（pre-fault）。

```bash
# Allocate 256 huge pages on node 0 specifically
echo 256 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
```

按节点分配大页可保证大页来自正确的 NUMA 节点。系统级大页分配不提供局部性保证。

---

## 内存带宽测量

```bash
stream                                               # STREAM TRIAD benchmark per node
numastat -p $(pidof inferenced)                      # per-process NUMA hit/miss stats
perf stat -e cache-misses,LLC-load-misses ./workload # LLC miss rate (proxy for remote access)
perf mem record -- ./workload && perf mem report     # memory access latency profiling
```

`numastat -p` 字段：
- `numa_hit`：在预期节点上分配的页
- `numa_miss`：因内存压力而在其他节点上分配的页
- `numa_foreign`：本应属于该节点却被分配到别处的页

`numa_miss` 比率高于 5% 是进程遭遇 NUMA 布局失败的强烈信号——要么是首选节点上的内存压力，要么是策略配置错误。

---

## 小结

| 策略 | 行为 | 适用场景 | 命令 | 注意事项 |
|--------|----------|----------|---------|--------|
| 默认（first-touch） | 在触发缺页的 CPU 所在节点分配 | 通用工作负载 | （隐式） | 若初始化线程 ≠ 消费线程则出错 |
| `MPOL_BIND` | 严格：仅限指定节点 | 延迟敏感的推理 | `numactl --membind=N` | 节点满则 OOM |
| `MPOL_INTERLEAVE` | 跨节点轮询 | 带宽受限的 GEMM、all-reduce | `numactl --interleave=all` | 每次访问延迟更高 |
| `MPOL_PREFERRED` | 优先节点；允许回退 | 混合工作负载 | `numactl --preferred=N` | 可能静默转为远端 |
| AutoNUMA | 自动迁移热页 | 长时间运行的稳定工作负载 | `echo 1 > .../numa_balancing` | 延迟尖峰不可预测 |

### 概念回顾

- **NUMA 性能问题为什么难以发现？** 没有错误也没有告警——程序产生正确结果，只是更慢。远端内存访问对应用是透明的。只有 `numastat -p`（显示 `numa_miss` 计数）或 `perf mem report`（显示远端访问延迟）才能揭示问题。

- **first-touch 策略为什么对 AI 工作负载有问题？** AI 框架常在主线程或加载线程上初始化张量，随后在推理工作线程上使用它们。如果这些线程运行在不同的 NUMA 节点上，那么在整个进程生命周期内，所有权重数据都在错误的节点上。

- **什么时候该用 `MPOL_INTERLEAVE` 而不是 `MPOL_BIND`？** 当工作负载是带宽受限而非延迟受限时——具体来说，当模型权重大到单个节点的内存带宽无法以推理速度流式供给时，或在分布式训练 all-reduce 期间。

- **`nvidia-smi topo -m` 能告诉你什么，为什么重要？** 它显示每对 GPU 之间的互连以及每个 GPU 的 CPU 亲和性。`SYS` 连接会经过较慢的 QPI/UPI 链路。这能精确告诉你每个 GPU 应绑定到哪些 CPU 核与内存节点，以避免跨 socket 惩罚。

- **为什么延迟敏感的推理服务器上应禁用 AutoNUMA？** AutoNUMA 会扫描页表并以不可预测的间隔触发迁移，导致 TLB shootdown 和短暂的线程停顿。这些会在推理延迟直方图中表现为 p99/p999 尾延迟尖峰。

- **`numactl --membind` 与 `numactl --preferred` 的区别是什么？** 如果指定节点无法满足分配，`--membind` 会失败（OOM）。`--preferred` 会静默回退到其他节点。当需要本地分配的保证时用 `--membind`；当可以接受尽力而为的局部性时用 `--preferred`。

---


<details>
<summary>English original</summary>

**Huge Pages + NUMA**

Pre-allocate huge pages per NUMA node for model weight buffers. HugeTLBFS supports per-node allocation via `/sys/devices/system/node/nodeN/hugepages/`. Combine with `madvise(MADV_WILLNEED)` to pre-fault pages local before inference starts.

```bash
# Allocate 256 huge pages on node 0 specifically
echo 256 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
```

Per-node huge page allocation guarantees the huge pages come from the correct NUMA node. System-wide huge page allocation has no locality guarantee.

---

**Memory Bandwidth Measurement**

```bash
stream                                               # STREAM TRIAD benchmark per node
numastat -p $(pidof inferenced)                      # per-process NUMA hit/miss stats
perf stat -e cache-misses,LLC-load-misses ./workload # LLC miss rate (proxy for remote access)
perf mem record -- ./workload && perf mem report     # memory access latency profiling
```

`numastat -p` fields:
- `numa_hit`: pages allocated on the intended node
- `numa_miss`: pages allocated on a different node due to pressure
- `numa_foreign`: pages intended for this node but allocated elsewhere

A `numa_miss` rate above 5% is a strong signal that the process is experiencing NUMA placement failures — either due to memory pressure on the preferred node or incorrect policy configuration.

---

**Summary**

| Policy | Behavior | Best for | Command | Caveat |
|--------|----------|----------|---------|--------|
| Default (first-touch) | Alloc on faulting CPU's node | General workloads | (implicit) | Wrong if initializer ≠ consumer |
| `MPOL_BIND` | Strict: only named nodes | Latency-sensitive inference | `numactl --membind=N` | OOM if node is full |
| `MPOL_INTERLEAVE` | Round-robin across nodes | Bandwidth-bound GEMM, all-reduce | `numactl --interleave=all` | Higher latency per access |
| `MPOL_PREFERRED` | Prefer node; allow fallback | Mixed workloads | `numactl --preferred=N` | May silently go remote |
| AutoNUMA | Migrate hot pages automatically | Long-running stable workloads | `echo 1 > .../numa_balancing` | Unpredictable latency spikes |

**Conceptual Review**

- **What makes NUMA performance problems hard to detect?** There are no errors or warnings — the program produces correct results, just slower. Remote memory access is transparent to the application. Only `numastat -p` (showing `numa_miss` counts) or `perf mem report` (showing remote access latency) reveals the problem.

- **Why is the first-touch policy problematic for AI workloads?** AI frameworks often initialize tensors on a main or loader thread, then use them on inference worker threads. If the threads run on different NUMA nodes, all weight data is on the wrong node for the lifetime of the process.

- **When should you use `MPOL_INTERLEAVE` instead of `MPOL_BIND`?** When the workload is bandwidth-bound rather than latency-bound — specifically, when model weights are larger than a single node's memory bandwidth can stream at inference speed, or during distributed training all-reduce.

- **What does `nvidia-smi topo -m` tell you and why does it matter?** It shows the interconnect between each GPU pair and the CPU affinity of each GPU. `SYS` connections traverse the slow QPI/UPI link. This tells you exactly which CPU cores and memory nodes to bind to for each GPU to avoid cross-socket penalties.

- **Why should AutoNUMA be disabled on latency-sensitive inference servers?** AutoNUMA scans page tables and triggers migrations at unpredictable intervals, causing TLB shootdowns and brief thread stalls. These show up as p99/p999 tail latency spikes in inference latency histograms.

- **What is the difference between `numactl --membind` and `numactl --preferred`?** `--membind` fails (OOM) if the specified node cannot satisfy the allocation. `--preferred` falls back to other nodes silently. Use `--membind` when you require local allocation guarantees; use `--preferred` when best-effort locality is acceptable.

---

</details>

## AI Hardware Connection

- 多 GPU 推理服务器需要 NUMA 绑定，使 CPU 预处理线程、锁页主机内存与目标 GPU 共享同一个 PCIe root complex，避免每次 H2D 传输都受到 QPI/UPI 惩罚
- first-touch 初始化必须在绑定到正确 NUMA 节点的线程上完成；若 PyTorch DataLoader worker 在错误的节点上初始化权重张量，会在该进程的整个生命周期内静默地把带宽降低 2×
- 在所有节点上做 NUMA interleave，可在分布式训练的全规约 pass 中最大化聚合内存带宽——这类场景的瓶颈是内存吞吐而非延迟
- `numastat -p` 是识别已部署推理流水线中远程内存访问的主要诊断手段；`numa_miss` 高于 5% 表明存在 NUMA 布局 bug
- 对延迟敏感的推理服务器必须禁用 AutoNUMA，以防后台页面迁移与推理 kernel 执行争抢资源而引发尾延迟尖峰
- Jetson Orin 只有一个 NUMA 节点（CPU-GPU 统一 DRAM）；NUMA 策略不适用，但 Cortex-A78 簇与 Cortex-X1 核之间的 CPU 簇亲和性仍会影响 LLC 共享与带宽


<details>
<summary>English original</summary>

**AI Hardware Connection**

- Multi-GPU inference servers require NUMA binding so that CPU pre-processing threads, pinned host memory, and the target GPU share the same PCIe root complex, avoiding QPI/UPI penalty on every H2D transfer
- First-touch initialization must occur on the thread pinned to the correct NUMA node; PyTorch DataLoader workers that initialize weight tensors on the wrong node silently degrade bandwidth by 2× for the lifetime of the process
- NUMA interleave across all nodes maximizes aggregate memory bandwidth for distributed training all-reduce passes where the bottleneck is memory throughput rather than latency
- `numastat -p` is the primary diagnostic for identifying remote memory access in a deployed inference pipeline; `numa_miss` values above 5% indicate a NUMA placement bug
- AutoNUMA must be disabled on latency-sensitive inference servers to prevent tail latency spikes caused by background page migration competing with inference kernel execution
- Jetson Orin has a single NUMA node (unified CPU-GPU DRAM); NUMA policies do not apply, but CPU-cluster affinity between the Cortex-A78 cluster and Cortex-X1 cores still affects LLC sharing and bandwidth

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-16.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-16.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
