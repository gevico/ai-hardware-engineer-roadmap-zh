---
title: 内存系统
description: 内存系统
---

# 内存系统

## 核心约束

Jetson Orin Nano Super 拥有 8 GB LPDDR5，由 CPU、GPU、DLA、摄像头与 OS 共享。扣除固件预留与 OS 之后，可用约 5.5–6 GB。模型、KV cache、scratch 缓冲区与 CUDA runtime 都必须塞进这块空间。

## 内存预算

`MemoryBudget`（`jllm_memory.h`）跟踪每一类分配：

```
╔══════════════════════════════════╗
║   JLLM Memory Budget             ║
╠══════════════════════════════════╣
║ Total DRAM:      7633 MB         ║  ← /proc/meminfo MemTotal
║ OS + kernel:    - 500 MB         ║  ← MemTotal - MemAvailable - CMA
║ CMA reserved:  - 768 MB         ║  ← /proc/meminfo CmaTotal
║ CUDA context:  - 300 MB         ║  ← estimate (updated after cudaSetDevice)
║ Model weights: -1800 MB         ║  ← actual GGUF file size after mmap
║ KV cache:      - 200 MB         ║  ← KVCachePool allocated size
║ Scratch:       -  64 MB         ║  ← ScratchPool allocated size
║ Safety margin: - 256 MB         ║  ← headroom to prevent OOM
╠══════════════════════════════════╣
║ FREE:           3745 MB         ║
╚══════════════════════════════════╝
```

### 来源：`src/memory/budget.cpp`

`probe_system_memory()` 读取 `/proc/meminfo` 字段：
- `MemTotal` — Linux 可见 RAM 总量
- `MemAvailable` — 实际可用（已计入缓存、缓冲区）
- `CmaTotal` — 连续内存分配器预留

### 自动上下文计算

引擎根据剩余内存计算最大上下文长度：

```
max_context = max_kv_mb × 1024 × 1024 / kv_per_token_bytes

kv_per_token_bytes = 2 × n_layers × n_kv_heads × head_dim × kv_type_bytes

Example (Llama 3.2 3B, INT8 KV):
  max_kv_mb = 7633 - 500 - 768 - 300 - 1800 - 64 - 256 = 3945 MB
  kv_per_token = 2 × 26 × 8 × 128 × 1 = 53,248 bytes
  max_context = 3945 × 1024 × 1024 / 53,248 = ~77,700 tokens
  (capped at model's max_seq_len)
```

## OOM 防护

`OOMGuard`（`jllm_memory.h`）在每次扩展 KV cache 前检查真实内存。

### 工作原理

生成每个 token 之前：
1. 从 `/proc/meminfo` 读取 `MemAvailable`（真实的 kernel 值，非缓存值）
2. 与 `kv_per_token_bytes + safety_margin` 比较
3. 若不足：优雅停止生成，并在统计中上报

```cpp
bool OOMGuard::can_extend(int64_t additional_bytes) const {
    int64_t free = real_free_mb();  // reads /proc/meminfo
    int64_t needed_mb = additional_bytes / (1024 * 1024) + 1;
    return free > (needed_mb + safety_mb_);
}
```

### 紧急释放

`emergency_free()` 丢弃文件系统缓存并触发 compaction：
- `echo 3 > /proc/sys/vm/drop_caches`
- `echo 1 > /proc/sys/vm/compact_memory`

仅作为最后手段调用——需要 root 权限。

## KV Cache 池

`KVCachePool`（`src/memory/kv_cache.cpp`）管理所有 layer 的 key/value 张量。

### 两级设计

```
┌─────────────────────────────────────────────┐
│  Fast Pool (cudaMallocHost)                  │
│  Pinned DRAM — GPU reads at full bandwidth  │
│  Recent tokens live here                     │
│  Size: n_layers × entry_bytes × max_context │
├─────────────────────────────────────────────┤
│  Overflow Pool (malloc)                      │
│  Unpinned DRAM — GPU reads via page faults  │
│  Old tokens evicted here                     │
│  Size: n_layers × entry_bytes × overflow    │
└─────────────────────────────────────────────┘
```

### 为什么这在 Jetson 上可行

在独立 GPU 上，“CPU offload”意味着 PCIe 传输（约 64 GB/s，高延迟）。在 Jetson 上，CPU 与 GPU 共享同一块物理 DRAM。“fast pool”使用 `cudaMallocHost`（pinned——无缺页，约 102 GB/s）。“overflow pool”使用 `malloc`（pageable——GPU 通过缺页访问，约 50 GB/s，但仍是同一块 DRAM）。

### 淘汰

fast pool 满时，最旧的 token 被移到 overflow：
```
Before:  Fast: [tok 0][tok 1][tok 2]...[tok 2047]  (full)
After:   Fast: [tok 512][tok 513]...[tok 2047]      (moved 0-511 to overflow)
         Overflow: [tok 0][tok 1]...[tok 511]
```

这只是同一 DRAM 内的一次 `memcpy`——很快。

### 内存布局

每个 layer、每个 token：
```
entry_bytes = 2 × n_kv_heads × head_dim × kv_type_bytes

For Llama 3.2 3B (8 KV heads, 128 dim, INT8):
  entry = 2 × 8 × 128 × 1 = 2,048 bytes per token per layer
  
26 layers × 2,048 bytes × 2,048 tokens = 109 MB for 2K context
26 layers × 2,048 bytes × 4,096 tokens = 218 MB for 4K context
```

## Scratch 池

`ScratchPool`（`src/memory/pool.cpp`）为中间张量提供临时缓冲区。


<details>
<summary>English original</summary>

**Memory System**

**The Core Constraint**

Jetson Orin Nano Super has 8 GB LPDDR5 shared between CPU, GPU, DLA, camera, and OS. After firmware carveouts and OS, ~5.5–6 GB is available. The model, KV cache, scratch buffers, and CUDA runtime must all fit in this space.

**Memory Budget**

`MemoryBudget` (`jllm_memory.h`) tracks every allocation category:

```
╔══════════════════════════════════╗
║   JLLM Memory Budget             ║
╠══════════════════════════════════╣
║ Total DRAM:      7633 MB         ║  ← /proc/meminfo MemTotal
║ OS + kernel:    - 500 MB         ║  ← MemTotal - MemAvailable - CMA
║ CMA reserved:  - 768 MB         ║  ← /proc/meminfo CmaTotal
║ CUDA context:  - 300 MB         ║  ← estimate (updated after cudaSetDevice)
║ Model weights: -1800 MB         ║  ← actual GGUF file size after mmap
║ KV cache:      - 200 MB         ║  ← KVCachePool allocated size
║ Scratch:       -  64 MB         ║  ← ScratchPool allocated size
║ Safety margin: - 256 MB         ║  ← headroom to prevent OOM
╠══════════════════════════════════╣
║ FREE:           3745 MB         ║
╚══════════════════════════════════╝
```

**Source: `src/memory/budget.cpp`**

`probe_system_memory()` reads `/proc/meminfo` fields:
- `MemTotal` — total Linux-visible RAM
- `MemAvailable` — realistic available (accounts for cache, buffers)
- `CmaTotal` — contiguous memory allocator reservation

**Auto Context Calculation**

The engine calculates maximum context length from remaining memory:

```
max_context = max_kv_mb × 1024 × 1024 / kv_per_token_bytes

kv_per_token_bytes = 2 × n_layers × n_kv_heads × head_dim × kv_type_bytes

Example (Llama 3.2 3B, INT8 KV):
  max_kv_mb = 7633 - 500 - 768 - 300 - 1800 - 64 - 256 = 3945 MB
  kv_per_token = 2 × 26 × 8 × 128 × 1 = 53,248 bytes
  max_context = 3945 × 1024 × 1024 / 53,248 = ~77,700 tokens
  (capped at model's max_seq_len)
```

**OOM Guard**

`OOMGuard` (`jllm_memory.h`) checks real memory before every KV cache extension.

**How it works**

Before generating each token:
1. Read `MemAvailable` from `/proc/meminfo` (real kernel value, not cached)
2. Compare against `kv_per_token_bytes + safety_margin`
3. If insufficient: stop generation gracefully, report in stats

```cpp
bool OOMGuard::can_extend(int64_t additional_bytes) const {
    int64_t free = real_free_mb();  // reads /proc/meminfo
    int64_t needed_mb = additional_bytes / (1024 * 1024) + 1;
    return free > (needed_mb + safety_mb_);
}
```

**Emergency Free**

`emergency_free()` drops filesystem caches and triggers compaction:
- `echo 3 > /proc/sys/vm/drop_caches`
- `echo 1 > /proc/sys/vm/compact_memory`

Only called as a last resort — requires root.

**KV Cache Pool**

`KVCachePool` (`src/memory/kv_cache.cpp`) manages key/value tensors for all layers.

**Two-Tier Design**

```
┌─────────────────────────────────────────────┐
│  Fast Pool (cudaMallocHost)                  │
│  Pinned DRAM — GPU reads at full bandwidth  │
│  Recent tokens live here                     │
│  Size: n_layers × entry_bytes × max_context │
├─────────────────────────────────────────────┤
│  Overflow Pool (malloc)                      │
│  Unpinned DRAM — GPU reads via page faults  │
│  Old tokens evicted here                     │
│  Size: n_layers × entry_bytes × overflow    │
└─────────────────────────────────────────────┘
```

**Why This Works on Jetson**

On discrete GPUs, "CPU offload" means PCIe transfer (~64 GB/s, high latency). On Jetson, CPU and GPU share the same physical DRAM. The "fast pool" uses `cudaMallocHost` (pinned — no page faults, ~102 GB/s). The "overflow pool" uses `malloc` (pageable — GPU accesses via page faults, ~50 GB/s, but still the same DRAM).

**Eviction**

When the fast pool is full, oldest tokens are moved to overflow:
```
Before:  Fast: [tok 0][tok 1][tok 2]...[tok 2047]  (full)
After:   Fast: [tok 512][tok 513]...[tok 2047]      (moved 0-511 to overflow)
         Overflow: [tok 0][tok 1]...[tok 511]
```

This is a `memcpy` within the same DRAM — fast.

**Memory Layout**

Per layer, per token:
```
entry_bytes = 2 × n_kv_heads × head_dim × kv_type_bytes

For Llama 3.2 3B (8 KV heads, 128 dim, INT8):
  entry = 2 × 8 × 128 × 1 = 2,048 bytes per token per layer
  
26 layers × 2,048 bytes × 2,048 tokens = 109 MB for 2K context
26 layers × 2,048 bytes × 4,096 tokens = 218 MB for 4K context
```

**Scratch Pool**

`ScratchPool` (`src/memory/pool.cpp`) provides temporary buffers for intermediate tensors.

</details>

### Bump Allocator

```
Pre-allocated backing:  [────────────────────── 64 MB ──────────────────────]
                         ^offset=0

After get(1024):        [used│──────────────── remaining ───────────────────]
                              ^offset=1024

After get(2048):        [used│used│────────── remaining ────────────────────]
                                   ^offset=3072

After reset():          [────────────────────── 64 MB ──────────────────────]
                         ^offset=0 (all memory reusable)
```

- `get(size)` — 返回指针，推进 offset。按 256 字节对齐。
- `reset()` — 把 offset 重置为 0。每个 decode（逐 token 生成阶段）步骤开始时调用。
- 推理期间 `malloc`/`free` 归零 —— 只是指针运算。

### Sizing

scratch 大小在加载时计算：
```
scratch = hidden_dim × 8 (attention intermediates)
        + n_heads × head_dim × 3 (Q, K, V projections)
        + intermediate_dim × 4 (FFN intermediates)
        + vocab_size × sizeof(float) + vocab_size × sizeof(half) (logits)
        minimum 64 MB
```

## 统一内存模式

### 模型权重：mmap + cudaHostRegister

```
File on disk (GGUF)
      │
      ▼ mmap(PROT_READ, MAP_PRIVATE)
DRAM pages (copy-on-write, demand-paged)
      │
      ▼ cudaHostRegister(ptr, size, cudaHostRegisterReadOnly)
GPU can read these pages directly (no copy, no page faults)
      │
      ▼ madvise(MADV_RANDOM)
Kernel knows access pattern is random (inference reads scattered weights)
```

### KV cache：cudaMallocHost

```
cudaMallocHost(&ptr, size)
  → Allocates pinned DRAM
  → Registered with CUDA runtime
  → GPU reads at full 102 GB/s bandwidth
  → CPU can read/write directly (for debugging, export)
  → Never swapped to disk
```

### Scratch：cudaMallocHost

与 KV cache 相同 —— pinned、GPU 可访问、CPU 可访问。每一步复用。


<details>
<summary>English original</summary>

**Bump Allocator**

```
Pre-allocated backing:  [────────────────────── 64 MB ──────────────────────]
                         ^offset=0

After get(1024):        [used│──────────────── remaining ───────────────────]
                              ^offset=1024

After get(2048):        [used│used│────────── remaining ────────────────────]
                                   ^offset=3072

After reset():          [────────────────────── 64 MB ──────────────────────]
                         ^offset=0 (all memory reusable)
```

- `get(size)` — returns pointer, advances offset. Aligns to 256 bytes.
- `reset()` — resets offset to 0. Called at start of each decode step.
- Zero `malloc`/`free` during inference — just pointer arithmetic.

**Sizing**

Scratch size is calculated at load time:
```
scratch = hidden_dim × 8 (attention intermediates)
        + n_heads × head_dim × 3 (Q, K, V projections)
        + intermediate_dim × 4 (FFN intermediates)
        + vocab_size × sizeof(float) + vocab_size × sizeof(half) (logits)
        minimum 64 MB
```

**Unified Memory Patterns**

**Model Weights: mmap + cudaHostRegister**

```
File on disk (GGUF)
      │
      ▼ mmap(PROT_READ, MAP_PRIVATE)
DRAM pages (copy-on-write, demand-paged)
      │
      ▼ cudaHostRegister(ptr, size, cudaHostRegisterReadOnly)
GPU can read these pages directly (no copy, no page faults)
      │
      ▼ madvise(MADV_RANDOM)
Kernel knows access pattern is random (inference reads scattered weights)
```

**KV Cache: cudaMallocHost**

```
cudaMallocHost(&ptr, size)
  → Allocates pinned DRAM
  → Registered with CUDA runtime
  → GPU reads at full 102 GB/s bandwidth
  → CPU can read/write directly (for debugging, export)
  → Never swapped to disk
```

**Scratch: cudaMallocHost**

Same as KV cache — pinned, GPU-accessible, CPU-accessible. Reused every step.

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/memory.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/memory.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
