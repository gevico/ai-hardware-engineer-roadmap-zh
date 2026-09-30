---
title: ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano - 项目指南
description: ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano - 项目指南
---

# ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano - 项目指南

> **目标：** 在 **Jetson Orin Nano 8GB Developer Kit** 上，用 **ESP-Hosted-NG over SPI** 将 **ESP32-C6** bring-up（上电点亮/调通）为外部 Wi-Fi 协处理器，让 Jetson 通过 40-pin header 获得一个正常的 Linux 无线接口。

**枢纽：** [网络与连接](Guide.md)  
**相关本地指南：** [外设访问](../01-外设访问/Guide.md) · [Orin Nano GPIO/SPI/I2C/CAN 深入解析](../../01-Nvidia-Jetson平台/05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)

---


## 1. 这个项目为什么重要

Jetson Orin Nano developer kit 不带板载 Wi-Fi。在许多 edge 构建中，你会用 USB dongle 或 M.2 Key E 卡解决这个问题，但 **ESP32-C6 + ESP-Hosted-NG** 这条路径展示了一种不同、也更贴近嵌入式的模式：

- Jetson 仍是主 Linux 应用处理器
- ESP32-C6 充当专用的连接外设
- SPI 和 GPIO 成为系统集成工作的一部分
- 最终结果在 Jetson 上仍然表现为一个标准的 Linux 网络接口

对于想了解 Linux、GPIO 中断、板级连线和无线连接如何在 AI edge 设备上协同工作的人来说，这是一个很好的项目。

**重要的范围说明：** 这不仅仅是刷个固件。Espressif 把 Raspberry Pi 记录为参考的 Linux SPI host。在 Jetson 上，**ESP 固件仍走上游**，但 **Linux host 集成需要一小块平台移植**，用于 SPI 总线选择、GPIO 映射和驱动加载。

---

## 2. 目标架构

```text
Jetson Orin Nano 8GB dev kit
  |
  |-- SPI1: MOSI / MISO / SCLK / CS0
  |-- GPIO: Handshake / Data Ready / Reset
  |
  +--> ESP32-C6 running ESP-Hosted-NG (SPI peripheral)
         |
         +--> 2.4 GHz Wi-Fi connection to the network

Linux on Jetson then uses wlanX through the normal stack:
NetworkManager / nmcli / wpa_supplicant / hostapd / ip / iw
```

**目标结果**

- Jetson 通过 ESP-Hosted 暴露一个可用的 `wlanX` 接口
- `nmcli dev wifi list` 在 Jetson 上可用
- Jetson 能加入 AP 并传输正常流量
- 传输足够稳定，可以用 `ping` 和 `iperf3` 测量

---

## 3. 硬件与软件前置要求

### 硬件

- Jetson Orin Nano 8GB Developer Kit
- 带 USB、可用于烧录和供电的 ESP32-C6 开发板
- 8-10 根短跳线，最好短于 10 cm
- Jetson 与 ESP 板之间共地
- 可选逻辑分析仪，用于 SPI 时序/调试

### 软件

- Jetson 上的 JetPack 6.x / L4T 36.x
- Jetson 上已安装 Linux 内核头文件
- `gpiod`、`spi-tools`、`NetworkManager`、`iperf3`
- Espressif 的 [esp-hosted](https://github.com/espressif/esp-hosted) 仓库
- 通过 `esp_hosted_ng/esp/esp_driver/setup.sh` 装好的 ESP-IDF

### 电源与信号安全

- Jetson 40-pin header 使用 **3.3 V 逻辑电平**
- bring-up 期间让 ESP32-C6 使用自身的 USB 供电
- 两块板之间**共地**
- 早期测试时**不要**假定应该用 Jetson 的 3.3 V header 引脚为整个 ESP 开发板供电

---

## 4. 将 Jetson header 连线到 ESP32-C6

Espressif 的 SPI 设置指南记录了 ESP32-C6 在 Raspberry Pi host 上的信号角色。在 Jetson 上，保持相同的信号角色，并把它们重新映射到 Orin Nano 的 SPI1 header 和空闲 GPIO 上。

| 功能 | Jetson 引脚 | Jetson J12 标签 | ESP32-C6 引脚 | 方向 |
|----------|------------|---------------|--------------|-----------|
| MOSI | 19 | `SPI0_MOSI` | `IO7` | Jetson -> ESP |
| MISO | 21 | `SPI0_MISO` | `IO2` | ESP -> Jetson |
| SCLK | 23 | `SPI0_SCK` | `IO6` | Jetson -> ESP |
| CS0 | 24 | `SPI0_CS0` | `IO10` | Jetson -> ESP |
| Handshake | 22 | `SPI1_MISO` / legacy 全局 GPIO `471` | `IO3` | ESP -> Jetson |
| Data Ready | 15 | `GPIO12` / legacy 全局 GPIO `433` | `IO4` | ESP -> Jetson |
| Reset | 18 | `SPI1_CS0` / legacy 全局 GPIO `473` | `RST` 或 `EN` | Jetson -> ESP |
| Ground | 20 或 25 | `GND` | `GND` | 公共参考 |

### 官方参考图

Jetson Orin Nano 40-pin header 参考：

![Jetson Orin Nano 40-pin header](https://developer.download.nvidia.com/embedded/images/jetsonOrinNano/user_guide/images/jonano_cbspec_figure_3-1_white-bg.png#only-light)

ESP32-C6-DevKitC-1 引脚布局参考：

![ESP32-C6-DevKitC-1 pin layout](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/_images/esp32-c6-devkitc-1-pin-layout.png)


<details>
<summary>English original</summary>

**ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano - Project Guide**

> **Goal:** Bring up an **ESP32-C6** as an external Wi-Fi coprocessor on the **Jetson Orin Nano 8GB Developer Kit** using **ESP-Hosted-NG over SPI**, so the Jetson gets a normal Linux wireless interface driven through the 40-pin header.

**Hub:** [Network and Connectivity](Guide.md)  
**Related local guides:** [Peripheral Access](../01-外设访问/Guide.md) · [Orin Nano GPIO/SPI/I2C/CAN deep-dive](../../01-Nvidia-Jetson平台/05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)

---


**1. Why this project matters**

The Jetson Orin Nano developer kit does not include onboard Wi-Fi. In many edge builds you solve that with a USB dongle or an M.2 Key E card, but an **ESP32-C6 + ESP-Hosted-NG** path teaches a different and more embedded-friendly pattern:

- the Jetson stays the main Linux application processor
- the ESP32-C6 acts as a dedicated connectivity peripheral
- SPI and GPIOs become part of your system integration work
- the final result still looks like a standard Linux network interface on Jetson

This makes it a strong project for anyone learning how Linux, GPIO interrupts, board wiring, and wireless connectivity fit together on an AI edge device.

**Important scope note:** this is not just a firmware flash. Espressif documents Raspberry Pi as the reference Linux SPI host. On Jetson, the **ESP firmware stays upstream**, but the **Linux host integration needs a small platform port** for SPI bus selection, GPIO mapping, and driver loading.

---

**2. Target architecture**

```text
Jetson Orin Nano 8GB dev kit
  |
  |-- SPI1: MOSI / MISO / SCLK / CS0
  |-- GPIO: Handshake / Data Ready / Reset
  |
  +--> ESP32-C6 running ESP-Hosted-NG (SPI peripheral)
         |
         +--> 2.4 GHz Wi-Fi connection to the network

Linux on Jetson then uses wlanX through the normal stack:
NetworkManager / nmcli / wpa_supplicant / hostapd / ip / iw
```

**Target outcome**

- Jetson exposes a usable `wlanX` interface through ESP-Hosted
- `nmcli dev wifi list` works on the Jetson
- Jetson can join an AP and pass normal traffic
- the transport is stable enough to measure with `ping` and `iperf3`

---

**3. Hardware and software prerequisites**

**Hardware**

- Jetson Orin Nano 8GB Developer Kit
- ESP32-C6 development board with USB access for flashing and power
- 8-10 short jumper wires, ideally under 10 cm
- common ground between Jetson and ESP board
- optional logic analyzer for SPI timing/debug

**Software**

- JetPack 6.x / L4T 36.x on Jetson
- Linux kernel headers installed on Jetson
- `gpiod`, `spi-tools`, `NetworkManager`, `iperf3`
- Espressif [esp-hosted](https://github.com/espressif/esp-hosted) repository
- ESP-IDF set up through `esp_hosted_ng/esp/esp_driver/setup.sh`

**Power and signal safety**

- The Jetson 40-pin header uses **3.3 V logic**
- Keep the ESP32-C6 on its own USB power during bring-up
- Share **ground** between boards
- Do **not** assume the Jetson 3.3 V header pin should power the full ESP dev board during early testing

---

**4. Wiring the Jetson header to ESP32-C6**

Espressif's SPI setup guide documents the signal roles for ESP32-C6 on a Raspberry Pi host. On Jetson, keep the same signal roles and remap them onto the Orin Nano's SPI1 header and spare GPIOs.

| Function | Jetson pin | Jetson J12 label | ESP32-C6 pin | Direction |
|----------|------------|---------------|--------------|-----------|
| MOSI | 19 | `SPI0_MOSI` | `IO7` | Jetson -> ESP |
| MISO | 21 | `SPI0_MISO` | `IO2` | ESP -> Jetson |
| SCLK | 23 | `SPI0_SCK` | `IO6` | Jetson -> ESP |
| CS0 | 24 | `SPI0_CS0` | `IO10` | Jetson -> ESP |
| Handshake | 22 | `SPI1_MISO` / legacy global GPIO `471` | `IO3` | ESP -> Jetson |
| Data Ready | 15 | `GPIO12` / legacy global GPIO `433` | `IO4` | ESP -> Jetson |
| Reset | 18 | `SPI1_CS0` / legacy global GPIO `473` | `RST` or `EN` | Jetson -> ESP |
| Ground | 20 or 25 | `GND` | `GND` | common reference |

**Official reference images**

Jetson Orin Nano 40-pin header reference:

![Jetson Orin Nano 40-pin header](https://developer.download.nvidia.com/embedded/images/jetsonOrinNano/user_guide/images/jonano_cbspec_figure_3-1_white-bg.png#only-light)

ESP32-C6-DevKitC-1 pin layout reference:

![ESP32-C6-DevKitC-1 pin layout](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/_images/esp32-c6-devkitc-1-pin-layout.png)

</details>

### ESP32-C6 DevKitC-1 引脚检查

在官方 **ESP32-C6-DevKitC-1** 开发板上，本指南使用的信号均可用，并能直接按印刷的 **GPIO 标签** 接线。

对于实际的台面接线，请使用本指南和 Espressif 开发板图片中给出的 **信号名**：

- `IO7` 用于 `MOSI`
- `IO2` 用于 `MISO`
- `IO6` 用于 `SCLK`
- `IO10` 用于 `CS0`
- `IO3` 用于 `Handshake`
- `IO4` 用于 `Data Ready`
- `RST` 用于 reset

这比依赖开发板排针位置编号更不容易出错。

注意事项：Espressif 文档将 **GPIO4** 记为 ESP32-C6 上的 **strapping pin**。将其用于 **Data Ready** 仍然可行，但不要让外部接线在 ESP 复位或上电期间强制为不安全电平。

另一个实际注意事项：如果 ESP32-C6 的 `RST` 或 `EN` 引脚已经连接到 Jetson 排针的 `18` 引脚，Jetson 侧可能会干扰从 PC 进行的 USB 烧录。如果 `esptool` 无法连接或 ESP 在烧录期间不断复位，请暂时仅断开来自 Jetson 的 **reset wire**，或者确保 Jetson 主机驱动已卸载且未驱动该 GPIO。

对于本项目中三条非 SPI 控制线，Jetson 主机分支使用 **legacy global Linux GPIO numbers**，而不是 `gpiochip` 线路偏移。使用此处的 J12 引脚定义，当前映射为：

- 引脚 `15` -> `gpio433` 用于 **Data Ready**
- 引脚 `18` -> `gpio473` 用于 **Reset**
- 引脚 `22` -> `gpio471` 用于 **Handshake**

### 有用的 J12 引脚定义摘录

Jetson Orin Nano / Nano Super 扩展排针是 **J12**。在常见的 Jetson 引脚定义参考中，I2C 和 UART 引脚默认已分配。大多数其他非电源引脚默认为 GPIO，而诸如 `SPI0_MOSI` 或 `SPI1_MISO` 之类的标签是这些排针位置的建议功能。

对于本项目，以下 J12 线路是有用的交叉检查：

| J12 引脚 | J12 标签 | Linux global GPIO | 本项目中的用途 |
|----------|-----------|-------------------|---------------------|
| 13 | `SPI1_SCK` | `gpio470` | 此处未使用，但容易与引脚 `23` 混淆 |
| 15 | `GPIO12` | `gpio433` | `Data Ready` |
| 16 | `SPI1_CS1` | `gpio474` | 未使用 |
| 18 | `SPI1_CS0` | `gpio473` | 可选 `Reset` |
| 19 | `SPI0_MOSI` | `gpio483` | `MOSI` |
| 21 | `SPI0_MISO` | `gpio482` | `MISO` |
| 22 | `SPI1_MISO` | `gpio471` | `Handshake` |
| 23 | `SPI0_SCK` | `gpio481` | `SCLK` |
| 24 | `SPI0_CS0` | `gpio484` | `CS0` |
| 26 | `SPI0_CS1` | `gpio485` | 本项目未使用 |
| 37 | `SPI1_MOSI` | `gpio472` | 此处未使用，但容易与引脚 `19` 混淆 |

这是你应该记住的命名不匹配：

- Jetson-IO 预设名称：`SPI1`
- 实时 overlay mux 名称：`spi1_*`
- 引脚 `19/21/23/24/26` 上的 J12 标签：`SPI0_*`
- 已验证开发套件流程上的 Linux 设备节点：`spidev0.0`

在真实 Jetson 上，可直接用以下命令验证这些编号：

```bash
sudo cat /sys/kernel/debug/gpio | egrep 'gpio-(433|471|473)'
```


### 为什么额外的 GPIO 很重要

ESP-Hosted SPI 不仅仅是 4 线 SPI 总线：

- **Handshake** 告诉主机 ESP 侧已就绪
- **Data Ready** 告诉主机 ESP 侧有挂起的数据
- **Reset** 是必需的，以便主机可以强制 ESP 侧进入已知状态

如果这三个 GPIO 接错，SPI 传输可能会部分初始化，或者在第一个事件后失败。

### 推荐的 bring-up（上电点亮/调通）实践

- 保持导线短且长度相近
- 先在台面上开始，而不是在机箱内
- 在首次上电前清楚标记三个非 SPI GPIO
- 如果有逻辑分析仪，探测 `CS`、`SCLK`、`Handshake` 和 `Data Ready`

---

## 5. 准备 Jetson SPI 主机侧


<details>
<summary>English original</summary>

**ESP32-C6 DevKitC-1 pinout check**

On the official **ESP32-C6-DevKitC-1** board, the signals used in this guide are available and can be wired directly by their printed **GPIO labels**.

For actual bench wiring, use the **signal names** shown in the guide and on the Espressif board image:

- `IO7` for `MOSI`
- `IO2` for `MISO`
- `IO6` for `SCLK`
- `IO10` for `CS0`
- `IO3` for `Handshake`
- `IO4` for `Data Ready`
- `RST` for reset

That is less error-prone than relying on board header position numbers.

One caution: Espressif documents **GPIO4** as a **strapping pin** on ESP32-C6. Using it for **Data Ready** can still work, but do not let external wiring force an unsafe level during ESP reset or power-up.

Another practical caution: if the ESP32-C6 `RST` or `EN` pin is already wired to Jetson header pin `18`, the Jetson side can interfere with USB flashing from your PC. If `esptool` cannot connect or the ESP keeps resetting during flash, temporarily disconnect only the **reset wire** from Jetson, or make sure the Jetson host driver is unloaded and not driving that GPIO.

For the three non-SPI control lines in this project, the Jetson host fork uses **legacy global Linux GPIO numbers**, not `gpiochip` line offsets. With the J12 pinout used here, the current mapping is:

- pin `15` -> `gpio433` for **Data Ready**
- pin `18` -> `gpio473` for **Reset**
- pin `22` -> `gpio471` for **Handshake**

**Useful J12 pinout excerpt**

The Jetson Orin Nano / Nano Super expansion header is **J12**. In the common Jetson pinout references, I2C and UART pins are assigned by default. Most other non-power pins default to GPIO, and labels such as `SPI0_MOSI` or `SPI1_MISO` are suggested functions for those header positions.

For this project, these J12 lines are the useful cross-check:

| J12 pin | J12 label | Linux global GPIO | Use in this project |
|----------|-----------|-------------------|---------------------|
| 13 | `SPI1_SCK` | `gpio470` | not used here, but easy to confuse with pin `23` |
| 15 | `GPIO12` | `gpio433` | `Data Ready` |
| 16 | `SPI1_CS1` | `gpio474` | not used |
| 18 | `SPI1_CS0` | `gpio473` | optional `Reset` |
| 19 | `SPI0_MOSI` | `gpio483` | `MOSI` |
| 21 | `SPI0_MISO` | `gpio482` | `MISO` |
| 22 | `SPI1_MISO` | `gpio471` | `Handshake` |
| 23 | `SPI0_SCK` | `gpio481` | `SCLK` |
| 24 | `SPI0_CS0` | `gpio484` | `CS0` |
| 26 | `SPI0_CS1` | `gpio485` | unused in this project |
| 37 | `SPI1_MOSI` | `gpio472` | not used here, but easy to confuse with pin `19` |

This is the naming mismatch you should keep in your head:

- Jetson-IO preset name: `SPI1`
- live overlay mux names: `spi1_*`
- J12 labels on pins `19/21/23/24/26`: `SPI0_*`
- Linux device node on the validated dev kit flow: `spidev0.0`

On a real Jetson, you can verify those numbers directly with:

```bash
sudo cat /sys/kernel/debug/gpio | egrep 'gpio-(433|471|473)'
```

**Why the extra GPIOs matter**

ESP-Hosted SPI is not only a 4-wire SPI bus:

- **Handshake** tells the host the ESP side is ready
- **Data Ready** tells the host that the ESP side has data pending
- **Reset** is required so the host can force the ESP side into a known state

If those three GPIOs are wrong, the SPI transport may partially initialize or fail after the first event.

**Recommended bring-up practice**

- keep wires short and similar in length
- start on a bench, not inside an enclosure
- label the three non-SPI GPIOs clearly before first power-up
- if you have a logic analyzer, probe `CS`, `SCLK`, `Handshake`, and `Data Ready`

---

**5. Prepare the Jetson SPI host side**

</details>

### 5.1 在 40-pin 排针上启用 SPI1

用 Jetson-IO 启用排针引脚 19/21/23/24 背后的 SPI 控制器：

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
```

在全新的 Orin Nano 开发套件上，Jetson-IO 常把排针显示为大部分未分配的状态，例如：

```text
|                    unused ( 19) .. ( 20) GND                       |
|                    unused ( 21) .. ( 22) unused                    |
|                    unused ( 23) .. ( 24) unused                    |
|                       GND ( 25) .. ( 26) unused                    |
```

这是**起始状态**，不是错误。它表示 SPI 功能尚未映射到这些排针引脚上。

按以下菜单流程操作：

1. `Configure Jetson 40-pin Header`
2. `SPI1 (1 device)`
3. `Save and reboot to reconfigure pins`

为什么是 `SPI1 (1 device)`：

- 本项目只用到引脚 `24` 上的 **CS0**
- 本项目中的 ESP32-C6 不需要引脚 `26`
- 引脚 `15`、`18` 和 `22` 应保持为普通 GPIO 线可用，分别用于 `Data Ready`、`Reset` 和 `Handshake`

重启后，引脚 `19`、`21`、`23` 和 `24` 应不再是通用的 `unused` 排针引脚。它们应被分配给 SPI1 功能，Linux 应把该总线暴露为 `spidev` 设备。

保存成功时，Jetson-IO 会显示类似如下的消息：

```text
Modified /boot/extlinux/extlinux.conf to add following DTBO entries:

/boot/jetson-io-hdr40-user-custom.dtbo

Press any key to reboot the system now or Ctrl-C to abort
```

这意味着：

- Jetson-IO 为 40-pin 排针生成了一个设备树 overlay
- 它把该 overlay 加入了 `extlinux.conf`
- 新的排针映射将在下次启动时生效

对本项目而言，正确的操作是**立即重启**，让 SPI 排针映射生效。

重启后：

```bash
ls -l /dev/spidev0.0
ls /dev/spidev*
```

在开发套件上，排针的 SPI1 控制器在 Linux 中通常显示为 **`/dev/spidev0.0`**。

如果看到类似这样的内容：

```text
crw-rw---- 1 root gpio 153, 0 Apr 18 00:35 /dev/spidev0.0
```

这意味着：

- SPI 设备节点已存在
- Jetson 侧已把该总线暴露给 Linux
- `gpio` 组中的用户可以打开它

在某些系统上，你可能还会看到：

```text
/dev/spidev0.0  /dev/spidev0.1  /dev/spidev1.0  /dev/spidev1.1
```

要谨慎解读：

- `spidev0.0` 仍是最可能的 **40-pin 排针 SPI1 CS0** 设备
- `spidev0.1` 表示同一条总线上还暴露了第二个片选
- `spidev1.0` 和 `spidev1.1` 表示系统中某处还有**另一个 SPI 控制器**可用

对本项目而言，**不要**假定每个 `spidev` 节点都属于 40-pin 排针。要用 Jetson-IO overlay 和实际的排针引脚映射来确定正确的总线。

这**并不**意味着整个 ESP-Hosted 项目已经跑通。它只证明 **Jetson SPI 总线可用**。你仍然需要：

- 到 ESP32-C6 的正确接线
- 正确的 `Handshake`、`Data Ready` 和 `Reset` GPIO 映射
- 为 **SPI** 构建的 ESP32-C6 固件
- 移植并正确加载 Linux ESP-Hosted 主机侧

### 5.1.1 本项目里“unused”的含义

Jetson-IO 用 `unused` 表示“当前未分配给排针上任何一个具名的外设预设”。

对本项目而言，这正是你希望那几条额外控制线所处的状态：

- 引脚 `22` 用于 **Handshake**
- 引脚 `15` 用于 **Data Ready**
- 引脚 `18` 用于 **Reset**

这三个引脚**不**需要纳入 SPI1 预设。它们只需保持可用，供 Linux 作 GPIO 使用。

因此干净的目标状态是：

- 引脚 `19/21/23/24` 分配给 **SPI1**
- 引脚 `15/18/22` 保留为 **GPIO** 可用

如果 Jetson-IO 或其他 overlay 把其中某个控制引脚分配给了别的外设，先修正后再继续。


<details>
<summary>English original</summary>

**5.1 Enable SPI1 on the 40-pin header**

Use Jetson-IO to enable the SPI controller behind header pins 19/21/23/24:

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
```

On a fresh Orin Nano dev kit, Jetson-IO often shows the header in a mostly unassigned state, for example:

```text
|                    unused ( 19) .. ( 20) GND                       |
|                    unused ( 21) .. ( 22) unused                    |
|                    unused ( 23) .. ( 24) unused                    |
|                       GND ( 25) .. ( 26) unused                    |
```

That is the **starting state**, not an error. It means the SPI function is not yet mapped onto those header pins.

Use this menu flow:

1. `Configure Jetson 40-pin Header`
2. `SPI1 (1 device)`
3. `Save and reboot to reconfigure pins`

Why `SPI1 (1 device)`:

- this project uses only **CS0** on pin `24`
- pin `26` is not needed by the ESP32-C6 in this project
- pins `15`, `18`, and `22` should remain available as ordinary GPIO lines for `Data Ready`, `Reset`, and `Handshake`

After the reboot, pins `19`, `21`, `23`, and `24` should no longer be generic `unused` header pins. They should be assigned to the SPI1 function, and Linux should expose the bus as a `spidev` device.

When you save successfully, Jetson-IO shows a message like:

```text
Modified /boot/extlinux/extlinux.conf to add following DTBO entries:

/boot/jetson-io-hdr40-user-custom.dtbo

Press any key to reboot the system now or Ctrl-C to abort
```

That means:

- Jetson-IO generated a device-tree overlay for the 40-pin header
- it added that overlay to `extlinux.conf`
- the new header mapping will take effect on the next boot

For this project, the correct action is to **reboot now** so the SPI header mapping becomes active.

After reboot:

```bash
ls -l /dev/spidev0.0
ls /dev/spidev*
```

On the developer kit, the header's SPI1 controller typically appears to Linux as **`/dev/spidev0.0`**.

If you see something like:

```text
crw-rw---- 1 root gpio 153, 0 Apr 18 00:35 /dev/spidev0.0
```

that means:

- the SPI device node exists
- the Jetson side has exposed the bus to Linux
- users in the `gpio` group can open it

On some systems you may also see:

```text
/dev/spidev0.0  /dev/spidev0.1  /dev/spidev1.0  /dev/spidev1.1
```

Interpret that carefully:

- `spidev0.0` is still the most likely candidate for the **40-pin header SPI1 CS0** device
- `spidev0.1` means a second chip-select is also exposed on the same bus
- `spidev1.0` and `spidev1.1` mean **another SPI controller** is available somewhere in the system

For this project, do **not** assume every `spidev` node belongs to the 40-pin header. Use the Jetson-IO overlay and the physical header pin mapping to identify the correct bus.

It does **not** mean the full ESP-Hosted project is working yet. It only proves the **Jetson SPI bus is available**. You still need:

- correct wiring to the ESP32-C6
- correct `Handshake`, `Data Ready`, and `Reset` GPIO mapping
- ESP32-C6 firmware built for **SPI**
- the Linux ESP-Hosted host side ported and loaded correctly

**5.1.1 What "unused" means in this project**

Jetson-IO uses `unused` to mean "not currently assigned to one of the named peripheral presets on the header."

For this project, that is actually what you want for the extra control lines:

- pin `22` for **Handshake**
- pin `15` for **Data Ready**
- pin `18` for **Reset**

Those three pins do **not** need to be part of the SPI1 preset. They just need to stay available for GPIO use from Linux.

So the clean target state is:

- pins `19/21/23/24` assigned to **SPI1**
- pins `15/18/22` left available as **GPIO**

If Jetson-IO or another overlay assigns one of those control pins to some other peripheral, fix that before continuing.

</details>

### 5.1.2 像工程师一样读懂 Jetson-IO overlay

若反编译生成的 overlay：

```bash
sudo dtc -I dtb -O dts \
  -o /tmp/jetson-io-hdr40-user-custom.dts \
  /boot/jetson-io-hdr40-user-custom.dtbo
```

会看到类似这样的条目：

```dts
hdr40-pin19 {
    nvidia,pins = "spi1_mosi_pz5";
    nvidia,function = "spi1";
};

hdr40-pin21 {
    nvidia,pins = "spi1_miso_pz4";
    nvidia,function = "spi1";
};

hdr40-pin23 {
    nvidia,pins = "spi1_sck_pz3";
    nvidia,function = "spi1";
};

hdr40-pin24 {
    nvidia,pins = "spi1_cs0_pz6";
    nvidia,function = "spi1";
};

hdr40-pin26 {
    nvidia,pins = "spi1_cs1_pz7";
    nvidia,function = "spi1";
};
```

以下部分是教学中最关键的内容：

- `hdr40-pin19` 表示**物理排针引脚 19**
- `nvidia,pins = "spi1_mosi_pz5"` 表示该排针引脚背后的 SoC pad 就是 NVIDIA 命名为 `spi1_mosi_pz5` 的 pad
- `nvidia,function = "spi1"` 表示 Jetson-IO 将该 pad 分配给 **SPI1 外设功能**

因此直白地说，这个块表达的是：

- 引脚 `19` 现在是 **SPI1 MOSI**
- 引脚 `21` 现在是 **SPI1 MISO**
- 引脚 `23` 现在是 **SPI1 SCLK**
- 引脚 `24` 现在是 **SPI1 CS0**
- 引脚 `26` 现在是 **SPI1 CS1**

这正是该 ESP32-C6 项目所需要的。

实际 bring-up（上电点亮/调通）提示：在某些 Jetson-IO 结果中，`SPI1 (1 device)` 仍会生成一个 overlay，把 **引脚 26** 复用为 `spi1_cs1_pz7`，同时还暴露出 `/dev/spidev0.1`。对该项目而言这是可接受的。除非有意添加第二个 SPI 目标，否则只需让引脚 `26` 和 `spidev0.1` 保持未使用。

### 5.1.3 其他设备树段落的含义

反编译得到的 overlay 还包含如下段落：

- `fragment@0`
- `fragment@1`
- `__symbols__`
- `__fixups__`

简要说明：

- `fragment@0` 是普通 Tegra pin controller 的主要 pinmux 变更
- `fragment@1` 是 **AON** pin controller 的配套块；对于本次 SPI1 变更，它通常不包含值得关注的排针重映射行
- `__symbols__` 为 overlay 内的节点命名，以便设备树的其他部分可以引用它们
- `__fixups__` 告知 bootloader 或设备树加载器在对基础板级设备树应用该 overlay 时的位置

要熟练使用 Jetson-IO，你并**不**需要理解 overlay 内部的每个细节。对于实际 bring-up，关键检查是：

1. overlay 存在于 `/boot/jetson-io-hdr40-user-custom.dtbo`
2. `extlinux.conf` 引用了它
3. overlay 将排针引脚 `19/21/23/24` 映射到 `spi1`
4. 重启后 `/dev/spidev0.0` 存在

若这四项均成立，则 Jetson 侧的 SPI pinmux 就处于正确状态。

### 5.1.4 引脚 26 在实际中的含义

选择 **`SPI1 (1 device)`** 后，可能出现两种有效结果：

1. 在 overlay 与 `/dev/spidev0.0` 中只有 `CS0` 路径是明显的
2. `CS0` 与 `CS1` 都被复用，且 Linux 还会暴露出 `/dev/spidev0.1`

若引脚 `26` 显示为：

```dts
hdr40-pin26 {
    nvidia,pins = "spi1_cs1_pz7";
    nvidia,function = "spi1";
};
```

这表示 Jetson-IO 还把 **排针引脚 26** 映射到了 **SPI1 CS1**。

这**不会**破坏该项目。它只意味着：

- 排针总线现在有第二个可用的 chip-select
- Linux 可能暴露出 `/dev/spidev0.1`
- 在此 ESP32-C6 方案中，引脚 `26` 应保持物理上不连接

该项目仍使用：

- `spidev0.0`
- 引脚 `24` 上的 `CS0`
- 仅一个 ESP32-C6 目标


<details>
<summary>English original</summary>

**5.1.2 Read the Jetson-IO overlay like an engineer**

If you decompile the generated overlay:

```bash
sudo dtc -I dtb -O dts \
  -o /tmp/jetson-io-hdr40-user-custom.dts \
  /boot/jetson-io-hdr40-user-custom.dtbo
```

you will see entries like:

```dts
hdr40-pin19 {
    nvidia,pins = "spi1_mosi_pz5";
    nvidia,function = "spi1";
};

hdr40-pin21 {
    nvidia,pins = "spi1_miso_pz4";
    nvidia,function = "spi1";
};

hdr40-pin23 {
    nvidia,pins = "spi1_sck_pz3";
    nvidia,function = "spi1";
};

hdr40-pin24 {
    nvidia,pins = "spi1_cs0_pz6";
    nvidia,function = "spi1";
};

hdr40-pin26 {
    nvidia,pins = "spi1_cs1_pz7";
    nvidia,function = "spi1";
};
```

This is the part that matters most for teaching:

- `hdr40-pin19` means **physical header pin 19**
- `nvidia,pins = "spi1_mosi_pz5"` means the SoC pad behind that header pin is the pad NVIDIA names `spi1_mosi_pz5`
- `nvidia,function = "spi1"` means Jetson-IO is assigning that pad to the **SPI1 peripheral function**

So in plain language, that block says:

- pin `19` is now **SPI1 MOSI**
- pin `21` is now **SPI1 MISO**
- pin `23` is now **SPI1 SCLK**
- pin `24` is now **SPI1 CS0**
- pin `26` is now **SPI1 CS1**

That is exactly what this ESP32-C6 project needs.

Real bring-up note: on some Jetson-IO results, `SPI1 (1 device)` still produces an overlay that muxes **pin 26** to `spi1_cs1_pz7` and also exposes `/dev/spidev0.1`. That is acceptable for this project. You simply leave pin `26` and `spidev0.1` unused unless you intentionally add a second SPI target.

**5.1.3 What the other device-tree sections mean**

Your decompiled overlay also contains sections like:

- `fragment@0`
- `fragment@1`
- `__symbols__`
- `__fixups__`

Short explanation:

- `fragment@0` is the main pinmux change for the normal Tegra pin controller
- `fragment@1` is the companion block for the **AON** pin controller; for this SPI1 change it usually does not carry the interesting header remap lines
- `__symbols__` gives names to nodes inside the overlay so other parts of the device tree can refer to them
- `__fixups__` tells the bootloader or device-tree loader where to apply the overlay against the base board device tree

You do **not** need to understand every overlay internals detail to use Jetson-IO well. For practical bring-up, the key test is:

1. the overlay exists in `/boot/jetson-io-hdr40-user-custom.dtbo`
2. `extlinux.conf` references it
3. the overlay maps header pins `19/21/23/24` to `spi1`
4. `/dev/spidev0.0` exists after reboot

If all four are true, the Jetson side SPI pinmux is in the right state.

**5.1.4 What pin 26 means in practice**

There are two valid outcomes you may see after selecting **`SPI1 (1 device)`**:

1. Only the `CS0` path is obvious in the overlay and `/dev/spidev0.0`
2. Both `CS0` and `CS1` are muxed, and Linux also exposes `/dev/spidev0.1`

If pin `26` appears as:

```dts
hdr40-pin26 {
    nvidia,pins = "spi1_cs1_pz7";
    nvidia,function = "spi1";
};
```

that means Jetson-IO also mapped **header pin 26** to **SPI1 CS1**.

That does **not** break this project. It only means:

- the header bus now has a second available chip-select
- Linux may expose `/dev/spidev0.1`
- you should leave pin `26` physically unconnected for this ESP32-C6 setup

This project still uses:

- `spidev0.0`
- `CS0` on pin `24`
- one ESP32-C6 target only

</details>

### 5.1.5 在真实 Jetson 上验证 live 设备树

重启后，也可以 dump live 设备树：

```bash
sudo dtc -I fs -O dts -o /tmp/live.dts /sys/firmware/devicetree/base
```

该命令在 Jetson 上打印大量警告是正常的。这些警告通常反映 NVIDIA 如何发布 live 设备树，而**不**意味着你的 SPI 设置失败。

对本项目而言，有用的检查要窄得多：

1. `/boot/extlinux/extlinux.conf` 包含：
   `OVERLAYS /boot/jetson-io-hdr40-user-custom.dtbo`
2. `/boot/jetson-io-hdr40-user-custom.dtbo` 存在
3. 反编译后的 overlay 将排针引脚 `19/21/23/24` 映射到 `spi1`
4. 重启后 `/dev/spidev0.0` 存在

如果这些都成立，Jetson 侧 SPI 排针设置已经正确到足以继续。

可以再深入一层，证明每个 `spidev` 节点属于哪个 Linux SPI 控制器：

```bash
for d in /sys/class/spidev/spidev*; do
  echo "== $(basename "$d") =="
  readlink -f "$d/device"
done
```

来自真实 Orin Nano dev kit 的示例：

```text
== spidev0.0 ==
/sys/devices/platform/bus@0/3210000.spi/spi_master/spi0/spi0.0
== spidev0.1 ==
/sys/devices/platform/bus@0/3210000.spi/spi_master/spi0/spi0.1
== spidev1.0 ==
/sys/devices/platform/bus@0/3230000.spi/spi_master/spi1/spi1.0
== spidev1.1 ==
/sys/devices/platform/bus@0/3230000.spi/spi_master/spi1/spi1.1
```

这就是清晰的证据：

- 在 Jetson-IO 中选择为 **`SPI1`** 的 40-pin 排针路径映射到 Linux **`spi0`**，因此也就是 **`spidev0.*`**
- `spidev0.0` 是本项目需要的 **CS0** 设备
- `spidev0.1` 是同一控制器的 **CS1**
- `spidev1.0` 和 `spidev1.1` 是**不同的 SPI 控制器**

这种命名不匹配在 Jetson 上很正常。面向板级的名称是 **header SPI1**，而 Linux 可见的总线名称通常是 **`spi0`**。

### 5.2 安装实用工具

```bash
sudo apt update
sudo apt install -y \
  linux-headers-$(uname -r) \
  libgpiod-dev gpiod \
  spi-tools \
  network-manager \
  iperf3
```

### 5.3 先验证基本 SPI 路径

在加载 ESP-Hosted 驱动之前，确保控制器处于活动状态且排针配置正确。

- 确认 `/dev/spidev0.0` 存在
- 用 `ls /dev/spidev*` 列出所有 SPI 设备节点
- 确认 Jetson-IO 不再将引脚 `19/21/23/24` 显示为普通 `unused`
- 确认你选择的 GPIO 线空闲可用
- 验证没有冲突设备已经绑定到同一总线/chip-select

如果在你开始本指南**之前** `/dev/spidev0.0` 已经存在，那么你的 Jetson 可能已通过之前的 Jetson-IO 配置启用了 SPI1。在这种情况下，Jetson 侧的“启用 SPI 总线”步骤你已经完成，但仍需完成 ESP-Hosted 接线和 host 驱动工作。

如果你还看到 `spidev0.1`，通常意味着同一排针 SPI 总线上有 **CS1** 可用。本项目忽略它。

如果看到类似 `spidev1.0` 和 `spidev1.1` 的额外节点，在证明并非如此之前，把它们当作**不同的 SPI 控制器**。不要仅仅因为它们存在，就让 ESP-Hosted host 代码指向它们。

如果计划直接使用 Espressif 内核模块，请注意，一旦从原始 SPI 健全性检查转向真实 host 驱动路径，**generic `spidev` 可能需要禁用**。

### 5.4 保守启动

首次 bring-up（上电点亮/调通）期间：

- 使用短线
- 如果时序不稳定，从 **1 MHz** SPI 时钟开始
- 避免一次更改多个变量

---


<details>
<summary>English original</summary>

**5.1.5 Live device-tree verification on a real Jetson**

After reboot, you can also dump the live device tree:

```bash
sudo dtc -I fs -O dts -o /tmp/live.dts /sys/firmware/devicetree/base
```

It is normal for that command to print a large number of warnings on Jetson. Those warnings usually reflect how NVIDIA ships the live tree and do **not** mean your SPI setup failed.

For this project, the useful checks are much narrower:

1. `/boot/extlinux/extlinux.conf` contains:
   `OVERLAYS /boot/jetson-io-hdr40-user-custom.dtbo`
2. `/boot/jetson-io-hdr40-user-custom.dtbo` exists
3. the decompiled overlay maps header pins `19/21/23/24` to `spi1`
4. `/dev/spidev0.0` exists after reboot

If those are true, the Jetson side SPI header setup is correct enough to continue.

You can go one level deeper and prove which Linux SPI controller each `spidev` node belongs to:

```bash
for d in /sys/class/spidev/spidev*; do
  echo "== $(basename "$d") =="
  readlink -f "$d/device"
done
```

Example from a real Orin Nano dev kit:

```text
== spidev0.0 ==
/sys/devices/platform/bus@0/3210000.spi/spi_master/spi0/spi0.0
== spidev0.1 ==
/sys/devices/platform/bus@0/3210000.spi/spi_master/spi0/spi0.1
== spidev1.0 ==
/sys/devices/platform/bus@0/3230000.spi/spi_master/spi1/spi1.0
== spidev1.1 ==
/sys/devices/platform/bus@0/3230000.spi/spi_master/spi1/spi1.1
```

That is the clean proof that:

- the 40-pin header path selected in Jetson-IO as **`SPI1`** maps to Linux **`spi0`** and therefore **`spidev0.*`**
- `spidev0.0` is the **CS0** device you want for this project
- `spidev0.1` is the same controller's **CS1**
- `spidev1.0` and `spidev1.1` are a **different SPI controller**

This naming mismatch is normal on Jetson. The board-facing name is **header SPI1**, while the Linux-visible bus name is often **`spi0`**.

**5.2 Install useful tools**

```bash
sudo apt update
sudo apt install -y \
  linux-headers-$(uname -r) \
  libgpiod-dev gpiod \
  spi-tools \
  network-manager \
  iperf3
```

**5.3 Verify the basic SPI path first**

Before loading the ESP-Hosted driver, make sure the controller is alive and the header is configured correctly.

- confirm `/dev/spidev0.0` exists
- list all SPI device nodes with `ls /dev/spidev*`
- confirm Jetson-IO no longer shows pins `19/21/23/24` as plain `unused`
- confirm your chosen GPIO lines are free for use
- verify there is no conflicting device already bound to the same bus/chip-select

If `/dev/spidev0.0` already exists **before** you start this guide, your Jetson may already have SPI1 enabled from an earlier Jetson-IO configuration. In that case, you are already past the "enable SPI bus" step on the Jetson side, but you still need to finish the ESP-Hosted wiring and host-driver work.

If you also see `spidev0.1`, that usually means **CS1** is available on the same header SPI bus. Ignore it for this project.

If you see additional nodes like `spidev1.0` and `spidev1.1`, treat them as **different SPI controllers** until proven otherwise. Do not point the ESP-Hosted host code at them just because they exist.

If you plan to use the Espressif kernel module directly, be aware that **generic `spidev` may need to be disabled** once you move from raw SPI sanity checks to the real host driver path.

**5.4 Start conservatively**

During first bring-up:

- use short wires
- start with **1 MHz** SPI clock if timing is unstable
- avoid changing multiple variables at once

---

</details>

## 6. 为 ESP32-C6 构建并烧录 ESP-Hosted-NG

克隆仓库，并严格基于上游 ESP-Hosted-NG 代码树准备 ESP 侧：

```bash
git clone https://github.com/espressif/esp-hosted.git
cd esp-hosted/esp_hosted_ng/esp/esp_driver
./setup.sh
cd esp-idf
. ./export.sh
cd ../network_adapter
```

将目标设置为 ESP32-C6：

```bash
idf.py set-target esp32c6
```

打开配置：

```bash
idf.py menuconfig
```

选择 SPI 传输方式：

- `Example Configuration`
- `Transport layer`
- `SPI interface`

然后打开 `Example Configuration -> SPI Configuration`，除非有明确理由需要修改，否则保留 **ESP32-C6** 的默认值：

- `GPIO pin for handshake` = `3`
- `GPIO pin for data ready interrupt` = `4`
- `ESP to Host SPI queue size` = `20`
- `Host to ESP SPI queue size` = `20`
- `SPI checksum ENABLE/DISABLE` = enabled
- `De-assert HS on CS` = disabled

这些默认值与 Espressif 针对 **ESP32-C6** 的 SPI 配置一致：

- `IO10` = `CS0`
- `IO6` = `SCLK`
- `IO2` = `MISO`
- `IO7` = `MOSI`
- `IO3` = `Handshake`
- `IO4` = `Data Ready`
- `RST` = reset

从 Linux 主机 PC 烧录时，**ESP32-C6-DevKitC-1** 上内置的 **USB Serial/JTAG** 端口通常显示为 **`/dev/ttyACM0`**。像 **`/dev/ttyUSB0`** 这样的设备往往是独立的 USB-UART 桥接器，直接烧录 C6 时不应优先尝试该端口。

要干净地识别正确端口：

```bash
sudo dmesg -w
```

然后拔下并重新连接 ESP 板。新出现的 **`ttyACM*`** 设备通常就是正确的端口。

然后构建并烧录：

```bash
idf.py -p /dev/ttyACM0 build flash
idf.py -p /dev/ttyACM0 monitor
```

仅当你的板子出现在另一个 **`ttyACM*`** 设备上时，才替换 `/dev/ttyACM0`。

如果需要在完整烧录前确认端口，可直接查询芯片：

```bash
python -m esptool --chip esp32c6 -p /dev/ttyACM0 chip_id
```

### 连接 Jetson 接线时烧录

如果 ESP 板已接到 Jetson 40-pin 排针上，最安全的烧录顺序是：

1. 保持 ESP 板通过 USB 连接到主机 PC。
2. 断开从 Jetson `Reset` 接到 ESP `RST` 或 `EN` 的连线，或先卸载 Jetson 主机驱动。
3. 从 PC 烧录 ESP 固件。
4. 对于本指南中已验证的 Jetson bring-up（上电点亮/调通）流程，保持 reset 连线断开，并在 Jetson 侧使用 `resetpin=-1`。
5. 完成上述步骤后，再加载 Jetson ESP-Hosted 主机驱动。

如果你使用的是本指南后文所述的 Jetson 端口，卸载主机驱动的操作如下：

```bash
sudo rmmod esp32_spi
```

这样可避免 Jetson 的 reset GPIO 干扰 USB 烧录过程。

### 预期现象

- 构建和烧录成功
- 串口监视器上出现 ESP 启动日志
- USB 连接的 DevKitC-1 在主机 PC 上显示为 `ttyACM*` 设备
- 固件所配置的传输方式与你要在 Jetson 主机侧使用的一致

**切勿混用传输方式。** 为 SPI 构建的主机与为 SDIO 或 UART 构建的 ESP 固件搭配使用时，会以看似板子或时序 bug 的方式失败。

---

## 7. 将 Linux 主机侧从 Raspberry Pi 移植到 Jetson

Espressif 以 Raspberry Pi 作为参考 Linux 主机。在 Jetson 上，应以该主机实现为起点，而不是试图另造一套新栈。

### 7.1 从上游主机代码树开始

相关上游位置：

- `esp_hosted_ng/host/`
- `esp_hosted_ng/host/spi/`
- `esp_hosted_ng/docs/setup.md`
- `esp_hosted_ng/docs/porting_guide.md`

Raspberry Pi 辅助脚本仅作为上游参考有用：

```bash
cd esp-hosted/esp_hosted_ng/host
```

在 Raspberry Pi 上，文档给出的入口点是：

```bash
bash rpi_init.sh spi
```

在 Jetson 上，**不要**将 `rpi_init.sh` 作为主要的 bring-up 路径。它是 Raspberry Pi 专用的。对于本指南所述的 Jetson Orin Nano 流程，请改用面向 Jetson 的 fork 及其主机 README：

- `esp_hosted_ng/host/README.md`
- `esp_hosted_ng/host/jetson_orin_nano_init.sh`

该路径已包含本指南中经过验证的 Jetson 设置：

- `resetpin=-1`
- `spi_handshake_gpio=471`
- `spi_dataready_gpio=433`
- `spi_bus_num=0`
- `spi_chip_select=0`
- `spi_mode=2`
- `clockspeed=10`


<details>
<summary>English original</summary>

**6. Build and flash ESP-Hosted-NG for ESP32-C6**

Clone the repo and prepare the ESP side exactly from the upstream ESP-Hosted-NG tree:

```bash
git clone https://github.com/espressif/esp-hosted.git
cd esp-hosted/esp_hosted_ng/esp/esp_driver
./setup.sh
cd esp-idf
. ./export.sh
cd ../network_adapter
```

Set the target to ESP32-C6:

```bash
idf.py set-target esp32c6
```

Open configuration:

```bash
idf.py menuconfig
```

Select the SPI transport:

- `Example Configuration`
- `Transport layer`
- `SPI interface`

Then open `Example Configuration -> SPI Configuration` and keep the default **ESP32-C6** values unless you have a specific reason to change them:

- `GPIO pin for handshake` = `3`
- `GPIO pin for data ready interrupt` = `4`
- `ESP to Host SPI queue size` = `20`
- `Host to ESP SPI queue size` = `20`
- `SPI checksum ENABLE/DISABLE` = enabled
- `De-assert HS on CS` = disabled

Those defaults match Espressif's SPI setup for **ESP32-C6**:

- `IO10` = `CS0`
- `IO6` = `SCLK`
- `IO2` = `MISO`
- `IO7` = `MOSI`
- `IO3` = `Handshake`
- `IO4` = `Data Ready`
- `RST` = reset

When flashing from a Linux host PC, the built-in **USB Serial/JTAG** port on an **ESP32-C6-DevKitC-1** usually appears as **`/dev/ttyACM0`**. A device such as **`/dev/ttyUSB0`** is often a separate USB-UART bridge and is not the first port to try for direct C6 flashing.

To identify the right port cleanly:

```bash
sudo dmesg -w
```

Then unplug and reconnect the ESP board. The newly appearing **`ttyACM*`** device is usually the correct port.

Then build and flash:

```bash
idf.py -p /dev/ttyACM0 build flash
idf.py -p /dev/ttyACM0 monitor
```

Replace `/dev/ttyACM0` only if your board appears on a different **`ttyACM*`** device.

If you need to prove the port before a full flash, query the chip directly:

```bash
python -m esptool --chip esp32c6 -p /dev/ttyACM0 chip_id
```

**Flashing with Jetson wiring attached**

If the ESP board is already wired to the Jetson 40-pin header, the safest flashing sequence is:

1. Keep the ESP board connected to the host PC over USB.
2. Disconnect the Jetson `Reset` wire from ESP `RST` or `EN`, or unload the Jetson host driver first.
3. Flash the ESP firmware from the PC.
4. For the proven Jetson bring-up flow in this guide, leave the reset wire disconnected and use `resetpin=-1` on the Jetson side.
5. Only then load the Jetson ESP-Hosted host driver.

If you are using the Jetson port described later in this guide, unloading the host driver looks like this:

```bash
sudo rmmod esp32_spi
```

This avoids the Jetson reset GPIO interfering with the USB flashing process.

**What you should see**

- successful build and flash
- ESP boot logs on the serial monitor
- the USB-connected DevKitC-1 appearing as a `ttyACM*` device on the host PC
- the firmware configured for the same transport you intend to use on the Jetson host side

**Do not mix transports.** A host built for SPI and an ESP firmware built for SDIO or UART will fail in ways that look like board or timing bugs.

---

**7. Port the Linux host side from Raspberry Pi to Jetson**

Espressif ships Raspberry Pi as the reference Linux host. On Jetson, use that host implementation as the starting point rather than trying to invent a new stack.

**7.1 Start from the upstream host tree**

Relevant upstream locations:

- `esp_hosted_ng/host/`
- `esp_hosted_ng/host/spi/`
- `esp_hosted_ng/docs/setup.md`
- `esp_hosted_ng/docs/porting_guide.md`

The Raspberry Pi helper script is useful as upstream reference only:

```bash
cd esp-hosted/esp_hosted_ng/host
```

On Raspberry Pi the documented entry point is:

```bash
bash rpi_init.sh spi
```

On Jetson, do **not** use `rpi_init.sh` as your main bring-up path. It is Raspberry Pi specific. For the Jetson Orin Nano flow described in this guide, use the Jetson-oriented fork and its host README instead:

- `esp_hosted_ng/host/README.md`
- `esp_hosted_ng/host/jetson_orin_nano_init.sh`

That path already captures the validated Jetson settings in this guide:

- `resetpin=-1`
- `spi_handshake_gpio=471`
- `spi_dataready_gpio=433`
- `spi_bus_num=0`
- `spi_chip_select=0`
- `spi_mode=2`
- `clockspeed=10`

</details>

### 7.2 Jetson 专用移植工作

将这些部分移植到 Jetson：

1. **复位 GPIO**
   Jetson 排针引脚 `18` 映射到传统全局 GPIO `473`，但在本指南已验证的 bring-up（上电点亮/调通）中，主机通过 `resetpin=-1` 将该路径保持为禁用。这样可避免 Jetson 把 ESP 保持在复位状态，或干扰 USB 烧录。只有在另行验证复位连线稳定后，才启用 `resetpin=473`。

2. **握手与数据就绪 GPIO**
   更新主机侧 SPI 定义，使其与你选定的 Jetson GPIO 排针引脚一致：
   - 握手在引脚 `22` -> 传统全局 GPIO `471`
   - 数据就绪在引脚 `15` -> 传统全局 GPIO `433`

3. **SPI 总线与片选选择**
   设置主机侧 SPI 总线与片选值，使其与排针背后 Linux 可见的设备一致。在本指南已验证的 Orin Nano dev kit 流程中，即：
   - 排针 `SPI1`
   - Linux 控制器 `spi0`
   - 设备节点 `spidev0.0`
   - sysfs 路径 `3210000.spi`
   - 主机模块值：`bus 0`、`chip-select 0`

   如果你的系统还暴露了 `spidev0.1`，在你有意迁移到 **CS1** 上的第二个目标之前，保持其未使用。除非你已另行验证某个不同控制器才是你想要的，否则不要切换到 `spidev1.*`。

4. **设备树与引脚复用**
   确认：
   - SPI1 已在 40-pin 排针上启用
   - 三个额外 GPIO 未被占用，且已配置为 GPIO 用途
   - 没有冲突的 overlay 或驱动占用相同引脚

5. **`spidev` 冲突处理**
   一旦迁移到真正的 ESP-Hosted 内核模块路径，如果通用 `spidev` 阻止 Espressif 驱动占用总线，就将其禁用。

6. **构建环境**
   如果在端侧构建，将主机侧构建指向 Jetson kernel 头文件。如果交叉编译，更新 `ARCH`、`CROSS_COMPILE` 以及 `aarch64` 的 kernel 路径。

### 7.2.1 实用的 Jetson 主机路径

现已存在一个面向 Jetson 的 fork，位于：

- `https://github.com/ai-hpc/jetson-esp-hosted`

它新增了：

- 一个 Jetson 专用辅助脚本：
  - `esp_hosted_ng/host/jetson_orin_nano_init.sh`
- 一个 Jetson 主机 README：
  - `esp_hosted_ng/host/README.md`
- 以下模块参数：
  - SPI 总线号
  - 片选
  - 握手 GPIO
  - 数据就绪 GPIO
  - SPI 模式

在真实的 Jetson Orin Nano dev kit 上，该辅助脚本已被验证可以：

- 构建 `esp32_spi.ko`
- 在当前启动中，将 `spi0.0` 从通用 `spidev` 解绑
- 干净地插入 ESP-Hosted SPI 主机模块
- 即使 ESP 启动事件请求 `26 MHz`，仍将 runtime SPI 时钟上限保持在 `10 MHz`
- 在手动用 `resetpin=-1` 复位 ESP 后，bring up `wlan0`

这证明 **Jetson 主机驱动构建/加载路径** 与 **可工作的 SPI/Wi-Fi 传输路径** 在此硬件上都是真实成立的。已验证的序列为：

1. 用 `resetpin=-1` 加载 Jetson 模块
2. 保持 `sudo dmesg -w` 打开
3. 手动按下 ESP 复位按钮
4. 等待 ESP 启动事件、芯片组检测、时钟钳制以及 `wlan0`

### 7.3 良好的 bring-up 顺序

按此顺序操作。它能减少歧义。

1. 启用 SPI1 并验证 `/dev/spidev0.0`
2. 验证任何额外的 `spidev1.*` 节点都不是你打算使用的总线
3. 确认 Jetson GPIO 选择与实际连线在物理上匹配
4. 向 ESP32-C6 烧录启用 SPI 的 ESP-Hosted 固件
5. 移植主机侧 GPIO 映射与总线选择
6. 从 `resetpin=-1` 和 `clockspeed=10` 开始
7. 加载主机模块，然后手动按下 ESP 复位按钮
8. 观察 `dmesg`，等待 ESP 启动事件、芯片组检测，以及 26 MHz 请求被钳制到 10 MHz
9. 确认 `wlan0` 出现
10. 只有到那时才进行 Wi-Fi 关联与吞吐测试

---

## 8. 验证检查清单

### 8.1 传输层验证

```bash
sudo dmesg -w
```

期望看到：

- Jetson 侧驱动干净加载
- 在手动复位 ESP 后，主机收到来自 ESP 侧的第一个事件
- 芯片组检测成功
- ESP 对 `26 MHz` 的请求被钳制到主机配置的上限 `10 MHz`

### 8.2 接口层验证

```bash
ip link show
nmcli device status
hciconfig -a
bluetoothctl list
```

预期结果：

- 一个新的无线接口，例如 `wlan0` 或 `wlan1`
- 一个蓝牙控制器，例如 `hci0`
- `hciconfig -a` 显示 `Bus: SPI`
- `bluetoothctl list` 显示 ESP 控制器

在本指南已验证的 Jetson Orin Nano 加 ESP32-C6 流程中，实际预期的接口是 `wlan0` 和 `hci0`。

### 8.3 加入网络

```bash
nmcli dev wifi list ifname wlan0
sudo nmcli dev wifi connect "SSID" password "password"
ip addr show
ping -c 4 8.8.8.8
```


<details>
<summary>English original</summary>

**7.2 Jetson-specific porting work**

Port these pieces to Jetson:

1. **Reset GPIO**
   Jetson header pin `18` maps to legacy global GPIO `473`, but on the validated bring-up in this guide the host keeps that path disabled with `resetpin=-1`. That avoids the Jetson holding the ESP in reset or interfering with USB flashing. Only opt in to `resetpin=473` after you have separately proven that your reset wiring is stable.

2. **Handshake and Data Ready GPIOs**
   Update the host SPI definitions so they match your chosen Jetson GPIO header pins:
   - handshake on pin `22` -> legacy global GPIO `471`
   - data ready on pin `15` -> legacy global GPIO `433`

3. **SPI bus and chip select selection**
   Set the host-side SPI bus and chip-select values to match the Linux-visible device behind the header. On the validated Orin Nano dev kit flow in this guide, that is:
   - header `SPI1`
   - Linux controller `spi0`
   - device node `spidev0.0`
   - sysfs path `3210000.spi`
   - host module values: `bus 0`, `chip-select 0`

   If your system also exposes `spidev0.1`, leave that unused unless you intentionally move to a second target on **CS1**. Do not switch to `spidev1.*` unless you have separately proven that a different controller is the one you want.

4. **Device tree and pinmux**
   Make sure:
   - SPI1 is enabled on the 40-pin header
   - the three extra GPIOs are free and configured for GPIO use
   - no conflicting overlay or driver claims the same pins

5. **`spidev` conflict handling**
   Once you move to the real ESP-Hosted kernel module path, disable generic `spidev` if it prevents the Espressif driver from claiming the bus.

6. **Build environment**
   If you build on-device, point the host build at Jetson kernel headers. If you cross-compile, update `ARCH`, `CROSS_COMPILE`, and kernel paths for `aarch64`.

**7.2.1 Practical Jetson host path**

A Jetson-oriented fork now exists at:

- `https://github.com/ai-hpc/jetson-esp-hosted`

It adds:

- a Jetson-specific helper script:
  - `esp_hosted_ng/host/jetson_orin_nano_init.sh`
- a Jetson host README:
  - `esp_hosted_ng/host/README.md`
- module parameters for:
  - SPI bus number
  - chip select
  - handshake GPIO
  - data-ready GPIO
  - SPI mode

On a real Jetson Orin Nano dev kit, that helper has already been validated to:

- build `esp32_spi.ko`
- unbind `spi0.0` from generic `spidev` for the current boot
- insert the ESP-Hosted SPI host module cleanly
- keep the runtime SPI clock capped at `10 MHz` even when the ESP boot-up event requests `26 MHz`
- bring up `wlan0` after a manual ESP reset with `resetpin=-1`

That proves the **Jetson host driver build/load path** and the **working SPI/Wi-Fi transport path** are both real on this hardware. The validated sequence is:

1. load the Jetson module with `resetpin=-1`
2. keep `sudo dmesg -w` open
3. manually press the ESP reset button
4. wait for the ESP boot-up event, chipset detection, clock clamp, and `wlan0`

**7.3 Good bring-up order**

Use this order. It reduces ambiguity.

1. Enable SPI1 and verify `/dev/spidev0.0`
2. Verify that any additional `spidev1.*` nodes are not the bus you intend to use
3. Confirm Jetson GPIO choices physically match your wiring
4. Flash the ESP32-C6 with SPI-enabled ESP-Hosted firmware
5. Port the host-side GPIO mapping and bus selection
6. Start with `resetpin=-1` and `clockspeed=10`
7. Load the host module, then manually press the ESP reset button
8. Watch `dmesg` for the ESP boot-up event, chipset detection, and the 26 MHz request being clamped to 10 MHz
9. Confirm `wlan0` appears
10. Only then move to Wi-Fi association and throughput tests

---

**8. Validation checklist**

**8.1 Transport-level validation**

```bash
sudo dmesg -w
```

You want to see:

- the Jetson-side driver loads cleanly
- the host receives the first event from the ESP side after you manually reset the ESP
- chipset detection succeeds
- the ESP request for `26 MHz` is clamped to the configured host limit of `10 MHz`

**8.2 Interface-level validation**

```bash
ip link show
nmcli device status
hciconfig -a
bluetoothctl list
```

Expected result:

- a new wireless interface such as `wlan0` or `wlan1`
- a Bluetooth controller such as `hci0`
- `hciconfig -a` shows `Bus: SPI`
- `bluetoothctl list` shows the ESP controller

On the validated Jetson Orin Nano plus ESP32-C6 flow in this guide, the real expected interfaces are `wlan0` and `hci0`.

**8.3 Join a network**

```bash
nmcli dev wifi list ifname wlan0
sudo nmcli dev wifi connect "SSID" password "password"
ip addr show
ping -c 4 8.8.8.8
```

</details>

### 8.4 BLE 发现冒烟测试

这里的 ESP32-C6 主机侧通路是 **BLE-only**，而非经典蓝牙音频。

```bash
rfkill list
bluetoothctl
```

在 `bluetoothctl` 内：

```text
power on
scan on
devices
show
```

预期结果：

- `scan on` 成功
- 附近的 BLE 设备出现在 `devices` 中
- `show` 列出控制器角色与广播特性

在本指南已验证的流程上，`bluetoothctl` 成功发现了通过 SPI 暴露的 ESP32-C6 控制器附近的 BLE 设备。

如果 `rfkill` 显示 Wi-Fi 被软阻塞而 Bluetooth 未被阻塞，BLE 扫描仍可工作。

在 BLE bring-up（上电点亮/调通）早期，也可能看到以下之一：

- `Can't read local name on hci0: Input/output error (5)`
- `Failed to set local name: Failed (0x03)`

这些行并不理想，但在本处已验证的 Jetson 流程上，它们没有阻断 BLE 扫描与发现。

### 8.5 基本吞吐冒烟测试

在另一台机器上：

```bash
iperf3 -s
```

在 Jetson 上：

```bash
iperf3 -c <server-ip>
```

不要过早优化。先证明：

- 扫描稳定
- 关联稳定
- 数据包流干净
- 没有反复的传输层复位

---

## 9. 常见失效模式

### `dmesg` 中没有首个事件

通常是以下之一：

- 用 `resetpin=-1` 加载了主机，但尚未手动复位 ESP
- reset GPIO 有误
- handshake 或 data-ready GPIO 映射错误
- SPI 模式或时钟过于激进
- ESP 固件实际上不是为 SPI 构建的

如果这发生在刚烧录之后，还要确认烧录期间 ESP 没有被 Jetson 的 reset 线保持在复位状态。

### 首个事件出现后流量停滞

常见原因：

- 主机上 handshake/data-ready 中断配置不正确
- GPIO 边沿极性错误
- 杜邦线过长或有噪声
- 主机驱动与 ESP 固件版本不一致
- 主机跳到高于布线所能支持的 SPI 时钟

在本指南已验证的 Jetson fork 流程上，保持 `clockspeed=10`。主机会把 ESP 请求的 `26 MHz` runtime 重配置钳制回 `10 MHz`。

### 总线存在，但 ESP-Hosted 驱动无法占用它

可能原因：

- 通用 `spidev` 仍占用内核模块所需的设备树路径

### Wi-Fi 接口出现但不稳定

按以下顺序排查：

1. 将 SPI 频率降到 1 MHz
2. 缩短连线
3. 复查共地
4. 确认 ESP 板电源干净
5. 用靠近测试台的简单 AP 重新测试

### 传输可用，但性能偏弱

早期这是正常的。通路稳定后：

- 逐步提高 SPI 时钟
- 测量每次改动
- 记录哪个频率开始失效

---

## 10. 进阶目标

- 增加 AP 模式支持，把 ESP32-C6 用作 Jetson 的配网射频
- 记录基于同一 ESP-Hosted 协议栈版本的 GATT 工作流与 Wi-Fi/BLE 共存测试
- 用小型适配器板或定制载板互连替换杜邦线
- 增加设备树 overlay 与脚本，使 Jetson 配置可复现
- 以 USB Wi-Fi 网卡为基线，测量功耗、延迟与吞吐

---

## 11. 参考资料

### 官方上游参考资料

- [Espressif esp-hosted 仓库](https://github.com/espressif/esp-hosted)
- [ESP-Hosted-NG 设置指南](https://github.com/espressif/esp-hosted/blob/master/esp_hosted_ng/docs/setup.md)
- [ESP-Hosted-NG SPI 协议说明](https://github.com/espressif/esp-hosted/blob/master/esp_hosted_ng/docs/spi_protocol.md)
- [ESP-Hosted-NG Linux 移植指南](https://github.com/espressif/esp-hosted/blob/master/esp_hosted_ng/docs/porting_guide.md)
- [ESP32-C6-DevKitC-1 用户指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/user_guide.html)
- [ESP32-C6 建立串口连接](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/establish-serial-connection.html)
- [ESP32-C6 USB Serial/JTAG 控制台](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/usb-serial-jtag-console.html)
- [面向 AI-HPC Jetson 的 jetson-esp-hosted fork](https://github.com/ai-hpc/jetson-esp-hosted)

### 本地路线图参考资料

- [网络与连接](Guide.md)
- [外设访问](../01-外设访问/Guide.md)
- [Orin Nano GPIO/SPI/I2C/CAN 深入剖析](../../01-Nvidia-Jetson平台/05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)


<details>
<summary>English original</summary>

**8.4 BLE discovery smoke test**

The ESP32-C6 host path here is **BLE-only**, not classic Bluetooth audio.

```bash
rfkill list
bluetoothctl
```

Inside `bluetoothctl`:

```text
power on
scan on
devices
show
```

Expected result:

- `scan on` succeeds
- nearby BLE devices appear in `devices`
- `show` lists the controller roles and advertising features

On the validated flow for this guide, `bluetoothctl` successfully discovered nearby BLE devices from the ESP32-C6 controller exposed over SPI.

If `rfkill` shows Wi-Fi as soft-blocked but Bluetooth as unblocked, BLE scan can still work.

You may also see one of these during early BLE bring-up:

- `Can't read local name on hci0: Input/output error (5)`
- `Failed to set local name: Failed (0x03)`

Those lines are not ideal, but they did not block BLE scan and discovery on the validated Jetson flow here.

**8.5 Basic throughput smoke test**

On another machine:

```bash
iperf3 -s
```

On the Jetson:

```bash
iperf3 -c <server-ip>
```

Do not optimize too early. First prove:

- stable scan
- stable association
- clean packet flow
- no repeated transport resets

---

**9. Common failure modes**

**No first event in `dmesg`**

Usually one of these:

- you loaded the host with `resetpin=-1` but did not manually reset the ESP yet
- reset GPIO is wrong
- handshake or data-ready GPIO is mapped incorrectly
- SPI mode or clock is too aggressive
- ESP firmware is not actually built for SPI

If this happens right after a fresh flash attempt, also make sure the ESP was not being held in reset by the Jetson reset wire during flashing.

**First event appears, then traffic stalls**

Common causes:

- handshake/data-ready interrupts are not configured correctly on the host
- GPIO edge polarity is wrong
- jumper wires are too long or noisy
- the host driver and ESP firmware are from inconsistent revisions
- the host jumped to a higher SPI clock than the wiring can support

On the validated Jetson fork flow in this guide, keep `clockspeed=10`. The host clamps the ESP's requested `26 MHz` runtime reconfigure back down to `10 MHz`.

**Bus exists, but the ESP-Hosted driver cannot claim it**

Likely cause:

- generic `spidev` still owns the device tree path that the kernel module needs

**Wi-Fi interface appears but is unstable**

Work through this order:

1. Lower SPI frequency to 1 MHz
2. Shorten the wires
3. Re-check shared ground
4. Confirm the ESP board power is clean
5. Re-test with a simple AP close to the bench

**Transport works, but performance is weak**

That is normal early on. Once the path is stable:

- increase SPI clock stepwise
- measure each change
- keep notes on which frequency starts to fail

---

**10. Stretch goals**

- add AP mode support and use the ESP32-C6 as the Jetson's provisioning radio
- document GATT workflows and Wi-Fi/BLE coexistence testing from the same ESP-Hosted stack revision
- replace jumper wires with a small adapter board or custom carrier interconnect
- add a device-tree overlay and scripts so the Jetson setup becomes reproducible
- measure power, latency, and throughput against a USB Wi-Fi dongle baseline

---

**11. References**

**Official upstream references**

- [Espressif esp-hosted repository](https://github.com/espressif/esp-hosted)
- [ESP-Hosted-NG setup guide](https://github.com/espressif/esp-hosted/blob/master/esp_hosted_ng/docs/setup.md)
- [ESP-Hosted-NG SPI protocol notes](https://github.com/espressif/esp-hosted/blob/master/esp_hosted_ng/docs/spi_protocol.md)
- [ESP-Hosted-NG Linux porting guide](https://github.com/espressif/esp-hosted/blob/master/esp_hosted_ng/docs/porting_guide.md)
- [ESP32-C6-DevKitC-1 user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/user_guide.html)
- [ESP32-C6 establish serial connection](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/establish-serial-connection.html)
- [ESP32-C6 USB Serial/JTAG console](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/usb-serial-jtag-console.html)
- [AI-HPC Jetson-oriented jetson-esp-hosted fork](https://github.com/ai-hpc/jetson-esp-hosted)

**Local roadmap references**

- [Network and Connectivity](Guide.md)
- [Peripheral Access](../01-外设访问/Guide.md)
- [Orin Nano GPIO/SPI/I2C/CAN deep-dive](../../01-Nvidia-Jetson平台/05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/2. Network and Connectivity/ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/2.%20Network%20and%20Connectivity/ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
