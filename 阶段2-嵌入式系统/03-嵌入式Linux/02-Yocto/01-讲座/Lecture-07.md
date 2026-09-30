---
title: 第 7 讲 — 模块 5：layer：如何让元数据保持可维护
description: 第 7 讲 — 模块 5：layer：如何让元数据保持可维护
---

# 第 7 讲 — 模块 5：layer：如何让元数据保持可维护

**课程：** [Yocto 指南](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [第 06 讲](Lecture-06.md) | **下一讲：** [第 08 讲 — 模块 6](Lecture-08.md)

---

## 1. layer 为何存在

layer 让团队**分离关注点**：BSP 掌握硬件知识，公司发行版策略横跨多个产品，产品特有的调整被隔离且可评审。

## 2. 创建自定义 layer（模式）

针对你的 release，使用官方 **bitbake-layers create-layer** 工作流。最终应得到 conf/layer.conf、recipes-* 目录树，以及可选的 README，用于说明 layer 意图与维护者。

## 3. bbappend 纪律

如果上游 recipe 不是你写的，优先在你的 layer 中用 **.bbappend**。好的做法：加一个 patch、调整 PACKAGECONFIG、添加 runtime 依赖、通过 FILESPATH 模式安装配置。坏的做法：**fork 一个 recipe**，只为改一个本可 append 的变量。

## 4. 实验 5 — 你自己的 layer，最小改动

创建 meta-mycourse，把它加入 bblayers.conf，对某个 recipe append 一处微不足道的改动（无害的 PACKAGECONFIG 开关或 FILES 微调）。

**完成标准：** bitbake-layers show-layers 按预期优先级顺序列出你的 layer，且构建仍然成功。

---

**上一讲：** [第 06 讲](Lecture-06.md) | **下一讲：** [第 08 讲 — 模块 6](Lecture-08.md)


<details>
<summary>English original</summary>

**Lecture 7 — Module 5: Layers: how metadata stays maintainable**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 06](Lecture-06.md) | **Next:** [Lecture 08 — Module 6](Lecture-08.md)

---

**1. Why layers exist**

Layers let teams **separate concerns**: BSP with hardware knowledge, corporate distro policy across products, product-specific tweaks isolated and reviewable.

**2. Creating a custom layer (pattern)**

Use the official **bitbake-layers create-layer** workflow for your release. You should end with conf/layer.conf, recipes-* trees, and optional README for layer intent and maintainers.

**3. bbappend discipline**

If you did not write the upstream recipe, prefer **.bbappend** in your layer. Good: add a patch, tweak PACKAGECONFIG, add a runtime dependency, install a config via FILESPATH patterns. Bad: **fork a recipe** to change one variable you could append.

**4. Lab 5 — Your own layer, minimal change**

Create meta-mycourse, add it to bblayers.conf, append a trivial change to a recipe (harmless PACKAGECONFIG toggle or FILES tweak).

**Done when:** bitbake-layers show-layers lists your layer in the expected priority order and the build still succeeds.

---

**Previous:** [Lecture 06](Lecture-06.md) | **Next:** [Lecture 08 — Module 6](Lecture-08.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
