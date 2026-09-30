---
title: 阶段 2：嵌入式系统
description: 阶段 2：嵌入式系统
---

# 阶段 2：嵌入式系统

<div class="course-identity embedded-systems" markdown="1">
<div class="course-identity__icon">EMB</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 2 · 嵌入式系统</p>
<p class="course-identity__title">从抽象计算走向板卡、总线、启动流程、受限设备与产品。</p>
<p class="course-identity__meta">产物：bring-up 日志 + 嵌入式 demo · 度量：启动、延迟、功耗、可靠性</p>
</div>
</div>


> *从抽象计算模型走向真实板卡、总线、启动流程，以及必须在实验室之外正常工作的受限系统。*

**层映射：** 主要落在 **L4**（固件、RTOS、BSP、嵌入式 Linux），并直接连接到 **L3**（驱动、DMA、设备接口）与 **L1**（边缘 AI 部署）。

**岗位目标：** 嵌入式软件工程师 · RTOS 工程师 · BSP 工程师 · 嵌入式 Linux 工程师 · 边缘 AI 工程师

**前置要求：** [阶段 1 — 数字基础](../阶段1-基础知识/Guide.md)

**后续内容：** [阶段 3 — 人工智能](../阶段3-人工智能/Guide.md)，然后是阶段 4 的其中一条路线：[Xilinx FPGA](../阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md)、[NVIDIA Jetson](../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md) 或 [ML 编译器](../阶段4-路线C-深度学习推理优化/Guide.md)

---

## 为什么要有这个阶段

AI 硬件并非孤立存在。它被装入系统出货，而这些系统必须：

- 可靠启动
- 与传感器和外设通信
- 在功耗与热管理限制下存活
- 在现场安全更新
- 与 Linux、RTOS 以及板级专用软件栈集成

本阶段讲授的是原始硬件知识与可部署产品之间的那一层工程。

---

## 阶段结构

| # | 模块 | 你会学到什么 | 为什么重要 |
|---|--------|----------------|----------------|
| **1** | [原理图绘制与 PCB 设计](01-原理图与PCB设计/Guide.md) | 板级设计、接口、布局布线基础、bring-up（上电点亮/调通）关注点 | AI 产品仍然依赖真实的硬件集成 |
| **2** | [嵌入式软件](02-嵌入式软件/Guide.md) | Cortex-M、FreeRTOS、中断、DMA、SPI/I2C/UART/CAN 等总线 | 围绕传感器、外设和低层设备的控制平面 |
| **3** | [嵌入式 Linux](03-嵌入式Linux/Guide.md) | Yocto、BSP 定制、rootfs、kernel 集成、生产级 Linux | Jetson、机器人与边缘设备的软件基础 |
| **4** | [产品设计](04-产品设计/Guide.md) | 产品角色、信任线索、房间内行为、安装配置与系统边界 | 嵌入式系统做得再好，若沦为弱产品仍然会失败 |

**推荐顺序：** `1 → 2 → 3 → 4`

若使用开发套件而非自研板卡，模块 2 可与模块 1 的部分内容并行开始。当你已经积累足够的软硬件背景，能够权衡取舍而非只对比功能时，模块 4 的价值最大。

---

## 你应该产出什么

到本阶段结束时，你至少应具备：

- 一个面向硬件或接口的产物，例如原理图评审、接口映射或 bring-up 检查清单
- 一个 MCU/RTOS 产物，例如外设驱动、ISR 设计说明或 FreeRTOS 项目
- 一个嵌入式 Linux 产物，例如 Yocto 镜像构建说明、设备树改动、rootfs 定制或 BSP 文档
- 一个产品产物，例如 V1 设计简报、信任模型或布局与控制方案论证

这些产出之所以重要，是因为嵌入式的可信度来自能跑起来的系统，而非泛泛的熟悉度。

---

## 达成标准

当你能做到以下各点时，就可以继续：

- 解释外设如何经由总线、中断和驱动到达软件
- 对 RTOS 任务化 vs 裸机 vs Linux 的取舍做出论证
- 指出 BSP、bootloader、设备树和 rootfs 定制在部署栈中各自的位置
- 至少解释一项涉及控制、信任、声学、布局或安装配置的产品取舍
- 产出至少一个可复现的 bring-up 或嵌入式 Linux 产物

---

## 谁应优先考虑本阶段

- **Jetson / 机器人 / 边缘 AI 方向：** 将本阶段视为必修
- **编译器优先的学习者：** 做到足以理解部署现实即可，然后进入阶段 3 与阶段 4C
- **FPGA / 加速器学习者：** 聚焦于把定制硬件接入可用系统的 Linux 与接口部分
- **消费设备 / 家庭 AI / 语音产品方向：** 不要跳过模块 4，因为房间角色与信任决策可能让其他方面扎实的工程前功尽弃

---


<details>
<summary>English original</summary>

**Phase 2: Embedded Systems**

<div class="course-identity embedded-systems" markdown="1">
<div class="course-identity__icon">EMB</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 2 · Embedded Systems</p>
<p class="course-identity__title">Move from abstract computing to boards, buses, boot flows, constrained devices, and products.</p>
<p class="course-identity__meta">Artifact: bring-up log + embedded demo · Measure: boot, latency, power, reliability</p>
</div>
</div>


> *Move from abstract computing models to real boards, buses, boot flows, and constrained systems that must work outside the lab.*

**Layer mapping:** Primarily **L4** (firmware, RTOS, BSP, embedded Linux), with direct connections into **L3** (drivers, DMA, device interfaces) and **L1** (edge AI deployment).

**Role targets:** Embedded Software Engineer · RTOS Engineer · BSP Engineer · Embedded Linux Engineer · Edge AI Engineer

**Prerequisites:** [Phase 1 — Digital Foundations](../阶段1-基础知识/Guide.md)

**What comes after:** [Phase 3 — Artificial Intelligence](../阶段3-人工智能/Guide.md), then one of the Phase 4 tracks: [Xilinx FPGA](../阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md), [NVIDIA Jetson](../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md), or [ML Compiler](../阶段4-路线C-深度学习推理优化/Guide.md)

---

**Why This Phase Exists**

AI hardware does not live in isolation. It ships inside systems that must:

- boot reliably
- talk to sensors and peripherals
- survive power and thermal limits
- update safely in the field
- integrate with Linux, RTOS, and board-specific software stacks

This phase teaches the engineering layer between raw hardware knowledge and deployable products.

---

**Phase Structure**

| # | Module | What you learn | Why it matters |
|---|--------|----------------|----------------|
| **1** | [Schematic Capture and PCB Design](01-原理图与PCB设计/Guide.md) | board-level design, interfaces, layout basics, bring-up concerns | AI products still depend on real hardware integration |
| **2** | [Embedded Software](02-嵌入式软件/Guide.md) | Cortex-M, FreeRTOS, interrupts, DMA, buses like SPI/I2C/UART/CAN | The control plane around sensors, peripherals, and low-level devices |
| **3** | [Embedded Linux](03-嵌入式Linux/Guide.md) | Yocto, BSP customization, rootfs, kernel integration, production Linux | The software foundation for Jetson, robotics, and edge devices |
| **4** | [Product Design](04-产品设计/Guide.md) | product role, trust cues, room behavior, setup, and system boundaries | Good embedded systems still fail if they become weak products |

**Recommended order:** `1 → 2 → 3 → 4`

If you are using development kits instead of custom boards, Module 2 can start in parallel with parts of Module 1. Module 4 is most valuable once you already have enough hardware and software context to reason about tradeoffs, not just features.

---

**What You Should Produce**

By the end of this phase, you should have at least:

- one hardware or interface-oriented artifact such as a schematic review, interface map, or bring-up checklist
- one MCU/RTOS artifact such as a peripheral driver, ISR design note, or FreeRTOS project
- one embedded Linux artifact such as a Yocto image build note, device-tree change, rootfs customization, or BSP write-up
- one product artifact such as a V1 design brief, trust model, or placement-and-controls rationale

These outputs matter because embedded credibility comes from working systems, not generic familiarity.

---

**Exit Criteria**

You are ready to continue when you can:

- explain how a peripheral reaches software through buses, interrupts, and drivers
- reason about RTOS tasking vs bare-metal vs Linux tradeoffs
- identify where BSP, bootloader, device tree, and rootfs customization fit in a deployment stack
- explain at least one product tradeoff involving controls, trust, acoustics, placement, or setup
- produce at least one reproducible bring-up or embedded Linux artifact

---

**Who Should Prioritize This Phase**

- **Jetson / robotics / edge AI targets:** treat this phase as mandatory
- **Compiler-first learners:** do enough to understand deployment realities, then move to Phase 3 and Phase 4C
- **FPGA / accelerator learners:** focus on the Linux and interface pieces that connect custom hardware to usable systems
- **Consumer device / home AI / voice product targets:** do not skip Module 4, because room-role and trust decisions can invalidate otherwise solid engineering

---

</details>

## 下一步

→ [**阶段 3 — 人工智能**](../阶段3-人工智能/Guide.md) · [**阶段 4 方向 A — Xilinx FPGA**](../阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md) · [**阶段 4 方向 B — NVIDIA Jetson**](../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md) · [**阶段 4 方向 C — ML 编译器**](../阶段4-路线C-深度学习推理优化/Guide.md)


<details>
<summary>English original</summary>

**Next**

→ [**Phase 3 — Artificial Intelligence**](../阶段3-人工智能/Guide.md) · [**Phase 4 Track A — Xilinx FPGA**](../阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md) · [**Phase 4 Track B — NVIDIA Jetson**](../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md) · [**Phase 4 Track C — ML Compiler**](../阶段4-路线C-深度学习推理优化/Guide.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
