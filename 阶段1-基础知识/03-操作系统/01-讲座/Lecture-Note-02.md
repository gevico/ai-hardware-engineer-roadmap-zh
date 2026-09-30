---
title: 讲义 02（L3、L4）：中断、异常与下半部；系统调用、vDSO 与 eBPF
description: 讲义 02（L3、L4）：中断、异常与下半部；系统调用、vDSO 与 eBPF
---

# 讲义 02（L3、L4）：中断、异常与下半部；系统调用、vDSO 与 eBPF

**合并：** 讲义 L3（中断、异常与下半部）与讲义 L4（系统调用、vDSO 与 eBPF）。

---

## 本讲义的组织方式

1. **第 1 部分（L3）—— 中断与下半部：** 中断与异常；GIC/APIC；MSI/MSI-X；上半部与下半部；softirq、tasklet、workqueue；中断流程与延迟。
2. **第 2 部分（L4）—— 系统调用与 vDSO：** 系统调用路径（x86/ARM64）；开销与缓解措施；用于计时的 vDSO；关键系统调用（mmap、ioctl）；用于可观测性的 eBPF。

---

# 第 1 部分（L3）：中断、异常与下半部

**上下文：** 硬件异步通知 CPU（摄像头帧、GPU 完成、数据包）。内核使用**上半部**（快速、确认硬件、调度工作）与**下半部**（在不持有 IRQ 关闭的情况下干活）。误用会导致丢帧与延迟尖峰。

---

## 中断与异常

| 类型 | 来源 | 同步？ | 示例 |
|------|--------|-------|--------|
| 硬件中断 | 设备 IRQ | 否 | NIC、GPU 完成、摄像头帧 |
| 软件中断 | INT/SVC | 是 | 系统调用、断点 |
| 异常（fault） | CPU 错误 | 是 | 缺页、除零 |
| 异常（abort） | 不可恢复 | 是 | 机器检查 |

**Fault：** 修复后重新执行指令。**Trap：** 处理程序之后前进。**Abort：** 不返回。

---

## 中断控制器

**ARM GIC v3：** SGI（0–15，IPI）、PPI（16–31，每 CPU）、SPI（32–1019，设备）。优先级 0–255；PMR 屏蔽。**x86 APIC：** Local APIC + I/O APIC；IDT；TPR。**MSI/MSI-X（PCIe）：** 消息信号中断；MSI-X 提供**每队列向量与 CPU 亲和性**——NVMe、GPU、NIC 为实现可扩展性而使用。

---

## 上半部与下半部

**上半部（ISR）：** 在本地 CPU 关闭 IRQ 的情况下运行；必须极短（确认硬件、保存指针、调度下半部）；不能睡眠。**下半部：** softirq、tasklet 或 workqueue；可做真正的工作；部分可睡眠（workqueue）。上半部过长会阻塞其他 IRQ 并增加延迟。

---

## 下半部机制

**Softirq：** 静态分配；在 ksoftirqd 中或 hardirq 之后运行；不能睡眠。**Tasklet：** 构建于 softirq 之上；每 CPU 一次一个。**Workqueue：** 进程上下文；可睡眠；用于较重的工作（如块 I/O）。线程化 IRQ 在处理程序里跑 kthread —— 在 PREEMPT_RT 下可抢占。

---

# 第 2 部分（L4）：系统调用、vDSO 与 eBPF

**上下文：** 用户代码只能通过**系统调用**（收费站）抵达内核。代价：模式切换、Spectre/Meltdown 缓解措施、TLB 影响（**约 100–400 ns 往返**）。**vDSO** 为计时避开系统调用；**eBPF** 在不改代码的情况下观测内核。

---

## 系统调用路径

x86-64：SYSCALL → entry_SYSCALL_64 → sys_call_table[rax] → SYSRET。ARM64：SVC → el0_svc → sys_call_table。用 `strace -c` / `strace -T -e mmap,ioctl` 来计数并计时系统调用。

---

## 系统调用开销

**模式切换约 50–150 ns**；缓解措施（KPTI、IBRS、retpoline）在 x86 上再加约 50–200 ns；ARM64 更轻。200 fps × 4 次 ioctl = 1600/s × 300 ns ≈ 0.5 ms/s；**批处理与零拷贝**（mmap、io_uring）减少穿越次数。

---

## vDSO

内核映射一个**只读页**，含时间（及少数其他）辅助函数。`clock_gettime(CLOCK_MONOTONIC)`、`gettimeofday()` 可在**用户空间**解析（约 10–20 ns），无需 SYSCALL。用 CLOCK_MONOTONIC 做计时与传感器融合；**CLOCK_REALTIME 会跳变**（NTP）。

---

## 关键系统调用：mmap、ioctl

**mmap：** MAP_SHARED 用于零拷贝 shm；MAP_HUGETLB 用于大缓冲区并减少 TLB；CUDA pinned 与 Jetson 统一内存在设备节点上使用 mmap。**ioctl：** 设备控制（V4L2、GPU）；几乎所有设备特定操作；路径：sys_ioctl → 驱动 ioctl_ops（例如 VIDIOC_DQBUF 阻塞直到有帧）。

---

## eBPF

程序在内核中运行（**经校验、JIT**）；挂接到 kprobes、tracepoints、uprobes、XDP。**在不修改内核或应用的情况下实现可观测性**：剖析系统调用、调度器、驱动路径。bpftrace、BCC、libbpf；**生产安全的插桩**。

---

## 小结

| L3 | IRQ 与异常；GIC/APIC；MSI-X；上半部/下半部；softirq/tasklet/workqueue；保持上半部 &lt;1 µs。 |
| L4 | 系统调用路径与代价；用于计时的 vDSO；mmap/ioctl；用于可观测性的 eBPF。 |

---

## AI 硬件关联

- L3：摄像头/GPU/NVMe 流水线依赖 IRQ 流程；NVMe 与 GPU 使用 MSI-X 每队列；过长的 ISR 会阻塞 RT 任务 —— 用下半部干活。
- L4：摄像头路径中的 V4L2 ioctl 代价；用 vDSO 打时间戳；用 eBPF 在不改代码的前提下 trace 推理与驱动延迟。

---

*合并讲义 L3、L4（中断、异常与下半部；系统调用、vDSO 与 eBPF）。*


<details>
<summary>English original</summary>

**Lecture Note 02 (L3, L4): Interrupts, Exceptions & Bottom Halves; System Calls, vDSO & eBPF**

**Combines:** Lecture L3 (Interrupts, Exceptions & Bottom Halves) and Lecture L4 (System Calls, vDSO & eBPF).

---

**How This Note Is Organized**

1. **Part 1 (L3) — Interrupts & bottom halves:** Interrupts vs exceptions; GIC/APIC; MSI/MSI-X; top half vs bottom half; softirq, tasklet, workqueue; interrupt flow and latency.
2. **Part 2 (L4) — System calls & vDSO:** Syscall path (x86/ARM64); overhead and mitigations; vDSO for time; key syscalls (mmap, ioctl); eBPF for observability.

---

**Part 1 (L3): Interrupts, Exceptions & Bottom Halves**

**Context:** Hardware signals the CPU asynchronously (camera frame, GPU done, packet). Kernel uses a **top half** (fast, acknowledge HW, schedule work) and **bottom half** (do work without holding IRQs disabled). Misuse causes dropped frames and latency spikes.

---

**Interrupts vs Exceptions**

| Type | Origin | Sync? | Example |
|------|--------|-------|--------|
| Hardware interrupt | Device IRQ | No | NIC, GPU done, camera frame |
| Software interrupt | INT/SVC | Yes | Syscall, breakpoint |
| Exception (fault) | CPU error | Yes | Page fault, div-by-zero |
| Exception (abort) | Unrecoverable | Yes | Machine check |

**Fault:** re-execute instruction after fix. **Trap:** advance after handler. **Abort:** no return.

---

**Interrupt Controllers**

**ARM GIC v3:** SGI (0–15, IPI), PPI (16–31, per-CPU), SPI (32–1019, devices). Priority 0–255; PMR masks. **x86 APIC:** Local APIC + I/O APIC; IDT; TPR. **MSI/MSI-X (PCIe):** Message-signaled interrupts; MSI-X gives **per-queue vectors and CPU affinity** — used by NVMe, GPU, NICs for scalability.

---

**Top Half vs Bottom Half**

**Top half (ISR):** Runs with IRQs disabled on local CPU; must be very short (acknowledge HW, save pointer, schedule bottom half); cannot sleep. **Bottom half:** Softirq, tasklet, or workqueue; can do real work; some can sleep (workqueue). Long top half blocks other IRQs and adds latency.

---

**Bottom-Half Mechanisms**

**Softirq:** Statically allocated; runs in ksoftirqd or after hardirq; cannot sleep. **Tasklet:** Built on softirq; one at a time per CPU. **Workqueue:** Process context; can sleep; for heavier work (e.g. block I/O). Threaded IRQs run handler in a kthread — preemptible under PREEMPT_RT.

---

**Part 2 (L4): System Calls, vDSO & eBPF**

**Context:** User code reaches kernel only via **syscalls** (toll booth). Cost: mode switch, Spectre/Meltdown mitigations, TLB effects (**~100–400 ns round-trip**). **vDSO** avoids syscall for time; **eBPF** observes kernel without changing code.

---

**Syscall Path**

x86-64: SYSCALL → entry_SYSCALL_64 → sys_call_table[rax] → SYSRET. ARM64: SVC → el0_svc → sys_call_table. `strace -c` / `strace -T -e mmap,ioctl` to count and time syscalls.

---

**Syscall Overhead**

**Mode switch ~50–150 ns**; mitigations (KPTI, IBRS, retpoline) add ~50–200 ns on x86; ARM64 lighter. At 200 fps × 4 ioctls = 1600/s × 300 ns ≈ 0.5 ms/s; **batching and zero-copy** (mmap, io_uring) reduce crossings.

---

**vDSO**

Kernel maps a **read-only page** with time (and a few other) helpers. `clock_gettime(CLOCK_MONOTONIC)`, `gettimeofday()` can resolve **in userspace** (~10–20 ns) without SYSCALL. Use CLOCK_MONOTONIC for timing and sensor fusion; **CLOCK_REALTIME can jump** (NTP).

---

**Key Syscalls: mmap, ioctl**

**mmap:** MAP_SHARED for zero-copy shm; MAP_HUGETLB for large buffers and TLB reduction; CUDA pinned and Jetson unified memory use mmap on device nodes. **ioctl:** Device control (V4L2, GPU); nearly all device-specific ops; path: sys_ioctl → driver ioctl_ops (e.g. VIDIOC_DQBUF blocks until frame).

---

**eBPF**

Programs run in kernel (**verified, JIT**); attach to kprobes, tracepoints, uprobes, XDP. **Observability without modifying kernel or app**: profile syscalls, scheduler, driver paths. bpftrace, BCC, libbpf; **production-safe instrumentation**.

---

**Summary**

| L3 | IRQ vs exception; GIC/APIC; MSI-X; top/bottom half; softirq/tasklet/workqueue; keep top half &lt;1 µs. |
| L4 | Syscall path and cost; vDSO for time; mmap/ioctl; eBPF for observability. |

---

**AI Hardware Connection**

- L3: Camera/GPU/NVMe pipelines depend on IRQ flow; MSI-X per queue for NVMe and GPU; long ISR blocks RT tasks — use bottom half for work.
- L4: V4L2 ioctl cost in camera path; vDSO for timestamps; eBPF to trace inference and driver latency without code changes.

---

*Combines Lectures L3, L4 (Interrupts, Exceptions & Bottom Halves; System Calls, vDSO & eBPF).*

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-Note-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-Note-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
