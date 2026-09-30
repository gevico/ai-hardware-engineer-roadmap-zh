---
title: 第 19 讲：现代 I/O：iouring、DMA-BUF 与零拷贝流水线
description: 第 19 讲：现代 I/O：iouring、DMA-BUF 与零拷贝流水线
---

# 第 19 讲：现代 I/O：io_uring、DMA-BUF 与零拷贝流水线

## 概述

每当程序请求 OS 读写数据，都要交一笔税：**切入内核态的上下文切换**、内核内存与用户内存之间的**数据拷贝**，以及常常要等待慢速硬件。在中等数据速率下这笔税看不出来。在 AI 系统规模下——每秒数百万次 I/O 操作、每秒数 GB 的摄像头帧、持续不断的 GPU 推理——这部分开销会吞掉可用 CPU 的相当大一块。本讲就来处理这个问题。

贯穿本讲的心智模型是**拷贝链**：数据源自硬件（传感器、NIC、存储设备），目标是把它送到 GPU，而 CPU 尽量不必要地碰它。每一次拷贝、每一次 syscall、每一次上下文切换，都是潜在的消除目标。io_uring 消除存储 I/O 的 syscall 开销。DMA-BUF 消除内核子系统之间的拷贝。像 `sendfile`、DPDK 和 VisionIPC 这样的零拷贝技术消除其余每个阶段的拷贝。

AI 硬件工程师需要理解这些机制，因为实时感知流水线中的瓶颈往往**不是 GPU——而是喂给 GPU 的数据通路**。一帧摄像头图像花了 0.7 ms 在 PCIe 总线上被拷贝，就等于它晚了 0.7 ms 才到达推理。

---

## 传统 I/O 的局限

遗留的 **POSIX I/O 模型**带来的开销，在高吞吐 AI 数据流水线中会变成**瓶颈**。

- `read()`/`write()`：每次操作至少需要 2 次 syscall（发起 + 完成），外加一次内核到用户空间的数据拷贝
- `select()`/`poll()`：对文件描述符集合做 O(n) 扫描；随 fd 数量线性劣化
- `epoll`：消除了 O(n) 扫描，但每次事件通知仍需一次 syscall；在高 IOPS 下上下文切换开销不断累积

在 1M IOPS（NVMe 吞吐）下，仅 syscall 开销就能消耗 **30–50% 的 CPU 周期**。

> **关键洞见：** POSIX I/O API 是为正确性和可移植性设计的，不是为吞吐。每次 `read()` 调用都是一次往返：CPU 放下手头的事，进入内核态，拷贝数据，然后返回。在高 IOPS 下，CPU 花在这项开销上的时间比花在实际工作上的还多。

可以把它想象成一个仓库：每个箱子都必须亲手交给主管（内核），主管再把它交给送货司机（用户空间）。量小的时候没问题。每秒一百万个箱子时，主管就成了瓶颈。

---

## io_uring（Linux 5.1+）

io_uring 用**共享内存环形缓冲区**取代每次操作的 syscall，这些缓冲区对内核和用户空间同时可见。

### 环形缓冲区架构

有两个环驻留在同时映射进内核和用户空间的内存中：

- **SQE 环（Submission Queue Entry）**：应用在此写入操作描述符；内核读取它们
- **CQE 环（Completion Queue Entry）**：内核在此写入完成状态；应用无需 syscall 即可轮询

关键洞见在于，应用和内核都能**直接读写这些环**——正常操作无需跨越 syscall。

```
┌─────────────────────────────────────────────────────────────┐
│                    Shared Memory Region                      │
│                                                             │
│   SQ Ring (Submission Queue)        CQ Ring (Completion)   │
│  ┌──────────────────────────┐      ┌──────────────────────┐ │
│  │  SQE[0]: read fd=5       │      │ CQE[0]: res=512      │ │
│  │  SQE[1]: write fd=7      │      │ CQE[1]: res=0        │ │
│  │  SQE[2]: fsync fd=5      │      │ CQE[2]: res=512      │ │
│  │  SQE[3]: (empty)         │      │ CQE[3]: (empty)      │ │
│  └──────────────────────────┘      └──────────────────────┘ │
│        ↑ app writes here                ↑ kernel writes here │
│        ↓ kernel reads here             ↓ app polls here      │
└─────────────────────────────────────────────────────────────┘
         Userspace                          Kernel
         sees both rings ←── mmap ──→ sees both rings
```

> **关键洞见：** SQ 和 CQ 环位于对用户空间和内核同时可见的内存中。应用永远不需要“把数据交给内核”——它只是写入一个共享槽位。这就是 io_uring 在最激进的配置下能够做到零 syscall 的根本原因。


<details>
<summary>English original</summary>

**Lecture 19: Modern I/O: io_uring, DMA-BUF & Zero-Copy Pipelines**

**Overview**

Every time your program asks the OS to read or write data, it pays a tax: a **context switch into kernel mode**, a **data copy** between kernel and user memory, and often a wait for slow hardware. At modest data rates this tax is invisible. At AI-system scale — millions of I/O operations per second, gigabytes per second of camera frames, continuous GPU inference — this overhead consumes a significant fraction of available CPU. This lecture addresses that problem.

The mental model to carry through this lecture is the **copy chain**: data originates in hardware (a sensor, a NIC, a storage device), and the goal is to get it to the GPU without the CPU ever touching it unnecessarily. Every copy, every syscall, and every context switch is a potential elimination target. io_uring eliminates syscall overhead for storage I/O. DMA-BUF eliminates copies between kernel subsystems. Zero-copy techniques like `sendfile`, DPDK, and VisionIPC eliminate copies at every remaining stage.

AI hardware engineers need to understand these mechanisms because the bottleneck in a real-time perception pipeline is often **not the GPU — it is the data path feeding the GPU**. A camera frame that spends 0.7 ms being copied across the PCIe bus is a frame that arrived 0.7 ms late to inference.

---

**Traditional I/O Limitations**

The legacy **POSIX I/O model** imposes overhead that becomes a **bottleneck** in high-throughput AI data pipelines.

- `read()`/`write()`: each operation requires at least 2 syscalls (initiate + complete) plus a kernel-to-userspace data copy
- `select()`/`poll()`: O(n) scanning of file descriptor sets; degrades linearly with fd count
- `epoll`: eliminates O(n) scan but still requires one syscall per event notification; context switch cost accumulates at high IOPS

At 1M IOPS (NVMe throughput), syscall overhead alone can consume **30–50% of CPU cycles**.

> **Key Insight:** The POSIX I/O API was designed for correctness and portability, not for throughput. Each `read()` call is a round-trip: the CPU drops what it is doing, enters kernel mode, copies data, then returns. At high IOPS the CPU spends more time on this overhead than on actual work.

Think of it like a warehouse where every single box must be personally handed to a supervisor (kernel), who then hands it to the delivery driver (userspace). At low volumes this is fine. At a million boxes per second, the supervisor becomes the bottleneck.

---

**io_uring (Linux 5.1+)**

io_uring replaces per-operation syscalls with **shared memory ring buffers** visible to both kernel and userspace simultaneously.

**Ring Buffer Architecture**

Two rings reside in memory mapped into both kernel and userspace:

- **SQE ring (Submission Queue Entry)**: application writes operation descriptors here; kernel reads them
- **CQE ring (Completion Queue Entry)**: kernel writes completion status here; application polls without a syscall

The key insight is that both the application and the kernel can **read and write these rings directly** — no syscall crossing required for normal operation.

```
┌─────────────────────────────────────────────────────────────┐
│                    Shared Memory Region                      │
│                                                             │
│   SQ Ring (Submission Queue)        CQ Ring (Completion)   │
│  ┌──────────────────────────┐      ┌──────────────────────┐ │
│  │  SQE[0]: read fd=5       │      │ CQE[0]: res=512      │ │
│  │  SQE[1]: write fd=7      │      │ CQE[1]: res=0        │ │
│  │  SQE[2]: fsync fd=5      │      │ CQE[2]: res=512      │ │
│  │  SQE[3]: (empty)         │      │ CQE[3]: (empty)      │ │
│  └──────────────────────────┘      └──────────────────────┘ │
│        ↑ app writes here                ↑ kernel writes here │
│        ↓ kernel reads here             ↓ app polls here      │
└─────────────────────────────────────────────────────────────┘
         Userspace                          Kernel
         sees both rings ←── mmap ──→ sees both rings
```

> **Key Insight:** The SQ and CQ rings live in memory that is simultaneously visible to both userspace and the kernel. The application never needs to "hand data to the kernel" — it just writes to a shared slot. This is the fundamental reason io_uring can reach zero syscalls in its most aggressive configuration.

</details>

### 提交与完成流程

理解这一序列中的每一步，对于调优 io_uring 性能至关重要：

1. **应用调用 `io_uring_prep_read()`**：向一个 SQE 槽位填入操作描述符（fd、buffer、length、offset）。此时 kernel 尚未介入 —— 这只是用户态对 shared memory 的一次纯写入。
2. **应用调用 `io_uring_submit()`**：这可能触发 `io_uring_enter()`（用一次 syscall 提交 N 个操作的批）—— 或者在 SQPOLL 模式下，kernel 线程无需任何 syscall 就取走该 SQE。
3. **kernel 异步处理该操作**：I/O 被派发到块层、网络栈或文件系统。发起调用的线程可自由去做其他工作。
4. **kernel 将一个 CQE 写入 CQ ring**：完成结果（读取的字节数、错误码）被放入下一个可用的 CQ 槽位。这是对 shared memory 的一次写入 —— 无需向用户态发中断。
5. **应用轮询 CQ ring**：应用检查 CQ head 指针。若存在新的 CQE，就直接读取结果。**完成路径无需 syscall。**

使用 `IORING_SETUP_SQPOLL` 时，专用 kernel 线程持续排空 SQ ring。提交路径变为 **zero-syscall**。应用只需轮询 CQ ring。

> **常见陷阱：** 处理完一个完成项后忘记调用 `io_uring_cqe_seen()`。该调用会推进 CQ ring 的 head 指针。若不调用，ring 会被填满，新的完成事件被丢弃（除非设置了 `IORING_FEAT_NODROP`，此时改为施加背压），应用随即停滞。

### 关键标志与特性

| 标志 / 特性 | 含义 |
|---|---|
| `IORING_SETUP_SQPOLL` | kernel 线程自动提交；零 syscall 提交路径 |
| `IORING_SETUP_IOPOLL` | kernel 轮询完成事件（无 IRQ）；延迟最低 |
| `IORING_FEAT_NODROP` | CQE 永不丢弃；以背压代替丢失 |
| 固定缓冲区 | 预注册的缓冲区跳过每个操作的 `get_user_pages()` |
| Multishot | 单个 SQE 产生多个 CQE（例如 accept 循环） |

### 支持的操作

`read`, `write`, `send`, `recv`, `accept`, `connect`, `fsync`, `splice`, `openat`, `statx`, `timeout`, `link`, `hardlink`, `renameat`, `unlinkat`

支持的操作范围相当广：io_uring 不只是存储优化 —— 它能替代网络或文件服务器中几乎每一个阻塞式 I/O syscall。

### liburing 辅助库

```c
struct io_uring ring;
io_uring_queue_init(256, &ring, 0);          // init with depth 256 SQEs

struct io_uring_sqe *sqe = io_uring_get_sqe(&ring); // get a free SQE slot
io_uring_prep_read(sqe, fd, buf, len, 0);            // fill the SQE descriptor
io_uring_sqe_set_data(sqe, user_data_ptr);           // tag with user context pointer

io_uring_submit(&ring);                      // single syscall submits ALL pending SQEs

struct io_uring_cqe *cqe;
io_uring_wait_cqe(&ring, &cqe);             // blocks until one CQE ready (or poll)
// cqe->res contains bytes read, or negative errno on error
io_uring_cqe_seen(&ring, cqe);              // advance CQ head — MUST call this
```

`io_uring_submit()` 调用是批量的：自上次提交以来准备的所有 SQE 都在一次 syscall 中发送。在调用 submit 之前把大量读或写组织成流水线的应用，可获得接近零的 syscall 开销。

### 性能对比

| 方法 | IOPS（NVMe 4K 随机读） | CPU 利用率 | 每操作 syscall 数 |
|---|---|---|---|
| `pread()` | ~200K | 高 | 1 |
| `epoll` + `aio` | ~600K | 中等 | 2 |
| `io_uring`（默认） | ~800K | 低 | ~0.1（批量） |
| `io_uring` + SQPOLL | 1M+ | 接近零 | 0 |

> **关键洞察：** 从 200K IOPS 的 `pread()` 跃升到 1M+ IOPS 的 `io_uring` + SQPOLL，几乎完全是 CPU 开销被消除的结果，而非硬件加速。所有行中的 NVMe SSD 都是同一块。变化的是有多少 CPU 时间被浪费在 syscall 和拷贝上。

---


<details>
<summary>English original</summary>

**Submission and Completion Flow**

Understanding each step in this sequence is essential for tuning io_uring performance:

1. **Application calls `io_uring_prep_read()`**: fills one SQE slot with the operation descriptor (fd, buffer, length, offset). No kernel involvement yet — this is a pure userspace write to shared memory.
2. **Application calls `io_uring_submit()`**: this may call `io_uring_enter()` (one syscall for a batch of N operations) — or in SQPOLL mode, the kernel thread picks up the SQE without any syscall at all.
3. **Kernel processes the operation asynchronously**: I/O is dispatched to the block layer, network stack, or file system. The calling thread is free to do other work.
4. **Kernel writes a CQE to the CQ ring**: the completion result (bytes read, error code) is placed in the next available CQ slot. This is a write into shared memory — no interrupt to userspace required.
5. **Application polls the CQ ring**: the app checks the CQ head pointer. If a new CQE is present, it reads the result directly. **No syscall required for completion.**

With `IORING_SETUP_SQPOLL`, a dedicated kernel thread continuously drains the SQ ring. The submit path becomes **zero-syscall**. The application only polls the CQ ring.

> **Common Pitfall:** Forgetting to call `io_uring_cqe_seen()` after processing a completion entry. This advances the CQ ring head pointer. Without it, the ring fills up, new completions are dropped (unless `IORING_FEAT_NODROP` is set, which applies backpressure instead), and the application stalls.

**Key Flags and Features**

| Flag / Feature | Meaning |
|---|---|
| `IORING_SETUP_SQPOLL` | Kernel thread auto-submits; zero syscall submit path |
| `IORING_SETUP_IOPOLL` | Kernel polls for completions (no IRQ); lowest latency |
| `IORING_FEAT_NODROP` | CQEs never dropped; backpressure instead of loss |
| Fixed buffers | Pre-registered buffers skip `get_user_pages()` per op |
| Multishot | Single SQE generates multiple CQEs (e.g., accept loop) |

**Supported Operations**

`read`, `write`, `send`, `recv`, `accept`, `connect`, `fsync`, `splice`, `openat`, `statx`, `timeout`, `link`, `hardlink`, `renameat`, `unlinkat`

The breadth of supported operations is significant: io_uring is not just a storage optimization — it can replace nearly every blocking I/O syscall in a network or file server.

**liburing Helper Library**

```c
struct io_uring ring;
io_uring_queue_init(256, &ring, 0);          // init with depth 256 SQEs

struct io_uring_sqe *sqe = io_uring_get_sqe(&ring); // get a free SQE slot
io_uring_prep_read(sqe, fd, buf, len, 0);            // fill the SQE descriptor
io_uring_sqe_set_data(sqe, user_data_ptr);           // tag with user context pointer

io_uring_submit(&ring);                      // single syscall submits ALL pending SQEs

struct io_uring_cqe *cqe;
io_uring_wait_cqe(&ring, &cqe);             // blocks until one CQE ready (or poll)
// cqe->res contains bytes read, or negative errno on error
io_uring_cqe_seen(&ring, cqe);              // advance CQ head — MUST call this
```

The `io_uring_submit()` call is batched: all SQEs prepared since the last submit are sent in a single syscall. Applications that pipeline many reads or writes before calling submit achieve near-zero syscall overhead.

**Performance Comparison**

| Method | IOPS (NVMe 4K rand read) | CPU utilization | Syscalls per op |
|---|---|---|---|
| `pread()` | ~200K | high | 1 |
| `epoll` + `aio` | ~600K | moderate | 2 |
| `io_uring` (default) | ~800K | low | ~0.1 (batched) |
| `io_uring` + SQPOLL | 1M+ | near-zero | 0 |

> **Key Insight:** The jump from `pread()` at 200K IOPS to `io_uring` + SQPOLL at 1M+ IOPS is almost entirely CPU overhead elimination, not hardware speedup. The NVMe SSD is the same in all rows. What changes is how much CPU time is wasted on syscalls and copies.

---

</details>

## DMA-BUF：跨子系统缓冲区共享

io_uring 解决了存储的 syscall 开销后，下一个瓶颈是 kernel 子系统之间的数据拷贝——例如 camera 驱动与 GPU 之间。DMA-BUF 解决了这个问题。

DMA-BUF 为内存缓冲区提供了一层 **file descriptor 抽象**，使多个 kernel 子系统与用户态能够**不拷贝地共享**。

- 一个子系统分配缓冲区，并通过 `dma_buf_export()` → `dma_buf_fd()` 将其导出为 fd
- 另一个子系统通过 `dma_buf_get()` 导入该 fd，并把它映射进自己的 DMA 地址空间
- 用户态通过 `sendmsg()` 或直接的 API 调用在进程之间传递 fd

把 DMA-BUF fd 看作储物柜的钥匙。camera 驱动把数据放进储物柜。GPU 驱动用同一把钥匙打开同一个储物柜。没有任何数据被搬动——传递的只是钥匙。

```
┌──────────────┐   exports fd    ┌─────────────────────────────────┐
│ GPU Allocator│ ─────────────→  │       DMA-BUF Object             │
│ (nvmap/ION)  │                 │  (physical pages in device mem)  │
└──────────────┘                 └──────────────────────────────────┘
                                          ↑            ↑
                               imports   │            │ imports
                               ┌─────────┘            └──────────┐
                          ┌────┴─────┐               ┌───────────┴──┐
                          │ V4L2     │               │ CUDA kernel  │
                          │ camera   │               │ (device ptr) │
                          │ DMA eng. │               └──────────────┘
                          └──────────┘
                          ISP writes                 GPU reads
                          directly here              directly here
                                ↕ NO CPU COPY ↕
```

### V4L2 + DMA-BUF 集成

- `V4L2_MEMORY_DMABUF`：接受外部 DMA-BUF fd 的 V4L2 缓冲区类型
- 应用从 GPU 分配器（nvmap、ION 或 DRM 分配器）导出 DMA-BUF fd
- 通过 `VIDIOC_QBUF` 把 fd 传给 camera 驱动，并设置 `m.fd`
- camera DMA 引擎将采集到的帧直接写入 GPU 可访问的内存
- `VIDIOC_EXPBUF`：把 V4L2 `MMAP` 缓冲区导出为 DMA-BUF fd，供其它设备导入

> **关键洞察：** `V4L2_MEMORY_DMABUF` 反转了常规流程。不再由 camera 驱动分配自己的缓冲区再拷贝出去，而是由应用提供一个缓冲区，camera 引擎直接写入其中。应用选定的缓冲区同时对 GPU 可见，使拷贝在物理上不可能发生（数据只有一份，位于同一个位置）。

---

## 零拷贝 camera 到推理流水线（Jetson）

现在把 io_uring 与 DMA-BUF 组合成完整的零拷贝流水线。本节展示具体的收益：从每一帧 camera 图像中消除 1–2 次内存拷贝。

传统流水线：Camera → DMA → kernel 缓冲区 → 拷贝到用户态 → 拷贝到 GPU（**多出 2 次拷贝**）。

基于 DMA-BUF 的零拷贝流水线：

1. **GPU 分配缓冲区**：`cudaMalloc()` → nvmap handle → `dma_buf_fd()`。该缓冲区从一开始就位于 GPU 可访问的内存中。
2. **把缓冲区 fd 交给 camera（V4L2）**：`VIDIOC_QBUF` 配合 `V4L2_MEMORY_DMABUF`；fd 传给 camera 驱动。camera 驱动此时确切知道该把帧写到哪里。
3. **camera ISP DMA 引擎直接写入该 GPU 映射的缓冲区**：camera 硬件把采集到的帧 DMA 进 GPU 缓冲区。这次传输不涉及 CPU。
4. **CUDA 推理**：缓冲区已在 GPU 内存中；不需要 `cudaMemcpy()`。CUDA kernel 从第 1 步分配的指针读取这一帧。
5. **通过 `cudaGraphicsMapResources()` 或直接的 device pointer 访问**：标准 CUDA API 照常工作；只是它们操作的数据是在没有任何 CPU 侧拷贝的情况下到达的。

延迟降低：消除 1–2 次 CPU-GPU memcpy 操作。在 PCIe Gen3 带宽（~12 GB/s）下，拷贝一帧 1080p RGBA（~8 MB）耗时 ~0.7 ms。在 3 路 camera、30 fps 的情况下，消除的拷贝节省了可观的内存带宽。

> **常见陷阱：** 常见错误是用 `malloc()` 分配 camera 缓冲区，之后才试图通过 DMA-BUF 导入它。缓冲区必须从一开始就通过 GPU 可见的分配器（nvmap、ION 或 `cudaMallocManaged`）分配。被 `malloc()` 过的缓冲区没有 DMA-BUF handle，不能作为目标传给 camera 驱动。

---


<details>
<summary>English original</summary>

**DMA-BUF: Cross-Subsystem Buffer Sharing**

With io_uring handling syscall overhead for storage, the next bottleneck is copying data between kernel subsystems — for example, between a camera driver and a GPU. DMA-BUF solves this.

DMA-BUF provides a **file descriptor abstraction** for memory buffers that multiple kernel subsystems and userspace can **share without copying**.

- One subsystem allocates a buffer and exports it as an fd via `dma_buf_export()` → `dma_buf_fd()`
- Another subsystem imports the fd via `dma_buf_get()` and maps it into its own DMA address space
- Userspace passes fds between processes via `sendmsg()` or direct API calls

Think of a DMA-BUF fd as a key to a locker. The camera driver puts data in the locker. The GPU driver opens the same locker with the same key. No data is moved — only the key is passed.

```
┌──────────────┐   exports fd    ┌─────────────────────────────────┐
│ GPU Allocator│ ─────────────→  │       DMA-BUF Object             │
│ (nvmap/ION)  │                 │  (physical pages in device mem)  │
└──────────────┘                 └──────────────────────────────────┘
                                          ↑            ↑
                               imports   │            │ imports
                               ┌─────────┘            └──────────┐
                          ┌────┴─────┐               ┌───────────┴──┐
                          │ V4L2     │               │ CUDA kernel  │
                          │ camera   │               │ (device ptr) │
                          │ DMA eng. │               └──────────────┘
                          └──────────┘
                          ISP writes                 GPU reads
                          directly here              directly here
                                ↕ NO CPU COPY ↕
```

**V4L2 + DMA-BUF Integration**

- `V4L2_MEMORY_DMABUF`: V4L2 buffer type that accepts external DMA-BUF fds
- Application exports a DMA-BUF fd from GPU allocator (nvmap, ION, or DRM allocator)
- Passes fd to camera driver via `VIDIOC_QBUF` with `m.fd` set
- Camera DMA engine writes captured frame directly to GPU-accessible memory
- `VIDIOC_EXPBUF`: export a V4L2 `MMAP` buffer as a DMA-BUF fd for import by another device

> **Key Insight:** `V4L2_MEMORY_DMABUF` inverts the normal flow. Instead of the camera driver allocating its own buffer and then copying out, the application provides a buffer that the camera engine writes into directly. The application chooses a buffer that is also visible to the GPU, making the copy physically impossible (there is only one copy of the data, in one location).

---

**Zero-Copy Camera to Inference Pipeline (Jetson)**

Now we combine io_uring and DMA-BUF into the full zero-copy pipeline. This section shows the concrete benefit: eliminating 1–2 memory copies from every camera frame.

Traditional pipeline: Camera → DMA → kernel buffer → copy to userspace → copy to GPU (**2 extra copies**).

Zero-copy pipeline via DMA-BUF:

1. **GPU allocates buffer**: `cudaMalloc()` → nvmap handle → `dma_buf_fd()`. The buffer lives in GPU-accessible memory from the start.
2. **Camera (V4L2) is given the buffer fd**: `VIDIOC_QBUF` with `V4L2_MEMORY_DMABUF`; fd passed to camera driver. The camera driver now knows exactly where to write the frame.
3. **Camera ISP DMA engine writes directly into that GPU-mapped buffer**: the camera hardware DMAs the captured frame into the GPU buffer. No CPU is involved in this transfer.
4. **CUDA inference**: buffer already in GPU memory; no `cudaMemcpy()` needed. The CUDA kernel reads the frame from the pointer that was allocated in step 1.
5. **Access via `cudaGraphicsMapResources()` or direct device pointer**: standard CUDA APIs work normally; they just happen to be operating on data that arrived without any CPU-side copy.

Latency reduction: eliminates 1–2 CPU-GPU memcpy operations. At PCIe Gen3 bandwidth (~12 GB/s), copying a 1080p RGBA frame (~8 MB) costs ~0.7 ms. At 30 fps with 3 cameras, eliminated copies save significant memory bandwidth.

> **Common Pitfall:** A common mistake is allocating the camera buffer with `malloc()` and only later trying to import it via DMA-BUF. The buffer must be allocated through the GPU-visible allocator (nvmap, ION, or `cudaMallocManaged`) from the start. A `malloc()`'d buffer has no DMA-BUF handle and cannot be passed to the camera driver as a target.

---

</details>

## splice 与 sendfile：kernel 到 kernel 零拷贝

一旦数据进入 kernel，`splice` 与 `sendfile` 允许其在 kernel 子系统之间移动，**而无需暴露到用户空间**。

这两个调用都在文件描述符之间移动数据，且不拷贝到用户空间：

- `sendfile(out_fd, in_fd, &offset, count)`：在 kernel 内部将数据从文件或 socket 拷贝到 socket；用于 HTTP 视频流与静态文件服务
- `splice(fd_in, off_in, fd_out, off_out, len, flags)`：更通用；可与管道配合；可跨调用链式组合
- `tee(fd_in, fd_out, len, flags)`：复制管道数据而不消费它

用例：摄像头录制服务器通过 HTTP 发送 H.264 帧，无需用户空间缓冲区拷贝。

> **关键洞察：** `sendfile` 是 Web 服务器能够以接近线速提供 1 GB 视频文件、且几乎不耗 CPU 的原因。数据流向为：NVMe → 页缓存 → NIC DMA，全程无需跨越用户/kernel 边界。服务器进程发起一次系统调用，其余由硬件处理。

---

## DPDK：用户空间网络驱动

在推理服务场景中，网络 I/O 必须匹配 GPU 吞吐，此时连 kernel 网络栈的开销都变得不可接受。DPDK 将其彻底消除。

DPDK（Data Plane Development Kit）**完全绕过 kernel 网络栈**：

- 用户空间 PMD（Poll Mode Driver）独占 NIC；无 kernel 驱动，无中断
- 大页（2 MB/1 GB）用于包缓冲区：消除线速下的 TLB 未命中
- CPU 核 100% 轮询 —— 单核即可达到 100 Gbps
- 无上下文切换，无系统调用，无 socket 缓冲区拷贝
- 用于网络 I/O 必须匹配 GPU 吞吐的推理服务前端

工具：`dpdk-testpmd` 用于 benchmark；`rte_mbuf` 用于零拷贝包缓冲区；`rte_ring` 用于核间包传递。

> **常见陷阱：** DPDK 将一个 CPU 核专门用于 100% 轮询。这是有意为之 —— 这是近乎零延迟网络 I/O 的代价。在共享核上运行 DPDK，或在该核上与其他工作负载并行运行，会破坏其延迟保证。DPDK 的核必须在 kernel 启动参数中用 `isolcpus` 隔离。

---

## VisionIPC：openpilot 零拷贝视频 IPC

零拷贝拼图的最后一块，是在进程之间传递视频帧而不做拷贝。VisionIPC 在生产级别展示了这一点。

openpilot 用**共享内存**取代了基于 socket 的视频传输：

- `vipc_server` 在启动时通过 `mmap(MAP_SHARED)` 分配一组共享内存缓冲区
- `camerad`（生产者）：填充缓冲区，通过信号量向消费者发布缓冲区索引
- `modeld` 与 `encoderd`（消费者）：接收索引，映射同一内存区域，直接读取
- 进程之间无视频数据拷贝；只传递一个小整数（缓冲区索引）
- `cereal` 通过 `msgq`（同样是共享内存）上的 capnproto 处理所有其他 IPC（非视频）

```
┌────────────┐  fills buffer[N]   ┌─────────────────────────────┐
│  camerad   │ ──────────────────→│  Shared Memory Buffer Pool  │
│ (producer) │  posts index N     │  buf[0]: 1920×1208 YUV      │
└────────────┘  via semaphore     │  buf[1]: 1920×1208 YUV      │
                      ↓           │  buf[2]: 1920×1208 YUV      │
             ┌────────┴────────┐  └─────────────────────────────┘
             │                 │           ↑          ↑
        ┌────▼─────┐   ┌───────▼────┐    reads      reads
        │  modeld  │   │  encoderd  │  buffer[N]   buffer[N]
        │(inference│   │(H.265 enc.)│  directly    directly
        └──────────┘   └────────────┘
         GPU kernel     encoder DMA
         reads buf[N]   reads buf[N]
         NO COPY        NO COPY
```

> **关键洞察：** VisionIPC 展示了本讲每个概念在真实世界中的应用。摄像头只填充一次缓冲区。多个消费者 —— 神经网络推理引擎与视频编码器 —— 从同一缓冲区读取。GPU 就地处理该帧。数据从不被复制。这之所以可行，是因为 OS 共享内存原语（`mmap(MAP_SHARED)`、DMA-BUF）允许多个子系统同时引用同一块物理内存。

---

## 总结

| I/O 方式 | 每次操作的系统调用 | 数据拷贝 | 最大吞吐 | 用例 |
|---|---|---|---|---|
| `read()`/`write()` | 1 | kernel 到用户 | ~200K IOPS | 简单文件 I/O |
| `epoll` + 回调 | 每事件 1 次 | kernel 到用户 | ~600K IOPS | 网络服务器 |
| `io_uring` 批量 | ~0.1（批量） | kernel 到用户 | ~800K IOPS | NVMe 日志 |
| `io_uring` SQPOLL | 0 | kernel 到用户 | 1M+ IOPS | 超低延迟 |
| `sendfile` | 1 | 无（kernel） | 线速 | 视频流 |
| DMA-BUF + V4L2 | 0（DMA） | 无 | 硬件 DMA 速率 | 摄像头到 GPU |
| DPDK | 0 | 无 | 100 Gbps | 推理服务 |
| VisionIPC | 0（共享内存） | 无 | 内存带宽 | openpilot 摄像头 IPC |


<details>
<summary>English original</summary>

**splice and sendfile: Kernel-to-Kernel Zero-Copy**

Once data is in the kernel, `splice` and `sendfile` allow it to be moved between kernel subsystems **without ever surfacing in userspace**.

Both calls move data between file descriptors without copying to userspace:

- `sendfile(out_fd, in_fd, &offset, count)`: copy from file or socket to socket inside kernel; used for HTTP video streaming and static file serving
- `splice(fd_in, off_in, fd_out, off_out, len, flags)`: more general; works with pipes; can chain across calls
- `tee(fd_in, fd_out, len, flags)`: duplicate pipe data without consuming it

Use case: camera recording server sending H.264 frames over HTTP without a userspace buffer copy.

> **Key Insight:** `sendfile` is the reason a web server can serve a 1 GB video file at near-wire speed using almost no CPU. The data travels: NVMe → page cache → NIC DMA, all without crossing the user/kernel boundary. The server process issues a single syscall and the hardware handles the rest.

---

**DPDK: User-Space Network Driver**

For inference-serving scenarios where network I/O must match GPU throughput, even kernel network stack overhead becomes unacceptable. DPDK eliminates it entirely.

DPDK (Data Plane Development Kit) **bypasses the kernel network stack** entirely:

- User-space PMD (Poll Mode Driver) owns the NIC; no kernel driver, no interrupts
- Huge pages (2 MB/1 GB) for packet buffers: eliminates TLB misses at line rate
- CPU core polling at 100% — achieves 100 Gbps with a single core
- No context switches, no syscalls, no socket buffer copies
- Used in inference-serving front-ends where network I/O must match GPU throughput

Tools: `dpdk-testpmd` for benchmarking; `rte_mbuf` for zero-copy packet buffers; `rte_ring` for inter-core packet passing.

> **Common Pitfall:** DPDK dedicates a CPU core to 100% polling. This is intentional — it is the price of near-zero latency network I/O. Running DPDK on a shared core, or alongside other workloads on that core, destroys its latency guarantees. DPDK cores must be isolated with `isolcpus` in the kernel boot parameters.

---

**VisionIPC: openpilot Zero-Copy Video IPC**

The final piece of the zero-copy puzzle is passing video frames between processes without copying. VisionIPC demonstrates this at a production level.

openpilot replaces socket-based video transfer with **shared memory**:

- `vipc_server` allocates a pool of shared memory buffers at startup via `mmap(MAP_SHARED)`
- `camerad` (producer): fills a buffer, posts the buffer index to consumers via semaphore
- `modeld` and `encoderd` (consumers): receive the index, map the same memory region, read directly
- No video data copy between processes; only a small integer (buffer index) is communicated
- `cereal` handles all other IPC (non-video) via capnproto over `msgq` (also shared memory)

```
┌────────────┐  fills buffer[N]   ┌─────────────────────────────┐
│  camerad   │ ──────────────────→│  Shared Memory Buffer Pool  │
│ (producer) │  posts index N     │  buf[0]: 1920×1208 YUV      │
└────────────┘  via semaphore     │  buf[1]: 1920×1208 YUV      │
                      ↓           │  buf[2]: 1920×1208 YUV      │
             ┌────────┴────────┐  └─────────────────────────────┘
             │                 │           ↑          ↑
        ┌────▼─────┐   ┌───────▼────┐    reads      reads
        │  modeld  │   │  encoderd  │  buffer[N]   buffer[N]
        │(inference│   │(H.265 enc.)│  directly    directly
        └──────────┘   └────────────┘
         GPU kernel     encoder DMA
         reads buf[N]   reads buf[N]
         NO COPY        NO COPY
```

> **Key Insight:** VisionIPC shows the real-world application of every concept in this lecture. The camera fills a buffer once. Multiple consumers — the neural network inference engine and the video encoder — read from that same buffer. The GPU processes the frame in-place. No data is ever duplicated. This is achievable because the OS shared memory primitives (`mmap(MAP_SHARED)`, DMA-BUF) allow multiple subsystems to reference the same physical memory simultaneously.

---

**Summary**

| I/O Method | Syscall per op | Data copy | Max throughput | Use case |
|---|---|---|---|---|
| `read()`/`write()` | 1 | kernel to user | ~200K IOPS | Simple file I/O |
| `epoll` + callbacks | 1 per event | kernel to user | ~600K IOPS | Network servers |
| `io_uring` batched | ~0.1 (batched) | kernel to user | ~800K IOPS | NVMe logging |
| `io_uring` SQPOLL | 0 | kernel to user | 1M+ IOPS | Ultra-low latency |
| `sendfile` | 1 | none (kernel) | line rate | Video streaming |
| DMA-BUF + V4L2 | 0 (DMA) | none | hardware DMA rate | Camera to GPU |
| DPDK | 0 | none | 100 Gbps | Inference serving |
| VisionIPC | 0 (shared mem) | none | memory bandwidth | openpilot camera IPC |

</details>

### 概念回顾

- **传统的 `read()` 调用的根本开销是什么？** 两次模式切换（用户态→内核态→用户态），加上一次从内核缓冲区到用户缓冲区的数据拷贝。在 1M IOPS 下，这一开销可消耗 30–50% 的可用 CPU 周期。

- **io_uring 的环形缓冲区如何消除系统调用？** 通过把提交队列和完成队列放在同时映射进内核地址空间与用户地址空间的内存中，应用无需进入内核态就能提交工作并读取结果。使用 SQPOLL 时，一个内核线程持续排空提交环。

- **DMA-BUF 解决了哪些 io_uring 解决不了的问题？** io_uring 降低的是既有 I/O 路径上的系统调用开销。DMA-BUF 消除的是内核子系统之间的整段拷贝——相机驱动、GPU 驱动和显示驱动都能通过一个文件描述符引用同一块物理缓冲区，因此一方写入的数据另一方无需任何拷贝即可立即读取。

- **为什么零拷贝的相机到 GPU 流水线要求由 GPU 分配缓冲区，而不是相机驱动？** 缓冲区必须对 GPU 硬件可见（映射进 GPU 地址空间）。只有 GPU 分配器（nvmap、ION）才能产出同时带有 DMA-BUF fd（给相机驱动）和 CUDA 设备指针（给推理）的缓冲区。如果由相机驱动分配缓冲区，它位于内核内存中，没有 GPU 映射。

- **DPDK 轮询模型的取舍是什么？** 一个专用 CPU 核持续以 100% 利用率运行。这消除了所有中断和上下文切换开销，实现线速包处理。代价是一个完整 CPU 核被永久占用。在网卡核与大量 GPU 核配对的推理服务系统中，这是可以接受的。

- **VisionIPC 如何防止多个进程争用同一缓冲区？** 信号量（由 `camerad` 在填满缓冲区后发出）发出就绪信号。消费者根据通过信号量收到的缓冲区编号，从共享内存区域中索引读取。在所有消费者确认之前，缓冲区不会归还到池中，从而防止 camerad 覆盖仍被 modeld 或 encoderd 使用的缓冲区。

---

## AI 硬件关联

- DMA-BUF 配合 `V4L2_MEMORY_DMABUF` 可在 Jetson 上实现真正的零拷贝相机到 GPU 流水线；帧从 ISP 直接到达 GPU 内存，数据路径上完全没有 CPU 参与
- io_uring 配合 `IORING_SETUP_SQPOLL` 适用于 openpilot 中的高吞吐 CAN 总线和传感器日志记录，以近乎为零的 CPU 开销实现 1M+ IOPS
- VisionIPC 展示了消除生产级自动驾驶软件中进程间视频拷贝的 OS 级共享内存设计
- DPDK 用于云推理前端，此处 100 Gbps 的网络 I/O 必须在无内核开销的前提下与 GPU 吞吐相匹配
- `sendfile` 使相机流中继服务器能够把编码后的视频转发给客户端，完全不涉及用户态缓冲区
- 理解完整的拷贝链路（相机 ISP → 内核缓冲区 → 用户态 → GPU）是诊断任何 AI 感知流水线延迟的前置要求

---


<details>
<summary>English original</summary>

**Conceptual Review**

- **What is the fundamental cost of a traditional `read()` call?** Two mode switches (user→kernel→user) plus one data copy from kernel buffer to user buffer. At 1M IOPS this overhead can consume 30–50% of available CPU cycles.

- **How does io_uring's ring buffer eliminate syscalls?** By placing the submission and completion queues in memory that is simultaneously mapped into both kernel and user address spaces, the application can post work and read results without ever entering kernel mode. With SQPOLL, a kernel thread drains the submission ring continuously.

- **What problem does DMA-BUF solve that io_uring does not?** io_uring reduces syscall overhead for existing I/O paths. DMA-BUF eliminates entire copies between kernel subsystems — the camera driver, GPU driver, and display driver can all reference the same physical buffer via a file descriptor, so data written by one is immediately readable by another without any copy.

- **Why does the zero-copy camera-to-GPU pipeline require the GPU to allocate the buffer, not the camera driver?** The buffer must be visible to GPU hardware (mapped into GPU address space). Only the GPU allocator (nvmap, ION) produces a buffer with both a DMA-BUF fd (for the camera driver) and a CUDA device pointer (for inference). If the camera driver allocates the buffer, it lives in kernel memory with no GPU mapping.

- **What is the trade-off of DPDK's polling model?** A dedicated CPU core runs at 100% utilization continuously. This eliminates all interrupt and context-switch overhead, achieving line-rate packet processing. The cost is one full CPU core permanently consumed. This is acceptable in inference-serving systems where the NIC core is paired with many GPU cores.

- **How does VisionIPC prevent multiple processes from racing on the same buffer?** A semaphore (posted by `camerad` after filling a buffer) signals readiness. Consumers read from the shared memory region indexed by the buffer number received via the semaphore. The buffer is not returned to the pool until all consumers acknowledge, preventing camerad from overwriting a buffer still in use by modeld or encoderd.

---

**AI Hardware Connection**

- DMA-BUF with `V4L2_MEMORY_DMABUF` enables true zero-copy camera-to-GPU pipelines on Jetson; frames arrive in GPU memory directly from the ISP with no CPU involvement in the data path
- io_uring with `IORING_SETUP_SQPOLL` is applicable to high-throughput CAN bus and sensor logging in openpilot, achieving 1M+ IOPS with near-zero CPU cost
- VisionIPC demonstrates the OS-level shared memory design that eliminates inter-process video copies in production autonomous driving software
- DPDK is used in cloud inference front-ends where network I/O at 100 Gbps must be matched to GPU throughput without kernel overhead
- `sendfile` enables camera stream relay servers to forward encoded video to clients with zero userspace buffer involvement
- Understanding the full copy chain (camera ISP → kernel buffer → userspace → GPU) is prerequisite for diagnosing latency in any AI perception pipeline

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-19.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-19.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
