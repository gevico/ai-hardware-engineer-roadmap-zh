---
title: 第 12 讲：虚拟内存与 Linux 内存模型
description: 第 12 讲：虚拟内存与 Linux 内存模型
---

# 第 12 讲：虚拟内存与 Linux 内存模型

## 概述

本讲要解决的核心问题是：在硬件只有固定容量物理 RAM 的前提下，每个进程如何获得自己私有的、看似无限的内存空间？虚拟内存是操作系统最根本的抽象——它让每个进程以为自己独占整个地址空间，隐藏物理内存布局的细节，并在 RAM 不足时让操作系统把页换出到磁盘。这里要建立的思维模型是旅馆：每位客人以为自己有单独的房间（虚拟地址），但旅馆按需把房间对应到实际的床位（物理内存），房间满了还能把行李存进库房（swap）。对 AI 硬件工程师而言，虚拟内存直接影响推理延迟（缺页可额外增加 10ms）、GPU 缓冲区共享（零拷贝需要共享映射）以及内存统计（RSS 与 PSS 决定系统会不会 OOM）。

---

## 虚拟地址空间

每个进程都有私有的 **虚拟地址空间**，与其他所有进程相互隔离。**MMU** 借助**页表树**按页把虚拟地址翻译成物理地址。除非经由内核显式中介的共享，任何进程都无法直接访问另一进程的物理内存。

关键特性：
- **隔离**：一个进程中的野指针无法破坏另一个进程的内存
- **超售**：所有进程的虚拟内存总量可以超过物理 RAM；页按需分配
- **抽象**：无论物理 DRAM 布局或碎片情况如何，都提供统一的平坦地址空间

> **关键洞见：** 虚拟内存不只是性能特性——它是安全性与稳定性的保证。没有它，任何进程里一个失控的指针都可能破坏内核或其他进程的内存。隔离保证意味着，一个崩溃的推理进程不会破坏运行在另一进程中的控制回路，这正是 openpilot 把各组件作为独立进程运行的原因。

---

## x86-64 虚拟地址布局

```
┌─────────────────────────────────────────────────────────┐
│         x86-64 Virtual Address Space (per process)      │
├─────────────────────────────────────────────────────────┤
│  0xFFFFFFFFFFFFFFFF                                      │
│  ┌───────────────────────────────────┐                  │
│  │  Kernel Space (~128 TB)           │  (TTBR1 / CR3)   │
│  │  - Direct physical mapping        │                  │
│  │  - vmalloc region                 │                  │
│  │  - Kernel text + data             │                  │
│  │  - Module text                    │                  │
│  │  - vDSO / vsyscall                │                  │
│  └───────────────────────────────────┘                  │
│  0xFFFF800000000000  ◄── kernel base                    │
│                                                         │
│  [non-canonical hole — access triggers #GP fault]       │
│                                                         │
│  0x00007FFFFFFFFFFF  ◄── user space ceiling             │
│  ┌───────────────────────────────────┐                  │
│  │  Stack (grows downward ↓)         │                  │
│  │  ...                              │                  │
│  │  mmap region (grows downward ↓)   │  (.so, anon mmap)│
│  │  ...                              │                  │
│  │  Heap (grows upward ↑)            │  (malloc, brk)   │
│  │  BSS / Data segment               │  (global vars)   │
│  │  Text segment (code)              │  (read+exec)     │
│  └───────────────────────────────────┘                  │
│  0x0000000000000000                                      │
└─────────────────────────────────────────────────────────┘
```

```
0x0000000000000000 – 0x00007FFFFFFFFFFF   User space (~128 TB)
  [ text | data/BSS | heap → | ← mmap region | ← stack ]

0xFFFF800000000000 – 0xFFFFFFFFFFFFFFFF   Kernel space (~128 TB)
  [ direct physical mapping | vmalloc | kernel text | modules | vDSO ]
```

- **ASLR**：在进程启动时随机化栈、堆和 mmap 基地址；通过 `/proc/sys/kernel/randomize_va_space` 控制；在对延迟敏感的确定性系统上应关闭
- **规范地址**：第 48–63 位必须对第 47 位做符号扩展；非规范地址会触发 #GP 异常；57 位 VA（5 级分页）将其扩展到第 56 位

---

## ARM64 虚拟地址布局

| 寄存器 | 映射 | 备注 |
|---|---|---|
| `TTBR0_EL1` | 用户空间（VA 从 0x0 开始） | 每进程独立；每次上下文切换时重新加载 |
| `TTBR1_EL1` | 内核空间（VA 高位被置位） | 恒定；对所有进程始终存在 |

- 默认 4KB 页 × 4 级页表 = 48 位 VA
- 64KB 页 = 3 级页表
- ARMv8.2 LPA 扩展：52 位 VA；用于 Cortex-A78、A710、Jetson Orin（Cortex-A78AE）

---


<details>
<summary>English original</summary>

**Lecture 12: Virtual Memory & the Linux Memory Model**

**Overview**

The core problem this lecture addresses is: how does every process get its own private, seemingly infinite memory space, even though the hardware has a fixed amount of physical RAM? Virtual memory is the OS's most fundamental abstraction — it makes each process believe it owns the entire address space, hides the details of physical memory layout, and lets the OS swap pages to disk when RAM runs low. The mental model to carry here is that of a hotel: each guest believes they have their own room (virtual address), but the hotel maps rooms to actual beds (physical memory) on demand, and can put luggage in storage (swap) when rooms are full. For an AI hardware engineer, virtual memory directly affects inference latency (page faults can add 10ms), GPU buffer sharing (zero-copy requires shared mappings), and memory accounting (RSS vs PSS determine whether your system will OOM).

---

**Virtual Address Space**

Each process has a private **virtual address space** isolated from all other processes. The **MMU** translates virtual addresses to physical addresses per-page using the **page table tree**. No process can directly access another's physical memory without explicit kernel-mediated sharing.

Key properties:
- **Isolation**: a bad pointer in one process cannot corrupt another's memory
- **Overcommit**: total virtual memory across all processes can exceed physical RAM; pages allocated on demand
- **Abstraction**: uniform flat address space regardless of physical DRAM layout or fragmentation

> **Key Insight:** Virtual memory is not just a performance feature — it is a security and stability guarantee. Without it, a single runaway pointer in any process could corrupt the kernel or another process's memory. The isolation guarantee means that a crashing inference process cannot corrupt the control loop running in a separate process, which is why openpilot runs its components as separate processes.

---

**x86-64 Virtual Address Layout**

```
┌─────────────────────────────────────────────────────────┐
│         x86-64 Virtual Address Space (per process)      │
├─────────────────────────────────────────────────────────┤
│  0xFFFFFFFFFFFFFFFF                                      │
│  ┌───────────────────────────────────┐                  │
│  │  Kernel Space (~128 TB)           │  (TTBR1 / CR3)   │
│  │  - Direct physical mapping        │                  │
│  │  - vmalloc region                 │                  │
│  │  - Kernel text + data             │                  │
│  │  - Module text                    │                  │
│  │  - vDSO / vsyscall                │                  │
│  └───────────────────────────────────┘                  │
│  0xFFFF800000000000  ◄── kernel base                    │
│                                                         │
│  [non-canonical hole — access triggers #GP fault]       │
│                                                         │
│  0x00007FFFFFFFFFFF  ◄── user space ceiling             │
│  ┌───────────────────────────────────┐                  │
│  │  Stack (grows downward ↓)         │                  │
│  │  ...                              │                  │
│  │  mmap region (grows downward ↓)   │  (.so, anon mmap)│
│  │  ...                              │                  │
│  │  Heap (grows upward ↑)            │  (malloc, brk)   │
│  │  BSS / Data segment               │  (global vars)   │
│  │  Text segment (code)              │  (read+exec)     │
│  └───────────────────────────────────┘                  │
│  0x0000000000000000                                      │
└─────────────────────────────────────────────────────────┘
```

```
0x0000000000000000 – 0x00007FFFFFFFFFFF   User space (~128 TB)
  [ text | data/BSS | heap → | ← mmap region | ← stack ]

0xFFFF800000000000 – 0xFFFFFFFFFFFFFFFF   Kernel space (~128 TB)
  [ direct physical mapping | vmalloc | kernel text | modules | vDSO ]
```

- **ASLR**: randomizes stack, heap, and mmap base addresses at process start; controlled via `/proc/sys/kernel/randomize_va_space`; disable on latency-sensitive deterministic systems
- **Canonical addresses**: bits 48–63 must sign-extend bit 47; non-canonical address triggers #GP fault; 57-bit VA (5-level paging) extends this to bit 56

---

**ARM64 Virtual Address Layout**

| Register | Maps | Notes |
|---|---|---|
| `TTBR0_EL1` | User space (VA starting at 0x0) | Per-process; reloaded on every context switch |
| `TTBR1_EL1` | Kernel space (VA with high bits set) | Constant; always present for all processes |

- 4KB pages × 4 levels of page tables = 48-bit VA by default
- 64KB pages = 3 levels of page tables
- ARMv8.2 LPA extension: 52-bit VA; used on Cortex-A78, A710, Jetson Orin (Cortex-A78AE)

---

</details>

## 页表遍历

MMU 通过**遍历内存中的页表树**把虚拟地址翻译为物理地址。在采用 4KB 页和**四级分页**的 x86-64 上：

```
  4-Level Page Table Walk (x86-64, 48-bit VA):

  Virtual Address (48 bits):
  ┌───────┬───────┬───────┬───────┬────────────────┐
  │PGD idx│PUD idx│PMD idx│PTE idx│  Page Offset   │
  │  9b   │  9b   │  9b   │  9b   │     12b        │
  └───────┴───────┴───────┴───────┴────────────────┘

  CR3 register
  (physical addr of PGD)
       │
       ▼
  ┌──────────┐         ┌──────────┐         ┌──────────┐         ┌──────────┐
  │  PGD     │ ──────► │  PUD     │ ──────► │  PMD     │ ──────► │  PTE     │
  │(Page     │         │(Page     │         │(Page     │         │(Page     │
  │ Global   │         │ Upper    │         │ Middle   │         │ Table    │
  │ Dir)     │         │ Dir)     │         │ Dir)     │         │ Entry)   │
  └──────────┘         └──────────┘         └──────────┘         └────┬─────┘
                                                                       │
                                                                       ▼
                                                              Physical Page Base
                                                              + Page Offset
                                                              = Physical Address

  Each level lookup: physical_addr = table[index].pfn << PAGE_SHIFT
  TLB caches the final PGD→physical mapping to avoid repeated walks
```

**TLB**（Translation Lookaside Buffer，转译后备缓冲器）缓存最近的虚拟→物理地址翻译。一次 **TLB miss** 需要在 RAM 中完成完整的四级遍历——4 次内存访问。**TLB shootdown**（映射变更后刷新远端 CPU 的 TLB）需要处理器间中断，在大型 SMP 系统上增加约 10–30µs。

---

## mm_struct —— 进程内存描述符

`struct mm_struct` 描述一个进程的**完整虚拟地址空间**。**每个进程一个实例**；一个进程的所有线程共享同一个 `mm_struct`。

| 字段 | 含义 |
|---|---|
| `pgd` | 页全局目录（顶层页表）的物理地址 |
| `mmap` | 所有 `vm_area_struct` 项组成的链表 |
| `mm_rb` | 用于 O(log n) 地址查找的 VMA 红黑树 |
| `start_code`, `end_code` | Text 段边界 |
| `start_data`, `end_data` | 已初始化数据段边界 |
| `start_brk`, `brk` | 堆边界（`brk` 通过 `sbrk()`/`brk()` 系统调用向上增长） |
| `start_stack` | 初始栈指针值 |
| `total_vm` | 已映射的虚拟页总数 |
| `locked_vm` | 被 `mlock`/`mlockall` 钉住的页 |

---

## vm_area_struct (VMA)

一个 **VMA** 表示虚拟地址空间中**一块连续、映射方式统一的区域**。可以把它看作地址空间这本书中的一"章"——每一章有起点和终点、一组**权限**，以及可选的**后备文件**。

| 字段 | 含义 |
|---|---|
| `vm_start`, `vm_end` | 地址范围（页对齐；结束地址不包含） |
| `vm_flags` | `VM_READ`, `VM_WRITE`, `VM_EXEC`, `VM_SHARED`, `VM_LOCKED`, `VM_GROWSDOWN` |
| `vm_file` | 指向后备 `struct file` 的指针（匿名映射为 NULL） |
| `vm_pgoff` | 后备文件内的偏移（以页为单位） |
| `vm_ops` | VMA 操作：`fault()`, `open()`, `close()`, `mmap()` |

---

## VMA 类型

| VMA 类型 | `vm_flags` | 后备来源 | 缺页动作 | 示例 |
|---|---|---|---|---|
| Text（代码） | `VM_READ\|VM_EXEC` | 可执行 / `.so` 文件 | 从文件读取 | `main()` 代码 |
| 数据/BSS | `VM_READ\|VM_WRITE` | 可执行文件 / 零填充 | 从文件读取或清零 | 全局变量 |
| 堆 | `VM_READ\|VM_WRITE` | 匿名 | 按需零填充 | `malloc()` 区域 |
| 栈 | `VM_READ\|VM_WRITE\|VM_GROWSDOWN` | 匿名 | 零填充；缺页时扩展 | 线程栈 |
| 文件 mmap（共享） | `VM_READ\|VM_WRITE\|VM_SHARED` | 文件页 | 从文件读取 | 模型权重 mmap |
| 匿名共享 | `VM_READ\|VM_WRITE\|VM_SHARED` | `tmpfs` / swap | 零填充 | POSIX shm, VisionIPC |
| vDSO | `VM_READ\|VM_EXEC` | 内核提供 | 预映射 | `gettimeofday()` |

---


<details>
<summary>English original</summary>

**Page Table Walk**

The MMU translates a virtual address to a physical address by **walking a tree of page tables** in memory. On x86-64 with 4KB pages and **4-level paging**:

```
  4-Level Page Table Walk (x86-64, 48-bit VA):

  Virtual Address (48 bits):
  ┌───────┬───────┬───────┬───────┬────────────────┐
  │PGD idx│PUD idx│PMD idx│PTE idx│  Page Offset   │
  │  9b   │  9b   │  9b   │  9b   │     12b        │
  └───────┴───────┴───────┴───────┴────────────────┘

  CR3 register
  (physical addr of PGD)
       │
       ▼
  ┌──────────┐         ┌──────────┐         ┌──────────┐         ┌──────────┐
  │  PGD     │ ──────► │  PUD     │ ──────► │  PMD     │ ──────► │  PTE     │
  │(Page     │         │(Page     │         │(Page     │         │(Page     │
  │ Global   │         │ Upper    │         │ Middle   │         │ Table    │
  │ Dir)     │         │ Dir)     │         │ Dir)     │         │ Entry)   │
  └──────────┘         └──────────┘         └──────────┘         └────┬─────┘
                                                                       │
                                                                       ▼
                                                              Physical Page Base
                                                              + Page Offset
                                                              = Physical Address

  Each level lookup: physical_addr = table[index].pfn << PAGE_SHIFT
  TLB caches the final PGD→physical mapping to avoid repeated walks
```

The **TLB** (Translation Lookaside Buffer) caches recent virtual→physical translations. A **TLB miss** requires a full 4-level walk through RAM — 4 memory accesses. **TLB shootdowns** (flushing remote CPUs' TLBs after a mapping change) require inter-processor interrupts and add ~10–30µs on large SMP systems.

---

**mm_struct — Process Memory Descriptor**

`struct mm_struct` describes a process's **complete virtual address space**. **One instance per process**; all threads of a process share the same `mm_struct`.

| Field | Meaning |
|---|---|
| `pgd` | Physical address of Page Global Directory (top-level page table) |
| `mmap` | Linked list of all `vm_area_struct` entries |
| `mm_rb` | Red-black tree of VMAs for O(log n) address lookup |
| `start_code`, `end_code` | Text segment bounds |
| `start_data`, `end_data` | Initialized data segment bounds |
| `start_brk`, `brk` | Heap bounds (`brk` grows upward via `sbrk()`/`brk()` syscall) |
| `start_stack` | Initial stack pointer value |
| `total_vm` | Total virtual pages mapped |
| `locked_vm` | Pages pinned by `mlock`/`mlockall` |

---

**vm_area_struct (VMA)**

A **VMA** represents **one contiguous, uniformly-mapped region** of virtual address space. Think of it as a "chapter" in the address space book — each chapter has a start and end, a set of **permissions**, and an optional **backing file**.

| Field | Meaning |
|---|---|
| `vm_start`, `vm_end` | Address range (page-aligned; exclusive end) |
| `vm_flags` | `VM_READ`, `VM_WRITE`, `VM_EXEC`, `VM_SHARED`, `VM_LOCKED`, `VM_GROWSDOWN` |
| `vm_file` | Pointer to backing `struct file` (NULL for anonymous mappings) |
| `vm_pgoff` | Offset within the backing file (in pages) |
| `vm_ops` | VMA operations: `fault()`, `open()`, `close()`, `mmap()` |

---

**VMA Types**

| VMA Type | `vm_flags` | Backed By | Fault Action | Example |
|---|---|---|---|---|
| Text (code) | `VM_READ\|VM_EXEC` | Executable / `.so` file | Read from file | `main()` code |
| Data/BSS | `VM_READ\|VM_WRITE` | Executable file / zero-fill | Read from file or zero | Global variables |
| Heap | `VM_READ\|VM_WRITE` | Anonymous | Zero-fill on demand | `malloc()` regions |
| Stack | `VM_READ\|VM_WRITE\|VM_GROWSDOWN` | Anonymous | Zero-fill; expand on fault | Thread stack |
| File mmap (shared) | `VM_READ\|VM_WRITE\|VM_SHARED` | File pages | Read from file | Model weights mmap |
| Anonymous shared | `VM_READ\|VM_WRITE\|VM_SHARED` | `tmpfs` / swap | Zero-fill | POSIX shm, VisionIPC |
| vDSO | `VM_READ\|VM_EXEC` | Kernel-provided | Pre-mapped | `gettimeofday()` |

---

</details>

## mmap() — 创建映射

`mmap()` 是**创建 VMA** 的系统调用。它用于**文件映射**、**匿名内存**以及进程之间的**共享内存**。

```c
// File-backed, shared — changes visible to all processes mapping same file
// Ideal for model weights: load once, share read-only across worker processes
void *p = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset);

// Anonymous, private — zero-filled; not backed by file; used for heap/stack extensions
void *p = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

// Anonymous, shared — IPC between related processes; backed by tmpfs
// Used in VisionIPC for camera frame ring buffers shared between processes
void *p = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

// HUGE pages — 2MB pages; reduce TLB pressure for large model buffers
// A 1GB model with 4KB pages requires 262144 TLB entries; with 2MB pages only 512
void *p = mmap(NULL, length, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
```

`mmap()` 会创建 VMA 描述符，但**不**分配物理页。物理页通过**按需分页**在**首次访问**时分配。

> **关键洞察：** `mmap()` 立即返回而不分配物理页，这正是 overcommit 得以实现的原因。kernel 只给出一个虚拟承诺 ——「这里是地址空间」—— 只有当每一页首次被触碰时才落实其物理后端。这样做很高效，但对实时而言很危险：任何未映射页的首次访问都会引发缺页异常。

---

## 按需分页

页表项（PTE）初始并不存在。首次访问时，CPU 引发**缺页异常**。kernel 的 `do_page_fault()` 处理程序：

1. 找到覆盖引发异常的虚拟地址的 VMA —— 在红黑树中做 O(log n) 查找
2. 校验权限（`vm_flags` 与异常类型的对比 —— 读/写/执行）
3. 从空闲链表中分配一个物理页帧
4. 填充该页帧：零填充（匿名）、从文件读取（文件映射）、换入（此前被换出的页）
5. 在进程的页表中安装 PTE，并刷新本地 TLB 表项
6. 返回用户空间；引发异常的指令透明地重试

| Fault Type | Cause | Typical Cost |
|---|---|---|
| Minor (soft) | 页已在内存中但 PTE 缺失（COW、demand-zero） | ~1µs |
| Major (hard) | 页必须从磁盘读取（文件映射或 swap） | 1–10ms |

**Major 缺页对实时延迟保证是致命的**。`mlockall()` 能完全杜绝这类缺页。

> **常见陷阱：** 很多开发者调用 `mlockall()` 后就以为所有缺页都消除了。但 `mlockall(MCL_FUTURE)` 只能防止*未来*的页被换出 —— 从未因缺页而载入的页，在首次访问时仍会按需分页。必须在 `mlockall()` 之后预触碰所有 buffer，才能一并消除 minor 缺页。漏掉一次预触碰，就是一次迟早会发生的延迟尖峰。

---

## 写时复制（COW）

`fork()` 之后，父进程与子进程**共享所有以只读方式映射的页**。对共享页的**首次写**发生时：

1. 硬件引发缺页异常（对只读 PTE 执行写）
2. kernel 分配一个新的页帧
3. 将原页内容复制到新页帧
4. 在发起写的那个进程的页表中安装可写 PTE
5. 原页保持共享且不变

```
  Copy-on-Write after fork():

  BEFORE first write:                AFTER child writes to page X:
  Parent PTE ──► Physical Page X     Parent PTE ──► Physical Page X (unchanged)
  Child PTE  ──► Physical Page X     Child PTE  ──► Physical Page X' (private copy)
  (read-only shared)                 (writable, new allocation)
```

COW 使 `fork()` 成为 **O(1)** —— 地址空间大小无关紧要；**只有被写入的页**才会被物理复制。在 `fork()` 之后通过 COW 以只读方式共享的模型权重只消耗**一份物理副本**。

> **关键洞察：** 正因有 COW，fork 一个已在内存中载入 1GB 模型的进程并不需要额外的 1GB RAM。父进程和子进程共享模型权重所在的同一批物理页。只有某个进程实际修改的页（如栈帧和堆分配）才会被复制。正是这一机制让 openpilot 重启 `modeld` 时不会使物理 RAM 占用翻倍。

---


<details>
<summary>English original</summary>

**mmap() — Creating Mappings**

`mmap()` is the system call that **creates VMAs**. It is used for **file-backed mappings**, **anonymous memory**, and **shared memory** between processes.

```c
// File-backed, shared — changes visible to all processes mapping same file
// Ideal for model weights: load once, share read-only across worker processes
void *p = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset);

// Anonymous, private — zero-filled; not backed by file; used for heap/stack extensions
void *p = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

// Anonymous, shared — IPC between related processes; backed by tmpfs
// Used in VisionIPC for camera frame ring buffers shared between processes
void *p = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

// HUGE pages — 2MB pages; reduce TLB pressure for large model buffers
// A 1GB model with 4KB pages requires 262144 TLB entries; with 2MB pages only 512
void *p = mmap(NULL, length, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
```

`mmap()` creates the VMA descriptor but does **not** allocate physical pages. Pages are allocated **on first access** via **demand paging**.

> **Key Insight:** The fact that `mmap()` returns immediately without allocating physical pages is what enables overcommit. The kernel makes a virtual promise — "here is address space" — and only fulfills the physical backing when each page is first touched. This is efficient but dangerous for real-time: the first access to any unmapped page causes a page fault.

---

**Demand Paging**

Page table entry (PTE) is initially absent. On first access, the CPU raises a **page fault**. The kernel's `do_page_fault()` handler:

1. Finds the VMA covering the faulting virtual address — O(log n) lookup in the red-black tree
2. Validates permissions (`vm_flags` vs. fault type — read/write/execute)
3. Allocates a physical page frame from the free list
4. Fills the frame: zero-fill (anonymous), read from file (file-backed), swap-in (previously evicted)
5. Installs the PTE in the process's page table and flushes the local TLB entry
6. Returns to user space; the faulting instruction retries transparently

| Fault Type | Cause | Typical Cost |
|---|---|---|
| Minor (soft) | Page in memory but PTE absent (COW, demand-zero) | ~1µs |
| Major (hard) | Page must be read from disk (file-backed or swap) | 1–10ms |

**Major faults are fatal** to real-time latency guarantees. `mlockall()` prevents them entirely.

> **Common Pitfall:** Many developers call `mlockall()` and assume all page faults are eliminated. But `mlockall(MCL_FUTURE)` only prevents *future* pages from being evicted — pages that have never been faulted in are still demand-paged on first access. You must pre-touch all buffers after `mlockall()` to eliminate minor faults as well. A missed pre-touch is a latency spike waiting to happen.

---

**Copy-on-Write (COW)**

After `fork()`, parent and child **share all pages mapped read-only**. On **first write** to a shared page:

1. Hardware page fault triggered (write to read-only PTE)
2. Kernel allocates a new page frame
3. Copies original page content to new frame
4. Installs a writable PTE in the writing process's page table
5. Original page remains shared and unchanged

```
  Copy-on-Write after fork():

  BEFORE first write:                AFTER child writes to page X:
  Parent PTE ──► Physical Page X     Parent PTE ──► Physical Page X (unchanged)
  Child PTE  ──► Physical Page X     Child PTE  ──► Physical Page X' (private copy)
  (read-only shared)                 (writable, new allocation)
```

COW makes `fork()` **O(1)** — the address space size does not matter; **only written pages** are physically duplicated. Model weights shared read-only via COW after `fork()` consume **only one physical copy**.

> **Key Insight:** COW is why forking a process with a 1GB model loaded in memory does not require 1GB of additional RAM. Both parent and child share the same physical pages for the model weights. Only pages that one process actually modifies (like stack frames and heap allocations) get duplicated. This is the mechanism that lets openpilot restart `modeld` without doubling physical RAM usage.

---

</details>

## mlock() / mlockall() —— 将页面固定在 RAM 中

```c
mlockall(MCL_CURRENT | MCL_FUTURE);   // pin all current + future mappings
// MCL_CURRENT: locks all pages that are currently mapped and present
// MCL_FUTURE: any future mmap() or heap growth is automatically locked on fault-in

mlock(addr, length);                   // pin specific address range only
munlockall();                          // release all locks; allows eviction again
```

- `MCL_CURRENT`：锁定地址空间中当前已映射的所有页面
- `MCL_FUTURE`：后续任何 `mmap()` 或堆增长都会被自动锁定
- 需要将 `CAP_IPC_LOCK` 或 `RLIMIT_MEMLOCK` 提升到足够大

执行 `mlockall()` 之后，预先触碰所有页面，以一并消除 minor fault：

```c
// Force demand-zero pages to be physically allocated immediately
// Without this, the first access to each page still causes a minor page fault (~1µs)
memset(buffer, 0, buffer_size);
// After memset, buffer pages are faulted in, PTEs installed, and mlocked —
// subsequent accesses have zero fault overhead
```

---

## 检查虚拟内存

理解进程虚拟地址空间中实际驻留的内容，对于调试内存问题、OOM 故障以及意外缺页至关重要。

```bash
cat /proc/<pid>/maps           # all VMAs: address range, permissions, backing file
cat /proc/<pid>/smaps          # per-VMA detail: RSS, PSS, dirty, swap, AnonHugePages
cat /proc/<pid>/smaps_rollup   # per-process totals from smaps
pmap -x <pid>                  # formatted smaps output
```

| 指标 | 定义 |
|---|---|
| RSS | Resident Set Size：已映射的物理页面总量（会重复计算共享页面） |
| PSS | Proportional Set Size：类似 RSS，但共享页面按比例分摊；按进程统计的正确指标 |
| Swap | 当前被换出到 swap 的页面 |
| AnonHugePages | 由 2MB 透明大页支撑的匿名页面 |

在多进程系统中，共享库否则会被 **RSS 重复计算**，此时 **PSS 才是按进程统计内存的正确指标**。

```
  RSS vs PSS for shared libraries:

  libcuda.so (100MB physical):
  ├── Process A: RSS counts 100MB
  ├── Process B: RSS counts 100MB
  └── Total RSS: 200MB ← WRONG (only 100MB physical)

  PSS: split proportionally (3 processes sharing):
  ├── Process A: PSS counts 33MB
  ├── Process B: PSS counts 33MB
  ├── Process C: PSS counts 33MB
  └── Total PSS: 100MB ← CORRECT
```

---

## 小结

| VMA 类型 | 标志位 | 支撑来源 | 缺页动作 | 示例 |
|---|---|---|---|---|
| Text segment | `R-X` | ELF / `.so` 文件 | 从文件读取 | 代码页 |
| Data/BSS | `RW-` | ELF 文件 / 零 | 读取文件或零填充 | 全局变量 |
| Heap | `RW-` | 匿名 | 按需零填充 | `malloc` |
| Stack | `RW-`（向下增长） | 匿名 | 零填充、自动扩展 | 线程栈 |
| File mmap（共享） | `RWS` | 文件 | 从文件读取 | 模型权重文件 |
| 匿名共享 | `RWS` | `tmpfs` | 零填充 | VisionIPC ring buffer |
| vDSO | `R-X` | 内核提供 | 启动时预先映射 | `clock_gettime` |


<details>
<summary>English original</summary>

**mlock() / mlockall() — Pinning Pages in RAM**

```c
mlockall(MCL_CURRENT | MCL_FUTURE);   // pin all current + future mappings
// MCL_CURRENT: locks all pages that are currently mapped and present
// MCL_FUTURE: any future mmap() or heap growth is automatically locked on fault-in

mlock(addr, length);                   // pin specific address range only
munlockall();                          // release all locks; allows eviction again
```

- `MCL_CURRENT`: locks all pages currently mapped in the address space
- `MCL_FUTURE`: any future `mmap()` or heap growth is automatically locked
- Requires `CAP_IPC_LOCK` or `RLIMIT_MEMLOCK` raised sufficiently

After `mlockall()`, pre-touch all pages to eliminate minor faults as well:

```c
// Force demand-zero pages to be physically allocated immediately
// Without this, the first access to each page still causes a minor page fault (~1µs)
memset(buffer, 0, buffer_size);
// After memset, buffer pages are faulted in, PTEs installed, and mlocked —
// subsequent accesses have zero fault overhead
```

---

**Inspecting Virtual Memory**

Understanding what is actually in your process's virtual address space is essential for debugging memory issues, OOM failures, and unexpected page faults.

```bash
cat /proc/<pid>/maps           # all VMAs: address range, permissions, backing file
cat /proc/<pid>/smaps          # per-VMA detail: RSS, PSS, dirty, swap, AnonHugePages
cat /proc/<pid>/smaps_rollup   # per-process totals from smaps
pmap -x <pid>                  # formatted smaps output
```

| Metric | Definition |
|---|---|
| RSS | Resident Set Size: total physical pages mapped (double-counts shared pages) |
| PSS | Proportional Set Size: RSS but shared pages divided proportionally; correct per-process metric |
| Swap | Pages evicted to swap currently |
| AnonHugePages | Anonymous pages backed by 2MB transparent huge pages |

**PSS is the correct metric** for per-process memory accounting in multi-process systems where shared libraries would otherwise be **double-counted by RSS**.

```
  RSS vs PSS for shared libraries:

  libcuda.so (100MB physical):
  ├── Process A: RSS counts 100MB
  ├── Process B: RSS counts 100MB
  └── Total RSS: 200MB ← WRONG (only 100MB physical)

  PSS: split proportionally (3 processes sharing):
  ├── Process A: PSS counts 33MB
  ├── Process B: PSS counts 33MB
  ├── Process C: PSS counts 33MB
  └── Total PSS: 100MB ← CORRECT
```

---

**Summary**

| VMA Type | Flags | Backed By | Fault Action | Example |
|---|---|---|---|---|
| Text segment | `R-X` | ELF / `.so` file | Read from file | Code pages |
| Data/BSS | `RW-` | ELF file / zero | File read or zero-fill | Global vars |
| Heap | `RW-` | Anonymous | Zero-fill on demand | `malloc` |
| Stack | `RW-` (growsdown) | Anonymous | Zero-fill, auto-extend | Thread stack |
| File mmap (shared) | `RWS` | File | Read from file | Model weight file |
| Anonymous shared | `RWS` | `tmpfs` | Zero-fill | VisionIPC ring buffer |
| vDSO | `R-X` | Kernel-provided | Pre-mapped at boot | `clock_gettime` |

</details>

### 概念回顾

- **为什么 `mmap()` 即使映射 1GB 也会立即返回？** 因为 `mmap()` 只创建 VMA 描述符——它不做任何物理内存分配。物理页在首次访问时通过按需分页惰性分配。这正是 overcommit 得以实现、以及 `fork()` 如此之快的原因。
- **minor page fault 与 major page fault 有什么区别？** minor fault 意味着物理页已在 RAM 中，但 PTE 缺失（例如 `fork()` COW 或 demand-zero 之后）。major fault 意味着该页必须从磁盘读取（file-backed 或 swap）。minor fault 开销约 1µs；major fault 开销 1–10ms，对实时性保证是致命的。
- **为什么 `mlockall()` 需要随后 memset 才能彻底消除缺页？** `mlockall(MCL_FUTURE)` 会在页被换入后将其 pin 住并防止换出。但从未被访问过的页尚未进入 RAM——它们首次触碰时仍走按需分页，引发 minor fault。Memset 强制所有页同时被换入并锁定。
- **为什么正确的指标是 PSS 而不是 RSS？** RSS 会重复计算共享页。像 `libcuda.so` 这样的共享库可能在 10 个不同进程的 RSS 数值中各出现 100MB，合计 1GB——但实际只用到 100MB 物理内存。PSS 按比例分摊共享页，给出准确的合计值。
- **COW 如何让 `fork()` 不论地址空间多大都是 O(1)？** `fork()` 之后，所有页都被标记为只读并共享。不发生任何复制。子进程立即开始执行。只有当父进程或子进程*写入*某个共享页时，才会制作私有副本——而且只针对那一页。对于一个拥有 1GB 从不写入的模型权重的进程，永远不会发生任何复制。
- **页表遍历做什么，何时会被避免？** TLB 缓存近期的 VA→PA 转换。如果 TLB 中含该映射，MMU 直接使用。TLB 未命中时，MMU 遍历 RAM 中的 4 级页表（4 次内存读取）。TLB shootdown（发生在 munmap、fork 或进程退出期间）需要向远端 CPU 发送 IPI，会给无关线程增加 10–30µs。

---

## AI 硬件关联

- `mmap(MAP_SHARED)` 作用于 DMA-BUF 文件描述符，把 GPU 分配的 DRAM 映射进 CPU 进程的虚拟地址空间，实现 V4L2 摄像头采集与 CUDA 推理之间的零拷贝缓冲区共享；没有任何数据复制跨越 PCIe 总线
- 在 Jetson Orin 上的实时推理守护进程中，启动时会调用 `mlockall(MCL_CURRENT|MCL_FUTURE)`，以消除推理循环中的 major page fault；前向传播期间一次换页缺页就可能增加 10ms 延迟
- COW `fork()` 让 openpilot 可以创建子进程，共享父进程在物理内存中的模型权重页，而不必使 RAM 占用翻倍；对于 512MB 的权重张量，这避免了每次进程重启时 512MB 的物理复制
- 由 `memfd_create` 支撑的共享匿名 `mmap`（`MAP_SHARED|MAP_ANONYMOUS`）为 VisionIPC 提供进程间摄像头帧环形缓冲区；`camerad` 把帧直接写入共享区域，`modeld` 读取时无需任何复制
- 在多进程边缘 AI 系统中，`/proc/<pid>/smaps` PSS 是按进程计账的正确内存指标；RSS 会把每个共享库（libcuda、libtorch）在 `camerad`、`modeld` 和 `controlsd` 中重复计数多次
- CUDA Unified Virtual Memory（UVM）使用与 Linux VM 相同的按需分页机制：GPU 缺页会通过 CUDA 驱动的 fault handler 触发 CPU→GPU 或 GPU→CPU 的页迁移，把 GPU 与 CPU 的虚拟地址空间透明地映射到同一块物理内存


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why does `mmap()` return immediately even for a 1GB mapping?** Because `mmap()` only creates the VMA descriptor — it makes no physical memory allocation. Physical pages are allocated lazily on first access via demand paging. This is what makes overcommit possible and `fork()` fast.
- **What is the difference between a minor and major page fault?** A minor fault means the physical page is already in RAM but the PTE is absent (e.g., after `fork()` COW or demand-zero). A major fault means the page must be read from disk (file-backed or swap). Minor faults cost ~1µs; major faults cost 1–10ms and are fatal to real-time guarantees.
- **Why does `mlockall()` require a subsequent memset to fully eliminate faults?** `mlockall(MCL_FUTURE)` pins pages once they are faulted in and prevents eviction. But pages that have never been accessed are not yet in RAM — they are still demand-paged on first touch, causing minor faults. Memset forces all pages to be faulted in and locked simultaneously.
- **Why is PSS the correct metric and not RSS?** RSS double-counts shared pages. A shared library like `libcuda.so` might appear as 100MB in 10 different process's RSS figures, giving a total of 1GB — but only 100MB of physical memory is actually used. PSS divides shared pages proportionally and gives an accurate sum.
- **How does COW enable `fork()` to be O(1) regardless of address space size?** After `fork()`, all pages are marked read-only and shared. No copying occurs. The child starts executing immediately. Only when either the parent or child *writes* to a shared page does a private copy get made — and only for that one page. For a process with 1GB of model weights that are never written, zero copying ever occurs.
- **What does the page table walk do, and when is it avoided?** The TLB caches recent VA→PA translations. If the TLB contains the mapping, the MMU uses it directly. On a TLB miss, the MMU walks the 4-level page table in RAM (4 memory reads). TLB shootdowns (during munmap, fork, or process exit) require IPIs to remote CPUs and can add 10–30µs to unrelated threads.

---

**AI Hardware Connection**

- `mmap(MAP_SHARED)` on a DMA-BUF file descriptor maps GPU-allocated DRAM into a CPU process's virtual address space for zero-copy buffer sharing between V4L2 camera capture and CUDA inference; no data copy crosses the PCIe bus
- `mlockall(MCL_CURRENT|MCL_FUTURE)` is called at startup in real-time inference daemons on Jetson Orin to eliminate major page faults from the inference loop; a single fault to swap during a forward pass can add 10ms of latency
- COW `fork()` allows openpilot to spawn child processes sharing the parent's model weight pages in physical memory without doubling RAM; on a 512MB weight tensor, this avoids a 512MB physical copy on every process restart
- Shared anonymous `mmap` (`MAP_SHARED|MAP_ANONYMOUS`) backed by `memfd_create` provides the inter-process camera frame ring buffer in VisionIPC; `camerad` writes frames directly into the shared region and `modeld` reads without any copy
- `/proc/<pid>/smaps` PSS is the correct memory metric for per-process accounting in multi-process edge AI systems; RSS would count each shared library (libcuda, libtorch) multiple times across `camerad`, `modeld`, and `controlsd`
- CUDA Unified Virtual Memory (UVM) uses the same demand-paging mechanism as Linux VM: GPU page faults trigger CPU→GPU or GPU→CPU page migrations via the CUDA driver's fault handler, transparently mapping GPU and CPU virtual address spaces to the same physical memory

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-12.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-12.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
