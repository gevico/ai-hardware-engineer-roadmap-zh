---
title: 操作系统 — 期末练习（5 道题）
description: 操作系统 — 期末练习（5 道题）
---

# 操作系统 — 期末练习（5 道题）

与 [Operating Systems — Guide](Guide.md) 以及下方阶段 1 讲义配套的简答题练习。难度：**中等**。**每道编号题都是一个单一问题**（除特别说明外，写一段简短文字作答）。

**建议用时：** 总计约 45–60 分钟。

---

## 问题 1 — 构建自定义内核

**概述如何为目标平台（嵌入式 AI 板卡或通用源码树）构建自定义 Linux 内核**，并**列出过程中使用的典型工具或工作流**——例如**配置**发生在哪里、**交叉编译**如何融入其中，以及至少一种**发行版式**的做法（不要只说“在笔记本上为 x86 运行 `make`”）。

*对应：[Lecture 25 — Capstone: custom Linux images with Yocto](01-讲座/Lecture-25.md)（工厂工作流）；[Lecture 5](01-讲座/Lecture-05.md) / 厂商 BSP 背景下的 **defconfig**、**DTB** 和烧录。*

---

## 问题 2 — PREEMPT_RT（实时 Linux）

**`PREEMPT_RT` 是什么，它解决“普通” Linux 上的什么问题，以及它在 kernel 层面大致改了什么**（足以表明你理解*可预测性*与原始速度的区别）？

*对应：[Lecture 7 — Real-Time Linux: PREEMPT_RT & Determinism](01-讲座/Lecture-07.md)。*

---

## 问题 3 — ext4

**ext4 文件系统是什么，它对典型 Linux 根文件系统 / 嵌入式板卡的主要优势有哪些**（列出讲义中若干具体优点——分配、目录、日志/崩溃行为、成熟度等）？

*对应：[Lecture 21 — Filesystems: ext4, btrfs, F2FS & overlayfs](01-讲座/Lecture-21.md)。*

---

## 问题 4 — 启动顺序

**对于 ARM SoC 风格的嵌入式启动流程（如讲义所述），按顺序列出从上电到 PID 1 的启动链**，命名**至少六个**明确的阶段（顺序必须正确）。

*对应：[Lecture 5 — Kernel Modules, Boot Process & Device Tree](01-讲座/Lecture-05.md)。*

---

## 问题 5 — 内核模块与设备驱动

**什么是可加载内核模块，对于由 Device Tree 匹配的驱动，driver bring-up 通常如何工作**——包括 **`compatible` 匹配**、**`probe()`**，以及为什么通常用 **`modprobe`** 而不是 **`insmod`**？

*对应：[Lecture 5 — Kernel Modules, Boot Process & Device Tree](01-讲座/Lecture-05.md)。*

---

## 答案（用于自查）

<details>
<summary>点击展开</summary>

**1.** **典型流程：** 获取 **kernel 源码**（mainline、厂商/L4T BSP，或通过构建系统的 recipe）→ 选择 **config**（**`defconfig`**、**`make menuconfig` / nconfig**，或 Yocto **`bbappend`** 中的 **fragments** / `CONFIG_*`）→ 用与目标匹配的**交叉工具链**构建（**`aarch64-linux-gnu-*`** 等），或让 **Yocto/BitBake** 在 recipe 内调用 **`make`** → 产出 **`Image`/`zImage`**、**模块**，并匹配 **DTB** / 启动产物 → 部署（烧录、OTA，或替换 `/boot`）。**工具 / 技术栈（任何自洽的组合都可得分）：** **Yocto**（**Poky**、**BitBake**、**meta-tegra** / machine layer）、可选的 **`make` + `LLVM=`**、作为替代嵌入式集成器的 **Buildroot**、用于已有内核上**树外**模块的 **DKMS**、用于打包的 **`installkernel` / `modules_install`**。

**2.** **`PREEMPT_RT`** 针对的是**有界的最坏情况延迟**（硬/软实时**可预测性**），而不是更高的平均吞吐。它让几乎整个 kernel 变为**可抢占**：把许多 **spinlock 变成可睡眠的 rtmutex**，把 **IRQ 处理**移入**可调度的线程**，并让 **softirq** 工作**可抢占**——从而缩短那些会拖延高优先级任务的长不可抢占区段。

**3.** **ext4** 是 **VFS** 之下、**块层**之上的默认 **ext 系列**（ext2/3/4）**日志式** Linux 磁盘文件系统。**优点**（示例）：**基于 extent** 的分配与 **64 位**卷；**延迟分配**带来更好的局部性；用于大目录的 **dir_index**（htree）；**日志**以及 **`data=ordered`** 之类的模式带来合理的崩溃恢复；成熟，广泛用于 **rootfs**（如 Jetson）。

**4.** 示例顺序：**BootROM** → **SPL / 一级 bootloader**（DRAM bring-up）→ **TF-A BL31 / PSCI**（secure monitor）→ **U-Boot 或 UEFI**（加载 kernel + **DTB** + initramfs）→ **Linux 入口**（解压 / 早期启动）→ **`start_kernel()`** 与驱动探测 → **PID 1**（`systemd` / `init`）。

**5.** **模块**是一段 **`.ko`**，被链接进**正在运行**的 kernel（额外的驱动/代码），**无需**重启。对于 **DT** 设备，**`MODULE_DEVICE_TABLE` / `compatible`** 匹配到一个节点；**`modprobe`** 加载该模块（及其**依赖**）；当设备**被绑定**时（启动或热插拔），内核调用 **`probe()`**。之所以优先用 **`modprobe`** 而不是 **`insmod`**，是因为它依据 **`modules.dep`** **解析依赖顺序**。

</details>


<details>
<summary>English original</summary>

**Operating Systems — Final Practice (5 problems)**

Short-answer practice aligned with [Operating Systems — Guide](Guide.md) and the Phase 1 lecture notes below. Difficulty: **moderate**. **Each numbered problem is a single question** (write a short paragraph unless noted).

**Suggested time:** ~45–60 minutes total.

---

**Problem 1 — Building a custom kernel**

**Outline how you build a custom Linux kernel** for a target (embedded AI board or generic tree), and **name typical tools or workflows** used along the way — e.g. where **configuration** happens, how **cross-compilation** fits in, and at least one **distribution-style** approach (not only “run `make` on your laptop for x86”).

*Maps to: [Lecture 25 — Capstone: custom Linux images with Yocto](01-讲座/Lecture-25.md) (factory workflow); [Lecture 5](01-讲座/Lecture-05.md) / vendor BSP context for **defconfig**, **DTB**, and flashing.*

---

**Problem 2 — PREEMPT_RT (real-time Linux)**

**What is `PREEMPT_RT`, what problem on “normal” Linux does it address, and what does it change in the kernel at a high level** (enough to show you understand *predictability* vs raw speed)?

*Maps to: [Lecture 7 — Real-Time Linux: PREEMPT_RT & Determinism](01-讲座/Lecture-07.md).*

---

**Problem 3 — ext4**

**What is the ext4 filesystem, and what are its main strengths** for typical Linux roots / embedded boards (name several concrete pros from the lecture — allocation, directories, journaling/crash behavior, maturity, etc.)?

*Maps to: [Lecture 21 — Filesystems: ext4, btrfs, F2FS & overlayfs](01-讲座/Lecture-21.md).*

---

**Problem 4 — Boot order**

**For an ARM SoC–style embedded boot flow (as in the lecture), list the boot chain in order from power-on to PID 1**, naming **at least six** clear stages (order must be correct).

*Maps to: [Lecture 5 — Kernel Modules, Boot Process & Device Tree](01-讲座/Lecture-05.md).*

---

**Problem 5 — Kernel modules and device drivers**

**What is a loadable kernel module, and how does driver bring-up typically work** for a **Device Tree–matched** driver — including **`compatible` matching**, **`probe()`**, and why **`modprobe`** is usually used instead of **`insmod`**?

*Maps to: [Lecture 5 — Kernel Modules, Boot Process & Device Tree](01-讲座/Lecture-05.md).*

---

**Answer key (for self-check)**

<details>
<summary>Click to expand</summary>

**1.** **Typical flow:** obtain **kernel source** (mainline, vendor/L4T BSP, or via a build system recipe) → choose **config** (**`defconfig`**, **`make menuconfig` / nconfig**, or **fragments** / `CONFIG_*` in Yocto **`bbappend`**) → build with a **cross-toolchain** matching the target (**`aarch64-linux-gnu-*`** etc.) or let **Yocto/BitBake** invoke **`make`** inside the recipe → produce **`Image`/`zImage`**, **modules**, and match **DTB** / boot artifacts → deploy (flash, OTA, or replace `/boot`). **Tools / stacks (credit any coherent set):** **Yocto** (**Poky**, **BitBake**, **meta-tegra** / machine layers), **`make` + `LLVM=` optional**, **Buildroot** as an alternative embedded integrator, **DKMS** for **out-of-tree** modules on an existing kernel, **`installkernel` / `modules_install`** for packaging.

**2.** **`PREEMPT_RT`** targets **bounded worst-case latency** (hard/soft RT **predictability**), not higher average throughput. It makes almost the whole kernel **preemptible** by turning many **spinlocks into sleeping rtmutexes**, moving **IRQ handling** into **schedulable threads**, and making **softirq** work **preemptible** — shrinking long non-preemptible sections that delay high-priority tasks.

**3.** **ext4** is the default **ext family** (ext2/3/4) **journaled** Linux disk FS under **VFS**, above the **block layer**. **Pros** (examples): **extent-based** allocation and **64-bit** volumes; **delayed allocation** for better locality; **dir_index** (htree) for large directories; **journaling** and modes like **`data=ordered`** for sensible crash recovery; mature and widely used on **rootfs** (e.g. Jetson).

**4.** Example order: **BootROM** → **SPL / primary bootloader** (DRAM bring-up) → **TF-A BL31 / PSCI** (secure monitor) → **U-Boot or UEFI** (loads kernel + **DTB** + initramfs) → **Linux entry** (decompression / early boot) → **`start_kernel()`** and driver probing → **PID 1** (`systemd` / `init`).

**5.** A **module** is a **`.ko`** linked into the **running** kernel (extra drivers/code) **without** reboot. For **DT** devices, **`MODULE_DEVICE_TABLE` / `compatible`** matches a node; **`modprobe`** loads the module (and **dependencies**); the kernel calls **`probe()`** when the device is **bound** (boot or hotplug). **`modprobe`** is preferred over **`insmod`** because it **resolves dependency order** from **`modules.dep`**.

</details>

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Final-Test-Problems.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Final-Test-Problems.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
