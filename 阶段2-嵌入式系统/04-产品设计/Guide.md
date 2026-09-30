---
title: 嵌入式系统的产品设计
description: 嵌入式系统的产品设计
---

# 嵌入式系统的产品设计

<div class="course-identity product-design" markdown="1">
<div class="course-identity__icon">PRD</div>
<div markdown="1">
<p class="course-identity__eyebrow">模块 4 · 产品设计</p>
<p class="course-identity__title">把能跑通的硬件变成一台在设置、控制、信任与部署边界上都站得住脚的设备。</p>
<p class="course-identity__meta">产物：V1 产品简报 · 度量：设置摩擦、可靠性、信任线索</p>
</div>
</div>


一门结构化的迷你课程，面向已经能对板子、固件、Linux 和连接性做推理的工程师，他们想了解这些技术选择如何变成**真正的产品**。

本课程隶属于 **阶段 2 - 嵌入式系统**，因为这里的产品设计不是指营销话术或 app 原型图。它指的是：

- 设备是干什么用的
- 它放在哪里
- 它做了哪些取舍
- 它如何传递信任
- 硬件、声学、控制、软件与设置如何协同工作

贯穿全程的示例是 **AI 智能音箱 / 家庭 AI 设备**。

---

## 为什么有这门课程

很多嵌入式工程师能做出可运行的系统，却仍会在如下产品决策上犯难：

- 这台设备应该语音优先还是屏幕优先？
- 它应该感觉像一件工具、一台家电，还是房间里的一个物件？
- 麦克风、扬声器、通风口和控制件应该放在哪里？
- 什么时候物理静音开关是必须的？
- 多大的设置摩擦是可接受的？
- 一项技术上很有意思的功能，何时反而会削弱产品？

这些是产品设计问题，但它们同时也是嵌入式系统问题，因为它们会改变：

- 外壳布局
- 散热设计
- 声学架构
- 板级约束
- 软件边界
- 隐私行为
- 用户信任

---

## 你将学到什么

- 为什么产品设计属于嵌入式工程内部，而不是其外。
- 第一波智能音箱市场做对了什么、做错了什么。
- HomePod 2018 发布会在硬件强、助手弱方面教给了我们什么。
- 后续的 HomePod / 2023 时代平台经验，在房间角色、设置和家庭工作流方面教给了我们什么。
- 如何把这些经验转化为 local-first 的 AI 智能音箱设计。
- 如何把信任、麦克风、扬声器、算力部署位置、设置和生态边界当作一个产品系统来思考。

---

## 循序渐进的课程

每讲都是 **[Lecture/](01-讲座/README.md)** 下的一个独立文件。按顺序学习。

| # | 主题 | 讲 |
|---|-------|---------|
| 1 | 为什么产品设计属于嵌入式系统 | [Lecture-01.md](01-讲座/Lecture-01.md) |
| 2 | HomePod 2018 的经验：好音质还不够 | [Lecture-02.md](01-讲座/Lecture-02.md) |
| 3 | 后续 HomePod / 2023 时代的平台经验 | [Lecture-03.md](01-讲座/Lecture-03.md) |
| 4 | AI 智能音箱设计工作坊 | [Lecture-04.md](01-讲座/Lecture-04.md) |

---

## 建议的学习方式

每一讲：

1. 找出正在讨论的产品决策
2. 把它映射到一个真实的嵌入式后果
3. 问一问什么变好了、什么变差了
4. 为你自己的设计写一条你会保留的简短产品规则

不要把它当作抽象的品牌分析来学。把它当作系统设计来学。

---

## 这门课程不是什么

这门课程不是：

- UI 设计理论
- 泛泛的创业建议
- 为讲历史而讲消费电子史

它是一门面向嵌入式工程师的实用产品思维课程，以智能音箱为案例。

---

## 建议产出

学完这门迷你课程，你应该能够写出：

- 一页产品意图说明
- 一份 V1 硬件/产品取舍清单
- 一份信任与隐私设计检查清单
- 一份简单的 AI 智能音箱设计简报

这才是正确的产物类型，因为它证明你能把工程选择与产品行为连接起来。

---

**下一讲：** [Lecture 01 - 为什么产品设计属于嵌入式系统](01-讲座/Lecture-01.md)


<details>
<summary>English original</summary>

**Product Design for Embedded Systems**

<div class="course-identity product-design" markdown="1">
<div class="course-identity__icon">PRD</div>
<div markdown="1">
<p class="course-identity__eyebrow">Module 4 · Product Design</p>
<p class="course-identity__title">Turn working hardware into a device with credible setup, controls, trust, and deployment boundaries.</p>
<p class="course-identity__meta">Artifact: V1 product brief · Measure: setup friction, reliability, trust cues</p>
</div>
</div>


A structured mini-course for engineers who can already reason about boards, firmware, Linux, and connectivity, but want to learn how those technical choices become a **real product**.

This course sits under **Phase 2 - Embedded Systems** because product design here does not mean marketing language or app mockups. It means:

- what the device is for
- where it lives
- what tradeoffs it makes
- how it signals trust
- how hardware, acoustics, controls, software, and setup work together

The running example is an **AI smart speaker / home AI appliance**.

---

**Why this course exists**

A lot of embedded engineers can build a working system and still struggle with product decisions such as:

- should this device be voice-first or screen-first?
- should it feel like a tool, an appliance, or a room object?
- where should microphones, speakers, vents, and controls go?
- when is a physical mute switch mandatory?
- how much setup friction is acceptable?
- when does a technically interesting feature actually weaken the product?

Those are product-design questions, but they are also embedded-system questions because they change:

- enclosure layout
- thermal design
- acoustic architecture
- board constraints
- software boundaries
- privacy behavior
- user trust

---

**What you will learn**

- Why product design belongs inside embedded engineering, not outside it.
- What the first-wave smart-speaker market got right and wrong.
- What the HomePod 2018 launch taught about hardware strength versus assistant weakness.
- What later HomePod / 2023-era platform lessons taught about room role, setup, and household workflows.
- How to translate those lessons into a local-first AI smart-speaker design.
- How to think about trust, microphones, speakers, compute placement, setup, and ecosystem boundaries as one product system.

---

**Step-by-step lectures**

Each lecture is a separate file under **[Lecture/](01-讲座/README.md)**. Work in order.

| # | Topic | Lecture |
|---|-------|---------|
| 1 | Why product design belongs in embedded systems | [Lecture-01.md](01-讲座/Lecture-01.md) |
| 2 | HomePod 2018 lessons: great sound is not enough | [Lecture-02.md](01-讲座/Lecture-02.md) |
| 3 | Later HomePod / 2023-era platform lessons | [Lecture-03.md](01-讲座/Lecture-03.md) |
| 4 | AI smart speaker design studio | [Lecture-04.md](01-讲座/Lecture-04.md) |

---

**Recommended study pattern**

For each lecture:

1. identify the product decision being discussed
2. map it to a real embedded consequence
3. ask what gets better and what gets worse
4. write one short product rule you would keep for your own design

Do not study this as abstract brand analysis. Study it as system design.

---

**What this course is not**

This course is not:

- UI design theory
- generic startup advice
- consumer-electronics history for its own sake

It is a practical product-thinking course for embedded engineers using a smart-speaker case study.

---

**Suggested output**

By the end of this mini-course, you should be able to write:

- a one-page product intent note
- a V1 hardware/product tradeoff list
- a trust and privacy design checklist
- a simple AI smart-speaker design brief

That is the right kind of artifact because it proves you can connect engineering choices to product behavior.

---

**Next:** [Lecture 01 - Why product design belongs in embedded systems](01-讲座/Lecture-01.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/4. Product Design/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/4.%20Product%20Design/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
