---
title: 第 5 讲：ML 到硬件的编译流水线 — TVM、基于 MLIR 的编译器与 tinygrad
description: 第 5 讲：ML 到硬件的编译流水线 — TVM、基于 MLIR 的编译器与 tinygrad
---

# 第 5 讲：ML 到硬件的编译流水线 — TVM、基于 MLIR 的编译器与 tinygrad

## 概述

前四讲打下了基础：LLVM IR 作为通用的低层表示（第 1–2 讲），MLIR 作为逐级 lowering 的多层框架（第 3–4 讲）。本讲把这一切连接到真实世界：实际的 ML 编译器如何拿一个 PyTorch 或 TensorFlow 模型，为 GPU、NPU、FPGA 和定制 AI 加速器生成优化后的代码？核心挑战在于理解**端到端流水线**——从 Python 中的 `model.forward()` 调用，到运行在硬件上的融合、分块、向量化的机器码。本讲考察三类代表不同设计哲学的编译器：**Apache TVM**（基于 schedule，LLVM 后端）、**基于 MLIR 的编译器**（IREE、Triton-MLIR、torch-mlir）与 **tinygrad**（极简的惰性求值编译器）。对 AI 硬件工程师而言，理解这些流水线能精确告诉你自定义后端该插在哪里——以及编译器需要从硬件描述中得到什么。

---

## 全局图景：三类编译器

```
                        ML Model (PyTorch, ONNX, etc.)
                                    │
              ┌─────────────────────┼─────────────────────┐
              ▼                     ▼                     ▼
     ┌─────────────────┐  ┌──────────────────┐  ┌─────────────────┐
     │   Apache TVM    │  │  MLIR-Based      │  │    tinygrad      │
     │                 │  │                  │  │                 │
     │  Relay/Relax IR │  │  torch-mlir /    │  │  LazyBuffer DAG │
     │       │         │  │  StableHLO       │  │       │         │
     │       ▼         │  │       │          │  │       ▼         │
     │  TIR (Tensor IR)│  │       ▼          │  │  UOp Graph      │
     │       │         │  │  linalg/tensor   │  │  (linearized IR)│
     │       ▼         │  │       │          │  │       │         │
     │  Schedule +     │  │       ▼          │  │       ▼         │
     │  Auto-tuning    │  │  tiling/fusion   │  │  BEAM search    │
     │       │         │  │       │          │  │  (auto-tuning)  │
     │       ▼         │  │       ▼          │  │       │         │
     │  LLVM codegen   │  │  vector/gpu/llvm │  │       ▼         │
     │  (or microTVM)  │  │  dialect lowering│  │  Backend codegen│
     │       │         │  │       │          │  │  (CUDA/OpenCL/  │
     │       ▼         │  │       ▼          │  │   Metal/custom) │
     │  x86/ARM/CUDA/  │  │  LLVM / NVVM /  │  │       │         │
     │  FPGA / custom  │  │  SPIRV           │  │       ▼         │
     └─────────────────┘  └──────────────────┘  └─────────────────┘
```

---

## 1. Apache TVM

TVM 是**最成熟的开源 ML 编译器**。其关键创新在于**将计算与 schedule 分离**——同一算法可通过 schedule 变换，针对不同硬件做不同优化。

### 流水线

```
PyTorch / ONNX / TensorFlow
         │
         ▼
┌──────────────────────┐
│  Frontend Import     │  torch.export → Relay/Relax graph
│  (Relay or Relax IR) │  ONNX → Relay graph
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Graph-Level Passes  │  Operator fusion (conv+bn+relu → single op)
│                      │  Constant folding, layout optimization (NCHW→NHWC)
│                      │  Quantization-aware rewrites
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  TIR (Tensor IR)     │  Low-level loop representation
│  + Schedule          │  Each op becomes a loop nest with schedule primitives
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Auto-Tuning         │  AutoTVM / MetaSchedule / ANSOR
│                      │  Search tile sizes, unroll factors, vectorization widths
│                      │  for the specific target hardware
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Code Generation     │  TIR → LLVM IR (CPU targets)
│                      │  TIR → CUDA source (GPU targets)
│                      │  TIR → C code (microTVM for MCUs)
│                      │  TIR → Verilog (FPGA, experimental)
└──────────────────────┘
```


<details>
<summary>English original</summary>

**Lecture 5: ML-to-Hardware Compilation Pipelines — TVM, MLIR-Based Compilers & tinygrad**

**Overview**

The previous four lectures built up the foundation: LLVM IR as the universal low-level representation (Lectures 1–2), and MLIR as the multi-level framework for progressive lowering (Lectures 3–4). This lecture connects everything to the real world: how do actual ML compilers take a PyTorch or TensorFlow model and produce optimized code for GPUs, NPUs, FPGAs, and custom AI accelerators? The core challenge is understanding the **end-to-end pipeline** — from a `model.forward()` call in Python to fused, tiled, vectorized machine code running on hardware. We examine three compiler families that represent different design philosophies: **Apache TVM** (schedule-based, LLVM backend), **MLIR-based compilers** (IREE, Triton-MLIR, torch-mlir), and **tinygrad** (minimal lazy-evaluation compiler). For an AI hardware engineer, understanding these pipelines tells you exactly where your custom backend plugs in — and what the compiler needs from your hardware description.

---

**The Big Picture: Three Compiler Families**

```
                        ML Model (PyTorch, ONNX, etc.)
                                    │
              ┌─────────────────────┼─────────────────────┐
              ▼                     ▼                     ▼
     ┌─────────────────┐  ┌──────────────────┐  ┌─────────────────┐
     │   Apache TVM    │  │  MLIR-Based      │  │    tinygrad      │
     │                 │  │                  │  │                 │
     │  Relay/Relax IR │  │  torch-mlir /    │  │  LazyBuffer DAG │
     │       │         │  │  StableHLO       │  │       │         │
     │       ▼         │  │       │          │  │       ▼         │
     │  TIR (Tensor IR)│  │       ▼          │  │  UOp Graph      │
     │       │         │  │  linalg/tensor   │  │  (linearized IR)│
     │       ▼         │  │       │          │  │       │         │
     │  Schedule +     │  │       ▼          │  │       ▼         │
     │  Auto-tuning    │  │  tiling/fusion   │  │  BEAM search    │
     │       │         │  │       │          │  │  (auto-tuning)  │
     │       ▼         │  │       ▼          │  │       │         │
     │  LLVM codegen   │  │  vector/gpu/llvm │  │       ▼         │
     │  (or microTVM)  │  │  dialect lowering│  │  Backend codegen│
     │       │         │  │       │          │  │  (CUDA/OpenCL/  │
     │       ▼         │  │       ▼          │  │   Metal/custom) │
     │  x86/ARM/CUDA/  │  │  LLVM / NVVM /  │  │       │         │
     │  FPGA / custom  │  │  SPIRV           │  │       ▼         │
     └─────────────────┘  └──────────────────┘  └─────────────────┘
```

---

**1. Apache TVM**

TVM is the **most mature open-source ML compiler**. Its key innovation is **separating computation from schedule** — the same algorithm can be optimized differently for different hardware via schedule transformations.

**Pipeline**

```
PyTorch / ONNX / TensorFlow
         │
         ▼
┌──────────────────────┐
│  Frontend Import     │  torch.export → Relay/Relax graph
│  (Relay or Relax IR) │  ONNX → Relay graph
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Graph-Level Passes  │  Operator fusion (conv+bn+relu → single op)
│                      │  Constant folding, layout optimization (NCHW→NHWC)
│                      │  Quantization-aware rewrites
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  TIR (Tensor IR)     │  Low-level loop representation
│  + Schedule          │  Each op becomes a loop nest with schedule primitives
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Auto-Tuning         │  AutoTVM / MetaSchedule / ANSOR
│                      │  Search tile sizes, unroll factors, vectorization widths
│                      │  for the specific target hardware
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Code Generation     │  TIR → LLVM IR (CPU targets)
│                      │  TIR → CUDA source (GPU targets)
│                      │  TIR → C code (microTVM for MCUs)
│                      │  TIR → Verilog (FPGA, experimental)
└──────────────────────┘
```

</details>

### TIR：张量中间表示

TIR 是 TVM 的底层中间表示 —— 大致等价于 MLIR 的 `affine` + `scf` 方言，但带有 TVM 特有的调度原语。

```python
# TVM TIR for a matrix multiply (before scheduling)
import tvm
from tvm import te

M, N, K = 1024, 1024, 1024
A = te.placeholder((M, K), name="A", dtype="float32")
B = te.placeholder((K, N), name="B", dtype="float32")

# Define computation
k = te.reduce_axis((0, K), name="k")
C = te.compute((M, N), lambda i, j: te.sum(A[i, k] * B[k, j], axis=k), name="C")

# Create schedule
s = te.create_schedule(C.op)

# Apply schedule transformations (this is where hardware knowledge enters)
# Split loops for tiling
xo, xi = s[C].split(s[C].op.axis[0], factor=32)   # tile M by 32
yo, yi = s[C].split(s[C].op.axis[1], factor=32)   # tile N by 32
ko, ki = s[C].split(k, factor=4)                   # tile K by 4

# Reorder for cache locality: tile-level outer loops, element-level inner
s[C].reorder(xo, yo, ko, xi, yi, ki)

# Vectorize innermost loop
s[C].vectorize(yi)

# Unroll the k-reduction inner loop
s[C].unroll(ki)
```

### TVM → LLVM 代码生成

TVM 的 LLVM 代码生成（`src/target/llvm/codegen_llvm.cc`）用 IRBuilder API 把 TIR 直接翻译成 LLVM IR：

```python
# Generate LLVM code for x86 with AVX2
target = tvm.target.Target("llvm -mcpu=skylake -mattr=+avx2,+fma")
func = tvm.build(s, [A, B, C], target=target, name="matmul")

# Inspect the generated LLVM IR
print(func.get_source("ll"))
# Shows vectorized loops with <8 x float> operations, FMA intrinsics
```

### 自动调优：MetaSchedule

TVM 自动调优的关键洞见：不要为每个硬件目标手写 schedule，而是**自动搜索合法 schedule 的空间**。

```python
from tvm import meta_schedule as ms

# Define the search space
database = ms.tune_tir(
    mod=tir_module,
    target="nvidia/nvidia-a100",
    max_trials_global=2000,
    work_dir="./tune_results",
)

# The tuner explores tile sizes, loop orders, vectorization widths,
# shared memory usage, and thread binding — evaluating each candidate
# by compiling and running it on the actual hardware
```

| 自动调优器 | 方法 | 搜索空间 |
|---|---|---|
| **AutoTVM** | 基于模板：人工编写带可调旋钮的 schedule 模板 | 受模板设计限制 |
| **ANSOR** | 任务级：生成 sketch + 随机标注 | 更大；能发现新颖的 schedule |
| **MetaSchedule** | 统一：基于 trace，配模块化搜索规则 | 最灵活；可用于生产 |

### microTVM：面向微控制器

针对边缘 AI，TVM 可以把模型编译成裸机 C 代码，运行在 Cortex-M 与 RISC-V 微控制器上：

```python
target = tvm.target.Target("c -mcpu=cortex-m7")
# Generates C code with CMSIS-NN integration
# Runs on devices with as little as 256KB SRAM
```

> **关键洞见：** TVM 的威力在于，同一份模型描述（Relay 图）能编译成服务器上的 AVX-512 代码、GPU 上的 CUDA kernel、Cortex-M 上的 CMSIS-NN 调用 —— 只需更换 target 和 schedule。对 AI 硬件工程师而言，给 TVM 增加一个新 target 意味着实现一个代码生成器（往往只是 LLVM 后端 + 调度规则）并提供自动调优参数。

---

## 2. 基于 MLIR 的编译器

### torch-mlir：PyTorch → MLIR

`torch-mlir` 把 PyTorch 模型转换成 MLIR，是基于 MLIR 的编译流水线的入口。

```
PyTorch Model
      │
      ▼  (torch.export / torch.jit.trace)
TorchScript / FX Graph
      │
      ▼  (torch-mlir)
┌─────────────────────────────────────┐
│  Torch Dialect (MLIR)               │
│  torch.aten.mm, torch.aten.relu,    │
│  torch.aten.conv2d, ...             │
└──────────────┬──────────────────────┘
               │
      ┌────────┼────────┐
      ▼        ▼        ▼
   TOSA    StableHLO   Linalg        Multiple lowering targets
```

```python
# Using torch-mlir
import torch
import torch_mlir

class MyModel(torch.nn.Module):
    def forward(self, x, y):
        return torch.mm(x, y).relu()

model = MyModel()
example_input = (torch.randn(128, 64), torch.randn(64, 256))

# Export to MLIR (linalg dialect)
module = torch_mlir.compile(model, example_input,
                            output_type="linalg-on-tensors")
print(module.operation.get_asm())
```

输出：
```mlir
func.func @forward(%arg0: tensor<128x64xf32>, %arg1: tensor<64x256xf32>) -> tensor<128x256xf32> {
  %cst = arith.constant 0.0 : f32
  %0 = tensor.empty() : tensor<128x256xf32>
  %1 = linalg.fill ins(%cst) outs(%0) -> tensor<128x256xf32>
  %2 = linalg.matmul ins(%arg0, %arg1) outs(%1) -> tensor<128x256xf32>
  %3 = linalg.generic {/* relu */} ins(%2) outs(%0) -> tensor<128x256xf32>
  return %3 : tensor<128x256xf32>
}
```


<details>
<summary>English original</summary>

**TIR: Tensor IR**

TIR is TVM's low-level IR — roughly equivalent to MLIR's `affine` + `scf` dialects but with TVM-specific scheduling primitives.

```python
# TVM TIR for a matrix multiply (before scheduling)
import tvm
from tvm import te

M, N, K = 1024, 1024, 1024
A = te.placeholder((M, K), name="A", dtype="float32")
B = te.placeholder((K, N), name="B", dtype="float32")

# Define computation
k = te.reduce_axis((0, K), name="k")
C = te.compute((M, N), lambda i, j: te.sum(A[i, k] * B[k, j], axis=k), name="C")

# Create schedule
s = te.create_schedule(C.op)

# Apply schedule transformations (this is where hardware knowledge enters)
# Split loops for tiling
xo, xi = s[C].split(s[C].op.axis[0], factor=32)   # tile M by 32
yo, yi = s[C].split(s[C].op.axis[1], factor=32)   # tile N by 32
ko, ki = s[C].split(k, factor=4)                   # tile K by 4

# Reorder for cache locality: tile-level outer loops, element-level inner
s[C].reorder(xo, yo, ko, xi, yi, ki)

# Vectorize innermost loop
s[C].vectorize(yi)

# Unroll the k-reduction inner loop
s[C].unroll(ki)
```

**TVM → LLVM Code Generation**

TVM's LLVM codegen (`src/target/llvm/codegen_llvm.cc`) translates TIR directly to LLVM IR using the IRBuilder API:

```python
# Generate LLVM code for x86 with AVX2
target = tvm.target.Target("llvm -mcpu=skylake -mattr=+avx2,+fma")
func = tvm.build(s, [A, B, C], target=target, name="matmul")

# Inspect the generated LLVM IR
print(func.get_source("ll"))
# Shows vectorized loops with <8 x float> operations, FMA intrinsics
```

**Auto-Tuning: MetaSchedule**

The key insight of TVM's auto-tuning: instead of hand-writing schedules for every hardware target, **search the space of valid schedules automatically**.

```python
from tvm import meta_schedule as ms

# Define the search space
database = ms.tune_tir(
    mod=tir_module,
    target="nvidia/nvidia-a100",
    max_trials_global=2000,
    work_dir="./tune_results",
)

# The tuner explores tile sizes, loop orders, vectorization widths,
# shared memory usage, and thread binding — evaluating each candidate
# by compiling and running it on the actual hardware
```

| Auto-Tuner | Approach | Search Space |
|---|---|---|
| **AutoTVM** | Template-based: human writes schedule template with knobs | Bounded by template design |
| **ANSOR** | Task-level: generates sketch + random annotation | Larger; discovers novel schedules |
| **MetaSchedule** | Unified: trace-based with modular search rules | Most flexible; production-ready |

**microTVM: Targeting Microcontrollers**

For edge AI, TVM can compile models to bare-metal C code that runs on Cortex-M and RISC-V microcontrollers:

```python
target = tvm.target.Target("c -mcpu=cortex-m7")
# Generates C code with CMSIS-NN integration
# Runs on devices with as little as 256KB SRAM
```

> **Key Insight:** TVM's power is that the same model description (Relay graph) compiles to AVX-512 code on a server, CUDA kernels on a GPU, and CMSIS-NN calls on a Cortex-M — by changing only the target and schedule. For an AI hardware engineer, adding a new target to TVM means implementing a code generator (often just LLVM backend + scheduling rules) and providing auto-tuning parameters.

---

**2. MLIR-Based Compilers**

**torch-mlir: PyTorch → MLIR**

`torch-mlir` converts PyTorch models into MLIR, serving as the entry point for MLIR-based compilation pipelines.

```
PyTorch Model
      │
      ▼  (torch.export / torch.jit.trace)
TorchScript / FX Graph
      │
      ▼  (torch-mlir)
┌─────────────────────────────────────┐
│  Torch Dialect (MLIR)               │
│  torch.aten.mm, torch.aten.relu,    │
│  torch.aten.conv2d, ...             │
└──────────────┬──────────────────────┘
               │
      ┌────────┼────────┐
      ▼        ▼        ▼
   TOSA    StableHLO   Linalg        Multiple lowering targets
```

```python
# Using torch-mlir
import torch
import torch_mlir

class MyModel(torch.nn.Module):
    def forward(self, x, y):
        return torch.mm(x, y).relu()

model = MyModel()
example_input = (torch.randn(128, 64), torch.randn(64, 256))

# Export to MLIR (linalg dialect)
module = torch_mlir.compile(model, example_input,
                            output_type="linalg-on-tensors")
print(module.operation.get_asm())
```

Output:
```mlir
func.func @forward(%arg0: tensor<128x64xf32>, %arg1: tensor<64x256xf32>) -> tensor<128x256xf32> {
  %cst = arith.constant 0.0 : f32
  %0 = tensor.empty() : tensor<128x256xf32>
  %1 = linalg.fill ins(%cst) outs(%0) -> tensor<128x256xf32>
  %2 = linalg.matmul ins(%arg0, %arg1) outs(%1) -> tensor<128x256xf32>
  %3 = linalg.generic {/* relu */} ins(%2) outs(%0) -> tensor<128x256xf32>
  return %3 : tensor<128x256xf32>
}
```

</details>

### IREE：Google 的生产级 ML 编译器

IREE（Intermediate Representation Execution Environment）是基于 MLIR 的最完整的 ML 编译器，面向 CPU、GPU 与自定义加速器。

```
StableHLO / TOSA / Linalg
         │
         ▼
┌────────────────────────────┐
│  IREE Flow Dialect         │  Graph-level: dispatch region formation
│  (workload partitioning)   │  Decides which ops run together as one kernel
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  IREE Stream Dialect       │  Execution scheduling: async dispatch,
│  (resource management)     │  buffer allocation, synchronization
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  IREE HAL Dialect          │  Hardware Abstraction Layer:
│  (hardware abstraction)    │  command buffers, executables, buffers
└────────────┬───────────────┘
             │
        ┌────┼────┐
        ▼    ▼    ▼
     LLVM  SPIR-V  VMVX       Backend targets
     (CPU) (GPU)   (portable VM)
```

**IREE 的关键创新：** 它把 ML 编译当作一个**系统问题**，而不只是 kernel 优化问题。它负责：
- 多 kernel 调度与流水线
- 跨异构设备的异步执行
- 内存分配与生命周期管理
- 可执行文件打包与部署

### Triton 的 MLIR 流水线

Triton（被 `torch.compile` 使用）已转向基于 MLIR 的流水线：

```
Triton Python (user-written kernel)
         │
         ▼
┌────────────────────────┐
│  Triton IR (TTIR)      │  High-level: block-level operations
│  tt.dot, tt.load,      │  on tensor<128x128xf16> blocks
│  tt.store, tt.reduce   │
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  Triton GPU IR (TTGIR) │  GPU-specific: thread/warp layout,
│  shared memory alloc,  │  data movement planning
│  pipeline stages       │
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  LLVM Dialect (MLIR)   │  Low-level: near-LLVM-IR operations
│  + NVVM intrinsics     │  with GPU-specific intrinsics
└────────────┬───────────┘
             │
             ▼
         LLVM IR → PTX → cubin
```

```python
# Triton kernel — compiles through MLIR
import triton
import triton.language as tl

@triton.jit
def matmul_kernel(A, B, C, M, N, K,
                  BLOCK_M: tl.constexpr, BLOCK_N: tl.constexpr, BLOCK_K: tl.constexpr):
    pid_m = tl.program_id(0)
    pid_n = tl.program_id(1)

    # Compute block pointers
    offs_m = pid_m * BLOCK_M + tl.arange(0, BLOCK_M)
    offs_n = pid_n * BLOCK_N + tl.arange(0, BLOCK_N)

    # Accumulator
    acc = tl.zeros((BLOCK_M, BLOCK_N), dtype=tl.float32)

    for k in range(0, K, BLOCK_K):
        offs_k = k + tl.arange(0, BLOCK_K)
        a = tl.load(A + offs_m[:, None] * K + offs_k[None, :])
        b = tl.load(B + offs_k[:, None] * N + offs_n[None, :])
        acc += tl.dot(a, b)    # This becomes tt.dot → WGMMA on Hopper

    tl.store(C + offs_m[:, None] * N + offs_n[None, :], acc)
```

Triton 的 `tl.dot` 经由 TTIR → TTGIR → LLVM+NVVM 逐级 lowering，在 Hopper 上最终生成 `wgmma.mma_async` 指令。MLIR 基础设施让每个变换阶段都可组合、可调试。

---

## 3. tinygrad：极简编译器

tinygrad 采取了**截然不同的思路**：不用 LLVM/MLIR 基础设施，而是用 **约 10K 行 Python 实现一个自包含的编译器**。

### 流水线

```
Python tensor operations (tinygrad API)
         │
         ▼
┌────────────────────────┐
│  LazyBuffer DAG        │  Deferred execution: operations are recorded,
│                        │  not executed. Creates a computation graph.
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  Scheduler             │  Decides kernel boundaries: which ops fuse
│  (kernel partitioning) │  into one kernel launch.
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  UOp Graph             │  Linearized IR: ~12 primitive operations
│  (linearized IR)       │  (LOAD, STORE, ALU, REDUCE, CONST, etc.)
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  Optimization          │  Pattern-matching rewrite rules
│  + BEAM Search         │  Auto-tune: tile sizes, local sizes,
│                        │  unroll factors, upcast widths
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  Backend Codegen       │  Emit source code for target:
│                        │  CUDA, OpenCL, Metal, HIP, LLVM, WebGPU
│                        │  Each backend: ~200-500 lines of Python
└────────────────────────┘
```


<details>
<summary>English original</summary>

**IREE: Google's Production ML Compiler**

IREE (Intermediate Representation Execution Environment) is the most complete MLIR-based ML compiler, targeting CPUs, GPUs, and custom accelerators.

```
StableHLO / TOSA / Linalg
         │
         ▼
┌────────────────────────────┐
│  IREE Flow Dialect         │  Graph-level: dispatch region formation
│  (workload partitioning)   │  Decides which ops run together as one kernel
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  IREE Stream Dialect       │  Execution scheduling: async dispatch,
│  (resource management)     │  buffer allocation, synchronization
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  IREE HAL Dialect          │  Hardware Abstraction Layer:
│  (hardware abstraction)    │  command buffers, executables, buffers
└────────────┬───────────────┘
             │
        ┌────┼────┐
        ▼    ▼    ▼
     LLVM  SPIR-V  VMVX       Backend targets
     (CPU) (GPU)   (portable VM)
```

**IREE's key innovation:** It treats ML compilation as a **systems problem**, not just a kernel optimization problem. It handles:
- Multi-kernel scheduling and pipelining
- Async execution across heterogeneous devices
- Memory allocation and lifetime management
- Executable packaging and deployment

**Triton's MLIR Pipeline**

Triton (used by `torch.compile`) has transitioned to an MLIR-based pipeline:

```
Triton Python (user-written kernel)
         │
         ▼
┌────────────────────────┐
│  Triton IR (TTIR)      │  High-level: block-level operations
│  tt.dot, tt.load,      │  on tensor<128x128xf16> blocks
│  tt.store, tt.reduce   │
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  Triton GPU IR (TTGIR) │  GPU-specific: thread/warp layout,
│  shared memory alloc,  │  data movement planning
│  pipeline stages       │
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  LLVM Dialect (MLIR)   │  Low-level: near-LLVM-IR operations
│  + NVVM intrinsics     │  with GPU-specific intrinsics
└────────────┬───────────┘
             │
             ▼
         LLVM IR → PTX → cubin
```

```python
# Triton kernel — compiles through MLIR
import triton
import triton.language as tl

@triton.jit
def matmul_kernel(A, B, C, M, N, K,
                  BLOCK_M: tl.constexpr, BLOCK_N: tl.constexpr, BLOCK_K: tl.constexpr):
    pid_m = tl.program_id(0)
    pid_n = tl.program_id(1)

    # Compute block pointers
    offs_m = pid_m * BLOCK_M + tl.arange(0, BLOCK_M)
    offs_n = pid_n * BLOCK_N + tl.arange(0, BLOCK_N)

    # Accumulator
    acc = tl.zeros((BLOCK_M, BLOCK_N), dtype=tl.float32)

    for k in range(0, K, BLOCK_K):
        offs_k = k + tl.arange(0, BLOCK_K)
        a = tl.load(A + offs_m[:, None] * K + offs_k[None, :])
        b = tl.load(B + offs_k[:, None] * N + offs_n[None, :])
        acc += tl.dot(a, b)    # This becomes tt.dot → WGMMA on Hopper

    tl.store(C + offs_m[:, None] * N + offs_n[None, :], acc)
```

Triton's `tl.dot` is lowered through TTIR → TTGIR → LLVM+NVVM, and on Hopper it ultimately emits `wgmma.mma_async` instructions. The MLIR infrastructure makes each transformation stage composable and debuggable.

---

**3. tinygrad: The Minimal Compiler**

tinygrad takes a **radically different approach**: instead of LLVM/MLIR infrastructure, it implements a **self-contained compiler in ~10K lines of Python**.

**Pipeline**

```
Python tensor operations (tinygrad API)
         │
         ▼
┌────────────────────────┐
│  LazyBuffer DAG        │  Deferred execution: operations are recorded,
│                        │  not executed. Creates a computation graph.
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  Scheduler             │  Decides kernel boundaries: which ops fuse
│  (kernel partitioning) │  into one kernel launch.
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  UOp Graph             │  Linearized IR: ~12 primitive operations
│  (linearized IR)       │  (LOAD, STORE, ALU, REDUCE, CONST, etc.)
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  Optimization          │  Pattern-matching rewrite rules
│  + BEAM Search         │  Auto-tune: tile sizes, local sizes,
│                        │  unroll factors, upcast widths
└────────────┬───────────┘
             │
             ▼
┌────────────────────────┐
│  Backend Codegen       │  Emit source code for target:
│                        │  CUDA, OpenCL, Metal, HIP, LLVM, WebGPU
│                        │  Each backend: ~200-500 lines of Python
└────────────────────────┘
```

</details>

### UOp IR：12 个原语

tinygrad 把所有张量计算归约为约 12 个原语操作：

| UOp | 含义 | 示例 |
|---|---|---|
| `LOAD` | 从 buffer 读取 | 加载权重张量元素 |
| `STORE` | 写入 buffer | 存储输出激活值 |
| `CONST` | 常量值 | bias 初始化的零、scale factor |
| `ALU` | 算术（add、mul、max 等） | 逐元素操作 |
| `REDUCE` | 沿轴归约（sum、max） | 在矩阵乘中对 k 维求和 |
| `DEFINE_GLOBAL` | 声明 buffer 参数 | 输入/输出张量指针 |
| `DEFINE_LOCAL` | 声明 local/shared memory | 共享内存分块 |
| `DEFINE_ACC` | 声明累加器 | 用于部分和的寄存器 |
| `RANGE` | 循环范围 | 对分块迭代 |
| `SPECIAL` | 线程/块索引 | `threadIdx.x`、`blockIdx.y` |
| `BARRIER` | 同步 | `__syncthreads()` |
| `CAST` | 类型转换 | 混合精度下的 FP32→FP16 |

### tinygrad 代码生成示例

```python
import tinygrad
from tinygrad import Tensor

# This lazy expression builds a UOp graph — nothing executes yet
a = Tensor.rand(1024, 512)
b = Tensor.rand(512, 1024)
c = (a @ b).relu()  # matmul + relu — the scheduler will fuse these

# Force execution — triggers compilation and kernel launch
c.realize()
```

内部发生了什么：
1. `a @ b` 创建一个 op=`MATMUL(a, b)` 的 `LazyBuffer`
2. `.relu()` 创建一个 op=`MAX(matmul_result, 0)` 的 `LazyBuffer`
3. `.realize()` 触发调度器
4. 调度器看到 matmul→relu 链，融合为**一个 kernel**
5. 构建 UOp 图：嵌套的 RANGE 循环、从 a/b 的 LOAD、ALU mul+add（归约）、ALU max（relu）、STORE
6. BEAM 搜索为目标 GPU 找到最优分块尺寸
7. 后端生成 CUDA/OpenCL/Metal 源代码
8. 代码经编译（nvcc/clang）并启动

### tinygrad vs. LLVM/MLIR

| 方面 | tinygrad | TVM / MLIR |
|---|---|---|
| **IR 复杂度** | 约 12 个 UOp | 跨多个方言的数百个 op |
| **代码库** | 约 10K 行 Python | 数百万行 C++ |
| **后端工作量** | 每个目标约 300 行 | 每个目标数千行 |
| **优化** | BEAM 搜索（runtime） | 编译期分析 + 可选调优 |
| **成熟度** | 活跃开发，硬件支持有限 | 在众多目标上经过生产验证 |
| **可改造性** | 一个人就能理解整个编译器 | 需要团队才能完全理解 |
| **峰值性能** | 达到厂商库的 70–90%（持续改进） | 配合厂商后端达 90–100% |
| **最适合** | 非 NVIDIA 硬件、快速原型 | 生产部署、NVIDIA GPU、自定义 ASIC |

> **关键洞察：** tinygrad 证明了，用少得惊人的代码行就能构建一个可用的 ML 编译器 —— 核心编译逻辑比大多数工程师设想的更简单。它的局限在于，要在 NVIDIA 硬件上达到峰值性能，需要架构特定的模板（FlashAttention、WGMMA），而这些无法从 12 个原语操作推导出来。对硬件设计者的启示：如果你的加速器的编程模型足够简单，一个 tinygrad 风格的编译器可能就够用了。如果你的硬件具有复杂的、非正交的特性（比如带特定布局要求的张量核心），就需要 MLIR 级别的基础设施来表达它们。

---

## 4. 对比：各流水线擅长之处

### 为你的硬件选择编译器

| 你的硬件 | 推荐编译器 | 原因 |
|---|---|---|
| **标准 CPU（x86、ARM）** | 带 LLVM 后端的 TVM | 成熟的自动调优，LLVM 代码生成能很好地处理 SIMD |
| **NVIDIA GPU** | Triton（MLIR）或直接写 CUDA | 自定义 kernel 用 Triton；标准 op 用 cuDNN/cuBLAS |
| **AMD GPU** | TVM 或 IREE（MLIR） | ROCm LLVM 后端已可用于生产 |
| **移动 NPU（Qualcomm、MediaTek）** | tinygrad 或 TVM | tinygrad 已有 Qualcomm 后端；TVM 有广泛的移动端支持 |
| **Edge TPU（Coral）** | TensorFlow Lite + EdgeTPU 编译器 | 专有，无开源替代方案 |
| **自定义 FPGA 加速器** | MLIR 自定义方言 → HLS/RTL | MLIR 渐进式 lowering 天然契合 FPGA 设计 |
| **自定义 ASIC** | MLIR 自定义方言 → 你的工具链 | 定义与硬件匹配的 op；通过 MLIR 栈逐级 lower |
| **MCU（微控制器，Cortex-M、RISC-V）** | microTVM 或 TFLM | microTVM 用于自动调优的 C；TFLM 用于手工优化的 CMSIS-NN |


<details>
<summary>English original</summary>

**UOp IR: The 12 Primitives**

tinygrad reduces all tensor computation to ~12 primitive operations:

| UOp | Meaning | Example |
|---|---|---|
| `LOAD` | Read from buffer | Load weight tensor element |
| `STORE` | Write to buffer | Store output activation |
| `CONST` | Constant value | Zero for bias init, scale factor |
| `ALU` | Arithmetic (add, mul, max, etc.) | Element-wise operations |
| `REDUCE` | Reduction (sum, max) over axis | Sum over k-dimension in matmul |
| `DEFINE_GLOBAL` | Declare a buffer argument | Input/output tensor pointers |
| `DEFINE_LOCAL` | Declare local/shared memory | Shared memory tile |
| `DEFINE_ACC` | Declare accumulator | Register for partial sums |
| `RANGE` | Loop range | Iteration over tiles |
| `SPECIAL` | Thread/block index | `threadIdx.x`, `blockIdx.y` |
| `BARRIER` | Synchronization | `__syncthreads()` |
| `CAST` | Type conversion | FP32→FP16 for mixed precision |

**tinygrad Codegen Example**

```python
import tinygrad
from tinygrad import Tensor

# This lazy expression builds a UOp graph — nothing executes yet
a = Tensor.rand(1024, 512)
b = Tensor.rand(512, 1024)
c = (a @ b).relu()  # matmul + relu — the scheduler will fuse these

# Force execution — triggers compilation and kernel launch
c.realize()
```

What happens inside:
1. `a @ b` creates a `LazyBuffer` with op=`MATMUL(a, b)`
2. `.relu()` creates a `LazyBuffer` with op=`MAX(matmul_result, 0)`
3. `.realize()` triggers the scheduler
4. Scheduler sees matmul→relu chain and fuses into **one kernel**
5. UOp graph is built: nested RANGE loops, LOAD from a/b, ALU mul+add (reduction), ALU max (relu), STORE
6. BEAM search finds optimal tile sizes for the target GPU
7. Backend emits CUDA/OpenCL/Metal source code
8. Code is compiled (nvcc/clang) and launched

**tinygrad vs. LLVM/MLIR**

| Aspect | tinygrad | TVM / MLIR |
|---|---|---|
| **IR complexity** | ~12 UOps | Hundreds of ops across multiple dialects |
| **Codebase** | ~10K Python | Millions of lines of C++ |
| **Backend effort** | ~300 lines per target | Thousands of lines per target |
| **Optimization** | BEAM search (runtime) | Compile-time analysis + optional tuning |
| **Maturity** | Active development, limited hardware | Production-proven on many targets |
| **Hackability** | One person can understand the whole compiler | Team effort to understand fully |
| **Peak performance** | 70–90% of vendor libraries (improving) | 90–100% with vendor backends |
| **Best for** | Non-NVIDIA hardware, rapid prototyping | Production deployment, NVIDIA GPU, custom ASIC |

> **Key Insight:** tinygrad proves that a functional ML compiler can be built in remarkably few lines — the core compilation logic is simpler than most engineers assume. Its limitation is that peak performance on NVIDIA hardware requires architecture-specific templates (FlashAttention, WGMMA) that can't be derived from 12 primitive operations. The lesson for hardware designers: if your accelerator's programming model is simple enough, a tinygrad-style compiler may be all you need. If your hardware has complex, non-orthogonal features (like tensor cores with specific layout requirements), you'll need MLIR-level infrastructure to express them.

---

**4. Comparison: Where Each Pipeline Excels**

**Choosing a Compiler for Your Hardware**

| Your Hardware | Recommended Compiler | Why |
|---|---|---|
| **Standard CPU (x86, ARM)** | TVM with LLVM backend | Mature auto-tuning, LLVM codegen handles SIMD well |
| **NVIDIA GPU** | Triton (MLIR) or direct CUDA | Triton for custom kernels; cuDNN/cuBLAS for standard ops |
| **AMD GPU** | TVM or IREE (MLIR) | ROCm LLVM backend is production-ready |
| **Mobile NPU (Qualcomm, MediaTek)** | tinygrad or TVM | tinygrad already has Qualcomm backend; TVM has broad mobile support |
| **Edge TPU (Coral)** | TensorFlow Lite + EdgeTPU compiler | Proprietary, no open-source alternative |
| **Custom FPGA accelerator** | MLIR custom dialect → HLS/RTL | MLIR progressive lowering maps naturally to FPGA design |
| **Custom ASIC** | MLIR custom dialect → your toolchain | Define ops matching your hardware; lower through MLIR stack |
| **MCU (Cortex-M, RISC-V)** | microTVM or TFLM | microTVM for auto-tuned C; TFLM for hand-optimized CMSIS-NN |

</details>

### 端到端示例：用 MLIR 定制加速器

如果正在设计一款定制 AI 加速器，编译器流水线大致如下：

```
PyTorch Model
      │
      ▼  torch-mlir
Linalg on Tensors (MLIR)
      │
      ▼  linalg tiling (tile to your MAC array size, e.g., 16×16)
Tiled Linalg + SCF loops
      │
      ▼  custom lowering pass
Your Accelerator Dialect (MLIR)
      │  myaccel.dma_load %weight_sram, %global_weights, [tile_i, tile_k]
      │  myaccel.dma_load %act_sram, %global_acts, [tile_k, tile_j]
      │  myaccel.matmul %acc_regs, %weight_sram, %act_sram
      │  myaccel.activate %acc_regs, "relu"
      │  myaccel.dma_store %global_output, %acc_regs, [tile_i, tile_j]
      │
      ▼  your backend (MLIR → assembly or binary)
Custom Assembly / Binary
      │
      ▼  your assembler / runtime
Executable on your chip
```


关键决策有：
1. **分块尺寸** — 必须匹配硬件的 MAC 阵列维度与 SRAM 容量
2. **数据布局** — SRAM bank 可能需要特定的数据排布方式
3. **双缓冲** — 让 DMA 传输与计算重叠
4. **融合** — 哪些算子能在加速器上运行，哪些回退到 CPU

---

## 5. 收敛：行业正在走向何方

ML 编译器的格局正围绕几个关键理念收敛：

**1. MLIR 作为公共基础设施。** TVM 正在集成 MLIR（TVM Unity）。Triton 已转向 MLIR。IREE 构建在 MLIR 之上。硬件厂商（Intel、AMD、Qualcomm）都在构建 MLIR 后端。就连 tinygrad 的 UOp 图在概念上也类似于一个 MLIR dialect。

**2. 算法与调度分离。** 无论是通过 TVM 的 schedule、MLIR 变换，还是 tinygrad 的 BEAM 搜索 —— 原则都一样：计算只表达一次，执行计划单独优化。

**3. 自动调优取代手写 kernel。** 基于搜索的方法（TVM MetaSchedule、tinygrad BEAM、Triton 自动调优）正在越来越多的工作负载上取代手工优化的库 kernel。例外是 attention 这类关键路径 kernel，手写实现（FlashAttention）仍然占主导。

**4. 软硬件协同设计。** 编译器的能力**约束了硬件设计空间**，反之亦然。在不理解编译器流水线的情况下去设计加速器，就像在不理解软件的情况下去设计 ISA —— 你会造出编译器根本用不上的特性。

---

## 动手练习

1. **TVM 端到端：** 安装 TVM。通过 `torch.export` + `from_exported_program` 从 PyTorch 导入 ResNet-18。针对 `llvm -mcpu=skylake` 编译。提取 LLVM IR（`mod.get_source("ll")`）。找到向量化的循环并识别出 AVX 指令。然后针对 `cuda` 编译，对比生成的 CUDA 源码。

2. **torch-mlir 探索：** 安装 torch-mlir。把一个简单模型（linear + relu）转换为 MLIR linalg。然后手动运行 lowering 流水线：`--linalg-tile --convert-linalg-to-loops --lower-affine --convert-to-llvm`。检查每个阶段的输出。

3. **tinygrad kernel 检视：** 安装 tinygrad。设置 `DEBUG=4` 环境变量。运行一个简单的矩阵乘（`Tensor.rand(512,512) @ Tensor.rand(512,512)`）。检查打印出的 UOp 图与生成的 kernel 源码。切换后端（CPU 用 `CLANG=1`，GPU 用 `CUDA=1`）并对比生成的代码。

4. **定制后端设计（纸面练习）：** 为一款假想的加速器设计编译流水线，其配置为：
   - 32×32 INT8 脉动阵列
   - 128KB 权重 SRAM，64KB 激活值 SRAM
   - 用于 host↔SRAM 传输的 DMA 引擎
   - 输出流水线中融合 ReLU/ReLU6

   写出 MLIR dialect 算子，勾勒从 `linalg.matmul` 到你所设计 dialect 的 lowering，并描述自动调优参数（分块尺寸、双缓冲深度、DMA 调度）。

5. **编译器对比 benchmark：** 取单个模型（例如 MobileNetV2），针对同一目标（例如 x86 CPU 或 CUDA GPU），分别用 TVM（自动调优）、ONNX Runtime 和 tinygrad 编译。比较推理延迟、编译时间和生成的代码体积。记录各编译器在哪些地方做出了不同的分块/融合决策。

---

## 要点

| 概念 | 对 AI 硬件为何重要 |
|---|---|
| TVM 的 schedule 分离 | 计算只表达一次，针对每个硬件目标分别优化 |
| MLIR 渐进式 lowering | 每个 dialect 层级都保留下一层所需的信息 |
| tinygrad 的极简主义 | 一个完整的 ML 编译器可以只有约 10K 行 —— 复杂度是一种选择，而非必需品 |
| 自动调优 | 搜索分块尺寸与 schedule 往往胜过手写优化 |
| 定制 MLIR dialect | 把加速器接入 ML 生态的机制 |
| 收敛到 MLIR | 行业正在标准化 —— 学一次 MLIR，就能用于任何硬件目标 |

---


<details>
<summary>English original</summary>

**End-to-End Example: Custom Accelerator with MLIR**

If you're designing a custom AI accelerator, here's how the compiler pipeline would look:

```
PyTorch Model
      │
      ▼  torch-mlir
Linalg on Tensors (MLIR)
      │
      ▼  linalg tiling (tile to your MAC array size, e.g., 16×16)
Tiled Linalg + SCF loops
      │
      ▼  custom lowering pass
Your Accelerator Dialect (MLIR)
      │  myaccel.dma_load %weight_sram, %global_weights, [tile_i, tile_k]
      │  myaccel.dma_load %act_sram, %global_acts, [tile_k, tile_j]
      │  myaccel.matmul %acc_regs, %weight_sram, %act_sram
      │  myaccel.activate %acc_regs, "relu"
      │  myaccel.dma_store %global_output, %acc_regs, [tile_i, tile_j]
      │
      ▼  your backend (MLIR → assembly or binary)
Custom Assembly / Binary
      │
      ▼  your assembler / runtime
Executable on your chip
```

The critical decisions are:
1. **Tile sizes** — must match your hardware's MAC array dimensions and SRAM capacity
2. **Data layout** — your SRAM banks may require specific data arrangements
3. **Double buffering** — overlap DMA transfers with computation
4. **Fusion** — which ops can run on the accelerator vs. fall back to CPU

---

**5. The Convergence: Where the Industry Is Heading**

The ML compiler landscape is converging around a few key ideas:

**1. MLIR as the common infrastructure.** TVM is integrating MLIR (TVM Unity). Triton has moved to MLIR. IREE is built on MLIR. Hardware vendors (Intel, AMD, Qualcomm) are building MLIR backends. Even tinygrad's UOp graph is conceptually similar to an MLIR dialect.

**2. Separation of algorithm from schedule.** Whether via TVM schedules, MLIR transformations, or tinygrad's BEAM search — the principle is the same: express the computation once, optimize the execution plan separately.

**3. Auto-tuning over hand-written kernels.** The search-based approach (TVM MetaSchedule, tinygrad BEAM, Triton auto-tuning) is replacing hand-optimized library kernels for an increasing fraction of workloads. The exception: critical-path kernels like attention, where hand-written implementations (FlashAttention) still dominate.

**4. Hardware-software co-design.** The compiler's capabilities **constrain the hardware design space**, and vice versa. Designing an accelerator without understanding the compiler pipeline is like designing an ISA without understanding the software — you'll build features that the compiler can't use.

---

**Hands-On Exercises**

1. **TVM end-to-end:** Install TVM. Import a ResNet-18 from PyTorch via `torch.export` + `from_exported_program`. Compile for `llvm -mcpu=skylake`. Extract the LLVM IR (`mod.get_source("ll")`). Find the vectorized loop and identify AVX instructions. Then compile for `cuda` and compare the generated CUDA source.

2. **torch-mlir exploration:** Install torch-mlir. Convert a simple model (linear + relu) to MLIR linalg. Then manually run the lowering pipeline: `--linalg-tile --convert-linalg-to-loops --lower-affine --convert-to-llvm`. Examine the output at each stage.

3. **tinygrad kernel inspection:** Install tinygrad. Set `DEBUG=4` environment variable. Run a simple matmul (`Tensor.rand(512,512) @ Tensor.rand(512,512)`). Examine the printed UOp graph and generated kernel source. Change the backend (`CLANG=1` for CPU, `CUDA=1` for GPU) and compare the generated code.

4. **Custom backend design (paper exercise):** Design a compilation pipeline for a hypothetical accelerator with:
   - 32×32 INT8 systolic array
   - 128KB weight SRAM, 64KB activation SRAM
   - DMA engine for host↔SRAM transfers
   - Fused ReLU/ReLU6 in the output pipeline

   Write the MLIR dialect operations, sketch the lowering from `linalg.matmul` to your dialect, and describe the auto-tuning parameters (tile sizes, double-buffer depth, DMA scheduling).

5. **Compiler comparison benchmark:** Take a single model (e.g., MobileNetV2) and compile it with TVM (auto-tuned), ONNX Runtime, and tinygrad for the same target (e.g., x86 CPU or CUDA GPU). Compare inference latency, compile time, and generated code size. Document where each compiler makes different tiling/fusion decisions.

---

**Key Takeaways**

| Concept | Why It Matters for AI Hardware |
|---|---|
| TVM's schedule separation | Express computation once, optimize for each hardware target separately |
| MLIR progressive lowering | Each dialect level retains information the next level needs |
| tinygrad's minimalism | A complete ML compiler can be ~10K lines — complexity is a choice, not a requirement |
| Auto-tuning | Searching tile sizes and schedules often beats hand-written optimization |
| Custom MLIR dialects | The mechanism for connecting your accelerator to the ML ecosystem |
| Convergence on MLIR | Industry is standardizing — learn MLIR once, apply to any hardware target |

---

</details>

## 资源

* **[Apache TVM Documentation](https://tvm.apache.org/docs/)：** 官方文档、教程与 API 参考。
* **[TVM: An Automated End-to-End Optimizing Compiler for Deep Learning (OSDI 2018)](https://www.usenix.org/conference/osdi18/presentation/chen)：** TVM 的奠基性论文。
* **[ANSOR: Generating High-Performance Tensor Programs for Deep Learning (OSDI 2020)](https://www.usenix.org/conference/osdi20/presentation/zheng)：** TVM 的自动调度。
* **[torch-mlir GitHub](https://github.com/llvm/torch-mlir)：** PyTorch 到 MLIR 的桥接。
* **[IREE Documentation](https://iree.dev/)：** Google 基于 MLIR 的生产级 ML 编译器。
* **[Triton MLIR Pipeline (OpenAI)](https://triton-lang.org/)：** Triton 如何用 MLIR 做 GPU kernel 编译。
* **[tinygrad GitHub](https://github.com/tinygrad/tinygrad)：** 代码库——一个周末就能读完。
* **[Compiler Explorer (godbolt.org)](https://godbolt.org/)：** 用于查看编译器输出的交互式工具——支持 LLVM IR、多种架构。


<details>
<summary>English original</summary>

**Resources**

* **[Apache TVM Documentation](https://tvm.apache.org/docs/):** Official docs, tutorials, and API reference.
* **[TVM: An Automated End-to-End Optimizing Compiler for Deep Learning (OSDI 2018)](https://www.usenix.org/conference/osdi18/presentation/chen):** The foundational TVM paper.
* **[ANSOR: Generating High-Performance Tensor Programs for Deep Learning (OSDI 2020)](https://www.usenix.org/conference/osdi20/presentation/zheng):** Auto-scheduling for TVM.
* **[torch-mlir GitHub](https://github.com/llvm/torch-mlir):** PyTorch to MLIR bridge.
* **[IREE Documentation](https://iree.dev/):** Google's production MLIR-based ML compiler.
* **[Triton MLIR Pipeline (OpenAI)](https://triton-lang.org/):** How Triton uses MLIR for GPU kernel compilation.
* **[tinygrad GitHub](https://github.com/tinygrad/tinygrad):** The codebase — readable in a weekend.
* **[Compiler Explorer (godbolt.org)](https://godbolt.org/):** Interactive tool for examining compiler output — supports LLVM IR, multiple architectures.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track F - AI Chip Design/Lectures/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20F%20-%20AI%20Chip%20Design/Lectures/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
