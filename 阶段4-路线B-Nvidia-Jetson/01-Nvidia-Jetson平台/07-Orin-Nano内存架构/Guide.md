---
title: Orin Nano 8GB — 内存架构深度剖析
description: Orin Nano 8GB — 内存架构深度剖析
---

# Orin Nano 8GB — 内存架构深度剖析

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">ON8M</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · Jetson 专题</p>
<p class="course-identity__title">Orin Nano 8GB 的专项课程标识 —— 内存架构深度剖析。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 度量：延迟、内存、功耗、日志</p>
</div>

</div>


> **范围：**对 Jetson Orin Nano 8GB（T234 SoC）内存工作原理的生产级理解 —— 从 DRAM 初始化，到 SMMU 转换、CMA 内部机制、相机零拷贝流水线、安全世界隔离，以及真实生产环境调试。
>
> **前置要求：**你应当熟悉 [Orin Nano 启动链](../Guide.md)以及基本的 Linux 内存概念。

---


## 0. Jetson 与独立 GPU —— 根本差异

在进入任何细节之前：先理解 **Jetson 内存为何与桌面/服务器 GPU 有本质区别**。这改变了你为边缘 AI 编写 CUDA 代码的一切方式。

### 独立 GPU（桌面/服务器：RTX 4090、H100）

```
┌─────────────────────────────────┐    PCIe Gen5 x16     ┌──────────────────────────┐
│          CPU (Host)             │◄════════════════════►│       GPU (Device)        │
│                                 │     64 GB/s          │                          │
│  DDR5 System RAM                │                      │  HBM3 / GDDR6X           │
│  64–512 GB                      │                      │  24–192 GB               │
│  ~100 GB/s                      │                      │  ~1–3.35 TB/s            │
│                                 │                      │                          │
│  CPU can NOT access GPU memory  │                      │  GPU can NOT access RAM   │
│  directly                       │                      │  directly                │
└─────────────────────────────────┘                      └──────────────────────────┘

Problem: every byte must cross PCIe (64 GB/s bottleneck)
  cudaMemcpy(d_ptr, h_ptr, size, cudaMemcpyHostToDevice);  ← mandatory, slow
  cudaMemcpy(h_ptr, d_ptr, size, cudaMemcpyDeviceToHost);  ← mandatory, slow
```

### Jetson（Orin Nano Super / Orin NX / AGX Orin）

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     T234 SoC (Orin Nano Super)                          │
│                                                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐  │
│  │  CPU     │  │  GPU     │  │  DLA     │  │  ISP/VI  │  │ NVENC  │  │
│  │  A78AE   │  │  Ampere  │  │          │  │  Camera  │  │ NVDEC  │  │
│  │  6 cores │  │  1024    │  │ ~10 TOPS │  │          │  │        │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  └───┬────┘  │
│       │             │             │             │             │        │
│       └─────────────┴─────────────┴─────────────┴─────────────┘        │
│                              │                                          │
│                    ┌─────────┴─────────┐                                │
│                    │ Memory Controller │                                │
│                    │  (MC) + SMMU      │                                │
│                    └─────────┬─────────┘                                │
└──────────────────────────────┼──────────────────────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │  8 GB LPDDR5        │
                    │  ~102 GB/s bandwidth │
                    │  SHARED by ALL      │
                    └─────────────────────┘

No PCIe. No copy. CPU, GPU, DLA, camera ALL access the SAME physical memory.
```

### 这对你的代码意味着什么

| 操作 | 独立 GPU | Jetson |
|-----------|-------------|--------|
| **分配 GPU 内存** | `cudaMalloc`（独立 VRAM） | `cudaMalloc`（同一 DRAM 池） |
| **主机→设备拷贝** | `cudaMemcpy`**（强制，慢）** | **通常不必** —— 用零拷贝 |
| **设备→主机拷贝** | `cudaMemcpy`**（强制，慢）** | **通常不必** —— 用零拷贝 |
| **托管内存** | 经 PCIe 做页迁移（极慢） | 同一 DRAM 内页迁移（快） |
| **相机 → GPU** | Camera→RAM→PCIe→VRAM（3 次拷贝） | Camera→DRAM→GPU 读同一 DRAM（**0 次拷贝**） |
| **内存容量** | CPU：512 GB + GPU：192 GB（各自独立） | **总计 8 GB**（一切共享） |
| **带宽** | CPU：100 GB/s，GPU：3,350 GB/s（各自独立） | **约 102 GB/s 共享**（各方争抢） |


<details>
<summary>English original</summary>

**Orin Nano 8GB — Memory Architecture Deep Dive**

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">ON8M</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Orin Nano 8GB — Memory Architecture Deep Dive.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Scope:** Production-level understanding of how memory works on Jetson Orin Nano 8GB (T234 SoC) — from DRAM initialization through SMMU translation, CMA internals, camera zero-copy pipelines, secure world isolation, and real production debugging.
>
> **Prerequisites:** You should be familiar with the [Orin Nano boot chain](../Guide.md) and basic Linux memory concepts.

---


**0. Jetson vs Discrete GPU — The Fundamental Difference**

Before any detail: understand **why Jetson memory is fundamentally different** from a desktop/server GPU. This changes everything about how you write CUDA code for edge AI.

**Discrete GPU (Desktop/Server: RTX 4090, H100)**

```
┌─────────────────────────────────┐    PCIe Gen5 x16     ┌──────────────────────────┐
│          CPU (Host)             │◄════════════════════►│       GPU (Device)        │
│                                 │     64 GB/s          │                          │
│  DDR5 System RAM                │                      │  HBM3 / GDDR6X           │
│  64–512 GB                      │                      │  24–192 GB               │
│  ~100 GB/s                      │                      │  ~1–3.35 TB/s            │
│                                 │                      │                          │
│  CPU can NOT access GPU memory  │                      │  GPU can NOT access RAM   │
│  directly                       │                      │  directly                │
└─────────────────────────────────┘                      └──────────────────────────┘

Problem: every byte must cross PCIe (64 GB/s bottleneck)
  cudaMemcpy(d_ptr, h_ptr, size, cudaMemcpyHostToDevice);  ← mandatory, slow
  cudaMemcpy(h_ptr, d_ptr, size, cudaMemcpyDeviceToHost);  ← mandatory, slow
```

**Jetson (Orin Nano Super / Orin NX / AGX Orin)**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     T234 SoC (Orin Nano Super)                          │
│                                                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐  │
│  │  CPU     │  │  GPU     │  │  DLA     │  │  ISP/VI  │  │ NVENC  │  │
│  │  A78AE   │  │  Ampere  │  │          │  │  Camera  │  │ NVDEC  │  │
│  │  6 cores │  │  1024    │  │ ~10 TOPS │  │          │  │        │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  └───┬────┘  │
│       │             │             │             │             │        │
│       └─────────────┴─────────────┴─────────────┴─────────────┘        │
│                              │                                          │
│                    ┌─────────┴─────────┐                                │
│                    │ Memory Controller │                                │
│                    │  (MC) + SMMU      │                                │
│                    └─────────┬─────────┘                                │
└──────────────────────────────┼──────────────────────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │  8 GB LPDDR5        │
                    │  ~102 GB/s bandwidth │
                    │  SHARED by ALL      │
                    └─────────────────────┘

No PCIe. No copy. CPU, GPU, DLA, camera ALL access the SAME physical memory.
```

**What This Means for Your Code**

| Operation | Discrete GPU | Jetson |
|-----------|-------------|--------|
| **Allocate GPU memory** | `cudaMalloc` (separate VRAM) | `cudaMalloc` (same DRAM pool) |
| **Copy host→device** | `cudaMemcpy` **(mandatory, slow)** | **Often unnecessary** — use zero-copy |
| **Copy device→host** | `cudaMemcpy` **(mandatory, slow)** | **Often unnecessary** — use zero-copy |
| **Managed memory** | Page migration over PCIe (very slow) | Page migration in same DRAM (fast) |
| **Camera → GPU** | Camera→RAM→PCIe→VRAM (3 copies) | Camera→DRAM→GPU reads same DRAM (**0 copies**) |
| **Memory capacity** | CPU: 512 GB + GPU: 192 GB (separate) | **8 GB total** (shared by everything) |
| **Bandwidth** | CPU: 100 GB/s, GPU: 3,350 GB/s (separate) | **~102 GB/s shared** (everyone competes) |

</details>

### 三个编程层面的影响

**1. 零拷贝是最大优势。**
在独立 GPU 上，`cudaMemcpy` 往往主导执行时间。在 Jetson 上，可以跳过：

```cpp
// ── Discrete GPU: mandatory copy ──────────────────────────────
float *h_data = (float*)malloc(size);          // host RAM
float *d_data;
cudaMalloc(&d_data, size);                     // GPU VRAM
cudaMemcpy(d_data, h_data, size, cudaMemcpyHostToDevice);  // PCIe copy (slow!)
kernel<<<grid, block>>>(d_data);
cudaMemcpy(h_data, d_data, size, cudaMemcpyDeviceToHost);  // PCIe copy (slow!)

// ── Jetson: zero-copy with pinned host memory ─────────────────
float *shared;
cudaMallocHost(&shared, size);                 // pinned, in same DRAM
// Both CPU and GPU access 'shared' directly — NO copy needed
fill_data_on_cpu(shared);                      // CPU writes
kernel<<<grid, block>>>(shared);               // GPU reads same memory
cudaDeviceSynchronize();
read_results_on_cpu(shared);                   // CPU reads GPU output
```

**2. 瓶颈是内存带宽，不是算力。**
独立 H100：3,350 GB/s HBM → 989 TFLOPS。Jetson Orin Nano Super：102 GB/s LPDDR5 → 67 TOPS（GPU）+ ~10 TOPS（DLA）≈ 77 TOPS 合计。算力与带宽之比差异极大：

```
H100 ridge point:         989 TFLOPS / 3,350 GB/s ≈ 295 FLOP/byte
Orin Nano Super ridge:    67 TOPS / 102 GB/s ≈ 0.66 OP/byte

→ Almost EVERYTHING is memory-bound on Jetson.
→ Tiling, data reuse, and INT8 quantization are not optional — they're mandatory.
```

**3. 内存容量极其宝贵——总共 8 GB。**
在服务器上，7B 大语言模型以 FP16 存放需占 14 GB GPU 显存，剩余 500+ GB 系统内存留给其他所有用途。在 Jetson 8 GB 上，同样的模型根本放不下。必须：
- 激进量化（INT8 → 7 GB，INT4 → 3.5 GB）
- 计入 OS + camera + CUDA runtime 的开销（~2–3 GB）
- 选择符合预算的模型（3.2M 参数的 YOLOv8-N，而非 68M 的 YOLOv8-X）

### 内存预算对比

```
Discrete Server (H100 80GB + 512 GB DDR5):
  GPU VRAM: 80 GB  │ System RAM: 512 GB
  ─────────────────│────────────────────
  Model: 40 GB     │ OS: 4 GB
  Activations: 20 GB│ App: 2 GB
  Scratch: 15 GB   │ Datasets: 200 GB
  Free: 5 GB       │ Free: 306 GB

Jetson Orin Nano 8GB (shared):
  ┌──────────────────────────────────┐
  │ 8 GB LPDDR5 (total)             │
  │                                  │
  │ Firmware carveouts:   ~0.4 GB   │
  │ OS + kernel:          ~0.5 GB   │
  │ CMA (camera buffers): ~0.75 GB  │
  │ CUDA runtime:         ~0.3 GB   │
  │ Model (INT8 YOLO):    ~0.1 GB   │
  │ Inference scratch:     ~0.2 GB   │
  │ ─────────────────────────────── │
  │ Free for userspace:    ~5.75 GB │
  └──────────────────────────────────┘
```

### Jetson 上的 CUDA 内存 API 决策树

```
What are you allocating?
│
├── Camera frames / video decoder output
│   └── Use DMA-BUF / NvBufSurface (zero-copy from hardware engine)
│
├── Model weights (loaded once, read by GPU)
│   └── cudaMalloc (pinned device memory, fastest GPU access)
│
├── Pre/post-processing buffers (CPU writes, GPU reads, or vice versa)
│   └── cudaMallocHost (pinned, both CPU and GPU access, no copy)
│
├── Prototyping / irregular access pattern
│   └── cudaMallocManaged (automatic migration, but unpredictable latency)
│
└── Temporary GPU-only scratch
    └── cudaMalloc (standard, fastest)
```

> **关键思维转变：** 在独立 GPU 上，你思考的是「最小化 PCIe 传输」。在 Jetson 上，你思考的是「最小化 DRAM 总带宽消耗」——因为 CPU、GPU、摄像头和显示共用同一条 ~102 GB/s 的管道。

---

## 1. T234 SoC 内存架构概览

Orin Nano 8GB 中的 Tegra234 SoC 采用**统一内存架构**——CPU、GPU、DLA 和所有加速器共享同一个 8GB LPDDR5 内存池。不存在独立的 GPU 显存。

```
                        8GB LPDDR5
                    ┌───────────────┐
                    │               │
       ┌────────────┤  Memory       ├────────────┐
       │            │  Controllers  │            │
       │            │  (MC)         │            │
       │            └───────┬───────┘            │
       │                    │                    │
  ┌────┴────┐         ┌────┴────┐         ┌─────┴────┐
  │ CPU     │         │ GPU     │         │ DLA/ISP  │
  │ A78AE   │         │ Ampere  │         │ NVENC    │
  │ cluster │         │ 1024    │         │ NVDEC    │
  │         │         │ cores   │         │ CSI/VI   │
  └─────────┘         └─────────┘         └──────────┘
```

### 内存控制器

T234 有多个连接到 LPDDR5 接口的内存控制器（MC）通道。MC 负责：

* CPU、GPU 与所有加速器之间的仲裁
* 带宽划分（可通过 BPMP 固件配置）
* 对延迟敏感引擎（如显示、摄像头）的服务质量（QoS）优先级


<details>
<summary>English original</summary>

**The Three Programming Implications**

**1. Zero-copy is your biggest advantage.**
On discrete GPU, `cudaMemcpy` often dominates execution time. On Jetson, skip it:

```cpp
// ── Discrete GPU: mandatory copy ──────────────────────────────
float *h_data = (float*)malloc(size);          // host RAM
float *d_data;
cudaMalloc(&d_data, size);                     // GPU VRAM
cudaMemcpy(d_data, h_data, size, cudaMemcpyHostToDevice);  // PCIe copy (slow!)
kernel<<<grid, block>>>(d_data);
cudaMemcpy(h_data, d_data, size, cudaMemcpyDeviceToHost);  // PCIe copy (slow!)

// ── Jetson: zero-copy with pinned host memory ─────────────────
float *shared;
cudaMallocHost(&shared, size);                 // pinned, in same DRAM
// Both CPU and GPU access 'shared' directly — NO copy needed
fill_data_on_cpu(shared);                      // CPU writes
kernel<<<grid, block>>>(shared);               // GPU reads same memory
cudaDeviceSynchronize();
read_results_on_cpu(shared);                   // CPU reads GPU output
```

**2. Memory bandwidth is your bottleneck — not compute.**
Discrete H100: 3,350 GB/s HBM → 989 TFLOPS. Jetson Orin Nano Super: 102 GB/s LPDDR5 → 67 TOPS (GPU) + ~10 TOPS (DLA) ≈ 77 TOPS total. The compute-to-bandwidth ratio is dramatically different:

```
H100 ridge point:         989 TFLOPS / 3,350 GB/s ≈ 295 FLOP/byte
Orin Nano Super ridge:    67 TOPS / 102 GB/s ≈ 0.66 OP/byte

→ Almost EVERYTHING is memory-bound on Jetson.
→ Tiling, data reuse, and INT8 quantization are not optional — they're mandatory.
```

**3. Memory capacity is precious — 8 GB total.**
On a server, a 7B LLM model takes 14 GB (FP16) in GPU VRAM, leaving 500+ GB system RAM for everything else. On Jetson 8 GB, that same model won't even fit. You must:
- Quantize aggressively (INT8 → 7 GB, INT4 → 3.5 GB)
- Account for OS + camera + CUDA runtime (~2–3 GB)
- Choose models that fit the budget (YOLOv8-N at 3.2M params, not YOLOv8-X at 68M)

**Memory Budget Comparison**

```
Discrete Server (H100 80GB + 512 GB DDR5):
  GPU VRAM: 80 GB  │ System RAM: 512 GB
  ─────────────────│────────────────────
  Model: 40 GB     │ OS: 4 GB
  Activations: 20 GB│ App: 2 GB
  Scratch: 15 GB   │ Datasets: 200 GB
  Free: 5 GB       │ Free: 306 GB

Jetson Orin Nano 8GB (shared):
  ┌──────────────────────────────────┐
  │ 8 GB LPDDR5 (total)             │
  │                                  │
  │ Firmware carveouts:   ~0.4 GB   │
  │ OS + kernel:          ~0.5 GB   │
  │ CMA (camera buffers): ~0.75 GB  │
  │ CUDA runtime:         ~0.3 GB   │
  │ Model (INT8 YOLO):    ~0.1 GB   │
  │ Inference scratch:     ~0.2 GB   │
  │ ─────────────────────────────── │
  │ Free for userspace:    ~5.75 GB │
  └──────────────────────────────────┘
```

**CUDA Memory API Decision Tree for Jetson**

```
What are you allocating?
│
├── Camera frames / video decoder output
│   └── Use DMA-BUF / NvBufSurface (zero-copy from hardware engine)
│
├── Model weights (loaded once, read by GPU)
│   └── cudaMalloc (pinned device memory, fastest GPU access)
│
├── Pre/post-processing buffers (CPU writes, GPU reads, or vice versa)
│   └── cudaMallocHost (pinned, both CPU and GPU access, no copy)
│
├── Prototyping / irregular access pattern
│   └── cudaMallocManaged (automatic migration, but unpredictable latency)
│
└── Temporary GPU-only scratch
    └── cudaMalloc (standard, fastest)
```

> **Key mindset shift:** On discrete GPU, you think "minimize PCIe transfers." On Jetson, you think "minimize total DRAM bandwidth consumption" — because CPU, GPU, camera, and display all share the same ~102 GB/s pipe.

---

**1. T234 SoC Memory Architecture Overview**

The Tegra234 SoC in Orin Nano 8GB uses a **unified memory architecture** — CPU, GPU, DLA, and all accelerators share a single 8GB LPDDR5 pool. There is no discrete GPU memory.

```
                        8GB LPDDR5
                    ┌───────────────┐
                    │               │
       ┌────────────┤  Memory       ├────────────┐
       │            │  Controllers  │            │
       │            │  (MC)         │            │
       │            └───────┬───────┘            │
       │                    │                    │
  ┌────┴────┐         ┌────┴────┐         ┌─────┴────┐
  │ CPU     │         │ GPU     │         │ DLA/ISP  │
  │ A78AE   │         │ Ampere  │         │ NVENC    │
  │ cluster │         │ 1024    │         │ NVDEC    │
  │         │         │ cores   │         │ CSI/VI   │
  └─────────┘         └─────────┘         └──────────┘
```

**Memory Controllers**

T234 has multiple memory controller (MC) channels connected to the LPDDR5 interface. The MC handles:

* Arbitration between CPU, GPU, and all accelerators
* Bandwidth partitioning (configurable via BPMP firmware)
* Quality-of-Service (QoS) priorities for latency-sensitive engines (e.g., display, camera)

</details>

### 统一虚拟内存（UVM）

Jetson 上的 CUDA 支持统一虚拟寻址（UVA）。`cudaMallocManaged()` 返回的指针在 CPU 和 GPU 上都有效 —— runtime 通过缺页和迁移来维护一致性。然而，对延迟敏感的通路（摄像头、推理），用 `cudaMalloc()` 或 NVMM buffer 显式分配可避免迁移开销。

---

## 2. 缓存层级

理解缓存对于优化带宽受限的推理工作负载至关重要。

```
CPU Core (A78AE)
  L1I: 64KB per core (instruction)
  L1D: 64KB per core (data)
  L2:  256KB per core
  L3:  2MB shared (cluster-level)

GPU (Ampere)
  L1 / Shared Memory: per-SM (configurable)
  L2: shared across all SMs
```

### 关键影响

* **CPU L3 很小（2MB）** —— 大的工作集很快溢出到 DRAM。这对 CPU 上的前/后处理有影响。
* **GPU L2** 是共享的 —— 推理 kernel 相互争抢缓存。kernel 分块与 occupancy 直接影响 L2 命中率。
* **CPU 与 GPU 之间没有硬件缓存一致性** —— 这正是需要显式同步（例如 `cudaStreamSynchronize`）的原因，也是相比 CPU-GPU memcpy 更倾向用 NVMM/DMA-BUF 实现零拷贝的原因。

---

## 3. 启动时的内存初始化

在 Orin Nano 上，内存在 Linux 运行之前就已分阶段建立完成。

### MB1 —— DRAM 训练

MB1（从 QSPI NOR 加载）执行 LPDDR5 训练：

* 校准每个 DRAM 通道的时序参数
* 保存训练数据，供后续启动快速启动
* 若启用 ECC 则进行配置

如果 DRAM 训练失败，板子无法启动 —— 无显示、无串口输出。

### MB2 —— carveout 设置

MB2 读取设备树，并为固件处理器预留内存区域（carveout）：

* **BPMP** —— 电源管理固件
* **SPE** —— 安全处理器
* **RCE** —— 摄像头实时引擎
* **OP-TEE** —— 安全世界
* **VPR** —— Video Protected Region（DRM）

这些 carveout 在 kernel 启动前就从 Linux 可见的内存映射中移除。

### UEFI —— 内存映射交接

UEFI 构建 EFI 内存映射，描述哪些区域：

* 可供 Linux 使用（常规内存）
* 保留（固件、carveout）
* ACPI/runtime 服务

Linux 通过 `efi_memmap` 接收该映射，并据此初始化其内存子系统。

### 设备树内存节点

UEFI 加载的 DTB 定义了：

```dts
memory@80000000 {
    device_type = "memory";
    reg = <0x0 0x80000000 0x0 0x70000000>,   /* Region 1 */
          <0x0 0xf0200000 0x0 0x0fe00000>;   /* Region 2 */
};
```

carveout 则单独定义：

```dts
reserved-memory {
    #address-cells = <2>;
    #size-cells = <2>;
    ranges;

    bpmp_carveout: bpmp {
        compatible = "nvidia,bpmp-shmem";
        reg = <0x0 0x40000000 0x0 0x200000>;
        no-map;
    };
};
```

`no-map` 属性意味着 Linux 不能访问这块内存。

如果 DTB 中的 carveout 配置有误，系统可能在 `start_kernel()` 之前崩溃，或者固件处理器会出现异常。

---

## 4. 8GB 实际长什么样

在 Orin Nano 上，DRAM 由 MB1 → MB2 → UEFI → Linux 依次初始化。极早期的启动阶段就会预留内存，**甚至在 Linux 启动之前**。

### 典型的 `/proc/iomem` 布局（概念性）

```
00000000-000fffff : Reserved (Boot ROM, vectors)
00100000-3fffffff : System RAM
40000000-4fffffff : Reserved (VPR, carveouts)
50000000-57ffffff : CMA
58000000-ffffffff : System RAM
```

### 保留区域

| 区域                          | 用途                            |
|------------------------------|---------------------------------|
| VPR (Video Protected Region) | DRM / 安全视频播放               |
| SPE carveout                 | 安全处理器固件                    |
| BPMP carveout                | 电源管理固件                      |
| OP-TEE secure RAM            | 可信执行环境                      |
| CMA                          | 用于摄像头 / GPU 的连续 DMA        |

这些都在 UEFI 加载的**设备树（DTB）**中定义。

### 查看真实内存映射

```bash
# Full memory map
cat /proc/iomem

# Linux-visible memory summary
cat /proc/meminfo

# CMA-specific stats
grep Cma /proc/meminfo
```

Orin Nano 8GB 上 `/proc/meminfo` 的输出示例：

```
MemTotal:        7633536 kB    ← Not 8GB! Carveouts took the rest
CmaTotal:         786432 kB    ← 768MB reserved for CMA
CmaFree:          524288 kB    ← Currently unused CMA
```

8GB 与 `MemTotal` 之间约 400MB 的差值，被固件 carveout、kernel 代码和保留区域占用。

---


<details>
<summary>English original</summary>

**Unified Virtual Memory (UVM)**

CUDA on Jetson supports unified virtual addressing (UVA). A pointer returned by `cudaMallocManaged()` is valid on both CPU and GPU — the runtime handles coherence via page faults and migration. However, for latency-critical paths (camera, inference), explicit allocation with `cudaMalloc()` or NVMM buffers avoids migration overhead.

---

**2. Cache Hierarchy**

Understanding caches is critical for optimizing memory-bound inference workloads.

```
CPU Core (A78AE)
  L1I: 64KB per core (instruction)
  L1D: 64KB per core (data)
  L2:  256KB per core
  L3:  2MB shared (cluster-level)

GPU (Ampere)
  L1 / Shared Memory: per-SM (configurable)
  L2: shared across all SMs
```

**Key Implications**

* **CPU L3 is small (2MB)** — large working sets spill to DRAM quickly. This matters for pre/post-processing on CPU.
* **GPU L2** is shared — inference kernels compete for cache. Kernel tiling and occupancy directly affect L2 hit rates.
* **No hardware cache coherence between CPU and GPU** — this is why explicit synchronization (e.g., `cudaStreamSynchronize`) is required, and why zero-copy via NVMM/DMA-BUF is preferred over CPU-GPU memcpy.

---

**3. Boot-Time Memory Initialization**

On Orin Nano, memory is set up in stages before Linux ever runs.

**MB1 — DRAM Training**

MB1 (loaded from QSPI NOR) performs LPDDR5 training:

* Calibrates timing parameters for each DRAM channel
* Stores training data for fast-boot on subsequent boots
* Configures ECC if enabled

If DRAM training fails, the board does not boot — no display, no serial output.

**MB2 — Carveout Setup**

MB2 reads the device tree and reserves memory regions (carveouts) for firmware processors:

* **BPMP** — power management firmware
* **SPE** — safety processor
* **RCE** — camera real-time engine
* **OP-TEE** — secure world
* **VPR** — Video Protected Region (DRM)

These carveouts are removed from the Linux-visible memory map before the kernel starts.

**UEFI — Memory Map Handoff**

UEFI constructs the EFI memory map describing which regions are:

* Usable by Linux (conventional memory)
* Reserved (firmware, carveouts)
* ACPI/runtime services

Linux receives this map via `efi_memmap` and initializes its memory subsystem accordingly.

**Device Tree Memory Nodes**

The DTB loaded by UEFI defines:

```dts
memory@80000000 {
    device_type = "memory";
    reg = <0x0 0x80000000 0x0 0x70000000>,   /* Region 1 */
          <0x0 0xf0200000 0x0 0x0fe00000>;   /* Region 2 */
};
```

Carveouts are defined separately:

```dts
reserved-memory {
    #address-cells = <2>;
    #size-cells = <2>;
    ranges;

    bpmp_carveout: bpmp {
        compatible = "nvidia,bpmp-shmem";
        reg = <0x0 0x40000000 0x0 0x200000>;
        no-map;
    };
};
```

The `no-map` property means Linux cannot touch this memory.

If DTB carveouts are wrong, the system may crash before `start_kernel()` or firmware processors will malfunction.

---

**4. What the 8GB Really Looks Like**

On Orin Nano, DRAM is initialized by MB1 → MB2 → UEFI → Linux. Very early boot reserves memory **before Linux even starts**.

**Typical `/proc/iomem` Layout (Conceptual)**

```
00000000-000fffff : Reserved (Boot ROM, vectors)
00100000-3fffffff : System RAM
40000000-4fffffff : Reserved (VPR, carveouts)
50000000-57ffffff : CMA
58000000-ffffffff : System RAM
```

**Reserved Regions**

| Region                       | Purpose                         |
|------------------------------|---------------------------------|
| VPR (Video Protected Region) | DRM / secure video playback     |
| SPE carveout                 | Safety processor firmware       |
| BPMP carveout                | Power management firmware       |
| OP-TEE secure RAM            | Trusted execution environment   |
| CMA                          | Contiguous DMA for camera / GPU |

These are defined in the **Device Tree (DTB)** loaded by UEFI.

**Inspecting the Real Memory Map**

```bash
# Full memory map
cat /proc/iomem

# Linux-visible memory summary
cat /proc/meminfo

# CMA-specific stats
grep Cma /proc/meminfo
```

Example `/proc/meminfo` output on Orin Nano 8GB:

```
MemTotal:        7633536 kB    ← Not 8GB! Carveouts took the rest
CmaTotal:         786432 kB    ← 768MB reserved for CMA
CmaFree:          524288 kB    ← Currently unused CMA
```

The ~400MB difference between 8GB and `MemTotal` is consumed by firmware carveouts, kernel code, and reserved regions.

---

</details>

## 5. Linux 内存区域

Linux 把物理内存划分为若干区域：

| Zone        | Address Range    | Purpose                           |
|-------------|------------------|-----------------------------------|
| ZONE_DMA    | 0 – 16MB         | 传统 ISA DMA（基本已弃用）    |
| ZONE_DMA32  | 0 – 4GB          | 32 位可寻址 DMA            |
| ZONE_NORMAL | 4GB 以上        | 通用分配        |

Orin Nano 主要使用 **ZONE_NORMAL**，因为 LPDDR5 的大部分地址空间都映射在 4GB 边界之上。

可以查看区域状态：

```bash
cat /proc/zoneinfo
```

关键字段：

* `free` —— 该区域当前空闲的页数
* `min` / `low` / `high` —— 触发回收的水位线
* `nr_free_pages` —— 所有 order 上的空闲总量

当 `free` 降到 `min` 以下时，kernel 会开始激进回收（kswapd、direct reclaim）。在 Jetson 上，配合大块 camera buffer，这会引发延迟尖峰。

---

## 6. Buddy Allocator 内部机制

Linux 的 buddy allocator 以 2 的幂为单位管理空闲页：

```
Order 0 =   4KB  (1 page)
Order 1 =   8KB  (2 pages)
Order 2 =  16KB  (4 pages)
Order 3 =  32KB  (8 pages)
...
Order 9 =   2MB  (512 pages)
Order 10 =  4MB  (1024 pages)
```

### 分配如何工作

当驱动请求 64KB（order 4）时：

1. 检查 order-4 空闲链表
2. 若为空，则拆分一个更高 order 的块（例如 order 5 → 两个 order 4 块）
3. 返回其中一个块，另一个保留为空闲

### 释放如何工作

当一个块被释放时：

1. 检查相邻的 "buddy" 块是否也空闲
2. 若是，则合并为更高 order 的块
3. 沿链向上重复

### 检查碎片化

```bash
cat /proc/buddyinfo
```

输出示例：

```
Node 0, zone   Normal  1024  512  256  128  64  32  16  8  4  2  1
```

每个数字是该 order（0 到 10）上空闲块的数量。如果高 order 列显示为 `0`，说明系统已碎片化 —— 即使空闲内存总量足够，大块连续分配也会失败。

### 这在 Jetson 上为何重要

Camera 和 GPU 驱动需要大块连续分配。一个空闲内存充足、但没有高 order 块的碎片化系统会分配 buffer 失败 —— 这正是 CMA 存在的原因。

---

<a id="7-cma--contiguous-memory-allocator"></a>
## 7. CMA —— 连续内存分配器

CMA **不是**独立的内存。它是：

> 位于普通 RAM 内部的一块预留可迁移区域，Linux 可借此保证物理连续的分配。

Linux 把 CMA 页标记为 **MIGRATE_CMA**。当这些页不用于 DMA 时，它们存放可迁移数据（例如用户态页）。当需要连续分配时，kernel 会把这些页迁移到别处。

### 分配流程

当驱动调用 `dma_alloc_contiguous()` 时：

1. 在 CMA 区域内寻找空闲页
2. 用 buddy allocator 定位一个连续块
3. 如有必要则做内存规整（把可迁移页迁走）
4. 把该分配映射进 SMMU，供发起请求的设备使用
5. 返回 DMA 地址（IOVA）和内核虚拟地址

### 长时间运行的系统为何会失败

随着时间推移，内存碎片不断累积：

```
CMA region:
[used][free][used][free][used][free][used]
```

即使 CMA 空闲总量充足，也没有任何单个连续块足够大。camera 会失败并报：

```
Failed to allocate buffer
```

这是嵌入式量产中的典型问题。缓解手段：

* 按你的工作负载**合理设定 CMA 大小**（见下一节）
* **在启动时预分配 buffer** 并复用
* 在量产环境中**监控 CMA 碎片化**（见第 16 节）
* **使用 NVMM buffer pool**，而不是逐帧分配/释放

---

## 8. 如何调整 CMA 大小

### 方法 1：内核命令行

编辑 `/boot/extlinux/extlinux.conf`：

```
APPEND ... cma=1024M
```

这会把 CMA 设为 1GB。

### 方法 2：设备树

```dts
reserved-memory {
    linux,cma {
        compatible = "shared-dma-pool";
        reusable;
        size = <0x0 0x40000000>;   /* 1GB */
        linux,cma-default;
    };
};
```

修改后重新烧写 DTB。

### 容量规划建议

| Workload                      | Recommended CMA |
|-------------------------------|-----------------|
| 单 camera、轻量推理 | 256–512MB       |
| 单个 4K camera + TensorRT   | 512–768MB       |
| 多 camera AI 流水线       | 768MB–1GB       |
| 多 camera + 大模型    | 1–1.5GB         |

权衡：

* CMA 过大 = 留给用户态、模型权重和 CUDA 的内存变少
* CMA 过小 = 负载下 camera buffer 分配失败
* 量产的多 camera AI 工作负载常用 768MB–1GB

### 修改后验证

```bash
grep Cma /proc/meminfo
```

确认 `CmaTotal` 与你配置的大小一致。

---

## 9. SMMU（IOMMU）—— 真正的地址转换路径

Orin Nano 使用 **ARM SMMU v2**（System Memory Management Unit）。每个执行 DMA 的设备都要经过 SMMU。


<details>
<summary>English original</summary>

**5. Linux Memory Zones**

Linux divides physical memory into zones:

| Zone        | Address Range    | Purpose                           |
|-------------|------------------|-----------------------------------|
| ZONE_DMA    | 0 – 16MB         | Legacy ISA DMA (mostly unused)    |
| ZONE_DMA32  | 0 – 4GB          | 32-bit addressable DMA            |
| ZONE_NORMAL | Above 4GB        | General-purpose allocations        |

Orin Nano mostly uses **ZONE_NORMAL** since LPDDR5 is mapped above the 4GB boundary for most of the address space.

You can inspect zone state:

```bash
cat /proc/zoneinfo
```

Key fields:

* `free` — pages currently free in this zone
* `min` / `low` / `high` — watermarks that trigger reclaim
* `nr_free_pages` — total free across all orders

When `free` drops below `min`, the kernel starts aggressive reclaim (kswapd, direct reclaim). On Jetson with large camera buffers, this can cause latency spikes.

---

**6. Buddy Allocator Internals**

The Linux buddy allocator manages free pages in powers of two:

```
Order 0 =   4KB  (1 page)
Order 1 =   8KB  (2 pages)
Order 2 =  16KB  (4 pages)
Order 3 =  32KB  (8 pages)
...
Order 9 =   2MB  (512 pages)
Order 10 =  4MB  (1024 pages)
```

**How Allocation Works**

When a driver requests 64KB (order 4):

1. Check the order-4 free list
2. If empty, split a higher-order block (e.g., order 5 → two order 4 blocks)
3. Return one block, keep the other as free

**How Freeing Works**

When a block is freed:

1. Check if the adjacent "buddy" block is also free
2. If yes, merge into a higher-order block
3. Repeat up the chain

**Inspecting Fragmentation**

```bash
cat /proc/buddyinfo
```

Example output:

```
Node 0, zone   Normal  1024  512  256  128  64  32  16  8  4  2  1
```

Each number is the count of free blocks at that order (0 through 10). If high-order columns show `0`, the system is fragmented — large contiguous allocations will fail even if total free memory is sufficient.

**Why This Matters on Jetson**

Camera and GPU drivers need large contiguous allocations. A fragmented system with plenty of free memory but no high-order blocks will fail to allocate buffers — this is why CMA exists.

---

<a id="7-cma--contiguous-memory-allocator"></a>
**7. CMA — Contiguous Memory Allocator**

CMA is **not** separate memory. It is:

> A reserved movable region inside normal RAM where Linux can guarantee physically contiguous allocations.

Linux marks CMA pages as **MIGRATE_CMA**. When not used for DMA, these pages hold movable data (e.g., userspace pages). When a contiguous allocation is needed, the kernel migrates those pages elsewhere.

**Allocation Flow**

When a driver calls `dma_alloc_contiguous()`:

1. Find free pages inside the CMA region
2. Use the buddy allocator to locate a contiguous block
3. Compact memory if needed (migrate movable pages out of the way)
4. Map the allocation into the SMMU for the requesting device
5. Return the DMA address (IOVA) and kernel virtual address

**Why Long-Running Systems Fail**

Over time, memory fragmentation builds up:

```
CMA region:
[used][free][used][free][used][free][used]
```

Even if total free CMA is sufficient, no single contiguous chunk is large enough. The camera fails with:

```
Failed to allocate buffer
```

This is a classic embedded production issue. Mitigations:

* **Right-size CMA** for your workload (see next section)
* **Pre-allocate buffers at boot** and reuse them
* **Monitor CMA fragmentation** in production (see Section 16)
* **Use NVMM buffer pools** rather than allocating/freeing per-frame

---

**8. How to Resize CMA**

**Method 1: Kernel Command Line**

Edit `/boot/extlinux/extlinux.conf`:

```
APPEND ... cma=1024M
```

This sets CMA to 1GB.

**Method 2: Device Tree**

```dts
reserved-memory {
    linux,cma {
        compatible = "shared-dma-pool";
        reusable;
        size = <0x0 0x40000000>;   /* 1GB */
        linux,cma-default;
    };
};
```

Reflash DTB after modification.

**Sizing Guidelines**

| Workload                      | Recommended CMA |
|-------------------------------|-----------------|
| Single camera, light inference | 256–512MB       |
| Single 4K camera + TensorRT   | 512–768MB       |
| Multi-camera AI pipeline       | 768MB–1GB       |
| Multi-camera + large models    | 1–1.5GB         |

Trade-offs:

* Too large CMA = less memory for userspace, model weights, and CUDA
* Too small CMA = camera buffer allocation failures under load
* 768MB–1GB is common for production multi-camera AI workloads

**Verify After Change**

```bash
grep Cma /proc/meminfo
```

Confirm `CmaTotal` matches your configured size.

---

**9. SMMU (IOMMU) — Real Translation Path**

Orin Nano uses **ARM SMMU v2** (System Memory Management Unit). Every device that performs DMA goes through the SMMU.

</details>

### 转换流程

```
Device (GPU, CSI, ISP, etc.)
   ↓
IOVA (I/O Virtual Address — what the device sees)
   ↓
SMMU page tables (owned by Linux kernel)
   ↓
Physical DRAM (actual memory location)
```

设备**从不直接看到物理 RAM**。内核：

1. 分配内存（来自 CMA 或普通页）
2. 将这些物理页映射到目标设备的 SMMU 页表中
3. 给设备一个 IOVA
4. 设备使用该 IOVA 执行 DMA

### 为什么这种架构如此强大

* **设备隔离** — 行为异常的设备无法破坏其他设备的内存
* **无越权 DMA** — 未映射的访问会触发 SMMU fault（被捕获并记录）
* **共享缓冲区** — 多个设备可以把同一批物理页映射到不同的 IOVA
* **scatter-gather 表现为连续** — 物理上分散的页在设备看来是连续的

这就是 GPU 与摄像头能够安全共享缓冲区而无需拷贝的原因。

### SMMU 页表结构

ARM SMMU 使用与 CPU MMU 类似的多级页表：

```
Stream Table Entry (per device/stream ID)
   ↓
Context Descriptor
   ↓
Level 1 Page Table (covers large VA range)
   ↓
Level 2 Page Table
   ↓
Level 3 Page Table (4KB granule)
   ↓
Physical Page
```

每个设备都有自己的流 ID 和自己的一组页表，从而提供完全隔离。

### SMMU fault 调试

当设备访问未映射的 IOVA 时：

```bash
dmesg | grep smmu
```

fault 示例：

```
arm-smmu 12000000.iommu: Unhandled context fault: iova=0x1234000, fsynr=0x11
```

这表示设备试图访问 IOVA `0x1234000`，但不存在对应的映射。常见原因：

* 缓冲区已释放，而设备仍在引用它
* DMA-BUF 导入错误
* 驱动中 IOVA 映射的 bug

---

## 10. Camera → ISP → CUDA 零拷贝路径

这是 Orin Nano 上实时 AI 视觉的关键数据通路。

### 步骤 1 — 传感器采集

```
MIPI CSI-2 camera sensor
    ↓ (serial lanes)
NVCSI controller (deserializes)
    ↓
VI (Video Input — captures frames)
    ↓
ISP (Image Signal Processor — debayer, denoise, tone-map)
```

### 步骤 2 — ISP 输出到 CMA

ISP 把处理后的帧写入 **CMA 内存**。之所以需要 CMA，是因为 ISP 进行 DMA 时需要物理连续的缓冲区。ISP 硬件不具备 scatter-gather 能力。

### 步骤 3 — DMA-BUF 导出

V4L2 驱动把该缓冲区导出为 **DMA-BUF** 文件描述符。一旦导出，同一段物理内存就可以被任何支持 DMA-BUF 的消费者导入：

* CUDA（通过 `cudaExternalMemory`）
* NvBufSurface（NVIDIA 多媒体缓冲区 API）
* GStreamer（通过 `nvv4l2camerasrc`）
* DeepStream（通过 source bin）

### 步骤 4 — GPU 经 SMMU 访问

GPU 导入该 DMA-BUF，并通过自己的 SMMU 上下文将其映射：

```
GPU IOVA → SMMU → Same physical CMA pages that ISP wrote to
```

**没有 memcpy。** 这是真正的零拷贝。GPU 读取的正是 ISP 写入的那段物理内存，只是通过不同的 SMMU 流映射到了不同的虚拟地址。

### 完整的零拷贝流水线

```
Sensor → NVCSI → VI → ISP → CMA buffer
                                ↓ (DMA-BUF export)
                          ┌─────┴─────┐
                          │           │
                     GPU (CUDA)   Display
                     via SMMU     via SMMU
                          │
                     TensorRT
                     inference
```

这就是实时 4K 视觉在 Orin Nano 上高效运行、且不会打满内存带宽的方式。

### 内存流程总结

| 阶段 | 内存类型 | 分配方 |
|-------------|--------------------|---------------------|
| CSI/VI | CMA（连续） | V4L2 / nvargus |
| ISP 输出 | CMA（同一缓冲区） | 复用自 VI |
| GPU 输入 | DMA-BUF 导入 | CUDA / NvBufSurface |
| GPU 输出 | CUDA 设备内存 | cudaMalloc |

---

## 11. GPU 内存管理

### NVMM（NVIDIA Multimedia Memory）

NVMM 是 NVIDIA 面向多媒体流水线（摄像头、编码、decode）的缓冲区管理层。关键特性：

* 缓冲区从 CMA 或 carve-out 内存分配
* 可被所有 NVIDIA 引擎访问（ISP、GPU、VIC、NVENC、NVDEC）
* 引擎之间通过 DMA-BUF 实现零拷贝
* 由 `libnvbufsurface` 管理

NVMM 是 GStreamer 与 DeepStream 零拷贝流水线的基础。


<details>
<summary>English original</summary>

**Translation Flow**

```
Device (GPU, CSI, ISP, etc.)
   ↓
IOVA (I/O Virtual Address — what the device sees)
   ↓
SMMU page tables (owned by Linux kernel)
   ↓
Physical DRAM (actual memory location)
```

Devices **never see physical RAM directly**. The kernel:

1. Allocates memory (from CMA or normal pages)
2. Maps those physical pages into the SMMU page tables for the target device
3. Gives the device an IOVA
4. The device performs DMA using the IOVA

**Why This Architecture Is Powerful**

* **Device isolation** — a misbehaving device cannot corrupt another device's memory
* **No rogue DMA** — unmapped accesses cause SMMU faults (caught and logged)
* **Shared buffers** — multiple devices can map the same physical pages at different IOVAs
* **Scatter-gather as contiguous** — physically scattered pages appear contiguous to the device

This is why GPU and camera can share buffers safely without copies.

**SMMU Page Table Structure**

ARM SMMU uses multi-level page tables similar to CPU MMU:

```
Stream Table Entry (per device/stream ID)
   ↓
Context Descriptor
   ↓
Level 1 Page Table (covers large VA range)
   ↓
Level 2 Page Table
   ↓
Level 3 Page Table (4KB granule)
   ↓
Physical Page
```

Each device has its own stream ID and its own set of page tables, providing full isolation.

**SMMU Fault Debugging**

When a device accesses an unmapped IOVA:

```bash
dmesg | grep smmu
```

Example fault:

```
arm-smmu 12000000.iommu: Unhandled context fault: iova=0x1234000, fsynr=0x11
```

This means the device tried to access IOVA `0x1234000` but no mapping existed. Common causes:

* Buffer freed while device still referencing it
* Incorrect DMA-BUF import
* Driver bug in IOVA mapping

---

**10. Camera → ISP → CUDA Zero-Copy Path**

This is the critical data path for real-time AI vision on Orin Nano.

**Step 1 — Sensor Capture**

```
MIPI CSI-2 camera sensor
    ↓ (serial lanes)
NVCSI controller (deserializes)
    ↓
VI (Video Input — captures frames)
    ↓
ISP (Image Signal Processor — debayer, denoise, tone-map)
```

**Step 2 — ISP Output to CMA**

ISP writes processed frames into **CMA memory**. CMA is required because ISP needs physically contiguous buffers for DMA. The ISP hardware has no scatter-gather capability.

**Step 3 — DMA-BUF Export**

The V4L2 driver exports the buffer as a **DMA-BUF** file descriptor. Once exported, the same physical memory can be imported by any DMA-BUF-aware consumer:

* CUDA (via `cudaExternalMemory`)
* NvBufSurface (NVIDIA multimedia buffer API)
* GStreamer (via `nvv4l2camerasrc`)
* DeepStream (via source bin)

**Step 4 — GPU Access via SMMU**

The GPU imports the DMA-BUF and maps it through its own SMMU context:

```
GPU IOVA → SMMU → Same physical CMA pages that ISP wrote to
```

**No memcpy.** This is true zero-copy. The GPU reads the exact same physical memory the ISP wrote, just mapped at a different virtual address through a different SMMU stream.

**Full Zero-Copy Pipeline**

```
Sensor → NVCSI → VI → ISP → CMA buffer
                                ↓ (DMA-BUF export)
                          ┌─────┴─────┐
                          │           │
                     GPU (CUDA)   Display
                     via SMMU     via SMMU
                          │
                     TensorRT
                     inference
```

This is how real-time 4K vision runs efficiently on Orin Nano without saturating memory bandwidth.

**Memory Flow Summary**

| Stage       | Memory Type        | Allocated By        |
|-------------|--------------------|---------------------|
| CSI/VI      | CMA (contiguous)   | V4L2 / nvargus      |
| ISP output  | CMA (same buffer)  | Reused from VI      |
| GPU input   | DMA-BUF import     | CUDA / NvBufSurface |
| GPU output  | CUDA device memory | cudaMalloc          |

---

**11. GPU Memory Management**

**NVMM (NVIDIA Multimedia Memory)**

NVMM is NVIDIA's buffer management layer for multimedia pipelines (camera, encode, decode). Key properties:

* Buffers are allocated from CMA or carved-out memory
* Accessible by all NVIDIA engines (ISP, GPU, VIC, NVENC, NVDEC)
* Zero-copy between engines via DMA-BUF
* Managed by `libnvbufsurface`

NVMM is the foundation for GStreamer and DeepStream zero-copy pipelines.

</details>

### Jetson 上的 CUDA 内存

在 Jetson（统一内存架构）上，CUDA 内存类型的行为与独立 GPU 上不同：

| API                    | 内存所在位置        | CPU 可访问？    | GPU 可访问？    | 备注                           |
|------------------------|---------------------|-----------------|-----------------|--------------------------------|
| `cudaMalloc`           | DRAM（pinned）       | 否              | 是             | 仅 GPU 访问时最快    |
| `cudaMallocManaged`    | DRAM（migrating）    | 是              | 是             | 基于 page fault，延迟更高 |
| `cudaMallocHost`       | DRAM（pinned）       | 是              | 是             | 适合 CPU↔GPU 共享数据   |
| DMA-BUF import         | CMA / NVMM          | 是              | 是             | 从摄像头/解码器零拷贝  |

对于推理流水线，优先使用：

* **DMA-BUF import** 处理摄像头帧（零拷贝）
* **`cudaMalloc`** 用于模型权重和临时内存
* **`cudaMallocHost`** 用于与 GPU 共享的 CPU 前/后处理缓冲区

在延迟敏感路径中避免使用 `cudaMallocManaged` —— page fault 会增加不可预测的延迟。

### GPU 内存压力

监控 GPU 内存使用：

```bash
# Total GPU memory (same as system memory on Jetson)
nvidia-smi  # or tegrastats

# Detailed CUDA memory
python3 -c "import torch; print(torch.cuda.memory_summary())"
```

在 Orin Nano 8GB 上，GPU 与 CPU 争用同一块 8GB 内存。在 CUDA 中加载大模型会减少摄像头缓冲区、CMA 和 OS 可用的内存。内存规划必不可少。

---

## 12. DLA 内存路径

> **深入解析：** 如需 DLA 的完整内容 —— 硬件架构（MAC/SDP/PDP/CDP）、TensorRT 集成、支持的 layer、多引擎调度、性能剖析与生产部署模式 —— 参见 [**Orin Nano DLA Deep Dive**](../04-Orin-Nano-DLA深入解析/Guide.md)。

Orin Nano 8GB 包含 **1 个 DLA（Deep Learning Accelerator，深度学习加速器）**，算力最高 10 TOPS（INT8）。

### DLA 如何访问内存

DLA 使用同一统一 DRAM，但拥有自己的 SMMU stream：

```
DLA engine
   ↓
DLA IOVA
   ↓
SMMU (DLA stream context)
   ↓
Physical DRAM
```

### TensorRT 的 DLA 执行

当 TensorRT 在 DLA 上运行 layer 时：

1. TensorRT 在 DRAM 中分配输入/输出缓冲区
2. 将它们映射到 DLA 的 SMMU 上下文
3. 配置 DLA 寄存器（layer 配置、权重指针、I/O 指针）
4. DLA 执行该 layer，从 DRAM 读取权重和输入，将输出写入 DRAM
5. 若下一个 layer 在 GPU 上运行，同一输出缓冲区会映射到 GPU 的 SMMU —— 零拷贝交接

### DLA 与 GPU 的内存权衡

| 方面          | GPU                        | DLA                         |
|-----------------|----------------------------|-----------------------------|
| Memory bandwidth | 高（与 CPU 共享）    | 较低（端口受限）       |
| 精度       | FP32、FP16、INT8           | 仅 FP16、INT8             |
| layer 支持   | 全部 TensorRT layer        | 子集（conv、pool 等）   |
| 功耗          | 较高                     | 每 TOPS 低得多         |

在 DLA 上运行 layer 可让 GPU 腾出来处理其他任务并降低功耗 —— 这对边缘/电池供电系统至关重要。

---

## 13. TensorRT 引擎内存管理

TensorRT 是 Jetson 上的主要推理引擎。理解它如何分配和使用内存，对于在 8 GB 预算内装下模型至关重要。

### TensorRT 如何使用内存

```
TensorRT engine lifecycle:

1. Build phase (on workstation or Jetson):
   Model (ONNX/UFF) → TensorRT optimizer → serialized engine (.engine file)

2. Load phase (on Jetson at startup):
   Read .engine from disk → deserialize → allocate:
     ├── Weight memory:     model weights (pinned, read-only after load)
     ├── Activation memory: intermediate layer outputs (reused across layers)
     ├── Workspace memory:  scratch space for conv/GEMM algorithms
     └── I/O buffers:       input and output tensors

3. Inference phase (per frame):
   Write input → execute() → read output
   No new allocations — everything is preallocated
```


<details>
<summary>English original</summary>

**CUDA Memory on Jetson**

On Jetson (unified memory architecture), CUDA memory types behave differently than on discrete GPUs:

| API                    | Where Memory Lives  | CPU Accessible? | GPU Accessible? | Notes                          |
|------------------------|---------------------|-----------------|-----------------|--------------------------------|
| `cudaMalloc`           | DRAM (pinned)       | No              | Yes             | Fastest for GPU-only access    |
| `cudaMallocManaged`    | DRAM (migrating)    | Yes             | Yes             | Page-fault based, higher latency |
| `cudaMallocHost`       | DRAM (pinned)       | Yes             | Yes             | Good for CPU↔GPU shared data   |
| DMA-BUF import         | CMA / NVMM          | Yes             | Yes             | Zero-copy from camera/decoder  |

For inference pipelines, prefer:

* **DMA-BUF import** for camera frames (zero-copy)
* **`cudaMalloc`** for model weights and scratch memory
* **`cudaMallocHost`** for CPU pre/post-processing buffers shared with GPU

Avoid `cudaMallocManaged` in latency-critical paths — page faults add unpredictable latency.

**GPU Memory Pressure**

Monitor GPU memory usage:

```bash
# Total GPU memory (same as system memory on Jetson)
nvidia-smi  # or tegrastats

# Detailed CUDA memory
python3 -c "import torch; print(torch.cuda.memory_summary())"
```

On Orin Nano 8GB, GPU and CPU compete for the same 8GB. A large model loaded in CUDA reduces memory available for camera buffers, CMA, and OS. Memory planning is essential.

---

**12. DLA Memory Path**

> **Deep dive:** For full DLA coverage — hardware architecture (MAC/SDP/PDP/CDP), TensorRT integration, supported layers, multi-engine scheduling, profiling, and production deployment patterns — see [**Orin Nano DLA Deep Dive**](../04-Orin-Nano-DLA深入解析/Guide.md).

Orin Nano 8GB includes **1 DLA (Deep Learning Accelerator)** capable of up to 10 TOPS (INT8).

**How DLA Accesses Memory**

DLA uses the same unified DRAM but has its own SMMU stream:

```
DLA engine
   ↓
DLA IOVA
   ↓
SMMU (DLA stream context)
   ↓
Physical DRAM
```

**TensorRT DLA Execution**

When TensorRT runs layers on DLA:

1. TensorRT allocates input/output buffers in DRAM
2. Maps them into DLA's SMMU context
3. Programs DLA registers (layer config, weights pointer, I/O pointers)
4. DLA executes the layer, reading weights and input from DRAM, writing output to DRAM
5. If next layer runs on GPU, the same output buffer is mapped into GPU's SMMU — zero-copy handoff

**DLA vs GPU Memory Trade-offs**

| Aspect          | GPU                        | DLA                         |
|-----------------|----------------------------|-----------------------------|
| Memory bandwidth | High (shared with CPU)    | Lower (limited ports)       |
| Precision       | FP32, FP16, INT8           | FP16, INT8 only             |
| Layer support   | All TensorRT layers        | Subset (conv, pool, etc.)   |
| Power           | Higher                     | Much lower per TOPS         |

Running layers on DLA frees GPU for other tasks and reduces power consumption — critical for edge/battery systems.

---

**13. TensorRT Engine Memory Management**

TensorRT is the primary inference engine on Jetson. Understanding how it allocates and uses memory is essential for fitting models into the 8 GB budget.

**How TensorRT Uses Memory**

```
TensorRT engine lifecycle:

1. Build phase (on workstation or Jetson):
   Model (ONNX/UFF) → TensorRT optimizer → serialized engine (.engine file)

2. Load phase (on Jetson at startup):
   Read .engine from disk → deserialize → allocate:
     ├── Weight memory:     model weights (pinned, read-only after load)
     ├── Activation memory: intermediate layer outputs (reused across layers)
     ├── Workspace memory:  scratch space for conv/GEMM algorithms
     └── I/O buffers:       input and output tensors

3. Inference phase (per frame):
   Write input → execute() → read output
   No new allocations — everything is preallocated
```

</details>

### 典型模型的内存构成

```
YOLOv8-S (INT8, 640×640 input, batch=1):

  Component              Memory      Notes
  ──────────────────────────────────────────────────
  Weights (INT8)          ~5 MB      Quantized from ~22 MB FP32
  Activation buffers     ~12 MB      Largest intermediate tensor
  Workspace              ~30 MB      Convolution algorithm scratch
  I/O buffers             ~2 MB      Input image + output detections
  ──────────────────────────────────────────────────
  Total                  ~49 MB      Fits easily

ResNet-50 (FP16, 224×224, batch=1):

  Weights (FP16)         ~48 MB
  Activation buffers     ~25 MB
  Workspace              ~50 MB
  I/O buffers             ~1 MB
  ──────────────────────────────────────────────────
  Total                 ~124 MB

Llama 3.2 3B (INT4-AWQ, via TensorRT-LLM):

  Weights (INT4)       ~1500 MB      3B × 0.5 bytes
  KV cache (INT8)       ~110 MB      26 layers × 8 heads × 128 dim × 2048 ctx
  Activation buffers    ~200 MB      Attention + FFN intermediates
  Workspace             ~100 MB
  ──────────────────────────────────────────────────
  Total               ~1910 MB      Fits, but consumes ~2 GB of 5.5 GB available
```

### 工作区内存 —— 隐形的消耗者

TensorRT 在构建期会尝试多种卷积算法并挑出最快的那个。更快的算法往往需要更大的工作区。这个取舍由你控制：

```cpp
config->setMemoryPoolLimit(MemoryPoolType::kWORKSPACE, 64 << 20);  // 64 MB max
// Larger workspace = TensorRT can try faster algorithms
// Smaller workspace = less memory used, possibly slower algorithms
```

**Jetson 建议：** 将工作区设为 64–128 MB。在 Orin Nano 较小的 SM 上，设得更高基本没有收益。

### 激活值内存复用

TensorRT 会跨 layer 复用激活值 buffer。如果 Layer 5 的数据已不再需要，它的输出 buffer 就可以被 Layer 10 的输出复用。

```
Layer 1 → [Buffer A] → Layer 2 → [Buffer B] → Layer 3 → [Buffer A] (reused!)
                                                           ↑
                                              Layer 1's data no longer needed
```

这就是为什么 TensorRT 的内存占用远低于朴素的 PyTorch 推理（后者会把所有中间张量一直保留到 backward pass）。

### 查看 TensorRT 内存占用

```python
import tensorrt as trt

# After building engine
engine = runtime.deserialize_cuda_engine(engine_data)

# Check memory
print(f"Device memory: {engine.device_memory_size / 1024**2:.1f} MB")
print(f"Layers: {engine.num_layers}")
print(f"Max batch: {engine.max_batch_size}")

# Per-layer memory (advanced)
inspector = engine.create_engine_inspector()
print(inspector.get_engine_information(trt.LayerInformationFormat.JSON))
```

---

## 14. 多模型与多引擎推理

量产的 Jetson 系统常常同时运行多个 AI 模型：目标检测 + 分类 + 跟踪，或检测 + 分割 + 大语言模型。

### 多模型流水线的内存规划

```
Example: Autonomous robot vision pipeline

  Camera → YOLOv8-N (detect) → ResNet-18 (classify) → DeepSORT (track)
                  ↓                     ↓                    ↓
              ~30 MB               ~25 MB                ~15 MB
              GPU + DLA            GPU                   CPU

Pipeline memory budget:
  Model weights:      30 + 25 + 15         = ~70 MB
  Activation buffers: 12 + 8 + 5           = ~25 MB
  Workspace:          30 + 20 + 0          = ~50 MB
  Camera buffers:     4 frames × 3 MB      = ~12 MB
  ───────────────────────────────────────────────────
  Total AI pipeline:                        ~157 MB

  + OS/kernel/CMA/CUDA overhead:           ~2500 MB
  ───────────────────────────────────────────────────
  Remaining from 8 GB:                     ~5343 MB  ← plenty of room
```

### GPU + DLA 拆分 —— 最大吞吐

让检测跑在 DLA 上、分类跑在 GPU 上即可实现重叠 —— 两个引擎在同一 DRAM 上并行执行：

```
Timeline (overlapped):

  Frame N:   │ DLA: YOLO detect ──────│
             │ GPU: (idle)            │ GPU: ResNet classify ──│
             │                         │                        │
  Frame N+1: │                    DLA: YOLO detect ──────│     │
             │                         │ GPU: ResNet classify ──│

DLA and GPU read different parts of DRAM simultaneously.
Memory controller arbitrates bandwidth between them.
```

**关键约束：** DLA 与 GPU 争抢同一份约 102 GB/s 的 DRAM 带宽。如果两者都跑满带宽，总吞吐会下降。用 `tegrastats` 监控：

```bash
tegrastats --interval 500
# Look for: GR3D_FREQ (GPU utilization), EMC_FREQ (memory clock)
# If EMC is at 100%, you're bandwidth-limited — reduce model size or batch
```


<details>
<summary>English original</summary>

**Memory Breakdown for a Typical Model**

```
YOLOv8-S (INT8, 640×640 input, batch=1):

  Component              Memory      Notes
  ──────────────────────────────────────────────────
  Weights (INT8)          ~5 MB      Quantized from ~22 MB FP32
  Activation buffers     ~12 MB      Largest intermediate tensor
  Workspace              ~30 MB      Convolution algorithm scratch
  I/O buffers             ~2 MB      Input image + output detections
  ──────────────────────────────────────────────────
  Total                  ~49 MB      Fits easily

ResNet-50 (FP16, 224×224, batch=1):

  Weights (FP16)         ~48 MB
  Activation buffers     ~25 MB
  Workspace              ~50 MB
  I/O buffers             ~1 MB
  ──────────────────────────────────────────────────
  Total                 ~124 MB

Llama 3.2 3B (INT4-AWQ, via TensorRT-LLM):

  Weights (INT4)       ~1500 MB      3B × 0.5 bytes
  KV cache (INT8)       ~110 MB      26 layers × 8 heads × 128 dim × 2048 ctx
  Activation buffers    ~200 MB      Attention + FFN intermediates
  Workspace             ~100 MB
  ──────────────────────────────────────────────────
  Total               ~1910 MB      Fits, but consumes ~2 GB of 5.5 GB available
```

**Workspace Memory — The Hidden Consumer**

TensorRT tries multiple convolution algorithms during build and picks the fastest. Faster algorithms often need more workspace. You control the trade-off:

```cpp
config->setMemoryPoolLimit(MemoryPoolType::kWORKSPACE, 64 << 20);  // 64 MB max
// Larger workspace = TensorRT can try faster algorithms
// Smaller workspace = less memory used, possibly slower algorithms
```

**Jetson recommendation:** Set workspace to 64–128 MB. Going higher rarely helps on Orin Nano's smaller SMs.

**Activation Memory Reuse**

TensorRT reuses activation buffers across layers. Layer 5's output buffer can be reused for Layer 10's output if Layer 5's data is no longer needed.

```
Layer 1 → [Buffer A] → Layer 2 → [Buffer B] → Layer 3 → [Buffer A] (reused!)
                                                           ↑
                                              Layer 1's data no longer needed
```

This is why TensorRT uses far less memory than naive PyTorch inference (which keeps all intermediate tensors alive until backward pass).

**Inspecting TensorRT Memory Usage**

```python
import tensorrt as trt

# After building engine
engine = runtime.deserialize_cuda_engine(engine_data)

# Check memory
print(f"Device memory: {engine.device_memory_size / 1024**2:.1f} MB")
print(f"Layers: {engine.num_layers}")
print(f"Max batch: {engine.max_batch_size}")

# Per-layer memory (advanced)
inspector = engine.create_engine_inspector()
print(inspector.get_engine_information(trt.LayerInformationFormat.JSON))
```

---

**14. Multi-Model and Multi-Engine Inference**

Production Jetson systems often run multiple AI models simultaneously: object detection + classification + tracking, or detection + segmentation + LLM.

**Memory Planning for Multi-Model Pipelines**

```
Example: Autonomous robot vision pipeline

  Camera → YOLOv8-N (detect) → ResNet-18 (classify) → DeepSORT (track)
                  ↓                     ↓                    ↓
              ~30 MB               ~25 MB                ~15 MB
              GPU + DLA            GPU                   CPU

Pipeline memory budget:
  Model weights:      30 + 25 + 15         = ~70 MB
  Activation buffers: 12 + 8 + 5           = ~25 MB
  Workspace:          30 + 20 + 0          = ~50 MB
  Camera buffers:     4 frames × 3 MB      = ~12 MB
  ───────────────────────────────────────────────────
  Total AI pipeline:                        ~157 MB

  + OS/kernel/CMA/CUDA overhead:           ~2500 MB
  ───────────────────────────────────────────────────
  Remaining from 8 GB:                     ~5343 MB  ← plenty of room
```

**GPU + DLA Split — Maximum Throughput**

Running detection on DLA while classification runs on GPU achieves overlap — both engines execute in parallel on the same DRAM:

```
Timeline (overlapped):

  Frame N:   │ DLA: YOLO detect ──────│
             │ GPU: (idle)            │ GPU: ResNet classify ──│
             │                         │                        │
  Frame N+1: │                    DLA: YOLO detect ──────│     │
             │                         │ GPU: ResNet classify ──│

DLA and GPU read different parts of DRAM simultaneously.
Memory controller arbitrates bandwidth between them.
```

**Key constraint:** DLA and GPU compete for the same ~102 GB/s DRAM bandwidth. If both are bandwidth-saturated, total throughput drops. Monitor with `tegrastats`:

```bash
tegrastats --interval 500
# Look for: GR3D_FREQ (GPU utilization), EMC_FREQ (memory clock)
# If EMC is at 100%, you're bandwidth-limited — reduce model size or batch
```

</details>

### Multi-Engine TensorRT 上下文

```cpp
// Load two engines
auto det_engine = runtime->deserializeCudaEngine(det_data, det_size);
auto cls_engine = runtime->deserializeCudaEngine(cls_data, cls_size);

// Create execution contexts (share GPU, separate state)
auto det_ctx = det_engine->createExecutionContext();
auto cls_ctx = cls_engine->createExecutionContext();

// Run on separate CUDA streams for overlap
cudaStream_t det_stream, cls_stream;
cudaStreamCreate(&det_stream);
cudaStreamCreate(&cls_stream);

det_ctx->enqueueV2(det_bindings, det_stream, nullptr);
cls_ctx->enqueueV2(cls_bindings, cls_stream, nullptr);

// Both execute concurrently if resources allow
cudaStreamSynchronize(det_stream);
cudaStreamSynchronize(cls_stream);
```

---

## 15. 统一架构上的 LLM 内存模式

LLM 具有独特的内存访问模式，与 Jetson 统一架构的交互方式不同于视觉模型。

### 自回归 decode —— 内存访问模式

```
Prefill phase (process entire prompt):
  All tokens processed in parallel
  Memory pattern: large GEMM (batch = prompt_length × hidden_dim)
  Bandwidth usage: HIGH (loading full weight matrices)
  Compute utilization: GOOD (large batch amortizes weight loading)

Decode phase (generate one token at a time):
  One token generated per step
  Memory pattern: skinny GEMV (batch=1 × hidden_dim)
  Bandwidth usage: HIGH (still load full weight matrices for 1 token!)
  Compute utilization: TERRIBLE (~1% — almost all time spent loading weights)

This is why LLM decode is severely memory-bandwidth-bound on Jetson.
```

### 在 Jetson 上权重加载占主导

```
Llama 3.2 3B INT4 decode — one token:

  Weight loading: 1.5 GB read from DRAM
  Computation:    3B × 2 FLOPs = 6 GFLOP
  Time to load:   1.5 GB / 102 GB/s = ~14.7 ms
  Time to compute: 6 GFLOP / 67 TOPS = ~0.09 ms
  ─────────────────────────────────────────────
  Decode time:    ~15 ms per token → ~67 tokens/sec

  Compute utilization: 0.09 / 14.7 = 0.6%

  → 99.5% of time is waiting for DRAM to deliver weights
  → Faster compute (more CUDA cores) would not help
  → Only bandwidth helps: smaller weights (INT4 > INT8 > FP16)
```

**这与服务器 GPU 有根本不同：**

```
H100 with same model:
  Time to load:   1.5 GB / 3350 GB/s = ~0.45 ms
  → ~2,200 tokens/sec (bandwidth difference)

Jetson Orin Nano Super is ~33× slower for LLM decode purely due to bandwidth.
This is not fixable by software — it's physics.
```

### 统一内存中的 KV cache —— 优势所在

在独立 GPU 上，KV cache 位于 GPU VRAM 中。如果需要对 attention 模式进行 CPU 后处理（用于调试、可解释性），必须通过 PCIe 拷回。

在 Jetson 上，KV cache 位于同一块 DRAM 中 —— CPU 可以直接检查：

```cpp
// Allocate KV cache with cudaMallocHost (zero-copy on Jetson)
float* kv_cache;
cudaMallocHost(&kv_cache, kv_cache_size);

// GPU writes during attention
attention_kernel<<<grid, block>>>(q, k, v, kv_cache, ...);
cudaDeviceSynchronize();

// CPU can read KV cache directly — no copy!
for (int l = 0; l < num_layers; l++) {
    printf("Layer %d, head 0, token 0: K=%.3f\n",
           l, kv_cache[l * kv_stride]);
}
```

### 对话过程中 LLM 内存的增长

```
KV cache grows with each generated token:

Token 1:    Model (1.5 GB) + KV (0.05 MB) = 1500 MB
Token 100:  Model (1.5 GB) + KV (5.3 MB)  = 1505 MB
Token 1000: Model (1.5 GB) + KV (53 MB)   = 1553 MB
Token 2048: Model (1.5 GB) + KV (109 MB)  = 1609 MB
Token 4096: Model (1.5 GB) + KV (218 MB)  = 1718 MB
Token 8192: Model (1.5 GB) + KV (436 MB)  = 1936 MB  ← danger zone on 8 GB

Monitor continuously:
  tegrastats --interval 1000 | grep -oP 'RAM \d+/\d+MB'
```

在生产环境中设置硬性上下文上限以防止 OOM：

```bash
# llama.cpp: cap context to prevent OOM
./llama-cli -m model.gguf -c 2048 --memory-f32 0  # INT8 KV cache
```

---

## 16. 推理内存优化策略

### 16.1 量化对内存的影响

量化是最有效的内存优化手段 —— 每节省一个 bit 就节省一份带宽：

```
Same model, different precisions — memory and tokens/sec:

  Precision   Size     DRAM reads/token   Est. tokens/sec
  ─────────────────────────────────────────────────────────
  FP32        12 GB    12 GB/token        won't fit
  FP16         6 GB    6 GB/token         ~8 tok/s
  INT8         3 GB    3 GB/token         ~17 tok/s
  INT4         1.5 GB  1.5 GB/token       ~33 tok/s
  INT3         1.1 GB  1.1 GB/token       ~45 tok/s (quality degrades)

  Tokens/sec scales almost linearly with quantization level
  because decode is 99%+ memory-bandwidth-bound.
```


<details>
<summary>English original</summary>

**Multi-Engine TensorRT Contexts**

```cpp
// Load two engines
auto det_engine = runtime->deserializeCudaEngine(det_data, det_size);
auto cls_engine = runtime->deserializeCudaEngine(cls_data, cls_size);

// Create execution contexts (share GPU, separate state)
auto det_ctx = det_engine->createExecutionContext();
auto cls_ctx = cls_engine->createExecutionContext();

// Run on separate CUDA streams for overlap
cudaStream_t det_stream, cls_stream;
cudaStreamCreate(&det_stream);
cudaStreamCreate(&cls_stream);

det_ctx->enqueueV2(det_bindings, det_stream, nullptr);
cls_ctx->enqueueV2(cls_bindings, cls_stream, nullptr);

// Both execute concurrently if resources allow
cudaStreamSynchronize(det_stream);
cudaStreamSynchronize(cls_stream);
```

---

**15. LLM Memory Patterns on Unified Architecture**

LLMs have unique memory patterns that interact with Jetson's unified architecture differently than vision models.

**Autoregressive Decode — The Memory Access Pattern**

```
Prefill phase (process entire prompt):
  All tokens processed in parallel
  Memory pattern: large GEMM (batch = prompt_length × hidden_dim)
  Bandwidth usage: HIGH (loading full weight matrices)
  Compute utilization: GOOD (large batch amortizes weight loading)

Decode phase (generate one token at a time):
  One token generated per step
  Memory pattern: skinny GEMV (batch=1 × hidden_dim)
  Bandwidth usage: HIGH (still load full weight matrices for 1 token!)
  Compute utilization: TERRIBLE (~1% — almost all time spent loading weights)

This is why LLM decode is severely memory-bandwidth-bound on Jetson.
```

**Weight Loading Dominates on Jetson**

```
Llama 3.2 3B INT4 decode — one token:

  Weight loading: 1.5 GB read from DRAM
  Computation:    3B × 2 FLOPs = 6 GFLOP
  Time to load:   1.5 GB / 102 GB/s = ~14.7 ms
  Time to compute: 6 GFLOP / 67 TOPS = ~0.09 ms
  ─────────────────────────────────────────────
  Decode time:    ~15 ms per token → ~67 tokens/sec

  Compute utilization: 0.09 / 14.7 = 0.6%

  → 99.5% of time is waiting for DRAM to deliver weights
  → Faster compute (more CUDA cores) would not help
  → Only bandwidth helps: smaller weights (INT4 > INT8 > FP16)
```

**This is fundamentally different from server GPUs:**

```
H100 with same model:
  Time to load:   1.5 GB / 3350 GB/s = ~0.45 ms
  → ~2,200 tokens/sec (bandwidth difference)

Jetson Orin Nano Super is ~33× slower for LLM decode purely due to bandwidth.
This is not fixable by software — it's physics.
```

**KV Cache in Unified Memory — The Advantage**

On discrete GPUs, KV cache lives in GPU VRAM. If you need CPU post-processing of attention patterns (for debugging, interpretability), you must copy back over PCIe.

On Jetson, KV cache is in the same DRAM — CPU can inspect it directly:

```cpp
// Allocate KV cache with cudaMallocHost (zero-copy on Jetson)
float* kv_cache;
cudaMallocHost(&kv_cache, kv_cache_size);

// GPU writes during attention
attention_kernel<<<grid, block>>>(q, k, v, kv_cache, ...);
cudaDeviceSynchronize();

// CPU can read KV cache directly — no copy!
for (int l = 0; l < num_layers; l++) {
    printf("Layer %d, head 0, token 0: K=%.3f\n",
           l, kv_cache[l * kv_stride]);
}
```

**LLM Memory Growth During Conversation**

```
KV cache grows with each generated token:

Token 1:    Model (1.5 GB) + KV (0.05 MB) = 1500 MB
Token 100:  Model (1.5 GB) + KV (5.3 MB)  = 1505 MB
Token 1000: Model (1.5 GB) + KV (53 MB)   = 1553 MB
Token 2048: Model (1.5 GB) + KV (109 MB)  = 1609 MB
Token 4096: Model (1.5 GB) + KV (218 MB)  = 1718 MB
Token 8192: Model (1.5 GB) + KV (436 MB)  = 1936 MB  ← danger zone on 8 GB

Monitor continuously:
  tegrastats --interval 1000 | grep -oP 'RAM \d+/\d+MB'
```

Set a hard context limit in production to prevent OOM:

```bash
# llama.cpp: cap context to prevent OOM
./llama-cli -m model.gguf -c 2048 --memory-f32 0  # INT8 KV cache
```

---

**16. Inference Memory Optimization Strategies**

**16.1 Quantization Impact on Memory**

Quantization is the single most effective memory optimization — every bit saved is bandwidth saved:

```
Same model, different precisions — memory and tokens/sec:

  Precision   Size     DRAM reads/token   Est. tokens/sec
  ─────────────────────────────────────────────────────────
  FP32        12 GB    12 GB/token        won't fit
  FP16         6 GB    6 GB/token         ~8 tok/s
  INT8         3 GB    3 GB/token         ~17 tok/s
  INT4         1.5 GB  1.5 GB/token       ~33 tok/s
  INT3         1.1 GB  1.1 GB/token       ~45 tok/s (quality degrades)

  Tokens/sec scales almost linearly with quantization level
  because decode is 99%+ memory-bandwidth-bound.
```

</details>

### 16.2 带宽的权重布局

权重在内存中如何存储会影响带宽利用率：

```
Row-major (default):
  Weight matrix [4096 × 4096] stored as 4096 rows of 4096 elements
  For batch=1 GEMV: read entire matrix row by row
  Memory access: sequential → good for DRAM burst reads ✓

Blocked layout (TensorRT-LLM):
  Matrix split into tiles that fit in SM shared memory
  Each tile loaded once, used for multiple output elements
  Better reuse → fewer total DRAM reads ✓

Interleaved quantized layout:
  INT4 weights packed 2 per byte, interleaved for Tensor Core alignment
  Dequantize in registers during compute → no extra bandwidth ✓
```

TensorRT 和 llama.cpp 会自动处理布局优化。如果编写自定义 kernel，布局至关重要。

### 16.3 AI kernel 的共享内存分块

CUDA 第 3 节中的同一分块原则适用于 Jetson 上的所有 AI kernel：

```
Attention kernel without tiling:
  For each output element:
    Load Q row from DRAM (4096 × 2 bytes = 8 KB)
    Load K column from DRAM (context × 2 bytes)
    Compute dot product
    Load V column from DRAM
    Accumulate
  Total DRAM: O(seq² × d) — quadratic in sequence length

Attention kernel with tiling (FlashAttention):
  Load Q tile (64 × 128) into shared memory
  For each K/V tile:
    Load K tile into shared memory
    Load V tile into shared memory
    Compute partial attention in shared memory
    Accumulate in registers
  Write final output to DRAM
  Total DRAM: O(seq × d) — linear! (tiles reused within shared memory)
```

在每 SM 拥有 48 KB 共享内存的 Orin Nano 上，典型分块大小：
- Attention：Q 分块 32×64，K/V 分块 32×64
- GEMM：64×64 分块（INT4 权重 + FP16 激活值）
- 卷积：16×16 输出分块

### 16.4 用于 AI 视觉流水线的 DMA-BUF

从摄像头到 AI 模型的零拷贝路径是 Jetson 上最高效的流水线：

```
Optimal AI vision pipeline (zero-copy throughout):

  Camera → ISP → [DMA-BUF] → GPU preprocess → [same buffer] → TensorRT
                    ↑                                              ↓
              CMA allocation                              Detection output
              (done once at startup)                      in pinned memory
                                                               ↓
                                                          CPU post-process
                                                          (direct access, no copy)

  Total copies: ZERO
  Total DRAM bandwidth: only what compute needs (no wasted copy traffic)
```

与朴素流水线对比：
```
  Camera → ISP → memcpy → CPU buffer → cudaMemcpy → GPU buffer → TensorRT → cudaMemcpy → CPU
  Total copies: 3 (each wastes ~3–12 MB × 30 FPS of bandwidth)
  Wasted bandwidth: ~360 MB/s for 1080p — 0.35% of total 102 GB/s
  For 4K: ~1.4 GB/s wasted — 2.7% of total bandwidth

  On bandwidth-starved Jetson, every percent counts.
```

### 16.5 多模型内存共享模式

运行多个 AI 模型时，尽可能共享缓冲区：

```cpp
// Pre-allocate one buffer for the largest input
size_t max_input_size = std::max({yolo_input_size, resnet_input_size, seg_input_size});
void* shared_input;
cudaMallocHost(&shared_input, max_input_size);  // pinned, zero-copy

// Reuse for all models (they run sequentially)
preprocess_for_yolo(camera_frame, shared_input);
yolo_ctx->enqueueV2({shared_input, yolo_output}, stream, nullptr);

preprocess_for_resnet(crop, shared_input);  // reuse same buffer
resnet_ctx->enqueueV2({shared_input, resnet_output}, stream, nullptr);
```

**节省的内存：** 不再使用 3 个独立输入缓冲区（3 × 3 MB = 9 MB），而使用 1 × 3 MB = 3 MB。在多个模型和多个摄像头的情况下，这一差异会迅速累积。

---

## 17. OP-TEE 与安全内存

Orin Nano 支持 ARM TrustZone，具有两个执行世界：

```
Normal World (Linux, CUDA, all userspace)
─────────────────────────────────────────
Secure World (OP-TEE Trusted OS)
```

### 安全内存预留

DRAM 中划出一块区域，专供安全世界使用：

* **未映射到 Linux** — `cat /proc/iomem` 不会显示它
* **受 TrustZone 保护** — 硬件阻止普通世界访问
* **SMMU 无法访问** — 即使来自设备的 DMA 也无法到达它

用于：

* DRM 密钥存储与内容解密
* runtime 时的安全启动链校验
* 密码学服务（硬件加速）
* 安全存储（加密密钥材料）

### 内存影响

如果 OP-TEE 预留配置得过大，Linux 可用 RAM 会缩小。在内存受限的 8GB 系统上，每一 MB 都至关重要。默认预留通常为 16–64MB。

### 与 OP-TEE 交互

Linux 通过 TEE 子系统与 OP-TEE 通信：

```bash
# Check if OP-TEE is running
ls /dev/tee*

# Typical devices
/dev/tee0        # TEE device
/dev/teepriv0    # Privileged TEE device
```

应用程序使用 OP-TEE 客户端库（`libteec`）来调用安全世界中的可信应用（TA）。

---


<details>
<summary>English original</summary>

**16.2 Weight Layout for Bandwidth**

How weights are stored in memory affects bandwidth utilization:

```
Row-major (default):
  Weight matrix [4096 × 4096] stored as 4096 rows of 4096 elements
  For batch=1 GEMV: read entire matrix row by row
  Memory access: sequential → good for DRAM burst reads ✓

Blocked layout (TensorRT-LLM):
  Matrix split into tiles that fit in SM shared memory
  Each tile loaded once, used for multiple output elements
  Better reuse → fewer total DRAM reads ✓

Interleaved quantized layout:
  INT4 weights packed 2 per byte, interleaved for Tensor Core alignment
  Dequantize in registers during compute → no extra bandwidth ✓
```

TensorRT and llama.cpp handle layout optimization automatically. If writing custom kernels, layout matters enormously.

**16.3 Shared Memory Tiling for AI Kernels**

The same tiling principle from CUDA Section 3 applies to all AI kernels on Jetson:

```
Attention kernel without tiling:
  For each output element:
    Load Q row from DRAM (4096 × 2 bytes = 8 KB)
    Load K column from DRAM (context × 2 bytes)
    Compute dot product
    Load V column from DRAM
    Accumulate
  Total DRAM: O(seq² × d) — quadratic in sequence length

Attention kernel with tiling (FlashAttention):
  Load Q tile (64 × 128) into shared memory
  For each K/V tile:
    Load K tile into shared memory
    Load V tile into shared memory
    Compute partial attention in shared memory
    Accumulate in registers
  Write final output to DRAM
  Total DRAM: O(seq × d) — linear! (tiles reused within shared memory)
```

On Orin Nano with 48 KB shared memory per SM, typical tile sizes:
- Attention: Q tile 32×64, K/V tile 32×64
- GEMM: 64×64 tile (INT4 weights + FP16 activations)
- Convolution: 16×16 output tile

**16.4 DMA-BUF for AI Vision Pipelines**

The zero-copy path from camera to AI model is the most efficient pipeline on Jetson:

```
Optimal AI vision pipeline (zero-copy throughout):

  Camera → ISP → [DMA-BUF] → GPU preprocess → [same buffer] → TensorRT
                    ↑                                              ↓
              CMA allocation                              Detection output
              (done once at startup)                      in pinned memory
                                                               ↓
                                                          CPU post-process
                                                          (direct access, no copy)

  Total copies: ZERO
  Total DRAM bandwidth: only what compute needs (no wasted copy traffic)
```

Compare with naive pipeline:
```
  Camera → ISP → memcpy → CPU buffer → cudaMemcpy → GPU buffer → TensorRT → cudaMemcpy → CPU
  Total copies: 3 (each wastes ~3–12 MB × 30 FPS of bandwidth)
  Wasted bandwidth: ~360 MB/s for 1080p — 0.35% of total 102 GB/s
  For 4K: ~1.4 GB/s wasted — 2.7% of total bandwidth

  On bandwidth-starved Jetson, every percent counts.
```

**16.5 Multi-Model Memory Sharing Patterns**

When running multiple AI models, share buffers where possible:

```cpp
// Pre-allocate one buffer for the largest input
size_t max_input_size = std::max({yolo_input_size, resnet_input_size, seg_input_size});
void* shared_input;
cudaMallocHost(&shared_input, max_input_size);  // pinned, zero-copy

// Reuse for all models (they run sequentially)
preprocess_for_yolo(camera_frame, shared_input);
yolo_ctx->enqueueV2({shared_input, yolo_output}, stream, nullptr);

preprocess_for_resnet(crop, shared_input);  // reuse same buffer
resnet_ctx->enqueueV2({shared_input, resnet_output}, stream, nullptr);
```

**Memory saved:** instead of 3 separate input buffers (3 × 3 MB = 9 MB), use 1 × 3 MB = 3 MB. This adds up quickly with multiple models and multiple cameras.

---

**17. OP-TEE and Secure Memory**

Orin Nano supports ARM TrustZone with two execution worlds:

```
Normal World (Linux, CUDA, all userspace)
─────────────────────────────────────────
Secure World (OP-TEE Trusted OS)
```

**Secure Memory Carveout**

A region of DRAM is carved out exclusively for the secure world:

* **Not mapped in Linux** — `cat /proc/iomem` will not show it
* **Protected by TrustZone** — hardware prevents normal-world access
* **Not accessible by SMMU** — even DMA from devices cannot reach it

Used for:

* DRM key storage and content decryption
* Secure boot chain validation at runtime
* Cryptographic services (hardware-accelerated)
* Secure storage (encrypted key material)

**Memory Impact**

If the OP-TEE carveout is configured too large, Linux usable RAM shrinks. On a memory-constrained 8GB system, every MB counts. Default carveouts are typically 16–64MB.

**Interacting with OP-TEE**

Linux communicates with OP-TEE via the TEE subsystem:

```bash
# Check if OP-TEE is running
ls /dev/tee*

# Typical devices
/dev/tee0        # TEE device
/dev/teepriv0    # Privileged TEE device
```

Applications use the OP-TEE client library (`libteec`) to call Trusted Applications (TAs) in the secure world.

---

</details>

## 18. 多摄像头内存规划

生产环境中的 Jetson 系统常同时运行 2–6 路摄像头。内存规划至关重要。

### 单摄像头内存预算

对于单路 1080p、30 FPS 摄像头：

| 组件                  | 每帧内存 | 缓冲帧数 | 合计      |
|----------------------------|-----------------|-----------------|------------|
| CSI/VI 采集缓冲      | ~6MB（RAW10）    | 4（环形）        | ~24MB      |
| ISP 输出（NV12）          | ~3MB            | 4（环形）        | ~12MB      |
| CUDA 推理输入       | ~3MB            | 2               | ~6MB       |
| **单摄像头合计**       |                 |                 | **~42MB**  |

对于 4K（3840x2160）：

| 组件                  | 每帧内存 | 缓冲帧数 | 合计      |
|----------------------------|-----------------|-----------------|------------|
| CSI/VI 采集缓冲      | ~24MB（RAW10）   | 4（环形）        | ~96MB      |
| ISP 输出（NV12）          | ~12MB           | 4（环形）        | ~48MB      |
| CUDA 推理输入       | ~12MB           | 2               | ~24MB      |
| **单摄像头合计**       |                 |                 | **~168MB** |

### 系统内存预算（4 路 1080p 示例）

| 组件                    | 内存    |
|------------------------------|-----------|
| OS + 内核 + 服务       | ~500MB    |
| 摄像头缓冲（4 路摄像头）   | ~168MB    |
| CMA 保留                 | ~768MB    |
| TensorRT 模型（YOLOv8-S）    | ~100MB    |
| CUDA runtime + scratch       | ~200MB    |
| 固件预留内存            | ~400MB    |
| **用户态可用剩余**  | **~5.5GB** |

模型更大或使用 4K 摄像头时，该预算会显著收紧。部署前先规划并实测。

### 缓冲池策略

面向生产系统：

1. **启动时预分配全部缓冲** —— 避免 runtime 分配/释放循环
2. **使用固定大小缓冲池** —— NVMM 池或固定数量的 V4L2 REQBUFS
3. **固定（pin）缓冲** —— 防止内核换出或迁移
4. **监控碎片化** —— 定期记录 CMA 与 buddy 状态

---

## 19. 性能监控与性能剖析

### tegrastats

NVIDIA 的实时系统监控工具：

```bash
tegrastats --interval 1000
```

输出包括：

* RAM 使用量（已用/总量）
* 每核 CPU 利用率
* GPU 利用率百分比
* GPU 频率
* 温度（CPU、GPU、板卡）
* 各供电轨功耗

### /proc/meminfo

Jetson 内存分析的关键字段：

```bash
cat /proc/meminfo
```

| 字段        | 含义                              |
|--------------|------------------------------------------------|
| MemTotal     | Linux 可见 RAM 总量（扣除预留后）      |
| MemAvailable | 可供新分配的估算可用内存 |
| CmaTotal     | CMA 区域总大小                          |
| CmaFree      | 空闲 CMA 内存                                |
| Slab         | 内核 slab 分配器占用                      |
| Mapped       | 内存映射的文件页                       |

### /proc/buddyinfo

按 zone 和 order 显示碎片化情况：

```bash
cat /proc/buddyinfo
```

健康系统：所有 order 都有数值。
碎片化系统：order 0–2 计数高，order 6 及以上为 0。

### Nsight Systems

将 CUDA、摄像头与推理一起做性能剖析：

```bash
nsys profile --trace=cuda,nvtx,osrt ./my_inference_app
```

展示以下时间线：

* CUDA kernel 启动
* 内存分配与传输
* CPU/GPU 同步点
* 操作系统 runtime 事件

### SMMU 与 DMA 调试

```bash
# SMMU faults
dmesg | grep smmu

# IOMMU groups (which devices share SMMU context)
ls /sys/kernel/iommu_groups/*/devices/

# DMA-BUF usage
cat /sys/kernel/debug/dma_buf/bufinfo
```

---

## 20. 生产环境调试检查清单

摄像头或推理在运行数小时后随机失效时：

### 第 1 步 —— 检查整体内存

```bash
cat /proc/meminfo | grep -E "MemTotal|MemAvailable|CmaTotal|CmaFree"
```

若 `MemAvailable` 很低，系统处于内存压力之下。若 `CmaFree` 低，摄像头缓冲可能分配失败。

### 第 2 步 —— 检查碎片化

```bash
cat /proc/buddyinfo
```

若高阶列（order 6 及以上）显示 `0`，说明内存已碎片化。即使有空闲内存，大块连续分配也会失败。

### 第 3 步 —— 检查 CMA 使用情况

```bash
grep Cma /proc/meminfo
```

将 `CmaFree` 与预期的单帧分配大小对比。若 `CmaFree` 小于所需的最大单次分配量，分配将失败。

### 第 4 步 —— 检查 SMMU 故障

```bash
dmesg | grep smmu
```

SMMU 故障示例：

```
arm-smmu 12000000.iommu: Unhandled context fault: iova=0x1234000, fsynr=0x11
```

这表明某设备访问了未映射的 IOVA —— 通常是 use-after-free bug 或 DMA-BUF 处理不当。

### 第 5 步 —— 检查 OOM 事件

```bash
dmesg | grep -i "out of memory\|oom"
```

OOM killer 可能已终止某个进程。检查被杀的是哪个进程以及原因。


<details>
<summary>English original</summary>

**18. Multi-Camera Memory Planning**

Production Jetson systems often run 2–6 cameras simultaneously. Memory planning is critical.

**Per-Camera Memory Budget**

For a single 1080p camera at 30 FPS:

| Component                  | Memory Per Frame | Frames Buffered | Total      |
|----------------------------|-----------------|-----------------|------------|
| CSI/VI capture buffer      | ~6MB (RAW10)    | 4 (ring)        | ~24MB      |
| ISP output (NV12)          | ~3MB            | 4 (ring)        | ~12MB      |
| CUDA inference input       | ~3MB            | 2               | ~6MB       |
| **Total per camera**       |                 |                 | **~42MB**  |

For 4K (3840x2160):

| Component                  | Memory Per Frame | Frames Buffered | Total      |
|----------------------------|-----------------|-----------------|------------|
| CSI/VI capture buffer      | ~24MB (RAW10)   | 4 (ring)        | ~96MB      |
| ISP output (NV12)          | ~12MB           | 4 (ring)        | ~48MB      |
| CUDA inference input       | ~12MB           | 2               | ~24MB      |
| **Total per camera**       |                 |                 | **~168MB** |

**System Memory Budget (4-camera 1080p Example)**

| Component                    | Memory    |
|------------------------------|-----------|
| OS + kernel + services       | ~500MB    |
| Camera buffers (4 cameras)   | ~168MB    |
| CMA reserved                 | ~768MB    |
| TensorRT model (YOLOv8-S)    | ~100MB    |
| CUDA runtime + scratch       | ~200MB    |
| Firmware carveouts            | ~400MB    |
| **Remaining for userspace**  | **~5.5GB** |

With larger models or 4K cameras, this budget tightens significantly. Plan and measure before deployment.

**Buffer Pool Strategy**

For production systems:

1. **Pre-allocate all buffers at startup** — avoid runtime allocation/free cycles
2. **Use fixed-size buffer pools** — NVMM pools or V4L2 REQBUFS with fixed count
3. **Pin buffers** — prevent kernel from swapping or migrating them
4. **Monitor fragmentation** — log CMA and buddy state periodically

---

**19. Performance Monitoring and Profiling**

**tegrastats**

NVIDIA's real-time system monitor:

```bash
tegrastats --interval 1000
```

Output includes:

* RAM usage (used/total)
* CPU utilization per core
* GPU utilization percentage
* GPU frequency
* Temperature (CPU, GPU, board)
* Power consumption per rail

**/proc/meminfo**

Key fields for Jetson memory analysis:

```bash
cat /proc/meminfo
```

| Field        | What It Tells You                              |
|--------------|------------------------------------------------|
| MemTotal     | Total Linux-visible RAM (after carveouts)      |
| MemAvailable | Estimated available memory for new allocations |
| CmaTotal     | Total CMA region size                          |
| CmaFree      | Free CMA memory                                |
| Slab         | Kernel slab allocator usage                    |
| Mapped       | Memory-mapped file pages                       |

**/proc/buddyinfo**

Shows fragmentation per zone and order:

```bash
cat /proc/buddyinfo
```

Healthy system: numbers across all orders.
Fragmented system: high counts at order 0–2, zeros at order 6+.

**Nsight Systems**

For profiling CUDA + camera + inference together:

```bash
nsys profile --trace=cuda,nvtx,osrt ./my_inference_app
```

Shows timeline of:

* CUDA kernel launches
* Memory allocations and transfers
* CPU/GPU synchronization points
* OS runtime events

**SMMU and DMA Debugging**

```bash
# SMMU faults
dmesg | grep smmu

# IOMMU groups (which devices share SMMU context)
ls /sys/kernel/iommu_groups/*/devices/

# DMA-BUF usage
cat /sys/kernel/debug/dma_buf/bufinfo
```

---

**20. Production Debug Checklist**

When camera or inference randomly fails after hours of operation:

**Step 1 — Check Overall Memory**

```bash
cat /proc/meminfo | grep -E "MemTotal|MemAvailable|CmaTotal|CmaFree"
```

If `MemAvailable` is very low, the system is under memory pressure. If `CmaFree` is low, camera buffers may fail to allocate.

**Step 2 — Check Fragmentation**

```bash
cat /proc/buddyinfo
```

If high-order columns (order 6+) show `0`, memory is fragmented. Even with free memory, large contiguous allocations will fail.

**Step 3 — Check CMA Usage**

```bash
grep Cma /proc/meminfo
```

Compare `CmaFree` to your expected per-frame allocation size. If `CmaFree` is less than the largest single allocation needed, allocation will fail.

**Step 4 — Check for SMMU Faults**

```bash
dmesg | grep smmu
```

SMMU fault example:

```
arm-smmu 12000000.iommu: Unhandled context fault: iova=0x1234000, fsynr=0x11
```

This means a device accessed an unmapped IOVA — typically a use-after-free bug or incorrect DMA-BUF handling.

**Step 5 — Check for OOM Events**

```bash
dmesg | grep -i "out of memory\|oom"
```

The OOM killer may have terminated a process. Check which process was killed and why.

</details>

### Step 6 — 检查热节流

```bash
cat /sys/devices/virtual/thermal/thermal_zone*/temp
cat /sys/devices/virtual/thermal/thermal_zone*/type
```

温度超过限值后，系统会降低时钟频率。这会导致推理错过截止时间、buffer 排队堆积、内存持续增长。

---

## 21. 常见生产问题与解决方案

### CMA 耗尽

**现象：** 运行数小时后 `Failed to allocate buffer`。

**原因：** 反复 alloc/free 循环导致 CMA 碎片化。

**解决方案：**
* 启动时预分配 buffer 池；永不释放
* 增大 CMA 大小（见第 8 节）
* 使用数量固定的 NVMM buffer 池
* 定期触发 compaction：`echo 1 > /proc/sys/vm/compact_memory`

### 负载下的 SMMU 错误

**现象：** dmesg 中出现 `Unhandled context fault`，帧被丢弃。

**原因：** 设备仍在执行 DMA 时 buffer 已被释放，或 DMA-BUF 处理存在竞态条件。

**解决方案：**
* 释放 DMA-BUF 前确保正确同步
* 检查共享 buffer 的引用计数
* 正确使用 V4L2 QBUF/DQBUF（buffer 在队列中时不要访问）

### Jetson 上的 OOM Killer

**现象：** 应用被意外杀死；`dmesg` 显示 OOM。

**原因：** GPU + CPU + 摄像头内存合计超出可用 RAM。

**解决方案：**
* 缩小模型（量化到 INT8、剪枝）
* 减少摄像头 buffer 数量或降低分辨率
* 将 layer 卸载到 DLA（释放 GPU 内存供其他用途）
* 监控内存预算并设置进程内存上限

### 热节流导致延迟尖峰

**现象：** 推理 FPS 周期性下降；`tegrastats` 显示时钟频率降低。

**原因：** SoC 温度超出热限值。

**解决方案：**
* 增加主动散热（风扇、散热片）
* 用 `nvpmodel` 设置合适的电源模式
* 降低工作负载或占空比
* 检查环境温度与机箱通风

### 长时间运行推理中的内存泄漏

**现象：** `MemAvailable` 在数小时/数天内持续下降。

**原因：** CUDA 内存未释放、DMA-BUF 未关闭、Python 对象堆积。

**解决方案：**
* 若使用 PyTorch，定期调用 `torch.cuda.empty_cache()`
* 确保每个 `cudaMalloc` 都有配对的 `cudaFree`
* 使用后关闭 DMA-BUF 文件描述符
* 用 `cuda-memcheck` 或 Nsight Compute 做 profile

---

## 22. 参考资料

* [NVIDIA Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/) — 涵盖启动、内存、驱动的官方文档
* [Jetson Orin Nano Datasheet](https://developer.nvidia.com/embedded/jetson-orin-nano) — 硬件规格
* [ARM SMMU Architecture Specification](https://developer.arm.com/documentation/ihi0070/) — SMMU v3 参考（SMMU v2 子集）
* [Linux CMA Documentation](https://www.kernel.org/doc/html/latest/mm/cma.html) — kernel CMA 子系统
* [Linux Buddy Allocator](https://www.kernel.org/doc/html/latest/mm/page_allocator.html) — 页分配器内部实现
* [OP-TEE Documentation](https://optee.readthedocs.io/) — 可信执行环境
* [GStreamer on Jetson](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Multimedia/AcceleratedGstreamer.html) — 硬件加速多媒体流水线
* [TensorRT DLA Documentation](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/) — DLA 与 TensorRT 集成
* 主指南：[Nvidia Jetson Platform Guide](../Guide.md)


<details>
<summary>English original</summary>

**Step 6 — Check Thermal Throttling**

```bash
cat /sys/devices/virtual/thermal/thermal_zone*/temp
cat /sys/devices/virtual/thermal/thermal_zone*/type
```

If temperature exceeds limits, the system throttles clocks. This can cause inference to miss deadlines, buffers to queue up, and memory to grow.

---

**21. Common Production Issues and Solutions**

**CMA Exhaustion**

**Symptom:** `Failed to allocate buffer` after hours of operation.

**Cause:** CMA fragmentation from repeated alloc/free cycles.

**Solutions:**
* Pre-allocate buffer pools at startup; never free them
* Increase CMA size (see Section 8)
* Use NVMM buffer pools with fixed count
* Periodically trigger compaction: `echo 1 > /proc/sys/vm/compact_memory`

**SMMU Faults Under Load**

**Symptom:** `Unhandled context fault` in dmesg, frames dropped.

**Cause:** Buffer freed while device still performing DMA, or race condition in DMA-BUF handling.

**Solutions:**
* Ensure proper synchronization before freeing DMA-BUF
* Check refcounting on shared buffers
* Use V4L2 QBUF/DQBUF properly (don't access buffer while queued)

**OOM Killer on Jetson**

**Symptom:** Application killed unexpectedly; `dmesg` shows OOM.

**Cause:** Combined GPU + CPU + camera memory exceeds available RAM.

**Solutions:**
* Reduce model size (quantize to INT8, prune)
* Reduce camera buffer count or resolution
* Offload layers to DLA (frees GPU memory for other uses)
* Monitor memory budget and set process memory limits

**Thermal Throttling Causing Latency Spikes**

**Symptom:** Inference FPS drops periodically; `tegrastats` shows clock reduction.

**Cause:** SoC temperature exceeds thermal limits.

**Solutions:**
* Add active cooling (fan, heatsink)
* Use `nvpmodel` to set appropriate power mode
* Reduce workload or duty cycle
* Check ambient temperature and enclosure ventilation

**Memory Leak in Long-Running Inference**

**Symptom:** `MemAvailable` steadily decreases over hours/days.

**Cause:** CUDA memory not freed, DMA-BUF not closed, Python object accumulation.

**Solutions:**
* Use `torch.cuda.empty_cache()` periodically if using PyTorch
* Ensure every `cudaMalloc` has a matching `cudaFree`
* Close DMA-BUF file descriptors after use
* Profile with `cuda-memcheck` or Nsight Compute

---

**22. References**

* [NVIDIA Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/) — official documentation covering boot, memory, drivers
* [Jetson Orin Nano Datasheet](https://developer.nvidia.com/embedded/jetson-orin-nano) — hardware specifications
* [ARM SMMU Architecture Specification](https://developer.arm.com/documentation/ihi0070/) — SMMU v3 reference (SMMU v2 subset)
* [Linux CMA Documentation](https://www.kernel.org/doc/html/latest/mm/cma.html) — kernel CMA subsystem
* [Linux Buddy Allocator](https://www.kernel.org/doc/html/latest/mm/page_allocator.html) — page allocator internals
* [OP-TEE Documentation](https://optee.readthedocs.io/) — Trusted Execution Environment
* [GStreamer on Jetson](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Multimedia/AcceleratedGstreamer.html) — hardware-accelerated multimedia pipelines
* [TensorRT DLA Documentation](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/) — DLA integration with TensorRT
* Main guide: [Nvidia Jetson Platform Guide](../Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/1. Nvidia Jetson Platform/Orin-Nano-Memory-Architecture/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/1.%20Nvidia%20Jetson%20Platform/Orin-Nano-Memory-Architecture/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
