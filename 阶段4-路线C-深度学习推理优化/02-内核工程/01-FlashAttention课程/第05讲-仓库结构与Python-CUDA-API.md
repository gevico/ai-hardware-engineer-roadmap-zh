---
title: 第 5 讲 — 代码仓库解剖与 Python / CUDA API
description: 第 5 讲 — 代码仓库解剖与 Python / CUDA API
---

# 第 5 讲 — 代码仓库解剖与 Python / CUDA API

**父级：** [FlashAttention Course](Guide.md)

**一句话目的：** 把 `Dao-AILab/flash-attention` 中每一个重要目录与入口点都映射出来，这样你就能把任意一次调用从 Python 一路追踪到启动的 CUDA kernel 而不迷路。

**前置要求：** 第 1–4 讲。在本地针对你的 CUDA toolkit 构建好的 `flash-attention` 克隆。

**产物：** 一份从 `flash_attn_func(...)` 一直到 CUDA kernel 启动的带注释调用 trace，外加一份能在全新 checkout 下存活的极简本地构建 recipe。

---

## 为什么重要

FA 仓库如今约有 150 个源文件，横跨 Python、C++ 和 CUDA，有多套 kernel 家族（FA1 / FA2 / FA3）、多个 API（`func`、`qkvpacked`、`varlen`、`with_kvcache`），还有一套复杂的构建流程，会按 CUDA arch 编译不同的 kernel 家族。没有地图，你要花好几个小时 grep；有了地图，第一次就能落到正确的文件。

---

## 心智模型

### 顶层布局

```
flash-attention/
├── flash_attn/                        # Python package
│   ├── flash_attn_interface.py        # the API users call
│   ├── flash_attn_triton.py           # Triton reference impl
│   ├── modules/                       # MHA / Block / Norm helpers
│   ├── ops/                           # rotary, layernorm, etc.
│   └── utils/                         # benchmarking, distributed helpers
├── csrc/
│   ├── flash_attn/                    # FA2 (Ampere/Ada) C++/CUDA
│   │   ├── flash_api.cpp              # pybind / TORCH_LIBRARY entry
│   │   └── src/
│   │       ├── flash_fwd_kernel.h     # forward kernel
│   │       ├── flash_bwd_kernel.h     # backward kernel
│   │       ├── flash_fwd_launch_template.h
│   │       ├── kernel_traits.h        # tile sizes per head_dim
│   │       ├── softmax.h              # rowmax/rowsum/rescale
│   │       └── mask.h                 # causal / sliding window / alibi
│   ├── flash_attn_hopper/             # FA3 (Hopper, sm90)
│   │   └── ...                        # WGMMA + TMA + warp-specialised
│   ├── ft_attention/                  # legacy faster-transformer attention
│   ├── layer_norm/                    # fused LayerNorm CUDA
│   ├── rotary/                        # rotary embedding CUDA
│   └── xentropy/                      # cross-entropy CUDA
├── tests/                             # pytest suites; correctness vs ref
├── benchmarks/                        # microbench scripts
└── setup.py                           # picks which kernels to compile per arch
```

你会花最多时间的两个目录是 `flash_attn/`（面向 Python 的 API）和 `csrc/flash_attn/src/`（FA2 kernel）或 `csrc/flash_attn_hopper/`（FA3 kernel）。

### 两种 API 风格

| API | 形状 | 使用场景 |
|-----|--------|-------------|
| `flash_attn_func(q, k, v, ...)` | `[B, S, H, D]` | 简单、定长的训练/评测路径 |
| `flash_attn_qkvpacked_func(qkv, ...)` | `[B, S, 3, H, D]` | QKV 融合时（为很多训练器省掉一次 transpose） |
| `flash_attn_varlen_func(q, k, v, cu_seqlens_q, cu_seqlens_k, ...)` | packed `[total_tokens, H, D]` + `[B+1]` indptr | 变长 batch（例如 vLLM 风格的 packed 序列） |
| `flash_attn_with_kvcache(...)` | decode（逐 token 生成阶段）路径：`[B, 1, H, D]` query、paged KV cache | 推理 decode 步骤 |

四种最终都在不同的 launch template 下调用同一批 kernel。

### 一次 Python 调用如何抵达 GPU

```
flash_attn_func(q, k, v, dropout_p, softmax_scale, causal=True)
  │
  ▼ flash_attn/flash_attn_interface.py
  │   flash_attn_func -> FlashAttnFunc.apply (autograd)
  │     forward(): calls torch.ops.flash_attn._flash_attn_forward(...)
  │
  ▼ torch.ops.flash_attn._flash_attn_forward
  │   bound via TORCH_LIBRARY in flash_attn/_C
  │
  ▼ csrc/flash_attn/flash_api.cpp
  │   mha_fwd(...) — argument parsing, dispatch on head_dim
  │
  ▼ csrc/flash_attn/src/flash_fwd_launch_template.h
  │   run_mha_fwd_<head_dim, is_causal, ...>() — kernel traits, grid, launch
  │
  ▼ csrc/flash_attn/src/flash_fwd_kernel.h
      flash_fwd_kernel<...><<<grid, block, smem>>>(params)
      // does the FA1/FA2 algorithm from Lecture 3
```

反向传播走的是与之平行的文件（`flash_bwd_kernel.h`、`flash_bwd_launch_template.h`）。

仓库的组织方式是：**算法**在 `flash_fwd_kernel.h`，**形状分发**在 `flash_fwd_launch_template.h`，**分块大小**在 `kernel_traits.h`，**API 表面**在 `flash_api.cpp`。心里有了这个划分，找东西就很快。


<details>
<summary>English original</summary>

**Lecture 5 — Repo Anatomy and Python / CUDA API**

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** Map every important directory and entry point in `Dao-AILab/flash-attention` so you can trace any call from Python to the launched CUDA kernel without getting lost.

**Prerequisites:** Lectures 1–4. A local clone of `flash-attention` built against your CUDA toolkit.

**Artifact:** An annotated call trace from `flash_attn_func(...)` down to the CUDA kernel launch, plus a minimal local build recipe that survives a fresh checkout.

---

**Why it matters**

The FA repo is now ~150 source files across Python, C++, and CUDA, with multiple kernel families (FA1 / FA2 / FA3), several APIs (`func`, `qkvpacked`, `varlen`, `with_kvcache`), and a complicated build that compiles different kernel families per CUDA arch. Without a map you will spend hours grepping; with a map you can land in the right file on the first try.

---

**Mental model**

**Top-level layout**

```
flash-attention/
├── flash_attn/                        # Python package
│   ├── flash_attn_interface.py        # the API users call
│   ├── flash_attn_triton.py           # Triton reference impl
│   ├── modules/                       # MHA / Block / Norm helpers
│   ├── ops/                           # rotary, layernorm, etc.
│   └── utils/                         # benchmarking, distributed helpers
├── csrc/
│   ├── flash_attn/                    # FA2 (Ampere/Ada) C++/CUDA
│   │   ├── flash_api.cpp              # pybind / TORCH_LIBRARY entry
│   │   └── src/
│   │       ├── flash_fwd_kernel.h     # forward kernel
│   │       ├── flash_bwd_kernel.h     # backward kernel
│   │       ├── flash_fwd_launch_template.h
│   │       ├── kernel_traits.h        # tile sizes per head_dim
│   │       ├── softmax.h              # rowmax/rowsum/rescale
│   │       └── mask.h                 # causal / sliding window / alibi
│   ├── flash_attn_hopper/             # FA3 (Hopper, sm90)
│   │   └── ...                        # WGMMA + TMA + warp-specialised
│   ├── ft_attention/                  # legacy faster-transformer attention
│   ├── layer_norm/                    # fused LayerNorm CUDA
│   ├── rotary/                        # rotary embedding CUDA
│   └── xentropy/                      # cross-entropy CUDA
├── tests/                             # pytest suites; correctness vs ref
├── benchmarks/                        # microbench scripts
└── setup.py                           # picks which kernels to compile per arch
```

The two directories you will spend the most time in are `flash_attn/` (the Python facing API) and `csrc/flash_attn/src/` (FA2 kernels) or `csrc/flash_attn_hopper/` (FA3 kernels).

**The two API styles**

| API | Shapes | When to use |
|-----|--------|-------------|
| `flash_attn_func(q, k, v, ...)` | `[B, S, H, D]` | The simple, fixed-length training/eval path |
| `flash_attn_qkvpacked_func(qkv, ...)` | `[B, S, 3, H, D]` | When QKV is fused (saves a transpose for many trainers) |
| `flash_attn_varlen_func(q, k, v, cu_seqlens_q, cu_seqlens_k, ...)` | packed `[total_tokens, H, D]` + `[B+1]` indptr | Variable-length batches (e.g. vLLM-style packed sequences) |
| `flash_attn_with_kvcache(...)` | decode path: `[B, 1, H, D]` query, paged KV cache | Inference decode step |

All four end up calling the same kernels under different launch templates.

**How a Python call reaches the GPU**

```
flash_attn_func(q, k, v, dropout_p, softmax_scale, causal=True)
  │
  ▼ flash_attn/flash_attn_interface.py
  │   flash_attn_func -> FlashAttnFunc.apply (autograd)
  │     forward(): calls torch.ops.flash_attn._flash_attn_forward(...)
  │
  ▼ torch.ops.flash_attn._flash_attn_forward
  │   bound via TORCH_LIBRARY in flash_attn/_C
  │
  ▼ csrc/flash_attn/flash_api.cpp
  │   mha_fwd(...) — argument parsing, dispatch on head_dim
  │
  ▼ csrc/flash_attn/src/flash_fwd_launch_template.h
  │   run_mha_fwd_<head_dim, is_causal, ...>() — kernel traits, grid, launch
  │
  ▼ csrc/flash_attn/src/flash_fwd_kernel.h
      flash_fwd_kernel<...><<<grid, block, smem>>>(params)
      // does the FA1/FA2 algorithm from Lecture 3
```

Backward goes through the parallel files (`flash_bwd_kernel.h`, `flash_bwd_launch_template.h`).

The repo is organised so that the **algorithm** lives in `flash_fwd_kernel.h`, the **shape dispatch** lives in `flash_fwd_launch_template.h`, the **tile sizes** live in `kernel_traits.h`, and the **API surface** lives in `flash_api.cpp`. Once you have that mental partition, finding things is fast.

</details>

### FA1、FA2、FA3 究竟各自位于何处

- **FA1：** 历史遗留，main 分支中已不再作为独立路径存在。FA2 代码库就是「FA1 + 更好的调度」；读 FA1 论文并与 FA2 kernel 对照，即可还原 FA1 算法。
- **FA2：** `csrc/flash_attn/`。支持 sm80/sm86/sm89/sm90 构建。
- **FA3：** `csrc/flash_attn_hopper/`。仅支持 sm90a 构建。用 CUTLASS / CuTe 实现 WGMMA + TMA。头文件布局刻意与 FA2 保持一致，便于 diff。

### 构建过程（为什么你的第一次构建会失败）

`setup.py` 按 `(head_dim, is_causal, dropout, alibi)` 枚举 kernel，并为每个组合生成一个 `.cu`。开启全部 flag 时会产生数百个 TU，编译耗时 30–60 分钟。使用 `MAX_JOBS` 和 `FLASH_ATTN_FORCE_BUILD` 环境变量；在 workstation 上，`MAX_JOBS=4` 通常是安全的。如果只关心一个 head_dim，设置 `FLASH_ATTENTION_DISABLE_BACKWARD=TRUE` 并修改 `setup.py` 只编译单个 dim——构建时间降到几分钟。

就本课程而言，可以直接用预编译的 wheel；只有打算给 kernel 打补丁时才需要 *构建*（Lecture 10）。

---

## 动手构建

### 1. 追踪一次调用

在装好 FA 的 Python REPL 中：

```python
import torch, flash_attn
print(flash_attn.__file__)
print(flash_attn.flash_attn_interface.__file__)
```

打开第二个文件。找到 `flash_attn_func`。注意它调用了 `torch.ops.flash_attn._flash_attn_forward`。该符号由 C++ 注册——找出注册位置：

```
grep -RIn "_flash_attn_forward" csrc/
```

命中的位置在 `csrc/flash_attn/flash_api.cpp`。从头到尾读 `mha_fwd` 函数：参数校验 → `set_params_fprop()` → `run_mha_fwd<...>()`。顺着 `run_mha_fwd` 调用走到 `flash_fwd_launch_template.h`。

把这条链路画成一页图。保存为 `flash_attn_call_trace.md`。

### 2. 本地最小构建

```
git clone https://github.com/Dao-AILab/flash-attention.git
cd flash-attention
pip install ninja packaging
MAX_JOBS=4 pip install --no-build-isolation -e .
```

在 16 核配 H100 的 workstation 上，首次构建约需 30–45 分钟。如果 nvcc 编译时 OOM，把 `MAX_JOBS` 降到 2。

验证：

```python
import torch
from flash_attn import flash_attn_func
q = torch.randn(2, 1024, 8, 64, device="cuda", dtype=torch.bfloat16)
o = flash_attn_func(q, q, q, causal=True)
print(o.shape, o.dtype)
```

如果 import 成功且调用有返回，构建就没问题。

### 3. 一个「hello, kernel」补丁

在 `flash_fwd_kernel.h` 的入口附近找一个 `printf`（或在由 constexpr `if (thread0()) printf(...)` 保护的 `#if 0 ... #endif` 块里加一个）。重新构建。跑步骤 2 的测试。如果看到 printf，说明你已经有了可用的 build → patch → test 循环。这正是 Lecture 10 必须具备的。

---

## 在真实技术栈中使用

三个使用这些 API 的生产案例：

- **vLLM** 用 `flash_attn_with_kvcache` 做 paged decode，用 `flash_attn_varlen_func` 做 chunked prefill。
- **TransformerEngine**（NVIDIA）在 Hopper 上通过 `flash_attn_hopper` 路径用 FA3 替换 SDPA。
- **PyTorch SDPA 的「flash」backend** 经 `torch._C._aten._scaled_dot_product_flash_attention` 绑定到 FA2。

打开其中一个，定位那处调用。对 vLLM，搜索 `vllm/attention/backends/flash_attn.py`——会看到直接使用 `flash_attn_with_kvcache` 和 `flash_attn_varlen_func`。这正是 Lecture 8 要讲的 API。

---

## 测量

使用官方 benchmark：

```
python benchmarks/benchmark_flash_attention.py --mode fwd --batch_size 2 --seqlen 4096 --nheads 16 --headdim 128
```

它会打印指定 shape 下 FA2 与 PyTorch SDPA 的时间、TFLOPs 和 HBM 带宽。跑一个小 sweep 并把 CSV 存下来；Lecture 6 中你会用这个基线来验证对 FA2 work-partitioning 的理解。

---

## 交付

在你的 `flash-attn-course/` 中：

1. `flash_attn_call_trace.md`——从 `flash_attn_func` 到 kernel 启动的带注释链路。
2. `local_build_notes.md`——确切的 `pip install` 命令行、总构建时间、遇到的报错以及如何修复。
3. 一个能展示的「hello, kernel」补丁：一句 printf、一个 no-op 的矩阵乘累加计数计数器，或一个由环境变量控制的无害分块大小覆盖。只要能证明你能修改并重新构建 kernel、并让 Python 看到变化即可。

这三项产物是后续每个 lab 的前置要求。

---

## 相关页面

- [Lecture 4 — GPU kernel 性能基础](第04讲-GPU-kernel性能基础.md)
- [Lecture 6 — FlashAttention-2](第06讲-FlashAttention-2.md)
- [Lecture 8 — 推理路径](第08讲-推理路径的KV-cache与decode.md)


<details>
<summary>English original</summary>

**Where FA1 vs FA2 vs FA3 actually live**

- **FA1:** historical, no longer in the main branch as a separate path. The FA2 codebase is "FA1 + better scheduling"; the FA1 algorithm is recoverable by reading the FA1 paper and matching it against the FA2 kernel.
- **FA2:** `csrc/flash_attn/`. Builds for sm80/sm86/sm89/sm90.
- **FA3:** `csrc/flash_attn_hopper/`. Builds only for sm90a. Uses CUTLASS / CuTe for WGMMA + TMA. Header layout intentionally mirrors FA2 so you can diff them.

**The build (why your first build will fail)**

`setup.py` enumerates kernels by `(head_dim, is_causal, dropout, alibi)` and emits one `.cu` per combination. With all flags it produces hundreds of TUs and takes 30–60 minutes to compile. Use the `MAX_JOBS` and `FLASH_ATTN_FORCE_BUILD` env vars; for a workstation, `MAX_JOBS=4` is usually safe. If you only care about one head_dim, set `FLASH_ATTENTION_DISABLE_BACKWARD=TRUE` and patch `setup.py` to compile a single dim — your build time drops to a couple of minutes.

For this course's purposes you can use the pre-built wheel; you only need to *build* if you are going to patch a kernel (Lecture 10).

---

**Build it**

**1. Trace a call**

In a Python REPL with FA installed:

```python
import torch, flash_attn
print(flash_attn.__file__)
print(flash_attn.flash_attn_interface.__file__)
```

Open the second file. Find `flash_attn_func`. Note that it calls `torch.ops.flash_attn._flash_attn_forward`. That symbol is registered from C++ — find where:

```
grep -RIn "_flash_attn_forward" csrc/
```

The hit lands in `csrc/flash_attn/flash_api.cpp`. Read the `mha_fwd` function top to bottom: argument validation → `set_params_fprop()` → `run_mha_fwd<...>()`. Follow the `run_mha_fwd` call to `flash_fwd_launch_template.h`.

Write down the chain as a single-page diagram. Save it as `flash_attn_call_trace.md`.

**2. Local minimal build**

```
git clone https://github.com/Dao-AILab/flash-attention.git
cd flash-attention
pip install ninja packaging
MAX_JOBS=4 pip install --no-build-isolation -e .
```

On a 16-core workstation with H100 this takes ~30–45 minutes the first time. If you hit OOM during nvcc compile, drop `MAX_JOBS` to 2.

Verify:

```python
import torch
from flash_attn import flash_attn_func
q = torch.randn(2, 1024, 8, 64, device="cuda", dtype=torch.bfloat16)
o = flash_attn_func(q, q, q, causal=True)
print(o.shape, o.dtype)
```

If the import works and the call returns, your build is good.

**3. A "hello, kernel" patch**

Find a `printf` near the entry of `flash_fwd_kernel.h` (or add one inside an `#if 0 ... #endif` block guarded by a constexpr `if (thread0()) printf(...)`). Rebuild. Run your test from step 2. If you see the printf, you have a working build → patch → test cycle. This is what you need to have for Lecture 10.

---

**Use it in the real stack**

Three production examples that exercise these APIs:

- **vLLM** uses `flash_attn_with_kvcache` for paged decode and `flash_attn_varlen_func` for chunked prefill.
- **TransformerEngine** (NVIDIA) replaces SDPA with FA3 on Hopper via the `flash_attn_hopper` path.
- **PyTorch SDPA's "flash" backend** is FA2 bound through `torch._C._aten._scaled_dot_product_flash_attention`.

Open one of these and locate the call. For vLLM, search `vllm/attention/backends/flash_attn.py` — you will see `flash_attn_with_kvcache` and `flash_attn_varlen_func` used directly. This is exactly the API we cover in Lecture 8.

---

**Measure it**

Use the official benchmark:

```
python benchmarks/benchmark_flash_attention.py --mode fwd --batch_size 2 --seqlen 4096 --nheads 16 --headdim 128
```

It prints time, TFLOPs, and HBM bandwidth for FA2 and PyTorch SDPA at the requested shape. Run a small sweep and save the CSV; you will use this baseline in Lecture 6 to validate your FA2 work-partitioning understanding.

---

**Ship it**

In your `flash-attn-course/`:

1. `flash_attn_call_trace.md` — your annotated chain from `flash_attn_func` to the kernel launch.
2. `local_build_notes.md` — exact `pip install` line, total build time, any errors you hit and how you fixed them.
3. A "hello, kernel" patch you can show: a printf, a no-op MMA-count counter, or a benign tile-size override gated by an env var. Anything that proves you can edit and rebuild a kernel and have Python see the change.

These three artifacts are the prerequisite for every later lab.

---

**Related pages**

- [Lecture 4 — GPU kernel performance basics](第04讲-GPU-kernel性能基础.md)
- [Lecture 6 — FlashAttention-2](第06讲-FlashAttention-2.md)
- [Lecture 8 — Inference path](第08讲-推理路径的KV-cache与decode.md)

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Lecture 05 - Repo Anatomy and Python CUDA API.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Lecture%2005%20-%20Repo%20Anatomy%20and%20Python%20CUDA%20API.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
