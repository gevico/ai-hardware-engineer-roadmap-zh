---
title: 第 2 讲 - ESP32 路径的嵌入式开发基础
description: 第 2 讲 - ESP32 路径的嵌入式开发基础
---

# 第 2 讲 - ESP32 路径的嵌入式开发基础

**课程：** [官方教育路径指南](../Guide.md) | **阶段 2 - 嵌入式软件**

**上一讲：** [第 01 讲](Lecture-01.md) | **下一讲：** [第 03 讲 - ESP32 入门：芯片、开发板、烧录与首批示例](Lecture-03.md)

---

## 本讲为何重要

在乐鑫官方教育页面上，学习计划从 **嵌入式开发基础** 开始。

这是正确的选择。

在谈论以下内容之前：

- Wi-Fi
- BLE
- Matter
- Zigbee
- 语音

你仍然需要底层的**嵌入式基础**。

---

## 官方学习计划强调什么

乐鑫的学习计划部分包含如下基础内容：

- 电压、电流、电阻、电容
- 常见元器件
- C 语言
- 函数、指针、内存
- 项目编译与链接
- GPIO、timer、UART、SPI、I2C
- TCP 与 UDP
- Git
- FreeRTOS
- Linux 指令

这份清单是一个有力的提醒：

> ESP32 开发仍然是嵌入式系统，而不只是“无线应用代码”。

---

## 如何把这些基础映射到本路线图

如果你已经走完阶段 1 和嵌入式软件主模块，就不需要在这里从零重学全部内容。

相反，把乐鑫官方清单当作一份**检查清单**：

### 本路线图已覆盖

- C 与系统编程基础
- MCU 与外设思维
- FreeRTOS
- UART、SPI、I2C 等总线
- 基本网络概念

### 乐鑫补充的内容

- 一个让上述所有部分交汇的具体平台
- 真实的无线 SoC
- 面向实践的云与协议方案路径
- 面向产品的套件与示例

---

## 重要的思维方式

看到 ESP32 示例时，要问：

- 真正用的是哪条总线？
- 这里隐含了哪些内存假设？
- 这是阻塞式还是事件驱动？
- 哪部分是与开发板相关的？
- FreeRTOS 在哪里隐式出现？

这样你才能把示例转化为**工程理解**。

---

## 不要做什么

不要用乐鑫的高层示例来掩盖**薄弱的基础**。

如果你不理解：

- UART
- I2C
- GPIO
- 中断
- 内存

那么高级 ESP32 示例依然会像魔法一样。

而魔法是脆弱的。

---

## 真正的教训

官方教育路径从基础开始，是因为**联网产品总是栽在基础错误上**：

- 引脚接错
- 电压假设错误
- 时序不佳
- 任务划分不当
- 通信处理薄弱

这就是本讲排在前面位置的原因。

---

## 实验

写一份检查清单，包含以下分组：

- C / 内存
- 总线 / 外设
- 网络基础
- RTOS / 项目工具

针对每一组，写下：

- 我已经掌握的
- 在开展复杂 ESP32 项目之前仍需加强的

---

**上一讲：** [第 01 讲](Lecture-01.md) | **下一讲：** [第 03 讲 - ESP32 入门：芯片、开发板、烧录与首批示例](Lecture-03.md)


<details>
<summary>English original</summary>

**Lecture 2 - Embedded development basics for the ESP32 path**

**Course:** [Official education path guide](../Guide.md) | **Phase 2 - Embedded Software**

**Previous:** [Lecture 01](Lecture-01.md) | **Next:** [Lecture 03 - Getting started with ESP32: chips, boards, flashing, and first examples](Lecture-03.md)

---

**Why this lecture matters**

On Espressif’s official education page, the study plan starts with **Embedded Development Basics**.

That is the correct choice.

Before talking about:

- Wi-Fi
- BLE
- Matter
- Zigbee
- voice

you still need the **embedded foundation** underneath.

---

**What the official study plan emphasizes**

Espressif’s study-plan section includes basics like:

- voltage, current, resistance, capacitance
- common components
- C language
- functions, pointers, memory
- project compilation and linking
- GPIO, timer, UART, SPI, I2C
- TCP and UDP
- Git
- FreeRTOS
- Linux instructions

That list is a strong reminder:

> ESP32 development is still embedded systems, not just “wireless app code.”

---

**How to map those basics into this roadmap**

You do not need to relearn everything here from scratch if you already worked through Phase 1 and the main Embedded Software module.

Instead, use the official Espressif list as a **checklist**:

**Already covered by our roadmap**

- C and system programming foundations
- MCU and peripheral thinking
- FreeRTOS
- buses like UART, SPI, I2C
- basic networking concepts

**What Espressif adds**

- a concrete platform where all those pieces meet
- real wireless SoCs
- practical cloud and protocol solution paths
- product-oriented kits and examples

---

**The important mindset**

When you see an ESP32 example, ask:

- which bus is really being used?
- what memory assumptions are hidden here?
- is this blocking or event-driven?
- what part is board-specific?
- where does FreeRTOS show up implicitly?

That is how you turn examples into **engineering understanding**.

---

**What not to do**

Do not use Espressif’s higher-level examples to hide **weak fundamentals**.

If you do not understand:

- UART
- I2C
- GPIO
- interrupts
- memory

then advanced ESP32 examples will still feel like magic.

And magic is fragile.

---

**The real lesson**

The official education path starts with basics because **connected products fail on basic mistakes** all the time:

- wrong pins
- wrong voltage assumptions
- poor timing
- incorrect tasking
- weak communication handling

That is why this lecture belongs early.

---

**Lab**

Write a checklist with these groups:

- C / memory
- buses / peripherals
- networking basics
- RTOS / project tools

For each group, write:

- what I already know
- what I still need to strengthen before complex ESP32 projects

---

**Previous:** [Lecture 01](Lecture-01.md) | **Next:** [Lecture 03 - Getting started with ESP32: chips, boards, flashing, and first examples](Lecture-03.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/Espressif/Education/Lecture/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/Espressif/Education/Lecture/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
