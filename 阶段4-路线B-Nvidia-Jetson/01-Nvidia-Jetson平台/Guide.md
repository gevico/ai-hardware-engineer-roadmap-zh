---
title: Nvidia Jetson 平台
description: Nvidia Jetson 平台
---

# Nvidia Jetson 平台

<div class="course-identity jetson-platform" markdown="1">
<div class="course-identity__icon">JET</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 B1 · NVIDIA Jetson 平台</p>
<p class="course-identity__title">Orin 硬件 bring-up（上电点亮/调通）、启动流程、CUDA、传感器、TensorRT、功耗与生产约束。</p>
<p class="course-identity__meta">产物：Jetson 平台验证报告 · 衡量指标：启动、内存、功耗、推理</p>
</div>
</div>


**阶段 4 — 方向 B — Nvidia Jetson** · 模块 1 / 7

> **重点：** 从开箱的 **Jetson Orin Nano 8GB** 硬件起步，一路做到生产级 AI 流水线，涵盖 ROS 2 集成、传感器融合、优化推理、OTA 更新与强化安全。
>
> **主要硬件：** Jetson Orin Nano 8GB Developer Kit

**下一章：** [2. 定制载板设计](../02-定制载板设计与启动/Guide.md)

---


<a id="1-orin-nano-8gb--hardware--boot-chain-internals"></a>
## 1. Orin Nano 8GB — 硬件与启动链内部机制

本节逐一剖析**真实的启动链**、固件布局、内存占用、A/B 槽位，以及 Orin Nano 8GB 上特有的行为。理解这些，才有基础去调试启动失败、定制固件、优化内存，并在硬件层面从容使用 Jetson 平台。

> **深入阅读：** 生产级内存架构细节（SMMU 地址转换、CMA 内部机制、摄像头零拷贝流水线、DLA 内存路径、多摄像头规划与生产调试）见 [**Orin Nano 内存架构深入剖析**](07-Orin-Nano内存架构/Guide.md)。

> **深入阅读：** 生产规模的 Yocto/OpenEmbedded BSP 开发 —— meta-tegra layer、自定义 layer、rootfs 优化、交叉编译、安全启动集成、CI/CD 流水线、规模化 OTA（25,000+ 台设备）、系统 bring-up、启动性能、许可合规与发布工程 —— 见 [**Orin Nano Yocto BSP 与生产部署**](16-Orin-Nano-Yocto-BSP生产部署/Guide.md)。

> **深入阅读：** **Tensor Core 架构**及其在 Orin Nano（Ampere 架构）上的工作方式 —— Tensor Core 是什么、与 CUDA 核心有何区别、矩阵乘累加（MMA）、精度（FP16/INT8），以及 TensorRT/cuDNN 如何利用它们获得高 TOPS —— 见 [**Orin Nano —— Tensor Core 架构及其工作方式**](14-Orin-Nano张量/Guide.md)。

### 1.1 硬件背景 —— Orin Nano 8GB 到底是什么

Orin Nano 8GB 采用：

* **SoC：** Tegra234（T234）
* **CPU：** ARM Cortex-A78AE 簇（6 核）
* **GPU：** Ampere 架构（1024 个 CUDA 核心）
* **内存：** 8GB LPDDR5（统一内存 —— CPU 与 GPU 共享）
* **存储（Dev Kit）：** NVMe（通常）
* **启动存储：** QSPI NOR flash（用于 bootloader + 固件）

关键区别：

* 启动组件**并非全部存放在 NVMe 上** —— 关键启动阶段位于 **QSPI NOR flash**
* 相比 PC 架构，Jetson 更接近**智能手机架构**

### 1.2 安全启动链（硬件信任根）

启动从 SoC 硅片内部开始。每个阶段在移交控制权前验证下一阶段。

#### 第 1 步：BootROM（BR）

* 硬编码在硅片中 —— 无法修改
* 上电后立即执行
* 读取熔丝位以获得安全启动配置
* 验证下一阶段（MB1）

若启用安全启动：

* 使用 PKC（公钥密码学）
* 校验所有后续阶段的数字签名

#### 第 2 步：MB1（Microboot1）

从 QSPI NOR flash 加载。

职责：

* DRAM 训练（LPDDR5 初始化）
* 电源轨配置
* 时钟设置
* 安全配置
* 初始化 BPMP（Boot and Power Management Processor）
* 验证 MB2

DRAM 训练不正确，系统就无法启动。

#### 第 3 步：MB2

仍从 QSPI NOR flash 加载。

MB2 负责：

* 额外的硬件初始化
* 内存 carveout 准备
* 加载 UEFI、OP-TEE（可信 OS）与固件 blob

此时 CPU 仍未运行 Linux —— 这仍属于 NVIDIA 的启动世界。

### 1.3 UEFI 阶段（固件层）

Jetson Orin Nano 使用基于 EDK2 的 UEFI 实现。

UEFI 职责：

* 枚举设备（PCIe、NVMe、USB）
* 选择启动设备
* 处理 A/B 槽位逻辑
* 加载 `Image`（kernel）、`kernel-dtb`（设备树）与 `initrd`

启动变量存储在 QSPI 与 EFI 变量存储中。

### 1.4 A/B 分区布局（对 OTA 至关重要）

Orin Nano 使用冗余机制实现安全的 OTA 更新：

```
APP        → rootfs A
APP_b      → rootfs B

kernel      → A
kernel_b    → B
```

UEFI 检查：

* 哪个槽位处于活动状态
* 启动成功标志
* 重试计数器

若启动失败次数过多，系统会自动切换到另一个槽位。这由 Boot Control Block（BCB）与 EFI 变量处理 —— OTA 更新正是借此安全进行。

### 1.5 Linux 内核阶段（在 Orin Nano 上）

UEFI 跳转到 kernel 入口。

kernel 二进制：

```
/boot/Image
```

设备树：

```
tegra234-p3767-0000-p3768-0000-a0.dtb
```


<details>
<summary>English original</summary>

**Nvidia Jetson Platform**

<div class="course-identity jetson-platform" markdown="1">
<div class="course-identity__icon">JET</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track B1 · NVIDIA Jetson Platform</p>
<p class="course-identity__title">Bring up Orin hardware, boot flow, CUDA, sensors, TensorRT, power, and production constraints.</p>
<p class="course-identity__meta">Artifact: Jetson platform validation report · Measure: boot, memory, power, inference</p>
</div>
</div>


**Phase 4 — Track B — Nvidia Jetson** · Module 1 of 7

> **Focus:** Go from unboxed **Jetson Orin Nano 8GB** hardware to a production-quality AI pipeline with ROS 2 integration, sensor fusion, optimized inference, OTA updates, and hardened security.
>
> **Primary hardware:** Jetson Orin Nano 8GB Developer Kit

**Next:** [2. Custom Carrier Board Design](../02-定制载板设计与启动/Guide.md)

---


<a id="1-orin-nano-8gb--hardware--boot-chain-internals"></a>
**1. Orin Nano 8GB — Hardware & Boot Chain Internals**

This section walks through the **real boot chain**, firmware layout, memory usage, A/B slots, and what happens specifically on Orin Nano 8GB. Understanding this gives you the foundation to debug boot failures, customize firmware, optimize memory, and work confidently with the Jetson platform at the hardware level.

> **Deep dive:** For production-level memory architecture details (SMMU translation, CMA internals, camera zero-copy pipeline, DLA memory path, multi-camera planning, and production debugging), see [**Orin Nano Memory Architecture Deep Dive**](07-Orin-Nano内存架构/Guide.md).

> **Deep dive:** For production-scale Yocto/OpenEmbedded BSP development — meta-tegra layer, custom layers, rootfs optimization, cross-compilation, secure boot integration, CI/CD pipelines, OTA at scale (25,000+ devices), system bring-up, boot performance, licensing compliance, and release engineering — see [**Orin Nano Yocto BSP & Production Deployment**](16-Orin-Nano-Yocto-BSP生产部署/Guide.md).

> **Deep dive:** For **tensor core architecture** and how it works on Orin Nano (Ampere) — what tensor cores are, how they differ from CUDA cores, matrix multiply-accumulate (MMA), precision (FP16/INT8), and how TensorRT/cuDNN use them for high TOPS — see [**Orin Nano — Tensor Core Architecture and How It Works**](14-Orin-Nano张量/Guide.md).

**1.1 Hardware Context — What Orin Nano 8GB Actually Is**

Orin Nano 8GB uses:

* **SoC:** Tegra234 (T234)
* **CPU:** ARM Cortex-A78AE cluster (6 cores)
* **GPU:** Ampere architecture (1024 CUDA cores)
* **Memory:** 8GB LPDDR5 (unified — shared between CPU and GPU)
* **Storage (Dev Kit):** NVMe (usually)
* **Boot storage:** QSPI NOR flash (for bootloader + firmware)

Important distinctions:

* Boot components are **not all stored on NVMe** — critical boot stages live in **QSPI NOR flash**
* Jetson is closer to **smartphone architecture** than PC architecture

**1.2 Secure Boot Chain (Hardware Root of Trust)**

Boot starts inside SoC silicon. Each stage verifies the next before handing off control.

**Step 1: BootROM (BR)**

* Hard-coded in silicon — cannot be modified
* Executes immediately at power-on
* Reads fuses for secure boot configuration
* Verifies next stage (MB1)

If secure boot is enabled:

* Uses PKC (Public Key Cryptography)
* Verifies digital signatures on all subsequent stages

**Step 2: MB1 (Microboot1)**

Loaded from QSPI NOR flash.

Responsibilities:

* DRAM training (LPDDR5 initialization)
* Power rails configuration
* Clock setup
* Security configuration
* Initializes BPMP (Boot and Power Management Processor)
* Verifies MB2

Without correct DRAM training, the system will not boot.

**Step 3: MB2**

Still loaded from QSPI NOR flash.

MB2 handles:

* Additional hardware initialization
* Memory carveout preparation
* Loading UEFI, OP-TEE (Trusted OS), and firmware blobs

At this stage the CPU is still not running Linux — this is still the NVIDIA boot world.

**1.3 UEFI Stage (Firmware Layer)**

Jetson Orin Nano uses an EDK2-based UEFI implementation.

UEFI responsibilities:

* Enumerates devices (PCIe, NVMe, USB)
* Selects boot device
* Handles A/B slot logic
* Loads `Image` (kernel), `kernel-dtb` (device tree), and `initrd`

Boot variables are stored in QSPI and EFI variable storage.

**1.4 A/B Partition Layout (Critical for OTA)**

Orin Nano uses a redundancy system for safe over-the-air updates:

```
APP        → rootfs A
APP_b      → rootfs B

kernel      → A
kernel_b    → B
```

UEFI checks:

* Which slot is active
* Boot success flag
* Retry counter

If boot fails too many times, the system automatically switches to the other slot. This is handled by the Boot Control Block (BCB) and EFI variables — this is how OTA updates work safely.

**1.5 Linux Kernel Stage (On Orin Nano)**

UEFI jumps to kernel entry.

Kernel binary:

```
/boot/Image
```

Device Tree:

```
tegra234-p3767-0000-p3768-0000-a0.dtb
```

</details>

#### CPU 设置

* 切换到 EL1（异常级别 1）
* 设置页表
* 启用 MMU
* 初始化 SMP 核心

#### 内存布局

8GB RAM 分为：

* **Linux 可用 RAM** — 可供用户空间和内核使用
* **CMA 区域** — 用于 GPU 和 V4L2 分配
* **预留区域** 用于：
  * BPMP（启动和电源管理处理器）
  * RCE（摄像头实时引擎）
  * SPE（安全处理器引擎）
  * OP-TEE（可信执行环境）
  * 显示固件

检查内存布局，使用：

```bash
cat /proc/iomem
```


#### NVIDIA 驱动栈

与普通 PC Linux 不同，Orin 加载 Jetson 专用驱动：

* `nvgpu` — GPU 驱动
* `nvhost` — host1x 多媒体子系统
* `tegra-camrtc` — 摄像头实时控制器
* `vi` — 视频输入驱动
* `isp` — 图像信号处理器驱动
* `nvcsi` — NVIDIA CSI 驱动

摄像头流水线路径：

```
Sensor → NVCSI → VI → ISP → Memory (NVMM) → CUDA / V4L2
```


### 1.6 摄像头流水线（Jetson 专用）

在 Orin Nano 上，摄像头数据流为：

1. 传感器通过 CSI（摄像头串行接口）连接
2. NVCSI 模块接收原始数据
3. VI（视频输入）捕获帧
4. ISP 处理（去马赛克、降噪、色调映射）
5. 通过 CMA 分配内存
6. 以 `/dev/video0` 形式暴露

零拷贝路径：

* **NVMM 内存** — NVIDIA 多媒体内存
* **DMA-BUF** — 内核缓冲区共享
* **CUDA 互操作** — 无需拷贝直接访问 GPU

这就是为什么 CMA 大小对摄像头 + AI 工作负载至关重要。

### 1.7 initramfs 阶段

在挂载完整 rootfs 之前：

* 加载内核模块
* 检查根分区
* 处理加密（如果启用）
* 切换根

然后执行：

```bash
exec /sbin/init
```


### 1.8 systemd 阶段

在 Jetson Ubuntu 上，systemd 启动：

* `nvargus-daemon` — 摄像头服务
* `nvpmodel` 服务 — 电源模式管理
* `networkd` — 网络
* `display manager` — GUI
* `docker` — 容器 runtime（如果启用）

NVIDIA 专用服务：

* `nvpmodel` 控制电源模式（例如，15W 与 7W）
* `jetson_clocks` 工具调整时钟以获得最大性能

### 1.9 图形栈

显示流程：

```
Kernel DRM driver
 → NVIDIA display controller
  → Wayland (default on recent Ubuntu)
   → GNOME
    → Login screen
```

GPU 固件在驱动探测期间加载。

### 1.10 完整启动链总结

```
Power On
 ↓
BootROM (silicon — immutable)
 ↓
MB1 (QSPI — DRAM training + security)
 ↓
MB2 (QSPI — loads UEFI + OP-TEE + firmware)
 ↓
UEFI (device enumeration + A/B slot selection)
 ↓
Load Kernel + DTB + initrd
 ↓
Linux Kernel (memory + drivers + GPU + camera)
 ↓
Mount APP or APP_b
 ↓
initramfs → switch root
 ↓
systemd (NVIDIA services + desktop)
```


### 1.11 高级工程细节

#### bootloader 存储在哪里？

QSPI NOR flash 包含：

* MB1, MB2, UEFI
* 固件 blob
* 启动配置表

rootfs（APP 分区）位于：

* NVMe（开发套件）
* 或 eMMC（生产模块变体）

#### 在 Linux 之外运行的固件

即使 Linux 正在运行，这些微控制器仍保持活动：

* **BPMP** — 启动和电源管理处理器
* **SPE** — 安全处理器引擎
* **RCE** — 摄像头实时引擎

它们运行自己在启动期间加载的固件。Linux 通过 mailbox + IPC 与它们通信。

### 1.12 Orin Nano 与 PC 有何不同？

| PC 启动            | Jetson Orin 启动              |
|--------------------|-------------------------------|
| BIOS/UEFI          | NVIDIA MB1 + MB2 + UEFI      |
| 简单的 DRAM 初始化 | 复杂的 LPDDR5 训练       |
| 标准 GPU           | 固件密集的嵌入式 GPU   |
| 无预留            | 多个内存预留          |
| 无 BPMP           | 专用电源 MCU (BPMP)    |

Jetson 更接近智能手机架构而非 PC — 理解这一区别是在该平台上高效工作的关键。

### 1.13 解读真实的首次启动日志

以下是对实际 Orin Nano 8GB 首次启动（JetPack 36.4.3，SD 卡，2025-01-08 固件）的带注释讲解。你在这里看到的每一行都是正常的 — 理解每个部分的含义可以避免数小时不必要的调试。

#### UEFI 提示符（内核之前）

```
Jetson System firmware version 36.4.3-gcid-38968081 date 2025-01-08
ESC   to enter Setup.
F11   to enter Boot Manager Menu.
Enter to continue boot.
L4TLauncher: Attempting Direct Boot
EFI stub: Booting Linux Kernel...
EFI stub: Using DTB from configuration table
EFI stub: Loaded initrd from LINUX_EFI_INITRD_MEDIA_GUID device path
EFI stub: Exiting boot services...
```

- **36.4.3** = L4T（Linux for Tegra）版本。映射到 JetPack 6.1。
- **L4TLauncher** = NVIDIA 的 EFI 应用程序，选择 A/B 槽并移交内核。
- **EFI stub** = 内核内置的 EFI 加载器。DTB 和 initrd 在此处加载，在 Linux 接管之前。


<details>
<summary>English original</summary>

**CPU Setup**

* Switch to EL1 (Exception Level 1)
* Setup page tables
* Enable MMU
* Initialize SMP cores

**Memory Layout**

8GB RAM is split into:

* **Linux usable RAM** — available to userspace and kernel
* **CMA region** — for GPU and V4L2 allocations
* **Carveouts** for:
  * BPMP (Boot and Power Management Processor)
  * RCE (camera real-time engine)
  * SPE (Safety Processor Engine)
  * OP-TEE (Trusted Execution Environment)
  * Display firmware

Inspect the memory layout with:

```bash
cat /proc/iomem
```

**NVIDIA Driver Stack**

Unlike normal PC Linux, Orin loads Jetson-specific drivers:

* `nvgpu` — GPU driver
* `nvhost` — host1x multimedia subsystem
* `tegra-camrtc` — camera real-time controller
* `vi` — Video Input driver
* `isp` — Image Signal Processor driver
* `nvcsi` — NVIDIA CSI driver

Camera pipeline path:

```
Sensor → NVCSI → VI → ISP → Memory (NVMM) → CUDA / V4L2
```

**1.6 Camera Pipeline (Jetson Specific)**

On Orin Nano, the camera data flow is:

1. Sensor connected via CSI (Camera Serial Interface)
2. NVCSI block receives raw data
3. VI (Video Input) captures frames
4. ISP processes (debayer, denoise, tone-map)
5. Memory allocated via CMA
6. Exposed as `/dev/video0`

Zero-copy path:

* **NVMM memory** — NVIDIA multimedia memory
* **DMA-BUF** — kernel buffer sharing
* **CUDA interop** — direct GPU access without copies

This is why CMA size is critical for camera + AI workloads.

**1.7 initramfs Stage**

Before mounting the full rootfs:

* Loads kernel modules
* Checks root partition
* Handles encryption (if enabled)
* Switches root

Then executes:

```bash
exec /sbin/init
```

**1.8 systemd Stage**

On Jetson Ubuntu, systemd starts:

* `nvargus-daemon` — camera service
* `nvpmodel` service — power mode management
* `networkd` — networking
* `display manager` — GUI
* `docker` — container runtime (if enabled)

NVIDIA-specific services:

* `nvpmodel` controls power modes (e.g., 15W vs 7W)
* `jetson_clocks` tool adjusts clocks for maximum performance

**1.9 Graphics Stack**

Display flow:

```
Kernel DRM driver
 → NVIDIA display controller
  → Wayland (default on recent Ubuntu)
   → GNOME
    → Login screen
```

GPU firmware is loaded during driver probe.

**1.10 Full Boot Chain Summary**

```
Power On
 ↓
BootROM (silicon — immutable)
 ↓
MB1 (QSPI — DRAM training + security)
 ↓
MB2 (QSPI — loads UEFI + OP-TEE + firmware)
 ↓
UEFI (device enumeration + A/B slot selection)
 ↓
Load Kernel + DTB + initrd
 ↓
Linux Kernel (memory + drivers + GPU + camera)
 ↓
Mount APP or APP_b
 ↓
initramfs → switch root
 ↓
systemd (NVIDIA services + desktop)
```

**1.11 Advanced Engineering Details**

**Where Are Bootloaders Stored?**

QSPI NOR flash contains:

* MB1, MB2, UEFI
* Firmware blobs
* Boot configuration tables

Rootfs (APP partition) lives on:

* NVMe (Dev Kit)
* Or eMMC (production module variants)

**Firmware Running Outside Linux**

Even when Linux is running, these microcontrollers remain active:

* **BPMP** — Boot and Power Management Processor
* **SPE** — Safety Processor Engine
* **RCE** — Camera Real-Time Engine

They run their own firmware loaded during boot. Linux communicates with them via mailbox + IPC.

**1.12 What Makes Orin Nano Different From PC?**

| PC Boot            | Jetson Orin Boot              |
|--------------------|-------------------------------|
| BIOS/UEFI          | NVIDIA MB1 + MB2 + UEFI      |
| Simple DRAM init   | Complex LPDDR5 training       |
| Standard GPU       | Firmware-heavy embedded GPU   |
| No carveouts       | Multiple memory carveouts     |
| No BPMP            | Dedicated power MCU (BPMP)    |

Jetson is closer to smartphone architecture than PC — understanding this distinction is key to working effectively with the platform.

**1.13 Reading a Real First-Boot Log**

Below is an annotated walkthrough of an actual Orin Nano 8GB first boot (JetPack 36.4.3, SD card, 2025-01-08 firmware). Every line you will see here is normal — understanding what each section means prevents hours of unnecessary debugging.

**UEFI prompt (before kernel)**

```
Jetson System firmware version 36.4.3-gcid-38968081 date 2025-01-08
ESC   to enter Setup.
F11   to enter Boot Manager Menu.
Enter to continue boot.
L4TLauncher: Attempting Direct Boot
EFI stub: Booting Linux Kernel...
EFI stub: Using DTB from configuration table
EFI stub: Loaded initrd from LINUX_EFI_INITRD_MEDIA_GUID device path
EFI stub: Exiting boot services...
```

- **36.4.3** = L4T (Linux for Tegra) version. Maps to JetPack 6.1.
- **L4TLauncher** = NVIDIA's EFI application that selects A/B slot and hands off to the kernel.
- **EFI stub** = the kernel's built-in EFI loader. DTB and initrd are loaded here, before Linux takes over.

</details>

#### CPU 与内存

```
[    0.000000] Machine model: NVIDIA Jetson Orin Nano Engineering Reference Developer Kit Super
[    0.000000] Linux version 5.15.148-tegra
[    0.000000] Memory: 7517560K/8133248K available (... 353544K reserved, 262144K cma-reserved)
[    0.000000] Reserved memory: created CMA memory pool at 0x000000024a000000, size 256 MiB
[    0.000000] NUMA: Faking a node at [mem 0x0000000080000000-0x0000000277ffffff]
```

- **8 GB 中可用 7.17 GB** — 约 615 MB 被保留的 carveout 占用（BPMP、OP-TEE、固件 blob、SMMU 页表）。
- **256 MB CMA** — 供 GPU 与 V4L2 零拷贝摄像头缓冲区使用。可在设备树中配置。
- **NUMA 伪造** — Orin 是单节点 SoC；Linux 创建一个合成 NUMA 节点以满足 kernel 的 NUMA API。

```
[    0.005418] CPU1: Booted secondary processor 0x0000000100
[    0.005935] CPU2: Booted secondary processor 0x0000000200
[    0.006368] CPU3: Booted secondary processor 0x0000000300
[    0.008843] CPU4: Booted secondary processor 0x0000010200
[    0.009374] CPU5: Booted secondary processor 0x0000010300
[    0.009461] smp: Brought up 1 node, 6 CPUs
```

确认 6 个核。CPU 0–3 位于 cluster 0（MPIDR `0x0000_00XX`）；CPU 4–5 位于 cluster 1（MPIDR `0x0001_02XX`）。这对 CPU 亲和性有影响——把延迟关键型线程隔离到同一个 cluster 可避免跨 cluster 一致性流量。

#### 安全状态

```
[    0.000000] secureboot: Secure boot disabled
[    0.000000] CPU features: detected: Spectre-v4
[    0.000000] CPU features: detected: Spectre-BHB
[    0.000000] CPU features: kernel page table isolation forced ON by KASLR
[    0.000000] CPU features: detected: Kernel page table isolation (KPTI)
```

- **安全启动已禁用** — 开发套件开箱即用的预期状态。启用它需要烧写 fuse（见 Security 一节）。
- **Spectre 缓解已启用** — KPTI 与 SSBS 均开启。有少量性能开销（在系统调用密集的工作负载上约 2–5%）；生产环境不要禁用。

```
[    3.321122] optee: probing for conduit method.
[    3.321179] optee: revision 4.2 (d78bc5fa)
[    3.380504] optee: dynamic shared memory is enabled
[    3.380768] optee: initialized driver
```

OP-TEE（可信执行环境）即使在安全启动禁用时也会运行。它为密钥存储、安全存储和密码学操作提供安全世界侧的支持。

```
I/TC: WARNING: Failed to get monotonic counter for REE FS, using 0
I/TC: WARNING: Failed to commit dirh counter 2
```

> 这两条 OP-TEE 警告会出现在每一台安全启动已禁用的开发套件上。它们意味着安全存储的防回滚计数器无法提交到硬件一次性计数器——而这需要烧写安全启动 fuse。**不是 bug。开发中忽略。**

#### SMMU（IOMMU）

```
arm-smmu 8000000.iommu: SMMUv2 with:
  stage 1 translation, stage 2 translation, nested translation
  stream matching with 128 register groups
  128 context banks
  Stage-1: 48-bit VA -> 48-bit IPA
  Stage-2: 48-bit IPA -> 48-bit PA
```

三个 SMMUv2 实例服务于不同的外设组。SMMU 实施 DMA 隔离——行为异常的外设无法读取任意物理内存。在编写自定义 kernel 驱动或 DMA 引擎时相关。

#### RTCPU IVC 警告

```
tegra-ivc-bus bc00000.rtcpu:ivc-bus:echo@0: ivc channel driver missing
tegra-ivc-bus bc00000.rtcpu:ivc-bus:ivccapture@4: ivc channel driver missing
```

> 这些警告在未接 CSI 摄像头、`tegra-camrtc` 摄像头实时引擎无事可控时出现。**未连接摄像头时属正常。** 一旦接上摄像头并加载完整的 V4L2 摄像头栈，它们就会消失。

#### 存储与分区

```
[    3.909550] mmc0: new ultra high speed SDR104 SDXC card at address 0001
[    3.910216] mmcblk0: mmc0:0001 SD16G 117 GiB
[    3.918828] GPT:Primary header thinks Alt. header is not at the end of the disk.
[    3.918831] GPT:47044607 != 245759999
[    3.918837] GPT: Use GNU Parted to correct GPT errors.
[    3.918855]  mmcblk0: p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 p11 p12 p13 p14 p15
```

GPT 警告在烧写后出现属预期：烧写工具写入的分区表按最小镜像尺寸设定，而物理卡要大得多。修复它以使用整张卡的容量：

```bash
# On the Jetson (after first boot)
sudo parted /dev/mmcblk0 ---pretend-input-tty resizepart 1 Yes 100%
sudo resize2fs /dev/mmcblk0p1
```

15 个分区对 Orin Nano 属正常——它们包括 A/B kernel、DTB、固件 blob，以及 rootfs APP 分区（p1）。

#### nvmap 页池

```
nvmap_page_pool_init: nvmap page pool size: 243839 pages (952 MB)
```

nvmap 预分配 952 MB 的置零页，供 GPU 与多媒体分配使用。这部分从可用 RAM 中扣除。在 8 GB 设备处于内存压力下时，可通过 `nvmap_pagepoolsize` kernel 参数减小它。


<details>
<summary>English original</summary>

**CPU and memory**

```
[    0.000000] Machine model: NVIDIA Jetson Orin Nano Engineering Reference Developer Kit Super
[    0.000000] Linux version 5.15.148-tegra
[    0.000000] Memory: 7517560K/8133248K available (... 353544K reserved, 262144K cma-reserved)
[    0.000000] Reserved memory: created CMA memory pool at 0x000000024a000000, size 256 MiB
[    0.000000] NUMA: Faking a node at [mem 0x0000000080000000-0x0000000277ffffff]
```

- **7.17 GB available of 8 GB** — ~615 MB consumed by reserved carveouts (BPMP, OP-TEE, firmware blobs, SMMU tables).
- **256 MB CMA** — used for GPU and V4L2 zero-copy camera buffers. Configurable in device tree.
- **NUMA faking** — Orin is a single-node SoC; Linux creates a synthetic NUMA node to satisfy the kernel's NUMA APIs.

```
[    0.005418] CPU1: Booted secondary processor 0x0000000100
[    0.005935] CPU2: Booted secondary processor 0x0000000200
[    0.006368] CPU3: Booted secondary processor 0x0000000300
[    0.008843] CPU4: Booted secondary processor 0x0000010200
[    0.009374] CPU5: Booted secondary processor 0x0000010300
[    0.009461] smp: Brought up 1 node, 6 CPUs
```

6 cores confirmed. CPUs 0–3 are in cluster 0 (MPIDR `0x0000_00XX`); CPUs 4–5 are in cluster 1 (MPIDR `0x0001_02XX`). This matters for CPU affinity — isolating latency-critical threads to one cluster avoids cross-cluster coherence traffic.

**Security state**

```
[    0.000000] secureboot: Secure boot disabled
[    0.000000] CPU features: detected: Spectre-v4
[    0.000000] CPU features: detected: Spectre-BHB
[    0.000000] CPU features: kernel page table isolation forced ON by KASLR
[    0.000000] CPU features: detected: Kernel page table isolation (KPTI)
```

- **Secure boot disabled** — expected on the developer kit out of the box. Enabling it requires fuse programming (covered in the Security section).
- **Spectre mitigations active** — KPTI and SSBS are on. Small performance cost (~2–5% on syscall-heavy workloads); do not disable in production.

```
[    3.321122] optee: probing for conduit method.
[    3.321179] optee: revision 4.2 (d78bc5fa)
[    3.380504] optee: dynamic shared memory is enabled
[    3.380768] optee: initialized driver
```

OP-TEE (Trusted Execution Environment) runs even with secure boot disabled. It provides the secure-world side for key storage, secure storage, and cryptographic operations.

```
I/TC: WARNING: Failed to get monotonic counter for REE FS, using 0
I/TC: WARNING: Failed to commit dirh counter 2
```

> These two OP-TEE warnings appear on every developer kit with secure boot disabled. They mean the secure storage anti-rollback counter cannot be committed to a hardware one-time counter — which requires secure boot fuses to be blown. **Not a bug. Ignore in development.**

**SMMU (IOMMU)**

```
arm-smmu 8000000.iommu: SMMUv2 with:
  stage 1 translation, stage 2 translation, nested translation
  stream matching with 128 register groups
  128 context banks
  Stage-1: 48-bit VA -> 48-bit IPA
  Stage-2: 48-bit IPA -> 48-bit PA
```

Three SMMUv2 instances serve different peripheral groups. The SMMU enforces DMA isolation — a misbehaving peripheral cannot read arbitrary physical memory. Relevant when writing custom kernel drivers or DMA engines.

**RTCPU IVC warnings**

```
tegra-ivc-bus bc00000.rtcpu:ivc-bus:echo@0: ivc channel driver missing
tegra-ivc-bus bc00000.rtcpu:ivc-bus:ivccapture@4: ivc channel driver missing
```

> These appear when no CSI camera is attached and the `tegra-camrtc` camera real-time engine has nothing to control. **Normal with no camera connected.** They disappear once you attach a camera and load the full V4L2 camera stack.

**Storage and partitions**

```
[    3.909550] mmc0: new ultra high speed SDR104 SDXC card at address 0001
[    3.910216] mmcblk0: mmc0:0001 SD16G 117 GiB
[    3.918828] GPT:Primary header thinks Alt. header is not at the end of the disk.
[    3.918831] GPT:47044607 != 245759999
[    3.918837] GPT: Use GNU Parted to correct GPT errors.
[    3.918855]  mmcblk0: p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 p11 p12 p13 p14 p15
```

The GPT warning is expected after flashing: the flash tool writes a partition table sized for the minimum image, but the physical card is much larger. Fix it to use the full card capacity:

```bash
# On the Jetson (after first boot)
sudo parted /dev/mmcblk0 ---pretend-input-tty resizepart 1 Yes 100%
sudo resize2fs /dev/mmcblk0p1
```

15 partitions is normal for Orin Nano — they include A/B kernel, DTB, firmware blobs, and the rootfs APP partition (p1).

**nvmap page pool**

```
nvmap_page_pool_init: nvmap page pool size: 243839 pages (952 MB)
```

nvmap pre-allocates 952 MB of zeroed pages for GPU and multimedia allocations. This comes out of available RAM. On an 8 GB device under memory pressure, reduce it via the `nvmap_pagepoolsize` kernel parameter.

</details>

#### 首次启动 USB 串口提示符

```
[   24.092716] Please complete system configuration setup on the serial port
               provided by Jetson's USB device mode connection. e.g. /dev/ttyACMx
```

首次启动配置期间，Jetson 会暴露一个 USB gadget 串口（ACM）。从主机连接：

```bash
sudo screen /dev/ttyACM0 115200
# or
sudo minicom -D /dev/ttyACM0 -b 115200
```

按向导完成 Ubuntu 初始设置（locale、用户创建等）。该向导只出现一次。

#### 启动耗时摘要（来自日志）

| 里程碑 | 时间戳 |
|-----------|-----------|
| UEFI 移交 kernel | 0.000 s |
| 全部 6 个 CPU 上线 | 0.009 s |
| kernel 驱动加载完成 | ~3.5 s |
| rootfs 挂载完成（mmcblk0p1） | 7.9 s |
| systemd 启动 | 8.2 s |
| USB 串口配置提示符 | 24.1 s |

从 SD 卡冷启动到可用 shell 总计：**~25 秒**。NVMe 可将其缩短至 ~15 秒。

---

## 2. Jetson 硬件概览

### Orin Nano 8GB 与 Orin 系列对比

| 模块              | CPU           | GPU         | RAM   | AI TOPS | 功耗  | 用例              |
|---------------------|---------------|-------------|-------|---------|--------|-----------------------|
| Orin Nano 4GB       | 6 核 A78AE  | 512 核 A  | 4GB   | 20      | 5–10W  | 轻量推理       |
| **Orin Nano 8GB**   | **6 核 A78AE** | **1024 核 A** | **8GB** | **40** | **5–15W** | **本指南** |
| Orin NX 8GB         | 6 核 A78AE  | 1024 核 A | 8GB   | 70      | 10–20W | 中端机器人    |
| Orin NX 16GB        | 8 核 A78AE  | 1024 核 A | 16GB  | 100     | 10–25W | 重负载推理       |
| AGX Orin 32GB       | 12 核 A78AE | 2048 核 A | 32GB  | 200     | 15–60W | 自动驾驶汽车   |
| AGX Orin 64GB       | 12 核 A78AE | 2048 核 A | 64GB  | 275     | 15–60W | 完整 AV 系统       |

### Orin Nano 8GB 关键硬件

```
CPU:  6× Arm Cortex-A78AE @ up to 1.5 GHz
GPU:  1024× CUDA cores (Ampere architecture)
      32× Tensor Cores
DLA:  1× Deep Learning Accelerator (up to 10 TOPS)
RAM:  8GB LPDDR5 (shared between CPU + GPU)
Storage: microSD + M.2 NVMe slot (2280)
I/O:
  1× USB 3.2 Gen2 Type-A
  1× USB 3.2 Gen2 Type-C (DisplayPort alt mode)
  1× Gigabit Ethernet
  40-pin GPIO header (I2C, SPI, UART, PWM, I2S)
  M.2 Key M (NVMe SSD)
  M.2 Key E (WiFi/BT)
  Camera connector (CSI-2, 2-lane)
```

> **Tensor Core** 是 Orin Nano 上实现大部分 **40 AI TOPS** 的硬件单元：它们能在一条指令内完成 FP16/INT8 的矩阵乘累加（MMA），这正是 FP16 和 INT8 推理远快于 FP32 的原因。关于 Tensor Core 架构及其工作原理的详细说明，参见 [**Orin Nano — Tensor Core Architecture and How It Works**](14-Orin-Nano张量/Guide.md)。

### 内存架构（对 AI 至关重要）

Orin Nano 采用**统一内存** —— CPU 和 GPU 共享同一物理 LPDDR5 内存池：

```
CPU processes ←──────────────── 8GB LPDDR5 ────────────────→ GPU processes
                          No PCIe transfer needed!
                    Tensors live in unified address space
```

这意味着零拷贝 GPU 推理：CPU 采集的相机帧留在原地，GPU 直接读取，无需拷贝。这是相对于独立 GPU 系统的重大优势。

---

## 3. 安装 JetPack —— 分步指南

### JetPack 是什么？

JetPack 是 NVIDIA 为 Jetson 提供的完整 SDK 栈，包含：
- **L4T (Linux for Tegra)**：基于 Ubuntu 的操作系统，含 Jetson kernel + 驱动
- **CUDA Toolkit**：GPU 编程 runtime
- **cuDNN**：GPU 加速的深度学习原语
- **TensorRT**：优化的推理引擎
- **VPI**（Vision Programming Interface）：硬件加速的 CV
- **DeepStream**：视频分析流水线 SDK
- **Multimedia API**：相机/视频采集

### JetPack 版本 → 软件栈

| JetPack | Ubuntu | CUDA  | cuDNN | TensorRT | L4T     |
|---------|--------|-------|-------|----------|---------|
| 5.1.4   | 20.04  | 11.4  | 8.6   | 8.6      | 35.6.x  |
| 6.0     | 22.04  | 12.2  | 9.0   | 10.0     | 36.3.x  |
| 6.1     | 22.04  | 12.6  | 9.3   | 10.3     | 36.4.x  |
| 6.2     | 22.04  | 12.8  | 9.7   | 10.7     | 36.5.x  |

### 方法 1：SD 卡镜像（最简单 —— 仅限开发套件）

Orin Nano 开发套件可从 microSD 启动。这是上手最快的方式。

```bash
# Step 1: Download the SD card image from NVIDIA
# Go to: developer.nvidia.com/embedded/jetpack
# Select: Jetson Orin Nano Developer Kit → JetPack 6.x → SD Card Image
# File: jp6x-orin-nano-sd-card-image.zip  (~15GB)

# Step 2: Flash with Balena Etcher (GUI) or dd (CLI)
# Using dd (Linux):
unzip jp6x-orin-nano-sd-card-image.zip
sudo dd if=jp6x-orin-nano-sd-card-image.img of=/dev/sdX bs=1M status=progress
# Replace /dev/sdX with your SD card device (check with lsblk)

# Step 3: Insert SD card, connect HDMI + keyboard + power
# System boots into Ubuntu 22.04 setup wizard
```


<details>
<summary>English original</summary>

**First-boot USB serial prompt**

```
[   24.092716] Please complete system configuration setup on the serial port
               provided by Jetson's USB device mode connection. e.g. /dev/ttyACMx
```

The Jetson exposes a USB gadget serial port (ACM) during first-boot setup. Connect from the host:

```bash
sudo screen /dev/ttyACM0 115200
# or
sudo minicom -D /dev/ttyACM0 -b 115200
```

Walk through the Ubuntu initial-setup wizard (locale, user creation, etc.). This only appears once.

**Boot timing summary (from the log)**

| Milestone | Timestamp |
|-----------|-----------|
| UEFI hands off to kernel | 0.000 s |
| All 6 CPUs online | 0.009 s |
| Kernel drivers loaded | ~3.5 s |
| Rootfs mounted (mmcblk0p1) | 7.9 s |
| systemd started | 8.2 s |
| USB serial config prompt | 24.1 s |

Total cold boot to usable shell: **~25 seconds** from SD card. NVMe reduces this to ~15 seconds.

---

**2. Jetson Hardware Overview**

**Orin Nano 8GB vs the Orin Family**

| Module              | CPU           | GPU         | RAM   | AI TOPS | Power  | Use Case              |
|---------------------|---------------|-------------|-------|---------|--------|-----------------------|
| Orin Nano 4GB       | 6-core A78AE  | 512-core A  | 4GB   | 20      | 5–10W  | Light inference       |
| **Orin Nano 8GB**   | **6-core A78AE** | **1024-core A** | **8GB** | **40** | **5–15W** | **This guide** |
| Orin NX 8GB         | 6-core A78AE  | 1024-core A | 8GB   | 70      | 10–20W | Mid-range robotics    |
| Orin NX 16GB        | 8-core A78AE  | 1024-core A | 16GB  | 100     | 10–25W | Heavy inference       |
| AGX Orin 32GB       | 12-core A78AE | 2048-core A | 32GB  | 200     | 15–60W | Autonomous vehicles   |
| AGX Orin 64GB       | 12-core A78AE | 2048-core A | 64GB  | 275     | 15–60W | Full AV systems       |

**Orin Nano 8GB Key Hardware**

```
CPU:  6× Arm Cortex-A78AE @ up to 1.5 GHz
GPU:  1024× CUDA cores (Ampere architecture)
      32× Tensor Cores
DLA:  1× Deep Learning Accelerator (up to 10 TOPS)
RAM:  8GB LPDDR5 (shared between CPU + GPU)
Storage: microSD + M.2 NVMe slot (2280)
I/O:
  1× USB 3.2 Gen2 Type-A
  1× USB 3.2 Gen2 Type-C (DisplayPort alt mode)
  1× Gigabit Ethernet
  40-pin GPIO header (I2C, SPI, UART, PWM, I2S)
  M.2 Key M (NVMe SSD)
  M.2 Key E (WiFi/BT)
  Camera connector (CSI-2, 2-lane)
```

> **Tensor Cores** are the hardware units that deliver most of the **40 AI TOPS** on Orin Nano: they perform matrix multiply-accumulate (MMA) in one instruction for FP16/INT8, which is why FP16 and INT8 inference are so much faster than FP32. For a detailed explanation of tensor core architecture and how they work, see [**Orin Nano — Tensor Core Architecture and How It Works**](14-Orin-Nano张量/Guide.md).

**Memory Architecture (Critical for AI)**

The Orin Nano uses **unified memory** — CPU and GPU share the same physical LPDDR5 pool:

```
CPU processes ←──────────────── 8GB LPDDR5 ────────────────→ GPU processes
                          No PCIe transfer needed!
                    Tensors live in unified address space
```

This means zero-copy GPU inference: camera frames captured by CPU stay in place and the GPU reads them directly without copying. This is a major advantage over discrete GPU systems.

---

**3. Installing JetPack — Step by Step**

**What is JetPack?**

JetPack is NVIDIA's full SDK stack for Jetson. It bundles:
- **L4T (Linux for Tegra)**: Ubuntu-based OS with Jetson kernel + drivers
- **CUDA Toolkit**: GPU programming runtime
- **cuDNN**: GPU-accelerated deep learning primitives
- **TensorRT**: optimized inference engine
- **VPI** (Vision Programming Interface): hardware-accelerated CV
- **DeepStream**: video analytics pipeline SDK
- **Multimedia API**: camera/video capture

**JetPack Version → Software Stack**

| JetPack | Ubuntu | CUDA  | cuDNN | TensorRT | L4T     |
|---------|--------|-------|-------|----------|---------|
| 5.1.4   | 20.04  | 11.4  | 8.6   | 8.6      | 35.6.x  |
| 6.0     | 22.04  | 12.2  | 9.0   | 10.0     | 36.3.x  |
| 6.1     | 22.04  | 12.6  | 9.3   | 10.3     | 36.4.x  |
| 6.2     | 22.04  | 12.8  | 9.7   | 10.7     | 36.5.x  |

**Method 1: SD Card Image (Easiest — Dev Kit Only)**

The Orin Nano Developer Kit can boot from microSD. This is the fastest way to get started.

```bash
# Step 1: Download the SD card image from NVIDIA
# Go to: developer.nvidia.com/embedded/jetpack
# Select: Jetson Orin Nano Developer Kit → JetPack 6.x → SD Card Image
# File: jp6x-orin-nano-sd-card-image.zip  (~15GB)

# Step 2: Flash with Balena Etcher (GUI) or dd (CLI)
# Using dd (Linux):
unzip jp6x-orin-nano-sd-card-image.zip
sudo dd if=jp6x-orin-nano-sd-card-image.img of=/dev/sdX bs=1M status=progress
# Replace /dev/sdX with your SD card device (check with lsblk)

# Step 3: Insert SD card, connect HDMI + keyboard + power
# System boots into Ubuntu 22.04 setup wizard
```

</details>

### 方法 2：SDK Manager（完全控制，需要主机 PC）

SDK Manager 可精确控制要安装哪些 JetPack 组件，并支持配置 NVMe 启动。

**要求：**
- 运行 Ubuntu 20.04 或 22.04 的主机 PC（原生安装，不推荐用 VM）
- 主机 PC 与 Jetson USB-C 端口之间的 USB-C 线缆（数据线，不能是仅充电线）
- Jetson Orin Nano Dev Kit 处于 **恢复模式**

```bash
# ── HOST PC SETUP ──────────────────────────────────────────

# Step 1: Download SDK Manager
# developer.nvidia.com/sdk-manager → download .deb
sudo dpkg -i sdkmanager_*.deb
sudo apt-get install -f   # fix any dependency issues

# Step 2: Launch SDK Manager
sdkmanager

# Step 3: Log in with NVIDIA Developer account

# ── JETSON: ENTER RECOVERY MODE ────────────────────────────
# On Orin Nano Dev Kit:
#   1. Power OFF the board
#   2. Hold the RECOVERY button (3-pin header, middle pin to GND)
#   3. While holding, press POWER button
#   4. Release RECOVERY button after 2 seconds
#   5. Connect USB-C from host PC to Jetson USB-C port

# Verify Jetson is detected in recovery mode:
lsusb | grep NVIDIA
# Should show: NVIDIA Corp. APX
```


<details>
<summary>English original</summary>

**Method 2: SDK Manager (Full Control, Host PC Required)**

SDK Manager gives you precise control over which JetPack components to install and enables NVMe boot configuration.

**Requirements:**
- Host PC running Ubuntu 20.04 or 22.04 (native, not VM recommended)
- USB-C cable (data, not charge-only) between host PC and Jetson USB-C port
- Jetson Orin Nano Dev Kit in **recovery mode**

```bash
# ── HOST PC SETUP ──────────────────────────────────────────

# Step 1: Download SDK Manager
# developer.nvidia.com/sdk-manager → download .deb
sudo dpkg -i sdkmanager_*.deb
sudo apt-get install -f   # fix any dependency issues

# Step 2: Launch SDK Manager
sdkmanager

# Step 3: Log in with NVIDIA Developer account

# ── JETSON: ENTER RECOVERY MODE ────────────────────────────
# On Orin Nano Dev Kit:
#   1. Power OFF the board
#   2. Hold the RECOVERY button (3-pin header, middle pin to GND)
#   3. While holding, press POWER button
#   4. Release RECOVERY button after 2 seconds
#   5. Connect USB-C from host PC to Jetson USB-C port

# Verify Jetson is detected in recovery mode:
lsusb | grep NVIDIA
# Should show: NVIDIA Corp. APX
```

</details>

#### JetPack 6.2.2 组件明细（SDK Manager）

下面是 SDK Manager 中显示的、**Jetson Orin Nano 8GB Developer Kit** 上 **JetPack 6.2.2** 的完整组件列表。逐一理解每个组件的作用——这与 8-layer stack 一一对应。

**Jetson Linux (L4T) — 烧录到设备**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| Jetson Linux Image | 36.5 | 2455 MB | L4 | 完整的 L4T rootfs 镜像（Ubuntu 22.04 基础系统 + NVIDIA 驱动） |
| Drivers for Jetson | 36.5 | 715 MB | L3/L4 | 内核模块：nvgpu、nvhost、摄像头驱动、显示、PCIe、DMA |
| File System and OS | 36.5 | 1749 MB | L4 | 含 NVIDIA 专有软件包与配置的 Ubuntu 22.04 rootfs |
| **Flash Jetson Linux** | **36.5** | **9520 MB** | — | 通过 USB recovery mode 将上述内容写入 eMMC/NVMe |

**Jetson Runtime Components — 安装到目标机**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| Additional Setups | — | 4.1 MB | L4 | 烧录后的配置脚本 |
| DateTime Target Setup | — | 1.0 MB | L4 | NTP / 硬件时钟同步 |
| GStreamer | 36.5 | 1.4 MB | L3 | 多媒体流水线框架（为 DeepStream、摄像头流水线提供输入） |
| DLA Compiler | 36.5 | 2.7 MB | L2 | 为深度学习加速器（DLA）硬件编译 TensorRT layer |

**CUDA & AI Runtime — 推理栈**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| CUDA Runtime | 12.6 | 2199 MB | L3 | GPU runtime：cudart、cudaMemcpy、流、事件、kernel 启动 |
| CUDA X-AI Runtime | — | 1197 MB | L1/L3 | cuBLAS、cuFFT、cuSPARSE、cuSOLVER —— 面向 AI 工作负载的数学库 |
| cuDNN Runtime | 9.3 | 778 MB | L1/L2 | 面向神经网络的优化 conv、attention、normalization kernel |
| TensorRT Runtime | 10.3 | 419 MB | L2/L3 | 图优化、layer 融合、INT8/FP16 engine 构建与执行 |

**计算机视觉 Runtime**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| OpenCV Runtime | 4.8 | 12.1 MB | L1 | 图像处理、特征检测、相机校准 |
| cuPVA Runtime | 2.5 | 0.3 MB | L3 | 可编程视觉加速器 —— Orin 上的硬件 CV 引擎 |
| VPI Runtime | 3.2 | 33.0 MB | L1/L3 | Vision Programming Interface —— CPU/GPU/PVA/DLA 视觉算子的统一 API |

**容器与多媒体**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| NVIDIA Container Runtime | — | 4.9 MB | L3/L4 | 运行可访问 GPU 的 NGC 容器（docker + nvidia-container-toolkit） |
| Multimedia API | 36.5 | 71.9 MB | L3 | NVDEC/NVENC 硬件视频解码/编码、V4L2 摄像头、ISP 访问 |

**Jetson SDK 组件 — 开发者工具（主机 + 目标机）**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| CUDA Toolkit for L4T | 12.6 | 2196 MB | L3 | nvcc 编译器、CUDA 头文件、示例 —— 在端侧构建 CUDA kernel |
| Nsight Systems | 2024.5 | 299 MB | L1–L3 | 全系统性能分析器：CPU、GPU、DLA、内存、流的时序视图 |
| Nsight Graphics | 2024.2 | 196 MB | L1 | 图形调试器与帧性能分析器 |
| DeepStream | 7.1 | 603 MB | L1/L3 | 基于 GStreamer 的多路流视频分析流水线（nvinfer + tracker） |
| GXF Runtime | 4.1 | 466 MB | L3 | Graph eXecution Framework —— 面向 Holoscan/传感器流水线的 dataflow runtime |
| Jetson Platform Services | 2.0 | 0.1 MB | L4 | 用于设备群管理、监控、诊断的系统服务 |

> **总下载量：** ~22 GB。烧录 + 安装后，Orin Nano rootfs 占用约 14 GB。
>
> **如何选择：** 做 AI 推理就全选。做最小化边缘部署时，可跳过 Nsight Graphics、DeepStream 和 GXF —— 之后再通过 `apt` 安装。

```bash
# ── SDK MANAGER STEPS ──────────────────────────────────────
# 1. Select: Jetson Orin Nano [8GB Developer Kit]
# 2. JetPack version: 6.2.2
# 3. Select target components (see tables above)
# 4. Click Continue, accept licenses
# 5. Flashing starts — takes 10–20 minutes
```


<details>
<summary>English original</summary>

**JetPack 6.2.2 Component Breakdown (SDK Manager)**

Below is the exact component list for **JetPack 6.2.2** on **Jetson Orin Nano 8GB Developer Kit** as shown in SDK Manager. Understand what each component does — this maps directly to the 8-layer stack.

**Jetson Linux (L4T) — Flash to device**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| Jetson Linux Image | 36.5 | 2455 MB | L4 | Full L4T root filesystem image (Ubuntu 22.04 base + NVIDIA drivers) |
| Drivers for Jetson | 36.5 | 715 MB | L3/L4 | Kernel modules: nvgpu, nvhost, camera drivers, display, PCIe, DMA |
| File System and OS | 36.5 | 1749 MB | L4 | Ubuntu 22.04 rootfs with NVIDIA-specific packages and config |
| **Flash Jetson Linux** | **36.5** | **9520 MB** | — | Writes all above to eMMC/NVMe via USB recovery mode |

**Jetson Runtime Components — Install on target**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| Additional Setups | — | 4.1 MB | L4 | Post-flash configuration scripts |
| DateTime Target Setup | — | 1.0 MB | L4 | NTP / hardware clock sync |
| GStreamer | 36.5 | 1.4 MB | L3 | Multimedia pipeline framework (feeds DeepStream, camera pipelines) |
| DLA Compiler | 36.5 | 2.7 MB | L2 | Compiles TensorRT layers for Deep Learning Accelerator hardware |

**CUDA & AI Runtime — The inference stack**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| CUDA Runtime | 12.6 | 2199 MB | L3 | GPU runtime: cudart, cudaMemcpy, streams, events, kernel launch |
| CUDA X-AI Runtime | — | 1197 MB | L1/L3 | cuBLAS, cuFFT, cuSPARSE, cuSOLVER — math libraries for AI workloads |
| cuDNN Runtime | 9.3 | 778 MB | L1/L2 | Optimized conv, attention, normalization kernels for neural networks |
| TensorRT Runtime | 10.3 | 419 MB | L2/L3 | Graph optimization, layer fusion, INT8/FP16 engine build + execution |

**Computer Vision Runtime**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| OpenCV Runtime | 4.8 | 12.1 MB | L1 | Image processing, feature detection, camera calibration |
| cuPVA Runtime | 2.5 | 0.3 MB | L3 | Programmable Vision Accelerator — hardware CV engine on Orin |
| VPI Runtime | 3.2 | 33.0 MB | L1/L3 | Vision Programming Interface — unified API for CPU/GPU/PVA/DLA vision ops |

**Container & Multimedia**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| NVIDIA Container Runtime | — | 4.9 MB | L3/L4 | Run NGC containers with GPU access (docker + nvidia-container-toolkit) |
| Multimedia API | 36.5 | 71.9 MB | L3 | NVDEC/NVENC hardware video decode/encode, V4L2 camera, ISP access |

**Jetson SDK Components — Developer tools (host + target)**

| Component | Version | Size | Layer | What it does |
|-----------|---------|------|:-----:|-------------|
| CUDA Toolkit for L4T | 12.6 | 2196 MB | L3 | nvcc compiler, cuda headers, samples — build CUDA kernels on-device |
| Nsight Systems | 2024.5 | 299 MB | L1–L3 | System-wide profiler: timeline view of CPU, GPU, DLA, memory, streams |
| Nsight Graphics | 2024.2 | 196 MB | L1 | Graphics debugger and frame profiler |
| DeepStream | 7.1 | 603 MB | L1/L3 | GStreamer-based multi-stream video analytics pipeline (nvinfer + tracker) |
| GXF Runtime | 4.1 | 466 MB | L3 | Graph eXecution Framework — dataflow runtime for Holoscan/sensor pipelines |
| Jetson Platform Services | 2.0 | 0.1 MB | L4 | System services for fleet management, monitoring, diagnostics |

> **Total download:** ~22 GB. After flash + install, the Orin Nano rootfs uses ~14 GB.
>
> **What to select:** For AI inference work, select everything. For minimal edge deployment, you can skip Nsight Graphics, DeepStream, and GXF — add them later via `apt`.

```bash
# ── SDK MANAGER STEPS ──────────────────────────────────────
# 1. Select: Jetson Orin Nano [8GB Developer Kit]
# 2. JetPack version: 6.2.2
# 3. Select target components (see tables above)
# 4. Click Continue, accept licenses
# 5. Flashing starts — takes 10–20 minutes
```

</details>

### 方法 3：NVMe 启动（生产环境推荐）

microSD 速度慢（读取上限约 ~100 MB/s）且写入寿命有限。NVMe SSD 提供：
- 读取：~3500 MB/s（比 SD 快 35×）
- 写入寿命大幅提高
- 模型加载与数据集访问更快

```bash
# After initial boot from SD card:

# Step 1: Install NVMe SSD in M.2 Key M slot (2280 form factor)
# (power off first, insert SSD, power on)

# Step 2: On Jetson, clone SD card to NVMe
sudo apt-get install pv
sudo dd if=/dev/mmcblk0 | pv | sudo dd of=/dev/nvme0n1 bs=4M status=progress

# Step 3: Expand NVMe partition
sudo parted /dev/nvme0n1 resizepart 1 100%
sudo resize2fs /dev/nvme0n1p1

# Step 4: Update UEFI boot order to prefer NVMe
# On Orin, use: sudo nvbootctrl set-active-boot-slot 0
# Or use NVIDIA's provided extlinux script:
sudo /opt/nvidia/jetson-io/config-by-hardware.py

# Step 5: Reboot and verify
df -h   # root should show NVMe size, not SD size
lsblk   # verify boot partition
```

### 安装后验证

```bash
# Check JetPack version
cat /etc/nv_tegra_release

# Check CUDA
nvcc --version
# Expected: Cuda compilation tools, release 12.x

# Check GPU
nvidia-smi
# Shows: Orin GPU, memory, driver version

# Check TensorRT
dpkg -l | grep tensorrt
python3 -c "import tensorrt; print(tensorrt.__version__)"

# Run NVIDIA system profiler
sudo tegrastats
# Shows: CPU%, GPU%, RAM, power, temperature — live
```

---

## 4. 在 Orin Nano 8GB 上升级 JetPack

### 能否原地升级？

**简短回答：** 小版本升级（例如 6.0 → 6.1）有时可通过 `apt` 完成。大版本升级（例如 5.x → 6.x）**始终需要重新刷写**。

### 通过 apt 进行小版本升级（同一大版本）

```bash
# Add NVIDIA Jetson apt repository
sudo apt-get update
sudo apt-get upgrade   # upgrades L4T packages if available

# Check what version you'll get before upgrading:
apt-cache policy nvidia-l4t-core

# Upgrade specific JetPack components
sudo apt-get install --only-upgrade \
  cuda-toolkit-12-x \
  libcudnn9 \
  tensorrt

# After upgrade, reboot
sudo reboot

# Verify new version
cat /etc/nv_tegra_release
```

**警告：** 原地 apt 升级偶尔会破坏依赖关系。在生产系统上升级前务必制作快照/备份。

### 大版本升级（5.x → 6.x）——需要重新刷写

```
JetPack 5.x (Ubuntu 20.04, CUDA 11.x)
             ↓  Cannot apt-upgrade across major versions
JetPack 6.x (Ubuntu 22.04, CUDA 12.x)

Why reflash?
  - Different Ubuntu base (20.04 → 22.04)
  - Different L4T kernel series (35.x → 36.x)
  - Different bootchain
  - Different partition layout
```

```bash
# Process for Orin Nano 8GB: 5.x → 6.x
# 1. Backup your application data and custom configs
rsync -av /home/user/ /media/backup/

# 2. Note all installed packages
dpkg --get-selections > /media/backup/installed_packages.txt

# 3. Reflash using SDK Manager (Method 2 above)
#    Select JetPack 6.x on host PC

# 4. After flash, reinstall your packages and restore data
# 5. Reinstall Python packages (new Python version on Ubuntu 22.04)
pip3 install -r /media/backup/requirements.txt
```

### 升级决策矩阵

| 当前 → 目标        | 方法       | 耗时    | 风险 | 数据丢失 |
|-------------------------|--------------|---------|------|-----------|
| JP6.0 → JP6.1           | apt upgrade  | 10 min  | Low  | No        |
| JP6.0 → JP6.2           | apt or reflash | 15 min | Med | No (apt) |
| JP5.1.x → JP6.x         | Reflash only | 30 min  | Med  | Yes*      |
| JP4.x → JP5.x/6.x       | Reflash only | 30 min  | High | Yes*      |

*重新刷写前将数据备份至外部存储。

---

## 5. 更高 JetPack 版本的优势

### JetPack 6.x 与 5.x 对比（Orin Nano 语境）

```
JetPack 5.1.4                    JetPack 6.2
─────────────────                ─────────────────────────
Ubuntu 20.04                  →  Ubuntu 22.04 (LTS until 2027)
Python 3.8                    →  Python 3.10
CUDA 11.4                     →  CUDA 12.8
cuDNN 8.6                     →  cuDNN 9.7
TensorRT 8.6                  →  TensorRT 10.7
GCC 9                         →  GCC 11
OpenCV 4.5                    →  OpenCV 4.8
ROS2 Foxy (EOL)               →  ROS2 Humble/Jazzy (supported)
VPI 2.x                       →  VPI 3.x (CUDA graphs, better CPU)
DeepStream 6.x                →  DeepStream 7.x
```


<details>
<summary>English original</summary>

**Method 3: NVMe Boot (Recommended for Production)**

microSD is slow (max ~100 MB/s read) and has limited write endurance. NVMe SSD gives:
- Read: ~3500 MB/s (35× faster than SD)
- Much higher write endurance
- Faster model loading and dataset access

```bash
# After initial boot from SD card:

# Step 1: Install NVMe SSD in M.2 Key M slot (2280 form factor)
# (power off first, insert SSD, power on)

# Step 2: On Jetson, clone SD card to NVMe
sudo apt-get install pv
sudo dd if=/dev/mmcblk0 | pv | sudo dd of=/dev/nvme0n1 bs=4M status=progress

# Step 3: Expand NVMe partition
sudo parted /dev/nvme0n1 resizepart 1 100%
sudo resize2fs /dev/nvme0n1p1

# Step 4: Update UEFI boot order to prefer NVMe
# On Orin, use: sudo nvbootctrl set-active-boot-slot 0
# Or use NVIDIA's provided extlinux script:
sudo /opt/nvidia/jetson-io/config-by-hardware.py

# Step 5: Reboot and verify
df -h   # root should show NVMe size, not SD size
lsblk   # verify boot partition
```

**Post-Install Verification**

```bash
# Check JetPack version
cat /etc/nv_tegra_release

# Check CUDA
nvcc --version
# Expected: Cuda compilation tools, release 12.x

# Check GPU
nvidia-smi
# Shows: Orin GPU, memory, driver version

# Check TensorRT
dpkg -l | grep tensorrt
python3 -c "import tensorrt; print(tensorrt.__version__)"

# Run NVIDIA system profiler
sudo tegrastats
# Shows: CPU%, GPU%, RAM, power, temperature — live
```

---

**4. Upgrading JetPack on Orin Nano 8GB**

**Can You Upgrade In-Place?**

**Short answer:** Minor version upgrades (e.g., 6.0 → 6.1) can sometimes be done via `apt`. Major version upgrades (e.g., 5.x → 6.x) **always require reflashing**.

**Minor Version Upgrade via apt (Same Major Version)**

```bash
# Add NVIDIA Jetson apt repository
sudo apt-get update
sudo apt-get upgrade   # upgrades L4T packages if available

# Check what version you'll get before upgrading:
apt-cache policy nvidia-l4t-core

# Upgrade specific JetPack components
sudo apt-get install --only-upgrade \
  cuda-toolkit-12-x \
  libcudnn9 \
  tensorrt

# After upgrade, reboot
sudo reboot

# Verify new version
cat /etc/nv_tegra_release
```

**Warning:** In-place apt upgrades can occasionally break dependencies. Always snapshot/backup before upgrading production systems.

**Major Version Upgrade (5.x → 6.x) — Reflash Required**

```
JetPack 5.x (Ubuntu 20.04, CUDA 11.x)
             ↓  Cannot apt-upgrade across major versions
JetPack 6.x (Ubuntu 22.04, CUDA 12.x)

Why reflash?
  - Different Ubuntu base (20.04 → 22.04)
  - Different L4T kernel series (35.x → 36.x)
  - Different bootchain
  - Different partition layout
```

```bash
# Process for Orin Nano 8GB: 5.x → 6.x
# 1. Backup your application data and custom configs
rsync -av /home/user/ /media/backup/

# 2. Note all installed packages
dpkg --get-selections > /media/backup/installed_packages.txt

# 3. Reflash using SDK Manager (Method 2 above)
#    Select JetPack 6.x on host PC

# 4. After flash, reinstall your packages and restore data
# 5. Reinstall Python packages (new Python version on Ubuntu 22.04)
pip3 install -r /media/backup/requirements.txt
```

**Upgrade Decision Matrix**

| Current → Target        | Method       | Time    | Risk | Data Loss |
|-------------------------|--------------|---------|------|-----------|
| JP6.0 → JP6.1           | apt upgrade  | 10 min  | Low  | No        |
| JP6.0 → JP6.2           | apt or reflash | 15 min | Med | No (apt) |
| JP5.1.x → JP6.x         | Reflash only | 30 min  | Med  | Yes*      |
| JP4.x → JP5.x/6.x       | Reflash only | 30 min  | High | Yes*      |

*Back up data to external storage before reflash.

---

**5. Benefits of Higher JetPack Versions**

**JetPack 6.x vs 5.x (Orin Nano Context)**

```
JetPack 5.1.4                    JetPack 6.2
─────────────────                ─────────────────────────
Ubuntu 20.04                  →  Ubuntu 22.04 (LTS until 2027)
Python 3.8                    →  Python 3.10
CUDA 11.4                     →  CUDA 12.8
cuDNN 8.6                     →  cuDNN 9.7
TensorRT 8.6                  →  TensorRT 10.7
GCC 9                         →  GCC 11
OpenCV 4.5                    →  OpenCV 4.8
ROS2 Foxy (EOL)               →  ROS2 Humble/Jazzy (supported)
VPI 2.x                       →  VPI 3.x (CUDA graphs, better CPU)
DeepStream 6.x                →  DeepStream 7.x
```

</details>

### 具体收益

**CUDA 12.x 改进：**
```
- CUDA Graph launch overhead: -40% vs CUDA 11.x
- INT8/FP8 Tensor Core utilization: improved Ampere support
- cudaMemcpyAsync improvements: better overlap with computation
- Cooperative Groups improvements
```

**TensorRT 10.x 改进：**
```
- Strongly Typed mode: explicit tensor types throughout the network
- Faster engine build times
- Better INT8 calibration
- New plugins for attention layers (Transformers on edge)
- Improved BF16 support
- Better quantization-aware training (QAT) support
```

**cuDNN 9.x 改进：**
```
- New graph-based API (replaces legacy API)
- Better memory reuse across operations
- Fused attention (FlashAttention-style) on Ampere
```

**JetPack 6 中的安全性改进：**
```
- Secure Boot with anti-rollback protection
- UEFI Secure Boot support
- Disk encryption (DM-Crypt) officially supported
- Measured Boot support
- OTA (Over-the-Air) update infrastructure built-in
```

**ROS2 兼容性：**
```
JetPack 5.x → Ubuntu 20.04 → ROS2 Foxy (EOL 2023) or Galactic (EOL 2022)
JetPack 6.x → Ubuntu 22.04 → ROS2 Humble (LTS until 2027) ← Use this
```

**实用准则：** 除非有明确理由（例如某个依赖尚未完成移植），否则始终使用最新的 JetPack。更新的 JetPack = 更好的性能、更好的安全性、更长的支持周期。

---

## 6. 将 AI 模型移植到 Jetson

### 移植流水线

```
Training Environment          Jetson Deployment
(Desktop/Cloud)               (Orin Nano 8GB)

PyTorch / TensorFlow          TensorRT Engine
     model.pt          →      model.engine
     model.pb          →      (compiled, optimized,
     model.onnx        →       quantized)

Steps:
  1. Train model (any framework)
  2. Export to ONNX (universal format)
  3. Convert ONNX → TensorRT engine
  4. Run inference with TensorRT runtime
```

### 第 1 步：将 PyTorch 模型导出为 ONNX

```python
# On training machine (or Jetson if RAM allows)
import torch
import torch.onnx

model = MyModel()
model.load_state_dict(torch.load("model.pt"))
model.eval()

# Define dummy input matching your model's expected input
dummy_input = torch.randn(1, 3, 640, 640)   # e.g., YOLO input

torch.onnx.export(
    model,
    dummy_input,
    "model.onnx",
    opset_version=17,              # use latest stable
    input_names=["images"],
    output_names=["output"],
    dynamic_axes={
        "images": {0: "batch"},    # dynamic batch size
        "output": {0: "batch"}
    }
)

# Verify ONNX model
import onnx
model_onnx = onnx.load("model.onnx")
onnx.checker.check_model(model_onnx)
print("ONNX export successful")
```

### 第 2 步：在 Jetson 上构建 TensorRT 引擎

**务必在目标 Jetson 上构建 TensorRT 引擎** —— 引擎与硬件相关。

```bash
# Method A: trtexec (command-line tool, easiest)

# FP32 (no quantization)
trtexec --onnx=model.onnx \
        --saveEngine=model_fp32.engine \
        --verbose

# FP16 (2× speedup, <1% accuracy loss typically)
trtexec --onnx=model.onnx \
        --saveEngine=model_fp16.engine \
        --fp16 \
        --verbose

# INT8 (4× speedup, needs calibration data)
trtexec --onnx=model.onnx \
        --saveEngine=model_int8.engine \
        --int8 \
        --calib=calib_data.cache \
        --verbose

# Dynamic batch size (1–16)
trtexec --onnx=model.onnx \
        --saveEngine=model_dynamic.engine \
        --fp16 \
        --minShapes=images:1x3x640x640 \
        --optShapes=images:4x3x640x640 \
        --maxShapes=images:16x3x640x640
```

```python
# Method B: Python TensorRT API (more control)
import tensorrt as trt

TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

def build_engine(onnx_path, engine_path, fp16=True):
    with trt.Builder(TRT_LOGGER) as builder, \
         builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH)) as network, \
         trt.OnnxParser(network, TRT_LOGGER) as parser, \
         builder.create_builder_config() as config:

        config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, 2 << 30)  # 2GB

        if fp16:
            config.set_flag(trt.BuilderFlag.FP16)

        with open(onnx_path, 'rb') as f:
            if not parser.parse(f.read()):
                for error in range(parser.num_errors):
                    print(parser.get_error(error))
                raise RuntimeError("ONNX parse failed")

        serialized = builder.build_serialized_network(network, config)
        with open(engine_path, 'wb') as f:
            f.write(serialized)
        print(f"Engine saved to {engine_path}")

build_engine("model.onnx", "model_fp16.engine", fp16=True)
```


<details>
<summary>English original</summary>

**Concrete Benefits**

**CUDA 12.x improvements:**
```
- CUDA Graph launch overhead: -40% vs CUDA 11.x
- INT8/FP8 Tensor Core utilization: improved Ampere support
- cudaMemcpyAsync improvements: better overlap with computation
- Cooperative Groups improvements
```

**TensorRT 10.x improvements:**
```
- Strongly Typed mode: explicit tensor types throughout the network
- Faster engine build times
- Better INT8 calibration
- New plugins for attention layers (Transformers on edge)
- Improved BF16 support
- Better quantization-aware training (QAT) support
```

**cuDNN 9.x improvements:**
```
- New graph-based API (replaces legacy API)
- Better memory reuse across operations
- Fused attention (FlashAttention-style) on Ampere
```

**Security improvements in JetPack 6:**
```
- Secure Boot with anti-rollback protection
- UEFI Secure Boot support
- Disk encryption (DM-Crypt) officially supported
- Measured Boot support
- OTA (Over-the-Air) update infrastructure built-in
```

**ROS2 compatibility:**
```
JetPack 5.x → Ubuntu 20.04 → ROS2 Foxy (EOL 2023) or Galactic (EOL 2022)
JetPack 6.x → Ubuntu 22.04 → ROS2 Humble (LTS until 2027) ← Use this
```

**Practical rule:** Always use the latest JetPack unless you have a specific reason not to (e.g., a dependency that hasn't been ported yet). Newer JetPack = better performance, better security, longer support.

---

**6. Porting AI Models to Jetson**

**The Porting Pipeline**

```
Training Environment          Jetson Deployment
(Desktop/Cloud)               (Orin Nano 8GB)

PyTorch / TensorFlow          TensorRT Engine
     model.pt          →      model.engine
     model.pb          →      (compiled, optimized,
     model.onnx        →       quantized)

Steps:
  1. Train model (any framework)
  2. Export to ONNX (universal format)
  3. Convert ONNX → TensorRT engine
  4. Run inference with TensorRT runtime
```

**Step 1: Export PyTorch Model to ONNX**

```python
# On training machine (or Jetson if RAM allows)
import torch
import torch.onnx

model = MyModel()
model.load_state_dict(torch.load("model.pt"))
model.eval()

# Define dummy input matching your model's expected input
dummy_input = torch.randn(1, 3, 640, 640)   # e.g., YOLO input

torch.onnx.export(
    model,
    dummy_input,
    "model.onnx",
    opset_version=17,              # use latest stable
    input_names=["images"],
    output_names=["output"],
    dynamic_axes={
        "images": {0: "batch"},    # dynamic batch size
        "output": {0: "batch"}
    }
)

# Verify ONNX model
import onnx
model_onnx = onnx.load("model.onnx")
onnx.checker.check_model(model_onnx)
print("ONNX export successful")
```

**Step 2: Build TensorRT Engine on Jetson**

**Always build TensorRT engines ON the target Jetson** — engines are hardware-specific.

```bash
# Method A: trtexec (command-line tool, easiest)

# FP32 (no quantization)
trtexec --onnx=model.onnx \
        --saveEngine=model_fp32.engine \
        --verbose

# FP16 (2× speedup, <1% accuracy loss typically)
trtexec --onnx=model.onnx \
        --saveEngine=model_fp16.engine \
        --fp16 \
        --verbose

# INT8 (4× speedup, needs calibration data)
trtexec --onnx=model.onnx \
        --saveEngine=model_int8.engine \
        --int8 \
        --calib=calib_data.cache \
        --verbose

# Dynamic batch size (1–16)
trtexec --onnx=model.onnx \
        --saveEngine=model_dynamic.engine \
        --fp16 \
        --minShapes=images:1x3x640x640 \
        --optShapes=images:4x3x640x640 \
        --maxShapes=images:16x3x640x640
```

```python
# Method B: Python TensorRT API (more control)
import tensorrt as trt

TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

def build_engine(onnx_path, engine_path, fp16=True):
    with trt.Builder(TRT_LOGGER) as builder, \
         builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH)) as network, \
         trt.OnnxParser(network, TRT_LOGGER) as parser, \
         builder.create_builder_config() as config:

        config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, 2 << 30)  # 2GB

        if fp16:
            config.set_flag(trt.BuilderFlag.FP16)

        with open(onnx_path, 'rb') as f:
            if not parser.parse(f.read()):
                for error in range(parser.num_errors):
                    print(parser.get_error(error))
                raise RuntimeError("ONNX parse failed")

        serialized = builder.build_serialized_network(network, config)
        with open(engine_path, 'wb') as f:
            f.write(serialized)
        print(f"Engine saved to {engine_path}")

build_engine("model.onnx", "model_fp16.engine", fp16=True)
```

</details>

### 步骤 3：运行 TensorRT 推理

```python
import tensorrt as trt
import numpy as np
import pycuda.driver as cuda
import pycuda.autoinit

class TRTInferencer:
    def __init__(self, engine_path):
        with open(engine_path, 'rb') as f:
            runtime = trt.Runtime(trt.Logger(trt.Logger.WARNING))
            self.engine = runtime.deserialize_cuda_engine(f.read())
        self.context = self.engine.create_execution_context()

        # Allocate device buffers
        self.inputs, self.outputs, self.bindings = [], [], []
        for binding in self.engine:
            size = trt.volume(self.engine.get_binding_shape(binding))
            dtype = trt.nptype(self.engine.get_binding_dtype(binding))
            host_mem = cuda.pagelocked_empty(size, dtype)
            device_mem = cuda.mem_alloc(host_mem.nbytes)
            self.bindings.append(int(device_mem))
            if self.engine.binding_is_input(binding):
                self.inputs.append({'host': host_mem, 'device': device_mem})
            else:
                self.outputs.append({'host': host_mem, 'device': device_mem})
        self.stream = cuda.Stream()

    def infer(self, input_data):
        np.copyto(self.inputs[0]['host'], input_data.ravel())
        cuda.memcpy_htod_async(self.inputs[0]['device'], self.inputs[0]['host'], self.stream)
        self.context.execute_async_v2(bindings=self.bindings, stream_handle=self.stream.handle)
        cuda.memcpy_dtoh_async(self.outputs[0]['host'], self.outputs[0]['device'], self.stream)
        self.stream.synchronize()
        return self.outputs[0]['host']

# Usage
inferencer = TRTInferencer("model_fp16.engine")
frame = np.random.randn(1, 3, 640, 640).astype(np.float32)
result = inferencer.infer(frame)
```

### DLA（深度学习加速器）——额外效率

Orin Nano 有 1 个 DLA 引擎。对于受支持的 layer，DLA 以约 10 TOPS 运行，同时把 GPU 释放出来处理其他任务。

```python
# Enable DLA in TensorRT engine build
config.default_device_type = trt.DeviceType.DLA
config.DLA_core = 0           # Orin Nano has DLA 0 only
config.set_flag(trt.BuilderFlag.GPU_FALLBACK)  # GPU fallback for unsupported layers
config.set_flag(trt.BuilderFlag.FP16)           # DLA requires FP16 or INT8

# Check which layers run on DLA vs GPU after building
# Use: trtexec --onnx=model.onnx --useDLACore=0 --verbose 2>&1 | grep "DLA"
```

---

## 7. ROS2 集成

### 在 JetPack 6（Ubuntu 22.04）上安装 ROS2 Humble

```bash
# Add ROS2 apt repository
sudo apt install -y software-properties-common curl
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
    -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
    http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
    | sudo tee /etc/apt/sources.list.d/ros2.list

sudo apt update
sudo apt install -y ros-humble-desktop          # full install with RViz
# Or for minimal footprint:
sudo apt install -y ros-humble-ros-base         # no GUI tools

# Install build tools
sudo apt install -y python3-colcon-common-extensions python3-rosdep

# Initialize rosdep
sudo rosdep init
rosdep update

# Add to .bashrc
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

### ROS2 + TensorRT：AI 节点架构

```
Camera (CSI/USB) ─→ image_raw ─→ [preprocessing node] ─→ [TensorRT inference node]
                                                                ↓
LiDAR ──────────→ scan ─────────→ [pointcloud node] ──→ [sensor fusion node]
                                                                ↓
IMU ────────────→ imu ──────────→ [EKF node] ───────→ [object tracker]
                                                                ↓
                                                     [control output node]
                                                                ↓
                                                     cmd_vel / actuator commands
```

### 编写 TensorRT 推理 ROS2 节点

```python
#!/usr/bin/env python3
# ros2_trt_node.py

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from vision_msgs.msg import Detection2DArray, Detection2D, BoundingBox2D
from cv_bridge import CvBridge
import numpy as np
import cv2
import tensorrt as trt
import pycuda.driver as cuda
import pycuda.autoinit

class TRTDetectionNode(Node):
    def __init__(self):
        super().__init__('trt_detection_node')

        # Parameters
        self.declare_parameter('engine_path', 'model_fp16.engine')
        self.declare_parameter('confidence_threshold', 0.5)
        self.declare_parameter('input_size', [640, 640])

        engine_path = self.get_parameter('engine_path').value
        self.conf_thresh = self.get_parameter('confidence_threshold').value

        # Load TensorRT engine
        self.engine = self.load_engine(engine_path)
        self.context = self.engine.create_execution_context()
        self.allocate_buffers()

        self.bridge = CvBridge()

        # Subscribe to camera image
        self.sub = self.create_subscription(
            Image,
            '/camera/image_raw',
            self.image_callback,
            10
        )

        # Publish detections
        self.pub = self.create_publisher(Detection2DArray, '/detections', 10)

        self.get_logger().info(f'TRT inference node started: {engine_path}')

    def load_engine(self, path):
        with open(path, 'rb') as f:
            runtime = trt.Runtime(trt.Logger(trt.Logger.WARNING))
            return runtime.deserialize_cuda_engine(f.read())

    def allocate_buffers(self):
        self.inputs, self.outputs, self.bindings = [], [], []
        self.stream = cuda.Stream()
        for binding in self.engine:
            size = trt.volume(self.engine.get_binding_shape(binding))
            host = cuda.pagelocked_empty(size, np.float32)
            device = cuda.mem_alloc(host.nbytes)
            self.bindings.append(int(device))
            if self.engine.binding_is_input(binding):
                self.inputs.append({'host': host, 'device': device})
            else:
                self.outputs.append({'host': host, 'device': device})

    def preprocess(self, img):
        img = cv2.resize(img, (640, 640))
        img = img[:, :, ::-1].transpose(2, 0, 1)  # BGR→RGB, HWC→CHW
        img = img.astype(np.float32) / 255.0
        img = np.expand_dims(img, 0)               # add batch dim
        return np.ascontiguousarray(img)

    def infer(self, data):
        np.copyto(self.inputs[0]['host'], data.ravel())
        cuda.memcpy_htod_async(self.inputs[0]['device'], self.inputs[0]['host'], self.stream)
        self.context.execute_async_v2(self.bindings, self.stream.handle)
        cuda.memcpy_dtoh_async(self.outputs[0]['host'], self.outputs[0]['device'], self.stream)
        self.stream.synchronize()
        return self.outputs[0]['host'].copy()

    def image_callback(self, msg):
        # Convert ROS Image to OpenCV
        frame = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')

        # Preprocess
        input_data = self.preprocess(frame)

        # Run inference
        output = self.infer(input_data)

        # Parse output and publish
        detections = self.parse_detections(output, frame.shape, msg.header)
        self.pub.publish(detections)

    def parse_detections(self, output, img_shape, header):
        # Implement based on your model's output format
        # Example for YOLO-style output
        detections = Detection2DArray()
        detections.header = header
        # ... parse boxes, scores, classes from output ...
        return detections

def main(args=None):
    rclpy.init(args=args)
    node = TRTDetectionNode()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()
```

### ROS2 节点性能：Executor 策略

```python
# Single-threaded executor (default): simple, no race conditions
rclpy.spin(node)

# Multi-threaded executor: callbacks run in parallel
from rclpy.executors import MultiThreadedExecutor
from rclpy.callback_groups import ReentrantCallbackGroup, MutuallyExclusiveCallbackGroup

class MyNode(Node):
    def __init__(self):
        super().__init__('my_node')
        # Camera callback: can overlap with LiDAR callback
        camera_group = MutuallyExclusiveCallbackGroup()
        lidar_group = MutuallyExclusiveCallbackGroup()

        self.camera_sub = self.create_subscription(
            Image, '/camera/image_raw',
            self.camera_cb, 10,
            callback_group=camera_group
        )
        self.lidar_sub = self.create_subscription(
            PointCloud2, '/lidar/points',
            self.lidar_cb, 10,
            callback_group=lidar_group
        )

executor = MultiThreadedExecutor(num_threads=4)
executor.add_node(node)
executor.spin()
```

### 面向传感器数据的 ROS2 QoS

```python
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSHistoryPolicy, QoSDurabilityPolicy

# Sensor data (real-time, drop old frames, don't retry)
sensor_qos = QoSProfile(
    reliability=QoSReliabilityPolicy.BEST_EFFORT,  # drop if can't deliver
    history=QoSHistoryPolicy.KEEP_LAST,
    depth=1,                                        # only care about latest frame
    durability=QoSDurabilityPolicy.VOLATILE
)

# Control commands (must be delivered, no drops)
control_qos = QoSProfile(
    reliability=QoSReliabilityPolicy.RELIABLE,
    history=QoSHistoryPolicy.KEEP_LAST,
    depth=10
)

self.camera_sub = self.create_subscription(
    Image, '/camera/image_raw',
    self.camera_cb, sensor_qos   # Use sensor QoS
)
self.cmd_pub = self.create_publisher(
    Twist, '/cmd_vel', control_qos  # Use reliable QoS
)
```

---

## 8. 优化 AI 推理

> **深度剖析：** 关于 DLA 专项优化——硬件架构、TensorRT DLA 集成、支持的 layer、多引擎调度（DLA + GPU）、性能剖析与生产部署模式——参见 [**Orin Nano DLA Deep Dive**](04-Orin-Nano-DLA深入解析/Guide.md)。

> **深度剖析：** 关于 Jetson 上的 CUDA 编程——统一内存、kernel 优化、流、共享内存、GStreamer/TensorRT 集成，以及功耗感知的 CUDA 模式——参见 [**Orin Nano CUDA Programming**](01-Orin-Nano-CUDA编程/Guide.md)。

> **深度剖析：** 关于实时与确定性推理——TensorRT engine 优化、DLA 延迟一致性、CUDA graphs、CPU 隔离、多模型调度、Triton 推理服务，以及 p99 延迟性能剖析——参见 [**Orin Nano Real-Time & Deterministic Inference**](11-Orin-Nano实时推理/Guide.md)。
>
> 关于内核级 RT Linux 内部机制——PREEMPT_RT 补丁架构、中断线程化、锁原语、优先级反转/PI mutex、SCHED_DEADLINE、rt-tests 套件、ARM Cortex-A78AE 特性、GICv3 调优、WCET 分析、RT 安全的内核模块，以及 ftrace 调试——参见 [**Orin Nano RT Linux Deep Dive**](10-Orin-Nano-RT-Linux深入解析/Guide.md)。

### 精度取舍

```
Precision    Memory   Speed      Accuracy   Use Case
FP32         100%     1×         Baseline   Training, debug
FP16         50%      2–3×       ~0.1% drop General inference
INT8         25%      3–5×       ~1% drop   Production
INT4         12.5%    4–8×       ~3% drop   Very edge, LLMs

On Orin Nano (Ampere GPU):
  FP16 Tensor Cores: native support, excellent
  INT8 Tensor Cores: native support, fastest path
  FP32: no Tensor Core acceleration
```

### CUDA 流：工作重叠

```python
import pycuda.driver as cuda

stream_inference = cuda.Stream()
stream_preprocess = cuda.Stream()

# Pipeline: while GPU is running inference on frame N,
#           CPU is preprocessing frame N+1

with cuda.Stream() as s1, cuda.Stream() as s2:
    # Frame N: copy to GPU on s1
    cuda.memcpy_htod_async(gpu_input_n, cpu_frame_n, s1)

    # Frame N: run inference on s1
    context.execute_async_v2(bindings, s1.handle)

    # Frame N+1: preprocessing on CPU happens concurrently
    cpu_frame_n1 = preprocess(next_raw_frame)

    # Sync s1, get result
    s1.synchronize()
    cuda.memcpy_dtoh_async(cpu_output_n, gpu_output_n, s1)
```

### CUDA Graphs：降低启动开销

对于固定的推理工作负载（每次调用输入 shape 相同），CUDA Graphs 可消除 kernel 启动开销：

```python
import torch

model = model.cuda().half()

# Warm up
dummy = torch.randn(1, 3, 640, 640, device='cuda', dtype=torch.float16)
for _ in range(3):
    _ = model(dummy)

# Capture CUDA graph
g = torch.cuda.CUDAGraph()
with torch.cuda.graph(g):
    output = model(dummy)

# Replay graph (ultra-low overhead)
def fast_infer(x):
    dummy.copy_(x)    # update input in-place
    g.replay()        # replay captured graph
    return output.clone()
```

### 批推理策略

```
Single-frame inference (typical naive approach):
  Frame → [wait] → Model → Result → Frame → [wait] → ...
  Throughput: 1 frame / inference_time

Batched inference (correct approach):
  Accumulate frames → [batch=4] → Model → 4 results
  Throughput: 4 frames / inference_time (same GPU time!)
  Latency: slightly higher, but throughput multiplied

Dynamic batching: accept 1–N frames, fill timeout or max batch
```

### 推理流水线的性能剖析

```bash
# 1. Quick throughput benchmark
trtexec --loadEngine=model_fp16.engine \
        --batch=1 \
        --iterations=100 \
        --warmUp=500 \
        --avgRuns=100

# 2. System-wide profiling with Nsight Systems
nsys profile \
    --trace=cuda,cudnn,tensorrt,osrt \
    --output=profile \
    python3 inference_script.py

# View report:
nsys-ui profile.qdrep

# 3. GPU kernel profiling with Nsight Compute
ncu --set full \
    --target-processes all \
    python3 inference_script.py

# 4. Jetson-specific: tegrastats
sudo tegrastats --interval 100 | tee tegrastats.log
```


<details>
<summary>English original</summary>

**ROS2 QoS for Sensor Data**

```python
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSHistoryPolicy, QoSDurabilityPolicy

# Sensor data (real-time, drop old frames, don't retry)
sensor_qos = QoSProfile(
    reliability=QoSReliabilityPolicy.BEST_EFFORT,  # drop if can't deliver
    history=QoSHistoryPolicy.KEEP_LAST,
    depth=1,                                        # only care about latest frame
    durability=QoSDurabilityPolicy.VOLATILE
)

# Control commands (must be delivered, no drops)
control_qos = QoSProfile(
    reliability=QoSReliabilityPolicy.RELIABLE,
    history=QoSHistoryPolicy.KEEP_LAST,
    depth=10
)

self.camera_sub = self.create_subscription(
    Image, '/camera/image_raw',
    self.camera_cb, sensor_qos   # Use sensor QoS
)
self.cmd_pub = self.create_publisher(
    Twist, '/cmd_vel', control_qos  # Use reliable QoS
)
```

---

**8. Optimizing AI Inference**

> **Deep dive:** For DLA-specific optimization — hardware architecture, TensorRT DLA integration, supported layers, multi-engine scheduling (DLA + GPU), profiling, and production deployment patterns — see [**Orin Nano DLA Deep Dive**](04-Orin-Nano-DLA深入解析/Guide.md).

> **Deep dive:** For CUDA programming on Jetson — unified memory, kernel optimization, streams, shared memory, GStreamer/TensorRT integration, and power-aware CUDA patterns — see [**Orin Nano CUDA Programming**](01-Orin-Nano-CUDA编程/Guide.md).

> **Deep dive:** For real-time and deterministic inference — TensorRT engine optimization, DLA latency consistency, CUDA graphs, CPU isolation, multi-model scheduling, Triton serving, and p99 latency profiling — see [**Orin Nano Real-Time & Deterministic Inference**](11-Orin-Nano实时推理/Guide.md).
>
> For kernel-level RT Linux internals — PREEMPT_RT patch architecture, interrupt threading, lock primitives, priority inversion/PI mutexes, SCHED_DEADLINE, rt-tests suite, ARM Cortex-A78AE specifics, GICv3 tuning, WCET analysis, RT-safe kernel modules, and ftrace debugging — see [**Orin Nano RT Linux Deep Dive**](10-Orin-Nano-RT-Linux深入解析/Guide.md).

**Precision Tradeoffs**

```
Precision    Memory   Speed      Accuracy   Use Case
FP32         100%     1×         Baseline   Training, debug
FP16         50%      2–3×       ~0.1% drop General inference
INT8         25%      3–5×       ~1% drop   Production
INT4         12.5%    4–8×       ~3% drop   Very edge, LLMs

On Orin Nano (Ampere GPU):
  FP16 Tensor Cores: native support, excellent
  INT8 Tensor Cores: native support, fastest path
  FP32: no Tensor Core acceleration
```

**CUDA Streams: Overlapping Work**

```python
import pycuda.driver as cuda

stream_inference = cuda.Stream()
stream_preprocess = cuda.Stream()

# Pipeline: while GPU is running inference on frame N,
#           CPU is preprocessing frame N+1

with cuda.Stream() as s1, cuda.Stream() as s2:
    # Frame N: copy to GPU on s1
    cuda.memcpy_htod_async(gpu_input_n, cpu_frame_n, s1)

    # Frame N: run inference on s1
    context.execute_async_v2(bindings, s1.handle)

    # Frame N+1: preprocessing on CPU happens concurrently
    cpu_frame_n1 = preprocess(next_raw_frame)

    # Sync s1, get result
    s1.synchronize()
    cuda.memcpy_dtoh_async(cpu_output_n, gpu_output_n, s1)
```

**CUDA Graphs: Reduce Launch Overhead**

For a fixed inference workload (same input shape every call), CUDA Graphs eliminate kernel launch overhead:

```python
import torch

model = model.cuda().half()

# Warm up
dummy = torch.randn(1, 3, 640, 640, device='cuda', dtype=torch.float16)
for _ in range(3):
    _ = model(dummy)

# Capture CUDA graph
g = torch.cuda.CUDAGraph()
with torch.cuda.graph(g):
    output = model(dummy)

# Replay graph (ultra-low overhead)
def fast_infer(x):
    dummy.copy_(x)    # update input in-place
    g.replay()        # replay captured graph
    return output.clone()
```

**Batch Inference Strategy**

```
Single-frame inference (typical naive approach):
  Frame → [wait] → Model → Result → Frame → [wait] → ...
  Throughput: 1 frame / inference_time

Batched inference (correct approach):
  Accumulate frames → [batch=4] → Model → 4 results
  Throughput: 4 frames / inference_time (same GPU time!)
  Latency: slightly higher, but throughput multiplied

Dynamic batching: accept 1–N frames, fill timeout or max batch
```

**Profiling the Inference Pipeline**

```bash
# 1. Quick throughput benchmark
trtexec --loadEngine=model_fp16.engine \
        --batch=1 \
        --iterations=100 \
        --warmUp=500 \
        --avgRuns=100

# 2. System-wide profiling with Nsight Systems
nsys profile \
    --trace=cuda,cudnn,tensorrt,osrt \
    --output=profile \
    python3 inference_script.py

# View report:
nsys-ui profile.qdrep

# 3. GPU kernel profiling with Nsight Compute
ncu --set full \
    --target-processes all \
    python3 inference_script.py

# 4. Jetson-specific: tegrastats
sudo tegrastats --interval 100 | tee tegrastats.log
```

</details>

### 延迟测量

```python
import time
import numpy as np

def benchmark_inference(inferencer, input_data, n_runs=200, warmup=50):
    # Warmup (important: first runs include JIT compilation)
    for _ in range(warmup):
        inferencer.infer(input_data)

    # Measure
    times = []
    for _ in range(n_runs):
        t0 = time.perf_counter()
        inferencer.infer(input_data)
        times.append((time.perf_counter() - t0) * 1000)  # ms

    times = np.array(times)
    print(f"Latency: mean={times.mean():.2f}ms  "
          f"p50={np.percentile(times,50):.2f}ms  "
          f"p95={np.percentile(times,95):.2f}ms  "
          f"p99={np.percentile(times,99):.2f}ms")
    print(f"Throughput: {1000/times.mean():.1f} FPS")

benchmark_inference(inferencer, dummy_input)
```

---

## 9. 端到端 AI 流水线：传感器 → 推理 → 控制

> **深入阅读：** 硬件视频编解码器、GStreamer 加速流水线与 DeepStream SDK —— NVDEC/NVENC 规格、转码、多流推理、分析、零拷贝视频流水线与 RTSP 推理服务 —— 参见 [**Orin Nano Video Codec & DeepStream**](15-Orin-Nano视频编解码DeepStream/Guide.md).

### 完整流水线架构

```
┌─────────────────────────────────────────────────────────────────────┐
│  SENSOR LAYER                                                        │
│  Camera (30fps) ──► CSI/USB → V4L2 → CUDA buffer                   │
│  LiDAR (10Hz)  ──► Ethernet/USB → PointCloud buffer                 │
│  IMU (100Hz)   ──► I2C/SPI → ring buffer                            │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────────┐
│  PREPROCESSING LAYER (GPU — CUDA/VPI)                                │
│  Image: resize, normalize, color convert (CUDA)                      │
│  PointCloud: voxelization, ground removal (CUDA)                     │
│  IMU: integration, bias correction (CPU)                             │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────────┐
│  INFERENCE LAYER (GPU — TensorRT)                                    │
│  Object Detection: YOLO / SSD on camera frames                       │
│  PointCloud Detection: PointPillars on LiDAR scan                    │
│  Fusion: BEVFusion / simple box fusion                               │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────────┐
│  FUSION & TRACKING LAYER (CPU + GPU)                                 │
│  EKF/UKF: fuse IMU + camera + LiDAR detections                      │
│  Object tracker: Hungarian algorithm + Kalman filter                 │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────────┐
│  CONTROL LAYER (CPU — real-time)                                     │
│  Path planner (A*, DWA, MPC)                                         │
│  Velocity/steering commands → actuators                              │
└─────────────────────────────────────────────────────────────────────┘
```

### 零拷贝摄像头流水线（优化后）

```python
# Use GStreamer + nvarguscamerasrc for zero-copy CSI camera pipeline
import cv2

# CSI camera (native, zero-copy, hardware-accelerated)
gst_pipeline = (
    "nvarguscamerasrc sensor-id=0 ! "
    "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! "
    "nvvidconv ! "                                    # stays in GPU memory
    "video/x-raw, format=BGRx ! "
    "videoconvert ! "
    "video/x-raw, format=BGR ! "
    "appsink drop=1"
)

cap = cv2.VideoCapture(gst_pipeline, cv2.CAP_GSTREAMER)
```

```python
# Better: use pycuda + nvargus for true zero-copy (frame stays in GPU)
# or use NVIDIA's Jetson.Utils library:
import jetson.utils as ju

camera = ju.videoSource("csi://0", argv=["--width=1920", "--height=1080", "--framerate=30"])
display = ju.videoOutput("display://0")

while True:
    img = camera.Capture()         # CUDA image, stays on GPU
    # img is a jetson.utils.cudaImage — pointer into shared GPU memory
    # pass directly to TensorRT without any CPU roundtrip
    result = detect(img)
    display.Render(img)
```

### 流水线时序预算（示例：30 FPS = 33ms 每帧）

```
Budget: 33ms total for one pipeline cycle at 30 FPS

Camera capture:     2ms   (DMA from ISP to memory)
Preprocessing:      3ms   (GPU: resize, normalize)
TensorRT inference: 10ms  (GPU: FP16 detection model)
LiDAR processing:   5ms   (GPU: voxelization, PointPillars)
Sensor fusion:      4ms   (CPU: EKF update)
Object tracking:    2ms   (CPU: Hungarian + Kalman)
Path planning:      4ms   (CPU: DWA or A*)
Control output:     1ms   (CAN/UART command send)
Margin:             2ms
─────────────────────────
Total:             33ms = 30 FPS ✓
```

### 用线程实现流水线

```python
import threading
import queue
import time

# Thread-safe queues between pipeline stages
raw_frame_q = queue.Queue(maxsize=2)     # camera → preprocess
gpu_frame_q  = queue.Queue(maxsize=2)    # preprocess → inference
detection_q  = queue.Queue(maxsize=2)    # inference → fusion
control_q    = queue.Queue(maxsize=2)    # fusion → control

def camera_thread():
    """Runs at camera frequency (30 Hz)"""
    while True:
        ret, frame = cap.read()
        if not raw_frame_q.full():
            raw_frame_q.put_nowait(frame)

def preprocess_thread():
    """GPU preprocessing"""
    while True:
        frame = raw_frame_q.get()
        processed = gpu_preprocess(frame)  # CUDA kernel
        gpu_frame_q.put(processed)

def inference_thread():
    """TensorRT inference"""
    while True:
        processed = gpu_frame_q.get()
        detections = trt_infer(processed)
        detection_q.put(detections)

def control_thread():
    """Control loop — must run at fixed rate"""
    while True:
        if not detection_q.empty():
            detections = detection_q.get_nowait()
            cmd = compute_control(detections)
            send_command(cmd)
        time.sleep(0.01)  # 100 Hz control loop

# Start all threads
threads = [
    threading.Thread(target=camera_thread, daemon=True),
    threading.Thread(target=preprocess_thread, daemon=True),
    threading.Thread(target=inference_thread, daemon=True),
    threading.Thread(target=control_thread, daemon=True),
]
for t in threads:
    t.start()
```

---

## 10. LiDAR（激光雷达）、摄像头、IMU — 实战集成

> **深入阅读：** 摄像头子系统内部细节 —— NVCSI/VI/ISP 硬件、sensor 驱动开发、设备树配置、ISP 调优、Libargus API、多摄像头同步，以及摄像头到 CUDA 的零拷贝 —— 见 [**Orin Nano Camera ISP & Sensor Bringup**](02-Orin-Nano摄像头ISP传感器启动/Guide.md)。

> **深入阅读：** 40-pin 排针外设 —— GPIO、I2C、SPI、UART、PWM、CAN 总线、引脚复用、sensor 集成、电机控制，以及自定义设备树 overlay —— 见 [**Orin Nano GPIO/SPI/I2C/CAN**](05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)。

### 摄像头集成

#### CSI 摄像头（性能场景推荐）

```bash
# Check if CSI camera is detected
sudo dmesg | grep imx          # IMX219, IMX477 modules
v4l2-ctl --list-devices

# Test: capture a frame
nvgstcapture-1.0 --sensor-id=0

# List available formats
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

```python
# Camera calibration with OpenCV (critical for accurate 3D projection)
import cv2
import numpy as np

# After capturing calibration images with checkerboard:
objpoints = []  # 3D world points
imgpoints = []  # 2D image points

# ... fill objpoints and imgpoints from checkerboard detection ...

ret, camera_matrix, dist_coeffs, rvecs, tvecs = cv2.calibrateCamera(
    objpoints, imgpoints, gray.shape[::-1], None, None
)

print(f"Camera matrix:\n{camera_matrix}")
print(f"Distortion coeffs: {dist_coeffs}")

# Save for use in ROS2 camera_info topic
np.save('camera_matrix.npy', camera_matrix)
np.save('dist_coeffs.npy', dist_coeffs)
```

#### USB 摄像头配置

```bash
# Identify camera
ls /dev/video*
v4l2-ctl --device=/dev/video0 --list-formats-ext

# Check USB speed (should be USB 3.x for high-res)
lsusb -t

# Set optimal format
v4l2-ctl --device=/dev/video0 \
    --set-fmt-video=width=1280,height=720,pixelformat=MJPG
```

### LiDAR（激光雷达）集成

#### RPLIDAR（常见、低成本）

```bash
# Install RPLIDAR ROS2 driver
sudo apt-get install ros-humble-rplidar-ros

# Connect via USB, find port
ls /dev/ttyUSB*
sudo chmod 666 /dev/ttyUSB0

# Launch
ros2 launch rplidar_ros rplidar_a2_launch.py \
    serial_port:=/dev/ttyUSB0 \
    serial_baudrate:=115200 \
    frame_id:=laser
```

#### Velodyne LiDAR（高端）

```bash
sudo apt-get install ros-humble-velodyne

# Velodyne connects via Ethernet (192.168.1.201 by default)
sudo ip addr add 192.168.1.100/24 dev eth0

ros2 launch velodyne_driver velodyne_driver_node-VLP16-launch.py
```


<details>
<summary>English original</summary>

**Pipeline Timing Budget (Example: 30 FPS = 33ms per frame)**

```
Budget: 33ms total for one pipeline cycle at 30 FPS

Camera capture:     2ms   (DMA from ISP to memory)
Preprocessing:      3ms   (GPU: resize, normalize)
TensorRT inference: 10ms  (GPU: FP16 detection model)
LiDAR processing:   5ms   (GPU: voxelization, PointPillars)
Sensor fusion:      4ms   (CPU: EKF update)
Object tracking:    2ms   (CPU: Hungarian + Kalman)
Path planning:      4ms   (CPU: DWA or A*)
Control output:     1ms   (CAN/UART command send)
Margin:             2ms
─────────────────────────
Total:             33ms = 30 FPS ✓
```

**Pipelining with Threads**

```python
import threading
import queue
import time

# Thread-safe queues between pipeline stages
raw_frame_q = queue.Queue(maxsize=2)     # camera → preprocess
gpu_frame_q  = queue.Queue(maxsize=2)    # preprocess → inference
detection_q  = queue.Queue(maxsize=2)    # inference → fusion
control_q    = queue.Queue(maxsize=2)    # fusion → control

def camera_thread():
    """Runs at camera frequency (30 Hz)"""
    while True:
        ret, frame = cap.read()
        if not raw_frame_q.full():
            raw_frame_q.put_nowait(frame)

def preprocess_thread():
    """GPU preprocessing"""
    while True:
        frame = raw_frame_q.get()
        processed = gpu_preprocess(frame)  # CUDA kernel
        gpu_frame_q.put(processed)

def inference_thread():
    """TensorRT inference"""
    while True:
        processed = gpu_frame_q.get()
        detections = trt_infer(processed)
        detection_q.put(detections)

def control_thread():
    """Control loop — must run at fixed rate"""
    while True:
        if not detection_q.empty():
            detections = detection_q.get_nowait()
            cmd = compute_control(detections)
            send_command(cmd)
        time.sleep(0.01)  # 100 Hz control loop

# Start all threads
threads = [
    threading.Thread(target=camera_thread, daemon=True),
    threading.Thread(target=preprocess_thread, daemon=True),
    threading.Thread(target=inference_thread, daemon=True),
    threading.Thread(target=control_thread, daemon=True),
]
for t in threads:
    t.start()
```

---

**10. LiDAR, Camera, IMU — Practical Integration**

> **Deep dive:** For camera subsystem internals — NVCSI/VI/ISP hardware, sensor driver development, device tree configuration, ISP tuning, Libargus API, multi-camera sync, and camera-to-CUDA zero-copy — see [**Orin Nano Camera ISP & Sensor Bringup**](02-Orin-Nano摄像头ISP传感器启动/Guide.md).

> **Deep dive:** For 40-pin header peripherals — GPIO, I2C, SPI, UART, PWM, CAN bus, pin multiplexing, sensor integration, motor control, and custom device tree overlays — see [**Orin Nano GPIO/SPI/I2C/CAN**](05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md).

**Camera Integration**

**CSI Camera (Recommended for performance)**

```bash
# Check if CSI camera is detected
sudo dmesg | grep imx          # IMX219, IMX477 modules
v4l2-ctl --list-devices

# Test: capture a frame
nvgstcapture-1.0 --sensor-id=0

# List available formats
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

```python
# Camera calibration with OpenCV (critical for accurate 3D projection)
import cv2
import numpy as np

# After capturing calibration images with checkerboard:
objpoints = []  # 3D world points
imgpoints = []  # 2D image points

# ... fill objpoints and imgpoints from checkerboard detection ...

ret, camera_matrix, dist_coeffs, rvecs, tvecs = cv2.calibrateCamera(
    objpoints, imgpoints, gray.shape[::-1], None, None
)

print(f"Camera matrix:\n{camera_matrix}")
print(f"Distortion coeffs: {dist_coeffs}")

# Save for use in ROS2 camera_info topic
np.save('camera_matrix.npy', camera_matrix)
np.save('dist_coeffs.npy', dist_coeffs)
```

**USB Camera Setup**

```bash
# Identify camera
ls /dev/video*
v4l2-ctl --device=/dev/video0 --list-formats-ext

# Check USB speed (should be USB 3.x for high-res)
lsusb -t

# Set optimal format
v4l2-ctl --device=/dev/video0 \
    --set-fmt-video=width=1280,height=720,pixelformat=MJPG
```

**LiDAR Integration**

**RPLIDAR (Common, low-cost)**

```bash
# Install RPLIDAR ROS2 driver
sudo apt-get install ros-humble-rplidar-ros

# Connect via USB, find port
ls /dev/ttyUSB*
sudo chmod 666 /dev/ttyUSB0

# Launch
ros2 launch rplidar_ros rplidar_a2_launch.py \
    serial_port:=/dev/ttyUSB0 \
    serial_baudrate:=115200 \
    frame_id:=laser
```

**Velodyne LiDAR (High-end)**

```bash
sudo apt-get install ros-humble-velodyne

# Velodyne connects via Ethernet (192.168.1.201 by default)
sudo ip addr add 192.168.1.100/24 dev eth0

ros2 launch velodyne_driver velodyne_driver_node-VLP16-launch.py
```

</details>

#### 点云处理

```python
# ROS2 PointCloud2 → numpy
from sensor_msgs.msg import PointCloud2
import sensor_msgs_py.point_cloud2 as pc2
import numpy as np

def lidar_callback(self, msg):
    # Extract XYZ points
    points = np.array(list(pc2.read_points(
        msg, field_names=('x', 'y', 'z', 'intensity'),
        skip_nans=True
    )))

    if len(points) == 0:
        return

    xyz = points[:, :3]                    # shape [N, 3]
    intensity = points[:, 3:4]             # shape [N, 1]

    # Ground removal (simple height filter)
    mask = xyz[:, 2] > -0.3               # remove points below 30cm
    xyz_filtered = xyz[mask]

    # Pass to CUDA for voxelization
    self.process_pointcloud_gpu(xyz_filtered)
```

### IMU 集成

#### 通过 I2C 读取 IMU（MPU6050 / ICM42688）

```bash
# Enable I2C on Jetson GPIO header
sudo i2cdetect -y -r 1    # scan I2C bus 1
# Should show device address (0x68 for MPU6050)
```

```python
import smbus2
import struct
import time

class MPU6050:
    ADDR = 0x68
    PWR_MGMT_1 = 0x6B
    ACCEL_XOUT_H = 0x3B
    GYRO_XOUT_H = 0x43

    def __init__(self, bus_num=1):
        self.bus = smbus2.SMBus(bus_num)
        self.bus.write_byte_data(self.ADDR, self.PWR_MGMT_1, 0)  # wake up
        time.sleep(0.1)

    def _read_word(self, reg):
        high = self.bus.read_byte_data(self.ADDR, reg)
        low  = self.bus.read_byte_data(self.ADDR, reg + 1)
        val  = (high << 8) + low
        return val - 65536 if val >= 0x8000 else val

    def get_accel(self):
        ax = self._read_word(self.ACCEL_XOUT_H) / 16384.0     # g
        ay = self._read_word(self.ACCEL_XOUT_H + 2) / 16384.0
        az = self._read_word(self.ACCEL_XOUT_H + 4) / 16384.0
        return ax, ay, az

    def get_gyro(self):
        gx = self._read_word(self.GYRO_XOUT_H) / 131.0        # °/s
        gy = self._read_word(self.GYRO_XOUT_H + 2) / 131.0
        gz = self._read_word(self.GYRO_XOUT_H + 4) / 131.0
        return gx, gy, gz

# Publish as ROS2 Imu message
from sensor_msgs.msg import Imu

imu = Imu()
imu.header.stamp = self.get_clock().now().to_msg()
imu.header.frame_id = 'imu_link'
ax, ay, az = mpu.get_accel()
imu.linear_acceleration.x = ax * 9.81
imu.linear_acceleration.y = ay * 9.81
imu.linear_acceleration.z = az * 9.81
self.imu_pub.publish(imu)
```

### 外参校准：相机 ↔ 激光雷达

外参校准求解传感器坐标系之间的刚体变换（旋转 + 平移）。这对融合至关重要。

```bash
# Install calibration tools
sudo apt-get install ros-humble-camera-calibration
pip3 install kalibr

# Kalibr calibration (recommended):
# 1. Print an AprilGrid target
# 2. Record a ROS2 bag with camera + IMU moving together
ros2 bag record -o calib_bag /camera/image_raw /imu/data

# 3. Run Kalibr
kalibr_calibrate_cameras \
    --bag calib_bag.bag \
    --topics /camera/image_raw \
    --models pinhole-equi \
    --target aprilgrid.yaml

# Camera-LiDAR extrinsic:
# Use: github.com/PJLab-ADLab/SensorsCalibration
```

```python
# Apply extrinsic transform: project LiDAR points into camera image
import numpy as np

# Extrinsic: 4×4 transform matrix (LiDAR frame → camera frame)
T_cam_lidar = np.array([
    [ 0.9998, -0.0052,  0.0191,  0.1500],   # example values
    [ 0.0049,  0.9999,  0.0104, -0.0050],
    [-0.0192, -0.0103,  0.9997,  0.0300],
    [ 0.0000,  0.0000,  0.0000,  1.0000]
])

# Camera intrinsic matrix
K = np.array([[fx, 0, cx],
              [0, fy, cy],
              [0,  0,  1]])

def lidar_to_image(points_lidar, T_cam_lidar, K):
    """Project LiDAR points onto camera image plane"""
    N = points_lidar.shape[0]
    pts_h = np.hstack([points_lidar[:, :3], np.ones((N, 1))])   # homogeneous
    pts_cam = (T_cam_lidar @ pts_h.T).T                          # camera frame

    # Keep only points in front of camera
    mask = pts_cam[:, 2] > 0
    pts_cam = pts_cam[mask]

    # Project to image
    pts_img = (K @ pts_cam[:, :3].T).T
    u = pts_img[:, 0] / pts_img[:, 2]
    v = pts_img[:, 1] / pts_img[:, 2]
    depth = pts_cam[:, 2]

    return u, v, depth, mask
```

### 传感器间的时间同步

```python
# Use message_filters for approximate time synchronization
import message_filters
from sensor_msgs.msg import Image, PointCloud2

class FusionNode(Node):
    def __init__(self):
        super().__init__('fusion_node')

        self.camera_sub = message_filters.Subscriber(self, Image, '/camera/image_raw')
        self.lidar_sub  = message_filters.Subscriber(self, PointCloud2, '/lidar/points')

        # Synchronize: accept messages within 50ms of each other
        self.ts = message_filters.ApproximateTimeSynchronizer(
            [self.camera_sub, self.lidar_sub],
            queue_size=10,
            slop=0.05    # 50ms tolerance
        )
        self.ts.registerCallback(self.fused_callback)

    def fused_callback(self, camera_msg, lidar_msg):
        # Both messages are time-synchronized
        # Process together for accurate fusion
        pass
```

---

## 11. 设备树配置

> **深入剖析：** 完整的 kernel 内部机制——源码树结构、构建系统、设备树架构、驱动模型、摄像头 kernel 栈、nvgpu 驱动、启动时间优化、自定义模块开发与生产级加固——参见 [**Orin Nano Kernel Internals & Customization**](06-Orin-Nano内核内部机制/Guide.md)。

在 NVIDIA Jetson 平台上，设备树配置对于使能 I2C 总线、配置 40-pin header 上的 GPIO 引脚以及集成摄像头传感器（CSI）至关重要。Jetson 系统使用 **设备树 Overlay**（`.dtbo`）在启动期间修改基础硬件配置，从而无需重新烧录整块板卡即可定制外设。

### 什么是 Pinmux？

**Pinmux**（引脚复用）是把 SoC 上每个物理 I/O 引脚分配到特定功能的过程。Jetson SoC 具有 **Multi-Purpose I/O（MPIO）** 引脚，可配置为以下之一：

- **GPIO** — 用于自定义逻辑、LED、按键等的通用 I/O。
- **SFIO** — 用于专用接口（I2C、SPI、UART、PWM 等）的 Special Function I/O。

40-pin header 上的信号经过多路复用器；设备树（或载板设计用的 Pinmux Excel 表）配置每个引脚承担的功能。对于自定义载板，NVIDIA 提供 Pinmux Excel 模板，可生成 `config-pinmux.dtsi`、`padvoltage-default.dtsi` 和 `gpio-default.dtsi`，用于集成到启动流程中。

**参考：** [Jetson AGX Series Module Pinmux Application Note (DA-12015-001)](https://developer.download.nvidia.com/assets/embedded/secure/jetson/agx_orin/Jetson-AGX-Series-Module-Pinmux-Application-Note_DA-12015-001v1.0.pdf) — NVIDIA 官方 pinmux 流程、Excel 表用法以及强制接口引脚方向配置。*（也可从 [Jetson Download Center](https://developer.nvidia.com/embedded/jetson) 获取——搜索 “Orin Pinmux” 或 “Pinmux Application Note”。）*

### 10.1 Jetson-IO 工具：首选方法

在 Jetson（Orin Nano/NX、Xavier NX）上管理设备树最简单的方式是 **jetson-io** 工具，它为 40-pin header 生成 overlay。

```bash
# Run the tool
sudo /opt/nvidia/jetson-io/jetson-io.py
```

**用途：** 为 I2C、SPI、PWM 和 UART 配置 pinmux，并将其保存为自定义 `.dtbo`。

**方法：** 选择 “Configure Jetson 40pin Header”，使能所需接口（如 I2C1），并保存到新的 overlay。

### 10.2 I2C 与 GPIO 配置

**Pinmux 配置：** 40-pin header 上的信号从 Tegra SoC 经过多路复用器。设备树为这些多路复用器配置特定角色。

**I2C：** 要更改 I2C 速率（如从 400 kHz 改为 100 kHz）或使能某条总线，请修改对应 I2C 控制器的 DTSI 文件中的 `clock-frequency` 和 `status` 属性。

**GPIO：** 引脚可配置为 GPIO 或 SFIO（Special Function I/O）。一个最小 overlay 会定义引脚的 Linux 名称（如引脚 7 的 `PIO09`），并将其功能设为 `gpio`。

**电压：** Jetson GPIO 引脚额定为 **3.3V**；5V 信号可能损坏板卡。

### 10.3 摄像头/传感器集成

摄像头集成需要修改设备树，以定义传感器、I2C 总线、CSI lane 与电源管理。

- **tegra-camera-platform：** 在设备树中定义 `tegra-camera-platform` 节点以描述摄像头模块，包括位置（如 `"rear"`）和朝向。
- **V4L2 传感器驱动：** 新驱动开发使用 V4L2 framework 2.0，确保 `devname` 与 I2C 地址匹配（如 `imx185 30-001a`）。
- **资源：** 在传感器 DTSI 文件中指定 regulator（`vana-supply`、`vdig-supply`）以及用于复位/掉电的 GPIO（如 `H3-gpio`、`H6-gpio`）。


<details>
<summary>English original</summary>

**Time Synchronization Between Sensors**

```python
# Use message_filters for approximate time synchronization
import message_filters
from sensor_msgs.msg import Image, PointCloud2

class FusionNode(Node):
    def __init__(self):
        super().__init__('fusion_node')

        self.camera_sub = message_filters.Subscriber(self, Image, '/camera/image_raw')
        self.lidar_sub  = message_filters.Subscriber(self, PointCloud2, '/lidar/points')

        # Synchronize: accept messages within 50ms of each other
        self.ts = message_filters.ApproximateTimeSynchronizer(
            [self.camera_sub, self.lidar_sub],
            queue_size=10,
            slop=0.05    # 50ms tolerance
        )
        self.ts.registerCallback(self.fused_callback)

    def fused_callback(self, camera_msg, lidar_msg):
        # Both messages are time-synchronized
        # Process together for accurate fusion
        pass
```

---

**11. Device Tree Configuration**

> **Deep dive:** For full kernel internals — source tree structure, build system, device tree architecture, driver model, camera kernel stack, nvgpu driver, boot time optimization, custom module development, and production hardening — see [**Orin Nano Kernel Internals & Customization**](06-Orin-Nano内核内部机制/Guide.md).

Device tree configuration on NVIDIA Jetson platforms is critical for enabling I2C buses, configuring GPIO pins on the 40-pin header, and integrating camera sensors (CSI). Jetson systems utilize **Device Tree Overlays** (`.dtbo`) to modify base hardware configurations during boot, allowing for peripheral customization without re-flashing the entire board.

**What is Pinmux?**

**Pinmux** (pin multiplexing) is the process of assigning each physical I/O pin on the SoC to a specific function. Jetson SoCs have **Multi-Purpose I/O (MPIO)** pins that can be configured as either:

- **GPIO** — General-Purpose I/O for custom logic, LEDs, buttons, etc.
- **SFIO** — Special Function I/O for dedicated interfaces (I2C, SPI, UART, PWM, etc.)

Signals on the 40-pin header pass through multiplexers; the device tree (or Pinmux Excel sheet for carrier-board design) configures which function each pin serves. For custom carrier boards, NVIDIA provides a Pinmux Excel template that generates `config-pinmux.dtsi`, `padvoltage-default.dtsi`, and `gpio-default.dtsi` for integration into the boot process.

**Reference:** [Jetson AGX Series Module Pinmux Application Note (DA-12015-001)](https://developer.download.nvidia.com/assets/embedded/secure/jetson/agx_orin/Jetson-AGX-Series-Module-Pinmux-Application-Note_DA-12015-001v1.0.pdf) — official NVIDIA pinmux process, Excel sheet usage, and mandatory interface pin direction configuration. *(Also available from [Jetson Download Center](https://developer.nvidia.com/embedded/jetson) — search for "Orin Pinmux" or "Pinmux Application Note".)*

**10.1 Jetson-IO Tool: The Preferred Method**

The easiest way to manage device trees on Jetson (Orin Nano/NX, Xavier NX) is the **jetson-io** tool, which generates overlays for the 40-pin header.

```bash
# Run the tool
sudo /opt/nvidia/jetson-io/jetson-io.py
```

**Purpose:** Configure pinmux for I2C, SPI, PWM, and UART, and save them as a custom `.dtbo`.

**Method:** Select "Configure Jetson 40pin Header", enable the desired interface (e.g., I2C1), and save to a new overlay.

**10.2 I2C and GPIO Configuration**

**Pinmuxing:** Signals on the 40-pin header travel from the Tegra SoC through multiplexers. The device tree configures these multiplexers for specific roles.

**I2C:** To change I2C speeds (e.g., 400 kHz to 100 kHz) or enable a bus, modify the `clock-frequency` and `status` properties in the DTSI file corresponding to the I2C controller.

**GPIO:** Pins can be configured as GPIO or SFIO (Special Function I/O). A minimal overlay defines the pin's Linux name (e.g., `PIO09` for pin 7) and sets its function to `gpio`.

**Voltage:** Jetson GPIO pins are **3.3V rated**; 5V signals can damage the board.

**10.3 Camera/Sensor Integration**

Camera integration requires modifying the device tree to define the sensor, I2C bus, CSI lanes, and power management.

- **tegra-camera-platform:** Define the `tegra-camera-platform` node in the device tree to describe the camera module, including position (e.g., `"rear"`) and orientation.
- **V4L2 Sensor Driver:** Use V4L2 framework 2.0 for new driver development, ensuring the `devname` matches the I2C address (e.g., `imx185 30-001a`).
- **Resources:** Specify regulators (`vana-supply`, `vdig-supply`) and GPIOs for reset/power-down (e.g., `H3-gpio`, `H6-gpio`) in the sensor DTSI file.

</details>

### 10.4 创建并应用自定义 overlay

对于 jetson-io 未覆盖的复杂外设，需要手动创建 overlay。

1. **编写 DTS：** 创建 `.dts` 文件，确保 `compatible` 字符串与你的 Jetson 板卡匹配（例如 Orin Nano 对应 `"nvidia,p3509-0000+p3668-0001"`）。
2. **编译：** 使用设备树编译器（`dtc`）：

   ```bash
   dtc -O dtb -o my-overlay.dtbo -@ my-overlay.dts
   ```

3. **应用：** 将 `.dtbo` 文件移动到 `/boot/`，并更新 `/boot/extlinux/extlinux.conf`，通过 `FDT` 或 `OVERLAY_DTB_FILE` 条目加载它。

### 10.5 关键提示

- **验证：** 在运行中的系统上查看 `/proc/device-tree` 处的活动设备树。
- **摄像头节点：** 摄像头传感器属性（clock、I2C 地址、reset GPIO）通常定义在主平台 DTS 所包含的 `<sensor>.dtsi` 文件中。
- **安全：** 始终在 `extlinux.conf` 中保留一份备份配置，以防 overlay 配置错误导致启动循环。

**延伸阅读：** [NVIDIA Jetson Developer Docs](https://docs.nvidia.com/jetson/)（设备树、摄像头），[RidgeRun Developer Wiki](https://developer.ridgerun.com/)（自定义 overlay、extlinux）。

---

## 12. 实践中的电源与热管理

> **深入阅读：** 关于电源与热管理的内部机制 —— PMIC 架构、nvpmodel 深入解析、DVFS 机制、INA3221 功耗测量、热区、风扇控制、电池供电、按工作负载的功耗剖析、外壳热设计以及量产优化 —— 参见 [**Orin Nano Power Optimization & Thermal Design**](09-Orin-Nano电源与散热/Guide.md)。

### 电源模式

Orin Nano 8GB 具备可配置的 TDP 等级：

```bash
# Check current power mode
sudo nvpmodel -q

# Available modes for Orin Nano 8GB
# Mode 0: MAXN — full performance (15W TDP)
# Mode 1: 7W   — balanced (7W TDP)

# Set max performance mode
sudo nvpmodel -m 0

# Set max CPU and GPU clocks (within current power mode)
sudo jetson_clocks

# Verify
cat /sys/kernel/debug/bpmp/debug/clk/cpu0/rate   # CPU frequency
cat /sys/kernel/debug/bpmp/debug/clk/gpu/rate     # GPU frequency
```

### 实时功耗与温度监测

```bash
# tegrastats: comprehensive live view
sudo tegrastats --interval 100

# Example output:
# RAM 3045/7772MB (lfb 2x2MB) SWAP 0/3886MB
# CPU [35%@1510,28%@1510,12%@1510,9%@1510,15%@1510,8%@1510]
# EMC_FREQ 38% GR3D_FREQ 89%
# CPU@42C SOC0@40C SOC1@38C SOC2@38C GPU@44C tj@44C
# VDD_IN 6234mW VDD_CPU_GPU_CV 2901mW VDD_SOC 1158mW

# Parse tegrastats programmatically:
import subprocess
import re

def get_tegrastats():
    result = subprocess.run(['sudo', 'tegrastats', '--once'],
                            capture_output=True, text=True)
    line = result.stdout.strip()

    gpu_temp  = float(re.search(r'GPU@(\d+\.?\d*)C', line).group(1))
    cpu_temp  = float(re.search(r'CPU@(\d+\.?\d*)C', line).group(1))
    power_mw  = float(re.search(r'VDD_IN (\d+)mW', line).group(1))
    gpu_freq  = int(re.search(r'GR3D_FREQ (\d+)%', line).group(1))

    return {
        'gpu_temp_c': gpu_temp,
        'cpu_temp_c': cpu_temp,
        'power_mw': power_mw,
        'gpu_util_pct': gpu_freq
    }
```

### 热降频 —— 预防

热降频会悄无声息地扼杀性能。当温度超过热预算时，CPU/GPU 时钟会在毫无预警的情况下下降。

```bash
# Check throttle temperature thresholds
cat /sys/devices/virtual/thermal/thermal_zone*/trip_point_*_temp

# Monitor for throttle events
dmesg | grep -i throttle
journalctl -f | grep -i thermal

# Practical thresholds for Orin Nano:
# CPU/GPU @ 85°C: begin throttling
# CPU/GPU @ 95°C: emergency shutdown
# Operating target: keep below 75°C
```

### 实用散热方案

```
Dev Kit enclosure: passive heatsink only → OK for 7W mode, marginal at 15W
Production fixes:
  1. Active cooling (5V fan on GPIO header)
  2. Thermal paste quality check (factory paste is mediocre)
  3. Heatsink + copper shim for better contact
  4. Enclosure with forced air flow (cut vents top and bottom)
  5. Avoid direct sunlight on enclosure
  6. Derate: run at 10W instead of 15W for reliable 24/7 operation
```

```python
# Automatic fan control via PWM (GPIO pin 33 = PWM0)
import Jetson.GPIO as GPIO
import time

GPIO.setmode(GPIO.BOARD)
GPIO.setup(33, GPIO.OUT)

fan_pwm = GPIO.PWM(33, 25000)   # 25kHz PWM frequency
fan_pwm.start(0)                 # 0% duty cycle = off

def set_fan_speed(pct):
    """pct: 0-100"""
    fan_pwm.ChangeDutyCycle(pct)

def thermal_control():
    while True:
        stats = get_tegrastats()
        temp = max(stats['gpu_temp_c'], stats['cpu_temp_c'])

        if temp < 50:
            set_fan_speed(0)    # off
        elif temp < 60:
            set_fan_speed(30)   # 30%
        elif temp < 70:
            set_fan_speed(60)   # 60%
        elif temp < 80:
            set_fan_speed(80)   # 80%
        else:
            set_fan_speed(100)  # full blast

        time.sleep(2)
```


<details>
<summary>English original</summary>

**10.4 Creating and Applying Custom Overlays**

For complex peripherals not covered by jetson-io, manual overlay creation is necessary.

1. **Write the DTS:** Create a `.dts` file, ensuring the `compatible` string matches your Jetson board (e.g., `"nvidia,p3509-0000+p3668-0001"` for Orin Nano).
2. **Compile:** Use the Device Tree Compiler (`dtc`):

   ```bash
   dtc -O dtb -o my-overlay.dtbo -@ my-overlay.dts
   ```

3. **Apply:** Move the `.dtbo` file to `/boot/` and update `/boot/extlinux/extlinux.conf` to load it using the `FDT` or `OVERLAY_DTB_FILE` entry.

**10.5 Essential Tips**

- **Verification:** Inspect the active device tree at `/proc/device-tree` on a running system.
- **Camera Nodes:** Camera sensor properties (clock, I2C address, reset GPIOs) are typically defined in a `<sensor>.dtsi` file included in the main platform DTS.
- **Safety:** Always have a backup configuration in `extlinux.conf` to prevent boot loops if an overlay is misconfigured.

**Further reading:** [NVIDIA Jetson Developer Docs](https://docs.nvidia.com/jetson/) (device tree, camera), [RidgeRun Developer Wiki](https://developer.ridgerun.com/) (custom overlays, extlinux).

---

**12. Power and Thermal Management in Practice**

> **Deep dive:** For power and thermal internals — PMIC architecture, nvpmodel deep dive, DVFS mechanics, INA3221 power measurement, thermal zones, fan control, battery operation, per-workload power profiling, enclosure thermal design, and production optimization — see [**Orin Nano Power Optimization & Thermal Design**](09-Orin-Nano电源与散热/Guide.md).

**Power Modes**

The Orin Nano 8GB has configurable TDP levels:

```bash
# Check current power mode
sudo nvpmodel -q

# Available modes for Orin Nano 8GB
# Mode 0: MAXN — full performance (15W TDP)
# Mode 1: 7W   — balanced (7W TDP)

# Set max performance mode
sudo nvpmodel -m 0

# Set max CPU and GPU clocks (within current power mode)
sudo jetson_clocks

# Verify
cat /sys/kernel/debug/bpmp/debug/clk/cpu0/rate   # CPU frequency
cat /sys/kernel/debug/bpmp/debug/clk/gpu/rate     # GPU frequency
```

**Real-Time Power and Temperature Monitoring**

```bash
# tegrastats: comprehensive live view
sudo tegrastats --interval 100

# Example output:
# RAM 3045/7772MB (lfb 2x2MB) SWAP 0/3886MB
# CPU [35%@1510,28%@1510,12%@1510,9%@1510,15%@1510,8%@1510]
# EMC_FREQ 38% GR3D_FREQ 89%
# CPU@42C SOC0@40C SOC1@38C SOC2@38C GPU@44C tj@44C
# VDD_IN 6234mW VDD_CPU_GPU_CV 2901mW VDD_SOC 1158mW

# Parse tegrastats programmatically:
import subprocess
import re

def get_tegrastats():
    result = subprocess.run(['sudo', 'tegrastats', '--once'],
                            capture_output=True, text=True)
    line = result.stdout.strip()

    gpu_temp  = float(re.search(r'GPU@(\d+\.?\d*)C', line).group(1))
    cpu_temp  = float(re.search(r'CPU@(\d+\.?\d*)C', line).group(1))
    power_mw  = float(re.search(r'VDD_IN (\d+)mW', line).group(1))
    gpu_freq  = int(re.search(r'GR3D_FREQ (\d+)%', line).group(1))

    return {
        'gpu_temp_c': gpu_temp,
        'cpu_temp_c': cpu_temp,
        'power_mw': power_mw,
        'gpu_util_pct': gpu_freq
    }
```

**Thermal Throttling — Prevention**

Thermal throttling kills performance silently. The CPU/GPU clocks drop without warning when the temperature exceeds the thermal budget.

```bash
# Check throttle temperature thresholds
cat /sys/devices/virtual/thermal/thermal_zone*/trip_point_*_temp

# Monitor for throttle events
dmesg | grep -i throttle
journalctl -f | grep -i thermal

# Practical thresholds for Orin Nano:
# CPU/GPU @ 85°C: begin throttling
# CPU/GPU @ 95°C: emergency shutdown
# Operating target: keep below 75°C
```

**Practical Cooling Solutions**

```
Dev Kit enclosure: passive heatsink only → OK for 7W mode, marginal at 15W
Production fixes:
  1. Active cooling (5V fan on GPIO header)
  2. Thermal paste quality check (factory paste is mediocre)
  3. Heatsink + copper shim for better contact
  4. Enclosure with forced air flow (cut vents top and bottom)
  5. Avoid direct sunlight on enclosure
  6. Derate: run at 10W instead of 15W for reliable 24/7 operation
```

```python
# Automatic fan control via PWM (GPIO pin 33 = PWM0)
import Jetson.GPIO as GPIO
import time

GPIO.setmode(GPIO.BOARD)
GPIO.setup(33, GPIO.OUT)

fan_pwm = GPIO.PWM(33, 25000)   # 25kHz PWM frequency
fan_pwm.start(0)                 # 0% duty cycle = off

def set_fan_speed(pct):
    """pct: 0-100"""
    fan_pwm.ChangeDutyCycle(pct)

def thermal_control():
    while True:
        stats = get_tegrastats()
        temp = max(stats['gpu_temp_c'], stats['cpu_temp_c'])

        if temp < 50:
            set_fan_speed(0)    # off
        elif temp < 60:
            set_fan_speed(30)   # 30%
        elif temp < 70:
            set_fan_speed(60)   # 60%
        elif temp < 80:
            set_fan_speed(80)   # 80%
        else:
            set_fan_speed(100)  # full blast

        time.sleep(2)
```

</details>

### 电池供电系统的功耗优化

```bash
# Disable unused hardware
sudo systemctl disable bluetooth    # if not needed
sudo systemctl disable cups         # printer service, never needed

# CPU frequency governor
echo powersave | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# or for max performance:
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Disable HDMI output when not needed (saves ~0.5W)
sudo systemctl disable gdm3          # disable GUI
sudo systemctl set-default multi-user.target

# Measure actual power draw per component
sudo cat /sys/bus/i2c/drivers/ina3221/*/iio:device*/in_power*_input
# Shows VDD_IN, VDD_CPU_GPU_CV, VDD_SOC individually
```

### 移动机器人功耗预算（示例：5Ah @ 12V = 60Wh）

```
Component               Typical Draw    Peak
Jetson Orin Nano        8W              15W
Camera (USB)            2W              2.5W
LiDAR (RPLIDAR A2)      1.5W            2W
IMU                     0.1W            0.1W
Ethernet/WiFi           0.5W            1W
Drive motors            5–30W           50W
──────────────────────────────────────────
AI compute (no motors): ~12W typical
Runtime on 5Ah@12V: 60Wh / 12W = 5 hours
```

---

## 13. OTA 更新最佳实践

> **深入阅读：** 关于 OTA 的完整内部机制 —— nv_update_engine、BUP 生成、Tegra bootloader 更新链、QSPI flash 操作、payload 签名、增量更新、SWUpdate/Mender/RAUC 集成、设备群规模部署、回滚机制、掉电安全，以及现场故障案例研究 —— 见 [**Orin Nano OTA 深入阅读**](08-Orin-Nano-OTA深入解析/Guide.md)。
>
> 关于生产级 rootfs 架构、A/B 冗余内部机制、flash XML 布局、安全的现场更新设计与 bootloop 调试，见 [**Orin Nano Rootfs 与 A/B 冗余深入阅读**](12-Orin-Nano-Rootfs与AB冗余/Guide.md)。

### Jetson 上的 OTA 更新策略

#### 策略 1：基于 apt 的更新（简单，用于应用更新）

```bash
# Unattended security updates (OS packages only)
sudo apt-get install unattended-upgrades
sudo dpkg-reconfigure unattended-upgrades

# Only enable security updates, not all upgrades
# Edit /etc/apt/apt.conf.d/50unattended-upgrades:
Unattended-Upgrade::Allowed-Origins {
    "Ubuntu:${distro_codename}-security";
    "NVIDIA:${distro_codename}";   // if NVIDIA updates are needed
};

# Test update process:
sudo unattended-upgrades --dry-run --debug
```

#### 策略 2：NVIDIA Jetson OTA（完整 JetPack 更新）

NVIDIA 为 JetPack 6.x 中的 L4T 更新提供官方 OTA 基础设施：

```bash
# Check available OTA updates
sudo apt-get update
apt list --upgradable 2>/dev/null | grep nvidia-l4t

# Apply L4T OTA update (minor version, e.g., 36.3 → 36.4)
sudo apt-get upgrade nvidia-l4t-core nvidia-l4t-cuda

# Reboot to activate new kernel
sudo reboot
```

#### 策略 3：A/B 分区方案（生产环境 —— 零停机）

Orin 支持双启动分区（A/B slot）。这是可靠 OTA 的行业标准：

```
Slot A (active):  JetPack 6.1  ← currently running
Slot B (standby): JetPack 6.2  ← being written / updated

OTA process:
  1. Download new image
  2. Write to Slot B while Slot A keeps running
  3. Verify Slot B image integrity (SHA256)
  4. Atomically switch boot to Slot B
  5. Reboot into Slot B
  6. If boot fails: automatically fall back to Slot A
  7. If boot succeeds for N minutes: mark Slot B as permanent
```

```bash
# Check current boot slot
sudo nvbootctrl dump-slots-info

# Mark current slot as successful (call from application after health check)
sudo nvbootctrl mark-boot-successful

# Manually switch slot (for testing)
sudo nvbootctrl set-active-boot-slot 1   # switch to slot B
sudo reboot

# Check rollback info
sudo nvbootctrl get-current-slot
sudo nvbootctrl get-active-boot-slot
```

#### 策略 4：使用 Mender.io 或 Balena 的自定义 OTA

用于设备群管理（10 台以上设备）：

```bash
# Install Mender client on Jetson
# Reference: docs.mender.io/get-started/preparation/prepare-a-raspberrypi-device
# (Jetson Orin support available in Mender 3.x+)

# Mender provides:
#  - Web dashboard for fleet management
#  - Staged rollouts (deploy to 10% first, then 100%)
#  - Rollback on failure
#  - Delta updates (only send changed files, saves bandwidth)
#  - Device authentication and authorization
```


<details>
<summary>English original</summary>

**Power Optimization for Battery-Powered Systems**

```bash
# Disable unused hardware
sudo systemctl disable bluetooth    # if not needed
sudo systemctl disable cups         # printer service, never needed

# CPU frequency governor
echo powersave | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# or for max performance:
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Disable HDMI output when not needed (saves ~0.5W)
sudo systemctl disable gdm3          # disable GUI
sudo systemctl set-default multi-user.target

# Measure actual power draw per component
sudo cat /sys/bus/i2c/drivers/ina3221/*/iio:device*/in_power*_input
# Shows VDD_IN, VDD_CPU_GPU_CV, VDD_SOC individually
```

**Power Budget for Mobile Robot (Example: 5Ah @ 12V = 60Wh)**

```
Component               Typical Draw    Peak
Jetson Orin Nano        8W              15W
Camera (USB)            2W              2.5W
LiDAR (RPLIDAR A2)      1.5W            2W
IMU                     0.1W            0.1W
Ethernet/WiFi           0.5W            1W
Drive motors            5–30W           50W
──────────────────────────────────────────
AI compute (no motors): ~12W typical
Runtime on 5Ah@12V: 60Wh / 12W = 5 hours
```

---

**13. OTA Update Best Practices**

> **Deep dive:** For complete OTA internals — nv_update_engine, BUP generation, Tegra bootloader update chain, QSPI flash operations, payload signing, delta updates, SWUpdate/Mender/RAUC integration, fleet-scale deployment, rollback mechanisms, power-fail safety, and field failure case studies — see [**Orin Nano OTA Deep Dive**](08-Orin-Nano-OTA深入解析/Guide.md).
>
> For production-level rootfs architecture, A/B redundancy internals, flash XML layout, safe field update design, and bootloop debugging, see [**Orin Nano Rootfs & A/B Redundancy Deep Dive**](12-Orin-Nano-Rootfs与AB冗余/Guide.md).

**OTA Update Strategies on Jetson**

**Strategy 1: apt-based updates (Simple, for application updates)**

```bash
# Unattended security updates (OS packages only)
sudo apt-get install unattended-upgrades
sudo dpkg-reconfigure unattended-upgrades

# Only enable security updates, not all upgrades
# Edit /etc/apt/apt.conf.d/50unattended-upgrades:
Unattended-Upgrade::Allowed-Origins {
    "Ubuntu:${distro_codename}-security";
    "NVIDIA:${distro_codename}";   // if NVIDIA updates are needed
};

# Test update process:
sudo unattended-upgrades --dry-run --debug
```

**Strategy 2: NVIDIA Jetson OTA (Full JetPack updates)**

NVIDIA provides official OTA infrastructure for L4T updates in JetPack 6.x:

```bash
# Check available OTA updates
sudo apt-get update
apt list --upgradable 2>/dev/null | grep nvidia-l4t

# Apply L4T OTA update (minor version, e.g., 36.3 → 36.4)
sudo apt-get upgrade nvidia-l4t-core nvidia-l4t-cuda

# Reboot to activate new kernel
sudo reboot
```

**Strategy 3: A/B Partition Scheme (Production — Zero Downtime)**

The Orin supports dual-boot partition (A/B slots). This is the industry standard for reliable OTA:

```
Slot A (active):  JetPack 6.1  ← currently running
Slot B (standby): JetPack 6.2  ← being written / updated

OTA process:
  1. Download new image
  2. Write to Slot B while Slot A keeps running
  3. Verify Slot B image integrity (SHA256)
  4. Atomically switch boot to Slot B
  5. Reboot into Slot B
  6. If boot fails: automatically fall back to Slot A
  7. If boot succeeds for N minutes: mark Slot B as permanent
```

```bash
# Check current boot slot
sudo nvbootctrl dump-slots-info

# Mark current slot as successful (call from application after health check)
sudo nvbootctrl mark-boot-successful

# Manually switch slot (for testing)
sudo nvbootctrl set-active-boot-slot 1   # switch to slot B
sudo reboot

# Check rollback info
sudo nvbootctrl get-current-slot
sudo nvbootctrl get-active-boot-slot
```

**Strategy 4: Custom OTA with Mender.io or Balena**

For fleet management (10+ devices):

```bash
# Install Mender client on Jetson
# Reference: docs.mender.io/get-started/preparation/prepare-a-raspberrypi-device
# (Jetson Orin support available in Mender 3.x+)

# Mender provides:
#  - Web dashboard for fleet management
#  - Staged rollouts (deploy to 10% first, then 100%)
#  - Rollback on failure
#  - Delta updates (only send changed files, saves bandwidth)
#  - Device authentication and authorization
```

</details>

### OTA 最佳实践

```
1. ALWAYS use A/B partitioning for OS updates
   Never update the running system in place

2. Verify before applying:
   - Check SHA256/GPG signature of update package
   - Verify the update is from a trusted source
   - Test on a staging device before fleet rollout

3. Staged rollout:
   Fleet of 100 devices → deploy to 5 → monitor 24h → deploy to 100
   Never push to 100% simultaneously

4. Rollback conditions (auto-trigger):
   - Boot fails to complete in X seconds
   - Application fails health check after boot
   - Critical service fails to start

5. Bandwidth management:
   - Use delta updates (only changed files)
   - Schedule updates during off-hours (low activity)
   - Implement rate limiting to not saturate network

6. Application update vs OS update:
   - Application: can update via docker pull / Python package
   - OS/drivers/kernel: requires full OTA with A/B
   - Treat these separately with different cadences
```

### 应用层更新（Docker）

```bash
# Package your AI application in a container
# docker-compose.yml on Jetson:

version: '3'
services:
  ai_pipeline:
    image: your-registry/jetson-ai:latest
    runtime: nvidia
    environment:
      - NVIDIA_VISIBLE_DEVICES=all
    volumes:
      - /dev:/dev
      - ./models:/models
    devices:
      - /dev/video0
    restart: unless-stopped

# Update application only (no reflash needed):
docker compose pull && docker compose up -d

# Rollback application:
docker compose down
docker tag your-registry/jetson-ai:previous your-registry/jetson-ai:latest
docker compose up -d
```

---

## 14. 安全加固

> **深入剖析：**安全架构内部机制 —— T234 Security Engine、安全启动链、OP-TEE 可信应用、基于硬件 AES 的磁盘加密、熔丝烧录、安全存储（RPMB）、固件更新安全、runtime 加固（seccomp/AppArmor）、模型保护、调试锁定以及供应链安全 —— 参见 [**Orin Nano Security Deep Dive**](13-Orin-Nano安全/Guide.md)。

### Jetson 边缘设备威胁模型

```
Attack Vectors:
  Physical access:     device stolen, SD card extracted, JTAG debug
  Network:             SSH brute force, unencrypted MQTT, open ports
  Supply chain:        malicious model weights, tampered update packages
  Side channel:        power analysis, timing attacks on crypto
  Application:         input injection via camera/LiDAR data, model poisoning

Assets to Protect:
  Model weights (IP)
  Sensor data (privacy)
  Control authority (safety-critical)
  Device identity/credentials
```

### 安全启动

```bash
# Jetson Orin supports UEFI Secure Boot
# Configure during flash with SDK Manager:
# "Secure Boot" option → requires signing key pair

# Or manually:
# 1. Generate RSA-2048 key pair (on air-gapped machine, store offline)
openssl genrsa -out secure_boot_key.pem 2048
openssl req -new -x509 -key secure_boot_key.pem -out secure_boot_cert.pem -days 3650

# 2. Enroll in UEFI (via SDK Manager or L4T flash scripts)
# 3. After enrollment: only signed bootloaders will run
# 4. This prevents booting modified L4T from attacker's SD card

# Enable anti-rollback (prevent downgrade attacks):
# Set fuse JTAG_DISABLE + BOOTROM_PRODUCTION_MODE
# WARNING: IRREVERSIBLE — test thoroughly before fusing production units
```

### 磁盘加密

```bash
# Enable LUKS encryption on data partition (not rootfs)
sudo cryptsetup luksFormat /dev/nvme0n1p3          # data partition
sudo cryptsetup luksOpen /dev/nvme0n1p3 data_enc
sudo mkfs.ext4 /dev/mapper/data_enc
sudo mount /dev/mapper/data_enc /data

# Store LUKS key in TPM or secure enclave (not on disk)
# Or derive from device-unique hardware ID:
sudo tpm2_createprimary -C e -c primary.ctx
sudo tpm2_create -C primary.ctx -u key.pub -r key.priv -a "fixedtpm|fixedparent|sensitivedataorigin|userwithauth|decrypt|sign"
```


<details>
<summary>English original</summary>

**OTA Best Practices**

```
1. ALWAYS use A/B partitioning for OS updates
   Never update the running system in place

2. Verify before applying:
   - Check SHA256/GPG signature of update package
   - Verify the update is from a trusted source
   - Test on a staging device before fleet rollout

3. Staged rollout:
   Fleet of 100 devices → deploy to 5 → monitor 24h → deploy to 100
   Never push to 100% simultaneously

4. Rollback conditions (auto-trigger):
   - Boot fails to complete in X seconds
   - Application fails health check after boot
   - Critical service fails to start

5. Bandwidth management:
   - Use delta updates (only changed files)
   - Schedule updates during off-hours (low activity)
   - Implement rate limiting to not saturate network

6. Application update vs OS update:
   - Application: can update via docker pull / Python package
   - OS/drivers/kernel: requires full OTA with A/B
   - Treat these separately with different cadences
```

**Application-Level Update (Docker)**

```bash
# Package your AI application in a container
# docker-compose.yml on Jetson:

version: '3'
services:
  ai_pipeline:
    image: your-registry/jetson-ai:latest
    runtime: nvidia
    environment:
      - NVIDIA_VISIBLE_DEVICES=all
    volumes:
      - /dev:/dev
      - ./models:/models
    devices:
      - /dev/video0
    restart: unless-stopped

# Update application only (no reflash needed):
docker compose pull && docker compose up -d

# Rollback application:
docker compose down
docker tag your-registry/jetson-ai:previous your-registry/jetson-ai:latest
docker compose up -d
```

---

**14. Security Hardening**

> **Deep dive:** For security architecture internals — T234 Security Engine, secure boot chain, OP-TEE trusted applications, disk encryption with hardware AES, fuse programming, secure storage (RPMB), firmware update security, runtime hardening (seccomp/AppArmor), model protection, debug lockdown, and supply chain security — see [**Orin Nano Security Deep Dive**](13-Orin-Nano安全/Guide.md).

**Threat Model for Jetson Edge Devices**

```
Attack Vectors:
  Physical access:     device stolen, SD card extracted, JTAG debug
  Network:             SSH brute force, unencrypted MQTT, open ports
  Supply chain:        malicious model weights, tampered update packages
  Side channel:        power analysis, timing attacks on crypto
  Application:         input injection via camera/LiDAR data, model poisoning

Assets to Protect:
  Model weights (IP)
  Sensor data (privacy)
  Control authority (safety-critical)
  Device identity/credentials
```

**Secure Boot**

```bash
# Jetson Orin supports UEFI Secure Boot
# Configure during flash with SDK Manager:
# "Secure Boot" option → requires signing key pair

# Or manually:
# 1. Generate RSA-2048 key pair (on air-gapped machine, store offline)
openssl genrsa -out secure_boot_key.pem 2048
openssl req -new -x509 -key secure_boot_key.pem -out secure_boot_cert.pem -days 3650

# 2. Enroll in UEFI (via SDK Manager or L4T flash scripts)
# 3. After enrollment: only signed bootloaders will run
# 4. This prevents booting modified L4T from attacker's SD card

# Enable anti-rollback (prevent downgrade attacks):
# Set fuse JTAG_DISABLE + BOOTROM_PRODUCTION_MODE
# WARNING: IRREVERSIBLE — test thoroughly before fusing production units
```

**Disk Encryption**

```bash
# Enable LUKS encryption on data partition (not rootfs)
sudo cryptsetup luksFormat /dev/nvme0n1p3          # data partition
sudo cryptsetup luksOpen /dev/nvme0n1p3 data_enc
sudo mkfs.ext4 /dev/mapper/data_enc
sudo mount /dev/mapper/data_enc /data

# Store LUKS key in TPM or secure enclave (not on disk)
# Or derive from device-unique hardware ID:
sudo tpm2_createprimary -C e -c primary.ctx
sudo tpm2_create -C primary.ctx -u key.pub -r key.priv -a "fixedtpm|fixedparent|sensitivedataorigin|userwithauth|decrypt|sign"
```

</details>

### Network Hardening

```bash
# 1. Firewall (nftables/ufw)
sudo apt-get install ufw
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow from 192.168.1.0/24 to any port 22  # SSH: local network only
sudo ufw allow from 192.168.1.0/24 to any port 7400  # ROS2 DDS: local only
sudo ufw enable

# 2. Disable unnecessary services
sudo systemctl disable avahi-daemon     # mDNS (if not needed)
sudo systemctl disable cups             # printer
sudo systemctl disable ModemManager    # cellular (if no modem)
sudo systemctl list-units --type=service --state=active  # audit all

# 3. SSH hardening (/etc/ssh/sshd_config)
PermitRootLogin no
PasswordAuthentication no          # key-based auth only
PubkeyAuthentication yes
AllowUsers your_user               # whitelist specific user
Port 2222                          # non-standard port (minor obscurity)
MaxAuthTries 3
ClientAliveInterval 300
ClientAliveCountMax 2

# 4. Change default NVIDIA credentials immediately after flash!
# Default: user=nvidia, password=nvidia  ← ALWAYS change this
passwd  # change current user password
sudo passwd -l root  # lock root account
```

### ROS2 Security（DDS Security）

```bash
# ROS2 uses DDS (Data Distribution Service) which by default is unencrypted
# Enable ROS2 Security (SROS2):

# 1. Install security tools
sudo apt-get install ros-humble-sros2

# 2. Create key infrastructure
ros2 security create_keystore ~/ros2_keystore
ros2 security create_key ~/ros2_keystore /trt_detection_node
ros2 security create_key ~/ros2_keystore /control_node

# 3. Set permissions policy (define who can publish/subscribe to what)
# Create a permissions file defining topic access per node

# 4. Launch with security enabled
export ROS_SECURITY_KEYSTORE=~/ros2_keystore
export ROS_SECURITY_ENABLE=true
export ROS_SECURITY_STRATEGY=Enforce

ros2 run your_package trt_detection_node
```

### Model Weight Protection

```python
# Encrypt TensorRT engine weights at rest
from cryptography.fernet import Fernet

# Generate key (store in hardware security module or TPM in production)
key = Fernet.generate_key()

def encrypt_engine(engine_path, encrypted_path, key):
    f = Fernet(key)
    with open(engine_path, 'rb') as fp:
        data = fp.read()
    with open(encrypted_path, 'wb') as fp:
        fp.write(f.encrypt(data))

def load_encrypted_engine(encrypted_path, key):
    f = Fernet(key)
    with open(encrypted_path, 'rb') as fp:
        data = f.decrypt(fp.read())
    runtime = trt.Runtime(trt.Logger(trt.Logger.WARNING))
    return runtime.deserialize_cuda_engine(data)
    # Engine is only in memory, never decrypted to disk
```

### Security Audit Checklist

```
Before deployment, verify:
□ Default passwords changed
□ SSH key-based auth only, no password auth
□ Firewall enabled with minimal open ports
□ Secure boot enabled and verified
□ Anti-rollback fuses set (if production)
□ Data partition encrypted (if sensitive data)
□ All services audited, unused ones disabled
□ Kernel patched to latest L4T version
□ ROS2 DDS security enabled (if network-connected)
□ Model weights encrypted at rest
□ OTA update channel uses TLS + signature verification
□ Physical ports secured (disable USB in production if not needed)
□ Log aggregation set up (centralized syslog for anomaly detection)
```

---

## 15. Projects

### Project 1: Fresh JetPack 6 Install + NVMe Boot
Flash Orin Nano 8GB to JetPack 6.x with SDK Manager, configure NVMe boot, and benchmark I/O speed vs SD card.

### Project 2: YOLO on Jetson with TensorRT
Train YOLOv8 on custom dataset, export to ONNX, convert to TensorRT FP16 engine, achieve >25 FPS on Orin Nano.

### Project 3: Camera + LiDAR Fusion Pipeline
Build a ROS2 node that fuses camera detection boxes with LiDAR point cloud depth, publish 3D bounding boxes.

### Project 4: Power vs Performance Profiling
Using `tegrastats`, plot FPS vs power consumption for: FP32 / FP16 / INT8 / DLA modes. Find the optimal operating point.

### Project 5: Thermal Stress Test + Cooling Comparison
Run inference at 100% load for 30 minutes. Compare: bare heatsink vs active cooling. Measure FPS drop due to throttling.

### Project 6: A/B OTA Update
Implement a simple OTA update service that fetches a new Docker image, tests it in a container, then switches production traffic to it with rollback capability.

### Project 7: Security Hardening
Start from a fresh Jetson install. Apply all items in the Security Audit Checklist. Verify each item. Run `nmap` from another machine to confirm attack surface.


<details>
<summary>English original</summary>

**Network Hardening**

```bash
# 1. Firewall (nftables/ufw)
sudo apt-get install ufw
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow from 192.168.1.0/24 to any port 22  # SSH: local network only
sudo ufw allow from 192.168.1.0/24 to any port 7400  # ROS2 DDS: local only
sudo ufw enable

# 2. Disable unnecessary services
sudo systemctl disable avahi-daemon     # mDNS (if not needed)
sudo systemctl disable cups             # printer
sudo systemctl disable ModemManager    # cellular (if no modem)
sudo systemctl list-units --type=service --state=active  # audit all

# 3. SSH hardening (/etc/ssh/sshd_config)
PermitRootLogin no
PasswordAuthentication no          # key-based auth only
PubkeyAuthentication yes
AllowUsers your_user               # whitelist specific user
Port 2222                          # non-standard port (minor obscurity)
MaxAuthTries 3
ClientAliveInterval 300
ClientAliveCountMax 2

# 4. Change default NVIDIA credentials immediately after flash!
# Default: user=nvidia, password=nvidia  ← ALWAYS change this
passwd  # change current user password
sudo passwd -l root  # lock root account
```

**ROS2 Security (DDS Security)**

```bash
# ROS2 uses DDS (Data Distribution Service) which by default is unencrypted
# Enable ROS2 Security (SROS2):

# 1. Install security tools
sudo apt-get install ros-humble-sros2

# 2. Create key infrastructure
ros2 security create_keystore ~/ros2_keystore
ros2 security create_key ~/ros2_keystore /trt_detection_node
ros2 security create_key ~/ros2_keystore /control_node

# 3. Set permissions policy (define who can publish/subscribe to what)
# Create a permissions file defining topic access per node

# 4. Launch with security enabled
export ROS_SECURITY_KEYSTORE=~/ros2_keystore
export ROS_SECURITY_ENABLE=true
export ROS_SECURITY_STRATEGY=Enforce

ros2 run your_package trt_detection_node
```

**Model Weight Protection**

```python
# Encrypt TensorRT engine weights at rest
from cryptography.fernet import Fernet

# Generate key (store in hardware security module or TPM in production)
key = Fernet.generate_key()

def encrypt_engine(engine_path, encrypted_path, key):
    f = Fernet(key)
    with open(engine_path, 'rb') as fp:
        data = fp.read()
    with open(encrypted_path, 'wb') as fp:
        fp.write(f.encrypt(data))

def load_encrypted_engine(encrypted_path, key):
    f = Fernet(key)
    with open(encrypted_path, 'rb') as fp:
        data = f.decrypt(fp.read())
    runtime = trt.Runtime(trt.Logger(trt.Logger.WARNING))
    return runtime.deserialize_cuda_engine(data)
    # Engine is only in memory, never decrypted to disk
```

**Security Audit Checklist**

```
Before deployment, verify:
□ Default passwords changed
□ SSH key-based auth only, no password auth
□ Firewall enabled with minimal open ports
□ Secure boot enabled and verified
□ Anti-rollback fuses set (if production)
□ Data partition encrypted (if sensitive data)
□ All services audited, unused ones disabled
□ Kernel patched to latest L4T version
□ ROS2 DDS security enabled (if network-connected)
□ Model weights encrypted at rest
□ OTA update channel uses TLS + signature verification
□ Physical ports secured (disable USB in production if not needed)
□ Log aggregation set up (centralized syslog for anomaly detection)
```

---

**15. Projects**

**Project 1: Fresh JetPack 6 Install + NVMe Boot**
Flash Orin Nano 8GB to JetPack 6.x with SDK Manager, configure NVMe boot, and benchmark I/O speed vs SD card.

**Project 2: YOLO on Jetson with TensorRT**
Train YOLOv8 on custom dataset, export to ONNX, convert to TensorRT FP16 engine, achieve >25 FPS on Orin Nano.

**Project 3: Camera + LiDAR Fusion Pipeline**
Build a ROS2 node that fuses camera detection boxes with LiDAR point cloud depth, publish 3D bounding boxes.

**Project 4: Power vs Performance Profiling**
Using `tegrastats`, plot FPS vs power consumption for: FP32 / FP16 / INT8 / DLA modes. Find the optimal operating point.

**Project 5: Thermal Stress Test + Cooling Comparison**
Run inference at 100% load for 30 minutes. Compare: bare heatsink vs active cooling. Measure FPS drop due to throttling.

**Project 6: A/B OTA Update**
Implement a simple OTA update service that fetches a new Docker image, tests it in a container, then switches production traffic to it with rollback capability.

**Project 7: Security Hardening**
Start from a fresh Jetson install. Apply all items in the Security Audit Checklist. Verify each item. Run `nmap` from another machine to confirm attack surface.

</details>

### 项目 8：端到端产品集成（自主设计 capstone）
把 **方向 B** 的模块 **2–7** 串起来：一台 Jetson Orin Nano 级设备（开发套件或自研载板）、**L4T** 镜像、**应用**栈（网络、可选 GUI、推理）、**安全 / OTA**，以及一份适用于试产的**合规 / 制造**检查清单。可选：使用 [OpenClaw](https://github.com/openclaw/openclaw) 或其他编排器处理语音、自动化或浏览器工具——自行界定范围并记录自己的验收测试。

---

<a id="16-jetson-containers--cloud-native-ml-deployment"></a>
## 16. Jetson 容器 — 云原生 ML 部署

> **深入阅读：** 关于容器与集群部署——nvidia-container-runtime、L4T 基础镜像、jetson-containers 项目、交叉编译、容器中的 GPU/摄像头访问、Docker Compose、Jetson 上的 K3s、集群管理（Balena/AWS IoT/Azure IoT Edge）、CI/CD 流水线、监控与容器安全——参见 [**Orin Nano 容器与集群部署**](03-Orin-Nano容器集群/Guide.md)。

[jetson-containers](https://github.com/dusty-nv/jetson-containers) 是一个模块化的容器构建系统，为 NVIDIA Jetson 和 JetPack-L4T 提供最新的 AI/ML 软件包。它让**云原生部署**在边缘设备上成为可能：可复现的环境、锁定版本的依赖，以及经由 `docker pull` 实现的 OTA 友好更新。

### 为什么在 Jetson 上用容器？

| 裸机安装 | 容器化（jetson-containers） |
|--------------------|------------------------------------|
| `pip install` 冲突、venv 损坏 | 每个容器的依赖相互隔离 |
| JetPack 升级破坏 PyTorch | 在镜像 tag 中锁定 L4T/CUDA |
| 手动匹配 CUDA/cuDNN/TensorRT | 为你的 JetPack 提供预构建 wheel |
| 难以跨设备复现 | 同一镜像 → 相同行为 |
| OTA = 全量刷机或有风险的 apt | OTA = `docker compose pull && up -d` |

### 架构总览

```
┌─────────────────────────────────────────────────────────────────────────┐
│  jetson-containers (Python CLI + package definitions)                   │
│  - autotag: finds compatible image for your JetPack (r36.x, cu12.x)     │
│  - build: composes packages (pytorch + transformers + ros)             │
│  - run: docker run with --runtime nvidia, /data mount, devices          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Package Stack (modular)                                                │
│  ML: pytorch, tensorflow, onnxruntime, deepstream, jax                   │
│  LLM: ollama, vllm, sglang, llama.cpp, transformers, exllama            │
│  VLM: llava, vila, nanoowl, nanosam                                     │
│  Robotics: ros:humble-desktop, lerobot, openvla, zed                    │
│  Speech: whisper, whisper_trt, faster-whisper, piper                    │
│  RAG: llama-index, langchain, nanodb, faiss                             │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Base: L4T (Ubuntu 22.04/24.04) + CUDA + cuDNN + TensorRT               │
│  Registry: dustynv/* on Docker Hub, pypi.jetson-ai-lab.io for wheels     │
└─────────────────────────────────────────────────────────────────────────┘
```

### 安装与快速上手

```bash
# On Jetson (after JetPack 6.2 or 7.x installed)
git clone https://github.com/dusty-nv/jetson-containers
bash jetson-containers/install.sh

# Pull and run a compatible PyTorch container (no build needed)
jetson-containers run $(autotag l4t-pytorch)

# Or run a specific pre-built image
sudo docker run --runtime nvidia -it --rm --network=host dustynv/l4t-pytorch:r36.2.0
```

### 关键概念

**`autotag`** — 为你的 JetPack/L4T 版本解析出正确的镜像 tag。若不存在预构建镜像，则可触发一次构建。示例：在 JetPack 6.2 上 `autotag l4t-pytorch` → `dustynv/l4t-pytorch:r36.4.0`。

**`jetson-containers run`** — 封装 `docker run`，附带：
- `--runtime nvidia`（GPU 访问）
- `-v /data:/data`（模型的持久缓存）
- `--network=host`（用于 ROS2、流式传输）
- 针对摄像头、串口的设备直通

**软件包组合** — 将多个包合并到单个镜像中：

```bash
# Build custom image: PyTorch + Transformers + ROS2 Humble
jetson-containers build --name=my_ai_robot pytorch transformers ros:humble-desktop

# Run it
jetson-containers run my_ai_robot
```

### 版本与 CUDA 定制

```bash
# Rebuild for specific CUDA version
CUDA_VERSION=12.6 jetson-containers build transformers

# Ubuntu 24.04 base (JetPack 6/7)
LSB_RELEASE=24.04 jetson-containers build pytorch:2.8

# Request specific PyTorch / cuDNN / TensorRT via env vars (see docs/build.md)
```


<details>
<summary>English original</summary>

**Project 8: End-to-end product integration (self-directed capstone)**
Tie together **Track B** modules **2–7**: a Jetson Orin Nano–class device (dev kit or your custom carrier), **L4T** image, **application** stack (networking, optional GUI, inference), **security / OTA**, and a **compliance / manufacturing** checklist suitable for a pilot build. Optional: use [OpenClaw](https://github.com/openclaw/openclaw) or another orchestrator for voice, automation, or browser tooling — scope and document your own acceptance tests.

---

<a id="16-jetson-containers--cloud-native-ml-deployment"></a>
**16. Jetson Containers — Cloud-Native ML Deployment**

> **Deep dive:** For container and fleet deployment — nvidia-container-runtime, L4T base images, jetson-containers project, cross-compilation, GPU/camera access in containers, Docker Compose, K3s on Jetson, fleet management (Balena/AWS IoT/Azure IoT Edge), CI/CD pipelines, monitoring, and container security — see [**Orin Nano Container & Fleet Deployment**](03-Orin-Nano容器集群/Guide.md).

[jetson-containers](https://github.com/dusty-nv/jetson-containers) is a modular container build system that provides the latest AI/ML packages for NVIDIA Jetson and JetPack-L4T. It enables **cloud-native deployment** on edge devices: reproducible environments, version-pinned dependencies, and OTA-friendly updates via `docker pull`.

**Why Containers on Jetson?**

| Bare-metal install | Containerized (jetson-containers) |
|--------------------|------------------------------------|
| `pip install` conflicts, broken venvs | Isolated per-container dependencies |
| JetPack upgrade breaks PyTorch | Pin L4T/CUDA in image tag |
| Manual CUDA/cuDNN/TensorRT matching | Pre-built wheels for your JetPack |
| Hard to reproduce across devices | Same image → same behavior |
| OTA = full reflash or risky apt | OTA = `docker compose pull && up -d` |

**Architecture Overview**

```
┌─────────────────────────────────────────────────────────────────────────┐
│  jetson-containers (Python CLI + package definitions)                   │
│  - autotag: finds compatible image for your JetPack (r36.x, cu12.x)     │
│  - build: composes packages (pytorch + transformers + ros)             │
│  - run: docker run with --runtime nvidia, /data mount, devices          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Package Stack (modular)                                                │
│  ML: pytorch, tensorflow, onnxruntime, deepstream, jax                   │
│  LLM: ollama, vllm, sglang, llama.cpp, transformers, exllama            │
│  VLM: llava, vila, nanoowl, nanosam                                     │
│  Robotics: ros:humble-desktop, lerobot, openvla, zed                    │
│  Speech: whisper, whisper_trt, faster-whisper, piper                    │
│  RAG: llama-index, langchain, nanodb, faiss                             │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Base: L4T (Ubuntu 22.04/24.04) + CUDA + cuDNN + TensorRT               │
│  Registry: dustynv/* on Docker Hub, pypi.jetson-ai-lab.io for wheels     │
└─────────────────────────────────────────────────────────────────────────┘
```

**Installation and Quick Start**

```bash
# On Jetson (after JetPack 6.2 or 7.x installed)
git clone https://github.com/dusty-nv/jetson-containers
bash jetson-containers/install.sh

# Pull and run a compatible PyTorch container (no build needed)
jetson-containers run $(autotag l4t-pytorch)

# Or run a specific pre-built image
sudo docker run --runtime nvidia -it --rm --network=host dustynv/l4t-pytorch:r36.2.0
```

**Key Concepts**

**`autotag`** — Resolves the correct image tag for your JetPack/L4T version. If no pre-built image exists, it can trigger a build. Example: `autotag l4t-pytorch` → `dustynv/l4t-pytorch:r36.4.0` on JetPack 6.2.

**`jetson-containers run`** — Wraps `docker run` with:
- `--runtime nvidia` (GPU access)
- `-v /data:/data` (persistent cache for models)
- `--network=host` (for ROS2, streaming)
- Device passthrough for cameras, serial ports

**Package composition** — Combine packages into a single image:

```bash
# Build custom image: PyTorch + Transformers + ROS2 Humble
jetson-containers build --name=my_ai_robot pytorch transformers ros:humble-desktop

# Run it
jetson-containers run my_ai_robot
```

**Version and CUDA Customization**

```bash
# Rebuild for specific CUDA version
CUDA_VERSION=12.6 jetson-containers build transformers

# Ubuntu 24.04 base (JetPack 6/7)
LSB_RELEASE=24.04 jetson-containers build pytorch:2.8

# Request specific PyTorch / cuDNN / TensorRT via env vars (see docs/build.md)
```

</details>

### 支持的 JetPack 版本

- **JetPack 6.2**（CUDA 12.6，L4T r36.5）——主要目标
- **JetPack 7**（CUDA 13.x）——支持
- **Ubuntu 24.04**——可用于更新的软件栈

### 与 OTA 集成（第 12 节）

jetson-containers 可直接嵌入基于 Docker 的 OTA 流程：

```bash
# Your docker-compose.yml can use dustynv images
services:
  inference:
    image: dustynv/l4t-pytorch:r36.4.0
    # or your custom-built image from a private registry
    runtime: nvidia
    volumes:
      - /data:/data
      - ./models:/models

# OTA update: pull new image, restart
docker compose pull && docker compose up -d
```

### 实际用例

| 用例 | 软件包 | 命令 |
|----------|------------|---------|
| PyTorch 推理 | `l4t-pytorch` | `jetson-containers run $(autotag l4t-pytorch)` |
| 本地 LLM（Ollama） | `ollama` | `jetson-containers run $(autotag ollama)` |
| ROS2 + PyTorch | `pytorch`、`ros:humble-desktop` | 构建组合镜像 |
| Whisper 语音转文本 | `whisper` 或 `whisper_trt` | `jetson-containers run $(autotag whisper)` |
| 目标检测（ViT） | `nanoowl` | `jetson-containers run $(autotag nanoowl)` |
| 结合向量数据库的 RAG（检索增强生成） | `nanodb`、`llama-index` | 构建或运行 `nanodb` |

### 文档与社区

- **软件包列表**：[github.com/dusty-nv/jetson-containers/packages](https://github.com/dusty-nv/jetson-containers/tree/master/packages)
- **系统设置**：Docker daemon 配置、内存/存储调优
- **Jetson AI Lab**：[jetson-ai-lab.com](https://www.jetson-ai-lab.com)——教程、SLM/VLM 演示

---

## 17. 资源

### 官方文档
- **Jetson Linux Developer Guide**（L4T r36.4）：[docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/index.html)——主要参考：引导、kernel、设备树、pinmux、摄像头、刷写、OTA、安全
- **JetPack SDK**：developer.nvidia.com/embedded/jetpack
- **NVIDIA Device Tree / Camera**：docs.nvidia.com/jetson——摄像头 sensor DTSI、tegra-camera-platform
- **NVIDIA SDK Manager**：developer.nvidia.com/sdk-manager
- **L4T Developer Guide**（存档）：docs.nvidia.com/jetson/archives/
- **TensorRT Developer Guide**：docs.nvidia.com/deeplearning/tensorrt/developer-guide/
- **VPI Documentation**：docs.nvidia.com/vpi/

### 社区与工具
- **JetsonHacks**（https://jetsonhacks.com/）：实用的 Jetson 教程、GPIO 引脚定义（Orin Nano、AGX Orin、Xavier、Nano）、JetPack 更新、硬件设置——精通 Jetson 的必备参考
- **NVIDIA NGC**：ngc.nvidia.com——针对 Jetson 优化的预构建容器
- **jetson-containers**（https://github.com/dusty-nv/jetson-containers）：深入讲解见 [第 16 节](#16-jetson-containers--cloud-native-ml-deployment)。快速开始：`jetson-containers run $(autotag l4t-pytorch)`。
- **DeepStream Getting Started**：docs.nvidia.com/metropolis/deepstream/

### 性能与性能剖析
- `tegrastats`——开发期间始终在侧边终端中运行
- Nsight Systems——系统级 GPU+CPU trace
- `trtexec --verbose`——TensorRT engine 构建 + benchmark

### 安全
- **NVIDIA Jetson Security Guide**：docs.nvidia.com/jetson/archives/l4t-archived/（Jetson Security 章节）
- **SROS2 Wiki**：wiki.ros.org/sros2

---

*下一篇：[2. 定制载板](../02-定制载板设计与启动/Guide.md) → [3. L4T 定制](../03-L4T定制/Guide.md) → [4. FSP 定制](../04-FSP固件支持包定制/Guide.md) → [5. 应用开发](../05-应用开发/Guide.md) → [6. 安全与 OTA](../06-安全与OTA/Guide.md) → [7. 合规与制造](../07-合规与制造/Guide.md)*


<details>
<summary>English original</summary>

**Supported JetPack Versions**

- **JetPack 6.2** (CUDA 12.6, L4T r36.5) — primary target
- **JetPack 7** (CUDA 13.x) — supported
- **Ubuntu 24.04** — available for newer stacks

**Integration with OTA (Section 12)**

jetson-containers fits directly into the Docker-based OTA flow:

```bash
# Your docker-compose.yml can use dustynv images
services:
  inference:
    image: dustynv/l4t-pytorch:r36.4.0
    # or your custom-built image from a private registry
    runtime: nvidia
    volumes:
      - /data:/data
      - ./models:/models

# OTA update: pull new image, restart
docker compose pull && docker compose up -d
```

**Practical Use Cases**

| Use case | Package(s) | Command |
|----------|------------|---------|
| PyTorch inference | `l4t-pytorch` | `jetson-containers run $(autotag l4t-pytorch)` |
| Local LLM (Ollama) | `ollama` | `jetson-containers run $(autotag ollama)` |
| ROS2 + PyTorch | `pytorch`, `ros:humble-desktop` | Build combined image |
| Whisper speech-to-text | `whisper` or `whisper_trt` | `jetson-containers run $(autotag whisper)` |
| Object detection (ViT) | `nanoowl` | `jetson-containers run $(autotag nanoowl)` |
| RAG with vector DB | `nanodb`, `llama-index` | Build or run `nanodb` |

**Documentation and Community**

- **Package list**: [github.com/dusty-nv/jetson-containers/packages](https://github.com/dusty-nv/jetson-containers/tree/master/packages)
- **System setup**: Docker daemon config, memory/storage tuning
- **Jetson AI Lab**: [jetson-ai-lab.com](https://www.jetson-ai-lab.com) — tutorials, SLM/VLM demos

---

**17. Resources**

**Official Documentation**
- **Jetson Linux Developer Guide** (L4T r36.4): [docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/index.html) — primary reference: boot, kernel, device tree, pinmux, camera, flashing, OTA, security
- **JetPack SDK**: developer.nvidia.com/embedded/jetpack
- **NVIDIA Device Tree / Camera**: docs.nvidia.com/jetson — camera sensor DTSI, tegra-camera-platform
- **NVIDIA SDK Manager**: developer.nvidia.com/sdk-manager
- **L4T Developer Guide** (archives): docs.nvidia.com/jetson/archives/
- **TensorRT Developer Guide**: docs.nvidia.com/deeplearning/tensorrt/developer-guide/
- **VPI Documentation**: docs.nvidia.com/vpi/

**Community and Tools**
- **JetsonHacks** (https://jetsonhacks.com/): Practical Jetson tutorials, GPIO pinouts (Orin Nano, AGX Orin, Xavier, Nano), JetPack updates, hardware setup — essential reference for Jetson mastering
- **NVIDIA NGC**: ngc.nvidia.com — pre-built containers optimized for Jetson
- **jetson-containers** (https://github.com/dusty-nv/jetson-containers): See [Section 16](#16-jetson-containers--cloud-native-ml-deployment) for deep dive. Quick start: `jetson-containers run $(autotag l4t-pytorch)`.
- **DeepStream Getting Started**: docs.nvidia.com/metropolis/deepstream/

**Performance and Profiling**
- `tegrastats` — always running in a side terminal during development
- Nsight Systems — system-level GPU+CPU trace
- `trtexec --verbose` — TensorRT engine build + benchmark

**Security**
- **NVIDIA Jetson Security Guide**: docs.nvidia.com/jetson/archives/l4t-archived/ (Jetson Security section)
- **SROS2 Wiki**: wiki.ros.org/sros2

---

*Next: [2. Custom Carrier Board](../02-定制载板设计与启动/Guide.md) → [3. L4T Customization](../03-L4T定制/Guide.md) → [4. FSP Customization](../04-FSP固件支持包定制/Guide.md) → [5. Application Development](../05-应用开发/Guide.md) → [6. Security and OTA](../06-安全与OTA/Guide.md) → [7. Compliance and Manufacturing](../07-合规与制造/Guide.md)*

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/1. Nvidia Jetson Platform/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/1.%20Nvidia%20Jetson%20Platform/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
