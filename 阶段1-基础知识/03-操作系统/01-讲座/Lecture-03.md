---
title: 第 3 讲：中断、异常与下半部
description: 第 3 讲：中断、异常与下半部
---

# 第 3 讲：中断、异常与下半部

## 概述

硬件不会等软件来询问它——当有事情需要处理时，它会异步地通知 CPU。本讲要解决的核心挑战是：内核如何足够快地响应硬件事件（一帧相机图像到达、一个 GPU 任务完成、一个网络数据包抵达），从而不丢任何东西，同时又不会为了记账而独占 CPU？心智模型是一条 **两阶段流水线**：上半部快速执行，在微秒级确认硬件；下半部灵活处理，稍后完成真正的工作而不阻塞正常执行。对 AI 硬件工程师而言，中断是每条相机流水线、每个 GPU 完成事件、每条 CAN 总线消息的基础——误解它们会导致丢帧、延迟尖峰以及事后极难调试的驱动挂起。

---

## 中断与异常

**中断**和**异常**都会让 CPU 停下正在做的事并运行内核代码，但它们在**来源和时序**上有所不同。

| 类型 | 来源 | 同步？ | 示例 |
|---|---|---|---|
| 硬件中断 | 外部设备拉高 IRQ 线 | 否（异步） | NIC 数据包到达、GPU 任务完成、相机帧结束 |
| 软件中断 | CPU 执行 INT/SVC 指令 | 是（同步陷阱） | 系统调用、调试断点 |
| 异常（fault） | CPU 在指令执行期间检测到错误 | 是（同步） | 缺页、除零、GP fault |
| 异常（abort） | 不可恢复的硬件错误 | 是（同步） | 机器检查、双重 fault |

**Fault** 在处理程序解决错误后重新执行出错的指令（例如，缺页处理程序安装一个 PTE）。**Trap** 在处理程序返回后前进到下一条指令（例如，系统调用）。**Abort** 不返回。

> **关键洞察：**“fault”和“trap”之间的区别有一个关键的实际后果。缺页是一种 fault——在内核安装有效的页表项之后，硬件会重新执行出错的那次内存访问，因此用户程序根本不知道它发生过。系统调用是一种 trap——内核执行完服务后，返回到 `SYSCALL` 指令*之后*的那条指令。在自定义异常处理程序中弄错这一点，意味着要么重新执行一条本不该重复的指令，要么跳过一条本不该跳过的指令。

---

## 中断控制器

### ARM GIC（通用中断控制器 v3）

用于 Jetson Orin、Qualcomm SoC、NXP i.MX。

| 中断类型 | ID 范围 | 描述 |
|---|---|---|
| SGI（软件生成） | 0–15 | IPI——一个 CPU 向另一个 CPU 发信号；用于调度器迁移和 TLB shootdown |
| PPI（每 CPU 私有） | 16–31 | 每核定时器、PMU（性能监控） |
| SPI（共享外设） | 32–1019 | 所有外部设备中断：相机、GPU、NVMe、CAN |

**优先级级别**：0（最高）到 255（最低）。CPU 接口寄存器 `PMR`（Priority Mask Register）屏蔽低于某个阈值的中断——用于自旋锁持有区间。

```c
gic_send_sgi(target_cpu, sgi_id);  /* trigger IPI from kernel/smp.c */
```

### x86 APIC

- 每 CPU 的 Local APIC：接收 IPI 和本地定时器中断
- I/O APIC：将外部设备 IRQ 路由到特定 CPU
- IDT（中断描述符表）：256 个条目；0–31 保留给 CPU 异常；32–255 用于外部 IRQ 和软件陷阱
- TPR（任务优先级寄存器）：屏蔽优先级等于或低于某级别中断

---

## MSI 与 MSI-X（PCIe）

传统的基于线的 IRQ 共享物理线路，限制了并行性。PCIe **MSI**（消息信号中断）用**对特殊 MMIO 地址的内存写**取代了拉线——CPU 的 APIC/GIC 将该写解释为一次中断。

把传统 IRQ 线想象成一条**共享电话线**——同一时刻只有一个呼叫者。**MSI-X** 为每个设备队列提供其自己的**专用电话线**，并配有专用的接收 CPU。

| 特性 | MSI | MSI-X |
|---|---|---|
| 每设备最大向量数 | 32 | 2048 |
| 每向量 CPU 亲和性 | 否 | 是（每个向量可独立设置亲和性） |
| 向量表位置 | 能力寄存器 | 独立的 BAR 区域 |


<details>
<summary>English original</summary>

**Lecture 3: Interrupts, Exceptions & Bottom Halves**

**Overview**

Hardware does not wait for software to ask it questions — it signals the CPU asynchronously when something needs attention. The core challenge this lecture addresses is: how does the kernel respond to hardware events (a camera frame arriving, a GPU job completing, a network packet landing) quickly enough that nothing is dropped, while also not monopolizing the CPU for bookkeeping? The mental model is a **two-stage pipeline**: a fast top half that acknowledges the hardware in microseconds, and a flexible bottom half that does the real work later without blocking normal execution. For an AI hardware engineer, interrupts are the foundation of every camera pipeline, GPU completion event, and CAN bus message — misunderstanding them leads to dropped frames, latency spikes, and driver hangs that are very hard to debug after the fact.

---

**Interrupts vs Exceptions**

Both **interrupts** and **exceptions** cause the CPU to stop what it is doing and run kernel code, but they differ in **origin and timing**.

| Type | Origin | Synchronous? | Example |
|---|---|---|---|
| Hardware interrupt | External device asserts IRQ line | No (async) | NIC packet arrives, GPU job done, camera frame end |
| Software interrupt | CPU executes INT/SVC instruction | Yes (sync trap) | System call, debug breakpoint |
| Exception (fault) | CPU detects error during instruction | Yes (sync) | Page fault, divide-by-zero, GP fault |
| Exception (abort) | Unrecoverable hardware error | Yes (sync) | Machine check, double fault |

**Faults** re-execute the faulting instruction after the handler resolves the error (e.g., page fault installs a PTE). **Traps** advance to the next instruction after the handler returns (e.g., syscall). **Aborts** do not return.

> **Key Insight:** The distinction between "fault" and "trap" has a critical practical consequence. A page fault is a fault — the hardware re-executes the faulting memory access after the kernel installs a valid page table entry, so the user program never knows it happened. A system call is a trap — the kernel executes the service and returns to the instruction *after* the `SYSCALL` instruction. Getting this wrong in a custom exception handler means either re-executing an instruction that shouldn't be repeated, or skipping one that should.

---

**Interrupt Controllers**

**ARM GIC (Generic Interrupt Controller v3)**

Used on Jetson Orin, Qualcomm SoCs, NXP i.MX.

| Interrupt type | ID range | Description |
|---|---|---|
| SGI (Software Generated) | 0–15 | IPI — one CPU signals another; used by scheduler migration and TLB shootdowns |
| PPI (Per-CPU Private) | 16–31 | Per-core timers, PMU (performance monitoring) |
| SPI (Shared Peripheral) | 32–1019 | All external device interrupts: cameras, GPUs, NVMe, CAN |

**Priority levels**: 0 (highest) to 255 (lowest). CPU interface register `PMR` (Priority Mask Register) masks interrupts below a threshold — used during spinlock-held sections.

```c
gic_send_sgi(target_cpu, sgi_id);  /* trigger IPI from kernel/smp.c */
```

**x86 APIC**

- Local APIC per CPU: receives IPIs and local timer interrupts
- I/O APIC: routes external device IRQs to specific CPUs
- IDT (Interrupt Descriptor Table): 256 entries; 0–31 reserved for CPU exceptions; 32–255 for external IRQs and software traps
- TPR (Task Priority Register): masks interrupts at or below a priority level

---

**MSI and MSI-X (PCIe)**

Legacy wire-based IRQs share physical lines, limiting parallelism. PCIe **MSI** (Message Signaled Interrupts) replaces line assertion with a **memory write to a special MMIO address** — the CPU's APIC/GIC interprets the write as an interrupt.

Think of legacy IRQ lines as a single **shared phone line** — only one caller at a time. **MSI-X** gives each device queue its own **dedicated phone line** with a dedicated recipient CPU.

| Feature | MSI | MSI-X |
|---|---|---|
| Max vectors per device | 32 | 2048 |
| Per-vector CPU affinity | No | Yes (each vector independently affinable) |
| Vector table location | Capability register | Separate BAR region |

</details>

### 为什么 MSI-X 对 AI 硬件很重要

- **NVMe**：32+ 个队列各自获得专属的 MSI-X 向量，并绑定到运行该队列 I/O 的核上 —— 消除经 GPUDirect Storage 加载大模型权重时的争用。
- **NVIDIA GPU**：CUDA compute engine 完成、copy engine 完成与故障通知各使用独立的 MSI-X 向量 —— CUDA 的事件系统建立在按引擎投递 MSI-X 之上。
- **NICs（100GbE）**：按 TX/RX 队列独立的 MSI-X 使多队列 RSS 无需锁竞争。

```bash
cat /proc/interrupts | grep nvidia   # per-CPU counts for each GPU MSI-X vector
cat /proc/interrupts | grep nvme     # per-queue NVMe completion counts
```

> **关键洞察：** 当 GPU 上的 CUDA kernel 执行完毕，GPU 会把完成值写入一个内存映射寄存器。PCIe 桥将其转换为一次发往 CPU APIC 的 MSI-X 写。CPU 触发中断，唤醒 CUDA runtime 线程。从 GPU 完成到用户态唤醒的整条链路都要经过 MSI-X。若 MSI-X 向量被路由到错误的 CPU（一个正忙于其他工作的 CPU），唤醒延迟会增加 10–50 µs。把 MSI-X 向量绑定到 CUDA stream 管理核即可消除该问题。

---

## 中断处理流程

从硬件事件到内核处理程序的完整路径是**确定性的**，每次都走同一条路径。理解这条路径有助于在正确的点做埋点，以进行**延迟测量**。

```
Hardware Interrupt Flow — Full Path
┌────────────────────────────────────────────────────────┐
│  HARDWARE                                              │
│  Camera sensor → frame-end pulse → CSI controller     │
│  → GIC SPI assertion                                  │
└────────────────────┬───────────────────────────────────┘
                     │  IRQ line asserted
                     ▼
┌────────────────────────────────────────────────────────┐
│  INTERRUPT CONTROLLER (GIC / APIC)                     │
│  1. Assigns IRQ number                                 │
│  2. Selects target CPU (affinity mask)                 │
│  3. Signals selected CPU                               │
└────────────────────┬───────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────┐
│  CPU — TOP HALF (hardirq context)                      │
│  4. Completes current instruction                      │
│  5. Saves minimal state to kernel stack (PC, SP, regs) │
│  6. Looks up handler:                                  │
│     x86: IDT[irq_number] → ISR function pointer       │
│     ARM64: VBAR_EL1 vector table entry                 │
│  7. ISR runs: acknowledge HW, save data ptr,           │
│     schedule bottom half (raise_softirq / queue_work) │
│  8. EOI (End Of Interrupt) written to GIC/APIC         │
└────────────────────┬───────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────┐
│  BOTTOM HALF (softirq / workqueue / threaded IRQ)      │
│  9. Process captured data                              │
│  10. Wake waiting user-space processes (camerad)       │
│  11. Return to previously preempted context            │
└────────────────────────────────────────────────────────┘
```

编号序列详解：

1. **设备置起 IRQ**：相机传感器的 frame-end 引脚拉高；GIC 捕获到该信号。
2. **GIC 选择目标 CPU**：使用来自 `/proc/irq/N/smp_affinity` 的亲和性配置。
3. **CPU 执行完当前指令**：CPU 不会在指令中途丢弃；它先完成当前操作。
4. **状态保存到内核栈**：CPU 硬件自动把最小寄存器状态（PC、SP、PSR/RFLAGS）压入当前进程的内核栈。
5. **查向量表**：x86 读取 IDT；ARM64 跳转到对应异常类的 VBAR_EL1 向量入口。
6. **ISR（上半部）运行**：确认硬件寄存器（清除设备中的中断标志），保存指向所接收数据的指针，调度延迟工作。
7. **写入 EOI**：告知中断控制器本 CPU 已完成该中断的处理；允许控制器投递下一个中断。
8. **softirqs/workqueues 运行**：真正的数据处理在此进行，此时中断已重新使能。

---


<details>
<summary>English original</summary>

**Why MSI-X Matters for AI Hardware**

- **NVMe**: 32+ queues each get a dedicated MSI-X vector, pinned to the core running that queue's I/O — eliminates contention during large model weight loading via GPUDirect Storage.
- **NVIDIA GPU**: separate MSI-X vectors for CUDA compute engine completion, copy engine completion, and fault notification — CUDA's event system is built on per-engine MSI-X delivery.
- **NICs (100GbE)**: per-TX/RX-queue MSI-X enables multi-queue RSS without lock contention.

```bash
cat /proc/interrupts | grep nvidia   # per-CPU counts for each GPU MSI-X vector
cat /proc/interrupts | grep nvme     # per-queue NVMe completion counts
```

> **Key Insight:** When a CUDA kernel finishes on a GPU, the GPU writes a completion value to a memory-mapped register. The PCIe bridge converts this into an MSI-X write to the CPU's APIC. The CPU fires the interrupt, wakes the CUDA runtime thread. The entire chain from GPU completion to userspace wakeup passes through MSI-X. If the MSI-X vector is routed to the wrong CPU (one that is busy with other work), the wakeup latency increases by 10–50 µs. Pinning the MSI-X vector to the CUDA stream management core eliminates this.

---

**Interrupt Handling Flow**

The full path from hardware event to kernel handler is **deterministic** and takes the same path every time. Understanding this path helps you instrument the right points for **latency measurement**.

```
Hardware Interrupt Flow — Full Path
┌────────────────────────────────────────────────────────┐
│  HARDWARE                                              │
│  Camera sensor → frame-end pulse → CSI controller     │
│  → GIC SPI assertion                                  │
└────────────────────┬───────────────────────────────────┘
                     │  IRQ line asserted
                     ▼
┌────────────────────────────────────────────────────────┐
│  INTERRUPT CONTROLLER (GIC / APIC)                     │
│  1. Assigns IRQ number                                 │
│  2. Selects target CPU (affinity mask)                 │
│  3. Signals selected CPU                               │
└────────────────────┬───────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────┐
│  CPU — TOP HALF (hardirq context)                      │
│  4. Completes current instruction                      │
│  5. Saves minimal state to kernel stack (PC, SP, regs) │
│  6. Looks up handler:                                  │
│     x86: IDT[irq_number] → ISR function pointer       │
│     ARM64: VBAR_EL1 vector table entry                 │
│  7. ISR runs: acknowledge HW, save data ptr,           │
│     schedule bottom half (raise_softirq / queue_work) │
│  8. EOI (End Of Interrupt) written to GIC/APIC         │
└────────────────────┬───────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────┐
│  BOTTOM HALF (softirq / workqueue / threaded IRQ)      │
│  9. Process captured data                              │
│  10. Wake waiting user-space processes (camerad)       │
│  11. Return to previously preempted context            │
└────────────────────────────────────────────────────────┘
```

The numbered sequence in detail:

1. **Device asserts IRQ**: the camera sensor's frame-end pin goes high; the GIC sees it.
2. **GIC selects target CPU**: using the affinity configuration from `/proc/irq/N/smp_affinity`.
3. **CPU finishes current instruction**: the CPU does not drop mid-instruction; it completes the current operation first.
4. **State saved to kernel stack**: the CPU hardware automatically pushes the minimal register state (PC, SP, PSR/RFLAGS) to the current process's kernel stack.
5. **Vector table lookup**: x86 reads the IDT; ARM64 jumps to the VBAR_EL1 vector entry for the appropriate exception class.
6. **ISR (top half) runs**: acknowledges the hardware register (clears the interrupt flag in the device), saves a pointer to the received data, schedules deferred work.
7. **EOI written**: signals the interrupt controller that this CPU has finished handling the interrupt; allows the controller to deliver the next interrupt.
8. **Softirqs/workqueues run**: the actual data processing happens here, with interrupts re-enabled.

---

</details>

## 上半部 vs 下半部

| 部分 | 上下文 | 能否睡眠？ | 目标 |
|---|---|---|---|
| 上半部（hardirq / 中断服务程序 ISR） | 本地 CPU 上 IRQ 被禁用 | 否 | 应答硬件；调度延迟工作；理想情况下须在 < 1 µs 内完成 |
| 下半部 | IRQ 重新启用 | 取决于机制 | 处理数据；唤醒等待者；执行实际工作 |

**ISR 必须尽量精简**：应答中断控制器、保存指向接收数据的指针，并调度一个下半部。所有 I/O 处理都发生在**下半部**中。

> **关键洞察：** 上半部之所以必须短，是因为在它运行期间，本地 CPU 无法接收任何其他中断（中断被禁用）。如果处理一帧的摄像头帧 ISR 要花 100 µs，那么在该时间窗口内，这个 CPU 上的其他中断都无法被应答——包括调度器 tick，这意味着其他 RT 任务无法被唤醒。过长的 ISR 会产生延迟气泡，表现为完全不相关进程中莫名其妙的延迟。

---

## 下半部机制

Linux 中有三种主要的**下半部机制**，在延迟、灵活性和兼容性之间有着不同的取舍。选对机制关系到**驱动的正确性与性能**。

### 软中断

- 10 种静态定义的类型：`NET_TX`、`NET_RX`、`BLOCK`、`TASKLET`、`SCHED`、`HRTIMER`、`RCU` 以及其他
- 在 hardirq 完成后立即于中断上下文中运行；可在多个 CPU 上同时并发运行（无 per-softirq 锁）
- 不能睡眠；不打补丁改内核就无法动态添加
- 当积压过大时，`ksoftirqd/N` 内核线程会排空队列以限制延迟

```c
raise_softirq(NET_RX_SOFTIRQ);   /* schedule NET_RX softirq from ISR */
```

### Tasklet

- 动态分配；构建在 `TASKLET_SOFTIRQ` 之上
- 按实例串行化——同一个 tasklet 无法同时在两个 CPU 上运行
- 自 Linux 5.x 起在新驱动代码中**已废弃**；应迁移到工作队列或线程化 IRQ

### 工作队列

延迟工作在**内核线程**（`kworker/N:M`）中执行。**进程上下文**——可以睡眠、分配内存、获取互斥锁。

```c
INIT_WORK(&work, handler_fn);         /* initialize work item; binds handler function */
schedule_work(&work);                  /* queue onto system_wq; runs in next available kworker */
schedule_work_on(cpu, &work);         /* queue to specific CPU's kworker; avoids migration */

/* Dedicated high-priority workqueue for camera ISP completion */
wq = alloc_workqueue("isp_done", WQ_HIGHPRI | WQ_UNBOUND, 1);
/* WQ_HIGHPRI: kworker runs at nice -20; WQ_UNBOUND: not tied to a specific CPU */
queue_work(wq, &work);
```

这段代码为摄像头 ISP 完成创建了一个专用的高优先级工作队列。`WQ_HIGHPRI` 标志确保内核 worker 线程在普通优先级工作之前处理 ISP 完成，从而缩短从帧捕获到 `camerad` 能出队缓冲区之间的延迟。

| 工作队列 | 线程 | 用途 |
|---|---|---|
| `system_wq` | 共享 `kworker` 池 | 通用延迟工作 |
| `system_highpri_wq` | 高优先级 kworker | 延迟敏感路径（摄像头、CAN） |
| `system_unbound_wq` | 不绑定 CPU | 应自由迁移的工作 |
| 通过 `alloc_workqueue()` 自定义 | 专用 | 单个驱动独占的队列 |


<details>
<summary>English original</summary>

**Top Half vs Bottom Half**

| Half | Context | Can sleep? | Goal |
|---|---|---|---|
| Top half (hardirq / ISR) | IRQs disabled on local CPU | No | Acknowledge hardware; schedule deferred work; must complete in < 1 µs ideal |
| Bottom half | IRQs re-enabled | Depends on mechanism | Process data; wake waiters; do actual work |

The **ISR must be minimal**: acknowledge the interrupt controller, save a pointer to received data, and schedule a bottom half. All I/O processing happens in **bottom halves**.

> **Key Insight:** The reason the top half must be short is that while it runs, the local CPU cannot receive any other interrupts (they are disabled). If a camera frame ISR takes 100 µs to process a frame, no other interrupts on that CPU can be acknowledged during that window — including the scheduler tick, which means other RT tasks cannot be woken. Long ISRs create latency bubbles that appear as mysterious delays in completely unrelated processes.

---

**Bottom-Half Mechanisms**

There are three main **bottom-half mechanisms** in Linux, with different tradeoffs between latency, flexibility, and compatibility. Choosing the right one matters for **driver correctness and performance**.

**Softirqs**

- 10 statically defined types: `NET_TX`, `NET_RX`, `BLOCK`, `TASKLET`, `SCHED`, `HRTIMER`, `RCU`, and others
- Run in interrupt context immediately after hardirq completion; can run concurrently on multiple CPUs simultaneously (no per-softirq lock)
- Cannot sleep; cannot be dynamically added without patching the kernel
- When backlog grows too large, `ksoftirqd/N` kernel threads drain the queue to bound latency

```c
raise_softirq(NET_RX_SOFTIRQ);   /* schedule NET_RX softirq from ISR */
```

**Tasklets**

- Dynamically allocated; built on `TASKLET_SOFTIRQ`
- Serialized per instance — same tasklet cannot run on two CPUs simultaneously
- **Deprecated** in new driver code since Linux 5.x; migrate to workqueues or threaded IRQs

**Workqueues**

Deferred work executed in **kernel threads** (`kworker/N:M`). **Process context** — can sleep, allocate memory, take mutexes.

```c
INIT_WORK(&work, handler_fn);         /* initialize work item; binds handler function */
schedule_work(&work);                  /* queue onto system_wq; runs in next available kworker */
schedule_work_on(cpu, &work);         /* queue to specific CPU's kworker; avoids migration */

/* Dedicated high-priority workqueue for camera ISP completion */
wq = alloc_workqueue("isp_done", WQ_HIGHPRI | WQ_UNBOUND, 1);
/* WQ_HIGHPRI: kworker runs at nice -20; WQ_UNBOUND: not tied to a specific CPU */
queue_work(wq, &work);
```

This code creates a dedicated high-priority workqueue for camera ISP completions. The `WQ_HIGHPRI` flag ensures the kernel worker thread processes ISP completions before normal-priority work, reducing the delay between frame capture and when `camerad` can dequeue the buffer.

| Workqueue | Threads | Use |
|---|---|---|
| `system_wq` | Shared `kworker` pool | General deferred work |
| `system_highpri_wq` | High-priority kworkers | Latency-sensitive paths (camera, CAN) |
| `system_unbound_wq` | CPU-unbound | Work that should migrate freely |
| Custom via `alloc_workqueue()` | Dedicated | Exclusive queue for one driver |

</details>

### 线程化 IRQ

```c
request_threaded_irq(irq, hard_handler, thread_fn,
                     IRQF_SHARED, "cam-frame-done", dev);
/* hard_handler: runs in hardirq context — must be minimal */
/* thread_fn: runs in kernel thread "irq/N-cam-frame-done" — can sleep */
```

- `hard_handler`：最小的 hardirq 上下文 —— 应答硬件后返回 `IRQ_WAKE_THREAD`
- `thread_fn`：运行在专用内核线程（`irq/N-cam-frame-done`）中 —— 可以睡眠、加锁、调用 `v4l2_buffer_done()`
- **所有新驱动的首选**；在 `PREEMPT_RT` 下为必需（Linux 6.12 已合入主线）
- 线程优先级可通过 `chrt` 设置，从而借助 RT 调度器实现有界延迟
- `IRQF_NO_THREAD`：即使在 PREEMPT_RT 下也强制使用真正的 hardirq 上下文 —— 仅用于中断控制器和 `hrtimer`

```
Bottom-Half Mechanism Comparison
┌──────────────┬──────────────┬───────────────┬──────────────────┐
│  Mechanism   │   Context    │  Can Sleep?   │  Latency Target  │
├──────────────┼──────────────┼───────────────┼──────────────────┤
│  Softirq     │ IRQ context  │      No       │    < 10 µs       │
│  Tasklet     │ Via softirq  │      No       │    < 10 µs       │
│  (deprecated)│              │               │                  │
│  Workqueue   │  kworker     │     Yes       │   10–100s µs     │
│  Threaded IRQ│  irq/N thd   │     Yes       │  RT-schedulable  │
└──────────────┴──────────────┴───────────────┴──────────────────┘
```

> **关键洞察：** 线程化 IRQ 是对一个根本性矛盾的现代解答：hardirq 上下文必须快且不能睡眠，但真正要做的工作（处理一帧相机图像、完成一次 DMA 传输）往往需要分配内存、获取 mutex，或调用会睡眠的 API。把工作搬到内核线程后，线程化 IRQ 兼得两者之长 —— 响应依然很快（hard handler 立即应答硬件），而实际处理运行在一个优先级有界的可调度线程中。

> **常见陷阱：** 对在 ISR 中调用睡眠函数的驱动，使用 `request_irq()` 而非 `request_threaded_irq()` 会导致 `BUG: scheduling while atomic` 内核告警，并可能崩溃。任何可能阻塞的函数（mutex_lock、msleep、copy_to_user、GFP_KERNEL 分配）在 hardirq 上下文中都是禁止的。在 `PREEMPT_RT` 下，这条规则更严格 —— spinlock 变成睡眠锁，因此 hardirq ISR 中几乎任何操作都可能意外睡眠。

---

## 中断亲和性

```bash
cat /proc/irq/42/smp_affinity         # hex bitmask (e.g., 0x8 = CPU 3)
cat /proc/irq/42/smp_affinity_list    # human-readable (e.g., "3")
echo 8 > /proc/irq/42/smp_affinity   # pin IRQ 42 to CPU 3
systemctl stop irqbalance             # prevent irqbalance from overriding manual settings
```

把 GPU 的 MSI-X 完成向量绑定到与 CUDA stream 管理线程相同的核心，可消除 10–50 µs 的**跨核唤醒延迟**。相机帧完成 IRQ 应当**绑定到远离 RT 推理核心的位置**，以免 ISR 执行干扰截止时间任务。

> **常见陷阱：** `irqbalance` 是一个守护进程，会自动在 CPU 之间重新分配 IRQ 以均衡负载。它会周期性地覆盖任何手动的 `smp_affinity` 设置。在生产推理系统上手动绑定 IRQ 之前，务必先停止 `irqbalance`。若需要部分手动控制，可用 `irqbalance --banirq=N` 把特定 IRQ 排除在均衡之外。

理解了 IRQ 如何投递与处理后，接下来看一项针对高带宽设备的专门优化 —— 这类设备若逐个中断上报，会把 CPU 淹没。

---

## 中断合并：NAPI

对于网络和高带宽设备，在高包速率下每包触发一次中断是不可持续的。**NAPI**（New API）采用中断合并：

1. **首个包到达** → 触发一次中断。
2. **ISR 关闭该队列的后续中断**，通过 softirq 调度一个 `poll()` 回调。
3. **`poll()` 在 softirq 上下文中运行**，循环清空队列（最多 budget 个包）。
4. **队列清空** → 重新启用该队列的中断。

这样就把**中断开销分摊**到大量包上。代价是**延迟**：重新启用中断后的首个包可能要等到下一次中断。

取舍：合并会增加延迟（最多 `ethtool -C eth0 rx-usecs`），但在高包速率下能大幅降低中断开销。这对多相机 Ethernet（GigE Vision）以及 AI 服务器机架中的 RDMA NIC 配置都适用。

---


<details>
<summary>English original</summary>

**Threaded IRQs**

```c
request_threaded_irq(irq, hard_handler, thread_fn,
                     IRQF_SHARED, "cam-frame-done", dev);
/* hard_handler: runs in hardirq context — must be minimal */
/* thread_fn: runs in kernel thread "irq/N-cam-frame-done" — can sleep */
```

- `hard_handler`: minimal hardirq context — acknowledges hardware, returns `IRQ_WAKE_THREAD`
- `thread_fn`: runs in dedicated kernel thread (`irq/N-cam-frame-done`) — can sleep, take locks, call `v4l2_buffer_done()`
- **Preferred for all new drivers**; required under `PREEMPT_RT` (mainlined in Linux 6.12)
- Thread priority is settable via `chrt`, enabling bounded latency via the RT scheduler
- `IRQF_NO_THREAD`: forces true hardirq context even under PREEMPT_RT — use only for interrupt controllers and `hrtimer`

```
Bottom-Half Mechanism Comparison
┌──────────────┬──────────────┬───────────────┬──────────────────┐
│  Mechanism   │   Context    │  Can Sleep?   │  Latency Target  │
├──────────────┼──────────────┼───────────────┼──────────────────┤
│  Softirq     │ IRQ context  │      No       │    < 10 µs       │
│  Tasklet     │ Via softirq  │      No       │    < 10 µs       │
│  (deprecated)│              │               │                  │
│  Workqueue   │  kworker     │     Yes       │   10–100s µs     │
│  Threaded IRQ│  irq/N thd   │     Yes       │  RT-schedulable  │
└──────────────┴──────────────┴───────────────┴──────────────────┘
```

> **Key Insight:** Threaded IRQs are the modern answer to a fundamental tension: hardirq context needs to be fast and cannot sleep, but real work (processing a camera frame, completing a DMA transfer) often needs to allocate memory, take a mutex, or call sleeping APIs. By moving the work to a kernel thread, threaded IRQs get the best of both worlds — they still respond quickly (the hard handler acknowledges the hardware immediately), but the actual processing runs in a schedulable thread with bounded priority.

> **Common Pitfall:** Using `request_irq()` instead of `request_threaded_irq()` for a driver that calls sleeping functions in its ISR will cause `BUG: scheduling while atomic` kernel warnings and potential crashes. Any function that can block (mutex_lock, msleep, copy_to_user, GFP_KERNEL allocation) is forbidden in hardirq context. Under `PREEMPT_RT`, this becomes even stricter — spinlocks become sleeping locks, so nearly everything in a hardirq ISR can accidentally sleep.

---

**Interrupt Affinity**

```bash
cat /proc/irq/42/smp_affinity         # hex bitmask (e.g., 0x8 = CPU 3)
cat /proc/irq/42/smp_affinity_list    # human-readable (e.g., "3")
echo 8 > /proc/irq/42/smp_affinity   # pin IRQ 42 to CPU 3
systemctl stop irqbalance             # prevent irqbalance from overriding manual settings
```

Pinning a GPU MSI-X completion vector to the same core as the CUDA stream management thread eliminates **cross-core wakeup latency** of 10–50 µs. Camera frame-done IRQs should be **pinned away from RT inference cores** to prevent ISR execution interfering with deadline tasks.

> **Common Pitfall:** `irqbalance` is a daemon that automatically redistributes IRQs across CPUs to balance load. It will override any manual `smp_affinity` settings periodically. Always stop `irqbalance` before manually pinning IRQs on a production inference system. Use `irqbalance --banirq=N` to exclude specific IRQs from balancing if you need partial manual control.

Now that we understand how IRQs are delivered and handled, let's look at a specialized optimization for high-bandwidth devices that would drown the CPU in individual interrupts.

---

**Interrupt Coalescing: NAPI**

For network and high-bandwidth devices, firing one interrupt per packet is unsustainable at high rates. **NAPI** (New API) uses interrupt coalescing:

1. **First packet arrives** → one interrupt fires.
2. **ISR disables further interrupts** for this queue, schedules a `poll()` callback via softirq.
3. **`poll()` runs in softirq context**, drains the queue in a loop (up to budget packets).
4. **Queue drained** → re-enable interrupts for this queue.

This **amortizes the interrupt overhead** across many packets. The tradeoff is **latency**: the first packet after re-enabling interrupts may wait until the next interrupt.

Tradeoff: coalescing adds latency (up to `ethtool -C eth0 rx-usecs`) but dramatically reduces interrupt overhead at high packet rates. Relevant for multi-camera Ethernet (GigE Vision) and RDMA NIC configurations in AI server racks.

---

</details>

## 异常处理：缺页

**缺页**是正常运行中最频繁的异常。处理函数：`do_page_fault()` → `handle_mm_fault()`。

缺页原因（ARM64 `ESR_EL1` 或 x86 CR2 + 错误码）：
- **匿名映射尚未分配**：分配物理页、更新 PTE、返回——程序完全感知不到。
- **文件映射不在 page cache 中**：从存储读入 cache、建立 PTE 映射——模型文件加载延迟正源于此。
- **CoW 写缺页**：分配私有副本、更新 PTE、返回——这就是 `fork()` 写时复制机制在起作用。
- **未映射地址上的权限错误**：向进程发送 SIGSEGV——这是应用程序的真实 bug。

缺页处理流程：

1. **MMU 触发错误**：硬件检测到某虚拟地址没有有效 PTE（或权限不匹配），触发错误异常。
2. **CPU 保存状态**：保存出错指令的 PC 与出错地址（x86 上存于 CR2，ARM64 上存于 FAR_EL1）。
3. **调用 `do_page_fault()`**：内核的缺页处理函数查找包含出错地址的 VMA（Virtual Memory Area，虚拟内存区域）。
4. **判定缺页类型**：依据 VMA 标志判定为匿名/文件映射/CoW/非法。
5. **处理函数执行**：按缺页类型分配页、从磁盘读取或执行复制。
6. **装入 PTE**：把新物理页的地址写入页表。
7. **重新执行指令**：重试出错指令——这次成功，因为 PTE 现已有效。

在 `PREEMPT_RT` 下，RT 线程中的缺页会引入无界的延迟尖峰。`mlockall(MCL_CURRENT | MCL_FUTURE)` 通过在 RT 阶段开始前把所有页锁定在 RAM 中来避免这一点。

```c
mlockall(MCL_CURRENT | MCL_FUTURE);
// Pin ALL currently-mapped and future-mapped pages in RAM.
// MCL_CURRENT: lock all pages mapped right now (stack, heap, libraries).
// MCL_FUTURE:  lock all pages mapped after this call (new mmap, malloc, stack growth).
// Without this, a page fault during inference adds 1–10 ms of latency
// as the kernel reads from NVMe into page cache before resuming the RT thread.
// Requires CAP_IPC_LOCK capability.
```

这段代码防止推理进程的任何页在 RT 线程运行期间被换出到 swap 或被解除映射。

> **常见陷阱：** 只调用 `mlockall()` 而不预先触发缺页，每个页的首次访问仍会引发一次缺页，尽管此后该页会保持锁定。正确的做法是：`mlockall(MCL_CURRENT | MCL_FUTURE)`，然后触碰工作集的每一个页（向每个页写入一个字节），再进入 RT 循环。`MCL_FUTURE` 标志确保栈增长和新的 `mmap` 调用也会锁定各自的页，但这些页在首次访问时仍会发生缺页——只是缺页后立即被锁定。

---

## 小结

| Mechanism | Context | Can sleep? | Latency target | Example use |
|---|---|---|---|---|
| Top half (ISR/hardirq) | Interrupts disabled | No | < 1 µs | Acknowledge HW, start DMA |
| Softirq | Post-hardirq interrupt context | No | < 10 µs | Network RX/TX, block completion |
| Tasklet | Via softirq (serialized) | No | < 10 µs | Legacy; deprecated |
| Workqueue | Process context (kworker) | Yes | 10s–100s µs | General deferred work, memory allocation |
| Threaded IRQ | Process context (irq/N thread) | Yes | Bounded by RT scheduler | Modern drivers, PREEMPT_RT compatible |


<details>
<summary>English original</summary>

**Exception Handling: Page Faults**

**Page fault** is the most frequent exception in normal operation. Handler: `do_page_fault()` → `handle_mm_fault()`.

Fault reasons (ARM64 `ESR_EL1` or x86 CR2 + error code):
- **Anonymous mapping not yet allocated**: allocate physical page, update PTE, return — the program never sees this.
- **File-backed mapping not in page cache**: read from storage into cache, map PTE — this is where model file loading latency comes from.
- **CoW write fault**: allocate private copy, update PTE, return — this is the `fork()` copy-on-write mechanism in action.
- **Permission fault on unmapped address**: send SIGSEGV to process — a real bug in the application.

The page fault handling sequence:

1. **MMU raises fault**: the hardware detects that a virtual address has no valid PTE (or wrong permissions) and raises a fault exception.
2. **CPU saves state**: the faulting instruction's PC and the fault address are saved (in CR2 on x86, in FAR_EL1 on ARM64).
3. **`do_page_fault()` called**: the kernel's fault handler looks up the VMA (Virtual Memory Area) containing the faulting address.
4. **Fault type determined**: anonymous/file-backed/CoW/invalid based on VMA flags.
5. **Handler runs**: allocates page, reads from disk, or copies — depending on fault type.
6. **PTE installed**: the new physical page's address is written into the page table.
7. **Instruction re-executed**: the faulting instruction is retried — it succeeds this time because the PTE is now valid.

Under `PREEMPT_RT`, page faults in RT threads introduce unbounded latency spikes. `mlockall(MCL_CURRENT | MCL_FUTURE)` prevents this by locking all pages in RAM before the RT phase begins.

```c
mlockall(MCL_CURRENT | MCL_FUTURE);
// Pin ALL currently-mapped and future-mapped pages in RAM.
// MCL_CURRENT: lock all pages mapped right now (stack, heap, libraries).
// MCL_FUTURE:  lock all pages mapped after this call (new mmap, malloc, stack growth).
// Without this, a page fault during inference adds 1–10 ms of latency
// as the kernel reads from NVMe into page cache before resuming the RT thread.
// Requires CAP_IPC_LOCK capability.
```

This code prevents any page that is part of the inference process from being evicted to swap or unmapped while the RT thread is running.

> **Common Pitfall:** Calling `mlockall()` without pre-faulting pages can still result in the first access to each page causing a fault, even though the page will stay locked afterward. The correct pattern is: `mlockall(MCL_CURRENT | MCL_FUTURE)`, then touch every page of your working set (write a byte to each page), then enter the RT loop. The `MCL_FUTURE` flag ensures stack growth and new `mmap` calls also lock their pages, but the pages are still faulted in on first access — just locked immediately after.

---

**Summary**

| Mechanism | Context | Can sleep? | Latency target | Example use |
|---|---|---|---|---|
| Top half (ISR/hardirq) | Interrupts disabled | No | < 1 µs | Acknowledge HW, start DMA |
| Softirq | Post-hardirq interrupt context | No | < 10 µs | Network RX/TX, block completion |
| Tasklet | Via softirq (serialized) | No | < 10 µs | Legacy; deprecated |
| Workqueue | Process context (kworker) | Yes | 10s–100s µs | General deferred work, memory allocation |
| Threaded IRQ | Process context (irq/N thread) | Yes | Bounded by RT scheduler | Modern drivers, PREEMPT_RT compatible |

</details>

### 概念回顾

- **为什么上半部（ISR）必须尽可能短？** 当一个 hardirq ISR 在某个 CPU 上运行时，该 CPU 无法接收任何其他中断——它们被屏蔽了。过长的 ISR 会制造一个窗口期，在此期间该 CPU 上的所有其他设备都得不到服务。这会导致 camera 流水线丢帧，以及 RT 任务错过截止时间。
- **softirq 与 workqueue 有什么区别？** softirq 在 hardirq 完成后立即在中断上下文中运行——它不能睡眠、不能用 `GFP_KERNEL` 分配内存、也不能获取 mutex。workqueue 在内核线程（进程上下文）中运行——上述事情它都能做。当延迟执行的工作需要睡眠或加锁时，选择 workqueue。
- **为什么新驱动更倾向使用 threaded IRQ？** 它们与 `PREEMPT_RT` 兼容（此时 spinlock 会变成睡眠锁，使传统的 hardirq ISR 不再安全），其优先级可通过 `chrt` 调节，并且它们让实际的驱动工作可以使用完整的内核 API，包括会睡眠的函数。
- **什么是 page fault，什么时候代价高昂？** 当虚拟地址没有有效的物理映射时，就会发生 page fault。Minor fault（页尚未分配）在微秒级完成。Major fault（数据必须从磁盘读取）需要毫秒级。对 RT 推理线程而言，即便是 minor fault 也不可接受——`mlockall()` 可同时避免两者。
- **MSI-X 提供了哪些传统 IRQ 无法提供的能力？** 按向量的 CPU 亲和性：每个 MSI-X 向量都可以独立绑定到特定 CPU。这意味着 32 队列的 NVMe 盘可以让每个队列的完成中断唤醒提交该 I/O 的那个 CPU 线程，从而消除跨核锁竞争。
- **为什么 NAPI 要合并中断，而不是每个包一个中断？** 在 100 Gbit/s 下，网卡对 64 字节包每秒可产生约 1.48 亿次中断。每次中断都会强制一次模式切换并带来缓存影响。NAPI 把收包处理批量合并为每次中断一次 `poll()` 调用，将开销降到可管理的水平，代价是首个包的延迟略有增加。

---

## AI 硬件关联

- GPU 的 MSI-X 按引擎完成向量支持异步 CUDA stream 操作；将每个向量绑定到 CUDA stream 管理核，可消除推理流水线中 10–50 µs 的跨核唤醒开销。
- camera 帧完成 ISR → threaded IRQ 路径（或 workqueue）为 `camerad` 设定了端到端延迟下限；用 `bpftrace tracepoint:irq:irq_handler_entry` 测量它，可直接量化 openpilot 中 camera 到模型输入的延迟。
- NVMe 按队列的 MSI-X 是 GPUDirect Storage 的前置要求：每个 NVMe 完成都必须唤醒正确 NUMA 节点上的 DMA 引擎，且不产生跨节点内存流量——这要求同时配置 MSI-X 和 CPU 亲和性。
- 在 PREEMPT_RT（Linux 6.12 mainline）下，threaded IRQ 是强制要求：camera 和 GPU 的完成处理程序变为可调度线程，其延迟由 RT 调度器约束，而非由关中断窗口决定。
- `/proc/irq/N/smp_affinity` 是 AV 平台上 IRQ 隔离的主要控制点——GPU、NVMe 和 camera 的 IRQ 被移出 RT 核，以防上半部执行干扰 `modeld` 和 `controlsd` 的截止时间。
- Zynq/MPSoC 上 FPGA AXI-stream DMA 完成中断遵循同样的上半部/workqueue 模式：ISR 应答 CDMA，将处理结果缓冲区的工作项入队，并通过 wait queue 通知推理线程。


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why must the top half (ISR) be as short as possible?** While a hardirq ISR runs on a CPU, that CPU cannot receive any other interrupts — they are masked. A long ISR creates a window during which all other devices on that CPU are unserviced. This causes dropped frames in camera pipelines and missed deadlines in RT tasks.
- **What is the difference between a softirq and a workqueue?** A softirq runs in interrupt context immediately after the hardirq completes — it cannot sleep, allocate memory with `GFP_KERNEL`, or take mutexes. A workqueue runs in a kernel thread (process context) — it can do all of those things. Choose workqueues when deferred work needs to sleep or take locks.
- **Why are threaded IRQs preferred for new drivers?** They are compatible with `PREEMPT_RT` (where spinlocks become sleeping locks, making traditional hardirq ISRs unsafe), their priority is tunable via `chrt`, and they allow the actual driver work to use the full kernel API including sleeping functions.
- **What is a page fault and when is it expensive?** A page fault occurs when a virtual address has no valid physical mapping. Minor faults (page not yet allocated) complete in microseconds. Major faults (data must be read from disk) take milliseconds. For RT inference threads, even minor faults are unacceptable — `mlockall()` prevents both.
- **What does MSI-X provide that legacy IRQs cannot?** Per-vector CPU affinity: each MSI-X vector can be independently pinned to a specific CPU. This means a 32-queue NVMe drive can have each queue's completion interrupt wake the exact CPU thread that submitted the I/O, eliminating cross-core lock contention.
- **Why does NAPI coalesce interrupts instead of using one interrupt per packet?** At 100 Gbit/s, a NIC could generate ~148 million interrupts per second for 64-byte packets. Each interrupt forces a mode switch and cache effects. NAPI batches packet processing into a single `poll()` call per interrupt, reducing overhead to a manageable level at the cost of slightly increased latency for the first packet.

---

**AI Hardware Connection**

- GPU MSI-X per-engine completion vectors enable async CUDA stream operation; pinning each vector to the CUDA stream management core eliminates 10–50 µs cross-core wakeup overhead in inference pipelines.
- Camera frame-done ISR → threaded IRQ path (or workqueue) sets the end-to-end latency floor for `camerad`; measuring it with `bpftrace tracepoint:irq:irq_handler_entry` directly quantifies camera-to-model input delay in openpilot.
- NVMe per-queue MSI-X is prerequisite for GPUDirect Storage: each NVMe completion must wake the DMA engine on the correct NUMA node without cross-node memory traffic — requires both MSI-X and CPU affinity to be configured.
- Threaded IRQs are mandatory under PREEMPT_RT (Linux 6.12 mainline): camera and GPU completion handlers become schedulable threads, bounding their latency via the RT scheduler rather than interrupt-disable windows.
- `/proc/irq/N/smp_affinity` is the primary control point for IRQ isolation on AV platforms — GPU, NVMe, and camera IRQs are moved off RT cores to prevent top-half execution interfering with `modeld` and `controlsd` deadlines.
- FPGA AXI-stream DMA completion interrupts on Zynq/MPSoC follow the same top-half/workqueue pattern: the ISR acknowledges the CDMA, queues a work item that processes the result buffer and signals the inference thread via a wait queue.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
