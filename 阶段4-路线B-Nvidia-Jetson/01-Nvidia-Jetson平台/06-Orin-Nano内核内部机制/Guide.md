---
title: Orin Nano 8GB — Kernel Internals & Customization
description: Orin Nano 8GB — Kernel Internals & Customization
---

# Orin Nano 8GB — Kernel Internals & Customization

<div class="course-identity auto-course" style="--course-accent: #ea580c; --course-accent-rgb: 234, 88, 12;" markdown="1">
<div class="course-identity__icon">ON8K</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · Jetson 学习路径</p>
<p class="course-identity__title">针对 Orin Nano 8GB — Kernel Internals & Customization 的专项课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


> **范围：** 在 Orin Nano 8GB 上达到生产级的 Jetson Linux（L4T）kernel 理解 —— 从源码树结构与构建系统，到设备树架构、驱动模型、kernel 配置、启动时间优化、自定义模块开发，以及生产环境 kernel 加固。
>
> **前置要求：** 熟悉 [Orin Nano 启动链](../Guide.md)、[内存架构](../07-Orin-Nano内存架构/Guide.md)，以及 Linux kernel 基本概念（进程、系统调用、模块）。

---


## 1. Jetson kernel 概览

Jetson Linux（L4T）kernel **不是主线 Linux**。它：

* 基于 Linux 5.10（JetPack 5.x）或 Linux 5.15（JetPack 6.x）
* 由 NVIDIA 打补丁，加入 Tegra 专用驱动、设备树绑定和子系统改动
* 包含树外 NVIDIA 模块（nvgpu、camera 栈、多媒体）
* 编译目标为 `aarch64`（ARM 64 位）

### NVIDIA 相对主线做了哪些改动

| 领域              | 主线 Linux                    | Jetson L4T kernel                          |
|-------------------|-----------------------------------|--------------------------------------------|
| GPU 驱动        | nouveau（开源，功能有限）    | nvgpu（NVIDIA 专有，功能完整）  |
| 摄像头            | 标准 V4L2                     | V4L2 + NVIDIA VI/ISP/CSI 扩展        |
| 电源管理  | 通用 DVFS                      | 基于 BPMP，集成 nvpmodel           |
| 显示           | DRM/KMS                           | DRM/KMS + NVIDIA 显示控制器        |
| 设备树       | 主线 ARM DT 绑定          | NVIDIA 专用绑定 + overlay        |
| 热管理           | 通用热管理框架         | Tegra 专用热区 + 调控器   |

在 Jetson 上**无法**使用主线 kernel —— NVIDIA 驱动和固件要求使用 L4T kernel。

---

## 2. kernel 源码树结构

下载 L4T kernel 源码之后：

```
Linux_for_Tegra/source/
├── kernel/
│   └── kernel-5.10/              ← Main kernel source
│       ├── arch/arm64/           ← ARM64 architecture code
│       │   ├── boot/dts/         ← Mainline device trees
│       │   └── configs/          ← defconfig files
│       ├── drivers/              ← All kernel drivers
│       │   ├── gpu/nvgpu/        ← NVIDIA GPU driver
│       │   ├── media/            ← V4L2 + NVIDIA camera
│       │   ├── platform/tegra/   ← Tegra platform drivers
│       │   └── ...
│       ├── include/              ← Kernel headers
│       ├── net/                  ← Networking stack
│       └── Makefile
├── hardware/
│   └── nvidia/
│       ├── platform/t23x/        ← T234 platform device trees
│       │   └── concord/          ← Orin module DTS files
│       └── soc/t23x/             ← T234 SoC-level DTS
└── nvidia-oot/                   ← Out-of-tree NVIDIA modules
    ├── drivers/
    │   ├── video/tegra/host/     ← nvhost (multimedia)
    │   ├── gpu/nvgpu/            ← nvgpu module
    │   └── media/platform/tegra/ ← Camera drivers
    └── Makefile
```

### 关键目录

| 路径                              | 内容                                    |
|-----------------------------------|--------------------------------------------|
| `kernel/kernel-5.10/`             | 基础 Linux kernel                          |
| `hardware/nvidia/platform/t23x/`  | T234 设备树源文件              |
| `hardware/nvidia/soc/t23x/`       | SoC 级设备树 include             |
| `nvidia-oot/`                      | NVIDIA 树外模块（nvgpu、camera） |

---

## 3. 从源码构建 kernel

### 前置要求

```bash
# Install cross-compilation toolchain
sudo apt install gcc-aarch64-linux-gnu build-essential bc flex bison libssl-dev

# Set environment variables
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64
export LOCALVERSION=-tegra
```

### 构建步骤

```bash
cd Linux_for_Tegra/source/kernel/kernel-5.10/

# Step 1: Configure kernel (use Jetson defconfig)
make tegra_defconfig

# Step 2: (Optional) Customize configuration
make menuconfig

# Step 3: Build kernel Image
make -j$(nproc) Image

# Step 4: Build device tree blobs
make -j$(nproc) dtbs

# Step 5: Build modules
make -j$(nproc) modules

# Step 6: Install modules to staging directory
make modules_install INSTALL_MOD_PATH=<staging_dir>
```


<details>
<summary>English original</summary>

**Orin Nano 8GB — Kernel Internals & Customization**

<div class="course-identity auto-course" style="--course-accent: #ea580c; --course-accent-rgb: 234, 88, 12;" markdown="1">
<div class="course-identity__icon">ON8K</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Orin Nano 8GB — Kernel Internals & Customization.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Scope:** Production-level understanding of the Jetson Linux (L4T) kernel on Orin Nano 8GB — from source tree structure and build system through device tree architecture, driver model, kernel configuration, boot time optimization, custom module development, and production kernel hardening.
>
> **Prerequisites:** Familiarity with the [Orin Nano boot chain](../Guide.md), [memory architecture](../07-Orin-Nano内存架构/Guide.md), and basic Linux kernel concepts (processes, syscalls, modules).

---


**1. Jetson Kernel Overview**

The Jetson Linux (L4T) kernel is **not mainline Linux**. It is:

* Based on Linux 5.10 (JetPack 5.x) or Linux 5.15 (JetPack 6.x)
* Patched by NVIDIA with Tegra-specific drivers, device tree bindings, and subsystem modifications
* Includes out-of-tree NVIDIA modules (nvgpu, camera stack, multimedia)
* Compiled for `aarch64` (ARM 64-bit)

**What NVIDIA Changes From Mainline**

| Area              | Mainline Linux                    | Jetson L4T Kernel                          |
|-------------------|-----------------------------------|--------------------------------------------|
| GPU driver        | nouveau (open-source, limited)    | nvgpu (NVIDIA proprietary, full features)  |
| Camera            | Standard V4L2                     | V4L2 + NVIDIA VI/ISP/CSI extensions        |
| Power management  | Generic DVFS                      | BPMP-based, nvpmodel integration           |
| Display           | DRM/KMS                           | DRM/KMS + NVIDIA display controller        |
| Device tree       | Mainline ARM DT bindings          | NVIDIA-specific bindings + overlays        |
| Thermal           | Generic thermal framework         | Tegra-specific thermal zones + governors   |

You **cannot** use a mainline kernel on Jetson — NVIDIA drivers and firmware require the L4T kernel.

---

**2. Kernel Source Tree Structure**

After downloading the L4T kernel sources:

```
Linux_for_Tegra/source/
├── kernel/
│   └── kernel-5.10/              ← Main kernel source
│       ├── arch/arm64/           ← ARM64 architecture code
│       │   ├── boot/dts/         ← Mainline device trees
│       │   └── configs/          ← defconfig files
│       ├── drivers/              ← All kernel drivers
│       │   ├── gpu/nvgpu/        ← NVIDIA GPU driver
│       │   ├── media/            ← V4L2 + NVIDIA camera
│       │   ├── platform/tegra/   ← Tegra platform drivers
│       │   └── ...
│       ├── include/              ← Kernel headers
│       ├── net/                  ← Networking stack
│       └── Makefile
├── hardware/
│   └── nvidia/
│       ├── platform/t23x/        ← T234 platform device trees
│       │   └── concord/          ← Orin module DTS files
│       └── soc/t23x/             ← T234 SoC-level DTS
└── nvidia-oot/                   ← Out-of-tree NVIDIA modules
    ├── drivers/
    │   ├── video/tegra/host/     ← nvhost (multimedia)
    │   ├── gpu/nvgpu/            ← nvgpu module
    │   └── media/platform/tegra/ ← Camera drivers
    └── Makefile
```

**Key Directories**

| Path                              | Content                                    |
|-----------------------------------|--------------------------------------------|
| `kernel/kernel-5.10/`             | Base Linux kernel                          |
| `hardware/nvidia/platform/t23x/`  | T234 device tree source files              |
| `hardware/nvidia/soc/t23x/`       | SoC-level device tree includes             |
| `nvidia-oot/`                      | NVIDIA out-of-tree modules (nvgpu, camera) |

---

**3. Building the Kernel From Source**

**Prerequisites**

```bash
# Install cross-compilation toolchain
sudo apt install gcc-aarch64-linux-gnu build-essential bc flex bison libssl-dev

# Set environment variables
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64
export LOCALVERSION=-tegra
```

**Build Steps**

```bash
cd Linux_for_Tegra/source/kernel/kernel-5.10/

# Step 1: Configure kernel (use Jetson defconfig)
make tegra_defconfig

# Step 2: (Optional) Customize configuration
make menuconfig

# Step 3: Build kernel Image
make -j$(nproc) Image

# Step 4: Build device tree blobs
make -j$(nproc) dtbs

# Step 5: Build modules
make -j$(nproc) modules

# Step 6: Install modules to staging directory
make modules_install INSTALL_MOD_PATH=<staging_dir>
```

</details>

### 构建 NVIDIA 树外模块

```bash
cd Linux_for_Tegra/source/nvidia-oot/

# Build against the kernel you just compiled
make -j$(nproc) \
    KERNEL_SRC=../kernel/kernel-5.10 \
    M=$(pwd) \
    modules
```

### 部署到 Jetson

```bash
# Copy kernel Image
cp arch/arm64/boot/Image <Linux_for_Tegra>/kernel/Image

# Copy DTB
cp arch/arm64/boot/dts/nvidia/*.dtb <Linux_for_Tegra>/kernel/dtb/

# Copy modules
cp -r <staging_dir>/lib/modules/* <Linux_for_Tegra>/rootfs/lib/modules/

# Flash (or copy to device via SCP for development)
sudo ./flash.sh jetson-orin-nano-devkit internal
```

迭代开发时，用 SCP 直接把 Image 和模块复制到 Jetson，而不重新刷机：

```bash
scp arch/arm64/boot/Image jetson:/boot/Image
scp -r <staging_dir>/lib/modules/<version> jetson:/lib/modules/
ssh jetson "sudo reboot"
```

---

## 4. Kernel 配置（defconfig）

### 默认配置

Jetson 默认配置启用一切 —— 所有支持的硬件、所有文件系统、所有调试功能。这带来最大兼容性，但会增加启动时间和 kernel 体积。

```bash
# View current config on running Jetson
zcat /proc/config.gz | grep CONFIG_NVGPU
# CONFIG_NVGPU=m

# Or check the defconfig file
cat arch/arm64/configs/tegra_defconfig
```

### 配置类别

| 类别           | 默认状态    | 生产环境建议              |
|--------------------|------------------|----------------------------------------|
| 所有 NVIDIA 驱动 | 启用          | 保持启用（GPU/摄像头必需） |
| 文件系统        | 多为内建    | 未使用的改为模块（NTFS、FUSE、VFAT）  |
| 音频编解码器       | 启用          | 无需音频时禁用             |
| USB gadget         | 启用          | 未使用时禁用                    |
| 调试/trace        | 启用          | 禁用（FTRACE、KMEMLEAK 等）       |
| 网络协议  | 大量启用     | 只保留需要的（TCP/IP，用到则保留 CAN） |
| HID 驱动        | 启用          | 改为模块（启动时不需要）        |

### 创建自定义 defconfig

```bash
# Start from Jetson default
make tegra_defconfig

# Customize
make menuconfig

# Save as custom defconfig
make savedefconfig
cp defconfig arch/arm64/configs/my_product_defconfig

# Future builds use your config
make my_product_defconfig
```

### AI 边缘系统的关键配置

```
# Must be enabled
CONFIG_NVGPU=m                    # GPU driver
CONFIG_VIDEO_TEGRA=m              # Camera pipeline
CONFIG_TEGRA_BPMP=y               # Power management
CONFIG_ARM_SMMU=y                 # IOMMU (required for DMA)
CONFIG_CMA=y                      # Contiguous memory allocation
CONFIG_DMA_CMA=y                  # CMA for DMA
CONFIG_CMA_SIZE_MBYTES=768        # CMA size (adjust per workload)
CONFIG_IOMMU_SUPPORT=y            # SMMU support
CONFIG_VFIO=n                     # Usually not needed on edge

# Should be modularized for boot speed
CONFIG_FUSE_FS=m
CONFIG_VFAT_FS=m
CONFIG_NTFS_FS=m
CONFIG_USB_HID=m
CONFIG_SND_SOC_TEGRA_ALT=n        # Disable if no audio

# Should be disabled in production
# CONFIG_FTRACE is not set
# CONFIG_KMEMLEAK is not set
# CONFIG_DEBUG_INFO is not set
# CONFIG_DYNAMIC_DEBUG is not set
```

---

## 5. T234 上的设备树架构

设备树（DT）向 kernel 描述硬件。在 Jetson 上，它是一个多层系统。

### DTS 文件层级

```
tegra234-p3767-0000-p3768-0000-a0.dts     ← Top-level (what gets compiled)
 └── includes:
     ├── tegra234-p3767-0000.dtsi          ← Orin Nano module
     │   └── tegra234-soc.dtsi             ← T234 SoC peripherals
     │       └── tegra234-soc-base.dtsi    ← Base SoC definitions
     ├── tegra234-p3768-0000.dtsi          ← Dev Kit carrier board
     └── tegra234-power-tree.dtsi          ← Power domains
```

### 关键设备树目录

```
hardware/nvidia/platform/t23x/concord/     ← Orin module + carrier board DTS
hardware/nvidia/soc/t23x/                   ← SoC-level device tree includes
```

被烧录的 DTB：

```
hardware/nvidia/platform/t23x/concord/kernel-dts/tegra234-p3767-0000-p3768-0000-a0.dts
```

### 设备树节点结构

SoC 上每个硬件块都有一个设备树节点：

```dts
/* Example: VI (Video Input) controller */
vi@15c10000 {
    compatible = "nvidia,tegra234-vi";
    reg = <0x0 0x15c10000 0x0 0x10000>;     /* MMIO registers */
    interrupts = <GIC_SPI 200 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&bpmp_clks TEGRA234_CLK_VI>;
    clock-names = "vi";
    resets = <&bpmp_resets TEGRA234_RESET_VI>;
    reset-names = "vi";
    power-domains = <&bpmp TEGRA234_POWER_DOMAIN_VE>;
    iommus = <&smmu TEGRA234_SID_VI>;       /* SMMU stream ID */
    status = "okay";                         /* "okay" = enabled */
};
```


<details>
<summary>English original</summary>

**Build NVIDIA Out-of-Tree Modules**

```bash
cd Linux_for_Tegra/source/nvidia-oot/

# Build against the kernel you just compiled
make -j$(nproc) \
    KERNEL_SRC=../kernel/kernel-5.10 \
    M=$(pwd) \
    modules
```

**Deploy to Jetson**

```bash
# Copy kernel Image
cp arch/arm64/boot/Image <Linux_for_Tegra>/kernel/Image

# Copy DTB
cp arch/arm64/boot/dts/nvidia/*.dtb <Linux_for_Tegra>/kernel/dtb/

# Copy modules
cp -r <staging_dir>/lib/modules/* <Linux_for_Tegra>/rootfs/lib/modules/

# Flash (or copy to device via SCP for development)
sudo ./flash.sh jetson-orin-nano-devkit internal
```

For iterative development, copy Image and modules directly to the Jetson via SCP instead of reflashing:

```bash
scp arch/arm64/boot/Image jetson:/boot/Image
scp -r <staging_dir>/lib/modules/<version> jetson:/lib/modules/
ssh jetson "sudo reboot"
```

---

**4. Kernel Configuration (defconfig)**

**Default Configuration**

The Jetson default config enables everything — all supported hardware, all filesystems, all debugging. This provides maximum compatibility but increases boot time and kernel size.

```bash
# View current config on running Jetson
zcat /proc/config.gz | grep CONFIG_NVGPU
# CONFIG_NVGPU=m

# Or check the defconfig file
cat arch/arm64/configs/tegra_defconfig
```

**Configuration Categories**

| Category           | Default State    | Production Recommendation              |
|--------------------|------------------|----------------------------------------|
| All NVIDIA drivers | Enabled          | Keep enabled (required for GPU/camera) |
| Filesystems        | Many built-in    | Modularize unused (NTFS, FUSE, VFAT)  |
| Audio codecs       | Enabled          | Disable if no audio needed             |
| USB gadget         | Enabled          | Disable if not used                    |
| Debug/trace        | Enabled          | Disable (FTRACE, KMEMLEAK, etc.)       |
| Network protocols  | Many enabled     | Keep only needed (TCP/IP, CAN if used) |
| HID drivers        | Enabled          | Modularize (not needed at boot)        |

**Creating a Custom defconfig**

```bash
# Start from Jetson default
make tegra_defconfig

# Customize
make menuconfig

# Save as custom defconfig
make savedefconfig
cp defconfig arch/arm64/configs/my_product_defconfig

# Future builds use your config
make my_product_defconfig
```

**Critical Configs for AI Edge Systems**

```
# Must be enabled
CONFIG_NVGPU=m                    # GPU driver
CONFIG_VIDEO_TEGRA=m              # Camera pipeline
CONFIG_TEGRA_BPMP=y               # Power management
CONFIG_ARM_SMMU=y                 # IOMMU (required for DMA)
CONFIG_CMA=y                      # Contiguous memory allocation
CONFIG_DMA_CMA=y                  # CMA for DMA
CONFIG_CMA_SIZE_MBYTES=768        # CMA size (adjust per workload)
CONFIG_IOMMU_SUPPORT=y            # SMMU support
CONFIG_VFIO=n                     # Usually not needed on edge

# Should be modularized for boot speed
CONFIG_FUSE_FS=m
CONFIG_VFAT_FS=m
CONFIG_NTFS_FS=m
CONFIG_USB_HID=m
CONFIG_SND_SOC_TEGRA_ALT=n        # Disable if no audio

# Should be disabled in production
# CONFIG_FTRACE is not set
# CONFIG_KMEMLEAK is not set
# CONFIG_DEBUG_INFO is not set
# CONFIG_DYNAMIC_DEBUG is not set
```

---

**5. Device Tree Architecture on T234**

The device tree (DT) describes the hardware to the kernel. On Jetson, it is a multi-layered system.

**DTS File Hierarchy**

```
tegra234-p3767-0000-p3768-0000-a0.dts     ← Top-level (what gets compiled)
 └── includes:
     ├── tegra234-p3767-0000.dtsi          ← Orin Nano module
     │   └── tegra234-soc.dtsi             ← T234 SoC peripherals
     │       └── tegra234-soc-base.dtsi    ← Base SoC definitions
     ├── tegra234-p3768-0000.dtsi          ← Dev Kit carrier board
     └── tegra234-power-tree.dtsi          ← Power domains
```

**Key Device Tree Directories**

```
hardware/nvidia/platform/t23x/concord/     ← Orin module + carrier board DTS
hardware/nvidia/soc/t23x/                   ← SoC-level device tree includes
```

The DTB that gets flashed:

```
hardware/nvidia/platform/t23x/concord/kernel-dts/tegra234-p3767-0000-p3768-0000-a0.dts
```

**Device Tree Node Anatomy**

Every hardware block on the SoC has a device tree node:

```dts
/* Example: VI (Video Input) controller */
vi@15c10000 {
    compatible = "nvidia,tegra234-vi";
    reg = <0x0 0x15c10000 0x0 0x10000>;     /* MMIO registers */
    interrupts = <GIC_SPI 200 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&bpmp_clks TEGRA234_CLK_VI>;
    clock-names = "vi";
    resets = <&bpmp_resets TEGRA234_RESET_VI>;
    reset-names = "vi";
    power-domains = <&bpmp TEGRA234_POWER_DOMAIN_VE>;
    iommus = <&smmu TEGRA234_SID_VI>;       /* SMMU stream ID */
    status = "okay";                         /* "okay" = enabled */
};
```

</details>

### 禁用未使用的硬件

禁用你用不到的外设（可缩短启动时间并降低功耗）：

```dts
/* In an overlay or board-level DTS */
&spi0 {
    status = "disabled";    /* Kernel skips probing this device */
};

&sound {
    status = "disabled";    /* No audio codec initialization */
};
```

每个被禁用的节点都能在启动时省下探测时间。

---

## 6. 设备树 Overlay

Overlay 无需修改原始 DTS 文件即可修改基础设备树。添加传感器、更改 pin muxing，或为产品配置外设，都靠它完成。

### Overlay 文件结构

```dts
/* my-camera-overlay.dts */
/dts-v1/;
/plugin/;

/ {
    overlay-name = "My Camera Overlay";
    compatible = "nvidia,p3768-0000+p3767-0000";

    fragment@0 {
        target = <&vi>;
        __overlay__ {
            num-channels = <1>;
            /* camera channel configuration */
        };
    };

    fragment@1 {
        target = <&i2c2>;
        __overlay__ {
            imx219@10 {
                compatible = "sony,imx219";
                reg = <0x10>;
                /* sensor properties */
            };
        };
    };
};
```

### 编译与应用 Overlay

```bash
# Compile overlay
dtc -@ -I dts -O dtb -o my-camera-overlay.dtbo my-camera-overlay.dts

# Copy to Jetson
scp my-camera-overlay.dtbo jetson:/boot/

# Apply via extlinux.conf
# Add to APPEND line:
# FDT /boot/tegra234-p3767-0000-p3768-0000-a0.dtb
# FDTOVERLAYS /boot/my-camera-overlay.dtbo
```

### Jetson-IO 工具

NVIDIA 提供 `jetson-io.py` 用于常见的 overlay 任务：

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
```

这个 GUI/CLI 工具可配置：

* Pin muxing（GPIO、SPI、I2C、UART）
* CSI 摄像头 lane
* 风扇控制
* SPI flash

---

## 7. NVIDIA 驱动模型 —— Jetson 驱动如何工作

Jetson 驱动与标准 Linux 驱动不同，因为其中许多驱动要与专用固件处理器交互。

### 驱动分类

| 类别 | 示例 | 工作方式 |
|------------------|-----------------------------|--------------------------------------------|
| **平台驱动**     | VI、ISP、NVCSI              | 直接 MMIO 寄存器访问、DMA           |
| **依赖 BPMP**  | 时钟、复位、电源门控 | 向 BPMP 固件发送 IPC 消息        |
| **固件 IPC** | nvgpu、camera RTC           | 通过 mailbox 与固件通信     |
| **标准驱动**     | USB、Ethernet、I2C          | 标准 Linux 驱动模型                |

### BPMP 驱动通信

T234 上的许多硬件控制都要经过 BPMP（Boot and Power Management Processor）：

```
Linux driver
   ↓ (IPC message via tegra-bpmp driver)
BPMP firmware
   ↓ (controls hardware registers)
Actual hardware (clocks, resets, power domains)
```

示例 —— 设置时钟频率：

```c
/* In kernel driver code */
clk = devm_clk_get(&pdev->dev, "vi");
clk_set_rate(clk, 400000000);  /* 400 MHz */

/* This doesn't directly program clock registers.
   Instead, the clock framework sends an IPC message to BPMP,
   which programs the actual PLL/divider registers. */
```

这意味着时钟与电源的变更要求 BPMP 固件处于运行且可响应的状态。若 BPMP 挂起，所有时钟/电源操作都会停滞。

### 模块加载顺序

Jetson 启动时，模块按依赖顺序加载：

```
tegra-bpmp.ko          ← First: BPMP IPC (needed by everything)
 ↓
tegra-fuse.ko          ← Fuse reading (chip identification)
 ↓
arm-smmu.ko            ← IOMMU (needed before any DMA device)
 ↓
nvgpu.ko               ← GPU driver
 ↓
tegra-vi.ko            ← Video Input
tegra-isp.ko           ← Image Signal Processor
nvcsi.ko               ← CSI controller
 ↓
(camera sensor drivers) ← I2C sensor drivers (imx219, etc.)
```

若某个模块加载失败，所有依赖它的模块也会失败。这就是 kernel/模块版本不匹配会导致级联故障的原因。

---

## 8. Orin Nano 上的关键 Kernel 子系统

### 内存管理

详见 [Memory Architecture Deep Dive](../07-Orin-Nano内存架构/Guide.md) 了解全部细节。与 kernel 相关的要点：

* **CMA** —— 通过 kernel 命令行或 DTB 配置；对摄像头与 DMA 至关重要
* **SMMU** —— ARM SMMU v2 驱动（`arm-smmu.ko`）；为所有 DMA 设备映射 IOVA
* **DMA-BUF** —— kernel 缓冲区共享框架；实现各引擎之间的零拷贝

### 中断处理

T234 使用 ARM GICv3（Generic Interrupt Controller）：

```bash
# View interrupt distribution across CPUs
cat /proc/interrupts

# Key interrupts on Orin Nano
# nvgpu           — GPU interrupts
# tegra-vi        — camera frame complete
# host1x          — multimedia engine sync
# tegra-pmc       — power management controller
```

对于实时工作负载，可用 `irqbalance` 或手动 `/proc/irq/<N>/smp_affinity` 将特定中断绑定到特定 CPU 核。


<details>
<summary>English original</summary>

**Disabling Unused Hardware**

To disable a peripheral you are not using (reduces boot time and power):

```dts
/* In an overlay or board-level DTS */
&spi0 {
    status = "disabled";    /* Kernel skips probing this device */
};

&sound {
    status = "disabled";    /* No audio codec initialization */
};
```

Every disabled node saves probe time during boot.

---

**6. Device Tree Overlays**

Overlays modify the base device tree without editing the original DTS files. This is how you add sensors, change pin muxing, or configure peripherals for your product.

**Overlay File Structure**

```dts
/* my-camera-overlay.dts */
/dts-v1/;
/plugin/;

/ {
    overlay-name = "My Camera Overlay";
    compatible = "nvidia,p3768-0000+p3767-0000";

    fragment@0 {
        target = <&vi>;
        __overlay__ {
            num-channels = <1>;
            /* camera channel configuration */
        };
    };

    fragment@1 {
        target = <&i2c2>;
        __overlay__ {
            imx219@10 {
                compatible = "sony,imx219";
                reg = <0x10>;
                /* sensor properties */
            };
        };
    };
};
```

**Compiling and Applying Overlays**

```bash
# Compile overlay
dtc -@ -I dts -O dtb -o my-camera-overlay.dtbo my-camera-overlay.dts

# Copy to Jetson
scp my-camera-overlay.dtbo jetson:/boot/

# Apply via extlinux.conf
# Add to APPEND line:
# FDT /boot/tegra234-p3767-0000-p3768-0000-a0.dtb
# FDTOVERLAYS /boot/my-camera-overlay.dtbo
```

**Jetson-IO Tool**

NVIDIA provides `jetson-io.py` for common overlay tasks:

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
```

This GUI/CLI tool configures:

* Pin muxing (GPIO, SPI, I2C, UART)
* CSI camera lanes
* Fan control
* SPI flash

---

**7. NVIDIA Driver Model — How Jetson Drivers Work**

Jetson drivers differ from standard Linux drivers because many interact with dedicated firmware processors.

**Driver Categories**

| Category         | Examples                    | How They Work                              |
|------------------|-----------------------------|--------------------------------------------|
| **Platform**     | VI, ISP, NVCSI              | Direct MMIO register access, DMA           |
| **BPMP-backed**  | Clocks, resets, power gates | Sends IPC messages to BPMP firmware        |
| **Firmware IPC** | nvgpu, camera RTC           | Communicates with firmware via mailbox     |
| **Standard**     | USB, Ethernet, I2C          | Standard Linux driver model                |

**BPMP Driver Communication**

Many hardware controls on T234 go through BPMP (Boot and Power Management Processor):

```
Linux driver
   ↓ (IPC message via tegra-bpmp driver)
BPMP firmware
   ↓ (controls hardware registers)
Actual hardware (clocks, resets, power domains)
```

Example — setting a clock rate:

```c
/* In kernel driver code */
clk = devm_clk_get(&pdev->dev, "vi");
clk_set_rate(clk, 400000000);  /* 400 MHz */

/* This doesn't directly program clock registers.
   Instead, the clock framework sends an IPC message to BPMP,
   which programs the actual PLL/divider registers. */
```

This means clock and power changes require BPMP firmware to be running and responsive. If BPMP hangs, all clock/power operations stall.

**Module Loading Order**

On Jetson boot, modules load in dependency order:

```
tegra-bpmp.ko          ← First: BPMP IPC (needed by everything)
 ↓
tegra-fuse.ko          ← Fuse reading (chip identification)
 ↓
arm-smmu.ko            ← IOMMU (needed before any DMA device)
 ↓
nvgpu.ko               ← GPU driver
 ↓
tegra-vi.ko            ← Video Input
tegra-isp.ko           ← Image Signal Processor
nvcsi.ko               ← CSI controller
 ↓
(camera sensor drivers) ← I2C sensor drivers (imx219, etc.)
```

If a module fails to load, all dependent modules also fail. This is why kernel/module version mismatch causes cascading failures.

---

**8. Key Kernel Subsystems on Orin Nano**

**Memory Management**

See [Memory Architecture Deep Dive](../07-Orin-Nano内存架构/Guide.md) for full details. Kernel-relevant highlights:

* **CMA** — configured via kernel command line or DTB; critical for camera and DMA
* **SMMU** — ARM SMMU v2 driver (`arm-smmu.ko`); maps IOVA for all DMA devices
* **DMA-BUF** — kernel buffer sharing framework; enables zero-copy between engines

**Interrupt Handling**

T234 uses ARM GICv3 (Generic Interrupt Controller):

```bash
# View interrupt distribution across CPUs
cat /proc/interrupts

# Key interrupts on Orin Nano
# nvgpu           — GPU interrupts
# tegra-vi        — camera frame complete
# host1x          — multimedia engine sync
# tegra-pmc       — power management controller
```

For real-time workloads, you can pin specific interrupts to specific CPU cores using `irqbalance` or manual `/proc/irq/<N>/smp_affinity`.

</details>

### 调度

默认调度器是 CFS（Completely Fair Scheduler）。对于延迟敏感的推理：

```bash
# Set real-time priority for inference process
sudo chrt -f 50 ./my_inference_app

# Or use SCHED_DEADLINE for guaranteed timing
# (requires kernel CONFIG_SCHED_DEADLINE=y)
```

### 文件系统

默认 rootfs 文件系统是 ext4。生产环境：

* **ext4** — 可靠、久经测试、支持日志
* **squashfs** — 只读压缩 rootfs（节省存储、挂载快）
* **overlayfs** — 叠在只读 squashfs 之上的可写层

---

## 9. 摄像头 kernel 栈（V4L2 + NVIDIA）

Jetson 上的摄像头子系统是最复杂的 kernel 栈之一。

### 架构

```
Userspace:
  libargus / nvargus-daemon
  GStreamer (nvv4l2camerasrc)
  V4L2 ioctl interface
       ↓
Kernel:
  ┌─────────────────────────────────────────┐
  │ V4L2 subsystem (media controller)       │
  │   ↓                                     │
  │ tegra-video (nvidia,tegra234-vi)         │
  │   ↓                                     │
  │ tegra-isp (nvidia,tegra234-isp)          │
  │   ↓                                     │
  │ nvcsi (nvidia,tegra234-nvcsi)            │
  │   ↓                                     │
  │ I2C sensor driver (e.g., imx219)         │
  └─────────────────────────────────────────┘
       ↓
Hardware:
  Sensor → CSI lanes → NVCSI → VI → ISP → DRAM (via SMMU)
```

### Media Controller 图

Linux media controller 把流水线暴露成一张图：

```bash
# View media graph
media-ctl -p -d /dev/media0

# Output shows connected entities:
# entity: nvcsi-0 → vi-output-0 → tegra-isp
```

### sensor 驱动的构成

每个摄像头 sensor 都需要一个 kernel 驱动：

```c
static const struct of_device_id imx219_of_match[] = {
    { .compatible = "sony,imx219" },
    { },
};

static struct i2c_driver imx219_driver = {
    .driver = {
        .name = "imx219",
        .of_match_table = imx219_of_match,
    },
    .probe = imx219_probe,
    .remove = imx219_remove,
};
```

probe 函数：

1. 通过 I2C 读取 sensor ID
2. 注册 V4L2 subdevice
3. 配置格式、分辨率、帧率
4. 在 media graph 中连接到 NVCSI

### 新增一个摄像头 sensor

1. 编写或移植 sensor 驱动（I2C、V4L2 subdev）
2. 在 I2C 总线下添加地址正确的设备树节点
3. 添加用于 CSI 通道配置的设备树节点
4. 创建设备树 overlay，连接 sensor → NVCSI → VI
5. 构建并安装驱动模块
6. 用 `v4l2-ctl --list-devices` 验证

---

## 10. GPU kernel 驱动（nvgpu）

### 架构

nvgpu 驱动是 NVIDIA 的 Jetson GPU 内核模块：

```
CUDA runtime (userspace)
   ↓ (ioctl)
/dev/nvhost-gpu
   ↓
nvgpu.ko (kernel)
   ↓
GPU hardware (Ampere cores, memory controller, tensor cores)
   ↓
SMMU (memory isolation)
   ↓
DRAM
```

### nvgpu 的主要职责

* GPU 电源管理（power gating、时钟伸缩）
* 通道管理（多个 CUDA 上下文）
* 内存管理（GPU 页表、缓冲区映射）
* Fence/sync point 管理（各引擎之间的同步）
* 固件加载（GPU 微码）

### GPU 固件

GPU 需要在驱动 probe 期间加载固件：

```
/lib/firmware/nvidia/gv11b/
├── acr_ucode.bin          ← ACR (Advanced Code Region) loader
├── fecs.bin               ← Front End Context Switching
├── gpccs.bin              ← GPC Context Switching
└── ...
```

如果固件文件缺失或损坏：

```
nvgpu: firmware "nvidia/gv11b/acr_ucode.bin" not found
```

GPU 将无法初始化 —— 没有 CUDA、没有推理、没有显示加速。

### nvgpu 模块参数

```bash
# View current parameters
cat /sys/module/nvgpu/parameters/*

# Key parameters:
# enable_elcg   — Enable clock gating (saves power)
# enable_elpg   — Enable power gating (deeper sleep)
# gpu_freq      — Current GPU frequency
```

---

## 11. kernel 中的电源与时钟管理

### nvpmodel —— 电源模式框架

nvpmodel 是一个用户态工具，用于配置 kernel 的电源与时钟设置：

```bash
# View current power mode
sudo nvpmodel -q

# Set to maximum performance (15W on Orin Nano)
sudo nvpmodel -m 0

# Set to power-saving mode (7W)
sudo nvpmodel -m 1
```

在内部，nvpmodel 写入：

* `/sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq` — CPU 最大频率
* `/sys/devices/gpu.0/devfreq/*/max_freq` — GPU 最大频率
* `/sys/kernel/nvpmodel_emc_cap/emc_iso_cap` — 内存带宽上限

### 时钟树

T234 上的时钟由 BPMP 固件管理。kernel 时钟框架通过 IPC 发送请求：

```bash
# View clock tree
cat /sys/kernel/debug/clk/clk_summary

# Key clocks:
# vi_clk      — Video Input (camera capture rate)
# isp_clk     — ISP processing rate
# nvdec_clk   — Video decoder
# gpu_clk     — GPU core clock
# emc_clk     — Memory controller clock (DRAM bandwidth)
```


<details>
<summary>English original</summary>

**Scheduling**

Default scheduler is CFS (Completely Fair Scheduler). For latency-sensitive inference:

```bash
# Set real-time priority for inference process
sudo chrt -f 50 ./my_inference_app

# Or use SCHED_DEADLINE for guaranteed timing
# (requires kernel CONFIG_SCHED_DEADLINE=y)
```

**Filesystem**

Default rootfs filesystem is ext4. For production:

* **ext4** — reliable, well-tested, supports journaling
* **squashfs** — read-only compressed rootfs (saves storage, fast mount)
* **overlayfs** — writable layer on top of read-only squashfs

---

**9. Camera Kernel Stack (V4L2 + NVIDIA)**

The camera subsystem on Jetson is one of the most complex kernel stacks.

**Architecture**

```
Userspace:
  libargus / nvargus-daemon
  GStreamer (nvv4l2camerasrc)
  V4L2 ioctl interface
       ↓
Kernel:
  ┌─────────────────────────────────────────┐
  │ V4L2 subsystem (media controller)       │
  │   ↓                                     │
  │ tegra-video (nvidia,tegra234-vi)         │
  │   ↓                                     │
  │ tegra-isp (nvidia,tegra234-isp)          │
  │   ↓                                     │
  │ nvcsi (nvidia,tegra234-nvcsi)            │
  │   ↓                                     │
  │ I2C sensor driver (e.g., imx219)         │
  └─────────────────────────────────────────┘
       ↓
Hardware:
  Sensor → CSI lanes → NVCSI → VI → ISP → DRAM (via SMMU)
```

**Media Controller Graph**

The Linux media controller exposes the pipeline as a graph:

```bash
# View media graph
media-ctl -p -d /dev/media0

# Output shows connected entities:
# entity: nvcsi-0 → vi-output-0 → tegra-isp
```

**Sensor Driver Anatomy**

Every camera sensor needs a kernel driver:

```c
static const struct of_device_id imx219_of_match[] = {
    { .compatible = "sony,imx219" },
    { },
};

static struct i2c_driver imx219_driver = {
    .driver = {
        .name = "imx219",
        .of_match_table = imx219_of_match,
    },
    .probe = imx219_probe,
    .remove = imx219_remove,
};
```

The probe function:

1. Reads sensor ID via I2C
2. Registers V4L2 subdevice
3. Configures format, resolution, frame rate
4. Links to NVCSI in the media graph

**Adding a New Camera Sensor**

1. Write or port the sensor driver (I2C, V4L2 subdev)
2. Add device tree node under the I2C bus with correct address
3. Add device tree node for CSI channel configuration
4. Create device tree overlay linking sensor → NVCSI → VI
5. Build and install the driver module
6. Verify with `v4l2-ctl --list-devices`

---

**10. GPU Kernel Driver (nvgpu)**

**Architecture**

The nvgpu driver is NVIDIA's Jetson GPU kernel module:

```
CUDA runtime (userspace)
   ↓ (ioctl)
/dev/nvhost-gpu
   ↓
nvgpu.ko (kernel)
   ↓
GPU hardware (Ampere cores, memory controller, tensor cores)
   ↓
SMMU (memory isolation)
   ↓
DRAM
```

**Key nvgpu Responsibilities**

* GPU power management (power gating, clock scaling)
* Channel management (multiple CUDA contexts)
* Memory management (GPU page tables, buffer mapping)
* Fence/sync point management (synchronization between engines)
* Firmware loading (GPU microcode)

**GPU Firmware**

The GPU requires firmware loaded during driver probe:

```
/lib/firmware/nvidia/gv11b/
├── acr_ucode.bin          ← ACR (Advanced Code Region) loader
├── fecs.bin               ← Front End Context Switching
├── gpccs.bin              ← GPC Context Switching
└── ...
```

If firmware files are missing or corrupted:

```
nvgpu: firmware "nvidia/gv11b/acr_ucode.bin" not found
```

GPU will fail to initialize — no CUDA, no inference, no display acceleration.

**nvgpu Module Parameters**

```bash
# View current parameters
cat /sys/module/nvgpu/parameters/*

# Key parameters:
# enable_elcg   — Enable clock gating (saves power)
# enable_elpg   — Enable power gating (deeper sleep)
# gpu_freq      — Current GPU frequency
```

---

**11. Power and Clock Management in Kernel**

**nvpmodel — Power Mode Framework**

nvpmodel is a userspace tool that configures the kernel's power and clock settings:

```bash
# View current power mode
sudo nvpmodel -q

# Set to maximum performance (15W on Orin Nano)
sudo nvpmodel -m 0

# Set to power-saving mode (7W)
sudo nvpmodel -m 1
```

Internally, nvpmodel writes to:

* `/sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq` — CPU max frequency
* `/sys/devices/gpu.0/devfreq/*/max_freq` — GPU max frequency
* `/sys/kernel/nvpmodel_emc_cap/emc_iso_cap` — memory bandwidth cap

**Clock Tree**

Clocks on T234 are managed by BPMP firmware. The kernel clock framework sends requests via IPC:

```bash
# View clock tree
cat /sys/kernel/debug/clk/clk_summary

# Key clocks:
# vi_clk      — Video Input (camera capture rate)
# isp_clk     — ISP processing rate
# nvdec_clk   — Video decoder
# gpu_clk     — GPU core clock
# emc_clk     — Memory controller clock (DRAM bandwidth)
```

</details>

### 动态电压与频率调节（DVFS）

kernel 根据负载调整 GPU 和 CPU 的频率：

```bash
# GPU DVFS (devfreq governor)
cat /sys/devices/gpu.0/devfreq/*/governor
# "nvhost_podgov" — NVIDIA's load-based governor

# CPU DVFS (cpufreq governor)
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# "schedutil" — scheduler-integrated frequency scaling
```

### jetson_clocks

把所有时钟锁定到最大值（绕过 DVFS）：

```bash
sudo jetson_clocks

# Useful for benchmarking (deterministic performance)
# NOT recommended for production (high power, high temperature)
```

---

## 12. Orin Nano 上的 kernel 启动流程

UEFI 加载 kernel Image 之后：

### 早期启动（架构相关）

```
head.S (arch/arm64/kernel/head.S)
 ↓
__primary_switched
 ↓
start_kernel()          ← First C function
 ↓
setup_arch()            ← ARM64 architecture setup
  ├── parse DTB
  ├── setup page tables + MMU
  ├── configure memory zones
  └── reserve CMA
 ↓
mm_init()               ← Memory management init
  ├── buddy allocator
  ├── slab allocator (kmalloc)
  └── CMA init
```

### 驱动初始化

```
 ↓
driver_init()
  ├── buses_init()      ← Register bus types (platform, I2C, SPI, PCI)
  ├── devices_init()    ← Create /sys/devices
  └── platform_bus_init()
 ↓
do_initcalls()          ← Execute all __init functions (level 0–7)
  Level 0: pure_initcall    ← Very early (irqchip, clocksource)
  Level 1: core_initcall     ← Core subsystems
  Level 2: postcore_initcall ← Bus drivers
  Level 3: arch_initcall     ← Architecture-specific
  Level 4: subsys_initcall   ← Subsystem init (SMMU, DMA)
  Level 5: fs_initcall       ← Filesystem init
  Level 6: device_initcall   ← Device drivers (nvgpu, tegra-vi)
  Level 7: late_initcall     ← Late initialization
```

### 初始化后

```
 ↓
prepare_namespace()     ← Find and mount rootfs
  ├── initramfs or direct mount
  └── root= from kernel command line
 ↓
kernel_init()
 ↓
run_init_process("/sbin/init")  ← Hand off to systemd
```

### 测量启动时间

```bash
# Kernel boot timeline
dmesg | head -50
# [    0.000000] Booting Linux on physical CPU 0x0000000000
# [    0.123456] ... each line shows timestamp

# systemd-analyze for full boot breakdown
systemd-analyze
systemd-analyze blame    # Show slowest services
systemd-analyze critical-chain   # Show critical path
```

---

## 13. kernel 启动时间优化

默认的 L4T kernel 是按最大兼容性配置的，而非最短启动时间。生产系统应当优化。

### 优化分类

#### 1. 禁用未使用的设备树节点

每个启用的设备树节点都会触发驱动 probe。把不用的部分禁掉：

```dts
/* Disable SPI if not used */
&spi0 { status = "disabled"; };
&spi1 { status = "disabled"; };

/* Disable audio if not used */
&sound { status = "disabled"; };
&tegra_sound { status = "disabled"; };

/* Disable USB if not used */
&xusb { status = "disabled"; };
```

设备树目录：

```
<top>/hardware/nvidia/platform/t23x/
<top>/hardware/nvidia/soc/t23x/
```

#### 2. 关闭 UART 上的控制台打印

控制台打印是启动时间的一大瓶颈。每次 `printk` 都要等待 UART 传输。

对 Orin 系列，编辑平台配置文件并移除：

```
console=ttyTCU0,115200
```

启动后仍可通过 framebuffer console 或 `dmesg` 查看日志。

或者降低控制台输出的详细程度：

```bash
# In kernel command line (extlinux.conf)
APPEND ... loglevel=1 quiet
```

`loglevel=1` 只显示 KERN_EMERG。`quiet` 会抑制大部分启动消息。

#### 3. 将非必要驱动模块化

把启动时不需要的驱动从内建（`=y`）改为模块（`=m`）：

```
# Filesystems (loaded on demand)
CONFIG_FUSE_FS=m
CONFIG_VFAT_FS=m
CONFIG_NTFS_FS=m

# HID (USB keyboard/mouse — not needed at boot for headless)
CONFIG_USB_HID=m
CONFIG_HID_GENERIC=m

# Network drivers not used at boot
CONFIG_NET_VENDOR_INTEL=m
CONFIG_NET_VENDOR_REALTEK=m

# QSPI (not needed after boot)
CONFIG_SPI_TEGRA210_QUAD=m
```

这样可减小 kernel Image 体积并推迟初始化。

#### 4. 使用异步 probe

驱动可以异步 probe，而不阻塞启动流程：

```c
static struct platform_driver my_driver = {
    .driver = {
        .name = "my-driver",
        .of_match_table = my_of_match,
        .probe_type = PROBE_PREFER_ASYNCHRONOUS,  /* Non-blocking probe */
    },
    .probe = my_probe,
    .remove = my_remove,
};
```

NVIDIA 已对部分驱动这么做。你可以把它加到自定义驱动上，或给更多 Jetson 驱动打补丁。

#### 5. 禁用音频配置

如果产品没有音频（纯视觉 AI 边缘设备很常见）：

```
# CONFIG_SND_SOC_TEGRA_ALT is not set
# CONFIG_SND_SOC_TEGRA_ALT_FORCE_CARD_REG is not set
# CONFIG_SND_SOC_TEGRA_T186REF_ALT is not set
# CONFIG_SND_SOC_TEGRA_T186REF_MOBILE_ALT is not set
```

音频 codec 初始化会增加数百毫秒。


<details>
<summary>English original</summary>

**Dynamic Voltage and Frequency Scaling (DVFS)**

The kernel adjusts GPU and CPU frequencies based on load:

```bash
# GPU DVFS (devfreq governor)
cat /sys/devices/gpu.0/devfreq/*/governor
# "nvhost_podgov" — NVIDIA's load-based governor

# CPU DVFS (cpufreq governor)
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# "schedutil" — scheduler-integrated frequency scaling
```

**jetson_clocks**

Locks all clocks to maximum (bypasses DVFS):

```bash
sudo jetson_clocks

# Useful for benchmarking (deterministic performance)
# NOT recommended for production (high power, high temperature)
```

---

**12. Kernel Boot Sequence on Orin Nano**

After UEFI loads the kernel Image:

**Early Boot (Architecture-Specific)**

```
head.S (arch/arm64/kernel/head.S)
 ↓
__primary_switched
 ↓
start_kernel()          ← First C function
 ↓
setup_arch()            ← ARM64 architecture setup
  ├── parse DTB
  ├── setup page tables + MMU
  ├── configure memory zones
  └── reserve CMA
 ↓
mm_init()               ← Memory management init
  ├── buddy allocator
  ├── slab allocator (kmalloc)
  └── CMA init
```

**Driver Initialization**

```
 ↓
driver_init()
  ├── buses_init()      ← Register bus types (platform, I2C, SPI, PCI)
  ├── devices_init()    ← Create /sys/devices
  └── platform_bus_init()
 ↓
do_initcalls()          ← Execute all __init functions (level 0–7)
  Level 0: pure_initcall    ← Very early (irqchip, clocksource)
  Level 1: core_initcall     ← Core subsystems
  Level 2: postcore_initcall ← Bus drivers
  Level 3: arch_initcall     ← Architecture-specific
  Level 4: subsys_initcall   ← Subsystem init (SMMU, DMA)
  Level 5: fs_initcall       ← Filesystem init
  Level 6: device_initcall   ← Device drivers (nvgpu, tegra-vi)
  Level 7: late_initcall     ← Late initialization
```

**Post-Init**

```
 ↓
prepare_namespace()     ← Find and mount rootfs
  ├── initramfs or direct mount
  └── root= from kernel command line
 ↓
kernel_init()
 ↓
run_init_process("/sbin/init")  ← Hand off to systemd
```

**Measuring Boot Time**

```bash
# Kernel boot timeline
dmesg | head -50
# [    0.000000] Booting Linux on physical CPU 0x0000000000
# [    0.123456] ... each line shows timestamp

# systemd-analyze for full boot breakdown
systemd-analyze
systemd-analyze blame    # Show slowest services
systemd-analyze critical-chain   # Show critical path
```

---

**13. Kernel Boot Time Optimization**

The default L4T kernel is configured for maximum compatibility, not minimum boot time. Production systems should optimize.

**Optimization Categories**

**1. Disable Unused Device Tree Nodes**

Every enabled device tree node triggers driver probing. Disable what you do not use:

```dts
/* Disable SPI if not used */
&spi0 { status = "disabled"; };
&spi1 { status = "disabled"; };

/* Disable audio if not used */
&sound { status = "disabled"; };
&tegra_sound { status = "disabled"; };

/* Disable USB if not used */
&xusb { status = "disabled"; };
```

Device tree directories:

```
<top>/hardware/nvidia/platform/t23x/
<top>/hardware/nvidia/soc/t23x/
```

**2. Disable Console Printing Over UART**

Console printing is a major boot time bottleneck. Each `printk` waits for UART transmission.

For Orin series, edit the platform configuration file and remove:

```
console=ttyTCU0,115200
```

You can still review logs via framebuffer console or `dmesg` after boot.

Alternatively, reduce console verbosity:

```bash
# In kernel command line (extlinux.conf)
APPEND ... loglevel=1 quiet
```

`loglevel=1` shows only KERN_EMERG. `quiet` suppresses most boot messages.

**3. Modularize Non-Essential Drivers**

Move drivers not needed at boot time from built-in (`=y`) to module (`=m`):

```
# Filesystems (loaded on demand)
CONFIG_FUSE_FS=m
CONFIG_VFAT_FS=m
CONFIG_NTFS_FS=m

# HID (USB keyboard/mouse — not needed at boot for headless)
CONFIG_USB_HID=m
CONFIG_HID_GENERIC=m

# Network drivers not used at boot
CONFIG_NET_VENDOR_INTEL=m
CONFIG_NET_VENDOR_REALTEK=m

# QSPI (not needed after boot)
CONFIG_SPI_TEGRA210_QUAD=m
```

This reduces kernel Image size and defers initialization.

**4. Use Asynchronous Probe**

Drivers can probe asynchronously instead of blocking the boot sequence:

```c
static struct platform_driver my_driver = {
    .driver = {
        .name = "my-driver",
        .of_match_table = my_of_match,
        .probe_type = PROBE_PREFER_ASYNCHRONOUS,  /* Non-blocking probe */
    },
    .probe = my_probe,
    .remove = my_remove,
};
```

NVIDIA already uses this for some drivers. You can add it to custom drivers or patch additional Jetson drivers.

**5. Disable Audio Configurations**

If your product has no audio (common for vision-only AI edge devices):

```
# CONFIG_SND_SOC_TEGRA_ALT is not set
# CONFIG_SND_SOC_TEGRA_ALT_FORCE_CARD_REG is not set
# CONFIG_SND_SOC_TEGRA_T186REF_ALT is not set
# CONFIG_SND_SOC_TEGRA_T186REF_MOBILE_ALT is not set
```

Audio codec initialization adds hundreds of milliseconds.

</details>

#### 6. 禁用内核调试

生产内核不应包含调试基础设施：

```
# CONFIG_FTRACE is not set
# CONFIG_FUNCTION_TRACER is not set
# CONFIG_KMEMLEAK is not set
# CONFIG_DEBUG_INFO is not set
# CONFIG_DYNAMIC_DEBUG is not set
# CONFIG_DEBUG_FS is not set           # Removes /sys/kernel/debug
# CONFIG_KPROBES is not set
# CONFIG_PROFILING is not set
```

这可减小内核体积、缩短启动时间、降低内存占用。

#### 7. 优化 initramfs

* 从 initramfs 中移除不必要的模块
* 使用 `lz4` 压缩（解压速度比 gzip 快）
* 若 rootfs 位于 NVMe 且始终存在，可考虑不使用 initramfs 启动

### 启动时间测量

```bash
# Total kernel boot time (from first message to init)
dmesg | grep "Freeing unused kernel"
# Time between first dmesg line and this line = kernel boot time

# Driver probe times
dmesg | grep "probe" | sort -t'[' -k2 -n

# Detailed boot chart
systemd-analyze plot > boot.svg
```

### 典型结果

| Configuration        | Kernel Boot Time |
|----------------------|-----------------|
| 默认 L4T 内核   | 8–15 秒    |
| 优化后（见上文）    | 3–6 秒     |
| 激进（最小化） | 1–3 秒     |

---

## 14. 编写自定义内核模块

### 模块骨架

```c
/* my_module.c */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

static int my_probe(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "my_module probed\n");
    /* Initialize hardware, register interfaces */
    return 0;
}

static int my_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "my_module removed\n");
    return 0;
}

static const struct of_device_id my_of_match[] = {
    { .compatible = "my-company,my-device" },
    { },
};
MODULE_DEVICE_TABLE(of, my_of_match);

static struct platform_driver my_driver = {
    .driver = {
        .name = "my-module",
        .of_match_table = my_of_match,
    },
    .probe = my_probe,
    .remove = my_remove,
};
module_platform_driver(my_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("My custom Jetson module");
```

### Makefile（树外构建）

```makefile
# Makefile
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

obj-m := my_module.o

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
```

### 构建与加载

```bash
# Build (on Jetson or cross-compile)
make KERNEL_SRC=/path/to/kernel-5.10

# Load
sudo insmod my_module.ko

# Verify
dmesg | tail -5
lsmod | grep my_module

# Auto-load on boot
sudo cp my_module.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
echo "my_module" | sudo tee /etc/modules-load.d/my_module.conf
```

### 从模块访问硬件

Jetson 模块的常见模式：

```c
/* Memory-mapped I/O */
void __iomem *base = devm_ioremap_resource(&pdev->dev, res);
writel(0x1, base + CONTROL_REG);

/* Clocks (via BPMP) */
struct clk *clk = devm_clk_get(&pdev->dev, "my-clk");
clk_prepare_enable(clk);

/* DMA with SMMU */
dma_addr_t dma_handle;
void *buf = dma_alloc_coherent(&pdev->dev, size, &dma_handle, GFP_KERNEL);

/* GPIO */
struct gpio_desc *gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
```

---

## 15. Jetson 上的内核调试

### 串口控制台

最主要的调试工具。连接到调试 UART 排针：

```
Baud: 115200
Data: 8N1
```

所有内核消息（`printk`）都会在此出现。在 SSH 不可用的启动失败场景中不可或缺。

### 动态调试

在 runtime 按文件或按函数启用调试消息：

```bash
# Enable debug for nvgpu driver
echo "module nvgpu +p" | sudo tee /sys/kernel/debug/dynamic_debug/control

# Enable debug for a specific file
echo "file tegra-vi.c +p" | sudo tee /sys/kernel/debug/dynamic_debug/control

# View all available debug points
cat /sys/kernel/debug/dynamic_debug/control | grep tegra
```

需要 `CONFIG_DYNAMIC_DEBUG=y`（默认启用，生产环境应禁用）。

### ftrace

用于性能剖析与调试的内核函数跟踪器：

```bash
# Trace all function calls
echo function | sudo tee /sys/kernel/debug/tracing/current_tracer
echo 1 | sudo tee /sys/kernel/debug/tracing/tracing_on

# Read trace
cat /sys/kernel/debug/tracing/trace

# Trace specific functions
echo nvgpu_* | sudo tee /sys/kernel/debug/tracing/set_ftrace_filter
```


<details>
<summary>English original</summary>

**6. Disable Kernel Debugging**

Production kernels should not include debug infrastructure:

```
# CONFIG_FTRACE is not set
# CONFIG_FUNCTION_TRACER is not set
# CONFIG_KMEMLEAK is not set
# CONFIG_DEBUG_INFO is not set
# CONFIG_DYNAMIC_DEBUG is not set
# CONFIG_DEBUG_FS is not set           # Removes /sys/kernel/debug
# CONFIG_KPROBES is not set
# CONFIG_PROFILING is not set
```

This reduces kernel size, boot time, and memory usage.

**7. Optimize initramfs**

* Remove unnecessary modules from initramfs
* Use `lz4` compression (faster decompression than gzip)
* If rootfs is on NVMe and always present, consider booting without initramfs

**Boot Time Measurement**

```bash
# Total kernel boot time (from first message to init)
dmesg | grep "Freeing unused kernel"
# Time between first dmesg line and this line = kernel boot time

# Driver probe times
dmesg | grep "probe" | sort -t'[' -k2 -n

# Detailed boot chart
systemd-analyze plot > boot.svg
```

**Typical Results**

| Configuration        | Kernel Boot Time |
|----------------------|-----------------|
| Default L4T kernel   | 8–15 seconds    |
| Optimized (above)    | 3–6 seconds     |
| Aggressive (minimal) | 1–3 seconds     |

---

**14. Writing Custom Kernel Modules**

**Module Skeleton**

```c
/* my_module.c */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

static int my_probe(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "my_module probed\n");
    /* Initialize hardware, register interfaces */
    return 0;
}

static int my_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "my_module removed\n");
    return 0;
}

static const struct of_device_id my_of_match[] = {
    { .compatible = "my-company,my-device" },
    { },
};
MODULE_DEVICE_TABLE(of, my_of_match);

static struct platform_driver my_driver = {
    .driver = {
        .name = "my-module",
        .of_match_table = my_of_match,
    },
    .probe = my_probe,
    .remove = my_remove,
};
module_platform_driver(my_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("My custom Jetson module");
```

**Makefile (Out-of-Tree Build)**

```makefile
# Makefile
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

obj-m := my_module.o

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
```

**Build and Load**

```bash
# Build (on Jetson or cross-compile)
make KERNEL_SRC=/path/to/kernel-5.10

# Load
sudo insmod my_module.ko

# Verify
dmesg | tail -5
lsmod | grep my_module

# Auto-load on boot
sudo cp my_module.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
echo "my_module" | sudo tee /etc/modules-load.d/my_module.conf
```

**Accessing Hardware From a Module**

Common patterns for Jetson modules:

```c
/* Memory-mapped I/O */
void __iomem *base = devm_ioremap_resource(&pdev->dev, res);
writel(0x1, base + CONTROL_REG);

/* Clocks (via BPMP) */
struct clk *clk = devm_clk_get(&pdev->dev, "my-clk");
clk_prepare_enable(clk);

/* DMA with SMMU */
dma_addr_t dma_handle;
void *buf = dma_alloc_coherent(&pdev->dev, size, &dma_handle, GFP_KERNEL);

/* GPIO */
struct gpio_desc *gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
```

---

**15. Kernel Debugging on Jetson**

**Serial Console**

The primary debugging tool. Connect to the debug UART header:

```
Baud: 115200
Data: 8N1
```

All kernel messages (`printk`) appear here. Essential for boot failures where SSH is not available.

**Dynamic Debug**

Enable per-file or per-function debug messages at runtime:

```bash
# Enable debug for nvgpu driver
echo "module nvgpu +p" | sudo tee /sys/kernel/debug/dynamic_debug/control

# Enable debug for a specific file
echo "file tegra-vi.c +p" | sudo tee /sys/kernel/debug/dynamic_debug/control

# View all available debug points
cat /sys/kernel/debug/dynamic_debug/control | grep tegra
```

Requires `CONFIG_DYNAMIC_DEBUG=y` (enabled by default, disable for production).

**ftrace**

Kernel function tracer for profiling and debugging:

```bash
# Trace all function calls
echo function | sudo tee /sys/kernel/debug/tracing/current_tracer
echo 1 | sudo tee /sys/kernel/debug/tracing/tracing_on

# Read trace
cat /sys/kernel/debug/tracing/trace

# Trace specific functions
echo nvgpu_* | sudo tee /sys/kernel/debug/tracing/set_ftrace_filter
```

</details>

### Kernel 崩溃分析

如果 kernel panic：

1. 抓取串口控制台输出（其中含 panic trace）
2. 解码栈 trace：

```bash
# On host, with matching vmlinux
scripts/decode_stacktrace.sh vmlinux < panic_log.txt
```

3. Jetson 常见 panic 原因：

| Panic 消息                     | 可能原因                          |
|-----------------------------------|---------------------------------------|
| `Unable to handle kernel NULL pointer` | 驱动 bug（指针未初始化） |
| `arm-smmu: Unhandled context fault`    | DMA 到未映射地址            |
| `nvgpu: gpu init failed`              | GPU 固件缺失或损坏     |
| `kernel BUG at mm/page_alloc.c`       | 内存损坏或 CMA 问题      |

---

## 16. 生产环境 Kernel 加固

### Lockdown

```
CONFIG_SECURITY_LOCKDOWN_LSM=y
CONFIG_LOCK_DOWN_KERNEL_FORCE_INTEGRITY=y
```

阻止加载未签名模块，并限制对敏感 kernel 接口的访问。

### 模块签名

```
CONFIG_MODULE_SIG=y
CONFIG_MODULE_SIG_FORCE=y
CONFIG_MODULE_SIG_SHA256=y
```

只有用你的私钥签名的模块才能被加载。防止未授权代码在 kernel 空间执行。

### 移除调试接口

```
# CONFIG_DEBUG_FS is not set       # No /sys/kernel/debug
# CONFIG_PROC_KCORE is not set     # No /proc/kcore (memory dump)
# CONFIG_KEXEC is not set          # No kexec (reboot into arbitrary kernel)
# CONFIG_KALLSYMS is not set       # No symbol table (harder to exploit)
```

### 地址空间随机化

```
CONFIG_RANDOMIZE_BASE=y          # KASLR
CONFIG_RANDOMIZE_MODULE_REGION_FULL=y
```

通过随机化内存布局，加大 kernel 漏洞利用的难度。

### Watchdog

启用硬件 watchdog，以便从 kernel 挂起中恢复：

```bash
# Enable watchdog
sudo systemctl enable watchdog
sudo systemctl start watchdog

# Configure timeout (e.g., 60 seconds)
echo 60 | sudo tee /sys/class/watchdog/watchdog0/timeout
```

如果 kernel 挂起且无人喂 watchdog，系统会自动重启。配合 A/B 冗余，可实现自动恢复。

---

## 17. 基于 A/B 的 Kernel 更新策略

在 A/B 系统中更新 kernel 时，kernel、DTB 和模块必须原子地一起更新。

### 每个 slot 更新哪些内容

| 组件    | 分区        | 必须匹配           |
|--------------|------------------|----------------------|
| Kernel Image | `kernel` / `kernel_b` | 模块版本   |
| DTB          | `kernel-dtb` / `kernel-dtb_b` | Kernel 版本 |
| 模块          | rootfs 中的 `/lib/modules/` | Kernel 版本 |
| GPU 固件      | rootfs 中的 `/lib/firmware/` | nvgpu 版本  |

### 安全更新流程

```
1. Currently running Slot A (kernel 5.10.104-tegra)
2. Prepare Slot B:
   a. Write new kernel Image to kernel_b partition
   b. Write new DTB to kernel-dtb_b partition
   c. Write new rootfs (with matching modules + firmware) to APP_b
3. Set Slot B active
4. Reboot
5. Validate (GPU loads, camera works, inference runs)
6. Mark Slot B successful
```

完整的 OTA 细节见 [Rootfs & A/B Redundancy Guide](../12-Orin-Nano-Rootfs与AB冗余/Guide.md)。

### 绝对不要这样做

* 只更新 kernel 而不更新模块 → `modprobe` 失败
* 更新模块但不更新 kernel → 版本不匹配导致 panic
* 用 `apt upgrade` 更新 kernel 包 → 破坏 A/B 不变式
* 跨 slot 混用 JetPack 版本 → 驱动/固件不兼容

---

## 18. 常见 Kernel 问题与解决方案

### GPU 驱动加载失败

```
nvgpu: probe of gpu.0 failed with error -110
```

**原因：**
* `/lib/firmware/nvidia/` 中缺少 GPU 固件
* DTB 中未使能电源域
* BPMP 固件版本不匹配

**解决方案：** 确认固件文件存在，检查 `dmesg | grep bpmp`，确保 DTB 与你的 JetPack 版本匹配。

### 摄像头未被检测到

```
tegra-vi: no channels found
```

**原因：**
* 摄像头设备树节点缺失或 `status = "disabled"`
* DTB 中 I2C 地址错误
* CSI lane 配置不匹配
* Sensor 驱动未加载

**解决方案：** 检查 `media-ctl -p`，核对 DTB 中的摄像头节点，检查 `i2cdetect` 确认 sensor 是否存在。

### 模块版本不匹配

```
modprobe: FATAL: Module nvgpu not found in directory /lib/modules/5.10.104-tegra
```

**原因：** Kernel 版本与已安装的模块不匹配。

**解决方案：** 重新构建并安装与运行中 kernel 匹配的模块，或用匹配的 kernel + rootfs 重新刷写。

### 推理期间的 Kernel OOM

```
Out of memory: Killed process 1234 (python3)
```

**原因：** CUDA + CPU + 摄像头内存合计超出可用 RAM。

**解决方案：** 量化模型（INT8）、减少摄像头 buffer 数量、使用 DLA 卸载、用 `tegrastats` 监控。见 [Memory Architecture Guide](../07-Orin-Nano内存架构/Guide.md)。


<details>
<summary>English original</summary>

**Kernel Crash Analysis**

If kernel panics:

1. Capture serial console output (has the panic trace)
2. Decode the stack trace:

```bash
# On host, with matching vmlinux
scripts/decode_stacktrace.sh vmlinux < panic_log.txt
```

3. Common Jetson panic causes:

| Panic Message                     | Likely Cause                          |
|-----------------------------------|---------------------------------------|
| `Unable to handle kernel NULL pointer` | Driver bug (uninitialized pointer) |
| `arm-smmu: Unhandled context fault`    | DMA to unmapped address            |
| `nvgpu: gpu init failed`              | GPU firmware missing or corrupt     |
| `kernel BUG at mm/page_alloc.c`       | Memory corruption or CMA issue      |

---

**16. Production Kernel Hardening**

**Lockdown**

```
CONFIG_SECURITY_LOCKDOWN_LSM=y
CONFIG_LOCK_DOWN_KERNEL_FORCE_INTEGRITY=y
```

Prevents unsigned module loading and restricts access to sensitive kernel interfaces.

**Module Signing**

```
CONFIG_MODULE_SIG=y
CONFIG_MODULE_SIG_FORCE=y
CONFIG_MODULE_SIG_SHA256=y
```

Only modules signed with your private key can be loaded. Prevents unauthorized code execution in kernel space.

**Remove Debug Interfaces**

```
# CONFIG_DEBUG_FS is not set       # No /sys/kernel/debug
# CONFIG_PROC_KCORE is not set     # No /proc/kcore (memory dump)
# CONFIG_KEXEC is not set          # No kexec (reboot into arbitrary kernel)
# CONFIG_KALLSYMS is not set       # No symbol table (harder to exploit)
```

**Address Space Randomization**

```
CONFIG_RANDOMIZE_BASE=y          # KASLR
CONFIG_RANDOMIZE_MODULE_REGION_FULL=y
```

Makes kernel exploitation harder by randomizing memory layout.

**Watchdog**

Enable hardware watchdog to recover from kernel hangs:

```bash
# Enable watchdog
sudo systemctl enable watchdog
sudo systemctl start watchdog

# Configure timeout (e.g., 60 seconds)
echo 60 | sudo tee /sys/class/watchdog/watchdog0/timeout
```

If the kernel hangs and the watchdog is not fed, the system reboots automatically. Combined with A/B redundancy, this provides automatic recovery.

---

**17. Kernel Update Strategy With A/B**

When updating the kernel in an A/B system, the kernel, DTB, and modules must be updated atomically.

**What Gets Updated Per Slot**

| Component    | Partition        | Must Match           |
|--------------|------------------|----------------------|
| Kernel Image | `kernel` / `kernel_b` | Module version   |
| DTB          | `kernel-dtb` / `kernel-dtb_b` | Kernel version |
| Modules      | In rootfs `/lib/modules/` | Kernel version |
| GPU firmware | In rootfs `/lib/firmware/` | nvgpu version  |

**Safe Update Flow**

```
1. Currently running Slot A (kernel 5.10.104-tegra)
2. Prepare Slot B:
   a. Write new kernel Image to kernel_b partition
   b. Write new DTB to kernel-dtb_b partition
   c. Write new rootfs (with matching modules + firmware) to APP_b
3. Set Slot B active
4. Reboot
5. Validate (GPU loads, camera works, inference runs)
6. Mark Slot B successful
```

See [Rootfs & A/B Redundancy Guide](../12-Orin-Nano-Rootfs与AB冗余/Guide.md) for full OTA details.

**Never Do This**

* Update only the kernel without updating modules → `modprobe` failures
* Update modules without updating the kernel → version mismatch panics
* Use `apt upgrade` to update kernel packages → breaks A/B invariant
* Mix JetPack versions across slots → driver/firmware incompatibility

---

**18. Common Kernel Issues and Solutions**

**GPU Driver Fails to Load**

```
nvgpu: probe of gpu.0 failed with error -110
```

**Causes:**
* GPU firmware missing from `/lib/firmware/nvidia/`
* Power domain not enabled in DTB
* BPMP firmware version mismatch

**Solution:** Verify firmware files exist, check `dmesg | grep bpmp`, ensure DTB matches your JetPack version.

**Camera Not Detected**

```
tegra-vi: no channels found
```

**Causes:**
* Camera device tree node missing or `status = "disabled"`
* I2C address wrong in DTB
* CSI lane configuration mismatch
* Sensor driver not loaded

**Solution:** Check `media-ctl -p`, verify DTB camera nodes, check `i2cdetect` for sensor presence.

**Module Version Mismatch**

```
modprobe: FATAL: Module nvgpu not found in directory /lib/modules/5.10.104-tegra
```

**Cause:** Kernel version does not match installed modules.

**Solution:** Rebuild and install modules matching the running kernel, or reflash with matching kernel + rootfs.

**Kernel OOM During Inference**

```
Out of memory: Killed process 1234 (python3)
```

**Cause:** Combined CUDA + CPU + camera memory exceeds available RAM.

**Solution:** Quantize models (INT8), reduce camera buffer count, use DLA offload, monitor with `tegrastats`. See [Memory Architecture Guide](../07-Orin-Nano内存架构/Guide.md).

</details>

### 启动慢

**原因：** 默认 kernel 配置启用了所有驱动与调试。

**解决办法：** 参照第 13 节的优化 —— 禁用未使用的 DT 节点、将驱动模块化、关闭调试、抑制 UART console 输出。

---

## 19. 参考资料

* [NVIDIA Jetson Linux — Kernel](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Kernel/Kernel.html) — 官方 kernel 文档
* [NVIDIA Jetson Linux — Kernel Customization](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Kernel/KernelCustomization.html) — 定制指南
* [NVIDIA Jetson Linux — Kernel Boot Time Optimization](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Kernel/KernelBootTimeOptimization.html) — 启动优化参考
* [NVIDIA Jetson Linux — Device Tree](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Kernel/KernelAdaptation.html) — 设备树适配
* [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/) — 上游 kernel 文档
* [ARM64 Booting](https://www.kernel.org/doc/html/latest/arm64/booting.html) — ARM64 启动协议
* 主指南：[Nvidia Jetson Platform Guide](../Guide.md)
* 内存深入解析：[Orin Nano Memory Architecture](../07-Orin-Nano内存架构/Guide.md)
* rootfs 与 OTA：[Orin Nano Rootfs & A/B Redundancy](../12-Orin-Nano-Rootfs与AB冗余/Guide.md)


<details>
<summary>English original</summary>

**Slow Boot**

**Cause:** Default kernel config with all drivers and debug enabled.

**Solution:** Follow Section 13 optimizations — disable unused DT nodes, modularize drivers, disable debug, suppress UART console.

---

**19. References**

* [NVIDIA Jetson Linux — Kernel](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Kernel/Kernel.html) — official kernel documentation
* [NVIDIA Jetson Linux — Kernel Customization](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Kernel/KernelCustomization.html) — customization guide
* [NVIDIA Jetson Linux — Kernel Boot Time Optimization](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Kernel/KernelBootTimeOptimization.html) — boot optimization reference
* [NVIDIA Jetson Linux — Device Tree](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Kernel/KernelAdaptation.html) — device tree adaptation
* [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/) — upstream kernel docs
* [ARM64 Booting](https://www.kernel.org/doc/html/latest/arm64/booting.html) — ARM64 boot protocol
* Main guide: [Nvidia Jetson Platform Guide](../Guide.md)
* Memory deep dive: [Orin Nano Memory Architecture](../07-Orin-Nano内存架构/Guide.md)
* Rootfs & OTA: [Orin Nano Rootfs & A/B Redundancy](../12-Orin-Nano-Rootfs与AB冗余/Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/1. Nvidia Jetson Platform/Orin-Nano-Kernel-Internals/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/1.%20Nvidia%20Jetson%20Platform/Orin-Nano-Kernel-Internals/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
