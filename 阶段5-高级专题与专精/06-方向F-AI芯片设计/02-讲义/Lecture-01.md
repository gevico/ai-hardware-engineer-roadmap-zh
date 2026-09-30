---
title: 第 1 讲：LLVM IR 与架构
description: 第 1 讲：LLVM IR 与架构
---

# 第 1 讲：LLVM IR 与架构

## 概述

每个 AI 编译器 —— TVM、Triton、tinygrad、XLA、torch.compile —— 最终都需要把高层张量运算转成在特定芯片上运行的**机器指令**。LLVM 就是让这件事成为可能的基础设施，**无需为每个新硬件目标从头编写一个完整编译器**。本讲要解决的核心问题是：LLVM 如何以一种既硬件无关（使优化在任何地方都适用）、又硬件感知（使最终代码能利用特定芯片特性）的方式来表示程序？需要带走的思维模型是：LLVM 是一个**通用翻译层**：前端（C、Rust、CUDA、ML 编译器）把程序 lower 成 LLVM IR，LLVM 优化该 IR，然后由目标专用后端生成原生代码。对 AI 芯片设计者而言，理解 LLVM IR 意味着可以为自己的定制加速器编写后端，并立刻继承数十年的编译器优化成果 —— 而无需重新实现常量折叠、死代码消除或循环展开。

---

## 为什么 LLVM 对 AI 硬件很重要

| 没有 LLVM | 有 LLVM |
|---|---|
| 每块新芯片都需要从头编写完整编译器 | 只需编写后端；继承优化流水线 |
| 优化按目标重复实现 | 约 70 个优化 pass 在所有目标间共享 |
| ML 框架必须直接生成汇编 | 框架发出 LLVM IR；LLVM 处理指令选择、寄存器分配、调度 |
| 没有生态工具 | 免费获得调试器（LLDB）、性能分析器、sanitizer、LTO |

**AI 领域的具体例子：**
- **TVM** 用 LLVM 生成 CPU kernel（x86 AVX-512、ARM NEON/SVE）和 GPU kernel（CUDA 的 NVPTX、ROCm 的 AMDGPU）
- **Triton**（被 torch.compile 使用）lower 到 LLVM IR → NVPTX 后端 → PTX → cubin
- **Julia**（用于 Flux.jl ML 框架）编译到 LLVM IR
- **MLIR**（后续几讲）构建在 LLVM 的基础设施之上
- **定制 AI 加速器编译器**（Cerebras、SambaNova、Graphcore）使用 LLVM 后端

---

## LLVM 架构：三阶段设计

```
┌──────────────┐     ┌──────────────────┐     ┌──────────────────┐
│   Frontend    │     │   Middle-End     │     │    Backend       │
│              │     │   (Optimizer)     │     │   (CodeGen)      │
│  C → Clang   │     │                  │     │                  │
│  Rust → rustc│────▶│   LLVM IR        │────▶│  x86-64          │
│  CUDA → nvcc │     │   Optimization   │     │  AArch64         │
│  TVM → codegen│    │   Passes         │     │  NVPTX (GPU)     │
│  Triton      │     │                  │     │  AMDGPU          │
│              │     │                  │     │  RISC-V          │
│              │     │                  │     │  Your custom chip │
└──────────────┘     └──────────────────┘     └──────────────────┘
```

关键洞见是**关注点分离**：
- 前端只需发出合法的 LLVM IR —— 它们不了解寄存器、指令编码或流水线冒险
- 优化器在 LLVM IR 上工作，且（大部分）与目标无关 —— 它不知道代码来自 C 还是神经网络编译器
- 后端只需把 LLVM IR 翻译成机器码 —— 它们不知道程序是 web 服务器还是矩阵乘 kernel

> **关键洞见：** 正是这种三阶段设计让 LLVM 主宰了编译器基础设施。新增一门源语言，只需编写一个前端。新增一个硬件目标，只需编写一个后端。N×M 问题（N 种语言 × M 个目标）变成 N+M。对 AI 硬件而言，这意味着只要写好后端，你的定制芯片就免费获得一个编译器 —— 而且每个发出 LLVM IR 的 ML 框架都能以你的芯片为目标。

---

## LLVM IR：语言之间的语言

LLVM IR 是一种**带类型、基于 SSA 的底层虚拟指令集**。这些性质每一条都很重要：

### 静态单赋值（SSA）

每个变量**恰好被赋值一次**。不是修改变量，而是创建新变量。

```llvm
; NOT SSA (imperative style — LLVM doesn't allow this):
;   x = 5
;   x = x + 3    ← x is assigned twice

; SSA form (what LLVM requires):
%x1 = add i32 5, 3       ; %x1 = 8, assigned once
%x2 = mul i32 %x1, 2     ; %x2 = 16, assigned once
```

**为什么用 SSA？** 它让优化的正确性变得显而易见。如果 `%x1` 恰好只定义一次，那么 `%x1` 的每一次使用看到的都是同一个值 —— 无需追踪哪个赋值到达哪次使用。这使得**常量传播、死代码消除和公共子表达式消除**成为简单的图变换。

当控制流汇合时（例如 if-else 之后），SSA 用 **phi 节点**来选择使用哪个值：

```llvm
; if (cond) { a = 1; } else { a = 2; }
; result = a;

entry:
  br i1 %cond, label %then, label %else

then:
  br label %merge

else:
  br label %merge

merge:
  %a = phi i32 [ 1, %then ], [ 2, %else ]
  ; %a is 1 if we came from %then, 2 if from %else
```


<details>
<summary>English original</summary>

**Lecture 1: LLVM IR & Architecture**

**Overview**

Every AI compiler — TVM, Triton, tinygrad, XLA, torch.compile — eventually needs to turn high-level tensor operations into **machine instructions** that run on a specific chip. LLVM is the infrastructure that makes this possible **without writing a full compiler from scratch** for every new hardware target. The core challenge this lecture addresses is: how does LLVM represent programs in a way that is both hardware-independent (so optimizations apply everywhere) and hardware-aware (so the final code exploits specific chip features)? The mental model to carry forward is that LLVM is a **universal translation layer**: frontends (C, Rust, CUDA, ML compilers) lower their programs into LLVM IR, LLVM optimizes the IR, and then a target-specific backend generates native code. For an AI chip designer, understanding LLVM IR means you can write a backend for your custom accelerator and immediately inherit decades of compiler optimizations — without reimplementing constant folding, dead code elimination, or loop unrolling.

---

**Why LLVM Matters for AI Hardware**

| Without LLVM | With LLVM |
|---|---|
| Every new chip needs a full compiler from scratch | Write only the backend; inherit the optimization pipeline |
| Optimizations reimplemented per target | ~70 optimization passes shared across all targets |
| ML frameworks must generate assembly directly | Frameworks emit LLVM IR; LLVM handles instruction selection, register allocation, scheduling |
| No ecosystem tooling | Get debuggers (LLDB), profilers, sanitizers, LTO for free |

**Concrete examples in AI:**
- **TVM** uses LLVM to generate CPU kernels (x86 AVX-512, ARM NEON/SVE) and GPU kernels (NVPTX for CUDA, AMDGPU for ROCm)
- **Triton** (used by torch.compile) lowers to LLVM IR → NVPTX backend → PTX → cubin
- **Julia** (used in Flux.jl ML framework) compiles to LLVM IR
- **MLIR** (next lectures) is built on top of LLVM's infrastructure
- **Custom AI accelerator compilers** (Cerebras, SambaNova, Graphcore) use LLVM backends

---

**LLVM Architecture: The Three-Phase Design**

```
┌──────────────┐     ┌──────────────────┐     ┌──────────────────┐
│   Frontend    │     │   Middle-End     │     │    Backend       │
│              │     │   (Optimizer)     │     │   (CodeGen)      │
│  C → Clang   │     │                  │     │                  │
│  Rust → rustc│────▶│   LLVM IR        │────▶│  x86-64          │
│  CUDA → nvcc │     │   Optimization   │     │  AArch64         │
│  TVM → codegen│    │   Passes         │     │  NVPTX (GPU)     │
│  Triton      │     │                  │     │  AMDGPU          │
│              │     │                  │     │  RISC-V          │
│              │     │                  │     │  Your custom chip │
└──────────────┘     └──────────────────┘     └──────────────────┘
```

The key insight is **separation of concerns**:
- Frontends only need to emit valid LLVM IR — they don't know about registers, instruction encoding, or pipeline hazards
- The optimizer works on LLVM IR and is target-independent (mostly) — it doesn't know whether the code came from C or a neural network compiler
- Backends only need to translate LLVM IR to machine code — they don't know whether the program is a web server or a matrix multiply kernel

> **Key Insight:** This three-phase design is why LLVM dominates compiler infrastructure. Adding a new source language means writing one frontend. Adding a new hardware target means writing one backend. The N×M problem (N languages × M targets) becomes N+M. For AI hardware, this means your custom chip gets a compiler for free once you write the backend — and every ML framework that emits LLVM IR can target your chip.

---

**LLVM IR: The Language Between Languages**

LLVM IR is a **typed, SSA-based, low-level virtual instruction set**. Each of these properties matters:

**Static Single Assignment (SSA)**

Every variable is **assigned exactly once**. Instead of mutating variables, you create new ones.

```llvm
; NOT SSA (imperative style — LLVM doesn't allow this):
;   x = 5
;   x = x + 3    ← x is assigned twice

; SSA form (what LLVM requires):
%x1 = add i32 5, 3       ; %x1 = 8, assigned once
%x2 = mul i32 %x1, 2     ; %x2 = 16, assigned once
```

**Why SSA?** It makes optimization trivially correct. If `%x1` is defined exactly once, every use of `%x1` sees the same value — no need to track which assignment reaches which use. This enables **constant propagation, dead code elimination, and common subexpression elimination** to be simple graph transformations.

When control flow merges (e.g., after an if-else), SSA uses **phi nodes** to select which value to use:

```llvm
; if (cond) { a = 1; } else { a = 2; }
; result = a;

entry:
  br i1 %cond, label %then, label %else

then:
  br label %merge

else:
  br label %merge

merge:
  %a = phi i32 [ 1, %then ], [ 2, %else ]
  ; %a is 1 if we came from %then, 2 if from %else
```

</details>

### 类型系统

LLVM IR 是强类型的。每个值都有显式类型。这能尽早发现错误，并支撑基于类型的优化。

| 类型 | 语法 | AI 相关性 |
|---|---|---|
| 整数 | `i1`、`i8`、`i16`、`i32`、`i64` | INT8/INT4 量化权重、布尔掩码 |
| 浮点 | `half`（f16）、`bfloat`（bf16）、`float`（f32）、`double`（f64） | 模型精度：FP16/BF16 训练、FP32 累加 |
| 向量 | `<4 x float>`、`<16 x i8>` | SIMD：AVX-512 一次处理 16 个 float，NEON 一次处理 16 个 byte |
| 指针 | `ptr`（LLVM 15 起为 opaque） | 张量缓冲区的内存地址 |
| 数组 | `[1024 x float]` | 固定尺寸的张量维度 |
| 结构体 | `{ i32, float, ptr }` | 张量元数据（shape、步长、数据指针） |

### 关键指令

**算术：**
```llvm
%sum  = add i32 %a, %b              ; integer add
%prod = fmul float %x, %y           ; floating-point multiply
%mac  = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
                                      ; fused multiply-add: a*b + c
                                      ; maps to FMA instruction on hardware that supports it
```

**访存：**
```llvm
%ptr  = alloca [1024 x float]        ; allocate 1024 floats on stack
%val  = load float, ptr %ptr          ; load from memory
store float %result, ptr %out_ptr     ; store to memory
%elem = getelementptr [1024 x float], ptr %ptr, i64 0, i64 %idx
                                      ; pointer arithmetic for array indexing
```

**控制流：**
```llvm
br i1 %cond, label %true_bb, label %false_bb   ; conditional branch
br label %loop_header                            ; unconditional branch
ret float %result                                ; return
```

**向量操作（对 AI 至关重要）：**
```llvm
; Load 8 floats from memory as a vector
%vec = load <8 x float>, ptr %tensor_ptr

; Element-wise multiply two 8-float vectors
%mul = fmul <8 x float> %vec_a, %vec_b

; Horizontal reduction (sum all 8 elements)
%sum = call float @llvm.vector.reduce.fadd.v8f32(float 0.0, <8 x float> %mul)

; Shuffle: rearrange elements (used in transpose, gather operations)
%shuf = shufflevector <4 x float> %a, <4 x float> %b, <4 x i32> <i32 0, i32 4, i32 1, i32 5>
```

---

## 实践中的 LLVM IR：一个点积

考虑计算两个 4 元素向量的点积——这是每一次矩阵乘内部的基本操作，而矩阵乘是神经网络计算的核心。

**C 源码：**
```c
float dot4(float *a, float *b) {
    float sum = 0.0f;
    for (int i = 0; i < 4; i++)
        sum += a[i] * b[i];
    return sum;
}
```

**LLVM IR（简化版，优化之后）：**
```llvm
define float @dot4(ptr %a, ptr %b) {
entry:
  ; Load all 4 elements as vectors
  %va = load <4 x float>, ptr %a, align 16
  %vb = load <4 x float>, ptr %b, align 16

  ; Element-wise multiply
  %vmul = fmul <4 x float> %va, %vb

  ; Horizontal sum (reduce)
  %sum = call float @llvm.vector.reduce.fadd.v4f32(float 0.0, <4 x float> %vmul)

  ret float %sum
}
```

**各后端生成的结果：**

| 目标 | 生成的指令 |
|---|---|
| x86-64 (AVX) | `vmovaps` → `vmulps` → `vhaddps` → `vhaddps` |
| AArch64 (NEON) | `ld1` → `fmul` → `faddp` → `faddp` |
| NVPTX (CUDA) | `ld.global.v4.f32` → `fma.rn.f32` (×4) |

**同一份 LLVM IR** 为三种完全不同的架构生成了最优代码。这就是**三阶段设计**的威力。

---

## LLVM IR 的表示形式：三种形态

LLVM IR 存在三种等价形式：

| 形式 | 扩展名 | 使用场景 |
|---|---|---|
| **人类可读文本** | `.ll` | 调试、学习、人工检查 |
| **Bitcode（二进制）** | `.bc` | 磁盘存储、LTO（链接时优化） |
| **内存中的 C++ 对象** | — | 由前端和 pass 以编程方式构造 |

```bash
# Compile C to LLVM IR (text)
clang -S -emit-llvm -O2 dot4.c -o dot4.ll

# Compile to bitcode
clang -c -emit-llvm -O2 dot4.c -o dot4.bc

# Convert between forms
llvm-as dot4.ll -o dot4.bc    # text → bitcode
llvm-dis dot4.bc -o dot4.ll   # bitcode → text

# Compile bitcode to native object
llc dot4.bc -o dot4.o -filetype=obj

# Run optimization passes on bitcode
opt -O2 dot4.bc -o dot4_opt.bc
```

> **关键洞见：** TVM 这类 ML 编译器使用 C++ API（或 C 绑定）以编程方式构造 LLVM IR。它们从不写 `.ll` 文件——而是在内存中构建 `llvm::Module`、`llvm::Function`、`llvm::BasicBlock`、`llvm::Instruction` 对象，然后调用后端直接生成机器码。理解文本形式对调试必不可少，但实践中要用的是编程式 API。

---


<details>
<summary>English original</summary>

**Type System**

LLVM IR is strongly typed. Every value has an explicit type. This catches errors early and enables type-based optimizations.

| Type | Syntax | AI Relevance |
|---|---|---|
| Integer | `i1`, `i8`, `i16`, `i32`, `i64` | INT8/INT4 quantized weights, boolean masks |
| Float | `half` (f16), `bfloat` (bf16), `float` (f32), `double` (f64) | Model precision: FP16/BF16 training, FP32 accumulation |
| Vector | `<4 x float>`, `<16 x i8>` | SIMD: AVX-512 processes 16 floats, NEON processes 16 bytes |
| Pointer | `ptr` (opaque since LLVM 15) | Memory addresses for tensor buffers |
| Array | `[1024 x float]` | Fixed-size tensor dimensions |
| Struct | `{ i32, float, ptr }` | Tensor metadata (shape, stride, data pointer) |

**Key Instructions**

**Arithmetic:**
```llvm
%sum  = add i32 %a, %b              ; integer add
%prod = fmul float %x, %y           ; floating-point multiply
%mac  = call float @llvm.fmuladd.f32(float %a, float %b, float %c)
                                      ; fused multiply-add: a*b + c
                                      ; maps to FMA instruction on hardware that supports it
```

**Memory:**
```llvm
%ptr  = alloca [1024 x float]        ; allocate 1024 floats on stack
%val  = load float, ptr %ptr          ; load from memory
store float %result, ptr %out_ptr     ; store to memory
%elem = getelementptr [1024 x float], ptr %ptr, i64 0, i64 %idx
                                      ; pointer arithmetic for array indexing
```

**Control flow:**
```llvm
br i1 %cond, label %true_bb, label %false_bb   ; conditional branch
br label %loop_header                            ; unconditional branch
ret float %result                                ; return
```

**Vector operations (critical for AI):**
```llvm
; Load 8 floats from memory as a vector
%vec = load <8 x float>, ptr %tensor_ptr

; Element-wise multiply two 8-float vectors
%mul = fmul <8 x float> %vec_a, %vec_b

; Horizontal reduction (sum all 8 elements)
%sum = call float @llvm.vector.reduce.fadd.v8f32(float 0.0, <8 x float> %mul)

; Shuffle: rearrange elements (used in transpose, gather operations)
%shuf = shufflevector <4 x float> %a, <4 x float> %b, <4 x i32> <i32 0, i32 4, i32 1, i32 5>
```

---

**LLVM IR in Practice: A Dot Product**

Consider computing the dot product of two 4-element vectors — the fundamental operation inside every matrix multiply, which is the core of neural network computation.

**C source:**
```c
float dot4(float *a, float *b) {
    float sum = 0.0f;
    for (int i = 0; i < 4; i++)
        sum += a[i] * b[i];
    return sum;
}
```

**LLVM IR (simplified, after optimization):**
```llvm
define float @dot4(ptr %a, ptr %b) {
entry:
  ; Load all 4 elements as vectors
  %va = load <4 x float>, ptr %a, align 16
  %vb = load <4 x float>, ptr %b, align 16

  ; Element-wise multiply
  %vmul = fmul <4 x float> %va, %vb

  ; Horizontal sum (reduce)
  %sum = call float @llvm.vector.reduce.fadd.v4f32(float 0.0, <4 x float> %vmul)

  ret float %sum
}
```

**What the backends produce:**

| Target | Generated instructions |
|---|---|
| x86-64 (AVX) | `vmovaps` → `vmulps` → `vhaddps` → `vhaddps` |
| AArch64 (NEON) | `ld1` → `fmul` → `faddp` → `faddp` |
| NVPTX (CUDA) | `ld.global.v4.f32` → `fma.rn.f32` (×4) |

The **same LLVM IR** produces optimal code for three completely different architectures. This is the power of the **three-phase design**.

---

**LLVM IR Representations: Three Forms**

LLVM IR exists in three equivalent forms:

| Form | Extension | Use Case |
|---|---|---|
| **Human-readable text** | `.ll` | Debugging, learning, manual inspection |
| **Bitcode (binary)** | `.bc` | On-disk storage, LTO (Link-Time Optimization) |
| **In-memory C++ objects** | — | Programmatic construction by frontends and passes |

```bash
# Compile C to LLVM IR (text)
clang -S -emit-llvm -O2 dot4.c -o dot4.ll

# Compile to bitcode
clang -c -emit-llvm -O2 dot4.c -o dot4.bc

# Convert between forms
llvm-as dot4.ll -o dot4.bc    # text → bitcode
llvm-dis dot4.bc -o dot4.ll   # bitcode → text

# Compile bitcode to native object
llc dot4.bc -o dot4.o -filetype=obj

# Run optimization passes on bitcode
opt -O2 dot4.bc -o dot4_opt.bc
```

> **Key Insight:** ML compilers like TVM construct LLVM IR programmatically using the C++ API (or the C bindings). They never write `.ll` files — they build `llvm::Module`, `llvm::Function`, `llvm::BasicBlock`, and `llvm::Instruction` objects in memory, then call the backend to emit machine code directly. Understanding the text form is essential for debugging, but the programmatic API is what you'll use in practice.

---

</details>

## 模块结构

一个 LLVM IR 模块是一个完整的编译单元。对于 AI kernel，一个模块通常包含一个或少数几个函数（kernel 入口点）以及元数据。

```llvm
; Module-level declarations
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Global constant (e.g., lookup table for activation function)
@relu_lut = internal constant [256 x i8] [ ... ]

; Function declaration (external — will be linked later)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

; Function definition (the actual kernel)
define void @matmul_4x4(ptr %A, ptr %B, ptr %C) #0 {
entry:
  ; ... kernel body ...
  ret void
}

; Function attributes
attributes #0 = { nounwind "target-cpu"="skylake" "target-features"="+avx2,+fma" }

; Metadata (debug info, optimization hints)
!llvm.module.flags = !{!0}
!0 = !{i32 1, !"wchar_size", i32 4}
```

**AI 编译器的关键字段：**
- `target datalayout` — 字节序、指针大小、对齐。你的自研芯片自行定义。
- `target triple` — `arch-vendor-os`。对于 NVIDIA GPU：`nvptx64-nvidia-cuda`。对于你的芯片：`myaccel-unknown-unknown`。
- `attributes` — 告知优化器哪些硬件特性可用（AVX-512、FMA 等）

---

## 地址空间

LLVM 通过指针类型支持多个地址空间，这对 GPU 与加速器编程至关重要，因为不同内存区域的性能特征各不相同。

| 地址空间 | GPU 含义 | AI 相关性 |
|---|---|---|
| 0（默认） | 通用 | CPU 内存，或通用 GPU 内存 |
| 1 | Global | VRAM 中的张量数据（权重、激活值） |
| 3 | Shared | 片上 SRAM 中的分块缓冲（GPU 上的共享内存） |
| 4 | Constant | 只读数据（量化缩放因子、LUT） |
| 5 | Local（私有） | 每线程的寄存器/栈 |

```llvm
; Load from global memory (GPU VRAM)
%val = load float, ptr addrspace(1) %global_ptr

; Store to shared memory (on-chip SRAM)
store float %val, ptr addrspace(3) %shared_ptr
```

对于自研 AI 加速器，你要定义自己的地址空间映射来描述存储层次（例如地址空间 1 = 权重 SRAM，2 = 激活值 SRAM，3 = 累加器寄存器）。

---

## Intrinsics：硬件特定操作

Intrinsics 是 LLVM 暴露**硬件特定操作**的机制，这些操作在通用 IR 中没有对应表示。它们看起来像函数调用，但会编译为**特定指令**。

```llvm
; Fused multiply-add (maps to FMA instruction)
%fma = call float @llvm.fmuladd.f32(float %a, float %b, float %c)

; Vector reduction (maps to horizontal add on x86, FADDP on ARM)
%sum = call float @llvm.vector.reduce.fadd.v8f32(float 0.0, <8 x float> %v)

; Matrix multiply intrinsic (AMX on Sapphire Rapids)
call void @llvm.x86.tamx.tdpbf16ps(i8 %dst, i8 %src1, i8 %src2)

; NVIDIA-specific: warp shuffle (requires NVPTX backend)
%shfl = call i32 @llvm.nvvm.shfl.sync.bfly.i32(i32 -1, i32 %val, i32 %lane, i32 31)
```

**对于自研加速器设计：** 你要定义自己的 intrinsics。若芯片有专用的 MAC（乘累加）单元，作用于 8×8 INT8 分块，则可这样定义：
```llvm
declare <64 x i32> @llvm.myaccel.mac.i8.8x8(<64 x i8> %a, <64 x i8> %b, <64 x i32> %acc)
```

后端把这个 intrinsic 模式匹配到你的硬件指令。

---


<details>
<summary>English original</summary>

**Module Structure**

An LLVM IR module is a complete compilation unit. For an AI kernel, a module typically contains one or a few functions (the kernel entry points) plus metadata.

```llvm
; Module-level declarations
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Global constant (e.g., lookup table for activation function)
@relu_lut = internal constant [256 x i8] [ ... ]

; Function declaration (external — will be linked later)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)

; Function definition (the actual kernel)
define void @matmul_4x4(ptr %A, ptr %B, ptr %C) #0 {
entry:
  ; ... kernel body ...
  ret void
}

; Function attributes
attributes #0 = { nounwind "target-cpu"="skylake" "target-features"="+avx2,+fma" }

; Metadata (debug info, optimization hints)
!llvm.module.flags = !{!0}
!0 = !{i32 1, !"wchar_size", i32 4}
```

**Key fields for AI compilers:**
- `target datalayout` — endianness, pointer sizes, alignment. Your custom chip defines its own.
- `target triple` — `arch-vendor-os`. For NVIDIA GPUs: `nvptx64-nvidia-cuda`. For your chip: `myaccel-unknown-unknown`.
- `attributes` — tell the optimizer which hardware features are available (AVX-512, FMA, etc.)

---

**Address Spaces**

LLVM supports multiple address spaces via the pointer type, which is critical for GPU and accelerator programming where different memory regions have different performance characteristics.

| Address Space | GPU Meaning | AI Relevance |
|---|---|---|
| 0 (default) | Generic | CPU memory, or generic GPU memory |
| 1 | Global | Tensor data in VRAM (weights, activations) |
| 3 | Shared | Tile buffers in on-chip SRAM (shared memory on GPU) |
| 4 | Constant | Read-only data (quantization scale factors, LUTs) |
| 5 | Local (private) | Per-thread registers/stack |

```llvm
; Load from global memory (GPU VRAM)
%val = load float, ptr addrspace(1) %global_ptr

; Store to shared memory (on-chip SRAM)
store float %val, ptr addrspace(3) %shared_ptr
```

For a custom AI accelerator, you define your own address space mapping to describe your memory hierarchy (e.g., address space 1 = weight SRAM, 2 = activation SRAM, 3 = accumulator registers).

---

**Intrinsics: Hardware-Specific Operations**

Intrinsics are LLVM's mechanism for exposing **hardware-specific operations** that have no equivalent in generic IR. They look like function calls but compile to **specific instructions**.

```llvm
; Fused multiply-add (maps to FMA instruction)
%fma = call float @llvm.fmuladd.f32(float %a, float %b, float %c)

; Vector reduction (maps to horizontal add on x86, FADDP on ARM)
%sum = call float @llvm.vector.reduce.fadd.v8f32(float 0.0, <8 x float> %v)

; Matrix multiply intrinsic (AMX on Sapphire Rapids)
call void @llvm.x86.tamx.tdpbf16ps(i8 %dst, i8 %src1, i8 %src2)

; NVIDIA-specific: warp shuffle (requires NVPTX backend)
%shfl = call i32 @llvm.nvvm.shfl.sync.bfly.i32(i32 -1, i32 %val, i32 %lane, i32 31)
```

**For custom accelerator design:** You define your own intrinsics. If your chip has a dedicated MAC (multiply-accumulate) unit that operates on 8×8 INT8 tiles, you'd define:
```llvm
declare <64 x i32> @llvm.myaccel.mac.i8.8x8(<64 x i8> %a, <64 x i8> %b, <64 x i32> %acc)
```

The backend pattern-matches this intrinsic to your hardware instruction.

---

</details>

## 以编程方式构建 LLVM IR

ML 编译器（TVM、Triton 等）实际上就是这样构造 LLVM IR 的——不是编写文本文件，而是使用 C++ API。

```cpp
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

// Create context and module
LLVMContext ctx;
Module mod("ai_kernel", ctx);
IRBuilder<> builder(ctx);

// Define function: void relu(float* input, float* output, int n)
Type *floatPtrTy = PointerType::get(Type::getFloatTy(ctx), 0);
Type *i32Ty = Type::getInt32Ty(ctx);
FunctionType *fnTy = FunctionType::get(
    Type::getVoidTy(ctx),
    {floatPtrTy, floatPtrTy, i32Ty},
    false
);
Function *relu = Function::Create(fnTy, Function::ExternalLinkage, "relu", mod);

// Create basic blocks
BasicBlock *entry = BasicBlock::Create(ctx, "entry", relu);
BasicBlock *loop  = BasicBlock::Create(ctx, "loop", relu);
BasicBlock *exit  = BasicBlock::Create(ctx, "exit", relu);

// Entry block: initialize loop counter
builder.SetInsertPoint(entry);
builder.CreateBr(loop);

// Loop block: load, compare, select (ReLU), store
builder.SetInsertPoint(loop);
PHINode *i = builder.CreatePHI(i32Ty, 2, "i");
i->addIncoming(ConstantInt::get(i32Ty, 0), entry);

// ... load input[i], compute max(0, val), store to output[i] ...
// ... increment i, branch back or exit ...
```

> **关键洞察：** TVM 的 `codegen_llvm.cc` 正是这么做的——它遍历 TVM IR（TIR），为每个节点发出 LLVM `IRBuilder` 调用。如果要为自定义 AI 加速器构建编译器，理解 IRBuilder API 是必不可少的。你不需要是编译器专家——一旦理解了 IR 结构，这套 API 就很直白。

---

## 动手练习

1. **检查 ML 编译器输出：** 安装 TVM，针对 `llvm` 目标编译一个简单模型（例如对 1024 元素张量做逐元素 ReLU）。用 `mod.get_source("ll")` 提取 LLVM IR。识别循环结构、向量化以及所用到的任何 intrinsic。

2. **用 LLVM IR 写点积：** 写一个 `.ll` 文件，用向量指令计算两个 8 元素 float 向量的点积。用 `llc` 分别为 x86-64 和 AArch64 目标编译。比较生成的汇编。

3. **探索地址空间：** 写一段 LLVM IR，使用地址空间 1（global）和地址空间 3（shared）。用 `llc -march=nvptx64` 为 NVPTX 目标编译。检查 PTX 输出——你应当看到 `ld.global` 和 `ld.shared` 指令。

4. **自定义 intrinsic 草图：** 为一个假想的加速器设计 intrinsic 签名，该加速器有一条融合的 Conv2D+ReLU 指令，作用于 3×3 INT8 patch。输入类型、输出类型和副作用分别是什么？

---

## 关键要点

| Concept | Why It Matters for AI Hardware |
|---|---|
| 静态单赋值形式 | 使优化 pass 能让生成的代码变快 |
| 类型系统 | 直接映射到硬件数据类型（INT8、FP16、BF16） |
| 向量类型 | SIMD 的 IR 表示——AI 编译器表达线程内并行性的方式 |
| 地址空间 | 对 GPU 和自定义加速器的存储层次建模 |
| intrinsic | 硬件特定操作（张量核心、MAC 单元）的逃生通道 |
| 三阶段设计 | 写一个后端 → 免费获得所有 ML 框架 |

---

## 资源

* **[LLVM Language Reference Manual](https://llvm.org/docs/LangRef.html)：** LLVM IR 的权威规范——每一种类型、指令和 intrinsic。
* **[LLVM Programmer's Manual](https://llvm.org/docs/ProgrammersManual.html)：** 如何使用 C++ API（IRBuilder、PassManager 等）。
* **"Getting Started with LLVM Core Libraries"，作者 Bruno Cardoso Lopes 和 Rafael Auler：** 在 LLVM 上构建工具的实用书籍。
* **[Mapping High-Level Constructs to LLVM IR](https://mapping-high-level-constructs-to-llvm-ir.readthedocs.io/)：** 把编程模式翻译成 IR 的社区指南。
* **TVM 源码：`src/target/llvm/codegen_llvm.cc`：** ML 编译器以编程方式生成 LLVM IR 的真实示例。


<details>
<summary>English original</summary>

**Building LLVM IR Programmatically**

This is how ML compilers (TVM, Triton, etc.) actually construct LLVM IR — not by writing text files, but by using the C++ API.

```cpp
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

// Create context and module
LLVMContext ctx;
Module mod("ai_kernel", ctx);
IRBuilder<> builder(ctx);

// Define function: void relu(float* input, float* output, int n)
Type *floatPtrTy = PointerType::get(Type::getFloatTy(ctx), 0);
Type *i32Ty = Type::getInt32Ty(ctx);
FunctionType *fnTy = FunctionType::get(
    Type::getVoidTy(ctx),
    {floatPtrTy, floatPtrTy, i32Ty},
    false
);
Function *relu = Function::Create(fnTy, Function::ExternalLinkage, "relu", mod);

// Create basic blocks
BasicBlock *entry = BasicBlock::Create(ctx, "entry", relu);
BasicBlock *loop  = BasicBlock::Create(ctx, "loop", relu);
BasicBlock *exit  = BasicBlock::Create(ctx, "exit", relu);

// Entry block: initialize loop counter
builder.SetInsertPoint(entry);
builder.CreateBr(loop);

// Loop block: load, compare, select (ReLU), store
builder.SetInsertPoint(loop);
PHINode *i = builder.CreatePHI(i32Ty, 2, "i");
i->addIncoming(ConstantInt::get(i32Ty, 0), entry);

// ... load input[i], compute max(0, val), store to output[i] ...
// ... increment i, branch back or exit ...
```

> **Key Insight:** TVM's `codegen_llvm.cc` does exactly this — it walks the TVM IR (TIR) and emits LLVM `IRBuilder` calls for each node. Understanding the IRBuilder API is essential if you're building a compiler for a custom AI accelerator. You don't need to be a compiler expert — the API is straightforward once you understand the IR structure.

---

**Hands-On Exercises**

1. **Inspect ML compiler output:** Install TVM, compile a simple model (e.g., element-wise ReLU on a 1024-element tensor) for the `llvm` target. Extract the LLVM IR with `mod.get_source("ll")`. Identify the loop structure, vectorization, and any intrinsics used.

2. **Write a dot product in LLVM IR:** Write a `.ll` file that computes the dot product of two 8-element float vectors using vector instructions. Compile with `llc` for both x86-64 and AArch64 targets. Compare the generated assembly.

3. **Explore address spaces:** Write LLVM IR that uses address space 1 (global) and address space 3 (shared). Compile for the NVPTX target with `llc -march=nvptx64`. Examine the PTX output — you should see `ld.global` and `ld.shared` instructions.

4. **Custom intrinsic sketch:** Design the intrinsic signature for a hypothetical accelerator that has a fused Conv2D+ReLU instruction operating on 3×3 INT8 patches. What are the input types, output types, and side effects?

---

**Key Takeaways**

| Concept | Why It Matters for AI Hardware |
|---|---|
| SSA form | Enables the optimization passes that make generated code fast |
| Type system | Maps directly to hardware data types (INT8, FP16, BF16) |
| Vector types | The IR representation of SIMD — how AI compilers express parallelism within a thread |
| Address spaces | Model the memory hierarchy of GPUs and custom accelerators |
| Intrinsics | The escape hatch for hardware-specific operations (tensor cores, MAC units) |
| Three-phase design | Write one backend → get all ML frameworks for free |

---

**Resources**

* **[LLVM Language Reference Manual](https://llvm.org/docs/LangRef.html):** The authoritative specification of LLVM IR — every type, instruction, and intrinsic.
* **[LLVM Programmer's Manual](https://llvm.org/docs/ProgrammersManual.html):** How to use the C++ API (IRBuilder, PassManager, etc.).
* **"Getting Started with LLVM Core Libraries" by Bruno Cardoso Lopes and Rafael Auler:** Practical book for building tools on LLVM.
* **[Mapping High-Level Constructs to LLVM IR](https://mapping-high-level-constructs-to-llvm-ir.readthedocs.io/):** Community guide for translating programming patterns to IR.
* **TVM source: `src/target/llvm/codegen_llvm.cc`:** Real-world example of an ML compiler generating LLVM IR programmatically.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track F - AI Chip Design/Lectures/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20F%20-%20AI%20Chip%20Design/Lectures/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
