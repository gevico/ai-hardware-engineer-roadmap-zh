---
title: 自定义载板设计与 bring-up（上电点亮/调通）
description: 自定义载板设计与 bring-up（上电点亮/调通）
---

# 自定义载板设计与 bring-up（上电点亮/调通）

<div class="course-identity carrier-board" markdown="1">
<div class="course-identity__icon">BRD</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 B2 · 载板 bring-up</p>
<p class="course-identity__title">围绕 Jetson 模块设计并验证载板：电源、信号、连接器与 bring-up 风险。</p>
<p class="course-identity__meta">产物：载板 bring-up 检查清单 · 度量：电源轨、接口、信号完整性、故障</p>
</div>
</div>


**阶段 4 — 方向 B — Nvidia Jetson** · 模块 2 / 7

> **重点：** 从 NVIDIA **P3768** 参考设计出发，为 **Jetson Orin Nano 8GB** SoM 设计**自定义载板**，涵盖原理图绘制、PCB 布局布线、热管理与电源树设计，以及首件载板 bring-up 验证。
>
> **主要硬件：** 自定义载板上的 Jetson Orin Nano 8GB（P3767 模块）

**上一节：** [1. Nvidia Jetson Platform](../01-Nvidia-Jetson平台/Guide.md) · **下一节：** [3. L4T Customization](../03-L4T定制/Guide.md) · **配套：** [3. L4T Customization](../03-L4T定制/Guide.md)（自定义载板的 BSP 侧） · **Runtime 配套：** [5.1 Peripheral Access](../05-应用开发/01-外设访问/Guide.md)

**范围边界：** 本模块负责自定义载板的**连接器选型**、**原理图**、**布线**、**电源**与**引脚复用规划**。若要在运行中的 Jetson 上通过 runtime Linux 访问 GPIO、SPI、I2C、UART、CAN、USB 或存储，请使用 [5.1 Peripheral Access](../05-应用开发/01-外设访问/Guide.md)。

---


## 1. 为什么需要自定义载板

Orin Nano 8GB Developer Kit（P3768 载板）是为评估而设计，而非为量产出货产品。自定义载板可以：

- **移除**未使用的接口（DisplayPort、多余的 USB、SD 插槽），以降低成本与板面积
- **增加**产品专属 I/O（CAN 总线、工业以太网、PoE PD、自定义音频 codec、额外的 CSI 摄像头）
- **优化**外形尺寸、安装、散热路径与连接器布局，以适配你的外壳
- **掌控**BOM、第二供应源与长期供应

**开发套件足够时：** 原型验证、小规模内部部署（<50 台）、与开发套件连接器集合完全匹配的应用。

**必须自定义时：** 量产产品、恶劣环境部署、法规认证（自定义屏蔽/滤波）、成本优化，或任何开发套件外形尺寸不适配的设计。

---

## 2. NVIDIA 参考设计（P3768）研读

NVIDIA 公开**Orin Nano Developer Kit 载板**的设计文件以供参考。在用 EDA 工具画下第一根线之前，先从这里开始。

### 关键文档

| 文档 | 可提取内容 |
|----------|-----------------|
| **Jetson Orin Nano OEM Product Design Guide** | 模块引脚定义、电源时序要求、禁布区、热界面规格 |
| **P3768 载板原理图**（Altium/OrCAD） | 各接口的参考电路：USB、PCIe、CSI、HDMI、以太网、电源 |
| **P3768 载板布局布线**（Altium/OrCAD） | 叠层、阻抗目标、高速 trace 布线、过孔策略 |
| **Jetson Orin Nano Design Guide** | 电气与机械规格、绝对最大额定值 |
| **引脚复用（pinmux）电子表格**（`Jetson_Orin_NX_and_Orin_Nano_series_Pinmux_Config_Template`） | 引脚功能分配、驱动强度、上拉/下拉配置 |

### 模块与载板间的连接器

Orin Nano SoM 通过 **260-pin Molex 板对板连接器**（0.5 mm 间距）连接。仔细研读引脚定义：

- 电源引脚（VIN、GND、模块电源轨）
- 高速差分对（USB 3.2、PCIe Gen3/4、CSI-2、HDMI/DP）
- 低速 I/O（UART、SPI、I2C、GPIO、CAN、I2S）
- 配置与控制引脚（FORCE_RECOVERY、POWER_BTN、SYS_RESET）
- JTAG 调试引脚

### 参考原理图走读

逐一走读 P3768 原理图中的每个功能模块：

1. **电源输入与稳压** — 桶形插座、USB-C PD、主降压稳压器
2. **USB hub 与端口** — USB 3.2 Gen 1 hub、Type-A 连接器
3. **PCIe M.2 插槽** — 用于 NVMe SSD 的 M.2 Key M
4. **CSI 摄像头连接器** — 2x 22-pin MIPI CSI-2 FPC
5. **显示输出** — HDMI 和/或 DisplayPort
6. **以太网** — 千兆以太网 PHY 与带网络变压器的 RJ45
7. **GPIO 排针** — 40-pin 兼容 Raspberry Pi 的排针
8. **调试** — 用于 UART 控制台的 micro-USB、恢复模式按钮
9. **风扇排针** — 带转速计的 4-pin PWM 风扇连接器

---

## 3. 原理图绘制 — 从参考到自定义

### 策略

1. 将 P3768 参考原理图**导入** EDA 工具（KiCad、Altium、OrCAD）
2. **删除**不需要的模块（例如无头产品中的 DisplayPort）
3. **修改**需要改动的模块（例如更换 USB hub 型号、增加 CAN 收发器）
4. 为产品专属接口**增加**新模块
5. 对照引脚复用电子表格与 OEM Design Guide **验证**每个改动过的引脚


<details>
<summary>English original</summary>

**Custom Carrier Board Design and Bring-Up**

<div class="course-identity carrier-board" markdown="1">
<div class="course-identity__icon">BRD</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track B2 · Carrier Board Bring-Up</p>
<p class="course-identity__title">Design and validate the board around a Jetson module: power, signals, connectors, and bring-up risk.</p>
<p class="course-identity__meta">Artifact: carrier bring-up checklist · Measure: rails, interfaces, signal integrity, faults</p>
</div>
</div>


**Phase 4 — Track B — Nvidia Jetson** · Module 2 of 7

> **Focus:** Design a **custom carrier board** for the **Jetson Orin Nano 8GB** SoM starting from the NVIDIA **P3768** reference design, through schematic capture, PCB layout, thermal and power-tree design, and first-article board bring-up validation.
>
> **Primary hardware:** Jetson Orin Nano 8GB (P3767 module) on custom carrier

**Previous:** [1. Nvidia Jetson Platform](../01-Nvidia-Jetson平台/Guide.md) · **Next:** [3. L4T Customization](../03-L4T定制/Guide.md) · **Companion:** [3. L4T Customization](../03-L4T定制/Guide.md) (BSP side of custom carriers) · **Runtime companion:** [5.1 Peripheral Access](../05-应用开发/01-外设访问/Guide.md)

**Scope boundary:** this module owns **connector choice**, **schematics**, **routing**, **power**, and **pinmux planning** for a custom carrier. For runtime Linux access to GPIO, SPI, I2C, UART, CAN, USB, or storage on a running Jetson, use [5.1 Peripheral Access](../05-应用开发/01-外设访问/Guide.md).

---


**1. Why a custom carrier board**

The Orin Nano 8GB Developer Kit (P3768 carrier) is designed for evaluation, not for shipping products. A custom carrier lets you:

- **Remove** unused interfaces (DisplayPort, extra USB, SD slot) to reduce cost and board area
- **Add** product-specific I/O (CAN bus, industrial Ethernet, PoE PD, custom audio codec, additional CSI cameras)
- **Optimize** the form factor, mounting, thermal path, and connector placement for your enclosure
- **Control** the BOM, second-sourcing, and long-term supply

**When the dev kit is sufficient:** Prototyping, small internal deployments (<50 units), applications that match the dev kit's connector set exactly.

**When custom is required:** Volume products, harsh-environment deployments, regulatory certification (custom shielding/filtering), cost optimization, or any design where the dev kit form factor doesn't fit.

---

**2. NVIDIA reference design (P3768) study**

NVIDIA publishes the **Orin Nano Developer Kit carrier** design files for reference. Start here before drawing a single line in your EDA tool.

**Key documents**

| Document | What to extract |
|----------|-----------------|
| **Jetson Orin Nano OEM Product Design Guide** | Module pinout, power sequencing requirements, keep-out zones, thermal interface spec |
| **P3768 carrier schematic** (Altium/OrCAD) | Reference circuits for every interface: USB, PCIe, CSI, HDMI, Ethernet, power |
| **P3768 carrier layout** (Altium/OrCAD) | Stackup, impedance targets, high-speed trace routing, via strategy |
| **Jetson Orin Nano Design Guide** | Electrical and mechanical specifications, absolute maximum ratings |
| **Pinmux spreadsheet** (`Jetson_Orin_NX_and_Orin_Nano_series_Pinmux_Config_Template`) | Pin function assignment, drive strength, pull configuration |

**Module-to-carrier connector**

The Orin Nano SoM connects via a **260-pin Molex board-to-board connector** (0.5 mm pitch). Study the pinout carefully:

- Power pins (VIN, GND, module power rails)
- High-speed differential pairs (USB 3.2, PCIe Gen3/4, CSI-2, HDMI/DP)
- Low-speed I/O (UART, SPI, I2C, GPIO, CAN, I2S)
- Configuration and control pins (FORCE_RECOVERY, POWER_BTN, SYS_RESET)
- JTAG debug pins

**Reference schematic walkthrough**

Walk through each functional block in the P3768 schematic:

1. **Power input and regulation** — barrel jack, USB-C PD, main buck regulator
2. **USB hub and ports** — USB 3.2 Gen 1 hub, Type-A connectors
3. **PCIe M.2 slot** — M.2 Key M for NVMe SSD
4. **CSI camera connectors** — 2x 22-pin MIPI CSI-2 FPC
5. **Display output** — HDMI and/or DisplayPort
6. **Ethernet** — Gigabit Ethernet PHY and RJ45 with magnetics
7. **GPIO header** — 40-pin Raspberry Pi-compatible header
8. **Debug** — micro-USB for UART console, recovery mode button
9. **Fan header** — 4-pin PWM fan connector with tachometer

---

**3. Schematic capture — from reference to custom**

**Strategy**

1. **Import** the P3768 reference schematic into your EDA tool (KiCad, Altium, OrCAD)
2. **Delete** blocks you don't need (e.g., DisplayPort for a headless product)
3. **Modify** blocks that need changes (e.g., swap USB hub for a different part, add CAN transceiver)
4. **Add** new blocks for product-specific interfaces
5. **Verify** every modified pin against the Pinmux spreadsheet and OEM Design Guide

</details>

### 常见改动

| 产品类型 | 移除 | 增加 |
|-------------|--------|-----|
| **无头边缘 AI 盒子** | Display、SD 卡、GPIO 排针 | PoE PD、工业以太网、额外的 NVMe |
| **机器人平台** | Display、部分 USB 端口 | CAN 总线（×2）、IMU（SPI）、额外的 CSI 摄像头 |
| **智能相机** | 大部分 I/O、NVMe 插槽 | CSI FPC（×4）、ISP（图像信号处理器）触发 GPIO、PoE PD、音频编解码器 |
| **车载网关** | Display、USB 集线器 | CAN-FD（×3）、车载以太网、LTE/5G 模组（USB 或 PCIe） |

### EDA 工具流程

```
Reference schematic (Altium project)
  │
  ├─ Export to KiCad (if preferred)
  │     └─ Use Altium-to-KiCad converter or redraw key blocks
  │
  ├─ Annotate and organize into hierarchical sheets:
  │     ├─ Power
  │     ├─ SoM connector + decoupling
  │     ├─ USB
  │     ├─ PCIe / NVMe
  │     ├─ CSI / camera
  │     ├─ Ethernet
  │     ├─ CAN / serial
  │     ├─ Audio (if applicable)
  │     └─ Debug / test
  │
  └─ Run ERC (Electrical Rules Check)
```

---

## 4. 连接器与接口选型

根据产品的机械约束、环境要求和目标成本选择连接器。

### 接口决策

| 接口 | 开发套件 | 典型定制方案 |
|-----------|---------|----------------------|
| **USB 3.2** | Type-A × 4（经集线器） | Type-C × 1（无集线器，成本更低），或内部排针 |
| **USB 2.0** | 经集线器 | 直连模块引脚，micro-B 用于调试 |
| **CSI-2** | 22-pin FPC × 2 | 15-pin RPi FPC、Fakra（车载）、板对板（模块相机） |
| **PCIe** | M.2 Key M | M.2 Key E（WiFi）、直连板边连接器、mini-PCIe、定制底板 |
| **Ethernet** | 带磁性元件的 RJ45 | RJ45（标准）、M12（工业）、SFP（光纤），或仅 PHY（磁性元件放在载板上） |
| **Display** | HDMI + DP | micro HDMI（省空间）、eDP（嵌入式面板）、LVDS（经桥接），或无 |
| **CAN** | 开发套件上没有 | MCP2515（SPI）或 MCP2562（原生 CAN 经 pinmux 后使用的收发器） |
| **调试 UART** | Micro-USB FTDI | 排针（0.1"）、TagConnect（弹簧针，不占板面积），或测试点 |
| **电源** | 桶形插座 + USB-C | 桶形插座、接线端子（工业）、PoE PD（802.3at）、带保护的车载 12V/24V |

### 环境与机械考虑

- **振动：** 使用锁紧式连接器（M12、Molex Micro-Fit），而非标准 RJ45/USB-A
- **IP 等级：** 面板安装式连接器配 O 型圈密封
- **温度：** 额定 -40 到 +85 C 的工业级连接器
- **插拔次数：** 选择额定次数满足预期使用寿命的连接器

---

## 5. 电源树设计

### 模块供电要求

Orin Nano 8GB 模块需要：

| 电源轨 | 电压 | 最大电流 | 备注 |
|------|---------|-------------|-------|
| **VIN_PWR_BAT** | 标称 5V（4.75–5.25V） | 3A（15W 模式） | 模块的主电源输入 |
| | | 1.4A（7W 模式） | |

### 电源时序

OEM 设计指南规定了**严格的上电时序**。违反它可能损坏模块或导致无法启动：

1. 在 **POWER_BTN** 被拉高之前，**VIN_PWR_BAT** 必须已稳定
2. 初始上电爬升期间，**SYS_RESET_N** 必须保持低电平
3. 模块输出的 **CARRIER_PWR_ON** 表示载板可以开启自己的电源轨

### 载板电源树设计

```
DC Input (12V/24V/PoE)
  │
  ├─ Input protection (TVS, reverse polarity, fuse)
  │
  ├─ Main buck regulator → 5V rail (for module VIN_PWR_BAT)
  │     └─ Current monitoring (INA3221 or INA226)
  │
  ├─ 3.3V rail (for carrier peripherals: Ethernet PHY, CAN, GPIO)
  │     └─ LDO or secondary buck from 5V
  │
  ├─ 1.8V rail (if needed for level shifters or specific I/O)
  │
  └─ Fan power (5V or 12V, switched by CARRIER_PWR_ON)
```

### 关键设计考量

- **浪涌电流：** 主稳压器使用软启动；模块的旁路电容会吸取较大浪涌电流
- **负载开关时序：** 载板外设使用使能引脚接到 CARRIER_PWR_ON 的负载开关
- **电流监测：** 在 VIN_PWR_BAT 上放置电流检测电阻以进行功耗剖析（bring-up 上电点亮/调通期间必不可少）
- **功耗预算表：** 在最坏情况下（GPU 时钟最高 + 所有外设开启）累加所有电源轨电流，并按 20% 余量选取稳压器

---

## 6. PCB 布局布线与叠层

### 叠层

对于带高速接口的载板，推荐至少采用 **6 层**叠层：

```
Layer 1  — Signal (top, components, high-speed traces)
Layer 2  — GND plane (continuous, no splits under high-speed traces)
Layer 3  — Signal (inner, low-speed routing)
Layer 4  — Power plane(s)
Layer 5  — GND plane
Layer 6  — Signal (bottom, components, routing)
```

**8 层**叠层可为 USB 3.2 Gen 2、PCIe Gen 4 及高密度设计提供更好的信号完整性。


<details>
<summary>English original</summary>

**Common modifications**

| Product type | Remove | Add |
|-------------|--------|-----|
| **Headless edge AI box** | Display, SD card, GPIO header | PoE PD, industrial Ethernet, additional NVMe |
| **Robotics platform** | Display, some USB ports | CAN bus (×2), IMU (SPI), additional CSI cameras |
| **Smart camera** | Most I/O, NVMe slot | CSI FPC (×4), ISP trigger GPIO, PoE PD, audio codec |
| **Vehicle gateway** | Display, USB hub | CAN-FD (×3), automotive Ethernet, LTE/5G modem (USB or PCIe) |

**EDA tool workflow**

```
Reference schematic (Altium project)
  │
  ├─ Export to KiCad (if preferred)
  │     └─ Use Altium-to-KiCad converter or redraw key blocks
  │
  ├─ Annotate and organize into hierarchical sheets:
  │     ├─ Power
  │     ├─ SoM connector + decoupling
  │     ├─ USB
  │     ├─ PCIe / NVMe
  │     ├─ CSI / camera
  │     ├─ Ethernet
  │     ├─ CAN / serial
  │     ├─ Audio (if applicable)
  │     └─ Debug / test
  │
  └─ Run ERC (Electrical Rules Check)
```

---

**4. Connector and interface selection**

Choose connectors based on your product's mechanical constraints, environmental requirements, and target cost.

**Interface decisions**

| Interface | Dev kit | Typical custom options |
|-----------|---------|----------------------|
| **USB 3.2** | Type-A × 4 (via hub) | Type-C × 1 (no hub, lower cost), or internal header |
| **USB 2.0** | Via hub | Direct to module pins, micro-B for debug |
| **CSI-2** | 22-pin FPC × 2 | 15-pin RPi FPC, Fakra (automotive), board-to-board (module camera) |
| **PCIe** | M.2 Key M | M.2 Key E (WiFi), direct edge connector, mini-PCIe, custom baseboard |
| **Ethernet** | RJ45 with magnetics | RJ45 (standard), M12 (industrial), SFP (fiber), or PHY-only (magnetics on carrier) |
| **Display** | HDMI + DP | HDMI micro (space), eDP (embedded panel), LVDS (via bridge), or none |
| **CAN** | Not on dev kit | MCP2515 (SPI) or MCP2562 (transceiver for native CAN if pinmuxed) |
| **Debug UART** | Micro-USB FTDI | Pin header (0.1"), TagConnect (pogo, no-footprint), or test pad |
| **Power** | Barrel jack + USB-C | Barrel jack, terminal block (industrial), PoE PD (802.3at), automotive 12V/24V with protection |

**Environmental and mechanical considerations**

- **Vibration:** Locking connectors (M12, Molex Micro-Fit) instead of standard RJ45/USB-A
- **IP rating:** Panel-mount connectors with O-ring seals
- **Temperature:** Industrial-grade connectors rated to -40 to +85 C
- **Mating cycles:** Choose connectors rated for your expected service life

---

**5. Power tree design**

**Module power requirements**

The Orin Nano 8GB module requires:

| Rail | Voltage | Max current | Notes |
|------|---------|-------------|-------|
| **VIN_PWR_BAT** | 5V nominal (4.75–5.25V) | 3A (15W mode) | Main power input to module |
| | | 1.4A (7W mode) | |

**Power sequencing**

The OEM Design Guide specifies a **strict power-on sequence**. Violating it can damage the module or prevent boot:

1. **VIN_PWR_BAT** must be stable before **POWER_BTN** is asserted
2. **SYS_RESET_N** must be held low during initial power ramp
3. **CARRIER_PWR_ON** output from module indicates carrier can enable its own rails

**Carrier power tree design**

```
DC Input (12V/24V/PoE)
  │
  ├─ Input protection (TVS, reverse polarity, fuse)
  │
  ├─ Main buck regulator → 5V rail (for module VIN_PWR_BAT)
  │     └─ Current monitoring (INA3221 or INA226)
  │
  ├─ 3.3V rail (for carrier peripherals: Ethernet PHY, CAN, GPIO)
  │     └─ LDO or secondary buck from 5V
  │
  ├─ 1.8V rail (if needed for level shifters or specific I/O)
  │
  └─ Fan power (5V or 12V, switched by CARRIER_PWR_ON)
```

**Key design considerations**

- **Inrush current:** Use soft-start on the main regulator; the module's bypass capacitors draw significant inrush
- **Load-switch sequencing:** Use load switches with enable pins tied to CARRIER_PWR_ON for carrier peripherals
- **Current monitoring:** Place current sense resistors on VIN_PWR_BAT for power profiling (essential during bring-up)
- **Power budget spreadsheet:** Sum all rail currents at worst-case (max GPU clock + all peripherals active) and size regulators with 20% margin

---

**6. PCB layout and stackup**

**Stackup**

A **6-layer** stackup is the minimum recommended for a carrier with high-speed interfaces:

```
Layer 1  — Signal (top, components, high-speed traces)
Layer 2  — GND plane (continuous, no splits under high-speed traces)
Layer 3  — Signal (inner, low-speed routing)
Layer 4  — Power plane(s)
Layer 5  — GND plane
Layer 6  — Signal (bottom, components, routing)
```

An **8-layer** stackup provides better signal integrity for USB 3.2 Gen 2, PCIe Gen 4, and dense designs.

</details>

### 关键布局布线规则

| 规则 | 目标 |
|------|--------|
| **模块禁布区** | 遵守 NVIDIA 规定的、SoM 连接器下方及周边的禁布区 |
| **SoM 安装** | 遵循 Design Guide 中的螺柱位置与螺钉扭矩 |
| **去耦** | 在 VIN_PWR_BAT 引脚处放置 100 nF + 10 uF，尽可能靠近连接器 |
| **地平面** | SoM 连接器或高速差分对下方不得有分割或空洞 |
| **过孔缝合** | 沿 SoM 封装外围和板边缝合 GND 过孔 |
| **散热过孔** | 散热器安装区下方，布置通向内层 GND 平面的散热过孔阵列 |

### 阻抗控制走线

| 接口 | 阻抗 | 线对类型 |
|-----------|-----------|-----------|
| USB 3.2 Gen 1/2 | 90 ohm 差分 | 耦合差分对 |
| PCIe Gen 3/4 | 85 ohm 差分 | 耦合差分对 |
| CSI-2 | 100 ohm 差分 | 耦合差分对 |
| HDMI/DP | 100 ohm 差分 | 耦合差分对 |
| 以太网（RGMII） | 50 ohm 单端，100 ohm 差分 | 混合 |

使用 PCB 板厂的阻抗计算器，或 Saturn PCB Toolkit 之类的工具，根据叠层计算走线宽度与间距。

---

## 7. 高速信号完整性

### USB 3.2 Gen 2（10 Gbps）

- 在表层按紧耦合差分对走线
- **长度匹配：** 对内 ±5 mil，对间 ±100 mil（若为多 lane）
- **最大走线长度：** 从模块引脚到连接器保持在 150 mm（6 英寸）以内
- 差分路径中**避免使用过孔**；若无法避免，使用背钻或带填充的盘中孔
- **串联 AC 耦合电容：** 100 nF、0402，放置在靠近连接器一端

### PCIe Gen 3/4

- 差分对规则与 USB 相同，但 lane 内的长度匹配更严格
- **参考时钟布线：** 100 MHz REFCLK 必须与数据对做长度匹配
- 尽可能让 **TX 与 RX 位于同一层**；尽量减少换层次数

### CSI-2（MIPI）

- 走线长度要短（CSI 通常为板载或经短 FPC 连接）
- **时钟与数据 lane 偏斜：** 单个 CSI 端口内 ±0.5 mm
- 若走线长度超过 100 mm，按 MIPI D-PHY 规范做端接

### 常见信号完整性故障

| 现象 | 可能原因 | 解决措施 |
|---------|-------------|-----|
| USB 3.0 降速到 2.0 | 阻抗不匹配、过孔残桩 | 检查叠层阻抗，背钻或缩短残桩 |
| PCIe 链路只训练到 Gen 1 | Gen 3/4 速率下损耗过大 | 缩短走线，改善阻抗控制，检查连接器 |
| CSI 摄像头无图像 | 时钟/数据偏斜 | 重新布线并加强长度匹配 |
| 以太网 CRC 错误 | 缺少网络变压器端接、布局布线耦合 | 检查 PHY 参考设计，与噪声走线隔离 |

---

## 8. 定制外壳的热管理设计

### 热预算

| 功耗模式 | 模块 TDP | GPU 密集型工作负载典型值 |
|------------|-----------|---------------------------|
| 7W（MAXN 关闭） | 7W | 持续约 6W |
| 15W（MAXN） | 15W | 持续约 12–14W |

### 散热路径

```
Junction (die)
  │  Rjc ≈ 1.5 °C/W (module internal)
  │
Module top surface (thermal interface)
  │  Rtim ≈ 0.5–2 °C/W (thermal pad/paste)
  │
Heatsink
  │  Rhs ≈ 2–8 °C/W (depends on design)
  │
Ambient air
```

**目标：** 环境温度 40 C（工业级）或 25 C（消费级）下，结温 < 95 C。

### 散热器方案

| 方案 | Rhs（约） | 应用 |
|----------|-------------|-------------|
| **被动鳍片散热器**（40×40×20 mm） | 5–8 C/W | 消费级、静音、7W 模式 |
| **被动 + 外壳兼作散热器** | 3–5 C/W | 工业级密封盒体 |
| **主动风扇 + 小散热器** | 1.5–3 C/W | 15W 模式、连续 AI 工作负载 |
| **热管 + 鳍片组** | 1–2 C/W | 高性能、紧凑 |

### 风扇控制

模块提供一路 **PWM 风扇输出**引脚。连接到 4 针风扇接头：

| 引脚 | 功能 |
|-----|----------|
| 1 | GND |
| 2 | +5V 或 +12V |
| 3 | 转速计（sense） |
| 4 | PWM 控制 |

通过 `jetson_clocks` 或自定义的 `pwm-fan` 设备树条目配置风扇曲线。

### 热验证

- 在执行持续 AI 工作负载时运行 `tegrastats`
- 监控 **TJ（结温）**、**GPU %** 和**降频事件**
- 若发生降频，说明散热方案不足 —— 增大散热器尺寸或增加强制风冷

---

## 9. 可测试性设计与调试排针

良好的测试接入可在 bring-up（上电点亮/调通）阶段节省数周时间，并简化产线测试。

### 必备测试点

| 测试点 | 用途 |
|------------|---------|
| **VIN_PWR_BAT** | 验证模块输入电压与纹波 |
| **5V、3.3V、1.8V 电源轨** | 验证载板所有电源轨 |
| **CARRIER_PWR_ON** | 确认模块已发出载板上电信号 |
| **SYS_RESET_N** | 监控或手动触发复位 |
| **POWER_BTN** | 用于调试的手动电源按键 |


<details>
<summary>English original</summary>

**Critical layout rules**

| Rule | Target |
|------|--------|
| **Module keepout** | Respect NVIDIA-specified keepout zones under and around the SoM connector |
| **SoM mounting** | Follow standoff placement and screw torque from the Design Guide |
| **Decoupling** | Place 100 nF + 10 uF at VIN_PWR_BAT pins, as close as possible to connector |
| **Ground plane** | No splits or voids under the SoM connector or high-speed differential pairs |
| **Via stitching** | Stitch GND vias around the SoM footprint and along board edges |
| **Thermal vias** | Under the heatsink mounting area, array of thermal vias to inner GND planes |

**Impedance-controlled traces**

| Interface | Impedance | Pair type |
|-----------|-----------|-----------|
| USB 3.2 Gen 1/2 | 90 ohm differential | Coupled differential pair |
| PCIe Gen 3/4 | 85 ohm differential | Coupled differential pair |
| CSI-2 | 100 ohm differential | Coupled differential pair |
| HDMI/DP | 100 ohm differential | Coupled differential pair |
| Ethernet (RGMII) | 50 ohm single-ended, 100 ohm differential | Mixed |

Use your PCB fab's impedance calculator or a tool like Saturn PCB Toolkit to compute trace widths and spacing for your stackup.

---

**7. High-speed signal integrity**

**USB 3.2 Gen 2 (10 Gbps)**

- Route as tightly coupled differential pairs on the surface layer
- **Length match:** ±5 mil within a pair, ±100 mil between pairs (if multi-lane)
- **Max trace length:** keep under 150 mm (6 inches) from module pin to connector
- **Avoid vias** in the differential path; if unavoidable, use back-drilled or via-in-pad with fill
- **Series AC coupling caps:** 100 nF, 0402, placed close to the connector end

**PCIe Gen 3/4**

- Same differential pair rules as USB but tighter length matching within a lane
- **Reference clock routing:** 100 MHz REFCLK must be length-matched to data pairs
- **TX and RX on same layer** if possible; minimize layer transitions

**CSI-2 (MIPI)**

- Short trace lengths (CSI is typically on-board or via short FPC)
- **Clock and data lane skew:** ±0.5 mm within a CSI port
- Terminate per MIPI D-PHY specification if trace length exceeds 100 mm

**Common signal integrity failures**

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| USB 3.0 drops to 2.0 | Impedance mismatch, via stubs | Check stackup impedance, back-drill or shorten stubs |
| PCIe link trains at Gen 1 | Excessive loss at Gen 3/4 speeds | Shorten traces, improve impedance control, check connector |
| CSI camera no image | Clock/data skew | Re-route with tighter length matching |
| Ethernet CRC errors | Missing magnetics termination, layout coupling | Check PHY reference design, isolate from noisy traces |

---

**8. Thermal design for custom enclosures**

**Thermal budget**

| Power mode | Module TDP | Typical GPU-heavy workload |
|------------|-----------|---------------------------|
| 7W (MAXN disabled) | 7W | ~6W sustained |
| 15W (MAXN) | 15W | ~12–14W sustained |

**Thermal path**

```
Junction (die)
  │  Rjc ≈ 1.5 °C/W (module internal)
  │
Module top surface (thermal interface)
  │  Rtim ≈ 0.5–2 °C/W (thermal pad/paste)
  │
Heatsink
  │  Rhs ≈ 2–8 °C/W (depends on design)
  │
Ambient air
```

**Target:** Junction temperature < 95 C with ambient at 40 C (industrial) or 25 C (consumer).

**Heatsink options**

| Solution | Rhs (approx) | Application |
|----------|-------------|-------------|
| **Passive finned heatsink** (40×40×20 mm) | 5–8 C/W | Consumer, quiet, 7W mode |
| **Passive + enclosure as heatsink** | 3–5 C/W | Industrial sealed box |
| **Active fan + small heatsink** | 1.5–3 C/W | 15W mode, continuous AI workload |
| **Heat pipe + fin stack** | 1–2 C/W | High-performance, compact |

**Fan control**

The module provides a **PWM fan output** pin. Connect to a 4-pin fan header:

| Pin | Function |
|-----|----------|
| 1 | GND |
| 2 | +5V or +12V |
| 3 | Tachometer (sense) |
| 4 | PWM control |

Configure fan curves via `jetson_clocks` or custom `pwm-fan` device tree entries.

**Thermal validation**

- Run `tegrastats` while executing a sustained AI workload
- Monitor **TJ (junction temperature)**, **GPU %**, and **throttling events**
- If throttling occurs, the thermal solution is insufficient — increase heatsink size or add forced airflow

---

**9. Design-for-test and debug headers**

Good test access saves weeks during bring-up and simplifies factory testing.

**Essential test points**

| Test point | Purpose |
|------------|---------|
| **VIN_PWR_BAT** | Verify module input voltage and ripple |
| **5V, 3.3V, 1.8V rails** | Verify all carrier power rails |
| **CARRIER_PWR_ON** | Confirm module has signaled carrier power-on |
| **SYS_RESET_N** | Monitor or manually assert reset |
| **POWER_BTN** | Manual power button for debug |

</details>

### 调试排针

| 排针 | 用途 |
|--------|---------|
| **UART 调试**（3-pin：TX、RX、GND） | bootloader 与 kernel 的控制台输出；使用 TagConnect 或 0.1" 排针 |
| **JTAG**（10-pin ARM Cortex 或 20-pin） | UART 尚不可用时，启动失败的底层调试 |
| **I2C 扫描排针** | 引出 I2C 总线，供 bring-up（上电点亮/调通）期间探测 |
| **SPI 测试排针** | 引出 SPI 总线，用于回环测试 |

### LED 指示灯

| LED | 连接到 | 用途 |
|-----|-------------|---------|
| Power LED | 主稳压器之后 | 板卡已接入输入电源 |
| 模块电源 LED | CARRIER_PWR_ON | 模块已上电并运行 |
| 启动进度 LED | GPIO（由 bootloader 或 kernel 翻转） | 可视化的启动进度指示 |
| 网络 LED | Ethernet PHY | 链路与活动状态 |

---

## 10. BOM 管理与元器件选型

### 关键元器件决策

| 元器件 | 关键指标 | 第二货源策略 |
|-----------|-------------|----------------------|
| **SoM 连接器**（260-pin） | 必须与 NVIDIA 规范完全一致 | 单一货源（Molex）——不可替代 |
| **主 buck 稳压器** | 5V、3A+、效率 >90%、工业级温度 | 从 TI/MPS/Analog 中选型——挑选有引脚兼容替代品的型号 |
| **Ethernet PHY** | 千兆、RGMII、必要时工业级温度 | Realtek RTL8211F 或 Microchip LAN8720（封装不同） |
| **USB hub**（若使用） | USB 3.2 Gen 1、工业级温度 | Microchip USB5744 或类似型号 |
| **CAN 收发器** | CAN 2.0B 或 CAN-FD、3.3V、ESD 保护 | TI TCAN1042、NXP TJA1042、Microchip MCP2562——大多引脚兼容 |
| **无源器件** | 使用标准值（0402/0603）、±1% 电阻 | BOM 中始终给出两家制造商可选 |

### BOM 最佳实践

- **避免单一货源元器件**，除非迫不得已（SoM 连接器、特定 IC）
- **每一行都标明制造商料号 + 分销商料号**
- 对量产时省去的调试元器件**保留 DNP（Do Not Place）选项**
- **跟踪交期**——SoM 本身经 NVIDIA 分销渠道的交期为 12–16 周
- **使用 AVL（Approved Vendor List，合格供应商清单）**，并按硬件版本锁定

---

## 11. 板卡 bring-up 流程

### 上电前检查清单

插入 SoM 模块之前：

1. **目视检查**——检查是否存在锡桥、缺件，极性元件方向是否正确
2. **通断检查**——确认 VIN 与 GND、3.3V 与 GND 等之间无短路
3. **电源轨测试**——加输入电源，用万用表测量载板所有电源轨（不插模块）：
   - 5V 轨在规格内（4.75–5.25V）
   - 3.3V 轨在规格内
   - 1.8V 轨（若有）
4. **纹波检查**——用示波器确认 5V 轨纹波 <50 mV

### 首次启动

1. **插入模块**——确认对齐，并按正确扭矩拧紧螺丝
2. **连接 UART 调试线**——以 115200 baud 打开终端
3. **上电**——观察 UART 输出
4. **预期时序：**
   ```
   [MB1] DRAM training...
   [MB2] Loading UEFI...
   [UEFI] Booting Linux...
   [kernel] ...
   ```
5. 若无 UART 输出——检查 POWER_BTN、SYS_RESET、带载下的 VIN 电压

### 首次烧录

```bash
# On host (Ubuntu 22.04)
cd Linux_for_Tegra
sudo ./flash.sh <board_config> mmcblk0p1
# or for NVMe:
sudo ./tools/kernel_flash/l4t_initrd_flash.sh <board_config> external
```

使用 [Module 3 — L4T Customization](../03-L4T定制/Guide.md) 中的板级配置，并针对你的定制载板做适配。

---

## 12. 引脚复用配置与验证

### 使用 NVIDIA Pinmux 表格

1. 打开 `Jetson_Orin_NX_and_Orin_Nano_series_Pinmux_Config_Template_v1.2.xlsm`
2. 对载板使用的每个引脚，选择正确的功能（GPIO、SPI、I2C、UART 等）
3. 设置驱动强度、上拉/下拉、输入/输出方向
4. **导出**生成的 `.dtsi` 片段

### 生成 pinmux 设备树片段

该表格会生成设备树源包含（`.dtsi`）文件：

- `tegra234-mb1-bct-pinmux-<board>.dtsi`——MB1 boot 阶段的 pinmux
- `tegra234-mb1-bct-gpio-<board>.dtsi`——MB1 的 GPIO 配置
- `tegra234-mb1-bct-padvoltage-<board>.dtsi`——pad 电压电平

将这些文件放到 `Linux_for_Tegra/bootloader/generic/BCT/`，并在你的板级 `.conf` 文件中引用它们。详见 [T23x BCT reference](../03-L4T定制/T23x-Deployment.md)。

### 验证

```bash
# On target — verify the board came up with the expected pin and bus behavior
sudo gpioinfo gpiochip0
sudo gpioget gpiochip0 43

# Verify I2C bus detection
sudo i2cdetect -y -r 1

# Verify SPI bus
sudo spidev_test -D /dev/spidev0.0 -v

# Verify UART
sudo cat /dev/ttyTHS0  # (check for expected data)
```

使用目标板真实的 line offset 和总线编号。关于 `gpiochip` 在 runtime 下的含义、line offset，以及 `libgpiod` 的安全用法，参见 [5.1 外设访问](../05-应用开发/01-外设访问/Guide.md)。


<details>
<summary>English original</summary>

**Debug headers**

| Header | Purpose |
|--------|---------|
| **UART debug** (3-pin: TX, RX, GND) | Console output from bootloader and kernel; use a TagConnect or 0.1" header |
| **JTAG** (10-pin ARM Cortex or 20-pin) | Low-level debug if boot fails before UART is available |
| **I2C scan header** | Expose I2C buses for probing during bring-up |
| **SPI test header** | Expose SPI buses for loopback testing |

**LED indicators**

| LED | Connected to | Purpose |
|-----|-------------|---------|
| Power LED | After main regulator | Board has input power |
| Module power LED | CARRIER_PWR_ON | Module is powered and running |
| Boot progress LED | GPIO (toggled by bootloader or kernel) | Visual boot progress indicator |
| Network LED | Ethernet PHY | Link and activity |

---

**10. BOM management and component selection**

**Critical component decisions**

| Component | Key criteria | Second-source strategy |
|-----------|-------------|----------------------|
| **SoM connector** (260-pin) | Must match NVIDIA spec exactly | Single source (Molex) — no substitution |
| **Main buck regulator** | 5V, 3A+, efficiency >90%, industrial temp | Select from TI/MPS/Analog — pick parts with pin-compatible alternates |
| **Ethernet PHY** | Gigabit, RGMII, industrial temp if needed | Realtek RTL8211F or Microchip LAN8720 (different footprint) |
| **USB hub** (if used) | USB 3.2 Gen 1, industrial temp | Microchip USB5744 or similar |
| **CAN transceiver** | CAN 2.0B or CAN-FD, 3.3V, ESD protected | TI TCAN1042, NXP TJA1042, Microchip MCP2562 — mostly pin-compatible |
| **Passive components** | Use standard values (0402/0603), ±1% resistors | Always specify two manufacturer options in BOM |

**BOM best practices**

- **Avoid single-source components** except where forced (SoM connector, specific ICs)
- **Specify manufacturer part number + distributor part number** for every line
- **Include DNP (Do Not Place) options** for debug components stripped in production
- **Track lead times** — the SoM itself has 12–16 week lead times through NVIDIA distribution
- **Use an AVL (Approved Vendor List)** and lock it per hardware revision

---

**11. Board bring-up procedure**

**Pre-power checklist**

Before inserting the SoM module:

1. **Visual inspection** — check for solder bridges, missing components, correct orientation of polarized parts
2. **Continuity check** — verify no shorts between VIN and GND, 3.3V and GND, etc.
3. **Power rail test** — apply input power, measure all carrier rails with a multimeter (module not inserted):
   - 5V rail within spec (4.75–5.25V)
   - 3.3V rail within spec
   - 1.8V rail (if present)
4. **Ripple check** — use oscilloscope to verify ripple on 5V rail is <50 mV

**First boot**

1. **Insert module** — verify alignment and secure with screws at correct torque
2. **Connect UART debug cable** — open terminal at 115200 baud
3. **Apply power** — observe UART output
4. **Expected sequence:**
   ```
   [MB1] DRAM training...
   [MB2] Loading UEFI...
   [UEFI] Booting Linux...
   [kernel] ...
   ```
5. If no UART output — check POWER_BTN, SYS_RESET, VIN voltage under load

**First flash**

```bash
# On host (Ubuntu 22.04)
cd Linux_for_Tegra
sudo ./flash.sh <board_config> mmcblk0p1
# or for NVMe:
sudo ./tools/kernel_flash/l4t_initrd_flash.sh <board_config> external
```

Use the board config from [Module 3 — L4T Customization](../03-L4T定制/Guide.md) adapted for your custom carrier.

---

**12. Pinmux configuration and validation**

**Using the NVIDIA Pinmux spreadsheet**

1. Open `Jetson_Orin_NX_and_Orin_Nano_series_Pinmux_Config_Template_v1.2.xlsm`
2. For each pin your carrier uses, select the correct function (GPIO, SPI, I2C, UART, etc.)
3. Set drive strength, pull-up/pull-down, and input/output direction
4. **Export** the generated `.dtsi` fragments

**Generating pinmux DT fragments**

The spreadsheet generates device tree source include (`.dtsi`) files:

- `tegra234-mb1-bct-pinmux-<board>.dtsi` — pinmux for MB1 boot stage
- `tegra234-mb1-bct-gpio-<board>.dtsi` — GPIO configuration for MB1
- `tegra234-mb1-bct-padvoltage-<board>.dtsi` — pad voltage levels

Place these in `Linux_for_Tegra/bootloader/generic/BCT/` and reference them in your board `.conf` file. See [T23x BCT reference](../03-L4T定制/T23x-Deployment.md) for details.

**Validation**

```bash
# On target — verify the board came up with the expected pin and bus behavior
sudo gpioinfo gpiochip0
sudo gpioget gpiochip0 43

# Verify I2C bus detection
sudo i2cdetect -y -r 1

# Verify SPI bus
sudo spidev_test -D /dev/spidev0.0 -v

# Verify UART
sudo cat /dev/ttyTHS0  # (check for expected data)
```

Use real line offsets and bus numbers from your target. For the runtime meaning of `gpiochip`, line offsets, and safe `libgpiod` usage, see [5.1 Peripheral Access](../05-应用开发/01-外设访问/Guide.md).

---

</details>

## 13. 外设验证检查清单

在自定义 pinmux 下首次启动成功后，逐个验证每个接口：

| 接口 | 测试 | 预期结果 | 命令 / 工具 |
|-----------|------|-----------------|----------------|
| **USB 3.2** | 插入 USB 设备 | 以 super-speed 枚举 | `lsusb -t` |
| **USB 2.0** | 插入 USB 设备 | 以 high-speed 枚举 | `lsusb -t` |
| **Ethernet** | 连接网线，运行 iperf | 链路 up，~940 Mbps | `ethtool eth0`, `iperf3 -c <host>` |
| **PCIe / NVMe** | 插入 NVMe SSD | 检测到设备 | `lspci`, `nvme list` |
| **CSI camera** | 连接摄像头模块 | 捕获到帧 | `v4l2-ctl --stream-mmap` |
| **I2C** | 扫描总线 | 预期设备地址有响应 | `i2cdetect -y -r <bus>` |
| **SPI** | 回环（MOSI→MISO） | 数据一致 | `spidev_test -D /dev/spidevX.Y` |
| **CAN** | 发送/接收帧 | 总线上有帧 | `cansend can0 123#DEADBEEF`, `candump can0` |
| **UART** | 回环或对接设备 | 字符回显 | `minicom` 或 `picocom` |
| **GPIO** | 翻转输出、读取输入 | 状态变化 | `gpioset`/`gpioget` |
| **Audio**（如有） | 播放/录音 | 声音输出 / 波形 | `aplay`, `arecord` |
| **Fan** | 设置 PWM 占空比 | 风扇转速变化 | 写入 PWM sysfs 或使用 `jetson_clocks` |
| **Display**（如有） | 连接显示器 | 桌面或 framebuffer 输出 | `xrandr` 或检查 `/dev/fb0` |

---

## 14. 常见的 bring-up（上电点亮/调通）失败与调试

| 现象 | 可能原因 | 调试方法 |
|---------|-------------|----------------|
| **完全无 UART 输出** | 上电时序错误、RESET 被拉低、UART TX/RX 接反 | 带载检查 VIN、核对 RESET 时序、对调 TX/RX |
| **DRAM training 失败** | 带载时 VIN 电压跌落 | 启动过程中用示波器测量 VIN、检查稳压器能力 |
| **Kernel 启动但 USB 不枚举** | 阻抗不匹配、连接器焊接不良 | 检查 USB 眼图、重新检查 SoM 连接器焊点 |
| **未检测到 PCIe 设备** | PERST# 未正确置位、时钟未布线 | 用示波器确认 PERST# 翻转、检查 REFCLK |
| **CSI camera：无帧** | DT 中 lane 映射错误、时钟/数据偏斜 | 核对 DT camera 节点与 pinmux、检查 CSI 走线长度 |
| **Ethernet 链路已建立但无流量** | 网络变压器缺失或选型错误、MDIO 配置错误 | 通过 MDIO 检查 PHY 寄存器访问、核对变压器接线 |
| **跑 AI 时模块热关断** | 散热方案不足 | 改进散热器、加风扇，或降到 7W 功耗模式 |
| **flash.sh 报 Board ID EEPROM 错误** | EEPROM 未贴装或 I2C 地址错误 | 使用 `flash.sh` 环境变量覆盖标志，或贴装载板 EEPROM |

---

## 15. 项目

- **最小无头载板：** 为无头 edge 节点设计一块载板，带 USB-C 供电、GbE、NVMe M.2、调试 UART 和风扇插座 —— 无显示、无 USB hub。目标 4 层 PCB。
- **机器人载板：** 设计一块载板，带双 CAN-FD、4x CSI-2、PoE PD、IMU（SPI）和 GPS（UART）。目标 6 层 PCB，选用工业温度级元件。
- **bring-up 验证套件：** 编写一个 shell 脚本，运行第 13 节完整的外设验证检查清单，并生成 pass/fail 报告。

---

## 16. 资源

| 资源 | 说明 |
|----------|-------------|
| **NVIDIA Jetson Download Center** | 参考设计文件、Design Guides、pinmux 表格 |
| **Jetson Orin Nano OEM Product Design Guide** | 模块引脚定义、上电时序、热管理规格、禁布区 |
| **P3768 carrier reference design** | Altium/OrCAD 原理图和布局布线文件 |
| **Jetson Orin Nano Design Guide** | 电气与机械规格 |
| **KiCad**（kicad.org） | 用于原理图和 PCB 布局布线的开源 EDA |
| **Saturn PCB Design Toolkit** | 阻抗计算器、过孔电流、走线宽度计算器 |
| **IPC-2221** | 印制板设计通用标准（走线宽度、间距、过孔） |
| [2. L4T Customization](../03-L4T定制/Guide.md) | 自定义载板的 BSP 集成（flash 配置、DT 移植） |
| [Jetson Module Adaptation and Bring-Up](../03-L4T定制/Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) | NVIDIA 关于从开发套件转向自定义载板的指南 |
| [Orin Nano Custom Board L4T Engineering Flow](../03-L4T定制/Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md) | 自定义载板 + L4T 的端到端工程流程图 |


<details>
<summary>English original</summary>

**13. Peripheral validation checklist**

After a successful first boot with your custom pinmux, systematically validate every interface:

| Interface | Test | Expected result | Command / tool |
|-----------|------|-----------------|----------------|
| **USB 3.2** | Plug USB device | Enumeration at super-speed | `lsusb -t` |
| **USB 2.0** | Plug USB device | Enumeration at high-speed | `lsusb -t` |
| **Ethernet** | Connect cable, run iperf | Link up, ~940 Mbps | `ethtool eth0`, `iperf3 -c <host>` |
| **PCIe / NVMe** | Insert NVMe SSD | Device detected | `lspci`, `nvme list` |
| **CSI camera** | Connect camera module | Frames captured | `v4l2-ctl --stream-mmap` |
| **I2C** | Scan bus | Expected device addresses respond | `i2cdetect -y -r <bus>` |
| **SPI** | Loopback (MOSI→MISO) | Data matches | `spidev_test -D /dev/spidevX.Y` |
| **CAN** | Send/receive frames | Frames on bus | `cansend can0 123#DEADBEEF`, `candump can0` |
| **UART** | Loopback or paired device | Characters echo | `minicom` or `picocom` |
| **GPIO** | Toggle output, read input | State changes | `gpioset`/`gpioget` |
| **Audio** (if present) | Play/record | Sound output / waveform | `aplay`, `arecord` |
| **Fan** | Set PWM duty | Fan speed changes | Write to PWM sysfs or use `jetson_clocks` |
| **Display** (if present) | Connect monitor | Desktop or framebuffer output | `xrandr` or check `/dev/fb0` |

---

**14. Common bring-up failures and debug**

| Symptom | Likely cause | Debug approach |
|---------|-------------|----------------|
| **No UART output at all** | Power sequencing wrong, RESET held low, UART TX/RX swapped | Check VIN under load, verify RESET timing, swap TX/RX |
| **DRAM training fail** | VIN voltage sag under load | Measure VIN with scope during boot, check regulator capacity |
| **Kernel boots but USB doesn't enumerate** | Impedance mismatch, bad solder on connector | Check USB eye diagram, re-inspect SoM connector solder joints |
| **PCIe device not detected** | PERST# not asserted correctly, clock not routed | Verify PERST# toggling with scope, check REFCLK |
| **CSI camera: no frames** | Lane mapping wrong in DT, clock/data skew | Verify DT camera node vs pinmux, check CSI trace lengths |
| **Ethernet link but no traffic** | Missing or wrong magnetics, MDIO misconfigured | Check PHY register access via MDIO, verify magnetics wiring |
| **Module thermal shutdown during AI** | Insufficient thermal solution | Improve heatsink, add fan, or reduce to 7W power mode |
| **Board ID EEPROM error in flash.sh** | EEPROM not populated or wrong I2C address | Use `flash.sh` env override flags or populate carrier EEPROM |

---

**15. Projects**

- **Minimal headless carrier:** Design a carrier for a headless edge node with USB-C power, GbE, NVMe M.2, debug UART, and fan header — no display, no USB hub. Target 4-layer PCB.
- **Robotics carrier:** Design a carrier with dual CAN-FD, 4x CSI-2, PoE PD, IMU (SPI), and GPS (UART). Target 6-layer PCB with industrial-temp components.
- **Bring-up validation suite:** Write a shell script that runs the full peripheral validation checklist from Section 13 and generates a pass/fail report.

---

**16. Resources**

| Resource | Description |
|----------|-------------|
| **NVIDIA Jetson Download Center** | Reference design files, Design Guides, pinmux spreadsheets |
| **Jetson Orin Nano OEM Product Design Guide** | Module pinout, power sequencing, thermal spec, keepout zones |
| **P3768 carrier reference design** | Altium/OrCAD schematic and layout files |
| **Jetson Orin Nano Design Guide** | Electrical and mechanical specifications |
| **KiCad** (kicad.org) | Open-source EDA for schematic and PCB layout |
| **Saturn PCB Design Toolkit** | Impedance calculator, via current, trace width calculator |
| **IPC-2221** | Generic standard on printed board design (trace width, spacing, via) |
| [2. L4T Customization](../03-L4T定制/Guide.md) | BSP integration for custom carriers (flash config, DT porting) |
| [Jetson Module Adaptation and Bring-Up](../03-L4T定制/Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) | NVIDIA's guide for moving from dev kit to custom carrier |
| [Orin Nano Custom Board L4T Engineering Flow](../03-L4T定制/Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md) | End-to-end engineering flowchart for custom carrier + L4T |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/2. Custom Carrier Board Design and Bring-Up/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/2.%20Custom%20Carrier%20Board%20Design%20and%20Bring-Up/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
