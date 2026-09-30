---
title: 第 12 讲 — 模块 10：许可证、合规与供应链卫生
description: 第 12 讲 — 模块 10：许可证、合规与供应链卫生
---

# 第 12 讲 — 模块 10：许可证、合规与供应链卫生

**课程：** [Yocto guide](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [第 11 讲](Lecture-11.md) | **下一讲：** [第 13 讲 — 模块 11](Lecture-13.md)

---

## 1. 为什么这在硬件产品中很重要

你的固件镜像捆绑了**第三方版权**。客户和收购方会要求提供**软件 BOM**、许可证文本，以及在需要时提供源码。

## 2. 实用习惯

- 启用与你的发布版本相适应的许可证清单生成（变量名会演进；以当前文档为准）。
- 把 recipe 中的 **LICENSE** 和 **LIC_FILES_CHKSUM** 当作一等评审项。
- 对任何五年后仍必须可重建的东西，**固定分支**或使用镜像源。

## 3. 实验 10 — 检查清单

生成你的镜像并定位**许可证清单**产物。挑三个软件包核对：SPDX 或许可证文件是否已记录，版本是否与你以为已发布的一致。

---

**上一讲：** [第 11 讲](Lecture-11.md) | **下一讲：** [第 13 讲 — 模块 11](Lecture-13.md)


<details>
<summary>English original</summary>

**Lecture 12 — Module 10: Licenses, compliance, and supply-chain hygiene**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 11](Lecture-11.md) | **Next:** [Lecture 13 — Module 11](Lecture-13.md)

---

**1. Why this matters in hardware products**

Your firmware image bundles **third-party copyrights**. Customers and acquirers ask for **software BOMs**, license texts, and source offers where required.

**2. Practical habits**

- Enable license manifest generation appropriate to your release (variable names evolve; follow current docs).
- Treat **LICENSE** and **LIC_FILES_CHKSUM** in recipes as first-class review items.
- **Pin branches** or use mirrors for anything that must be rebuildable in five years.

**3. Lab 10 — Inspect the manifests**

Generate your image and locate the **license manifest** artifacts. Pick three packages and verify: SPDX or license file is recorded, and the version matches what you thought you shipped.

---

**Previous:** [Lecture 11](Lecture-11.md) | **Next:** [Lecture 13 — Module 11](Lecture-13.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-12.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-12.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
