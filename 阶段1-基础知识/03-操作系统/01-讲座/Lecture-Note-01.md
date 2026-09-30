---
title: 讲义 Note 01（L1、L2）：OS 架构与 Linux 内核；进程、taskstruct 与 Linux 进程模型
description: 讲义 Note 01（L1、L2）：OS 架构与 Linux 内核；进程、taskstruct 与 Linux 进程模型
---

# 讲义 Note 01（L1、L2）：OS 架构与 Linux 内核；进程、task_struct 与 Linux 进程模型

**涵盖：** 讲义 L1（现代 OS 架构与 Linux 内核）与讲义 L2（进程、task_struct 与 Linux 进程模型）。

---

## 本讲义的组织方式

1. **第 1 部分（L1）——OS 与内核：** OS 角色；特权级（x86 Ring、ARM EL0–EL3）；宏内核；版本管理；/proc、/sys；内核模块；源码目录结构；AI 平台。
2. **第 2 部分（L2）——进程与 task_struct：** 进程抽象；task_struct 及关键字段；进程状态；fork/exec/wait 与 COW；clone() 与线程；命名空间；cgroups v2；上下文切换；/proc/[pid] 查看。

---

# 第 1 部分（L1）：现代 OS 架构与 Linux 内核

**背景：** OS 是**可信裁决者**：它拥有硬件、实施保护并提供抽象。用户代码运行在 **Ring 3 / EL0**；内核运行在 **Ring 0 / EL1**。在 Jetson 上，TensorRT 运行于 EL0，只能经由内核访问硬件。

---

## OS：三种角色

| 角色 | 含义 |
|------|--------|
| 资源管理器 | 跨进程分配 CPU 时间、内存、I/O、网络 |
| 抽象层 | 在硬件之上提供统一接口（文件、socket、VM） |
| 保护边界 | 隔离进程与内核；由硬件强制实施 |

---

## 特权级

**x86：** Ring 0 = 内核（可执行全部指令）；Ring 3 = 用户态（执行特权指令 → fault）。切换：SYSCALL → Ring 0；SYSRET → Ring 3。

**ARM64（AArch64）：** EL0 = 用户态（TensorRT、ROS2）；EL1 = 内核（MMU、驱动）；EL2 = hypervisor（KVM）；EL3 = secure monitor（TrustZone、PSCI）。Jetson：内核位于 EL1；NVIDIA 固件位于 EL3。

---

## Linux 内核：宏内核 + 模块

**宏内核：** 核心与驱动同处 Ring 0/EL1 的单一地址空间；内核内调用快；**驱动崩溃可能使系统 panic**。对比：**微内核**（QNX）将驱动放在独立进程中运行。**可加载模块**（`.ko`）可在 runtime 添加驱动，无需重新构建内核。

**子系统：** kernel/（调度器、信号）、mm/（内存）、drivers/（GPU、V4L2、NVMe、PCIe）、fs/（VFS）、net/、arch/。**版本管理：** mainline → stable → LTS（2–6 年）。AI 平台为求 BSP 与驱动稳定而固定到 LTS（例如 Jetson 5.10/6.1、Yocto 5.15/6.6）。

---

## /proc 与 /sys

**/proc：** 虚拟文件系统；读取会调用内核代码。示例：cpuinfo、meminfo、interrupts、cmdline；按进程：maps、status、fd、wchan。**/sys（sysfs）：** 设备/总线层次；class（net、thermal、gpio）、cgroup、firmware/devicetree。在 Jetson 上，热管理区与 GPU 状态位于 /sys。

---

## 内核模块

`insmod`/`rmmod`/`modprobe`；`lsmod`、`modinfo`。模块 init/exit；`MODULE_DEVICE_TABLE(of, ...)` 用于在设备树匹配时由 udev 自动加载。需针对运行中的内核头文件编译；树外模块（如 NVIDIA、FPGA）使用 DKMS。

---

# 第 2 部分（L2）：进程、task_struct 与 Linux 进程模型

**背景：** 每个可运行实体都是一个 `task_struct`。**线程就是任务**，它们共享 `mm_struct` 与 `files_struct`。调度类、亲和性、cgroups 都位于 task_struct 中；**调优推理 = 调优这些字段**。

---

## 进程抽象

进程 = 执行中的程序：**虚拟 CPU**（寄存器位于 task_struct）、**虚拟内存**（mm_struct）、**资源**（文件、信号、cgroups）。线程 = 共享 mm 与文件的 task；除 clone 标志之外，内核不区分“线程”与“进程”。

---

## task_struct 关键字段

`pid`（thread ID，gettid()）、`tgid`（process ID，getpid()）、`state`、`mm`、`files`、`sched_class`、`se`（CFS/EEVDF）、`rt`（RT）、`dl`（DEADLINE）、`cgroups`、`cpus_mask`。调度器与亲和性作用于该结构。

---

## 进程状态

TASK_RUNNING（R）、TASK_INTERRUPTIBLE（S）、TASK_UNINTERRUPTIBLE（D —— 例如 DMA、VIDIOC_DQBUF）、TASK_KILLABLE、STOPPED（T）、EXIT_ZOMBIE（Z）。**持续处于 D = 驱动/硬件挂起**。**僵尸进程** = 父进程尚未调用 wait()。

---

## fork / exec / wait；COW

`fork()` 创建子进程；物理页并不复制 —— **写时复制（Copy-on-Write）**：页在首次写入前以只读方式共享，写入时才复制。`execve()` 替换地址空间。`waitpid()` 回收僵尸进程。CoW 使 fork() 相对地址空间大小为 O(1)；openpilot 的多进程架构依赖于此。

---

## clone() 与线程

`clone(CLONE_VM | CLONE_FILES | CLONE_SIGHAND)` = **线程**（共享 mm、文件）。`getpid()` = tgid；`gettid()` = 每线程的 pid。**调度器对一切任务一视同仁**；可按任务设置亲和性与调度类。

---

## 命名空间与 cgroups v2

**命名空间：** pid、mnt、net、uts、ipc、user、cgroup、time —— 隔离资源的可见视图；容器的基石。**cgroups v2**（统一挂载于 /sys/fs/cgroup/）：cpu.max、cpuset.cpus/mems、memory.max、io.max、pids.max。Kubernetes 用它们限制 pod；cpu.stat 的 throttled_usec 表示 CPU 被限流。

---

## 上下文切换

`switch_mm`（装入新页表，CR3/TTBR0）；`switch_to`（保存/恢复寄存器）。**ASID/PCID 可避免全量 TLB flush**。开销 **~1–10 µs**。/proc/[pid]/sched、wchan、cgroup、oom_score 可供查看。

---


<details>
<summary>English original</summary>

**Lecture Note 01 (L1, L2): OS Architecture & the Linux Kernel; Processes, task_struct & the Linux Process Model**

**Combines:** Lecture L1 (Modern OS Architecture & the Linux Kernel) and Lecture L2 (Processes, task_struct & the Linux Process Model).

---

**How This Note Is Organized**

1. **Part 1 (L1) — OS & kernel:** OS roles; privilege levels (x86 Rings, ARM EL0–EL3); monolithic kernel; versioning; /proc, /sys; kernel modules; source layout; AI platforms.
2. **Part 2 (L2) — Processes & task_struct:** Process abstraction; task_struct and key fields; process states; fork/exec/wait and COW; clone() and threads; namespaces; cgroups v2; context switch; /proc/[pid] inspection.

---

**Part 1 (L1): Modern OS Architecture & the Linux Kernel**

**Context:** The OS is the **trusted referee**: it owns hardware, enforces protection, and provides abstractions. User code runs at **Ring 3 / EL0**; kernel at **Ring 0 / EL1**. On Jetson, TensorRT runs at EL0 and reaches hardware only via the kernel.

---

**OS: Three Roles**

| Role | Meaning |
|------|--------|
| Resource manager | CPU time, memory, I/O, network across processes |
| Abstraction layer | Uniform interfaces (files, sockets, VM) over hardware |
| Protection boundary | Isolates processes and kernel; enforced in hardware |

---

**Privilege Levels**

**x86:** Ring 0 = kernel (all instructions); Ring 3 = user (privileged instruction → fault). Switch: SYSCALL → Ring 0; SYSRET → Ring 3.

**ARM64 (AArch64):** EL0 = user (TensorRT, ROS2); EL1 = kernel (MMU, drivers); EL2 = hypervisor (KVM); EL3 = secure monitor (TrustZone, PSCI). Jetson: kernel at EL1; NVIDIA firmware at EL3.

---

**Linux Kernel: Monolithic + Modules**

**Monolithic:** core and drivers in one address space at Ring 0/EL1; fast in-kernel calls; a **crashing driver can panic the system**. Contrast: **microkernels** (QNX) run drivers in separate processes. **Loadable modules** (`.ko`) add drivers at runtime without rebuilding the kernel.

**Subsystems:** kernel/ (scheduler, signals), mm/ (memory), drivers/ (GPU, V4L2, NVMe, PCIe), fs/ (VFS), net/, arch/. **Versioning:** mainline → stable → LTS (2–6 years). AI platforms pin to LTS (e.g. Jetson 5.10/6.1, Yocto 5.15/6.6) for BSP and driver stability.

---

**/proc and /sys**

**/proc:** Virtual filesystem; reads invoke kernel code. Examples: cpuinfo, meminfo, interrupts, cmdline; per-process: maps, status, fd, wchan. **/sys (sysfs):** Device/bus hierarchy; class (net, thermal, gpio), cgroup, firmware/devicetree. On Jetson, thermal zones and GPU state are in /sys.

---

**Kernel Modules**

`insmod`/`rmmod`/`modprobe`; `lsmod`, `modinfo`. Module init/exit; `MODULE_DEVICE_TABLE(of, ...)` for udev auto-load on Device Tree match. Compile against running kernel headers; DKMS for out-of-tree (e.g. NVIDIA, FPGA).

---

**Part 2 (L2): Processes, task_struct & the Linux Process Model**

**Context:** Every runnable entity is a `task_struct`. **Threads are tasks** that share `mm_struct` and `files_struct`. Scheduler class, affinity, cgroups live in task_struct; **tuning inference = tuning these fields**.

---

**Process Abstraction**

Process = program in execution: **virtual CPU** (registers in task_struct), **virtual memory** (mm_struct), **resources** (files, signals, cgroups). Thread = task with shared mm and files; kernel does not distinguish “thread” vs “process” beyond clone flags.

---

**task_struct Key Fields**

`pid` (thread ID, gettid()), `tgid` (process ID, getpid()), `state`, `mm`, `files`, `sched_class`, `se` (CFS/EEVDF), `rt` (RT), `dl` (DEADLINE), `cgroups`, `cpus_mask`. Scheduler and affinity act on this structure.

---

**Process States**

TASK_RUNNING (R), TASK_INTERRUPTIBLE (S), TASK_UNINTERRUPTIBLE (D — e.g. DMA, VIDIOC_DQBUF), TASK_KILLABLE, STOPPED (T), EXIT_ZOMBIE (Z). **Persistent D = driver/hardware hang**. **Zombie** = parent has not called wait().

---

**fork / exec / wait; COW**

`fork()` creates child; physical pages not copied — **Copy-on-Write**: pages shared read-only until first write, then copy. `execve()` replaces address space. `waitpid()` reaps zombie. CoW makes fork() O(1) for address space size; openpilot multi-process relies on it.

---

**clone() and Threads**

`clone(CLONE_VM | CLONE_FILES | CLONE_SIGHAND)` = **thread** (shared mm, files). `getpid()` = tgid; `gettid()` = per-thread pid. **Scheduler treats all tasks alike**; per-task affinity and scheduling class.

---

**Namespaces and cgroups v2**

**Namespaces:** pid, mnt, net, uts, ipc, user, cgroup, time — isolate view of resources; basis of containers. **cgroups v2** (unified at /sys/fs/cgroup/): cpu.max, cpuset.cpus/mems, memory.max, io.max, pids.max. Kubernetes uses them for pod limits; cpu.stat throttled_usec indicates CPU throttling.

---

**Context Switch**

`switch_mm` (install new page table, CR3/TTBR0); `switch_to` (save/restore registers). **ASID/PCID avoid full TLB flush**. Cost **~1–10 µs**. /proc/[pid]/sched, wchan, cgroup, oom_score for inspection.

---

</details>

## 概要

| L1 | OS 角色；Rings/EL；宏内核；/proc、/sys；模块；LTS。 |
| L2 | task_struct；状态；fork/exec/wait；COW；clone/线程；命名空间；cgroups；上下文切换。 |

---

## 与 AI 硬件的关联

- L1：L4T/AGNOS/Yocto 锁定内核以保证驱动和 ABI 稳定；/proc/interrupts 和 /sys/thermal 用于诊断；宏内核设计意味着驱动崩溃可能导致系统 panic —— IOMMU 与测试至关重要。
- L2：用 SCHED_FIFO 和 cpus_mask 服务 modeld；用 cgroup cpu.max 和 throttled_usec 处理 pod 限流；用 CoW 支持多进程；V4L2/NVMe 路径中的 TASK_UNINTERRUPTIBLE；用 oom_score_adj 保护推理免受 OOM 影响。

---

*合并自讲座 L1、L2（OS 架构与 Linux 内核；进程、task_struct 与进程模型）。*


<details>
<summary>English original</summary>

**Summary**

| L1 | OS roles; Rings/EL; monolithic kernel; /proc, /sys; modules; LTS. |
| L2 | task_struct; states; fork/exec/wait; COW; clone/threads; namespaces; cgroups; context switch. |

---

**AI Hardware Connection**

- L1: L4T/AGNOS/Yocto pin kernel for driver and ABI stability; /proc/interrupts and /sys/thermal for diagnostics; monolithic design means driver crash can panic system — IOMMU and testing matter.
- L2: SCHED_FIFO and cpus_mask for modeld; cgroup cpu.max and throttled_usec for pod throttling; CoW for multi-process; TASK_UNINTERRUPTIBLE in V4L2/NVMe paths; oom_score_adj to protect inference from OOM.

---

*Combines Lectures L1, L2 (OS Architecture & Linux Kernel; Processes, task_struct & Process Model).*

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-Note-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-Note-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
