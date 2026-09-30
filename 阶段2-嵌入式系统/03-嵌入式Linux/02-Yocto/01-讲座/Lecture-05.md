---
title: 第 5 讲 — 模块 3：首次构建成功
description: 第 5 讲 — 模块 3：首次构建成功
---

# 第 5 讲 — 模块 3：首次构建成功

**课程：** [Yocto 指南](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [第 04 讲](Lecture-04.md) | **下一讲：** [第 06 讲 — 模块 4](Lecture-06.md)

---

## 1. 获取 Poky（参考流程）

具体分支名会变；始终优先使用**你所选版本的文档**。

典型流程（概念层面）：在指定的 release 分支上克隆 **Poky**；为你的构建目录运行 oe-init-build-env 脚本；把 **MACHINE** 设为参考机器（学习时通常是 QEMU）；先构建 core-image-minimal。

## 2. 成功会产出什么

你应当能指出构建目录 deploy 区域下的**镜像产物**（布局随版本而异），以及适配所选 MACHINE 的 **kernel** 和 **rootfs**。

## 3. 实验 3 — 最小镜像，在 QEMU（或硬件）上启动

若你的代码检出支持，为支持 QEMU 的机器构建 **core-image-minimal**。按该版本的文档启动。登录后用标准 shell 命令验证 **kernel 与 OS 的身份**。

**完成标准：** 你已在干净的 shell 中仅凭自己的笔记重现了这次构建（没有随手从博客复制粘贴）。

---

**上一讲：** [第 04 讲](Lecture-04.md) | **下一讲：** [第 06 讲 — 模块 4](Lecture-06.md)


<details>
<summary>English original</summary>

**Lecture 5 — Module 3: First successful build**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 04](Lecture-04.md) | **Next:** [Lecture 06 — Module 4](Lecture-06.md)

---

**1. Get Poky (reference flow)**

Exact branch names change; always prefer the **documentation for the release you chose**.

Typical sequence (conceptual): clone **Poky** at a named release branch; run the oe-init-build-env script for your build directory; set **MACHINE** to a reference machine (often QEMU for learning); build core-image-minimal first.

**2. What success produces**

You should be able to point to the **image artifacts** under the build directory deploy area (layout varies by version), and a **kernel** and **rootfs** suitable for the selected MACHINE.

**3. Lab 3 — Minimal image, boot under QEMU (or hardware)**

Build **core-image-minimal** for a QEMU-capable machine if your checkout supports it. Boot per the release docs. Log in and verify **kernel and OS identity** with standard shell commands.

**Done when:** you have repeated the build from a clean shell using only your notes (no random blog copy-paste).

---

**Previous:** [Lecture 04](Lecture-04.md) | **Next:** [Lecture 06 — Module 4](Lecture-06.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
