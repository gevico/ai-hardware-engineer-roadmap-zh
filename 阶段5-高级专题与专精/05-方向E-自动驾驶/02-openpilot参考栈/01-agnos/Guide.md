---
title: AGNOS：用操作系统课程学习
description: AGNOS：用操作系统课程学习
---

# AGNOS：用操作系统课程学习

<div class="course-identity auto-course" style="--course-accent: #0f766e; --course-accent-rgb: 15, 118, 110;" markdown="1">
<div class="course-identity__icon">ALWT</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入剖析 · 专项方向</p>
<p class="course-identity__title">AGNOS：用操作系统课程学习的专项课程标识。</p>
<p class="course-identity__meta">产物：专项案例研究 · 度量：性能、可靠性、岗位匹配度</p>
</div>
</div>


**目标：** 用 [Operating Systems](../../../../阶段1-基础知识/03-操作系统/Guide.md) 课程（阶段 1）理解 AGNOS——那个在 comma 3X 和 comma four 上**在道路上运行 openpilot** 的**经 fork 并定制修改的 Linux**。各讲既对应代码所在的位置，也对应 comma 在该 fork 中为这一实际用例所做的**开发改动**。

---

## AGNOS 是什么：为在路上运行的 openpilot 而 fork 的 Linux + 定制开发

AGNOS **不是**通用 Linux 发行版。它是：

1. **Linux 内核的一个 fork** —— [agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) 基于 Linux 内核（SDM845 的 Android/common 基线），随后由 comma 针对其硬件和 openpilot 做了**定制修改与开发**。
2. **一个定制构建的 OS image** —— [agnos-builder](https://github.com/commaai/agnos-builder) 产出完整栈：该 kernel、启动链（XBL、ABL 等）、设备树、initramfs，以及基于 Ubuntu 的用户空间。
3. **只为一个实际用例而构建** —— **在车里、在道路上运行 openpilot。** 摄像头流水线、CAN 总线、实时控制回路和推理全都依赖这个 OS。fork 中的开发改动（驱动补丁、设备树、启动配置、调度器行为）正是为了让 openpilot 在生产中满足延迟与可靠性要求。

因此，当你学完 OS 课程再来看 AGNOS 时，你看到的是**一个真实团队如何 fork Linux 并改造它**，以支撑一个具体产品（comma 设备上的 openpilot）。下表把每一讲同时对应到两件事：该主题在代码树中**出现在哪里**，以及 AGNOS 中与之相关的是**何种开发改动**。

---


<details>
<summary>English original</summary>

**AGNOS: Learn with the Operating System Course**

<div class="course-identity auto-course" style="--course-accent: #0f766e; --course-accent-rgb: 15, 118, 110;" markdown="1">
<div class="course-identity__icon">ALWT</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for AGNOS: Learn with the Operating System Course.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Goal:** Use the [Operating Systems](../../../../阶段1-基础知识/03-操作系统/Guide.md) course (Phase 1) to understand AGNOS—the **forked and custom-modified Linux** that runs **openpilot on the road** on comma 3X and comma four. The lectures map to both where code lives and to the **development changes** comma made in the fork for this practical use case.

---

**What AGNOS Is: Forked Linux + Custom Development for openpilot on the Road**

AGNOS is **not** a generic Linux distro. It is:

1. **A fork of the Linux kernel** — [agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) is based on the Linux kernel (Android/common baseline for SDM845), then **custom modified and developed** by comma for their hardware and for openpilot.
2. **A custom-built OS image** — [agnos-builder](https://github.com/commaai/agnos-builder) produces the full stack: that kernel, boot chain (XBL, ABL, etc.), device tree, initramfs, and Ubuntu-based userspace.
3. **Built for one practical use case** — **running openpilot in the car, on the road.** Camera pipelines, CAN bus, real-time control loops, and inference all depend on this OS. The development changes in the fork (driver patches, device tree, boot config, scheduler behavior) are there so that openpilot can meet latency and reliability requirements in production.

So when you study the OS lectures and then look at AGNOS, you are looking at **how a real team forked Linux and changed it** to support a specific product (openpilot on comma devices). The table below ties each lecture to both **where** that topic appears in the tree and **what kind of development change** in AGNOS relates to it.

---

</details>

## 按功能划分的 Git 历史（自分叉日期）→ OS 讲义

[agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) 仓库创建于 **2019-08-26**，基于 Qualcomm/Android msm-4.9 (SDM845)。Comma 的改动叠加在其上。下文按 **功能 / 领域**（基于文件夹）对 commit 历史分组，而非按单个 commit。每个领域对应相关的 OS 讲义。

| 功能 / 领域 | 典型路径 | 示例改动（来自 commit 信息） | OS 讲义 |
|------------------|---------------|----------------------------------------|------------|
| **Camera (msm/camera)** | `drivers/media/platform/msm/`, `techpack/` | 暴露 IFE PHY_NUM_SEL，sysfs 上的 workqueue，高优先级 WQ，取消 WQ_UNBOUND；mclk 驱动强度；ICP 使能；Thundercomm camerad 更新；Bantian 调优 | [18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) V4L2，[6](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md) workqueue 优先级 |
| **MIPI / display (dsi)** | `drivers/gpu/drm/msm/dsi-staging/`, `drivers/video/` | MIPI DCS 调试，TE line 初始化，60Hz 抖动，高温下 panel 初始化；brightness sysfs；lcd3v3 稳压器；Mate 10 lite / Tizi / Bantian 显示屏 bringup | [18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) 字符设备驱动 |
| **Touch** | `drivers/input/touchscreen/` | Hynitron，Samsung 克隆，touch count，IRQ 重试，固件烧录器；Mici bringup | [18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) 中断驱动 I/O，[3](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md) IRQ |
| **设备树（DTS）** | `arch/arm64/boot/dts/` | 移除 comma_tici.dts，dts 清理，迁移到设备树；支持 sdm845，sdm v2；Mici dtsi；SDM845 MTP 的 dts 合并 | [5](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md)，[17](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md) |
| **启动 / defconfig** | `arch/arm64/configs/`, `init/` | 精简 tici_defconfig 以缩短启动时间；修复 XBL 支持；修复重启 | [5](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md) |
| **SPI (CAN-over-SPI)** | `drivers/spi/` | spidev bufsiz 8192；spi-geni-qcom delay_usecs | [18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md)，openpilot CAN |
| **存储 / block** | `drivers/scsi/`, `block/`, `fs/` | NVMe APST 回退，NVMe 稳压器；sdcard；jbd2 上游修复；squashfs，cramfs | [20](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-20.md)，[21](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-21.md)，[22](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-22.md) |
| **调度器 / workqueue** | `kernel/sched/`，workqueue 用法 | Camera 高优先级 WQ，取消 WQ_UNBOUND；在 sysfs 上暴露 WQ | [6](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md)，[8](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-08.md) |
| **电源 / 热管理** | `drivers/power/`, `drivers/thermal/` | C4 上的热探针；QPNP_FG_GEN3；CPU 频率 governor 上限；mici 热传感器 | [15](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-15.md) DMA/power |
| **网络 / WiFi** | `net/`, `drivers/net/` | WiFi 日志级别；在主 kernel 中构建 wifi；RNDIS；CONFIG_IFB，NETEM，TTL；MAC 来自 SOC serial | [4](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-04.md) 网络 |
| **Kernel 配置 / cgroups** | `Kconfig`, `kernel/cgroup/` | 为 cgroups 启用内存控制；audio，uart，logitech 模块 | [5](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md)，[23](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-23.md) |
| **平台 / 杂项驱动** | `drivers/`, `arch/arm64/` | SOM id 引脚驱动；USB serial PID；hostname（comma/tici）；nfs | [17](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)，[2](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-02.md) |
| **上游 / Qualcomm 合并** | 各类 | msm-4.9 合并，dts overlay 合并，camera/mdss/ipa 修复 | 基础；多篇讲义 |

*自行查看历史：`git log --oneline -- drivers/media/`（camera），`git log --oneline -- arch/arm64/boot/dts/`（DT）等等。仓库：[agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845)。*

---


<details>
<summary>English original</summary>

**Git History by Function (from Fork Date) → OS Lecture**

The [agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) repo was created **2019-08-26** and is based on Qualcomm/Android msm-4.9 (SDM845). Comma’s changes are layered on top. Below, commit history is grouped by **function / area** (folder-based), not by individual commit. Each area maps to the relevant OS lecture.

| Function / area | Typical paths | Example changes (from commit messages) | OS lecture |
|------------------|---------------|----------------------------------------|------------|
| **Camera (msm/camera)** | `drivers/media/platform/msm/`, `techpack/` | Expose IFE PHY_NUM_SEL, workqueues on sysfs, high-priority WQ, unset WQ_UNBOUND; mclk drive strength; ICP enable; Thundercomm camerad updates; Bantian tuning | [18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) V4L2, [6](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md) workqueue priority |
| **MIPI / display (dsi)** | `drivers/gpu/drm/msm/dsi-staging/`, `drivers/video/` | MIPI DCS debug, TE line init, 60Hz jitter, panel init in heat; brightness sysfs; lcd3v3 regulator; Mate 10 lite / Tizi / Bantian display bringup | [18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) character drivers |
| **Touch** | `drivers/input/touchscreen/` | Hynitron, Samsung clones, touch count, IRQ retry, firmware flasher; Mici bringup | [18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) interrupt-driven I/O, [3](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md) IRQ |
| **Device Tree (DTS)** | `arch/arm64/boot/dts/` | Remove comma_tici.dts, dts cleanup, move to device tree; support sdm845, sdm v2; Mici dtsi; dts merge for SDM845 MTP | [5](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md), [17](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md) |
| **Boot / defconfig** | `arch/arm64/configs/`, `init/` | Slim tici_defconfig for boot time; fix XBL support; fix reboot | [5](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md) |
| **SPI (CAN-over-SPI)** | `drivers/spi/` | spidev bufsiz 8192; spi-geni-qcom delay_usecs | [18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md), openpilot CAN |
| **Storage / block** | `drivers/scsi/`, `block/`, `fs/` | NVMe APST revert, NVMe regulator; sdcard; jbd2 upstream fixes; squashfs, cramfs | [20](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-20.md), [21](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-21.md), [22](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-22.md) |
| **Scheduler / workqueues** | `kernel/sched/`, workqueue usage | Camera high-priority WQ, unset WQ_UNBOUND; expose WQ on sysfs | [6](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md), [8](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-08.md) |
| **Power / thermal** | `drivers/power/`, `drivers/thermal/` | Thermal probes on C4; QPNP_FG_GEN3; CPU freq governor cap; mici thermal sensors | [15](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-15.md) DMA/power |
| **Network / WiFi** | `net/`, `drivers/net/` | WiFi log level; build wifi in main kernel; RNDIS; CONFIG_IFB, NETEM, TTL; MAC from SOC serial | [4](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-04.md) networking |
| **Kernel config / cgroups** | `Kconfig`, `kernel/cgroup/` | enable memory control for cgroups; audio, uart, logitech modules | [5](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md), [23](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-23.md) |
| **Platform / misc drivers** | `drivers/`, `arch/arm64/` | SOM id pins driver; USB serial PID; hostname (comma/tici); nfs | [17](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md), [2](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-02.md) |
| **Upstream / Qualcomm merges** | various | msm-4.9 merges, dts overlay merge, camera/mdss/ipa fixes | Base; many lectures |

*To inspect history yourself: `git log --oneline -- drivers/media/` (camera), `git log --oneline -- arch/arm64/boot/dts/` (DT), etc. Repo: [agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845).*

---

</details>

## 仓库（克隆与学习）

| 仓库 | 用途 | 链接 |
|------|--------|------|
| **agnos-kernel-sdm845** | 面向 SDM845（Snapdragon 845）模块的 Linux 内核。Camera ISP（图像信号处理器）、CAN-over-SPI、电源管理、调度器、驱动。 | [commaai/agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) |
| **agnos-builder** | 构建 AGNOS：内核 + 系统镜像、initramfs、启动产物、用户态。把内核作为 submodule。 | [commaai/agnos-builder](https://github.com/commaai/agnos-builder) |

**本路线图中的本地路径：**

- `../agnos-kernel-sdm845/` — 内核源码
- `../agnos-builder/` — 构建系统、脚本、用户态、固件

### 一次性克隆（若尚未存在）

```bash
cd "Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles"

# Kernel (large history; shallow clone recommended)
git clone --depth 1 https://github.com/commaai/agnos-kernel-sdm845.git

# Builder (smaller)
git clone https://github.com/commaai/agnos-builder.git
cd agnos-builder
git submodule update --init agnos-kernel-sdm845   # if building
./tools/extract_tools.sh                          # if building
```

---

## 如何学习：OS 课程 → AGNOS 仓库

先学习 **Operating Systems** 课程，再把每个主题映射到 AGNOS 的代码与配置。**每一讲（1–26）** 都在下方链接到 **agnos-kernel-sdm845** 与 **agnos-builder** 中的具体路径。

---

<a id="all-os-lectures--agnos-overview"></a>


<details>
<summary>English original</summary>

**Repositories (Clone and Study)**

| Repo | Purpose | Link |
|------|--------|------|
| **agnos-kernel-sdm845** | Linux kernel for SDM845 (Snapdragon 845) modules. Camera ISP, CAN-over-SPI, power management, scheduler, drivers. | [commaai/agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) |
| **agnos-builder** | Builds AGNOS: kernel + system image, initramfs, boot artifacts, userspace. Uses kernel as submodule. | [commaai/agnos-builder](https://github.com/commaai/agnos-builder) |

**Local paths in this roadmap:**

- `../agnos-kernel-sdm845/` — kernel source
- `../agnos-builder/` — build system, scripts, userspace, firmware

**One-time clone (if not already present)**

```bash
cd "Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles"

# Kernel (large history; shallow clone recommended)
git clone --depth 1 https://github.com/commaai/agnos-kernel-sdm845.git

# Builder (smaller)
git clone https://github.com/commaai/agnos-builder.git
cd agnos-builder
git submodule update --init agnos-kernel-sdm845   # if building
./tools/extract_tools.sh                          # if building
```

---

**How to Learn: OS Lectures → AGNOS Repos**

Study the **Operating Systems** lectures first, then map each topic to AGNOS code and config. **Every lecture (1–26)** is linked below to concrete paths in **agnos-kernel-sdm845** and **agnos-builder**.

---

<a id="all-os-lectures--agnos-overview"></a>

</details>

## All OS Lectures → AGNOS (Overview)

<div class="lecture-map" markdown>


<details>
<summary>English original</summary>

**All OS Lectures → AGNOS (Overview)**

<div class="lecture-map" markdown>

</details>

| # | 讲座 | Kernel（agnos-kernel-sdm845） | 构建器（agnos-builder） |
|:-:|--------|-------------------------------|--------------------------|
| 1 | [现代 OS 架构与 Linux 内核](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md) | `arch/`，`kernel/`，`mm/`，`drivers/` — 单体式结构 | — |
| 2 | [进程、task_struct 与 Linux 进程模型](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-02.md) | `kernel/fork.c`，`include/linux/sched.h`（task_struct） | `userspace/` — 由 systemd 启动的进程 |
| 3 | [中断、异常与下半部](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md) | `arch/arm64/kernel/entry.S`，`kernel/irq/`，驱动 IRQ 处理程序 | — |
| 4 | [系统调用、vDSO 与 eBPF](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-04.md) | `arch/arm64/kernel/syscall.c`，`arch/arm64/kernel/vdso/` | — |
| 5 | [内核模块、启动过程与设备树](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md) | `arch/arm64/boot/dts/`，`drivers/`（of_match_table），`init/` | `firmware/`，`build_kernel.sh`，`build_system.sh`，initramfs |
| 6 | [CPU 调度：CFS、EEVDF 与实时类](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md) | `kernel/sched/core.c`，`fair.c`，`rt.c` | 启动 cmdline（isolcpus 等） |
| 7 | [实时 Linux：PREEMPT_RT 与确定性](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-07.md) | `kernel/sched/`，`kernel/locking/`，`Kconfig` 中的抢占配置 | — |
| 8 | [多核调度、CPU 亲和性与 isolcpus](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-08.md) | `kernel/sched/core.c`（亲和性），`arch/arm64/`（CPU 拓扑） | isolcpus 的启动配置；openpilot `set_core_affinity()` |
| 9 | [同步：自旋锁、互斥锁、读写锁](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-09.md) | `kernel/locking/`，`include/linux/spinlock.h`，`mutex.c` | — |
| 10 | [无锁编程：RCU、原子操作](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-10.md) | `kernel/rcu/`，`include/linux/atomic.h` | — |
| 11 | [死锁、优先级反转与 PI 互斥锁](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-11.md) | `kernel/locking/rtmutex.c`，调度器中的 PI | — |
| 12 | [虚拟内存与 Linux 内存模型](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-12.md) | `mm/`（vm_area、页表），`arch/arm64/mm/` | — |
| 13 | [页表、TLB 与大页](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-13.md) | `arch/arm64/mm/`，`mm/memory.c`，大页支持 | — |
| 14 | [内存分配：SLUB、kmalloc 与 CMA](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-14.md) | `mm/slub.c`，`mm/cma.c`，`include/linux/slab.h` | — |
| 15 | [DMA、IOMMU 与 GPU 内存管理](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-15.md) | `drivers/iommu/`，`drivers/base/dma-mapping.c`，`drivers/` 中的 GPU | — |
| 16 | [NUMA 拓扑与高性能计算（HPC）内存优化](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-16.md) | `arch/arm64/mm/`，若启用则 NUMA；SDM845 是 UMA | — |
| 17 | [Linux 设备驱动模型与设备树](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md) | `drivers/base/`，`arch/arm64/boot/dts/`，platform_driver，of_* | — |
| 18 | [字符驱动、中断驱动 I/O 与 V4L2](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) | `drivers/media/`，comma 3X 的 V4L2 摄像头流水线 | — |
| 19 | [现代 I/O：io_uring、DMA-BUF 与零拷贝](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-19.md) | `io_uring/`，`drivers/dma-buf/` 中的 DMA-BUF | VisionIpc / openpilot 上层的零拷贝 |
| 20 | [PCIe、NVMe 与 GPU 驱动架构](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-20.md) | `drivers/pci/`，`drivers/nvme/`，`drivers/` 下的 GPU（SDM845 上的 Adreno） | — |
| 21 | [文件系统：ext4、btrfs、F2FS 与 overlayfs](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-21.md) | `fs/ext4/`，`fs/overlayfs/`（常见于 Android/AGNOS rootfs） | `userspace/` rootfs 布局；build_system 打包 fs |
| 22 | [嵌入式存储：eMMC、UFS、NVMe 与 OTA](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-22.md) | `drivers/scsi/`，用于 SDM845 存储的 UFS，块层 | `firmware/`，分区布局，构建器中的 A/B 槽位 |
| 23 | [容器、cgroups v2 与 NVIDIA Container](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-23.md) | `kernel/cgroup/` | 可选：AGNOS 用户空间可使用 cgroups 进行隔离 |
| 24 | [面向 AI 系统的 OS：L4T、openpilot OS 与 RT 调优](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-24.md) | 整个 kernel 作为 “openpilot OS”（Agnos）；调度器、驱动、RT | agnos-builder = 为 “openpilot OS” 构建；config 中的 RT 调优 |
| 25 | [顶点项目：使用 Yocto 的自定义 Linux 镜像](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-25.md) | —（Agnos 使用自己的构建器，而非 Yocto） | **agnos-builder** 是顶点项目：自定义镜像构建（kernel + rootfs） |
| 26 | [eBPF：可编程 kernel 可观测性](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-26.md) | `kernel/bpf/`，`net/bpf/`（若在 config 中启用） | 通过 eBPF 工具实现 AGNOS 上 openpilot 的可观测性 |


<details>
<summary>English original</summary>

| # | Lecture | Kernel (agnos-kernel-sdm845) | Builder (agnos-builder) |
|:-:|--------|-------------------------------|--------------------------|
| 1 | [Modern OS Architecture & the Linux Kernel](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md) | `arch/`, `kernel/`, `mm/`, `drivers/` — monolithic layout | — |
| 2 | [Processes, task_struct & the Linux Process Model](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-02.md) | `kernel/fork.c`, `include/linux/sched.h` (task_struct) | `userspace/` — processes started by systemd |
| 3 | [Interrupts, Exceptions & Bottom Halves](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md) | `arch/arm64/kernel/entry.S`, `kernel/irq/`, driver IRQ handlers | — |
| 4 | [System Calls, vDSO & eBPF](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-04.md) | `arch/arm64/kernel/syscall.c`, `arch/arm64/kernel/vdso/` | — |
| 5 | [Kernel Modules, Boot Process & Device Tree](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md) | `arch/arm64/boot/dts/`, `drivers/` (of_match_table), `init/` | `firmware/`, `build_kernel.sh`, `build_system.sh`, initramfs |
| 6 | [CPU Scheduling: CFS, EEVDF & Real-Time Classes](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md) | `kernel/sched/core.c`, `fair.c`, `rt.c` | Boot cmdline (isolcpus, etc.) |
| 7 | [Real-Time Linux: PREEMPT_RT & Determinism](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-07.md) | `kernel/sched/`, `kernel/locking/`, preempt config in `Kconfig` | — |
| 8 | [Multi-Core Scheduling, CPU Affinity & isolcpus](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-08.md) | `kernel/sched/core.c` (affinity), `arch/arm64/` (CPU topology) | Boot config for isolcpus; openpilot `set_core_affinity()` |
| 9 | [Synchronization: Spinlocks, Mutexes, RW Locks](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-09.md) | `kernel/locking/`, `include/linux/spinlock.h`, `mutex.c` | — |
| 10 | [Lock-Free Programming: RCU, Atomics](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-10.md) | `kernel/rcu/`, `include/linux/atomic.h` | — |
| 11 | [Deadlock, Priority Inversion & PI Mutexes](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-11.md) | `kernel/locking/rtmutex.c`, PI in scheduler | — |
| 12 | [Virtual Memory & the Linux Memory Model](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-12.md) | `mm/` (vm_area, page tables), `arch/arm64/mm/` | — |
| 13 | [Page Tables, TLBs & Huge Pages](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-13.md) | `arch/arm64/mm/`, `mm/memory.c`, huge page support | — |
| 14 | [Memory Allocation: SLUB, kmalloc & CMA](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-14.md) | `mm/slub.c`, `mm/cma.c`, `include/linux/slab.h` | — |
| 15 | [DMA, IOMMU & GPU Memory Management](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-15.md) | `drivers/iommu/`, `drivers/base/dma-mapping.c`, GPU in `drivers/` | — |
| 16 | [NUMA Topology & HPC Memory Optimization](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-16.md) | `arch/arm64/mm/`, NUMA if enabled; SDM845 is UMA | — |
| 17 | [Linux Device Driver Model & Device Tree](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md) | `drivers/base/`, `arch/arm64/boot/dts/`, platform_driver, of_* | — |
| 18 | [Character Drivers, Interrupt-Driven I/O & V4L2](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md) | `drivers/media/`, V4L2 camera pipeline for comma 3X | — |
| 19 | [Modern I/O: io_uring, DMA-BUF & Zero-Copy](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-19.md) | `io_uring/`, DMA-BUF in `drivers/dma-buf/` | VisionIpc / zero-copy in openpilot on top |
| 20 | [PCIe, NVMe & GPU Driver Architecture](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-20.md) | `drivers/pci/`, `drivers/nvme/`, GPU under `drivers/` (Adreno on SDM845) | — |
| 21 | [Filesystems: ext4, btrfs, F2FS & overlayfs](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-21.md) | `fs/ext4/`, `fs/overlayfs/` (often in Android/AGNOS rootfs) | `userspace/` rootfs layout; build_system packs fs |
| 22 | [Embedded Storage: eMMC, UFS, NVMe & OTA](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-22.md) | `drivers/scsi/`, UFS for SDM845 storage, block layer | `firmware/`, partition layout, A/B slots in builder |
| 23 | [Containers, cgroups v2 & NVIDIA Container](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-23.md) | `kernel/cgroup/` | Optional: AGNOS userspace can use cgroups for isolation |
| 24 | [OS for AI Systems: L4T, openpilot OS & RT Tuning](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-24.md) | Entire kernel as “openpilot OS” (Agnos); sched, drivers, RT | agnos-builder = build for “openpilot OS”; RT tuning in config |
| 25 | [Capstone: Custom Linux Images with Yocto](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-25.md) | — (Agnos uses its own builder, not Yocto) | **agnos-builder** is the capstone: custom image build (kernel + rootfs) |
| 26 | [eBPF: Programmable Kernel Observability](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-26.md) | `kernel/bpf/`, `net/bpf/` (if enabled in config) | Observability of openpilot on AGNOS via eBPF tools |

</details>

</div>

---

<a id="os-lectures--agnos-development-changes-fork--custom-mods-for-openpilot"></a>

## OS 课程 ↔ AGNOS 开发改动（为 openpilot 做 fork + 定制修改）

AGNOS 中的 kernel **fork 自 Linux 并做了定制修改**。下表把每一节 OS 课程与 comma 在该 fork 中为**车载 openpilot** 用例所做的**那类开发改动**对应起来。用它可以看出每个 OS 主题在实践中为何重要。

<div class="lecture-map" markdown>


<details>
<summary>English original</summary>

**OS Lectures ↔ AGNOS Development Changes (Fork + Custom Mods for openpilot)**

The kernel in AGNOS is **forked from Linux and custom modified**. The table below connects each OS lecture to the **kind of development change** comma made in that fork for the **on-the-road openpilot** use case. Use it to see why each OS topic matters in practice.

<div class="lecture-map" markdown>

</details>

| # | 课程主题 | AGNOS 中的开发变更（fork / 定制工作） | 对 openpilot 上路的意义 |
|:-:|----------------|--------------------------------------------------|------------------------------------------|
| 1 | OS 架构、宏内核 | fork 保留 Linux 的宏内核布局；`arch/arm64/` 中的**平台相关代码**，以及为 SDM845 和 comma 3X 硬件增补或打补丁的**厂商驱动**（camera ISP、CAN、GPU）。 | 单一 kernel 镜像同时驱动 camera、CAN 和 GPU；无额外 IPC 开销。上游变更可能破坏 ABI——comma **固定 kernel 版本**，以保证 camerad 和 controlsd 所用的 V4L2/ISP 与 SocketCAN 稳定。 |
| 2 | 进程、task_struct、fork/exec | kernel 进程模型不变；**agnos-builder** 定义**哪些进程运行**（systemd、openpilot 的 camerad、modeld、controlsd）以及它们的启动方式。 | openpilot 的多进程设计（camerad → modeld → plannerd → controlsd）依赖于此；崩溃隔离与 COW fork 都出自同一进程模型。 |
| 3 | 中断、异常、下半部 | **驱动变更**：camera 与 CAN 驱动注册 IRQ 和下半部路径；**这些路径的延迟**直接影响帧采集和 CAN 写入时序。comma 的补丁使中断与 softirq 处理保持可预测。 | 高帧率 camera 和 100 Hz CAN 需要中断延迟有上界；`/proc/interrupts` 和 IRQ 亲和性对调优很关键。 |
| 4 | 系统调用、vDSO | 系统调用与 vDSO 代码基本来自上游；**无重大 fork 专属变更**。openpilot 用 vDSO `clock_gettime` 获取传感器和模型时间戳。 | 为融合与日志提供稳定、低开销的时间戳，且无系统调用开销。 |
| 5 | 启动、设备树、内核模块 | **大量开发**：(1) **启动链**——agnos-builder 提供固件（XBL、ABL）、kernel、DTB、initrd；(2) **设备树**——comma 3X/four 的 DTS（camera、SPI、CAN、GPIO）；(3) **驱动 probe**——增补或打补丁 camera ISP、CAN-over-SPI、电源管理驱动，并通过 DT 中的 `compatible` 完成匹配。 | camera 和 CAN 要能被识别，正确的启动与 DT 是前提；DT 错误或驱动缺失 = openpilot 无法上路。 |
| 6 | CPU 调度（CFS、SCHED_FIFO） | **配置与用法**：kernel 具备 CFS/RT 调度类；**启动 cmdline**（如 isolcpus）以及 **openpilot** 对 `set_realtime_priority()` 和 `set_core_affinity()` 的使用（位于 openpilot 仓库）。comma 可能针对 RT 工作负载调整默认调度器或 cmdline。 | controlsd 和 modeld 必须满足截止时间；SCHED_FIFO 与亲和性可避免 CFS 抖动导致 CAN 帧遗漏或丢帧。 |
| 7 | 实时（PREEMPT_RT、确定性） | **kernel 配置**：为低延迟选择抢占与锁选项；fork 中可能合入 PREEMPT_RT 或相关补丁以获得确定性响应。 | controlsd 的 CAN 写入与推理循环需要延迟有上界；RT 配置是“openpilot 上路”可靠性的一部分。 |
| 8 | 多核、亲和性、isolcpus | **启动与用户态**：**agnos-builder** 或设备配置设置 **isolcpus**（及相关 cmdline）；openpilot 的 **set_core_affinity()** 将 camerad/modeld/controlsd 绑定到指定核。fork 可能包含调度器/亲和性修复。 | 隔离核并绑定 RT 进程可避免其他工作的干扰，降低尾延迟。 |
| 9 | 同步（自旋锁、互斥锁） | **驱动与核心代码**：任何**新增或修改的驱动**（camera、CAN、SPI）都会用到 kernel 锁原语；fork 中的锁设计影响争用与延迟。 | 驱动中糟糕的锁实现会导致停顿或优先级反转；PI mutex（第 11 讲）对 openpilot 高优先级控制与较低优先级读取者共存的场景很关键。 |
| 10 | RCU、无锁、原子操作 | **kernel 核心**使用 RCU 和原子操作；fork 可能为 SDM845 回移植或调优。没有 openpilot 专属的 RCU 变更，但隔离核上的 **rcu_nocbs**（cmdline）会把 RCU 移出 RT 核。 | 运行控制与推理的核上抖动更小。 |
| 11 | 死锁、优先级反转、PI mutex | **kernel** 具备 rtmutex 与 PI；**openpilot** 中存在共享状态的高、低优先级进程（如 cereal）。fork 保留 PI mutex 支持，使高优先级控制不被低优先级读取者阻塞。 | 避免生产环境中出现“Mars Pathfinder”式的优先级反转。 |
| 12 | 虚拟内存、COW | **核心 mm/** 与 **arch/arm64/mm**；fork 保留标准 VM 和 COW。**openpilot** 由此受益：进程隔离，以及 modeld/camerad 的 COW fork 不会使 RAM 翻倍。 | 为多进程 openpilot 和大模型映射提供稳定的 VM 模型。 |
| 13–14 | 页表、TLB、SLUB、CMA | **mm/** 与 **arch/arm64/mm**；**CMA** 对嵌入式 SoC 上的 **camera 与 DMA 缓冲区**很重要——fork 可能为 SDM845 启用或调优 CMA。 | camera 流水线和零拷贝缓冲区依赖连续内存分配与 slab 分配。 |
| 15 | DMA、IOMMU、GPU 内存 | **驱动与 SoC 支持**：fork 中的 DMA 与 GPU（Adreno）驱动；如启用则含 IOMMU。**缓冲区共享**（camera ↔ GPU ↔ display）与平台相关。 | camera → GPU 推理与显示无需额外拷贝。 |
| 16 | NUMA | SDM845 是 UMA；**几乎没有 fork 专属的 NUMA 工作**。此处亲和性（第 8 讲）比 NUMA 更重要。 | — |
| 17 | 设备驱动模型、设备树 | **fork 的核心**：为 comma 3X/four 新增 **DTS 文件**，以及驱动中的 **of_match_table**；平台与总线代码把 DT 与驱动 probe 关联起来。 | 每个 comma 专属设备（camera、CAN-over-SPI 等）都通过该模型拉起。 |
| 18 | 字符驱动、V4L2 | **主要开发**：为 comma 硬件上的**前视与驾驶员监控 camera** 增补或大幅打补丁 **camera ISP 和 V4L2** 驱动。寄存器映射与 subdevice 布局与设备相关。 | camerad 依赖 V4L2；“固定其 kernel 版本以维持 camera ISP 寄存器映射兼容性”（Lecture-01）——被固定的正是这些驱动。 |
| 19 | io_uring、DMA-BUF、零拷贝 | kernel 中的 **DMA-BUF 与零拷贝**；**openpilot VisionIpc** 在其上使用共享内存和 DMA-BUF 式共享。fork 可能为 camera/GPU 启用或打补丁 DMA-BUF。 | 从 camera 到 modeld 与编码器的低延迟零拷贝通路。 |
| 20 | PCIe、NVMe、GPU | fork 中用于 SDM845 的 **GPU（Adreno）**与存储（UFS）驱动；载板上若有 PCIe/NVMe 则包含。 | GPU 执行推理；存储承载 OS 与日志。 |
| 21 | 文件系统（ext4、overlayfs） | **agnos-builder** 选择 rootfs 布局（ext4 或类似）以及用于更新的 overlay；kernel 具备相应的 fs 支持。 | 只读根 + 可写 overlay 是现场可靠更新的常见做法。 |
| 22 | eMMC/UFS、OTA 分区 | **agnos-builder** 定义**分区布局与 A/B 槽位**；kernel 的块设备与 UFS 驱动支持该器件。**OTA** 流程（如 updateinstallerd）依赖于此。 | 为现场 openpilot 提供安全、可回退的更新，不会让设备变砖。 |
| 23 | cgroups | 在 AGNOS 用户态中**可选**，用于隔离或资源限制；kernel 的 cgroup 支持是标准的。 | 可限制非关键服务，避免其挤占 openpilot 资源。 |
| 24 | 面向 AI 的 OS（L4T vs openpilot OS、RT 调优） | **AGNOS = “openpilot OS”**——**整个 fork 和 builder** 即为此。前面所有行（启动、DT、驱动、调度器、内存、存储）都是面向路上 AI/RT 的“开发变更”。L4T 是 Jetson 侧的对应物；Agnos 是 comma 侧的对应物。 | 直接对应：本讲说明 Agnos 为何存在，以及它如何为 openpilot 调优。 |
| 25 | Capstone（定制 Linux 镜像） | **agnos-builder** 是 comma 设备的**定制镜像构建**（不是 Yocto，但思路相同）：可复现的 kernel + rootfs + 启动产物。 | 交付单个“AGNOS”镜像，烧录到设备并运行 openpilot。 |
| 26 | eBPF 可观测性 | **kernel** 可能启用 CONFIG_BPF；用 bpftrace/perf 对 openpilot（camerad、modeld、controlsd）做**可观测性**分析即运行在该 kernel 上。 | 在开发阶段与现场对 openpilot 进行调试与性能剖析。 |


<details>
<summary>English original</summary>

| # | Lecture topic | Development change in AGNOS (fork / custom work) | Why it matters for openpilot on the road |
|:-:|----------------|--------------------------------------------------|------------------------------------------|
| 1 | OS architecture, monolithic kernel | Fork keeps Linux monolithic layout; **platform-specific code** in `arch/arm64/` and **vendor drivers** (camera ISP, CAN, GPU) added or patched for SDM845 and comma 3X hardware. | One kernel image drives cameras, CAN, and GPU; no extra IPC cost. Upstream changes could break ABI — comma **pins the kernel** to keep V4L2/ISP and SocketCAN stable for camerad and controlsd. |
| 2 | Processes, task_struct, fork/exec | Kernel process model unchanged; **agnos-builder** defines **which processes run** (systemd, openpilot’s camerad, modeld, controlsd) and how they are started. | openpilot’s multi-process design (camerad → modeld → plannerd → controlsd) relies on this; crash isolation and COW fork come from the same process model. |
| 3 | Interrupts, exceptions, bottom halves | **Driver changes**: camera and CAN drivers register IRQs and bottom-half paths; **latency of these paths** directly affects frame capture and CAN write timing. Comma’s patches keep interrupt and softirq handling predictable. | High-framerate camera and 100 Hz CAN need bounded interrupt latency; `/proc/interrupts` and IRQ affinity matter for tuning. |
| 4 | System calls, vDSO | Syscall and vDSO code is largely upstream; **no major fork-specific change**. vDSO `clock_gettime` is used by openpilot for sensor and model timestamps. | Stable, low-overhead timestamps for fusion and logging without syscall cost. |
| 5 | Boot, Device Tree, kernel modules | **Heavy development**: (1) **Boot chain** — agnos-builder supplies firmware (XBL, ABL), kernel, DTB, initrd; (2) **Device Tree** — DTS for comma 3X/four (cameras, SPI, CAN, GPIO); (3) **Driver probe** — camera ISP, CAN-over-SPI, power management drivers added/patched and matched via `compatible` in DT. | Correct boot and DT are required for cameras and CAN to appear; wrong DT or missing driver = no openpilot on the road. |
| 6 | CPU scheduling (CFS, SCHED_FIFO) | **Config and usage**: kernel has CFS/RT classes; **boot cmdline** (e.g. isolcpus) and **openpilot** use of `set_realtime_priority()` and `set_core_affinity()` (in openpilot repo). Comma may tune default scheduler or cmdline for RT workloads. | controlsd and modeld must hit deadlines; SCHED_FIFO and affinity avoid CFS jitter that would cause missed CAN frames or dropped frames. |
| 7 | Real-time (PREEMPT_RT, determinism) | **Kernel config**: preemption and locking options chosen for low latency; PREEMPT_RT or related patches may be applied in the fork for deterministic response. | controlsd CAN writes and inference loops need bounded latency; RT config is part of “openpilot on the road” reliability. |
| 8 | Multi-core, affinity, isolcpus | **Boot and userspace**: **agnos-builder** or device config sets **isolcpus** (and related cmdline); openpilot **set_core_affinity()** pins camerad/modeld/controlsd to chosen cores. Fork may carry scheduler/affinity fixes. | Isolating cores and pinning RT processes avoids interference from other work and reduces tail latency. |
| 9 | Synchronization (spinlocks, mutexes) | **Driver and core code**: any **new or modified driver** (camera, CAN, SPI) uses kernel locking primitives; lock design in the fork affects contention and latency. | Bad locking in a driver can cause stalls or priority inversion; PI mutex (Lecture 11) is relevant for openpilot’s high-priority control vs lower-priority readers. |
| 10 | RCU, lock-free, atomics | **Core kernel** uses RCU and atomics; fork may backport or tune for SDM845. No openpilot-specific RCU change, but **rcu_nocbs** on isolated cores (cmdline) moves RCU off RT cores. | Less jitter on cores running control and inference. |
| 11 | Deadlock, priority inversion, PI mutex | **Kernel** has rtmutex and PI; **openpilot** has high- and low-priority processes sharing state (e.g. cereal). Fork keeps PI mutex support so high-priority control is not blocked by low-priority readers. | Avoids “Mars Pathfinder”–style priority inversion in production. |
| 12 | Virtual memory, COW | **Core mm/** and **arch/arm64/mm**; fork keeps standard VM and COW. **openpilot** benefits: process isolation and COW fork for modeld/camerad without doubling RAM. | Stable VM model for multi-process openpilot and large model mappings. |
| 13–14 | Page tables, TLBs, SLUB, CMA | **mm/** and **arch/arm64/mm**; **CMA** is important for **camera and DMA buffers** on embedded SoCs — fork may enable or tune CMA for SDM845. | Camera pipeline and zero-copy buffers depend on contiguous and slab allocation. |
| 15 | DMA, IOMMU, GPU memory | **Driver and SoC support**: DMA and GPU (Adreno) drivers in the fork; IOMMU if enabled. **Buffer sharing** (camera ↔ GPU ↔ display) is platform-specific. | Needed for camera → GPU inference and display without extra copies. |
| 16 | NUMA | SDM845 is UMA; **little fork-specific NUMA work**. Affinity (Lecture 8) matters more than NUMA here. | — |
| 17 | Device driver model, Device Tree | **Central to the fork**: **new DTS files** and **of_match_table** in drivers for comma 3X/four; platform and bus code tie DT to driver probe. | Every comma-specific device (camera, CAN-over-SPI, etc.) is brought up via this model. |
| 18 | Character drivers, V4L2 | **Major development**: **camera ISP and V4L2** drivers are added or heavily patched for **road-facing and driver-monitoring cameras** on comma hardware. Register maps and subdevice layout are device-specific. | camerad depends on V4L2; “pins its kernel to maintain camera ISP register map compatibility” (Lecture-01) — these are the drivers that get pinned. |
| 19 | io_uring, DMA-BUF, zero-copy | **DMA-BUF and zero-copy** in kernel; **openpilot VisionIpc** uses shared memory and DMA-BUF-style sharing on top. Fork may enable or patch DMA-BUF for camera/GPU. | Low-latency, zero-copy path from camera to modeld and encoder. |
| 20 | PCIe, NVMe, GPU | **GPU (Adreno)** and storage (UFS) drivers in the fork for SDM845; PCIe/NVMe if present on carrier. | GPU runs inference; storage holds OS and logs. |
| 21 | Filesystems (ext4, overlayfs) | **agnos-builder** chooses rootfs layout (ext4 or similar) and overlay for updates; kernel has matching fs support. | Read-only root + writable overlay is common for reliable in-field updates. |
| 22 | eMMC/UFS, OTA partitioning | **agnos-builder** defines **partition layout and A/B slots**; kernel block and UFS drivers support the device. **OTA** flow (e.g. updateinstallerd) relies on this. | Safe, resettable updates for openpilot in the field without bricking the device. |
| 23 | cgroups | **Optional** in AGNOS userspace for isolation or resource limits; kernel cgroup support is standard. | Can limit non-critical services so they don’t starve openpilot. |
| 24 | OS for AI (L4T vs openpilot OS, RT tuning) | **AGNOS = “openpilot OS”** — the **entire fork and builder** are this. All previous rows (boot, DT, drivers, scheduler, memory, storage) are the “development changes” for AI/RT on the road. L4T is the Jetson counterpart; Agnos is the comma counterpart. | Direct mapping: this lecture describes why Agnos exists and how it’s tuned for openpilot. |
| 25 | Capstone (custom Linux image) | **agnos-builder** is the **custom image build** for comma devices (not Yocto, but same idea): reproducible kernel + rootfs + boot artifacts. | Delivers the single “AGNOS” image that goes on the device and runs openpilot. |
| 26 | eBPF observability | **Kernel** may enable CONFIG_BPF; **observability** of openpilot (camerad, modeld, controlsd) with bpftrace/perf runs on this kernel. | Debugging and profiling openpilot in development and in the field. |

</details>

</div>

---

## 各讲详细对应

### 第 1 讲：现代 OS 架构与 Linux kernel  
**OS 讲义：** [Lecture-01](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 宏内核布局 | **agnos-kernel-sdm845:** 顶层 `arch/`、`kernel/`、`mm/`、`drivers/`、`fs/`、`net/`——与任何 Linux 源码树相同。 |
| 平台相关代码 | `arch/arm64/`——ARM64 入口、MMU、异常、SDM845 的 SoC 设置。 |
| Agnos / “openpilot OS” | 运行在 comma 3X/four 上的就是这个 kernel；Lecture-01 的 “openpilot Agnos” 一节指的就是这个仓库。 |

---

### 第 2 讲：进程、task_struct 与 Linux 进程模型  
**OS 讲义：** [Lecture-02](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-02.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| task_struct、PCB | **agnos-kernel-sdm845:** `include/linux/sched.h`、`kernel/fork.c`（copy_process、task_struct 布局）。 |
| fork/exec/wait | `kernel/fork.c`、`fs/exec.c`——每个用户态进程都会用到；openpilot 的 camerad、modeld、controlsd 就是这类进程。 |
| 用户态进程 | **agnos-builder:** `userspace/`——systemd unit、init 脚本；运行在 AGNOS 上的进程（包括 openpilot）都从这一用户态启动。 |

---

### 第 3 讲：中断、异常与下半部  
**OS 讲义：** [Lecture-03](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 异常入口、IRQ 处理 | **agnos-kernel-sdm845:** `arch/arm64/kernel/entry.S`、`arch/arm64/kernel/irq.c`、`kernel/irq/`（通用 IRQ、芯片驱动）。 |
| 下半部、softirq、tasklet | `kernel/softirq.c`、`kernel/time/`（定时器）。 |
| 驱动中断 | `drivers/` 中任何执行 `request_irq()` 的驱动——例如 camera、SPI、GPIO；对 camerad 的延迟至关重要。 |

---

### 第 4 讲：系统调用、vDSO 与 eBPF  
**OS 讲义：** [Lecture-04](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-04.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 系统调用表与分发 | **agnos-kernel-sdm845:** `arch/arm64/kernel/syscall.c`、`include/uapi/asm-generic/unistd.h`。 |
| vDSO | `arch/arm64/kernel/vdso/`——例如 openpilot 用 clock_gettime 取时间戳。 |
| eBPF（若启用） | `kernel/bpf/`——取决于配置；用于可观测性（第 26 讲）。 |

---

### 第 5 讲：内核模块、启动流程与设备树  
**OS 讲义：** [Lecture-05](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| **启动序列**（ROM → bootloader → kernel → init） | **agnos-builder:** `firmware/`（XBL、ABL 等）、`build_system.sh`、`build_kernel.sh`。启动链与平台相关；builder 产出 kernel + initrd + rootfs。 |
| **设备树**（`.dts`/`.dtb`、`compatible`、驱动 probe） | **agnos-kernel-sdm845:** `arch/arm64/boot/dts/`、`*.dts` / `*.dtsi`。在 `drivers/` 中搜索 `compatible` 与驱动 `of_match_table`。 |
| **内核命令行**（`isolcpus`、`nohz_full`、`rcu_nocbs`） | **agnos-builder:** 启动配置；**agnos-kernel-sdm845:** `kernel/sched/` 消费 cmdline 以处理 isolcpus 等。 |
| **initramfs** | **agnos-builder:** `build_system.sh` 与 `userspace/` 打包早期 rootfs；检查其中的 init 与 switch_root。 |
| **内核模块** | **agnos-kernel-sdm845:** `drivers/`、`Kconfig`、`Makefile`；builder 在构建 AGNOS 镜像时构建 kernel（及模块）。 |

**任务：** `grep -r "compatible" agnos-kernel-sdm845/arch/arm64/boot/dts/ | head -30`；在 `agnos-builder/build_kernel.sh` 与 `build_system.sh` 中 trace 产物。

---

### 第 6 讲：CPU 调度（CFS、EEVDF、SCHED_FIFO）  
**OS 讲义：** [Lecture-06](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| **公平调度器（CFS/EEVDF）** | **agnos-kernel-sdm845:** `kernel/sched/core.c`、`fair.c`（CFS）、`rt.c`（RT 类）。4.x kernel 使用 CFS。 |
| **SCHED_FIFO** | kernel 实现 `sched_setscheduler(SCHED_FIFO)`；openpilot 在 `openpilot/common/util.cc` 中的 `set_realtime_priority()` 会调用它（见 [Lecture-06 Real example](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md)）。 |
| **CPU 隔离 / 亲和性** | 启动 cmdline（来自 builder）+ openpilot `set_core_affinity()`；调度器遵循 isolcpus 与亲和性。 |

**任务：** 检查 `kernel/sched/fair.c`（vruntime、pick-next）与 `rt.c`（FIFO/RR）。


<details>
<summary>English original</summary>

**Detailed Mapping by Lecture**

**Lecture 1: Modern OS Architecture & the Linux Kernel**
**OS Lecture:** [Lecture-01](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-01.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Monolithic kernel layout | **agnos-kernel-sdm845:** Top-level `arch/`, `kernel/`, `mm/`, `drivers/`, `fs/`, `net/` — same as any Linux tree. |
| Platform-specific code | `arch/arm64/` — ARM64 entry, MMU, exceptions, SoC setup for SDM845. |
| Agnos / “openpilot OS” | This kernel is the one running on comma 3X/four; Lecture-01’s “openpilot Agnos” section refers to this repo. |

---

**Lecture 2: Processes, task_struct & the Linux Process Model**
**OS Lecture:** [Lecture-02](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-02.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| task_struct, PCB | **agnos-kernel-sdm845:** `include/linux/sched.h`, `kernel/fork.c` (copy_process, task_struct layout). |
| fork/exec/wait | `kernel/fork.c`, `fs/exec.c` — used by every userspace process; openpilot’s camerad, modeld, controlsd are such processes. |
| Userspace processes | **agnos-builder:** `userspace/` — systemd units, init scripts; processes that run on AGNOS (including openpilot) are started from this userspace. |

---

**Lecture 3: Interrupts, Exceptions & Bottom Halves**
**OS Lecture:** [Lecture-03](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Exception entry, IRQ handling | **agnos-kernel-sdm845:** `arch/arm64/kernel/entry.S`, `arch/arm64/kernel/irq.c`, `kernel/irq/` (generic IRQ, chip drivers). |
| Bottom halves, softirq, tasklets | `kernel/softirq.c`, `kernel/time/` (timers). |
| Driver IRQs | Any driver in `drivers/` that does `request_irq()` — e.g. camera, SPI, GPIO; critical for camerad latency. |

---

**Lecture 4: System Calls, vDSO & eBPF**
**OS Lecture:** [Lecture-04](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-04.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Syscall table and dispatch | **agnos-kernel-sdm845:** `arch/arm64/kernel/syscall.c`, `include/uapi/asm-generic/unistd.h`. |
| vDSO | `arch/arm64/kernel/vdso/` — e.g. clock_gettime used by openpilot for timestamps. |
| eBPF (if enabled) | `kernel/bpf/` — config-dependent; used for observability (Lecture 26). |

---

**Lecture 5: Kernel Modules, Boot Process & Device Tree**
**OS Lecture:** [Lecture-05](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| **Boot sequence** (ROM → bootloader → kernel → init) | **agnos-builder:** `firmware/` (XBL, ABL, etc.), `build_system.sh`, `build_kernel.sh`. Boot chain is platform-specific; builder produces kernel + initrd + rootfs. |
| **Device Tree** (`.dts`/`.dtb`, `compatible`, driver probe) | **agnos-kernel-sdm845:** `arch/arm64/boot/dts/`, `*.dts` / `*.dtsi`. Search for `compatible` and driver `of_match_table` in `drivers/`. |
| **Kernel command line** (`isolcpus`, `nohz_full`, `rcu_nocbs`) | **agnos-builder:** Boot config; **agnos-kernel-sdm845:** `kernel/sched/` consumes cmdline for isolcpus etc. |
| **initramfs** | **agnos-builder:** `build_system.sh` and `userspace/` pack early rootfs; inspect for init and switch_root. |
| **Kernel modules** | **agnos-kernel-sdm845:** `drivers/`, `Kconfig`, `Makefile`; builder builds kernel (and modules) as part of AGNOS image. |

**Tasks:** `grep -r "compatible" agnos-kernel-sdm845/arch/arm64/boot/dts/ | head -30`; trace artifacts in `agnos-builder/build_kernel.sh` and `build_system.sh`.

---

**Lecture 6: CPU Scheduling (CFS, EEVDF, SCHED_FIFO)**
**OS Lecture:** [Lecture-06](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| **Fair scheduler (CFS/EEVDF)** | **agnos-kernel-sdm845:** `kernel/sched/core.c`, `fair.c` (CFS), `rt.c` (RT class). 4.x kernel uses CFS. |
| **SCHED_FIFO** | Kernel implements `sched_setscheduler(SCHED_FIFO)`; openpilot’s `set_realtime_priority()` in `openpilot/common/util.cc` calls it (see [Lecture-06 Real example](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md)). |
| **CPU isolation / affinity** | Boot cmdline (from builder) + openpilot `set_core_affinity()`; scheduler respects isolcpus and affinity. |

**Tasks:** Inspect `kernel/sched/fair.c` (vruntime, pick-next) and `rt.c` (FIFO/RR).

---

</details>

### Lecture 7: 实时 Linux（PREEMPT_RT 与确定性）
**OS Lecture:** [Lecture-07](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-07.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 抢占配置 | **agnos-kernel-sdm845:** `Kconfig`（CONFIG_PREEMPT_*）、`kernel/sched/`、`kernel/locking/`（若为 PREEMPT_RT 则为 rtmutex）。 |
| 延迟、确定性 | 相同的调度器与锁代码；openpilot 的 controlsd/modeld 依赖低延迟——见 Lecture 6 和 8。 |

---

### Lecture 8: 多核调度、CPU 亲和性与 isolcpus
**OS Lecture:** [Lecture-08](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-08.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 亲和性、CPU 掩码 | **agnos-kernel-sdm845:** `kernel/sched/core.c`（set_cpus_allowed_ptr、负载均衡）、`arch/arm64/`（topology）。 |
| isolcpus | 通过内核 cmdline 设置；由 **agnos-builder** 或设备启动配置提供；openpilot 随后用 `set_core_affinity()`（util.cc）绑核。 |

---

### Lecture 9: 同步（自旋锁、互斥锁、读写锁）
**OS Lecture:** [Lecture-09](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-09.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 自旋锁、互斥锁、rwlock | **agnos-kernel-sdm845:** `kernel/locking/spinlock.c`、`mutex.c`、`rwlock.c`、`include/linux/spinlock.h`、`mutex.h`。 |
| 驱动中的用法 | 任何保护共享状态的 `drivers/` 代码；例如 V4L2、SPI、块层。 |

---

### Lecture 10: 无锁编程（RCU、原子操作）
**OS Lecture:** [Lecture-10](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-10.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| RCU | **agnos-kernel-sdm845:** `kernel/rcu/`——被广泛使用（调度器、网络、VFS）。 |
| 原子操作、内存序 | `include/linux/atomic.h`，架构相关部分在 `arch/arm64/include/asm/atomic.h` 中。 |

---

### Lecture 11: 死锁、优先级反转与 PI 互斥锁
**OS Lecture:** [Lecture-11](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-11.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| rtmutex、优先级继承 | **agnos-kernel-sdm845:** `kernel/locking/rtmutex.c`；调度器集成 PI（见 Lecture 11 中 openpilot cereal/controlsd 的说明）。 |
| 死锁避免 | `kernel/locking/` 与驱动中的锁顺序和设计。 |

---

### Lecture 12: 虚拟内存与 Linux 内存模型
**OS Lecture:** [Lecture-12](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-12.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| VMA、页表 | **agnos-kernel-sdm845:** `mm/mmap.c`、`mm/memory.c`、`arch/arm64/mm/`（页表遍历、TLB）。 |
| COW、fork | `kernel/fork.c`（私有映射的写时复制）；openpilot 多进程（camerad、modeld、controlsd）使用该机制。 |

---

### Lecture 13: 页表、TLB 与大页
**OS Lecture:** [Lecture-13](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-13.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| PTE 操作、TLB 刷新 | **agnos-kernel-sdm845:** `arch/arm64/mm/`（pgtable、tlbflush）、`mm/memory.c`。 |
| 大页 | `arch/arm64/mm/`，hugetlbfs 在 `fs/hugetlbfs/` 中（若启用）。 |

---

### Lecture 14: 内存分配（SLUB、kmalloc、CMA）
**OS Lecture:** [Lecture-14](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-14.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| SLUB、kmalloc | **agnos-kernel-sdm845:** `mm/slub.c`、`include/linux/slab.h`；`kmalloc`/`kfree` 被所有驱动使用。 |
| CMA（连续内存分配器） | `mm/cma.c`——常用于摄像头缓冲区、嵌入式 SoC 上的 DMA。 |

---

### Lecture 15: DMA、IOMMU 与 GPU 内存管理
**OS Lecture:** [Lecture-15](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-15.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| DMA API、IOMMU | **agnos-kernel-sdm845:** `drivers/base/dma-mapping.c`、`drivers/iommu/`（若为 SDM845 启用）。 |
| GPU（SDM845 上的 Adreno） | GPU 驱动位于 `drivers/` 下（vendor/Qualcomm）；与摄像头和显示共享缓冲区。 |

---

### Lecture 16: NUMA 拓扑与 HPC（高性能计算）内存优化
**OS Lecture:** [Lecture-16](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-16.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| NUMA（若存在） | **agnos-kernel-sdm845:** `arch/arm64/mm/`、NUMA 配置；SDM845 通常是 UMA——单节点。 |
| 内存拓扑 | `arch/arm64/` / 内核中的 CPU 拓扑；与亲和性相关（Lecture 8）。 |

---


<details>
<summary>English original</summary>

**Lecture 7: Real-Time Linux (PREEMPT_RT & Determinism)**
**OS Lecture:** [Lecture-07](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-07.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Preemption config | **agnos-kernel-sdm845:** `Kconfig` (CONFIG_PREEMPT_*), `kernel/sched/`, `kernel/locking/` (rtmutex if PREEMPT_RT). |
| Latency, determinism | Same scheduler and locking code; openpilot’s controlsd/modeld depend on low latency — see Lecture 6 and 8. |

---

**Lecture 8: Multi-Core Scheduling, CPU Affinity & isolcpus**
**OS Lecture:** [Lecture-08](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-08.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Affinity, CPU mask | **agnos-kernel-sdm845:** `kernel/sched/core.c` (set_cpus_allowed_ptr, load balance), `arch/arm64/` (topology). |
| isolcpus | Set via kernel cmdline; **agnos-builder** or device boot config provides it; openpilot then pins with `set_core_affinity()` (util.cc). |

---

**Lecture 9: Synchronization (Spinlocks, Mutexes, RW Locks)**
**OS Lecture:** [Lecture-09](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-09.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Spinlocks, mutexes, rwlock | **agnos-kernel-sdm845:** `kernel/locking/spinlock.c`, `mutex.c`, `rwlock.c`, `include/linux/spinlock.h`, `mutex.h`. |
| Usage in drivers | Any `drivers/` code that protects shared state; e.g. V4L2, SPI, block layer. |

---

**Lecture 10: Lock-Free Programming (RCU, Atomics)**
**OS Lecture:** [Lecture-10](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-10.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| RCU | **agnos-kernel-sdm845:** `kernel/rcu/` — used widely (scheduler, networking, VFS). |
| Atomics, memory ordering | `include/linux/atomic.h`, arch-specific in `arch/arm64/include/asm/atomic.h`. |

---

**Lecture 11: Deadlock, Priority Inversion & PI Mutexes**
**OS Lecture:** [Lecture-11](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-11.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| rtmutex, priority inheritance | **agnos-kernel-sdm845:** `kernel/locking/rtmutex.c`; scheduler integrates PI (see Lecture 11’s openpilot cereal/controlsd note). |
| Deadlock avoidance | Lock ordering and design in `kernel/locking/` and drivers. |

---

**Lecture 12: Virtual Memory & the Linux Memory Model**
**OS Lecture:** [Lecture-12](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-12.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| VMAs, page tables | **agnos-kernel-sdm845:** `mm/mmap.c`, `mm/memory.c`, `arch/arm64/mm/` (page table walk, TLB). |
| COW, fork | `kernel/fork.c` (copy-on-write for private mappings); openpilot multi-process (camerad, modeld, controlsd) uses this. |

---

**Lecture 13: Page Tables, TLBs & Huge Pages**
**OS Lecture:** [Lecture-13](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-13.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| PTE manipulation, TLB flush | **agnos-kernel-sdm845:** `arch/arm64/mm/` (pgtable, tlbflush), `mm/memory.c`. |
| Huge pages | `arch/arm64/mm/`, hugetlbfs in `fs/hugetlbfs/` (if enabled). |

---

**Lecture 14: Memory Allocation (SLUB, kmalloc, CMA)**
**OS Lecture:** [Lecture-14](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-14.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| SLUB, kmalloc | **agnos-kernel-sdm845:** `mm/slub.c`, `include/linux/slab.h`; `kmalloc`/`kfree` used by all drivers. |
| CMA (Contiguous Memory Allocator) | `mm/cma.c` — often used for camera buffers, DMA on embedded SoCs. |

---

**Lecture 15: DMA, IOMMU & GPU Memory Management**
**OS Lecture:** [Lecture-15](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-15.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| DMA API, IOMMU | **agnos-kernel-sdm845:** `drivers/base/dma-mapping.c`, `drivers/iommu/` (if enabled for SDM845). |
| GPU (Adreno on SDM845) | GPU driver under `drivers/` (vendor/Qualcomm); shares buffers with camera and display. |

---

**Lecture 16: NUMA Topology & HPC Memory Optimization**
**OS Lecture:** [Lecture-16](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-16.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| NUMA (if present) | **agnos-kernel-sdm845:** `arch/arm64/mm/`, NUMA config; SDM845 is typically UMA — single node. |
| Memory topology | CPU topology in `arch/arm64/` / kernel; relevant for affinity (Lecture 8). |

---

</details>

### Lecture 17: Linux 设备驱动模型与设备树  
**OS 课程：** [Lecture-17](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 平台驱动、of_*（OF = 设备树） | **agnos-kernel-sdm845：** `drivers/base/platform.c`、`drivers/base/of_*.c`、`arch/arm64/boot/dts/`。 |
| 总线、设备、驱动模型 | `drivers/base/`、`include/linux/device.h`；每个摄像头、SPI、I2C 驱动都在此接入。 |

---

### Lecture 18: 字符驱动、中断驱动 I/O 与 V4L2  
**OS 课程：** [Lecture-18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| V4L2、摄像头流水线 | **agnos-kernel-sdm845：** `drivers/media/` — V4L2 子设备、视频采集；这就是 openpilot camerad 所交互的对象。 |
| 字符驱动、chardev | `drivers/`（例如 SPI、I2C 暴露 chardev 或被 V4L2 使用）；驱动处理函数中的中断驱动 I/O。 |

---

### Lecture 19: 现代 I/O（io_uring、DMA-BUF 与零拷贝）  
**OS 课程：** [Lecture-19](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-19.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| io_uring | **agnos-kernel-sdm845：** `io_uring/`（若在配置中启用）。 |
| DMA-BUF、零拷贝 | `drivers/dma-buf/`；被摄像头/GPU 流水线使用；openpilot VisionIpc 构建于共享内存 / 零拷贝之上。 |

---

### Lecture 20: PCIe、NVMe 与 GPU 驱动架构  
**OS 课程：** [Lecture-20](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-20.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| PCIe、NVMe | **agnos-kernel-sdm845：** `drivers/pci/`、`drivers/nvme/`（若平台上使用）。 |
| GPU（Adreno） | GPU 驱动位于 `drivers/`（Qualcomm）；用于 openpilot 推理（例如 tinygrad/OpenCL/Vulkan）。 |

---

### Lecture 21: 文件系统（ext4、btrfs、F2FS、overlayfs）  
**OS 课程：** [Lecture-21](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-21.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| VFS、ext4、overlayfs | **agnos-kernel-sdm845：** `fs/ext4/`、`fs/overlayfs/`、`fs/`（VFS layer）。 |
| Rootfs 布局 | **agnos-builder：** `userspace/` 与 `build_system.sh` 定义 rootfs 上放置的内容；通常为 ext4 或类似格式。 |

---

### Lecture 22: 嵌入式存储（eMMC、UFS、OTA 分区）  
**OS 课程：** [Lecture-22](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-22.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 块层、UFS（SDM845 存储） | **agnos-kernel-sdm845：** `drivers/scsi/`（UFS）、`block/`。 |
| 分区、A/B、OTA | **agnos-builder：** `firmware/`、分区脚本；用于系统更新的 A/B 槽位（类似 Lecture 22 中 openpilot Agnos 的说明）。 |

---

### Lecture 23: 容器、cgroups v2  
**OS 课程：** [Lecture-23](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-23.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| cgroups | **agnos-kernel-sdm845：** `kernel/cgroup/` — 若启用则为 cgroup v2。 |
| 用户空间 | **agnos-builder：** 可选地在用户空间使用 cgroups 进行进程隔离或资源限制。 |

---

### Lecture 24: AI 系统的 OS（L4T、openpilot OS 与 RT 调优）  
**OS 课程：** [Lecture-24](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-24.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| “openpilot OS” | **AGNOS = openpilot 的 OS**，运行于 comma 3X/four：**agnos-kernel-sdm845** + **agnos-builder** 构建的用户空间。 |
| RT 调优（modeld、controlsd、camerad） | 调度器（Lecture 6、8）、亲和性、isolcpus；openpilot 在此内核上 `util.cc`（set_realtime_priority、set_core_affinity）。 |
| L4T 对比 | Lecture 24 比较 L4T（Jetson）与 Agnos（comma）；本仓库是 Agnos 一侧。 |

---

### Lecture 25: Capstone — 自定义 Linux 镜像（Yocto）  
**OS 课程：** [Lecture-25](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-25.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| 自定义 OS 镜像 | **agnos-builder** 是 comma 设备“自定义 Linux 镜像”的 capstone：它构建内核（来自 agnos-kernel-sdm845）+ rootfs + 启动产物，不是 Yocto，但思路相同。 |
| 可复现构建 | agnos-builder 中基于 Docker 的构建；带版本号的内核子模块。 |

---


<details>
<summary>English original</summary>

**Lecture 17: Linux Device Driver Model & Device Tree**
**OS Lecture:** [Lecture-17](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Platform driver, of_* (OF = Device Tree) | **agnos-kernel-sdm845:** `drivers/base/platform.c`, `drivers/base/of_*.c`, `arch/arm64/boot/dts/`. |
| Bus, device, driver model | `drivers/base/`, `include/linux/device.h`; every camera, SPI, I2C driver plugs in here. |

---

**Lecture 18: Character Drivers, Interrupt-Driven I/O & V4L2**
**OS Lecture:** [Lecture-18](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| V4L2, camera pipeline | **agnos-kernel-sdm845:** `drivers/media/` — V4L2 subdevs, video capture; this is what openpilot camerad talks to. |
| Character drivers, chardev | `drivers/` (e.g. SPI, I2C expose chardev or are used by V4L2); interrupt-driven I/O in driver handlers. |

---

**Lecture 19: Modern I/O (io_uring, DMA-BUF & Zero-Copy)**
**OS Lecture:** [Lecture-19](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-19.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| io_uring | **agnos-kernel-sdm845:** `io_uring/` (if enabled in config). |
| DMA-BUF, zero-copy | `drivers/dma-buf/`; used by camera/GPU pipeline; openpilot VisionIpc builds on shared memory / zero-copy. |

---

**Lecture 20: PCIe, NVMe & GPU Driver Architecture**
**OS Lecture:** [Lecture-20](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-20.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| PCIe, NVMe | **agnos-kernel-sdm845:** `drivers/pci/`, `drivers/nvme/` (if used on platform). |
| GPU (Adreno) | GPU driver in `drivers/` (Qualcomm); used for openpilot inference (e.g. tinygrad/OpenCL/Vulkan). |

---

**Lecture 21: Filesystems (ext4, btrfs, F2FS, overlayfs)**
**OS Lecture:** [Lecture-21](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-21.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| VFS, ext4, overlayfs | **agnos-kernel-sdm845:** `fs/ext4/`, `fs/overlayfs/`, `fs/` (VFS layer). |
| Rootfs layout | **agnos-builder:** `userspace/` and `build_system.sh` define what goes on rootfs; often ext4 or similar. |

---

**Lecture 22: Embedded Storage (eMMC, UFS, OTA Partitioning)**
**OS Lecture:** [Lecture-22](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-22.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Block layer, UFS (SDM845 storage) | **agnos-kernel-sdm845:** `drivers/scsi/` (UFS), `block/`. |
| Partitions, A/B, OTA | **agnos-builder:** `firmware/`, partition scripts; A/B slots for system updates (similar to Lecture 22’s openpilot Agnos note). |

---

**Lecture 23: Containers, cgroups v2**
**OS Lecture:** [Lecture-23](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-23.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| cgroups | **agnos-kernel-sdm845:** `kernel/cgroup/` — cgroup v2 if enabled. |
| Userspace | **agnos-builder:** Optional use of cgroups in userspace for process isolation or resource limits. |

---

**Lecture 24: OS for AI Systems (L4T, openpilot OS & RT Tuning)**
**OS Lecture:** [Lecture-24](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-24.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| “openpilot OS” | **AGNOS = openpilot’s OS** on comma 3X/four: **agnos-kernel-sdm845** + **agnos-builder**-built userspace. |
| RT tuning (modeld, controlsd, camerad) | Scheduler (Lectures 6, 8), affinity, isolcpus; openpilot `util.cc` (set_realtime_priority, set_core_affinity) on this kernel. |
| L4T comparison | Lecture 24 compares L4T (Jetson) vs Agnos (comma); this repo is the Agnos side. |

---

**Lecture 25: Capstone — Custom Linux Images (Yocto)**
**OS Lecture:** [Lecture-25](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-25.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| Custom OS image | **agnos-builder** is the capstone for “custom Linux image” for comma devices: it builds kernel (from agnos-kernel-sdm845) + rootfs + boot artifacts, not Yocto but same idea. |
| Reproducible build | Docker-based build in agnos-builder; versioned kernel submodule. |

---

</details>

### 第 26 讲：eBPF — 可编程的内核可观测性  
**OS 课程：** [Lecture-26](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-26.md)

| 概念 | 在 AGNOS 中的位置 |
|--------|-----------------|
| eBPF 核心 | **agnos-kernel-sdm845：** `kernel/bpf/`、`net/bpf/`（若启用 CONFIG_BPF）。 |
| openpilot 的可观测性 | 在 AGNOS 上用 bpftrace/perf 对 openpilot（camerad、modeld、controlsd）做追踪；第 26 讲的 openpilot 流水线示例适用于该 kernel。 |

---

## 建议的学习路径

1. **阶段 1 — OS 课程（全部 26 讲）**  
   使用上文 [全部 OS 讲次 → AGNOS（概览）](#all-os-lectures--agnos-overview) 表格：每一讲都链接到 OS 幻灯片以及具体的 kernel/builder 路径。从第 1–6 讲开始（体系结构、进程、中断、系统调用、boot/DT、调度），然后按顺序或按主题学习其余部分。

2. **克隆并打开这两个仓库**  
   把 [阶段 1 — 操作系统 — 指南](../../../../阶段1-基础知识/03-操作系统/Guide.md) 和讲次列表放在手边。

3. **agnos-kernel-sdm845**  
   - 浏览 `arch/arm64/`、`kernel/sched/`、`drivers/`。  
   - 搜索 `compatible`、`of_match_table`、`probe`、`module_init`，查看设备树与模块流程（第 5 讲）。  
   - 查看 `kernel/sched/fair.c` 和 `rt.c`，了解 CFS 与 RT（第 6 讲）。

4. **agnos-builder**  
   - 阅读 `README.md`，运行（若已有 Docker）`./build_kernel.sh` 和/或 `./build_system.sh`。  
   - 追踪启动产物：脚本与 `firmware/` 中的 kernel 镜像、DTB、initrd、rootfs。

5. **与 openpilot 交叉关联**  
   openpilot 运行在 AGNOS 上。[Lecture-05](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md) 和 [Lecture-06](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md) 的“Real example in openpilot”一节指向 `openpilot/common/util.cc`（`set_realtime_priority`、`set_core_affinity`）——这些系统调用的目标是 AGNOS kernel。

---

## 速查：已克隆仓库中的关键路径

| 内容 | 路径（相对于仓库根目录） |
|------|-------------------------------|
| ARM64 boot / 设备树 | `agnos-kernel-sdm845/arch/arm64/` |
| 设备树源码 | `agnos-kernel-sdm845/arch/arm64/boot/dts/` |
| 调度器（CFS、RT） | `agnos-kernel-sdm845/kernel/sched/` |
| 驱动（camera、SPI 等） | `agnos-kernel-sdm845/drivers/` |
| 构建 kernel | `agnos-builder/build_kernel.sh` |
| 构建系统镜像 | `agnos-builder/build_system.sh` |
| 用户态 / initramfs 内容 | `agnos-builder/userspace/` |
| 与 boot/firmware 相关 | `agnos-builder/firmware/` |

---

## 资源

- **agnos-kernel-sdm845：** [GitHub — commaai/agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) ——“SDM845 模块的 kernel”。  
- **agnos-builder：** [GitHub — commaai/agnos-builder](https://github.com/commaai/agnos-builder) ——“构建 AGNOS，即 comma three、3X 和 four 的操作系统”。  
- **OS 课程：** [阶段 1 — 操作系统 — 指南](../../../../阶段1-基础知识/03-操作系统/Guide.md) 和 [讲次索引](../../../../阶段1-基础知识/03-操作系统/Guide.md)。  
- **openpilot：** 运行在 AGNOS 上；参见 [自动驾驶指南](../Guide.md) 和 [流程图](../flow-diagram.md)。


<details>
<summary>English original</summary>

**Lecture 26: eBPF — Programmable Kernel Observability**
**OS Lecture:** [Lecture-26](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-26.md)

| Concept | Where in AGNOS |
|--------|-----------------|
| eBPF core | **agnos-kernel-sdm845:** `kernel/bpf/`, `net/bpf/` (if CONFIG_BPF enabled). |
| Observability of openpilot | Tracing openpilot (camerad, modeld, controlsd) on AGNOS with bpftrace/perf; Lecture 26’s openpilot pipeline examples apply to this kernel. |

---

**Suggested Study Path**

1. **Phase 1 — OS course (all 26 lectures)**  
   Use the [All OS Lectures → AGNOS (Overview)](#all-os-lectures--agnos-overview) table above: every lecture links to the OS slide and to concrete kernel/builder paths. Start with Lectures 1–6 (architecture, processes, interrupts, syscalls, boot/DT, scheduling), then follow the rest in order or by topic.

2. **Clone and open the two repos**  
   Keep [Phase 1 — Operating Systems — Guide](../../../../阶段1-基础知识/03-操作系统/Guide.md) and lecture list handy.

3. **agnos-kernel-sdm845**  
   - Browse `arch/arm64/`, `kernel/sched/`, `drivers/`.  
   - Search for `compatible`, `of_match_table`, `probe`, `module_init` to see Device Tree and module flow (Lecture 5).  
   - Look at `kernel/sched/fair.c` and `rt.c` for CFS and RT (Lecture 6).

4. **agnos-builder**  
   - Read `README.md`, run (if you have Docker) `./build_kernel.sh` and/or `./build_system.sh`.  
   - Trace boot artifacts: kernel image, DTB, initrd, rootfs in scripts and `firmware/`.

5. **Cross-link with openpilot**  
   openpilot runs on AGNOS. [Lecture-05](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md) and [Lecture-06](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-06.md) “Real example in openpilot” sections point to `openpilot/common/util.cc` (`set_realtime_priority`, `set_core_affinity`) — those system calls target the AGNOS kernel.

---

**Quick Reference: Key Paths in Cloned Repos**

| What | Path (relative to repo root) |
|------|-------------------------------|
| ARM64 boot / Device Tree | `agnos-kernel-sdm845/arch/arm64/` |
| Device Tree sources | `agnos-kernel-sdm845/arch/arm64/boot/dts/` |
| Scheduler (CFS, RT) | `agnos-kernel-sdm845/kernel/sched/` |
| Drivers (camera, SPI, etc.) | `agnos-kernel-sdm845/drivers/` |
| Build kernel | `agnos-builder/build_kernel.sh` |
| Build system image | `agnos-builder/build_system.sh` |
| Userspace / initramfs content | `agnos-builder/userspace/` |
| Boot/firmware related | `agnos-builder/firmware/` |

---

**Resources**

- **agnos-kernel-sdm845:** [GitHub — commaai/agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) — “Kernel for the SDM845 modules”.  
- **agnos-builder:** [GitHub — commaai/agnos-builder](https://github.com/commaai/agnos-builder) — “Build AGNOS, the operating system for the comma three, 3X, and four.”  
- **OS course:** [Phase 1 — Operating Systems — Guide](../../../../阶段1-基础知识/03-操作系统/Guide.md) and [Lecture index](../../../../阶段1-基础知识/03-操作系统/Guide.md).  
- **openpilot:** Runs on AGNOS; see [Autonomous Driving Guide](../Guide.md) and [flow diagram](../flow-diagram.md).

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/2. openpilot Reference Stack/agnos/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/2.%20openpilot%20Reference%20Stack/agnos/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
