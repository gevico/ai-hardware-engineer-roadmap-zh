---
title: 操作系统（阶段 1 §3）
description: 操作系统（阶段 1 §3）
---

# 操作系统（阶段 1 §3）

<div class="course-identity operating-systems" markdown="1">
<div class="course-identity__icon">OS</div>
<div markdown="1">
<p class="course-identity__eyebrow">模块 3 · 操作系统</p>
<p class="course-identity__title">学习调度进程、内存、文件、驱动和设备的 runtime 层。</p>
<p class="course-identity__meta">产物：OS 行为实验 · 度量：syscall、上下文切换、内存、争用</p>
</div>

</div>


**主要来源：** [Caltech CS124 Spring 2024](https://users.cms.caltech.edu/~donnie/cs124/lectures/)（Donnie Pinkston）。

本节是阶段 1 的 **OS 理论 + Linux 形态实践**层。它位于 [**§2 — 计算机体系结构**](../02-计算机体系结构与硬件/Guide.md)（你需要 CPU/内存的上下文）之后，[**§4 — C++ 与并行计算**](../04-C++与并行计算/Guide.md)（编写 host 和 CUDA 代码时会用到进程、线程和 VM 的相关概念）之前。

**为什么这对 AI 硬件很重要：** 部署的软件栈运行在 Linux（Jetson、ADAS、服务器）或更小的 RTOS 上。调度、虚拟内存、I/O 以及用户/kernel 边界，正是你在追延迟、调试驱动或推演 zero-copy 的 camera → GPU 路径时所调优的对象。

---

## 本文件夹的组织方式

你需要的一切都**在本目录下**——`Lectures/` 旁边没有单独的“side”目录树。

| 位置 | 作用 |
|----------|------|
| **`Lectures/Lecture-NN.md`** | **26** 个主题的主笔记（见下方索引）。 |
| **`Lectures/Lecture-Note-NN.md`** | 有则提供更深入的可选笔记。 |
| **`Lectures/demos/rt-demo/`** | 把 **Lectures 1–9** 与可运行代码关联起来的小型**用户态** C demo（[README](01-讲座/01-演示/01-rt-demo/README.md)）。 |
| **`Final-Test-Problems.md`** | 五道集成题，带有明确的讲座交叉链接。 |

**学习流程：** 按下面的主题顺序阅读讲座 → 完成 **L9** 后使用 **rt-demo**（可选但有用）→ 在本节接近末尾时尝试 **Final-Test-Problems**。

---

## 课程概览

| 方面 | 详情 |
|--------|---------|
| **基础课程** | Caltech CS124 —— 概念、幻灯片与节奏 |
| **本仓库中的讲义** | **26** 篇 markdown 讲座（包括与本路线图对齐的 capstone 式 **L25** 和 **L26** 扩展） |
| **前置要求** | C 编程；阶段 1 §2（体系结构 / 存储层次） |
| **经典动手实践（外部）** | [Pintos](https://web.stanford.edu/class/cs140/projects/pintos/) —— Stanford 教学 kernel（线程、用户程序、VM、文件） |

---

## 推荐顺序（按主题）

按此顺序进行，不要跳来跳去；后面的讲座会假定你已掌握前面讲座的术语。

1. **系统形态与启动（L1–L5）** —— 历史、UNIX I/O、trap 与结构、微内核、固件/启动。
2. **kernel 模型中的并发（L6–L9）** —— 进程、线程、中断上下文、同步与死锁。  
   *可选：* 在此构建并运行 **[rt-demo](01-讲座/01-演示/01-rt-demo/README.md)**，把 syscall、调度、亲和性与 pthread 原语同真实代码连起来。
3. **调度（L10–L12）** —— 高级同步、CPU 调度、实时与 Linux 调度器。
4. **用户/kernel 边界（L13–L14）** —— 系统调用、信号。
5. **内存（L15–L20）** —— 虚拟内存、页表、替换、抖动、Pintos VM 讨论。
6. **文件系统（L21–L24）** —— 分配、加锁、SSD、Pintos FS 设计、日志。
7. **集成（L25–L26）** —— Yocto capstone 衔接；eBPF / 可观测性。

---

<a id="lecture-index"></a>


<details>
<summary>English original</summary>

**Operating Systems (Phase 1 §3)**

<div class="course-identity operating-systems" markdown="1">
<div class="course-identity__icon">OS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Module 3 · Operating Systems</p>
<p class="course-identity__title">Learn the runtime layer that schedules processes, memory, files, drivers, and devices.</p>
<p class="course-identity__meta">Artifact: OS behavior lab · Measure: syscalls, context switches, memory, contention</p>
</div>
</div>


**Primary source:** [Caltech CS124 Spring 2024](https://users.cms.caltech.edu/~donnie/cs124/lectures/) (Donnie Pinkston).

This section is the **OS theory + Linux-shaped practice** layer of Phase 1. It sits after [**§2 — Computer Architecture**](../02-计算机体系结构与硬件/Guide.md) (you need CPU/memory context) and before [**§4 — C++ and Parallel Computing**](../04-C++与并行计算/Guide.md) (you will use processes, threads, and VM ideas when writing host and CUDA code).

**Why it matters for AI hardware:** Deployed stacks run on Linux (Jetson, ADAS, servers) or smaller RTOSes. Scheduling, virtual memory, I/O, and the user/kernel boundary are what you tune when chasing latency, debugging drivers, or reasoning about zero-copy camera → GPU paths.

---

**How this folder is laid out**

Everything you need is **under this directory**—there is no separate “side” tree next to `Lectures/`.

| Location | Role |
|----------|------|
| **`Lectures/Lecture-NN.md`** | Main notes for **26** topics (see index below). |
| **`Lectures/Lecture-Note-NN.md`** | Optional deeper notes where present. |
| **`Lectures/demos/rt-demo/`** | Small **user-space** C demo tying **Lectures 1–9** to runnable code ([README](01-讲座/01-演示/01-rt-demo/README.md)). |
| **`Final-Test-Problems.md`** | Five integration problems with explicit lecture cross-links. |

**Study flow:** Read lectures in the thematic order below → use **rt-demo** after you finish **L9** (optional but useful) → attempt **Final-Test-Problems** near the end of the section.

---

**Course snapshot**

| Aspect | Details |
|--------|---------|
| **Base curriculum** | Caltech CS124 — concepts, slides, and pacing |
| **Write-ups in this repo** | **26** markdown lectures (including capstone-style **L25** and **L26** extensions aligned with this roadmap) |
| **Prerequisites** | C programming; Phase 1 §2 (architecture / memory hierarchy) |
| **Classic hands-on (external)** | [Pintos](https://web.stanford.edu/class/cs140/projects/pintos/) — Stanford teaching kernel (threads, user programs, VM, files) |

---

**Recommended order (by theme)**

Follow this order rather than skipping around; later lectures assume earlier vocabulary.

1. **System shape & boot (L1–L5)** — history, UNIX I/O, traps and structure, microkernels, firmware/boot.
2. **Concurrency in the kernel model (L6–L9)** — processes, threads, interrupt context, synchronization and deadlock.  
   *Optional:* build and run **[rt-demo](01-讲座/01-演示/01-rt-demo/README.md)** here to connect syscalls, scheduling, affinity, and pthread primitives to real code.
3. **Scheduling (L10–L12)** — advanced sync, CPU scheduling, real-time and Linux schedulers.
4. **User/kernel boundary (L13–L14)** — system calls, signals.
5. **Memory (L15–L20)** — virtual memory, page tables, replacement, thrashing, Pintos VM discussion.
6. **Filesystems (L21–L24)** — allocation, locking, SSDs, Pintos FS design, journaling.
7. **Integration (L25–L26)** — Yocto capstone tie-in; eBPF / observability.

---

<a id="lecture-index"></a>

</details>

## 讲座索引（全部主题）

| # | 主题 | 关键概念 |
|:-:|-------|----------------|
| [1](01-讲座/Lecture-01.md) | 引言与 OS 历史 | 大型机 → 分时、虚拟化、RTOS |
| [2](01-讲座/Lecture-02.md) | OS 组件与 UNIX I/O | 系统调用、内核/用户模式、fd、管道、shell |
| [3](01-讲座/Lecture-03.md) | 陷阱、中断与结构 | 陷阱/故障、抢占、宏内核 vs 微内核 |
| [4](01-讲座/Lecture-04.md) | 微内核与外核 | Mach、L4、IPC、混合内核 |
| [5](01-讲座/Lecture-05.md) | 引导与固件 | BIOS/UEFI、ACPI、链式加载；**Linux 内核讲座**与 DT、模块的衔接 |
| [6](01-讲座/Lecture-06.md) | 进程抽象 | 状态、PCB、上下文切换、队列 |
| [7](01-讲座/Lecture-07.md) | 线程 | 用户线程 vs 内核线程、模型、Amdahl |
| [8](01-讲座/Lecture-08.md) | 内核栈与中断 | 可重入、中断上下文、临界区 |
| [9](01-讲座/Lecture-09.md) | 同步与死锁 | Peterson、自旋锁、信号量、死锁 |
| [10](01-讲座/Lecture-10.md) | 高级同步 | RCU、读写锁、锁粒度 |
| [11](01-讲座/Lecture-11.md) | 进程调度 | FCFS、RR、SJF、优先级、MLQ |
| [12](01-讲座/Lecture-12.md) | 实时与 Linux 调度器 | EDF、速率单调、CFS |
| [13](01-讲座/Lecture-13.md) | 系统调用 | 陷阱路径、参数、指针检查 |
| [14](01-讲座/Lecture-14.md) | UNIX 信号 | 处理函数、掩码、sigreturn |
| [15](01-讲座/Lecture-15.md) | 虚拟内存与 MMU | 分页、TLB、按需分页 |
| [16](01-讲座/Lecture-16.md) | 页表与 COW | PTE 位、fork/vfork |
| [17](01-讲座/Lecture-17.md) | 帧表与替换 | FIFO、最优、Belady |
| [18](01-讲座/Lecture-18.md) | 替换策略 | LRU、时钟、工作集 |
| [19](01-讲座/Lecture-19.md) | Pintos VM 设计 | 面向项目的讨论 |
| [20](01-讲座/Lecture-20.md) | 分配与抖动 | 全局 vs 局部、工作集 |
| [21](01-讲座/Lecture-21.md) | 文件系统 | 目录、inode、分配 |
| [22](01-讲座/Lecture-22.md) | 文件锁与 SSD | flock、FTL、TRIM |
| [23](01-讲座/Lecture-23.md) | Pintos 文件系统设计 | 面向项目的讨论 |
| [24](01-讲座/Lecture-24.md) | 日志文件系统 | 崩溃一致性 |
| [25](01-讲座/Lecture-25.md) | 综合项目：定制 Linux 镜像 | Yocto；与阶段 2 嵌入式 Linux 配套 |
| [26](01-讲座/Lecture-26.md) | eBPF 深入剖析 | 验证器、map、CO-RE、XDP、可观测性 |

---

## 练习

1. **[Final-Test-Problems.md](Final-Test-Problems.md)** — 五道题（内核构建、PREEMPT_RT、ext4、启动链、模块 + DT）；每道题都指向具体的讲座。
2. **[Lectures/demos/rt-demo/](01-讲座/01-演示/01-rt-demo/README.md)** — 一个面向 **L1–L9** 的程序（调度、亲和性、`mlockall`、pthread 同步）。需要 Linux 或 WSL。

---

## 资源

- **幻灯片：** [CS124 讲座（PDF）](https://users.cms.caltech.edu/~donnie/cs124/lectures/)
- **教材：** *Operating System Concepts*（Silberschatz, Galvin, Gagne）
- **内核叙述：** [Linux Insides](https://0xax.gitbooks.io/linux-insides/)
- **真实分支（openpilot / comma）：** [agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845)、[agnos-builder](https://github.com/commaai/agnos-builder) — [**AGNOS Guide**](../../阶段5-高级专题与专精/05-方向E-自动驾驶/02-openpilot参考栈/01-agnos/Guide.md) 把**全部 26 个**讲座主题映射到内核路径与分支特有改动。

---

## AI / 嵌入式相关性

| OS 概念 | 典型 AI / 边缘用途 |
|---------|------------------------|
| **实时调度** | 推理截止时间、传感器流水线 |
| **虚拟内存与 DMA** | 权重放在 RAM，摄像头 → 加速器缓冲区 |
| **fd 与 I/O** | 摄像头、CAN、日志记录 |
| **进程 / 线程模型** | 采集、推理、控制各自独立的守护进程 |
| **用户 vs 内核** | 驱动 vs 用户态 ML runtime |
| **eBPF** | 性能剖析、调度器分析、XDP 过滤 |


<details>
<summary>English original</summary>

**Lecture index (all topics)**

| # | Topic | Key concepts |
|:-:|-------|----------------|
| [1](01-讲座/Lecture-01.md) | Introduction & OS history | Mainframes → time-sharing, virtualization, RTOS |
| [2](01-讲座/Lecture-02.md) | OS components & UNIX I/O | Syscalls, kernel/user mode, fds, pipes, shells |
| [3](01-讲座/Lecture-03.md) | Traps, interrupts & structure | Traps/faults, preemption, monolithic vs microkernel |
| [4](01-讲座/Lecture-04.md) | Microkernels & exokernels | Mach, L4, IPC, hybrid kernels |
| [5](01-讲座/Lecture-05.md) | Bootstrap & firmware | BIOS/UEFI, ACPI, chain loading; **Linux kernel lectures** tie-in to DT, modules |
| [6](01-讲座/Lecture-06.md) | Process abstraction | States, PCB, context switch, queues |
| [7](01-讲座/Lecture-07.md) | Threads | User vs kernel threads, models, Amdahl |
| [8](01-讲座/Lecture-08.md) | Kernel stacks & interrupts | Reentrancy, interrupt context, critical sections |
| [9](01-讲座/Lecture-09.md) | Synchronization & deadlock | Peterson, spinlocks, semaphores, deadlock |
| [10](01-讲座/Lecture-10.md) | Advanced synchronization | RCU, rwlocks, lock granularity |
| [11](01-讲座/Lecture-11.md) | Process scheduling | FCFS, RR, SJF, priority, MLQs |
| [12](01-讲座/Lecture-12.md) | Real-time & Linux schedulers | EDF, rate-monotonic, CFS |
| [13](01-讲座/Lecture-13.md) | System calls | Trap path, arguments, pointer checks |
| [14](01-讲座/Lecture-14.md) | UNIX signals | Handlers, masks, sigreturn |
| [15](01-讲座/Lecture-15.md) | Virtual memory & MMU | Paging, TLB, demand paging |
| [16](01-讲座/Lecture-16.md) | Page tables & COW | PTE bits, fork/vfork |
| [17](01-讲座/Lecture-17.md) | Frame tables & replacement | FIFO, optimal, Belady |
| [18](01-讲座/Lecture-18.md) | Replacement policies | LRU, clock, working set |
| [19](01-讲座/Lecture-19.md) | Pintos VM design | Project-oriented discussion |
| [20](01-讲座/Lecture-20.md) | Allocation & thrashing | Global vs local, working set |
| [21](01-讲座/Lecture-21.md) | Filesystems | Directories, inodes, allocation |
| [22](01-讲座/Lecture-22.md) | File locking & SSDs | flock, FTL, TRIM |
| [23](01-讲座/Lecture-23.md) | Pintos file system design | Project-oriented discussion |
| [24](01-讲座/Lecture-24.md) | Journaling filesystems | Crash consistency |
| [25](01-讲座/Lecture-25.md) | Capstone: custom Linux images | Yocto; pairs with Phase 2 embedded Linux |
| [26](01-讲座/Lecture-26.md) | eBPF deep dive | Verifier, maps, CO-RE, XDP, observability |

---

**Practice**

1. **[Final-Test-Problems.md](Final-Test-Problems.md)** — five problems (kernel build, PREEMPT_RT, ext4, boot chain, modules + DT); each points to specific lectures.
2. **[Lectures/demos/rt-demo/](01-讲座/01-演示/01-rt-demo/README.md)** — one program for **L1–L9** (scheduling, affinity, `mlockall`, pthread sync). Requires Linux or WSL.

---

**Resources**

- **Slides:** [CS124 lectures (PDF)](https://users.cms.caltech.edu/~donnie/cs124/lectures/)
- **Textbook:** *Operating System Concepts* (Silberschatz, Galvin, Gagne)
- **Kernel narrative:** [Linux Insides](https://0xax.gitbooks.io/linux-insides/)
- **Real fork (openpilot / comma):** [agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845), [agnos-builder](https://github.com/commaai/agnos-builder) — [**AGNOS Guide**](../../阶段5-高级专题与专精/05-方向E-自动驾驶/02-openpilot参考栈/01-agnos/Guide.md) maps **all 26** lecture topics to kernel paths and fork-specific changes.

---

**AI / embedded relevance**

| OS idea | Typical AI / edge use |
|---------|------------------------|
| **Real-time scheduling** | Inference deadlines, sensor pipelines |
| **Virtual memory & DMA** | Weights in RAM, camera → accelerator buffers |
| **fds & I/O** | Cameras, CAN, logging |
| **Process / thread model** | Separate daemons for capture, inference, control |
| **User vs kernel** | Drivers vs userspace ML runtimes |
| **eBPF** | Profiling, scheduler analysis, XDP filtering |

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
