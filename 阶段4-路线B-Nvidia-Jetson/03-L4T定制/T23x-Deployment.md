---
title: T23x Boot Configuration Table (BCT) — 部署参考
description: T23x Boot Configuration Table (BCT) — 部署参考
---

# T23x Boot Configuration Table (BCT) — 部署参考

**路线图重点：** Jetson **Orin Nano** 与 **Orin NX**（P3767 模块系列），SoC **T234**（tegra234）。本文正文总体遵循 NVIDIA **DU-10990**（`<platform>` 占位符与混合示例）。Orin Nano 专有的路径与 flash 变量，见 [Orin Nano: BCT path cheat sheet](#orin-nano-bct-path-cheat-sheet)。

**来源：** *T23x BCT Deployment Guide*（DU-10990-001），2022 年 6 月，为本文路线图重新排版。`flash.sh` / `tegrabct_v2` 的细节，以及随版本变化的路径，见 [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/)，对应你的 JetPack / L4T 版本。

## 概览

| 术语 | 含义 |
|------|---------|
| **BCT** | Linux 之前由 BootROM、MB1 和 MB2 读取的早期启动 **二进制**，用于配置引脚、电源轨、存储、UPHY、SDRAM 与安全设置。 |
| **T23x** | tegra234 级别的芯片：Orin Nano、Orin NX、AGX Orin、… |
| **输入格式** | 设备树（`.dtsi` / `.dts`），用 `tegrabct_v2` 构建。旧式 `parameter = value` `.cfg` 在本文档中以 **Legacy `.cfg`** 出现，与 **NEW DTS** 示例并列。 |

## Orin Nano：BCT 路径速查表

### 硬件 ID

| ID | 说明 |
|----|-------------|
| **P3767** | Orin Nano / Orin NX 模块（SOM）；BCT 名称中常包含 `p3767`。 |
| **P3768** | Orin Nano 开发者套件载板（CVB）；kernel DT 常为 `tegra234-p3768-0000+p3767-xxxx`。 |
| **P3766** | Orin Nano 开发者套件 SKU（模块 + 载板）。 |

### BCT 源码在主机上的位置

| 路径 | 作用 |
|------|------|
| `Linux_for_Tegra/bootloader/generic/BCT/` | 当前 Orin Nano 流程的主 MB1/MB2 BCT `.dts` / `.dtsi` 输入（pinmux、pad 电压、PMIC、GPIO intmap、MB2 misc、…）。 |
| `Linux_for_Tegra/bootloader/t186ref/BCT/` | 某些 L4T 版本上额外的 tegra234 MB1 片段（pinmux、gpioint `.dtsi`）。搜索 `tegra234-mb1-bct-*p3767*`。 |
| `Linux_for_Tegra/bootloader/` | `tegra234-firewall-config-base.dtsi` 以及 MB2 相关的 SCR/firewall 源码。 |

### 示例片段（请在 BSP tarball 中核实）

| 主题 | 示例 |
|-------|---------|
| Pinmux | `tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi` |
| Pad 电压 | `tegra234-mb1-bct-padvoltage-p3767-dp-a03.dtsi` |
| MB1 GPIO | `tegra234-mb1-bct-gpio-p3767-dp-a03.dtsi` |
| GPIO 中断映射 | `tegra234-mb1-bct-gpioint-p3767-0000.dts` |
| MB2 misc | `tegra234-mb2-bct-misc-p3767-0000.dts` |
| MB2 SCR（载板） | `tegra234-mb2-bct-scr-p3767-0000.dts` |

### Flash 配置

板级 `.conf` 文件通常 `source "${LDK_DIR}/p3767.conf.common"`，并把 `PINMUX_CONFIG`、`PMC_CONFIG`、`GPIOINT_CONFIG`、`MB2_BCT`、`SCR_CONFIG`、`DTB_FILE` 等设置为与上面片段类似的值。分步 bring-up（上电点亮/调通）：[Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md)（MB1 修改、无 EEPROM 载板、`ODMDATA` / UPHY）。

### 相关笔记

- [ODMDATA-and-GPIO-Jetson-Linux.md](ODMDATA-and-GPIO-Jetson-Linux.md) — UPHY、pinmux 与用户态 GPIO  
- [Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md](Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md) — 定制载板流程  
- [FSP / SPE firmware](../04-FSP固件支持包定制/Guide.md) — 涉及 `p3767` BCT 的 SPE demo

## 文件所在位置（通用 + 上游）

| 位置 | 作用 |
|----------|------|
| `Linux_for_Tegra/bootloader/`（`generic/BCT/`、`t186ref/BCT/`） | Orin Nano：[cheat sheet](#orin-nano-bct-path-cheat-sheet)。其他 T23x 板使用其他 `p####` 前缀（例如 AGX Orin 参考载板上的 p3701）。 |
| `Linux_for_Tegra/sources/hardware/nvidia/platform/t23x/.../bct/` | `source_sync.sh` / kernel BSP 之后的上游目录布局；与 DU-10990 示例一致。 |
| DU-10990 中的 `<platform>` | PDF 中的 Board/CVB 目录名；对 Nano 而言，可视为 `p3767` / `p3768`。 |

**流程**（电子表格 → `.dtsi`、flash、PCIe/USB）见 [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md)；**语义**（每个 BCT 章节可包含什么）见**本文档**。

## 如何阅读本参考

1. 阅读 **第 1 章**，了解哪个产物（BR-BCT、MB1-BCT、Mem-BCT、MB2-BCT）承载哪些数据。
2. 按任务（pinmux、PMIC、存储、UPHY、SDRAM、GPIO intmap、SCR）经[目录](#table-of-contents)跳转。
3. 把冗长的 **DTS** 摘录当作 **PDF 插图**；在交付设备树之前，从 pinmux 电子表格重新排版或重新生成。

### 代码约定

- `dts` 围栏代码块：设备树示例（可能带有来自 PDF 的错误换行）。
- `text` 围栏代码块：旧式 `.cfg` 风格（`device.foo.bar = value`）。
- 较早文本中未加围栏的 `/ { … }` 片段：仅为结构模板，不是完整文件。


<details>
<summary>English original</summary>

**T23x Boot Configuration Table (BCT) — deployment reference**

**Roadmap focus:** Jetson **Orin Nano** and **Orin NX** (P3767 module family), SoC **T234** (tegra234). The body of this file follows NVIDIA **DU-10990** generically (`<platform>` placeholders and mixed examples). For Orin Nano–specific paths and flash variables, use [Orin Nano: BCT path cheat sheet](#orin-nano-bct-path-cheat-sheet).

**Source:** *T23x BCT Deployment Guide* (DU-10990-001), June 2022, reformatted for this roadmap. For `flash.sh` / `tegrabct_v2` details and paths that change by release, use the [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) for your JetPack / L4T version.

**At a glance**

| Term | Meaning |
|------|---------|
| **BCT** | Early-boot **binary** consumed by BootROM, MB1, and MB2 to configure pins, power rails, storage, UPHY, SDRAM, and security before Linux. |
| **T23x** | tegra234-class silicon: Orin Nano, Orin NX, AGX Orin, … |
| **Input format** | Device tree (`.dtsi` / `.dts`) built with `tegrabct_v2`. Legacy `parameter = value` `.cfg` appears in this doc as **Legacy `.cfg`** next to **NEW DTS** examples. |

**Orin Nano: BCT path cheat sheet**

**Hardware IDs**

| ID | Description |
|----|-------------|
| **P3767** | Orin Nano / Orin NX module (SOM); BCT names often include `p3767`. |
| **P3768** | Orin Nano Developer Kit carrier (CVB); kernel DT often `tegra234-p3768-0000+p3767-xxxx`. |
| **P3766** | Orin Nano Developer Kit SKU (module + carrier). |

**Where BCT sources live on the host**

| Path | Role |
|------|------|
| `Linux_for_Tegra/bootloader/generic/BCT/` | Main MB1/MB2 BCT `.dts` / `.dtsi` inputs for current Orin Nano flows (pinmux, pad voltage, PMIC, GPIO intmap, MB2 misc, …). |
| `Linux_for_Tegra/bootloader/t186ref/BCT/` | Extra tegra234 MB1 fragments on some L4T versions (pinmux, gpioint `.dtsi`). Search for `tegra234-mb1-bct-*p3767*`. |
| `Linux_for_Tegra/bootloader/` | `tegra234-firewall-config-base.dtsi` and related SCR/firewall sources for MB2. |

**Example fragments (verify in your BSP tarball)**

| Topic | Example |
|-------|---------|
| Pinmux | `tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi` |
| Pad voltage | `tegra234-mb1-bct-padvoltage-p3767-dp-a03.dtsi` |
| MB1 GPIO | `tegra234-mb1-bct-gpio-p3767-dp-a03.dtsi` |
| GPIO interrupt map | `tegra234-mb1-bct-gpioint-p3767-0000.dts` |
| MB2 misc | `tegra234-mb2-bct-misc-p3767-0000.dts` |
| MB2 SCR (carrier) | `tegra234-mb2-bct-scr-p3767-0000.dts` |

**Flash configuration**

Board `.conf` files usually `source "${LDK_DIR}/p3767.conf.common"` and set `PINMUX_CONFIG`, `PMC_CONFIG`, `GPIOINT_CONFIG`, `MB2_BCT`, `SCR_CONFIG`, `DTB_FILE`, and similar to the fragments above. Step-by-step bring-up: [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) (MB1 edits, EEPROM-less carrier, `ODMDATA` / UPHY).

**Related notes**

- [ODMDATA-and-GPIO-Jetson-Linux.md](ODMDATA-and-GPIO-Jetson-Linux.md) — UPHY, pinmux vs userspace GPIO  
- [Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md](Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md) — custom carrier flow  
- [FSP / SPE firmware](../04-FSP固件支持包定制/Guide.md) — SPE demos that touch `p3767` BCT

**Where files live (generic + upstream)**

| Location | Role |
|----------|------|
| `Linux_for_Tegra/bootloader/` (`generic/BCT/`, `t186ref/BCT/`) | Orin Nano: [cheat sheet](#orin-nano-bct-path-cheat-sheet). Other T23x boards use other `p####` prefixes (e.g. p3701 on AGX Orin reference carrier). |
| `Linux_for_Tegra/sources/hardware/nvidia/platform/t23x/.../bct/` | Upstream layout after `source_sync.sh` / kernel BSP; matches DU-10990 examples. |
| `<platform>` in DU-10990 | Board/CVB directory name in the PDF; for Nano, think `p3767` / `p3768`. |

Use [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) for **procedure** (spreadsheet → `.dtsi`, flash, PCIe/USB). Use **this file** for **semantics** (what each BCT chapter can contain).

**How to read this reference**

1. Read **Chapter 1** to see which artifact (BR-BCT, MB1-BCT, Mem-BCT, MB2-BCT) owns which data.
2. Jump by task (pinmux, PMIC, storage, UPHY, SDRAM, GPIO intmap, SCR) via the [table of contents](#table-of-contents).
3. Treat long **DTS** excerpts as **PDF illustrations**; reformat or regenerate from the pinmux spreadsheet before you ship a tree.

**Code conventions**

- `dts` fenced blocks: device tree examples (may have bad line wraps from the PDF).
- `text` fenced blocks: legacy `.cfg` style (`device.foo.bar = value`).
- Unfenced `/ { … }` snippets in older text: shape templates only, not complete files.

</details>

## 快速对照：哪个 BCT 由谁消费

| 产物 | 加载者 | 作用 |
|----------|-----------|------|
| **BR-BCT** | BootROM（冷启动）；recovery 中的 MB1 | BootROM 优先；部分字段供 MB1 / CPU-BL 使用 |
| **MB1-BCT** | MB1 从存储或 USB（RCM）加载 | 引脚复用、prod、pad 电压、PMIC、存储、UPHY、ratchet、… |
| **Mem-BCT** | 与 MB1-BCT 相同路径 | SDRAM / MC–EMC bring-up（上电点亮/调通） |
| **MB2-BCT** | MB1 为 MB2 加载 | GPIO 中断映射、security SCR、MB2 杂项 |

## 目录

0. [Orin Nano：BCT 路径速查表](#orin-nano-bct-path-cheat-sheet)
1. [第 1 章 — 引言](#chapter-1-introduction)
2. [第 2 章 — 引脚复用与 GPIO 配置](#chapter-2-pinmux-and-gpio-configuration)
3. [第 3 章 — 通用 Prod 配置](#chapter-3-common-prod-configuration)
4. [第 4 章 — 控制器 Prod 配置](#chapter-4-controller-prod-configuration)
5. [第 5 章 — Pad 电压绑定](#chapter-5-pad-voltage-binding)
6. [第 6 章 — PMIC 配置](#chapter-6-pmic-configuration)
7. [第 7 章 — 存储设备配置](#chapter-7-storage-device-configuration)
8. [第 8 章 — UPHY Lane 配置](#chapter-8-uphy-lane-configuration)
9. [第 9 章 — OEM FW Ratchet 配置](#chapter-9-oem-fw-ratchet-configuration)
10. [第 10 章 — BootROM 复位 PMIC 配置](#chapter-10-bootrom-reset-pmic-configuration)
11. [第 11 章 — 杂项配置](#chapter-11-miscellaneous-configuration)
12. [第 12 章 — SDRAM 配置](#chapter-12-sdram-configuration)
13. [第 13 章 — GPIO 中断映射配置](#chapter-13-gpio-interrupt-mapping-configuration)
14. [第 14 章 — MB2 BCT 杂项配置](#chapter-14-mb2-bct-misc-configuration)
15. [第 15 章 — 安全配置](#chapter-15-security-configuration)

---

## 第 1 章 引言

**Boot Configuration Table (BCT)** 是早期启动阶段的平台数据。BootROM 与 MB1 消费的 **二进制** BCT，由 Device Tree Source（`.dts` / `.dtsi`）经 `tegrabct_v2` 生成。更早的版本则改用扁平的 `parameter = value` `.cfg` 文件。

在 T23x 上，NVIDIA 将 DTS 统一为 BCT 的输入格式，原因如下：

- 设备树能干净地支持 `#include` 与属性覆盖。
- 工具链可以复刻 DTC 风格的工作流，且该结构比遗留的 CFG 更易于 diff。

### 1.1 BR-BCT

**BR-BCT** 在冷启动时由 **BootROM** 从存储加载，在 recovery 中由 **MB1** 加载。BootROM 是主要消费者；部分字段也由 **MB1** 与 **CPU-BL** 使用。

### 1.2 MB1-BCT

**MB1 BCT** 由 **MB1** 从存储（冷启动）或经 **USB**（RCM）加载。MB1 是主要消费者。它由若干配置主题构成（参见本文件各章）：

- [第 2 章 — 引脚复用与 GPIO 配置](#chapter-2-pinmux-and-gpio-configuration)
- [第 3 章 — 通用 Prod 配置](#chapter-3-common-prod-configuration)
- [第 4 章 — 控制器 Prod 配置](#chapter-4-controller-prod-configuration)
- [第 5 章 — Pad 电压绑定](#chapter-5-pad-voltage-binding)
- [第 6 章 — PMIC 配置](#chapter-6-pmic-configuration)
- [第 7 章 — 存储设备配置](#chapter-7-storage-device-configuration)
- [第 8 章 — UPHY Lane 配置](#chapter-8-uphy-lane-configuration)
- [第 9 章 — OEM FW Ratchet 配置](#chapter-9-oem-fw-ratchet-configuration)
- [第 10 章 — BootROM 复位 PMIC 配置](#chapter-10-bootrom-reset-pmic-configuration)
- [第 11 章 — 杂项配置](#chapter-11-miscellaneous-configuration)

### 1.3 Mem-BCT

**Mem-BCT** 的加载与使用方式与 MB1-BCT 相同，但它主要承载供 **MC** 与 **EMC** 初始化使用的 **SDRAM** 参数。参见 [第 12 章 — SDRAM 配置](#chapter-12-sdram-configuration)。

### 1.4 MB2-BCT

**MB2 BCT** 由 **MB1**（存储或 RCM）为 **MB2** 加载。它由如下主题构成：

- [第 13 章 — GPIO 中断映射配置](#chapter-13-gpio-interrupt-mapping-configuration)
- [第 15 章 — 安全配置](#chapter-15-security-configuration)
- [第 14 章 — MB2 BCT 杂项配置](#chapter-14-mb2-bct-misc-configuration)


<details>
<summary>English original</summary>

**Quick map: which BCT, who consumes it**

| Artifact | Loaded by | Role |
|----------|-----------|------|
| **BR-BCT** | BootROM (cold boot); MB1 in recovery | BootROM-first; some fields for MB1 / CPU-BL |
| **MB1-BCT** | MB1 from storage or USB (RCM) | Pinmux, prod, pad voltage, PMIC, storage, UPHY, ratchet, … |
| **Mem-BCT** | Same path as MB1-BCT | SDRAM / MC–EMC bring-up |
| **MB2-BCT** | MB1 loads for MB2 | GPIO interrupt map, security SCRs, MB2 misc |

**Table of contents**

0. [Orin Nano: BCT path cheat sheet](#orin-nano-bct-path-cheat-sheet)
1. [Chapter 1 — Introduction](#chapter-1-introduction)
2. [Chapter 2 — Pinmux and GPIO Configuration](#chapter-2-pinmux-and-gpio-configuration)
3. [Chapter 3 — Common Prod Configuration](#chapter-3-common-prod-configuration)
4. [Chapter 4 — Controller Prod Configuration](#chapter-4-controller-prod-configuration)
5. [Chapter 5 — Pad Voltage Binding](#chapter-5-pad-voltage-binding)
6. [Chapter 6 — PMIC Configuration](#chapter-6-pmic-configuration)
7. [Chapter 7 — Storage Device Configuration](#chapter-7-storage-device-configuration)
8. [Chapter 8 — UPHY Lane Configuration](#chapter-8-uphy-lane-configuration)
9. [Chapter 9 — OEM FW Ratchet Configuration](#chapter-9-oem-fw-ratchet-configuration)
10. [Chapter 10 — BootROM Reset PMIC Configuration](#chapter-10-bootrom-reset-pmic-configuration)
11. [Chapter 11 — Miscellaneous Configuration](#chapter-11-miscellaneous-configuration)
12. [Chapter 12 — SDRAM Configuration](#chapter-12-sdram-configuration)
13. [Chapter 13 — GPIO Interrupt Mapping Configuration](#chapter-13-gpio-interrupt-mapping-configuration)
14. [Chapter 14 — MB2 BCT Misc Configuration](#chapter-14-mb2-bct-misc-configuration)
15. [Chapter 15 — Security Configuration](#chapter-15-security-configuration)

---

**Chapter 1. Introduction**

A **Boot Configuration Table (BCT)** is early-boot platform data. BootROM and MB1 consume a **binary** BCT produced from Device Tree Source (`.dts` / `.dtsi`) with `tegrabct_v2`. Older releases used flat `parameter = value` `.cfg` files instead.

On T23x, NVIDIA standardized on DTS for BCT input because:

- The tree supports `#include` and property overrides cleanly.
- Tooling can mirror DTC-style workflows, and the structure is easier to diff than legacy CFG.

**1.1 BR-BCT**

**BR-BCT** is loaded by **BootROM** from storage on cold boot, and by **MB1** in recovery. BootROM is the main consumer; some fields are also used by **MB1** and **CPU-BL**.

**1.2 MB1-BCT**

**MB1 BCT** is loaded by **MB1** from storage (cold boot) or over **USB** (RCM). MB1 is the main consumer. It is built from several configuration topics (see the chapters in this file):

- [Chapter 2 — Pinmux and GPIO Configuration](#chapter-2-pinmux-and-gpio-configuration)
- [Chapter 3 — Common Prod Configuration](#chapter-3-common-prod-configuration)
- [Chapter 4 — Controller Prod Configuration](#chapter-4-controller-prod-configuration)
- [Chapter 5 — Pad Voltage Binding](#chapter-5-pad-voltage-binding)
- [Chapter 6 — PMIC Configuration](#chapter-6-pmic-configuration)
- [Chapter 7 — Storage Device Configuration](#chapter-7-storage-device-configuration)
- [Chapter 8 — UPHY Lane Configuration](#chapter-8-uphy-lane-configuration)
- [Chapter 9 — OEM FW Ratchet Configuration](#chapter-9-oem-fw-ratchet-configuration)
- [Chapter 10 — BootROM Reset PMIC Configuration](#chapter-10-bootrom-reset-pmic-configuration)
- [Chapter 11 — Miscellaneous Configuration](#chapter-11-miscellaneous-configuration)

**1.3 Mem-BCT**

**Mem-BCT** is loaded and used like MB1-BCT, but it mainly carries **SDRAM** parameters for **MC** and **EMC** init. See [Chapter 12 — SDRAM Configuration](#chapter-12-sdram-configuration).

**1.4 MB2-BCT**

**MB2 BCT** is loaded by **MB1** (storage or RCM) for **MB2**. It is built from topics such as:

- [Chapter 13 — GPIO Interrupt Mapping Configuration](#chapter-13-gpio-interrupt-mapping-configuration)
- [Chapter 15 — Security Configuration](#chapter-15-security-configuration)
- [Chapter 14 — MB2 BCT Misc Configuration](#chapter-14-mb2-bct-misc-configuration)

</details>

## 第 2 章 Pinmux 与 GPIO 配置

pinmux 文件描述 **pinmux** 与 **GPIO**；内容通常由 NVIDIA 的 **pinmux spreadsheet** 生成。**DTS** 的形态与旧 **CFG** 布局差别很大，因为 spreadsheet 的输出面向设备树。

Pinmux `.dtsi` 文件位于：

`hardware/nvidia/platform/t23x/<platform>/bct/`

在 Orin Nano 上，把 **`<platform>`** 映射到 `Linux_for_Tegra` 下的 `p3767` / `p3768` 文件名（例如 `tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi`），它可能与上游的 `platform/t23x/.../bct/` 目录字符串不一致。参见 [Orin Nano：BCT 路径速查表](#orin-nano-bct-path-cheat-sheet)。

> **注意：** 下面这段大段 **NEW DTS** 摘录复制自 NVIDIA 的指南，可能出现**断行错乱**（例如 `TEGRA_PIN_PULL_` 被拆到多行）。用它来识别**结构**（`pinmux@…`、`nvidia,pins`、`nvidia,function`）；实际产品中，应从 **pinmux spreadsheet** **重新生成**，或在编辑器中**重新格式化**。

**pinmux 配置文件的 NEW DTS 格式示例：**

```dts
/* This dtsi file was generated by e3360_1099_slt_a01.xlsm Revision: 126 */
#include <dt-bindings/pinctrl/pinctrl-tegra.h>
/ {
pinmux@2430000 {
pinctrl-names =
"default", "drive",
"unused"; pinctrl-0 =
<&pinmux_default>;
pinctrl-1 = <&drive_default>;
pinctrl-2 = <&pinmux_unused_lowpower>;
pinmux_default: common {
/* SFIO Pin
Configuration */
dap1_sclk_ps0 {
nvidia,pins =
"dap1_sclk_ps0";
nvidia,function
= "i2s1";
nvidia,pull =
<TEGRA_PIN_PULL_
NONE>;
nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
dap1_dout_ps1 {
nvidia,pins =
"dap1_dout_ps1";
nvidia,function
= "i2s1";
nvidia,pull =
<TEGRA_PIN_PULL_
NONE>;
nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
dap1_din_ps2 {
nvidia,pins =
"dap1_din_ps2";
nvidia,function
= "i2s1";
nvidia,pull =
<TEGRA_PIN_PULL_
DOWN>;
nvidia,tristate =
<TEGRA_PIN_ENABLE>;
nvidia,enable-input
=
<TEGRA_PIN_ENABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
dap1_fs_ps3 {
nvidia,pins =
"dap1_fs_ps3";
nvidia,function
= "i2s1";
nvidia,pull =
<TEGRA_PIN_PULL_
NONE>;
nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
aud_mclk_ps4 {
nvidia,pins =
"aud_mclk_ps4";
nvidia,function =
"aud";
nvidia,pull =
<TEGRA_PIN_PULL_NONE>
; nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
soc_gpio31_ps6 {
nvidia,pins =
"soc_gpio31_ps6";
nvidia,function =
"sdmmc1";
nvidia,pull =
<TEGRA_PIN_PULL_U
P>;
nvidia,tristate =
<TEGRA_PIN_ENABLE
>;
nvidia,enable-input =
<TEGRA_PIN_ENABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
soc_gpio32_ps7 {
nvidia,pins =
"soc_gpio32_ps7";
nvidia,function =
"spdif";
nvidia,pull =
<TEGRA_PIN_PULL_D
OWN>;
nvidia,tristate =
<TEGRA_PIN_ENABLE
>;
nvidia,enable-input =
<TEGRA_PIN_ENABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
soc_gpio33_pt0 {
nvidia,pins =
"soc_gpio33_pt0"
;
nvidia,function
= "spdif";
nvidia,pull =
<TEGRA_PIN_PULL_
NONE>;
nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
};
drive_default: drive {
};
};
};
```

**旧式 `.cfg` 格式（DTS 之前）：**

```text
//////// Pinmux for used pins ////////
pinmux.0x02434060 = <value1>; //
gen1_i2c_scl_pc5.PADCTL_CONN_GEN1_I2C_SCL_0 pinmux.0x02434064 =
<value2>; // gen1_i2c_scl_pc5.PADCTL_CONN_CFG2TMC_GEN1_I2C_SCL_0
pinmux.0x02434068 = <value1>; //
gen1_i2c_sda_pc6.PADCTL_CONN_GEN1_I2C_SDA_0 pinmux.0x0243406C =
<value2>; // gen1_i2c_sda_pc6.PADCTL_CONN_CFG2TMC_GEN1_I2C_DA_0
//////// Pinmux for unused pins for low-power
configuration //////// pinmux.0x02434040 = <value1>;
// gpio_wan4_ph0.PADCTL_CONN_GPIO_WAN4_0
pinmux.0x02434044 = <value2>; //
gpio_wan4_ph0.PADCTL_CONN_CFG2TMC_GPIO_WAN4_0
pinmux.0x02434048 = <value1>; //
gpio_wan3_ph1.PADCTL_CONN_GPIO_WAN3_0
pinmux.0x0243404C = <value2>; //
gpio_wan3_ph1.PADCTL_CONN_CFG2TMC_GPIO_WAN3_0
```


<details>
<summary>English original</summary>

**Chapter 2. Pinmux and GPIO Configuration**

The pinmux file describes **pinmux** and **GPIO**; content is normally generated from NVIDIA’s **pinmux spreadsheet**. The **DTS** shape differs a lot from the old **CFG** layout because the spreadsheet output targets device tree.

Pinmux `.dtsi` files live under:

`hardware/nvidia/platform/t23x/<platform>/bct/`

On Orin Nano, map **`<platform>`** to the `p3767` / `p3768` file names under `Linux_for_Tegra` (e.g. `tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi`), which may not match the upstream `platform/t23x/.../bct/` directory string. See [Orin Nano: BCT path cheat sheet](#orin-nano-bct-path-cheat-sheet).

> **Note:** The large **NEW DTS** excerpt below is copied from NVIDIA’s guide and may show **broken line wraps** (e.g. `TEGRA_PIN_PULL_` split across lines). Use it to recognize **structure** (`pinmux@…`, `nvidia,pins`, `nvidia,function`); for real products, **regenerate** from the **pinmux spreadsheet** or **reformat** in your editor.

**NEW DTS format example of pinmux configuration file:**

```dts
/* This dtsi file was generated by e3360_1099_slt_a01.xlsm Revision: 126 */
#include <dt-bindings/pinctrl/pinctrl-tegra.h>
/ {
pinmux@2430000 {
pinctrl-names =
"default", "drive",
"unused"; pinctrl-0 =
<&pinmux_default>;
pinctrl-1 = <&drive_default>;
pinctrl-2 = <&pinmux_unused_lowpower>;
pinmux_default: common {
/* SFIO Pin
Configuration */
dap1_sclk_ps0 {
nvidia,pins =
"dap1_sclk_ps0";
nvidia,function
= "i2s1";
nvidia,pull =
<TEGRA_PIN_PULL_
NONE>;
nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
dap1_dout_ps1 {
nvidia,pins =
"dap1_dout_ps1";
nvidia,function
= "i2s1";
nvidia,pull =
<TEGRA_PIN_PULL_
NONE>;
nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
dap1_din_ps2 {
nvidia,pins =
"dap1_din_ps2";
nvidia,function
= "i2s1";
nvidia,pull =
<TEGRA_PIN_PULL_
DOWN>;
nvidia,tristate =
<TEGRA_PIN_ENABLE>;
nvidia,enable-input
=
<TEGRA_PIN_ENABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
dap1_fs_ps3 {
nvidia,pins =
"dap1_fs_ps3";
nvidia,function
= "i2s1";
nvidia,pull =
<TEGRA_PIN_PULL_
NONE>;
nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
aud_mclk_ps4 {
nvidia,pins =
"aud_mclk_ps4";
nvidia,function =
"aud";
nvidia,pull =
<TEGRA_PIN_PULL_NONE>
; nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
soc_gpio31_ps6 {
nvidia,pins =
"soc_gpio31_ps6";
nvidia,function =
"sdmmc1";
nvidia,pull =
<TEGRA_PIN_PULL_U
P>;
nvidia,tristate =
<TEGRA_PIN_ENABLE
>;
nvidia,enable-input =
<TEGRA_PIN_ENABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
soc_gpio32_ps7 {
nvidia,pins =
"soc_gpio32_ps7";
nvidia,function =
"spdif";
nvidia,pull =
<TEGRA_PIN_PULL_D
OWN>;
nvidia,tristate =
<TEGRA_PIN_ENABLE
>;
nvidia,enable-input =
<TEGRA_PIN_ENABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
soc_gpio33_pt0 {
nvidia,pins =
"soc_gpio33_pt0"
;
nvidia,function
= "spdif";
nvidia,pull =
<TEGRA_PIN_PULL_
NONE>;
nvidia,tristate =
<TEGRA_PIN_DISABLE>;
nvidia,enable-input =
<TEGRA_PIN_DISABLE>;
nvidia,lpdr =
<TEGRA_PIN_DISABLE>;
};
};
drive_default: drive {
};
};
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
//////// Pinmux for used pins ////////
pinmux.0x02434060 = <value1>; //
gen1_i2c_scl_pc5.PADCTL_CONN_GEN1_I2C_SCL_0 pinmux.0x02434064 =
<value2>; // gen1_i2c_scl_pc5.PADCTL_CONN_CFG2TMC_GEN1_I2C_SCL_0
pinmux.0x02434068 = <value1>; //
gen1_i2c_sda_pc6.PADCTL_CONN_GEN1_I2C_SDA_0 pinmux.0x0243406C =
<value2>; // gen1_i2c_sda_pc6.PADCTL_CONN_CFG2TMC_GEN1_I2C_DA_0
//////// Pinmux for unused pins for low-power
configuration //////// pinmux.0x02434040 = <value1>;
// gpio_wan4_ph0.PADCTL_CONN_GPIO_WAN4_0
pinmux.0x02434044 = <value2>; //
gpio_wan4_ph0.PADCTL_CONN_CFG2TMC_GPIO_WAN4_0
pinmux.0x02434048 = <value1>; //
gpio_wan3_ph1.PADCTL_CONN_GPIO_WAN3_0
pinmux.0x0243404C = <value2>; //
gpio_wan3_ph1.PADCTL_CONN_CFG2TMC_GPIO_WAN3_0
```

</details>

## 第 3 章 Common Prod Configuration

**Prod** 设置是**经硅片与板级表征**的微调，用于让接口满足时序与信号质量要求。它们分为：

- **Common prod**（本章）——**pad / pinmux** 侧，以寄存器 patch 列表的形式给出。
- **Controller prod**（第 4 章）——**位于**特定控制器（QSPI、SDMMC、……）**内部**。

**必需属性：**
- addr-value-data：由 <绝对 PADCTL 寄存器地址, mask, data> 构成的列表
对于 prod 配置文件中的每个此类条目，MB1 从指定地址读取 data，依据 mask 和 value 修改 data，再把 data 写回该地址。

```text
val = read(address)
val = (val & ~mask) | (value & mask)
write(val, address)
```

common prod DTS 文件位于
hardware/nvidia/platform/t23x/<platform>/bct/ 目录。

**prod 配置文件的 NEW DTS 格式示例：**

```dts
/dts-v1/;
/ {
prod {
addr-mask-data = <0x0c302030
0x0000100 0x00000000>,
<0x0c302040 0x0000100 0x00000000>,
<0x0244100c 0xff1ff000 0x0a00a000>,
<0x02441004 0xff1ff000 0x0a00a000>;
};
};
```

**旧版 `.cfg` 格式（pre-DTS）：**

```text
prod.major = 1;
prod.minor = 0;
prod.0x0c302030.0x0000100 =
0x00000000;
prod.0x0c302040.0x0000100 =
0x00000000;
prod.0x0244100c.0xff1ff000 =
0x0a00a000;
prod.0x02441004.0xff1ff000 =
0x0a00a000;
```

## 第 4 章 Controller Prod Configuration

本章是**控制器级** prod：寄存器元组绑定到**特定 IP 实例**（QSPI、SDMMC、……），通常位于 **`deviceprod`** 节点下。

**DTS 结构（模板）：**

```text
/ {
    deviceprod {
        <controller-name>-<Instance> = <&Label>;
        #prod-cells = <N>;
        <Label>: <controller-name>@<base-address> {
            <mode> {
                prod = < <address offset> <mask> <value> >;
            };
        };
    };
};
```

**其中：**

| 符号 | 含义 |
|--------|---------|
| **Instance** | 控制器实例 id。 |
| **Label** | 用于将 instance 映射到设备节点的节点 label。 |
| **`<controller-name>`** | 模块名（例如 `sdmmc`、`qspi`、`se`、`i2c`）。 |
| **`<base-address>`** | 控制器基地址。 |
| **`<mode>`** | prod 生效的模式（例如 `default`、`hs400`）。 |
| **`<address offset>`** | 相对控制器基地址的寄存器偏移。 |
| **`<mask>`**、**`<value>`** | 32 位 RMW mask 与 data。 |
| **`#prod-cells`** | 每个 `prod` 元组的 cell 数（例如 **3** ⇒ offset、mask、value）。 |

旧版配置格式使用设备实例 <controller-name>.<instance-index>
而非设备的基地址。旧版格式用 1 字节存储
instance，而新 DTS 格式带基地址，在 BCT 中需要 4 字节。因此，
BCT 结构中的部分字段必须相应移位。

对每个元组，**MB1** 对控制器寄存器执行 read–modify–write：

```text
val = read(address)
val = (val & ~mask) | (value & mask)
write(val, address)
```

controller prod 文件位于 `hardware/nvidia/platform/t23x/<platform>/bct/`。

**prod 配置文件的 NEW DTS 示例：**

```dts
/dts-v1/;
/ {
deviceprod {
qspi-0 = <&qspi0>; qspi-1 = <&qspi1>; sdmmc-3 = <&sdmmc3>;
#prodcells =
<0x3>;
qspi0:
qspi@327
0000 {
default {
prod = <0x00000004 0x7C00
0x0>,
<0x00000004 0xFF 0x10>;
};
};
qspi1:
qspi
@330
0000
{
defa
ult
{
prod = <0x00000004 0x7C00
0x0>,
<0x00000004 0xFF 0x10>;
};
};
sdmmc3:
sdmm
c@34
6000
0 {
defa
ult
{
prod = <0x000001e4 0x00003FFF 0x0>;
};
hs400 {
prod = <0x00000100 0x1FFF0000
0x14080000>,
<0x0000010c 0x00003F00 0x00000028>;
};
ddr52 {
prod = <0x00000100 0x1FFF0000 0x14080000>;
};
};
};
};
```

**旧版 `.cfg` 格式（pre-DTS）：**

```text
//Qspi0
deviceprod.qspi.0.default.0x03270004.0
x7C00 = 0x0 //TX Trimmer
deviceprod.qspi.0.default.0x03270004.0
xFF =
0x10 //RX Trimmer
//Qspi1
deviceprod.qspi.1.default.0x03300004.0
x7C00 = 0x0 //TX Trimmer
deviceprod.qspi.1.default.0x03300004.0
xFF =
0x10 //RX Trimmer
//SDMMC
deviceprod.sdmmc.3.default.0x034601e4.0x00003FFF = 0x0 // auto cal pd and
pu offsets deviceprod.sdmmc.3.hs400.0x03460100.0x1FFF0000 = 0x14080000 //
tap and trim values deviceprod.sdmmc.3.hs400.0x0346010c.0x00003F00 =
0x00000028 // DQS trim val deviceprod.sdmmc.3.ddr52.0x03460100.0x1FFF0000 =
0x14080000 // tap and trim values
```


<details>
<summary>English original</summary>

**Chapter 3. Common Prod Configuration**

**Prod** settings are **silicon- and board-characterized** tweaks so an interface meets timing and signal quality. They are split into:

- **Common prod** (this chapter) — **pad / pinmux** side, as a list of register patches.
- **Controller prod** (Chapter 4) — **inside** specific controllers (QSPI, SDMMC, …).

**Required properties:**
- addr-value-data: List of <Absolute PADCTL register address, mask, data>
For each such entry in the prod configuration file, MB1 reads the data from the specified address, modifies the data based on mask and value, and writes the data back to the address.

```text
val = read(address)
val = (val & ~mask) | (value & mask)
write(val, address)
```

The common prod DTS file are kept in the
hardware/nvidia/platform/t23x/<platform>/bct/ directory.

**NEW DTS format example of the prod config file:**

```dts
/dts-v1/;
/ {
prod {
addr-mask-data = <0x0c302030
0x0000100 0x00000000>,
<0x0c302040 0x0000100 0x00000000>,
<0x0244100c 0xff1ff000 0x0a00a000>,
<0x02441004 0xff1ff000 0x0a00a000>;
};
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
prod.major = 1;
prod.minor = 0;
prod.0x0c302030.0x0000100 =
0x00000000;
prod.0x0c302040.0x0000100 =
0x00000000;
prod.0x0244100c.0xff1ff000 =
0x0a00a000;
prod.0x02441004.0xff1ff000 =
0x0a00a000;
```

**Chapter 4. Controller Prod Configuration**

This chapter is **controller-level** prod: register tuples tied to a **specific IP instance** (QSPI, SDMMC, …), usually under a **`deviceprod`** node.

**DTS shape (template):**

```text
/ {
    deviceprod {
        <controller-name>-<Instance> = <&Label>;
        #prod-cells = <N>;
        <Label>: <controller-name>@<base-address> {
            <mode> {
                prod = < <address offset> <mask> <value> >;
            };
        };
    };
};
```

**Where:**

| Symbol | Meaning |
|--------|---------|
| **Instance** | Controller instance id. |
| **Label** | Node label used to map instance → device node. |
| **`<controller-name>`** | Module name (e.g. `sdmmc`, `qspi`, `se`, `i2c`). |
| **`<base-address>`** | Controller base address. |
| **`<mode>`** | Mode the prod applies to (e.g. `default`, `hs400`). |
| **`<address offset>`** | Register offset from controller base. |
| **`<mask>`**, **`<value>`** | 32-bit RMW mask and data. |
| **`#prod-cells`** | Number of cells per `prod` tuple (e.g. **3** ⇒ offset, mask, value). |

The legacy config format used device instance <controller-name>.<instance-index>
instead of using the base address of the device. The legacy format keeps one byte to store the
instance, but new DTS format has a base address, which requires four bytes in the BCT. As a
result, some of the fields in the BCT structure have to be shifted accordingly.

For each tuple, **MB1** does a read–modify–write on the controller register:

```text
val = read(address)
val = (val & ~mask) | (value & mask)
write(val, address)
```

Controller prod files live in `hardware/nvidia/platform/t23x/<platform>/bct/`.

**NEW DTS example of prod configuration file:**

```dts
/dts-v1/;
/ {
deviceprod {
qspi-0 = <&qspi0>; qspi-1 = <&qspi1>; sdmmc-3 = <&sdmmc3>;
#prodcells =
<0x3>;
qspi0:
qspi@327
0000 {
default {
prod = <0x00000004 0x7C00
0x0>,
<0x00000004 0xFF 0x10>;
};
};
qspi1:
qspi
@330
0000
{
defa
ult
{
prod = <0x00000004 0x7C00
0x0>,
<0x00000004 0xFF 0x10>;
};
};
sdmmc3:
sdmm
c@34
6000
0 {
defa
ult
{
prod = <0x000001e4 0x00003FFF 0x0>;
};
hs400 {
prod = <0x00000100 0x1FFF0000
0x14080000>,
<0x0000010c 0x00003F00 0x00000028>;
};
ddr52 {
prod = <0x00000100 0x1FFF0000 0x14080000>;
};
};
};
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
//Qspi0
deviceprod.qspi.0.default.0x03270004.0
x7C00 = 0x0 //TX Trimmer
deviceprod.qspi.0.default.0x03270004.0
xFF =
0x10 //RX Trimmer
//Qspi1
deviceprod.qspi.1.default.0x03300004.0
x7C00 = 0x0 //TX Trimmer
deviceprod.qspi.1.default.0x03300004.0
xFF =
0x10 //RX Trimmer
//SDMMC
deviceprod.sdmmc.3.default.0x034601e4.0x00003FFF = 0x0 // auto cal pd and
pu offsets deviceprod.sdmmc.3.hs400.0x03460100.0x1FFF0000 = 0x14080000 //
tap and trim values deviceprod.sdmmc.3.hs400.0x0346010c.0x00003F00 =
0x00000028 // DQS trim val deviceprod.sdmmc.3.ddr52.0x03460100.0x1FFF0000 =
0x14080000 // tap and trim values
```

</details>

## 第 5 章 Pad 电压绑定

Pad 可运行在 **1.2 V**、**1.8 V** 或 **3.3 V**（受 SoC 与 ball 规则约束）。**MB1** 必须对 **pad 电压** 编程，使其与你的 **电源树** 和接口匹配：

- 如果针对实际 I/O 电源轨的 **pad 电压设置错误**，信号可能失效——而在 **欠压** 情况下，硬件可能 **损坏**。
- Pad 电压 **`.dtsi`** 通常 **由 pinmux 表格生成**（与 pinmux 相同的流程）。

文件位于 `hardware/nvidia/platform/t23x/<platform>/bct/`。**DTS** 布局遵循表格输出，所以看起来可能与旧的 **CFG** 片段不同。

**pad 电压配置文件的 NEW DTS 格式示例：**

```dts
/*This dtsi file was generated by e3360_1099_slt_a01.xlsm Revision: 126 */
/ {
pmc@c360000 {
 io-pad-defaults {
 sdmmc1_hv {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
sdmmc3_hv {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
eqos {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
qspi {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
debug {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
ao_hv {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_3_3V>;
};
audio_hv {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_3_3V>;
};
ufs {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_2V>;
};
};
};
};
```

**旧版 `.cfg` 格式（DTS 之前）：**

```text
pad-voltage.major = 1;
pad-voltage.minor = 0;
pad-voltage.0x0c36003c = 0x0000003e;
// PMC_IMPL_E_18V_PWR_0 padvoltage.0x0c360040 = 0x00000079; //
PMC_IMPL_E_33V_PWR_0
```

## 第 6 章 PMIC 配置

启动期间，**MB1** 会拉起供 **CPU**、**CORE**、**DRAM** 及相关逻辑使用的 **PMIC** 电源轨。典型工作包括：

- 使能电源轨并设置电压  
- **FPS**（上电时序）配置  
- 平台特定的 **I2C** / **PWM** / **MMIO** 序列（有时步骤之间有 **延时**）  

PMIC BCT 分为 **common** 属性（全局）和 **rail-specific** 的 **blocks**。

### 6.1 通用配置

适用于所有电源轨。模板：

```dts
/ {
    pmic {
        <parameter> = <value>;
    };
};
```

| 参数 | 描述 |
|-----------|-------------|
| `rail-count` | 文件中描述的电源轨数量。 |
| `command-retries-count` | MB1 可重试一条命令的次数。 |
| `wait-before-start-bus-clear-us` | 在执行总线清除命令之前等待 **1 << n** 微秒（**n** = 字段值）。 |

*(属性拼写遵循 NVIDIA DU-10990；你的实际 DTS 可能使用 BSP 中带连字符的名称。)*

### 6.2 电源轨特定配置

- 每个 **rail** 可以有多个 **`block@N`** 节点。  
- 每个 **block** 使用 **一种** 传输方式：**`i2c-controller`**、**`pwm`** 或 **`mmio`**。  

模板：

```dts
/ {
    pmic {
        <rail-name> {
            block@<index> {
                <parameter> = <value>;
            };
        };
    };
};
```

**电源轨名称**（`<rail-name>`）：

| 名称 | 作用 |
|------|------|
| `system` | 系统 PMIC |
| `cpu` / `cpu0` | CPU 电源轨 |
| `cpu1` | 第二路 CPU 电源轨 |
| `core` | Core / SoC 电源轨 |
| `memio` | 与 DRAM 相关的电源轨 |
| `thermal` | 外部温度传感器通路 |
| `platform` | 其他平台 I2C |

**命令：** 在一个 block 下，**`commands { … }`** 可以包含可选的 **groups** 和 **`command@N`** 条目，条目带有 `reg-addr`、`mask`、`value`（语义取决于 **MMIO** 还是 **I2C**——参见 NVIDIA 的完整表格）。

**面向 I2C 的 block 参数**（代表性示例）：

| 参数 | 描述 |
|-----------|-------------|
| `block-delay` | block 中 **每条** 命令之后的延时（µs）。 |
| `i2c-controller-id` | I2C 控制器实例。 |
| `slave-addr` | 7 位 I2C 地址。 |
| `reg-data-size` | 数据宽度：`0` / `8`（1 字节）或 `16`（2 字节）。 |
| `reg-addr-size` | 寄存器地址宽度（允许取值见官方文档）。 |

**PWM block 参数**（当 type 为 PWM 时）：`controller-id`、`source-frq-hz`、`period-ns`、`min-microvolts`、`max-microvolts`、`init-microvolts`、`enable`（`0` = 仅配置，`1` = 配置后使能）。


<details>
<summary>English original</summary>

**Chapter 5. Pad Voltage Binding**

Pads can run at **1.2 V**, **1.8 V**, or **3.3 V** (subject to SoC and ball rules). **MB1** must program **pad voltage** to match your **power tree** and interface:

- If the **pad voltage setting is wrong** for the actual I/O rail, signals may fail—or in the **undervoltage** case, hardware can be **damaged**.
- Pad-voltage **`.dtsi`** is normally **generated from the pinmux spreadsheet** (same flow as pinmux).

Files live in `hardware/nvidia/platform/t23x/<platform>/bct/`. The **DTS** layout follows spreadsheet output, so it may look different from older **CFG** snippets.

**NEW DTS format example of pad-voltage configuration file:**

```dts
/*This dtsi file was generated by e3360_1099_slt_a01.xlsm Revision: 126 */
/ {
pmc@c360000 {
 io-pad-defaults {
 sdmmc1_hv {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
sdmmc3_hv {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
eqos {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
qspi {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
debug {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_8V>;
};
ao_hv {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_3_3V>;
};
audio_hv {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_3_3V>;
};
ufs {
nvidia,io-pad-init-voltage = <IO_PAD_VOLTAGE_1_2V>;
};
};
};
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
pad-voltage.major = 1;
pad-voltage.minor = 0;
pad-voltage.0x0c36003c = 0x0000003e;
// PMC_IMPL_E_18V_PWR_0 padvoltage.0x0c360040 = 0x00000079; //
PMC_IMPL_E_33V_PWR_0
```

**Chapter 6. PMIC Configuration**

During boot, **MB1** brings up **PMIC** rails for **CPU**, **CORE**, **DRAM**, and related logic. Typical work includes:

- Enabling rails and setting voltages  
- **FPS** (sequencing) configuration  
- Platform-specific **I2C** / **PWM** / **MMIO** sequences (sometimes with **delays** between steps)  

PMIC BCT splits into **common** properties (global) and **rail-specific** **blocks**.

**6.1 Common configuration**

Applies to all rails. Template:

```dts
/ {
    pmic {
        <parameter> = <value>;
    };
};
```

| Parameter | Description |
|-----------|-------------|
| `rail-count` | Number of rails described in the file. |
| `command-retries-count` | How many times MB1 may retry a command. |
| `wait-before-start-bus-clear-us` | Wait **1 << n** microseconds before a bus-clear command (**n** = field value). |

*(Property spelling follows NVIDIA DU-10990; your exact DTS may use the hyphenated names from the BSP.)*

**6.2 Rail-specific configuration**

- Each **rail** may have multiple **`block@N`** nodes.  
- Each **block** uses **one** transport: **`i2c-controller`**, **`pwm`**, or **`mmio`**.  

Template:

```dts
/ {
    pmic {
        <rail-name> {
            block@<index> {
                <parameter> = <value>;
            };
        };
    };
};
```

**Rail names** (`<rail-name>`):

| Name | Role |
|------|------|
| `system` | System PMIC |
| `cpu` / `cpu0` | CPU rail |
| `cpu1` | Second CPU rail |
| `core` | Core / SoC rail |
| `memio` | DRAM-related rail |
| `thermal` | External thermal sensor path |
| `platform` | Other platform I2C |

**Commands:** Under a block, **`commands { … }`** may contain optional **groups** and **`command@N`** entries with `reg-addr`, `mask`, `value` (semantics depend on **MMIO** vs **I2C**—see NVIDIA’s full tables).

**I2C-oriented block parameters** (representative):

| Parameter | Description |
|-----------|-------------|
| `block-delay` | Delay (µs) after **each** command in the block. |
| `i2c-controller-id` | I2C controller instance. |
| `slave-addr` | 7-bit I2C address. |
| `reg-data-size` | Data width: `0` / `8` (1 byte) or `16` (2 bytes). |
| `reg-addr-size` | Register address width (see official doc for allowed values). |

**PWM block parameters** (when type is PWM): `controller-id`, `source-frq-hz`, `period-ns`, `min-microvolts`, `max-microvolts`, `init-microvolts`, `enable` (`0` = configure only, `1` = enable after configure).

</details>

### 6.3 执行顺序（MB1）

MB1 将 PMIC section 视为**可选**，若某个 block 缺失则**告警**。典型的**执行顺序**：

1. 外部温度传感器
2. 通用**系统** PMIC
3. **SOC / core** 电源轨
4. **DRAM 相关**电源轨
5. DRAM 初始化
6. **CPU** 电源轨
7. CPU microcode 加载 / CPU 上电
8. **平台**其他 I2C

PMIC DTS 片段位于 `hardware/nvidia/platform/t23x/<platform>/bct/`。

**PMIC 配置文件的新 DTS 格式示例**


<details>
<summary>English original</summary>

**6.3 Order of execution (MB1)**

MB1 treats PMIC sections as **optional** and **warns** if a block is missing. Typical **execution order**:

1. External thermal sensor  
2. Generic **system** PMIC  
3. **SOC / core** rail  
4. **DRAM-related** rail  
5. DRAM init  
6. **CPU** rail(s)  
7. CPU microcode load / CPUs on  
8. **Platform** other I2C  

PMIC DTS fragments live in `hardware/nvidia/platform/t23x/<platform>/bct/`.

**NEW DTS format example of PMIC configuration file**

</details>

```dts
/dts-v1/;
/ {
 pmic {
 system {
 block@0 {
 controller-id = <4>;
slave-addr = <0x78>; // 7BIt:0x3c reg-data-size = <8>;
 reg-addr-size = <8>;
 block-delay = <10>;
i2c-update-verify = <1>; //update and verify
 commands {
 cpu-rail-cmds {
 command@0 {
 reg-addr = <0x50>;
mask = <0xC0>;
value = <0x00>;
 };
 command@1 {
 reg-addr = <0x51>;
mask = <0xC0>;
 value = <0x00>;
 };
 command@2 {
 reg-addr = <0x4A>;
mask = <0xC0>;
 value = <0x00>;
 };
command@3 {
 reg-addr = <0x4B>;
mask = <0xC0>;
 value = <0x00>;
 };
command@4 {
 reg-addr = <0x4C>;
 mask = <0xC0>;
value = <0x00>;
 };
};
gpio07-cmds {
 command@0 {
 reg-addr = <0xAA>;
 mask = <0xBB>;
 value = <0xCC>;
 };
 command@1 {
 reg-addr = <0xDD>;
 mask = <0xEE>;
value = < 0xFF>;
 };
};
misc-cmds {
 command@0 {
 reg-addr = <0x53>;
 mask = <0x38>;
value = <0x00>;
 };
 command@1 {
 reg-addr = <0x55>;
mask = <0x38>;
 value = < 0x10>;
 };
 command@2 {
 reg-addr = <0x41>;
mask = <0x1C>;
value = <0x1C>;
 };
 };
 };
 };
};
core {
 block@0 {
 pwm;
 controller-id = <6>;
 source-frq-hz = <204000000>;
 period-ns = <1255>;
 min-microvolts = <400000>;
 max-microvolts = <1200000>;
 init-microvolts = <850000>; enable;
};
block@1 {
 mmio;
 block-delay = <3000>; commands {
 command@0 {
 reg-addr = <0x02434080>;
mask = <0x10>;
 value = <0x0>;
 };
 };
 };
};
cpu@0 {
 block@0 {
 pwm;
controller-id = <5>;
source-frq-hz = <204000000>;
 period-ns = <1255>;
min-microvolts = <400000>;
max-microvolts = <1200000>;
 init-microvolts = <800000>; enable;
};
block@1 {
 mmio;
 block-delay = <3>; commands {
 commands@0 {
 reg-addr = <0x02214e00>;
 mask = <0x3>;
 value = <0x00000003>;
 };
 command@1 {
 reg-addr = <0x02214e0c>;
 mask = <0x1>;
value = <0x00000000>;
 };
 command@2 {
 reg-addr = <0x02214e10>;
mask = <0x1>;
value = <0x00000001>;
 };
command@3 {
 reg-addr = <0x02446008>;
mask = <0x400>;
value = <0x00000000>;
 };
command@4 {
 reg-addr = <0x02446008>;
 mask = <0x10>;
 value = <0x00000000>;
 };
 };
};
block@2 {
mmio;
commands {
 command@0 {
 reg-addr = <0x02434098>;
 mask = <0x10>;
value = <0x00>;
 };
 };
 };
};
platform {
 block@0 {
 i2c-controller; controller-id = <1>;
 slave-addr = <0x40>;
 reg-data-size = <8>;
 reg-addr-size = <8>;
 block-delay = <10>;
 i2c-update-verify = <1>;
commands {
 command@0 {
 reg-addr = <0x03>;
mask = <0x30>;
 value = <0x00>;
 };
 command@1 {
 reg-addr = <0x01>;
 mask = <0x30>;
 value = <0x20>;
 };
 };
 };
 };
 };
};
/dts-v1/;
/ {
 pmic {
 system {
 block@0 {
 controller-id = <4>;
 slave-addr = <0x78>; // 7BIt:0x3c reg-data-size = <8>;
 reg-addr-size = <8>;
 block-delay = <10>;
 i2c-update-verify = <1>; //update and verify commands {
 cpu-rail-cmds {
 command@0 {
 reg-addr = <0x50>;
 mask = <0xC0>;
 value = <0x00>;
 };
command@1 {
 reg-addr = <0x51>;
mask = <0xC0>;
value = <0x00>;
 };
 command@2 {
 reg-addr = <0x4A>;
mask = <0xC0>;
value = <0x00>;
 };
 command@3 {
 reg-addr = <0x4B>;
mask = <0xC0>;
value = <0x00>;
 };
command@4 {
 reg-addr = <0x4C>;
mask = <0xC0>;
value = <0x00>;
 };
};
gpio07-cmds {
 command@0 {
 reg-addr = <0xAA>;
mask = <0xBB>;
value = <0xCC>;
 };
command@1 {
 reg-addr = <0xDD>;
mask = <0xEE>;
value = < 0xFF>;
 };
 };
misc-cmds {
 command@0 {
 reg-addr = <0x53>;
mask = <0x38>;
 value = <0x00>;
 };
command@1 {
 reg-addr = <0x55>;
mask = <0x38>;
 value = < 0x10>;
 };
 command@2 {
 reg-addr = <0x41>;
 mask = <0x1C>;
 value = <0x1C>;
 };
 };
 };
 };
};
core {
 block@0 {
 pwm;
 controller-id = <6>;
 source-frq-hz = <204000000>;
 period-ns = <1255>;
 min-microvolts = <400000>;
 max-microvolts = <1200000>;
 init-microvolts = <850000>; enable;
};
Block@1 {
 mmio;
 block-delay = <3000>; commands {
 command@0 {
 reg-addr = <0x02434080>;
 mask = <0x10>;
 value = <0x0>;
 };
 };
 };
};
cpu@0 {
 block@0 {
pwm;
controller-id = <5>;
source-frq-hz = <204000000>;
period-ns = <1255>;
min-microvolts = <400000>;
max-microvolts = <1200000>;
init-microvolts = <800000>; enable;
};
 block@1 {
 mmio;
block-delay = <3>; commands {
 commands@0 {
 reg-addr = <0x02214e00>;
mask = <0x3>;
value = <0x00000003>;
 };
command@1 {
 reg-addr = <0x02214e0c>;
mask = <0x1>;
 value = <0x00000000>;
 };
command@2 {
 reg-addr = <0x02214e10>;
mask = <0x1>;
value = <0x00000001>;
 };
 command@3 {
 reg-addr = <0x02446008>;
mask = <0x400>;
value = <0x00000000>;
 };
command@4 {
 reg-addr = <0x02446008>;
 mask = <0x10>;
value = <0x00000000>;
 };
 };
};
block@2 {
 mmio;
 commands {
 command@0 {
 reg-addr = <0x02434098>;
mask = <0x10>;
value = <0x00>;
 };
 };
 };
};
platform {
 block@0 {
 i2c-controller;
 controller-id = <1>;
 slave-addr = <0x40>;
 reg-data-size = <8>;
 reg-addr-size = <8>;
 block-delay = <10>;
 i2c-update-verify = <1>;
 commands {
 command@0 {
 reg-addr = <0x03>;
 mask = <0x30>;
 value = <0x00>;
 };
 command@1 {
 reg-addr = <0x01>;
mask = <0x30>;
 value = <0x20>;
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
//////////////////////////////////////////////// System Configurations
////////
// PMIC FPS to turn SD4 (VDD_DDR_1V1) on in time slot 0
// PMIC FPS to set GPIO2 (EN_DDR_VDDQ) high in time slot 1
// Set SLPEN = 1 and CLRSE on
POR reset
pmic.system.block[0].type =
1; //I2C
pmic.system.block[0].controll
er-id = 4;
pmic.system.block[0].slaveadd = 0x78; // 7BIt:0x3c
pmic.system.block[0].regdata-size = 8;
pmic.system.block[0].reg-add-size = 8;
pmic.system.block[0].block-delay = 10;
pmic.system.block[0].i2c-update-verify = 1; //update and verify
pmic.system.block[0].commands[0].0x53.0x38 = 0x00; // SD4 FPS UP
slot 0 pmic.system.block[0].commands[1].0x55.0x38 = 0x10; //
GPIO2 FPS UP slot 2 pmic.system.block[0].commands[2].0x41.0x1C =
0x1C; // SLPEN=1, CLRSE = 11
// PMIC FPS programming to reassign SD1, SD2, LDO4 and LDO5 to
// FPS0 to leave those rails
on in SC7
pmic.system.block[1].type =
1; //I2C
pmic.system.block[1].controll
er-id = 4;
pmic.system.block[1].slaveadd = 0x78; // 7BIt:0x3c
pmic.system.block[1].regdata-size = 8;
pmic.system.block[1].reg-add-size = 8;
pmic.system.block[1].block-delay = 10;
pmic.system.block[1].i2c-update-verify = 1; //update and verify
pmic.system.block[1].commands[0].0x50.0xC0 = 0x00; // SD1 FPS to
FPS0 pmic.system.block[1].commands[1].0x51.0xC0 = 0x00; // SD2
FPS to FPS0 pmic.system.block[1].commands[2].0x4A.0xC0 = 0x00;
// LDO4 FPS to FPS0 pmic.system.block[1].commands[3].0x4B.0xC0 =
0x00; // LDO5 FPS to FPS0
pmic.system.block[1].commands[4].0x4C.0xC0 = 0x00; // LDO6 FPS to
FPS0
// VDDIO_DDR to 1.1V, SD4 to 1.1V
pmic.system.block[2].type =
1; //I2C
pmic.system.block[2].controll
er-id = 4;
pmic.system.block[2].slaveadd = 0x78; // 7BIt:0x3c
pmic.system.block[2].regdata-size = 8;
pmic.system.block[2].reg-add-size = 8;
pmic.system.block[2].block-delay = 10;
pmic.system.block[2].i2c-update-verify = 1; //update and verify
pmic.system.block[2].commands[0].0x1A.0xFF = 0x28; // SD4 to
1.1V
//////////////////////////////////////////////// //CORE(SOC) RAIL
Configurations /////////////////////////////
// 1. Set 850mV
voltage.
pmic.core.block[0].typ
e = 2; // PWM Type
pmic.core.block[0].controller-id
= 6; //SOC_GPIO10: PWM7
pmic.core.block[0].source-frq-hz
= 204000000; //204MHz
pmic.core.block[0].period-ns =
1255; // 800KHz.
pmic.core.block[0].min-microvolts
= 400000;
pmic.core.block[0].max-microvolts = 1200000;
pmic.core.block[0].init-microvolts = 850000;
pmic.core.block[0].enable = 1;
// 2. Make soc_gpio10
pin in non-tristate
pmic.core.block[1].ty
pe = 0; // MMIO TYPE
pmic.core.block[1].bl
ock-delay = 3000;
pmic.core.block[1].commands[0].0x02434080.0x10 = 0x0; // soc_gpio10: tristate
(b4) = 0
//////////////////////////////////////////////// //CPU0 RAIL configurations
//////////////////////////////
// 1. Set 800mV
voltage.
pmic.cpu0.block[0].typ
e = 2; // PWM Type
pmic.cpu0.block[0].controller-id
= 5; //soc_gpio13; PWM6
pmic.cpu0.block[0].source-frq-hz
= 204000000; //204MHz
pmic.cpu0.block[0].period-ns =
1255; // 800KHz.
pmic.cpu0.block[0].min-microvolts
= 400000;
pmic.cpu0.block[0].max-microvolts = 1200000;
pmic.cpu0.block[0].init-microvolts = 800000;
pmic.cpu0.block[0].enable = 1;
// 2. CPU PWR_REQ
cpu_pwr_req_0_pb0 to be
1
pmic.cpu0.block[1].type
= 0; // MMIO TYPE
pmic.cpu0.block[1].bloc
k-delay = 3;
pmic.cpu0.block[1].commands[0].0x02214e00.0x3 = 0x00000003; // CONFIG B0
pmic.cpu0.block[1].commands[1].0x02214e0c.0x1 = 0x00000000; // CONTROL B0
pmic.cpu0.block[1].commands[2].0x02214e10.0x1 = 0x00000001; // OUTPUT B0
pmic.cpu0.block[1].commands[3].0x02446008.0x400 = 0x00000000; //
cpu_pwr_req_0_pb0 to GPIO mode
pmic.cpu0.block[1].commands[4].0x02446008.0x10 = 0x00000000; //
cpu_pwr_req_0_pb0 tristate(b4)=0
// 3. Set soc_gpio13 to
untristate
pmic.cpu0.block[2].type = 0; //
MMIO Type
pmic.cpu0.block[2].commands[4].0x02434098.0x10 = 0x00; // soc_gpio13 to be
untristate
//////////////////////////////////////////////// Platform Configurations
//////////////////////////////
// Configure pin4/pin5 as output of gpio expander(0x40)
// Configure pin4 low and pin5 high
of gpio expander(0x40)
pmic.platform.block[0].type = 1;
//I2C
pmic.platform.block[0].controllerid = 1; //gen2
pmic.platform.block[0].slave-add =
0x40; // 7BIt:0x20
pmic.platform.block[0].reg-datasize = 8;
pmic.platform.block[0].reg-add-size = 8;
pmic.platform.block[0].block-delay = 10;
pmic.platform.block[0].i2c-update-verify
= 1; //update and verify
pmic.platform.block[0].commands[0].0x03.0x30 = 0x00; //Configure pin4/pin5
as output pmic.platform.block[0].commands[1].0x01.0x30 = 0x20; //Configure
pin4 low and pin5 high
```

## 第 7 章 存储设备配置
存储设备配置文件包含平台特定的设置，
用于 MB1/MB2 阶段中的存储设备。
DTS 配置文件的形式如下：
/ {
device {
<storage_device>@instance-# {
<parameter> = <value>;
};
};
};
其中：
- <storage-device> 是存储设备控制器（qspiflash/ufs/sdmmc/sata）。
- <instance-#> 是存储控制器的实例
- <parameter> 是控制器特定参数，如下所示。
### 7.1 QSPI Flash 参数
参数 描述
clock-source-id QSPI 控制器时钟源
1: CLK_M
3: PLLP_OUT0
4: PLLM_OUT0
5: PLLC_OUT0
6: PLLC4_MUXED
clock-source-frequency 时钟源频率（单位 Hz）
interface-frequency QSPI 控制器频率（单位 Hz）
enable-ddr-mode 0: QSPI SDR 模式
1: QSPI DDR 模式
参数 描述
maximum-bus-width 最大 QSPI 总线宽度
0: QSPI x1 lane
2: QSPI x4 lane
fifo-access-mode 0: PIO 模式
1: DMA 模式
ready-dummy-cycle 按照 QSPI flash 的 dummy cycle 数量
trimmer1-val TX trimmer 值
trimmer2-val RX trimmer 值
### 7.2 SDMMC 参数
参数 描述
clock-source-id SDMMC 控制器时钟源
0: PLLP_OUT0
1: PLLC4_OUT2_LJ
2: PLLC4_OUT0_LJ
3: PLLC4_OUT2
4: PLLC4_OUT1
5: PLLC4_OUT1_LJ
6: CLK_M
7: PLLC4_VCO
clock-source-frequency 时钟源频率（单位 Hz）
best-mode 支持的最高工作模式
0: SDR26
1: DDR52
2: HS200
3: HS400
pd-offset 下拉偏移
pu-offset 上拉偏移
enable-strobe-hs400 启用 HS400 strobe
dqs-trim-hs400 HS400 DQS trim 值
### 7.3 UFS 参数
参数 描述
max-hs-mode UFS 设备支持的最高 HS 模式
1: HS GEAR1
2: HS GEAR2
3: HS GEAR3
max-pwm-mode UFS 设备支持的最高 PWM 模式
1: PWM GEAR1
2: PWM GEAR2
3: PWM GEAR3

4: PWM GEAR4
max-active-lanes 最大 UFS lane 数量（1-2）
page-align-size UFS 数据结构所用页面的对齐（单位字节）
enable-hs-mode 是否启用 UFS HS 模式
0: 禁用
1: 启用
enable-fast-auto-mode 启用 fast auto 模式
0: 禁用
1: 启用
enable-hs-rate-a 启用 HS rate A
0: 禁用
1: 启用
参数 描述
enable-hs-rate-b 启用 HS rate B
0: 禁用
1: 启用
init-state UFS 设备在 MB1 入口处的初始状态
0: UFS 未由 BootROM 初始化
1: UFS 已由 BootROM 初始化（MB1 可跳过某些步骤）
### 7.4 SATA 参数
参数 描述
transfer-speed 0: GEN1
1: GEN2
存储设备配置文件保存在
hardware/nvidia/platform/t23x/<platform>/bct/ 目录中。

**存储设备配置文件的新 DTS 格式示例：**

```dts
/dts-v1/;
#include <defines.h>
/ {
device {
qspiflash@0 {
clock-source-id
= <PLLC_MUXED>;
clock-sourcefrequency =
<13000000>;
interfacefrequency =
<13000000>;
enable-ddrmode;
maximum-buswidth =
<QSPI_4_LANE>;
fifo-accessmode =
<DMA_MODE>;
read-dummycycle = <8>;
trimmer1-val = <0>;
trimmer2-val = <0>;
};
sdmmc@3 {
clock-source-id
= <PLLC4_OUT2>;
clock-sourcefrequency =
<52000000>;
best-mode =
<HS400>;
pd-offset = <0>;
pu-offset = <0>;
//enable-strobe-hs400; This property is not
there means it is disabled dqs-trim-hs400 =
<0>;
};
ufs@0 {
max-hsmode =
<HS_GEAR_3
>; maxpwm-mode =
<PWM_GEAR_
4>; maxactivelanes =
<2>;
pagealignsize =
<4096>;
enablehsmode;
//enabl
e-fastautomode;
enablehsrate-b;
//enable-hs-rate-a = <0>;
init-state = <0>;
};
};
};
```

**旧版 `.cfg` 格式（pre-DTS）：**

```text
// QSPI flash 0
device.qspiflash.0
.clock-source-id =
6;
device.qspiflash.0.clock-source-frequency = 13000000;
device.qspiflash.0.interface-frequency = 13000000;
device.qspiflash.0.enable-ddr-mode = 0;
device.qspiflash.0.maximum-bus-width = 2;
device.qspiflash.0.fifo-access-mode = 1;
device.qspiflash.0.read-dummy-cycle = 8;
device.qspiflash.0.trimmer1-val = 0;
device.qspiflash.0.trimmer2-val = 0;
// Sdmmc 3
device.sdmmc.3.clocksource-id = 3; //PLLP_OUT0
device.sdmmc.3.clocksource-frequency =
52000000;
device.sdmmc.3.best-mode =
3; //1=DDR52, 3=HS400
device.sdmmc.3.pd-offset =
0;
device.sdmmc.3.pu-offset = 0;
device.sdmmc.3.enable-strobe-hs400 = 0;
device.sdmmc.3.dqs-trim-hs400 = 0;
// Ufs 0
device.ufs.0.max-hs-mode = 3;
device.ufs.0.max-pwm-mode = 4;
device.ufs.0.max-active-lanes = 2;
device.ufs.0.page-align-size = 4096;
device.ufs.0.enable-hs-mode = 1;
device.ufs.0.enable-fast-auto-mode = 0;
device.ufs.0.enable-hs-rate-b = 1;
device.ufs.0.enable-hs-rate-a = 0;
device.ufs.0.init-state = 0;
```


<details>
<summary>English original</summary>

**Chapter 7. Storage Device Configuration**
The Storage Device configuration file contains the platform-specific settings for storage
devices in the MB1/MB2 stages.
The DTS Configuration file is of the following form:
/ {
device {
<storage_device>@instance-# {
<parameter> = <value>;
};
};
};
where:
- <storage-device> is the storage device controller (qspiflash/ufs/sdmmc/sata).
- <instance-#> is the instance of the storage controlle
- <parameter> is controller-specific parameter as shown below.
**7.1 QSPI Flash Parameters**
Parameter Description
clock-source-id QSPI controller Clock Source
1: CLK_M
3: PLLP_OUT0
4: PLLM_OUT0
5: PLLC_OUT0
6: PLLC4_MUXED
clock-source-frequency Frequency of clock source (in Hz)
interface-frequency QSPI controller frequency (in Hz)
enable-ddr-mode 0: QSPI SDR mode
1: QSPI DDR mode
Parameter Description
maximum-bus-width Maximum QSPI bus width
0: QSPI x1 lane
2: QSPI x4 lane
fifo-access-mode 0: PIO mode
1: DMA mode
ready-dummy-cycle No. of dummy cycles as per QSPI flash
trimmer1-val TX trimmer value
trimmer2-val RX trimmer value
**7.2 SDMMC Parameters**
Parameter Description
clock-source-id SDMMC controller Clock Source
0: PLLP_OUT0
1: PLLC4_OUT2_LJ
2: PLLC4_OUT0_LJ
3: PLLC4_OUT2
4: PLLC4_OUT1
5: PLLC4_OUT1_LJ
6: CLK_M
7: PLLC4_VCO
clock-source-frequency Frequency of clock source (in Hz)
best-mode Highest supported mode of operation
0: SDR26
1: DDR52
2: HS200
3: HS400
pd-offset Pull-down offset
pu-offset Pull-up offset
enable-strobe-hs400 Enable HS400 strobe
dqs-trim-hs400 HS400 DQS trim value
**7.3 UFS Parameters**
Parameter Description
max-hs-mode Highest HS mode supproted by UFS device
1: HS GEAR1
2: HS GEAR2
3: HS GEAR3
max-pwm-mode Highest PWM mode supproted by UFS device
1: PWM GEAR1
2: PWM GEAR2
3: PWM GEAR3

4: PWM GEAR4
max-active-lanes Maximum number of UFS lanes (1-2)
page-align-size Alignment of pages used for UFS data structures (in bytes)
enable-hs-mode Whether to enable UFS HS modes
0: disable
1: enable
enable-fast-auto-mode Enable fast auto mode
0: disable
1: enable
enable-hs-rate-a Enable HS rate A
0: disable
1: enable
Parameter Description
enable-hs-rate-b Enable HS rate B
0: disable
1: enable
init-state Initial state of UFS device at MB1 entry
0: UFS not initialized by BootROM
1: UFS is initialized by BootROM (MB1 can skip certain steps)
**7.4 SATA Parameters**
Parameter Description
transfer-speed 0: GEN1
1: GEN2
The storage device configuration file are kept in the
hardware/nvidia/platform/t23x/<platform>/bct/ directory.

**NEW DTS format example of storage device configuration file:**

```dts
/dts-v1/;
#include <defines.h>
/ {
device {
qspiflash@0 {
clock-source-id
= <PLLC_MUXED>;
clock-sourcefrequency =
<13000000>;
interfacefrequency =
<13000000>;
enable-ddrmode;
maximum-buswidth =
<QSPI_4_LANE>;
fifo-accessmode =
<DMA_MODE>;
read-dummycycle = <8>;
trimmer1-val = <0>;
trimmer2-val = <0>;
};
sdmmc@3 {
clock-source-id
= <PLLC4_OUT2>;
clock-sourcefrequency =
<52000000>;
best-mode =
<HS400>;
pd-offset = <0>;
pu-offset = <0>;
//enable-strobe-hs400; This property is not
there means it is disabled dqs-trim-hs400 =
<0>;
};
ufs@0 {
max-hsmode =
<HS_GEAR_3
>; maxpwm-mode =
<PWM_GEAR_
4>; maxactivelanes =
<2>;
pagealignsize =
<4096>;
enablehsmode;
//enabl
e-fastautomode;
enablehsrate-b;
//enable-hs-rate-a = <0>;
init-state = <0>;
};
};
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
// QSPI flash 0
device.qspiflash.0
.clock-source-id =
6;
device.qspiflash.0.clock-source-frequency = 13000000;
device.qspiflash.0.interface-frequency = 13000000;
device.qspiflash.0.enable-ddr-mode = 0;
device.qspiflash.0.maximum-bus-width = 2;
device.qspiflash.0.fifo-access-mode = 1;
device.qspiflash.0.read-dummy-cycle = 8;
device.qspiflash.0.trimmer1-val = 0;
device.qspiflash.0.trimmer2-val = 0;
// Sdmmc 3
device.sdmmc.3.clocksource-id = 3; //PLLP_OUT0
device.sdmmc.3.clocksource-frequency =
52000000;
device.sdmmc.3.best-mode =
3; //1=DDR52, 3=HS400
device.sdmmc.3.pd-offset =
0;
device.sdmmc.3.pu-offset = 0;
device.sdmmc.3.enable-strobe-hs400 = 0;
device.sdmmc.3.dqs-trim-hs400 = 0;
// Ufs 0
device.ufs.0.max-hs-mode = 3;
device.ufs.0.max-pwm-mode = 4;
device.ufs.0.max-active-lanes = 2;
device.ufs.0.page-align-size = 4096;
device.ufs.0.enable-hs-mode = 1;
device.ufs.0.enable-fast-auto-mode = 0;
device.ufs.0.enable-hs-rate-b = 1;
device.ufs.0.enable-hs-rate-a = 0;
device.ufs.0.init-state = 0;
```

</details>

## 第 8 章 UPHY Lane 配置

**UPHY** Lane 可分配给不同的消费者（**XUSB**、**NVMe**、**UFS**、**PCIe**、**NVLink**、…）。**MB1** 必须配置 **MB1/MB2** 访问**启动存储**（例如 **NVMe**、**UFS**）所需的 Lane。在 **T23x** 上，**MB1** 加载 **BPMP-FW**；**MB2** 依赖 BPMP 进行进一步的 UPHY 设置——此 BCT 片段是 MB1 所需的**早期** Lane 归属信息。

**模板：**

```dts
/ {
    uphy-lane {
        <instance-type> {
            lane-owner-map = < <id> <owner-id> >, < <id> <owner-id> >;
        };
    };
};
```

**字段：**

| 字段 | 含义 |
|--------|---------|
| **`<instance-type>`** | 要配置的 UPHY flavor，例如 **`hsio`** 或 **`nvhs`**。 |
| **`<id>`** | 要分配的 Lane 或 PLL 索引。 |
| **`<owner-id>`** | 该 Lane/PLL 的数字 **owner** id（见 NVIDIA owner 表 / 电子表格）。 |

文件位于 `hardware/nvidia/platform/t23x/<platform>/bct/`。

**新 DTS 示例 uphy lane DTS 配置文件和旧 CFG 文件格式：**

```dts
/dts-v1/;
/ {
uphy-lane {
 hsio {
 lane-owner-map = <10 2>,
 <11 1>;
 };
 };
};
```

**旧版 `.cfg` 格式（DTS 之前）：**

```text
//UPHY
uphy-lane.major = 1;
uphy-lane.minor = 0;
uphy-lane.hsio.lane.10 = 2;
uphy-lane.hsio.lane.11 = 1;
```

## 第 9 章 OEM FW Ratchet 配置
oem-fw 的回滚防护通过 OEM-FW Ratchet 配置进行控制。
Ratcheting 是指阻止旧版本软件加载。软件的 ratchet 版本在
修复安全漏洞后递增，并将该版本与
加载前存储在软件 Boot Component Header（BCH）中的版本进行比较。该文件
定义 OEM-FW 组件的最低 ratchet 级别。如果 BCH 中的版本低于
BCT 中的最低 ratchet 级别，则该二进制/固件不会被加载。
配置文件中的每个条目形式如下：
/dts-v1/;
/ {
ratchet {
<loader_name1> {
<fw_name1> = < <fw_index1> <ratchet_value> >;
<fw_name2> = < <fw_index2> <ratchet_value> >;
};
<loader_name2> {
<fw_name3> = < <fw_index3> <ratchet_value> >;
};
};
};
其中：
- <fw_index#> 是每个 oem-fw 的唯一索引。
- <loader_name#> 是 Boot Stage 二进制的名称，它加载与 fw_index 对应的
固件。
- <fw_name#> 是固件的名称。
- <ratchet_value> 是该固件的 ratchet_value。
ratchet 配置文件位于
hardware/nvidia/platform/t23x/<platform>/bct/ratchet 目录。

**新 DTS 示例**

```dts
/dts-v1/;
/ {
ratchet {
 mb1 {
 mb1bct = <1 3>;
 spefw = <2 0>;
 };
 mb2 {
cpubl = <11 5>;
};
};
};
```

**旧版 `.cfg` 格式（DTS 之前）：**

```text
//ratchet
ratchet.1.
mb1.mb1bct
= 3;
ratchet.2.mb1.spefw = 0;
ratchet.11.mb2.cpubl = 5;
```

## 第 10 章 BootROM 复位 PMIC 配置
对于某些 T23x 平台，在 L1 和 L2 复位启动路径中，可能要求 BootROM 将 PMIC 供电轨拉到
OTP 值。该过程通过发出 I2C 命令完成，这些命令由 MB1 编码到 AO scratch
寄存器中，并基于 MB1 BCT 中的 BootROM 复位配置。
- BootROM 发出这些命令的复位情况包括：
- Watchdog 5 超时
- Watchdog 4 超时
- SC7 退出
- SC8 退出
- SW-Reset
- AO-Tag/sensor 复位
- VF Sensor 复位
- HSM 复位
- 每个复位情况可以有三组 AO 命令块。
- 每个 AO 块包含多个块，每个块可以有多条命令。
在配置文件中，先指定 AO 块，然后使用
AO 块的 ID 初始化复位条件。


<details>
<summary>English original</summary>

**Chapter 8. UPHY Lane Configuration**

**UPHY** lanes can be assigned to different consumers (**XUSB**, **NVMe**, **UFS**, **PCIe**, **NVLink**, …). **MB1** must configure lanes that **MB1/MB2** need to reach **boot storage** (e.g. **NVMe**, **UFS**). On **T23x**, **MB1** loads **BPMP-FW**; **MB2** depends on BPMP for further UPHY setup—this BCT fragment is the **early** lane ownership MB1 needs.

**Template:**

```dts
/ {
    uphy-lane {
        <instance-type> {
            lane-owner-map = < <id> <owner-id> >, < <id> <owner-id> >;
        };
    };
};
```

**Fields:**

| Field | Meaning |
|--------|---------|
| **`<instance-type>`** | UPHY flavor to configure, e.g. **`hsio`** or **`nvhs`**. |
| **`<id>`** | Lane or PLL index to assign. |
| **`<owner-id>`** | Numeric **owner** id for that lane/PLL (see NVIDIA owner tables / spreadsheet). |

Files live in `hardware/nvidia/platform/t23x/<platform>/bct/`.

**NEW DTS example uphy lane DTS configuration file and old CFG file format:**

```dts
/dts-v1/;
/ {
uphy-lane {
 hsio {
 lane-owner-map = <10 2>,
 <11 1>;
 };
 };
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
//UPHY
uphy-lane.major = 1;
uphy-lane.minor = 0;
uphy-lane.hsio.lane.10 = 2;
uphy-lane.hsio.lane.11 = 1;
```

**Chapter 9. OEM FW Ratchet Configuration**
Roll-back prevention for oem-fw is controlled through the OEM-FW Ratchet configuration.
Ratcheting is when older version of software is precluded from loading. The ratchet version of
a software is incremented after fixing the security bugs, and this version is compared to the
version stored in the Boot Component Header(BCH) of the software before loading. This file
defines the minimum ratchet level for OEM-FW components. If the version in BCH is lower
than the minimum ratchet level in BCT, the binary/firmware will not be loaded.
Each entry in the config file is of the following form:
/dts-v1/;
/ {
ratchet {
<loader_name1> {
<fw_name1> = < <fw_index1> <ratchet_value> >;
<fw_name2> = < <fw_index2> <ratchet_value> >;
};
<loader_name2> {
<fw_name3> = < <fw_index3> <ratchet_value> >;
};
};
};
Where:
- <fw_index#> is the unique index for each oem-fw.
- <loader_name#> is the name of the Boot Stage binary, which loads the firmware that
corresponds to fw_index.
- <fw_name#> is the name of the firmware.
- <ratchet_value> is the ratchet_value for the firmware.
The ratchet configuration file is in the
hardware/nvidia/platform/t23x/<platform>/bct/ratchet directory.

**NEW DTS example**

```dts
/dts-v1/;
/ {
ratchet {
 mb1 {
 mb1bct = <1 3>;
 spefw = <2 0>;
 };
 mb2 {
cpubl = <11 5>;
};
};
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
//ratchet
ratchet.1.
mb1.mb1bct
= 3;
ratchet.2.mb1.spefw = 0;
ratchet.11.mb2.cpubl = 5;
```

**Chapter 10. BootROM Reset PMIC Configuration**
For some T23x platforms, in L1 and L2 reset boot paths, BootROM might be required to bring PMIC rails to
OTP values. This process is completed by issuing I2C commands, which are encoded in AO scratch
registers by MB1 and are based on the BootROM reset configuration in MB1 BCT.
- The reset cases where the BootROM issues these commands includes:
- Watchdog 5 expiry
- Watchdog 4 expiry
- SC7 exit
- SC8 exit
- SW-Reset
- AO-Tag/sensor reset
- VF Sensor reset
- HSM reset
- Each reset case can have three sets of AO blocks of commands.
- Each AO block has multiple blocks, and each block can have multiple commands.
In the configuration file, AO blocks are specified first, and then the reset conditions are initialized by using
the ID of the AO blocks.

</details>

### 10.1 指定 AO Block
配置文件中每条与 AO block 相关的行格式如下：
{
 <ResetType>-<Ao-command-index> = <&AoBlock-Label>
 reset {
 <AoBlock-Label>: aoblock@<aoblock-index> {
 <parameter> = <value>;
 ...
 block@<block-index> {
 <parameter> = <value>;
 };
 };
 <AoBlock-Label>: aoblock@<aoblock-index> {
 ...
 }
 };
};
节点 <parameter> 描述
<reset-type>-<Aocommandindex>
<&AoBlock-Label> • <reset-type> 指定复位类型，
取值必须为以下之一：
watchdog5, watchdog4, sc7,sc8,
soft-reset, sensor-aotag,
vfsensor 或 hsm。
- <AO-command-index> 是
AO 命令的索引，取值可为 0、1 或 2。
每条 reset path 最多可指向
三个 aoblock
- <AoBblock-Label> 是分配给
某个 Ao Block 的标签
aoblock@<aoblockindex>
command-retries-count 指定 <aoblockindex> 对应的
AO-block 允许的命令尝试次数
delay-between-command-us 指定不同命令之间的
延迟（单位：微秒）。
延迟按 1 << n 微秒计算，
其中 n 由此
参数提供。
wait-before-start-bus-clear-us 指定在向给定
AO block 发出 bus clear 命令之前的
等待超时（单位：微秒）。
等待时间按 1 << n 微秒计算，
其中 n 由此
参数提供。
block@<block-index> <command-type>/td> <command-type> 只能取一个
值 - i2c-controller。这是唯一
支持的取值
count 指定 block <block-index> 中的
命令数量。
i2c-controller-id I2C 控制器实例
slave-addr 7 位 I2C 从机地址
reg-data-size 寄存器大小，单位为 bit。有效值为 0(1-byte),
8(1-byte) 和 16(2-byte),
reg-addr-size 寄存器地址大小，单位为 bit。有效值为
0(1-byte), 8(1-byte) 和 16(2-byte)
节点 <parameter> 描述
commands <Address Value> 对列表，其中 value
将被写入 I2c 从机寄存器地址
<reg-addr>，对应索引为
<command- index> 的命令。

**BootROM 复位配置文件的 NEW DTS 示例：**


<details>
<summary>English original</summary>

**10.1 Specifying AO Blocks**
Each AO block-related line in the configuration file is of the following format:
{
 <ResetType>-<Ao-command-index> = <&AoBlock-Label>
 reset {
 <AoBlock-Label>: aoblock@<aoblock-index> {
 <parameter> = <value>;
 ...
 block@<block-index> {
 <parameter> = <value>;
 };
 };
 <AoBlock-Label>: aoblock@<aoblock-index> {
 ...
 }
 };
};
Node <parameter> Description
<reset-type>-<Aocommandindex>
<&AoBlock-Label> • <reset-type> specifies the reset type and
must have one of the values,
watchdog5, watchdog4, sc7,sc8,
soft-reset, sensor-aotag,
vfsensor, or hsm.
- <AO-command-index> is the index of the
AO command and can have values 0, 1 or 2.
Each reset path can point to maxi- mum
three aoblocks
- <AoBblock-Label> is the label given to
one of the Ao Blocks
aoblock@<aoblockindex>
command-retries-count Specifies the number of command attempts
allowed for AO-block with <aoblockindex>
delay-between-command-us Specifies the delay (in microseconds), in
between different commands.
The delay is calculated as 1 << n
microseconds where n is provided by this
parameter.
wait-before-start-bus-clear-us Specifies the wait timeout (in microseconds),
before issuing the bus clear command for
given AO block.
The wait time is calculated as 1 << n
microseconds where n is provided by this
parameter.
block@<block-index> <command-type>/td> <command-type> can only be one
value - i2c-controller. That is the only one
supported
count Specifies the number of commands in the
block <block-index>.
i2c-controller-id I2C controller instance
slave-addr 7-bit I2C slave address
reg-data-size Register size in bits. Valid values are 0(1-byte),
8(1-byte) and 16(2-byte),
reg-addr-size Register address size in bits. Valid values are
0(1-byte), 8(1-byte) and 16(2-byte)
Node <parameter> Description
commands List of <Address Value> pairs where value
to be written to the I2c slave register address
<reg-addr> for the command indexed by
<command- index>.

**NEW DTS example of BootROM reset configuration file:**

</details>

```dts
/dts-v1/;
/ {
/dts-v1/;
/ {
reset {
// Each reset path can point to upto three aoblocks
// This is a map of reset paths to aoblocks
// <reset-path>-<index-pointer> = <aoblock-id>
// index-number should be 0, 1 or 2
// aoblock-id is the id of the one of the blocks
mentioned above sensor-aotag-1 = <&aoblock0>;
sc7-1 = <&aoblock2>;
aoblock0: aoblock@0 {
command-retries-count = <1>;
delay-between-commands-us = <1>;
wait-before-start-bus-clear-us = <1>;
block@0 {
i2c-controller;
slave-add = <0x3c>; // 7BIt:0x3c reg-data-size = <8>;
reg-add-size = <8>;
commands {
command@0 {
reg-addr = <0x42>; value = <0xda>;
};
command@1 {
reg-addr =
<0x41>;
value =
<0xf8>;
};
};
};
};
// Shutdown: Set MAX77620
// Register ONOFFCNFG2, bit SFT_RST_WK = <0>
// Register ONOFFCNFG1, bit SFT_RST
= <1> aoblock1: aoblock@1 {
// Shutdown: Set MAX77620
// Register ONOFFCNFG2, bit SFT_RST_WK = <0>
// Register ONOFFCNFG1, bit
SFT_RST = <1> command-retriescount = <1>;
delay-between-commands-us = <1>;
wait-before-start-bus-clear-us =
<1>; block@0 {
i2c-controller;
slave-add = <0x3c>; //
7BIt:0x3c reg-data-size
= <8>;
reg-add-size = <8>;
commands {
command@0 {
reg-addr = <0x42>; value = <0x5a>;
};
command@1 {
reg-addr = <0x41>; value = <0xf8>;
};
};
};
};
// SC7 exit
// Clear PMC_IMPL_DPD_ENABLE_0[ON]=0
during SC7 exit aoblock2: aoblock@2 {
command-retries-count = <1>;
delay-between-commands-us = <256>;
wait-before-start-bus-clear-us = <1>; block@0 {
mmio;
command
s {
command@0 {
reg-addr =
<0x0c360010>;
value = <0x0>;
};
};
};
};
};
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
/ CFG Version 1.0
// This contains the BOOTROM commands in MB1 for
multiple reset paths. reset.major = 1;
reset.minor = 0;
// Automatic power cycling: Set MAX77620
// Register ONOFFCNFG2, bit SFT_RST_WK = 1 (default is "0" after cold boot),
// Register ONOFFCNFG1, bit
SFT_RST = 1
reset.aoblock[0].commandretries-count = 1;
reset.aoblock[0].delay-between-commands-us = 1;
reset.aoblock[0].wait-before-start-bus-clear-us = 1;
reset.aoblock[0].block[0].type = 1; // I2C
Type reset.aoblock[0].block[0].slave-add =
0x3c; // 7BIt:0x3c
reset.aoblock[0].block[0].reg-data-size =
8;
reset.aoblock[0].block[0].reg-add-size =
8;
reset.aoblock[0].block[0].commands[0].0x4
2 = 0xda;
reset.aoblock[0].block[0].commands[1].0x4
1 = 0xf8;
// Shutdown: Set MAX77620
// Register ONOFFCNFG2, bit SFT_RST_WK = 0
// Register ONOFFCNFG1, bit
SFT_RST = 1
reset.aoblock[1].commandretries-count = 1;
reset.aoblock[1].delay-between-commands-us = 1;
reset.aoblock[1].wait-before-start-bus-clear-us = 1;
reset.aoblock[1].block[0].type = 1; // I2C
Type reset.aoblock[1].block[0].slave-add =
0x3c; // 7BIt:0x3c
reset.aoblock[1].block[0].reg-data-size =
8;
reset.aoblock[1].block[0].reg-add-size =
8;
reset.aoblock[1].block[0].commands[0].0x4
2 = 0x5a;
reset.aoblock[1].block[0].commands[1].0x4
1 = 0xf8;
// SC7 exit
// Clear PMC_IMPL_DPD_ENABLE_0[ON]=0
during SC7 exit
reset.aoblock[2].command-retries-count
= 1;
reset.aoblock[2].delay-between-commands-us = 256;
reset.aoblock[2].wait-before-start-bus-clear-us = 1;
reset.aoblock[2].block[0].type = 0; // MMIO Type
reset.aoblock[2].block[0].commands[0].0x0c360010 = 0x0;
// Shutdown in sensor/ao-tag
// no commands for other
case reset.sensoraotag.aocommand[0] = 1;
reset.sc7.aocommand[0] = 2;
```

## 第 11 章 杂项配置
不适用于其他类别的各种设置记录在杂项配置文件中。
### 11.1 MB1 特性字段
字段 说明

disable_spe • 0：允许由 MB1 加载 SPE-FW。
- 1：允许由 MB1 加载 SPE-FW。
enable_dram_page_blacklisting

- 0：禁用 DRAM ECC 页黑名单特性。
- 1：启用 DRAM ECC 页黑名单特性。
disable_sc7 • 0：启用 SC7 进入/退出支持。
- 1：启用 SC7 进入/退出支持。
disable_fuse_visibility 默认情况下某些 fuse 不可读写，因为它们不可见。
- 0：保持 fuse 的默认可见性。
- 1：启用此类 fuse 的可见性。
enable_vpr_resize • 0：VPR 依据 SDRAM 配置文件的 McVideoProtectSizeMb 和 McVideoProtect, WriteAccess 字段分配。
- 1：启用 VPR-resize 特性（例如 MB1 不分配 VPR carveout，且启用对 VPR carveout 的 TZ 写访问）。
l2_mss_encrypt_regeneration 在 L2 RAMDUMP 复位时，为各 carveout 重新生成 MSS 加密密钥。这是一个位域，位映射为：
- 1:TZDRAM
- 2:VPR
- 3:GSC
se_ctx_save_tz_lock 将 SE 上下文保存和 SHA_CTX_INTEGRITY 操作限制在 TZ 内。
disable_mb2_glitch_protection 禁用对 DCLS 故障、TCM parity 错误、TCM 与 cache ECC 的检查
字段 说明

enable_dram_error_injection • 0：禁用 DRAM 错误注入
测试
- 1：启用 DRAM 错误注入
测试
enable_dram_staged_scrubbi
ng
- 0：若启用 DRAM ECC，则擦写整个 DRAM
- 1：若启用 DRAM ECC，则分阶段擦写 DRAM —— 每个 BL 负责其使用的 DRAM 部分。
wait_for_debugger_connection • 0：不在 MB1 结束时等待调试器连接
- 1：在 MB1 结束时于 while(1) 循环中自旋等待调试器连接
limit_l1_boot_client_freq 0：L0 和 L1 复位时保持引导客户端频率（BPMP、SE、CBB 等）相同
reset
#### 11.1.1 时钟数据
这些字段允许对时钟相关的配置进行一定定制。
字段 说明

bpmp_cpu_nic_divide
r
控制 BPMP CPU 频率
0：跳过编程
CLK_SOURCE_BPMP_CPU_NIC[BPMP_CPU_NIC_CLK_DI, VISOR]
非零：1 + 待编程到
CLK_SOURCE_BPMP_CPU_NIC[BPMP,
_CPU_NIC_CLK_DIVISOR] 中的值
bpmp_apb_divider 控制 BPMP APB 频率
- 0：跳过编程
CLK_SOURCE_BPMP_APB[BPMP_APB_CLK_DIVISOR] 非零：
- 1 + 待编程到
CLK_SOURCE_BPMP_APB[BPMP_APB,_CLK_DIVISOR] 中的值
axi_cbb_divider 控制 CBB（控制主干）频率
- 0：跳过编程 CLK_SOURCE_AXI_CBB[AXI_CBB_CLK_DIVISOR]
非零：
- 1 + 待编程到 CLK_SOURCE_AXI_CBB[AXI_CBB_CL←,
K_DIVISOR] 中的值
se_divider 控制 SE（安全引擎）频率
- 0：跳过编程 CLK_SOURCE_SE[SE_CLK_DIVISOR]
- 非零：1 + 待编程到
CLK_SOURCE_SE[SE_CLK_DIVISOR] 中的值
aon_cpu_nic_divider 控制 AON/SPE CPU 频率
- 0：跳过编程
CLK_SOURCE_AON_CPU_NIC[AON_CPU_NIC_CLK_DI←, VISOR]
非零：
- 1 + 待编程到 CLK_SOURCE_AON_CPU_NIC[AON_C,
PU_NIC_CLK_DIVISOR] 中的值
字段 说明

aon_apb_divider 控制 AON APB 频率
- 0：跳过编程
CLK_SOURCE_AON_CPU_NIC[AON_CPU_NIC_CLK_DI←, VISOR]
- 非零：1 + 待编程到
CLK_SOURCE_AON_CPU_NIC[AON_C, PU_NIC_CLK_DIVISOR] 中的值
aon_can0_divider 控制 AON CAN1 频率
- 0：跳过编程 CLK_SOURCE_CAN1[CAN1_CLK_DIVISOR]
- 非零：1 + 待编程到
CLK_SOURCE_CAN1[CAN1_CLK_DIVI, SOR] 中的值
aon_can1_divider 控制 AON CAN2 频率
- 0：跳过编程 CLK_SOURCE_CAN2[CAN2_CLK_DIVISOR]
- 非零：1 + 待编程到
CLK_SOURCE_CAN2[CAN2_CLK_DIVI, SOR] 中的值
osc_drive_strength 振荡器驱动强度
pllaon_divn PLLAON 的 DIVN 值
- 0：使用 PLLAON_DIVN = 30, PLLAON_DIVM = 1, PLLAON_DIVP = 2
- 非零：1 + 待编程到 PLLAON_BASE[PLLAON_DIVN] 中的值
pllaon_divm PLLAON 的 DIVM 值（当 clock.pllaon_divn = 0 时忽略）
1 + 待编程到 PLLAON_BASE[PLLAON_DIVM] 中的值
pllaon_divp PLLAON 的 DIVP 值（当 clock.pllaon_divn = 0 时忽略）
1 + 待编程到 PLLAON_BASE[PLLAON_DIVP] 中的值
pllaon_divn_frac 待编程的值
#### 11.1.2 AST 数据
MB1/MB2 使用地址转换（AST）模块，将不同固件的 DRAM carveout 映射到各辅助处理器簇的 32bit 虚拟地址空间。


<details>
<summary>English original</summary>

**Chapter 11. Miscellaneous Configuration**
The different settings that do not fit into the other categories are documented in the miscellaneous
configuration file.
**11.1 MB1 Feature Fields**
Field Descriptions
disable_spe • 0: Enables load of SPE-FW by MB1.
- 1: Enables load of SPE-FW by MB1.
enable_dram_page_blacklisting

- 0: Disables DRAM ECC page blacklisting feature.
- 1: Enables DRAM ECC page blacklisting feature.
disable_sc7 • 0: Enables SC7-entry/exit support.
- 1: Enables SC7-entry/exit support.
disable_fuse_visibility Certain fuses cannot be read or written by default because they are not
visible.
- 0: Keeps the default visibility of fuses.
- 1: Enables visibility of such fuses.
enable_vpr_resize • 0: VPR is allocated based on McVideoProtectSizeMb and
McVideoProtect, WriteAccess fields of SDRAM config file.
- 1: VPR-resize feature is enabled (for example, no VPR carveout is
allocated by MB1 and TZ write access to VPR carveout is enabled ).
l2_mss_encrypt_regeneration On L2 RAMDUMP reset, regenerate MSS encryption keys for the
carveouts. This is a bit-field with the bit-mapping:
- 1:TZDRAM
- 2:VPR
- 3:GSC
se_ctx_save_tz_lock Restrict SE context save and SHA_CTX_INTEGRITY operation to TZ.
disable_mb2_glitch_protection Disable checks on DCLS faults, TCM parity error, TCM and cache ECC
Field Descriptions
enable_dram_error_injection • 0: Disable DRAM error injection
tests
- 1: Enable DRAM error injection
tests
enable_dram_staged_scrubbi
ng
- 0: If DRAM ECC is enabled, scrub entire DRAM
- 1: If DRAM ECC is enabled, scrub DRAM in stages - each BL
responsible for the DRAM portions that it uses.
wait_for_debugger_connection • 0: Don't wait for debugger connection at end of MB1
- 1: Spin in a while(1) loop at end of MB1 for debugger connection
limit_l1_boot_client_freq 0: Keep boot client frequencies (BPMP, SE, CBB, etc) same for L0 and
L1
reset
**11.1.1 Clock Data**
These fields allow certain clock-related customization.
Field Description
bpmp_cpu_nic_divide
r
Controls BPMP CPU frequency
0: Skip programming
CLK_SOURCE_BPMP_CPU_NIC[BPMP_CPU_NIC_CLK_DI, VISOR]
non-zero: 1 + Value to be programmed in
CLK_SOURCE_BPMP_CPU_NIC[BPMP,
_CPU_NIC_CLK_DIVISOR]
bpmp_apb_divider Controls BPMP APB frequency
- 0: Skip programming of
CLK_SOURCE_BPMP_APB[BPMP_APB_CLK_DIVISOR] non-zero:
- 1 + Value to be programmed in
CLK_SOURCE_BPMP_APB[BPMP_APB,_CLK_DIVISOR]
axi_cbb_divider Controls CBB (control backbone) frequency
- 0: Skip programming of CLK_SOURCE_AXI_CBB[AXI_CBB_CLK_DIVISOR]
non-zero:
- 1 + Value to be programmed in CLK_SOURCE_AXI_CBB[AXI_CBB_CL←,
K_DIVISOR]
se_divider Controls SE (security engine) frequency
- 0: Skip programming of CLK_SOURCE_SE[SE_CLK_DIVISOR]
- non-zero: 1 + Value to be programmed in
CLK_SOURCE_SE[SE_CLK_DIVISOR]
aon_cpu_nic_divider Controls AON/SPE CPU frequency
- 0: Skip programming of
CLK_SOURCE_AON_CPU_NIC[AON_CPU_NIC_CLK_DI←, VISOR]
non-zero:
- 1 + Value to be programmed in CLK_SOURCE_AON_CPU_NIC[AON_C,
PU_NIC_CLK_DIVISOR]
Field Description
aon_apb_divider Controls AON APB frequency
- 0: Skip programming of
CLK_SOURCE_AON_CPU_NIC[AON_CPU_NIC_CLK_DI←, VISOR]
- non-zero: 1 + Value to be programmed in
CLK_SOURCE_AON_CPU_NIC[AON_C, PU_NIC_CLK_DIVISOR]
aon_can0_divider Controls AON CAN1 frequency
- 0: Skip programming of CLK_SOURCE_CAN1[CAN1_CLK_DIVISOR]
- non-zero: 1 + Value to be programmed in
CLK_SOURCE_CAN1[CAN1_CLK_DIVI, SOR]
aon_can1_divider Controls AON CAN2 frequency
- 0: Skip programming of CLK_SOURCE_CAN2[CAN2_CLK_DIVISOR]
- non-zero: 1 + Value to be programmed in
CLK_SOURCE_CAN2[CAN2_CLK_DIVI, SOR]
osc_drive_strength Oscillator drive strength
pllaon_divn DIVN value of PLLAON
- 0: Use PLLAON_DIVN = 30, PLLAON_DIVM = 1, PLLAON_DIVP = 2
- non-zero: 1 + Value to be programmed in PLLAON_BASE[PLLAON_DIVN]
pllaon_divm DIVM value of PLLAON (ignored when clock.pllaon_divn = 0)
1 + Value to be programmed in PLLAON_BASE[PLLAON_DIVM]
pllaon_divp DIVP value of PLLAON (ignored when clock.pllaon_divn = 0)
1 + Value to be programmed in PLLAON_BASE[PLLAON_DIVP]
pllaon_divn_frac Value to be programmed
**11.1.2 AST Data**
MB1/MB2 uses the address-translation (AST) module to map the DRAM carveout for different firmware in
the 32bit virtual address space of the various auxiliary processor clusters.

</details>

#### 11.1.3 MB1 AST 数据
字段 描述
mb2_va 用于 BPMP-R5 地址空间中 MB2 carveout 的虚拟地址。
spe_fw_va 用于 AON-R5 地址空间中 SPE-FW carveout 的虚拟地址。
misc_carveout_va 用于 SCE-R5 地址空间中 MISC carveout 的虚拟地址。
rcm_blob_carveout_va 用于 SCE-R5 地址空间中 RCM-blob carveout 的虚拟地址。
temp_map_a_carveout_va 用于临时映射 A 的虚拟地址，该映射在加载
二进制文件时使用
temp_map_a_carveout_siz
e
用于临时映射 A 的大小，该映射在加载二进制文件时使用
temp_map_a_carveout_va 用于临时映射 B 的虚拟地址，该映射在加载
二进制文件时使用
temp_map_a_carveout_siz
e
用于临时映射 B 的大小，该映射在加载二进制文件时使用
注意：上述 VA 空间均不得与 MMIO 区域或彼此重叠。
- Size 字段应为 2 的幂
- VA 字段应与其映射/carveout 对齐。
#### 11.1.4 Carveout 配置
尽管第 59 页的“SDRAM Configuration”包含 MC carveout 的首选基地址、大小和权限，但
它不具备 MB1 分配这些 carveout 所需的全部信息。这些附加信息
通过 miscellaneous 配置文件指定。
对于不受 MC 保护的 carveout，包括大小和首选基地址在内的所有信息，均
通过 miscellaneous 配置文件指定。
每个 MC carveout 配置参数采用以下形式：
/ {
misc {
carveout {
<carveout-type> {
<parameter> = <value>;
};
};
};
};
其中：
- <carveout-type> 标识 carveout，为以下之一：
carveout 类型
描述
gsc@[1-31] 用于各种用途的 GSC carveout
mts MTS/CPU-uCode carveout
vpr VPR carveout
tzdram 用于 SecureOS 的 TZDRAM carveout
os 用于加载 OS kernel 的 OS carveout
rcm 用于在 RCM 模式下加载 RCM-blob 的 RCM carveout（临时启动
carveout）
- <parameter> 为以下参数之一：
参数 描述
pref_base carveout 的首选基地址
size carveout 的大小（以字节为单位）
alignment carveout 基地址的对齐（以字节为单位）
ecc_protected 当启用基于 DRAM 区域的 ECC 且存在非 ECC 保护的
DR、AM 区域时，是否从 ECC 保护区域分配该 carveout
0：从非 ECC 保护区域分配 1：从 ECC 保护
区域分配
bad_page_tolerant 当启用 DRAM 页黑名单时，是否可以有坏页
在 carveout 中（仅可能用于非常大的 carveout，且这些 carveout 由
能够避开坏页的组件完全处理，使用
SMMU/MMU）
0：不允许 carveout 有坏页 1：允许
carveout 有坏页（分配时无需过滤坏页）
carveout 及其参数的有效组合如下表所示：
支持的 carveout-type pref_base size alignme
nt
ecc_protected bad_page_
tolerant
gsc-[1-31], mts, vpr N/A N/A YES YES YES
tzdram, mb2, cpubl, misc,
os, rcm
YES YES YES YES YES
#### 11.1.5 Coresight 数据
字段 描述
cfg_system_ctl 要编程到 CORESIGHT_CFG_SYSTEM_CTL 的值
cfg_csite_mc_wr_ctrl 要编程到
CORESIGHT_CFG_CSITE_MC_WR_CTRL
cfg_csite_mc_rd_ctrl 要编程到
CORESIGHT_CFG_CSITE_MC_RD_CTRL
cfg_etr_mc_wr_ctrl 要编程到
CORESIGHT_CFG_ETR_MC_WR_CTRL
cfg_etr_mc_rd_ctrl 要编程到
CORESIGHT_CFG_ETR_MC_RD_CTRL
cfg_csite_cbb_wr_ctrl 要编程到
CORESIGHT_CFG_CSITE_CBB_WR_CTRL
cfg_csite_cbb_rd_ctrl 要编程到
CORESIGHT_CFG_CSITE_CBB_RD_CTRL
#### 11.1.6 固件加载与入口配置
固件配置如下所示：
/{
misc {
...
...
firmware {
<firmware-type> {
<parameter> = <value>;
};
}
};
};
其中 <firmware-type> 为 mb2 或 tzram-el3 之一，<parameter> 在下表中指定
字段 描述
load-offset 在 <firmware> carveout 中加载 <firmware> 二进制文件位置的
偏移量。
entry-offset <firmware> 入口点在 <firmware> carveout 中的偏移量。


<details>
<summary>English original</summary>

**11.1.3 MB1 AST Data**
Field Description
mb2_va Virtual address for MB2 carveout in BPMP-R5 address-space.
spe_fw_va Virtual address for SPE-FW carveout in AON-R5 addressspace.
misc_carveout_va Virtual address for MISC carveout in SCE-R5 address-space.
rcm_blob_carveout_va Virtual address for RCM-blob carveout in SCE-R5 addressspace.
temp_map_a_carveout_va Virtual address for temporary mapping A used while loading
binaries
temp_map_a_carveout_siz
e
Size for temporary mapping A used while loading binaries
temp_map_a_carveout_va Virtual address for temporary mapping B used while loading
binaries
temp_map_a_carveout_siz
e
Size for temporary mapping B used while loading binaries
Note: None of the above VA spaces should overlap with MMIO region or with each other.
- Size fields should be power of 2
- VA fields should be aligned to their mapping/carveouts.
**11.1.4 Carveout Configuration**
Although “SDRAM Configuration” on page 59 has MC carveout's preferred base, size and permissions, it
does not have all information required to allocate the carveouts by MB1. This additional information is
specified using miscellaneous configuration file.
For carveouts that are not protected by MC, all information, including size and preferred base address, is
specified by using the miscellaneous configuration file.
Each MC carveout configuration parameter is of the following form:
/ {
misc {
carveout {
<carveout-type> {
<parameter> = <value>;
};
};
};
};
where:
- <carveout-type> identifies the carveout and is one of the following:
carveouttype
Description
gsc@[1-31] GSC carveout for various purposes
mts MTS/CPU-uCode carveout
vpr VPR carveout
tzdram TZDRAM carveout used for SecureOS
os OS carveout used for loading OS kernel
rcm RCM carveout used for loading RCM-blob during RCM mode (temporary boot
carveout)
- <parameter> is one of the following parameters:
Parameter Description
pref_base Preferred base address of carveout
size Size of carveout (in bytes)
alignment Alignment of base address of carveout (in bytes)
ecc_protected When DRAM region-based ECC is enabled and there are non-ECC protected
DR, AM regions, whether to allocate the carveout from ECC protected region
0: Allocate from non ECC protected region 1: Allocate from ECC protected
region
bad_page_tolerant When DRAM page blacklisting is enabled, whether it is ok to have bad pages
in the carveout (only possible for very large carveouts and which are
handled completely by component that can avoid bad pages using
SMMU/MMU)
0: No bad pages allowed for the carveout 1: Bad pages allowed for the
carveout (allocation can be done without filtering bad pages)
The valid combination of the carveouts and their parameters are specified in following table:
Supported carveout-type pref_base size alignme
nt
ecc_protected bad_page_
tolerant
gsc-[1-31], mts, vpr N/A N/A YES YES YES
tzdram, mb2, cpubl, misc,
os, rcm
YES YES YES YES YES
**11.1.5 Coresight Data**
Field Description
cfg_system_ctl Value to be programmed to CORESIGHT_CFG_SYSTEM_CTL
cfg_csite_mc_wr_ctrl Value to be programmed to
CORESIGHT_CFG_CSITE_MC_WR_CTRL
cfg_csite_mc_rd_ctrl Value to be programmed to
CORESIGHT_CFG_CSITE_MC_RD_CTRL
cfg_etr_mc_wr_ctrl Value to be programmed to
CORESIGHT_CFG_ETR_MC_WR_CTRL
cfg_etr_mc_rd_ctrl Value to be programmed to
CORESIGHT_CFG_ETR_MC_RD_CTRL
cfg_csite_cbb_wr_ctrl Value to be programmed to
CORESIGHT_CFG_CSITE_CBB_WR_CTRL
cfg_csite_cbb_rd_ctrl Value to be programmed to
CORESIGHT_CFG_CSITE_CBB_RD_CTRL
**11.1.6 Firmware Load and Entry Configuration**
Firmware configuration is specified as follows:
/{
misc {
...
...
firmware {
<firmware-type> {
<parameter> = <value>;
};
}
};
};
Where <firmware-type> is one of the mb2 or tzram-el3 and <parameter> is specified in below table
Field Description
load-offset Offset in <firmware> carveout where <firmware> binary is
loaded.
entry-offset Offset of <firmware> entry point in <firmware> carveout.

</details>

#### 11.1.7 CPU 配置
字段 描述
ccplex_platform_features CPU 平台特性（应为 0）
clock_mode.clock_burst_policy CCPLEX 时钟突发策略
clock_mode.max_avfs_mode 最高 CCPLEX AVFS 模式
nafll_cfg2/fll_init CCPLEX NAFLL CFG2 [Fll Init]
nafll_cfg2/fll_ldmem CCPLEX NAFLL CFG2 [Fll Ldmem]
nafll_cfg2/fll_switch_ldmem CCPLEX NAFLL CFG2 [Fll Switch Ldmem]
nafll_cfg3 CCPLEX NAFLL CFG3
nafll_ctrl1 CCPLEX NAFLL CTRL1
nafll_ctrl2 CCPLEX NAFLL CTRL2
lut_sw_freq_req/sw_override_ndiv CCPLEX LUT 频率请求的 SW 覆盖
lut_sw_freq_req/ndiv CCPLEX LUT 频率请求的 NDIV
lut_sw_freq_req/vfgain CCPLEX LUT 频率请求的 VFGAIN
lut_sw_freq_req/sw_override_vfga
in
用于 CCPLEX LUT 频率的 VFGAIN 覆盖
请求
nafll_coeff/mdiv NAFLL 系数的 MDIV
nafll_coeff/pdiv NAFLL 系数的 PDIV
nafll_coeff/fll_frug_main NAFLL 系数的 FLL frug main
字段 描述
nafll_coeff/fll_frug_fast NAFLL 系数的 FLL frug fast
adc_vmon.enable 使能 CCPLEX ADC 电压监控器
min_adc_fuse_rev 最低 ADC fuse 修订版本
pllx_base/divm PLLX DIVM
pllx_base/divn PLLX DIVN
pllx_base/divp PLLX DIVP
pllx_base/enable 使能 PLLX
pllx_base/bypass PLLX 旁路使能


<details>
<summary>English original</summary>

**11.1.7 CPU Configuration**
Field Description
ccplex_platform_features CPU platform features (should be 0)
clock_mode.clock_burst_policy CCPLEX clock burst policy
clock_mode.max_avfs_mode Highest CCPLEX AVFS mode
nafll_cfg2/fll_init CCPLEX NAFLL CFG2 [Fll Init]
nafll_cfg2/fll_ldmem CCPLEX NAFLL CFG2 [Fll Ldmem]
nafll_cfg2/fll_switch_ldmem CCPLEX NAFLL CFG2 [Fll Switch Ldmem]
nafll_cfg3 CCPLEX NAFLL CFG3
nafll_ctrl1 CCPLEX NAFLL CTRL1
nafll_ctrl2 CCPLEX NAFLL CTRL2
lut_sw_freq_req/sw_override_ndiv SW override for CCPLEX LUT frequency request
lut_sw_freq_req/ndiv NDIV for CCPLEX LUT frequency request
lut_sw_freq_req/vfgain VFGAIN for CCPLEX LUT frequency request
lut_sw_freq_req/sw_override_vfga
in
VFGAIN override for CCPLEX LUT frequency
request
nafll_coeff/mdiv MDIV for NAFLL coefficient
nafll_coeff/pdiv PDIV for NAFLL coefficient
nafll_coeff/fll_frug_main FLL frug main for NAFLL coefficient
Field Description
nafll_coeff/fll_frug_fast FLL frug fast for NAFLL coefficient
adc_vmon.enable Enable CCPLEX ADC voltage monitor
min_adc_fuse_rev Minimum ADC fuse revision
pllx_base/divm PLLX DIVM
pllx_base/divn PLLX DIVN
pllx_base/divp PLLX DIVP
pllx_base/enable Enable PLLX
pllx_base/bypass PLLX Bypass Enable

</details>

#### 11.1.8 其他配置
字段 描述
aocluster.evp_reset_addr AON/SPE 复位向量（位于 SPE BTCM 中）
carveout_alloc_direction Carveout 分配方向
0：DRAM 末尾
1：2GB DRAM 末尾（32b 地址空间）
2：DRAM 起始
se_oem_group 要编程到 SE0_OEM_GROUP_0 / SE_RNG1_RNG1_OEM_G 的值
ROUP_0
（NVHS / NVLS / Lock 位被强制置位）
i2c-freq < <i2c controller instance> <I2C 控制器
实例的频率（单位 KHz）>> 的列表，其中，I2C 控制器实例的有效取值为 0 到 8。
11.1.8.1 MB1 软熔丝配置
MB1 中存在某些平台特定的配置或决策，它们必须在存储初始化、MB1-BCT 被读取之前确定（例如 debug 端口详情、某些 boot-mode 控制、失败情况下的行为等）。为满足该需求，BR-BCT 的签名段中新增了一个包含 64 个字段的数组，该数组对 BR 不透明，将由 MB1 消费。在 recovery 模式下，BR 不读取 BR-BCT，因此该数组保留为 RCM message 的一部分，并由 BR 拷贝到冷启动时 BR 将其作为 BR-BCT 一部分保存的位置。该数组称为 MB1 软件熔丝（soft-fuses）。
11.1.8.2 调试控制
字段 描述
verbosity 控制 UART 上 debug 日志的详细程度（取值越大，详细程度越高）：
- 0：UART 日志禁用
- 1：Critical 打印
仅 2：Error
- 3：Warn
- 4：Info
- 5：Debug
uart_instance debug 日志输出到的 UART 控制器编号：
- 0：UARTA（仅用于仿真平台）
- 2：UARTC（open-box debug）
- 5：UARTF（closed-box debug，USB-Type C 经由 DP_AUX
引脚）
- 7：UARTH（closed-box debug，USB-Type C 经由 USBOTG
usb_2_nvjtag 连接到 USB2
引脚的片上控制器：
- 0：ARMJTAG
- 1：NVJTAG
swd_usb_port_sel 应在其上配置 SWD 的 USB2 端口：
- 0：USB2 Port0
- 1：USB2 Port1
uart8_usb_port_sel 应在其上配置 UART 的 USB2 端口：
- 0：USB2 Port0
- 1：USB2 Port1
wdt_enable 在 MB1/MB2 执行期间使能 BPMP WDT 第 5 次超时（boolean）。
wdt_period_secs 每次超时的 BPMP WDT 周期（单位：秒）。
11.1.8.3 启动失败控制
字段 描述
switch_bootchain 在失败时切换 boot chain（boolean）
reset_to_recovery 在失败时触发 L1 RCM 复位（boolean）
bootchain_switch_mechanis
m
switch_bootchain 设为 1 时使用：
- 0：使用基于 BR 的 boot-chain 切换
- 1：使用基于 Android A/B 的 boot-chain 切换
bootchain_retry_count 单条 boot-chain 的最大重试次数（0-15）。
11.1.8.4 On/Off IST 模式控制
字段 描述
platform_detection_flow 在 RCM 模式下，进入平台检测流程（boolean）
enable_tegrashell 在 RCM 模式下，进入 tegrashell 模式
（仅当 FUSE_SECURITY_MODE_0 未被烧写时允许）（boolean）。
enable_IST 使能 Key ON/OFF IST 启动模式（boolean）。
enable_L0_IST 使能 Key ON（L0）IST 启动模式（boolean）。仅当
EnableIST 设为 1 时使用。
enable_dgpu_IST 在 IST 启动模式期间使能 dGPU IST（boolean）。
11.1.8.5 频率监控控制
这些内容应在与安全团队评审后为 OEM 编写文档。
字段 描述
vrefRO_calib_override 基于 SoftFuse 而非 fuse 覆盖 VrefRO Calibration Override
（boolean）
vrefRO_min_rev_threshold 若（FUSE_ECO_RESERVE_1[3:0] > vrefRO_min_rev_threshold），则基于 FUSE_VREF_CALIB_0
编程 VrefRO 频率调整目标
（在 vrefRO_calib_override 设为 0 时使用）
vrefRO_calib_val 用于编程 VrefRO 频率调整目标的 VrefRO Calibration Value
（在 vrefRO_calib_override 设为 1 时使用）
osc_threshold_low OSC 时钟的 FMON 计数器下限阈值
osc_threshold_high OSC 时钟的 FMON 计数器上限阈值
pmc_threshold_low 32K 时钟的 FMON 计数器下限阈值
pmc_threshold_high 32K 时钟的 FMON 计数器上限阈值
其他配置文件位于 bct/t23x/misc/ 目录下。


<details>
<summary>English original</summary>

**11.1.8 Other Configuration**
Field Description
aocluster.evp_reset_addr AON/SPE reset vector (in SPE BTCM)
carveout_alloc_direction Carveout allocation direction
0: End of DRAM
1: End of 2GB DRAM (32b address space)
2: Start of DRAM
se_oem_group Value to be programmed to SE0_OEM_GROUP_0 / SE_RNG1_RNG1_OEM_G
ROUP_0
(NVHS / NVLS / Lock bits are forced set)
i2c-freq List of < <i2c controller instance> <Frequency (in KHz) of I2C controller
Instance > where, I2C controller instance has valid values 0 to 8.
11.1.8.1 MB1 Soft Fuse Configurations
There are certain platform-specific configurations or decisions in MB1 that are required before th estorage
is initialized and MB1-BCT is read (for example, debug port details, certain boot-mode controls, behavior in
case of failure, and so on.). To address this requirement, an array of 64 fields has been added to signed
section of BR-BCT that is opaque to BR and will be consumed by MB1. In recovery mode, BR does not read
BR-BCT, so this array is kept part of RCM message and is copied by BR to the location that BR would have
kept it in coldboot as part of BR-BCT. This array is called MB1 software fuses (soft-fuses).
11.1.8.2 Debug Controls
Field Description
verbosity Controls verbosity of debug logs on UART (verbosity increases with increasing
value):
- 0: UART logs disabled
- 1: Critical prints
only 2: Error
- 3: Warn
- 4: Info
- 5: Debug
uart_instance UART controller number where debug logs are
spewed:
- 0: UARTA (only for simulation platforms)
- 2: UARTC (open-box debug)
- 5: UARTF (closed-box debug, USB-Type C over DP_AUX
pins)
- 7: UARTH (closed-box debug, USB-Type C over USBOTG
usb_2_nvjtag On-chip controller connected to the USB2
pins:
- 0: ARMJTAG
- 1: NVJTAG
swd_usb_port_sel USB2 port over which SWD should be
configured:
- 0: USB2 Port0
- 1: USB2 Port1
uart8_usb_port_sel USB2 port over which UART should be
configured:
- 0: USB2 Port0
- 1: USB2 Port1
wdt_enable Enable BPMP WDT 5th-expiry during execution of MB1/MB2 (boolean).
wdt_period_secs BPMP WDT period (in secs) per expiry.
11.1.8.3 Boot Failure Controls
Field Description
switch_bootchain Switch boot chain in case of failure (boolean)
reset_to_recovery Trigger L1 RCM reset on failure (boolean)
bootchain_switch_mechanis
m
Used if switch_bootchain is set to 1:
- 0: Use BR-based boot-chain switching
- 1: Use Android A/B based boot-chain switching
bootchain_retry_count Maximum number of retries for single boot-chain (0-15).
11.1.8.4 On/Off IST Mode Controls
Field Description
platform_detection_flow In RCM mode, enter platform detection flow (boolean)
enable_tegrashell In RCM mode, enter tegrashell mode
(Allowed only if FUSE_SECURITY_MODE_0 is not blown) (boolean).
enable_IST Enable Key ON/OFF IST boot mode (boolean).
enable_L0_IST Enable Key ON (L0) IST boot mode (boolean). Used
only if EnableIST is set to 1.
enable_dgpu_IST Enable dGPU IST during IST boot modes (boolean).
11.1.8.5 Frequency Monitor Controls
These should be documented for OEMs after review with security team.
Field Description
vrefRO_calib_override Override VrefRO Calibration Override based on SoftFuse instead of fuse
(boolean)
vrefRO_min_rev_threshold Program VrefRO frequency adjustment target based on FUSE_VREF_CALIB_0
if (FUSE_ECO_RESERVE_1[3:0] > vrefRO_min_rev_threshold)
(Used if vrefRO_calib_override is set to 0)
vrefRO_calib_val VrefRO Calibration Value used to program VrefRO frequency adjustment
target (Used if vrefRO_calib_override is set to 1)
osc_threshold_low Lower Threshold of FMON counter for OSC clock
osc_threshold_high Upper Threshold of FMON counter for OSC clock
pmc_threshold_low Lower Threshold of FMON counter for 32K clock
pmc_threshold_high Upper Threshold of FMON counter for 32K clock
The miscellaneous configuration files are in the bct/t23x/misc/ directory.

</details>

**NEW DTS example of miscellaneous configuration file:**

```dts
/dts-v1/;
/ {
misc {
disable_spe = <0>;
enable_vpr_resize = <0>;
disable_sc7 = <1>;
disable_fuse_visibility = <0>;
disable_mb2_glitch_protection = <0>;
carveout_alloc_direction = <2>;
//Specify i2c bus speed in KHz
i2c_freqency = <4 918>;
// Soft fuse configurations
verbosity = <4>; // 0: Disabled: 1: Critical, 2: Error, 3: Warn, 4: Info, 5:
Debug
uart_instance = <2>;
emulate_prod_flow = <0>; // 1: emulate prod flow. For
pre-prod only switch_bootchain = <0>; // 1: Switch to
alternate Boot Chain on failure
bootchain_switch_mechanism = <1>; // 1: use a/b boot
reset_to_recovery = <1>; // 1: Switch to Forced Recovery on failure
platform_detection_flow = <0>; //0: Boot in RCM flow for platform
detection nv3p_checkSum = <1>; //1 Check-sum enabled for Nv3P
command
vrefRO_calib_override = <0>; //1: program VrefRO as per soft fuse VrefRO
calibration value vrefRO_min_rev_tThreshold = <1>;
vrefRO_calib_val = <0>;
osc_threshold_low =
<0x30E>;
osc_threshold_high =
<0x375>;
pmc_threshold_low =
<0x59E>;
pmc_threshold_high =
<0x64E>;
bootchain_retry_count = <0>; //Specifies the max count to retry a particular
boot chain, Max value is
cpu {
////////// cpu variables
////////// nafll_cfg3 =
<0x38000000>; nafll_ctrl1 =
<0x0000000C>; nafll_ctrl2 =
<0x22250000>;
adc_vmon.enable = <0x0>;
min_adc_fuse_rev = <1>;
ccplex_platform_features =
<0x00000>; clock_mode {
clock_burst_policy = <15>;
max_avfs_mode = <0x2E>;
};
lut_sw_freq_req {
sw_override_ndiv =
<3>;
ndiv = <102>;
vfgain = <2>;
sw_override_vfgain = <3>;
};
nafll_coeff {
mdiv =
<3>;
pdiv =
<0x1>;
fll_frug_main = <0x9>;
fll_frug_fast = <0xb>;
};
nafll_cfg2 {
fll_init = <0xd>;
fll_ldmem = <0xc>;
fll_switch_ldmem =
<0xa>;
};
pllx_base {
divm = <2>;
divn = <104>;
divp = <2>;
enable = <1>;
bypass = <0>;
};
};
wp {
waypoint0 {
rails_shorted = <1>;
vsense0_cg0 = <1>;
};
};
////////// sw_carveout variables
////////// carveout {
misc {
size = <0x800000>; //
8MB alignment =
<0x800000>; // 8MB
};
os {
size = <0x08000000>; //128MB
pref_base =
<0x80000000>; alignment
= <0x200000>;
};
cpubl {
alignment = <0x200000>;
size = <0x04000000>; // 64MB
};
rcm {
size =
<0x0>;
alignment =
<0>;
};
mb2 {
size = <0x01000000>; //16MB
alignment = <0x01000000>; //16MB
};
tzdram {
size = <0x01000000>; //16MB
alignment = <0x00100000>; //1MB
};
////////// mc carveout alignment
////////// vpr {
alignment = <0x100000>;
};
gsc@6 {
alignment = <0x400000>;
};
gsc@7 {
alignment = <0x100000>;
};
gsc@8 {
alignment = <0x100000>;
};
gsc@9 {
alignment = <0x800000>;
};
gsc@10 {
alignment = <0x100000>;
};
gsc@12 {
alignment = <0x100000>;
};
gsc@17 {
alignment = <0x200000>;
};
gsc@19 {
alignment = <0x2000000>;
};
gsc@24 {
alignment = <0x200000>;
};
gsc@27 {
alignment = <0x200000>;
};
gsc@28 {
alignment = <0x200000>;
};
gsc@29 {
alignment = <0x200000>;
};
};
////////// mb1 ast va //////////
ast{
mb2_va = <0x52000000>;
misc_carveout_va =
<0x70000000>;
rcm_blob_carveout_va =
<0x60000000>;
temp_map_a_carveout_va =
<0x80000000>;
temp_map_a_carveout_size =
<0x40000000>;
temp_map_b_carveout_va =
<0xc0000000>;
temp_map_b_carveout_size =
<0x20000000>;
};
////////// clock variables
////////// clock {
pllaon_divp =
<0x3>;
pllaon_divn =
<0x1F>;
pllaon_divm =
<0x1>;
pllaon_divn_frac = <0x03E84000>;
// For bpmp_cpu_nic, bpmp_apb, axi_cbb and se,
// specify the divider with PLLP_OUT0 (408MHz) as source.
// For aon_cpu_nic, aon_can0 and aon_can1,
// specify the divider with PLLAON_OUT as source.
// In both cases, BCT specified value = (1 + expected divider
value). bpmp_cpu_nic_divider = <1>;
bpmp_apb_divider = <1>;
axi_cbb_divider = <1>;
se_divider = <1>;
};
////////// aotag variables
////////// aotag {
boot_temp_threshold = <97000>;
cooldown_temp_threshold = <87000>;
cooldown_temp_timeout = <30000>;
enable_shutdown = <1>;
};
//////// aocluster data
//////// aocluster {
evp_reset_addr = <0xc480000>;
};
};
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
disable_spe = 0;
enable_vpr_resize = 0;
disable_sc7 = 1;
disable_fuse_visibility = 0;
disable_mb2_glitch_protection = 0;
carveout_alloc_direction = 2;
////////// cpu variables //////////
cpu.ccplex_platform_features = 0x00000;
cpu.clock_mode.clock_burst_policy = 15;
cpu.clock_mode.max_avfs_mode = 0x2E;
cpu.lut_sw_freq_req.sw_override_ndiv = 3;
cpu.lut_sw_freq_req.ndiv = 102;
cpu.lut_sw_freq_req.vfgain = 2;
cpu.lut_sw_freq_req.sw_override_vfgain = 3;
cpu.nafll_coeff.mdiv = 3;
cpu.nafll_coeff.pdiv = 0x1;
cpu.nafll_coeff.fll_frug_main =
0x9;
cpu.nafll_coeff.fll_frug_fast =
0xb; cpu.nafll_cfg2.fll_init =
0xd; cpu.nafll_cfg2.fll_ldmem =
0xc;
cpu.nafll_cfg2.fll_switch_ldmem
= 0xa; cpu.nafll_cfg3 =
0x38000000; cpu.nafll_ctrl1 =
0x0000000C;
cpu.nafll_ctrl2 =
0x22250000;
cpu.adc_vmon.enable =
0x0; cpu.pllx_base.divm
= 2;
cpu.pllx_base.divn = 104;
cpu.pllx_base.divp = 2;
cpu.pllx_base.enable = 1;
cpu.pllx_base.bypass = 0;
cpu.min_adc_fuse_rev = 1;
wp.waypoint0.rails_shorted = 1;
wp.waypoint0.vsense0_cg0 = 1;
////////// sw_carveout variables
////////// carveout.misc.size =
0x800000; // 8MB
carveout.misc.alignment =
0x800000; // 8MB
carveout.os.size = 0x08000000;
//128MB carveout.os.pref_base =
0x80000000;
carveout.os.alignment =
0x200000;
carveout.cpubl.alignment =
0x200000;
firmware.cpubl_load_offset =
0x600000; carveout.cpubl.size =
0x04000000; // 64MB
carveout.rcm.size = 0x0;
carveout.rcm.alignment = 0;
carveout.mb2.size = 0x01000000;
//16MB carveout.mb2.alignment =
0x01000000; //16MB
carveout.tzdram.size =
0x01000000; //16MB
carveout.tzdram.alignment = 0x00100000; //1MB
////////// mc carveout alignment
////////// carveout.vpr.alignment =
0x100000; carveout.gsc[6].alignment =
0x400000; carveout.gsc[7].alignment =
0x800000; carveout.gsc[8].alignment =
0x100000; carveout.gsc[9].alignment =
0x100000; carveout.gsc[10].alignment =
0x800000; carveout.gsc[12].alignment =
0x100000; carveout.gsc[17].alignment =
0x100000; carveout.gsc[19].alignment =
0x200000; carveout.gsc[24].alignment =
0x2000000; carveout.gsc[27].alignment =
0x200000; carveout.gsc[28].alignment =
0x200000; carveout.gsc[29].alignment =
0x200000;
////////// mb1 ast va
////////// ast.mb2_va =
0x52000000;
ast.misc_carveout_va =
0x70000000;
ast.rcm_blob_carveout_va =
0x60000000;
ast.temp_map_a_carveout_va =
0x80000000;
ast.temp_map_a_carveout_size =
0x40000000;
ast.temp_map_b_carveout_va =
0xc0000000;
ast.temp_map_b_carveout_size =
0x20000000;
////////// MB2 AST VA //////////
carveout.bpmp_ast_va = 0x50000000;
carveout.ape_ast_va = 0x80000000;
carveout.apr_ast_va = 0xC0000000;
carveout.sce_ast_va = 0x70000000;
carveout.rce_ast_va = 0x70000000;
carveout.camera_task_ast_va =
0x78000000;
////////// clock variables
////////// clock.pllaon_divp =
0x3; clock.pllaon_divn = 0x1F;
clock.pllaon_divm = 0x1;
clock.pllaon_divn_frac =
0x03E84000;
// For bpmp_cpu_nic, bpmp_apb, axi_cbb and se,
// specify the divider with PLLP_OUT0 (408MHz) as source.
// For aon_cpu_nic, aon_can0 and aon_can1,
// specify the divider with PLLAON_OUT as source.
// In both cases, BCT specified value = (1 + expected
divider value). clock.bpmp_cpu_nic_divider = 1;
clock.bpmp_apb_divider = 1;
clock.axi_cbb_divider = 1;
clock.se_divider = 1;
////////// aotag variables //////////
aotag.boot_temp_threshold = 97000;
aotag.cooldown_temp_threshold = 87000;
aotag.cooldown_temp_timeout = 30000;
aotag.enable_shutdown = 1;
//Specify i2c bus speed
in KHz i2c.4 = 918;
//////// aocluster data ////////
aocluster.evp_reset_addr = 0xc480000;
//////// mb2 feature flags
//////// enable_sce = 1;
enable_rce = 1;
enable_ape = 1;
enable_combined_uart = 1;
spe_uart_instance = 2;
```

## 第 12 章 SDRAM 配置

**Mem-BCT** 承载 **SDRAM / MC / EMC** 参数。DTS 将其组织为 **memory configuration** 节点，外加可选的 **overrides**。

**形态（概念性）：**

```dts
/ {
    sdram {
        mem_cfg_<N>: mem-cfg@<N> {
            <parameter> = <value>;
        };
    };
};

&mem_cfg_<N> {
    #include "<mem_override.dtsi>"
};
```

| 符号 | 含义 |
|--------|---------|
| **`<N>`** | 备用 DRAM profile 的 memory config 索引（0、1、…）。 |
| **`<parameter>`** | 通常是 **MC/EMC** 寄存器名 / 属性。 |
| **`<value>`** | 为该参数写入的 32-bit 数据。 |
| **`mem_override.dtsi`** | 应用于给定 `mem_cfg_*` 节点的可选 include。 |

Override 文件通常就是形如 `<parameter> = <value>;` 的行。**MSS / memory HW** 常使用 NVIDIA 内部工具将 **legacy** mem BCT 转换为这种 DTS。

### 12.1 Carveout

硬件 **carveout** 为固件、安全、视频保护区（**VPR**）等划分内存。本指南中出现两类：

- **GSC**（generalized security carveout）  
- **Non-GSC**（例如 **MTS**、**VPR** 尺寸）  

**重要的 GSC 相关字段**（名称依据 DU-10990）：

| 字段 | 作用 |
|-------|------|
| **McGeneralizedCarveout&lt;N&gt;Bom** / **BomHi** | GSC carveout **N** 的首选 **base**（常为 **0**，由 MB1 决定）。 |
| **McGeneralizedCarveout&lt;N&gt;Size128kb** | 编码后的 size：高位 → **4 KB** 粒度；低位 → **128 KB** 粒度（完整公式见 TRM / 原始指南）。 |
| **McGeneralizedCarveout&lt;N&gt;Access[0–5]** | 该 carveout 的 client 访问寄存器（**TRM** 位定义）。 |
| **McVideoProtectBom** / **BomHi**、**McVideoProtectSizeMb** | **VPR** 的 base 与 size（**MB**）；另见杂项配置中的 **`enable_vpr_resize`**。 |

### 12.2 GSC carveout

**GSC 索引 → 名称**的映射（摘自 NVIDIA 列表）：

| # | 名称 |
|---|------|
| 1 | NVDEC |
| 2 | WPR1 |
| 3 | WPR2 |
| 4 | TSECA |
| 5 | TSECB |
| 6 | BPMP |
| 7 | APE |
| 8 | SPE |
| 9 | SCE |
| 10 | APR |
| 11 | TZRAM |
| 12 | IPC_SE_TSEC |
| 13 | BPMP_RCE |
| 14 | BPMP_DMCE |
| 15 | SE_SC7 |
| 16 | BPMP_SPE |
| 17 | RCE |
| 18 | CPUTZ_BPMP |
| 19 | VM_ENCRYPT1 |
| 20 | CPU_NS_BPMP |
| 21 | OEM_SC7 |
| 22 | IPC_SE_SPE_SCE_BPMP |
| 23 | SC7_RF |
| 24 | CAMERA_TASK |
| 25 | SCE_BPMP |
| 26 | CV |
| 27 | VM_ENCRYPT2 |
| 28 | HYPERVISOR |
| 29 | SMMU |

### 12.3 非 GSC carveout

| # | 名称 |
|---|------|
| 32 | MTS |
| 33 | VPR |

## 第 13 章 GPIO 中断映射配置

为降低 **GPIO** 中断延迟，**T23x** 为每个 **GPIO controller** 提供最多 **八** 条进入 **LIC** 的中断线。通过 **gpio-intmap** BCT 片段，可选择每个 **pin** 使用**哪条线（0–7）**。

**模板：**

```dts
gpio-intmap {
    port@<PORT_ID> {
        pin-<N>-int-line = <INTERRUPT_NUMBER>;
    };
};
```

| 符号 | 含义 |
|--------|---------|
| **PORT_ID** | 端口字母：`A`…`Z`、`AA`、`BB`、… |
| **N** | 端口内的 pin 编号，**0–7**。 |
| **INTERRUPT_NUMBER** | 该 pin 对应的 LIC 线 **0–7**。 |

文件位于 `hardware/nvidia/platform/t23x/<platform>/bct/`。

**GPIO 中断映射配置文件的 NEW DTS 示例：**

```dts
/dts-v1/;
/ {
    gpio-intmap {
        port@B {
            pin-1-int-line = <0>; /* GPIO B1 → INT0 */
        };
        port@AA {
            pin-0-int-line = <0>; /* GPIO AA0 → INT0 */
            pin-1-int-line = <0>; /* GPIO AA1 → INT0 */
            pin-2-int-line = <0>; /* GPIO AA2 → INT0 */
        };
    };
};
```

**legacy `.cfg` 格式（pre-DTS）：**

```text
gpio-intmap.port.B.pin.1 = 0;       // GPIO B1 → INT0
gpio-intmap.port.AA.pin.0 = 0;      // GPIO AA0 → INT0
gpio-intmap.port.AA.pin.1 = 0;      // GPIO AA1 → INT0
gpio-intmap.port.AA.pin.2 = 0;      // GPIO AA2 → INT0
```

## 第 14 章 MB2 BCT 杂项配置

杂项的 **MB2** 行为与**固件加载**地址位于此处。

### 14.1 MB2 feature 字段

**MB2** 的布尔型标志：

| 属性 | 效果 |
|----------|--------|
| **`disable-cpu-l2ecc`** | 存在 ⇒ **禁用** CPU L2 ECC；不存在 ⇒ 启用。 |
| **`enable-combined-uart`** | 存在 ⇒ 开启 **combined UART**；不存在 ⇒ 关闭。 |
| **`spe-uart-instance`** | **SPE** 用于 combined UART 的 **UART** 实例。 |


<details>
<summary>English original</summary>

**Chapter 12. SDRAM Configuration**

**Mem-BCT** carries **SDRAM / MC / EMC** parameters. DTS groups them as **memory configuration** nodes plus optional **overrides**.

**Shape (conceptual):**

```dts
/ {
    sdram {
        mem_cfg_<N>: mem-cfg@<N> {
            <parameter> = <value>;
        };
    };
};

&mem_cfg_<N> {
    #include "<mem_override.dtsi>"
};
```

| Symbol | Meaning |
|--------|---------|
| **`<N>`** | Memory config index (0, 1, …) for alternate DRAM profiles. |
| **`<parameter>`** | Usually an **MC/EMC** register name / property. |
| **`<value>`** | 32-bit data written for that parameter. |
| **`mem_override.dtsi`** | Optional include applied to a given `mem_cfg_*` node. |

Override files are typically lines like `<parameter> = <value>;`. **MSS / memory HW** often converts **legacy** mem BCT to this DTS using NVIDIA’s internal tooling.

**12.1 Carveouts**

Hardware **carveouts** partition memory for firmware, security, video protected region (**VPR**), etc. Two families appear in the guide:

- **GSC** (generalized security carveouts)  
- **Non-GSC** (e.g. **MTS**, **VPR** sizing)  

**Important GSC-related fields** (names per DU-10990):

| Field | Role |
|-------|------|
| **McGeneralizedCarveout&lt;N&gt;Bom** / **BomHi** | Preferred **base** of GSC carveout **N** (often **0** so MB1 picks). |
| **McGeneralizedCarveout&lt;N&gt;Size128kb** | Encoded size: high bits → **4 KB** granularity; low bits → **128 KB** granularity (full formula in TRM / original guide). |
| **McGeneralizedCarveout&lt;N&gt;Access[0–5]** | Client access registers for that carveout (**TRM** bit definitions). |
| **McVideoProtectBom** / **BomHi**, **McVideoProtectSizeMb** | **VPR** base and size (**MB**); see also **`enable_vpr_resize`** in miscellaneous configuration. |

**12.2 GSC carveouts**

Mapping of **GSC index → name** (excerpt from NVIDIA list):

| # | Name |
|---|------|
| 1 | NVDEC |
| 2 | WPR1 |
| 3 | WPR2 |
| 4 | TSECA |
| 5 | TSECB |
| 6 | BPMP |
| 7 | APE |
| 8 | SPE |
| 9 | SCE |
| 10 | APR |
| 11 | TZRAM |
| 12 | IPC_SE_TSEC |
| 13 | BPMP_RCE |
| 14 | BPMP_DMCE |
| 15 | SE_SC7 |
| 16 | BPMP_SPE |
| 17 | RCE |
| 18 | CPUTZ_BPMP |
| 19 | VM_ENCRYPT1 |
| 20 | CPU_NS_BPMP |
| 21 | OEM_SC7 |
| 22 | IPC_SE_SPE_SCE_BPMP |
| 23 | SC7_RF |
| 24 | CAMERA_TASK |
| 25 | SCE_BPMP |
| 26 | CV |
| 27 | VM_ENCRYPT2 |
| 28 | HYPERVISOR |
| 29 | SMMU |

**12.3 Non-GSC carveouts**

| # | Name |
|---|------|
| 32 | MTS |
| 33 | VPR |

**Chapter 13. GPIO Interrupt Mapping Configuration**

To cut **GPIO** interrupt latency, **T23x** gives each **GPIO controller** up to **eight** interrupt lines into the **LIC**. You choose **which line (0–7)** each **pin** uses via the **gpio-intmap** BCT fragment.

**Template:**

```dts
gpio-intmap {
    port@<PORT_ID> {
        pin-<N>-int-line = <INTERRUPT_NUMBER>;
    };
};
```

| Symbol | Meaning |
|--------|---------|
| **PORT_ID** | Port letter(s): `A`…`Z`, `AA`, `BB`, … |
| **N** | Pin within the port, **0–7**. |
| **INTERRUPT_NUMBER** | LIC line **0–7** for that pin. |

Files live in `hardware/nvidia/platform/t23x/<platform>/bct/`.

**NEW DTS example of GPIO interrupt mapping configuration file:**

```dts
/dts-v1/;
/ {
    gpio-intmap {
        port@B {
            pin-1-int-line = <0>; /* GPIO B1 → INT0 */
        };
        port@AA {
            pin-0-int-line = <0>; /* GPIO AA0 → INT0 */
            pin-1-int-line = <0>; /* GPIO AA1 → INT0 */
            pin-2-int-line = <0>; /* GPIO AA2 → INT0 */
        };
    };
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
gpio-intmap.port.B.pin.1 = 0;       // GPIO B1 → INT0
gpio-intmap.port.AA.pin.0 = 0;      // GPIO AA0 → INT0
gpio-intmap.port.AA.pin.1 = 0;      // GPIO AA1 → INT0
gpio-intmap.port.AA.pin.2 = 0;      // GPIO AA2 → INT0
```

**Chapter 14. MB2 BCT Misc Configuration**

Miscellaneous **MB2** behavior and **firmware load** addresses live here.

**14.1 MB2 feature fields**

Boolean-style flags for **MB2**:

| Property | Effect |
|----------|--------|
| **`disable-cpu-l2ecc`** | Present ⇒ **disable** CPU L2 ECC; absent ⇒ enabled. |
| **`enable-combined-uart`** | Present ⇒ **combined UART** on; absent ⇒ off. |
| **`spe-uart-instance`** | Which **UART** instance **SPE** uses for combined UART. |

</details>

### 14.2 MB2 固件数据

模板：

```dts
/ {
    mb2-misc {
        <firmware-type> {
            <parameter> = <value>;
        };
    };
};
```

**`<firmware-type>`** 示例：`cpubl`、`ape-fw`、`bpmp-fw`、`rce-fw`、`sce-fw`、`camera-taskfw`、`apr-fw`。

| 参数 | 描述 |
|-----------|-------------|
| **`enable`** | 存在 ⇒ MB2 从其 carveout **加载**该固件；不存在 ⇒ 不从 MB2 加载。 |
| **`ast-va`** | 固件 carveout 在 **BPMP R5** 地址空间中的虚拟地址。 |
| **`load-offset`** | 二进制文件在 carveout 内的放置偏移。 |
| **`entry-offset`** | carveout 内的入口点偏移。 |

**示例（摘自 NVIDIA 指南）：**

```dts
/dts-v1/;
/ {
    mb2-misc {
        disable-cpu-l2ecc;
        enable-combined-uart;
        spe-uart-instance = <0x2>;
        firmware {
            sce {
                ast-va = <0x70000000>;
            };
            ape {
                enable;
                ast-va = <0x80000000>;
            };
            rce {
                enable;
                ast-va = <0x70000000>;
            };
            cpubl {
                load-offset = <0x600000>;
            };
            apr {
                ast-va = <0xC0000000>;
            };
            camera-task {
                ast-va = <0x78000000>;
            };
        };
    };
};
```

## 第 15 章 安全配置

**MB1** 和 **MB2** 依据固定的 **index → register** 列表来编程 **SCR**（安全配置）和 **firewall** 寄存器。BCT 为每个 index 提供 **`exclusion-info`** 和 **`value`**。

**模板：**

```dts
/ {
    scr {
        reg@<index> {
            <parameter> = <value>;
        };
    };
};
```

| 字段 | 含义 |
|--------|---------|
| **`<index>`** | NVIDIA **预定** SCR/firewall 表中的槽位（不一定与文件顺序一致）。 |
| **`exclusion-info`** | 位掩码，控制该 SCR 在**何时**被编程（见下表）。 |
| **`value`** | 写入 SCR 的 **32 位**数据。 |

**`exclusion-info` 位：**

| 位 | 含义 |
|-----|---------|
| **0** | 在 cold boot 或 **SC7** 退出时**不**编程。 |
| **1** | 在 **cold boot** 时编程，在 **SC7** 退出时跳过。 |
| **2** | 将编程推迟到 **MB2 结束**时（而不是 MB1）。 |

**注意：** 硬件按 **index 递增顺序**应用条目，不一定与各行在 DTS 中出现的顺序一致。文件中**省略**的条目仍会以默认方式**锁定**，该方式**不会**收紧对受保护寄存器的访问（据 NVIDIA 所述）。

SCR 源文件位于 `hardware/nvidia/platform/t23x/common/bct/scr/`。

**SCR 配置文件的新 DTS 格式示例：**

```dts
/dts-v1/;
/ {
 scr {
 reg@161 {
 exclusion-info = <7>;
 value = <0x3f008080>;
 };
 reg@162 {
 exclusion-info = <4>;
 value = <0x18000303>;
 };
 reg@163 {
 exclusion-info = <4>;
 value = <0x18000303>;
 };
 };
};
```

**旧式 `.cfg` 格式（DTS 之前）：**

```text
scr.161.7 = 0x3f008080;   /* TKE_AON_SCR_WDTSCR0_0 */
scr.162.4 = 0x18000303;   /* DMAAPB_1_SCR_BR_INTR_SCR_0 */
scr.163.4 = 0x18000303;   /* DMAAPB_1_SCR_BR_SCR_0 */
```

---

*© 2022 NVIDIA Corporation。完整的保修、商标和法律文本见官方 DU-10990-001 PDF。*


<details>
<summary>English original</summary>

**14.2 MB2 firmware data**

Template:

```dts
/ {
    mb2-misc {
        <firmware-type> {
            <parameter> = <value>;
        };
    };
};
```

**`<firmware-type>`** examples: `cpubl`, `ape-fw`, `bpmp-fw`, `rce-fw`, `sce-fw`, `camera-taskfw`, `apr-fw`.

| Parameter | Description |
|-----------|-------------|
| **`enable`** | Present ⇒ MB2 **loads** this firmware from its carveout; absent ⇒ do not load from MB2. |
| **`ast-va`** | Virtual address of the firmware carveout in **BPMP R5** address space. |
| **`load-offset`** | Offset within the carveout where the binary is placed. |
| **`entry-offset`** | Entry point offset within the carveout. |

**Example (from NVIDIA guide):**

```dts
/dts-v1/;
/ {
    mb2-misc {
        disable-cpu-l2ecc;
        enable-combined-uart;
        spe-uart-instance = <0x2>;
        firmware {
            sce {
                ast-va = <0x70000000>;
            };
            ape {
                enable;
                ast-va = <0x80000000>;
            };
            rce {
                enable;
                ast-va = <0x70000000>;
            };
            cpubl {
                load-offset = <0x600000>;
            };
            apr {
                ast-va = <0xC0000000>;
            };
            camera-task {
                ast-va = <0x78000000>;
            };
        };
    };
};
```

**Chapter 15. Security Configuration**

**MB1** and **MB2** program **SCR** (security configuration) and **firewall** registers from a fixed **index → register** list. Your BCT supplies **`exclusion-info`** and **`value`** per index.

**Template:**

```dts
/ {
    scr {
        reg@<index> {
            <parameter> = <value>;
        };
    };
};
```

| Field | Meaning |
|--------|---------|
| **`<index>`** | Slot in NVIDIA’s **predetermined** SCR/firewall table (not necessarily file order). |
| **`exclusion-info`** | Bit mask controlling **when** this SCR is programmed (see table below). |
| **`value`** | **32-bit** data written to the SCR. |

**`exclusion-info` bits:**

| Bit | Meaning |
|-----|---------|
| **0** | Do **not** program at cold boot or **SC7** exit. |
| **1** | Program on **cold boot**, skip on **SC7** exit. |
| **2** | Defer programming to **end of MB2** (instead of MB1). |

**Note:** Hardware applies entries in **increasing index order**, not necessarily the order lines appear in the DTS. Entries **omitted** from the file are still **locked** in a default way that does **not** tighten access to protected registers (per NVIDIA).

SCR sources live in `hardware/nvidia/platform/t23x/common/bct/scr/`.

**NEW DTS format example of SCR config file:**

```dts
/dts-v1/;
/ {
 scr {
 reg@161 {
 exclusion-info = <7>;
 value = <0x3f008080>;
 };
 reg@162 {
 exclusion-info = <4>;
 value = <0x18000303>;
 };
 reg@163 {
 exclusion-info = <4>;
 value = <0x18000303>;
 };
 };
};
```

**Legacy `.cfg` format (pre-DTS):**

```text
scr.161.7 = 0x3f008080;   /* TKE_AON_SCR_WDTSCR0_0 */
scr.162.4 = 0x18000303;   /* DMAAPB_1_SCR_BR_INTR_SCR_0 */
scr.163.4 = 0x18000303;   /* DMAAPB_1_SCR_BR_SCR_0 */
```

---

*© 2022 NVIDIA Corporation. Full warranty, trademark, and legal text is in the official DU-10990-001 PDF.*

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/3. L4T Customization/T23x-Deployment.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/3.%20L4T%20Customization/T23x-Deployment.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
