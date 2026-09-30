---
title: 构建系统
description: 构建系统
---

# 构建系统

## 环境要求

| 组件 | 最低版本 | 推荐版本 |
|-----------|---------|-------------|
| 平台 | Jetson Orin (aarch64) | Orin Nano Super 8GB |
| JetPack | 5.1+ | 6.1 (R36.4) |
| CUDA | 11.4+ | 12.6 |
| CMake | 3.20+ | 3.24+ |
| GCC | 11+ | 11.4 |
| nvcc | 与 CUDA 版本匹配 | 12.6.68 |

**无法在 x86 上交叉编译。** CMakeLists.txt 会以 fatal error 强制执行 `aarch64`。

## 快速构建

```bash
cmake -B build -DCMAKE_CUDA_ARCHITECTURES="87" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## 构建目标

| 目标 | 二进制文件 | 说明 |
|--------|--------|-------------|
| `jetson-llm` | `build/jetson-llm` | CLI 推理 |
| `jetson-llm-server` | `build/jetson-llm-server` | HTTP API 服务器 |
| `test_memory` | `build/test_memory` | 内存子系统测试 |
| `test_kernels` | `build/test_kernels` | CUDA kernel 测试 |
| `test_model_load` | `build/test_model_load` | GGUF 加载测试 |

## CMake 配置

### 关键变量

| 变量 | 取值 | 原因 |
|----------|-------|-----|
| `CMAKE_CUDA_ARCHITECTURES` | `87` | SM 8.7 = Orin Nano/NX/AGX |
| `CMAKE_BUILD_TYPE` | `Release` | -O3 优化 |
| `CMAKE_CXX_STANDARD` | `17` | structured bindings、constexpr if 所必需 |
| `CMAKE_CUDA_STANDARD` | `17` | 与 C++ 标准匹配 |

### 编译选项

```
CXX:  -O3 -march=armv8.2-a+fp16 -ffast-math -Wno-format-truncation -Wno-unused-result
CUDA: -O3 --use_fast_math --ptxas-options=-v --diag-suppress=177
```

- `-march=armv8.2-a+fp16` — 在 ARM 上启用 FP16 NEON intrinsics
- `--use_fast_math` — GPU 数学运算快但精度较低（推理场景可接受）
- `--ptxas-options=-v` — 显示每个 kernel 的寄存器/共享内存用量
- `-Wno-format-truncation` — 抑制 snprintf 截断警告
- `-Wno-unused-result` — 抑制 fscanf 返回值警告

### 依赖

由 CMake 自动查找：
- `CUDAToolkit` — 提供 `CUDA::cudart`、`CUDA::cublas`、头文件包含路径
- `Threads` — pthreads

无需外部库。HTTP、JSON 与 GGUF 解析均为内置。

## 库架构

```
libjetson_llm_core.a (static library)
  ├── src/memory/    (budget, kv_cache, pool)     ← .cpp → g++
  ├── src/jetson/    (power, thermal, sysinfo)    ← .cpp → g++
  ├── src/kernels/   (6 CUDA kernels)             ← .cu  → nvcc
  └── src/engine/    (model, decode, sample, tok)  ← .cpp/.cu → g++/nvcc

jetson-llm          → links libjetson_llm_core.a
jetson-llm-server   → links libjetson_llm_core.a + http_server.cpp
test_*              → links libjetson_llm_core.a
```

## 文件类型

| 扩展名 | 编译器 | 原因 |
|-----------|----------|-----|
| `.cpp` | g++ | 不含 CUDA kernel，不含 `__global__`，不做 `half` 运算 |
| `.cu` | nvcc | 包含 `__global__` kernel，使用 `__half2float` 与 CUDA 数学运算 |

`decode.cu` 属于 `.cu`，因为它定义了 `vec_add_kernel` 和 `fp16_to_fp32_kernel`。
其余所有 engine 文件均为 `.cpp`（它们调用 kernel 函数，但不定义这些函数）。
CUDA 头文件（`cuda_runtime.h`、`cuda_fp16.h`）通过 `CUDAToolkit_INCLUDE_DIRS` 对 `.cpp` 可见。

## 干净重建

```bash
rm -rf build
cmake -B build -DCMAKE_CUDA_ARCHITECTURES="87" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## 故障排查

| 报错 | 解决方法 |
|-------|-----|
| `fatal error: cuda_runtime.h` | 安装 CUDA toolkit：`sudo apt install cuda-toolkit-12-6` |
| `aarch64 ONLY` error | 在 Jetson 上构建，而非 x86 |
| `no kernel image for sm_87` | 确保 `-DCMAKE_CUDA_ARCHITECTURES="87"` |
| `nvcc not found` | 加入 PATH：`export PATH=/usr/local/cuda/bin:$PATH` |
| 链接错误 | 干净构建：`rm -rf build` 并重新配置 |


<details>
<summary>English original</summary>

**Build System**

**Requirements**

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| Platform | Jetson Orin (aarch64) | Orin Nano Super 8GB |
| JetPack | 5.1+ | 6.1 (R36.4) |
| CUDA | 11.4+ | 12.6 |
| CMake | 3.20+ | 3.24+ |
| GCC | 11+ | 11.4 |
| nvcc | matches CUDA | 12.6.68 |

**Cannot cross-compile on x86.** CMakeLists.txt enforces `aarch64` with a fatal error.

**Quick Build**

```bash
cmake -B build -DCMAKE_CUDA_ARCHITECTURES="87" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Build Targets**

| Target | Binary | Description |
|--------|--------|-------------|
| `jetson-llm` | `build/jetson-llm` | CLI inference |
| `jetson-llm-server` | `build/jetson-llm-server` | HTTP API server |
| `test_memory` | `build/test_memory` | Memory subsystem tests |
| `test_kernels` | `build/test_kernels` | CUDA kernel tests |
| `test_model_load` | `build/test_model_load` | GGUF loading tests |

**CMake Configuration**

**Key Variables**

| Variable | Value | Why |
|----------|-------|-----|
| `CMAKE_CUDA_ARCHITECTURES` | `87` | SM 8.7 = Orin Nano/NX/AGX |
| `CMAKE_BUILD_TYPE` | `Release` | -O3 optimizations |
| `CMAKE_CXX_STANDARD` | `17` | Required for structured bindings, constexpr if |
| `CMAKE_CUDA_STANDARD` | `17` | Match C++ standard |

**Compiler Flags**

```
CXX:  -O3 -march=armv8.2-a+fp16 -ffast-math -Wno-format-truncation -Wno-unused-result
CUDA: -O3 --use_fast_math --ptxas-options=-v --diag-suppress=177
```

- `-march=armv8.2-a+fp16` — enables FP16 NEON intrinsics on ARM
- `--use_fast_math` — fast but less precise GPU math (acceptable for inference)
- `--ptxas-options=-v` — shows register/shared memory usage per kernel
- `-Wno-format-truncation` — suppresses snprintf truncation warnings
- `-Wno-unused-result` — suppresses fscanf return value warnings

**Dependencies**

Found automatically via CMake:
- `CUDAToolkit` — provides `CUDA::cudart`, `CUDA::cublas`, include paths
- `Threads` — pthreads

No external libraries required. All HTTP, JSON, and GGUF parsing is built-in.

**Library Architecture**

```
libjetson_llm_core.a (static library)
  ├── src/memory/    (budget, kv_cache, pool)     ← .cpp → g++
  ├── src/jetson/    (power, thermal, sysinfo)    ← .cpp → g++
  ├── src/kernels/   (6 CUDA kernels)             ← .cu  → nvcc
  └── src/engine/    (model, decode, sample, tok)  ← .cpp/.cu → g++/nvcc

jetson-llm          → links libjetson_llm_core.a
jetson-llm-server   → links libjetson_llm_core.a + http_server.cpp
test_*              → links libjetson_llm_core.a
```

**File Types**

| Extension | Compiler | Why |
|-----------|----------|-----|
| `.cpp` | g++ | No CUDA kernels, no `__global__`, no `half` arithmetic |
| `.cu` | nvcc | Contains `__global__` kernels, uses `__half2float`, CUDA math |

`decode.cu` is `.cu` because it defines `vec_add_kernel` and `fp16_to_fp32_kernel`.
All other engine files are `.cpp` (they call kernel functions but don't define them).
CUDA headers (`cuda_runtime.h`, `cuda_fp16.h`) are visible to `.cpp` via `CUDAToolkit_INCLUDE_DIRS`.

**Clean Rebuild**

```bash
rm -rf build
cmake -B build -DCMAKE_CUDA_ARCHITECTURES="87" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Troubleshooting**

| Error | Fix |
|-------|-----|
| `fatal error: cuda_runtime.h` | Install CUDA toolkit: `sudo apt install cuda-toolkit-12-6` |
| `aarch64 ONLY` error | Build on Jetson, not x86 |
| `no kernel image for sm_87` | Ensure `-DCMAKE_CUDA_ARCHITECTURES="87"` |
| `nvcc not found` | Add to PATH: `export PATH=/usr/local/cuda/bin:$PATH` |
| Linker errors | Clean build: `rm -rf build` and reconfigure |

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/build.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/build.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
