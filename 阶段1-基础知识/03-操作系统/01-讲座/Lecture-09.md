---
title: 第 9 讲：同步：自旋锁、互斥锁、读写锁与顺序锁
description: 第 9 讲：同步：自旋锁、互斥锁、读写锁与顺序锁
---

# 第 9 讲：同步：自旋锁、互斥锁、读写锁与顺序锁

## 概述

本讲要解决的核心问题是：多个 CPU 或线程如何安全地访问同一份数据而不破坏它？当两个线程同时写入同一内存位置时，结果是未定义的——这就是竞态条件。同步原语是防止竞态的契约，它确保同一时刻只有一个写者（或多个读者）能访问共享状态。这里要带着的心智模型是办公室里的共享白板：自旋锁就是有人站在白板前原地打转、等着轮到自己；互斥锁就是有人先去坐下干别的活，直到被通知；顺序锁就是读者复制白板内容，然后检查在复制期间有没有人擦掉过它。对 AI 硬件工程师而言，选错同步原语，可能意味着一个能在 100Hz 下工作的摄像头驱动，和一个每次帧交接都引入 50µs 延迟的驱动之间的差别。

---

## 互斥问题

**竞态条件**发生在多个 CPU 或线程在没有协调的情况下并发访问共享状态，且结果取决于调度顺序时。

正确互斥的三项要求：

1. **互斥**：同一时刻至多有一个线程处于临界区
2. **进展**：如果没有线程处于临界区，等待的线程必须最终能进入
3. **有界等待**：没有线程无限等待（无饥饿）

> **关键洞察：** 选择正确的同步原语不只是一个正确性决策——它还是一个性能决策。选错会增加延迟、降低吞吐，或引入优先级反转。本讲的每种原语都有特定使用场景；在本该用顺序锁的地方用互斥锁，会让写者白白浪费 CPU 周期，而它们根本不会发生争用。

---

## 自旋锁（`spinlock_t`）

在原子 test-and-set 指令上**忙等待（自旋）**，直到锁可用。持有期间会**禁用本地 CPU 上的抢占**。

```c
spin_lock(&lock);                        // acquire; disables preemption on local CPU
/* critical section */
spin_unlock(&lock);                      // release; re-enables preemption

// When data is shared with an interrupt service routine:
spin_lock_irqsave(&lock, flags);         // disables preemption + local IRQs; saves IRQ state
/* critical section (safe against ISR) */
spin_unlock_irqrestore(&lock, flags);    // restores IRQ state + re-enables preemption
// flags saves the interrupt enable/disable state before the lock — not a priority number
// This is necessary because the caller may have already disabled IRQs before acquiring the lock
```

关键约束：
- 仅适用于**极短的临界区**（<1µs）；持有期间睡眠是内核 bug
- 在 NUMA 系统上，争用的自旋锁会导致跨 socket 的**缓存行弹跳**
- 在 `PREEMPT_RT` 下：`spinlock_t` 变成会睡眠的 `rtmutex`；`raw_spinlock_t` 保留真正的自旋行为

```
  Spinlock Acquisition Flow:

  Thread A              Thread B (attempting lock)
  ────────────          ────────────────────────────
  spin_lock(&L)         spin_lock(&L)
  [lock acquired]       [lock busy → spin in tight loop]
  ... critical ...      test-and-set ... test-and-set ...
  spin_unlock(&L)  ──►  [lock acquired]
                        ... critical ...
                        spin_unlock(&L)

  Note: Thread B burns CPU cycles while waiting.
  This is intentional for very short waits where a context switch
  would cost more than the spin itself.
```

> **常见陷阱：** 如果自旋锁下的临界区调用了任何可能睡眠的函数——`kmalloc(GFP_KERNEL)`、`copy_from_user()`、`msleep()`——内核会在 PREEMPT_RT 内核上触发 BUG_ON，而在标准内核上会静默破坏状态。要审计自旋锁持有区内的每一行，排查可能的睡眠点。

---


<details>
<summary>English original</summary>

**Lecture 9: Synchronization: Spinlocks, Mutexes, RW Locks & Seqlocks**

**Overview**

The core problem this lecture addresses is: how do multiple CPUs or threads safely access the same data without corrupting it? When two threads write to the same memory location simultaneously, the result is undefined — this is a race condition. Synchronization primitives are the contracts that prevent races by ensuring only one writer (or multiple readers) can access shared state at a time. The mental model to carry here is that of a shared whiteboard in an office: a spinlock is someone standing in front of it spinning in place waiting for their turn; a mutex is someone going to sit down and do other work until notified; a seqlock is a reader who copies the board and then checks whether anyone erased it while they were copying. For an AI hardware engineer, choosing the wrong synchronization primitive can be the difference between a camera driver that works at 100Hz and one that introduces 50µs of latency on every frame handoff.

---

**Mutual Exclusion Problem**

A **race condition** occurs when multiple CPUs or threads access shared state concurrently without coordination, and the outcome depends on scheduling order.

Three requirements for correct mutual exclusion:

1. **Mutual exclusion**: at most one thread in the critical section at a time
2. **Progress**: if no thread is in the critical section, a waiting thread must eventually enter
3. **Bounded waiting**: no thread waits indefinitely (no starvation)

> **Key Insight:** Choosing the right synchronization primitive is not just a correctness decision — it is a performance decision. The wrong choice can add latency, reduce throughput, or introduce priority inversion. Each primitive in this lecture has a specific use case; using a mutex where a seqlock is appropriate wastes CPU cycles on writers that never contend.

---

**Spinlock (`spinlock_t`)**

**Busy-waits (spins)** on an atomic test-and-set instruction until the lock is available. **Disables preemption** on the local CPU while held.

```c
spin_lock(&lock);                        // acquire; disables preemption on local CPU
/* critical section */
spin_unlock(&lock);                      // release; re-enables preemption

// When data is shared with an interrupt service routine:
spin_lock_irqsave(&lock, flags);         // disables preemption + local IRQs; saves IRQ state
/* critical section (safe against ISR) */
spin_unlock_irqrestore(&lock, flags);    // restores IRQ state + re-enables preemption
// flags saves the interrupt enable/disable state before the lock — not a priority number
// This is necessary because the caller may have already disabled IRQs before acquiring the lock
```

Key constraints:
- Only for **very short critical sections** (<1µs); holding while sleeping is a kernel bug
- On NUMA systems, contended spinlocks cause **cache-line bouncing** across sockets
- Under `PREEMPT_RT`: `spinlock_t` becomes a sleeping `rtmutex`; `raw_spinlock_t` retains true spin behavior

```
  Spinlock Acquisition Flow:

  Thread A              Thread B (attempting lock)
  ────────────          ────────────────────────────
  spin_lock(&L)         spin_lock(&L)
  [lock acquired]       [lock busy → spin in tight loop]
  ... critical ...      test-and-set ... test-and-set ...
  spin_unlock(&L)  ──►  [lock acquired]
                        ... critical ...
                        spin_unlock(&L)

  Note: Thread B burns CPU cycles while waiting.
  This is intentional for very short waits where a context switch
  would cost more than the spin itself.
```

> **Common Pitfall:** If the critical section under a spinlock calls any function that can sleep — `kmalloc(GFP_KERNEL)`, `copy_from_user()`, `msleep()` — the kernel will BUG_ON on a PREEMPT_RT kernel and silently corrupt state on a standard kernel. Audit every line inside a spinlock-held section for potential sleep points.

---

</details>

## 互斥锁（`struct mutex`）

任务在锁不可用时 **阻塞**（`TASK_UNINTERRUPTIBLE`）；调度器运行其他任务。**不会因自旋而浪费 CPU**。

```c
mutex_lock(&mutex);                    // blocks until acquired; process context only
/* critical section */
mutex_unlock(&mutex);

mutex_trylock(&mutex);                 // non-blocking; returns 1 if acquired, 0 if not
// Use trylock when you can do useful work while waiting (e.g., process other requests)

mutex_lock_interruptible(&mutex);      // returns -EINTR if signal received while waiting
// Preferred in syscall handlers so that kill/ctrl-C can interrupt a blocked task
```

- 不能在中断上下文中使用——中断不能睡眠
- 用户空间等价物：`pthread_mutex_t`，由 `futex` 支撑（无竞争时快路径避免系统调用）
- 内核 `mutex` 与 `pthread_mutex_t` 不同；实现和规则都不一样

从自旋锁到互斥锁的转变体现了 **CPU 与调度器的权衡**：自旋锁浪费 CPU 周期，却避免了上下文切换开销。对于 **长于几微秒** 的临界区，让任务睡眠几乎总是更划算。

### rtmutex——优先级继承互斥锁

`rtmutex` 是一种带 **优先级继承（PI）** 的互斥锁：
- 当高优先级任务 `H` 阻塞在由低优先级任务 `L` 持有的 `rtmutex` 上时，内核临时把 `L` 的优先级提升到 `H` 的级别
- `L` 更早运行，更早释放锁，`H` 随即解除阻塞；`L` 在释放时恢复原优先级
- 在 `PREEMPT_RT` 下，大多数内核 `spinlock_t` 实例会被替换为 `rtmutex`
- 在内核代码中避免 Mars Pathfinder 场景（见第 11 讲）

> **关键洞见：** 优先级继承正是 `rtmutex` 区别于普通互斥锁之处。没有 PI，等待低优先级任务所持锁的高优先级任务，可能被抢占锁持有者的中优先级任务无限期拖延。有了 PI，锁持有者的优先级会被提升到与等待者相同，从而消除这种无限期拖延。

---

## 读写信号量（`struct rw_semaphore`）

允许 **多个并发读者** **或** **一个独占写者**；但二者不能同时。当 **读远多于写** 时最为理想——这是配置表和模型权重结构的常见模式。

```c
down_read(&rw_semaphore);          // shared read lock; multiple readers can hold concurrently
/* read shared data */
up_read(&rw_semaphore);

down_write(&rw_semaphore);         // exclusive write lock; blocks all readers + other writers
/* modify shared data */
up_write(&rw_semaphore);

// Downgrades write lock to read lock without releasing (atomic, no window of unlocked state)
downgrade_write(&rw_semaphore);
```

- 睡眠锁；仅限进程上下文；不能在中断上下文中使用
- 写者优先：一旦有写者等待，新的读者就会被阻塞，以防止写者饥饿
- 用于：`mmap_lock`（在 `mm_struct` 中）、文件系统 inode 锁、设备驱动配置表
- 针对中断上下文存在自旋锁变体 `rwlock_t`（忙等；不睡眠）

```
  RW Semaphore Access Patterns:

  Timeline ──────────────────────────────────────────────────►

  Reader A: ──[read]──────────────────────────────
  Reader B:    ──[read]──────────────────────       (concurrent with A)
  Reader C:         ──[read]──────────────────      (concurrent with A and B)
  Writer D:                        ──[WAIT]──[write]──
                                    ▲
                              blocks new readers after this point
  Reader E:                        ──[WAIT]──────[read]──
                                               ▲
                                         resumes after writer finishes
```

---


<details>
<summary>English original</summary>

**Mutex (`struct mutex`)**

Task **blocks** (`TASK_UNINTERRUPTIBLE`) if lock is unavailable; scheduler runs other tasks. **No CPU wasted spinning**.

```c
mutex_lock(&mutex);                    // blocks until acquired; process context only
/* critical section */
mutex_unlock(&mutex);

mutex_trylock(&mutex);                 // non-blocking; returns 1 if acquired, 0 if not
// Use trylock when you can do useful work while waiting (e.g., process other requests)

mutex_lock_interruptible(&mutex);      // returns -EINTR if signal received while waiting
// Preferred in syscall handlers so that kill/ctrl-C can interrupt a blocked task
```

- Cannot use in interrupt context — interrupts cannot sleep
- Userspace equivalent: `pthread_mutex_t` backed by `futex` (fast path avoids syscall when uncontended)
- Kernel `mutex` is not the same as `pthread_mutex_t`; different implementation and rules

The transition from spinlock to mutex mirrors the **CPU vs. scheduler trade-off**: spinlocks waste CPU cycles but avoid context switch overhead. For critical sections **longer than a few microseconds**, putting the task to sleep is almost always cheaper.

**rtmutex — Priority Inheritance Mutex**

`rtmutex` is a mutex with **priority inheritance (PI)**:
- When high-priority task `H` blocks on an `rtmutex` held by low-priority task `L`, the kernel temporarily boosts `L`'s priority to `H`'s level
- `L` runs sooner, releases the lock sooner, `H` unblocks; `L` returns to original priority on release
- Under `PREEMPT_RT`, most kernel `spinlock_t` instances are replaced with `rtmutex`
- Prevents the Mars Pathfinder scenario (see Lecture 11) in kernel code

> **Key Insight:** Priority inheritance is what separates `rtmutex` from a regular mutex. Without PI, a high-priority task waiting for a lock held by a low-priority task can be stalled indefinitely by medium-priority tasks that preempt the lock holder. With PI, the lock holder's priority is raised to match the waiter's, eliminating that indefinite stall.

---

**Read/Write Semaphore (`struct rw_semaphore`)**

Allows **multiple concurrent readers** **or** **one exclusive writer**; not both simultaneously. This is ideal when **reads are far more frequent than writes** — a common pattern for configuration tables and model weight structures.

```c
down_read(&rw_semaphore);          // shared read lock; multiple readers can hold concurrently
/* read shared data */
up_read(&rw_semaphore);

down_write(&rw_semaphore);         // exclusive write lock; blocks all readers + other writers
/* modify shared data */
up_write(&rw_semaphore);

// Downgrades write lock to read lock without releasing (atomic, no window of unlocked state)
downgrade_write(&rw_semaphore);
```

- Sleeping lock; process context only; cannot use in interrupt context
- Writer-bias: once a writer is waiting, new readers are blocked to prevent writer starvation
- Used for: `mmap_lock` (in `mm_struct`), filesystem inode locks, device driver config tables
- Spinlock variant `rwlock_t` exists for interrupt context (busy-waits; not sleeping)

```
  RW Semaphore Access Patterns:

  Timeline ──────────────────────────────────────────────────►

  Reader A: ──[read]──────────────────────────────
  Reader B:    ──[read]──────────────────────       (concurrent with A)
  Reader C:         ──[read]──────────────────      (concurrent with A and B)
  Writer D:                        ──[WAIT]──[write]──
                                    ▲
                              blocks new readers after this point
  Reader E:                        ──[WAIT]──────[read]──
                                               ▲
                                         resumes after writer finishes
```

---

</details>

## Seqlock (`seqlock_t`)

**写者永不阻塞**。读者通过**序列计数器**检测并发写入，必要时重试。Seqlock 用**偶发的读者重试**换取**写者零阻塞**——当写者需要保证前进时，这就是正确的选择。

```c
// Writer — always proceeds immediately, never blocks on readers
write_seqlock(&seqlock);
/* sequence counter incremented to ODD (signals: write in progress) */
/* modify shared data */
write_sequnlock(&seqlock);
/* sequence counter incremented to EVEN (signals: write complete) */

// Reader — retries if a concurrent write was detected
unsigned seq;
do {
    seq = read_seqbegin(&seqlock);   // sample counter; if ODD, a write is in progress (spin/retry)
    /* read shared data into local variables */
} while (read_seqretry(&seqlock, seq));  // if counter changed, our read may be torn — retry
// After the loop, local variables hold a consistent snapshot
```

Seqlock 机制之所以成立，是因为写入期间计数器始终为奇数，其余时候为偶数。读者若在计数器为奇数时开始读取，就知道要立即重试。读者若读完发现计数器已变化，就知道自己拿到的数据不一致。

特性：
- 写者开销：两次原子计数器自增；无阻塞
- 无写入路径上的读者开销：两次读取计数器（极其廉价）
- 写入路径上的读者开销：重试循环（写入不频繁时很少发生）
- 不适用于包含指针的数据（读者可能在重试检测到变化之前就解引用已释放的指针）

Linux 内核在以下场景使用 seqlock：`jiffies_64`、用于时钟读取的 `timespec64`、内核 `xtime` 计时、`vDSO` 时间变量。

> **关键洞见：** Seqlock 反转了通常的锁权衡。多数锁阻塞写者以保护读者。Seqlock 让写者自由推进，读者若运气不好就重试。当写者是高优先级实时任务（例如更新时间戳的传感器 ISR）、读者是低优先级消费者时，这是正确的选择。

> **常见陷阱：** 绝不要用 seqlock 保护含指针的数据。如果读者读到一个指针，随后写者替换了该对象并释放旧的，读者可能在 `read_seqretry` 检测到变化之前就解引用已释放的指针。含指针的数据结构请用 RCU（第 10 讲）。

---

## Completion (`struct completion`)

**一次性同步**：线程 A 等待由线程 B 发出信号的事件。对于一次性事件，它比信号量更干净，因为其意图是**显式**的，且**没有虚假唤醒风险**。

```c
DECLARE_COMPLETION(dma_done);         // static declaration; starts in "not complete" state

// Waiter thread
wait_for_completion(&dma_done);               // blocks in TASK_UNINTERRUPTIBLE until signaled
wait_for_completion_timeout(&dma_done, HZ);   // with timeout; returns 0 on timeout, remaining jiffies on success
wait_for_completion_interruptible(&dma_done); // can be interrupted by a signal (returns -ERESTARTSYS)

// Signaler (e.g., DMA completion ISR)
complete(&dma_done);      // wake one waiter (FIFO order)
complete_all(&dma_done);  // wake all waiters simultaneously

reinit_completion(&dma_done);  // reset for reuse after complete_all
```

使用场景：DMA 传输完成通知、kthread 启动同步、驱动 probe 顺序编排、FPGA 固件加载完成。

用 completion 时同步流程毫不含糊：ISR 发出 "DMA done" 信号，等待中的线程随即恢复。与之相比，互斥锁用于保护访问，信号量用于统计可用资源，而 completion 专门用于 "wait until event X happens"。

---


<details>
<summary>English original</summary>

**Seqlock (`seqlock_t`)**

**Writers never block**. Readers detect concurrent writes via a **sequence counter** and retry if needed. The seqlock trades **occasional reader retries** for **zero writer blocking** — the right choice when writers need guaranteed forward progress.

```c
// Writer — always proceeds immediately, never blocks on readers
write_seqlock(&seqlock);
/* sequence counter incremented to ODD (signals: write in progress) */
/* modify shared data */
write_sequnlock(&seqlock);
/* sequence counter incremented to EVEN (signals: write complete) */

// Reader — retries if a concurrent write was detected
unsigned seq;
do {
    seq = read_seqbegin(&seqlock);   // sample counter; if ODD, a write is in progress (spin/retry)
    /* read shared data into local variables */
} while (read_seqretry(&seqlock, seq));  // if counter changed, our read may be torn — retry
// After the loop, local variables hold a consistent snapshot
```

The seqlock mechanism works because the counter is always odd during a write and even otherwise. A reader that starts when the counter is odd knows to retry immediately. A reader that finishes and finds the counter changed knows its data is inconsistent.

Properties:
- Writer overhead: two atomic counter increments; no blocking
- Reader overhead on write-free path: two reads of the counter (extremely cheap)
- Reader overhead on write path: retry loop (rare if writes are infrequent)
- Not suitable for pointer-containing data (reader may dereference freed pointer before retry detects the change)

Linux kernel uses seqlocks for: `jiffies_64`, `timespec64` for clock reads, kernel `xtime` timekeeping, `vDSO` time variables.

> **Key Insight:** Seqlocks invert the usual lock trade-off. Most locks block writers to protect readers. Seqlocks let writers proceed freely and ask readers to retry if they were unlucky. This is the right choice when the writer is a high-priority real-time task (e.g., a sensor ISR updating a timestamp) and readers are lower-priority consumers.

> **Common Pitfall:** Never use a seqlock to protect data containing pointers. If a reader reads a pointer, then a writer replaces the object and frees the old one, the reader may dereference the freed pointer before `read_seqretry` detects the change. Use RCU (Lecture 10) for pointer-containing data structures.

---

**Completion (`struct completion`)**

**One-shot synchronization**: thread A waits for an event signaled by thread B. Cleaner than a semaphore for one-time events because its intent is **explicit** and it has **no spurious-wakeup risk**.

```c
DECLARE_COMPLETION(dma_done);         // static declaration; starts in "not complete" state

// Waiter thread
wait_for_completion(&dma_done);               // blocks in TASK_UNINTERRUPTIBLE until signaled
wait_for_completion_timeout(&dma_done, HZ);   // with timeout; returns 0 on timeout, remaining jiffies on success
wait_for_completion_interruptible(&dma_done); // can be interrupted by a signal (returns -ERESTARTSYS)

// Signaler (e.g., DMA completion ISR)
complete(&dma_done);      // wake one waiter (FIFO order)
complete_all(&dma_done);  // wake all waiters simultaneously

reinit_completion(&dma_done);  // reset for reuse after complete_all
```

Use cases: DMA transfer complete notification, kthread startup synchronization, driver probe sequencing, FPGA firmware load completion.

With completions, the synchronization flow is unambiguous: the ISR signals "DMA done" and the waiting thread resumes. Compare this to a mutex (which guards access) or a semaphore (which counts available resources) — a completion is specifically for "wait until event X happens."

---

</details>

## lockdep — 锁依赖校验器

上文描述的所有同步原语都可能被**错误使用**。`lockdep` 是 kernel 的 **runtime 工具，用于在开发期捕获这些错误**，避免它们演变成难以复现的**死锁**。

`CONFIG_PROVE_LOCKING` 启用 lockdep，即 kernel 的 runtime 锁依赖校验器：

- 根据锁在二进制中的静态地址，为每个锁分配一个 **lock class**
- 记录锁获取链：“持有锁 A 时获取了锁 B”
- 首次出现时即上报 **AB-BA 环**（潜在死锁）——早于其表现为实际挂死
- 上报**从中断中加锁的违规**：在中断上下文中获取会睡眠的锁
- 输出：`WARNING: possible circular locking dependency`，并在 `dmesg` 中给出完整栈回溯

```bash
# Enable in kernel config
CONFIG_PROVE_LOCKING=y
CONFIG_LOCK_STAT=y    # adds lock contention statistics per lock class

# View lock stats after a workload run
cat /proc/lock_stat | head -40
# Output shows: lock name, acquisition count, contention count, wait time histogram
```

**驱动开发期间务必启用**，回归测试也应启用。`lockdep` 增加约 10% 开销；**生产环境禁用**。

> **Key Insight:** `lockdep` 在*首次*观察到某个新的加锁顺序时就能检出潜在死锁——即便该具体执行顺序从未真正死锁过。这比等待实际挂死更强，因为实际挂死可能只在生产环境的罕见时序条件下才会出现。可以把 lockdep 看作一个在 runtime 运行的静态分析器。

---

## Summary

| Primitive | Context Usable | Blocks? | PI Support | Best For |
|---|---|---|---|---|
| `spinlock_t` | Process + IRQ | No (busy-wait) | No | 短 CS（<1µs）、ISR 共享数据 |
| `raw_spinlock_t` | Process + IRQ | No (busy-wait) | No | 硬件关键路径、不能用会睡眠的锁 |
| `mutex` | Process only | Yes | No | 进程上下文中较长的 CS |
| `rtmutex` | Process only | Yes | Yes | RT 任务、PREEMPT_RT kernel |
| `rwlock_t` | Process + IRQ | No (spin) | No | 读多写少、短 CS、IRQ 上下文 |
| `rw_semaphore` | Process only | Yes | No | 读多写少、较长 CS |
| `seqlock_t` | Process + IRQ（写者）；retry（读者） | 写者：No；读者：retry | No | 极少写、频繁读的数据 |
| `struct completion` | Process only | Yes | No | 一次性事件通知 |

### Conceptual Review

- **什么时候该用 spinlock 而不是 mutex？** 临界区非常短（<1µs）时，处于中断上下文（不能睡眠）时，或上下文切换的开销超过自旋等待时间时。除此之外，或处于进程上下文时，mutex 几乎总是更好的选择。
- **`rtmutex` 与 `mutex` 的区别是什么？** 优先级继承。当高优先级任务阻塞在低优先级任务持有的 rtmutex 上时，低优先级任务的优先级会被临时提升，以避免优先级反转。普通 `mutex` 没有 PI 机制。
- **为什么不能在中断处理程序里用 mutex？** 中断处理程序不能睡眠。锁不可用时，mutex 会阻塞调用者，也就是让当前执行上下文睡眠。中断处理程序没有“当前任务”可睡眠——它运行在被中断的那段代码之上。
- **seqlock 的基本假设是什么？** 写少读多。若写频繁，读者会不断自旋重试，seqlock 反而比 mutex 更差。它还假设受保护数据中不含可能在撕裂读期间被解引用的指针。
- **lockdep 能捕获哪类 bug？** AB-BA 死锁环（CPU 0 上先锁 A 再锁 B，CPU 1 上先锁 B 再锁 A）以及在中断上下文中获取会睡眠的锁的违规。它在首次观察到新顺序时就捕获这些问题，早于实际死锁发生。
- **与 ISR 共享数据时，为什么用 `spin_lock_irqsave` 而不是 `spin_lock`？** ISR 可以抢占持有 spinlock 的线程。若该 ISR 随后尝试获取同一 spinlock，就会永远自旋——死锁。`spin_lock_irqsave` 会先禁用本地中断，防止锁被持有期间 ISR 运行。

---


<details>
<summary>English original</summary>

**lockdep — Lock Dependency Validator**

All the synchronization primitives described above can be **used incorrectly**. `lockdep` is the kernel's **runtime tool to catch those mistakes** at development time, before they manifest as hard-to-reproduce **deadlocks**.

`CONFIG_PROVE_LOCKING` enables lockdep, the kernel's runtime lock dependency validator:

- Assigns each lock a **lock class** based on its static address in the binary
- Records lock acquisition chains: "lock A held when lock B was acquired"
- Reports **AB-BA cycles** (potential deadlocks) at first occurrence — before they manifest as actual hangs
- Reports **lock-from-interrupt violations**: sleeping lock acquired in interrupt context
- Output: `WARNING: possible circular locking dependency` with full stack traces in `dmesg`

```bash
# Enable in kernel config
CONFIG_PROVE_LOCKING=y
CONFIG_LOCK_STAT=y    # adds lock contention statistics per lock class

# View lock stats after a workload run
cat /proc/lock_stat | head -40
# Output shows: lock name, acquisition count, contention count, wait time histogram
```

**Always enable during driver development** and regression testing. `lockdep` adds ~10% overhead; **disable in production**.

> **Key Insight:** `lockdep` detects potential deadlocks the *first time* a new lock ordering is observed — even if that specific execution order has never actually deadlocked. This is more powerful than waiting for an actual hang, which may only occur under rare timing conditions in production. Think of lockdep as a static analyzer that runs at runtime.

---

**Summary**

| Primitive | Context Usable | Blocks? | PI Support | Best For |
|---|---|---|---|---|
| `spinlock_t` | Process + IRQ | No (busy-wait) | No | Short CS (<1µs), ISR-shared data |
| `raw_spinlock_t` | Process + IRQ | No (busy-wait) | No | Hardware-critical, cannot be sleeping lock |
| `mutex` | Process only | Yes | No | Longer CS in process context |
| `rtmutex` | Process only | Yes | Yes | RT tasks, PREEMPT_RT kernel |
| `rwlock_t` | Process + IRQ | No (spin) | No | Read-heavy, short CS, IRQ context |
| `rw_semaphore` | Process only | Yes | No | Read-heavy, longer CS |
| `seqlock_t` | Process + IRQ (writer); retry (reader) | Writer: No; Reader: retry | No | Rarely-written, frequently-read data |
| `struct completion` | Process only | Yes | No | One-shot event signaling |

**Conceptual Review**

- **When should you use a spinlock instead of a mutex?** When the critical section is very short (<1µs), when you are in interrupt context (cannot sleep), or when the cost of a context switch exceeds the spin wait time. For anything longer or in process context, a mutex is almost always better.
- **What makes `rtmutex` different from `mutex`?** Priority inheritance. When a high-priority task blocks on an rtmutex held by a low-priority task, the low-priority task's priority is temporarily raised to avoid priority inversion. Regular `mutex` has no PI mechanism.
- **Why can't you use a mutex in an interrupt handler?** Interrupt handlers cannot sleep. A mutex blocks the caller when the lock is unavailable, which means putting the current execution context to sleep. An interrupt handler has no "current task" to sleep — it runs on top of whatever was interrupted.
- **What is the seqlock's fundamental assumption?** That writes are rare and reads are frequent. If writes are frequent, readers spin-retry constantly and the seqlock becomes worse than a mutex. It also assumes the protected data contains no pointers that could be dereferenced during a torn read.
- **What class of bugs does lockdep catch?** AB-BA deadlock cycles (lock A then B on CPU 0, lock B then A on CPU 1) and sleeping-lock-in-interrupt-context violations. It catches these the first time a new ordering is observed, before an actual deadlock occurs.
- **Why use `spin_lock_irqsave` instead of `spin_lock` when sharing data with an ISR?** An ISR can preempt the thread holding a spinlock. If the ISR then tries to acquire the same spinlock, it spins forever — deadlock. `spin_lock_irqsave` disables local interrupts first, preventing the ISR from running while the lock is held.

---

</details>

## AI 硬件关联

- 摄像头 DMA 完成中断服务程序（ISR）中的 `spin_lock_irqsave` 保护与推理线程共享的当前帧缓冲索引；不得在中断上下文中睡眠；临界区只是一次索引写入
- `rw_semaphore` 用于模型权重热重载：多个并发推理线程在前向传播期间持有读锁；权重更新器仅在指针交换期间持有写锁，因此推理被阻塞的时间不会超过交换本身
- Seqlock 将硬件时间戳从传感器采集线程传到融合流水线，而不会因慢读者阻塞传感器线程；读者重试可忽略不计，因为传感器写入以 100Hz 发生，而读取以 1kHz 进行
- `completion` 用于 FPGA 加速器驱动中的 “DMA transfer done” 同步：主机 CPU 提交命令缓冲区，阻塞在 `wait_for_completion` 上，FPGA 中断处理程序在处理完成时调用 `complete()`
- 带优先级继承的 `rtmutex` 在 PREEMPT_RT AV 计算平台上必不可少；所有 `spinlock_t` 锁在整个系统中都变为 `rtmutex` 实例，无需修改驱动代码即可在每个内核子系统中提供 PI
- Jetson 嵌入式驱动开发期间的 `lockdep` 会在硬件部署到车辆或机器人之前，捕获摄像头、ISP（图像信号处理器）和 DMA 子系统中的 AB-BA 锁顺序违规


<details>
<summary>English original</summary>

**AI Hardware Connection**

- `spin_lock_irqsave` in a camera DMA completion ISR protects the current frame buffer index shared with the inference thread; must not sleep in interrupt context; critical section is a single index write
- `rw_semaphore` for model weight hot-reload: many concurrent inference threads hold the read lock during forward passes; the weight updater holds the write lock only during the pointer swap, so inference is never stalled longer than the swap itself
- Seqlock passes hardware timestamps from the sensor acquisition thread to the fusion pipeline without blocking the sensor thread on slow readers; reader retries are negligible since sensor writes occur at 100Hz against reads at 1kHz
- `completion` for "DMA transfer done" synchronization in FPGA accelerator drivers: the host CPU submits a command buffer, blocks on `wait_for_completion`, and the FPGA interrupt handler calls `complete()` when processing finishes
- `rtmutex` with priority inheritance is essential on PREEMPT_RT AV compute platforms; all `spinlock_t` locks become `rtmutex` instances system-wide, providing PI in every kernel subsystem without modifying driver code
- `lockdep` during Jetson embedded driver development catches AB-BA lock ordering violations in camera, ISP, and DMA subsystems before the hardware is deployed in a vehicle or robot

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-09.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-09.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
