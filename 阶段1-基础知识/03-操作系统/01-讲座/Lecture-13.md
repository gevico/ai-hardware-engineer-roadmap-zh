---
title: 第 13 讲：页表、TLB 与巨页
description: 第 13 讲：页表、TLB 与巨页
---

# 第 13 讲：页表、TLB 与巨页

## 概述

程序每次访问内存时，CPU 都必须把虚拟地址——程序看到的地址——翻译成物理地址——实际的 DRAM 位置。这项翻译工作由 **内存管理单元（MMU）** 负责，依据的是一种称为**页表**的多级结构。核心挑战在于：每一次内存访问都必须做这个翻译，因此它的速度决定了整个系统的性能。本讲贯穿始终的心智模型是多级目录查找：把它类比成邮政系统，一个完整地址被拆成 国家 → 省 → 市 → 街道 → 门牌号，每一级都作为一张表存储在内存中。对 AI 硬件工程师而言，这一点极其重要：一个为推理加载的 70 亿参数模型要触及数 GB 内存、跨越数百万个虚拟地址，而糟糕的页表设计意味着 CPU 花在地址翻译上的时间比执行推理计算还多。巨页与 TLB 感知在任何生产级 AI 系统中都是一等一的性能手段。

---

## x86-64 四级页表遍历

### 虚拟地址分解

翻译过程首先把虚拟地址切成若干**索引字段**，用它们引导**层次化查找**。一个 48 位规范虚拟地址被拆成**五个字段**：

```
┌─────────────────────────────────────────────────────────┐
│          48-bit Virtual Address Layout                  │
├──────────┬──────────┬──────────┬──────────┬────────────┤
│ [47:39]  │ [38:30]  │ [29:21]  │ [20:12]  │  [11:0]   │
│  9 bits  │  9 bits  │  9 bits  │  9 bits  │  12 bits  │
│  PGD idx │  PUD idx │  PMD idx │  PTE idx │  offset   │
│ (512 ent)│ (512 ent)│ (512 ent)│ (512 ent)│ (4KB page)│
└──────────┴──────────┴──────────┴──────────┴────────────┘
```

```
Bits [47:39] → PGD index   (9 bits, 512 entries)
Bits [38:30] → PUD index   (9 bits, 512 entries)
Bits [29:21] → PMD index   (9 bits, 512 entries)
Bits [20:12] → PTE index   (9 bits, 512 entries)
Bits [11:0]  → page offset (12 bits, 4KB page)
```

遍历：`CR3` 保存 **PGD 的物理地址**。每张表恰好是一个 4KB 页，包含 512 个 8 字节表项。硬件 MMU 在 **TLB 未命中时**自动执行这次遍历，在最终的数据访问之前最多发出**四次顺序内存读取**。

实际执行中的四级遍历——每个箭头代表一次内存读取：

```
CR3 register
    │ (holds PGD physical address)
    ▼
┌─────────┐   [47:39]   ┌─────────┐   [38:30]   ┌─────────┐
│   PGD   │ ──────────> │   PUD   │ ──────────> │   PMD   │
│(4KB tbl)│             │(4KB tbl)│             │(4KB tbl)│
└─────────┘             └─────────┘             └─────────┘
                                                      │ [29:21]
                                                      ▼
                                                ┌─────────┐   [20:12]   ┌──────────┐
                                                │   PTE   │ ──────────> │ Physical │
                                                │(4KB tbl)│             │  Page    │
                                                └─────────┘             └──────────┘
                                                                              │ + [11:0] offset
                                                                              ▼
                                                                        Final data
```

> **关键洞察：** TLB 未命中时，在实际数据访问之前会发生四次内存读取。这正是 TLB 缓存存在的原因——没有它，每次内存访问都要付出 5 倍的内存延迟。冷页表遍历在现代 DDR5 DRAM 上可能耗时 300–400 ns。


<details>
<summary>English original</summary>

**Lecture 13: Page Tables, TLBs & Huge Pages**

**Overview**

Every time a program accesses memory, the CPU must translate a virtual address — the address the program sees — into a physical address — the actual DRAM location. This translation is the job of the **Memory Management Unit (MMU)**, guided by a multi-level structure called the **page table**. The core challenge is that this translation must happen on every single memory access, so its speed determines overall system performance. The mental model to carry through this lecture is a multi-level directory lookup: think of it like a postal system where a full address is broken into country → state → city → street → house number, and each level is stored as a table in memory. For an AI hardware engineer, this matters enormously: a 7-billion-parameter model loaded for inference touches gigabytes of memory across millions of virtual addresses, and poor page table design means the CPU spends more time translating addresses than executing inference computations. Huge pages and TLB-awareness are first-class performance tools in any production AI system.

---

**x86-64 Four-Level Page Table Walk**

**Virtual Address Decomposition**

The translation process begins by slicing a virtual address into **index fields** that guide a **hierarchical lookup**. A 48-bit canonical virtual address is split into **five fields**:

```
┌─────────────────────────────────────────────────────────┐
│          48-bit Virtual Address Layout                  │
├──────────┬──────────┬──────────┬──────────┬────────────┤
│ [47:39]  │ [38:30]  │ [29:21]  │ [20:12]  │  [11:0]   │
│  9 bits  │  9 bits  │  9 bits  │  9 bits  │  12 bits  │
│  PGD idx │  PUD idx │  PMD idx │  PTE idx │  offset   │
│ (512 ent)│ (512 ent)│ (512 ent)│ (512 ent)│ (4KB page)│
└──────────┴──────────┴──────────┴──────────┴────────────┘
```

```
Bits [47:39] → PGD index   (9 bits, 512 entries)
Bits [38:30] → PUD index   (9 bits, 512 entries)
Bits [29:21] → PMD index   (9 bits, 512 entries)
Bits [20:12] → PTE index   (9 bits, 512 entries)
Bits [11:0]  → page offset (12 bits, 4KB page)
```

Walk: `CR3` holds the **physical address of the PGD**. Each table is exactly one 4KB page containing 512 × 8-byte entries. The hardware MMU performs the walk automatically **on a TLB miss**, issuing up to **four sequential memory reads** before the final data access.

The four-level walk in action — each arrow represents one memory read:

```
CR3 register
    │ (holds PGD physical address)
    ▼
┌─────────┐   [47:39]   ┌─────────┐   [38:30]   ┌─────────┐
│   PGD   │ ──────────> │   PUD   │ ──────────> │   PMD   │
│(4KB tbl)│             │(4KB tbl)│             │(4KB tbl)│
└─────────┘             └─────────┘             └─────────┘
                                                      │ [29:21]
                                                      ▼
                                                ┌─────────┐   [20:12]   ┌──────────┐
                                                │   PTE   │ ──────────> │ Physical │
                                                │(4KB tbl)│             │  Page    │
                                                └─────────┘             └──────────┘
                                                                              │ + [11:0] offset
                                                                              ▼
                                                                        Final data
```

> **Key Insight:** Four memory reads happen before the actual data access on a TLB miss. This is why the TLB cache exists — without it, every memory access would cost 5× memory latency. A cold page table walk can take 300–400 ns on modern DDR5 DRAM.

</details>

### 页表项（PTE）字段

每个 8 字节的 PTE 项不只编码了**物理帧号**，还编码了**权限位**和**缓存策略提示**：

| 位 | 名称 | 功能 |
|-----|------|----------|
| 0 | Present (P) | 项有效；清零则触发 page fault |
| 1 | R/W | 0 = 只读，1 = 读写 |
| 2 | U/S | 0 = 仅特权态，1 = 用户态可访问 |
| 3 | PWT | 页写穿（Write-Through）缓存策略 |
| 4 | PCD | 页缓存禁用（uncached MMIO） |
| 5 | Accessed (A) | 任意读取时由硬件置位；供页回收使用 |
| 6 | Dirty (D) | 写入时由硬件置位；供换页器使用 |
| 7 | PAT | 页属性表（PAT）索引扩展 |
| 8 | Global (G) | CR3 重载时跳过 TLB 刷新（内核页） |
| 63 | NX/XD | 不可执行；阻止取指 |
| 51:12 | PFN | 物理帧号 |

**PCD** 位对 AI 硬件工程师尤为重要：MMIO 映射的设备寄存器（PCIe BAR 窗口、GPU 寄存器）必须以 PCD=1（禁用缓存）映射，以确保读取总是落到硬件，而不是过期的 CPU 缓存行。

> **常见坑：** 把 MMIO 区域映射为可缓存（PCD=0）是一个隐蔽却严重的 bug。读取可能返回来自 CPU 缓存的过期值，而不是硬件寄存器。症状是驱动行为非确定性的。设备寄存器区域始终应使用 `ioremap()`（它会设置 PCD），而不是手工构造 PTE。

---

## ARM64 页表遍历

ARM64 使用**两个独立的基址寄存器**：`TTBR0_EL1` 用于用户空间（低 VA，`0x0000...`），`TTBR1_EL1` 用于内核空间（高 VA，`0xFFFF...`）。这避免了在进入内核时**切换单个 CR3** 的需要。

这是与 x86 的关键架构差异。在 x86 上，CR3 同时保存内核与用户空间的页表根，因此进入内核可能需要 TLB 管理。在 ARM64 上，两个 TTBR 让内核的 TLB 项保持不动，同时替换用户空间项——降低内核进入/退出开销。

```
User VA (0x0000...)          Kernel VA (0xFFFF...)
       │                              │
       ▼                              ▼
  TTBR0_EL1                     TTBR1_EL1
  (User PGD)                  (Kernel PGD)
       │                              │
       └──────────┬───────────────────┘
                  ▼
           MMU walk (same
           table structure,
           separate roots)
```

| 配置 | 遍历深度 | 说明 |
|---------------|-----------|-------|
| 4KB 页，48 位 VA | 4 级（PGD→PUD→PMD→PTE） | 标准 Linux ARM64 |
| 4KB 页，52 位 VA | 5 级（ARMv8.2-LPA） | 大物理地址扩展 |
| 16KB 页，48 位 VA | 3 级 | 深度降低，粒度更粗 |
| 64KB 页，48 位 VA | 2–3 级 | 每 GB 的 TLB 项数少 |

> **关键洞察：** ARM64 的 64KB 页粒度选项在 Jetson 级 SoC 上很有价值。它把页表遍历深度降到 2–3 级，缩小了大型连续缓冲区（如 DMA 输入张量和 feature map）的地址转换开销。

---

## 转译后备缓冲器

**TLB** 是近期虚拟地址→物理地址转换的硬件缓存。**命中**时，CPU 在**一个周期**内取得物理地址。**未命中**时，硬件页表遍历器执行完整的多级遍历。

可以把 TLB 想象成 CPU 的通讯录。当你经常拨打同一个人，你会记住他的号码，不需要再查。当通讯录太小，装不下所有条目，查找就变得昂贵。

```
Virtual Address
      │
      ▼
 ┌──────────┐  HIT (1 cycle)   ┌──────────────┐
 │  L1 TLB  │ ───────────────> │ Physical Addr│
 │ (64 ent) │                  └──────────────┘
 └──────────┘
      │ MISS
      ▼
 ┌──────────┐  HIT (6-8 cycles) ┌──────────────┐
 │  L2 TLB  │ ────────────────> │ Physical Addr│
 │(1536 ent)│                   └──────────────┘
 └──────────┘
      │ MISS
      ▼
 Hardware Page Table Walker
 (4 sequential DRAM reads, 300-400 ns)
      │
      ▼
 Physical Address + TLB fill
```

### 典型 TLB 层级（Intel Skylake / AMD Zen）

| 层级 | 类型 | 条目数 | 延迟 |
|-------|------|---------|---------|
| L1 iTLB | 指令 | 128（4KB），8（2MB） | 1 周期 |
| L1 dTLB | 数据 | 64（4KB），32（2MB） | 1 周期 |
| L2 TLB | 统一 | 1024–1536（4KB） | 6–8 周期 |
| DRAM 未命中时硬件遍历器 | — | — | 100–200 周期 |

一次冷遍历在 uncached DRAM 中触达全部四级页表，代价是四次串行 DRAM 往返（在 80 ns 延迟的 DDR5 上约 300–400 ns），这主导了稀疏访问模式的内存访问时间。


<details>
<summary>English original</summary>

**Page Table Entry (PTE) Fields**

Each 8-byte PTE entry encodes not just the **physical frame number** but also **permission bits** and **cache policy hints**:

| Bit | Name | Function |
|-----|------|----------|
| 0 | Present (P) | Entry valid; page fault if clear |
| 1 | R/W | 0 = read-only, 1 = read/write |
| 2 | U/S | 0 = supervisor only, 1 = user accessible |
| 3 | PWT | Page Write-Through cache policy |
| 4 | PCD | Page Cache Disable (uncached MMIO) |
| 5 | Accessed (A) | Set by hardware on any read; used by page reclaim |
| 6 | Dirty (D) | Set by hardware on write; used by swapper |
| 7 | PAT | Page Attribute Table index extension |
| 8 | Global (G) | Skip TLB flush on CR3 reload (kernel pages) |
| 63 | NX/XD | No-Execute; prevents instruction fetch |
| 51:12 | PFN | Physical Frame Number |

The **PCD** bit is especially important for AI hardware engineers: MMIO-mapped device registers (PCIe BAR windows, GPU registers) must be mapped with PCD=1 (cache disabled) to ensure reads always go to the hardware, not a stale CPU cache line.

> **Common Pitfall:** Mapping MMIO regions as cached (PCD=0) is a subtle but serious bug. Reads may return stale values from CPU cache instead of the hardware register. The symptom is non-deterministic driver behavior. Always use `ioremap()` (which sets PCD) rather than manually constructing PTEs for device register regions.

---

**ARM64 Page Table Walk**

ARM64 uses **two separate base registers**: `TTBR0_EL1` for user-space (low VA, `0x0000...`) and `TTBR1_EL1` for kernel-space (high VA, `0xFFFF...`). This avoids the need to **swap a single CR3** on kernel entry.

This is a key architectural difference from x86. On x86, CR3 holds both kernel and user-space page table roots, so a kernel entry may require TLB management. On ARM64, the two TTBRs allow the kernel's TLB entries to remain in place while user-space entries are replaced — reducing kernel entry/exit overhead.

```
User VA (0x0000...)          Kernel VA (0xFFFF...)
       │                              │
       ▼                              ▼
  TTBR0_EL1                     TTBR1_EL1
  (User PGD)                  (Kernel PGD)
       │                              │
       └──────────┬───────────────────┘
                  ▼
           MMU walk (same
           table structure,
           separate roots)
```

| Configuration | Walk depth | Notes |
|---------------|-----------|-------|
| 4KB pages, 48-bit VA | 4 levels (PGD→PUD→PMD→PTE) | Standard Linux ARM64 |
| 4KB pages, 52-bit VA | 5 levels (ARMv8.2-LPA) | Large Physical Address extension |
| 16KB pages, 48-bit VA | 3 levels | Reduced depth, coarser granularity |
| 64KB pages, 48-bit VA | 2–3 levels | Low TLB entry count per GB |

> **Key Insight:** ARM64's 64KB page granule option is valuable on Jetson-class SoCs. It reduces the page table walk depth to 2–3 levels, shrinking translation overhead for large contiguous buffers like DMA input tensors and feature maps.

---

**Translation Lookaside Buffer**

The **TLB** is a hardware cache of recent virtual→physical translations. On a **hit**, the CPU obtains the physical address in **one cycle**. On a **miss**, the hardware page table walker executes the full multi-level walk.

Think of the TLB as the CPU's address book. When you frequently call the same person, you remember their number and don't need to look it up again. When the address book is too small to hold all entries, lookups become expensive.

```
Virtual Address
      │
      ▼
 ┌──────────┐  HIT (1 cycle)   ┌──────────────┐
 │  L1 TLB  │ ───────────────> │ Physical Addr│
 │ (64 ent) │                  └──────────────┘
 └──────────┘
      │ MISS
      ▼
 ┌──────────┐  HIT (6-8 cycles) ┌──────────────┐
 │  L2 TLB  │ ────────────────> │ Physical Addr│
 │(1536 ent)│                   └──────────────┘
 └──────────┘
      │ MISS
      ▼
 Hardware Page Table Walker
 (4 sequential DRAM reads, 300-400 ns)
      │
      ▼
 Physical Address + TLB fill
```

**Typical TLB Hierarchy (Intel Skylake / AMD Zen)**

| Level | Type | Entries | Latency |
|-------|------|---------|---------|
| L1 iTLB | Instruction | 128 (4KB), 8 (2MB) | 1 cycle |
| L1 dTLB | Data | 64 (4KB), 32 (2MB) | 1 cycle |
| L2 TLB | Unified | 1024–1536 (4KB) | 6–8 cycles |
| Hardware walker on DRAM miss | — | — | 100–200 cycles |

A cold walk touching all four page table levels in uncached DRAM costs four sequential DRAM round-trips (~300–400 ns on DDR5 at 80 ns latency), dominating memory access time for sparse access patterns.

</details>

### SMP 上的 TLB Shootdown

当某颗 CPU 修改了其他 CPU 可能已缓存在其 TLB 中的 PTE 时，所有过期副本都必须被无效化。该过程称为 **TLB shootdown**，是内核中开销较高的同步操作之一：

1. **修改 PTE**（在页表内存中）— 更新物理映射。
2. **在本地发出 `INVLPG vaddr`** — 立即刷新本地 CPU 的过期条目。
3. **向所有可能持有该映射的 CPU 发送 IPI** — 跨处理器中断。
4. **目标 CPU 执行 `INVLPG` 或 `TLBI`（ARM64）** — 每个远端 CPU 刷新其副本。
5. **发起 CPU 等待所有确认** — 必须等到每颗 CPU 都确认刷新后才能继续。

`flush_tlb_range()` 实现了这一点。Shootdown 在 **CPU 数量多时开销很大**（N×IPI 往返延迟），并且在 `mmap()`/`munmap()` 密集型工作负载（如 Python 内存分配器和 JVM 垃圾回收器）中是一条 **热路径**。

> **常见陷阱：** 在频繁调用 `mmap()`/`munmap()`（例如反复加载模型分片）的多线程推理服务器中，CPU 数量多时 TLB shootdown 会成为瓶颈。用 `perf stat -e tlb:tlb_flush` 进行性能分析以检测此问题。考虑用 `mmap(MAP_FIXED)` 预分配固定内存池，而不是按请求重新映射。

---

## PCID 与 ASID：上下文切换优化

如果没有进程标记，每次上下文切换都需要 **刷新整个 TLB**，以防止一个进程使用另一个进程缓存的地址转换。**PCID**（x86）和 **ASID**（ARM64）通过用拥有它的进程 **标记每个 TLB 条目** 来解决这个问题。

### PCID（x86，进程上下文标识符）

**PCID** 是存储在 `CR3[11:0]` 中的 12-bit 标记。TLB 条目携带安装它们的进程的 PCID。在上下文切换时，新的 CR3 值携带新进程的 PCID。当 CR3 中设置 `NOFLUSH` 位时，来自其他 PCID 的 TLB 条目会被保留，但对新进程不可见。Linux 在 kernel ≥ 4.14 上启用 PCID，在系统调用密集型工作负载中将上下文切换开销降低 10–20%。

### ASID（ARM64，地址空间标识符）

**ASID** 是写入 `TTBR0` 的 8-bit 或 16-bit 标记（可在 kernel 构建时配置）。每个 `mm_struct` 分配一个唯一 ASID。当 ASID 空间耗尽时，一次 rollover 事件会在所有核上触发全局 TLB 刷新和 ASID 重新分配。

> **关键洞察：** 在频繁切换推理线程与 kernel I/O 线程（例如处理 GPU 完成中断）的实时推理守护进程中，PCID/ASID 可避免每次上下文切换时昂贵的完整 TLB 刷新。这是仅通过运行现代 Linux 内核就能实现的“免费” 10–20% 提升。

---

## 标准页 vs. 大页

既然已经理解了 TLB 条目是什么以及它们为何有限，就能明白页大小为何如此重要：更大的页使每个 TLB 条目覆盖更多地址空间。

### 4KB 标准页

- 细粒度物理内存保护与分配
- 1 GB 分配 → 262,144 个 PTE → 512 个页表 → 2 MB 页表内存
- 以零 miss 覆盖 1 GB 需要 262,144 个 TLB 条目；远超 L2 TLB 容量
- 7B 参数模型在 fp16（14 GB）下需要约 3.6 million 个活跃 PTE

使用 4KB 页和 1536 个条目的 L2 TLB，7B 模型会导致 **持续的 TLB miss**。每次 miss 都会触发一次耗时数百纳秒的硬件遍历。在触及每个权重张量的前向传播期间，这会 **迅速累积**。


<details>
<summary>English original</summary>

**TLB Shootdown on SMP**

When a CPU modifies a PTE that other CPUs may have cached in their TLBs, all stale copies must be invalidated. This process is called a **TLB shootdown** and is one of the more expensive synchronization operations in the kernel:

1. **Modify the PTE** in page table memory — update the physical mapping.
2. **Issue `INVLPG vaddr` locally** — flush the local CPU's stale entry immediately.
3. **Send IPI to all CPUs** that may hold the mapping — cross-processor interrupt.
4. **Target CPUs execute `INVLPG` or `TLBI` (ARM64)** — each remote CPU flushes its copy.
5. **Originating CPU waits for all acknowledgements** — must not proceed until every CPU has confirmed the flush.

`flush_tlb_range()` implements this. Shootdown is **expensive at high CPU counts** (N×IPI round-trip latency) and is a **hot path** in `mmap()`/`munmap()`-heavy workloads such as Python memory allocators and JVM garbage collectors.

> **Common Pitfall:** In multi-threaded inference servers that frequently call `mmap()`/`munmap()` (e.g., repeatedly loading model shards), TLB shootdowns become a bottleneck at high CPU counts. Profile with `perf stat -e tlb:tlb_flush` to detect this. Consider pre-allocating fixed memory pools with `mmap(MAP_FIXED)` rather than remapping per request.

---

**PCID and ASID: Context-Switch Optimization**

Without process tagging, every context switch requires **flushing the entire TLB** to prevent one process from using another's cached translations. **PCID** (x86) and **ASID** (ARM64) solve this by **tagging each TLB entry** with the process that owns it.

**PCID (x86, Process Context Identifier)**

**PCID** is a 12-bit tag stored in `CR3[11:0]`. TLB entries carry the PCID of the process that installed them. On a context switch, the new CR3 value carries the new process PCID. With the `NOFLUSH` bit set in CR3, TLB entries from other PCIDs are retained but invisible to the new process. Linux enables PCID on kernel ≥ 4.14, reducing context-switch overhead by 10–20% in syscall-heavy workloads.

**ASID (ARM64, Address Space Identifier)**

**ASID** is an 8-bit or 16-bit tag (configurable at kernel build time) written into `TTBR0`. Each `mm_struct` is assigned a unique ASID. When the ASID space is exhausted, a rollover event triggers a global TLB flush and ASID reassignment across all cores.

> **Key Insight:** In a real-time inference daemon that frequently switches between inference threads and kernel I/O threads (e.g., handling GPU completion interrupts), PCID/ASID prevents the costly full TLB flush at each context switch. This is a "free" 10–20% improvement enabled simply by running a modern Linux kernel.

---

**Standard vs. Huge Pages**

Now that we understand what TLB entries are and why they're limited, we can see why page size matters so much: larger pages cover more address space per TLB entry.

**4KB Standard Pages**

- Fine-grained physical memory protection and allocation
- 1 GB allocation → 262,144 PTEs → 512 page tables → 2 MB of page table memory
- 262,144 TLB entries required to cover 1 GB with zero misses; far exceeds L2 TLB capacity
- A 7B parameter model at fp16 (14 GB) requires ~3.6 million active PTEs

With 4KB pages and an L2 TLB of 1536 entries, a 7B model causes **constant TLB misses**. Every miss triggers a hardware walk costing hundreds of nanoseconds. This **adds up quickly** during a forward pass that touches every weight tensor.

</details>

### 2MB 大页（PMD 级，x86）

单个标记为大页的 **PMD 条目** 会 **直接覆盖 2MB**；PMD[21] `PS` 位表示叶子条目。相同地址范围内，TLB 表项数量 **减少 512×**。

```
4KB Pages (1 GB of model weights)         2MB Huge Pages (same 1 GB)
┌─────────────────────────────┐           ┌─────────────────────────────┐
│ PTE[0]   → 4KB page         │           │ PMD[0]  → 2MB huge page     │
│ PTE[1]   → 4KB page         │           │ PMD[1]  → 2MB huge page     │
│ ...                          │           │ ...                          │
│ PTE[262143] → 4KB page      │           │ PMD[511] → 2MB huge page    │
│                              │           │                              │
│ 262,144 TLB entries needed  │           │ 512 TLB entries needed      │
│ (exceeds L2 TLB capacity)   │           │ (fits in L2 TLB)            │
└─────────────────────────────┘           └─────────────────────────────┘
```

**HugeTLBFS**（显式预分配）：
```bash
echo 512 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
mmap(NULL, size, PROT_READ|PROT_WRITE,
     MAP_HUGETLB|MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)
```
页被 pinned；不能被 swap 或迁移。用于数据库（PostgreSQL shared_buffers）、DPDK packet pools 以及 GPU 驱动 host-pinned 缓冲区。

**透明大页（THP）**（内核自动提升）：
```bash
echo always  > /sys/kernel/mm/transparent_hugepage/enabled
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
echo never   > /sys/kernel/mm/transparent_hugepage/enabled

# Per-VMA opt-in (effective under madvise mode)
madvise(addr, len, MADV_HUGEPAGE);
```
`khugepaged` 内核线程扫描匿名 VMA，并在对齐和可用性允许时，将 512 个连续 4KB 页折叠为一个 PMD 条目。`/proc/vmstat` 字段 `thp_collapse_alloc` 和 `thp_split_page` 跟踪提升和降级事件。

THP 使用 **`madvise` 模式** 将控制权交给应用：只有显式标记为 `MADV_HUGEPAGE` 的区域才会被提升。这是 **推理工作负载的推荐模式** —— `always` 模式可能因后台 khugepaged 活动导致 **意外延迟尖峰**。

> **常见陷阱：**将 THP 设为 `always` 可能导致延迟敏感的推理服务器出现意外延迟。`khugepaged` 后台线程会在不可预测的时间折叠页，造成短暂停顿。应改用 `madvise` 模式，并只在测量到 TLB 压力的特定权重张量分配上调用 `madvise(MADV_HUGEPAGE)`。

### 1GB 大页（PUD 级，x86）

单个 PUD 叶子条目覆盖 1GB。只能通过内核启动参数进行静态分配：
```
hugepagesz=1G hugepages=4
```
无法在 runtime 释放。最大程度减少全模型权重缓冲区的 TLB 占用。

---

## madvise() 提示

`madvise()` 系统调用是向内核 VM 传达内存访问模式的主要接口。可将其视为给内核的提示，以便它针对你的工作负载优化行为。`madvise(addr, len, advice)` 向内核 VM 传达访问模式提示：

| 提示 | 效果 |
|------|--------|
| `MADV_HUGEPAGE` | 为此 VMA 启用 THP |
| `MADV_NOHUGEPAGE` | 禁用 THP；强制使用 4KB 页 |
| `MADV_WILLNEED` | 将页预取到 RAM（同步预读） |
| `MADV_DONTNEED` | 释放物理页；保留 VMA 映射 |
| `MADV_SEQUENTIAL` | 为线性扫描增大预读窗口 |
| `MADV_RANDOM` | 禁用预读；随机访问模式 |
| `MADV_FREE` | 页可能被延迟回收；回收时数据丢失 |

对 AI 工作负载而言，`MADV_WILLNEED` **尤其有价值**：在首次推理调用前对模型权重区域调用它，会强制内核立即将这些页读入 RAM，从而从关键推理路径中 **消除缺页延迟**。

---

## 监控 TLB 与大页行为

概念框架已经就绪，这些监控工具可以准确展示内核在 runtime 对页和地址转换的处理：

`/proc/[pid]/smaps` 每 VMA 字段：
- `AnonHugePages`：此 VMA 中由 2MB THP 支持的字节数
- `THPeligible: 1`：VMA 满足提升所需的大小和对齐条件

`/proc/vmstat` 系统级计数器：
- `thp_fault_alloc`：在缺页时直接分配的 THP
- `thp_collapse_alloc`：由 khugepaged 后台扫描创建的 THP
- `thp_split_page`：THP 回退拆分为 4KB（对齐失败、写时复制等）

`perf stat -e dTLB-load-misses,iTLB-load-misses`：每次运行的硬件 PMU TLB 缺失率。

高 `thp_split_page` 率表明大页被分配后立即被拆分 —— 通常是由于写时复制 fork 或跨越对齐边界的内存映射。出现这种情况时，应检查 VMA 对齐。

---


<details>
<summary>English original</summary>

**2MB Huge Pages (PMD-level, x86)**

A single **PMD entry** marked as a huge page covers **2MB directly**; the PMD[21] `PS` bit indicates a leaf entry. TLB entry count **reduced 512×** for the same address range.

```
4KB Pages (1 GB of model weights)         2MB Huge Pages (same 1 GB)
┌─────────────────────────────┐           ┌─────────────────────────────┐
│ PTE[0]   → 4KB page         │           │ PMD[0]  → 2MB huge page     │
│ PTE[1]   → 4KB page         │           │ PMD[1]  → 2MB huge page     │
│ ...                          │           │ ...                          │
│ PTE[262143] → 4KB page      │           │ PMD[511] → 2MB huge page    │
│                              │           │                              │
│ 262,144 TLB entries needed  │           │ 512 TLB entries needed      │
│ (exceeds L2 TLB capacity)   │           │ (fits in L2 TLB)            │
└─────────────────────────────┘           └─────────────────────────────┘
```

**HugeTLBFS** (explicit pre-allocation):
```bash
echo 512 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
mmap(NULL, size, PROT_READ|PROT_WRITE,
     MAP_HUGETLB|MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)
```
Pages are pinned; cannot be swapped or migrated. Used by databases (PostgreSQL shared_buffers), DPDK packet pools, and GPU driver host-pinned buffers.

**Transparent Huge Pages (THP)** (automatic kernel promotion):
```bash
echo always  > /sys/kernel/mm/transparent_hugepage/enabled
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
echo never   > /sys/kernel/mm/transparent_hugepage/enabled

# Per-VMA opt-in (effective under madvise mode)
madvise(addr, len, MADV_HUGEPAGE);
```
The `khugepaged` kernel thread scans anonymous VMAs and collapses 512 contiguous 4KB pages into one PMD entry when alignment and availability allow. `/proc/vmstat` fields `thp_collapse_alloc` and `thp_split_page` track promotion and demotion events.

THP uses **`madvise` mode** to give the application control: only regions explicitly marked with `MADV_HUGEPAGE` are promoted. This is the **recommended mode for inference workloads** — `always` mode can cause **unexpected latency spikes** from background khugepaged activity.

> **Common Pitfall:** Setting THP to `always` can cause unexpected latency in latency-sensitive inference servers. The `khugepaged` background thread collapses pages at unpredictable times, causing brief stalls. Use `madvise` mode instead and call `madvise(MADV_HUGEPAGE)` only on the specific weight tensor allocations where TLB pressure is measured.

**1GB Huge Pages (PUD-level, x86)**

Single PUD leaf entry covers 1GB. Static-only allocation via kernel boot parameter:
```
hugepagesz=1G hugepages=4
```
Cannot be freed at runtime. Maximally reduces TLB footprint for full-model weight buffers.

---

**madvise() Hints**

The `madvise()` system call is the main interface for communicating memory access patterns to the kernel VM. Think of it as hints you give the kernel so it can optimize its behavior for your workload. `madvise(addr, len, advice)` communicates access pattern hints to the kernel VM:

| Hint | Effect |
|------|--------|
| `MADV_HUGEPAGE` | Enable THP for this VMA |
| `MADV_NOHUGEPAGE` | Disable THP; force 4KB pages |
| `MADV_WILLNEED` | Prefetch pages into RAM (synchronous readahead) |
| `MADV_DONTNEED` | Release physical pages; VMA mapping retained |
| `MADV_SEQUENTIAL` | Increase readahead window for linear scan |
| `MADV_RANDOM` | Disable readahead; random access pattern |
| `MADV_FREE` | Pages may be reclaimed lazily; data lost on reclaim |

For AI workloads, `MADV_WILLNEED` is **particularly valuable**: calling it on model weight regions before the first inference call forces the kernel to read those pages into RAM immediately, **eliminating page-fault latency** from the critical inference path.

---

**Monitoring TLB and Huge Page Behavior**

With the conceptual framework in place, these monitoring tools show exactly what the kernel is doing with pages and translations at runtime:

`/proc/[pid]/smaps` per-VMA fields:
- `AnonHugePages`: bytes in this VMA backed by 2MB THP
- `THPeligible: 1`: VMA meets size and alignment criteria for promotion

`/proc/vmstat` system-wide counters:
- `thp_fault_alloc`: THP allocated directly on page fault
- `thp_collapse_alloc`: THP created by khugepaged background scan
- `thp_split_page`: THP split back to 4KB (alignment failure, copy-on-write, etc.)

`perf stat -e dTLB-load-misses,iTLB-load-misses`: hardware PMU TLB miss rate per run.

A high `thp_split_page` rate indicates huge pages are being allocated and then immediately split — often due to copy-on-write forks or memory mappings that cross alignment boundaries. Investigate VMA alignment when this occurs.

---

</details>

## 小结

| 页大小 | 架构层级 | 1GB 页所需的 TLB 条目数 | 分配方式 | 适用场景 |
|-----------|-----------|---------------------|-------------------|----------|
| 4KB | PTE | 262,144 | 默认 `mmap` / 缺页异常 | 通用场景，保护粒度细 |
| 2MB | PMD | 512 | THP（自动）或 HugeTLBFS（显式） | 模型权重、DPDK 缓冲区、GPU pinned memory |
| 1GB | PUD | 1 | HugeTLBFS 静态启动参数 | 整模型单次映射、NUMA 服务器 |
| 64KB (ARM64) | PTE | 16,384 | 内核编译配置（`PAGE_SIZE=64K`） | 采用 64KB granule 的 ARM SoC，遍历层级更少 |

### 概念回顾

- **TLB 是什么，为什么存在？** TLB 是虚拟地址到物理地址转换的硬件缓存。它之所以存在，是因为完整的 4 级页表遍历最多需要 4 次 DRAM 读取，若不加以缓存，每次内存访问都会慢 5×。
- **为什么大页能降低 TLB 缺失率？** 每个 TLB 条目覆盖整个页的大小。覆盖相同的地址范围，2MB 页所需的 TLB 条目数比 4KB 页少 512×。条目更少，意味着在固定大小的 TLB 中命中率更高。
- **什么是 TLB shootdown，何时发生？** TLB shootdown 是 PTE 被修改时触发的跨 CPU 失效序列。之所以需要它，是因为每个 CPU 都独立缓存地址转换。它发生在 `munmap()`、`mprotect()` 以及写时复制期间。
- **HugeTLBFS 与 THP 有什么区别？** HugeTLBFS 页在启动时或通过 sysfs 预分配，已被 pin 住，不能换出。THP 页由 `khugepaged` 合并 512 个相邻 4KB 页动态创建。HugeTLBFS 更可预测；THP 更灵活。
- **什么时候该用 `MADV_WILLNEED`？** 当首次访问的延迟很关键时，在对大数据区域（模型权重、embedding 表）第一次访问之前使用。它会触发异步内核预读，把页预先填充进 RAM。
- **为什么 `PCD=1` 对设备驱动开发者很重要？** MMIO 寄存器不能被 CPU 缓存。设置 PCD=1 可确保每次寄存器读取都直接抵达硬件。使用 `ioremap()`（会自动设置 PCD）才是正确的驱动写法。

---

## 与 AI 硬件的联系

- 配合 `MADV_HUGEPAGE` 的 THP 可降低 PyTorch 与 TensorRT 模型权重张量的 TLB 缺失率；一个 7B 参数的 fp16 模型在 2MB 粒度下需要 27 个 PMD 条目，而在 4KB 粒度下需要 360 万个 PTE
- HugeTLBFS 在带网络的 AI 推理服务器上为 DPDK 数据包缓冲池预分配 2MB 页，保证线速数据包处理期间零 TLB 压力
- PCID（x86）与 ASID（ARM64）消除了推理守护进程与内核 I/O 线程之间上下文切换时的全量 TLB 刷新，当 GPU 完成中断与推理共用同一批 CPU 核时尤为关键
- 对模型权重文件执行 `madvise(MADV_WILLNEED)`，可在首次推理调用前把页预缺页进 RAM，从而将缺页延迟从冷启动推理的关键路径上移除
- NVIDIA GPU 驱动在主机侧使用 2MB PMD 条目映射大块 VRAM BAR aperture，降低 CPU 通过 PCIe BAR 窗口访问 GPU 内存时主机的 TLB 压力
- 在采用 64KB 页 granule 构建 ARM64 内核的 Jetson 上，NVDLA DMA 分配覆盖大块连续输入特征图缓冲区所需的页表层级更少、TLB 条目也更少


<details>
<summary>English original</summary>

**Summary**

| Page size | Arch level | TLB entries for 1GB | Allocation method | Use case |
|-----------|-----------|---------------------|-------------------|----------|
| 4KB | PTE | 262,144 | Default `mmap` / page fault | General purpose, fine protection granularity |
| 2MB | PMD | 512 | THP (auto) or HugeTLBFS (explicit) | Model weights, DPDK buffers, GPU pinned memory |
| 1GB | PUD | 1 | HugeTLBFS static boot parameter | Full-model single-mapping, NUMA servers |
| 64KB (ARM64) | PTE | 16,384 | Kernel build config (`PAGE_SIZE=64K`) | ARM SoC with 64KB granule, fewer walk levels |

**Conceptual Review**

- **What is the TLB and why does it exist?** The TLB is a hardware cache of virtual-to-physical address translations. It exists because a full 4-level page table walk requires up to 4 DRAM reads, which would make every memory access 5× slower without caching.

- **Why do huge pages reduce TLB miss rate?** Each TLB entry covers the entire page size. A 2MB page needs 512× fewer TLB entries than 4KB pages for the same address range. Fewer entries means a higher hit rate in the fixed-size TLB.

- **What is a TLB shootdown and when does it happen?** A TLB shootdown is a cross-CPU invalidation sequence triggered when a PTE is modified. It is needed because each CPU caches translations independently. It happens during `munmap()`, `mprotect()`, and copy-on-write.

- **What is the difference between HugeTLBFS and THP?** HugeTLBFS pages are pre-allocated at boot or via sysfs, are pinned, and cannot be swapped. THP pages are created dynamically by `khugepaged` by collapsing 512 adjacent 4KB pages. HugeTLBFS is more predictable; THP is more flexible.

- **When should you use `MADV_WILLNEED`?** Before the first access to a large data region (model weights, embedding tables) when latency on first access is critical. It triggers asynchronous kernel readahead to pre-populate pages into RAM.

- **Why does `PCD=1` matter for device driver authors?** MMIO registers must not be cached by the CPU. Setting PCD=1 ensures every register read goes directly to the hardware. Using `ioremap()` (which sets PCD automatically) is the correct driver pattern.

---

**AI Hardware Connection**

- THP with `MADV_HUGEPAGE` reduces TLB miss rate for PyTorch and TensorRT model weight tensors; a 7B parameter fp16 model needs 27 PMD entries at 2MB granularity versus 3.6 million PTEs at 4KB
- HugeTLBFS pre-allocates 2MB pages for DPDK packet buffer pools on network-attached AI inference servers, guaranteeing zero TLB pressure during line-rate packet processing
- PCID (x86) and ASID (ARM64) eliminate full TLB flushes on context switches between the inference daemon and kernel I/O threads, critical when GPU completion interrupts and inference share the same CPU cores
- `madvise(MADV_WILLNEED)` on model weight files pre-faults pages into RAM before the first inference call, removing page-fault latency from the critical path of cold-start inference
- NVIDIA GPU drivers map large VRAM BAR apertures using 2MB PMD entries on the host side, reducing host TLB pressure when CPU accesses GPU memory through the PCIe BAR window
- On Jetson with the ARM64 kernel built for 64KB page granule, NVDLA DMA allocations require fewer page table levels and fewer TLB entries to cover large contiguous input feature map buffers

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-13.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-13.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
