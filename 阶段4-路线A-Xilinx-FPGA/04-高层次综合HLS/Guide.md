---
title: 4. 高层综合（HLS）
description: 4. 高层综合（HLS）
---

# 4. 高层综合（HLS）

<div class="course-identity hls" markdown="1">
<div class="course-identity__icon">HLS</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 A4 · 高层综合</p>
<p class="course-identity__title">使用 C/C++ 生成 FPGA 硬件，并验证生成的 RTL 是否值得交付。</p>
<p class="course-identity__meta">产物：HLS 加速器 + Pareto 报告 · 测量：II、延迟、面积、数据搬运</p>
</div>
</div>

> 使用 C/C++ 生成 FPGA 硬件，然后验证生成的 RTL 是否真正满足吞吐、面积、内存和接口要求。

**层级映射：** L2、L5 和 L6。HLS 连接算法代码、编译器调度、内存架构、RTL 生成和 FPGA 实现。

**目标角色：** HLS 工程师 · FPGA 加速工程师 · ML 编译器/硬件协同设计工程师 · AI 加速器原型工程师

**前置要求：** [Xilinx FPGA 开发](../01-Xilinx-FPGA开发/Guide.md)、[高级 FPGA 设计](../03-高级FPGA设计/Guide.md)、C/C++，以及基本性能剖析。

**后续内容：** [Runtime 与驱动开发](../05-运行时与驱动开发/Guide.md)、[ML 编译器与图优化](../../阶段4-路线C-深度学习推理优化/Guide.md)，以及 [AI 芯片设计](../../阶段5-高级专题与专精/06-方向F-AI芯片设计/Guide.md)。

---

## 本模块存在的意义

HLS 在缩短从算法到硬件的路径时很有用。当它隐藏了内存、循环、接口和调度的硬件成本时，则很危险。

HLS 的问题始终是：

```text
What hardware did this C/C++ imply, and is that hardware better than the CPU/GPU/RTL alternative?
```


本模块将 HLS 作为编译器和硬件设计学科来教授，而非绕过 RTL 理解的捷径。

---

## 课程成果

学完后，你应当能够：

- 编写可综合的 C/C++ 用于 HLS，而不会意外引发硬件爆炸
- 阅读 HLS 调度、延迟、启动间隔和资源报告
- 有意使用流水线、展开、数组分区和 dataflow 指令
- 设计 AXI-Lite、AXI 内存映射和 AXI-Stream 接口
- 通过 C 仿真、联合仿真和 RTL 集成验证 HLS 模块
- 将 HLS 生成的硬件与 CPU、GPU 和手写 RTL 基线进行比较
- 用数字记录设计空间的取舍

---

## 单元地图

| 单元 | 重点 | 产物 |
|------|-------|----------|
| 1 | HLS 流程与报告 | 综合基线 kernel |
| 2 | 循环与流水线 | 启动间隔实验 |
| 3 | 内存架构 | 数组分区/分 bank 报告 |
| 4 | Dataflow 设计 | 带背压的流式流水线 |
| 5 | 接口 | AXI 连接的 HLS IP 模块 |
| 6 | 验证 | C 仿真、联合仿真和 RTL 验证包 |
| 7 | 设计空间探索 | 延迟/面积/功耗的 Pareto 表 |

---

## 单元 1：HLS 流程与报告

### 学习

- C/C++ 到 RTL 的转换
- 综合、C 仿真、联合仿真、导出
- 延迟、启动间隔、tripcount 和资源估计
- 为什么估计值可能与实现结果不同
- 定点与浮点的含义

### 构建

实现三个基线 kernel：

1. 向量加法
2. FIR 滤波器或类卷积 stencil
3. 矩阵-向量乘法

首先在不使用激进指令的情况下综合每个 kernel。

### 测量

- 延迟
- 启动间隔
- LUT/FF/BRAM/DSP 估计
- 达到的时钟目标
- 如果导出，HLS 估计与实现之间的差异

### 交付

一份基线 HLS 报告，解释代码隐含了什么硬件。

---

## 单元 2：循环与流水线

### 学习

- 循环流水线
- 循环展开
- 循环携带依赖
- 启动间隔限制
- 资源复用
- 延迟与吞吐的取舍

### 构建

取矩阵-向量或 FIR kernel，运行以下变体：

- 无流水线
- 流水线循环
- 展开循环
- 流水线 + 展开

### 测量

- 启动间隔
- 延迟
- 吞吐
- DSP 使用量
- BRAM 压力
- 实现后的时序

### 交付

一张表，显示哪个指令提高了吞吐及其代价。

---

## 单元 3：内存架构

### 学习

- 数组分区
- 数组重塑
- 内存分 bank
- BRAM、URAM 与寄存器
- 突发访问
- 数据复用
- 内存带宽作为 HLS 的常见瓶颈

### 构建

优化一个带宽受限的 kernel：

- 矩阵-向量乘法
- 小卷积
- 直方图
- 特征提取器

使用分区或分 bank 为并行计算提供数据。

### 测量

- 所需的读/写端口
- BRAM/URAM 使用量
- 达到的 II
- 突发效率
- 内存更改前后的吞吐

### 交付

一份内存架构报告，包含数据搬运和存储的示意图。

---

## 单元 4：Dataflow 设计


<details>
<summary>English original</summary>

**4. High-Level Synthesis (HLS)**

<div class="course-identity hls" markdown="1">
<div class="course-identity__icon">HLS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track A4 · High-Level Synthesis</p>
<p class="course-identity__title">Use C/C++ to generate FPGA hardware and prove whether the generated RTL is worth shipping.</p>
<p class="course-identity__meta">Artifact: HLS accelerator + Pareto report · Measure: II, latency, area, data movement</p>
</div>
</div>


> Use C/C++ to generate FPGA hardware, then verify whether the generated RTL actually meets throughput, area, memory, and interface requirements.

**Layer mapping:** L2, L5, and L6. HLS connects algorithm code, compiler scheduling, memory architecture, RTL generation, and FPGA implementation.

**Role targets:** HLS Engineer · FPGA Acceleration Engineer · ML Compiler/Hardware Co-design Engineer · AI Accelerator Prototyping Engineer

**Prerequisites:** [Xilinx FPGA Development](../01-Xilinx-FPGA开发/Guide.md), [Advanced FPGA Design](../03-高级FPGA设计/Guide.md), C/C++, and basic performance profiling.

**What comes after:** [Runtime and Driver Development](../05-运行时与驱动开发/Guide.md), [ML Compiler and Graph Optimization](../../阶段4-路线C-深度学习推理优化/Guide.md), and [AI Chip Design](../../阶段5-高级专题与专精/06-方向F-AI芯片设计/Guide.md).

---

**Why This Module Exists**

HLS is useful when it shortens the path from algorithm to hardware. It is dangerous when it hides the hardware cost of memory, loops, interfaces, and scheduling.

The HLS question is always:

```text
What hardware did this C/C++ imply, and is that hardware better than the CPU/GPU/RTL alternative?
```

This module teaches HLS as a compiler and hardware-design discipline, not as a shortcut around RTL understanding.

---

**Course Outcomes**

By the end, you should be able to:

- write synthesizable C/C++ for HLS without accidental hardware explosions
- read HLS scheduling, latency, initiation interval, and resource reports
- use pipelining, unrolling, array partitioning, and dataflow directives intentionally
- design AXI-Lite, AXI memory-mapped, and AXI-Stream interfaces
- verify HLS blocks with C simulation, co-simulation, and RTL integration
- compare HLS-generated hardware against CPU, GPU, and hand-written RTL baselines
- document design-space tradeoffs with numbers

---

**Unit Map**

| Unit | Focus | Artifact |
|------|-------|----------|
| 1 | HLS flow and reports | synthesized baseline kernel |
| 2 | Loops and pipelining | initiation-interval experiment |
| 3 | Memory architecture | array partitioning/banking report |
| 4 | Dataflow design | streaming pipeline with backpressure |
| 5 | Interfaces | AXI-connected HLS IP block |
| 6 | Verification | C sim, co-sim, and RTL validation package |
| 7 | Design-space exploration | Pareto table for latency/area/power |

---

**Unit 1: HLS Flow And Reports**

**Learn**

- C/C++ to RTL transformation
- synthesis, C simulation, co-simulation, export
- latency, initiation interval, tripcount, and resource estimates
- why estimates can differ from implemented results
- fixed-point versus floating-point implications

**Build It**

Implement three baseline kernels:

1. vector add
2. FIR filter or convolution-like stencil
3. matrix-vector multiply

Synthesize each without aggressive directives first.

**Measure It**

- latency
- initiation interval
- LUT/FF/BRAM/DSP estimate
- achieved clock target
- difference between HLS estimate and implementation if exported

**Ship It**

A baseline HLS report explaining what hardware the code implied.

---

**Unit 2: Loops And Pipelining**

**Learn**

- loop pipelining
- loop unrolling
- loop-carried dependencies
- initiation interval limits
- resource sharing
- latency versus throughput

**Build It**

Take the matrix-vector or FIR kernel and run variants:

- no pipeline
- pipelined loop
- unrolled loop
- pipelined + unrolled

**Measure It**

- initiation interval
- latency
- throughput
- DSP usage
- BRAM pressure
- timing after implementation

**Ship It**

A table showing which directive improved throughput and what it cost.

---

**Unit 3: Memory Architecture**

**Learn**

- array partitioning
- array reshaping
- memory banking
- BRAM versus URAM versus registers
- burst access
- data reuse
- memory bandwidth as the common HLS bottleneck

**Build It**

Optimize a memory-bound kernel:

- matrix-vector multiply
- small convolution
- histogram
- feature extractor

Use partitioning or banking to feed parallel compute.

**Measure It**

- read/write ports required
- BRAM/URAM usage
- achieved II
- burst efficiency
- throughput before/after memory changes

**Ship It**

A memory architecture report with diagrams for data movement and storage.

---

**Unit 4: Dataflow Design**

</details>

### 学习

- task 级并行
- `dataflow` regions
- FIFO 与流
- 生产者/消费者平衡
- 背压
- 死锁风险
- 流水线填充与排空行为

### 构建

创建一个三级流式流水线：

```text
load -> transform -> store
```

然后把 transform 拆成两级，并加入 FIFO 深度实验。

### 测量

- 级延迟
- 端到端吞吐
- FIFO 深度敏感性
- 停顿行为
- 资源开销

### 交付

一个流式流水线 benchmark，并附一条背压或死锁调试笔记。

---

## Unit 5：接口

### 学习

- AXI4-Lite 控制接口
- AXI 内存映射 master 接口
- AXI4-Stream 接口
- 寄存器映射
- DMA 集成
- 主机软件控制路径
- 接口时序与协议约束

### 构建

把一个 HLS kernel 打包为 IP，具备：

- AXI-Lite 控制
- AXI 内存或 stream 数据通路
- 测试应用或驱动
- block design 集成

### 测量

- 主机到 kernel 的建立延迟
- 传输吞吐
- kernel 吞吐
- 包含数据搬运在内的端到端加速

### 交付

已连接 AXI 的 HLS IP，含寄存器映射、集成图与 benchmark。

---

## Unit 6：验证

### 学习

- 黄金参考模型
- C 仿真
- C/RTL 协同仿真
- 测试向量生成
- 定点的数值容差
- 波形检查
- 与 RTL 或软件的集成测试

### 构建

为一个 HLS kernel 构建验证包：

- C 参考
- 随机化测试
- 边界用例测试
- 如相关，则进行定点比较
- 协同仿真运行
- 导出 RTL 集成冒烟测试

### 测量

- 测试数量
- 边界用例覆盖
- 数值误差
- 协同仿真通过/失败日志

### 交付

一份验证报告，能让另一位工程师信任生成的硬件。

---

## Unit 7：设计空间探索

### 学习

- 指令扫描
- 帕累托分析
- 延迟/面积/功耗取舍
- 自动化报告提取
- HLS 不适用的情况

### 构建

跨以下维度运行参数扫描：

- 展开因子
- 流水线目标
- 数据位宽
- 数组划分因子
- FIFO 深度

### 测量

- 延迟
- 吞吐
- LUT/FF/BRAM/DSP 用量
- 时序收敛
- 估算功耗

### 交付

一张帕累托表与建议：你会交付哪个设计点，以及为什么。

---

## 综合项目

为一个真实的 kernel 构建 HLS 加速器：

- 图像滤波
- 音频 DSP 模块
- 矩阵-向量乘
- 量化神经网络原语
- 包处理级

要求提交的证据：

- CPU 基线
- HLS 基线
- 优化后的 HLS 版本
- C 仿真与协同仿真结果
- 实现报告
- AXI 集成
- 端到端 benchmark
- 设计空间表

当报告能证明 HLS 对该工作负载是否为好的实现选择时，综合项目即为完成。

---

## 达成标准

当你能做到以下各项时，就可以继续前进：

- 读懂 HLS 报告并预判实现风险
- 有意识地优化循环与内存
- 构建无死锁的流式流水线
- 把 HLS IP 集成进 AXI 系统
- 针对参考模型验证生成的 RTL
- 在计入数据搬运与控制开销后比较加速比
- 说明何时手写 RTL 或 GPU kernel 更合适


<details>
<summary>English original</summary>

**Learn**

- task-level parallelism
- `dataflow` regions
- FIFOs and streams
- producer/consumer balance
- backpressure
- deadlock risks
- pipeline fill and drain behavior

**Build It**

Create a three-stage streaming pipeline:

```text
load -> transform -> store
```

Then split transform into two stages and add FIFO sizing experiments.

**Measure It**

- stage latency
- end-to-end throughput
- FIFO depth sensitivity
- stall behavior
- resource cost

**Ship It**

A streaming pipeline benchmark with a backpressure or deadlock debugging note.

---

**Unit 5: Interfaces**

**Learn**

- AXI4-Lite control interfaces
- AXI memory-mapped master interfaces
- AXI4-Stream interfaces
- register maps
- DMA integration
- host software control path
- interface timing and protocol constraints

**Build It**

Package one HLS kernel as IP with:

- AXI-Lite control
- AXI memory or stream data path
- test application or driver
- block design integration

**Measure It**

- host-to-kernel setup latency
- transfer throughput
- kernel throughput
- end-to-end acceleration including data movement

**Ship It**

AXI-connected HLS IP with register map, integration diagram, and benchmark.

---

**Unit 6: Verification**

**Learn**

- golden reference model
- C simulation
- C/RTL co-simulation
- test vector generation
- numerical tolerance for fixed point
- waveform inspection
- integration testing with RTL or software

**Build It**

Build a verification package for one HLS kernel:

- C reference
- randomized tests
- edge-case tests
- fixed-point comparison if relevant
- co-simulation run
- exported RTL integration smoke test

**Measure It**

- test count
- coverage of edge cases
- numerical error
- co-simulation pass/fail logs

**Ship It**

A verification report that would let another engineer trust the generated hardware.

---

**Unit 7: Design-Space Exploration**

**Learn**

- directive sweeps
- Pareto analysis
- latency/area/power tradeoffs
- automated report extraction
- when HLS is the wrong tool

**Build It**

Run a parameter sweep across:

- unroll factors
- pipeline targets
- data widths
- array partitioning factors
- FIFO depths

**Measure It**

- latency
- throughput
- LUT/FF/BRAM/DSP usage
- timing closure
- estimated power

**Ship It**

A Pareto table and recommendation: which design point you would ship and why.

---

**Capstone**

Build an HLS accelerator for a realistic kernel:

- image filter
- audio DSP block
- matrix-vector multiply
- quantized neural-network primitive
- packet-processing stage

Required evidence:

- CPU baseline
- HLS baseline
- optimized HLS version
- C sim and co-sim results
- implementation reports
- AXI integration
- end-to-end benchmark
- design-space table

The capstone is complete when the report proves whether HLS was a good implementation choice for the workload.

---

**Exit Criteria**

You are ready to move on when you can:

- read HLS reports and predict implementation risk
- optimize loops and memory intentionally
- build streaming pipelines without deadlock
- integrate HLS IP into an AXI system
- verify generated RTL against a reference model
- compare speedup after including data movement and control overhead
- explain when hand-written RTL or a GPU kernel would be better

</details>

---

> 原文：[`Phase 4 - Track A - Xilinx FPGA/4. High-Level Synthesis (HLS)/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20A%20-%20Xilinx%20FPGA/4.%20High-Level%20Synthesis%20%28HLS%29/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
