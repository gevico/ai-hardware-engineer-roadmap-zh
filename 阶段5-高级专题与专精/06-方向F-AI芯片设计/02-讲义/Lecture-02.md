---
title: Lecture 2：LLVM Pass 与硬件后端的代码生成
description: Lecture 2：LLVM Pass 与硬件后端的代码生成
---

# Lecture 2：LLVM Pass 与硬件后端的代码生成

## 概述

Lecture 1 确立了 LLVM IR 是前端与后端之间的通用语言。本讲回答接下来的问题：该 IR 在输入与输出之间经历了什么？答案是 **pass**——分析、优化并最终将 IR 降低为机器码的模块化变换。核心挑战在于理解：以 LLVM IR 中简单循环形式表达的矩阵乘法，如何变成一串使用最优寄存器、且没有多余内存访问的向量 FMA 指令。心智模型是一条 **变换流水线**：每个 pass 读取 IR，做出一项具体改进，再把结果交给下一个 pass。对 AI 硬件工程师而言，这直接相关，因为（1）为自定义加速器编写后端时，需要实现目标相关的 pass；（2）理解优化器能做与不能做的事，能告诉你哪些必须由硬件在硅上处理，哪些可由编译器在软件中解决。

---

## Pass 流水线

LLVM 把 pass 组织成按特定顺序运行的流水线。`opt` 工具在 LLVM IR 上运行中端 pass；`llc` 运行后端（代码生成）pass。

```
LLVM IR (.ll / .bc)
    │
    ▼
┌────────────────────────────────────────────┐
│          Middle-End Passes (opt)           │
│                                            │
│  Analysis:  dominator tree, alias analysis │
│  Transform: SROA, GVN, LICM, vectorize    │
│  Cleanup:   DCE, SimplifyCFG              │
└────────────────────┬───────────────────────┘
                     │
                     ▼  (optimized LLVM IR)
┌────────────────────────────────────────────┐
│          Backend Passes (llc)              │
│                                            │
│  Instruction Selection (DAG → MachineInstr)│
│  Register Allocation                       │
│  Instruction Scheduling                    │
│  Machine-specific optimizations            │
│  Code Emission (assembly / object)         │
└────────────────────────────────────────────┘
                     │
                     ▼
              Native Code (.o / .ptx / .s)
```

---

## 中端优化 pass

这些 pass 作用于 LLVM IR，且**目标无关**——无论最终由哪款芯片运行，它们都会改进代码。

### Pass 类别

| 类别 | 目的 | 关键 pass |
|---|---|---|
| **标量** | 优化单个值与表达式 | SROA, GVN, SCCP, InstCombine |
| **循环** | 优化循环（AI kernel 的关键路径） | LICM, IndVarSimplify, LoopUnroll, LoopVectorize |
| **过程间** | 跨函数边界优化 | Inlining, DeadArgElim, GlobalOpt |
| **向量化** | 将标量代码转为向量操作 | LoopVectorize, SLPVectorize |
| **清理** | 移除其他 pass 之后的冗余代码 | DCE, SimplifyCFG, MergedLoadStoreMotion |


<details>
<summary>English original</summary>

**Lecture 2: LLVM Passes & Code Generation for Hardware Backends**

**Overview**

Lecture 1 established that LLVM IR is the universal language between frontends and backends. This lecture addresses the next question: what happens to that IR between input and output? The answer is **passes** — modular transformations that analyze, optimize, and ultimately lower the IR into machine code. The core challenge is understanding how a matrix multiply expressed as simple loops in LLVM IR becomes a sequence of vector FMA instructions with optimal register usage and no unnecessary memory traffic. The mental model is a **pipeline of transformations**: each pass reads the IR, makes a specific improvement, and hands the result to the next pass. For AI hardware engineers, this is directly relevant because (1) when you write a backend for a custom accelerator, you need to implement target-specific passes, and (2) understanding what the optimizer can and cannot do tells you what your hardware must handle in silicon vs. what the compiler can fix in software.

---

**The Pass Pipeline**

LLVM organizes passes into a pipeline that runs in a specific order. The `opt` tool runs middle-end passes on LLVM IR; `llc` runs the backend (code generation) passes.

```
LLVM IR (.ll / .bc)
    │
    ▼
┌────────────────────────────────────────────┐
│          Middle-End Passes (opt)           │
│                                            │
│  Analysis:  dominator tree, alias analysis │
│  Transform: SROA, GVN, LICM, vectorize    │
│  Cleanup:   DCE, SimplifyCFG              │
└────────────────────┬───────────────────────┘
                     │
                     ▼  (optimized LLVM IR)
┌────────────────────────────────────────────┐
│          Backend Passes (llc)              │
│                                            │
│  Instruction Selection (DAG → MachineInstr)│
│  Register Allocation                       │
│  Instruction Scheduling                    │
│  Machine-specific optimizations            │
│  Code Emission (assembly / object)         │
└────────────────────────────────────────────┘
                     │
                     ▼
              Native Code (.o / .ptx / .s)
```

---

**Middle-End Optimization Passes**

These passes work on LLVM IR and are **target-independent** — they improve the code regardless of which chip will run it.

**Pass Categories**

| Category | Purpose | Key Passes |
|---|---|---|
| **Scalar** | Optimize individual values and expressions | SROA, GVN, SCCP, InstCombine |
| **Loop** | Optimize loops (the critical path for AI kernels) | LICM, IndVarSimplify, LoopUnroll, LoopVectorize |
| **Interprocedural** | Optimize across function boundaries | Inlining, DeadArgElim, GlobalOpt |
| **Vectorization** | Convert scalar code to vector operations | LoopVectorize, SLPVectorize |
| **Cleanup** | Remove redundant code after other passes | DCE, SimplifyCFG, MergedLoadStoreMotion |

</details>

### 对 AI 工作负载至关重要的 pass

**1. Loop Vectorization (LoopVectorize)**

这是 CPU 上 AI kernel 性能**最重要的单个 pass**。它把标量循环转换成使用 SIMD 硬件的**向量操作**。

```llvm
; BEFORE vectorization — processes one element at a time
loop:
  %i = phi i64 [ 0, %entry ], [ %i.next, %loop ]
  %a_ptr = getelementptr float, ptr %A, i64 %i
  %b_ptr = getelementptr float, ptr %B, i64 %i
  %a = load float, ptr %a_ptr
  %b = load float, ptr %b_ptr
  %mul = fmul float %a, %b
  %c_ptr = getelementptr float, ptr %C, i64 %i
  store float %mul, ptr %c_ptr
  %i.next = add i64 %i, 1
  %cmp = icmp slt i64 %i.next, %n
  br i1 %cmp, label %loop, label %exit

; AFTER vectorization (VF=8 on AVX-256) — processes 8 elements at a time
loop.vec:
  %i = phi i64 [ 0, %entry ], [ %i.next, %loop.vec ]
  %a_ptr = getelementptr float, ptr %A, i64 %i
  %b_ptr = getelementptr float, ptr %B, i64 %i
  %va = load <8 x float>, ptr %a_ptr, align 32
  %vb = load <8 x float>, ptr %b_ptr, align 32
  %vmul = fmul <8 x float> %va, %vb
  %c_ptr = getelementptr float, ptr %C, i64 %i
  store <8 x float> %vmul, ptr %c_ptr, align 32
  %i.next = add i64 %i, 8
  %cmp = icmp slt i64 %i.next, %n
  br i1 %cmp, label %loop.vec, label %exit
```

向量化器需要证明各次迭代是**独立的**（不存在循环携带依赖），并且内存访问**不互为别名**。这正是 `restrict` 指针与对齐标注对 AI kernel 性能如此重要的原因。

**2. Loop Unrolling (LoopUnroll)**

降低循环开销，并向硬件调度器暴露更多指令级并行（ILP）。

```llvm
; BEFORE: tight loop with branch every iteration
; AFTER (unroll factor 4): 4 iterations inlined, branch every 4th
loop.unrolled:
  %v0 = load <8 x float>, ptr %p0     ; iteration 0
  %v1 = load <8 x float>, ptr %p1     ; iteration 1
  %v2 = load <8 x float>, ptr %p2     ; iteration 2
  %v3 = load <8 x float>, ptr %p3     ; iteration 3
  ; ... compute on all four ...
  ; single branch back to loop header
```

对 AI kernel 而言，展开对于保持流水线满载至关重要——尤其是在 GPU 上，每个 warp 都需要足够多的独立指令来隐藏内存延迟。

**3. Inlining**

用函数体替换函数调用。对 AI 编译器至关重要，因为小型辅助函数（激活函数、量化 scale）在被内联后成本降为零。

```llvm
; BEFORE: call overhead for every element
define float @relu(float %x) {
  %cmp = fcmp ogt float %x, 0.0
  %r = select i1 %cmp, float %x, float 0.0
  ret float %r
}

; AFTER inlining: relu body is directly in the loop
; No call/ret overhead, enables further vectorization
```

**4. Global Value Numbering (GVN)**

消除冗余计算。如果两个表达式计算出相同的值，GVN 保留其中一个，并把另一个替换为对它的引用。

```llvm
; BEFORE: redundant address computation
%ptr1 = getelementptr float, ptr %base, i64 %idx
%val1 = load float, ptr %ptr1
; ... some code that doesn't modify *ptr1 ...
%ptr2 = getelementptr float, ptr %base, i64 %idx   ; same as ptr1!
%val2 = load float, ptr %ptr2                        ; same as val1!

; AFTER GVN: second load eliminated
%ptr1 = getelementptr float, ptr %base, i64 %idx
%val1 = load float, ptr %ptr1
; ... uses %val1 instead of %val2 ...
```

**5. Loop-Invariant Code Motion (LICM)**

把在循环各次迭代之间不发生变化的计算移到循环之外。

```llvm
; BEFORE: scale factor recomputed every iteration (expensive division)
loop:
  %scale = fdiv float 1.0, %max_val    ; loop-invariant!
  %val = load float, ptr %p
  %scaled = fmul float %val, %scale
  ; ...

; AFTER LICM: division moved before the loop
preheader:
  %scale = fdiv float 1.0, %max_val    ; computed once
  br label %loop
loop:
  %val = load float, ptr %p
  %scaled = fmul float %val, %scale    ; uses precomputed scale
```

---


<details>
<summary>English original</summary>

**Passes Critical for AI Workloads**

**1. Loop Vectorization (LoopVectorize)**

This is the **single most important pass** for AI kernel performance on CPUs. It transforms scalar loops into **vector operations** that use SIMD hardware.

```llvm
; BEFORE vectorization — processes one element at a time
loop:
  %i = phi i64 [ 0, %entry ], [ %i.next, %loop ]
  %a_ptr = getelementptr float, ptr %A, i64 %i
  %b_ptr = getelementptr float, ptr %B, i64 %i
  %a = load float, ptr %a_ptr
  %b = load float, ptr %b_ptr
  %mul = fmul float %a, %b
  %c_ptr = getelementptr float, ptr %C, i64 %i
  store float %mul, ptr %c_ptr
  %i.next = add i64 %i, 1
  %cmp = icmp slt i64 %i.next, %n
  br i1 %cmp, label %loop, label %exit

; AFTER vectorization (VF=8 on AVX-256) — processes 8 elements at a time
loop.vec:
  %i = phi i64 [ 0, %entry ], [ %i.next, %loop.vec ]
  %a_ptr = getelementptr float, ptr %A, i64 %i
  %b_ptr = getelementptr float, ptr %B, i64 %i
  %va = load <8 x float>, ptr %a_ptr, align 32
  %vb = load <8 x float>, ptr %b_ptr, align 32
  %vmul = fmul <8 x float> %va, %vb
  %c_ptr = getelementptr float, ptr %C, i64 %i
  store <8 x float> %vmul, ptr %c_ptr, align 32
  %i.next = add i64 %i, 8
  %cmp = icmp slt i64 %i.next, %n
  br i1 %cmp, label %loop.vec, label %exit
```

The vectorizer needs to prove that iterations are **independent** (no loop-carried dependencies) and that memory accesses **don't alias**. This is why `restrict` pointers and alignment annotations matter so much for AI kernel performance.

**2. Loop Unrolling (LoopUnroll)**

Reduces loop overhead and exposes more instruction-level parallelism (ILP) to the hardware scheduler.

```llvm
; BEFORE: tight loop with branch every iteration
; AFTER (unroll factor 4): 4 iterations inlined, branch every 4th
loop.unrolled:
  %v0 = load <8 x float>, ptr %p0     ; iteration 0
  %v1 = load <8 x float>, ptr %p1     ; iteration 1
  %v2 = load <8 x float>, ptr %p2     ; iteration 2
  %v3 = load <8 x float>, ptr %p3     ; iteration 3
  ; ... compute on all four ...
  ; single branch back to loop header
```

For AI kernels, unrolling is essential to keep the pipeline full — especially on GPUs where each warp needs enough independent instructions to hide memory latency.

**3. Inlining**

Replaces a function call with the function body. Critical for AI compilers because small helper functions (activation functions, quantization scales) become zero-cost when inlined.

```llvm
; BEFORE: call overhead for every element
define float @relu(float %x) {
  %cmp = fcmp ogt float %x, 0.0
  %r = select i1 %cmp, float %x, float 0.0
  ret float %r
}

; AFTER inlining: relu body is directly in the loop
; No call/ret overhead, enables further vectorization
```

**4. Global Value Numbering (GVN)**

Eliminates redundant computations. If two expressions compute the same value, GVN keeps one and replaces the other with a reference.

```llvm
; BEFORE: redundant address computation
%ptr1 = getelementptr float, ptr %base, i64 %idx
%val1 = load float, ptr %ptr1
; ... some code that doesn't modify *ptr1 ...
%ptr2 = getelementptr float, ptr %base, i64 %idx   ; same as ptr1!
%val2 = load float, ptr %ptr2                        ; same as val1!

; AFTER GVN: second load eliminated
%ptr1 = getelementptr float, ptr %base, i64 %idx
%val1 = load float, ptr %ptr1
; ... uses %val1 instead of %val2 ...
```

**5. Loop-Invariant Code Motion (LICM)**

Moves computations that don't change across loop iterations out of the loop.

```llvm
; BEFORE: scale factor recomputed every iteration (expensive division)
loop:
  %scale = fdiv float 1.0, %max_val    ; loop-invariant!
  %val = load float, ptr %p
  %scaled = fmul float %val, %scale
  ; ...

; AFTER LICM: division moved before the loop
preheader:
  %scale = fdiv float 1.0, %max_val    ; computed once
  br label %loop
loop:
  %val = load float, ptr %p
  %scaled = fmul float %val, %scale    ; uses precomputed scale
```

---

</details>

## 别名分析：优化的守门人

许多关键优化（向量化、加载消除、代码移动）都要求编译器能证明**两个指针不指向同一块内存**。这就是**别名分析**。

```llvm
; Can the compiler vectorize this loop?
define void @add(ptr %A, ptr %B, ptr %C, i64 %n) {
  ; If A == C (output aliases input), vectorization changes semantics!
  ; The compiler must prove A != C or insert runtime checks
}
```

**为什么这对 AI 编译器很重要：** TVM 和 Triton 会用 `noalias` 元数据标注它们的缓冲区，因为它们知道张量缓冲区不会重叠。没有这一点，LLVM 会保守地拒绝对大多数张量操作做向量化。

```llvm
; TVM-generated IR includes noalias:
define void @fused_relu(ptr noalias %input, ptr noalias %output, i64 %n) {
  ; Compiler can freely vectorize — buffers guaranteed not to overlap
}
```

> **关键洞察：** 在构建 AI 编译器时，对代码质量影响最大的一件事就是提供准确的别名信息。只要漏掉一个 `noalias` 标注，就可能阻止整个 kernel 的向量化，在 CPU 上造成 4–16× 的性能回退。

---

## 后端：从 LLVM IR 到机器码

后端（代码生成器）把 LLVM IR 转换为面向特定目标的**原生指令**。为定制芯片构建编译器时，AI 硬件工程师**大部分时间**都花在这里。

### 后端流水线

```
LLVM IR
    │
    ▼
┌─────────────────────────────────┐
│ 1. Instruction Selection        │  IR → SelectionDAG → MachineDAG
│    (pattern matching)           │  "Which hardware instruction
│                                 │   implements this IR operation?"
├─────────────────────────────────┤
│ 2. Instruction Scheduling       │  Reorder to minimize pipeline
│    (pre-register-allocation)    │  stalls and maximize ILP
├─────────────────────────────────┤
│ 3. Register Allocation          │  Map virtual registers to
│    (graph coloring / linear scan)│  physical registers
├─────────────────────────────────┤
│ 4. Instruction Scheduling       │  Post-RA scheduling: account for
│    (post-register-allocation)   │  actual register constraints
├─────────────────────────────────┤
│ 5. Machine-Specific Passes      │  Peephole optimization, branch
│                                 │  relaxation, constant pooling
├─────────────────────────────────┤
│ 6. Code Emission                │  Encode to assembly (.s) or
│    (MCInst → bytes)             │  machine code (.o)
└─────────────────────────────────┘
```

### 阶段 1：指令选择（ISel）

ISel 使用 **TableGen** 模式匹配，把 LLVM IR 模式映射为目标特定的机器指令。

**TableGen** 是 LLVM 用于描述目标架构的领域特定语言。你编写 `.td` 文件来声明寄存器、指令和模式：

```tablegen
// Define a register class for 32 general-purpose registers
def GPR : RegisterClass<"MyAccel", [i32, f32], 32,
                         (sequence "R%u", 0, 31)>;

// Define an instruction: MAC r1, r2, r3 → r1 = r1 + r2 * r3
def MAC : Instruction {
  let OutOperandList = (outs GPR:$dst);
  let InOperandList = (ins GPR:$acc, GPR:$src1, GPR:$src2);
  let AsmString = "mac $dst, $acc, $src1, $src2";

  // Pattern: match fmuladd intrinsic → emit MAC instruction
  let Pattern = [(set GPR:$dst,
                   (fma GPR:$src1, GPR:$src2, GPR:$acc))];
}
```

当 LLVM 在 SelectionDAG 中看到 `fma` 节点时，它会匹配该模式并发射一条 `MAC` 指令。你的定制加速器的硬件操作就是这样接入 LLVM IR 的。

### 阶段 2：指令调度

调度器对指令重新排序，以最大化执行单元的利用率并隐藏延迟。它使用一个描述硬件流水线的**调度模型**。

```tablegen
// Define pipeline stages for a simple accelerator
def MyPipeline : SchedMachineModel {
  let IssueWidth = 2;           // 2 instructions per cycle
  let LoadLatency = 3;          // loads take 3 cycles
  let MispredictPenalty = 10;   // branch mispredict cost
}

// MAC instruction uses the multiply unit for 2 cycles
def : WriteRes<WriteFMul, [MulUnit]> { let Latency = 2; }
```

调度器利用该模型把加载与计算交错起来——当一个 MAC 正在执行时，下一次迭代的加载可以处于 in-flight 状态。这就是编译器层面的**软件流水线**，对保持加速器数据通路忙碌至关重要。


<details>
<summary>English original</summary>

**Alias Analysis: The Gatekeeper of Optimization**

Many critical optimizations (vectorization, load elimination, code motion) require the compiler to prove that **two pointers don't refer to the same memory**. This is **alias analysis**.

```llvm
; Can the compiler vectorize this loop?
define void @add(ptr %A, ptr %B, ptr %C, i64 %n) {
  ; If A == C (output aliases input), vectorization changes semantics!
  ; The compiler must prove A != C or insert runtime checks
}
```

**Why this matters for AI compilers:** TVM and Triton annotate their buffers with `noalias` metadata because they know tensor buffers don't overlap. Without this, LLVM would conservatively refuse to vectorize most tensor operations.

```llvm
; TVM-generated IR includes noalias:
define void @fused_relu(ptr noalias %input, ptr noalias %output, i64 %n) {
  ; Compiler can freely vectorize — buffers guaranteed not to overlap
}
```

> **Key Insight:** When building an AI compiler, the most impactful thing you can do for code quality is provide accurate alias information. A single missing `noalias` annotation can prevent vectorization of an entire kernel, causing a 4–16× performance regression on CPUs.

---

**The Backend: From LLVM IR to Machine Code**

The backend (code generator) transforms LLVM IR into **native instructions** for a specific target. This is where AI hardware engineers spend **most of their time** when building a compiler for a custom chip.

**Backend Pipeline**

```
LLVM IR
    │
    ▼
┌─────────────────────────────────┐
│ 1. Instruction Selection        │  IR → SelectionDAG → MachineDAG
│    (pattern matching)           │  "Which hardware instruction
│                                 │   implements this IR operation?"
├─────────────────────────────────┤
│ 2. Instruction Scheduling       │  Reorder to minimize pipeline
│    (pre-register-allocation)    │  stalls and maximize ILP
├─────────────────────────────────┤
│ 3. Register Allocation          │  Map virtual registers to
│    (graph coloring / linear scan)│  physical registers
├─────────────────────────────────┤
│ 4. Instruction Scheduling       │  Post-RA scheduling: account for
│    (post-register-allocation)   │  actual register constraints
├─────────────────────────────────┤
│ 5. Machine-Specific Passes      │  Peephole optimization, branch
│                                 │  relaxation, constant pooling
├─────────────────────────────────┤
│ 6. Code Emission                │  Encode to assembly (.s) or
│    (MCInst → bytes)             │  machine code (.o)
└─────────────────────────────────┘
```

**Stage 1: Instruction Selection (ISel)**

ISel maps LLVM IR patterns to target-specific machine instructions using **TableGen** pattern matching.

**TableGen** is LLVM's domain-specific language for describing target architectures. You write `.td` files that declare registers, instructions, and patterns:

```tablegen
// Define a register class for 32 general-purpose registers
def GPR : RegisterClass<"MyAccel", [i32, f32], 32,
                         (sequence "R%u", 0, 31)>;

// Define an instruction: MAC r1, r2, r3 → r1 = r1 + r2 * r3
def MAC : Instruction {
  let OutOperandList = (outs GPR:$dst);
  let InOperandList = (ins GPR:$acc, GPR:$src1, GPR:$src2);
  let AsmString = "mac $dst, $acc, $src1, $src2";

  // Pattern: match fmuladd intrinsic → emit MAC instruction
  let Pattern = [(set GPR:$dst,
                   (fma GPR:$src1, GPR:$src2, GPR:$acc))];
}
```

When LLVM sees an `fma` node in the SelectionDAG, it matches the pattern and emits a `MAC` instruction. This is how your custom accelerator's hardware operations get connected to LLVM IR.

**Stage 2: Instruction Scheduling**

The scheduler reorders instructions to maximize utilization of execution units and hide latency. It uses a **scheduling model** that describes your hardware's pipeline.

```tablegen
// Define pipeline stages for a simple accelerator
def MyPipeline : SchedMachineModel {
  let IssueWidth = 2;           // 2 instructions per cycle
  let LoadLatency = 3;          // loads take 3 cycles
  let MispredictPenalty = 10;   // branch mispredict cost
}

// MAC instruction uses the multiply unit for 2 cycles
def : WriteRes<WriteFMul, [MulUnit]> { let Latency = 2; }
```

The scheduler uses this model to interleave loads with computes — while one MAC is executing, the next iteration's load can be in-flight. This is the compiler equivalent of **software pipelining**, essential for keeping accelerator datapaths busy.

</details>

### Stage 3：寄存器分配

把无限的虚拟寄存器映射到有限的物理寄存器堆。对于拥有大寄存器堆的 AI 加速器（GPU 每个 SM 有 65536 个寄存器），**寄存器压力往往是瓶颈** —— 寄存器用尽会迫使数值**溢出到内存**，这对性能是灾难性的。

| 算法 | 速度 | 代码质量 | 使用场景 |
|---|---|---|---|
| 线性扫描 | 快 | 好 | JIT 编译、-O1 |
| 图着色（贪心） | 中等 | 最佳 | 提前编译、-O2/-O3 |
| PBQP | 慢 | 对不规则架构最佳 | 特殊目标 |

> **关键洞见：** 对 AI 加速器而言，寄存器分配质量直接决定 kernel 性能。能装进寄存器的矩阵乘分块以计算速度运行；发生溢出的则以内存速度运行。这就是 GPU 编程如此在意“occupancy”的原因 —— 它本质上关乎寄存器压力。设计加速器的寄存器堆大小时，要在代表性 AI kernel 上模拟寄存器分配，以在面积与溢出频率之间找到合适的折衷。

---

## 编写自定义后端：最小步骤

要让 LLVM 针对你的自定义 AI 加速器，需要提供以下组件：

```
llvm/lib/Target/MyAccel/
├── MyAccelTargetMachine.cpp      # Entry point: creates the target
├── MyAccelInstrInfo.td           # TableGen: instruction definitions
├── MyAccelRegisterInfo.td        # TableGen: register file description
├── MyAccelInstrInfo.cpp          # Instruction semantics, legalization
├── MyAccelISelDAGToDAG.cpp       # Custom instruction selection patterns
├── MyAccelFrameLowering.cpp      # Stack frame layout
├── MyAccelAsmPrinter.cpp         # Emit assembly text
└── MyAccelSubtarget.td           # TableGen: CPU variants, features
```

**最小可用后端工作流：**

1. **注册目标：** `TargetRegistry::RegisterTarget(TheMyAccelTarget, "myaccel", ...)`
2. **定义寄存器：** 在 TableGen 中列出你的寄存器堆
3. **定义指令：** 把 LLVM IR 操作映射到你的硬件指令
4. **处理合法化：** 告诉 LLVM 哪些类型和操作是你的硬件原生支持的，哪些必须展开（例如“我的芯片没有 64 位除法 —— 展开为库函数调用”）
5. **实现 AsmPrinter：** 为你的芯片汇编器输出文本汇编

```cpp
// Legalization example: "my accelerator only supports i8 and i32 multiply"
void MyAccelTargetLowering::setOperationAction() {
  // i8 multiply: native hardware instruction
  setOperationAction(ISD::MUL, MVT::i8, Legal);

  // i32 multiply: native
  setOperationAction(ISD::MUL, MVT::i32, Legal);

  // i16 multiply: promote to i32, then multiply
  setOperationAction(ISD::MUL, MVT::i16, Promote);

  // i64 multiply: expand to two i32 multiplies + adds
  setOperationAction(ISD::MUL, MVT::i64, Expand);

  // f32 fused multiply-add: native (our MAC unit)
  setOperationAction(ISD::FMA, MVT::f32, Legal);
}
```

---

## 案例研究：NVPTX 后端（CUDA）

NVPTX 后端是 LLVM 编译到 NVIDIA GPU 的方式 —— 也是 Triton 生成 GPU 代码的范本。

**关键设计决策：**
- 输出 **PTX**（汇编）而非二进制 SASS —— 由 NVIDIA 的 `ptxas` 负责最终的指令调度与寄存器分配
- 把 LLVM 地址空间映射到 GPU 内存：0→generic、1→global、3→shared、4→constant
- 为线程索引（`threadIdx.x`）、屏障（`__syncthreads`）、warp shuffle、Tensor Core 操作定义 intrinsic
- 后端无需寄存器分配 —— PTX 使用虚拟寄存器；由 `ptxas` 完成物理寄存器分配

```
LLVM IR → NVPTX Backend → PTX assembly → ptxas → SASS binary (cubin)
```

**Triton 做了什么：**
```
Triton Python → Triton IR → LLVM IR (NVPTX) → PTX → ptxas → cubin
                                                  ↑
                                    LLVM handles this step
```

这就是为什么理解 LLVM 的后端流水线，对于理解 Triton 这类 AI 编译器实际如何工作至关重要。

---

## 查看 pass 流水线

```bash
# See all passes that -O2 runs
opt -O2 -print-pipeline-passes input.ll -o /dev/null

# Run specific passes and inspect the result
opt -passes="loop-vectorize,loop-unroll" input.ll -S -o output.ll

# See the backend pipeline for a specific target
llc -O2 -debug-pass=Structure input.ll -o /dev/null 2>&1 | head -50

# View SelectionDAG (instruction selection) for debugging
llc -view-isel-dags input.ll    # generates a .dot graph

# View the final machine instructions before emission
llc -print-after-all input.ll -o /dev/null 2>&1
```

---


<details>
<summary>English original</summary>

**Stage 3: Register Allocation**

Maps unlimited virtual registers to the finite physical register file. For AI accelerators with large register files (GPUs have 65536 registers per SM), **register pressure is often the bottleneck** — running out of registers forces values to **spill to memory**, which is catastrophic for performance.

| Algorithm | Speed | Code Quality | Used When |
|---|---|---|---|
| Linear scan | Fast | Good | JIT compilation, -O1 |
| Graph coloring (Greedy) | Moderate | Best | Ahead-of-time, -O2/-O3 |
| PBQP | Slow | Best for irregular architectures | Special targets |

> **Key Insight:** For AI accelerators, register allocation quality directly determines kernel performance. A matrix multiply tile that fits in registers runs at compute speed; one that spills runs at memory speed. This is why GPU programming cares so much about "occupancy" — it's really about register pressure. When designing your accelerator's register file size, simulate the register allocation on representative AI kernels to find the right trade-off between area and spill frequency.

---

**Writing a Custom Backend: The Minimal Steps**

To make LLVM target your custom AI accelerator, you need to provide these components:

```
llvm/lib/Target/MyAccel/
├── MyAccelTargetMachine.cpp      # Entry point: creates the target
├── MyAccelInstrInfo.td           # TableGen: instruction definitions
├── MyAccelRegisterInfo.td        # TableGen: register file description
├── MyAccelInstrInfo.cpp          # Instruction semantics, legalization
├── MyAccelISelDAGToDAG.cpp       # Custom instruction selection patterns
├── MyAccelFrameLowering.cpp      # Stack frame layout
├── MyAccelAsmPrinter.cpp         # Emit assembly text
└── MyAccelSubtarget.td           # TableGen: CPU variants, features
```

**Minimum viable backend workflow:**

1. **Register the target:** `TargetRegistry::RegisterTarget(TheMyAccelTarget, "myaccel", ...)`
2. **Define registers:** List your register file in TableGen
3. **Define instructions:** Map LLVM IR operations to your hardware instructions
4. **Handle legalization:** Tell LLVM which types and operations your hardware supports natively vs. which must be expanded (e.g., "my chip has no 64-bit divide — expand to a library call")
5. **Implement AsmPrinter:** Emit the text assembly for your chip's assembler

```cpp
// Legalization example: "my accelerator only supports i8 and i32 multiply"
void MyAccelTargetLowering::setOperationAction() {
  // i8 multiply: native hardware instruction
  setOperationAction(ISD::MUL, MVT::i8, Legal);

  // i32 multiply: native
  setOperationAction(ISD::MUL, MVT::i32, Legal);

  // i16 multiply: promote to i32, then multiply
  setOperationAction(ISD::MUL, MVT::i16, Promote);

  // i64 multiply: expand to two i32 multiplies + adds
  setOperationAction(ISD::MUL, MVT::i64, Expand);

  // f32 fused multiply-add: native (our MAC unit)
  setOperationAction(ISD::FMA, MVT::f32, Legal);
}
```

---

**Case Study: The NVPTX Backend (CUDA)**

The NVPTX backend is how LLVM compiles to NVIDIA GPUs — and the model for how Triton generates GPU code.

**Key design decisions:**
- Emits **PTX** (assembly) rather than binary SASS — NVIDIA's `ptxas` handles final instruction scheduling and register allocation
- Maps LLVM address spaces to GPU memory: 0→generic, 1→global, 3→shared, 4→constant
- Defines intrinsics for thread indexing (`threadIdx.x`), barriers (`__syncthreads`), warp shuffles, tensor core operations
- No register allocation needed in the backend — PTX uses virtual registers; `ptxas` does physical register allocation

```
LLVM IR → NVPTX Backend → PTX assembly → ptxas → SASS binary (cubin)
```

**What Triton does:**
```
Triton Python → Triton IR → LLVM IR (NVPTX) → PTX → ptxas → cubin
                                                  ↑
                                    LLVM handles this step
```

This is why understanding LLVM's backend pipeline is essential for understanding how AI compilers like Triton actually work.

---

**Viewing the Pass Pipeline**

```bash
# See all passes that -O2 runs
opt -O2 -print-pipeline-passes input.ll -o /dev/null

# Run specific passes and inspect the result
opt -passes="loop-vectorize,loop-unroll" input.ll -S -o output.ll

# See the backend pipeline for a specific target
llc -O2 -debug-pass=Structure input.ll -o /dev/null 2>&1 | head -50

# View SelectionDAG (instruction selection) for debugging
llc -view-isel-dags input.ll    # generates a .dot graph

# View the final machine instructions before emission
llc -print-after-all input.ll -o /dev/null 2>&1
```

---

</details>

## 动手练习

1. **观察向量化：** 用 C 写一个简单的逐元素 ReLU 循环。用 `clang -S -emit-llvm -O2` 编译，查看生成的 LLVM IR。再用 `clang -S -O2 -march=skylake` 编译，查看 x86 汇编——找到 `vmaxps` 或 `vblendvps` 指令。用 `-march=armv8-a+simd` 对 NEON 重复一遍。

2. **用别名破坏向量化：** 取出该 ReLU kernel，从指针参数中移除 `restrict` 限定符。重新编译，观察向量化器要么放弃，要么插入代价高昂的 runtime 别名检查。在 `.ll` 文件中加入 `noalias` 元数据，验证向量化恢复。

3. **探索 NVPTX 后端：** 写一个简单的 LLVM IR 函数，从地址空间 1（global）加载，乘以一个常数，再存回。用 `llc -march=nvptx64 -mcpu=sm_80` 编译。阅读 PTX 输出——识别 `ld.global`、`mul.f32` 和 `st.global` 指令。

4. **后端骨架：** 参照 LLVM 文档，为一个假想的加速器勾勒一份 TableGen 文件，包含：16 个通用寄存器（R0–R15）、`LOAD`、`STORE`、`ADD`、`MUL` 以及 `MAC`（融合乘加）指令。为 `MAC` 定义模式以匹配 `fadd(fmul(a, b), c)`。

---

## 要点总结

| Concept | Why It Matters for AI Hardware |
|---|---|
| 循环向量化 | 把标量 AI kernel 转为 SIMD——CPU 上提速 4–16× |
| 别名分析 | 没有 `noalias`，向量化就会失败——代码质量中最大的单一影响因素 |
| 指令选择 | 你的自定义指令如何被 LLVM 使用 |
| TableGen | 向 LLVM 描述你的芯片的声明式语言 |
| 寄存器分配 | 决定你的 kernel 跑在计算速度还是访存速度上 |
| 调度模型 | 告诉 LLVM 如何为你的流水线交错排布指令 |
| NVPTX 后端 | 一个与 AI 相关的后端如何工作的参考范例 |

---

## 资源

* **[Writing an LLVM Backend](https://llvm.org/docs/WritingAnLLVMBackend.html)：** 实现新目标的官方指南。
* **[LLVM TableGen Reference](https://llvm.org/docs/TableGen/)：** `.td` 文件的语法与语义。
* **[NVPTX Backend Documentation](https://llvm.org/docs/NVPTXUsage.html)：** LLVM 如何面向 NVIDIA GPU。
* **[LLVM's Analysis and Transform Passes](https://llvm.org/docs/Passes.html)：** 所有内置 pass 的参考。
* **"Engineering a Compiler" by Cooper & Torczon：** 深入讲解指令选择、寄存器分配和调度的教科书。
* **Triton 源码：`python/triton/backends/nvidia/compiler.py`：** Triton 如何调用 LLVM NVPTX 后端。


<details>
<summary>English original</summary>

**Hands-On Exercises**

1. **Observe vectorization:** Write a simple element-wise ReLU loop in C. Compile with `clang -S -emit-llvm -O2` and examine the generated LLVM IR. Then compile with `clang -S -O2 -march=skylake` and examine the x86 assembly — find the `vmaxps` or `vblendvps` instructions. Repeat with `-march=armv8-a+simd` for NEON.

2. **Kill vectorization with aliasing:** Take the ReLU kernel and remove the `restrict` qualifier from the pointer parameters. Recompile and observe that the vectorizer either gives up or inserts expensive runtime alias checks. Add `noalias` metadata in the `.ll` file and verify vectorization returns.

3. **Explore the NVPTX backend:** Write a simple LLVM IR function that loads from address space 1 (global), multiplies by a constant, and stores back. Compile with `llc -march=nvptx64 -mcpu=sm_80`. Read the PTX output — identify `ld.global`, `mul.f32`, and `st.global` instructions.

4. **Backend skeleton:** Using LLVM's documentation, sketch a TableGen file for a hypothetical accelerator with: 16 general-purpose registers (R0–R15), `LOAD`, `STORE`, `ADD`, `MUL`, and `MAC` (fused multiply-add) instructions. Define the pattern for `MAC` to match `fadd(fmul(a, b), c)`.

---

**Key Takeaways**

| Concept | Why It Matters for AI Hardware |
|---|---|
| Loop vectorization | Converts scalar AI kernels to SIMD — 4–16× speedup on CPUs |
| Alias analysis | Without `noalias`, vectorization fails — the single biggest code quality factor |
| Instruction selection | How your custom instructions get used by LLVM |
| TableGen | The declarative language for describing your chip to LLVM |
| Register allocation | Determines whether your kernel runs at compute or memory speed |
| Scheduling model | Tells LLVM how to interleave instructions for your pipeline |
| NVPTX backend | The reference for how an AI-relevant backend works |

---

**Resources**

* **[Writing an LLVM Backend](https://llvm.org/docs/WritingAnLLVMBackend.html):** Official guide for implementing a new target.
* **[LLVM TableGen Reference](https://llvm.org/docs/TableGen/):** Syntax and semantics of `.td` files.
* **[NVPTX Backend Documentation](https://llvm.org/docs/NVPTXUsage.html):** How LLVM targets NVIDIA GPUs.
* **[LLVM's Analysis and Transform Passes](https://llvm.org/docs/Passes.html):** Reference for all built-in passes.
* **"Engineering a Compiler" by Cooper & Torczon:** Textbook covering instruction selection, register allocation, and scheduling in depth.
* **Triton source: `python/triton/backends/nvidia/compiler.py`:** How Triton invokes the LLVM NVPTX backend.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track F - AI Chip Design/Lectures/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20F%20-%20AI%20Chip%20Design/Lectures/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
