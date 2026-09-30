<div class="home-hero">
<div class="home-hero__copy">
<p class="home-hero__eyebrow">AI 硬件工程师路线图</p>
<h1 class="home-hero__title"><span>AI 硬件工程。</span> <span>从模型到机器。</span></h1>
<p class="home-hero__lede">一套动手实践的课程，用于理解、优化并部署真实硬件上的 AI —— GPU kernel、agent runtime、嵌入式系统与 Jetson —— 并通向设计底层加速器的路径。从一个可运行的实验开始；以他人能复现的结果收尾。</p>
</div>
<div class="home-hero__art">
  <img src="Assets/images/physical-ai-chip.png" alt="AI Hardware Engineering — from models to machines" />
</div>
</div>

<div class="home-rail" markdown="1">
<div class="home-rail__row">
  <a class="home-rail__step" href="阶段1-基础知识/Guide.md"><span class="home-rail__n">01</span><span class="home-rail__t">数字基础</span></a>
  <a class="home-rail__step" href="阶段2-嵌入式系统/Guide.md"><span class="home-rail__n">02</span><span class="home-rail__t">嵌入式系统</span></a>
  <a class="home-rail__step" href="阶段3-人工智能/Guide.md"><span class="home-rail__n">03</span><span class="home-rail__t">人工智能</span></a>
  <a class="home-rail__step" href="阶段4-路线C-深度学习推理优化/Guide.md"><span class="home-rail__n">04</span><span class="home-rail__t">部署 &amp; 编译</span></a>
  <a class="home-rail__step" href="阶段5-高级专题与专精/Guide.md"><span class="home-rail__n">05</span><span class="home-rail__t">专精方向</span></a>
  <a class="home-rail__step" href="阶段6-面试准备/README.md"><span class="home-rail__n">06</span><span class="home-rail__t">面试准备</span></a>
</div>
</div>

---

## 目标

> **掌握 AI 推理、AI agent harness（agent 运行时框架）系统与硬件工程 —— 然后设计一颗物理 AI 芯片。**

终点是一颗单片 die：它运行生产级 AI 工作负载、承载真实的 agent runtime，并通过 Wi-Fi/低功耗蓝牙（BLE）/Thread 与物理世界通信 —— 一颗 Jetson 级 AI 大脑，与一套 ESP32 级无线协议栈融合在同一颗芯片上。设计它需要同时具备三套可落地的技能：**推理工程**（Qwen 级 decode（逐 token 生成阶段）、kernel、量化、roofline（性能上界模型）、多 GPU 推理服务）、**agent harness 系统**（会话、工具、多 agent 循环、RAG（检索增强生成）、评测、生产可观测性）与**硬件工程**（RTL、嵌入式 Linux、Jetson、ESP32、RF、ASIC 流程）。本路线图将这三者并排教授 —— 芯片是产物，但这三大支柱才是真正的工作。

---

## 适合谁读

- **AI/ML 工程师**：希望不再把推理当作黑盒，转而设计运行它的芯片。
- **推理工程师**：希望向上延伸到 agent runtime 协同设计，向下延伸到 kernel 与硅。
- **嵌入式/固件工程师**：希望沿技术栈向上攀登 —— 从板卡到 runtime 再到芯片架构。
- **硬件/RTL/FPGA 工程师**：在定义加速器规格之前，需要工作负载与 runtime 的直觉。
- **计算机专业学生**：想要一条结构化路径，终点是「我设计了一颗芯片」，而不是「我读过关于芯片的资料」。

**基线：** 熟悉 Python 或 C++，会用 Linux 命令行基础操作。不要求硬件背景。

**已经过了基础阶段？** [阶段 1](阶段1-基础知识/Guide.md) 与 [阶段 2](阶段2-嵌入式系统/Guide.md) 的存在是为了填补特定缺口 —— 数字逻辑、嵌入式 Linux —— 而不是用来设门槛的。如果已经会 CUDA/C++，直接进入 [阶段 3](阶段3-人工智能/Guide.md)，只在后续模块明确说需要时，再去补阶段 1–2 的内容。

如果只想调用 LLM API，这份路线图不适合你。如果想设计调用它的硅片，继续往下读。

---


<details>
<summary>English original</summary>

<div class="home-hero">
<div class="home-hero__copy">
<p class="home-hero__eyebrow">The AI Hardware Engineer Roadmap</p>
<h1 class="home-hero__title"><span>AI Hardware Engineering.</span> <span>From models to machines.</span></h1>
<p class="home-hero__lede">A hands-on curriculum for understanding, optimizing, and deploying AI on real hardware — GPU kernels, agent runtimes, embedded systems, and Jetson — with a path toward designing the accelerator underneath. Start with one working experiment; finish with results someone else can reproduce.</p>
</div>
<div class="home-hero__art">
  <img src="Assets/images/physical-ai-chip.png" alt="AI Hardware Engineering — from models to machines" />
</div>
</div>

<div class="home-rail" markdown="1">
<div class="home-rail__row">
  <a class="home-rail__step" href="阶段1-基础知识/Guide.md"><span class="home-rail__n">01</span><span class="home-rail__t">Digital Foundations</span></a>
  <a class="home-rail__step" href="阶段2-嵌入式系统/Guide.md"><span class="home-rail__n">02</span><span class="home-rail__t">Embedded Systems</span></a>
  <a class="home-rail__step" href="阶段3-人工智能/Guide.md"><span class="home-rail__n">03</span><span class="home-rail__t">Artificial Intelligence</span></a>
  <a class="home-rail__step" href="阶段4-路线C-深度学习推理优化/Guide.md"><span class="home-rail__n">04</span><span class="home-rail__t">Deployment &amp; Compilation</span></a>
  <a class="home-rail__step" href="阶段5-高级专题与专精/Guide.md"><span class="home-rail__n">05</span><span class="home-rail__t">Specialization</span></a>
  <a class="home-rail__step" href="阶段6-面试准备/README.md"><span class="home-rail__n">06</span><span class="home-rail__t">Interview Prep</span></a>
</div>
</div>

---

**The Goal**

> **Master AI inference, AI agent harness systems, and hardware engineering — then design a physical AI chip.**

The endpoint is a single die that runs production AI workloads, hosts a real agent runtime, and talks to the physical world over Wi-Fi/BLE/Thread — a Jetson-class AI brain fused with an ESP32-class wireless stack on one chip. Designing it takes three working skill sets at once: **inference engineering** (Qwen-class decode, kernels, quantization, rooflines, multi-GPU serving), **agent harness systems** (sessions, tools, multi-agent loops, RAG, evals, production observability), and **hardware engineering** (RTL, embedded Linux, Jetson, ESP32, RF, ASIC flow). This roadmap teaches all three side by side — the chip is the artifact, but the three pillars are the actual work.

---

**Who This Is For**

- **AI/ML engineers** who want to stop treating inference as a black box and design the chip that runs it.
- **Inference engineers** who want to extend up into agent-runtime co-design and down into kernel + silicon.
- **Embedded/firmware engineers** who want to climb the stack — from boards to runtimes to chip architecture.
- **Hardware/RTL/FPGA engineers** who need workload and runtime intuition before specing accelerators.
- **CS students** who want a structured path that ends at "I designed a chip" rather than "I read about chips."

**Baseline:** comfortable with Python or C++, and basic Linux command-line use. No prior hardware background required.

**Already past the basics?** [Phase 1](阶段1-基础知识/Guide.md) and [Phase 2](阶段2-嵌入式系统/Guide.md) exist to fill specific gaps — digital logic, embedded Linux — not to gate you. If you already know CUDA/C++, go straight to [Phase 3](阶段3-人工智能/Guide.md) and pick up Phase 1–2 material only where a later module says you need it.

If you only want to call an LLM API, this isn't for you. If you want to design the silicon that calls it, keep reading.

---

</details>

## Three Ways In

选一条与你想要达到的能力相匹配的终点线，而不是一个赛道编号。

| Path | 你会构建什么 | 终点线产物 | 从这里开始 |
|---|---|---|---|
| **AI 推理工程** | Transformer 执行、GEMV（矩阵-向量乘）/GEMM（矩阵-矩阵乘）kernel、量化、推理服务 | 一份 benchmark 报告：真实模型上预测与实测 tok/s 的对比，并解释其中差距 | [阶段 3 — AI Workloads](阶段3-人工智能/Guide.md) |
| **AI Agent Harness 系统** | 会话、工具调用、多 agent 循环、评测、可观测性 | 一套生产形态的 agent harness（agent 运行时框架），可端到端运行 | [Agentic AI & GenAI — 42 lectures](阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/README.md) |
| **物理硬件工程** | 板卡、嵌入式 Linux、RTL、FPGA、ASIC 流程 | 一块自己 bring-up 的板卡，或一份带真实数字的加速器规格 | [阶段 1 — Digital Foundations](阶段1-基础知识/Guide.md) |

每条路径都自成一体 —— 不需要另外两条，你也能从中得到实打实的收获。[The Path](#the-path) 是三条路径汇聚到 physical-AI-chip 端点的地方。

---

## 三大支柱

*(上面的路径已经选好了？本节深入解释三者为何相互关联 —— 若只想看逐阶段的拆解，直接跳到 [The Path](#the-path)。)*

这条路线图不是三条互不相关的学习轨道，而是一个协同设计闭环。推理工作负载告诉你芯片必须加速什么，agent harness 告诉你 runtime 必须支撑什么产品行为，硬件平台则告诉你哪些功耗、内存、I/O、安全与制造约束是真实存在的。

```
                               TARGET ARTIFACT
┌────────────────────────────────────────────────────────────────────────────┐
│                         Physical AI Agent Chip                             │
│ Linux-capable CPU + NPU/GPU/DLA + SRAM/DRAM fabric + ISP/audio/sensors     │
│ + Wi-Fi/BLE/Thread/Zigbee + secure boot/OTA + local agent runtime support  │
└────────────────────────────────────┬───────────────────────────────────────┘
                                     │
        ┌────────────────────────────┼────────────────────────────┐
        ▼                            ▼                            ▼
┌───────────────────────┐  ┌───────────────────────┐  ┌───────────────────────┐
│  AI Inference Systems │  │ Agent Harness Systems │  │ Physical HW Platform  │
├───────────────────────┤  ├───────────────────────┤  ├───────────────────────┤
│ Transformer execution │  │ Sessions and memory   │  │ Digital logic + RTL   │
│ GEMV/GEMM kernels     │  │ Tool/skill execution  │  │ CPU/NPU/GPU/DLA arch  │
│ Attention + KV cache  │  │ Gateway/RPC protocols │  │ SRAM/DRAM/DMA fabric  │
│ Quantization formats  │  │ Planner/executor loop │  │ MIPI CSI-2 + ISP      │
│ Tensor/pipeline para. │  │ Multi-agent control   │  │ Audio/GPIO/CAN/I2C    │
│ Serving schedulers    │  │ Evals and guardrails  │  │ Embedded Linux/L4T    │
│ CUDA/Triton kernels   │  │ Telemetry and tracing │  │ ESP32-class wireless  │
│ Roofline profiling    │  │ Product update path   │  │ FPGA -> ASIC flow     │
│ Real models: Qwen etc │  │ OpenClaw/SDKs/APIs    │  │ Board -> SoC thinking │
└───────────┬───────────┘  └───────────┬───────────┘  └───────────┬───────────┘
            │                          │                          │
            └──────────────┬───────────┴───────────┬──────────────┘
                           ▼                       ▼
        ┌────────────────────────────────────────────────────────────────┐
        │ Cross-layer contracts you learn to write                       │
        ├────────────────────────────────────────────────────────────────┤
        │ Workload contract: tokens/s, TTFT, context length, KV memory,  │
        │ batch shape, precision, latency tail, power budget.            │
        │ Runtime contract: APIs, cancellation, scheduling, security,    │
        │ observability, OTA, failure recovery, agent state persistence. │
        │ Hardware contract: MAC array, SRAM size, DMA plan, interconnect│
        │ bandwidth, radio wake events, camera/audio I/O, boot chain.    │
        └────────────────────────────────────────────────────────────────┘

                              CONVERGENCE PATH
┌────────────────────────────────────────────────────────────────────────────┐
│ Jetson-class AI subsystem        -> NPU/GPU/DLA/CPU compute tile           │
│ ESP32-class connectivity         -> Wi-Fi/BLE/Thread/Zigbee radio block    │
│ Camera, audio, and sensors       -> MIPI CSI-2, ISP, codecs, low-power I/O │
│ Agent runtime and serving stack  -> scheduler, memory manager, telemetry   │
│ FPGA/RTL/HLS/MLIR experiments    -> accelerator spec and tape-out target   │
└────────────────────────────────────────────────────────────────────────────┘
```


<details>
<summary>English original</summary>

**Three Ways In**

Pick the finish line that matches what you want to be able to do, not a track number.

| Path | You'll build | Finish-line artifact | Start here |
|---|---|---|---|
| **AI Inference Engineering** | Transformer execution, GEMV/GEMM kernels, quantization, serving | A benchmark report: predicted vs. measured tok/s on a real model, with the gap explained | [Phase 3 — AI Workloads](阶段3-人工智能/Guide.md) |
| **AI Agent Harness Systems** | Sessions, tool calling, multi-agent loops, evals, observability | A production-shaped agent harness running end to end | [Agentic AI & GenAI — 42 lectures](阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/README.md) |
| **Physical Hardware Engineering** | Boards, embedded Linux, RTL, FPGA, ASIC flow | A board you brought up yourself, or an accelerator spec with real numbers | [Phase 1 — Digital Foundations](阶段1-基础知识/Guide.md) |

Each path stands on its own — you don't need the other two to get something real out of it. [The Path](#the-path) below is where all three converge into the physical-AI-chip endpoint.

---

**The Three Pillars**

*(Already picked a path above? This section is the deep-dive on why the three connect — skip to [The Path](#the-path) if you just want the phase-by-phase breakdown.)*

This roadmap is not three unrelated study tracks. It is a co-design loop. The
inference workload tells you what the silicon must accelerate, the agent
harness tells you what product behavior the runtime must support, and the
hardware platform tells you what power, memory, I/O, security, and manufacturing
constraints are real.

```
                               TARGET ARTIFACT
┌────────────────────────────────────────────────────────────────────────────┐
│                         Physical AI Agent Chip                             │
│ Linux-capable CPU + NPU/GPU/DLA + SRAM/DRAM fabric + ISP/audio/sensors     │
│ + Wi-Fi/BLE/Thread/Zigbee + secure boot/OTA + local agent runtime support  │
└────────────────────────────────────┬───────────────────────────────────────┘
                                     │
        ┌────────────────────────────┼────────────────────────────┐
        ▼                            ▼                            ▼
┌───────────────────────┐  ┌───────────────────────┐  ┌───────────────────────┐
│  AI Inference Systems │  │ Agent Harness Systems │  │ Physical HW Platform  │
├───────────────────────┤  ├───────────────────────┤  ├───────────────────────┤
│ Transformer execution │  │ Sessions and memory   │  │ Digital logic + RTL   │
│ GEMV/GEMM kernels     │  │ Tool/skill execution  │  │ CPU/NPU/GPU/DLA arch  │
│ Attention + KV cache  │  │ Gateway/RPC protocols │  │ SRAM/DRAM/DMA fabric  │
│ Quantization formats  │  │ Planner/executor loop │  │ MIPI CSI-2 + ISP      │
│ Tensor/pipeline para. │  │ Multi-agent control   │  │ Audio/GPIO/CAN/I2C    │
│ Serving schedulers    │  │ Evals and guardrails  │  │ Embedded Linux/L4T    │
│ CUDA/Triton kernels   │  │ Telemetry and tracing │  │ ESP32-class wireless  │
│ Roofline profiling    │  │ Product update path   │  │ FPGA -> ASIC flow     │
│ Real models: Qwen etc │  │ OpenClaw/SDKs/APIs    │  │ Board -> SoC thinking │
└───────────┬───────────┘  └───────────┬───────────┘  └───────────┬───────────┘
            │                          │                          │
            └──────────────┬───────────┴───────────┬──────────────┘
                           ▼                       ▼
        ┌────────────────────────────────────────────────────────────────┐
        │ Cross-layer contracts you learn to write                       │
        ├────────────────────────────────────────────────────────────────┤
        │ Workload contract: tokens/s, TTFT, context length, KV memory,  │
        │ batch shape, precision, latency tail, power budget.            │
        │ Runtime contract: APIs, cancellation, scheduling, security,    │
        │ observability, OTA, failure recovery, agent state persistence. │
        │ Hardware contract: MAC array, SRAM size, DMA plan, interconnect│
        │ bandwidth, radio wake events, camera/audio I/O, boot chain.    │
        └────────────────────────────────────────────────────────────────┘

                              CONVERGENCE PATH
┌────────────────────────────────────────────────────────────────────────────┐
│ Jetson-class AI subsystem        -> NPU/GPU/DLA/CPU compute tile           │
│ ESP32-class connectivity         -> Wi-Fi/BLE/Thread/Zigbee radio block    │
│ Camera, audio, and sensors       -> MIPI CSI-2, ISP, codecs, low-power I/O │
│ Agent runtime and serving stack  -> scheduler, memory manager, telemetry   │
│ FPGA/RTL/HLS/MLIR experiments    -> accelerator spec and tape-out target   │
└────────────────────────────────────────────────────────────────────────────┘
```

</details>

### 1. AI 推理工程
你的芯片将要运行的工作负载。这一支柱自底向上讲授 Transformer 的执行：tokenization、embedding、QKV projection、RoPE、attention、MLP、sampling、KV cache 增长、量化、批处理与 serving。你会明白为什么 decode（逐 token 生成阶段）常常受内存带宽约束，为什么 prefill（首字前的整段计算）与 decode 表现不同，以及 GEMV/GEMM kernel、CUDA Graphs、FlashAttention、paged attention、张量并行与 roofline（性能上界模型）分析如何改变系统。

**本支柱的产出：** benchmark 报告、kernel 实验、模型内存预算、量化选型，以及精确到足以驱动加速器架构的工作负载契约。

### 2. AI Agent Harness 系统
位于你芯片*之上*的软件栈。这一支柱涵盖 agentic runtime、会话模型、网关 RPC、工具调用、skills、多 agent 循环、RAG（检索增强生成）、评测、可观测性、策略控制与产品更新流程。物理 AI 芯片不只是跑 matmul。它跑的是面向用户的循环，涉及状态、超时、取消、工具失败、网络事件、传感器中断与安全约束。

**本支柱的产出：** 一套生产级 agent harness（agent 运行时框架），具备清晰的 runtime 接口、遥测、评测、工具边界、调度需求，以及你的芯片必须支持的运行行为。

### 3. 物理硬件工程
底座本身。这一支柱从数字设计、计算机体系结构、C/C++ 系统开发、嵌入式 Linux、Jetson Orin、自定义载板、L4T、TensorRT/DLA（深度学习加速器）、ESP32、OpenThread、Zigbee、ESP-Hosted、传感器、camera bring-up（上电点亮/调通）、音频、电源、散热、合规与制造起步。随后转向 FPGA、HLS（高层次综合）、MLIR/编译器工作、RTL、SoC 架构与 AI 芯片设计。

**本支柱的产出：** 可工作的板卡、bring-up 日志、Linux 镜像、无线集成、传感器流水线、FPGA/RTL 原型，以及一份可信的芯片规格，涵盖计算、内存、I/O、安全与制造约束。

| 设计问题 | 推理支柱的回答 | Agent 支柱的回答 | 硬件支柱的回答 |
|---|---|---|---|
| 芯片必须多快？ | TTFT、tok/s、批形状、上下文长度 | 用户可见延迟、工具循环时序 | MACs、SRAM、DRAM 带宽、时钟 |
| 多少内存才够？ | 权重、KV cache、activations、量化 | 会话状态、工具缓冲区、日志 | SRAM、LPDDR、DMA、缓存层级 |
| 它如何与外界通信？ | 流式推理、多模态输入 | 事件、RPC、工具、按需唤醒 | Wi-Fi/BLE/Thread/Zigbee、MIPI、音频 |
| 它如何安全地出货？ | 可复现的 benchmark、模型更新 | 评测、策略、可观测性、回滚 | 安全启动、OTA、合规、测试 |

**为什么是这种组合？** 没有 runtime 的芯片是一块砖。没有 agent 栈的 runtime 只是一个 benchmark。没有推理成本纪律的 agent 栈只是一个 demo。无法通过无线、摄像头、音频与传感器接口与物理世界通信的推理加速器，是别人不得不替你集成的协处理器。这三大支柱，就是你打造一颗**能在真实物理产品中出货**的芯片的方式：工作负载、runtime、射频、传感器、板卡、编译器与硅片从一开始就对齐。

---

## 最终你会得到什么

这份路线图的用意，写成一份检查清单：

- [ ] 你能拿到一个 Transformer 模型，从第一性原理出发预测它在给定芯片上的 decode tok/s，并解释它在哪里达不到 roofline。
- [ ] 你能为 Qwen 级模型手工调优 CUDA/kernel 路径 —— fused QKV、fused gate+up、CUDA Graphs、INT8 KV、投机解码 —— 并给出调优前后的 benchmark。
- [ ] 你能端到端跑起一套生产级 agent harness：网关、会话、skills、工具调用、多 agent 监管、可观测性看板，全套。
- [ ] 你能拿到一个 Jetson 模块，为它设计载板，bring-up 自定义 L4T，量产烧录，并按 FCC/CE 要求出货产品。
- [ ] 你能通过 SPI bring-up 一个 ESP32 无线协处理器，把它作为 Wi-Fi/BLE/Thread/Zigbee 射频暴露给 Linux 主机，并集成进同一个产品。
- [ ] 你能写 RTL，在真实 FPGA 上驱动时序收敛，并通过 HLS 或自定义 MLIR dialect 下移一个小的 Transformer block。
- [ ] 你能为**物理 AI agent 芯片**写出架构规格 —— 单颗裸片上包含一个 NPU 分块（边缘功耗下做 Qwen 级 decode）、一个无线子系统（Wi-Fi 6/BLE 5/Thread/Zigbee）、MIPI CSI-2 摄像头输入、ISP（图像信号处理器）、音频 I/O，以及一个能跑 Linux 的 CPU —— 并给出分块尺寸、SRAM 预算、MAC 阵列、DMA、射频集成与编译器/runtime 接口的真实数字。

最后一条就是目标。前六条的存在，是为了让它成真。

---


<details>
<summary>English original</summary>

**1. AI Inference Engineering**
The workload your chip will run. This pillar teaches transformer execution from
the bottom up: tokenization, embeddings, QKV projection, RoPE, attention, MLP,
sampling, KV cache growth, quantization, batching, and serving. You learn why
decode is often memory-bandwidth bound, why prefill and decode behave
differently, and how GEMV/GEMM kernels, CUDA Graphs, FlashAttention, paged
attention, tensor parallelism, and roofline analysis change the system.

**Output of this pillar:** benchmark reports, kernel experiments, model-memory
budgets, quantization choices, and workload contracts precise enough to drive an
accelerator architecture.

**2. AI Agent Harness Systems**
The software stack that lives *above* your chip. This pillar covers agentic
runtimes, session models, gateway RPCs, tool calling, skills, multi-agent loops,
RAG, evals, observability, policy controls, and product update flows. A physical
AI chip does not just run matmuls. It runs user-facing loops with state,
timeouts, cancellations, tool failures, network events, sensor interrupts, and
safety constraints.

**Output of this pillar:** a production-style agent harness with clear runtime
interfaces, telemetry, evals, tool boundaries, scheduling requirements, and the
operational behavior your chip must support.

**3. Physical Hardware Engineering**
The substrate itself. This pillar starts with digital design, computer
architecture, C/C++ systems work, embedded Linux, Jetson Orin, custom carrier
boards, L4T, TensorRT/DLA, ESP32, OpenThread, Zigbee, ESP-Hosted, sensors,
camera bring-up, audio, power, thermal, compliance, and manufacturing. Then it
moves toward FPGA, HLS, MLIR/compiler work, RTL, SoC architecture, and AI chip
design.

**Output of this pillar:** working boards, bring-up logs, Linux images, wireless
integration, sensor pipelines, FPGA/RTL prototypes, and a credible chip spec
with compute, memory, I/O, security, and manufacturing constraints.

| Design question | Inference pillar answers | Agent pillar answers | Hardware pillar answers |
|---|---|---|---|
| How fast must the chip be? | TTFT, tok/s, batch shape, context length | User-visible latency, tool-loop timing | MACs, SRAM, DRAM bandwidth, clocks |
| How much memory is enough? | Weights, KV cache, activations, quantization | Session state, tool buffers, logs | SRAM, LPDDR, DMA, cache hierarchy |
| How does it talk to the world? | Streaming inference, multimodal inputs | Events, RPC, tools, wake-on-demand | Wi-Fi/BLE/Thread/Zigbee, MIPI, audio |
| How does it ship safely? | Reproducible benchmarks, model updates | Evals, policy, observability, rollback | Secure boot, OTA, compliance, test |

**Why the combination?** A chip without a runtime is a brick. A runtime without
an agent stack is a benchmark. An agent stack without inference cost discipline
is a demo. An inference accelerator that cannot talk to the physical world over
wireless, camera, audio, and sensor interfaces is a coprocessor someone else has
to integrate. The three pillars are how you build a chip that **ships in a real
physical product**: workload, runtime, radio, sensors, board, compiler, and
silicon aligned from the start.

---

**What You'll Have at the End**

The reason for this roadmap, written as a checklist:

- [ ] You can take a transformer model, predict its decode tok/s on a given chip from first principles, and explain where it falls short of the roofline.
- [ ] You can hand-tune a CUDA/kernel path for a Qwen-class model — fused QKV, fused gate+up, CUDA Graphs, INT8 KV, speculative decoding — and quote a before/after benchmark.
- [ ] You can run a production agent harness end-to-end: gateway, sessions, skills, tool calls, multi-agent supervision, observability dashboards, the lot.
- [ ] You can take a Jetson module, design a carrier board for it, bring up custom L4T, flash it in volume, and ship a product against FCC/CE.
- [ ] You can bring up an ESP32 wireless coprocessor over SPI, expose it to a Linux host as a Wi-Fi/BLE/Thread/Zigbee radio, and integrate it into the same product.
- [ ] You can write RTL, drive timing closure on a real FPGA, and lower a small transformer block through HLS or a custom MLIR dialect.
- [ ] You can write the architecture spec for a **physical AI agent chip** — one die containing an NPU tile (Qwen-class decode at edge power), a wireless subsystem (Wi-Fi 6/BLE 5/Thread/Zigbee), MIPI CSI-2 camera input, ISP, audio I/O, and a Linux-capable CPU — with realistic numbers for tile size, SRAM budget, MAC array, DMA, RF integration, and compiler/runtime interface.

That last bullet is the goal. The first six exist to make it real.

---

</details>

## AI 芯片栈

本路线图中的一切内容都映射到一个 8 层栈上。重点不是记住各层——而是理解一层的决策如何波及到其他层。

![AI 芯片栈示意图](Assets/images/ai-chip-stack.png)

设计芯片时，**每一**层既是约束也是自由度。路线图教你读懂整列。

---

## 路径

五个阶段。前四个是基础；第五个是三大支柱汇聚之处。

### [阶段 1 — 数字基础](阶段1-基础知识/Guide.md) *(硬件支柱)*
*硬件的语言。逻辑门 → GPU 代码。*

| 模块 | 你将学到什么 |
|--------|------------------|
| [数字设计与 HDL](阶段1-基础知识/01-数字设计与HDL/Guide.md) | Verilog/SystemVerilog、仿真，后续用来编写加速器的语言 |
| [计算机体系结构](阶段1-基础知识/02-计算机体系结构与硬件/Guide.md) | CPU、GPU、缓存、内存层次结构——芯片背后的心智模型 |
| [操作系统](阶段1-基础知识/03-操作系统/Guide.md) | 进程、驱动、调度——芯片的主机实际做什么 |
| [C++ 与并行计算](阶段1-基础知识/04-C++与并行计算/Guide.md) | SIMD、OpenMP、**CUDA**、ROCm、OpenCL/SYCL |

### [阶段 2 — 嵌入式系统](阶段2-嵌入式系统/Guide.md) *(硬件支柱)*
*上手真实硬件。MCU、传感器、嵌入式 Linux。*

| 模块 | 你将学到什么 |
|--------|------------------|
| [原理图与 PCB 设计](阶段2-嵌入式系统/01-原理图与PCB设计/Guide.md) | 阅读原理图，设计载板 |
| [嵌入式软件](阶段2-嵌入式系统/02-嵌入式软件/Guide.md) | Cortex-M、FreeRTOS、SPI/I²C/CAN、IoT（OpenThread、Zigbee） |
| [嵌入式 Linux](阶段2-嵌入式系统/03-嵌入式Linux/Guide.md) | Yocto、PetaLinux、驱动 bring-up（上电点亮/调通） |
| [产品设计](阶段2-嵌入式系统/04-产品设计/Guide.md) | 从原型到可交付产品 |

### [阶段 3 — AI 工作负载](阶段3-人工智能/Guide.md) *(推理与 agent 支柱在此开始)*
*理解芯片必须服务的工作负载。核心 + 两个方向。*

**核心（所有人）：**
- [神经网络](阶段3-人工智能/01-神经网络/Guide.md) — 反向传播、CNN、从第一性原理理解 Transformer
- [**Transformer 基础**](阶段3-人工智能/01-神经网络/01-Transformer基础/Lecture-01.md) — 后续每节推理课的前置要求
- [深度学习框架](阶段3-人工智能/02-深度学习框架/Guide.md) — micrograd → PyTorch → tinygrad

**方向 A — 硬件与边缘 AI：** 计算机视觉、传感器融合、语音 AI、边缘 AI 与优化。为阶段 4B 和阶段 5C 提供基础。

**方向 B — Agentic AI 与 ML 工程：** [42 讲](阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/README.md)，涵盖 agent harness（agent 运行时框架）、LangGraph、多 agent 系统、RAG（检索增强生成）、评估、生产 runtime 纪律、OpenClaw、OpenAI Agents SDK、安全，外加一门 [Qwen3.5-4B-Base Unsloth 微调课程](阶段3-人工智能/04-智能体AI与ML工程/03-LLM应用开发/01-Qwen3-5-4B-Unsloth微调/Guide.md)。这是 **agent harness 支柱的主要形态**——如果你的目标是芯片 + runtime + harness 这条线，请按顺序阅读。

### 阶段 4 — 部署与编译 *(三大支柱在此共存)*
*将 AI 落地到真实芯片。三个专门方向。*

| 方向 | 重点 | 支柱 |
|-------|-------|--------|
| [**A — Xilinx FPGA**](阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md) | Vivado、Zynq MPSoC、HLS、驱动开发、视频流水线 | 硬件 |
| [**B — NVIDIA Jetson**](阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md) | Orin 平台、定制载板、L4T、OTA、TensorRT/DLA（深度学习加速器） | 硬件 + 推理 |
| [**C — DL 推理优化**](阶段4-路线C-深度学习推理优化/Guide.md) | MLIR、TVM、Triton、kernel 工程、量化、runtime | 推理 |

你不必三个都做。但要进入芯片设计，你需要足够的 **A** 来编写 RTL，足够的 **B** 来了解推理平台长什么样，以及足够的 **C** 来了解编译器将如何面向你的芯片。


<details>
<summary>English original</summary>

**The AI Chip Stack**

Everything in this roadmap maps onto an 8-layer stack. The point isn't to memorize layers — it's to understand how decisions in one layer ripple through the others.

![AI Chip Stack Diagram](Assets/images/ai-chip-stack.png)

When you're designing a chip, **every** layer is a constraint and a degree of freedom. The roadmap teaches you to read the whole column.

---

**The Path**

Five phases. The first four are foundation; the fifth is where the three pillars converge.

**[Phase 1 — Digital Foundations](阶段1-基础知识/Guide.md) *(Hardware pillar)***
*The language of hardware. Logic gates → GPU code.*

| Module | What you'll learn |
|--------|------------------|
| [Digital Design & HDL](阶段1-基础知识/01-数字设计与HDL/Guide.md) | Verilog/SystemVerilog, simulation, the language you'll later write your accelerator in |
| [Computer Architecture](阶段1-基础知识/02-计算机体系结构与硬件/Guide.md) | CPUs, GPUs, caches, memory hierarchies — the mental model behind your chip |
| [Operating Systems](阶段1-基础知识/03-操作系统/Guide.md) | Processes, drivers, scheduling — what your chip's host actually does |
| [C++ & Parallel Computing](阶段1-基础知识/04-C++与并行计算/Guide.md) | SIMD, OpenMP, **CUDA**, ROCm, OpenCL/SYCL |

**[Phase 2 — Embedded Systems](阶段2-嵌入式系统/Guide.md) *(Hardware pillar)***
*Get hands-on with real hardware. MCUs, sensors, embedded Linux.*

| Module | What you'll learn |
|--------|------------------|
| [Schematic & PCB Design](阶段2-嵌入式系统/01-原理图与PCB设计/Guide.md) | Read schematics, design carrier boards |
| [Embedded Software](阶段2-嵌入式系统/02-嵌入式软件/Guide.md) | Cortex-M, FreeRTOS, SPI/I²C/CAN, IoT (OpenThread, Zigbee) |
| [Embedded Linux](阶段2-嵌入式系统/03-嵌入式Linux/Guide.md) | Yocto, PetaLinux, driver bring-up |
| [Product Design](阶段2-嵌入式系统/04-产品设计/Guide.md) | Going from prototype to shippable product |

**[Phase 3 — AI Workloads](阶段3-人工智能/Guide.md) *(Inference & Agent pillars start here)***
*Understand the workloads your chip must serve. Core + two tracks.*

**Core (everyone):**
- [Neural Networks](阶段3-人工智能/01-神经网络/Guide.md) — backprop, CNNs, transformers from first principles
- [**Transformer Fundamentals**](阶段3-人工智能/01-神经网络/01-Transformer基础/Lecture-01.md) — the prerequisite for every inference lecture downstream
- [Deep Learning Frameworks](阶段3-人工智能/02-深度学习框架/Guide.md) — micrograd → PyTorch → tinygrad

**Track A — Hardware & Edge AI:** Computer vision, sensor fusion, Voice AI, Edge AI & optimization. Feeds Phase 4B and Phase 5C.

**Track B — Agentic AI & ML Engineering:** [42 lectures](阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/README.md) on agent harnesses, LangGraph, multi-agent systems, RAG, evaluation, production runtime discipline, OpenClaw, OpenAI Agents SDK, security, plus a [Qwen3.5-4B-Base Unsloth fine-tuning course](阶段3-人工智能/04-智能体AI与ML工程/03-LLM应用开发/01-Qwen3-5-4B-Unsloth微调/Guide.md). This is the **agent harness pillar in its primary form** — read in order if your destination is the chip + runtime + harness story.

**Phase 4 — Deployment & Compilation *(All three pillars co-exist here)***
*Take AI to real silicon. Three specialized tracks.*

| Track | Focus | Pillar |
|-------|-------|--------|
| [**A — Xilinx FPGA**](阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md) | Vivado, Zynq MPSoC, HLS, driver dev, video pipeline | Hardware |
| [**B — NVIDIA Jetson**](阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md) | Orin platform, custom carrier, L4T, OTA, TensorRT/DLA | Hardware + Inference |
| [**C — DL Inference Optimization**](阶段4-路线C-深度学习推理优化/Guide.md) | MLIR, TVM, Triton, kernel engineering, quantization, runtimes | Inference |

You don't have to do all three. But to land at chip design, you want enough of **A** to write RTL, enough of **B** to know what an inference platform looks like, and enough of **C** to know how a compiler will target your chip.

</details>

### [阶段 5 — 专业化与收敛](阶段5-高级专题与专精/Guide.md)
*三大支柱在此汇聚。专业化方向，加上芯片设计这一终点。*

| 方向 | 你将专精的内容 | 支柱 |
|-------|---------------------------|-----------|
| [**A — GPU 基础设施**](阶段5-高级专题与专精/01-方向A-GPU基础设施/Guide.md) | 多 GPU、NVLink、NCCL、AMD ROCm/HIP、MI300X | 推理 |
| [**B — 高性能计算（CUDA-X）**](阶段5-高级专题与专精/02-方向B-高性能计算/Guide.md) | cuBLAS、cuDNN、NVSHMEM、40+ 个库 | 推理 |
| [**C — 边缘 AI**](阶段5-高级专题与专精/03-方向C-边缘AI/Guide.md) | Holoscan、[边缘 LLM 推理内幕](阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md)、[**Qwen 推理优化（6 讲系列）**](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/README.md)、AI 驱动的无线 | 推理 |
| [**D — 机器人**](阶段5-高级专题与专精/04-方向D-机器人/Guide.md) | ROS 2、Nav2、运动规划、集群 | 硬件 + 推理 |
| [**E — 自动驾驶汽车**](阶段5-高级专题与专精/05-方向E-自动驾驶/Guide.md) | openpilot、BEV（鸟瞰图）感知、ISO 26262、TRACE32 调试 | 硬件 + 推理 |
| [**F — AI 芯片设计**](阶段5-高级专题与专精/06-方向F-AI芯片设计/Guide.md) | **终点。** 脉动阵列、dataflow 架构、tinygrad↔硬件、RISC-V AI 加速器设计、ASIC 流程 — 以及那个集成问题：如何把 NPU、一个 ESP32 级别的射频、一个 ISP、一个 Linux CPU 放到同一颗 die 上？ | **全部三大支柱** |
| [**G — ML 系统工程**](阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md) | 训练系统、推理 runtime、GPU 调度、分布式推理服务、编译器/runtime 工作、可观测性 | 推理 + 基础设施 |

**标志性路径：** 阶段 1 → 阶段 2 → 阶段 3（Core + 方向 B）→ 阶段 4（选读）→ 阶段 5C + 阶段 5F。

**MLSys 路径：** 阶段 1 §3/§4 → 阶段 3 Core → 阶段 4B/4C → 阶段 5A/B/C → 阶段 5G。

---


<details>
<summary>English original</summary>

**[Phase 5 — Specialization & Convergence](阶段5-高级专题与专精/Guide.md)**
*The three pillars converge here. Specialization tracks plus the chip-design endpoint.*

| Track | What you'll specialize in | Pillar(s) |
|-------|---------------------------|-----------|
| [**A — GPU Infrastructure**](阶段5-高级专题与专精/01-方向A-GPU基础设施/Guide.md) | Multi-GPU, NVLink, NCCL, AMD ROCm/HIP, MI300X | Inference |
| [**B — HPC (CUDA-X)**](阶段5-高级专题与专精/02-方向B-高性能计算/Guide.md) | cuBLAS, cuDNN, NVSHMEM, 40+ libraries | Inference |
| [**C — Edge AI**](阶段5-高级专题与专精/03-方向C-边缘AI/Guide.md) | Holoscan, [Edge LLM Inference Internals](阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md), [**Qwen Inference Optimization (6-lecture series)**](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/README.md), AI-driven wireless | Inference |
| [**D — Robotics**](阶段5-高级专题与专精/04-方向D-机器人/Guide.md) | ROS 2, Nav2, motion planning, swarm | Hardware + Inference |
| [**E — Autonomous Vehicles**](阶段5-高级专题与专精/05-方向E-自动驾驶/Guide.md) | openpilot, BEV perception, ISO 26262, TRACE32 debug | Hardware + Inference |
| [**F — AI Chip Design**](阶段5-高级专题与专精/06-方向F-AI芯片设计/Guide.md) | **The endpoint.** Systolic arrays, dataflow architectures, tinygrad↔hardware, RISC-V AI accelerator design, ASIC flow — and the integration question: how do you put an NPU, an ESP32-class radio, an ISP, and a Linux CPU on one die? | **All three** |
| [**G — ML Systems Engineering**](阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md) | Training systems, inference runtimes, GPU scheduling, distributed serving, compiler/runtime work, observability | Inference + Infrastructure |

**The signature path:** Phase 1 → Phase 2 → Phase 3 (Core + Track B) → Phase 4 (selected) → Phase 5C + Phase 5F.

**The MLSys path:** Phase 1 §3/§4 → Phase 3 Core → Phase 4B/4C → Phase 5A/B/C → Phase 5G.

---

</details>

## 精选推理讲座

最深入、最新的技术内容都在这些阶段 5 讲座里 —— 把它们当作一条完整弧线来读：

<div class="lecture-map" markdown>

| # | 讲座 | 讲授内容 |
|---|---------|-----------------|
| 1 | [边缘 LLM 推理内部机制](阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) | GEMV vs GEMM roofline（性能上界模型），K-quants，KV-cache 数学，Jetson `nvpmodel`/`jetson_clocks` 诊断 |
| 2 | [Qwen 架构深入解析](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-01.md) | Qwen3-4B 与 Qwen2.5-72B 并排对照，GQA，RoPE-NeoX，SwiGLU，完整的 `config.json` → 张量形状推导 |
| 3 | [把 Qwen3-4B 量化到 Q4](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md) | Q4_K_M vs AWQ vs GPTQ，为什么 V 和 FFN-down 会被升级，校准，GGUF 布局 |
| 4 | [Jetson 上的 decode 优化（逐 token 生成阶段）](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-03.md) | 0.2 → 30 tok/s 的阶梯，融合 QKV/gate-up，CUDA Graphs，INT8 KV，投机解码 |
| 5 | [Qwen2.5-72B 多 GPU FP16](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-04.md) | TP=8 切分，NCCL 热路径，paged attention，YaRN，runtime recipe |
| 6 | [跨模型与生产推理服务](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-05.md) | 投机解码配对，边缘/云端路由，可观测性，容量规划 |
| 7 | [批处理 GEMM vs 普通 GEMM](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-06.md) | cuBLAS API 形式，列主序的舞蹈，张量核心，位精确可复现性 |
| 8 | [AI 推理工程师 2026 —— 特别课程](阶段5-高级专题与专精/07-方向G-ML系统工程/01-AI推理工程师2026/README.md) | 以 27 讲串起的完整 2026 生产推理技术栈：dense → MoE（混合专家模型），Hopper → Blackwell，FP16 → FP8 → FP4，vLLM / SGLang / TensorRT-LLM，张量并行，集合通信/通信内部机制，分离式 prefill（首字前的整段计算）/decode，roofline |
| 9 | [优化一个真实引擎 —— 第 4 部分案例研究](阶段5-高级专题与专精/07-方向G-ML系统工程/01-AI推理工程师2026/04-真实引擎优化/README.md) | 10 讲，讲述一段有实测数据的优化历程：Kimi K3（2.8T，896 个专家）跑在 8× H200 上，在 128k 下跨约 96 个 PR 达到 **1.01 → 60.17 tok/s**。构建一个无法作假的 benchmark，诊断瓶颈上限，launch geometry，融合，split-context attention，专家分片 + Amdahl，CUDA graphs，批处理 prefill，以及让 benchmark *更优* 的 bug |

</div>

如果你是新手，就按顺序读。如果不是，就跳到能解决你当前问题的那一篇。

---

## 如何使用这份路线图

不要把它当成书来读。把它当作一套 **边构建边测量的课程**。

对每一个模块：

1. 读理论。
2. 把子系统搭出来，或把方法实现出来。
3. 测量某个指标 —— 延迟、吞吐、occupancy、带宽、功耗、准确率、面积、困惑度。
4. 交付一个可复用的产物（benchmark、kernel、板子、dashboard、RTL 模块、评测报告）。

每一个产物都是你将要设计的那颗芯片里的一块砖。

动手之前，先定三件事：

1. **你从技术栈的哪一层切入。**（见上文“Who This Is For”。）
2. **你实际能用上什么硬件。** Jetson Orin Nano 是最便宜的端到端推理目标；一块 RTX 或租来的 L40S/H100 能覆盖大部分数据中心路径；一块 Xilinx Zynq 开发板覆盖 FPGA；一块 ESP32 + 传感器扩展板覆盖嵌入式。
3. **你如何跟踪产出。** 一个 notebook、一个 benchmark 仓库、一份项目日志 —— 任何你真正会用起来的系统。

---


<details>
<summary>English original</summary>

**Featured Inference Lectures**

The deepest, most current technical content lives in these Phase 5 lectures — read them as a single arc:

<div class="lecture-map" markdown>

| # | Lecture | What it teaches |
|---|---------|-----------------|
| 1 | [Edge LLM Inference Internals](阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) | GEMV vs GEMM rooflines, K-quants, KV-cache math, Jetson `nvpmodel`/`jetson_clocks` diagnostics |
| 2 | [Qwen Architecture Deep Dive](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-01.md) | Qwen3-4B and Qwen2.5-72B side by side, GQA, RoPE-NeoX, SwiGLU, full `config.json` → tensor-shape derivation |
| 3 | [Quantizing Qwen3-4B to Q4](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md) | Q4_K_M vs AWQ vs GPTQ, why V and FFN-down get upgraded, calibration, GGUF layout |
| 4 | [Decode Optimization on Jetson](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-03.md) | 0.2 → 30 tok/s ladder, fused QKV/gate-up, CUDA Graphs, INT8 KV, speculative decoding |
| 5 | [Qwen2.5-72B Multi-GPU FP16](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-04.md) | TP=8 partitioning, NCCL hot path, paged attention, YaRN, runtime recipes |
| 6 | [Cross-Model & Production Serving](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-05.md) | Speculative decoding pairings, edge/cloud routing, observability, capacity planning |
| 7 | [Batched GEMM vs Normal GEMM](阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/Lecture-06.md) | cuBLAS API forms, column-major dance, tensor cores, bit-exact reproducibility |
| 8 | [AI Inference Engineer 2026 — special course](阶段5-高级专题与专精/07-方向G-ML系统工程/01-AI推理工程师2026/README.md) | The full 2026 production inference stack as a 27-lecture arc: dense → MoE, Hopper → Blackwell, FP16 → FP8 → FP4, vLLM / SGLang / TensorRT-LLM, tensor parallelism, collective/communication internals, disaggregated prefill/decode, rooflines |
| 9 | [Optimizing a Real Engine — Part 4 case study](阶段5-高级专题与专精/07-方向G-ML系统工程/01-AI推理工程师2026/04-真实引擎优化/README.md) | 10 lectures on a measured optimization history: Kimi K3 (2.8T, 896 experts) on 8× H200, **1.01 → 60.17 tok/s** at 128k across ~96 PRs. Building a benchmark that cannot be gamed, diagnosing the binding ceiling, launch geometry, fusion, split-context attention, expert sharding + Amdahl, CUDA graphs, batched prefill, and bugs that make the benchmark *better* |

</div>

Read them in order if you're new. Skip to whichever solves your current problem if you're not.

---

**How to Use This Roadmap**

Don't read this like a book. Treat it like a **build-and-measure curriculum**.

For every block:

1. Read the theory.
2. Build the subsystem or implement the technique.
3. Measure something — latency, throughput, occupancy, bandwidth, power, accuracy, area, perplexity.
4. Ship one reusable artifact (benchmark, kernel, board, dashboard, RTL block, eval report).

Each artifact is a brick in the chip you're going to design.

Before you start, decide three things:

1. **Where you're entering the stack.** (See "Who This Is For" above.)
2. **What hardware you can actually use.** Jetson Orin Nano is the cheapest end-to-end inference target; an RTX or rented L40S/H100 covers most of the datacenter path; a Xilinx Zynq dev board covers FPGA; an ESP32 + sensor breakout covers embedded.
3. **How you'll track outputs.** A notebook, a benchmarks repo, a project log — any system you actually use.

---

</details>

## 课程质量标准

这条路线图中每个严肃的模块都应以证据收尾，而不是凭感觉。

每个课程模块都用这套标准：

| 步骤 | 做什么 | 证据 |
|------|------------|----------|
| 理解 | 学懂概念，理解它在整个技术栈中为什么重要 | 简短的设计说明或图 |
| 构建 | 实现子系统、kernel、模型路径、驱动、板级流程或 runtime 特性 | 代码、RTL、配置、原理图或构建脚本 |
| 度量 | 收集真实数据 | 延迟、吞吐、内存、功耗、时序、利用率、准确率、面积或启动时间 |
| 调试 | 解释至少一种失效模式 | 日志、波形、profiler trace、ILA 抓取或根因说明 |
| 交付 | 把成果打包以便评审 | README、命令、原始结果与最终报告 |

弱完成：

```text
I read about CUDA, TensorRT, and FPGAs.
```

强完成：

```text
I built a TensorRT INT8 benchmark on Orin Nano, captured latency/RAM/power,
compared it to FP16, and explained why one layer stayed memory-bound.
```

这条路线图刻意做得很宽，但完成标准很窄：做出真实的东西，测量它，并解释取舍。

---

## 参考项目

这些项目是为了让你去研究，而不只是读一读：

| 项目 | 为什么收录 |
|---------|---------------|
| [**jetson-llm-runtime**](项目/01-Jetson-LLM运行时/README.md) &nbsp;·&nbsp; [`GeniePod/genie-ai-runtime` v1.0.0](https://github.com/GeniePod/genie-ai-runtime) | 定制的 Jetson LLM 推理 runtime — 每个 GEMV/GEMM kernel、KV cache、paged-attention 路径、构建流程。本文件夹中的脚手架已成长为 `GeniePod/genie-ai-runtime` 处的生产 runtime：prefill（首字前的整段计算）38 tok/s，在 Orin Nano Super 8 GB 上相比 `llama-bench` 提升 +115 %，tensor-core MMQ，持久化 KV，默认 INT8 KV，OpenAI 形态的 HTTP 服务器。推理支柱的代码落地。 |
| [**llm-inference-viz**](https://github.com/ai-hpc/llm-inference-viz) | 稠密 decoder-only LLM 推理的交互式 3D 可视化 — 走一遍前向传播，观察每个阶段在 H200 roofline（性能上界模型）上落在内存受限还是算力受限，并用张量并行把模型切分到多块 GPU 上。把推理的心智模型变得可见；AI Inference Engineer 2026 课程的配套项目。 |
| [**jetson-esp-hosted**](https://github.com/ai-hpc/jetson-esp-hosted) | 经 Jetson 验证的 ESP-Hosted 分支，用于 SPI/Wi-Fi/BLE bring-up（上电点亮/调通）。嵌入式支柱的代码落地。 |
| [**tinygrad**](https://github.com/tinygrad/tinygrad) | 约 10 K 行的 ML 框架。在一个仓库里读透框架 → 编译器 → kernel → 后端的最干净的地方。 |
| [**openpilot**](https://github.com/commaai/openpilot) | 生产级 ADAS 栈。在一块板子上实现端到端的感知、ML 与嵌入式软件。 |

---

## 这条路能通向的目标岗位

这条路线图是刻意做成全栈的，但沿途会产出几个高薪的专业岗位：

| 岗位 | 关键阶段 |
|------|-----------|
| **AI 推理工程师** | 3 + 4C + 5A/B/C |
| **ML 系统工程师** | 1 + 3 + 4B/4C + 5A/B/C/G |
| **AI 编译器工程师** | 1 + 4C + 5B |
| **边缘 AI 工程师** | 3A + 4B + 5C |
| **GPU Runtime / Kernel 工程师** | 1 + 4B + 5A |
| **Agentic AI / Agent Harness（agent 运行时框架）工程师** | 3B（完整系列讲座）+ 5C |
| **嵌入式 / 固件工程师** | 1 + 2 + 4B |
| **自动驾驶汽车工程师** | 3A + 4B + 5E |
| **RTL / FPGA 设计工程师** | 1 + 4A |
| **AI 加速器架构师** | 1 + 4A + 5F |
| **物理 AI 芯片架构师** | 完整路径 — Jetson + ESP32 融合成一颗 SoC；芯片设计的终点 |

→ 薪资数据、23 个细分岗位、远程比例和招聘信号，见 [**Roles & Market Analysis**](角色与市场分析.md)。

---

## 为什么要有这条路线图

一颗**物理 AI 芯片** — Jetson 级别的算力大脑 + ESP32 级别的射频 + 传感器 + 单 die 上的 Linux — 是小团队能尝试的最苛刻的工程项目之一。它需要：

- **工作负载的真相。** 不知道 Qwen 级别的 decode（逐 token 生成阶段）会向你抛出多少 bytes-per-token，就无法设计 NPU 分块或存储层次。这就是推理支柱。
- **系统的真相。** 你的芯片要承载 runtime，而这些 runtime 要在电池供电的产品里承载 agent harness。访问模式错了（batch=1 聊天 vs 常开唤醒词 vs 长上下文检索）、无线唤醒策略错了、boot ROM 错了，那你就已经出货了一颗错的芯片。这就是 agent harness 支柱。
- **工程的真相 — 两半都要。** 硅片不在乎你的意图。RTL、时序、功耗、嵌入式软件、板级、天线、FCC、制造 — 没有捷径。你需要 AI 计算侧（Jetson 栈）*和*无线侧（ESP32 栈）在同一颗 die 上*和*同一个 Linux 上。这就是硬件支柱。

大多数人只学一根支柱。有些人学两根。这条路线图是给想学全部三根、然后做出那个把 AI agent 放进真实产品里的东西的人 — 与传感器对话、与网络对话、与人对话，全都跑在一颗 SoC 上。

---


<details>
<summary>English original</summary>

**Course Quality Bar**

Every serious module in this roadmap should end with evidence, not vibes.

Use this standard for each course block:

| Step | What to do | Evidence |
|------|------------|----------|
| Understand | Learn the concept and why it matters in the stack | short design note or diagram |
| Build | Implement the subsystem, kernel, model path, driver, board flow, or runtime feature | code, RTL, config, schematic, or build script |
| Measure | Collect real numbers | latency, throughput, memory, power, timing, utilization, accuracy, area, or boot time |
| Debug | Explain at least one failure mode | log, waveform, profiler trace, ILA capture, or root-cause note |
| Ship | Package the work for review | README, commands, raw results, and final report |

Weak completion:

```text
I read about CUDA, TensorRT, and FPGAs.
```

Strong completion:

```text
I built a TensorRT INT8 benchmark on Orin Nano, captured latency/RAM/power,
compared it to FP16, and explained why one layer stayed memory-bound.
```

The roadmap is intentionally broad, but the completion standard is narrow: build something real, measure it, and explain the tradeoff.

---

**Reference Projects**

These projects exist for you to study, not just read about:

| Project | Why it's here |
|---------|---------------|
| [**jetson-llm-runtime**](项目/01-Jetson-LLM运行时/README.md) &nbsp;·&nbsp; [`GeniePod/genie-ai-runtime` v1.0.0](https://github.com/GeniePod/genie-ai-runtime) | Custom Jetson LLM inference runtime — every GEMV/GEMM kernel, KV cache, paged-attention path, build flow. The scaffold in this folder graduated into the production runtime at `GeniePod/genie-ai-runtime`: 38 tok/s prefill, +115 % vs `llama-bench` on Orin Nano Super 8 GB, tensor-core MMQ, persistent KV, INT8 KV default, OpenAI-shape HTTP server. The inference pillar in code. |
| [**llm-inference-viz**](https://github.com/ai-hpc/llm-inference-viz) | Interactive 3D visualization of dense decoder-only LLM inference — walk the forward pass, watch each stage land memory- vs compute-bound on an H200 roofline, and shard the model across GPUs with tensor parallelism. The inference mental model, made visible; companion to the AI Inference Engineer 2026 course. |
| [**jetson-esp-hosted**](https://github.com/ai-hpc/jetson-esp-hosted) | Jetson-validated ESP-Hosted fork for SPI/Wi-Fi/BLE bring-up. The embedded pillar in code. |
| [**tinygrad**](https://github.com/tinygrad/tinygrad) | ~10 K-line ML framework. The cleanest place to read framework → compiler → kernel → backend in one repo. |
| [**openpilot**](https://github.com/commaai/openpilot) | Production ADAS stack. End-to-end perception, ML, and embedded software on one board. |

---

**Target Roles This Enables**

The roadmap is full-stack on purpose, but it produces several well-paid specialist roles along the way:

| Role | Key Phases |
|------|-----------|
| **AI Inference Engineer** | 3 + 4C + 5A/B/C |
| **ML Systems Engineer** | 1 + 3 + 4B/4C + 5A/B/C/G |
| **AI Compiler Engineer** | 1 + 4C + 5B |
| **Edge AI Engineer** | 3A + 4B + 5C |
| **GPU Runtime / Kernel Engineer** | 1 + 4B + 5A |
| **Agentic AI / Agent Harness Engineer** | 3B (full lecture series) + 5C |
| **Embedded / Firmware Engineer** | 1 + 2 + 4B |
| **Autonomous Vehicles Engineer** | 3A + 4B + 5E |
| **RTL / FPGA Design Engineer** | 1 + 4A |
| **AI Accelerator Architect** | 1 + 4A + 5F |
| **Physical AI Chip Architect** | Full path — Jetson + ESP32 fused into one SoC; the chip-design endpoint |

→ See [**Roles & Market Analysis**](角色与市场分析.md) for salary data, 23 sub-roles, remote percentages, and hiring signals.

---

**Why This Roadmap Exists**

A **physical AI chip** — Jetson-class brain + ESP32-class radio + sensors + Linux on one die — is one of the most demanding engineering projects a small team can attempt. It needs:

- **Workload truth.** You can't design an NPU tile or a memory hierarchy without knowing what bytes-per-token a Qwen-class decode will throw at you. That's the inference pillar.
- **System truth.** Your chip is going to host runtimes that host agent harnesses in a battery-powered product. Wrong access pattern (batch=1 chat vs always-on wake word vs long-context retrieval), wrong wake-on-radio policy, wrong boot ROM, you've shipped the wrong chip. That's the agent harness pillar.
- **Engineering truth — both halves.** Silicon doesn't care about your intentions. RTL, timing, power, embedded software, board, antenna, FCC, manufacturing — there's no shortcut. You need the AI-compute side (Jetson stack) *and* the wireless side (ESP32 stack) on the same die *and* on the same Linux. That's the hardware pillar.

Most people learn one pillar. Some learn two. This roadmap is for the people who want to learn all three, and then build the thing that puts an AI agent in a real product — talking to sensors, talking to networks, talking to humans, off a single SoC.

---

</details>

## Star History

<a href="https://github.com/ai-hpc/ai-hardware-engineer-roadmap/stargazers">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/ai-hpc/ai-hardware-engineer-roadmap/main/Assets/star-history/star-history-dark.svg" />
    <source media="(prefers-color-scheme: light)" srcset="https://raw.githubusercontent.com/ai-hpc/ai-hardware-engineer-roadmap/main/Assets/star-history/star-history-light.svg" />
    <img alt="Star History Chart for ai-hpc/ai-hardware-engineer-roadmap" src="https://raw.githubusercontent.com/ai-hpc/ai-hardware-engineer-roadmap/main/Assets/star-history/star-history-light.svg" width="800" />
  </picture>
</a>

---

<div align="center" markdown="1">

**构建工作负载。构建 runtime。构建射频。构建芯片。交付物理 AI 芯片。**

[⭐ 给这个 repo 点 Star](https://github.com/ai-hpc/ai-hardware-engineer-roadmap)，如果你也走在这条路上 —— 它能帮下一位工程师找到它。

[<img src="https://cdn.simpleicons.org/discord/5865F2" alt="" width="14" height="14" align="absmiddle" /> 加入社区](https://discord.gg/r8DKtDzrsm) —— Inference Engineering Discord，面向端到端构建这套技术栈的工程师。

</div>


<details>
<summary>English original</summary>

**Star History**

<a href="https://github.com/ai-hpc/ai-hardware-engineer-roadmap/stargazers">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/ai-hpc/ai-hardware-engineer-roadmap/main/Assets/star-history/star-history-dark.svg" />
    <source media="(prefers-color-scheme: light)" srcset="https://raw.githubusercontent.com/ai-hpc/ai-hardware-engineer-roadmap/main/Assets/star-history/star-history-light.svg" />
    <img alt="Star History Chart for ai-hpc/ai-hardware-engineer-roadmap" src="https://raw.githubusercontent.com/ai-hpc/ai-hardware-engineer-roadmap/main/Assets/star-history/star-history-light.svg" width="800" />
  </picture>
</a>

---

<div align="center" markdown="1">

**Build the workload. Build the runtime. Build the radio. Build the silicon. Ship the physical AI chip.**

[⭐ Star this repo](https://github.com/ai-hpc/ai-hardware-engineer-roadmap) if you're on this path — it helps the next engineer find it.

[<img src="https://cdn.simpleicons.org/discord/5865F2" alt="" width="14" height="14" align="absmiddle" /> Join the Community](https://discord.gg/r8DKtDzrsm) — the Inference Engineering Discord, for engineers building this stack end to end.

</div>

</details>

---

> 原文：[`README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
