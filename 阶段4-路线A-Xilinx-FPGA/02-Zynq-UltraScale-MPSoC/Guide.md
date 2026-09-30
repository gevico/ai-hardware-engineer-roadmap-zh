---
title: 2. Zynq UltraScale+ MPSoC
description: 2. Zynq UltraScale+ MPSoC
---

# 2. Zynq UltraScale+ MPSoC

<div class="course-identity zynq-mpsoc" markdown="1">
<div class="course-identity__icon">ZYNQ</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 A2 · Zynq UltraScale+ MPSoC</p>
<p class="course-identity__title">在 ARM 软件、可编程逻辑、AXI、DMA、中断和 Linux 之间划分工作。</p>
<p class="course-identity__meta">产物：PS/PL 加速器 demo · 度量：DMA 吞吐、延迟、CPU 开销</p>
</div>
</div>


> 构建在 ARM 处理核心、可编程逻辑、内存、DMA、中断和嵌入式 Linux 之间干净地划分工作的系统。

**层次映射：** L3-L6。本模块连接处理系统软件、可编程逻辑、AXI 互连、DMA、启动流程、设备树、Linux 驱动，以及硬件/软件协同设计。

**目标岗位：** FPGA 系统工程师 · 嵌入式 Linux 工程师 · BSP 工程师 · 硬件/软件协同设计工程师 · 边缘加速工程师

**前置要求：** [Xilinx FPGA 开发](../01-Xilinx-FPGA开发/Guide.md)、[嵌入式 Linux](../../阶段2-嵌入式系统/03-嵌入式Linux/Guide.md)，以及基础 C/C++。

**后续内容：** [高级 FPGA 设计](../03-高级FPGA设计/Guide.md)、[高层次综合](../04-高层次综合HLS/Guide.md)，以及 [runtime 与驱动开发](../05-运行时与驱动开发/Guide.md)。

---

## 本模块为何存在

Zynq 不只是挂了个处理器的 FPGA。它是一个异构系统，软件和硬件共享内存、中断、时钟和失效模式。

核心设计问题是：

```text
What runs on the processing system, what runs in programmable logic, and how do they exchange data safely and fast enough?
```

本模块教授这条边界。

---

## 课程成果

学完后，你应当能够：

- 解释 Zynq UltraScale+ 处理系统与可编程逻辑的边界
- 设计一个 AXI 连接的 PL 外设
- 通过 AXI DMA 搬运数据
- 通过设备树和 userspace 或内核驱动把 PL 硬件暴露给 Linux
- 分析缓存一致性、中断和内存映射
- 为 Zynq 板卡构建并启动定制 Linux 镜像
- 用测量数据记录 PS/PL 性能瓶颈

---

## 单元总览

| 单元 | 重点 | 产物 |
|------|-------|----------|
| 1 | MPSoC 架构 | PS/PL 架构图 |
| 2 | AXI 与内存映射 | 带地址映射的 block design |
| 3 | DMA 与流 | 实测的 PS/PL 传输路径 |
| 4 | 启动与 Linux | 可启动镜像和设备树补丁 |
| 5 | 驱动接口 | userspace 或内核访问路径 |
| 6 | 协同设计优化 | 瓶颈报告和调优后的设计 |

---

## 单元 1：MPSoC 架构

### 学习

- Cortex-A 应用核心
- Cortex-R 实时核心
- 可编程逻辑资源
- 片上内存、DDR 和缓存层次结构
- AXI 高性能端口和一致性端口
- 中断路由
- 时钟和复位域
- 高层次的启动链

### 构建

创建一份板卡专属的架构说明：

- 处理器核心
- 内存区域
- PL 资源
- 可用接口
- 启动介质
- 调试端口
- 目标工作负载

### 度量

- DDR 带宽基线
- CPU 内存拷贝带宽
- PL 时钟目标
- 板卡启动时间

### 交付

一份说明计算、内存和控制位于何处的 Zynq 系统图。

---

## 单元 2：AXI 与内存映射

### 学习

- 用 AXI4-Lite 做控制寄存器
- 用于批量访问的 AXI4 内存映射接口
- 用于数据通路的 AXI4-Stream
- 地址分配
- 寄存器映射
- 时钟/复位跨域问题
- AXI 协议调试

### 构建

创建一个 block design，包含：

- PS block
- AXI 互连
- 自定义 AXI-Lite 控制外设
- 可选的 AXI-Stream 数据通路 block

把控制寄存器暴露给软件。

### 度量

- 寄存器读/写延迟
- 地址映射正确性
- 利用率和时序
- 若可观测，非法访问的错误行为

### 交付

block design 图、地址映射、寄存器定义，以及寄存器访问的软件证明。

---

## 单元 3：DMA 与流

### 学习

- AXI DMA
- scatter-gather 与 simple 模式对比
- 缓存一致性和 buffer 刷新
- 物理连续内存
- 流式背压
- 吞吐与延迟对比

### 构建

实现一个 PS -> DMA -> PL -> DMA -> PS 环路。

从一个直通的 stream block 开始，然后用一个简单变换替换它：

- 加常量
- 阈值
- 定点乘法
- 校验和

### 度量

- 传输延迟
- 持续吞吐
- CPU 开销
- buffer 大小的影响
- 缓存维护的影响

### 交付

一份带原始结果的 DMA benchmark，以及对瓶颈的简短说明。

---

## 单元 4：启动与嵌入式 Linux

### 学习

- boot ROM、FSBL、PMU 固件、U-Boot、内核、设备树、rootfs
- PetaLinux 或基于 Yocto 的镜像流程
- PL 外设的设备树节点
- 内核配置
- systemd 服务设置
- 启动日志和失效排查


<details>
<summary>English original</summary>

**2. Zynq UltraScale+ MPSoC**

<div class="course-identity zynq-mpsoc" markdown="1">
<div class="course-identity__icon">ZYNQ</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track A2 · Zynq UltraScale+ MPSoC</p>
<p class="course-identity__title">Split work between ARM software, programmable logic, AXI, DMA, interrupts, and Linux.</p>
<p class="course-identity__meta">Artifact: PS/PL accelerator demo · Measure: DMA throughput, latency, CPU overhead</p>
</div>
</div>


> Build systems that split work cleanly between ARM processing cores, programmable logic, memory, DMA, interrupts, and embedded Linux.

**Layer mapping:** L3-L6. This module connects processing system software, programmable logic, AXI interconnects, DMA, boot flow, device tree, Linux drivers, and hardware/software co-design.

**Role targets:** FPGA Systems Engineer · Embedded Linux Engineer · BSP Engineer · Hardware/Software Co-design Engineer · Edge Acceleration Engineer

**Prerequisites:** [Xilinx FPGA Development](../01-Xilinx-FPGA开发/Guide.md), [Embedded Linux](../../阶段2-嵌入式系统/03-嵌入式Linux/Guide.md), and basic C/C++.

**What comes after:** [Advanced FPGA Design](../03-高级FPGA设计/Guide.md), [High-Level Synthesis](../04-高层次综合HLS/Guide.md), and [Runtime and Driver Development](../05-运行时与驱动开发/Guide.md).

---

**Why This Module Exists**

Zynq is not just an FPGA with a processor attached. It is a heterogeneous system where software and hardware share memory, interrupts, clocks, and failure modes.

The core design question is:

```text
What runs on the processing system, what runs in programmable logic, and how do they exchange data safely and fast enough?
```

This module teaches that boundary.

---

**Course Outcomes**

By the end, you should be able to:

- explain the Zynq UltraScale+ processing system and programmable logic boundary
- design an AXI-connected PL peripheral
- move data through AXI DMA
- expose PL hardware to Linux through device tree and userspace or kernel drivers
- reason about cache coherency, interrupts, and memory mapping
- build and boot a custom Linux image for a Zynq board
- document a PS/PL performance bottleneck with measurements

---

**Unit Map**

| Unit | Focus | Artifact |
|------|-------|----------|
| 1 | MPSoC architecture | PS/PL architecture map |
| 2 | AXI and memory maps | block design with address map |
| 3 | DMA and streaming | measured PS/PL transfer path |
| 4 | Boot and Linux | bootable image and device tree patch |
| 5 | Driver interface | userspace or kernel access path |
| 6 | Co-design optimization | bottleneck report and tuned design |

---

**Unit 1: MPSoC Architecture**

**Learn**

- Cortex-A application cores
- Cortex-R real-time cores
- programmable logic resources
- on-chip memory, DDR, and cache hierarchy
- AXI high-performance and coherent ports
- interrupt routing
- clock and reset domains
- boot chain at a high level

**Build It**

Create a board-specific architecture note:

- processor cores
- memory regions
- PL resources
- available interfaces
- boot media
- debug ports
- target workloads

**Measure It**

- DDR bandwidth baseline
- CPU memory-copy bandwidth
- PL clock targets
- board boot time

**Ship It**

A Zynq system map that explains where compute, memory, and control live.

---

**Unit 2: AXI And Memory Maps**

**Learn**

- AXI4-Lite for control registers
- AXI4 memory-mapped interfaces for bulk access
- AXI4-Stream for datapaths
- address assignment
- register maps
- clock/reset crossing concerns
- AXI protocol debugging

**Build It**

Create a block design with:

- PS block
- AXI interconnect
- custom AXI-Lite control peripheral
- optional AXI-Stream datapath block

Expose control registers to software.

**Measure It**

- register read/write latency
- address map correctness
- utilization and timing
- error behavior for invalid access if observable

**Ship It**

Block design diagram, address map, register definition, and software proof of register access.

---

**Unit 3: DMA And Streaming**

**Learn**

- AXI DMA
- scatter-gather versus simple mode
- cache coherency and buffer flushing
- physically contiguous memory
- streaming backpressure
- throughput versus latency

**Build It**

Implement a PS -> DMA -> PL -> DMA -> PS loop.

Start with a pass-through stream block. Then replace it with a simple transform:

- add constant
- threshold
- fixed-point multiply
- checksum

**Measure It**

- transfer latency
- sustained throughput
- CPU overhead
- effect of buffer size
- effect of cache maintenance

**Ship It**

A DMA benchmark with raw results and a short explanation of the bottleneck.

---

**Unit 4: Boot And Embedded Linux**

**Learn**

- boot ROM, FSBL, PMU firmware, U-Boot, kernel, device tree, rootfs
- PetaLinux or Yocto-based image flow
- device tree nodes for PL peripherals
- kernel configuration
- systemd service setup
- boot logs and failure triage

</details>

### Build It

构建并启动一个自定义 Linux 镜像，其中包含：

- 你的 PL 外设的设备树条目
- 用户空间测试程序
- 启动服务或测试脚本
- 捕获的启动日志

### Measure It

- 构建可复现性
- 启动时间
- kernel 日志整洁度
- 外设探测成功

### Ship It

镜像构建说明、启动日志、设备树补丁和验证命令。

---

## Unit 5: Driver Interface

### Learn

- `/dev/mem` 与 UIO 的取舍
- 平台驱动
- 字符设备
- 中断处理
- DMA 缓冲区
- 同步与错误处理
- 何时可以接受用户空间访问，何时需要 kernel 驱动

### Build It

通过以下方式之一暴露你的 PL 外设：

- 针对简单控制块的用户空间内存映射访问
- UIO 驱动
- 最小平台驱动
- 带中断通知的字符设备

### Measure It

- 控制路径延迟
- 中断延迟
- CPU 利用率
- 错误输入或硬件缺失时的失败行为

### Ship It

驱动或用户空间访问路径，含测试、日志和已记录的局限。

---

## Unit 6: Hardware/Software Co-design Optimization

### Learn

- 任务划分
- CPU 与 PL 的成本模型
- 数据搬运是首要瓶颈
- 批处理与缓冲
- 定点与浮点的取舍
- 实时约束
- 同时性能剖析 PS 和 PL

### Build It

选择一种工作负载：

- 传感器预处理
- 图像滤波器
- 音频 DSP 模块
- 数据包解析器
- 矩阵-向量运算

实现一个 CPU 基线和一条 PL 加速路径。

### Measure It

- 延迟
- 吞吐
- CPU 利用率
- PL 资源利用率
- 功耗（如有）
- 包含传输开销的端到端加速比

### Ship It

一份协同设计报告，说明在计入数据搬运和软件开销后，加速是否值得。

---

## Capstone

构建一个 Zynq PS/PL 加速器 demo：

- 定制 PL 模块
- AXI 控制路径
- DMA 数据路径
- Linux 镜像或启动说明
- 设备树补丁
- 驱动或用户空间接口
- benchmark 脚本
- 包含时序、利用率和吞吐的报告

当硬件/软件边界清晰、可测量且可复现时，capstone 即告完成。

---

## Exit Criteria

当你能做到以下各项时，就可以继续推进：

- 解释针对某个工作负载的 PS/PL 划分
- 构建并验证一个 AXI 连接的外设
- 通过 DMA 搬运数据并解释缓存一致性问题
- 用包含 PL 硬件设备树条目的配置启动 Linux
- 通过站得住脚的接口将 PL 模块暴露给软件
- 测量端到端加速，而不仅仅是 kernel 速度


<details>
<summary>English original</summary>

**Build It**

Build and boot a custom Linux image that includes:

- device tree entry for your PL peripheral
- userspace test program
- startup service or test script
- captured boot log

**Measure It**

- build reproducibility
- boot time
- kernel log cleanliness
- peripheral probe success

**Ship It**

Image build notes, boot log, device tree patch, and validation commands.

---

**Unit 5: Driver Interface**

**Learn**

- `/dev/mem` and UIO tradeoffs
- platform drivers
- character devices
- interrupt handling
- DMA buffers
- synchronization and error handling
- when userspace access is acceptable and when a kernel driver is required

**Build It**

Expose your PL peripheral through one of:

- userspace memory-mapped access for a simple control block
- UIO driver
- minimal platform driver
- character device with interrupt notification

**Measure It**

- control path latency
- interrupt latency
- CPU utilization
- failure behavior on bad input or missing hardware

**Ship It**

Driver or userspace access path with tests, logs, and documented limitations.

---

**Unit 6: Hardware/Software Co-design Optimization**

**Learn**

- task partitioning
- CPU versus PL cost model
- memory movement as the first bottleneck
- batching and buffering
- fixed-point versus floating-point tradeoffs
- real-time constraints
- profiling PS and PL together

**Build It**

Choose one workload:

- sensor preprocessing
- image filter
- audio DSP block
- packet parser
- matrix-vector operation

Implement a CPU baseline and a PL-accelerated path.

**Measure It**

- latency
- throughput
- CPU utilization
- PL resource utilization
- power if available
- end-to-end speedup including transfer overhead

**Ship It**

A co-design report that explains whether acceleration was worth it after data movement and software overhead.

---

**Capstone**

Build a Zynq PS/PL accelerator demo:

- custom PL block
- AXI control path
- DMA data path
- Linux image or boot notes
- device tree patch
- driver or userspace interface
- benchmark script
- report with timing, utilization, and throughput

The capstone is complete when the hardware/software boundary is clear, measured, and reproducible.

---

**Exit Criteria**

You are ready to move on when you can:

- explain PS/PL partitioning for a workload
- build and validate an AXI-connected peripheral
- move data through DMA and explain cache coherency issues
- boot Linux with a device tree entry for PL hardware
- expose a PL block to software through a defensible interface
- measure end-to-end acceleration rather than only kernel speed

</details>

---

> 原文：[`Phase 4 - Track A - Xilinx FPGA/2. Zynq UltraScale+ MPSoC/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20A%20-%20Xilinx%20FPGA/2.%20Zynq%20UltraScale%2B%20MPSoC/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
