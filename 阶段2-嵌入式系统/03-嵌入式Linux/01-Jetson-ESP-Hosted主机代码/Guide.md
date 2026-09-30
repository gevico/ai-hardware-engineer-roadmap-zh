---
title: Jetson ESP-Hosted Host Code — Embedded Linux 驱动阅读课程
description: Jetson ESP-Hosted Host Code — Embedded Linux 驱动阅读课程
---

# Jetson ESP-Hosted Host Code — Embedded Linux 驱动阅读课程

<div class="course-identity auto-course" style="--course-accent: #0f766e; --course-accent-rgb: 15, 118, 110;" markdown="1">
<div class="course-identity__icon">JEHH</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入剖析 · 嵌入式系统</p>
<p class="course-identity__title">Jetson ESP-Hosted Host Code — Embedded Linux 驱动阅读课程的专属课程身份标识。</p>
<p class="course-identity__meta">产物：bring-up（上电点亮/调通）或固件 demo · 度量：启动、延迟、功耗、可靠性</p>
</div>
</div>


一门结构化的迷你课程，面向那些想读懂一套**真实的嵌入式 Linux host 栈**、而不只是消费板级 bring-up 教程的工程师。

这里研读的代码是面向 Jetson 的分支：

- [ai-hpc/jetson-esp-hosted](https://github.com/ai-hpc/jetson-esp-hosted)

重心在 `esp_hosted_ng/host/` 目录树，尤其是在 **Jetson Orin Nano** 上配合 **ESP32-C6** 验证过的 SPI 路径。

---

## 为什么有这门课程

大多数嵌入式 Linux 学习资料把世界拆成互不相干的盒子：

- SPI 与 GPIO 教程
- 内核模块教程
- Wi-Fi 教程
- 蓝牙教程

真实系统不是这样切分的。

这套代码库有价值，因为一个 host 栈同时横跨了上述全部：

- 一个树外内核模块
- 一个板级专用的 bring-up 脚本
- SPI 传输与 GPIO 中断
- `cfg80211` Wi-Fi 集成
- 用于 BLE 的 HCI / BlueZ 集成
- Linux 用户态工具，例如 `nmcli`、`bluetoothctl`、`hciconfig` 和 `rfkill`

这使它成为一个强有力的嵌入式 Linux 案例研究。

---

## 你将学到什么

- Linux host 驱动如何把远端 ESP 芯片映射成常规 Linux 接口。
- 板级 shell 工具与通用内核模块代码如何拼合在一起。
- 如何在 `esp_spi.c` 中阅读传输层代码而不迷失在实现细节里。
- Wi-Fi 如何经由 `cfg80211` 变成 `wlan0`。
- BLE 如何经由 HCI 子系统变成 `hci0`。
- 如何区分：
  - **board 策略**
  - **传输策略**
  - **Linux 子系统集成**

## kernel 接口地图

这门迷你课程同时也是一次对真实 Linux 内核接口的导览。读每一讲时带着两个问题：

1. 这段代码在与哪个内核对象或子系统边界对话？
2. 哪个用户可见的 Linux 产物能证明这一步成功了？

| 讲 | 代码中面向 Linux 的接口 | 需牢记的 kernel 概念 | 基础回顾 |
|---|---|---|---|
| 1 | `esp_add_card(...)`、`esp_add_network_ifaces(...)`、`esp_init_bt(...)` | 子系统边界、kernel 与用户态、驱动为 Linux 产出什么 | [OS Lecture 1 — Modern OS Architecture & the Linux Kernel](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md) |
| 2 | `insmod`、`module_param(...)`、`spidev` unbind、树外 `.ko` 构建 | 可加载内核模块、启动时硬件描述、设备归属 | [OS Lecture 5 — Kernel Modules, Boot Process & Device Tree](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md) |
| 3 | `spi_find_device(...)`、`gpio_to_irq(...)`、`request_irq(...)` | SPI 设备模型、上半部与延后工作、中断驱动 I/O | [OS Lecture 3 — Interrupts, Exceptions & Bottom Halves](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md)、[OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)、[OS Lecture 18 — Character Drivers, Interrupt-Driven I/O & V4L2](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) |
| 4 | `wiphy_new(...)`、`wiphy_register(...)`、`esp_cfg80211_add_iface(...)`、`register_netdevice(...)` | Linux wireless 子系统契约、`wiphy`、`wireless_dev`、netdev 注册 | [OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md) |
| 5 | `hci_alloc_dev()`、`hci_register_dev()`、`hci_recv_frame(...)` | 作为子系统边界的蓝牙 HCI、控制器注册、host/controller 分离 | [OS Lecture 1 — Modern OS Architecture & the Linux Kernel](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)、[OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md) |

这个习惯比背下函数名更有价值。

---

## 你应当已经掌握的内容

在进入这门迷你课程之前，你应当熟悉：

- Linux 命令行基础
- 高层次的启动链：bootloader -> kernel -> root filesystem
- 入门级的设备树与内核模块
- 基本的 SPI/GPIO 概念

若不熟悉，请先完成主线的嵌入式 Linux 指南与 Yocto 课程。

---


<details>
<summary>English original</summary>

**Jetson ESP-Hosted Host Code — Embedded Linux Driver Reading Course**

<div class="course-identity auto-course" style="--course-accent: #0f766e; --course-accent-rgb: 15, 118, 110;" markdown="1">
<div class="course-identity__icon">JEHH</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Embedded Systems</p>
<p class="course-identity__title">Specialized course identity for Jetson ESP-Hosted Host Code — Embedded Linux Driver Reading Course.</p>
<p class="course-identity__meta">Artifact: bring-up or firmware demo · Measure: boot, latency, power, reliability</p>
</div>
</div>


A structured mini-course for engineers who want to read a **real Embedded Linux host stack** instead of only consuming board bring-up tutorials.

The code studied here is the Jetson-oriented fork:

- [ai-hpc/jetson-esp-hosted](https://github.com/ai-hpc/jetson-esp-hosted)

The center of gravity is the `esp_hosted_ng/host/` tree, especially the SPI path validated on **Jetson Orin Nano** with **ESP32-C6**.

---

**Why this course exists**

Most Embedded Linux learning material splits the world into separate boxes:

- SPI and GPIO tutorials
- kernel module tutorials
- Wi-Fi tutorials
- Bluetooth tutorials

Real systems are not split that way.

This codebase is useful because one host stack crosses all of those at once:

- an out-of-tree kernel module
- a board-specific bring-up script
- SPI transport and GPIO IRQs
- `cfg80211` Wi-Fi integration
- HCI / BlueZ integration for BLE
- Linux userspace tools such as `nmcli`, `bluetoothctl`, `hciconfig`, and `rfkill`

That makes it a strong Embedded Linux case study.

---

**What you will learn**

- How a Linux host driver maps a remote ESP chip into normal Linux interfaces.
- How board-specific shell tooling and generic kernel module code fit together.
- How to read transport code in `esp_spi.c` without getting lost in implementation detail.
- How Wi-Fi becomes `wlan0` through `cfg80211`.
- How BLE becomes `hci0` through the HCI subsystem.
- How to separate:
  - **board policy**
  - **transport policy**
  - **Linux subsystem integration**

**Kernel interface map**

This mini-course is also a guided tour of real Linux kernel interfaces. Read each lecture with two questions in mind:

1. Which kernel object or subsystem boundary is this code talking to?
2. What user-visible Linux artifact proves that step worked?

| Lecture | Linux-facing interfaces in the code | Kernel concepts to keep in mind | Foundational refresh |
|---|---|---|---|
| 1 | `esp_add_card(...)`, `esp_add_network_ifaces(...)`, `esp_init_bt(...)` | subsystem boundaries, kernel vs userspace, what a driver is producing for Linux | [OS Lecture 1 — Modern OS Architecture & the Linux Kernel](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md) |
| 2 | `insmod`, `module_param(...)`, `spidev` unbind, out-of-tree `.ko` build | loadable kernel modules, boot-time hardware description, device ownership | [OS Lecture 5 — Kernel Modules, Boot Process & Device Tree](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md) |
| 3 | `spi_find_device(...)`, `gpio_to_irq(...)`, `request_irq(...)` | SPI device model, top half vs deferred work, interrupt-driven I/O | [OS Lecture 3 — Interrupts, Exceptions & Bottom Halves](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md), [OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md), [OS Lecture 18 — Character Drivers, Interrupt-Driven I/O & V4L2](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) |
| 4 | `wiphy_new(...)`, `wiphy_register(...)`, `esp_cfg80211_add_iface(...)`, `register_netdevice(...)` | Linux wireless subsystem contracts, `wiphy`, `wireless_dev`, netdev registration | [OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md) |
| 5 | `hci_alloc_dev()`, `hci_register_dev()`, `hci_recv_frame(...)` | Bluetooth HCI as a subsystem boundary, controller registration, host/controller split | [OS Lecture 1 — Modern OS Architecture & the Linux Kernel](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md), [OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md) |

That habit is more valuable than memorizing function names.

---

**What you should already know**

Before this mini-course, you should be comfortable with:

- Linux command-line basics
- the boot chain at a high level: bootloader -> kernel -> root filesystem
- device tree and kernel modules at a beginner level
- basic SPI/GPIO concepts

If not, do the main Embedded Linux guide and Yocto course first.

---

</details>

## 分步课程

每讲都位于 **[Lecture/](01-讲座/README.md)** 下。按顺序进行。

| # | 主题 | 课程 |
|---|-------|---------|
| 1 | 该 host 协议栈的 Linux 心智模型 | [Lecture-01.md](01-讲座/Lecture-01.md) |
| 2 | Jetson 上的构建、加载与板级策略 | [Lecture-02.md](01-讲座/Lecture-02.md) |
| 3 | SPI 传输、GPIO 与 IRQ 驱动的 bring-up（上电点亮/调通） | [Lecture-03.md](01-讲座/Lecture-03.md) |
| 4 | Wi-Fi 如何变成 `wlan0` | [Lecture-04.md](01-讲座/Lecture-04.md) |
| 5 | 低功耗蓝牙（BLE）如何变成 `hci0`，以及如何验证整条路径 | [Lecture-05.md](01-讲座/Lecture-05.md) |

---

## 推荐的学习方式

对每一讲：

1. 先读 Linux 侧的解释
2. 打开 repo 中对应的具体文件
3. 自己 trace 其中提到的函数
4. 把解释与真实代码对照
5. 先回答实验问题，再继续下一讲

不要试图记住整个 repo。要学会识别架构与子系统边界。

---

## 经验证的真实硬件背景

本课程反映的是路线图中其他地方已经记录过的、经验证的 Jetson 流程：

- 基于 SPI 的 Jetson Orin Nano
- `spi0.0`
- `resetpin=-1`
- `spi_handshake_gpio=471`
- `spi_dataready_gpio=433`
- runtime SPI 时钟上限为 `10 MHz`
- 手动复位 ESP 后出现 `wlan0`
- `hci0` 出现，且低功耗蓝牙（BLE）扫描正常工作

这一点很重要，因为这不是一次假设性的读驱动练习。它扎根于一条真正在硬件上完成 bring-up 的路径。


<details>
<summary>English original</summary>

**Step-by-step lectures**

Each lecture is under **[Lecture/](01-讲座/README.md)**. Work in order.

| # | Topic | Lecture |
|---|-------|---------|
| 1 | Linux mental model for this host stack | [Lecture-01.md](01-讲座/Lecture-01.md) |
| 2 | Build, load, and board policy on Jetson | [Lecture-02.md](01-讲座/Lecture-02.md) |
| 3 | SPI transport, GPIOs, and IRQ-driven bring-up | [Lecture-03.md](01-讲座/Lecture-03.md) |
| 4 | How Wi-Fi becomes `wlan0` | [Lecture-04.md](01-讲座/Lecture-04.md) |
| 5 | How BLE becomes `hci0` and how to validate the full path | [Lecture-05.md](01-讲座/Lecture-05.md) |

---

**Recommended study pattern**

For each lecture:

1. read the Linux-side explanation first
2. open the exact file in the repo
3. trace the named functions yourself
4. compare the explanation against the real code
5. answer the lab questions before moving on

Do not try to memorize the whole repo. Learn to recognize the architecture and the subsystem boundaries.

---

**Validated real-hardware context**

This course reflects the validated Jetson flow already documented elsewhere in the roadmap:

- Jetson Orin Nano over SPI
- `spi0.0`
- `resetpin=-1`
- `spi_handshake_gpio=471`
- `spi_dataready_gpio=433`
- runtime SPI clock capped at `10 MHz`
- `wlan0` appears after manual ESP reset
- `hci0` appears and BLE scan works

That matters because this is not a hypothetical driver-reading exercise. It is grounded in a path that was actually brought up on hardware.

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Jetson ESP-Hosted Host Code/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Jetson%20ESP-Hosted%20Host%20Code/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
