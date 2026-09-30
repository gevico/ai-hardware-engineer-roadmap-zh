---
title: 讲义 03（L5、L6）：内核模块、启动与设备树；CPU 调度（CFS、EEVDF 与 RT）
description: 讲义 03（L5、L6）：内核模块、启动与设备树；CPU 调度（CFS、EEVDF 与 RT）
---

# 讲义 03（L5、L6）：内核模块、启动与设备树；CPU 调度（CFS、EEVDF 与 RT）

**合并：** 讲座 L5（内核模块、启动过程与设备树）与讲座 L6（CPU 调度：CFS、EEVDF 与实时类）。

---

## 本讲义的组织方式

1. **第 1 部分（L5）——启动与设备树：** ARM SoC 启动链；安全启动；UEFI 与 U-Boot；initramfs；设备树（DTS/DTB、compatible、reg、interrupts）；驱动绑定。
2. **第 2 部分（L6）——CPU 调度：** 调度器类层次（stop、dl、rt、fair、idle）；CFS vruntime 与 nice；EEVDF（eligible、虚拟截止时间）；SCHED_FIFO/SCHED_RR/SCHED_DEADLINE；何时使用 RT 类。

---

# 第 1 部分（L5）：内核模块、启动过程与设备树

**背景：** 在推理运行之前，bootloader 加载 kernel + DTB；kernel 解析**设备树**以发现 SoC 外设；驱动通过 **compatible 字符串**绑定。同一驱动二进制文件通过**不同的 DT 支持多个板卡**。

---

## Linux 启动序列（ARM SoC）

上电 → BootROM → SPL（DRAM、时钟） → TF-A BL31（EL3） → U-Boot/CBoot（kernel + DTB + initramfs） → kernel 解压 → start_kernel() → init（systemd）。Jetson：用 MB1、MB2、CBoot 代替 SPL/U-Boot。**信任链：** 每个阶段验证下一阶段的签名。

---

## 安全启动、UEFI 与 U-Boot

安全启动：ROM 验证 SPL；SPL 验证 kernel；kernel 可验证 rootfs/模块。UEFI：x86 及部分 ARM 服务器；EFI stub，DTB 来自配置。U-Boot：嵌入式（Zynq、Jetson、i.MX）；在寄存器中传递 kernel + DTB；环境变量、boot.cmd。

---

## initramfs

压缩的 cpio；**早期用户空间**（busybox、udev、cryptsetup、init）。Kernel 将其挂载为 root；运行 /init；然后切换到真正的 root 并执行 switch_root。当**根文件系统需要 kernel 中没有的驱动或工具**时（如 LUKS）需要。Jetson：initramfs 中的 TNSPEC 和 extlinux。

---

## 设备树（DTS → DTB）

**目的：** 描述 SoC 硬件（地址、IRQ、时钟），无需在 kernel 中硬编码。DTS 源文件 → dtc → DTB；bootloader 传递 DTB 物理地址（如 ARM64 x0）。Kernel 解析并创建 platform_device 节点；**compatible** 字符串绑定到驱动的 of_match_table。

**节点：** compatible、reg（地址/大小）、interrupts、clocks、status（"okay"/"disabled"）。驱动在 probe 中通过 of_iomap、of_irq_get、of_get_property 获取资源。MODULE_DEVICE_TABLE(of, ...) 用于匹配时 udev 加载。

---

# 第 2 部分（L6）：CPU 调度——CFS、EEVDF 与实时类

**背景：** 调度器决定下一个运行哪个任务。**实时类（dl、rt）在公平类之前检查**（CFS/EEVDF）。一个优先级为 1 的 SCHED_FIFO 任务抢占所有 SCHED_NORMAL 任务。对于截止时间（如推理、CAN），使用 **SCHED_FIFO 或 SCHED_DEADLINE**。

---

## 调度器类层次

（从高到低）**stop_sched_class**（内部） → **dl_sched_class**（SCHED_DEADLINE、CBS/EDF） → **rt_sched_class**（SCHED_FIFO、SCHED_RR、优先级 1–99） → **fair_sched_class**（SCHED_NORMAL、CFS/EEVDF） → **idle_sched_class**。更高类总是抢占更低类；不可覆盖。

---

## CFS（完全公平调度器，&lt; 6.6）

理想 CPU 速度为 1/N；**vruntime** = 加权 runtime；调度器选择最小的 vruntime（红黑树）。**Nice** 改变权重（例如 nice -5 ≈ 3 倍于 nice 0 的份额）。**弱点：** 新唤醒的任务最多可能等待 sched_latency_ns（例如 6 ms），如果其他任务的 vruntime 更低——对唤醒延迟不利。

---

## EEVDF（Linux 6.6+）

取代 CFS。**lag** = 欠付的 CPU 时间与理想值之比；**eligible** = 未超过公平份额；**虚拟截止时间** = 下一个时间片到期的时间。在 **eligible** 任务中，运行**最早虚拟截止时间**。具有正 lag 的唤醒任务比在 CFS 下调度得更快——对推理有更好的尾部延迟。

---

## 实时类

**SCHED_FIFO：** 优先级 1–99；一直运行直到让出或被更高优先级抢占；无时间片。**SCHED_RR：** 相同，但在优先级内轮转。**SCHED_DEADLINE：** Runtime、截止时间、周期（CBS/EDF）。用于 controlsd、modeld、传感器循环。CFS/EEVDF 用于一般工作；将推理/控制设置为 SCHED_FIFO 或 DEADLINE 以避免 6 ms 唤醒延迟。

---

## 调优与检查

sched_latency_ns、sched_min_granularity_ns（CFS）。/proc/[pid]/sched（vruntime、时间片）。**chrt** 用于策略/优先级。对于**确定性的延迟**，使用 RT 类 + isolcpus/PREEMPT_RT（见讲义 04）。

---

## 总结

| L5 | 启动链；安全启动；UEFI 与 U-Boot；initramfs；设备树（compatible、reg、interrupts）；驱动绑定。 |
| L6 | 调度器类（dl、rt、fair、idle）；CFS vruntime/nice；EEVDF 资格/截止时间；用于 RT 的 SCHED_FIFO/RR/DEADLINE。 |

---


<details>
<summary>English original</summary>

**Lecture Note 03 (L5, L6): Kernel Modules, Boot & Device Tree; CPU Scheduling (CFS, EEVDF & RT)**

**Combines:** Lecture L5 (Kernel Modules, Boot Process & Device Tree) and Lecture L6 (CPU Scheduling: CFS, EEVDF & Real-Time Classes).

---

**How This Note Is Organized**

1. **Part 1 (L5) — Boot & Device Tree:** ARM SoC boot chain; secure boot; UEFI vs U-Boot; initramfs; Device Tree (DTS/DTB, compatible, reg, interrupts); driver binding.
2. **Part 2 (L6) — CPU scheduling:** Scheduler class hierarchy (stop, dl, rt, fair, idle); CFS vruntime and nice; EEVDF (eligibility, virtual deadline); SCHED_FIFO/SCHED_RR/SCHED_DEADLINE; when to use RT classes.

---

**Part 1 (L5): Kernel Modules, Boot Process & Device Tree**

**Context:** Before inference runs, bootloader loads kernel + DTB; kernel parses **Device Tree** to discover SoC peripherals; drivers bind via **compatible strings**. Same driver binary supports **multiple boards via different DTs**.

---

**Linux Boot Sequence (ARM SoC)**

Power-on → BootROM → SPL (DRAM, clocks) → TF-A BL31 (EL3) → U-Boot/CBoot (kernel + DTB + initramfs) → kernel decompress → start_kernel() → init (systemd). Jetson: MB1, MB2, CBoot in place of SPL/U-Boot. **Chain of trust:** each stage verifies next signature.

---

**Secure Boot, UEFI vs U-Boot**

Secure boot: ROM verifies SPL; SPL verifies kernel; kernel can verify rootfs/modules. UEFI: x86 and some ARM servers; EFI stub, DTB from config. U-Boot: embedded (Zynq, Jetson, i.MX); passes kernel + DTB in registers; env vars, boot.cmd.

---

**initramfs**

Compressed cpio; **early userspace** (busybox, udev, cryptsetup, init). Kernel mounts as root; runs /init; then real root and switch_root. Needed when **root needs drivers or tools not in kernel** (e.g. LUKS). Jetson: TNSPEC and extlinux in initramfs.

---

**Device Tree (DTS → DTB)**

**Purpose:** Describe SoC hardware (addresses, IRQs, clocks) without hardcoding in kernel. DTS source → dtc → DTB; bootloader passes DTB physical address (e.g. ARM64 x0). Kernel parses and creates platform_device nodes; **compatible** string binds to driver’s of_match_table.

**Node:** compatible, reg (address/size), interrupts, clocks, status ("okay"/"disabled"). Driver gets resources in probe via of_iomap, of_irq_get, of_get_property. MODULE_DEVICE_TABLE(of, ...) for udev load on match.

---

**Part 2 (L6): CPU Scheduling — CFS, EEVDF & Real-Time Classes**

**Context:** Scheduler decides which task runs next. **Real-time classes (dl, rt) are checked before the fair class** (CFS/EEVDF). One SCHED_FIFO task at priority 1 preempts all SCHED_NORMAL tasks. For deadlines (e.g. inference, CAN), use **SCHED_FIFO or SCHED_DEADLINE**.

---

**Scheduler Class Hierarchy**

(High to low) **stop_sched_class** (internal) → **dl_sched_class** (SCHED_DEADLINE, CBS/EDF) → **rt_sched_class** (SCHED_FIFO, SCHED_RR, priority 1–99) → **fair_sched_class** (SCHED_NORMAL, CFS/EEVDF) → **idle_sched_class**. Higher class always preempts lower; no override.

---

**CFS (Completely Fair Scheduler, &lt; 6.6)**

Ideal CPU at 1/N speed; **vruntime** = weighted runtime; scheduler picks smallest vruntime (red-black tree). **Nice** changes weight (e.g. nice -5 ≈ 3× share of nice 0). **Weakness:** Newly woken task can wait up to sched_latency_ns (e.g. 6 ms) if others have lower vruntime — bad for wakeup latency.

---

**EEVDF (Linux 6.6+)**

Replaces CFS. **lag** = CPU time owed vs ideal; **eligible** = not ahead of fair share; **virtual deadline** = when next slice is due. Among **eligible** tasks, run **earliest virtual deadline**. Woken task with positive lag gets scheduled sooner than under CFS — better tail latency for inference.

---

**Real-Time Classes**

**SCHED_FIFO:** Priority 1–99; runs until yield or preempted by higher priority; no time slice. **SCHED_RR:** Same but with round-robin within priority. **SCHED_DEADLINE:** Runtime, deadline, period (CBS/EDF). Use for controlsd, modeld, sensor loops. CFS/EEVDF for general work; set inference/control to SCHED_FIFO or DEADLINE to avoid 6 ms wakeup delay.

---

**Tuning and Inspection**

sched_latency_ns, sched_min_granularity_ns (CFS). /proc/[pid]/sched (vruntime, slice). **chrt** for policy/priority. For **deterministic latency**, use RT class + isolcpus/PREEMPT_RT (see Lecture-Note 04).

---

**Summary**

| L5 | Boot chain; secure boot; UEFI vs U-Boot; initramfs; Device Tree (compatible, reg, interrupts); driver binding. |
| L6 | Scheduler classes (dl, rt, fair, idle); CFS vruntime/nice; EEVDF eligibility/deadline; SCHED_FIFO/RR/DEADLINE for RT. |

---

</details>

## AI 硬件联系

- L5：DTB 在 Jetson/定制 SoC 上定义 camera、NPU、加速器；一个驱动支持多块板；udev 在 compatible 匹配时加载模块。
- L6：modeld/controlsd 以 SCHED_FIFO 或 DEADLINE 运行以满足帧与 CAN 截止时间；EEVDF 改善 6.6+ 上 fair-class 的尾延迟；CFS 唤醒延迟是需要对推理使用 RT class 的原因。

---

*综合 L5、L6 两讲（内核模块、启动与设备树；CPU 调度：CFS、EEVDF 与实时类）。*


<details>
<summary>English original</summary>

**AI Hardware Connection**

- L5: DTB defines camera, NPU, accelerators on Jetson/custom SoC; one driver supports multiple boards; udev loads module on compatible match.
- L6: modeld/controlsd on SCHED_FIFO or DEADLINE to meet frame and CAN deadlines; EEVDF improves fair-class tail latency on 6.6+; CFS wakeup latency reason to use RT class for inference.

---

*Combines Lectures L5, L6 (Kernel Modules, Boot & Device Tree; CPU Scheduling: CFS, EEVDF & Real-Time Classes).*

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-Note-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-Note-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
