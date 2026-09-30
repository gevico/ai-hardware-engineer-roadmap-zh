---
title: 方向 A §5 — FPGA 加速器的 runtime 与驱动开发
description: 方向 A §5 — FPGA 加速器的 runtime 与驱动开发
---

# 方向 A §5 — FPGA 加速器的 runtime 与驱动开发

<div class="course-identity fpga-runtime" markdown="1">
<div class="course-identity__icon">DRV</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 A5 · FPGA Runtime & Drivers</p>
<p class="course-identity__title">通过驱动、用户态 runtime 与生产控制路径暴露 FPGA 加速器。</p>
<p class="course-identity__meta">产物：驱动/runtime 接口 · 度量：延迟、吞吐、错误、恢复</p>
</div>
</div>


> *把 FPGA 加速器接到主机上 —— 编写让软件能用起硬件的 runtime、驱动与内存管理。*

**前置要求：** 方向 A §1–2（Vivado、Zynq PS/PL、AXI），阶段 1 §3（操作系统 —— 驱动、内存、系统调用），阶段 1 §4（C++）。

**层映射：** AI 芯片栈的 **Layer 3**（Runtime & Driver）。本模块把 Layer 6（你的 RTL/HLS 加速器）与 Layer 1（调用它的 AI 框架或应用）连接起来。

**目标岗位：** FPGA Runtime 工程师 · 加速器驱动开发工程师 · SoC 平台工程师 · 嵌入式 Linux BSP 工程师

---

## 为什么 runtime 与驱动对 FPGA 至关重要

你可以用 RTL 或 HLS 设计出最快的加速器 —— 但没有 runtime 和驱动，任何软件都用不上它。本模块教你：
- 通过 DMA 在主机 CPU 与 FPGA 加速器之间搬运数据
- 管理设备内存（PL 外挂的 DDR、BRAM、片上 buffer）
- 构建应用可调用以提交推理任务的用户态 API
- 为自定义 IP 编写或扩展 Linux 内核驱动

---

## 1. Xilinx Runtime（XRT）架构

* **XRT 概览：**
    * 用户态库（`libxrt_core`）+ 内核驱动（`xocl` / `zocl`）。
    * 支持的平台：Alveo（PCIe）、Zynq/MPSoC（嵌入式）、Versal。
    * `xclbin` 容器格式：bitstream + 元数据 + kernel 签名。

* **主机编程模型：**
    * `xrt::device`、`xrt::bo`（buffer object）、`xrt::kernel`、`xrt::run`。
    * 同步与异步执行模型。
    * buffer 分配：设备内存、主机独占、主机—设备共享。
    * 内存 bank 分配与拓扑感知。

* **FPGA 上的 OpenCL（Vitis 流程）：**
    * Vitis 如何把 HLS kernel 包装成 OpenCL kernel。
    * `cl::Buffer`、`cl::Kernel`、`cl::CommandQueue` —— 底层映射到 XRT。
    * 何时用 XRT 原生 API，何时用 OpenCL API。

**项目：**
* 编写一个 XRT 主机应用：加载 `xclbin`、分配输入/输出 buffer、运行 vector-add kernel，并验证结果。
* 用 `xbutil examine` 和 `vitis_analyzer` 对同一个 kernel 做性能剖析。区分 DMA 传输时间与计算时间。

---

## 2. DMA 与内存管理

* **AXI DMA 引擎：**
    * 内存映射（MM2S）与流（S2MM）通道。
    * Scatter-gather DMA：为非连续传输构建描述符链。
    * Simple DMA 与 SG DMA：权衡（延迟、灵活性、CPU 开销）。

* **buffer 管理模式：**
    * 双缓冲 / ping-pong：让计算与传输重叠。
    * 零拷贝：把设备内存映射到用户态（经驱动 `mmap`）。
    * CMA（Contiguous Memory Allocator），用于 Zynq 上大的物理连续 buffer。

* **Zynq/Alveo 上的存储层次：**
    * PS DDR ↔ PL DDR ↔ BRAM ↔ UltraRAM。
    * AXI SmartConnect / 互连的带宽与仲裁。
    * PS（ARM）与 PL 之间的缓存一致性：HPC 端口、ACE、snoop control unit。

* **IOMMU/SMMU：**
    * Zynq UltraScale+ 上用于 DMA 的虚拟地址（ARM SMMU）。
    * 为何重要：内存隔离、安全、多租户 FPGA。

**项目：**
* 用 AXI DMA IP 在 Zynq 上实现 PS 与 PL 之间的 DMA 传输。测量不同 buffer 尺寸（1KB–16MB）下的吞吐。
* 实现双缓冲：加速器处理 buffer A 的同时，DMA 填充 buffer B。测量吞吐提升。


<details>
<summary>English original</summary>

**Track A §5 — Runtime & Driver Development for FPGA Accelerators**

<div class="course-identity fpga-runtime" markdown="1">
<div class="course-identity__icon">DRV</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track A5 · FPGA Runtime & Drivers</p>
<p class="course-identity__title">Expose FPGA accelerators through drivers, userspace runtimes, and production control paths.</p>
<p class="course-identity__meta">Artifact: driver/runtime interface · Measure: latency, throughput, errors, recovery</p>
</div>
</div>


> *Connect your FPGA accelerator to the host — write the runtime, drivers, and memory management that make hardware usable from software.*

**Prerequisites:** Track A §1–2 (Vivado, Zynq PS/PL, AXI), Phase 1 §3 (Operating Systems — drivers, memory, system calls), Phase 1 §4 (C++).

**Layer mapping:** **Layer 3** (Runtime & Driver) of the AI chip stack. This module bridges Layer 6 (your RTL/HLS accelerator) with Layer 1 (the AI framework or application calling into it).

**Role targets:** FPGA Runtime Engineer · Accelerator Driver Developer · SoC Platform Engineer · Embedded Linux BSP Engineer

---

**Why runtime & driver matters for FPGA**

You can design the fastest accelerator in RTL or HLS — but without a runtime and driver, no software can use it. This module teaches you to:
- Move data between host CPU and FPGA accelerator via DMA
- Manage device memory (DDR attached to PL, BRAM, on-chip buffers)
- Build a user-space API that applications call to submit inference jobs
- Write or extend Linux kernel drivers for your custom IP

---

**1. Xilinx Runtime (XRT) Architecture**

* **XRT overview:**
    * User-space library (`libxrt_core`) + kernel driver (`xocl` / `zocl`).
    * Supported platforms: Alveo (PCIe), Zynq/MPSoC (embedded), Versal.
    * `xclbin` container format: bitstream + metadata + kernel signatures.

* **Host programming model:**
    * `xrt::device`, `xrt::bo` (buffer object), `xrt::kernel`, `xrt::run`.
    * Synchronous and asynchronous execution models.
    * Buffer allocation: device memory, host-only, host-device shared.
    * Memory bank assignment and topology awareness.

* **OpenCL on FPGA (Vitis flow):**
    * How Vitis wraps HLS kernels as OpenCL kernels.
    * `cl::Buffer`, `cl::Kernel`, `cl::CommandQueue` — mapping to XRT underneath.
    * When to use XRT native API vs OpenCL API.

**Projects:**
* Write an XRT host application that loads an `xclbin`, allocates input/output buffers, runs a vector-add kernel, and verifies results.
* Profile the same kernel with `xbutil examine` and `vitis_analyzer`. Identify DMA transfer time vs compute time.

---

**2. DMA and Memory Management**

* **AXI DMA engine:**
    * Memory-mapped (MM2S) and stream (S2MM) channels.
    * Scatter-gather DMA: descriptor chains for non-contiguous transfers.
    * Simple DMA vs SG DMA: trade-offs (latency, flexibility, CPU overhead).

* **Buffer management patterns:**
    * Double-buffering / ping-pong: overlap compute and transfer.
    * Zero-copy: mapping device memory into user-space (`mmap` via driver).
    * CMA (Contiguous Memory Allocator) for large physically-contiguous buffers on Zynq.

* **Memory hierarchy on Zynq/Alveo:**
    * PS DDR ↔ PL DDR ↔ BRAM ↔ UltraRAM.
    * AXI SmartConnect / interconnect bandwidth and arbitration.
    * Cache coherency between PS (ARM) and PL: HPC ports, ACE, snoop control unit.

* **IOMMU/SMMU:**
    * Virtual addresses for DMA on Zynq UltraScale+ (ARM SMMU).
    * Why this matters: memory isolation, security, multi-tenant FPGA.

**Projects:**
* Implement a DMA transfer between PS and PL on Zynq using AXI DMA IP. Measure throughput for different buffer sizes (1KB–16MB).
* Implement double-buffering: while the accelerator processes buffer A, DMA fills buffer B. Measure throughput improvement.

---

</details>

## 3. FPGA 的 Linux 内核驱动开发

* **平台设备驱动（Zynq）：**
    * 设备树绑定：`compatible`、`reg`、`interrupts`。
    * `probe()` / `remove()` 生命周期。
    * 内存映射 I/O：`ioremap()`、`readl()` / `writel()` 用于控制寄存器。

* **中断处理：**
    * 注册中断处理函数（`request_irq`）。
    * 用于 DMA 完成的上半部 / 下半部（tasklet、workqueue）。
    * 等待加速器完成：`wait_for_completion()` / 轮询。

* **字符设备接口：**
    * 通过 `/dev/myaccel` 将加速器暴露给用户空间。
    * `file_operations`：`open`、`release`、`ioctl`、`mmap`、`poll`。
    * `ioctl` 命令设计：提交任务、查询状态、配置参数。

* **UIO（Userspace I/O）驱动：**
    * 轻量级替代方案：将寄存器 + 中断映射到用户空间。
    * UIO 何时足够，何时需要完整的内核驱动。
    * 用于快速原型验证的 `generic-uio` 设备树 overlay。

* **PCIe 驱动（Alveo）：**
    * BAR（Base Address Register）映射。
    * MSI-X 中断。
    * XRT 的 `xocl` 驱动作为参考实现。

**项目：**
* 为 Zynq 上的自定义 AXI-Lite IP 编写一个最小平台驱动。通过 `/dev` + `ioctl` 暴露寄存器读/写。
* 为驱动添加中断驱动的 DMA 完成。对比延迟与轮询。
* 使用 UIO 从用户空间 Python 控制 HLS 加速器。与内核驱动路径进行 benchmark 对比。

---

## 4. 用户空间 runtime 库

* **Runtime API 设计：**
    * 参照熟悉的模式设计 API：`create_context()`、`allocate_buffer()`、`submit()`、`sync()`。
    * 异步执行：命令队列、事件、回调。
    * 错误处理：硬件超时、DMA 错误、ECC 故障。

* **构建 C++ runtime：**
    * 为设备句柄、缓冲区和 kernel 对象提供 RAII 封装。
    * 使用 `std::future` / 完成回调进行线程安全的任务提交。
    * 内存池：预分配设备缓冲区，避免每次推理的分配开销。

* **Python 绑定：**
    * 基于 C++ runtime 的 `pybind11` 封装。
    * NumPy ↔ 设备缓冲区零拷贝传输。
    * 集成点：PyTorch 自定义算子或 ONNX Runtime execution provider。

* **性能剖析与可观测性：**
    * 在 DMA 开始、DMA 结束、计算开始、计算结束处打时间戳。
    * 硬件性能计数器（如果已设计进 RTL）。
    * 通过 sysfs 或 runtime API 暴露指标。

**项目：**
* 为 HLS 矩阵乘加速器构建 C++ runtime 库：`init()`、`malloc_device()`、`submit_matmul()`、`sync()`、`free()`。
* 添加 Python 绑定。运行一个 PyTorch 模型，其中矩阵乘通过你的 runtime 卸载到 FPGA。
* 添加性能剖析：测量逐 layer 延迟分解（host 开销、DMA、计算）。

---

## 5. 推理 runtime 集成

* **Vitis AI runtime：**
    * 在 Zynq/Alveo 上集成 DPU（Deep Processing Unit）。
    * 模型编译：Vitis AI quantizer → 编译器 → `xmodel`。
    * `vart`（Vitis AI Runtime）：用于运行已编译模型的 C++/Python API。

* **FINN runtime：**
    * FINN 生成的加速器：带 AXI-Stream 接口的拼接 IP。
    * `finn-rtlib`：用于 FINN 加速器的 Python runtime。
    * FINN 流式架构下吞吐与延迟的权衡。

* **PYNQ overlay 模型：**
    * 从 Python 加载 bitstream + 驱动。
    * 使用 `pynq.allocate()`（CMA-backed）分配缓冲区。
    * ML 加速器的快速原型验证路径。

* **FPGA 上的 TVM：**
    * TVM 的 VTA（Versatile Tensor Accelerator）作为参考 FPGA 目标。
    * 用于将子图卸载到 FPGA 加速器的 BYOC（与方向 C 关联）。

**项目：**
* 在 Vitis AI DPU（Zynq 或 Alveo）上部署量化 CNN。测量 FPS 并与 CPU 基线对比。
* 运行 FINN 生成的二值 CNN 加速器。对流式吞吐进行性能剖析。
* 使用 PYNQ 进行原型验证：加载自定义 HLS overlay，从 Jupyter 运行推理，可视化延迟。

---

## 与其他模块的关系

| 本模块（A §5） | 连接到 |
|---------------------|-------------|
| DMA 与内存管理 | A §2（Zynq PS/PL、AXI 互连） |
| 内核驱动开发 | 阶段 1 §3（OS —— 驱动、中断、内存） |
| 用户空间 runtime | A §4（HLS —— 所驱动的加速器） |
| 推理 runtime（Vitis AI、FINN） | 阶段 3（神经网络 —— 待部署的模型） |
| FPGA 上的 TVM/BYOC | 方向 C（ML 编译器 —— 自定义后端） |

---

## 构建总结

| 小节 | 动手交付物 |
|---------|---------------------|
| §1 XRT | XRT host 应用 + 性能剖析 |
| §2 DMA | AXI DMA 吞吐 benchmark、双缓冲 |
| §3 内核驱动 | 带中断驱动 DMA 的平台驱动 |
| §4 用户空间 runtime | 面向 HLS 加速器的 C++ runtime + Python 绑定 |
| §5 推理 runtime | Vitis AI DPU 部署、FINN 流式、PYNQ 原型 |


<details>
<summary>English original</summary>

**3. Linux Kernel Driver Development for FPGA**

* **Platform device driver (Zynq):**
    * Device tree binding: `compatible`, `reg`, `interrupts`.
    * `probe()` / `remove()` lifecycle.
    * Memory-mapped I/O: `ioremap()`, `readl()` / `writel()` for control registers.

* **Interrupt handling:**
    * Registering interrupt handlers (`request_irq`).
    * Top-half / bottom-half (tasklet, workqueue) for DMA completion.
    * Waiting for accelerator completion: `wait_for_completion()` / poll.

* **Character device interface:**
    * Exposing accelerator to user-space via `/dev/myaccel`.
    * `file_operations`: `open`, `release`, `ioctl`, `mmap`, `poll`.
    * `ioctl` command design: submit job, query status, configure parameters.

* **UIO (Userspace I/O) driver:**
    * Lightweight alternative: map registers + interrupt to user-space.
    * When UIO is sufficient vs when a full kernel driver is needed.
    * `generic-uio` device tree overlay for quick prototyping.

* **PCIe driver (Alveo):**
    * BAR (Base Address Register) mapping.
    * MSI-X interrupts.
    * XRT's `xocl` driver as reference implementation.

**Projects:**
* Write a minimal platform driver for a custom AXI-Lite IP on Zynq. Expose register read/write via `/dev` + `ioctl`.
* Add interrupt-driven DMA completion to your driver. Compare latency vs polling.
* Use UIO to control an HLS accelerator from user-space Python. Benchmark vs kernel driver path.

---

**4. User-Space Runtime Library**

* **Runtime API design:**
    * Model the API after familiar patterns: `create_context()`, `allocate_buffer()`, `submit()`, `sync()`.
    * Asynchronous execution: command queues, events, callbacks.
    * Error handling: hardware timeouts, DMA errors, ECC faults.

* **Building a C++ runtime:**
    * RAII wrappers for device handles, buffers, and kernel objects.
    * Thread-safe job submission with `std::future` / completion callbacks.
    * Memory pool: pre-allocate device buffers to avoid allocation overhead per inference.

* **Python bindings:**
    * `pybind11` wrapper over C++ runtime.
    * NumPy ↔ device buffer zero-copy transfer.
    * Integration point: PyTorch custom operator or ONNX Runtime execution provider.

* **Profiling and observability:**
    * Timestamps at DMA start, DMA end, compute start, compute end.
    * Hardware performance counters (if designed into RTL).
    * Exposing metrics via sysfs or runtime API.

**Projects:**
* Build a C++ runtime library for your HLS matmul accelerator: `init()`, `malloc_device()`, `submit_matmul()`, `sync()`, `free()`.
* Add Python bindings. Run a PyTorch model where the matmul is offloaded to FPGA via your runtime.
* Add profiling: measure per-layer latency breakdown (host overhead, DMA, compute).

---

**5. Inference Runtime Integration**

* **Vitis AI runtime:**
    * DPU (Deep Processing Unit) integration on Zynq/Alveo.
    * Model compilation: Vitis AI quantizer → compiler → `xmodel`.
    * `vart` (Vitis AI Runtime): C++/Python APIs for running compiled models.

* **FINN runtime:**
    * FINN-generated accelerators: stitched IP with AXI-Stream interfaces.
    * `finn-rtlib`: Python runtime for FINN accelerators.
    * Throughput vs latency trade-offs with FINN's streaming architecture.

* **PYNQ overlay model:**
    * Load bitstream + driver from Python.
    * Allocate buffers with `pynq.allocate()` (CMA-backed).
    * Quick prototyping path for ML accelerators.

* **TVM on FPGA:**
    * TVM's VTA (Versatile Tensor Accelerator) as reference FPGA target.
    * BYOC for offloading subgraphs to FPGA accelerator (connection to Track C).

**Projects:**
* Deploy a quantized CNN on Vitis AI DPU (Zynq or Alveo). Measure FPS and compare with CPU baseline.
* Run a FINN-generated binary CNN accelerator. Profile streaming throughput.
* Use PYNQ to prototype: load a custom HLS overlay, run inference from Jupyter, visualize latency.

---

**Relationship to Other Modules**

| This module (A §5) | Connects to |
|---------------------|-------------|
| DMA and memory management | A §2 (Zynq PS/PL, AXI interconnect) |
| Kernel driver development | Phase 1 §3 (OS — drivers, interrupts, memory) |
| User-space runtime | A §4 (HLS — the accelerator you're driving) |
| Inference runtime (Vitis AI, FINN) | Phase 3 (Neural Networks — model to deploy) |
| TVM/BYOC on FPGA | Track C (ML Compiler — custom backend) |

---

**Build Summary**

| Section | Hands-on deliverable |
|---------|---------------------|
| §1 XRT | XRT host app + profiling |
| §2 DMA | AXI DMA throughput benchmark, double-buffering |
| §3 Kernel driver | Platform driver with interrupt-driven DMA |
| §4 User-space runtime | C++ runtime + Python bindings for HLS accelerator |
| §5 Inference runtime | Vitis AI DPU deployment, FINN streaming, PYNQ prototype |

</details>

---

> 原文：[`Phase 4 - Track A - Xilinx FPGA/5. Runtime and Driver Development/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20A%20-%20Xilinx%20FPGA/5.%20Runtime%20and%20Driver%20Development/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
