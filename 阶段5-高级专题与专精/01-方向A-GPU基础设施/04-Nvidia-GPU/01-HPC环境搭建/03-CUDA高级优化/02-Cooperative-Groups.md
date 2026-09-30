---
title: 02 — Cooperative Groups
description: 02 — Cooperative Groups
---

# 02 — Cooperative Groups

## 1. 问题：CUDA 的同步壁垒

传统 CUDA 只在单个 thread block 内做同步：

```
__global__ void naive_reduction(float* data, float* result) {
    __shared__ float sdata[256];
    int tid = threadIdx.x;
    sdata[tid] = data[blockIdx.x * 256 + tid];
    __syncthreads();   // only syncs 256 threads in this block

    // Reduction within block...
    // But to get the GLOBAL sum, you need another kernel launch!
}
```

其局限：

```
Thread block size limit: 1024 threads
GPU SM count: 108 (H100/H200)
Concurrent threads: 108 × 2048 active = ~220,000 threads

To sync ALL threads: you MUST launch a second kernel
                     which requires CPU involvement + kernel overhead
```

**Cooperative Groups** 打破了这道壁垒：在多个层级上提供可编程同步 —— warp 内、block 内、乃至整个 grid —— 全程无需退出 kernel。

---

## 2. Cooperative Groups 的层级

```
GRID (all blocks in a kernel launch)
│
├── THREAD BLOCK (256–1024 threads, one SM)
│   │
│   ├── WARP (32 threads, execute in lockstep)
│   │   │
│   │   └── TILED PARTITION (8, 16, or 32 threads)
│   │
│   └── COALESCED GROUP (active threads in a warp)
│
└── MULTI-GRID (multiple kernels, rarely used)
```

每一层都有自己的 `sync()`、shuffle 操作和元数据。

---

## 3. Thread Block Group（drop-in 替换 `__syncthreads()`）

```cpp
#include <cooperative_groups.h>
namespace cg = cooperative_groups;

__global__ void block_sync_example(float* data) {
    // Get handle to current thread block
    cg::thread_block block = cg::this_thread_block();

    __shared__ float sdata[1024];
    int tid = threadIdx.x;

    sdata[tid] = data[blockIdx.x * blockDim.x + tid];

    // Equivalent to __syncthreads() but more readable + composable
    block.sync();

    // Can also get metadata
    unsigned size   = block.size();          // blockDim.x * blockDim.y * blockDim.z
    unsigned rank   = block.thread_rank();   // unique ID within block (0..size-1)
    dim3     tidx   = block.thread_index();  // = threadIdx
    dim3     bidx   = block.group_index();   // = blockIdx
}
```

相比裸用 `__syncthreads()` 的优势：该 group 是一个**一等对象**，可以传给函数。

```cpp
// Pass the sync group to helper functions — impossible with __syncthreads()
__device__ void reduce_in_block(cg::thread_block& block, float* sdata) {
    for (int s = block.size() / 2; s > 0; s >>= 1) {
        if (block.thread_rank() < s)
            sdata[block.thread_rank()] += sdata[block.thread_rank() + s];
        block.sync();   // proper sync within the group
    }
}
```

---

## 4. Tiled Partition —— 把 block 划分为子组

```cpp
__global__ void tiled_example(float* data) {
    cg::thread_block block = cg::this_thread_block();

    // Partition block into groups of 32 (warp-sized)
    cg::thread_block_tile<32> warp = cg::tiled_partition<32>(block);

    // Or partition into groups of 16, 8, 4, or 2
    cg::thread_block_tile<16> half_warp = cg::tiled_partition<16>(block);
    cg::thread_block_tile<4>  quad      = cg::tiled_partition<4>(block);

    // Each tile syncs independently
    float val = data[threadIdx.x];

    // Warp-level reduction using shuffle within tile
    for (int offset = warp.size() / 2; offset > 0; offset >>= 1) {
        val += warp.shfl_down(val, offset);
    }

    // Only thread 0 of each warp has the full sum
    if (warp.thread_rank() == 0)
        atomicAdd(&result, val);   // one atomic per warp (not per thread!)
}
```

对于 warp 级归约，tiled partition 的 shuffle 比共享内存**快得多**：
- 无需分配 `__shared__`
- 无 bank conflict
- 1 cycle 延迟（共享内存需 4+）

---

## 5. Coalesced Groups —— 处理 warp 分歧

当 warp 中的线程走不同分支（diverge）时，部分线程处于非活跃状态。Coalesced groups 会创建一个**只含活跃线程**的 group：

```cpp
__global__ void divergent_kernel(float* data, float* result, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;  // some threads exit early → divergence

    // Get a group of only threads that are still active here
    cg::coalesced_group active = cg::coalesced_threads();

    float val = data[idx];

    // Reduce across ONLY the active threads in this warp
    // (no waste from inactive thread slots)
    for (int offset = active.size() / 2; offset > 0; offset >>= 1) {
        val += active.shfl_down(val, offset);
    }

    if (active.thread_rank() == 0) {
        atomicAdd(result, val);
    }
}
```

若没有 coalesced groups，warp 分歧会迫使非活跃线程串行执行。Coalesced groups 把**活跃线程打包**起来并高效执行。

---



---


<details>
<summary>English original</summary>

**02 — Cooperative Groups**

**1. The Problem: CUDA's Synchronization Wall**

Traditional CUDA synchronizes only within a thread block:

```
__global__ void naive_reduction(float* data, float* result) {
    __shared__ float sdata[256];
    int tid = threadIdx.x;
    sdata[tid] = data[blockIdx.x * 256 + tid];
    __syncthreads();   // only syncs 256 threads in this block

    // Reduction within block...
    // But to get the GLOBAL sum, you need another kernel launch!
}
```

The limitation:

```
Thread block size limit: 1024 threads
GPU SM count: 108 (H100/H200)
Concurrent threads: 108 × 2048 active = ~220,000 threads

To sync ALL threads: you MUST launch a second kernel
                     which requires CPU involvement + kernel overhead
```

**Cooperative Groups** break this wall by providing programmable synchronization at multiple hierarchical levels — within a warp, across a block, and across the entire grid — without leaving the kernel.

---

**2. The Cooperative Groups Hierarchy**

```
GRID (all blocks in a kernel launch)
│
├── THREAD BLOCK (256–1024 threads, one SM)
│   │
│   ├── WARP (32 threads, execute in lockstep)
│   │   │
│   │   └── TILED PARTITION (8, 16, or 32 threads)
│   │
│   └── COALESCED GROUP (active threads in a warp)
│
└── MULTI-GRID (multiple kernels, rarely used)
```

Each level has its own `sync()`, shuffle operations, and metadata.

---

**3. Thread Block Group (Drop-in `__syncthreads()` replacement)**

```cpp
#include <cooperative_groups.h>
namespace cg = cooperative_groups;

__global__ void block_sync_example(float* data) {
    // Get handle to current thread block
    cg::thread_block block = cg::this_thread_block();

    __shared__ float sdata[1024];
    int tid = threadIdx.x;

    sdata[tid] = data[blockIdx.x * blockDim.x + tid];

    // Equivalent to __syncthreads() but more readable + composable
    block.sync();

    // Can also get metadata
    unsigned size   = block.size();          // blockDim.x * blockDim.y * blockDim.z
    unsigned rank   = block.thread_rank();   // unique ID within block (0..size-1)
    dim3     tidx   = block.thread_index();  // = threadIdx
    dim3     bidx   = block.group_index();   // = blockIdx
}
```

Advantage over raw `__syncthreads()`: the group is a **first-class object** you can pass to functions.

```cpp
// Pass the sync group to helper functions — impossible with __syncthreads()
__device__ void reduce_in_block(cg::thread_block& block, float* sdata) {
    for (int s = block.size() / 2; s > 0; s >>= 1) {
        if (block.thread_rank() < s)
            sdata[block.thread_rank()] += sdata[block.thread_rank() + s];
        block.sync();   // proper sync within the group
    }
}
```

---

**4. Tiled Partition — Divide a Block into Subgroups**

```cpp
__global__ void tiled_example(float* data) {
    cg::thread_block block = cg::this_thread_block();

    // Partition block into groups of 32 (warp-sized)
    cg::thread_block_tile<32> warp = cg::tiled_partition<32>(block);

    // Or partition into groups of 16, 8, 4, or 2
    cg::thread_block_tile<16> half_warp = cg::tiled_partition<16>(block);
    cg::thread_block_tile<4>  quad      = cg::tiled_partition<4>(block);

    // Each tile syncs independently
    float val = data[threadIdx.x];

    // Warp-level reduction using shuffle within tile
    for (int offset = warp.size() / 2; offset > 0; offset >>= 1) {
        val += warp.shfl_down(val, offset);
    }

    // Only thread 0 of each warp has the full sum
    if (warp.thread_rank() == 0)
        atomicAdd(&result, val);   // one atomic per warp (not per thread!)
}
```

The tiled partition shuffle is **much faster** than shared memory for warp-level reductions:
- No `__shared__` allocation
- No bank conflicts
- 1 cycle latency (vs 4+ for shared memory)

---

**5. Coalesced Groups — Handle Warp Divergence**

When threads in a warp take different paths (diverge), some threads are inactive. Coalesced groups create a group of **only the active threads**:

```cpp
__global__ void divergent_kernel(float* data, float* result, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;  // some threads exit early → divergence

    // Get a group of only threads that are still active here
    cg::coalesced_group active = cg::coalesced_threads();

    float val = data[idx];

    // Reduce across ONLY the active threads in this warp
    // (no waste from inactive thread slots)
    for (int offset = active.size() / 2; offset > 0; offset >>= 1) {
        val += active.shfl_down(val, offset);
    }

    if (active.thread_rank() == 0) {
        atomicAdd(result, val);
    }
}
```

Without coalesced groups, warp divergence forces inactive threads to serialize. Coalesced groups **pack active threads** and execute them efficiently.

---

</details>

## 6. Grid Group — 全网格同步

最强大的特性：在**单个 kernel 内部**同步所有 block 中的所有线程。

```cpp
// Requires special launch: cudaLaunchCooperativeKernel
#include <cooperative_groups.h>
namespace cg = cooperative_groups;

__global__ void grid_sync_kernel(float* data, float* partial, float* result) {
    cg::grid_group grid = cg::this_grid();

    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Phase 1: every thread computes its local value
    partial[idx] = compute(data[idx]);

    // === GRID-WIDE SYNCHRONIZATION ===
    grid.sync();   // all blocks across the entire GPU wait here

    // Phase 2: use results from Phase 1 (now safe, all computed)
    if (idx == 0) {
        float total = 0;
        for (int i = 0; i < gridDim.x * blockDim.x; i++)
            total += partial[i];
        *result = total;
    }
}
```

**没有 grid sync：** 需要：
```
kernel_phase1<<<grid, block>>>(data, partial);
cudaDeviceSynchronize();   // CPU round-trip
kernel_phase2<<<grid, block>>>(partial, result);
```

**有 grid sync：** 单个 kernel，无需 CPU 参与，没有 kernel 启动开销。

### Grid Group 启动（C++）

```cpp
// MUST use cooperative launch API — not standard <<<grid, block>>>
void* args[] = {&data, &partial, &result};

// Check device supports cooperative launch
int supportsCoopLaunch;
cudaDeviceGetAttribute(&supportsCoopLaunch,
    cudaDevAttrCooperativeLaunch, device_id);
assert(supportsCoopLaunch == 1);

// Check max blocks for cooperative launch
int numBlocksPerSM;
cudaOccupancyMaxActiveBlocksPerMultiprocessor(
    &numBlocksPerSM, grid_sync_kernel, blockSize, 0);
int numSMs;
cudaDeviceGetAttribute(&numSMs, cudaDevAttrMultiProcessorCount, device_id);
int maxBlocks = numBlocksPerSM * numSMs;

// Launch with cooperative API
cudaLaunchCooperativeKernel(
    (void*)grid_sync_kernel,
    dim3(maxBlocks),   // must not exceed max blocks!
    dim3(blockSize),
    args,
    0, stream
);
```

**关键约束：** grid sync 要求**所有 block 同时驻留在 GPU 上**——启动的 block 数不能超过 GPU 一次可调度的数量。

---

## 7. 通过 Cooperative Groups 使用 warp 级原语

Cooperative groups 以简洁方式暴露 warp intrinsics：

```cpp
__global__ void warp_ops_demo(float* data) {
    auto warp = cg::tiled_partition<32>(cg::this_thread_block());

    float val = data[threadIdx.x];
    int   rank = warp.thread_rank();

    // === SHUFFLE DOWN (reduction) ===
    // Each thread gets val from thread (rank + offset)
    float neighbor = warp.shfl_down(val, 16);   // get from thread+16
    val += neighbor;

    // === SHUFFLE XOR (butterfly reduction) ===
    // Pairs up threads by XOR of their rank
    for (int mask = 16; mask > 0; mask >>= 1)
        val += warp.shfl_xor(val, mask);

    // === BALLOT (which threads satisfy a condition?) ===
    unsigned active_mask = warp.ballot(val > 0.0f);
    // active_mask: bit i = 1 if thread i has val > 0

    // === ANY / ALL (warp-wide predicate) ===
    bool any_positive = warp.any(val > 0.0f);
    bool all_positive = warp.all(val > 0.0f);

    // === MATCH (find threads with same value) ===
    unsigned same_val = warp.match_any(val);
    // same_val: mask of all threads in warp with identical `val`
}
```

---

## 8. 实战示例：用 Grid Group 做并行前缀扫描

前缀扫描（累加和）是基本的并行原语。借助 grid sync，它可以在**单个 kernel** 内完成：

```cpp
__global__ void prefix_scan(float* data, float* output, float* block_sums, int n) {
    auto grid  = cg::this_grid();
    auto block = cg::this_thread_block();
    auto warp  = cg::tiled_partition<32>(block);

    extern __shared__ float sdata[];
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int tid = threadIdx.x;

    // Phase 1: Intra-block scan
    sdata[tid] = (gid < n) ? data[gid] : 0.0f;
    block.sync();

    // Up-sweep (reduce)
    for (int stride = 1; stride < blockDim.x; stride <<= 1) {
        int idx = (tid + 1) * stride * 2 - 1;
        if (idx < blockDim.x)
            sdata[idx] += sdata[idx - stride];
        block.sync();
    }

    // Save block sum
    if (tid == blockDim.x - 1)
        block_sums[blockIdx.x] = sdata[tid];

    // === WAIT FOR ALL BLOCKS TO FINISH PHASE 1 ===
    grid.sync();   // <-- the key grid-wide sync

    // Phase 2: Only block 0 scans the block_sums array
    if (blockIdx.x == 0) {
        // scan block_sums... (single block, no grid sync needed)
    }

    // === WAIT FOR PHASE 2 ===
    grid.sync();

    // Phase 3: Add prefix from block_sums to each element
    float prefix = (blockIdx.x > 0) ? block_sums[blockIdx.x - 1] : 0.0f;
    if (gid < n)
        output[gid] = sdata[tid] + prefix;
}
```

在 grid sync 出现之前：这需要 **三次独立的 kernel 启动**，并在其间用 CPU 同步。

---


<details>
<summary>English original</summary>

**6. Grid Group — Full Grid Synchronization**

The most powerful feature: sync ALL threads across ALL blocks **inside one kernel**.

```cpp
// Requires special launch: cudaLaunchCooperativeKernel
#include <cooperative_groups.h>
namespace cg = cooperative_groups;

__global__ void grid_sync_kernel(float* data, float* partial, float* result) {
    cg::grid_group grid = cg::this_grid();

    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Phase 1: every thread computes its local value
    partial[idx] = compute(data[idx]);

    // === GRID-WIDE SYNCHRONIZATION ===
    grid.sync();   // all blocks across the entire GPU wait here

    // Phase 2: use results from Phase 1 (now safe, all computed)
    if (idx == 0) {
        float total = 0;
        for (int i = 0; i < gridDim.x * blockDim.x; i++)
            total += partial[i];
        *result = total;
    }
}
```

**Without grid sync:** You'd need:
```
kernel_phase1<<<grid, block>>>(data, partial);
cudaDeviceSynchronize();   // CPU round-trip
kernel_phase2<<<grid, block>>>(partial, result);
```

**With grid sync:** Single kernel, no CPU involvement, no kernel launch overhead.

**Grid Group Launch (C++)**

```cpp
// MUST use cooperative launch API — not standard <<<grid, block>>>
void* args[] = {&data, &partial, &result};

// Check device supports cooperative launch
int supportsCoopLaunch;
cudaDeviceGetAttribute(&supportsCoopLaunch,
    cudaDevAttrCooperativeLaunch, device_id);
assert(supportsCoopLaunch == 1);

// Check max blocks for cooperative launch
int numBlocksPerSM;
cudaOccupancyMaxActiveBlocksPerMultiprocessor(
    &numBlocksPerSM, grid_sync_kernel, blockSize, 0);
int numSMs;
cudaDeviceGetAttribute(&numSMs, cudaDevAttrMultiProcessorCount, device_id);
int maxBlocks = numBlocksPerSM * numSMs;

// Launch with cooperative API
cudaLaunchCooperativeKernel(
    (void*)grid_sync_kernel,
    dim3(maxBlocks),   // must not exceed max blocks!
    dim3(blockSize),
    args,
    0, stream
);
```

**Critical constraint:** Grid sync requires that **all blocks fit on the GPU simultaneously** — you cannot launch more blocks than the GPU can schedule at once.

---

**7. Warp-Level Primitives via Cooperative Groups**

Cooperative groups expose warp intrinsics cleanly:

```cpp
__global__ void warp_ops_demo(float* data) {
    auto warp = cg::tiled_partition<32>(cg::this_thread_block());

    float val = data[threadIdx.x];
    int   rank = warp.thread_rank();

    // === SHUFFLE DOWN (reduction) ===
    // Each thread gets val from thread (rank + offset)
    float neighbor = warp.shfl_down(val, 16);   // get from thread+16
    val += neighbor;

    // === SHUFFLE XOR (butterfly reduction) ===
    // Pairs up threads by XOR of their rank
    for (int mask = 16; mask > 0; mask >>= 1)
        val += warp.shfl_xor(val, mask);

    // === BALLOT (which threads satisfy a condition?) ===
    unsigned active_mask = warp.ballot(val > 0.0f);
    // active_mask: bit i = 1 if thread i has val > 0

    // === ANY / ALL (warp-wide predicate) ===
    bool any_positive = warp.any(val > 0.0f);
    bool all_positive = warp.all(val > 0.0f);

    // === MATCH (find threads with same value) ===
    unsigned same_val = warp.match_any(val);
    // same_val: mask of all threads in warp with identical `val`
}
```

---

**8. Practical Example: Parallel Prefix Scan with Grid Groups**

Prefix scan (cumulative sum) is a fundamental parallel primitive. With grid sync, it can be done in **a single kernel**:

```cpp
__global__ void prefix_scan(float* data, float* output, float* block_sums, int n) {
    auto grid  = cg::this_grid();
    auto block = cg::this_thread_block();
    auto warp  = cg::tiled_partition<32>(block);

    extern __shared__ float sdata[];
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int tid = threadIdx.x;

    // Phase 1: Intra-block scan
    sdata[tid] = (gid < n) ? data[gid] : 0.0f;
    block.sync();

    // Up-sweep (reduce)
    for (int stride = 1; stride < blockDim.x; stride <<= 1) {
        int idx = (tid + 1) * stride * 2 - 1;
        if (idx < blockDim.x)
            sdata[idx] += sdata[idx - stride];
        block.sync();
    }

    // Save block sum
    if (tid == blockDim.x - 1)
        block_sums[blockIdx.x] = sdata[tid];

    // === WAIT FOR ALL BLOCKS TO FINISH PHASE 1 ===
    grid.sync();   // <-- the key grid-wide sync

    // Phase 2: Only block 0 scans the block_sums array
    if (blockIdx.x == 0) {
        // scan block_sums... (single block, no grid sync needed)
    }

    // === WAIT FOR PHASE 2 ===
    grid.sync();

    // Phase 3: Add prefix from block_sums to each element
    float prefix = (blockIdx.x > 0) ? block_sums[blockIdx.x - 1] : 0.0f;
    if (gid < n)
        output[gid] = sdata[tid] + prefix;
}
```

Before grid sync: this required **three separate kernel launches** with CPU synchronization between them.

---

</details>

## 9. PyTorch 自定义 CUDA 扩展中的 Cooperative Groups

```cpp
// custom_kernel.cu — used via torch.utils.cpp_extension
#include <torch/extension.h>
#include <cooperative_groups.h>
namespace cg = cooperative_groups;

__global__ void warp_reduce_kernel(
    const float* __restrict__ input,
    float* __restrict__ output,
    int n
) {
    auto warp = cg::tiled_partition<32>(cg::this_thread_block());
    int gid = blockIdx.x * blockDim.x + threadIdx.x;

    float val = (gid < n) ? input[gid] : 0.0f;

    // Warp reduce
    for (int offset = 16; offset > 0; offset >>= 1)
        val += warp.shfl_down(val, offset);

    // One atomic per warp
    if (warp.thread_rank() == 0)
        atomicAdd(output, val);
}

torch::Tensor warp_reduce(torch::Tensor input) {
    auto output = torch::zeros(1, input.options());
    int n = input.numel();
    int threads = 256;
    int blocks  = (n + threads - 1) / threads;
    warp_reduce_kernel<<<blocks, threads>>>(
        input.data_ptr<float>(), output.data_ptr<float>(), n
    );
    return output;
}
```

---

## 10. 何时使用每种 group 类型

| 场景 | 使用 |
|---|---|
| `__syncthreads()` 的直接替换 | `thread_block.sync()` |
| warp 级归约（无 shared mem） | `tiled_partition<32>` + `shfl_down` |
| 子 warp 并行（例如树遍历） | `tiled_partition<N>` (N = 4, 8, 16) |
| 干净地处理发散线程 | `coalesced_threads()` |
| 无需额外 kernel 的两阶段算法 | `this_grid().sync()` |
| 在小 group 内进行广播的算法 | `tiled_partition<N>.shfl()` |
| 统计/查找 warp 中的活跃线程 | `coalesced_threads().size()` |

---

## 参考资料

- [CUDA Cooperative Groups 编程指南](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cooperative-groups)
- [Cooperative Groups 示例 (NVIDIA)](https://github.com/NVIDIA/cuda-samples/tree/master/Samples/3_CUDA_Features/conjugateGradientMultiBlockCG)
- [Warp 级原语](https://developer.nvidia.com/blog/using-cuda-warp-level-primitives/)
- [通过 Cooperative Kernels 实现 Grid 同步](https://developer.nvidia.com/blog/cooperative-groups/)


<details>
<summary>English original</summary>

**9. Cooperative Groups in PyTorch Custom CUDA Extensions**

```cpp
// custom_kernel.cu — used via torch.utils.cpp_extension
#include <torch/extension.h>
#include <cooperative_groups.h>
namespace cg = cooperative_groups;

__global__ void warp_reduce_kernel(
    const float* __restrict__ input,
    float* __restrict__ output,
    int n
) {
    auto warp = cg::tiled_partition<32>(cg::this_thread_block());
    int gid = blockIdx.x * blockDim.x + threadIdx.x;

    float val = (gid < n) ? input[gid] : 0.0f;

    // Warp reduce
    for (int offset = 16; offset > 0; offset >>= 1)
        val += warp.shfl_down(val, offset);

    // One atomic per warp
    if (warp.thread_rank() == 0)
        atomicAdd(output, val);
}

torch::Tensor warp_reduce(torch::Tensor input) {
    auto output = torch::zeros(1, input.options());
    int n = input.numel();
    int threads = 256;
    int blocks  = (n + threads - 1) / threads;
    warp_reduce_kernel<<<blocks, threads>>>(
        input.data_ptr<float>(), output.data_ptr<float>(), n
    );
    return output;
}
```

---

**10. When to Use Each Group Type**

| Situation | Use |
|---|---|
| Drop-in `__syncthreads()` replacement | `thread_block.sync()` |
| Warp-level reduction (no shared mem) | `tiled_partition<32>` + `shfl_down` |
| Sub-warp parallelism (e.g., tree traversal) | `tiled_partition<N>` (N = 4, 8, 16) |
| Handle divergent threads cleanly | `coalesced_threads()` |
| Two-phase algorithm without extra kernel | `this_grid().sync()` |
| Algorithm with broadcast within small group | `tiled_partition<N>.shfl()` |
| Count/find active threads in warp | `coalesced_threads().size()` |

---

**References**

- [CUDA Cooperative Groups Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cooperative-groups)
- [Cooperative Groups Examples (NVIDIA)](https://github.com/NVIDIA/cuda-samples/tree/master/Samples/3_CUDA_Features/conjugateGradientMultiBlockCG)
- [Warp-Level Primitives](https://developer.nvidia.com/blog/using-cuda-warp-level-primitives/)
- [Grid-Synchronization via Cooperative Kernels](https://developer.nvidia.com/blog/cooperative-groups/)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/CUDA-Advanced-Optimization/02-Cooperative-Groups.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/CUDA-Advanced-Optimization/02-Cooperative-Groups.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
