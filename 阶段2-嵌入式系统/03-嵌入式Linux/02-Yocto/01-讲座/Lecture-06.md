---
title: Lecture 6 — 模块 4：recipe：系统的原子
description: Lecture 6 — 模块 4：recipe：系统的原子
---

# Lecture 6 — 模块 4：recipe：系统的原子

**课程：** [Yocto guide](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [Lecture 05](Lecture-05.md) | **下一讲：** [Lecture 07 — 模块 5](Lecture-07.md)

---

## 1. recipe 的解剖

一个 **recipe** 回答的都是可预期的问题：**源码** 从哪里来（Git、tarball、license 文件）；如何配置与编译（autotools、CMake、Meson、Makefile、预编译产物）；哪些文件被安装到 staged root；如何拆分进 **runtime 包**（dev、dbg 等）。

## 2. 继承与 class

recipe 不复制样板代码，而是 **inherit** 行为：autotools、cmake、meson、kernel、image 及相关 class。class 固化了这类软件通常的构建方式。

## 3. 你会反复见到的变量（概念层面）

名称会有细微演变，但思路不变：**SRC_URI**（下载什么；常列出 patch）；**S**（解压后的源码）；**B**（out-of-tree 构建目录）；**WORKDIR**（每个 recipe 的临时工作区）；**PN、PV、PR**（name/version/revision）。

## 4. Lab 4 — 检查一个真实的 recipe

在 **OE-Core** 中挑一个小 recipe。追查它继承的是哪个 **class**、存在哪些 task（do_compile、do_install 等），以及它产出哪些包名。

用 bitbake -e 加上你的 recipe 名查看生效环境（输出很大：要学会检索它）。准备好探索 kernel 时，可选做 bitbake -c menuconfig linux-yocto。

**完成标准：** 你能说出该 recipe 从源码到包之间有哪些 task。

---

**上一讲：** [Lecture 05](Lecture-05.md) | **下一讲：** [Lecture 07 — 模块 5](Lecture-07.md)


<details>
<summary>English original</summary>

**Lecture 6 — Module 4: Recipes: the atoms of the system**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 05](Lecture-05.md) | **Next:** [Lecture 07 — Module 5](Lecture-07.md)

---

**1. Anatomy of a recipe**

A **recipe** answers predictable questions: where **sources** come from (Git, tarball, license file); how it is configured and compiled (autotools, CMake, Meson, Makefile, prebuilt); what files are installed into staged roots; how it is split into **runtime packages** (dev, dbg, etc.).

**2. Inheritance and classes**

Instead of copying boilerplate, recipes **inherit** behavior: autotools, cmake, meson, kernel, image, and related classes. Classes encode how this kind of software is usually built.

**3. Variables you will see constantly (conceptual)**

Names evolve slightly, but the ideas do not: **SRC_URI** (what to download; patches often listed); **S** (unpacked sources); **B** (out-of-tree build dir); **WORKDIR** (per-recipe scratch); **PN, PV, PR** (name/version/revision).

**4. Lab 4 — Inspect a real recipe**

Pick a small recipe in **OE-Core**. Trace which **class** it inherits, which tasks exist (do_compile, do_install, etc.), and what package names it produces.

Use bitbake -e with your recipe name for effective environment (large output: learn to search it). Optionally bitbake -c menuconfig linux-yocto when you are ready for kernel exploration.

**Done when:** you can name the tasks between source and package for that recipe.

---

**Previous:** [Lecture 05](Lecture-05.md) | **Next:** [Lecture 07 — Module 5](Lecture-07.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
