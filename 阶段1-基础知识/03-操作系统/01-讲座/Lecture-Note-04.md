---
title: 讲义 04（L7、L8、L9）：实时 Linux、多核调度与同步
description: 讲义 04（L7、L8、L9）：实时 Linux、多核调度与同步
---

# 讲义 04（L7、L8、L9）：实时 Linux、多核调度与同步

---

## 全局图景：为什么这很重要

想象一条机械臂，在检测到人时必须于 2 ms 内停下。如果 OS 因为忙于别的事情，有时把你的“停止”命令延迟了 5 ms，那么系统就是不安全的——即使它通常很快。**可预测性**（最坏情况行为）比平均速度更重要。

本笔记讲的是让系统变得可预测的三件事：

| 主题 | 它回答的问题 |
|-------|-------------------------|
| **Part 1：实时 Linux** | “如何让 OS 在保证的时间内响应？” |
| **Part 2：多核与 CPU 布局** | “如何让关键任务待在正确的 CPU 上，并避免来自调度器的随机延迟？” |
| **Part 3：同步** | “如何让多个线程安全地共享数据，而不出现竞态或死锁？” |

**一句话总结：** 你需要可预测的 kernel（Part 1）、任务在 CPU 上可预测的*布局*（Part 2），以及正确使用锁，使高优先级工作不会卡在低优先级工作之后（Part 3）。

---

## 本笔记的组织方式

1. **Part 1 — 实时 Linux：** 有界响应时间：抢占模型、PREEMPT_RT、延迟的测量与调优。
2. **Part 2 — 多核调度：** 控制哪个任务在哪个 CPU 上运行；affinity、isolation、NUMA。
3. **Part 3 — 同步：** spinlock、mutex、read/write lock、seqlock、completion 与 lockdep。

**你会看到的术语：**  
- *Latency* = 从“某件事发生”到“作出响应”之间的延迟。  
- *Preemption* = 更高优先级的任务打断当前任务。  
- *Kernel* = 以完整硬件访问权限运行的核心 OS 代码。  
- *IRQ* = 硬件中断（例如设备完成 DMA、定时器 tick）。
- *Top half* = 中断处理的第一部分：中断触发时立即运行的 IRQ 处理程序；必须非常短（例如确认硬件、把工作排队）。*Bottom half* = 在上半部之后运行的延迟工作（softirqs、tasklets、workqueues）；可以完成更多工作，而不必在中断上下文中占着 CPU。
- *Critical section* = 访问共享数据、且必须在持有锁的情况下运行的代码。

**如何使用本笔记：** 先读 **全局图景** 与 **本笔记的组织方式**。然后直接跳到你需要的那一部分（实时、多核或同步）。每一部分在章节开头都有简短的 **Context** 行，在展开细节前说明该主题*为什么*重要。

---

# Part 1：实时 Linux（PREEMPT_RT 与确定性）

**Context：** 普通 Linux 针对吞吐进行调优（完成大量工作）。实时则针对**有保证的**响应进行调优：“无论发生什么，都在 X 微秒内作出响应。”

---

## “实时”到底意味着什么

**实时并不意味着“快”。** 它意味着**有界的最坏情况延迟**：存在截止时间，系统必须*始终*满足它。

| 类型 | 含义 | 示例 |
|------|--------|--------|
| **Soft RT** | 错过截止时间会损害质量 | 音频丢帧、视频延迟 |
| **Hard RT** | 错过截止时间即为失败 | 电机过冲、刹车过晚 |

真正重要的指标是**最坏情况延迟**，而非平均值。即使平均值是 10 µs，一次 1 ms 的尖峰也会打破 500 µs 的硬截止时间。

> **要点：** PREEMPT_RT 并不能让 Linux 更快；它让 Linux 更**可预测**。你关心的是最坏延迟，而不是平均延迟。

---

## Linux 抢占模型

**抢占**的含义是：“更高优先级的任务能否打断当前正在运行的任务？”  
- **无抢占：** kernel 可以长时间运行，而不把 CPU 让给你的 RT 任务。  
- **完全抢占（PREEMPT_RT）：** kernel 几乎在任何地方都可被中断，因此你的 RT 任务能很快运行。

这在内核**构建**时通过 `CONFIG_PREEMPT_*` 选择：

| 配置 | kernel 是否可被抢占？ | 典型最坏情况延迟 | 适用场景 |
|--------|---------------------------|---------------------------|----------|
| `PREEMPT_NONE` | 几乎不能 | >1 ms | 服务器、最大吞吐 |
| `PREEMPT_VOLUNTARY` | 仅在特定让出点 | ~500 µs | 通用桌面 |
| `PREEMPT` | 大部分 kernel 代码 | ~100–200 µs | 交互式桌面 |
| `PREEMPT_RT` | 几乎整个 kernel | 可低至 <50 µs | 机器人、电机控制、AV |

**PREEMPT_RT** 已在 **Linux 6.12**（2024 年末）合入主线。在此之前，它长期是一个树外补丁。

**直观理解：** 把 kernel 想象成带有若干“禁止进入”区域，在这些区域里任何东西都无法中断。PREEMPT_NONE 有很多这样的区域；PREEMPT_RT 几乎没有，因此你的 RT 任务能更早运行。

---

## PREEMPT_RT 如何工作（三项主要改动）

在普通 kernel 中，有三样东西会长时间阻塞你的 RT 任务：spinlock（CPU 忙等）、中断处理程序和 softirq。PREEMPT_RT 分别修复了这三者。


<details>
<summary>English original</summary>

**Lecture Note 04 (L7, L8, L9): Real-Time Linux, Multi-Core Scheduling & Synchronization**

---

**Big Picture: Why This Matters**

Imagine a robot arm that must stop within 2 ms when it detects a person. If the OS sometimes delays your “stop” command by 5 ms because it was busy with something else, the system is unsafe — even if it’s usually fast. **Predictability** (worst-case behavior) matters more than average speed.

This note is about three things that make systems predictable:

| Topic | The question it answers |
|-------|-------------------------|
| **Part 1: Real-Time Linux** | “How do I make the OS respond within a guaranteed time?” |
| **Part 2: Multi-Core & CPU Placement** | “How do I keep my critical task on the right CPU and avoid random delays from the scheduler?” |
| **Part 3: Synchronization** | “How do I let multiple threads share data safely without races or deadlocks?” |

**One-line summary:** You need a predictable kernel (Part 1), predictable *placement* of your task on CPUs (Part 2), and correct use of locks so high-priority work isn’t stuck behind low-priority work (Part 3).

---

**How This Note Is Organized**

1. **Part 1 — Real-Time Linux:** Bounded response time: preemption models, PREEMPT_RT, measuring and tuning latency.
2. **Part 2 — Multi-Core Scheduling:** Controlling which task runs on which CPU; affinity, isolation, NUMA.
3. **Part 3 — Synchronization:** Spinlocks, mutexes, read/write locks, seqlocks, completions, and lockdep.

**Terms you’ll see:**  
- *Latency* = delay between “something happened” and “we responded.”  
- *Preemption* = a higher-priority task interrupting the current one.  
- *Kernel* = the core OS code that runs with full hardware access.  
- *IRQ* = hardware interrupt (e.g. device finished DMA, timer tick).
- *Top half* = the first part of interrupt handling: the IRQ handler that runs immediately when the interrupt fires; must be very short (e.g. acknowledge hardware, queue work). *Bottom half* = deferred work run after the top half (softirqs, tasklets, workqueues); can do more work without holding the CPU in interrupt context.
- *Critical section* = code that touches shared data and must run with a lock held.

**How to use this note:** Read the **Big Picture** and **How This Note Is Organized** first. Then go to the part you need (Real-Time, Multi-Core, or Synchronization). Each part has short **Context** lines at the start of sections to explain *why* the topic matters before the details.

---

**Part 1: Real-Time Linux (PREEMPT_RT & Determinism)**

**Context:** Normal Linux is tuned for throughput (get lots of work done). Real-time is tuned for **guaranteed** response: “no matter what, we respond within X microseconds.”

---

**What “Real-Time” Really Means**

**Real-time does not mean “fast.”** It means **bounded worst-case latency**: there is a deadline, and the system must *always* meet it.

| Type | Meaning | Example |
|------|--------|--------|
| **Soft RT** | Missing a deadline hurts quality | Dropped audio frame, delayed video |
| **Hard RT** | Missing a deadline is a failure | Motor overshoot, brake too late |

The metric that matters is **worst-case latency**, not average. One 1 ms spike breaks a 500 µs hard deadline even if the average is 10 µs.

> **Takeaway:** PREEMPT_RT does not make Linux faster; it makes it more **predictable**. You care about the worst delay, not the average.

---

**Linux Preemption Models**

**Preemption** means: “Can a higher-priority task interrupt the one currently running?”  
- **No preemption:** The kernel can run for a long time without giving the CPU to your RT task.  
- **Full preemption (PREEMPT_RT):** The kernel can be interrupted almost everywhere, so your RT task can run soon.

This is chosen when the kernel is **built**, via `CONFIG_PREEMPT_*`:

| Config | Can kernel be preempted? | Typical worst-case delay | Good for |
|--------|---------------------------|---------------------------|----------|
| `PREEMPT_NONE` | Almost no | >1 ms | Servers, max throughput |
| `PREEMPT_VOLUNTARY` | Only at specific yield points | ~500 µs | General desktop |
| `PREEMPT` | Most kernel code | ~100–200 µs | Interactive desktop |
| `PREEMPT_RT` | Almost entire kernel | <50 µs possible | Robotics, motor control, AV |

**PREEMPT_RT** was merged into mainline in **Linux 6.12** (late 2024). Before that it was a long-standing out-of-tree patch.

**Simple picture:** Think of the kernel as having “no entry” zones where nothing can interrupt. PREEMPT_NONE has many such zones; PREEMPT_RT has almost none, so your RT task can run sooner.

---

**How PREEMPT_RT Works (Three Main Changes)**

In a normal kernel, three things can block your RT task for a long time: spinlocks (CPU busy-waiting), interrupt handlers, and softirqs. PREEMPT_RT fixes each one.

</details>

### 1. 自旋锁变成“可睡眠”锁（rtmutex）

**spinlock**（自旋锁）是一种锁：若锁被占用，CPU 就在循环中等待（“自旋”），而不去做别的事。在普通 kernel 中，这还会关闭抢占，因此在锁被释放前，你的 RT 任务无法在该 CPU 上运行。

- **普通 kernel：** `spin_lock()` = 忙等 + 关闭抢占。该 CPU 被“卡住”，直到锁被释放。
- **PREEMPT_RT：** `spin_lock()` 被替换为**可睡眠**的锁（rtmutex）。若锁被占用，任务会**睡眠**（让出 CPU），因此更高优先级的任务可以运行。锁空闲时，任务被唤醒。
- **`raw_spinlock_t`：** 例外——对极小的硬件关键区段（例如定时器中断入口）仍保留真正的自旋锁。持有它时绝不能睡眠。

**通俗地说：** 在 RT 下，大多数“自旋锁”不再独占 CPU；它们让调度器去运行你的 RT 任务。这就是最坏情况延迟下降的原因。

### 2. 中断处理程序变成线程（硬中断线程化）

**背景：** Linux 把中断处理拆成**上半部**（中断触发时立即运行的 IRQ 处理程序——必须非常短，例如确认硬件并排队工作）和**下半部**（延后执行的工作，如 softirq 或 tasklet）。在普通 Linux 中，硬件中断（IRQ）触发时，CPU 立即运行**上半部**，且它不会被你的 RT 任务抢占。这会带来不可预测的延迟。

- 在 PREEMPT_RT 下，**IRQ 处理程序（上半部）作为普通内核线程运行**（默认优先级 50）。
- 因此优先级 99 的 RT 任务**可以抢占** IRQ 处理程序。中断不再是“碰不得的”；它们遵守同一个调度器。
- 确实不能睡眠的处理程序用 `IRQF_NO_THREAD` 保留旧行为。

### 3. softirq 运行在可抢占的线程中（下半部）

**背景：** **下半部**是 kernel 在中断之后做延后工作的地方（例如网络 RX 处理、块 I/O 完成）。常见的一种形式是 **softirq**。在普通 kernel 中，它们可能运行很长时间，阻塞你的 RT 任务。

- 在 PREEMPT_RT 下，softirq 运行在 `ksoftirqd` **线程**内（每个 CPU 一个），这些是普通可抢占任务。
- 因此 RT 任务可以抢占 softirq 工作，避免“softirq 风暴”带来的长时间、无界延迟。

---

## 测量调度延迟：cyclictest

**为什么要测量？** 你需要*证明*最坏情况延迟低于你的要求（例如 300 µs）。**cyclictest** 是标准工具：它以固定间隔唤醒，并测量实际唤醒晚了多少。

```bash
# Basic: 1 thread, highest RT priority, 1 ms interval, 10000 loops
cyclictest -t1 -p 99 -n -i 1000 -l 10000

# Histogram: 60 s run, buckets up to 200 µs (see full distribution, including worst case)
cyclictest --histogram=200 -D 60s -p 99 -n
```


**为什么用直方图模式？** **平均**延迟可能看上去没问题，但少数罕见的尖峰就会打破你的截止时间。直方图展示完整分布——包括最坏情况。有 1 个样本落在 400 µs 桶里，就意味着你不满足 300 µs 的要求。

**各领域的粗略目标**（最大可接受延迟）：

| 领域 | 最大可接受延迟 |
|--------|------------------------|
| 自动驾驶规划（软 RT） | <100 µs |
| 机器人伺服 | <500 µs |
| 电机控制（硬 RT） | <50 µs |
| 安全关键（例如 ASIL-B） | <20 µs |

---

## 延迟从何而来

当 cyclictest 显示尖峰时，延迟来自某个地方。要修好它，就得知道可能的来源。

### 软件（你可以改进）

- **关中断 / raw_spinlock 区段** — 保持在 ~1 µs 以内。
- **热路径上的内存分配** — 即使是 `GFP_ATOMIC` 也可能获取锁。
- **缓存缺失 / TLB shootdown** — 大型 SMP 系统上的 IPI 可能增加 ~10–30 µs。
- **CPU 频率变化** — 例如 `schedutil` 切换频率可能增加最多 ~200 µs。

### SMI（系统管理中断）—— 常常不可见

**背景：** 有些中断由**固件**（BIOS/UEFI）处理，而不是 OS。CPU 进入特殊模式（SMM）；OS 的时钟实际上停止。OS 看不到也无法修复。

- SMI 用于电源管理、热管理降频、ECC 内存巡检等。
- 典型开销：每次事件 50–300 µs；某些平台每秒触发 10–100 次 SMI。
- **工具：** `hwlatdetect` — 运行紧凑循环读取硬件定时器；若两次读取之间出现大间隔，说明有不可见的东西（SMI）抢占了 CPU。

```bash
hwlatdetect --duration=60s --threshold=20
```

如果它报告违例，**仅靠软件调优是不够的**；固件/BIOS 可能需要改动（例如 ECC 巡检）。


<details>
<summary>English original</summary>

**1. Spinlocks Become “Sleeping” Locks (rtmutex)**

A **spinlock** is a lock where, if the lock is busy, the CPU just waits in a loop (“spins”) instead of doing something else. In the normal kernel that also disables preemption, so your RT task can’t run on that CPU until the lock is released.

- **Normal kernel:** `spin_lock()` = busy-wait + preemption off. That CPU is “stuck” until the lock is released.
- **PREEMPT_RT:** `spin_lock()` is replaced by a **sleeping** lock (rtmutex). If the lock is busy, the task **sleeps** (gives up the CPU), so a higher-priority task can run. When the lock is free, the task is woken.
- **`raw_spinlock_t`:** The exception — stays a real spinlock for tiny hardware-critical sections (e.g. timer interrupt entry). Never sleep while holding this.

**In plain English:** Under RT, most “spinlocks” no longer hog the CPU; they let the scheduler run your RT task. That’s why worst-case latency drops.

**2. Interrupt Handlers as Threads (Hardirq Threading)**

**Context:** Linux splits interrupt handling into a **top half** (the IRQ handler that runs immediately when the interrupt fires — must be very short, e.g. acknowledge hardware and queue work) and a **bottom half** (deferred work such as softirqs or tasklets). In normal Linux, when a hardware interrupt (IRQ) fires, the CPU runs the **top half** immediately and it can’t be preempted by your RT task. That can add unpredictable delay.

- Under PREEMPT_RT, **IRQ handlers (top half) run as normal kernel threads** (default priority 50).
- So an RT task at priority 99 **can preempt** an IRQ handler. Interrupts are no longer “untouchable”; they obey the same scheduler.
- Handlers that truly cannot sleep keep the old behavior with `IRQF_NO_THREAD`.

**3. Softirqs in Preemptible Threads (Bottom Half)**

**Context:** The **bottom half** is where the kernel does deferred work after an interrupt (e.g. network RX processing, block I/O completion). One common form is **softirqs**. In the normal kernel they can run for a long time and block your RT task.

- Under PREEMPT_RT, softirqs run inside `ksoftirqd` **threads** (one per CPU), which are normal preemptible tasks.
- So RT tasks can preempt softirq work and avoid long, unbounded delays from “softirq storms.”

---

**Measuring Scheduling Latency: cyclictest**

**Why measure?** You need to *prove* that worst-case delay stays under your requirement (e.g. 300 µs). **cyclictest** is the standard tool: it wakes up at a fixed interval and measures how late the wakeup actually was.

```bash
# Basic: 1 thread, highest RT priority, 1 ms interval, 10000 loops
cyclictest -t1 -p 99 -n -i 1000 -l 10000

# Histogram: 60 s run, buckets up to 200 µs (see full distribution, including worst case)
cyclictest --histogram=200 -D 60s -p 99 -n
```

**Why use histogram mode?** The **average** latency can look fine while a few rare spikes break your deadline. The histogram shows the full distribution — including the worst case. One sample in the 400 µs bucket means you fail a 300 µs requirement.

**Rough targets by domain** (max acceptable delay):

| Domain | Max acceptable latency |
|--------|------------------------|
| AV planning (soft RT) | <100 µs |
| Robotics servo | <500 µs |
| Motor control (hard RT) | <50 µs |
| Safety-critical (e.g. ASIL-B) | <20 µs |

---

**Where Latency Comes From**

When cyclictest shows a spike, the delay came from somewhere. Fixing it means knowing the possible sources.

**Software (you can improve)**

- **IRQ-off / raw_spinlock sections** — Keep under ~1 µs.
- **Memory allocation on hot path** — Even `GFP_ATOMIC` can take locks.
- **Cache misses / TLB shootdowns** — IPIs on big SMP systems can add ~10–30 µs.
- **CPU frequency changes** — e.g. `schedutil` stepping frequency can add up to ~200 µs.

**SMI (System Management Interrupts) — Often Invisible**

**Context:** Some interrupts are handled by the **firmware** (BIOS/UEFI), not the OS. The CPU enters a special mode (SMM); the OS’s clock effectively stops. The OS cannot see or fix this.

- SMIs are used for power management, thermal throttling, ECC memory scrubbing, etc.
- Typical cost: 50–300 µs per event; some platforms fire 10–100 SMIs per second.
- **Tool:** `hwlatdetect` — runs a tight loop reading a hardware timer; if there’s a big gap between reads, something invisible (SMI) stole the CPU.

```bash
hwlatdetect --duration=60s --threshold=20
```

If this reports violations, **software tuning alone is not enough**; firmware/BIOS may need changes (e.g. ECC scrubbing).

</details>

### 硬件 / 平台

- **NUMA:** 在多 socket 机器上，访问另一个 socket 上的内存更慢。每次缓存未命中，跨 socket 访问都会带来额外开销。
- **CPU idle（C-states）：** 更深的睡眠（如 C6）省电，但唤醒更慢（如 ~100 µs）。对 RT 而言，用 `idle=poll` 或 `intel_idle.max_cstate=1` 限制深度。
- **Turbo / frequency:** 如果 CPU 在任务执行中途改变频率，时序就变得不可预测。使用 `performance` governor，让频率保持在最高。

---

## 实时调优检查清单

调优是分层的：**kernel config** 奠定基础，**boot parameters** 预留并隔离 CPU，**runtime** 设置固定频率与内存行为，而**你的应用**必须锁定内存，并避免在 RT 循环中做分配。

### Kernel config（示例）

```
CONFIG_PREEMPT_RT=y
CONFIG_HZ_1000=y
CONFIG_NO_HZ_FULL=y
CONFIG_RCU_NOCB_CPU=y
```

### Boot parameters（示例：隔离 CPU 2,3）

```
isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3 irqaffinity=0,1
```

| 参数 | 作用 |
|-----------|--------|
| `isolcpus=` | 这些 CPU 不被通用调度器使用；只有你绑定的任务才会跑在上面。 |
| `nohz_full=` | 这些 CPU 上没有周期性 timer tick（中断更少）。 |
| `rcu_nocbs=` | RCU 回调在其他 CPU 上运行，不在隔离的 CPU 上。 |
| `irqaffinity=` | 硬件 IRQ 只发往 CPU 0,1，不发往隔离的 CPU。 |

### Runtime

```bash
cpupower frequency-set -g performance
echo 0 > /proc/sys/kernel/numa_balancing
echo never > /sys/kernel/mm/transparent_hugepage/enabled
```

### 在你的 RT 应用中（实时循环之前）

**上下文：** kernel 可能把你的进程内存换出到磁盘，或延迟分配。RT 循环中发生一次 **page fault**（从磁盘取回页面或分配页面）就会增加 1–10 ms，直接打破你的截止时间。所以，所有“有风险”的工作都要在进入时间关键循环之前做完。

- **锁定所有内存：** `mlockall(MCL_CURRENT | MCL_FUTURE)` —— 告诉 kernel 不要把你的页面换出。（如果页面已被换出而你访问了它，就会触发 major fault = 巨大延迟。）
- **使用 SCHED_FIFO：** 例如优先级 80 —— 你的任务按实时方式调度，可以抢占普通任务。
- **预先 fault 栈和缓冲区：** 触碰你将用到的每一个页面（例如 `memset` 栈和缓冲区）。这会强制 kernel *现在* 就分配它们，使之后 RT 循环中不再发生 fault。
- **RT 循环中不要 malloc：** 所有东西都在循环之前分配；循环内部不要用 `malloc`（它会加锁并触发 fault）。

> **陷阱：** `mlockall()` 只能防止*换出*；它不会加载从未被触碰过的页面。你必须**触碰**这些页面（例如用 `memset`），让它们真正驻留在 RAM 中。

---

## 定位延迟尖峰的成因：ftrace

- **cyclictest** 告诉你尖峰*确实发生了*（以及它有多大）。
- **ftrace**（latency tracer）告诉你时间花在 kernel 的*哪里* —— 延迟期间正在执行哪个函数。

```bash
echo 0 > /sys/kernel/tracing/tracing_on
echo latency > /sys/kernel/tracing/current_tracer
echo 1 > /sys/kernel/tracing/tracing_on
# ... run workload until spike ...
cat /sys/kernel/tracing/trace
```

你会得到时间戳、最坏情况唤醒延迟，以及从唤醒到任务运行之间的 kernel 调用栈。

---

## 作为对比：QNX（商用 RTOS）

- **微内核：** 驱动、文件系统、网络都作为独立的用户进程运行。
- **从一开始就为 RT 设计**（不像 PREEMPT_RT 那样做“改造”）。
- 用于 QNX CAR、医疗、航空电子。通常**与 hypervisor 配合**：QNX 负责安全关键的控制（刹车、转向），Linux 负责 AI 栈；hypervisor 让它们在同一个 SoC 上保持隔离。

---

# 第 2 部分：多核调度、CPU 亲和性与 isolcpus

**上下文：** 在多核系统上，Linux 调度器会尽量把工作均匀分摊到各 CPU。它可能**把你的任务**从一个 CPU 搬到另一个。每次搬迁都会刷掉该任务的缓存，并可能带来数百微秒的抖动。对实时和推理而言，你需要**固定分配**：“这个任务始终跑在这些 CPU 上，其他任何东西都不在那里运行。”

---

## 问题所在

在 CPU 很多时，调度器会不断搬运任务来“均衡负载”。这对**公平性**和**吞吐**有好处，但对**延迟可预测性**不利：迁移会刷掉缓存，并带来数百微秒的额外开销。对 RT 和推理而言，你需要**指定座位** —— 特定任务固定在特定 CPU 上，并把来自 OS 或其他任务的干扰降到最低。

---

## 一段话讲清 SMP 调度器

**SMP** = 对称多处理（多个 CPU）。Linux 为每个 CPU 维护一个 **runqueue**（可运行任务列表）。**load balancer** 周期性地尝试均衡负载：把任务从繁忙的 CPU 搬到空闲的 CPU，优先同 core → 同 package → 同 NUMA node。对吞吐而言这是好事；但对 RT 和推理，你通常**不希望**调度器碰你的关键 core —— 你把它们隔离出来，再把任务绑上去。

---


<details>
<summary>English original</summary>

**Hardware / Platform**

- **NUMA:** On multi-socket machines, accessing memory on another socket is slower. Cross-socket access adds cost on every cache miss.
- **CPU idle (C-states):** Deeper sleep (e.g. C6) saves power but takes longer to wake (e.g. ~100 µs). For RT, limit depth with `idle=poll` or `intel_idle.max_cstate=1`.
- **Turbo / frequency:** If the CPU changes frequency in the middle of your task, timing becomes unpredictable. Use the `performance` governor so frequency stays at max.

---

**Real-Time Tuning Checklist**

Tuning is layered: **kernel config** sets the base, **boot parameters** reserve and isolate CPUs, **runtime** settings fix frequency and memory behavior, and **your application** must lock memory and avoid allocations in the RT loop.

**Kernel config (examples)**

```
CONFIG_PREEMPT_RT=y
CONFIG_HZ_1000=y
CONFIG_NO_HZ_FULL=y
CONFIG_RCU_NOCB_CPU=y
```

**Boot parameters (example: CPUs 2,3 isolated)**

```
isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3 irqaffinity=0,1
```

| Parameter | Effect |
|-----------|--------|
| `isolcpus=` | These CPUs are not used by the general scheduler; only tasks you pin go there. |
| `nohz_full=` | No periodic timer tick on these CPUs (fewer interruptions). |
| `rcu_nocbs=` | RCU callbacks run on other CPUs, not on isolated ones. |
| `irqaffinity=` | Hardware IRQs go to CPUs 0,1 only, not to isolated CPUs. |

**Runtime**

```bash
cpupower frequency-set -g performance
echo 0 > /proc/sys/kernel/numa_balancing
echo never > /sys/kernel/mm/transparent_hugepage/enabled
```

**In your RT application (before the real-time loop)**

**Context:** The kernel can swap your process’s memory to disk or allocate it lazily. A single **page fault** (bringing a page from disk or allocating it) in the RT loop can add 1–10 ms and break your deadline. So you do all “risky” work before entering the time-critical loop.

- **Lock all memory:** `mlockall(MCL_CURRENT | MCL_FUTURE)` — tells the kernel not to swap your pages out. (If a page is swapped out and you touch it, you get a major fault = huge delay.)
- **Use SCHED_FIFO:** e.g. priority 80 — your task is scheduled as real-time and can preempt normal tasks.
- **Pre-fault stack and buffers:** Touch every page you’ll use (e.g. `memset` stack and buffers). That forces the kernel to allocate them *now* so no fault happens later in the RT loop.
- **No malloc in the RT loop:** Allocate everything before the loop; inside the loop, no `malloc` (it can take locks and trigger faults).

> **Pitfall:** `mlockall()` only prevents *eviction*; it doesn’t load pages that were never touched. You must **touch** the pages (e.g. with `memset`) so they are actually in RAM.

---

**Finding What Caused a Latency Spike: ftrace**

- **cyclictest** tells you *that* a spike happened (and how big it was).
- **ftrace** (latency tracer) tells you *where* in the kernel the time was spent — which function was running during the delay.

```bash
echo 0 > /sys/kernel/tracing/tracing_on
echo latency > /sys/kernel/tracing/current_tracer
echo 1 > /sys/kernel/tracing/tracing_on
# ... run workload until spike ...
cat /sys/kernel/tracing/trace
```

You get timestamps, worst-case wakeup latency, and the kernel call stack from wakeup to task run.

---

**QNX as a Contrast (Commercial RTOS)**

- **Microkernel:** drivers, filesystems, network run as separate user processes.
- **Designed for RT from the start** (no “retrofit” like PREEMPT_RT).
- Used in QNX CAR, medical, avionics. Often **paired with a hypervisor**: QNX for safety-critical control (brakes, steering), Linux for AI stack; hypervisor keeps them isolated on the same SoC.

---

**Part 2: Multi-Core Scheduling, CPU Affinity & isolcpus**

**Context:** On a multi-core system, the Linux scheduler tries to spread work evenly across CPUs. It may **move your task** from one CPU to another. Each move flushes that task’s caches and can add hundreds of microseconds of jitter. For real-time and inference you want **fixed assignment**: “this task always runs on these CPUs, and nothing else runs there.”

---

**The Problem**

With many CPUs, the scheduler constantly moves tasks to “balance load.” That’s good for **fairness** and **throughput** but bad for **latency predictability**: migrations flush caches and add hundreds of microseconds. For RT and inference you want **assigned seats** — certain tasks on certain CPUs, with minimal interference from the OS or other tasks.

---

**SMP Scheduler in One Paragraph**

**SMP** = symmetric multi-processing (multiple CPUs). Linux keeps a **runqueue** (list of runnable tasks) per CPU. A **load balancer** periodically tries to equalize load: move tasks from busy CPUs to idle ones, preferring same core → same package → same NUMA node. For throughput this is good; for RT and inference you usually **don’t** want the scheduler to touch your critical cores — you isolate them and pin your task there.

---

</details>

## CPU 拓扑（为什么布局很重要）

**背景：** 并非所有 “CPU” 都是等价的。两个逻辑 CPU 可以是同一物理核心上的 **SMT 同级线程**（超线程）——它们共享 L1/L2 缓存和执行单元。把两个重负载线程放在同一物理核心上会因争用而让两者都变慢。

粗略的层级：

```
Socket (package) → Die → Physical core → SMT threads (e.g. 2 logical CPUs per core)
```

**经验法则：** 对推理而言，每个物理核心一个线程通常优于把两个重负载线程放到两个 SMT 同级线程上。

**查看拓扑：**

```bash
lscpu
lscpu -e
cat /sys/devices/system/cpu/cpu0/topology/core_id
cat /sys/devices/system/cpu/cpu0/topology/core_cpus_list
```

---

## CPU 亲和性（把任务绑到 CPU）

**亲和性** = 一个任务*被允许*在哪些 CPU 上运行。不设置它，调度器可以把你的任务跑在任意 CPU 上，还可能迁移它；设置了亲和性，你的任务只在你指定的 CPU 上运行。

**在 C 中：**

```c
cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(2, &mask); CPU_SET(3, &mask);
sched_setaffinity(pid, sizeof(mask), &mask);
```

**从 shell：**

```bash
taskset -c 2,3 ./inference_app
taskset -cp 2,3 <pid>
```

**为什么要绑核？** 更少的迁移 → 缓存更热、TLB 刷新开销更低、时序更可预测。

**重要：** 亲和性只限制*你的*任务。它**不会**阻止其他任务或 kernel 使用那些 CPU。要实现完全隔离，你需要 **isolcpus**（及相关启动选项），这样调度器就不会把其他任何人放到那里。

---

## isolcpus：为你的 RT/推理任务保留 CPU

**背景：** 亲和性说的是 “我的任务只在 CPU 2、3 上运行”。但调度器仍然可以把*其他*任务（守护进程、kernel 线程）放到 2 和 3 上，这会造成缓存污染和抖动。**isolcpus** 说的是 “CPU 2 和 3 对通用调度器*不可用*”；只有你显式绑到那里的任务才会在那里运行。

- **`isolcpus=2,3`** — 启动时，CPU 2 和 3 会**从通用调度器中移除**。除非你用 `taskset` 或 `sched_setaffinity` 把它绑到那里，否则不会有普通任务被放到那里。
- 通常配合使用：
  - **`nohz_full=2,3`** — 那些 CPU 上没有周期性定时器 tick（更少中断）。
  - **`rcu_nocbs=2,3`** — RCU 回调在其他 CPU 上运行，不在 2、3 上。
  - **`irqaffinity=0,1`** — 硬件 IRQ 只路由到 CPU 0、1，不到 2、3。

**结果：** 隔离的 CPU 几乎只运行你绑到那里的东西；OS 抖动可以从几百 µs 降到远低于 10 µs。

| 机制 | 作用 | 适用时机 |
|-----------|----------------|------------------|
| `sched_setaffinity` / taskset | 把一个任务限制到一组 CPU | 每进程 |
| `isolcpus` | 把 CPU 从通用池中移除 | 启动时 |
| cpuset cgroup | 把一组进程限制到 CPU（和 NUMA 节点） | runtime（如容器） |
| numactl | 把进程绑到 NUMA 节点和 CPU | 每次运行 |

---

## CPU 频率：使用 “performance” 调频器

可变频率会造成**时序抖动**。在 RT/推理核心上使用固定的最大频率：

```bash
cpupower frequency-set -g performance
```

在移动 SoC（如 Jetson）上，热管理降频仍会改变频率；在负载下监控 scaling_cur_freq。

---

## cpuset cgroup（runtime CPU 划分）

无需重启，你可以用 cpuset 把一**组**进程分配到特定的 CPU（和 NUMA 节点）：

```bash
mkdir /sys/fs/cgroup/cpuset/inference
echo "4-7" > /sys/fs/cgroup/cpuset/inference/cpuset.cpus
echo "0"   > /sys/fs/cgroup/cpuset/inference/cpuset.mems
echo <pid> > /sys/fs/cgroup/cpuset/inference/cgroup.procs
```

Kubernetes 可以用 `cpu_manager_policy=static` 和 `topologyManagerPolicy=single-numa-node` 为 guaranteed pod 做类似的事。

---

## 缓存与 Intel CAT（可选）

**背景：** L1/L2 缓存是每核心独立的；**L3（LLC）** 由封装内所有核心共享。因此即使你的推理任务被绑到一个核心，另一个核心上的另一个任务仍可能把你的数据从 L3 中**逐出**，导致额外的缓存未命中和延迟。

- **Intel CAT（Cache Allocation Technology / RDT）：** 让你在多个组之间划分 L3 的 “way”（如 “推理” 与 “OS”）。这样 OS 和其他工作负载就不能使用为推理保留的 way，也就无法逐出你的热点数据。通过 `resctrl` 或诸如 `pqos` 之类的工具配置。

---

## NUMA 亲和性

**背景：** **NUMA**（Non-Uniform Memory Access）指在多路或某些 SoC 系统中，内存挂接在特定的 “节点”（如 socket）上。访问*你*节点上的内存比访问另一个节点上的内存更快。所以你的进程内存来自**哪个 NUMA 节点**很关键；跨 NUMA 访问在每次缓存未命中时都要付出更高代价。

```bash
numactl --cpunodebind=0 --membind=0 ./inference
numastat -p <pid>
nvidia-smi topo -m   # CPU–GPU topology
```

在延迟敏感的系统上关闭 **AutoNUMA**：`echo 0 > /proc/sys/kernel/numa_balancing`。否则页迁移会造成 TLB shootdown 和抖动。

---


<details>
<summary>English original</summary>

**CPU Topology (Why Placement Matters)**

**Context:** Not all “CPUs” are equal. Two logical CPUs can be **SMT siblings** (hyperthreads) on the same physical core — they share L1/L2 cache and execution units. Putting two heavy threads on the same physical core can make both slower due to contention.

Rough hierarchy:

```
Socket (package) → Die → Physical core → SMT threads (e.g. 2 logical CPUs per core)
```

**Rule of thumb:** For inference, often **one thread per physical core** is better than using both SMT siblings for two heavy threads.

**Inspect topology:**

```bash
lscpu
lscpu -e
cat /sys/devices/system/cpu/cpu0/topology/core_id
cat /sys/devices/system/cpu/cpu0/topology/core_cpus_list
```

---

**CPU Affinity (Pinning a Task to CPUs)**

**Affinity** = which CPUs a task is *allowed* to run on. Without setting it, the scheduler can run your task on any CPU and may migrate it; with affinity, your task only runs on the CPUs you specify.

**In C:**

```c
cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(2, &mask); CPU_SET(3, &mask);
sched_setaffinity(pid, sizeof(mask), &mask);
```

**From shell:**

```bash
taskset -c 2,3 ./inference_app
taskset -cp 2,3 <pid>
```

**Why pin?** Fewer migrations → warmer caches, less TLB flush cost, more predictable timing.

**Important:** Affinity only restricts *your* task. It does **not** stop other tasks or the kernel from using those CPUs. For full isolation you need **isolcpus** (and related boot options) so the scheduler doesn’t put anyone else there.

---

**isolcpus: Reserve CPUs for Your RT/Inference Tasks**

**Context:** Affinity says “my task runs only on CPUs 2,3.” But the scheduler can still put *other* tasks (daemons, kernel threads) on 2 and 3, which causes cache pollution and jitter. **isolcpus** says “CPUs 2 and 3 are *off limits* to the general scheduler”; only tasks you explicitly pin there will run there.

- **`isolcpus=2,3`** — At boot, CPUs 2 and 3 are **removed from the general scheduler**. No normal task is placed there unless you pin it with `taskset` or `sched_setaffinity`.
- Usually combined with:
  - **`nohz_full=2,3`** — No periodic timer tick on those CPUs (fewer interruptions).
  - **`rcu_nocbs=2,3`** — RCU callbacks run on other CPUs, not on 2,3.
  - **`irqaffinity=0,1`** — Hardware IRQs are routed only to CPUs 0,1, not to 2,3.

**Result:** Isolated CPUs run almost only what you pin there; OS jitter can drop from hundreds of µs to well under 10 µs.

| Mechanism | What it does | When it applies |
|-----------|----------------|------------------|
| `sched_setaffinity` / taskset | Restrict one task to a set of CPUs | Per process |
| `isolcpus` | Remove CPUs from general pool | Boot time |
| cpuset cgroup | Restrict a group of processes to CPUs (and NUMA nodes) | Runtime (e.g. containers) |
| numactl | Bind process to NUMA node(s) and CPUs | Per run |

---

**CPU Frequency: Use “performance” Governor**

Variable frequency causes **timing jitter**. Use a fixed max frequency on RT/inference cores:

```bash
cpupower frequency-set -g performance
```

On mobile SoCs (e.g. Jetson), thermal throttling can still change frequency; monitor scaling_cur_freq under load.

---

**cpuset Cgroups (Runtime CPU Partitioning)**

Without rebooting, you can assign a **group** of processes to specific CPUs (and NUMA nodes) using cpusets:

```bash
mkdir /sys/fs/cgroup/cpuset/inference
echo "4-7" > /sys/fs/cgroup/cpuset/inference/cpuset.cpus
echo "0"   > /sys/fs/cgroup/cpuset/inference/cpuset.mems
echo <pid> > /sys/fs/cgroup/cpuset/inference/cgroup.procs
```

Kubernetes can do similar things with `cpu_manager_policy=static` and `topologyManagerPolicy=single-numa-node` for guaranteed pods.

---

**Cache and Intel CAT (Optional)**

**Context:** L1/L2 caches are per-core; **L3 (LLC)** is shared by all cores in a package. So even if your inference task is pinned to one core, another task on another core can **evict** your data from L3, causing extra cache misses and latency.

- **Intel CAT (Cache Allocation Technology / RDT):** Lets you partition L3 “ways” between groups (e.g. “inference” vs “OS”). The OS and other workloads then cannot use the ways reserved for inference, so they can’t evict your hot data. Configure via `resctrl` or tools like `pqos`.

---

**NUMA Affinity**

**Context:** **NUMA** (Non-Uniform Memory Access) means that on multi-socket or some SoC systems, memory is attached to a specific “node” (e.g. socket). Accessing memory on *your* node is faster than accessing memory on another node. So **which NUMA node** your process’s memory comes from matters; cross-NUMA access costs more on every cache miss.

```bash
numactl --cpunodebind=0 --membind=0 ./inference
numastat -p <pid>
nvidia-smi topo -m   # CPU–GPU topology
```

Turn off **AutoNUMA** on latency-sensitive systems: `echo 0 > /proc/sys/kernel/numa_balancing`. Otherwise page migrations cause TLB shootdowns and jitter.

---

</details>

# 第 3 部分：同步 —— 自旋锁、互斥锁、读写锁与顺序锁

**上下文：** 当多个线程（或 CPU）访问同一份数据时，它们必须协调。否则一个线程可能读到半更新的数据，或者两个写者可能破坏内存。这就是**竞态条件**。同步原语是强制执行诸如“一次只能有一个写者”或“多个读者可以，一个写者独占”之类规则的工具。

---

## 问题：竞态条件

当两个线程在没有协调的情况下使用同一份数据时，结果可能取决于**时序**——即**竞态条件**。同步原语确保一次只有一个写者（或多个读者，取决于原语）访问共享状态。

**临界区** = 访问共享数据的那段代码。我们需要三个性质：

1. **互斥** —— 任意时刻至多一个线程处于临界区。
2. **前进** —— 如果无人位于临界区内，等待的线程最终能进入。
3. **有限等待** —— 没有线程永远等待（没有饥饿）。

---

## 自旋锁（`spinlock_t`）

- **行为：** 要获取锁，CPU 会在循环中**忙等**（自旋），直到锁空闲。没有上下文切换。持有锁期间，该 CPU 上的抢占被禁用。
- **使用场景：** 临界区**非常短**（< ~1 µs），并且你可能处于**中断上下文**（其中你**不能睡眠**——例如在 IRQ 处理程序内部）。
- **与 IRQ 处理程序共享数据时：** 使用 `spin_lock_irqsave` / `spin_unlock_irqrestore`。这会在你持有锁期间禁用本地中断，这样处理程序就无法在同一 CPU 上运行并试图获取同一把锁（这会导致死锁）。

```c
spin_lock_irqsave(&lock, flags);
/* critical section */
spin_unlock_irqrestore(&lock, flags);
```

**规则：** 持有自旋锁时绝不睡眠（不要 `kmalloc(GFP_KERNEL)`、`msleep()` 等）。在 PREEMPT_RT 上，`spinlock_t` 变为睡眠锁（rtmutex）；`raw_spinlock_t` 仍是真正的自旋锁——不要带着它睡眠。

---

## 互斥锁（`struct mutex`）

- **行为：** 如果锁忙，任务会**阻塞**（睡眠），调度器运行其他任务。没有自旋——CPU 可以空闲去做其他工作。
- **使用场景：** 较长的临界区，并且**仅在进程上下文**中（不在中断处理程序中，因为中断不能睡眠）。
- **变体：** `mutex_trylock`（不阻塞，返回成功/失败）、`mutex_lock_interruptible`（可以被信号中断）。

**rtmutex：** 一种带有**优先级继承**的互斥锁。如果高优先级任务 H 正在等待低优先级任务 L 持有的锁，kernel 会临时**将 L 的优先级提升**到 H 的级别，这样 L 能更快运行、更快释放锁，H 就能继续。这避免了**优先级反转**（H 被 L 卡住，而 L 又被中等优先级任务卡住）。在 PREEMPT_RT 下，许多 kernel “自旋锁”实际上是 rtmutex。

---

## 读写信号量（`rw_semaphore`）

- **行为：** 多个**读者**可以同时持有锁，或者一个**写者**独占——绝不能两者同时。当读频繁而写罕见时很好。
- **使用场景：** 读比写频繁得多（例如配置表、模型权重）。许多线程可以并行读；只有一个能写。
- **API：** `down_read` / `up_read`、`down_write` / `up_write`；`downgrade_write` 将写锁转为读锁而不释放（这样其他写者无法插入）。
- **上下文：** 仅限进程上下文（它是睡眠锁）。对于 **IRQ 上下文**中的短读密集型区段，kernel 有 `rwlock_t`（基于自旋，不睡眠）。

---

## 顺序锁（`seqlock_t`）

- **行为：** **写者**从不阻塞——它们只是递增一个序列计数器并写入。**读者**读取序列号，复制数据，然后再次检查序列；如果它变了（发生了写入），它们就**重试**。因此写者从不会被读者延迟。
- **使用场景：** 写**罕见**，读频繁，并且你需要**写者从不等待**（例如高优先级传感器线程更新时间戳；读者可以重试）。
- **限制：** **不要**用于包含**指针**的数据，这些指针可能被释放并重用（读者可能在重试检测到变化之前解引用一个已释放的指针）。对于指针密集型结构，改用 **RCU**。顺序锁适用于普通数据（数字、不含指针的结构体）。

---

## 完成量（`struct completion`）

- **行为：** 一次性的“事件已发生”信号。一个线程调用 `wait_for_completion()` 并阻塞；另一个线程（或中断处理程序）稍后调用 `complete()` 来唤醒它。没有锁保护共享数据——只是“等待直到 X 完成”。
- **API：** `wait_for_completion`、`wait_for_completion_timeout`、`complete`（唤醒一个）/ `complete_all`（唤醒全部）；`reinit_completion` 用于在 `complete_all` 之后重用。
- **使用场景：** 单个“完成”事件（DMA 完成、kthread 启动、固件加载）。比使用信号量做一次性信号更干净且更不易出错。

---


<details>
<summary>English original</summary>

**Part 3: Synchronization — Spinlocks, Mutexes, RW Locks & Seqlocks**

**Context:** When multiple threads (or CPUs) touch the same data, they must coordinate. Otherwise one thread might read half-updated data or two writers might corrupt memory. That’s a **race condition**. Synchronization primitives are the tools that enforce rules like “only one writer at a time” or “many readers OK, one writer exclusive.”

---

**The Problem: Race Conditions**

When two threads use the same data without coordination, the result can depend on **timing** — a **race condition**. Synchronization primitives ensure that only one writer (or many readers, depending on the primitive) access shared state at a time.

**Critical section** = the piece of code that touches shared data. We want three properties:

1. **Mutual exclusion** — At most one thread in the critical section at a time.
2. **Progress** — If no one is inside, a waiting thread eventually gets in.
3. **Bounded waiting** — No thread waits forever (no starvation).

---

**Spinlock (`spinlock_t`)**

- **Behavior:** To acquire the lock, the CPU **busy-waits** (spins) in a loop until the lock is free. No context switch. While the lock is held, preemption is disabled on that CPU.
- **Use when:** The critical section is **very short** (< ~1 µs) and you might be in **interrupt context** (where you **cannot sleep** — e.g. inside an IRQ handler).
- **When sharing data with an IRQ handler:** Use `spin_lock_irqsave` / `spin_unlock_irqrestore`. This disables local interrupts while you hold the lock so the handler can’t run on the same CPU and try to take the same lock (which would deadlock).

```c
spin_lock_irqsave(&lock, flags);
/* critical section */
spin_unlock_irqrestore(&lock, flags);
```

**Rule:** Never sleep (no `kmalloc(GFP_KERNEL)`, `msleep()`, etc.) while holding a spinlock. On PREEMPT_RT, `spinlock_t` becomes a sleeping lock (rtmutex); `raw_spinlock_t` stays a real spinlock — do not sleep with it.

---

**Mutex (`struct mutex`)**

- **Behavior:** If the lock is busy, the task **blocks** (sleeps) and the scheduler runs other tasks. No spinning — the CPU is free to do other work.
- **Use when:** Longer critical sections, and **only in process context** (not in interrupt handlers, because interrupts cannot sleep).
- **Variants:** `mutex_trylock` (don’t block, return success/failure), `mutex_lock_interruptible` (can be interrupted by a signal).

**rtmutex:** A mutex with **priority inheritance**. If high-priority task H is waiting on a lock held by low-priority task L, the kernel temporarily **boosts L’s priority** to H’s level so L runs sooner, releases the lock sooner, and H can proceed. This avoids **priority inversion** (H stuck behind L, which is stuck behind medium-priority tasks). Under PREEMPT_RT, many kernel “spinlocks” are actually rtmutexes.

---

**Read/Write Semaphore (`rw_semaphore`)**

- **Behavior:** Many **readers** can hold the lock at the same time, OR one **writer** exclusively — never both. Good when reads are frequent and writes are rare.
- **Use when:** Reads are much more frequent than writes (e.g. config tables, model weights). Many threads can read in parallel; only one can write.
- **APIs:** `down_read` / `up_read`, `down_write` / `up_write`; `downgrade_write` turns a write lock into a read lock without releasing (so no other writer can slip in).
- **Context:** Process context only (it’s a sleeping lock). For short read-heavy sections in **IRQ context** the kernel has `rwlock_t` (spin-based, no sleep).

---

**Seqlock (`seqlock_t`)**

- **Behavior:** **Writers** never block — they just increment a sequence counter and write. **Readers** read the sequence number, copy the data, then check the sequence again; if it changed (a write happened), they **retry**. So writers are never delayed by readers.
- **Use when:** Writes are **rare**, reads are frequent, and you need **writers to never wait** (e.g. a high-priority sensor thread updating a timestamp; readers can retry).
- **Limitation:** Do **not** use for data that contains **pointers** that might be freed and reused (reader could dereference a freed pointer before retry detects the change). For pointer-heavy structures use **RCU** instead. Seqlock is for plain data (numbers, structs without pointers).

---

**Completion (`struct completion`)**

- **Behavior:** A one-shot “event happened” signal. One thread calls `wait_for_completion()` and blocks; another thread (or an interrupt handler) later calls `complete()` to wake it. No lock protecting shared data — just “wait until X is done.”
- **APIs:** `wait_for_completion`, `wait_for_completion_timeout`, `complete` (wake one) / `complete_all` (wake all); `reinit_completion` to reuse after `complete_all`.
- **Use when:** A single “done” event (DMA finished, kthread started, firmware loaded). Cleaner and less error-prone than using a semaphore for one-shot signaling.

---

</details>

## lockdep — 捕获死锁与错误锁用法

- **作用：** kernel 的 **锁依赖跟踪器**。它记录获取锁的顺序，并检测 **潜在死锁**（例如 CPU 0 持有 A 并请求 B，CPU 1 持有 B 并请求 A）和 **非法使用**（例如在中断上下文中获取睡眠锁）。它可以在 *首次* 看到错误顺序时报告这些问题，甚至在真正挂起之前。
- **启用：** `CONFIG_PROVE_LOCKING=y`、`CONFIG_LOCK_STAT=y`。开发期间使用；生产环境禁用（会增加开销）。
- **输出：** `dmesg` 中的警告及完整栈回溯。`cat /proc/lock_stat` 显示每个锁的争用统计。

---

# 汇总表

## 抢占 / RT

| 配置 | 典型最坏情况 | 最适合 |
|--------|---------------------|----------|
| PREEMPT_NONE | >1 ms | 服务器 |
| PREEMPT_VOLUNTARY | ~500 µs | 通用 |
| PREEMPT | ~100–200 µs | 桌面 |
| PREEMPT_RT | <50 µs | 机器人、AV、电机控制 |
| QNX（RTOS） | <10 µs | 硬实时、安全认证 |

## 同步原语

**快速“何时使用什么”：** 极短临界区 + IRQ？→ spinlock。较长临界区，仅进程？→ mutex。多读者、少写者？→ rw_semaphore。写者绝不能等待？→ seqlock。一次性“事件完成”？→ completion。

| 原语 | 上下文 | 阻塞？ | 优先级继承？ | 最适合 |
|-----------|---------|--------|------------------------|----------|
| spinlock_t | 进程 + IRQ | 否（自旋） | 否 | 极短 CS，IRQ 共享数据 |
| raw_spinlock_t | 进程 + IRQ | 否（自旋） | 否 | 硬件关键，不能睡眠 |
| mutex | 进程 | 是 | 否 | 进程上下文中较长 CS |
| rtmutex | 进程 | 是 | 是 | RT、PREEMPT_RT |
| rw_semaphore | 进程 | 是 | 否 | 读密集，较长 CS |
| seqlock_t | 进程 + IRQ（写者） | 写者否；读者重试 | 否 | 写少，读频繁 |
| completion | 进程 | 是 | 否 | 一次性事件 |

## CPU / 隔离机制

| 机制 | 范围 | 时机 | 工具 / 位置 |
|-----------|--------|------|----------------|
| sched_setaffinity | 每个任务 | Runtime | taskset |
| isolcpus | 每个 CPU | 启动 | Kernel cmdline |
| nohz_full、rcu_nocbs、irqaffinity | 每个 CPU | 启动 | Kernel cmdline |
| cpuset cgroup | 每个 cgroup | Runtime | cgset、Kubernetes |
| cpufreq performance | 每个 CPU | Runtime | cpupower |
| numactl | 每个进程 / NUMA 节点 | 每次运行 | numactl |

---

# AI 硬件 / 边缘相关性

- **PREEMPT_RT** 用于控制回路（例如 openpilot `controlsd`）必须在硬窗口（例如 10 ms）内完成的场景；cyclictest 直方图输入安全时序分析（例如 ISO 26262 ASIL-B）。
- Jetson Orin 上的 **isolcpus + nohz_full**（例如核 4–11 用于推理，0–3 用于 OS）将 OS 抖动排除在推理延迟预算之外。
- **mlockall** 和预缺页是 RT 推理的标准做法；单次主缺页可能增加 1–10 ms 并破坏截止时间。
- bring-up（上电点亮/调通）期间用 **hwlatdetect** 发现 SMI 引起的延迟；修复它通常需要固件/BIOS，而不仅仅是 kernel 调优。
- **CPU affinity + isolcpus** 用于 ROS2 RT 回调组和推理线程，提供确定性的布局和稳定的 WCET。
- **NUMA 绑定**（`numactl`、cpuset mems）使推理和 GPU 位于同一 socket 并避免跨 NUMA 惩罚。
- **Intel CAT** 可以为推理预留 LLC way，使 OS 活动不会逐出模型权重并导致延迟尖峰。
- camera/DMA 中断服务程序（ISR）中的 **自旋锁** 保护帧索引（短，不睡眠）；**rw_semaphore** 用于权重热重载（多读者，一个写者）；**seqlock** 用于传感器时间戳（写者从不阻塞）；**completion** 用于加速器驱动中的“DMA 完成”。
- PREEMPT_RT 下的 **rtmutex** 在整个 kernel 中提供优先级继承，避免 AV/机器人栈中的优先级反转。
- 驱动开发期间的 **lockdep** 在部署前捕获 camera、ISP（图像信号处理器）和 DMA 代码中的锁顺序和中断上下文 bug。

---

*结合 Lectures L7、L8、L9（实时 Linux、多核调度 & isolcpus、同步）。*


<details>
<summary>English original</summary>

**lockdep — Catch Deadlocks and Bad Lock Use**

- **What it does:** The kernel’s **lock dependency tracker**. It records the order in which locks are taken and detects **potential deadlocks** (e.g. CPU 0 holds A and wants B, CPU 1 holds B and wants A) and **invalid use** (e.g. taking a sleeping lock in interrupt context). It can report these the *first* time a bad ordering is seen, before an actual hang happens.
- **Enable:** `CONFIG_PROVE_LOCKING=y`, `CONFIG_LOCK_STAT=y`. Use during development; disable in production (adds overhead).
- **Output:** Warnings in `dmesg` with full stack traces. `cat /proc/lock_stat` shows per-lock contention statistics.

---

**Summary Tables**

**Preemption / RT**

| Config | Typical worst-case | Best for |
|--------|---------------------|----------|
| PREEMPT_NONE | >1 ms | Servers |
| PREEMPT_VOLUNTARY | ~500 µs | General |
| PREEMPT | ~100–200 µs | Desktop |
| PREEMPT_RT | <50 µs | Robotics, AV, motor control |
| QNX (RTOS) | <10 µs | Hard RT, safety-certified |

**Synchronization Primitives**

**Quick “when to use what”:** Very short section + IRQ? → spinlock. Longer section, process only? → mutex. Many readers, few writers? → rw_semaphore. Writer must never wait? → seqlock. One-shot “event done”? → completion.

| Primitive | Context | Blocks? | Priority inheritance? | Best for |
|-----------|---------|--------|------------------------|----------|
| spinlock_t | Process + IRQ | No (spin) | No | Very short CS, IRQ-shared data |
| raw_spinlock_t | Process + IRQ | No (spin) | No | Hardware-critical, must not sleep |
| mutex | Process | Yes | No | Longer CS in process context |
| rtmutex | Process | Yes | Yes | RT, PREEMPT_RT |
| rw_semaphore | Process | Yes | No | Read-heavy, longer CS |
| seqlock_t | Process + IRQ (writer) | Writer no; reader retries | No | Rare writes, frequent reads |
| completion | Process | Yes | No | One-shot event |

**CPU / Isolation Mechanisms**

| Mechanism | Scope | When | Tool / where |
|-----------|--------|------|----------------|
| sched_setaffinity | Per task | Runtime | taskset |
| isolcpus | Per CPU | Boot | Kernel cmdline |
| nohz_full, rcu_nocbs, irqaffinity | Per CPU | Boot | Kernel cmdline |
| cpuset cgroup | Per cgroup | Runtime | cgset, Kubernetes |
| cpufreq performance | Per CPU | Runtime | cpupower |
| numactl | Per process / node | Per run | numactl |

---

**AI Hardware / Edge Relevance**

- **PREEMPT_RT** is used where control loops (e.g. openpilot `controlsd`) must complete within a hard window (e.g. 10 ms); cyclictest histograms feed into timing analysis for safety (e.g. ISO 26262 ASIL-B).
- **isolcpus + nohz_full** on Jetson Orin (e.g. cores 4–11 for inference, 0–3 for OS) keeps OS jitter out of the inference latency budget.
- **mlockall** and pre-faulting are standard for RT inference; a single major page fault can add 1–10 ms and break deadlines.
- **hwlatdetect** during bring-up finds SMI-induced latency; fixing it often requires firmware/BIOS, not just kernel tuning.
- **CPU affinity + isolcpus** for ROS2 RT callback groups and inference threads give deterministic placement and stable WCET.
- **NUMA binding** (`numactl`, cpuset mems) keeps inference and GPU on the same socket and avoids cross-NUMA penalty.
- **Intel CAT** can reserve LLC ways for inference so OS activity doesn’t evict model weights and cause latency spikes.
- **Spinlocks** in camera/DMA ISRs protect frame indices (short, no sleep); **rw_semaphore** for weight hot-reload (many readers, one writer); **seqlock** for sensor timestamps (writer never blocked); **completion** for “DMA done” in accelerator drivers.
- **rtmutex** under PREEMPT_RT gives priority inheritance across the kernel, avoiding priority inversion in AV/robotics stacks.
- **lockdep** during driver development catches lock-order and interrupt-context bugs in camera, ISP, and DMA code before deployment.

---

*Combines Lectures L7, L8, L9 (Real-Time Linux, Multi-Core Scheduling & isolcpus, Synchronization).*

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-Note-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-Note-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
