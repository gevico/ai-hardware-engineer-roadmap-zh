---
title: 讲义 05（L10、L11）：无锁编程（RCU、原子操作）与死锁 / 优先级反转
description: 讲义 05（L10、L11）：无锁编程（RCU、原子操作）与死锁 / 优先级反转
---

# 讲义 05（L10、L11）：无锁编程（RCU、原子操作）与死锁 / 优先级反转

**涵盖：** L10 讲（无锁：RCU、原子操作、内存序）与 L11 讲（死锁、优先级反转、PI 互斥锁）。

---

## 本讲义的组织结构

1. **第 1 部分 —— 无锁：** 为何要避免锁；C11 内存模型；原子操作与 CAS；RCU；SPSC 环形缓冲区；hazard pointer。
2. **第 2 部分 —— 死锁与优先级反转：** Coffman 条件；预防（锁顺序、trylock）；优先级反转与 Mars Pathfinder；PI 与优先级天花板；lockdep；watchdog。

---

# 第 1 部分：无锁编程 —— RCU、原子操作与内存序

**背景：** 即使没有竞争，锁也会带来开销：**缓存行弹跳**、上下文切换、优先级反转。在热路径（相机流水线、传感器融合、模型配置）上，**无锁技术用原子操作和 RCU** 在不做互斥的前提下完成协调。

---

## 为什么用无锁？

- **缓存行弹跳：** 多个 CPU 反复获取和释放同一把锁时，锁所在的内存位置会在它们各自的缓存之间来回传递。这种传递（即“弹跳”）在 NUMA 系统上会造成约 100–300 个周期的延迟，因为每个 CPU 都必须先从另一个 CPU 的缓存取回最新副本才能继续。
- **上下文切换：** 被阻塞的线程要承担调度器开销（约 1–10 µs）。
- **优先级反转：** 低优先级的持有者拖慢高优先级的等待者（见第 2 部分）。
- **护航效应：** 众多线程排队堵在一个慢速持有者后面。

**无锁编程**指的是不使用传统锁（如互斥锁）而在线程之间进行协调。它依靠**原子**硬件指令（例如 compare-and-swap，即 CAS）以及 RCU 这类机制。“无锁”并不意味着没有协调——只是协调改用原子操作完成，相比锁，这通常能提供**更可预测、更可控的开销**。

---

## C11/C++11 内存模型

现代 CPU 和编译器经常**重排（reorder）内存操作**以提升性能，这可能导致一个线程的更新不会立即被另一个线程看到，除非显式加以控制。**C11/C++11 内存模型**描述了允许这类重排的方式与时机，并给出工具（C++ 中的 `std::atomic<T>` / C 中的 `_Atomic T`）来指定线程之间所需的可见性与顺序保证类型。


| 内存序 | 保证内容 |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| `memory_order_relaxed` | 只保证该操作是原子的（不会被中断），但对任何内存访问的顺序完全不作承诺。 |
| `memory_order_acquire` | 保证代码中写在该操作*之后*的所有加载与存储确实会在它*之后*发生（在该 CPU 上）。 |
| `memory_order_release` | 保证代码中写在该操作*之前*的所有加载与存储确实会在它*之前*发生（在该 CPU 上）。 |
| `memory_order_acq_rel` | 同时结合 acquire 和 release：适用于 read-modify-write（如 CAS），保证该操作前后的顺序。 |
| `memory_order_seq_cst` | 最强：仿佛所有线程间的这类操作存在单一全序；原子操作通常的默认值。 |


### 何时以及为何要有效地使用 `memory_order_relaxed`？

当你需要原子性（变量上没有竞争），但**不**需要在线程之间同步或排序内存时，就用 `memory_order_relaxed`。它对其他内存的可见性或顺序*不*作任何保证——只保证每个操作是原子的（不可分割）。

#### 有效的使用场景：

- **计数器、统计、ID 生成器：** 多个线程递增一个共享计数器，但递增的*确切次序*或*时机*无关紧要，只要不丢失任何一次递增。
- **不控制其他数据发布的标志位或进度标记：** 例如上报心跳、进度或状态更新，此时漏掉最新的值是可以接受的，其他数据也不需要与该标志位同步。
- **随机数生成器（原子更新种子）：** 只要求原子更新，一致性不依赖于与其他内存的时序关系。

#### 何时不要用：

- 生产者—消费者或交接（hand-off），即一个线程在继续之前必须看到另一个线程*所有*先前的写入。（见前面的例子——`memory_order_relaxed` *无法*保证这一点，并且会破坏正确性。）


<details>
<summary>English original</summary>

**Lecture Note 05 (L10, L11): Lock-Free Programming (RCU, Atomics) & Deadlock / Priority Inversion**

**Combines:** Lecture L10 (Lock-Free: RCU, Atomics, Memory Ordering) and Lecture L11 (Deadlock, Priority Inversion, PI Mutexes).

---

**How This Note Is Organized**

1. **Part 1 — Lock-free:** Why avoid locks; C11 memory model; atomics and CAS; RCU; SPSC ring buffer; hazard pointers.
2. **Part 2 — Deadlock & priority inversion:** Coffman conditions; prevention (lock ordering, trylock); priority inversion and Mars Pathfinder; PI and priority ceiling; lockdep; watchdog.

---

**Part 1: Lock-Free Programming — RCU, Atomics & Memory Ordering**

**Context:** Locks add cost even when uncontended: **cache-line bouncing**, context switches, priority inversion. On hot paths (camera pipeline, sensor fusion, model config), **lock-free techniques use atomics and RCU** to coordinate without mutual exclusion.

---

**Why Lock-Free?**

- **Cache-line bouncing:** When multiple CPUs repeatedly acquire and release the same lock, the lock's memory location is transferred back and forth between their caches. This transfer (the "bounce") causes delays of ~100–300 cycles on NUMA systems, as each CPU must fetch the latest copy from another CPU's cache before proceeding.
- **Context switches:** Blocked threads pay scheduler overhead (~1–10 µs).
- **Priority inversion:** Low-priority holder delays high-priority waiter (see Part 2).
- **Convoying:** Many threads queue behind one slow holder.

**Lock-free programming** means coordinating between threads without using traditional locks (like mutexes). Instead, it relies on **atomic** hardware instructions (such as compare-and-swap, or CAS) and mechanisms like RCU. "Lock-free" doesn't mean there's no coordination—rather, the coordination is done using atomics, which generally provides **more predictable and bounded costs** compared to locks.

---

**C11/C++11 Memory Model**

Modern CPUs and compilers often **rearrange (reorder) memory operations** to improve performance, which can lead to one thread's updates not being visible to another thread right away, unless explicitly controlled. The **C11/C++11 memory model** describes how and when such reordering is allowed and gives tools (`std::atomic<T>` in C++ / `_Atomic T` in C) to specify the kind of visibility and ordering guarantees required between threads.


| Memory Order           | What It Guarantees                                                                                                                         |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| `memory_order_relaxed` | Only guarantees that this operation is atomic (can't be interrupted), but makes no promises at all about the order of any memory accesses. |
| `memory_order_acquire` | Guarantees that all loads and stores written *after* this operation in your code will really happen *after* it (on this CPU).              |
| `memory_order_release` | Guarantees that all loads and stores written *before* this operation in your code will really happen *before* it (on this CPU).            |
| `memory_order_acq_rel` | Combines both acquire and release: useful for read-modify-write (like CAS), guarantees before+after ordering on this operation.            |
| `memory_order_seq_cst` | Strongest: behaves as if there is a single total order for all such operations amongst all threads; the usual default for atomics.         |


**When and why to use `memory_order_relaxed` effectively?**

`memory_order_relaxed` is used when you need atomicity (no races on a variable), but you do **not** need to synchronize or order memory between threads. It gives *no* guarantees about visibility or ordering of other memory—only that each operation is atomic (indivisible).

**Effective use cases:**

- **Counters, statistics, ID generators:** When multiple threads increment a shared counter, but the *exact sequence* or *timing* of increments doesn't matter, only that no increments are lost.
- **Flags or progress markers that do NOT control publication of other data:** For example, reporting heartbeats, progress, or status updates, where missing the very latest value is acceptable and other data does not need to be synchronized with the flag.
- **Random number generators (atomic seed update):** You just want atomic update, consistency doesn't depend on timing with other memory.

**When NOT to use:**

- Producer–consumer or hand-off, where one thread must see *all* previous writes from another before proceeding. (See earlier example — `memory_order_relaxed` *cannot* guarantee this and can break correctness.)

</details>

#### 示例：用 `memory_order_relaxed` 保证安全（原子计数器）

```cpp
std::atomic<uint64_t> requests_handled{0};

void worker_thread() {
    // ... handle a request ...
    requests_handled.fetch_add(1, std::memory_order_relaxed);
    // No need to synchronize with any other memory.
}
```

这里，`memory_order_relaxed` 是正确且最高效的：它能防止更新丢失（原子性），但不会引入昂贵的内存栅栏，因为顺序无关紧要。

#### 关键要点：

- 仅当你需要原子性，而**不**需要跨线程可见性或其他数据的顺序时，才使用 `memory_order_relaxed`。
- 若要发布数据或在线程间传递信号，请使用 `memory_order_release`（生产者）和 `memory_order_acquire`（消费者）。

> 总结：`memory_order_relaxed` 最快，但只有在除原子变量本身之外，你不关心任何内容的顺序或可见性时才是安全的。

**获取-释放配对（完整示例）：**  
对变量 X 的 **release** 存储会与另一线程中随后对 X 的 **acquire** 加载建立“先行发生”关系。这意味着，一个线程中在 release 存储之前执行的所有内存写入，保证对在观察到该值之后执行 acquire 加载的线程可见。

### 示例：用 C++11 原子操作实现生产者–消费者

```cpp
#include <atomic>
#include <cassert>
#include <thread>
#include <iostream>

std::atomic<int> flag{0};
int data = 0;

void producer() {
    data = 42;  // (1) Store data first
    flag.store(1, std::memory_order_release);  // (2) Signal with release store
}

void consumer() {
    while (flag.load(std::memory_order_acquire) != 1) {
        // spin/wait until producer signals
    }
    // (3) After acquire load observes "1" in flag, all writes before the release are visible
    assert(data == 42);  // Always succeeds!
    std::cout << "Consumer sees data: " << data << std::endl;
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
    return 0;
}
```

**这演示了什么：**  

- 生产者写入 `data`，然后执行 `flag.store(..., memory_order_release)`
- 消费者自旋，直到通过 `flag.load(..., memory_order_acquire)` 看到 `flag == 1`
- 看到标志置位后，消费者*保证*能看到 `data == 42`，因为跨线程时，release 存储之前的所有写入在 acquire 加载之后都可见。

> **常见陷阱：** 用 `memory_order_relaxed` 作为“数据就绪”标志只能保证标志本身的原子性——它*不*保证在标志存储之前写入的负载对读取者可见。生产者–消费者信号传递一定要用 **release**（生产者）/ **acquire**（消费者）。


<details>
<summary>English original</summary>

**Example: Safe with `memory_order_relaxed` (atomic counter)**

```cpp
std::atomic<uint64_t> requests_handled{0};

void worker_thread() {
    // ... handle a request ...
    requests_handled.fetch_add(1, std::memory_order_relaxed);
    // No need to synchronize with any other memory.
}
```

Here, `memory_order_relaxed` is correct and most efficient: it prevents lost updates (atomicity), but doesn't incur expensive memory fences since ordering is irrelevant.

**Key takeaway:**

- Use `memory_order_relaxed` only when you want atomicity, **not** cross-thread visibility or ordering of other data.  
- For publishing data or signaling between threads, use `memory_order_release` (producer) and `memory_order_acquire` (consumer).

> In summary: `memory_order_relaxed` is fastest, but safe only for cases where you do not care about the order or visibility of anything except the atomic variable itself.

**Acquire-release pairing (Full Example):**  
A **release** store to variable X establishes a "happens-before" relationship with a subsequent **acquire** load from X in another thread. This means that all memory writes performed before the release store in one thread are guaranteed to be visible to the thread that does the acquire load after observing the value.

**Example: Producer–Consumer with C++11 Atomics**

```cpp
#include <atomic>
#include <cassert>
#include <thread>
#include <iostream>

std::atomic<int> flag{0};
int data = 0;

void producer() {
    data = 42;  // (1) Store data first
    flag.store(1, std::memory_order_release);  // (2) Signal with release store
}

void consumer() {
    while (flag.load(std::memory_order_acquire) != 1) {
        // spin/wait until producer signals
    }
    // (3) After acquire load observes "1" in flag, all writes before the release are visible
    assert(data == 42);  // Always succeeds!
    std::cout << "Consumer sees data: " << data << std::endl;
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
    return 0;
}
```

**What this demonstrates:**  

- The producer writes to `data`, then does a `flag.store(..., memory_order_release)`
- The consumer spins until it sees `flag == 1` via `flag.load(..., memory_order_acquire)`
- After seeing the flag set, the consumer is *guaranteed* to see `data == 42`, as all writes before the release store are visible after the acquire load across threads.

> **Common pitfall:** Using `memory_order_relaxed` for a "data ready" flag only guarantees atomicity of the flag — it does *not* guarantee that the payload written before the flag store is visible to the reader. Always use **release** (producer) / **acquire** (consumer) for producer-consumer signaling.

</details>

### 示例：带 compare-exchange 的 `memory_order_acq_rel`

读-改-写操作（如 CAS）既要向其他线程 **publish** 新值，又要 **observe** 先前的写入。`memory_order_acq_rel` 在同一次操作上同时完成这两件事：成功时，CAS 兼具 acquire（看到最新状态）与 release（使本次更新可见）的语义。当该 atomic 变量是某共享结构（如 lock-free 计数器或栈指针）唯一的同步点时，就使用它。

```cpp
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

std::atomic<int> counter{0};

void increment() {
    int expected = counter.load(std::memory_order_relaxed);
    while (!counter.compare_exchange_strong(
        expected, expected + 1,
        std::memory_order_acq_rel,   // success: publish our write + see others'
        std::memory_order_acquire))   // failure: only acquire (expected gets current)
    {
        // expected was updated; retry with new value
    }
}

int main() {
    const int num_threads = 4;
    const int increments_per_thread = 100000;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                increment();
            }
        });
    }
    for (auto& t : threads)
        t.join();

    std::cout << "counter = " << counter.load() << " (expected "
              << num_threads * increments_per_thread << ")\n";
    return 0;
}
```

**CAS 循环的工作方式：**

- `**compare_exchange_strong(expected, desired, success_order, failure_order)`** 执行一次原子步骤：
  - **如果** `counter` 的当前值等于 `expected`：将 `desired`（即 `expected + 1`）存入 `counter` 并返回 `true`。
  - **否则**：保持 `counter` 不变，**把 `counter` 的当前值写入 `expected`**，并返回 `false`。
- `**while (! ...)**` 的含义是：不断重试，直到交换成功。每次失败，都是在本次操作之前有其他线程改动了 `counter`，而 CPU 已经把那个新值放进 `expected`，因此下一次迭代使用的是最新的 `expected`（不需要额外的 load）。
- **成功序 `memory_order_acq_rel`：** 成功存入 `expected + 1` 时，既 **release**（使本次写入对其他线程可见），又 **acquire**（从而看到其他线程先前的所有写入）。这使计数器与任何与之关联的其他共享状态保持一致。
- **失败序 `memory_order_acquire`：** 比较失败时，不修改 `counter`，只是 **read** 它。有 acquire 就足够看到最新值（且 `expected` 已更新）。失败时不需要 release，因为没有写入。

没有这个循环，单次 CAS 可能失败（例如在 load 与 CAS 之间有另一个线程做了自增），这样就会漏掉一次自增。循环会不断重试，直到我们的自增成为“胜出”的那一次。


<details>
<summary>English original</summary>

**Example: `memory_order_acq_rel` with compare-exchange**

Read-modify-write operations (e.g. CAS) need to both **publish** the new value to other threads and **observe** prior writes. `memory_order_acq_rel` does both on the same operation: on success, the CAS acts as acquire (sees the latest state) and release (makes the update visible). Use it when the atomic is the single synchronization point for a shared structure (e.g. lock-free counter or stack pointer).

```cpp
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

std::atomic<int> counter{0};

void increment() {
    int expected = counter.load(std::memory_order_relaxed);
    while (!counter.compare_exchange_strong(
        expected, expected + 1,
        std::memory_order_acq_rel,   // success: publish our write + see others'
        std::memory_order_acquire))   // failure: only acquire (expected gets current)
    {
        // expected was updated; retry with new value
    }
}

int main() {
    const int num_threads = 4;
    const int increments_per_thread = 100000;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                increment();
            }
        });
    }
    for (auto& t : threads)
        t.join();

    std::cout << "counter = " << counter.load() << " (expected "
              << num_threads * increments_per_thread << ")\n";
    return 0;
}
```

**How the CAS loop works:**

- `**compare_exchange_strong(expected, desired, success_order, failure_order)`** does one atomic step:
  - **If** `counter`’s current value equals `expected`: store `desired` (i.e. `expected + 1`) into `counter` and return `true`.
  - **Else**: leave `counter` unchanged, **write the current value of `counter` into `expected`**, and return `false`.
- `**while (! ...)**` means: keep retrying until the exchange succeeds. On each failure, another thread changed `counter` before we did, and the CPU already put that new value into `expected`, so the next iteration uses an up-to-date `expected` (we don’t need an extra load).
- **Success order `memory_order_acq_rel`:** When we successfully store `expected + 1`, we both **release** (so our write is visible to others) and **acquire** (so we see all prior writes by other threads). That keeps the counter consistent with any other shared state tied to it.
- **Failure order `memory_order_acquire`:** When the compare fails, we don’t modify `counter`; we only **read** it. Acquire is enough so we see the latest value (and `expected` is updated). We don’t need release on failure because we didn’t write.

Without the loop, a single CAS could fail (e.g. another thread incremented between our load and our CAS), and we would have skipped an increment. The loop retries until our increment is the one that “wins.”

</details>

### 示例：`memory_order_seq_cst` — 单一全序

`memory_order_seq_cst` 为所有线程间的全部 seq_cst 操作给出一个**单一全序**。每个线程对 seq_cst 加载与存储的顺序看法一致。当你希望对某个小型协调点有简单、全局一致的视图时，这很有用。在未指定顺序时，它是 `std::atomic` 操作的默认选择。

```cpp
#include <atomic>
#include <iostream>
#include <thread>

std::atomic<bool> stop{false};
std::atomic<int> next_id{0};

void worker() {
    while (!stop.load(std::memory_order_seq_cst)) {
        // do work
    }
}

void request_stop() {
    stop.store(true, std::memory_order_seq_cst);
}

int allocate_id() {
    return next_id.fetch_add(1, std::memory_order_seq_cst);
}

int main() {
    std::thread t1(worker);
    std::thread t2(worker);

    std::cout << "Allocated id: " << allocate_id() << "\n";

    request_stop();

    t1.join();
    t2.join();
    return 0;
}
```

**这说明了什么：**

- `stop` 是一个简单的关闭标志，许多工作线程都可以检查它。
- `request_stop()` 把该标志设置一次，每个工作线程最终都会看到它并退出。
- `allocate_id()` 使用 `fetch_add(1)` 在各线程间分发严格递增的 ID。

**真实场景示例：** 设想一台相机或 web 服务器同时处理大量任务。每个到来的请求或帧都需要一个唯一 ID，以便之后能把日志、trace 和结果对应起来。`allocate_id()` 就像一台取号机：

- 线程 A 拿到 0 号票
- 线程 B 拿到 1 号票
- 线程 C 拿到 2 号票

即使多个线程同时调用它，也不会有两个线程拿到相同的号。这比用 `id = next_id; next_id = next_id + 1;` 安全得多，后者可能发生竞争并产生重复。

**真实场景的关闭示例：** 在区块链矿工程序中，当没有更多 PoW 工作要做时，停止标志会通知所有挖矿线程干净地停止：

```cpp
std::atomic<bool> stop{false};

void worker() {
    while (!stop.load(std::memory_order_seq_cst)) {
        // try a nonce, test PoW, submit a share if found
    }
}

void miner_manager() {
    // wait for new block template / work
    while (!stop.load(std::memory_order_seq_cst)) {
        // dispatch work to miners

        // if no new work is available and mining should pause/stop:
        // stop.store(true, std::memory_order_seq_cst);
    }
}

void request_stop() {
    stop.store(true, std::memory_order_seq_cst);
}
```

- `stop == false` 表示矿工应继续搜索 nonce。
- 当管理器决定停止、暂停或退出时，`request_stop()` 把该标志设置一次。
- 每个工作线程在自己的循环中检查该标志，完成当前这次尝试，然后干净退出。

这样可以避免把 CPU 浪费在无用的哈希上、避免矿池空了之后线程还在跑，也避免在共享状态更新中途强杀工作线程。所以是的，一个管理器线程可以设置一个共享原子标志，所有挖矿线程都会在下一次检查时停止。

为什么 `seq_cst` 适合这里：

- 容易推理。
- 每个线程看到的是原子操作的一个全局顺序。
- 当该标志是更大并发协议的一部分时，它能避免一些隐蔽的 bug。

在实际系统中，`seq_cst` 常被选用于：

- 关闭标志
- “就绪”信号
- 全局计数器
- debug 构建与正确性优先的代码
- 性能不是主要关注点的小型协调点

如果需要更高性能，并且能容忍更弱的保证，那么对关闭标志来说 acquire/release 通常就足够了。但对于简单的控制路径，`seq_cst` 是最安全、最清晰的选择。

---


<details>
<summary>English original</summary>

**Example: `memory_order_seq_cst` — single total order**

`memory_order_seq_cst` gives a **single total order** of all seq_cst operations across all threads. Every thread agrees on the order of seq_cst loads and stores. This is useful when you want a simple, globally consistent view of a small coordination point. It is the default for `std::atomic` operations when you don’t specify an order.

```cpp
#include <atomic>
#include <iostream>
#include <thread>

std::atomic<bool> stop{false};
std::atomic<int> next_id{0};

void worker() {
    while (!stop.load(std::memory_order_seq_cst)) {
        // do work
    }
}

void request_stop() {
    stop.store(true, std::memory_order_seq_cst);
}

int allocate_id() {
    return next_id.fetch_add(1, std::memory_order_seq_cst);
}

int main() {
    std::thread t1(worker);
    std::thread t2(worker);

    std::cout << "Allocated id: " << allocate_id() << "\n";

    request_stop();

    t1.join();
    t2.join();
    return 0;
}
```

**What this shows:**

- `stop` is a simple shutdown flag that many worker threads can check.
- `request_stop()` sets the flag once, and every worker eventually sees it and exits.
- `allocate_id()` uses `fetch_add(1)` to hand out a strictly increasing ID across threads.

**Real-world example:** imagine a camera or web server processing many tasks at once. Each incoming request or frame needs a unique ID so logs, traces, and results can be matched later. `allocate_id()` acts like a ticket dispenser:

- thread A gets ticket 0
- thread B gets ticket 1
- thread C gets ticket 2

Even if several threads call it at the same time, no two of them get the same number. That is much safer than doing `id = next_id; next_id = next_id + 1;`, which can race and produce duplicates.

**Real-world shutdown example:** in a blockchain miner, the stop flag tells all mining threads to stop cleanly when there is no more PoW work to do:

```cpp
std::atomic<bool> stop{false};

void worker() {
    while (!stop.load(std::memory_order_seq_cst)) {
        // try a nonce, test PoW, submit a share if found
    }
}

void miner_manager() {
    // wait for new block template / work
    while (!stop.load(std::memory_order_seq_cst)) {
        // dispatch work to miners

        // if no new work is available and mining should pause/stop:
        // stop.store(true, std::memory_order_seq_cst);
    }
}

void request_stop() {
    stop.store(true, std::memory_order_seq_cst);
}
```

- `stop == false` means miners should keep searching nonces.
- `request_stop()` sets the flag once when the manager decides to stop, pause, or exit.
- Each worker checks the flag in its loop, finishes the current attempt, and exits cleanly.

This avoids wasting CPU on useless hashes, leaving threads running after the pool is empty, or hard-killing workers in the middle of shared-state updates. So yes, one manager thread can set a shared atomic flag, and all mining threads will stop on the next check.

Why `seq_cst` fits here:

- It is easy to reason about.
- Every thread sees one global order of the atomic operations.
- It avoids subtle bugs when the flag is part of a larger concurrent protocol.

In real systems, `seq_cst` is often chosen for:

- shutdown flags
- “ready” signals
- global counters
- debug builds and correctness-first code
- small coordination points where performance is not the main concern

If you need more performance and can tolerate a weaker guarantee, acquire/release is often enough for a shutdown flag. But for simple control paths, `seq_cst` is the safest and clearest choice.

---

</details>

## 硬件内存模型与 kernel barrier

**这是什么意思？**  
不同的 CPU 可能以不同的顺序观察到内存操作，*除非*程序员使用显式控制来强制顺序。这称为**硬件内存模型**。在某些 CPU（如 x86-64）上，内存访问大多按程序中编写的顺序出现。在另一些 CPU（如 ARM64 和 RISC-V）上，为提升性能，它们可以被自由重排，除非你加入显式同步。

由于这些差异，在 kernel（或底层）代码中，你经常需要插入**内存屏障**：告诉 CPU“不要跨过此点重排内存访问”的特殊指令。这些屏障可以是显式的（特殊指令），也可以是隐式的（使用带特定内存序的 C11 原子操作）。


| Architecture | Memory model                     | What this means / Barrier requirements                                                                                                                                             |
| ------------ | -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| x86-64       | TSO (Total Store Order) — strong | 几乎总是保持存储/加载顺序；很少需要显式 barrier。原子操作使用 `LOCK` 前缀。                                                                      |
| ARM64        | 弱序                   | 加载/存储可以重排；你*必须*使用 barrier：`DMB`/`DSB`（内存屏障指令），以及 `LDAR`/`STLR`（加载/存储的 acquire/release 形式）以实现正确的同步。 |
| RISC-V       | 弱序（RVWMO）                     | 同样可以重排；使用 `FENCE` 指令同步，并使用 `LR`/`SC` 实现 LL/SC 风格的原子操作。                                                                                 |


**Linux kernel barrier 宏：**（可移植的 API，会展开为该架构对应的正确指令）

- `smp_mb()` — **内存屏障：**确保 barrier 之前的所有加载和存储都在其后的任何操作之前被全局观察到。
- `smp_rmb()` — **读（加载）内存屏障：**防止加载跨过 barrier 重排。
- `smp_wmb()` — **写（存储）内存屏障：**防止存储跨过 barrier 重排。

**实际示例/含义：**  
在 ARM64（例如 Jetson Orin）上，即便是那种在 x86 上能正常工作的“简单”多线程代码（因为 x86 具有强内存序）也可能出错，除非你使用原子操作/barrier——CPU 可能重排更新，读者可能看到陈旧或不一致的数据。这就是为什么 kernel（以及 C11/C++11）代码需要对共享变量使用显式原子操作或内存屏障：以*保证*所有线程按预期顺序看到更新，并防止只在弱序芯片上才暴露的 bug。

**示例：为什么需要内存屏障**

假设你有一个简单的生产者/消费者标志协议：

```c
// Producer
data = 123;
flag = 1;

// Consumer
if (flag == 1) {
    assert(data == 123);  // Could fail!
}
```

在 x86 上，这几乎总能按预期工作。但在 ARM64 或 RISC-V 上，CPU 可能将到 `flag` 的存储重排到 `data` 之前，因此消费者可能看到 `flag == 1`，但 `data` 仍然是旧值或垃圾值。

**用 barrier 修复：**

```c
// Producer (ARM64-style)
data = 123;
smp_wmb();      // Ensure data is visible before flag
flag = 1;

// Consumer
if (flag == 1) {
    smp_rmb();  // Ensure load of data happens after seeing the flag
    assert(data == 123);  // Now guaranteed
}
```

通过在设置标志之前（生产者）添加 `smp_wmb()`，以及在检查标志之后（消费者）添加 `smp_rmb()`，可以防止重排，并确保当 `flag` 表示已就绪时，`data` 具有正确的可见性。

---

## 原子操作（Kernel）

映射到单条不可分割的硬件指令：x86 上的 `LOCK ADD`/`XADD`/`CMPXCHG`，ARMv8.1 LSE 上的 `LDADD`/`CAS`。

```c
atomic_t counter = ATOMIC_INIT(0);
atomic_inc(&counter);                   // LOCK XADD / LDADD
atomic_dec_and_test(&counter);          // decrement; return true if zero (refcounts)
int old = atomic_cmpxchg(&counter, 5, 10);  // if counter==5 set to 10; return old value
atomic_add_return(n, &counter);         // add n, return new value (rate limiting)
```

- `atomic_t` 是 32 位；`atomic64_t` 用于 64 位。用于：引用计数、标志、per-CPU 统计。


<details>
<summary>English original</summary>

**Hardware Memory Models & Kernel Barriers**

**What does this mean?**  
Different CPUs can observe memory operations in different orders *unless* the programmer uses explicit controls to enforce order. This is called the **hardware memory model**. On some CPUs (like x86-64), memory accesses appear mostly in the order written in the program. On others (like ARM64 and RISC-V), they can be freely reordered for performance unless you add explicit synchronization.

Because of these differences, in kernel (or low-level) code you often need to insert **memory barriers**: special instructions that tell the CPU, "do not reorder memory accesses across this point." These can be explicit (special instructions) or implicit (using C11 atomics with specific orderings).


| Architecture | Memory model                     | What this means / Barrier requirements                                                                                                                                             |
| ------------ | -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| x86-64       | TSO (Total Store Order) — strong | Almost always keeps stores/loads in order; few explicit barriers needed. Atomic operations use `LOCK` prefix.                                                                      |
| ARM64        | Weakly ordered                   | Loads/stores can reorder; you *must* use barriers: `DMB`/`DSB` (memory barrier instructions), and `LDAR`/`STLR` (acquire/release forms of load/store) for correct synchronization. |
| RISC-V       | Weak (RVWMO)                     | Also can reorder; synchronize with `FENCE` instructions and use `LR`/`SC` for LL/SC-style atomics.                                                                                 |


**Linux kernel barrier macros:** (portable APIs that expand to the right instructions for the architecture)

- `smp_mb()` — **Memory Barrier:** Ensures both loads and stores before the barrier are globally observed before any after it.
- `smp_rmb()` — **Read (load) Memory Barrier:** Prevent reordering of loads across the barrier.
- `smp_wmb()` — **Write (store) Memory Barrier:** Prevent reordering of stores across the barrier.

**Practical example/meaning:**  
On ARM64 (e.g. Jetson Orin), even "simple" multithreaded code that works on x86 (because of its strong memory ordering) can break unless you use atomics/barriers — the CPU might reorder updates and readers could see stale or inconsistent data. That's why kernel (and C11/C++11) code needs explicit atomic operations or memory barriers for shared variables: to *guarantee* all threads see updates in the intended order and prevent bugs that only show up on weakly ordered chips.

**Example: Why memory barriers are needed**

Suppose you have a simple producer/consumer flag protocol:

```c
// Producer
data = 123;
flag = 1;

// Consumer
if (flag == 1) {
    assert(data == 123);  // Could fail!
}
```

On x86, this almost always works as expected. But on ARM64 or RISC-V, the CPU may reorder the store to `flag` before `data`, so the consumer could see `flag == 1` but `data` still having an old or garbage value.

**Fix with barriers:**

```c
// Producer (ARM64-style)
data = 123;
smp_wmb();      // Ensure data is visible before flag
flag = 1;

// Consumer
if (flag == 1) {
    smp_rmb();  // Ensure load of data happens after seeing the flag
    assert(data == 123);  // Now guaranteed
}
```

By adding `smp_wmb()` before setting the flag (producer) and `smp_rmb()` after checking the flag (consumer), you prevent reordering and ensure correct visibility of `data` when `flag` indicates it is ready.

---

**Atomic Operations (Kernel)**

Map to single indivisible hardware instructions: `LOCK ADD`/`XADD`/`CMPXCHG` on x86, `LDADD`/`CAS` on ARMv8.1 LSE.

```c
atomic_t counter = ATOMIC_INIT(0);
atomic_inc(&counter);                   // LOCK XADD / LDADD
atomic_dec_and_test(&counter);          // decrement; return true if zero (refcounts)
int old = atomic_cmpxchg(&counter, 5, 10);  // if counter==5 set to 10; return old value
atomic_add_return(n, &counter);         // add n, return new value (rate limiting)
```

- `atomic_t` is 32-bit; `atomic64_t` for 64-bit. Use for: reference counts, flags, per-CPU statistics.

</details>

## Compare-and-Swap (CAS)：含义

**含义：**  
Compare-and-Swap (CAS) 是一条硬件支持的原子指令，它检查某个内存位置是否包含期望值，如果包含，就将其更新为新值——**全部在一个不可分割的步骤中完成**。这是实现允许多个线程**不使用锁**进行协调的数据结构和算法的基本构建块。借助 CAS，线程即使同时运行也能安全地更新共享数据，因为该操作保证不会有两条线程在同一次更新中都成功。

**实际用途：**  
使用 CAS 可实现无锁队列、栈、引用计数器以及其他可能有多条线程同时尝试更新同一值的算法。如果两条线程竞争更新同一个值，只有一条会成功。另一条会看到自己竞争失败，并可以重试。

**代码示例：**

```cpp
std::atomic<int> val{0};
int expected = 0;
bool ok = val.compare_exchange_strong(expected, 1,
    std::memory_order_acq_rel,   // On success: this thread sees all prior writes/updates, and its own write is visible to others.
    std::memory_order_acquire);  // On failure: just read latest, no write.
```

- 如果 `val` 当前等于 `expected` (0)，则将其设为 1，并且 `ok` 为 true。
- 如果不是，则 `val` 保持不变，`expected` 会被更新为该值原来的内容，`ok` 为 false，通常需要重试。
- **在循环内部**，通常使用 `compare_exchange_weak`（在某些硬件上可能产生假阴性，但可能更快）。

**陷阱——ABA 问题（含义）：**  
假设某个指针的值是 A，之后被改为 B，然后*又*变回 A（但现在 A 指向不同的对象）。CAS 只检查是否为“A”，如果看到 A，就假定没有任何变化——但实际上，A 处的对象可能已被删除并重用，从而导致 use-after-free 缺陷。

**ABA 解决方案的含义：**  

1. **版本标签：** 将一个计数器附加到指针上，这样任何变更都会递增计数（为此需要双倍宽度的原子比较，例如 128-bit CAS）。
2. **危险指针：** 读者声明“我正在使用此指针”；写者仅在没有读者使用它之后才释放内存。
3. **RCU：** 更新确保旧值在所有先前的读者完成前不会被释放，从而防止该缺陷。

**示例——版本标签如何修复 ABA：**  
将指针和版本号一起存储。如果指针从 `A` 变为其他值，之后又回到 `A`，版本号仍会不同，因此 CAS 会失败，而不会被相同的地址欺骗。

```cpp
struct TaggedPtr {
    Node* ptr;
    uint64_t version;
};

std::atomic<TaggedPtr> head;

void update_head(Node* old_ptr, Node* new_ptr) {
    TaggedPtr expected = {old_ptr, old_ptr ? old_ptr->version : 0};
    TaggedPtr desired  = {new_ptr, expected.version + 1};

    // Fails if either the pointer OR the version changed.
    head.compare_exchange_strong(expected, desired,
                                 std::memory_order_acq_rel,
                                 std::memory_order_acquire);
}
```

如果另一条线程将 `head` 从 `A` 改为 `B`，然后又改回 `A`，指针值可能看起来相同，但版本号会更高。这表明该对象在此期间被修改过，因此过期的 CAS 不会成功。

总之：  
CAS 是“相等则原子，否则更新并重试”，通过提供一种让线程*无需*锁即可在共享内存变更上达成一致的方式——仅使用内置的检查并交换步骤，该步骤绝不会半途更新值——从而构成安全、高效的无锁编程的基础。

---

## SPSC 环形缓冲区（无锁）：结合 MengRao 的 SPSC_Queue 深入探讨

无锁 **SPSC**（“单生产者、单消费者”）队列是一种用于在**恰好一个生产者线程和一个消费者线程**之间通信的高效队列。下面剖析其工作原理，*受广泛尊重的 [MengRao/SPSC_Queue](https://github.com/MengRao/SPSC_Queue) 实现启发并参考了它*，它是高性能 SPSC 队列的黄金标准。

### 关键属性

- **无锁：** 任何线程都不会阻塞或等待；所有队列操作都是非阻塞的，并且独立推进。
- **无伪共享：** 生产者和消费者只接触各自的同步变量，减少缓存行弹跳。
- **缓存效率：** 有效载荷缓冲区绝不会被两侧同时“锁定”，因此可以实现高吞吐（对于摄像头帧或音频等高速流水线至关重要）。

---


<details>
<summary>English original</summary>

**Compare-and-Swap (CAS): Meaning**

**Meaning:**  
Compare-and-Swap (CAS) is a hardware-supported atomic instruction that checks if a memory location contains an expected value, and if so, updates it to a new value—**all in one indivisible step**. This is the basic building block for implementing data structures and algorithms that allow multiple threads to coordinate **without using locks**. With CAS, threads can safely update shared data despite running at the same time, because the operation guarantees that no two threads can both succeed at the same update.

**Practical use:**  
You use CAS to implement things like lock-free queues, stacks, reference counters, and other algorithms where multiple threads might try to update the same value at once. If two threads race to update a value, only one will succeed. The other will see that it lost the race and can try again.

**Code example:**

```cpp
std::atomic<int> val{0};
int expected = 0;
bool ok = val.compare_exchange_strong(expected, 1,
    std::memory_order_acq_rel,   // On success: this thread sees all prior writes/updates, and its own write is visible to others.
    std::memory_order_acquire);  // On failure: just read latest, no write.
```

- If `val` currently equals `expected` (0), set it to 1, and `ok` is true.
- If not, `val` is unchanged, `expected` is updated to whatever the value was, `ok` is false, and you typically retry.
- **Inside a loop**, you usually use `compare_exchange_weak` (can give false negatives on some hardware but may be faster).

**Pitfall — the ABA problem (meaning):**  
Suppose a pointer's value is A, then it gets changed to B, then *back* to A (but now A points to a different object). CAS only checks for "A", and if it sees A, it assumes nothing changed—but in reality, the object at A may have been deleted and reused, causing a use-after-free bug.

**Meaning of ABA Solutions:**  

1. **Version tags:** Attach a counter to the pointer, so any change increments the count (need double-wide atomic compare for this, e.g. 128-bit CAS).
2. **Hazard pointers:** Readers advertise "I'm using this pointer"; writers only free memory after no reader is using it.
3. **RCU:** Updates ensure old values are not freed until all prior readers have finished, preventing the bug.

**Example — how version tags fix ABA:**  
Store both the pointer and a version number together. If the pointer changes away from `A` and later comes back to `A`, the version will still be different, so CAS will fail instead of being fooled by the same address.

```cpp
struct TaggedPtr {
    Node* ptr;
    uint64_t version;
};

std::atomic<TaggedPtr> head;

void update_head(Node* old_ptr, Node* new_ptr) {
    TaggedPtr expected = {old_ptr, old_ptr ? old_ptr->version : 0};
    TaggedPtr desired  = {new_ptr, expected.version + 1};

    // Fails if either the pointer OR the version changed.
    head.compare_exchange_strong(expected, desired,
                                 std::memory_order_acq_rel,
                                 std::memory_order_acquire);
}
```

If another thread changes `head` from `A` to `B` and then back to `A`, the pointer value may look the same, but the version will be higher. That tells us the object was modified in between, so the stale CAS does not succeed.

In summary:  
CAS is "atomic if equal, else update and retry", forming the foundation of safe, efficient lock-free programming by providing a way for threads to agree on shared memory changes *without* locks—just using a built-in check-and-swap step that never halfway updates a value.

---

**SPSC Ring Buffer (Lock-Free): In-Depth with MengRao's SPSC_Queue**

The lock-free **SPSC** ("Single Producer, Single Consumer") queue is an efficient queue for communication between **exactly one producer thread and one consumer thread**. Let's break down how it works, *inspired by and referencing the widely respected [MengRao/SPSC_Queue](https://github.com/MengRao/SPSC_Queue) implementation*, which is a gold standard for high-performance SPSC queues.

**Key Properties**

- **Lock-Free:** No thread ever blocks or waits; all queue operations are non-blocking and progress independently.
- **No False Sharing:** Producer and consumer only touch separate synchronization variables, reducing cache line bouncing.
- **Cache Efficiency:** Payload buffer is never "locked" by both sides at once, so high throughput is possible (critical for high-rate pipelines like camera frames or audio).

---

</details>

### 基本结构

MengRao 的 queue，以及大多数快速 SPSC queue，都采用如下设计：

- **Buffer：** 固定大小的环形缓冲区（数组），所有 slot 预先分配，按缓冲区大小取模访问（大小为 2 的幂）。
- **分离的索引：** 两个索引，一个属于 producer（`tail` 或 `write_idx`），一个属于 consumer（`head` 或 `read_idx`），各自只由一个线程持有并更新。
- **Fence：** 使用恰当的内存序（`release`、`acquire`）确保 producer 写入的数据**只有在** consumer 观察到索引更新之后才对 consumer 可见，反之亦然。

### 参考结构 — MengRao SPSC_Queue

下面是一份带注释的版本，融合原始风格与 C/C++ 惯例（完整细节见 [MengRao/SPSC_Queue/queue_spsc.h](https://github.com/MengRao/SPSC_Queue/blob/master/queue_spsc.h)):

```cpp
template<typename T, size_t N = 256>
class SPSCQueue {
    alignas(64) T buf[N]; // Circular buffer for data; cache-line aligned to avoid false sharing
    alignas(64) std::atomic<size_t> head = 0; // Read index — owned only by consumer
    alignas(64) std::atomic<size_t> tail = 0; // Write index — owned only by producer

public:
    // Called only from the producer thread
    bool enqueue(const T &item) {
        size_t tail_cache = tail.load(std::memory_order_relaxed);
        size_t head_cache = head.load(std::memory_order_acquire); // see what consumer has consumed
        if (((tail_cache + 1) & (N - 1)) == (head_cache & (N - 1))) {
            // Buffer is full (one slot left empty to avoid overwrite ambiguity)
            return false;
        }
        buf[tail_cache & (N - 1)] = item;
        tail.store(tail_cache + 1, std::memory_order_release); // publish
        return true;
    }

    // Called only from the consumer thread
    bool dequeue(T &item) {
        size_t head_cache = head.load(std::memory_order_relaxed);
        size_t tail_cache = tail.load(std::memory_order_acquire); // see what producer has added
        if ((head_cache & (N - 1)) == (tail_cache & (N - 1))) {
            // Buffer is empty
            return false;
        }
        item = buf[head_cache & (N - 1)];
        head.store(head_cache + 1, std::memory_order_release);
        return true;
    }
};
```

#### 要点：

- **单一所有权原则：** 只有 consumer 线程修改 `head`，只有 producer 修改 `tail`。每个线程只读（不写）对方的索引。
- **回绕：** 缓冲区大小是 2 的幂 —— 回绕用位掩码（`& (N - 1)`）完成，以提升速度。
- **满 vs. 空：** 刻意空置一个 slot，因此 `tail + 1 == head` 表示满，`tail == head` 表示空（这样可避免歧义）。
- **内存屏障：**
  - `store(..., memory_order_release)` 保证在发布索引之前，此前对缓冲区的所有 store 均已可见。
  - `load(..., memory_order_acquire)` 保证随后的缓冲区读取能看到最新数据。

### 为什么这比锁或朴素的原子操作更好？

- **数据零竞争：** 只有相关线程会触碰自己的索引；缓冲区 slot 总是先写入，*然后*才更新索引。
- **无锁、无阻塞：** 没有 mutex、semaphore 或重量级同步。
- **面向高吞吐的可扩展性：** 在现代 CPU 上，配合精心对齐和显式原子操作，该设计可以以最小的缓存流量实现每秒数千万到数亿条消息。

### 实际用法（如 OpenPilot VisionIPC、ML 数据流等）

- 用于相机帧流、音频流水线或神经模型流水线等高吞吐场景，在这些场景中经典锁或阻塞同步会成为性能瓶颈。
- **约束：** **必须**恰好一个 producer 和一个 consumer。若需要多个，请参见 MPMC（multi-producer multi-consumer）queue，它们复杂得多。

---

### 汇总表


| Feature           | SPSC MengRao Queue     |
| ----------------- | ---------------------- |
| 支持的线程 | 1 个 producer，1 个 consumer |
| 需要锁？    | 否                     |
| 阻塞/等待？ | 从不                  |
| 吞吐        | 极高         |
| 注意事项            | 恰好 1 prod/1 cons  |
| 使用场景           | 网络、视觉、ML |


---

**更多内容：**  

- [MengRao/SPSC_Queue - queue_spsc.h](https://github.com/MengRao/SPSC_Queue/blob/master/queue_spsc.h) —— 另见 README 了解 benchmark 与用法。

---

## RCU（Read-Copy-Update）

Linux 内核**以读为主的共享数据的主要机制**。**读侧开销 O(1)：** 无锁、无原子操作、无缓存行写入 —— 只有 preempt disable/enable。

### 读侧

```c
rcu_read_lock();
ptr = rcu_dereference(gp);   // READ_ONCE + compiler barrier
/* use *ptr — guaranteed valid until rcu_read_unlock */
rcu_read_unlock();
```


<details>
<summary>English original</summary>

**Basic Structure**

MengRao's queue, and most fast SPSC queues, use the following design:

- **Buffer:** A fixed-size circular buffer (array), where all slots are preallocated and accessed modulo the buffer size (which is a power of 2).
- **Separate Indices:** Two indices, one for the producer (`tail` or `write_idx`) and one for the consumer (`head` or `read_idx`), each owned and only updated by one thread.
- **Fences:** Use of proper memory orderings (`release`, `acquire`) ensures that data written by the producer is visible to the consumer **only after** the consumer observes the index update, and vice versa.

**Reference Structure — MengRao SPSC_Queue**

Here is an annotated version, blending the original style with C/C++ conventions (for full detail see [MengRao/SPSC_Queue/queue_spsc.h](https://github.com/MengRao/SPSC_Queue/blob/master/queue_spsc.h)):

```cpp
template<typename T, size_t N = 256>
class SPSCQueue {
    alignas(64) T buf[N]; // Circular buffer for data; cache-line aligned to avoid false sharing
    alignas(64) std::atomic<size_t> head = 0; // Read index — owned only by consumer
    alignas(64) std::atomic<size_t> tail = 0; // Write index — owned only by producer

public:
    // Called only from the producer thread
    bool enqueue(const T &item) {
        size_t tail_cache = tail.load(std::memory_order_relaxed);
        size_t head_cache = head.load(std::memory_order_acquire); // see what consumer has consumed
        if (((tail_cache + 1) & (N - 1)) == (head_cache & (N - 1))) {
            // Buffer is full (one slot left empty to avoid overwrite ambiguity)
            return false;
        }
        buf[tail_cache & (N - 1)] = item;
        tail.store(tail_cache + 1, std::memory_order_release); // publish
        return true;
    }

    // Called only from the consumer thread
    bool dequeue(T &item) {
        size_t head_cache = head.load(std::memory_order_relaxed);
        size_t tail_cache = tail.load(std::memory_order_acquire); // see what producer has added
        if ((head_cache & (N - 1)) == (tail_cache & (N - 1))) {
            // Buffer is empty
            return false;
        }
        item = buf[head_cache & (N - 1)];
        head.store(head_cache + 1, std::memory_order_release);
        return true;
    }
};
```

**Key Points:**

- **Single Ownership Principle:** Only the consumer thread changes `head`, and only the producer changes `tail`. Each thread reads (but does not write) the other's index.
- **Wrap-around:** Buffer size is a power of two — wrapping is done with a bitmask (`& (N - 1)`) for speed.
- **Full vs. Empty:** We keep one slot empty intentionally, so `tail + 1 == head` means full, `tail == head` means empty (this avoids ambiguity).
- **Memory Barriers:**
  - `store(..., memory_order_release)` ensures that all prior stores to the buffer are visible before publishing the index.
  - `load(..., memory_order_acquire)` ensures that the subsequent buffer reads see the latest data.

**Why Is This Better Than Locks or Naive Atomics?**

- **Zero contention on data:** Only the relevant thread ever touches its own index; the buffer slot is always written *then* the index updated.
- **No locks, no blocking:** No mutex, semaphore, or heavy synchronization.
- **Scalable for high-throughput:** On modern CPUs, with careful alignment and use of explicit atomics, this design can achieve tens or hundreds of millions of messages per second with minimal cache traffic.

**Practical Usage (like OpenPilot VisionIPC, ML data streaming, etc.)**

- Used in high-throughput situations like camera frame streaming, audio pipelines, or neural model pipes, where classic locks or blocking sync would be a performance bottleneck.
- **Constraint:** You **must** have exactly one producer and one consumer. If you need multiple, see MPMC (multi-producer multi-consumer) queues, which are much more complex.

---

**Summary Table**


| Feature           | SPSC MengRao Queue     |
| ----------------- | ---------------------- |
| Threads supported | 1 producer, 1 consumer |
| Lock required?    | No                     |
| Blocking/Waiting? | Never                  |
| Throughput        | Extremely high         |
| Caveat            | Exactly 1 prod/1 cons  |
| Used in           | Networking, vision, ML |


---

**See more:**  

- [MengRao/SPSC_Queue - queue_spsc.h](https://github.com/MengRao/SPSC_Queue/blob/master/queue_spsc.h) — See also the README for benchmarking and usage.

---

**RCU (Read-Copy-Update)**

Linux kernel’s **primary mechanism for read-mostly shared data**. **O(1) read-side cost:** no locks, no atomics, no cache-line writes — only preempt disable/enable.

**Reader side**

```c
rcu_read_lock();
ptr = rcu_dereference(gp);   // READ_ONCE + compiler barrier
/* use *ptr — guaranteed valid until rcu_read_unlock */
rcu_read_unlock();
```

</details>

### 写者侧（复制 → 修改 → 发布 → 等待 → 释放）

1. 分配并填充对象的新版本。
2. 发布：`rcu_assign_pointer(gp, new_obj)`（smp_wmb + 指针存储）。
3. 等待一个**宽限期**：所有已存在的读者都已完成。
4. 释放旧对象（已无读者仍持有它）。

```c
new_obj = kmalloc(sizeof(*new_obj), GFP_KERNEL);
*new_obj = *old_obj;
new_obj->field = updated_value;
rcu_assign_pointer(gp, new_obj);
synchronize_rcu();   // blocks until grace period
kfree(old_obj);
```

**call_rcu(&old->rcu_head, free_fn)：**异步形式；写者不阻塞；用于中断上下文，或写者不能阻塞的场景。

### 宽限期

**宽限期**是指从 `rcu_assign_pointer()` 之后开始，直到每个 CPU 都经过一个**静默状态**（上下文切换、进入 idle 或返回用户空间）的这段时间间隔。内核跟踪这些事件；当所有 CPU 都静默后，就不存在仍持有旧指针的既有读者。

### RCU 变体


| 变体              | 读侧临界区可睡眠？ | 使用场景                                              |
| -------------------- | ----------------------- | ----------------------------------------------------- |
| Classic RCU          | 否                      | 路由表、task_struct 查找、模块参数 |
| SRCU（Sleepable RCU） | 是                     | 通知链、子系统注册              |
| PREEMPT_RCU          | 否（但可抢占）    | CONFIG_PREEMPT 内核                                |


**rcu_nocbs=`<cpulist>`：**把 `call_rcu` 回调卸载到非隔离 CPU 上的 `rcuoc` kthread。在隔离的推理核上，正常 RCU 会在不可预测的时刻于该核上运行回调；`rcu_nocbs=` 消除了这一延迟来源。

---

## kfifo（内核 SPSC FIFO）

```c
DECLARE_KFIFO(my_fifo, int, 64);   // size power of 2
INIT_KFIFO(my_fifo);
kfifo_put(&my_fifo, value);        // producer
kfifo_get(&my_fifo, &value);       // consumer
kfifo_len(&my_fifo);
```

在内核驱动中用于 ISR（生产者）与进程上下文（消费者）之间的 RX 缓冲区。内核标准的无锁 SPSC FIFO。

## Hazard Pointers

RCU 的无锁回收在用户空间的替代方案：

- 读者把即将解引用的指针**发布**到每线程的 hazard pointer 槽位。
- 释放之前，回收者扫描所有 hazard pointer 槽位查找该指针。
- 若找到 → 推迟释放；若未找到 → 可安全释放。

用于 Folly（Meta）、Java `java.util.concurrent`；C++26 中的 `std::hazard_pointer`。

---

## 第 1 部分小结表


| 技术       | 读者开销        | 写者开销         | ISR 中安全？                | 局限                        |
| --------------- | ------------------ | ------------------- | --------------------------- | --------------------------------- |
| Spinlock        | 缓存写 + 自旋 | 缓存写         | 是                         | 浪费 CPU；不可睡眠              |
| CAS loop        | 原子 RMW + 重试 | 原子 RMW + 重试  | 是                         | ABA；重试开销                   |
| SPSC ring       | acquire load       | release store       | 是（生产者或消费者）  | 仅限单生产者且单消费者 |
| RCU             | 禁用抢占    | 复制 + 宽限期 | 否（synchronize_rcu 会阻塞） | 读多写少；写者付出代价          |
| Hazard pointers | 发布 + 加载     | 扫描 HP 槽位       | 否                          | 回收开销              |
| kfifo           | acquire load       | release store       | 是                         | 仅内核；仅 SPSC            |


### 第 1 部分 —— 概念回顾

- **为什么 relaxed 不适合生产者-消费者信号？** relaxed 只保证原子变量本身的原子性；它并不保证在 store 之前写入的数据对读者可见。用 release/acquire 来标记“数据就绪”。
- **为什么 RCU 无需读者侧锁也能工作？** 内核会检测**静默状态**（上下文切换、idle、返回用户）。只有当每个 CPU 都已静默，宽限期才结束，因此不存在仍持有旧指针的既有读者。
- **CAS 何时失败，重试循环该怎么做？** 当另一个线程改变了该值时 CAS 失败。失败时，`expected` 变量会被更新为当前值；循环应重新读取相关状态、重新计算新值并重试——而不是带着陈旧的中间结果重试。
- **模型配置该用 RCU 还是 rwlock？** 用 rwlock 时，写者会阻塞新读者。用 RCU 时，读者永远不会被阻塞；写者在副本上操作并原子发布。对于每周期都读取配置的 100 Hz 推理循环，RCU 是正确选择。

---

# 第 2 部分：死锁、优先级反转与 PI 互斥锁

**背景：**死锁 = 任务互相永久等待。优先级反转 = 高优先级任务被低优先级任务（通过共享资源）阻塞。两者都需要设计期与运行期的措施（顺序、PI、lockdep）。

---


<details>
<summary>English original</summary>

**Writer side (copy → modify → publish → wait → free)**

1. Allocate and populate a new version of the object.
2. Publish: `rcu_assign_pointer(gp, new_obj)` (smp_wmb + pointer store).
3. Wait for a **grace period**: all pre-existing readers have finished.
4. Free the old object (no reader can still hold it).

```c
new_obj = kmalloc(sizeof(*new_obj), GFP_KERNEL);
*new_obj = *old_obj;
new_obj->field = updated_value;
rcu_assign_pointer(gp, new_obj);
synchronize_rcu();   // blocks until grace period
kfree(old_obj);
```

**call_rcu(&old->rcu_head, free_fn):** asynchronous form; writer does not block; used in interrupt context or when writer must not block.

**Grace period**

A **grace period** is the interval after `rcu_assign_pointer()` until every CPU has passed a **quiescent state** (context switch, entry to idle, or return to user space). The kernel tracks these events; when all CPUs have quiesced, no pre-existing reader can still hold the old pointer.

**RCU variants**


| Variant              | Read section can sleep? | Use case                                              |
| -------------------- | ----------------------- | ----------------------------------------------------- |
| Classic RCU          | No                      | Routing tables, task_struct lookup, module parameters |
| SRCU (Sleepable RCU) | Yes                     | Notifier chains, subsystem registrations              |
| PREEMPT_RCU          | No (but preemptible)    | CONFIG_PREEMPT kernels                                |


**rcu_nocbs=`<cpulist>`:** Offloads `call_rcu` callbacks to `rcuoc` kthreads on non-isolated CPUs. On an isolated inference core, normal RCU would run callbacks there at unpredictable times; `rcu_nocbs=` removes that latency source.

---

**kfifo (Kernel SPSC FIFO)**

```c
DECLARE_KFIFO(my_fifo, int, 64);   // size power of 2
INIT_KFIFO(my_fifo);
kfifo_put(&my_fifo, value);        // producer
kfifo_get(&my_fifo, &value);       // consumer
kfifo_len(&my_fifo);
```

Used in kernel drivers for RX buffers between ISR (producer) and process context (consumer). Kernel’s standard lock-free SPSC FIFO.

**Hazard Pointers**

Userspace alternative to RCU for lock-free reclamation:

- Reader **publishes** the pointer it is about to dereference into a per-thread hazard pointer slot.
- Before freeing, the reclaimer scans all hazard pointer slots for that pointer.
- If found → defer free; if not found → free is safe.

Used in Folly (Meta), Java `java.util.concurrent`; `std::hazard_pointer` in C++26.

---

**Part 1 Summary Table**


| Technique       | Reader cost        | Writer cost         | Safe in ISR?                | Limitation                        |
| --------------- | ------------------ | ------------------- | --------------------------- | --------------------------------- |
| Spinlock        | Cache write + spin | Cache write         | Yes                         | Wasted CPU; no sleep              |
| CAS loop        | Atomic RMW + retry | Atomic RMW + retry  | Yes                         | ABA; retry cost                   |
| SPSC ring       | Acquire load       | Release store       | Yes (producer or consumer)  | Single producer AND consumer only |
| RCU             | Preempt disable    | Copy + grace period | No (synchronize_rcu blocks) | Read-mostly; writer pays          |
| Hazard pointers | Publish + load     | Scan HP slots       | No                          | Reclamation overhead              |
| kfifo           | Acquire load       | Release store       | Yes                         | Kernel-only; SPSC only            |


**Part 1 — Conceptual review**

- **Why is relaxed wrong for producer-consumer signaling?** Relaxed guarantees only atomicity of the atomic variable; it does not guarantee that data written before the store is visible to the reader. Use release/acquire for “data ready” flags.
- **Why does RCU work without reader-side locks?** The kernel detects **quiescent states** (context switch, idle, return to user). A grace period ends only after every CPU has quiesced, so no pre-existing reader can still hold the old pointer.
- **When does CAS fail and what should the retry loop do?** CAS fails when another thread changed the value. On failure, the `expected` variable is updated with the current value; the loop should re-read dependent state, recompute the new value, and retry — not retry with stale computation.
- **RCU vs rwlock for model config?** With rwlock, writers block new readers. With RCU, readers are never blocked; the writer works on a copy and publishes atomically. For a 100 Hz inference loop reading config every cycle, RCU is the right choice.

---

**Part 2: Deadlock, Priority Inversion & PI Mutexes**

**Context:** Deadlock = tasks waiting for each other forever. Priority inversion = high-priority task blocked by a low-priority one (via a shared resource). Both require design-time and runtime measures (ordering, PI, lockdep).

---

</details>

## 死锁：定义与 Coffman 条件

**死锁：** 一组进程各自等待该组中另一个进程持有的资源；**任何进程都无法取得进展**。

```
  Process A holds L1, waiting for L2  ──►  Process B holds L2, waiting for L1
  Neither can proceed. Both wait forever.
```

**Coffman 条件**（四个条件必须同时成立才可能死锁；破坏其中任意一个即可防止）：


| 条件 | 定义 |
| -------------------- | ------------------------------------------------------------------------------ |
| **互斥** | 至少有一个资源不可共享——同一时刻只能由一个进程持有 |
| **持有并等待** | 进程在等待获取另一资源时，至少持有一个资源 |
| **不可抢占** | 资源不能被强制夺走；只能主动释放 |
| **循环等待** | 资源分配图中存在环：P1→R1→P2→R2→P1 |


---

## 死锁预防

在设计阶段攻击四个条件之一：


| 要破坏的条件 | 技术 | 代价 |
| ------------------ | --------------------------------------------------------------------- | ---------------------------------------------- |
| 持有并等待 | 一次性获取全部锁；任一获取失败则全部释放 | 并发度更低；需要重试逻辑 |
| 循环等待 | **全局锁顺序：** 始终按固定且有文档记录的顺序获取 | 所有调用点都需遵守纪律；由 lockdep 强制执行 |
| 不可抢占 | `mutex_trylock()`，配合随机化指数退避 | 重试开销；存在活锁风险 |
| 互斥 | 无锁数据结构 | 实现复杂度更高 |


**全局锁顺序（分步）：**

1. 枚举子系统中所有 mutex/锁。
2. 为每个锁分配一个数字等级（例如 Lock A = 1，Lock B = 2）。
3. 强制规定：始终按等级升序获取。
4. 在锁的声明处记录该顺序。
5. 使用 `lockdep_set_class`，使 lockdep 能自动校验顺序。
6. 在代码评审中，拒绝任何在持有高等级锁时获取低等级锁的补丁。

> **陷阱：** 新增锁时若不更新全局顺序，顺序就会被破坏——两个开发者可能分别引入 A→C→D 和 A→D 的环。在 CI 中使用 lockdep 捕获环。

## 死锁检测与恢复

- **资源分配图：** 节点 = 进程（P）与资源（R）；边 P→R 表示 P 等待 R；边 R→P 表示 R 被 P 持有。存在环 = 死锁。
- **等待图：** 简化形式；边 P→Q 表示 P 等待 Q 持有的资源；存在环 = 死锁。

**检测后的恢复方案：** (1) 中止一个进程并释放其资源（按优先级、runtime、持有资源选择牺牲者）。(2) 从某个进程抢占一个资源（需要回滚支持）。(3) 回滚到安全检查点（需要检查点机制）。Linux 中的 **lockdep** 在首次观察到新的加锁顺序时，于 runtime 检测*潜在*死锁——早于实际挂起发生。

---

## 优先级反转

**机制（四步）：**

1. **L**（低优先级）获取 mutex M。
2. **H**（高优先级）被唤醒并尝试获取 M——阻塞等待 L。
3. **M**（中优先级）被唤醒并**抢占 L**（M > L）。
4. L 始终得不到运行 → 无法释放 M → H 虽为最高优先级却无限期等待。

H 实际上以 L 的优先级运行——**发生了反转**。没有 PI 时，反转的持续时间**无上界**。

```
  Time ─────────────────────────────────────────────────────────►
  H (high):  [woken]──[BLOCKED on mutex held by L]────────────────►
  M (med):   ─────────────────────[RUNNING]────────────────────────►
  L (low):   [holds mutex]──[PREEMPTED by M]──────[never runs]
```

> **要点：** 即使每个任务本身都正确，反转也可能发生。它是交互产生的涌现效应；仅靠代码评审无法发现。


<details>
<summary>English original</summary>

**Deadlock: Definition & Coffman Conditions**

**Deadlock:** A set of processes are each waiting for a resource held by another in the set; **no process can ever make progress**.

```
  Process A holds L1, waiting for L2  ──►  Process B holds L2, waiting for L1
  Neither can proceed. Both wait forever.
```

**Coffman conditions** (all four must hold for deadlock to be possible; break any one to prevent it):


| Condition            | Definition                                                                     |
| -------------------- | ------------------------------------------------------------------------------ |
| **Mutual exclusion** | At least one resource is non-sharable — only one process may hold it at a time |
| **Hold-and-wait**    | A process holds at least one resource while waiting to acquire another         |
| **No preemption**    | Resources cannot be forcibly taken; only voluntary release                     |
| **Circular wait**    | A cycle exists in the resource-allocation graph: P1→R1→P2→R2→P1                |


---

**Deadlock Prevention**

Attack one of the four conditions at design time:


| Condition to break | Technique                                                             | Trade-off                                      |
| ------------------ | --------------------------------------------------------------------- | ---------------------------------------------- |
| Hold-and-wait      | Acquire all locks at once; release all if any acquisition fails       | Less concurrency; retry logic                  |
| Circular wait      | **Global lock ordering:** always acquire in a fixed, documented order | Discipline at all call sites; lockdep enforces |
| No preemption      | `mutex_trylock()` with randomized exponential backoff                 | Retry overhead; livelock risk                  |
| Mutual exclusion   | Lock-free data structures                                             | Higher implementation complexity               |


**Global lock ordering (step-by-step):**

1. Enumerate all mutexes/locks in the subsystem.
2. Assign each a numeric rank (e.g. Lock A = 1, Lock B = 2).
3. Mandate: always acquire in ascending rank order.
4. Document the ordering at the lock declaration site.
5. Use `lockdep_set_class` so lockdep can verify ordering automatically.
6. In code review, reject any patch that acquires a lower-ranked lock while holding a higher-ranked one.

> **Pitfall:** Ordering breaks when new locks are added without updating the global order — two developers can introduce an A→C→D and A→D cycle. Use lockdep in CI to catch cycles.

**Deadlock Detection & Recovery**

- **Resource allocation graph:** Nodes = processes (P) and resources (R); edge P→R = P waits for R; edge R→P = R held by P. A cycle = deadlock.
- **Wait-for graph:** Simplified; edge P→Q means P waits for something held by Q; cycle = deadlock.

**Recovery options after detection:** (1) Abort one process and free its resources (choose victim by priority, runtime, resources held). (2) Preempt a resource from one process (requires rollback support). (3) Roll back to a safe checkpoint (requires checkpointing). **lockdep** in Linux detects *potential* deadlock at runtime when a new lock ordering is first seen — before an actual hang.

---

**Priority Inversion**

**Mechanism (four steps):**

1. **L** (low priority) acquires mutex M.
2. **H** (high priority) wakes and tries to acquire M — blocks waiting for L.
3. **M** (medium priority) wakes and **preempts L** (M > L).
4. L never runs → cannot release M → H waits indefinitely despite being highest priority.

H is effectively running at L’s priority — **inverted**. Duration of inversion is **unbounded** without PI.

```
  Time ─────────────────────────────────────────────────────────►
  H (high):  [woken]──[BLOCKED on mutex held by L]────────────────►
  M (med):   ─────────────────────[RUNNING]────────────────────────►
  L (low):   [holds mutex]──[PREEMPTED by M]──────[never runs]
```

> **Insight:** Inversion can occur even when every task is correct. It’s an emergent effect of interactions; code review alone cannot catch it.

</details>

### Mars Pathfinder（1997）

巡视器在着陆约 18 小时后出现**周期性复位**。**根因：** 无界优先级反转。


| 任务     | 优先级 | 角色                                    |
| -------- | -------- | --------------------------------------- |
| bc_dist  | 高     | 数据分发总线；需要该 mutex |
| bc_sched | 低      | 总线调度器；持有该 mutex           |
| ASI_MET  | 中   | 气象数据；CPU 密集型      |


**时序：** bc_sched（低）持有 mutex → bc_dist（高）阻塞 → ASI_MET（中）抢占 bc_sched 并运行 → bc_sched 始终无法运行 → bc_dist 错过其看门狗 → VxWorks 看门狗触发 → 系统整体复位。**修复：** 在共享 mutex 上启用**优先级继承**（该特性 VxWorks 中已有，但默认关闭）。修复通过上行链路上传至巡视器。

**教训：** 在安全关键或无法接触的系统中，只要某个 mutex 会在不同优先级的任务之间共享，PI 就必须作为默认配置。

---

## 优先级继承（PI）

当 H 阻塞在 L 持有的 mutex 上时，kernel **临时把 L 提升**到 H 的优先级，直到 L 释放该 mutex。**传递性 PI：** 若 L 又阻塞在 X 持有的另一个 mutex 上，该提升会**传播到 X**，使整条链都能运行并释放。

```
  BEFORE PI:                          AFTER PI:
  H (90): BLOCKED                     H (90): BLOCKED
  M (50): RUNNING                     M (50): BLOCKED (cannot preempt L)
  L (10): PREEMPTED                   L (10→90): RUNNING → releases mutex → H runs
```

- **Linux：** `rtmutex` 实现了 PI；PREEMPT_RT 把大多数 `spinlock_t` 转为 `rtmutex` → PI 覆盖全系统（例如 CAN、V4L2、GPU 驱动），且无需改动驱动。
- **用户态：**

```c
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
pthread_mutex_init(&mutex, &attr);
```

> **陷阱：** PI 在 SCHED_FIFO/SCHED_RR 下最有效。在 SCHED_OTHER（CFS）下，“优先级”是相对的，提升可能达不到预期效果。

---

## 优先级天花板协议

每个 mutex 都有一个**天花板优先级** = 所有可能获取它的任务中的最高优先级。任何获取它的任务在持有期间**立即**以天花板优先级运行——因此**高优先级等待者永远不必阻塞**（L 已处于天花板）。无需运行时探测即可防止反转；但需要**静态分析**（设计时已知所有获取者及其优先级）。


|                    | PI                            | 优先级天花板                    |
| ------------------ | ----------------------------- | ----------------------------------- |
| 提升时机 | H 阻塞在 L 上时            | 每次获取时                |
| 开销           | 仅在发生争用时          | 每次获取                   |
| 适用场景          | 动态任务集、通用 RT | AUTOSAR、ARINC 653（固定任务集） |


POSIX：`PTHREAD_PRIO_PROTECT`。**何时强制要求：** AUTOSAR OS 与 ARINC 653 要求形式化的有界反转证明；优先级天花板以静态方式提供该证明。

---

## lockdep —— 实时死锁检测器

`CONFIG_PROVE_LOCKING=y`，`CONFIG_LOCK_STAT=y`（可选，逐锁争用统计）。

- 为每个锁分配一个**锁类**（来自 kernel 二进制中的静态地址）。
- 记录每条获取链：“持有锁 A 时获取了锁 B。”
- 首次发现新的加锁顺序时就报告 **AB-BA 环**——在实际挂死之前。
- 报告**无效上下文：** 例如从 hardirq 中 `mutex_lock()`（在 IRQ 中获取会睡眠的锁）。

示例报告：

```
WARNING: possible circular locking dependency detected
task/1234 is trying to acquire lock: (&lockB){...}, at: function_b+0x30
but task holds lock: (&lockA){...}, taken at: function_a+0x20
which lock already depends on the new lock.
```

- **lockdep_assert_held(&lock)：** 记录并校验在某个位置持有锁。
- 在开发/CI 中启用；在生产中关闭（约 10% 开销）。

## 看门狗定时器

若进程未在超时前**喂看门狗**，系统就会复位或进入安全状态——这是针对漏网死锁的**纵深防御**。在 Mars Pathfinder 上，看门狗**确实**检测到 bc_dist 错过了截止时间；真正的修复是启用 PI，从而不再错过截止时间。**陷阱：** 把看门狗复位当作优先级反转可接受的“恢复”手段是错误的——例如在 ADAS 控制器上，它会强制退出自动驾驶；正确的修复是消除反转。

---


<details>
<summary>English original</summary>

**Mars Pathfinder (1997)**

Rover had **periodic resets** ~18 hours after landing. **Root cause:** unbounded priority inversion.


| Task     | Priority | Role                                    |
| -------- | -------- | --------------------------------------- |
| bc_dist  | High     | Data distribution bus; needed the mutex |
| bc_sched | Low      | Bus scheduler; held the mutex           |
| ASI_MET  | Medium   | Meteorological data; CPU-intensive      |


**Sequence:** bc_sched (low) held mutex → bc_dist (high) blocked → ASI_MET (medium) preempted bc_sched and ran → bc_sched never ran → bc_dist missed its watchdog → VxWorks watchdog fired → full system reset. **Fix:** Enable **priority inheritance** on the shared mutex (feature existed in VxWorks but was off by default). Fix was uploaded to the rover via uplink.

**Lesson:** PI must be the default for any mutex shared between tasks of different priorities in safety-critical or inaccessible systems.

---

**Priority Inheritance (PI)**

When H blocks on a mutex held by L, the kernel **temporarily boosts L** to H’s priority until L releases the mutex. **Transitive PI:** if L is also blocked on another mutex held by X, the boost **propagates to X** so the whole chain can run and release.

```
  BEFORE PI:                          AFTER PI:
  H (90): BLOCKED                     H (90): BLOCKED
  M (50): RUNNING                     M (50): BLOCKED (cannot preempt L)
  L (10): PREEMPTED                   L (10→90): RUNNING → releases mutex → H runs
```

- **Linux:** `rtmutex` implements PI; PREEMPT_RT converts most `spinlock_t` to `rtmutex` → PI system-wide (e.g. CAN, V4L2, GPU drivers) without driver changes.
- **Userspace:**

```c
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
pthread_mutex_init(&mutex, &attr);
```

> **Pitfall:** PI is most effective with SCHED_FIFO/SCHED_RR. On SCHED_OTHER (CFS), “priority” is relative and the boost may not have the desired effect.

---

**Priority Ceiling Protocol**

Each mutex has a **ceiling priority** = highest priority of any task that will ever acquire it. Any task that acquires it **immediately** runs at the ceiling for the duration — so a **high-priority waiter never has to block** (L is already at ceiling). Prevents inversion without runtime discovery; requires **static analysis** (all acquirers and their priorities known at design time).


|                    | PI                            | Priority ceiling                    |
| ------------------ | ----------------------------- | ----------------------------------- |
| When boost happens | When H blocks on L            | On every acquisition                |
| Overhead           | Only when contention          | Every acquisition                   |
| Suited to          | Dynamic task sets, general RT | AUTOSAR, ARINC 653 (fixed task set) |


POSIX: `PTHREAD_PRIO_PROTECT`. **When mandatory:** AUTOSAR OS and ARINC 653 require formal bounded-inversion proofs; priority ceiling provides that statically.

---

**lockdep — Live Deadlock Detector**

`CONFIG_PROVE_LOCKING=y`, `CONFIG_LOCK_STAT=y` (optional, per-lock contention stats).

- Assigns each lock a **lock class** (from static address in kernel binary).
- Records every acquisition chain: “lock A was held when lock B was acquired.”
- Reports **AB-BA cycles** the first time a new ordering is seen — before an actual hang.
- Reports **invalid context:** e.g. `mutex_lock()` from hardirq (sleeping lock in IRQ).

Example report:

```
WARNING: possible circular locking dependency detected
task/1234 is trying to acquire lock: (&lockB){...}, at: function_b+0x30
but task holds lock: (&lockA){...}, taken at: function_a+0x20
which lock already depends on the new lock.
```

- **lockdep_assert_held(&lock):** Documents and verifies that a lock is held at a given point.
- Enable in development/CI; disable in production (~10% overhead).

**Watchdog Timers**

If a process does not **kick the watchdog** within the timeout, the system resets or enters a safe state — **defense-in-depth** against deadlocks that slip through. On Mars Pathfinder the watchdog **did** detect bc_dist missing its deadline; the real fix was enabling PI so the deadline was not missed. **Pitfall:** Treating watchdog reset as an acceptable “recovery” for priority inversion is wrong — e.g. on an ADAS controller it forces disengagement; the correct fix is to eliminate the inversion.

---

</details>

## Part 2 小结表


| 问题 | 症状 | 检测 | 解决 |
| ------------------ | --------------------------- | ----------------------- | ----------------------------------- |
| 死锁 | 挂起；任务永久阻塞 | 资源图；lockdep | 锁排序；一次性获取；trylock |
| 优先级反转 | 高优先级任务停滞 | 延迟；看门狗 | PI mutex；优先级天花板 |
| 活锁 | CPU 繁忙，无进展 | 性能剖析 | 退避；仲裁 |
| 饥饿 | 低优先级永不运行 | 等待监控 | 老化；FIFO；提升 |


### Part 2 — 概念回顾

- **为什么“正确”的代码也会发生优先级反转？** 它是涌现出来的：L 正确地持有锁，H 正确地等待，M 正确地抢占。故障出在系统设计上（跨优先级共享 mutex 而未启用 PI）。
- **Mars Pathfinder 的教训：** PI 在 VxWorks 中已存在，但默认关闭。安全关键系统必须审计每一个跨优先级共享的 mutex 并启用 PI。
- **PI 与优先级天花板？** PI 只在高优先级等待者阻塞时提升持有者（动态）。天花板在每次获取时把获取者提升到天花板（静态）。对固定任务集（AUTOSAR、ARINC 653），天花板有更强的可预测性。
- **lockdep *不*报告什么？** 它不报告优先级反转——那需要延迟分析（如 cyclictest）或调度分析。
- **为什么传递性 PI 很重要？** 若 H 阻塞在 L 上、L 又阻塞在 X 上，X 也必须被提升；否则 L 无法运行，也就无法为 H 释放 mutex。

---

## AI 硬件关联

- **RCU：** 在线切换模型配置和 LoRA 而无需暂停推理；写者发布新配置，旧配置在宽限期后释放。读者（推理线程）永不阻塞。
- **SPSC ring + acquire/release：** 零拷贝相机通路（如 openpilot 中 camerad 与 modeld 之间的 VisionIPC）；30 Hz 帧通路上无 mutex；吞吐受内存带宽限制。
- **rcu_nocbs=：** 在 Jetson／推理专用核上，把 RCU 回调卸载到辅助 CPU，使隔离的 RT 核看不到回调抖动。
- **基于 CAS 的队列：** 用于 VisionIPC 式流水线做多生产者传感器聚合；每个传感器入队无需 mutex；推理线程每周期排空一次。
- **原子引用计数（kref、std::atomic）：** 管理 DMA-BUF 缓冲区在 V4L2 与 GPU 消费者之间的生命周期；最后一个消费者减到零时释放缓冲区。
- **PI mutex（rtmutex / PTHREAD_PRIO_INHERIT）：** openpilot 的 controlsd/plannerd 以及与 plannerd（中优先级）和 loggers（低优先级）共享的 cereal 状态需要它；Mars Pathfinder 是 ASIL-B 安全评审的标准参考案例。
- **ROS2：** rclcpp 实时 executor 文档明确要求 PTHREAD_PRIO_INHERIT，以免控制回调被数据记录线程饿死。
- **lockdep：** 在 Jetson/i.MX 上做相机、ISP 和 DMA 驱动 bring-up（上电点亮/调通）时的标配，部署前必须消除 AB-BA 环。
- **优先级天花板：** 用于任务集固定的 AUTOSAR ECU；对 ASIL-D 认证功能更强。
- **PREEMPT_RT：** 全系统 spinlock→rtmutex，在 CAN、V4L2 和 GPU 驱动上自动获得 PI，无需改驱动代码。

---

*Lecture Note 05 — 合并 L10（无锁：RCU、原子操作、内存序）与 L11（死锁、优先级反转、PI Mutex）。*


<details>
<summary>English original</summary>

**Part 2 Summary Table**


| Problem            | Symptom                     | Detection               | Solution                            |
| ------------------ | --------------------------- | ----------------------- | ----------------------------------- |
| Deadlock           | Hang; tasks blocked forever | Resource graph; lockdep | Lock ordering; acquire-all; trylock |
| Priority inversion | High-priority task stalls   | Latency; watchdog       | PI mutex; priority ceiling          |
| Livelock           | CPU busy, no progress       | Profiling               | Backoff; arbitration                |
| Starvation         | Low-priority never runs     | Wait monitoring         | Aging; FIFO; boost                  |


**Part 2 — Conceptual review**

- **Why can priority inversion occur with “correct” code?** It’s emergent: L holds the lock correctly, H waits correctly, M preempts correctly. The failure is in the system design (shared mutex across priority levels without PI).
- **Mars Pathfinder lesson:** PI existed in VxWorks but was off by default. Safety-critical systems must audit every mutex shared across priorities and enable PI.
- **PI vs priority ceiling?** PI boosts the holder only when a higher-priority waiter blocks (dynamic). Ceiling boosts the acquirer to ceiling on every acquisition (static). Ceiling has stronger predictability for fixed task sets (AUTOSAR, ARINC 653).
- **What does lockdep *not* report?** It does not report priority inversion — that needs latency analysis (e.g. cyclictest) or scheduling analysis.
- **Why is transitive PI important?** If H blocks on L and L blocks on X, X must also be boosted; otherwise L cannot run and cannot release the mutex for H.

---

**AI Hardware Connection**

- **RCU:** Live model config and LoRA swaps without pausing inference; writer publishes new config, old freed after grace period. Readers (inference threads) never block.
- **SPSC ring + acquire/release:** Zero-copy camera path (e.g. openpilot VisionIPC between camerad and modeld); no mutex on the 30 Hz frame path; throughput limited by memory bandwidth.
- **rcu_nocbs=:** On Jetson/inference-dedicated cores, offloads RCU callbacks to helper CPUs so isolated RT cores don’t see callback jitter.
- **CAS-based queues:** Used in VisionIPC-style pipelines for multi-producer sensor aggregation; each sensor enqueues without a mutex; inference thread drains once per cycle.
- **Atomic refcounts (kref, std::atomic):** Manage DMA-BUF buffer lifetime across V4L2 and GPU consumers; buffer freed when last consumer decrements to zero.
- **PI mutex (rtmutex / PTHREAD_PRIO_INHERIT):** Required for openpilot controlsd/plannerd and cereal shared state with plannerd (medium) and loggers (low); Mars Pathfinder is a standard reference in ASIL-B safety reviews.
- **ROS2:** rclcpp real-time executor docs explicitly require PTHREAD_PRIO_INHERIT so control callbacks are not starved by data-logging threads.
- **lockdep:** Standard in camera, ISP, and DMA driver bring-up on Jetson/i.MX before deployment; AB-BA cycles must be eliminated.
- **Priority ceiling:** Used in AUTOSAR ECUs with fixed task sets; stronger for ASIL-D certified functions.
- **PREEMPT_RT:** System-wide spinlock→rtmutex gives PI automatically on CAN, V4L2, and GPU drivers without driver code changes.

---

*Lecture Note 05 — Combines L10 (Lock-Free: RCU, Atomics, Memory Ordering) and L11 (Deadlock, Priority Inversion, PI Mutexes).*

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-Note-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-Note-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
