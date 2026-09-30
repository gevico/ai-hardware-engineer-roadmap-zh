---
title: ROCm 与 HIP（阶段 1 §4 — 子轨道 4）
description: ROCm 与 HIP（阶段 1 §4 — 子轨道 4）
---

# ROCm 与 HIP（阶段 1 §4 — 子轨道 4）

<div class="course-identity auto-course" style="--course-accent: #7c3aed; --course-accent-rgb: 124, 58, 237;" markdown="1">
<div class="course-identity__icon">RAHP</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度探索 · 数字基础</p>
<p class="course-identity__title">ROCm 与 HIP 的专项课程标识（阶段 1 §4 — 子轨道 4）。</p>
<p class="course-identity__meta">产物：可运行的低层 demo · 度量：时序、内存、正确性</p>
</div>
</div>


**父级：** [C++ 与并行计算](../Guide.md)

> *AMD 对 CUDA 的回应 — 编写可同时运行在 NVIDIA 与 AMD 硬件上的 GPU 代码。*

**前置要求：** 子轨道 3（CUDA 与 SIMT）。必须先具备可用的 CUDA 知识 — HIP 靠对比来学。

**Layer 映射：** **L1**（应用 — 你编写 HIP kernel），**L3**（runtime — HIP runtime、ROCm 驱动栈）。

---

## ROCm 是什么

**ROCm**（Radeon Open Compute）是 AMD 的开源 GPU 计算平台。它包括 HIP 编程语言、kernel 驱动（`amdgpu`）、runtime、编译器（`amd-clang`）、数学库（rocBLAS、MIOpen、rocFFT），以及性能剖析工具（Omniperf、Omnitrace）。ROCm 之于 AMD，正如 CUDA 之于 NVIDIA。

## HIP 是什么

**HIP**（Heterogeneous-computing Interface for Portability）是一个与 CUDA 几乎完全相同的 C++ API。HIP 代码既能编译到 AMD GPU（经 ROCm），也能编译到 NVIDIA GPU（经 CUDA 后端）。这意味着只需编写一个 kernel，就能在两家的硬件上运行。

**为什么现在就要学（而不是只等到阶段 5A）：**
- AMD Instinct GPU（MI300X、MI350）已被 Microsoft Azure、Meta、Oracle 大规模部署
- 同时理解两套生态，在任何 GPU 岗位上都更有价值
- HIP 是从 CUDA 走向可移植 GPU 代码的最快路径
- 阶段 5A（GPU 基础设施）会讲得更深；本子轨道给你打编程基础

---

## 1. CDNA vs RDNA — AMD 的两套 GPU 架构

AMD 面向不同市场做了两套完全不同的 GPU 架构。在编写任何 AMD GPU 代码之前，理解二者的差异是必需的。

### RDNA（Radeon DNA）— 游戏与消费级

RDNA 用于 Radeon RX 消费级 GPU。为图形工作负载优化：高时钟频率、光栅化、光线追踪、显示输出。

- **产品：** Radeon RX 7900 XTX、RX 7800 XT、Steam Deck APU、PlayStation 5、Xbox Series X
- **设计重点：** 高单线程性能、图形流水线、低功耗
- **计算单元：** 双发射 SIMD、32 宽 wavefront（RDNA 3+ 同时支持 wave32 *和* wave64）
- **内存：** GDDR6（最高 24 GB），无 HBM
- **用例：** 游戏、内容创作、轻量 ML 推理

### CDNA（Compute DNA）— 数据中心与 AI

CDNA 用于 AMD Instinct 数据中心 GPU。为矩阵运算和 HPC 优化 — 完全没有图形流水线。

- **产品：** MI300X、MI300A、MI250X、MI210
- **设计重点：** 最大化计算吞吐、矩阵运算、多 GPU 扩展
- **计算单元：** 64 宽 wavefront，针对 FP64/FP32/FP16/INT8 矩阵运算优化
- **Matrix Core：** 硬件矩阵乘累加（相当于 NVIDIA 张量核心）
- **内存：** HBM3（MI300X 上最高 192 GB，带宽 5.3 TB/s）
- **互连：** 面向 GPU 到 GPU 的 Infinity Fabric（类似 NVLink）
- **用例：** AI 训练、AI 推理、HPC、科学计算

### CDNA vs RDNA 对比

| | RDNA（消费级） | CDNA（数据中心） |
|---|---|---|
| **目标** | 游戏、桌面 | AI 训练/推理、HPC |
| **产品** | Radeon RX 7900 XTX | Instinct MI300X |
| **图形流水线** | 有（光栅化、光线追踪） | **无** — 仅计算 |
| **Wavefront 大小** | 32（原生）+ 64（兼容） | **64**（原生） |
| **Matrix Core** | 有限 | 完整矩阵核心阵列（FP16、BF16、FP8、INT8） |
| **内存** | GDDR6（最高 24 GB） | **HBM3**（最高 192 GB，5.3 TB/s） |
| **多 GPU** | CrossFire（消费级） | **Infinity Fabric**（数据中心） |
| **ECC** | 无 | 有 |
| **FP64** | 1/16 速率 | **全速率**（对科学计算很重要） |
| **ROCm 支持** | 有限 | **完整** |
| **价格** | $500–1,000 | $10,000–25,000 |

**为什么这对 AI 硬件工程师很重要：**
- 当你读到 "AMD GPU for AI" 时，它指的是 **CDNA / Instinct**，而不是 RDNA / Radeon
- CDNA 的 matrix core 是 AMD 对 NVIDIA 张量核心的回应 — 也是你在阶段 5F（AI 芯片设计）中要研究（或与之竞争）的硬件
- MI300X 的 chiplet 设计（单个封装上 8 个 XCD）是先进封装（L8）的参考


<details>
<summary>English original</summary>

**ROCm and HIP (Phase 1 §4 — Sub-Track 4)**

<div class="course-identity auto-course" style="--course-accent: #7c3aed; --course-accent-rgb: 124, 58, 237;" markdown="1">
<div class="course-identity__icon">RAHP</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Digital Foundations</p>
<p class="course-identity__title">Specialized course identity for ROCm and HIP (Phase 1 §4 — Sub-Track 4).</p>
<p class="course-identity__meta">Artifact: working low-level demo · Measure: timing, memory, correctness</p>
</div>
</div>


**Parent:** [C++ and Parallel Computing](../Guide.md)

> *AMD's answer to CUDA — write GPU code that runs on both NVIDIA and AMD hardware.*

**Prerequisites:** Sub-Track 3 (CUDA and SIMT). You need working CUDA knowledge first — HIP is learned by comparison.

**Layer mapping:** **L1** (application — you write HIP kernels), **L3** (runtime — HIP runtime, ROCm driver stack).

---

**What ROCm Is**

**ROCm** (Radeon Open Compute) is AMD's open-source GPU compute platform. It includes the HIP programming language, kernel driver (`amdgpu`), runtime, compiler (`amd-clang`), math libraries (rocBLAS, MIOpen, rocFFT), and profiling tools (Omniperf, Omnitrace). ROCm is to AMD what CUDA is to NVIDIA.

**What HIP Is**

**HIP** (Heterogeneous-computing Interface for Portability) is a C++ API almost identical to CUDA. HIP code compiles to both AMD GPUs (via ROCm) and NVIDIA GPUs (via CUDA backend). This means you can write one kernel and run it on both vendors.

**Why learn this now (not just in Phase 5A):**
- AMD Instinct GPUs (MI300X, MI350) are deployed at scale by Microsoft Azure, Meta, Oracle
- Understanding both ecosystems makes you more valuable for any GPU role
- HIP is the fastest path from CUDA to portable GPU code
- Phase 5A (GPU Infrastructure) goes deeper; this sub-track gives you the programming foundation

---

**1. CDNA vs RDNA — AMD's Two GPU Architectures**

AMD makes two completely different GPU architectures for different markets. Understanding the difference is essential before writing any AMD GPU code.

**RDNA (Radeon DNA) — Gaming and Consumer**

RDNA powers Radeon RX consumer GPUs. Optimized for graphics workloads: high clock speeds, rasterization, ray tracing, display output.

- **Products:** Radeon RX 7900 XTX, RX 7800 XT, Steam Deck APU, PlayStation 5, Xbox Series X
- **Design focus:** High single-thread performance, graphics pipeline, low power
- **Compute units:** Dual-issue SIMD, 32-wide wavefronts (RDNA 3+ supports wave32 *and* wave64)
- **Memory:** GDDR6 (up to 24 GB), no HBM
- **Use case:** Gaming, content creation, lightweight ML inference

**CDNA (Compute DNA) — Data Center and AI**

CDNA powers AMD Instinct data-center GPUs. Optimized for matrix math and HPC — no graphics pipeline at all.

- **Products:** MI300X, MI300A, MI250X, MI210
- **Design focus:** Maximum compute throughput, matrix operations, multi-GPU scaling
- **Compute units:** 64-wide wavefronts, optimized for FP64/FP32/FP16/INT8 matrix operations
- **Matrix Cores:** Hardware matrix multiply-accumulate (equivalent to NVIDIA Tensor Cores)
- **Memory:** HBM3 (up to 192 GB on MI300X with 5.3 TB/s bandwidth)
- **Interconnect:** Infinity Fabric for GPU-to-GPU (like NVLink)
- **Use case:** AI training, AI inference, HPC, scientific computing

**CDNA vs RDNA Comparison**

| | RDNA (Consumer) | CDNA (Data Center) |
|---|---|---|
| **Target** | Gaming, desktop | AI training/inference, HPC |
| **Products** | Radeon RX 7900 XTX | Instinct MI300X |
| **Graphics pipeline** | Yes (rasterization, ray tracing) | **No** — compute only |
| **Wavefront size** | 32 (native) + 64 (compatibility) | **64** (native) |
| **Matrix Cores** | Limited | Full matrix core array (FP16, BF16, FP8, INT8) |
| **Memory** | GDDR6 (up to 24 GB) | **HBM3** (up to 192 GB, 5.3 TB/s) |
| **Multi-GPU** | CrossFire (consumer) | **Infinity Fabric** (data center) |
| **ECC** | No | Yes |
| **FP64** | 1/16 rate | **Full rate** (important for scientific computing) |
| **ROCm support** | Limited | **Full** |
| **Price** | $500–1,000 | $10,000–25,000 |

**Why this matters for AI hardware engineers:**
- When you read "AMD GPU for AI," it means **CDNA / Instinct**, not RDNA / Radeon
- CDNA's matrix cores are AMD's answer to NVIDIA tensor cores — the hardware you'd study (or compete with) in Phase 5F (AI Chip Design)
- MI300X's chiplet design (8 XCDs on one package) is a reference for advanced packaging (L8)

</details>

### MI300X 架构（当前旗舰）

```
┌─────────────────────────────────────────────────┐
│              MI300X Package                      │
│                                                  │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐              │
│  │ XCD │ │ XCD │ │ XCD │ │ XCD │  ← 8 Compute │
│  │  0  │ │  1  │ │  2  │ │  3  │    Chiplets   │
│  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘    (CDNA 3)  │
│     │       │       │       │                    │
│  ┌──┴───────┴───────┴───────┴──┐                │
│  │      Infinity Fabric         │                │
│  │    (inter-chiplet network)   │                │
│  └──┬───────┬───────┬───────┬──┘                │
│  ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐              │
│  │ XCD │ │ XCD │ │ XCD │ │ XCD │               │
│  │  4  │ │  5  │ │  6  │ │  7  │               │
│  └─────┘ └─────┘ └─────┘ └─────┘               │
│                                                  │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐          │
│  │ HBM3 │ │ HBM3 │ │ HBM3 │ │ HBM3 │  192 GB │
│  │stack │ │stack │ │stack │ │stack │  5.3TB/s │
│  └──────┘ └──────┘ └──────┘ └──────┘          │
└─────────────────────────────────────────────────┘
```

每个 XCD 包含 38 个 Compute Unit（CU），每个 CU 有 64 个流处理器 + matrix core。总计：304 个 CU、19,456 个流处理器。

---

## 2. CUDA vs HIP —— API 几乎完全相同

| CUDA | HIP | 说明 |
|------|-----|-------|
| `cudaMalloc()` | `hipMalloc()` | 签名相同 |
| `cudaMemcpy()` | `hipMemcpy()` | 签名相同 |
| `cudaStream_t` | `hipStream_t` | 概念相同 |
| `__shared__` | `__shared__` | 完全一致 |
| `__syncthreads()` | `__syncthreads()` | 完全一致 |
| `threadIdx.x` | `threadIdx.x` | 完全一致 |
| `cudaDeviceSynchronize()` | `hipDeviceSynchronize()` | 相同 |
| `cudaLaunchKernel()` | `hipLaunchKernelGGL()` | 略有不同 |
| Warp size: 32 | **Wavefront size: 64** | **关键架构差异** |

### HIP Kernel 示例

```cpp
#include <hip/hip_runtime.h>

__global__ void vector_add(float* a, float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        c[i] = a[i] + b[i];
    }
}

int main() {
    float *d_a, *d_b, *d_c;
    hipMalloc(&d_a, n * sizeof(float));
    hipMalloc(&d_b, n * sizeof(float));
    hipMalloc(&d_c, n * sizeof(float));

    hipMemcpy(d_a, h_a, n * sizeof(float), hipMemcpyHostToDevice);
    hipMemcpy(d_b, h_b, n * sizeof(float), hipMemcpyHostToDevice);

    vector_add<<<(n+255)/256, 256>>>(d_a, d_b, d_c, n);

    hipMemcpy(h_c, d_c, n * sizeof(float), hipMemcpyDeviceToHost);
    hipFree(d_a); hipFree(d_b); hipFree(d_c);
}
```

如果懂 CUDA，就已经掌握了 95% 的 HIP。关键差异如下：

**1. Wavefront = 64 个线程（而 CUDA warp = 32）：**
这会影响任何使用 warp 级原语的代码：
```cpp
// CUDA: assumes warp = 32
unsigned mask = __ballot_sync(0xFFFFFFFF, predicate);  // 32-bit mask

// HIP on AMD: wavefront = 64
unsigned long long mask = __ballot(predicate);          // 64-bit mask
```
归约、shuffle 和 vote 操作都需要针对更宽的 wavefront 做调整。

**2. 共享内存（LDS）bank：** AMD 上是 64 个 bank（NVIDIA 上是 32 个）。bank 冲突模式不同——在 NVIDIA 上能避免冲突的代码在 AMD 上仍可能冲突，反之亦然。

**3. Matrix Core（而非 Tensor Core）：** AMD 使用 `rocWMMA`（Wavefront Matrix Multiply-Accumulate）或 Composable Kernel（CK）库，而不是 NVIDIA 的 `mma.sync` / CUTLASS。

---

## 3. HIPIFY —— CUDA → HIP 自动转换

HIPIFY 是 AMD 用来把 CUDA 代码转换为 HIP 的工具套件。它是把现有 CUDA 项目移植到 AMD GPU 上运行的最快途径。

### 两个转换工具

| 工具 | 工作原理 | 准确率 | 速度 |
|------|-------------|----------|-------|
| **hipify-clang** | 用 Clang 的 AST 解析器从语义上理解 CUDA 代码 | 准确率约 95% | 较慢（完整编译） |
| **hipify-perl** | 基于正则的查找替换（`cuda` → `hip`） | 准确率约 85% | 非常快 |

### hipify-clang（推荐）

```bash
# Install (comes with ROCm)
sudo apt install hipify-clang

# Convert a single CUDA file
hipify-clang my_kernel.cu -o my_kernel.hip.cpp

# Convert an entire project (recursive)
hipify-clang --project-dir ./cuda_project --output-dir ./hip_project

# Show what would change without writing (dry run)
hipify-clang my_kernel.cu --print-stats
```

### hipify-perl（快而糙）

```bash
# Simple text replacement — fast but misses context-dependent conversions
hipify-perl my_kernel.cu > my_kernel.hip.cpp
```


<details>
<summary>English original</summary>

**MI300X Architecture (Current Flagship)**

```
┌─────────────────────────────────────────────────┐
│              MI300X Package                      │
│                                                  │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐              │
│  │ XCD │ │ XCD │ │ XCD │ │ XCD │  ← 8 Compute │
│  │  0  │ │  1  │ │  2  │ │  3  │    Chiplets   │
│  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘    (CDNA 3)  │
│     │       │       │       │                    │
│  ┌──┴───────┴───────┴───────┴──┐                │
│  │      Infinity Fabric         │                │
│  │    (inter-chiplet network)   │                │
│  └──┬───────┬───────┬───────┬──┘                │
│  ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐              │
│  │ XCD │ │ XCD │ │ XCD │ │ XCD │               │
│  │  4  │ │  5  │ │  6  │ │  7  │               │
│  └─────┘ └─────┘ └─────┘ └─────┘               │
│                                                  │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐          │
│  │ HBM3 │ │ HBM3 │ │ HBM3 │ │ HBM3 │  192 GB │
│  │stack │ │stack │ │stack │ │stack │  5.3TB/s │
│  └──────┘ └──────┘ └──────┘ └──────┘          │
└─────────────────────────────────────────────────┘
```

Each XCD contains 38 Compute Units (CUs), each CU has 64 stream processors + matrix cores. Total: 304 CUs, 19,456 stream processors.

---

**2. CUDA vs HIP — Almost Identical API**

| CUDA | HIP | Notes |
|------|-----|-------|
| `cudaMalloc()` | `hipMalloc()` | Same signature |
| `cudaMemcpy()` | `hipMemcpy()` | Same signature |
| `cudaStream_t` | `hipStream_t` | Same concept |
| `__shared__` | `__shared__` | Identical |
| `__syncthreads()` | `__syncthreads()` | Identical |
| `threadIdx.x` | `threadIdx.x` | Identical |
| `cudaDeviceSynchronize()` | `hipDeviceSynchronize()` | Same |
| `cudaLaunchKernel()` | `hipLaunchKernelGGL()` | Slightly different |
| Warp size: 32 | **Wavefront size: 64** | **Key architectural difference** |

**HIP Kernel Example**

```cpp
#include <hip/hip_runtime.h>

__global__ void vector_add(float* a, float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        c[i] = a[i] + b[i];
    }
}

int main() {
    float *d_a, *d_b, *d_c;
    hipMalloc(&d_a, n * sizeof(float));
    hipMalloc(&d_b, n * sizeof(float));
    hipMalloc(&d_c, n * sizeof(float));

    hipMemcpy(d_a, h_a, n * sizeof(float), hipMemcpyHostToDevice);
    hipMemcpy(d_b, h_b, n * sizeof(float), hipMemcpyHostToDevice);

    vector_add<<<(n+255)/256, 256>>>(d_a, d_b, d_c, n);

    hipMemcpy(h_c, d_c, n * sizeof(float), hipMemcpyDeviceToHost);
    hipFree(d_a); hipFree(d_b); hipFree(d_c);
}
```

If you know CUDA, you already know 95% of HIP. The critical differences:

**1. Wavefront = 64 threads (vs CUDA warp = 32):**
This affects any code that uses warp-level primitives:
```cpp
// CUDA: assumes warp = 32
unsigned mask = __ballot_sync(0xFFFFFFFF, predicate);  // 32-bit mask

// HIP on AMD: wavefront = 64
unsigned long long mask = __ballot(predicate);          // 64-bit mask
```
Reductions, shuffles, and vote operations all need adjustment for the wider wavefront.

**2. Shared memory (LDS) banks:** 64 banks on AMD (vs 32 on NVIDIA). Different bank conflict patterns — code that avoids conflicts on NVIDIA may still conflict on AMD, and vice versa.

**3. Matrix Cores (not Tensor Cores):** AMD uses `rocWMMA` (Wavefront Matrix Multiply-Accumulate) or the Composable Kernel (CK) library instead of NVIDIA's `mma.sync` / CUTLASS.

---

**3. HIPIFY — Automatic CUDA → HIP Conversion**

HIPIFY is AMD's tool suite for converting CUDA code to HIP. It's the fastest way to port existing CUDA projects to run on AMD GPUs.

**Two Conversion Tools**

| Tool | How it works | Accuracy | Speed |
|------|-------------|----------|-------|
| **hipify-clang** | Uses Clang's AST parser to understand CUDA code semantically | ~95% accurate | Slower (full compilation) |
| **hipify-perl** | Regex-based find-and-replace (`cuda` → `hip`) | ~85% accurate | Very fast |

**hipify-clang (Recommended)**

```bash
# Install (comes with ROCm)
sudo apt install hipify-clang

# Convert a single CUDA file
hipify-clang my_kernel.cu -o my_kernel.hip.cpp

# Convert an entire project (recursive)
hipify-clang --project-dir ./cuda_project --output-dir ./hip_project

# Show what would change without writing (dry run)
hipify-clang my_kernel.cu --print-stats
```

**hipify-perl (Quick and Dirty)**

```bash
# Simple text replacement — fast but misses context-dependent conversions
hipify-perl my_kernel.cu > my_kernel.hip.cpp
```

</details>

### HIPIFY 能自动转换的内容

| CUDA | 转换为 HIP | 状态 |
|------|------------------|--------|
| `cuda*.h` 头文件 | `hip/hip_runtime.h` | 自动 |
| `cudaMalloc/Free/Memcpy` | `hipMalloc/Free/Memcpy` | 自动 |
| `cudaStream_t`、`cudaEvent_t` | `hipStream_t`、`hipEvent_t` | 自动 |
| `__syncthreads()` | `__syncthreads()` | 无需修改 |
| `atomicAdd()` | `atomicAdd()` | 无需修改 |
| `cuBLAS` 调用 | `rocBLAS` 调用 | **部分** —— API 略有差异 |
| `cuDNN` 调用 | `MIOpen` 调用 | **手动** —— API 设计不同 |
| `cuFFT` 调用 | `rocFFT` 调用 | **部分** |

### HIPIFY 无法转换的内容（需手动处理）

| CUDA 特性 | 为何无法自动转换 | 手动修改 |
|-------------|--------------------------|-----------|
| **内联 PTX 汇编** | PTX 是 NVIDIA 专有的 ISA | 改用 HIP intrinsics 或 GCN 汇编重写 |
| **`__ballot_sync(0xFFFFFFFF, ...)`** | 假定 32 位 warp 掩码 | 使用 `__ballot()`（AMD 上为 64 位）|
| **`__shfl_sync(mask, val, lane)`** | 依赖 warp 大小 | 结合 wavefront 感知使用 `__shfl(val, lane)` |
| **Cooperative groups** | NVIDIA 专有扩展 | 使用 HIP cooperative groups（仅支持子集）|
| **cuDNN → MIOpen** | API 完全不同 | 用 MIOpen API 重写 |
| **Thrust** | NVIDIA 模板库 | 使用 rocThrust（大部分兼容）|
| **CUTLASS** | NVIDIA 模板库 | 使用 AMD Composable Kernel (CK) |

### 实际项目中典型的 HIPIFY 工作流

```bash
# Step 1: Run hipify-clang on the entire project
hipify-clang --project-dir ./my_cuda_project --output-dir ./my_hip_project

# Step 2: Try to build
cd my_hip_project
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=hipcc
make -j$(nproc)

# Step 3: Fix compilation errors (usually 5-15% of files need manual fixes)
# Common issues:
#   - warp size assumptions (32 → 64)
#   - library API differences (cuBLAS → rocBLAS)
#   - missing HIP equivalents for NVIDIA extensions

# Step 4: Run and validate
./my_program
# Compare output with CUDA version for correctness

# Step 5: Profile and optimize
rocprof --stats ./my_program
omniperf analyze -p ./profile_output
```

---

## 4. AMD GPU 架构 —— CDNA Compute Unit 深入剖析

要写出快速的 HIP kernel，理解 CU（Compute Unit）至关重要 —— 它相当于 NVIDIA 的 SM。

```
┌───────────────────────────────────────────┐
│           CDNA 3 Compute Unit (CU)        │
│                                           │
│  ┌─────────────────────────────────────┐  │
│  │  4x SIMD Units (16-wide each)      │  │
│  │  = 64 stream processors total      │  │
│  │  Execute one wavefront (64 threads) │  │
│  └─────────────────────────────────────┘  │
│                                           │
│  ┌─────────────────────────────────────┐  │
│  │  Matrix Cores                       │  │
│  │  FP16, BF16, FP8, INT8 MMA         │  │
│  │  (like NVIDIA Tensor Cores)         │  │
│  └─────────────────────────────────────┘  │
│                                           │
│  ┌──────────────┐  ┌──────────────────┐  │
│  │  Scalar Unit │  │  LDS (64 KB)     │  │
│  │  (control)   │  │  (shared memory) │  │
│  └──────────────┘  └──────────────────┘  │
│                                           │
│  ┌─────────────────────────────────────┐  │
│  │  Vector Register File               │  │
│  │  256 KB (vs ~256 KB per SM)         │  │
│  └─────────────────────────────────────┘  │
│                                           │
│  ┌──────────────┐  ┌──────────────────┐  │
│  │  L1 Cache    │  │  Scheduler       │  │
│  │  (16 KB)     │  │  (wavefront mgr) │  │
│  └──────────────┘  └──────────────────┘  │
└───────────────────────────────────────────┘
```

### NVIDIA SM 与 AMD CU 对比

| | NVIDIA SM (Hopper) | AMD CU (CDNA 3) |
|---|---|---|
| **每单元 ALU 数** | 128 个 CUDA 核心 | 64 个 stream processor |
| **线程组** | Warp（32 线程）| Wavefront（64 线程）|
| **共享内存** | 0–228 KB（可与 L1 配置）| 64 KB LDS（固定）|
| **寄存器堆** | 256 KB | 256 KB |
| **矩阵单元** | Tensor Core（第 4 代）| Matrix Core |
| **L1 缓存** | 与 SMEM 共享（可配置）| 16 KB（与 LDS 分离）|
| **最大 wavefront/warp 数** | 每个 SM 64 个 warp | 每个 CU 32 个 wavefront |
| **Occupancy 模型** | 由 warp 隐藏延迟 | 由 wavefront 隐藏延迟（更少但更宽）|

### 从 CUDA 移植时的关键优化差异

- **Block 大小：** 在 NVIDIA 上，256 个线程 = 8 个 warp。在 AMD 上，256 个线程 = 4 个 wavefront。wavefront 更少意味着延迟隐藏能力更弱 —— 在 AMD 上应考虑每个 block 使用 512 或 1024 个线程。
- **LDS 与共享内存：** AMD 的 LDS 固定为每个 CU 64 KB（不可配置）。在 NVIDIA 上可以用共享内存换取 L1 缓存。相应地规划分块。
- **Bank 冲突：** 64 个 LDS bank（NVIDIA 上为 32 个）。在 NVIDIA 上无冲突的步长 2，在 AMD 上会造成 2 路冲突。

---


<details>
<summary>English original</summary>

**What HIPIFY Converts Automatically**

| CUDA | Converted to HIP | Status |
|------|------------------|--------|
| `cuda*.h` headers | `hip/hip_runtime.h` | Automatic |
| `cudaMalloc/Free/Memcpy` | `hipMalloc/Free/Memcpy` | Automatic |
| `cudaStream_t`, `cudaEvent_t` | `hipStream_t`, `hipEvent_t` | Automatic |
| `__syncthreads()` | `__syncthreads()` | No change needed |
| `atomicAdd()` | `atomicAdd()` | No change needed |
| `cuBLAS` calls | `rocBLAS` calls | **Partial** — API differs slightly |
| `cuDNN` calls | `MIOpen` calls | **Manual** — different API design |
| `cuFFT` calls | `rocFFT` calls | **Partial** |

**What HIPIFY Cannot Convert (Manual Work Required)**

| CUDA feature | Why it can't auto-convert | Manual fix |
|-------------|--------------------------|-----------|
| **Inline PTX assembly** | PTX is NVIDIA-specific ISA | Rewrite using HIP intrinsics or GCN assembly |
| **`__ballot_sync(0xFFFFFFFF, ...)`** | Assumes 32-bit warp mask | Use `__ballot()` (64-bit on AMD) |
| **`__shfl_sync(mask, val, lane)`** | Warp size dependent | Use `__shfl(val, lane)` with wavefront awareness |
| **Cooperative groups** | NVIDIA-specific extension | Use HIP cooperative groups (subset supported) |
| **cuDNN → MIOpen** | Completely different API | Rewrite using MIOpen API |
| **Thrust** | NVIDIA template library | Use rocThrust (mostly compatible) |
| **CUTLASS** | NVIDIA template library | Use AMD Composable Kernel (CK) |

**Typical HIPIFY Workflow for a Real Project**

```bash
# Step 1: Run hipify-clang on the entire project
hipify-clang --project-dir ./my_cuda_project --output-dir ./my_hip_project

# Step 2: Try to build
cd my_hip_project
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=hipcc
make -j$(nproc)

# Step 3: Fix compilation errors (usually 5-15% of files need manual fixes)
# Common issues:
#   - warp size assumptions (32 → 64)
#   - library API differences (cuBLAS → rocBLAS)
#   - missing HIP equivalents for NVIDIA extensions

# Step 4: Run and validate
./my_program
# Compare output with CUDA version for correctness

# Step 5: Profile and optimize
rocprof --stats ./my_program
omniperf analyze -p ./profile_output
```

---

**4. AMD GPU Architecture — CDNA Compute Unit Deep Dive**

Understanding the CU (Compute Unit) is essential for writing fast HIP kernels — it's AMD's equivalent of NVIDIA's SM.

```
┌───────────────────────────────────────────┐
│           CDNA 3 Compute Unit (CU)        │
│                                           │
│  ┌─────────────────────────────────────┐  │
│  │  4x SIMD Units (16-wide each)      │  │
│  │  = 64 stream processors total      │  │
│  │  Execute one wavefront (64 threads) │  │
│  └─────────────────────────────────────┘  │
│                                           │
│  ┌─────────────────────────────────────┐  │
│  │  Matrix Cores                       │  │
│  │  FP16, BF16, FP8, INT8 MMA         │  │
│  │  (like NVIDIA Tensor Cores)         │  │
│  └─────────────────────────────────────┘  │
│                                           │
│  ┌──────────────┐  ┌──────────────────┐  │
│  │  Scalar Unit │  │  LDS (64 KB)     │  │
│  │  (control)   │  │  (shared memory) │  │
│  └──────────────┘  └──────────────────┘  │
│                                           │
│  ┌─────────────────────────────────────┐  │
│  │  Vector Register File               │  │
│  │  256 KB (vs ~256 KB per SM)         │  │
│  └─────────────────────────────────────┘  │
│                                           │
│  ┌──────────────┐  ┌──────────────────┐  │
│  │  L1 Cache    │  │  Scheduler       │  │
│  │  (16 KB)     │  │  (wavefront mgr) │  │
│  └──────────────┘  └──────────────────┘  │
└───────────────────────────────────────────┘
```

**NVIDIA SM vs AMD CU Comparison**

| | NVIDIA SM (Hopper) | AMD CU (CDNA 3) |
|---|---|---|
| **ALUs per unit** | 128 CUDA cores | 64 stream processors |
| **Thread group** | Warp (32 threads) | Wavefront (64 threads) |
| **Shared memory** | 0–228 KB (configurable with L1) | 64 KB LDS (fixed) |
| **Register file** | 256 KB | 256 KB |
| **Matrix unit** | Tensor Cores (4th gen) | Matrix Cores |
| **L1 cache** | Shared with SMEM (configurable) | 16 KB (separate from LDS) |
| **Max wavefronts/warps** | 64 warps per SM | 32 wavefronts per CU |
| **Occupancy model** | Warps hide latency | Wavefronts hide latency (fewer but wider) |

**Key Optimization Differences When Porting from CUDA**

- **Block size:** On NVIDIA, 256 threads = 8 warps. On AMD, 256 threads = 4 wavefronts. Fewer wavefronts means less latency hiding — consider using 512 or 1024 threads per block on AMD.
- **LDS vs shared memory:** AMD's LDS is fixed at 64 KB per CU (not configurable). On NVIDIA you can trade shared memory for L1 cache. Plan your tiling accordingly.
- **Bank conflicts:** 64 LDS banks (vs 32 on NVIDIA). A stride of 2 that's conflict-free on NVIDIA causes 2-way conflicts on AMD.

---

</details>

## 5. ROCm 软件栈

```
┌──────────────────────────────────────┐
│  Your HIP Application                │
├──────────────────────────────────────┤
│  Libraries: rocBLAS, MIOpen, rocFFT  │
│             Composable Kernel (CK)   │
├──────────────────────────────────────┤
│  HIP Runtime (hiprt)                 │
├──────────────────────────────────────┤
│  ROCm Compiler (amd-clang / hipcc)   │
│  LLVM AMDGPU backend                 │
├──────────────────────────────────────┤
│  ROCr (Runtime) + ROCt (Thunk)       │
├──────────────────────────────────────┤
│  amdgpu kernel driver (Linux)        │
├──────────────────────────────────────┤
│  AMD GPU Hardware (CDNA / RDNA)      │
└──────────────────────────────────────┘
```

### ROCm 库（对应 CUDA-X）

| CUDA-X 库 | ROCm 对应项 | 备注 |
|---------------|-----------------|-------|
| cuBLAS | **rocBLAS** | GEMM、BLAS 例程 |
| cuDNN | **MIOpen** | API 不同 —— 并非可直接替换 |
| cuFFT | **rocFFT** | FFT 例程 |
| cuSPARSE | **rocSPARSE** | 稀疏矩阵运算 |
| cuRAND | **rocRAND** | 随机数生成 |
| NCCL | **RCCL** | 多 GPU 集合通信 |
| CUTLASS | **Composable Kernel (CK)** | 自定义 GEMM/attention kernel |
| Thrust | **rocThrust** | 并行算法（大部分兼容） |
| CUB | **hipCUB** | Block/warp 原语 |
| Nsight Systems | **Omnitrace** | 时间线性能剖析 |
| Nsight Compute | **Omniperf** | kernel 级分析、roofline（性能上界模型） |

### 性能剖析工具

```bash
# Timeline profiling (like nsys)
omnitrace-run -- ./my_hip_program

# Kernel-level roofline analysis (like ncu)
omniperf profile -n my_run -- ./my_hip_program
omniperf analyze -p workloads/my_run
```

---

## 6. HIP 流与异步执行

与 CUDA 流一样，HIP 流让你重叠计算、内存传输和主机工作。

```cpp
hipStream_t s1, s2;
hipStreamCreate(&s1);
hipStreamCreate(&s2);

// Overlap two independent kernels on different streams
kernel_A<<<grid, block, 0, s1>>>(d_a);
kernel_B<<<grid, block, 0, s2>>>(d_b);

// Overlap H2D copy with compute
hipMemcpyAsync(d_in, h_in, bytes, hipMemcpyHostToDevice, s1);
kernel<<<grid, block, 0, s1>>>(d_in, d_out);
hipMemcpyAsync(h_out, d_out, bytes, hipMemcpyDeviceToHost, s1);

// Wait for both streams
hipStreamSynchronize(s1);
hipStreamSynchronize(s2);
hipStreamDestroy(s1);
hipStreamDestroy(s2);
```

**用于时序的事件：**

```cpp
hipEvent_t start, stop;
hipEventCreate(&start);
hipEventCreate(&stop);

hipEventRecord(start, stream);
kernel<<<grid, block, 0, stream>>>(args);
hipEventRecord(stop, stream);
hipEventSynchronize(stop);

float ms;
hipEventElapsedTime(&ms, start, stop);
printf("Kernel: %.3f ms\n", ms);
```

**Pinned（页锁定）内存** —— 异步传输与计算重叠所必需：

```cpp
float *h_pinned;
hipHostMalloc(&h_pinned, bytes, hipHostMallocDefault);  // pinned on host
// hipMemcpyAsync now truly async (DMA engine, no CPU involvement)
hipHostFree(h_pinned);
```

---

## 7. 内存管理

### 7.1 显式分配（标准）

```cpp
float *d_ptr;
hipMalloc(&d_ptr, bytes);                               // device memory
hipMemcpy(d_ptr, h_ptr, bytes, hipMemcpyHostToDevice);  // explicit copy
hipFree(d_ptr);
```

### 7.2 托管内存（HMM）

AMD 的异构内存管理（HMM）提供了类 CUDA 的托管内存 —— runtime 在主机和设备之间自动迁移页面：

```cpp
float *managed;
hipMallocManaged(&managed, bytes);    // accessible from both host and device

// Host code: just use it
for (int i = 0; i < n; i++) managed[i] = i;

// Device code: pages migrate on demand
kernel<<<grid, block>>>(managed, n);
hipDeviceSynchronize();

// Host reads back: pages migrate back
printf("%f\n", managed[0]);
hipFree(managed);
```

**何时使用托管内存：**
- 原型设计（避免手动拷贝记录）
- 无法预测 GPU 需要哪些数据的不规则访问模式
- **不**用于性能关键路径 —— 使用 pinned 内存的显式异步拷贝总是更快

### 7.3 HIP 内存对比

| 方法 | 性能 | 易用性 | 使用场景 |
|--------|-----------|-------------|-------------|
| `hipMalloc` + `hipMemcpy` | 最佳 | 手动 | 生产 kernel |
| `hipMalloc` + `hipMemcpyAsync` + pinned | 最佳（重叠） | 手动 | 流水线重叠 |
| `hipMallocManaged` | 良好（缺页） | 自动 | 原型设计、不规则访问 |
| `hipHostMalloc` (coherent) | 中等 | 直接访问 | 小数据、host+device 共享 |

---


<details>
<summary>English original</summary>

**5. ROCm Software Stack**

```
┌──────────────────────────────────────┐
│  Your HIP Application                │
├──────────────────────────────────────┤
│  Libraries: rocBLAS, MIOpen, rocFFT  │
│             Composable Kernel (CK)   │
├──────────────────────────────────────┤
│  HIP Runtime (hiprt)                 │
├──────────────────────────────────────┤
│  ROCm Compiler (amd-clang / hipcc)   │
│  LLVM AMDGPU backend                 │
├──────────────────────────────────────┤
│  ROCr (Runtime) + ROCt (Thunk)       │
├──────────────────────────────────────┤
│  amdgpu kernel driver (Linux)        │
├──────────────────────────────────────┤
│  AMD GPU Hardware (CDNA / RDNA)      │
└──────────────────────────────────────┘
```

**ROCm Libraries (Equivalent to CUDA-X)**

| CUDA-X Library | ROCm Equivalent | Notes |
|---------------|-----------------|-------|
| cuBLAS | **rocBLAS** | GEMM, BLAS routines |
| cuDNN | **MIOpen** | Different API — not a drop-in replacement |
| cuFFT | **rocFFT** | FFT routines |
| cuSPARSE | **rocSPARSE** | Sparse matrix operations |
| cuRAND | **rocRAND** | Random number generation |
| NCCL | **RCCL** | Multi-GPU collectives |
| CUTLASS | **Composable Kernel (CK)** | Custom GEMM/attention kernels |
| Thrust | **rocThrust** | Parallel algorithms (mostly compatible) |
| CUB | **hipCUB** | Block/warp primitives |
| Nsight Systems | **Omnitrace** | Timeline profiling |
| Nsight Compute | **Omniperf** | Kernel-level analysis, roofline |

**Profiling Tools**

```bash
# Timeline profiling (like nsys)
omnitrace-run -- ./my_hip_program

# Kernel-level roofline analysis (like ncu)
omniperf profile -n my_run -- ./my_hip_program
omniperf analyze -p workloads/my_run
```

---

**6. HIP Streams and Asynchronous Execution**

Just like CUDA streams, HIP streams let you overlap compute, memory transfers, and host work.

```cpp
hipStream_t s1, s2;
hipStreamCreate(&s1);
hipStreamCreate(&s2);

// Overlap two independent kernels on different streams
kernel_A<<<grid, block, 0, s1>>>(d_a);
kernel_B<<<grid, block, 0, s2>>>(d_b);

// Overlap H2D copy with compute
hipMemcpyAsync(d_in, h_in, bytes, hipMemcpyHostToDevice, s1);
kernel<<<grid, block, 0, s1>>>(d_in, d_out);
hipMemcpyAsync(h_out, d_out, bytes, hipMemcpyDeviceToHost, s1);

// Wait for both streams
hipStreamSynchronize(s1);
hipStreamSynchronize(s2);
hipStreamDestroy(s1);
hipStreamDestroy(s2);
```

**Events for timing:**

```cpp
hipEvent_t start, stop;
hipEventCreate(&start);
hipEventCreate(&stop);

hipEventRecord(start, stream);
kernel<<<grid, block, 0, stream>>>(args);
hipEventRecord(stop, stream);
hipEventSynchronize(stop);

float ms;
hipEventElapsedTime(&ms, start, stop);
printf("Kernel: %.3f ms\n", ms);
```

**Pinned (page-locked) memory** — required for async transfers to overlap with compute:

```cpp
float *h_pinned;
hipHostMalloc(&h_pinned, bytes, hipHostMallocDefault);  // pinned on host
// hipMemcpyAsync now truly async (DMA engine, no CPU involvement)
hipHostFree(h_pinned);
```

---

**7. Memory Management**

**7.1 Explicit Allocation (Standard)**

```cpp
float *d_ptr;
hipMalloc(&d_ptr, bytes);                               // device memory
hipMemcpy(d_ptr, h_ptr, bytes, hipMemcpyHostToDevice);  // explicit copy
hipFree(d_ptr);
```

**7.2 Managed Memory (HMM)**

AMD's Heterogeneous Memory Management (HMM) provides CUDA-like managed memory — the runtime migrates pages between host and device automatically:

```cpp
float *managed;
hipMallocManaged(&managed, bytes);    // accessible from both host and device

// Host code: just use it
for (int i = 0; i < n; i++) managed[i] = i;

// Device code: pages migrate on demand
kernel<<<grid, block>>>(managed, n);
hipDeviceSynchronize();

// Host reads back: pages migrate back
printf("%f\n", managed[0]);
hipFree(managed);
```

**When to use managed memory:**
- Prototyping (avoids manual copy bookkeeping)
- Irregular access patterns where you can't predict which data the GPU needs
- **NOT** for performance-critical paths — explicit async copies with pinned memory are always faster

**7.3 HIP Memory Comparison**

| Method | Performance | Ease of use | When to use |
|--------|-----------|-------------|-------------|
| `hipMalloc` + `hipMemcpy` | Best | Manual | Production kernels |
| `hipMalloc` + `hipMemcpyAsync` + pinned | Best (overlapped) | Manual | Pipeline overlapping |
| `hipMallocManaged` | Good (page faults) | Automatic | Prototyping, irregular access |
| `hipHostMalloc` (coherent) | Moderate | Direct access | Small data, host+device shared |

---

</details>

## 8. Matrix Core 编程（rocWMMA）

Matrix Core 是 AMD 对标 NVIDIA Tensor Core 的产物。`rocWMMA` 库提供了一套 WMMA 风格的接口，与 NVIDIA 的 `nvcuda::wmma` 类似。

```cpp
#include <rocwmma/rocwmma.hpp>

using namespace rocwmma;

// Matrix dimensions: 16×16 tiles, FP16 input, FP32 accumulate
constexpr int M = 16, N = 16, K = 16;

__global__ void gemm_wmma(half* A, half* B, float* C, int lda, int ldb, int ldc) {
    // Declare matrix fragments (live in registers)
    fragment<matrix_a, M, N, K, half, row_major> frag_a;
    fragment<matrix_b, M, N, K, half, col_major> frag_b;
    fragment<accumulator, M, N, K, float>         frag_c;

    // Initialize accumulator to zero
    fill_fragment(frag_c, 0.0f);

    // Load tiles from global memory into fragments
    load_matrix_sync(frag_a, A + blockRow * M * lda, lda);
    load_matrix_sync(frag_b, B + blockCol * N, ldb);

    // Matrix multiply-accumulate: C += A × B
    mma_sync(frag_c, frag_a, frag_b, frag_c);

    // Store result back to global memory
    store_matrix_sync(C + blockRow * M * ldc + blockCol * N, frag_c, ldc, mem_row_major);
}
```

**MI300X 上支持的数据类型：**

| 输入类型 | 累加类型 | 分块大小 | 吞吐 |
|-----------|----------------|-----------|------------|
| FP16      | FP32           | 16×16×16  | 全速率  |
| BF16      | FP32           | 16×16×16  | 全速率  |
| FP8 (E4M3/E5M2) | FP32    | 16×16×32  | 2× FP16   |
| INT8      | INT32          | 16×16×32  | 2× FP16   |
| FP64      | FP64           | 16×16×4   | 全速率  |

要写生产级质量的 GEMM kernel，请使用 **Composable Kernel (CK)** —— 即 AMD 对标 CUTLASS 的实现。CK 提供基于模板、自动调优的矩阵乘与 attention kernel，面向 Matrix Core。

---

## 9. 使用 RCCL 的多 GPU 编程

RCCL（ROCm Communication Collectives Library）是 AMD 对标 NCCL 的实现。它实现了 ring-allreduce、all-gather、广播等集合通信操作，可跨多块通过 Infinity Fabric 或 PCIe 互连的 AMD GPU。

```cpp
#include <rccl/rccl.h>

// Initialize one communicator per GPU
int nGPUs = 8;
ncclComm_t comms[8];
int devs[8] = {0, 1, 2, 3, 4, 5, 6, 7};
ncclCommInitAll(comms, nGPUs, devs);

// All-reduce: sum gradients across all GPUs
for (int i = 0; i < nGPUs; i++) {
    hipSetDevice(i);
    ncclAllReduce(send_buf[i], recv_buf[i], count,
                  ncclFloat, ncclSum, comms[i], streams[i]);
}

// Synchronize all streams
for (int i = 0; i < nGPUs; i++) {
    hipSetDevice(i);
    hipStreamSynchronize(streams[i]);
}
```

**MI300X 系统上的多 GPU 拓扑：**

```
8× MI300X in OAM form factor:

  GPU 0 ←─ Infinity Fabric ──→ GPU 1
    │                             │
    │         ┌───────────┐       │
    ├─────────┤ xGMI/IF  ├───────┤
    │         │  switch   │       │
    ├─────────┤  fabric   ├───────┤
    │         └───────────┘       │
  GPU 2 ←─────────────────────→ GPU 3
    ⋮                             ⋮
  GPU 6 ←─────────────────────→ GPU 7

Each link: ~400 GB/s bidirectional (comparable to NVLink 4.0)
All-to-all bandwidth: sufficient for 8-way tensor parallelism
```

**AMD 上的 PyTorch —— 开箱即用：**

```bash
# Install ROCm-enabled PyTorch
pip install torch --index-url https://download.pytorch.org/whl/rocm6.3

# Same code, different backend
import torch
x = torch.randn(4096, 4096, device='cuda')  # 'cuda' maps to HIP on AMD
y = torch.mm(x, x)  # calls rocBLAS under the hood
```

PyTorch 以 HIP 作为后端 —— `torch.cuda.*` API 在 AMD GPU 上行为完全一致。这正是 HIP 兼容 CUDA 的 API 设计如此重要的原因。

---

## 10. 资源

| Resource | URL |
|----------|-----|
| ROCm Documentation | https://rocm.docs.amd.com/ |
| HIP Programming Guide | https://rocm.docs.amd.com/projects/HIP/en/latest/ |
| HIPIFY | https://rocm.docs.amd.com/projects/HIPIFY/en/latest/ |
| Composable Kernel (CK) | https://github.com/ROCm/composable_kernel |
| Omniperf | https://rocm.docs.amd.com/projects/omniperf/en/latest/ |
| AMD Instinct MI300X | https://www.amd.com/en/products/accelerators/instinct/mi300/mi300x.html |

---


<details>
<summary>English original</summary>

**8. Matrix Core Programming (rocWMMA)**

Matrix Cores are AMD's equivalent of NVIDIA Tensor Cores. The `rocWMMA` library provides a WMMA-style interface similar to NVIDIA's `nvcuda::wmma`.

```cpp
#include <rocwmma/rocwmma.hpp>

using namespace rocwmma;

// Matrix dimensions: 16×16 tiles, FP16 input, FP32 accumulate
constexpr int M = 16, N = 16, K = 16;

__global__ void gemm_wmma(half* A, half* B, float* C, int lda, int ldb, int ldc) {
    // Declare matrix fragments (live in registers)
    fragment<matrix_a, M, N, K, half, row_major> frag_a;
    fragment<matrix_b, M, N, K, half, col_major> frag_b;
    fragment<accumulator, M, N, K, float>         frag_c;

    // Initialize accumulator to zero
    fill_fragment(frag_c, 0.0f);

    // Load tiles from global memory into fragments
    load_matrix_sync(frag_a, A + blockRow * M * lda, lda);
    load_matrix_sync(frag_b, B + blockCol * N, ldb);

    // Matrix multiply-accumulate: C += A × B
    mma_sync(frag_c, frag_a, frag_b, frag_c);

    // Store result back to global memory
    store_matrix_sync(C + blockRow * M * ldc + blockCol * N, frag_c, ldc, mem_row_major);
}
```

**Supported data types on MI300X:**

| Input type | Accumulate type | Tile size | Throughput |
|-----------|----------------|-----------|------------|
| FP16      | FP32           | 16×16×16  | Full rate  |
| BF16      | FP32           | 16×16×16  | Full rate  |
| FP8 (E4M3/E5M2) | FP32    | 16×16×32  | 2× FP16   |
| INT8      | INT32          | 16×16×32  | 2× FP16   |
| FP64      | FP64           | 16×16×4   | Full rate  |

For production-quality GEMM kernels, use **Composable Kernel (CK)** — AMD's equivalent of CUTLASS. CK provides template-based, auto-tuned matrix multiply and attention kernels that target Matrix Cores.

---

**9. Multi-GPU Programming with RCCL**

RCCL (ROCm Communication Collectives Library) is AMD's equivalent of NCCL. It implements ring-allreduce, all-gather, broadcast, and other collectives across multiple AMD GPUs connected via Infinity Fabric or PCIe.

```cpp
#include <rccl/rccl.h>

// Initialize one communicator per GPU
int nGPUs = 8;
ncclComm_t comms[8];
int devs[8] = {0, 1, 2, 3, 4, 5, 6, 7};
ncclCommInitAll(comms, nGPUs, devs);

// All-reduce: sum gradients across all GPUs
for (int i = 0; i < nGPUs; i++) {
    hipSetDevice(i);
    ncclAllReduce(send_buf[i], recv_buf[i], count,
                  ncclFloat, ncclSum, comms[i], streams[i]);
}

// Synchronize all streams
for (int i = 0; i < nGPUs; i++) {
    hipSetDevice(i);
    hipStreamSynchronize(streams[i]);
}
```

**Multi-GPU topology on MI300X systems:**

```
8× MI300X in OAM form factor:

  GPU 0 ←─ Infinity Fabric ──→ GPU 1
    │                             │
    │         ┌───────────┐       │
    ├─────────┤ xGMI/IF  ├───────┤
    │         │  switch   │       │
    ├─────────┤  fabric   ├───────┤
    │         └───────────┘       │
  GPU 2 ←─────────────────────→ GPU 3
    ⋮                             ⋮
  GPU 6 ←─────────────────────→ GPU 7

Each link: ~400 GB/s bidirectional (comparable to NVLink 4.0)
All-to-all bandwidth: sufficient for 8-way tensor parallelism
```

**PyTorch on AMD — it just works:**

```bash
# Install ROCm-enabled PyTorch
pip install torch --index-url https://download.pytorch.org/whl/rocm6.3

# Same code, different backend
import torch
x = torch.randn(4096, 4096, device='cuda')  # 'cuda' maps to HIP on AMD
y = torch.mm(x, x)  # calls rocBLAS under the hood
```

PyTorch uses HIP as the backend — the `torch.cuda.*` API works identically on AMD GPUs. This is why HIP's CUDA-compatible API design was so important.

---

**10. Resources**

| Resource | URL |
|----------|-----|
| ROCm Documentation | https://rocm.docs.amd.com/ |
| HIP Programming Guide | https://rocm.docs.amd.com/projects/HIP/en/latest/ |
| HIPIFY | https://rocm.docs.amd.com/projects/HIPIFY/en/latest/ |
| Composable Kernel (CK) | https://github.com/ROCm/composable_kernel |
| Omniperf | https://rocm.docs.amd.com/projects/omniperf/en/latest/ |
| AMD Instinct MI300X | https://www.amd.com/en/products/accelerators/instinct/mi300/mi300x.html |

---

</details>

## 11. 项目

1. **HIPIFY 你的 CUDA kernel** — 拿来 Sub-Track 3 里的 CUDA 向量加和 matmul。用 `hipify-clang` 转成 HIP。用 `hipcc` 构建。在 AMD GPU（或 NVIDIA 上的 ROCm Docker）上运行。验证输出一致。
2. **Wavefront 与 warp** — 写一个并行归约 kernel。分别在 AMD（wavefront=64）和 NVIDIA（warp=32）上运行。测量 wavefront 宽度差异如何影响性能与代码结构。
3. **用 Omniperf 做 profiling** — 用 `omniperf` 对你写的 HIP 分块 matmul 做 profiling。生成 roofline（性能上界模型）图。与同一 kernel 在 NVIDIA `ncu` roofline 上的结果对比。
4. **rocBLAS 与 cuBLAS** — 调用 rocBLAS `rocblas_sgemm` 做矩阵乘。对相同矩阵尺寸，比较其与 cuBLAS `cublasSgemm` 的性能和 API。
5. **HIPIFY 一个真实项目** — 挑一个小型开源 CUDA 项目（例如卷积 kernel 或排序算法）。跑完整 HIPIFY 工作流：转换 → 构建 → 修错 → 验证 → profiling。
6. **流重叠** — 实现一条流水线，在 3 个流上重叠 H2D 拷贝、kernel 执行和 D2H 拷贝。测量相对同步执行的吞吐提升。
7. **rocWMMA GEMM** — 用 rocWMMA fragment 写一个分块 FP16 矩阵乘。对 M=N=K=4096 比较其与 rocBLAS `rocblas_hgemm` 的吞吐。
8. **多 GPU allreduce** — 用 RCCL 在 2 个以上 GPU 上对 1 GB float 数组求和。测量带宽并对比理论 Infinity Fabric 上限。

---

## Next

→ [**Sub-Track 5 — OpenCL 和 SYCL**](../03-OpenCL与SYCL/Guide.md) — 跨 GPU、FPGA 和 CPU 的可移植计算。


<details>
<summary>English original</summary>

**11. Projects**

1. **HIPIFY your CUDA kernel** — Take your CUDA vector add and matmul from Sub-Track 3. Convert to HIP using `hipify-clang`. Build with `hipcc`. Run on AMD GPU (or ROCm Docker on NVIDIA). Verify identical output.
2. **Wavefront vs warp** — Write a parallel reduction kernel. Run on both AMD (wavefront=64) and NVIDIA (warp=32). Measure how the wavefront width difference affects performance and code structure.
3. **Profile with Omniperf** — Profile your HIP tiled matmul with `omniperf`. Generate a roofline plot. Compare with NVIDIA `ncu` roofline for the same kernel.
4. **rocBLAS vs cuBLAS** — Call rocBLAS `rocblas_sgemm` for matrix multiply. Compare performance and API with cuBLAS `cublasSgemm` for the same matrix sizes.
5. **HIPIFY a real project** — Pick a small open-source CUDA project (e.g., a convolution kernel or sorting algorithm). Run the full HIPIFY workflow: convert → build → fix errors → validate → profile.
6. **Stream overlap** — Implement a pipeline that overlaps H2D copy, kernel execution, and D2H copy across 3 streams. Measure throughput improvement over synchronous execution.
7. **rocWMMA GEMM** — Write a tiled FP16 matrix multiply using rocWMMA fragments. Compare throughput against rocBLAS `rocblas_hgemm` for M=N=K=4096.
8. **Multi-GPU allreduce** — Use RCCL to sum a 1 GB float array across 2+ GPUs. Measure bandwidth and compare to theoretical Infinity Fabric limit.

---

**Next**

→ [**Sub-Track 5 — OpenCL and SYCL**](../03-OpenCL与SYCL/Guide.md) — portable compute across GPU, FPGA, and CPU.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/4. C++ and Parallel Computing/ROCm and HIP/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/4.%20C%2B%2B%20and%20Parallel%20Computing/ROCm%20and%20HIP/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
