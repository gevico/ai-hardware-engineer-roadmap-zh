---
title: 第 2 讲 - 角色、拓扑与网络组建
description: 第 2 讲 - 角色、拓扑与网络组建
---

# 第 2 讲 - 角色、拓扑与网络组建

**课程：** [Zigbee 指南](../Guide.md) | **阶段 2 - 嵌入式软件、IoT**

**上一讲：** [第 01 讲 - Zigbee 是什么，以及它适合用在哪里](Lecture-01.md) | **下一讲：** [第 03 讲 - Zigbee 协议栈：ZDO、APS、端点与簇](Lecture-03.md)

---

## 三种主要节点角色

Zigbee 网络由三种主要节点类型构成：

- **协调器**
- **路由器**
- **终端设备**

官方参考：[Silicon Labs 网络节点类型](https://docs.silabs.com/zigbee/8.2.0/zigbee-fundamentals/03-network-node-types)

### 协调器

一个 Zigbee 网络中最多只有**一个协调器**。

它的职责是：

- 组建网络
- 选择信道与标识符
- 作为网络的锚点
- 通常充当**信任中心**

实践中，协调器通常是**类似网关、或由市电供电的中心节点**。

### 路由器

路由器：

- 转发流量
- 扩展网络覆盖范围
- 允许子节点接入
- 保持唤醒，而不是激进休眠

路由器是**基础设施**，而不是以电池优先的端点。

### 终端设备

终端设备：

- 不为其他节点路由流量
- 通常通过父节点通信
- 是电池供电的传感器与执行器的天然归属

部分终端设备是**休眠型（sleepy）**的，这对后续的功耗设计影响很大。

---

## 拓扑：星型、网状与混合现实

Zigbee 常被描述为「mesh」，但真正对工程有用的要点更具体：

- 协调器可以锚定一个网络
- 路由器可以中继流量
- 终端设备可以挂接在父节点下

这意味着实际部署往往呈现**混合**形态，而不是教科书里某个纯粹的图示。

官方参考：[Silicon Labs Zigbee 网状组网](https://docs.silabs.com/zigbee/8.2.1/zigbee-fundamentals/02-zigbee-mesh-networking)

![包含协调器、路由器与终端设备的简单 Zigbee 网络](../../../../../Assets/images/zigbee-emcu-stack.png)

这张图很有用，因为它直观展示了角色划分：

- 中心是**一个协调器**
- 若干**路由器**扩展网络
- 若干叶子**设备**挂在路由基础设施之下

来源：[eMcU Home Automation Zigbee 拓扑图](https://www.emcu-homeautomation.org/wp-content/uploads/2021/06/immagine-111.png)

实践中：

- 小型部署的行为可能几乎像星型
- 规模较大的部署用路由器来扩大覆盖并提升韧性
- 休眠型终端设备始终是叶子节点

所以真正的问题不是「它是星型还是网状？」，而是：

- 谁负责路由？
- 谁休眠？
- 谁组建网络？
- 故障点在哪里？

---

## Zigbee 网络如何组建

概括而言：

1. 协调器扫描并选择信道
2. 它创建网络
3. 路由器与终端设备发现并加入
4. 选出父节点
5. 随着网络增长，路由状态逐步形成

这是在接触 SDK 示例之前必须烂熟于心的基本 **bring-up 流程**（上电点亮/调通）。

关键细节：

- 协调器承担特殊的组网职责
- 网络一旦建立，路由器与父节点关系在日常运行中更为重要

---

## 信任中心及其管理含义

协调器不只是第一个路由器。

在许多 Zigbee 部署中，它还充当**信任中心**，这意味着它是以下事项的核心：

- 授权
- 密钥处理
- 网络准入

这也是**协调器设计质量**如此重要的原因之一。协调器实现若很弱，整个网络都会受影响。

---

## 嵌入式设计上的后果

在固件中选择 Zigbee 角色时，你做的不是一个装饰性设置。你在选择：

- 功耗行为
- RAM/flash 压力
- 射频占空比
- 路由职责
- 安全职责

这就是为什么角色属于嵌入式软件范畴，而不只是网络图上的标注。

---

## 实验

画一个包含 6 个设备的小型 Zigbee 网络，并标注：

- 一个协调器
- 两个路由器
- 三个终端设备

然后回答：

- 哪些节点必须保持唤醒？
- 哪些节点可以合理地休眠？
- 哪个节点最可能充当信任中心？

---

**上一讲：** [第 01 讲 - Zigbee 是什么，以及它适合用在哪里](Lecture-01.md) | **下一讲：** [第 03 讲 - Zigbee 协议栈：ZDO、APS、端点与簇](Lecture-03.md)


<details>
<summary>English original</summary>

**Lecture 2 - Roles, topology, and network formation**

**Course:** [Zigbee guide](../Guide.md) | **Phase 2 - Embedded Software, IoT**

**Previous:** [Lecture 01 - What Zigbee is and where it fits](Lecture-01.md) | **Next:** [Lecture 03 - The Zigbee stack: ZDO, APS, endpoints, and clusters](Lecture-03.md)

---

**The three main node roles**

Zigbee networks are built from three main node types:

- **Coordinator**
- **Router**
- **End Device**

Official reference: [Silicon Labs network node types](https://docs.silabs.com/zigbee/8.2.0/zigbee-fundamentals/03-network-node-types)

**Coordinator**

There is at most **one coordinator** in a Zigbee network.

Its job is to:

- form the network
- choose channel and identifiers
- anchor the network
- often act as the **trust center**

In practice, the coordinator is usually the **gateway-like or mains-powered central node**.

**Router**

Routers:

- forward traffic
- extend network range
- allow children to attach
- stay awake instead of sleeping aggressively

A router is **infrastructure**, not a battery-first endpoint.

**End Device**

End devices:

- do not route traffic for others
- usually communicate through a parent
- are the natural place for battery-powered sensors and actuators

Some end devices are **sleepy**, which matters a lot for power design later.

---

**Topologies: star, mesh, and hybrid reality**

Zigbee is often described as "mesh", but the useful engineering point is more specific:

- a coordinator can anchor a network
- routers can relay traffic
- end devices can hang from a parent

That means real deployments often look **hybrid**, not like one pure textbook diagram.

Official reference: [Silicon Labs Zigbee mesh networking](https://docs.silabs.com/zigbee/8.2.1/zigbee-fundamentals/02-zigbee-mesh-networking)

![Simple Zigbee network with coordinator, routers, and end devices](../../../../../Assets/images/zigbee-emcu-stack.png)

This picture is useful because it shows the role split visually:

- one **Coordinator** at the center
- several **Routers** extending the network
- several leaf **Devices** hanging from routing infrastructure

Source: [eMcU Home Automation Zigbee topology image](https://www.emcu-homeautomation.org/wp-content/uploads/2021/06/immagine-111.png)

In practice:

- a small deployment may behave almost like a star
- a larger deployment uses routers for reach and resilience
- sleepy end devices remain leaves

So the real question is not "is it star or mesh?" but:

- who routes?
- who sleeps?
- who forms the network?
- where are the failure points?

---

**How a Zigbee network forms**

At a high level:

1. a coordinator scans and selects a channel
2. it creates the network
3. routers and end devices discover and join
4. parents are selected
5. routing state develops as the network grows

This is the basic **bring-up flow** you must hold in your head before touching SDK examples.

Key detail:

- the coordinator has special formation responsibility
- once the network exists, routers and parent relationships matter more for daily operation

---

**Trust center and management implications**

The coordinator is not just the first router.

In many Zigbee deployments it also acts as the **trust center**, which means it is central to:

- authorization
- key handling
- network admission

That is one reason **coordinator design quality** matters so much. If the coordinator is weakly implemented, the whole network suffers.

---

**The embedded-design consequence**

When you choose a Zigbee role in firmware, you are not making a cosmetic setting. You are choosing:

- power behavior
- RAM/flash pressure
- radio duty cycle
- routing responsibility
- security responsibility

That is why roles belong in Embedded Software, not only in networking diagrams.

---

**Lab**

Draw a small 6-device Zigbee network and label:

- one coordinator
- two routers
- three end devices

Then answer:

- which nodes must stay awake?
- which nodes can reasonably sleep?
- which node is most likely to act as trust center?

---

**Previous:** [Lecture 01 - What Zigbee is and where it fits](Lecture-01.md) | **Next:** [Lecture 03 - The Zigbee stack: ZDO, APS, endpoints, and clusters](Lecture-03.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/IoT/Zigbee/Lecture/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/IoT/Zigbee/Lecture/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
