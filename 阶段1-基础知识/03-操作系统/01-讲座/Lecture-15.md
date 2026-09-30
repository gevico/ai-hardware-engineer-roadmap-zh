---
title: 第 15 讲：DMA、IOMMU 与 GPU 内存管理
description: 第 15 讲：DMA、IOMMU 与 GPU 内存管理
---

# 第 15 讲：DMA、IOMMU 与 GPU 内存管理

## 概述

现代 AI 系统依赖于在 CPU、GPU、摄像头 ISP（图像信号处理器）和存储之间高速搬运大量数据——摄像头帧、模型权重、激活值。通过 CPU 完成这项工作（在软件中拷贝数据）会慢得不可行，并浪费推理所需的 CPU 周期。**直接内存访问（DMA）** 让硬件引擎无需 CPU 参与即可自主传输数据。核心挑战是确保 CPU 的缓存与设备始终看到一致的数据。本讲的思维模型是一条生产流水线：原始传感器数据进入内存，依次由多个硬件引擎处理，最终到达推理引擎——理想情况下无需任何软件拷贝。对于 AI 硬件工程师，掌握 DMA 与 IOMMU 是设计零拷贝的摄像头到推理流水线、编写正确的 PCIe 设备驱动，以及理解为什么 Jetson 的统一内存架构行为与独立 GPU 不同的关键。

---

## DMA：直接内存访问

**DMA** 允许设备（NIC、NVMe 控制器、GPU PCIe DMA 引擎、摄像头 ISP）在数据路径中 **无需 CPU 参与** 即可直接向系统 RAM 传输数据或从系统 RAM 传输数据。CPU 编程一个 **描述符**，指定源地址、目标地址、传输长度和标志；设备自主执行传输；完成通过中断或驱动轮询的状态标志来通知。

优势：在批量传输期间 CPU 被释放；传输延迟与 CPU 计算重叠；系统吞吐增加。

```
Without DMA (CPU-copy path):
Device → [device buffer] → CPU reads → CPU writes → [system RAM]
           (device generates data)    (CPU memcpy)   (destination)
CPU is fully occupied during the entire transfer.

With DMA:
Device → DMA engine → [system RAM]
CPU submits one descriptor, then is free to do other work.
Completion interrupt notifies CPU when transfer is done.

┌──────────────┐   DMA descriptor    ┌─────────────┐
│   Device /   │ ─────────────────>  │  DMA Engine │
│  Controller  │   (src, dst, len)   │             │
│              │ <─────────────────  │             │
│              │   IRQ on complete   └──────┬──────┘
└──────────────┘                            │ bus master
                                            ▼
                                     ┌─────────────┐
                                     │  System RAM │
                                     └─────────────┘
```

---

## 缓存一致性问题

现代 CPU 将数据缓存在 L1/L2/L3 中。当设备写入 RAM（DMA 写）时，CPU 可能持有 **过期的缓存副本**。当设备从 RAM 读取（DMA 读）时，设备可能读到 CPU 已在缓存中修改但尚未写回的数据。**两种编程模型** 解决此问题：

```
Cache Coherency Hazard (DMA write, device→CPU):

[Device DMA writes new data to PA 0x1000]
    ↓
[RAM at 0x1000] = new value
    ↓ (but)
[CPU L1/L2 cache for VA→0x1000] = old stale value  ← BUG if CPU reads here

Solution A (Coherent DMA): hardware coherency fabric or non-cached mapping
Solution B (Streaming DMA): driver explicitly invalidates cache before CPU reads
```

### 一致（连贯）DMA

`dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL)`

- 返回物理连续的 CPU 虚拟地址以及设备可见的 `dma_handle`
- 由 `dma_alloc_coherent` 分配的内存 **不可缓存** *仅针对该特定 DMA 缓冲区*，以确保 CPU 与设备始终看到相同的数据。当 CPU 访问此缓冲区时，其数据不被缓存。
- 重要的是，CPU *仍可缓存所有其他正常内存区域的数据*——不可缓存设置仅适用于一致 DMA 分配返回的缓冲区。所有其他（非 DMA）内存使用正常的 CPU 缓存，不受影响。
- 由于对一致 DMA 缓冲区的 CPU 访问是非缓存的，读写较慢；将此类内存用于小型控制环、描述符表或状态块等正确性比速度更重要的场景。


<details>
<summary>English original</summary>

**Lecture 15: DMA, IOMMU & GPU Memory Management**

**Overview**

Modern AI systems depend on moving large amounts of data — camera frames, model weights, activations — between CPU, GPU, camera ISP, and storage at high speed. Doing this through the CPU (copying data in software) would be impossibly slow and waste CPU cycles needed for inference. **Direct Memory Access (DMA)** lets hardware engines transfer data autonomously without CPU involvement. The core challenge is ensuring the CPU's caches and the device always see consistent data. The mental model for this lecture is a production pipeline: raw sensor data enters memory, gets processed by multiple hardware engines in sequence, and reaches the inference engine — ideally without a single software copy. For an AI hardware engineer, mastering DMA and the IOMMU is essential for designing zero-copy camera-to-inference pipelines, writing correct PCIe device drivers, and understanding why Jetson's unified memory architecture behaves differently from a discrete GPU.

---

**DMA: Direct Memory Access**

**DMA** allows a device (NIC, NVMe controller, GPU PCIe DMA engine, camera ISP) to transfer data directly to or from system RAM **without CPU participation** in the data path. The CPU programs a **descriptor** specifying source address, destination address, transfer length, and flags; the device executes the transfer autonomously; completion is signaled via an interrupt or a status flag the driver polls.

Benefits: CPU is freed during bulk transfers; transfer latency overlaps with CPU computation; system throughput increases.

```
Without DMA (CPU-copy path):
Device → [device buffer] → CPU reads → CPU writes → [system RAM]
           (device generates data)    (CPU memcpy)   (destination)
CPU is fully occupied during the entire transfer.

With DMA:
Device → DMA engine → [system RAM]
CPU submits one descriptor, then is free to do other work.
Completion interrupt notifies CPU when transfer is done.

┌──────────────┐   DMA descriptor    ┌─────────────┐
│   Device /   │ ─────────────────>  │  DMA Engine │
│  Controller  │   (src, dst, len)   │             │
│              │ <─────────────────  │             │
│              │   IRQ on complete   └──────┬──────┘
└──────────────┘                            │ bus master
                                            ▼
                                     ┌─────────────┐
                                     │  System RAM │
                                     └─────────────┘
```

---

**Cache Coherency Problem**

Modern CPUs cache data in L1/L2/L3. When a device writes to RAM (DMA write), the CPU may hold a **stale cached copy**. When a device reads from RAM (DMA read), the device may read stale data that the CPU has modified in cache but not yet written back. **Two programming models** address this:

```
Cache Coherency Hazard (DMA write, device→CPU):

[Device DMA writes new data to PA 0x1000]
    ↓
[RAM at 0x1000] = new value
    ↓ (but)
[CPU L1/L2 cache for VA→0x1000] = old stale value  ← BUG if CPU reads here

Solution A (Coherent DMA): hardware coherency fabric or non-cached mapping
Solution B (Streaming DMA): driver explicitly invalidates cache before CPU reads
```

**Coherent (Consistent) DMA**

`dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL)`

- Returns a physically contiguous CPU virtual address and a device-visible `dma_handle`
- The memory allocated by `dma_alloc_coherent` is **non-cacheable** *only for that specific DMA buffer*, to ensure CPU and device always see the same data. When the CPU accesses this buffer, its data is not cached.
- Importantly, the CPU *can still cache data for all other normal memory regions*—the non-cacheable setting applies only to the buffer returned by the coherent DMA allocation. All other (non-DMA) memory uses normal CPU caching and is unaffected.
- Since CPU access to the coherent DMA buffer is uncached, reads and writes are slower; use this kind of memory for things like small control rings, descriptor tables, or status blocks where correctness is more important than speed.

</details>

### 流式（非一致性）DMA

CPU 使用缓存内存；驱动**显式同步**。这更复杂，但当 CPU 不与设备共享缓冲区时，允许 CPU 更快地访问数据：

```c
/* CPU writes or updates buffer contents, preparing data for device */
/* dma_map_single flushes (writes back) any cached data covering cpu_ptr, so device sees the latest data in RAM */
dma_addr_t dma = dma_map_single(dev, cpu_ptr, size, DMA_TO_DEVICE);
/* At this point, the buffer must not be touched by the CPU until dma_unmap_single is called */
/* submit descriptor to device hardware to start DMA */
/* ... device performs DMA read from system RAM into its memory ... */
/* When DMA completes, tell the kernel DMA transfer is finished and the CPU can safely reuse or modify cpu_ptr */
dma_unmap_single(dev, dma, size, DMA_TO_DEVICE);
/* After dma_unmap_single, ownership of cpu_ptr returns to CPU and it is safe for CPU to access or modify the memory */
```

`dma_map_single()` 为指定方向的 DMA 执行所需的缓存维护：它为 `DMA_TO_DEVICE` 刷新（写回并失效）覆盖缓冲区的 CPU 缓存行（以便设备看到所有 CPU 更新），或为 `DMA_FROM_DEVICE` 失效缓存（以便设备写入后 CPU 不会看到陈旧数据）。`dma_unmap_single()` 恢复 CPU 所有权，并根据方向，可能再次失效缓存，以便 CPU 读取设备写入的任何数据。

映射和解映射调用执行**缓存同步**。在 `dma_map_single()` 和 `dma_unmap_single()` 之间，缓冲区“属于”设备——**CPU 不得触碰它**。

对于碎片化内存，`dma_map_sg()` / `dma_unmap_sg()` 在 scatter-gather 列表上操作。

DMA 方向：
- `DMA_TO_DEVICE`：CPU→设备；映射前刷新缓存
- `DMA_FROM_DEVICE`：设备→CPU；设备写入前失效缓存，之后再次失效
- `DMA_BIDIRECTIONAL`：双向；最保守（刷新 + 失效）

> **关键洞察：** 方向标志不仅仅是文档——它控制执行哪种缓存操作。`DMA_TO_DEVICE` 刷新脏缓存行，以便设备读取正确数据。`DMA_FROM_DEVICE` 失效缓存行，以便设备写入后 CPU 无法读取陈旧数据。搞错方向会导致静默数据损坏，极难调试。

> **常见陷阱：** 在 `dma_map_single()` 和 `dma_unmap_single()` 之间从 CPU 访问缓冲区是未定义行为。内核文档称之为“所有权”：设备在映射和解映射之间拥有缓冲区。某些架构会静默返回陈旧的缓存数据；另一些则会看到 CPU 和设备写入冲突。始终遵守所有权规则。

---

## IOMMU（输入输出内存管理单元）

**IOMMU** 位于 PCIe（或 AXI）设备和系统内存总线之间。它使用设备特定的页表将设备发出的 **IOVA**（I/O 虚拟地址）转换为物理地址。将 IOMMU 视为**专用于设备的第二个 MMU**——正如 CPU 的 MMU 为每个进程提供自己的虚拟地址空间，IOMMU 为每个设备提供自己的 I/O 虚拟地址空间。

```
Without IOMMU:
Device issues DMA to physical address 0x8000_0000
                ↓
          [System RAM]     ← Device can DMA anywhere in physical RAM
                              (any malicious or buggy driver = security hole)

With IOMMU:
Device issues DMA to IOVA 0x0001_0000
                ↓
         ┌──────────────┐
         │    IOMMU     │   IOVA → PA translation table (per device/group)
         │  page tables │
         └──────┬───────┘
                │ maps to PA 0x8000_0000 (if mapped)
                │ or IOMMU fault (if not mapped)
                ▼
          [System RAM]     ← Device can only reach explicitly mapped regions
```

| IOMMU 实现 | 平台 |
|----------------------|----------|
| Intel VT-d | x86 Intel SoC 和 Xeon |
| AMD-Vi（IOMMU） | x86 AMD Ryzen / EPYC |
| ARM SMMU v2/v3 | ARM SoC、Jetson Orin、服务器 ARM |

### 安全与隔离

没有 IOMMU：设备（或被攻陷的驱动）可以对任何物理地址发起 DMA，读取或破坏任意内存。有 IOMMU：设备只能 DMA 到显式映射的 IOVA；任何在映射窗口之外的访问都会导致 IOMMU 故障（记录日志，设备停滞或重置）。

### IOMMU 组

**IOMMU 组**是一组共享相同 IOMMU 转换硬件的 PCIe 设备，这意味着它们的内存访问被一起管理，并且在 IOMMU 级别**无法分离**。因此，在配置访问控制时，同一组中的所有设备必须作为一个单元处理——例如，当将设备分配给用户空间时（如直通到另一个软件组件或用户进程）。不可能只允许访问组中的一个设备而限制其他设备；访问总是授予整个组。


<details>
<summary>English original</summary>

**Streaming (Non-Coherent) DMA**

CPU uses cached memory; driver **explicitly synchronizes**. This is more complex but allows faster CPU access to the data when the CPU is not sharing the buffer with a device:

```c
/* CPU writes or updates buffer contents, preparing data for device */
/* dma_map_single flushes (writes back) any cached data covering cpu_ptr, so device sees the latest data in RAM */
dma_addr_t dma = dma_map_single(dev, cpu_ptr, size, DMA_TO_DEVICE);
/* At this point, the buffer must not be touched by the CPU until dma_unmap_single is called */
/* submit descriptor to device hardware to start DMA */
/* ... device performs DMA read from system RAM into its memory ... */
/* When DMA completes, tell the kernel DMA transfer is finished and the CPU can safely reuse or modify cpu_ptr */
dma_unmap_single(dev, dma, size, DMA_TO_DEVICE);
/* After dma_unmap_single, ownership of cpu_ptr returns to CPU and it is safe for CPU to access or modify the memory */
```

`dma_map_single()` performs the required cache maintenance for DMA in the specified direction: it flushes (writes back and invalidates) CPU cache lines covering the buffer for `DMA_TO_DEVICE` (so the device sees all CPU updates), or invalidates cache for `DMA_FROM_DEVICE` (so the CPU doesn't see stale data after device writes). `dma_unmap_single()` restores CPU ownership and, depending on direction, may invalidate the cache again so the CPU will read any data written by the device.

The mapping and unmapping calls perform the **cache synchronization**. Between `dma_map_single()` and `dma_unmap_single()`, the buffer "belongs" to the device — **the CPU must not touch it**.

For fragmented memory, `dma_map_sg()` / `dma_unmap_sg()` operate on scatter-gather lists.

DMA directions:
- `DMA_TO_DEVICE`: CPU→device; flush cache before map
- `DMA_FROM_DEVICE`: device→CPU; invalidate cache before device write, invalidate again after
- `DMA_BIDIRECTIONAL`: both; most conservative (flush + invalidate)

> **Key Insight:** The direction flag is not just documentation — it controls which cache operation is performed. `DMA_TO_DEVICE` flushes dirty cache lines so the device reads correct data. `DMA_FROM_DEVICE` invalidates cache lines so the CPU cannot read stale data after the device writes. Getting the direction wrong causes silent data corruption that is extremely hard to debug.

> **Common Pitfall:** Accessing a buffer between `dma_map_single()` and `dma_unmap_single()` from the CPU is undefined behavior. The kernel documentation calls this "ownership": the device owns the buffer between map and unmap. Some architectures will silently return stale cached data; others will see both CPU and device writes collide. Always obey the ownership rule.

---

**IOMMU (Input-Output Memory Management Unit)**

The **IOMMU** sits between PCIe (or AXI) devices and the system memory bus. It translates device-issued **IOVAs** (I/O Virtual Addresses) to physical addresses using device-specific page tables. Think of the IOMMU as a **second MMU dedicated to devices** — just as the CPU's MMU gives each process its own virtual address space, the IOMMU gives each device its own I/O virtual address space.

```
Without IOMMU:
Device issues DMA to physical address 0x8000_0000
                ↓
          [System RAM]     ← Device can DMA anywhere in physical RAM
                              (any malicious or buggy driver = security hole)

With IOMMU:
Device issues DMA to IOVA 0x0001_0000
                ↓
         ┌──────────────┐
         │    IOMMU     │   IOVA → PA translation table (per device/group)
         │  page tables │
         └──────┬───────┘
                │ maps to PA 0x8000_0000 (if mapped)
                │ or IOMMU fault (if not mapped)
                ▼
          [System RAM]     ← Device can only reach explicitly mapped regions
```

| IOMMU implementation | Platform |
|----------------------|----------|
| Intel VT-d | x86 Intel SoCs and Xeon |
| AMD-Vi (IOMMU) | x86 AMD Ryzen / EPYC |
| ARM SMMU v2/v3 | ARM SoCs, Jetson Orin, server ARM |

**Security and Isolation**

Without IOMMU: a device (or compromised driver) can issue DMA to any physical address, reading or corrupting arbitrary memory. With IOMMU: device can only DMA to explicitly mapped IOVAs; any access outside mapped windows causes an IOMMU fault (logged, device stalled or reset).

**IOMMU Groups**

An **IOMMU group** is a set of PCIe devices that share the same IOMMU translation hardware, meaning their memory accesses are managed together and **cannot be separated** at the IOMMU level. As a result, all devices in the same group must be treated as a unit when configuring access control—for example, when assigning devices to userspace (such as for passthrough to another software component or user process). It is not possible to give access to only one device in the group while restricting the others; access is always granted to the entire group together.

</details>

### Kernel API

```c
iommu_map(domain, iova, paddr, size, IOMMU_READ | IOMMU_WRITE);
iommu_unmap(domain, iova, size);
```

这些底层调用通常由框架代码（DMA 子系统、VFIO）使用。驱动开发者使用更高层的 `dma_map_*()` API，它会自动处理 IOMMU 映射。

### VFIO

**VFIO**（Virtual Function I/O）向**用户态暴露由 IOMMU 支撑的设备访问，无需内核驱动**。使用者包括：DPDK（用户态 NIC 驱动）、SPDK（用户态 NVMe）、FPGA 用户态驱动、KVM GPU 直通。VFIO 容器将 group 映射到 IOMMU domain，并允许用户态通过 `/dev/vfio/N` 进行 DMA。

---

## DMA-BUF 框架

在理解了各个 DMA API 之后，下一个挑战是在多个硬件引擎之间共享单个 DMA 缓冲区——例如摄像头 ISP、GPU 推理引擎和显示引擎都处理同一帧。在每个阶段之间拷贝数据会完全消除 DMA 的带宽优势。**DMA-BUF** 解决了这个问题。

DMA-BUF 是一种内核抽象，用于在独立子系统（CPU、GPU、摄像头 ISP、视频编码器、显示引擎）之间共享 DMA 缓冲区而无需拷贝。

### 角色

- **Exporter**：拥有缓冲区；分配后备页或设备内存；实现 `struct dma_buf_ops`（`attach`、`map_attachment`、`unmap_attachment`、`mmap`、`release`、`vmap`）
- **Importer**：附着到已导出的缓冲区；为其设备的 DMA 引擎映射该缓冲区

### 生命周期

```c
/* Exporter */
struct dma_buf *buf = dma_buf_export(&exp_info);  /* create exportable buffer */
int fd = dma_buf_fd(buf, O_CLOEXEC);              /* get a file descriptor handle */
/* pass fd to importer process via Unix socket */

/* Importer */
struct dma_buf *buf = dma_buf_get(fd);            /* get reference from fd */
struct dma_buf_attachment *att = dma_buf_attach(buf, importer_dev);  /* attach device */
struct sg_table *sgt = dma_buf_map_attachment(att, DMA_FROM_DEVICE); /* get sg list */
/* sgt contains scatter-gather list with IOVAs for importer device */
dma_buf_unmap_attachment(att, sgt, DMA_FROM_DEVICE);
dma_buf_detach(buf, att);
dma_buf_put(buf);
```

该 fd 是指向同一物理缓冲区的**跨进程、跨子系统句柄**。通过 Unix socket 传递 fd，可让两个独立进程共享同一 DMA 缓冲区，**无需任何内核拷贝**。

### 零拷贝流水线

```
┌──────────────┐    DMA-BUF fd    ┌─────────────────┐   DMA-BUF fd   ┌──────────────────┐
│  V4L2 camera │ ───────────────> │  CUDA importer  │ ────────────>  │  Display engine  │
│  driver      │  (kernel buffer  │  (same physical │                │  (same physical  │
│  (exporter)  │   export as fd)  │   pages mapped  │                │   pages mapped   │
└──────────────┘                  │   into GPU VA)  │                │   for display)   │
                                  └─────────────────┘                └──────────────────┘
                    No CPU copy at any stage — same physical pages throughout
```

```
V4L2 camera driver → DMABUF fd → CUDA importer → inference kernel → display
```

任何阶段都无 CPU 拷贝。fd 可通过 Unix socket 上的 `SCM_RIGHTS` 在进程之间传递，实现跨进程零拷贝。生产者与消费者之间的同步使用 DMA fence 框架（`dma_fence_wait()`）。

### V4L2 集成

V4L2 支持将 DMA-BUF 作为一种内存类型（`V4L2_MEMORY_DMABUF`）。用户态在 `struct v4l2_buffer.m.fd` 中传入 DMA-BUF fd。ISP 驱动在 `VIDIOC_QBUF` 时自动为其 DMA 引擎映射该缓冲区。

> **关键洞察：** DMA-BUF 文件描述符是共享硬件缓冲区的“护照”。单个 fd 可以跨越进程边界传递，被多个设备驱动导入，并由每个设备的 DMA 引擎映射到设备的地址空间——全程无需在系统 RAM 中移动任何数据。

---

## GPU 内存架构

在建立了通用 DMA 框架之后，GPU 内存管理又增加了一层复杂性：GPU VRAM 与系统 DRAM 相互独立，并通过 PCIe 连接，形成必须谨慎管理的带宽瓶颈。


<details>
<summary>English original</summary>

**Kernel API**

```c
iommu_map(domain, iova, paddr, size, IOMMU_READ | IOMMU_WRITE);
iommu_unmap(domain, iova, size);
```

These low-level calls are typically used by framework code (DMA subsystem, VFIO). Driver authors use the higher-level `dma_map_*()` API, which handles IOMMU mapping automatically.

**VFIO**

**VFIO** (Virtual Function I/O) exposes IOMMU-backed device access to **userspace without a kernel driver**. Used by: DPDK (user-space NIC drivers), SPDK (user-space NVMe), FPGA userspace drivers, KVM GPU passthrough. The VFIO container maps groups to IOMMU domains and allows userspace DMA via `/dev/vfio/N`.

---

**DMA-BUF Framework**

With individual DMA APIs understood, the next challenge is sharing a single DMA buffer between multiple hardware engines — for example, a camera ISP, a GPU inference engine, and a display engine all processing the same frame. Copying the data between each stage would eliminate the bandwidth benefit of DMA entirely. **DMA-BUF** solves this.

DMA-BUF is a kernel abstraction for sharing DMA buffers between independent subsystems (CPU, GPU, camera ISP, video encoder, display engine) without copying.

**Roles**

- **Exporter**: owns the buffer; allocates backing pages or device memory; implements `struct dma_buf_ops` (`attach`, `map_attachment`, `unmap_attachment`, `mmap`, `release`, `vmap`)
- **Importer**: attaches to an exported buffer; maps it for its device's DMA engine

**Lifecycle**

```c
/* Exporter */
struct dma_buf *buf = dma_buf_export(&exp_info);  /* create exportable buffer */
int fd = dma_buf_fd(buf, O_CLOEXEC);              /* get a file descriptor handle */
/* pass fd to importer process via Unix socket */

/* Importer */
struct dma_buf *buf = dma_buf_get(fd);            /* get reference from fd */
struct dma_buf_attachment *att = dma_buf_attach(buf, importer_dev);  /* attach device */
struct sg_table *sgt = dma_buf_map_attachment(att, DMA_FROM_DEVICE); /* get sg list */
/* sgt contains scatter-gather list with IOVAs for importer device */
dma_buf_unmap_attachment(att, sgt, DMA_FROM_DEVICE);
dma_buf_detach(buf, att);
dma_buf_put(buf);
```

The fd is a **cross-process, cross-subsystem handle** to the same physical buffer. Passing an fd via a Unix socket lets two separate processes share the same DMA buffer **without any kernel copy**.

**Zero-Copy Pipeline**

```
┌──────────────┐    DMA-BUF fd    ┌─────────────────┐   DMA-BUF fd   ┌──────────────────┐
│  V4L2 camera │ ───────────────> │  CUDA importer  │ ────────────>  │  Display engine  │
│  driver      │  (kernel buffer  │  (same physical │                │  (same physical  │
│  (exporter)  │   export as fd)  │   pages mapped  │                │   pages mapped   │
└──────────────┘                  │   into GPU VA)  │                │   for display)   │
                                  └─────────────────┘                └──────────────────┘
                    No CPU copy at any stage — same physical pages throughout
```

```
V4L2 camera driver → DMABUF fd → CUDA importer → inference kernel → display
```

No CPU copy at any stage. The fd is passable between processes via `SCM_RIGHTS` on a Unix socket, enabling cross-process zero-copy. Synchronization between producers and consumers uses the DMA fence framework (`dma_fence_wait()`).

**V4L2 Integration**

V4L2 supports DMA-BUF as a memory type (`V4L2_MEMORY_DMABUF`). Userspace passes the DMA-BUF fd in `struct v4l2_buffer.m.fd`. The ISP driver maps the buffer for its DMA engine automatically on `VIDIOC_QBUF`.

> **Key Insight:** DMA-BUF file descriptors are the "passport" of shared hardware buffers. A single fd can be passed across process boundaries, imported by multiple device drivers, and mapped by each device's DMA engine into the device's address space — all without any data movement in system RAM.

---

**GPU Memory Architecture**

With the general DMA framework established, GPU memory management adds one more layer of complexity: GPU VRAM is separate from system DRAM and connected via PCIe, creating a bandwidth bottleneck that must be carefully managed.

</details>

### VRAM 与 PCIe BAR

GPU VRAM（GeForce 上为 GDDR6X，数据中心 GPU 上为 HBM2e/HBM3）由 CPU 通过 PCIe **基址寄存器（BAR）** 访问。GPU 暴露一段 **BAR 窗口**；CPU 将其映射为 uncached MMIO。

```
CPU (System DRAM)                           GPU (VRAM)
┌──────────────┐         PCIe Bus          ┌──────────────┐
│   DRAM       │ <─────────────────────>   │   VRAM       │
│  (CPU DDR5)  │  ~128 GB/s (PCIe 5.0 x16) │  (HBM3)      │
└──────────────┘                           └──────────────┘
       │                                         │
  CPU accesses                           GPU accesses
  GPU VRAM via                           VRAM directly
  PCIe BAR                               at 3.35 TB/s
  (uncached MMIO)                        (H100 HBM3)
```

- PCIe 5.0 x16 峰值带宽：约 128 GB/s 双向
- 独立 GPU 通常通过 256 MB 或 16 GB BAR 暴露 1/8 的 VRAM（resizable BAR / Above-4G Decoding）
- NVIDIA NVLink 绕过 PCIe 进行 GPU-GPU 传输（H100 NVLink 4.0 上为 600 GB/s）

### CUDA 内存类型

| 类型 | API | CPU↔GPU 一致？ | 备注 |
|------|-----|-------------------|-------|
| Device memory | `cudaMalloc()` | 否 | VRAM；GPU kernel 最快 |
| Pinned host memory | `cudaMallocHost()` | 否 | 可 DMA；H2D/D2H 传输更快 |
| Unified Memory | `cudaMallocManaged()` | 是（按需迁移） | 单一指针，CPU 和 GPU 均可用 |
| Mapped host memory | `cudaHostGetDevicePointer()` | 否 | 通过 PCIe 零拷贝；带宽低 |

### CUDA 统一内存

**单一指针在 CPU 和 GPU 上均有效**。CUDA 驱动与操作系统协作，在缺页时以 64KB 粒度页面迁移：
- GPU 缺页 → 从 CPU 迁移至 VRAM
- CPU 缺页 → 从 VRAM 迁移至 CPU DRAM

```
cudaMallocManaged() allocation lifecycle:
                    ┌─────────────────────┐
                    │  Single virtual ptr │
                    │  (valid on CPU+GPU) │
                    └─────────┬───────────┘
                              │
              ┌───────────────┴───────────────┐
              │                               │
    CPU accesses                      GPU accesses
    (page fault)                      (page fault)
              │                               │
    ┌─────────▼──────┐              ┌─────────▼──────┐
    │ Migrate to     │              │ Migrate to     │
    │ CPU DRAM       │              │ GPU VRAM       │
    └────────────────┘              └────────────────┘
```

`cudaMemPrefetchAsync(ptr, size, device, stream)`：访问前显式预取；隐藏迁移延迟。
`cudaMemAdvise()` 提示：`cudaMemAdviseSetReadMostly`（跨设备复制）、`cudaMemAdviseSetPreferredLocation`（首选驻留）。

生产推理代码通常使用显式 `cudaMemcpy()` 配合 pinned 缓冲区，以获得**确定性的延迟**。统一内存最适用于**快速原型开发**以及超出 VRAM 的内存受限模型。

> **常见陷阱：** 在生产推理中使用 `cudaMallocManaged()` 而不加 `cudaMemPrefetchAsync()`，首次访问时会出现不可预测的延迟尖峰。首次 kernel 调用会触发页面迁移，可能停顿数毫秒。务必在计时推理路径开始前预取统一内存区域。

### Jetson 上的 nvmap

Jetson 采用**统一内存架构**（CPU 与 GPU 共享 DRAM）。**nvmap** 为无 IOMMU 的外设管理 carveout（物理连续）与 IOMMU 映射的分配：

```
Jetson Unified Memory Architecture
┌────────────────────────────────────────────────┐
│                  Shared DRAM                   │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
│  │ CPU code │  │ GPU code │  │  NVDLA/VIC   │ │
│  │  & data  │  │  & data  │  │  DMA buffers │ │
│  └────┬─────┘  └────┬─────┘  └──────┬───────┘ │
│       │             │               │          │
│       └─────────────┴───────────────┘          │
│              All point to same physical         │
│              pages — no PCIe transfer!          │
└────────────────────────────────────────────────┘
```

- NVDLA、VIC、camera ISP、显示引擎使用 nvmap 缓冲区
- `/dev/nvmap` 用户空间接口；`NVMAP_IOC_ALLOC`、`NVMAP_IOC_SHARE` ioctl
- CPU 与 GPU 访问相同的物理页；无需 PCIe 传输

> **关键洞察：** 在 Jetson 上，"CPU 内存"与"GPU 内存"的区分不复存在——它们都是同一块 DRAM。这彻底消除了 PCIe 传输开销。由 ISP 捕获的一帧 camera 图像可立即被 CPU 和 GPU CUDA kernel 以全 DRAM 带宽访问，无需任何拷贝或 PCIe 事务。

---


<details>
<summary>English original</summary>

**VRAM and PCIe BAR**

GPU VRAM (GDDR6X on GeForce, HBM2e/HBM3 on datacenter GPUs) is accessed by the CPU through PCIe **Base Address Registers (BAR)**. The GPU exposes a **BAR aperture**; the CPU maps it as uncached MMIO.

```
CPU (System DRAM)                           GPU (VRAM)
┌──────────────┐         PCIe Bus          ┌──────────────┐
│   DRAM       │ <─────────────────────>   │   VRAM       │
│  (CPU DDR5)  │  ~128 GB/s (PCIe 5.0 x16) │  (HBM3)      │
└──────────────┘                           └──────────────┘
       │                                         │
  CPU accesses                           GPU accesses
  GPU VRAM via                           VRAM directly
  PCIe BAR                               at 3.35 TB/s
  (uncached MMIO)                        (H100 HBM3)
```

- PCIe 5.0 x16 peak bandwidth: ~128 GB/s bidirectional
- Discrete GPU typically exposes 1/8 of VRAM via the 256 MB or 16 GB BAR (resizable BAR / Above-4G Decoding)
- NVIDIA NVLink bypasses PCIe for GPU-GPU transfers (600 GB/s on H100 NVLink 4.0)

**CUDA Memory Types**

| Type | API | Coherent CPU↔GPU? | Notes |
|------|-----|-------------------|-------|
| Device memory | `cudaMalloc()` | No | VRAM; fastest for GPU kernels |
| Pinned host memory | `cudaMallocHost()` | No | DMA-able; faster H2D/D2H transfers |
| Unified Memory | `cudaMallocManaged()` | Yes (demand migration) | Single pointer, both CPU and GPU |
| Mapped host memory | `cudaHostGetDevicePointer()` | No | Zero-copy over PCIe; low bandwidth |

**CUDA Unified Memory**

A **single pointer is valid on both CPU and GPU**. The CUDA driver and OS collaborate to migrate 64KB-granule pages on fault:
- GPU page fault → migrate from CPU to VRAM
- CPU page fault → migrate from VRAM to CPU DRAM

```
cudaMallocManaged() allocation lifecycle:
                    ┌─────────────────────┐
                    │  Single virtual ptr │
                    │  (valid on CPU+GPU) │
                    └─────────┬───────────┘
                              │
              ┌───────────────┴───────────────┐
              │                               │
    CPU accesses                      GPU accesses
    (page fault)                      (page fault)
              │                               │
    ┌─────────▼──────┐              ┌─────────▼──────┐
    │ Migrate to     │              │ Migrate to     │
    │ CPU DRAM       │              │ GPU VRAM       │
    └────────────────┘              └────────────────┘
```

`cudaMemPrefetchAsync(ptr, size, device, stream)`: explicit prefetch before access; hides migration latency.
`cudaMemAdvise()` hints: `cudaMemAdviseSetReadMostly` (replicate across devices), `cudaMemAdviseSetPreferredLocation` (preferred residency).

Production inference code typically uses explicit `cudaMemcpy()` with pinned buffers for **deterministic latency**. Unified Memory is most useful for **rapid prototyping** and memory-constrained models that exceed VRAM.

> **Common Pitfall:** Using `cudaMallocManaged()` in production inference without `cudaMemPrefetchAsync()` causes unpredictable latency spikes at first access. The first kernel invocation triggers page migrations that can stall for milliseconds. Always prefetch Unified Memory regions before the timed inference path begins.

**nvmap on Jetson**

Jetson uses a **unified memory architecture** (CPU and GPU share DRAM). **nvmap** manages carveout (physically contiguous) and IOMMU-mapped allocations for IOMMU-less peripherals:

```
Jetson Unified Memory Architecture
┌────────────────────────────────────────────────┐
│                  Shared DRAM                   │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
│  │ CPU code │  │ GPU code │  │  NVDLA/VIC   │ │
│  │  & data  │  │  & data  │  │  DMA buffers │ │
│  └────┬─────┘  └────┬─────┘  └──────┬───────┘ │
│       │             │               │          │
│       └─────────────┴───────────────┘          │
│              All point to same physical         │
│              pages — no PCIe transfer!          │
└────────────────────────────────────────────────┘
```

- NVDLA, VIC, camera ISP, display engine use nvmap buffers
- `/dev/nvmap` userspace interface; `NVMAP_IOC_ALLOC`, `NVMAP_IOC_SHARE` ioctls
- CPU and GPU access the same physical pages; no PCIe transfer needed

> **Key Insight:** On Jetson, the distinction between "CPU memory" and "GPU memory" collapses — it is all the same DRAM. This eliminates PCIe transfer cost entirely. A camera frame captured by the ISP is immediately visible to both the CPU and the GPU CUDA kernel at full DRAM bandwidth, without any copy or PCIe transaction.

---

</details>

## 小结

| DMA 类型 | 缓存同步？ | 需要 IOMMU？ | API | 用例 |
|----------|------------|--------------|-----|---------|
| 一致性 | 否（硬件） | 否 | `dma_alloc_coherent()` | 描述符环、控制寄存器 |
| 流式单缓冲 | 是（显式） | 可选 | `dma_map_single()` | 单缓冲区批量传输 |
| 流式 SG | 是（显式） | 可选 | `dma_map_sg()` | NVMe、网络 scatter-gather |
| DMA-BUF | 基于 fence | 可选 | `dma_buf_*` | 跨设备零拷贝流水线 |
| CUDA Unified | 自动（驱动） | N/A | `cudaMallocManaged()` | 原型多级推理 |

### 概念回顾

- **为什么在非一致性系统上 DMA 需要缓存同步？** CPU 会把数据缓存在 L1/L2/L3 中。如果 CPU 写入某缓冲区，设备随后通过 DMA 读取同一物理地址，设备可能从 DRAM 读到尚未从 CPU 缓存写回的陈旧数据。`dma_map_single(DMA_TO_DEVICE)` 会先刷掉缓存。

- **一致性 DMA 与流式 DMA 的区别是什么？** 一致性 DMA 内存在 CPU 与设备之间始终同步——通常是因为它被映射为非缓存，或者由硬件一致性 fabric 保持其一致。流式 DMA 为速度使用缓存内存，但需要显式的 `map`/`unmap` 调用来同步。

- **IOMMU 防御的是什么？** 有缺陷或恶意的设备向任意物理地址发起 DMA。IOMMU 把每个设备限制在仅为它显式映射的 IOVA 之内。任何落在这些窗口之外的访问都会触发 fault，被记录，且设备被停滞。

- **为什么 DMA-BUF 文件描述符可以在进程之间传递？** DMA-BUF fd 是内核文件描述符，背后是一个带引用计数的缓冲区对象。与任何 fd 一样，它可以通过 Unix socket 上的 `SCM_RIGHTS` 发送。接收进程拿到自己的 fd，指向同一块物理缓冲区——不发生拷贝。

- **何时应当优先用 pinned 缓冲区配 `cudaMemcpy`，而不是 Unified Memory？** 在要求确定性延迟的生产推理中。Unified Memory 的迁移在首次访问时惰性发生，导致不可预测的延迟。pinned 缓冲区配显式的 `cudaMemcpyAsync` 给出确定性的传输时序。

- **为什么 Jetson 在 CPU 与 GPU 之间不需要 PCIe 传输？** Jetson 的集成 SoC 架构把 CPU 和 GPU 放在同一颗 die 上，共享同一 DRAM。不存在跨 PCIe 链路的独立 GPU。CPU 与 GPU 都能直接访问 DRAM，因此“传输”只是虚拟地址重映射——零数据搬运。

---

## AI 硬件关联

- DMA-BUF 配 `V4L2_MEMORY_DMABUF` 在 Jetson 上实现 camera frame→CUDA 零拷贝；省掉一整帧 1080p60 的拷贝（约 12 MB/帧 × 60 = 720 MB/s 的 CPU 拷贝被避免）
- `dma_alloc_coherent` 是 Zynq PL↔PS AXI DMA 命令环与状态描述符的正确 API，此处正确性与简洁性优先于吞吐
- IOMMU（Jetson Orin 上的 ARM SMMU）按硬件引擎划分 DRAM 访问；一个故障的 NVDLA kernel 无法破坏其映射 IOVA 窗口之外的 VIC 或 camera ISP 缓冲区
- 配 `cudaMemPrefetchAsync` 的 CUDA Unified Memory 让原型推理代码通过 CPU DRAM 流式输送权重，从而超出 VRAM 容量，代价是迁移延迟
- 配 `DMA_FROM_DEVICE` 的流式 DMA 是 NVMe-to-GPU GPUDirect Storage 的访问模式，NVMe 数据绕过 CPU 缓存，直接落到 GPU 可访问的 pinned 内存
- VFIO 支持完全绕过内核驱动模型的用户态 FPGA DMA 驱动，适用于要求亚微秒级命令提交的低延迟 AI 加速器卡


<details>
<summary>English original</summary>

**Summary**

| DMA type | Cache sync? | IOMMU needed? | API | Use case |
|----------|------------|--------------|-----|---------|
| Coherent | No (hardware) | No | `dma_alloc_coherent()` | Descriptor rings, control registers |
| Streaming single | Yes (explicit) | Optional | `dma_map_single()` | Single-buffer bulk transfers |
| Streaming SG | Yes (explicit) | Optional | `dma_map_sg()` | NVMe, network scatter-gather |
| DMA-BUF | Fence-based | Optional | `dma_buf_*` | Cross-device zero-copy pipelines |
| CUDA Unified | Automatic (driver) | N/A | `cudaMallocManaged()` | Prototype multi-stage inference |

**Conceptual Review**

- **Why does DMA require cache synchronization on non-coherent systems?** The CPU caches data in L1/L2/L3. If the CPU writes to a buffer and the device then reads the same physical addresses via DMA, the device may read stale data from DRAM that has not yet been written back from CPU cache. `dma_map_single(DMA_TO_DEVICE)` flushes the cache first.

- **What is the difference between coherent and streaming DMA?** Coherent DMA memory is always in sync between CPU and device — typically because it is mapped uncached or because a hardware coherency fabric keeps it consistent. Streaming DMA uses cached memory for speed but requires explicit `map`/`unmap` calls for synchronization.

- **What does the IOMMU protect against?** A buggy or malicious device that issues DMA to arbitrary physical addresses. The IOMMU limits each device to only the IOVAs explicitly mapped for it. Any access outside those windows causes a fault, logged and the device is stalled.

- **Why can DMA-BUF file descriptors be passed between processes?** A DMA-BUF fd is a kernel file descriptor backed by a reference-counted buffer object. Like any fd, it can be sent via `SCM_RIGHTS` on a Unix socket. The receiving process gets its own fd pointing to the same physical buffer — no copy occurs.

- **When should you prefer `cudaMemcpy` with pinned buffers over Unified Memory?** In production inference where deterministic latency is required. Unified Memory migrations happen lazily on first access, causing unpredictable latency. Pinned buffers with explicit `cudaMemcpyAsync` give deterministic transfer timing.

- **Why does Jetson not need PCIe transfers between CPU and GPU?** Jetson's integrated SoC architecture puts CPU and GPU on the same die sharing the same DRAM. There is no discrete GPU across a PCIe link. CPU and GPU both have direct DRAM access, so "transfer" is simply a virtual address remapping — zero data movement.

---

**AI Hardware Connection**

- DMA-BUF with `V4L2_MEMORY_DMABUF` enables camera frame→CUDA zero-copy on Jetson; eliminates a full 1080p60 frame copy (~12 MB/frame × 60 = 720 MB/s CPU copy avoided)
- `dma_alloc_coherent` is the correct API for Zynq PL↔PS AXI DMA command rings and status descriptors where correctness and simplicity outweigh throughput
- IOMMU (ARM SMMU on Jetson Orin) partitions DRAM access per hardware engine; a malfunctioning NVDLA kernel cannot corrupt VIC or camera ISP buffers outside its mapped IOVA window
- CUDA Unified Memory with `cudaMemPrefetchAsync` allows prototype inference code to exceed VRAM size by streaming weights through CPU DRAM, at the cost of migration latency
- Streaming DMA with `DMA_FROM_DEVICE` is the access pattern for NVMe-to-GPU GPUDirect Storage, where NVMe data bypasses CPU cache and lands directly in GPU-accessible pinned memory
- VFIO enables userspace FPGA DMA drivers that bypass the kernel driver model entirely, useful for low-latency AI accelerator cards requiring sub-microsecond command submission

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-15.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-15.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
