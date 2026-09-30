---
title: Orin Nano 8GB -- CUDA Programming Deep Dive
description: Orin Nano 8GB -- CUDA Programming Deep Dive
---

# Orin Nano 8GB -- CUDA Programming Deep Dive

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">ON8C</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Orin Nano 8GB -- CUDA Programming Deep Dive 的专用课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


> **范围：** Jetson Orin Nano 8GB（T234 SoC）上的生产级 CUDA 编程——工具链、kernel 编写、内存优化、性能剖析、摄像头零拷贝、TensorRT 集成与生产模式。
>
> **前置要求：** 熟悉 [Orin Nano 内存架构](../07-Orin-Nano内存架构/Guide.md)（统一内存、CMA、SMMU）与 C/C++。已安装 JetPack 6.x。

---


## 1. Jetson 上的 CUDA 与独立 GPU 对比

关键的思维模型转变：**CPU 与 GPU 之间没有 PCIe 总线**。二者共享同一块物理 LPDDR5。

| 属性 | 独立 GPU（如 A100） | Jetson Orin Nano 8GB |
|---|---|---|
| 内存 | 独立 VRAM（HBM/GDDR） | 共享 8GB LPDDR5 |
| CPU-GPU 传输 | PCIe/NVLink DMA | 无需传输——同一 DRAM |
| `cudaMemcpy` H2D/D2H | 真正的 DMA 拷贝 | DRAM 内的 memcpy（不跨总线） |
| 内存带宽 | 2 TB/s（A100 HBM） | 68 GB/s，CPU + GPU + DLA + ISP 共享 |
| `cudaMallocManaged` | 通过 PCIe 进行页迁移 | 页已在统一池中 |
| 功耗范围 | 300-700 W 系统 | 7-15 W 模块 |
| SM 数量 | 108（A100） | 8 个 SM（1024 个 CUDA 核心） |

实际影响：优先使用 `cudaMallocManaged` 或零拷贝，而非显式的 `cudaMemcpy`。带宽是瓶颈——68 GB/s 由所有引擎共享。kernel 启动开销占比更大；应积极地做融合。

---

## 2. Jetson CUDA 工具链

### 2.1 验证与编译

```bash
nvcc --version                    # JetPack 6.x ships CUDA 12.2+
nvcc -arch=sm_87 -O2 -o my_kernel my_kernel.cu   # sm_87 = Orin Nano

# Key flags:
#   -arch=sm_87          Ampere GA10B compute capability 8.7
#   --ptxas-options=-v   Show register/shared memory usage per kernel
#   -use_fast_math       Fast but less precise (use with care)
```

### 2.2 从 x86 主机交叉编译

```bash
/usr/local/cuda-12.2/bin/nvcc -arch=sm_87 \
    --compiler-bindir=/usr/bin/aarch64-linux-gnu-g++ \
    -O2 -o my_kernel my_kernel.cu
```

### 2.3 CMakeLists.txt 模板

```cmake
cmake_minimum_required(VERSION 3.18)
project(jetson_cuda LANGUAGES CXX CUDA)
set(CMAKE_CUDA_ARCHITECTURES 87)
set(CMAKE_CUDA_STANDARD 17)
add_executable(my_kernel src/my_kernel.cu)
target_link_libraries(my_kernel PRIVATE cuda cudart)
```

---

## 3. Orin Nano 上的 Ampere 架构

精简版 GA10B（Ampere 架构），计算能力 **sm_87**。

| 资源 | 数值 | 资源 | 数值 |
|---|---|---|---|
| SM | 8 | 张量核心/SM | 4（第三代） |
| CUDA 核心/SM | 128（4x32 FP32） | 最大线程数/SM | 1536 |
| CUDA 核心总数 | 1024 | warp 大小 | 32 |
| 寄存器/SM | 65536（32 位） | 共享内存/SM | 最高 164 KB |
| L2 缓存 | 512 KB | 最高时钟 | ~625 MHz |

线程层级：Grid -> Block（最多 1024 个线程）-> Warp（32 个线程）-> 线程。每个 SM 有 4 个子分区，每个子分区各有自己的 warp 调度器、32 个 FP32 ALU 和 1 个 Tensor Core。共享资源：L1/共享内存（可配置 164 KB）与寄存器文件（256 KB）。

**寄存器压力：** 65536 个 reg / 1536 个最大线程 = 每个线程 42 个 reg，超过即导致 occupancy 下降。用 `nvcc --ptxas-options=-v -arch=sm_87 kernel.cu` 检查。

---

## 4. 张量核心

固定功能 **D = A * B + C** 单元。FP16：16x8x16 分块，256 FMAs/TC/cycle。INT8：16x8x32，512 MACs。TF32：16x8x8，128 FMAs。

### 4.1 WMMA 示例（FP16 GEMM）

```cuda
#include <mma.h>
using namespace nvcuda::wmma;

__global__ void tc_gemm(half *A, half *B, float *C, int M, int N, int K) {
    fragment<matrix_a, 16, 16, 16, half, row_major> a_frag;
    fragment<matrix_b, 16, 16, 16, half, col_major> b_frag;
    fragment<accumulator, 16, 16, 16, float>         c_frag;
    fill_fragment(c_frag, 0.0f);
    int warpM = (blockIdx.x * blockDim.x + threadIdx.x) / 32 * 16;
    int warpN = blockIdx.y * 16;
    for (int k = 0; k < K; k += 16) {
        load_matrix_sync(a_frag, A + warpM*K + k, K);
        load_matrix_sync(b_frag, B + k*N + warpN, N);
        mma_sync(c_frag, a_frag, b_frag, c_frag);
    }
    store_matrix_sync(C + warpM*N + warpN, c_frag, N, mem_row_major);
}
```

何时使用：cuBLAS/TensorRT 未覆盖的稠密 FP16/INT8 矩阵运算（它们通过 `cublasGemmEx` 配合 `CUBLAS_COMPUTE_16F` 自动路由到 TC）。

---


<details>
<summary>English original</summary>

**Orin Nano 8GB -- CUDA Programming Deep Dive**

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">ON8C</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Orin Nano 8GB -- CUDA Programming Deep Dive.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Scope:** Production-level CUDA programming on Jetson Orin Nano 8GB (T234 SoC) -- toolchain, kernel authoring, memory optimization, profiling, camera zero-copy, TensorRT integration, and production patterns.
>
> **Prerequisites:** Familiarity with [Orin Nano memory architecture](../07-Orin-Nano内存架构/Guide.md) (unified memory, CMA, SMMU) and C/C++. JetPack 6.x installed.

---


**1. CUDA on Jetson vs Discrete GPU**

The critical mental-model shift: **there is no PCIe bus between CPU and GPU**. They share the same physical LPDDR5.

| Property | Discrete GPU (e.g. A100) | Jetson Orin Nano 8GB |
|---|---|---|
| Memory | Separate VRAM (HBM/GDDR) | Shared 8GB LPDDR5 |
| CPU-GPU transfer | PCIe/NVLink DMA | No transfer -- same DRAM |
| `cudaMemcpy` H2D/D2H | Real DMA copy | memcpy within DRAM (no bus crossing) |
| Memory bandwidth | 2 TB/s (A100 HBM) | 68 GB/s shared CPU + GPU + DLA + ISP |
| `cudaMallocManaged` | Page migration over PCIe | Pages already in unified pool |
| Power envelope | 300-700 W system | 7-15 W module |
| SM count | 108 (A100) | 8 SMs (1024 CUDA cores) |

Practical consequences: prefer `cudaMallocManaged` or zero-copy over explicit `cudaMemcpy`. Bandwidth is the bottleneck -- 68 GB/s is shared across all engines. Kernel launch overhead is proportionally larger; fuse aggressively.

---

**2. Jetson CUDA Toolchain**

**2.1 Verification and Compilation**

```bash
nvcc --version                    # JetPack 6.x ships CUDA 12.2+
nvcc -arch=sm_87 -O2 -o my_kernel my_kernel.cu   # sm_87 = Orin Nano

# Key flags:
#   -arch=sm_87          Ampere GA10B compute capability 8.7
#   --ptxas-options=-v   Show register/shared memory usage per kernel
#   -use_fast_math       Fast but less precise (use with care)
```

**2.2 Cross-Compilation from x86 Host**

```bash
/usr/local/cuda-12.2/bin/nvcc -arch=sm_87 \
    --compiler-bindir=/usr/bin/aarch64-linux-gnu-g++ \
    -O2 -o my_kernel my_kernel.cu
```

**2.3 CMakeLists.txt Template**

```cmake
cmake_minimum_required(VERSION 3.18)
project(jetson_cuda LANGUAGES CXX CUDA)
set(CMAKE_CUDA_ARCHITECTURES 87)
set(CMAKE_CUDA_STANDARD 17)
add_executable(my_kernel src/my_kernel.cu)
target_link_libraries(my_kernel PRIVATE cuda cudart)
```

---

**3. Ampere Architecture on Orin Nano**

Cut-down GA10B (Ampere), compute capability **sm_87**.

| Resource | Value | Resource | Value |
|---|---|---|---|
| SMs | 8 | Tensor Cores/SM | 4 (3rd-gen) |
| CUDA cores/SM | 128 (4x32 FP32) | Max threads/SM | 1536 |
| Total CUDA cores | 1024 | Warp size | 32 |
| Registers/SM | 65536 (32-bit) | Shared mem/SM | Up to 164 KB |
| L2 cache | 512 KB | Max clock | ~625 MHz |

Thread hierarchy: Grid -> Block (up to 1024 threads) -> Warp (32 threads) -> Thread. Each SM has 4 sub-partitions, each with its own warp scheduler, 32 FP32 ALUs, and 1 Tensor Core. Shared resources: L1/shared memory (164 KB configurable) and register file (256 KB).

**Register pressure:** 65536 regs / 1536 max threads = 42 regs/thread before occupancy drops. Check with `nvcc --ptxas-options=-v -arch=sm_87 kernel.cu`.

---

**4. Tensor Cores**

Fixed-function **D = A * B + C** units. FP16: 16x8x16 tile, 256 FMAs/TC/cycle. INT8: 16x8x32, 512 MACs. TF32: 16x8x8, 128 FMAs.

**4.1 WMMA Example (FP16 GEMM)**

```cuda
#include <mma.h>
using namespace nvcuda::wmma;

__global__ void tc_gemm(half *A, half *B, float *C, int M, int N, int K) {
    fragment<matrix_a, 16, 16, 16, half, row_major> a_frag;
    fragment<matrix_b, 16, 16, 16, half, col_major> b_frag;
    fragment<accumulator, 16, 16, 16, float>         c_frag;
    fill_fragment(c_frag, 0.0f);
    int warpM = (blockIdx.x * blockDim.x + threadIdx.x) / 32 * 16;
    int warpN = blockIdx.y * 16;
    for (int k = 0; k < K; k += 16) {
        load_matrix_sync(a_frag, A + warpM*K + k, K);
        load_matrix_sync(b_frag, B + k*N + warpN, N);
        mma_sync(c_frag, a_frag, b_frag, c_frag);
    }
    store_matrix_sync(C + warpM*N + warpN, c_frag, N, mem_row_major);
}
```

When to use: dense FP16/INT8 matrix ops not covered by cuBLAS/TensorRT (which route to TCs automatically via `cublasGemmEx` with `CUBLAS_COMPUTE_16F`).

---

</details>

## 5. 统一内存深入剖析

| API | 在 Jetson 上的最佳用途 |
|---|---|
| `cudaMalloc` | 仅 GPU 缓冲区、CUDA graphs |
| `cudaMallocHost` | CPU+GPU 并发访问、DMA 目标 |
| `cudaMallocManaged` | 默认 -- 页同时映射在 CPU/GPU 页表中，无拷贝 |
| `NvBufSurface` | 摄像头/视频零拷贝（DMA-BUF 支撑） |

在 Jetson 上，`cudaMallocManaged` 页永不发生物理迁移 -- 两个地址空间中是同一个 LPDDR5 页：

```cuda
float *data;
cudaMallocManaged(&data, N * sizeof(float));
for (int i = 0; i < N; i++) data[i] = i * 0.1f;  // CPU writes
kernel<<<blocks, threads>>>(data, N);              // GPU reads -- same DRAM, no copy
cudaDeviceSynchronize();
printf("result[0] = %f\n", data[0]);               // CPU reads -- no copy back
cudaFree(data);
```

---

## 6. 内存访问模式

### 6.1 合并式全局内存访问

一个 warp 访问连续的 4 字节元素只发起一次 128-byte 事务。步长访问会导致 10-30 倍减速 -- 在 68 GB/s 被共享时尤为关键。

```cuda
// GOOD: coalesced -- threads read consecutive addresses
int i = blockIdx.x * blockDim.x + threadIdx.x;
out[i] = in[i] * 2.0f;
// BAD: strided -- each thread skips 'stride' elements
int i = (blockIdx.x * blockDim.x + threadIdx.x) * stride;
```

### 6.2 共享内存 Bank 冲突

32 个 bank，每个 4 字节。warp 中两个线程访问同一个 bank（不同地址）会串行化。同一地址则免费广播。

### 6.3 L2 缓存驻留控制

Orin Nano 的 L2 为 512 KB。工作集需保持在此以内。使用 `cudaAccessPropertyPersisting` 提示：

```cuda
cudaStreamAttrValue attr;
attr.accessPolicyWindow.base_ptr = (void *)data;
attr.accessPolicyWindow.num_bytes = 256 * 1024;
attr.accessPolicyWindow.hitRatio = 1.0f;
attr.accessPolicyWindow.hitProp = cudaAccessPropertyPersisting;
attr.accessPolicyWindow.missProp = cudaAccessPropertyStreaming;
cudaStreamSetAttribute(stream, cudaStreamAttributeAccessPolicyWindow, &attr);
```

---

## 7. 在 Jetson 上编写第一个 CUDA kernel

### 7.1 向量加法（完整示例）

```cuda
// vecadd.cu -- compile: nvcc -arch=sm_87 -O2 -o vecadd vecadd.cu
#include <cstdio>
#include <cuda_runtime.h>

__global__ void vec_add(const float *a, const float *b, float *c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}
int main() {
    const int N = 1 << 20;
    float *a, *b, *c;
    cudaMallocManaged(&a, N*sizeof(float));
    cudaMallocManaged(&b, N*sizeof(float));
    cudaMallocManaged(&c, N*sizeof(float));
    for (int i = 0; i < N; i++) { a[i] = 1.0f; b[i] = 2.0f; }
    vec_add<<<(N+255)/256, 256>>>(a, b, c, N);
    cudaDeviceSynchronize();
    printf("c[0] = %f (expected 3.0)\n", c[0]);
    cudaFree(a); cudaFree(b); cudaFree(c);
}
```

### 7.2 朴素矩阵乘法

```cuda
__global__ void matmul(const float *A, const float *B, float *C, int M, int N, int K) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < M && col < N) {
        float sum = 0.0f;
        for (int k = 0; k < K; k++) sum += A[row*K + k] * B[k*N + col];
        C[row*N + col] = sum;
    }
}  // Launch: dim3 block(16,16); dim3 grid((N+15)/16, (M+15)/16);
```

---

## 8. 卷积 kernel

### 8.1 朴素二维卷积

```cuda
__global__ void conv2d_naive(const float *in, const float *kern,
                             float *out, int H, int W, int KH, int KW) {
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int hKH = KH / 2, hKW = KW / 2;
    if (row < H && col < W) {
        float sum = 0.0f;
        for (int kh = 0; kh < KH; kh++)
            for (int kw = 0; kw < KW; kw++) {
                int r = row + kh - hKH, c = col + kw - hKW;
                if (r >= 0 && r < H && c >= 0 && c < W)
                    sum += in[r * W + c] * kern[kh * KW + kw];
            }
        out[row * W + col] = sum;
    }
}
```


<details>
<summary>English original</summary>

**5. Unified Memory Deep Dive**

| API | Best Use on Jetson |
|---|---|
| `cudaMalloc` | GPU-only buffers, CUDA graphs |
| `cudaMallocHost` | CPU+GPU concurrent access, DMA targets |
| `cudaMallocManaged` | Default -- pages mapped in both CPU/GPU page tables, no copy |
| `NvBufSurface` | Camera/video zero-copy (DMA-BUF backed) |

On Jetson, `cudaMallocManaged` pages never physically migrate -- same LPDDR5 page in both address spaces:

```cuda
float *data;
cudaMallocManaged(&data, N * sizeof(float));
for (int i = 0; i < N; i++) data[i] = i * 0.1f;  // CPU writes
kernel<<<blocks, threads>>>(data, N);              // GPU reads -- same DRAM, no copy
cudaDeviceSynchronize();
printf("result[0] = %f\n", data[0]);               // CPU reads -- no copy back
cudaFree(data);
```

---

**6. Memory Access Patterns**

**6.1 Coalesced Global Memory Access**

A warp accessing consecutive 4-byte elements issues one 128-byte transaction. Strided access causes 10-30x slowdown -- critical when 68 GB/s is shared.

```cuda
// GOOD: coalesced -- threads read consecutive addresses
int i = blockIdx.x * blockDim.x + threadIdx.x;
out[i] = in[i] * 2.0f;
// BAD: strided -- each thread skips 'stride' elements
int i = (blockIdx.x * blockDim.x + threadIdx.x) * stride;
```

**6.2 Shared Memory Bank Conflicts**

32 banks, 4 bytes each. Two threads in a warp accessing the same bank (different addresses) serialize. Same address broadcasts for free.

**6.3 L2 Cache Residency Control**

Orin Nano L2 is 512 KB. Keep working sets under this. Use `cudaAccessPropertyPersisting` hints:

```cuda
cudaStreamAttrValue attr;
attr.accessPolicyWindow.base_ptr = (void *)data;
attr.accessPolicyWindow.num_bytes = 256 * 1024;
attr.accessPolicyWindow.hitRatio = 1.0f;
attr.accessPolicyWindow.hitProp = cudaAccessPropertyPersisting;
attr.accessPolicyWindow.missProp = cudaAccessPropertyStreaming;
cudaStreamSetAttribute(stream, cudaStreamAttributeAccessPolicyWindow, &attr);
```

---

**7. Writing Your First CUDA Kernel on Jetson**

**7.1 Vector Addition (Complete Example)**

```cuda
// vecadd.cu -- compile: nvcc -arch=sm_87 -O2 -o vecadd vecadd.cu
#include <cstdio>
#include <cuda_runtime.h>

__global__ void vec_add(const float *a, const float *b, float *c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}
int main() {
    const int N = 1 << 20;
    float *a, *b, *c;
    cudaMallocManaged(&a, N*sizeof(float));
    cudaMallocManaged(&b, N*sizeof(float));
    cudaMallocManaged(&c, N*sizeof(float));
    for (int i = 0; i < N; i++) { a[i] = 1.0f; b[i] = 2.0f; }
    vec_add<<<(N+255)/256, 256>>>(a, b, c, N);
    cudaDeviceSynchronize();
    printf("c[0] = %f (expected 3.0)\n", c[0]);
    cudaFree(a); cudaFree(b); cudaFree(c);
}
```

**7.2 Naive Matrix Multiply**

```cuda
__global__ void matmul(const float *A, const float *B, float *C, int M, int N, int K) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < M && col < N) {
        float sum = 0.0f;
        for (int k = 0; k < K; k++) sum += A[row*K + k] * B[k*N + col];
        C[row*N + col] = sum;
    }
}  // Launch: dim3 block(16,16); dim3 grid((N+15)/16, (M+15)/16);
```

---

**8. Convolution Kernel**

**8.1 Naive 2D Convolution**

```cuda
__global__ void conv2d_naive(const float *in, const float *kern,
                             float *out, int H, int W, int KH, int KW) {
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int hKH = KH / 2, hKW = KW / 2;
    if (row < H && col < W) {
        float sum = 0.0f;
        for (int kh = 0; kh < KH; kh++)
            for (int kw = 0; kw < KW; kw++) {
                int r = row + kh - hKH, c = col + kw - hKW;
                if (r >= 0 && r < H && c >= 0 && c < W)
                    sum += in[r * W + c] * kern[kh * KW + kw];
            }
        out[row * W + col] = sum;
    }
}
```

</details>

### 8.2 使用共享内存分块（3x3）

```cuda
#define TILE 16
#define RAD  1

__global__ void conv2d_tiled(const float *in, const float *kern, float *out, int H, int W) {
    __shared__ float tile[TILE + 2*RAD][TILE + 2*RAD];
    int tx = threadIdx.x, ty = threadIdx.y;
    int col = blockIdx.x * TILE + tx, row = blockIdx.y * TILE + ty;
    int sc = tx + RAD, sr = ty + RAD;

    tile[sr][sc] = (row < H && col < W) ? in[row * W + col] : 0.0f;
    // Load halo edges (left, right, top, bottom -- boundary threads only)
    if (tx < RAD)
        tile[sr][tx] = (row < H && col-RAD >= 0) ? in[row*W + col-RAD] : 0.0f;
    if (tx >= TILE - RAD)
        tile[sr][sc+RAD] = (row < H && col+RAD < W) ? in[row*W + col+RAD] : 0.0f;
    // (top/bottom halo similar)
    __syncthreads();

    if (row < H && col < W) {
        float sum = 0.0f;
        for (int kr = -RAD; kr <= RAD; kr++)
            for (int kc = -RAD; kc <= RAD; kc++)
                sum += tile[sr+kr][sc+kc] * kern[(kr+RAD)*3 + (kc+RAD)];
        out[row * W + col] = sum;
    }
}
```

分块版本把全局读取量从 H\*W\*K\*K 降到约 H\*W\*(1 + halo) —— 对 3x3-7x7 有 5-9 倍提升。

---

## 9. CUDA 流与并发

流支持 kernel 并发执行。在 Orin Nano（8 个 SM）上，只有当单个 kernel 较小时，并发才有收益。

```cuda
cudaStream_t s1, s2;
cudaStreamCreate(&s1); cudaStreamCreate(&s2);
kernelA<<<grid, block, 0, s1>>>(dataA);  // may run concurrently
kernelB<<<grid, block, 0, s2>>>(dataB);
cudaStreamSynchronize(s1); cudaStreamSynchronize(s2);
cudaStreamDestroy(s1); cudaStreamDestroy(s2);
```

### 9.1 多流流水线

在 GPU 上处理第 N 帧，同时 CPU 准备第 N+1 帧。用 2 个流做双缓冲：

```cuda
cudaStream_t st[2];
for (int i = 0; i < 2; i++) cudaStreamCreate(&st[i]);
for (int f = 0; f < total; f++) {
    int s = f % 2;
    preprocess<<<g, b, 0, st[s]>>>(in[s], pre[s]);
    inference<<<g, b, 0, st[s]>>>(pre[s], res[s]);
    postprocess<<<g, b, 0, st[s]>>>(res[s], out[s]);
}
for (int i = 0; i < 2; i++) { cudaStreamSynchronize(st[i]); cudaStreamDestroy(st[i]); }
```

---

## 10. 同步原语

### 10.1 Event（时序）

```cuda
cudaEvent_t start, stop;
cudaEventCreate(&start); cudaEventCreate(&stop);
cudaEventRecord(start, stream);
my_kernel<<<grid, block, 0, stream>>>(data);
cudaEventRecord(stop, stream);
cudaEventSynchronize(stop);
float ms; cudaEventElapsedTime(&ms, start, stop);
```

### 10.2 流间依赖

```cuda
cudaEventRecord(ev, streamA);          // record after kernelA
cudaStreamWaitEvent(streamB, ev, 0);   // streamB blocks until ev fires
kernelB<<<g, b, 0, streamB>>>(dataB);  // guaranteed after kernelA
```

### 10.3 同步 API

`cudaDeviceSynchronize()` 阻塞所有流。`cudaStreamSynchronize(s)` 只阻塞一个流。`cudaEventQuery(e)` / `cudaStreamQuery(s)` 轮询而不阻塞。

---

## 11. CUDA + 摄像头零拷贝

摄像头帧的流向是 ISP -> NvBufSurface（DMA-BUF）。直接导入 CUDA —— 无需拷贝。

```
Sensor -> VI -> ISP -> NvBufSurface (DMA-BUF fd)
                            |
                 EGLImageKHR (EGL interop)
                            |
                 cudaGraphicsResource (cuGraphicsEGLRegisterImage)
                            |
                 cudaArray / cudaSurfaceObject -> CUDA Kernel
```

```cuda
#include <cudaEGL.h>
void process_frame(EGLImageKHR egl_image) {
    cudaGraphicsResource_t res;
    cudaGraphicsEGLRegisterImage(&res, egl_image, cudaGraphicsRegisterFlagsReadOnly);
    cudaArray_t arr;
    cudaGraphicsSubResourceGetMappedArray(&arr, res, 0, 0);
    cudaResourceDesc rd = {}; rd.resType = cudaResourceTypeArray; rd.res.array.array = arr;
    cudaSurfaceObject_t surf; cudaCreateSurfaceObject(&surf, &rd);
    process_kernel<<<grid, block>>>(surf, width, height);
    cudaDeviceSynchronize();
    cudaDestroySurfaceObject(surf);
    cudaGraphicsUnregisterResource(res);
}
```

更高层接口：先 `NvBufSurfaceMapEglImage(surface, 0)` 再 `surface->surfaceList[0].mappedAddr.eglImage`。真正的零拷贝 —— ISP 写入的 LPDDR5 页由 CUDA kernel 直接读取。

---

## 12. CUDA + TensorRT 集成

### 12.1 带 CUDA kernel 的自定义 Plugin

```cpp
class NormalizePlugin : public nvinfer1::IPluginV2DynamicExt {
    int enqueue(const nvinfer1::PluginTensorDesc *inDesc, /*...*/) override {
        int n = inDesc[0].dims.d[0] * inDesc[0].dims.d[1] * inDesc[0].dims.d[2] * inDesc[0].dims.d[3];
        normalize_kernel<<<(n+255)/256, 256, 0, stream>>>(
            (const half*)inputs[0], (half*)outputs[0], n, mean_, std_);
        return 0;
    }
};
```


<details>
<summary>English original</summary>

**8.2 Tiled with Shared Memory (3x3)**

```cuda
#define TILE 16
#define RAD  1

__global__ void conv2d_tiled(const float *in, const float *kern, float *out, int H, int W) {
    __shared__ float tile[TILE + 2*RAD][TILE + 2*RAD];
    int tx = threadIdx.x, ty = threadIdx.y;
    int col = blockIdx.x * TILE + tx, row = blockIdx.y * TILE + ty;
    int sc = tx + RAD, sr = ty + RAD;

    tile[sr][sc] = (row < H && col < W) ? in[row * W + col] : 0.0f;
    // Load halo edges (left, right, top, bottom -- boundary threads only)
    if (tx < RAD)
        tile[sr][tx] = (row < H && col-RAD >= 0) ? in[row*W + col-RAD] : 0.0f;
    if (tx >= TILE - RAD)
        tile[sr][sc+RAD] = (row < H && col+RAD < W) ? in[row*W + col+RAD] : 0.0f;
    // (top/bottom halo similar)
    __syncthreads();

    if (row < H && col < W) {
        float sum = 0.0f;
        for (int kr = -RAD; kr <= RAD; kr++)
            for (int kc = -RAD; kc <= RAD; kc++)
                sum += tile[sr+kr][sc+kc] * kern[(kr+RAD)*3 + (kc+RAD)];
        out[row * W + col] = sum;
    }
}
```

Tiled version reduces global reads from H\*W\*K\*K to ~H\*W\*(1 + halo) -- 5-9x improvement for 3x3-7x7.

---

**9. CUDA Streams and Concurrency**

Streams enable concurrent kernel execution. On Orin Nano (8 SMs), concurrency helps when individual kernels are small.

```cuda
cudaStream_t s1, s2;
cudaStreamCreate(&s1); cudaStreamCreate(&s2);
kernelA<<<grid, block, 0, s1>>>(dataA);  // may run concurrently
kernelB<<<grid, block, 0, s2>>>(dataB);
cudaStreamSynchronize(s1); cudaStreamSynchronize(s2);
cudaStreamDestroy(s1); cudaStreamDestroy(s2);
```

**9.1 Multi-Stream Pipeline**

Process frame N on GPU while CPU prepares N+1. Double-buffer with 2 streams:

```cuda
cudaStream_t st[2];
for (int i = 0; i < 2; i++) cudaStreamCreate(&st[i]);
for (int f = 0; f < total; f++) {
    int s = f % 2;
    preprocess<<<g, b, 0, st[s]>>>(in[s], pre[s]);
    inference<<<g, b, 0, st[s]>>>(pre[s], res[s]);
    postprocess<<<g, b, 0, st[s]>>>(res[s], out[s]);
}
for (int i = 0; i < 2; i++) { cudaStreamSynchronize(st[i]); cudaStreamDestroy(st[i]); }
```

---

**10. Synchronization Primitives**

**10.1 Events (Timing)**

```cuda
cudaEvent_t start, stop;
cudaEventCreate(&start); cudaEventCreate(&stop);
cudaEventRecord(start, stream);
my_kernel<<<grid, block, 0, stream>>>(data);
cudaEventRecord(stop, stream);
cudaEventSynchronize(stop);
float ms; cudaEventElapsedTime(&ms, start, stop);
```

**10.2 Inter-Stream Dependencies**

```cuda
cudaEventRecord(ev, streamA);          // record after kernelA
cudaStreamWaitEvent(streamB, ev, 0);   // streamB blocks until ev fires
kernelB<<<g, b, 0, streamB>>>(dataB);  // guaranteed after kernelA
```

**10.3 Sync API**

`cudaDeviceSynchronize()` blocks on all streams. `cudaStreamSynchronize(s)` blocks on one. `cudaEventQuery(e)` / `cudaStreamQuery(s)` poll without blocking.

---

**11. CUDA + Camera Zero-Copy**

Camera frames flow ISP -> NvBufSurface (DMA-BUF). Import directly into CUDA -- no copy.

```
Sensor -> VI -> ISP -> NvBufSurface (DMA-BUF fd)
                            |
                 EGLImageKHR (EGL interop)
                            |
                 cudaGraphicsResource (cuGraphicsEGLRegisterImage)
                            |
                 cudaArray / cudaSurfaceObject -> CUDA Kernel
```

```cuda
#include <cudaEGL.h>
void process_frame(EGLImageKHR egl_image) {
    cudaGraphicsResource_t res;
    cudaGraphicsEGLRegisterImage(&res, egl_image, cudaGraphicsRegisterFlagsReadOnly);
    cudaArray_t arr;
    cudaGraphicsSubResourceGetMappedArray(&arr, res, 0, 0);
    cudaResourceDesc rd = {}; rd.resType = cudaResourceTypeArray; rd.res.array.array = arr;
    cudaSurfaceObject_t surf; cudaCreateSurfaceObject(&surf, &rd);
    process_kernel<<<grid, block>>>(surf, width, height);
    cudaDeviceSynchronize();
    cudaDestroySurfaceObject(surf);
    cudaGraphicsUnregisterResource(res);
}
```

Higher-level: `NvBufSurfaceMapEglImage(surface, 0)` then `surface->surfaceList[0].mappedAddr.eglImage`. True zero-copy -- ISP-written LPDDR5 pages read directly by CUDA kernel.

---

**12. CUDA + TensorRT Integration**

**12.1 Custom Plugin with CUDA Kernel**

```cpp
class NormalizePlugin : public nvinfer1::IPluginV2DynamicExt {
    int enqueue(const nvinfer1::PluginTensorDesc *inDesc, /*...*/) override {
        int n = inDesc[0].dims.d[0] * inDesc[0].dims.d[1] * inDesc[0].dims.d[2] * inDesc[0].dims.d[3];
        normalize_kernel<<<(n+255)/256, 256, 0, stream>>>(
            (const half*)inputs[0], (half*)outputs[0], n, mean_, std_);
        return 0;
    }
};
```

</details>

### 12.2 融合预处理 kernel

在单个 kernel 内完成 resize + normalize + HWC-to-CHW，可避免 3 次独立 pass：

```cuda
__global__ void preprocess_fused(const uint8_t *src, float *dst,
                                  int srcH, int srcW, int dstH, int dstW, int C) {
    int dx = blockIdx.x * blockDim.x + threadIdx.x;
    int dy = blockIdx.y * blockDim.y + threadIdx.y;
    if (dx >= dstW || dy >= dstH) return;
    // bilinear sample -> normalize -> write CHW
    for (int c = 0; c < C; c++)
        dst[c*dstH*dstW + dy*dstW + dx] = (pixel[c]/255.0f - mean[c]) / std_dev[c];
}
```

完整流水线：摄像头（零拷贝）-> CUDA 预处理（融合）-> TensorRT `engine.execute` -> CUDA 后处理（NMS/decode）-> 输出。

---

## 13. Nsight Systems 性能剖析

```bash
nsys profile -o trace --trace=cuda,nvtx,osrt --gpu-metrics-device=0 ./my_app
# Remote: nsys profile --target=ssh://user@jetson_ip --trace=cuda,nvtx -o trace /path/to/app
```

把 `.nsys-rep` 传输到主机以便 GUI 分析。重点关注：kernel 之间的空隙（CPU 瓶颈）、缺失的流重叠、不必要的 `cudaMemcpy`。用 NVTX 标注：

```cuda
#include <nvtx3/nvToolsExt.h>
nvtxRangePush("Preprocessing");
preprocess<<<g, b, 0, stream>>>(data);
nvtxRangePop();
```

---

## 14. Nsight Compute

```bash
ncu --set full --kernel-name "my_kernel" --launch-skip 2 --launch-count 3 ./my_app
```

| 指标 | 目标 | 危险信号 |
|---|---|---|
| 实测 Occupancy | >50% | <25% -- register/smem 压力 |
| 内存吞吐 | 接近 68 GB/s | 远低于 -- 检查 coalescing |
| SM 吞吐 | >60% | <30% -- kernel 过小或带宽受限 |

以编程方式寻找最优 block size：`cudaOccupancyMaxPotentialBlockSize(&minGrid, &blockSz, my_kernel, 0, 0);`

---

## 15. 常用优化技术

**Occupancy 调优：** 用 `__launch_bounds__(256, 4)` 提示最大线程数与每 SM 最小 block 数。可减少寄存器分配，增加并发 warp 数。

**Warp 分歧：** 避免在一个 warp 内分支（`threadIdx.x % 2`）。重构代码，使分歧对齐到 warp 边界（32 的倍数）。

**ILP（指令级并行）：** 让每个线程处理多个元素，使相互独立的操作可以流水线化：

```cuda
__global__ void ilp4(float *out, const float *in, int N) {
    int base = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    float a=in[base], b=in[base+1], c=in[base+2], d=in[base+3];
    out[base]=a*a; out[base+1]=b*b; out[base+2]=c*c; out[base+3]=d*d;
}
```

**Kernel 融合：** 在 Jetson 上，启动开销（约 5-10 us）所占比例很高。融合顺序执行的 kernel：

```cuda
// BEFORE: normalize<<<g,b>>>(d); activate<<<g,b>>>(d); scale<<<g,b>>>(d);
// AFTER:
__global__ void fused(float *data, float mean, float std, float s) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    data[i] = fmaxf((data[i] - mean) / std, 0.0f) * s;
}
```

---

## 16. 功耗感知的 CUDA 编程

功耗模式：**15W**（GPU 约 625 MHz）、**7W**（GPU 约 306 MHz）。两种模式都保持全部 8 个 SM 工作。

```bash
sudo nvpmodel -m 0 && sudo jetson_clocks     # max performance
tegrastats --interval 1000                    # monitor power
# Lock GPU clock for latency-sensitive work:
sudo sh -c 'echo 625500000 > /sys/devices/17000000.gpu/devfreq/17000000.gpu/min_freq'
```

最大化 TOPS/watt：使用 INT8/FP16（Tensor Core 的最佳区间），最小化 GPU 空闲时间，合并小 kernel，把 conv layer 卸载到 DLA（深度学习加速器）（TOPS/watt 提升 2-5 倍 -- 见 [DLA Deep Dive](../04-Orin-Nano-DLA深入解析/Guide.md)）。GPU governor（`nvhost_podgov`）会随负载调节时钟；突发型工作负载在 ramp-up 期间可能以较低时钟运行。

---

## 17. 在 Jetson 上调试 CUDA

### 17.1 compute-sanitizer

```bash
compute-sanitizer --tool memcheck  ./my_app   # OOB/misaligned
compute-sanitizer --tool racecheck ./my_app   # race conditions (slow)
compute-sanitizer --tool initcheck ./my_app   # uninitialized reads
```

### 17.2 常见陷阱

| 陷阱 | 现象 | 修复 |
|---|---|---|
| 缺少 `cudaDeviceSynchronize()` | CPU 读到过期的 GPU 结果 | 在 CPU 读取前同步 |
| 启动静默失败 | 结果错误，无报错 | 每次启动后 `cudaGetLastError()` |
| 共享内存溢出 | `cudaErrorLaunchFailure` | 减小 smem 或 block size |
| 8GB 共享池 OOM | `cudaErrorMemoryAllocation` | 用 `tegrastats` 监控；减小批大小 |
| 架构不匹配（在 sm_87 上用 sm_50） | `cudaErrorInvalidDeviceFunction` | 用 `-arch=sm_87` 编译 |


<details>
<summary>English original</summary>

**12.2 Fused Pre-Processing Kernel**

Resize + normalize + HWC-to-CHW in a single kernel avoids 3 separate passes:

```cuda
__global__ void preprocess_fused(const uint8_t *src, float *dst,
                                  int srcH, int srcW, int dstH, int dstW, int C) {
    int dx = blockIdx.x * blockDim.x + threadIdx.x;
    int dy = blockIdx.y * blockDim.y + threadIdx.y;
    if (dx >= dstW || dy >= dstH) return;
    // bilinear sample -> normalize -> write CHW
    for (int c = 0; c < C; c++)
        dst[c*dstH*dstW + dy*dstW + dx] = (pixel[c]/255.0f - mean[c]) / std_dev[c];
}
```

Full pipeline: Camera (zero-copy) -> CUDA preprocess (fused) -> TensorRT `engine.execute` -> CUDA postprocess (NMS/decode) -> Output.

---

**13. Nsight Systems Profiling**

```bash
nsys profile -o trace --trace=cuda,nvtx,osrt --gpu-metrics-device=0 ./my_app
# Remote: nsys profile --target=ssh://user@jetson_ip --trace=cuda,nvtx -o trace /path/to/app
```

Transfer `.nsys-rep` to host for GUI analysis. Look for: gaps between kernels (CPU bottleneck), missing stream overlap, unnecessary `cudaMemcpy`. Annotate with NVTX:

```cuda
#include <nvtx3/nvToolsExt.h>
nvtxRangePush("Preprocessing");
preprocess<<<g, b, 0, stream>>>(data);
nvtxRangePop();
```

---

**14. Nsight Compute**

```bash
ncu --set full --kernel-name "my_kernel" --launch-skip 2 --launch-count 3 ./my_app
```

| Metric | Target | Red Flag |
|---|---|---|
| Achieved Occupancy | >50% | <25% -- register/smem pressure |
| Memory Throughput | Near 68 GB/s | Well below -- check coalescing |
| SM Throughput | >60% | <30% -- kernel too small or memory-bound |

Find optimal block size programmatically: `cudaOccupancyMaxPotentialBlockSize(&minGrid, &blockSz, my_kernel, 0, 0);`

---

**15. Common Optimization Techniques**

**Occupancy tuning:** Use `__launch_bounds__(256, 4)` to hint max threads and min blocks/SM. Reduces register allocation, increases concurrent warps.

**Warp divergence:** Avoid branching within a warp (`threadIdx.x % 2`). Restructure so divergence aligns to warp boundaries (multiples of 32).

**ILP (Instruction-Level Parallelism):** Each thread processes multiple elements so independent operations pipeline:

```cuda
__global__ void ilp4(float *out, const float *in, int N) {
    int base = (blockIdx.x * blockDim.x + threadIdx.x) * 4;
    float a=in[base], b=in[base+1], c=in[base+2], d=in[base+3];
    out[base]=a*a; out[base+1]=b*b; out[base+2]=c*c; out[base+3]=d*d;
}
```

**Kernel fusion:** Launch overhead (~5-10 us) is proportionally expensive on Jetson. Fuse sequential kernels:

```cuda
// BEFORE: normalize<<<g,b>>>(d); activate<<<g,b>>>(d); scale<<<g,b>>>(d);
// AFTER:
__global__ void fused(float *data, float mean, float std, float s) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    data[i] = fmaxf((data[i] - mean) / std, 0.0f) * s;
}
```

---

**16. Power-Aware CUDA Programming**

Power modes: **15W** (~625 MHz GPU), **7W** (~306 MHz GPU). Both keep all 8 SMs active.

```bash
sudo nvpmodel -m 0 && sudo jetson_clocks     # max performance
tegrastats --interval 1000                    # monitor power
# Lock GPU clock for latency-sensitive work:
sudo sh -c 'echo 625500000 > /sys/devices/17000000.gpu/devfreq/17000000.gpu/min_freq'
```

Maximizing TOPS/watt: use INT8/FP16 (Tensor Core sweet spot), minimize idle GPU time, batch small kernels, offload conv layers to DLA (2-5x better TOPS/watt -- see [DLA Deep Dive](../04-Orin-Nano-DLA深入解析/Guide.md)). The GPU governor (`nvhost_podgov`) scales clocks on load; bursty workloads may run at lower clocks during ramp-up.

---

**17. Debugging CUDA on Jetson**

**17.1 compute-sanitizer**

```bash
compute-sanitizer --tool memcheck  ./my_app   # OOB/misaligned
compute-sanitizer --tool racecheck ./my_app   # race conditions (slow)
compute-sanitizer --tool initcheck ./my_app   # uninitialized reads
```

**17.2 Common Pitfalls**

| Pitfall | Symptom | Fix |
|---|---|---|
| Missing `cudaDeviceSynchronize()` | Stale GPU results on CPU | Sync before CPU reads |
| Silent launch failure | Wrong results, no error | `cudaGetLastError()` after every launch |
| Shared memory overflow | `cudaErrorLaunchFailure` | Reduce smem or block size |
| OOM on 8GB shared pool | `cudaErrorMemoryAllocation` | Monitor with `tegrastats`; reduce batch |
| Wrong arch (sm_50 on sm_87) | `cudaErrorInvalidDeviceFunction` | Compile with `-arch=sm_87` |

</details>

### 17.3 错误检查宏

```cuda
#define CUDA_CHECK(call) do {                                       \
    cudaError_t err = call;                                         \
    if (err != cudaSuccess) {                                       \
        fprintf(stderr, "CUDA error %s:%d: %s\n",                  \
                __FILE__, __LINE__, cudaGetErrorString(err));        \
        exit(EXIT_FAILURE); }                                       \
} while (0)

// Usage: check alloc, launch, and sync
CUDA_CHECK(cudaMallocManaged(&data, N * sizeof(float)));
my_kernel<<<grid, block>>>(data);
CUDA_CHECK(cudaGetLastError());          // launch config errors
CUDA_CHECK(cudaDeviceSynchronize());     // execution errors
```

---

## 18. 生产级 CUDA 模式

### 18.1 RAII 资源管理

```cpp
struct CudaBuffer {
    void *ptr = nullptr; size_t size = 0;
    CudaBuffer(size_t bytes) : size(bytes) { CUDA_CHECK(cudaMallocManaged(&ptr, bytes)); }
    ~CudaBuffer() { if (ptr) cudaFree(ptr); }
    CudaBuffer(const CudaBuffer&) = delete;
    CudaBuffer& operator=(const CudaBuffer&) = delete;
    CudaBuffer(CudaBuffer&& o) noexcept : ptr(o.ptr), size(o.size) { o.ptr = nullptr; }
};
```

### 18.2 带错误传播的异步流水线

```cpp
class CudaPipeline {
    cudaStream_t stream_; cudaEvent_t done_;
public:
    CudaPipeline() {
        CUDA_CHECK(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking));
        CUDA_CHECK(cudaEventCreateWithFlags(&done_, cudaEventDisableTiming));
    }
    ~CudaPipeline() { cudaStreamDestroy(stream_); cudaEventDestroy(done_); }
    void submit(const float *in, float *out, int N) {
        my_kernel<<<(N+255)/256, 256, 0, stream_>>>(in, out, N);
        CUDA_CHECK(cudaGetLastError());
        cudaEventRecord(done_, stream_);
    }
    bool is_complete() { return cudaEventQuery(done_) == cudaSuccess; }
    void wait() { CUDA_CHECK(cudaEventSynchronize(done_)); }
};
```

### 18.3 Python（Jetson 上的 CuPy）

```python
import cupy as cp
a = cp.random.randn(1024, 1024, dtype=cp.float32)
c = cp.matmul(a, a)  # runs on Orin Nano GPU

# Custom kernel via RawKernel
kern = cp.RawKernel(r'''
extern "C" __global__ void relu(float *d, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) d[i] = fmaxf(d[i], 0.0f);
}''', 'relu')
data = cp.random.randn(1 << 20, dtype=cp.float32)
kern(((data.size + 255) // 256,), (256,), (data, data.size))
```

---

## 19. 参考资料

### 内部指南

* [Orin Nano Platform Guide](../Guide.md) —— boot chain、JetPack、ROS2、推理优化
* [Orin Nano Memory Architecture](../07-Orin-Nano内存架构/Guide.md) —— SMMU、CMA、零拷贝、GPU 内存
* [Orin Nano DLA Deep Dive](../04-Orin-Nano-DLA深入解析/Guide.md) —— DLA 与 GPU 对比、TensorRT DLA、多引擎调度

### NVIDIA 文档

* [CUDA C++ Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)
* [CUDA C++ Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)
* [Jetson Orin Nano Developer Kit](https://developer.nvidia.com/embedded/jetson-orin-nano-developer-kit)
* [Nsight Systems User Guide](https://docs.nvidia.com/nsight-systems/)
* [Nsight Compute User Guide](https://docs.nvidia.com/nsight-compute/)
* [TensorRT Developer Guide](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/)
* [CUDA WMMA API](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#wmma)
* [Jetson Linux Multimedia API](https://docs.nvidia.com/jetson/l4t-multimedia/)


<details>
<summary>English original</summary>

**17.3 Error-Checking Macro**

```cuda
#define CUDA_CHECK(call) do {                                       \
    cudaError_t err = call;                                         \
    if (err != cudaSuccess) {                                       \
        fprintf(stderr, "CUDA error %s:%d: %s\n",                  \
                __FILE__, __LINE__, cudaGetErrorString(err));        \
        exit(EXIT_FAILURE); }                                       \
} while (0)

// Usage: check alloc, launch, and sync
CUDA_CHECK(cudaMallocManaged(&data, N * sizeof(float)));
my_kernel<<<grid, block>>>(data);
CUDA_CHECK(cudaGetLastError());          // launch config errors
CUDA_CHECK(cudaDeviceSynchronize());     // execution errors
```

---

**18. Production CUDA Patterns**

**18.1 RAII Resource Management**

```cpp
struct CudaBuffer {
    void *ptr = nullptr; size_t size = 0;
    CudaBuffer(size_t bytes) : size(bytes) { CUDA_CHECK(cudaMallocManaged(&ptr, bytes)); }
    ~CudaBuffer() { if (ptr) cudaFree(ptr); }
    CudaBuffer(const CudaBuffer&) = delete;
    CudaBuffer& operator=(const CudaBuffer&) = delete;
    CudaBuffer(CudaBuffer&& o) noexcept : ptr(o.ptr), size(o.size) { o.ptr = nullptr; }
};
```

**18.2 Async Pipeline with Error Propagation**

```cpp
class CudaPipeline {
    cudaStream_t stream_; cudaEvent_t done_;
public:
    CudaPipeline() {
        CUDA_CHECK(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking));
        CUDA_CHECK(cudaEventCreateWithFlags(&done_, cudaEventDisableTiming));
    }
    ~CudaPipeline() { cudaStreamDestroy(stream_); cudaEventDestroy(done_); }
    void submit(const float *in, float *out, int N) {
        my_kernel<<<(N+255)/256, 256, 0, stream_>>>(in, out, N);
        CUDA_CHECK(cudaGetLastError());
        cudaEventRecord(done_, stream_);
    }
    bool is_complete() { return cudaEventQuery(done_) == cudaSuccess; }
    void wait() { CUDA_CHECK(cudaEventSynchronize(done_)); }
};
```

**18.3 Python (CuPy on Jetson)**

```python
import cupy as cp
a = cp.random.randn(1024, 1024, dtype=cp.float32)
c = cp.matmul(a, a)  # runs on Orin Nano GPU

# Custom kernel via RawKernel
kern = cp.RawKernel(r'''
extern "C" __global__ void relu(float *d, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) d[i] = fmaxf(d[i], 0.0f);
}''', 'relu')
data = cp.random.randn(1 << 20, dtype=cp.float32)
kern(((data.size + 255) // 256,), (256,), (data, data.size))
```

---

**19. References**

**Internal Guides**

* [Orin Nano Platform Guide](../Guide.md) -- boot chain, JetPack, ROS2, inference optimization
* [Orin Nano Memory Architecture](../07-Orin-Nano内存架构/Guide.md) -- SMMU, CMA, zero-copy, GPU memory
* [Orin Nano DLA Deep Dive](../04-Orin-Nano-DLA深入解析/Guide.md) -- DLA vs GPU, TensorRT DLA, multi-engine scheduling

**NVIDIA Documentation**

* [CUDA C++ Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)
* [CUDA C++ Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)
* [Jetson Orin Nano Developer Kit](https://developer.nvidia.com/embedded/jetson-orin-nano-developer-kit)
* [Nsight Systems User Guide](https://docs.nvidia.com/nsight-systems/)
* [Nsight Compute User Guide](https://docs.nvidia.com/nsight-compute/)
* [TensorRT Developer Guide](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/)
* [CUDA WMMA API](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#wmma)
* [Jetson Linux Multimedia API](https://docs.nvidia.com/jetson/l4t-multimedia/)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/1. Nvidia Jetson Platform/Orin-Nano-CUDA-Programming/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/1.%20Nvidia%20Jetson%20Platform/Orin-Nano-CUDA-Programming/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
