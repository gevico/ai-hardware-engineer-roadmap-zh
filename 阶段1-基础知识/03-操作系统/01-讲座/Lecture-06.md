---
title: 第 6 讲：CPU 调度：CFS、EEVDF 与实时调度类
description: 第 6 讲：CPU 调度：CFS、EEVDF 与实时调度类
---

# 第 6 讲：CPU 调度：CFS、EEVDF 与实时调度类

## 概述

CPU 调度器决定下一个运行哪个任务、以及运行多久。在只有一个任务的简单世界里，不存在调度问题。在真实的 AI 系统中，`camerad`、`modeld`、`controlsd`、遥测日志记录器和 kernel worker 都在争抢 CPU 时间，调度器的选择直接决定了推理是在帧截止时间之内完成，还是超出 5 ms 而错过。核心挑战在于：如何在给每个进程公平的 CPU 份额的同时，保证安全关键的实时任务——例如 CAN 总线写入和模型推理——永远不会被后台工作拖慢？心智模型是**严格的队列层级**：实时任务总是最先被检查，甚至早于公平调度器获得机会。对 AI 硬件工程师而言，懂得如何指定正确的调度类、设置正确的优先级并验证结果，就是「一个偶尔出毛病的 demo」与「一个能守住截止时间的生产系统」之间的差别。

---

## 调度器类层级

Linux 调度器类按**严格的优先级顺序**检查——较高类**总是抢占**较低类：

```
Scheduler Class Priority Hierarchy
┌────────────────────────────────────────────────────────┐
│  stop_sched_class        ← HIGHEST PRIORITY            │
│  CPU migration, stop-machine operations                │
│  (internal kernel use only)                            │
├────────────────────────────────────────────────────────┤
│  dl_sched_class                                        │
│  SCHED_DEADLINE — CBS/EDF; periodic RT tasks          │
│  Example: modeld at 30fps, sensor pipeline             │
├────────────────────────────────────────────────────────┤
│  rt_sched_class                                        │
│  SCHED_FIFO, SCHED_RR — static priority 1–99          │
│  Example: controlsd CAN writes, IMU read loop         │
├────────────────────────────────────────────────────────┤
│  fair_sched_class                                      │
│  SCHED_NORMAL / SCHED_BATCH                           │
│  CFS (< Linux 6.6) or EEVDF (>= Linux 6.6)           │
│  Example: most userspace processes, glibc, Python     │
├────────────────────────────────────────────────────────┤
│  idle_sched_class        ← LOWEST PRIORITY             │
│  SCHED_IDLE — below nice +19                          │
│  Example: telemetry logging, log compression          │
└────────────────────────────────────────────────────────┘
         ↑ Higher class always preempts lower class ↑
```

一个优先级为 1 的 `SCHED_FIFO` 任务会**抢占系统中的每一个 CFS/EEVDF 任务**。**不存在协作式的绕过机制**——内核无条件强制执行。

> **关键洞察：** 调度器类的检查发生在每一个唤醒点和抢占点。当 `controlsd`（SCHED_FIFO，优先级 50）因 CAN 帧到达而唤醒时，内核会立刻抢占正在运行的 `SCHED_NORMAL` 任务——即使该任务正处在 Python 解释器循环的中间。正是这种无条件抢占，让实时调度成为确定性的。

---

## CFS：完全公平调度器（Linux 2.6.23 – 6.5）

**CFS** 建模了一个“理想 CPU”，它以 1/N 的速度同时运行所有可运行任务。在 Linux 6.6 中被 EEVDF 取代。

### vruntime 与红黑树

每个任务按自身的调度权重累积**虚拟运行时**：

```
vruntime += actual_runtime x (NICE_0_WEIGHT / task_weight)
```

可以把 vruntime 看作一个**欠债记录器**：累积 CPU 时间越多的任务欠债越高。调度器总是把 CPU 时间给**欠债最少**（vruntime 最低）的任务。**Nice 值**改变欠债的累积速率：nice -5 的任务欠债累积速度比 nice 0 的任务慢 3 倍，因此获得 3 倍的 CPU 份额。

任务存放在以 `vruntime` 为键的**红黑树**中。调度器总是选取最左节点（最小 vruntime）：插入/删除 O(log n)，选取下一个 O(1)。

```
CFS Red-Black Tree (sorted by vruntime)
                    [vruntime=100]
                   /              \
          [vruntime=50]      [vruntime=200]
          /          \
   [vruntime=20] [vruntime=80]
         ↑
    leftmost node = next task to run
```

### Nice 值与权重

| Nice | 权重 | 相对 nice-0 的 CPU 份额 |
|---|---|---|
| -20 | 88761 | ~88x 基线 |
| -5 | 3121 | ~3x 基线 |
| 0 | 1024 | 基线 |
| +10 | 110 | ~1/9 基线 |
| +19 | 15 | ~1/68 基线 |

`weight = 1024 / (1.25 ^ nice)`——每一档对应 CPU 分配 25% 的变化。


<details>
<summary>English original</summary>

**Lecture 6: CPU Scheduling: CFS, EEVDF & Real-Time Classes**

**Overview**

The CPU scheduler decides which task runs next and for how long. In a simple world with one task, there is no scheduling problem. In a real AI system with `camerad`, `modeld`, `controlsd`, telemetry loggers, and kernel workers all competing for CPU time, the scheduler's choices directly determine whether inference completes within the frame deadline or misses it by 5 ms. The core challenge is: how do you give every process a fair share of CPU while ensuring that safety-critical real-time tasks — like CAN bus writes and model inference — are never delayed by background work? The mental model is a **strict hierarchy of queues**: real-time tasks are checked first, always, before the fair scheduler even gets a turn. For an AI hardware engineer, knowing how to assign the right scheduling class, set the right priority, and verify the result is the difference between a demo that sometimes glitches and a production system that meets its deadlines.

---

**Scheduler Class Hierarchy**

Linux scheduler classes are checked in **strict priority order** — a higher class **always preempts** a lower one:

```
Scheduler Class Priority Hierarchy
┌────────────────────────────────────────────────────────┐
│  stop_sched_class        ← HIGHEST PRIORITY            │
│  CPU migration, stop-machine operations                │
│  (internal kernel use only)                            │
├────────────────────────────────────────────────────────┤
│  dl_sched_class                                        │
│  SCHED_DEADLINE — CBS/EDF; periodic RT tasks          │
│  Example: modeld at 30fps, sensor pipeline             │
├────────────────────────────────────────────────────────┤
│  rt_sched_class                                        │
│  SCHED_FIFO, SCHED_RR — static priority 1–99          │
│  Example: controlsd CAN writes, IMU read loop         │
├────────────────────────────────────────────────────────┤
│  fair_sched_class                                      │
│  SCHED_NORMAL / SCHED_BATCH                           │
│  CFS (< Linux 6.6) or EEVDF (>= Linux 6.6)           │
│  Example: most userspace processes, glibc, Python     │
├────────────────────────────────────────────────────────┤
│  idle_sched_class        ← LOWEST PRIORITY             │
│  SCHED_IDLE — below nice +19                          │
│  Example: telemetry logging, log compression          │
└────────────────────────────────────────────────────────┘
         ↑ Higher class always preempts lower class ↑
```

A single `SCHED_FIFO` task at priority 1 **preempts every CFS/EEVDF task** on the system. There is **no cooperative override** — the kernel enforces it unconditionally.

> **Key Insight:** The scheduler class check happens at every wakeup and preemption point. When `controlsd` (SCHED_FIFO, priority 50) wakes up because a CAN frame arrived, the kernel immediately preempts whatever `SCHED_NORMAL` task was running — even if that task is in the middle of a Python interpreter loop. This unconditional preemption is what makes real-time scheduling deterministic.

---

**CFS: Completely Fair Scheduler (Linux 2.6.23 – 6.5)**

**CFS** models an "ideal CPU" running all runnable tasks simultaneously at 1/N speed. Replaced by EEVDF in Linux 6.6.

**vruntime and the Red-Black Tree**

Each task accumulates **virtual runtime** weighted by its scheduling weight:

```
vruntime += actual_runtime x (NICE_0_WEIGHT / task_weight)
```

Think of vruntime as a **debt tracker**: tasks with more CPU time accumulated have higher debt. The scheduler always gives CPU time to the task with the **least debt** (lowest vruntime). **Nice values** change the debt accumulation rate: a nice -5 task accumulates debt 3x slower than a nice 0 task, so it gets 3x more CPU share.

Tasks are stored in a **red-black tree** keyed by `vruntime`. The scheduler always picks the leftmost node (minimum vruntime): O(log n) insert/delete, O(1) pick-next.

```
CFS Red-Black Tree (sorted by vruntime)
                    [vruntime=100]
                   /              \
          [vruntime=50]      [vruntime=200]
          /          \
   [vruntime=20] [vruntime=80]
         ↑
    leftmost node = next task to run
```

**Nice Values and Weights**

| Nice | Weight | CPU share vs nice-0 |
|---|---|---|
| -20 | 88761 | ~88x baseline |
| -5 | 3121 | ~3x baseline |
| 0 | 1024 | Baseline |
| +10 | 110 | ~1/9 baseline |
| +19 | 15 | ~1/68 baseline |

`weight = 1024 / (1.25 ^ nice)` — each step is a 25% change in CPU allocation.

</details>

### CFS 调优参数

```bash
/proc/sys/kernel/sched_latency_ns          # scheduling period (default 6 ms)
/proc/sys/kernel/sched_min_granularity_ns  # minimum slice (default 0.75 ms)
```

在 nice 0 下有 8 个任务时：每个任务获得 6 ms / 8 = 0.75 ms。**CFS 弱点**：一个新唤醒的延迟敏感任务，如果许多任务具有更低的 vruntime，可能**最多等待 `sched_latency_ns`**。

> **常见陷阱：** 在 CFS 下，如果其他任务具有更低的 vruntime，一个刚唤醒的推理线程可能最多被延迟 `sched_latency_ns`（默认 6 ms）。这是经典的 CFS“唤醒延迟”问题。如果 `modeld` 在等待一个摄像头帧后唤醒，且有 7 个其他任务具有更低的 vruntime，那么它在运行前最多等待 6 ms。这就是为什么需要确定性的延迟的推理线程应使用 `SCHED_FIFO` 或 `SCHED_DEADLINE`，而不是依赖 CFS。

既然已经理解 CFS 的局限性，下面看它的替代者，以及它为何能改进 AI 工作负载的尾延迟。

---

## EEVDF：最早合格虚拟截止时间优先（Linux 6.6+）

**EEVDF** 在 Linux 6.6 中完全取代 CFS。CFS 代码已从内核代码树中移除。EEVDF 保留公平性（与 CFS 一样），但修复了 CFS 的主要弱点：刚唤醒的任务往往不得不排在已经运行、因而具有 *更低* vruntime 的任务后面，即使被唤醒的任务是延迟敏感的。EEVDF 改用 **合格性** 和 **虚拟截止时间**，使一个“应得”CPU 的任务在允许运行时尽快运行，而不会被卡在其他任务的 vruntime 历史之后。

### EEVDF 解决的问题（CFS 回顾）

在 CFS 下，调度器总是运行具有 **最小 vruntime** 的任务。当一个任务睡眠时（例如等待一个摄像头帧），它停止累积 vruntime。其他可运行任务继续运行，其 vruntime 增长。当睡眠任务唤醒时，它的 vruntime 并不比任何人的更旧（更小）——它只是*不是最小*，因为许多任务一直在运行。因此 CFS 可能直到其他几个任务轮到之后才会选择它，而被唤醒的任务可能看到高达整个调度周期的**唤醒到运行延迟**（例如 6 ms）。对于 30 Hz 推理流水线，这种额外抖动是不可接受的。EEVDF 改变选择规则，使“我应得多少 CPU？”和“我的下一个截止时间是什么时候？”比原始 vruntime 顺序更重要。

### 关键概念（通俗解释）

- **lag**  
  与“理想”公平份额相比，一个任务**应得**多少 CPU 时间。如果理想调度器到现在会给该任务 10 ms，但它只得到 5 ms（例如因为它一直在睡眠），则 **lag = +5 ms**（它落后了）。如果它得到 12 ms，则 **lag = −2 ms**（它超前了）。  
  - **正 lag** → 任务落后；它*应该*很快获得 CPU。  
  - **零或负 lag** → 任务至少已获得其公平份额；其他任务可以先运行。

- **合格**  
  从公平性角度看，一个任务被允许运行时，它就是**合格**的。在 EEVDF 中，这意味着它的“虚拟开始时间”等于或早于当前虚拟时间，这对应 **lag ≥ 0**：该任务没有超出其公平份额。  
  - 所以：**直观上 合格 ≈ “具有非负 lag（lag ≥ 0）”** —— 实际上合格性通过虚拟时间定义，但效果是：如果一个任务已经获得了 *超过* 其公平份额的 CPU（负 lag），它**不**合格，并且在虚拟时间赶上之前不会被纳入选择。

- **虚拟截止时间**  
  每个可运行任务都会获得一个**虚拟截止时间**：虚拟时间中的一个点，任务应在该点之前获得其下一个 CPU 时间片。内核根据任务的 `sched_slice`（每轮调度获得多少虚拟时间）和当前 vruntime 计算该值。需要更快响应的任务获得更小的时间片，因而获得**更早**的虚拟截止时间。

- **选择规则**  
  在**所有可运行任务**中，先剔除任何**不合格**的任务。在**合格**的任务中，运行具有**最早虚拟截止时间**的任务。因此：**最早合格虚拟截止时间优先**。

### 为什么“合格”很重要

如果调度器只选择“最早截止时间”，那么一个已经消耗了 *超过* 其公平份额的任务（负 lag）仍可能具有最早的截止时间并持续被选中，从而使其他任务饥饿。**合格**过滤器可防止这种情况：只有未超出其公平份额的任务（即合格的任务）才是候选者。这样可同时获得**公平性**（只有合格的任务运行）和**良好的延迟**（在合格任务中，截止时间最早的任务接下来运行）。


<details>
<summary>English original</summary>

**CFS Tuning Parameters**

```bash
/proc/sys/kernel/sched_latency_ns          # scheduling period (default 6 ms)
/proc/sys/kernel/sched_min_granularity_ns  # minimum slice (default 0.75 ms)
```

With 8 tasks at nice 0: each gets 6 ms / 8 = 0.75 ms. **CFS weakness**: a newly woken latency-sensitive task may **wait up to `sched_latency_ns`** if many tasks have lower vruntime.

> **Common Pitfall:** A freshly woken inference thread can be delayed up to `sched_latency_ns` (6 ms by default) under CFS if other tasks have lower vruntime. This is the classic CFS "wakeup latency" problem. If `modeld` wakes up after waiting for a camera frame and 7 other tasks have lower vruntime, it waits up to 6 ms before running. This is why inference threads that need deterministic latency should use `SCHED_FIFO` or `SCHED_DEADLINE` rather than relying on CFS.

Now that we understand CFS's limitations, let's look at its replacement and why it improves tail latency for AI workloads.

---

**EEVDF: Earliest Eligible Virtual Deadline First (Linux 6.6+)**

**EEVDF** replaces CFS entirely in Linux 6.6. CFS code is removed from the kernel tree. EEVDF keeps fairness (like CFS) but fixes CFS’s main weakness: a task that just woke up often has to wait behind tasks that have been running and thus have *lower* vruntime, even when the woken task is latency‑sensitive. EEVDF instead uses **eligibility** and **virtual deadlines** so that a task that is “owed” CPU runs as soon as it is allowed to, without being stuck behind others’ vruntime history.

**The Problem EEVDF Solves (CFS Recap)**

Under CFS, the scheduler always runs the task with the **smallest vruntime**. When a task sleeps (e.g. waiting for a camera frame), it stops accumulating vruntime. Other runnable tasks keep running and their vruntime grows. When the sleeping task wakes up, its vruntime is *older* (smaller) than no one’s — it’s just *not the smallest* among the many tasks that have been running. So CFS may not pick it until after several other tasks get their turn, and the woken task can see **wakeup-to-run latency** of up to the full scheduling period (e.g. 6 ms). For a 30 Hz inference pipeline, that extra jitter is unacceptable. EEVDF changes the selection rule so that “how much CPU am I owed?” and “when is my next deadline?” matter more than raw vruntime order.

**Key Concepts (Plain Language)**

- **lag**  
  How much CPU time a task is **owed** compared to an “ideal” fair share. If the ideal scheduler would have given the task 10 ms by now but it only got 5 ms (e.g. because it was sleeping), **lag = +5 ms** (it is behind). If it got 12 ms, **lag = −2 ms** (it is ahead).  
  - **Positive lag** → task is behind; it *should* get CPU soon.  
  - **Zero or negative lag** → task has had at least its fair share; others can go first.

- **eligible**  
  A task is **eligible** when it is allowed to run from a fairness point of view. In EEVDF, that means its “virtual start time” is at or before the current virtual time, which corresponds to **lag ≥ 0**: the task is not ahead of its fair share.  
  - So: **eligible ≈ “has non-negative lag (lag ≥ 0)” in the intuition** — actually eligibility is defined via virtual time, but the effect is: if a task has already had *more* than its fair share (negative lag), it is **not** eligible and is not considered for selection until virtual time catches up.

- **virtual deadline**  
  Each runnable task gets a **virtual deadline**: a point in virtual time by which it “should” get its next slice of CPU. The kernel computes this from the task’s `sched_slice` (how much virtual time it gets per scheduling round) and the current vruntime. A task that needs to be more responsive gets a smaller slice and thus an **earlier** virtual deadline.

- **Selection rule**  
  Among **all runnable tasks**, first drop any that are **not eligible**. Among the **eligible** ones, run the task with the **earliest virtual deadline**. So: **Earliest Eligible Virtual Deadline First**.

**Why “Eligible” Matters**

If the scheduler only chose “earliest deadline,” a task that had already consumed *more* than its fair share (negative lag) could still have the earliest deadline and keep getting selected, starving others. The **eligible** filter prevents that: only tasks that are not ahead of their fair share (i.e. eligible) are candidates. So we get both **fairness** (only eligible tasks run) and **good latency** (among those, the one with the earliest deadline runs next).

</details>

### EEVDF 选择逻辑 — 逐步解析

在每次调度决策时，kernel 有一组可运行任务。对每个任务，它（概念上）知道 **lag** 和 **虚拟截止时间**。

1. **计算合格性**  
   对每个可运行任务，检查它是否 **合格**（虚拟开始时间 ≤ 当前虚拟时间；用“谁被欠 CPU？”的视角：未超前于其公平份额）。将不合格任务（如 lag &lt; 0）标记为 **非**候选。

2. **仅在合格任务中**  
   本次决策忽略不合格任务。

3. **选取最早的虚拟截止时间**  
   在合格任务中，选择 **虚拟截止时间** 最小的那个（在虚拟时间中最早）。该任务接下来运行。

**示例：**

```
EEVDF Selection — at current virtual time T

  Task A:  lag = +5 ms (owed CPU)     deadline = T + 2 ms   → eligible, deadline in 2 ms
  Task B:  lag = +1 ms (owed CPU)     deadline = T + 8 ms   → eligible, deadline in 8 ms
  Task C:  lag = −2 ms (ahead)        deadline = T + 1 ms   → NOT eligible (already got extra CPU)

Eligible set: {A, B}. Earliest deadline among them: A (T+2 ms).
→ EEVDF picks Task A.
```

任务 C 的截止时间最早（T+1 ms），但它是 **不合格的**，因此不被考虑。这保持了公平性。在 A 和 B 之间，A 的截止时间更早，所以 A 运行——这与 A 更“落后”（正 lag 更大）的事实一致。

### 为什么 EEVDF 能改善尾部延迟（尤其是对 AI 工作负载）

- **CFS：** 刚被唤醒的任务（例如一帧到达后的推理线程）的 vruntime 往往大于那些一直在运行的任务。因此 CFS 会先运行那些其他任务，而被唤醒的任务可能要等满整个调度周期（如 6 ms）。这表现为 **高尾部延迟** 和抖动。

- **EEVDF：** 当推理线程唤醒时，它通常具有 **正 lag**（它曾被阻塞，因此“被欠”CPU）。所以它是 **合格** 的。它的虚拟截止时间根据其 `sched_slice` 设置。在所有合格任务中，调度器选择 **最早的截止时间**。因此，被唤醒的推理线程一旦合格且具有最早截止时间就会运行——**无需** 在 vruntime 上“追赶”长时间运行的后台任务。这降低了混合工作负载（推理 + 日志、遥测等）中的唤醒到运行延迟和尾部延迟。

所以：**EEVDF 改善尾部延迟**，因为调度由“谁被欠 CPU？”（合格性）和“谁的截止时间最紧？”（虚拟截止时间）驱动，而不是由谁的 vruntime 最小驱动。一个刚被唤醒、对延迟敏感的任务通常既被欠 CPU，又被赋予较早的截止时间，因此会很快被调度。

### 查看 EEVDF（和 CFS）行为

```bash
cat /proc/[pid]/sched | grep slice    # per-task sched_slice; smaller = more responsive
```

`sched_slice` 越小，意味着任务每轮获得的虚拟时间片越短、虚拟截止时间越早，因此在 EEVDF 下往往响应更及时。

### EEVDF 与 CFS 的使用场景

- **Linux 6.6+**（主线）：EEVDF 是默认公平调度器；CFS 代码已移除。
- **Jetson JetPack 6.x**（kernel 5.15）：仍使用 CFS。
- **Yocto Scarthgap** 及其他基于 6.6+ 的 BSP：EEVDF 是默认。

> **关键洞察：** EEVDF 相对 CFS 对 AI 工作负载的改进在于：一个刚被唤醒、被“欠”CPU（正 lag）的推理线程，一旦合格且具有最早截止时间就会被调度，而不管其他任务的 vruntime 历史如何。在 CFS 下，同一线程可能会被延迟，而 vruntime 更低的后台任务先运行。EEVDF 的合格性 + 最早截止时间规则避免了这一点，并降低了同一核心上混合工作负载（推理、日志、遥测）的尾部延迟。

---

## SCHED_FIFO

`rt_sched_class`；静态优先级 1–99（99 为最高）。

- 一直运行，直到它 **主动阻塞**、调用 `sched_yield()`，或被更高优先级的 RT 任务抢占
- 同一优先级内 **没有时间片**——prio 99 的行为不当任务会饿死其下所有任务
- 适合 **CPU 使用率已知且有界** 的任务：CAN 总线写、IMU 读循环

```bash
chrt -f 50 ./controlsd                   # launch with SCHED_FIFO priority 50
chrt -f -p 70 $(pgrep modeld)            # change priority of running process
chrt -p $(pgrep camerad)                 # query scheduler class and priority
```


<details>
<summary>English original</summary>

**EEVDF Selection Logic — Step by Step**

At each scheduling decision, the kernel has a set of runnable tasks. For each task it knows (conceptually) **lag** and **virtual deadline**.

1. **Compute eligibility**  
   For each runnable task, check if it is **eligible** (virtual start time ≤ current virtual time; in the “who is owed CPU?” view: not ahead of its fair share). Mark ineligible tasks (e.g. lag &lt; 0) as **not** candidates.

2. **Among eligible tasks only**  
   Ignore ineligible tasks for this decision.

3. **Pick earliest virtual deadline**  
   Among the eligible tasks, choose the one whose **virtual deadline** is smallest (earliest in virtual time). That task runs next.

**Example:**

```
EEVDF Selection — at current virtual time T

  Task A:  lag = +5 ms (owed CPU)     deadline = T + 2 ms   → eligible, deadline in 2 ms
  Task B:  lag = +1 ms (owed CPU)     deadline = T + 8 ms   → eligible, deadline in 8 ms
  Task C:  lag = −2 ms (ahead)        deadline = T + 1 ms   → NOT eligible (already got extra CPU)

Eligible set: {A, B}. Earliest deadline among them: A (T+2 ms).
→ EEVDF picks Task A.
```

Task C has the earliest deadline (T+1 ms) but is **ineligible**, so it is not considered. That preserves fairness. Between A and B, A has the earlier deadline, so A runs — which matches the fact that A is more “behind” (larger positive lag).

**Why EEVDF Improves Tail Latency (Especially for AI Workloads)**

- **CFS:** A freshly woken task (e.g. inference thread after a frame arrives) often has vruntime larger than that of tasks that have been running. So CFS runs those others first, and the woken task can wait up to the full scheduling period (e.g. 6 ms). That shows up as **high tail latency** and jitter.

- **EEVDF:** When the inference thread wakes up, it typically has **positive lag** (it was blocked, so it is “owed” CPU). So it is **eligible**. Its virtual deadline is set from its `sched_slice`. Among all eligible tasks, the scheduler picks the **earliest deadline**. So the woken inference thread runs as soon as it is eligible and has the earliest deadline — **without** having to “catch up” in vruntime behind long‑running background tasks. That reduces wakeup-to-run latency and tail latency in mixed workloads (inference + logging, telemetry, etc.).

So: **EEVDF improves tail latency** because scheduling is driven by “who is owed CPU?” (eligibility) and “who has the tightest deadline?” (virtual deadline), not by who has the smallest vruntime. A freshly woken, latency‑sensitive task is often both owed CPU and given an early deadline, so it gets scheduled quickly.

**Inspecting EEVDF (and CFS) Behavior**

```bash
cat /proc/[pid]/sched | grep slice    # per-task sched_slice; smaller = more responsive
```

Smaller `sched_slice` means the task gets a shorter virtual-time slice per round and an earlier virtual deadline, so it tends to be more responsive under EEVDF.

**Where EEVDF vs CFS Is Used**

- **Linux 6.6+** (mainline): EEVDF is the default fair scheduler; CFS code is removed.
- **Jetson JetPack 6.x** (kernel 5.15): still uses CFS.
- **Yocto Scarthgap** and other 6.6+‑based BSPs: EEVDF is the default.

> **Key Insight:** EEVDF’s improvement over CFS for AI workloads is that a freshly woken inference thread that is “owed” CPU (positive lag) is scheduled as soon as it is eligible and has the earliest deadline, regardless of other tasks’ vruntime history. Under CFS, the same thread could be delayed while background tasks with lower vruntime run first. EEVDF’s eligibility + earliest‑deadline rule avoids that and reduces tail latency for mixed workloads (inference, logging, telemetry) on the same cores.

---

**SCHED_FIFO**

`rt_sched_class`; static priority 1–99 (99 = highest).

- Runs until it **voluntarily blocks**, calls `sched_yield()`, or is preempted by a higher-priority RT task
- **No time slice** within a priority level — a misbehaving task at prio 99 starves everything below it
- Suitable for tasks with **well-understood, bounded CPU usage**: CAN bus writes, IMU read loops

```bash
chrt -f 50 ./controlsd                   # launch with SCHED_FIFO priority 50
chrt -f -p 70 $(pgrep modeld)            # change priority of running process
chrt -p $(pgrep camerad)                 # query scheduler class and priority
```

</details>

### RT 节流

```bash
cat /proc/sys/kernel/sched_rt_runtime_us   # default 950000 (950 ms)
cat /proc/sys/kernel/sched_rt_period_us    # default 1000000 (1 s) — 95% CPU cap for all RT tasks
```

默认情况下，RT 任务被整体**节流到 CPU 的 95%**——非 RT 任务至少保留 5%。设置 `sched_rt_runtime_us = -1` 会**完全禁用节流**；用于 AV/机器人场景，其中所有 RT 任务的运行时间都已知且有界，非 RT 任务被饿死可接受。

> **常见陷阱：** 一个永不阻塞的高优先级失控 `SCHED_FIFO` 任务会饿死所有低优先级任务，包括 shell、SSH daemon 和监控工具。这会导致系统不硬重启就无法恢复。在生产系统上以高优先级运行前，务必验证 `SCHED_FIFO` 任务的正确性（它们必须周期性地在 I/O 或 `nanosleep` 上阻塞）。5% 的 RT 节流（`sched_rt_runtime_us`）作为安全网存在——禁用它需要确信所有 RT 任务都行为良好。

---

## SCHED_RR

与 SCHED_FIFO 相同，外加一个时间片。

- 在同一优先级内，任务在时间片到期后**轮转**
- 时间片长度：`/proc/sys/kernel/sched_rr_timeslice_ms`（默认 100 ms）
- 适用于**多个同优先级 RT 任务**必须在无协作让出的情况下共享时间

---

## SCHED_DEADLINE

`dl_sched_class`；使用 **Constant Bandwidth Server (CBS)** 配合 **Earliest Deadline First (EDF)**。

### 参数

```c
struct sched_attr attr = {
    .size           = sizeof(attr),
    .sched_policy   = SCHED_DEADLINE,
    .sched_runtime  = 5000000,    /* 5 ms: CPU budget consumed before forced descheduling */
    .sched_deadline = 16666666,   /* 16.7 ms: relative deadline from period start (must finish by here) */
    .sched_period   = 16666666,   /* 16.7 ms: period — activates once per period (60 fps) */
};
sched_setattr(0, &attr, 0);       /* requires CAP_SYS_NICE */
```

### 属性

- **准入控制**：如果加入该任务会使 CPU 集合上的 sum(runtime/period) > 1.0，kernel 会以 `EBUSY` 拒绝 `sched_setattr()`——硬性的可调度性保证
- **预算强制**：任务消耗完其 `runtime` 预算后被取消调度；在下一个周期得到补充——行为不当的任务无法饿死其他任务
- **无静态优先级**：kernel 的 EDF 逻辑按绝对截止时间动态排序任务

```bash
# 30fps inference: 10ms budget, 33ms deadline, 33ms period
chrt -d --sched-runtime 10000000 --sched-deadline 33333333 --sched-period 33333333 0 ./modeld
```

```
SCHED_DEADLINE Timeline (30fps, 10ms budget, 33ms period)
t=0          t=10ms       t=16ms       t=33ms       t=43ms
│            │            │            │            │
├────────────┤░░░░░░░░░░░░├────────────┤░░░░░░░░░░░░├──
│ modeld     │ idle/other │ modeld     │ idle/other │
│ runs up to │ period     │ runs up to │ period     │
│ 10ms budget│ continues  │ 10ms budget│ continues  │
└────────────┘            └────────────┘

Legend: ── = modeld running  ░ = other tasks running / modeld done early
```

> **关键洞察：** `SCHED_DEADLINE` 的准入控制是 kernel 层面的形式化可调度性证明。当以 deadline 参数调用 `sched_setattr()` 时，kernel 会检查所有 deadline 任务的 `runtime/period` 比值之和是否仍能装进 CPU 的容量。若不能，则返回 `EBUSY`。这意味着 kernel 可以从数学上保证所有被准入的 deadline 任务都能满足其截止时间——这是任何基于优先级的方案（SCHED_FIFO）都无法提供的。这是周期性推理流水线的正确调度类。

> **常见陷阱：** `sched_runtime` 设得太低，会导致任务超出预算时收到 `SIGXCPU`，或者任务被直接提前取消调度。如果 `modeld` 有时 8 ms 完成，偶尔飙到 12 ms，设置 `sched_runtime = 10ms` 会导致偶发的提前终止。用性能剖析（`perf sched latency`、`bpftrace`）测量 99 分位执行时间，并把 `sched_runtime` 至少设为该值再留些余量。

---

## 调度器检查

```bash
cat /proc/[pid]/sched                    # vruntime, nr_voluntary_switches, nr_involuntary_switches
schedtool [pid]                          # scheduler class, priority, affinity (human-readable)

# Trace scheduler decisions
trace-cmd record -e sched_switch -e sched_wakeup ./workload
trace-cmd report | head -100
# Shows exact sequence of task switches and wakeups — identifies which task preempted which

# Per-task scheduling latency report
perf sched record -- sleep 5
perf sched latency                       # avg/max wakeup-to-run latency per task
# Most useful field: max wakeup latency — if >1ms on inference thread, investigate

# Run queue latency histogram (eBPF; no recompile needed)
runqlat -m 10                            # histogram in milliseconds, 10 second window
# Shows time tasks spend waiting on run queue before getting CPU
```


<details>
<summary>English original</summary>

**RT Throttling**

```bash
cat /proc/sys/kernel/sched_rt_runtime_us   # default 950000 (950 ms)
cat /proc/sys/kernel/sched_rt_period_us    # default 1000000 (1 s) — 95% CPU cap for all RT tasks
```

RT tasks are collectively **throttled to 95% of CPU** by default — non-RT tasks retain at least 5%. Setting `sched_rt_runtime_us = -1` **disables throttling entirely**; used in AV/robotics setups where all RT tasks have known bounded runtime and starvation of non-RT is acceptable.

> **Common Pitfall:** A runaway `SCHED_FIFO` task at high priority that never blocks will starve all lower-priority tasks, including the shell, SSH daemon, and monitoring tools. This can make the system impossible to recover without a hard reboot. Always test `SCHED_FIFO` tasks for correctness (they must block periodically on I/O or `nanosleep`) before running at high priority on a production system. The 5% RT throttling (`sched_rt_runtime_us`) exists as a safety net — disabling it requires confidence that all RT tasks are well-behaved.

---

**SCHED_RR**

Same as SCHED_FIFO plus a time slice.

- Within the same priority level, tasks **round-robin** after their slice expires
- Slice length: `/proc/sys/kernel/sched_rr_timeslice_ms` (default 100 ms)
- Useful when **multiple equal-priority RT tasks** must share time without cooperative yielding

---

**SCHED_DEADLINE**

`dl_sched_class`; uses **Constant Bandwidth Server (CBS)** with **Earliest Deadline First (EDF)**.

**Parameters**

```c
struct sched_attr attr = {
    .size           = sizeof(attr),
    .sched_policy   = SCHED_DEADLINE,
    .sched_runtime  = 5000000,    /* 5 ms: CPU budget consumed before forced descheduling */
    .sched_deadline = 16666666,   /* 16.7 ms: relative deadline from period start (must finish by here) */
    .sched_period   = 16666666,   /* 16.7 ms: period — activates once per period (60 fps) */
};
sched_setattr(0, &attr, 0);       /* requires CAP_SYS_NICE */
```

**Properties**

- **Admission control**: kernel rejects `sched_setattr()` with `EBUSY` if adding this task makes sum(runtime/period) > 1.0 on the CPU set — a hard schedulability guarantee
- **Budget enforcement**: task is descheduled after consuming its `runtime` budget; replenished at the next period — misbehaving tasks cannot starve others
- **No static priority**: the kernel's EDF logic dynamically orders tasks by absolute deadline

```bash
# 30fps inference: 10ms budget, 33ms deadline, 33ms period
chrt -d --sched-runtime 10000000 --sched-deadline 33333333 --sched-period 33333333 0 ./modeld
```

```
SCHED_DEADLINE Timeline (30fps, 10ms budget, 33ms period)
t=0          t=10ms       t=16ms       t=33ms       t=43ms
│            │            │            │            │
├────────────┤░░░░░░░░░░░░├────────────┤░░░░░░░░░░░░├──
│ modeld     │ idle/other │ modeld     │ idle/other │
│ runs up to │ period     │ runs up to │ period     │
│ 10ms budget│ continues  │ 10ms budget│ continues  │
└────────────┘            └────────────┘

Legend: ── = modeld running  ░ = other tasks running / modeld done early
```

> **Key Insight:** `SCHED_DEADLINE`'s admission control is a formal schedulability proof at the kernel level. When you call `sched_setattr()` with deadline parameters, the kernel checks whether the sum of all deadline tasks' `runtime/period` ratios still fits within the CPU's capacity. If it doesn't, `EBUSY` is returned. This means the kernel can mathematically guarantee that all admitted deadline tasks will meet their deadlines — something no priority-based scheme (SCHED_FIFO) can provide. This is the correct scheduling class for periodic inference pipelines.

> **Common Pitfall:** Setting `sched_runtime` too low causes `SIGXCPU` to be sent to the task when it exceeds its budget, or the task is simply descheduled early. If `modeld` sometimes finishes in 8 ms but occasionally spikes to 12 ms, setting `sched_runtime = 10ms` will cause occasional early termination. Use profiling (`perf sched latency`, `bpftrace`) to measure the 99th-percentile execution time and set `sched_runtime` to at least that value with some headroom.

---

**Scheduler Inspection**

```bash
cat /proc/[pid]/sched                    # vruntime, nr_voluntary_switches, nr_involuntary_switches
schedtool [pid]                          # scheduler class, priority, affinity (human-readable)

# Trace scheduler decisions
trace-cmd record -e sched_switch -e sched_wakeup ./workload
trace-cmd report | head -100
# Shows exact sequence of task switches and wakeups — identifies which task preempted which

# Per-task scheduling latency report
perf sched record -- sleep 5
perf sched latency                       # avg/max wakeup-to-run latency per task
# Most useful field: max wakeup latency — if >1ms on inference thread, investigate

# Run queue latency histogram (eBPF; no recompile needed)
runqlat -m 10                            # histogram in milliseconds, 10 second window
# Shows time tasks spend waiting on run queue before getting CPU
```

</details>

### /proc/[pid]/sched 关键字段

| 字段 | 含义 |
|---|---|
| `nr_voluntary_switches` | 任务自愿让出 CPU 的次数（阻塞 I/O、睡眠） |
| `nr_involuntary_switches` | 任务被抢占的次数（时间片到期、更高优先级任务唤醒） |
| `se.load.weight` | CFS 调度权重（由 nice 值推导） |
| `se.vruntime` | 累积的 virtual runtime |
| `policy` | 调度器策略整数：0=NORMAL，1=FIFO，2=RR，6=DEADLINE |
| `prio` | 有效优先级：100=RT prio 99；120=nice 0；139=nice 19 |

`modeld` 上的高 `nr_involuntary_switches` 表明 **CFS 抢占**——升级到 `SCHED_FIFO` 或 `SCHED_DEADLINE` 的第一信号。

> **关键洞察：** `/proc/[pid]/sched` 中的 `nr_involuntary_switches` 是 CFS 抢占问题的诊断金丝雀。如果该计数器在 `modeld` 运行推理时快速增长，意味着调度器在 `modeld` 完成之前就将其从 CPU 强行移除——因为其他任务具有更低的 vruntime 或更高的优先级。修复方法是将 `modeld` 提升到 `SCHED_FIFO` 或 `SCHED_DEADLINE`。在动用更复杂的性能剖析工具之前，先检查该字段。

---

## 总结

| 策略 | 类别 | 优先级范围 | 时间片 | 可被谁抢占 | 用例 |
|---|---|---|---|---|---|
| `SCHED_DEADLINE` | `dl_sched_class` | EDF/CBS 动态 | 按 runtime 预算 | 截止时间更高的 DL 任务 | 周期性 RT：modeld、传感器流水线 |
| `SCHED_FIFO` | `rt_sched_class` | 1–99（99 最高） | 无 | 更高 RT 优先级 | 硬 RT：CAN 写入、执行器线程 |
| `SCHED_RR` | `rt_sched_class` | 1–99 | `sched_rr_timeslice_ms` | 更高 RT 优先级 | 同优先级 RT 共享 |
| `SCHED_NORMAL` | `fair_sched_class` | nice -20 到 +19 | `sched_latency_ns / n` | 任何 RT 或 DL 任务 | 通用进程、后台工作 |
| `SCHED_IDLE` | `idle_sched_class` | 低于 nice +19 | CFS/EEVDF 时间片 | 其他所有情况 | 遥测、日志压缩 |

### 概念回顾

- **为什么单个优先级为 1 的 `SCHED_FIFO` 任务会抢占所有 `SCHED_NORMAL` 任务？** 调度器类在每个调度决策点都按严格的优先级降序检查。`rt_sched_class` 在 `fair_sched_class` 之前被检查。只要存在任何可运行的 RT 任务，公平调度器就永远不会运行。这是内核的无条件保证：RT 任务比普通任务优先获得 CPU。
- **什么是 vruntime，CFS 为什么使用它？** vruntime 是任务已获得的 CPU 时间量，按任务的权重（nice 值）归一化。CFS 选择 vruntime 最低的任务——即获得公平份额最少的那个。Nice 值影响 vruntime 累积的速度：nice -5 的任务其 vruntime 增长慢 3 倍，因此获得约 3 倍的 CPU 份额。
- **对于延迟敏感型任务，CFS 的根本弱点是什么？** 刚被唤醒的任务可能具有比其他可运行任务更高的 vruntime（因为它在睡眠期间，其他任务累积了较低的 vruntime）。CFS 必须等到该任务在调度周期中轮到自己——最多可达 `sched_latency_ns`（默认 6 ms）。EEVDF 通过 eligibility + earliest-deadline-first 规则修复了这一问题。
- **`SCHED_DEADLINE` 准入控制保证什么？** 当调用 `sched_setattr()` 时，内核检查 CPU 集合上所有截止时间任务的 sum(runtime/period) 是否 ≤ 1.0。如果是，则接纳该任务，并保证所有已接纳任务都能满足其截止时间。如果否，则返回 `EBUSY`。这是经过数学证明的可调度性保证。
- **何时应使用 `SCHED_FIFO` 而非 `SCHED_DEADLINE`？** 对于由硬件事件触发的短时、有界突发任务（CAN 写入、IMU 读取），其完成时间总是很短，且只需要严格优先级时，使用 `SCHED_FIFO`。对于每个周期都有已知 CPU 预算的周期性任务（30fps 推理），希望内核强制执行预算并提供形式化可调度性保证时，使用 `SCHED_DEADLINE`。
- **`/proc/[pid]/sched` 中较高的 `nr_involuntary_switches` 表明什么？** 任务正被调度器抢占——要么其时间片到期，要么更高优先级任务被唤醒。对于 CFS 下的推理线程，这意味着调度器正在推理中途移除它。修复方法是迁移到 `SCHED_FIFO`，或通过 `cpuset` 隔离来减少同一 CPU 上竞争任务的数量。

---


<details>
<summary>English original</summary>

**/proc/[pid]/sched Key Fields**

| Field | Meaning |
|---|---|
| `nr_voluntary_switches` | Times task gave up CPU willingly (blocking I/O, sleep) |
| `nr_involuntary_switches` | Times task was preempted (slice expired, higher-priority task woke) |
| `se.load.weight` | CFS scheduling weight (derived from nice value) |
| `se.vruntime` | Accumulated virtual runtime |
| `policy` | Scheduler policy integer: 0=NORMAL, 1=FIFO, 2=RR, 6=DEADLINE |
| `prio` | Effective priority: 100=RT prio 99; 120=nice 0; 139=nice 19 |

High `nr_involuntary_switches` on `modeld` indicates **CFS preemption** — first signal to elevate to `SCHED_FIFO` or `SCHED_DEADLINE`.

> **Key Insight:** `nr_involuntary_switches` in `/proc/[pid]/sched` is the diagnostic canary for CFS preemption problems. If this counter grows quickly while `modeld` is running inference, it means the scheduler is forcibly removing `modeld` from the CPU before it finishes — because other tasks have lower vruntime or higher priority. The fix is to elevate `modeld` to `SCHED_FIFO` or `SCHED_DEADLINE`. Check this field first before reaching for more complex profiling tools.

---

**Summary**

| Policy | Class | Priority range | Time slice | Preemptible by | Use case |
|---|---|---|---|---|---|
| `SCHED_DEADLINE` | `dl_sched_class` | EDF/CBS dynamic | Per runtime budget | Higher-deadline DL task | Periodic RT: modeld, sensor pipeline |
| `SCHED_FIFO` | `rt_sched_class` | 1–99 (99 highest) | None | Higher RT priority | Hard RT: CAN writes, actuation thread |
| `SCHED_RR` | `rt_sched_class` | 1–99 | `sched_rr_timeslice_ms` | Higher RT priority | Equal-priority RT sharing |
| `SCHED_NORMAL` | `fair_sched_class` | nice -20 to +19 | `sched_latency_ns / n` | Any RT or DL task | General processes, background work |
| `SCHED_IDLE` | `idle_sched_class` | Below nice +19 | CFS/EEVDF slice | Everything else | Telemetry, log compression |

**Conceptual Review**

- **Why does a single `SCHED_FIFO` task at priority 1 preempt all `SCHED_NORMAL` tasks?** Scheduler classes are checked in strict descending priority order at every scheduling decision. `rt_sched_class` is checked before `fair_sched_class`. As long as any runnable RT task exists, the fair scheduler never runs. This is the kernel's unconditional guarantee that RT tasks get CPU over normal tasks.
- **What is vruntime and why does CFS use it?** vruntime is the amount of CPU time a task has received, normalized by the task's weight (nice value). CFS picks the task with the lowest vruntime — the one that has received the least fair share. Nice values affect how fast vruntime accumulates: a nice -5 task's vruntime grows 3x slower, so it gets ~3x more CPU share.
- **What is the fundamental weakness of CFS for latency-sensitive tasks?** A freshly woken task may have a higher vruntime than other runnable tasks (because it was sleeping while they accumulated low vruntime). CFS must wait until this task's turn comes around in the scheduling period — up to `sched_latency_ns` (6 ms default). EEVDF fixes this with the eligibility + earliest-deadline-first rule.
- **What does `SCHED_DEADLINE` admission control guarantee?** When `sched_setattr()` is called, the kernel checks whether sum(runtime/period) across all deadline tasks on the CPU set is ≤ 1.0. If yes, it admits the task and guarantees all admitted tasks will meet their deadlines. If no, it returns `EBUSY`. This is a mathematically proven schedulability guarantee.
- **When should you use `SCHED_FIFO` vs `SCHED_DEADLINE`?** Use `SCHED_FIFO` for tasks that run in short, bounded bursts triggered by hardware events (CAN writes, IMU reads) where the completion time is always short and you just need strict priority. Use `SCHED_DEADLINE` for periodic tasks with known CPU budgets per period (inference at 30fps) where you want the kernel to enforce the budget and provide formal schedulability guarantees.
- **What does a high `nr_involuntary_switches` in `/proc/[pid]/sched` indicate?** The task is being preempted by the scheduler — either its time slice expired or a higher-priority task woke up. For an inference thread under CFS, this means the scheduler is removing it mid-inference. The fix is to move to `SCHED_FIFO` or reduce the number of competing tasks on the same CPU via `cpuset` isolation.

---

</details>

## AI 硬件关联

- `SCHED_DEADLINE` 直接对应周期性推理任务：`modeld` 以 30fps 声明 `runtime=10ms, deadline=33ms, period=33ms`；kernel 的准入控制证明可调度性并强制预算——无需用户空间 watchdog。
- `SCHED_FIFO` 在优先级 50–70 是 openpilot `controlsd` 的标准做法：100Hz 的 CAN 总线写入不能被 CFS 抖动延迟，在未经调优的多进程系统上抖动可超过 5 ms。
- EEVDF（Linux 6.6）降低混合工作负载的尾部延迟——当 TensorRT 推理、传感器读取和日志共享同一 Jetson 而无完整 CPU 隔离时尤为相关；新唤醒的推理线程比在 CFS 的 vruntime 排序下更快被调度。
- `chrt -f 50 $(pgrep modeld)` 是 openpilot 和基于 Autoware 的 AV 栈上的标准生产调优步骤；持久化配置使用 systemd unit 选项 `CPUSchedulingPolicy=fifo` 和 `CPUSchedulingPriority=50`。
- 在安全认证的 AV ECU 部署中会禁用 `rt_throttling`（`sched_rt_runtime_us = -1`），其中所有 RT 任务都有经形式化验证的有界 CPU 占用，非 RT 饥饿通过在 `SCHED_IDLE` 运行遥测来缓解。
- 现场测试后 `perf sched latency`，可暴露应用级 timing 中不可见的调度器引起的延迟——当同一硬件上部署时的推理延迟相比台架测试升高时，这是首要诊断手段。

### openpilot 中的真实示例（本 repo）

本路线图中的 openpilot 代码实现了本讲中的相同调度概念：

| 课程概念 | openpilot 实现 |
|------------------|---------------------------|
| **SCHED_FIFO** / `chrt -f` | **`common/util.cc`**：`set_realtime_priority(int level)` 使用 `sched_setscheduler(tid, SCHED_FIFO, &sa)`。注释说明它 *"equivalent to the 'chrt' command"*。优先级为 1–99（与 Lecture-06 相同）。 |
| **CPU affinity**（将任务绑定到核） | **`common/util.cc`**：`set_core_affinity(std::vector<int> cores)` 使用 `sched_setaffinity(tid, ...)` 将调用线程绑定到给定核——避免 RT 线程的迁移和缓存颠簸。 |
| **声明** | **`common/util.h`**：声明 `set_realtime_priority(int level)` 和 `set_core_affinity(std::vector<int> cores)`。 |

相关片段：

- **`openpilot/common/util.cc`**（第 36–61 行）：`set_realtime_priority()`——通过 `gettid` 获取 TID，用 `sched_priority` 填充 `sched_param`，调用 `sched_setscheduler(tid, SCHED_FIFO, &sa)`。
- **`openpilot/common/util.cc`**（第 63–76 行）：`set_core_affinity()`——构建 `cpu_set_t`，调用 `sched_setaffinity(tid, ...)`。

像 `controlsd` 和 `modeld` 这样的进程（或其启动器）在启动时调用这些辅助函数，以 SCHED_FIFO 和绑定的核运行，从而让 CAN 写入和推理满足截止时间。在 openpilot 代码树中搜索 `set_realtime_priority` 和 `set_core_affinity` 以找到确切的调用位置。


<details>
<summary>English original</summary>

**AI Hardware Connection**

- `SCHED_DEADLINE` maps directly to periodic inference tasks: `modeld` at 30fps declares `runtime=10ms, deadline=33ms, period=33ms`; the kernel's admission control proves schedulability and enforces the budget — no user-space watchdog required.
- `SCHED_FIFO` at priority 50–70 is standard for openpilot `controlsd`: CAN bus writes at 100Hz must not be delayed by CFS jitter, which can exceed 5 ms on an untuned multi-process system.
- EEVDF (Linux 6.6) reduces tail latency for mixed workloads — relevant when TensorRT inference, sensor reading, and logging share the same Jetson without full CPU isolation; newly woken inference threads are scheduled sooner than under CFS's vruntime ordering.
- `chrt -f 50 $(pgrep modeld)` is a standard production tuning step on openpilot and Autoware-based AV stacks; for persistent configuration use systemd unit options `CPUSchedulingPolicy=fifo` and `CPUSchedulingPriority=50`.
- `rt_throttling` disabled (`sched_rt_runtime_us = -1`) is used in safety-certified AV ECU deployments where all RT tasks have formally verified bounded CPU usage and non-RT starvation is mitigated by running telemetry at `SCHED_IDLE`.
- `perf sched latency` after a field test surfaces scheduler-induced delays invisible in application-level timing — the primary diagnostic when inference latency increases in deployment vs. bench testing on the same hardware.

**Real example in openpilot (this repo)**

The openpilot code in this roadmap implements the same scheduling concepts from this lecture:

| Lecture concept | Openpilot implementation |
|------------------|---------------------------|
| **SCHED_FIFO** / `chrt -f` | **`common/util.cc`**: `set_realtime_priority(int level)` uses `sched_setscheduler(tid, SCHED_FIFO, &sa)`. The comment states it is *"equivalent to the 'chrt' command"*. Priority is 1–99 (same as Lecture-06). |
| **CPU affinity** (pin task to cores) | **`common/util.cc`**: `set_core_affinity(std::vector<int> cores)` uses `sched_setaffinity(tid, ...)` to pin the calling thread to the given cores — avoids migration and cache thrash for RT threads. |
| **Declarations** | **`common/util.h`**: Declares `set_realtime_priority(int level)` and `set_core_affinity(std::vector<int> cores)`. |

Relevant snippets:

- **`openpilot/common/util.cc`** (lines 36–61): `set_realtime_priority()` — gets TID via `gettid`, fills `sched_param` with `sched_priority`, calls `sched_setscheduler(tid, SCHED_FIFO, &sa)`.
- **`openpilot/common/util.cc`** (lines 63–76): `set_core_affinity()` — builds `cpu_set_t`, calls `sched_setaffinity(tid, ...)`.

Processes like `controlsd` and `modeld` (or their launchers) call these helpers at startup to run with SCHED_FIFO and pinned cores so CAN writes and inference meet deadlines. Search the openpilot tree for `set_realtime_priority` and `set_core_affinity` to find exact call sites.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
