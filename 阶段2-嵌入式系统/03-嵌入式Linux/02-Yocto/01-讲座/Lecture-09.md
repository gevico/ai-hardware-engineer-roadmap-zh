---
title: Lecture 9 — 模块 7：Kernel、bootloader、设备树（集成者级别）
description: Lecture 9 — 模块 7：Kernel、bootloader、设备树（集成者级别）
---

# Lecture 9 — 模块 7：Kernel、bootloader、设备树（集成者级别）

**课程：** [Yocto guide](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [Lecture 08](Lecture-08.md) | **下一讲：** [Lecture 10 — 模块 8](Lecture-10.md)

---

## 1. Yocto 对你的要求

在此阶段，**不要求你成为 kernel 维护者**。你负责：

- 为你的 BSP 选择合适的 **kernel provider** / recipe。
- 携带**板级设备树**以及硬件所需的任何 **kernel config 片段**。
- 理解 deploy 目录中的启动产物来自**何处**。

## 2. 典型定制路径

- **Config 片段** — 在受支持时，这是可维护地修改 kernel 选项的首选方式。
- **补丁** — 用于驱动、DTS 修复或 backport（需有评审与上游合入计划）。
- **树外模块** — 有时是专有或快速迭代驱动的合适边界。

## 3. 实验 7 — trace 启动产物

针对你的 MACHINE，列出哪些 task 会产出：

- Kernel 镜像 / 设备树 blob（若适用）。
- Bootloader 二进制文件。

**完成标准：** 你能打开 deploy 目录，并指出烧录脚本会用到的每个文件。

---

**上一讲：** [Lecture 08](Lecture-08.md) | **下一讲：** [Lecture 10 — 模块 8](Lecture-10.md)


<details>
<summary>English original</summary>

**Lecture 9 — Module 7: Kernel, bootloader, device tree (integrator level)**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 08](Lecture-08.md) | **Next:** [Lecture 10 — Module 8](Lecture-10.md)

---

**1. What Yocto expects you to know**

At this stage you are **not required to be a kernel maintainer**. You are responsible for:

- Selecting the right **kernel provider** / recipe for your BSP.
- Carrying **board device trees** and any **kernel config fragments** your hardware needs.
- Understanding **where** boot artifacts come from in the deploy directory.

**2. Typical customization paths**

- **Config fragments** — preferred for maintainable kernel option changes when supported.
- **Patches** — for drivers, DTS fixes, or backports (with review and upstreaming plan).
- **Out-of-tree modules** — sometimes the right boundary for proprietary or fast-moving drivers.

**3. Lab 7 — Trace boot artifacts**

For your MACHINE, list which tasks produce:

- Kernel image / device tree blobs (if applicable).
- Bootloader binary(ies).

**Done when:** you can open the deploy directory and point to each file the flashing script would use.

---

**Previous:** [Lecture 08](Lecture-08.md) | **Next:** [Lecture 10 — Module 8](Lecture-10.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-09.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-09.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
