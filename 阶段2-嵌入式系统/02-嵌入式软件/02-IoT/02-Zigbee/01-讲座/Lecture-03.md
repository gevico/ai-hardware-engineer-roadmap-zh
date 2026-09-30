---
title: 第 3 讲 - Zigbee 协议栈：ZDO、APS、端点与簇
description: 第 3 讲 - Zigbee 协议栈：ZDO、APS、端点与簇
---

# 第 3 讲 - Zigbee 协议栈：ZDO、APS、端点与簇

**课程：** [Zigbee guide](../Guide.md) | **阶段 2 - Embedded Software, IoT**

**上一讲：** [第 02 讲 - 角色、拓扑与网络组建](Lecture-02.md) | **下一讲：** [第 04 讲 - 安全、入网调试、休眠设备与 OTA](Lecture-04.md)

---

## 为什么这一讲感觉很难

在这一讲，Zigbee 常常**不再让人觉得简单**。

到目前为止，故事都还容易：

- 设备加入网络
- 一些设备负责路由
- 一些设备休眠

然后 Zigbee 开始抛出一堆词，比如：

- `NWK`
- `APS`
- `ZDO`
- `ZCL`
- 端点
- 簇

于是所有东西听起来都像规范语言。

所以本讲自始至终只用一个简单类比。

官方参考：[Silicon Labs The Zigbee Stack](https://docs.silabs.com/zigbee/8.2.1/zigbee-fundamentals/05-the-zigbee-stack)

---

## 想象一场大型家庭派对

想象你正在办一场大型家庭派对。

你的房子已经挤满了人。

你的朋友们分散在：

- 你的房子
- 院子
- 车库
- 邻居家

有些人帮忙传递消息。
有些人只是露个面、听听就走。
有些人则负责统筹整个活动。

这正是理解 Zigbee 的一个好心智模型。

现在想象你希望促成一件事：

- 你按下墙壁开关
- 智能灯泡亮起

要做到这一点，系统必须回答四个问题：

1. **消息怎么送到那里？**
2. **究竟该由谁接收？**
3. **这条消息是什么意思？**
4. **谁负责什么，谁又是谁？**

这四个问题对应：

- `NWK` = 怎么到达那里？
- `APS` = 究竟该由谁接收？
- `ZCL` = 这条消息是什么意思？
- `ZDO` = 你是谁，规则又是什么？

只要记住这组对应关系，Zigbee 剩下的部分就会好读得多。

---

## 一张图看懂协议栈

从高层看：

- **PHY / MAC** 来自 IEEE 802.15.4
- **NWK** 负责网络行为与路由
- **APS** 负责面向应用的投递
- **ZDO** 负责设备身份、发现与管理
- **ZCL** 定义通用的应用行为

![展示 PHY、MAC、NWK 与应用层组件的 Zigbee 架构](../../../../../Assets/images/zigbee-architecture-overview.png)

这张图有用，因为它展示了两个重要事实：

- `PHY` 与 `MAC` 是来自 IEEE 802.15.4 的无线基础
- Zigbee 增加了更高层的组件，让设备不仅可达，而且可理解

来源：[Electrical Technology ZigBee Architecture diagram](https://www.electricaltechnology.org/wp-content/uploads/2017/07/ZigBee-Architecture.png)

---

## 认识 Zigbee 派对团队

### 1. `NWK` - 社区地图与走廊巡查员

`NWK` 意为**网络层**。

在派对上，它相当于：

- 知道地图的人
- 知道哪栋房子连哪栋的人
- 沿最优路线传递纸条的人

他们的职责是：

- 知道通过哪台路由器可以到达哪台设备
- 逐跳转发消息
- 在某个路由失效时恢复

所以如果一条消息要这样传递：

- Switch -> Router A -> Router B -> Bulb

这正是 `NWK` 在发挥作用。

简单来说：

- `NWK` 回答：**怎么到达那里？**

---

### 2. `APS` - 收发室与通讯录

`APS` 意为**应用支持子层**。

在派对上，它相当于：

- 收发室的工作人员
- 拿着通讯录的人
- 确保纸条送到正确房间里的正确人手上的人

这一点很重要，因为仅仅送到正确的房子还不够。

你还需要送到：

- 正确的设备
- 该设备内部正确的功能

这就是 `APS` 与以下内容紧密配合的原因：

- 端点
- 绑定
- 投递行为

简单来说：

- `APS` 回答：**究竟该由谁接收？**

---

### 3. `ZCL` - 派对上的通用俚语

`ZCL` 意为 **Zigbee Cluster Library**。

在派对上，它相当于：

- 人人都懂的通用俚语

你不会希望每个品牌的灯泡和开关都说各自的私密语言。

你想要的是一种共同语言，比如：

- `On`
- `Off`
- `Toggle`
- `Move to Level`

这正是 `ZCL` 提供的东西。

它定义通用行为，让不同厂商仍能互通协作。

简单来说：

- `ZCL` 回答：**这条消息是什么意思？**

---

### 4. `ZDO` - 主人与校长

`ZDO` 意为 **Zigbee Device Object**。

在派对上，它相当于：

- 主人
- 校长
- 核对宾客名单的人
- 给人做介绍的人
- 决定谁扮演什么角色的人

`ZDO` 处理这类问题：

- 你是谁？
- 你是 Coordinator、Router 还是 End Device？
- 你有哪些端点？
- 你属于哪类设备？
- 你能做什么？

这属于管理与发现，而不是普通用户命令。

简单来说：

- `ZDO` 回答：**你是谁，规则又是什么？**

---

## 最重要的几个支撑术语

在展开完整故事之前，你还需要了解三个术语。


<details>
<summary>English original</summary>

**Lecture 3 - The Zigbee stack: ZDO, APS, endpoints, and clusters**

**Course:** [Zigbee guide](../Guide.md) | **Phase 2 - Embedded Software, IoT**

**Previous:** [Lecture 02 - Roles, topology, and network formation](Lecture-02.md) | **Next:** [Lecture 04 - Security, commissioning, sleepy devices, and OTA](Lecture-04.md)

---

**Why this lecture feels hard**

This is the lecture where Zigbee often **stops feeling simple**.

Up to now, the story is easy:

- devices join a network
- some devices route
- some devices sleep

Then Zigbee starts throwing around words like:

- `NWK`
- `APS`
- `ZDO`
- `ZCL`
- endpoint
- cluster

and everything starts to sound like spec language.

So this lecture uses one simple analogy all the way through.

Official reference: [Silicon Labs The Zigbee Stack](https://docs.silabs.com/zigbee/8.2.1/zigbee-fundamentals/05-the-zigbee-stack)

---

**Imagine a huge house party**

Imagine you are throwing a huge house party.

Your house is full.

Your friends are spread across:

- your house
- the yard
- the garage
- your neighbor's house

Some people help pass messages around.
Some people just show up, listen, and leave.
Some people are in charge of the whole event.

That is a good mental model for Zigbee.

Now imagine you want one thing to happen:

- you press the wall switch
- the smart bulb turns on

To make that happen, the system has to answer four questions:

1. **How do we get the message there?**
2. **Who exactly should receive it?**
3. **What does the message mean?**
4. **Who is in charge, and who is who?**

Those four questions map to:

- `NWK` = how do we get there?
- `APS` = who exactly should receive it?
- `ZCL` = what does the message mean?
- `ZDO` = who are you, and what are the rules?

If you remember only that mapping, the rest of Zigbee becomes much easier to read.

---

**The stack in one picture**

At a high level:

- **PHY / MAC** come from IEEE 802.15.4
- **NWK** handles network behavior and routing
- **APS** handles application-oriented delivery
- **ZDO** handles device identity, discovery, and management
- **ZCL** defines common application behavior

![Zigbee architecture showing PHY, MAC, NWK, and application-layer pieces](../../../../../Assets/images/zigbee-architecture-overview.png)

This picture is useful because it shows two important facts:

- `PHY` and `MAC` are the radio foundation from IEEE 802.15.4
- Zigbee adds higher-level pieces that make devices understandable, not just reachable

Source: [Electrical Technology ZigBee Architecture diagram](https://www.electricaltechnology.org/wp-content/uploads/2017/07/ZigBee-Architecture.png)

---

**Meet the Zigbee party crew**

**1. `NWK` - the neighborhood map and hall monitors**

`NWK` means **Network Layer**.

At the party, this is:

- the people who know the map
- the people who know which houses connect to which
- the people who pass notes through the best route

Their job is:

- know which device is reachable through which router
- forward the message hop by hop
- recover if one route stops working

So if a message has to travel like this:

- Switch -> Router A -> Router B -> Bulb

that is `NWK` doing its job.

Simple version:

- `NWK` answers: **how do we get there?**

---

**2. `APS` - the mailroom and the address book**

`APS` means **Application Support Sublayer**.

At the party, this is:

- the mailroom staff
- the people holding the address book
- the people who make sure the note goes to the right person in the right room

This matters because reaching the right house is not enough.

You also need to reach:

- the right device
- the right function inside that device

That is why `APS` works closely with:

- endpoints
- binding
- delivery behavior

Simple version:

- `APS` answers: **who exactly should receive this?**

---

**3. `ZCL` - the shared party slang**

`ZCL` means **Zigbee Cluster Library**.

At the party, this is:

- the shared slang everybody understands

You do not want every brand of bulb and switch speaking a different private language.

You want a common language like:

- `On`
- `Off`
- `Toggle`
- `Move to Level`

That is what `ZCL` gives you.

It defines common behavior so different vendors can still cooperate.

Simple version:

- `ZCL` answers: **what does this message mean?**

---

**4. `ZDO` - the host and the principal**

`ZDO` means **Zigbee Device Object**.

At the party, this is:

- the host
- the principal
- the person checking the guest list
- the person introducing people
- the person deciding who plays which role

`ZDO` handles questions like:

- who are you?
- are you a Coordinator, Router, or End Device?
- what endpoints do you have?
- what kind of device are you?
- what can you do?

This is management and discovery, not normal user commands.

Simple version:

- `ZDO` answers: **who are you, and what are the rules?**

---

**The most important support words**

Before the full story, you need three more terms.

</details>

### 节点

**节点**是物理设备。

示例：

- 一个灯泡
- 一个墙壁开关
- 一个传感器

### 端点

**端点**是设备内部的逻辑应用实例。

一个物理节点可以暴露：

- 一个用于照明的端点
- 另一个用于传感的端点

因此：

- 节点 = 派对上的整个人
- 端点 = 那个人正在扮演的具体角色

### 簇

**簇**是可复用的功能块。

示例：

- `On/Off`
- `Level Control`
- `Temperature Measurement`

因此：

- 端点 = 哪个应用实例
- 簇 = 哪个功能

---

## 属性和命令

在一个簇内部，通常关注两件事：

- **属性** = 状态值
- **命令** = 动作

示例：

- 一个 `On/Off` 簇可能支持类似 `On`、`Off` 和 `Toggle` 的命令
- 一个温度簇可能暴露类似 `Measured Value` 的属性

这是日常 Zigbee 模型：

- 读属性
- 写属性
- 发送命令

这比只考虑以下内容更有用：

- 发送数据包
- 接收数据包

---

## 完整派对流程

现在走查一个简单事件：

- 你按下 Zigbee 墙壁开关
- 一个 Zigbee 灯泡打开

### Step 1: 设备已经在派对现场

网络已经存在。

Coordinator 之前组建了它。

开关已加入。

灯泡已加入。

所以现在所有设备都“在派对里”。

### Step 2: `ZDO` 帮助所有设备理解谁是谁

在有用的控制发生之前，设备需要标识与发现信息。

系统需要了解类似以下内容：

- 这是灯泡
- 这是开关
- 灯泡暴露一个照明端点
- 灯泡支持 `On/Off` 簇

这属于 `ZDO` 的范畴。

它不是在说：

- 打开灯

它是在说：

- 你是谁，你支持什么？

### Step 3: 目标端点已知

假设灯泡暴露：

- 用于照明的端点 `1`

现在系统不仅知道：

- 哪个节点是灯泡

还知道：

- 灯泡内部哪个应用实例应接收照明消息

### Step 4: 正确的簇已知

灯泡的端点 `1` 支持：

- `On/Off`

现在系统知道：

- 正确的节点
- 正确的端点
- 正确的簇

### Step 5: 开关创建一个 `ZCL` 命令

用户按下开关。

开关创建标准应用命令：

- `On`

那是一个 `ZCL` 含义。

它不只是原始字节。

它是具有共享含义的标准命令。

### Step 6: `APS` 正确寻址它

现在 `APS` 接管。

`APS` 确保命令被定向到：

- 正确的目标节点
- 正确的目标端点
- 正确的应用目标

这部分将：

- “把它发到某处”

转换为：

- “把这条照明命令发送到灯泡的灯光控制端点”

### Step 7: `NWK` 通过 mesh 为其布线

如果灯泡不能直接到达，`NWK` 会找到路径。

例如：

- Switch -> Router A -> Router B -> Bulb

这就是布线过程。

`NWK` 不决定 `On` 的含义。

它只确保消息在网络中传输。

### Step 8: 灯泡理解命令

灯泡接收消息。

其应用侧看到：

- 端点 = `1`
- 簇 = `On/Off`
- 命令 = `On`

现在灯泡打开。

整个流程非常清楚地展示了层间拆分：

- `ZDO` 帮助识别设备与能力
- `APS` 确保消息到达正确的应用目标
- `NWK` 通过 mesh 将消息送达那里
- `ZCL` 赋予命令含义

---

## 绑定：保存的关系

**绑定**就像在派对通讯录中保存一段关系。

不必每次都问：

- 这个开关应该控制哪个灯泡？

系统可以记住：

- 这个开关端点绑定到那个灯泡端点

这使控制更简洁，并减少重复查找工作。

简单版本：

- 绑定 = **记住谁与谁通信**

---

## 组：一条消息发给多个设备

**组**就像派对群聊。

不必发送：

- 一条命令给灯泡 A
- 一条命令给灯泡 B
- 一条命令给灯泡 C

可以发送：

- 一条命令给“客厅灯”组

然后该组中的所有灯泡都响应。

简单版本：

- 组 = **一条消息发给多个设备**

---

## 需要避免的常见混淆

### 混淆 1：`ZDO` 和 `ZCL` 听起来相似

它们做的事情不同。

- `ZDO` = 标识、发现、管理
- `ZCL` = 命令、属性、应用含义

一个问：

- 你是谁？

另一个说：

- 打开

### 混淆 2：人们跳过 `APS`

初学者常认为：

- 网络发送消息
- 应用接收它

但 Zigbee 比那更有结构。

`APS` 很重要，因为它处理到正确应用目标的投递。

### 混淆 3：节点和端点不同

- 节点 = 物理设备
- 端点 = 设备内部的逻辑应用

一个物理设备可以有多个端点。

---


<details>
<summary>English original</summary>

**Node**

A **node** is the physical device.

Examples:

- one bulb
- one wall switch
- one sensor

**Endpoint**

An **endpoint** is a logical application instance inside the device.

One physical node can expose:

- one endpoint for lighting
- another endpoint for sensing

So:

- node = the whole person at the party
- endpoint = the specific role that person is playing

**Cluster**

A **cluster** is a reusable feature block.

Examples:

- `On/Off`
- `Level Control`
- `Temperature Measurement`

So:

- endpoint = which application instance
- cluster = which feature

---

**Attributes and commands**

Inside a cluster, you usually care about two things:

- **attributes** = state values
- **commands** = actions

Examples:

- an `On/Off` cluster may support commands like `On`, `Off`, and `Toggle`
- a temperature cluster may expose an attribute like `Measured Value`

This is the daily Zigbee model:

- read an attribute
- write an attribute
- send a command

That is more useful than thinking only:

- send packet
- receive packet

---

**The full party flow**

Now let us walk through one simple event:

- you press a Zigbee wall switch
- a Zigbee bulb turns on

**Step 1: the devices are already at the party**

The network already exists.

The Coordinator formed it earlier.

The switch joined.

The bulb joined.

So now everyone is "in the party."

**Step 2: `ZDO` helps everyone understand who is who**

Before useful control happens, devices need identity and discovery information.

The system needs to learn things like:

- this is the bulb
- this is the switch
- the bulb exposes a lighting endpoint
- the bulb supports the `On/Off` cluster

This is `ZDO` territory.

It is not saying:

- turn on the light

It is saying:

- who are you and what do you support?

**Step 3: the target endpoint is known**

Suppose the bulb exposes:

- endpoint `1` for lighting

Now the system knows not only:

- which node is the bulb

but also:

- which application instance inside the bulb should receive the lighting message

**Step 4: the correct cluster is known**

The bulb's endpoint `1` supports:

- `On/Off`

Now the system knows:

- the right node
- the right endpoint
- the right cluster

**Step 5: the switch creates a `ZCL` command**

The user presses the switch.

The switch creates the standard application command:

- `On`

That is a `ZCL` meaning.

It is not just raw bytes.

It is a standard command with shared meaning.

**Step 6: `APS` addresses it properly**

Now `APS` takes over.

`APS` makes sure the command is directed to:

- the correct destination node
- the correct destination endpoint
- the correct application target

This is the part that turns:

- "send this somewhere"

into:

- "send this lighting command to the bulb's light-control endpoint"

**Step 7: `NWK` routes it through the mesh**

If the bulb is not directly reachable, `NWK` finds the path.

For example:

- Switch -> Router A -> Router B -> Bulb

That is the routing story.

`NWK` does not decide what `On` means.

It only makes sure the message travels across the network.

**Step 8: the bulb understands the command**

The bulb receives the message.

Its application side sees:

- endpoint = `1`
- cluster = `On/Off`
- command = `On`

Now the bulb turns on.

That whole flow shows the layer split very clearly:

- `ZDO` helped identify devices and capabilities
- `APS` made sure the message reached the right application target
- `NWK` got the message there through the mesh
- `ZCL` gave the command meaning

---

**Binding: the saved relationship**

**Binding** is like keeping a saved relationship in the party address book.

Instead of asking every time:

- which bulb should this switch control?

the system can remember:

- this switch endpoint is bound to that bulb endpoint

That makes control cleaner and reduces repeated lookup work.

Simple version:

- binding = **remember who talks to whom**

---

**Groups: one message for many devices**

**Groups** are like a party group chat.

Instead of sending:

- one command to bulb A
- one command to bulb B
- one command to bulb C

you can send:

- one command to the "living room lights" group

Then all bulbs in that group respond.

Simple version:

- group = **one message for many devices**

---

**Common confusion to avoid**

**Confusion 1: `ZDO` and `ZCL` sound similar**

They are not doing the same job.

- `ZDO` = identity, discovery, management
- `ZCL` = commands, attributes, application meaning

One asks:

- who are you?

The other says:

- turn on

**Confusion 2: people skip `APS`**

Beginners often think:

- the network sends the message
- the application receives it

But Zigbee is more structured than that.

`APS` is important because it handles delivery to the correct application target.

**Confusion 3: node and endpoint are not the same**

- node = physical device
- endpoint = logical application inside the device

One physical device can have multiple endpoints.

---

</details>

## 一句话总结

对于 Zigbee：

- `NWK` = **我们怎么到达那里？**
- `APS` = **到底谁应该收到这条消息？**
- `ZCL` = **这条消息是什么意思？**
- `ZDO` = **你是谁，规则又是什么？**

这就是整讲内容的一句话总结。

---

## 为什么嵌入式工程师要关心

在固件中，Zigbee 不只是：

- 初始化射频
- 加入网络
- 发送字节

它更像是：

- 定义端点
- 选择簇
- 暴露属性
- 支持命令
- 管理设备之间的关系

这就是为什么 Zigbee 更像是在构建一个**结构化的设备模型**，而不只是把比特推过一条传输通道。

---

## 实验

拿一个设备套用聚会的类比：

- 智能灯泡
- 墙壁开关
- 温度传感器

写下：

1. 物理节点是什么
2. 它可能有一个还是多个端点
3. 哪个簇最重要
4. 它可能支持的一个属性或命令
5. `ZDO` 能帮助发现关于它的一件事
6. `APS` 起作用的一个地方
7. `NWK` 起作用的一个地方

如果你能用这七个步骤解释你的设备，那么 Zigbee 协议栈就开始变得实用，而不再是抽象的。

---

**上一讲：** [第 02 讲 - 角色、拓扑与网络组建](Lecture-02.md) | **下一讲：** [第 04 讲 - 安全、入网配置、休眠设备与 OTA](Lecture-04.md)


<details>
<summary>English original</summary>

**In one nutshell**

For Zigbee:

- `NWK` = **how do we get there?**
- `APS` = **who exactly should receive this?**
- `ZCL` = **what does the message mean?**
- `ZDO` = **who are you, and what are the rules?**

That is the whole lecture in one summary.

---

**Why embedded engineers should care**

In firmware, Zigbee is not just:

- initialize radio
- join network
- send bytes

It is more like:

- define endpoints
- pick clusters
- expose attributes
- support commands
- manage relationships between devices

That is why Zigbee feels more like building a **structured device model** than just pushing bits through a transport.

---

**Lab**

Use the party analogy on one device:

- smart bulb
- wall switch
- temperature sensor

Write down:

1. what the physical node is
2. what endpoint or endpoints it likely has
3. which cluster matters most
4. one attribute or command it likely supports
5. one thing `ZDO` would help discover about it
6. one place where `APS` matters
7. one place where `NWK` matters

If you can explain your device using those seven steps, then the Zigbee stack is starting to become practical instead of abstract.

---

**Previous:** [Lecture 02 - Roles, topology, and network formation](Lecture-02.md) | **Next:** [Lecture 04 - Security, commissioning, sleepy devices, and OTA](Lecture-04.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/IoT/Zigbee/Lecture/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/IoT/Zigbee/Lecture/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
