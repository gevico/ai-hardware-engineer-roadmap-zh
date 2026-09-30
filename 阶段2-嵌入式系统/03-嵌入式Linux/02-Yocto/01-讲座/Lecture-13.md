---
title: 第 13 讲 — 模块 11：性能、缓存与 CI
description: 第 13 讲 — 模块 11：性能、缓存与 CI
---

# 第 13 讲 — 模块 11：性能、缓存与 CI

**课程：** [Yocto guide](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [第 12 讲](Lecture-12.md) | **下一讲：** [第 14 讲 — Capstone](Lecture-14.md)

---

## 1. 一句话说清共享状态（sstate）

**sstate** 缓存任务输出，因此在配置正确时，干净构建会**跳过未发生变化的工作**。

## 2. CI 设计目标

为发布流使用**确定性的分支**，而不是 main 不断移动带来的意外。在共享存储上**分离 downloads 与 sstate 目录**，并配合加锁纪律。用 SRCREV 或适合自身流程的 manifest 工具固定 layer。

## 3. 实验 11 — 测量一次重建

分别计时：缓存已热时的**空操作重建**，与清空 tmp 但保留 downloads 之后的重建。记录差值。

**完成标准：** 能说明在 CI 中绝不可删除的内容，以及可以安全清理的内容。

---

**上一讲：** [第 12 讲](Lecture-12.md) | **下一讲：** [第 14 讲 — Capstone](Lecture-14.md)


<details>
<summary>English original</summary>

**Lecture 13 — Module 11: Performance, caching, and CI**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 12](Lecture-12.md) | **Next:** [Lecture 14 — Capstone](Lecture-14.md)

---

**1. Shared state (sstate) in one sentence**

**sstate** caches task outputs so clean builds **skip work that has not changed**, when configured correctly.

**2. CI design goals**

Use **deterministic branches** for release streams, not moving-main surprises. **Separate download and sstate directories** on shared storage with locking discipline. Pin layers with SRCREV or manifest tools appropriate to your process.

**3. Lab 11 — Measure a rebuild**

Time a **no-op rebuild** with warm cache vs after wiping tmp but keeping downloads. Record the delta.

**Done when:** you can explain what you would never delete in CI vs what is safe to scrub.

---

**Previous:** [Lecture 12](Lecture-12.md) | **Next:** [Lecture 14 — Capstone](Lecture-14.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-13.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-13.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
