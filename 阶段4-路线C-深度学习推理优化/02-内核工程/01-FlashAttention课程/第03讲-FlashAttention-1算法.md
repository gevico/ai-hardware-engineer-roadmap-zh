---
title: Lecture 3 — FlashAttention-1 算法
description: Lecture 3 — FlashAttention-1 算法
---

# Lecture 3 — FlashAttention-1 算法

**父级：** [FlashAttention 课程](Guide.md)

**一句话目的：** 走一遍 FA1 的前向与反向算法，解释其 IO 复杂度上界，并把论文中的分块与源码中的变量对应起来。

**前置要求：** Lecture 1 和 Lecture 2。应当能够推导出朴素 attention 的 IO，并跑通 blockwise-softmax 的 NumPy 验证。

**产物：** 一份伪代码走读，外加一个 HBM 字节计数器，可对任意 `(N, d, B_r, B_c)` 运行，并把论文的 `O(N²d²/M)` 上界复现到相差一个常数以内。

---

## 为什么重要

FA1 是第一个把 attention 从 `O(N²)` 带宽的操作变成 `O(N²d²/M)` 带宽的操作的 kernel，其中 `M` 是片上内存（SRAM）。正是这一改动让 32k–1M 上下文的训练变得可负担。理解了 FA1，FA2 和 FA3 就只是「同一件事，调度得更好」。

---

## 心智模型

### 对 Q、K、V 分块

把矩阵切成块：

- 把 `Q` 切成 `T_r = ceil(N / B_r)` 个大小为 `B_r × d` 的行块。
- 把 `K` 和 `V` 切成 `T_c = ceil(N / B_c)` 个大小为 `B_c × d` 的列块。

每个块必须连同运行中的暂存区一起塞进 SRAM。对 Ampere/Hopper 来说，这大约意味着 `B_r · d + B_c · d + B_r · B_c` 个字。

### 前向循环（FA1 论文中的 Algorithm 1）

```
allocate O ∈ [N, d], LSE ∈ [N], all zero / -inf
for i = 0 .. T_r - 1:                           # outer: over query tiles
    Q_i = load Q rows [i·B_r : (i+1)·B_r]       # into SRAM
    m_i = -inf vector of length B_r              # running max
    ℓ_i =  0 vector of length B_r                # running sum-exp
    O_i =  0 matrix [B_r, d]                     # running output
    for j = 0 .. T_c - 1:                       # inner: over KV tiles
        K_j, V_j = load tile [j·B_c : (j+1)·B_c]
        S_ij = (Q_i @ K_j.T) / sqrt(d)          # SRAM only, [B_r, B_c]
        m_ij = rowmax(S_ij)                      # in-register
        P_ij = exp(S_ij - m_ij)                  # local probs
        ℓ_ij = rowsum(P_ij)
        m_new = max(m_i, m_ij)
        rescale_old = exp(m_i  - m_new)
        rescale_new = exp(m_ij - m_new)
        O_i = rescale_old * O_i + rescale_new * (P_ij @ V_j)
        ℓ_i = rescale_old * ℓ_i + rescale_new * ℓ_ij
        m_i = m_new
    O[i·B_r : (i+1)·B_r] = O_i / ℓ_i             # write back to HBM
    LSE[i·B_r : (i+1)·B_r] = m_i + log(ℓ_i)
```

这就是 Lecture 2 中那段一模一样的 NumPy 代码，映射到两层循环上：外层循环负责 `O` 和 `LSE` 的 HBM 写入，内层循环则留在 SRAM 中。

### 为什么是 `O(N²d²/M)` 字节

内层循环在每次外层迭代 `i` 中把每个 `(K_j, V_j)` 块加载一次。这就是 `K + V` 的 `T_r · T_c · (2 B_c · d)` 字节，再加上 `Q` 的 `T_r · (B_r · d)`，以及 `O / LSE` 写入的 `T_r · (B_r · d)`。

代入 `T_r = N/B_r` 和 `T_c = N/B_c`：

```
KV traffic ≈ (N/B_r) · (N/B_c) · (2 B_c · d · 2 bytes)
           = 4 N² d / B_r           bytes
```

取 `B_r = Θ(M / d)`（在 `[B_r, d]` 的 query 块及其相关数据能塞进 SRAM `M` 的前提下，最大的 `B_r`），就变成 `O(N² d² / M)` 字节——即 FA1 上界。

对每个 SM 典型的 `M = 100 KB` SRAM、`d = 64` 和 `N = 8192` 而言，大约是 `N² d² / M ≈ (8192² · 64²) / 100e3 ≈ 27 MB` 的 HBM 流量——相比朴素变体的数百 MB。这就是收益所在。

### 一句话说反向

FA1 不保存 `S` 或 `P`。为了计算梯度，它借助 `Q, K, V` 和保存的 `LSE`，逐块**重算**它们。这是用少量额外计算换取不必保存 `[N, N]` 矩阵。反向传播在结构上与前向传播完全一致，只是多了几个 `[B_r, B_c]` 矩阵乘——同样的 `O(N²d²/M)` HBM 复杂度。

---


<details>
<summary>English original</summary>

**Lecture 3 — FlashAttention-1 Algorithm**

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** Walk the FA1 forward and backward algorithm, explain its IO complexity bound, and connect the tiles in the paper to the variables in the source.

**Prerequisites:** Lectures 1 and 2. You should be able to derive the IO of naive attention and run the blockwise-softmax NumPy check.

**Artifact:** A pseudocode walk-through plus an HBM-byte counter you can run against arbitrary `(N, d, B_r, B_c)` that reproduces the paper's `O(N²d²/M)` bound to within a constant.

---

**Why it matters**

FA1 is the first kernel that turned attention from an `O(N²)`-bandwidth operation into an `O(N²d²/M)`-bandwidth one, where `M` is on-chip memory (SRAM). That single change is what made 32k–1M context training affordable. If you understand FA1, FA2 and FA3 become "the same thing, scheduled better."

---

**Mental model**

**Tiling Q, K, V**

Split the matrices into tiles:

- `Q` into `T_r = ceil(N / B_r)` row blocks of size `B_r × d`.
- `K` and `V` into `T_c = ceil(N / B_c)` column blocks of size `B_c × d`.

Each block must fit, together with running scratch, in SRAM. For Ampere/Hopper that means roughly `B_r · d + B_c · d + B_r · B_c` words.

**The forward loop (Algorithm 1 from the FA1 paper)**

```
allocate O ∈ [N, d], LSE ∈ [N], all zero / -inf
for i = 0 .. T_r - 1:                           # outer: over query tiles
    Q_i = load Q rows [i·B_r : (i+1)·B_r]       # into SRAM
    m_i = -inf vector of length B_r              # running max
    ℓ_i =  0 vector of length B_r                # running sum-exp
    O_i =  0 matrix [B_r, d]                     # running output
    for j = 0 .. T_c - 1:                       # inner: over KV tiles
        K_j, V_j = load tile [j·B_c : (j+1)·B_c]
        S_ij = (Q_i @ K_j.T) / sqrt(d)          # SRAM only, [B_r, B_c]
        m_ij = rowmax(S_ij)                      # in-register
        P_ij = exp(S_ij - m_ij)                  # local probs
        ℓ_ij = rowsum(P_ij)
        m_new = max(m_i, m_ij)
        rescale_old = exp(m_i  - m_new)
        rescale_new = exp(m_ij - m_new)
        O_i = rescale_old * O_i + rescale_new * (P_ij @ V_j)
        ℓ_i = rescale_old * ℓ_i + rescale_new * ℓ_ij
        m_i = m_new
    O[i·B_r : (i+1)·B_r] = O_i / ℓ_i             # write back to HBM
    LSE[i·B_r : (i+1)·B_r] = m_i + log(ℓ_i)
```

This is the exact NumPy code from Lecture 2, mapped onto a two-level loop where the outer loop owns the HBM writes of `O` and `LSE`, and the inner loop stays in SRAM.

**Why this is `O(N²d²/M)` bytes**

Inner loop loads each `(K_j, V_j)` tile once per outer iteration `i`. That is `T_r · T_c · (2 B_c · d)` bytes for `K + V`, plus `T_r · (B_r · d)` for `Q` and `T_r · (B_r · d)` for `O / LSE` writes.

Substituting `T_r = N/B_r` and `T_c = N/B_c`:

```
KV traffic ≈ (N/B_r) · (N/B_c) · (2 B_c · d · 2 bytes)
           = 4 N² d / B_r           bytes
```

With `B_r = Θ(M / d)` (the largest `B_r` such that a `[B_r, d]` query tile and friends fit in SRAM `M`), this becomes `O(N² d² / M)` bytes — the FA1 bound.

For typical `M = 100 KB` of SRAM per SM, `d = 64`, and `N = 8192`, that is roughly `N² d² / M ≈ (8192² · 64²) / 100e3 ≈ 27 MB` of HBM traffic — vs hundreds of MB for the naive variant. This is the win.

**Backward in one sentence**

FA1 does not store `S` or `P`. To compute gradients it **recomputes** them tile by tile using `Q, K, V` and the saved `LSE`. That trades a small amount of extra compute for not having to store the `[N, N]` matrix. The backward pass is structurally identical to the forward pass with a few extra `[B_r, B_c]` matmuls — same `O(N²d²/M)` HBM complexity.

---

</details>

## 构建它

阅读 FA1 论文第 3 节（Algorithm 1 + 关于反向传播的 Section 3.2）。然后编写这个字节计数器：

```python
# fa1_iobytes.py
def naive_bytes(N, d, dtype=2):
    qkv_load = 3 * N * d * dtype
    s_rw     = 2 * N * N * dtype          # write S, read S
    p_rw     = 2 * N * N * dtype          # write P, read P for PV
    o_write  =     N * d * dtype
    return qkv_load + s_rw + p_rw + o_write

def fa1_bytes(N, d, Br, Bc, dtype=2):
    Tr = -(-N // Br)
    Tc = -(-N // Bc)
    q_load   = Tr * (Br * d) * dtype                       # Q once
    kv_load  = Tr * Tc * (2 * Bc * d) * dtype              # K, V per outer i
    out_write = Tr * (Br * d) * dtype                      # O
    lse_write = Tr * Br * 4                                # fp32 LSE
    return q_load + kv_load + out_write + lse_write

if __name__ == "__main__":
    for N in [1024, 2048, 4096, 8192, 16384]:
        for d in [64, 128]:
            Br, Bc = 64, 64
            naive = naive_bytes(N, d) / 1e6
            fa1   = fa1_bytes(N, d, Br, Bc) / 1e6
            ratio = naive / fa1
            print(f"N={N:>5} d={d:>3} naive={naive:>8.1f} MB  fa1={fa1:>8.1f} MB  speedup={ratio:>5.1f}x")
```

运行它；你应该会看到加速比大致随 `N` 线性增长（因为在你关心的 shape 下，`N²` 项相对 naive 占主导）。

另外：试一下 `B_r = B_c = 32, 64, 128`。分块越小，每个外层 `i` 要做的 KV pass 就越多，HBM 开销也越高——把这一点与 FA1 在 runtime 的选取结果对照，方法是阅读 `csrc/flash_attn/src/flash_fwd_launch_template.h`（按 `head_dim` 的 dispatch 表）。

---

## 在真实技术栈中使用它

追踪一次从 Python 到 kernel 的 forward 调用。从以下位置开始：

`flash-attention/flash_attn/flash_attn_interface.py → flash_attn_func()`
↓
`flash-attention/flash_attn/_C.pyi → mha_fwd(...)`
↓
`flash-attention/csrc/flash_attn/flash_api.cpp → mha_fwd()`
↓
`flash-attention/csrc/flash_attn/src/flash_fwd_launch_template.h → run_mha_fwd_<...>()`
↓
`flash-attention/csrc/flash_attn/src/flash_fwd_kernel.h → flash_fwd_kernel()`

在 `flash_fwd_kernel.h` 中，查找：

- `for (int n_block = ...)` 循环——这就是内层 KV 循环。
- `__syncthreads()` 以及 `tOrO`、`tOrS`、`tOrP` 寄存器变量——即运行中的输出分块与概率分块。
- `softmax_rescale_o_` 及类似项——按 `exp(m_old - m_new)` 重新缩放的步骤。

把 FA1 论文里的每个变量与 kernel 中的真实变量对应起来。在你的笔记中写出映射表（第 6 讲把它与 FA2 略有不同的命名做对比时会用到）。

---

## 测量它

你无法直接从 PyTorch 轻松统计 HBM 字节数，但 Nsight Compute 可以：

```
ncu --set full --target-processes all \
    --metrics dram__bytes.sum,sm__sass_thread_inst_executed_op_dfma_pred_on.sum \
    python attention_bench.py
```

在相同的 `(N, d, H)` 下，比较 naive SDPA 调用与 FlashAttention 调用的 `dram__bytes.sum`。你应该会看到 FA 的字节数大致按 `N · d` 而非 `N²` 增长，与你的 `fa1_bytes` 模型在约 20% 以内吻合（其余部分由 driver 和 L2 流量构成）。

---

## 交付它

在你的 `flash-attn-course/` 工作目录中添加：

1. `fa1_iobytes.py`——你的字节计数器，至少打印三行 `(N, d)`。
2. 一份简短的 Markdown 笔记，把 FA1 论文符号（`m_i`、`ℓ_i`、`O_i`、`m_ij`、`ℓ_ij`）映射到 `flash_fwd_kernel.h` 中的变量名。
3. 一份单一 shape 下的 Nsight Compute 报告，展示 naive 与 FA 的 `dram__bytes.sum`。

如果这三份产物都在，说明你确实读过 kernel——而不只是读过论文。

---

## 相关页面

- [第 2 讲——Online softmax 与数值正确性](第02讲-在线softmax与数值正确性.md)
- [第 4 讲——GPU kernel 性能基础](第04讲-GPU-kernel性能基础.md)
- [第 6 讲——FlashAttention-2](第06讲-FlashAttention-2.md)


<details>
<summary>English original</summary>

**Build it**

Read the FA1 paper Section 3 (Algorithm 1 + Section 3.2 on backward). Then write this byte counter:

```python
# fa1_iobytes.py
def naive_bytes(N, d, dtype=2):
    qkv_load = 3 * N * d * dtype
    s_rw     = 2 * N * N * dtype          # write S, read S
    p_rw     = 2 * N * N * dtype          # write P, read P for PV
    o_write  =     N * d * dtype
    return qkv_load + s_rw + p_rw + o_write

def fa1_bytes(N, d, Br, Bc, dtype=2):
    Tr = -(-N // Br)
    Tc = -(-N // Bc)
    q_load   = Tr * (Br * d) * dtype                       # Q once
    kv_load  = Tr * Tc * (2 * Bc * d) * dtype              # K, V per outer i
    out_write = Tr * (Br * d) * dtype                      # O
    lse_write = Tr * Br * 4                                # fp32 LSE
    return q_load + kv_load + out_write + lse_write

if __name__ == "__main__":
    for N in [1024, 2048, 4096, 8192, 16384]:
        for d in [64, 128]:
            Br, Bc = 64, 64
            naive = naive_bytes(N, d) / 1e6
            fa1   = fa1_bytes(N, d, Br, Bc) / 1e6
            ratio = naive / fa1
            print(f"N={N:>5} d={d:>3} naive={naive:>8.1f} MB  fa1={fa1:>8.1f} MB  speedup={ratio:>5.1f}x")
```

Run it; you should see speedups growing roughly linearly in `N` (because the `N²` term dominates naive at the shapes you care about).

Also: try `B_r = B_c = 32, 64, 128`. The smaller the tile, the more KV passes you do per outer `i`, and the higher the HBM cost — match this against what FA1 picks at runtime by reading `csrc/flash_attn/src/flash_fwd_launch_template.h` (the dispatch table by `head_dim`).

---

**Use it in the real stack**

Trace one forward call from Python to the kernel. Start at:

`flash-attention/flash_attn/flash_attn_interface.py → flash_attn_func()`
↓
`flash-attention/flash_attn/_C.pyi → mha_fwd(...)`
↓
`flash-attention/csrc/flash_attn/flash_api.cpp → mha_fwd()`
↓
`flash-attention/csrc/flash_attn/src/flash_fwd_launch_template.h → run_mha_fwd_<...>()`
↓
`flash-attention/csrc/flash_attn/src/flash_fwd_kernel.h → flash_fwd_kernel()`

Inside `flash_fwd_kernel.h`, look for:

- The `for (int n_block = ...)` loop — that is the inner KV loop.
- `__syncthreads()` and the `tOrO`, `tOrS`, `tOrP` register variables — the running output and probability tiles.
- `softmax_rescale_o_` and similar — the rescale-by-`exp(m_old - m_new)` step.

Match each FA1-paper variable to a real variable in the kernel. Write the mapping table in your notes (you will need it in Lecture 6 when you compare against FA2's slightly different naming).

---

**Measure it**

You cannot easily count HBM bytes from PyTorch directly, but Nsight Compute can:

```
ncu --set full --target-processes all \
    --metrics dram__bytes.sum,sm__sass_thread_inst_executed_op_dfma_pred_on.sum \
    python attention_bench.py
```

Compare `dram__bytes.sum` for a naive SDPA call vs a FlashAttention call at the same `(N, d, H)`. You should see the FA byte count scale roughly as `N · d` rather than `N²`, matching your `fa1_bytes` model within ~20% (driver and L2 traffic make up the rest).

---

**Ship it**

Add to your `flash-attn-course/` working dir:

1. `fa1_iobytes.py` — your byte counter, with at least three `(N, d)` rows printed.
2. A short Markdown note mapping FA1 paper symbols (`m_i`, `ℓ_i`, `O_i`, `m_ij`, `ℓ_ij`) to the variable names in `flash_fwd_kernel.h`.
3. One Nsight Compute report at a single shape, showing `dram__bytes.sum` for naive vs FA.

If those three artifacts exist, you have actually read the kernel — not just the paper.

---

**Related pages**

- [Lecture 2 — Online softmax and numerical correctness](第02讲-在线softmax与数值正确性.md)
- [Lecture 4 — GPU kernel performance basics](第04讲-GPU-kernel性能基础.md)
- [Lecture 6 — FlashAttention-2](第06讲-FlashAttention-2.md)

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Lecture 03 - FlashAttention-1 Algorithm.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Lecture%2003%20-%20FlashAttention-1%20Algorithm.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
