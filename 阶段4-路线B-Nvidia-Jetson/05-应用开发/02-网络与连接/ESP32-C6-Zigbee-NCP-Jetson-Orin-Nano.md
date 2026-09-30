---
title: 在 Jetson Orin Nano 上运行 ESP32-C6 Zigbee NCP - 项目指南
description: 在 Jetson Orin Nano 上运行 ESP32-C6 Zigbee NCP - 项目指南
---

# 在 Jetson Orin Nano 上运行 ESP32-C6 Zigbee NCP - 项目指南

> **目标：** 使用**第二块 ESP32-C6**作为 **Jetson Orin Nano 8GB Developer Kit** 的 **Zigbee Network Co-Processor (NCP)**，使 Jetson 可以充当更高层的 Zigbee 主机，同时让第一块 ESP32-C6 继续专用于 **ESP-Hosted Wi-Fi/BLE**。

**Hub：** [Network and Connectivity](Guide.md)  
**相关本地指南：** [ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md) · [ESP32-C6 OpenThread RCP on Jetson Orin Nano](ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md) · [Peripheral Access](../01-外设访问/Guide.md)

---

## 1. 为什么这个项目重要

这是在你当前的 Jetson + ESP32-C6 联网工作之后，干净利落的下一个实验：

- 第一块 ESP32-C6 已经通过 **ESP-Hosted** 为 Jetson 提供 `wlan0` 和 BLE
- 第二块 ESP32-C6 已经证明它可以充当 **802.15.4 协处理器**
- 你当前的 **Thread** 路径在射频或 UART 层面并未受阻，但在现有 Jetson kernel 上 **完整的 OTBR** 受阻，因为缺少 `CONFIG_IP_MROUTE` 和 `CONFIG_IPV6_MROUTE`

Zigbee 是不错的下一步，因为它仍然使用 **IEEE 802.15.4**，但**不**需要 OTBR 的 IPv6 边界路由路径。这意味着你在 OTBR 上遇到的那个具体的 Linux 组播路由阻塞点，在 Zigbee host/NCP 设计中不应成为主要问题。最后这一点是基于协议架构的工程推断，并不代表整个 Jetson Zigbee 栈已经开箱即用。

这个项目能教给你什么：

- Linux 主机如何与 Zigbee 协处理器通信，而不是与 Thread RCP 通信
- 尽管 Zigbee 与 Thread 都使用 802.15.4，二者有何不同
- 如何让 Wi-Fi/BLE 与 Zigbee 分处不同射频，以获得更干净的系统设计
- Jetson 集成在何处变成了主机应用问题，而不是 Linux 网络接口问题

---

## 2. 当前状态与范围

在你当前的 Jetson 镜像上，OpenThread RCP 路径已经验证到节点可以成为 `leader` 这一步。故障发生在之后，当 `otbr-agent` 尝试初始化边界路由支持时，kernel 报告：

```text
InitMulticastRouterSock() ... Protocol not available
```

你也直接确认了 kernel 缺少：

```text
# CONFIG_IP_MROUTE is not set
# CONFIG_IPV6_MROUTE is not set
```

因此本指南的范围为：

- **当前可行的下一步：** 在现有 Jetson 镜像上进行 Zigbee 主机 + 协处理器的工作
- **暂缓的路径：** 之后再重建或更换 JetPack / kernel 以实现完整的 OTBR

有一点预期必须说清楚：Jetson 上的 Zigbee 不会像 Jetson 上的 Thread 那样。

- Thread + OTBR 会创建一个面向 Linux 的接口，例如 `wpan0`
- Zigbee NCP **不会**创建 Linux IP 接口
- 相反，Jetson 主机应用使用 Zigbee 协处理器协议通信，并自行管理 Zigbee 角色、入网、簇和设备状态

---

## 3. 目标架构

```text
Jetson Orin Nano
  |
  |-- SPI -> ESP32-C6 #1 -> ESP-Hosted -> wlan0 + hci0
  |
  |-- UART (recommended first) -> ESP32-C6 #2 -> Zigbee NCP
  |                                 ^
  |                                 |
  |                         ESP ZNSP over SLIP
  |
  +--> Linux host side
        |-- Zigbee host application / gateway logic
        |-- Coordinator or Router role management
        |
        +--> Wi-Fi / Ethernet / USB network uplink
```

**目标结果**

- Jetson 继续使用来自第一块 ESP32-C6 的 `wlan0` 作为其常规 Wi-Fi 路径
- Jetson 通过 UART 或 SPI 与第二块 ESP32-C6 通信，将其作为 Zigbee 协处理器
- Zigbee 侧可以组建或加入一个 Zigbee 网络
- 已加入的 Zigbee 设备由主机侧通过 Zigbee 命令管理，而不是通过 Linux 的 `wpan0` 接口管理

---

## 4. 为什么要用第二块 ESP32-C6

本指南有意让 Zigbee 不占用第一块 ESP32-C6。

原因：

- **ESP-Hosted** 已经把第一块芯片用作 Jetson 的 Wi-Fi/BLE 协处理器
- Zigbee 也要用 ESP32-C6 的 **802.15.4** 射频
- Espressif 的文档指出，ESP32-C6 有**一个共享的 2.4 GHz 射频模块**，供 Wi-Fi、Bluetooth LE 和 IEEE 802.15.4 使用，并通过时分复用管理访问
- 把这一切合并到一套自定义固件栈中理论上可行，但比使用两个射频要难稳定得多

因此更稳妥的拆分仍然是：

- **ESP32-C6 #1** 通过 ESP-Hosted 负责 Jetson 的 Wi-Fi/BLE
- **ESP32-C6 #2** 负责 Zigbee 协处理器工作

---

## 5. 硬件与软件前置要求

### 硬件

- Jetson Orin Nano 8GB Developer Kit
- 你现有的 **ESP32-C6 #1**，已通过 SPI 跑通 ESP-Hosted
- 一块**第二块 ESP32-C6 开发板**
- 用于直连 UART 的杜邦线，或者如果你先在开发板的 USB-UART 桥上验证，也可选配 USB 线
- 用于烧录和监视第二块 ESP32-C6 的 USB 线
- 一个或多个 Zigbee 设备用于后续验证：
  - 另一块运行 Zigbee 终端设备示例的 ESP32-C6 或 ESP32-H2
  - 一款商用 Zigbee 终端设备


<details>
<summary>English original</summary>

**ESP32-C6 Zigbee NCP on Jetson Orin Nano - Project Guide**

> **Goal:** Use a **second ESP32-C6** as a **Zigbee Network Co-Processor (NCP)** for the **Jetson Orin Nano 8GB Developer Kit**, so the Jetson can act as the higher-level Zigbee host while keeping your first ESP32-C6 dedicated to **ESP-Hosted Wi-Fi/BLE**.

**Hub:** [Network and Connectivity](Guide.md)  
**Related local guides:** [ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md) · [ESP32-C6 OpenThread RCP on Jetson Orin Nano](ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md) · [Peripheral Access](../01-外设访问/Guide.md)

---

**1. Why this project matters**

This is the clean next experiment after your current Jetson + ESP32-C6 networking work:

- the first ESP32-C6 already gives Jetson `wlan0` and BLE through **ESP-Hosted**
- the second ESP32-C6 already proved it can act as an **802.15.4 coprocessor**
- your current **Thread** path is not blocked at the radio or UART level, but **full OTBR** is blocked on the present Jetson kernel because `CONFIG_IP_MROUTE` and `CONFIG_IPV6_MROUTE` are missing

Zigbee is a good next step because it still uses **IEEE 802.15.4**, but it does **not** require OTBR's IPv6 border-routing path. That means the specific Linux multicast-routing blocker you hit with OTBR should not be the main issue for a Zigbee host/NCP design. That last point is an engineering inference from the protocol architecture, not a claim that the whole Jetson Zigbee stack is already turnkey.

What this project teaches:

- how a Linux host talks to a Zigbee coprocessor instead of a Thread RCP
- how Zigbee differs from Thread even though both use 802.15.4
- how to keep Wi-Fi/BLE and Zigbee on separate radios for a cleaner system design
- where Jetson integration becomes a host-application problem instead of a Linux-network-interface problem

---

**2. Current Status and Scope**

On your current Jetson image, the OpenThread RCP path has already been validated up to the point where the node can become `leader`. The failure happens later when `otbr-agent` tries to initialize border-routing support and the kernel reports:

```text
InitMulticastRouterSock() ... Protocol not available
```

You also confirmed directly that the kernel is missing:

```text
# CONFIG_IP_MROUTE is not set
# CONFIG_IPV6_MROUTE is not set
```

This guide is therefore scoped as:

- **current practical next path:** Zigbee host + coprocessor work on the existing Jetson image
- **deferred path:** rebuild or replace JetPack / kernel later for full OTBR

One expectation must be stated clearly: Zigbee on Jetson will not look like Thread on Jetson.

- Thread + OTBR creates a Linux-facing interface such as `wpan0`
- Zigbee NCP does **not** create a Linux IP interface
- instead, the Jetson host application speaks a Zigbee coprocessor protocol and manages Zigbee roles, joins, clusters, and device state itself

---

**3. Target Architecture**

```text
Jetson Orin Nano
  |
  |-- SPI -> ESP32-C6 #1 -> ESP-Hosted -> wlan0 + hci0
  |
  |-- UART (recommended first) -> ESP32-C6 #2 -> Zigbee NCP
  |                                 ^
  |                                 |
  |                         ESP ZNSP over SLIP
  |
  +--> Linux host side
        |-- Zigbee host application / gateway logic
        |-- Coordinator or Router role management
        |
        +--> Wi-Fi / Ethernet / USB network uplink
```

**Target outcome**

- Jetson keeps using `wlan0` from the first ESP32-C6 as its normal Wi-Fi path
- Jetson talks to the second ESP32-C6 over UART or SPI as a Zigbee coprocessor
- the Zigbee side can form or join a Zigbee network
- joined Zigbee devices are managed from the host side through Zigbee commands, not through a Linux `wpan0` interface

---

**4. Why Use a Second ESP32-C6**

This guide intentionally keeps Zigbee off the first ESP32-C6.

Why:

- **ESP-Hosted** already uses the first chip as a Jetson Wi-Fi/BLE coprocessor
- Zigbee also uses the ESP32-C6's **802.15.4** radio
- Espressif documents that ESP32-C6 has **one shared 2.4 GHz RF module** for Wi-Fi, Bluetooth LE, and IEEE 802.15.4, with time-division multiplexing managing access
- combining all of that into one custom firmware stack is possible in theory, but much harder to stabilize than using two radios

So the safer split remains:

- **ESP32-C6 #1** for Jetson Wi-Fi/BLE through ESP-Hosted
- **ESP32-C6 #2** for Zigbee coprocessor work

---

**5. Hardware and Software Prerequisites**

**Hardware**

- Jetson Orin Nano 8GB Developer Kit
- your existing **ESP32-C6 #1** already working with ESP-Hosted over SPI
- a **second ESP32-C6 dev board**
- jumper wires for a direct UART link, or optionally a USB cable if you first validate over the board's USB-UART bridge
- USB cable for flashing and monitoring the second ESP32-C6
- one or more Zigbee devices for later validation:
  - another ESP32-C6 or ESP32-H2 running a Zigbee end-device example
  - a commercial Zigbee end device

</details>

### 软件

- Jetson 上的 JetPack 6.x / L4T 36.x
- 在 Linux 构建机上安装好 ESP-IDF
- Espressif [ESP Zigbee SDK](https://github.com/espressif/esp-zigbee-sdk)
- Jetson 上 host 侧应用的方案：
  - 自己用 C/C++/Python 写的 Zigbee host 工具
  - 或日后基于 Espressif 已文档化的 NCP 协议编写的 host 示例

### 推荐的首选传输方式

Espressif 文档中的 Zigbee NCP 协议可走 **UART** 或 **SPI**。在 Jetson 上，**UART 是更好的首选传输方式**。

原因：

- 可避免与 ESP-Hosted 已占用的 SPI 总线冲突
- Jetson 排针 UART 通路在做 Thread/RCP 时电气特性已经摸清
- NCP 协议有明确的 UART 文档
- 调试带帧的串口流量，比同时调试一套新的 SPI host 协议栈和一套新的 Zigbee 协议栈更简单

若要最快完成实验室 bring-up（上电点亮/调通），用板载 USB-UART 接到 Jetson 是可以接受的。若要更干净的嵌入式通路，等 host 侧逻辑理清后改用 Jetson 排针 UART。

---

## 6. 你要实现的官方模型

Espressif 当前的 Zigbee 资料给出的是**两种相关但不同的 host 侧模式**：

### Zigbee NCP 模式

官方 Zigbee NCP 指南把 **ESP ZNSP** 定义为 **host 应用处理器**用来与 **Network Co-Processor** 上的 Zigbee 协议栈交互的协议。这些帧通过 **SLIP** 承载，传输层可以是 **UART** 或 **SPI**。

这个概念模型最契合 Jetson：

- Jetson 是 host 处理器
- ESP32-C6 作为协处理器运行 Zigbee 协议栈
- Jetson 通过 NCP 链路下发网络管理与应用命令

官方 NCP API 还给出了以下 host 连接模式：

- `NCP_HOST_CONNECTION_MODE_UART`
- `NCP_HOST_CONNECTION_MODE_SPI`

### Zigbee gateway + RCP 模式

Espressif 当前的示例树里还有一个 **`zigbee_gateway`** 示例。该示例运行在支持 Wi-Fi 的 ESP host SoC 上，并使用在另一颗芯片上运行 `ot_rcp` 的 **802.15.4 RCP**。

这对 Jetson 有两点意义：

- 说明 Espressif 已经把 Zigbee gateway 设计当作 **多芯片 host + 802.15.4 射频**问题来处理
- 也说明最新官方示例的重点更偏向 **ESP host SoC gateway**，而不是开箱即用的 Linux host 守护进程

截至 **2026 年 4 月 21 日**，官方 `esp_zigbee_ncp` 示例的 README 仍表示稍后会提供一个独立的 host 示例作为参考。因此对 Jetson 的正确预期是：

- **wire 协议与 NCP 设备侧已有官方文档**
- **Linux host 侧仍比 OTBR 更需要自行开发**

---

## 7. Jetson 上的 Zigbee 角色：哪些合理

Espressif 文档支持以下角色：

- **Coordinator**
- **Router**
- **(Sleepy) End Device**

对 Jetson 承载的 gateway 来说，最有用的角色是：

### Coordinator

如果由 Jetson 自己创建并管理 Zigbee 网络，就用这个角色。对充当 gateway、bridge 或 coordinator 设备的 Linux host 来说，这是最自然的角色。

### Router

如果 Jetson 要加入已有的 Zigbee 网络，而不是自建网络，就用这个角色。

### End Device

对 Jetson 级别的 host 来说，这通常**不是**你想要的角色。end device 行为更适合电池设备或小型应用节点，而不是 Linux gateway。

---

## 8. 把 Jetson UART 接到 Zigbee NCP

官方 `esp_zigbee_ncp` 示例给出了一种简单的 UART 连接：host TX 接 NCP 的 `GPIO4` RX 引脚，host RX 来自 NCP 的 `GPIO5` TX 引脚。这与你在 Jetson 上已用过的 UART 接线方式很吻合。

对于当前镜像上的 Jetson Orin Nano 排针通路：

|---|---:|---|---|

| 功能 | Jetson 引脚 | Linux 设备 | 方向 |
| UART1_TXD | `8` | `/dev/ttyTHS1` | Jetson -> ESP |
| UART1_RXD | `10` | `/dev/ttyTHS1` | ESP -> Jetson |
| 接地 | `6` 或 `9` 或 `14` | -- | 公共参考地 |

ESP32-C6 NCP 侧：

|---|---:|---|

| 功能 | ESP32-C6 引脚 | 方向 |
| NCP RX | `GPIO4` | Jetson TX -> ESP RX |
| NCP TX | `GPIO5` | ESP TX -> Jetson RX |
| 接地 | `GND` | 公共参考地 |

因此直连方式为：

```text
Jetson pin 8  (UART1_TXD) -> ESP32-C6 GPIO4
Jetson pin 10 (UART1_RXD) <- ESP32-C6 GPIO5
Jetson GND                -> ESP32-C6 GND
```

官方 NCP 示例的 README 还指出，这些 UART 引脚可在以下位置修改：

```text
idf.py menuconfig -> Component config -> Zigbee Network Co-processor
```

因此，如果板级布线或所选传输方式不同，请把示例的引脚设置与自己的台面接线当作一组配套设置。


<details>
<summary>English original</summary>

**Software**

- JetPack 6.x / L4T 36.x on Jetson
- ESP-IDF installed on a Linux build machine
- Espressif [ESP Zigbee SDK](https://github.com/espressif/esp-zigbee-sdk)
- a host-side application plan on Jetson:
  - your own Zigbee host tool in C/C++/Python
  - or a future host example based on Espressif's documented NCP protocol

**Recommended first transport**

Espressif documents the Zigbee NCP protocol over either **UART** or **SPI**. On your Jetson, **UART is the better first transport**.

Why:

- it avoids colliding with the SPI bus already used by ESP-Hosted
- your Jetson header UART path is already electrically understood from the Thread/RCP work
- the NCP protocol is explicitly documented over UART
- debugging framed serial traffic is simpler than debugging a new SPI host stack and a new Zigbee stack at the same time

For the fastest lab bring-up, a board USB-UART path to Jetson is acceptable. For the cleaner embedded path, move to the Jetson header UART once the host logic is understood.

---

**6. The Official Models You Are Implementing**

Espressif's current Zigbee material exposes **two related but distinct host-side patterns**:

**Zigbee NCP model**

The official Zigbee NCP guide defines **ESP ZNSP** as the protocol a **host application processor** uses to interact with the Zigbee stack on a **Network Co-Processor**. Those frames are carried over **SLIP**, and the transport can be **UART** or **SPI**.

That is the conceptual model that best fits Jetson:

- Jetson is the host processor
- ESP32-C6 runs the Zigbee stack as the coprocessor
- Jetson sends network-management and application commands over the NCP link

The official NCP API also exposes host connection modes for:

- `NCP_HOST_CONNECTION_MODE_UART`
- `NCP_HOST_CONNECTION_MODE_SPI`

**Zigbee gateway + RCP model**

Espressif's current example tree also contains a **`zigbee_gateway`** example. That example runs on a Wi-Fi-capable ESP host SoC and uses an **802.15.4 RCP** running `ot_rcp` on another chip.

That matters for Jetson for two reasons:

- it shows Espressif already treats Zigbee gateway designs as a **multi-chip host + 802.15.4 radio** problem
- it also shows that the latest official example emphasis is stronger on an **ESP host SoC gateway** than on a turnkey Linux host daemon

As of **April 21, 2026**, the official `esp_zigbee_ncp` example README still says a separate host example will be provided later as a reference. So the right expectation on Jetson is:

- the **wire protocol and NCP device side are officially documented**
- the **Linux host side is still more custom than OTBR**

---

**7. Zigbee Roles on Jetson: What Makes Sense**

Espressif documents support for:

- **Coordinator**
- **Router**
- **(Sleepy) End Device**

For a Jetson-hosted gateway, the most useful roles are:

**Coordinator**

Use this if the Jetson should create and manage the Zigbee network itself. This is the most natural role for a Linux host acting as a gateway, bridge, or coordinator appliance.

**Router**

Use this if the Jetson should join an existing Zigbee network instead of forming its own.

**End Device**

This is generally **not** the role you want for a Jetson-class host. End-device behavior is more appropriate for battery devices or compact application nodes, not for a Linux gateway.

---

**8. Wire the Jetson UART to the Zigbee NCP**

The official `esp_zigbee_ncp` example shows a simple UART connection where the host TX goes to the NCP's `GPIO4` RX pin and the host RX comes from the NCP's `GPIO5` TX pin. That aligns well with the UART wiring pattern you already used on Jetson.

For the Jetson Orin Nano header path on your current image:

| Function | Jetson pin | Linux device | Direction |
|---|---:|---|---|
| UART1_TXD | `8` | `/dev/ttyTHS1` | Jetson -> ESP |
| UART1_RXD | `10` | `/dev/ttyTHS1` | ESP -> Jetson |
| Ground | `6` or `9` or `14` | -- | common reference |

For the ESP32-C6 NCP side:

| Function | ESP32-C6 pin | Direction |
|---|---:|---|
| NCP RX | `GPIO4` | Jetson TX -> ESP RX |
| NCP TX | `GPIO5` | ESP TX -> Jetson RX |
| Ground | `GND` | common reference |

So the direct wiring is:

```text
Jetson pin 8  (UART1_TXD) -> ESP32-C6 GPIO4
Jetson pin 10 (UART1_RXD) <- ESP32-C6 GPIO5
Jetson GND                -> ESP32-C6 GND
```

The official NCP example README also notes that these UART pins can be changed in:

```text
idf.py menuconfig -> Component config -> Zigbee Network Co-processor
```

So if your board routing or chosen transport differs, treat the example's pin settings and your bench wiring as one matched pair.

</details>

### Jetson UART 前置要求

Thread 指南中关于 Jetson 侧 UART 的注意事项同样适用：

- 如果 `nvgetty` 占用了排针 UART，则禁用它
- 让该端口保持在 **3.3 V 逻辑**电平
- 不要让多个进程争抢 `/dev/ttyTHS1`

端口基本配置示例：

```bash
sudo systemctl stop nvgetty
sudo systemctl disable nvgetty
sudo stty -F /dev/ttyTHS1 115200 cs8 -cstopb -parenb raw -echo
```

使用两侧实际配置的波特率。Zigbee NCP 示例所用的具体波特率必须与主机应用的串口配置一致。

---

## 9. 构建并烧写 ESP32-C6 Zigbee NCP 固件

官方 Techpedia Zigbee 方案页面指向：

```text
esp-zigbee-sdk/examples/esp_zigbee_ncp
```

当前官方 GitHub 示例页面将其记录为 ESP32-C6 的 NCP 设备示例。

在 Linux 构建主机上：

```bash
git clone --depth=1 https://github.com/espressif/esp-zigbee-sdk.git
cd esp-zigbee-sdk/examples/esp_zigbee_ncp

# Use a compatible ESP-IDF version for the SDK
# The current SDK README recommends ESP-IDF v5.5.4 for new work.

idf.py set-target esp32c6
idf.py menuconfig
idf.py build
```

烧写它：

```bash
# Use the actual USB serial port for the second ESP32-C6 board
idf.py -p /dev/ttyUSB0 flash monitor
```

该示例 README 说明：

- 该开发板作为 Zigbee NCP 工作
- 它通过 UART 与主机协同工作
- `GPIO4` / `GPIO5` 是示例的 RX/TX 映射
- `NETWORK_INIT`、`NETWORK_PRIMARY_CHANNEL_SET`、`NETWORK_FORMNETWORK`、`START` 等主机侧操作会触发网络组建与协调器行为

### 端口命名说明

与 Thread 指南相同：

- 不要硬编码 `/dev/ttyACM0`
- 不要假定每块板子都显示为 `/dev/ttyUSB0`

使用 Linux 机器上第二块 ESP32-C6 板实际暴露的设备路径。

---

## 10. Jetson 主机实际需要做什么

这是与 OTBR 最大的概念差异。

采用 Thread + OTBR 时，Jetson 主机启动一个已有的 daemon，并得到一条面向 Linux 的控制路径，例如 `ot-ctl`。采用 Zigbee NCP 时，主机必须更直接地驱动 Zigbee 控制流。

官方 Zigbee NCP 帧列表列出的主机操作包括：

- `NETWORK_INIT`
- `NETWORK_START`
- `NETWORK_STATE`
- `NETWORK_FORM`
- `NETWORK_JOIN`
- `NETWORK_PERMIT_JOINING`
- `NETWORK_ROLE_GET` / `NETWORK_ROLE_SET`
- `NETWORK_CHANNEL_SET`
- `NETWORK_PAN_ID_SET`
- `NETWORK_PRIMARY_KEY_SET`
- `ZCL_ATTR_READ`
- `ZCL_ATTR_WRITE`
- `APS_DATA_REQUEST`

因此 Jetson 上的主机侧负责：

- 打开 UART 或 SPI 传输
- 对 **ESP ZNSP** 包进行 SLIP 成帧与解析
- 组建或加入一个 Zigbee 网络
- 跟踪角色与协议栈状态
- 向设备发送 ZDO、APS 和 ZCL 命令
- 对入网、离网、属性上报和指示事件做出响应

这就是为什么 Jetson 上的 Zigbee **不像 OTBR 那样被同一个 kernel 问题阻塞**，但也**尚未像 `otbr-agent` 那样开箱即用**。

---

## 11. Jetson 上推荐的 bring-up（上电点亮/调通）里程碑

把它当作一个分阶段的主机集成问题来处理。

### 里程碑 1：仅 NCP 传输

验证 Jetson 能够打开 NCP 串口路径，并在不出现复位或丢字节的情况下交换成帧流量。

成功的表现是：

- `/dev/ttyTHS1` 上没有争抢
- ESP 侧的 NCP 启动日志稳定
- 主机/NCP 的成帧交换成功

### 里程碑 2：基本网络控制

实现或验证：

- `NETWORK_INIT`
- `NETWORK_STATE`
- `NETWORK_ROLE_SET`
- 信道 / PAN / 密钥配置命令

成功的表现是：

- 主机能读取协议栈状态
- 主机能设置预期角色
- NCP 能正确持久化或上报所配置的网络设置

### 里程碑 3：组建协调器网络

如果 Jetson 作为网关：

- 设置 Coordinator 角色
- 组建新网络
- 在测试时间窗内开放入网

成功的表现是：

- 网络组建成功
- permit-join 窗口打开
- 一个测试用 Zigbee 终端设备能够入网

### 里程碑 4：真实设备控制

从协议栈 bring-up 转向应用行为：

- 发现已入网设备
- 读写属性
- 测试簇命令
- 验证设备通知或上报能到达主机

到那时，Zigbee 集成就不再是串口演示练习，而成为真正的协调器/网关设计。

---


<details>
<summary>English original</summary>

**Jetson UART prerequisites**

The same Jetson-side UART precautions from the Thread guide still apply:

- disable `nvgetty` if it owns the header UART
- keep the port on **3.3 V logic**
- do not let multiple processes fight over `/dev/ttyTHS1`

Basic port setup example:

```bash
sudo systemctl stop nvgetty
sudo systemctl disable nvgetty
sudo stty -F /dev/ttyTHS1 115200 cs8 -cstopb -parenb raw -echo
```

Use the actual baud configured on both sides. The exact Zigbee NCP example baud must match the host application's serial configuration.

---

**9. Build and Flash the ESP32-C6 Zigbee NCP Firmware**

The official Techpedia Zigbee solution page points to:

```text
esp-zigbee-sdk/examples/esp_zigbee_ncp
```

and the current official GitHub example page documents it as the NCP device example for ESP32-C6.

On your Linux build host:

```bash
git clone --depth=1 https://github.com/espressif/esp-zigbee-sdk.git
cd esp-zigbee-sdk/examples/esp_zigbee_ncp

# Use a compatible ESP-IDF version for the SDK
# The current SDK README recommends ESP-IDF v5.5.4 for new work.

idf.py set-target esp32c6
idf.py menuconfig
idf.py build
```

Flash it:

```bash
# Use the actual USB serial port for the second ESP32-C6 board
idf.py -p /dev/ttyUSB0 flash monitor
```

The example README documents:

- the board acts as a Zigbee NCP
- it works together with a host via UART
- `GPIO4` / `GPIO5` are the example RX/TX mapping
- host-side operations such as `NETWORK_INIT`, `NETWORK_PRIMARY_CHANNEL_SET`, `NETWORK_FORMNETWORK`, and `START` trigger network formation and coordinator behavior

**Port naming note**

As with the Thread guide:

- do not hardcode `/dev/ttyACM0`
- do not assume every board shows up as `/dev/ttyUSB0`

Use the actual device path exposed by the second ESP32-C6 board on your Linux machine.

---

**10. What the Jetson Host Actually Has to Do**

This is the biggest conceptual difference from OTBR.

With Thread + OTBR, the Jetson host launches an existing daemon and gets a Linux-facing control path such as `ot-ctl`. With Zigbee NCP, the host must drive the Zigbee control flow more directly.

The official Zigbee NCP frame list shows host operations such as:

- `NETWORK_INIT`
- `NETWORK_START`
- `NETWORK_STATE`
- `NETWORK_FORM`
- `NETWORK_JOIN`
- `NETWORK_PERMIT_JOINING`
- `NETWORK_ROLE_GET` / `NETWORK_ROLE_SET`
- `NETWORK_CHANNEL_SET`
- `NETWORK_PAN_ID_SET`
- `NETWORK_PRIMARY_KEY_SET`
- `ZCL_ATTR_READ`
- `ZCL_ATTR_WRITE`
- `APS_DATA_REQUEST`

So the host side on Jetson is responsible for:

- opening the UART or SPI transport
- SLIP-framing and parsing **ESP ZNSP** packets
- forming or joining a Zigbee network
- keeping track of role and stack status
- sending ZDO, APS, and ZCL commands to devices
- reacting to join, leave, attribute-report, and indication events

That is why Zigbee on Jetson is **not blocked by the same kernel issue as OTBR**, but is also **not yet as turnkey as `otbr-agent`**.

---

**11. Recommended Bring-Up Milestones on Jetson**

Treat this as a staged host-integration problem.

**Milestone 1: NCP transport only**

Prove the Jetson can open the NCP serial path and exchange framed traffic without resets or dropped bytes.

Success looks like:

- no contention on `/dev/ttyTHS1`
- stable NCP startup logs on the ESP side
- successful host/NCP framing exchange

**Milestone 2: basic network control**

Implement or validate:

- `NETWORK_INIT`
- `NETWORK_STATE`
- `NETWORK_ROLE_SET`
- channel / PAN / key configuration commands

Success looks like:

- the host can read stack state
- the host can set the intended role
- the NCP persists or reports the configured network settings correctly

**Milestone 3: form a coordinator network**

If Jetson is the gateway:

- set Coordinator role
- form a new network
- open joining for a test window

Success looks like:

- the network forms successfully
- permit-join window opens
- a test Zigbee end device can join

**Milestone 4: real device control**

Move from stack bring-up to application behavior:

- discover joined devices
- read or write attributes
- test cluster commands
- verify device notifications or reports reach the host

At that point, Zigbee integration is no longer a serial-demo exercise. It becomes a real coordinator/gateway design.

---

</details>

## 12. 为什么这条路径不同于当前的 Thread 阻塞点

当前的 OTBR 失效是特指 **Linux border routing**，而不是 ESP32-C6 射频或 802.15.4 本身。

Thread 工作给出的证据是：

- Jetson 已经能与 ESP32-C6 RCP 通信
- Thread 节点已经能变成 `leader`
- 崩溃发生在之后，OTBR 尝试初始化 Linux multicast-routing 支持时

Zigbee NCP 改变了问题的形态：

- 没有 OTBR
- 没有 `wpan0`
- 没有 IPv6 border-routing socket
- 主机侧逻辑改为通过串口传输 Zigbee 控制消息

因此缺失的 Jetson kernel 选项：

- `CONFIG_IP_MROUTE`
- `CONFIG_IPV6_MROUTE`

不应成为 Zigbee host/NCP 工作的主要阻塞点。

这就是架构上的理由：在把 JetPack/kernel 重建推迟到后续 Thread OTBR 工作时，这是正确的下一个实验。

---

## 13. 常见失效模式

### 主机期望出现一个类似 `wpan0` 的 Linux 无线接口

对 Zigbee NCP 而言这是错误的模型。成功的衡量标准不是新增一个 Linux 网络接口，而是主机/NCP 命令交互、组网或入网，以及实际的 Zigbee 设备控制。

### UART 传输不稳定

典型原因：

- 波特率错误
- 多个进程同时打开 `/dev/ttyTHS1`
- `nvgetty` 仍处于连接状态
- TX/RX 接反
- ESP UART 引脚配置与台面接线不匹配

### 你假设当前 SDK 仓库只支持网关/RCP 路径

官方文档同时公开了：

- Zigbee 网关 / RCP 示例
- Zigbee NCP 示例与 NCP 协议文档

但 Linux 主机侧仍比 Thread OTBR 流程更偏手动，所以要把文档读作 API/协议契约，而不是 Jetson 上一条命令就能起 daemon 的承诺。

### 试图立刻把 Zigbee 合并到第一个 ESP32-C6 上

暂时避免这么做。

你已经拥有：

- 可工作的 ESP-Hosted 拆分
- 已验证的第二射频方案

在独立的 Zigbee 路径稳定之前，保持这套干净架构。

---

## 14. 好的下一步

- 把 `esp_zigbee_ncp` 烧录到第二个 ESP32-C6 上
- 先跑 UART，再尝试 SPI
- 把 Jetson 侧工作当作主机协议 bring-up（上电点亮/调通），而不是 Linux netdev bring-up
- 如果 Jetson 要作为网关，优先选 **Coordinator**
- 把 Thread OTBR kernel 重建保留为后续独立里程碑

---

## 15. 参考资料

### 官方上游参考

- [ESP Zigbee SDK 介绍](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/introduction.html)
- [ESP Zigbee NCP 指南](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/user-guide/ncp.html)
- [ESP Zigbee NCP API 参考](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/api-reference/esp_zigbee_ncp.html)
- [ESP Zigbee SDK 仓库](https://github.com/espressif/esp-zigbee-sdk)
- [ESP Zigbee NCP 示例](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_ncp)
- [ESP Zigbee 网关示例](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/zigbee_gateway)
- [ESP-Techpedia Zigbee 方案介绍](https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/solution-introduction/zigbee/zigbee-solution.html)
- [ESP32-C6 射频共存](https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32c6/api-guides/coexist.html)

### 本地路线图参考

- [ESP32-C6 在 Jetson Orin Nano 上通过 SPI 运行 ESP-Hosted](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md)
- [ESP32-C6 在 Jetson Orin Nano 上运行 OpenThread RCP](ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md)
- [网络与连接中心](Guide.md)


<details>
<summary>English original</summary>

**12. Why This Path Is Different from the Current Thread Blocker**

Your current OTBR failure is specifically tied to **Linux border routing**, not to the ESP32-C6 radio or 802.15.4 itself.

The proof from the Thread work is:

- Jetson could already talk to the ESP32-C6 RCP
- the Thread node could already become `leader`
- the crash happened later when OTBR tried to initialize Linux multicast-routing support

Zigbee NCP changes the problem shape:

- no OTBR
- no `wpan0`
- no IPv6 border-routing socket
- host logic speaks Zigbee control messages over serial instead

So the missing Jetson kernel options:

- `CONFIG_IP_MROUTE`
- `CONFIG_IPV6_MROUTE`

should not be the main blocker for Zigbee host/NCP work.

That is the architectural reason this is the right next experiment while you defer the JetPack/kernel rebuild for later Thread OTBR work.

---

**13. Common Failure Modes**

**The host expects a Linux radio interface like `wpan0`**

That is the wrong model for Zigbee NCP. Success is not measured by a new Linux network interface. It is measured by host/NCP command exchange, network formation or join, and actual Zigbee device control.

**UART transport is unstable**

Typical causes:

- wrong baud rate
- multiple processes opening `/dev/ttyTHS1`
- `nvgetty` still attached
- TX/RX swapped
- ESP UART pin config does not match the bench wiring

**You assume the current SDK repo only supports the gateway/RCP path**

The official docs expose both:

- a Zigbee gateway / RCP example
- a Zigbee NCP example and NCP protocol documentation

But the Linux host side is still more manual than the Thread OTBR flow, so read the docs as an API/protocol contract, not as a promise of a one-command Jetson daemon.

**Trying to merge Zigbee onto the first ESP32-C6 immediately**

Avoid it for now.

You already have:

- a working ESP-Hosted split
- a validated second-radio concept

Preserve that clean architecture until the separate Zigbee path is stable.

---

**14. Good Next Steps**

- flash `esp_zigbee_ncp` onto the second ESP32-C6
- start with UART before attempting SPI
- treat the Jetson work as a host-protocol bring-up, not a Linux netdev bring-up
- choose **Coordinator** first if Jetson is meant to be the gateway
- keep the Thread OTBR kernel rebuild as a separate later milestone

---

**15. References**

**Official upstream references**

- [ESP Zigbee SDK introduction](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/introduction.html)
- [ESP Zigbee NCP guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/user-guide/ncp.html)
- [ESP Zigbee NCP API reference](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/api-reference/esp_zigbee_ncp.html)
- [ESP Zigbee SDK repository](https://github.com/espressif/esp-zigbee-sdk)
- [ESP Zigbee NCP example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_ncp)
- [ESP Zigbee gateway example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/zigbee_gateway)
- [ESP-Techpedia Zigbee solution introduction](https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/solution-introduction/zigbee/zigbee-solution.html)
- [ESP32-C6 RF coexistence](https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32c6/api-guides/coexist.html)

**Local roadmap references**

- [ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md)
- [ESP32-C6 OpenThread RCP on Jetson Orin Nano](ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md)
- [Network and Connectivity hub](Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/2. Network and Connectivity/ESP32-C6-Zigbee-NCP-Jetson-Orin-Nano.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/2.%20Network%20and%20Connectivity/ESP32-C6-Zigbee-NCP-Jetson-Orin-Nano.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
