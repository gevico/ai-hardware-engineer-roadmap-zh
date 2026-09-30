---
title: 第 1 讲 - 为什么产品设计属于嵌入式系统
description: 第 1 讲 - 为什么产品设计属于嵌入式系统
---

# 第 1 讲 - 为什么产品设计属于嵌入式系统

**课程：** [Product Design for Embedded Systems](../Guide.md) | **阶段 2 - 嵌入式系统**

**下一讲：** [第 02 讲 - HomePod 2018 的教训：只有好音质是不够的](Lecture-02.md)

---

## 从错误的心智模型开始

许多工程师把 **产品设计** 当作真正的工程工作完成之后才开始的事。

这种错误的心智模型听起来是这样的：

- 硬件团队做出板子
- 固件团队让它启动
- 软件团队让功能跑起来
- 之后再找人「设计产品」

优秀的嵌入式产品**并不是这样做出来的**。

在真实系统中，产品设计出现得要早得多，体现在这类问题上：

- 这个物件应该放在桌面、墙上、架子上，还是厨房台面上？
- 它看起来应该像设备、家具，还是家电？
- 用户一眼看过去应该信任什么？
- 哪些状态必须能隔着房间看清？
- 麦克风、扬声器、通风孔和按键在物理上应该放在哪里？
- 在还没有任何云端或 app 配置之前，哪些功能就应该能用？

这些是产品问题，但它们同样会改变：

- PCB 形状
- 机械布局
- 散热路径
- 音频路径
- EMI 风险
- GPIO 与按键数量
- 软件职责边界

---

## AI 智能音箱示例

本课程以 **AI 智能音箱 / 家庭 AI 家电** 作为贯穿案例，因为它把许多产品与嵌入式问题逼到同一个物件上。

一台好的智能音箱不只是：

- 一台 Linux 盒子
- 一个麦克风阵列
- 一个扬声器单元
- 一个 LLM 端点

它同时还是：

- 一件房间里的物件
- 一件承载信任的物件
- 一件音频物件
- 一段配置体验
- 一套家庭行为系统

正因如此，智能音箱是非常有用的设计练习。

它们清楚地表明，**产品设计是系统设计的一部分**。

---

## 产品设计意味着塑造系统的决策

对嵌入式系统而言，产品设计通常指五个方面的决策。

### 1. 产品角色

这个东西本质上想成为什么？

示例：

- 开发板
- 家电
- 房间助手
- 便携工具
- 隐藏模块

这个选择**会改变下游的一切**。

客厅 AI 音箱不应该让人感觉像：

- 路由器
- 监控摄像头
- 套了外壳的迷你 PC

### 2. 信任模型

用户必须能立刻明白什么？

示例：

- 麦克风静音了吗？
- 设备正在听吗？
- 放在共享空间里安全吗？
- 它带摄像头吗？

如果信任只能靠 app UI 建立，这个产品就是弱的。

### 3. 物理交互模型

用户用物理方式能做什么？

示例：

- 静音
- 音量
- 配对
- 复位

**物理静音开关**不只是 UI 细节。它是用硬件实现的产品承诺。

### 4. 环境适配

产品放在哪里使用？

示例：

- 架子
- 床头柜
- 工厂车间
- 汽车仪表台

这会改变：

- 尺寸
- 朝向
- 出线位置
- 通风孔位置
- 声学调校
- 按键布局

### 5. 系统边界

哪些属于产品内部，哪些属于产品外部？

对智能音箱来说，这意味着这类问题：

- 什么是本地优先？
- 哪些依赖云端？
- 哪些由设备大脑负责？
- 哪些由 Home Assistant 或其它生态负责？

这些同样是产品设计。

---

## 工程师为什么要在意

如果忽视产品设计，得到的往往是**技术上令人印象深刻但很弱的物件**。

典型失效模式：

- 硬件不错，摆放别扭
- 软件强大，信任提示薄弱
- 功能强悍，配置费解
- 用料高端，手感廉价
- 声学出色，助手不好用

换句话说：

- 系统能跑
- 但产品输了

这正是本课程试图填补的差距。

---

## 一份简单的嵌入式产品检查清单

在真正动手做嵌入式产品之前，先问：

1. 它是为哪个房间或环境设计的？
2. 这个物件需要在物理上传达什么？
3. 即使软件降级，哪种用户操作也必须始终可用？
4. 设备应该在本地负责什么？
5. 哪种外部依赖会让产品显得更弱？

如果答不上来，说明工程定义仍然**不充分**。

---

## 本课程后续的贯穿案例

在接下来的各讲中，请使用这个设想中的产品：

- 高端本地优先 AI 智能音箱
- 客厅优先
- 远场麦克风
- 出色的语音播放音质
- 默认不带摄像头
- 可见的物理静音
- 在生态扩展之前就有用

HomePod 的教训和最终的设计工作坊，都要透过这个视角才能理解。

---

## 实验

写一小段文字回答：

- 你想设计什么样的嵌入式产品？
- 它放在哪里使用？
- 用户必须立刻信任什么？
- 哪一个物理控件无论如何都必须存在？

如果能清楚地回答这些问题，你已经在做产品设计，而不只是实现。

---

**下一讲：** [第 02 讲 - HomePod 2018 的教训：只有好音质是不够的](Lecture-02.md)


<details>
<summary>English original</summary>

**Lecture 1 - Why product design belongs in embedded systems**

**Course:** [Product Design for Embedded Systems](../Guide.md) | **Phase 2 - Embedded Systems**

**Next:** [Lecture 02 - HomePod 2018 lessons: great sound is not enough](Lecture-02.md)

---

**Start with the wrong mental model**

Many engineers treat **product design** as if it begins after the real engineering work is done.

That wrong mental model sounds like this:

- hardware team builds the board
- firmware team makes it boot
- software team makes the features work
- later someone "designs the product"

That is **not how good embedded products are made**.

In real systems, product design shows up much earlier in questions like:

- should this object live on a desk, wall, shelf, or kitchen counter?
- should it look like equipment, furniture, or an appliance?
- what should the user trust at a glance?
- which states must be visible across the room?
- where should microphones, speakers, vents, and buttons physically live?
- what should work before any cloud or app setup exists?

Those are product questions, but they also change:

- PCB shape
- mechanical layout
- thermal path
- audio path
- EMI risk
- GPIO and button count
- software ownership boundaries

---

**The AI smart-speaker example**

This course uses an **AI smart speaker / home AI appliance** as the running example because it forces many product and embedded questions into one object.

A good smart speaker is not only:

- a Linux box
- a microphone array
- a speaker driver
- an LLM endpoint

It is also:

- a room object
- a trust object
- an audio object
- a setup experience
- a household behavior system

That is why smart speakers are such a useful design exercise.

They make it obvious that **product design is part of system design**.

---

**Product design means system-shaping decisions**

For embedded systems, product design usually means decisions in five areas.

**1. Product role**

What is this thing fundamentally trying to be?

Examples:

- dev kit
- appliance
- room assistant
- portable tool
- hidden module

This choice **changes everything downstream**.

A living-room AI speaker should not feel like:

- a router
- a surveillance camera
- a mini PC in a shell

**2. Trust model**

What must the user be able to understand instantly?

Examples:

- is the microphone muted?
- is the device listening?
- is it safe in shared space?
- does it contain a camera?

If trust depends only on app UI, the product is weak.

**3. Physical interaction model**

What can the user do physically?

Examples:

- mute
- volume
- pairing
- reset

A **physical mute switch** is not just a UI detail. It is a product claim implemented in hardware.

**4. Environmental fit**

Where does the product live?

Examples:

- shelf
- bedside table
- factory floor
- car dashboard

This changes:

- size
- orientation
- cable exit
- vent location
- acoustic tuning
- button placement

**5. System boundary**

What belongs inside the product, and what belongs outside it?

For a smart speaker, that means questions like:

- what is local-first?
- what depends on cloud?
- what is owned by the device brain?
- what is owned by Home Assistant or another ecosystem?

That is product design too.

---

**Why engineers should care**

If you ignore product design, you often get a **technically impressive but weak object**.

Typical failure modes:

- good hardware, awkward placement
- powerful software, weak trust cues
- strong features, confusing setup
- premium parts, cheap-feeling object
- strong acoustics, weak assistant usefulness

In other words:

- the system works
- but the product loses

That is the exact gap this course is trying to fix.

---

**A simple embedded-product checklist**

Before building a real embedded product, ask:

1. What room or environment is this designed for?
2. What does the object need to signal physically?
3. What user action must always work even if software is degraded?
4. What should the device own locally?
5. What external dependency would make the product feel weaker?

If you cannot answer those, the engineering is still **underspecified**.

---

**Running example for the rest of this course**

For the rest of the lectures, use this imagined product:

- premium local-first AI smart speaker
- living-room first
- far-field microphones
- strong spoken audio
- no camera by default
- visible physical mute
- useful before ecosystem expansion

That is the lens through which the HomePod lessons and the final design studio will make sense.

---

**Lab**

Write a short paragraph answering:

- what kind of embedded product do you want to design?
- where does it live?
- what must users trust instantly?
- what is one physical control that should exist no matter what?

If you can answer that clearly, you are already doing product design, not just implementation.

---

**Next:** [Lecture 02 - HomePod 2018 lessons: great sound is not enough](Lecture-02.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/4. Product Design/Lecture/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/4.%20Product%20Design/Lecture/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
