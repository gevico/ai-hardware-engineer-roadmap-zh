---
title: RT demo — 用户态实验（讲座 1–9）
description: RT demo — 用户态实验（讲座 1–9）
---

# RT demo — 用户态实验（讲座 1–9）

小型 **Linux** 程序，演示前九个操作系统讲座中的思想：进程与线程、系统调用、调度（`SCHED_FIFO`）、CPU 亲和性、内存锁定 / 预缺页，以及 pthread 同步。

内核侧主题（中断、启动、模块、设备树）仅在注释或下文描述——此处不实现。

## 构建与运行

在本目录下：

```bash
cd "Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/demos/rt-demo"
make
./rt_demo [options]
```

不带 `make` 时：`gcc -Wall -O2 -pthread -o rt_demo rt_demo.c`

**选项：** `./rt_demo --help` — 例如 `--cpu 2,3`、`--rt`（需要 root / `cap_sys_nice`）、`--lock-memory`（需要 root / `cap_ipc_lock`）、`--no-rt`。

**跟踪系统调用：** `strace -e sched_setscheduler,sched_setaffinity,mlockall,gettid,clock_gettime ./rt_demo --rt --cpu 1 2>&1 | head -50`

## 讲座对应关系

| 讲座 | 主题 | 本 demo 中 |
|--------|--------|----------------|
| **L1** | OS 作为资源管理器；用户态 vs 内核态 | 运行在用户空间；用系统调用请求内核服务 |
| **L2** | 进程 / 线程 | `main()` + worker 线程；`getpid()`、`gettid()`、亲和性 |
| **L3** | 中断、上半部/下半部 | 仅事件式唤醒；真正的 IRQ 属于内核/驱动 |
| **L4** | 系统调用 | `sched_setscheduler`、`sched_setaffinity`、`mlockall`、`clock_gettime`、…… |
| **L5** | 启动、模块、设备树 | 不在代码中；见下文 **真实机器上的 L5** |
| **L6** | 调度 | `sched_setscheduler(SCHED_FIFO)` |
| **L7** | RT Linux、延迟 | `mlockall()`、预缺页、热循环中不分配 |
| **L8** | 亲和性与隔离 | `sched_setaffinity()` |
| **L9** | 同步 | `pthread_mutex_t`、`pthread_rwlock_t`、`pthread_cond_t` |

### 真实机器上的 L5

- **启动：** 启动时 `dmesg`；`systemd-analyze`
- **模块：** `lsmod`、`modinfo`、`modprobe`（例如 `loop`）
- **设备树（ARM）：** `/sys/firmware/devicetree/base/` 或启动日志中的 machine model

## 环境要求

Linux 或 WSL，带 pthreads 和 `sched.h`。`--rt` / `--lock-memory` 通常需要 root 或相应的 capabilities。

## 文件

| 文件 | 用途 |
|------|---------|
| `rt_demo.c` | 源代码 |
| `Makefile` | 构建 |
| `README.md` | 本文件 |

返回章节主页：**[Operating Systems Guide](../../../Guide.md)**。


<details>
<summary>English original</summary>

**RT demo — user-space lab (Lectures 1–9)**

Small **Linux** program that illustrates ideas from the first nine OS lectures: processes and threads, syscalls, scheduling (`SCHED_FIFO`), CPU affinity, memory locking / pre-faulting, and pthread synchronization.

Kernel-side topics (interrupts, boot, modules, device tree) are only described in comments or below—not implemented here.

**Build and run**

From this directory:

```bash
cd "Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/demos/rt-demo"
make
./rt_demo [options]
```

Without `make`: `gcc -Wall -O2 -pthread -o rt_demo rt_demo.c`

**Options:** `./rt_demo --help` — e.g. `--cpu 2,3`, `--rt` (needs root / `cap_sys_nice`), `--lock-memory` (needs root / `cap_ipc_lock`), `--no-rt`.

**Trace syscalls:** `strace -e sched_setscheduler,sched_setaffinity,mlockall,gettid,clock_gettime ./rt_demo --rt --cpu 1 2>&1 | head -50`

**Lecture mapping**

| Lecture | Topic | In this demo |
|--------|--------|----------------|
| **L1** | OS as resource manager; user vs kernel | Runs in user space; uses syscalls to request kernel services |
| **L2** | Process / threads | `main()` + worker thread; `getpid()`, `gettid()`, affinity |
| **L3** | Interrupts, top/bottom half | Event-style wakeup only; real IRQs are kernel/driver |
| **L4** | System calls | `sched_setscheduler`, `sched_setaffinity`, `mlockall`, `clock_gettime`, … |
| **L5** | Boot, modules, device tree | Not in code; see **L5 on a real machine** below |
| **L6** | Scheduling | `sched_setscheduler(SCHED_FIFO)` |
| **L7** | RT Linux, latency | `mlockall()`, pre-faulting, no alloc in hot loop |
| **L8** | Affinity & isolation | `sched_setaffinity()` |
| **L9** | Synchronization | `pthread_mutex_t`, `pthread_rwlock_t`, `pthread_cond_t` |

**L5 on a real machine**

- **Boot:** `dmesg` at boot; `systemd-analyze`
- **Modules:** `lsmod`, `modinfo`, `modprobe` (e.g. `loop`)
- **Device tree (ARM):** `/sys/firmware/devicetree/base/` or machine model in boot logs

**Requirements**

Linux or WSL with pthreads and `sched.h`. `--rt` / `--lock-memory` usually require root or the matching capabilities.

**Files**

| File | Purpose |
|------|---------|
| `rt_demo.c` | Source |
| `Makefile` | Build |
| `README.md` | This file |

Back to the section hub: **[Operating Systems Guide](../../../Guide.md)**.
</think>


<｜tool▁calls▁begin｜><｜tool▁call▁begin｜>
StrReplace

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/demos/rt-demo/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/demos/rt-demo/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
