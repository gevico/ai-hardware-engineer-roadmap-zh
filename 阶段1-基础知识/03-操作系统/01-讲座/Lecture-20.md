---
title: 第 20 讲：PCIe、NVMe 与 GPU 驱动架构
description: 第 20 讲：PCIe、NVMe 与 GPU 驱动架构
---

# 第 20 讲：PCIe、NVMe 与 GPU 驱动架构

## 概述

AI 系统中的每一块 GPU、NVMe SSD、FPGA 和高速 NIC，都通过同一条骨干互连：PCIe。理解 PCIe 就是理解系统中数据搬运的物理极限。若你想知道为什么从 NVMe SSD 拷贝数据到 GPU 会花掉那么多时间，或为什么同一台服务器中的两块 GPU 通信能快于预期，答案就在 PCIe 拓扑里。

本讲要贯穿始终的心智模型是**互连的层级结构**：数据从存储（NVMe）经 PCIe fabric 流向计算（GPU），每一跳的带宽与延迟由 PCIe 代际和 lane 数量决定。GPU 驱动架构则坐落在这层物理层之上，把 CUDA API 调用翻译成 DMA 事务与寄存器写。

AI 硬件工程师需要理解 PCIe 拓扑与 GPU 驱动内部机制，因为 GPUDirect Storage、GPU 点对点通信和 FPGA 推理加速器都依赖于知道：哪些设备共享同一个 PCIe switch、有哪些 BAR 空间可用、以及 IOMMU 如何与 DMA 映射交互。诊断训练与推理流水线中的性能异常，常常需要读取 PCIe 带宽计数器并理解驱动的 ioctl 路径。

---

## PCIe 拓扑

PCIe（Peripheral Component Interconnect Express）是 AI 硬件系统中 GPU、NVMe SSD、FPGA 和 NIC 的标准**高速互连**。

**层级**：Root Complex（CPU/SoC）→ Root Ports → PCIe Switches → 端点（GPU、NVMe、NIC、FPGA）

各代际的 **lane 带宽**（每 lane、每方向）：

| 代际 | 传输速率 | x16 带宽（双向） |
|---|---|---|
| Gen3 | 8 GT/s（约 1 GB/s/lane） | 约 32 GB/s |
| Gen4 | 16 GT/s（约 2 GB/s/lane） | 约 64 GB/s |
| Gen5 | 32 GT/s（约 4 GB/s/lane） | 约 128 GB/s |

典型 AI 服务器的拓扑如下。设备在树中的**位置**直接决定可用于点对点传输的带宽。

```
CPU (Root Complex)
├── Root Port 0
│   └── PCIe Switch
│       ├── GPU 0 (x16)     ← A100/H100
│       ├── GPU 1 (x16)     ← A100/H100
│       └── NVMe SSD (x4)   ← GPUDirect Storage target
├── Root Port 1
│   └── PCIe Switch
│       ├── GPU 2 (x16)
│       ├── GPU 3 (x16)
│       └── NIC (x16)       ← 100 GbE for distributed training
└── Root Port 2
    └── FPGA (x8)           ← Pre-processing accelerator
```

> **关键洞见：** 连接到同一台 PCIe switch 的两块 GPU 可以点对点通信，数据无需经过系统 RAM。上图中的 GPU 0 与 GPU 1 可以通过该 switch 直接 DMA 到彼此的内存。GPU 0 与 GPU 2 则必须经 Root Complex 并跨两个 Root Port 路由——这样更慢，且视 IOMMU 配置可能还要穿过系统 RAM。

### PCIe 设备发现

启动期间，BIOS/固件通过配置周期遍历总线层级。每个设备都暴露一个**配置空间**：

- 标准（256 B）：Vendor ID、Device ID、Command、Status、Revision、Class Code
- 扩展（4 KB）：PCIe 能力链表（MSI-X、AER、SR-IOV 等）
- 内核通过 `/sys/bus/pci/devices/0000:01:00.0/config` 读取配置空间

启动时的发现流程如下：

1. **BIOS 向每个可能的 bus/device/function 组合发出 Configuration Read 事务**，探测设备是否存在。
2. **每个设备以自己的 Vendor ID 和 Device ID 应答**，标明自身身份。若该位置无设备，读取返回 0xFFFF。
3. **BIOS 读取基地址寄存器（BAR）**：先写入全 1，再读回大小掩码。据此得知每个设备需要多少地址空间。
4. **BIOS 编程 BAR 地址**，把物理地址写入各 BAR 寄存器。自此之后，设备会在这些地址上响应内存映射 I/O。
5. **Linux 内核在启动时重新枚举**，通过 `pci_scan_root_bus()` 读取相同的配置空间并构建 `struct pci_dev` 树。

### 基地址寄存器（BAR）

BAR 声明设备需要哪些**内存与 I/O 区域**。内核在 PCI 枚举期间分配物理地址并写入这些寄存器。

- 内核通过 `ioremap()` 把 BAR 映射到虚拟地址空间；用 `readl()`/`writel()` 访问
- **GPU BAR0**：设备寄存器与控制（16 MB–256 MB）
- **GPU BAR1**：VRAM aperture（CPU 可访问的 GPU framebuffer 窗口；在支持 Resizable BAR / SAM 的较新 GPU 上可达全部 VRAM）

> **关键洞见：** Resizable BAR（AMD 称为 Smart Access Memory 或 SAM）把 GPU BAR1 扩展到覆盖整个 VRAM——在 H100 上最多 80 GB。没有它时，任一时刻只有一小段 VRAM 窗口（通常 256 MB）可被 CPU 访问，大块分配需要驱动移动该窗口。有了完整 BAR，CPU 可直接寻址任意 GPU 内存位置，这对 GPUDirect Storage 零拷贝至关重要。


<details>
<summary>English original</summary>

**Lecture 20: PCIe, NVMe & GPU Driver Architecture**

**Overview**

Every GPU, NVMe SSD, FPGA, and high-speed NIC in an AI system is connected through the same backbone: PCIe. Understanding PCIe is understanding the physical limits of data movement in your system. If you wonder why copying data from an NVMe SSD to the GPU takes a certain amount of time, or why two GPUs in the same server can communicate faster than expected, the answer lies in the PCIe topology.

The mental model to carry through this lecture is the **hierarchy of interconnects**: data flows from storage (NVMe) through the PCIe fabric to compute (GPU), and the bandwidth and latency at each hop are determined by the PCIe generation and the number of lanes. The GPU driver architecture then sits on top of this physical layer, translating CUDA API calls into DMA transactions and register writes.

AI hardware engineers need to understand PCIe topology and GPU driver internals because GPUDirect Storage, peer-to-peer GPU communication, and FPGA inference accelerators all depend on knowing which devices share a PCIe switch, what BAR space is available, and how the IOMMU interacts with DMA mappings. Diagnosing performance anomalies in training and inference pipelines frequently requires reading PCIe bandwidth counters and understanding the driver's ioctl paths.

---

**PCIe Topology**

PCIe (Peripheral Component Interconnect Express) is the standard **high-speed interconnect** for GPUs, NVMe SSDs, FPGAs, and NICs in AI hardware systems.

**Hierarchy**: Root Complex (CPU/SoC) → Root Ports → PCIe Switches → Endpoints (GPU, NVMe, NIC, FPGA)

**Lane bandwidth** by generation (per lane, each direction):

| Generation | Transfer rate | x16 bandwidth (bidirectional) |
|---|---|---|
| Gen3 | 8 GT/s (~1 GB/s/lane) | ~32 GB/s |
| Gen4 | 16 GT/s (~2 GB/s/lane) | ~64 GB/s |
| Gen5 | 32 GT/s (~4 GB/s/lane) | ~128 GB/s |

The topology of a typical AI server looks like this. The **position of a device in the tree** directly determines the bandwidth available for peer-to-peer transfers.

```
CPU (Root Complex)
├── Root Port 0
│   └── PCIe Switch
│       ├── GPU 0 (x16)     ← A100/H100
│       ├── GPU 1 (x16)     ← A100/H100
│       └── NVMe SSD (x4)   ← GPUDirect Storage target
├── Root Port 1
│   └── PCIe Switch
│       ├── GPU 2 (x16)
│       ├── GPU 3 (x16)
│       └── NIC (x16)       ← 100 GbE for distributed training
└── Root Port 2
    └── FPGA (x8)           ← Pre-processing accelerator
```

> **Key Insight:** Two GPUs connected to the same PCIe switch can communicate peer-to-peer without data transiting system RAM. GPU 0 and GPU 1 in the diagram above can DMA directly to each other's memory via the switch. GPU 0 and GPU 2 must route through the Root Complex and across two Root Ports — this is slower and may traverse system RAM depending on the IOMMU configuration.

**PCIe Device Discovery**

During boot, BIOS/firmware walks the bus hierarchy via configuration cycles. Each device exposes a **config space**:

- Standard (256 B): Vendor ID, Device ID, Command, Status, Revision, Class Code
- Extended (4 KB): PCIe capabilities linked list (MSI-X, AER, SR-IOV, etc.)
- Kernel reads config space via `/sys/bus/pci/devices/0000:01:00.0/config`

The boot-time discovery sequence works as follows:

1. **BIOS issues Configuration Read transactions** to each possible bus/device/function combination to probe for device presence.
2. **Each device responds with its Vendor ID and Device ID**, identifying itself. If nothing is present, the read returns 0xFFFF.
3. **BIOS reads the Base Address Registers (BARs)** by writing all-ones and reading back the size mask. This tells the BIOS how much address space each device needs.
4. **BIOS programs BAR addresses** by writing physical addresses into each BAR register. From this point, the device responds to memory-mapped I/O at those addresses.
5. **Linux kernel re-enumerates at boot** via `pci_scan_root_bus()`, reading the same config space and building the `struct pci_dev` tree.

**Base Address Registers (BARs)**

BARs declare what **memory and I/O regions** a device needs. The kernel allocates physical addresses and programs them during PCI enumeration.

- Kernel maps BAR into virtual address space via `ioremap()`; accessed with `readl()`/`writel()`
- **GPU BAR0**: device registers and control (16 MB–256 MB)
- **GPU BAR1**: VRAM aperture (CPU-accessible window into GPU framebuffer; up to full VRAM on recent GPUs with Resizable BAR / SAM)

> **Key Insight:** Resizable BAR (called Smart Access Memory or SAM by AMD) expands GPU BAR1 to cover the entire VRAM — up to 80 GB on an H100. Without it, only a small window (typically 256 MB) of VRAM is CPU-accessible at any time, requiring the driver to move the window for large allocations. With full BAR, the CPU can directly address any GPU memory location, which is critical for GPUDirect Storage zero-copy.

</details>

### PCIe DMA

设备作为 **总线主设备** 对系统 RAM 执行 DMA。内核提供 `dma_map_sg()` 用于 scatter-gather 映射。IOMMU 把总线地址转换为物理地址，提供 **隔离与保护**。

> **常见陷阱：** 启用 PCIe peer-to-peer DMA 时，IOMMU 必须要么被配置为允许该 P2P 事务，要么通过禁用 ACS（Access Control Services）被旁路。默认情况下，许多系统把所有 DMA 都经由 IOMMU 路由，即使存在直接的 switch 级通路，也可能让 P2P 流量串行经过 Root Complex。检查 `lspci -vvv` 中的 ACS 能力与当前 ACS 使能位。

### PCIe Peer-to-Peer (P2P)

同一 PCIe switch 上的设备可以 **直接相互 DMA**，数据无需经过系统 RAM：

- 需要 `pci_p2pmem_alloc_sgl()`，且 IOMMU 旁路或 ACS（Access Control Services）已禁用
- GPUDirect Storage 使用 P2P：NVMe SSD → GPU 内存，无需 CPU 参与
- FPGA → GPU 推理流水线：FPGA 直接把结果投递到 GPU buffer

```
Without P2P (data path through RAM):
NVMe SSD → PCIe Switch → Root Complex → System RAM → Root Complex → PCIe Switch → GPU
  ~100 µs latency, double bandwidth consumption, CPU involvement

With P2P (direct switch path):
NVMe SSD → PCIe Switch → GPU
  ~50 µs latency, no system RAM bandwidth consumed, zero CPU involvement
```

---

## NVMe

在 PCIe 拓扑确立之后，对于闪存 SSD，NVMe 就是 **运行在 PCIe 之上的存储协议**。它从一开始就是针对闪存特性设计的 —— 不像 SATA，后者是为旋转磁盘设计的。

NVMe（Non-Volatile Memory Express）是为闪存 SSD 设计的 **PCIe 原生存储协议**。

- **延迟**：~100 µs（SATA SSD 约 ~5 ms，HDD 约 ~5–10 ms）
- **队列深度**：最多 64K 个队列 × 每队列 64K 条命令
- **MSI-X**：每个队列一个中断向量；队列映射到 CPU 核以提升 NUMA 效率
- **Namespace**：逻辑驱动器抽象；每个控制器支持多个 namespace
- **ZNS（Zoned Namespaces）**：把 SSD 划分为顺序写入的 zone；降低日志类工作负载的写放大

> **关键洞察：** 64K×64K 的队列结构并不是理论上的余量 —— 它反映了一个现实：现代 NVMe SSD 能够跨内部 NAND 通道并行处理数千个 I/O 请求。一块 SATA SSD 只有一个深度为 32 的队列。一块拥有 64K 个队列的 NVMe SSD，在大型服务器上意味着每个 CPU 核一个队列，核间提交 I/O 时没有锁竞争。

### NVMe Linux 驱动栈

- `nvme_core.ko` + transport 模块（PCIe 用 `nvme.ko`，NVMe-oF 用 `nvme-rdma.ko`）
- `blk-mq`：多队列 block 层；每 CPU 软件队列映射到 NVMe 硬件队列
- `io_uring` 或 `libaio` 直接提交到 blk-mq；用 `O_DIRECT` 旁路 page cache

NVMe 与 io_uring 的关系（见第 19 讲）是直接的：io_uring 的 SQE 被翻译成 **blk-mq 请求**，再分派到 NVMe 硬件队列。每个 NVMe 硬件队列映射到一个 MSI-X 中断向量和一个 CPU 核，因此完成事件中断的 **正是提交该 I/O 的那个核**。

---

## GPU 驱动架构（NVIDIA）

GPU 驱动分为 **两层**：管理硬件的 **内核态驱动**，以及管理 CUDA 上下文和流的 **用户态库**。理解这种分层，就能解释为什么 CUDA API 调用有时涉及 ioctl、有时不涉及。

### 内核态驱动（KMD）

`nvidia.ko` 是内核态驱动，负责：

- PCIe 设备初始化、BAR 映射、固件加载
- GPU 内存管理（VRAM 分配、页表建立）
- UVM（Unified Virtual Memory）：按需在 CPU 与 GPU 之间迁移页
- 上下文调度：在多个进程之间分时共享 GPU
- 针对完成事件的 MSI-X 中断处理
- GSP-RM（GPU System Processor Resource Manager）：自 Ampere/Ada 起，固件运行在 GPU 内部的 ARM 核上；`nvidia.ko` 通过 RPC 与 GSP 通信

> **关键洞察：** 自 Ampere 起，NVIDIA 把很大一部分资源管理移入 GSP —— GPU 裸片内一个专用的 ARM Cortex-A9 核。这意味着，许多以前需要 `nvidia.ko` 执行复杂寄存器序列的操作，现在只需向 GSP 发送一条 RPC 消息。好处是驱动更新更快、更可靠（固件更新 GSP；内核模块保持不变）。代价是调试时多了一层间接。


<details>
<summary>English original</summary>

**PCIe DMA**

Devices perform DMA to system RAM as **bus masters**. The kernel provides `dma_map_sg()` for scatter-gather mappings. The IOMMU translates bus addresses to physical addresses, providing **isolation and protection**.

> **Common Pitfall:** When enabling PCIe peer-to-peer DMA, the IOMMU must either be configured to permit the P2P transaction or bypassed via ACS (Access Control Services) disable. By default, many systems route all DMA through the IOMMU, which may serialize P2P traffic through the Root Complex even when a direct switch-level path exists. Check `lspci -vvv` for ACS capability and the current ACS enable bit.

**PCIe Peer-to-Peer (P2P)**

Devices on the same PCIe switch can **DMA directly to each other** without data transiting system RAM:

- Requires `pci_p2pmem_alloc_sgl()` and IOMMU bypass or ACS (Access Control Services) disabled
- GPUDirect Storage uses P2P: NVMe SSD → GPU memory without CPU involvement
- FPGA → GPU inference pipeline: FPGA posts results directly to GPU buffers

```
Without P2P (data path through RAM):
NVMe SSD → PCIe Switch → Root Complex → System RAM → Root Complex → PCIe Switch → GPU
  ~100 µs latency, double bandwidth consumption, CPU involvement

With P2P (direct switch path):
NVMe SSD → PCIe Switch → GPU
  ~50 µs latency, no system RAM bandwidth consumed, zero CPU involvement
```

---

**NVMe**

With PCIe topology established, NVMe is the **storage protocol that runs on top of PCIe** for flash SSDs. It was designed from the ground up for flash characteristics — unlike SATA, which was designed for spinning disks.

NVMe (Non-Volatile Memory Express) is the **PCIe-native storage protocol** designed for flash SSDs.

- **Latency**: ~100 µs (vs ~5 ms for SATA SSD, ~5–10 ms for HDD)
- **Queue depth**: up to 64K queues × 64K commands per queue
- **MSI-X**: one interrupt vector per queue; queues mapped to CPU cores for NUMA efficiency
- **Namespace**: logical drive abstraction; supports multiple namespaces per controller
- **ZNS (Zoned Namespaces)**: divide SSD into sequential-write zones; reduces write amplification for log workloads

> **Key Insight:** The 64K×64K queue structure is not theoretical headroom — it reflects the reality that modern NVMe SSDs can process thousands of I/O requests in parallel across internal NAND channels. A SATA SSD has one queue of depth 32. An NVMe SSD with 64K queues means one queue per CPU core on a large server, with no lock contention between cores submitting I/O.

**NVMe Linux Driver Stack**

- `nvme_core.ko` + transport module (`nvme.ko` for PCIe, `nvme-rdma.ko` for NVMe-oF)
- `blk-mq`: multi-queue block layer; per-CPU software queues map to NVMe hardware queues
- `io_uring` or `libaio` submit directly to blk-mq; bypass page cache with `O_DIRECT`

The relationship between NVMe and io_uring (from Lecture 19) is direct: io_uring SQEs are translated into **blk-mq requests**, which are dispatched to NVMe hardware queues. Each NVMe hardware queue maps to one MSI-X interrupt vector and one CPU core, so completions interrupt **exactly the core that submitted the I/O**.

---

**GPU Driver Architecture (NVIDIA)**

The GPU driver is split into **two layers**: a **kernel-mode driver** that manages hardware, and a **user-mode library** that manages CUDA contexts and streams. Understanding this split explains why CUDA API calls sometimes involve ioctls and sometimes do not.

**Kernel-Mode Driver (KMD)**

`nvidia.ko` is the kernel-mode driver responsible for:

- PCIe device initialization, BAR mapping, firmware load
- GPU memory management (VRAM allocation, page table setup)
- UVM (Unified Virtual Memory): migrate pages between CPU and GPU on demand
- Context scheduling: time-share GPU across multiple processes
- MSI-X interrupt handling for completion events
- GSP-RM (GPU System Processor Resource Manager): since Ampere/Ada, firmware runs on the GPU's internal ARM core; `nvidia.ko` communicates with GSP via RPC

> **Key Insight:** Since Ampere, NVIDIA moved a large portion of resource management into the GSP — a dedicated ARM Cortex-A9 core inside the GPU die. This means that many operations that previously required `nvidia.ko` to do complex register sequences now just involve sending an RPC message to the GSP. The benefit is faster and more reliable driver updates (firmware updates the GSP; the kernel module stays the same). The cost is one more indirection layer to debug.

</details>

### 用户态驱动（UMD）

`libcuda.so` 是 CUDA 用户态驱动：

- 通过 `/dev/nvidia0`、`/dev/nvidiactl`、`/dev/nvidia-uvm` 上的 `ioctl()` 与 `nvidia.ko` 通信
- 在用户态管理 CUDA 上下文、流和事件
- NVCC 经由 `ptxas` 将 CUDA C++ 编译为 PTX（可移植汇编）→ SASS（GPU ISA）

从 CUDA API 调用到硬件的完整调用路径如下：

1. 应用程序调用 `cudaMemcpyAsync(dst, src, size, cudaMemcpyHostToDevice, stream)`。
2. `libcuda.so` 将其打包为 ioctl 负载并调用 `ioctl(/dev/nvidia0, NV_ESC_RM_DMA_COPY, ...)`。
3. `nvidia.ko` 接收该 ioctl，对照进程的页表校验地址，并配置 GPU 的 copy engine DMA 描述符环。
4. copy engine 的 DMA 引擎通过 PCIe 从系统 RAM 读取并写入 VRAM。
5. DMA 完成后，GPU 触发 MSI-X 中断。
6. `nvidia.ko` 中断处理程序写入完成记录，并可选地唤醒正在等待的 `cudaStreamSynchronize()` 调用。

### 设备节点

| 节点 | 用途 |
|---|---|
| `/dev/nvidia0` | 每 GPU：上下文创建、内存分配 |
| `/dev/nvidiactl` | 全局：设备枚举、能力查询 |
| `/dev/nvidia-uvm` | 统一虚拟内存：托管内存、预取 |
| `/dev/nvidia-modeset` | 显示输出（经由 `nvidia-drm.ko` 的 DRM/KMS） |

### CUDA 执行模型

- **上下文**：每进程的 GPU 状态；默认每个进程每个设备一个上下文
- **流**：上下文内的顺序命令队列；多个流可实现计算与内存传输的重叠
- **事件**：同步原语；`cudaEventRecord()` / `cudaStreamWaitEvent()`

```
Process A                          GPU
  Context 0
    Stream 0: ──kernel──copy──────────────────▶  SM partition
    Stream 1: ──copy──kernel──────────────────▶  Copy engine (overlapped)
  Context 1 (another process)
    Stream 0: ──kernel────────────────────────▶  SM partition (time-shared)
```

> **常见陷阱：** 在同一进程内创建多个 CUDA 上下文（直接使用驱动 API）是允许的，但每个上下文有独立的 VRAM 分配，若不显式 export/import 就无法共享内存。大多数应用应使用 runtime API，它会为每个设备创建一个隐式上下文。如果两个线程各自调用 `cuCtxCreate()`，它们会得到独立的 VRAM 地址空间，无法在彼此之间直接传递指针。

---

## 开源 GPU 驱动

更广泛的 GPU 驱动生态远不止 NVIDIA 的专有栈。

| 驱动 | GPU | 状态 | 用户态 |
|---|---|---|---|
| `amdgpu` | AMD RDNA/CDNA | 完全开源（驱动 + 固件） | `mesa`（ROCm、OpenGL、Vulkan） |
| `i915` | Intel | 完全开源 | `mesa` |
| `nouveau` | NVIDIA | 逆向工程；性能受限 | `mesa`（无 CUDA） |
| `nvidia-open` | NVIDIA（Turing+） | 开源 KMD；专有固件 | CUDA（与专有驱动相同） |

所有开源驱动都使用 Linux 的 **DRM（Direct Rendering Manager）** 子系统。DRM 提供内核框架，用于 **GPU 命令提交**、GEM buffer object，以及用于显示的 KMS（Kernel Mode Setting）。

> **关键洞察：** `nvidia-open`（自 2022 年起对 Turing 及之后架构开源）使用的开源内核模块实现与专有 `nvidia.ko` 相同的硬件接口，但 GSP 固件 blob 仍为专有。这使 Linux 发行版可以从源码构建该内核模块（改善安全审计与安全启动兼容性），同时 NVIDIA 仍掌控 GPU 的内部固件。

向开源基础设施的转变还意味着 `nvidia-open` 参与到 DRM 子系统中，从而能与显示管理器、休眠/恢复以及基于 GSP 的重配置更好地集成——这些此前都需要专有的内核钩子。

---


<details>
<summary>English original</summary>

**User-Mode Driver (UMD)**

`libcuda.so` is the CUDA user-mode driver:

- Communicates with `nvidia.ko` via `ioctl()` on `/dev/nvidia0`, `/dev/nvidiactl`, `/dev/nvidia-uvm`
- Manages CUDA contexts, streams, and events in userspace
- NVCC compiles CUDA C++ → PTX (portable assembly) → SASS (GPU ISA) via `ptxas`

The full call path from a CUDA API call to hardware looks like this:

1. Application calls `cudaMemcpyAsync(dst, src, size, cudaMemcpyHostToDevice, stream)`.
2. `libcuda.so` packages this as an ioctl payload and calls `ioctl(/dev/nvidia0, NV_ESC_RM_DMA_COPY, ...)`.
3. `nvidia.ko` receives the ioctl, validates the addresses against the process's page tables, and programs the GPU's copy engine DMA descriptor rings.
4. The copy engine DMA engine reads from system RAM and writes to VRAM over PCIe.
5. When the DMA completes, the GPU raises an MSI-X interrupt.
6. `nvidia.ko` interrupt handler writes a completion record and optionally wakes a waiting `cudaStreamSynchronize()` call.

**Device Nodes**

| Node | Purpose |
|---|---|
| `/dev/nvidia0` | Per-GPU: context creation, memory allocation |
| `/dev/nvidiactl` | Global: device enumeration, capability query |
| `/dev/nvidia-uvm` | Unified Virtual Memory: managed memory, prefetch |
| `/dev/nvidia-modeset` | Display output (DRM/KMS via `nvidia-drm.ko`) |

**CUDA Execution Model**

- **Context**: per-process GPU state; one context per device per process by default
- **Stream**: in-order command queue within a context; multiple streams enable overlap of compute and memory transfer
- **Event**: synchronization primitive; `cudaEventRecord()` / `cudaStreamWaitEvent()`

```
Process A                          GPU
  Context 0
    Stream 0: ──kernel──copy──────────────────▶  SM partition
    Stream 1: ──copy──kernel──────────────────▶  Copy engine (overlapped)
  Context 1 (another process)
    Stream 0: ──kernel────────────────────────▶  SM partition (time-shared)
```

> **Common Pitfall:** Creating multiple CUDA contexts in the same process (using the driver API directly) is allowed but each context has independent VRAM allocations and cannot share memory without explicit export/import. Most applications should use the runtime API which creates one implicit context per device. If two threads each call `cuCtxCreate()`, they get separate VRAM address spaces and cannot directly pass pointers between them.

---

**Open-Source GPU Drivers**

The broader GPU driver ecosystem extends beyond NVIDIA's proprietary stack.

| Driver | GPU | Status | Userspace |
|---|---|---|---|
| `amdgpu` | AMD RDNA/CDNA | Fully open (driver + firmware) | `mesa` (ROCm, OpenGL, Vulkan) |
| `i915` | Intel | Fully open | `mesa` |
| `nouveau` | NVIDIA | Reverse-engineered; limited perf | `mesa` (no CUDA) |
| `nvidia-open` | NVIDIA (Turing+) | Open KMD; proprietary firmware | CUDA (same as proprietary) |

All open drivers use the Linux **DRM (Direct Rendering Manager)** subsystem. DRM provides the kernel framework for **GPU command submission**, GEM buffer objects, and KMS (Kernel Mode Setting) for display.

> **Key Insight:** `nvidia-open` (open since 2022 for Turing and later) uses an open-source kernel module that performs the same hardware interface as the proprietary `nvidia.ko`, but the GSP firmware blob remains proprietary. This allows Linux distributions to build the kernel module from source (improving security auditing and Secure Boot compatibility) while NVIDIA retains control of the GPU's internal firmware.

The transition to open-source infrastructure also means that `nvidia-open` participates in the DRM subsystem, enabling better integration with display managers, hibernation/resume, and GSP-based reconfiguration that previously required proprietary kernel hooks.

---

</details>

## FPGA 作为 PCIe 端点

FPGA 在 AI 硬件流水线中以**预处理加速器**（雷达信号处理、传感器融合）或**推理加速器**（面向自定义神经网络架构）的形式出现。

Xilinx/AMD XDMA IP 核将 FPGA 暴露为 **PCIe DMA 设备**：

- H2C（Host-to-Card）与 C2H（Card-to-Host）DMA 通道
- 设备节点：`/dev/xdma0_h2c_0`、`/dev/xdma0_c2h_0`
- 在 runtime 重配置（部分重配置）期间通过 PCIe 写入 bitstream
- PCIe P2P：FPGA 可以直接 DMA 到 GPU buffer，用于后处理流水线

典型 AI 流水线中的 FPGA 数据流：

```
Radar ADC input
      ↓
FPGA (XDMA endpoint):
  - Signal processing (FFT, CFAR)
  - Feature extraction
  - Packs feature tensor into C2H DMA buffer
      ↓ PCIe P2P (no CPU, no system RAM)
GPU memory:
  - Feature tensor arrives as CUDA device pointer
  - Neural network inference kernel runs on tensor
      ↓
Detection output
```

> **常见陷阱：** 从 FPGA 到 GPU 的 PCIe P2P 要求两个设备处于同一个 PCIe 域（位于同一个 Root Complex 之下），并且启用 P2P 能力。在多路服务器上，连到 CPU 0 的 PCIe root 的 FPGA 与连到 CPU 1 的 PCIe root 的 GPU 无法做直接 P2P —— 数据必须跨过 QPI/UPI 的跨 socket 链路并经过系统 RAM。设计 P2P 流水线之前，务必用 `lstopo` 或 `nvidia-smi topo -m` 核对拓扑。

---

## 小结

| 组件 | 接口 | 延迟 | 带宽 | kernel 驱动 |
|---|---|---|---|---|
| GPU (A100) | PCIe Gen4 x16 | ~1 µs (DMA) | ~64 GB/s | `nvidia.ko` |
| NVMe SSD | PCIe Gen4 x4 | ~100 µs | ~7 GB/s | `nvme.ko` |
| FPGA (XDMA) | PCIe Gen3/4 x8 | ~5 µs (DMA) | ~16–32 GB/s | `xdma.ko` |
| NIC (100 GbE) | PCIe Gen4 x16 | ~1 µs | ~12 GB/s | `mlx5_core.ko` |
| GPU（P2P 到 NVMe） | PCIe switch | ~100 µs | PCIe switch BW | GPUDirect Storage |

### 概念回顾

- **每 lane 的 PCIe 带宽与整个插槽的总带宽有何区别？** 每条 PCIe lane 在每个方向上都提供带宽（它是全双工的）。一个 Gen4 x16 插槽每个方向约提供 2 GB/s per lane × 16 lanes = 32 GB/s，双向合计约 64 GB/s。每一代每 lane 带宽翻倍（Gen3 → Gen4 → Gen5）。

- **为什么 BAR 大小对 GPUDirect Storage 很重要？** GPUDirect Storage 要求 CPU 建立跨越 GPU VRAM 的 DMA 映射。在 BAR 较小的情况下（例如 256 MB），CPU 在某一时刻只能直接寻址 VRAM 的一部分。采用 Resizable BAR（暴露全部 VRAM）后，整个 GPU 地址空间始终可见，DMA 可以指向任意位置，无需做 BAR 窗口管理。

- **`nvidia.ko` 与 `libcuda.so` 的作用分别是什么？** `nvidia.ko` 拥有硬件：它初始化 GPU、管理 VRAM 页表、处理 MSI-X 中断，并在多个进程之间做仲裁。`libcuda.so` 是 CUDA 的用户态界面：它提供开发者 API、把 PTX 编译为 SASS、管理 stream 与 event —— 而这一切都是通过向 `nvidia.ko` 发送 ioctl 完成的。

- **MIG（在第 23 讲中讲过）与 PCIe 是什么关系？** MIG 把 GPU 内部的计算与内存资源划分成相互独立的实例，但所有 MIG 实例仍然共享同一条与主机相连的 PCIe 连接。PCIe 带宽在 MIG 实例之间共享；只有计算 SM 与 HBM 切片是独享的。这意味着 MIG 的隔离是为了防止邻居干扰 GPU 利用率，而不是隔离 PCIe 带宽。

- **P2P DMA 对 ACS（Access Control Services）有什么要求？** ACS 控制 PCIe switch 是直接在下游端口之间转发点对点事务，还是把它们经由 Root Complex 路由。要实现真正的 P2P（绕过系统 RAM），必须禁用 ACS，或把它配置为允许 P2P 转发。Linux 内核通过 `lspci -vvv | grep ACS` 暴露 ACS 状态。

- **为什么 NVMe 使用按队列的 MSI-X 中断，而不是单条中断线？** 若只有一条中断线，所有 I/O 完成都会落到同一个 CPU 核上，形成瓶颈，并且需要加锁才能把完成事件分派给正确的发起线程。采用按队列的 MSI-X 后，每个完成事件恰好中断拥有该队列的那个 CPU 核，彻底消除跨核唤醒与锁竞争。

---


<details>
<summary>English original</summary>

**FPGA as PCIe Endpoint**

FPGAs appear in AI hardware pipelines as **pre-processing accelerators** (radar signal processing, sensor fusion) or as **inference accelerators** for custom neural network architectures.

Xilinx/AMD XDMA IP core exposes FPGA as a **PCIe DMA device**:

- H2C (Host-to-Card) and C2H (Card-to-Host) DMA channels
- Device nodes: `/dev/xdma0_h2c_0`, `/dev/xdma0_c2h_0`
- Write bitstream via PCIe during runtime reconfiguration (partial reconfiguration)
- PCIe P2P: FPGA can DMA directly to GPU buffer for post-processing pipelines

The FPGA data flow in a typical AI pipeline:

```
Radar ADC input
      ↓
FPGA (XDMA endpoint):
  - Signal processing (FFT, CFAR)
  - Feature extraction
  - Packs feature tensor into C2H DMA buffer
      ↓ PCIe P2P (no CPU, no system RAM)
GPU memory:
  - Feature tensor arrives as CUDA device pointer
  - Neural network inference kernel runs on tensor
      ↓
Detection output
```

> **Common Pitfall:** PCIe P2P from FPGA to GPU requires that both devices be in the same PCIe domain (behind the same Root Complex) and that P2P capability be enabled. On multi-socket servers, an FPGA connected to CPU 0's PCIe root and a GPU connected to CPU 1's PCIe root cannot do direct P2P — the data must cross the QPI/UPI inter-socket link and pass through system RAM. Always verify topology with `lstopo` or `nvidia-smi topo -m` before designing a P2P pipeline.

---

**Summary**

| Component | Interface | Latency | Bandwidth | Kernel driver |
|---|---|---|---|---|
| GPU (A100) | PCIe Gen4 x16 | ~1 µs (DMA) | ~64 GB/s | `nvidia.ko` |
| NVMe SSD | PCIe Gen4 x4 | ~100 µs | ~7 GB/s | `nvme.ko` |
| FPGA (XDMA) | PCIe Gen3/4 x8 | ~5 µs (DMA) | ~16–32 GB/s | `xdma.ko` |
| NIC (100 GbE) | PCIe Gen4 x16 | ~1 µs | ~12 GB/s | `mlx5_core.ko` |
| GPU (P2P to NVMe) | PCIe switch | ~100 µs | PCIe switch BW | GPUDirect Storage |

**Conceptual Review**

- **What is the difference between PCIe bandwidth per lane and total slot bandwidth?** Each PCIe lane provides bandwidth in each direction (it is full duplex). A Gen4 x16 slot provides approximately 2 GB/s per lane × 16 lanes = 32 GB/s in each direction, totalling ~64 GB/s bidirectional. The generation doubles bandwidth per lane each step (Gen3 → Gen4 → Gen5).

- **Why does BAR size matter for GPUDirect Storage?** GPUDirect Storage requires the CPU to set up DMA mappings that span GPU VRAM. With a small BAR (e.g., 256 MB), only a portion of VRAM is directly addressable by the CPU at one time. With Resizable BAR (full VRAM exposed), the entire GPU address space is always visible and DMA can target any location without BAR window management.

- **What is the role of `nvidia.ko` vs `libcuda.so`?** `nvidia.ko` owns the hardware: it initializes the GPU, manages VRAM page tables, handles MSI-X interrupts, and arbitrates between multiple processes. `libcuda.so` is the userspace face of CUDA: it provides the developer API, compiles PTX to SASS, and manages streams and events — all by sending ioctls to `nvidia.ko`.

- **How does MIG (covered in Lecture 23) relate to PCIe?** MIG partitions the GPU's internal compute and memory resources into independent instances, but all MIG instances still share the same PCIe connection to the host. The PCIe bandwidth is shared between MIG instances; only compute SMs and HBM slices are dedicated. This means MIG isolation is about preventing noisy-neighbor GPU utilization, not PCIe bandwidth isolation.

- **What is the ACS (Access Control Services) requirement for P2P DMA?** ACS controls whether a PCIe switch forwards peer-to-peer transactions directly between downstream ports or routes them through the Root Complex. For true P2P (bypassing system RAM), ACS must be disabled or configured to allow P2P forwarding. The Linux kernel exposes ACS state via `lspci -vvv | grep ACS`.

- **Why does NVMe use per-queue MSI-X interrupts instead of a single interrupt line?** With one interrupt line, all I/O completions would arrive at one CPU core, creating a bottleneck and requiring locking to dispatch completions to the correct requesting thread. With per-queue MSI-X, each completion interrupts exactly the CPU core that owns that queue, eliminating cross-core wakeups and lock contention entirely.

---

</details>

## AI 硬件连接

- 通过 `ioremap()` 完成 GPU BAR0 寄存器映射是每个 CUDA 上下文的基础；`nvidia.ko` 通过该接口编程 GPU 页表
- GPUDirect Storage 使用 PCIe P2P 将数据集批从 NVMe 直接流式传输进 GPU HBM，全程无需 CPU 参与，消除了训练流水线中的关键瓶颈
- FPGA 与 GPU 之间的 PCIe P2P 可实现实时预处理（例如 FPGA 上的雷达信号处理 → GPU 内存中的特征张量），且数据路径中没有 CPU
- XDMA 驱动将 FPGA 的 H2C/C2H 通道暴露为字符设备；推理加速器原型通常使用该接口传输张量
- `nvidia.ko` ioctl 路径是每个 CUDA API 调用最终抵达 GPU 的途径；理解它们对于性能剖析和调试异常的 CUDA 启动延迟是必要的
- NVMe 上的每队列 MSI-X 中断（以及 GPU 上的每流中断）允许将完成中断固定到特定 CPU 核，从而消除多 GPU 服务器中的跨 NUMA 中断开销


<details>
<summary>English original</summary>

**AI Hardware Connection**

- GPU BAR0 register mapping via `ioremap()` is the foundation of every CUDA context; `nvidia.ko` programs GPU page tables through this interface
- GPUDirect Storage uses PCIe P2P to stream dataset batches from NVMe directly into GPU HBM without CPU involvement, eliminating a critical bottleneck in training pipelines
- PCIe P2P between FPGA and GPU enables real-time pre-processing (e.g., radar signal processing on FPGA → feature tensors in GPU memory) with no CPU in the data path
- The XDMA driver exposes FPGA H2C/C2H channels as character devices; inference accelerator prototypes commonly use this interface for tensor transfer
- `nvidia.ko` ioctl paths are how every CUDA API call ultimately reaches the GPU; understanding them is necessary for profiling and debugging anomalous CUDA launch latency
- MSI-X per-queue interrupts on NVMe (and per-stream on GPU) allow pinning completion interrupts to specific CPU cores, eliminating cross-NUMA interrupt overhead in multi-GPU servers

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-20.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-20.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
