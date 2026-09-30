---
title: 第 5 讲 - ESP32-C6 实践路径：设备、NCP、网关与 Jetson 上下文
description: 第 5 讲 - ESP32-C6 实践路径：设备、NCP、网关与 Jetson 上下文
---

# 第 5 讲 - ESP32-C6 实践路径：设备、NCP、网关与 Jetson 上下文

**课程：** [Zigbee 指南](../Guide.md) | **阶段 2 - 嵌入式软件、IoT**

**上一讲：** [第 04 讲 - 安全、入网、休眠设备与 OTA](Lecture-04.md)

---

## 为什么 ESP32-C6 在这里重要

Espressif 官方的 Zigbee 资料使 ESP32-C6 变得相关，因为它可用于构建：

- Zigbee 设备
- Zigbee 协调器与路由器
- 网关式的主机/协处理器系统

官方参考：[ESP Zigbee SDK 简介](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/introduction.html)

这使 ESP32-C6 成为以下两者之间有价值的桥梁：

- 纯 MCU Zigbee 设备
- Linux 承载的网关实验

---

## 三种实践实现风格

### 1. 独立 Zigbee 设备

在此模型中，ESP32-C6 **在本地运行整个 Zigbee 应用**。

适用于：

- 传感器
- 开关
- 简单控制器

这是学习端点、簇与角色配置的**最清晰方式**。

### 2. 网络协处理器（NCP）

在 Zigbee NCP 模型中：

- Zigbee 协议栈位于协处理器上
- 主机处理器通过主机接口控制它
- 协议是 **SLIP 之上的 ESP ZNSP**
- 传输层可以是 **UART** 或 **SPI**

官方参考：[ESP Zigbee NCP 指南](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/user-guide/ncp.html)、[ESP Zigbee NCP API](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/api-reference/esp_zigbee_ncp.html)

对于 Jetson 这样的 Linux 主机，这是**最相关的模型**。

### 3. 网关 / 多芯片设计

Espressif 还记录了一个 `zigbee_gateway` 示例，其中使用了：

- 一个主机 SoC
- 一个 802.15.4 射频侧

这很有用，因为它表明真实的 Zigbee 网关设计往往是**多处理器系统**，而不是单芯片玩具。

官方参考：[ESP Zigbee 网关示例](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/zigbee_gateway)

---

## 在 Jetson 上有何变化

对于 Jetson 上的 Thread，模型是：

- Linux 上的主机协议栈
- `otbr-agent` 或 `ot-daemon`
- `wpan0`

对于 Jetson 上的 Zigbee，模型则不同：

- 没有 `wpan0`
- 没有 OTBR 风格的 IPv6 边界路由器路径
- 主机向协处理器发送 Zigbee 控制消息

这意味着成功与否取决于：

- 组建或加入一个 Zigbee 网络
- 从协处理器读取状态
- 发送簇或管理命令

而不是看是否出现一个 Linux IP 接口。

---

## 为什么这对你当前的路线图路径很重要

这条路线图现在包含全部三个相关想法：

- [OpenThread](../../01-OpenThread/Guide.md)，用于低功耗 IP 网状网络
- [ESP32-C6 OpenThread RCP on Jetson Orin Nano](../../../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/02-网络与连接/ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md)，用于带射频协处理器的 Linux 承载 Thread
- [ESP32-C6 Zigbee NCP on Jetson Orin Nano](../../../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/02-网络与连接/ESP32-C6-Zigbee-NCP-Jetson-Orin-Nano.md)，用于 Zigbee 协处理器路径

这种对比很有价值，因为它表明**同一射频系列可以支持非常不同的软件架构**。

---

## 从整个课程中应得的正确结论

Zigbee 现在应该看起来是：

- 一个低功耗嵌入式网络
- 具有强设备行为建模
- 其中节点角色、安全与应用结构紧密耦合
- 并且其中主机/NCP 设计是真实可行的选项，而不只是单芯片示例

如果你能把 Zigbee 与 Thread 作比较，并解释为什么一个是 IP 原生，而另一个以簇和端点为中心，那么这门课程就完成了它的任务。

---

## 实验

为同一个智能建筑产品设计两种架构：

### 架构 A

- ESP32-C6 作为独立 Zigbee 设备

### 架构 B

- Jetson 作为主机
- ESP32-C6 作为 Zigbee NCP

对每一种，回答：

- 应用逻辑位于哪里？
- 网络凭证位于哪里？
- 哪一侧处理用户可见的设备行为？
- 什么变得更容易？
- 什么变得更难？

---

**上一讲：** [第 04 讲 - 安全、入网、休眠设备与 OTA](Lecture-04.md) | **下一讲：** [课程中心](../Guide.md)


<details>
<summary>English original</summary>

**Lecture 5 - ESP32-C6 practical path: devices, NCP, gateway, and Jetson context**

**Course:** [Zigbee guide](../Guide.md) | **Phase 2 - Embedded Software, IoT**

**Previous:** [Lecture 04 - Security, commissioning, sleepy devices, and OTA](Lecture-04.md)

---

**Why ESP32-C6 matters here**

Espressif's official Zigbee material makes ESP32-C6 relevant because it can be used to build:

- Zigbee devices
- Zigbee coordinators and routers
- gateway-style host/co-processor systems

Official reference: [ESP Zigbee SDK introduction](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/introduction.html)

That makes ESP32-C6 a useful bridge between:

- pure MCU Zigbee devices
- Linux-hosted gateway experiments

---

**Three practical implementation styles**

**1. Standalone Zigbee device**

In this model, the ESP32-C6 runs the **whole Zigbee application locally**.

Use this for:

- sensors
- switches
- simple controllers

This is the **cleanest way** to learn endpoint, cluster, and role configuration.

**2. Network Co-Processor (NCP)**

In the Zigbee NCP model:

- the Zigbee stack lives on the coprocessor
- a host processor controls it over a host interface
- the protocol is **ESP ZNSP over SLIP**
- the transport can be **UART** or **SPI**

Official references: [ESP Zigbee NCP guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/user-guide/ncp.html), [ESP Zigbee NCP API](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/api-reference/esp_zigbee_ncp.html)

This is the **most relevant model** for a Linux host such as Jetson.

**3. Gateway / multi-chip design**

Espressif also documents a `zigbee_gateway` example that uses:

- a host SoC
- an 802.15.4 radio side

That is useful because it shows real Zigbee gateway designs are often **multi-processor systems**, not single-chip toys.

Official reference: [ESP Zigbee gateway example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/zigbee_gateway)

---

**What changes on Jetson**

For Thread on Jetson, the model was:

- host stack on Linux
- `otbr-agent` or `ot-daemon`
- `wpan0`

For Zigbee on Jetson, the model is different:

- no `wpan0`
- no OTBR-style IPv6 border-router path
- the host speaks Zigbee control messages to the coprocessor

That means success is measured by:

- forming or joining a Zigbee network
- reading state from the coprocessor
- sending cluster or management commands

not by seeing a Linux IP interface appear.

---

**Why this matters for your current roadmap path**

This roadmap now contains all three related ideas:

- [OpenThread](../../01-OpenThread/Guide.md) for low-power IP mesh networking
- [ESP32-C6 OpenThread RCP on Jetson Orin Nano](../../../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/02-网络与连接/ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md) for Linux-hosted Thread with a radio coprocessor
- [ESP32-C6 Zigbee NCP on Jetson Orin Nano](../../../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/02-网络与连接/ESP32-C6-Zigbee-NCP-Jetson-Orin-Nano.md) for a Zigbee coprocessor path

That comparison is valuable because it shows the **same radio family can support very different software architectures**.

---

**The right takeaway from this whole course**

Zigbee should now look like:

- a low-power embedded network
- with strong device-behavior modeling
- where node role, security, and application structure are tightly coupled
- and where host/NCP designs are a real option, not just single-chip examples

If you can compare Zigbee against Thread and explain why one is IP-native while the other is cluster-and-endpoint-centric, then this course has done its job.

---

**Lab**

Design two architectures for the same smart-building product:

**Architecture A**

- ESP32-C6 as a standalone Zigbee device

**Architecture B**

- Jetson as host
- ESP32-C6 as Zigbee NCP

For each, answer:

- where does application logic live?
- where do network credentials live?
- which side handles user-visible device behavior?
- what becomes easier?
- what becomes harder?

---

**Previous:** [Lecture 04 - Security, commissioning, sleepy devices, and OTA](Lecture-04.md) | **Next:** [Course hub](../Guide.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/IoT/Zigbee/Lecture/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/IoT/Zigbee/Lecture/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
