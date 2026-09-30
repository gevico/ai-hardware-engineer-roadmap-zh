---
title: Lecture 4 — 模块 2：一张图看懂架构
description: Lecture 4 — 模块 2：一张图看懂架构
---

# Lecture 4 — 模块 2：一张图看懂架构

**课程：** [Yocto guide](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [Lecture 03](Lecture-03.md) | **下一讲：** [Lecture 05 — 模块 3](Lecture-05.md)

---

## 1. 数据流（心智模型）

layer 中的**元数据**（conf/、recipes-*）流入 **BitBake**，由它解析 recipe 以及 MACHINE、DISTRO 和 image。这会产出一张 **task graph**：fetch、unpack、patch、configure、compile、install、package，然后是 image。输出产物包括 RPM/IPK/DEB 包、rootfs、kernel、bootloader 和 SDK。

## 2. 关键对象

- **Recipe（.bb）** — 如何构建一份软件（或抓取一个二进制）。
- **Append 文件（.bbappend）** — 你的 overlay 改动，不必编辑上游 recipe。
- **Layer** — 一个元数据 + 配置的文件夹，作为整体做版本管理。
- **Image recipe** — 定义这个产品镜像的 root filesystem 上会落地哪些包。
- **MACHINE** — 选择板级相关的 kernel/bootloader/固件假设。
- **DISTRO** — 策略开关：libc 选择、init 系统偏好、安全姿态默认值（随 distro layer 而异）。

## 3. 你经常改的两个配置

- **conf/bblayers.conf** — BitBake 被允许读取哪些 layer。
- **conf/local.conf** — 本机设置：并行度、下载目录、额外的 image 特性、临时调整。

**白话版：** bblayers.conf 决定有哪些规则手册。local.conf 决定你的笔记本或构建服务器如何套用它们。

## 4. 实验 2 — 画出你的 layer 栈

在纸上或白板上，把 **Poky/OE-Core** 画在底部，有自己的 **BSP layer** 时画在中间，把 **product layer** 画在顶部。标出哪个 layer 该负责 kernel 调整、rootfs 包、systemd units 和版本锁定。

**完成标准：** 在打开编辑器之前，你能说清一处改动该落在哪里。

---

**上一讲：** [Lecture 03](Lecture-03.md) | **下一讲：** [Lecture 05 — 模块 3](Lecture-05.md)


<details>
<summary>English original</summary>

**Lecture 4 — Module 2: Architecture in one picture**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 03](Lecture-03.md) | **Next:** [Lecture 05 — Module 3](Lecture-05.md)

---

**1. Data flow (mental model)**

**Metadata** in layers (conf/, recipes-*) flows into **BitBake**, which parses recipes plus MACHINE, DISTRO, and images. That produces a **task graph**: fetch, unpack, patch, configure, compile, install, package, then image. Output artifacts include RPM/IPK/DEB packages, rootfs, kernel, bootloader, and SDK.

**2. Key objects**

- **Recipe (.bb)** — how to build one piece of software (or fetch a binary).
- **Append file (.bbappend)** — your overlay changes without editing upstream recipes.
- **Layer** — a folder of metadata + configuration, versioned as a unit.
- **Image recipe** — defines what packages land on the root filesystem for this product image.
- **MACHINE** — selects board-specific kernel/bootloader/firmware assumptions.
- **DISTRO** — policy knobs: libc choices, init system preferences, security posture defaults (varies by distro layer).

**3. Two configs you touch constantly**

- **conf/bblayers.conf** — which layers BitBake is allowed to read.
- **conf/local.conf** — local machine settings: parallelism, download directory, extra image features, temporary tweaks.

**Plain English:** bblayers.conf is which rulebooks exist. local.conf is how your laptop or build server applies them.

**4. Lab 2 — Draw your layer stack**

On paper or a whiteboard, draw **Poky/OE-Core** at the bottom, your **BSP layer** when you have one, and your **product layer** on top. Mark which layer should own kernel tweaks, rootfs packages, systemd units, and version pins.

**Done when:** you can explain where a change should live before you open an editor.

---

**Previous:** [Lecture 03](Lecture-03.md) | **Next:** [Lecture 05 — Module 3](Lecture-05.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
