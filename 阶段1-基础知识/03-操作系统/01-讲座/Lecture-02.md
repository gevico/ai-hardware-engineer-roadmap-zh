---
title: 第 2 讲：进程、taskstruct 与 Linux 进程模型
description: 第 2 讲：进程、taskstruct 与 Linux 进程模型
---

# 第 2 讲：进程、task_struct 与 Linux 进程模型

## 概述

一个运行中的 AI 系统不是单个程序 —— 它是竞争同一 CPU、内存与硬件设备、彼此又不得互相干扰的一组进程。本讲要解决的核心问题是：Linux 内核如何跟踪每一个正在运行的程序，并在它们之间安全地复用硬件？需要建立的心智模型是：Linux 中每一个运行实体 —— 无论是进程、线程还是内核 worker —— 都由一个结构表示：`task_struct`。理解这个结构，就是理解内核如何看待你的代码。对 AI 硬件工程师而言，这一点很重要，因为调度器类、CPU 亲和性、cgroup 归属和内存布局全都是 `task_struct` 中的字段，而要调优推理流水线性能，就必须知道哪个旋钮对应哪个字段。

---

## 进程抽象

**进程**是执行中的程序。它由三个正交部分组成：

- **虚拟 CPU**：抢占时保存在 `task_struct` 中的寄存器状态（PC、SP、通用寄存器）
- **虚拟内存**：地址空间 —— text、data、heap、stack 以及内存映射区域，由 `mm_struct` 描述
- **资源**：文件描述符表、信号处理函数、socket、cgroup 归属 —— 均可从 `task_struct` 到达

**线程**是共享 `mm_struct` 和 `files_struct`、但拥有独立栈和寄存器状态的进程。Linux 在**内核层面不区分**“进程”与“线程” —— 二者都由 `task_struct` 表示。

> **关键洞察：** Linux 在内核层面没有单独的“线程”概念。线程只是一个与其他任务共享其 `mm_struct`（地址空间）的任务。这一设计简化了调度器，但意味着每个线程都有自己的 `task_struct`、自己的 PID（可通过 `gettid()` 查看）以及自己的调度器实体。当你为线程设置 CPU 亲和性时，你写入的就是该线程的 `task_struct.cpus_mask`。

---

## task_struct 关键字段

`task_struct` 定义在 `include/linux/sched.h` 中。它很大（约 5 KB）；此处只列出与 AI/嵌入式工作相关的字段。

```
task_struct — The kernel's representation of a running task
┌─────────────────────────────────────────────────────────┐
│  pid       — unique thread ID (gettid())                │
│  tgid      — thread group ID; all threads share this    │
│              (getpid() returns tgid)                    │
│  state     — TASK_RUNNING / TASK_INTERRUPTIBLE / etc.   │
├─────────────────────────────────────────────────────────┤
│  mm ──────────────────────────────> mm_struct           │
│                                     (virtual address    │
│                                      space, page table) │
├─────────────────────────────────────────────────────────┤
│  files ────────────────────────────> files_struct       │
│                                     (open FD table;     │
│                                      shared by threads) │
├─────────────────────────────────────────────────────────┤
│  sched_class ──> rt / fair / dl / idle / stop           │
│  se     — CFS/EEVDF entity (vruntime, load weight)      │
│  rt     — RT entity (static priority, time slice)       │
│  dl     — DEADLINE entity (runtime, deadline, period)   │
├─────────────────────────────────────────────────────────┤
│  cgroups ──────────────────────────> css_set            │
│  cpus_mask  — CPU affinity bitmask                      │
└─────────────────────────────────────────────────────────┘
```

| 字段 | 类型 | 用途 |
|---|---|---|
| `pid` | `pid_t` | 进程 ID —— 系统中每个线程唯一 |
| `tgid` | `pid_t` | 线程组 ID —— 所有线程共享；由 `getpid()` 返回 |
| `state` | `unsigned int` | 当前运行状态（TASK_RUNNING、TASK_INTERRUPTIBLE 等） |
| `mm` | `struct mm_struct *` | 虚拟内存描述符；内核线程为 NULL |
| `fs` | `struct fs_struct *` | 文件系统根目录与工作目录 |
| `files` | `struct files_struct *` | 打开的文件描述符表 |
| `signal` | `struct signal_struct *` | 信号处理函数、待处理信号、进程组 |
| `sched_class` | pointer | 由哪个调度器类处理该任务 |
| `se` | `struct sched_entity` | CFS/EEVDF 实体：vruntime、负载权重、时间片 |
| `rt` | `struct sched_rt_entity` | RT 实体：静态优先级、时间片 |
| `dl` | `struct sched_dl_entity` | DEADLINE 实体：runtime、deadline、period 预算 |
| `cgroups` | `struct css_set *` | cgroup v2 归属；指向 CSS 集合 |
| `cpus_mask` | `cpumask_t` | 允许的 CPU 亲和性；通过 `sched_setaffinity()` 设置 |

---


<details>
<summary>English original</summary>

**Lecture 2: Processes, task_struct & the Linux Process Model**

**Overview**

A running AI system is not a single program — it is a collection of competing processes that must share a CPU, memory, and hardware devices without interfering with each other. The core challenge this lecture addresses is: how does the Linux kernel track every running program and safely multiplex the hardware among them? The mental model to carry forward is that every running entity in Linux — whether a process, a thread, or a kernel worker — is represented by one structure: `task_struct`. Understanding this structure is understanding how the kernel sees your code. For an AI hardware engineer, this matters because scheduler class, CPU affinity, cgroup membership, and memory layout are all fields in `task_struct`, and tuning inference pipeline performance means knowing which knobs map to which fields.

---

**The Process Abstraction**

A **process** is a program in execution. It combines three orthogonal components:

- **Virtual CPU**: register state (PC, SP, general-purpose registers) saved in `task_struct` during preemption
- **Virtual memory**: address space — text, data, heap, stack, and memory-mapped regions, described by `mm_struct`
- **Resources**: file descriptor table, signal handlers, sockets, cgroup membership — all reachable from `task_struct`

**Threads** are processes that share `mm_struct` and `files_struct` but have independent stacks and register state. Linux makes **no kernel distinction** between "process" and "thread" — both are represented by `task_struct`.

> **Key Insight:** Linux has no separate "thread" concept at the kernel level. A thread is simply a task that shares its `mm_struct` (address space) with another task. This design simplifies the scheduler but means every thread has its own `task_struct`, its own PID (visible via `gettid()`), and its own scheduler entity. When you pin CPU affinity for a thread, you are writing to that thread's `task_struct.cpus_mask`.

---

**task_struct Key Fields**

`task_struct` is defined in `include/linux/sched.h`. It is large (~5 KB); only the fields relevant to AI/embedded work are listed here.

```
task_struct — The kernel's representation of a running task
┌─────────────────────────────────────────────────────────┐
│  pid       — unique thread ID (gettid())                │
│  tgid      — thread group ID; all threads share this    │
│              (getpid() returns tgid)                    │
│  state     — TASK_RUNNING / TASK_INTERRUPTIBLE / etc.   │
├─────────────────────────────────────────────────────────┤
│  mm ──────────────────────────────> mm_struct           │
│                                     (virtual address    │
│                                      space, page table) │
├─────────────────────────────────────────────────────────┤
│  files ────────────────────────────> files_struct       │
│                                     (open FD table;     │
│                                      shared by threads) │
├─────────────────────────────────────────────────────────┤
│  sched_class ──> rt / fair / dl / idle / stop           │
│  se     — CFS/EEVDF entity (vruntime, load weight)      │
│  rt     — RT entity (static priority, time slice)       │
│  dl     — DEADLINE entity (runtime, deadline, period)   │
├─────────────────────────────────────────────────────────┤
│  cgroups ──────────────────────────> css_set            │
│  cpus_mask  — CPU affinity bitmask                      │
└─────────────────────────────────────────────────────────┘
```

| Field | Type | Purpose |
|---|---|---|
| `pid` | `pid_t` | Process ID — unique per thread in the system |
| `tgid` | `pid_t` | Thread group ID — shared across all threads; returned by `getpid()` |
| `state` | `unsigned int` | Current run state (TASK_RUNNING, TASK_INTERRUPTIBLE, etc.) |
| `mm` | `struct mm_struct *` | Virtual memory descriptor; NULL for kernel threads |
| `fs` | `struct fs_struct *` | Filesystem root and working directory |
| `files` | `struct files_struct *` | Open file descriptor table |
| `signal` | `struct signal_struct *` | Signal handlers, pending signals, process group |
| `sched_class` | pointer | Which scheduler class handles this task |
| `se` | `struct sched_entity` | CFS/EEVDF entity: vruntime, load weight, slice |
| `rt` | `struct sched_rt_entity` | RT entity: static priority, time slice |
| `dl` | `struct sched_dl_entity` | DEADLINE entity: runtime, deadline, period budgets |
| `cgroups` | `struct css_set *` | cgroup v2 membership; pointer to CSS set |
| `cpus_mask` | `cpumask_t` | Allowed CPU affinity; set via `sched_setaffinity()` |

---

</details>

## 进程状态

理解 **进程状态** 对调试至关重要。`task_struct` 中的 state 字段精确告诉你 kernel 认为某个进程此刻在做什么。这就是 `ps` 命令的 `STAT` 列中可见的信息。

| State | Macro | 可被信号唤醒？ | `ps` 字母 | 示例原因 |
|---|---|---|---|---|
| Running 或 runnable | `TASK_RUNNING` | — | R | 在 CPU 上或位于运行队列 |
| Interruptible sleep | `TASK_INTERRUPTIBLE` | 是 | S | 等待 I/O、事件或定时器 |
| Uninterruptible sleep | `TASK_UNINTERRUPTIBLE` | 否 | D | DMA 等待、kernel I/O 路径（V4L2、NVMe） |
| Killable | `TASK_KILLABLE` | 仅 SIGKILL | D | 不可中断，但可被 kill |
| Stopped | `__TASK_STOPPED` | SIGCONT | T | SIGSTOP 或调试器 attach |
| Zombie | `EXIT_ZOMBIE` | — | Z | 已退出；等待父进程 `wait()` 调用 |
| Dead | `TASK_DEAD` | — | — | 父进程 reap 后完全回收 |

`ps` 中的 `D` 状态表示进程 **阻塞在 kernel I/O 路径内部**。持续的 `D` 状态是 **驱动挂死指示** — 常见于 V4L2 buffer dequeue 失败或 NVMe 超时期间。

```
Process State Machine
                   ┌───────────────────────────────┐
                   │                               │
           schedule()                      preempted / slice expires
                   │                               │
                   ▼                               │
           ┌─────────────┐     blocks on I/O   ┌──┴──────────┐
  fork() → │ TASK_RUNNING│ ─────────────────→  │ TASK_INTER- │
  exec()   │  (runnable) │ ←─────────────────  │  RUPTIBLE   │
           └──────┬──────┘    signal / event   └─────────────┘
                  │                                    │
        kernel DMA/I/O path                      SIGKILL only
                  │                                    ▼
                  ▼                          ┌──────────────────┐
           ┌────────────┐                   │  TASK_KILLABLE   │
           │  TASK_UN-  │                   └──────────────────┘
           │INTERRUPTIBLE│
           └──────┬──────┘
                  │
               exit()
                  ▼
           ┌────────────┐    parent wait()   ┌──────────┐
           │EXIT_ZOMBIE │ ─────────────────→ │TASK_DEAD │
           └────────────┘                   └──────────┘
```

> **关键洞察：** `TASK_UNINTERRUPTIBLE` 之所以存在，是因为某些 kernel 操作 — 尤其是 DMA 传输和硬件 I/O — 无法在中途安全中断。如果等待 `VIDIOC_DQBUF`（V4L2 dequeue buffer）的进程可在任意时刻被杀掉，DMA 引擎就可能写入已释放的内存。`D` 状态是 kernel 在说「我正处于硬件操作中途；请稍等。」持续的 `D` 状态意味着硬件从未完成其操作。

> **常见陷阱：** 僵尸进程（`ps` 中的 `Z`）不是子进程的 bug — 而是父进程的 bug。子进程已退出并释放了内存，但 kernel 会保留一个最小的 `task_struct` 条目，直到父进程调用 `wait()` 收集退出码。如果父进程从不调用 `wait()`，僵尸进程会不断累积，最终耗尽 PID namespace。在 openpilot 中，进程监管者必须 reap 所有子进程。

---

## fork / exec / wait

**fork/exec/wait** 三者是 Unix 中创建新进程的基本机制。理解这一序列，也是理解 openpilot 的**多进程架构**为何高效的关键。


<details>
<summary>English original</summary>

**Process States**

Understanding **process states** is essential for debugging. The state field in `task_struct` tells you exactly what the kernel thinks a process is doing at any moment. This is the information visible in the `ps` command's `STAT` column.

| State | Macro | Wakeable by signal? | `ps` letter | Example cause |
|---|---|---|---|---|
| Running or runnable | `TASK_RUNNING` | — | R | On CPU or on a run queue |
| Interruptible sleep | `TASK_INTERRUPTIBLE` | Yes | S | Waiting for I/O, event, or timer |
| Uninterruptible sleep | `TASK_UNINTERRUPTIBLE` | No | D | DMA wait, kernel I/O path (V4L2, NVMe) |
| Killable | `TASK_KILLABLE` | SIGKILL only | D | Uninterruptible but yields to kill |
| Stopped | `__TASK_STOPPED` | SIGCONT | T | SIGSTOP or debugger attach |
| Zombie | `EXIT_ZOMBIE` | — | Z | Exited; awaiting parent `wait()` call |
| Dead | `TASK_DEAD` | — | — | Fully reclaimed after parent reaps |

`D` state in `ps` indicates a process **blocked inside a kernel I/O path**. Persistent `D` state is a **driver hang indicator** — common during V4L2 buffer dequeue failures or NVMe timeout.

```
Process State Machine
                   ┌───────────────────────────────┐
                   │                               │
           schedule()                      preempted / slice expires
                   │                               │
                   ▼                               │
           ┌─────────────┐     blocks on I/O   ┌──┴──────────┐
  fork() → │ TASK_RUNNING│ ─────────────────→  │ TASK_INTER- │
  exec()   │  (runnable) │ ←─────────────────  │  RUPTIBLE   │
           └──────┬──────┘    signal / event   └─────────────┘
                  │                                    │
        kernel DMA/I/O path                      SIGKILL only
                  │                                    ▼
                  ▼                          ┌──────────────────┐
           ┌────────────┐                   │  TASK_KILLABLE   │
           │  TASK_UN-  │                   └──────────────────┘
           │INTERRUPTIBLE│
           └──────┬──────┘
                  │
               exit()
                  ▼
           ┌────────────┐    parent wait()   ┌──────────┐
           │EXIT_ZOMBIE │ ─────────────────→ │TASK_DEAD │
           └────────────┘                   └──────────┘
```

> **Key Insight:** `TASK_UNINTERRUPTIBLE` exists because some kernel operations — particularly DMA transfers and hardware I/O — cannot be safely interrupted mid-way. If a process waiting on `VIDIOC_DQBUF` (V4L2 dequeue buffer) could be killed at any point, the DMA engine might write into freed memory. The `D` state is the kernel saying "I'm in the middle of a hardware operation; please wait." A persistent `D` state means the hardware never completed its operation.

> **Common Pitfall:** A zombie process (`Z` in `ps`) is not a bug in the child — it is a bug in the parent. The child has exited and freed its memory, but the kernel keeps a minimal `task_struct` entry until the parent calls `wait()` to collect the exit code. If a parent process never calls `wait()`, zombies accumulate and eventually exhaust the PID namespace. In openpilot, process supervisors must reap all child processes.

---

**fork / exec / wait**

The **fork/exec/wait** trio is the fundamental mechanism for creating new processes in Unix. Understanding this sequence is also key to understanding why openpilot's **multi-process architecture** works efficiently.

</details>

### fork() 与写时复制

`fork()` 以父进程的结构性副本创建子进程。物理内存**不会**立即复制——**写时复制**推迟了分配：

```
fork() — Copy-on-Write Memory Model
┌─────────────┐   fork()   ┌─────────────┐
│   Parent    │ ─────────> │    Child    │
│   Process   │            │   Process   │
│             │            │             │
│ page table: │            │ page table: │
│  0x1000 ──────────────────────> [RO]  │  ← same physical page
│  0x2000 ──────────────────────> [RO]  │    marked read-only
│  0x3000 ──────────────────────> [RO]  │
└─────────────┘            └─────────────┘
                                  │
                           write to 0x2000
                                  │
                                  ▼
                           ┌─────────────┐
                           │ PAGE FAULT  │
                           │ kernel      │
                           │ allocates   │
                           │ new page    │
                           │ child 0x2000│
                           │ → new page  │
                           └─────────────┘
```

调用 `fork()` 时的执行序列：

1. **内核复制 `task_struct`**：分配一个新的 `task_struct`，用父进程的内容填充，并赋予新的 PID。
2. **复制 `mm_struct`**：新任务获得自己的虚拟地址空间描述符，但页表项指向与父进程相同的物理页。
3. **页被标记为只读**：内核在父、子进程的页表中把所有共享页标记为只读。
4. **子进程返回 0，父进程返回子进程 PID**：两者都从 `fork()` 之后的那条指令继续执行。
5. **首次写入时**：触发缺页异常。内核分配一个新的物理页，复制内容，只更新执行写操作的那个任务的页表。这才是真正的 "copy"——推迟到必要时才做。
6. **代码页永不复制**：只读的 text 段（程序的可执行代码）被真正永久共享，从不复制。

CoW 让 `fork()` **即使对大型进程也很快**。openpilot 的多进程架构正依赖于此：`camerad`、`modeld`、`plannerd` 和 `controlsd` 都从一个公共基座 fork 而来，无需复制数 MB 的共享库代码。

### exec() 与 wait()

`execve()` **将当前地址空间替换**为新的 ELF 二进制。未设置 `O_CLOEXEC` 的文件描述符会跨 exec 存活。`waitpid()` 回收**僵尸子进程**，收回其 `task_struct`。没有 `wait()`，僵尸进程会不断累积，最终耗尽 PID 空间。

如果父进程在回收前退出，**孤儿进程**会被重新挂到 PID 1（systemd）下，由后者在内部调用 `wait()`。

```
The fork / exec / wait lifecycle
┌─────────┐
│ Parent  │
│ Process │
└────┬────┘
     │ fork()
     ├──────────────────────────────────┐
     │                                  ▼
     │                           ┌─────────────┐
     │ (continues running)       │    Child    │
     │                           │  (PID = N)  │
     │                           └──────┬──────┘
     │                                  │ execve("/usr/bin/camerad")
     │                                  ▼
     │                           ┌─────────────┐
     │                           │  camerad    │
     │                           │  (new ELF)  │
     │                           └──────┬──────┘
     │                                  │ exit(0)
     │                                  ▼
     │                           ┌─────────────┐
     │ waitpid(N, &status, 0) ←─ │   ZOMBIE    │
     │ (reaps child)             │  (PID = N)  │
     ▼                           └─────────────┘
┌─────────┐
│ Parent  │
│(continues│
└─────────┘
```

---

## clone() 与线程

`clone()` 是 `fork()` 和 `pthread_create()` **两者**背后的底层系统调用。**flags 参数**决定新任务与父进程共享什么。

| 标志 | 效果 |
|---|---|
| `CLONE_VM` | 共享 `mm_struct`——两个任务使用同一地址空间（线程） |
| `CLONE_FILES` | 共享打开的文件描述符表 |
| `CLONE_SIGHAND` | 共享信号处理函数 |
| `CLONE_NEWPID` | 新建 PID namespace——子进程在其中是 PID 1 |
| `CLONE_NEWNET` | 新建 network namespace——隔离的接口/路由表 |
| `CLONE_NEWNS` | 新建 mount namespace——隔离的文件系统视图 |

线程不过是用 `CLONE_VM | CLONE_FILES | CLONE_SIGHAND` 创建的任务。`getpid()` 返回 `tgid`（对同一进程内所有线程相同）；`gettid()` 返回每线程唯一的 `pid`。

> **关键洞察：** 线程与进程是同一结构（`task_struct`）这一事实，意味着调度器对二者一视同仁。一个处于 `SCHED_FIFO`、优先级为 80 的线程抢占优先级 50 的进程，与它抢占另一个优先级 50 的线程一样干脆。CPU 亲和性、cgroup 归属和调度类都是按 `task_struct` 区分的——也就是说，你可以为同一进程内的不同线程设置不同的调度策略。

---


<details>
<summary>English original</summary>

**fork() and Copy-on-Write**

`fork()` creates a child as a structural copy of the parent. Physical memory is **not** copied immediately — **Copy-on-Write** defers allocation:

```
fork() — Copy-on-Write Memory Model
┌─────────────┐   fork()   ┌─────────────┐
│   Parent    │ ─────────> │    Child    │
│   Process   │            │   Process   │
│             │            │             │
│ page table: │            │ page table: │
│  0x1000 ──────────────────────> [RO]  │  ← same physical page
│  0x2000 ──────────────────────> [RO]  │    marked read-only
│  0x3000 ──────────────────────> [RO]  │
└─────────────┘            └─────────────┘
                                  │
                           write to 0x2000
                                  │
                                  ▼
                           ┌─────────────┐
                           │ PAGE FAULT  │
                           │ kernel      │
                           │ allocates   │
                           │ new page    │
                           │ child 0x2000│
                           │ → new page  │
                           └─────────────┘
```

The sequence when `fork()` is called:

1. **Kernel copies `task_struct`**: a new `task_struct` is allocated and populated from the parent's, with a new PID.
2. **`mm_struct` is duplicated**: the new task gets its own virtual address space descriptor, but the page table entries point to the same physical pages as the parent.
3. **Pages marked read-only**: the kernel marks all shared pages read-only in both parent and child page tables.
4. **Child returns 0, parent returns child PID**: both resume execution from the instruction after `fork()`.
5. **On first write**: a page fault fires. The kernel allocates a new physical page, copies the content, and updates only the writing task's page table. This is the actual "copy" — deferred until necessary.
6. **Code pages are never copied**: read-only text segments (the program's executable code) are genuinely shared forever, never duplicated.

CoW makes `fork()` **fast even for large processes**. openpilot's multi-process architecture relies on this: `camerad`, `modeld`, `plannerd`, and `controlsd` each fork from a common base without duplicating megabytes of shared library code.

**exec() and wait()**

`execve()` **replaces the current address space** with a new ELF binary. File descriptors without `O_CLOEXEC` survive across exec. `waitpid()` reaps a **zombie child**, reclaiming its `task_struct`. Without `wait()`, zombies accumulate and eventually exhaust PID space.

If a parent exits before reaping, **orphan children** are reparented to PID 1 (systemd), which calls `wait()` internally.

```
The fork / exec / wait lifecycle
┌─────────┐
│ Parent  │
│ Process │
└────┬────┘
     │ fork()
     ├──────────────────────────────────┐
     │                                  ▼
     │                           ┌─────────────┐
     │ (continues running)       │    Child    │
     │                           │  (PID = N)  │
     │                           └──────┬──────┘
     │                                  │ execve("/usr/bin/camerad")
     │                                  ▼
     │                           ┌─────────────┐
     │                           │  camerad    │
     │                           │  (new ELF)  │
     │                           └──────┬──────┘
     │                                  │ exit(0)
     │                                  ▼
     │                           ┌─────────────┐
     │ waitpid(N, &status, 0) ←─ │   ZOMBIE    │
     │ (reaps child)             │  (PID = N)  │
     ▼                           └─────────────┘
┌─────────┐
│ Parent  │
│(continues│
└─────────┘
```

---

**clone() and Threads**

`clone()` is the underlying syscall behind **both** `fork()` and `pthread_create()`. The **flags argument** determines what the new task shares with its parent.

| Flag | Effect |
|---|---|
| `CLONE_VM` | Share `mm_struct` — both tasks use the same address space (thread) |
| `CLONE_FILES` | Share open file descriptor table |
| `CLONE_SIGHAND` | Share signal handlers |
| `CLONE_NEWPID` | New PID namespace — child is PID 1 inside it |
| `CLONE_NEWNET` | New network namespace — isolated interface/routing table |
| `CLONE_NEWNS` | New mount namespace — isolated filesystem view |

A thread is simply a task created with `CLONE_VM | CLONE_FILES | CLONE_SIGHAND`. `getpid()` returns `tgid` (same for all threads in a process); `gettid()` returns the unique per-thread `pid`.

> **Key Insight:** The fact that threads and processes are the same structure (`task_struct`) means the scheduler treats them identically. A thread at `SCHED_FIFO` priority 80 will preempt a process at priority 50 just as readily as it preempts another thread at priority 50. CPU affinity, cgroup membership, and scheduling class are per-`task_struct` — meaning you can set different scheduling policies for different threads within the same process.

---

</details>

## Linux Namespaces

**命名空间**对内核资源进行分区，使一组进程看到隔离的视图。它们是容器的基础。

| Namespace | 隔离内容 | 容器用途 |
|---|---|---|
| `pid` | 进程 ID 编号 | 容器 init 显示为 PID 1 |
| `mnt` | 文件系统挂载树 | 容器私有根文件系统 |
| `net` | 网络接口、路由、iptables | 每容器网络 |
| `uts` | 主机名与域名 | 容器专属主机名 |
| `ipc` | System V IPC、POSIX 消息队列 | 容器间 IPC 隔离 |
| `user` | UID/GID 映射 | 无 root 容器 |
| `cgroup` | cgroup 根视图 | 嵌套 cgroup 层级 |
| `time` | 时钟偏移 | 每容器 time namespace |

```bash
ls -la /proc/[pid]/ns/          # inspect namespace membership of a running process
unshare --pid --fork bash       # launch shell in new PID namespace
```

CUDA 要在容器内初始化，必须把 GPU 设备文件（`/dev/nvidia0`、`/dev/nvhost-ctrl`）bind-mount 进容器的 mount namespace。

> **常见陷阱：** 在 Docker 容器内跑 TensorRT 或 CUDA 时，容器有自己的 mount namespace。NVIDIA runtime 必须把 `/dev/nvidia*` 和 `/dev/nvhost-*` bind-mount 进容器。如果这一步静默失败，即使宿主机能看到 GPU，CUDA 也会报 “no devices found”。在追查 CUDA 驱动 bug 之前，务必先检查 `docker run --gpus all` 或 NVIDIA 容器 runtime 配置。

现在理解了进程如何创建与隔离，接着看内核如何限制进程可消耗的资源 —— cgroups。

---

## cgroups v2：资源控制

**统一层级**位于 `/sys/fs/cgroup/`。所有控制器（cpu、memory、io、cpuset）都挂到同一棵层级树上。

| 控制器 | 关键文件 | 示例值 | 效果 |
|---|---|---|---|
| cpu | `cpu.max` | `50000 100000` | 单 CPU 的 50%（quota µs / period µs） |
| cpuset | `cpuset.cpus` | `0-3` | 限制到核 0–3 |
| cpuset | `cpuset.mems` | `0` | 限制到 NUMA 节点 0 |
| memory | `memory.max` | `4G` | 超限则 OOM-kill |
| memory | `memory.swap.max` | `0` | 对该组禁用 swap |
| io | `io.max` | `8:0 rbps=104857600` | 设备 8:0 上读 100 MB/s |
| pids | `pids.max` | `512` | 限制不可信容器中的 fork 炸弹 |

```bash
cat /proc/[pid]/cgroup              # cgroup membership path for a process
cat /sys/fs/cgroup/[path]/cpu.stat  # throttled_usec, nr_throttled — detect throttling
```

Kubernetes 用 **cgroup v2** 对推理 pod 执行 CPU 和内存限制。一个设了 `cpu.max = 200000 1000000`（单核的 20%）的 pod，超出该预算就会被 `modeld` **限流**。

> **关键洞察：** `cpu.stat` 的 `throttled_usec` 字段是 cgroup 导致延迟的确凿证据。如果推理 pod 出现稳定的 2–3 ms 延迟尖峰，且 `throttled_usec` 在攀升，那么瓶颈就是 Kubernetes 的 CPU 限制 —— 不是模型、不是 GPU、也不是调度器。当 `perf` 和 `bpftrace` 显示推理线程出现 CPU 停顿时，这是第一个该查的文件。

> **常见陷阱：** 在 NUMA 系统上只设 `cpuset.cpus` 而不设 `cpuset.mems`，可能导致内存从错误的 NUMA 节点分配。这会造成跨节点内存流量，每次缓存行未命中额外增加约 100 ns。在多路服务器上跑对延迟敏感的推理工作负载时，务必将 CPU 亲和性与 NUMA 内存节点绑定配对设置。

---


<details>
<summary>English original</summary>

**Linux Namespaces**

**Namespaces** partition kernel resources so a set of processes sees an isolated view. They are the foundation of containers.

| Namespace | Isolates | Container use |
|---|---|---|
| `pid` | Process ID numbering | Container init appears as PID 1 |
| `mnt` | Filesystem mount tree | Container-private root filesystem |
| `net` | Network interfaces, routes, iptables | Per-container networking |
| `uts` | Hostname and domain name | Container-specific hostname |
| `ipc` | System V IPC, POSIX message queues | IPC isolation between containers |
| `user` | UID/GID mappings | Rootless containers |
| `cgroup` | cgroup root view | Nested cgroup hierarchies |
| `time` | Clock offsets | Time namespace per container |

```bash
ls -la /proc/[pid]/ns/          # inspect namespace membership of a running process
unshare --pid --fork bash       # launch shell in new PID namespace
```

GPU device files (`/dev/nvidia0`, `/dev/nvhost-ctrl`) must be bind-mounted into the container's mount namespace for CUDA to initialize inside containers.

> **Common Pitfall:** When running TensorRT or CUDA inside a Docker container, the container has its own mount namespace. The NVIDIA runtime must bind-mount `/dev/nvidia*` and `/dev/nvhost-*` into the container. If this fails silently, CUDA will report "no devices found" even though the host can see the GPU. Always check `docker run --gpus all` or the NVIDIA container runtime configuration before chasing a CUDA driver bug.

Now that we understand how processes are created and isolated, let's look at how the kernel limits what resources they can consume — cgroups.

---

**cgroups v2: Resource Control**

**Unified hierarchy** at `/sys/fs/cgroup/`. All controllers (cpu, memory, io, cpuset) attach to the same hierarchy tree.

| Controller | Key file | Example value | Effect |
|---|---|---|---|
| cpu | `cpu.max` | `50000 100000` | 50% of one CPU (quota µs / period µs) |
| cpuset | `cpuset.cpus` | `0-3` | Restrict to cores 0–3 |
| cpuset | `cpuset.mems` | `0` | Restrict to NUMA node 0 |
| memory | `memory.max` | `4G` | OOM-kill if exceeded |
| memory | `memory.swap.max` | `0` | Disable swap for this group |
| io | `io.max` | `8:0 rbps=104857600` | 100 MB/s read on device 8:0 |
| pids | `pids.max` | `512` | Limit fork bombs in untrusted containers |

```bash
cat /proc/[pid]/cgroup              # cgroup membership path for a process
cat /sys/fs/cgroup/[path]/cpu.stat  # throttled_usec, nr_throttled — detect throttling
```

Kubernetes uses **cgroup v2** to enforce CPU and memory limits on inference pods. A pod with `cpu.max = 200000 1000000` (20% of one core) will have `modeld` **throttled** if it exceeds that budget.

> **Key Insight:** `cpu.stat`'s `throttled_usec` field is the smoking gun for cgroup-induced latency. If your inference pod shows consistent 2–3 ms latency spikes and `throttled_usec` is climbing, the Kubernetes CPU limit is the bottleneck — not the model, not the GPU, not the scheduler. This is the first file to check after `perf` and `bpftrace` show CPU stalls in the inference thread.

> **Common Pitfall:** Setting `cpuset.cpus` without also setting `cpuset.mems` on a NUMA system can lead to memory being allocated from the wrong NUMA node. This causes cross-node memory traffic that adds ~100 ns per cache line miss. Always pair CPU affinity with NUMA memory node pinning for latency-sensitive inference workloads on multi-socket servers.

---

</details>

## 上下文切换机制

现在我们已经理解 kernel 如何跟踪任务及其资源，接下来看在各任务之间切换执行的操作 —— **上下文切换**。

`context_switch()` 位于 `kernel/sched/core.c` 中。它执行两种不同的操作：

1. **`switch_mm_irqs_off()`** —— 安装新进程的**虚拟地址空间**。在 x86 上写入 CR3（页表基址寄存器）；在 ARM64 上写入 TTBR0_EL1。这一步使新进程的内存可见，并隐藏旧进程的内存。此后的每次内存访问都经过新的页表。

2. **`switch_to()`** —— 将即将切出的任务的被调用者保存寄存器（x86 上的 rbx、rbp、r12–r15；ARM64 上的 x19–x28、fp、lr）和栈指针保存到其 `task_struct`，然后恢复即将切入的任务的已保存寄存器。当 `switch_to()` 返回时，CPU 已在新任务的上下文中执行。

3. **恢复** —— 新任务从它上次被抢占的那条指令处精确恢复，仿佛什么都没发生过。它的寄存器状态、栈和虚拟内存全部被还原。

```
Context Switch Timeline
┌──────────────┐                    ┌──────────────┐
│  Task A      │                    │  Task B      │
│  (running)   │                    │  (waiting)   │
└──────┬───────┘                    └──────┬───────┘
       │                                   │
       │ scheduler tick / block            │
       │                                   │
       ▼                                   │
┌─────────────────────────────────┐        │
│  context_switch(A → B)          │        │
│  1. switch_mm: write TTBR0/CR3  │        │
│  2. switch_to: save A's regs    │        │
│               restore B's regs  │        │
└─────────────────────┬───────────┘        │
                      │                    │
                      └───────────────────►│
                                           │  (Task B resumes here)
                                           ▼
                                    ┌──────────────┐
                                    │  Task B      │
                                    │  (running)   │
                                    └──────────────┘
```

**TLB 开销**：ARM64 使用**带 ASID 标记的 TLB** —— 在具有有效 ASID 的任务之间切换可避免完全刷新 TLB。x86 出于同样目的使用 **PCID**。上下文切换开销：1–10 µs，取决于缓存状态以及是否必须刷新 TLB。对于 1 kHz 的控制循环（`controlsd` 以 100 Hz 输出 CAN），调度器抖动必须远低于 1 ms。

> **关键洞见：** TLB（Translation Lookaside Buffer）是一种硬件缓存，存放近期的虚拟地址到物理地址转换。若没有 ASID 标记，每次上下文切换都需要完全刷新 TLB —— 也就是让所有已缓存的转换失效 —— 因为新进程的地址空间完全不同。ASID 标记让硬件能区分“进程 A 的转换”与“进程 B 的转换”，因此旧条目仍然有效，新进程可以立即命中 TLB。这就是为什么 ASID 耗尽（256 或 65536 个 ASID 槽位全部占满）会强制刷新 TLB 并增加延迟。

---

## /proc/[pid]/ runtime 检查

| Path | Contents |
|---|---|
| `/proc/[pid]/maps` | 虚拟内存区域：地址、权限、后备文件 |
| `/proc/[pid]/smaps` | 各区域的 RSS 和 PSS；识别内存浪费与共享 |
| `/proc/[pid]/status` | 状态、VmRSS、threads、capability 集合 |
| `/proc/[pid]/fd/` | 指向已打开文件、socket、V4L2 设备节点的符号链接 |
| `/proc/[pid]/sched` | CFS/EEVDF：vruntime、nr_voluntary_switches、se.load.weight |
| `/proc/[pid]/wchan` | 任务当前睡眠所在的 kernel 函数 |
| `/proc/[pid]/cgroup` | cgroup v2 所属路径 |
| `/proc/[pid]/oom_score` | OOM killer 评分；内存压力下分值高者先被杀 |
| `/proc/[pid]/oom_score_adj` | 可写：调整 OOM 优先级（-1000 = 永不杀，+1000 = 先杀） |

> **常见陷阱：** 内存压力下，OOM killer 选择 `oom_score` 最高的进程终止。默认情况下，占用内存大的进程评分最高。在同时运行 `modeld` 和数据记录服务的 Jetson 上，如果 `modeld` 的 RSS 更大，OOM killer 可能终止 `modeld` 而不是记录器。对关键推理进程设置 `oom_score_adj = -500` 以保护它们。反过来，对非关键的记录进程设置 `oom_score_adj = +500`，使其优先被杀。

---


<details>
<summary>English original</summary>

**Context Switch Mechanics**

Now that we understand how the kernel tracks tasks and their resources, let's look at the operation that switches execution between them — the **context switch**.

`context_switch()` is in `kernel/sched/core.c`. It performs two distinct operations:

1. **`switch_mm_irqs_off()`** — install the new process's **virtual address space**. On x86 this writes CR3 (the page table base register); on ARM64 it writes TTBR0_EL1. This is the step that makes the new process's memory visible and hides the old process's memory. Every memory access after this point goes through the new page table.

2. **`switch_to()`** — save the outgoing task's callee-saved registers (rbx, rbp, r12–r15 on x86; x19–x28, fp, lr on ARM64) and stack pointer to its `task_struct`, then restore the incoming task's saved registers. When `switch_to()` returns, the CPU is executing in the context of the new task.

3. **Resume** — the new task resumes at the exact instruction where it was last preempted, as if nothing happened. Its register state, stack, and virtual memory are all restored.

```
Context Switch Timeline
┌──────────────┐                    ┌──────────────┐
│  Task A      │                    │  Task B      │
│  (running)   │                    │  (waiting)   │
└──────┬───────┘                    └──────┬───────┘
       │                                   │
       │ scheduler tick / block            │
       │                                   │
       ▼                                   │
┌─────────────────────────────────┐        │
│  context_switch(A → B)          │        │
│  1. switch_mm: write TTBR0/CR3  │        │
│  2. switch_to: save A's regs    │        │
│               restore B's regs  │        │
└─────────────────────┬───────────┘        │
                      │                    │
                      └───────────────────►│
                                           │  (Task B resumes here)
                                           ▼
                                    ┌──────────────┐
                                    │  Task B      │
                                    │  (running)   │
                                    └──────────────┘
```

**TLB cost**: ARM64 uses **ASID-tagged TLBs** — switching between tasks with valid ASIDs avoids a full TLB flush. x86 uses **PCID** for the same purpose. Context switch overhead: 1–10 µs depending on cache state and whether the TLB must be flushed. For a 1 kHz control loop (`controlsd` at 100 Hz CAN output), scheduler jitter must stay well below 1 ms.

> **Key Insight:** The TLB (Translation Lookaside Buffer) is a hardware cache that stores recent virtual-to-physical address translations. Without ASID tags, every context switch would require flushing the TLB entirely — that is, invalidating all cached translations — because the new process has a completely different address space. ASID tags let the hardware distinguish "translation for process A" from "translation for process B," so old entries remain valid and the new process can hit the TLB immediately. This is why ASID exhaustion (when all 256 or 65536 ASID slots fill up) forces a TLB flush and adds latency.

---

**/proc/[pid]/ Runtime Inspection**

| Path | Contents |
|---|---|
| `/proc/[pid]/maps` | Virtual memory regions: address, permissions, backing file |
| `/proc/[pid]/smaps` | Per-region RSS and PSS; identifies memory waste and sharing |
| `/proc/[pid]/status` | State, VmRSS, threads, capability sets |
| `/proc/[pid]/fd/` | Symlinks to open files, sockets, V4L2 device nodes |
| `/proc/[pid]/sched` | CFS/EEVDF: vruntime, nr_voluntary_switches, se.load.weight |
| `/proc/[pid]/wchan` | Kernel function where task is currently sleeping |
| `/proc/[pid]/cgroup` | cgroup v2 membership path |
| `/proc/[pid]/oom_score` | OOM killer score; higher value killed first under memory pressure |
| `/proc/[pid]/oom_score_adj` | Writable: tune OOM priority (-1000 = never kill, +1000 = kill first) |

> **Common Pitfall:** Under memory pressure, the OOM killer selects the process with the highest `oom_score` to terminate. By default, large-memory processes score highest. On a Jetson running both `modeld` and a data logging service, the OOM killer may terminate `modeld` rather than the logger if `modeld` has a larger RSS. Set `oom_score_adj = -500` on critical inference processes to protect them. Conversely, set `oom_score_adj = +500` on non-critical logging processes so they are killed first.

---

</details>

## 概述

| 状态 | 宏 | 可唤醒？ | 示例原因 |
|---|---|---|---|
| 运行 / 可运行 | `TASK_RUNNING` | — | 在 CPU 上运行或等待运行队列 |
| 可中断睡眠 | `TASK_INTERRUPTIBLE` | 是（信号） | 阻塞在 `read()`、`epoll_wait()` |
| 不可中断睡眠 | `TASK_UNINTERRUPTIBLE` | 否 | DMA 等待、驱动中的 `VIDIOC_DQBUF` |
| 可终止 | `TASK_KILLABLE` | 仅 SIGKILL | NFS 软挂载等待 |
| 已停止 | `__TASK_STOPPED` | SIGCONT | 调试器、SIGSTOP |
| 僵尸 | `EXIT_ZOMBIE` | — | 等待父进程 `waitpid()` |

### 概念回顾

- **为什么 Linux 对进程和线程使用同一个 `task_struct`？** 简单且一致。调度器、OOM killer、cgroup 记账和 CPU 亲和性机制都作用于 `task_struct`，无需为线程做特殊处理。进程与线程的区别完全在于通过 `clone()` 标志决定哪些字段（`mm`、`files`）被共享。
- **什么是僵尸进程，它为什么存在？** 僵尸进程是已调用 `exit()` 但父进程尚未调用 `wait()` 的进程。内核保留一份最小的 `task_struct`，以便父进程取回子进程的退出状态。僵尸进程占用一个 PID 槽位，但不消耗 CPU 或内存。当父进程未能回收子进程时，它们就会累积。
- **为什么 `fork()` 使用 Copy-on-Write 而不是立即复制内存？** 在 `fork()` 时复制整个地址空间，对大型进程而言慢得无法接受。大多数 `fork()+exec()` 对根本不写父进程的页——`execve()` 会立即替换地址空间。CoW 把复制开销推迟到真正需要的那一刻。
- **实践中 `TASK_UNINTERRUPTIBLE` 意味着什么？** 进程阻塞在内核代码路径（通常是硬件 I/O 操作）中，无法被安全中断。处于该状态的进程用 SIGKILL 也杀不掉——只有当内核 I/O 路径完成（或超时）后，进程才变得可终止。持续处于 `D` 状态意味着硬件挂死。
- **`clone()` 与 `fork()`、`pthread_create()` 有何关系？** `fork()` 和 `pthread_create()` 都是基于 `clone()` 系统调用实现的。`fork()` 调用 `clone()` 时不带任何共享标志（新的 `mm`、新的 `files`）。`pthread_create()` 调用 `clone()` 时带 `CLONE_VM | CLONE_FILES | CLONE_SIGHAND`（共享地址空间、共享文件描述符、共享信号处理函数）。
- **什么是 CPU 亲和性，它对推理为何重要？** `task_struct.cpus_mask` 是一个位掩码，指明任务允许在哪些 CPU 上运行。把 `modeld` 绑定到 big cluster（例如 Orin 上的 Cortex-A78AE），可防止调度器在推理中途把它迁移到 LITTLE 核。迁移会导致缓存失效与流水线停顿；亲和性消除了这种不确定性。

---

## AI 硬件关联

- `task_struct.sched_class` 决定每个任务使用哪个调度器；把 `modeld` 赋为 `SCHED_FIFO` 可将其切换为 `rt_sched_class`，避免 CFS/EEVDF 抖动在未调优系统上把帧处理延迟多达 5 ms。
- cgroup v2 中的 `cpuset.cpus` 把推理进程固定到隔离的核上，防止迁移到与中断处理程序共享的核；在 Jetson Orin 上，big cluster 的 Cortex-A78AE 核通常保留给 `modeld` 和 `camerad`。
- `cpu.max` 把后台进程（遥测、日志）限流到固定配额，使推理线程保有突发 CPU 余量——在 Kubernetes 中可直接写入 `/sys/fs/cgroup/[pod]/cpu.max`。
- openpilot 的 `camerad`、`modeld`、`sensord`、`plannerd` 和 `controlsd` 作为独立进程运行；CoW fork 语义给每个进程独立的 `mm_struct`，实现崩溃隔离而不破坏兄弟进程的地址空间。
- `TASK_UNINTERRUPTIBLE` 出现在摄像头与 DMA 驱动代码路径中——`/proc/[pid]/wchan` 中一个持续处于 `D` 状态的进程指向 `v4l2_dqbuf` 或 `nvdla_submit`，即可立即定位挂死的硬件接口。
- Kubernetes 推理 pod 中的 PID namespace 隔离服务进程树；宿主机上的 `/proc/[pid]/cgroup` 可把任意 guest PID 映射到其 pod 的资源记账组，用于 OOM 排查。


<details>
<summary>English original</summary>

**Summary**

| State | Macro | Wakeable? | Example cause |
|---|---|---|---|
| Running / runnable | `TASK_RUNNING` | — | On CPU or waiting on run queue |
| Interruptible sleep | `TASK_INTERRUPTIBLE` | Yes (signal) | Blocked on `read()`, `epoll_wait()` |
| Uninterruptible sleep | `TASK_UNINTERRUPTIBLE` | No | DMA wait, `VIDIOC_DQBUF` in driver |
| Killable | `TASK_KILLABLE` | SIGKILL only | NFS soft mount wait |
| Stopped | `__TASK_STOPPED` | SIGCONT | Debugger, SIGSTOP |
| Zombie | `EXIT_ZOMBIE` | — | Awaiting parent `waitpid()` |

**Conceptual Review**

- **Why does Linux use a single `task_struct` for both processes and threads?** Simplicity and consistency. The scheduler, OOM killer, cgroup accounting, and CPU affinity mechanisms all operate on `task_struct` without needing special cases for threads. The distinction between process and thread is entirely in which fields are shared (`mm`, `files`) via the `clone()` flags.
- **What is a zombie process and why does it exist?** A zombie is a process that has called `exit()` but whose parent has not yet called `wait()`. The kernel keeps a minimal `task_struct` so the parent can retrieve the child's exit status. Zombies consume a PID slot but no CPU or memory. They accumulate when a parent fails to reap its children.
- **Why does `fork()` use Copy-on-Write instead of immediately copying memory?** Copying the entire address space at `fork()` time would be prohibitively slow for large processes. Most `fork()+exec()` pairs never write to the parent's pages at all — `execve()` replaces the address space immediately. CoW defers the copy cost to the moment it is actually needed.
- **What does `TASK_UNINTERRUPTIBLE` mean in practice?** The process is blocked inside a kernel code path (typically a hardware I/O operation) that cannot be safely interrupted. You cannot kill a process in this state with SIGKILL — only when the kernel I/O path completes (or times out) will the process become killable. Persistent `D` state means the hardware is hung.
- **How does `clone()` relate to `fork()` and `pthread_create()`?** Both `fork()` and `pthread_create()` are implemented in terms of the `clone()` syscall. `fork()` calls `clone()` with no sharing flags (new `mm`, new `files`). `pthread_create()` calls `clone()` with `CLONE_VM | CLONE_FILES | CLONE_SIGHAND` (shared address space, shared file descriptors, shared signal handlers).
- **What is CPU affinity and why does it matter for inference?** `task_struct.cpus_mask` is a bitmask of CPUs the task is allowed to run on. Pinning `modeld` to the big cluster (e.g., Cortex-A78AE on Orin) prevents the scheduler from migrating it to a LITTLE core mid-inference. Migration causes cache invalidation and pipeline stalls; affinity eliminates this variability.

---

**AI Hardware Connection**

- `task_struct.sched_class` determines the scheduler for each task; assigning `modeld` to `SCHED_FIFO` switches it to `rt_sched_class`, preventing CFS/EEVDF jitter from delaying frame processing by up to 5 ms on an untuned system.
- `cpuset.cpus` in cgroup v2 pins inference processes to isolated cores, preventing migration to cores shared with interrupt handlers; on Jetson Orin, the big-cluster Cortex-A78AE cores are typically reserved for `modeld` and `camerad`.
- `cpu.max` throttles background processes (telemetry, logging) to a fixed quota so inference threads retain burst CPU headroom — directly writable at `/sys/fs/cgroup/[pod]/cpu.max` in Kubernetes.
- openpilot's `camerad`, `modeld`, `sensord`, `plannerd`, and `controlsd` run as separate processes; CoW fork semantics give each an independent `mm_struct`, enabling crash isolation without corrupting sibling address spaces.
- `TASK_UNINTERRUPTIBLE` appears in camera and DMA driver code paths — a persistent `D`-state process in `/proc/[pid]/wchan` pointing to `v4l2_dqbuf` or `nvdla_submit` immediately identifies the stalled hardware interface.
- PID namespaces in Kubernetes inference pods isolate service process trees; `/proc/[pid]/cgroup` on the host maps any guest PID to its pod's resource accounting group for OOM investigation.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
