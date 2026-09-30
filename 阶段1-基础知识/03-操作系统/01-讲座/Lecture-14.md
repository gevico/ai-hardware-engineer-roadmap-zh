---
title: 第 14 讲：内存分配：SLUB、kmalloc 与 CMA
description: 第 14 讲：内存分配：SLUB、kmalloc 与 CMA
---

# 第 14 讲：内存分配：SLUB、kmalloc 与 CMA

## 概述

内核需要管理物理 RAM 的每一个字节：决定哪个进程或子系统拥有哪些页，如何高效地把大页切成更小的对象，以及如何保证具备 DMA 能力的设备拿到物理上连续的内存。核心挑战是**碎片化**——随着时间推移，反复的分配与释放把 RAM 变成一块块空隙拼凑的补丁，即使空闲内存总量充足，也无法满足一次大的连续请求。本讲的心智模型是一个分配器层级：**buddy 分配器**管理物理页，**SLUB** 把这些页切成对象，**vmalloc** 提供虚拟连续的兜底，**CMA** 在启动时为 DMA 预留一段连续区域。对 AI 硬件工程师而言，这些分配器直接决定了 camera 的 ISP（图像信号处理器）驱动能否拿到它的 DMA 缓冲区，自定义内核驱动是否会泄漏内存，以及推理进程能否在嵌入式系统上熬过一次内存压力事件。

---

## 物理内存：buddy 分配器

**buddy 分配器**是 Linux 内核主要的物理页管理器。它是所有其他内核分配器赖以构建的基础。空闲内存记录在 11 个按 order 索引的**空闲链表**中（2^0 到 2^10 个页）。`alloc_pages(gfp_flags, order)` 返回 2^order 个物理上连续的页；`__get_free_pages()` 直接返回内核虚拟地址。

```
Buddy Allocator Free Lists
Order 0  (4KB):   [page] [page] [page] ...
Order 1  (8KB):   [page-pair] [page-pair] ...
Order 4  (64KB):  [block] [block] ...
Order 9  (2MB):   [huge-block] ...
Order 10 (4MB):   [max-block] ...

alloc_pages(GFP_KERNEL, 9)
  → finds a free order-9 block (2MB contiguous pages)
  → if none, splits an order-10 block into two order-9 blocks
  → returns one; puts the other ("buddy") back in the order-9 list
```

| Order | 大小 | 典型用途 |
|-------|------|-------------|
| 0 | 4 KB | 单个页、页表 |
| 1 | 8 KB | 小型 DMA 描述符环 |
| 4 | 64 KB | 中型 DMA 缓冲区 |
| 9 | 2 MB | 大页后备 |
| 10 | 4 MB | 大块连续分配 |

### 碎片化

随着时间推移，空闲页会变得**非连续**。即使空闲内存总量充足，高阶分配也会失败，因为不存在单个连续的 2^N 块。`/proc/buddyinfo` 显示每个 order、每个 zone、每个 NUMA 节点的空闲块数量。`echo 3 > /proc/sys/vm/drop_caches` 回收干净的页缓存和 slab 对象（生产环境中慎用；并不解决碎片化）。

> **关键洞察：** 碎片化正是 CMA 存在的原因。在启动时，趁着还没有任何进程有机会把内存搞碎，CMA 就划出一段保证连续的区域。如果等到 AI 加速器驱动需要 DMA 缓冲区时才动手，那些连续页可能已经拿不到了。

> **常见陷阱：** 在 runtime 调用 `alloc_pages(GFP_KERNEL, 9)` 的驱动（为了一块 2MB 的 DMA 缓冲区）可能会以 `-ENOMEM` 失败，即使还有几百 MB 的空闲内存，因为碎片化已经消灭了所有连续的 2MB 块。应当始终使用 CMA，或者在驱动 probe 阶段就预分配好大的 DMA 缓冲区，而不是按需分配。

---

## SLUB 分配器

buddy 分配器以页为单位工作（最小 4KB）。大多数内核数据结构——网络 socket 描述符、inode 对象、驱动私有数据——都远小于一个页。**SLUB 分配器**（自内核 2.6.23 起为默认）通过管理**子页分配**来弥合这一差距。相同类型和尺寸的对象被归入同一个 **slab**。每个 slab 是一个或多个连续的页。SLUB 提供：

```
SLUB Allocator Structure
┌─────────────────────────────────────────┐
│            kmem_cache "myobj"           │
│  ┌─────────────────────────────────┐   │
│  │  Per-CPU slab (CPU 0)           │   │
│  │  [obj][obj][obj][FREE][FREE]..  │   │  ← Fast path: no lock
│  └─────────────────────────────────┘   │
│  ┌─────────────────────────────────┐   │
│  │  Per-CPU slab (CPU 1)           │   │
│  │  [obj][obj][FREE][FREE][FREE].. │   │
│  └─────────────────────────────────┘   │
│  ┌─────────────────────────────────┐   │
│  │  Partial slab list (node 0)     │   │  ← Shared; used when CPU slab full
│  │  [slab1] → [slab2] → [slab3]   │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

- **Per-CPU slab**：每个 CPU 维护一个本地活动 slab；分配和释放在快路径上无锁
- **部分 slab 链表**：每个 node 维护一个半满 slab 的链表；当本地 slab 耗尽时，在 CPU 之间共享
- **对象复用**：释放的对象归还给缓存而不清零（除非 `__GFP_ZERO`）；避免反复初始化的开销


<details>
<summary>English original</summary>

**Lecture 14: Memory Allocation: SLUB, kmalloc & CMA**

**Overview**

The kernel needs to manage every byte of physical RAM: deciding which process or subsystem owns which pages, how to efficiently carve large pages into smaller objects, and how to guarantee that DMA-capable devices get physically contiguous memory. The core challenge is **fragmentation** — over time, repeated allocations and frees leave RAM as a patchwork of gaps that cannot satisfy a large contiguous request even when total free memory is ample. The mental model for this lecture is a hierarchy of allocators: the **buddy allocator** manages physical pages, **SLUB** carves those pages into objects, **vmalloc** provides virtually-contiguous fallback, and **CMA** reserves a contiguous region at boot for DMA. For an AI hardware engineer, these allocators directly determine whether a camera ISP driver can get its DMA buffers, whether a custom kernel driver leaks memory, and whether the inference process survives a memory pressure event on an embedded system.

---

**Physical Memory: The Buddy Allocator**

The **buddy allocator** is the Linux kernel's primary physical page manager. It is the foundation on which all other kernel allocators are built. Free memory is tracked in 11 **free lists** indexed by order (2^0 through 2^10 pages). `alloc_pages(gfp_flags, order)` returns 2^order physically contiguous pages; `__get_free_pages()` returns the kernel virtual address directly.

```
Buddy Allocator Free Lists
Order 0  (4KB):   [page] [page] [page] ...
Order 1  (8KB):   [page-pair] [page-pair] ...
Order 4  (64KB):  [block] [block] ...
Order 9  (2MB):   [huge-block] ...
Order 10 (4MB):   [max-block] ...

alloc_pages(GFP_KERNEL, 9)
  → finds a free order-9 block (2MB contiguous pages)
  → if none, splits an order-10 block into two order-9 blocks
  → returns one; puts the other ("buddy") back in the order-9 list
```

| Order | Size | Typical use |
|-------|------|-------------|
| 0 | 4 KB | Single page, page table |
| 1 | 8 KB | Small DMA descriptor ring |
| 4 | 64 KB | Medium DMA buffer |
| 9 | 2 MB | Huge page backing |
| 10 | 4 MB | Large contiguous allocation |

**Fragmentation**

Over time, free pages become **non-contiguous**. High-order allocations fail even when total free memory is sufficient because no single 2^N contiguous block exists. `/proc/buddyinfo` shows the count of free blocks at each order per zone per NUMA node. `echo 3 > /proc/sys/vm/drop_caches` reclaims clean page cache and slab objects (use with care in production; does not solve fragmentation).

> **Key Insight:** Fragmentation is why CMA exists. By boot time, before any process has had a chance to fragment memory, CMA carves out a guaranteed-contiguous region. If you wait until an AI accelerator driver needs DMA buffers, the contiguous pages may no longer be available.

> **Common Pitfall:** A driver that calls `alloc_pages(GFP_KERNEL, 9)` at runtime (for a 2MB DMA buffer) may fail with `-ENOMEM` even when hundreds of megabytes of free memory exist, because fragmentation has eliminated all contiguous 2MB blocks. Always use CMA or pre-allocate large DMA buffers at driver probe time, not on demand.

---

**SLUB Allocator**

The buddy allocator works in units of pages (4KB minimum). Most kernel data structures — network socket descriptors, inode objects, driver private data — are far smaller than a page. The **SLUB allocator** (default since kernel 2.6.23) bridges this gap by managing **sub-page allocations**. Objects of the same type and size are grouped into **slabs**. Each slab is one or more contiguous pages. SLUB provides:

```
SLUB Allocator Structure
┌─────────────────────────────────────────┐
│            kmem_cache "myobj"           │
│  ┌─────────────────────────────────┐   │
│  │  Per-CPU slab (CPU 0)           │   │
│  │  [obj][obj][obj][FREE][FREE]..  │   │  ← Fast path: no lock
│  └─────────────────────────────────┘   │
│  ┌─────────────────────────────────┐   │
│  │  Per-CPU slab (CPU 1)           │   │
│  │  [obj][obj][FREE][FREE][FREE].. │   │
│  └─────────────────────────────────┘   │
│  ┌─────────────────────────────────┐   │
│  │  Partial slab list (node 0)     │   │  ← Shared; used when CPU slab full
│  │  [slab1] → [slab2] → [slab3]   │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

- **Per-CPU slabs**: each CPU maintains a local active slab; allocation and free are lock-free on the fast path
- **Partial slab list**: per-node list of partially filled slabs; shared between CPUs when the local slab is exhausted
- **Object reuse**: freed objects are returned to the cache without zeroing (unless `__GFP_ZERO`); avoids repeated initialization overhead

</details>

### kmem_cache 接口

当驱动反复分配同一种结构体类型（例如每帧的 DMA 描述符）时，创建专用的 `kmem_cache` 比使用通用的 `kmalloc` 更高效：

```c
struct kmem_cache *cache = kmem_cache_create(
    "myobj",                        /* cache name visible in /proc/slabinfo */
    sizeof(struct myobj),           /* object size */
    __alignof__(struct myobj),      /* minimum alignment */
    SLAB_HWCACHE_ALIGN | SLAB_PANIC, /* align to cache line; panic if create fails */
    NULL);                          /* optional constructor */

struct myobj *p = kmem_cache_alloc(cache, GFP_KERNEL);
kmem_cache_free(cache, p);
kmem_cache_destroy(cache);  /* call only when module unloads */
```

`SLAB_HWCACHE_ALIGN` 将对象填充到 CPU 缓存行边界，防止不同 CPU 核上的对象之间发生伪共享——这对高频 ISR 路径中的每帧状态很重要。

### kmalloc

`kmalloc(size, gfp_flags)` 是**通用内核分配器**。它在基于大小的 SLUB 缓存中选择，覆盖 2 的幂次大小：8、16、32、64、128、256、512、1K、2K、4K、8K 字节。`kzalloc(size, gfp)` 会清零填充。`kfree(ptr)` 归还给所属的 slab 缓存。

可以把 `kmalloc` 看作内核版的 `malloc()`——不需要手动管理内存池，但要接受通用性带来的一些开销。对于大于 8KB 的大小，`kmalloc` 委托给 buddy 分配器，返回物理连续的分配。

### 诊断

- `/proc/slabinfo`：缓存名、活跃对象数、总对象数、对象大小、每 slab 页数
- `slabtop`：按内存占用排序的实时视图；排查驱动中 slab 泄漏的首选工具
- `ksize(ptr)`：返回实际分配的大小（由于缓存取整，可能超过请求的大小）
- KASAN/KFENCE：内核 sanitizer；检测 slab 对象中的 use-after-free 和越界访问

> **常见陷阱：** 驱动在 V4L2 `VIDIOC_QBUF` 路径中分配每帧 `kmalloc` 对象，但在错误路径上从未调用 `kfree()`，会静默泄漏 slab 内存。`slabtop` 会显示该缓存的对象数无界增长。务必在所有错误路径上将分配与释放配对，或在 probe 阶段分配时使用 `devm_kzalloc()`。

---

## vmalloc

在介绍完 buddy 和 SLUB 分配器之后，还有一种场景：一个大的内核缓冲区，用于 DMA 时不要求物理连续，但必须是虚拟连续以便 CPU 访问。`vmalloc(size)` 分配**虚拟连续但物理不连续**的内存。内核在 `vmalloc` VA 区域建立页表，映射各个页。由于建立和拆除时需要构建页表并刷新 TLB，它比 `kmalloc` 慢。

```
vmalloc virtual address space
┌─────────────────────────────────────────────────────┐
│  VA: 0xffff_c000_0000_0000 ... (vmalloc region)     │
│                                                     │
│  vaddr[0..4KB]   → physical page A (any location)  │
│  vaddr[4K..8KB]  → physical page B (any location)  │
│  vaddr[8K..12KB] → physical page C (any location)  │
│  ...                                                │
│  (pages are NOT contiguous in physical RAM)         │
└─────────────────────────────────────────────────────┘
CPU sees one contiguous buffer; DMA engine cannot use it directly.
```

- `vfree(ptr)`：释放；刷新 vmalloc VA 页表
- `kvmalloc(size, gfp)` / `kvfree(ptr)`：首选的混合方式；先尝试 `kmalloc`（连续、快速）；对于大尺寸回退到 `vmalloc`
- 使用场景：不需要物理连续 DMA 的大内核缓冲区（模块 BSS、FPGA bitstream 暂存、大固件镜像）
- 最大大小：受 `VMALLOC_SIZE` 限制；在 64 位内核上通常为数百 GB

> **关键洞察：** `kvmalloc` 是未知大小的可变尺寸分配的最佳默认选择。它会透明地为小尺寸提供最快的分配路径（物理连续的 `kmalloc`），失败时回退到 `vmalloc`——无需调用者知道走了哪条路径。

---


<details>
<summary>English original</summary>

**kmem_cache Interface**

When a driver allocates the same structure type repeatedly (e.g., per-frame DMA descriptors), creating a dedicated `kmem_cache` is more efficient than using generic `kmalloc`:

```c
struct kmem_cache *cache = kmem_cache_create(
    "myobj",                        /* cache name visible in /proc/slabinfo */
    sizeof(struct myobj),           /* object size */
    __alignof__(struct myobj),      /* minimum alignment */
    SLAB_HWCACHE_ALIGN | SLAB_PANIC, /* align to cache line; panic if create fails */
    NULL);                          /* optional constructor */

struct myobj *p = kmem_cache_alloc(cache, GFP_KERNEL);
kmem_cache_free(cache, p);
kmem_cache_destroy(cache);  /* call only when module unloads */
```

`SLAB_HWCACHE_ALIGN` pads objects to a CPU cache line boundary, preventing false sharing between objects on different CPU cores — important for per-frame state in high-frequency ISR paths.

**kmalloc**

`kmalloc(size, gfp_flags)` is the **general-purpose kernel allocator**. It selects among size-based SLUB caches covering power-of-2 sizes: 8, 16, 32, 64, 128, 256, 512, 1K, 2K, 4K, 8K bytes. `kzalloc(size, gfp)` zero-fills. `kfree(ptr)` returns to the owning slab cache.

Think of `kmalloc` as the kernel equivalent of `malloc()` — you don't need to manage a pool manually, but you accept some overhead from the generality. For sizes above 8KB, `kmalloc` delegates to the buddy allocator and returns a physically contiguous allocation.

**Diagnostics**

- `/proc/slabinfo`: cache name, active objects, total objects, object size, pages-per-slab
- `slabtop`: live sorted view by memory consumption; first tool to check for slab leaks in drivers
- `ksize(ptr)`: returns the actual allocated size (may exceed requested size due to cache rounding)
- KASAN/KFENCE: kernel sanitizers; detect use-after-free and out-of-bounds in slab objects

> **Common Pitfall:** A driver that allocates a per-frame `kmalloc` object in the V4L2 `VIDIOC_QBUF` path but never calls `kfree()` on error paths will silently leak slab memory. `slabtop` will show the cache's object count growing unboundedly. Always pair allocations with frees in all error paths, or use `devm_kzalloc()` for probe-time allocations.

---

**vmalloc**

With the buddy and SLUB allocators covered, there is one more scenario: a large kernel buffer that does not need to be physically contiguous for DMA, but must be virtually contiguous for easy CPU access. `vmalloc(size)` allocates **virtually contiguous but physically non-contiguous** memory. The kernel sets up page tables in the `vmalloc` VA region, mapping individual pages. It is slower than `kmalloc` due to page table construction and TLB flushing on setup and teardown.

```
vmalloc virtual address space
┌─────────────────────────────────────────────────────┐
│  VA: 0xffff_c000_0000_0000 ... (vmalloc region)     │
│                                                     │
│  vaddr[0..4KB]   → physical page A (any location)  │
│  vaddr[4K..8KB]  → physical page B (any location)  │
│  vaddr[8K..12KB] → physical page C (any location)  │
│  ...                                                │
│  (pages are NOT contiguous in physical RAM)         │
└─────────────────────────────────────────────────────┘
CPU sees one contiguous buffer; DMA engine cannot use it directly.
```

- `vfree(ptr)`: release; flushes vmalloc VA page tables
- `kvmalloc(size, gfp)` / `kvfree(ptr)`: preferred hybrid; attempts `kmalloc` first (contiguous, fast); falls back to `vmalloc` for large sizes
- Use cases: large kernel buffers that do not require physically contiguous DMA (module BSS, FPGA bitstream staging, large firmware images)
- Maximum size: limited by `VMALLOC_SIZE`; typically hundreds of GB on 64-bit kernels

> **Key Insight:** `kvmalloc` is the best default for variable-size allocations of unknown size. It transparently gives you the fastest allocation path (physically contiguous `kmalloc`) for small sizes and falls back to `vmalloc` when that fails — without requiring the caller to know which path was taken.

---

</details>

## 内存区

物理地址空间依据硬件施加的**可访问性约束**划分为若干**内存区**。并非所有物理 RAM 对所有用途都同样可用——传统 ISA 设备、32 位 PCIe 设备和现代 64 位设备各有不同的 DMA 地址范围限制。

```
Physical Address Space → Memory Zones
┌─────────────────────────────────────────────┐
│ 0x000_0000_0000 (0GB)                       │
│   ┌─────────────────────────────────────┐   │
│   │ ZONE_DMA    (0–16 MB)               │   │  GFP_DMA    — legacy ISA
│   └─────────────────────────────────────┘   │
│   ┌─────────────────────────────────────┐   │
│   │ ZONE_DMA32  (0–4 GB)                │   │  GFP_DMA32  — 32-bit PCIe
│   └─────────────────────────────────────┘   │
│ 0x000_1000_0000 (4GB)                       │
│   ┌─────────────────────────────────────┐   │
│   │ ZONE_NORMAL (4 GB+)                 │   │  GFP_KERNEL — standard kernel
│   └─────────────────────────────────────┘   │
│   ┌─────────────────────────────────────┐   │
│   │ ZONE_MOVABLE (configurable)         │   │  — CMA, page migration
│   └─────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
```

| 内存区 | 物理范围 | GFP 标志 | 说明 |
|------|---------------|----------|-------|
| ZONE_DMA | 0–16 MB | `GFP_DMA` | 传统 ISA 24 位 DMA；如今很少需要 |
| ZONE_DMA32 | 0–4 GB | `GFP_DMA32` | 支持 32 位 DMA 的设备（PCIe 默认） |
| ZONE_NORMAL | 4 GB+ | `GFP_KERNEL` | 标准 kernel 分配 |
| ZONE_MOVABLE | 可配置 | — | 可迁移/可用于 CMA 的页 |

---

## GFP 标志（Get Free Pages）

**GFP 标志**（Get Free Pages）会传给每个分配函数，并控制两件事：从哪个**内存区**分配，以及内存紧张时分配器被允许做什么（睡眠、回收、swap）。

GFP 标志控制分配上下文与内存区选择：

| 标志 | 行为 |
|------|----------|
| `GFP_KERNEL` | 可以睡眠、可以回收；用于进程上下文 |
| `GFP_ATOMIC` | 不睡眠、不回收；用于中断或自旋锁上下文；可能返回 NULL |
| `GFP_DMA` | 必须从 ZONE_DMA（0–16 MB）分配 |
| `GFP_DMA32` | 必须从 ZONE_DMA32（0–4 GB）分配 |
| `__GFP_ZERO` | 将返回的内存清零 |
| `__GFP_NOFAIL` | 无限重试直到成功；谨慎使用 |
| `__GFP_NOWARN` | 分配失败时抑制 OOM 告警 |
| `__GFP_COMP` | 返回复合页（huge page 后备所需） |

关键约束：**绝不要在中断上下文中使用 `GFP_KERNEL`**，也不要在持有自旋锁时使用；它可能睡眠等待回收。在这些路径中使用 `GFP_ATOMIC`，并处理 `NULL` 返回值。

面向驱动作者的 GFP 标志决策树：
1. 是否处于中断上下文或持有自旋锁？→ `GFP_ATOMIC`
2. 设备的 DMA 引擎是否只能寻址 32 位地址？→ `GFP_DMA32`
3. 是否为需要 16 MB 以下内存的传统 ISA 设备？→ `GFP_DMA`
4. 其他情况 → `GFP_KERNEL`

> **常见陷阱：** 在中断服务程序中使用 `GFP_KERNEL` 会触发 kernel BUG splat（"sleeping function called from invalid context"）。ISR 运行时中断处于关闭状态，无法阻塞等待内存回收。在 ISR 上下文中始终使用 `GFP_ATOMIC`，并在进入中断路径之前预分配缓冲区。

---

## CMA（Contiguous Memory Allocator）

考虑到碎片问题，CMA 提供了一种干净的解决方案：在启动时、任何碎片产生之前**预留一块物理连续区域**，并按需提供给 DMA 设备。CMA 在启动时于 `ZONE_MOVABLE` 内预留物理连续区域。普通可迁移页可以使用这块内存，直到有设备请求它；此时 kernel 会把这些可迁移页迁出预留区域。

```
System Boot
┌────────────────────────────────────────────────────┐
│ Physical RAM                                       │
│ ┌──────────────┬──────────────────┬─────────────┐ │
│ │ Kernel image │   CMA reserved   │ General RAM │ │
│ │  (static)    │  (256MB @ 4GB)   │  (movable)  │ │
│ └──────────────┴──────────────────┴─────────────┘ │
└────────────────────────────────────────────────────┘

At runtime (before NVDLA needs it):
CMA region: [movable page][movable page][movable page]...
           (general OS pages using the reserved space)

When NVDLA driver calls dma_alloc_coherent():
  1. Kernel migrates movable pages out of CMA region
  2. Returns the now-free contiguous physical region to NVDLA
  3. NVDLA DMA engine has guaranteed contiguous PA
```

### 配置

Kernel 命令行：
```
cma=256M
cma=128M@0x100000000   # reserve 128 MB starting at 4 GB PA
```

设备树（用于平台级分配）：
```dts
reserved-memory {
    linux,cma {
        compatible = "shared-dma-pool";
        reusable;
        size = <0 0x10000000>;
        linux,cma-default;
    };
};
```


<details>
<summary>English original</summary>

**Memory Zones**

The physical address space is partitioned into **zones** based on **accessibility constraints** imposed by hardware. Not all physical RAM is equally usable for all purposes — legacy ISA devices, 32-bit PCIe devices, and modern 64-bit devices each have different DMA address range limits.

```
Physical Address Space → Memory Zones
┌─────────────────────────────────────────────┐
│ 0x000_0000_0000 (0GB)                       │
│   ┌─────────────────────────────────────┐   │
│   │ ZONE_DMA    (0–16 MB)               │   │  GFP_DMA    — legacy ISA
│   └─────────────────────────────────────┘   │
│   ┌─────────────────────────────────────┐   │
│   │ ZONE_DMA32  (0–4 GB)                │   │  GFP_DMA32  — 32-bit PCIe
│   └─────────────────────────────────────┘   │
│ 0x000_1000_0000 (4GB)                       │
│   ┌─────────────────────────────────────┐   │
│   │ ZONE_NORMAL (4 GB+)                 │   │  GFP_KERNEL — standard kernel
│   └─────────────────────────────────────┘   │
│   ┌─────────────────────────────────────┐   │
│   │ ZONE_MOVABLE (configurable)         │   │  — CMA, page migration
│   └─────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
```

| Zone | Physical range | GFP flag | Notes |
|------|---------------|----------|-------|
| ZONE_DMA | 0–16 MB | `GFP_DMA` | Legacy ISA 24-bit DMA; rarely needed today |
| ZONE_DMA32 | 0–4 GB | `GFP_DMA32` | 32-bit DMA-capable devices (PCIe default) |
| ZONE_NORMAL | 4 GB+ | `GFP_KERNEL` | Standard kernel allocations |
| ZONE_MOVABLE | Configurable | — | Pages eligible for migration/CMA |

---

**GFP Flags (Get Free Pages)**

**GFP flags** (Get Free Pages) are passed to every allocation function and control two things: which **memory zone** to allocate from, and what the allocator is allowed to do when memory is tight (sleep, reclaim, swap).

GFP flags control the allocation context and zone selection:

| Flag | Behavior |
|------|----------|
| `GFP_KERNEL` | May sleep, may reclaim; use in process context |
| `GFP_ATOMIC` | No sleep, no reclaim; use in interrupt or spinlock context; may return NULL |
| `GFP_DMA` | Must allocate from ZONE_DMA (0–16 MB) |
| `GFP_DMA32` | Must allocate from ZONE_DMA32 (0–4 GB) |
| `__GFP_ZERO` | Zero-fill the returned memory |
| `__GFP_NOFAIL` | Retry indefinitely until success; use sparingly |
| `__GFP_NOWARN` | Suppress OOM warning on allocation failure |
| `__GFP_COMP` | Return compound page (required for huge page backing) |

Key constraint: **never use `GFP_KERNEL` in interrupt context** or while holding a spinlock; it may sleep waiting for reclaim. Use `GFP_ATOMIC` in these paths and handle `NULL` return.

The GFP flag decision tree for driver authors:
1. Am I in interrupt context or holding a spinlock? → `GFP_ATOMIC`
2. Does the device's DMA engine address only 32-bit addresses? → `GFP_DMA32`
3. Is this a legacy ISA device needing sub-16MB memory? → `GFP_DMA`
4. Otherwise → `GFP_KERNEL`

> **Common Pitfall:** Using `GFP_KERNEL` in an interrupt service routine causes a kernel BUG splat ("sleeping function called from invalid context"). The ISR runs with interrupts disabled and cannot block waiting for memory reclaim. Always use `GFP_ATOMIC` in ISR context and pre-allocate buffers before the interrupt path is entered.

---

**CMA (Contiguous Memory Allocator)**

With fragmentation in mind, CMA provides a clean solution: **reserve a physically contiguous region at boot time**, before any fragmentation occurs, and make it available to DMA devices on demand. CMA reserves physically contiguous regions at boot time within `ZONE_MOVABLE`. Normal movable pages can use this memory until a device requests it; at that point, the kernel migrates the movable pages out of the reserved region.

```
System Boot
┌────────────────────────────────────────────────────┐
│ Physical RAM                                       │
│ ┌──────────────┬──────────────────┬─────────────┐ │
│ │ Kernel image │   CMA reserved   │ General RAM │ │
│ │  (static)    │  (256MB @ 4GB)   │  (movable)  │ │
│ └──────────────┴──────────────────┴─────────────┘ │
└────────────────────────────────────────────────────┘

At runtime (before NVDLA needs it):
CMA region: [movable page][movable page][movable page]...
           (general OS pages using the reserved space)

When NVDLA driver calls dma_alloc_coherent():
  1. Kernel migrates movable pages out of CMA region
  2. Returns the now-free contiguous physical region to NVDLA
  3. NVDLA DMA engine has guaranteed contiguous PA
```

**Configuration**

Kernel command line:
```
cma=256M
cma=128M@0x100000000   # reserve 128 MB starting at 4 GB PA
```

Device Tree (for platform-level assignment):
```dts
reserved-memory {
    linux,cma {
        compatible = "shared-dma-pool";
        reusable;
        size = <0 0x10000000>;
        linux,cma-default;
    };
};
```

</details>

### API

`dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL)` 在支持 CMA 的平台上使用 CMA。返回一个 CPU 虚拟地址和一个设备可见的 `dma_handle`（物理地址或 IOVA）。该区域不可缓存或硬件一致。

这个单一的 API 调用屏蔽了平台之间的差异：在带 IOMMU 的 x86 上，它返回带 IOVA 的 pinned pages；在没有 IOMMU 的 Jetson 上，它返回物理连续的 PA。驱动代码在两者之间可移植。

Jetson 上的 CMA 使用者：
- NVDLA：输入/输出 feature map 的 DMA buffer
- VIC（Video Image Compositor）：framebuffer 传输
- Camera ISP：raw frame DMA
- Display engine：framebuffer
- NVIDIA 无 IOMMU 的 DMA 需要物理连续的 PA

### Monitoring

`/proc/cma`：总量、已用、最大分配。启动时的 `dmesg | grep cma` 确认预留。

> **关键洞察：** CMA 对驱动不可见——它们只是调用 `dma_alloc_coherent()`。CMA 的工作在后台透明进行，在设备需要时将普通页从预留区域迁出。启动时的预留正是其可靠性的来源：此时尚未发生碎片化。

---

## OOM Killer

当所有回收路径（page cache 驱逐、swap、slab shrinker）都失败时，**OOM killer** 会选择一个进程并将其终止。理解 OOM killer 对**嵌入式 AI 系统**至关重要，这类系统没有 swap，且多个进程争抢有限的 RAM。

选择：`badness(task)` 按进程的 RSS 和 swap 使用量成比例地打分。可按进程调整：
```bash
echo -1000 > /proc/$(pidof inferenced)/oom_score_adj   # protect
echo +500  > /proc/$(pidof bloated_loader)/oom_score_adj  # prefer killing
```
Range：-1000（永不杀死）到 +1000（优先杀死）。分数 -1000 会完全禁用该进程的 OOM kill。

在推理守护进程上设置 `oom_score_adj = -1000` 是**嵌入式系统上的标准做法**。推理守护进程代表系统的主要功能——在内存压力下杀死它，比杀死后台数据加载器或日志聚合器更糟。

---

## Memory Pressure Tuning

这些内核参数决定了 VM 在压力下回收内存的激进程度：

- `vm.swappiness`（0–200）：倾向于换出匿名页还是回收 page cache；0 避免 swap，100 平衡，200 偏好 swap
- `vm.min_free_kbytes`：`kswapd` 回收激活前的最小空闲内存；在无 swap 的嵌入式系统上应调高，以避免 GFP_ATOMIC 失败
- `vm.overcommit_memory`：0 = 启发式，1 = 始终允许 overcommit，2 = 严格（已提交 ≤ RAM + swap × ratio）

在无 swap 的嵌入式推理系统上，设置 `vm.swappiness=0`（在没有 swap 的设备上避免 swap 活动），并调高 `vm.min_free_kbytes`，以确保中断处理程序中的 `GFP_ATOMIC` 分配始终成功。

---

## Summary

| 分配器 | 连续 PA？ | 可睡眠？ | 最大尺寸 | 主要用途 |
|-----------|---------------|-----------|---------|-------------|
| kmalloc / SLUB | 是（最大约 8KB） | 取决于 GFP | 通常 8 KB | 内核对象、驱动结构体 |
| alloc_pages | 是 | 取决于 GFP | 4 MB（order 10） | huge page backing、大块连续 |
| vmalloc | 否（仅虚拟） | 是 | vmalloc VA 范围（数百 GB） | 固件镜像、大的非 DMA buffer |
| dma_alloc_coherent / CMA | 是 | 是 | CMA 预留大小 | 支持 DMA 的设备 buffer |


<details>
<summary>English original</summary>

**API**

`dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL)` uses CMA on platforms that support it. Returns a CPU virtual address and a device-visible `dma_handle` (physical address or IOVA). The region is non-cacheable or hardware-coherent.

This single API call abstracts the difference between platforms: on x86 with IOMMU, it returns pinned pages with an IOVA; on Jetson without IOMMU, it returns a physically contiguous PA. Driver code is portable across both.

CMA users on Jetson:
- NVDLA: input/output feature map DMA buffers
- VIC (Video Image Compositor): frame buffer transfers
- Camera ISP: raw frame DMA
- Display engine: framebuffer
- NVIDIA IOMMU-less DMA requires physically contiguous PA

**Monitoring**

`/proc/cma`: total, used, maximum allocation. `dmesg | grep cma` at boot confirms reservation.

> **Key Insight:** CMA is invisible to drivers — they just call `dma_alloc_coherent()`. CMA's job happens transparently in the background, migrating ordinary pages out of the reserved region when a device needs it. The reservation at boot is what makes this reliable: no fragmentation has occurred yet.

---

**OOM Killer**

When all reclaim paths (page cache eviction, swap, slab shrinkers) fail, the **OOM killer** selects and terminates a process. Understanding the OOM killer is critical for **embedded AI systems** where there is no swap and multiple processes compete for limited RAM.

Selection: `badness(task)` scores each process proportional to its RSS and swap usage. Adjustable per-process:
```bash
echo -1000 > /proc/$(pidof inferenced)/oom_score_adj   # protect
echo +500  > /proc/$(pidof bloated_loader)/oom_score_adj  # prefer killing
```
Range: -1000 (never kill) to +1000 (kill first). Score -1000 disables OOM kill entirely for that process.

Setting `oom_score_adj = -1000` on the inference daemon is **standard practice on embedded systems**. The inference daemon represents the primary system function — killing it during memory pressure is worse than killing a background data loader or log aggregator.

---

**Memory Pressure Tuning**

These kernel parameters shape how aggressively the VM reclaims memory under pressure:

- `vm.swappiness` (0–200): tendency to swap anonymous pages vs. reclaiming page cache; 0 avoids swap, 100 is balanced, 200 prefers swap
- `vm.min_free_kbytes`: minimum free memory before `kswapd` reclaim activates; increase on embedded systems with no swap to avoid GFP_ATOMIC failures
- `vm.overcommit_memory`: 0 = heuristic, 1 = always allow overcommit, 2 = strict (committed ≤ RAM + swap × ratio)

On embedded inference systems with no swap, set `vm.swappiness=0` (avoid swap activity on a device that has no swap) and increase `vm.min_free_kbytes` to ensure that `GFP_ATOMIC` allocations in interrupt handlers always succeed.

---

**Summary**

| Allocator | Contiguous PA? | Can sleep? | Max size | Primary use |
|-----------|---------------|-----------|---------|-------------|
| kmalloc / SLUB | Yes (up to ~8KB) | Depends on GFP | 8 KB typical | Kernel objects, driver structs |
| alloc_pages | Yes | Depends on GFP | 4 MB (order 10) | Huge page backing, large contiguous |
| vmalloc | No (virtual only) | Yes | vmalloc VA range (100s GB) | Firmware images, large non-DMA buffers |
| dma_alloc_coherent / CMA | Yes | Yes | CMA reservation size | DMA-capable device buffers |

</details>

### 概念回顾

- **为什么碎片化会导致高阶 buddy 分配失败？** buddy 分配器要求 2^N 个物理连续的页。经过 runtime 的分配与释放活动后，空闲页零散地分布在非连续的位置上。即便有 500MB 空闲内存，也可能不存在单个 2MB 的连续块。

- **`GFP_KERNEL` 和 `GFP_ATOMIC` 有什么区别？** `GFP_KERNEL` 可能会睡眠以等待内存回收，因此只能在进程上下文中调用。`GFP_ATOMIC` 从不睡眠，在中断上下文中是安全的，但内存紧张时可能返回 NULL。`GFP_ATOMIC` 分配会消耗少量应急保留内存。

- **为什么 CMA 在启动时预留内存，而不是在驱动 probe 时预留？** 等到驱动 probe 时，系统已经运行了足够久，内存已经碎片化。启动时预留可保证在碎片化开始之前就得到一段连续区域。

- **`SLAB_HWCACHE_ALIGN` 的用途是什么？** 它把每个 slab 对象填充到 CPU 缓存行的边界。这可防止伪共享（false sharing）——两个恰好共享同一条缓存行的对象被不同 CPU 核修改，从而产生代价高昂的缓存一致性流量。

- **什么时候应该用 `kmem_cache_create` 而不是 `kmalloc`？** 当你反复分配大量同一类型的对象时（例如摄像头驱动中每帧的描述符）。专用缓存开销更低、内存局部性更好，并且在 `slabtop` 中清晰可见，便于检测泄漏。

- **OOM killer 如何决定杀死哪个进程？** 它计算一个 `badness` 分数，该分数与每个进程的 RSS 和 swap 用量成正比，并受 `oom_score_adj` 修正。分数最高的进程最先被杀。设置 `oom_score_adj = -1000` 可防止某个进程被选中。

---

## AI 硬件关联

- CMA 在启动时为 Jetson NVDLA 和 VIC 的 DMA 缓冲区预留物理连续内存，确保在任何用户空间进程造成内存碎片化之前就已可用
- 为 AI 加速器卡编写 PCIe 设备驱动时，若其 DMA 引擎无法寻址 4 GB 以上的物理内存，就必须使用 `GFP_DMA32`
- `GFP_ATOMIC` 是 V4L2 驱动中摄像头 ISR 缓冲区分配的正确 flag，此处中断处理程序不能睡眠等待回收
- OOM 分数保护（`oom_score_adj = -1000`）可防止推理 daemon（modeld、controllsd）在 openpilot 嵌入式环境的内存压力下被杀掉
- 在自定义传感器驱动中，若每帧的 slab 分配不断累积却没有匹配的释放，`slabtop` 是定位内存泄漏的首选诊断工具
- 在加载可变大小 bitstream 的 FPGA 驱动代码中，`kvmalloc` 是首选模式，它会根据可用性透明地选择连续或非连续的后备内存


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why does fragmentation cause high-order buddy allocations to fail?** The buddy allocator requires 2^N physically contiguous pages. After runtime allocation and free activity, free pages are scattered non-contiguously. Even with 500MB free, there may be no single 2MB contiguous block.

- **What is the difference between `GFP_KERNEL` and `GFP_ATOMIC`?** `GFP_KERNEL` may sleep to wait for memory reclaim, so it can only be called in process context. `GFP_ATOMIC` never sleeps and is safe in interrupt context, but may return NULL if memory is tight. `GFP_ATOMIC` allocations consume a small emergency reserve.

- **Why does CMA reserve memory at boot rather than at driver probe time?** By the time a driver probes, the system has been running long enough for memory to fragment. Boot-time reservation guarantees a contiguous region before fragmentation begins.

- **What is the purpose of `SLAB_HWCACHE_ALIGN`?** It pads each slab object to a CPU cache line boundary. This prevents false sharing — two objects that happen to share a cache line being modified by different CPU cores, which would cause expensive cache coherence traffic.

- **When should you use `kmem_cache_create` instead of `kmalloc`?** When you allocate many objects of the same type repeatedly (e.g., per-frame descriptors in a camera driver). A dedicated cache has lower overhead, better memory locality, and shows clearly in `slabtop` for leak detection.

- **How does the OOM killer decide which process to kill?** It computes a `badness` score proportional to each process's RSS and swap usage, modified by `oom_score_adj`. The process with the highest score is killed first. Setting `oom_score_adj = -1000` prevents a process from ever being chosen.

---

**AI Hardware Connection**

- CMA reserves physically contiguous memory at boot for Jetson NVDLA and VIC DMA buffers, ensuring availability before any user-space process has fragmented memory
- `GFP_DMA32` is required when writing PCIe device drivers for AI accelerator cards whose DMA engines cannot address physical memory above 4 GB
- `GFP_ATOMIC` is the correct flag for camera ISR buffer allocation in V4L2 drivers, where the interrupt handler cannot sleep waiting for reclaim
- OOM score protection (`oom_score_adj = -1000`) prevents the inference daemon (modeld, controlsd) from being killed under memory pressure in openpilot's embedded environment
- `slabtop` is the first diagnostic tool for identifying memory leaks in custom sensor drivers where per-frame slab allocations accumulate without matching frees
- `kvmalloc` is the preferred pattern in FPGA driver code that loads variable-size bitstreams, transparently selecting contiguous or non-contiguous backing based on availability

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-14.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-14.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
