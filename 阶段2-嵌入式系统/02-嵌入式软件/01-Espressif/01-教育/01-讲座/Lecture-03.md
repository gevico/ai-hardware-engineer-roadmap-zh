---
title: 第 3 讲 - ESP32 入门：芯片、开发板、烧录与第一批示例
description: 第 3 讲 - ESP32 入门：芯片、开发板、烧录与第一批示例
---

# 第 3 讲 - ESP32 入门：芯片、开发板、烧录与第一批示例

**课程：** [官方教育路径指南](../Guide.md) | **阶段 2 - 嵌入式软件**

**上一讲：** [第 02 讲](Lecture-02.md) | **下一讲：** [第 04 讲 - 解决方案路线：AI、连接、外设、低功耗与网关](Lecture-04.md)

---

## 这一讲为什么重要

学完基础之后，Espressif 官方教育路径进入 **ESP32 入门**。

在这里，学习者需要回答：

- 芯片、模块与开发板之间有什么区别？
- 我该用哪个开发环境？
- 我真正该先跑哪些示例？

---

## 芯片 vs 模块 vs 开发板

这一区分很重要，却常被跳过。

### 芯片

芯片就是实际的 **SoC**。

示例：

- `ESP32-C6`
- `ESP32-S3`

### 模块

模块把芯片与以下这些东西封装在一起：

- flash
- RF 设计
- 认证支持

**许多真实产品**用的就是模块。

### 开发板

开发板为你提供：

- USB
- 易于接线的排针
- 供电支持
- 快速上手

如果把这三者混为一谈，之后就会做出 **糟糕的硬件决策**。

Espressif 的教育页面明确引导学习者先理解这些差异。

---

## 首先需要掌握的硬件概念

Espressif 的学习计划还特别指出：

- strapping 引脚
- 运行模式
- 下载模式

这一点很重要，因为“开发板无法烧录”往往 **不是软件问题**。

它往往是：

- 启动模式不对
- USB/串口假设不对
- 开发板配置不对

所以即使是最初的 **bring-up**（上电点亮/调通），仍然是嵌入式工程。

---

## 选择开发环境

Espressif 官方教育页面给出了三个有用的层次：

- **UIFlow**
  - 极低代码
  - 不是本路线图的主线
- **Arduino**
  - 适合入门和快速落地的实践项目
- **ESP-IDF**
  - 适合更复杂的项目和灵活开发

这是一条健康的官方表述，因为它没有假装某个环境适合所有学习者和所有产品。

对于本路线图：

- 硬件和联网设备的快速起步用 **Arduino**
- 当项目结构和控制开始更重要时用 **ESP-IDF**

---

## 真正有价值的首批示例

Espressif 的教育页面把学习者引向那些合理的示例：

- `hello_world`
- `blink`
- Wi-Fi station
- 外设示例

这个进阶顺序是对的，因为它从以下顺序推进：

- 编译与烧录
- 到 GPIO
- 到连接性
- 到外设

这比一上来就做 **大型展示 demo** 好得多。

---

## 为什么构建系统很早就重要

教育页面还引导学习者了解：

- 分区表
- `CMakeLists`
- `sdkconfig`
- `Kconfig`
- 组件管理器

这是 Espressif 给出的一个重要提示：

> 认真的 ESP32 工作很快就会变成需要理解构建系统的工作

所以即便从 Arduino 起步，也应该知道存在更深层的项目模型。

---

## 实验

写一份简短的计划，包含：

1. 你会先上手的一块开发板
2. 你会先上手的一个框架
3. 你会按顺序运行的三个示例

然后解释为什么这个顺序比直接跳进一个复杂 demo 更好。

---

**上一讲：** [第 02 讲](Lecture-02.md) | **下一讲：** [第 04 讲 - 解决方案路线：AI、连接、外设、低功耗与网关](Lecture-04.md)


<details>
<summary>English original</summary>

**Lecture 3 - Getting started with ESP32: chips, boards, flashing, and first examples**

**Course:** [Official education path guide](../Guide.md) | **Phase 2 - Embedded Software**

**Previous:** [Lecture 02](Lecture-02.md) | **Next:** [Lecture 04 - Solution tracks: AI, connectivity, peripherals, low power, and gateways](Lecture-04.md)

---

**Why this lecture matters**

After basics, Espressif’s official education path moves into **Getting Started with ESP32**.

That is where learners need to answer:

- what is the difference between a chip, module, and development board?
- which development environment should I use?
- what are the first examples I should really run?

---

**Chip vs module vs development board**

This distinction is important and often skipped.

**Chip**

The chip is the actual **SoC**.

Example:

- `ESP32-C6`
- `ESP32-S3`

**Module**

The module packages the chip with things like:

- flash
- RF design
- certification support

This is what **many real products** use.

**Development board**

The development board gives you:

- USB
- easy headers
- power support
- a fast start

If you blur these three together, you make **poor hardware decisions** later.

Espressif’s education page explicitly points learners to understanding these differences first.

---

**The first hardware concepts that matter**

Espressif’s study plan also calls out:

- strapping pins
- run mode
- download mode

That matters because "board does not flash" is often **not a software problem**.

It is often:

- wrong boot mode
- wrong USB/serial assumptions
- wrong board setup

So even first **bring-up** is still embedded engineering.

---

**Choosing the development environment**

Espressif’s official education page presents three useful levels:

- **UIFlow**
  - very low-code
  - not the main route for this roadmap
- **Arduino**
  - good for entry-level and fast practical projects
- **ESP-IDF**
  - good for more complex projects and flexible development

That is a healthy official message because it avoids pretending one environment is right for every learner and every product.

For this roadmap:

- use **Arduino** for fast hardware and connected-device starts
- use **ESP-IDF** when project structure and control start to matter more

---

**The first examples that actually matter**

Espressif’s education page points learners toward the kinds of examples that make sense:

- `hello_world`
- `blink`
- Wi-Fi station
- peripheral examples

That progression is correct because it moves from:

- compile and flash
- to GPIO
- to connectivity
- to peripherals

This is much better than starting with a **giant showcase demo**.

---

**Why the build system matters early**

The education page also points learners to:

- partition table
- `CMakeLists`
- `sdkconfig`
- `Kconfig`
- component manager

That is a major hint from Espressif:

> serious ESP32 work quickly becomes build-system-aware work

So even when you begin with Arduino, you should know that the deeper project model exists.

---

**Lab**

Write a short plan with:

1. one board you would start with
2. one framework you would start with
3. three examples you would run in order

Then explain why that order is better than jumping straight into a complex demo.

---

**Previous:** [Lecture 02](Lecture-02.md) | **Next:** [Lecture 04 - Solution tracks: AI, connectivity, peripherals, low power, and gateways](Lecture-04.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/Espressif/Education/Lecture/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/Espressif/Education/Lecture/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
