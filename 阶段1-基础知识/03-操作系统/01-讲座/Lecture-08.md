---
title: 第 8 讲：多核调度、CPU 亲和性与 isolcpus
description: 第 8 讲：多核调度、CPU 亲和性与 isolcpus
---

# 第 8 讲：多核调度、CPU 亲和性与 isolcpus

## 概述

本讲要解决的核心问题是：当你有多个 CPU 和多个任务时，如何控制*哪个任务跑在哪个 CPU 上*，以及这种控制为何对性能至关重要？在现代服务器或 SoC 上，Linux 调度器每秒会做几十次 CPU 布局决策。这些决策在公平性上很出色，但对延迟可预测性极其糟糕 —— 调度器随时可能把任务在 CPU 之间迁移，冲掉它已预热的缓存，并增加数百微秒的开销。这里要建立的思维模型是飞机上的指定座位：默认的（自由入座）对大多数乘客可行，但对关键角色，你需要预留的、有保证的座位。对 AI 硬件工程师而言，控制 CPU 布局，决定了神经网络推理流水线是具备可预测的 2ms 延迟，还是会出现由 OS 来回挪动线程导致的随机 10ms 尖峰。

---

## SMP 调度器架构

Linux SMP 调度器使用 **per-CPU 运行队列**（`struct rq`）来 **减少竞争**：

- `load_balance()` 由以下事件触发：检测到空闲 CPU、周期性 `rebalance_domains()`、以及显式的迁移请求
- 负载度量：每个运行队列上任务权重的总和（CFS nice 加权）
- `migration/N` 内核线程代表调度器执行实际的任务迁移
- 目标：在遵守拓扑约束的前提下均衡各 CPU 的负载（优先同核 > 同封装 > 同 NUMA 节点）

```
  SMP Load Balancing Overview:

  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
  │   CPU 0      │    │   CPU 1      │    │   CPU 2      │    │   CPU 3      │
  │  runqueue    │    │  runqueue    │    │  runqueue    │    │  runqueue    │
  │  [T1][T2]   │    │  [T3][T4]   │    │  [ ]         │    │  [T5]        │
  │  [T5]       │    │             │    │              │    │              │
  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘    └──────┬───────┘
         │                   │                   │                   │
         └───────────────────┴─── load_balance() ┴───────────────────┘
                         (equalizes task counts across CPUs)
```

**负载均衡**对吞吐型工作负载有益，但对 **实时和推理工作负载有害**，因为在这类负载中，缓存热度和延迟可预测性比公平性更重要。

---

## CPU 拓扑层级

```
Physical Package (Socket)
  └── DIE
        └── MC (physical core)
              └── SMT Thread (hyperthreading — 2 logical CPUs per core)
```

Linux 将其建模为 `struct sched_domain` 层级。**不均衡阈值与迁移代价**在更高的域层级上增大；调度器 **不愿跨 NUMA 边界迁移**。

```
  Cache Sharing by Topology Level:

  ┌─────────────────────────────────────────────────┐
  │  NUMA Node 0 (Socket 0)                         │
  │  ┌───────────────────┐  ┌───────────────────┐   │
  │  │  Physical Core 0  │  │  Physical Core 1  │   │
  │  │  ┌────┐  ┌────┐  │  │  ┌────┐  ┌────┐  │   │
  │  │  │CPU0│  │CPU1│  │  │  │CPU2│  │CPU3│  │   │
  │  │  └────┘  └────┘  │  │  └────┘  └────┘  │   │
  │  │  Shared L1+L2    │  │  Shared L1+L2    │   │
  │  └─────────┬─────────┘  └─────────┬─────────┘   │
  │            └──────────┬────────────┘             │
  │                 Shared L3 (LLC)                  │
  └─────────────────────────────────────────────────┘
```

### 拓扑查看

```bash
lscpu                                                     # summary + per-CPU table
lscpu -e                                                  # extended per-CPU table
cat /sys/devices/system/cpu/cpu0/topology/core_id         # physical core ID
cat /sys/devices/system/cpu/cpu0/topology/physical_package_id  # socket ID
cat /sys/devices/system/cpu/cpu0/topology/thread_siblings # sibling SMT bitmask
cat /sys/devices/system/cpu/cpu0/topology/core_cpus_list  # all logical CPUs on this core
```

**SMT 兄弟线程**共享 L1/L2 缓存与执行单元。推理工作负载应 **绑定到物理核**（每核一个线程），而不是绑定到 SMT 对的两个兄弟线程，以避免 **资源竞争**。

> **关键洞察：** 同一物理核上的两个超线程共享 L1 指令缓存、L1 数据缓存以及整数/浮点执行单元。在互为兄弟的 SMT CPU 上运行两个相互竞争的推理线程，可能 *比* 把它们放在不同物理核上 *更慢*。在假设超线程有帮助之前，务必用性能剖析验证。

---


<details>
<summary>English original</summary>

**Lecture 8: Multi-Core Scheduling, CPU Affinity & isolcpus**

**Overview**

The core problem this lecture addresses is: when you have multiple CPUs and multiple tasks, how do you control *which task runs on which CPU*, and why does that control matter for performance? On a modern server or SoC, the Linux scheduler makes CPU placement decisions dozens of times per second. Those decisions are excellent for fairness but terrible for latency predictability — the scheduler can migrate a task between CPUs at any time, flushing its warm cache and adding hundreds of microseconds of overhead. The mental model to carry here is that of assigned seating on a plane: the default (open seating) works for most passengers, but for critical roles you need reserved, guaranteed seats. For an AI hardware engineer, controlling CPU placement is the difference between a neural network inference pipeline with predictable 2ms latency and one with random 10ms spikes caused by the OS moving threads around.

---

**SMP Scheduler Architecture**

Linux SMP scheduler uses **per-CPU runqueues** (`struct rq`) to **reduce contention**:

- `load_balance()` triggered by: idle CPU detection, periodic `rebalance_domains()`, and explicit migration requests
- Load metric: sum of task weights (CFS nice-weighted) on each runqueue
- `migration/N` kernel threads execute the actual task moves on behalf of the scheduler
- Goal: equalize load across CPUs while respecting topology constraints (prefer same-core > same-package > same-NUMA node)

```
  SMP Load Balancing Overview:

  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
  │   CPU 0      │    │   CPU 1      │    │   CPU 2      │    │   CPU 3      │
  │  runqueue    │    │  runqueue    │    │  runqueue    │    │  runqueue    │
  │  [T1][T2]   │    │  [T3][T4]   │    │  [ ]         │    │  [T5]        │
  │  [T5]       │    │             │    │              │    │              │
  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘    └──────┬───────┘
         │                   │                   │                   │
         └───────────────────┴─── load_balance() ┴───────────────────┘
                         (equalizes task counts across CPUs)
```

While **load balancing** is beneficial for throughput workloads, it is **harmful for real-time and inference workloads** where cache warmth and latency predictability matter more than fairness.

---

**CPU Topology Hierarchy**

```
Physical Package (Socket)
  └── DIE
        └── MC (physical core)
              └── SMT Thread (hyperthreading — 2 logical CPUs per core)
```

Linux models this as a `struct sched_domain` hierarchy. **Imbalance threshold and migration cost** increase at higher domain levels; the scheduler is **reluctant to migrate across NUMA boundaries**.

```
  Cache Sharing by Topology Level:

  ┌─────────────────────────────────────────────────┐
  │  NUMA Node 0 (Socket 0)                         │
  │  ┌───────────────────┐  ┌───────────────────┐   │
  │  │  Physical Core 0  │  │  Physical Core 1  │   │
  │  │  ┌────┐  ┌────┐  │  │  ┌────┐  ┌────┐  │   │
  │  │  │CPU0│  │CPU1│  │  │  │CPU2│  │CPU3│  │   │
  │  │  └────┘  └────┘  │  │  └────┘  └────┘  │   │
  │  │  Shared L1+L2    │  │  Shared L1+L2    │   │
  │  └─────────┬─────────┘  └─────────┬─────────┘   │
  │            └──────────┬────────────┘             │
  │                 Shared L3 (LLC)                  │
  └─────────────────────────────────────────────────┘
```

**Topology Inspection**

```bash
lscpu                                                     # summary + per-CPU table
lscpu -e                                                  # extended per-CPU table
cat /sys/devices/system/cpu/cpu0/topology/core_id         # physical core ID
cat /sys/devices/system/cpu/cpu0/topology/physical_package_id  # socket ID
cat /sys/devices/system/cpu/cpu0/topology/thread_siblings # sibling SMT bitmask
cat /sys/devices/system/cpu/cpu0/topology/core_cpus_list  # all logical CPUs on this core
```

**SMT siblings** share L1/L2 caches and execution units. Inference workloads should **pin to physical cores** (one thread per core), not to both siblings of an SMT pair, to avoid **resource contention**.

> **Key Insight:** Two hyperthreads on the same physical core share the L1 instruction cache, L1 data cache, and integer/FP execution units. Running two competing inference threads on sibling SMT CPUs can be *slower* than running them on separate physical cores. Always verify with profiling before assuming hyperthreading helps.

---

</details>

## CPU 亲和性

**亲和性掩码**：位掩码，指定任务允许在哪些 CPU 上执行。若不绑定，调度器可**在任意时刻把任务迁移到任意 CPU**。

```c
// System call interface
cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(2, &mask); CPU_SET(3, &mask);  // allow execution only on CPUs 2 and 3
sched_setaffinity(pid, sizeof(mask), &mask);
sched_getaffinity(pid, sizeof(mask), &mask);
```

```bash
taskset -c 2,3 ./inference_app      # launch process with affinity to CPUs 2 and 3
taskset -cp 2,3 <pid>               # apply affinity to an already-running process
cat /proc/<pid>/status | grep Cpus_allowed
```

亲和性绑定的好处：
- 消除调度器引发的迁移；保持 L1/L2 缓存热度
- 避免迁移时的 TLB flush 开销（x86 上约数千周期）
- 降低延迟波动；让时序更可预测

| 机制 | kernel 接口 | 粒度 | 持久性 | 工具 |
|---|---|---|---|---|
| CPU 亲和性 | `sched_setaffinity` | 按任务 | 直到进程退出 | `taskset` |
| `isolcpus` | 启动参数 | 按 CPU | 启动时 | kernel cmdline |
| cpuset cgroup | `/sys/fs/cgroup/cpuset` | 按 cgroup | 直到重新配置 | `cgset`, k8s |
| `numactl` | `numactl --cpunodebind` | 按 NUMA 节点 | 按调用 | `numactl` |
| Intel CAT | `/sys/fs/resctrl` | 按 LLC 路 | 直到重新配置 | `pqos`, `resctrl` |

> **常见陷阱：** `sched_setaffinity` 限制的是任务*可以*在哪里运行，但并不能阻止其他任务在同一批 CPU 上运行。若把推理线程绑定到 CPU 2 却不隔离 CPU 2，OS 守护进程和内核线程仍可在那里运行，并把你缓存里的数据挤出去。仅靠亲和性绑定并不够——必须与 `isolcpus` 结合才能实现完全隔离。

---

## isolcpus —— 把 CPU 从通用调度器中移除

`isolcpus=` 是一个 **kernel 启动参数**。列出的 CPU 在启动时被**永久移出通用调度池**。除非通过 `taskset` 或 `sched_setaffinity` **显式指定**，否则不会有任务被放到隔离 CPU 上。

实现完全隔离的配套参数：

```
isolcpus=2,3,4,5 nohz_full=2,3,4,5 rcu_nocbs=2,3,4,5 irqaffinity=0,1
```

| 参数 | 作用 |
|---|---|
| `isolcpus=N` | 把 CPU N 从通用调度池中移除 |
| `nohz_full=N` | 关闭 CPU N 上的周期性调度器 tick（tickless） |
| `rcu_nocbs=N` | 把 CPU N 上的 RCU 回调卸载到 `rcuoc` 内核线程 |
| `irqaffinity=0,1` | 把所有硬件 IRQ 只路由到 CPU 0 和 1 |

```
  CPU Isolation Layout (4-core example):

  ┌───────────────────────────────────────────────────────┐
  │  CPU 0          CPU 1          CPU 2          CPU 3   │
  │  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐ │
  │  │ OS/Kernel│   │ OS/Kernel│   │  RT/AI   │   │  RT/AI   │ │
  │  │ tasks    │   │ tasks    │   │  thread  │   │  thread  │ │
  │  │ IRQs ✓   │   │ IRQs ✓   │   │  IRQs ✗  │   │  IRQs ✗  │ │
  │  │ tick ✓   │   │ tick ✓   │   │  tick ✗  │   │  tick ✗  │ │
  │  │ RCU ✓    │   │ RCU ✓    │   │  RCU ✗   │   │  RCU ✗   │ │
  │  └──────────┘   └──────────┘   └──────────┘   └──────────┘ │
  │  ◄── General scheduler pool ──►◄───── Isolated ─────────►  │
  └───────────────────────────────────────────────────────┘
```

**综合效果**：OS 抖动从最坏情况约 200µs 降到**隔离核上 <10µs**。启动后可用 `cyclictest` 验证，并通过 `cat /proc/interrupts` 确认没有 IRQ 投递到隔离 CPU。

`tuna`：命令行工具，封装了 `isolcpus` 与 IRQ 亲和性管理，用于在 runtime 配置 CPU 屏蔽。

> **关键洞察：** `isolcpus` 是启动时的声明，表示“这些 CPU 是保留的”。`nohz_full` 关闭定时器 tick（否则会每 1ms 中断一次隔离 CPU）。`rcu_nocbs` 移除 RCU 回调。三者合起来才把 OS 几乎完全推离隔离 CPU。缺其中任何一个，隔离都不完整。

---


<details>
<summary>English original</summary>

**CPU Affinity**

**Affinity mask**: bitmask specifying which CPUs a task is allowed to execute on. Without pinning, the scheduler is free to **migrate a task to any CPU at any time**.

```c
// System call interface
cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(2, &mask); CPU_SET(3, &mask);  // allow execution only on CPUs 2 and 3
sched_setaffinity(pid, sizeof(mask), &mask);
sched_getaffinity(pid, sizeof(mask), &mask);
```

```bash
taskset -c 2,3 ./inference_app      # launch process with affinity to CPUs 2 and 3
taskset -cp 2,3 <pid>               # apply affinity to an already-running process
cat /proc/<pid>/status | grep Cpus_allowed
```

Benefits of affinity pinning:
- Eliminates scheduler-induced migrations; preserves L1/L2 cache warmth
- Avoids TLB flush cost on migration (~thousands of cycles on x86)
- Reduces latency variance; makes timing more predictable

| Mechanism | Kernel Interface | Granularity | Persistence | Tool |
|---|---|---|---|---|
| CPU affinity | `sched_setaffinity` | Per-task | Until process exits | `taskset` |
| `isolcpus` | Boot parameter | Per-CPU | Boot-time | kernel cmdline |
| cpuset cgroup | `/sys/fs/cgroup/cpuset` | Per-cgroup | Until reconfigured | `cgset`, k8s |
| `numactl` | `numactl --cpunodebind` | Per-NUMA node | Per-invocation | `numactl` |
| Intel CAT | `/sys/fs/resctrl` | Per-LLC-way | Until reconfigured | `pqos`, `resctrl` |

> **Common Pitfall:** `sched_setaffinity` restricts where a task *can* run, but it does not prevent other tasks from running on those same CPUs. If you pin your inference thread to CPU 2 but don't isolate CPU 2, OS daemons and kernel threads can still run there and evict your cache. Affinity pinning alone is not sufficient — it must be combined with `isolcpus` for full isolation.

---

**isolcpus — Removing CPUs from the General Scheduler**

`isolcpus=` is a **kernel boot parameter**. CPUs listed are **removed from the general scheduling pool** permanently at boot. No task is placed on an isolated CPU unless **explicitly assigned** via `taskset` or `sched_setaffinity`.

Complementary parameters for full isolation:

```
isolcpus=2,3,4,5 nohz_full=2,3,4,5 rcu_nocbs=2,3,4,5 irqaffinity=0,1
```

| Parameter | Effect |
|---|---|
| `isolcpus=N` | Removes CPU N from general scheduler pool |
| `nohz_full=N` | Disables periodic scheduler tick on CPU N (tickless) |
| `rcu_nocbs=N` | Offloads RCU callbacks off CPU N to `rcuoc` kthreads |
| `irqaffinity=0,1` | Routes all hardware IRQs to CPUs 0 and 1 only |

```
  CPU Isolation Layout (4-core example):

  ┌───────────────────────────────────────────────────────┐
  │  CPU 0          CPU 1          CPU 2          CPU 3   │
  │  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐ │
  │  │ OS/Kernel│   │ OS/Kernel│   │  RT/AI   │   │  RT/AI   │ │
  │  │ tasks    │   │ tasks    │   │  thread  │   │  thread  │ │
  │  │ IRQs ✓   │   │ IRQs ✓   │   │  IRQs ✗  │   │  IRQs ✗  │ │
  │  │ tick ✓   │   │ tick ✓   │   │  tick ✗  │   │  tick ✗  │ │
  │  │ RCU ✓    │   │ RCU ✓    │   │  RCU ✗   │   │  RCU ✗   │ │
  │  └──────────┘   └──────────┘   └──────────┘   └──────────┘ │
  │  ◄── General scheduler pool ──►◄───── Isolated ─────────►  │
  └───────────────────────────────────────────────────────┘
```

**Combined effect**: OS jitter reduced from ~200µs worst-case to **<10µs on isolated cores**. After boot, verify with `cyclictest` and confirm no IRQs are delivered to isolated CPUs via `cat /proc/interrupts`.

`tuna`: command-line tool that wraps `isolcpus` and IRQ affinity management for runtime CPU shielding configuration.

> **Key Insight:** `isolcpus` is a boot-time declaration that says "these CPUs are reserved." `nohz_full` turns off the timer tick (which would otherwise interrupt the isolated CPU every 1ms). `rcu_nocbs` removes RCU callbacks. Together they push the OS almost entirely off the isolated CPUs. Without all three, isolation is incomplete.

---

</details>

## CPU 频率调节

**变化的 CPU 频率**是**延迟抖动**的另一个来源。当 CPU 从低功耗状态切换到高频时，每秒执行的指令数在任务中途发生变化，使时序分析不再可靠。

| Governor | Behavior | Use Case |
|---|---|---|
| `performance` | 始终处于最高频率 | 确定性的 RT / 推理延迟 |
| `powersave` | 始终处于最低频率 | 受电池约束的空闲系统 |
| `schedutil` | 跟踪 CFS 利用率信号 | 通用吞吐类工作负载 |
| `ondemand` | 有负载时升频，空闲时降频 | 传统桌面 |

```bash
# Set performance governor on all CPUs
echo performance > /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# Or via cpupower
cpupower frequency-set -g performance
```

变频会引起时序抖动；频率切换最多会带来 200µs 的延迟。在 RT 与推理核心上**一律使用 `performance` governor**。

> **常见陷阱：** 在移动端 SoC（Jetson、Qualcomm）上，`performance` governor 可能与热降频冲突。在负载测试期间监控 `cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq` 以确认频率保持固定。若触及热限制，无论 governor 如何设置，CPU 都会降频。

---

## cpuset Cgroup 控制器

`isolcpus` 只在启动时生效，而 **`cpuset` cgroup** 提供了**运行时机制**，可在**不重启的情况下**把进程组分配到 CPU 子集上。

```bash
mkdir /sys/fs/cgroup/cpuset/inference
echo "4-7" > /sys/fs/cgroup/cpuset/inference/cpuset.cpus   # assign CPUs 4-7 to this group
echo "0"   > /sys/fs/cgroup/cpuset/inference/cpuset.mems   # bind to NUMA node 0 memory
echo <pid> > /sys/fs/cgroup/cpuset/inference/cgroup.procs  # add process to the group
```

这会创建一个 CPU 分区：`inference` cgroup 中的所有线程只在 CPU 4–7 上调度，其内存分配来自 NUMA node 0。

Kubernetes 内部使用 `cpu_manager_policy=static` 通过 cpuset 为 **Guaranteed QoS** pod 分配独占 CPU。可与 `topologyManagerPolicy=single-numa-node` 配合，为 GPU pod 对齐 CPU 与 GPU 的 PCIe 拓扑。

---

## 缓存拓扑与 Intel CAT

CPU 亲和性与隔离控制的是哪些线程在哪里运行，但无法阻止**共享缓存污染**。即便是绑定的推理线程，也要与其他核心上运行的 OS 守护进程共享 **L3 LLC**。**Intel CAT** 正是为解决该问题而生。

### 缓存共享

- L1（32–64 KB）、L2（256 KB–1 MB）：每个物理核心私有
- L3 / LLC（8–64 MB+）：同一封装内所有核心共享
- SMT 同胞线程共享 L1 + L2 + 执行单元；避免把相互竞争的工作负载放在同一物理核心上

```bash
perf stat -e cache-misses,cache-references ./inference    # measure LLC miss rate
```

### Intel Cache Allocation Technology（CAT / RDT）

使用 Capacity Bitmasks（CBM）在工作负载之间划分 LLC way。防止同处的 OS 守护进程把热点模型权重逐出。

```
  Intel CAT LLC Partitioning (16-way cache):

  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │12 │13 │14 │15 │  LLC ways
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
  ◄───────────────────────────►◄──────────────────────────────────►
      Inference group (ways 0-7)         OS/other (ways 8-15)
      CBM: 0x00FF                        CBM: 0xFF00
```

```bash
mount -t resctrl resctrl /sys/fs/resctrl
mkdir /sys/fs/resctrl/inference
# Allocate LLC ways 0–3 (4 ways) exclusively to inference group
echo "L3:0=0x000f" > /sys/fs/resctrl/inference/schemata
# 0x000f = binary 0000 0000 0000 1111 = ways 0,1,2,3
echo <pid> > /sys/fs/resctrl/inference/tasks
```

管理工具：`pqos`（Intel RDT OSS 工具）。还支持 Memory Bandwidth Allocation（MBA），用于限制后台工作负载消耗的 DRAM 带宽。

> **关键洞见：** 没有 Intel CAT 时，一次 `kworker` 或 `systemd-journald` 突发就可能冲刷掉 LLC 的 30–50%。下一次推理前向传播就会在每次权重访问上付出 L3 缺失的代价。CAT 为模型权重提供了一块 OS 活动无法触及的保留缓存分区。

---


<details>
<summary>English original</summary>

**CPU Frequency Scaling**

**Variable CPU frequency** is another source of **latency jitter**. When a CPU transitions from a low power state to high frequency, instructions per second changes mid-task, making timing analysis unreliable.

| Governor | Behavior | Use Case |
|---|---|---|
| `performance` | Always at maximum frequency | Deterministic RT / inference latency |
| `powersave` | Always at minimum frequency | Battery-constrained idle systems |
| `schedutil` | Tracks CFS utilization signal | General throughput workloads |
| `ondemand` | Ramps on load, scales down on idle | Legacy desktop |

```bash
# Set performance governor on all CPUs
echo performance > /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# Or via cpupower
cpupower frequency-set -g performance
```

Variable frequency causes timing jitter; frequency transitions add up to 200µs of latency. **Always use `performance` governor** on RT and inference cores.

> **Common Pitfall:** On mobile SoCs (Jetson, Qualcomm), the `performance` governor may conflict with thermal throttling. Monitor `cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq` during load tests to confirm the frequency stays fixed. If thermal limits are hit, the CPU throttles regardless of governor setting.

---

**cpuset Cgroup Controller**

While `isolcpus` works at boot time, **`cpuset` cgroups** provide a **runtime mechanism** to assign process groups to CPU subsets **without a reboot**.

```bash
mkdir /sys/fs/cgroup/cpuset/inference
echo "4-7" > /sys/fs/cgroup/cpuset/inference/cpuset.cpus   # assign CPUs 4-7 to this group
echo "0"   > /sys/fs/cgroup/cpuset/inference/cpuset.mems   # bind to NUMA node 0 memory
echo <pid> > /sys/fs/cgroup/cpuset/inference/cgroup.procs  # add process to the group
```

This creates a CPU partition: all threads in the `inference` cgroup are scheduled only on CPUs 4–7, and their memory allocations come from NUMA node 0.

Kubernetes uses `cpu_manager_policy=static` to allocate exclusive CPUs to **Guaranteed QoS** pods via cpuset internally. Pair with `topologyManagerPolicy=single-numa-node` to align CPU and GPU PCIe topology for GPU pods.

---

**Cache Topology and Intel CAT**

CPU affinity and isolation control which threads run where, but they don't prevent **shared cache pollution**. Even a pinned inference thread shares the **L3 LLC** with OS daemons running on other cores. **Intel CAT** addresses this.

**Cache Sharing**

- L1 (32–64 KB), L2 (256 KB–1 MB): private per physical core
- L3 / LLC (8–64 MB+): shared across all cores in a package
- SMT siblings share L1 + L2 + execution units; avoid co-locating competing workloads on same physical core

```bash
perf stat -e cache-misses,cache-references ./inference    # measure LLC miss rate
```

**Intel Cache Allocation Technology (CAT / RDT)**

Partitions LLC ways between workloads using Capacity Bitmasks (CBM). Prevents co-located OS daemons from evicting hot model weights.

```
  Intel CAT LLC Partitioning (16-way cache):

  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │12 │13 │14 │15 │  LLC ways
  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
  ◄───────────────────────────►◄──────────────────────────────────►
      Inference group (ways 0-7)         OS/other (ways 8-15)
      CBM: 0x00FF                        CBM: 0xFF00
```

```bash
mount -t resctrl resctrl /sys/fs/resctrl
mkdir /sys/fs/resctrl/inference
# Allocate LLC ways 0–3 (4 ways) exclusively to inference group
echo "L3:0=0x000f" > /sys/fs/resctrl/inference/schemata
# 0x000f = binary 0000 0000 0000 1111 = ways 0,1,2,3
echo <pid> > /sys/fs/resctrl/inference/tasks
```

Management tool: `pqos` (Intel RDT OSS tools). Also supports Memory Bandwidth Allocation (MBA) to limit DRAM bandwidth consumed by background workloads.

> **Key Insight:** Without Intel CAT, a single `kworker` or `systemd-journald` burst can flush 30–50% of the LLC. The next inference forward pass then pays L3-miss penalties on every weight access. CAT gives model weights a reserved cache partition that OS activity cannot touch.

---

</details>

## NUMA 亲和性

在多路服务器和具有非对称内存拓扑的 SoC 上，**内存从哪个 NUMA 节点分配**与由哪个 CPU 运行任务同样重要。**跨 NUMA 内存访问**会在每次缓存未命中时增加延迟。

```bash
# Pin process to socket 0 CPUs + socket 0 memory
numactl --cpunodebind=0 --membind=0 ./inference

# Show CPU <-> GPU PCIe topology
nvidia-smi topo -m

# Show per-process NUMA access stats
numastat -p <pid>

# Show NUMA hit/miss counters
numastat
```

**AutoNUMA**（`/proc/sys/kernel/numa_balancing`）：**在延迟敏感的推理节点上禁用**。自动页面迁移会引发 **TLB shootdown IPI**，带来抖动尖峰。

> **常见陷阱：** 如果 GPU 的 PCIe 根端口挂在 socket 0 上，而推理进程在 socket 1 上分配内存（socket 0 压力大时这是 OS 的默认行为），则每次 DMA 传输都要承受跨 NUMA 延迟。启动推理后务必用 `numastat -p <pid>` 确认 NUMA 绑定，并核实 `Numa_miss` 计数器接近零。

---

## 小结

| 机制 | 内核/用户态？ | 粒度 | 持久性 | 工具 |
|---|---|---|---|---|
| `sched_setaffinity` | 内核系统调用 | 每任务 | 进程生命周期 | `taskset` |
| `isolcpus` | 内核启动参数 | 每 CPU | 启动时 | kernel cmdline |
| `nohz_full` | 内核启动参数 | 每 CPU | 启动时 | kernel cmdline |
| `rcu_nocbs` | 内核启动参数 | 每 CPU | 启动时 | kernel cmdline |
| cpuset cgroup | 内核 cgroup | 每 cgroup | 动态 | `cgset`, kubectl |
| `cpufreq performance` | 内核驱动 | 每 CPU | 动态 | `cpupower` |
| Intel CAT | 内核 resctrl | 每 LLC way | 动态 | `pqos` |
| `numactl` | 用户空间封装 | 每 NUMA 节点 | 每次调用 | `numactl` |

### 概念回顾

- **为什么仅靠 CPU 亲和性不足以消除抖动？** 亲和性阻止目标任务迁走，但无法阻止其他任务在被固定的 CPU 上运行。必须用 `isolcpus` 将其他所有任务从该 CPU 上排除。
- **`isolcpus` 与 `sched_setaffinity` 有什么区别？** `isolcpus` 将某个 CPU 从内核的通用调度器池中移除——调度器永远不会把普通任务放到那里。`sched_setaffinity` 将某个特定任务限制在一组 CPU 上，但这些 CPU 仍留在通用池中。
- **为什么要在非隔离 CPU 上禁用 `nohz_full`？** 对于运行多个任务的 CPU，准确的调度器计账需要定时器 tick。只有隔离 CPU 才能从无 tick 运行中获益。
- **为什么竞争性工作负载要避开 SMT 兄弟线程？** 同一物理核心上的两个超线程共享 L1 缓存和执行单元。竞争线程会逐出推理线程的缓存行，并争夺 FP 单元，导致不可预测的变慢。
- **Intel CAT 解决了哪些 CPU 亲和性解决不了的问题？** 亲和性控制由哪个 CPU 运行哪个任务。CAT 控制哪些 LLC 缓存 *way* 可供哪个任务使用。即使做了亲和性固定，其他核心上的 OS 守护进程仍可能把模型权重从共享 LLC 中逐出。
- **为什么要在推理节点上禁用 AutoNUMA？** AutoNUMA 会在 NUMA 节点间迁移页面以改善局部性，但迁移本身会在映射被迁页面的所有 CPU 上引发 TLB shootdown IPI。这会带来延迟尖峰，任务本身看不到，但在延迟直方图中可见。

---

## AI 硬件关联

- `isolcpus=4-11 nohz_full=4-11 rcu_nocbs=4-11` 在 Jetson Orin 上把 8 个 ARM 核专用于 AI 推理线程；OS 和传感器驱动运行在 core 0–3 上，避免 OS 抖动进入推理延迟预算
- Kubernetes `cpu_manager_policy=static` 使用 cpuset cgroup 为 TensorRT 推理 pod 分配独占的物理 CPU 核，消除多租户推理集群中的 noisy-neighbor 调度器干扰
- 为 ROS2 实时回调组做 CPU 亲和性固定，可确保 LiDAR（激光雷达）处理和控制节点的确定性执行；调度器迁移会让 WCET 测量失效
- NUMA 感知的内存绑定（`numactl -m 0`）可降低多路服务器上的首次推理延迟——这类服务器上 GPU 的 PCIe 根端口挂在 socket 0；放在 socket-1 内存中的模型权重会在每次前向传播时产生跨 NUMA 取数开销
- Intel CAT 对 LLC 分区，使 OS 守护进程无法在推理期间逐出热的模型 layer 权重；没有 CAT 时，一次 `kworker` 突发可冲掉 30–50% 的 LLC，导致下一次前向传播开始时出现 LLC 未命中延迟尖峰
- 在 AV 边缘计算上，`cpufreq performance` governor 没有商量余地：从低功耗 C-state 升频的延迟会给空闲期后的第一次 GPU kernel 启动增加 100–200µs


<details>
<summary>English original</summary>

**NUMA Affinity**

On multi-socket servers and SoCs with asymmetric memory topology, the **NUMA node from which memory is allocated** matters as much as which CPU runs the task. **Cross-NUMA memory access** adds latency on every cache miss.

```bash
# Pin process to socket 0 CPUs + socket 0 memory
numactl --cpunodebind=0 --membind=0 ./inference

# Show CPU <-> GPU PCIe topology
nvidia-smi topo -m

# Show per-process NUMA access stats
numastat -p <pid>

# Show NUMA hit/miss counters
numastat
```

**AutoNUMA** (`/proc/sys/kernel/numa_balancing`): **disable on latency-sensitive inference nodes**. Automatic page migration causes **TLB shootdown IPIs** that add jitter spikes.

> **Common Pitfall:** If the GPU PCIe root port attaches to socket 0 and your inference process allocates memory on socket 1 (the OS default when socket 0 is under pressure), every DMA transfer suffers cross-NUMA latency. Always confirm NUMA binding with `numastat -p <pid>` after launching inference and verify the `Numa_miss` counter is near zero.

---

**Summary**

| Mechanism | Kernel/User? | Granularity | Persistence | Tool |
|---|---|---|---|---|
| `sched_setaffinity` | Kernel syscall | Per-task | Process lifetime | `taskset` |
| `isolcpus` | Kernel boot param | Per-CPU | Boot-time | kernel cmdline |
| `nohz_full` | Kernel boot param | Per-CPU | Boot-time | kernel cmdline |
| `rcu_nocbs` | Kernel boot param | Per-CPU | Boot-time | kernel cmdline |
| cpuset cgroup | Kernel cgroup | Per-cgroup | Dynamic | `cgset`, kubectl |
| `cpufreq performance` | Kernel driver | Per-CPU | Dynamic | `cpupower` |
| Intel CAT | Kernel resctrl | Per-LLC-way | Dynamic | `pqos` |
| `numactl` | Userspace wrapper | Per-NUMA node | Per-invocation | `numactl` |

**Conceptual Review**

- **Why is CPU affinity alone not enough to eliminate jitter?** Affinity prevents the target task from migrating away, but it does not prevent other tasks from running on the pinned CPU. `isolcpus` must be used to exclude all other tasks from the CPU.
- **What is the difference between `isolcpus` and `sched_setaffinity`?** `isolcpus` removes a CPU from the kernel's general scheduler pool — the scheduler will never place an ordinary task there. `sched_setaffinity` restricts one specific task to a set of CPUs, but those CPUs remain in the general pool.
- **Why disable `nohz_full` on non-isolated CPUs?** The timer tick is needed for accurate scheduler accounting on CPUs running multiple tasks. Only isolated CPUs benefit from tickless operation.
- **Why avoid SMT siblings for competing workloads?** Two hyperthreads on the same physical core share L1 cache and execution units. A competing thread evicts the inference thread's cache lines and competes for FP units, causing unpredictable slowdowns.
- **What problem does Intel CAT solve that CPU affinity does not?** Affinity controls which CPU runs which task. CAT controls which LLC cache *ways* are available to which task. Even with affinity pinning, an OS daemon on a different core can evict model weights from the shared LLC.
- **Why disable AutoNUMA on inference nodes?** AutoNUMA migrates pages between NUMA nodes to improve locality, but the migration itself causes TLB shootdown IPIs on all CPUs that map the migrated page. This adds latency spikes that are invisible to the task but visible in latency histograms.

---

**AI Hardware Connection**

- `isolcpus=4-11 nohz_full=4-11 rcu_nocbs=4-11` on Jetson Orin dedicates 8 ARM cores exclusively to AI inference threads; the OS and sensor drivers run on cores 0–3, preventing OS jitter from entering the inference latency budget
- Kubernetes `cpu_manager_policy=static` uses cpuset cgroups to give TensorRT inference pods exclusive physical CPU cores, eliminating noisy-neighbor scheduler interference in multi-tenant inference clusters
- CPU affinity pinning for ROS2 real-time callback groups ensures deterministic execution of LiDAR processing and motion control nodes; scheduler migration would invalidate WCET measurements
- NUMA-aware memory binding (`numactl -m 0`) reduces first-inference latency on multi-socket servers where the GPU PCIe root port attaches to socket 0; model weights in socket-1 memory incur cross-NUMA fetch overhead on every forward pass
- Intel CAT partitions LLC so OS daemons cannot evict hot model layer weights during inference; without CAT, a `kworker` burst can flush 30–50% of LLC, causing a spike of LLC-miss latency at the start of the next forward pass
- `cpufreq performance` governor is non-negotiable on AV edge compute: frequency ramp-up delay from a low-power C-state can add 100–200µs to the first GPU kernel launch after an idle period

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
