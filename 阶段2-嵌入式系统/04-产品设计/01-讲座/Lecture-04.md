---
title: 第 4 讲 - AI 智能音箱设计工作坊
description: 第 4 讲 - AI 智能音箱设计工作坊
---

# 第 4 讲 - AI 智能音箱设计工作坊

**课程：** [Product Design for Embedded Systems](../Guide.md) | **阶段 2 - 嵌入式系统**

**上一讲：** [Lecture 03](Lecture-03.md)

---

## 本讲目标

更早的几讲讲的是经验教训。

本讲把这些经验教训转化为一个**具体产品方向**。

示例产品是：

- 一款面向客厅的高端本地优先 AI 智能音箱

把它视为 HomePod 级别的房间物件，但具备：

- 更强的本地优先智能
- 更清晰的信任边界
- 更少的生态锁定

---

## 步骤 1：用一句话定义产品

糟糕的定义：

- 带大语言模型的智能音箱

更好的定义：

- 面向共享空间的本地家庭 AI 家电

这句话更强，因为它已经隐含：

- 房间摆放
- 家电行为
- 信任要求
- 家庭使用，而不只是个人使用

---

## 步骤 2：确定物件应给人什么感觉

对这个产品，目标感觉应是：

- 居家
- 平静
- 可信
- 高端
- 低调智能

它不应感觉像：

- 套壳开发套件
- 监控摄像头
- 游戏玩家外设
- 展示中的迷你 PC

这立即塑造**机械与工业设计选择**。

---

## 步骤 3：冻结物理身份

对 V1 AI 智能音箱，这些是有力的默认决策：

- 默认无摄像头
- 顶部麦克风阵列
- 下部扬声器系统
- 可见的物理静音控制
- 整洁的后下方 I/O 区
- 稳定的竖直房间物件形态

这些选择为何重要：

- 默认无摄像头让共享空间中的信任保持简单
- 顶部麦克风有助于远场拾音
- 下部扬声器有助于声学隔离和稳定性
- 物理静音为隐私声明提供硬件层面的真实依据
- 隐藏 I/O 减少可见杂乱

这就是**产品设计转化为硬件布局**。

---

## 步骤 4：划分内部区域

一种优秀的智能音箱布局采用如下垂直堆叠：

1. **顶部交互区**
   - 麦克风
   - 静音
   - 音量
   - 状态灯

2. **上部安静区**
   - 麦克风周围的空气间隙和屏蔽

3. **中部计算与热管理区**
   - SoM 或主计算板
   - 载板 / 主板
   - 散热器
   - 气流路径

4. **下部声学区**
   - 低音单元
   - 高音单元
   - 声学腔体

5. **后下方维护区**
   - 电源
   - 维护 I/O
   - 线缆出口

这样为何有效：

- 麦克风远离风扇湍流
- 扬声器振动远离麦克风罩
- 重部件保持足够低以维持稳定
- 物件有家电感，而非暴露内部

---

## 步骤 5：让信任可见

对家庭 AI 设备，信任不能**只依赖手机 app**。

产品需要可见的真实依据。

这意味着：

- 物理静音
- 静音状态全房间可见
- 聆听状态清晰可见
- 响应 / 思考状态清晰可见

如果用户需要猜测设备是否在工作，产品就是弱的。

这就是为什么**物理静音开关**不是一个微小的 UX 细节。它是一个**核心产品架构决策**。

---

## 步骤 6：选择合适的音频目标

对 V1 智能音箱，目标不应是：

- 发烧友炫耀资本

目标应是：

- 明显高端的语音可懂度
- 可信的客厅音频
- 强远场交互

一个合理产品方向是：

- `1 woofer`
- `2 angled tweeters`
- 顶部麦克风阵列
- 主动或计算调音思路

这比以下方案带来更好的产品故事：

- 尽可能便宜的单声道扬声器

但它也避免假装成：

- 高保真塔式音箱

产品首先是家庭 AI 家电，而不是发烧友展示品。

---

## 步骤 7：确定智能边界

好的 V1 产品不应感觉像**别人大脑的外壳**。

对本地优先 AI 音箱，一个强划分是：

### 设备应拥有

- 唤醒词
- 对话策略
- 记忆策略
- 响应风格
- 隐私控制
- 本地智能编排

### 智能家居平台应拥有

- 设备图
- 自动化
- 集成
- 场景
- 长尾兼容性

这意味着像 Home Assistant 这样的平台应增强能力，但不定义产品身份。

用户应感觉到：

- 一个连贯的家电

而不是：

- 另一系统上的语音皮肤

---

## 步骤 8：定义 V1 非目标

强产品常常同样由**它们拒绝什么**定义，也由它们包含什么定义。

对这个 AI 智能音箱 V1，好的非目标是：

- 默认无摄像头
- 无屏幕优先身份
- 核心智能不依赖附近的另一设备
- 不要求理解原始智能家居实体名称
- 无生态牢笼

这保护产品不变得模糊。

---


<details>
<summary>English original</summary>

**Lecture 4 - AI smart speaker design studio**

**Course:** [Product Design for Embedded Systems](../Guide.md) | **Phase 2 - Embedded Systems**

**Previous:** [Lecture 03](Lecture-03.md)

---

**The goal of this lecture**

The earlier lectures were about lessons.

This lecture turns those lessons into a **concrete product direction**.

The example product is:

- a premium local-first AI smart speaker for the living room

Think of it as a HomePod-class room object, but with:

- stronger local-first intelligence
- clearer trust boundaries
- less ecosystem lock-in

---

**Step 1: define the product in one sentence**

Bad definition:

- smart speaker with LLM

Better definition:

- local home AI appliance for shared space

That one sentence is stronger because it already implies:

- room placement
- appliance behavior
- trust requirements
- household use, not only solo use

---

**Step 2: decide what the object should feel like**

For this product, the target feeling should be:

- domestic
- calm
- trustworthy
- premium
- quietly intelligent

It should not feel like:

- a dev kit in a shell
- a surveillance camera
- a gamer gadget
- a mini PC on display

This immediately shapes **mechanical and industrial choices**.

---

**Step 3: freeze the physical identity**

For a V1 AI smart speaker, these are strong default decisions:

- no camera by default
- top microphone array
- lower speaker system
- visible physical mute control
- clean rear-bottom I/O zone
- stable vertical room-object form

Why these choices matter:

- no camera by default keeps trust simple in shared space
- top microphones help far-field pickup
- lower speakers help acoustic separation and stability
- physical mute gives hardware truth to the privacy claim
- hidden I/O reduces visible clutter

This is **product design turning into hardware layout**.

---

**Step 4: separate the internal zones**

One strong smart-speaker layout uses a vertical stack like this:

1. **Top interaction zone**
   - microphones
   - mute
   - volume
   - status lighting

2. **Upper quiet zone**
   - air gap and shielding around the microphones

3. **Middle compute and thermal zone**
   - SoM or main compute board
   - carrier / main board
   - heatsink
   - airflow path

4. **Lower acoustic zone**
   - woofer
   - tweeters
   - acoustic chamber

5. **Rear-bottom service zone**
   - power
   - service I/O
   - cable exit

Why this works:

- microphones stay away from fan turbulence
- speaker vibration stays away from the mic cap
- heavy parts stay low enough for stability
- the object feels appliance-like instead of exposed

---

**Step 5: make trust visible**

For a home AI device, trust cannot depend **only on the phone app**.

The product needs visible truth.

That means:

- physical mute
- mute state visible across the room
- listening state clearly visible
- response / thinking state clearly visible

If users need to guess whether the device is live, the product is weak.

That is why a **physical mute switch** is not a tiny UX detail. It is a **core product architecture decision**.

---

**Step 6: choose the right audio ambition**

For a V1 smart speaker, the goal should not be:

- audiophile bragging rights

The goal should be:

- clearly premium spoken intelligibility
- believable living-room audio
- strong far-field interaction

A sensible product direction is:

- `1 woofer`
- `2 angled tweeters`
- top mic array
- active or computational tuning mindset

This gives a better product story than:

- cheapest possible mono speaker

But it also avoids pretending to be:

- a hi-fi tower

The product is a home AI appliance first, not an audiophile showcase.

---

**Step 7: decide the intelligence boundary**

A good V1 product should not feel like a **shell around someone else's brain**.

For a local-first AI speaker, a strong split is:

**The device should own**

- wake word
- conversation policy
- memory policy
- response style
- privacy controls
- local intelligence orchestration

**The smart-home platform should own**

- device graph
- automations
- integrations
- scenes
- long-tail compatibility

That means a platform like Home Assistant should increase power, but not define product identity.

The user should feel:

- one coherent appliance

not:

- a voice skin over another system

---

**Step 8: define the V1 non-goals**

Strong products are often defined as much by **what they refuse** as by what they include.

For this AI smart-speaker V1, good non-goals are:

- no camera by default
- no screen-first identity
- no dependence on another nearby device for core intelligence
- no requirement to understand raw smart-home entity names
- no ecosystem prison

This protects the product from becoming blurry.

---

</details>

## 一份好的 V1 产品简报

如果把整讲压缩成一份简报，就变成：

**产品角色**
- 面向客厅的 local-first 家庭 AI 设备

**主要优势**
- 信任
- 语音交互
- 房间在场感
- 家庭实用性

**物理规则**
- 顶部麦克风阵列
- 下方扬声器系统
- 可见的静音
- 沉稳的物体
- 默认无摄像头

**软件规则**
- local-first 的核心行为
- 兼容 Home Assistant，但不依赖它
- 在生态扩展之前就已可用

**要避免的产品风险**
- 高端硬件，日常实用性却很弱

---

## 最终要点

正确的嵌入式产品问题不是：

- 我们能造出这个东西吗？

更好的问题是：

- 如果我们造出这个东西，它在用户家里会变成怎样一种物体？

这才是 **embedded engineering 与产品设计之间**真正的桥梁。

---

## 实验

为你自己的 AI 智能音箱概念写一份一页纸的 V1 设计简报。

它必须包含：

1. 一句话产品角色
2. 目标房间或 placement
3. 信任模型
4. 物理控制清单
5. 内部区域布局
6. local 与 external 的软件边界
7. 三个 V1 non-goals

如果你能把这些写清楚，你就不再只是在描述一台设备。你是在描述一款产品。

---

**上一讲：** [Lecture 03](Lecture-03.md)


<details>
<summary>English original</summary>

**A good V1 product brief**

If you compress the whole lecture into one brief, it becomes:

**Product role**
- local-first home AI appliance for the living room

**Primary strengths**
- trust
- voice interaction
- room presence
- household usefulness

**Physical rules**
- top mic array
- lower speaker system
- visible mute
- calm object
- no camera by default

**Software rules**
- local-first core behavior
- Home Assistant-compatible but not dependent
- useful before ecosystem expansion

**Product risk to avoid**
- premium hardware with weak everyday usefulness

---

**Final takeaway**

The right embedded-product question is not:

- can we build this?

The better question is:

- if we build this, what kind of object will it become in the user's home?

That is the real bridge between **embedded engineering and product design**.

---

**Lab**

Write a one-page V1 design brief for your own AI smart-speaker concept.

It must include:

1. one-sentence product role
2. target room or placement
3. trust model
4. physical control list
5. internal zone layout
6. local vs external software boundary
7. three V1 non-goals

If you can write that clearly, you are no longer just describing a device. You are describing a product.

---

**Previous:** [Lecture 03](Lecture-03.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/4. Product Design/Lecture/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/4.%20Product%20Design/Lecture/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
