---
title: OpenCL 与 SYCL（阶段 1 §4 — Sub-Track 5）
description: OpenCL 与 SYCL（阶段 1 §4 — Sub-Track 5）
---

# OpenCL 与 SYCL（阶段 1 §4 — Sub-Track 5）

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">OASP</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入剖析 · 数字基础</p>
<p class="course-identity__title">OpenCL 与 SYCL 的专项课程标识（阶段 1 §4 — Sub-Track 5）。</p>
<p class="course-identity__meta">产物：可运行的低层 demo · 度量：时序、内存、正确性</p>
</div>
</div>


**父级：** [C++ 与并行计算](../Guide.md)

> *厂商中立的并行计算 — 一套面向 CPU、GPU 和 FPGA 的编程模型。*

**前置要求：** Sub-Track 3（CUDA 与 SIMT）。OpenCL 直接映射到 CUDA 概念上；SYCL 则在其之上叠加现代 C++。

**Layer 映射：** **L1**（应用 — 你编写 kernel）、**L3**（runtime — OpenCL/SYCL runtime、设备驱动）。

---

## 为什么学这些

CUDA 把你锁死在 NVIDIA 上。HIP 让你用上 AMD。OpenCL 和 SYCL 则让你拿到**一切** — Intel GPU、AMD GPU、FPGA、CPU，甚至定制加速器。作为 AI 硬件工程师，你会遇到 NVIDIA 并非选项的系统：Intel Gaudi 加速器、Xilinx/AMD FPGA、嵌入式 SoC，或多厂商云实例。

| | CUDA | HIP | OpenCL | SYCL |
|---|---|---|---|---|
| **厂商** | 仅 NVIDIA | AMD + NVIDIA | 任意（Khronos 标准） | 任意（Khronos 标准） |
| **语言** | CUDA C++ | HIP C++ | OpenCL C（独立） | 标准 C++ |
| **Kernel 风格** | 内联 `__global__` | 内联 `__global__` | 独立的 `.cl` 源码 | 主机代码中的 lambda |
| **成熟度** | 18+ 年 | 8+ 年 | 15+ 年 | 5+ 年 |
| **AI 生态** | 主导 | 成长中 | 有限 | 成长中（Intel） |
| **最适合** | NVIDIA GPU | AMD/NVIDIA GPU | 可移植的存量代码、FPGA | 现代可移植 C++ |

---

## 第 1 部分：OpenCL

### 1.1 平台模型

OpenCL 把硬件组织成层次结构：

```
Platform (vendor driver: Intel, AMD, NVIDIA, Xilinx)
  └── Device (GPU, CPU, FPGA, accelerator)
        └── Compute Unit (CU) — SM on NVIDIA, CU on AMD, PE on FPGA
              └── Processing Element (PE) — individual ALU / thread
```

**上下文**把一个或多个设备绑定在一起。**命令队列**把工作发送到上下文内的特定设备。

```
Host (CPU)
  │
  ├── Context ─────────── Device 0 (GPU)
  │     │                     └── Command Queue 0
  │     │
  │     └──────────────── Device 1 (FPGA)
  │                           └── Command Queue 1
  │
  └── Buffers, Programs, Kernels (shared within context)
```

### 1.2 执行模型

OpenCL 的执行模型直接映射到 CUDA 概念：

| OpenCL | 对应的 CUDA 概念 | 含义 |
|--------|----------------|---------|
| Work-item | 线程 | 单个执行实例 |
| Work-group | Block | 一组带共享内存的 work-item |
| NDRange | Grid | 全局问题空间（1D/2D/3D） |
| `get_global_id(0)` | `blockIdx.x * blockDim.x + threadIdx.x` | 全局索引 |
| `get_local_id(0)` | `threadIdx.x` | work-group 内的局部索引 |
| `get_group_id(0)` | `blockIdx.x` | work-group 索引 |
| `barrier(CLK_LOCAL_MEM_FENCE)` | `__syncthreads()` | work-group 屏障 |

### 1.3 内存模型

```
OpenCL memory spaces:

┌─────────────────────────────────────────────┐
│ Global Memory (device DRAM / HBM)           │  ← CUDA: global memory
│   Accessible by all work-items              │
│   Slow (~300 cycles)                        │
├─────────────────────────────────────────────┤
│ Constant Memory                              │  ← CUDA: constant memory
│   Read-only, cached                          │
├─────────────────────────────────────────────┤
│ Local Memory (per work-group)               │  ← CUDA: shared memory (__shared__)
│   Fast (~4 cycles), programmer-managed      │
│   Shared within work-group                  │
├─────────────────────────────────────────────┤
│ Private Memory (per work-item)              │  ← CUDA: registers / local
│   Fastest, compiler-allocated               │
└─────────────────────────────────────────────┘
```

### 1.4 OpenCL C Kernel

kernel 用 **OpenCL C**（一种 C99 方言）编写，并在 runtime 期间从源码字符串编译。

```c
// kernel.cl — vector addition
__kernel void vector_add(
    __global const float* a,
    __global const float* b,
    __global float* c,
    const int n)
{
    int i = get_global_id(0);
    if (i < n) {
        c[i] = a[i] + b[i];
    }
}
```

**内存限定符：**
- `__global` — 指向设备全局内存的指针
- `__local` — 指向 work-group 共享内存的指针
- `__constant` — 指向常量内存的指针
- `__private` —（默认）每个 work-item 一份


<details>
<summary>English original</summary>

**OpenCL and SYCL (Phase 1 §4 — Sub-Track 5)**

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">OASP</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Digital Foundations</p>
<p class="course-identity__title">Specialized course identity for OpenCL and SYCL (Phase 1 §4 — Sub-Track 5).</p>
<p class="course-identity__meta">Artifact: working low-level demo · Measure: timing, memory, correctness</p>
</div>
</div>


**Parent:** [C++ and Parallel Computing](../Guide.md)

> *Vendor-neutral parallel compute — one programming model for CPU, GPU, and FPGA.*

**Prerequisites:** Sub-Track 3 (CUDA and SIMT). OpenCL maps directly onto CUDA concepts; SYCL then adds modern C++ on top.

**Layer mapping:** **L1** (application — you write kernels), **L3** (runtime — OpenCL/SYCL runtime, device drivers).

---

**Why Learn This**

CUDA locks you to NVIDIA. HIP gets you AMD. OpenCL and SYCL get you **everything** — Intel GPUs, AMD GPUs, FPGAs, CPUs, even custom accelerators. As an AI hardware engineer, you'll encounter systems where NVIDIA isn't an option: Intel Gaudi accelerators, Xilinx/AMD FPGAs, embedded SoCs, or multi-vendor cloud instances.

| | CUDA | HIP | OpenCL | SYCL |
|---|---|---|---|---|
| **Vendor** | NVIDIA only | AMD + NVIDIA | Any (Khronos standard) | Any (Khronos standard) |
| **Language** | CUDA C++ | HIP C++ | OpenCL C (separate) | Standard C++ |
| **Kernel style** | Inline `__global__` | Inline `__global__` | Separate `.cl` source | Lambda in host code |
| **Maturity** | 18+ years | 8+ years | 15+ years | 5+ years |
| **AI ecosystem** | Dominant | Growing | Limited | Growing (Intel) |
| **Best for** | NVIDIA GPUs | AMD/NVIDIA GPUs | Portable legacy, FPGAs | Modern portable C++ |

---

**Part 1: OpenCL**

**1.1 Platform Model**

OpenCL organises hardware into a hierarchy:

```
Platform (vendor driver: Intel, AMD, NVIDIA, Xilinx)
  └── Device (GPU, CPU, FPGA, accelerator)
        └── Compute Unit (CU) — SM on NVIDIA, CU on AMD, PE on FPGA
              └── Processing Element (PE) — individual ALU / thread
```

**Context** binds one or more devices together. **Command queues** send work to a specific device within a context.

```
Host (CPU)
  │
  ├── Context ─────────── Device 0 (GPU)
  │     │                     └── Command Queue 0
  │     │
  │     └──────────────── Device 1 (FPGA)
  │                           └── Command Queue 1
  │
  └── Buffers, Programs, Kernels (shared within context)
```

**1.2 Execution Model**

OpenCL's execution model maps directly onto CUDA concepts:

| OpenCL | CUDA equivalent | Meaning |
|--------|----------------|---------|
| Work-item | Thread | Single execution instance |
| Work-group | Block | Group of work-items with shared memory |
| NDRange | Grid | Global problem space (1D/2D/3D) |
| `get_global_id(0)` | `blockIdx.x * blockDim.x + threadIdx.x` | Global index |
| `get_local_id(0)` | `threadIdx.x` | Local index within work-group |
| `get_group_id(0)` | `blockIdx.x` | Work-group index |
| `barrier(CLK_LOCAL_MEM_FENCE)` | `__syncthreads()` | Work-group barrier |

**1.3 Memory Model**

```
OpenCL memory spaces:

┌─────────────────────────────────────────────┐
│ Global Memory (device DRAM / HBM)           │  ← CUDA: global memory
│   Accessible by all work-items              │
│   Slow (~300 cycles)                        │
├─────────────────────────────────────────────┤
│ Constant Memory                              │  ← CUDA: constant memory
│   Read-only, cached                          │
├─────────────────────────────────────────────┤
│ Local Memory (per work-group)               │  ← CUDA: shared memory (__shared__)
│   Fast (~4 cycles), programmer-managed      │
│   Shared within work-group                  │
├─────────────────────────────────────────────┤
│ Private Memory (per work-item)              │  ← CUDA: registers / local
│   Fastest, compiler-allocated               │
└─────────────────────────────────────────────┘
```

**1.4 OpenCL C Kernel**

Kernels are written in **OpenCL C** (a C99 dialect) and compiled at runtime from source strings.

```c
// kernel.cl — vector addition
__kernel void vector_add(
    __global const float* a,
    __global const float* b,
    __global float* c,
    const int n)
{
    int i = get_global_id(0);
    if (i < n) {
        c[i] = a[i] + b[i];
    }
}
```

**Memory qualifiers:**
- `__global` — pointer to device global memory
- `__local` — pointer to work-group shared memory
- `__constant` — pointer to constant memory
- `__private` — (default) per-work-item

</details>

### 1.5 Host 代码 —— 完整搭建

OpenCL host 代码很啰嗦 —— 必须显式创建 platform、device、上下文、queue、buffer、program 和 kernel 对象。这些样板代码是可移植性的代价。

```cpp
#include <CL/cl.h>
#include <cstdio>
#include <cstdlib>

int main() {
    const int N = 1024;
    size_t bytes = N * sizeof(float);
    float *h_a = (float*)malloc(bytes);
    float *h_b = (float*)malloc(bytes);
    float *h_c = (float*)malloc(bytes);

    for (int i = 0; i < N; i++) { h_a[i] = i; h_b[i] = i * 2; }

    // ── Step 1: Get platform and device ────────────────────────
    cl_platform_id platform;
    clGetPlatformIDs(1, &platform, NULL);

    cl_device_id device;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

    // ── Step 2: Create context and command queue ───────────────
    cl_context ctx = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);
    cl_command_queue queue = clCreateCommandQueueWithProperties(ctx, device, 0, NULL);

    // ── Step 3: Create buffers ─────────────────────────────────
    cl_mem d_a = clCreateBuffer(ctx, CL_MEM_READ_ONLY,  bytes, NULL, NULL);
    cl_mem d_b = clCreateBuffer(ctx, CL_MEM_READ_ONLY,  bytes, NULL, NULL);
    cl_mem d_c = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, bytes, NULL, NULL);

    // ── Step 4: Upload data ────────────────────────────────────
    clEnqueueWriteBuffer(queue, d_a, CL_TRUE, 0, bytes, h_a, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, d_b, CL_TRUE, 0, bytes, h_b, 0, NULL, NULL);

    // ── Step 5: Build program from source ──────────────────────
    const char* src = "__kernel void vector_add("
        "__global const float* a, __global const float* b,"
        "__global float* c, const int n) {"
        "  int i = get_global_id(0);"
        "  if (i < n) c[i] = a[i] + b[i];"
        "}";

    cl_program prog = clCreateProgramWithSource(ctx, 1, &src, NULL, NULL);
    clBuildProgram(prog, 1, &device, "-cl-fast-relaxed-math", NULL, NULL);

    // ── Step 6: Create kernel and set arguments ────────────────
    cl_kernel kernel = clCreateKernel(prog, "vector_add", NULL);
    clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_a);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_b);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_c);
    clSetKernelArg(kernel, 3, sizeof(int),    &N);

    // ── Step 7: Launch kernel ──────────────────────────────────
    size_t global_size = N;
    size_t local_size  = 256;
    clEnqueueNDRangeKernel(queue, kernel, 1, NULL,
                           &global_size, &local_size, 0, NULL, NULL);

    // ── Step 8: Read back results ──────────────────────────────
    clEnqueueReadBuffer(queue, d_c, CL_TRUE, 0, bytes, h_c, 0, NULL, NULL);

    printf("c[0] = %f, c[%d] = %f\n", h_c[0], N-1, h_c[N-1]);

    // ── Cleanup ────────────────────────────────────────────────
    clReleaseKernel(kernel);
    clReleaseProgram(prog);
    clReleaseMemObject(d_a); clReleaseMemObject(d_b); clReleaseMemObject(d_c);
    clReleaseCommandQueue(queue);
    clReleaseContext(ctx);
    free(h_a); free(h_b); free(h_c);
}
```

```bash
# Compile (link against OpenCL ICD loader)
gcc -O2 -o vecadd vecadd.c -lOpenCL

# Run (works on any GPU/CPU with an OpenCL driver)
./vecadd
```

### 1.6 使用 Local Memory 的分块矩阵乘

这是 CUDA shared-memory 分块 GEMM 的 OpenCL 等价实现：

```c
// matmul.cl — tiled matrix multiply using local memory
#define TILE 16

__kernel void matmul(
    __global const float* A,
    __global const float* B,
    __global float* C,
    const int N)
{
    int row = get_local_id(1);
    int col = get_local_id(0);
    int gRow = get_global_id(1);
    int gCol = get_global_id(0);

    __local float tileA[TILE][TILE];
    __local float tileB[TILE][TILE];

    float sum = 0.0f;

    for (int t = 0; t < N / TILE; t++) {
        // Load tiles into local memory
        tileA[row][col] = A[gRow * N + t * TILE + col];
        tileB[row][col] = B[(t * TILE + row) * N + gCol];
        barrier(CLK_LOCAL_MEM_FENCE);

        // Compute partial dot product from local memory
        for (int k = 0; k < TILE; k++)
            sum += tileA[row][k] * tileB[k][col];
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    C[gRow * N + gCol] = sum;
}
```

用 2D NDRange 启动：

```cpp
size_t global[2] = {N, N};
size_t local[2]  = {TILE, TILE};   // 16×16 = 256 work-items per group
clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global, local, 0, NULL, NULL);
```

### 1.7 基于事件的性能剖析

```cpp
// Create queue with profiling enabled
cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
cl_command_queue queue = clCreateCommandQueueWithProperties(ctx, device, props, NULL);

cl_event event;
clEnqueueNDRangeKernel(queue, kernel, 1, NULL,
                       &global_size, &local_size, 0, NULL, &event);
clWaitForEvents(1, &event);

cl_ulong start, end;
clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(start), &start, NULL);
clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END,   sizeof(end),   &end,   NULL);

printf("Kernel time: %.3f ms\n", (end - start) / 1e6);
```

### 1.8 设备查询

在假设设备能力之前，始终先查询设备支持什么：

```cpp
char name[128];
cl_uint cu_count;
cl_ulong global_mem, local_mem;
size_t max_wg;

clGetDeviceInfo(device, CL_DEVICE_NAME,                128, name,       NULL);
clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS,     4, &cu_count,  NULL);
clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE,       8, &global_mem,NULL);
clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE,        8, &local_mem, NULL);
clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE,sizeof(max_wg), &max_wg, NULL);

printf("Device: %s\n", name);
printf("CUs: %u, Global mem: %lu MB, Local mem: %lu KB, Max WG: %zu\n",
       cu_count, global_mem / (1024*1024), local_mem / 1024, max_wg);
```

---

## 第 2 部分：SYCL

### 2.1 SYCL 是什么

SYCL 是构建在 OpenCL（以及现在的其他后端）之上的**现代 C++ 抽象**。它通过将 kernel 表达为 C++ lambda 并使用 RAII 访问器管理 buffer，消除了 OpenCL 的样板代码。

```
OpenCL:
  Platform → Device → Context → Queue → Buffer → Program → Kernel → SetArgs → Launch
  (~50 lines of setup)

SYCL:
  queue q;
  q.submit([&](handler& h) { h.parallel_for(..., [=](id<1> i) { ... }); });
  (~5 lines)
```

**SYCL 实现：**

| 实现 | 厂商 | 后端目标 |
|---------------|--------|-----------------|
| **Intel oneAPI DPC++** | Intel | Intel GPU、CPU、FPGA、NVIDIA GPU（通过插件） |
| **AdaptiveCpp**（原 hipSYCL） | 开源 | AMD GPU、NVIDIA GPU、CPU |
| **ComputeCpp** | Codeplay | ARM Mali、Renesas、定制 |
| **triSYCL** | Xilinx/AMD | FPGA（实验性） |

### 2.2 SYCL Hello World — 向量加法

```cpp
#include <sycl/sycl.hpp>
#include <vector>
#include <iostream>

int main() {
    const int N = 1024;
    std::vector<float> a(N), b(N), c(N);

    for (int i = 0; i < N; i++) { a[i] = i; b[i] = i * 2; }

    {
        // Create buffers that wrap host vectors
        sycl::buffer<float> buf_a(a.data(), N);
        sycl::buffer<float> buf_b(b.data(), N);
        sycl::buffer<float> buf_c(c.data(), N);

        // Select device (GPU preferred, falls back to CPU)
        sycl::queue q(sycl::gpu_selector_v);
        std::cout << "Device: "
                  << q.get_device().get_info<sycl::info::device::name>()
                  << "\n";

        q.submit([&](sycl::handler& h) {
            // Accessors: read from a,b; write to c
            auto A = buf_a.get_access<sycl::access::mode::read>(h);
            auto B = buf_b.get_access<sycl::access::mode::read>(h);
            auto C = buf_c.get_access<sycl::access::mode::write>(h);

            h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
                C[i] = A[i] + B[i];
            });
        });
    }  // buffer destructor → implicit copy back to host vectors

    std::cout << "c[0] = " << c[0] << ", c[" << N-1 << "] = " << c[N-1] << "\n";
}
```

```bash
# Compile with Intel DPC++
icpx -fsycl -o vecadd vecadd.cpp

# Or with AdaptiveCpp (targeting NVIDIA)
acpp -o vecadd vecadd.cpp --acpp-targets="cuda:sm_80"

# Run
./vecadd
```

**与 OpenCL 对比：** SYCL 的 buffer/accessor 模型自动处理数据移动。无需显式 `clCreateBuffer`、`clEnqueueWriteBuffer`、`clSetKernelArg` — SYCL runtime 会推断依赖关系并调度传输。


<details>
<summary>English original</summary>

**1.7 Event-Based Profiling**

```cpp
// Create queue with profiling enabled
cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
cl_command_queue queue = clCreateCommandQueueWithProperties(ctx, device, props, NULL);

cl_event event;
clEnqueueNDRangeKernel(queue, kernel, 1, NULL,
                       &global_size, &local_size, 0, NULL, &event);
clWaitForEvents(1, &event);

cl_ulong start, end;
clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(start), &start, NULL);
clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END,   sizeof(end),   &end,   NULL);

printf("Kernel time: %.3f ms\n", (end - start) / 1e6);
```

**1.8 Device Query**

Always query what the device supports before assuming capabilities:

```cpp
char name[128];
cl_uint cu_count;
cl_ulong global_mem, local_mem;
size_t max_wg;

clGetDeviceInfo(device, CL_DEVICE_NAME,                128, name,       NULL);
clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS,     4, &cu_count,  NULL);
clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE,       8, &global_mem,NULL);
clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE,        8, &local_mem, NULL);
clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE,sizeof(max_wg), &max_wg, NULL);

printf("Device: %s\n", name);
printf("CUs: %u, Global mem: %lu MB, Local mem: %lu KB, Max WG: %zu\n",
       cu_count, global_mem / (1024*1024), local_mem / 1024, max_wg);
```

---

**Part 2: SYCL**

**2.1 What SYCL Is**

SYCL is a **modern C++ abstraction** built on top of OpenCL (and now other backends). It eliminates OpenCL's boilerplate by expressing kernels as C++ lambdas and managing buffers with RAII accessors.

```
OpenCL:
  Platform → Device → Context → Queue → Buffer → Program → Kernel → SetArgs → Launch
  (~50 lines of setup)

SYCL:
  queue q;
  q.submit([&](handler& h) { h.parallel_for(..., [=](id<1> i) { ... }); });
  (~5 lines)
```

**SYCL implementations:**

| Implementation | Vendor | Backend targets |
|---------------|--------|-----------------|
| **Intel oneAPI DPC++** | Intel | Intel GPUs, CPUs, FPGAs, NVIDIA GPUs (via plugin) |
| **AdaptiveCpp** (formerly hipSYCL) | Open-source | AMD GPUs, NVIDIA GPUs, CPUs |
| **ComputeCpp** | Codeplay | ARM Mali, Renesas, custom |
| **triSYCL** | Xilinx/AMD | FPGAs (experimental) |

**2.2 SYCL Hello World — Vector Addition**

```cpp
#include <sycl/sycl.hpp>
#include <vector>
#include <iostream>

int main() {
    const int N = 1024;
    std::vector<float> a(N), b(N), c(N);

    for (int i = 0; i < N; i++) { a[i] = i; b[i] = i * 2; }

    {
        // Create buffers that wrap host vectors
        sycl::buffer<float> buf_a(a.data(), N);
        sycl::buffer<float> buf_b(b.data(), N);
        sycl::buffer<float> buf_c(c.data(), N);

        // Select device (GPU preferred, falls back to CPU)
        sycl::queue q(sycl::gpu_selector_v);
        std::cout << "Device: "
                  << q.get_device().get_info<sycl::info::device::name>()
                  << "\n";

        q.submit([&](sycl::handler& h) {
            // Accessors: read from a,b; write to c
            auto A = buf_a.get_access<sycl::access::mode::read>(h);
            auto B = buf_b.get_access<sycl::access::mode::read>(h);
            auto C = buf_c.get_access<sycl::access::mode::write>(h);

            h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
                C[i] = A[i] + B[i];
            });
        });
    }  // buffer destructor → implicit copy back to host vectors

    std::cout << "c[0] = " << c[0] << ", c[" << N-1 << "] = " << c[N-1] << "\n";
}
```

```bash
# Compile with Intel DPC++
icpx -fsycl -o vecadd vecadd.cpp

# Or with AdaptiveCpp (targeting NVIDIA)
acpp -o vecadd vecadd.cpp --acpp-targets="cuda:sm_80"

# Run
./vecadd
```

**Compare to OpenCL:** SYCL's buffer/accessor model handles data movement automatically. No explicit `clCreateBuffer`, `clEnqueueWriteBuffer`, `clSetKernelArg` — the SYCL runtime infers dependencies and schedules transfers.

</details>

### 2.3 USM（Unified Shared Memory）—— SYCL 中类 CUDA 的指针

Buffers/accessors 安全，但 CUDA 程序员对其不熟悉。USM 提供裸指针：

```cpp
#include <sycl/sycl.hpp>

int main() {
    sycl::queue q(sycl::gpu_selector_v);
    const int N = 1024;

    // Allocate device memory (like cudaMalloc)
    float* d_a = sycl::malloc_device<float>(N, q);
    float* d_b = sycl::malloc_device<float>(N, q);
    float* d_c = sycl::malloc_device<float>(N, q);

    // Host data
    std::vector<float> h_a(N, 1.0f), h_b(N, 2.0f), h_c(N);

    // Copy to device (like cudaMemcpy)
    q.memcpy(d_a, h_a.data(), N * sizeof(float));
    q.memcpy(d_b, h_b.data(), N * sizeof(float));
    q.wait();

    // Launch kernel
    q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
        d_c[i] = d_a[i] + d_b[i];
    }).wait();

    // Copy back
    q.memcpy(h_c.data(), d_c, N * sizeof(float)).wait();

    // Free
    sycl::free(d_a, q);
    sycl::free(d_b, q);
    sycl::free(d_c, q);
}
```

**USM 分配类型：**

| 类型 | 主机访问 | 设备访问 | 数据搬移 |
|------|-----------|--------------|---------------|
| `malloc_device` | 否 | 是 | 显式 `memcpy` |
| `malloc_host` | 是 | 是（经 PCIe） | 隐式（慢） |
| `malloc_shared` | 是 | 是 | 自动迁移 |

### 2.4 局部内存与工作组内分块

SYCL 通过 `local_accessor` 暴露局部内存：

```cpp
q.submit([&](sycl::handler& h) {
    auto A = buf_a.get_access<sycl::access::mode::read>(h);
    auto B = buf_b.get_access<sycl::access::mode::read>(h);
    auto C = buf_c.get_access<sycl::access::mode::write>(h);

    constexpr int TILE = 16;
    sycl::local_accessor<float, 1> tileA(sycl::range<1>(TILE * TILE), h);
    sycl::local_accessor<float, 1> tileB(sycl::range<1>(TILE * TILE), h);

    h.parallel_for(
        sycl::nd_range<2>({N, N}, {TILE, TILE}),
        [=](sycl::nd_item<2> item) {
            int row = item.get_local_id(1);
            int col = item.get_local_id(0);
            int gRow = item.get_global_id(1);
            int gCol = item.get_global_id(0);

            float sum = 0.0f;
            for (int t = 0; t < N / TILE; t++) {
                tileA[row * TILE + col] = A[gRow * N + t * TILE + col];
                tileB[row * TILE + col] = B[(t * TILE + row) * N + gCol];
                item.barrier(sycl::access::fence_space::local_space);

                for (int k = 0; k < TILE; k++)
                    sum += tileA[row * TILE + k] * tileB[k * TILE + col];
                item.barrier(sycl::access::fence_space::local_space);
            }
            C[gRow * N + gCol] = sum;
        }
    );
});
```

### 2.5 Sub-Group（等价于 warp/wavefront）

SYCL `sub_group` 映射到 CUDA warp（32）或 AMD wavefront（64）：

```cpp
q.parallel_for(
    sycl::nd_range<1>(N, 256),
    [=](sycl::nd_item<1> item) {
        auto sg = item.get_sub_group();
        int lane = sg.get_local_id();
        int sg_size = sg.get_local_range()[0];   // 32 on NVIDIA, 64 on AMD

        float val = data[item.get_global_id(0)];

        // Warp-level reduction (portable!)
        for (int offset = sg_size / 2; offset > 0; offset /= 2)
            val += sycl::shift_group_left(sg, val, offset);

        if (lane == 0)
            result[sg.get_group_id()] = val;
    }
);
```

这段代码无需修改即可在 NVIDIA（sub_group = 32）、AMD（sub_group = 64）和 Intel（sub_group = 8/16/32）上正确运行。

### 2.6 面向 FPGA 的 SYCL（Intel oneAPI）

Intel oneAPI DPC++ 可以把 SYCL kernel 编译为 FPGA bitstream。同一份 C++ 代码既能面向 GPU，也能面向 FPGA —— 但为 FPGA 优化的 kernel 写法不同（pipes、循环展开、banking）。

```cpp
// FPGA-optimised: use pipes for streaming
using PipeA = sycl::ext::intel::pipe<class pA, float, 16>;  // depth 16

// Producer kernel
q.submit([&](sycl::handler& h) {
    h.single_task([=]() {
        for (int i = 0; i < N; i++)
            PipeA::write(input[i]);
    });
});

// Consumer kernel (runs concurrently on FPGA fabric)
q.submit([&](sycl::handler& h) {
    h.single_task([=]() {
        for (int i = 0; i < N; i++)
            output[i] = PipeA::read() * 2.0f;
    });
});
```

FPGA 编译耗时数小时（完整的布局布线），因此开发期间应使用**仿真**：

```bash
# Emulation (fast, runs on CPU)
icpx -fsycl -fintelfpga -DFPGA_EMULATOR -o emu emu.cpp

# FPGA hardware compile (30min–2hrs)
icpx -fsycl -fintelfpga -Xshardware -o hw hw.cpp
```

---


<details>
<summary>English original</summary>

**2.3 USM (Unified Shared Memory) — CUDA-Like Pointers in SYCL**

Buffers/accessors are safe but unfamiliar to CUDA programmers. USM provides raw pointers:

```cpp
#include <sycl/sycl.hpp>

int main() {
    sycl::queue q(sycl::gpu_selector_v);
    const int N = 1024;

    // Allocate device memory (like cudaMalloc)
    float* d_a = sycl::malloc_device<float>(N, q);
    float* d_b = sycl::malloc_device<float>(N, q);
    float* d_c = sycl::malloc_device<float>(N, q);

    // Host data
    std::vector<float> h_a(N, 1.0f), h_b(N, 2.0f), h_c(N);

    // Copy to device (like cudaMemcpy)
    q.memcpy(d_a, h_a.data(), N * sizeof(float));
    q.memcpy(d_b, h_b.data(), N * sizeof(float));
    q.wait();

    // Launch kernel
    q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
        d_c[i] = d_a[i] + d_b[i];
    }).wait();

    // Copy back
    q.memcpy(h_c.data(), d_c, N * sizeof(float)).wait();

    // Free
    sycl::free(d_a, q);
    sycl::free(d_b, q);
    sycl::free(d_c, q);
}
```

**USM allocation types:**

| Type | Host access | Device access | Data movement |
|------|-----------|--------------|---------------|
| `malloc_device` | No | Yes | Explicit `memcpy` |
| `malloc_host` | Yes | Yes (over PCIe) | Implicit (slow) |
| `malloc_shared` | Yes | Yes | Automatic migration |

**2.4 Local Memory and Work-Group Tiling**

SYCL exposes local memory through `local_accessor`:

```cpp
q.submit([&](sycl::handler& h) {
    auto A = buf_a.get_access<sycl::access::mode::read>(h);
    auto B = buf_b.get_access<sycl::access::mode::read>(h);
    auto C = buf_c.get_access<sycl::access::mode::write>(h);

    constexpr int TILE = 16;
    sycl::local_accessor<float, 1> tileA(sycl::range<1>(TILE * TILE), h);
    sycl::local_accessor<float, 1> tileB(sycl::range<1>(TILE * TILE), h);

    h.parallel_for(
        sycl::nd_range<2>({N, N}, {TILE, TILE}),
        [=](sycl::nd_item<2> item) {
            int row = item.get_local_id(1);
            int col = item.get_local_id(0);
            int gRow = item.get_global_id(1);
            int gCol = item.get_global_id(0);

            float sum = 0.0f;
            for (int t = 0; t < N / TILE; t++) {
                tileA[row * TILE + col] = A[gRow * N + t * TILE + col];
                tileB[row * TILE + col] = B[(t * TILE + row) * N + gCol];
                item.barrier(sycl::access::fence_space::local_space);

                for (int k = 0; k < TILE; k++)
                    sum += tileA[row * TILE + k] * tileB[k * TILE + col];
                item.barrier(sycl::access::fence_space::local_space);
            }
            C[gRow * N + gCol] = sum;
        }
    );
});
```

**2.5 Sub-Groups (Warp/Wavefront Equivalent)**

SYCL `sub_group` maps to CUDA warps (32) or AMD wavefronts (64):

```cpp
q.parallel_for(
    sycl::nd_range<1>(N, 256),
    [=](sycl::nd_item<1> item) {
        auto sg = item.get_sub_group();
        int lane = sg.get_local_id();
        int sg_size = sg.get_local_range()[0];   // 32 on NVIDIA, 64 on AMD

        float val = data[item.get_global_id(0)];

        // Warp-level reduction (portable!)
        for (int offset = sg_size / 2; offset > 0; offset /= 2)
            val += sycl::shift_group_left(sg, val, offset);

        if (lane == 0)
            result[sg.get_group_id()] = val;
    }
);
```

This code runs correctly on NVIDIA (sub_group = 32), AMD (sub_group = 64), and Intel (sub_group = 8/16/32) without modification.

**2.6 SYCL for FPGA (Intel oneAPI)**

Intel oneAPI DPC++ can compile SYCL kernels to FPGA bitstreams. The same C++ code targets GPU or FPGA — but FPGA-optimised kernels look different (pipes, loop unrolling, banking).

```cpp
// FPGA-optimised: use pipes for streaming
using PipeA = sycl::ext::intel::pipe<class pA, float, 16>;  // depth 16

// Producer kernel
q.submit([&](sycl::handler& h) {
    h.single_task([=]() {
        for (int i = 0; i < N; i++)
            PipeA::write(input[i]);
    });
});

// Consumer kernel (runs concurrently on FPGA fabric)
q.submit([&](sycl::handler& h) {
    h.single_task([=]() {
        for (int i = 0; i < N; i++)
            output[i] = PipeA::read() * 2.0f;
    });
});
```

FPGA compilation takes hours (full place-and-route), so use **emulation** during development:

```bash
# Emulation (fast, runs on CPU)
icpx -fsycl -fintelfpga -DFPGA_EMULATOR -o emu emu.cpp

# FPGA hardware compile (30min–2hrs)
icpx -fsycl -fintelfpga -Xshardware -o hw hw.cpp
```

---

</details>

## 第 3 部分：OpenCL vs SYCL 决策指南

| 标准 | 选 OpenCL | 选 SYCL |
|-----------|--------------|-------------|
| **已有 C 代码库** | 是 | 否 |
| **需要 FPGA（Xilinx）** | 是（Vitis） | 部分（仅 Intel） |
| **偏好现代 C++** | 否 | 是 |
| **Intel GPU / FPGA** | 可用 | 最佳（oneAPI） |
| **AMD GPU** | 可用 | 可用（AdaptiveCpp） |
| **NVIDIA GPU** | 可用 | 可用（DPC++ plugin、AdaptiveCpp） |
| **Runtime kernel 编译** | 是（源码字符串） | 可选（online compiler） |
| **团队熟悉 CUDA** | 迁移较难 | 迁移较易（感觉相近） |
| **长期方向** | 维护模式 | 活跃开发 |

**实用建议：** 面向多家厂商的新项目，用 SYCL（具体是 Intel DPC++ 或 AdaptiveCpp）。遗留代码或 Xilinx FPGA，OpenCL 仍是正确选择。只针对 NVIDIA 或只针对 AMD，直接用 CUDA 或 HIP —— 它们更简单也更快。

---

## 资源

| 资源 | 类型 | 侧重 |
|----------|------|-------|
| Khronos OpenCL 3.0 规范 | 标准 | 权威 API 参考 |
| Khronos SYCL 2020 规范 | 标准 | 权威 SYCL 参考 |
| Intel oneAPI DPC++ 文档 | 文档 | SYCL 实现 + FPGA |
| AdaptiveCpp（github.com/AdaptiveCpp） | 开源 | AMD/NVIDIA/CPU 上的 SYCL |
| *OpenCL Programming Guide* —— Munshi 等 | 教材 | 全面的 OpenCL |
| *Data Parallel C++* —— Reinders、Ashbaugh、Brodman | 教材 | SYCL/DPC++（Intel 提供免费 PDF） |
| Codeplay 开发者博客 | 博客 | SYCL 教程、GPU 移植指南 |

---

## 项目

| # | 项目 | 练习的概念 | 复杂度 |
|---|---------|-------------------|------------|
| 1 | **OpenCL vector add** | platform/device/context/queue 设置、kernel 编译 | 入门 |
| 2 | **SYCL vector add（buffer + USM）** | buffer/accessor 与 USM 的对比、设备选择 | 入门 |
| 3 | **OpenCL 分块矩阵乘** | local memory、barrier、2D NDRange | 中级 |
| 4 | **SYCL 分块矩阵乘** | local_accessor、nd_range、nd_item | 中级 |
| 5 | **跨设备 benchmark** | 在 CPU + GPU 上运行同一 kernel，比较吞吐 | 中级 |
| 6 | **SYCL sub-group 归约** | 可移植到 warp/wavefront 的原语 | 中级 |
| 7 | **把 CUDA kernel 移植到 SYCL** | 对同一算法比较 CUDA 与 SYCL | 中级 |
| 8 | **OpenCL 设备查询工具** | 枚举所有 platform/device，打印能力 | 入门 |
| 9 | **SYCL FPGA pipe 流水线** | Intel FPGA、pipe、仿真、硬件编译 | 高级 |
| 10 | **多设备任务图** | SYCL event 依赖，把工作拆分到 GPU+CPU | 高级 |

---

## 下一步

→ 返回 [C++ 与并行计算 hub](../Guide.md)

→ [阶段 3 —— 神经网络](../../../阶段3-人工智能/01-神经网络/Guide.md)


<details>
<summary>English original</summary>

**Part 3: OpenCL vs SYCL Decision Guide**

| Criterion | Choose OpenCL | Choose SYCL |
|-----------|--------------|-------------|
| **Existing C codebase** | Yes | No |
| **Need FPGA (Xilinx)** | Yes (Vitis) | Partial (Intel only) |
| **Modern C++ preferred** | No | Yes |
| **Intel GPU / FPGA** | Works | Best (oneAPI) |
| **AMD GPU** | Works | Works (AdaptiveCpp) |
| **NVIDIA GPU** | Works | Works (DPC++ plugin, AdaptiveCpp) |
| **Runtime kernel compilation** | Yes (source strings) | Optional (online compiler) |
| **Team knows CUDA** | Harder transition | Easier transition (similar feel) |
| **Long-term direction** | Maintenance mode | Actively developed |

**Practical advice:** For new projects targeting multiple vendors, use SYCL (specifically Intel DPC++ or AdaptiveCpp). For legacy code or Xilinx FPGA, OpenCL is still the right choice. For NVIDIA-only or AMD-only, just use CUDA or HIP — they're simpler and faster.

---

**Resources**

| Resource | Type | Focus |
|----------|------|-------|
| Khronos OpenCL 3.0 specification | Standard | Authoritative API reference |
| Khronos SYCL 2020 specification | Standard | Authoritative SYCL reference |
| Intel oneAPI DPC++ documentation | Docs | SYCL implementation + FPGA |
| AdaptiveCpp (github.com/AdaptiveCpp) | Open-source | SYCL on AMD/NVIDIA/CPU |
| *OpenCL Programming Guide* — Munshi et al. | Textbook | Comprehensive OpenCL |
| *Data Parallel C++* — Reinders, Ashbaugh, Brodman | Textbook | SYCL/DPC++ (free PDF from Intel) |
| Codeplay developer blog | Blog | SYCL tutorials, GPU porting guides |

---

**Projects**

| # | Project | Concepts practiced | Complexity |
|---|---------|-------------------|------------|
| 1 | **OpenCL vector add** | Platform/device/context/queue setup, kernel compile | Beginner |
| 2 | **SYCL vector add (buffer + USM)** | Buffer/accessor vs USM, device selection | Beginner |
| 3 | **OpenCL tiled matmul** | Local memory, barriers, 2D NDRange | Intermediate |
| 4 | **SYCL tiled matmul** | local_accessor, nd_range, nd_item | Intermediate |
| 5 | **Cross-device benchmark** | Run same kernel on CPU + GPU, compare throughput | Intermediate |
| 6 | **SYCL sub-group reduction** | Warp/wavefront-portable primitives | Intermediate |
| 7 | **Port CUDA kernel to SYCL** | Compare CUDA vs SYCL for same algorithm | Intermediate |
| 8 | **OpenCL device query tool** | Enumerate all platforms/devices, print capabilities | Beginner |
| 9 | **SYCL FPGA pipe pipeline** | Intel FPGA, pipe, emulation, hardware compile | Advanced |
| 10 | **Multi-device task graph** | SYCL event dependencies, split work across GPU+CPU | Advanced |

---

**Next**

→ Back to [C++ and Parallel Computing hub](../Guide.md)

→ [Phase 3 — Neural Networks](../../../阶段3-人工智能/01-神经网络/Guide.md)

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/4. C++ and Parallel Computing/OpenCL and SYCL/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/4.%20C%2B%2B%20and%20Parallel%20Computing/OpenCL%20and%20SYCL/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
