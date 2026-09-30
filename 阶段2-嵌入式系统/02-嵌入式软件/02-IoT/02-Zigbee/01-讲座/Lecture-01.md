---
title: 第 1 讲 - Zigbee 是什么，它处于什么位置
description: 第 1 讲 - Zigbee 是什么，它处于什么位置
---

# 第 1 讲 - Zigbee 是什么，它处于什么位置

**课程：** [Zigbee 指南](../Guide.md) | **阶段 2 - 嵌入式软件、IoT**

**下一讲：** [第 02 讲 - 角色、拓扑与网络组建](Lecture-02.md)

---

## 最短且正确的定义

Zigbee 是构建在 **IEEE 802.15.4** 之上的**面向嵌入式设备的低功耗无线网络协议栈**。

这句话很重要，因为它能避免两个常见错误：

- Zigbee **不是**仅仅「802.15.4」
- Zigbee **不是**像 Thread 那样的 IP 网络

IEEE 802.15.4 为 Zigbee 提供射频与 MAC 基础。Zigbee 在其上加入自己的：

- 网络行为
- routing 模型
- 设备角色
- 安全模型
- 应用层对象模型

官方参考：[CSA Zigbee 规范](https://csa-iot.org/wp-content/uploads/2023/04/05-3474-23-csg-zigbee-specification-compressed.pdf)、[Silicon Labs Zigbee 概览](https://docs.silabs.com/zigbee/8.2.1/zigbee-fundamentals/01-overview)

---

## Zigbee 为何存在

Zigbee 面向的系统需要：

- 低功耗
- 中等数据速率
- 大量设备
- 在嘈杂的真实射频环境中足够好的可靠性
- 设备到设备的控制与感知

因此它很适合：

- 家庭自动化
- 楼宇自动化
- 传感器
- 照明
- 计量
- 工业监测

如果你的首要需求是下列之一，Zigbee 就不合适：

- 高带宽
- 视频
- 大文件传输
- 直接的通用 IP 组网

这就是它更接近**控制与遥测**，而非 Wi-Fi 式组网的原因。

---

## Zigbee 与 Thread

你在这条 IoT 路径中已经学过 OpenThread，所以最有用的是直接对比。

### 二者的共同点

- 二者通常都使用 **IEEE 802.15.4**
- 二者都面向低功耗嵌入式网络
- 二者都支持类 mesh 行为
- 二者都常见于智能家居与 gateway 产品

### 二者的不同做法

- **Thread** 是 IPv6 优先的受限 IP 网络
- **Zigbee** 是非 IP 的网络协议栈，有自己的应用模型

这一差异改变了**整个工程风格**。

用 Thread 时，你思考的是：

- IPv6
- 6LoWPAN
- UDP
- 边界路由器

用 Zigbee 时，你思考的是：

- 端点
- 簇
- 属性
- 绑定
- 信任中心与网络密钥

所以即便在原理图上两者射频看起来相似，**软件架构并不相同**。

---

## Zigbee 为何属于嵌入式软件

Zigbee 不只是网络话题。它深深带有**嵌入式软件的形态**。

你需要推敲：

- 设备角色与电源状态
- 持久化的网络凭证
- 事件驱动的应用逻辑
- 端点与簇配置
- flash 或安全存储中的安全材料
- 应用在微型 MCU 上的行为

这恰恰是固件、网络与产品行为**紧耦合**的那类系统。

---

## 需要保持的心智模型

把 Zigbee 看作：

- 一个**低功耗、具备 mesh 能力的网络**
- 配有**强大的应用模型**
- 为**设备控制与感知**而构建
- 建立在 **802.15.4** 之上

这就是本课程其余所有内容的正确起点。

---

## 实验

写一份简短的三栏对比笔记：

- 「Zigbee 从 802.15.4 继承了什么」
- 「Zigbee 在 802.15.4 之上增加了什么」

然后再加一栏：

- 「Thread 的做法有何不同」

如果你能清楚地解释这些，就可以进入下一讲了。

---

**上一讲：** [课程中心](../Guide.md) | **下一讲：** [第 02 讲 - 角色、拓扑与网络组建](Lecture-02.md)


<details>
<summary>English original</summary>

**Lecture 1 - What Zigbee is and where it fits**

**Course:** [Zigbee guide](../Guide.md) | **Phase 2 - Embedded Software, IoT**

**Next:** [Lecture 02 - Roles, topology, and network formation](Lecture-02.md)

---

**The shortest correct definition**

Zigbee is a **low-power wireless networking stack for embedded devices** built on top of **IEEE 802.15.4**.

That sentence matters because it prevents two common mistakes:

- Zigbee is **not** just "802.15.4"
- Zigbee is **not** an IP network like Thread

IEEE 802.15.4 gives Zigbee the radio and MAC foundation. Zigbee adds its own:

- network behavior
- routing model
- device roles
- security model
- application-layer object model

Official references: [CSA Zigbee specification](https://csa-iot.org/wp-content/uploads/2023/04/05-3474-23-csg-zigbee-specification-compressed.pdf), [Silicon Labs Zigbee overview](https://docs.silabs.com/zigbee/8.2.1/zigbee-fundamentals/01-overview)

---

**Why Zigbee exists**

Zigbee was designed for systems that need:

- low power
- modest data rates
- many devices
- good enough reliability in noisy real-world radio environments
- device-to-device control and sensing

This makes it a good fit for:

- home automation
- building automation
- sensors
- lighting
- metering
- industrial monitoring

Zigbee is a bad fit if your first requirement is:

- high bandwidth
- video
- large file transfer
- direct general-purpose IP networking

That is why it sits closer to **control and telemetry** than to Wi-Fi-style networking.

---

**Zigbee vs Thread**

You already have OpenThread in this IoT path, so the most useful comparison is direct.

**What they share**

- both commonly use **IEEE 802.15.4**
- both target low-power embedded networks
- both support mesh-like behavior
- both are common in smart-home and gateway products

**What they do differently**

- **Thread** is an IPv6-first constrained IP network
- **Zigbee** is a non-IP networking stack with its own application model

That difference changes the **whole engineering style**.

With Thread, you think in terms of:

- IPv6
- 6LoWPAN
- UDP
- border routers

With Zigbee, you think in terms of:

- endpoints
- clusters
- attributes
- bindings
- trust center and network keys

So even though the radios may look similar on a schematic, the **software architecture is not the same**.

---

**Why Zigbee belongs in Embedded Software**

Zigbee is not just a networking topic. It is deeply **embedded-software-shaped**.

You need to reason about:

- device roles and power states
- persistent network credentials
- event-driven application logic
- endpoint and cluster configuration
- security material in flash or secure storage
- application behavior on tiny MCUs

This is exactly the kind of system where firmware, networking, and product behavior are **tightly coupled**.

---

**The mental model to keep**

Think of Zigbee as:

- a **low-power mesh-capable network**
- with a **strong application model**
- built for **device control and sensing**
- on top of **802.15.4**

That is the correct starting point for everything else in this course.

---

**Lab**

Write a short comparison note with two columns:

- "What Zigbee inherits from 802.15.4"
- "What Zigbee adds above 802.15.4"

Then add one more column:

- "What Thread does differently"

If you can explain that clearly, you are ready for the next lecture.

---

**Previous:** [Course hub](../Guide.md) | **Next:** [Lecture 02 - Roles, topology, and network formation](Lecture-02.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/IoT/Zigbee/Lecture/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/IoT/Zigbee/Lecture/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
