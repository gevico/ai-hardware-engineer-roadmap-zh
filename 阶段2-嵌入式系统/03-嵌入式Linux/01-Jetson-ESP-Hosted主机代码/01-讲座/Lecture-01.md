---
title: 第 1 讲 — 这套 host stack 的 Linux 心智模型
description: 第 1 讲 — 这套 host stack 的 Linux 心智模型
---

# 第 1 讲 — 这套 host stack 的 Linux 心智模型

**课程：** [Jetson ESP-Hosted Host Code 指南](../Guide.md) · **阶段 2 — 嵌入式 Linux**

**下一讲：** [第 02 讲 — 构建、加载与板级策略](Lecture-02.md)

---

## 1. Linux 认为正在发生什么

从 Linux 用户态看，经过验证的 Jetson 流程最终显得很平常：

- `wlan0` 存在
- `hci0` 存在
- `nmcli` 能扫描 Wi-Fi
- `bluetoothctl` 能扫描 BLE 设备

但 Linux 并不是通过 PCIe 或 USB 与原生 Wi-Fi 或蓝牙芯片通信，而是通过一套定制的 **SPI transport** 与 **ESP32-C6** 通信。

所以真正的问题是：

普通的 Linux 抽象，是如何从一套非普通的 transport 中产生的？

答案就在 `esp_hosted_ng/host/` 中的 host stack。

## 相关的 Linux 内核概念

把此前几讲 OS 课程作为本案例研究的概念基础：

- [OS 第 1 讲 — 现代 OS 架构与 Linux 内核](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)
- [OS 第 5 讲 — 内核模块、启动流程与设备树](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md)

OS 课程的第 1 讲把内核解释为**拥有硬件、并向用户态暴露干净抽象**的那一层。这正是这套 host stack 在做的事：它把定制的 ESP-over-SPI 链路隐藏起来，转而向 Linux 提供 `wlan0` 和 `hci0` 这样的常规对象。

第 5 讲在这里很关键，因为只有在 boot 已经描述了板子并加载了正确的驱动路径之后，这段代码才说得通。ESP-Hosted 的 host 代码并不是从零开始发明硬件；它坐落在一个已经了解 SPI 设备、模块和板级描述的内核之上。

换种说法：用户态不能把「与 GPIO 471 对话」或「与 SPI mode 2 对话」直接当作网络模型来用。内核驱动吸收了这些底层细节，并发布更高层的子系统对象，于是用户态可以处理 Wi‑Fi 和蓝牙概念，而不是 transport 接线。

这就是为什么 `wlan0` 和 `hci0` 作为 **bring-up 历程中的里程碑**如此重要。它们不仅证明字节在 SPI 上跑通了，还证明内核接受了驱动与**真实 Linux 子系统**的集成，并向上暴露了预期的接口。

本讲需要记住的面向 Linux 的接口有：

- `cfg80211`
- HCI / BlueZ
- `wireless_dev`
- `wiphy`
- `net_device`

---

## 2. 分层图景

自上而下读：

1. 用户态工具
2. Linux 子系统 API
3. host 驱动集成代码
4. transport 代码
5. ESP 侧固件

在这个代码库中，对应为：

- 用户态：
  - `nmcli`
  - `iw`
  - `bluetoothctl`
  - `hciconfig`
- Linux 子系统：
  - `cfg80211` 负责 Wi-Fi
  - HCI / BlueZ 负责蓝牙/BLE
- host 集成：
  - `esp_cfg80211.c`
  - `esp_bt.c`
  - `main.c`
- transport：
  - `spi/esp_spi.c`
- 板级 bring-up 策略：
  - `jetson_orin_nano_init.sh`

这就是第一条 **嵌入式 Linux 经验**：

- 面向 Linux 的代码并不等同于
- 面向硬件的 transport 代码

如果把两者混为一谈，读驱动代码很快就会变得混乱。

### 代码锚点：一次 boot 事件生成两种 Linux 身份

观察这一拆分最清晰的地方是 `main.c`。单次 ESP 启动事件最终会扇出为：

- 通过 `esp_add_card(...)` 的 Wi-Fi 注册
- 通过 `init_bt(...)` 的蓝牙注册

```c
static void init_bt(struct esp_adapter *adapter)
{
	if ((adapter->capabilities & ESP_BT_SPI_SUPPORT) ||
		(adapter->capabilities & ESP_BT_SDIO_SUPPORT)) {
		msleep(200);
		esp_info("ESP Bluetooth init\n");
		esp_init_bt(adapter);
	}
}

static int process_event_esp_bootup(struct esp_adapter *adapter, u8 *evt_buf, u8 len)
{
	...
	if (esp_add_card(adapter)) {
		esp_err("network interface init failed\n");
		return -1;
	}
	init_bt(adapter);
	...
	print_capabilities(adapter->capabilities);
}
```

而 Wi-Fi 一侧被显式创建为普通的 Linux station 接口：

```c
static int esp_add_network_ifaces(struct esp_adapter *adapter)
{
	struct wireless_dev *wdev = NULL;

	rtnl_lock();
	wdev = esp_cfg80211_add_iface(adapter->wiphy, "wlan%d", 1,
				      NL80211_IFTYPE_STATION, NULL);
	rtnl_unlock();

	if (wdev)
		return 0;

	return -1;
}
```

这就是**整门课程的缩影**：

- 一颗远端芯片
- 一套 transport
- 两个不同的 Linux 子系统身份

---


<details>
<summary>English original</summary>

**Lecture 1 — Linux mental model for this host stack**

**Course:** [Jetson ESP-Hosted Host Code guide](../Guide.md) · **Phase 2 — Embedded Linux**

**Next:** [Lecture 02 — Build, load, and board policy](Lecture-02.md)

---

**1. What Linux thinks is happening**

From Linux userspace, the validated Jetson flow eventually looks ordinary:

- `wlan0` exists
- `hci0` exists
- `nmcli` can scan Wi-Fi
- `bluetoothctl` can scan BLE devices

But Linux is not talking to a native Wi-Fi or Bluetooth chip over PCIe or USB. It is talking to an **ESP32-C6** over a custom **SPI transport**.

So the real question is:

How do ordinary Linux abstractions come out of a non-ordinary transport?

The answer is the host stack in `esp_hosted_ng/host/`.

**Related Linux kernel concepts**

Use these earlier OS lectures as the conceptual base under this case study:

- [OS Lecture 1 — Modern OS Architecture & the Linux Kernel](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)
- [OS Lecture 5 — Kernel Modules, Boot Process & Device Tree](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md)

Lecture 1 from the OS course explains the kernel as the layer that **owns hardware and exposes clean abstractions** to userspace. That is exactly what this host stack is doing: it hides a custom ESP-over-SPI link and instead gives Linux normal objects like `wlan0` and `hci0`.

Lecture 5 matters here because this code only makes sense after boot has already described the board and loaded the right driver path. The ESP-Hosted host code is not inventing hardware from scratch; it is sitting on top of a kernel that already knows about SPI devices, modules, and board description.

Another way to say it is: userspace does not get to “talk to GPIO 471” or “talk to SPI mode 2” directly as a networking model. The kernel driver absorbs those low-level details and publishes higher-level subsystem objects, so userspace can work with Wi‑Fi and Bluetooth concepts instead of transport wiring.

This is why `wlan0` and `hci0` are such important **milestones in the bring-up story**. They prove not just that bytes moved over SPI, but that the kernel accepted the driver’s integration with **real Linux subsystems** and exposed the expected interfaces upward.

The Linux-facing interfaces to keep in mind in this lecture are:

- `cfg80211`
- HCI / BlueZ
- `wireless_dev`
- `wiphy`
- `net_device`

---

**2. The layered picture**

Read this from top to bottom:

1. userspace tools
2. Linux subsystem APIs
3. host driver integration code
4. transport code
5. ESP side firmware

In this codebase, that becomes:

- userspace:
  - `nmcli`
  - `iw`
  - `bluetoothctl`
  - `hciconfig`
- Linux subsystems:
  - `cfg80211` for Wi-Fi
  - HCI / BlueZ for Bluetooth/BLE
- host integration:
  - `esp_cfg80211.c`
  - `esp_bt.c`
  - `main.c`
- transport:
  - `spi/esp_spi.c`
- board bring-up policy:
  - `jetson_orin_nano_init.sh`

That is the first **Embedded Linux lesson**:

- the Linux-facing code is not the same thing as
- the hardware-facing transport code

If you blur those together, driver reading becomes confusing fast.

**Code anchor: one boot event creates two Linux personalities**

The cleanest place to see the split is in `main.c`. A single ESP boot-up event eventually fans out into:

- Wi-Fi registration through `esp_add_card(...)`
- Bluetooth registration through `init_bt(...)`

```c
static void init_bt(struct esp_adapter *adapter)
{
	if ((adapter->capabilities & ESP_BT_SPI_SUPPORT) ||
		(adapter->capabilities & ESP_BT_SDIO_SUPPORT)) {
		msleep(200);
		esp_info("ESP Bluetooth init\n");
		esp_init_bt(adapter);
	}
}

static int process_event_esp_bootup(struct esp_adapter *adapter, u8 *evt_buf, u8 len)
{
	...
	if (esp_add_card(adapter)) {
		esp_err("network interface init failed\n");
		return -1;
	}
	init_bt(adapter);
	...
	print_capabilities(adapter->capabilities);
}
```

And the Wi-Fi side is explicitly created as a normal Linux station interface:

```c
static int esp_add_network_ifaces(struct esp_adapter *adapter)
{
	struct wireless_dev *wdev = NULL;

	rtnl_lock();
	wdev = esp_cfg80211_add_iface(adapter->wiphy, "wlan%d", 1,
				      NL80211_IFTYPE_STATION, NULL);
	rtnl_unlock();

	if (wdev)
		return 0;

	return -1;
}
```

That is the **whole course in miniature**:

- one remote chip
- one transport
- two different Linux subsystem identities

---

</details>

## 3. 真正有效的文件阅读顺序

按以下顺序读：

1. `jetson_orin_nano_init.sh`
2. `Makefile`
3. `main.c`
4. `spi/esp_spi.c`
5. `esp_cfg80211.c`
6. `esp_bt.c`

这个顺序为什么有效：

- shell 脚本告诉你板级假设
- `Makefile` 告诉你内核模块的形态
- `main.c` 展示生命周期
- `esp_spi.c` 展示字节究竟怎么流动
- `esp_cfg80211.c` 展示 Linux Wi-Fi 如何出现
- `esp_bt.c` 展示 Linux Bluetooth 如何出现

不要从 `esp_spi.c` 的中间读起，除非你已经知道 host 想要产出什么。

---

## 4. host 代码真正在产出什么

host 代码不只是“一个内核模块”。

它在产出两种 Linux 身份：

- 通过 `cfg80211` 呈现为一个 Wi-Fi 设备
- 通过 Bluetooth 协议栈呈现为一个 HCI 控制器

这意味着这个仓库实质上是对 **Linux 子系统集成** 的研究。

理论上传输层是可以换的：

- SPI
- SDIO
- UART

但 Linux 看待 Wi-Fi 和 Bluetooth 的方式，仍然必须映射到同样的上层子系统预期上。

这就是仓库要拆分的原因：

- 公共逻辑
- 传输相关逻辑
- Linux 集成逻辑

---

## 5. 经过验证的 runtime 流程

在已验证的 Jetson 路径上，关键的 runtime 阶段是：

1. 模块插入
2. SPI 设备被认领
3. 握手/data-ready IRQ 挂接
4. 收到 ESP boot-up 事件
5. 通过 SPI 识别 chipset
6. Wi-Fi 接口创建
7. Bluetooth 控制器创建

这个顺序很重要。

对嵌入式 Linux 调试来说，**“模块插入成功”并不够**。

你需要知道：

- 协议事件到了吗？
- 上层子系统注册了吗？
- Linux 真的拿到一个接口了吗？

这样才能在 bring-up（上电点亮/调通）过程中避免假成功。

---

## 6. 阅读时应当打开的具体文件

把这些文件并排打开：

- `esp_hosted_ng/host/jetson_orin_nano_init.sh`
- `esp_hosted_ng/host/Makefile`
- `esp_hosted_ng/host/main.c`
- `esp_hosted_ng/host/spi/esp_spi.c`
- `esp_hosted_ng/host/esp_cfg80211.c`
- `esp_hosted_ng/host/esp_bt.c`

边读边问：

- 这个文件在和哪个 Linux 子系统打交道？
- 它编码了哪条硬件假设？
- 哪一行日志能证明这个阶段工作了？

这个习惯才是本课程真正的目标。

---

## Lab

写一页说明，回答：

1. 哪些文件主要属于 **板级策略**？
2. 哪些文件主要属于 **传输逻辑**？
3. 哪些文件主要属于 **Linux 子系统集成**？
4. 为什么 `wlan0` 的含义多于 `/dev/spidev0.0`？

---

**上一篇：** [Course hub](../Guide.md) · **下一篇：** [Lecture 02 — Build, load, and board policy](Lecture-02.md)


<details>
<summary>English original</summary>

**3. The file-reading order that actually works**

Use this order:

1. `jetson_orin_nano_init.sh`
2. `Makefile`
3. `main.c`
4. `spi/esp_spi.c`
5. `esp_cfg80211.c`
6. `esp_bt.c`

Why this order works:

- the shell script tells you the board assumptions
- the `Makefile` tells you the kernel module shape
- `main.c` shows the lifecycle
- `esp_spi.c` shows how the bytes actually move
- `esp_cfg80211.c` shows how Linux Wi-Fi appears
- `esp_bt.c` shows how Linux Bluetooth appears

Do not start in the middle of `esp_spi.c` unless you already know what the host is trying to produce.

---

**4. What the host code is really producing**

The host code is not just “a kernel module.”

It is producing two Linux personalities:

- a Wi-Fi device through `cfg80211`
- an HCI controller through the Bluetooth stack

That means this repo is really a study in **Linux subsystem integration**.

The transport could change in theory:

- SPI
- SDIO
- UART

But the way Linux sees Wi-Fi and Bluetooth would still need to map into the same upper subsystem expectations.

That is why the repo splits:

- common logic
- transport-specific logic
- Linux integration logic

---

**5. The validated runtime story**

On the validated Jetson path, the important runtime stages were:

1. module inserted
2. SPI device claimed
3. handshake/data-ready IRQs attached
4. ESP boot-up event received
5. chipset identified over SPI
6. Wi-Fi interface created
7. Bluetooth controller created

That sequence matters.

For Embedded Linux debugging, **“module inserted successfully” is not enough**.

You want to know:

- did the protocol event arrive?
- did the upper subsystem register?
- did Linux gain a real interface?

This is how you avoid false success during bring-up.

---

**6. The exact files to have open while reading**

Keep these open side by side:

- `esp_hosted_ng/host/jetson_orin_nano_init.sh`
- `esp_hosted_ng/host/Makefile`
- `esp_hosted_ng/host/main.c`
- `esp_hosted_ng/host/spi/esp_spi.c`
- `esp_hosted_ng/host/esp_cfg80211.c`
- `esp_hosted_ng/host/esp_bt.c`

As you read, ask:

- what Linux subsystem is this file talking to?
- what hardware assumption does it encode?
- what log line would prove this stage worked?

That habit is the real course objective.

---

**Lab**

Write a one-page note that answers:

1. Which files are mostly **board policy**?
2. Which files are mostly **transport logic**?
3. Which files are mostly **Linux subsystem integration**?
4. Why does `wlan0` mean more than `/dev/spidev0.0`?

---

**Previous:** [Course hub](../Guide.md) · **Next:** [Lecture 02 — Build, load, and board policy](Lecture-02.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Jetson ESP-Hosted Host Code/Lecture/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Jetson%20ESP-Hosted%20Host%20Code/Lecture/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
