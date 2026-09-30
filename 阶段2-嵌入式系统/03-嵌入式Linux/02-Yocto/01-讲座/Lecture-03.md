---
title: 第 3 讲 — 模块 1：Yocto 是什么（以及不是什么）
description: 第 3 讲 — 模块 1：Yocto 是什么（以及不是什么）
---

# 第 3 讲 — 模块 1：Yocto 是什么（以及不是什么）

**课程：**[Yocto guide](../Guide.md) | **阶段 2 — 嵌入式 Linux，Yocto**

**上一讲：**[第 02 讲](Lecture-02.md) | **下一讲：**[第 04 讲 — 模块 2](Lecture-04.md)

---

## 1. 问题

嵌入式产品需要的 OS 应当满足：

- **可重复**（相同输入产生对你的发布流程而言足够一致的输出）。
- **可审计**（许可证、版本、补丁可知且被记录）。
- 在可能之处做到**最小化**（更少的软件包、更少的 CVE、更小的更新载荷）。
- **与硬件对齐**（kernel、固件、启动链、驱动）。

## 2. Yocto Project 提供了什么

**Yocto Project** 是一个伞形组织与治理模型。实际工作中你接触的是：

- **OpenEmbedded-Core (OE-Core)** — 基础 recipe、class 和策略。
- **BitBake** — 任务调度器 / 构建引擎。
- **Poky** — 一个参考集成（OE-Core + BitBake + 工具链），可供你构建并从中学习。

**直白地说：**Yocto 不是一个 Linux 发行版。它是一个**工厂**，能从元数据产出类发行版的产物（镜像、软件包、SDK）。

## 3. Yocto vs Buildroot（选型指引）

| 维度 | Yocto / OpenEmbedded | Buildroot |
|-----------|----------------------|-----------|
| 理念 | layer 化，跨产品共享元数据 | 单树配置，非常直接 |
| 软件包生态 | 庞大，社区 + 厂商 layer | 默认集较小；可扩展 |
| 学习曲线 | 更陡 | 更平缓 |
| 多产品复用 | 强（layer、distro、配置） | 可行；通常更手工 |
| 厂商提供 layer 的情况 | 常常是预期的集成路径 | 较少见（但并非从不） |

两者都不总是更好。如果你的**芯片厂商**维护着成熟的 Yocto layer，这往往就决定了默认选择。

## 4. 实验 1 — 用 Yocto 的语言写下你的产品需求

用一页纸回答：

- **目标硬件**（SoC、已知时的 machine 名）。
- **连接性**（以太网、Wi-Fi、USB gadget、CAN 等）。
- **存储布局**（eMMC、NAND、NVMe、仅 SD 的演示）。
- **更新策略**（A/B、单分区、包管理器、基于镜像）。
- **必备用户态**（是否用容器、图形或无头、实时需求）。

**完成标准：**你能说清哪些必须进镜像，哪些只是锦上添花。

### 完整示例（可选）

一份针对 **Jetson 级硬件上的边缘 AI 相机**的完整一页式答案（硬件、连接性、存储、OTA、用户态、必备 vs 可选），并配有 Yocto 映射与文件草稿：**[Lab 01 — Worked example (edge AI camera)](Lab-01-Worked-Example-Edge-AI-Camera.md)**。把它当作模板；把 MACHINE 名、layer 和软件包替换成你的 BSP 与发布版本。

---

**上一讲：**[第 02 讲](Lecture-02.md) | **下一讲：**[第 04 讲 — 模块 2](Lecture-04.md)


<details>
<summary>English original</summary>

**Lecture 3 — Module 1: What Yocto is (and is not)**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 02](Lecture-02.md) | **Next:** [Lecture 04 — Module 2](Lecture-04.md)

---

**1. The problem**

Embedded products need an OS that is:

- **Repeatable** (same inputs lead to same enough outputs for your release process).
- **Auditable** (licenses, versions, patches known and recorded).
- **Minimal** where possible (fewer packages, fewer CVEs and smaller update payloads).
- **Aligned to hardware** (kernel, firmware, boot chain, drivers).

**2. What the Yocto Project provides**

**Yocto Project** is an umbrella and governance model. Practically, you work with:

- **OpenEmbedded-Core (OE-Core)** — base recipes, classes, and policies.
- **BitBake** — the task scheduler / build engine.
- **Poky** — a reference integration (OE-Core + BitBake + tooling) you can build and learn from.

**Plain English:** Yocto is not a Linux distro. It is a **factory** that can produce distro-like artifacts (images, packages, SDKs) from metadata.

**3. Yocto vs Buildroot (decision guidance)**

| Dimension | Yocto / OpenEmbedded | Buildroot |
|-----------|----------------------|-----------|
| Philosophy | Layers, sharing metadata across products | Single-tree config, very direct |
| Package ecosystem | Large, community + vendor layers | Smaller default set; extensions possible |
| Learning curve | Steeper | Gentler |
| Multi-product reuse | Strong (layers, distros, configs) | Possible; often more manual |
| When vendors ship layers | Often the expected integration path | Less common (but not never) |

Neither is always better. If your **silicon vendor** maintains a mature Yocto layer, that often decides the default.

**4. Lab 1 — Write your product requirements in Yocto terms**

Answer in one page:

- **Target hardware** (SoC, machine name if known).
- **Connectivity** (Ethernet, Wi-Fi, USB gadgets, CAN, etc.).
- **Storage layout** (eMMC, NAND, NVMe, SD-only demo).
- **Update strategy** (A/B, single partition, package manager, image-based).
- **Must-have userspace** (containers or not, graphics or headless, real-time needs).

**Done when:** you can name what must be in the image vs what is nice to have.

**Worked example (optional)**

A full one-page-style answer for an **edge AI camera on Jetson-class hardware** (hardware, connectivity, storage, OTA, userspace, must vs nice) with Yocto mappings and file sketches: **[Lab 01 — Worked example (edge AI camera)](Lab-01-Worked-Example-Edge-AI-Camera.md)**. Use it as a template; replace MACHINE names, layers, and packages with your BSP and release.

---

**Previous:** [Lecture 02](Lecture-02.md) | **Next:** [Lecture 04 — Module 2](Lecture-04.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
