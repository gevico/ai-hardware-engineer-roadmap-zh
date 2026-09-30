---
title: 第 10 讲：无锁编程：RCU、原子操作与内存序
description: 第 10 讲：无锁编程：RCU、原子操作与内存序
---

# 第 10 讲：无锁编程：RCU、原子操作与内存序

## 概述

本讲要解决的核心问题是：能否完全不使用锁就在线程间共享数据？锁是正确的，但开销高昂——它们会导致缓存行弹跳、上下文切换和优先级反转。对于系统中最热的数据路径（30Hz 摄像头帧流水线、1kHz 传感器融合循环、实时模型配置更新），锁开销是可测量且不可接受的。这里要记住的心智模型是：一个图书馆有一条特殊规则：读者总可以不询问任何人就拿起书，但当图书管理员需要用新版本替换一本书时，他们会把旧书留在原处，直到每个已经在读这本书的读者都把它放下。对 AI 硬件工程师而言，无锁技术是零拷贝摄像头流水线、模型实时热重载以及千赫兹级传感器数据聚合的基础。

---

## 无锁的动机

即使没有竞争，锁也会引入不可避免的开销：

- **缓存行弹跳**：竞争期间锁变量在 CPU L1 缓存之间来回弹跳；在 NUMA 上，每次获取要花费 ~100–300 个周期
- **上下文切换**：阻塞的线程会招致调度器开销（每次切换 ~1–10µs）
- **优先级反转**：低优先级的锁持有者延迟高优先级的等待者（见第 11 讲）
- **护航效应**：多个线程在一个慢的持有者后面排队，使热路径串行化

无锁算法使用原子硬件原语，在无需互斥的情况下实现安全的并发访问。它们在性能关键路径上提供更好的可扩展性，并消除优先级反转。

> **关键洞察：**“无锁”并不意味着“无需协调”。它意味着协调通过原子硬件指令完成，而不是通过互斥。这些原子操作仍然有开销——缓存行所有权、内存屏障——但这些开销是有界的且可预测的，而锁竞争则不是。

---

## C++11/C11 内存模型

CPU 和编译器会为性能**重排指令**。没有显式的顺序规则，线程 A 中的写入在任意时长内**可能不可见**于线程 B。**C11/C++11 内存模型**通过 `std::atomic<T>` / `_Atomic T` 定义顺序保证：

| 内存序 | 保证 |
|---|---|
| `memory_order_relaxed` | 仅保证原子性；与其他操作无顺序关系；用于计数器 |
| `memory_order_acquire` | 此点之后的加载/存储不得被重排到它之前；与 release 配对 |
| `memory_order_release` | 此点之前的加载/存储不得被重排到它之后；与 acquire 配对 |
| `memory_order_acq_rel` | 同时具有 acquire + release；用于原子 read-modify-write 操作 |
| `memory_order_seq_cst` | 所有线程间的全局全序；全屏障；`std::atomic<>` 的默认值 |

**Acquire-release 配对**：对变量 X 的 `release` 存储“happens-before”另一个线程中后续对 X 的 `acquire` 加载。release 之前的所有写入在 acquire 之后可见。

```
  Acquire-Release Synchronization:

  Thread A (producer)           Thread B (consumer)
  ─────────────────────         ─────────────────────
  data = 42;                    // reading data before load is wrong
  flag.store(1,                 int f = flag.load(
    memory_order_release);        memory_order_acquire);
                                if (f == 1) {
  ─────────────────────────────► assert(data == 42); // guaranteed
  All writes before release      All loads after acquire
  are visible after acquire      see those writes
```

> **常见陷阱：**将 `memory_order_relaxed` 用于表示“数据已就绪”的标志是常见错误。relaxed 顺序仅保证标志本身的原子性——它不承诺在标志存储之前写入的数据对读取该标志的线程可见。在生产者-消费者信号传递中始终使用 `release`/`acquire`。

---


<details>
<summary>English original</summary>

**Lecture 10: Lock-Free Programming: RCU, Atomics & Memory Ordering**

**Overview**

The core problem this lecture addresses is: can we share data between threads without using locks at all? Locks are correct but expensive — they cause cache-line bouncing, context switches, and priority inversion. For the hottest data paths in a system (a 30Hz camera frame pipeline, a 1kHz sensor fusion loop, a live model configuration update), lock overhead is measurable and unacceptable. The mental model to carry here is that of a library with a special rule: readers can always pick up a book without asking anyone, but when the librarian needs to replace a book with a new edition, they do it by leaving the old book in place until every reader who was already reading it has put it down. For an AI hardware engineer, lock-free techniques are the foundation of zero-copy camera pipelines, live model hot-reload, and sensor data aggregation at kilohertz rates.

---

**Motivation for Lock-Free**

Locks introduce unavoidable costs even when not contended:

- **Cache-line bouncing**: the lock variable ping-pongs between CPU L1 caches during contention; each acquire costs ~100–300 cycles on NUMA
- **Context switches**: blocked threads incur scheduler overhead (~1–10µs per switch)
- **Priority inversion**: low-priority lock holder delays high-priority waiter (see Lecture 11)
- **Convoying**: multiple threads queue behind one slow holder, serializing a hot path

Lock-free algorithms use atomic hardware primitives to achieve safe concurrent access without mutual exclusion. They provide better scalability and eliminate priority inversion on performance-critical paths.

> **Key Insight:** "Lock-free" does not mean "without coordination." It means coordination is done through atomic hardware instructions rather than mutual exclusion. These atomics still have costs — cache-line ownership, memory barriers — but those costs are bounded and predictable in a way that lock contention is not.

---

**C++11/C11 Memory Model**

CPUs and compilers **reorder instructions** for performance. Without explicit ordering rules, a write in thread A **may not be visible** to thread B for an arbitrary time. The **C11/C++11 memory model** defines ordering guarantees via `std::atomic<T>` / `_Atomic T`:

| Memory Order | Guarantee |
|---|---|
| `memory_order_relaxed` | Atomicity only; no ordering relative to other operations; use for counters |
| `memory_order_acquire` | No load/store after this point may be reordered before it; pairs with release |
| `memory_order_release` | No load/store before this point may be reordered after it; pairs with acquire |
| `memory_order_acq_rel` | Both acquire + release; for atomic read-modify-write operations |
| `memory_order_seq_cst` | Total global order across all threads; full fence; default for `std::atomic<>` |

**Acquire-release pairing**: a `release` store to variable X "happens-before" a subsequent `acquire` load from X in another thread. All writes before the release are visible after the acquire.

```
  Acquire-Release Synchronization:

  Thread A (producer)           Thread B (consumer)
  ─────────────────────         ─────────────────────
  data = 42;                    // reading data before load is wrong
  flag.store(1,                 int f = flag.load(
    memory_order_release);        memory_order_acquire);
                                if (f == 1) {
  ─────────────────────────────► assert(data == 42); // guaranteed
  All writes before release      All loads after acquire
  are visible after acquire      see those writes
```

> **Common Pitfall:** Using `memory_order_relaxed` for a flag that signals "data is ready" is a common error. Relaxed order only guarantees atomicity of the flag itself — it makes no promise that the data written before the flag store is visible to the thread that reads the flag. Always use `release`/`acquire` for producer-consumer signaling.

---

</details>

## 硬件内存模型

不同 CPU 架构具有**不同的默认顺序保证**。在阅读使用**裸内存屏障**而非 C11 原子操作的 Linux 内核代码时，理解这一点很重要。

| 架构 | 内存模型 | 屏障要求 |
|---|---|---|
| x86-64 | TSO（Total Store Order）——相对较强 | 几乎不需要显式屏障；原子操作使用 LOCK 前缀 |
| ARM64 | 弱有序 | 需要显式 `DMB`/`DSB` 屏障；原子操作用 `LDADD`/`CAS` |
| RISC-V | 弱有序（RVWMO） | `FENCE` 指令；LL/SC 原子操作用 `LR`/`SC` |

Linux 内核屏障宏：
- `smp_mb()`：完整内存屏障（两个方向的 load + store）
- `smp_rmb()`：仅读（load）屏障
- `smp_wmb()`：仅写（store）屏障

在 ARM64（Jetson Orin、Cortex-A78AE）上，每个 acquire/release 操作都会转换为显式的 `LDAR`/`STLR` 指令。在 x86-64 TSO 上不加屏障也能正确运行的代码，**在 ARM64 上不加屏障可能会静默失败**。对共享变量**始终使用 C11 原子操作或内核屏障宏**，而不要用普通的 load/store。

---

## 原子操作

映射到**单条不可分割的硬件指令**：x86 上的 `LOCK ADD`/`XADD`/`CMPXCHG`，ARMv8.1 LSE 上的 `LDADD`/`CAS`。

```c
atomic_t counter = ATOMIC_INIT(0);      // initialize to zero
atomic_inc(&counter);                   // LOCK XADD on x86; LDADD on ARMv8.1; increment atomically
atomic_dec_and_test(&counter);          // decrement; return true if result is zero (useful for refcounts)
int old = atomic_cmpxchg(&counter, 5, 10);  // CAS: if counter==5, set to 10; return old value either way
atomic_add_return(n, &counter);         // add n, return new value; useful for rate limiting
```

- `atomic_t` 是 32 位；`atomic64_t` 用于 64 位值
- `ATOMIC_INIT(n)` 用于静态初始化
- 用于：引用计数、标志、per-CPU 统计累加器

---

## 比较并交换（CAS）

**大多数无锁数据结构的基础。** CAS 原子地检测一个值，并**仅在其匹配时才替换它**——无需锁即可提供一种“条件写”。

```cpp
std::atomic<int> val{0};
int expected = 0;
// Atomically: if val == expected, set val = 1; return true (success)
// If val != expected: load actual value into expected; return false (failure, retry)
bool ok = val.compare_exchange_strong(expected, 1,
              std::memory_order_acq_rel,  // success: both acquire (for reads after) and release (for writes before)
              std::memory_order_acquire); // failure: acquire only (we read the current value)
// On failure, 'expected' now holds the actual value — retry loop uses this updated value
```

`compare_exchange_weak`：在 LL/SC 架构（ARM、RISC-V）上可能虚假失败；在重试循环中更受青睐，因为它避免了一条单独的 load 指令。

```
  CAS Lock-Free Update Pattern:

  Thread A                      Thread B
  ──────────────────────        ──────────────────────
  read val (= 0)                read val (= 0)
  CAS(val, 0, 1) ──► SUCCESS    CAS(val, 0, 1) ──► FAIL (val is now 1)
  val is now 1                  expected = 1 (updated)
                                CAS(val, 1, 2) ──► SUCCESS
                                val is now 2
```

### ABA 问题

CAS 看到**相同的值，但对象已被替换**：在 load 与 CAS 之间指针 A → B → A；**CAS 错误地成功**。

```
  ABA Problem:

  Thread A: reads ptr = 0xABC0 (points to Node A)
  Thread B: removes Node A, inserts Node C at 0xABC0 (same address, new object!)
  Thread A: CAS(ptr, 0xABC0, new_node) — SUCCEEDS incorrectly
            Node A may be freed; dereference = use-after-free
```

解决方案：
- **版本标签**：在 128 位 CAS 中把单调递增的计数器与指针打包在一起（x86-64 上的 `CMPXCHG16B`）；指针变化始终可检测
- **Hazard pointer**：读者在解引用之前发布指针；回收者在释放之前扫描所有 hazard pointer 槽位
- **RCU**：宽限期机制确保在所有读者完成之前不释放旧对象

> **常见陷阱：** ABA 很隐蔽，因为出错的情形要求精确复用内存地址——测试中容易漏掉，而在会复用已释放内存的生产环境分配器中则很危险。编写基于 CAS 的链表或栈操作时，始终要考虑 ABA。改用 tagged pointer 或 RCU。


<details>
<summary>English original</summary>

**Hardware Memory Models**

Different CPU architectures have **different default ordering guarantees**. Understanding this matters when reading Linux kernel code that uses **bare memory barriers** instead of C11 atomics.

| Architecture | Memory Model | Barrier Requirement |
|---|---|---|
| x86-64 | TSO (Total Store Order) — relatively strong | Few explicit barriers needed; LOCK prefix for atomics |
| ARM64 | Weakly ordered | Explicit `DMB`/`DSB` barriers required; `LDADD`/`CAS` for atomics |
| RISC-V | Weakly ordered (RVWMO) | `FENCE` instruction; `LR`/`SC` for LL/SC atomics |

Linux kernel barrier macros:
- `smp_mb()`: full memory barrier (load + store in both directions)
- `smp_rmb()`: read (load) barrier only
- `smp_wmb()`: write (store) barrier only

On ARM64 (Jetson Orin, Cortex-A78AE), every acquire/release operation translates to explicit `LDAR`/`STLR` instructions. Code that runs correctly on x86-64 TSO without barriers **may silently fail on ARM64** without them. **Always use C11 atomics or kernel barrier macros** rather than plain loads/stores for shared variables.

---

**Atomic Operations**

Map to **single indivisible hardware instructions**: `LOCK ADD`/`XADD`/`CMPXCHG` on x86, `LDADD`/`CAS` on ARMv8.1 LSE.

```c
atomic_t counter = ATOMIC_INIT(0);      // initialize to zero
atomic_inc(&counter);                   // LOCK XADD on x86; LDADD on ARMv8.1; increment atomically
atomic_dec_and_test(&counter);          // decrement; return true if result is zero (useful for refcounts)
int old = atomic_cmpxchg(&counter, 5, 10);  // CAS: if counter==5, set to 10; return old value either way
atomic_add_return(n, &counter);         // add n, return new value; useful for rate limiting
```

- `atomic_t` is 32-bit; `atomic64_t` for 64-bit values
- `ATOMIC_INIT(n)` for static initialization
- Use for: reference counts, flags, per-CPU statistics accumulators

---

**Compare-and-Swap (CAS)**

**Foundation of most lock-free data structures.** CAS atomically tests a value and **replaces it only if it matches** — providing a "conditional write" without a lock.

```cpp
std::atomic<int> val{0};
int expected = 0;
// Atomically: if val == expected, set val = 1; return true (success)
// If val != expected: load actual value into expected; return false (failure, retry)
bool ok = val.compare_exchange_strong(expected, 1,
              std::memory_order_acq_rel,  // success: both acquire (for reads after) and release (for writes before)
              std::memory_order_acquire); // failure: acquire only (we read the current value)
// On failure, 'expected' now holds the actual value — retry loop uses this updated value
```

`compare_exchange_weak`: may spuriously fail on LL/SC architectures (ARM, RISC-V); preferred inside retry loops because it avoids a separate load instruction.

```
  CAS Lock-Free Update Pattern:

  Thread A                      Thread B
  ──────────────────────        ──────────────────────
  read val (= 0)                read val (= 0)
  CAS(val, 0, 1) ──► SUCCESS    CAS(val, 0, 1) ──► FAIL (val is now 1)
  val is now 1                  expected = 1 (updated)
                                CAS(val, 1, 2) ──► SUCCESS
                                val is now 2
```

**ABA Problem**

CAS sees the **same value but the object was replaced**: pointer A → B → A between load and CAS; **CAS succeeds incorrectly**.

```
  ABA Problem:

  Thread A: reads ptr = 0xABC0 (points to Node A)
  Thread B: removes Node A, inserts Node C at 0xABC0 (same address, new object!)
  Thread A: CAS(ptr, 0xABC0, new_node) — SUCCEEDS incorrectly
            Node A may be freed; dereference = use-after-free
```

Solutions:
- **Version tag**: pack a monotonically increasing counter alongside the pointer in a 128-bit CAS (`CMPXCHG16B` on x86-64); pointer change is always detectable
- **Hazard pointers**: reader publishes pointer before dereferencing; reclaimer scans all hazard pointer slots before freeing
- **RCU**: grace period mechanism ensures old objects are not freed until all readers are done

> **Common Pitfall:** ABA is subtle because the buggy case requires exact memory address reuse — easy to miss in testing, dangerous in production allocators that reuse freed memory. Always consider ABA when writing CAS-based list or stack manipulations. Use tagged pointers or RCU instead.

---

</details>

## SPSC 环形缓冲区（无锁）

**单生产者 / 单消费者** 环形缓冲区只需在 head 和 tail 索引上使用 `acquire`/`release` 原子操作。**无锁，payload 数据无缓存行弹跳**。

```c
#define N 256  // must be power of 2 for efficient modulo via bitmask
T buf[N];
atomic_size_t head = 0, tail = 0;  // head: consumer read position, tail: producer write position

// Producer (single thread only — SPSC means exactly one producer)
buf[tail % N] = item;             // write item to current tail slot
// Release store: all writes to buf[tail%N] are visible before this store is seen
atomic_store_explicit(&tail, tail + 1, memory_order_release);

// Consumer (single thread only)
size_t t = atomic_load_explicit(&tail, memory_order_acquire);
// Acquire load: reads tail, and guarantees we see all buf writes that preceded the tail store
if (head != t) {                  // check if there is an item to consume
    item = buf[head % N];         // read item from current head slot
    atomic_store_explicit(&head, head + 1, memory_order_release);
}
```

tail 上的 acquire-release 配对就是同步点：生产者通过更新 tail 释放对数据的所有权；消费者通过读取 tail 获取所有权。因为恰好只有一个写者和一个读者，所以无需加锁。

**吞吐**：仅受内存带宽限制；**无锁开销**。openpilot VisionIPC 用它实现从采集进程到推理进程的**零拷贝相机帧传递**，频率 30Hz。

> **关键洞察：** SPSC 环形缓冲区是 `memory_order_acquire`/`release` 为何存在的典型例证。对 tail 的 `relaxed` 存储本身是原子的，但消费者可能*先*看到更新后的 tail，之后才看到写入 `buf[tail%N]` 的数据。release/acquire 配对建立了显式的顺序保证："当你看到 tail 递增时，buf 中的数据已经就绪。"

---

## RCU（Read-Copy-Update）

Linux 内核中用于**以读为主的共享数据结构**的首要机制。实现了 **O(1) 读侧开销**：无锁、无原子操作、无缓存行写入。RCU 在概念上是本讲中最强大的机制。

概述中的图书管理员类比直接对应 RCU：读者无需询问即可取走书（指针）；图书管理员把新版放到书架上完成替换，然后等所有正在读旧版的人都读完后，再撤下旧版。

### 读者侧

```c
rcu_read_lock();                    // disable preemption only; O(1); no memory barrier emitted on most architectures
ptr = rcu_dereference(gp);          // loads pointer with READ_ONCE + compiler barrier
                                    // prevents compiler from caching or reordering the pointer load
/* safely use *ptr — guaranteed to remain valid until rcu_read_unlock */
rcu_read_unlock();                  // re-enable preemption
```

唯一代价：禁用/启用抢占 —— 一次 per-CPU 标志写入。

### 写者侧

写侧遵循严格的顺序：复制、修改、发布、等待、释放。

1. 分配并填充对象的新版本
2. 原子地发布新指针（使新读者看到新版本）
3. 等待一个**宽限期**（所有先前已存在的读者都已结束）
4. 释放旧对象（安全：已无读者仍持有引用）

```c
new_obj = kmalloc(sizeof(*new_obj), GFP_KERNEL);  // allocate new version
*new_obj = *old_obj;               // copy: start from the current state
new_obj->field = updated_value;    // modify the copy — old_obj is still visible to current readers
rcu_assign_pointer(gp, new_obj);   // atomic publish: smp_wmb() + pointer store; new readers see new_obj
synchronize_rcu();                 // BLOCK until all CPUs have exited any RCU read section that started
                                   // before rcu_assign_pointer — the "grace period"
kfree(old_obj);                    // safe: no pre-existing reader holds old_obj anymore
```

`call_rcu(&old->rcu_head, free_fn)`：异步回调形式；避免阻塞 `synchronize_rcu()`；用于中断上下文，或在写者不可阻塞时使用。

### 宽限期

**宽限期**是从 `rcu_assign_pointer()` 之后开始，直到每个 CPU 都经历一次**静止状态**为止的区间：一次上下文切换、进入 idle，或返回用户空间。宽限期结束后，任何先前已存在的读者都无法再持有指向旧指针的引用。

```
  RCU Grace Period Timeline:

  CPU 0: [RCU read A]──────────────────────[done]
  CPU 1: [RCU read A]──────────[done]
  CPU 2:                 [context switch]           ← quiescent state
  CPU 3: [idle]──────[idle]                         ← quiescent state

  Writer: rcu_assign_pointer(gp, new) ──► ... wait ... ──► kfree(old)
                                          ◄── grace period ──►
                                    (ends when all CPUs have passed
                                     through at least one quiescent state)
```


<details>
<summary>English original</summary>

**SPSC Ring Buffer (Lock-Free)**

**Single-producer / single-consumer** ring buffer requires only `acquire`/`release` atomics on head and tail indices. **No locks, no cache-line bouncing** on payload data.

```c
#define N 256  // must be power of 2 for efficient modulo via bitmask
T buf[N];
atomic_size_t head = 0, tail = 0;  // head: consumer read position, tail: producer write position

// Producer (single thread only — SPSC means exactly one producer)
buf[tail % N] = item;             // write item to current tail slot
// Release store: all writes to buf[tail%N] are visible before this store is seen
atomic_store_explicit(&tail, tail + 1, memory_order_release);

// Consumer (single thread only)
size_t t = atomic_load_explicit(&tail, memory_order_acquire);
// Acquire load: reads tail, and guarantees we see all buf writes that preceded the tail store
if (head != t) {                  // check if there is an item to consume
    item = buf[head % N];         // read item from current head slot
    atomic_store_explicit(&head, head + 1, memory_order_release);
}
```

The acquire-release pair on tail is the synchronization point: the producer releases ownership of the data by updating tail; the consumer acquires ownership by reading tail. No lock needed because there is exactly one writer and one reader.

**Throughput**: limited only by memory bandwidth; **no lock overhead**. Used in openpilot VisionIPC for **zero-copy camera frame passing** from capture process to inference process at 30Hz.

> **Key Insight:** The SPSC ring buffer is the canonical example of why `memory_order_acquire`/`release` exist. A `relaxed` store of tail would be atomic, but the consumer might see the updated tail *before* seeing the data written to `buf[tail%N]`. The release/acquire pair creates an explicit ordering guarantee: "the data in buf is ready by the time you see the tail increment."

---

**RCU (Read-Copy-Update)**

Linux kernel's primary mechanism for **read-mostly shared data structures**. Achieves **O(1) read-side overhead**: no locks, no atomics, no cache-line writes. RCU is conceptually the most powerful mechanism in this lecture.

The librarian analogy from the overview maps directly to RCU: readers pick up the book (pointer) without asking; the librarian replaces it by placing the new edition on the shelf, then waiting until everyone reading the old edition finishes before removing it.

**Reader Side**

```c
rcu_read_lock();                    // disable preemption only; O(1); no memory barrier emitted on most architectures
ptr = rcu_dereference(gp);          // loads pointer with READ_ONCE + compiler barrier
                                    // prevents compiler from caching or reordering the pointer load
/* safely use *ptr — guaranteed to remain valid until rcu_read_unlock */
rcu_read_unlock();                  // re-enable preemption
```

Only cost: disable/enable preemption — a single per-CPU flag write.

**Writer Side**

The write side follows a strict sequence: copy, modify, publish, wait, free.

1. Allocate and populate a new version of the object
2. Atomically publish the new pointer (so new readers see the new version)
3. Wait for a **grace period** (all pre-existing readers have finished)
4. Free the old object (safe: no reader can still hold a reference)

```c
new_obj = kmalloc(sizeof(*new_obj), GFP_KERNEL);  // allocate new version
*new_obj = *old_obj;               // copy: start from the current state
new_obj->field = updated_value;    // modify the copy — old_obj is still visible to current readers
rcu_assign_pointer(gp, new_obj);   // atomic publish: smp_wmb() + pointer store; new readers see new_obj
synchronize_rcu();                 // BLOCK until all CPUs have exited any RCU read section that started
                                   // before rcu_assign_pointer — the "grace period"
kfree(old_obj);                    // safe: no pre-existing reader holds old_obj anymore
```

`call_rcu(&old->rcu_head, free_fn)`: asynchronous callback form; avoids blocking `synchronize_rcu()`; used in interrupt context or when writer must not block.

**Grace Period**

A **grace period** is the interval after `rcu_assign_pointer()` until every CPU has passed through a **quiescent state**: a context switch, entry to idle, or return to user space. After the grace period, no pre-existing reader can hold a reference to the old pointer.

```
  RCU Grace Period Timeline:

  CPU 0: [RCU read A]──────────────────────[done]
  CPU 1: [RCU read A]──────────[done]
  CPU 2:                 [context switch]           ← quiescent state
  CPU 3: [idle]──────[idle]                         ← quiescent state

  Writer: rcu_assign_pointer(gp, new) ──► ... wait ... ──► kfree(old)
                                          ◄── grace period ──►
                                    (ends when all CPUs have passed
                                     through at least one quiescent state)
```

</details>

### RCU 变体

| 变体 | 读段能否睡眠？ | 使用场景 |
|---|---|---|
| Classic RCU | 否 | 路由表、`task_struct` 查找、模块参数 |
| SRCU（Sleepable RCU） | 是 | 通知链、子系统注册 |
| PREEMPT_RCU | 否（但可抢占） | `CONFIG_PREEMPT` kernel |

### rcu_nocbs= 内核参数

`rcu_nocbs=<cpulist>` **将 `call_rcu` 回调卸载**到运行在非隔离 CPU 上的 `rcuoc` 内核线程。消除隔离的 RT 核或推理核上的 RCU 回调调用，去掉一个**不可预测的延迟抖动**来源。

> **关键洞察：** RCU 的威力来自把所有开销都移到写侧。读侧完全自由：无锁、无原子操作、无内存屏障。这就是 RCU 能随读侧数量完美扩展的原因——增加更多读侧带来的开销恰好为零。写侧要付出一个宽限期的代价，但在路由表、进程列表、模型配置这类以读为主的结构上，写操作很少。

---

## kfifo — 内核无锁 SPSC FIFO

```c
DECLARE_KFIFO(my_fifo, int, 64);   // statically declare a SPSC FIFO of 64 ints; size must be power of 2
INIT_KFIFO(my_fifo);               // initialize head and tail indices to 0

kfifo_put(&my_fifo, value);        // producer: enqueue; safe without lock for SPSC
kfifo_get(&my_fifo, &value);       // consumer: dequeue; safe without lock for SPSC
kfifo_len(&my_fifo);               // number of elements currently available
```

在内核驱动中用于 ISR（生产者）与进程上下文（消费者）之间的 RX 数据缓冲区。`kfifo` 是上文所述 SPSC 环形缓冲区模式的内核标准库版本。

---

## Hazard Pointers

用于无锁内存回收的 **RCU 用户态替代方案**：
- 读侧把即将解引用的指针**发布**到每线程的 hazard pointer 槽位
- 释放之前，回收者扫描所有 hazard pointer 槽位查找该指针
- 若找到，则必须延迟；若未找到，则可安全释放

```
  Hazard Pointer Reclamation:

  Thread A reading ptr P:    HP[A] = P         ← "I am using this pointer"
  Thread B freeing ptr P:    scan all HP slots
                             HP[A] == P? YES → defer free
  Thread A done:             HP[A] = NULL
  Thread B retries:          scan all HP slots
                             HP[A] == NULL? YES → kfree(P) safe
```

用于：Folly（Meta）、Java `java.util.concurrent`。`std::hazard_pointer` 已在 C++26 中标准化。

---

## 小结

| 技术 | 读侧开销 | 写侧开销 | 对 ISR 安全？ | 主要限制 |
|---|---|---|---|---|
| 自旋锁 | 缓存行写 + 自旋 | 缓存行写 | 是 | 浪费 CPU；不可睡眠 |
| CAS 循环 | 原子 RMW + 重试 | 原子 RMW + 重试 | 是 | ABA 问题；重试开销 |
| SPSC 环形缓冲区 | Acquire load | Release store | 是（生产者或消费者） | 仅限单生产者且单消费者 |
| RCU | 抢占禁用/启用 | 复制 + 宽限期 | 否（synchronize_rcu 阻塞） | 写侧付出宽限期；以读为主 |
| Hazard pointers | 发布 + 加载 | 扫描所有 HP 槽位 | 否 | 回收开销高于 RCU |
| `kfifo` | Acquire load | Release store | 是 | 仅内核；仅 SPSC |


<details>
<summary>English original</summary>

**RCU Variants**

| Variant | Read Section Can Sleep? | Use Case |
|---|---|---|
| Classic RCU | No | Routing tables, `task_struct` lookup, module parameters |
| SRCU (Sleepable RCU) | Yes | Notifier chains, subsystem registrations |
| PREEMPT_RCU | No (but preemptible) | `CONFIG_PREEMPT` kernels |

**rcu_nocbs= Kernel Parameter**

`rcu_nocbs=<cpulist>` **offloads `call_rcu` callbacks** to `rcuoc` kthreads running on non-isolated CPUs. Eliminates RCU callback invocations from isolated RT or inference cores, removing a source of **unpredictable latency jitter**.

> **Key Insight:** RCU's power comes from moving all overhead to the writer side. Readers are completely free: no lock, no atomic, no memory barrier. This is why RCU scales perfectly with reader count — adding more readers adds exactly zero overhead. The writer pays a grace period, but writers are rare on read-mostly structures like routing tables, process lists, and model configurations.

---

**kfifo — Kernel Lock-Free SPSC FIFO**

```c
DECLARE_KFIFO(my_fifo, int, 64);   // statically declare a SPSC FIFO of 64 ints; size must be power of 2
INIT_KFIFO(my_fifo);               // initialize head and tail indices to 0

kfifo_put(&my_fifo, value);        // producer: enqueue; safe without lock for SPSC
kfifo_get(&my_fifo, &value);       // consumer: dequeue; safe without lock for SPSC
kfifo_len(&my_fifo);               // number of elements currently available
```

Used in kernel drivers for RX data buffers between ISR (producer) and process context (consumer). `kfifo` is the kernel's standard-library version of the SPSC ring buffer pattern described above.

---

**Hazard Pointers**

**Userspace alternative to RCU** for lock-free memory reclamation:
- Reader **publishes** the pointer it is about to dereference into a per-thread hazard pointer slot
- Before freeing, the reclaimer scans all hazard pointer slots for the pointer
- If found, deferral is required; if not found, free is safe

```
  Hazard Pointer Reclamation:

  Thread A reading ptr P:    HP[A] = P         ← "I am using this pointer"
  Thread B freeing ptr P:    scan all HP slots
                             HP[A] == P? YES → defer free
  Thread A done:             HP[A] = NULL
  Thread B retries:          scan all HP slots
                             HP[A] == NULL? YES → kfree(P) safe
```

Used in: Folly (Meta), Java `java.util.concurrent`. `std::hazard_pointer` standardized in C++26.

---

**Summary**

| Technique | Reader Overhead | Writer Overhead | Safe for ISR? | Main Limitation |
|---|---|---|---|---|
| Spinlock | Cache-line write + spin | Cache-line write | Yes | Wasted CPU; no sleep |
| CAS loop | Atomic RMW + retry | Atomic RMW + retry | Yes | ABA problem; retry cost |
| SPSC ring buffer | Acquire load | Release store | Yes (producer or consumer) | Single producer AND single consumer only |
| RCU | Preempt disable/enable | Copy + grace period | No (synchronize_rcu blocks) | Writer pays grace period; read-mostly |
| Hazard pointers | Publish + load | Scan all HP slots | No | Higher reclamation overhead than RCU |
| `kfifo` | Acquire load | Release store | Yes | Kernel-only; SPSC only |

</details>

### 概念回顾

- **为什么 `memory_order_relaxed` 不适合生产者-消费者信令？** Relaxed 只保证原子变量自身的原子性。它不保证在 `relaxed` 存储之前写入的数据，对读取 `relaxed` 变量的线程可见。要用 `release`/`acquire` 建立 happens-before 关系。
- **SPSC 环形缓冲区的根本约束是什么？** 有且仅有一个生产者线程、有且仅有一个消费者线程。存在多个生产者或多个消费者时，head/tail 更新会变成竞态，需要额外的同步（MPMC 队列要复杂得多）。
- **为什么 RCU 无需任何读端锁也能工作？** RCU 借助 CPU 的静止态检测：每次上下文切换、进入 idle 或返回用户空间，都表明该 CPU 上没有正在进行的 RCU 读临界区。内核跟踪这些事件，只有在所有 CPU 都进入静止态之后，才宣告一个宽限期结束。
- **`rcu_nocbs=` 在隔离的推理核上解决了什么问题？** 通常，`call_rcu` 回调在将其入队的那个 CPU 上执行。在隔离的推理核上，这意味着 RCU 回调会在不可预测的时刻于推理循环内部触发。`rcu_nocbs=` 把这些回调卸载到非隔离 CPU 上的辅助线程，从而消除这一延迟来源。
- **CAS 何时失败，重试循环该怎么做？** 当另一个线程在 load 与 CAS 指令之间修改了该值时，CAS 失败。失败时，CAS 会把当前值写入 `expected` 变量。重试循环应重新读取依赖的状态、重新计算期望的新值后再试一次——而不是用过期计算盲目重试。
- **对于模型配置，RCU 与读写锁相比如何？** rwlock 在写者持有写锁期间会阻塞新的读者。RCU 从不阻塞读者——写者在私有副本上操作，然后原子地发布它。对于每个周期都读取配置的 100Hz 推理循环，即使是偶发的短暂写锁阻塞也无法接受。RCU 才是正确选择。

---

## AI 硬件关联

- RCU 支持在线更新模型配置（LoRA 适配器切换、量化配置变更）而不暂停推理线程；写者原子地发布新配置，旧配置只有在所有进行中的前向传播退出读临界区之后才被释放
- 使用 `acquire`/`release` 原子操作的 SPSC 环形缓冲区，为 openpilot VisionIPC 提供零拷贝相机帧流水线，消除了 `camerad` 与 `modeld` 之间 30Hz 视频路径上的 mutex 开销
- 在 Jetson 推理专用核上使用 `rcu_nocbs=`，可消除 RCU 回调执行的抖动，从而在隔离的实时推理核上实现亚毫秒级的最坏情况延迟上界
- openpilot VisionIPC 使用基于 CAS 的无锁队列做多生产者传感器数据汇聚：每个传感器驱动无需 mutex 即可把读数入队；推理线程每个推理周期排空一次队列
- 原子引用计数（`kref`、`std::atomic<int>`）在 V4L2 + CUDA 流水线中管理跨越多个 GPU 消费者的 DMA-BUF 缓冲区生命周期；只有当最后一个消费者把计数递减到零时，缓冲区才被释放
- SPSC 环形缓冲区是 ISR 到推理线程流水线的正确数据结构；在 ISR 中使用 mutex 需要 `spin_lock_irqsave`，会增加延迟；原子环形缓冲区则完全避免了这一点


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why is `memory_order_relaxed` wrong for producer-consumer signaling?** Relaxed only guarantees atomicity of the atomic variable itself. It does not guarantee that data written before a `relaxed` store is visible to a thread that reads the `relaxed` variable. Use `release`/`acquire` to establish a happens-before relationship.
- **What is the fundamental constraint of SPSC ring buffers?** Exactly one producer thread and exactly one consumer thread. With multiple producers or consumers, the head/tail updates become races that require additional synchronization (MPMC queues are significantly more complex).
- **Why does RCU work without any reader-side locks?** RCU leverages the CPU's quiescent state detection: every context switch, idle entry, or user-space return is evidence that no active RCU read section is in progress on that CPU. The kernel tracks these events and declares a grace period complete only after all CPUs have quiesced.
- **What problem does `rcu_nocbs=` solve on isolated inference cores?** Normally, `call_rcu` callbacks execute on the CPU that queued them. On an isolated inference core, this means RCU callbacks fire inside the inference loop at unpredictable times. `rcu_nocbs=` offloads these callbacks to helper threads on non-isolated CPUs, eliminating that latency source.
- **When does CAS fail, and what should the retry loop do?** CAS fails when another thread changed the value between the load and the CAS instruction. On failure, CAS writes the current value into the `expected` variable. The retry loop should re-read dependent state, recompute the desired new value, and try again — not blindly retry with stale computation.
- **How does RCU compare to a reader-writer lock for a model configuration?** An rwlock blocks new readers while a writer holds the write lock. RCU never blocks readers — the writer works on a private copy and publishes it atomically. For a 100Hz inference loop that reads configuration every cycle, even occasional brief write-lock blocking is unacceptable. RCU is the correct choice.

---

**AI Hardware Connection**

- RCU enables live model configuration updates (LoRA adapter swaps, quantization config changes) without pausing inference threads; the writer publishes the new config atomically and the old config is freed only after all in-progress forward passes exit their read sections
- SPSC ring buffer with `acquire`/`release` atomics provides the zero-copy camera frame pipeline in openpilot VisionIPC, eliminating mutex overhead on the 30Hz video path between `camerad` and `modeld`
- `rcu_nocbs=` on Jetson inference-dedicated cores removes RCU callback execution jitter, enabling sub-millisecond worst-case latency bounds on isolated real-time inference cores
- CAS-based lock-free queue is used in openpilot VisionIPC for multi-producer sensor data aggregation: each sensor driver enqueues readings without a mutex; the inference thread drains the queue once per inference cycle
- Atomic reference counts (`kref`, `std::atomic<int>`) manage DMA-BUF buffer lifetimes across multiple GPU consumers in the V4L2 + CUDA pipeline; the buffer is freed only when the last consumer decrements the count to zero
- SPSC ring buffer is the correct data structure for ISR-to-inference-thread pipelines; using a mutex in an ISR would require `spin_lock_irqsave`, adding latency; the atomic ring buffer avoids this entirely

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-10.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-10.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
