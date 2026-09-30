---
title: 第 8 讲 — 模块 6：镜像、软件包与特性
description: 第 8 讲 — 模块 6：镜像、软件包与特性
---

# 第 8 讲 — 模块 6：镜像、软件包与特性

**课程：** [Yocto 指南](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [第 07 讲](Lecture-07.md) | **下一讲：** [第 09 讲 — 模块 7](Lecture-09.md)

---

## 1. image recipe 与 packagegroup

- **image recipe** — 指明哪些软件包构成可作为可烧写产物的 rootfs。
- **packagegroup recipe** — 捆绑相关软件包，以便在多个镜像间复用。

## 2. IMAGE_INSTALL 及相关变量

团队通常用以下方式扩展镜像：

- **IMAGE_INSTALL:append** — 追加式，在许多配置下便于评审。

**避免直接修改 Poky 镜像**；应通过 bbappend，或在你自己的 layer 中自定义 image recipe 来扩展。

## 3. 镜像特性（概念）

调试辅助、工具或 init 调整之类的特性，通常通过 **镜像特性** 来控制（确切名称因发行版/版本而异）。把它们视为**策略开关**，而不是随意的变量。

## 4. 实验 6 — 自定义镜像

创建 recipes-core/images/mycourse-image.bb，使其：

- 依据你的版本，从最小基础镜像中恰当地 require 或 inherit。
- 添加你需要的两个软件包（例如 strace 和你喜欢的小型 shell 工具）。

**完成标准：** bitbake mycourse-image 产出新的可部署产物，并且你能说出它与 minimal 的大小差异。

---

**上一讲：** [第 07 讲](Lecture-07.md) | **下一讲：** [第 09 讲 — 模块 7](Lecture-09.md)


<details>
<summary>English original</summary>

**Lecture 8 — Module 6: Images, packages, and features**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 07](Lecture-07.md) | **Next:** [Lecture 09 — Module 7](Lecture-09.md)

---

**1. Image recipes vs packagegroups**

- **Image recipe** — names the packages that become the rootfs for a flashable artifact.
- **packagegroup recipes** — bundles related packages for reuse across images.

**2. IMAGE_INSTALL and friends**

Teams usually extend images with:

- **IMAGE_INSTALL:append** — additive, review-friendly in many setups.

**Avoid editing Poky images directly**; extend via bbappend or a custom image recipe in your layer.

**3. Image features (conceptual)**

Features like debugging helpers, tools, or init tweaks are often gated as **image features** (exact names vary by distro/release). Treat them as **policy switches**, not random variables.

**4. Lab 6 — Custom image**

Create recipes-core/images/mycourse-image.bb that:

- requires or inherits appropriately from a minimal base image for your release.
- Adds two packages you need (for example, strace and your favorite tiny shell utility).

**Done when:** bitbake mycourse-image produces a new deployable artifact and you can identify its size difference vs minimal.

---

**Previous:** [Lecture 07](Lecture-07.md) | **Next:** [Lecture 09 — Module 7](Lecture-09.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
