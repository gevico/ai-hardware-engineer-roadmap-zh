---
title: Lecture 10 — 模块 8：SDK 与应用工作流
description: Lecture 10 — 模块 8：SDK 与应用工作流
---

# Lecture 10 — 模块 8：SDK 与应用工作流

**课程：** [Yocto 指南](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [Lecture 09](Lecture-09.md) | **下一讲：** [Lecture 11 — 模块 9](Lecture-11.md)

---

## 1. SDK 为什么存在

镜像构建证明系统能拼装到一起。**SDK** 让应用开发者拿到交叉工具链以及与 sysroot 匹配的头文件/库，而**不必**在一个 tree 里构建整个世界。

## 2. 标准套路

- 构建 `meta-toolchain` 或你的文档推荐的 image-specific SDK target。
- 把 SDK shell 脚本产物安装到可预测的路径。
- `source` 环境脚本，并用所提供的 `CC`、`PKG_CONFIG_PATH` 等构建你的 app。

## 3. Lab 8 — 交叉编译 "hello"

针对 **SDK sysroot** 交叉编译一个简单的 C 程序，并在**目标镜像**上运行它。

**完成标准：** 你确信 SDK 版本字符串与你烧录的镜像一致（没有隐性的 ABI 偏移）。

---

**上一讲：** [Lecture 09](Lecture-09.md) | **下一讲：** [Lecture 11 — 模块 9](Lecture-11.md)


<details>
<summary>English original</summary>

**Lecture 10 — Module 8: SDK and application workflow**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 09](Lecture-09.md) | **Next:** [Lecture 11 — Module 9](Lecture-11.md)

---

**1. Why the SDK exists**

The image build proves the system fits together. The **SDK** gives application developers a cross-toolchain and sysroot-matched headers/libs **without** building the whole world in one tree.

**2. Standard pattern**

- Build `meta-toolchain` or the image-specific SDK target your docs recommend.
- Install the SDK shell script output into a predictable path.
- `source` the environment script and build your app with the provided `CC`, `PKG_CONFIG_PATH`, etc.

**3. Lab 8 — Cross-compile "hello"**

Cross-compile a trivial C program against the **SDK sysroot** and run it on the **target image**.

**Done when:** you trust the SDK version string matches the image you flashed (no silent ABI skew).

---

**Previous:** [Lecture 09](Lecture-09.md) | **Next:** [Lecture 11 — Module 9](Lecture-11.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-10.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-10.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
