---
title: Jetson 音频设置与开发 - 项目指南
description: Jetson 音频设置与开发 - 项目指南
---

# Jetson 音频设置与开发 - 项目指南

<div class="course-identity auto-course" style="--course-accent: #7e22ce; --course-accent-rgb: 126, 34, 206;" markdown="1">
<div class="course-identity__icon">JASA</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度解析 · Jetson 方向</p>
<p class="course-identity__title">Jetson 音频设置与开发 - 项目指南的专属课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成演示 · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


> **目标：** 把 Jetson 音频理解得足够透彻，从「USB 音频能用」走到「我能在 Jetson Orin Nano 上设计并调试真实的音频产品」，借助 NVIDIA 的 **ALSA + ASoC + APE/AHUB** 模型。

**Hub：** [Multimedia](../Guide.md)  
**主要官方来源：** [NVIDIA Jetson Linux Developer Guide - Audio Setup and Development](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/SD/Communications/AudioSetupAndDevelopment.html)  
**相关本地指南：** [Application Development](../../Guide.md) · [ESP32-LyraT I2S Microphone Capture](../01-ESP32-LyraT-I2S麦克风Jetson-Orin-Nano/Guide.md) · [Orin Nano GPIO / SPI / I2C / CAN](../../../01-Nvidia-Jetson平台/05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)

---

## 1. 为什么要有这门课程

NVIDIA 的指南非常出色，但它是作为**平台参考**写的，面向众多 Jetson 板卡和众多音频通路。

本课程刻意收窄范围：

- 目标板卡：**Jetson Orin Nano 8GB Developer Kit**
- 产品示例：**AI 智能音箱 / 语音设备**
- 学习目标：把整条软件栈理解得足够透彻，能够构建并调试音频系统

所以，本指南不把官方页面当成一整面堆满音频术语的墙，而是按项目里真正会问的问题重新组织：

- Jetson 音频作为一个系统长什么样？
- 官方文档里哪些章节对 Orin Nano 重要？
- 应该先用哪个：USB、DP 音频、DMIC，还是 I2S？
- 设备树和引脚复用要改什么？
- 如何正确地调试「声卡存在但没有声音」？

NVIDIA 官方指南仍是权威的底层参考。本课程是该材料的**路线图版本**。

---

## 2. 一句话心智模型

在 Jetson 上，音频通常是：

```text
app -> ALSA -> ASoC drivers -> ADMAIF/XBAR/I2S/DMIC inside AHUB -> real hardware
```

如果只记一件事，就记这一件：

> Jetson 音频不只是「一块声卡」。它是 SoC 内部的**路由网络**，外加一套必须正确连通之后声音才能流通的 Linux 驱动模型。

以智能音箱产品为例，通常的路径是：

- **原型阶段**
  - USB 麦克风阵列
  - USB 音箱或 DP 显示器音频
- **真实产品阶段**
  - 40-pin 排针上的 I2S codec 或功放
  - 也可能是数字麦克风或多通道外部前端

---

## 3. 面向 Jetson 产品的 ASoC 驱动

这是 NVIDIA 指南里的第一个大章节，也是正确的起点。

### NVIDIA 指的是什么

NVIDIA 使用标准的 Linux **ALSA** 框架，再叠加 Jetson 特有的 **ASoC** 驱动，使 ALSA 能够控制 Jetson 的内部音频硬件以及你接入的任何外部 codec。

### 这对你意味着什么

当你运行：

```bash
aplay sound.wav
```

你并不是在直接跟硬件对话。

你经过的是：

- ALSA 用户态工具与库
- ALSA kernel 框架
- ASoC 驱动
- Jetson 内部音频硬件
- 最后才到真正的接口，如 DP、USB、I2S 或 DMIC

### 本节的重要子主题

#### ALSA

ALSA 是标准的 Linux 音频系统。

你会用这些工具接触它：

- `aplay`
- `arecord`
- `amixer`

这些是你最初用来查明存在哪些设备、以及基本播放或采集是否可用的工具。

#### DAPM

**动态音频电源管理（Dynamic Audio Power Management）**听起来抽象，但实际含义很简单：

- ALSA 维护一张音频通路的图
- 只有需要的模块才应上电
- 如果通路不完整，播放或采集实际上不会发生

这就是为什么「声卡存在」还不够。路由也必须有效。

#### 设备树

设备树告诉 Linux：

- 存在哪个 codec
- 由哪条 I2C 或 SPI 总线控制它
- 连接的是哪个 I2S 控制器
- 声卡的路由和 widget 长什么样

对 Jetson 音频工作而言，设备树不是可选项。它是常规 bring-up（上电点亮/调通）的一部分。

#### ASoC 驱动

NVIDIA 遵循标准的 ASoC 划分：

- **platform 驱动**
- **codec 驱动**
- **machine 驱动**

这一划分很重要，因为它告诉你在出问题时该往哪里查。

### 落到产品设计上

如果你在做 AI 智能音箱，这一节基本上是在告诉你：

- 用户态音频工具只是最上面一层
- 路由是动态的，不是硬连线的
- 你的 codec 和板级描述必须正确
- 除非 ASoC 这一侧也正确，否则「在 Linux 上能用」并不等于「在 Jetson 上能用」

---


<details>
<summary>English original</summary>

**Jetson Audio Setup and Development - Project Guide**

<div class="course-identity auto-course" style="--course-accent: #7e22ce; --course-accent-rgb: 126, 34, 206;" markdown="1">
<div class="course-identity__icon">JASA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Jetson Audio Setup and Development - Project Guide.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Goal:** Understand Jetson audio well enough to move from "USB audio works" to "I can design and debug a real audio product on Jetson Orin Nano," using NVIDIA's **ALSA + ASoC + APE/AHUB** model.

**Hub:** [Multimedia](../Guide.md)  
**Primary official source:** [NVIDIA Jetson Linux Developer Guide - Audio Setup and Development](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/SD/Communications/AudioSetupAndDevelopment.html)  
**Related local guides:** [Application Development](../../Guide.md) · [ESP32-LyraT I2S Microphone Capture](../01-ESP32-LyraT-I2S麦克风Jetson-Orin-Nano/Guide.md) · [Orin Nano GPIO / SPI / I2C / CAN](../../../01-Nvidia-Jetson平台/05-Orin-Nano-GPIO-SPI-I2C-CAN/Guide.md)

---

**1. Why this course exists**

The NVIDIA guide is excellent, but it is written as a **platform reference** for many Jetson boards and many audio paths.

This course is narrower on purpose:

- target board: **Jetson Orin Nano 8GB Developer Kit**
- product example: **AI smart speaker / voice appliance**
- learning goal: understand the stack well enough to build and debug audio systems

So instead of treating the official page as one long wall of audio terms, this guide reorganizes it into the questions you actually ask during a project:

- what does Jetson audio look like as a system?
- which official sections matter for Orin Nano?
- what should I use first: USB, DP audio, DMIC, or I2S?
- what has to change in device tree and pinmux?
- how do I debug "card exists but no sound" properly?

The official NVIDIA guide remains the authoritative low-level reference. This course is the **roadmap version** of that material.

---

**2. The one-sentence mental model**

On Jetson, audio is usually:

```text
app -> ALSA -> ASoC drivers -> ADMAIF/XBAR/I2S/DMIC inside AHUB -> real hardware
```

If you remember only one thing, remember this:

> Jetson audio is not just "a sound card." It is a **routing fabric** inside the SoC plus a Linux driver model that must be connected correctly before sound can flow.

For our smart-speaker product example, the usual paths are:

- **prototype phase**
  - USB mic array
  - USB speaker or DP monitor audio
- **real product phase**
  - I2S codec or amplifier on the 40-pin header
  - possibly digital microphones or multichannel external front ends

---

**3. ASoC Driver for Jetson Products**

This is the first big section in NVIDIA's guide, and it is the right place to start.

**What NVIDIA means**

NVIDIA uses the standard Linux **ALSA** framework, and then adds Jetson-specific **ASoC** drivers so ALSA can control Jetson's internal audio hardware and any external codecs you attach.

**What that means for you**

When you run:

```bash
aplay sound.wav
```

you are not talking directly to the hardware.

You are going through:

- ALSA user-space tools and libraries
- ALSA kernel framework
- ASoC drivers
- Jetson's internal audio hardware
- then finally to the real interface like DP, USB, I2S, or DMIC

**The important subtopics from this section**

**ALSA**

ALSA is the standard Linux audio system.

You will touch it with:

- `aplay`
- `arecord`
- `amixer`

These are your first tools for discovering what exists and whether basic playback or capture works.

**DAPM**

**Dynamic Audio Power Management** sounds abstract, but the practical meaning is simple:

- ALSA keeps a graph of the audio path
- only the needed blocks should power on
- if the path is incomplete, playback or capture will not really happen

That is why "the sound card exists" is not enough. The route must also be valid.

**Device Tree**

Device tree tells Linux:

- which codec exists
- which I2C or SPI bus controls it
- which I2S controller is connected
- what the sound card routing and widgets look like

For Jetson audio work, device tree is not optional. It is part of normal bring-up.

**ASoC driver**

NVIDIA follows the standard ASoC split:

- **platform driver**
- **codec driver**
- **machine driver**

That split matters because it tells you where to look when something fails.

**Product-design translation**

If you are building an AI smart speaker, this section is basically telling you:

- user-space audio tools are only the top layer
- the route is dynamic, not hardwired
- your codec and board description must be correct
- "works on Linux" does not mean "works on Jetson" unless the ASoC side is also right

---

</details>

## 4. Audio Hub 硬件架构

这是 NVIDIA 指南中的第二大节，也是大多数人第一次冒出「这也太多了」念头的地方。

简化版本是：

> **APE/AHUB** 是 Jetson 内部的音频引擎与路由矩阵。

### APE vs AHUB

- **APE**
  - 专用的音频处理引擎模块
  - 让 Jetson 做音频工作时，不必以最朴素的方式把一切都推给 CPU
- **AHUB**
  - 用于路由和处理音频 stream 的内部互连与模块集合

### 最先需要关注的 AHUB 模块

不必记住每一个模块。做 Orin Nano 产品时，从下面这份清单开始：

| 模块 | 简要含义 | 为什么关注 |
|---|---|---|
| `ADMAIF` | 面向内存的音频端口 | ALSA 把它暴露为 `hw:APE,<n>` |
| `XBAR` | 路由开关 | 它决定音频在 SoC 内部的走向 |
| `I2S` | 串行音频接口 | 这是与众多外部 codec 和功放通信的方式 |
| `DMIC` | 数字麦克风接口 | 使用 PDM 数字麦克风时有用 |
| `DSPK` | 数字扬声器接口 | 数字扬声器通路中有用 |
| `AMX` / `ADX` | 合并/拆分 stream | 与 TDM 或高级多通道通路更相关 |
| `SFC` / `ASRC` | 采样率转换 | 当时钟与格式不再干净匹配时有用 |
| `MVC` | 增益/音量模块 | 用于内部增益控制场景 |

### 需要留在脑子里的图景

简单的播放通路：

```text
WAV file -> ALSA PCM -> ADMAIF1 -> XBAR -> I2S2 -> external codec -> speaker
```

简单的采集通路：

```text
mic -> codec -> I2S2 -> XBAR -> ADMAIF1 -> ALSA PCM -> recorded file
```

### 为什么这对本项目重要

如果要做智能音箱，最终会需要：

- 麦克风
- 播放
- 可能还有波束成形或多通道采集
- 可能还有采样率转换

AHUB 是 Jetson 能干净利落地完成这些事的原因，但这也意味着：

- 路由是可配置的
- 默认值并不总是你想要的
- 必须刻意把各个部件连接起来

### 一个重要的官方细节

NVIDIA 的文档指出，Orin 内部有多个 `I2S`、`DMIC`、`ADMAIF` 之类的模块实例，但**载板并不会把它们全部引出**。

这意味着：

- SoC 支持的能力可能多于 dev kit 引出的能力
- 板级访问方式很重要
- 规划硬件前必须先查 **Board Interfaces** 一节

---

## 5. ASoC 驱动软件架构

官方文档在这里变得更加具体，对本课程而言，这一节解释了为什么 `amixer` 如此重要。

### 用大白话说软件模型

NVIDIA 的 ASoC 设计使用：

- **ADMAIF 作为 platform driver**
- 大多数 AHUB 模块（如 `I2S`、`XBAR`、`AMX`、`ADX`、`DMIC`、`Mixer`）作为类 codec driver
- 一个 **machine driver** 用于注册最终的声卡

### 三种角色

#### Platform driver

它负责面向 PCM 的部分。

在 Jetson 上，这主要指 **ADMAIF**。

它是以下两者之间的桥梁：

- memory / ALSA PCM buffer
- 以及内部音频路由结构

这就是 NVIDIA 做如下映射的原因：

- `ADMAIF1` -> `hw:APE,0`
- `ADMAIF2` -> `hw:APE,1`
- 以此类推

这个差一映射在实际调试中影响很大。

#### Codec driver

在 Jetson 上，这指两种略有不同的事物：

- 真实的外部 codec，比如一颗 I2S 音频 codec 芯片
- 行为类似 ASoC component 的内部 AHUB 模块

这些 driver 定义：

- DAIs
- widgets
- routes
- mixer controls

#### Machine driver

这是把所有部件拼成一张可用声卡的绑定层。

如果要一个最短的有用定义：

> machine driver 就是让 Linux 把「Jetson 音频硬件 + codec + 路由描述」视为一张声卡的东西。

### 为什么路由不会凭空出现

NVIDIA 对此说得很明确：**启动时 XBAR 默认没有任何可用的路由连接**。

这意味着：

- 声卡可以存在
- ALSA PCM 节点可以存在
- 而你依然什么都听不到

因为 AHUB 内部的通路不完整。

这就是为什么 `amixer` 不是旁枝末节。它是 Jetson 音频路由的正常组成部分。

### 最重要的例子

这一行表示：

```bash
amixer -c APE cset name='I2S2 Mux' ADMAIF1
```

「把来自 `ADMAIF1` 的 stream 送进 `I2S2`。」

这就是内部路由。

然后这一行：

```bash
aplay -D hw:APE,0 sound.wav
```

才真正把数据送进 `ADMAIF1`，此时它才有了有用的去处。

### 落到产品设计上

对我们的智能音箱路线来说，这意味着：

- 如果播放没声音，不要只怪 codec
- 如果采集没声音，不要只怪麦克风
- 先问**内部路由是否完整**

---

## 6. High Definition Audio

NVIDIA 收录了完整的 **High Definition Audio** 一节，因为 Jetson 设备支持由 HDA 承载的显示音频。


<details>
<summary>English original</summary>

**4. Audio Hub Hardware Architecture**

This is the second big section in NVIDIA's guide, and it is where most people first think, "this is too much."

The simple version is:

> The **APE/AHUB** is Jetson's internal audio engine and routing matrix.

**APE vs AHUB**

- **APE**
  - the dedicated audio processing engine block
  - lets Jetson do audio work without pushing everything through the CPU in the most naive way
- **AHUB**
  - the internal interconnect and module collection used to route and process audio streams

**The AHUB blocks that matter first**

You do not need to memorize every module. For Orin Nano product work, start with this list:

| Block | Simple meaning | Why you care |
|---|---|---|
| `ADMAIF` | memory-facing audio port | this is what ALSA exposes as `hw:APE,<n>` |
| `XBAR` | routing switch | this decides where audio goes inside the SoC |
| `I2S` | serial audio interface | this is how you talk to many external codecs and amps |
| `DMIC` | digital mic interface | useful if you use PDM digital microphones |
| `DSPK` | digital speaker interface | useful for digital speaker paths |
| `AMX` / `ADX` | combine/split streams | more relevant for TDM or advanced multichannel paths |
| `SFC` / `ASRC` | sample-rate conversion | useful once clocks and formats stop matching cleanly |
| `MVC` | gain/volume block | useful for internal gain control use cases |

**The picture to keep in your head**

For a simple playback path:

```text
WAV file -> ALSA PCM -> ADMAIF1 -> XBAR -> I2S2 -> external codec -> speaker
```

For a simple capture path:

```text
mic -> codec -> I2S2 -> XBAR -> ADMAIF1 -> ALSA PCM -> recorded file
```

**Why this matters for our project**

If you are building a smart speaker, you eventually want:

- microphones
- playback
- maybe beamforming or multichannel capture
- maybe sample-rate conversion

The AHUB is the reason Jetson can do those things cleanly, but it also means:

- routes are configurable
- defaults are not always what you want
- you must deliberately connect the pieces

**One important official detail**

NVIDIA documents that Orin has many internal instances of modules like `I2S`, `DMIC`, and `ADMAIF`, but the **carrier board does not expose all of them**.

That means:

- the SoC may support more than the dev kit exposes
- board-level access matters
- you must check the **Board Interfaces** section before planning hardware

---

**5. ASoC Driver Software Architecture**

This is where the official doc gets more concrete, and for our course this is the section that explains why `amixer` is so important.

**The software model in plain English**

NVIDIA's ASoC design uses:

- **ADMAIF as the platform driver**
- most AHUB blocks like `I2S`, `XBAR`, `AMX`, `ADX`, `DMIC`, and `Mixer` as codec-like drivers
- a **machine driver** to register the final sound card

**The three roles**

**Platform driver**

This owns the PCM-facing part.

On Jetson, that mainly means **ADMAIF**.

It is the bridge between:

- memory / ALSA PCM buffers
- and the internal audio routing fabric

This is why NVIDIA maps:

- `ADMAIF1` -> `hw:APE,0`
- `ADMAIF2` -> `hw:APE,1`
- and so on

That off-by-one mapping matters a lot in real debugging.

**Codec driver**

On Jetson this means two slightly different things:

- a real external codec, like an I2S audio codec chip
- internal AHUB modules that behave like ASoC components

These drivers define:

- DAIs
- widgets
- routes
- mixer controls

**Machine driver**

This is the binding layer that turns all the parts into one usable sound card.

If you want the shortest useful definition:

> The machine driver is what makes Linux treat "Jetson audio hardware + codec + routing description" as one sound card.

**Why routes do not just appear by magic**

NVIDIA is explicit about this: **XBAR has no useful routing connections by default at boot**.

That means:

- the sound card can exist
- ALSA PCM nodes can exist
- and you can still hear nothing

because the path inside AHUB is incomplete.

That is why `amixer` is not a side topic. It is part of normal Jetson audio routing.

**The most important example**

This one line means:

```bash
amixer -c APE cset name='I2S2 Mux' ADMAIF1
```

"Take the stream coming from `ADMAIF1` and feed it into `I2S2`."

That is the internal route.

Then this line:

```bash
aplay -D hw:APE,0 sound.wav
```

actually sends data into `ADMAIF1`, which now has somewhere useful to go.

**Product-design translation**

For our smart-speaker path, this means:

- if playback is dead, do not only blame the codec
- if capture is dead, do not only blame the microphones
- first ask whether the **internal route is complete**

---

**6. High Definition Audio**

NVIDIA includes a full **High Definition Audio** section because Jetson devices support HDA-backed display audio.

</details>

### 在 Orin Nano 上意味着什么

对于 Orin Nano 开发者套件，这主要意味着：

- **DisplayPort 音频**
- 通过 `HDA` 声卡暴露

这不是你的智能音箱量产路径，但它是非常有用的快速检查。

### 为什么重要

如果你能通过 DP 显示器播放音频：

- ALSA 是活的
- 开发板的基本播放能工作
- 你的问题可能不是“Jetson 音频完全损坏”

### 实用的 Orin Nano 注意事项

NVIDIA 的表格将 **Jetson Orin Nano DP 单流音频** 映射到 `HDA` 声卡上的 **PCM 设备 ID `3`**。

所以典型命令是：

```bash
aplay -Dhw:HDA,3 sound.wav
```

但仍要在真实开发板上验证：

```bash
cat /proc/asound/cards
ls /dev/snd/pcmC?D*
```

因为声卡索引可能随启动变化。

### 针对我们路线图的定制建议

将 HDA / DP 音频用作：

- 快速播放检查
- kiosk 或外接显示器产品的演示路径

**不要**将其作为以下的主路径：

- 自定义音箱产品
- 麦克风阵列
- 语音设备

对于这些，I2S 或 USB 通常是真正的路线。

---

## 7. USB 音频

这是本项目最重要的章节之一，尽管它在技术上比自定义 ASoC codec bring-up（上电点亮/调通）更简单。

### 为什么 USB 音频如此重要

对于 AI 智能音箱或语音原型，USB 音频通常是解除以下阻塞的最快方式：

- 麦克风采集
- 扬声器播放
- 语音流水线实验
- 唤醒词
- 自动语音识别 / TTS 测试

### 产品策略

当你想验证以下内容时，先使用 USB 音频：

- 你的应用栈
- 音频质量预期
- 延迟
- 录音和播放脚本

然后当你想要以下内容时，转向 I2S：

- 自定义硬件
- 量产中更低的集成成本
- 对 codec 和功放设计更紧密的控制

### 典型工作流

插入设备，然后检查：

```bash
cat /proc/asound/cards
aplay -l
arecord -l
```

然后测试：

```bash
aplay -Dhw:<cardID>,<devID> sound.wav
arecord -Dhw:<cardID>,<devID> -r 48000 -c 2 -f S16_LE test.wav
```

### 针对我们路线图的定制建议

如果你的真实目标是本地助手或智能音箱：

1. 先使用 **USB 麦克风阵列**
2. 证明你的语音栈能工作
3. 然后设计量产音频路径

这个顺序能节省时间。

---

## 8. 开发板接口

这是回答“我的开发板实际暴露了哪些音频接口？”的官方章节。

对于我们 Orin Nano dev kit 路径，这是重要的部分。

### 重要的 Orin Nano 接口

| 接口 | 音频路径 | 需要引脚复用 | 声卡 |
|---|---|---:|---|
| 40 针排针 | `I2S2` | 是 | `APE` |
| 40 针排针 | `DMIC3` | 是 | `APE` |
| M.2 Key E | `I2S4` | 否 | `APE` |
| DisplayPort | `HDA` | 否 | `HDA` |
| USB | USB 音频 | 否 | 动态创建的 USB 声卡 |

### 通俗解释

- **40 针排针**
  - 自定义嵌入式音频硬件的最佳路径
  - 需要引脚复用
- **40 针排针上的 DMIC3**
  - 如果你使用直接数字麦克风，会很有意思
  - 仍不是智能音箱原型开发的通常第一步
- **M.2 Key E 音频**
  - 平台型号中可用，但不是本路线图中通常的首选路径
- **DisplayPort**
  - 最简单的内置输出检查
- **USB**
  - 整体最简单的 bring-up 路径

### 产品决策表

| 如果你想…… | 最佳首选 |
|---|---|
| 证明你的语音应用能工作 | USB 麦克风 / USB 耳机 |
| 证明播放基本能工作 | DP 显示器音频 |
| 构建真正的音箱产品 | 40 针 I2S + 外部 codec/功放 |
| 探索直接数字麦克风 | 40 针 DMIC 路径 |

---

## 9. 40 针 GPIO 扩展排针

这是 Orin Nano 上真正嵌入式音频工作最重要的官方章节。

### 为什么这一节重要

当你的产品不再只是 dev-kit 演示时，这就是你要用的路径。

使用它的典型原因：

- 自定义 codec 板
- 专用 DAC/ADC
- 外部功放
- 产品专用的播放和采集硬件

### 暴露的 Orin Nano 排针音频引脚

对于 Orin Nano dev kit，NVIDIA 文档记录：

#### I2S2

| 信号 | 排针引脚 |
|---|---:|
| `FS` | `35` |
| `SCLK` | `12` |
| `DIN` | `38` |
| `DOUT` | `40` |
| `AUD_MCLK` | `7` |

#### DMIC3

| 信号 | 排针引脚 |
|---|---:|
| `CLK` | `32` |
| `DAT` | `16` |

### 第一条规则：调试前先配置引脚复用

NVIDIA 说得很清楚：

> 音频引脚必须配置为 **SFIO**，而不是普通 GPIO。

如果跳过这一步，下游的一切都会变得令人困惑：

- codec 探测可能看起来正常
- 混音器控制可能已存在
- 但总线在物理线路上始终不会翻转

### 实际 bring-up 顺序

#### 1. 选择合理的 codec

NVIDIA 这里的建议完全正确。检查 codec 是否：

- 匹配你的接口类型
- 支持你的采样率和字长
- 有 Linux 驱动
- 有足够的示例或参考设置，使布线易于理解

对于许多项目，即使模拟规格看起来不错，Linux 支持薄弱的 codec 也是糟糕的工程选择。


<details>
<summary>English original</summary>

**What it means on Orin Nano**

For the Orin Nano developer kit, this mostly means:

- **DisplayPort audio**
- exposed through the `HDA` sound card

This is not your smart-speaker production path, but it is a very useful sanity check.

**Why it matters**

If you can play audio out through a DP monitor:

- ALSA is alive
- the board has basic playback working
- your problem is probably not "Jetson audio is completely broken"

**The practical Orin Nano note**

NVIDIA's table maps **Jetson Orin Nano DP single-stream audio** to **PCM device ID `3`** on the `HDA` card.

So a typical command is:

```bash
aplay -Dhw:HDA,3 sound.wav
```

But still verify on the real board:

```bash
cat /proc/asound/cards
ls /dev/snd/pcmC?D*
```

because card indexes can move across boots.

**Tailored advice for our roadmap**

Use HDA / DP audio as:

- a quick playback sanity test
- a demo path for kiosk or monitor-attached products

Do **not** treat it as the main path for:

- custom speaker products
- microphone arrays
- voice devices

For those, I2S or USB are usually the real routes.

---

**7. USB Audio**

This is one of the most important sections for our project, even though it is technically simpler than custom ASoC codec bring-up.

**Why USB audio matters so much**

For an AI smart speaker or voice prototype, USB audio is usually the fastest way to unblock:

- microphone capture
- speaker playback
- speech pipeline experiments
- wake word
- ASR / TTS testing

**The product strategy**

Use USB audio first when you want to validate:

- your app stack
- audio quality expectations
- latency
- recording and playback scripts

Then move to I2S when you want:

- custom hardware
- lower integration cost in production
- tighter control over codec and amp design

**Typical workflow**

Plug in the device, then inspect:

```bash
cat /proc/asound/cards
aplay -l
arecord -l
```

Then test:

```bash
aplay -Dhw:<cardID>,<devID> sound.wav
arecord -Dhw:<cardID>,<devID> -r 48000 -c 2 -f S16_LE test.wav
```

**Tailored recommendation for our roadmap**

If your real target is a local assistant or smart speaker:

1. use a **USB mic array** first
2. prove your speech stack works
3. then design the production audio path

That order saves time.

---

**8. Board Interfaces**

This is the official section that answers: "Which audio interfaces does my board actually expose?"

For our Orin Nano dev kit path, this is the important part.

**Orin Nano interfaces that matter**

| Interface | Audio path | Pinmux needed | Card |
|---|---|---:|---|
| 40-pin header | `I2S2` | Yes | `APE` |
| 40-pin header | `DMIC3` | Yes | `APE` |
| M.2 Key E | `I2S4` | No | `APE` |
| DisplayPort | `HDA` | No | `HDA` |
| USB | USB audio | No | USB card created dynamically |

**What this means in plain language**

- **40-pin header**
  - best path for custom embedded audio hardware
  - needs pinmux
- **DMIC3 on the 40-pin header**
  - interesting if you use direct digital microphones
  - still not the usual first step for smart-speaker prototyping
- **M.2 Key E audio**
  - available in the platform model, but not the usual first path in this roadmap
- **DisplayPort**
  - easiest built-in output check
- **USB**
  - easiest overall bring-up path

**Product decision table**

| If you want to... | Best first choice |
|---|---|
| prove your speech app works | USB mic / USB headset |
| prove playback basically works | DP monitor audio |
| build a real speaker product | 40-pin I2S + external codec/amp |
| explore direct digital mics | 40-pin DMIC path |

---

**9. 40-pin GPIO Expansion Header**

This is the most important official section for real embedded audio work on Orin Nano.

**Why this section matters**

This is the path you use when your product is no longer just a dev-kit demo.

Typical reasons to use it:

- custom codec board
- dedicated DAC/ADC
- external amplifier
- product-specific playback and capture hardware

**The exposed Orin Nano header audio pins**

For the Orin Nano dev kit, NVIDIA documents:

**I2S2**

| Signal | Header pin |
|---|---:|
| `FS` | `35` |
| `SCLK` | `12` |
| `DIN` | `38` |
| `DOUT` | `40` |
| `AUD_MCLK` | `7` |

**DMIC3**

| Signal | Header pin |
|---|---:|
| `CLK` | `32` |
| `DAT` | `16` |

**The first rule: pinmux before debugging**

NVIDIA is very clear:

> Audio pins must be configured as **SFIO**, not plain GPIO.

If you skip this, everything downstream gets confusing:

- codec probe may look fine
- mixer controls may exist
- and still the bus never toggles on the wires

**Practical bring-up order**

**1. Choose a codec that makes sense**

NVIDIA's advice here is exactly right. Check that the codec:

- matches your interface type
- supports your sample rates and word sizes
- has a Linux driver
- has enough examples or reference setups to make routing understandable

For many projects, a codec with weak Linux support is a bad engineering choice even if the analog specs look good.

</details>

#### 2. 确认控制总线

对于 Orin 系列的 40-pin 排针，NVIDIA 文档中给出的排针引出 I2C 控制器为：

```text
0x0c250000
```

如果 codec 由 I2C 控制，codec 的控制接口通常就在这里。

#### 3. 确认 I2S 控制器

对于 Orin 系列的 40-pin 排针，NVIDIA 文档中给出的排针引出 I2S 控制器为：

```text
0x02901100
```

I2S 节点必须通过以下内容使能：

```dts
status = "okay";
```

#### 4. 使用正确的 DAI link

对于 Orin 系列的 40-pin 排针，NVIDIA 文档中给出的相关 DAI link 实例为：

```text
&i2s2_dap
```

在 40-pin 排针上接自定义 codec 时，通常就是在设备树里覆盖或扩展这个 link。

#### 5. 配置 sound 节点

这是最容易被低估的部分。

`sound` 节点必须描述：

- widgets
- routes
- codec link
- 时钟假设
- I2S 模式

### 最常见的自定义声卡故障模式

通常的经过是这样：

1. codec 节点存在
2. I2S 节点存在
3. 声卡完成注册
4. 音频仍然不可用

为什么？

因为 route 不完整，或者 DAI link/时钟设置不对。

### 你必须做出的时钟决策

NVIDIA 描述了两种常见的 codec 时钟提供方式：

- codec 自带**独立晶振**
- codec 使用来自 Jetson 的 **`AUD_MCLK`**

这是板级设计决策，而不只是软件设置。

### Master 与 slave

NVIDIA 还特别指出一个经典的 bring-up（上电点亮/调通）陷阱：

- codec 作为 I2S master
- codec 作为 I2S slave

如果 codec 是 master，Jetson 就依赖外部时钟正确到达。
如果 codec 是 slave，Jetson 必须驱动时钟。

这个选择会在后续复位失败或总线看起来没反应时暴露出来。

### 这对智能音箱示例为何重要

如果后续要构建：

- 自定义功放板
- 扬声器输出级
- 本地麦克风前端

这一节就会成为整个产品的核心。

---

## 10. HD Audio Header

这是官方文档中篇幅较大的章节之一，但在我们的路线图里需要给一个非常直白的说明。

### 官方适用范围

NVIDIA 指出该章节适用于：

- **Jetson AGX Orin**
- **Jetson Thor**

不适用于 Jetson Orin Nano 开发套件。

### 这对我们意味着什么

对于我们的 Orin Nano 课程：

- 知道这一节存在
- 知道它属于另一类板级接口
- 然后基本可以忽略，除非你转向 AGX 级硬件

### 那为什么还要放进课程

因为读官方文档时，看到一个大篇幅的音频章节却与你的板子对不上，不该感到困惑。

所以路线图上的解读是：

> 在 **Orin Nano** 上，**HD Audio Header** 一节属于背景知识，不是你的主要实现路径。

对我们来说，重要的音频硬件路径仍然是：

- USB
- DP / HDA
- 40-pin I2S / DMIC

---

## 11. 用法与示例

NVIDIA 指南在这里进入实战。我们保持同样的思路，但收窄到 Orin Nano 上最先需要关注的流程。

### 第 1 步：检查板子实际注册了什么

永远从这里开始：

```bash
cat /proc/asound/cards
aplay -l
arecord -l
ls /dev/snd/pcmC?D*
```

原因：

- 声卡索引会变动
- USB 声卡是动态出现的
- 看到板子的实时状态后，`APE` 和 `HDA` 更容易推断

### 第 2 步：理解声卡名称

在 Orin 系列板子上，通常要关注：

- `HDA`
  - 显示音频
- `APE`
  - Jetson 内部音频引擎和 AHUB 路径
- 厂商/型号相关的 USB 名称
  - 取决于你的 USB 设备上报什么

### 第 3 步：先用最简单的播放路径

#### DisplayPort 播放

NVIDIA 将 Orin Nano 的 DP 单流播放映射到 HDA 设备 ID `3`。

所以常规测试是：

```bash
aplay -Dhw:HDA,3 sound.wav
```

这是很好的第一道快速验证。

#### USB 播放与采集

```bash
aplay -Dhw:<cardID>,<devID> sound.wav
arecord -Dhw:<cardID>,<devID> -r 48000 -c 2 -f S16_LE cap.wav
```

这里是验证以下内容的合适位置：

- ASR 输入
- TTS 输出
- 全双工应用行为

### 第 4 步：只有确实要用 Jetson 内部路由时，才转向 `APE`

对 `APE`，记住这个映射：

- `ADMAIF1` -> `hw:APE,0`
- `ADMAIF2` -> `hw:APE,1`

### 最先需要掌握的 `I2S2` 示例

#### 播放

```bash
amixer -c APE cset name="I2S2 Mux" ADMAIF1
aplay -D hw:APE,0 sound.wav
```

#### 采集

```bash
amixer -c APE cset name="ADMAIF1 Mux" I2S2
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE cap.wav
```

#### 内部回环

```bash
amixer -c APE cset name="I2S2 Mux" "ADMAIF1"
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
amixer -c APE cset name="I2S2 Loopback" "on"
aplay -D hw:APE,0 sound.wav &
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE loopback.wav
```

这个回环用例是整个栈里最好的调试工具之一。

它能帮助回答：

- 内部路由是否活着？
- `I2S2` 是否上电？
- 问题是在 SoC 之外而不是 SoC 内部？


<details>
<summary>English original</summary>

**2. Confirm the control bus**

For the Orin-series 40-pin header, NVIDIA documents the header-exposed I2C controller as:

```text
0x0c250000
```

That is usually where your codec control interface lives if the codec is I2C-controlled.

**3. Confirm the I2S controller**

For the Orin-series 40-pin header, NVIDIA documents the header-exposed I2S controller as:

```text
0x02901100
```

The I2S node must be enabled with:

```dts
status = "okay";
```

**4. Use the correct DAI link**

For the Orin-series 40-pin header, NVIDIA documents the relevant DAI link instance as:

```text
&i2s2_dap
```

That is the link you normally override or extend in device tree for a custom codec on the 40-pin header.

**5. Configure the sound node**

This is the part most people underestimate.

The `sound` node must describe:

- widgets
- routes
- codec link
- clocking assumptions
- I2S mode

**The most common custom-card failure pattern**

This is the usual sequence:

1. codec node exists
2. I2S node exists
3. card registers
4. audio still fails

Why?

Because the route is incomplete, or the DAI link/clocking settings are wrong.

**Clocking decision you must make**

NVIDIA describes two common ways to provide codec clocking:

- codec has its **own oscillator**
- codec uses **`AUD_MCLK`** from Jetson

This is a board design decision, not just a software setting.

**Master vs slave**

NVIDIA also calls out a classic bring-up trap:

- codec as I2S master
- codec as I2S slave

If the codec is master, Jetson depends on external clocks arriving correctly.
If the codec is slave, Jetson must drive the clocking.

This choice shows up later when resets fail or buses look dead.

**Why this matters for our smart-speaker example**

If you later build:

- custom amplifier board
- speaker output stage
- local microphone frontend

this section becomes the center of the whole product.

---

**10. HD Audio Header**

This is one of the official big sections, but for our roadmap it needs a very blunt note.

**Official scope**

NVIDIA says this section applies to:

- **Jetson AGX Orin**
- **Jetson Thor**

not to Jetson Orin Nano dev kit.

**What that means for us**

For our Orin Nano course:

- understand that the section exists
- understand that it is a different board interface family
- then mostly ignore it unless you move to AGX-class hardware

**Why keep it in the course at all**

Because when you read the official doc, you should not be confused by seeing a major audio section that does not match your board.

So the roadmap translation is:

> On **Orin Nano**, the **HD Audio Header** section is background knowledge, not your main implementation path.

For us, the important audio hardware paths are still:

- USB
- DP / HDA
- 40-pin I2S / DMIC

---

**11. Usage and Examples**

This is where NVIDIA's guide becomes practical. We will keep the same spirit, but narrow it to the flows that matter first on Orin Nano.

**Step 1: inspect what the board actually registered**

Always begin here:

```bash
cat /proc/asound/cards
aplay -l
arecord -l
ls /dev/snd/pcmC?D*
```

Why:

- card indexes can move
- USB cards appear dynamically
- `APE` and `HDA` are easier to reason about once you see the live board state

**Step 2: understand the card names**

On Orin-family boards you usually care about:

- `HDA`
  - display audio
- `APE`
  - Jetson internal audio engine and AHUB paths
- vendor/model-specific USB names
  - whatever your USB device reports

**Step 3: use the simplest playback path first**

**DisplayPort playback**

NVIDIA maps Orin Nano DP single-stream playback to HDA device ID `3`.

So the normal test is:

```bash
aplay -Dhw:HDA,3 sound.wav
```

This is a great first sanity check.

**USB playback and capture**

```bash
aplay -Dhw:<cardID>,<devID> sound.wav
arecord -Dhw:<cardID>,<devID> -r 48000 -c 2 -f S16_LE cap.wav
```

This is the right place to validate:

- ASR input
- TTS output
- full-duplex app behavior

**Step 4: move to `APE` only when you mean to use Jetson's internal routing**

For `APE`, remember the mapping:

- `ADMAIF1` -> `hw:APE,0`
- `ADMAIF2` -> `hw:APE,1`

**The first `I2S2` examples that matter**

**Playback**

```bash
amixer -c APE cset name="I2S2 Mux" ADMAIF1
aplay -D hw:APE,0 sound.wav
```

**Capture**

```bash
amixer -c APE cset name="ADMAIF1 Mux" I2S2
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE cap.wav
```

**Internal loopback**

```bash
amixer -c APE cset name="I2S2 Mux" "ADMAIF1"
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
amixer -c APE cset name="I2S2 Loopback" "on"
aplay -D hw:APE,0 sound.wav &
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE loopback.wav
```

This loopback case is one of the best debug tools in the whole stack.

It helps answer:

- is the internal routing alive?
- does `I2S2` power up?
- is the problem outside the SoC rather than inside it?

</details>

### 产品示例：3 个模拟 `IM73A135` 麦克风，搭配 `ES7210`

这类示例有助于让整篇指南变得具体。

假设你的产品是：

- 一个 **Jetson Orin Nano**
- 一块 **3 麦克风远场采集板**
- **模拟麦克风**，例如 `IM73A135` 的模拟版本
- 一个外部 **音频 ADC / codec**，例如 `ES7210`
- Jetson 通过以下方式连接到 codec：
  - `I2S2`，用于数字音频
  - `I2C`，用于 codec 控制

心智模型是：

```text
analog microphone
  -> analog voltage
  -> ES7210 ADC
  -> I2S/TDM digital stream
  -> Jetson I2S2
  -> AHUB / ADMAIF
  -> ALSA capture device
  -> ASR / beamforming / recording app
```

与 **数字麦克风** 设计的关键区别很简单：

- **数字麦克风** 已经输出数字音频，通常为 `PDM`
- **模拟麦克风** 输出模拟波形
- 因此需要一个 **ADC 级**

在本示例中，`ES7210` 就是 ADC 级。

#### 各部分在做什么

| 部件 | 在系统中的职责 |
|---|---|
| `IM73A135` 模拟麦克风 | 将声压转换为模拟电压 |
| `ES7210` | 将麦克风信号数字化，并将其成帧为数字音频 |
| Jetson 上的 `I2S2` | 将数字音频流送入 SoC |
| `AHUB` / `ADMAIF1` | 将该流路由到 ALSA |
| 你的应用 | 录制、流式传输、做波束成形，或馈送给自动语音识别 |

#### 为什么这是一个有用的 Jetson 示例

这个示例适用于以下真实场景：

- AI 智能音箱
- 语音家电
- 本地助手
- 嵌入式会议室设备
- 机器人语音接口

它也符合一种常见产品模式：

- **定制板上的模拟麦克风阵列**
- **外部多通道 ADC**
- **Jetson 只看到数字音频**

这才是思考边界的正确方式：

> Jetson 不直接读取模拟麦克风。Jetson 读取的是 **codec 的数字输出**。

#### 硬件视角

在系统层面，通常会有：

- 每个模拟麦克风的麦克风偏置 / 供电
- 从每个麦克风进入 `ES7210` 的模拟布线
- `MCLK`、`BCLK`、`LRCLK` 和 `DOUT` 或等效的数字音频线，位于 `ES7210` 与 Jetson 之间
- `I2C` 控制线，以便 Jetson 配置 codec 寄存器
- 干净的模拟接地与布局布线，因为模拟麦克风对板级噪声比数字接口敏感得多

如果使用 **3 个麦克风** 搭配 **4 通道 codec**，这仍然正常。

典型产品选择有：

- 让一个通道闲置
- 预留一个通道用于未来扩展
- 将一个通道用于参考输入或不同的模拟源

#### 软件视角

从 Linux 和 ASoC 的角度看，流程是：

1. `ES7210` 的 codec 驱动被实例化
2. machine 驱动将 Jetson `I2S2` 绑定到该 codec
3. Jetson 内部的路由在 `I2S2` 和 `ADMAIF` 之间移动数据
4. ALSA 暴露一个 capture PCM 节点
5. `arecord` 或你的应用读取样本

因此你的调试层次变为：

```text
analog mic hardware
  -> codec power and clocks
  -> codec driver probe
  -> I2C control path
  -> I2S data path
  -> AHUB route
  -> ALSA capture
  -> application
```

#### 需要记住的设备树形态

真实板卡的确切 DTS 取决于：

- 你的载板
- 你的 pinmux
- 确切的 `I2S` 实例
- 时钟主 / 从决策
- codec 使用的是普通 `I2S` 还是多通道 `TDM` 模式
- 你的 machine 驱动绑定

因此下面的示例是 **说明性的**，不是可直接用于生产的 DTS：

```dts
&i2s2 {
    status = "okay";
};

&i2c1 {
    es7210: audio-codec@40 {
        compatible = "everest,es7210";
        reg = <0x40>;
        status = "okay";
    };
};

sound {
    compatible = "nvidia,tegra186-audio-graph-card";
    status = "okay";

    dais = <&i2s2_port>;

    audio-routing =
        "Mic Jack", "MIC1",
        "Mic Jack", "MIC2",
        "Mic Jack", "MIC3";
};
```

你从这个示例中应该学到的不是确切的属性名。

重要的是结构：

- Jetson `I2S2` 必须存在
- codec 必须在 `I2C` 上探测
- 声卡必须绑定 CPU DAI 和 codec DAI
- 布线必须描述 capture widgets 如何连接

#### 首先要尝试的第一个 capture 路由

一旦 codec 实际完成探测且声卡存在，首先要考虑的 Jetson 侧路由是：

```bash
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
```

这意味着：

> 将来自 `I2S2` 的音频发送到 `ADMAIF1`，ALSA 将其暴露为 `hw:APE,0`

那么第一个 capture 测试是：

```bash
arecord -D hw:APE,0 -r 48000 -c 3 -f S16_LE test-3mic.wav
```

如果你的 codec 输出的是 4 通道而不是 3 通道，则测试真实通道数：

```bash
arecord -D hw:APE,0 -r 48000 -c 4 -f S16_LE test-4ch.wav
```

这个细节很重要，因为 codec 格式和 capture 命令必须一致。


<details>
<summary>English original</summary>

**Product example: 3x analog `IM73A135` microphones with `ES7210`**

This is the kind of example that helps the whole guide become concrete.

Assume your product is:

- a **Jetson Orin Nano**
- a **3-microphone far-field capture board**
- **analog microphones** such as the analog version of `IM73A135`
- an external **audio ADC / codec** such as `ES7210`
- Jetson connected to the codec through:
  - `I2S2` for digital audio
  - `I2C` for codec control

The mental model is:

```text
analog microphone
  -> analog voltage
  -> ES7210 ADC
  -> I2S/TDM digital stream
  -> Jetson I2S2
  -> AHUB / ADMAIF
  -> ALSA capture device
  -> ASR / beamforming / recording app
```

The key difference from a **digital microphone** design is simple:

- a **digital mic** already outputs digital audio, often `PDM`
- an **analog mic** outputs an analog waveform
- so you need an **ADC stage**

In this example, `ES7210` is the ADC stage.

**What each part is doing**

| Part | Job in the system |
|---|---|
| `IM73A135` analog mic | turns sound pressure into analog voltage |
| `ES7210` | digitizes microphone signals and frames them as digital audio |
| `I2S2` on Jetson | carries the digital audio stream into the SoC |
| `AHUB` / `ADMAIF1` | routes that stream to ALSA |
| your app | records, streams, beamforms, or feeds ASR |

**Why this is a useful Jetson example**

This example is realistic for:

- AI smart speakers
- voice appliances
- local assistants
- embedded meeting-room devices
- robotics voice interfaces

It also matches a common product pattern:

- **analog microphone array on a custom board**
- **external multichannel ADC**
- **Jetson only sees digital audio**

That is the right way to think about the boundary:

> Jetson does not read the analog microphones directly. Jetson reads the **codec's digital output**.

**Hardware view**

At a system level you would usually have:

- microphone bias / power for each analog mic
- analog routing from each mic into `ES7210`
- `MCLK`, `BCLK`, `LRCLK`, and `DOUT` or equivalent digital audio lines between `ES7210` and Jetson
- `I2C` control lines so Jetson can configure codec registers
- clean analog grounding and layout, because analog microphones are much more sensitive to board noise than digital interfaces

If you use **3 microphones** with a **4-channel codec**, that is still normal.

Typical product choices are:

- leave one channel unused
- reserve one channel for future expansion
- use one channel for a reference input or different analog source

**Software view**

From Linux and ASoC's point of view, the flow is:

1. the codec driver for `ES7210` is instantiated
2. the machine driver binds Jetson `I2S2` to that codec
3. the route inside Jetson moves data between `I2S2` and `ADMAIF`
4. ALSA exposes a capture PCM node
5. `arecord` or your app reads the samples

So your debugging layers become:

```text
analog mic hardware
  -> codec power and clocks
  -> codec driver probe
  -> I2C control path
  -> I2S data path
  -> AHUB route
  -> ALSA capture
  -> application
```

**A device-tree shape to keep in your head**

The exact DTS for a real board depends on:

- your carrier board
- your pinmux
- exact `I2S` instance
- clock master/slave decisions
- whether the codec is using plain `I2S` or a multichannel `TDM` mode
- your machine driver binding

So the example below is **illustrative**, not drop-in production DTS:

```dts
&i2s2 {
    status = "okay";
};

&i2c1 {
    es7210: audio-codec@40 {
        compatible = "everest,es7210";
        reg = <0x40>;
        status = "okay";
    };
};

sound {
    compatible = "nvidia,tegra186-audio-graph-card";
    status = "okay";

    dais = <&i2s2_port>;

    audio-routing =
        "Mic Jack", "MIC1",
        "Mic Jack", "MIC2",
        "Mic Jack", "MIC3";
};
```

What you should learn from this example is not the exact property names.

What matters is the structure:

- Jetson `I2S2` must exist
- the codec must probe on `I2C`
- the sound card must bind the CPU DAI and codec DAI
- routing must describe how capture widgets connect

**The first capture route to try**

Once the codec is actually probed and the sound card exists, the first Jetson-side route to think about is:

```bash
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
```

That means:

> take audio coming in from `I2S2` and send it to `ADMAIF1`, which ALSA exposes as `hw:APE,0`

Then the first capture test is:

```bash
arecord -D hw:APE,0 -r 48000 -c 3 -f S16_LE test-3mic.wav
```

If your codec is outputting 4 channels instead of 3, then test the real channel count:

```bash
arecord -D hw:APE,0 -r 48000 -c 4 -f S16_LE test-4ch.wav
```

That detail matters because the codec format and the capture command must agree.

</details>

#### 实用的 bring-up（上电点亮/调通）流程

对于这个具体的模拟麦克风设计，风险最低的 bring-up 顺序是：

1. 验证 codec 在 `I2C` 上探测成功
2. 验证声卡出现
3. 验证 `I2S2` 路由到 `ADMAIF1`
4. 录制原始多通道音频
5. 确认每个麦克风通道都有信号
6. 只有到那时才加入 beamforming、语音活动检测、唤醒词或自动语音识别

这个顺序很重要。

**不要**从远场 DSP 这类主张开始：

- 回声消除
- beamforming
- 说话人跟踪
- 唤醒词调优

直到你已经证明：

- 时钟正确
- 通道没有交换
- 采样率正确
- 增益合理
- 原始录音干净

#### 通常最先出问题的地方

对这类设计，最常见的早期故障有：

- codec 从未在 `I2C` 上探测到
- `I2S2` 的 pinmux 错误
- 时钟方向或帧格式错误
- 通道数错误
- ALSA 路由错误
- 来自布局布线或电源的模拟前端噪声
- 增益分级问题会让录音看似“死寂”，即使路径仍然活跃

这就是为什么正确的调试问题不仅是：

> “`arecord` 能运行吗？”

还包括：

> “我是否有干净、顺序正确、时钟正确的通道？”

#### 如何简单地向自己解释这一点

使用这一行总结：

> 模拟麦克风需要 codec 或 ADC 才能变成数字音频。然后 Jetson 会将该 codec 视作通过 `I2S` 连接的任何其他外部音频前端。

这就是核心思想。

如果把这个模型记在脑中，指南里的 ASoC、AHUB、布线和设备树部分就不再显得随机。

### 哪些官方示例应推迟

NVIDIA 指南还展示：

- hostless AHUB 布线
- TDM 采集
- AMX / ADX
- SFC / ASRC
- MVC

这些很重要，但不是我们智能音箱路径的首次 bring-up 主题。

稍后在以下情况使用：

- 立体声稳定
- 时钟稳定
- codec 布线已验证
- 确实需要多通道或采样率转换

### 智能音箱定制工作流

如果真正的目标是 AI 音箱产品，最清晰的顺序是：

1. DP 或 USB 播放健全性检查
2. USB 麦克风阵列采集
3. 完整语音应用验证
4. 定制 I2S 输出板
5. 定制采集板或多通道前端

这是风险最低的路径。

---

## 12. 故障排查

本节应像决策树一样使用，而不是当作一堆随机命令。

### 问题 A：找不到声卡

先执行：

```bash
cat /proc/asound/cards
dmesg | grep "ASoC"
```

#### 如果看到缺失 source 或 sink widget

NVIDIA 指南指出了常见原因：

- kernel 中未启用 codec 驱动
- codec 从未实例化
- I2C pinmux 错误或 I2C 总线错误
- `sound-name-prefix` 错误 / DAI 前缀不匹配

最有用的检查是：

```bash
cat /sys/kernel/debug/asoc/components
```

如果 codec 不在，就停止调试 mixer 设置。codec 尚未工作。

#### 如果看到 “CPU DAI not registered”

这通常意味着 I2S/DAI 侧未正确实例化。

同样，正确的反应是：

- 检查活动设备树
- 检查组件列表
- 验证 `i2s@...` 节点和 link 实例

而不是：

- 不断尝试不同的 `aplay` 命令

### 问题 B：声卡存在，但听不到声音或未录制到声音

这是 Jetson 上最常见的情况之一。

按以下顺序操作。

#### 1. 检查 DAPM 路径是否完整

启用链路追踪：

```bash
for i in `find /sys/kernel/debug/tracing/events -name "enable" | grep snd_soc_`; do
  echo 1 | sudo tee "$i" >/dev/null
done

sudo cat /sys/kernel/debug/tracing/trace_pipe | grep '\*'
```

期望看到：

- 从 source 到 sink 的完整路由
- 播放或采集开始时 widget 打开

如果路径不完整，再乐观也无法让音频工作。

#### 2. 验证 pinmux

如果使用 40-pin 排针：

- 引脚必须是 SFIO
- 而不是普通 GPIO

如果 pinmux 错误，软件可能看起来正常，而 wire 始终无信号。

#### 3. 验证 DT 状态

检查相关接口已启用：

```bash
dtc -I fs -O dts /proc/device-tree >/tmp/dt.log
```

然后检查真实烧录的设备树，而不只是源文件。

#### 4. 探测信号

如果使用 I2S：

- 检查 `FS`
- 检查 `BCLK`
- 检查流启动时它们是否实际存在

此时，示波器或逻辑分析仪比继续猜测更好。

### 问题 C：I2S 软件复位失败

NVIDIA 将此记录为接口时钟并未真正活动的典型迹象。

这通常发生在：

- codec 本应提供位时钟
- 但该外部时钟从未到达

所以真正的问题不是“复位为什么失败？”
而是：

> 谁应该是时钟主设备，以及该时钟是否真的存在？


<details>
<summary>English original</summary>

**A practical bring-up sequence**

For this exact analog-mic design, the lowest-risk bring-up order is:

1. verify the codec probes on `I2C`
2. verify the sound card appears
3. verify `I2S2` route into `ADMAIF1`
4. record raw multichannel audio
5. confirm each microphone channel is alive
6. only then add beamforming, VAD, wake-word, or ASR

That order is important.

Do **not** start with far-field DSP claims like:

- echo cancellation
- beamforming
- speaker tracking
- wake-word tuning

until you have already proven:

- clocks are correct
- channels are not swapped
- sample rate is correct
- gain is reasonable
- the raw recording is clean

**What usually breaks first**

For this kind of design, the most common early failures are:

- codec never probes on `I2C`
- wrong pinmux for `I2S2`
- wrong clock direction or frame format
- wrong channel count
- wrong ALSA route
- analog front-end noise from layout or power
- gain staging problems that make recordings seem "dead" even when the path is alive

That is why the right debug question is not only:

> "Does `arecord` run?"

but also:

> "Do I have clean, correctly ordered, correctly clocked channels?"

**How to explain this to yourself simply**

Use this one-line summary:

> Analog microphones need a codec or ADC to become digital audio. Jetson then treats that codec like any other external audio front end connected over `I2S`.

That is the core idea.

If you keep that model in your head, the guide's ASoC, AHUB, routing, and device-tree pieces stop feeling random.

**Which official examples should you postpone**

The NVIDIA guide also shows:

- hostless AHUB routing
- TDM capture
- AMX / ADX
- SFC / ASRC
- MVC

These are important, but not first-bring-up topics for our smart-speaker path.

Use them later when:

- stereo is stable
- clocks are stable
- codec routing is proven
- you truly need multichannel or rate conversion

**Smart-speaker tailored workflow**

If your real goal is an AI speaker product, the cleanest order is:

1. DP or USB playback sanity check
2. USB mic array capture
3. full speech app validation
4. custom I2S output board
5. custom capture board or multichannel front end

That is the lowest-risk path.

---

**12. Troubleshooting**

This section should be used like a decision tree, not as a bag of random commands.

**Problem A: no sound cards found**

Start with:

```bash
cat /proc/asound/cards
dmesg | grep "ASoC"
```

**If you see missing source or sink widgets**

NVIDIA's guide points to the usual causes:

- codec driver not enabled in kernel
- codec never instantiated
- wrong I2C pinmux or wrong I2C bus
- bad `sound-name-prefix` / DAI prefix mismatch

The most useful check is:

```bash
cat /sys/kernel/debug/asoc/components
```

If your codec is not there, stop debugging mixer settings. The codec is not alive yet.

**If you see "CPU DAI not registered"**

This usually means the I2S/DAI side was not instantiated correctly.

Again, the correct reaction is:

- inspect the live device tree
- inspect the component list
- verify the `i2s@...` node and link instance

Not:

- keep trying different `aplay` commands

**Problem B: sound card exists, but sound is not audible or not recorded**

This is one of the most common states on Jetson.

Follow this order.

**1. Check whether the DAPM path completes**

Enable tracing:

```bash
for i in `find /sys/kernel/debug/tracing/events -name "enable" | grep snd_soc_`; do
  echo 1 | sudo tee "$i" >/dev/null
done

sudo cat /sys/kernel/debug/tracing/trace_pipe | grep '\*'
```

What you want:

- a complete route from source to sink
- widgets turning on when playback or capture starts

If the path is incomplete, no amount of optimism will make the audio work.

**2. Verify pinmux**

If you are using the 40-pin header:

- pins must be SFIO
- not plain GPIO

If pinmux is wrong, the software may look fine while the wires stay dead.

**3. Verify the DT status**

Check that the relevant interface is enabled:

```bash
dtc -I fs -O dts /proc/device-tree >/tmp/dt.log
```

Then inspect the real flashed tree, not only your source files.

**4. Probe the signals**

If you are on I2S:

- check `FS`
- check `BCLK`
- check whether they are actually present when the stream starts

At this point, a scope or logic analyzer is better than more guessing.

**Problem C: I2S software reset failed**

NVIDIA documents this as a classic sign that the interface clock is not really active.

This usually happens when:

- the codec is supposed to provide bit clock
- but that external clock never arrives

So the real question is not "why did reset fail?"
It is:

> Who is supposed to be the clock master, and is that clock actually there?

</details>

### 问题 D：播放或采集期间出现 XRUN

XRUN 意味着音频缓冲区的生产者与消费者跟不上了。

在 Jetson 上，NVIDIA 建议先检查系统性能。

可采取的做法：

- 运行 max clocks 模式
- 测试期间避免走慢速文件系统路径
- 如有需要，采集/播放文件使用 RAM-backed 存储
- 只有确认问题确实与延迟相关后，才增大缓冲区大小

在初次调试音频时，XRUN 通常意味着：

- 系统负载过高
- 存储路径不佳
- 时序假设过于激进

### 问题 E：爆音与咔嗒声

NVIDIA 指出，这通常意味着音频数据在 codec 干净地完成上电或下电之前就开始流动了。

官方的调试开关是：

```bash
echo 10 | sudo tee /sys/kernel/debug/asoc/APE/dapm_pop_time
```

它会延迟状态切换，以减少启动/关闭时的杂音。

### 最佳排障思路

不要按这个顺序调试 Jetson 音频：

1. 随手的 mixer 命令
2. 随手的 mixer 命令
3. 又一条随手的 mixer 命令

按这个顺序调试：

1. 声卡注册
2. codec 实例化
3. DAI / 接口使能
4. DAPM 路由建立完成
5. 真实引脚上出现信号
6. 性能或时钟质量问题

按这个顺序，你才不会被逼疯。

---

## 13. 对本项目而言最关键的是什么

如果你真正的目标是 **AI 智能音箱** 或 **语音设备**，以下是浓缩后的课程成果。

### 优先使用什么

- **USB 麦克风阵列**
  - 最省事的语音原型路线
- **USB 音箱或 DP 音频**
  - 最省事的播放验证路线

### 后续使用什么

- **40-pin I2S2**
  - 面向量产的播放路径
- **40-pin DMIC3**
  - 可尝试直接接数字麦克风的实验
- **高级 AHUB 模块**
  - 待基础路径验证通过后再用

### 暂时忽略什么

- **HD Audio Header**
  - 不是 Orin Nano 的路径
- **高级 AMX / ADX / hostless 示例**
  - 以后有用，不是第一周该看的内容

### 「完成」的理解

当你能解释清楚以下问题时，就算过关了：

- 为什么 `APE`、`HDA` 和 USB 是不同的
- 为什么 Jetson 音频需要路由，而不仅仅是一个设备节点
- 为什么 40-pin 排针路径需要 pinmux 和 DT 方面的工作
- 为什么 `ADMAIF1` 映射到 `hw:APE,0`
- 为什么 DAPM 往往是「声卡存在」与「音频能用」之间的分水岭
- 为什么 USB 是语音原型开发的正确第一步，但并不总是合适的产品接口

到这一步，你做的才是真正的 Jetson 音频工程，而不只是在命令行里乱戳。

---

## 14. 参考资料

- [NVIDIA Jetson Linux Developer Guide - Audio Setup and Development](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/SD/Communications/AudioSetupAndDevelopment.html)
- [ALSA Project](https://www.alsa-project.org/)
- [ASoC DAPM documentation](https://www.kernel.org/doc/html/latest/sound/soc/dapm.html)
- [Jetson Expansion Header configuration](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/HR/ConfiguringTheJetsonExpansionHeaders.html)


<details>
<summary>English original</summary>

**Problem D: XRUN during playback or capture**

An XRUN means the producer and consumer of the audio buffer are not keeping up.

On Jetson, NVIDIA suggests checking system performance first.

Practical actions:

- run max clocks mode
- avoid slow filesystem paths during testing
- use RAM-backed capture/playback files if needed
- increase buffer size only after confirming the issue is really latency-related

For first audio debugging, XRUNs often mean:

- too much system load
- bad storage path
- timing assumptions that are too aggressive

**Problem E: pops and clicks**

NVIDIA points out that this usually means audio data starts moving before the codec has cleanly powered up or down.

The official debug knob is:

```bash
echo 10 | sudo tee /sys/kernel/debug/asoc/APE/dapm_pop_time
```

This delays the transition to reduce startup/shutdown artifacts.

**The best troubleshooting mindset**

Do not debug Jetson audio in this order:

1. random mixer command
2. random mixer command
3. another random mixer command

Debug in this order:

1. card registration
2. codec instantiation
3. DAI / interface enablement
4. DAPM route completion
5. signal presence on real pins
6. performance or clock-quality issues

That order is how you stay sane.

---

**13. What matters most for our project**

If your real target is an **AI smart speaker** or **voice appliance**, here is the condensed course outcome.

**What to use first**

- **USB mic array**
  - easiest speech prototype path
- **USB speaker or DP audio**
  - easiest playback sanity path

**What to use later**

- **40-pin I2S2**
  - production-minded playback path
- **40-pin DMIC3**
  - possible direct digital mic experiments
- **advanced AHUB blocks**
  - later once the basic path is proven

**What to ignore for now**

- **HD Audio Header**
  - not an Orin Nano path
- **advanced AMX / ADX / hostless examples**
  - useful later, not first week material

**The "finished" understanding**

You are in good shape when you can explain:

- why `APE`, `HDA`, and USB are different
- why Jetson audio needs routing, not just a device node
- why the 40-pin header path needs pinmux and DT work
- why `ADMAIF1` maps to `hw:APE,0`
- why DAPM is often the difference between "card exists" and "audio works"
- why USB is the right first step for voice prototyping, but not always the right production interface

That is the point where you are doing real Jetson audio engineering, not just command-line poking.

---

**14. References**

- [NVIDIA Jetson Linux Developer Guide - Audio Setup and Development](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/SD/Communications/AudioSetupAndDevelopment.html)
- [ALSA Project](https://www.alsa-project.org/)
- [ASoC DAPM documentation](https://www.kernel.org/doc/html/latest/sound/soc/dapm.html)
- [Jetson Expansion Header configuration](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/HR/ConfiguringTheJetsonExpansionHeaders.html)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/4. Multimedia/Jetson-Audio-Setup-and-Development/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/4.%20Multimedia/Jetson-Audio-Setup-and-Development/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
