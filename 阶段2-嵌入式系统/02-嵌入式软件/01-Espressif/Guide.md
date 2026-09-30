---
title: Espressif
description: Espressif
---

# Espressif

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">ESP</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · 嵌入式系统</p>
<p class="course-identity__title">Espressif 专用课程标识。</p>
<p class="course-identity__meta">产物：bring-up（上电点亮/调通）或固件 demo · 度量：启动、延迟、功耗、可靠性</p>
</div>
</div>


面向希望**认真学习 Espressif 平台**、而不只是做孤立板级 demo 的工程师的结构化嵌入式软件路线。

本路线隶属于 **阶段 2 - 嵌入式软件**，因为 Espressif 开发天然融合了：

- MCU（微控制器）固件
- 外设与板级 bring-up
- 连接性
- FreeRTOS
- ESP-IDF
- 通过 Arduino 快速原型开发

它是主线 **ARM MCU / FreeRTOS / 总线** 内容与 **IoT** 子路线的天然配套。

---

## 为什么有这条路线

很多人学习 Espressif 平台的顺序是错的：

- 先照抄 Arduino sketch
- 然后加上 Wi-Fi 或低功耗蓝牙（BLE）示例
- 直到后来才意识到该平台包含：
  - 不同的 ESP32 系列 SoC
  - ESP-IDF
  - FreeRTOS
  - 多种板卡变体
  - 连接协议栈
  - RainMaker、ESP-SR、OpenThread 等解决方案框架

这就导致一些实际工程问题很难回答：

- 在 Espressif 生态里应该从哪里入手？
- 什么时候用 Arduino 就够了？
- 什么时候该转向 ESP-IDF？
- 官方教育材料之间如何衔接？
- 如何从入门示例走到产品方向？

本路线通过把 Espressif 学习拆成两条相互衔接的路径来解决这个问题。

---

## 你将学到什么

- `arduino-esp32` 如何融入 Espressif 技术栈。
- Espressif 官方教育路径如何组织学习。
- 如何在 Arduino、ESP-IDF 与更高级的解决方案路径之间做选择。
- 如何把板卡、框架、外设与连接性串成一份学习计划。
- 如何从首次板级 bring-up 走到产品形态的固件工作。

---

## 本路线的课程

### 1. Arduino-ESP32 基础

如果你的主要入口是 `espressif/arduino-esp32`，先看这个。

- [课程指南](02-讲座/README.md)
- 重点：支持的芯片、板级 bring-up、Arduino 分层、ESP-IDF component 路径

### 2. 官方教育路径

想跟随 Espressif 更完整的官方教育方向时看这个。

- [课程指南](01-教育/Guide.md)
- 重点：学习计划、开发环境、入门示例、解决方案路径与实用课程方向

---

## 推荐学习方式

推荐顺序：

1. 如果需要快速且具体的入门，先看 **Arduino-ESP32 基础**
2. 再走一遍 **官方教育路径**，把视野从 sketch 扩展到更大范围
3. 然后转向：
   - [IoT](../02-IoT/Guide.md)，用于 OpenThread 与 Zigbee
   - 之后的阶段 4 路线，用于 Jetson、FPGA 或更深入的 AI 部署

不要停在「编译通过」。要理解它周围的系统。

---

## 全篇使用的官方参考

- [Espressif Education](https://www.espressif.com/en/ecosystem/education)
- [Arduino core for the ESP32 family of SoCs - README](https://github.com/espressif/arduino-esp32)
- [Arduino-ESP32 在线文档](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [入门](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)
- [安装](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [库](https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html)
- [将 Arduino 作为 ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
- [迁移指南 2.x 到 3.0](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html)

---

**下一节：** [Arduino-ESP32 讲义](02-讲座/README.md) · [官方教育路径](01-教育/Guide.md)


<details>
<summary>English original</summary>

**Espressif**

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">ESP</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Embedded Systems</p>
<p class="course-identity__title">Specialized course identity for Espressif.</p>
<p class="course-identity__meta">Artifact: bring-up or firmware demo · Measure: boot, latency, power, reliability</p>
</div>
</div>


A structured embedded-software track for engineers who want to learn **Espressif platforms seriously**, not just as isolated board demos.

This track sits under **Phase 2 - Embedded Software** because Espressif development naturally combines:

- MCU firmware
- peripherals and board bring-up
- connectivity
- FreeRTOS
- ESP-IDF
- rapid prototyping through Arduino

It is the natural companion to the main **ARM MCU / FreeRTOS / buses** material and to the **IoT** subtrack.

---

**Why this track exists**

Many people learn Espressif platforms in the wrong order:

- first by copying Arduino sketches
- then by adding Wi-Fi or BLE examples
- only later by realizing the platform includes:
  - different ESP32-family SoCs
  - ESP-IDF
  - FreeRTOS
  - board variants
  - connectivity stacks
  - solution frameworks like RainMaker, ESP-SR, and OpenThread

That makes it hard to answer practical engineering questions like:

- where should I start in the Espressif ecosystem?
- when is Arduino enough?
- when should I move to ESP-IDF?
- how do the official education materials fit together?
- how do I go from starter examples to a product direction?

This track fixes that by splitting Espressif learning into two connected paths.

---

**What you will learn**

- How `arduino-esp32` fits into the Espressif stack.
- How Espressif’s official education path organizes learning.
- How to choose between Arduino, ESP-IDF, and more advanced solution paths.
- How to connect boards, frameworks, peripherals, and connectivity into one learning plan.
- How to move from first board bring-up to product-shaped firmware work.

---

**Courses in this track**

**1. Arduino-ESP32 foundation**

Use this first if your main entry point is `espressif/arduino-esp32`.

- [Course guide](02-讲座/README.md)
- focus: supported chips, board bring-up, Arduino layering, ESP-IDF component path

**2. Official education path**

Use this to follow Espressif’s broader official education direction.

- [Course guide](01-教育/Guide.md)
- focus: study plan, development environments, starter examples, solution tracks, and practical course directions

---

**Recommended study pattern**

Recommended order:

1. start with the **Arduino-ESP32 foundation** if you need a fast and concrete entry
2. work through the **official education path** to widen your view beyond sketches
3. branch toward:
   - [IoT](../02-IoT/Guide.md) for OpenThread and Zigbee
   - later Phase 4 tracks for Jetson, FPGA, or deeper AI deployment

Do not stop at "it compiled." Understand the system around it.

---

**Official references used throughout**

- [Espressif Education](https://www.espressif.com/en/ecosystem/education)
- [Arduino core for the ESP32 family of SoCs - README](https://github.com/espressif/arduino-esp32)
- [Arduino-ESP32 online documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [Getting Started](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)
- [Installing](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [Libraries](https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html)
- [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
- [Migration guide 2.x to 3.0](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html)

---

**Next:** [Arduino-ESP32 lectures](02-讲座/README.md) · [Official education path](01-教育/Guide.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/Espressif/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/Espressif/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
