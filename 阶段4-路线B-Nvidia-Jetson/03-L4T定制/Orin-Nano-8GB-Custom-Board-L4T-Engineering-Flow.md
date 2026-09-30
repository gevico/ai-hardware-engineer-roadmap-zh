---
title: Orin Nano 8GB — 定制载板 L4T 工程流程
description: Orin Nano 8GB — 定制载板 L4T 工程流程
---

# Orin Nano 8GB — 定制载板 L4T 工程流程

**阶段 4 — 方向 B — Nvidia Jetson** · L4T 定制模块

本页是一份**具体的分步流程图**，用于在 **Jetson Orin Nano 8GB** 周围的**定制板**上将 **Jetson Linux (L4T)** bring-up（上电点亮/调通）：每个阶段的输入、过程和输出。从 [NVIDIA Jetson documentation](https://docs.nvidia.com/jetson/) 为你的量产基线锁定**确切**的 JetPack / L4T 构建和 tarball 名称——下面的示例发布标签仅作说明。

**另见：** [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) (官方风格适配)、[T23x-Deployment.md](T23x-Deployment.md) (BCT / MB1)、[ODMDATA-and-GPIO-Jetson-Linux.md](ODMDATA-and-GPIO-Jetson-Linux.md) (UPHY / GPIO layers)。

---

## 开始前（范围检查）

| 主题 | 说明 |
|--------|------|
| **参考设计** | 从与你的 **SoC + module SKU** 匹配的 **NVIDIA Orin Nano devkit** 或 **模块 + 载板**文档开始；复制**最接近的** DT / flash 配置，然后针对你的原理图做差异调整。 |
| **刷写目标** | **`mmcblk0p1`** 适用于许多 **eMMC/SD** 流程；定制载板上的**外部 NVMe/USB** 通常使用 **`l4t_initrd_flash.sh`** 和一个**不同的**板级/存储参数——针对你的存储拓扑匹配 **Module Adaptation** 指南。 |
| **`flash.sh` 板名** | 第二个参数是**配置的板名**（见 `Linux_for_Tegra/*.conf`），不是自由形式的字符串。适配后，你可以使用由 `BOARD` / `.conf` 对定义的**自定义**名称。 |
| **设备树工作流** | 将 **`.dtb` → `.dts`** 反编译对于**学习和快速差异对比**是有效的；**生产**通常编辑 BSP 的 kernel 源码树下的 **kernel DTS 源码**，重新构建 **`Image` + DTBs**，并按照 NVIDIA 的 kernel 定制指南将产物安装到 **`Linux_for_Tegra/`**。 |

---

## 步骤 1：准备主机环境

| 步骤 | 输入 | 过程 | 输出 / 结果 |
|------|--------|---------|------------------|
| 安装 Ubuntu 与工具 | 干净的 **Ubuntu 22.04** 主机（JetPack 6.x 主机刷写的典型配置） | 安装 `build-essential`、`device-tree-compiler`（`dtc`）、`git`、`python3`、`libncurses-dev`，以及 NVIDIA 为你的 L4T 版本列出的任何软件包 | 主机可以编译 kernel、DTB，并运行刷写脚本 |
| 创建工作区 | 目录路径 | `mkdir -p ~/jetson-l4t-custom`（或你的标准路径） | 用于 BSP、源码和构建树的工作区文件夹 |

---

## 步骤 2：下载 L4T BSP 与源码

| 步骤 | 输入 | 过程 | 输出 / 结果 |
|------|--------|---------|------------------|
| 下载 L4T | 目标 **JetPack / L4T**（例如：**JetPack 6.2.1 / L4T 36.4.4**） | 从该版本的 **NVIDIA Jetson** 开发者 / 嵌入式页面下载 | 诸如 `Jetson_Linux_R36.4.4_aarch64.tbz2`、`Tegra_Linux_Sample-Root-Filesystem_R36.4.4_aarch64.tbz2` 之类的归档文件，以及 **public_sources** / kernel 源码 tarball（名称因版本略有不同——请使用对应版本的页面） |
| 解压 BSP | L4T 驱动包归档文件 | `tar -xf Jetson_Linux_R*_aarch64.tbz2` | **`Linux_for_Tegra/`**，包含刷写脚本、预构建 kernel 产物、sample rootfs 暂存路径 |
| 解压 kernel 源码 | 同一发布线的 kernel / 源码 tarball | 按该 JetPack 的 **NVIDIA kernel 定制**说明进行解压 | kernel 源码树已就绪，可进行补丁、配置和构建（`ARCH=arm64`） |

---

## 步骤 3：准备设备树（DTB）

| 步骤 | 输入 | 过程 | 输出 / 结果 |
|------|--------|---------|------------------|
| 选择参考 DTB | 用于 Orin Nano + 你的起始载板的 **基线** DTB（来自解压后的 BSP / 预构建产物，或来自首次 kernel 构建） | 将 **最接近的** `.dtb` 复制到工作文件夹 | 可用于对比的已知良好二进制文件 |
| 转换 DTB → DTS | `.dtb` 文件 | `dtc -I dtb -O dts -o custom_board.dts <reference>.dtb` | 可编辑的 `.dts` 供审阅（可选；发布时优先使用源码树中的源 DTS） |
| 编辑 DTS | 板级原理图、网络名、稳压器、I2C/SPI/UART、GPIO、PCIe/USB/CSI | 与模块适配指南的 **pinmux / 移植**部分保持一致；使 **ODMDATA / UPHY** 与物理连线保持一致 | 描述你的硬件的 `custom_board.dts`（或 kernel `*.dts` 补丁） |
| 编译 DTS → DTB | 编辑后的 `.dts` | `dtc -I dts -O dtb -o custom_board.dtb custom_board.dts`（和/或 kernel `make dtbs`） | `.dtb`，可安装到 **`Linux_for_Tegra/kernel/dtb/`** 下（或你的 flash 配置期望的路径） |

---


<details>
<summary>English original</summary>

**Orin Nano 8GB — custom carrier L4T engineering flow**

**Phase 4 — Track B — Nvidia Jetson** · L4T customization module

This page is a **concrete step-by-step flowchart** for bringing **Jetson Linux (L4T)** up on a **custom board** around **Jetson Orin Nano 8GB**: inputs, process, and outputs per stage. Pin **exact** JetPack / L4T builds and tarball names from [NVIDIA Jetson documentation](https://docs.nvidia.com/jetson/) for your ship baseline—the example release tags below are illustrative.

**See also:** [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) (official-style adaptation), [T23x-Deployment.md](T23x-Deployment.md) (BCT / MB1), [ODMDATA-and-GPIO-Jetson-Linux.md](ODMDATA-and-GPIO-Jetson-Linux.md) (UPHY / GPIO layers).

---

**Before you start (scope check)**

| Topic | Note |
|--------|------|
| **Reference design** | Start from the **NVIDIA Orin Nano devkit** or **module + carrier** documentation that matches your **SoC + module SKU**; copy the **closest** DT / flash config, then delta for your schematic. |
| **Flash target** | **`mmcblk0p1`** applies to many **eMMC/SD** flows; **external NVMe/USB** on a custom carrier often uses **`l4t_initrd_flash.sh`** and a **different** board/storage argument—match the **Module Adaptation** guide for your storage topology. |
| **`flash.sh` board name** | The second argument is a **configured board name** (see `Linux_for_Tegra/*.conf`), not a free-form string. After adaptation you may use a **custom** name defined by your `BOARD` / `.conf` pair. |
| **Device tree workflow** | Decompiling a **`.dtb` → `.dts`** is valid for **learning and quick diffs**; **production** usually edits **kernel DTS sources** under the BSP’s kernel tree, rebuilds **`Image` + DTBs**, and installs artifacts into **`Linux_for_Tegra/`** per NVIDIA’s kernel customization guide. |

---

**Step 1: Prepare host environment**

| Step | Input | Process | Output / result |
|------|--------|---------|------------------|
| Install Ubuntu & tools | Clean **Ubuntu 22.04** host (typical for JetPack 6.x host flashing) | Install `build-essential`, `device-tree-compiler` (`dtc`), `git`, `python3`, `libncurses-dev`, and any packages NVIDIA lists for your L4T release | Host can compile kernel, DTB, and run flash scripts |
| Create workspace | Directory path | `mkdir -p ~/jetson-l4t-custom` (or your standard) | Workspace folder for BSP, sources, and build trees |

---

**Step 2: Download L4T BSP and sources**

| Step | Input | Process | Output / result |
|------|--------|---------|------------------|
| Download L4T | Target **JetPack / L4T** (example: **JetPack 6.2.1 / L4T 36.4.4**) | Download from the **NVIDIA Jetson** developer / embedded pages for that release | Archives such as `Jetson_Linux_R36.4.4_aarch64.tbz2`, `Tegra_Linux_Sample-Root-Filesystem_R36.4.4_aarch64.tbz2`, and **public_sources** / kernel source tarballs (names vary slightly by release—use the page for your version) |
| Extract BSP | L4T driver package archive | `tar -xf Jetson_Linux_R*_aarch64.tbz2` | **`Linux_for_Tegra/`** with flash scripts, prebuilt kernel artifacts, sample rootfs staging paths |
| Extract kernel sources | Kernel / sources tarball from the same release line | Extract per **NVIDIA kernel customization** instructions for that JetPack | Kernel tree ready to patch, configure, and build (`ARCH=arm64`) |

---

**Step 3: Prepare device tree (DTB)**

| Step | Input | Process | Output / result |
|------|--------|---------|------------------|
| Select reference DTB | **Baseline** DTB for Orin Nano + your starting carrier (from unpacked BSP / prebuilts, or from a first kernel build) | Copy the **closest** `.dtb` into a working folder | Known-good binary to diff against |
| Convert DTB → DTS | `.dtb` file | `dtc -I dtb -O dts -o custom_board.dts <reference>.dtb` | Editable `.dts` for review (optional; prefer source DTS in tree for shipping) |
| Edit DTS | Board schematic, net names, regulators, I2C/SPI/UART, GPIO, PCIe/USB/CSI | Align with **pinmux / porting** sections of the module adaptation guide; keep **ODMDATA / UPHY** consistent with physical wiring | `custom_board.dts` (or kernel `*.dts` patches) describing your hardware |
| Compile DTS → DTB | Edited `.dts` | `dtc -I dts -O dtb -o custom_board.dtb custom_board.dts` (and/or kernel `make dtbs`) | `.dtb` ready to install under **`Linux_for_Tegra/kernel/dtb/`** (or the path your flash config expects) |

---

</details>

## 步骤 4：kernel 配置与构建

| 步骤 | 输入 | 过程 | 输出 / 结果 |
|------|--------|---------|------------------|
| 配置 kernel | kernel 源码、来自 NVIDIA 的 SoC 系列 `defconfig` | `make ARCH=arm64 CROSS_COMPILE=... O=build tegra_defconfig`（具体的 `defconfig` 名称随版本而异） | 与 Jetson 对齐的基础配置 |
| 启用驱动 | 所需外设 | `make ARCH=arm64 O=build menuconfig`（或 `nconfig`） | 含存储、网络、传感器、摄像头等驱动的配置 |
| 编译 kernel | 已配置的源码树 | `make ARCH=arm64 O=build -j"$(nproc)"`（若使用树外模块，还需 `modules` / `modules_install`） | `Image`、`modules`，以及按源码树布局构建出的 **DTBs** |
| 安装到 `Linux_for_Tegra` | 构建好的 `Image`、DTBs、模块 | 按 NVIDIA 指南复制或 `install` 到 **`Linux_for_Tegra/kernel/`**（若模块要放到目标板上，则还需放入 rootfs） | 烧录步骤会采用你的 kernel + DTB |

---

## 步骤 5：定制根文件系统

| 步骤 | 输入 | 过程 | 输出 / 结果 |
|------|--------|---------|------------------|
| 解压 rootfs | 示例 rootfs 归档包 | `sudo tar -xpf Tegra_Linux_Sample-Root-Filesystem_R*_aarch64.tbz2 -C rootfs` | 可编辑的、基于 Ubuntu 的 rootfs |
| 安装软件包 | 产品依赖项 | `chroot` + `qemu-aarch64-static`（交叉）或按指南使用本机构建主机流程；`apt install` | 含库与应用的 rootfs |
| 可选重新打包 | 修改后的源码树 | `sudo tar -cJpf rootfs_custom.tbz2 -C rootfs .` | 烧录前由 **`Linux_for_Tegra/rootfs`** 指向的自定义归档包 |

---

## 步骤 6：烧录 / 启动

| 步骤 | 输入 | 过程 | 输出 / 结果 |
|------|--------|---------|------------------|
| 烧录到 SD / eMMC | BSP、kernel、DTB、rootfs | `sudo ./flash.sh <board_name> <storage>` — 示例模式：**internal** eMMC/SD 启动目标通常形如 `mmcblk0p1`；请对照你板子的 `flash.xml` / conf **核实** | 带 Linux + 你的 DTB/kernel 的可启动介质 |
| Initrd / 外部存储 | 自定义载板上的 NVMe / USB 启动 | 通常配合板级专用 env 执行 **`./tools/kernel_flash/l4t_initrd_flash.sh`**（见适配指南） | 镜像写入正确的设备 |
| 串口控制台 | 调试排针上的 USB–UART | 按载板文档给出的波特率进行监视 | 用于 bring-up（上电点亮/调通）的启动日志、UEFI / CBoot / kernel 消息 |

---

## 步骤 7：外设测试与调试

| 步骤 | 输入 | 过程 | 输出 / 结果 |
|------|--------|---------|------------------|
| 最小启动 | kernel + DTB + rootfs | 上电 | 通过串口 / SSH 进入 shell 或登录 |
| 总线 / GPIO 冒烟测试 | 已连接的硬件 | `i2cdetect`、`spidev` 测试、UART 回环、JetPack 6 上的 **libgpiod** / line 名称（新工作不要依赖已废弃的 sysfs GPIO） | 电气 + 驱动通路已确认 |
| 摄像头 / USB / PCIe | 模块与线缆 | `dmesg`、`v4l2`、应用测试 | 集成已验证 |
| 调试 | 日志、设备树、regulator 报错 | 调整 DTS、kernel 配置或 BCT / pinmux；迭代步骤 4–6 | 稳定的板级支持 |

---

## 步骤 8：迭代与版本管理

| 步骤 | 输入 | 过程 | 输出 / 结果 |
|------|--------|---------|------------------|
| 版本控制 | DTS、kernel `defconfig` 片段、补丁、烧录配置 | 在 tag 或 README 中带 **L4T version** 的 `git` 提交 | 可复现的基线与回滚 |
| 增量测试 | 小幅改动 | 重复 kernel/DTB/烧录循环 | 面向你的 Orin Nano 8GB 自定义板的量产就绪镜像 |

---

## 单行依赖图

```text
Host + BSP ──► kernel/DTS work ──► install to Linux_for_Tegra ──► rootfs deltas ──► flash.sh / initrd flash ──► UART + tests ──► git tag
```

---

## 仓库内相关资料

- **[Guide.md](Guide.md)** — L4T 模块概览与 EchoPilot / NVMe 参考。
- **[Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md)** — 板卡命名、`l4t_initrd_flash.sh`、pinmux、DT 移植。
- **[T23x-Deployment.md](T23x-Deployment.md)** — 当烧录或启动固件行为需要改变时的 MB1/MB2 BCT 表。


<details>
<summary>English original</summary>

**Step 4: Kernel configuration and build**

| Step | Input | Process | Output / result |
|------|--------|---------|------------------|
| Configure kernel | Kernel sources, SoC family `defconfig` from NVIDIA | `make ARCH=arm64 CROSS_COMPILE=... O=build tegra_defconfig` (exact `defconfig` name is release-specific) | Base config aligned with Jetson |
| Enable drivers | Required peripherals | `make ARCH=arm64 O=build menuconfig` (or `nconfig`) | Config with drivers for storage, networking, sensors, cameras, etc. |
| Compile kernel | Configured tree | `make ARCH=arm64 O=build -j"$(nproc)"` (plus `modules` / `modules_install` if you use out-of-tree modules) | `Image`, `modules`, and built **DTBs** per tree layout |
| Install into `Linux_for_Tegra` | Built `Image`, DTBs, modules | Copy or `install` per NVIDIA guide into **`Linux_for_Tegra/kernel/`** (and rootfs if modules go on target) | Flash step picks up your kernel + DTB |

---

**Step 5: Customize root filesystem**

| Step | Input | Process | Output / result |
|------|--------|---------|------------------|
| Extract rootfs | Sample rootfs archive | `sudo tar -xpf Tegra_Linux_Sample-Root-Filesystem_R*_aarch64.tbz2 -C rootfs` | Editable Ubuntu-based rootfs |
| Install packages | Product dependencies | `chroot` + `qemu-aarch64-static` (cross) or native build host workflow per guide; `apt install` | Rootfs with libraries and apps |
| Optional repack | Modified tree | `sudo tar -cJpf rootfs_custom.tbz2 -C rootfs .` | Custom archive you point **`Linux_for_Tegra/rootfs`** at before flash |

---

**Step 6: Flash / boot**

| Step | Input | Process | Output / result |
|------|--------|---------|------------------|
| Flash to SD / eMMC | BSP, kernel, DTB, rootfs | `sudo ./flash.sh <board_name> <storage>` — example pattern: **internal** eMMC/SD boot targets often look like `mmcblk0p1`; **verify** against your board’s `flash.xml` / conf | Bootable media with Linux + your DTB/kernel |
| Initrd / external storage | NVMe / USB boot on custom carrier | Often **`./tools/kernel_flash/l4t_initrd_flash.sh`** with board-specific env (see adaptation guide) | Image written to the correct device |
| Serial console | USB–UART to debug header | Monitor at the baud rate in your carrier docs | Boot logs, UEFI / CBoot / kernel messages for bring-up |

---

**Step 7: Test and debug peripherals**

| Step | Input | Process | Output / result |
|------|--------|---------|------------------|
| Minimal boot | Kernel + DTB + rootfs | Power on | Shell or login over serial / SSH |
| Bus / GPIO smoke tests | Connected hardware | `i2cdetect`, `spidev` tests, UART loopback, **libgpiod** / line names on JetPack 6 (avoid relying on deprecated sysfs GPIO for new work) | Confirmed electrical + driver path |
| Camera / USB / PCIe | Modules and cables | `dmesg`, `v4l2`, application tests | Integration verified |
| Debug | Logs, device tree, regulator errors | Adjust DTS, kernel config, or BCT / pinmux; iterate Steps 4–6 | Stable board support |

---

**Step 8: Iteration and versioning**

| Step | Input | Process | Output / result |
|------|--------|---------|------------------|
| Version control | DTS, kernel `defconfig` fragments, patches, flash config | `git` commits with **L4T version** in tag or README | Reproducible baselines and rollback |
| Incremental testing | Small deltas | Repeat kernel/DTB/flash cycle | Production-ready image for your Orin Nano 8GB custom board |

---

**One-line dependency graph**

```text
Host + BSP ──► kernel/DTS work ──► install to Linux_for_Tegra ──► rootfs deltas ──► flash.sh / initrd flash ──► UART + tests ──► git tag
```

---

**Related in-repo material**

- **[Guide.md](Guide.md)** — L4T module overview and EchoPilot / NVMe reference.
- **[Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md)** — board naming, `l4t_initrd_flash.sh`, pinmux, DT porting.
- **[T23x-Deployment.md](T23x-Deployment.md)** — MB1/MB2 BCT tables when flash or boot firmware behavior must change.

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/3. L4T Customization/Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/3.%20L4T%20Customization/Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
