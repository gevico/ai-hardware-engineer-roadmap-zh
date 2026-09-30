---
title: 第 6 讲：批 GEMM 与 Normal GEMM —— kernel、声明与位级一致结果
description: 第 6 讲：批 GEMM 与 Normal GEMM —— kernel、声明与位级一致结果
---

# 第 6 讲：批 GEMM 与 Normal GEMM —— kernel、声明与位级一致结果

## 概述

第 3 讲指出，在边缘硬件上 **decode**（逐 token 生成阶段）受 GEMV（矩阵-向量乘）限制。第 4 讲提到 **prefill**（首字前的整段计算）和 **批 decode** 受 GEMM（矩阵-矩阵乘）限制，但没有花时间展开。本讲介于两者之间：在 cuBLAS API 层面剖析 GEMM 与**批** GEMM kernel，展示三种形式（single、array-of-pointers、strided），说明各自在 Qwen 推理路径中的位置，并且 —— 最重要的是 —— 说明在需要交叉验证时，如何在不同形式之间拿到**相同的数值结果**。

如果你曾把张量喂进 `cublasSgemmStridedBatched`，拿到的输出看起来*几乎*正确却略有偏差，那这讲就是为你准备的。

读完本讲，你应该能够：

* 读懂一段 cuBLAS GEMM 声明，并识别出布局、前导维度与批步长。
* 针对给定的工作负载，选择 `Gemm`、`GemmBatched` 或 `GemmStridedBatched`（并说明理由）。
* 从批 GEMM 复现单 GEMM 的结果（反之亦然），做到逐位一致。
* 识别出约八种会产生“错误但看似合理”输出的失效模式。

本讲以 CUDA / cuBLAS 为核心，因为 Qwen 的生产推理就运行在这里。同样的概念也适用于 rocBLAS（`rocblas_sgemm_strided_batched`）、MKL（`cblas_sgemm_batch`），以及 Apple 的 `Accelerate` 框架配合 `BNNSBatch`。API 名字不同，数学不变。

---

## 1. 批 GEMM 在 Qwen 推理中出现的位置

三个地方，都很重要：

### 1.1 Prefill

当你把长度为 `seq_len = N` 个 token 的 prompt 喂给 Qwen3-4B 时，每一个 layer 的每一个线性投影都会变成 GEMM，而不是 GEMV：

```
Input:   X = [d_model, N]               # d_model = 2560 for Qwen3-4B
W_Q:     [d_proj, d_model]              # d_proj = 4096
Q = W_Q @ X → [d_proj, N]               # GEMM: M=4096, N=N, K=2560
```

对于 `seq_len = 256`，这就是 `M=4096, N=256, K=2560` —— 尺寸适配张量核心，在 H100 上约 5 ms，在 Orin Nano 上约 50 ms。prefill 的首 token 时延几乎完全取决于这些 GEMM 跑得多快。

每个投影都是一个 **single GEMM** —— 而不是批 GEMM。“批”就是单个矩阵；第二维是序列。

### 1.2 逐 head 的 attention 分数（批 GEMM 就出现在这里）

经过 QKV 投影和旋转位置编码（RoPE）之后，得到：

```
Q:  [n_heads, N, head_dim]      # for Qwen3-4B: [32, N, 128]
K:  [n_kv_heads, N, head_dim]   # [8, N, 128]
V:  [n_kv_heads, N, head_dim]   # [8, N, 128]
```

分数计算是**逐 head** 的一次独立矩阵乘：

```
S[h] = Q[h] @ K[h//group]^T    # for each head h
                                # [N, head_dim] @ [head_dim, N] = [N, N]
```

`n_heads = 32` 个形状为 `(N, head_dim, N)` 的独立 GEMM。这就是批大小为 32 的**批 GEMM**。

实际上**已经没有人真的为 attention 调用 cuBLAS 了** —— FlashAttention 把 GEMM + softmax + 第二个 GEMM 融合进一个分块 kernel。但 FlashAttention 底层解的正是这种批 GEMM 形态的问题，而参考实现（Sionna、eager PyTorch）仍然走 `torch.matmul` → `cublasSgemmStridedBatched`。

### 1.3 批推理服务（Qwen2.5-72B 服务多用户）

当 vLLM 连续批处理 B 个长度相同的请求时，每个投影都变成：

```
X = [d_model, B · N]            # B sequences concatenated
Q = W_Q @ X → [d_proj, B · N]   # one big GEMM, not batched
```

这仍然是单个 GEMM，只是 N 更大。**批维度变成了 GEMM 的第二维。** 这是吞吐最高的场景 —— 当 `B·N ≥ 1024` 时，张量核心在 `M=4096, N=B·N, K=2560` 上达到满利用率。

“批 GEMM”这种 API 形式实际出现在序列**长度不同、KV 缓存布局也各异**的时候 —— 但这时就由 paged attention 配合自定义 kernel 接手了。

---

## 2. cuBLAS GEMM 的三种形式

### 2.1 Normal GEMM —— 单个矩阵对

```c
// y = alpha · op(A) · op(B) + beta · C
cublasStatus_t cublasSgemm(
    cublasHandle_t handle,
    cublasOperation_t transa,         // CUBLAS_OP_N (no transpose) | _T | _C
    cublasOperation_t transb,
    int m, int n, int k,
    const float *alpha,
    const float *A, int lda,           // device pointer + leading dim
    const float *B, int ldb,
    const float *beta,
    float *C, int ldc);
```

解读这些形状：结果是 `[m, n]`，`A` 在 `transa` 之后是 `[m, k]`，`B` 在 `transb` 之后是 `[k, n]`。“前导维度”就是**列与列之间的步长**（因为 cuBLAS 是列优先）。


<details>
<summary>English original</summary>

**Lecture 6: Batched GEMM vs Normal GEMM — Kernels, Declarations, and Bit-Comparable Results**

**Overview**

Lecture 3 showed that **decode** is GEMV-bound on edge hardware. Lecture 4 mentioned, without spending time on it, that **prefill** and **batched decode** are GEMM-bound. This lecture sits between them: it dissects the GEMM and **batched** GEMM kernels at the cuBLAS API level, shows the three forms (single, array-of-pointers, strided), explains where each one fits in the Qwen inference path, and — most importantly — shows how to get the **same numerical result** across forms when you need cross-validation.

If you've ever fed a tensor into `cublasSgemmStridedBatched` and gotten outputs that looked *almost* right but slightly off, this is your lecture.

By the end you should be able to:

* Read a cuBLAS GEMM declaration and identify the layout, leading dimensions, and batch stride.
* Choose `Gemm`, `GemmBatched`, or `GemmStridedBatched` for a given workload (and explain why).
* Reproduce a single-GEMM result from a batched GEMM (and vice versa) bit-for-bit.
* Recognize the eight or so failure modes that produce "wrong but plausible" outputs.

This lecture is CUDA / cuBLAS-centric because that's where Qwen production inference lives. The same concepts apply to rocBLAS (`rocblas_sgemm_strided_batched`), MKL (`cblas_sgemm_batch`), and Apple's `Accelerate` framework with `BNNSBatch`. The API names differ; the math doesn't.

---

**1. Where Batched GEMM Shows Up in Qwen Inference**

Three places, all important:

**1.1 Prefill**

When you feed a prompt of `seq_len = N` tokens to Qwen3-4B, every linear projection at every layer becomes a GEMM, not a GEMV:

```
Input:   X = [d_model, N]               # d_model = 2560 for Qwen3-4B
W_Q:     [d_proj, d_model]              # d_proj = 4096
Q = W_Q @ X → [d_proj, N]               # GEMM: M=4096, N=N, K=2560
```

For `seq_len = 256` this is `M=4096, N=256, K=2560` — sized for tensor cores, ~5 ms on H100, ~50 ms on Orin Nano. Prefill TTFT depends almost entirely on how fast these GEMMs run.

This is a **single GEMM** per projection — not a batched GEMM. The "batch" is one matrix; the second dimension is the sequence.

**1.2 Per-head attention scores (this is where batched GEMM appears)**

After QKV projections and RoPE, you have:

```
Q:  [n_heads, N, head_dim]      # for Qwen3-4B: [32, N, 128]
K:  [n_kv_heads, N, head_dim]   # [8, N, 128]
V:  [n_kv_heads, N, head_dim]   # [8, N, 128]
```

The score computation is one independent matrix multiply **per head**:

```
S[h] = Q[h] @ K[h//group]^T    # for each head h
                                # [N, head_dim] @ [head_dim, N] = [N, N]
```

`n_heads = 32` independent GEMMs of shape `(N, head_dim, N)`. This is a **batched GEMM** of batch size 32.

In practice **nobody actually calls cuBLAS for attention anymore** — FlashAttention fuses the GEMM + softmax + second GEMM into one tiled kernel. But the batched-GEMM-shaped problem is what FlashAttention is solving under the hood, and reference implementations (Sionna, eager PyTorch) still go through `torch.matmul` → `cublasSgemmStridedBatched`.

**1.3 Batched serving (Qwen2.5-72B with multiple users)**

When vLLM continuous-batches B requests with the same length, every projection becomes:

```
X = [d_model, B · N]            # B sequences concatenated
Q = W_Q @ X → [d_proj, B · N]   # one big GEMM, not batched
```

That's still a single GEMM with bigger N. **The batch dimension becomes the GEMM's second dimension.** This is the highest-throughput regime — tensor cores run at full utilization on `M=4096, N=B·N, K=2560` when `B·N ≥ 1024`.

The "batched GEMM" API form actually shows up when sequences have **different lengths and KV-cache layouts** — but that's where paged attention takes over with custom kernels.

---

**2. The Three cuBLAS GEMM Forms**

**2.1 Normal GEMM — single matrix pair**

```c
// y = alpha · op(A) · op(B) + beta · C
cublasStatus_t cublasSgemm(
    cublasHandle_t handle,
    cublasOperation_t transa,         // CUBLAS_OP_N (no transpose) | _T | _C
    cublasOperation_t transb,
    int m, int n, int k,
    const float *alpha,
    const float *A, int lda,           // device pointer + leading dim
    const float *B, int ldb,
    const float *beta,
    float *C, int ldc);
```

Reading the shapes: result is `[m, n]`, `A` is `[m, k]` after `transa`, `B` is `[k, n]` after `transb`. The "leading dimension" is the **stride between columns** (because cuBLAS is column-major).

</details>

### 2.2 GemmBatched — 指针数组

```c
cublasStatus_t cublasSgemmBatched(
    cublasHandle_t handle,
    cublasOperation_t transa,
    cublasOperation_t transb,
    int m, int n, int k,
    const float *alpha,
    const float *const Aarray[], int lda,    // device pointer to array of device pointers
    const float *const Barray[], int ldb,
    const float *beta,
    float *const Carray[], int ldc,
    int batchCount);
```

矩阵可以位于任意位置——`Aarray[i]` 指向 A_i，它不必与 `Aarray[i+1]` 连续。适用于：

- 矩阵来自一组分配。
- 每个批的矩阵具有**不同大小**（可以按相同大小的批分组循环）。
- 矩阵散布在多个内存池中。

**代价：** 指针数组本身就是一个设备数组，因此每个批元素要多付一次间接寻址。批大小 < ~16 时这一点很关键。

### 2.3 GemmStridedBatched — 连续带步长的矩阵

```c
cublasStatus_t cublasSgemmStridedBatched(
    cublasHandle_t handle,
    cublasOperation_t transa,
    cublasOperation_t transb,
    int m, int n, int k,
    const float *alpha,
    const float *A, int lda, long long int strideA,   // single base ptr + stride
    const float *B, int ldb, long long int strideB,
    const float *beta,
    float *C, int ldc, long long int strideC,
    int batchCount);
```

A_i 位于 `A + i * strideA`，每个批的 `m, k, lda` 相同。这正是**几乎所有 LLM 推理都在用的**——`Q`、`K`、`V` 张量都是 3D 连续张量，批（head）维度具有干净固定的步长。

只要能用就用它。它比指针数组形式更快，因为 cuBLAS 可以从步长算出指针，间接寻址成本为零。

### 2.4 Ex 变体 — 面向混合精度

对于 FP16 / BF16 / FP8 / TF32，使用 `Ex` 版本：

```c
cublasGemmEx(...)                   // single, mixed precision
cublasGemmBatchedEx(...)            // batched-pointer, mixed precision
cublasGemmStridedBatchedEx(...)     // strided-batched, mixed precision
```

它们接受单独的 `Atype`、`Btype`、`Ctype`、`computeType` 和 `algo` 参数。**这才是生产环境 LLM 推理实际调用的**——FP16/BF16 权重在 tensor cores 上以 FP32 累加。

---

## 3. 内存布局——列主序、行主序，以及转置之舞

cuBLAS 是**列主序**。PyTorch/NumPy/C/Python 是**行主序**。这是**最常见的 bug 来源**。

内存中一个行主序的 `[M, K]` 矩阵与一个列主序的 `[K, M]` 矩阵逐字节完全相同。字节相同，解释不同。所以当你对行主序数据调用 cuBLAS 时，要这样传：

```
"I have row-major X = [M, K]"   ⟺   "cuBLAS sees column-major X^T = [K, M]"
```

要计算 `Y = X @ W`（行主序，其中 `X = [M, K]`、`W = [K, N]`）：

```
PyTorch/NumPy view:
   Y[M, N] = X[M, K] · W[K, N]

cuBLAS view (after row→col reinterpretation):
   Y^T[N, M] = W^T[N, K] · X^T[K, M]

cuBLAS call (no extra transposes — the layout flip handles it):
   cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N,
               N, M, K,        // note: m=N, n=M
               &alpha,
               W, N,            // W^T from cuBLAS view, lda = N
               X, K,            // X^T from cuBLAS view, lda = K
               &beta,
               Y, N);           // Y^T, ldc = N
```

这就是所有使用 cuBLAS 的 C++ 推理代码库都会用的**交换参数惯用法**。仔细读一遍，然后内化。

对于批处理，步长遵循同样的翻转：

```
strideA = M * K   (PyTorch view) ↔  K * M (cuBLAS view, but same bytes)
```

字节是一样的；唯一的问题是哪个维度被 cuBLAS 称为 `m` 而非 `n`。一旦翻转了思维模型，一切自然就对了。

### leading dimension 速查表

| 布局 | 矩阵 `[rows, cols]` | `ld` 取值 |
|---|---|---|
| 列主序，未转置 | `[m, k]` | `m` |
| 列主序，转置 | `[k, m]`（存储为 `[m, k]`） | `m` |
| 行主序重新解释为列主序 | cuBLAS 视角下的 `[k, m]` | `k` |

当你设置 `ld > rows` 时，意味着列之间**有填充**——对对齐有用。大多数 LLM 推理使用 `ld == rows`（无填充）。

---

## 4. 在不同形式间产生相同结果

你经常会用一种 cuBLAS 形式写参考代码，用另一种写生产代码。问题是：它们何时会产生**完全相同**的结果？


<details>
<summary>English original</summary>

**2.2 GemmBatched — array of pointers**

```c
cublasStatus_t cublasSgemmBatched(
    cublasHandle_t handle,
    cublasOperation_t transa,
    cublasOperation_t transb,
    int m, int n, int k,
    const float *alpha,
    const float *const Aarray[], int lda,    // device pointer to array of device pointers
    const float *const Barray[], int ldb,
    const float *beta,
    float *const Carray[], int ldc,
    int batchCount);
```

The matrices can live anywhere — `Aarray[i]` points to A_i, which doesn't have to be contiguous with `Aarray[i+1]`. Useful when:

- Matrices come from a list of allocations.
- Per-batch matrices have **different sizes** (you'd loop over groups of same-size batches).
- Matrices are scattered across memory pools.

**Cost:** the array of pointers is itself a device array, so you pay an extra indirection per batch element. For batch sizes < ~16 this matters.

**2.3 GemmStridedBatched — contiguous strided matrices**

```c
cublasStatus_t cublasSgemmStridedBatched(
    cublasHandle_t handle,
    cublasOperation_t transa,
    cublasOperation_t transb,
    int m, int n, int k,
    const float *alpha,
    const float *A, int lda, long long int strideA,   // single base ptr + stride
    const float *B, int ldb, long long int strideB,
    const float *beta,
    float *C, int ldc, long long int strideC,
    int batchCount);
```

A_i lives at `A + i * strideA`, with the same `m, k, lda` for every batch. This is what **virtually all LLM inference uses** — `Q`, `K`, `V` tensors are 3D contiguous tensors, the batch (head) dimension has a clean fixed stride.

Use this whenever you can. It's faster than the pointer-array form because cuBLAS can compute pointers from the stride at zero indirection cost.

**2.4 The Ex variants — for mixed precision**

For FP16 / BF16 / FP8 / TF32, use the `Ex` versions:

```c
cublasGemmEx(...)                   // single, mixed precision
cublasGemmBatchedEx(...)            // batched-pointer, mixed precision
cublasGemmStridedBatchedEx(...)     // strided-batched, mixed precision
```

They take separate `Atype`, `Btype`, `Ctype`, `computeType`, and `algo` parameters. **This is what production LLM inference actually calls** — FP16/BF16 weights with FP32 accumulation on tensor cores.

---

**3. Memory Layout — Column-Major, Row-Major, and the Transpose Dance**

cuBLAS is **column-major**. PyTorch/NumPy/C/Python are **row-major**. This is the **single most common source of bugs**.

A row-major `[M, K]` matrix in memory is byte-for-byte identical to a column-major `[K, M]` matrix. Same bytes; different interpretation. So when you call cuBLAS on row-major data, you give it:

```
"I have row-major X = [M, K]"   ⟺   "cuBLAS sees column-major X^T = [K, M]"
```

To compute `Y = X @ W` (row-major, where `X = [M, K]`, `W = [K, N]`):

```
PyTorch/NumPy view:
   Y[M, N] = X[M, K] · W[K, N]

cuBLAS view (after row→col reinterpretation):
   Y^T[N, M] = W^T[N, K] · X^T[K, M]

cuBLAS call (no extra transposes — the layout flip handles it):
   cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N,
               N, M, K,        // note: m=N, n=M
               &alpha,
               W, N,            // W^T from cuBLAS view, lda = N
               X, K,            // X^T from cuBLAS view, lda = K
               &beta,
               Y, N);           // Y^T, ldc = N
```

This is the **swap-arguments idiom** every cuBLAS-using C++ inference codebase uses. Read it once carefully; then internalize.

For batched, the strides follow the same flip:

```
strideA = M * K   (PyTorch view) ↔  K * M (cuBLAS view, but same bytes)
```

The bytes are the same; the only question is which dimension cuBLAS calls `m` vs `n`. Once you flip your mental model, everything just works.

**Leading dimension cheat sheet**

| Layout | Matrix `[rows, cols]` | `ld` value |
|---|---|---|
| Column-major, not transposed | `[m, k]` | `m` |
| Column-major, transposed | `[k, m]` (stored as `[m, k]`) | `m` |
| Row-major reinterpreted as col-major | `[k, m]` in cuBLAS view | `k` |

When you set `ld > rows` it means you have **padding** between columns — useful for alignment. Most LLM inference uses `ld == rows` (no padding).

---

**4. Producing the Same Result Across Forms**

You'll often write reference code with one cuBLAS form and production code with another. The question: when do they produce **identical** results?

</details>

### 4.1 跨形式的位精确 —— 严格版本

要让两次 cuBLAS 调用（例如：循环调用 `Gemm` vs `GemmStridedBatched`）产生逐位相同的结果，必须**同时满足以下全部条件**：

1. **操作数字节完全相同。** 步长与偏移必须以相同方式排布出相同的矩阵。
2. **`alpha`、`beta`、`computeType` 完全相同。**
3. **选中相同的算法。** cuBLAS 会自动调优；用 `cublasGemmEx(..., CUBLAS_GEMM_ALGO0)` 或你固定的那个算法强制保持一致。
4. **相同架构。** 由于累加顺序不同，Tensor Core 路径产生的位模式与 CUDA Core 路径不同。
5. **不降级到 TF32。** 设置 `cublasSetMathMode(handle, CUBLAS_PEDANTIC_MATH)`。Ampere 架构及之后版本的默认 `CUBLAS_DEFAULT_MATH` 会在某些 FP32 路径上静默使用 TF32 —— 舍入方式不同。

当这五条全部成立时，在同一硬件上结果逐位一致。

### 4.2 数值等价（FP32 等价 epsilon）

在大多数实际场景中，你不需要位精确，你需要的是「差异至多相当于浮点噪声」。对 FP32 矩阵乘而言，大致意味着：

```
|result_a - result_b| / |result_a| < ~1e-6 · sqrt(K)
```

（`sqrt(K)` 来自累加 K 个乘积的随机游走误差模型。）

因此对 `K = 2560` 的 GEMM（矩阵-矩阵乘），在 FP32 下跨不同算法或累加顺序，相对差异最大可达 **~5e-5**。比这更大就说明存在真正的 bug。

对带 FP32 累加的 FP16 矩阵乘（常见的 LLM 场景），绝对误差界与之相近，因为累加是 FP32 —— 但 FP16 输入每个本身的舍入就有 ~1e-3。

### 4.3 哪里会出错 —— 八种常见 bug

| Bug | 症状 | 如何发现 |
|---|---|---|
| 行/列主序混淆 | 输出看起来被转置了 | 把结果 `[m, n]` reshape 成 `[n, m]` —— 对得上吗？ |
| `transa`/`transb` 标志设置错误 | 结果是 `A·B^T` 或 `A^T·B`，而不是 `A·B` | 用 `M=K=N=2` 运行并手工校验 |
| `lda`/`ldb`/`ldc` 错误 | 第一列之后输出像是乱码 | 缩小测试规模；打印前 2×2 块 |
| strided-batched 的步长错误 | 部分 batch 正确，其余重叠 | Output[i] 用到了 batch i+1 的元素 |
| 指针数组指向错误的偏移 | 随机的 batch 元素正确/错误 | 打印指针值；与手工计算做 diff |
| 混合精度：alpha 是 FP32，但被指向的值按 FP16 读取 | 输出约为应有值的一半 | 打印 alpha 值；检查 `computeType` |
| 张量核心要求的对齐未满足 | 回退到慢路径 | 只是性能提示，不影响正确性 |
| 用了 `beta != 0` 但 `C` 未初始化 | 会加上随机的 GPU 显存内容 | 为得到干净输出，设置 `beta = 0` |

最后一种是最经典的。cuBLAS 执行 `C = alpha · A·B + beta · C`。如果 `beta = 1` 且 `C` 是垃圾内存，那你加进去的就是垃圾。

---


<details>
<summary>English original</summary>

**4.1 Bit-exact across forms — the strict version**

For two cuBLAS calls (e.g., `Gemm` looped vs `GemmStridedBatched`) to produce bit-identical results, **all of**:

1. **Identical operand bytes.** The strides and offsets must lay out the same matrices.
2. **Identical `alpha`, `beta`, `computeType`.**
3. **Same algorithm selected.** cuBLAS will auto-tune; force consistency with `cublasGemmEx(..., CUBLAS_GEMM_ALGO0)` or whichever algo you pin.
4. **Same architecture.** Tensor core paths produce different bit patterns than CUDA-core paths because of different accumulation orders.
5. **No TF32 reduction.** Set `cublasSetMathMode(handle, CUBLAS_PEDANTIC_MATH)`. The default `CUBLAS_DEFAULT_MATH` on Ampere+ silently uses TF32 in some FP32 paths — different rounding.

When all five hold, the results match bit-for-bit on the same hardware.

**4.2 Numerically equivalent (FP32-equivalent epsilon)**

For most practical purposes you don't need bit-exact, you need "the difference is at most floating-point noise." For FP32 matmul that means roughly:

```
|result_a - result_b| / |result_a| < ~1e-6 · sqrt(K)
```

(The `sqrt(K)` comes from the random-walk error model of accumulating K products.)

So for a `K = 2560` GEMM, you should expect relative differences up to **~5e-5** in FP32 across different algorithms or accumulation orders. Anything bigger and you have a real bug.

For FP16 matmul with FP32 accumulation (the common LLM case), bound is similar in absolute terms because accumulation is FP32 — but the FP16 inputs round to ~1e-3 each.

**4.3 What goes wrong — the eight common bugs**

| Bug | Symptom | How to spot |
|---|---|---|
| Row/col-major confusion | Output looks transposed | Reshape result `[m, n]` to `[n, m]` — does it match? |
| Wrong `transa`/`transb` flags | Result is `A·B^T` or `A^T·B` instead of `A·B` | Run with `M=K=N=2` and verify by hand |
| Wrong `lda`/`ldb`/`ldc` | Output looks like junk past first column | Smaller test; print first 2×2 block |
| Strided-batched stride wrong | Some batches correct, others overlap | Output[i] uses elements from batch i+1 |
| Pointer-array points to wrong offsets | Random batch elements correct/wrong | Print pointer values; diff with manual calc |
| Mixed precision: alpha is FP32 but pointed-to value reads as FP16 | Output ~half what it should be | Print alpha value; check `computeType` |
| Tensor cores require alignment that isn't met | Falls back to slow path | Performance hint, not correctness |
| `beta != 0` but `C` not initialized | Adds random GPU memory contents | Set `beta = 0` for fresh outputs |

The last one is the absolute classic. cuBLAS does `C = alpha · A·B + beta · C`. If `beta = 1` and `C` is garbage memory, you add garbage.

---

</details>

## 5. 最小完整示例 —— 单次与跨步批量

验证 `GemmStridedBatched` 与循环调用 `Gemm` 在三个相同的 4×4 矩阵上产生同样的输出。

```c++
#include <cublas_v2.h>
#include <cuda_runtime.h>
#include <cstdio>
#include <cmath>

int main() {
    const int M = 4, N = 4, K = 4, BATCH = 3;
    const size_t SIZE = M * K * BATCH * sizeof(float);

    // Host data: three 4×4 matrices A, B, C; same data for clarity.
    float h_A[M * K * BATCH], h_B[K * N * BATCH], h_C_single[M * N * BATCH], h_C_batch[M * N * BATCH];
    for (int i = 0; i < M * K * BATCH; ++i) h_A[i] = (i % 7) * 0.1f;
    for (int i = 0; i < K * N * BATCH; ++i) h_B[i] = (i % 5) * 0.2f;

    // Device pointers
    float *d_A, *d_B, *d_C_single, *d_C_batch;
    cudaMalloc(&d_A, SIZE);  cudaMalloc(&d_B, SIZE);
    cudaMalloc(&d_C_single, SIZE);  cudaMalloc(&d_C_batch, SIZE);
    cudaMemcpy(d_A, h_A, SIZE, cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, SIZE, cudaMemcpyHostToDevice);
    cudaMemset(d_C_single, 0, SIZE);
    cudaMemset(d_C_batch,  0, SIZE);

    cublasHandle_t handle;
    cublasCreate(&handle);
    cublasSetMathMode(handle, CUBLAS_PEDANTIC_MATH);    // force deterministic path

    const float alpha = 1.0f, beta = 0.0f;

    // 1) Looped single GEMM
    for (int b = 0; b < BATCH; ++b) {
        cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N,
                    M, N, K,
                    &alpha,
                    d_A + b * M * K, M,
                    d_B + b * K * N, K,
                    &beta,
                    d_C_single + b * M * N, M);
    }

    // 2) One strided batched GEMM
    cublasSgemmStridedBatched(handle,
                              CUBLAS_OP_N, CUBLAS_OP_N,
                              M, N, K,
                              &alpha,
                              d_A, M, (long long)(M * K),     // strideA
                              d_B, K, (long long)(K * N),     // strideB
                              &beta,
                              d_C_batch, M, (long long)(M * N), // strideC
                              BATCH);

    cudaMemcpy(h_C_single, d_C_single, SIZE, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_C_batch,  d_C_batch,  SIZE, cudaMemcpyDeviceToHost);

    // Compare
    float max_diff = 0.0f;
    for (int i = 0; i < M * N * BATCH; ++i) {
        max_diff = fmaxf(max_diff, fabsf(h_C_single[i] - h_C_batch[i]));
    }
    printf("Max abs diff: %.3e\n", max_diff);
    // Expected: 0.0 with PEDANTIC_MATH, ~1e-7 otherwise.

    cublasDestroy(handle);
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C_single); cudaFree(d_C_batch);
}
```

编译并运行：

```bash
nvcc -lcublas batched_gemm_compare.cu -o batched_gemm_compare
./batched_gemm_compare
# Max abs diff: 0.000e+00
```

启用 `CUBLAS_PEDANTIC_MATH` 时，diff 恰好为零。不启用时（把该行注释掉），在 Ampere 架构/Hopper 上可能看到 `~1e-7` 的差异，因为默认 math mode 允许 tensor core 归约采用不同的累加顺序。

---

## 6. 由一种形式复现另一种形式 —— recipe

### 6.1 循环单次 → 跨步批量

当已有可用的 `Gemm` 循环并想将其融合时：

```c++
// Before (loop):
for (int b = 0; b < B; ++b) {
    cublasSgemm(..., A + b*sA, ..., B + b*sB, ..., C + b*sC);
}

// After (strided batched):
cublasSgemmStridedBatched(...,
                          A, lda, sA,
                          B, ldb, sB,
                          C, ldc, sC,
                          B);
```

条件：每个批元素具有相同的 `m,n,k,lda,ldb,ldc` 以及相同的 alpha/beta。若其中任何一项随批变化，必须使用指针数组形式，或在循环中按每次调用的正确取值调用 `Gemm`。

### 6.2 跨步批量 → 指针数组（当大小不同时）

当各批大小相同但指针分散时：

```c++
const float *Aarray[B];
const float *Barray[B];
float       *Carray[B];
for (int b = 0; b < B; ++b) {
    Aarray[b] = my_A_pool[b];   // wherever each matrix actually lives
    Barray[b] = my_B_pool[b];
    Carray[b] = my_C_pool[b];
}
// Aarray must live on device too — copy to a device array first
cudaMemcpyAsync(d_Aarray, Aarray, sizeof(Aarray), cudaMemcpyHostToDevice);
// ... same for Barray, Carray

cublasSgemmBatched(handle, ...,
                   d_Aarray, lda,
                   d_Barray, ldb,
                   d_Carray, ldc,
                   B);
```


<details>
<summary>English original</summary>

**5. Minimal Worked Example — Single vs Strided Batched**

We'll verify `GemmStridedBatched` produces the same output as a loop of `Gemm` calls, on three identical 4×4 matrices.

```c++
#include <cublas_v2.h>
#include <cuda_runtime.h>
#include <cstdio>
#include <cmath>

int main() {
    const int M = 4, N = 4, K = 4, BATCH = 3;
    const size_t SIZE = M * K * BATCH * sizeof(float);

    // Host data: three 4×4 matrices A, B, C; same data for clarity.
    float h_A[M * K * BATCH], h_B[K * N * BATCH], h_C_single[M * N * BATCH], h_C_batch[M * N * BATCH];
    for (int i = 0; i < M * K * BATCH; ++i) h_A[i] = (i % 7) * 0.1f;
    for (int i = 0; i < K * N * BATCH; ++i) h_B[i] = (i % 5) * 0.2f;

    // Device pointers
    float *d_A, *d_B, *d_C_single, *d_C_batch;
    cudaMalloc(&d_A, SIZE);  cudaMalloc(&d_B, SIZE);
    cudaMalloc(&d_C_single, SIZE);  cudaMalloc(&d_C_batch, SIZE);
    cudaMemcpy(d_A, h_A, SIZE, cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, SIZE, cudaMemcpyHostToDevice);
    cudaMemset(d_C_single, 0, SIZE);
    cudaMemset(d_C_batch,  0, SIZE);

    cublasHandle_t handle;
    cublasCreate(&handle);
    cublasSetMathMode(handle, CUBLAS_PEDANTIC_MATH);    // force deterministic path

    const float alpha = 1.0f, beta = 0.0f;

    // 1) Looped single GEMM
    for (int b = 0; b < BATCH; ++b) {
        cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N,
                    M, N, K,
                    &alpha,
                    d_A + b * M * K, M,
                    d_B + b * K * N, K,
                    &beta,
                    d_C_single + b * M * N, M);
    }

    // 2) One strided batched GEMM
    cublasSgemmStridedBatched(handle,
                              CUBLAS_OP_N, CUBLAS_OP_N,
                              M, N, K,
                              &alpha,
                              d_A, M, (long long)(M * K),     // strideA
                              d_B, K, (long long)(K * N),     // strideB
                              &beta,
                              d_C_batch, M, (long long)(M * N), // strideC
                              BATCH);

    cudaMemcpy(h_C_single, d_C_single, SIZE, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_C_batch,  d_C_batch,  SIZE, cudaMemcpyDeviceToHost);

    // Compare
    float max_diff = 0.0f;
    for (int i = 0; i < M * N * BATCH; ++i) {
        max_diff = fmaxf(max_diff, fabsf(h_C_single[i] - h_C_batch[i]));
    }
    printf("Max abs diff: %.3e\n", max_diff);
    // Expected: 0.0 with PEDANTIC_MATH, ~1e-7 otherwise.

    cublasDestroy(handle);
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C_single); cudaFree(d_C_batch);
}
```

Compile and run:

```bash
nvcc -lcublas batched_gemm_compare.cu -o batched_gemm_compare
./batched_gemm_compare
# Max abs diff: 0.000e+00
```

With `CUBLAS_PEDANTIC_MATH`, the diff is exactly zero. Without it (commenting that line out), you may see `~1e-7` differences on Ampere/Hopper because the default math mode allows tensor-core reduction with different accumulation orders.

---

**6. Reproducing One Form From Another — The Recipe**

**6.1 Looped single → strided batched**

When you have a working `Gemm` loop and want to fuse:

```c++
// Before (loop):
for (int b = 0; b < B; ++b) {
    cublasSgemm(..., A + b*sA, ..., B + b*sB, ..., C + b*sC);
}

// After (strided batched):
cublasSgemmStridedBatched(...,
                          A, lda, sA,
                          B, ldb, sB,
                          C, ldc, sC,
                          B);
```

Conditions: every batch element has same `m,n,k,lda,ldb,ldc` and same alpha/beta. If any of these vary per batch, you must use the pointer-array form or call `Gemm` in a loop with the right per-call values.

**6.2 Strided batched → array of pointers (when sizes vary)**

When the per-batch sizes are the same but pointers are scattered:

```c++
const float *Aarray[B];
const float *Barray[B];
float       *Carray[B];
for (int b = 0; b < B; ++b) {
    Aarray[b] = my_A_pool[b];   // wherever each matrix actually lives
    Barray[b] = my_B_pool[b];
    Carray[b] = my_C_pool[b];
}
// Aarray must live on device too — copy to a device array first
cudaMemcpyAsync(d_Aarray, Aarray, sizeof(Aarray), cudaMemcpyHostToDevice);
// ... same for Barray, Carray

cublasSgemmBatched(handle, ...,
                   d_Aarray, lda,
                   d_Barray, ldb,
                   d_Carray, ldc,
                   B);
```

</details>

### 6.3 单个大 GEMM → 批处理（当输入“实际上就是一个矩阵”时）

有时你的数据是一个单独的 `[M, B·N]` 矩阵，其各列在语义上属于不同的“批”。如果同一个 `A` 与它们全部相乘：

```
single:  C[M, B·N] = A[M, K] @ X[K, B·N]
```

没有理由使用 batched GEMM——这就是一个 N 更大的大型 GEMM，而张量核心很喜欢它。**这是 LLM 推理服务最高效的模式**——含 B 条序列的 batched decode（逐 token 生成阶段）变成一个大的 GEMM，而不是 B 个小的。

要避免的错误：把它写成配合 `batchCount=B` 的 `cublasSgemmBatched`。你白白付出启动开销和间接寻址开销；数学上它就是一个 GEMM。

---

## 7. 张量核心与底层发生的变化

cuBLAS 会在两者之间自动选择：

* **CUDA 核心路径**——FP32 FMA，若固定算法则累加顺序是确定性的。
* **Tensor Core 路径**——`mma.sync` 指令，FP16 乘 + FP32 累加，跨 warp 归约。

Tensor Core 路径在以下情况下启用：

1. `computeType` 是 `CUDA_R_16F`、`CUDA_R_32F`（在 Ampere 架构及以上配合 TF32）或量化类型之一。
2. 各维度是矩阵乘累加分块的整数倍（Ampere 架构：FP16 为 16×16×16；Hopper：WGMMA 为 16×16×16 或 64×16×16）。
3. 满足指针对齐（最少 16 字节，通常 128 字节）。
4. Math mode 不是 `CUBLAS_PEDANTIC_MATH`。

显式开启/关闭的方式为：

```c++
cublasSetMathMode(handle, CUBLAS_TF32_TENSOR_OP_MATH);  // allow TF32 on tensor cores
cublasSetMathMode(handle, CUBLAS_DEFAULT_MATH);         // let cuBLAS choose
cublasSetMathMode(handle, CUBLAS_PEDANTIC_MATH);        // strict IEEE, no tensor cores for FP32
```

对于 Qwen 推理：**始终使用张量核心**。FP32 累加器能兜住精度损失；吞吐提升为 5–10×。唯一需要禁用张量核心的场景是调试，或做逐位精确的数值可复现性检查。

---

## 8. PyTorch 对应关系

每个 PyTorch 算子底下的 cuBLAS 调用：

| PyTorch | 可能的 cuBLAS 调用 | 备注 |
|---|---|---|
| 针对 2D 的 `torch.matmul(A, B)` | `cublasGemmEx` | 若形状对矩阵乘累加友好 |
| 针对 3D 批的 `torch.matmul(A, B)` | `cublasGemmStridedBatchedEx` | 批维度必须连续 |
| `torch.bmm(A, B)` | `cublasGemmStridedBatchedEx` | 与 3D 矩阵乘相同；旧版别名 |
| `F.linear(x, W)` | `cublasGemmEx`（计算 `x @ W^T`） | LLM 中调用最频繁的算子之一 |
| `torch.einsum("bnd,bmd->bnm", q, k)` | Strided batched | 常见于 attention 参考实现 |
| `torch.nn.functional.scaled_dot_product_attention` | 自定义（FlashAttention 或内存高效版） | 非纯 cuBLAS |

若想查看 Jetson / 独立 GPU 上的 dispatch：

```bash
TORCH_SHOW_CPP_STACKTRACES=1 python -c "
import torch
A = torch.randn(4, 128, 64, device='cuda', dtype=torch.float16)
B = torch.randn(4, 64, 256, device='cuda', dtype=torch.float16)
torch.cuda.synchronize()
import torch.profiler as p
with p.profile(activities=[p.ProfilerActivity.CUDA], record_shapes=True) as prof:
    C = torch.bmm(A, B)
    torch.cuda.synchronize()
print(prof.key_averages().table(sort_by='cuda_time_total', row_limit=10))
"
```

应能看到类似 `aten::bmm` → `cublasGemmStridedBatchedEx_internal` 的内容。

---

## 9. Qwen 推理：各形式落在何处

Orin 上 **Qwen3-4B 在 seq_len = 512 时的 prefill（首字前的整段计算）** 的具体映射：

| 算子 | 形式 | 形状（cuBLAS 视角） |
|---|---|---|
| QKV 投影（融合） | 单个 GEMM | `M=6144, N=512, K=2560` |
| Attention score `Q @ K^T` | Strided batched（或 FlashAttention） | batch=32（heads），`M=512, N=512, K=128` |
| Softmax | 自定义 kernel（非 GEMM） | — |
| Attention value `S @ V` | Strided batched（或 FlashAttention） | batch=32，`M=512, N=128, K=512` |
| 输出投影 W_O | 单个 GEMM | `M=2560, N=512, K=4096` |
| FFN gate、up（融合） | 单个 GEMM | `M=13824, N=512, K=2560` |
| FFN down | 单个 GEMM | `M=2560, N=512, K=6912` |
| LM head（仅最后一层） | 单个 GEMM | `M=151936, N=512, K=2560` |

对于 **Qwen2.5-72B 在 seq_len = 4096、TP=4 时的 prefill**：结构相同，维度按比例放大。每个 TP rank 上的 QKV 投影为 `M=2560 (sharded), N=4096, K=8192`。都是大 GEMM；H100 的 prefill 时间大部分花在 cuBLAS 调用上。

对于 **batched decode（连续批处理）**：所有矩阵的 N 都等于有效批大小（活跃序列数之和）。都是单个 GEMM，不需要 batched 形式。

在现代 Qwen 推理路径中，batched-GEMM API 真正出现的唯一地方是**参考 attention 实现**（不带 FlashAttention 的 PyTorch eager 模式）。FlashAttention 的自定义 kernel 完成了 `Q @ K^T` 和 `S @ V` 所做的事，但采用分块方式。

---


<details>
<summary>English original</summary>

**6.3 Single big GEMM → batched (when input is "actually one matrix")**

Sometimes your data is a single `[M, B·N]` matrix where columns belong to different "batches" semantically. If the same `A` multiplies all of them:

```
single:  C[M, B·N] = A[M, K] @ X[K, B·N]
```

There's no reason to use batched GEMM — this is one large GEMM with bigger N, and tensor cores love it. **This is the most efficient regime for LLM serving** — batched decode with B sequences becomes one big GEMM, not B small ones.

The mistake to avoid: writing this as `cublasSgemmBatched` with `batchCount=B`. You pay launch and indirection overhead for nothing; the math is one GEMM.

---

**7. Tensor Cores and What Changes Under the Hood**

cuBLAS auto-selects between:

* **CUDA core path** — FP32 FMAs, deterministic accumulation order if you fix the algorithm.
* **Tensor core path** — `mma.sync` instructions, FP16 multiply + FP32 accumulate, reduction across a warp.

The tensor core path is enabled when:

1. `computeType` is one of `CUDA_R_16F`, `CUDA_R_32F` (with TF32 on Ampere+), or quantized types.
2. Dimensions are multiples of the MMA tile (Ampere: 16×16×16 for FP16; Hopper: 16×16×16 or 64×16×16 for WGMMA).
3. Pointer alignment is met (16-byte minimum, often 128-byte).
4. Math mode isn't `CUBLAS_PEDANTIC_MATH`.

You opt in/out explicitly with:

```c++
cublasSetMathMode(handle, CUBLAS_TF32_TENSOR_OP_MATH);  // allow TF32 on tensor cores
cublasSetMathMode(handle, CUBLAS_DEFAULT_MATH);         // let cuBLAS choose
cublasSetMathMode(handle, CUBLAS_PEDANTIC_MATH);        // strict IEEE, no tensor cores for FP32
```

For Qwen inference: **always use tensor cores**. The FP32 accumulator catches the precision loss; the throughput gain is 5–10×. The only time you'd disable tensor cores is for debugging or for bit-exact numerical reproducibility checks.

---

**8. PyTorch Equivalents**

The cuBLAS calls under each PyTorch op:

| PyTorch | Likely cuBLAS call | Notes |
|---|---|---|
| `torch.matmul(A, B)` for 2D | `cublasGemmEx` | If shapes are MMA-friendly |
| `torch.matmul(A, B)` for 3D batch | `cublasGemmStridedBatchedEx` | Batch dim must be contiguous |
| `torch.bmm(A, B)` | `cublasGemmStridedBatchedEx` | Same as 3D matmul; legacy alias |
| `F.linear(x, W)` | `cublasGemmEx` (computes `x @ W^T`) | One of the most-called ops in LLMs |
| `torch.einsum("bnd,bmd->bnm", q, k)` | Strided batched | Common in attention reference impls |
| `torch.nn.functional.scaled_dot_product_attention` | Custom (FlashAttention or memory-efficient) | Not pure cuBLAS |

If you want to inspect the dispatch on Jetson / discrete GPU:

```bash
TORCH_SHOW_CPP_STACKTRACES=1 python -c "
import torch
A = torch.randn(4, 128, 64, device='cuda', dtype=torch.float16)
B = torch.randn(4, 64, 256, device='cuda', dtype=torch.float16)
torch.cuda.synchronize()
import torch.profiler as p
with p.profile(activities=[p.ProfilerActivity.CUDA], record_shapes=True) as prof:
    C = torch.bmm(A, B)
    torch.cuda.synchronize()
print(prof.key_averages().table(sort_by='cuda_time_total', row_limit=10))
"
```

You should see something like `aten::bmm` → `cublasGemmStridedBatchedEx_internal`.

---

**9. Qwen Inference: Where Each Form Lands**

Concrete mapping for **Qwen3-4B prefill at seq_len = 512** on Orin:

| Op | Form | Shape (cuBLAS view) |
|---|---|---|
| QKV projection (fused) | Single GEMM | `M=6144, N=512, K=2560` |
| Attention score `Q @ K^T` | Strided batched (or FlashAttention) | batch=32 (heads), `M=512, N=512, K=128` |
| Softmax | Custom kernel (not GEMM) | — |
| Attention value `S @ V` | Strided batched (or FlashAttention) | batch=32, `M=512, N=128, K=512` |
| Output projection W_O | Single GEMM | `M=2560, N=512, K=4096` |
| FFN gate, up (fused) | Single GEMM | `M=13824, N=512, K=2560` |
| FFN down | Single GEMM | `M=2560, N=512, K=6912` |
| LM head (final layer only) | Single GEMM | `M=151936, N=512, K=2560` |

For **Qwen2.5-72B prefill at seq_len = 4096, TP=4**: same structure, dimensions scaled. The QKV projection on each TP rank is `M=2560 (sharded), N=4096, K=8192`. Big GEMMs; the H100 spends most of its prefill time in cuBLAS calls.

For **batched decode (continuous batching)**: all matrices have N = effective batch size (sum of active sequences). Single GEMMs, no batched form needed.

The only place batched-GEMM API actually appears in the modern Qwen inference path is in **reference attention implementations** (PyTorch eager mode without FlashAttention). FlashAttention's custom kernel does what `Q @ K^T` and `S @ V` would do, but tiled.

---

</details>

## 10. 调试 recipe ——“为什么我的数值不对？”

当一次 batched-GEMM 调用给出的输出与循环参考实现不一致时，按顺序逐项排查：

1. **打印 shape。** `m, n, k` 是否与你想的一致？出现 off-by-one 就很可疑。
2. **打印每个操作数的前 4×4 块。** 它和你的参考实现一致吗？
3. **强制 `CUBLAS_PEDANTIC_MATH`。** 如果 bug 消失，那问题就是算法选择的非确定性——并非真正的 bug。
4. **设置 `beta = 0` 并预先将 `C` 置零。** 排除“垃圾输入”这一情况。
5. **对 strided 形式尝试 `batchCount = 1`。** 应当与单次 `Sgemm` 调用完全一致。
6. **手工计算 M=N=K=2 来检查 transpose 标志。** 2×2 的例子能立刻定位 transpose bug。
7. **检查 stride。** 在 column-major 下，非转置 `A` 的 `strideA = lda · k`。**偏差 `lda · k vs lda · m` 是常见错误。**
8. 在 array-of-pointers 形式中**检查指针偏移**。打印 device 指针；与手工 `base + i * stride` 做 diff。

如果以上各项全部通过而数值仍然不同——那就是真正的 bug。通常出在 #7 或 #8。

---

## 11. 动手练习

1. **位精确验证。** 编译 §5 的示例。分别在开启与关闭 `CUBLAS_PEDANTIC_MATH` 的情况下运行它。记录每种情况下的 max-diff。然后在输出上叠加 `cudaDeviceProp.major`，并在两种不同的 GPU 架构上运行（例如 Orin 和桌面级 RTX）。报告跨架构的可复现性。

2. **Prefill（首字前的整段计算）GEMM benchmark。** 取 Qwen3-4B prefill 的 QKV projection 尺寸（`M=6144, K=2560`），针对 `N ∈ {1, 4, 16, 64, 256, 1024}` 做 benchmark。绘制 tok/s 与 GFLOPS。找出 tensor-core 利用率饱和处的 N。

3. **把 strided-batched 作为 attention 参考实现的唯一形式。** 用纯 cuBLAS 实现 attention `Q @ K^T → softmax → @ V`——两次 `GemmStridedBatched` 调用，外加一个你自选的 softmax kernel。在同一组输入上，将输出与 PyTorch 的 `F.scaled_dot_product_attention` 对比。

4. **transpose 之舞。** 取一个 row-major 的 PyTorch 张量 `X[128, 256]` 和一个 `W[256, 512]`。用两种方式计算 `Y = X @ W`：（a）通过 PyTorch，（b）通过采用 swap-arguments 惯用法的 cuBLAS `Sgemm`。验证 max-diff < 1e-4。

5. **strided 与 pointer-array 的对比 benchmark。** 对于由 64 个批处理的 64×64 GEMM 组成的工作负载，对 `GemmStridedBatched` 与 `GemmBatched` 做 benchmark（其中指针数组由你自己填充）。量化间接寻址的开销。对批大小 4 和批大小 1024 重复测试。

6. **PyTorch dispatch 检查。** 在 Qwen attention 中实际会出现的 shape 上（batch=32、M=512、K=128、N=512）对 `torch.bmm` 做 profile。通过 `nsys` 确认 PyTorch 分派到了 `cublasGemmStridedBatchedEx`。然后切换到 `torch.nn.functional.scaled_dot_product_attention`，观察是否改为调用 FlashAttention。

7. **Tensor Core 对齐。** 分别在 `N = 256`（整齐分块）与 `N = 251`（不整齐）下运行 QKV projection GEMM。测量 tok/s。确认不整齐的 N 明显更慢，因为 cuBLAS 无法对尾部分块使用张量核心。

---

## 12. 关键要点

| 要点 | 为何重要 |
|---|---|
| `GemmStridedBatched` 是 95% 的推理所采用的形式 | 相同的 alpha/beta、相同的 shape、连续的 stride——与 LLM attention 精确契合 |
| `GemmBatched`（指针数组）适用于分散的、大小相同的矩阵 | 需付出间接寻址代价；仅在 strided 形式不可用时使用 |
| cuBLAS 是 column-major；swap-arguments 惯用法处理 row-major 输入 | 布局类 bug 最常见的单一来源 |
| 只要 C 是新输出就设置 `beta = 0` | 经典的自伤陷阱：“结果里混进了随机 GPU 显存” |
| `CUBLAS_PEDANTIC_MATH` 是你的位精确调试开关 | 禁用张量核心与算法选择——慢但具有确定性 |
| 批处理推理服务是一个大 GEMM，而不是许多小 GEMM | 当你已有连续的拼接批时，不要动辄使用 batched API |
| 对 attention 而言，生产环境中 FlashAttention 已取代显式 batched GEMM | 参考实现仍会走它；现代推理服务则不会 |
| 八个调试 recipe 步骤能抓住 >90% 的“输出错误”bug | stride 与 transpose 错误是大家的最爱 |

---


<details>
<summary>English original</summary>

**10. Debugging Recipe — "Why Are My Numbers Wrong?"**

When a batched-GEMM call gives an output that doesn't match the looped reference, work through these in order:

1. **Print shapes.** Are `m, n, k` what you think? Off-by-one is suspicious.
2. **Print first 4×4 block of each operand.** Does it match your reference?
3. **Force `CUBLAS_PEDANTIC_MATH`.** If the bug goes away, your problem was algorithm-selection non-determinism — not actually a bug.
4. **Set `beta = 0` and pre-zero `C`.** Removes the "garbage input" case.
5. **Try `batchCount = 1` of the strided form.** Should match the single `Sgemm` call exactly.
6. **Check transpose flags by computing M=N=K=2 by hand.** A 2×2 example pinpoints transpose bugs immediately.
7. **Check strides.** `strideA = lda · k` for non-transposed `A` in column-major. **Off by `lda · k vs lda · m` is a frequent mistake.**
8. **Check pointer offsets** in the array-of-pointers form. Print the device pointers; diff against manual `base + i * stride`.

If all of these check out and the numbers still differ — you have a real bug. Usually it's #7 or #8.

---

**11. Hands-On Exercises**

1. **Bit-exact validation.** Compile the §5 example. Run it with `CUBLAS_PEDANTIC_MATH` on and off. Record the max-diff in each case. Then add `cudaDeviceProp.major` to the output and run on two different GPU architectures (e.g., Orin and a desktop RTX). Report cross-arch reproducibility.

2. **Prefill GEMM benchmarking.** Take Qwen3-4B prefill QKV projection sizes (`M=6144, K=2560`) and benchmark for `N ∈ {1, 4, 16, 64, 256, 1024}`. Plot tok/s and GFLOPS. Identify the N where tensor-core utilization saturates.

3. **Strided-batched as the only form for attention reference.** Implement the attention `Q @ K^T → softmax → @ V` in pure cuBLAS — two `GemmStridedBatched` calls plus a softmax kernel of your choice. Compare output to PyTorch's `F.scaled_dot_product_attention` on the same inputs.

4. **The transpose dance.** Take a row-major PyTorch tensor `X[128, 256]` and a `W[256, 512]`. Compute `Y = X @ W` two ways: (a) via PyTorch, (b) via cuBLAS `Sgemm` with the swap-arguments idiom. Verify max-diff is < 1e-4.

5. **Strided vs pointer-array benchmark.** For a workload of 64 batched 64×64 GEMMs, benchmark `GemmStridedBatched` vs `GemmBatched` (where you populate the pointer array yourself). Quantify the indirection overhead. Repeat for batch size 4 and batch size 1024.

6. **PyTorch dispatch inspection.** Profile `torch.bmm` on shapes you'd actually see in Qwen attention (batch=32, M=512, K=128, N=512). Confirm via `nsys` that PyTorch dispatches to `cublasGemmStridedBatchedEx`. Then switch to `torch.nn.functional.scaled_dot_product_attention` and observe FlashAttention being called instead.

7. **Tensor core alignment.** Run the QKV projection GEMM at `N = 256` (clean tile) and `N = 251` (awkward). Measure tok/s. Confirm the awkward N is significantly slower because cuBLAS can't use tensor cores for the trailing tile.

---

**12. Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| `GemmStridedBatched` is the form 95% of inference uses | Same alpha/beta, same shapes, contiguous strides — fits LLM attention exactly |
| `GemmBatched` (pointer array) is for scattered, same-size matrices | Pay indirection cost; only use when strided form isn't available |
| cuBLAS is column-major; the swap-arguments idiom handles row-major input | The single most common source of layout bugs |
| Set `beta = 0` whenever C is fresh output | The classic "I added random GPU memory" footgun |
| `CUBLAS_PEDANTIC_MATH` is your bit-exact debug button | Disables tensor cores and algorithm selection — slow but deterministic |
| Batched serving is one big GEMM, not many small ones | Don't reach for batched API when you have a contiguous concat batch |
| For attention, FlashAttention has replaced explicit batched GEMM in production | Reference implementations still go through it; modern serving doesn't |
| Eight debug-recipe steps catch >90% of "wrong output" bugs | Stride and transpose mistakes are everyone's favorite |

---

</details>

## 资源

* **[cuBLAS 库用户指南](https://docs.nvidia.com/cuda/cublas/index.html):** 涵盖所有 `Gemm*` 变体的参考资料。
* **[cuBLAS 示例 — strided batched GEMM（矩阵-矩阵乘）](https://github.com/NVIDIA/cuda-samples/tree/master/Samples/4_CUDA_Libraries/batchCUBLAS):** 同时使用 `Batched` 和 `StridedBatched` 的最小可运行示例。
* **[CUTLASS — 面向 GEMM 的可组合模板](https://github.com/NVIDIA/cutlass):** 当 cuBLAS 不够灵活时；模板化的分块级 GEMM，可完全控制。
* **[Marlin GPTQ kernel](https://github.com/IST-DASLab/marlin):** vLLM 使用的现代 4-bit GEMM；并非纯 cuBLAS，但遵循相同的形状约定。
* **["Efficient GEMM in Modern Architectures" — 矩阵乘法剖析](https://arxiv.org/abs/1808.07984):** 分块级 GEMM 设计的背景知识。
* **[NVIDIA 数学模式参考](https://docs.nvidia.com/cuda/cublas/index.html#cublasmath_t):** TF32、FP16、BF16、PEDANTIC 模式的说明。
* **[PyTorch `torch.bmm` 文档](https://pytorch.org/docs/stable/generated/torch.bmm.html):** 面向用户的批处理矩阵乘；会分派到 cuBLAS。
* **[阶段 5 — 边缘大语言模型推理内部机制](../03-边缘LLM推理内部机制/Lecture-01.md):** 本 GEMM 讲座的 GEMV（矩阵-向量乘）侧配套篇。
* **[阶段 4 — 方向 C — Kernel 工程](../../../阶段4-路线C-深度学习推理优化/02-内核工程/Guide.md):** 从编译器侧面更深入地审视 GEMM kernel 设计。


<details>
<summary>English original</summary>

**Resources**

* **[cuBLAS Library User Guide](https://docs.nvidia.com/cuda/cublas/index.html):** Reference for all the `Gemm*` variants.
* **[cuBLAS sample — strided batched GEMM](https://github.com/NVIDIA/cuda-samples/tree/master/Samples/4_CUDA_Libraries/batchCUBLAS):** Minimal working example with both `Batched` and `StridedBatched`.
* **[CUTLASS — Composable Templates for GEMM](https://github.com/NVIDIA/cutlass):** When cuBLAS isn't flexible enough; templated tile-level GEMM with full control.
* **[Marlin GPTQ kernel](https://github.com/IST-DASLab/marlin):** Modern 4-bit GEMM used by vLLM; not pure cuBLAS but follows the same shape conventions.
* **["Efficient GEMM in Modern Architectures" — Anatomy of Matrix Multiplication](https://arxiv.org/abs/1808.07984):** Background on tile-level GEMM design.
* **[NVIDIA Math Mode Reference](https://docs.nvidia.com/cuda/cublas/index.html#cublasmath_t):** TF32, FP16, BF16, PEDANTIC modes explained.
* **[PyTorch `torch.bmm` documentation](https://pytorch.org/docs/stable/generated/torch.bmm.html):** The user-facing batched matmul; dispatches to cuBLAS.
* **[Phase 5 — Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md):** The GEMV-side companion to this GEMM lecture.
* **[Phase 4 — Track C — Kernel Engineering](../../../阶段4-路线C-深度学习推理优化/02-内核工程/Guide.md):** Deeper compiler-side view of GEMM kernel design.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Qwen Inference Optimization/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Qwen%20Inference%20Optimization/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
