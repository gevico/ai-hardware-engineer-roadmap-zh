---
title: 第 4 讲：系统调用、vDSO 与 eBPF
description: 第 4 讲：系统调用、vDSO 与 eBPF
---

# 第 4 讲：系统调用、vDSO 与 eBPF

## 概述

每当用户态代码需要内核做某件事 —— 打开文件、分配内存、发送数据包、与 GPU 通信 —— 它都要通过系统调用跨越硬件特权边界。本讲要解决的核心问题是：这种边界跨越是如何工作的，代价有多高，以及如何在不修改源码的前提下观察内核内部正在发生什么。心智模型是**收费站**：每次系统调用都是从用户态进入内核态的一次受控跨越，无论实际工作耗时 1 ns 还是 1 ms，成本都是固定的。对 AI 硬件工程师而言，这一点很重要：一条 200 fps 的摄像头流水线，光是为 V4L2 ioctl 穿过收费站就要消耗可观的 CPU 时间；而 eBPF 正是你在生产环境中测量和诊断内核内部行为、且不必改动一行应用代码的仪器。

---

## 系统调用接口

**系统调用**是用户态代码请求内核服务的唯一受许可路径。用户态**无法**直接访问硬件寄存器、分配物理内存或更改调度器策略 —— 它只能通过 **syscall ABI** 向内核请求。

### 调用路径 —— x86-64

```
Syscall Path — x86-64
┌─────────────────────────────────────────────────────────┐
│  User Space (Ring 3)                                    │
│                                                         │
│  Application code                                       │
│      │ calls read(fd, buf, len)                         │
│      ▼                                                  │
│  glibc wrapper                                          │
│      │ mov $0, %eax    (syscall number for read = 0)    │
│      │ mov fd, %rdi    (first argument)                 │
│      │ mov buf, %rsi   (second argument)                │
│      │ mov len, %rdx   (third argument)                 │
│      │ SYSCALL         ← hardware mode switch           │
└──────┼──────────────────────────────────────────────────┘
       │ Ring 3 → Ring 0 (hardware enforced)
┌──────┼──────────────────────────────────────────────────┐
│  Kernel Space (Ring 0)                                  │
│      ▼                                                  │
│  entry_SYSCALL_64                                       │
│      │ saves registers to kernel stack                  │
│      │ looks up sys_call_table[RAX]                     │
│      ▼                                                  │
│  sys_read()                                             │
│      │ does actual work (VFS, page cache, driver)       │
│      │ return value → RAX                               │
│      │ SYSRET          ← hardware mode switch back      │
└──────┼──────────────────────────────────────────────────┘
       │ Ring 0 → Ring 3 (hardware enforced)
┌──────┼──────────────────────────────────────────────────┐
│  User Space (Ring 3)                                    │
│      ▼                                                  │
│  glibc wrapper returns to application                   │
└─────────────────────────────────────────────────────────┘
```

ARM64 使用 `SVC #0` → `VBAR_EL1 + 0x400` → `el0_svc` → `sys_call_table[x8]()` → `ERET`。

```bash
strace -c ./inference_app    # count syscalls by type and cumulative time
strace -T -e mmap,ioctl ./camerad  # trace specific calls with per-call duration
```

---

## 系统调用开销

| 开销组成 | 典型代价 |
|---|---|
| 模式切换（SYSCALL/SYSRET） | 50–150 ns |
| Spectre/Meltdown 缓解措施（IBRS、retpoline、KPTI） | 50–200 ns |
| TLB 与缓存影响（KPTI 会刷掉用户态 TLB 表项） | 20–100 ns |
| 总往返（内核工作量极小时） | 100–400 ns |

ARM64 的缓解措施（CSV2、SSBS）**比 x86 更轻量**。在 2 个摄像头各 200 fps、每帧需要 4 次 V4L2 ioctl 的场景下 = 1600 次系统调用/s × 300 ns = 0.5 ms/s 的纯**模式切换开销**。**批处理与零拷贝**（`mmap`、`io_uring`）可降低跨越频率。

> **关键洞察：** 与 2018 年之前的系统相比，Spectre 和 Meltdown 缓解措施（KPTI、IBRS、retpoline）使 x86 上的系统调用开销大约翻了一倍。内核在每次 user↔kernel 转换时刷掉或隔离页表项，以防推测执行泄露内核内存。ARM64 的缓解措施在架构上更轻量 —— 这也是嵌入式 AI 平台在延迟敏感工作负载上往往偏好 ARM 的原因之一。如果要把代码从 x86 benchmark 移植过来，不要假设系统调用开销相近。

既然已经理解了跨越特权边界的代价，接下来看看内核对调用最频繁的时间函数是如何避免这种跨越的。

---


<details>
<summary>English original</summary>

**Lecture 4: System Calls, vDSO & eBPF**

**Overview**

Every time user-space code needs the kernel to do something — open a file, allocate memory, send a packet, talk to a GPU — it crosses the hardware privilege boundary via a system call. The core challenge this lecture addresses is: how does this boundary crossing work, how expensive is it, and how can you observe what is happening inside the kernel without modifying source code? The mental model is a **toll booth**: every syscall is a controlled crossing from user space into kernel space, with a fixed cost whether the work takes 1 ns or 1 ms. For an AI hardware engineer, this matters because a 200 fps camera pipeline can spend measurable CPU time just crossing the toll booth for V4L2 ioctls, and eBPF is the instrument you use to measure and diagnose the kernel's internal behavior in production without changing a single line of application code.

---

**The System Call Interface**

**System calls** are the only sanctioned path for user-space code to request kernel services. User space **cannot** directly access hardware registers, allocate physical memory, or change scheduler policy — it asks the kernel via the **syscall ABI**.

**Call Path — x86-64**

```
Syscall Path — x86-64
┌─────────────────────────────────────────────────────────┐
│  User Space (Ring 3)                                    │
│                                                         │
│  Application code                                       │
│      │ calls read(fd, buf, len)                         │
│      ▼                                                  │
│  glibc wrapper                                          │
│      │ mov $0, %eax    (syscall number for read = 0)    │
│      │ mov fd, %rdi    (first argument)                 │
│      │ mov buf, %rsi   (second argument)                │
│      │ mov len, %rdx   (third argument)                 │
│      │ SYSCALL         ← hardware mode switch           │
└──────┼──────────────────────────────────────────────────┘
       │ Ring 3 → Ring 0 (hardware enforced)
┌──────┼──────────────────────────────────────────────────┐
│  Kernel Space (Ring 0)                                  │
│      ▼                                                  │
│  entry_SYSCALL_64                                       │
│      │ saves registers to kernel stack                  │
│      │ looks up sys_call_table[RAX]                     │
│      ▼                                                  │
│  sys_read()                                             │
│      │ does actual work (VFS, page cache, driver)       │
│      │ return value → RAX                               │
│      │ SYSRET          ← hardware mode switch back      │
└──────┼──────────────────────────────────────────────────┘
       │ Ring 0 → Ring 3 (hardware enforced)
┌──────┼──────────────────────────────────────────────────┐
│  User Space (Ring 3)                                    │
│      ▼                                                  │
│  glibc wrapper returns to application                   │
└─────────────────────────────────────────────────────────┘
```

ARM64 uses `SVC #0` → `VBAR_EL1 + 0x400` → `el0_svc` → `sys_call_table[x8]()` → `ERET`.

```bash
strace -c ./inference_app    # count syscalls by type and cumulative time
strace -T -e mmap,ioctl ./camerad  # trace specific calls with per-call duration
```

---

**Syscall Overhead**

| Cost component | Typical penalty |
|---|---|
| Mode switch (SYSCALL/SYSRET) | 50–150 ns |
| Spectre/Meltdown mitigations (IBRS, retpoline, KPTI) | 50–200 ns |
| TLB and cache effects (KPTI flushes user-space TLB entries) | 20–100 ns |
| Total round-trip (minimal kernel work) | 100–400 ns |

ARM64 mitigations (CSV2, SSBS) are **lighter than x86**. At 200 fps across 2 cameras, each frame requiring 4 V4L2 ioctls = 1600 syscalls/s × 300 ns = 0.5 ms/s pure **mode-switch overhead**. **Batching and zero-copy** (`mmap`, `io_uring`) reduce crossing frequency.

> **Key Insight:** Spectre and Meltdown mitigations (KPTI, IBRS, retpoline) roughly doubled syscall overhead on x86 compared to pre-2018 systems. The kernel flushes or isolates page table entries on each user↔kernel transition to prevent speculative execution from leaking kernel memory. ARM64 mitigations are architecturally lighter — one reason embedded AI platforms often prefer ARM for latency-sensitive workloads. If you are porting code from x86 benchmarks, do not assume syscall overhead is similar.

Now that we understand the cost of crossing the privilege boundary, let's look at how the kernel avoids that crossing for the most frequently called time functions.

---

</details>

## vDSO：无需进入内核的内核调用

**虚拟动态共享对象（vDSO）** 是内核在启动时映射进每个进程地址空间的只读 ELF 页。选定的时间函数从内核更新的共享 `vvar` 数据页读取 —— 没有 `SYSCALL` 指令，没有模式切换，没有 TLB flush。

可以把 vDSO 看作内核留在你地址空间里的一张便条：「这是当前时间，由内核持续更新。直接读取，不必问我。」

| 函数 | 完整系统调用 | vDSO |
|---|---|---|
| `clock_gettime(CLOCK_MONOTONIC)` | ~200 ns | ~10–20 ns |
| `clock_gettime(CLOCK_REALTIME)` | ~200 ns | ~10–20 ns |
| `gettimeofday()` | ~200 ns | ~10–20 ns |
| `getcpu()` | ~150 ns | ~5 ns |

通过 glibc 调用时，**可用时自动使用 vDSO** —— 无需修改应用。进程间计时用 `CLOCK_MONOTONIC`（不受 NTP 调整影响）；纯硬件单调计数器用 `CLOCK_MONOTONIC_RAW`。

```bash
# Verify vDSO is mapped
cat /proc/[pid]/maps | grep vdso
# Check which symbols are exported
nm /proc/[pid]/map_files/[vdso-range] 2>/dev/null | grep " T "
```

> **常见陷阱：** `CLOCK_REALTIME` 会被 NTP 调整，可能向后跳变。为传感器数据打时间戳或测量帧间间隔时，始终使用 `CLOCK_MONOTONIC`（或用 `CLOCK_MONOTONIC_RAW` 以同时排除 PTP/NTP 速率调整）。用 `CLOCK_REALTIME` 作为传感器融合时间戳，会在 NTP 于运行中途做出校正时产生隐蔽的同步 bug。

---

## AI 与嵌入式系统的关键系统调用

理解 AI 硬件工作中最重要的系统调用，意味着理解每次 V4L2 摄像头操作、GPU 命令提交和零拷贝缓冲区传输底层发生了什么。

### mmap / munmap

```c
/* Zero-copy shared memory between processes */
int fd = shm_open("/sensor_ring", O_CREAT | O_RDWR, 0600);
/* shm_open creates an anonymous file in /dev/shm backed by tmpfs */
void *buf = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
/* MAP_SHARED: writes are visible to all processes that mapped this fd */
/* No copy is ever made — all processes share the same physical pages */

/* Huge page mapping for large GPU staging buffer */
void *huge = mmap(NULL, 2*1024*1024, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
/* MAP_HUGETLB: allocate 2 MB huge pages instead of 4 KB pages */
/* Reduces TLB pressure: 1 TLB entry covers 2 MB instead of 4 KB */
/* Requires vm.nr_hugepages to be pre-allocated in /proc/sys/vm/ */
```

- `MAP_HUGETLB`：降低大型 staging buffer 的 TLB 压力；需要 `vm.nr_hugepages`
- `CUDA cudaMallocHost()` 为 pinned host memory 在 `/dev/nvidia*` 上调用 `mmap`
- 在 Jetson 统一内存上，`cudaMalloc` 在 `/dev/nvhost-as-gpu` 上使用 `mmap`

### ioctl

**主要的设备控制接口**；几乎所有硬件特定的操作都用它。

```
Syscall Path for V4L2 Camera Buffer Dequeue
┌─────────────┐
│  camerad    │  ioctl(fd, VIDIOC_DQBUF, &buf)
└──────┬──────┘
       │ SYSCALL (ioctl number)
       ▼
┌─────────────────────────────────────────────────────────┐
│  sys_ioctl() → vfs_ioctl() → v4l2_ioctl()              │
│  → video_device.ioctl_ops.vidioc_dqbuf()                │
│  → driver's dequeue function                           │
│  → blocks in TASK_UNINTERRUPTIBLE until frame arrives  │
│  → returns buffer with frame pointer                   │
└─────────────────────────────────────────────────────────┘
       │ SYSRET
       ▼
┌─────────────┐
│  camerad    │  buf.m.userptr now points to frame data
└─────────────┘
```

| ioctl | 设备 | 用途 |
|---|---|---|
| `VIDIOC_QBUF` / `VIDIOC_DQBUF` | V4L2 摄像头 | 入队 / 出队采集缓冲区 |
| `VIDIOC_STREAMON` / `STREAMOFF` | V4L2 | 开始 / 停止流式传输 |
| `DRM_IOCTL_GEM_*` | DRM/KMS | 缓冲区分配、显示扫描输出 |
| `NVGPU_IOCTL_CHANNEL_ALLOC_GPFIFO` | NVIDIA GPU (`/dev/nvhost-gpu`) | 分配 GPU 命令队列 |
| `RPMSG_CREATE_EPT_IOCTL` | RPMsg | 与 Cortex-M 协处理器（i.MX8）的 IPC |
| 自定义 `_IOWR(MAGIC, N, struct)` | FPGA PCIe 驱动 | 向加速器提交推理工作负载 |

### epoll

**跨多个文件描述符的每事件 O(1) 多路复用**。用于 openpilot 的 `cereal` 消息层，多路复用 CAN 帧、摄像头 V4L2 事件、IMU 数据和模型输出事件。

```c
int epfd = epoll_create1(EPOLL_CLOEXEC);
/* EPOLL_CLOEXEC: close the epoll fd automatically on exec() */
struct epoll_event ev = { .events = EPOLLIN | EPOLLET, .data.fd = camera_fd };
/* EPOLLET: edge-triggered — notify once when data arrives, not repeatedly */
epoll_ctl(epfd, EPOLL_CTL_ADD, camera_fd, &ev);
int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);
/* blocks until at least one fd is ready; returns number of ready events */
```

**边缘触发模式**（`EPOLLET`）更适用于延迟敏感路径 —— **每个边缘一次唤醒**，不会对未读数据重复通知。


<details>
<summary>English original</summary>

**vDSO: Kernel Calls Without Kernel Entry**

The **virtual Dynamic Shared Object (vDSO)** is a read-only ELF page mapped by the kernel into every process address space at startup. Selected time functions read from a shared `vvar` data page that the kernel updates — no `SYSCALL` instruction, no mode transition, no TLB flush.

Think of the vDSO as a memo that the kernel leaves in your address space: "Here is the current time, updated continuously by the kernel. Read it directly without asking me."

| Function | Full syscall | vDSO |
|---|---|---|
| `clock_gettime(CLOCK_MONOTONIC)` | ~200 ns | ~10–20 ns |
| `clock_gettime(CLOCK_REALTIME)` | ~200 ns | ~10–20 ns |
| `gettimeofday()` | ~200 ns | ~10–20 ns |
| `getcpu()` | ~150 ns | ~5 ns |

Calling through glibc **automatically uses vDSO** when available — no application change required. Use `CLOCK_MONOTONIC` for inter-process timing (not subject to NTP adjustments); use `CLOCK_MONOTONIC_RAW` for hardware-only monotonic counter.

```bash
# Verify vDSO is mapped
cat /proc/[pid]/maps | grep vdso
# Check which symbols are exported
nm /proc/[pid]/map_files/[vdso-range] 2>/dev/null | grep " T "
```

> **Common Pitfall:** `CLOCK_REALTIME` is adjusted by NTP and can jump backward. For timestamping sensor data or measuring inter-frame intervals, always use `CLOCK_MONOTONIC` (or `CLOCK_MONOTONIC_RAW` to also exclude PTP/NTP rate adjustments). Using `CLOCK_REALTIME` for sensor fusion timestamps creates subtle synchronization bugs when NTP makes a correction mid-run.

---

**Key Syscalls for AI and Embedded Systems**

Understanding the most important syscalls for AI hardware work means understanding what happens under the hood of every V4L2 camera operation, GPU command submission, and zero-copy buffer transfer.

**mmap / munmap**

```c
/* Zero-copy shared memory between processes */
int fd = shm_open("/sensor_ring", O_CREAT | O_RDWR, 0600);
/* shm_open creates an anonymous file in /dev/shm backed by tmpfs */
void *buf = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
/* MAP_SHARED: writes are visible to all processes that mapped this fd */
/* No copy is ever made — all processes share the same physical pages */

/* Huge page mapping for large GPU staging buffer */
void *huge = mmap(NULL, 2*1024*1024, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
/* MAP_HUGETLB: allocate 2 MB huge pages instead of 4 KB pages */
/* Reduces TLB pressure: 1 TLB entry covers 2 MB instead of 4 KB */
/* Requires vm.nr_hugepages to be pre-allocated in /proc/sys/vm/ */
```

- `MAP_HUGETLB`: reduces TLB pressure for large staging buffers; requires `vm.nr_hugepages`
- `CUDA cudaMallocHost()` calls `mmap` on `/dev/nvidia*` for pinned host memory
- On Jetson unified memory, `cudaMalloc` uses `mmap` on `/dev/nvhost-as-gpu`

**ioctl**

**Primary device control interface**; nearly all hardware-specific operations use it.

```
Syscall Path for V4L2 Camera Buffer Dequeue
┌─────────────┐
│  camerad    │  ioctl(fd, VIDIOC_DQBUF, &buf)
└──────┬──────┘
       │ SYSCALL (ioctl number)
       ▼
┌─────────────────────────────────────────────────────────┐
│  sys_ioctl() → vfs_ioctl() → v4l2_ioctl()              │
│  → video_device.ioctl_ops.vidioc_dqbuf()                │
│  → driver's dequeue function                           │
│  → blocks in TASK_UNINTERRUPTIBLE until frame arrives  │
│  → returns buffer with frame pointer                   │
└─────────────────────────────────────────────────────────┘
       │ SYSRET
       ▼
┌─────────────┐
│  camerad    │  buf.m.userptr now points to frame data
└─────────────┘
```

| ioctl | Device | Purpose |
|---|---|---|
| `VIDIOC_QBUF` / `VIDIOC_DQBUF` | V4L2 camera | Queue / dequeue capture buffer |
| `VIDIOC_STREAMON` / `STREAMOFF` | V4L2 | Start / stop streaming |
| `DRM_IOCTL_GEM_*` | DRM/KMS | Buffer allocation, display scanout |
| `NVGPU_IOCTL_CHANNEL_ALLOC_GPFIFO` | NVIDIA GPU (`/dev/nvhost-gpu`) | Allocate GPU command queue |
| `RPMSG_CREATE_EPT_IOCTL` | RPMsg | IPC with Cortex-M coprocessor (i.MX8) |
| Custom `_IOWR(MAGIC, N, struct)` | FPGA PCIe driver | Submit inference workload to accelerator |

**epoll**

**O(1) per-event multiplexing** across many file descriptors. Used in openpilot's `cereal` messaging layer to multiplex CAN frames, camera V4L2 events, IMU data, and model output events.

```c
int epfd = epoll_create1(EPOLL_CLOEXEC);
/* EPOLL_CLOEXEC: close the epoll fd automatically on exec() */
struct epoll_event ev = { .events = EPOLLIN | EPOLLET, .data.fd = camera_fd };
/* EPOLLET: edge-triggered — notify once when data arrives, not repeatedly */
epoll_ctl(epfd, EPOLL_CTL_ADD, camera_fd, &ev);
int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);
/* blocks until at least one fd is ready; returns number of ready events */
```

**Edge-triggered mode** (`EPOLLET`) is preferred for latency-sensitive paths — **one wakeup per edge**, no repeated notifications for unread data.

</details>

### prctl

```
PR_SET_NAME          Name thread; visible in ps/top/htop/perf
PR_SET_TIMERSLACK    Reduce timer coalescing (set to 1 ns for RT threads; default 50 µs)
PR_SET_SECCOMP       Apply seccomp-BPF filter to current thread
PR_SET_NO_NEW_PRIVS  Prevent privilege escalation across exec()
```

> **关键洞察：** `PR_SET_TIMERSLACK` 是时序抖动的隐藏来源。默认的 50 µs timer slack 允许内核合并邻近的定时器到期以节省功耗。一个等待 1 ms 定时器的 RT 线程可能最多迟到 50 µs 才被唤醒。设置 `prctl(PR_SET_TIMERSLACK, 1)`（1 ns）会为该线程禁用合并，消除这一抖动来源。这是 `controlsd` 以及 openpilot 上任何 CAN 写线程的标准做法。

### sched_setattr — SCHED_DEADLINE

```c
struct sched_attr attr = {
    .size           = sizeof(attr),
    .sched_policy   = SCHED_DEADLINE,
    .sched_runtime  = 5000000,     /* 5ms budget per period — CPU time consumed before descheduling */
    .sched_deadline = 16666666,    /* 16.7ms relative deadline — must complete by this time */
    .sched_period   = 16666666,    /* 16.7ms period — one activation per period (60fps) */
};
sched_setattr(0, &attr, 0);        /* 0 = self; requires CAP_SYS_NICE */
```

### perf_event_open

从用户态访问**硬件性能计数器**。被 `perf stat`、Nsight Systems 和 VTune 使用。

```c
struct perf_event_attr pe = {
    .type   = PERF_TYPE_HARDWARE,
    .config = PERF_COUNT_HW_CACHE_MISSES,  /* LLC miss counter */
    .disabled = 1,                          /* start disabled; enable manually */
};
int fd = perf_event_open(&pe, 0, -1, -1, 0);  /* measure self (pid=0) */
ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);          /* start counting */
/* ... inference workload runs here ... */
read(fd, &count, sizeof(count));               /* read accumulated miss count */
```

在 Jetson 上，ARM PMU 计数器测量 DNN 推理期间的 LLC miss rate——用于指导 INT8 分块决策。

### memfd_create

创建一个**匿名文件后端的内存区域**——可用作共享内存而无需文件系统路径。在 openpilot 的 `msgq` 中用于 `modeld` 与 `controlsd` 之间的**零拷贝 IPC**。

```c
int fd = memfd_create("shared_tensor", MFD_CLOEXEC);
/* Creates an anonymous file in kernel memory — no filesystem path needed */
ftruncate(fd, TENSOR_SIZE);
/* Sets the file size; backing pages are allocated lazily on first access */
void *ptr = mmap(NULL, TENSOR_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
/* Map the shared region into this process's address space */
/* pass fd over Unix socket to second process for its own mmap */
/* both processes now share the same physical pages — zero-copy IPC */
```

该模式让 `modeld` 将推理输出直接写入共享缓冲区，`controlsd` 无需任何拷贝即可读取。`fd` 使用 `SCM_RIGHTS` 通过 Unix domain socket 传递。

> **常见陷阱：** `memfd_create` 缓冲区位于 RAM（tmpfs）中。如果推理张量很大（例如 50 MB 的 feature map），以 30 fps 创建和销毁共享张量会因反复缺页而消耗大量 RAM 带宽。应在启动时一次性预分配 memfd 缓冲区并复用，而不是每帧新建。

---

## eBPF：挂接到内核的已验证程序

**eBPF** 程序在内核内运行，安全性经过验证：内核 verifier 在 JIT 编译为原生代码之前检查边界、循环终止和指针类型。无需内核模块，无需重新编译。可以把 eBPF 看作一台只读显微镜，可在 runtime 挂接到任意内核函数——它只观察，不修改。


<details>
<summary>English original</summary>

**prctl**

```
PR_SET_NAME          Name thread; visible in ps/top/htop/perf
PR_SET_TIMERSLACK    Reduce timer coalescing (set to 1 ns for RT threads; default 50 µs)
PR_SET_SECCOMP       Apply seccomp-BPF filter to current thread
PR_SET_NO_NEW_PRIVS  Prevent privilege escalation across exec()
```

> **Key Insight:** `PR_SET_TIMERSLACK` is a hidden source of timing jitter. The default 50 µs timer slack allows the kernel to coalesce nearby timer expirations to save power. An RT thread waiting for a 1 ms timer may be woken up to 50 µs late. Setting `prctl(PR_SET_TIMERSLACK, 1)` (1 ns) disables coalescing for that thread and removes this source of jitter. This is standard practice for `controlsd` and any CAN write thread on openpilot.

**sched_setattr — SCHED_DEADLINE**

```c
struct sched_attr attr = {
    .size           = sizeof(attr),
    .sched_policy   = SCHED_DEADLINE,
    .sched_runtime  = 5000000,     /* 5ms budget per period — CPU time consumed before descheduling */
    .sched_deadline = 16666666,    /* 16.7ms relative deadline — must complete by this time */
    .sched_period   = 16666666,    /* 16.7ms period — one activation per period (60fps) */
};
sched_setattr(0, &attr, 0);        /* 0 = self; requires CAP_SYS_NICE */
```

**perf_event_open**

Accesses **hardware performance counters** from userspace. Used by `perf stat`, Nsight Systems, and VTune.

```c
struct perf_event_attr pe = {
    .type   = PERF_TYPE_HARDWARE,
    .config = PERF_COUNT_HW_CACHE_MISSES,  /* LLC miss counter */
    .disabled = 1,                          /* start disabled; enable manually */
};
int fd = perf_event_open(&pe, 0, -1, -1, 0);  /* measure self (pid=0) */
ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);          /* start counting */
/* ... inference workload runs here ... */
read(fd, &count, sizeof(count));               /* read accumulated miss count */
```

On Jetson, ARM PMU counters measure LLC miss rate during DNN inference — guides INT8 tiling decisions.

**memfd_create**

Creates an **anonymous file-backed memory region** — usable as shared memory without a filesystem path. Used in openpilot's `msgq` for **zero-copy IPC** between `modeld` and `controlsd`.

```c
int fd = memfd_create("shared_tensor", MFD_CLOEXEC);
/* Creates an anonymous file in kernel memory — no filesystem path needed */
ftruncate(fd, TENSOR_SIZE);
/* Sets the file size; backing pages are allocated lazily on first access */
void *ptr = mmap(NULL, TENSOR_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
/* Map the shared region into this process's address space */
/* pass fd over Unix socket to second process for its own mmap */
/* both processes now share the same physical pages — zero-copy IPC */
```

This pattern allows `modeld` to write inference outputs directly into a shared buffer that `controlsd` reads without any copy. The `fd` is passed over a Unix domain socket using `SCM_RIGHTS`.

> **Common Pitfall:** `memfd_create` buffers live in RAM (tmpfs). If the inference tensor is large (e.g., 50 MB feature map), creating and destroying shared tensors at 30 fps consumes significant RAM bandwidth from repeated page faults. Pre-allocate the memfd buffers once at startup and reuse them, rather than creating new ones per frame.

---

**eBPF: Kernel-Attached Verified Programs**

**eBPF** programs run inside the kernel with verified safety: the kernel verifier checks bounds, loop termination, and pointer types before JIT-compiling to native code. No kernel module, no recompile. Think of eBPF as a read-only microscope you can attach to any kernel function at runtime — it observes without modifying.

</details>

### 架构

```
eBPF Program Lifecycle
┌──────────────────────────────────────────────────────────────┐
│  Development                                                 │
│  C source → clang + libbpf → BPF bytecode (.o file)         │
└───────────────────────────────┬──────────────────────────────┘
                                │ load via bpf() syscall
                                ▼
┌──────────────────────────────────────────────────────────────┐
│  Kernel Verification                                         │
│  → bounds checker (no out-of-bounds memory access)          │
│  → loop termination verifier (no infinite loops)            │
│  → pointer type checker (no arbitrary kernel ptr dereference)│
│  → JIT compile to native code                               │
└───────────────────────────────┬──────────────────────────────┘
                                │ attach to hook
                                ▼
┌──────────────────────────────────────────────────────────────┐
│  Runtime                                                     │
│  Event fires (syscall / tracepoint / kprobe / XDP packet)   │
│  → eBPF program runs in kernel context                      │
│  → writes results to BPF maps (ring buffer / hash / array)  │
└───────────────────────────────┬──────────────────────────────┘
                                │ user-space reads maps
                                ▼
┌──────────────────────────────────────────────────────────────┐
│  User Space                                                  │
│  bpftrace / BCC tool reads BPF maps                         │
│  → latency histograms, counts, traces                       │
└──────────────────────────────────────────────────────────────┘
```

### 附加点

| Hook | 用途 |
|---|---|
| `kprobe` / `kretprobe` | 任意 kernel 函数入口/返回 — 驱动内部 |
| `tracepoint` | 稳定的 kernel tracepoint：`sched:sched_switch`、`irq:irq_handler_entry`、`block:block_rq_issue` |
| `uprobe` | 用户态函数：TensorRT engine 执行、glibc 内存分配 |
| `XDP` | 驱动上下文中的栈前数据包处理 — 100GbE 线速过滤 |
| `TC`（流量控制） | 栈后出向/入向；用于延迟打标 |

### BCC / bpftrace 生产工具

```bash
runqlat                                   # scheduler run-queue latency histogram
# First tool to run when inference latency is inconsistent

offcputime -p $(pgrep modeld) 10          # where modeld spends time blocked off-CPU
# Shows what kernel function modeld is sleeping in and for how long

# Count context switches per process
bpftrace -e 'tracepoint:sched:sched_switch { @[comm] = count(); }'
# High counts on modeld → investigate CFS preemption; consider SCHED_FIFO

# Trace ioctl latency from camerad
bpftrace -e '
  tracepoint:syscalls:sys_enter_ioctl /comm == "camerad"/ { @s[tid] = nsecs; }
  tracepoint:syscalls:sys_exit_ioctl  /comm == "camerad"/ {
    @us = hist((nsecs - @s[tid]) / 1000); delete(@s[tid]); }'
# Outputs histogram of VIDIOC_DQBUF latency in microseconds

execsnoop        # trace new process executions
opensnoop        # trace file opens (useful to find what config camerad loads)
biolatency       # block I/O latency histogram (model loading from NVMe)
```

> **关键洞察：**eBPF 之所以格外强大，是因为它能附加到生产系统上，无需任何源码修改、重新编译或重启。你可以对现场 Jetson 上运行的 `modeld` 进行插桩，采集 `VIDIOC_DQBUF` 调用的延迟直方图，然后卸载探针——全程推理流水线持续运行。传统性能分析器做不到这一点，它们要么需要源码插桩，要么必须停止进程。

> **常见陷阱：**使用 `kprobe` hook 的 eBPF 程序在不同内核版本间很脆弱——内核内部函数名和签名会随版本变化。尽可能改用 `tracepoint` hook；tracepoint 是定义在 `Documentation/trace/tracepoints.rst` 中的稳定 ABI。`syscalls:sys_enter_ioctl` tracepoint 可适用于任何 Linux kernel 版本。

---


<details>
<summary>English original</summary>

**Architecture**

```
eBPF Program Lifecycle
┌──────────────────────────────────────────────────────────────┐
│  Development                                                 │
│  C source → clang + libbpf → BPF bytecode (.o file)         │
└───────────────────────────────┬──────────────────────────────┘
                                │ load via bpf() syscall
                                ▼
┌──────────────────────────────────────────────────────────────┐
│  Kernel Verification                                         │
│  → bounds checker (no out-of-bounds memory access)          │
│  → loop termination verifier (no infinite loops)            │
│  → pointer type checker (no arbitrary kernel ptr dereference)│
│  → JIT compile to native code                               │
└───────────────────────────────┬──────────────────────────────┘
                                │ attach to hook
                                ▼
┌──────────────────────────────────────────────────────────────┐
│  Runtime                                                     │
│  Event fires (syscall / tracepoint / kprobe / XDP packet)   │
│  → eBPF program runs in kernel context                      │
│  → writes results to BPF maps (ring buffer / hash / array)  │
└───────────────────────────────┬──────────────────────────────┘
                                │ user-space reads maps
                                ▼
┌──────────────────────────────────────────────────────────────┐
│  User Space                                                  │
│  bpftrace / BCC tool reads BPF maps                         │
│  → latency histograms, counts, traces                       │
└──────────────────────────────────────────────────────────────┘
```

**Attachment Points**

| Hook | Use |
|---|---|
| `kprobe` / `kretprobe` | Any kernel function entry/return — driver internals |
| `tracepoint` | Stable kernel tracepoints: `sched:sched_switch`, `irq:irq_handler_entry`, `block:block_rq_issue` |
| `uprobe` | User-space function: TensorRT engine execution, glibc allocations |
| `XDP` | Pre-stack packet processing in driver context — 100GbE line rate filtering |
| `TC` (traffic control) | Post-stack egress/ingress; used for latency tagging |

**BCC / bpftrace Production Tools**

```bash
runqlat                                   # scheduler run-queue latency histogram
# First tool to run when inference latency is inconsistent

offcputime -p $(pgrep modeld) 10          # where modeld spends time blocked off-CPU
# Shows what kernel function modeld is sleeping in and for how long

# Count context switches per process
bpftrace -e 'tracepoint:sched:sched_switch { @[comm] = count(); }'
# High counts on modeld → investigate CFS preemption; consider SCHED_FIFO

# Trace ioctl latency from camerad
bpftrace -e '
  tracepoint:syscalls:sys_enter_ioctl /comm == "camerad"/ { @s[tid] = nsecs; }
  tracepoint:syscalls:sys_exit_ioctl  /comm == "camerad"/ {
    @us = hist((nsecs - @s[tid]) / 1000); delete(@s[tid]); }'
# Outputs histogram of VIDIOC_DQBUF latency in microseconds

execsnoop        # trace new process executions
opensnoop        # trace file opens (useful to find what config camerad loads)
biolatency       # block I/O latency histogram (model loading from NVMe)
```

> **Key Insight:** eBPF is uniquely powerful because it attaches to production systems without any source code changes, recompilation, or restart. You can instrument `modeld` running on a Jetson in the field, collect a latency histogram of `VIDIOC_DQBUF` calls, and detach the probe — all while the inference pipeline continues running. This is impossible with traditional profilers that require either source instrumentation or stopping the process.

> **Common Pitfall:** eBPF programs that use `kprobe` hooks are fragile across kernel versions — kernel internal function names and signatures change between releases. Use `tracepoint` hooks instead when possible; tracepoints are stable ABIs defined in `Documentation/trace/tracepoints.rst`. The `syscalls:sys_enter_ioctl` tracepoint will work on any Linux kernel version.

---

</details>

## seccomp：系统调用过滤

一个在每次系统调用时求值的 **BPF 程序**；返回 `ALLOW`、`ERRNO(N)`、`KILL` 或 `TRAP`。通过 `prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)` 应用。

Docker 默认的 seccomp profile 会阻止约 44 个系统调用（`kexec_load`、`ptrace`、`mount`、`unshare` 等）。一个 TensorRT 推理容器所需的系统调用不到 50 个；一份严格的 allowlist 可消除其余的全部。可防止容器化 AI 部署中漏洞利用后的横向移动。

```
seccomp Decision Flow
┌─────────────┐
│  Application│  calls read(fd, buf, len)
└──────┬──────┘
       │ SYSCALL
       ▼
┌──────────────────────────────────┐
│  seccomp-BPF filter              │
│  checks syscall number against   │
│  the program's allow-list        │
│                                  │
│  read (0)?  → ALLOW → continue  │
│  ptrace(101)? → KILL             │
│  kexec_load? → ERRNO(EPERM)     │
└──────────────────────────────────┘
```

---

## Linux Capabilities

Root 特权被划分为 **约 40 项细粒度授权**。在初始化之后 **丢弃 capabilities**。

| Capability | 授予的权限 |
|---|---|
| `CAP_SYS_NICE` | `sched_setscheduler()`、`sched_setattr()` —— 无需 root 即可设置 RT 优先级 |
| `CAP_IPC_LOCK` | `mlockall()` —— 锁定所有内存页；消除 RT 缺页延迟 |
| `CAP_NET_ADMIN` | 网络配置、raw socket、eBPF TC 程序 |
| `CAP_SYS_RAWIO` | 直接硬件 I/O、PCIe MMIO 访问、FPGA 寄存器写 |
| `CAP_PERFMON` | 使用硬件计数器的 `perf_event_open()` |

```bash
setcap cap_sys_nice+ep /opt/inference/modeld    # grant RT capability; no sudo at runtime
grep Cap /proc/$(pgrep modeld)/status           # inspect effective capability mask
```

> **核心要点：** `setcap` 将 capability 写入 ELF 二进制的扩展属性。当 `modeld` 执行时，内核读取这些属性，并在无 root 访问权限的情况下授予这些 capabilities。这比以 root 身份运行 `modeld` 更安全，因为进程只拥有它所需的那些特定 capabilities —— 例如，它无法挂载文件系统（`CAP_SYS_ADMIN`）或加载内核模块（`CAP_SYS_MODULE`）。把最小权限原则应用到系统调用上。

> **常见陷阱：** 当二进制被替换时（例如被软件更新替换），`setcap` 授予的权限会丢失。一个复制新二进制却未重新应用 `setcap` 的 Makefile 或安装脚本会静默破坏 RT 调度。务必在安装步骤中包含 `setcap` 调用，并在部署后用 `getcap /opt/inference/modeld` 验证。

---

## 小结

| 机制 | 是否进入内核？ | 延迟 | 主要用途 |
|---|---|---|---|
| Syscall（SYSCALL / SVC） | 是 | 100–400 ns | 所有内核服务 |
| vDSO（`clock_gettime`） | 否 | 10–20 ns | 高频时间戳 |
| `mmap`（初始化后） | 否（仅缺页时） | 命中缓存时低于 1 ns | 零拷贝缓冲区、共享内存 |
| eBPF（JIT，内核 hook） | 在内核中运行 | 每次 probe < 1 µs | 生产环境性能剖析、系统调用过滤 |
| seccomp filter | 每次系统调用检查 | 5–20 ns 开销 | 沙箱；缩减攻击面 |
| vDSO `getcpu()` | 否 | ~5 ns | 为无锁 ring 确定当前 CPU |


<details>
<summary>English original</summary>

**seccomp: Syscall Filtering**

A **BPF program** evaluated on every syscall; returns `ALLOW`, `ERRNO(N)`, `KILL`, or `TRAP`. Applied with `prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)`.

Docker's default seccomp profile blocks ~44 syscalls (`kexec_load`, `ptrace`, `mount`, `unshare`, etc.). A TensorRT inference container needs fewer than 50 syscalls; a tight allowlist eliminates the rest. Prevents post-exploit lateral movement in containerized AI deployments.

```
seccomp Decision Flow
┌─────────────┐
│  Application│  calls read(fd, buf, len)
└──────┬──────┘
       │ SYSCALL
       ▼
┌──────────────────────────────────┐
│  seccomp-BPF filter              │
│  checks syscall number against   │
│  the program's allow-list        │
│                                  │
│  read (0)?  → ALLOW → continue  │
│  ptrace(101)? → KILL             │
│  kexec_load? → ERRNO(EPERM)     │
└──────────────────────────────────┘
```

---

**Linux Capabilities**

Root privilege is split into **~40 fine-grained grants**. **Drop capabilities** after setup.

| Capability | Grants |
|---|---|
| `CAP_SYS_NICE` | `sched_setscheduler()`, `sched_setattr()` — set RT priority without root |
| `CAP_IPC_LOCK` | `mlockall()` — lock all memory pages; eliminates RT page-fault latency |
| `CAP_NET_ADMIN` | Network configuration, raw sockets, eBPF TC programs |
| `CAP_SYS_RAWIO` | Direct hardware I/O, PCIe MMIO access, FPGA register writes |
| `CAP_PERFMON` | `perf_event_open()` with hardware counters |

```bash
setcap cap_sys_nice+ep /opt/inference/modeld    # grant RT capability; no sudo at runtime
grep Cap /proc/$(pgrep modeld)/status           # inspect effective capability mask
```

> **Key Insight:** `setcap` writes the capability into the ELF binary's extended attributes. When `modeld` executes, the kernel reads these attributes and grants the capabilities without root access. This is safer than running `modeld` as root because the process only has the specific capabilities it needs — it cannot, for example, mount filesystems (`CAP_SYS_ADMIN`) or load kernel modules (`CAP_SYS_MODULE`). Principle of least privilege applied to system calls.

> **Common Pitfall:** `setcap` grants are lost when a binary is replaced (e.g., by a software update). A Makefile or install script that copies a new binary without re-applying `setcap` will silently break RT scheduling. Always include the `setcap` call in the install step and verify with `getcap /opt/inference/modeld` after deployment.

---

**Summary**

| Mechanism | Kernel entry? | Latency | Primary use |
|---|---|---|---|
| Syscall (SYSCALL / SVC) | Yes | 100–400 ns | All kernel services |
| vDSO (`clock_gettime`) | No | 10–20 ns | High-frequency timestamping |
| `mmap` (after setup) | No (page faults only) | Sub-ns when cached | Zero-copy buffers, shared memory |
| eBPF (JIT, kernel hook) | Runs in kernel | < 1 µs per probe | Production profiling, syscall filtering |
| seccomp filter | Per-syscall check | 5–20 ns overhead | Sandbox; attack surface reduction |
| vDSO `getcpu()` | No | ~5 ns | Determine current CPU for lockless rings |

</details>

### 概念回顾

- **为什么从用户空间跨到内核空间要花 100–400 ns？** CPU 必须保存寄存器、切换页表（KPTI）、刷 TLB 表项，并在 x86 上执行 Spectre/Meltdown 缓解措施。实际的内核工作可能微不足道，但这次切换本身就有无法避免的硬件开销。ARM64 的缓解措施在架构上更轻量，这正是嵌入式 AI 平台偏好 ARM 的原因之一。
- **什么是 vDSO，它为什么比系统调用快？** vDSO 是内核维护的一个 ELF 页，被映射进每个进程。它包含诸如 `clock_gettime` 之类的函数，这些函数从内核持续更新的共享 `vvar` 页中读取数据。不需要 `SYSCALL` 指令 —— 该函数完全在用户空间执行，把延迟从约 200 ns 降到约 15 ns。
- **`epoll` 的水平触发与边沿触发模式有什么区别？** 水平触发：只要有数据可读 `epoll_wait` 就返回（同一个 fd 可能被返回多次）。边沿触发：只有新数据到达时 `epoll_wait` 才返回（每个事件一次通知）。对于 V4L2 事件这类高吞吐路径，首选边沿触发，因为当消费者比生产者慢时它能避免重复唤醒。
- **eBPF 的内核 verifier 检查什么？** 它验证程序没有越界内存访问、一定会终止（没有无限循环），并且不解引用任意内核指针。只有通过全部检查的程序才会被 JIT 编译并挂载。这正是 eBPF 能在生产内核上下文中无需修改源码即可安全运行的原因。
- **为什么用 `memfd_create` 而不是 POSIX 共享内存（`shm_open`）？** `memfd_create` 文件是匿名的 —— 它们没有文件系统路径，当引用它们的文件描述符全部关闭后会被自动清理。`shm_open` 会在 `/dev/shm` 中创建一个文件，进程退出后仍然存在（直到显式 unlink），这会在多次运行之间泄漏共享内存。
- **什么是 seccomp，它对推理容器为什么重要？** seccomp 过滤进程被允许发起的系统调用。一个 TensorRT 推理容器只需要约 50 个系统调用（`mmap`、`ioctl`、`read`、`write`、`epoll_wait` 等）。屏蔽其他所有系统调用（包括 `ptrace`、`kexec_load`、`mount`）意味着被攻陷的推理进程无法提权或在系统上持久化 —— 在任何漏洞利用代码运行之前，内核就会拒绝危险的系统调用。

---

## AI 硬件关联

- vDSO `clock_gettime(CLOCK_MONOTONIC)` 以每次调用约 15 ns 的开销提供 µs 级精度的传感器时间戳 —— 对于在 openpilot 的传感器融合流水线中同步摄像头帧、IMU 采样与 CAN 报文而言不可或缺，且无需进入内核。
- `ioctl(VIDIOC_DQBUF)` 是每个 V4L2 摄像头流水线中的热路径；用 `bpftrace` 测量其延迟分布，就能在不修改、不重新编译 `camerad` 的前提下直接量化从摄像头到模型的输入延迟。
- `bpftrace runqlat` 是诊断推理延迟毛刺的第一手段 —— 模型线程上 5 ms 的调度器运行队列延迟会立刻出现在直方图中，无需改动代码也无需插入内核模块。
- 通过 `setcap` 使用 `CAP_SYS_NICE`，可让 `modeld` 在不以 root 运行的情况下设置自己的 RT 调度策略，与容器安全策略和 Kubernetes `securityContext.capabilities` 兼容。
- TensorRT 推理容器上的 seccomp 允许列表会屏蔽 `ptrace`、`kexec_load` 和 `mount` 之类的系统调用，它们对推理毫无意义，却在 AV 部署中提供了大量漏洞利用后路径。
- 在 Jetson Orin 上配合 ARM PMU 事件使用 `perf_event_open`，可测量 DNN 推理期间的 LLC 缺失率 —— 缓存缺失数据可指导 INT8 量化与 layer 分块决策，把内存工作集压到 L2/L3 缓存容量以下。


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why does crossing from user space to kernel space cost 100–400 ns?** The CPU must save registers, switch page tables (KPTI), flush TLB entries, and execute Spectre/Meltdown mitigations on x86. The actual kernel work may be trivial, but the transition itself has unavoidable hardware costs. ARM64 mitigations are architecturally lighter, which is one reason embedded AI platforms prefer ARM.
- **What is the vDSO and why is it faster than a syscall?** The vDSO is a kernel-maintained ELF page mapped into every process. It contains functions like `clock_gettime` that read from a shared `vvar` page the kernel updates continuously. No `SYSCALL` instruction is needed — the function executes entirely in user space, reducing latency from ~200 ns to ~15 ns.
- **What is the difference between `epoll` level-triggered and edge-triggered modes?** Level-triggered: `epoll_wait` returns as long as there is data available (can return the same fd multiple times). Edge-triggered: `epoll_wait` returns only when new data arrives (one notification per event). Edge-triggered is preferred for high-throughput paths like V4L2 events because it avoids repeated wakeups when the consumer is slower than the producer.
- **What does eBPF's kernel verifier check?** It verifies that the program has no out-of-bounds memory accesses, will always terminate (no infinite loops), and does not dereference arbitrary kernel pointers. Only programs that pass all checks are JIT-compiled and attached. This is what makes eBPF safe to run in production kernel context without source changes.
- **Why use `memfd_create` instead of POSIX shared memory (`shm_open`)?** `memfd_create` files are anonymous — they have no filesystem path and are automatically cleaned up when all file descriptors referencing them are closed. `shm_open` creates a file in `/dev/shm` that persists after process exit (until explicitly unlinked), which can leak shared memory between runs.
- **What is seccomp and why does it matter for inference containers?** seccomp filters the syscalls a process is allowed to make. A TensorRT inference container only needs ~50 syscalls (`mmap`, `ioctl`, `read`, `write`, `epoll_wait`, etc.). Blocking everything else (including `ptrace`, `kexec_load`, `mount`) means a compromised inference process cannot escalate privilege or persist on the system — the kernel refuses the dangerous syscall before any exploit code runs.

---

**AI Hardware Connection**

- vDSO `clock_gettime(CLOCK_MONOTONIC)` provides µs-accurate sensor timestamping at ~15 ns per call — essential for synchronizing camera frames, IMU samples, and CAN messages in openpilot's sensor fusion pipeline without kernel entry overhead.
- `ioctl(VIDIOC_DQBUF)` is the hot path in every V4L2 camera pipeline; measuring its latency distribution with `bpftrace` directly quantifies camera-to-model input delay without modifying or recompiling `camerad`.
- `bpftrace runqlat` is the first diagnostic for inference latency spikes — 5 ms scheduler run-queue latency on the model thread appears immediately in the histogram without code changes or kernel module insertion.
- `CAP_SYS_NICE` via `setcap` allows `modeld` to set its own RT scheduling policy without running as root, compatible with container security policies and Kubernetes `securityContext.capabilities`.
- seccomp allowlists on TensorRT inference containers block syscalls like `ptrace`, `kexec_load`, and `mount` that are meaningless for inference but provide significant post-exploit paths in AV deployments.
- `perf_event_open` with ARM PMU events on Jetson Orin measures LLC miss rate during DNN inference — cache miss data guides INT8 quantization and layer tiling decisions to reduce the memory working set below the L2/L3 cache size.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
