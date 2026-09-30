---
title: 1. Xilinx FPGA 开发
description: 1. Xilinx FPGA 开发
---

# 1. Xilinx FPGA 开发

<div class="course-identity fpga-track" markdown="1">
<div class="course-identity__icon">FPGA</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 A1 · Xilinx FPGA 开发</p>
<p class="course-identity__title">打通 FPGA 环路：RTL、仿真、综合、实现、时序、比特流、板级调试。</p>
<p class="course-identity__meta">产物：经板卡验证的 RTL 工程 · 度量：时序、利用率、调试抓取</p>
</div>
</div>


> 通过构建、仿真、时序分析、调试并记录真实 RTL 项目，学习 Xilinx FPGA 流程。

**层级映射：** L5-L6。本模块串联 RTL 设计、FPGA 实现、时序收敛、片上调试与硬件验证。

**岗位目标：** FPGA 工程师 · RTL 设计工程师 · 硬件加速工程师 · AI 加速器原型工程师

**前置要求：** [数字设计与 HDL](../../阶段1-基础知识/01-数字设计与HDL/Guide.md)、[计算机体系结构](../../阶段1-基础知识/02-计算机体系结构与硬件/Guide.md)，以及基本的命令行 Git。

**后续内容：** [Zynq UltraScale+ MPSoC](../02-Zynq-UltraScale-MPSoC/Guide.md)、[高级 FPGA 设计](../03-高级FPGA设计/Guide.md)，以及 [高层次综合](../04-高层次综合HLS/Guide.md)。

---

## 本模块为何存在

Vivado 本身不是技能。技能是把一个硬件构想变成经过验证、能在板卡上运行且满足时序的比特流。

本模块讲授完整的 FPGA 环路：

```text
RTL -> simulation -> synthesis -> implementation -> timing -> bitstream -> board debug -> report
```

不要把工具当成点按钮的 IDE。要把它当成一条工程流程，产出其他硬件工程师可以评审的产物。

---

## 课程成果

学完后，你应能够：

- 创建干净的 Vivado 工程并纳入版本控制
- 为小型模块编写可综合的 Verilog/SystemVerilog 或 VHDL
- 搭建自检 testbench
- 读懂综合、利用率、时序与功耗报告
- 正确约束时钟与基本 I/O
- 在仿真中和硬件上调试设计
- 打包可复用 IP 块并附文档
- 说明 RTL 仿真与实现后硬件之间有哪些变化

---

## 单元地图

| 单元 | 重点 | 产物 |
|------|-------|----------|
| 1 | Vivado 工程流程 | 可复现的工程骨架 |
| 2 | RTL 与仿真 | 自检 testbench 与波形抓取 |
| 3 | 综合与实现 | 利用率与时序报告 |
| 4 | 约束与时序 | XDC 文件与时序收敛说明 |
| 5 | IP Integrator 与 AXI 基础 | 带地址映射的 block design |
| 6 | 片上调试 | ILA/VIO 抓取与调试记录 |
| 7 | 可复用 IP 打包 | 带 README 的已打包 IP 核 |

---

## 单元 1：Vivado 工程流程

### 学习

- project 模式与 non-project 模式
- 源码层次结构与约束的组织
- 生成文件与源文件
- 可复现构建
- 板卡文件与器件选型
- 用于构建的 Tcl 自动化

### 构建

创建一个最小仓库：

```text
rtl/
tb/
constraints/
scripts/
docs/
reports/
```

添加一个 Tcl 脚本，用于创建工程、添加源码、运行综合并导出报告。

### 度量

- 工程能否从干净的 clone 重新构建？
- 生成文件是否已排除在版本控制之外？
- 报告是否写入可预期的路径？

### 交付

一个干净的 Vivado 工程骨架，附 `make` 或脚本驱动的重建说明。

---

## 单元 2：RTL 与仿真

### 学习

- 组合逻辑与时序逻辑
- 复位、时钟使能与寄存器传输结构
- 阻塞赋值与非阻塞赋值
- 模块接口与参数化
- testbench 结构
- 断言与自检测试

### 构建

实现三个模块：

1. 带使能与同步复位的计数器
2. 类 UART 的字节发送器或 SPI 风格的移位器
3. 带 valid/ready 握手的简单流式数据通路

为每个模块编写自检 testbench。

### 度量

- 定向测试的数量
- 有意触发的断言失败次数
- 能解释某个 bug 的波形抓取

### 交付

RTL、testbench、仿真命令，以及一份简短的调试记录。

---

## 单元 3：综合与实现

### 学习

- 综合与实现
- LUT、触发器、BRAM、DSP slice 与布线
- 推断硬件与例化硬件
- 资源共享与重定时
- 告警分级处理
- 比特流生成

### 构建

对单元 2 的流式数据通路做综合与实现。

生成：

- 利用率报告
- 时序摘要
- 功耗估算
- 如有用，原理图或网表截图

### 度量

- LUT/FF/BRAM/DSP 用量
- 关键路径
- 最差负裕量
- 达到的时钟频率


<details>
<summary>English original</summary>

**1. Xilinx FPGA Development**

<div class="course-identity fpga-track" markdown="1">
<div class="course-identity__icon">FPGA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track A1 · Xilinx FPGA Development</p>
<p class="course-identity__title">Build the FPGA loop: RTL, simulation, synthesis, implementation, timing, bitstream, board debug.</p>
<p class="course-identity__meta">Artifact: board-validated RTL project · Measure: timing, utilization, debug captures</p>
</div>
</div>


> Learn the Xilinx FPGA flow by building, simulating, timing, debugging, and documenting real RTL projects.

**Layer mapping:** L5-L6. This module connects RTL design, FPGA implementation, timing closure, on-chip debug, and hardware validation.

**Role targets:** FPGA Engineer · RTL Design Engineer · Hardware Acceleration Engineer · AI Accelerator Prototyping Engineer

**Prerequisites:** [Digital Design and HDL](../../阶段1-基础知识/01-数字设计与HDL/Guide.md), [Computer Architecture](../../阶段1-基础知识/02-计算机体系结构与硬件/Guide.md), and basic command-line Git.

**What comes after:** [Zynq UltraScale+ MPSoC](../02-Zynq-UltraScale-MPSoC/Guide.md), [Advanced FPGA Design](../03-高级FPGA设计/Guide.md), and [High-Level Synthesis](../04-高层次综合HLS/Guide.md).

---

**Why This Module Exists**

Vivado is not the skill. The skill is turning a hardware idea into a verified bitstream that works on a board and meets timing.

This module teaches the full FPGA loop:

```text
RTL -> simulation -> synthesis -> implementation -> timing -> bitstream -> board debug -> report
```

Do not treat the tool as a button-clicking IDE. Treat it as an engineering flow that produces artifacts another hardware engineer can review.

---

**Course Outcomes**

By the end, you should be able to:

- create a clean Vivado project and keep it under version control
- write synthesizable Verilog/SystemVerilog or VHDL for small modules
- build self-checking testbenches
- read synthesis, utilization, timing, and power reports
- constrain clocks and basic I/O correctly
- debug a design in simulation and on hardware
- package a reusable IP block with documentation
- explain what changed between RTL simulation and implemented hardware

---

**Unit Map**

| Unit | Focus | Artifact |
|------|-------|----------|
| 1 | Vivado project flow | reproducible project skeleton |
| 2 | RTL and simulation | self-checking testbench and waveform capture |
| 3 | Synthesis and implementation | utilization and timing report |
| 4 | Constraints and timing | XDC file and timing-closure note |
| 5 | IP Integrator and AXI basics | block design with address map |
| 6 | On-chip debug | ILA/VIO capture and debug write-up |
| 7 | Reusable IP packaging | packaged IP core with README |

---

**Unit 1: Vivado Project Flow**

**Learn**

- project mode versus non-project mode
- source hierarchy and constraints organization
- generated files versus source files
- reproducible builds
- board files and part selection
- Tcl automation for builds

**Build It**

Create a minimal repository:

```text
rtl/
tb/
constraints/
scripts/
docs/
reports/
```

Add a Tcl script that can create the project, add sources, run synthesis, and export reports.

**Measure It**

- Can the project be rebuilt from a clean clone?
- Are generated files excluded from version control?
- Are reports written to a predictable path?

**Ship It**

A clean Vivado project skeleton with `make` or script-driven rebuild instructions.

---

**Unit 2: RTL And Simulation**

**Learn**

- combinational versus sequential logic
- resets, clock enables, and register-transfer structure
- blocking versus non-blocking assignments
- module interfaces and parameterization
- testbench structure
- assertions and self-checking tests

**Build It**

Implement three modules:

1. counter with enable and synchronous reset
2. UART-like byte transmitter or SPI-style shifter
3. small streaming datapath with valid/ready handshake

For each module, write a self-checking testbench.

**Measure It**

- number of directed tests
- assertion failures caught intentionally
- waveform capture that explains one bug

**Ship It**

RTL, testbenches, simulation commands, and one short debug note.

---

**Unit 3: Synthesis And Implementation**

**Learn**

- synthesis versus implementation
- LUTs, flip-flops, BRAM, DSP slices, and routing
- inferred versus instantiated hardware
- resource sharing and retiming
- warning triage
- bitstream generation

**Build It**

Synthesize and implement the streaming datapath from Unit 2.

Generate:

- utilization report
- timing summary
- power estimate
- schematic or netlist screenshot if useful

**Measure It**

- LUT/FF/BRAM/DSP usage
- critical path
- worst negative slack
- achieved clock frequency

</details>

### 交付

一份实现报告，说明 RTL 最终变成了什么硬件。

---

## 第 4 单元：约束与时序

### 学习

- clock 约束
- 输入与输出延迟
- 生成时钟
- 伪路径与多周期路径
- setup、hold、slack 与关键路径解读
- 时序约束什么时候会掩盖 bug，而不是修复 bug

### 构建

为以下内容添加约束：

- 主时钟
- 复位路径策略
- 基本 I/O 时序
- 一个刻意过激的时钟目标

然后通过修改设计来收敛时序，而不只是改约束。

### 度量

- 优化前/后的最差负裕量
- 优化前/后的关键路径
- 修复方案的资源开销

### 交付

一份 XDC 文件，外加一份时序收敛说明，解释瓶颈与实际的硬件修复。

---

## 第 5 单元：IP Integrator 与 AXI 基础

### 学习

- IP 目录
- block design 结构
- AXI4-Lite、AXI4-Stream 与 AXI 内存映射接口的对比
- 地址映射
- 复位与时钟模块
- 将自定义 RTL 打包以供 block design 使用

### 构建

创建一个 block design，包含：

- 时钟/复位模块
- AXI 互连
- 一个自定义 AXI-Lite 寄存器模块或流式外设
- 一个简单的厂商 IP 模块

### 度量

- 地址映射正确性
- 寄存器读/写测试
- 集成后的时序与利用率

### 交付

block design 图、地址映射，以及证明自定义模块响应正确的软件或 testbench。

---

## 第 6 单元：片上调试

### 学习

- 仿真调试与硬件调试的对比
- Integrated Logic Analyzer（ILA）
- Virtual I/O（VIO）
- 触发条件
- 调试核及其时序/资源开销
- 如何避免「靠祈祷来调试」

### 构建

将 ILA 探针插入流式数据通路或 AXI 模块。

捕获：

- 复位释放
- 首次事务
- 一个错误或边界情况
- 一次吞吐测量（如适用）

### 度量

- 调试核的资源开销
- 捕获的周期时序
- 预期硬件行为与观察到的硬件行为之间的差异

### 交付

ILA 截图或导出的捕获数据，外加一份调试记录。

---

## 第 7 单元：可复用 IP 打包

### 学习

- 参数化 RTL
- 接口文档
- IP 打包工具
- 版本管理与元数据
- 示例设计
- 验证配套材料

### 构建

将本模块中的一个模块打包为可复用 IP。

包含：

- 参数
- 时钟/复位假设
- 接口时序
- testbench
- 示例例化
- 综合/时序报告

### 度量

- 在新项目中的集成时间
- 打包过程中产生的警告
- 目标板上的资源与时序数据

### 交付

一个可复用 IP 文件夹，另一位工程师无需阅读整个源码树即可例化。

---

## 综合项目

构建一个经板级验证的小型 FPGA 子系统：

- 自定义 RTL 数据通路
- 仿真 testbench
- Vivado 工程脚本
- XDC 约束
- 实现报告
- ILA 调试捕获
- 板级演示
- 含重建与验证步骤的 README

优秀的综合项目示例：

- AXI-Lite 控制的 PWM 或 GPIO 外设
- 带 FIFO 的 SPI 传感器读取器
- 流式图像滤波器
- UART 数据包解析器
- 定点矩阵-向量模块

当另一个人能够重建 bitstream、理解时序报告，并复现板级行为时，综合项目即告完成。

---

## 达成标准

当你能做到以下各项时，就准备好进入下一个 FPGA 模块：

- 从源码构建 Vivado 工程
- 编写并仿真小型 RTL 模块
- 解读时序与利用率报告
- 同时调试仿真行为与硬件行为
- 在不掩盖真实时序问题的前提下约束设计
- 打包一个小型可复用 IP 模块
- 解释一个可工作 bitstream 背后的工程证据


<details>
<summary>English original</summary>

**Ship It**

An implementation report that explains what hardware the RTL became.

---

**Unit 4: Constraints And Timing**

**Learn**

- clock constraints
- input and output delays
- generated clocks
- false paths and multicycle paths
- setup, hold, slack, and critical path interpretation
- when timing constraints hide bugs instead of fixing them

**Build It**

Add constraints for:

- primary clock
- reset path policy
- basic I/O timing
- one intentionally over-aggressive clock target

Then close timing by changing the design, not only the constraints.

**Measure It**

- before/after worst negative slack
- critical path before/after optimization
- resource cost of the fix

**Ship It**

An XDC file plus a timing-closure note explaining the bottleneck and the actual hardware fix.

---

**Unit 5: IP Integrator And AXI Basics**

**Learn**

- IP catalog
- block design structure
- AXI4-Lite versus AXI4-Stream versus AXI memory-mapped interfaces
- address maps
- reset and clocking blocks
- packaging custom RTL for block design use

**Build It**

Create a block design with:

- clock/reset block
- AXI interconnect
- one custom AXI-Lite register block or streaming peripheral
- one simple vendor IP block

**Measure It**

- address map correctness
- register read/write test
- timing and utilization after integration

**Ship It**

Block design diagram, address map, and software or testbench proof that the custom block responds correctly.

---

**Unit 6: On-Chip Debug**

**Learn**

- simulation debug versus hardware debug
- Integrated Logic Analyzer (ILA)
- Virtual I/O (VIO)
- trigger conditions
- debug cores and timing/resource cost
- how to avoid "debugging by hoping"

**Build It**

Insert ILA probes into the streaming datapath or AXI block.

Capture:

- reset release
- first transaction
- one error or corner case
- one throughput measurement if applicable

**Measure It**

- debug core resource overhead
- captured cycle timing
- difference between expected and observed hardware behavior

**Ship It**

ILA screenshots or exported captures plus a debug write-up.

---

**Unit 7: Reusable IP Packaging**

**Learn**

- parameterized RTL
- interface documentation
- IP packager
- versioning and metadata
- example designs
- verification collateral

**Build It**

Package one block from this module as reusable IP.

Include:

- parameters
- clock/reset assumptions
- interface timing
- testbench
- example instantiation
- synthesis/timing reports

**Measure It**

- integration time in a new project
- warnings generated during packaging
- resource and timing numbers on the target board

**Ship It**

A reusable IP folder that another engineer can instantiate without reading the whole source tree.

---

**Capstone**

Build a small board-validated FPGA subsystem:

- custom RTL datapath
- simulation testbench
- Vivado project script
- XDC constraints
- implementation reports
- ILA debug capture
- board demo
- README with rebuild and validation steps

Good capstone examples:

- AXI-Lite controlled PWM or GPIO peripheral
- SPI sensor reader with FIFO
- streaming image filter
- UART packet parser
- fixed-point matrix-vector block

The capstone is complete when someone else can rebuild the bitstream, understand the timing report, and reproduce the board-level behavior.

---

**Exit Criteria**

You are ready for the next FPGA modules when you can:

- build a Vivado project from source
- write and simulate small RTL blocks
- interpret timing and utilization reports
- debug both simulation and hardware behavior
- constrain a design without hiding real timing problems
- package a small reusable IP block
- explain the engineering evidence behind a working bitstream

</details>

---

> 原文：[`Phase 4 - Track A - Xilinx FPGA/1. Xilinx FPGA Development/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20A%20-%20Xilinx%20FPGA/1.%20Xilinx%20FPGA%20Development/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
