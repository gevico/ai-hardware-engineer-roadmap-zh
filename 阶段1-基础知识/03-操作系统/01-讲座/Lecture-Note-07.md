---
title: 讲义 07（L15、L16）：DMA、IOMMU 与 GPU 内存；NUMA 与 HPC（高性能计算）优化
description: 讲义 07（L15、L16）：DMA、IOMMU 与 GPU 内存；NUMA 与 HPC（高性能计算）优化
---

# 讲义 07（L15、L16）：DMA、IOMMU 与 GPU 内存；NUMA 与 HPC（高性能计算）优化

**合并：** L15 讲（DMA、IOMMU 与 GPU 内存管理）与 L16 讲（NUMA 拓扑与 HPC 内存优化）。

---

## 本讲义的组织方式

1. **第 1 部分 — DMA 与 IOMMU：** Direct Memory Access；缓存一致性（coherent 与 streaming）；DMA-BUF；IOMMU 与安全；VFIO；GPU 内存与零拷贝。
2. **第 2 部分 — NUMA：** 非一致性内存访问；first-touch 与布局；内存策略（bind、preferred、interleave）；numactl 与 libnuma；AutoNUMA；多 GPU 与 CPU–GPU 亲和性。

---

# 第 1 部分：DMA、IOMMU 与 GPU 内存管理

**背景：** 设备与 RAM 之间搬运数据**无需 CPU 拷贝**。CPU 不能看到**过期数据**（缓存一致性）。**IOMMU** 转换设备地址并限制访问；**DMA-BUF** 让同一个 buffer 在 CPU、GPU、摄像头、显示之间共享。

---

## DMA（Direct Memory Access）

- 设备（NIC、NVMe、GPU、camera ISP）**自主**与系统 RAM 之间传输数据。CPU 配置 **descriptor**（src、dst、length）；设备执行传输；通过 IRQ 或轮询通知完成。
- **无 DMA：** 设备 → CPU 拷贝 → RAM。**有 DMA：** 设备 → DMA engine → RAM；提交 descriptor 后 CPU 即空闲。

---

## 缓存一致性：coherent 与 streaming

- **Coherent DMA：** `dma_alloc_coherent()` — 非缓存或硬件一致；CPU 与设备始终看到相同数据；无需显式同步。用于小的控制/descriptor 区域。
- **Streaming DMA：** CPU 使用带缓存的内存；驱动显式**同步**：
  - **DMA_TO_DEVICE：** CPU 写入数据 → `dma_map_single()` 刷缓存 → 设备读取。
  - **DMA_FROM_DEVICE：** 设备将要写入 → 传输完成后，`dma_unmap_single()` 使缓存失效 → CPU 读取。在 map 与 unmap 之间，buffer 归设备“所有”——CPU 不得触碰。
- **dma_map_sg** / **dma_unmap_sg** 用于 scatter-gather（碎片化 buffer）。方向决定使用哪种缓存操作；方向错误 = 静默数据损坏。

---

## IOMMU（Input-Output MMU）

- 位于设备与内存总线之间。用按设备/按组的页表把 **IOVA**（I/O Virtual Address）转换为物理地址。
- **无 IOMMU：** 设备可 DMA 到任意物理地址（安全风险）。**有 IOMMU：** 仅允许已映射的 IOVA；访问未映射地址 → fault。
- **实现：** Intel VT-d、AMD-Vi、ARM SMMU（Jetson Orin）。**IOMMU 组：** 位于同一转换单元之后的设备；做 VFIO 直通时，整个组一起分配。
- 驱动通常使用 **dma_map_***，其自动走 IOMMU；框架代码中才用底层 `iommu_map`/`iommu_unmap`。

---

## DMA-BUF 与零拷贝流水线

- **DMA-BUF：** 内核抽象，让一个 DMA buffer 在多个子系统（CPU、GPU、camera ISP、display）间共享。**Exporter** 分配并导出；**importer** 附着并为其设备映射。
- **生命周期：** Exporter 创建 buffer，得到 fd；fd 被传递（例如 Unix socket）；importer `dma_buf_get(fd)`、`dma_buf_attach`、`dma_buf_map_attachment` → 得到该设备的带 IOVA 的 sg_table。
- **零拷贝：** V4L2 摄像头 → DMA-BUF fd → CUDA importer → 推理使用同一批物理页 → 显示。无 CPU 拷贝；通过 DMA fence 同步（`dma_fence_wait`）。V4L2 支持 `V4L2_MEMORY_DMABUF`；用户态在 buffer 中传入 fd。

---

## GPU 内存与统一内存

- 独立 GPU：专用 VRAM；驱动管理分配与 CPU↔GPU 拷贝。**统一内存（如 Jetson）：** CPU 与 GPU 共享物理 RAM；单一地址空间；共享 buffer 无需显式拷贝，但带宽与延迟取决于布局。
- **Resizable BAR（SAM）：** GPU BAR1 可覆盖全部 VRAM；CPU 能寻址所有 GPU 内存（对 GPUDirect Storage、零拷贝很重要）。

---

# 第 2 部分：NUMA 拓扑与 HPC 内存优化

**背景：** 在多 socket 机器上，挂在 socket 0 上的内存对 socket 0 上的 CPU 是**“本地”**，对 socket 1 是**“远端”**。远端访问**延迟更高、带宽更低**；数据与线程的布局很关键。

---

## NUMA 架构

- 每个 socket（NUMA 节点）有**本地 DRAM**（延迟更低、带宽满额）。访问另一节点的 DRAM 需经过**互连**（QPI/UPI、Infinity Fabric）——**延迟约 2×、带宽约一半**。
- **发现：** `numactl --hardware`、`lstopo`、`numastat`、`/sys/devices/system/node/nodeN/distance`。距离矩阵：本地 = 10；远端常为 20–40。

---

## First-Touch 与布局

- **默认（first-touch）：** 页分配在首次 **fault** 它的 CPU 所在节点上。若 node 0 上的主线程初始化了一个之后只被 node 1 上 worker 使用的 buffer，则每次访问都是远端——静默变慢。
- **修复：** 在将要使用该数据的同一节点上分配（并 fault）：把线程绑到该节点、设置 mempolicy，然后 malloc + memset；或用 `numa_alloc_onnode()` / `mbind(..., MPOL_MF_MOVE)` 迁移页。

---


<details>
<summary>English original</summary>

**Lecture Note 07 (L15, L16): DMA, IOMMU & GPU Memory; NUMA & HPC Optimization**

**Combines:** Lecture L15 (DMA, IOMMU & GPU Memory Management) and Lecture L16 (NUMA Topology & HPC Memory Optimization).

---

**How This Note Is Organized**

1. **Part 1 — DMA & IOMMU:** Direct Memory Access; cache coherency (coherent vs streaming); DMA-BUF; IOMMU and security; VFIO; GPU memory and zero-copy.
2. **Part 2 — NUMA:** Non-uniform memory access; first-touch and placement; memory policies (bind, preferred, interleave); numactl and libnuma; AutoNUMA; multi-GPU and CPU–GPU affinity.

---

**Part 1: DMA, IOMMU & GPU Memory Management**

**Context:** Devices move data to/from RAM **without CPU copies**. The CPU must not see **stale data** (cache coherency). The **IOMMU** translates device addresses and restricts access; **DMA-BUF** shares one buffer across CPU, GPU, camera, display.

---

**DMA (Direct Memory Access)**

- Device (NIC, NVMe, GPU, camera ISP) transfers data to/from system RAM **autonomously**. CPU programs **descriptor** (src, dst, length); device runs transfer; completion via IRQ or poll.
- **Without DMA:** Device → CPU copy → RAM. **With DMA:** Device → DMA engine → RAM; CPU is free after submitting descriptor.

---

**Cache Coherency: Coherent vs Streaming**

- **Coherent DMA:** `dma_alloc_coherent()` — uncached or hardware-coherent; CPU and device always see same data; no explicit sync. Use for small control/descriptor regions.
- **Streaming DMA:** CPU uses cached memory; driver **synchronizes** explicitly:
  - **DMA_TO_DEVICE:** CPU wrote data → `dma_map_single()` flushes cache → device reads.
  - **DMA_FROM_DEVICE:** Device will write → after transfer, `dma_unmap_single()` invalidates cache → CPU reads. Between map and unmap the buffer is "owned" by the device — CPU must not touch it.
- **dma_map_sg** / **dma_unmap_sg** for scatter-gather (fragmented buffers). Direction controls which cache op is used; wrong direction = silent corruption.

---

**IOMMU (Input-Output MMU)**

- Sits between devices and memory bus. Translates **IOVA** (I/O Virtual Address) to physical address using per-device/group page tables.
- **Without IOMMU:** Device can DMA to any physical address (security risk). **With IOMMU:** Only mapped IOVAs are allowed; unmapped access → fault.
- **Implementations:** Intel VT-d, AMD-Vi, ARM SMMU (Jetson Orin). **IOMMU groups:** Devices behind same translation unit; for VFIO passthrough, whole group is assigned together.
- Drivers usually use **dma_map_*** which uses IOMMU automatically; low-level `iommu_map`/`iommu_unmap` in framework code.

---

**DMA-BUF & Zero-Copy Pipeline**

- **DMA-BUF:** Kernel abstraction to share one DMA buffer across subsystems (CPU, GPU, camera ISP, display). **Exporter** allocates and exports; **importer** attaches and maps for its device.
- **Lifecycle:** Exporter creates buffer, gets fd; fd passed (e.g. Unix socket); importer `dma_buf_get(fd)`, `dma_buf_attach`, `dma_buf_map_attachment` → sg_table with IOVAs for that device.
- **Zero-copy:** V4L2 camera → DMA-BUF fd → CUDA importer → same physical pages for inference → display. No CPU copy; sync via DMA fence (`dma_fence_wait`). V4L2 supports `V4L2_MEMORY_DMABUF`; userspace passes fd in buffer.

---

**GPU Memory & Unified Memory**

- Discrete GPU: dedicated VRAM; driver manages allocations and CPU↔GPU copies. **Unified memory (e.g. Jetson):** CPU and GPU share physical RAM; one address space; no explicit copy for shared buffers, but bandwidth and latency depend on placement.
- **Resizable BAR (SAM):** GPU BAR1 can cover full VRAM; CPU can address all GPU memory (important for GPUDirect Storage, zero-copy).

---

**Part 2: NUMA Topology & HPC Memory Optimization**

**Context:** On multi-socket machines, memory attached to socket 0 is **"local"** to CPUs on socket 0 and **"remote"** to socket 1. Remote access has **higher latency and lower bandwidth**; placement of data and threads matters.

---

**NUMA Architecture**

- Each socket (NUMA node) has **local DRAM** (lower latency, full bandwidth). Access to another node's DRAM goes over **interconnect** (QPI/UPI, Infinity Fabric) — **~2× latency, ~half bandwidth**.
- **Discovery:** `numactl --hardware`, `lstopo`, `numastat`, `/sys/devices/system/node/nodeN/distance`. Distance matrix: local = 10; remote often 20–40.

---

**First-Touch and Placement**

- **Default (first-touch):** Page is allocated on the node of the CPU that first **faults** it. If main thread on node 0 initializes a buffer later used only by workers on node 1, every access is remote — silent slowdown.
- **Fix:** Allocate (and fault) on the same node that will use the data: pin thread to node, set mempolicy, then malloc + memset; or use `numa_alloc_onnode()` / `mbind(..., MPOL_MF_MOVE)` to move pages.

---

</details>

## 内存策略与 numactl

- **MPOL_DEFAULT：** 首次触碰。**MPOL_BIND：** 仅限列出的 node；写满即失败。**MPOL_PREFERRED：** 优先某一个 node；失败则回退。**MPOL_INTERLEAVE：** 跨 node 轮询（带宽受限场景）。
- **set_mempolicy**（进程）；**mbind**（VMA；MPOL_MF_MOVE 会迁移已有页）。
- **numactl：** `--cpunodebind=0 --membind=0 ./app`（把 CPU 和内存绑定到 node 0）；`--interleave=all`（交错）；`--preferred=1`（优先 node 1）。

---

## libnuma 与 AutoNUMA

- **libnuma：** `numa_node_of_cpu(sched_getcpu())`, `numa_alloc_onnode()`, `numa_bind()`, `numa_set_membind()`, `numa_set_interleave_mask()`.
- **AutoNUMA：** 内核把「热」页迁移到访问它的 CPU 所在 node。有开销（扫描、TLB shootdown、迁移）；可能造成尾延迟尖峰。**建议：** 在 RT 和对延迟敏感的推理场景中禁用（`numa_balancing=0`）；改用显式的 numactl/mbind。

---

## 多 GPU 与 CPU–GPU 亲和性

- GPU 挂在**某一个 socket 的 PCIe root** 上。来自另一个 socket 的 CPU↔GPU 流量会**跨越互连**。用 `nvidia-smi topo -m` 查看拓扑；**把进程和内存绑定到所用 GPU 所属的 node**。

---

## 汇总表

**DMA：** 一致性（coherent）= 不缓存/一致，无需同步。流式（streaming）= map（flush/invalidate）→ 设备使用 → unmap（invalidate/flush）；map 与 unmap 之间 CPU 不得访问。

**IOMMU：** IOVA→PA；按设备/组划分；限制 DMA；VFIO 将其暴露给用户态以做 passthrough。

**NUMA：** 本地与远程的延迟/带宽差异；first-touch；bind/preferred/interleave；numactl；RT 下禁用 AutoNUMA。

---

## 与 AI 硬件的联系

- **DMA-BUF + V4L2：** 摄像头到 GPU 零拷贝；通过 IPC 传 fd。**流式 DMA** 的方向必须正确（TO_DEVICE / FROM_DEVICE），否则会数据损坏。
- **IOMMU：** 为 GPU/VFIO 提供安全与隔离；安全 passthrough 的必要条件。
- **NUMA：** 把推理和权重固定到与 GPU 相同的 node；first-touch 落在错误的 node 会让有效带宽减半。用 **numastat -p** 验证；在推理服务器上禁用 AutoNUMA。
- **多 GPU：** 把进程和内存分配放在目标 GPU 所属的 node 上；用拓扑工具避免跨 socket PCIe。

---

*综合了 Lecture L15、L16（DMA、IOMMU、GPU 内存；NUMA、高性能计算（HPC）优化）。*


<details>
<summary>English original</summary>

**Memory Policies & numactl**

- **MPOL_DEFAULT:** First-touch. **MPOL_BIND:** Only listed nodes; fail if full. **MPOL_PREFERRED:** Prefer one node; fallback. **MPOL_INTERLEAVE:** Round-robin across nodes (bandwidth-bound).
- **set_mempolicy** (process); **mbind** (VMA; MPOL_MF_MOVE migrates existing pages).
- **numactl:** `--cpunodebind=0 --membind=0 ./app` (bind CPU and memory to node 0); `--interleave=all` (interleave); `--preferred=1` (prefer node 1).

---

**libnuma & AutoNUMA**

- **libnuma:** `numa_node_of_cpu(sched_getcpu())`, `numa_alloc_onnode()`, `numa_bind()`, `numa_set_membind()`, `numa_set_interleave_mask()`.
- **AutoNUMA:** Kernel migrates "hot" pages to the node of the accessing CPU. Overhead (scan, TLB shootdown, migration); can cause tail latency spikes. **Recommendation:** Disable (`numa_balancing=0`) on RT and latency-sensitive inference; use explicit numactl/mbind.

---

**Multi-GPU & CPU–GPU Affinity**

- GPUs are attached to **one socket's PCIe root**. CPU↔GPU traffic from the other socket **crosses interconnect**. Use `nvidia-smi topo -m` to see topology; **bind process and memory to the node that owns the GPU(s) used**.

---

**Summary Tables**

**DMA:** Coherent = uncached/coherent, no sync. Streaming = map (flush/invalidate) → device uses → unmap (invalidate/flush); CPU must not touch between map/unmap.

**IOMMU:** IOVA→PA; per-device/group; restricts DMA; VFIO exposes to userspace for passthrough.

**NUMA:** Local vs remote latency/bandwidth; first-touch; bind/preferred/interleave; numactl; disable AutoNUMA for RT.

---

**AI Hardware Connection**

- **DMA-BUF + V4L2:** Zero-copy camera → GPU; fd over IPC. **Streaming DMA** direction must be correct (TO_DEVICE / FROM_DEVICE) to avoid corruption.
- **IOMMU:** Security and isolation for GPU/VFIO; required for safe passthrough.
- **NUMA:** Pin inference and weights to same node as GPU; first-touch on wrong node halves effective bandwidth. **numastat -p** to verify; disable AutoNUMA on inference servers.
- **Multi-GPU:** Place processes and allocations on the node that owns the target GPU; use topology tools to avoid cross-socket PCIe.

---

*Combines Lectures L15, L16 (DMA, IOMMU, GPU Memory; NUMA, HPC Optimization).*

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-Note-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-Note-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
