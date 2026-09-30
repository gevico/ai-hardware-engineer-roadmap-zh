---
title: 第 4 讲 — Wi-Fi 如何变成 wlan0
description: 第 4 讲 — Wi-Fi 如何变成 wlan0
---

# 第 4 讲 — Wi-Fi 如何变成 `wlan0`

**课程：** [Jetson ESP-Hosted Host Code guide](../Guide.md) · **阶段 2 — 嵌入式 Linux**

**上一讲：** [第 3 讲](Lecture-03.md) · **下一讲：** [第 5 讲 — 低功耗蓝牙（BLE）如何变成 `hci0`](Lecture-05.md)

---

## 1. Linux 不想要「一个 SPI Wi-Fi 小玩意」

Linux 用户态工具既不知道、也不关心你的无线芯片挂在一个自定义 SPI 传输层后面。

它们要的是：

- 一个已注册的无线设备
- `wiphy`
- `wireless_dev`
- 一个用户态可管理的网络设备

这个映射发生在：

- `esp_hosted_ng/host/esp_cfg80211.c`
- 再加上 `esp_hosted_ng/host/main.c` 中的编排

## 相关的 Linux 内核概念

本讲最好与以下内容对照阅读：

- [OS 第 17 讲 — Linux 设备驱动模型与设备树](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)
- [OS 第 1 讲 — 现代 OS 架构与 Linux 内核](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)

第 17 讲是恰当的背景，因为 `wiphy`、`wireless_dev`、`net_device` 都是必须按正确顺序分配和注册的内核对象。`esp_cfg80211.c` 是驱动模型创建 Linux 可见对象的一个具体例子，这些对象代表远端的一块硬件。

第 1 讲之所以重要，是因为用户态从不直接看到传输层细节。`nmcli` 和 `iw` 能工作，只是因为驱动把 ESP 变成了标准的 Linux 无线抽象，而不是把「某个 SPI 无线芯片」作为自定义 API 暴露出去。

有用的心智模型是：`cfg80211` 是**你的驱动与 Linux 无线协议栈之间的一份契约**。驱动必须描述无线芯片能做什么、创建 Linux 期望的对象，并实现 Linux 会为 scan、connect、disconnect 和 regulatory 处理而调用的操作。

这就是为什么 `wlan0` 的出现是一个强信号。它意味着代码早已远超传输层 bring-up（上电点亮/调通），进入了子系统成功注册的阶段——此时内核认为存在一个可管理的无线接口，用户态工具可以正常操作它。

需要记住的内核接口有：

- `wiphy_new(...)`
- `wiphy_register(...)`
- `esp_cfg80211_add_iface(...)`
- `register_netdevice(...)`
- `rtnl_lock()`

---

## 2. 主控制流

在 `main.c` 中，研究：

- `process_esp_bootup_event(...)`
- `process_internal_event(...)`
- `process_rx_packet(...)`
- `esp_add_network_ifaces(...)`

逻辑是：

1. 传输层收到 ESP 启动事件
2. 主机校验芯片型号
3. 主机获知能力
4. 主机初始化面向 Linux 的接口层
5. Wi-Fi 侧完成注册

这正是把**「协议能跑通」变成「Linux 网络接口能跑通」**的关键。

下面是 `main.c` 中确切的编排：

```c
static int process_event_esp_bootup(struct esp_adapter *adapter, u8 *evt_buf, u8 len)
{
	...
	while (len_left > 0) {
		switch (*pos) {
		case ESP_BOOTUP_CAPABILITY:
			adapter->capabilities = *(pos + 2);
			break;
		case ESP_BOOTUP_FIRMWARE_CHIP_ID:
			ret = esp_validate_chipset(adapter, *(pos + 2));
			break;
		case ESP_BOOTUP_FW_DATA:
			fw_p = (struct fw_data *)(pos + 2);
			ret = process_fw_data(fw_p, tag_len);
			break;
		case ESP_BOOTUP_SPI_CLK_MHZ:
			ret = esp_adjust_spi_clock(adapter, *(pos + 2));
			break;
		}
		...
	}

	if (esp_add_card(adapter)) {
		esp_err("network interface init failed\n");
		return -1;
	}
	init_bt(adapter);
	...
}
```

这是代码库中控制面上的重大跃迁：

- 原始启动事件经 SPI 进来
- 驱动得知远端芯片是什么
- 面向 Linux 的接口在此认知之上建立

---

## 3. `wlan0` 创建的确切位置

在 `main.c` 中，`esp_add_network_ifaces(...)` 调用了：

- `esp_cfg80211_add_iface(adapter->wiphy, "wlan%d", 1, NL80211_IFTYPE_STATION, NULL)`

那一刻，代码请求了一个正常的 Linux 无线接口。

这一次调用是一堂极好的嵌入式 Linux 课：

- 传输层不会把原始数据包直接暴露给用户态
- 驱动**把远端无线芯片翻译成标准的 Linux 接口模型**

这就是 `nmcli`、`iw` 以及其他工具能工作的原因。

调用点小到足以背下来：

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

---


<details>
<summary>English original</summary>

**Lecture 4 — How Wi-Fi becomes `wlan0`**

**Course:** [Jetson ESP-Hosted Host Code guide](../Guide.md) · **Phase 2 — Embedded Linux**

**Previous:** [Lecture 03](Lecture-03.md) · **Next:** [Lecture 05 — How BLE becomes `hci0`](Lecture-05.md)

---

**1. Linux does not want “an SPI Wi-Fi gadget”**

Linux userspace tools do not know or care that your radio sits behind a custom SPI transport.

They want:

- a registered wireless device
- a `wiphy`
- a `wireless_dev`
- a net device that userspace can manage

That mapping happens in:

- `esp_hosted_ng/host/esp_cfg80211.c`
- plus the orchestration in `esp_hosted_ng/host/main.c`

**Related Linux kernel concepts**

This lecture is best read alongside:

- [OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)
- [OS Lecture 1 — Modern OS Architecture & the Linux Kernel](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)

Lecture 17 is the right background because `wiphy`, `wireless_dev`, and `net_device` are all kernel objects that must be allocated and registered in the right order. `esp_cfg80211.c` is a concrete example of the driver model creating Linux-visible objects that represent a remote piece of hardware.

Lecture 1 matters because userspace never sees the transport details directly. `nmcli` and `iw` work only because the driver turns the ESP into standard Linux wireless abstractions instead of exposing “some SPI radio” as a custom API.

The useful mental model is that `cfg80211` is a **contract between your driver and the Linux wireless stack**. The driver has to describe what the radio can do, create the objects Linux expects, and implement the operations Linux will call for scan, connect, disconnect, and regulatory handling.

That is why `wlan0` appearing is a strong signal. It means the code has progressed far beyond transport bring-up and into successful subsystem registration, where the kernel now believes there is a manageable wireless interface that userspace tools can operate on normally.

The kernel interfaces to keep in your head are:

- `wiphy_new(...)`
- `wiphy_register(...)`
- `esp_cfg80211_add_iface(...)`
- `register_netdevice(...)`
- `rtnl_lock()`

---

**2. The main control flow**

In `main.c`, study:

- `process_esp_bootup_event(...)`
- `process_internal_event(...)`
- `process_rx_packet(...)`
- `esp_add_network_ifaces(...)`

The logic is:

1. transport receives ESP boot-up event
2. host validates the chipset
3. host learns capabilities
4. host initializes the Linux-facing interface layer
5. the Wi-Fi side gets registered

This is what turns **“working protocol” into “working Linux network interface.”**

Here is the exact orchestration in `main.c`:

```c
static int process_event_esp_bootup(struct esp_adapter *adapter, u8 *evt_buf, u8 len)
{
	...
	while (len_left > 0) {
		switch (*pos) {
		case ESP_BOOTUP_CAPABILITY:
			adapter->capabilities = *(pos + 2);
			break;
		case ESP_BOOTUP_FIRMWARE_CHIP_ID:
			ret = esp_validate_chipset(adapter, *(pos + 2));
			break;
		case ESP_BOOTUP_FW_DATA:
			fw_p = (struct fw_data *)(pos + 2);
			ret = process_fw_data(fw_p, tag_len);
			break;
		case ESP_BOOTUP_SPI_CLK_MHZ:
			ret = esp_adjust_spi_clock(adapter, *(pos + 2));
			break;
		}
		...
	}

	if (esp_add_card(adapter)) {
		esp_err("network interface init failed\n");
		return -1;
	}
	init_bt(adapter);
	...
}
```

This is the major control-plane jump in the codebase:

- raw boot event comes in over SPI
- driver learns what the remote chip is
- Linux-facing interfaces get built on top of that knowledge

---

**3. The exact place where `wlan0` is created**

In `main.c`, `esp_add_network_ifaces(...)` calls:

- `esp_cfg80211_add_iface(adapter->wiphy, "wlan%d", 1, NL80211_IFTYPE_STATION, NULL)`

That is the moment where the code requests a normal Linux wireless interface.

That one call is a great Embedded Linux lesson:

- the transport does not expose raw packets to userspace directly
- the driver **translates the remote radio into a standard Linux interface model**

That is why `nmcli`, `iw`, and other tools can work.

The call site is small enough to memorize:

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

---

</details>

## 4. `esp_cfg80211.c` 贡献了什么

阅读 `esp_cfg80211.c` 中的这些区域：

- `esp_add_wiphy(...)`
- `esp_cfg80211_add_iface(...)`
- `esp_cfg80211_scan(...)`
- `esp_cfg80211_disconnect(...)`
- `esp_cfg80211_connect(...)`
- `esp_cfg80211_ops` 表

本文件是你的映射图，从：

- Linux wireless 子系统的预期

到：

- 发送给 ESP 的命令消息

主要思路是：

- 创建并注册一个 `wiphy`
- 声明支持的频段、速率与能力
- 为 scan/connect/disconnect/等提供操作处理函数
- 分配并注册 Linux 所期望的接口对象

这是标准的子系统集成工作。ESP 是远端设备，但 Linux 仍然要求常规的 `cfg80211` 契约。

两段简短的摘录很好地展示了分工。

首先，`esp_add_wiphy(...)` 创建并对外宣告 Linux 所期望的 wireless 能力：

```c
int esp_add_wiphy(struct esp_adapter *adapter)
{
	struct wiphy *wiphy;
	...
	wiphy = wiphy_new(&esp_cfg80211_ops, sizeof(struct esp_device));
	...
	wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION);
	wiphy->bands[NL80211_BAND_2GHZ] = &esp_wifi_bands_2ghz;
	...
	wiphy->max_scan_ssids = 10;
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wiphy->reg_notifier = esp_reg_notifier;
	...
	ret = wiphy_register(wiphy);
	return ret;
}
```

接着 `esp_cfg80211_add_iface(...)` 分配 netdev，并请求 ESP 固件初始化远端接口：

```c
struct wireless_dev *esp_cfg80211_add_iface(struct wiphy *wiphy,
		const char *name, unsigned char name_assign_type,
		enum nl80211_iftype type, struct vif_params *params)
{
	...
	ndev = ALLOC_NETDEV(sizeof(struct esp_wifi_device), name,
			    name_assign_type, ether_setup);
	...
	esp_wdev->wdev.iftype = type;
	...
	if (cmd_init_interface(esp_wdev))
		goto free_and_return;

	if (cmd_get_mac(esp_wdev))
		goto free_and_return;

	ETH_HW_ADDR_SET(ndev, esp_wdev->mac_address);
	...
	if (register_netdevice(ndev))
		goto free_and_return;
}
```

这就是**值得学习的嵌入式 Linux 模式**：

- 在本地构建标准的子系统对象
- 通过命令消息把它与远端设备同步
- 然后才把它注册到内核网络栈

---

## 5. 为什么 `wlan0` 是这么强的信号

`wlan0` 出现，其含义远不止「SPI 总线能工作」。

它意味着：

- 主机侧传输通道是活的
- boot-up 事件处理已推进到足以初始化上层
- `cfg80211` 注册成功
- 接口分配与注册成功

正因如此，已验证的 Jetson bring-up 把 `wlan0` 作为一项重要里程碑。

它区分的是：

- 底层电气或协议层面的成功

与：

- 真正的 Linux 子系统层面的成功

---

## 6. userspace 真正在检验什么

当你运行：

- `nmcli dev wifi list`
- `nmcli dev wifi connect ...`

你实际上是在间接测试：

- `cfg80211` 钩子
- 发往 ESP 的命令消息流
- 事件与响应回传进 Linux 的处理

所以一次 Wi-Fi scan 并非「只是一条用户命令」。它是对以下内容的**完整端到端测试**：

- Linux 子系统注册
- 传输可靠性
- ESP 固件命令处理

这正是这个 repo 作为嵌入式 Linux 教学范例的价值所在。

---

## 实验

回答以下问题：

1. `wiphy` 在哪里创建？
2. station 接口在哪里被请求？
3. 为什么 `wlan0` 比一次成功的 `insmod` 是更强的证明？
4. bring-up 之后，哪条 user-space 命令最能测试完整的 Wi-Fi 路径？

可选：

- 在高层面上把 `nmcli dev wifi list` 映射到可能的 `cfg80211` 调用路径

---

**Previous:** [第 03 讲](Lecture-03.md) · **Next:** [第 05 讲 — BLE 如何变成 `hci0`](Lecture-05.md)


<details>
<summary>English original</summary>

**4. What `esp_cfg80211.c` contributes**

Read these areas in `esp_cfg80211.c`:

- `esp_add_wiphy(...)`
- `esp_cfg80211_add_iface(...)`
- `esp_cfg80211_scan(...)`
- `esp_cfg80211_disconnect(...)`
- `esp_cfg80211_connect(...)`
- the `esp_cfg80211_ops` table

This file is your map from:

- Linux wireless subsystem expectations

to:

- command messages sent to the ESP

The major ideas are:

- create and register a `wiphy`
- declare supported bands, rates, and capabilities
- provide operation handlers for scan/connect/disconnect/etc.
- allocate and register the interface objects Linux expects

That is standard subsystem integration work. The ESP is remote, but Linux still wants the usual `cfg80211` contract.

Two short excerpts show the division of labor well.

First, `esp_add_wiphy(...)` creates and advertises the wireless capabilities Linux expects:

```c
int esp_add_wiphy(struct esp_adapter *adapter)
{
	struct wiphy *wiphy;
	...
	wiphy = wiphy_new(&esp_cfg80211_ops, sizeof(struct esp_device));
	...
	wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION);
	wiphy->bands[NL80211_BAND_2GHZ] = &esp_wifi_bands_2ghz;
	...
	wiphy->max_scan_ssids = 10;
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wiphy->reg_notifier = esp_reg_notifier;
	...
	ret = wiphy_register(wiphy);
	return ret;
}
```

Then `esp_cfg80211_add_iface(...)` allocates the netdev and asks the ESP firmware to initialize the remote interface:

```c
struct wireless_dev *esp_cfg80211_add_iface(struct wiphy *wiphy,
		const char *name, unsigned char name_assign_type,
		enum nl80211_iftype type, struct vif_params *params)
{
	...
	ndev = ALLOC_NETDEV(sizeof(struct esp_wifi_device), name,
			    name_assign_type, ether_setup);
	...
	esp_wdev->wdev.iftype = type;
	...
	if (cmd_init_interface(esp_wdev))
		goto free_and_return;

	if (cmd_get_mac(esp_wdev))
		goto free_and_return;

	ETH_HW_ADDR_SET(ndev, esp_wdev->mac_address);
	...
	if (register_netdevice(ndev))
		goto free_and_return;
}
```

That is the **Embedded Linux pattern worth learning**:

- build a standard subsystem object locally
- synchronize it with the remote device through command messages
- only then register it with the kernel networking stack

---

**5. Why `wlan0` is such a strong signal**

`wlan0` appearing means much more than “the SPI bus works.”

It implies:

- the host transport is alive
- boot-up event handling completed far enough to initialize upper layers
- `cfg80211` registration succeeded
- interface allocation and registration succeeded

That is why the validated Jetson bring-up used `wlan0` as a major milestone.

It is the difference between:

- low-level electrical or protocol success

and:

- actual Linux subsystem success

---

**6. What userspace is really exercising**

When you run:

- `nmcli dev wifi list`
- `nmcli dev wifi connect ...`

you are indirectly testing:

- `cfg80211` hooks
- command message flow to the ESP
- event and response handling back into Linux

So a Wi-Fi scan is not “just a user command.” It is a **full end-to-end test** of:

- Linux subsystem registration
- transport reliability
- ESP firmware command handling

That is what makes this repo valuable as an Embedded Linux teaching example.

---

**Lab**

Answer these:

1. Where is the `wiphy` created?
2. Where is the station interface requested?
3. Why is `wlan0` stronger proof than a successful `insmod`?
4. Which user-space command best tests the full Wi-Fi path after bring-up?

Optional:

- map `nmcli dev wifi list` to the likely `cfg80211` call path at a high level

---

**Previous:** [Lecture 03](Lecture-03.md) · **Next:** [Lecture 05 — How BLE becomes `hci0`](Lecture-05.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Jetson ESP-Hosted Host Code/Lecture/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Jetson%20ESP-Hosted%20Host%20Code/Lecture/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
