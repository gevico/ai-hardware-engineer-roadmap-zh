---
title: 讲义 08（L17、L18、L19、L20）：驱动模型与设备树；字符驱动与 V4L2；iouring 与零拷贝；PCIe、NVMe 与 GPU 驱动
description: 讲义 08（L17、L18、L19、L20）：驱动模型与设备树；字符驱动与 V4L2；iouring 与零拷贝；PCIe、NVMe 与 GPU 驱动
---

# 讲义 08（L17、L18、L19、L20）：驱动模型与设备树；字符驱动与 V4L2；io_uring 与零拷贝；PCIe、NVMe 与 GPU 驱动

**涵盖：** 讲义 L17（设备驱动模型与设备树）、L18（字符驱动、中断驱动 I/O 与 V4L2）、L19（io_uring、DMA-BUF 与零拷贝）、L20（PCIe、NVMe 与 GPU 驱动架构）。

---

## 本讲义的组织方式

1. **第 1 部分 — 驱动模型与设备树：** 总线、设备、驱动；match/probe；platform 设备；DTS/DTB；compatible、reg、interrupts；of_* API。
2. **第 2 部分 — 字符驱动与 V4L2：** chrdev 注册；file_operations（read、write、ioctl、mmap、poll）；ioctl 安全性；中断驱动 I/O；V4L2 流水线与 DMA-BUF。
3. **第 3 部分 — 现代 I/O 与零拷贝：** io_uring（SQ/CQ 环、批处理、SQPOLL）；DMA-BUF 共享；sendfile、零拷贝流水线。
4. **第 4 部分 — PCIe、NVMe 与 GPU：** PCIe 拓扑与 BAR；NVMe 队列与驱动栈；GPU 驱动结构；点对点 DMA。

---

# 第 1 部分：Linux 驱动模型与设备树

**背景：** 硬件种类繁多；**驱动模型**提供统一框架：总线枚举设备，**将设备与驱动匹配**，调用 probe/remove。在嵌入式 SoC 上，硬件由启动时传入的**设备树**（DTS → DTB）描述；platform 设备由此创建。

---

## 总线、设备、驱动

- **总线**（`struct bus_type`）：枚举设备并匹配到驱动（PCIe、USB、I2C、SPI、**platform**）。
- **设备**（`struct device`）：一个硬件实例。**驱动**（`struct device_driver`）：管理某一类设备的代码。总线维护设备链表与驱动链表；执行 **match**；匹配成功则调用 **driver->probe(device)**；解绑/移除时调用 **driver->remove(device)**。
- 驱动从**设备对象**获取资源（MMIO、IRQ、时钟），而非硬编码——借助不同的设备树，**同一份驱动二进制可用于不同板卡**。

---

## Platform 设备与设备树

- SoC 外设（UART、I2C、CSI、加速器）不像 PCIe 那样可自描述。它们是 **platform 设备**；其描述来自**设备树**或 ACPI。
- **DTS**（源文件）→ **dtc** → **DTB**（二进制）。bootloader 将 DTB 地址传给 kernel（例如 ARM64 的 x0）。kernel 解析 DTB；`of_platform_populate()` 为带 `status = "okay"` 的节点创建 **platform_device**。
- **匹配：** 驱动通过 `of_match_table` 持有 `compatible` 字符串。节点的 `compatible` 必须匹配。示例：`compatible = "vendor,mydev-v2"`；驱动持有 `{ .compatible = "vendor,mydev-v2" }, {}`。
- **DTS 中的资源：** `reg`（地址、大小）、`interrupts`、`clocks`、`status`。驱动在 probe 中通过 **of_*** API 获取：`of_iomap`、`of_irq_get`、`of_get_property` 等。
- **MODULE_DEVICE_TABLE(of, ...)** 将表嵌入模块，使 udev 在出现匹配节点时加载该模块。

---

# 第 2 部分：字符驱动、中断驱动 I/O 与 V4L2

**背景：** **字符驱动**在 `/dev/` 中暴露一个文件；用户态使用 read、write、ioctl、mmap、poll。**中断驱动 I/O** 让硬件在数据就绪时发出信号，而不必轮询。**V4L2** 是摄像头与视频的标准子系统。

---

## 字符设备注册

- **alloc_chrdev_region**（或静态分配）；用 **file_operations** 调 **cdev_init**；**cdev_add**；**class_create**；**device_create** → `/dev/mydev0`。
- **file_operations：** open、release、read、write、**unlocked_ioctl**、mmap、poll。每个系统调用分派到对应的函数。

---

## ioctl 与安全性

- **ioctl(fd, cmd, arg)** 用于设备专有命令。宏：`_IO`、`_IOR`、`_IOW`（magic、number、direction、type）。在 kernel 中：**绝不**解引用用户指针；使用 **copy_from_user** / **copy_to_user** 并检查返回值。不做拷贝就强制转换 `(struct foo *)arg` 是安全漏洞（TOCTOU、非法指针）。

---

## 驱动中的 mmap

- **mmap** 将 kernel 或设备内存映射到用户 VA。可实现对 DMA 缓冲的零拷贝访问（映射一次，之后在用户态读写）。连续物理内存（DMA、MMIO）使用 **remap_pfn_range**；MMIO/DMA 输出使用 **pgprot_noncached**，使 CPU 不做缓存。非连续或按需映射的区域使用 **vm_insert_page** / **vm_ops->fault**。

---

## 中断驱动 I/O

- 设备在数据就绪时触发 **IRQ**，而不是轮询。驱动：上半部（ISR）应答硬件、排队工作或唤醒等待队列；下半部或进程上下文取走数据。**wait_queue_head_t**；`wake_up_interruptible()` 用于解除 **read()** 的阻塞。避免忙等并降低延迟。

---

## V4L2（Video4Linux2）

- 用于采集/显示的子系统：设备位于 `/dev/video*` 下。**视频设备** → **缓冲队列**；用户态入队缓冲（例如 **V4L2_MEMORY_MMAP** 或 **V4L2_MEMORY_DMABUF**），启动流；驱动填充缓冲（采集）或消费缓冲（输出）；**ioctl** 用于格式、裁剪、缓冲管理。**DMA-BUF** 路径：用户态传入 fd；驱动用该缓冲做采集/输出——与 GPU/其他子系统之间实现零拷贝。

---


<details>
<summary>English original</summary>

**Lecture Note 08 (L17, L18, L19, L20): Driver Model & Device Tree; Char Drivers & V4L2; io_uring & Zero-Copy; PCIe, NVMe & GPU Drivers**

**Combines:** Lecture L17 (Device Driver Model & Device Tree), L18 (Character Drivers, Interrupt-Driven I/O & V4L2), L19 (io_uring, DMA-BUF & Zero-Copy), L20 (PCIe, NVMe & GPU Driver Architecture).

---

**How This Note Is Organized**

1. **Part 1 — Driver model & Device Tree:** Bus, device, driver; match/probe; platform devices; DTS/DTB; compatible, reg, interrupts; of_* API.
2. **Part 2 — Character drivers & V4L2:** chrdev registration; file_operations (read, write, ioctl, mmap, poll); ioctl safety; interrupt-driven I/O; V4L2 pipeline and DMA-BUF.
3. **Part 3 — Modern I/O & zero-copy:** io_uring (SQ/CQ rings, batching, SQPOLL); DMA-BUF sharing; sendfile, zero-copy pipelines.
4. **Part 4 — PCIe, NVMe & GPU:** PCIe topology and BARs; NVMe queues and driver stack; GPU driver layout; peer-to-peer DMA.

---

**Part 1: Linux Driver Model & Device Tree**

**Context:** Hardware is diverse; the **driver model** gives a single framework: buses enumerate devices, **match them to drivers**, call probe/remove. On embedded SoCs, hardware is described in the **Device Tree** (DTS → DTB) passed at boot; platform devices are created from it.

---

**Bus, Device, Driver**

- **Bus** (`struct bus_type`): Enumerates devices and matches them to drivers (PCIe, USB, I2C, SPI, **platform**).
- **Device** (`struct device`): One hardware instance. **Driver** (`struct device_driver`): Code that manages a device type. Bus holds device and driver lists; **match** runs; on match it calls **driver->probe(device)**; on unbind/remove, **driver->remove(device)**.
- Driver gets resources (MMIO, IRQ, clocks) from the **device object, not hardcoded** — **same driver binary for different boards** via different Device Tree.

---

**Platform Devices & Device Tree**

- SoC peripherals (UART, I2C, CSI, accelerator) are not self-describing like PCIe. They are **platform devices**; description comes from **Device Tree** or ACPI.
- **DTS** (source) → **dtc** → **DTB** (binary). Bootloader passes DTB address to kernel (e.g. ARM64 x0). Kernel parses DTB; `of_platform_populate()` creates **platform_device** for nodes with `status = "okay"`.
- **Matching:** Driver has `of_match_table` with `compatible` strings. Node’s `compatible` must match. Example: `compatible = "vendor,mydev-v2"`; driver has `{ .compatible = "vendor,mydev-v2" }, {}`.
- **Resources in DTS:** `reg` (address, size), `interrupts`, `clocks`, `status`. Driver gets them in probe via **of_*** API: `of_iomap`, `of_irq_get`, `of_get_property`, etc.
- **MODULE_DEVICE_TABLE(of, ...)** embeds table so udev can load the module when a matching node appears.

---

**Part 2: Character Drivers, Interrupt-Driven I/O & V4L2**

**Context:** A **character driver** exposes a file in `/dev/`; userspace uses read, write, ioctl, mmap, poll. **Interrupt-driven I/O** lets hardware signal when data is ready instead of polling. **V4L2** is the standard subsystem for cameras and video.

---

**Character Device Registration**

- **alloc_chrdev_region** (or static); **cdev_init** with **file_operations**; **cdev_add**; **class_create**; **device_create** → `/dev/mydev0`.
- **file_operations:** open, release, read, write, **unlocked_ioctl**, mmap, poll. Each syscall dispatches to the corresponding function.

---

**ioctl & Safety**

- **ioctl(fd, cmd, arg)** for device-specific commands. Macros: `_IO`, `_IOR`, `_IOW` (magic, number, direction, type). In kernel: **never** dereference user pointer; use **copy_from_user** / **copy_to_user** and check return. Casting `(struct foo *)arg` without copy is a security bug (TOCTOU, invalid pointer).

---

**mmap in Drivers**

- **mmap** maps kernel or device memory into user VA. Enables zero-copy access to DMA buffers (one map, then read/write in userspace). Use **remap_pfn_range** for contiguous physical (DMA, MMIO); **pgprot_noncached** for MMIO/DMA output so CPU does not cache. **vm_insert_page** / **vm_ops->fault** for non-contiguous or demand-mapped regions.

---

**Interrupt-Driven I/O**

- Instead of polling, device raises **IRQ** when data is ready. Driver: top half (ISR) acknowledges hardware, queues work or signals wait queue; bottom half or process context drains data. **wait_queue_head_t**; `wake_up_interruptible()` to unblock **read()**. Avoids busy-wait and reduces latency.

---

**V4L2 (Video4Linux2)**

- Subsystem for capture/display: devices under `/dev/video*`. **Video device** → **buffer queue**; userspace enqueues buffers (e.g. **V4L2_MEMORY_MMAP** or **V4L2_MEMORY_DMABUF**), starts streaming; driver fills buffers (capture) or consumes them (output); **ioctl** for format, crop, buffer management. **DMA-BUF** path: userspace passes fd; driver uses that buffer for capture/output — zero-copy with GPU/other subsystems.

---

</details>

# Part 3：现代 I/O —— io_uring、DMA-BUF 与零拷贝

**背景：** 传统 read/write 每次操作都要付出**系统调用与拷贝开销**。在高 IOPS 或高带宽下，**io_uring 减少系统调用**；**DMA-BUF 与零拷贝**技术避免 kernel 与设备之间的拷贝。

---

## io_uring（Linux 5.1+）

- **两个共享环：** **SQ（Submission Queue）** —— 应用写入 SQE 描述符；kernel 读取。**CQ（Completion Queue）** —— kernel 写入 CQE；应用读取。两者都经 mmap 映射；无需每次操作一次系统调用即可提交与收割（批量提交；轮询 CQ）。
- **流程：** 应用准备 SQE（例如 io_uring_prep_read），可选地批量；**io_uring_submit()**（一次系统调用处理多个操作）；kernel 异步处理；kernel 推入 CQE；应用轮询或等待 CQE，然后 **io_uring_cqe_seen()**。
- **IORING_SETUP_SQPOLL：** kernel 线程排空 SQ；提交路径可以是**零系统调用**。**IORING_SETUP_IOPOLL：** kernel 轮询完成（低延迟）。**固定缓冲区**避免每次操作调用 get_user_pages。
- **操作：** read、write、send、recv、accept、connect、fsync、splice、openat、statx 等。**liburing** 简化设置与使用。在高 IOPS 下，io_uring 相比 read/write 大幅降低 CPU 与系统调用数量。

---

## DMA-BUF 与零拷贝流水线

- **DMA-BUF**（见 Lecture-Note-04）：通过 fd 在驱动、GPU、摄像头、显示之间共享一个缓冲区。**零拷贝流水线：** 摄像头驱动导出 DMA-BUF fd → 用户空间传递给 CUDA/显示；全程使用相同物理页；通过 fence 同步。
- **sendfile()：** kernel 从文件拷贝到 socket（或在 fd 之间拷贝），不反弹到用户空间 —— 减少文件服务中的拷贝。**VisionIPC 风格：** 进程之间使用共享内存（例如共享区域的 mmap 或 DMA-BUF）；生产者写帧，消费者读帧；无拷贝。

---

# Part 4：PCIe、NVMe 与 GPU 驱动架构

**背景：** **PCIe** 是 GPU、NVMe、NIC、FPGA 的标准互连。**拓扑**（root complex → root ports → switches → 端点）与 **lane 数量决定带宽**。NVMe 运行在 PCIe 上；GPU 驱动位于 PCIe 之上并暴露 CUDA/OpenCL。

---

## PCIe 拓扑与发现

- **层级：** Root Complex → Root Ports → PCIe Switches → 端点（GPU、NVMe、NIC、FPGA）。**Lane 带宽：** Gen3 约 1 GB/s/lane；Gen4 约 2；Gen5 约 4。x16 ≈ 32/64/128 GB/s 双向。
- **发现：** BIOS/kernel 遍历总线；读取**配置空间**（Vendor ID、Device ID、BAR、capabilities）。**BAR（Base Address Registers）：** 设备声明 MMIO 大小；BIOS/kernel 分配物理基址；kernel 对 BAR 执行 **ioremap** 以访问寄存器。**GPU BAR0：** 控制/寄存器；**BAR1：** VRAM aperture（Resizable BAR 可将完整 VRAM 暴露给 CPU）。

---

## PCIe DMA 与 Peer-to-Peer

- 设备是总线主设备；DMA 到系统 RAM（也可能彼此 DMA）。kernel 使用 **dma_map_sg** 等；**IOMMU** 将 IOVA→PA 转换。**Peer-to-peer（P2P）：** **同一** PCIe 交换机上的两个设备可以互相 DMA，而无需经过系统 RAM —— 更低延迟，不占用 RAM 带宽。需要 P2P 映射支持，且通常需要 IOMMU/ACS 配置。**GPUDirect Storage：** 当位于同一交换机时，NVMe → GPU 内存通过 P2P。

---

## NVMe

- **NVMe** = 面向 SSD 的 PCIe 原生协议。低延迟（约 100 µs，而 SATA 为 ms）；大量队列（例如 64K 队列 × 64K 命令）。**MSI-X：** 每个队列一个向量；为 NUMA 将队列映射到 CPU。**Linux 栈：** nvme_core + transport（nvme.ko）；**blk-mq**（多队列块层）；io_uring 或 libaio 提交到 blk-mq；**O_DIRECT** 绕过页缓存以获得原始吞吐。

---

## GPU 驱动架构

- **用户空间：** CUDA/OpenCL runtime；对 kernel 驱动执行 ioctl。**Kernel 驱动：** 管理 GPU VM（地址空间）、提交（命令缓冲区）、DMA（分配、映射）、中断（完成）。**内存：** 分配 VRAM；为 CPU 映射（BAR 或 GMMU）；通过 DMA-BUF 与其他设备共享。**Resizable BAR：** 完整 VRAM 对 CPU 可见；对零拷贝和 GPUDirect 很重要。

---

## 汇总表

**驱动模型：** 总线将设备匹配到驱动 → probe；资源来自 DT/ACPI；对于 SoC，使用 platform_driver + of_match_table。

**字符驱动：** fops（open、read、write、ioctl、mmap、poll）；ioctl 使用 copy_from_user/copy_to_user；mmap 使用 remap_pfn_range；中断驱动 read 使用 wait_queue。

**io_uring：** SQ/CQ 环；批量提交；轮询 CQ；SQPOLL 用于零系统调用提交；IOPOLL 用于低延迟完成。

**PCIe：** 拓扑与 lane 决定带宽；BAR 用于 MMIO/VRAM；同一交换机上的 P2P；NVMe = PCIe + blk-mq；GPU = PCIe + kernel 驱动 + 用户空间 ioctl。

---


<details>
<summary>English original</summary>

**Part 3: Modern I/O — io_uring, DMA-BUF & Zero-Copy**

**Context:** Traditional read/write pays **syscall and copy cost per operation**. At high IOPS or high bandwidth, **io_uring reduces syscalls**; **DMA-BUF and zero-copy** techniques avoid copies between kernel and devices.

---

**io_uring (Linux 5.1+)**

- **Two shared rings:** **SQ (Submission Queue)** — application writes SQE descriptors; kernel reads. **CQ (Completion Queue)** — kernel writes CQE; application reads. Both mmap’d; can submit and reap without a syscall per op (batch submit; poll CQ).
- **Flow:** App prepares SQE(s) (e.g. io_uring_prep_read), optionally batches; **io_uring_submit()** (one syscall for many ops); kernel processes async; kernel pushes CQE; app polls or waits for CQE, then **io_uring_cqe_seen()**.
- **IORING_SETUP_SQPOLL:** Kernel thread drains SQ; submit path can be **zero-syscall**. **IORING_SETUP_IOPOLL:** Kernel polls for completion (low latency). **Fixed buffers** avoid get_user_pages per op.
- **Operations:** read, write, send, recv, accept, connect, fsync, splice, openat, statx, etc. **liburing** simplifies setup and use. At high IOPS, io_uring greatly reduces CPU and syscall count vs read/write.

---

**DMA-BUF & Zero-Copy Pipelines**

- **DMA-BUF** (see Lecture-Note-04): One buffer shared across driver, GPU, camera, display via fd. **Zero-copy pipeline:** Camera driver exports DMA-BUF fd → userspace passes to CUDA/display; same physical pages throughout; sync with fences.
- **sendfile():** Kernel copies from file to socket (or between fds) without bouncing to userspace — reduces copies in file serving. **VisionIPC-style:** Shared memory (e.g. mmap of shared region or DMA-BUF) between processes; producer writes frames, consumer reads; no copy.

---

**Part 4: PCIe, NVMe & GPU Driver Architecture**

**Context:** **PCIe** is the standard interconnect for GPUs, NVMe, NICs, FPGAs. **Topology** (root complex → root ports → switches → endpoints) and **lane count determine bandwidth**. NVMe runs on PCIe; GPU drivers sit on top of PCIe and expose CUDA/OpenCL.

---

**PCIe Topology & Discovery**

- **Hierarchy:** Root Complex → Root Ports → PCIe Switches → Endpoints (GPU, NVMe, NIC, FPGA). **Lane bandwidth:** Gen3 ~1 GB/s/lane; Gen4 ~2; Gen5 ~4. x16 ≈ 32/64/128 GB/s bidirectional.
- **Discovery:** BIOS/kernel walks bus; reads **config space** (Vendor ID, Device ID, BARs, capabilities). **BARs (Base Address Registers):** Device declares MMIO size; BIOS/kernel assigns physical base; kernel **ioremap**’s BAR for register access. **GPU BAR0:** control/registers; **BAR1:** VRAM aperture (Resizable BAR can expose full VRAM to CPU).

---

**PCIe DMA & Peer-to-Peer**

- Devices are bus masters; DMA to system RAM (and possibly each other). Kernel uses **dma_map_sg** etc.; **IOMMU** translates IOVA→PA. **Peer-to-peer (P2P):** Two devices on the **same** PCIe switch can DMA to each other without going through system RAM — lower latency, no RAM bandwidth. Requires P2P mapping support and often IOMMU/ACS configuration. **GPUDirect Storage:** NVMe → GPU memory via P2P when on same switch.

---

**NVMe**

- **NVMe** = PCIe-native protocol for SSDs. Low latency (~100 µs vs ms for SATA); many queues (e.g. 64K queues × 64K commands). **MSI-X:** one vector per queue; map queues to CPUs for NUMA. **Linux stack:** nvme_core + transport (nvme.ko); **blk-mq** (multi-queue block layer); io_uring or libaio submits to blk-mq; **O_DIRECT** bypasses page cache for raw throughput.

---

**GPU Driver Architecture**

- **Userspace:** CUDA/OpenCL runtime; ioctl to kernel driver. **Kernel driver:** Manages GPU VM (address space), submissions (command buffers), DMA (allocations, mapping), interrupts (completion). **Memory:** Allocate VRAM; map for CPU (BAR or GMMU); share with other devices via DMA-BUF. **Resizable BAR:** Full VRAM visible to CPU; important for zero-copy and GPUDirect.

---

**Summary Tables**

**Driver model:** Bus matches device to driver → probe; resources from DT/ACPI; platform_driver + of_match_table for SoC.

**Char driver:** fops (open, read, write, ioctl, mmap, poll); copy_from_user/copy_to_user for ioctl; remap_pfn_range for mmap; wait_queue for interrupt-driven read.

**io_uring:** SQ/CQ rings; batch submit; poll CQ; SQPOLL for zero-syscall submit; IOPOLL for low-latency completion.

**PCIe:** Topology and lanes set bandwidth; BARs for MMIO/VRAM; P2P on same switch; NVMe = PCIe + blk-mq; GPU = PCIe + kernel driver + userspace ioctl.

---

</details>

## AI 硬件连接

- **设备树**描述 Jetson/定制 SoC 上的摄像头、NPU 和加速器节点；一个驱动通过 compatible 字符串支持多块板卡。**V4L2 + DMA-BUF** 实现摄像头 → GPU 零拷贝；**ioctl** 配合 copy_from_user 做控制。
- **io_uring** 用于训练/数据流水线中的高吞吐存储与网络；**DMA-BUF** 用于摄像头–推理–显示流水线，无需拷贝。
- **PCIe 拓扑**与 **nvidia-smi topo** 用于把任务和内存放到正确的 socket 上；**P2P** 用于 NVMe→GPU 以及同一 switch 上的 GPU↔GPU。**Resizable BAR** 用于完整 VRAM 访问与 GPUDirect Storage。

---

*综合 Lecture L17、L18、L19、L20（驱动模型与设备树；字符驱动与 V4L2；io_uring 与零拷贝；PCIe、NVMe 与 GPU 驱动）。*


<details>
<summary>English original</summary>

**AI Hardware Connection**

- **Device Tree** describes camera, NPU, and accelerator nodes on Jetson/custom SoC; one driver supports multiple boards via compatible strings. **V4L2 + DMA-BUF** for camera → GPU zero-copy; **ioctl** with copy_from_user for control.
- **io_uring** for high-throughput storage and network in training/data pipelines; **DMA-BUF** for camera–inference–display pipeline without copies.
- **PCIe topology** and **nvidia-smi topo** for placing jobs and memory on the right socket; **P2P** for NVMe→GPU and GPU↔GPU when on same switch. **Resizable BAR** for full VRAM access and GPUDirect Storage.

---

*Combines Lectures L17, L18, L19, L20 (Driver Model & Device Tree; Char Drivers & V4L2; io_uring & Zero-Copy; PCIe, NVMe & GPU Drivers).*

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-Note-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-Note-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
