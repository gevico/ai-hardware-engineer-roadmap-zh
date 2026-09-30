---
title: T23x 启动配置表（BCT）——部署参考
description: T23x 启动配置表（BCT）——部署参考
---

# T23x 启动配置表（BCT）——部署参考

**来源：** NVIDIA *T23x BCT Deployment Guide*（DU-10990-001），2022 年 6 月。本文件对该指南重新排版，以便在路线图中阅读。关于你所用**具体** JetPack 分支的烧写行为，请对照相应 release 的 [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/)。

**BCT 是什么：** 启动时消费的平台数据。**BootROM** 和 **MB1** 使用二进制形式的 BCT。在 T23x 上，输入是由 **`tegrabct_v2`** 解析的 **Device Tree Source（`.dtsi`）** 文件（更早的 release 使用 `parameter = value` 文本）。

**阅读提示**

- 各章对应 `hardware/nvidia/platform/t23x/<platform>/bct/` 下**独立的 `.dtsi` 片段**（路径见原始文档）。
- 许多小节并列展示 **NEW DTS** 与 **OLD CFG**，便于把电子表格 / 旧版输出与 DTS 对应起来。
- 长表格和代码依然密集；用编辑器大纲（见下方标题）跳转。
- **载板 bring-up（上电点亮/调通）（Orin NX / Nano）** 的叙述形式：[Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md)（模块适配、pinmux、DT、PCIe、USB、UPHY、flash）。请把本文件与下方 BCT 参考配合使用。

## 快速索引：哪个 BCT，由谁消费

| 产物 | 加载方 | 作用 |
|----------|-----------|------|
| **BR-BCT** | BootROM（冷启动）；恢复模式下的 MB1 | BootROM 优先；部分字段供 MB1 / CPU-BL |
| **MB1-BCT** | MB1，从存储或 USB（RCM）加载 | Pinmux、prod、pad 电压、PMIC、storage、UPHY、ratchet…… |
| **Mem-BCT** | 与 MB1-BCT 同一路径 | SDRAM / MC–EMC bring-up |
| **MB2-BCT** | MB1 为 MB2 加载 | GPIO 中断映射、安全 SCR、MB2 杂项 |

## 目录

1. [第 1 章 — 简介](#chapter-1-introduction)
2. [第 2 章 — Pinmux 和 GPIO 配置](#chapter-2-pinmux-and-gpio-configuration)
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

## 第 1 章 简介
Boot Configuration Table (BCT) 是一组平台专属的配置数据，由
启动组件消费。BootROM 和 MB1 消费二进制形式的 BCT，其由
解析设备树源配置文件（扩展名为 dts）生成，解析工具为
tegrabct_v2。
从 T23x 开始，配置文件格式已从传统的 <parameter> =
<value> 改为 Device Tree Source（DTS）格式，原因如下：
- DTS 格式支持递归包含，以及通过包含覆盖属性。
- DTC 可以把 DTS/DTSI 转换为 DTB，后者本质上是树状格式，相比当前配置文件格式更易
解析。
### 1.1 BR-BCT
BR-BCT 在冷启动时由 BootROM 从存储加载，在恢复模式下由 MB1 加载。该
BCT 的主要消费者是 BootROM，但部分字段也被 MB1 和 CPU-BL 消费。
### 1.2 MB1-BCT
MB1 BCT 由 MB1 从存储（冷启动模式）或经 USB（RCM 模式）加载。它
主要被 MB1 消费，由以下多个配置文件构成：
- “Pinmux 和 GPIO 配置”，见第 3 页
- “通用 Prod 配置”，见第 7 页
- “控制器 Prod 配置”，见第 9 页
- “Pad 电压绑定”，见第 12 页
- “PMIC 配置”，见第 14 页
- “存储设备配置”，见第 27 页
- “UPHY Lane 配置”，见第 33 页
- “OEM FW Ratchet 配置”，见第 37 页
- “BootROM 复位 PMIC 配置”，见第 37 页
- “杂项配置”，见第 43 页
### 1.3 Mem-BCT
MemBCT 在加载和使用方式上与 MB1-BCT 类似。但它主要包含
用于初始化 MC 和 EMC 的 SDRAM 参数。更多信息参见 SDRAM Configuration。
### 1.4 MB2-BCT
MB2 BCT 由 MB1 从存储（冷启动模式）或经 USB（RCM 模式）加载。它
主要被 MB2 消费。

它由以下多个配置文件构成：
- GPIO 中断映射配置
- 安全配置
- MB2 BCT 杂项配置


<details>
<summary>English original</summary>

**T23x Boot Configuration Table (BCT) — deployment reference**

**Source:** NVIDIA *T23x BCT Deployment Guide* (DU-10990-001), June 2022. This file reformats that guide for reading in the roadmap. For flashing behavior on your **exact** JetPack line, cross-check the [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) for your release.

**What BCT is:** Platform data consumed at boot. **BootROM** and **MB1** use BCT in binary form. On T23x, inputs are **Device Tree Source (`.dtsi`)** files parsed by **`tegrabct_v2`** (older releases used `parameter = value` text).

**Reading tips**

- Chapters map to **separate `.dtsi` fragments** under `hardware/nvidia/platform/t23x/<platform>/bct/` (paths in the original doc).
- Many sections show **NEW DTS** vs **OLD CFG** so you can relate spreadsheet / legacy output to DTS.
- Long tables and code are still dense; use your editor outline (headings below) to jump.
- **Carrier bring-up (Orin NX / Nano)** in prose form: [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) (module adaptation, pinmux, DT, PCIe, USB, UPHY, flash). Use this file together with the BCT reference below.

**Quick map: which BCT, who consumes it**

| Artifact | Loaded by | Role |
|----------|-----------|------|
| **BR-BCT** | BootROM (cold boot); MB1 in recovery | BootROM-first; some fields for MB1 / CPU-BL |
| **MB1-BCT** | MB1 from storage or USB (RCM) | Pinmux, prod, pad voltage, PMIC, storage, UPHY, ratchet, … |
| **Mem-BCT** | Same path as MB1-BCT | SDRAM / MC–EMC bring-up |
| **MB2-BCT** | MB1 loads for MB2 | GPIO interrupt map, security SCRs, MB2 misc |

**Table of contents**

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
Boot Configuration Table (BCT) is a set of platform-specific configuration data that is
consumed by a boot component. BootROM and MB1 consume BCT in binary form, which is
generated by parsing device tree source configuration files (with dts file extension) by
tegrabct_v2.
Starting from T23x, the config file format has changed from legacy <parameter> =
<value> to the Device Tree Source (DTS) format for following reasons:
- DTS format supports recursive inclusion and the overriding of properties though inclusion.
- DTC can convert DTS/DTSI to DTB, which is essentially tree-like format and is easier to
parse compared to the current config file format.
**1.1 BR-BCT**
BR-BCT is loaded by BootROM from storage in coldboot and by MB1 in recovery mode. The
primary consumer of this BCT is BootROM, but some fields are also consumed by MB1 and
CPU-BL.
**1.2 MB1-BCT**
MB1 BCT is loaded by MB1 from storage (in coldboot mode) or over USB (in RCM mode). It is
primarily consumed by MB1 and is constructed out of the following multiple configuration
files:
- “Pinmux and GPIO Configuration” on page 3
- “Common Prod Configuration" on page 7
- “Controller Prod Configuration” on page 9
- “Pad Voltage Binding” on page 12
- “PMIC Configuration" on page 14
- “Storage Device Configuration" on page 27
- “UPHY Lane Configuration” on page 33
- “OEM FW Ratchet Configuration” on page 37
- “BootROM Reset PMIC Configuration” on page 37
- “Miscellaneous Configuration” on page 43
**1.3 Mem-BCT**
MemBCT is like MB1-BCT in terms of loading and usage. However, it primarily contains
SDRAM parameters that are used to initialize MC and EMC. Refer to SDRAM Configuration for
more information.
**1.4 MB2-BCT**
MB2 BCT is loaded by MB1 from storage (in coldboot mode) or over USB (in RCM mode). It is
primarily consumed by MB2.

It is constructed out of multiple configuration files as listed below:
- GPIO Interrupt Mapping Configuration
- Security Configuration
- MB2 BCT Misc Configuration

</details>

## 第 2 章 Pinmux 与 GPIO 配置
pinmux 配置文件提供 pinmux 与 GPIO 配置。pinmux 与 GPIO
配置由 pinmux 电子表格生成。
新 DTS 格式相对于旧 legacy 格式的鲜明差异，源于
pinmux 表格输出。
pinmux DTS 文件存放在
hardware/nvidia/platform/t23x/<platform>/bct/ 目录下。
pinmux 配置文件的 NEW DTS 格式示例：
/*This dtsi file was generated by
e3360_1099_slt_a01.xlsm Revision: 126 */ #include
<dt-bindings/pinctrl/pinctrl-tegra.h>
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
OLD CFG 格式
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
## 第 3 章 公共 prod 配置
prod 配置是接口与控制器设置的系统特性化取值，
这些取值是给定接口在平台上可靠工作所必需的。prod
配置分别在控制器级与 pinmux/pad 级设置。本文件包含
公共的 pinmux/pad 级 prod 设置。
必需属性：
- addr-value-data：<PADCTL 寄存器绝对地址, mask, data> 的列表
对于 prod 配置文件中的每个此类条目，MB1 从指定的
地址读取数据，基于 mask 和 value 修改数据，再把数据写回该地址。
val = read(address)
val = (val & ~mask) | (value & mask); write(val, address);
公共 prod DTS 文件存放在
hardware/nvidia/platform/t23x/<platform>/bct/ 目录下。
prod 配置文件的 NEW DTS 格式示例：
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
OLD CFG 格式
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


<details>
<summary>English original</summary>

**Chapter 2. Pinmux and GPIO Configuration**
The pinmux configuration file provides pinmux and GPIO configuration. Pinmux and GPIO
configuration is generated using pinmux spreadsheet.
The stark contrast in the New DTS format with respect to old legacy format is because of the
pinmux sheet output.
The pinmux DTS file are kept in the
hardware/nvidia/platform/t23x/<platform>/bct/ directory.
NEW DTS format example of pinmux configuration file:
/*This dtsi file was generated by
e3360_1099_slt_a01.xlsm Revision: 126 */ #include
<dt-bindings/pinctrl/pinctrl-tegra.h>
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
OLD CFG format
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
**Chapter 3. Common Prod Configuration**
The prod configurations are the system characterized values of interface and controller
settings, which are required for the given interface to work reliably for a platform. The prod
configuration are set separately at controller and pinmux/pad levels. This file contains the
common pinmux/pad level prod settings.
Required properties:
- addr-value-data: List of <Absolute PADCTL register address, mask, data>
For each such entry in the prod configuration file, MB1 reads the data from the specified
address, modifies the data based on mask and value, and writes the data back to the address.
val = read(address)
val = (val & ~mask) | (value & mask); write(val, address);
The common prod DTS file are kept in the
hardware/nvidia/platform/t23x/<platform>/bct/ directory.
NEW DTS format example of the prod config file:
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
OLD CFG format
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

</details>

## 第 4 章 控制器 Prod 配置
prod 配置是接口与控制器设置的、系统表征的取值，是指定接口在平台上可靠工作所必需的。prod 配置分别在控制器层级和 pinmux/pad 层级单独设置。本文件包含控制器层级的 prod 设置。
DTS 配置文件的格式如下：
/ {
deviceprod {
<controller-name>-
<Instance> = <&Label>;
#prod-cells = <N>;
<Label>: <controller-name>@<base-address> {
<mode> {
prod = <<address offset> <mask> <value>>;
};
};
};
};
其中：
- Instance 为控制器实例 id
- Label 为分配给节点的标签，可用于引用，以将实例映射到节点
- <controller-name> 为预定义的模块名（sdmmc、qspi、se、i2c）
- <base-address> 为控制器的基地址
- <mode> 为需要应用该 prod 设置的控制器模式（例如 default、hs400 等）
- <address offset> 为相对于控制器/实例基地址的寄存器地址偏移
- <mask> 为掩码值（4 字节，无符号）
- <value> 为数据值（4 字节，无符号）
- N 指定每个配置中 prod 设置元组有多少个条目。
若 #prod-cells == 3，则每个 prod 配置有三个条目（address _offset、mask 和 value）。
传统配置格式使用设备实例 <controller-name>.<instance-index>，而不使用设备的基地址。传统格式用一个字节存储实例，但新的 DTS 格式带有基地址，在 BCT 中需要四个字节。因此，BCT 结构中的部分字段必须相应地移位。
对于 prod 配置文件中的每个条目，MB1 从指定地址读取数据，根据 mask 和 value 修改数据，并将数据写回该地址。
val = read(address)
val = (val & ~mask) | (value
& mask); write(val,
address);
通用 prod 配置文件位于 hardware/nvidia/platform/t23x/<platform>/bct/ 目录中。
prod 配置文件的新 DTS 示例：
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
旧 CFG 格式
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
## 第 5 章 Pad 电压绑定
Tegra 的引脚与 pad 设计为支持接口上的多种电压电平。它们可工作在 1.2 伏（V）、1.8 V 或 3.3 V。根据给定平台的接口与电源树，软件必须向这些 pad 写入正确的电压以使能接口。若 pad 电压高于 I/O 电源轨，引脚在该电平下无法工作。若 pad 电压低于 I/O 电源轨，则可能损坏 SoC 的 pad。因此，必须配置正确的 pad 电压，且该配置基于电源树。
Pad 电压 DTSI 使用 pinmux 电子表格生成。
prod 配置文件保存在 hardware/nvidia/platform/t23x/<platform>/bct/ 目录中。新 DTS 格式中的这种差异源于 pinmux 表输出。
pad 电压配置文件的新 DTS 格式示例：
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
旧 CFG 格式
pad-voltage.major = 1;
pad-voltage.minor = 0;
pad-voltage.0x0c36003c = 0x0000003e;
// PMC_IMPL_E_18V_PWR_0 padvoltage.0x0c360040 = 0x00000079; //
PMC_IMPL_E_33V_PWR_0


<details>
<summary>English original</summary>

**Chapter 4. Controller Prod Configuration**
The prod configurations are the system characterized values of interface and controller
settings, which are required for the given interface to work reliably for a platform. The prod
configuration are set separately at controller and pinmux/pad levels. This file contains the
controller level prod settings.
The DTS configuration file is of the following form:
/ {
deviceprod {
<controller-name>-
<Instance> = <&Label>;
#prod-cells = <N>;
<Label>: <controller-name>@<base-address> {
<mode> {
prod = <<address offset> <mask> <value>>;
};
};
};
};
where:
- Instance is controller instance id
- Label is the label assigned to the node which can be referenced for mapping instance to a
node
- <controller-name> is predefined module name (sdmmc, qspi, se, i2c)
- <base-address> is base address of the controller
- <mode> is controller mode for which the prod setting needs to be applied (e.g. default,
hs400, etc)
- <address offset> is the register address offset from base address of the
controller/instance
- <mask> is the mask value (4 bytes, unsigned)
- <value> is the data value (4 bytes, unsigned)
- N specifies how many entries are there on prod setting tuples per configurations.
if #prod-cells == 3, there are three entries per prod configuration (address _offset,
mask, and value).
The legacy config format used device instance <controller-name>.<instance-index>
instead of using the base address of the device. The legacy format keeps one byte to store the
instance, but new DTS format has a base address, which requires four bytes in the BCT. As a
result, some of the fields in the BCT structure have to be shifted accordingly.
For each entry in the prod configuration file, MB1 reads data from the specified address,
modifies the data based on the mask and value, and the data back to the address.
val = read(address)
val = (val & ~mask) | (value
& mask); write(val,
address);
The common prod configuration file is in the
hardware/nvidia/platform/t23x/<platform>/bct/ directory.
NEW DTS example of prod configuration file:
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
OLD CFG format
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
**Chapter 5. Pad Voltage Binding**
Tegra pins and pads are designed to support multiple voltage levels at an interface. They can
operate at 1.2 volts (V), 1.8 V or 3.3 V. Based on the interface and power tree of a given
platform, the software must write to the correct voltage of these pads to enable interface. If
pad voltage is higher than the I/O power rail, the pin does not work on that level. If pad voltage
is lower than the I/O power rail, it can damage the SoC pads. Consequently, configuring the
correct pad voltage is required, and this configuration is based on the power tree.
The Pad voltage DTSI is generated using pinmux spread sheet.
The prod configuration files are kept in the
hardware/nvidia/platform/t23x/<platform>/bct/ directory. The contrast in the
New DTS format is because of the pinmux sheet output.
NEW DTS format example of pad-voltage configuration file:
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
OLD CFG format
pad-voltage.major = 1;
pad-voltage.minor = 0;
pad-voltage.0x0c36003c = 0x0000003e;
// PMC_IMPL_E_18V_PWR_0 padvoltage.0x0c360040 = 0x00000079; //
PMC_IMPL_E_33V_PWR_0

</details>

## 第 6 章 PMIC 配置
系统启动期间，MB1 为 CPU、CORE、DRAM 使能系统电源轨，并完成
一些系统 PMIC 配置。以下是典型配置列表：
- 使能电源轨
- 设置电源轨电压
- FPS 配置
使能和设置电源轨电压可能需要以下平台特定
配置：
- 向设备发送 I2C 命令
- 向设备发送 PWM 命令
- 对 Tegra 寄存器进行 MMIO 访问，读-修改-写或只写
- 命令后的延迟
PMIC 配置文件中的条目要么是通用的，要么是电源轨特定的。
### 6.1 通用配置
通用配置参数适用于所有电源轨。每个 PMIC 通用
配置采用以下形式：
/ {
pmic {
<parameter> = <value>;
};
};
其中：
- <parameter> 是以下之一：
<parameter> 描述
rail-count 配置文件中的电源轨数量。
command-retries-count 允许的命令尝试次数。
wait-before-start-bus-clearus
发出总线清除命令前的等待超时，单位微秒
命令。等待时间计算为 1 << n 微秒
其中 n 由该参数提供。
### 6.2 电源轨特定配置
电源轨特定配置被划分为块。
- 每个电源轨可具有一个或多个块。
- 每个块只能有一种类型的命令（I2C、PWM 或 MMIO）。
每个 PMIC 电源轨特定配置采用以下形式：
/ {
pmic {
 <rail-name> {
 block@<index> {
<parameter> = <value>;
};
};
};
};
其中：
- <rail-name> 标识电源轨，是以下之一：
名称 描述
system 系统 PMIC 配置
cpu / cpu0 CPU 电源轨配置
cpu1 CPU 电源轨配置
core Core/SOC 电源轨配置
memio DRAM 相关电源轨配置
thermal 外部热传感器配置
名称 描述
platform 平台的其他 I2C 配置
- block-<index> 电源轨特定命令被划分为块。
每个电源轨可有多个块。给定电源轨的每个块从 0 开始索引。
- <parameter> 是以下之一：
<parameter> 描述
<type> 块中命令的类型。有效属性为 i2c-controller、
pwm 或
mmio。
commands {
<group-name> {
command@N {
reg-addr = <regaddress>; mask = <regmask>;
value = <reg-value>;
};
};
};
<group-name> 节点是逻辑命令组名称，它是
可选的。N 是命令的顺序编号，从
0. MMIO 或 I2C 命令（基于
命令类型的类型）块 描述
mmio MMIO 命令（仅当
块具有 mmio 属性时有效），
其中，
<address> 是 MMIO 寄存器的
绝对地址，且
<mask> 是 32 位掩码，它
应用于从 MMIO 地址
读取的值，以
便于
读-修改-写操作。
<value> 写入寄存器的
值
i2c-controller I2c 命令，其中
<address> 是 I2c 从设备
寄存器地址，且
<mask> 是 I2C 从设备掩码，它
应用于读取的值
<parameter> 描述
<i2c-parameter> I2C 参数（仅当块具有 i2c-controller 属性时有效），它
是以下之一：
pwm PWM 参数（仅当块具有 pwm 属性时有效），它
是以下之一：
<pwm-parameter> 描述
controller-id PWM 控制器实例（0-7）
source-frq-hz PWM 时钟源频率（单位
Hz）
period-ns PWM 时间周期（单位
纳秒）
min-microvolts 来自 PWM 稳压器的 Vout，如果占空
比为 0
max-microvolts 来自 PWM 稳压器的 Vout，如果占空
比为 100
init-microvolts 来自 PWM 稳压器的 Vout，在
初始化后
enable 0（仅配置 PWM，不
使能）；
1（在配置后使能
PWM）


<details>
<summary>English original</summary>

**Chapter 6. PMIC Configuration**
During system boot, MB1 enables system power rails for CPU, CORE, DRAM and completes
some system PMIC configurations. Here is a list of the typical configurations:
- Enabling rails
- Setting rail voltages
- FPS configurations
Enabling and setting of voltages of rails might require following platform-specific
configurations:
- I2C commands to devices
- PWM commands to devices
- MMIO accesses to Tegra registers, either read-modify-write or write-only
- Delay after the commands
The entries in PMIC configuration files are either common or rail-specific.
**6.1 Common Configuration**
The common configuration parameters are applicable to all rails. Each PMIC common
configuration is of the following form:
/ {
pmic {
<parameter> = <value>;
};
};
where:
- <parameter> is one of the following:
<parameter> Description
rail-count Number of rails in the configuration file.
command-retries-count The number of allowed command attempts.
wait-before-start-bus-clearus
Wait timeout, in microseconds before issuing the bus clear
command. The wait time is calculated as 1 << n microseconds
where n is provided by this parameter.
**6.2 Rail-Specific Configuration**
The rail-specific configuration are divided into blocks.
- Each rail can have one or more blocks.
- Each block can have only one type of commands (I2C, PWM, or MMIO).
Each PMIC rail-specific configuration is of the following form:
/ {
pmic {
 <rail-name> {
 block@<index> {
<parameter> = <value>;
};
};
};
};
where:
- <rail-name> identifies the rail and is one of the following:
Name Description
system System PMIC configuration
cpu / cpu0 CPU rail configuration
cpu1 CPU rail configuration
core Core/SOC rail configuration
memio DRAM related rail configuration
thermal External thermal sensor configuration
Name Description
platform Platform's other I2C configuration
- block-<index> The rail specific commands are divided into blocks.
Each rail can have multiple blocks. Each block of given rails indexed starting from 0.
- <parameter> is one of the following:
<parameter> Description
<type> Type of commands in the block. Valid properties are i2c-controller,
pwm or
mmio.
commands {
<group-name> {
command@N {
reg-addr = <regaddress>; mask = <regmask>;
value = <reg-value>;
};
};
};
<group-name> node is the logical command group name and it is
OPTIONAL. N is the sequential number for the command starting from
0. MMIO or I2C commands (based on
Type of command type) block Description
mmio MMIO command (valid only if
block has mmio property),
where,
<address> is absolute address
of MMIO register and
<mask> is the 32bit mask that
is applied to the value read
from the MMIO address to
facilitate
read-modify-write operation.
<value> value written into
the register
i2c-controller I2c command, where
<address> is the I2c slave
register address, and
<mask> is I2C slave mask that
is applied to the value read
<parameter> Description
<i2c-parameter> I2C parameter (valid only if block has i2c- controller property), which
is one of the following:
pwm PWM parameter (valid only if block has pwm property), which is one
of the following:
<pwm-parameter> Description
controller-id PWM controller instance (0-7)
source-frq-hz PWM clock source frequency (in
Hz)
period-ns PWM time period (in
nanoseconds)
min-microvolts Vout from PWM regulator if duty
cycle is 0
max-microvolts Vout from PWM regulator if duty
cycle is 100
init-microvolts Vout from PWM regulator after
initialization
enable 0 (just configure PWM, do
not enable);
1 (enable PWM after
configuring)

</details>

### 6.3 MB1 执行 PMIC 配置的相对顺序

除了各 rail 配置执行的顺序和所处的 boot 阶段不同之外，MB1 处理这些配置的方式没有区别。视平台不同，其中一些配置可能是可选的。因此，MB1 把所有配置都视为可选，只在 MB1-BCT 中未提供某个配置时打印一条警告。

<i2c-parameter> 描述
block-delay 块中每条命令之后的延迟（单位微秒）。
<index> 为块索引
（从 0 开始）。
i2c-controller-id I2C 控制器实例
slave-addr 7 位 I2C 从机地址
reg-data-size 寄存器大小，单位 bit。有效值为 0(1-byte)、8(1-byte) 和
16(2-byte)
reg-addr-size 寄存器地址大小，单位 bit。
有效值为 0(1-byte)
这些序列由 MB1 按以下顺序执行：
1. 外部温度传感器配置。
2. 通用 PMIC 配置。
3. SOC rail 配置。
4. DRAM 相关 rail 配置。
5. DRAM 初始化。
6. CPU rail 配置。
7. 加载 CPU 相关微码并使能 CPU。
8. 平台的其他 I2C 配置。
PMIC 配置文件保存在
hardware/nvidia/platform/t23x/<platform>/bct/ 目录下。
PMIC 配置文件的 NEW DTS 格式示例
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
OLD CFG 格式
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
<a id="chapter-7-storage-device-configuration"></a>


<details>
<summary>English original</summary>

**6.3 Relative Order of Execution of the**
PMIC Configuration by MB1
Apart from the order and point in boot in which the different rail configurations are executed,
there is no difference in how MB1 treats each of these configurations. Depending on the
platform, some of these configurations might be optional. Therefore, MB1 treats all
configurations as optional and prints only a warning when a configuration is not provided in the
MB1-BCT.
<i2c-parameter> Description
block-delay Delay (in microseconds), after
each command in the block.
<index> is the block index
(starting from 0).
i2c-controller-id I2C controller instance
slave-addr 7-bit I2C slave address
reg-data-size Register size in bits. Valid values
are 0(1-byte), 8(1-byte) and
16(2-byte)
reg-addr-size Register address size in bits.
Valid values are 0(1-byte)
These sequences are executed in the following order by MB1:
1. External thermal sensor configuration.
2. Generic PMIC configuration.
3. SOC rail configuration.
4. DRAM related rail configuration.
5. DRAM initialization.
6. CPU rail configuration.
7. Loading CPU related microcode and enabling CPUs.
8. Platform's other I2C configuration.
The PMIC configuration files are kept in the
hardware/nvidia/platform/t23x/<platform>/bct/ directory.
NEW DTS format example of PMIC configuration file
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
OLD CFG format
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
<a id="chapter-7-storage-device-configuration"></a>

</details>

## 第 7 章 存储设备配置
存储设备配置文件包含存储设备在
MB1/MB2 阶段的平台特定设置。
DTS 配置文件的格式如下：
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
- <parameter> 是如下所示的控制器特定参数。
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
enable-ddr-mode 0：QSPI SDR 模式
1：QSPI DDR 模式
参数 描述
maximum-bus-width QSPI 最大总线宽度
0：QSPI x1 通道
2：QSPI x4 通道
fifo-access-mode 0：PIO 模式
1：DMA 模式
ready-dummy-cycle 根据 QSPI flash 的 dummy cycle 数量
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
max-active-lanes UFS lane 最大数量（1-2）
page-align-size 用于 UFS 数据结构的页对齐（单位：字节）
enable-hs-mode 是否启用 UFS HS 模式
0：禁用
1：启用
enable-fast-auto-mode 启用快速自动模式
0：禁用
1：启用
enable-hs-rate-a 启用 HS rate A
0：禁用
1：启用
参数 描述
enable-hs-rate-b 启用 HS rate B
0：禁用
1：启用
init-state UFS 设备在 MB1 入口处的初始状态
0：UFS 未由 BootROM 初始化
1：UFS 已由 BootROM 初始化（MB1 可跳过某些步骤）
### 7.4 SATA 参数
参数 描述
transfer-speed 0: GEN1
1: GEN2
存储设备配置文件保存在
hardware/nvidia/platform/t23x/<platform>/bct/ 目录中。
存储设备配置文件的 NEW DTS 格式示例：
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
OLD CFG 格式
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
<a id="chapter-8-uphy-lane-configuration"></a>


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
NEW DTS format example of storage device configuration file:
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
OLD CFG format
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
<a id="chapter-8-uphy-lane-configuration"></a>

</details>

## 第 8 章. UPHY lane 配置

UPHY lane 可配置为归属于多种 IP，例如 XUSB、NVME、MPHY、PCIE、NVLINK 等。对于需要配置为访问 MB1 与 MB2 中存储的 UPHY lane，MB1 支持 NVME、UFS 作为启动设备。本文件定义 MB1 所需的 UPHY lane 配置。

在 T23x 中，BPMP-FW 由 MB1 加载，MB2 依赖 BPMP-FW 完成 UPHY 配置。

配置文件中每个条目的形式如下：

/ {
uphy-lane {
<instance-type> {
lane-owner-map = < <id> <ownerid> >,
< <id> <owner-id> >;
};
};
};

其中：

- <instance-type> 是需要配置的 UPHY 类型，可以是 hsio 或 nvhs
- <uphy-component> 是需要配置的 lane 或 pll
- <id> 是需要配置的 lane/pll 编号
- <owner-id> 是 lane/pll 将要分配给的所有者的唯一 id

UPHY lane 配置保存在 hardware/nvidia/platform/t23x/<platform>/bct/ 目录中。

新 DTS 示例 uphy lane DTS 配置文件与旧 CFG 文件格式：

/dts-v1/;
/ {
uphy-lane {
 hsio {
 lane-owner-map = <10 2>,
 <11 1>;
 };
 };
};
旧 CFG 格式
//UPHY
uphy-lane.major = 1;
uphy-lane.minor = 0;
uphy-lane.hsio.lane.10 = 2;
uphy-lane.hsio.lane.11 = 1;
<a id="chapter-9-oem-fw-ratchet-configuration"></a>
## 第 9 章. OEM FW Ratchet 配置

oem-fw 的防回滚通过 OEM-FW Ratchet 配置控制。Ratcheting 是指阻止加载较老版本的软件。软件修复安全缺陷后，其 ratchet 版本会递增，加载前会将该版本与存储在该软件 Boot Component Header(BCH) 中的版本进行比较。本文件定义 OEM-FW 组件的最低 ratchet 级别。如果 BCH 中的版本低于 BCT 中的最低 ratchet 级别，则不会加载该二进制/固件。

配置文件中每个条目的形式如下：

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
- <loader_name#> 是 Boot Stage 二进制文件的名称，该二进制文件加载与 fw_index 对应的固件。
- <fw_name#> 是固件的名称。
- <ratchet_value> 是该固件的 ratchet_value。

ratchet 配置文件位于 hardware/nvidia/platform/t23x/<platform>/bct/ratchet 目录中。

新 DTS 示例

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
旧 CFG 格式
//ratchet
ratchet.1.
mb1.mb1bct
= 3;
ratchet.2.mb1.spefw = 0;
ratchet.11.mb2.cpubl = 5;
<a id="chapter-10-bootrom-reset-pmic-configuration"></a>
## 第 10 章. BootROM 复位 PMIC 配置

对某些 T23x 平台，在 L1 与 L2 复位启动路径中，可能要求 BootROM 将 PMIC 电源轨带到 OTP 值。该过程通过发出 I2C 命令完成，这些命令由 MB1 编码到 AO scratch 寄存器中，并基于 MB1 BCT 中的 BootROM 复位配置。

- BootROM 发出这些命令的复位情形包括：
- Watchdog 5 到期
- Watchdog 4 到期
- SC7 退出
- SC8 退出
- SW-Reset
- AO-Tag/sensor 复位
- VF Sensor 复位
- HSM 复位
- 每个复位情形可以有三组 AO 命令块。
- 每个 AO 块有多个块，每个块可以有多条命令。

在配置文件中，先指定 AO 块，然后使用 AO 块的 ID 初始化复位条件。


<details>
<summary>English original</summary>

**Chapter 8. UPHY Lane Configuration**
UPHY lanes can be configured to be owned by various IPs such as XUSB, NVME, MPHY, PCIE,
NVLINK, and so on. MB1 supports NVME, UFS as boot devices for the UPHY lanes that need to
be configured to access the storage in MB1 and MB2. This file defines the UPHY lane
configuration that is necessary for MB1.
In T23x, BPMP-FW is loaded by MB1 and MB2 relies on BPMP-FW for UPHY configuration.
Each entry in the configuration file is of the following form:
/ {
uphy-lane {
<instance-type> {
lane-owner-map = < <id> <ownerid> >,
< <id> <owner-id> >;
};
};
};
Where:
- <instance-type> is the type of UPHY which needs to be configured, it can be hsio or nvhs
- <uphy-component> is either lane or pll which needs to be configured
- <id> is the lane/pll number which needs to be configured
- <owner-id> is the unique id of the owner to which lane/pll will be assigned
The UPHY lane configurations are kept in the
hardware/nvidia/platform/t23x/<platform>/bct/ directory.
NEW DTS example uphy lane DTS configuration file and old CFG file format:
/dts-v1/;
/ {
uphy-lane {
 hsio {
 lane-owner-map = <10 2>,
 <11 1>;
 };
 };
};
OLD CFG format
//UPHY
uphy-lane.major = 1;
uphy-lane.minor = 0;
uphy-lane.hsio.lane.10 = 2;
uphy-lane.hsio.lane.11 = 1;
<a id="chapter-9-oem-fw-ratchet-configuration"></a>
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
NEW DTS example
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
OLD CFG format
//ratchet
ratchet.1.
mb1.mb1bct
= 3;
ratchet.2.mb1.spefw = 0;
ratchet.11.mb2.cpubl = 5;
<a id="chapter-10-bootrom-reset-pmic-configuration"></a>
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
配置文件中每条与 AO block 相关的行均采用以下格式：
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
其取值必须为以下之一：
watchdog5, watchdog4, sc7,sc8,
soft-reset, sensor-aotag,
vfsensor 或 hsm。
- <AO-command-index> 是 AO 命令的索引，
可取值为 0、1 或 2。
每条复位路径最多
可指向三个 aoblock
- <AoBblock-Label> 是赋予
某个 AO Block 的标签
aoblock@<aoblockindex>
command-retries-count 指定允许的命令尝试次数，
针对 <aoblockindex> 的 AO-block
delay-between-command-us 指定延迟（单位：微秒），位于
不同命令之间。
该延迟按 1 << n
微秒计算，其中 n 由该
参数提供。
wait-before-start-bus-clear-us 指定等待超时（单位：微秒），
在发出 bus clear 命令之前，针对
给定的 AO block。
等待时间按 1 << n
微秒计算，其中 n 由该
参数提供。
block@<block-index> <command-type>/td> <command-type> 只能取一个
值 - i2c-controller。这是唯一
支持的值
count 指定命令的数量，位于
block <block-index> 中。
i2c-controller-id I2C 控制器实例
slave-addr 7 位 I2C 从机地址
reg-data-size 寄存器大小，单位 bit。有效值为 0(1-byte)、
8(1-byte) 和 16(2-byte)，
reg-addr-size 寄存器地址大小，单位 bit。有效值为
0(1-byte)、8(1-byte) 和 16(2-byte)
节点 <parameter> 描述
commands <Address Value> 对的列表，其中的 value
要写入 I2C 从机寄存器地址
<reg-addr>，对应于由
<command- index> 索引的命令。
BootROM 复位配置文件的新版 DTS 示例：
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
旧版 CFG 格式
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
<a id="chapter-11-miscellaneous-configuration"></a>


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
NEW DTS example of BootROM reset configuration file:
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
OLD CFG format
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
<a id="chapter-11-miscellaneous-configuration"></a>

</details>

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
字段 说明
aocluster.evp_reset_addr AON/SPE 复位向量（位于 SPE BTCM 中）
carveout_alloc_direction Carveout 分配方向
0：DRAM 末尾
1：2GB DRAM 末尾（32 位地址空间）
2：DRAM 起始处
se_oem_group 待写入 SE0_OEM_GROUP_0 / SE_RNG1_RNG1_OEM_G 的值
ROUP_0
（NVHS / NVLS / Lock 位被强制置位）
i2c-freq <<i2c controller instance> <I2C 控制器的频率（单位 KHz）> 的列表
Instance > 其中，I2C controller instance 的合法取值为 0 到 8。
11.1.8.1 MB1 软熔丝配置
MB1 中存在一些平台特定的配置或决策，必须在存储初始化、MB1-BCT 被读取之前完成（例如调试端口细节、某些 boot-mode 控制、失败时的行为等）。为满足该需求，BR-BCT 的签名段中新增了一个 64 字段的数组，该段对 BR 不透明，将由 MB1 使用。在 recovery 模式下，BR 不读取 BR-BCT，因此该数组作为 RCM 消息的一部分保留，并由 BR 拷贝到冷启动时它作为 BR-BCT 一部分本应存放的位置。该数组称为 MB1 软件熔丝（soft-fuses）。
11.1.8.2 调试控制
字段 说明
verbosity 控制 UART 上调试日志的详细程度（取值越大，详细程度越高）：
- 0：UART 日志禁用
- 1：仅 Critical 打印
仅 2：Error
- 3：Warn
- 4：Info
- 5：Debug
uart_instance 输出调试日志的 UART 控制器编号：
- 0：UARTA（仅用于仿真平台）
- 2：UARTC（open-box 调试）
- 5：UARTF（closed-box 调试，USB-Type C 经 DP_AUX 引脚）
- 7：UARTH（closed-box 调试，USB-Type C 经 USBOTG）
usb_2_nvjtag 连接到 USB2 引脚的片上控制器：
- 0：ARMJTAG
- 1：NVJTAG
swd_usb_port_sel 应在其上配置 SWD 的 USB2 端口：
- 0：USB2 Port0
- 1：USB2 Port1
uart8_usb_port_sel 应在其上配置 UART 的 USB2 端口：
- 0：USB2 Port0
- 1：USB2 Port1
wdt_enable 在 MB1/MB2 执行期间启用 BPMP WDT 第 5 次超时（布尔值）。
wdt_period_secs 每次超时的 BPMP WDT 周期（单位：秒）。
11.1.8.3 启动失败控制
字段 说明
switch_bootchain 失败时切换 boot chain（布尔值）
reset_to_recovery 失败时触发 L1 RCM 复位（布尔值）
bootchain_switch_mechanis
m 在 switch_bootchain 置为 1 时使用：
- 0：使用基于 BR 的 boot-chain 切换
- 1：使用基于 Android A/B 的 boot-chain 切换
bootchain_retry_count 单条 boot-chain 的最大重试次数（0-15）。
11.1.8.4 On/Off IST 模式控制
字段 说明
platform_detection_flow 在 RCM 模式下进入平台检测流程（布尔值）
enable_tegrashell 在 RCM 模式下进入 tegrashell 模式
（仅当 FUSE_SECURITY_MODE_0 未烧写时允许）（布尔值）。
enable_IST 启用 Key ON/OFF IST 启动模式（布尔值）。
enable_L0_IST 启用 Key ON（L0）IST 启动模式（布尔值）。仅在 EnableIST 置为 1 时使用。
enable_dgpu_IST 在 IST 启动模式期间启用 dGPU IST（布尔值）。
11.1.8.5 频率监控控制
这些内容应在与安全团队评审后向 OEM 提供文档。
字段 说明
vrefRO_calib_override 基于 SoftFuse 而非 fuse 覆盖 VrefRO 校准值（布尔值）
vrefRO_min_rev_threshold 若 (FUSE_ECO_RESERVE_1[3:0] > vrefRO_min_rev_threshold)，则基于 FUSE_VREF_CALIB_0 配置 VrefRO 频率调整目标值
（在 vrefRO_calib_override 置为 0 时使用）
vrefRO_calib_val 用于配置 VrefRO 频率调整目标值的 VrefRO 校准值
（在 vrefRO_calib_override 置为 1 时使用）
osc_threshold_low OSC 时钟的 FMON 计数器下限阈值
osc_threshold_high OSC 时钟的 FMON 计数器上限阈值
pmc_threshold_low 32K 时钟的 FMON 计数器下限阈值
pmc_threshold_high 32K 时钟的 FMON 计数器上限阈值
杂项配置文件位于 bct/t23x/misc/ 目录下。
杂项配置文件的新 DTS 示例：
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
OLD CFG 格式
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
<a id="chapter-12-sdram-configuration"></a>


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
NEW DTS example of miscellaneous configuration file:
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
OLD CFG format
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
<a id="chapter-12-sdram-configuration"></a>

</details>

## 第 12 章 SDRAM 配置
SDRAM 配置的 DTS 格式：
/
{
sdram {
mem_cfg_<N>: mem-cfg@<N> {
<parameter> = <value>;
};
};
};
&mem_cfg_<N> {
#include " <mem_override_dts >"
};
其中
- <N> 是从 0 开始的编号，代表不同的内存配置
- <parameter> 是 SDRAM 参数。通常对应 MC/EMC 寄存器。
- <value> 是相应寄存器的 32bit 值。
- <mem_override_dts> 是 SDRAM 参数的 override 文件，将应用于所有配置
override dts 的格式为：
<parameter> = <value>; // example McVideoProtectBom = <0x00000000>;
Memory/MSS HW 团队将使用 SW 提供的工具，把传统配置格式转换为上述 DTS
格式。
### 12.1 Carveout
可用的硬件 carveout 如下：
- GSC carveout
- 非 GSC carveout BCT cfg 中的重要 carveout 字段
BCT cfg 中的重要 carveout 字段。
字段 说明
McGeneralizedCarveout<N>Bom and Mc
GeneralizedCarveout<N>BomHi
（其中 N 是 GSC 编号）
GSC carveout 的首选基地址（推荐值 = 0；允许 MB1 自行分配）
McGeneralizedCarveout<N>Size128kb
（其中 N 是 GSC 编号）
31:27 - 广义安全 carveout 区域的大小，粒度为 4kb
11:0 - 广义安全 carveout 区域的大小，
粒度为 128kb
Size = (MC_SECURITY_CARVEOUT1_SIZE_RAN,
GE_128KB << 17) | (MC_SECURITY_CARVEOU,
T1_SIZE_RANGE_4KB << 12)
McGeneralizedCarveout<N>Access[0-5]
（其中 N 是 GSC 编号）
Generalized Carveout 的客户端访问寄存器。位
描述的详细信息请参考 TRM
McVideoProtectBom 和 McVideoProtectBomHi VPR carveout 的首选基地址（推荐值 = 0；允许 MB1 自行分配）
McVideoProtectSizeMb 指定 VPR carveout 大小（单位为 MB）（另见
“Miscellaneous
Configuration 中的 enable_vpr_resize）。
### 12.2 GSC Carveout
下表给出 Carveout 名称与其对应 GSC 编号的映射：
Carveout # Carveout 名称
1 NVDEC
2 WPR1
3 WPR2
4 TSECA
5 TSECB
6 BPMP
7 APE
8 SPE
9 SCE
10 APR
11 TZRAM
12 IPC_SE_TSEC
13 BPMP_RCE
Carveout # Carveout 名称
14 BPMP_DMCE
15 SE_SC7
16 BPMP_SPE
17 RCE
18 CPUTZ_BPMP
19 VM_ENCRYPT1
20 CPU_NS_BPMP
21 OEM_SC7
22 IPC_SE_SPE_SCE_BPMP
23 SC7_RF
24 CAMERA_TASK
25 SCE_BPMP
26 CV
27 VM_ENCRYPT2
28 HYPERVISOR
29 SMMU
### 12.3 非 GSC Carveout
Carveout # Carveout 名称
32 MTS
33 VPR
<a id="chapter-13-gpio-interrupt-mapping-configuration"></a>
## 第 13 章 GPIO 中断映射配置
为缩短单个 ISR 查找 GPIO 引脚中断的时间，T23x 中每个 GPO 控制器有 8 条
连到 LIC 的中断线。这样就有机会把 GPIO 引脚映射到 8 个中断中的任意一个。其配置
在 GPIO 中断配置文件中指定。
此配置文件中的每个条目形式如下：
gpio-intmap {
port@<PORT-ID> {
pin-<N>-int-line = <INTERRUPT-NUMBER>;
};
port@<PORT-ID> {
pin-<N>-int-line = <INTERRUPT-NUMBER>;
};
};
其中：
- <PORT-ID> 是端口名，如 A、B、C..Z、AA、BB
- <N> 是端口内的引脚 id。有效值为 0-7。
- <INTERRUPT-NUMBER> 是该引脚的中断路由。有效值为 0-7。
gpio-interrupt 映射配置文件存放在 hardware/nvidia/platform/t23x/<platform>/bct/
GPIO 中断映射配置文件的新 DTS 示例：
/dts-v1/;
/ {
gpio-intmap {
port@B {
pin-1-int-line = <0>; // GPIO B1 to INT0
};
port@AA {
pin-0-int-line = <0>; // GPIO
AA0 to INT0 pin-1-int-line =
<0>; // GPIO AA1 to INT0 pin-2-
int-line = <0>; // GPIO AA2 to
INT0
};
};
};
旧的 CFG 格式
gpio-intmap.port.B.pin.1 = 0; // GPIO
B1 to INT0 gpio-intmap.port.AA.pin.0 =
0; // GPIO AA0 to INT0 gpiointmap.port.AA.pin.1 = 0; // GPIO AA1
to INT0 gpio-intmap.port.AA.pin.2 = 0;
// GPIO AA2 to INT0
<a id="chapter-14-mb2-bct-misc-configuration"></a>
## 第 14 章 MB2 BCT 杂项配置
### 14.1 MB2 特性字段
这些特性位是布尔标志，用于启用或禁用 MB2 中的某些功能。
字段 说明
disable-cpu-l2ecc 若存在此属性，则 CPU L2 ECC 被禁用。否则启用。
enable-combineduart
若存在此属性，则启用 combined uart
否则禁用。
spe-uart-instance SPE 用于 combined UART 的 UART 控制器。


<details>
<summary>English original</summary>

**Chapter 12. SDRAM Configuration**
DTS format for SDRAM configuration:
/
{
sdram {
mem_cfg_<N>: mem-cfg@<N> {
<parameter> = <value>;
};
};
};
&mem_cfg_<N> {
#include " <mem_override_dts >"
};
where
- <N> is number starting from 0 representing different memory configurations
- <parameter> is the SDRAM parameter. Usually, this corresponds to MC/EMC register.
- <value> is the 32bit value of the corresponding register.
- <mem_override_dts> is override file for SDRAM parameters which will be applied to all the configs
Format of override dts will be:
<parameter> = <value>; // example McVideoProtectBom = <0x00000000>;
Memory/MSS HW team will use a tool provided by SW to convert legacy configuration format to above DTS
format.
**12.1 Carveouts**
The following hardware carveouts are available:
- GSC carveouts
- Non-GSC carveouts Important carveout fields in BCT cfg
Important carveout fields in BCT cfg.
Field Description
McGeneralizedCarveout<N>Bom and Mc
GeneralizedCarveout<N>BomHi
(where N is the GSC#)
Preferred base address of the GSC carveout (recommended value = 0; allows MB1 to allocate on its own)
McGeneralizedCarveout<N>Size128kb
(where N is the GSC#)
31:27 - Size of the generalized security carveout region with 4kb granularity
11:0 - Size of the generalized security carveout region
with 128kb granularity
Size = (MC_SECURITY_CARVEOUT1_SIZE_RAN,
GE_128KB << 17) | (MC_SECURITY_CARVEOU,
T1_SIZE_RANGE_4KB << 12)
McGeneralizedCarveout<N>Access[0-5]
(where N is the GSC#)
Client Access Register for Generalized Carveout. For bit
description details refer to TRM
McVideoProtectBom and McVideoProtectBomHi Preferred base address of VPR carveout (recommended value = 0; allows MB1 to allocate on its own)
McVideoProtectSizeMb Specifies VPR carveout size (in MB) (see also
enable_vpr_resize in “Miscellaneous
Configuration).
**12.2 GSC Carveouts**
The following list gives a mapping between Carveout names and its corresponding GSC numbers:
Carveout # Carveout Name
1 NVDEC
2 WPR1
3 WPR2
4 TSECA
5 TSECB
6 BPMP
7 APE
8 SPE
9 SCE
10 APR
11 TZRAM
12 IPC_SE_TSEC
13 BPMP_RCE
Carveout # Carveout Name
14 BPMP_DMCE
15 SE_SC7
16 BPMP_SPE
17 RCE
18 CPUTZ_BPMP
19 VM_ENCRYPT1
20 CPU_NS_BPMP
21 OEM_SC7
22 IPC_SE_SPE_SCE_BPMP
23 SC7_RF
24 CAMERA_TASK
25 SCE_BPMP
26 CV
27 VM_ENCRYPT2
28 HYPERVISOR
29 SMMU
**12.3 Non-GSC Carveouts**
Carveout # Carveout Name
32 MTS
33 VPR
<a id="chapter-13-gpio-interrupt-mapping-configuration"></a>
**Chapter 13. GPIO Interrupt Mapping Configuration**
To reduce the interrupt hunt time for GPIO pins from single ISR, in T23x, each GPO controller has 8
interrupt lines to LIC. This gives opportunity to map the GPIO pin to any of 8 interrupts. The configuration for
the same is specified in the GPIO interrupt configuration file.
Each entry in this configuration file is in the following form:
gpio-intmap {
port@<PORT-ID> {
pin-<N>-int-line = <INTERRUPT-NUMBER>;
};
port@<PORT-ID> {
pin-<N>-int-line = <INTERRUPT-NUMBER>;
};
};
where:
- <PORT-ID> is the port name like A, B, C..Z, AA, BB
- <N> is the pin id in the port. Valid values are 0-7.
- <INTERRUPT-NUMBER> is interrupt route for that pin. Valid values are 0-7.
The gpio-interrupt mapping configuration file are kept at hardware/nvidia/platform/t23x/<platform>/bct/
NEW DTS example of GPIO interrupt mapping configuration file:
/dts-v1/;
/ {
gpio-intmap {
port@B {
pin-1-int-line = <0>; // GPIO B1 to INT0
};
port@AA {
pin-0-int-line = <0>; // GPIO
AA0 to INT0 pin-1-int-line =
<0>; // GPIO AA1 to INT0 pin-2-
int-line = <0>; // GPIO AA2 to
INT0
};
};
};
OLD CFG format
gpio-intmap.port.B.pin.1 = 0; // GPIO
B1 to INT0 gpio-intmap.port.AA.pin.0 =
0; // GPIO AA0 to INT0 gpiointmap.port.AA.pin.1 = 0; // GPIO AA1
to INT0 gpio-intmap.port.AA.pin.2 = 0;
// GPIO AA2 to INT0
<a id="chapter-14-mb2-bct-misc-configuration"></a>
**Chapter 14. MB2 BCT Misc Configuration**
**14.1 MB2 Feature Fields**
These feature bits are boolean flags that enable or disable certain functionality in MB2.
Field Description
disable-cpu-l2ecc If this property is present CPU L2 ECC is disabled. Otherwise, it is enabled.
enable-combineduart
If this property is present combined uart is enabled
Otherwise, it is disabled.
spe-uart-instance UART controller used for combined UART by SPE.

</details>

### 14.2 MB2 固件数据
Mb2 固件配置的指定形式为：
/ {
mb2-misc {
<firmware-type> {
<parameter> = <value>;
};
};
};
其中固件类型为 cpubl、ape-fw、bpmp-fw、rce-fw、sce-fw、camera-taskfw、apr-fw 之一。
<parameter> 为下表参数之一。
参数 描述
enable 若存在该属性，则启用从 MB2 加载固件；否则禁用
从 MB2 加载固件。
ast-va 固件 carveout 在 BPMP-R5 地址空间中的虚拟地址。
load-offset 固件二进制在固件 carveout 中的加载偏移。
entry-offset 固件入口点在固件 carveout 中的偏移。
Mb2 misc DTS 配置文件示例：
/dts-v1/;
/ {
mb2-misc {
disable-cpul2ecc; enable-combined-uart;
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
<a id="chapter-15-security-configuration"></a>
## 第 15 章 安全配置
MB1 和 MB2 会配置 T23x 中大部分的 SCR 和防火墙。SCR/防火墙的列表、顺序及地址都是预先确定的。取值来自 SCR 配置文件。
该配置文件中的每个条目形式如下：
/ {
scr {
reg@<index> {
<parameter> = <value>;
};
};
};
其中：
- <index> 是该 SCR/防火墙在预定义列表中的索引
- <parameter> 及其 <value> 可以如下：
参数 取值
exclusion-info 排除信息（exclusion info）是如下定义的位域
位 描述
0 在 coldboot 或 SC7-exit 中不进行配置
1 在 coldboot 中配置，但在 SC7-exit 中跳过
2 在 MB2 末尾配置，而非在 MB1 中配置
value SCR 寄存器的 32 位取值
注意：SCR 的取值按下标递增的顺序配置，而不是按它们在配置文件中出现的顺序。未在配置文件中指定的 scr/防火墙会被锁定，同时不限制对受保护寄存器的访问。
scr 配置文件存放在
hardware/nvidia/platform/t23x/common/bct/scr 目录中。
SCR 配置文件的 NEW DTS 格式示例：
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
OLD CFG 格式
scr.161.7 = 0x3f008080; //
TKE_AON_SCR_WDTSCR0_0 scr.162.4
= 0x18000303; //
DMAAPB_1_SCR_BR_INTR_SCR_0
scr.163.4 = 0x18000303; //
DMAAPB_1_SCR_BR_SCR_0

---

*© 2022 NVIDIA Corporation。完整的保修、商标与法律文本见官方 DU-10990-001 PDF。*


<details>
<summary>English original</summary>

**14.2 MB2 Firmware Data**
Mb2 firmware configuration is specified as:
/ {
mb2-misc {
<firmware-type> {
<parameter> = <value>;
};
};
};
Where firmware type is one of the cpubl, ape-fw, bpmp-fw, rce-fw, sce-fw, camera-taskfw, apr-fw.
<parameter> is one of the parameters from below table.
Parameters Description
enable Enable loading of firmware from MB2, if this property is present Otherwise disable
loading of firmware from MB2.
ast-va Virtual address for firmware carveout in BPMP-R5 address-space.
load-offset Offset in firmware carveout where firmware binary is loaded.
entry-offset Offset of firmware entry point in firmware carveout.
Example of an Mb2 misc DTS configuration file:
/dts-v1/;
/ {
mb2-misc {
disable-cpul2ecc; enable-combined-uart;
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
<a id="chapter-15-security-configuration"></a>
**Chapter 15. Security Configuration**
MB1 and MB2 program most of the SCRs and firewalls in T23x. The list of SCRs/firewalls,
their order, and their addresses is predetermined. The values are taken from the SCR
configuration file.
Each entry in this configuration file is in the following form:
/ {
scr {
reg@<index> {
<parameter> = <value>;
};
};
};
where:
- <index> is the index of the SCR/firewall in the predefined list
- <parameter> and its <value> can be as follows:
Parameter Value
exclusion-info Exclusion info is a bit field defined as follows
Bit Description
0 Do not program in coldboot or SC7-exit
1 Program in coldboot, but skip in SC7-exit
2 Program at end of MB2, instead of MB1
value 32 bit value for the SCR register
Note: The values of the SCRs are programmed in increasing order of indexes and not in the
order they appear in the configuration file. The scr/firewalls which are not specified in the
configuration file are locked without restricting the access to the protected registers.
The scr configuration files are kept in the
hardware/nvidia/platform/t23x/common/bct/scr directory.
NEW DTS format example of SCR config file:
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
OLD CFG format
scr.161.7 = 0x3f008080; //
TKE_AON_SCR_WDTSCR0_0 scr.162.4
= 0x18000303; //
DMAAPB_1_SCR_BR_INTR_SCR_0
scr.163.4 = 0x18000303; //
DMAAPB_1_SCR_BR_SCR_0

---

*© 2022 NVIDIA Corporation. Full warranty, trademark, and legal text is in the official DU-10990-001 PDF.*

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/3. L4T Customization/Deployment.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/3.%20L4T%20Customization/Deployment.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
