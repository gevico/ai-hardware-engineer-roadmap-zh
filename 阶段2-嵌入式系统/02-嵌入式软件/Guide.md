---
title: ARM MCU、FreeRTOS 与通信协议
description: ARM MCU、FreeRTOS 与通信协议
---

# ARM MCU、FreeRTOS 与通信协议

<div class="course-identity embedded-software" markdown="1">
<div class="course-identity__icon">MCU</div>
<div markdown="1">
<p class="course-identity__eyebrow">模块 2 · 嵌入式软件</p>
<p class="course-identity__title">围绕中断、DMA、总线、RTOS 任务与外设控制构建固件。</p>
<p class="course-identity__meta">产物：MCU/RTOS 项目 · 度量：延迟、抖动、栈、ISR 开销</p>
</div>
</div>


*在你调试**自研硬件**时，接续 [**阶段 2 第 1 节 —— 原理图绘制与 PCB 设计**](../01-原理图与PCB设计/Guide.md)；使用开发套件的学习者可以并行从这里开始。以阶段 1（数字、HDL、体系结构、OS）为基础 —— 聚焦 ARM Cortex-M 微控制器、RTOS 实践，以及连接传感器与外设的总线（SPI/UART/I2C/CAN）。*

本模块下设的专题迷你课程：

- [**IoT**](02-IoT/Guide.md)，涵盖 OpenThread 与 Zigbee 等网络协议栈
- [**Espressif**](01-Espressif/Guide.md)，涵盖 `arduino-esp32`、ESP32 系列工作流、乐鑫官方教育路径，以及通向 ESP-IDF 的桥梁

---

## 1. ARM Cortex-M 体系结构

### Cortex-M 核心变体

* **Cortex-M0/M0+、M3、M4、M7、M33/M55：** 理解该系列内部的差异 —— 指令集（Thumb、Thumb-2）、是否具备 FPU（M4/M7）、DSP 扩展（M4/M7），以及安全扩展（M23/M33/M55 上的 TrustZone-M）。
* **CMSIS（Cortex Microcontroller Software Interface Standard）：** 掌握 CMSIS-Core 做硬件抽象、CMSIS-DSP 做信号处理、CMSIS-NN 做 Cortex-M 上的神经网络推理。量产嵌入式代码与 TinyML 代码正是通过这些库来面向 ARM 硬件的。
* **ARM TrustZone-M（Cortex-M33/M55）：** 由硬件强制的安全分区 —— 安全世界与非安全世界、安全启动、可信执行环境（TEE）。与 IoT 安全和边缘 AI 部署直接相关。

### 存储系统与 MPU

* **哈佛架构 vs. 冯·诺依曼架构：** Cortex-M 采用改进型哈佛架构，指令总线与数据总线分离 —— 理解这对缓存行为和 DMA 的影响。
* **内存映射外设：** 所有外设（GPIO、定时器、UART、SPI、I2C）都通过内存地址访问 —— 寄存器就是地址。掌握这一模型才能编写底层驱动。
* **MPU（Memory Protection Unit）：** 用访问权限（读/写/执行）配置内存区域，以隔离任务、检测栈溢出、捕获错误的指针写入。是健壮量产固件的必备。
* **栈溢出检测：** 使用 MPU 保护区域与硬件栈保护，使溢出时触发可控的 HardFault，而非未定义行为。

### CPU 内部机制

* **指令集（Thumb-2）：** Cortex-M 执行 Thumb-2 —— 16 位与 32 位指令的混合。理解编译器如何面向该 ISA 生成代码，以及内联汇编在何处有用（例如 `__disable_irq()`、`__DSB()`）。
* **流水线与中断：** 3 级流水线（M0/M3）或带分支预测的 6 级流水线（M7）。理解中断延迟由什么决定，以及 `NVIC`（Nested Vectored Interrupt Controller）如何管理优先级与抢占。
* **异常模型：** HardFault、MemManage、BusFault、UsageFault —— 实现 Fault 处理程序，捕获寄存器状态并将栈 trace 写入 flash，以便事后调试。

### 面向嵌入式的 RISC-V（对比专题）

* **RISC-V ISA 与扩展：** 模块化 ISA —— RV32I 基础指令集 + M（乘法）、A（原子操作）、F/D（浮点）、C（压缩）。学习如何评估为你的应用启用哪些扩展。
* **嵌入式平台：** SiFive FE310、ESP32-C3、GD32VF103，或基于 FPGA 的软核（Zynq/Arty 上的 PicoRV32、VexRiscv）。在 RISC-V 软核上运行 FreeRTOS 是一个很好的阶段 3 衔接项目。

**资源：**
* *"The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors"* —— Joseph Yiu。Cortex-M 体系结构、NVIC 与存储系统的权威参考书。
* *"Embedded Systems: Introduction to Arm Cortex-M Microcontrollers"* —— Jonathan Valvano。面向 Cortex-M 硬件的实用 C 编程。
* CMSIS 文档（Arm/Keil）—— CMSIS-Core、CMSIS-DSP、CMSIS-NN。
* *"RISC-V Reader: An Open Architecture Atlas"* —— Patterson & Waterman。

**项目：**
* **CMSIS-NN 关键词识别：** 在 Cortex-M7 开发板上使用 CMSIS-NN 部署量化关键词识别模型。对推理周期数与 RAM 占用做 benchmark。
* **MPU 栈保护：** 在 Cortex-M4 上把一段 MPU 区域配置为栈下方的保护页。触发一次可控溢出，验证 MemManage Fault 处理程序被调用，而不是静默发生数据损坏。
* **RISC-V 软核上的 FreeRTOS：** 将 FreeRTOS 移植到 FPGA 上综合出的 VexRiscv 或 PicoRV32 核。验证任务切换与中断延迟。

---

## 2. ARM 嵌入式系统的 C 编程


<details>
<summary>English original</summary>

**ARM MCU, FreeRTOS, and Communication Protocols**

<div class="course-identity embedded-software" markdown="1">
<div class="course-identity__icon">MCU</div>
<div markdown="1">
<p class="course-identity__eyebrow">Module 2 · Embedded Software</p>
<p class="course-identity__title">Build firmware around interrupts, DMA, buses, RTOS tasks, and peripheral control.</p>
<p class="course-identity__meta">Artifact: MCU/RTOS project · Measure: latency, jitter, stack, ISR cost</p>
</div>
</div>


*Follows [**Phase 2 section 1 — Schematic Capture and PCB Design**](../01-原理图与PCB设计/Guide.md) when you are bringing up **custom hardware**; dev-kit learners can start here in parallel. Builds on Phase 1 (digital, HDL, architecture, OS) — focuses on ARM Cortex-M microcontrollers, RTOS practice, and buses (SPI/UART/I2C/CAN) that connect sensors and peripherals.*

Focused mini-courses that sit inside this module:

- [**IoT**](02-IoT/Guide.md) for networking stacks like OpenThread and Zigbee
- [**Espressif**](01-Espressif/Guide.md) for `arduino-esp32`, ESP32-family workflow, the official Espressif education path, and the bridge toward ESP-IDF

---

**1. ARM Cortex-M Architecture**

**Cortex-M Core Variants**

* **Cortex-M0/M0+, M3, M4, M7, M33/M55:** Understand differences across the family — instruction sets (Thumb, Thumb-2), presence of FPU (M4/M7), DSP extensions (M4/M7), and security extensions (TrustZone-M on M23/M33/M55).
* **CMSIS (Cortex Microcontroller Software Interface Standard):** Master CMSIS-Core for hardware abstraction, CMSIS-DSP for signal processing, CMSIS-NN for neural network inference on Cortex-M. These libraries are how production embedded and TinyML code targets ARM hardware.
* **ARM TrustZone-M (Cortex-M33/M55):** Hardware-enforced security partitioning — secure and non-secure worlds, secure boot, trusted execution environments (TEE). Directly relevant to IoT security and edge AI deployment.

**Memory System and MPU**

* **Harvard vs. Von Neumann:** Cortex-M uses a modified Harvard architecture with separate instruction and data buses — understand implications for cache behavior and DMA.
* **Memory-Mapped Peripherals:** All peripherals (GPIO, timers, UART, SPI, I2C) are accessed via memory addresses — registers are just addresses. Master this model to write low-level drivers.
* **MPU (Memory Protection Unit):** Configure memory regions with access permissions (read/write/execute) to isolate tasks, detect stack overflows, and catch errant pointer writes. Essential for robust production firmware.
* **Stack Overflow Detection:** Use MPU guard regions and hardware stack protection to trigger controlled HardFault on overflow rather than undefined behavior.

**CPU Internals**

* **Instruction Set (Thumb-2):** Cortex-M executes Thumb-2 — a mix of 16- and 32-bit instructions. Understand how the compiler targets this ISA and where inline assembly is useful (e.g., `__disable_irq()`, `__DSB()`).
* **Pipeline and Interrupts:** 3-stage pipeline (M0/M3) or 6-stage with branch prediction (M7). Understand how interrupt latency is determined and how `NVIC` (Nested Vectored Interrupt Controller) manages priority and preemption.
* **Exception Model:** HardFault, MemManage, BusFault, UsageFault — implement fault handlers that capture register state and stack trace to flash for post-mortem debugging.

**RISC-V for Embedded (Comparison Track)**

* **RISC-V ISA and Extensions:** Modular ISA — RV32I base + M (multiply), A (atomics), F/D (float), C (compressed). Learn how to evaluate which extensions to enable for your application.
* **Embedded Platforms:** SiFive FE310, ESP32-C3, GD32VF103 or FPGA-based soft cores (PicoRV32, VexRiscv on Zynq/Arty). Running FreeRTOS on a RISC-V soft core is a strong Phase 3 bridge project.

**Resources:**
* *"The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors"* — Joseph Yiu. The reference text for Cortex-M architecture, NVIC, and memory system.
* *"Embedded Systems: Introduction to Arm Cortex-M Microcontrollers"* — Jonathan Valvano. Practical C programming against Cortex-M hardware.
* CMSIS Documentation (Arm/Keil) — CMSIS-Core, CMSIS-DSP, CMSIS-NN.
* *"RISC-V Reader: An Open Architecture Atlas"* — Patterson & Waterman.

**Projects:**
* **CMSIS-NN keyword spotting:** Deploy a quantized keyword spotting model using CMSIS-NN on a Cortex-M7 board. Benchmark inference cycles and RAM usage.
* **MPU stack guard:** Configure an MPU region as a guard page below the stack on a Cortex-M4. Trigger a controlled overflow and verify the MemManage fault handler fires instead of silent corruption.
* **FreeRTOS on RISC-V soft core:** Port FreeRTOS to a VexRiscv or PicoRV32 core synthesized on an FPGA. Validate task switching and interrupt latency.

---

**2. C Programming for ARM Embedded Systems**

</details>

### 内存管理

* **分配策略：** 静态分配（全局变量、BSS）、栈分配，以及嵌入式里何时（以及何时不）使用堆（`malloc`/`free`）。理解碎片化，以及为什么许多嵌入式系统完全避免动态分配。
* **内存段：** `.text`、`.data`、`.bss`、`.rodata`——知道代码和数据位于 flash 还是 RAM，以及链接器脚本如何控制这一点。对 Cortex-M 启动代码和 `__attribute__((section(...)))` 至关重要。
* **DMA（直接内存访问）：** 在外设和内存之间传输数据，无需 CPU 参与。理解 DMA 描述符、缓存一致性问题（`__DSB()`、`SCB_CleanDCache()`）以及基于中断的完成，对高吞吐 I/O 至关重要。

### 位操作与寄存器访问

* **CMSIS 风格寄存器访问：** 使用 `typedef struct` + `volatile` + 位域或 `uint32_t` 掩码进行寄存器访问。编写可移植驱动，在寄存器级别工作，没有 HAL 开销。
* **位掩码模式：** `SET_BIT()`、`CLEAR_BIT()`、`READ_BIT()` 宏，Cortex-M3/M4 上的 bit-banding。掌握这些，以编写既正确又快速的 ISR 和驱动代码。

### 中断服务程序（ISR）

* **NVIC 优先级配置：** `NVIC_SetPriority()`、优先级分组和抢占。理解如何设置优先级，以确保时间关键的 ISR 抢占低优先级处理程序。
* **最小 ISR：** 保持 ISR 简短——设置标志或投递到队列，在任务上下文中完成工作。这是从裸机到 RTOS 设计的桥梁。
* **volatile 与内存屏障：** 在 ISR 和主上下文之间的共享变量上正确使用 `volatile`。需要时使用 `__DSB()` / `__ISB()`。

**资源：**
* *"C Programming for Embedded Systems"* — Kirk Zurell.
* STM32 HAL 源码——阅读 HAL 实现，以理解供应商驱动如何映射到 CMSIS 寄存器访问。
* ARM 应用笔记：AN321（CMSIS）、AN298（Cortex-M 内存系统）。

**项目：**
* **环形缓冲区 + DMA UART：** 实现一个无锁环形缓冲区，在 UART 接收模式下由 DMA 馈送。处理溢出，并在 ISR 中演示零拷贝接收。
* **自定义外设驱动：** 根据数据手册，为 SPI 传感器（例如 IMU）编写裸机驱动（不使用 HAL），使用 CMSIS 寄存器访问和中断驱动的完成。

---

## 3. FreeRTOS

### 核心 RTOS 概念

* **任务与调度：** 用 `xTaskCreate()` 创建任务，分配优先级，并理解抢占式调度。知道调度器何时运行（tick ISR、`taskYIELD()`、阻塞调用）。
* **调度算法：** 抢占式固定优先级、等优先级任务的时间片轮转，以及协作模式。理解优先级反转，以及如何用优先级继承互斥量防止它。
* **任务状态：** 运行、就绪、阻塞、挂起。trace 任务状态转换，以诊断调度问题。

### 任务间通信

* **队列：** `xQueueSend()` / `xQueueReceive()`——主要的 IPC 机制。在任务和 ISR 上下文中使用（`xQueueSendFromISR()`）。理解带超时的阻塞。
* **信号量与互斥量：** 二值信号量用于信令（ISR → 任务），计数信号量用于资源计数，互斥量用于带优先级继承的互斥。
* **事件组：** 用位标志同步多个任务或 ISR。`xEventGroupSetBits()` / `xEventGroupWaitBits()`——对状态机和多源同步有用。
* **流和消息缓冲区：** 在任务和 ISR 之间高效传递字节流或帧消息，对于批量数据比队列开销更低。

### 内存管理

* **堆方案（heap_1 到 heap_5）：** 选择正确的分配器——heap_1（无释放）、heap_4（最佳适配，最常见）、heap_5（非连续区域）。监控 `xPortGetFreeHeapSize()` 和 `uxTaskGetStackHighWaterMark()`。
* **静态分配：** 用 `xTaskCreateStatic()` / `xQueueCreateStatic()` 将 TCB 和栈放入静态定义的数组——完全消除堆使用，在安全关键设计中是必需的。


<details>
<summary>English original</summary>

**Memory Management**

* **Allocation Strategies:** Static allocation (globals, BSS), stack allocation, and when (and when not) to use heap (`malloc`/`free`) in embedded. Understand fragmentation and why many embedded systems avoid dynamic allocation entirely.
* **Memory Sections:** `.text`, `.data`, `.bss`, `.rodata` — know where code and data live in flash vs. RAM and how the linker script controls this. Critical for Cortex-M startup code and `__attribute__((section(...)))`.
* **DMA (Direct Memory Access):** Transfer data between peripherals and memory without CPU involvement. Understanding DMA descriptors, cache coherency issues (`__DSB()`, `SCB_CleanDCache()`), and interrupt-based completion is essential for high-throughput I/O.

**Bit Manipulation and Register Access**

* **CMSIS-style register access:** Use `typedef struct` + `volatile` + bitfields or `uint32_t` masks for register access. Write portable drivers that work at the register level without HAL overhead.
* **Bit masking patterns:** `SET_BIT()`, `CLEAR_BIT()`, `READ_BIT()` macros, bit-banding on Cortex-M3/M4. Master these to write ISRs and driver code that is both correct and fast.

**Interrupt Service Routines (ISRs)**

* **NVIC priority configuration:** `NVIC_SetPriority()`, priority grouping, and preemption. Understand how to set priorities to ensure time-critical ISRs preempt lower-priority handlers.
* **Minimal ISRs:** Keep ISRs short — set a flag or post to a queue, do work in task context. This is the bridge from bare-metal to RTOS design.
* **Volatile and memory barriers:** Use `volatile` correctly for shared variables between ISR and main context. Use `__DSB()` / `__ISB()` where needed.

**Resources:**
* *"C Programming for Embedded Systems"* — Kirk Zurell.
* STM32 HAL source code — read the HAL implementation to understand how vendor drivers map to CMSIS register access.
* ARM Application Notes: AN321 (CMSIS), AN298 (Cortex-M memory system).

**Projects:**
* **Circular buffer + DMA UART:** Implement a lockless circular buffer fed by DMA in UART receive mode. Handle overflow and demonstrate zero-copy receive in an ISR.
* **Custom peripheral driver:** Write a bare-metal driver (no HAL) for a SPI sensor (e.g., IMU) from the datasheet, using CMSIS register access and interrupt-driven completion.

---

**3. FreeRTOS**

**Core RTOS Concepts**

* **Tasks and Scheduling:** Create tasks with `xTaskCreate()`, assign priorities, and understand preemptive scheduling. Know when the scheduler runs (tick ISR, `taskYIELD()`, blocking calls).
* **Scheduling Algorithms:** Preemptive fixed-priority, time-slicing for equal-priority tasks, and co-operative mode. Understand priority inversion and how to prevent it with priority inheritance mutexes.
* **Task States:** Running, Ready, Blocked, Suspended. Trace task state transitions to diagnose scheduling issues.

**Inter-Task Communication**

* **Queues:** `xQueueSend()` / `xQueueReceive()` — the primary IPC mechanism. Use from both task and ISR context (`xQueueSendFromISR()`). Understand blocking with timeouts.
* **Semaphores and Mutexes:** Binary semaphore for signaling (ISR → task), counting semaphore for resource counting, mutex for mutual exclusion with priority inheritance.
* **Event Groups:** Synchronize multiple tasks or ISRs with bit-flags. `xEventGroupSetBits()` / `xEventGroupWaitBits()` — useful for state machines and multi-source synchronization.
* **Stream and Message Buffers:** Efficient byte-stream or framed-message passing between tasks and ISRs, lower overhead than queues for bulk data.

**Memory Management**

* **Heap schemes (heap_1 through heap_5):** Choose the right allocator — heap_1 (no free), heap_4 (best-fit, most common), heap_5 (non-contiguous regions). Monitor `xPortGetFreeHeapSize()` and `uxTaskGetStackHighWaterMark()`.
* **Static allocation:** Use `xTaskCreateStatic()` / `xQueueCreateStatic()` to place TCBs and stacks in statically-defined arrays — eliminates heap use entirely, required in safety-critical designs.

</details>

### 配置与调试

* **FreeRTOSConfig.h：** 调整 `configTICK_RATE_HZ`、`configMAX_PRIORITIES`、`configTOTAL_HEAP_SIZE`、`configUSE_TRACE_FACILITY`。每个选项都有代价——理解权衡取舍。
* **Runtime 统计：** 启用 `configGENERATE_RUN_TIME_STATS` 以测量每个任务的 CPU 时间。使用 `vTaskList()` / `vTaskGetRunTimeStats()` 获取文本格式的系统快照。
* **FreeRTOS+Trace（Tracealyzer）：** 使用 Percepio Tracealyzer 或 Segger SystemView 对应用进行插桩，以在时间线中可视化任务切换、队列操作和中断延迟。
* **栈溢出钩子：** 启用 `configCHECK_FOR_STACK_OVERFLOW`（方法 1 或 2）并实现 `vApplicationStackOverflowHook()`，以在 runtime 捕获溢出。

**资源：**
* *"Using the FreeRTOS Real Time Kernel: A Practical Guide"* —— Richard Barry。权威参考。
* *"Mastering the FreeRTOS Real Time Kernel"* —— Dr. Richard Barry（FreeRTOS 官方书籍，免费 PDF）。
* FreeRTOS 官方文档：freertos.org —— kernel 参考、移植指南、API 文档。
* Percepio Tracealyzer / Segger SystemView —— 提供免费层级用于链路追踪。

**项目：**
* **生产者-消费者流水线：** 两个任务通过队列通信——一个通过 SPI 读取传感器，另一个通过 UART 处理并格式化输出。用 Tracealyzer 验证不会发生死锁或饥饿。
* **实时数据采集：** 以固定速率采集 ADC 样本（定时器中断服务程序（ISR）→ 队列），在 FreeRTOS 任务中处理，并通过 UART 记录日志。用逻辑分析仪验证时序抖动。
* **多设备并发控制：** 从独立的 FreeRTOS 任务同时控制 LED PWM、伺服电机和显示器，这些任务通过信号量和事件组同步。

---

## 4. SPI（串行外设接口）

### 协议规范

* **时序与模式（CPOL/CPHA）：** 详细理解 4 种 SPI 模式——时钟极性（CPOL）和相位（CPHA）——并绘制每种模式的时序图。在编写驱动之前，了解特定传感器使用哪种模式。
* **数据帧格式：** 面向字节、面向字和可变长度传输。字节序（MSB 优先 vs. LSB 优先）、片选（CS）管理，以及多设备配置中的总线共享。
* **SPI 设备寻址：** 每设备独立 CS 线 vs. 菊花链。理解无毛刺的 CS 置位以及来自设备数据手册的时序约束。

### 驱动实现

* **裸机 SPI 驱动：** 配置 SPI 外设寄存器（STM32 上的 CR1/CR2、DR、SR），轮询或使用中断来完成 TX/RX。编写通用的 `spi_transfer(uint8_t *tx, uint8_t *rx, size_t len)` API。
* **DMA 驱动的 SPI：** 对 TX 和 RX 均使用 DMA，以在不阻塞 CPU 的情况下实现最大吞吐。处理缓存一致性，并确保 DMA 完成回调唤醒等待中的任务。
* **Linux SPI 驱动（spidev）：** 使用 `/dev/spidev*` 编写用户空间驱动，用于开发和原型验证。理解何时迁移到内核驱动以用于生产。

### FPGA 实现

* **Verilog 中的 SPI 控制器：** 在 RTL 中设计基于状态机的 SPI 主/从设备。包含 FIFO 缓冲、DMA 接口和模式选择寄存器。适用于阶段 3 的 FPGA 工作。

**项目：**
* **高速 IMU 数据采集：** 在 DMA 模式下通过 SPI 连接 IMU（例如 ICM-42688-P）。达到最大 ODR，并展示基于中断的数据就绪处理。
* **SPI OLED 显示驱动：** 为 SSD1306 OLED 编写完整驱动——初始化序列、帧缓冲 DMA 传输和局部更新。

---

## 5. UART（通用异步收发器）

### 协议规范

* **波特率与帧格式：** 波特率、位时间和时钟误差容限之间的关系。数据帧格式：起始位、数据位（5–9）、奇偶校验（偶/奇/无）、停止位（1/1.5/2）。知道如何计算波特率寄存器值。
* **流控制：** 硬件 RTS/CTS 和软件 XON/XOFF——何时使用各自、如何配置，以及常见陷阱（例如缺少 RTS 上拉）。
* **RS-232 vs. TTL vs. RS-485：** 电压电平、驱动器，以及何时使用线路驱动器 IC。RS-485 用于差分多站总线（工业传感器、电机驱动）。


<details>
<summary>English original</summary>

**Configuration and Debugging**

* **FreeRTOSConfig.h:** Tune `configTICK_RATE_HZ`, `configMAX_PRIORITIES`, `configTOTAL_HEAP_SIZE`, `configUSE_TRACE_FACILITY`. Every option has a cost — understand the trade-offs.
* **Runtime stats:** Enable `configGENERATE_RUN_TIME_STATS` to measure per-task CPU time. Use `vTaskList()` / `vTaskGetRunTimeStats()` for a text-format system snapshot.
* **FreeRTOS+Trace (Tracealyzer):** Instrument your application with Percepio Tracealyzer or Segger SystemView to visualize task switching, queue operations, and interrupt latency in a timeline.
* **Stack overflow hooks:** Enable `configCHECK_FOR_STACK_OVERFLOW` (method 1 or 2) and implement `vApplicationStackOverflowHook()` to catch overflows at runtime.

**Resources:**
* *"Using the FreeRTOS Real Time Kernel: A Practical Guide"* — Richard Barry. The canonical reference.
* *"Mastering the FreeRTOS Real Time Kernel"* — Dr. Richard Barry (FreeRTOS official book, free PDF).
* FreeRTOS official documentation: freertos.org — kernel reference, porting guide, API docs.
* Percepio Tracealyzer / Segger SystemView — free tiers available for tracing.

**Projects:**
* **Producer-consumer pipeline:** Two tasks communicating via queue — one reads a sensor over SPI, another processes and formats output over UART. Validate with Tracealyzer that no deadlock or starvation occurs.
* **Real-time data acquisition:** Acquire ADC samples at a fixed rate (timer ISR → queue), process in a FreeRTOS task, and log via UART. Verify timing jitter with a logic analyzer.
* **Multi-device concurrent control:** Control an LED PWM, a servo motor, and a display simultaneously from separate FreeRTOS tasks synchronized via semaphores and event groups.

---

**4. SPI (Serial Peripheral Interface)**

**Protocol Specifications**

* **Timing and Modes (CPOL/CPHA):** Understand the 4 SPI modes in detail — clock polarity (CPOL) and phase (CPHA) — and trace timing diagrams for each. Know which mode a specific sensor uses before writing a driver.
* **Data Framing:** Byte-oriented, word-oriented, and variable-length transfers. Endianness (MSB-first vs. LSB-first), chip select (CS) management, and bus sharing in multi-device configurations.
* **SPI Device Addressing:** CS line per device vs. daisy-chaining. Understand glitch-free CS assertion and timing constraints from device datasheets.

**Driver Implementation**

* **Bare-metal SPI driver:** Configure SPI peripheral registers (CR1/CR2, DR, SR on STM32), poll or use interrupts for TX/RX completion. Write a generic `spi_transfer(uint8_t *tx, uint8_t *rx, size_t len)` API.
* **DMA-driven SPI:** Use DMA for both TX and RX to achieve maximum throughput without blocking the CPU. Handle cache coherency and ensure DMA complete callback wakes the waiting task.
* **Linux SPI driver (spidev):** Write a userspace driver using `/dev/spidev*` for development and prototyping. Understand when to move to a kernel driver for production.

**FPGA Implementation**

* **SPI controller in Verilog:** Design a state-machine-based SPI master/slave in RTL. Include FIFO buffers, DMA interface, and mode selection registers. Useful for Phase 3 FPGA work.

**Projects:**
* **High-speed IMU data acquisition:** Interface with an IMU (e.g., ICM-42688-P) over SPI in DMA mode. Achieve maximum ODR and demonstrate interrupt-based data-ready handling.
* **SPI OLED display driver:** Write a complete driver for an SSD1306 OLED — init sequence, framebuffer DMA transfer, and partial update.

---

**5. UART (Universal Asynchronous Receiver/Transmitter)**

**Protocol Specifications**

* **Baud Rate and Framing:** Relationship between baud rate, bit time, and clock error tolerance. Data framing: start bit, data bits (5–9), parity (even/odd/none), stop bits (1/1.5/2). Know how to calculate baud rate register values.
* **Flow Control:** Hardware RTS/CTS and software XON/XOFF — when to use each, how to configure, and common pitfalls (e.g., missing RTS pull-up).
* **RS-232 vs. TTL vs. RS-485:** Voltage levels, drivers, and when to use a line driver IC. RS-485 for differential multi-drop buses (industrial sensors, motor drives).

</details>

### 驱动实现

* **中断驱动 + 环形缓冲区的 UART：** TX 与 RX 路径各自由一个环形缓冲区支撑。ISR 填充/排空缓冲区；应用读写不阻塞。
* **DMA UART 接收：** RX 使用循环模式的 DMA，无需逐字节中断即可捕获到来的字节。使用 DMA 半完成与完成回调，加上空闲线检测，处理变长帧。
* **配合 FreeRTOS 的 UART：** 用 mutex 保护 TX 缓冲区，通过队列或流缓冲区把收到的帧通知给处理任务。演示借助 direct-to-task DMA 的零拷贝接收。

**项目：**
* **UART bootloader：** 实现一个 bootloader，通过 UART 接收固件二进制（如 XMODEM 协议），写入 flash，校验通过后跳转到新应用。
* **GPS NMEA 解析器：** 通过 DMA UART 接收来自 GPS 模块的 NMEA 语句，解析纬度/经度/时间，并把结构化数据投递到 FreeRTOS 队列。
* **RS-485 Modbus RTU 节点：** 在 RS-485 上实现 Modbus RTU 从站——半双工方向控制、CRC16 校验、寄存器映射。

---

## 6. I2C（Inter-Integrated Circuit）

### 协议规范

* **总线机制：** START/STOP 条件、ACK/NACK、7 位与 10 位寻址、时钟拉伸。理解时钟拉伸在严格的 master 上如何引发超时问题。
* **多主多从：** 仲裁丢失检测、总线错误处理。掌握如何避免总线死锁（SDA 被卡住）并实现恢复（9 时钟脉冲流程）。
* **速率等级：** Standard（100 kHz）、Fast（400 kHz）、Fast-Plus（1 MHz）、High-Speed（3.4 MHz）。理解上拉电阻取值与电容、速率之间的关系。

### 驱动实现

* **中断 + DMA 的 I2C 驱动：** 避免轮询——使用基于中断或基于 DMA 的传输，配合 FreeRTOS 信号量，把调用任务阻塞到传输完成。
* **I2C 设备扫描与探测：** 实现总线扫描以枚举设备地址。用于板级 bring-up（上电点亮/调通）与诊断。
* **错误处理与恢复：** 实现总线超时检测与软件复位。妥当处理地址 NACK（设备不存在）与数据 NACK（溢出）。

**项目：**
* **多传感器 I2C 集线器：** 在同一条 I2C 总线上连接 3 个以上传感器（如 BME280 环境、MPU-6050 IMU、VL53L0X ToF 测距）。为每个传感器实现一个 FreeRTOS 任务，采用基于优先级的轮询。
* **I2C EEPROM 驱动：** 为 AT24C256 EEPROM 编写字节/页写/读驱动。处理页边界回绕与写周期时序。

---

## 7. CAN（Controller Area Network）

### 协议规范

* **帧类型：** 数据帧、远程帧、错误帧、过载帧。理解 CAN 帧的各字段：SOF、仲裁 ID（11 位标准 / 29 位扩展）、DLC、数据（0–8 字节）、CRC、ACK。
* **总线仲裁：** 非破坏性逐位仲裁——ID 较小者胜出。理解显性/隐性位电平，以及 CAN 为何对节点故障具有鲁棒性。
* **错误处理：** CAN 错误计数器（TEC/REC）、错误状态（error-active、error-passive、bus-off）、自动重传。了解何时以及如何从 bus-off 恢复。
* **CAN FD：** 更长的有效载荷（最多 64 字节）、数据段更高的位速率（最高 8 Mbps）。理解可变数据速率的帧格式与控制器要求（STM32G0/H7 上的 FDCAN 外设）。

### 上层协议

* **CANopen：** 嵌入式控制的标准应用层——对象字典、PDO（过程数据对象）、SDO（服务数据对象）、NMT 状态机。广泛用于工业机器人与运动控制。
* **SAE J1939：** 重型车辆与汽车标准——基于 PGN 的寻址、大载荷的传输协议、DM1/DM2 诊断报文。与 ADAS 和自动驾驶相关工作有关。
* **AUTOSAR COM / SOME/IP（背景）：** 理解 CAN 如何融入 AUTOSAR 协议栈，以及在现代车辆中如何桥接到基于以太网的协议。


<details>
<summary>English original</summary>

**Driver Implementation**

* **Interrupt-driven UART with circular buffer:** TX and RX paths each backed by a circular buffer. ISR fills/drains the buffer; application reads/writes without blocking.
* **DMA UART receive:** Use DMA in circular mode for RX to capture incoming bytes without per-byte interrupts. Use half-complete and complete DMA callbacks plus idle-line detection to handle variable-length frames.
* **UART with FreeRTOS:** Protect the TX buffer with a mutex, signal received frames via queue or stream buffer to a processing task. Demonstrate zero-copy receive with direct-to-task DMA.

**Projects:**
* **UART bootloader:** Implement a bootloader that receives a firmware binary over UART (e.g., XMODEM protocol), writes it to flash, and jumps to the new application after verification.
* **GPS NMEA parser:** Receive NMEA sentences from a GPS module via DMA UART, parse latitude/longitude/time, and post structured data to a FreeRTOS queue.
* **RS-485 Modbus RTU node:** Implement a Modbus RTU slave on RS-485 — half-duplex direction control, CRC16 checking, register map.

---

**6. I2C (Inter-Integrated Circuit)**

**Protocol Specifications**

* **Bus mechanics:** START/STOP conditions, ACK/NACK, 7-bit and 10-bit addressing, clock stretching. Understand how clock stretching can cause timeout issues with strict masters.
* **Multi-master and multi-slave:** Arbitration loss detection, bus error handling. Know how to avoid bus lockup (stuck SDA) and implement recovery (9-clock pulse procedure).
* **Speed grades:** Standard (100 kHz), Fast (400 kHz), Fast-Plus (1 MHz), High-Speed (3.4 MHz). Understand pull-up resistor sizing vs. capacitance vs. speed.

**Driver Implementation**

* **Interrupt + DMA I2C driver:** Avoid polling — use interrupt-based or DMA-based transfers with a FreeRTOS semaphore to block the calling task until completion.
* **I2C device scanning and probing:** Implement a bus scan to enumerate device addresses. Use for board bring-up and diagnostics.
* **Error handling and recovery:** Implement bus timeout detection and software reset. Handle NACK on address (device not present) and NACK on data (overrun) gracefully.

**Projects:**
* **Multi-sensor I2C hub:** Interface with 3+ sensors (e.g., BME280 environment, MPU-6050 IMU, VL53L0X ToF distance) on the same I2C bus. Implement a FreeRTOS task per sensor with priority-based polling.
* **I2C EEPROM driver:** Write a byte/page write/read driver for an AT24C256 EEPROM. Handle page boundary wrapping and write cycle timing.

---

**7. CAN (Controller Area Network)**

**Protocol Specifications**

* **Frame types:** Data frame, remote frame, error frame, overload frame. Understand the CAN frame fields: SOF, arbitration ID (11-bit standard / 29-bit extended), DLC, data (0–8 bytes), CRC, ACK.
* **Bus arbitration:** Non-destructive bitwise arbitration — lower ID wins. Understand dominant/recessive bit levels and why CAN is robust to node failures.
* **Error handling:** CAN error counters (TEC/REC), error states (error-active, error-passive, bus-off), and automatic retransmission. Know when and how to recover from bus-off.
* **CAN FD:** Extended payload (up to 64 bytes), higher bit rate in the data phase (up to 8 Mbps). Understand the flexible data-rate frame format and controller requirements (FDCAN peripheral on STM32G0/H7).

**Higher-Layer Protocols**

* **CANopen:** Standard application layer for embedded control — Object Dictionary, PDOs (Process Data Objects), SDOs (Service Data Objects), NMT state machine. Widely used in industrial robotics and motion control.
* **SAE J1939:** Heavy vehicle and automotive standard — PGN-based addressing, transport protocol for large payloads, DM1/DM2 diagnostic messages. Relevant for ADAS and autonomous vehicle work.
* **AUTOSAR COM / SOME/IP (context):** Understand how CAN fits in an AUTOSAR stack and how it bridges to Ethernet-based protocols in modern vehicles.

</details>

### 驱动实现

* **CAN 过滤器配置：** 在硬件 CAN 控制器上配置验收过滤器（掩码/列表模式），只接收相关的报文 ID，降低 CPU 负载。
* **基于 FreeRTOS 的 TX/RX：** 用队列管理 TX mailbox 与 RX FIFO 出队。正确处理 TX abort、RX overrun 和错误中断。
* **ISO-TP（ISO 15765-2）：** 承载于 CAN 之上的多帧传输协议，用于超过 8 字节的报文（UDS/OBD-II 诊断使用）。实现分段、流控与重组。

**资源：**
* *"Controller Area Network Projects"*——Wilfried Voss。实用的 CAN 协议指南。
* *"A Comprehensible Guide to J1939"*——Wilfried Voss。
* CANopen 规范（CiA 301）——可从 CAN in Automation 免费下载。
* STM32 CAN/FDCAN 参考手册 + 应用笔记（AN5348 FDCAN）。

**项目：**
* **双节点 CAN 网络：** 两个微控制器挂在同一条 CAN 总线上交换传感器数据（例如 IMU + 温度）。实现正确的终端匹配、过滤器配置与错误帧检测。
* **OBD-II 读取器（J1979）：** 通过 CAN 向车辆（或模拟器）发送 OBD-II PID，并解析响应得到发动机转速、车速、冷却液温度。
* **CANopen 从站节点：** 实现一个最小 CANopen 从站——心跳、NMT 状态机，以及至少一个映射传感器数据的 TPDO。使用开源协议栈（例如 CANopenNode）。
* **ISO-TP 层：** 实现 ISO 15765-2 分段传输，在 CAN 上收发最大 4095 字节的载荷。用一条 UDS 诊断请求（SID 0x22 ReadDataByIdentifier）验证。

---

## 8. 电源管理与 OTA 更新

### 低功耗设计

* **睡眠模式：** 掌握 Cortex-M 的睡眠模式——sleep、deep sleep、stop、standby——以及它们各自的唤醒延迟 / 外设保持权衡。
* **事件驱动架构：** 从轮询转向中断驱动、事件驱动的设计。事件之间用 `WFI`/`WFE` 指令和 FreeRTOS tickless idle 进入睡眠。
* **功耗剖析：** 用 Nordic PPK2、Otii Arc 或 µCurrent + 示波器测量平均电流。剖析各状态电流，用于 IoT 占空比计算。

### OTA 固件更新

* **Bootloader 设计：** 理解 MCUboot——镜像槽（primary、secondary）、签名校验（RSA/ECDSA），以及 swap/overwrite 更新机制。掌握从 ROM bootloader → MCUboot → 应用的链式启动。
* **双 bank A/B 更新：** 原子更新——把新固件写入非活动 bank，校验完整性，标记待交换，重启。校验失败则回滚。
* **OTA 传输：** BLE DFU（Nordic NRF5 SDK / Zephyr）、Wi-Fi 上的 MQTT/HTTP、蜂窝（LwM2M/CoAP）。实现增量更新（BSDiff）以减小载荷体积。

**资源：**
* MCUboot 文档（mcuboot.com）——支持 Zephyr、nRF Connect SDK、Mbed OS。
* *"Embedded Software Primer"*——David Simon。
* Mender.io——面向 Linux 与 MCU 目标的开源 OTA 框架。

**项目：**
* **安全启动 bootloader：** 兼容 MCUboot 的 Cortex-M4 bootloader，带 RSA-2048 签名校验和 A/B 镜像槽。
* **超低功耗 IoT 传感器节点：** 温度 + 加速度计节点，测量间隙进入 deep sleep，目标是 1 秒上报间隔下平均电流 <10 µA。
* **端到端 OTA 系统：** BLE 或 Wi-Fi OTA，带增量压缩，哈希校验失败时回滚。

---

## 9. IoT 网络与 OpenThread

### 为什么这属于嵌入式软件

一旦固件工程师跨出单板、单外设的范畴，下一个真正的问题就不只是「读一个传感器」，而是「让一个安全、低功耗的网络持续存活数月」。IoT 协议直接架在你的定时器、UART/SPI 链路、射频驱动、存储和电源状态之上，这正是它们属于嵌入式软件方向、而不该被当作独立云端话题的原因。

OpenThread 在这里是一个很好的首个协议，因为它把**嵌入式约束**与**真实网络结构**结合在一起。你必须把 802.15.4 射频、低功耗时序、报文格式、IPv6、入网配置、主机—控制器链路和边界路由器理解为一个连贯的系统。

### OpenThread 作为首个 IoT 协议

Thread 是构建在 IEEE 802.15.4 之上的、基于 IPv6 的低功耗 mesh 网络。OpenThread 是让这一切在真实硬件上落地的开源实现，覆盖 SoC 式 MCU 设计与 Linux 主机 + 射频协处理器设计。

这让 OpenThread 与路线图其余部分尤为相关。后续在 ESP32-C6 上构建 Thread RCP、通过 UART 接到 Jetson、让 Linux 运行上层 OpenThread 协议栈而由小射频芯片处理 802.15.4 时，同样的概念会再次出现。

从这里开始：

* [**IoT 网络与设备连接**](02-IoT/Guide.md)
* [**OpenThread**](02-IoT/01-OpenThread/Guide.md)

---


<details>
<summary>English original</summary>

**Driver Implementation**

* **CAN filter configuration:** Configure acceptance filters (mask/list mode) on the hardware CAN controller to receive only relevant message IDs and reduce CPU load.
* **TX/RX with FreeRTOS:** Use queues for TX mailbox management and RX FIFO dequeuing. Handle TX abort, RX overrun, and error interrupts properly.
* **ISO-TP (ISO 15765-2):** Multi-frame transport protocol over CAN for messages longer than 8 bytes (used by UDS/OBD-II diagnostics). Implement segmentation, flow control, and reassembly.

**Resources:**
* *"Controller Area Network Projects"* — Wilfried Voss. Practical CAN protocol guide.
* *"A Comprehensible Guide to J1939"* — Wilfried Voss.
* CANopen specification (CiA 301) — free download from CAN in Automation.
* STM32 CAN/FDCAN reference manual + application notes (AN5348 FDCAN).

**Projects:**
* **CAN network with two nodes:** Two microcontrollers on a CAN bus exchanging sensor data (e.g., IMU + temperature). Implement proper termination, filter configuration, and error-frame detection.
* **OBD-II reader (J1979):** Send OBD-II PIDs over CAN to a vehicle (or simulator) and parse responses for engine RPM, vehicle speed, coolant temperature.
* **CANopen slave node:** Implement a minimal CANopen slave — heartbeat, NMT state machine, and at least one TPDO mapping sensor data. Use an open-source stack (e.g., CANopenNode).
* **ISO-TP layer:** Implement ISO 15765-2 segmented transfer to send/receive payloads up to 4095 bytes over CAN. Validate with a UDS diagnostic request (SID 0x22 ReadDataByIdentifier).

---

**8. Power Management and OTA Updates**

**Low-Power Design**

* **Sleep modes:** Master Cortex-M sleep modes — sleep, deep sleep, stop, standby — and their wake-up latency / peripheral retention trade-offs.
* **Event-driven architecture:** Move from polling to interrupt-driven, event-driven designs. Sleep between events using `WFI`/`WFE` instructions and FreeRTOS tickless idle.
* **Power profiling:** Use Nordic PPK2, Otii Arc, or a µCurrent + oscilloscope to measure average current. Profile per-state current for IoT duty-cycle calculations.

**OTA Firmware Updates**

* **Bootloader design:** Understand MCUboot — image slots (primary, secondary), signature verification (RSA/ECDSA), and the swap/overwrite update mechanism. Know how to chain from ROM bootloader → MCUboot → application.
* **Dual-bank A/B updates:** Atomic update — write new firmware to the inactive bank, verify integrity, mark for swap, reboot. Rollback on failed verification.
* **OTA transport:** BLE DFU (Nordic NRF5 SDK / Zephyr), MQTT/HTTP over Wi-Fi, cellular (LwM2M/CoAP). Implement delta updates (BSDiff) to reduce payload size.

**Resources:**
* MCUboot documentation (mcuboot.com) — supports Zephyr, nRF Connect SDK, Mbed OS.
* *"Embedded Software Primer"* — David Simon.
* Mender.io — open-source OTA framework for both Linux and MCU targets.

**Projects:**
* **Secure bootloader:** MCUboot-compatible bootloader for Cortex-M4 with RSA-2048 signature verification and A/B image slots.
* **Ultra-low-power IoT sensor node:** Temperature + accelerometer node with deep-sleep between measurements, targeting <10 µA average at 1-second reporting interval.
* **End-to-end OTA system:** BLE or Wi-Fi OTA with delta compression and rollback on hash verification failure.

---

**9. IoT Networking and OpenThread**

**Why This Belongs in Embedded Software**

Once a firmware engineer moves beyond a single board and a single peripheral, the next real problem is not just "read a sensor" but "keep a secure, low-power network alive for months." IoT protocols sit directly on top of your timers, UART/SPI links, radio drivers, storage, and power states, which is why they belong in the embedded-software track instead of being treated as a separate cloud topic.

OpenThread is a strong first protocol here because it combines **embedded constraints** with **real networking structure**. You have to understand 802.15.4 radios, low-power timing, packet formats, IPv6, commissioning, host-controller links, and border routers as one coherent system.

**OpenThread as the First IoT Protocol**

Thread is an IPv6-based, low-power mesh network built on IEEE 802.15.4. OpenThread is the open-source implementation that makes this concrete on real hardware, including SoC-style MCU designs and Linux-host-plus-radio-co-processor designs.

This makes OpenThread especially relevant to the rest of the roadmap. The same concepts show up later when you build a Thread RCP on an ESP32-C6, attach it to a Jetson over UART, and let Linux run the higher-level OpenThread stack while the small radio chip handles 802.15.4.

Start here:

* [**IoT Networking and Device Connectivity**](02-IoT/Guide.md)
* [**OpenThread**](02-IoT/01-OpenThread/Guide.md)

---

</details>

## AI 硬件关联

| 主题 | 与 AI 硬件工程的关联 |
|-------|--------------------------------------|
| ARM Cortex-M + CMSIS-NN | 在 MCU 上进行 TinyML 推理 — 无需操作系统即可部署量化模型（关键词识别、异常检测） |
| FreeRTOS | 为向 AI 推理任务输送数据的传感器流水线提供实时调度；用于 openpilot 的 comma 3X panda 微控制器 |
| SPI / I2C / CAN | 摄像头、IMU、LiDAR（激光雷达）、雷达的传感器接口 — AI 感知系统的输入流水线 |
| CAN / J1939 | 车辆总线协议 — openpilot 如何在真实车辆上读取和写入执行器命令 |
| 电源管理 | 电池供电的边缘 AI 设备：在边缘运行推理时，每一 µA 都重要 |
| OTA 更新 | 生产环境边缘 AI 部署 — 将新模型权重和固件推送到已部署设备 |
| OpenThread / IoT 网络 | 在真实产品中连接低功耗传感器节点、网关和 Linux 主机，而非孤立的实验室配置 |

---

## 项目总结

| 项目 | 关键技能 |
|---------|-----------|
| Cortex-M7 上的 CMSIS-NN 关键词识别 | ARM 架构、CMSIS-NN、TinyML 部署 |
| FreeRTOS 生产者-消费者传感器流水线 | 任务设计、队列、中断服务程序（ISR）到任务交接 |
| OpenThread RCP + Linux 主机 | IoT 网络、UART/SPI 主机链路、边界路由器架构 |
| DMA 循环缓冲区 UART 接收器 | DMA、循环缓冲区、空闲线检测 |
| 以最大 ODR 运行的 SPI IMU | SPI DMA、中断驱动的数据就绪 |
| I2C 多传感器集线器 | I2C 总线管理、多任务轮询 |
| 带错误处理的 CAN 双节点网络 | CAN 帧格式、过滤器、错误状态 |
| OBD-II CAN 读取器 | J1979 协议、CAN 帧解析 |
| MCUboot 安全 bootloader | 安全启动、A/B 槽、签名验证 |
| 超低功耗 IoT 节点 | 深度睡眠、事件驱动、功耗剖析 |


<details>
<summary>English original</summary>

**AI Hardware Connection**

| Topic | Connection to AI Hardware Engineering |
|-------|--------------------------------------|
| ARM Cortex-M + CMSIS-NN | TinyML inference on MCUs — deploy quantized models (keyword spotting, anomaly detection) without an OS |
| FreeRTOS | Real-time scheduling for sensor pipelines feeding AI inference tasks; used in openpilot's comma 3X panda microcontroller |
| SPI / I2C / CAN | Sensor interfaces for cameras, IMUs, LiDAR, radar — the input pipeline for AI perception systems |
| CAN / J1939 | Vehicle bus protocol — how openpilot reads and writes actuator commands on real cars |
| Power management | Battery-powered edge AI devices: every µA counts when running inference at the edge |
| OTA updates | Production edge AI deployment — pushing new model weights and firmware to deployed devices |
| OpenThread / IoT networking | Connect low-power sensor nodes, gateways, and Linux hosts in real products instead of isolated lab setups |

---

**Projects Summary**

| Project | Key Skills |
|---------|-----------|
| CMSIS-NN keyword spotting on Cortex-M7 | ARM architecture, CMSIS-NN, TinyML deployment |
| FreeRTOS producer-consumer sensor pipeline | Task design, queues, ISR-to-task hand-off |
| OpenThread RCP + Linux host | IoT networking, UART/SPI host links, border-router architecture |
| DMA circular buffer UART receiver | DMA, circular buffers, idle-line detection |
| SPI IMU at maximum ODR | SPI DMA, interrupt-driven data-ready |
| I2C multi-sensor hub | I2C bus management, multi-task polling |
| CAN two-node network with error handling | CAN framing, filters, error states |
| OBD-II CAN reader | J1979 protocol, CAN frame parsing |
| MCUboot secure bootloader | Secure boot, A/B slots, signature verification |
| Ultra-low-power IoT node | Deep sleep, event-driven, power profiling |

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
