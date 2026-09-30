---
title: 原理图绘制与 PCB 设计
description: 原理图绘制与 PCB 设计
---

# 原理图绘制与 PCB 设计

<div class="course-identity pcb-design" markdown="1">
<div class="course-identity__icon">PCB</div>
<div markdown="1">
<p class="course-identity__eyebrow">模块 1 · 原理图与 PCB 设计</p>
<p class="course-identity__title">阅读、评审并设计让 AI 硬件可用的板级接口。</p>
<p class="course-identity__meta">产物：原理图评审或接口映射 · 度量：约束、风险、bring-up（上电点亮/调通）检查</p>
</div>
</div>


**阶段 2，第 1 节** —— 把 **netlist 意图** 变成一块 **已制造出来的板子**，可以上电、探测，之后再编程。它位于阶段 1（数字 + HDL + 架构直觉）之后，[**ARM MCU、FreeRTOS 与协议**](../02-嵌入式软件/Guide.md) 之前，那里默认已有一块能工作的 PCB 或开发板。

---

## 1. 工具与库

* **EDA 选型：** KiCad（开源、社区项目常用）、Altium/Cadence（工业界），或同类工具 —— 选定一套工具链，深入学习其库、原理图与板级编辑器。
* **符号与封装：** 核对过的封装与数据手册尺寸对照；用于机械检查的 3D 模型；项目 + 库的版本控制（如有需要，用 Git LFS 管理大文件）。
* **设计规则：** 尽早录入电气规则（网络类、间距），让布局布线工具一致地执行。

---

## 2. 原理图绘制

* **先画框图：** 电源树、处理器/MCU、传感器、通信（USB、Ethernet、CAN）、稳压器、时钟、复位与测试点。
* **纸面上的电源完整性：** 每路电源轨的去耦策略（大电容 + 陶瓷电容的布局预算）；使用多路供电时的上电时序。
* **接口：** 电平转换；连接器出板处的 ESD；在合适处为调试/串联端接加串联电阻。
* **可测试性设计：** 测试焊盘、0 Ω 跳线、为返修预留的可选未贴装封装。

---

## 3. PCB 布局布线（入门到中级）

* **叠层：** 层数、平面分配（地/电源）、走高速信号（USB、DDR、Ethernet PHY）时的受控阻抗。
* **布局：** 缩短关键路径（开关稳压器、晶振到 MCU、高速差分对）；发热器件下方加散热过孔。
* **布线：** 差分阻抗、规范要求时的等长匹配、回流路径与地平面分割（避免跨分割）。
* **DFM / DFA：** 外廓间距、基准点、拼板意识、便于装配的封装（手工焊接 vs 回流焊）。

---

## 4. 输出物与交接

* **制造包：** Gerber/OBD++、钻孔文件、适用时的贴片坐标文件、带叠层说明的制造图。
* **BOM：** 制造商料号、替代料、生命周期意识。
* **bring-up 计划：** 先电源轨（限流电源），再时钟/复位，然后编程/调试连接器 —— 直接对接 **第 2 节** 的固件工作。

---

## 资源

* 针对你的 MCU/PMIC/PHY 的厂商应用笔记（布局示例往往是最好的教材）。
* 当“经验法则”不够用时，参考 *High-Speed Digital Design* / *Signal and Power Integrity*。

---

## 阶段 2 后续

**[嵌入式软件 —— ARM MCU、FreeRTOS、协议](../02-嵌入式软件/Guide.md)**


<details>
<summary>English original</summary>

**Schematic Capture and PCB Design**

<div class="course-identity pcb-design" markdown="1">
<div class="course-identity__icon">PCB</div>
<div markdown="1">
<p class="course-identity__eyebrow">Module 1 · Schematic & PCB Design</p>
<p class="course-identity__title">Read, review, and design the board-level interfaces that make AI hardware usable.</p>
<p class="course-identity__meta">Artifact: schematic review or interface map · Measure: constraints, risks, bring-up checks</p>
</div>
</div>


**Phase 2, section 1** — turn a **netlist intent** into a **fabricated board** you can power, probe, and later program. This sits after Phase 1 (digital + HDL + architecture intuition) and before [**ARM MCU, FreeRTOS, and protocols**](../02-嵌入式软件/Guide.md), where you assume a working PCB or dev kit.

---

**1. Tooling and libraries**

* **EDA choice:** KiCad (open, common in community projects), Altium/Cadence (industry), or comparable — pick one stack and learn its library, schematic, and board editors deeply.
* **Symbols and footprints:** Verified footprints vs datasheet dimensions; 3D models for mechanical check; revision control for project + libraries (Git LFS for large assets if needed).
* **Design rules:** Capture electrical rules (net classes, clearances) early so the layout tool enforces them consistently.

---

**2. Schematic capture**

* **Block diagram first:** Power tree, processors/MCUs, sensors, comms (USB, Ethernet, CAN), regulators, clocks, reset, and test points.
* **Power integrity on paper:** Decoupling strategy per rail (bulk + ceramic placement budget); sequencing if you use multiple supplies.
* **Interfaces:** Level shifting, ESD where connectors leave the board, series resistors for debug/series termination where appropriate.
* **Design for test:** Test pads, 0 Ω jumpers, optional unpop footprints for rework.

---

**3. PCB layout (introductory through intermediate)**

* **Stackup:** Layer count, plane assignment (ground/power), controlled impedance when you run high-speed signals (USB, DDR, Ethernet PHY).
* **Placement:** Short critical paths (switching regulators, crystal to MCU, high-speed differential pairs), thermal vias under hot parts.
* **Routing:** Differential impedance, length matching where specs demand it, return paths and splits in ground (avoid crossing gaps).
* **DFM / DFA:** Courtyard clearances, fiducials, panelization awareness, assembly-friendly footprints (hand vs reflow).

---

**4. Outputs and handoff**

* **Fab package:** Gerbers/OBD++, drill files, pick-and-place if applicable, fab drawing with stackup notes.
* **BOM:** Manufacturer part numbers, alternates, lifecycle awareness.
* **Bring-up plan:** Power rails first (current-limited supply), then clocks/reset, then programming/debug connectors — feeds directly into firmware work in **section 2**.

---

**Resources**

* Vendor app notes for your MCU/PMIC/PHY (layout examples are often the best teaching).
* *High-Speed Digital Design* / *Signal and Power Integrity* references when you outgrow “rules of thumb.”

---

**Next in Phase 2**

**[Embedded Software — ARM MCU, FreeRTOS, protocols](../02-嵌入式软件/Guide.md)**

</details>

---

> 原文：[`Phase 2 - Embedded Systems/1. Schematic Capture and PCB Design/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/1.%20Schematic%20Capture%20and%20PCB%20Design/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
