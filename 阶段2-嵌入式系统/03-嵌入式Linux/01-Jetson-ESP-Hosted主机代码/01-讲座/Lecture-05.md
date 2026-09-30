---
title: 第 5 讲 — BLE 如何变成 hci0，以及如何验证整条路径
description: 第 5 讲 — BLE 如何变成 hci0，以及如何验证整条路径
---

# 第 5 讲 — BLE 如何变成 `hci0`，以及如何验证整条路径

**课程：** [Jetson ESP-Hosted 主机代码指南](../Guide.md) · **阶段 2 — 嵌入式 Linux**

**上一讲：** [第 04 讲](Lecture-04.md)

---

## 1. 蓝牙侧是独立的 Linux 子系统

Wi-Fi 和蓝牙走的是**同一个主机传输通道**，但 Linux 并不把它们当作同一回事。

对 BLE 来说，重要的文件是：

- `esp_hosted_ng/host/esp_bt.c`

该文件接入的是 Linux HCI 子系统，而不是 `cfg80211`。

这就是嵌入式 Linux 的关键一课：

- 一个传输通道
- 多个 Linux 子系统身份

## 相关的 Linux 内核概念

本讲最直接关联到：

- [OS 第 1 讲 — 现代 OS 架构与 Linux 内核](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)
- [OS 第 17 讲 — Linux 设备驱动模型与设备树](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)

第 1 讲在这里有用，是因为 HCI 是内核在非比寻常的硬件之上暴露标准抽象的又一例。ESP 依然是远端设备，依然挂在 SPI 之后，但一旦 `hci_register_dev()` 成功，Linux 蓝牙工具就能把它当作普通控制器对待。

第 17 讲之所以重要，是因为蓝牙侧同样遵循与 Wi-Fi 侧相同的对象注册模式，只是处在不同的子系统里。关键的转变是从“有一个传输报文到达”变为“内核把该报文当作 HCI 流量接收，并交给蓝牙协议栈”。

HCI 抽象之所以重要，是因为 Linux 中的蓝牙是围绕**主机/控制器分离**构建的。控制器可以挂在 USB、UART、SDIO 上，在本例中则是 SPI，但一旦驱动把报文送入 HCI 层，**BlueZ** 就能管理发现与连接，无须关心底层总线的细节。

正因如此，BLE 验证不能止步于“`hci0` 存在”。控制器可见只能证明注册成功；能扫描成功才证明 HCI 命令、事件和数据报文正确地穿过传输通道、回到 Linux 蓝牙协议栈。

需要关注的内核接口有：

- `hci_alloc_dev()`
- `hci_register_dev()`
- `hci_recv_frame(...)`
- 用 `HCI_SPI` 进行 HCI 总线类型设置

---

## 2. `hci0` 的确切代码路径

阅读以下函数：

- `esp_init_bt(...)`
- `esp_deinit_bt(...)`
- `esp_bt_send_frame(...)`

在 `esp_init_bt(...)` 中，驱动会：

- 分配一个 HCI 设备
- 将其与适配器关联
- 设置 HCI 总线类型
- 注册该 HCI 设备

在 SPI 路径上，总线类型变为：

- `HCI_SPI`

正因如此，验证过的 Jetson 日志和工具显示：

- `hci0`
- `Bus: SPI`

这不是表面功夫。它意味着 Linux 蓝牙工具集现在看到的是一个**标准控制器接口**。

`esp_bt.c` 中的关键代码块很短，而且非常直白：

```c
int esp_init_bt(struct esp_adapter *adapter)
{
	...
	hdev = hci_alloc_dev();
	...
	adapter->hcidev = hdev;
	hci_set_drvdata(hdev, adapter);

	hdev->bus = INVALID_HDEV_BUS;
	...
	else if (adapter->if_type == ESP_IF_TYPE_SPI)
		hdev->bus = HCI_SPI;

	hdev->open  = esp_bt_open;
	hdev->close = esp_bt_close;
	hdev->flush = esp_bt_flush;
	hdev->send  = esp_bt_send_frame;
	...
	ret = hci_register_dev(hdev);
	...
}
```

这一刻正是“来自远端 ESP 的蓝牙报文”变成“一个正常的 Linux HCI 控制器”的确切时刻。

---

## 3. 收到的 BLE 数据如何到达 Linux

现在回到 `main.c`，重读 `process_rx_packet(...)` 的一部分。

该函数会检查传入载荷的类型。

当报文属于蓝牙时：

- `payload_header->if_type == ESP_HCI_IF`

代码会用以下方式把数据转发给 HCI 协议栈：

- `hci_recv_frame(...)`

这就是关键的交接：

- 来自 ESP 的传输报文
- 变成 Linux 中的 HCI 流量

这正是自定义硬件路径被**转化为标准子系统视图**的方式。

`main.c` 中的交接是直接的：

```c
} else if (payload_header->if_type == ESP_HCI_IF) {
	if (hdev) {
		type = skb->data;
		hci_skb_pkt_type(skb) = *type;
		skb_pull(skb, 1);

		if (hci_recv_frame(hdev, skb)) {
			hdev->stat.err_rx++;
		} else {
			esp_hci_update_rx_counter(hdev, *type, skb->len);
		}
	}
}
```

该传输通道并不为 BLE 暴露自定义的用户 API。它把远端报文转换成 BlueZ 已经理解的、标准的 HCI 接收路径。


<details>
<summary>English original</summary>

**Lecture 5 — How BLE becomes `hci0` and how to validate the full path**

**Course:** [Jetson ESP-Hosted Host Code guide](../Guide.md) · **Phase 2 — Embedded Linux**

**Previous:** [Lecture 04](Lecture-04.md)

---

**1. The Bluetooth side is a separate Linux subsystem**

Wi-Fi and Bluetooth ride over the **same host transport**, but Linux does not treat them as the same thing.

For BLE, the important file is:

- `esp_hosted_ng/host/esp_bt.c`

This file integrates with the Linux HCI subsystem instead of `cfg80211`.

That is the key Embedded Linux lesson:

- one transport
- multiple Linux subsystem personalities

**Related Linux kernel concepts**

This lecture connects most directly to:

- [OS Lecture 1 — Modern OS Architecture & the Linux Kernel](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)
- [OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)

Lecture 1 is useful here because HCI is another example of the kernel exposing a standard abstraction over unusual hardware. The ESP is still remote and still behind SPI, but once `hci_register_dev()` succeeds Linux Bluetooth tools can treat it like a normal controller.

Lecture 17 matters because the Bluetooth side also follows the same object-registration pattern as the Wi-Fi side, just in a different subsystem. The important shift is from “a transport packet arrived” to “the kernel accepted that packet as HCI traffic and handed it to the Bluetooth stack.”

The HCI abstraction matters because Bluetooth in Linux is built around a **host/controller split**. The controller may be on USB, UART, SDIO, or in this case SPI, but once the driver feeds packets into the HCI layer, **BlueZ** can manage discovery and connections without caring about the underlying bus details.

That is why BLE validation cannot stop at “`hci0` exists.” A visible controller only proves registration; scanning successfully proves that HCI commands, events, and data packets are flowing correctly across the transport and back into the Linux Bluetooth stack.

The kernel interfaces to focus on are:

- `hci_alloc_dev()`
- `hci_register_dev()`
- `hci_recv_frame(...)`
- HCI bus typing with `HCI_SPI`

---

**2. The exact code path for `hci0`**

Read these functions:

- `esp_init_bt(...)`
- `esp_deinit_bt(...)`
- `esp_bt_send_frame(...)`

In `esp_init_bt(...)`, the driver:

- allocates an HCI device
- associates it with the adapter
- sets the HCI bus type
- registers the HCI device

On the SPI path, the bus becomes:

- `HCI_SPI`

That is why the validated Jetson logs and tools showed:

- `hci0`
- `Bus: SPI`

This is not cosmetic. It means Linux Bluetooth tooling now sees a **standard controller interface**.

The key block in `esp_bt.c` is short and very literal:

```c
int esp_init_bt(struct esp_adapter *adapter)
{
	...
	hdev = hci_alloc_dev();
	...
	adapter->hcidev = hdev;
	hci_set_drvdata(hdev, adapter);

	hdev->bus = INVALID_HDEV_BUS;
	...
	else if (adapter->if_type == ESP_IF_TYPE_SPI)
		hdev->bus = HCI_SPI;

	hdev->open  = esp_bt_open;
	hdev->close = esp_bt_close;
	hdev->flush = esp_bt_flush;
	hdev->send  = esp_bt_send_frame;
	...
	ret = hci_register_dev(hdev);
	...
}
```

That is the precise moment when “Bluetooth packets from a remote ESP” become “a normal Linux HCI controller.”

---

**3. How incoming BLE data reaches Linux**

Now return to `main.c` and re-read part of `process_rx_packet(...)`.

That function checks the incoming payload type.

When the packet is for Bluetooth:

- `payload_header->if_type == ESP_HCI_IF`

the code forwards the data to the HCI stack using:

- `hci_recv_frame(...)`

This is the crucial handoff:

- transport packet from ESP
- becomes HCI traffic in Linux

That is exactly how a custom hardware path gets **translated into a standard subsystem view**.

The handoff in `main.c` is direct:

```c
} else if (payload_header->if_type == ESP_HCI_IF) {
	if (hdev) {
		type = skb->data;
		hci_skb_pkt_type(skb) = *type;
		skb_pull(skb, 1);

		if (hci_recv_frame(hdev, skb)) {
			hdev->stat.err_rx++;
		} else {
			esp_hci_update_rx_counter(hdev, *type, skb->len);
		}
	}
}
```

The transport does not expose a custom user API for BLE. It converts remote packets into the standard HCI receive path that BlueZ already understands.

---

</details>

## 4. capability 日志在告诉你什么

在 `main.c` 中，capability 打印输出告诉你 ESP 声称支持什么。

在已验证的 Jetson 路径上，日志显示：

- `BT/BLE`
- `HCI over SPI`
- `BLE only`

最后一点对 **ESP32-C6** 很重要：

- 这条路径是 **BLE-only**
- 不要指望这种配置能提供经典蓝牙音频 profile

这是**实际的产品约束**，不只是代码细节。

capability 上报来自 `main.c`，值得在真实代码里扫一遍：

```c
if (cap & ESP_BT_SPI_SUPPORT)
	esp_info("\t   - HCI over SPI\n");

if ((cap & ESP_BLE_ONLY_SUPPORT) && (cap & ESP_BR_EDR_ONLY_SUPPORT))
	esp_info("\t   - BT/BLE dual mode\n");
else if (cap & ESP_BLE_ONLY_SUPPORT)
	esp_info("\t   - BLE only\n");
else if (cap & ESP_BR_EDR_ONLY_SUPPORT)
	esp_info("\t   - BR EDR only\n");
```

那些日志行不是装饰。它们是驱动在告诉你，远端固件希望 Linux 如何对待这个 radio。

---

## 5. 已验证的 Linux 侧 BLE 证明

在真实硬件上，验证路径是：

- `hciconfig -a`
- `bluetoothctl list`
- `bluetoothctl`
- `scan on`

而证明点是：

- `hci0` 存在
- BlueZ 识别出该 controller
- controller 总线报告了 `SPI`
- BLE 扫描发现了附近设备

这意味着 BLE 路径不只是注册上了。它是**可用的**。

这一点很重要：

- `hci0` 出现是好事
- `scan on` 能找到设备要好得多

这和你之前在 Wi-Fi 上看到的教训一样：

- 出现不等于行为经过验证

---

## 6. 这套代码库的端到端成功标准

对这个 Jetson 案例研究来说，完整成功意味着：

- transport 起来
- 收到 boot-up 事件
- chipset 验证通过
- `wlan0` 创建
- Wi-Fi 扫描可用
- `hci0` 创建
- BLE 扫描可用

这才是正确的嵌入式 Linux 思维：

- 在操作系统真正关心的子系统层面做验证
- 而不只是在信号或模块插入层面

---

## 最终实验

回答这些问题：

1. HCI 设备在哪里分配？
2. HCI 总线类型在哪里被设为 SPI？
3. 传入的蓝牙数据包在哪里交给 Linux？
4. 为什么 `scan on` 是比仅有 `bluetoothctl list` 更强的证据？
5. 为什么这个 repo 是子系统集成方面一个很好的嵌入式 Linux 案例研究？

可选扩展：

- 写一段简短对比：
  - Wi-Fi 的 `cfg80211` 集成
  - BLE 的 HCI 集成
- 指出两者之间哪些是共享的，哪些是子系统特有的

---

**Previous:** [Lecture 04](Lecture-04.md) · **Back to:** [Course hub](../Guide.md)


<details>
<summary>English original</summary>

**4. What the capability logs are telling you**

In `main.c`, the capability printout tells you what the ESP claims to support.

On the validated Jetson path, the logs showed:

- `BT/BLE`
- `HCI over SPI`
- `BLE only`

That last point matters for **ESP32-C6**:

- this path is **BLE-only**
- do not expect classic Bluetooth audio profiles from this configuration

That is a **practical product constraint**, not just a code detail.

The capability reporting comes from `main.c` and is worth scanning once in real code:

```c
if (cap & ESP_BT_SPI_SUPPORT)
	esp_info("\t   - HCI over SPI\n");

if ((cap & ESP_BLE_ONLY_SUPPORT) && (cap & ESP_BR_EDR_ONLY_SUPPORT))
	esp_info("\t   - BT/BLE dual mode\n");
else if (cap & ESP_BLE_ONLY_SUPPORT)
	esp_info("\t   - BLE only\n");
else if (cap & ESP_BR_EDR_ONLY_SUPPORT)
	esp_info("\t   - BR EDR only\n");
```

Those log lines are not decoration. They are the driver telling you how the remote firmware wants Linux to treat the radio.

---

**5. The validated Linux-side BLE proof**

On real hardware, the validation path was:

- `hciconfig -a`
- `bluetoothctl list`
- `bluetoothctl`
- `scan on`

And the proof points were:

- `hci0` existed
- BlueZ recognized the controller
- the controller bus reported `SPI`
- BLE scan discovered nearby devices

That means the BLE path was not just registered. It was **functional**.

This is important:

- `hci0` appearing is good
- `scan on` finding devices is much better

That is the same lesson you saw with Wi-Fi:

- appearance is not the same as validated behavior

---

**6. End-to-end success criteria for this codebase**

For this Jetson case study, full success means:

- transport up
- boot-up event received
- chipset validated
- `wlan0` created
- Wi-Fi scan works
- `hci0` created
- BLE scan works

That is the right Embedded Linux mindset:

- validate at the subsystem level the operating system actually cares about
- not just at the signal or module-insert level

---

**Final lab**

Answer these:

1. Where is the HCI device allocated?
2. Where is the HCI bus type set to SPI?
3. Where do incoming Bluetooth packets get handed to Linux?
4. Why is `scan on` stronger evidence than `bluetoothctl list` alone?
5. Why does this repo make a good Embedded Linux case study for subsystem integration?

Optional extension:

- write a short comparison of:
  - `cfg80211` integration for Wi-Fi
  - HCI integration for BLE
- identify what is shared between them and what is subsystem-specific

---

**Previous:** [Lecture 04](Lecture-04.md) · **Back to:** [Course hub](../Guide.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Jetson ESP-Hosted Host Code/Lecture/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Jetson%20ESP-Hosted%20Host%20Code/Lecture/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
