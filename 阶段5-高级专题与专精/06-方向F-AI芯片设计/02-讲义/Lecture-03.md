---
title: 第 3 讲：MLIR 基础 —— 多级 IR 与方言
description: 第 3 讲：MLIR 基础 —— 多级 IR 与方言
---

# 第 3 讲：MLIR 基础 —— 多级 IR 与方言

## 概述

LLVM IR 很强大，但存在一个根本性限制：它只工作在**单一抽象层级**上——大致相当于「带向量的 C」。当 ML 编译器需要推理张量运算、循环分块、数据布局或特定硬件的存储层次时，LLVM IR **过于底层**。优化器所需的信息，早就在 lowering 中被丢掉了。MLIR（Multi-Level Intermediate Representation）通过允许**多个抽象层级共存于同一份 IR 中**来解决这个问题，各层级之间由渐进式 lowering 连接。其心智模型是**一叠语言**：最顶层讨论「把两个张量做矩阵乘」；中间层讨论「对这个循环嵌套做分块，并把分块映射到 PE」；最底层发射 LLVM IR 或硬件指令。每一层都是一个**方言**，有自己的运算、类型和优化规则。对 AI 硬件工程师而言，MLIR 是把 ML 模型连接到自定义加速器后端的框架——它是 TVM、Triton（经由 TTIR/TTGIR）、IREE 以及各家硬件厂商编译器都在共同收敛的编译基础设施。

---

## 为什么 LLVM IR 对 AI 编译器来说不够用

| 问题 | LLVM IR 的局限 | MLIR 的解法 |
|---|---|---|
| 张量语义 | 没有张量类型——只有扁平数组和指针 | `tensor<128x64xf32>` 作为一等类型 |
| 循环分块决策 | 循环已被 lowering 成分支和 phi 节点 | `affine.for` 为多面体分析保留循环结构 |
| 硬件映射 | 单一扁平地址空间模型（带编号的地址空间） | 方言定义自定义存储层次（scratchpad、accumulator 等） |
| 多级优化 | 必须先把所有内容 lowering 到同一层级才能优化 | 每个方言在各自的抽象层级上优化 |
| 自定义运算 | 必须使用 intrinsic（不透明的函数调用） | 定义具备完整语义、验证和规范化的运算 |
| 可扩展性 | 新增一个概念需要修改 LLVM 核心 | 方言是模块化的——新增方言无需改动既有代码 |

**关键洞见：** 当你把 `matmul(A, B)` lowering 成 LLVM IR 循环时，就丢掉了「这是一个矩阵乘」这一信息。LLVM 的向量化器能向量化最内层循环，但无法为了缓存局部性**对循环嵌套做分块**，也无法**把它映射到脉动阵列**。MLIR 让高层语义保留得足够久，以便**硬件感知优化**能对其起作用。

---


<details>
<summary>English original</summary>

**Lecture 3: MLIR Fundamentals — Multi-Level IR & Dialects**

**Overview**

LLVM IR is powerful but it has a fundamental limitation: it operates at **one level of abstraction** — roughly "C with vectors." When an ML compiler needs to reason about tensor operations, loop tiling, data layout, or hardware-specific memory hierarchies, LLVM IR is **too low-level**. You've already lowered away the information the optimizer needs. MLIR (Multi-Level Intermediate Representation) solves this by allowing **multiple levels of abstraction to coexist in the same IR**, connected by progressive lowering. The mental model is a **stack of languages**: at the top, you talk about "matrix multiply two tensors"; in the middle, you talk about "tile this loop nest and map tiles to processing elements"; at the bottom, you emit LLVM IR or hardware instructions. Each level is a **dialect** with its own operations, types, and optimization rules. For AI hardware engineers, MLIR is the framework that connects ML models to custom accelerator backends — it is the compiler infrastructure that TVM, Triton (via TTIR/TTGIR), IREE, and hardware vendor compilers are all converging on.

---

**Why LLVM IR Is Not Enough for AI Compilers**

| Problem | LLVM IR Limitation | MLIR Solution |
|---|---|---|
| Tensor semantics | No tensor type — only flat arrays and pointers | `tensor<128x64xf32>` as a first-class type |
| Loop tiling decisions | Loops are already lowered to branches and phi nodes | `affine.for` preserves loop structure for polyhedral analysis |
| Hardware mapping | One flat address space model (with numbered spaces) | Dialects define custom memory hierarchy (scratchpad, accumulator, etc.) |
| Multi-level optimization | Must lower everything to one level before optimizing | Each dialect optimizes at its own abstraction level |
| Custom operations | Must use intrinsics (opaque function calls) | Define operations with full semantics, verification, and canonicalization |
| Extensibility | Adding a new concept requires modifying LLVM core | Dialects are modular — add new ones without touching existing code |

**The key insight:** When you lower `matmul(A, B)` to LLVM IR loops, you lose the information that this is a matrix multiply. The LLVM vectorizer can vectorize the innermost loop, but it cannot **tile the loop nest** for cache locality or **map it to a systolic array**. MLIR keeps the high-level semantics alive long enough for **hardware-aware optimizations** to act on them.

---

</details>

## MLIR 架构

```
┌──────────────────────────────────────────────────────────────────┐
│                        ML Framework                              │
│                   (PyTorch, TensorFlow, tinygrad)                │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                     High-Level Dialects                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────────┐  │
│  │  tosa     │  │ stablehlo│  │  torch   │  │  custom.myop   │  │
│  │(Tensor Op │  │(StableHLO│  │ (PyTorch │  │  (your own     │  │
│  │ Set Arch.)│  │ from XLA)│  │  ops)    │  │   dialect)     │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬─────────┘  │
│       │              │             │               │             │
│       └──────────────┼─────────────┼───────────────┘             │
│                      ▼                                           │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              Mid-Level Dialects                             │ │
│  │  ┌────────┐  ┌────────┐  ┌─────────┐  ┌─────────────────┐ │ │
│  │  │ linalg │  │ tensor │  │  arith  │  │    memref       │ │ │
│  │  │(Linear │  │(Tensor │  │(Arith-  │  │(Memory-backed   │ │ │
│  │  │Algebra)│  │ ops)   │  │ metic)  │  │ tensors)        │ │ │
│  │  └───┬────┘  └───┬────┘  └────┬────┘  └──────┬──────────┘ │ │
│  └──────┼───────────┼────────────┼───────────────┼────────────┘ │
│         │           │            │               │              │
│         └───────────┼────────────┼───────────────┘              │
│                     ▼                                           │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              Low-Level Dialects                             │ │
│  │  ┌────────┐  ┌────────┐  ┌──────────┐  ┌───────────────┐  │ │
│  │  │ affine │  │  scf   │  │   gpu    │  │   vector      │  │ │
│  │  │(Affine │  │(Struct.│  │(GPU      │  │(Hardware      │  │ │
│  │  │ loops) │  │Control │  │ mapping) │  │ vectors)      │  │ │
│  │  │        │  │ Flow)  │  │          │  │               │  │ │
│  │  └───┬────┘  └───┬────┘  └────┬─────┘  └──────┬────────┘  │ │
│  └──────┼───────────┼────────────┼───────────────┼────────────┘ │
│         └───────────┼────────────┼───────────────┘              │
│                     ▼                                           │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              Target Dialects                                │ │
│  │  ┌────────┐  ┌────────────┐  ┌───────────────────────────┐ │ │
│  │  │ llvm   │  │  nvvm /    │  │   your_accel              │ │ │
│  │  │(LLVM IR│  │  rocdl /   │  │   (custom accelerator     │ │ │
│  │  │ in MLIR│  │  spirv     │  │    instructions)          │ │ │
│  │  │ form)  │  │            │  │                           │ │ │
│  │  └───┬────┘  └────┬───────┘  └──────┬────────────────────┘ │ │
│  └──────┼────────────┼─────────────────┼──────────────────────┘ │
└─────────┼────────────┼─────────────────┼────────────────────────┘
          ▼            ▼                 ▼
    LLVM Backend    PTX / AMDGPU     Custom Assembly
```

---

## 核心概念

### 1. 操作（Operation）

**操作**是 MLIR 中计算的基本单元。IR 中的每个节点都是一个操作。操作是**完全可扩展的**——任何 dialect 都可以定义新的操作。

```mlir
// An operation has:
//   - a name (dialect.operation_name)
//   - operands (SSA values)
//   - results (SSA values)
//   - attributes (compile-time constants)
//   - regions (nested IR, for control flow)
//   - types

%result = arith.addf %a, %b : f32
//         ^^^^^^^^^^^         ^^^
//         operation name      type
//                    ^^  ^^
//                    operands

%out = linalg.matmul ins(%A, %B : tensor<128x64xf32>, tensor<64x256xf32>)
                     outs(%C : tensor<128x256xf32>) -> tensor<128x256xf32>
//     ^^^^^^^^^^^^^^
//     high-level operation: "matrix multiply"
//     carries full semantic information
```

### 2. 方言

**方言**是操作、类型与属性的命名空间。可以把它看作面向特定抽象层级或领域的迷你语言。

| 方言 | 抽象层级 | 用途 |
|---|---|---|
| `tosa` | 最高 | 标准张量操作（conv2d、matmul、relu）——硬件无关 |
| `stablehlo` | 最高 | 来自 XLA/JAX 生态的 StableHLO 操作 |
| `linalg` | 中高 | 张量上具名与通用的线性代数（matmul、conv、pooling） |
| `tensor` | 中 | 张量操作（extract_slice、insert_slice、reshape） |
| `memref` | 中 | 带显式 buffer 与 layout 的内存支撑张量 |
| `affine` | 中低 | 用于多面体优化的 affine 循环嵌套 |
| `scf` | 低 | 结构化控制流（for、while、if） |
| `vector` | 低 | 固定大小向量操作（映射到 SIMD/向量单元） |
| `gpu` | 低 | GPU 专用：thread/block 索引、barrier、共享内存 |
| `arith` | 低 | 标量与向量上的算术操作（add、mul、cmp） |
| `math` | 低 | 数学操作（exp、log、sqrt、tanh） |
| `llvm` | 最低 | 以 MLIR 语法表达的 LLVM IR 操作 |
| `nvvm` | 目标 | NVIDIA 专用：warp shuffle、tensor core、TMA |

### 3. 区域与块

操作可以包含**区域**，区域中包含操作的**块**。MLIR 正是用这种方式表示嵌套结构——循环体、函数体或 GPU kernel 体。

```mlir
// A function is an operation with a region containing blocks
func.func @relu(%input: tensor<1024xf32>) -> tensor<1024xf32> {
  // This is a block inside the function's region
  %zero = arith.constant 0.0 : f32
  %result = linalg.generic {
    indexing_maps = [affine_map<(d0) -> (d0)>, affine_map<(d0) -> (d0)>],
    iterator_types = ["parallel"]
  } ins(%input : tensor<1024xf32>)
    outs(%output : tensor<1024xf32>) {
    // This is a nested region inside linalg.generic
    ^bb0(%in: f32, %out: f32):
      %cmp = arith.cmpf ogt, %in, %zero : f32
      %relu = arith.select %cmp, %in, %zero : f32
      linalg.yield %relu : f32
  } -> tensor<1024xf32>
  return %result : tensor<1024xf32>
}
```

> **关键洞察：**区域的嵌套结构，正是 MLIR 能在单个 IR 中表示多级抽象的原因。一个 `linalg.matmul` 操作封装了计算；其区域定义了逐元素的计算体。一个 pass 可以选择对 matmul 分块、把分块映射到 GPU 线程，或者把整块 lower 成一条硬件指令——全都靠在合适的层级上变换 IR 来实现。

### 4. 类型

MLIR 的类型系统是可扩展的。每种方言都能定义自己的类型。

```mlir
// Built-in types
f16, bf16, f32, f64                           // floating point
i1, i8, i16, i32, i64                         // integer
index                                          // loop indices, sizes

// Tensor types (value semantics — no memory associated)
tensor<128x64xf32>                            // static shape
tensor<?x64xf32>                              // dynamic first dimension
tensor<*xf32>                                 // unranked (any shape)

// MemRef types (reference semantics — backed by memory)
memref<128x64xf32>                            // default layout (row-major)
memref<128x64xf32, affine_map<(d0,d1) -> (d1, d0)>>  // column-major
memref<128x64xf32, #gpu.address_space<workgroup>>     // GPU shared memory

// Vector types (fixed-size, for SIMD)
vector<8xf32>                                 // 8-element vector
vector<4x4xf32>                               // 2D vector (for matrix tiles)
```

**Tensor 与 MemRef：**这一区分是根本性的。
- `tensor` 具有值语义——就像数学中的矩阵。没有副作用。支持函数式风格的优化（融合、CSE）。
- `memref` 具有引用语义——它指向实际内存。有副作用（load/store）。代码生成必需。
- **Bufferization** 是把 `tensor` → `memref` 的 pass，它决定在哪里分配 buffer、何时复用它们。

---


<details>
<summary>English original</summary>

**2. Dialects**

A **dialect** is a namespace of operations, types, and attributes. Think of it as a mini-language for a specific abstraction level or domain.

| Dialect | Abstraction Level | Purpose |
|---|---|---|
| `tosa` | Highest | Standard tensor operations (conv2d, matmul, relu) — hardware-agnostic |
| `stablehlo` | Highest | StableHLO ops from XLA/JAX ecosystem |
| `linalg` | High-mid | Named and generic linear algebra on tensors (matmul, conv, pooling) |
| `tensor` | Mid | Tensor manipulation (extract_slice, insert_slice, reshape) |
| `memref` | Mid | Memory-backed tensors with explicit buffers and layouts |
| `affine` | Mid-low | Affine loop nests for polyhedral optimization |
| `scf` | Low | Structured control flow (for, while, if) |
| `vector` | Low | Fixed-size vector operations (maps to SIMD/vector units) |
| `gpu` | Low | GPU-specific: thread/block indexing, barriers, shared memory |
| `arith` | Low | Arithmetic operations (add, mul, cmp) on scalars and vectors |
| `math` | Low | Math operations (exp, log, sqrt, tanh) |
| `llvm` | Lowest | LLVM IR operations expressed in MLIR syntax |
| `nvvm` | Target | NVIDIA-specific: warp shuffles, tensor cores, TMA |

**3. Regions and Blocks**

Operations can contain **regions**, which contain **blocks** of operations. This is how MLIR represents nested structure — a loop body, a function body, or a GPU kernel body.

```mlir
// A function is an operation with a region containing blocks
func.func @relu(%input: tensor<1024xf32>) -> tensor<1024xf32> {
  // This is a block inside the function's region
  %zero = arith.constant 0.0 : f32
  %result = linalg.generic {
    indexing_maps = [affine_map<(d0) -> (d0)>, affine_map<(d0) -> (d0)>],
    iterator_types = ["parallel"]
  } ins(%input : tensor<1024xf32>)
    outs(%output : tensor<1024xf32>) {
    // This is a nested region inside linalg.generic
    ^bb0(%in: f32, %out: f32):
      %cmp = arith.cmpf ogt, %in, %zero : f32
      %relu = arith.select %cmp, %in, %zero : f32
      linalg.yield %relu : f32
  } -> tensor<1024xf32>
  return %result : tensor<1024xf32>
}
```

> **Key Insight:** The nested structure of regions is what enables MLIR to represent multi-level abstractions in a single IR. A `linalg.matmul` operation encapsulates the computation; its region defines the element-wise body. A pass can choose to tile the matmul, map tiles to GPU threads, or lower the whole thing to a hardware instruction — all by transforming the IR at the appropriate level.

**4. Types**

MLIR's type system is extensible. Each dialect can define its own types.

```mlir
// Built-in types
f16, bf16, f32, f64                           // floating point
i1, i8, i16, i32, i64                         // integer
index                                          // loop indices, sizes

// Tensor types (value semantics — no memory associated)
tensor<128x64xf32>                            // static shape
tensor<?x64xf32>                              // dynamic first dimension
tensor<*xf32>                                 // unranked (any shape)

// MemRef types (reference semantics — backed by memory)
memref<128x64xf32>                            // default layout (row-major)
memref<128x64xf32, affine_map<(d0,d1) -> (d1, d0)>>  // column-major
memref<128x64xf32, #gpu.address_space<workgroup>>     // GPU shared memory

// Vector types (fixed-size, for SIMD)
vector<8xf32>                                 // 8-element vector
vector<4x4xf32>                               // 2D vector (for matrix tiles)
```

**Tensor vs. MemRef:** This distinction is fundamental.
- `tensor` has value semantics — like a mathematical matrix. No side effects. Enables functional-style optimizations (fusion, CSE).
- `memref` has reference semantics — it points to actual memory. Has side effects (loads/stores). Required for code generation.
- **Bufferization** is the pass that converts `tensor` → `memref`, deciding where to allocate buffers and when to reuse them.

---

</details>

## 渐进式 Lowering

MLIR 的核心原则：**不要一次性 lower 所有内容**。一次只 lower 一层，**在每一层做优化**。

```
tosa.conv2d                           ← "convolution on tensors"
       │
       ▼  (tosa-to-linalg)
linalg.conv_2d_nhwc_hwcf              ← "conv as loop nest over tensors"
       │
       ▼  (linalg tiling)
linalg.conv (tiled to 4x4)            ← "tiled conv with explicit tile sizes"
       │
       ▼  (linalg-to-loops)
scf.for / affine.for                  ← "explicit loop nest"
       │
       ▼  (loop vectorization)
vector.contract / vector.fma          ← "vector operations on tiles"
       │
       ▼  (bufferization)
memref.load / memref.store            ← "explicit memory operations"
       │
       ▼  (convert-to-llvm)
llvm.load / llvm.store / llvm.call    ← "LLVM IR operations"
       │
       ▼  (mlir-translate)
LLVM IR                               ← "standard LLVM IR"
       │
       ▼  (llc)
Native code                           ← "machine instructions"
```

每个箭头都是一个 **lowering pass**，负责把操作从一个方言转换到另一个方言。在每一层，都可以运行该方言专属的优化 pass：

| 层级 | 可用的优化 |
|---|---|
| `linalg` | 分块、相邻 op 融合、交换（循环重排序） |
| `affine` | 多面体优化、依赖分析、循环倾斜 |
| `scf` | 循环展开、流水线化、剥离 |
| `vector` | 向量分发、transfer 读写优化 |
| `gpu` | 线程/block 映射、共享内存提升 |

> **关键洞见：** MLIR 之所以优于「把所有内容都 lower 成 LLVM IR 再在那里优化」，是因为每一层都保留了更低层会丢失的信息。在 `linalg` 层，你知道这是矩阵乘——可以针对脉动阵列做分块。在 `affine` 层，你知道循环边界是外层索引的仿射函数——可以应用多面体优化。一旦到了 LLVM IR，就只剩循环和 load——优化器能向量化、能展开，但无法重新分块，也无法重新映射到空间架构。

---

## Pass 与变换

MLIR pass 的工作方式与 LLVM pass 类似，但作用于 MLIR 操作。

### Pass 类型

```cpp
// Operation pass: runs on a specific operation type (e.g., FuncOp)
struct MyTilingPass : public PassWrapper<MyTilingPass, OperationPass<func::FuncOp>> {
  void runOnOperation() override {
    func::FuncOp func = getOperation();
    // Walk all linalg operations and tile them
    func.walk([](linalg::MatmulOp op) {
      // Tile with tile sizes [32, 32, 16]
      linalg::tileUsingForOp(op, {32, 32, 16});
    });
  }
};

// Module pass: runs on the entire module
struct MyBufferizationPass : public PassWrapper<MyBufferizationPass, OperationPass<ModuleOp>> {
  // ...
};
```

### 规范化

每个方言都可以注册 **规范化模式**——这些化简规则应用起来永远是正确的：

```mlir
// Before canonicalization:
%x = arith.addf %a, %zero : f32     // adding zero

// After canonicalization:
// %x is replaced with %a (the add is eliminated)
```

```mlir
// Before: redundant tensor.cast
%t1 = tensor.cast %input : tensor<128xf32> to tensor<?xf32>
%t2 = tensor.cast %t1 : tensor<?xf32> to tensor<128xf32>
// After: both casts eliminated, %t2 → %input
```

### 方言转换框架

从一个方言 lower 到另一个方言时，MLIR 提供了一套结构化的框架：

```cpp
// Define which operations to convert
struct MatmulToLoopsPattern : public OpConversionPattern<linalg::MatmulOp> {
  LogicalResult matchAndRewrite(
      linalg::MatmulOp op,
      OpAdaptor adaptor,
      ConversionPatternRewriter &rewriter) const override {
    // Replace linalg.matmul with nested scf.for loops
    auto loc = op.getLoc();
    auto zero = rewriter.create<arith.ConstantIndexOp>(loc, 0);
    // ... build loop nest ...
    rewriter.replaceOp(op, result);
    return success();
  }
};
```

---


<details>
<summary>English original</summary>

**Progressive Lowering**

The core principle of MLIR: **don't lower everything at once**. Lower one level at a time, **optimizing at each level**.

```
tosa.conv2d                           ← "convolution on tensors"
       │
       ▼  (tosa-to-linalg)
linalg.conv_2d_nhwc_hwcf              ← "conv as loop nest over tensors"
       │
       ▼  (linalg tiling)
linalg.conv (tiled to 4x4)            ← "tiled conv with explicit tile sizes"
       │
       ▼  (linalg-to-loops)
scf.for / affine.for                  ← "explicit loop nest"
       │
       ▼  (loop vectorization)
vector.contract / vector.fma          ← "vector operations on tiles"
       │
       ▼  (bufferization)
memref.load / memref.store            ← "explicit memory operations"
       │
       ▼  (convert-to-llvm)
llvm.load / llvm.store / llvm.call    ← "LLVM IR operations"
       │
       ▼  (mlir-translate)
LLVM IR                               ← "standard LLVM IR"
       │
       ▼  (llc)
Native code                           ← "machine instructions"
```

Each arrow is a **lowering pass** that converts operations from one dialect to another. At each level, optimization passes specific to that dialect can run:

| Level | Optimizations Available |
|---|---|
| `linalg` | Tiling, fusion of adjacent ops, interchange (loop reordering) |
| `affine` | Polyhedral optimization, dependence analysis, loop skewing |
| `scf` | Loop unrolling, pipelining, peeling |
| `vector` | Vector distribution, transfer read/write optimization |
| `gpu` | Thread/block mapping, shared memory promotion |

> **Key Insight:** The reason MLIR outperforms "lower everything to LLVM IR and optimize there" is that each level retains information that lower levels lose. At the `linalg` level, you know it's a matrix multiply — you can tile it for a systolic array. At the `affine` level, you know the loop bounds are affine functions of outer indices — you can apply polyhedral optimization. Once it's in LLVM IR, it's just loops and loads — the optimizer can vectorize and unroll but cannot re-tile or re-map to a spatial architecture.

---

**Passes and Transformations**

MLIR passes work similarly to LLVM passes but operate on MLIR operations.

**Pass Types**

```cpp
// Operation pass: runs on a specific operation type (e.g., FuncOp)
struct MyTilingPass : public PassWrapper<MyTilingPass, OperationPass<func::FuncOp>> {
  void runOnOperation() override {
    func::FuncOp func = getOperation();
    // Walk all linalg operations and tile them
    func.walk([](linalg::MatmulOp op) {
      // Tile with tile sizes [32, 32, 16]
      linalg::tileUsingForOp(op, {32, 32, 16});
    });
  }
};

// Module pass: runs on the entire module
struct MyBufferizationPass : public PassWrapper<MyBufferizationPass, OperationPass<ModuleOp>> {
  // ...
};
```

**Canonicalization**

Every dialect can register **canonicalization patterns** — simplification rules that are always correct to apply:

```mlir
// Before canonicalization:
%x = arith.addf %a, %zero : f32     // adding zero

// After canonicalization:
// %x is replaced with %a (the add is eliminated)
```

```mlir
// Before: redundant tensor.cast
%t1 = tensor.cast %input : tensor<128xf32> to tensor<?xf32>
%t2 = tensor.cast %t1 : tensor<?xf32> to tensor<128xf32>
// After: both casts eliminated, %t2 → %input
```

**Dialect Conversion Framework**

When lowering from one dialect to another, MLIR provides a structured framework:

```cpp
// Define which operations to convert
struct MatmulToLoopsPattern : public OpConversionPattern<linalg::MatmulOp> {
  LogicalResult matchAndRewrite(
      linalg::MatmulOp op,
      OpAdaptor adaptor,
      ConversionPatternRewriter &rewriter) const override {
    // Replace linalg.matmul with nested scf.for loops
    auto loc = op.getLoc();
    auto zero = rewriter.create<arith.ConstantIndexOp>(loc, 0);
    // ... build loop nest ...
    rewriter.replaceOp(op, result);
    return success();
  }
};
```

---

</details>

## 定义自定义方言

对于自定义 AI 加速器，你要定义自己的方言，其中的 operation 直接映射到你的**硬件指令**。

```tablegen
// MyAccel.td — TableGen dialect definition

def MyAccel_Dialect : Dialect {
  let name = "myaccel";
  let summary = "Dialect for MyAccel AI accelerator";
  let cppNamespace = "::mlir::myaccel";
}

// Define a matrix multiply operation for the accelerator
def MyAccel_MatMulOp : Op<MyAccel_Dialect, "matmul", [Pure]> {
  let summary = "Accelerator matrix multiply (8x8 INT8 tile)";
  let arguments = (ins
    MemRefOf<[I8]>:$lhs,       // left input tile in accelerator SRAM
    MemRefOf<[I8]>:$rhs,       // right input tile in accelerator SRAM
    MemRefOf<[I32]>:$acc       // accumulator in register file
  );
  let results = (outs MemRefOf<[I32]>:$result);

  let assemblyFormat = [{
    `(` $lhs `,` $rhs `,` $acc `)` attr-dict `:` type($result)
  }];
}

// Define a DMA transfer operation
def MyAccel_DMAOp : Op<MyAccel_Dialect, "dma_transfer", []> {
  let summary = "Transfer data between main memory and accelerator SRAM";
  let arguments = (ins
    MemRefOf<[AnyType]>:$src,
    MemRefOf<[AnyType]>:$dst,
    Index:$size
  );
}
```

针对你的加速器的 lowering 流水线会是：
```
linalg.matmul → tiling to 8×8 tiles → myaccel.dma_transfer + myaccel.matmul
```

---

## MLIR 工具

```bash
# Parse and verify MLIR
mlir-opt input.mlir

# Run specific passes
mlir-opt --linalg-tile="tile-sizes=32,32,16" input.mlir
mlir-opt --convert-linalg-to-loops input.mlir
mlir-opt --convert-scf-to-cf --convert-to-llvm input.mlir

# Full lowering pipeline
mlir-opt input.mlir \
  --linalg-tile="tile-sizes=32,32,16" \
  --convert-linalg-to-loops \
  --lower-affine \
  --convert-scf-to-cf \
  --convert-to-llvm \
  -o lowered.mlir

# Translate to LLVM IR
mlir-translate --mlir-to-llvmir lowered.mlir -o output.ll

# Then compile with LLVM
llc output.ll -o output.o -filetype=obj
```

---

## 动手练习

1. **阅读 MLIR 输出：** 安装 MLIR（随 LLVM 构建一起提供）。用 MLIR 文本格式写一个简单的 `linalg.matmul`。运行 `mlir-opt --convert-linalg-to-loops`，观察生成的 `scf.for` 循环嵌套。再运行 `--convert-scf-to-cf --convert-to-llvm`，观察 LLVM 方言输出。

2. **渐进式 lowering：** 从一个 `tosa.conv2d` operation 开始。让它依次经过 `tosa-to-linalg` → `linalg-tile` → `linalg-to-loops` → `convert-to-llvm` 进行 lower。在每个阶段打印 IR，观察信息如何被保留、又如何被消耗。

3. **Tensor 与 MemRef：** 写一个函数，接收 `tensor<16x16xf32>` 输入，执行逐元素加法，返回一个 tensor。运行 `--one-shot-bufferize`，观察 tensor 如何变成带有显式 `memref.alloc` 和 `memref.dealloc` 的 memref。

4. **方言设计练习：** 为一个假想的 NPU 设计（在纸上）一个 MLIR 方言，该 NPU 具有：16×16 INT8 MAC 阵列、64KB 权重 SRAM、32KB 激活值 SRAM，以及用于 host↔SRAM 传输的 DMA。定义 operation、类型和内存空间。画出从 `linalg.matmul` 到你的方言的 lowering 草图。

---

## 关键要点

| 概念 | 为什么它对 AI 硬件重要 |
|---|---|
| 多级 IR | 保留高层语义，以便进行硬件感知优化 |
| 方言 | 模块化、可扩展——无需 fork MLIR 即可添加你的加速器的 op |
| 渐进式 lowering | 在每一级做优化；不要过早丢弃信息 |
| Tensor 与 MemRef | 将算法（tensor）与内存管理（memref）分离 |
| Region | 支持嵌套结构：kernel、循环体、流水线级 |
| 自定义方言 | 把你的硬件接入 ML 框架的机制 |

---

## 资源

* **[MLIR Language Reference](https://mlir.llvm.org/docs/LangRef/)：** MLIR 语法与语义的权威规范。
* **[MLIR Dialects Documentation](https://mlir.llvm.org/docs/Dialects/)：** 所有内置方言（linalg、affine、scf、gpu、vector 等）的参考。
* **"MLIR: Scaling Compiler Infrastructure for Domain-Specific Computation"（CGO 2021）：** Lattner 等人的奠基性论文。
* **[MLIR Tutorial](https://mlir.llvm.org/docs/Tutorials/Toy/)：** 官方的 Toy 语言教程——用 MLIR 从零构建一个完整的编译器。
* **[MLIR Open Design Meetings（YouTube）](https://www.youtube.com/channel/UCMQl4dniSlBiEFPXl9n5ueg)：** MLIR 设计讨论的录像，涵盖真实用例。


<details>
<summary>English original</summary>

**Defining a Custom Dialect**

For a custom AI accelerator, you define your own dialect with operations that map directly to your **hardware instructions**.

```tablegen
// MyAccel.td — TableGen dialect definition

def MyAccel_Dialect : Dialect {
  let name = "myaccel";
  let summary = "Dialect for MyAccel AI accelerator";
  let cppNamespace = "::mlir::myaccel";
}

// Define a matrix multiply operation for the accelerator
def MyAccel_MatMulOp : Op<MyAccel_Dialect, "matmul", [Pure]> {
  let summary = "Accelerator matrix multiply (8x8 INT8 tile)";
  let arguments = (ins
    MemRefOf<[I8]>:$lhs,       // left input tile in accelerator SRAM
    MemRefOf<[I8]>:$rhs,       // right input tile in accelerator SRAM
    MemRefOf<[I32]>:$acc       // accumulator in register file
  );
  let results = (outs MemRefOf<[I32]>:$result);

  let assemblyFormat = [{
    `(` $lhs `,` $rhs `,` $acc `)` attr-dict `:` type($result)
  }];
}

// Define a DMA transfer operation
def MyAccel_DMAOp : Op<MyAccel_Dialect, "dma_transfer", []> {
  let summary = "Transfer data between main memory and accelerator SRAM";
  let arguments = (ins
    MemRefOf<[AnyType]>:$src,
    MemRefOf<[AnyType]>:$dst,
    Index:$size
  );
}
```

The lowering pipeline for your accelerator would be:
```
linalg.matmul → tiling to 8×8 tiles → myaccel.dma_transfer + myaccel.matmul
```

---

**MLIR Tools**

```bash
# Parse and verify MLIR
mlir-opt input.mlir

# Run specific passes
mlir-opt --linalg-tile="tile-sizes=32,32,16" input.mlir
mlir-opt --convert-linalg-to-loops input.mlir
mlir-opt --convert-scf-to-cf --convert-to-llvm input.mlir

# Full lowering pipeline
mlir-opt input.mlir \
  --linalg-tile="tile-sizes=32,32,16" \
  --convert-linalg-to-loops \
  --lower-affine \
  --convert-scf-to-cf \
  --convert-to-llvm \
  -o lowered.mlir

# Translate to LLVM IR
mlir-translate --mlir-to-llvmir lowered.mlir -o output.ll

# Then compile with LLVM
llc output.ll -o output.o -filetype=obj
```

---

**Hands-On Exercises**

1. **Read MLIR output:** Install MLIR (comes with LLVM build). Write a simple `linalg.matmul` in MLIR text format. Run `mlir-opt --convert-linalg-to-loops` and observe the generated `scf.for` loop nest. Then run `--convert-scf-to-cf --convert-to-llvm` and observe the LLVM dialect output.

2. **Progressive lowering:** Start with a `tosa.conv2d` operation. Lower it through `tosa-to-linalg` → `linalg-tile` → `linalg-to-loops` → `convert-to-llvm`. At each stage, print the IR and observe how information is preserved then consumed.

3. **Tensor vs MemRef:** Write a function that takes `tensor<16x16xf32>` inputs, performs an element-wise add, and returns a tensor. Run `--one-shot-bufferize` and observe how tensors become memrefs with explicit `memref.alloc` and `memref.dealloc`.

4. **Dialect design exercise:** Design (on paper) an MLIR dialect for a hypothetical NPU with: a 16×16 INT8 MAC array, 64KB weight SRAM, 32KB activation SRAM, and DMA for host↔SRAM transfers. Define the operations, types, and memory spaces. Sketch the lowering from `linalg.matmul` to your dialect.

---

**Key Takeaways**

| Concept | Why It Matters for AI Hardware |
|---|---|
| Multi-level IR | Preserve high-level semantics for hardware-aware optimization |
| Dialects | Modular, extensible — add your accelerator's ops without forking MLIR |
| Progressive lowering | Optimize at each level; don't prematurely discard information |
| Tensor vs MemRef | Separate algorithm (tensor) from memory management (memref) |
| Regions | Enable nested structure: kernels, loop bodies, pipeline stages |
| Custom dialects | The mechanism for connecting your hardware to ML frameworks |

---

**Resources**

* **[MLIR Language Reference](https://mlir.llvm.org/docs/LangRef/):** The authoritative specification of MLIR syntax and semantics.
* **[MLIR Dialects Documentation](https://mlir.llvm.org/docs/Dialects/):** Reference for all built-in dialects (linalg, affine, scf, gpu, vector, etc.).
* **"MLIR: Scaling Compiler Infrastructure for Domain-Specific Computation" (CGO 2021):** The foundational paper by Lattner et al.
* **[MLIR Tutorial](https://mlir.llvm.org/docs/Tutorials/Toy/):** The official Toy language tutorial — builds a full compiler using MLIR from scratch.
* **[MLIR Open Design Meetings (YouTube)](https://www.youtube.com/channel/UCMQl4dniSlBiEFPXl9n5ueg):** Recordings of MLIR design discussions covering real-world use cases.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track F - AI Chip Design/Lectures/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20F%20-%20AI%20Chip%20Design/Lectures/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
