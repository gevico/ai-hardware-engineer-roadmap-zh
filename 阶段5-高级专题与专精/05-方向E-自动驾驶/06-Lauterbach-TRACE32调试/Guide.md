---
title: Lauterbach TRACE32® Debug（自动驾驶 — 高级工具链）
description: Lauterbach TRACE32® Debug（自动驾驶 — 高级工具链）
---

# Lauterbach TRACE32® Debug（自动驾驶 — 高级工具链）

<div class="course-identity auto-course" style="--course-accent: #65a30d; --course-accent-rgb: 101, 163, 13;" markdown="1">
<div class="course-identity__icon">LTDA</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入专题 · 专项方向</p>
<p class="course-identity__title">Lauterbach TRACE32® Debug（自动驾驶 — 高级工具链）的专项课程标识。</p>
<p class="course-identity__meta">产物：专项案例研究 · 衡量：性能、可靠性、岗位匹配度</p>
</div>
</div>


**上级：** [阶段 5 — 自动驾驶](../Guide.md) · 可选的专业深度

**前置要求：** [阶段 1 — 操作系统](../../../阶段1-基础知识/03-操作系统/Guide.md)（boot、JTAG 概念）、[阶段 2 — 嵌入式 Linux](../../../阶段2-嵌入式系统/03-嵌入式Linux/Guide.md) 与 [阶段 2 — 嵌入式软件](../../../阶段2-嵌入式系统/02-嵌入式软件/Guide.md)（MCU（微控制器）、RTOS、bring-up（上电点亮/调通））。与 **车载 ECU**、**功能安全** 和 **芯片验证** 岗位高度重叠。

**TRACE32®** 是 **Lauterbach GmbH** 的注册商标。本路线图页面仅供 **教学** 用途；功能名称与培训均以 **厂商文档** 为准。

---

## 为什么它属于自动驾驶

**TRACE32** 并不是要替代健康 Linux 应用上的 **GDB**——它是一类**在电路调试与 trace** 工具，用于以下情况：

- 需要在 **bare-metal**、**RTOS** 或 **早期 bootloader** 代码上进行 **硬件辅助** 的运行控制，而这些代码没有 OS 调试器可用。
- 必须在故障后捕获 **非侵入式指令 trace**（ETM、PTM、Nexus 等），用于 **时序**、**覆盖率** 或 **post-mortem** 分析。
- 你从事 **车载** 或 **工业** SoC（**AURIX™ TriCore**、**ARM Cortex-R/M/A**、**Renesas RH850**、**RISC-V** 等众多平台）相关工作，而 **OEM/Tier-1** 流程统一采用 **Lauterbach** 或同类 **ICE** 工具。
- **安全** 论证（如 **ISO 26262** 证据）需要 **结构覆盖率** 或 **执行 trace**，这是纯软件工具无法单独提供的。

把本学习方向视为 **资深嵌入式 / 固件 / 验证** 工程师的 **工具素养**——它不能替代官方的 **Lauterbach 培训** 或你项目的 **安全计划**。

---

## TRACE32 是什么（心智模型）

| 层 | 作用 |
|-------|------|
| **调试探针** | 与目标之间的硬件接口（**JTAG**、**cJTAG**、**SWD**、**DAP**、厂商专有）——把 PC 主机连接到 SoC 的 **调试端口**。 |
| **TRACE32 软件** | **IDE + 调试器 + trace 查看器 + 脚本**（Practice / **PRACTICE** 语言）——加载符号、断点、内存、外设、多核同步。 |
| **Trace（可选）** | **片上 trace 缓冲**（ETB、**ETF**）或 **并行 trace 端口**（**TPIU**）+ **trace 探针**——**无需** 停止 CPU 即可重建程序流、时序与总线活动（取决于配置）。 |

**与开源方案相比：** **OpenOCD** + **GDB** 能以低成本覆盖许多 **MCU** bring-up 场景。**TRACE32** 通常在需要 **厂商芯片** 提供 **复杂 trace**、**多核 lockstep**、**车载** 生态支持，或需要 **专职 FAE** 配合的场合更胜一筹。

---

## 需要构建的核心技能

1. **目标连接** — 上电时序、**复位**、**调试连接器** 引脚定义、**适配器**、**目标电压**、**JTAG 链** 发现。  
2. **运行控制** — 单步、go、halt、**断点**（硬件与软件）、**观察点**、**条件** 断点。  
3. **内存与寄存器** — 核心 **GPR**、**特殊寄存器**、**MMIO**、**缓存** / **内存保护单元（MPU）** 意识（invalidate 与“陈旧”视图之别）。  
4. **多核** — **SMP** / **非对称多处理（AMP）** / **lockstep** 视图；在架构要求时进行 **同步** 运行控制。  
5. **Trace** — 使能 **ETM**（或等效机制）、确定 **buffer** 大小、设置 **触发**（例如围绕故障）、**导出** 以供分析；理解 **你的** PCB 上的 **trace 时钟** 与 **引脚** 要求。  
6. **脚本** — 用 **PRACTICE** 或主机侧脚本自动化 **回归** 调试（烧写、启动、测试、捕获 trace）。  
7. **RTOS 感知** — 当 **kernel** 暴露出正确的 **符号** 且你的 RTOS 有对应 **插件** 时，进行 **任务感知** 调试。

---


<details>
<summary>English original</summary>

**Lauterbach TRACE32® Debug (Autonomous Driving — advanced tooling)**

<div class="course-identity auto-course" style="--course-accent: #65a30d; --course-accent-rgb: 101, 163, 13;" markdown="1">
<div class="course-identity__icon">LTDA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for Lauterbach TRACE32® Debug (Autonomous Driving — advanced tooling).</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [Phase 5 — Autonomous Driving](../Guide.md) · Optional professional depth

**Prerequisites:** [Phase 1 — Operating Systems](../../../阶段1-基础知识/03-操作系统/Guide.md) (boot, JTAG concepts), [Phase 2 — Embedded Linux](../../../阶段2-嵌入式系统/03-嵌入式Linux/Guide.md) and [Phase 2 — Embedded Software](../../../阶段2-嵌入式系统/02-嵌入式软件/Guide.md) (MCU, RTOS, bring-up). Strong overlap with **automotive ECU**, **functional safety**, and **silicon validation** roles.

**TRACE32®** is a registered trademark of **Lauterbach GmbH**. This roadmap page is **educational**; feature names and training follow **vendor documentation**.

---

**Why this belongs in Autonomous Driving**

**TRACE32** is not a replacement for **GDB** on a healthy Linux app—it is a **class of in-circuit debug and trace** used when:

- You need **hardware-assisted** run-control on **bare-metal**, **RTOS**, or **early bootloader** code where no OS debugger exists.
- You must capture **non-intrusive instruction trace** (ETM, PTM, Nexus, etc.) for **timing**, **coverage**, or **post-mortem** analysis after a fault.
- You work on **automotive** or **industrial** SoCs (**AURIX™ TriCore**, **ARM Cortex-R/M/A**, **Renesas RH850**, **RISC-V**, and many others) where **OEM/Tier-1** flows standardize on **Lauterbach** or equivalent **ICE** tools.
- **Safety** arguments (e.g. **ISO 26262** evidence) require **structural coverage** or **execution trace** that software-only tools cannot provide alone.

Treat this track as **tooling literacy** for **senior embedded / firmware / validation** engineers—not a substitute for official **Lauterbach training** or your project’s **safety plan**.

---

**What TRACE32 is (mental model)**

| Layer | Role |
|-------|------|
| **Debug probe** | Hardware interface to the target (**JTAG**, **cJTAG**, **SWD**, **DAP**, vendor-specific) — connects PC host to SoC **debug port**. |
| **TRACE32 software** | **IDE + debugger + trace viewer + scripting** (Practice / **PRACTICE** language) — load symbols, breakpoints, memory, peripherals, multicore sync. |
| **Trace (optional)** | **On-chip trace buffer** (ETB, **ETF**) or **parallel trace port** (**TPIU**) + **trace probe** — reconstruct program flow, timing, and bus activity **without** stopping the CPU (configuration-dependent). |

**Compared to open-source stacks:** **OpenOCD** + **GDB** cover many **MCU** bring-up scenarios at low cost. **TRACE32** typically wins where **vendor silicon** ships **complex trace**, **multicore lockstep**, **automotive** ecosystem support, or **dedicated FAE** engagement is required.

---

**Core skills to build**

1. **Target connection** — Power sequencing, **reset**, **debug connector** pinout, **adapters**, **target voltage**, **JTAG chain** discovery.  
2. **Run-control** — Step, go, halt, **breakpoints** (hardware vs software), **watchpoints**, **conditional** breaks.  
3. **Memory and registers** — Core **GPRs**, **special registers**, **MMIO**, **cache** / **MPU** awareness (invalidate vs “stale” views).  
4. **Multicore** — **SMP** / **AMP** / **lockstep** views; **synchronized** run-control where the architecture requires it.  
5. **Trace** — Enable **ETM** (or equivalent), size **buffer**, **trigger** (e.g. around fault), **export** for analysis; understand **trace clock** and **pin** requirements on **your** PCB.  
6. **Scripting** — Automate **regression** debug (flash, boot, test, capture trace) with **PRACTICE** or host-side scripts.  
7. **RTOS awareness** — **Task-aware** debugging when the **kernel** exposes the right **symbols** and **plugins** exist for your RTOS.

---

</details>

## 建议的学习路径

| 阶段 | 行动 |
|-------|--------|
| **1** | 阅读 **Lauterbach** 的「Debugger Basics」/「Getting Started」，针对你在用的**某一**架构（如 **ARM**、**TriCore**）。 |
| **2** | 在一块 **Blinky** 已知正常的**实验板**上连接探头，验证 **CPU** 检测，以及适用时的 **flash** 烧写。 |
| **3** | 在**中断**和 **main** 中练习**断点**；在一个**状态寄存器**上加 **watchpoint**。 |
| **4** | 若硬件支持，完成一个 **trace** 实验：在函数入口设 **trigger**，**decode** **PC** 时间线。 |
| **5** | 把 **TRACE32** 的用法映射到**你**所处的 **V-model** 阶段：**unit**、**integration** 还是 **HIL**（最终 **ECU** 构建中 **trace** 常被限制或禁用——先弄清你的 **OEM** 规则）。 |

**官方入口（请核实当前 URL）：**

- [Lauterbach — TRACE32](https://www.lauterbach.com/trace32.html) — 产品概览与文档索引。  
- 各架构专属的 **PDF 手册**（ARM、PowerPC、TriCore、RH850、RISC-V、……），必要时注册后从 Lauterbach 文档门户获取。

---

## 硬件检查清单（针对你自己的载板 / ECU）

- **原理图**上的**调试连接器**（**Tag-Connect**、**MIPI-10/20**、**Samtec**、**OEM 专用**）——**trace** 级信号**不要**依赖**手工焊接**的飞线。  
- **专用**的 **JTAG/SWD** **引脚**，量产中**不得**与 **GPIO** 复用，除非 **strap** 允许安全 **bring-up**（上电点亮/调通）。  
- 对**并行 trace**：按 **SoC** 与**探头**手册做**等长**的 **trace** **走线**、**参考** **VTREF**、**地**以及**屏蔽**。  
- 当**调试器**「无法 attach」时，**复位**与 **power-good** 要能在**示波器**上**可观测**。

---

## 与其他路线图模块的关系

| 模块 | 关联 |
|--------|------------|
| **阶段 5 — 自动驾驶** | **ECU** / **VCU** 固件、与 **openpilot** 相近的技术栈，常把**应用**调试（Linux）与**另一颗芯片**上的 **MCU** **JTAG** 配合使用。 |
| **阶段 4 — L4T / Jetson** | **Jetson** 的 bring-up（上电点亮/调通）通常靠 **UART**、**USB recovery** 和 **Linux** 调试器；**TRACE32** 更多见于 **ARM/RISC-V MCU** 和**汽车** **SoC**，而非 **Orin** 应用处理器——但若你要碰**安全**类的**伴随** **MCU** 或**客户** **silicon**，它仍有价值。 |
| **阶段 5 — AI 芯片设计** | **Silicon bring-up**（上电点亮/调通）与 **RTL/验证**团队常在**仿真**之外配合使用**业界** **ICE** 工具。 |

---

## 项目（可选）

- **Bring-up 脚本** — 一个 **PRACTICE** 脚本：attach、加载 **ELF**、在 **`main`** 上设**断点**、运行、打印**栈**。  
- **Trace 小报告** — 捕获**定时器 ISR** 前后 **10 ms** 的 **trace**；在一份简短的 markdown 笔记里标注 **ISR** 的**延迟**。 |
- **对比笔记** — 一页：在**同一块** **MCU** 板上，针对**你**的痛点（attach 时间、**RTOS** 插件、**trace**）比较 **OpenOCD+GDB** 与 **TRACE32**。

---

## 小结

**Lauterbach TRACE32®** 是面向**严肃** **SoC** 与 **ECU** 工作的**高级嵌入式调试与 trace** 工具。当你从**「printf 加 GDB」**迈向**多核**、**silicon 验证**、**汽车**或与**安全**相关的**证据**时，把它加进你的路线图——并要为**探头硬件**、**许可证**和**厂商培训**预留预算。


<details>
<summary>English original</summary>

**Suggested learning path**

| Stage | Action |
|-------|--------|
| **1** | Read **Lauterbach** “Debugger Basics” / **Getting Started** for **one** architecture you use (e.g. **ARM**, **TriCore**). |
| **2** | On a **lab board** with known-good **Blinky**, connect probe, verify **CPU** detection and **flash** programming if applicable. |
| **3** | Exercise **breakpoints** in **interrupt** and **main**; add **watchpoint** on a **status register**. |
| **4** | If hardware supports it, complete one **trace** lab: **trigger** on function entry, **decode** **PC** timeline. |
| **5** | Map **TRACE32** usage to **your** **V-model** phase: **unit** vs **integration** vs **HIL** (often **trace** is constrained or disabled in final **ECU** builds—know your **OEM** rules). |

**Official entry points (verify current URLs):**

- [Lauterbach — TRACE32](https://www.lauterbach.com/trace32.html) — product overview and documentation index.  
- Architecture-specific **PDF manuals** (ARM, PowerPC, TriCore, RH850, RISC-V, …) from Lauterbach’s documentation portal after registration if required.

---

**Hardware checklist (for your own carrier / ECU)**

- **Debug connector** on **schematic** (**Tag-Connect**, **MIPI-10/20**, **Samtec**, **OEM-specific**) — **do not** rely on **handsolder** wires for **trace**-grade signals.  
- **Dedicated** **JTAG/SWD** **pins** **not** multiplexed with **GPIO** used in production unless **straps** allow safe **bring-up**.  
- For **parallel trace**: **length-matched** **trace** **lines**, **reference** **VTREF**, **ground**, and **shielding** per **SoC** and **probe** manual.  
- **Reset** and **power-good** **observable** on **scope** when **debugger** “cannot attach.”

---

**Relationship to other roadmap modules**

| Module | Connection |
|--------|------------|
| **Phase 5 — Autonomous Driving** | **ECU** / **VCU** firmware, **openpilot**-adjacent stacks often pair **application** debug (Linux) with **MCU** **JTAG** on separate chips. |
| **Phase 4 — L4T / Jetson** | **Jetson** bring-up is usually **UART**, **USB recovery**, and **Linux** debuggers; **TRACE32** is more typical on **ARM/RISC-V MCUs** and **automotive** **SoCs** than on **Orin** application processors—still valuable if you touch **safety** **companion** **MCUs** or **customer** **silicon**. |
| **Phase 5 — AI Chip Design** | **Silicon bring-up** and **RTL/verification** teams often use **industry** **ICE** tools alongside **simulation**. |

---

**Projects (optional)**

- **Bring-up script** — One **PRACTICE** script: attach, load **ELF**, set **breakpoint** on **`main`**, run, print **stack**.  
- **Trace mini-report** — Capture **10 ms** of **trace** around a **timer ISR**; annotate **ISR** **latency** in a short markdown note.  
- **Comparison note** — One page: **OpenOCD+GDB** vs **TRACE32** on the **same** **MCU** board for **your** pain points (attach time, **RTOS** plugins, **trace**).

---

**Summary**

**Lauterbach TRACE32®** is **advanced embedded debug and trace** tooling for **serious** **SoC** and **ECU** work. Add it to your roadmap when you move from **“printf and GDB”** to **multicore**, **silicon validation**, **automotive**, or **safety**-adjacent **evidence**—and budget for **probe hardware**, **licenses**, and **vendor training**.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/6. Lauterbach TRACE32 Debug/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/6.%20Lauterbach%20TRACE32%20Debug/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
