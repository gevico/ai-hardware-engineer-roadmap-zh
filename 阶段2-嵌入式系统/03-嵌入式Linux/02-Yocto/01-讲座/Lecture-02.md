---
title: Lecture 2 — Module 0：前置要求与主机环境准备
description: Lecture 2 — Module 0：前置要求与主机环境准备
---

# Lecture 2 — Module 0：前置要求与主机环境准备

**课程：** [Yocto guide](../Guide.md) · **阶段 2 — 嵌入式 Linux → Yocto**

**上一讲：** [Lecture 01](Lecture-01.md) · **下一讲：** [Lecture 03 — Module 1](Lecture-03.md)

---

## 1. 应已具备的技能

- 熟悉 **Linux 命令行**（路径、环境变量、`grep`、基本脚本编写）。
- 嵌入式基础概念：bootloader → kernel → 根文件系统；**设备树**在高层面上是什么。
- **Git 工作流**（clone、branch、commit）。Yocto *本身*就是一个重度依赖 Git 的生态。

## 2. 主机配置预期

Yocto 构建与其说“神秘”，不如说**对磁盘和 I/O 要求很高**。

| 资源 | 实际最低配置 | 舒适配置 |
|----------|-------------------|-------------|
| CPU | 4 核 | 8 核以上 |
| 内存 | 8 GB（仅限小镜像） | 16–32 GB |
| 磁盘（强烈建议 SSD） | 剩余 80 GB | 200+ GB |
| 操作系统 | Linux（所用的 Yocto 版本有受支持的发行版） | 同上，ext4 或类似文件系统 |

构建树要放在**区分大小写**的文件系统上。在某些平台上，不区分大小写会导致离奇且极难排查的失败。

## 3. 实验 0 — 验证主机

- 依照**你所用 Yocto 版本的官方指南**安装发行版的构建依赖（软件包名随发行版而变）。
- 确认 `git`、`python3`，以及一套可在*主机*上正常工作的编译器工具链均已就位。
- 建立专用的用户或目录约定，让路径保持稳定（团队会感谢你）。

**验收标准：**能在主机上 clone 一个大型 Git 仓库，并无错编译一个小型原生 C“hello world”。

---

**上一讲：** [Lecture 01](Lecture-01.md) · **下一讲：** [Lecture 03 — Module 1](Lecture-03.md)


<details>
<summary>English original</summary>

**Lecture 2 — Module 0: Prerequisites and host setup**

**Course:** [Yocto guide](../Guide.md) · **Phase 2 — Embedded Linux → Yocto**

**Previous:** [Lecture 01](Lecture-01.md) · **Next:** [Lecture 03 — Module 1](Lecture-03.md)

---

**1. Skills you should already have**

- Comfortable on the **Linux command line** (paths, environment variables, `grep`, basic scripting).
- Basic embedded concepts: bootloader → kernel → root filesystem; what a **device tree** is at a high level.
- **Git workflows** (clone, branch, commit). Yocto *is* a Git-heavy ecosystem.

**2. Host machine expectations**

Yocto builds are **disk- and I/O-heavy** more than they are “mystical.”

| Resource | Practical minimum | Comfortable |
|----------|-------------------|-------------|
| CPU | 4 cores | 8+ cores |
| RAM | 8 GB (small images only) | 16–32 GB |
| Disk (SSD strongly preferred) | 80 GB free | 200+ GB |
| OS | Linux (supported distro for your Yocto release) | Same, on ext4 or similar |

Use a **case-sensitive** filesystem for the build tree. On some platforms, case-insensitivity causes bizarre, hard-to-debug failures.

**3. Lab 0 — Verify your host**

- Install your distro’s build dependencies using the **official Yocto guide for your release** (package names drift by distro).
- Confirm `git`, `python3`, and a working compiler toolchain for the *host* are present.
- Create a dedicated user or directory convention so paths stay stable (teams will thank you).

**Done when:** you can clone a large Git repository and compile a small native C “hello world” on the host without errors.

---

**Previous:** [Lecture 01](Lecture-01.md) · **Next:** [Lecture 03 — Module 1](Lecture-03.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
