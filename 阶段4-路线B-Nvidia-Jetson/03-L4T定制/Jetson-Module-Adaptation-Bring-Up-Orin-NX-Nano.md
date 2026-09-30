---
title: Jetson 模块适配与 bring-up（上电点亮/调通）（Orin NX / Orin Nano）
description: Jetson 模块适配与 bring-up（上电点亮/调通）（Orin NX / Orin Nano）
---

# Jetson 模块适配与 bring-up（上电点亮/调通）（Orin NX / Orin Nano）

**来源：** NVIDIA *Jetson Linux Developer Guide* — *Jetson Module Adaptation and Bring-Up* → **Jetson Orin NX and Nano Series**（抓取快照，上游最后更新于 2025 年 2 月）。本文件为路线图重新格式化。请根据你的 **JetPack / L4T** 版本，对照 [Jetson 文档](https://docs.nvidia.com/jetson/) 确认细节。

**配套参考：** **[T23x-Deployment.md](T23x-Deployment.md)** — T23x **BCT**（MB1/MB2 启动配置表，DTS 中的 pinmux/prod/pad 电压/PMIC）。当本指南指向 **BCT**、**MB1**、**`tegrabct_v2`** 或 **`bootloader/generic/BCT`** 时使用它。

**阅读提示**

- 以下章节标题是 markdown 标题；使用编辑器大纲跳转。
- 从 HTML 恢复的列表和表格可能仍有缺口——关键数字请对照 NVIDIA 的 PDF/HTML 核实。
- 以 `$` 开头的 shell 命令行，与原文档一样，用于 **host** 或 **Jetson**。

**如何将其用作课程**

1. **背景** — [板配置](#board-configuration) 和 [板命名](#naming-the-board)：开发套件与你的载板分别是什么。
2. **契约** — [占位符](#placeholders-in-the-porting-instructions) 和 [rootfs 配置](#root-filesystem-configuration)：`<board>` 的含义以及 NVIDIA 对 rootfs 的期望。
3. **早期启动（MB1）** — [MB1 配置变更](#mb1-configuration-changes)：pinmux 表格 → `.dtsi`，GPIO 编号，可选 I2C/DP-AUX；与 **[T23x-Deployment.md](T23x-Deployment.md)** 配合以了解 BCT 布局。
4. **载板特定** — [EEPROM 修改](#eeprom-modifications)（如果你的载板没有 EEPROM）。
5. **Linux DT** — [移植 Linux 内核设备树](#porting-the-linux-kernel-device-tree)：`nv-public` 与 `nv-platform`，DTB 部署，overlay。
6. **高速 I/O** — [PCIe](#configuring-the-pcie-controller)，[USB](#porting-universal-serial-bus)，[UPHY / ODMDATA](#uphy-lane-configuration)。
7. **交付** — [刷写构建镜像](#flashing-the-build-image)，[可选刷写环境变量](#setting-optional-environmental-variables)，[HDMI](#hdmi-support)（如适用）。

**本文档中的代码**

- 围栏 **`dts`** / **`bash`** / **`diff`** 块在有助于理解之处添加；长摘录仍遵循 NVIDIA 指南的换行。
- **`diff`** 在源文件中可能跨页拆分——请对照你实际的 `Linux_for_Tegra` 树核实各代码块。

## 目录

1. [板配置](#board-configuration)
2. [板命名](#naming-the-board)
3. [移植说明中的占位符](#placeholders-in-the-porting-instructions)
4. [rootfs 配置](#root-filesystem-configuration)
5. [MB1 配置变更](#mb1-configuration-changes)（pinmux、GPIO 映射、I2C/DP1_AUX、动态 GPIO）
6. [EEPROM 修改](#eeprom-modifications)
7. [移植 Linux 内核设备树](#porting-the-linux-kernel-device-tree)
8. [配置 PCIe 控制器](#configuring-the-pcie-controller)
9. [移植 USB](#porting-universal-serial-bus)
10. [UPHY lane 配置](#uphy-lane-configuration)
11. [刷写构建镜像](#flashing-the-build-image)
12. [设置可选环境变量](#setting-optional-environmental-variables)
13. [HDMI 支持](#hdmi-support)

---

本指南说明如何使用 **Jetson Linux**（L4T）驱动包将 NVIDIA **Jetson Orin NX** 和 **Orin Nano** 模块 **适配** 到 **自定义载板**：板命名、MB1/MB2 相关配置、设备树、PCIe/USB/UPHY 以及刷写。

示例以 **Orin Nano Developer Kit** 套件（P3767 SOM + P3768 载板）为参考；你的文件名和 `.conf` 条目会随 **`<board>`** 和载板设计而变化。关于 **MB1 BCT** 结构（pinmux/prod/pmic 作为 `.dtsi`），请使用 **[T23x-Deployment.md](T23x-Deployment.md)**（DU-10990-001）。

## 板配置

Jetson Orin Nano Developer Kit 由连接到 P3768 载板的 P3767 系统级模块（SOM）组成。部件号 P3766 表示完整的 Jetson Orin Nano Developer Kit。SOM 和载板各有一个 EEPROM，其中保存了板 ID。Developer Kit 无需任何软件配置修改即可使用。

在将 SOM 与 P3768 之外的载板一起使用之前，你必须首先更改 kernel 设备树、MB1 配置、MB2 配置、ODM 数据和刷写配置，以对应新的载板。下一节提供了有关这些更改的更多信息。

## 板命名

为 **SOM + 载板** 对选择一个 **小写字母数字** 名称。允许使用连字符（`-`）和下划线（`_`）；**不允许使用空格**。

示例：

- `p3768-0000-devkit`
- `devboard`

该字符串会出现在 **文件路径**、**设备树名称** 以及一些 **`/proc`** 可见字符串中。在本主题中，**`<board>`** 表示你选择的名称。

以相同方式选择一个 **`<vendor>`** 字符串（例如 `nvidia`）。


<details>
<summary>English original</summary>

**Jetson module adaptation and bring-up (Orin NX / Orin Nano)**

**Source:** NVIDIA *Jetson Linux Developer Guide* — *Jetson Module Adaptation and Bring-Up* → **Jetson Orin NX and Nano Series** (scraped snapshot, last updated Feb 2025 in upstream). This file is reformatted for the roadmap. Confirm details against the [Jetson documentation](https://docs.nvidia.com/jetson/) for your **JetPack / L4T** version.

**Paired reference:** **[T23x-Deployment.md](T23x-Deployment.md)** — T23x **BCT** (MB1/MB2 boot configuration tables, pinmux/prod/pad voltage/PMIC in DTS). Use it when this guide points at **BCT**, **MB1**, **`tegrabct_v2`**, or **`bootloader/generic/BCT`**.

**Reading tips**

- Section titles below are markdown headings; use your editor outline to jump.
- Lists and tables recovered from HTML may still have gaps—verify critical numbers against NVIDIA’s PDF/HTML.
- Commands shown as shell lines starting with `$` are for the **host** or **Jetson** as in the original doc.

**How to use this as a course**

1. **Context** — [Board configuration](#board-configuration) and [Naming the board](#naming-the-board): what the dev kit is vs your carrier.
2. **Contracts** — [Placeholders](#placeholders-in-the-porting-instructions) and [Root filesystem configuration](#root-filesystem-configuration): what `<board>` means and what NVIDIA expects in rootfs.
3. **Early boot (MB1)** — [MB1 configuration changes](#mb1-configuration-changes): pinmux spreadsheet → `.dtsi`, GPIO numbering, optional I2C/DP-AUX; pair with **[T23x-Deployment.md](T23x-Deployment.md)** for BCT layout.
4. **Carrier-specific** — [EEPROM modifications](#eeprom-modifications) if you have no carrier EEPROM.
5. **Linux DT** — [Porting the Linux kernel device tree](#porting-the-linux-kernel-device-tree): `nv-public` vs `nv-platform`, DTB deploy, overlays.
6. **High-speed I/O** — [PCIe](#configuring-the-pcie-controller), [USB](#porting-universal-serial-bus), [UPHY / ODMDATA](#uphy-lane-configuration).
7. **Ship** — [Flashing the build image](#flashing-the-build-image), [optional flash env vars](#setting-optional-environmental-variables), [HDMI](#hdmi-support) if applicable.

**Code in this document**

- Fenced **`dts`** / **`bash`** / **`diff`** blocks are added where it helps; long excerpts still follow the NVIDIA guide’s line breaks.
- A **`diff`** may have been split across pages in the source—verify hunks against your real `Linux_for_Tegra` tree.

**Table of contents**

1. [Board configuration](#board-configuration)
2. [Naming the board](#naming-the-board)
3. [Placeholders in the porting instructions](#placeholders-in-the-porting-instructions)
4. [Root filesystem configuration](#root-filesystem-configuration)
5. [MB1 configuration changes](#mb1-configuration-changes) (pinmux, GPIO mapping, I2C/DP1_AUX, dynamic GPIO)
6. [EEPROM modifications](#eeprom-modifications)
7. [Porting the Linux kernel device tree](#porting-the-linux-kernel-device-tree)
8. [Configuring the PCIe controller](#configuring-the-pcie-controller)
9. [Porting Universal Serial Bus](#porting-universal-serial-bus)
10. [UPHY lane configuration](#uphy-lane-configuration)
11. [Flashing the build image](#flashing-the-build-image)
12. [Setting optional environmental variables](#setting-optional-environmental-variables)
13. [HDMI support](#hdmi-support)

---

This guide explains how to **adapt** NVIDIA **Jetson Orin NX** and **Orin Nano** modules to a **custom carrier** using the **Jetson Linux** (L4T) driver package: board naming, MB1/MB2-related configuration, device tree, PCIe/USB/UPHY, and flashing.

Examples target the **Orin Nano Developer Kit** stack (P3767 SOM + P3768 carrier) as a reference; your filenames and `.conf` entries change with **`<board>`** and carrier design. For **MB1 BCT** structure (pinmux/prod/pmic as `.dtsi`), use **[T23x-Deployment.md](T23x-Deployment.md)** (DU-10990-001).

**Board Configuration**

The Jetson Orin Nano Developer Kit consists of a P3767 System on Module (SOM) that is connected to a P3768 carrier board. Part number P3766 designates the complete Jetson Orin Nano Developer Kit. The SOM and carrier board each have an EEPROM where the board ID is saved. The Developer Kit can be used without any software configuration modifications.

Before you use the SOM with a carrier board other than the P3768, you must first change the kernel device tree, the MB1 configuration, the MB2 configuration, the ODM data, and the flashing configuration to correspond to the new carrier board. The next section provides more information about the changes.

**Naming the Board**

Pick a **lowercase alphanumeric** name for the **SOM + carrier** pair. Hyphens (`-`) and underscores (`_`) are allowed; **spaces are not**.

Examples:

- `p3768-0000-devkit`
- `devboard`

That string shows up in **file paths**, **device tree names**, and some **`/proc`**-visible strings. In this topic, **`<board>`** means the name you chose.

Pick a **`<vendor>`** string the same way (e.g. `nvidia`).

</details>

## 移植说明中的占位符

在路径或代码片段中看到占位符时，替换为真实取值。

| 占位符 | 含义 |
|-------------|---------|
| **`<function>`** | 功能域：例如 power-tree、pinmux、sdmmc-drv、keys、comm（Wi-Fi/Bluetooth®）、camera、…… |
| **`<board>`** | 平台名称（小写；参见 [Naming the board](#naming-the-board)）。参考载板使用开发套件中类似 P3768-side 的命名。 |
| **`<version>`** | 板卡版本字符串（例如 `a00`）。NVIDIA 参考代码树通常包含它；定制板卡可以省略。 |
| **`<vendor>`** | 用于文件命名的组织或厂商标识。 |

## rootfs 配置

Jetson Linux 可以使用**标准或定制** rootfs，但 NVIDIA 期望包含特定的**启动时集成**部分。若这些缺失，图形、时钟或电源配置的表现可能与参考套件不同。

通常需要对以下内容做对齐：

- 诸如 **`nv.sh`** / **`nvfb.sh`** 的脚本（kernel 路径中的平台初始化）。
- 若使用桌面环境，则需 **Xorg** / X 栈（无头设计可省略此项）。
- 目标平台的 **`nvpmodel`** 时钟与频率策略。

参考 **rootfs hooks** 位于：

`Linux_for_Tegra/nv_tegra/`（及其子目录）

将适用的部分合并进**你的** rootfs。对于 NVIDIA 的示例 Ubuntu rootfs，在解包示例文件系统后运行 **`Linux_for_Tegra/apply_binaries.sh`**，使 GPU 驱动和 NVIDIA 文件落到代码树中。

## MB1 配置变更

在 **T234** 上，**MB1** 读取多个 **`.dts` / `.dtsi`** 片段（pinmux、pad 电压、PMIC、storage、UPHY、……），这些片段编译进 **BCT** 二进制。这些源文件通常位于：

`<l4t_top>/bootloader/generic/BCT/`

关于**字段级 BCT** 参考（每个片段可包含的内容），请使用 **[T23x-Deployment.md](T23x-Deployment.md)**（DU-10990-001）。

### 生成 pinmux dtsi 文件

使用 NVIDIA 的 **Orin NX / Orin Nano pinmux** 表格（来自 [Jetson / embedded downloads](https://developer.nvidia.com/embedded/downloads)）来定义 **SFIO 与 GPIO**、上下拉及相关选项。该表格记录了每个引脚的 **ball 位置**和**设备树**视图。

**表格检查清单**

1. 出现提示时启用**宏**。
2. 保持 **Pin Direction** 与功能一致（例如 **I2C** 的时钟/数据为**双向**）。
3. 使 **Req Initial State** 与该方向匹配（例如仅在输出或双向适用处填 **Drive 0/1**）。
4. 仅在表格允许处使用 **3.3V Tolerance**；启用后会在适用场景下使该引脚变为**开漏**。
5. 编辑完成后，在表格中使用 **Generate DT File**。

典型输出（具体名称取决于表格的输入）：

- `pinmux.dtsi`
- `gpio.dtsi`
- `padvoltage.dtsi`

**安装路径**

- 将 **`pinmux.dtsi`** 和 **`padvoltage.dtsi`** 复制到 `<l4t_top>/bootloader/generic/BCT/`
- 将 **`gpio.dtsi`** 复制到 `<l4t_top>/bootloader/`

将 **board `.conf`** 指向这些文件（参见 [Flashing the build image](#flashing-the-build-image)）。


<details>
<summary>English original</summary>

**Placeholders in the Porting Instructions**

When you see placeholders in paths or snippets, substitute your real values.

| Placeholder | Meaning |
|-------------|---------|
| **`<function>`** | Functional area: e.g. power-tree, pinmux, sdmmc-drv, keys, comm (Wi-Fi/Bluetooth®), camera, … |
| **`<board>`** | Your platform name (lowercase; see [Naming the board](#naming-the-board)). Reference carriers use names like the P3768-side naming in the dev kit. |
| **`<version>`** | Board version string (e.g. `a00`). NVIDIA reference trees often include it; custom boards may omit it. |
| **`<vendor>`** | Your org or vendor tag for file naming. |

**Root Filesystem Configuration**

Jetson Linux can use a **standard or custom** rootfs, but NVIDIA expects certain **boot-time integration** pieces. If those are missing, graphics, clocks, or power profiles may not behave as on the reference kit.

Typically you need alignment for:

- Scripts such as **`nv.sh`** / **`nvfb.sh`** (platform setup in the kernel path).
- **Xorg** / X stack if you use a desktop (headless designs may drop this).
- **`nvpmodel`** clock and frequency policy for the target.

Reference **rootfs hooks** live under:

`Linux_for_Tegra/nv_tegra/` (and subdirectories)

Merge the pieces that apply into **your** rootfs. For NVIDIA’s sample Ubuntu rootfs, run **`Linux_for_Tegra/apply_binaries.sh`** after unpacking the sample filesystem so GPU drivers and NVIDIA files land in the tree.

**MB1 Configuration Changes**

On **T234**, **MB1** reads multiple **`.dts` / `.dtsi`** fragments (pinmux, pad voltage, PMIC, storage, UPHY, …) compiled into **BCT** binaries. Those sources normally live under:

`<l4t_top>/bootloader/generic/BCT/`

For **field-level BCT** reference (what each fragment can contain), use **[T23x-Deployment.md](T23x-Deployment.md)** (DU-10990-001).

**Generating the Pinmux dtsi Files**

Use NVIDIA’s **Orin NX / Orin Nano pinmux** spreadsheet (from [Jetson / embedded downloads](https://developer.nvidia.com/embedded/downloads)) to define **SFIO vs GPIO**, pulls, and related options. The sheet documents **ball locations** and the **device tree** view of each pin.

**Spreadsheet checklist**

1. Enable **macros** when prompted.
2. Keep **Pin Direction** consistent with the function (e.g. **I2C** clock/data are **bidirectional**).
3. Match **Req Initial State** to that direction (e.g. **Drive 0/1** only where output or bidirectional applies).
4. Use **3.3V Tolerance** only where the sheet allows; enabling it makes the pin **open-drain** where applicable.
5. After editing, use **Generate DT File** in the spreadsheet.

Typical outputs (exact names depend on your spreadsheet inputs):

- `pinmux.dtsi`
- `gpio.dtsi`
- `padvoltage.dtsi`

**Install paths**

- Copy **`pinmux.dtsi`** and **`padvoltage.dtsi`** → `<l4t_top>/bootloader/generic/BCT/`
- Copy **`gpio.dtsi`** → `<l4t_top>/bootloader/`

Point your **board `.conf`** at these files (see [Flashing the build image](#flashing-the-build-image)).

</details>

### 修改 Pinmux

要获得 **ODM data**、**UPHY** 的**概念图**，以及 **`devmem`**、**sysfs**、**`libgpiod`** 与 **MB1 BCT pinmux** 之间的关系，请先阅读 **[ODMDATA-and-GPIO-Jetson-Linux.md](ODMDATA-and-GPIO-Jetson-Linux.md)**——以下步骤是用于 **TRM + `devmem`** 调试的 **NVIDIA 流程**。

从 JetPack 6 开始，可以通过以下方式修改 pinmux：

按 Generating the Pinmux dtsi Files 中所述更新 MB1 pinmux BCT。

调试时，可以动态修改 pinmux。

动态修改 pinmux 的步骤：

获取 pinmux 寄存器地址。

在 TRM 中，点击 System Components → Multi-Purpose I/O Pins and Pin Multiplexing (PinMux) → Pinmux Registers。

搜索引脚名称（例如 SOC_GPIO37）。

记下完整的引脚名称。

例如，PADCTL_G3_SOC_GPIO37_0 以及 Offset（例如 SOC_GPIO37: Offset = 0x80）。

打开 https://developer.nvidia.com/orin-series-soc-technical-reference-manual, 处的 Jetson Orin Technical Reference Manual，在 Table 1-15: Pad Control Grouping 中找到 G3 pad control block = PADCTL_A0 条目。

在 Memory Architecture 页面中，点击 Memory Mapped I/O → Address Map。

搜索 PADCTL_A0。

基地址为 PADCTL_A0 = 0x02430000，pinmux 寄存器地址为 PADCTL 基地址 + offset。

例如，SOC_GPIO37 Pinmux 寄存器地址 = 0x02430000 + 0x80 = 0x02430080。

用 devmem 工具读取设备中当前的寄存器值。

安装 devmem。


```bash
$ sudo apt-get install busybox
$ busybox devmem <32-bit address>
```

例如，busybox devmem 0x02430080。

要把该引脚用作 GPIO，请更新 PADCTL 寄存器中的以下字段：

按步骤 2 所述，从 TRM 的 Pinmux Registers 章节中查找寄存器信息。

将 GPIO 的 Bit 10 设为 0。

输出时，设置 Bit 4 = 0 ; Bit 6 = 0。

输入时，设置 Bit 4 = 1 ; Bit 6 = 1。

使用 devmem 工具设置寄存器，例如 busybox devmem 0x02430080 w 0x050。

验证寄存器值已相应设置。

一个示例

更新 Pin MCLK05: SOC_GPIO33: PQ6：

调整 pinmux，把该引脚映射到 GPIO。

配置 GPIO 控制器（输入、输出高或输出低）。

为该引脚配置 pinmux：

Pinmux 寄存器基地址为 0x2430000。

Offset 为 0x70。

Pinmux 寄存器地址为 0x2430070。

运行以下命令。


```bash
$ busybox devmem 0x02430070
```


输出的值为 0x00000054。

要把该引脚设为输出，运行以下命令。


```bash
$ busybox devmem 0x02430070 w 0x004
```


要确认，请检查输出。


```bash
$ busybox devmem 0x02430070
```


从 JetPack 6 开始，GPIO sysfs 已废弃。

要在输入和输出之间设置 GPIO 控制器的方向，用户可以使用 upstream GPIO 工具，而不使用 GPIO sysfs。


<details>
<summary>English original</summary>

**Changing the Pinmux**

For a **conceptual map** of **ODM data**, **UPHY**, and how **`devmem`**, **sysfs**, and **`libgpiod`** relate to **MB1 BCT pinmux**, read **[ODMDATA-and-GPIO-Jetson-Linux.md](ODMDATA-and-GPIO-Jetson-Linux.md)** first—the steps below are the **NVIDIA procedure** for **TRM + `devmem`** debug.

Starting with JetPack 6, you can change the pinmux in the following ways:

Update the MB1 pinmux BCT as mentioned in Generating the Pinmux dtsi Files.

For debugging, you can dynamically change the pinmux.

To dynamically change the pinmux:

Get the pinmux register address.

In TRM, click System Components → Multi-Purpose I/O Pins and Pin Multiplexing (PinMux) → Pinmux Registers.

Search for the pin name (for example, SOC_GPIO37).

Write down the complete pin name.

For example, PADCTL_G3_SOC_GPIO37_0 and the Offset (for example, SOC_GPIO37: Offset = 0x80).

Go to the Jetson Orin Technical Reference Manual at https://developer.nvidia.com/orin-series-soc-technical-reference-manual, and in Table 1-15: Pad Control Grouping, find the G3 pad control block = PADCTL_A0 entry.

On the Memory Architecture page, click Memory Mapped I/O → Address Map.

Search for PADCTL_A0.

The base address is PADCTL_A0 = 0x02430000, and the pinmux register address is PADCTL base address + offset.

For example, SOC_GPIO37 Pinmux register address = 0x02430000 + 0x80 = 0x02430080.

Get the current register value in the device using the devmem tool.

Install devmem.


```bash
$ sudo apt-get install busybox
$ busybox devmem <32-bit address>
```

For example, busybox devmem 0x02430080.

To use the pin as GPIO, update the following fields in the PADCTL register:

Find the register information from the Pinmux Registers section in TRM as mentioned in step 2.

Set GPIO to Bit 10 = 0.

For the output, set Bit 4 = 0 ; Bit 6 = 0.

For Input, set Bit 4 = 1 ; Bit 6 = 1.

Use the devmem tool to set the register, for example, busybox devmem 0x02430080 w 0x050.

Verify that the register value is set accordingly.

An Example

To update Pin MCLK05: SOC_GPIO33: PQ6:

Adjust the pinmux to map the pin to GPIO.

Configure the GPIO controller (input, output high, or output low).

To configure the pinmux for this pin:

The Pinmux register base address is 0x2430000.

The Offset is 0x70.

The Pinmux register address is 0x2430070.

Run the following command.


```bash
$ busybox devmem 0x02430070
```


The output value is 0x00000054.

To set the pin as the output, run the following command.


```bash
$ busybox devmem 0x02430070 w 0x004
```


To confirm, check the output.


```bash
$ busybox devmem 0x02430070
```


Starting with JetPack 6, the GPIO sysfs has been deprecated.

To set the direction of the GPIO controller between the input and the output, users can use upstream GPIO tools instead of GPIO sysfs.

</details>

### 确定 GPIO 编号

在**自定义载板**上，用运行中 kernel 的控制器 **base**，加上 pinmux 文档中的 **port** 与 **pin** 偏移，即可把 **SOM ball / 表格名称** 映射为 **Linux GPIO 编号**。

**1. 从 kernel 读取 base**（示例）：

```text
root@jetson:/home/ubuntu# dmesg | grep gpiochip
[    5.726492] gpiochip0: registered GPIOs 348 to 511 on tegra234-gpio
[    5.732478] gpiochip1: registered GPIOs 316 to 347 on tegra234-gpio-aon
root@jetson:/home/ubuntu#
```

典型划分：**main** `tegra234-gpio`（例如 base **348**）与 **always-on** `tegra234-gpio-aon`（例如 base **316**）——**始终以你的 `dmesg` 为准**。

**2. 使用 port/offset 表**（配合 pinmux 表格）查询 **port_offset** 和 **pin_offset**：

以下是 tegra234 GPIO 端口及偏移映射列表：

| 端口 | 引脚数 | 端口偏移 |
|------|----------------|-------------|
| PORT_A | 8 | 0 |
| PORT_B | 8 | 8 |
| PORT_C | 8 | 9 |
| PORT_D | 4 | 17 |
| PORT_E | 8 | 21 |
| PORT_F | 6 | 29 |
| PORT_G | 8 | 35 |
| PORT_H | 8 | 43 |
| PORT_I | 7 | 51 |
| PORT_J | 6 | 58 |
| PORT_K | 8 | 64 |
| PORT_L | 4 | 72 |
| PORT_M | 8 | 76 |
| PORT_N | 8 | 84 |
| PORT_P | 8 | 92 |
| PORT_Q | 8 | 100 |
| PORT_R | 6 | 108 |
| PORT_X | 8 | 114 |
| PORT_Y | 8 | 122 |
| PORT_Z | 8 | 130 |
| PORT_AC | 8 | 138 |
| PORT_AD | 4 | 146 |
| PORT_AE | 2 | 150 |
| PORT_AF | 4 | 152 |
| PORT_AG | 8 | 156 |
| PORT_AA | 8 | 164 |
| PORT_BB | 4 | 172 |
| PORT_CC | 8 | 176 |
| PORT_DD | 3 | 184 |
| PORT_EE | 8 | 187 |
| PORT_GG | 1 | 195 |

> **注意：** 本表尾部（`PORT_AA` 起）在 HTML 抓取时被破坏（出现零值和小偏移）。上表数值按 `PORT_AG` 之后的**累加**间距给出（引脚数 + 前一偏移）。请对照当前 **Jetson Orin NX / Nano 适配**主题或你所用版本的 pinmux 表格确认。

从 Jetson Orin NX 系列和 Jetson Orin Nano 系列 Pinmux 表中查找引脚详情（参见 Generating the Pinmux dtsi Files）。

例如 SOC_GPIO08 为 GPIO3_PB.00。

确定 port 为 B，Pin_offset 为 0。

**公式**

```text
gpio_number = base + port_offset + pin_offset
```

**完整示例（SOC_GPIO08 → GPIO3_PB.00）**

- 从 `dmesg` 中，**main GPIO** 控制器 **`tegra234-gpio`** 通常 **base 为 348**（你的日志为准）。
- 从 pinmux 表和 port 映射表可知：**port B**、**pin offset 0** → **port_offset = 8**。
- 因此：`348 + 8 + 0 =` **356**。

**使用 debugfs**

在目标板上，引脚在 debugfs 中导出后：

```bash
cat /sys/kernel/debug/gpio | grep PB.00
```

示例行（数值随 kernel 而异）：

```text
gpio-356 (PB.00 ...
```

要把某个引脚用作 GPIO，需确保该 GPIO 引脚对应的 pinmux 寄存器中 E_IO_HV 字段已禁用。可以在 pinmux 表格中禁用 3.3V Tolerance Enable 字段。

此外，确保 Pin Direction 设为 Bidirectional，这样 userspace 框架就能以输入和输出两个方向操作 GPIO。完成这些配置后，用更新后的 pinmux 文件重新烧录板卡。

### 配置 I2C 和 DP1_AUX 的 pinmux 设置

**I2C 与 DP-AUX** 共用无法仅靠 pinmux 表格表达；需添加如下 **设备树** 片段：

```dts
miscreg-dpaux@00100000 {
	compatible = "nvidia,tegra194-misc-dpaux-padctl";
	reg = <0x0 0x00100000 0x0 0xf000>;
	dpaux_default: pinmux@0 {
		dpaux0_pins {
			pins = "dpaux-0";
			function = "i2c";
		};
	};
};

i2c@31b0000 {
	pinctrl-names = "default";
	pinctrl-0 = <&dpaux_default>;
};
```

### 更改 GPIO 引脚

只有满足以下**全部**条件时，才能在 runtime 用 **GPIO tools** 更改某些引脚（JetPack 6+ 已弃用 legacy GPIO sysfs）：

- 引脚位于 **40-pin** 排针上（在 pinmux 表格中确认）。
- MB1 BCT **未**把该引脚分配为你仍需要的固定 **SFIO** 功能。
- MB1 BCT 按 userspace 翻转所需把该引脚设为 **bidirectional**。

## EEPROM 修改

在**没有** carrier ID EEPROM 的**自定义载板**上，调整 **MB2 BCT**，使 MB2 不再期望读取 CVB EEPROM。NVIDIA 的示例文件：

`Linux_for_Tegra/bootloader/generic/BCT/tegra234-mb2-bct-misc-p3767-0000.dts`

```dts
- cvb_eeprom_read_size = <0x100>
+ cvb_eeprom_read_size = <0x0>
```

## 移植 Linux 内核设备树


<details>
<summary>English original</summary>

**Identifying the GPIO Number**

On a **custom carrier**, you map **SOM ball / spreadsheet name** → **Linux GPIO number** using the controller **base** from the running kernel plus **port** and **pin** offsets from the pinmux doc.

**1. Read bases from the kernel** (example):

```text
root@jetson:/home/ubuntu# dmesg | grep gpiochip
[    5.726492] gpiochip0: registered GPIOs 348 to 511 on tegra234-gpio
[    5.732478] gpiochip1: registered GPIOs 316 to 347 on tegra234-gpio-aon
root@jetson:/home/ubuntu#
```

Typical split: **main** `tegra234-gpio` (e.g. base **348**) vs **always-on** `tegra234-gpio-aon` (e.g. base **316**)—**always trust your `dmesg`**.

**2. Use the port/offset table** (with the pinmux spreadsheet) for **port_offset** and **pin_offset**:

Here is the list of the tegra234 GPIO ports and offset mapping:

| Port | Number of pins | Port offset |
|------|----------------|-------------|
| PORT_A | 8 | 0 |
| PORT_B | 8 | 8 |
| PORT_C | 8 | 9 |
| PORT_D | 4 | 17 |
| PORT_E | 8 | 21 |
| PORT_F | 6 | 29 |
| PORT_G | 8 | 35 |
| PORT_H | 8 | 43 |
| PORT_I | 7 | 51 |
| PORT_J | 6 | 58 |
| PORT_K | 8 | 64 |
| PORT_L | 4 | 72 |
| PORT_M | 8 | 76 |
| PORT_N | 8 | 84 |
| PORT_P | 8 | 92 |
| PORT_Q | 8 | 100 |
| PORT_R | 6 | 108 |
| PORT_X | 8 | 114 |
| PORT_Y | 8 | 122 |
| PORT_Z | 8 | 130 |
| PORT_AC | 8 | 138 |
| PORT_AD | 4 | 146 |
| PORT_AE | 2 | 150 |
| PORT_AF | 4 | 152 |
| PORT_AG | 8 | 156 |
| PORT_AA | 8 | 164 |
| PORT_BB | 4 | 172 |
| PORT_CC | 8 | 176 |
| PORT_DD | 3 | 184 |
| PORT_EE | 8 | 187 |
| PORT_GG | 1 | 195 |

> **Note:** The tail of this table (`PORT_AA` onward) was corrupted in the HTML scrape (zeros and small offsets). Values above follow **cumulative** spacing after `PORT_AG` (pin count + previous offset). Confirm against the current **Jetson Orin NX / Nano adaptation** topic or pinmux spreadsheet for your release.

Search for the pin details from the Jetson Orin NX Series and Jetson Orin Nano Series Pinmux table (refer to Generating the Pinmux dtsi Files).

For example SOC_GPIO08 is GPIO3_PB.00.

Identify the port as B and the Pin_offset as 0.

**Formula**

```text
gpio_number = base + port_offset + pin_offset
```

**Worked example (SOC_GPIO08 → GPIO3_PB.00)**

- From `dmesg`, **main GPIO** controller **`tegra234-gpio`** often has **base 348** (your log is authoritative).
- From the pinmux table and the port mapping table: **port B**, **pin offset 0** → **port_offset = 8**.
- So: `348 + 8 + 0 =` **356**.

**Using debugfs**

On the target, after the pin is exported in debugfs:

```bash
cat /sys/kernel/debug/gpio | grep PB.00
```

Example line (numbers vary by kernel):

```text
gpio-356 (PB.00 ...
```

To use a pin as the GPIO, ensure that the E_IO_HV field is disabled in the corresponding pinmux register of the GPIO pin. You can disable the 3.3V Tolerance Enable field in the pinmux spreadsheet.

Also, make sure Pin Direction is set to Bidirectional so that the userspace framework can operate GPIO in both input and output direction. After these configurations, reflash the board with the updated pinmux file.

**Configuring the pinmux Setting of I2C and DP1_AUX**

**I2C vs DP-AUX** sharing cannot be expressed from the pinmux spreadsheet alone; add a **device tree** fragment like:

```dts
miscreg-dpaux@00100000 {
	compatible = "nvidia,tegra194-misc-dpaux-padctl";
	reg = <0x0 0x00100000 0x0 0xf000>;
	dpaux_default: pinmux@0 {
		dpaux0_pins {
			pins = "dpaux-0";
			function = "i2c";
		};
	};
};

i2c@31b0000 {
	pinctrl-names = "default";
	pinctrl-0 = <&dpaux_default>;
};
```

**Changing the GPIO Pins**

You can change certain pins at runtime with **GPIO tools** (JetPack 6+ moves away from legacy GPIO sysfs) only if **all** of the following hold:

- The pin is on the **40-pin** header (confirm in the pinmux spreadsheet).
- MB1 BCT does **not** assign the pin as a fixed **SFIO** function you still need.
- MB1 BCT sets the pin **bidirectional** as required for userspace toggling.

**EEPROM Modifications**

On a **custom carrier without** a carrier ID EEPROM, adjust **MB2 BCT** so MB2 does not expect CVB EEPROM reads. NVIDIA’s example file:

`Linux_for_Tegra/bootloader/generic/BCT/tegra234-mb2-bct-misc-p3767-0000.dts`

```dts
- cvb_eeprom_read_size = <0x100>
+ cvb_eeprom_read_size = <0x0>
```

**Porting the Linux Kernel Device Tree**

</details>

### T23x 设备树结构概述

要下载和构建 DT 源码，请遵循 [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) 中适用于你的 JetPack 产品线的 **Kernel customization**。

完成上述链接中的步骤后，设备树文件将位于以下位置：

Linux_for_Tegra/source/hardware/nvidia/t23x/nv-public

设备树结构分为两个不同的层：

底层来自上游 Linux 内核，这使 NVIDIA 源码与上游保持一致。

这些文件位于 nv-public 文件夹的根目录（例如，参见 nv-public/tegra234-p3768-0000+p3767-0005.dts）。

顶层来自 nv-platform 目录。

每个平台的主 dts 文件命名与上游 dts 文件一致。然而，它不是 <board>.dts，而是 <board>-nv.dts。例如，nv-public/nv-platform/tegra234-p3768-0000+p3767-0005-nv.dts 文件包含上游 dts 文件，并还添加内容和更新。

对于 Orin Nano Developer Kit 上支持的每个 **P3767 模块 SKU**，每个 DTB 都有变体（SKU **0、1、3、4、5**）。顶层 **`nv`** DTB 示例：

- `tegra234-p3768-0000+p3767-0000-nv.dtb`
- `tegra234-p3768-0000+p3767-0001-nv.dtb`
- `tegra234-p3768-0000+p3767-0003-nv.dtb`
- `tegra234-p3768-0000+p3767-0004-nv.dtb`
- `tegra234-p3768-0000+p3767-0005-nv.dtb`

每个都有一个匹配的 **`.dts`**，位于 **`nv-platform/`** 下。

### 更新 DTB 文件

创建或修改 dtb 文件后，将文件复制到 Linux_for_Tegra/kernel/dtb 目录；在该目录中，flash.sh 脚本会根据关联 flash 配置文件中的 DTB_FILE 变量来拾取它。

默认情况下，UEFI 配置为 extlinux 启动，因此它会在 /boot/extlinux/extlinux.conf 文件中查找 FDT 条目。如果找到条目，且文件存在，则 UEFI 在加载 kernel dtb 文件时使用 rootfs 中的该文件。如果未指定，或未找到，则 UEFI 回退为使用其自身的 dtb 文件来加载 kernel dtb。

使用 fdtdump 验证你的更改是否已按预期生效。

例如，对输出进行分页和搜索：

```bash
fdtdump tegra234-p3768-0000+p3767-0005-nv.dtb | less
```

### 设备树 Overlays

flash 配置中属于 OVERLAY_DTB_FILE 变量的 Overlay 文件将被烧录到 UEFI 分区（A/B-cpu_bootloader）。这些 overlays 会同时应用于 UEFI 和 kernel。可选地，可以使用 extlinux.conf 文件通过 OVERLAYS 关键字指定额外的 overlay 文件，但 extlinux.conf 中指定的 overlays 仅应用于 kernel dtb。

## 配置 PCIe 控制器

PCIe 主机控制器基于 Synopsys Designware PCIe 知识产权，因此主机继承信息文件中定义的通用属性，文件位于：

$(KERNEL_TOP)/Documentation/devicetree/bindings/pci/nvidia,tegra194-pcie.txt

该文件涵盖的主题包括配置最大链路速度、链路宽度，以及不同 ASPM 状态的通告。

### PCIe 控制器特性

Jetson Orin NX/Nano 系列具有以下 PCIe 控制器及规格：

速度：

Jetson Orin NX 支持最高 Gen4 速度

Jetson Orin Nano 支持最高 Gen3 速度。

Lane 宽度：

C4：最高 x4

C7：最高 x2（1 个 lane 与 C9 复用）

C9：x1

C1：x1

控制器：

控制器 C4 支持双模式（根端口或端点）

C1、C7 和 C9 仅支持根端口

ASPM：所有控制器均支持 ASPM。

### 在客户载板设计中启用 PCIe

选择适合载板设计的适当 UPHY 配置，并相应更新 ODMDATA（更多信息请参阅 Configuring the UPHY Lane）。

从下表中启用相应的 PCIe 节点。

确保在 MB1 Pinmux 和 gpio dtsi 中正确配置控制器的 CLK 和 RST 引脚（更多信息请参阅 Generating the Pinmux dtsi Files）。

在 PCIe 控制器 DT 节点中，将 pipe2uphy phandle 条目添加为 phy 属性。

pipe2uphy DT 节点在 SoC DT 中定义，位置为 $(TOP)/hardware/nvidia/soc/t234/kernel-dts/tegra234-soc/tegra234-soc-pcie.dtsi。
每个 pipe2uphy 节点与 Configuring the UPHY Lane 中定义的 UPHY lane 一一映射。

以下是 PCIe 控制器 DT 模式：

PCIe 控制器与模式

PCIe 控制器 DT 模式

PCIe C1 RP

pcie@14100000

PCIe C4 RP

pcie@14160000

PCIe C4 EP

pcie_ep@14160000

PCIe C7 RP

pcie@141e0000

PCIe C9 RP

pcie@140c0000

## 移植 Universal Serial Bus

Jetson Orin NX/Nano 最多可支持三个增强型 SuperSpeed Universal Serial Bus (USB) 端口。在某些实现中，由于 PCIE、UFS 和 XUSB 之间的 UPHY lane 共享，并非所有这些端口都能使用。如果你设计了自己的载板，请咨询 NVIDIA 团队，以验证 Universal physical layer (UPHY) lane 映射以及 P3767 与自定义板之间的兼容性。


<details>
<summary>English original</summary>

**Overview of T23x Device Tree Structure**

For downloading and building DT sources, follow **Kernel customization** in the [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) for your JetPack line.

After you complete the steps from the link above, the device tree files will be in the following location:

Linux_for_Tegra/source/hardware/nvidia/t23x/nv-public

The device tree is structured as two distinct layers:

The bottom layer comes from the upstream Linux kernel, which keeps the NVIDIA sources aligned with the upstream.

These files are in the root of the nv-public folder (for example, see nv-public/tegra234-p3768-0000+p3767-0005.dts).

The top layer comes from the nv-platform directory.

The main dts file for each platform is named to coincide with the upstream dts file. However, instead of <board>.dts it is <board>-nv.dts. For example, the nv-public/nv-platform/tegra234-p3768-0000+p3767-0005-nv.dts file includes the upstream dts file and also adds content and updates.

There are variations of each DTB for each **P3767 module SKU** supported on the Orin Nano Developer Kit (SKUs **0, 1, 3, 4, 5**). Top-level **`nv`** DTB examples:

- `tegra234-p3768-0000+p3767-0000-nv.dtb`
- `tegra234-p3768-0000+p3767-0001-nv.dtb`
- `tegra234-p3768-0000+p3767-0003-nv.dtb`
- `tegra234-p3768-0000+p3767-0004-nv.dtb`
- `tegra234-p3768-0000+p3767-0005-nv.dtb`

Each has a matching **`.dts`** under **`nv-platform/`**.

**Updating DTB Files**

After creating or modifying a dtb file, copy the file to the Linux_for_Tegra/kernel/dtb directory where it will get picked up by the flash.sh script as determined by the DTB_FILE variable in the associated flash configuration file.

By default, UEFI is configured for extlinux boot, so it looks for an FDT entry in the /boot/extlinux/extlinux.conf file. If an entry is found, and the file exists, the UEFI uses that file from the rootfs when loading the kernel dtb file. If it is not specified, or not found, the UEFI falls back to using its own dtb file to load the kernel dtb.

Use fdtdump to verify whether your changes have taken effect as expected.

For example, to page and search the output:

```bash
fdtdump tegra234-p3768-0000+p3767-0005-nv.dtb | less
```

**Device Tree Overlays**

Overlay files that are part of the OVERLAY_DTB_FILE variable in the flash configuration will be flashed into the UEFI partition (A/B-cpu_bootloader). These overlays are applied for both UEFI and kernel. Optionally, the extlinux.conf file can be used to specify additional overlay files using the OVERLAYS keyword, but the overlays specified in extlinux.conf are applied only to the kernel dtb.

**Configuring the PCIe Controller**

The PCIe host controller is based on the Synopsys Designware PCIe intellectual property, so the host inherits the common properties that are defined in the information file at:

$(KERNEL_TOP)/Documentation/devicetree/bindings/pci/nvidia,tegra194-pcie.txt

This file covers topics that include configuring maximum link speed, link width, and the advertisement of different ASPM states.

**PCIe Controller Features**

Jetson Orin NX/Nano series has the following PCIe controllers with these specifications:

Speed:

Jetson Orin NX supports up to Gen4 speed

Jetson Orin Nano supports up to Gen3 speed.

Lane width:

C4: up to x4

C7: up to x2 (1 lane multiplexed with C9)

C9: x1

C1: x1

Controllers:

Controller C4 supports dual mode (root port or endpoint)

C1, C7, and C9 support root port only

ASPM: All controllers support ASPM.

**Enabling PCIe in a Customer Carrier Board Design**

Select the appropriate UPHY configuration that suits the carrier board design and update ODMDATA accordingly (refer to Configuring the UPHY Lane for more information).

Enable the appropriate PCIe node from the table below.

Ensure that the controller Pins CLK and RST are configured correctly in MB1 Pinmux and gpio dtsi (refer to Generating the Pinmux dtsi Files for more information).

Add the pipe2uphy phandle entries as a phy property in the PCIe controller DT node.

pipe2uphy DT nodes are defined in SoC DT at $(TOP)/hardware/nvidia/soc/t234/kernel-dts/tegra234-soc/tegra234-soc-pcie.dtsi.
Each pipe2uphy node is a 1:1 map to the UPHY lanes that are defined in Configuring the UPHY Lane.

Here are the PCIe controller DT modes:

PCIe Controller and Mode

PCIe Controller DT Mode

PCIe C1 RP

pcie@14100000

PCIe C4 RP

pcie@14160000

PCIe C4 EP

pcie_ep@14160000

PCIe C7 RP

pcie@141e0000

PCIe C9 RP

pcie@140c0000

**Porting Universal Serial Bus**

Jetson Orin NX/Nano can support up to three enhanced SuperSpeed Universal Serial Bus (USB) ports. In some implementations, not all of these ports can be used because of UPHY lane sharing among PCIE, UFS, and XUSB. If you designed your carrier board, verify the Universal physical layer (UPHY) lane mapping and compatibility between P3767 and your custom board by consulting the NVIDIA team.

</details>

### USB 结构

增强型 SuperSpeed USB 端口有九个引脚：

VBUS。

GND。

D+。

D−。

两对用于 SuperSpeed 数据传输的差分信号对。

一个地（GND_DRAIN），用于 drain wire 端接以及管理 EMI、RFI 和信号完整性。

USB SuperSpeed 端口引脚定义

D+/D− 信号引脚连接到 UTMI pad。SSTX/SSRX 信号引脚连接到 UPHY，由一个 UPHY lane 处理。由于 UPHY lane 在 PPCIE、UFS 和 XUSB 之间共享，因此必须根据定制载板的需求分配 UPHY lane。

### USB SerDes Lane 分配

USB SuperSpeed 的 SerDes lane 属于 UPHY 模块的一部分。Jetson P3767 SOM 的 USB 端口具有以下 UPHY lane：

Jetson SODIMM 信号名 | Orin UPHY 模块与 lane | Jetson Orin NX/Nano 功能

USBSS0_RX/TX | UPHY0, Lane 0 | USB 3.2 (P0)

DP0_TXD0/1 | UPHY0, Lane 1 | USB 3.2 (P1)

DP0_TXD2/3 | UPHY0, Lane 2 | USB 3.2 (P2)

支持的 PCIe 配置列表参见 UPHY Lane Configuration。

设计定制板之前，请参阅 NVIDIA Jetson Orin Series SOC Technical Reference Manual（TRM）、NVIDIA Jetson Orin NX Series and Orin Nano Product Design Guide（DG），然后联系 NVIDIA。


<details>
<summary>English original</summary>

**USB Structure**

An enhanced SuperSpeed USB port has nine pins:

VBUS.

GND.

D+.

D−.

Two differential signal pairs for SuperSpeed data transfer.

One ground (GND_DRAIN) for drain wire termination and managing EMI, RFI, and signal integrity.

USB SuperSpeed port pinout
The D+/D− signal pins connect to UTMI pads. The SSTX/SSRX signal pins connect to UPHY and are handled by one UPHY lane. As UPHY lanes are shared between PPCIE, UFS, and XUSB, the UPHY lanes must be assigned based on the custom carrier board’s requirements.

**USB SerDes Lane Assignment**

The SerDes lanes for USB SuperSpeed are part of the UPHY block. The Jetson P3767 SOM USB ports have the following UPHY lanes:

Jetson SODIMM Signal Name

Orin UPHY Block and Lane

Jetson Orin NX/Nano Function

USBSS0_RX/TX

UPHY0, Lane 0

USB 3.2 (P0)

DP0_TXD0/1

UPHY0, Lane 1

USB 3.2 (P1)

DP0_TXD2/3

UPHY0, Lane 2

USB 3.2 (P2)

Refer to UPHY Lane Configuration for a list of the supported PCIe configurations.

Before you design your custom board, refer to the NVIDIA Jetson Orin Series SOC Technical Reference Manual (TRM), the NVIDIA Jetson Orin NX Series and Orin Nano Product Design Guide (DG), and then contact NVIDIA.

</details>

### 所需的设备树更改

本节逐步介绍如何检查原理图以及在设备树中配置 USB 端口。示例基于 P3767 SOM 和 P3678 载板。

对于 Host-Only 端口
本节以 J6 type A 堆叠连接器作为 Host-Only 端口的示例。J6 的 USB 信号（USB2.0 和 USB3.2）经由 USB HUB 来自 SOM 的 Port 0 USBSS 线，如下图所示。

../../_images/HostOnlyPort.png
xusb_padctl 节点
设备树的 xusb_padctl 节点遵循 pinctrl-bindings.txt 的约定。它包含两个组，分别名为 pads 和 ports，用于描述 USB2 与 USB3 信号以及参数和端口号。pads 和 ports 中每个参数描述子节点的名称必须采用 <type>-<port_number> 形式，其中 <type> 为 "usb2" 或 "usb3"，<port_number> 为关联的端口号。

pads 子节点
nvidia,function：字符串，包含要复用（mux）到该引脚或引脚组的功能名称。必须为 xusb。

ports 子节点
mode：描述 USB 端口能力的字符串。

USB2 端口必须具有该属性。其取值必须为以下之一：

host

peripheral

OTG

nvidia,usb2-companion：该端口映射到的 USB2 端口（0、1 或 2）。

USB3 端口必须具有该属性。

nvidia,oc-pin：该端口使用的 VBUS 过流引脚。

该值必须为正数或零。

某些 Type-C 端口控制器（例如 Cypress 的）可以处理过流检测与处理。因此，对于带有 Type-C 端口控制器的 USB Type-C 连接器，无需设置该属性。

vbus-supply：对应 UTMI pad 的 VBUS 稳压器。

对于 dummy regulator，设置为 &vdd_5v0_sys。

关于 xusb_padctl 的更多信息，请参阅 https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/phy/nvidia,tegra194-xusb-padctl.yaml。

示例：xusb_padctl 的 USB 端口启用

以 J6（Type-A 堆叠端口）为例，创建 pad/port 节点和属性列表：


<details>
<summary>English original</summary>

**Required Device Tree Changes**

This section gives step-by-step guidance for checking schematics and configuring USB ports in the device tree. The examples are based on the P3767 SOM and P3678 carrier board.

For a Host-Only Port
This section takes a J6 type A stacked connector as an example of a host-only port. The USB signals (USB2.0 and USB3.2) for the J6 are coming from the Port 0 USBSS lines of the SOM through USB HUB as shown in the following image.

../../_images/HostOnlyPort.png
The xusb_padctl Node
The device tree’s xusb_padctl node follows the conventions of pinctrl-bindings.txt. It contains two groups, named pads and ports, which describe USB2 and USB3 signals along with parameters and port numbers. The name of each parameter description subnode in pads and ports must be in the form <type>-<port_number>, where <type> is "usb2" or "usb3", and <port_number> is the associated port number.

The pads Subnode
nvidia,function: A string containing the name of the function to mux to the pin or group. Must be xusb.

The ports Subnode
mode: A string that describes USB port capability.

A port for USB2 must have this property. It must be one of these values:

host

peripheral

OTG

nvidia,usb2-companion: USB2 port (0, 1, or 2) to which the port is mapped.

A port for USB3 must have this property.

nvidia,oc-pin: The overcurrent VBUS pin the port is using.

The value must be positive or zero.

Some Type-C port controllers, such as from Cypress can handle the overcurrent detection and handling. Therefore, you do not need to set this property for USB type C connectors that have Type-C port controllers.

vbus-supply: VBUS regulator for the corresponding UTMI pad.

Set to &vdd_5v0_sys for a dummy regulator.

Refer to https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/phy/nvidia,tegra194-xusb-padctl.yaml for more information about xusb_padctl.

Example: USB Port Enablement for xusb_padctl

Take J6 (Type-A stacked port), for example, and create a pad/port node and property list:

</details>

xusb_padctl: padctl@3520000 {
 ...
 pads {
       usb2 {
             lanes {
                    ...
                    usb2-1 {
                          nvidia,function = "xusb";
                          status = "okay";
                    };
                    ...
             };
       };
       usb3 {
             lanes {
                    ...
                    usb3-0 {
                          nvidia,function = "xusb";
                          status = "okay";
                    };
                    ...
             };
       };
 };
 ports {
        ...
        usb2-1 {
              mode = "host";
              status = "okay";
            };
            ...
            usb3-0 {
                  nvidia,usb2-companion = <1>;
                  status = "okay";
            };
            ...
 };
};
在 xHCI 节点下
Jetson Orin xHCI 控制器符合 xHCI 规范，支持 USB 2.0 HighSpeed/FullSpeed/LowSpeed 和 USB 3.2 SuperSpeed 协议。

phys：必须为 phy-names 中的每一项都包含一个条目。

phy-names：必须为控制器使用的每个 PHY 都包含一个条目。

名称必须采用 <type>-<port_number> 的形式，其中 <type> 为 "usb2" 或 "usb3"，<port_number> 为关联的端口号。

nvidia,xusb-padctl：指向 xusb-padctl 节点的指针。

有关 xHCI 的更多信息，请参阅 https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/usb/nvidia,tegra234-xusb.yaml。

示例：USB 端口启用或 tegra_xhci

本示例包含一个 J6 示例，并创建一个 xHCI 节点和属性列表：

usb@3610000 {
    ...
    phys = <&{/bus@0/padctl@3520000/pads/usb2/lanes/usb2-1}>,
           <&{/bus@0/padctl@3520000/pads/usb3/lanes/usb3-2}>;
    phy-names = "usb2-1", "usb3-2";
    nvidia,xusb-padctl = <&xusb_padctl>;
    status = "okay";
    ...
};
对于 On-The-Go 端口
有关 OTG 实现的示例，请参阅 For an On-The-GO Port。Orin Nano Developer Kit 的实现类似，但它使用 fusb301 控制器而非 ucsi_ccg 控制器。

从设备树中移除 Type-C 连接器和 FUSB301 驱动

在某些定制载板上，可能不存在 Type-C 连接器。这种情况下，需要调整设备树以反映这些组件的缺失。以下步骤概述了移除 Type-C 连接器和 FUSB301 驱动的过程：

从 i2c@c240000 中移除 fusb301 Type-C 控制器。


<details>
<summary>English original</summary>

 xusb_padctl: padctl@3520000 {
 ...
 pads {
       usb2 {
             lanes {
                    ...
                    usb2-1 {
                          nvidia,function = "xusb";
                          status = "okay";
                    };
                    ...
             };
       };
       usb3 {
             lanes {
                    ...
                    usb3-0 {
                          nvidia,function = "xusb";
                          status = "okay";
                    };
                    ...
             };
       };
 };
 ports {
        ...
        usb2-1 {
              mode = "host";
              status = "okay";
            };
            ...
            usb3-0 {
                  nvidia,usb2-companion = <1>;
                  status = "okay";
            };
            ...
 };
};
Under the xHCI Node
The Jetson Orin xHCI controller complies with xHCI specifications, which supports the USB 2.0 HighSpeed/FullSpeed/LowSpeed and USB 3.2 SuperSpeed protocols.

phys: Must contain an entry for each entry in phy-names.

phy-names: Must include an entry for each PHY used by the controller.

Names must be in the form <type>-<port_number>, where <type> is "usb2" or "usb3", and <port_number> is the associated port number.

nvidia,xusb-padctl: A pointer to the xusb-padctl node.

Refer to https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/devicetree/bindings/usb/nvidia,tegra234-xusb.yaml for more information about xHCI.

Example: USB Port Enablement or tegra_xhci

This example contains a J6 example and creates an xHCI node and property list:

usb@3610000 {
    ...
    phys = <&{/bus@0/padctl@3520000/pads/usb2/lanes/usb2-1}>,
           <&{/bus@0/padctl@3520000/pads/usb3/lanes/usb3-2}>;
    phy-names = "usb2-1", "usb3-2";
    nvidia,xusb-padctl = <&xusb_padctl>;
    status = "okay";
    ...
};
For an On-The-Go Port
For an example OTG implementation, refer to For an On-The-GO Port. The Orin Nano Developer Kit implementation is similar, but it uses the fusb301 controller instead of the ucsi_ccg controller.

Removing Type-C Connectors and FUSB301 Driver from the Device Tree

In some custom carrier boards, Type-C connectors may not be present. In such cases, adjustments to the device tree are necessary to reflect the absence of these components. The following steps outline the process of removing Type-C connectors and the FUSB301 driver:

Remove the fusb301 Type-C Controller from i2c@c240000.

</details>

如果不需要 ONSEMI FUSB301 Type-C Controller，则应从 i2c@c240000 下设备树中的对应条目中将其删除。

移除 USB 端口端点。

应移除 padctl@3520000/ports 下 usb2-0/port 的条目。这是必要的，因为在移除连接器后，关联的 remote-endpoint 不再需要。

为 USB 端口添加 role-switch-default-mode 属性（可选）。

role-switch-default-mode 属性是可选的，可用于 USB OTG 和外设端口，以在没有 Type-C 连接器时显式定义默认 USB 角色。

**示例 `git` 风格的 diff**（裁剪 Type-C / FUSB301 并设置默认角色——对照你的设备树验证）：


<details>
<summary>English original</summary>

If the ONSEMI FUSB301 Type-C Controller is not required, it should be removed from the corresponding entry in the device tree under i2c@c240000.

Remove the USB Port Endpoint.

The entry for the usb2-0/port under padctl@3520000/ports should be removed. This is necessary because the associated remote-endpoint is no longer required after the removal of the connector.

Add the role-switch-default-mode property for USB Ports (optional).

The role-switch-default-mode property is optional and may be used for USB OTG and peripheral ports to explicitly define the default USB role when no Type-C connector is present.

**Example `git`-style diff** (trim Type-C / FUSB301 and set default role—verify against your tree):

</details>

```diff
diff --git a/nv-platform/tegra234-p3768-0000+p3767-xxxx-nv-common.dtsi b/nv-platform/tegra234-p3768-0000+p3767-xxxx-nv-common.dtsi
index 4ac4ff0629fb..00f90ffffd2c 100644
--- a/nv-platform/tegra234-p3768-0000+p3767-xxxx-nv-common.dtsi
+++ b/nv-platform/tegra234-p3768-0000+p3767-xxxx-nv-common.dtsi
@@ -162,36 +162,8 @@
                        };
                };

-               padctl@3520000 {
-                       ports {
-                               usb2-0 {
-                                       port {
-                                               typec_p0: endpoint {
-                                                       remote-endpoint = <&fusb_p0>;
-                                               };
-                                       };
-                               };
-                       };
-               };
-
                i2c@c240000 {
                        status = "okay";
-                       fusb301@25 {
-                               compatible = "onsemi,fusb301";
-                               reg = <0x25>;
-                               status = "okay";
-                               #address-cells = <1>;
-                               #size-cells = <0>;
-                               interrupt-parent = <&gpio>;
-                               interrupts = <TEGRA234_MAIN_GPIO(Z, 1) IRQ_TYPE_LEVEL_LOW>;
-                               connector@0 {
-                                       port@0 {
-                                               fusb_p0: endpoint {
-                                                       remote-endpoint = <&typec_p0>;
-                                               };
-                                       };
-                               };
-                       };
                };

                /* PWM1, 40pin header, pin 15 */
diff --git a/tegra234-p3768-0000+p3767.dtsi b/tegra234-p3768-0000+p3767.dtsi
index 19340d13f789..9d935400baa0 100644
--- a/tegra234-p3768-0000+p3767.dtsi
+++ b/tegra234-p3768-0000+p3767.dtsi
@@ -102,6 +102,7 @@
                                        vbus-supply = <&vdd_5v0_sys>;
                                        status = "okay";
                                        usb-role-switch;
+                                       role-switch-default-mode = "peripheral";
                                };

                                /* hub */
```

## UPHY 通道配置

**UPHY** 通道在 **USB3**、**PCIe**、**Ethernet (GBE)** 及其他控制器之间共享。你通过你的板级 **`.conf`** 中的 **`ODMDATA`** 选择一个 **支持的 mux 预设**（通常是在 source `p3767.conf.common` 之后 **append**——不要就地编辑公共文件）。以下块内容 **逐字** 来自 NVIDIA 指南；在此 HTML 导出中，表格单元格通常 **每行一个值**——请使用 PDF 或开发者指南获取正确的网格。

**UPHY0 (HSIO) 配置选项**（摘录；完整主题见通道 5–7）：

通道

hsio-uphy-config-0

hsio-uphy-config-40

hsio-uphy-config-41

0

USB 3.2 (P0)

USB 3.2 (P0)

USB 3.2 (P0)

1

USB 3.2 (P1)

USB 3.2 (P1)

USB 3.2 (P1)

2

USB 3.2 (P2)

USB 3.2 (P2)

未使用

3

PCIe x1 (C1), RP

PCIe x1 (C1), RP (Gen2)

PCIe x1 (C1), RP

4

PCIe x4 (C4), RP

PCIe x4 (C4), EP

PCIe x4 (C4), EP

5

6

7

选择 hsio-uphy-config-40 时，PCIe C1 RP 将被限制为 Gen2 速率。

UPHY2 (GBE) 配置选项

通道

gbe-uphy-config-8

gbe-uphy-config-9

0

PCIe x2 (C7), RP

PCIe x1 (C7), RP

1

PCIe x1 (C9), RP

UPHY0/1/2 分别称为 HSIO、NVHS 和 GBE。Orin NX/Nano SOM 不使用 NVHS，且其已断电。

UPHY 配置通过在 flash 配置文件中使用 ODMDATA 变量来指定。p3767.conf.common 中定义的出厂配置如下所示：

ODMDATA="gbe-uphy-config-8,hsstp-lane-map-3,hsio-uphy-config-0";

GBE 配置和 HSIO 配置相互独立。例如，要将 PCIe C7 和 C9 用作独立的单通道控制器，请通过向 flash 配置文件添加如下所示的一行，选择 gbe-uphy-config-9 配置：

ODMDATA="gbe-uphy-config-9,hsstp-lane-map-3,hsio-uphy-config-0";

不要直接在 p3767.conf.common 文件中编辑此变量。你应改为在 flash 配置文件中追加该变量以覆盖默认值。要将 PCIE C4 用作端点，请使用上面的配置 hsio-uphy-config-40 或 hsio-uphy-config-41。更多信息请参阅 PCIE 端点模式。

### T234 的 ODM 数据

下表提供了 T234 的 ODM 数据信息。

31:26

25:23

22:18

17

16

15

14

13:0

HSIO UPHY 配置

NVHS UPHY 配置

GBE UPHY 配置

GBE3 模式

GBE2 模式

GBE1 模式

GBE0 模式

保留

HSIO UPHY 通道映射选项中的配置编号

NVHS UPHY 通道映射选项中的配置编号

GBE UPHY 通道映射选项中的配置编号

0:5G, 1:10G

0:5G, 1:10G

0:5G, 1:10G

0:5G, 1:10G

待定

BPMP-FW DTB /uphy/hsio-uphy-config

BPMP-FW DTB /uphy/nvhs-uphy-config

BPMP-FW DTB /uphy/gbe-uphy-config

BPMP-FW DTB /uphy/gbe3-enable-10g

BPMP-FW DTB /uphy/gbe2-enable-10g

BPMP-FW DTB /uphy/gbe1-enable-10g

BPMP-FW DTB /uphy/gbe0-enable-10g

待定

### HSIO UPHY 通道映射选项

配置编号

PLL0

通道 0

通道 1

PLL1

通道 2

通道 3

PLL2

通道 4

通道 5

通道 6

通道 7

PLL3

0

禁用

USB3.1 P0

USB3.1 P1

USB3/PCIE G2

USB3.1 P2

PCIE x1 C1

PCIE G4

PCIE x4 C4

PCIE x4 C4

PCIE x4 C4

PCIE x4 C4

USB3/PCIE G2

### GBE UPHY 通道映射选项

配置编号

PLL0

通道 0

通道 1

通道 2

通道 3

PLL1

通道 4

通道 5

通道 6

通道 7

PLL2

8

USB3/PCIE G2

PCIE x2 C7

PCIE x2 C7

PCIE x2 C8

PCIE x2 C8

PCIE G4

PCIE x2 C10

PCIE x2 C10

PCIE x2 C9

PCIE x2 C9

USB3/PCIE G2

9

USB3/PCIE G2

PCIE x1 C7

PCIE x1 C9

PCIE x2 C8

PCIE x2 C8

PCIE G4

PCIE x4 C10

PCIE x4 C10

PCIE x4 C10

PCIE x4 C10

仅 PCIE C10


<details>
<summary>English original</summary>

**UPHY Lane Configuration**

**UPHY** lanes are shared between **USB3**, **PCIe**, **Ethernet (GBE)**, and other controllers. You pick a **supported mux preset** using **`ODMDATA`** in your board **`.conf`** (usually **append** after sourcing `p3767.conf.common`—do not edit the common file in place). The blocks below are **verbatim** from NVIDIA’s guide; in this HTML export, table cells often appear **one value per line**—use the PDF or developer guide for a proper grid.

**UPHY0 (HSIO) configuration options** (excerpt; see full topic for lanes 5–7):

Lane

hsio-uphy-config-0

hsio-uphy-config-40

hsio-uphy-config-41

0

USB 3.2 (P0)

USB 3.2 (P0)

USB 3.2 (P0)

1

USB 3.2 (P1)

USB 3.2 (P1)

USB 3.2 (P1)

2

USB 3.2 (P2)

USB 3.2 (P2)

Unused

3

PCIe x1 (C1), RP

PCIe x1 (C1), RP (Gen2)

PCIe x1 (C1), RP

4

PCIe x4 (C4), RP

PCIe x4 (C4), EP

PCIe x4 (C4), EP

5

6

7

When selecting hsio-uphy-config-40, PCIe C1 RP will be restricted to Gen2 speeds.

UPHY2 (GBE) Configuration Options

Lane

gbe-uphy-config-8

gbe-uphy-config-9

0

PCIe x2 (C7), RP

PCIe x1 (C7), RP

1

PCIe x1 (C9), RP

UPHY0/1/2 are called HSIO, NVHS, and GBE respectively. The Orin NX/Nano SOM does not use NVHS, and it is powered down.

The UPHY configuration is specified using the ODMDATA variable in the flash configuration file. The out-of-box configuration as defined in p3767.conf.common looks like this:

ODMDATA="gbe-uphy-config-8,hsstp-lane-map-3,hsio-uphy-config-0";

The GBE configuration and HSIO configuration are independent. For example, to use PCIe C7 and C9 as independent 1-lane controllers, select the gbe-uphy-config-9 configuration by adding a line like the following to your flash configuration file:

ODMDATA="gbe-uphy-config-9,hsstp-lane-map-3,hsio-uphy-config-0";

Do not edit this variable directly in the p3767.conf.common file. You should instead append that variable in your flash configuration file to override the default. To use PCIE C4 as an endpoint, use configuration hsio-uphy-config-40 or hsio-uphy-config-41 above. Refer to PCIE Endpoint Mode for more information.

**ODM Data for T234**

The following table provides information about the ODM data for T234.

31:26

25:23

22:18

17

16

15

14

13:0

HSIO UPHY Config

NVHS UPHY Config

GBE UPHY Config

GBE3 Mode

GBE2 Mode

GBE1 Mode

GBE0 Mode

Reserved

Config Number in HSIO UPHY Lane Mapping Options

Config Number in NVHS UPHY Lane Mapping Options

Config Number in GBE UPHY Lane Mapping Options

0:5G, 1:10G

0:5G, 1:10G

0:5G, 1:10G

0:5G, 1:10G

TBD

BPMP-FW DTB /uphy/hsio-uphy-config

BPMP-FW DTB /uphy/nvhs-uphy-config

BPMP-FW DTB /uphy/gbe-uphy-config

BPMP-FW DTB /uphy/gbe3-enable-10g

BPMP-FW DTB /uphy/gbe2-enable-10g

BPMP-FW DTB /uphy/gbe1-enable-10g

BPMP-FW DTB /uphy/gbe0-enable-10g

TBD

**HSIO UPHY Lane Mapping Options**

Config Number

PLL0

Lane 0

Lane 1

PLL1

Lane 2

Lane 3

PLL2

Lane 4

Lane 5

Lane 6

Lane 7

PLL3

0

Disabled

USB3.1 P0

USB3.1 P1

USB3/PCIE G2

USB3.1 P2

PCIE x1 C1

PCIE G4

PCIE x4 C4

PCIE x4 C4

PCIE x4 C4

PCIE x4 C4

USB3/PCIE G2

**GBE UPHY Lane Mapping Options**

Config Number

PLL0

Lane 0

Lane 1

Lane 2

Lane 3

PLL1

Lane 4

Lane 5

Lane 6

Lane 7

PLL2

8

USB3/PCIE G2

PCIE x2 C7

PCIE x2 C7

PCIE x2 C8

PCIE x2 C8

PCIE G4

PCIE x2 C10

PCIE x2 C10

PCIE x2 C9

PCIE x2 C9

USB3/PCIE G2

9

USB3/PCIE G2

PCIE x1 C7

PCIE x1 C9

PCIE x2 C8

PCIE x2 C8

PCIE G4

PCIE x4 C10

PCIE x4 C10

PCIE x4 C10

PCIE x4 C10

PCIE C10 Only

</details>

## 烧写构建镜像

烧写构建镜像时，要使用你具体的板卡名称。烧写脚本在烧写过程中会使用 <board>.conf 文件中的配置。

**board** 片段示例（其中的值为 dev-kit 风格；你自己的值在 source 公共片段之后位于 **`<board>.conf`**）：

```bash
source "${LDK_DIR}/p3767.conf.common"

PINMUX_CONFIG="tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi"
PMC_CONFIG="tegra234-mb1-bct-padvoltage-p3767-dp-a03.dtsi"
BPFDTB_FILE="tegra234-bpmp-3767-0000-a02-3509-a02.dtb"
DTB_FILE="tegra234-p3767-0000-p3768-0000-a0.dtb"
TBCDTB_FILE="${DTB_FILE}"
EMMC_CFG="flash_t234_qspi_sd.xml"
```

像 `PINMUX_CONFIG` 这样的覆盖项必须出现在 `source ...conf.common` **之后**，这样才能替换默认值。

**NVMe 示例**（把 **`<board>`** 换成你的板卡名称）：

```bash
sudo ./tools/kernel_flash/l4t_initrd_flash.sh --external-device nvme0n1p1 \
  -c tools/kernel_flash/flash_l4t_t234_nvme.xml \
  -p "-c bootloader/generic/cfg/flash_t234_qspi.xml" \
  --showlogs --network usb0 <board> internal
```

更多烧写支持与选项，请参阅 Flashing Support。

UEFI 从 /boot/extlinux/extlinux.conf 文件中提到的 rootfs 路径选取 kernel image 和 dtb。如果你在该文件中指定了 image 和 dtb，则以这些项优先。例如，你可能想用 scp 把文件传到该文件中给出的 Jetson 目标路径。如果未指定该信息，或该文件不存在，则会从存储设备中已烧写的分区里选取 Kernel Image 或 dtb。

## 设置可选环境变量

`flash.sh` / initrd flash helper 会从 **EEPROM** 和 CLI 参数中推导出许多设置。要**强制**指定取值（例如自定义载板上 EEPROM 缺失），请在你的 **board** `.conf` 中设置变量，具体做法见 Jetson Linux **Flashing support** 主题的文档。

可选符号的节选（完整语义见 NVIDIA 指南）：

```text
# Optional Environment Variables:
# BCTFILE ---------------- Boot control table configuration file to be used.
# BOARDID ---------------- Pass boardid to override EEPROM value
# BOARDREV --------------- Pass board_revision to override EEPROM value
# BOARDSKU --------------- Pass board_sku to override EEPROM value
# BOOTLOADER ------------- Bootloader binary to be flashed
# BOOTPARTLIMIT ---------- GPT data limit. (== Max BCT size + PPT size)
# BOOTPARTSIZE ----------- Total eMMC HW boot partition size.
# CFGFILE ---------------- Partition table configuration file to be used.
# CMDLINE ---------------- Target cmdline. See help for more information.
# DEVSECTSIZE ------------ Device Sector size. (default = 512Byte).
# DTBFILE ---------------- Device Tree file to be used.
# EMMCSIZE --------------- Size of target device eMMC (boot0+boot1+user).
# FLASHAPP --------------- Flash application running in host machine.
# FLASHER ---------------- Flash server running in target machine.
# INITRD ----------------- Initrd image file to be flashed.
# KERNEL_IMAGE ----------- Linux kernel zImage file to be flashed.
# MTS -------------------- MTS file name such as mts_si.
# MTSPREBOOT ------------- MTS preboot file name such as mts_preboot_si.
# NFSARGS ---------------- Static Network assignments.
#                          <C-ipa>:<S-ipa>:<G-ipa>:<netmask>
# NFSROOT ---------------- NFSROOT i.e. <my IP addr>:/exported/rootfs_dir.
# ODMDATA ---------------- Odmdata to be used.
# PKCKEY ----------------- RSA key file to use to sign bootloader images.
# ROOTFSSIZE ------------- Linux RootFS size (internal emmc/nand only).
# ROOTFS_DIR ------------- Linux RootFS directory name.
# SBKKEY ----------------- SBK key file to use to encrypt bootloader images.
# SCEFILE ---------------- SCE firmware file such as camera-rtcpu-sce.img.
# SPEFILE ---------------- SPE firmware file path such as bootloader/spe.bin.
# FAB -------------------- Target board's FAB ID.
# TEGRABOOT -------------- lowerlayer bootloader such as nvtboot.bin.
# WB0BOOT ---------------- Warmboot code such as nvtbootwb0.bin
```

## HDMI 支持

Orin **NX** 与 **Nano** 模块既能驱动 **HDMI**，也能驱动 **DP**；参见你所用版本的 [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) 中的 **HDMI** 章节。你既可以在自己的载板上设计 **HDMI**，也可以使用诸如 **Xavier NX** 套件这类引出 HDMI 的载板。NVIDIA 针对该组合的示例 flash 配置为 **`p3509-a02-p3767-0000.conf`**。

例如，烧写支持 HDMI 的设备：


```bash
$ sudo ./tools/kernel_flash/l4t_initrd_flash.sh --external-device nvme0n1p1 \
-c tools/kernel_flash/flash_l4t_t234_nvme.xml -p "-c bootloader/generic/cfg/flash_t234_qspi.xml" \
--showlogs --network usb0 p3509-a02-p3767-0000 internal
```


<details>
<summary>English original</summary>

**Flashing the Build Image**

When flashing the build image, use your specific board name. The flashing script uses the configuration in the <board>.conf file during the flashing process.

Example **board** snippet (values are dev-kit style; yours live in **`<board>.conf`** after sourcing the common fragment):

```bash
source "${LDK_DIR}/p3767.conf.common"

PINMUX_CONFIG="tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi"
PMC_CONFIG="tegra234-mb1-bct-padvoltage-p3767-dp-a03.dtsi"
BPFDTB_FILE="tegra234-bpmp-3767-0000-a02-3509-a02.dtb"
DTB_FILE="tegra234-p3767-0000-p3768-0000-a0.dtb"
TBCDTB_FILE="${DTB_FILE}"
EMMC_CFG="flash_t234_qspi_sd.xml"
```

Overrides like `PINMUX_CONFIG` must appear **after** `source ...conf.common` so they replace defaults.

**NVMe example** (replace **`<board>`** with your board name):

```bash
sudo ./tools/kernel_flash/l4t_initrd_flash.sh --external-device nvme0n1p1 \
  -c tools/kernel_flash/flash_l4t_t234_nvme.xml \
  -p "-c bootloader/generic/cfg/flash_t234_qspi.xml" \
  --showlogs --network usb0 <board> internal
```

For more flashing support and options, refer to Flashing Support.

UEFI picks the kernel image and dtb from the rootfs path that is mentioned in /boot/extlinux/extlinux.conf file. If you mentioned an image and dtb in this file, these items will be given precedence. For example, you might want to scp the file to the Jetson target path in the file. If this information is not mentioned, or the file is not present, the Kernel Image or dtb will be selected from the flashed partition in the storage device.

**Setting Optional Environmental Variables**

`flash.sh` / initrd flash helpers derive many settings from **EEPROM** and CLI args. To **force** values (e.g. missing EEPROM on a custom carrier), set variables in your **board** `.conf` as documented in the Jetson Linux **Flashing support** topic.

Excerpt of optional symbols (see NVIDIA guide for full semantics):

```text
# Optional Environment Variables:
# BCTFILE ---------------- Boot control table configuration file to be used.
# BOARDID ---------------- Pass boardid to override EEPROM value
# BOARDREV --------------- Pass board_revision to override EEPROM value
# BOARDSKU --------------- Pass board_sku to override EEPROM value
# BOOTLOADER ------------- Bootloader binary to be flashed
# BOOTPARTLIMIT ---------- GPT data limit. (== Max BCT size + PPT size)
# BOOTPARTSIZE ----------- Total eMMC HW boot partition size.
# CFGFILE ---------------- Partition table configuration file to be used.
# CMDLINE ---------------- Target cmdline. See help for more information.
# DEVSECTSIZE ------------ Device Sector size. (default = 512Byte).
# DTBFILE ---------------- Device Tree file to be used.
# EMMCSIZE --------------- Size of target device eMMC (boot0+boot1+user).
# FLASHAPP --------------- Flash application running in host machine.
# FLASHER ---------------- Flash server running in target machine.
# INITRD ----------------- Initrd image file to be flashed.
# KERNEL_IMAGE ----------- Linux kernel zImage file to be flashed.
# MTS -------------------- MTS file name such as mts_si.
# MTSPREBOOT ------------- MTS preboot file name such as mts_preboot_si.
# NFSARGS ---------------- Static Network assignments.
#                          <C-ipa>:<S-ipa>:<G-ipa>:<netmask>
# NFSROOT ---------------- NFSROOT i.e. <my IP addr>:/exported/rootfs_dir.
# ODMDATA ---------------- Odmdata to be used.
# PKCKEY ----------------- RSA key file to use to sign bootloader images.
# ROOTFSSIZE ------------- Linux RootFS size (internal emmc/nand only).
# ROOTFS_DIR ------------- Linux RootFS directory name.
# SBKKEY ----------------- SBK key file to use to encrypt bootloader images.
# SCEFILE ---------------- SCE firmware file such as camera-rtcpu-sce.img.
# SPEFILE ---------------- SPE firmware file path such as bootloader/spe.bin.
# FAB -------------------- Target board's FAB ID.
# TEGRABOOT -------------- lowerlayer bootloader such as nvtboot.bin.
# WB0BOOT ---------------- Warmboot code such as nvtbootwb0.bin
```

**HDMI Support**

Orin **NX** and **Nano** modules can drive **HDMI** as well as **DP**; see the **HDMI** section in the [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) for your release. You can either design **HDMI** on your carrier or use a carrier such as the **Xavier NX** kit that exposes HDMI. NVIDIA’s example flash config for that pairing is **`p3509-a02-p3767-0000.conf`**.

For example, flash the device with HDMI support:


```bash
$ sudo ./tools/kernel_flash/l4t_initrd_flash.sh --external-device nvme0n1p1 \
-c tools/kernel_flash/flash_l4t_t234_nvme.xml -p "-c bootloader/generic/cfg/flash_t234_qspi.xml" \
--showlogs --network usb0 p3509-a02-p3767-0000 internal
```

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/3. L4T Customization/Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/3.%20L4T%20Customization/Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
