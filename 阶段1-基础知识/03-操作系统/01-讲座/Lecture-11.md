---
title: 第 11 讲：死锁、优先级反转与 PI 互斥锁
description: 第 11 讲：死锁、优先级反转与 PI 互斥锁
---

# 第 11 讲：死锁、优先级反转与 PI 互斥锁

## 概述

本讲要解决的核心问题是：当之前各讲中的同步原语被错误使用时，或者正确使用却导致涌现的时序失效时，会发生什么？两类失效模式占主导：死锁（任务永远互相等待）和优先级反转（高优先级任务被低优先级任务阻塞）。这里要建立的心智模型是十字路口的交通死锁，每辆车都挡住下一辆车——没人能动。优先级反转更微妙：设想大楼里最重要的人无法去开会，因为清洁工锁了会议室，然后又被一个慢吞吞的同事堵在后面。对 AI 硬件工程师来说，这些并非理论问题：1997 年 Mars Pathfinder 巡视器就因优先级反转而被复位，而 AV 系统共享了导致该问题的相同架构模式。

---

## 死锁：定义

**死锁**是一种状态，其中一组进程各自等待该集合中另一个进程持有的资源，且没有任何进程能够取得进展。

```
  Deadlock: Circular Wait on Two Resources

  Process A                   Process B
  ─────────────               ─────────────
  holds Lock L1               holds Lock L2
  waiting for Lock L2 ──────► (held by B)
  (held by A) ◄────────────── waiting for Lock L1

  Neither can proceed. Both wait forever.
```

---

## Coffman 条件

**四个条件必须同时成立**，死锁才可能发生。**打破其中任何一个条件都能防止死锁。**

| 条件 | 定义 |
|---|---|
| 互斥 | 至少有一种资源不可共享——同一时刻只能由一个进程持有 |
| 持有并等待 | 进程在等待获取其他资源时，至少持有一个资源 |
| 资源不可抢占 | 资源不能从持有者那里被强制夺走；只能自愿释放 |
| 循环等待 | 资源分配图中存在环：P1 → R1 → P2 → R2 → P1 |

> **关键洞见：** Coffman 条件是设计无死锁系统的检查清单。在编写获取多个锁的代码之前，逐项检查每个条件：能否让资源可共享？能否预先获取所有锁？能否使用 trylock？能否定义全局锁顺序？每个“是”都会移除一个条件并消除死锁。

---

## 死锁预防

在设计时破坏四个条件之一：

| 要打破的条件 | 技术 | 权衡 |
|---|---|---|
| 持有并等待 | 开始前同时获取所有锁；任一获取失败则释放所有锁 | 降低并发性；可能需要重试 |
| 循环等待 | 全局锁顺序：始终按固定且文档化的顺序获取锁 | 要求所有调用点都遵守纪律；lockdep 会强制执行 |
| 不可抢占 | 带随机指数退避的 `mutex_trylock()` | 重试开销；可能发生活锁 |
| 互斥 | 使用无锁数据结构 | 实现复杂度更高 |

**全局锁顺序**是 kernel 和嵌入式代码中最实用的方法。Linux 在代码注释中记录锁顺序，并在 runtime 通过 lockdep 注解强制执行（`lockdep_assert_held`、`lockdep_set_class`）。

分步说明：如何实现全局锁顺序：

1. 枚举子系统中的所有互斥锁/锁。
2. 为每个锁分配一个数字等级（例如，锁 A = 等级 1，锁 B = 等级 2）。
3. 强制规定始终按等级递增的顺序获取锁。
4. 在锁声明处的注释中记录此顺序。
5. 使用 `lockdep_set_class` 为每个锁分配一个类，以便 lockdep 能自动验证该顺序。
6. 在代码审查中，拒绝任何在持有更高等级锁的同时获取更低等级锁的补丁。

> **常见陷阱：** 当两个独立开发者添加新锁而未参考现有顺序时，全局锁顺序会静默失效。一个开发者添加了在持有锁 A 时获取的锁 C；另一个添加了在持有锁 C 时获取的锁 D。两人都不知道在别处也会在持有锁 A 时获取锁 D，从而形成 A→C→D 环。在 CI 中使用 lockdep 来自动捕获这种情况。

---


<details>
<summary>English original</summary>

**Lecture 11: Deadlock, Priority Inversion & PI Mutexes**

**Overview**

The core problem this lecture addresses is: what happens when the synchronization primitives from the previous lectures are used incorrectly, or when correct usage leads to emergent timing failures? Two failure modes dominate: deadlock (tasks waiting for each other forever) and priority inversion (a high-priority task stalled by a low-priority one). The mental model to carry here is that of a traffic deadlock at a four-way intersection where each car is blocking the path of the next — no one can move. Priority inversion is subtler: imagine the most important person in a building being unable to get to their meeting because a janitor locked the conference room and then got stuck behind a slow colleague. For an AI hardware engineer, these are not theoretical problems: the Mars Pathfinder rover was reset by priority inversion in 1997, and AV systems share the same architectural patterns that caused it.

---

**Deadlock: Definition**

A **deadlock** is a state where a set of processes are each waiting for a resource held by another process in the set, and no process can ever make progress.

```
  Deadlock: Circular Wait on Two Resources

  Process A                   Process B
  ─────────────               ─────────────
  holds Lock L1               holds Lock L2
  waiting for Lock L2 ──────► (held by B)
  (held by A) ◄────────────── waiting for Lock L1

  Neither can proceed. Both wait forever.
```

---

**Coffman Conditions**

**All four conditions must hold simultaneously** for a deadlock to be possible. **Breaking any single one prevents deadlock.**

| Condition | Definition |
|---|---|
| Mutual exclusion | At least one resource is non-sharable — only one process may hold it at a time |
| Hold-and-wait | A process holds at least one resource while waiting to acquire additional resources |
| No preemption of resources | Resources cannot be forcibly taken from a holder; only voluntary release |
| Circular wait | A cycle exists in the resource-allocation graph: P1 → R1 → P2 → R2 → P1 |

> **Key Insight:** The Coffman conditions are a checklist for designing deadlock-free systems. Before writing code that acquires multiple locks, check each condition: Can you make resources shareable? Can you acquire all locks upfront? Can you use trylock? Can you define a global lock ordering? Each "yes" removes one condition and eliminates deadlock.

---

**Deadlock Prevention**

Attack one of the four conditions at design time:

| Condition to Break | Technique | Trade-off |
|---|---|---|
| Hold-and-wait | Acquire all locks simultaneously before starting; release all if any acquisition fails | Reduces concurrency; may require retry |
| Circular wait | Global lock ordering: always acquire locks in a fixed, documented order | Requires discipline across all call sites; lockdep enforces this |
| No preemption | `mutex_trylock()` with randomized exponential backoff | Retry overhead; potential livelock |
| Mutual exclusion | Use lock-free data structures | Higher implementation complexity |

**Global lock ordering** is the most practical approach in kernel and embedded code. Linux documents lock ordering in code comments and enforces it at runtime with lockdep annotations (`lockdep_assert_held`, `lockdep_set_class`).

Step-by-step: how to implement global lock ordering:

1. Enumerate all mutexes/locks in your subsystem.
2. Assign each a numeric rank (e.g., Lock A = rank 1, Lock B = rank 2).
3. Mandate that locks are always acquired in ascending rank order.
4. Document this ordering in a comment at the lock declaration site.
5. Use `lockdep_set_class` to assign each lock to a class so lockdep can verify the ordering automatically.
6. In code review, reject any patch that acquires a lower-ranked lock while holding a higher-ranked one.

> **Common Pitfall:** Global lock ordering breaks silently when two independent developers add new locks without consulting the existing ordering. One developer adds Lock C acquired while holding Lock A; another adds Lock D acquired while holding Lock C. Neither knows that Lock D is also acquired while holding Lock A elsewhere, creating an A→C→D cycle. Use lockdep in CI to catch this automatically.

---

</details>

## 死锁检测

当预防的代价过高时，OS 可以在 runtime 检测死锁：

- **资源分配图**：节点为进程（P）与资源（R）；边 P→R 表示 P 等待 R；边 R→P 表示 R 被 P 持有；出现环即表示死锁
- **等待图**：简化形式；边 P→Q 表示 P 等待 Q 持有的某个东西；有环 = 死锁

```
  Resource Allocation Graph:

  P1 ──► R1 ──► P2
  ▲             │
  └──── R2 ◄────┘

  P1 holds R2, waits for R1
  P2 holds R1, waits for R2
  Cycle P1→R1→P2→R2→P1 = DEADLOCK
```

检测后的恢复方案：
1. 终止一个进程；释放其资源；按代价（优先级、runtime、持有的资源）选择牺牲者
2. 从某个进程抢占一个资源并交给另一个进程（需要 rollback 支持）
3. 将某个进程 rollback 到安全的检查点（需要检查点基础设施）

Linux 中的 `lockdep` 以静态分析的方式在 runtime 进行死锁检测，无需等待死锁真正发生。

---

## 优先级反转

**优先级反转**发生在高优先级任务 `H` 通过一条涉及**共享资源**的间接链条**被中优先级任务有效阻塞** `M` 之时。

### 机制

反转按四步展开：

1. `L`（低优先级）获取 mutex `M`
2. `H`（高优先级）被唤醒并尝试获取 mutex `M` —— 阻塞等待 `L`
3. `M`（中优先级）被唤醒并抢占 `L`（优先级上 `M` > `L`）
4. `L` 无法运行以释放 mutex `M`；`H` 尽管优先级最高，仍无限期等待

```
  Priority Inversion Timeline:

  Time ──────────────────────────────────────────────────────────►

  Task H (high):  [woken]──[BLOCKED on mutex held by L]──────────────────────────►
  Task M (med):           ──────────────────────[RUNNING freely]──────────────────►
  Task L (low):   [holds mutex]──[PREEMPTED by M]────────────────[never runs]

  H is effectively running at L's priority — INVERTED
  Duration of inversion: unbounded (as long as M runs)
```

在 `M` 执行的整个期间，`H` 的有效优先级**被反转到低于 `M` 的级别**。没有 PI 时，反转的持续时间是**无界的**。

> **关键洞察：** 优先级反转并不要求各个任务本身存在任何 bug。L、M、H 都按各自的逻辑正确运行。故障是系统性的：正确行为的相互作用产生了涌现出的错误结果。这正是它如此危险的原因 —— 仅靠代码审查无法发现。

---

## Mars Pathfinder 案例研究（1997）

Mars Pathfinder 巡视器在火星表面着陆约 18 小时后出现**周期性系统重启**。这个案例研究是每一门安全关键系统设计课程的**必读内容**，因为该 bug 是在 1.4 亿公里之外被发现的。

**根本原因：无界优先级反转**

| 任务 | 优先级 | 角色 |
|---|---|---|
| `bc_dist` | 高 | 数据分发总线 —— 持有那个关键 mutex |
| `bc_sched` | 低 | 总线调度器 —— 也需要该 mutex 才能释放它 |
| `ASI_MET` | 中 | 气象数据采集 —— CPU 密集型 |

时序：`bc_sched`（低）持有 mutex → `bc_dist`（高）阻塞等待 → `ASI_MET`（中）抢占 `bc_sched` 并持续运行 → `bc_sched` 始终得不到运行 → `bc_dist` 错过其看门狗截止时间 → VxWorks 看门狗定时器触发 → 系统整体重启。

```
  Mars Pathfinder Priority Inversion:

  bc_dist (HIGH):   [woken]──[BLOCKED waiting for mutex]────────────[WATCHDOG RESET]
  ASI_MET (MED):    ──────────────────────[RUNNING]────────────────►
  bc_sched (LOW):   [holds mutex]──[PREEMPTED]─────────────────────[never resumes]
                                   ▲
                              preempted by ASI_MET here
```

**修复**：在共享的 VxWorks mutex 上启用 `PTHREAD_PRIO_INHERIT` 标志。PI 特性在 VxWorks 中已存在，但默认被禁用。该修复通过上行指令上传到已经着陆的巡视器。

**对 AV/机器人领域的教训**：优先级反转可能在已部署、无法接触的硬件上引发安全关键的重启。在安全关键系统中，任何在不同优先级任务之间共享的资源都必须默认使用 PI mutex。

---


<details>
<summary>English original</summary>

**Deadlock Detection**

When prevention is too costly, the OS can detect deadlocks at runtime:

- **Resource allocation graph**: nodes for processes (P) and resources (R); edge P→R means P waits for R; edge R→P means R is held by P; a cycle indicates deadlock
- **Wait-for graph**: simplified form; edge P→Q means P waits for something held by Q; cycle = deadlock

```
  Resource Allocation Graph:

  P1 ──► R1 ──► P2
  ▲             │
  └──── R2 ◄────┘

  P1 holds R2, waits for R1
  P2 holds R1, waits for R2
  Cycle P1→R1→P2→R2→P1 = DEADLOCK
```

Recovery options after detection:
1. Abort one process; free its resources; select victim by cost (priority, runtime, resources held)
2. Preempt a resource from one process and give to another (requires rollback support)
3. Roll back a process to a safe checkpoint (requires checkpointing infrastructure)

`lockdep` in Linux performs static-analysis-style deadlock detection at runtime without waiting for the deadlock to actually occur.

---

**Priority Inversion**

**Priority inversion** occurs when a high-priority task `H` is **effectively blocked by a medium-priority task** `M` through an indirect chain involving a **shared resource**.

**Mechanism**

The inversion unfolds in four steps:

1. `L` (low priority) acquires mutex `M`
2. `H` (high priority) wakes and tries to acquire mutex `M` — blocks waiting for `L`
3. `M` (medium priority) wakes and preempts `L` (`M` > `L` in priority)
4. `L` cannot run to release mutex `M`; `H` waits indefinitely despite being highest priority

```
  Priority Inversion Timeline:

  Time ──────────────────────────────────────────────────────────►

  Task H (high):  [woken]──[BLOCKED on mutex held by L]──────────────────────────►
  Task M (med):           ──────────────────────[RUNNING freely]──────────────────►
  Task L (low):   [holds mutex]──[PREEMPTED by M]────────────────[never runs]

  H is effectively running at L's priority — INVERTED
  Duration of inversion: unbounded (as long as M runs)
```

Effective priority of `H` is **inverted to below `M`'s level** for the entire duration of `M`'s execution. Duration of inversion is **unbounded** without PI.

> **Key Insight:** Priority inversion does not require any bug in the individual tasks. L, M, and H are all behaving correctly according to their own logic. The failure is systemic: the interaction of correct behaviors produces an emergent incorrect outcome. This is why it is so dangerous — code review alone cannot catch it.

---

**Mars Pathfinder Case Study (1997)**

The Mars Pathfinder rover experienced **periodic system resets** approximately 18 hours after landing on the Martian surface. This case study is **required reading** in every safety-critical system design course because the bug was discovered 140 million km away.

**Root cause: unbounded priority inversion**

| Task | Priority | Role |
|---|---|---|
| `bc_dist` | High | Data distribution bus — held the critical mutex |
| `bc_sched` | Low | Bus scheduler — also needed the mutex to release it |
| `ASI_MET` | Medium | Meteorological data acquisition — CPU-intensive |

Sequence: `bc_sched` (low) held the mutex → `bc_dist` (high) blocked waiting → `ASI_MET` (medium) preempted `bc_sched` and ran continuously → `bc_sched` never ran → `bc_dist` missed its watchdog deadline → VxWorks watchdog timer fired → full system reset.

```
  Mars Pathfinder Priority Inversion:

  bc_dist (HIGH):   [woken]──[BLOCKED waiting for mutex]────────────[WATCHDOG RESET]
  ASI_MET (MED):    ──────────────────────[RUNNING]────────────────►
  bc_sched (LOW):   [holds mutex]──[PREEMPTED]─────────────────────[never resumes]
                                   ▲
                              preempted by ASI_MET here
```

**Fix**: enable the `PTHREAD_PRIO_INHERIT` flag on the shared VxWorks mutex. The PI feature existed in VxWorks but was disabled by default. The fix was uploaded to the already-landed rover via uplink command.

**Lesson for AV/robotics**: priority inversion can cause safety-critical resets in deployed, inaccessible hardware. PI mutexes must be the default for any resource shared between tasks of different priorities in safety-critical systems.

---

</details>

## 优先级继承（PI）

当高优先级任务 `H` 阻塞在低优先级任务 `L` 持有的互斥锁上时，kernel 会**临时把** `L` 的调度优先级**提升**到 `H` 的级别。当 `L` 释放该互斥锁后，提升被撤销。

- **传递性 PI**：若 `L` 也阻塞在 `X` 持有的另一把互斥锁上，提升会沿整条阻塞链传播到 `X`
- Linux `rtmutex` 为 kernel 代码和用户态实现 PI（经由 `FUTEX_LOCK_PI`）
- PREEMPT_RT 把大多数 kernel `spinlock_t` 转换为 `rtmutex` → 无需改动驱动代码即可获得全系统范围的 PI

```
  Priority Inheritance in Action:

  BEFORE PI:                          AFTER PI ENABLED:
  H (pri 90): BLOCKED                 H (pri 90): BLOCKED
  M (pri 50): RUNNING                 M (pri 50): BLOCKED (cannot preempt L anymore)
  L (pri 10): PREEMPTED by M          L (pri 10 → boosted to 90): RUNNING
                                                                   releases mutex
                                                                   L returns to pri 10
                                      H (pri 90): UNBLOCKED, runs
```

```c
// POSIX userspace PI mutex
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
// PTHREAD_PRIO_INHERIT: boost holder to waiter's priority when blocked
pthread_mutex_init(&mutex, &attr);
// Requires SCHED_FIFO/SCHED_RR threads for PI to have meaningful effect
// On SCHED_OTHER (CFS), priorities are not strictly enforced and PI has limited benefit
```

> **常见陷阱：** 在未使用 `SCHED_FIFO` 或 `SCHED_RR` 线程的情况下启用 PI 互斥锁，是常见错误。PI 会提升持有者的优先级，但如果所有线程都使用 `SCHED_OTHER`（CFS），"优先级" 的概念就是相对的，提升可能达不到预期效果。PI 在实时调度上下文中最为有效。

---

## 优先级天花板协议

每把互斥锁都被赋予一个**天花板优先级** = 所有可能获取它的任务中的最高优先级。任何获取该互斥锁的任务，在持有期间都会**立即以天花板优先级运行**。

- 完全防止优先级反转，且无需在 runtime 发现优先级
- 需要静态分析：所有可能的加锁者及其优先级必须在设计时已知
- POSIX：`PTHREAD_PRIO_PROTECT` 协议（`pthread_mutexattr_setprotocol`）
- 强制要求者：ARINC 653（航空电子分区 RTOS）、AUTOSAR OS（汽车 ECU）
- 对于任务模型在配置时即已固定的安全认证系统，比 PI 提供更强的形式化保证

```
  Priority Ceiling vs Priority Inheritance:

  PI: ceiling is discovered dynamically at runtime when H blocks
      Overhead: runtime priority boost when contention occurs
      Suitable for: dynamic task models, general POSIX RT

  Priority Ceiling: ceiling is set statically at mutex creation time
      Overhead: every acquisition raises priority to ceiling (even without contention)
      Suitable for: AUTOSAR ECUs, ARINC 653 where task set is fixed
      Stronger guarantee: L's priority is raised BEFORE H blocks, so H never blocks at all
```

从理解死锁预防过渡到理解优先级反转是自然的：两类问题都源于多把锁与多个任务之间的错误交互。区别在于，死锁是活性失败（无进展），而优先级反转是时序失败（结果正确，时机错误）。

---


<details>
<summary>English original</summary>

**Priority Inheritance (PI)**

When high-priority task `H` blocks on a mutex held by low-priority task `L`, the kernel **temporarily boosts** `L`'s scheduling priority to `H`'s level. The boost is removed when `L` releases the mutex.

- **Transitive PI**: if `L` is also blocked on another mutex held by `X`, the boost propagates to `X` along the entire blocking chain
- Linux `rtmutex` implements PI for kernel code and userspace (via `FUTEX_LOCK_PI`)
- PREEMPT_RT converts most kernel `spinlock_t` to `rtmutex` → system-wide PI without changing driver code

```
  Priority Inheritance in Action:

  BEFORE PI:                          AFTER PI ENABLED:
  H (pri 90): BLOCKED                 H (pri 90): BLOCKED
  M (pri 50): RUNNING                 M (pri 50): BLOCKED (cannot preempt L anymore)
  L (pri 10): PREEMPTED by M          L (pri 10 → boosted to 90): RUNNING
                                                                   releases mutex
                                                                   L returns to pri 10
                                      H (pri 90): UNBLOCKED, runs
```

```c
// POSIX userspace PI mutex
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
// PTHREAD_PRIO_INHERIT: boost holder to waiter's priority when blocked
pthread_mutex_init(&mutex, &attr);
// Requires SCHED_FIFO/SCHED_RR threads for PI to have meaningful effect
// On SCHED_OTHER (CFS), priorities are not strictly enforced and PI has limited benefit
```

> **Common Pitfall:** Enabling PI mutexes without using `SCHED_FIFO` or `SCHED_RR` threads is a common mistake. PI boosts the holder's priority, but if all threads use `SCHED_OTHER` (CFS), the concept of "priority" is relative and the boost may not have the desired effect. PI is most effective in real-time scheduling contexts.

---

**Priority Ceiling Protocol**

Each mutex is assigned a **ceiling priority** = highest priority of any task that will ever acquire it. Any task acquiring the mutex **immediately runs at the ceiling priority** for the duration of ownership.

- Prevents priority inversion entirely without needing runtime priority discovery
- Requires static analysis: all potential lock acquirers and their priorities must be known at design time
- POSIX: `PTHREAD_PRIO_PROTECT` protocol (`pthread_mutexattr_setprotocol`)
- Mandated by: ARINC 653 (avionics partitioned RTOS), AUTOSAR OS (automotive ECUs)
- Stronger formal guarantees than PI for safety-certified systems where the task model is fixed at configuration time

```
  Priority Ceiling vs Priority Inheritance:

  PI: ceiling is discovered dynamically at runtime when H blocks
      Overhead: runtime priority boost when contention occurs
      Suitable for: dynamic task models, general POSIX RT

  Priority Ceiling: ceiling is set statically at mutex creation time
      Overhead: every acquisition raises priority to ceiling (even without contention)
      Suitable for: AUTOSAR ECUs, ARINC 653 where task set is fixed
      Stronger guarantee: L's priority is raised BEFORE H blocks, so H never blocks at all
```

The transition from understanding deadlock prevention to understanding priority inversion is natural: both problems arise from incorrect interactions between multiple locks and multiple tasks. The difference is that deadlock is a liveness failure (no progress) while priority inversion is a timing failure (correct result, wrong time).

---

</details>

## lockdep — 实时死锁检测器

`CONFIG_PROVE_LOCKING` 会启用 lockdep，即 kernel 的 runtime 锁依赖图校验器：

```bash
CONFIG_PROVE_LOCKING=y
CONFIG_LOCK_STAT=y      # adds per-lock contention statistics
```

- 依据锁在 kernel 二进制中的静态地址，为每个锁分配一个 **lock class**
- 记录每一条锁获取链：“获取锁 B 时正持有锁 A”
- 在首次检测到 AB-BA 环时就报告——早于其表现为实际挂死
- 报告非法的从中断上下文加锁：例如在 hardirq 上下文中调用 `mutex_lock()`

```
WARNING: possible circular locking dependency detected
task/1234 is trying to acquire lock:
  (&lockB){...}, at: function_b+0x30
but task holds lock:
  (&lockA){...}, taken at: function_a+0x20
which lock already depends on the new lock.
```

在所有开发、CI 和回归测试中启用。`lockdep` 会增加约 10% 的 runtime 开销；在生产环境或性能敏感场景中禁用。

`lockdep_assert_held(&lock)`：用于记录并验证在给定位置持有某个锁的断言；对具有多步加锁协议的复杂驱动很有用。

> **关键洞察：** lockdep 的强大之处在于，当某条新的加锁顺序路径首次被走通时，它就能检测出*潜在*死锁——即便那种精确的线程交错从未导致过实际挂死。它构建一张全局依赖图，并立即报告图中的环。这远比等待那些在生产环境中引发实际死锁的罕见 timing 条件要可靠。

---

## 作为最后手段的看门狗定时器

硬件或软件**看门狗**：如果 process 未能在超时时间内**喂狗**，系统复位或进入安全状态。对已部署硬件中逃过 lockdep 检测的死锁提供**纵深防御**。

Mars Pathfinder 的看门狗工作正常；它正确检测到 `bc_dist` 错过了其截止时间。根本问题在于缺失的 PI 配置，正是这一配置最初导致 `bc_dist` 错过截止时间。

> **常见陷阱：** 把看门狗复位当作优先级反转的可接受恢复机制是错误的。车辆 ADAS 控制器上的看门狗复位会导致一次受控退出——驾驶员必须接管。最坏情况下，复位发生在驾驶员无暇反应的那一刻。正确的修法是消除反转，而不是依赖看门狗。

---

## 小结

| 问题 | 症状 | 检测工具 | 解决方案 |
|---|---|---|---|
| 死锁 | 系统挂死；任务永久阻塞 | 资源分配图；lockdep | 加锁顺序；一次性全部获取；`trylock` + 退避 |
| 优先级反转 | 高优先级任务意外停滞 | 延迟监控；看门狗超时 | PI mutex（`rtmutex`、`PTHREAD_PRIO_INHERIT`）；优先级天花板 |
| 活锁 | 任务在运行但不推进 | CPU 性能剖析（100% 利用率，无吞吐） | 随机退避；集中仲裁 |
| 饥饿 | 低优先级任务永不运行 | 长等待监控；`perf sched` | 老化；FIFO 等待队列；优先级提升 |


<details>
<summary>English original</summary>

**lockdep — Live Deadlock Detector**

`CONFIG_PROVE_LOCKING` enables lockdep, the kernel's runtime lock dependency graph validator:

```bash
CONFIG_PROVE_LOCKING=y
CONFIG_LOCK_STAT=y      # adds per-lock contention statistics
```

- Assigns each lock a **lock class** based on its static address in the kernel binary
- Records every lock acquisition chain: "lock A was held when lock B was acquired"
- Reports AB-BA cycles at first detection — before they manifest as actual hangs
- Reports invalid lock-from-interrupt context: e.g., `mutex_lock()` called from hardirq context

```
WARNING: possible circular locking dependency detected
task/1234 is trying to acquire lock:
  (&lockB){...}, at: function_b+0x30
but task holds lock:
  (&lockA){...}, taken at: function_a+0x20
which lock already depends on the new lock.
```

Enable during all development, CI, and regression testing. `lockdep` adds ~10% runtime overhead; disable in production or performance-sensitive contexts.

`lockdep_assert_held(&lock)`: assertion that documents and verifies that a lock is held at a given point; useful for complex drivers with multi-step lock protocols.

> **Key Insight:** lockdep's power is that it detects a *potential* deadlock the first time a new lock ordering path is taken — even if that exact thread interleaving has never caused an actual hang. It builds a global dependency graph and reports cycles in that graph immediately. This is far more reliable than waiting for the rare timing conditions that cause actual deadlocks in production.

---

**Watchdog Timers as Last Resort**

Hardware or software **watchdog**: if a process does not **kick the watchdog** within the timeout period, the system resets or enters a safe state. Provides **defense-in-depth** against deadlocks that escape lockdep in deployed hardware.

The Mars Pathfinder watchdog worked correctly; it correctly detected `bc_dist` missing its deadline. The root problem was the missing PI configuration that caused `bc_dist` to miss the deadline in the first place.

> **Common Pitfall:** Treating watchdog resets as an acceptable recovery mechanism for priority inversion is wrong. A watchdog reset on a vehicle's ADAS controller causes a controlled disengagement — the driver must take over. In the worst case, the reset occurs at a moment when the driver has no time to react. The correct fix is to eliminate the inversion, not to rely on the watchdog.

---

**Summary**

| Problem | Symptom | Detection Tool | Solution |
|---|---|---|---|
| Deadlock | System hangs; tasks blocked forever | Resource allocation graph; lockdep | Lock ordering; acquire-all-at-once; `trylock` + backoff |
| Priority inversion | High-priority task stalls unexpectedly | Latency monitoring; watchdog timeout | PI mutex (`rtmutex`, `PTHREAD_PRIO_INHERIT`); priority ceiling |
| Livelock | Tasks run but make no progress | CPU profiling (100% util, no throughput) | Randomized backoff; central arbitration |
| Starvation | Low-priority task never runs | Long wait monitoring; `perf sched` | Aging; FIFO wait queues; priority boosting |

</details>

### 概念回顾

- **为什么即使每个单独任务的行为都正确，优先级反转仍会发生？** 反转是任务交互的涌现属性，而不是任何单个任务的 bug。L 正确地持有 mutex。H 正确地等待它。M 正确地抢占 L。问题在于，没有人把系统设计成能阻止经由 mutex 形成的间接阻塞链 L→H。
- **Mars Pathfinder 最重要的单一教训是什么？** 优先级继承在 VxWorks 中本来就有，但默认是禁用的。安全关键系统必须审计不同优先级任务之间共享的每一个 mutex，并确保 PI 已启用。「该特性存在」不等于「该特性已启用」。
- **优先级继承与优先级天花板有什么区别？** PI 在高优先级等待者阻塞时，动态提升锁持有者的优先级。优先级天花板则立即把获取者提升到天花板值，即使没有高优先级任务在等待。PI 的平均开销更低；优先级天花板提供更强的可预测性保证。
- **lockdep 报告什么，不报告什么？** lockdep 报告潜在的死锁环（AB-BA 顺序）和锁上下文违规（IRQ 上下文中的睡眠锁）。它不报告优先级反转——那需要 `cyclictest` 之类的独立工具或实时调度分析。
- **为什么传递性 PI 很重要？** 如果 H 阻塞在 L 持有的 mutex 上，而 L 又阻塞在 X 持有的另一个 mutex 上，那么 X 也需要被提升——否则 L 无法运行，也就无法释放自己的 mutex 来解除 H 的阻塞。传递性 PI 会沿整条阻塞链传播提升。
- **什么时候优先级天花板是强制而非仅推荐？** 在 AUTOSAR OS 和 ARINC 653 环境中，完整任务集及其资源使用在配置时固定并经过形式化验证。这些标准要求对优先级反转的有界性给出形式化证明，而优先级天花板在静态上提供了这一点。

---

## AI 硬件关联

- 当 openpilot `controlsd`（最高优先级）与 `plannerd`（中）及日志写入进程（低）共享 cereal 共享内存状态时，需要 PI mutex（`rtmutex` / `PTHREAD_PRIO_INHERIT`）；没有 PI，Mars Pathfinder 的场景可能在生产 AV 系统中重演
- Mars Pathfinder 根因分析是 ASIL-B 安全评审中的强制性参考文档；它表明优先级反转能在已部署、无法接近的硬件上引发安全关键的复位，且完全没有人工干预的可能
- `PTHREAD_PRIO_INHERIT` 用于 ROS2 实时 executor 配置，以防止控制回调被数据记录线程饿死；rclcpp 实时 executor 文档明确引用了这一要求
- `lockdep` 是在 Jetson 和 i.MX 平台上进行嵌入式 Linux 驱动开发时的标准做法；camera、ISP 和 DMA 子系统锁中的 AB-BA 环必须在硬件部署前消除
- 优先级天花板协议用于符合 AUTOSAR 的 ECU 软件，其中所有任务优先级和共享资源集在配置时固定；对于 ASIL-D 认证的安全功能，这比 PI 更强
- PREEMPT_RT 把 `spinlock_t` 全系统转换为 `rtmutex`，意味着 PI 会自动应用到 CAN 总线驱动、V4L2 摄像头驱动和 GPU 驱动的所有内核同步上，无需任何驱动代码改动


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why can priority inversion occur even when every individual task is behaving correctly?** Inversion is an emergent property of task interactions, not a bug in any one task. L holds a mutex correctly. H waits for it correctly. M preempts L correctly. The problem is that no one designed the system to prevent the indirect blocking chain L→H via the mutex.
- **What is the single most important lesson of Mars Pathfinder?** Priority inheritance was available in VxWorks but disabled by default. Safety-critical systems must audit every mutex that is shared between tasks of different priorities and ensure PI is enabled. "The feature exists" is not the same as "the feature is enabled."
- **How does priority inheritance differ from priority ceiling?** PI boosts the lock holder's priority dynamically when a higher-priority waiter blocks. Priority ceiling boosts the acquirer to the ceiling immediately, even if no high-priority task is waiting. PI has lower average overhead; priority ceiling has stronger predictability guarantees.
- **What does lockdep report and what does it not report?** lockdep reports potential deadlock cycles (AB-BA orderings) and lock-context violations (sleeping lock in IRQ context). It does not report priority inversion — that requires separate tools like `cyclictest` or real-time scheduling analysis.
- **Why is transitive PI important?** If H blocks on a mutex held by L, and L blocks on a different mutex held by X, then X also needs to be boosted — otherwise L cannot run, cannot release its mutex to unblock H. Transitive PI propagates the boost along the entire blocking chain.
- **When is priority ceiling mandatory instead of just recommended?** In AUTOSAR OS and ARINC 653 environments where the complete task set and their resource usage is fixed at configuration time and formally verified. These standards require formal proof of bounded priority inversion, which priority ceiling provides statically.

---

**AI Hardware Connection**

- PI mutex (`rtmutex` / `PTHREAD_PRIO_INHERIT`) is required for openpilot `controlsd` (highest priority) sharing cereal shared-memory state with `plannerd` (medium) and log-writing processes (low); without PI, the Mars Pathfinder scenario can recur in a production AV system
- The Mars Pathfinder root cause analysis is a mandatory reference document in ASIL-B safety reviews; it demonstrates that priority inversion can cause safety-critical resets in deployed, inaccessible hardware with zero possibility of manual intervention
- `PTHREAD_PRIO_INHERIT` is used in ROS2 real-time executor configurations to prevent control callbacks from being starved by data-logging threads; the rclcpp real-time executor documentation explicitly references this requirement
- `lockdep` is standard practice during embedded Linux driver development on Jetson and i.MX platforms; AB-BA cycles in camera, ISP, and DMA subsystem locks must be eliminated before hardware deployment
- Priority ceiling protocol is used in AUTOSAR-compliant ECU software where all task priorities and shared resource sets are fixed at configuration time; this is stronger than PI for ASIL-D certified safety functions
- PREEMPT_RT's system-wide conversion of `spinlock_t` to `rtmutex` means that PI is automatically applied to all kernel synchronization on the CAN bus driver, V4L2 camera driver, and GPU driver without any driver code changes

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-11.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-11.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
