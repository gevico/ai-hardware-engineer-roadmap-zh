---
title: 基于 RISC-V 的 AI 芯片设计与市场分析
description: 基于 RISC-V 的 AI 芯片设计与市场分析
---

# 基于 RISC-V 的 AI 芯片设计与市场分析

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">RBAC</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 专业化方向</p>
<p class="course-identity__title">RISC-V Based AI Chip Design and Market Analysis 的专业化课程标识。</p>
<p class="course-identity__meta">产物：专业化案例研究 · 度量：性能、可靠性、角色契合度</p>
</div>

</div>


**父级：** [AI Chip Design](../Guide.md)

> 把 RISC-V 当作 AI 芯片平台来研究：ISA 何时重要、加速器何时更重要、软件使能如何决定采用、以及为什么 SpacemiT 是 2026 年一个有用的案例研究。

---

## 本课程为何存在

讨论 RISC-V 时，常有一种说法，仿佛仅凭开放的 ISA 就能改变 AI 硬件。

这种看法过于简单。

对 AI 芯片而言，ISA 只是其中一层。真正的产品是：

```text
workload -> model graph -> compiler/runtime -> kernels -> memory system -> accelerator fabric -> Linux/driver stack -> board/product
```

当 RISC-V 能帮助一家公司掌控整个栈时，它才变得有意思：

- 定制的向量或矩阵扩展
- 更低的授权依赖
- CPU 与加速器的紧耦合
- 边缘推理的能效
- 确定性的嵌入式控制
- 与开放软件生态对齐
- 国产或主权芯片战略

本课程教你如何从技术和商业两方面评估这些主张。

---

## 学习成果

学完之后，你应当能够：

1. 区分 RISC-V CPU、RISC-V 主机处理器与 RISC-V AI 加速器。
2. 解释主要的 RISC-V AI 芯片设计模式。
3. 以 SpacemiT K1/K3 为实际案例进行研究。
4. 分析为什么软件成熟度比 TOPS 营销更重要。
5. 比较 RISC-V AI 在边缘、机器人、工业、汽车、AI PC 和数据中心中的机会。
6. 为任何一家 RISC-V AI 芯片初创公司构建一份评估检查清单。
7. 为一个 RISC-V AI 平台撰写简短的架构与市场备忘录。

---

## 1. 核心思维模型

RISC-V AI 芯片可能指代好几种不同的东西。

| 设计类型 | RISC-V 承担的角色 | 示例形态 | 主要风险 |
|---|---|---|---|
| 带向量 AI 的 RISC-V CPU | 直接运行标量代码和向量 kernel | 宽 RVV CPU 或 AI CPU | 能效可能不如固定张量引擎 |
| RISC-V CPU 加 NPU | 控制一个独立的神经网络加速器 | CPU + NPU 的边缘 SoC | NPU 软件可能专有或碎片化 |
| 加速器内部的 RISC-V 控制核 | 编排张量/dataflow 阵列 | 内嵌 RISC-V 控制器的 AI 加速器 | 开发者可能根本接触不到 RISC-V |
| 面向 GPU/加速器的 RISC-V 主机 | 在更大的加速器外围替代 Arm/x86 主机 CPU | RISC-V 服务器/边缘主机加 GPU | AI 性能主要取决于外挂的加速器 |
| RISC-V 定制扩展平台 | 增加向量、矩阵、DSP 或领域专用指令 | 定制 ISA 扩展或 CFU | 编译器和生态支持变得困难 |

错误的说法是：

```text
RISC-V chip = AI accelerator
```

更好的问题是：

```text
Where is the actual AI compute, and can software use it efficiently?
```

---

## 2. RISC-V 为何对 AI 芯片有吸引力

RISC-V 赋予芯片团队架构上的自由。

这一点很重要，因为 AI 工作负载变化很快。一家公司可能希望围绕以下方面调优设计：

- INT8 和 INT4 推理
- FP8 Transformer 推理
- BF16 或 FP16 累加
- 稀疏矩阵格式
- 本地内存搬运
- 摄像头与传感器流水线
- 机器人控制回路
- 常开音频
- 实时安全岛

吸引力不只是「免费的 ISA」。

吸引力在于**控制权**：

- 对扩展的控制
- 对核数与存储层次的控制
- 对 NPU 耦合方式的控制
- 对编译器目标的控制
- 对供应链的控制
- 对长生命周期嵌入式支持的控制

这就是为什么 RISC-V 最先在**边界明确的系统**中最有优势——这类系统的产品所有者掌控着整个栈。

---

## 3. RISC-V AI 最先在哪里取胜

近期最强的市场并不是通用 AI 训练集群。

而是功耗、定制和产品集成至关重要的边缘与嵌入式系统。

### 强势市场

| 市场 | RISC-V 为何适配 |
|---|---|
| 工业视觉 | 模型边界明确、本地推理、生命周期长、成本压力大 |
| 机器人 | 推理 + 实时控制 + 定制 I/O |
| AI 边缘网关 | 隐私、本地处理、家电式部署 |
| 汽车控制器 | 安全、确定性、厂商中立架构的压力 |
| AI 传感器与 TinyML | 严格功耗限制下的常开推理 |
| 国产芯片生态 | ISA 开放性与供应链控制 |
| 开发者板与 AI PC | 生态培育与 Linux bring-up（上电点亮/调通） |


<details>
<summary>English original</summary>

**RISC-V Based AI Chip Design and Market Analysis**

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">RBAC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for RISC-V Based AI Chip Design and Market Analysis.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [AI Chip Design](../Guide.md)

> Study RISC-V as an AI-chip platform: when the ISA matters, when the accelerator matters more, how software enablement determines adoption, and why SpacemiT is a useful 2026 case study.

---

**Why This Course Exists**

RISC-V is often discussed as if the open ISA alone will change AI hardware.

That is too simple.

For AI chips, the ISA is only one layer. The real product is:

```text
workload -> model graph -> compiler/runtime -> kernels -> memory system -> accelerator fabric -> Linux/driver stack -> board/product
```

RISC-V becomes interesting when it helps a company control that whole stack:

- custom vector or matrix extensions
- lower licensing dependency
- tight CPU-to-accelerator coupling
- edge inference power efficiency
- deterministic embedded control
- open software ecosystem alignment
- domestic or sovereign silicon strategy

This course teaches how to evaluate those claims technically and commercially.

---

**Learning Outcomes**

By the end, you should be able to:

1. Distinguish a RISC-V CPU, a RISC-V host processor, and a RISC-V AI accelerator.
2. Explain the main RISC-V AI chip design patterns.
3. Review SpacemiT K1/K3 as a practical case study.
4. Analyze why software maturity matters more than TOPS marketing.
5. Compare RISC-V AI opportunities across edge, robotics, industrial, automotive, AI PC, and data center.
6. Build an evaluation checklist for any RISC-V AI chip startup.
7. Write a short architecture and market memo for a RISC-V AI platform.

---

**1. The Core Mental Model**

A RISC-V AI chip can mean several different things.

| Design Type | What RISC-V Does | Example Shape | Main Risk |
|---|---|---|---|
| RISC-V CPU with vector AI | Runs scalar code and vector kernels directly | Wide RVV CPU or AI CPU | Efficiency may trail fixed tensor engines |
| RISC-V CPU plus NPU | Controls a separate neural accelerator | Edge SoC with CPU + NPU | NPU software can be proprietary or fragmented |
| RISC-V control cores inside accelerator | Orchestrates tensor/dataflow fabric | AI accelerator with embedded RISC-V controllers | Developer may never see RISC-V directly |
| RISC-V host for GPU/accelerator | Replaces Arm/x86 host CPU around a bigger accelerator | RISC-V server/edge host plus GPU | AI performance mostly depends on attached accelerator |
| RISC-V custom extension platform | Adds vector, matrix, DSP, or domain-specific instructions | Custom ISA extension or CFU | Compiler and ecosystem support become hard |

The mistake is to say:

```text
RISC-V chip = AI accelerator
```

The better question is:

```text
Where is the actual AI compute, and can software use it efficiently?
```

---

**2. Why RISC-V Is Attractive for AI Chips**

RISC-V gives chip teams architectural freedom.

That matters because AI workloads change quickly. A company may want to tune the design around:

- INT8 and INT4 inference
- FP8 transformer inference
- BF16 or FP16 accumulation
- sparse matrix formats
- local memory movement
- camera and sensor pipelines
- robotics control loops
- always-on audio
- real-time safety islands

The attraction is not just "free ISA."

The attraction is **control**:

- control over extensions
- control over core count and memory hierarchy
- control over NPU coupling
- control over compiler target
- control over supply chain
- control over long-lifecycle embedded support

That is why RISC-V is strongest first in **bounded systems** where the product owner controls the stack.

---

**3. Where RISC-V AI Wins First**

The strongest near-term markets are not general-purpose AI training clusters.

They are edge and embedded systems where power, customization, and product integration matter.

**Strong Markets**

| Market | Why RISC-V Can Fit |
|---|---|
| Industrial vision | Bounded models, local inference, long lifecycle, cost pressure |
| Robotics | Inference + real-time control + custom I/O |
| AI edge gateways | Privacy, local processing, appliance-like deployment |
| Automotive controllers | Safety, determinism, vendor-neutral architecture pressure |
| AI sensors and TinyML | Always-on inference under strict power limits |
| Domestic silicon ecosystems | ISA openness and supply-chain control |
| Developer boards and AI PCs | Ecosystem seeding and Linux bring-up |

</details>

### 目前较弱的市

| 市场 | 为何困难 |
|---|---|
| 高端 AI 训练 | NVIDIA 的软件、HBM、互连与库构成的护城河 |
| 开箱即用的 PyTorch 研究 | CUDA 假设依然普遍 |
| 广泛的消费级笔记本 | 应用兼容性与 GPU/媒体栈仍然重要 |
| 超大规模 AI 服务器 | 目前由 Arm/x86 主机与 NVIDIA/ASIC 生态主导 |

短期论点：

> RISC-V AI 在系统围绕已知工作负载设计时胜出。当用户期望每个 CUDA/PyTorch 项目无需修改即可运行时，它就会陷入困境。

---

## 4. 设计模式 A：宽向量 AI CPU

该模式大量使用 RISC-V Vector（RVV）。

芯片试图把 AI kernel 当作可编程向量代码来运行，而不是把工作交给一个独立且不透明的 NPU。

优势：

- 可编程
- 更容易通过编译器暴露
- 内存一致性可能更简单
- CPU/NPU 之间的数据拷贝开销更少
- 更适合 AI 与非 AI 混合代码

风险：

- 峰值能效低于固定张量引擎
- 向量编译器质量成为关键
- 模型 kernel 必须与向量长度良好匹配
- TOPS 未必能转化为 tokens/sec 或 frames/sec

适合的工作负载：

- 中等规模本地 LLM 推理
- 图像预处理与后处理
- 量化矩阵运算
- 机器人感知加控制
- 边缘多模态流水线

在这一类别中，SpacemiT K3 值得研究，因为它突出 RISC-V AI CPU 定位与宽向量支持。

---

## 5. 设计模式 B：CPU + NPU SoC

这是最熟悉的边缘 AI 模式。

```text
RISC-V CPU
  + NPU
  + ISP / video codec
  + DMA
  + SRAM / LPDDR
  + Linux or RTOS
```

RISC-V CPU 负责：

- OS
- 控制流
- 模型调度
- 传感器管理
- 前/后处理
- 安全与更新逻辑

NPU 负责：

- 卷积
- 矩阵乘
- 类 attention kernel（若支持）
- 量化推理

优势：

- 对受支持的算子效率高
- 面向摄像头、传感器与网关的产品叙事清晰
- 比类 GPU 的大型设计更容易做功耗预算

风险：

- NPU 编译器可能只支持算子的一部分
- 回退到 CPU 会毁掉延迟
- 厂商 runtime 可能是闭源的
- 调试体验可能很差
- 不支持的模型 layer 会成为产品阻塞项

评估准则：

> NPU 的可用性，取决于其编译器、runtime 与回退行为。

---

## 6. 设计模式 C：AI 加速器中的 RISC-V 控制核

一些 AI 芯片在内部使用 RISC-V。

用户可能面对的是张量 runtime，而不是直接面对 RISC-V。

RISC-V 核可以管理：

- 命令队列
- DMA
- 同步
- 固件
- 电源管理
- 张量引擎编排

这很常见，因为 RISC-V 是一种良好的嵌入式控制架构。

但不应把它与「RISC-V 在做 AI 数学运算」混为一谈。

设计评审要问的问题是：

```text
Is RISC-V in the compute path, the control path, or both?
```

---

## 7. 设计模式 D：围绕 GPU 或加速器的 RISC-V 主机

RISC-V 也可以充当围绕另一个加速器的主机 CPU。

这对未来的异构系统很重要。

形态示例：

```text
RISC-V host CPU
  -> PCIe / CXL / chiplet link
  -> GPU or AI ASIC
```

AI 性能主要来自 GPU 或 ASIC。

RISC-V 贡献的是：

- 启动与平台控制
- 系统管理
- 安全信任根
- 工作负载编排
- 开放的主机 CPU 替代方案

这具有战略意义，但它与 RISC-V AI 加速器不是一回事。

---

## 8. SpacemiT 案例研究

SpacemiT 是个有用的例子，因为它试图构建一个可见的 RISC-V 计算平台，而不只是一个不可见的嵌入式控制器。

### K1：生态播种平台

K1 这一代有助于：

- RISC-V 上的 Linux
- 开发者板
- 低端 AI 实验
- 本地软件生态建设
- 证明真正消费级风格的 RISC-V 产品可以出货

它不是 AI 性能的主线。

K1 的教训是生态创建：

> 在 RISC-V AI 平台能够就模型展开竞争之前，开发者需要 Linux 镜像、驱动、开发板、文档、软件包仓库与稳定的工具链。

### K3：AI CPU 方向

截至 2026 年 4 月 30 日，公开的 K3 材料将其定位为 RISC-V AI CPU 平台，具备：

- 八个高性能 X100 RISC-V CPU 核
- 符合 RVA23
- 支持 1024-bit RISC-V Vector
- 面向 AI 推理的原生 FP8 支持
- 最高 60 TOPS AI 算力
- 最高 32 GB LPDDR5 内存
- 目标为本地中等规模 AI 与多模态边缘工作负载

重要的设计要点不只是 60 TOPS 这个数字。

重要的设计要点是 **同构 AI 计算** 这一方向：

```text
general CPU + vector AI + coherent memory + Linux software stack
```

这与一个小型 MCU（微控制器）加一个独立黑盒 NPU 不同。


<details>
<summary>English original</summary>

**Weak Markets for Now**

| Market | Why It Is Hard |
|---|---|
| High-end AI training | NVIDIA software, HBM, interconnect, and library moat |
| Plug-and-play PyTorch research | CUDA assumptions remain common |
| Broad consumer laptops | Application compatibility and GPU/media stacks still matter |
| Hyperscale AI servers | Arm/x86 hosts and NVIDIA/ASIC ecosystems dominate today |

The near-term thesis:

> RISC-V AI wins where the system is designed around a known workload. It struggles where users expect every CUDA/PyTorch project to work unmodified.

---

**4. Design Pattern A: Wide Vector AI CPU**

This pattern uses RISC-V Vector (RVV) heavily.

The chip tries to run AI kernels as programmable vector code instead of sending work to a separate opaque NPU.

Advantages:

- programmable
- easier to expose through compilers
- potentially simpler memory coherence
- less CPU/NPU data-copy overhead
- better for mixed AI and non-AI code

Risks:

- lower peak energy efficiency than fixed tensor engines
- vector compiler quality becomes critical
- model kernels must map well to vector lengths
- TOPS may not translate into tokens/sec or frames/sec

Good workloads:

- medium-scale local LLM inference
- image preprocessing and postprocessing
- quantized matrix operations
- robotics perception plus control
- edge multimodal pipelines

SpacemiT K3 is useful to study in this category because it emphasizes RISC-V AI CPU positioning and wide vector support.

---

**5. Design Pattern B: CPU + NPU SoC**

This is the most familiar edge AI pattern.

```text
RISC-V CPU
  + NPU
  + ISP / video codec
  + DMA
  + SRAM / LPDDR
  + Linux or RTOS
```

The RISC-V CPU handles:

- OS
- control flow
- model scheduling
- sensor management
- pre/post-processing
- security and update logic

The NPU handles:

- convolution
- matrix multiply
- attention-like kernels if supported
- quantized inference

Advantages:

- high efficiency for supported operators
- clear product story for cameras, sensors, and gateways
- easier power budgeting than large GPU-like designs

Risks:

- NPU compiler may support only a subset of operators
- fallback to CPU can destroy latency
- vendor runtime may be closed
- debugging can be poor
- unsupported model layers become product blockers

Evaluation rule:

> An NPU is only as useful as its compiler, runtime, and fallback behavior.

---

**6. Design Pattern C: RISC-V Control Cores in an AI Accelerator**

Some AI chips use RISC-V internally.

The user may interact with a tensor runtime, not with RISC-V directly.

RISC-V cores can manage:

- command queues
- DMA
- synchronization
- firmware
- power management
- tensor engine orchestration

This is common because RISC-V is a good embedded control architecture.

But it should not be confused with "RISC-V is doing the AI math."

The design review question is:

```text
Is RISC-V in the compute path, the control path, or both?
```

---

**7. Design Pattern D: RISC-V Host Around GPU or Accelerator**

RISC-V can also act as the host CPU around another accelerator.

This matters for future heterogeneous systems.

Example shape:

```text
RISC-V host CPU
  -> PCIe / CXL / chiplet link
  -> GPU or AI ASIC
```

The AI performance mostly comes from the GPU or ASIC.

RISC-V contributes:

- boot and platform control
- system management
- security root of trust
- workload orchestration
- open host CPU alternative

This is strategically important, but it is not the same as a RISC-V AI accelerator.

---

**8. SpacemiT Case Study**

SpacemiT is a useful example because it is trying to build a visible RISC-V computing platform, not only an invisible embedded controller.

**K1: Ecosystem-Seeding Platform**

The K1 generation is useful for:

- Linux on RISC-V
- developer boards
- low-end AI experimentation
- local software ecosystem building
- showing that real consumer-style RISC-V products can ship

It is not the main AI-performance story.

The lesson from K1 is ecosystem creation:

> Before a RISC-V AI platform can compete on models, developers need Linux images, drivers, boards, documentation, package repositories, and stable toolchains.

**K3: AI CPU Direction**

As of April 30, 2026, public K3 materials position it as a RISC-V AI CPU platform with:

- eight high-performance X100 RISC-V CPU cores
- RVA23 compliance
- 1024-bit RISC-V Vector support
- native FP8 support for AI inference
- up to 60 TOPS AI compute
- up to 32 GB LPDDR5 memory
- local medium-scale AI and multimodal edge workloads as the target

The important design point is not only the 60 TOPS number.

The important design point is the **homogeneous AI computing** direction:

```text
general CPU + vector AI + coherent memory + Linux software stack
```

That is different from a tiny MCU plus separate black-box NPU.

</details>

### K3 为何值得关注

K3 之所以值得关注，是因为它结合了：

- 应用级 RISC-V
- 面向 AI 的向量能力
- Linux/Ubuntu 支持
- 边缘与机器人定位
- 足以在本地做模型实验的内存容量

这一组合使其成为下一波 RISC-V 边缘 AI 平台的一个严肃教学案例。

### 哪些仍待验证

难的问题都很实际：

- 在有用的 LLM 上，真实 tokens/s 是多少？
- 开放 runtime 能触及 60 TOPS 中的多少？
- 哪些模型无需手工修改即可编译？
- 驱动与性能分析器的成熟度如何？
- GPU/媒体栈是否达到生产可用？
- 上游 Linux 支持的稳定性如何？
- 持续推理下的功耗是多少？
- 在真实工作负载下，它与 Jetson、Hailo、Qualcomm、Apple 和 x86 AI PC 相比如何？

严肃的评估不是：

```text
Does it advertise TOPS?
```

而是：

```text
Can a developer deploy a model, profile it, optimize it, and ship a product?
```

---

## 9. 软件栈：真正的采用门槛

RISC-V AI 芯片不能仅靠硬件取胜。

它们需要这样的栈：

```text
Linux / RTOS
  -> boot firmware
  -> device tree / ACPI
  -> kernel drivers
  -> vector/matrix compiler support
  -> AI runtime
  -> model converter
  -> quantization tools
  -> profiler
  -> reference applications
```

对于应用级系统，Linux 发行版支持是一个重要信号。

SpacemiT K1/K3 的 Ubuntu 支持之所以重要，是因为它降低了开发者的摩擦：

- 熟悉的软件包
- 标准 userland
- 稳定的发布流程
- 更简单的 CI 与可复现性
- 更广泛的社区测试

对于嵌入式产品，与之对等的信号是 Yocto、Buildroot、RTOS、安全工具链以及长期 BSP 支持。

需要问的软件问题：

- 编译器是上游的还是厂商独有的？
- LLVM 是否认识该向量/矩阵目标？
- TVM、IREE、ONNX Runtime、llama.cpp 或 PyTorch 能否使用该硬件？
- kernel 是否足够开放以便调试？
- 开发者能否剖析计算瓶颈与内存停顿？
- 回退路径是否明确？
- 板子能否运行标准容器？

没有这套栈的硬件只是 demo，不是平台。

---

## 10. 市场分析

### 看多论据

RISC-V 受益于多股市场力量：

- 边缘 AI 增长
- 汽车软件定义平台
- 工业自动化
- 机器人
- 供应链多元化
- 降低 ISA 授权依赖的诉求
- 围绕已知工作负载定制加速器的需求
- 开源软件势头

Omdia 预测 RISC-V 出货量将在 2030 年前快速增长，AI、尤其是边缘 AI 是主要采用驱动力。

ABI Research 在 2026 年 4 月的表述是一个有用的实务总结：RISC-V 的 AI 机会是真实的，但会先集中在边界明确的边缘系统，而不是广泛替代数据中心 CPU。

### 看空论据

RISC-V 仍面临重大采用障碍：

- 厂商扩展碎片化
- Linux 与驱动质量参差不齐
- 商业软件生态弱于 Arm/x86
- AI 性能剖析工具不成熟
- 生产级 ML kernel 更少
- 开发者熟悉度有限
- 部分初创公司的长期支持不明确
- 来自 NVIDIA、Arm、Qualcomm、Apple、Hailo 和 x86 AI PC 的激烈竞争

RISC-V 的开放性有帮助，但客户买的仍是能用的产品。

### 最可能的结果

RISC-V AI 的采用将是有选择性的。

可能率先胜出的领域：

- 工业边缘相机
- 机器人控制器
- AI 传感器中枢
- 汽车域/区域控制器
- 本土 RISC-V 开发者生态
- 专用 AI 设备

近期较难胜出的领域：

- 大规模训练集群
- 通用 CUDA 替代品
- 主流消费级笔记本
- 广泛的超大规模 AI CPU 份额

务实的预测：

> RISC-V 将在架构尚未定型、产品团队能够协同设计工作负载、硅片、runtime 和 OS 的地方最为重要。

---

## 11. 竞争格局

| 平台 | RISC-V 威胁程度 | 原因 |
|---|---:|---|
| NVIDIA GPU 训练 | 低 | CUDA、HBM、NVLink、库与生态仍占主导 |
| NVIDIA Jetson 边缘 AI | 中 | RISC-V 可在更低功耗或主权边缘设计中竞争 |
| Hailo 边缘加速器 | 中 | RISC-V 平台可能直接集成类 NPU 算力 |
| Qualcomm 机器人/IoT | 中 | RISC-V 可瞄准类似的异构边缘系统 |
| Arm Cortex-A / Cortex-M | 新嵌入式设计中为高 | RISC-V 直接争夺 CPU/控制角色 |
| x86 AI PC | 低到中 | 应用兼容性仍是障碍 |
| TinyML MCU | 高 | 定制 RISC-V MCU 与扩展适配良好 |
| 汽车控制器 | 随时间由中到高 | 安全生态与长生命周期将起决定作用 |

---

## 12. 评估检查清单

对任何 RISC-V AI 芯片都可用这份检查清单。


<details>
<summary>English original</summary>

**What Makes K3 Interesting**

K3 is interesting because it combines:

- application-class RISC-V
- AI-oriented vector capability
- Linux/Ubuntu enablement
- edge and robotics positioning
- enough memory capacity for local model experiments

That combination makes it a serious teaching case for the next wave of RISC-V edge AI platforms.

**What Still Needs Proof**

The hard questions are practical:

- What are real tokens/sec on useful LLMs?
- How much of the 60 TOPS is reachable by open runtimes?
- Which models compile without hand work?
- How mature are drivers and profilers?
- Is the GPU/media stack production-ready?
- How stable is upstream Linux support?
- What is power under sustained inference?
- How does it compare with Jetson, Hailo, Qualcomm, Apple, and x86 AI PCs on real workloads?

The serious evaluation is not:

```text
Does it advertise TOPS?
```

It is:

```text
Can a developer deploy a model, profile it, optimize it, and ship a product?
```

---

**9. Software Stack: The Real Adoption Gate**

RISC-V AI chips do not win through hardware alone.

They need a stack like this:

```text
Linux / RTOS
  -> boot firmware
  -> device tree / ACPI
  -> kernel drivers
  -> vector/matrix compiler support
  -> AI runtime
  -> model converter
  -> quantization tools
  -> profiler
  -> reference applications
```

For application-class systems, Linux distribution support is a major signal.

Ubuntu support for SpacemiT K1/K3 matters because it reduces developer friction:

- familiar packages
- standard userland
- stable release process
- easier CI and reproducibility
- wider community testing

For embedded products, the equivalent signal is Yocto, Buildroot, RTOS, safety tooling, and long-term BSP support.

Software questions to ask:

- Is the compiler upstream or vendor-only?
- Does LLVM know the vector/matrix target?
- Can TVM, IREE, ONNX Runtime, llama.cpp, or PyTorch use the hardware?
- Are kernels open enough to debug?
- Can developers profile compute versus memory stalls?
- Are fallback paths explicit?
- Can the board run standard containers?

Hardware without this stack is a demo, not a platform.

---

**10. Market Analysis**

**The Bull Case**

RISC-V benefits from several market forces:

- edge AI growth
- automotive software-defined platforms
- industrial automation
- robotics
- supply-chain diversification
- desire to reduce ISA licensing dependency
- need for custom accelerators around known workloads
- open-source software momentum

Omdia has forecast rapid RISC-V shipment growth through 2030, with AI and especially edge AI as major adoption drivers.

ABI Research's April 2026 framing is a useful practical summary: RISC-V's AI opportunity is real, but concentrated first in bounded edge systems rather than broad data-center CPU replacement.

**The Bear Case**

RISC-V still faces major adoption barriers:

- fragmented vendor extensions
- uneven Linux and driver quality
- weaker commercial software ecosystem than Arm/x86
- immature AI profiling tools
- fewer production ML kernels
- limited developer familiarity
- unclear long-term support for some startups
- hard competition from NVIDIA, Arm, Qualcomm, Apple, Hailo, and x86 AI PCs

RISC-V openness helps, but customers still buy working products.

**Most Likely Outcome**

RISC-V AI adoption will be selective.

Likely first winners:

- industrial edge cameras
- robotics controllers
- AI sensor hubs
- automotive domain/zonal controllers
- domestic RISC-V developer ecosystems
- specialized AI appliances

Less likely near-term winners:

- large-scale training clusters
- general CUDA replacement
- mainstream consumer laptops
- broad hyperscale AI CPU share

The practical forecast:

> RISC-V will matter most where architecture is still fluid and the product team can co-design workload, silicon, runtime, and OS together.

---

**11. Competitive Map**

| Platform | RISC-V Threat Level | Why |
|---|---:|---|
| NVIDIA GPU training | Low | CUDA, HBM, NVLink, libraries, and ecosystem remain dominant |
| NVIDIA Jetson edge AI | Medium | RISC-V can compete in lower-power or sovereign edge designs |
| Hailo edge accelerators | Medium | RISC-V platforms may integrate NPU-like compute directly |
| Qualcomm robotics/IoT | Medium | RISC-V can target similar heterogeneous edge systems |
| Arm Cortex-A / Cortex-M | High in new embedded designs | RISC-V directly competes for CPU/control roles |
| x86 AI PCs | Low to medium | Application compatibility remains a barrier |
| TinyML MCUs | High | Custom RISC-V MCUs and extensions fit well |
| Automotive controllers | Medium to high over time | Safety ecosystem and long lifecycle will decide |

---

**12. Evaluation Checklist**

Use this checklist for any RISC-V AI chip.

</details>

### 架构

- 核心数量与类别是什么？
- 是否支持 RVV？
- 是否支持矩阵扩展或自定义 AI 指令？
- AI 计算是基于向量、NPU、张量 fabric 还是混合？
- 支持哪些精度：INT8、INT4、FP8、BF16、FP16？
- 片上 SRAM 有多少？
- 外部内存带宽是多少？
- CPU 与加速器之间内存是否一致？
- DMA 和 scratchpad 传输是否显式？

### 软件

- 是否有 Linux 上游支持？
- 是否支持 Ubuntu、Debian、Yocto 或 Buildroot？
- 驱动是开源、部分开源还是闭源？
- 目前哪些 AI runtime 可用？
- 接受哪些模型格式？
- 遇到不支持的算子会怎样？
- 是否有性能分析器？
- 开发者能否使用容器？
- 编译器是否与 LLVM、MLIR、TVM、IREE 或厂商专有工具链集成？

### 性能

- 大语言模型 decode（逐 token 生成阶段）的实际 tokens/sec 是多少？
- prefill（首字前的整段计算）吞吐是多少？
- 批大小为 1 时视觉 FPS 是多少？
- 持续负载下的功耗是多少？
- 可用于模型权重和 KV cache 的内存有多少？
- 动态 shape 下性能是否会崩溃？
- CPU 到加速器的传输开销有多大？

### 市场

- 买家是谁？
- 它替代哪个现有平台？
- 工作负载是受控还是通用？
- 软件栈是否成熟到可用于生产？
- 是否有可靠的板卡、模块或系统供应商？
- 长期支持是否明确？
- 该平台是否在开放性、功耗、成本或集成度上有差异化？

---

## 13. 动手项目：RISC-V AI 芯片评审备忘录

选择一个 RISC-V AI 平台：

- SpacemiT K1 或 K3
- Tenstorrent 嵌入式或加速器 IP
- SiFive Intelligence 系列
- MIPS S8200
- Semidynamics Cervell NPU
- Andes AI/向量核
- Google Coral/Kelvin 风格的 RISC-V 边缘 AI 参考设计

写一份 4 页备忘录：

1. **架构摘要：** CPU、向量、NPU、内存、I/O、OS。
2. **软件栈：** Linux、编译器、runtime、模型支持、性能剖析。
3. **工作负载适配：** 哪些模型应能跑得好？哪些应避免？
4. **市场适配：** 它首先能在哪里胜出？
5. **风险：** 量产采用前必须证明什么？
6. **结论：** 开发板、产品平台还是学术 curiosities？

尽可能使用实测 benchmark。如果只有营销数据，明确说明。

---

## 14. 毕业设计项目：设计一款 RISC-V 边缘 AI SoC

设计一款面向本地多模态推理的产品级 SoC。

目标：

- 15-25 W 系统功耗
- 本地视觉与语音推理
- 可选的 7B 到 30B 量化大语言模型实验
- 支持 Linux 的开发环境
- 机器人或工业网关形态

指定：

- RISC-V CPU profile 与核心数量
- 向量长度与精度支持
- NPU 或张量单元设计
- 内存容量与带宽
- 片上 SRAM 与 DMA 策略
- 摄像头/视频/音频模块
- Linux 与驱动策略
- 编译器/runtime 路径
- benchmark 套件
- 目标客户

然后回答：

```text
Why should this be RISC-V instead of Arm, x86, Jetson, or a discrete NPU?
```


如果回答不了这个问题，设计就没有差异化。

---

## 关键要点

- RISC-V 是一种 ISA，不自动等于 AI 加速器。
- RISC-V AI 最强的机会在边界清晰的边缘系统，那里工作负载和软件栈可控。
- SpacemiT 是一个有用的 2026 年案例，因为它结合了应用级 RISC-V、面向 AI 的向量计算、Linux 支持与边缘 AI 定位。
- TOPS 不够；要评测 tokens/sec、FPS、功耗、内存带宽、算子覆盖与工具链。
- 市场机会真实但具有选择性：在广泛替代数据中心之前，先切入边缘、机器人、工业、汽车和 AI 设备。
- 可信的 RISC-V AI 芯片必须作为完整平台来设计：芯片、编译器、runtime、OS、板卡与参考应用。

---


<details>
<summary>English original</summary>

**Architecture**

- What is the core count and class?
- Does it support RVV?
- Does it support matrix extensions or custom AI instructions?
- Is AI compute vector-based, NPU-based, tensor-fabric-based, or mixed?
- What precisions are supported: INT8, INT4, FP8, BF16, FP16?
- How much on-chip SRAM exists?
- What is the external memory bandwidth?
- Is memory coherent between CPU and accelerator?
- Are DMA and scratchpad transfers explicit?

**Software**

- Is Linux upstream support available?
- Is there Ubuntu, Debian, Yocto, or Buildroot support?
- Are drivers open, partially open, or closed?
- Which AI runtimes work today?
- Which model formats are accepted?
- What happens on unsupported operators?
- Is there a profiler?
- Can developers use containers?
- Is the compiler integrated with LLVM, MLIR, TVM, IREE, or vendor-only tooling?

**Performance**

- What is real tokens/sec for LLM decode?
- What is prefill throughput?
- What is vision FPS at batch 1?
- What is power under sustained load?
- How much memory is available for model weights and KV cache?
- Does performance collapse with dynamic shapes?
- How costly are CPU-to-accelerator transfers?

**Market**

- Who is the buyer?
- What incumbent platform does it replace?
- Is the workload controlled or general-purpose?
- Is the software stack mature enough for production?
- Is there a credible board, module, or system vendor?
- Is long-term support clear?
- Is the platform differentiated by openness, power, cost, or integration?

---

**13. Hands-On Project: RISC-V AI Chip Review Memo**

Pick one RISC-V AI platform:

- SpacemiT K1 or K3
- Tenstorrent embedded or accelerator IP
- SiFive Intelligence family
- MIPS S8200
- Semidynamics Cervell NPU
- Andes AI/vector cores
- Google Coral/Kelvin-style RISC-V edge AI reference design

Write a 4-page memo:

1. **Architecture summary:** CPU, vector, NPU, memory, I/O, OS.
2. **Software stack:** Linux, compiler, runtime, model support, profiling.
3. **Workload fit:** Which models should it run well? Which should it avoid?
4. **Market fit:** Where can it win first?
5. **Risks:** What must be proven before production adoption?
6. **Verdict:** Developer board, product platform, or research curiosity?

Use measured benchmarks where possible. If only marketing data exists, state that clearly.

---

**14. Capstone Project: Design a RISC-V Edge AI SoC**

Design a product-level SoC for local multimodal inference.

Target:

- 15-25 W system power
- local vision and voice inference
- optional 7B to 30B quantized LLM experiments
- Linux-capable development environment
- robotics or industrial gateway form factor

Specify:

- RISC-V CPU profile and core count
- vector length and precision support
- NPU or tensor unit design
- memory capacity and bandwidth
- on-chip SRAM and DMA strategy
- camera/video/audio blocks
- Linux and driver strategy
- compiler/runtime path
- benchmark suite
- target customer

Then answer:

```text
Why should this be RISC-V instead of Arm, x86, Jetson, or a discrete NPU?
```

If you cannot answer that, the design is not differentiated.

---

**Key Takeaways**

- RISC-V is an ISA, not automatically an AI accelerator.
- The strongest RISC-V AI opportunities are bounded edge systems where workload and software stack can be controlled.
- SpacemiT is a useful 2026 case study because it combines application-class RISC-V, AI-oriented vector compute, Linux enablement, and edge AI positioning.
- TOPS is not enough; evaluate tokens/sec, FPS, power, memory bandwidth, operator coverage, and tooling.
- The market opportunity is real, but selective: edge, robotics, industrial, automotive, and AI appliances before broad data-center replacement.
- A credible RISC-V AI chip must be designed as a full platform: silicon, compiler, runtime, OS, board, and reference applications.

---

</details>

## 参考文献

- [SpacemiT announces Ubuntu on K3/K1 series RISC-V AI computing platforms](https://canonical.com/blog/spacemit-announces-availability-of-ubuntu-on-k3-k1-series) - Canonical 公告，涵盖 Ubuntu 支持与 RVA23 定位。
- [SpacemiT launches K3 AI CPU](https://www.globenewswire.com/news-release/2026/01/30/3229216/0/en/Chinese-RISC-V-Chipmaker-SpacemiT-Launches-K3-AI-CPU-Highlighting-the-Rise-of-Open-Source-Hardware-in-Intelligent-Computing.html) - 发布文章，包含 K3 规格与产品定位。
- [RISC-V adoption will be accelerated by AI](https://omdia.tech.informa.com/pr/2024/may/risc-v-adoption-will-be-accelerated-by-ai-according-to-new-omdia-research) - Omdia 对 RISC-V 出货量增长以及 AI/边缘应用的预测。
- [RISC-V's AI Opportunity Is Real](https://www.abiresearch.com/market-research/insight/7787778-risc-vs-ai-opportunity-is-real-but-can-edg) - ABI Research 关于边缘 AI 有限角色的市场简报。
- [Production-Ready, Automotive-Grade, AI-Native: RISC-V at Embedded World 2026](https://riscv.org/blog/embedded-world-2026/) - RISC-V International 对边缘 AI、嵌入式与汽车生态势头的概述。
- [RISC-V Annual Report 2025](https://riscv.org/wp-content/uploads/2026/01/RISC-V-Annual-Report-2025.pdf) - 生态报告，涵盖 profiles、边缘 AI、vector/matrix 方向与软件势头。


<details>
<summary>English original</summary>

**References**

- [SpacemiT announces Ubuntu on K3/K1 series RISC-V AI computing platforms](https://canonical.com/blog/spacemit-announces-availability-of-ubuntu-on-k3-k1-series) - Canonical announcement covering Ubuntu enablement and RVA23 positioning.
- [SpacemiT launches K3 AI CPU](https://www.globenewswire.com/news-release/2026/01/30/3229216/0/en/Chinese-RISC-V-Chipmaker-SpacemiT-Launches-K3-AI-CPU-Highlighting-the-Rise-of-Open-Source-Hardware-in-Intelligent-Computing.html) - Launch article with K3 specifications and product positioning.
- [RISC-V adoption will be accelerated by AI](https://omdia.tech.informa.com/pr/2024/may/risc-v-adoption-will-be-accelerated-by-ai-according-to-new-omdia-research) - Omdia forecast on RISC-V shipment growth and AI/edge adoption.
- [RISC-V's AI Opportunity Is Real](https://www.abiresearch.com/market-research/insight/7787778-risc-vs-ai-opportunity-is-real-but-can-edg) - ABI Research market note on bounded edge AI roles.
- [Production-Ready, Automotive-Grade, AI-Native: RISC-V at Embedded World 2026](https://riscv.org/blog/embedded-world-2026/) - RISC-V International overview of edge AI, embedded, and automotive ecosystem momentum.
- [RISC-V Annual Report 2025](https://riscv.org/wp-content/uploads/2026/01/RISC-V-Annual-Report-2025.pdf) - Ecosystem report covering profiles, edge AI, vector/matrix direction, and software momentum.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track F - AI Chip Design/RISC-V-AI-Chip-Design-and-Market/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20F%20-%20AI%20Chip%20Design/RISC-V-AI-Chip-Design-and-Market/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
