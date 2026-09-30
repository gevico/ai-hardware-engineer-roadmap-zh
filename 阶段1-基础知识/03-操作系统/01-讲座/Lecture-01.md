---
title: 第 1 讲：现代 OS 架构与 Linux 内核
description: 第 1 讲：现代 OS 架构与 Linux 内核
---

# 第 1 讲：现代 OS 架构与 Linux 内核

## 概述

每一件 AI 硬件——无论是 Jetson 推理节点、摄像头流水线服务器，还是自动驾驶汽车计算平台——都运行在一个操作系统之上，而该操作系统决定了代码如何触达硬件。本讲要解决的核心问题是：OS 如何在众多相互竞争的软件组件与其下方的物理硬件之间安全地居中仲裁？需要带走的思维模型是：OS 是一个**受信任的裁判**——它拥有硬件，执行关于谁可以触碰什么的规则，并提供干净的抽象，使应用代码永远不必知道寄存器地址。对 AI 硬件工程师而言，理解这套架构意味着：你能把一个缓慢的推理流水线追溯到某个内核子系统，能在不破坏整个系统的前提下调试崩溃的摄像头驱动，还能有意识地选择生产部署要针对哪个内核版本。

---

## OS 的定义：三种角色

| 角色 | 含义 |
|---|---|
| 资源管理器 | 在相互竞争的进程之间仲裁 CPU 时间、内存页、I/O 带宽和网络 |
| 抽象层 | 在异构硬件之上提供统一接口（文件、socket、虚拟内存） |
| 保护边界 | 将进程彼此隔离、并与内核数据结构隔离；由硬件强制执行 |

没有**保护**，一个有 bug 的摄像头驱动就会破坏内核内存。没有**抽象**，每个应用都必须了解特定的硬件寄存器布局。

> **关键洞察：** 这三种角色不可分割。只有抽象而无保护，任何进程都能破坏另一个进程对硬件的视图。只有保护而无抽象，则会迫使每位开发者编写设备专属代码。三者合一，才使 Linux 成为承载多并发工作负载的复杂 AI 系统的可行平台。

---

## 特权级

当代码在 CPU 上运行时，硬件强制执行谁被允许做什么。这是 OS **保护边界**的基础。可以把它想象成 CPU 充当保安：**用户空间**中的代码提出服务请求，OS 批准或拒绝，而硬件使绕过这项检查成为不可能。

### x86 环

| 环 | 模式 | 访问权限 | 典型占用者 |
|---|---|---|---|
| Ring 0 | 内核 | 所有指令、I/O 端口、MSR、CR0/CR3 | Linux 内核、设备驱动 |
| Ring 1–2 | 未使用 | — | 历史上的 OS/2；Linux 未使用 |
| Ring 3 | 用户 | 受限；执行特权指令 → GP Fault | 应用、runtime、Python |

模式切换：`SYSCALL` 指令 → Ring 0；`SYSRET` → Ring 3。

### ARM 异常级（AArch64）

| EL | 名称 | 用途 |
|---|---|---|
| EL0 | 用户 | 应用、TensorRT、ONNX Runtime、ROS2 节点 |
| EL1 | 内核 | Linux 内核、异常处理程序、MMU 配置 |
| EL2 | Hypervisor | KVM、Xen；控制 VM 之间的内存划分 |
| EL3 | 安全监控器 | ARM TrustZone、PSCI 电源管理、BCT/BL31 签名 |

两级 EL0/EL1 划分对大多数部署已足够。EL2 带来**虚拟化开销**；EL3 处理**安全世界**，并且在 Cortex-A 平台上始终存在。在 Jetson Orin（Cortex-A78AE）上，Linux 内核运行在 EL1，NVIDIA MB1/SC7 固件占据 EL3。

```
AArch64 Exception Level Stack
┌─────────────────────────────────────┐
│  EL3 — Secure Monitor (TrustZone)   │  ← NVIDIA MB1/SC7 firmware
│  ARM TrustZone, PSCI, OTP key mgmt  │
├─────────────────────────────────────┤
│  EL2 — Hypervisor (optional)        │  ← KVM, Xen
│  VM-to-VM memory partitioning       │
├─────────────────────────────────────┤
│  EL1 — Kernel                       │  ← Linux kernel
│  MMU, exception handlers, drivers   │
├─────────────────────────────────────┤
│  EL0 — User                         │  ← TensorRT, ROS2, Python
│  Restricted; must SVC to reach EL1  │
└─────────────────────────────────────┘
```

> **关键洞察：** 在 Jetson 上，你的 TensorRT 推理代码运行在 EL0，无法直接触碰 NVDLA 硬件寄存器。它必须通过系统调用和 ioctl 经过 EL1（Linux 内核）。正是这种间接层，才使并发运行多个推理工作负载变得安全——内核将访问串行化，防止一个进程破坏另一个进程的 DMA 缓冲区。

---


<details>
<summary>English original</summary>

**Lecture 1: Modern OS Architecture & the Linux Kernel**

**Overview**

Every piece of AI hardware — a Jetson inference node, a camera pipeline server, or an autonomous vehicle compute platform — runs on top of an operating system that determines how code reaches hardware. The core challenge this lecture addresses is: how does the OS safely mediate between many competing software components and the physical hardware beneath them? The mental model to carry forward is that the OS is a **trusted referee**: it owns the hardware, enforces rules about who can touch what, and presents clean abstractions so application code never has to know register addresses. For an AI hardware engineer, understanding this architecture means you can trace a slow inference pipeline to a kernel subsystem, debug a crashing camera driver without corrupting the whole system, and make deliberate choices about which kernel version to target for a production deployment.

---

**OS Definition: Three Roles**

| Role | Meaning |
|---|---|
| Resource manager | Arbitrates CPU time, memory pages, I/O bandwidth, and network across competing processes |
| Abstraction layer | Presents uniform interfaces (files, sockets, virtual memory) over heterogeneous hardware |
| Protection boundary | Isolates processes from each other and from kernel data structures; enforced in hardware |

Without **protection**, a buggy camera driver corrupts kernel memory. Without **abstraction**, each application must know specific hardware register layouts.

> **Key Insight:** These three roles are inseparable. Abstraction without protection would let any process break another's view of hardware. Protection without abstraction would force every developer to write device-specific code. All three together are what make Linux viable as a platform for complex AI systems with multiple concurrent workloads.

---

**Privilege Levels**

When code runs on a CPU, the hardware enforces who is allowed to do what. This is the foundation of the OS **protection boundary**. Think of it as the CPU acting as a security guard: code in **user space** asks for services, the OS grants or denies them, and the hardware makes it impossible to bypass the check.

**x86 Rings**

| Ring | Mode | Access | Example occupants |
|---|---|---|---|
| Ring 0 | Kernel | All instructions, I/O ports, MSRs, CR0/CR3 | Linux kernel, device drivers |
| Ring 1–2 | Unused | — | Historical OS/2; unused by Linux |
| Ring 3 | User | Restricted; privileged instruction → GP Fault | Applications, runtimes, Python |

Mode switch: `SYSCALL` instruction → Ring 0; `SYSRET` → Ring 3.

**ARM Exception Levels (AArch64)**

| EL | Name | Purpose |
|---|---|---|
| EL0 | User | Applications, TensorRT, ONNX Runtime, ROS2 nodes |
| EL1 | Kernel | Linux kernel, exception handlers, MMU configuration |
| EL2 | Hypervisor | KVM, Xen; controls VM-to-VM memory partitioning |
| EL3 | Secure Monitor | ARM TrustZone, PSCI power management, BCT/BL31 signing |

The two-level EL0/EL1 split is sufficient for most deployments. EL2 adds **virtualization overhead**; EL3 handles **secure world** and is always present on Cortex-A platforms. On Jetson Orin (Cortex-A78AE), the Linux kernel runs at EL1 and NVIDIA MB1/SC7 firmware occupies EL3.

```
AArch64 Exception Level Stack
┌─────────────────────────────────────┐
│  EL3 — Secure Monitor (TrustZone)   │  ← NVIDIA MB1/SC7 firmware
│  ARM TrustZone, PSCI, OTP key mgmt  │
├─────────────────────────────────────┤
│  EL2 — Hypervisor (optional)        │  ← KVM, Xen
│  VM-to-VM memory partitioning       │
├─────────────────────────────────────┤
│  EL1 — Kernel                       │  ← Linux kernel
│  MMU, exception handlers, drivers   │
├─────────────────────────────────────┤
│  EL0 — User                         │  ← TensorRT, ROS2, Python
│  Restricted; must SVC to reach EL1  │
└─────────────────────────────────────┘
```

> **Key Insight:** On a Jetson, your TensorRT inference code runs at EL0 and cannot directly touch the NVDLA hardware registers. It must pass through EL1 (the Linux kernel) via system calls and ioctls. This indirection is what makes it safe to run multiple inference workloads concurrently — the kernel serializes access and prevents one process from corrupting another's DMA buffers.

---

</details>

## Linux 内核架构

Linux 是**带可加载模块的宏内核**：所有核心子系统共享同一地址空间，运行在 Ring 0 / EL1，彼此之间没有 IPC 开销。驱动和文件系统编译为 `.ko` 模块，在 runtime 插入，无需重新编译内核。

宏内核设计带来快速的内核内调用，代价是**共享命运**——一个驱动崩溃就可能让整个系统 panic。与之相对的是**微内核**（QNX、seL4），驱动是独立进程；IPC 更慢，但有**崩溃隔离**。

```
Linux Kernel Internal Architecture (Monolithic)
┌──────────────────────────────────────────────────────────┐
│                    Ring 0 / EL1 (Kernel Space)           │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐  │
│  │Scheduler │  │  Memory  │  │   VFS    │  │  Net    │  │
│  │ (CFS/    │  │ Manager  │  │ (ext4,   │  │ (TCP/IP,│  │
│  │  EEVDF)  │  │ (mm/)    │  │  procfs) │  │  XDP)   │  │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘  │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │  Device Drivers (drivers/)  ~60% of kernel LOC   │    │
│  │  GPU / DRM │ V4L2 Camera │ NVMe │ PCIe │ GPIO    │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │  arch/ (arm64, x86): MMU, entry.S, IRQ setup     │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
              ↑ syscall / interrupt boundary ↑
┌──────────────────────────────────────────────────────────┐
│             Ring 3 / EL0 (User Space)                    │
│   TensorRT  │  camerad  │  modeld  │  Python  │  ROS2    │
└──────────────────────────────────────────────────────────┘
```

### 主要子系统

| 目录 | 子系统 | 职责 |
|---|---|---|
| `kernel/` | 调度器、信号、定时器 | CFS/EEVDF、工作队列、kthread |
| `mm/` | 内存管理 | 页分配器、slab/slub、OOM killer、mmap |
| `drivers/gpu/drm/` | DRM / GPU | 显示、渲染、GEM/PRIME 缓冲管理 |
| `drivers/media/` | V4L2 / Media | 摄像头 ISP、视频采集流水线 |
| `drivers/char/`, `drivers/block/` | 字符 / 块设备 | TTY、GPIO、NVMe、MMC |
| `fs/` | VFS | ext4、tmpfs、overlayfs、procfs、sysfs |
| `net/` | 网络 | TCP/IP、netfilter、XDP、RDMA |
| `arch/` | CPU 相关 | x86、arm64——异常表、系统调用入口、MMU |
| `include/` | 头文件 | 内核全局共享的类型定义 |
| `Documentation/` | 文档 | ABI 契约、设备树绑定、管理员指南 |

> **常见陷阱：** 在 Jetson 上，摄像头驱动崩溃可能让整个系统 kernel panic，因为所有驱动共享 Ring 0 空间。解决办法是在启用 IOMMU 保护的情况下测试驱动（内核命令行中的 `iommu=on`），使行为异常的 DMA 操作触发 fault，而不是乱写内核内存。

理解了内核作为宏内核系统如何组织之后，接下来看内核如何管理**版本**——以及 AI 平台为什么对运行的版本如此保守。

---

## 内核版本

格式：`major.minor.patch` —— 例如 `6.1.57`

| 分支 | 生命周期 | 用途 | 示例 |
|---|---|---|---|
| Mainline | 每个版本约 9 周 | 新功能在此合入 | 6.13, 6.14 |
| Stable | 约 3 个月 | 发布后仅接受修复 | 6.13.y |
| LTS | 2–6 年 | 仅安全与关键修复 | 5.10, 5.15, 6.1, 6.6 |

### AI 平台为什么锁定 LTS

**板级支持包**与特定的内核 **ABI** 紧密耦合。升级到 mainline 会破坏下游的**树外模块**和厂商驱动补丁。

| 平台 | 内核基线 | 主要新增 |
|---|---|---|
| Jetson L4T 35.x (JetPack 5) | 5.10 LTS | NVDLA、Argus ISP/VI/CSI、NvMedia、Tegra PCIe |
| Jetson L4T 36.x (JetPack 6) | 6.1 LTS | Orin NvDLA v2、Tegra ISP v5、PCIe Gen4 |
| openpilot Agnos (comma 3X) | 基于 Ubuntu LTS | Snapdragon 摄像头 ISP、CAN-over-SPI、openpilot 服务 |
| Yocto Kirkstone 嵌入式 AI | 5.15 LTS | 精简 BSP；FPGA/NPU 树外模块 |
| Yocto Scarthgap 嵌入式 AI | 6.6 LTS | EEVDF 调度器；最新 nftables、XDP |

> **关键洞察：** NVIDIA 基于 Linux 6.1 而非更新的内核发布 L4T 36.x，原因是 NVDLA v2 驱动、Tegra ISP v5 和 PCIe Gen4 支持都存在于树外补丁中。变基到更新的 mainline 内核，需要重新移植并重新验证数十万行厂商驱动代码。LTS 的稳定性是实际的工程约束，而非懒惰。

内核版本的背景已经交代清楚，接下来看内核对外的两个最重要的 runtime 检查接口：`/proc` 和 `/sys`。

---


<details>
<summary>English original</summary>

**Linux Kernel Architecture**

Linux is a **monolithic kernel with loadable modules**: all core subsystems share a single address space at Ring 0 / EL1 with no IPC overhead between them. Drivers and filesystems compile as `.ko` modules inserted at runtime without rebuilding the kernel.

The monolithic design delivers fast in-kernel calls at the cost of **shared fate** — a crashing driver can panic the whole system. Contrast with **microkernels** (QNX, seL4) where drivers are separate processes; slower IPC, but **crash isolation**.

```
Linux Kernel Internal Architecture (Monolithic)
┌──────────────────────────────────────────────────────────┐
│                    Ring 0 / EL1 (Kernel Space)           │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐  │
│  │Scheduler │  │  Memory  │  │   VFS    │  │  Net    │  │
│  │ (CFS/    │  │ Manager  │  │ (ext4,   │  │ (TCP/IP,│  │
│  │  EEVDF)  │  │ (mm/)    │  │  procfs) │  │  XDP)   │  │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘  │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │  Device Drivers (drivers/)  ~60% of kernel LOC   │    │
│  │  GPU / DRM │ V4L2 Camera │ NVMe │ PCIe │ GPIO    │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │  arch/ (arm64, x86): MMU, entry.S, IRQ setup     │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
              ↑ syscall / interrupt boundary ↑
┌──────────────────────────────────────────────────────────┐
│             Ring 3 / EL0 (User Space)                    │
│   TensorRT  │  camerad  │  modeld  │  Python  │  ROS2    │
└──────────────────────────────────────────────────────────┘
```

**Major Subsystems**

| Directory | Subsystem | Responsibility |
|---|---|---|
| `kernel/` | Scheduler, signals, timers | CFS/EEVDF, workqueues, kthreads |
| `mm/` | Memory manager | Page allocator, slab/slub, OOM killer, mmap |
| `drivers/gpu/drm/` | DRM / GPU | Display, render, GEM/PRIME buffer management |
| `drivers/media/` | V4L2 / Media | Camera ISP, video capture pipeline |
| `drivers/char/`, `drivers/block/` | Char / Block | TTY, GPIO, NVMe, MMC |
| `fs/` | VFS | ext4, tmpfs, overlayfs, procfs, sysfs |
| `net/` | Networking | TCP/IP, netfilter, XDP, RDMA |
| `arch/` | CPU-specific | x86, arm64 — exception tables, syscall entry, MMU |
| `include/` | Headers | Shared kernel-wide type definitions |
| `Documentation/` | Docs | ABI contracts, Device Tree bindings, admin guides |

> **Common Pitfall:** When a camera driver crashes on Jetson, it can kernel-panic the entire system because all drivers share Ring 0 space. The fix is to test drivers with IOMMU protection enabled (`iommu=on` in the kernel command line) so that a misbehaving DMA operation hits a fault rather than scribbling over kernel memory.

Now that we understand how the kernel is organized as a monolithic system, let's look at how the kernel manages **versioning** — and why AI platforms are conservative about which version they run.

---

**Kernel Versioning**

Format: `major.minor.patch` — e.g., `6.1.57`

| Track | Lifespan | Purpose | Example |
|---|---|---|---|
| Mainline | ~9 weeks per release | New features merge here | 6.13, 6.14 |
| Stable | ~3 months | Fixes only after release | 6.13.y |
| LTS | 2–6 years | Security and critical fixes only | 5.10, 5.15, 6.1, 6.6 |

**Why AI Platforms Pin to LTS**

**Board Support Packages** couple tightly to a specific kernel **ABI**. Upgrading to mainline breaks downstream **out-of-tree modules** and vendor driver patches.

| Platform | Kernel base | Notable additions |
|---|---|---|
| Jetson L4T 35.x (JetPack 5) | 5.10 LTS | NVDLA, Argus ISP/VI/CSI, NvMedia, Tegra PCIe |
| Jetson L4T 36.x (JetPack 6) | 6.1 LTS | Orin NvDLA v2, Tegra ISP v5, PCIe Gen4 |
| openpilot Agnos (comma 3X) | Ubuntu LTS-based | Snapdragon camera ISPs, CAN-over-SPI, openpilot services |
| Yocto Kirkstone embedded AI | 5.15 LTS | Stripped BSP; FPGA/NPU out-of-tree modules |
| Yocto Scarthgap embedded AI | 6.6 LTS | EEVDF scheduler; latest nftables, XDP |

> **Key Insight:** The reason NVIDIA ships L4T 36.x based on Linux 6.1 rather than a newer kernel is that the NVDLA v2 driver, Tegra ISP v5, and PCIe Gen4 support all live in out-of-tree patches. Rebasing to a newer mainline kernel would require re-porting and re-validating hundreds of thousands of lines of vendor driver code. LTS stability is a practical engineering constraint, not laziness.

With the kernel version context established, let's look at the two most important runtime inspection interfaces the kernel exposes: `/proc` and `/sys`.

---

</details>

## /proc 虚拟文件系统

`/proc` 是一个看起来像文件系统的 **kernel 接口**。文件**没有磁盘表示**；读取时会调用 kernel 函数，按需格式化数据。可以把它看作 kernel 的“仪表盘”——你像读文件一样读取它，但拿回来的是 kernel 状态的**实时快照**。

| 路径 | 内容 |
|---|---|
| `/proc/cpuinfo` | Per-CPU：型号、频率、flags（avx512、neon、crypto） |
| `/proc/meminfo` | MemTotal、MemFree、Buffers、Cached、HugePages |
| `/proc/interrupts` | Per-CPU：每条 IRQ 线及名称的中断计数 |
| `/proc/cmdline` | bootloader 传入的 kernel 启动参数 |
| `/proc/[pid]/maps` | VMA 布局：地址范围、权限、后备文件 |
| `/proc/[pid]/status` | State、VmRSS、线程、capability 集合 |
| `/proc/[pid]/fd/` | 指向所有已打开文件描述符的符号链接 |
| `/proc/[pid]/wchan` | 进程当前睡眠所在的 kernel 函数 |

> **常见陷阱：**`/proc/[pid]/maps` 显示的是虚拟地址范围，而不是物理地址范围。两个进程都可以在 `0x7fff0000` 处显示一个映射，而这两个映射完全独立——是物理 RAM 中不同的页。混淆虚拟地址与物理地址，是 DMA 和 GPU 缓冲区管理中常见的调试错误来源。

---

## /sys（sysfs）

**sysfs** 把 kernel 对象——设备、驱动、总线——导出为一棵目录树。其结构镜像的是 **kernel 对象模型**，而不是进程层级。`/proc` 关注的是进程和 kernel 状态，而 `/sys` 关注的是**设备与硬件模型**。

| 路径 | 用途 |
|---|---|
| `/sys/class/net/eth0/` | 接口属性：speed、mtu、carrier 状态 |
| `/sys/bus/pci/devices/` | PCI 设备；vendor/device ID、resource（BAR）文件 |
| `/sys/class/thermal/thermal_zone*/temp` | 以毫摄氏度表示的 zone 温度 |
| `/sys/class/gpio/` | GPIO 引脚导出、方向与值控制 |
| `/sys/fs/cgroup/` | cgroup v2 层级；资源控制器 |
| `/sys/kernel/debug/` | Debugfs：DVFS 状态、链路追踪、GPU 活动监控 |
| `/sys/firmware/devicetree/base/` | 来自运行中 kernel 的实时设备树 |

在 Jetson 上，`/sys/class/thermal/` 暴露 CPU、GPU 和 SoC 的 thermal zone。在推理期间轮询，可以在延迟尖峰出现之前检测到降频。

> **关键洞察：**`/sys/class/thermal/thermal_zoneN/temp` 不只是监控上的新奇玩意——它直接指示你的推理工作负载是否正被 kernel 的热管理 governor 降频。如果某个 GPU zone 超过其 trip point，kernel 会降低时钟频率，而不会警告你的应用。忽略热管理读数的推理 benchmark，测的并不是真实环境下的性能。

---

## Kernel 模块

**模块**运行在 Ring 0，拥有完整的 kernel 访问权限。它们为**设备驱动**提供了标准的集成路径，无需重建 kernel。

```bash
insmod my_driver.ko          # load from file; no dependency resolution
rmmod my_driver              # unload by name
modprobe nvidia              # load with dependency resolution (reads modules.dep)
lsmod                        # list loaded modules and usage count
modinfo nvme                 # show parameters, license, firmware requirements
```


<details>
<summary>English original</summary>

**/proc Virtual Filesystem**

`/proc` is a **kernel interface** that looks like a filesystem. Files have **no disk representation**; reads invoke kernel functions that format data on demand. Think of it as the kernel's "dashboard" — you read from it the same way you read a file, but what you get back is a **live snapshot** of kernel state.

| Path | Content |
|---|---|
| `/proc/cpuinfo` | Per-CPU: model, frequency, flags (avx512, neon, crypto) |
| `/proc/meminfo` | MemTotal, MemFree, Buffers, Cached, HugePages |
| `/proc/interrupts` | Per-CPU interrupt counts per IRQ line and name |
| `/proc/cmdline` | Kernel boot parameters passed by bootloader |
| `/proc/[pid]/maps` | VMA layout: address range, permissions, backing file |
| `/proc/[pid]/status` | State, VmRSS, threads, capability sets |
| `/proc/[pid]/fd/` | Symlinks to all open file descriptors |
| `/proc/[pid]/wchan` | Kernel function where process is currently sleeping |

> **Common Pitfall:** `/proc/[pid]/maps` shows virtual address ranges, not physical ones. Two processes can both show a mapping at `0x7fff0000` and those are completely independent — different pages in physical RAM. Confusing virtual and physical addresses is a frequent source of debugging errors in DMA and GPU buffer management.

---

**/sys (sysfs)**

**sysfs** exports kernel objects — devices, drivers, buses — as a directory tree. Structure mirrors the **kernel object model** rather than process hierarchy. While `/proc` is about processes and kernel state, `/sys` is about **devices and the hardware model**.

| Path | Purpose |
|---|---|
| `/sys/class/net/eth0/` | Interface attributes: speed, mtu, carrier state |
| `/sys/bus/pci/devices/` | PCI devices; vendor/device IDs, resource (BAR) files |
| `/sys/class/thermal/thermal_zone*/temp` | Zone temperature in millidegrees Celsius |
| `/sys/class/gpio/` | GPIO pin export, direction, and value control |
| `/sys/fs/cgroup/` | cgroup v2 hierarchy; resource controllers |
| `/sys/kernel/debug/` | Debugfs: DVFS state, tracing, GPU activity monitors |
| `/sys/firmware/devicetree/base/` | Live Device Tree from running kernel |

On Jetson, `/sys/class/thermal/` exposes CPU, GPU, and SoC thermal zones. Polling during inference detects throttling before it causes latency spikes.

> **Key Insight:** `/sys/class/thermal/thermal_zoneN/temp` is not just a monitoring curiosity — it is a direct signal of whether your inference workload is being throttled by the kernel's thermal governor. If a GPU zone exceeds its trip point, the kernel reduces clock frequency without warning your application. An inference benchmark that ignores thermal readings is not measuring real-world performance.

---

**Kernel Modules**

**Modules** run at Ring 0 and have full kernel access. They provide the standard integration path for **device drivers** without rebuilding the kernel.

```bash
insmod my_driver.ko          # load from file; no dependency resolution
rmmod my_driver              # unload by name
modprobe nvidia              # load with dependency resolution (reads modules.dep)
lsmod                        # list loaded modules and usage count
modinfo nvme                 # show parameters, license, firmware requirements
```

</details>

### C 语言中的模块生命周期

```c
static int __init my_init(void) {
    pr_info("my_driver: loaded\n");
    return 0;  /* non-zero = load failure; kernel will not insert the module */
}
static void __exit my_exit(void) {
    pr_info("my_driver: unloaded\n");
    /* release all resources here: IRQs, DMA buffers, device nodes */
}
module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, my_of_match);  /* enables udev auto-load on DT match */
```

`__init` 让内核在启动后丢弃初始化代码，回收内存。`MODULE_DEVICE_TABLE` 会生成 `modules.alias` 条目，使设备树节点出现时 `modprobe` 能自动加载。

当设备被发现时，模块生命周期遵循严格的顺序：

1. **DT 匹配**：内核找到 `compatible` 字符串与 `my_of_match` 匹配的设备树节点。
2. **udev 触发**：`modules.alias` 将 `compatible` 字符串映射为模块名；udev 调用 `modprobe`。
3. **模块加载**：内核校验签名（若启用 `CONFIG_MODULE_SIG_FORCE`），将 `.ko` 映射进 Ring 0 地址空间。
4. **`probe()` 运行**：驱动的 probe 函数申请硬件资源，注册设备节点。
5. **设备就绪**：应用代码此时可以 open `/dev/my_accel0` 并下发 ioctl。

**DKMS**（Dynamic Kernel Module Support）在内核更新时重新构建树外模块——开发主机上的 **NVIDIA GPU 驱动**和定制 FPGA PCIe 驱动都采用它。

> **常见陷阱：** 加载针对不同内核版本编译的模块会以 `version magic mismatch` 失败。始终应针对运行中内核的精确内核头文件（`uname -r`）编译模块。DKMS 会自动完成这次重建，但手动构建的模块会在一次提升内核版本的 `apt upgrade` 之后静默失效。

---

## 内核源码结构

| 目录 | 内容 |
|---|---|
| `kernel/` | 核心调度器、信号、定时器、printk、kprobes |
| `mm/` | Buddy 分配器、slab/slub、vmalloc、OOM killer |
| `drivers/` | 所有设备驱动——约占内核源码行数的 60% |
| `arch/arm64/`, `arch/x86/` | 平台入口（head.S、entry.S）、IRQ 初始化、NUMA |
| `fs/` | 文件系统：ext4、xfs、tmpfs、procfs、overlayfs |
| `include/` | 内核头文件；`include/linux/` 用于跨架构类型 |
| `net/` | TCP、UDP、netfilter、socket 层、XDP |
| `Documentation/` | `Documentation/ABI/` 定义稳定的 sysfs 接口 |

---

## AI 平台上的 Linux

在面对真实硬件、梳理厂商定制的内核树时，理解内核架构就会得到回报。下面每个平台都带有定制子系统，而这些子系统之所以存在，正是因为上文所述的模块与子系统架构。

### Jetson L4T

NVIDIA 的 **Linux for Tegra** 是一个 **下游 LTS 分支**，带有针对 NVDLA、VIC（Video Image Compositor）、ISP（Image Signal Processor，图像信号处理器）、NvMedia 和 Tegra PCIe IOMMU 的补丁。L4T 35.x 基于 5.10；L4T 36.x 变基到 6.1。这些补丁 **不在 mainline 中**；它们位于 L4T 树的 `drivers/gpu/`、`drivers/media/` 和 `arch/arm64/` 中。

### openpilot Agnos

comma 的 **AGNOS** 是一个 **分支化并定制修改的 Linux**，运行在 comma 3X 和 comma four（基于 Snapdragon）上。它的构建只服务于一个实际用途：**在道路上运行 openpilot**。其内核（[agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845)）从 Linux 分支而来，并经过 **定制开发**，带有针对前向道路摄像头与驾驶员监控摄像头 ISP、CAN-over-SPI，以及面向车辆常开运行的电源管理的补丁。OS 镜像由 [agnos-builder](https://github.com/commaai/agnos-builder)（引导链、设备树、userspace）生成。`modeld`、`camerad` 和 `controlsd` 依赖稳定的 V4L2 与 SocketCAN ABI；comma **锁定内核**，使上游变更不会破坏摄像头 ISP 寄存器映射或驱动接口。要了解每个 OS 课程主题如何对应 **AGNOS 分支中为道路上的 openpilot 所做的改动**，见阶段 5（自动驾驶方向）中的 [AGNOS Guide — OS Lectures ↔ Development Changes](../../../阶段5-高级专题与专精/05-方向E-自动驾驶/02-openpilot参考栈/01-agnos/Guide.md)。

### 面向定制板卡的 Yocto

量产推理节点使用 **Yocto** 构建最小化、可复现的内核 + rootfs 镜像。只编译必需的驱动；面向安全关键部署，**攻击面**与启动时间都压到最小。

---


<details>
<summary>English original</summary>

**Module Lifecycle in C**

```c
static int __init my_init(void) {
    pr_info("my_driver: loaded\n");
    return 0;  /* non-zero = load failure; kernel will not insert the module */
}
static void __exit my_exit(void) {
    pr_info("my_driver: unloaded\n");
    /* release all resources here: IRQs, DMA buffers, device nodes */
}
module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, my_of_match);  /* enables udev auto-load on DT match */
```

`__init` lets the kernel discard initialization code after boot, reclaiming memory. `MODULE_DEVICE_TABLE` generates a `modules.alias` entry so `modprobe` can auto-load when the Device Tree node appears.

The module lifecycle follows a strict sequence when a device is discovered:

1. **DT match**: kernel finds a Device Tree node whose `compatible` string matches `my_of_match`.
2. **udev triggers**: `modules.alias` maps the `compatible` string to the module name; udev calls `modprobe`.
3. **Module loads**: kernel verifies signature (if `CONFIG_MODULE_SIG_FORCE`), maps `.ko` into Ring 0 address space.
4. **`probe()` runs**: the driver's probe function claims hardware resources, registers device nodes.
5. **Device ready**: application code can now open `/dev/my_accel0` and issue ioctls.

**DKMS** (Dynamic Kernel Module Support) rebuilds out-of-tree modules when the kernel is updated — used by the **NVIDIA GPU driver** on development hosts and custom FPGA PCIe drivers.

> **Common Pitfall:** Loading a module compiled against a different kernel version fails with `version magic mismatch`. Always compile modules against the exact kernel headers of the running kernel (`uname -r`). DKMS automates this rebuild, but manual module builds break silently after a `apt upgrade` that bumps the kernel version.

---

**Kernel Source Layout**

| Directory | Contents |
|---|---|
| `kernel/` | Core scheduler, signals, timers, printk, kprobes |
| `mm/` | Buddy allocator, slab/slub, vmalloc, OOM killer |
| `drivers/` | All device drivers — approximately 60% of kernel source lines |
| `arch/arm64/`, `arch/x86/` | Platform entry (head.S, entry.S), IRQ setup, NUMA |
| `fs/` | Filesystems: ext4, xfs, tmpfs, procfs, overlayfs |
| `include/` | Kernel headers; `include/linux/` for cross-arch types |
| `net/` | TCP, UDP, netfilter, socket layer, XDP |
| `Documentation/` | `Documentation/ABI/` defines stable sysfs interfaces |

---

**Linux on AI Platforms**

Understanding kernel architecture pays off when you navigate vendor-specific kernel trees for real hardware. Each platform below has custom subsystems that only exist because of the module and subsystem architecture described above.

**Jetson L4T**

NVIDIA's **Linux for Tegra** is a **downstream LTS fork** with patches for NVDLA, VIC (Video Image Compositor), ISP (Image Signal Processor), NvMedia, and Tegra PCIe IOMMU. L4T 35.x is based on 5.10; L4T 36.x rebased to 6.1. These patches are **absent from mainline**; they live in `drivers/gpu/`, `drivers/media/`, and `arch/arm64/` of the L4T tree.

**openpilot Agnos**

Comma's **AGNOS** is a **forked and custom-modified Linux** that runs on comma 3X and comma four (Snapdragon-based). It is built for one practical use case: **running openpilot on the road**. The kernel ([agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845)) is forked from Linux and **custom developed** with patches for road-facing and driver-monitoring camera ISPs, CAN-over-SPI, and power management for always-on vehicle operation. The OS image is produced by [agnos-builder](https://github.com/commaai/agnos-builder) (boot chain, device tree, userspace). `modeld`, `camerad`, and `controlsd` depend on stable V4L2 and SocketCAN ABIs; comma **pins the kernel** so upstream changes do not break camera ISP register maps or driver interfaces. To see how each OS lecture topic maps to **what was changed in the AGNOS fork** for openpilot on the road, see the [AGNOS Guide — OS Lectures ↔ Development Changes](../../../阶段5-高级专题与专精/05-方向E-自动驾驶/02-openpilot参考栈/01-agnos/Guide.md) in Phase 5 (Autonomous Driving specialization).

**Yocto for Custom Boards**

Production inference nodes use **Yocto** to build minimal, reproducible kernel + rootfs images. Only required drivers are compiled; **attack surface** and boot time are minimized for safety-critical deployment.

---

</details>

## Summary

| Component | Location in kernel tree | Purpose |
|---|---|---|
| Process scheduler | `kernel/sched/` | CPU time allocation across tasks |
| Memory manager | `mm/` | Virtual memory, page and slab allocators |
| VFS | `fs/` | Uniform file API over all filesystem types |
| Device drivers | `drivers/` | Hardware abstraction and resource management |
| Architecture code | `arch/arm64/`, `arch/x86/` | Platform entry, MMU, exception tables |
| Network stack | `net/` | Protocols, socket layer, XDP |
| IPC | `kernel/` (futex, signal), `ipc/` | Inter-process communication primitives |

### Conceptual Review

- **Why is Linux monolithic rather than microkernel?** In-kernel calls between subsystems (scheduler → memory manager → driver) have zero IPC cost. A monolithic design delivers throughput required for high-bandwidth GPU and camera workloads; the tradeoff is that a driver bug can crash the whole system.
- **Why do AI platforms pin to LTS kernels?** Board Support Packages include hundreds of out-of-tree driver patches (NVDLA, ISP, PCIe IOMMU). These patches cannot be instantly rebased to a newer mainline kernel. LTS gives 2–6 years of security fixes without requiring a full BSP re-port.
- **What is `/proc` and why does it look like a filesystem?** `/proc` is a virtual filesystem backed entirely by kernel functions, not disk storage. Reads trigger kernel code that formats live data. The filesystem metaphor means existing tools (`cat`, `grep`, shell scripts) can query kernel state without special APIs.
- **What is a kernel module, and why would a driver be one?** A module is a `.ko` file that loads into Ring 0 at runtime without rebuilding the kernel. Device drivers are modules so new hardware can be supported without a full kernel rebuild and reboot.
- **What does `MODULE_DEVICE_TABLE` do?** It generates an entry in `modules.alias` that maps a Device Tree `compatible` string (or PCI vendor/device ID) to a module name, enabling udev to automatically `modprobe` the correct driver when hardware is detected.
- **What is sysfs (`/sys`) and how does it differ from `/proc`?** sysfs organizes kernel objects (devices, buses, drivers) in a hierarchy mirroring the kernel object model. `/proc` is process-oriented and kernel-state-oriented. Both are virtual filesystems with no disk backing.

---

## AI Hardware Connection

- L4T downstream patches add NVDLA, VIC, and ISP drivers absent from mainline; production Jetson deployments rely on these for hardware-accelerated inference and camera pipelines — understanding the kernel source layout locates them immediately when debugging initialization failures.
- `/sys/class/thermal/thermal_zoneN/temp` is the primary interface for detecting GPU and CPU throttling on Jetson; inference benchmarks should poll this to correlate latency spikes with temperature events.
- Custom FPGA PCIe accelerators require out-of-tree `.ko` modules compiled against the target LTS kernel; DKMS manages rebuilds across point releases automatically.
- openpilot Agnos pins its kernel to maintain camera ISP register map compatibility with comma 3X hardware; upstream kernel changes would break the V4L2 subdevice interface.
- `/proc/interrupts` is the first diagnostic for interrupt storm conditions in high-framerate camera pipelines — per-CPU counts per IRQ line reveal unbalanced distribution before it becomes a throughput problem.
- The monolithic architecture means an FPGA DMA driver crash kernel-panics the entire system; production AI hardware deployments invest in IOMMU protection and driver fault injection testing to contain faults before deployment.


<details>
<summary>English original</summary>

**Summary**

| Component | Location in kernel tree | Purpose |
|---|---|---|
| Process scheduler | `kernel/sched/` | CPU time allocation across tasks |
| Memory manager | `mm/` | Virtual memory, page and slab allocators |
| VFS | `fs/` | Uniform file API over all filesystem types |
| Device drivers | `drivers/` | Hardware abstraction and resource management |
| Architecture code | `arch/arm64/`, `arch/x86/` | Platform entry, MMU, exception tables |
| Network stack | `net/` | Protocols, socket layer, XDP |
| IPC | `kernel/` (futex, signal), `ipc/` | Inter-process communication primitives |

**Conceptual Review**

- **Why is Linux monolithic rather than microkernel?** In-kernel calls between subsystems (scheduler → memory manager → driver) have zero IPC cost. A monolithic design delivers throughput required for high-bandwidth GPU and camera workloads; the tradeoff is that a driver bug can crash the whole system.
- **Why do AI platforms pin to LTS kernels?** Board Support Packages include hundreds of out-of-tree driver patches (NVDLA, ISP, PCIe IOMMU). These patches cannot be instantly rebased to a newer mainline kernel. LTS gives 2–6 years of security fixes without requiring a full BSP re-port.
- **What is `/proc` and why does it look like a filesystem?** `/proc` is a virtual filesystem backed entirely by kernel functions, not disk storage. Reads trigger kernel code that formats live data. The filesystem metaphor means existing tools (`cat`, `grep`, shell scripts) can query kernel state without special APIs.
- **What is a kernel module, and why would a driver be one?** A module is a `.ko` file that loads into Ring 0 at runtime without rebuilding the kernel. Device drivers are modules so new hardware can be supported without a full kernel rebuild and reboot.
- **What does `MODULE_DEVICE_TABLE` do?** It generates an entry in `modules.alias` that maps a Device Tree `compatible` string (or PCI vendor/device ID) to a module name, enabling udev to automatically `modprobe` the correct driver when hardware is detected.
- **What is sysfs (`/sys`) and how does it differ from `/proc`?** sysfs organizes kernel objects (devices, buses, drivers) in a hierarchy mirroring the kernel object model. `/proc` is process-oriented and kernel-state-oriented. Both are virtual filesystems with no disk backing.

---

**AI Hardware Connection**

- L4T downstream patches add NVDLA, VIC, and ISP drivers absent from mainline; production Jetson deployments rely on these for hardware-accelerated inference and camera pipelines — understanding the kernel source layout locates them immediately when debugging initialization failures.
- `/sys/class/thermal/thermal_zoneN/temp` is the primary interface for detecting GPU and CPU throttling on Jetson; inference benchmarks should poll this to correlate latency spikes with temperature events.
- Custom FPGA PCIe accelerators require out-of-tree `.ko` modules compiled against the target LTS kernel; DKMS manages rebuilds across point releases automatically.
- openpilot Agnos pins its kernel to maintain camera ISP register map compatibility with comma 3X hardware; upstream kernel changes would break the V4L2 subdevice interface.
- `/proc/interrupts` is the first diagnostic for interrupt storm conditions in high-framerate camera pipelines — per-CPU counts per IRQ line reveal unbalanced distribution before it becomes a throughput problem.
- The monolithic architecture means an FPGA DMA driver crash kernel-panics the entire system; production AI hardware deployments invest in IOMMU protection and driver fault injection testing to contain faults before deployment.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
