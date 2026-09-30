---
title: Tinygrad
description: Tinygrad
---

# Tinygrad

属于 [AI Hardware Engineer Roadmap](../../../README.md) 的一部分。在 **可改造层面** 学习 tinygrad —— 理解编译器、IR、惰性求值，以及如何扩展它。被 [Openpilot](https://github.com/commaai/openpilot) 用于 Snapdragon 上的神经网络推理。

## 快速开始

```bash
pip install tinygrad numpy
python3 projects/00_intro.py
```

上游源码请克隆 [tinygrad](https://github.com/tinygrad/tinygrad)。

## 结构

```
tinygrad/
├── Guide.md          — Full learning guide (11 parts, ~730 lines)
├── notes/
│   ├── overview.md   — Philosophy, features, real-world usage
│   └── internals.md  — Hacking the compiler, IR, schedule, backends
├── ops/
│   ├── README.md     — Operations overview and composition reference
│   ├── elementwise.md — UnaryOps, BinaryOps, TernaryOps (16 primitives)
│   ├── reduce.md     — ReduceOps: SUM, MAX, derived ops
│   └── movement.md   — MovementOps: zero-copy via ShapeTracker
└── projects/
    ├── README.md     — Setup, debug env vars, project descriptions
    ├── 00_intro.py   — Run first: lazy eval, fusion, ShapeTracker demo
    ├── 01_tensor_basics.py    — Tensor API, lazy eval, op fusion
    ├── 02_op_types.py         — The 3 op types, manual matmul
    ├── 03_autograd.py         — Autograd, gradient checking, XOR MLP
    ├── 04_mnist.py            — Full CNN training (target: >98%)
    ├── 05_compiler_pipeline.py — Schedule, UOp IR, algebraic rewrites
    ├── 06_custom_ops.py       — GELU, LayerNorm, attention, loss fns
    └── 07_custom_backend.py   — Allocator + Compiler + Runner from scratch
```

## Tinygrad 为何易改造

| 概念 | 含义 |
|---------|--------------|
| **惰性求值** | 各 op 构建出计算图；不到 `.realize()` 不执行任何东西 |
| **3 种 op 类型** | ElementwiseOps、ReduceOps、MovementOps 组合出一切 |
| **没有 CONV/MATMUL** | 由原语构建：`RESHAPE + EXPAND + MUL + SUM` |
| **UOp IR** | 计算图在 Python 中直接暴露 —— 可读可改 |
| **Kernel 融合** | 连续的 elementwise op 编译成单个 GPU kernel |
| **ShapeTracker** | Reshape/permute/expand 是零拷贝 —— 只动元数据 |
| **全部用 Python** | 编译器、IR、代码生成 —— 没有隐藏的 C++/CUDA |

## 调试环境变量

| 命令 | 你能看到什么 |
|---------|-------------|
| `DEBUG=1 python3 script.py` | 每个 `.realize()` 的 kernel 数量与时序 |
| `DEBUG=2 python3 script.py` | kernel 名称与输出形状 |
| `DEBUG=3 python3 script.py` | 生成的 kernel 源码（C/CUDA/MSL） |
| `DEBUG=4 python3 script.py` | 每个优化阶段的完整 UOp IR |
| `VIZ=1 python3 script.py` | 基于浏览器的计算图可视化工具 |
| `BEAM=2 python3 script.py` | BEAM search kernel 自动调优 |
| `NOOPT=1 python3 script.py` | 禁用代数重写（基线） |
| `CLANG=1 python3 script.py` | 强制使用 CPU/Clang 后端（可读的 C） |

## 学习路径

| 步骤 | 资源 | 时间 |
|------|----------|------|
| 0 | 运行 `projects/00_intro.py` | 15 min |
| 1 | 阅读 `notes/overview.md` | 30 min |
| 2 | 阅读 `ops/README.md` + op 指南 | 1–2h |
| 3 | 阅读 `Guide.md`（完整理论） | 2–3h |
| 4 | 做完 `projects/01–07` | ~38h |
| 5 | 阅读 `notes/internals.md` + tinygrad 源码 | 持续进行 |

## 资源

- [tinygrad GitHub](https://github.com/tinygrad/tinygrad)
- [Discord](https://discord.gg/tinygrad)
- [abstractions.py](https://github.com/tinygrad/tinygrad/blob/master/docs/abstractions.py) —— 带注释的架构走读
- [Community notes](https://mesozoic-egg.github.io/tinygrad-notes/) —— JIT、ShapeTracker、BEAM、模式匹配器


<details>
<summary>English original</summary>

**Tinygrad**

Part of the [AI Hardware Engineer Roadmap](../../../README.md). Learn tinygrad at the **hackable level** — understand the compiler, IR, lazy evaluation, and how to extend it. Used by [Openpilot](https://github.com/commaai/openpilot) for neural network inference on Snapdragon.

**Quick Start**

```bash
pip install tinygrad numpy
python3 projects/00_intro.py
```

For upstream source code, clone [tinygrad](https://github.com/tinygrad/tinygrad).

**Structure**

```
tinygrad/
├── Guide.md          — Full learning guide (11 parts, ~730 lines)
├── notes/
│   ├── overview.md   — Philosophy, features, real-world usage
│   └── internals.md  — Hacking the compiler, IR, schedule, backends
├── ops/
│   ├── README.md     — Operations overview and composition reference
│   ├── elementwise.md — UnaryOps, BinaryOps, TernaryOps (16 primitives)
│   ├── reduce.md     — ReduceOps: SUM, MAX, derived ops
│   └── movement.md   — MovementOps: zero-copy via ShapeTracker
└── projects/
    ├── README.md     — Setup, debug env vars, project descriptions
    ├── 00_intro.py   — Run first: lazy eval, fusion, ShapeTracker demo
    ├── 01_tensor_basics.py    — Tensor API, lazy eval, op fusion
    ├── 02_op_types.py         — The 3 op types, manual matmul
    ├── 03_autograd.py         — Autograd, gradient checking, XOR MLP
    ├── 04_mnist.py            — Full CNN training (target: >98%)
    ├── 05_compiler_pipeline.py — Schedule, UOp IR, algebraic rewrites
    ├── 06_custom_ops.py       — GELU, LayerNorm, attention, loss fns
    └── 07_custom_backend.py   — Allocator + Compiler + Runner from scratch
```

**What Makes Tinygrad Hackable**

| Concept | What It Means |
|---------|--------------|
| **Lazy evaluation** | Ops build a graph; nothing executes until `.realize()` |
| **3 op types** | ElementwiseOps, ReduceOps, MovementOps compose everything |
| **No CONV/MATMUL** | Built from primitives: `RESHAPE + EXPAND + MUL + SUM` |
| **UOp IR** | The computation graph is exposed in Python — read and modify it |
| **Kernel fusion** | Consecutive elementwise ops compile to a single GPU kernel |
| **ShapeTracker** | Reshape/permute/expand are zero-copy — metadata only |
| **All in Python** | Compiler, IR, codegen — no hidden C++/CUDA |

**Debug Environment Variables**

| Command | What You See |
|---------|-------------|
| `DEBUG=1 python3 script.py` | Kernel count and timing per `.realize()` |
| `DEBUG=2 python3 script.py` | Kernel names and output shapes |
| `DEBUG=3 python3 script.py` | Generated kernel source (C/CUDA/MSL) |
| `DEBUG=4 python3 script.py` | Full UOp IR at every optimization stage |
| `VIZ=1 python3 script.py` | Browser-based computation graph visualizer |
| `BEAM=2 python3 script.py` | BEAM search kernel auto-tuning |
| `NOOPT=1 python3 script.py` | Disable algebraic rewrites (baseline) |
| `CLANG=1 python3 script.py` | Force CPU/Clang backend (readable C) |

**Learning Path**

| Step | Resource | Time |
|------|----------|------|
| 0 | Run `projects/00_intro.py` | 15 min |
| 1 | Read `notes/overview.md` | 30 min |
| 2 | Read `ops/README.md` + op guides | 1–2h |
| 3 | Read `Guide.md` (full theory) | 2–3h |
| 4 | Work through `projects/01–07` | ~38h |
| 5 | Read `notes/internals.md` + tinygrad source | ongoing |

**Resources**

- [tinygrad GitHub](https://github.com/tinygrad/tinygrad)
- [Discord](https://discord.gg/tinygrad)
- [abstractions.py](https://github.com/tinygrad/tinygrad/blob/master/docs/abstractions.py) — annotated architecture walkthrough
- [Community notes](https://mesozoic-egg.github.io/tinygrad-notes/) — JIT, ShapeTracker, BEAM, pattern matcher

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/3. tinygrad for Inference/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/3.%20tinygrad%20for%20Inference/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
