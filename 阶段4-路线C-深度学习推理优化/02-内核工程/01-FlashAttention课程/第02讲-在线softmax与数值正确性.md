---
title: 第 2 讲 — 在线 softmax 与数值正确性
description: 第 2 讲 — 在线 softmax 与数值正确性
---

# 第 2 讲 — 在线 softmax 与数值正确性

**父级：** [FlashAttention Course](Guide.md)

**一句话目的：** 推导并实现分块 log-sum-exp 递推，使 FlashAttention 无需物化完整的 `[N, N]` 分数矩阵即可产生精确的 softmax。

**前置要求：** 第 1 讲。熟悉 `softmax(x) = exp(x − m) / Σ exp(x − m)` 平移形式。

**产物：** 一个 NumPy 脚本，证明分块 LSE 在 fp64 中与一次性 softmax 逐位相等，外加一份针对真实形状观察到的实际 fp16 / bf16 误差的简短报告。

---

## 为什么重要

FlashAttention 不近似 softmax。它使用一个精确恒等式，允许你在流式处理一块又一块 `K` 和 `V` 时增量更新部分 softmax。如果不内化这个递推，就无法阅读 FA1 源码，无法调试数值 bug，也无法将算法移植到新后端。

唯一的技巧是：softmax 对其输入的常量平移不变，并且两个块的 **log-sum-exp** 可使用重缩放因子精确合并。就这些。其余都是工程。

---

## 心智模型

### 经典安全 softmax

为了在 fp16/bf16 下的数值稳定性，绝不直接计算 `exp(x)`。而是计算：

```
m = max(x)
softmax(x)_i = exp(x_i - m) / sum_j exp(x_j - m)
```

还保留 **log-sum-exp**：

```
LSE(x) = m + log(sum_j exp(x_j - m))
```

使得 `softmax(x)_i = exp(x_i - LSE(x))`。这一对 `(m, LSE)` 概括了合并来自不同块的 softmax 所需的一切。

### 精确合并两个块

假设有两个块 `A` 和 `B` 及其统计量：

- `m_A, ℓ_A = sum_j exp(A_j - m_A)`
- `m_B, ℓ_B = sum_j exp(B_j - m_B)`

合并后的最大值为 `m = max(m_A, m_B)`。合并后的分母为：

```
ℓ = exp(m_A - m) · ℓ_A + exp(m_B - m) · ℓ_B
```

就这些。对 `[A, B]` 的合并 softmax 为 `exp(x_i - m) / ℓ`。在算术上等价于对拼接执行一次性 softmax — 无信息丢失。

### 沿路携带输出

FlashAttention 在流式处理时应用 `P · V`。处理完块 `B` 后，运行输出 `O` 变为：

```
O_new = (ℓ_old · exp(m_old - m_new) / ℓ_new) · O_old
      + (exp(m_B   - m_new) / ℓ_new) · (P_B · V_B)
```

第一项为新归一化因子重新缩放已累积的输出；第二项加入此块的贡献。在最后一块之后，`O` 就是该查询分块的精确 `softmax(QKᵀ) · V` 行。**你永远不需要将 `S` 或 `P` 放在 HBM 中** — 只需每个查询行的运行标量 `(m, ℓ)` 以及每步一个 `V` 分块。

### 因果掩码只是改变哪些块参与

因果掩码对 `j > i` 设置 `S_{i,j} = -∞`。在递推中这意味着：当块 `B` 完全位于当前查询分块的对角线上方时，跳过它；当它跨越对角线时，在计算 `m_B, ℓ_B` 之前将 `S_B` 的上三角项掩蔽为 `-∞`。递推保持正确。

---


<details>
<summary>English original</summary>

**Lecture 2 — Online Softmax and Numerical Correctness**

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** Derive and implement the blockwise log-sum-exp recurrence that lets FlashAttention produce an exact softmax without materialising the full `[N, N]` score matrix.

**Prerequisites:** Lecture 1. Comfort with the `softmax(x) = exp(x − m) / Σ exp(x − m)` shifted form.

**Artifact:** A NumPy script that proves blockwise-LSE equals one-shot softmax bit-for-bit in fp64, plus a small report of the actual fp16 / bf16 error you see for realistic shapes.

---

**Why it matters**

FlashAttention does not approximate softmax. It uses an exact identity that lets you update a partial softmax incrementally as you stream tile after tile of `K` and `V`. If you do not internalise this recurrence you cannot read the FA1 source, you cannot debug a numerics bug, and you cannot port the algorithm to a new backend.

The single trick is: softmax is invariant to a constant shift in its input, and the **log-sum-exp** of two blocks can be combined exactly using a rescaling factor. That's it. Everything else is engineering.

---

**Mental model**

**The classical safe softmax**

For numerical stability with fp16/bf16, you never compute `exp(x)` directly. You compute:

```
m = max(x)
softmax(x)_i = exp(x_i - m) / sum_j exp(x_j - m)
```

You also keep the **log-sum-exp**:

```
LSE(x) = m + log(sum_j exp(x_j - m))
```

so that `softmax(x)_i = exp(x_i - LSE(x))`. The pair `(m, LSE)` summarises everything you need to combine softmaxes from different blocks.

**Combining two blocks exactly**

Suppose you have two blocks `A` and `B` and their statistics:

- `m_A, ℓ_A = sum_j exp(A_j - m_A)`
- `m_B, ℓ_B = sum_j exp(B_j - m_B)`

The combined max is `m = max(m_A, m_B)`. The combined denominator is:

```
ℓ = exp(m_A - m) · ℓ_A + exp(m_B - m) · ℓ_B
```

That's it. The combined softmax over `[A, B]` is `exp(x_i - m) / ℓ`. Equivalent in arithmetic to a one-shot softmax over the concatenation — no information lost.

**Carrying the output along**

FlashAttention applies `P · V` while it streams. After processing block `B`, the running output `O` becomes:

```
O_new = (ℓ_old · exp(m_old - m_new) / ℓ_new) · O_old
      + (exp(m_B   - m_new) / ℓ_new) · (P_B · V_B)
```

The first term rescales the already-accumulated output for the new normaliser; the second term adds this block's contribution. After the last block, `O` is the exact `softmax(QKᵀ) · V` row for that query tile. **You never need `S` or `P` in HBM** — just running scalars `(m, ℓ)` per query row and a tile of `V` per step.

**Causal masking just changes which blocks contribute**

Causal mask sets `S_{i,j} = -∞` for `j > i`. In the recurrence that means: when block `B` is entirely above the diagonal for the current query tile, you skip it; when it straddles the diagonal you mask the upper-triangular entries of `S_B` to `-∞` before computing `m_B, ℓ_B`. The recurrence stays correct.

---

</details>

## 实现它

阅读 FA1 论文第 3.1 节（Algorithm 1 —— 融合矩阵乘–softmax）以及 Tri Dao 的 [online-softmax 笔记](https://github.com/Dao-AILab/flash-attention/blob/main/assets/flashattn_banner.jpg)（论文里的算法框比 README 更清楚）。然后编写下面这个精确等价性测试：

```python
# online_softmax_check.py
import numpy as np

def one_shot_softmax(x):
    m = x.max(axis=-1, keepdims=True)
    e = np.exp(x - m)
    return e / e.sum(axis=-1, keepdims=True)

def online_softmax(x, block):
    """Compute softmax(x) by streaming blocks of size `block`."""
    N = x.shape[-1]
    m = np.full(x.shape[:-1] + (1,), -np.inf)
    ell = np.zeros_like(m)
    for s in range(0, N, block):
        e = x[..., s:s+block]
        m_blk = e.max(axis=-1, keepdims=True)
        ell_blk = np.exp(e - m_blk).sum(axis=-1, keepdims=True)
        m_new = np.maximum(m, m_blk)
        ell = np.exp(m - m_new) * ell + np.exp(m_blk - m_new) * ell_blk
        m = m_new
    # Second pass: produce normalised softmax with the converged (m, ell).
    p = np.empty_like(x)
    for s in range(0, N, block):
        p[..., s:s+block] = np.exp(x[..., s:s+block] - m) / ell
    return p

def online_attention(Q, K, V, block):
    """Same idea but folds P·V into the streaming loop. No N×N materialisation."""
    N = K.shape[-2]
    d = V.shape[-1]
    O = np.zeros(Q.shape[:-1] + (d,))
    m = np.full(Q.shape[:-1] + (1,), -np.inf)
    ell = np.zeros_like(m)
    scale = 1.0 / np.sqrt(Q.shape[-1])
    for s in range(0, N, block):
        Kb = K[..., s:s+block, :]
        Vb = V[..., s:s+block, :]
        Sb = scale * (Q @ Kb.swapaxes(-1, -2))      # [..., q, block]
        m_blk = Sb.max(axis=-1, keepdims=True)
        Pb = np.exp(Sb - m_blk)                     # local probs (unnormalised)
        ell_blk = Pb.sum(axis=-1, keepdims=True)
        m_new = np.maximum(m, m_blk)
        rescale_old = np.exp(m - m_new)
        rescale_new = np.exp(m_blk - m_new)
        O = rescale_old * O + rescale_new * (Pb @ Vb)
        ell = rescale_old * ell + rescale_new * ell_blk
        m = m_new
    return O / ell, m + np.log(ell)  # output and LSE

if __name__ == "__main__":
    rng = np.random.default_rng(0)
    H, N, d = 2, 1024, 64
    Q = rng.standard_normal((H, N, d))
    K = rng.standard_normal((H, N, d))
    V = rng.standard_normal((H, N, d))

    # Reference: one-shot
    S = (Q @ K.swapaxes(-1, -2)) / np.sqrt(d)
    P = one_shot_softmax(S)
    O_ref = P @ V

    O_flash, lse = online_attention(Q, K, V, block=64)
    print("max abs diff (fp64):", np.abs(O_ref - O_flash).max())
```

在 fp64 下，diff 应处于 `1e-13` 的量级 —— 纯粹是浮点归约顺序带来的噪声。这证明该算法是**精确的**。

现在在 fp32 和 bf16 下重复该测试（对 `Q, K, V` 做类型转换，跑同一段代码）。记录最大绝对误差，以及几个百分位切点（`p50`、`p99`、`max`）处的相对误差。fp32 的误差应在 `1e-5` 左右，bf16 的误差应在 `5e-3` 左右。这些数字就是你之后在正确性 harness 中要用的容差带（Lecture 7）。

---

## 在真实技术栈中使用它

打开 `flash-attention/flash_attn/flash_attn_interface.py`，找到 `flash_attn_func`。在 CUDA kernel 内运行的前向传播，实现的正是你刚用 NumPy 写下的那个递推。该 kernel 还会额外把逐行的 **log-sum-exp** 作为侧输出返回（`lse`）—— 这正是让反向传播变廉价的原因（Lecture 7）。

你可以通过阅读 `csrc/flash_attn/src/flash_fwd_kernel.h` 来验证：搜索 `running_max`、`running_sum` 和 `rescale` —— 它们就是你 NumPy 代码里 `m`、`ℓ` 和缩放因子在寄存器中的名字。

---

## 测量它

实验环节：

- fp64 参考实现 vs 你的 blockwise-LSE：最大绝对差应处于机器精度噪声水平（~1e-13）。如果不是，说明有 bug —— 最可能出在 `O` 或 `ℓ` 的 `exp(m_old - m_new)` 缩放上。
- fp32 vs fp32 blockwise：最大绝对差 ~1e-5。
- bf16 vs bf16 blockwise：最大绝对差 ~5e-3 到 1e-2，取决于 `N` 和 score 分布。

**不要**拿 bf16 去和 fp64 比较，然后抱怨 1e-2 的误差 —— 那是 dtype 的问题，不是算法的问题。

---

## 交付它

把脚本以 `online_softmax_check.py` 为名保存到课程工作目录中。它必须产出：

1. fp64 最大绝对差接近 `1e-13`（证明精确性）。
2. fp32 和 bf16 的最大绝对差与上面的预期数值一致。
3. 一段简短的 README 说明：“running `(m, ℓ)` 与 rescale-by-`exp(m_old - m_new)` 共同实现了一个精确的流式 softmax。”

之后你构建或修改的每个 kernel，都会复用这个脚本作为 gold reference。

---

## 相关页面

- [Lecture 1 — Attention 瓶颈与 roofline（性能上界模型）](第01讲-注意力瓶颈与roofline.md)
- [Lecture 3 — FlashAttention-1 算法](第03讲-FlashAttention-1算法.md)
- [Lecture 7 — 反向传播与验证](第07讲-反向传播与数值验证.md)


<details>
<summary>English original</summary>

**Build it**

Read FA1 paper Section 3.1 (Algorithm 1 — fused matmul–softmax) and Tri Dao's [online-softmax note](https://github.com/Dao-AILab/flash-attention/blob/main/assets/flashattn_banner.jpg) (the algorithm boxes in the paper are clearer than the README). Then write this exact-equivalence test:

```python
# online_softmax_check.py
import numpy as np

def one_shot_softmax(x):
    m = x.max(axis=-1, keepdims=True)
    e = np.exp(x - m)
    return e / e.sum(axis=-1, keepdims=True)

def online_softmax(x, block):
    """Compute softmax(x) by streaming blocks of size `block`."""
    N = x.shape[-1]
    m = np.full(x.shape[:-1] + (1,), -np.inf)
    ell = np.zeros_like(m)
    for s in range(0, N, block):
        e = x[..., s:s+block]
        m_blk = e.max(axis=-1, keepdims=True)
        ell_blk = np.exp(e - m_blk).sum(axis=-1, keepdims=True)
        m_new = np.maximum(m, m_blk)
        ell = np.exp(m - m_new) * ell + np.exp(m_blk - m_new) * ell_blk
        m = m_new
    # Second pass: produce normalised softmax with the converged (m, ell).
    p = np.empty_like(x)
    for s in range(0, N, block):
        p[..., s:s+block] = np.exp(x[..., s:s+block] - m) / ell
    return p

def online_attention(Q, K, V, block):
    """Same idea but folds P·V into the streaming loop. No N×N materialisation."""
    N = K.shape[-2]
    d = V.shape[-1]
    O = np.zeros(Q.shape[:-1] + (d,))
    m = np.full(Q.shape[:-1] + (1,), -np.inf)
    ell = np.zeros_like(m)
    scale = 1.0 / np.sqrt(Q.shape[-1])
    for s in range(0, N, block):
        Kb = K[..., s:s+block, :]
        Vb = V[..., s:s+block, :]
        Sb = scale * (Q @ Kb.swapaxes(-1, -2))      # [..., q, block]
        m_blk = Sb.max(axis=-1, keepdims=True)
        Pb = np.exp(Sb - m_blk)                     # local probs (unnormalised)
        ell_blk = Pb.sum(axis=-1, keepdims=True)
        m_new = np.maximum(m, m_blk)
        rescale_old = np.exp(m - m_new)
        rescale_new = np.exp(m_blk - m_new)
        O = rescale_old * O + rescale_new * (Pb @ Vb)
        ell = rescale_old * ell + rescale_new * ell_blk
        m = m_new
    return O / ell, m + np.log(ell)  # output and LSE

if __name__ == "__main__":
    rng = np.random.default_rng(0)
    H, N, d = 2, 1024, 64
    Q = rng.standard_normal((H, N, d))
    K = rng.standard_normal((H, N, d))
    V = rng.standard_normal((H, N, d))

    # Reference: one-shot
    S = (Q @ K.swapaxes(-1, -2)) / np.sqrt(d)
    P = one_shot_softmax(S)
    O_ref = P @ V

    O_flash, lse = online_attention(Q, K, V, block=64)
    print("max abs diff (fp64):", np.abs(O_ref - O_flash).max())
```

In fp64 the diff should be at the level of `1e-13` — purely floating-point reduction-order noise. That proves the algorithm is **exact**.

Now repeat the test in fp32 and bf16 (cast `Q, K, V` and run the same code). Record the max absolute error and the relative error at a few percentile cuts (`p50`, `p99`, `max`). You should see fp32 errors around `1e-5` and bf16 errors around `5e-3`. These numbers are the tolerance bands you will use later in your correctness harness (Lecture 7).

---

**Use it in the real stack**

Open `flash-attention/flash_attn/flash_attn_interface.py` and look for `flash_attn_func`. The forward pass that runs inside the CUDA kernel implements exactly the recurrence you just wrote in NumPy. The kernel additionally returns the per-row **log-sum-exp** as a side output (`lse`) — that is what makes the backward pass cheap (Lecture 7).

You can verify by reading `csrc/flash_attn/src/flash_fwd_kernel.h`: search for `running_max`, `running_sum`, and `rescale` — these are the in-register names for the `m`, `ℓ`, and rescale factor from your NumPy code.

---

**Measure it**

For the lab:

- fp64 reference vs your blockwise-LSE: max abs diff should be at machine precision noise (~1e-13). If it is not, you have a bug — most likely in the `exp(m_old - m_new)` rescale of either `O` or `ℓ`.
- fp32 vs fp32 blockwise: max abs diff ~1e-5.
- bf16 vs bf16 blockwise: max abs diff ~5e-3 to 1e-2, depending on `N` and the score distribution.

Do **not** compare bf16 against fp64 and complain about a 1e-2 error — that is the dtype, not the algorithm.

---

**Ship it**

Save the script as `online_softmax_check.py` in your course working directory. It must produce:

1. fp64 max abs diff close to `1e-13` (proves exactness).
2. fp32 and bf16 max abs diff in line with the expected numbers above.
3. A short README note: "the running `(m, ℓ)` and rescale-by-`exp(m_old - m_new)` together implement an exact streaming softmax."

You will reuse this script as the gold reference for every later kernel you build or modify.

---

**Related pages**

- [Lecture 1 — Attention bottleneck and roofline](第01讲-注意力瓶颈与roofline.md)
- [Lecture 3 — FlashAttention-1 algorithm](第03讲-FlashAttention-1算法.md)
- [Lecture 7 — Backward pass and validation](第07讲-反向传播与数值验证.md)

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Lecture 02 - Online Softmax and Numerical Correctness.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Lecture%2002%20-%20Online%20Softmax%20and%20Numerical%20Correctness.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
