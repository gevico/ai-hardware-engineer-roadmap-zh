---
title: 讲义 06（L12、L13、L14）：虚拟内存、页表与 TLB、内核内存分配
description: 讲义 06（L12、L13、L14）：虚拟内存、页表与 TLB、内核内存分配
---

# 讲义 06（L12、L13、L14）：虚拟内存、页表与 TLB、内核内存分配

**合并：** 讲座 L12（虚拟内存与 Linux 内存模型）、L13（页表、TLB 与大页）、L14（内存分配：SLUB、kmalloc 与 CMA）。

---

## 本讲义的组织方式

1. **第 1 部分 — 虚拟内存：** 地址空间；布局（x86-64/ARM64）；mm_struct、VMA；mmap；按需分页；COW；mlock；RSS 与 PSS。
2. **第 2 部分 — 页表与 TLB：** 页表遍历；PTE 位；TLB 与 TLB shootdown；PCID/ASID；大页（2MB、1GB）；THP 与 HugeTLBFS；madvise。
3. **第 3 部分 — 内核分配：** Buddy 分配器；SLUB/kmalloc；vmalloc；zone；GFP 标志；CMA。

---

# 第 1 部分：虚拟内存与 Linux 内存模型

**背景：** 每个进程拥有**私有的虚拟地址空间**。**MMU 按页完成 VA→PA 转换**。隔离、overcommit 与按需分页是核心；**缺页与 swap 对 RT 是关键的**（在热路径中应避免）。

---

## 虚拟地址空间

- **隔离：** 若无内核介入的共享，一个进程无法访问另一个进程的内存。
- **Overcommit：** 跨进程的虚拟内存总量可以超过物理 RAM；页按需分配。
- **布局（x86-64）：** 用户：text、data/BSS、堆（↑）、mmap（↓）、栈（↓）。内核：直接映射、vmalloc、内核 text、模块、vDSO。ARM64：TTBR0 = 用户，TTBR1 = 内核。

---

## mm_struct 与 VMA

- **mm_struct：** 每个进程一个；`pgd`、`mmap` 链表、`mm_rb` 树、`start_code`/`end_code`、`start_brk`/`brk`、`total_vm`、`locked_vm`。
- **VMA（vm_area_struct）：** 权限与后备存储相同的连续区域。字段：`vm_start`/`vm_end`、`vm_flags`（READ、WRITE、EXEC、SHARED、LOCKED）、`vm_file`、`vm_pgoff`、`vm_ops`（fault、open、close）。

**VMA 类型：** text（文件，R-X）；data/BSS（文件/零页）；堆（匿名，零填充）；栈（匿名，向下增长）；文件 mmap 共享（例如模型权重）；匿名共享（例如 VisionIPC）；vDSO（内核提供）。

---

## mmap() 与按需分页

- **mmap()** 创建 VMA；它**不**分配物理页。页在首次访问时分配（按需分页）。
- **缺页类型：** **Minor** — 页在 RAM 中但 PTE 缺失（例如 COW、按需零页）；约 1 µs。**Major** — 页来自磁盘（文件/swap）；1–10 ms；对 RT 是致命的。
- **mlockall(MCL_CURRENT|MCL_FUTURE)** 防止被换出；但不会把未触碰的页调入 — 需**预先触碰**（例如 memset），以避免 RT 循环中出现 minor 缺页。

---

## 写时复制（COW）

`fork()` 之后，父进程与子进程**以只读方式共享页**。首次写入时，内核分配新页、复制内容，并为写入方安装可写 PTE。这使得 `fork()` 为 O(1)；**以只读方式共享的模型权重只占用一份物理副本**。

---

## 查看内存：maps、smaps、RSS 与 PSS

- `/proc/<pid>/maps`：VMA（范围、权限、后备存储）。`/proc/<pid>/smaps`：每个 VMA 的 RSS、PSS、swap、AnonHugePages。
- **RSS：** 常驻集；会对共享页重复计数。**PSS：** 按比例；共享页按共享者数量分摊 — 适用于按进程统计。

---

# 第 2 部分：页表、TLB 与大页

**背景：** 每次访问都需要 VA→PA 转换。**TLB 将其缓存**；未命中会触发**多级页表遍历**（例如 4 次读取，300–400 ns）。**更大的页可降低 TLB 压力**。

---

## 页表遍历（x86-64）

- 48 位 VA 拆分：PGD 索引（9）、PUD（9）、PMD（9）、PTE（9）、offset（12）。CR3 → PGD；每一级一张 4 KB 表（512 项）。TLB 未命中时需四次内存读取。
- **PTE 位：** Present、R/W、U/S、PWT、PCD（禁止缓存 — 用于 MMIO）、Accessed、Dirty、Global、NX、PFN。设备寄存器用 **PCD=1**（`ioremap` 会设置它）。

---

## TLB 与 TLB shootdown

- **TLB：** VA→PA 的硬件缓存；命中耗时 1–6 个周期；未命中 → 硬件遍历。
- **TLB shootdown：** 当一个 CPU 修改 PTE 时，其他 CPU 可能仍持有过期的 TLB 项。内核发送 IPI；各 CPU 刷掉受影响的项。在 CPU 数量多时代价高昂；在 mmap/munmap 密集的工作负载中是热点。

---

## PCID / ASID

- **PCID（x86）：** 按进程给 TLB 项打标签；上下文切换时可保留 TLB（写入 PCID 相同的新 CR3，或使用 NOFLUSH）。降低切换开销。
- **ASID（ARM64）：** TTBR0 中的同类机制；避免切换时整体刷 TLB。

---

## 大页

- **4 KB：** 1 GB = 262K 个 PTE；超出典型 L2 TLB。
- **2 MB（PMD）：** 一个 PMD 项；相同范围内项数减少 512 倍。**HugeTLBFS：** 预分配、锁定；`MAP_HUGETLB`。**THP：** 内核通过 khugepaged 将 4 KB 提升为 2 MB；使用 `madvise` 模式，并在选定区域上使用 `MADV_HUGEPAGE`，以避免不可预测的延迟。
- **1 GB（PUD）：** 仅限启动时；`hugepagesz=1G hugepages=N`。
- **madvise：** `MADV_HUGEPAGE`、`MADV_NOHUGEPAGE`、`MADV_WILLNEED`（预取）、`MADV_DONTNEED`、`MADV_SEQUENTIAL`/`MADV_RANDOM`。

---

# 第 3 部分：内核内存 — Buddy、SLUB、vmalloc、CMA

**背景：** **Buddy** 管理物理页；**SLUB** 从页中切分对象；**vmalloc** 提供虚拟连续而物理不连续的内存；GFP 与 zone 控制从哪里分配以及如何分配；**CMA** 为 DMA 预留连续区域。

---


<details>
<summary>English original</summary>

**Lecture Note 06 (L12, L13, L14): Virtual Memory, Page Tables & TLBs, Kernel Memory Allocation**

**Combines:** Lecture L12 (Virtual Memory & Linux Memory Model), L13 (Page Tables, TLBs & Huge Pages), L14 (Memory Allocation: SLUB, kmalloc & CMA).

---

**How This Note Is Organized**

1. **Part 1 — Virtual memory:** Address space; layout (x86-64/ARM64); mm_struct, VMA; mmap; demand paging; COW; mlock; RSS vs PSS.
2. **Part 2 — Page tables & TLBs:** Page table walk; PTE bits; TLB and TLB shootdown; PCID/ASID; huge pages (2MB, 1GB); THP vs HugeTLBFS; madvise.
3. **Part 3 — Kernel allocation:** Buddy allocator; SLUB/kmalloc; vmalloc; zones; GFP flags; CMA.

---

**Part 1: Virtual Memory & the Linux Memory Model**

**Context:** Each process has a **private virtual address space**. The **MMU translates VA→PA** per page. Isolation, overcommit, and demand paging are central; **page faults and swap are critical for RT** (avoid in hot path).

---

**Virtual Address Space**

- **Isolation:** One process cannot access another's memory without kernel-mediated sharing.
- **Overcommit:** Total virtual across processes can exceed physical RAM; pages allocated on demand.
- **Layout (x86-64):** User: text, data/BSS, heap (↑), mmap (↓), stack (↓). Kernel: direct map, vmalloc, kernel text, modules, vDSO. ARM64: TTBR0 = user, TTBR1 = kernel.

---

**mm_struct & VMA**

- **mm_struct:** One per process; `pgd`, `mmap` list, `mm_rb` tree, `start_code`/`end_code`, `start_brk`/`brk`, `total_vm`, `locked_vm`.
- **VMA (vm_area_struct):** Contiguous region with same permissions and backing. Fields: `vm_start`/`vm_end`, `vm_flags` (READ, WRITE, EXEC, SHARED, LOCKED), `vm_file`, `vm_pgoff`, `vm_ops` (fault, open, close).

**VMA types:** Text (file, R-X); data/BSS (file/zero); heap (anonymous, zero-fill); stack (anonymous, grow-down); file mmap shared (e.g. model weights); anonymous shared (e.g. VisionIPC); vDSO (kernel-provided).

---

**mmap() & Demand Paging**

- **mmap()** creates a VMA; it does **not** allocate physical pages. Pages are allocated on first access (demand paging).
- **Fault types:** **Minor** — page in RAM but PTE absent (e.g. COW, demand-zero); ~1 µs. **Major** — page from disk (file/swap); 1–10 ms; fatal for RT.
- **mlockall(MCL_CURRENT|MCL_FUTURE)** prevents eviction; does not fault in untouched pages — **pre-touch** (e.g. memset) to avoid minor faults in the RT loop.

---

**Copy-on-Write (COW)**

After `fork()`, parent and child **share pages read-only**. On first write, kernel allocates new page, copies content, installs writable PTE for writer. Makes `fork()` O(1); **model weights shared read-only consume one physical copy**.

---

**Inspecting Memory: maps, smaps, RSS vs PSS**

- `/proc/<pid>/maps`: VMAs (range, perms, backing). `/proc/<pid>/smaps`: per-VMA RSS, PSS, swap, AnonHugePages.
- **RSS:** Resident set; double-counts shared pages. **PSS:** Proportional; shared pages split by number of sharers — correct for per-process accounting.

---

**Part 2: Page Tables, TLBs & Huge Pages**

**Context:** Every access needs VA→PA translation. The **TLB caches it**; a miss triggers a **multi-level page table walk** (e.g. 4 reads, 300–400 ns). **Larger pages reduce TLB pressure**.

---

**Page Table Walk (x86-64)**

- 48-bit VA split: PGD index (9), PUD (9), PMD (9), PTE (9), offset (12). CR3 → PGD; each level one 4 KB table (512 entries). Four memory reads on TLB miss.
- **PTE bits:** Present, R/W, U/S, PWT, PCD (cache disable — use for MMIO), Accessed, Dirty, Global, NX, PFN. **PCD=1** for device registers (`ioremap` sets it).

---

**TLB & TLB Shootdown**

- **TLB:** Hardware cache of VA→PA; hit in 1–6 cycles; miss → hardware walk.
- **TLB shootdown:** When one CPU changes a PTE, others may have stale TLB entries. Kernel sends IPI; each CPU flushes affected entries. Expensive on many CPUs; hot in mmap/munmap-heavy workloads.

---

**PCID / ASID**

- **PCID (x86):** Tag TLB entries by process; context switch can keep TLB (new CR3 with same PCID or NOFLUSH). Reduces switch cost.
- **ASID (ARM64):** Same idea in TTBR0; avoids full TLB flush on switch.

---

**Huge Pages**

- **4 KB:** 1 GB = 262K PTEs; exceeds typical L2 TLB.
- **2 MB (PMD):** One PMD entry; 512× fewer entries for same range. **HugeTLBFS:** Pre-allocated, pinned; `MAP_HUGETLB`. **THP:** Kernel promotes 4 KB→2 MB via khugepaged; use `madvise` mode and `MADV_HUGEPAGE` on chosen regions to avoid unpredictable latency.
- **1 GB (PUD):** Boot-time only; `hugepagesz=1G hugepages=N`.
- **madvise:** `MADV_HUGEPAGE`, `MADV_NOHUGEPAGE`, `MADV_WILLNEED` (prefetch), `MADV_DONTNEED`, `MADV_SEQUENTIAL`/`MADV_RANDOM`.

---

**Part 3: Kernel Memory — Buddy, SLUB, vmalloc, CMA**

**Context:** **Buddy** manages physical pages; **SLUB** carves objects from pages; **vmalloc** gives virtual continuity without physical continuity; GFP and zones control where and how; **CMA** reserves contiguous regions for DMA.

---

</details>

## Buddy Allocator

- 按 **order** 0..10（2^order 个页）组织空闲链表。`alloc_pages(gfp, order)` 返回 2^order 个连续页。需要时拆分高阶块；释放时合并 buddy。
- **碎片化：** 即使总空闲内存充足，高阶分配仍可能失败。**CMA** 在启动时、碎片化发生之前预留连续区域。

---

## SLUB 与 kmalloc

- **SLUB：** 每 CPU slab；快路径无锁。CPU slab 满时共享 partial slab 链表。
- **kmem_cache：** 为单一对象类型（如 DMA descriptor）建立的专用缓存；`kmem_cache_alloc`/`kmem_cache_free`。
- **kmalloc(size, gfp)：** 按大小分级的缓存（8、16、… 8K）；超过 8K 走 buddy。`kzalloc` = 清零。进程上下文中用 **GFP_KERNEL**；IRQ/spinlock 中用 **GFP_ATOMIC**（绝不睡眠）。

---

## vmalloc 与 kvmalloc

- **vmalloc(size)：** 虚拟连续、物理分散；页表建在 vmalloc 区域。比 kmalloc 慢；用于不需要 DMA 连续性的较大 buffer。
- **kvmalloc** / **kvfree：** 优先 kmalloc；大尺寸时回退到 vmalloc。

---

## Zone 与 GFP 标志

- **Zone：** ZONE_DMA（0–16 MB）、ZONE_DMA32（0–4 GB）、ZONE_NORMAL（4 GB+）、ZONE_MOVABLE（迁移/CMA）。GFP 选择 zone 与行为。
- **GFP_KERNEL：** 可睡眠、可回收；进程上下文。**GFP_ATOMIC：** 不睡眠/不回收；IRQ/spinlock；可返回 NULL。**GFP_DMA**/ **GFP_DMA32** 用于设备的 DMA 范围。**__GFP_ZERO**、**__GFP_NOFAIL**、**__GFP_NOWARN**。

---

## CMA (Contiguous Memory Allocator)

- 在启动时预留连续物理区域（位于 ZONE_MOVABLE）。可迁移页可以使用它，直到某设备请求；此时 kernel 将其迁移出去。驱动通过 `dma_alloc_*` 或从 CMA alloc_pages 获得连续 DMA 内存。避免大块 DMA buffer 在运行时因碎片化而分配失败。

---

## 汇总表

**VMA / fault：** text/data/heap/stack/文件 mmap/匿名共享 — fault = 文件读取、零填充或换入。minor 约 1 µs；major 1–10 ms。

**页大小 / TLB：** 4 KB → 262K 项/GB；2 MB → 512/GB；1 GB → 1/GB。THP = 动态；HugeTLBFS = 显式、pin 住。

**分配器：** Buddy = 页；SLUB = 对象；kmalloc = 通用；vmalloc = 虚拟连续；CMA = 为 DMA 提供连续内存。

---

## AI 硬件关联

- 对 DMA-BUF fd 调用 **mmap(MAP_SHARED)**：GPU/CPU buffer 零拷贝共享。RT 推理中用 **mlockall** + 预触碰以避免 fault。fork 后的 **COW** 共享模型权重，RAM 不会翻倍。
- 对权重张量使用 **THP / MADV_HUGEPAGE** 减少 TLB miss。用 smaps 中的 **PSS** 做正确的按进程内存统计。**PCID/ASID** 降低上下文切换开销。
- ISR 中用 **GFP_ATOMIC**；进程上下文中用 **GFP_KERNEL**。大块 DMA buffer 用 **CMA** 或提前分配；在碎片化系统上避免高阶 alloc_pages。用 **slabtop** 查 slab 泄漏。

---

*综合 Lecture L12、L13、L14（虚拟内存；页表、TLB、大页；SLUB、kmalloc、CMA）。*


<details>
<summary>English original</summary>

**Buddy Allocator**

- Free lists by **order** 0..10 (2^order pages). `alloc_pages(gfp, order)` returns 2^order contiguous pages. Splits higher-order blocks when needed; merges buddies on free.
- **Fragmentation:** High-order allocation can fail despite total free memory. **CMA** reserves contiguous region at boot before fragmentation.

---

**SLUB & kmalloc**

- **SLUB:** Per-CPU slabs; fast path lock-free. Partial slab list shared when CPU slab is full.
- **kmem_cache:** Dedicated cache for one object type (e.g. DMA descriptors); `kmem_cache_alloc`/`kmem_cache_free`.
- **kmalloc(size, gfp):** Size-based caches (8, 16, … 8K); above 8K uses buddy. `kzalloc` = zeroed. Use **GFP_KERNEL** in process context; **GFP_ATOMIC** in IRQ/spinlock (never sleep).

---

**vmalloc & kvmalloc**

- **vmalloc(size):** Virtually contiguous, physically scattered; page tables built in vmalloc region. Slower than kmalloc; use for large buffers that do not need DMA contiguity.
- **kvmalloc** / **kvfree:** Prefer kmalloc; fall back to vmalloc for large sizes.

---

**Zones & GFP Flags**

- **Zones:** ZONE_DMA (0–16 MB), ZONE_DMA32 (0–4 GB), ZONE_NORMAL (4 GB+), ZONE_MOVABLE (migration/CMA). GFP selects zone and behavior.
- **GFP_KERNEL:** May sleep, reclaim; process context. **GFP_ATOMIC:** No sleep/reclaim; IRQ/spinlock; can return NULL. **GFP_DMA**/ **GFP_DMA32** for device DMA range. **__GFP_ZERO**, **__GFP_NOFAIL**, **__GFP_NOWARN**.

---

**CMA (Contiguous Memory Allocator)**

- Reserves contiguous physical region at boot (in ZONE_MOVABLE). Movable pages can use it until a device requests it; then kernel migrates them out. Driver gets contiguous DMA memory via `dma_alloc_*` or alloc_pages from CMA. Avoids runtime fragmentation failure for large DMA buffers.

---

**Summary Tables**

**VMA / fault:** Text/data/heap/stack/file mmap/anon shared — fault = file read, zero-fill, or swap-in. Minor ~1 µs; major 1–10 ms.

**Page size / TLB:** 4 KB → 262K entries/GB; 2 MB → 512/GB; 1 GB → 1/GB. THP = dynamic; HugeTLBFS = explicit, pinned.

**Allocator:** Buddy = pages; SLUB = objects; kmalloc = general; vmalloc = virtual contiguity; CMA = contiguous for DMA.

---

**AI Hardware Connection**

- **mmap(MAP_SHARED)** on DMA-BUF fd: zero-copy GPU/CPU buffer sharing. **mlockall** + pre-touch in RT inference to avoid faults. **COW** after fork shares model weights without doubling RAM.
- **THP / MADV_HUGEPAGE** on weight tensors reduces TLB misses. **PSS** in smaps for correct per-process memory accounting. **PCID/ASID** reduce context-switch cost.
- **GFP_ATOMIC** in ISR; **GFP_KERNEL** in process context. **CMA** or early allocation for large DMA buffers; avoid high-order alloc_pages on fragmented systems. **slabtop** for slab leaks.

---

*Combines Lectures L12, L13, L14 (Virtual Memory; Page Tables, TLBs, Huge Pages; SLUB, kmalloc, CMA).*

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-Note-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-Note-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
