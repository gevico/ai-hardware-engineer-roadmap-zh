---
title: IoT 网络与设备连接
description: IoT 网络与设备连接
---

# IoT 网络与设备连接

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">INAD</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入探索 · 嵌入式系统</p>
<p class="course-identity__title">IoT 网络与设备连接的专项课程标识。</p>
<p class="course-identity__meta">产物：bring-up（上电点亮/调通）或固件 demo · 衡量：启动、延迟、功耗、可靠性</p>
</div>
</div>


*建立在 [**Embedded Software**](../Guide.md) 之上，前提是已熟悉 MCU（微控制器）总线、中断与 RTOS 基础。该子层从 SPI/UART/I2C/CAN 这类板级局部通信，转向必须入网、保障安全并长期维护真实部署的**网络化嵌入式系统**。*

---

## 为什么存在这个子层

点对点总线教你一个 MCU 如何与一个外设通信。IoT 协议教你多台设备如何组成网络、从节点丢失中恢复、节省功耗，并且仍然向网关与云服务呈现一个干净的面向 IP 的软件模型。

这一点重要，是因为现代嵌入式产品很少止步于「传感器挂在 MCU 上」。量产设备通常需要安全入网、现场更新、mesh 或星型网络，以及把低功耗射频桥接到 Linux 主机、移动 App 或云 API 的手段。

---

## 这里学什么

### OpenThread

OpenThread 是该子层中最好的入门协议，因为它迫使你把**真实的嵌入式约束**与**真实的网络思想**联系起来。你需要同时理解 IEEE 802.15.4 射频、低功耗调度、IPv6、报文压缩、host/MCU 分工以及边界路由器。

它还直接衔接到后续路线图的工作：

* **MCU / RTOS 路线：** 直接在 SoC 上运行 Thread 协议栈，并思考任务、定时器、射频驱动与电源状态。
* **Linux / host 路线：** 通过 UART 或 SPI 使用射频协处理器（RCP），让 Linux 主机运行更高层的 Thread 协议栈与边界路由器逻辑。

从这里开始：

* [**OpenThread**](01-OpenThread/Guide.md)

### Zigbee

Zigbee 是继 OpenThread 之后下一个有用的协议，因为它在同一低功耗射频族之上教给你另一套嵌入式网络哲学。Zigbee 并非 IP 优先，而是更偏向**设备模型优先**：端点、簇、绑定、信任中心行为与角色选择共同塑造固件架构。

它还直接衔接到后续路线图的工作：

* **MCU / RTOS 路线：** 直接在 ESP32-C6 这类 SoC 上构建协调器、路由器或终端设备固件。
* **Linux / host 路线：** 采用 Zigbee 网络协处理器（NCP）或网关式设计，让更强的 host 管理控制逻辑。

然后学习：

* [**Zigbee**](02-Zigbee/Guide.md)

---

## 为什么 IoT 协议对 AI 硬件工程师重要

边缘的 AI 系统很少是孤立的。它们存在于带有配网流程、低功耗传感器网络、电池约束、网关与安全远程管理的产品之中。

如果你能推理 Thread、RCP/NCP 分工、边界路由器与低功耗 IPv6 网络，你就更能构建出 MCU、Linux 主机、加速器与云服务相互协作的系统，而不是把它们做成互不相连的 demo。


<details>
<summary>English original</summary>

**IoT Networking and Device Connectivity**

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">INAD</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Embedded Systems</p>
<p class="course-identity__title">Specialized course identity for IoT Networking and Device Connectivity.</p>
<p class="course-identity__meta">Artifact: bring-up or firmware demo · Measure: boot, latency, power, reliability</p>
</div>
</div>


*Builds on [**Embedded Software**](../Guide.md) once you are comfortable with MCU buses, interrupts, and RTOS basics. This sub-layer shifts from board-local communication like SPI/UART/I2C/CAN to **networked embedded systems** that must join, secure, and maintain real deployments.*

---

**Why This Sub-Layer Exists**

Point-to-point buses teach you how one MCU talks to one peripheral. IoT protocols teach you how many devices form a network, recover from node loss, conserve power, and still present a clean IP-facing software model to gateways and cloud services.

This matters because modern embedded products rarely stop at "sensor attached to MCU." A production device usually needs secure onboarding, field updates, a mesh or star network, and a way to bridge low-power radios to Linux hosts, mobile apps, or cloud APIs.

---

**What You Study Here**

**OpenThread**

OpenThread is the best first protocol in this sub-layer because it forces you to connect **real embedded constraints** with **real networking ideas**. You need to understand IEEE 802.15.4 radios, low-power scheduling, IPv6, packet compression, host/MCU splits, and border routers all at once.

It also creates a direct bridge to later roadmap work:

* **MCU / RTOS path:** run the Thread stack directly on an SoC and reason about tasks, timers, radio drivers, and power states.
* **Linux / host path:** use a radio co-processor (RCP) over UART or SPI and let a Linux host run the higher Thread stack and border-router logic.

Start here:

* [**OpenThread**](01-OpenThread/Guide.md)

**Zigbee**

Zigbee is the next useful protocol after OpenThread because it teaches a different embedded networking philosophy on top of the same low-power radio family. Instead of being IP-first, Zigbee is much more **device-model-first**: endpoints, clusters, bindings, trust-center behavior, and role selection shape the firmware architecture.

It also creates a direct bridge to later roadmap work:

* **MCU / RTOS path:** build coordinator, router, or end-device firmware directly on an SoC such as ESP32-C6.
* **Linux / host path:** use a Zigbee Network Co-Processor (NCP) or gateway-style design and let a stronger host manage control logic.

Then study:

* [**Zigbee**](02-Zigbee/Guide.md)

---

**Why IoT Protocols Matter for AI Hardware Engineers**

AI systems at the edge are rarely isolated. They live inside products with commissioning flows, low-power sensor networks, battery constraints, gateways, and secure remote management.

If you can reason about Thread, RCP/NCP splits, border routers, and low-power IPv6 networking, you are much better prepared to build systems where MCUs, Linux hosts, accelerators, and cloud services all cooperate instead of existing as disconnected demos.

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/IoT/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/IoT/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
