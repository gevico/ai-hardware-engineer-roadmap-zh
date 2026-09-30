---
title: Jetson Orin Nano 上的 ESP32-C6 OpenThread RCP - 项目指南
description: Jetson Orin Nano 上的 ESP32-C6 OpenThread RCP - 项目指南
---

# Jetson Orin Nano 上的 ESP32-C6 OpenThread RCP - 项目指南

> **目标：** 将**第二块 ESP32-C6** 作为 **OpenThread Radio Co-Processor（RCP）** 为 **Jetson Orin Nano 8GB Developer Kit** 完成 bring-up（上电点亮/调通），使 Jetson 能够运行 **OpenThread Border Router（OTBR）** 或 **OpenThread Daemon**，同时让第一块 ESP32-C6 继续专用于 **ESP-Hosted Wi-Fi/BLE**。

**Hub：** [Network and Connectivity](Guide.md)  
**相关本地指南：** [ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md) · [Peripheral Access](../01-外设访问/Guide.md)

---

## 1. 为什么这个项目重要

当你的 Jetson 已经具备：

- 通过 SPI 由 **ESP-Hosted** 提供的 `wlan0`
- 由同一块 ESP32-C6 提供的、用于 BLE 的 `hci0`

那么对 **Thread** 而言，干净的下一步不是让同一颗芯片超载。更干净的架构是：

- 让第一块 ESP32-C6 继续作为 Jetson 的 Wi-Fi/BLE 协处理器
- 增加**第二块 ESP32-C6**，专用于 **802.15.4 / Thread**
- 让 Jetson 运行宿主侧的 OpenThread 协议栈

这遵循 OpenThread 标准的 **RCP 设计**：

- **host processor** 运行 OpenThread 协议栈
- **RCP** 只处理 Thread 射频 / MAC 层
- host 与 RCP 通过 **Spinel** 通信

该模型很适合 Jetson，因为 Linux 宿主始终在线，且性能足以运行 OTBR 及其他边缘服务。

---

## 2. 目标架构

```text
Jetson Orin Nano
  |
  |-- SPI -> ESP32-C6 #1 -> ESP-Hosted -> wlan0 + hci0
  |
  |-- UART (recommended first) -> ESP32-C6 #2 -> OpenThread RCP
  |                                 ^
  |                                 |
  |                             Spinel protocol
  |
  +--> Linux host side
        |-- otbr-agent   (for Thread Border Router)
        |-- or ot-daemon (for lighter RCP host use)
        |
        +--> wpan0
```

**目标结果**

- Jetson 继续使用来自第一块 ESP32-C6 的 `wlan0` 作为其常规 Wi-Fi 接口
- Jetson 通过 `wpan0` 获得一条 Thread 射频通路
- `otbr-agent` 或 `ot-daemon` 可通过 Spinel 与第二块 ESP32-C6 通信
- 第二块 ESP32-C6 专用于 Thread RCP，不与 ESP-Hosted 共享

---

## 3. 为什么使用第二块 ESP32-C6

本指南有意避开单芯片「在一块 ESP32-C6 上做所有事」的路线。

原因：

- **ESP-Hosted** 是一种 Linux Wi-Fi/BLE 协处理器方案
- **OpenThread RCP** 是另一种 host/协处理器模型
- 两者都想控制同一套射频与 host 传输通路
- 把它们合并为一份稳定固件的集成工作量，远高于使用两颗芯片

Espressif 官方文档说明：

- **ESP32-C6** 支持 **RCP mode**
- RCP 传输可以是 **SPI 或 UART**
- 对于基于 Wi-Fi 的 Thread Border Router 产品，推荐采用 **dual-SoC** 架构以获得更好的共存行为

因此本指南采用更安全的工程拆分：

- **ESP32-C6 #1** 用于 Jetson Wi-Fi/BLE
- **ESP32-C6 #2** 用于 Thread RCP

---

## 4. 硬件与软件前置要求

### 硬件

- Jetson Orin Nano 8GB Developer Kit
- 你现有的 **ESP32-C6 #1**，已通过 SPI 与 ESP-Hosted 正常工作
- **第二块 ESP32-C6 开发板**
- 用于 Jetson 与第二块 ESP32-C6 之间直连 UART 链路的杜邦线
- 用于烧录与监视第二块 ESP32-C6 的 USB 线缆
- 可选：再准备一个支持 Thread 的设备用于验证：
  - 另一块 ESP32-C6
  - ESP32-H2
  - 一个 Matter-over-Thread 终端设备

### 软件

- Jetson 上的 JetPack 6.x / L4T 36.x
- 在 Linux 构建/烧录机器上安装 ESP-IDF
- Jetson 上的 OpenThread Border Router 宿主软件：
  - 如果你想要真正的 Thread Border Router，用 `ot-br-posix`
  - 或者如果你想要更轻量的 RCP host 配置，用 OpenThread POSIX `ot-daemon`

### 推荐的初始传输方式

尽管 ESP-IDF 说明 RCP 可以使用 **SPI 或 UART**，本指南采用 Jetson 的 **40-pin header UART1** 作为首个真实 host 传输通路。

原因：

- 与 ESP-Hosted 已占用的 SPI 总线发生冲突的风险更低
- 在此已验证配置上给出稳定的 Jetson 设备路径：**`/dev/ttyTHS1`**
- 它反映真实的 host/RCP 接线模型，而不是将其隐藏在 USB-UART 桥后面

你仍然可以使用 ESP 板的 USB 连接来：

- 烧录
- 串口监视日志
- bring-up（上电点亮/调通）期间供电

如果之后需要更低延迟，可以再优化为 SPI。

---


<details>
<summary>English original</summary>

**ESP32-C6 OpenThread RCP on Jetson Orin Nano - Project Guide**

> **Goal:** Bring up a **second ESP32-C6** as an **OpenThread Radio Co-Processor (RCP)** for the **Jetson Orin Nano 8GB Developer Kit**, so the Jetson can run **OpenThread Border Router (OTBR)** or **OpenThread Daemon** while keeping your first ESP32-C6 dedicated to **ESP-Hosted Wi-Fi/BLE**.

**Hub:** [Network and Connectivity](Guide.md)  
**Related local guides:** [ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md) · [Peripheral Access](../01-外设访问/Guide.md)

---

**1. Why this project matters**

Once your Jetson already has:

- `wlan0` from **ESP-Hosted** over SPI
- `hci0` from the same ESP32-C6 for BLE

the clean next step for **Thread** is not to overload that same chip. The cleaner architecture is:

- keep the first ESP32-C6 as the Jetson's Wi-Fi/BLE coprocessor
- add a **second ESP32-C6** dedicated to **802.15.4 / Thread**
- let the Jetson run the host-side OpenThread stack

This follows the standard **RCP design** from OpenThread:

- the **host processor** runs the OpenThread stack
- the **RCP** only handles the Thread radio / MAC layer
- host and RCP talk over **Spinel**

That model is a good fit for Jetson because the Linux host is always on and already powerful enough to run OTBR and other edge services.

---

**2. Target architecture**

```text
Jetson Orin Nano
  |
  |-- SPI -> ESP32-C6 #1 -> ESP-Hosted -> wlan0 + hci0
  |
  |-- UART (recommended first) -> ESP32-C6 #2 -> OpenThread RCP
  |                                 ^
  |                                 |
  |                             Spinel protocol
  |
  +--> Linux host side
        |-- otbr-agent   (for Thread Border Router)
        |-- or ot-daemon (for lighter RCP host use)
        |
        +--> wpan0
```

**Target outcome**

- Jetson keeps using `wlan0` from the first ESP32-C6 as its normal Wi-Fi interface
- Jetson gains a Thread radio path through `wpan0`
- `otbr-agent` or `ot-daemon` can talk to the second ESP32-C6 over Spinel
- the second ESP32-C6 is dedicated to Thread RCP use and is not shared with ESP-Hosted

---

**3. Why use a second ESP32-C6**

This guide intentionally avoids the single-chip “do everything on one ESP32-C6” path.

Why:

- **ESP-Hosted** is a Linux Wi-Fi/BLE coprocessor solution
- **OpenThread RCP** is a different host/co-processor model
- both want control of the same radio and host transport path
- the integration effort to merge them into one stable firmware is much higher than using two chips

Espressif officially documents:

- **ESP32-C6** supports **RCP mode**
- the RCP transport can be **SPI or UART**
- for Wi-Fi-based Thread Border Router products, a **dual-SoC** architecture is recommended for better coexistence behavior

So this guide uses the safer engineering split:

- **ESP32-C6 #1** for Jetson Wi-Fi/BLE
- **ESP32-C6 #2** for Thread RCP

---

**4. Hardware and software prerequisites**

**Hardware**

- Jetson Orin Nano 8GB Developer Kit
- your existing **ESP32-C6 #1** already working with ESP-Hosted over SPI
- a **second ESP32-C6 dev board**
- jumper wires for a direct UART link between Jetson and the second ESP32-C6
- USB cable for flashing and monitoring the second ESP32-C6
- optionally, one more Thread-capable device for validation:
  - another ESP32-C6
  - ESP32-H2
  - a Matter-over-Thread end device

**Software**

- JetPack 6.x / L4T 36.x on Jetson
- ESP-IDF installed on a Linux build/flash machine
- OpenThread Border Router host software on Jetson:
  - `ot-br-posix` if you want a real Thread Border Router
  - or OpenThread POSIX `ot-daemon` if you want a lighter RCP host setup

**Recommended first transport**

Although ESP-IDF says RCP can use **SPI or UART**, this guide uses the Jetson's **40-pin header UART1** as the first real host transport.

Why:

- less risk of colliding with the SPI bus already used by ESP-Hosted
- it gives you a stable Jetson device path on this validated setup: **`/dev/ttyTHS1`**
- it reflects the real host/RCP wiring model instead of hiding it behind a USB-UART bridge

You can still use the ESP board's USB connection for:

- flashing
- serial monitor logs
- power during bring-up

You can optimize to SPI later if you need lower latency.

---

</details>

## 5. 你要实现的官方模型

OpenThread 的 **RCP 设计**意味着：

- OpenThread 核心运行在 **主机处理器** 上
- 射频芯片运行最小化的控制器固件
- 主机与控制器通过 **Spinel** 通信

在 Jetson 上，这意味着：

- **Jetson** 运行 `otbr-agent` 或 `ot-daemon`
- **ESP32-C6 #2** 运行来自 ESP-IDF 的 **`ot_rcp`** 固件

OpenThread 自身的协处理器文档将其描述为 **RCP 设计**，且 OT Daemon 被明确记录为 RCP 配置中的 POSIX 侧组件。

对于骨干/基础设施侧，在 Jetson 上有多种有效选择：

- 若想使用 Jetson USB 设备网络路径连接到主机 PC，则用 `l4tbr0`
- 若 Jetson 通过你的第一个 ESP32-C6 和 ESP-Hosted 接入网络，则用 `wlan0`
- 若之后用 Ethernet 作为 OTBR 骨干，则用 `enP8p1s0`

在当前配置下，USB 网络应选 **`l4tbr0`**，而不是 `usb0` 或 `usb1`，因为这些接口是 `l4tbr0` 网桥的成员。

---

## 6. 构建并烧录 ESP32-C6 RCP 固件

对第二个 ESP32-C6 使用 **ESP-IDF `ot_rcp` 示例**。

在你的 Linux 构建主机上：

```bash
cd $IDF_PATH/examples/openthread/ot_rcp

# Select the chip once
idf.py set-target esp32c6

# Optional: inspect settings
idf.py menuconfig

# Build
idf.py build
```

将其烧录到第二个 ESP32-C6：

```bash
# Use the actual serial port for your board
# On many ESP32-C6 dev boards with CP210x this is /dev/ttyUSB0
idf.py -p /dev/ttyUSB0 flash monitor
```

### 本指南中重要的 `menuconfig` 选择

若 `ot_rcp` 菜单显示：

```text
[*] Configure RCP UART pin manually
(4)     The number of RX pin
(5)     The number of TX pin
```

这与下文记录的 Jetson 直连 UART 接线一致。

本指南中：

- 保持 **`Configure RCP UART pin manually`** 启用
- 设置 **RCP RX pin = `4`**
- 设置 **RCP TX pin = `5`**
- 首次 bring-up（上电点亮/调通）时保持 **external coexist wire** 禁用

这意味着：

- Jetson **TX** 必须接到 ESP **GPIO4**（`RCP RX`）
- Jetson **RX** 必须来自 ESP **GPIO5**（`RCP TX`）

### 端口命名注意事项

不要仅因为某些上游 OTBR 示例中出现 `/dev/ttyACM0` 就将其硬编码。

在 Espressif 开发板上：

- 一块板子可能显示为 `/dev/ttyUSB0`
- 另一块可能显示为 `/dev/ttyACM0`

使用你机器上属于 **ESP32-C6 #2** 的实际端口。

若你使用带 CP210x 桥接的 Espressif 板，`/dev/ttyUSB0` 很常见。

该 USB 串口用于：

- 烧录 `ot_rcp` 镜像
- 读取 ESP 启动与日志输出

它**并非** Jetson 后续由 `otbr-agent` 使用的真实 UART 传输通道。

---

## 7. 在 Jetson 上启用 UART1 并接线到 RCP

Jetson Orin Nano 40 针排针将 **UART1** 引出为：

| 功能 | Jetson 引脚 | Linux 设备 | 方向 |
|---|---:|---|---|
| UART1_TXD | `8` | 当前镜像上的 `/dev/ttyTHS1` | Jetson -> ESP |
| UART1_RXD | `10` | 当前镜像上的 `/dev/ttyTHS1` | ESP -> Jetson |
| Ground | `6` 或 `9` 或 `14` | -- | 共地参考 |

本指南中 ESP32-C6 RCP 侧：

| 功能 | ESP32-C6 引脚 | 方向 |
|---|---:|---|
| RCP RX | `GPIO4` | Jetson TX -> ESP RX |
| RCP TX | `GPIO5` | ESP TX -> Jetson RX |
| Ground | `GND` | 共地参考 |

因此确切接线为：

```text
Jetson pin 8  (UART1_TXD) -> ESP32-C6 GPIO4
Jetson pin 10 (UART1_RXD) <- ESP32-C6 GPIO5
Jetson GND                -> ESP32-C6 GND
```

### Jetson UART 前置要求

在 Jetson 上，通常阻碍使用排针 UART 的主要因素是 `nvgetty`，它会占用用户 UART 设备。

先检查该设备：

```bash
ls -l /dev/ttyTHS1
```

然后若串口 getty 处于活动状态，则禁用它：

```bash
sudo systemctl stop nvgetty
sudo systemctl disable nvgetty
```

验证它不再处于活动状态：

```bash
systemctl status nvgetty
```

### Jetson UART 基本配置

将该端口配置为 RCP 主机链路要使用的波特率：

```bash
sudo stty -F /dev/ttyTHS1 460800 cs8 -cstopb -parenb raw -echo
```

本指南使用 `460800`，因为这是 Espressif 示例中常见的 `ot_rcp` / OTBR 搭配。若之后在 OpenThread 配置中选择不同波特率，需保持 Jetson 与 ESP 两侧一致。

### 电气警告

Jetson 40 针 UART **仅支持 3.3 V 逻辑电平**。

不要将其连接到：

- RS-232 电压电平
- 5 V UART
- 任何驱动超出 3.3 V 逻辑电平的外部适配器

对于 ESP32-C6 开发板，直接 3.3 V UART 接线没问题。

### 可选的连通性测试

在接入 ESP 板之前，可以先做 Jetson 回环测试：

1. 临时将 **pin 8** 短接到 **pin 10**
2. 运行：

```bash
sudo cat /dev/ttyTHS1 &
echo "JETSON_UART_OK" | sudo tee /dev/ttyTHS1
```

若文本被回显，则 Jetson 侧 UART 通路正常。

---


<details>
<summary>English original</summary>

**5. The official model you are implementing**

OpenThread's **RCP design** means:

- the OpenThread core lives on the **host processor**
- the radio chip runs a minimal controller firmware
- host and controller talk through **Spinel**

On Jetson, that means:

- **Jetson** runs `otbr-agent` or `ot-daemon`
- **ESP32-C6 #2** runs the **`ot_rcp`** firmware from ESP-IDF

OpenThread's own coprocessor docs describe this as an **RCP design**, and OT Daemon is explicitly documented as the POSIX-side component for RCP setups.

For the backbone / infrastructure side, you have multiple valid choices on Jetson:

- `l4tbr0` if you want to use the Jetson USB device networking path to a host PC
- `wlan0` if the Jetson reaches the network through your first ESP32-C6 and ESP-Hosted
- `enP8p1s0` if you later use Ethernet as the OTBR backbone

On your current setup, **`l4tbr0` is the right choice for USB networking**, not `usb0` or `usb1`, because those interfaces are members of the `l4tbr0` bridge.

---

**6. Build and flash the ESP32-C6 RCP firmware**

Use the **ESP-IDF `ot_rcp` example** for the second ESP32-C6.

On your Linux build host:

```bash
cd $IDF_PATH/examples/openthread/ot_rcp

# Select the chip once
idf.py set-target esp32c6

# Optional: inspect settings
idf.py menuconfig

# Build
idf.py build
```

Flash it to the second ESP32-C6:

```bash
# Use the actual serial port for your board
# On many ESP32-C6 dev boards with CP210x this is /dev/ttyUSB0
idf.py -p /dev/ttyUSB0 flash monitor
```

**Important `menuconfig` choices for this guide**

If the `ot_rcp` menu shows:

```text
[*] Configure RCP UART pin manually
(4)     The number of RX pin
(5)     The number of TX pin
```

that matches the direct Jetson UART wiring documented below.

For this guide:

- keep **`Configure RCP UART pin manually`** enabled
- set **RCP RX pin = `4`**
- set **RCP TX pin = `5`**
- leave **external coexist wire** disabled for first bring-up

That means:

- Jetson **TX** must go to ESP **GPIO4** (`RCP RX`)
- Jetson **RX** must come from ESP **GPIO5** (`RCP TX`)

**Port naming note**

Do not hardcode `/dev/ttyACM0` just because some upstream OTBR examples show it.

On Espressif dev boards:

- one board may show up as `/dev/ttyUSB0`
- another may show up as `/dev/ttyACM0`

Use the actual port that belongs to **ESP32-C6 #2** on your machine.

If you are using an Espressif board with a CP210x bridge, `/dev/ttyUSB0` is common.

This USB serial port is for:

- flashing the `ot_rcp` image
- reading ESP boot and log output

It is **not** the same thing as the Jetson's real UART transport used later by `otbr-agent`.

---

**7. Enable UART1 on the Jetson and wire it to the RCP**

The Jetson Orin Nano 40-pin header exposes **UART1** as:

| Function | Jetson pin | Linux device | Direction |
|---|---:|---|---|
| UART1_TXD | `8` | `/dev/ttyTHS1` on your current image | Jetson -> ESP |
| UART1_RXD | `10` | `/dev/ttyTHS1` on your current image | ESP -> Jetson |
| Ground | `6` or `9` or `14` | -- | common reference |

For the ESP32-C6 RCP side in this guide:

| Function | ESP32-C6 pin | Direction |
|---|---:|---|
| RCP RX | `GPIO4` | Jetson TX -> ESP RX |
| RCP TX | `GPIO5` | ESP TX -> Jetson RX |
| Ground | `GND` | common reference |

So the exact wiring is:

```text
Jetson pin 8  (UART1_TXD) -> ESP32-C6 GPIO4
Jetson pin 10 (UART1_RXD) <- ESP32-C6 GPIO5
Jetson GND                -> ESP32-C6 GND
```

**Jetson UART prerequisites**

On Jetson, the main thing that usually blocks use of the header UART is `nvgetty`, which can claim the user UART device.

Check the device first:

```bash
ls -l /dev/ttyTHS1
```

Then disable the serial getty if it is active:

```bash
sudo systemctl stop nvgetty
sudo systemctl disable nvgetty
```

Verify it is no longer active:

```bash
systemctl status nvgetty
```

**Basic Jetson UART configuration**

Configure the port to the baud rate you intend to use with the RCP host link:

```bash
sudo stty -F /dev/ttyTHS1 460800 cs8 -cstopb -parenb raw -echo
```

This guide uses `460800` because that is a common `ot_rcp` / OTBR pairing on Espressif examples. If you later choose a different baud in your OpenThread configuration, keep the Jetson and ESP sides aligned.

**Electrical warning**

The Jetson 40-pin UART is **3.3 V logic only**.

Do not connect it to:

- RS-232 voltage levels
- 5 V UART
- any external adapter that drives outside 3.3 V logic levels

For an ESP32-C6 dev board, direct 3.3 V UART wiring is fine.

**Optional sanity test**

Before involving the ESP board, you can do a Jetson loopback test:

1. temporarily short **pin 8** to **pin 10**
2. run:

```bash
sudo cat /dev/ttyTHS1 &
echo "JETSON_UART_OK" | sudo tee /dev/ttyTHS1
```

If the text echoes back, the Jetson side UART path is alive.

---

</details>

## 8. 在 Jetson 上安装 OTBR

如果要在 Jetson 上运行真正的 Thread Border Router，使用 **`ot-br-posix`**。

```bash
sudo apt update
sudo apt install -y git

git clone --depth=1 https://github.com/openthread/ot-br-posix
cd ot-br-posix

./script/bootstrap
INFRA_IF_NAME=l4tbr0 ./script/setup
```

为什么在你的当前 Jetson 上是 `INFRA_IF_NAME=l4tbr0`：

- OTBR 需要 **backbone / infrastructure interface**
- 你的 Jetson 当前将 USB 网络路径暴露为 bridge **`l4tbr0`**
- `usb0` 和 `usb1` 是该 bridge 的成员，因此 `l4tbr0` 是真正的 backbone interface

如果之后想让 backbone 为：

- ESP-Hosted Wi-Fi，使用 `INFRA_IF_NAME=wlan0`
- Ethernet，在该链路启用后使用 `INFRA_IF_NAME=enP8p1s0`

安装后：

```bash
sudo service otbr-agent status
```

官方 OTBR native 安装指南从高层展示了 agent 像这样运行：

```text
/usr/sbin/otbr-agent -I wpan0 -B wlan0 spinel+hdlc+uart:///dev/ttyACM0
```

对于你的设置，重要的部分是 **`-B <backbone>`** 选择。在你当前的 Jetson USB 网络路径上，该 backbone 应为 **`l4tbr0`**。

---

## 9. 让 OTBR 指向真正的 ESP32-C6 RCP 端口

OTBR 使用 `/etc/default/otbr-agent` 定义 Radio URL。

编辑它：

```bash
sudoedit /etc/default/otbr-agent
```

设置 RCP 路径和 baud rate。对于本指南中的 **direct Jetson header UART** 路径：

```bash
OTBR_AGENT_OPTS="-I wpan0 -B l4tbr0 spinel+hdlc+uart:///dev/ttyTHS1?uart-baudrate=460800"
```

如果故意改为选择 USB-serial 路径，将 `/dev/ttyTHS1` 替换为真实的 USB 串行设备，例如 `/dev/ttyUSB0` 或 `/dev/ttyACM0`。

如果之后将 OTBR backbone 移到 Wi-Fi，将 `-B l4tbr0` 改为 `-B wlan0`。

然后重启服务：

```bash
sudo systemctl restart otbr-agent
sudo systemctl status otbr-agent
sudo journalctl -u otbr-agent -n 100 --no-pager
```

为什么是 `460800`：

- Espressif 的 Thread BR FAQ 指出 OTBR 通常默认使用 `115200`
- 但 Espressif `ot_rcp` 示例通常使用 **`460800`**
- 如果 baud rate 错误，即使串行设备存在，host/RCP 通信也会失败

---

## 10. 先验证 host/RCP 链路

在组建 Thread 网络之前，证明 Jetson 能可靠地与 RCP 通信。

检查预期的 Jetson UART 设备是否存在：

```bash
ls -l /dev/ttyTHS1
```

检查 OTBR 服务健康状况：

```bash
sudo service otbr-agent status
sudo journalctl -u otbr-agent -n 100 --no-pager
ip link show wpan0
```

成功的样子：

- `otbr-agent` 是 `active (running)`
- `wpan0` 存在
- 日志不显示重复的 Spinel 超时

如果 `wpan0` 未出现，就此停止，先修复 RCP 链路，再尝试组建网络。

---

## 11. 在 Jetson 上组建 Thread 网络

一旦 OTBR 健康，在 Jetson 上使用 `ot-ctl`。

```bash
sudo ot-ctl state
sudo ot-ctl dataset init new
sudo ot-ctl dataset commit active
sudo ot-ctl ifconfig up
sudo ot-ctl thread start
sudo ot-ctl state
```

典型流程：

- 先 `disabled`
- 然后 `detached`
- 最后，如果这是新网络中的第一个 Thread 节点，则 `leader`

有用的后续命令：

```bash
sudo ot-ctl dataset active
sudo ot-ctl ipaddr
sudo ot-ctl netdata show
```

此时，Jetson 不再只是“连接到一个 RCP”。它正在主动运行 OpenThread host stack。

### 如何读取真实的 OTBR 和 `ot-ctl` 输出

本项目中最有用的习惯是按 layer 读取 host 侧状态，而不是寻找一行神奇的成功信息。

对于这个已验证的 Jetson 设置，第一行重要信息是：

```text
Radio Co-processor version: openthread-esp32/... esp32c6
```

该行表示 Linux host 成功通过以下方式与 ESP32-C6 通信：

* `spinel+hdlc+uart`
* `/dev/ttyTHS1`
* `460800` baud

如果该行缺失，且 `wpan0` 从未出现，问题仍在传输层：UART 设备错误、baud 错误、接线错误、RCP 镜像损坏或 RCP 复位。

一旦链路健康，下一个有用的快照通常是：

```bash
sudo ot-ctl extaddr
sudo ot-ctl rloc16
sudo ot-ctl ipaddr
sudo ot-ctl state
```

例如，一个真实会话可能显示：

```text
extaddr: fac5eb4acbada19e
rloc16: a400
ipaddr:
  fd3f:d825:5faf:9782:0:ff:fe00:a400
  fd3f:d825:5faf:9782:8b89:772a:39cd:737a
  fe80::f8c5:eb4a:cbad:a19e
state: detached
```

像这样读取该输出：

* `extaddr` 是 64 位 IEEE 802.15.4 无线身份。
* `rloc16` 是在 partition 逻辑内分配的 Thread mesh locator。
* `fd...ff:fe00:a400` 是从 mesh-local 前缀和 `rloc16` 派生的 RLOC IPv6 地址。
* `fd...8b89:...` 是 Mesh-Local EID，一种更稳定的 mesh-local 身份。
* `fe80::...` 是正常的 IPv6 link-local 地址。

这是关键细微之处：**你可以拥有有效的 Thread 地址，却仍然 `detached`**。这意味着 dataset 存在且 host/RCP stack 存活，但节点尚未完成对 partition 的 attachment。


<details>
<summary>English original</summary>

**8. Install OTBR on the Jetson**

If you want a real Thread Border Router on the Jetson, use **`ot-br-posix`**.

```bash
sudo apt update
sudo apt install -y git

git clone --depth=1 https://github.com/openthread/ot-br-posix
cd ot-br-posix

./script/bootstrap
INFRA_IF_NAME=l4tbr0 ./script/setup
```

Why `INFRA_IF_NAME=l4tbr0` on your current Jetson:

- OTBR needs a **backbone / infrastructure interface**
- your Jetson currently exposes the USB networking path as the bridge **`l4tbr0`**
- `usb0` and `usb1` are members of that bridge, so `l4tbr0` is the real backbone interface

If you later want the backbone to be:

- ESP-Hosted Wi-Fi, use `INFRA_IF_NAME=wlan0`
- Ethernet, use `INFRA_IF_NAME=enP8p1s0` once that link is active

After installation:

```bash
sudo service otbr-agent status
```

The official OTBR native install guide shows the agent running like this at a high level:

```text
/usr/sbin/otbr-agent -I wpan0 -B wlan0 spinel+hdlc+uart:///dev/ttyACM0
```

For your setup, the important part is the **`-B <backbone>`** choice. On your current Jetson USB networking path, that backbone should be **`l4tbr0`**.

---

**9. Point OTBR at the real ESP32-C6 RCP port**

OTBR uses `/etc/default/otbr-agent` to define the Radio URL.

Edit it:

```bash
sudoedit /etc/default/otbr-agent
```

Set the RCP path and baud rate. For the **direct Jetson header UART** path in this guide:

```bash
OTBR_AGENT_OPTS="-I wpan0 -B l4tbr0 spinel+hdlc+uart:///dev/ttyTHS1?uart-baudrate=460800"
```

If you intentionally choose a USB-serial path instead, replace `/dev/ttyTHS1` with the real USB serial device such as `/dev/ttyUSB0` or `/dev/ttyACM0`.

If you later move the OTBR backbone to Wi-Fi, change `-B l4tbr0` to `-B wlan0`.

Then restart the service:

```bash
sudo systemctl restart otbr-agent
sudo systemctl status otbr-agent
sudo journalctl -u otbr-agent -n 100 --no-pager
```

Why `460800`:

- Espressif's Thread BR FAQ notes that OTBR often defaults to `115200`
- but the Espressif `ot_rcp` example commonly uses **`460800`**
- if the baud rate is wrong, host/RCP communication will fail even though the serial device exists

---

**10. Validate the host/RCP link first**

Before forming a Thread network, prove that the Jetson can talk to the RCP reliably.

Check that the expected Jetson UART device exists:

```bash
ls -l /dev/ttyTHS1
```

Check OTBR service health:

```bash
sudo service otbr-agent status
sudo journalctl -u otbr-agent -n 100 --no-pager
ip link show wpan0
```

What success looks like:

- `otbr-agent` is `active (running)`
- `wpan0` exists
- logs do not show repeated Spinel timeouts

If `wpan0` does not appear, stop there and fix the RCP link before trying to form a network.

---

**11. Form a Thread network on the Jetson**

Once OTBR is healthy, use `ot-ctl` on the Jetson.

```bash
sudo ot-ctl state
sudo ot-ctl dataset init new
sudo ot-ctl dataset commit active
sudo ot-ctl ifconfig up
sudo ot-ctl thread start
sudo ot-ctl state
```

Typical progression:

- first `disabled`
- then `detached`
- finally `leader` if this is the first Thread node in the new network

Useful follow-up commands:

```bash
sudo ot-ctl dataset active
sudo ot-ctl ipaddr
sudo ot-ctl netdata show
```

At this point, the Jetson is no longer just “connected to an RCP.” It is actively running the OpenThread host stack.

**How to read the real OTBR and `ot-ctl` output**

The most useful habit in this project is to read the host-side state in layers instead of looking for one magic success line.

For this validated Jetson setup, the first important line is:

```text
Radio Co-processor version: openthread-esp32/... esp32c6
```

That line means the Linux host successfully talked to the ESP32-C6 over:

* `spinel+hdlc+uart`
* `/dev/ttyTHS1`
* `460800` baud

If that line is missing and `wpan0` never appears, the problem is still in the transport layer: wrong UART device, wrong baud, wrong wiring, bad RCP image, or an RCP reset.

Once the link is healthy, the next useful snapshot is usually:

```bash
sudo ot-ctl extaddr
sudo ot-ctl rloc16
sudo ot-ctl ipaddr
sudo ot-ctl state
```

For example, a real session may show:

```text
extaddr: fac5eb4acbada19e
rloc16: a400
ipaddr:
  fd3f:d825:5faf:9782:0:ff:fe00:a400
  fd3f:d825:5faf:9782:8b89:772a:39cd:737a
  fe80::f8c5:eb4a:cbad:a19e
state: detached
```

Read that output like this:

* `extaddr` is the 64-bit IEEE 802.15.4 radio identity.
* `rloc16` is the Thread mesh locator assigned inside the partition logic.
* `fd...ff:fe00:a400` is the RLOC IPv6 address derived from the mesh-local prefix and `rloc16`.
* `fd...8b89:...` is the Mesh-Local EID, a more stable mesh-local identity.
* `fe80::...` is the normal IPv6 link-local address.

This is the key nuance: **you can have valid Thread addresses and still be `detached`**. That means the dataset is present and the host/RCP stack is alive, but the node has not yet completed attachment to a partition.

</details>

### 常见 OTBR 日志行的含义

以下日志行是最需要能认出来的：

```text
Mle-----------: Send Link Request (ff02::2)
MeshForwarder-: Sent IPv6 UDP msg ... dst:[ff02::2]:19788
Settings------: Read NetworkInfo {rloc:0xa400, extaddr:..., role:leader, ...}
BorderAgent---: Registering service OpenThread BorderRouter #A19E _meshcop._udp
```

如何解读它们：

* `Mle-----------` 表示 Thread 控制面正在主动尝试发现或接入。
* `MeshForwarder-` 证明射频链路正在收发真实的 Thread 流量。
* `Settings------` 表明正在读取或写入持久化的 OpenThread 状态。
* `BorderAgent--- ... _meshcop._udp` 表明面向 commissioning 的 Border Agent 服务已由 OTBR 注册。

有一行很微妙，让很多人困惑：

```text
Settings------: Read NetworkInfo { ... role:leader, ... }
```

这**并不**证明该节点当前是 leader。它只说明协议栈读取了先前保存的网络状态，而该状态中记录过 leader 角色。实时角色仍要看 `sudo ot-ctl state`。

### 为什么 `detached` 仍可能是健康的中间状态

在全新的实验室环境中，预期的状态演进是：

* `disabled`
* `detached`
* 单节点分区时为 `leader`，加入现有 mesh 时为 `router` / `child`

因此 `detached` 并不等同于“UART 坏了”。在实际日志中，它与以下内容同时出现：

* 一条真实的 `Radio Co-processor version` 行
* `wpan0` 创建
* 有效的 `extaddr`、`rloc16` 和 `ipaddr` 输出
* 日志中的 MLE attach 尝试

这一组合说明主机/RCP 链路已经在工作。剩下的工作在于 **Thread 接入与分区形成**，而不是底层串口 bring-up（上电点亮/调通）。

### 如何解读 attach 尝试日志

像这样的行：

```text
Attach attempt 8, AnyPartition
Send Parent Request to routers
Send Parent Request to routers and REEDs
Attach attempt 8 unsuccessful, will try again in 32.128 seconds
```

表示节点存活且行为像一个 Thread 节点，但尚未完成 attach 过程。这是协议状态问题，并不自动是传输问题。

如果看到的是：

```text
Wait for response timeout
no response from RCP during initialization
```

那就要回到主机/RCP 路径上排查，应先调试 UART、波特率、RCP 镜像或板级稳定性。

### 会话 socket 与重启消息

还有两行容易被过度解读：

```text
P-Daemon------: Session socket is ready
P-Daemon------: Daemon read: Connection reset by peer
```

在正常使用中，这些通常只说明 `ot-ctl` 连接到了 OTBR 的控制 socket 然后退出。它们并不自动代表射频故障。

同样，重启 `otbr-agent` 之后，看到以下情况是正常的：

* `wpan0` 存在但仍是 `state DOWN`
* `sudo ot-ctl state` 返回 `disabled`

直到你运行：

```bash
sudo ot-ctl dataset init new
sudo ot-ctl dataset commit active
sudo ot-ctl ifconfig up
sudo ot-ctl thread start
```

对于这个 Jetson 项目，已知可用的主机侧基线是：

* RCP 传输：`spinel+hdlc+uart`
* 串口设备：`/dev/ttyTHS1`
* 波特率：`460800`
* OTBR 骨干网：`l4tbr0`


<details>
<summary>English original</summary>

**What the common OTBR log lines mean**

These log lines are the most important ones to recognize:

```text
Mle-----------: Send Link Request (ff02::2)
MeshForwarder-: Sent IPv6 UDP msg ... dst:[ff02::2]:19788
Settings------: Read NetworkInfo {rloc:0xa400, extaddr:..., role:leader, ...}
BorderAgent---: Registering service OpenThread BorderRouter #A19E _meshcop._udp
```

How to interpret them:

* `Mle-----------` means the Thread control plane is actively trying to discover or attach.
* `MeshForwarder-` proves the radio path is transmitting real Thread traffic.
* `Settings------` shows persisted OpenThread state being read or written.
* `BorderAgent--- ... _meshcop._udp` shows the commissioning-facing Border Agent service being registered by OTBR.

One subtle line confuses many people:

```text
Settings------: Read NetworkInfo { ... role:leader, ... }
```

This does **not** prove the node is currently leader. It only means the stack read previously saved network state that remembered a leader role. The live role still comes from `sudo ot-ctl state`.

**Why `detached` can still be a healthy intermediate state**

In a fresh lab setup, the expected state progression is:

* `disabled`
* `detached`
* `leader` for a one-node partition, or `router` / `child` when joining an existing mesh

So `detached` is not the same as "the UART is broken." In your actual logs, it appeared together with:

* a real `Radio Co-processor version` line
* `wpan0` creation
* valid `extaddr`, `rloc16`, and `ipaddr` output
* MLE attach attempts in the logs

That combination means the host/RCP link is already working. The remaining work is in **Thread attachment and partition formation**, not low-level serial bring-up.

**How to read the attach-attempt logs**

Lines like these:

```text
Attach attempt 8, AnyPartition
Send Parent Request to routers
Send Parent Request to routers and REEDs
Attach attempt 8 unsuccessful, will try again in 32.128 seconds
```

mean the node is alive and behaving like a Thread node, but it has not yet completed the attach process. This is a protocol-state problem, not automatically a transport problem.

If you instead see:

```text
Wait for response timeout
no response from RCP during initialization
```

that points back to the host/RCP path and you should debug the UART, baud, RCP image, or board stability first.

**Session-socket and restart messages**

Two more lines are easy to over-interpret:

```text
P-Daemon------: Session socket is ready
P-Daemon------: Daemon read: Connection reset by peer
```

In normal use, those often just mean that `ot-ctl` connected to OTBR's control socket and then exited. They are not automatically a radio failure.

Similarly, after restarting `otbr-agent`, it is normal to see:

* `wpan0` exist but still be `state DOWN`
* `sudo ot-ctl state` return `disabled`

until you run:

```bash
sudo ot-ctl dataset init new
sudo ot-ctl dataset commit active
sudo ot-ctl ifconfig up
sudo ot-ctl thread start
```

For this Jetson project, the known-good host-side baseline is:

* RCP transport: `spinel+hdlc+uart`
* serial device: `/dev/ttyTHS1`
* baud: `460800`
* OTBR backbone: `l4tbr0`

</details>

### 此 Jetson 镜像上的当前已验证状态

此时，项目当前已验证的状态是：

* ESP32-C6 **`ot_rcp`** 镜像可工作
* Jetson 可以通过 **Spinel + UART** 与 RCP 通信
* 已验证的 host 路径是：
  * `/dev/ttyTHS1`
  * `460800`
  * `l4tbr0`
* OTBR 创建 `wpan0`
* `ot-ctl` 可以管理 Thread dataset 与接口
* 节点可以从 `disabled` 推进到 `detached`
* 在前台运行 OTBR 时，节点最终会变为 **`leader`**

实际运行中最重要的 leader 形成日志行是：

```text
Allocate router id 41
RLOC16 fffe -> a400
Role detached -> leader
Partition ID 0x1a1d09c0
Route table ... me - leader
```

这些行证明 Thread 协议栈本身在这种硬件分工下可以正常工作。换言之，RCP 链路、dataset 处理、MLE attach 逻辑以及单节点 partition 形成在当前系统上均可工作。

然而，同一次前台运行随后遇到了：

```text
InitMulticastRouterSock() at multicast_routing.cpp:227: Protocol not available
```

这就是当前的关键限制。它意味着 OTBR 能够把 Thread 节点拉起来，甚至到达 `leader`，但在启用 Linux 侧的边界路由 / 组播路由特性时失败了。

内核检查确认了原因：

```text
# CONFIG_IP_MROUTE is not set
# CONFIG_IPV6_MROUTE is not set
```

因此当前正确的结论是：

* **Jetson 上的 Thread-over-RCP 可工作**
* **完整的 OTBR 边界路由器功能在此 Jetson 镜像上受阻**
* 阻塞点是**缺少内核组播路由支持**，不是 UART、不是 Spinel，也不是 ESP32-C6 RCP 固件

这正是本项目想要教授的那类混合软件边界问题：一个系统可以作为 Thread 节点成功工作，但作为完整的边界路由器产品仍不完整，因为 Linux 内核缺少 OTBR 所期望的某个特性。

---

## 12. 可选的更轻量路径：用 OT Daemon 替代 OTBR

如果需要一个不含完整边界路由器协议栈的 RCP host 路径，可使用 **OpenThread Daemon**。

根据 OpenThread 官方的协处理器文档：

- `ot-daemon` 是面向 RCP 设计的 POSIX 服务
- 它与 OTBR 一样使用 Spinel Radio URL

构建它：

```bash
git clone --depth=1 https://github.com/openthread/openthread
cd openthread

./script/bootstrap
./script/cmake-build posix -DOT_DAEMON=ON
```

针对真实 RCP 运行它：

```bash
./build/posix/src/posix/ot-daemon \
  'spinel+hdlc+uart:///dev/ttyTHS1?uart-baudrate=460800'
```

在另一个终端中：

```bash
./build/posix/src/posix/ot-ctl state
```

在以下情况使用此路径：

- 想先验证 host/RCP 行为
- 尚不需要完整的 Thread Border Router
- 想要比 OTBR 更小的调试面

在当前的 Jetson 镜像上，此路径尤其有用，因为内核目前缺少：

* `CONFIG_IP_MROUTE`
* `CONFIG_IPV6_MROUTE`

这意味着在你推迟为完整的 OTBR 边界路由支持而重建 JetPack 或内核期间，`ot-daemon` 是当前更干净的 Thread/RCP 工作 host 路径。

如果选择 USB 串口路径而非 Jetson 排针 UART，请替换为真实的 USB 设备路径。

---

## 13. 常见失效模式

### `otbr-agent` 正在运行，但缺少 `wpan0`

通常意味着：

- 串口设备错误
- 波特率错误
- RCP 固件实际上不是 `ot_rcp`
- `nvgetty` 仍占用着用户 UART 设备

检查：

```bash
sudo journalctl -u otbr-agent -n 100 --no-pager
```

### Spinel 超时告警

典型原因：

- UART 波特率错误
- USB 串口路径不稳定
- 重新插拔或重新烧录后串口错误
- Jetson UART1 实际上并未空出供应用使用

Espressif 的 FAQ 将这类症状描述为：

```text
Wait for response timeout
```

### `/dev/ttyTHS1` 存在，但 OTBR 仍无法与 RCP 通信

通常意味着以下之一：

- Jetson 的 `8/10` 引脚实际上未与 ESP 的 `GPIO4/5` 交叉连接
- TX/RX 接反
- `nvgetty` 仍占用该端口
- `menuconfig` 中 ESP 侧的 UART 引脚与实际接线不符

对本指南而言，正确的直连接线是：

```text
Jetson pin 8  -> ESP GPIO4
Jetson pin 10 <- ESP GPIO5
```

### 混淆两块 ESP32-C6 板

务必清晰区分：

- **ESP32-C6 #1** = ESP-Hosted Wi-Fi/BLE over SPI
- **ESP32-C6 #2** = Thread RCP over UART

不要将 `ot_rcp` 烧录到当前提供 `wlan0` 的那块板上。


<details>
<summary>English original</summary>

**Current validated status on this Jetson image**

At this point, the current validated status of the project is:

* the ESP32-C6 **`ot_rcp`** image is working
* Jetson can talk to the RCP over **Spinel + UART**
* the validated host path is:
  * `/dev/ttyTHS1`
  * `460800`
  * `l4tbr0`
* OTBR creates `wpan0`
* `ot-ctl` can manage the Thread dataset and interface
* the node can progress from `disabled` to `detached`
* in a foreground OTBR run, the node eventually becomes **`leader`**

The most important leader-formation lines from the real run were:

```text
Allocate router id 41
RLOC16 fffe -> a400
Role detached -> leader
Partition ID 0x1a1d09c0
Route table ... me - leader
```

Those lines prove that the Thread stack itself is functioning correctly on this hardware split. In other words, the RCP link, dataset handling, MLE attach logic, and one-node partition formation all work on the current system.

However, the same foreground run later hit:

```text
InitMulticastRouterSock() at multicast_routing.cpp:227: Protocol not available
```

That is the key current limitation. It means OTBR was able to bring up the Thread node and even reach `leader`, but then failed while enabling Linux-side border-routing / multicast-routing features.

The kernel check confirmed the reason:

```text
# CONFIG_IP_MROUTE is not set
# CONFIG_IPV6_MROUTE is not set
```

So the correct current conclusion is:

* **Thread-over-RCP on Jetson is working**
* **full OTBR border-router functionality is blocked on this Jetson image**
* the blocker is **missing kernel multicast-routing support**, not UART, not Spinel, and not the ESP32-C6 RCP firmware

This is exactly the kind of mixed-software boundary problem that this project is meant to teach: a system can be successful as a Thread node and still be incomplete as a full border-router product because the Linux kernel is missing one feature OTBR expects.

---

**12. Optional lighter path: OT Daemon instead of OTBR**

If you want an RCP host path without the full border-router stack, use **OpenThread Daemon**.

According to OpenThread's official coprocessor docs:

- `ot-daemon` is the POSIX service for RCP designs
- it uses a Spinel Radio URL just like OTBR

Build it:

```bash
git clone --depth=1 https://github.com/openthread/openthread
cd openthread

./script/bootstrap
./script/cmake-build posix -DOT_DAEMON=ON
```

Run it against the real RCP:

```bash
./build/posix/src/posix/ot-daemon \
  'spinel+hdlc+uart:///dev/ttyTHS1?uart-baudrate=460800'
```

In another terminal:

```bash
./build/posix/src/posix/ot-ctl state
```

Use this path if:

- you want to validate host/RCP behavior first
- you do not yet need a full Thread Border Router
- you want a smaller debugging surface than OTBR

On your current Jetson image, this path is especially useful because the kernel currently lacks:

* `CONFIG_IP_MROUTE`
* `CONFIG_IPV6_MROUTE`

That means `ot-daemon` is the cleaner current host path for Thread/RCP work while you defer a JetPack or kernel rebuild for full OTBR border-routing support.

If you choose the USB-serial path instead of Jetson header UART, substitute the real USB device path.

---

**13. Common failure modes**

**`otbr-agent` is running, but `wpan0` is missing**

Usually means:

- wrong serial device
- wrong baud rate
- RCP firmware is not actually `ot_rcp`
- `nvgetty` is still owning the user UART device

Check:

```bash
sudo journalctl -u otbr-agent -n 100 --no-pager
```

**Spinel timeout warnings**

Typical causes:

- wrong UART baud rate
- unstable USB serial path
- wrong serial port after replug or reflashing
- Jetson UART1 is not actually free for application use

Espressif's FAQ shows this family of symptom as:

```text
Wait for response timeout
```

**`/dev/ttyTHS1` exists, but OTBR still cannot talk to the RCP**

Usually means one of:

- Jetson pin `8/10` are not actually cross-wired to ESP `GPIO4/5`
- TX/RX are swapped incorrectly
- `nvgetty` still owns the port
- the ESP side UART pins in `menuconfig` do not match the actual wiring

For this guide, the correct direct wiring is:

```text
Jetson pin 8  -> ESP GPIO4
Jetson pin 10 <- ESP GPIO5
```

**Confusing the two ESP32-C6 boards**

Keep them clearly separated:

- **ESP32-C6 #1** = ESP-Hosted Wi-Fi/BLE over SPI
- **ESP32-C6 #2** = Thread RCP over UART

Do not flash `ot_rcp` onto the board currently providing `wlan0`.

</details>

### OTBR 安装正常但路由不工作

如果 OTBR 能与 RCP 通信、形成 `wpan0`，甚至到达 `leader`，但随后退出并给出类似这样的消息：

```text
InitMulticastRouterSock() ... Protocol not available
```

问题就不再是 ESP32-C6 或 UART 链路了。通常是主机 kernel 的限制。

在本项目已验证的 Jetson 运行中，确切原因是：

```text
# CONFIG_IP_MROUTE is not set
# CONFIG_IPV6_MROUTE is not set
```

在 Jetson 上用以下方式检查：

```bash
zcat /proc/config.gz | grep -E 'CONFIG_IPV6_MROUTE|CONFIG_IP_MROUTE'
```

或者，如果 `/proc/config.gz` 不可用：

```bash
grep -E 'CONFIG_IPV6_MROUTE|CONFIG_IP_MROUTE' /boot/config-$(uname -r)
```

如果这些选项缺失，实际可选的做法是：

* 当前 Thread/RCP 工作使用 **`ot-daemon`**
* 之后启用多播路由支持，重建或替换 kernel / JetPack 镜像

还要确认骨干接口正确：

- 如果 Jetson 通过 ESP-Hosted 接入网络，则为 `wlan0`
- 仅当有意将 Ethernet 用作 Thread BR 骨干时，才用 `eth0`

---

## 14. 首次 bring-up（上电点亮/调通）之后合理的下一步

- 保留当前镜像用于 RCP 和 Thread 学习，在不需要完整 OTBR 边界路由时使用 `ot-daemon`
- 之后在启用 `CONFIG_IP_MROUTE` 和 `CONFIG_IPV6_MROUTE` 的情况下重建 JetPack 或 Jetson kernel
- 添加第二个 Thread 设备并让它加入网络
- 验证 `ipaddr`、`ping` 以及 Thread 上的服务发现
- 先让 RCP 走 UART，之后仅在需要时再评估 SPI
- 如果之后要用 Matter，就保留这个 Jetson + RCP 分工并在其上构建

---

## 15. 参考资料

### 官方上游参考资料

- [OpenThread 协处理器设计](https://openthread.io/platforms/co-processor)
- [OpenThread Daemon（RCP 主机模式）](https://openthread.io/platforms/co-processor/ot-daemon)
- [OpenThread Border Router 原生安装](https://openthread.io/guides/border-router/build-native)
- [ESP32-C6 上的 ESP-IDF OpenThread](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/openthread.html)
- [ESP-IDF Thread / esp_openthread API 参考](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_openthread.html)
- [ESP Thread Border Router SDK](https://docs.espressif.com/projects/esp-thread-br/en/latest/)
- [ESP Thread BR FAQ：OTBR 与 `ot_rcp`](https://docs.espressif.com/projects/esp-thread-br/en/latest/qa.html#using-ot-rcp-with-otbr-ot-br-posix)
- [ESP32-C6 射频共存指南](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/coexist.html)

### 本地路线图参考资料

- [Jetson Orin Nano 上通过 SPI 的 ESP32-C6 ESP-Hosted](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md)
- [网络与连接中心](Guide.md)
- [Orin Nano GPIO/SPI/I2C/CAN 深入解析](../../01-Nvidia-Jetson平台/05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)


<details>
<summary>English original</summary>

**OTBR installs correctly but routing does not work**

If OTBR can talk to the RCP, forms `wpan0`, and even reaches `leader`, but then exits with a message like:

```text
InitMulticastRouterSock() ... Protocol not available
```

the problem is no longer the ESP32-C6 or the UART link. It is usually a host-kernel limitation.

On the validated Jetson run for this project, the exact cause was:

```text
# CONFIG_IP_MROUTE is not set
# CONFIG_IPV6_MROUTE is not set
```

Check on the Jetson with:

```bash
zcat /proc/config.gz | grep -E 'CONFIG_IPV6_MROUTE|CONFIG_IP_MROUTE'
```

or, if `/proc/config.gz` is unavailable:

```bash
grep -E 'CONFIG_IPV6_MROUTE|CONFIG_IP_MROUTE' /boot/config-$(uname -r)
```

If those options are missing, the practical choices are:

* use **`ot-daemon`** for current Thread/RCP work
* rebuild or replace the kernel / JetPack image later with multicast-routing support enabled

Also make sure the backbone interface is right:

- `wlan0` if Jetson reaches the network through ESP-Hosted
- `eth0` only if you intentionally use Ethernet as the Thread BR backbone

---

**14. Good next steps after first bring-up**

- keep this current image for RCP and Thread learning, using `ot-daemon` when you do not need full OTBR border routing
- rebuild JetPack or the Jetson kernel later with `CONFIG_IP_MROUTE` and `CONFIG_IPV6_MROUTE` enabled
- add a second Thread device and have it join the network
- validate `ipaddr`, `ping`, and service discovery over Thread
- keep the RCP on UART first, then evaluate SPI later only if needed
- if you want Matter later, keep this Jetson + RCP split and build on top of it

---

**15. References**

**Official upstream references**

- [OpenThread co-processor designs](https://openthread.io/platforms/co-processor)
- [OpenThread Daemon (RCP host mode)](https://openthread.io/platforms/co-processor/ot-daemon)
- [OpenThread Border Router native install](https://openthread.io/guides/border-router/build-native)
- [ESP-IDF OpenThread on ESP32-C6](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/openthread.html)
- [ESP-IDF Thread / esp_openthread API reference](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_openthread.html)
- [ESP Thread Border Router SDK](https://docs.espressif.com/projects/esp-thread-br/en/latest/)
- [ESP Thread BR FAQ: OTBR with `ot_rcp`](https://docs.espressif.com/projects/esp-thread-br/en/latest/qa.html#using-ot-rcp-with-otbr-ot-br-posix)
- [ESP32-C6 RF coexistence guidance](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/coexist.html)

**Local roadmap references**

- [ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md)
- [Network and Connectivity hub](Guide.md)
- [Orin Nano GPIO/SPI/I2C/CAN deep-dive](../../01-Nvidia-Jetson平台/05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/2. Network and Connectivity/ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/2.%20Network%20and%20Connectivity/ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
