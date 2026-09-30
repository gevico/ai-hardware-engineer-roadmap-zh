---
title: 第 2 讲 - 支持的芯片、安装路径，以及首次板级 bring-up（上电点亮/调通）
description: 第 2 讲 - 支持的芯片、安装路径，以及首次板级 bring-up（上电点亮/调通）
---

# 第 2 讲 - 支持的芯片、安装路径，以及首次板级 bring-up（上电点亮/调通）

**课程：** [Espressif 指南](../Guide.md) | **阶段 2 - 嵌入式软件**

**上一讲：** [第 01 讲](Lecture-01.md) | **下一讲：** [第 03 讲 - Arduino layer 之下究竟是什么：ESP-IDF、FreeRTOS 与核心架构](Lecture-03.md)

---

## 为什么这一讲重要

在写代码之前，你需要知道**自己究竟站在什么平台上**。

对于`arduino-esp32`，这意味着三件事：

1. 支持哪些 ESP32 芯片
2. 你用的是哪种工作流
3. 你的首次硬件 bring-up 应该是简单的还是进阶的

跳过这些问题，你就会照抄错误的芯片或错误的构建模型的示例。

---

## 支持的芯片

根据 Espressif 官方 `arduino-esp32` README，稳定版核心支持以下 ESP32 系列 SoC：

- `ESP32`
- `ESP32-C3`
- `ESP32-C5`
- `ESP32-C6`
- `ESP32-H2`
- `ESP32-P4`
- `ESP32-S2`
- `ESP32-S3`

Espressif 的重要提示：

- `ESP32-C2` 和 `ESP32-C61` 的支持方式有所不同
- 它们需要以下两者之一：
  - **Arduino 作为 ESP-IDF 组件**
  - 或者重新构建静态库

这一区别很重要，因为**并非每颗芯片都同样支持最简单的 Arduino 流程**。

官方参考：[README supported chips](https://github.com/espressif/arduino-esp32)，[Getting Started](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)，[Libraries](https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html)

---

## 两种主要工作流风格

### 1. 经典 Arduino 风格工作流

这是大多数人首先想到的路径：

- 安装开发板包
- 选择开发板
- 编写 sketch
- 上传

这是以下场景的最佳路径：

- 首次板级 bring-up
- GPIO、UART、Wi-Fi、BLE 与传感器实验
- 快速迭代

### 2. Arduino 作为 ESP-IDF 组件

Espressif 将其记录为更进阶的路径。

当你需要以下内容时，它最合适：

- 对工程结构更精细的控制
- 与 ESP-IDF 直接集成
- 进阶外设或中间件
- 更接近量产的固件组织方式

这一区别是 Espressif 生态中**最重要的实践经验**之一。

---

## “首次板级 bring-up”应该是什么样

你的第一次成功应该**刻意地乏味**。

不要从以下内容开始：

- Matter
- Thread
- 自定义分区
- 复杂多任务
- 云侧协议栈

从以下内容开始：

1. 开发板包正确安装
2. 选中正确的开发板
3. 串口上传可用
4. 串口监视器可用
5. 一个最小的 LED 或串口示例能跑起来

这能说明：

- USB 链路正确
- 烧录可用
- 你选择的开发板配置合理
- 开发环境不是阻塞点

---

## bring-up 期间要验证什么

首次上传之后，验证以下基础项：

- 串口上出现启动信息
- 你的代码能执行到 `setup()`
- loop 反复运行
- GPIO 输出或串口打印行为符合预期
- LED 引脚或 USB CDC 等板级特性与所选开发板配置一致

这听起来很琐碎，但它能在之后避免**许多小时的困惑**。

---

## 为什么在 Espressif 上芯片身份比初学者以为的更重要

很多 Arduino 教程把各种开发板混为一谈。

在 Espressif 上这很危险，因为：

- 不同芯片的射频组合不同
- 可用外设不同
- 内存与 flash 的前提假设不同
- 低功耗行为不同
- 一些较新的芯片支持成熟度不同

所以应按这个顺序思考：

- 哪颗 **SoC**？
- 哪个**开发板变体**？
- 哪种**工作流模型**？

而不只是：

- 哪个示例 sketch？

---

## 本路线图当前的最佳起点

对本路线图而言，一条非常扎实的学习路径是：

- `ESP32-C3` 或 `ESP32-S3`，用于获取广泛的社区示例
- 当你需要 Wi-Fi + BLE + 802.15.4 相关内容时，用 `ESP32-C6`

因此在以下内容之前，`arduino-esp32` 特别有用：

- OpenThread
- Zigbee
- 联网传感器产品
- 语音外设与本地网关

---

## 实验

做一个简表，包含以下列：

- `Chip`
- `Why I might choose it`
- `Would I start with Arduino workflow or ESP-IDF component workflow?`

至少填入：

- `ESP32-C3`
- `ESP32-C6`
- `ESP32-S3`

然后写一句话，说明为什么 `ESP32-C2` / `ESP32-C61` 需要额外注意。

---

**上一讲：** [第 01 讲](Lecture-01.md) | **下一讲：** [第 03 讲 - Arduino layer 之下究竟是什么：ESP-IDF、FreeRTOS 与核心架构](Lecture-03.md)


<details>
<summary>English original</summary>

**Lecture 2 - Supported chips, install path, and first board bring-up**

**Course:** [Espressif guide](../Guide.md) | **Phase 2 - Embedded Software**

**Previous:** [Lecture 01](Lecture-01.md) | **Next:** [Lecture 03 - What is really under the Arduino layer: ESP-IDF, FreeRTOS, and core architecture](Lecture-03.md)

---

**Why this lecture matters**

Before writing code, you need to know **what platform you are actually standing on**.

For `arduino-esp32`, that means three things:

1. which ESP32 chips are supported
2. which workflow you are using
3. whether your first hardware bring-up is supposed to be simple or advanced

If you skip those questions, you end up copying examples for the wrong chip or the wrong build model.

---

**Supported chips**

According to Espressif's official `arduino-esp32` README, the stable core supports these ESP32-family SoCs:

- `ESP32`
- `ESP32-C3`
- `ESP32-C5`
- `ESP32-C6`
- `ESP32-H2`
- `ESP32-P4`
- `ESP32-S2`
- `ESP32-S3`

Important note from Espressif:

- `ESP32-C2` and `ESP32-C61` are supported differently
- they require either:
  - **Arduino as an ESP-IDF component**
  - or rebuilding the static libraries

That distinction matters because **not every chip supports the simplest Arduino flow equally**.

Official references: [README supported chips](https://github.com/espressif/arduino-esp32), [Getting Started](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html), [Libraries](https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html)

---

**The two main workflow styles**

**1. Classic Arduino-style workflow**

This is the path most people imagine first:

- install the board package
- select the board
- write a sketch
- upload

This is the best path for:

- first board bring-up
- GPIO, UART, Wi-Fi, BLE, and sensor experiments
- fast iteration

**2. Arduino as an ESP-IDF component**

Espressif documents this as the more advanced path.

This is best when you need:

- tighter control over project structure
- direct ESP-IDF integration
- advanced peripherals or middleware
- more production-like firmware organization

This distinction is one of the **most important practical lessons** in the Espressif ecosystem.

---

**What “first board bring-up” should look like**

Your first success should be **deliberately boring**.

Do not start with:

- Matter
- Thread
- custom partitions
- complex multitasking
- cloud stacks

Start with:

1. board package installed correctly
2. correct board selected
3. serial upload works
4. serial monitor works
5. a minimal LED or serial example runs

That tells you:

- USB path is correct
- flashing works
- your chosen board profile is sane
- the development environment is not the blocker

---

**What to verify during bring-up**

After the first upload, verify these basics:

- boot messages appear on serial
- your code reaches `setup()`
- your loop runs repeatedly
- a GPIO output or serial print behaves as expected
- board-specific features like LED pin or USB CDC match the selected board profile

This sounds trivial, but it prevents **many hours of confusion** later.

---

**Why chip identity matters more on Espressif than beginners expect**

A lot of Arduino tutorials blur boards together.

That is dangerous on Espressif because:

- different chips have different radio combinations
- peripheral availability differs
- memory and flash assumptions differ
- low-power behavior differs
- some newer chips have different support maturity

So you should think in this order:

- which **SoC**?
- which **board variant**?
- which **workflow model**?

not just:

- which example sketch?

---

**Best current starting point for this roadmap**

For this roadmap, a very strong learning path is:

- `ESP32-C3` or `ESP32-S3` for broad community examples
- `ESP32-C6` when you want Wi-Fi + BLE + 802.15.4 relevance

That makes `arduino-esp32` especially useful before:

- OpenThread
- Zigbee
- connected sensor products
- voice peripherals and local gateways

---

**Lab**

Make a short table with these columns:

- `Chip`
- `Why I might choose it`
- `Would I start with Arduino workflow or ESP-IDF component workflow?`

Fill in at least:

- `ESP32-C3`
- `ESP32-C6`
- `ESP32-S3`

Then write one sentence about why `ESP32-C2` / `ESP32-C61` need extra care.

---

**Previous:** [Lecture 01](Lecture-01.md) | **Next:** [Lecture 03 - What is really under the Arduino layer: ESP-IDF, FreeRTOS, and core architecture](Lecture-03.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/Espressif/Lecture/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/Espressif/Lecture/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
