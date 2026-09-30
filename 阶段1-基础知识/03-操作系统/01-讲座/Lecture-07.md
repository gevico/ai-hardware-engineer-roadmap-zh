---
title: 第 7 讲：实时 Linux：PREEMPTRT 与确定性
description: 第 7 讲：实时 Linux：PREEMPTRT 与确定性
---

# 第 7 讲：实时 Linux：PREEMPT_RT 与确定性

## 概述

本讲要解决的核心问题是：如何让 Linux 在保证的时间界限内响应外部事件？标准 Linux 为吞吐而设计——它会推迟低优先级工作，但对任何特定任务何时运行不作硬性承诺。实时系统把这一优先级翻转过来：最坏情况延迟比平均吞吐更重要。这里要建立的心智模型是一条带严格时间槽的工厂产线——即使平均节奏没问题，错过一个槽位也可能让整条产线停摆。对 AI 硬件工程师而言，这一点直接相关：自动驾驶控制回路、机器人伺服控制器和安全看门狗都要求有界的响应时间。一个迟了 5ms 才算出正确结果的神经网络，和一个算错结果的神经网络同样危险。

---

## 实时定义

实时**不意味着快**。它意味着**有界的最坏情况延迟**——一个必须始终满足的截止时间。

- **软实时（Soft RT）**：错过截止时间会降低质量（如音频帧丢失、视频渲染延迟）
- **硬实时（Hard RT）**：错过截止时间即系统失败（如电机过冲、制动动作过晚）
- 关注的指标是**最坏情况延迟（WCL）**，而非平均值；即使平均值为 10µs，一次 1ms 的尖峰也违反 500µs 的硬截止时间

> **关键洞察：** PREEMPT_RT 并不会让 Linux 更快——它让 Linux 更*可预测*。目标是有界的最坏情况延迟，而不是最小的平均延迟。一个平均延迟 5µs 但偶尔出现 500µs 尖峰的系统无法满足硬实时要求。一个稳定在 80µs 延迟的系统则能通过。

---

## Linux 抢占模型

构建时通过 `CONFIG_PREEMPT_*` 配置：

| 配置 | 可抢占性 | 典型最坏情况延迟 | 使用场景 |
|---|---|---|---|
| `PREEMPT_NONE` | 不可抢占（仅让出点） | >1ms | 服务器、吞吐型工作负载 |
| `PREEMPT_VOLUNTARY` | 新增 `might_resched()` 点 | ~500µs | 通用桌面 Linux |
| `PREEMPT` | 多数内核路径可抢占 | ~100–200µs | 交互式桌面 |
| `PREEMPT_RT` | 内核完全可抢占 | 可达 <50µs | 机器人、电机控制、自动驾驶 |

`PREEMPT_RT` 于 **Linux 6.12**（2024 年 11 月发布）合入主线。在此之前，它是由 Thomas Gleixner 和 Ingo Molnar 长期维护的树外补丁系列。

可以把每种抢占模型理解为控制内核中存在多少「不可中断」区域。`PREEMPT_NONE` 有很多；`PREEMPT_RT` 几乎没有。

---

## PREEMPT_RT 内部机制

PREEMPT_RT 补丁系列通过三个核心机制实现**完全可抢占**：把**自旋锁转换为可睡眠锁**、把**中断处理程序移入可调度的线程**，以及让 **softirq 处理可抢占**。每个机制针对的延迟来源各不相同。

### 自旋锁转换

在 `PREEMPT_RT` 下，`spinlock_t` 被转换为可睡眠的 `rtmutex`：

- **RT 之前的行为**：`spin_lock()` 会关闭抢占并忙等；持有锁期间，该 CPU 上不能运行其他任务
- **RT 行为**：若锁存在竞争，`spin_lock()` 会让任务睡眠；更高优先级的任务可以抢占并运行
- **`raw_spinlock_t`**：逃生通道——仍是真正的不可抢占自旋锁；仅用于极短的硬件关键区段（如 per-CPU 计数器更新、arch 中断入口代码）

下图展示了转换前后的行为差异：

```
  PREEMPT_NONE / PREEMPT spinlock behavior:
  ┌──────────────────────────────────────────────────┐
  │  CPU 0                                           │
  │  spin_lock(&L)  ── preemption OFF ──────────────►│
  │  [busy-waits if contended]                       │
  │  spin_unlock(&L) ── preemption ON               │
  │                                                  │
  │  HIGH-priority task: CANNOT RUN during spin      │
  └──────────────────────────────────────────────────┘

  PREEMPT_RT rtmutex behavior:
  ┌──────────────────────────────────────────────────┐
  │  CPU 0                     CPU 1                 │
  │  spin_lock(&L)             HIGH-prio task wakes  │
  │  [sleeps if contended] ──► [preempts, runs now]  │
  │  [woken when L released]                         │
  │  spin_unlock(&L)                                 │
  │                                                  │
  │  HIGH-priority task: CAN RUN; no blocked CPU     │
  └──────────────────────────────────────────────────┘
```

> **关键洞察：** 自旋锁到 rtmutex 的转换，正是让 PREEMPT_RT 与 `CONFIG_PREEMPT` 有本质区别的原因。使用真正的自旋锁时，CPU 被占用，无论何种情况，更高优先级的任务都无法在其上运行。而使用 rtmutex 时，低优先级任务等待锁期间 CPU 是空闲的。


<details>
<summary>English original</summary>

**Lecture 7: Real-Time Linux: PREEMPT_RT & Determinism**

**Overview**

The core problem this lecture addresses is: how do you make Linux respond to external events within a guaranteed time bound? Standard Linux is designed for throughput — it defers low-priority work but gives no hard promises about when any particular task will run. Real-time systems flip this priority: worst-case latency matters more than average throughput. The mental model to carry here is that of a factory production line with strict time slots — a single missed slot can shut down the entire line, even if the average pace is fine. For an AI hardware engineer, this matters directly: autonomous vehicle control loops, robot servo controllers, and safety watchdogs all require bounded response time. A neural network that computes a correct result 5ms too late is as dangerous as one that computes a wrong result.

---

**Real-Time Definitions**

Real-time does **not mean fast**. It means **bounded worst-case latency** — a deadline that must always be met.

- **Soft RT**: missing a deadline degrades quality (e.g., dropped audio frame, delayed video render)
- **Hard RT**: missing a deadline is a system failure (e.g., motor overshoot, brake actuation too late)
- Metric of interest: **worst-case latency (WCL)**, not average; a single 1ms spike violates a 500µs hard deadline even if the average is 10µs

> **Key Insight:** PREEMPT_RT does not make Linux faster — it makes Linux more *predictable*. The goal is bounded worst-case latency, not minimum average latency. A system with average 5µs latency but occasional 500µs spikes fails hard-RT requirements. A system with a consistent 80µs latency passes.

---

**Linux Preemption Models**

Configured at build time via `CONFIG_PREEMPT_*`:

| Config | Preemptibility | Typical Worst-Case Latency | Use Case |
|---|---|---|---|
| `PREEMPT_NONE` | Not preemptible (yield points only) | >1ms | Servers, throughput workloads |
| `PREEMPT_VOLUNTARY` | `might_resched()` points added | ~500µs | General desktop Linux |
| `PREEMPT` | Most kernel paths preemptible | ~100–200µs | Interactive desktop |
| `PREEMPT_RT` | Fully preemptible kernel | <50µs achievable | Robotics, motor control, AV |

`PREEMPT_RT` was mainlined in **Linux 6.12** (released November 2024). Prior to that it was a long-running out-of-tree patch series maintained by Thomas Gleixner and Ingo Molnar.

Think of each preemption model as controlling how many "no interrupt" zones exist in the kernel. `PREEMPT_NONE` has many; `PREEMPT_RT` has almost none.

---

**PREEMPT_RT Internals**

The PREEMPT_RT patch series achieves **full preemptibility** through three core mechanisms: converting **spinlocks to sleeping locks**, moving **interrupt handlers into schedulable threads**, and making **softirq processing preemptible**. Each mechanism attacks a different source of latency.

**Spinlock Conversion**

Under `PREEMPT_RT`, `spinlock_t` is converted to a sleeping `rtmutex`:

- **Pre-RT behavior**: `spin_lock()` disables preemption and busy-waits; no other task can run on that CPU while holding the lock
- **RT behavior**: `spin_lock()` puts the task to sleep if the lock is contended; a higher-priority task can preempt and run
- **`raw_spinlock_t`**: the escape hatch — remains a true non-preemptible spinlock; reserved for very short hardware-critical sections (e.g., per-CPU counter updates, arch interrupt entry code)

The following diagram shows the behavioral difference before and after the conversion:

```
  PREEMPT_NONE / PREEMPT spinlock behavior:
  ┌──────────────────────────────────────────────────┐
  │  CPU 0                                           │
  │  spin_lock(&L)  ── preemption OFF ──────────────►│
  │  [busy-waits if contended]                       │
  │  spin_unlock(&L) ── preemption ON               │
  │                                                  │
  │  HIGH-priority task: CANNOT RUN during spin      │
  └──────────────────────────────────────────────────┘

  PREEMPT_RT rtmutex behavior:
  ┌──────────────────────────────────────────────────┐
  │  CPU 0                     CPU 1                 │
  │  spin_lock(&L)             HIGH-prio task wakes  │
  │  [sleeps if contended] ──► [preempts, runs now]  │
  │  [woken when L released]                         │
  │  spin_unlock(&L)                                 │
  │                                                  │
  │  HIGH-priority task: CAN RUN; no blocked CPU     │
  └──────────────────────────────────────────────────┘
```

> **Key Insight:** The spinlock-to-rtmutex conversion is what makes PREEMPT_RT fundamentally different from `CONFIG_PREEMPT`. With a true spinlock, the CPU is occupied and no higher-priority task can run on it, no matter what. With an rtmutex, the CPU is free while a low-priority task waits for the lock.

</details>

### 硬中断线程化

- 在 RT 下，IRQ handler 变成 **线程化的 kernel 线程**
- 默认线程优先级：`SCHED_FIFO`，优先级 50
- `IRQF_NO_THREAD` 标志：为无法睡眠的特定 handler 提供退出选项（例如定时器中断的硬件路径）
- 后果：优先级 99 的 RT 任务可以抢占以优先级 50 运行的 IRQ handler

这是一次概念上的转变。在标准 Linux 中，中断是神圣的——它们抢占一切。在 PREEMPT_RT 下，中断只是高优先级线程，与任何其他任务一样服从同一套调度规则。

### 软中断处理

- 软中断运行在 `ksoftirqd` 个 kernel 线程中（每个 CPU 一个）
- `ksoftirqd` 是普通的可抢占任务；RT 任务可在任意时刻抢占它
- 避免软中断风暴在无界时间内阻塞高优先级 RT 任务

### 在 kernel 中睡眠

- RT 之前：许多 kernel 代码路径带有“此处不可睡眠”的约束，因为自旋锁会关闭抢占
- 使用 RT 后：在大多数上下文中睡眠是安全的，因为 `spinlock_t` 现在是睡眠锁
- 剩余的 `raw_spinlock_t` 区段仍不可睡眠；这些区段必须保持极短

> **常见陷阱：** 在持有 `raw_spinlock_t` 时调用 `msleep()` 或 `schedule()` 的驱动开发者，会在 PREEMPT_RT kernel 上触发 kernel BUG。如果驱动是为非 RT 环境编写的，且通篇使用自旋锁，就要审计每一个临界区，确保其中没有睡眠点。

---

## 测量调度延迟

要验证 RT 配置是否达到所需的延迟上限，需要测量工具。**`cyclictest`** 是测量 **RT 调度延迟** 的标准工具。

```bash
# Basic: 1 thread, SCHED_FIFO priority 99, nanosleep, 1ms interval, 10000 loops
cyclictest -t1 -p 99 -n -i 1000 -l 10000

# Histogram mode: 60-second run, histogram up to 200µs buckets
cyclictest --histogram=200 -D 60s -p 99 -n
```

**直方图模式** 对安全性分析至关重要。你关心的不只是平均延迟——你需要知道 **整个分布，尤其是尾部**。400µs 桶中的单个数据点就可能让 300µs 的要求不达标。

按应用领域划分的目标：

| 领域 | 最大可接受延迟 |
|---|---|
| AV 规划循环（软 RT） | <100µs |
| 机器人伺服控制 | <500µs |
| 电机驱动控制（硬 RT） | <50µs |
| 安全关键（ASIL-B 认证） | <20µs |

---

## 延迟来源

理解延迟从何而来，才能有效地定位修复点。来源分为两类：可控制的软件来源，以及需要固件或平台改动的硬件来源。

### 可量化的软件来源

- **IRQ 关闭区段**：`raw_spinlock_t` 保持时间、硬件寄存器访问序列；目标：<1µs
- **热路径上的内存分配**：`GFP_ATOMIC` 绕过直接回收，但仍会获取 zone 锁
- **缓存未命中 / TLB shootdown**：在大型 SMP 系统上，用于刷新远端 TLB 的 IPI 会带来约 10–30µs 的额外开销
- **CPU 频率切换**：`schedutil` governor 会在任务执行中途调整频率；切换最多增加约 200µs

### SMI（系统管理中断）

SMI 是最危险的延迟来源，因为操作系统看不到它们，且仅靠软件无法阻止。

- 由 BIOS/UEFI/BMC 固件生成，用于电源管理、热降频、ECC 内存巡检
- **对操作系统不可见**：CPU 进入 SMM（ring -2），OS 时钟停止，OS 无法观测或统计这段时间
- 典型影响：每次 SMI 事件 50–300µs；某些平台每秒触发 10–100 次 SMI
- **检测工具**：`hwlatdetect`——在紧循环中轮询硬件定时器；轮询间隔出现较大空隙即表明有 SMI

```bash
# Run for 60 seconds; report any gap larger than 20 microseconds
hwlatdetect --duration=60s --threshold=20
# If this reports violations, the platform firmware must be tuned — software alone cannot fix SMI latency
```

`hwlatdetect` 的原理是 **独占一个 CPU**，并测量硬件定时器读取之间的间隔。任何大于轮询间隔的空隙都表明 **某个不可见的东西（SMI）偷走了 CPU 时间**。

> **常见陷阱：** 在许多 x86 服务器平台上，ECC 内存巡检每隔几秒就会产生 SMI。在某些 BIOS 版本上这无法禁用，会带来 100–300µs 的尖峰。这必须在 bring-up（上电点亮/调通）阶段发现，而不是在 ASIL 认证测试开始之后。

### NUMA 与硬件效应

- 在双路服务器上，跨 NUMA 内存访问每次 LLC 未命中增加约 100ns
- CPU C-state 退出延迟：C1 约 1µs，C6 约 100µs；使用 `idle=poll` 或 `intel_idle.max_cstate=1` 可消除
- 空闲退出后 Turbo boost 频率稳定过程会带来不确定的延迟；`performance` governor 可避免这种情况

---

## RT 调优检查清单

RT 延迟调优是一个分层过程：kernel 配置奠定基础，启动参数隔离硬件，runtime 设置与应用代码完成最终定型。


<details>
<summary>English original</summary>

**Hardirq Threading**

- IRQ handlers become **threaded kernel threads** under RT
- Default thread priority: `SCHED_FIFO`, priority 50
- `IRQF_NO_THREAD` flag: opt-out for specific handlers that cannot sleep (e.g., the timer interrupt hardware path)
- Consequence: an RT task at priority 99 can preempt an IRQ handler running at priority 50

This is a conceptual shift. In standard Linux, interrupts are sacred — they preempt everything. Under PREEMPT_RT, interrupts are just high-priority threads, subject to the same scheduler rules as any other task.

**Softirq Handling**

- Softirqs run in `ksoftirqd` kernel threads (one per CPU)
- `ksoftirqd` is a normal preemptible task; RT tasks can preempt it at any point
- Prevents softirq storms from blocking high-priority RT tasks for unbounded time

**Sleeping in Kernel**

- Pre-RT: many kernel code paths had "cannot sleep here" constraints because spinlocks held preemption off
- With RT: sleeping is safe in most contexts because `spinlock_t` is now a sleeping lock
- Remaining `raw_spinlock_t` sections still cannot sleep; these must be kept extremely short

> **Common Pitfall:** Driver developers who call `msleep()` or `schedule()` while holding a `raw_spinlock_t` will cause a kernel BUG on PREEMPT_RT kernels. If your driver was written for non-RT and uses spinlocks throughout, audit every critical section to ensure it contains no sleep points.

---

**Measuring Scheduling Latency**

To verify that your RT configuration achieves the required latency bounds, you need a measurement tool. **`cyclictest`** is the standard tool for measuring **RT scheduling latency**.

```bash
# Basic: 1 thread, SCHED_FIFO priority 99, nanosleep, 1ms interval, 10000 loops
cyclictest -t1 -p 99 -n -i 1000 -l 10000

# Histogram mode: 60-second run, histogram up to 200µs buckets
cyclictest --histogram=200 -D 60s -p 99 -n
```

The **histogram mode** is critical for safety analysis. You are not just interested in average latency — you need to know the **entire distribution, especially the tail**. A single data point in the 400µs bucket can fail a 300µs requirement.

Targets by application domain:

| Domain | Max Acceptable Latency |
|---|---|
| AV planning loop (soft RT) | <100µs |
| Robotics servo control | <500µs |
| Motor drive control (hard RT) | <50µs |
| Safety-critical (ASIL-B certified) | <20µs |

---

**Latency Sources**

Understanding where latency comes from lets you target fixes effectively. Sources fall into two categories: software sources you can control and hardware sources that require firmware or platform changes.

**Quantifiable Software Sources**

- **IRQ disable sections**: `raw_spinlock_t` hold time, hardware register access sequences; goal: <1µs
- **Memory allocation on hot path**: `GFP_ATOMIC` bypasses direct reclaim but still acquires zone locks
- **Cache misses / TLB shootdowns**: IPI to flush remote TLBs on large SMP systems adds ~10–30µs
- **CPU frequency transitions**: `schedutil` governor can step frequency mid-task; transition adds up to ~200µs

**SMI (System Management Interrupts)**

SMIs are the most dangerous latency source because they are invisible to the OS and impossible to prevent from software alone.

- Generated by BIOS/UEFI/BMC firmware for power management, thermal throttling, ECC memory scrubbing
- **Invisible to the OS**: CPU enters SMM (ring -2), OS clock stops, OS cannot observe or account for this time
- Typical impact: 50–300µs per SMI event; some platforms fire 10–100 SMIs per second
- **Detection tool**: `hwlatdetect` — polls a hardware timer in a tight loop; large polling gaps indicate SMI

```bash
# Run for 60 seconds; report any gap larger than 20 microseconds
hwlatdetect --duration=60s --threshold=20
# If this reports violations, the platform firmware must be tuned — software alone cannot fix SMI latency
```

`hwlatdetect` works by **monopolizing a CPU** and measuring gaps between hardware timer reads. Any gap larger than the polling interval indicates **something invisible (an SMI) stole CPU time**.

> **Common Pitfall:** On many x86 server platforms, ECC memory scrubbing generates SMIs every few seconds. On some BIOS versions this cannot be disabled and adds 100–300µs spikes. This must be discovered during bring-up, not after ASIL certification testing begins.

**NUMA and Hardware Effects**

- Cross-NUMA memory access adds ~100ns per LLC miss on two-socket servers
- CPU C-state exit latency: C1 ~1µs, C6 ~100µs; use `idle=poll` or `intel_idle.max_cstate=1` to eliminate
- Turbo boost frequency settling after idle exit adds variable latency; `performance` governor avoids this

---

**RT Tuning Checklist**

Tuning RT latency is a layered process: kernel configuration sets the foundation, boot parameters isolate the hardware, and runtime settings and application code finalize the setup.

</details>

### Kernel 配置

```
CONFIG_PREEMPT_RT=y    # enable fully preemptible kernel
CONFIG_HZ_1000=y       # 1ms timer tick (higher resolution)
CONFIG_NO_HZ_FULL=y    # enable tickless operation on isolated CPUs
CONFIG_RCU_NOCB_CPU=y  # enable RCU callback offloading
```

### 启动参数

```
isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3 irqaffinity=0,1
```

- `isolcpus=`：将 CPU 从 kernel 调度器池中移除；任务必须用 `taskset` 显式放置
- `nohz_full=`：在隔离的 CPU 上禁用周期性调度器 tick（tickless 运行）
- `rcu_nocbs=`：将 RCU 回调卸载到运行在非隔离 CPU 上的 `rcuoc` 内核线程
- `irqaffinity=`：将所有硬件 IRQ 从隔离 CPU 上引开

这四个参数**协同工作**。`isolcpus` 把 CPU 从调度器中移除。`nohz_full` 阻止 kernel 每毫秒中断它一次。`rcu_nocbs` 阻止 RCU 在它上面触发回调。`irqaffinity` 阻止硬件中断落到它上面。合在一起，它们为 RT 任务营造出**近乎裸机的执行环境**。

### Runtime 调优

```bash
# Fix CPU frequency to maximum; eliminates frequency ramp-up jitter
cpupower frequency-set -g performance

# Disable NUMA auto-balancing; page migrations cause TLB shootdown jitter
echo 0 > /proc/sys/kernel/numa_balancing

# Disable transparent hugepages; async promotions cause unpredictable latency
echo never > /sys/kernel/mm/transparent_hugepage/enabled
```

### 应用层 RT 设置

```c
mlockall(MCL_CURRENT | MCL_FUTURE);  // pin ALL current + future pages in RAM
// Without this, the kernel can evict pages to swap at any time
// A single major page fault during inference adds 1-10ms of latency

// Set SCHED_FIFO policy with explicit priority
struct sched_param param = { .sched_priority = 80 };
sched_setscheduler(0, SCHED_FIFO, &param);
// SCHED_FIFO: once running, this task runs until it yields or is preempted by higher priority
// Priority 80 leaves room for priority 81-99 tasks (e.g., hardirq threads at 50)

// Pre-fault thread stack before entering RT loop
char stack_probe[8192];
memset(stack_probe, 0, sizeof(stack_probe));
// The kernel allocates stack pages lazily; touching them now forces physical allocation
// Without this, the first deep function call in the RT loop triggers a minor page fault

// Pre-allocate ALL buffers before entering the real-time loop
// No malloc() calls inside the RT loop
// malloc() calls brk()/mmap() which may trigger page faults and take kernel locks
```

这套设置序列必须在进入实时循环之前完成。可以把它看作“起飞前检查”——所有可能引发延迟的事情都在初始化阶段被刻意触发，这样它们就不会在时间关键阶段意外发生。

> **常见陷阱：** 调用 `mlockall()` 之后不继续预先触碰所有页，只能防止未来的换出——尚未缺页调入的页在首次访问时仍会按需分页。在 `mlockall()` 之后一定要用 `memset` 或类似方式触碰 RT 循环中会用到的所有缓冲区。

---

## ftrace 延迟追踪器

当 `cyclictest` 显示出延迟尖峰，而你需要找出确切是哪个 kernel 代码路径导致时，`ftrace` 给出答案。

```bash
echo 0 > /sys/kernel/tracing/tracing_on         # stop tracing while configuring
echo latency > /sys/kernel/tracing/current_tracer  # select the latency tracer
echo 1 > /sys/kernel/tracing/tracing_on         # start tracing
# ... wait for or trigger a latency event ...
cat /sys/kernel/tracing/trace                   # read the captured trace
```

输出包括：时间戳、最坏情况唤醒延迟，以及从唤醒触发到任务恢复的完整 kernel 调用栈。可定位到造成所观测最坏延迟尖峰的确切函数。

> **关键洞见：** `cyclictest` 告诉你延迟违规*发生了*。`ftrace` 告诉你时间花在了 kernel 的*哪里*。两个工具都需要：cyclictest 确认系统满足其截止时间，ftrace 在系统不满足时诊断违规。

---


<details>
<summary>English original</summary>

**Kernel Configuration**

```
CONFIG_PREEMPT_RT=y    # enable fully preemptible kernel
CONFIG_HZ_1000=y       # 1ms timer tick (higher resolution)
CONFIG_NO_HZ_FULL=y    # enable tickless operation on isolated CPUs
CONFIG_RCU_NOCB_CPU=y  # enable RCU callback offloading
```

**Boot Parameters**

```
isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3 irqaffinity=0,1
```

- `isolcpus=`: removes CPUs from kernel scheduler pool; tasks must be explicitly placed with `taskset`
- `nohz_full=`: disables the periodic scheduler tick on isolated CPUs (tickless operation)
- `rcu_nocbs=`: offloads RCU callbacks to `rcuoc` kthreads running on non-isolated CPUs
- `irqaffinity=`: routes all hardware IRQs away from isolated CPUs

These four parameters **work together**. `isolcpus` removes the CPU from the scheduler. `nohz_full` stops the kernel from interrupting it every millisecond. `rcu_nocbs` stops RCU from firing callbacks on it. `irqaffinity` stops hardware interrupts from landing on it. Together, they create a **nearly bare-metal execution environment** for your RT task.

**Runtime Tuning**

```bash
# Fix CPU frequency to maximum; eliminates frequency ramp-up jitter
cpupower frequency-set -g performance

# Disable NUMA auto-balancing; page migrations cause TLB shootdown jitter
echo 0 > /proc/sys/kernel/numa_balancing

# Disable transparent hugepages; async promotions cause unpredictable latency
echo never > /sys/kernel/mm/transparent_hugepage/enabled
```

**Application-Level RT Setup**

```c
mlockall(MCL_CURRENT | MCL_FUTURE);  // pin ALL current + future pages in RAM
// Without this, the kernel can evict pages to swap at any time
// A single major page fault during inference adds 1-10ms of latency

// Set SCHED_FIFO policy with explicit priority
struct sched_param param = { .sched_priority = 80 };
sched_setscheduler(0, SCHED_FIFO, &param);
// SCHED_FIFO: once running, this task runs until it yields or is preempted by higher priority
// Priority 80 leaves room for priority 81-99 tasks (e.g., hardirq threads at 50)

// Pre-fault thread stack before entering RT loop
char stack_probe[8192];
memset(stack_probe, 0, sizeof(stack_probe));
// The kernel allocates stack pages lazily; touching them now forces physical allocation
// Without this, the first deep function call in the RT loop triggers a minor page fault

// Pre-allocate ALL buffers before entering the real-time loop
// No malloc() calls inside the RT loop
// malloc() calls brk()/mmap() which may trigger page faults and take kernel locks
```

This setup sequence must happen before entering the real-time loop. Think of it as the "pre-flight check" — everything that could cause latency is triggered deliberately during initialization, so it cannot happen unexpectedly during the time-critical phase.

> **Common Pitfall:** Calling `mlockall()` without subsequently pre-touching all pages only prevents future eviction — pages not yet faulted in are still demand-paged on first access. Always follow `mlockall()` with a `memset` or similar touch of all buffers you will use in the RT loop.

---

**ftrace Latency Tracer**

When `cyclictest` shows a latency spike and you need to find the exact kernel code path responsible, `ftrace` provides the answer.

```bash
echo 0 > /sys/kernel/tracing/tracing_on         # stop tracing while configuring
echo latency > /sys/kernel/tracing/current_tracer  # select the latency tracer
echo 1 > /sys/kernel/tracing/tracing_on         # start tracing
# ... wait for or trigger a latency event ...
cat /sys/kernel/tracing/trace                   # read the captured trace
```

Output includes: timestamp, worst-case wakeup latency, and full kernel call stack from wakeup trigger to task resumption. Identifies the exact function responsible for the worst observed latency spike.

> **Key Insight:** `cyclictest` tells you *that* a latency violation occurred. `ftrace` tells you *where* in the kernel the time was spent. You need both tools: cyclictest to confirm the system meets its deadline, and ftrace to diagnose violations when it doesn't.

---

</details>

## QNX：商用 RTOS 参考

QNX 代表了另一种设计取向：不是把通用 OS 改造为实时，而是从一开始就把实时内建进去。

- 微内核架构：驱动、文件系统与网络栈作为隔离的用户空间进程运行
- 从最初设计起就完全可抢占 —— 不像 Linux 上的 PREEMPT_RT 那样需要改造
- 部署场景：QNX CAR 平台、BlackBerry IVY、医疗设备、航电飞行管理
- 自适应分区调度器：为每个分区预留 CPU 时间预算，即使在过载下也保证最低额度
- 相关性：QNX + hypervisor 的搭配（例如在 NXP S32G、TI TDA4VM 上）可在同一 SoC 上把安全认证的 RTOS 与 Linux ADAS 栈隔离

实践中，现代 AV 平台往往**两者都跑**：QNX 负责**安全关键的实时控制**（制动、转向），Linux 负责 **AI 推理栈**。**hypervisor** 在两者之间提供硬件隔离。

---

## 小结

| 配置 | 最大延迟（典型） | 适用场景 | 取舍 |
|---|---|---|---|
| `PREEMPT_NONE` | >1ms | 批计算、服务器 | 吞吐最高 |
| `PREEMPT_VOLUNTARY` | ~500µs | 通用 Linux | 开销极小 |
| `PREEMPT` | ~100–200µs | 桌面、轻量实时 | 开销中等 |
| `PREEMPT_RT` | <50µs | 机器人、AV、电机控制 | 吞吐略降 |
| QNX | <10µs | 硬实时、安全认证 | 需要商业许可 |

### 概念回顾

- **为什么「快」不等于「实时」？** 系统可以有很低的平均延迟，但偶尔出现 500µs 的尖峰。实时要求最坏情况是*有界的* —— 尖峰必须被消除，而不只是变得罕见。
- **PREEMPT_RT 到底改变了内核的什么？** 它把 `spinlock_t` 转换为可睡眠的 `rtmutex`，把 IRQ 处理程序移入可调度的线程，并使 softirq 处理可抢占 —— 从而消除内核持有时间无界的三大来源。
- **为什么 PREEMPT_RT 内核中仍然需要 `raw_spinlock_t`？** 某些硬件关键路径（例如定时器中断入口、per-CPU 计数器更新）确实无法睡眠。`raw_spinlock_t` 为这些罕见情形保留了真正的 spinlock 语义，且必须把持有时间控制在亚微秒级。
- **`isolcpus` 到底做什么？** 它在启动时把一个 CPU 从内核的通用调度器池中移除。除非通过 `taskset` 显式指定，否则不会有任务被放到该 CPU 上。与 `nohz_full` 和 `rcu_nocbs` 结合，可为绑定的实时任务创造接近裸机的执行环境。
- **为什么要在 `cyclictest` 之前运行 `hwlatdetect`？** SMI 事件对 cyclictest 不可见 —— 从操作系统视角看，CPU 消失了。如果 hwlatdetect 报告违规，再怎么调内核都修不好。必须先修改平台固件。
- **为什么调用 `mlockall()` 之后还要 memset 所有缓冲区？** `mlockall(MCL_FUTURE)` 能防止后续被换出，但不会把尚未访问的页调入。预先触碰会强制物理分配，从而把次要缺页与主要缺页都从实时执行路径上消除。

---

## AI 硬件关联

- openpilot `controlsd` 需要 `PREEMPT_RT`：向车辆总线写 CAN 帧必须在 10ms 内完成，否则安全看门狗会触发受控退出；不能允许调度器抖动违反这一界限
- `cyclictest` 的直方图输出直接输入 ISO 26262 ASIL-B 时序分析；直方图尾部（观测到的最坏延迟）必须落在每个安全功能所分配的 WCET 预算之内
- Jetson Orin 上的 `isolcpus` + `nohz_full`（例如 4–11 核用于 DNN 推理，0–3 核用于 OS）可避免 Linux 的后台抖动表现为推理时序回路中的延迟尖峰
- 在任何实时推理进程中，`mlockall(MCL_CURRENT|MCL_FUTURE)` 都是强制要求；模型执行期间哪怕一次主要缺页，也会额外引入 1–10ms 的非预期延迟，违反硬截止时间
- `hwlatdetect` 在 ECU bring-up（上电点亮/调通）期间运行，用于定位嵌入式 x86 平台上的 SMI 来源；固件厂商必须限定或消除 SMI 延迟，才能取得 ASIL 认证
- 在隔离的推理核上运行 `rcu_nocbs=`，可消除 RCU 回调调用，否则这些调用会在推理循环的时序窗口内表现为随机的数微秒延迟尖峰


<details>
<summary>English original</summary>

**QNX: Commercial RTOS Reference**

QNX represents the alternative design point: rather than retrofitting a general-purpose OS for real-time, build RT in from the start.

- Microkernel architecture: drivers, filesystems, and network stack run as isolated user-space processes
- Fully preemptive from initial design — no retrofit required, unlike PREEMPT_RT on Linux
- Deployment: QNX CAR platform, BlackBerry IVY, medical devices, avionics flight management
- Adaptive partitioning scheduler: CPU time budget reserved per partition with guaranteed minimums even under overload
- Relevance: QNX + hypervisor pairing (e.g., on NXP S32G, TI TDA4VM) isolates safety-certified RTOS from Linux ADAS stack on the same SoC

In practice, modern AV platforms often run **both**: QNX handles **safety-critical real-time control** (brakes, steering), while Linux runs the **AI inference stack**. A **hypervisor** provides hardware isolation between the two.

---

**Summary**

| Config | Max Latency (Typical) | Suitable For | Tradeoff |
|---|---|---|---|
| `PREEMPT_NONE` | >1ms | Batch compute, servers | Highest throughput |
| `PREEMPT_VOLUNTARY` | ~500µs | General Linux | Minimal overhead |
| `PREEMPT` | ~100–200µs | Desktop, light RT | Moderate overhead |
| `PREEMPT_RT` | <50µs | Robotics, AV, motor control | Small throughput reduction |
| QNX | <10µs | Hard RT, safety-certified | Commercial license required |

**Conceptual Review**

- **Why doesn't "fast" mean "real-time"?** A system can have low average latency but occasional spikes of 500µs. RT requires a *bounded* worst case — the spike must be eliminated, not just made rare.
- **What does PREEMPT_RT actually change in the kernel?** It converts `spinlock_t` to sleeping `rtmutex`, moves IRQ handlers into schedulable threads, and makes softirq processing preemptible — eliminating the three main sources of unbounded kernel hold time.
- **Why is `raw_spinlock_t` still needed in a PREEMPT_RT kernel?** Some hardware-critical paths (e.g., timer interrupt entry, per-CPU counter updates) genuinely cannot sleep. `raw_spinlock_t` preserves true spinlock semantics for those rare cases and must be kept to sub-microsecond hold times.
- **What does `isolcpus` actually do?** It removes a CPU from the kernel's general scheduler pool at boot time. No task is placed on that CPU unless explicitly assigned via `taskset`. Combined with `nohz_full` and `rcu_nocbs`, it creates near-bare-metal execution for pinned RT tasks.
- **Why run `hwlatdetect` before `cyclictest`?** SMI events are invisible to cyclictest — the CPU disappears from the OS perspective. If hwlatdetect shows violations, no amount of kernel tuning will fix them. Platform firmware must be changed first.
- **Why call `mlockall()` and then memset all buffers?** `mlockall(MCL_FUTURE)` prevents future eviction but does not fault in pages that haven't been accessed yet. Pre-touching forces physical allocation, eliminating both minor and major page faults from the RT execution path.

---

**AI Hardware Connection**

- `PREEMPT_RT` is required for openpilot `controlsd`: CAN frame writes to the vehicle bus must complete within 10ms or the safety watchdog triggers a controlled disengage; scheduler jitter cannot be allowed to violate this bound
- `cyclictest` histogram output feeds directly into ISO 26262 ASIL-B timing analysis; the histogram tail (worst observed latency) must fall within the allocated WCET budget for each safety function
- `isolcpus` + `nohz_full` on Jetson Orin (e.g., cores 4–11 for DNN inference, 0–3 for OS) prevents Linux background jitter from appearing as latency spikes in the inference timing loop
- `mlockall(MCL_CURRENT|MCL_FUTURE)` is mandatory in any real-time inference process; a single major page fault during model execution can add 1–10ms of unexpected latency, violating hard deadlines
- `hwlatdetect` is run during ECU bring-up to locate SMI sources on embedded x86 platforms; firmware vendors must bound or eliminate SMI latency to achieve ASIL certification
- `rcu_nocbs=` on isolated inference cores removes RCU callback invocations that would otherwise appear as random multi-microsecond latency spikes inside the inference loop timing window

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
