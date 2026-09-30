---
title: 方向 B §8 — GPU/Jetson 推理的 runtime 与驱动开发
description: 方向 B §8 — GPU/Jetson 推理的 runtime 与驱动开发
---

# 方向 B §8 — GPU/Jetson 推理的 runtime 与驱动开发

<div class="course-identity jetson-runtime" markdown="1">
<div class="course-identity__icon">RT</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 B8 · Jetson runtime 与驱动</p>
<p class="course-identity__title">掌握 Linux、CUDA、TensorRT、传感器与应用代码之间的 runtime 与驱动层。</p>
<p class="course-identity__meta">产物：runtime/驱动案例研究 · 度量：延迟、内存拷贝、利用率、失败</p>
</div>
</div>


> *掌握编译后的模型与 GPU 硬件之间的软件栈 —— CUDA runtime、TensorRT engine 执行、DLA 调度，以及 Linux 驱动接口。*

**前置要求：** 方向 B §1（Jetson 平台、CUDA 基础）、阶段 1 §3（操作系统 —— 驱动、内存管理）、阶段 1 §4（C++/CUDA）。

**层级映射：** AI 芯片栈的**Layer 3**（Runtime 与驱动）。本模块通过驱动与 runtime 软件，把 Layer 2（编译器输出 —— TensorRT engine、CUDA kernel）连接到 Layer 5（GPU/DLA 硬件）。

**岗位方向：** GPU/加速器 Runtime 工程师 · CUDA Runtime 工程师 · 推理平台工程师 · 嵌入式 Linux BSP 工程师（Jetson）

---

## 为什么 runtime 与驱动对 Jetson/GPU 至关重要

编译器产出优化后的 kernel 与执行计划。runtime 让它们真正跑起来：管理 GPU 内存、把 kernel 调度到 CUDA 核心与 DLA 引擎上、处理多流并发，并与 Linux 内核驱动通信。要在一块真实硬件上达成延迟与吞吐目标，理解这一层必不可少。

---

## 1. CUDA Runtime 与驱动架构

* **两级 CUDA API：**
    * **Runtime API**（`cudart`）：`cudaMalloc`、`cudaMemcpy`、`cudaLaunchKernel`、流、事件。
    * **Driver API**（`cuda`）：`cuCtxCreate`、`cuModuleLoad`、`cuLaunchKernel` —— 更底层，显式管理上下文。
    * 何时用哪个：多数工作用 runtime API；多上下文、动态模块加载或构建框架时用 driver API。

* **CUDA 上下文与流：**
    * 上下文 = 每 GPU 的状态（地址空间、模块、流）。
    * 流 = GPU 工作的有序队列。默认流与显式流。
    * 多流并发：重叠 compute、DMA（H2D、D2H）与 peer transfer。
    * 事件用于同步与计时。

* **内存管理：**
    * 设备内存（`cudaMalloc`）、锁页主机内存（`cudaMallocHost`）、统一内存（`cudaMallocManaged`）。
    * 配合流的异步 memcpy：`cudaMemcpyAsync`。
    * 内存池（`cudaMallocAsync` / `cudaMemPool`）—— 减少分配开销。
    * Jetson 统一内存架构：GPU 与 CPU 共享 LPDDR5 —— 对零拷贝与显式拷贝的影响。

* **kernel 启动机制：**
    * Grid → block → thread 层级。occupancy 计算器。
    * 启动配置：`<<<grid, block, shared_mem, stream>>>`。
    * 协作组、动态并行（进阶）。

**项目：**
* 在 Jetson 上写一个多流流水线：流 1 做 H2D + kernel A，流 2 做 kernel B + D2H，用基于事件的同步。用 `nsys` 度量重叠情况。
* 在 Jetson Orin Nano 上，针对一个 CNN 推理工作负载，比较统一内存（`cudaMallocManaged`）与显式拷贝（`cudaMalloc` + `cudaMemcpy`）。用 Nsight Systems 做 profiling。

---


<details>
<summary>English original</summary>

**Track B §8 — Runtime & Driver Development for GPU/Jetson Inference**

<div class="course-identity jetson-runtime" markdown="1">
<div class="course-identity__icon">RT</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track B8 · Jetson Runtime & Drivers</p>
<p class="course-identity__title">Own the runtime and driver layer between Linux, CUDA, TensorRT, sensors, and application code.</p>
<p class="course-identity__meta">Artifact: runtime/driver case study · Measure: latency, memory copies, utilization, failures</p>
</div>
</div>


> *Own the software stack between the compiled model and the GPU hardware — CUDA runtime, TensorRT engine execution, DLA scheduling, and Linux driver interfaces.*

**Prerequisites:** Track B §1 (Jetson Platform, CUDA basics), Phase 1 §3 (Operating Systems — drivers, memory management), Phase 1 §4 (C++/CUDA).

**Layer mapping:** **Layer 3** (Runtime & Driver) of the AI chip stack. This module connects Layer 2 (compiler output — TensorRT engines, CUDA kernels) to Layer 5 (GPU/DLA hardware) through the driver and runtime software.

**Role targets:** GPU/Accelerator Runtime Engineer · CUDA Runtime Engineer · Inference Platform Engineer · Embedded Linux BSP Engineer (Jetson)

---

**Why runtime & driver matters for Jetson/GPU**

The compiler produces optimized kernels and execution plans. The runtime makes them actually run: managing GPU memory, scheduling kernels across CUDA cores and DLA engines, handling multi-stream concurrency, and communicating with the Linux kernel driver. Understanding this layer is essential for hitting latency and throughput targets on real hardware.

---

**1. CUDA Runtime & Driver Architecture**

* **Two-level CUDA API:**
    * **Runtime API** (`cudart`): `cudaMalloc`, `cudaMemcpy`, `cudaLaunchKernel`, streams, events.
    * **Driver API** (`cuda`): `cuCtxCreate`, `cuModuleLoad`, `cuLaunchKernel` — lower-level, explicit context management.
    * When to use which: runtime API for most work; driver API for multi-context, dynamic module loading, or building frameworks.

* **CUDA context and streams:**
    * Context = per-GPU state (address space, modules, streams).
    * Streams = ordered queues of GPU work. Default stream vs explicit streams.
    * Multi-stream concurrency: overlap compute, DMA (H2D, D2H), and peer transfer.
    * Events for synchronization and timing.

* **Memory management:**
    * Device memory (`cudaMalloc`), pinned host memory (`cudaMallocHost`), unified memory (`cudaMallocManaged`).
    * Async memcpy with streams: `cudaMemcpyAsync`.
    * Memory pools (`cudaMallocAsync` / `cudaMemPool`) — reduce allocation overhead.
    * Jetson unified memory architecture: GPU and CPU share LPDDR5 — implications for zero-copy vs explicit copy.

* **Kernel launch mechanics:**
    * Grid → block → thread hierarchy. Occupancy calculator.
    * Launch configuration: `<<<grid, block, shared_mem, stream>>>`.
    * Cooperative groups, dynamic parallelism (advanced).

**Projects:**
* Write a multi-stream pipeline on Jetson: stream 1 does H2D + kernel A, stream 2 does kernel B + D2H, with event-based synchronization. Measure overlap with `nsys`.
* Compare unified memory (`cudaMallocManaged`) vs explicit copy (`cudaMalloc` + `cudaMemcpy`) for a CNN inference workload on Jetson Orin Nano. Profile with Nsight Systems.

---

</details>

## 2. TensorRT Runtime 深入解析

* **引擎生命周期：**
    * 构建阶段：`IBuilder` → `INetworkDefinition` → `IBuilderConfig` → `ICudaEngine`。
    * 序列化：保存/加载引擎（`.engine` / `.plan` 文件）。为何引擎与硬件绑定。
    * 执行：`IExecutionContext` — 一个引擎，多个上下文用于并发推理。

* **TensorRT 中的内存管理：**
    * 绑定：输入/输出张量地址。静态形状 vs 动态形状。
    * 工作区内存：策略的暂存空间。大小权衡。
    * 设备内存：预分配所有推理缓冲区；避免每次推理都分配。

* **执行与调度：**
    * 基于入队的异步执行：`context->enqueueV3(stream)`。
    * 多流推理：为不同模型或批大小分配独立的流。
    * DLA 集成：`config->setDeviceType(layer, DeviceType::kDLA)`。
    * DLA + GPU 拆分：DLA 不支持的层回退到 GPU；runtime 管理交接。

* **插件：**
    * `IPluginV2DynamicExt` 接口：原生不支持的自定义层。
    * 插件生命周期：`getOutputDimensions`、`enqueue`、`serialize`。
    * 何时编写插件，何时依赖 TensorRT 的融合。

* **动态形状与批处理：**
    * 优化配置文件：每个输入的最小/最优/最大维度。
    * 每次推理前指定 runtime 形状。
    * 推理服务中的动态批处理：收集请求 → 补齐到批 → 推理 → 拆分结果。

**项目：**
* 在 Jetson Orin Nano 上用 INT8 量化构建 YOLOv8 的 TensorRT 引擎。benchmark 不同批大小（1、4、8）下的 FPS。
* 为自定义激活函数编写 TensorRT 插件。集成进引擎并验证正确性。
* 实现双引擎流水线：检测模型跑在 GPU，分类模型跑在 DLA，用独立的 CUDA 流并发运行。

---

## 3. DLA（深度学习加速器）Runtime

* **Orin 上的 DLA 架构：**
    * Orin Nano 上有两个 DLA 引擎（基于 NVDLA）。
    * 支持的层：Conv、deconv、pooling、LRN、batch norm、element-wise、softmax。
    * 限制：不支持自定义算子、动态形状受限、concat/split 行为受限。

* **DLA 编程模型：**
    * 编译期：构建引擎时 TensorRT 把层标记给 DLA。
    * Runtime：TensorRT runtime 通过 `nvhost` 驱动把分配给 DLA 的层派发到 DLA 引擎。
    * 回退：不支持的层在 GPU 上运行；在 DLA 与 GPU 内存之间传输数据。

* **DLA 驱动栈：**
    * `nvhost-nvdla` 内核驱动：提交 DLA 任务，管理 DLA 内存。
    * DLA 引擎上的 `nvdla_runtime` 固件。
    * NVDLA 开源参考：[github.com/nvdla](http://nvdla.org) — 可用于理解 DLA runtime 概念。

* **DLA 性能调优：**
    * 最大化 DLA 常驻层：减少 GPU↔DLA 切换。
    * DLA 友好的模型设计：避免不支持的算子，优先使用标准卷积。
    * 能效：对支持的算子，DLA 比 GPU 更省电。

**项目：**
* 拿一个 MobileNetV2，对所有支持的层用 `setDeviceType(kDLA)` 部署。记录哪些层回退到 GPU。优化模型以最大化 DLA 覆盖率。
* 测量仅 GPU、仅 DLA 与混合推理三种情况的功耗（Jetson `tegrastats`）。

---


<details>
<summary>English original</summary>

**2. TensorRT Runtime Deep Dive**

* **Engine lifecycle:**
    * Build phase: `IBuilder` → `INetworkDefinition` → `IBuilderConfig` → `ICudaEngine`.
    * Serialization: save/load engine (`.engine` / `.plan` file). Why engines are hardware-specific.
    * Execution: `IExecutionContext` — one engine, multiple contexts for concurrent inference.

* **Memory management in TensorRT:**
    * Bindings: input/output tensor addresses. Static vs dynamic shapes.
    * Workspace memory: scratch space for tactics. Sizing trade-offs.
    * Device memory: pre-allocate all inference buffers; avoid per-inference allocation.

* **Execution and scheduling:**
    * Enqueue-based async execution: `context->enqueueV3(stream)`.
    * Multi-stream inference: separate streams for different models or batch sizes.
    * DLA integration: `config->setDeviceType(layer, DeviceType::kDLA)`.
    * DLA + GPU split: layers that DLA doesn't support fall back to GPU; runtime manages handoff.

* **Plugins:**
    * `IPluginV2DynamicExt` interface: custom layers not natively supported.
    * Plugin lifecycle: `getOutputDimensions`, `enqueue`, `serialize`.
    * When to write a plugin vs when to rely on TensorRT's fusion.

* **Dynamic shapes and batching:**
    * Optimization profiles: min/opt/max dimensions for each input.
    * Runtime shape specification before each inference.
    * Dynamic batching in serving: collect requests → pad to batch → infer → split results.

**Projects:**
* Build a TensorRT engine for YOLOv8 on Jetson Orin Nano with INT8 quantization. Benchmark FPS at different batch sizes (1, 4, 8).
* Write a TensorRT plugin for a custom activation function. Integrate into the engine and verify correctness.
* Implement a dual-engine pipeline: detection model on GPU, classification model on DLA, running concurrently with separate CUDA streams.

---

**3. DLA (Deep Learning Accelerator) Runtime**

* **DLA architecture on Orin:**
    * Two DLA engines (NVDLA-based) on Orin Nano.
    * Supported layers: Conv, deconv, pooling, LRN, batch norm, element-wise, softmax.
    * Limitations: no custom ops, limited dynamic shapes, restricted concat/split behavior.

* **DLA programming model:**
    * Compile-time: TensorRT marks layers for DLA during engine build.
    * Runtime: TensorRT runtime dispatches DLA-assigned layers to DLA engine via `nvhost` driver.
    * Fallback: unsupported layers run on GPU; data transfer between DLA and GPU memory.

* **DLA driver stack:**
    * `nvhost-nvdla` kernel driver: submits DLA tasks, manages DLA memory.
    * `nvdla_runtime` firmware on DLA engine.
    * NVDLA open-source reference: [github.com/nvdla](http://nvdla.org) — study for understanding DLA runtime concepts.

* **DLA performance tuning:**
    * Maximize DLA-resident layers: fewer GPU↔DLA transitions.
    * DLA-friendly model design: avoid unsupported ops, prefer standard convolutions.
    * Power efficiency: DLA is more power-efficient than GPU for supported ops.

**Projects:**
* Take a MobileNetV2 and deploy with `setDeviceType(kDLA)` for all supported layers. Log which layers fall back to GPU. Optimize the model to maximize DLA coverage.
* Measure power consumption (Jetson `tegrastats`) for GPU-only vs DLA-only vs mixed inference.

---

</details>

## 4. Jetson 上的 NVIDIA 内核驱动栈

* **GPU 驱动（`nvgpu`）：**
    * 开源 Tegra GPU 驱动（不是专有的桌面驱动）。
    * 设备树配置：GPU 时钟、电源域、内存预留。
    * `nvgpu` sysfs：频率调节、功耗封顶、硬件计数器。
    * 用户态 CUDA 调用如何到达 `nvgpu`：`ioctl` 接口、channel 提交。

* **视频与相机驱动：**
    * `nvcsi` → `vi`（Video Input）→ `isp` 流水线。
    * 传感器驱动（`v4l2_subdev`）：I2C 寄存器编程、模式表。
    * 传感器数据如何流入 GPU 内存用于推理（经 NvBufSurface 的零拷贝路径）。

* **显示与多媒体：**
    * `tegra-dc` / `nvdisplay`：显示控制器驱动。
    * NVDEC/NVENC：硬件视频解码/编码。通过 `nvv4l2decoder` 集成 GStreamer。

* **电源管理驱动：**
    * DVFS（Dynamic Voltage and Frequency Scaling）：`devfreq` 框架。
    * 电源域与时钟树：`clk_tegra`、`tegra-pmc`。
    * 热管理：`thermal_zone`、降频策略。
    * `nvpmodel`：功耗模式配置，MAX_N 与 15W、7W 模式。
    * `jetson_clocks`：为 benchmark 锁定最高时钟。

* **Jetson 上的内核模块开发：**
    * 基于 L4T 内核头文件构建树外模块。
    * 为载板上的自定义硬件编写设备树 overlay。
    * 调试：`dmesg`、`ftrace`、`/sys/kernel/debug`。

**项目：**
* 编写一个最小内核模块，从 `nvgpu` sysfs 读取 GPU 利用率，并暴露 `/proc/gpu_monitor` 接口。
* 为 Jetson 载板上的自定义 SPI 传感器添加设备树 overlay。编写一个 `v4l2_subdev` stub 驱动，通过 SPI 读取传感器 ID。
* 对相机到推理的流水线做 profile：使用 `nsys` + kernel tracepoint，测量从传感器采集到推理结果的延迟。

---

## 5. 多引擎调度与系统级 runtime

* **引擎并发执行：**
    * GPU + DLA + NVDEC + PVA（可编程视觉加速器）同时运行。
    * 按引擎划分的 CUDA 流：避免跨引擎串行化。
    * 优先级调度：为延迟敏感的推理使用高优先级流（`cudaStreamCreateWithPriority`）。

* **实时推理调度：**
    * 帧率驱动的调度：传感器帧 → 预处理 → 推理 → 后处理 → 执行。
    * 感知截止时间的执行：丢帧 vs 排队帧 vs 自适应批处理。
    * CPU-GPU 同步：用异步 API + event 最小化 CPU 阻塞。

* **多模型部署：**
    * 多个 TensorRT engine 共享 GPU：时间片 vs MPS（Multi-Process Service）。
    * 内存划分：按模型预分配 buffer，避免碎片化。
    * 模型切换：engine 加载延迟、预热策略。

* **DeepStream 作为系统级 runtime：**
    * 基于 GStreamer 的流水线：decode → 预处理 → 推理 → 跟踪 → 显示。
    * `nvinfer` plugin：把 TensorRT engine 封装进 GStreamer element。
    * 多路视频流：在单条流水线中处理 N 路相机。
    * 自定义 `GstBuffer` 元数据，把推理结果向下游传递。

* **Jetson 上的 Triton Inference Server：**
    * 模型仓库、动态批处理、模型并发执行。
    * 后端选项：TensorRT、ONNX Runtime、PyTorch、自定义 C++ 后端。
    * Jetson 部署考量：内存约束、功耗预算。

**项目：**
* 在 Jetson 上构建 4 路相机的 DeepStream 流水线：decode → 检测（GPU）→ 分类（DLA）→ 跟踪 → OSD → 显示。测量端到端延迟。
* 在 Jetson 上的 Triton Inference Server 部署两个模型（检测 + 分割）。配置动态批处理，测量负载下的吞吐。
* 实现一个基于优先级的调度器：安全关键模型走高优先级流，遥测模型走低优先级。用 `nsys` 验证高优先级永不被饿死。

---

## 与其他模块的关系

| 本模块（B §8） | 连接至 |
|---------------------|-------------|
| CUDA runtime 与内存 | B §1（Jetson 平台 — CUDA 基础） |
| TensorRT engine 执行 | B §5（应用开发 — ML/AI） |
| DLA runtime | B §1 深入解析（DLA 架构、张量核心） |
| 内核驱动栈 | B §3（L4T 定制 — kernel、设备树） |
| 电源管理 | B §4（FSP — SPE 固件、电源控制） |
| 推理 runtime（Triton、DeepStream） | 阶段 3（神经网络 — 待部署模型） |
| 编译器输出 → runtime 输入 | 方向 C（ML 编译器 — 生成 engine/kernel） |

---

## 构建总结

| 章节 | 动手交付物 |
|---------|---------------------|
| §1 CUDA runtime | 多流流水线、统一内存与显式内存对比 |
| §2 TensorRT runtime | INT8 engine 构建、自定义 plugin、双 engine 流水线 |
| §3 DLA runtime | DLA 覆盖率最大化、功耗测量 |
| §4 内核驱动 | GPU 监控模块、设备树 overlay + 传感器驱动 |
| §5 系统 runtime | 4 路相机 DeepStream 流水线、Triton 部署、优先级调度器 |


<details>
<summary>English original</summary>

**4. NVIDIA Kernel Driver Stack on Jetson**

* **GPU driver (`nvgpu`):**
    * Open-source Tegra GPU driver (not the proprietary desktop driver).
    * Device tree configuration: GPU clocks, power domains, memory carve-outs.
    * `nvgpu` sysfs: frequency scaling, power capping, hardware counters.
    * How user-space CUDA calls reach `nvgpu`: `ioctl` interface, channel submission.

* **Video and camera drivers:**
    * `nvcsi` → `vi` (Video Input) → `isp` pipeline.
    * Sensor driver (`v4l2_subdev`): I2C register programming, mode tables.
    * How sensor data flows to GPU memory for inference (zero-copy path via NvBufSurface).

* **Display and multimedia:**
    * `tegra-dc` / `nvdisplay`: display controller driver.
    * NVDEC/NVENC: hardware video decode/encode. GStreamer integration via `nvv4l2decoder`.

* **Power management drivers:**
    * DVFS (Dynamic Voltage and Frequency Scaling): `devfreq` framework.
    * Power domains and clock tree: `clk_tegra`, `tegra-pmc`.
    * Thermal management: `thermal_zone`, throttling policies.
    * `nvpmodel`: power mode profiles, MAX_N vs 15W vs 7W modes.
    * `jetson_clocks`: pin max clocks for benchmarking.

* **Kernel module development on Jetson:**
    * Building out-of-tree modules against L4T kernel headers.
    * Device tree overlay for custom hardware on carrier board.
    * Debugging: `dmesg`, `ftrace`, `/sys/kernel/debug`.

**Projects:**
* Write a minimal kernel module that reads GPU utilization from `nvgpu` sysfs and exposes a `/proc/gpu_monitor` interface.
* Add a device tree overlay for a custom SPI sensor on your Jetson carrier board. Write a `v4l2_subdev` stub driver that reads sensor ID over SPI.
* Profile the camera-to-inference pipeline: measure latency from sensor capture to inference result using `nsys` + kernel tracepoints.

---

**5. Multi-Engine Scheduling & System-Level Runtime**

* **Concurrent engine execution:**
    * GPU + DLA + NVDEC + PVA (Programmable Vision Accelerator) running simultaneously.
    * Per-engine CUDA streams: avoid serialization across engines.
    * Priority scheduling: high-priority streams (`cudaStreamCreateWithPriority`) for latency-critical inference.

* **Real-time inference scheduling:**
    * Frame-rate-driven scheduling: sensor frame → preprocess → infer → postprocess → actuate.
    * Deadline-aware execution: drop frames vs queue frames vs adaptive batch.
    * CPU-GPU synchronization: minimizing CPU blocking with async APIs + events.

* **Multi-model deployment:**
    * Multiple TensorRT engines sharing GPU: time-slicing vs MPS (Multi-Process Service).
    * Memory partitioning: pre-allocate buffers per model to avoid fragmentation.
    * Model switching: engine loading latency, warm-up strategies.

* **DeepStream as system-level runtime:**
    * GStreamer-based pipeline: decode → preprocess → infer → track → display.
    * `nvinfer` plugin: wraps TensorRT engine inside a GStreamer element.
    * Multi-stream video: process N cameras in one pipeline.
    * Custom `GstBuffer` metadata for passing inference results downstream.

* **Triton Inference Server on Jetson:**
    * Model repository, dynamic batching, concurrent model execution.
    * Backend options: TensorRT, ONNX Runtime, PyTorch, custom C++ backend.
    * Jetson deployment considerations: memory constraints, power budget.

**Projects:**
* Build a 4-camera DeepStream pipeline on Jetson: decode → detect (GPU) → classify (DLA) → track → OSD → display. Measure end-to-end latency.
* Deploy two models (detection + segmentation) on Triton Inference Server on Jetson. Configure dynamic batching and measure throughput under load.
* Implement a priority-based scheduler: safety-critical model gets high-priority stream, telemetry model gets low-priority. Verify with `nsys` that high-priority is never starved.

---

**Relationship to Other Modules**

| This module (B §8) | Connects to |
|---------------------|-------------|
| CUDA runtime & memory | B §1 (Jetson Platform — CUDA basics) |
| TensorRT engine execution | B §5 (Application Development — ML/AI) |
| DLA runtime | B §1 deep dives (DLA architecture, tensor cores) |
| Kernel driver stack | B §3 (L4T Customization — kernel, device tree) |
| Power management | B §4 (FSP — SPE firmware, power control) |
| Inference runtime (Triton, DeepStream) | Phase 3 (Neural Networks — models to deploy) |
| Compiler output → runtime input | Track C (ML Compiler — generates engines/kernels) |

---

**Build Summary**

| Section | Hands-on deliverable |
|---------|---------------------|
| §1 CUDA runtime | Multi-stream pipeline, unified vs explicit memory comparison |
| §2 TensorRT runtime | INT8 engine build, custom plugin, dual-engine pipeline |
| §3 DLA runtime | DLA coverage maximization, power measurement |
| §4 Kernel drivers | GPU monitor module, device tree overlay + sensor driver |
| §5 System runtime | 4-camera DeepStream pipeline, Triton deployment, priority scheduler |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/8. Runtime and Driver Development/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/8.%20Runtime%20and%20Driver%20Development/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
