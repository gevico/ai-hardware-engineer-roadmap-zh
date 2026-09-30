---
title: 第 4 讲 - 安全、入网配置、休眠终端设备与 OTA
description: 第 4 讲 - 安全、入网配置、休眠终端设备与 OTA
---

# 第 4 讲 - 安全、入网配置、休眠终端设备与 OTA

**课程：** [Zigbee guide](../Guide.md) | **阶段 2 - 嵌入式软件、IoT**

**上一讲：** [第 03 讲 - Zigbee 协议栈：ZDO、APS、端点与簇](Lecture-03.md) | **下一讲：** [第 05 讲 - ESP32-C6 实践路径：设备、NCP、网关与 Jetson 上下文](Lecture-05.md)

---

## 安全不是可选项

Zigbee 常常部署在用于控制以下对象的产品中：

- 照明
- 门锁
- 传感器
- 楼宇设备

因此**安全模型**不是装饰性的，它就是**核心系统行为**。

官方参考：[Silicon Labs Zigbee security concepts](https://docs.silabs.com/zigbee/9.0.0/zigbee-security/02-concepts)

在高层来看，Zigbee 安全包括：

- 网络级安全
- 密钥处理
- 信任中心职责
- 安全准入与设备授权

---

## 网络密钥与信任中心思维

你不需要第一天就掌握规范的全部细节，但你需要正确的思维模型：

- 一个 Zigbee 网络拥有共享的安全材料
- 加入是受控的
- 信任中心是准入与安全策略的核心

这意味着**安全入网**是固件与网关设计的一部分，而不是事后补上的东西。

---

## 入网配置

入网配置是这样一组过程：

- 允许设备加入
- 给它正确的凭据与策略
- 把它集成到网络中

如果你来自 Thread，可以把它看作安全入网在 Zigbee 一侧的版本，只是它有自己的信任中心与应用生态约定。

具体的用户体验在不同产品之间可能不同，但工程原则是一样的：

- 设备不应该仅仅因为就在附近，就凭空出现在网络上

---

## 休眠终端设备

Zigbee 的一大价值点是**电池供电运行**。

休眠终端设备：

- 激进地关闭射频
- 不转发流量
- 依赖父节点/路由器关系
- 以延迟和简单性为代价换取节能

这与 Thread 中的休眠子节点在思路上相似，尽管协议栈并不相同。

对嵌入式而言的关键后果是：**低功耗从来不是免费的**。你要用以下代价来换取它：

- 对父节点的依赖
- 轮询行为
- 入站流量更长的延迟
- 更小心的状态处理

---

## OTA 更新

**OTA** 对 IoT 产品尤其重要，因为已部署的设备不会一成不变。

对 Zigbee 产品而言，OTA 之所以重要，是因为你可能需要：

- 修复 bug
- 修补安全问题
- 更新簇行为
- 维持与生态变化的兼容性

所以即便 OTA 看起来像应用功能，它同样是长期产品可维护性的一部分。

---

## 实践设计教训

设计 Zigbee 产品时，你实际上是在若干相互竞争的目标之间做选择：

- 电池寿命
- 响应速度
- 路由韧性
- 入网简便性
- 安全入网
- 现场维护

这正是为什么 Zigbee 的工作是**真正的嵌入式系统工程**，而不只是无线配置。

---

## 实验

选一个假想产品：

- 电池传感器
- 智能插座
- 房间控制器

针对该产品，写出：

- 它应该是休眠的还是常供电的？
- 它是否应该充当路由器？
- 如果入网环节薄弱，安全风险是什么？
- 为什么部署之后 OTA 会重要？

---

**上一讲：** [第 03 讲 - Zigbee 协议栈：ZDO、APS、端点与簇](Lecture-03.md) | **下一讲：** [第 05 讲 - ESP32-C6 实践路径：设备、NCP、网关与 Jetson 上下文](Lecture-05.md)


<details>
<summary>English original</summary>

**Lecture 4 - Security, commissioning, sleepy devices, and OTA**

**Course:** [Zigbee guide](../Guide.md) | **Phase 2 - Embedded Software, IoT**

**Previous:** [Lecture 03 - The Zigbee stack: ZDO, APS, endpoints, and clusters](Lecture-03.md) | **Next:** [Lecture 05 - ESP32-C6 practical path: devices, NCP, gateway, and Jetson context](Lecture-05.md)

---

**Security is not optional**

Zigbee is often deployed in products that control:

- lighting
- locks
- sensors
- building devices

So the **security model** is not decorative. It is **core system behavior**.

Official reference: [Silicon Labs Zigbee security concepts](https://docs.silabs.com/zigbee/9.0.0/zigbee-security/02-concepts)

At a high level, Zigbee security includes:

- network-level security
- key handling
- trust-center responsibilities
- secure admission and device authorization

---

**Network key and trust-center thinking**

You do not need every detail of the spec on day one, but you do need the right mental model:

- a Zigbee network has shared security material
- joining is controlled
- the trust center is central to admission and security policy

That means **secure onboarding** is part of firmware and gateway design, not an afterthought.

---

**Commissioning**

Commissioning is the process of:

- allowing a device to join
- giving it the right credentials and policies
- integrating it into the network

If you come from Thread, think of this as the Zigbee-side version of secure onboarding, but with its own trust-center and application-ecosystem conventions.

The exact user experience may differ across products, but the engineering principle is the same:

- a device should not simply appear on the network because it is nearby

---

**Sleepy end devices**

One of Zigbee's big value points is **battery-powered operation**.

Sleepy end devices:

- turn radios off aggressively
- do not route traffic
- depend on a parent/router relationship
- save energy by trading latency and simplicity

This is similar in spirit to sleepy children in Thread, even though the stacks are different.

The key embedded consequence is that **low power is never free**. You pay for it with:

- parent dependence
- polling behavior
- longer latency for inbound traffic
- more careful state handling

---

**OTA updates**

**OTA** is especially important for IoT products because deployed devices do not stay static.

For Zigbee products, OTA matters because you may need to:

- fix bugs
- patch security issues
- update cluster behavior
- maintain compatibility with ecosystem changes

So even if OTA looks like an application feature, it is also part of long-term product maintainability.

---

**The practical design lesson**

When you design a Zigbee product, you are really choosing among several competing goals:

- battery life
- responsiveness
- routing resilience
- join simplicity
- secure onboarding
- field maintenance

That is why Zigbee work is **real embedded-systems engineering** and not just wireless configuration.

---

**Lab**

Take one hypothetical product:

- battery sensor
- smart plug
- room controller

For that product, write:

- should it be sleepy or always-on?
- should it ever be a router?
- what is the security risk if onboarding is weak?
- why would OTA matter after deployment?

---

**Previous:** [Lecture 03 - The Zigbee stack: ZDO, APS, endpoints, and clusters](Lecture-03.md) | **Next:** [Lecture 05 - ESP32-C6 practical path: devices, NCP, gateway, and Jetson context](Lecture-05.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/IoT/Zigbee/Lecture/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/IoT/Zigbee/Lecture/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
