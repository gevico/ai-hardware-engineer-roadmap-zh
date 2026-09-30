---
title: Jetson Orin Nano 上的 ESP32-LyraT I2S 麦克风采集 - 应用指南
description: Jetson Orin Nano 上的 ESP32-LyraT I2S 麦克风采集 - 应用指南
---

# Jetson Orin Nano 上的 ESP32-LyraT I2S 麦克风采集 - 应用指南

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">ELIM</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入剖析 · Jetson 学习路径</p>
<p class="course-identity__title">ESP32-LyraT I2S 麦克风采集 on Jetson Orin Nano - 应用指南 的专用课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


> **目标：** 把 **ESP32-LyraT 音频开发套件** 用作 **Jetson Orin Nano 8GB / Super Developer Kit** 40-pin 排针上的实用 I2S 麦克风/音频前端，从而在设计自定义麦克风板之前，先测试 Jetson I2S 采集、AHUB routing 与 JetPack 音频调试。

**Hub：** [多媒体](../Guide.md)  
**音频基础：** [Jetson Audio Setup and Development](../02-Jetson音频设置与开发/Guide.md)

---

## 1. 这个应用在测试什么

这不是普通的“插上一个 USB 麦克风”测试。

本应用测试的是真实的嵌入式音频通路：

```text
microphone frontend
  -> codec / ADC
  -> I2S serial audio
  -> Jetson 40-pin header I2S2
  -> AHUB / ADMAIF
  -> ALSA capture
  -> recording, ASR, wake-word, or voice pipeline
```

ESP32-LyraT 在这里很有用，因为它本身已经具备：

- 板载麦克风
- 一颗 `ES8388` 音频 codec
- 引出的 I2S 排针
- 用于配置板侧音频通路的 ESP-ADF 示例

关键的一处纠正：

> 音频不通过 I2C 传输。在 LyraT 上，I2C 控制 codec 寄存器。音频采样通过 I2S 传输。

所以在本项目里说“I2C 麦克风”，通常指的是下面两者之一：

- **I2S 麦克风 / I2S 音频流：** 数字音频数据通路
- **I2C 控制的 codec：** 诸如 `ES8388` 这类 codec 的控制通路

---

## 2. 推荐的首个架构

把 LyraT 用作音频前端，由 ESP32 一侧配置 codec。

```text
ESP32-LyraT
  onboard mics
      |
      v
  ES8388 codec / ADC
      |
      | I2S clocks + ADC data on JP4
      v
Jetson Orin Nano 40-pin header
  I2S2 DIN / SCLK / FS
      |
      v
Jetson AHUB -> ADMAIF1 -> ALSA arecord
```

这是风险最低的 bring-up（上电点亮/调通）模型，因为：

- ESP32-LyraT 已经知道如何初始化 `ES8388`
- Jetson 只需证明自己能接收 I2S
- 避免出现两个 host 都想通过 I2C 控制同一颗 codec 的冲突

后续做真实产品时，把 LyraT 换成由 Jetson 直接控制的专用 codec 或 ADC 板。

---

## 3. 一开始不要做什么

不要一上来就让 Jetson 通过 I2C 直接控制 LyraT 的 `ES8388`。

这条路理论上可行，但作为第一次测试并不好，因为：

- 在 LyraT 上 `ES8388` 已经连到 ESP32
- ESP32 固件通常负责 codec 初始化
- Jetson 需要 ASoC codec 驱动与 machine driver 的绑定
- 同一 codec 控制通路上的两个 host 可能冲突
- 要让 ESP32 一侧完全进入三态，可能需要改板或改固件

首次 bring-up 时，把 LyraT 当作一个已配置好的 I2S 音频源。

---

## 4. 硬件前置要求

- Jetson Orin Nano 8GB / Super Developer Kit
- JetPack 6.x / L4T 36.x 或更新版本
- ESP32-LyraT V4.3 或类似的 LyraT 变体
- 用于给 LyraT 供电和烧录的 USB 线
- 杜邦线
- 逻辑分析仪或示波器，强烈建议配备
- 可选：有源音箱或耳机，用于 LyraT 一侧的自检

电压规则：

> Jetson 40-pin 排针的信号是 3.3 V 逻辑。不要接入 5 V 的 UART/I2S/控制信号。

LyraT 的 I2S 排针信号是 ESP32 一侧的 3.3 V 逻辑，因此直接接线是合理的。但仍要共地。

---

## 5. Jetson 40-pin 上的 I2S2 引脚

对于 Orin Nano 开发者套件的 40-pin 排针，音频指南使用的是排针引出的 `I2S2` 通路：

| Jetson 功能 | 40-pin 排针引脚 | 本测试中的方向 |
|---|---:|---|
| `I2S2_SCLK` / 位时钟 | `12` | 若 LyraT 为主设备，则 LyraT -> Jetson |
| `I2S2_FS` / LRCK / 帧同步 | `35` | 若 LyraT 为主设备，则 LyraT -> Jetson |
| `I2S2_DIN` | `38` | LyraT 音频数据 -> Jetson |
| `I2S2_DOUT` | `40` | Jetson 回放数据 -> LyraT，可选 |
| `AUD_MCLK` | `7` | 可选；首次采集测试不要连接 |
| `GND` | `6`、`9`、`14` 等 | 公共地 |

对于麦克风采集，最小可用接线是：

```text
LyraT bit clock  -> Jetson pin 12
LyraT LRCK       -> Jetson pin 35
LyraT ADC data   -> Jetson pin 38
LyraT GND        -> Jetson GND
```

不要两侧都设成时钟主设备。如果由 LyraT 驱动 `SCLK` 和 `LRCK`，那么在该 DAI 链路上 Jetson 必须配置为 I2S 时钟从设备。


<details>
<summary>English original</summary>

**ESP32-LyraT I2S Microphone Capture on Jetson Orin Nano - Application Guide**

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">ELIM</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for ESP32-LyraT I2S Microphone Capture on Jetson Orin Nano - Application Guide.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Goal:** Use an **ESP32-LyraT audio dev kit** as a practical I2S microphone/audio frontend for the **Jetson Orin Nano 8GB / Super Developer Kit** 40-pin header, so you can test Jetson I2S capture, AHUB routing, and JetPack audio debugging before designing a custom microphone board.

**Hub:** [Multimedia](../Guide.md)  
**Audio foundation:** [Jetson Audio Setup and Development](../02-Jetson音频设置与开发/Guide.md)

---

**1. What this application is testing**

This is not a normal "plug in a USB mic" test.

This application tests the real embedded-audio path:

```text
microphone frontend
  -> codec / ADC
  -> I2S serial audio
  -> Jetson 40-pin header I2S2
  -> AHUB / ADMAIF
  -> ALSA capture
  -> recording, ASR, wake-word, or voice pipeline
```

The ESP32-LyraT is useful here because it already has:

- onboard microphones
- an `ES8388` audio codec
- an exposed I2S header
- ESP-ADF examples for configuring the board-side audio path

The important correction:

> Audio is not carried over I2C. On the LyraT, I2C controls the codec registers. The audio samples move over I2S.

So if you say "I2C mic" in this project, usually you mean one of these:

- **I2S microphone / I2S audio stream:** digital audio data path
- **I2C-controlled codec:** control path for a codec such as `ES8388`

---

**2. Recommended first architecture**

Use the LyraT as the audio frontend and let the ESP32 side configure the codec.

```text
ESP32-LyraT
  onboard mics
      |
      v
  ES8388 codec / ADC
      |
      | I2S clocks + ADC data on JP4
      v
Jetson Orin Nano 40-pin header
  I2S2 DIN / SCLK / FS
      |
      v
Jetson AHUB -> ADMAIF1 -> ALSA arecord
```

This is the lowest-risk bring-up model because:

- ESP32-LyraT already knows how to initialize the `ES8388`
- Jetson only has to prove that it can receive I2S
- you avoid fighting two hosts trying to control the same codec over I2C

Later, for a real product, replace the LyraT with a dedicated codec or ADC board that Jetson controls directly.

---

**3. What not to do first**

Do not start by trying to make Jetson directly control the LyraT `ES8388` over I2C.

That path is possible in theory, but it is a poor first test because:

- the `ES8388` is already wired to the ESP32 on the LyraT
- the ESP32 firmware normally owns codec initialization
- Jetson would need an ASoC codec driver and machine-driver binding
- two hosts on the same codec control path can conflict
- you may need board rework or firmware changes to fully tri-state the ESP32 side

For first bring-up, treat the LyraT like a configured I2S audio source.

---

**4. Hardware prerequisites**

- Jetson Orin Nano 8GB / Super Developer Kit
- JetPack 6.x / L4T 36.x or newer
- ESP32-LyraT V4.3 or similar LyraT variant
- USB cable for powering and flashing the LyraT
- jumper wires
- logic analyzer or oscilloscope, strongly recommended
- optional powered speakers or headphones for LyraT-side sanity tests

Voltage rule:

> Jetson 40-pin header signals are 3.3 V logic. Do not connect 5 V UART/I2S/control signals.

The LyraT I2S header signals are ESP32-side 3.3 V logic, so direct signal wiring is reasonable. Still share ground.

---

**5. Jetson 40-pin I2S2 pins**

For the Orin Nano developer kit 40-pin header, the audio guide uses the header-exposed `I2S2` path:

| Jetson function | 40-pin header pin | Direction in this test |
|---|---:|---|
| `I2S2_SCLK` / bit clock | `12` | LyraT -> Jetson if LyraT is master |
| `I2S2_FS` / LRCK / frame sync | `35` | LyraT -> Jetson if LyraT is master |
| `I2S2_DIN` | `38` | LyraT audio data -> Jetson |
| `I2S2_DOUT` | `40` | Jetson playback data -> LyraT, optional |
| `AUD_MCLK` | `7` | optional; do not connect for first capture test |
| `GND` | `6`, `9`, `14`, etc. | common ground |

For microphone capture, the minimum useful wiring is:

```text
LyraT bit clock  -> Jetson pin 12
LyraT LRCK       -> Jetson pin 35
LyraT ADC data   -> Jetson pin 38
LyraT GND        -> Jetson GND
```

Do not connect both sides as clock masters. If LyraT drives `SCLK` and `LRCK`, Jetson must be configured as the I2S clock slave for that DAI link.

---

</details>

## 6. ESP32-LyraT JP4 I2S 排针

Espressif 文档说明 LyraT V4.3 I2S 排针 `JP4` 引出了板上的 I2S 信号：

| LyraT JP4 信号 | ESP32 引脚 | 本测试中的含义 |
|---|---|---|
| `MCLK` | `GPIO0` | master clock；首次 Jetson 采集测试通常不接 |
| `SCLK` | `GPIO5` | I2S 位时钟 |
| `LRCK` | `GPIO25` | I2S 左/右声道帧时钟 |
| `DSDIN` | `GPIO26` | 送入 ES8388 DAC 的数据，可用于向 LyraT 播放 |
| `ASDOUT` | `GPIO35` | 从 ES8388 输出的 ADC 数据，可用于把麦克风采集送入 Jetson |
| `GND` | `GND` | 公共参考地 |

要把 LyraT 麦克风的信号采集进 Jetson：

| LyraT 信号 | Jetson 信号 | Jetson 引脚 |
|---|---|---:|
| `SCLK` | `I2S2_SCLK` | `12` |
| `LRCK` | `I2S2_FS` | `35` |
| `ASDOUT` | `I2S2_DIN` | `38` |
| `GND` | `GND` | `6` / `9` / `14` |

可选的播放方向：

| Jetson 信号 | LyraT 信号 | 用途 |
|---|---|---|
| `I2S2_DOUT` 引脚 `40` | `DSDIN` | 把 Jetson 音频送入 LyraT DAC |

在采集跑通之前，保持播放不接。

---

## 7. Bring-up 计划

按这个顺序做。这样可以避免在连线级时钟还是错的时候去排查软件路由。

1. 验证 Jetson 内部音频路由。
2. 验证 LyraT 侧的麦克风和 codec 工作正常。
3. 让 LyraT 输出稳定的 I2S 麦克风流 —— 可参考 [§9 参考固件 `lyrat_jp4_passthrough`](#reference-firmware-lyrat_jp4_passthrough-verified) 和 [§14 已验证结果](#verified-test-result-lyrat-side-2026-05-11) 作为已知可用的对照。
4. 用示波器或逻辑分析仪验证 `SCLK`、`LRCK` 和 `ASDOUT`。
5. 使能 Jetson 40-pin 的 `I2S2` pinmux。
6. 配置 Jetson 音频路由，从 `I2S2` 到 `ADMAIF1`。
7. 用 `arecord` 录制原始音频。
8. 到这步之后，才加入 GStreamer、唤醒词、ASR 或波束成形。

---

## 8. Jetson 侧设置

### 确认 JetPack 和 ALSA 设备

```bash
cat /etc/nv_tegra_release
uname -a

aplay -l
arecord -l
amixer -c APE controls | grep -E 'I2S2|ADMAIF'
```

如果 `APE` 不存在，停下来，先把 Jetson 音频栈修好，再接外部硬件。

### 如何判断健康的 JetPack 6 / R36 基线

如果你的 Jetson 输出类似下面这样：

- `card 1: APE [NVIDIA Jetson Orin Nano APE]`
- `ADMAIF1` 到 `ADMAIF20` 的采集和播放设备
- 诸如 `I2S2 Mux`、`I2S2 codec frame mode`、`I2S2 codec master mode` 和 `ADMAIF1 Mux` 的 mixer 控件

那就是好迹象。

它意味着：

- `APE` 声卡存在
- AHUB / ADMAIF 架构已注册
- `I2S2` 驱动已暴露给 ALSA
- `hw:APE,0` 映射到 `ADMAIF1`，这是第一个可用的外部采集端点

这**并不**意味着外部排针的采集通路已经工作。

你仍然需要：

- 在 40-pin 排针上使能 `I2S2`
- 正确的主/从时钟关系
- 从 `I2S2` 到 `ADMAIF1` 的正确路由
- 线路上有实际的 `BCLK`、`LRCK` 和数据

### 使能 40-pin I2S2 pinmux

如果你的镜像支持，就用 Jetson-IO：

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
```

选择使能 `I2S2` 的 40-pin 排针配置，保存 overlay，然后重启。

重启之后：

```bash
amixer -c APE controls | grep I2S2
```

### 先做内部 loopback

在使用 LyraT 之前，先证明 Jetson 侧路由是活的：

```bash
amixer -c APE cset name="I2S2 Mux" "ADMAIF1"
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
amixer -c APE cset name="I2S2 Loopback" "on"

aplay -D hw:APE,0 test.wav &
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE jetson-i2s2-loopback.wav
```

这并不能证明外部引脚工作。它证明的是 AHUB 路由本身没问题。

---

## 9. LyraT 侧设置

在 LyraT 上，使用这样的 ESP-ADF 或 ESP-IDF 固件：

- 初始化 `ES8388`
- 选择板载麦克风输入
- 配置采样率，通常为 `48000`
- 配置立体声或单声道采集
- 稳定地驱动 I2S 时钟
- 让 ADC 数据在 `ASDOUT` 上可见

首次 Jetson 测试，选一个最普通的格式：

| 参数 | 推荐的初始值 |
|---|---|
| 采样率 | `48000` Hz |
| 声道数 | `2` |
| 采样格式 | `S16_LE`，或在 32-bit I2S slot 里放 16-bit 采样 |
| 时钟角色 | LyraT 主，Jetson 从 |
| 数据来源 | LyraT 板载麦克风通路，经 `ES8388` ADC |

常见的立体声 48 kHz 配置下，预期的连线级时钟：

| 信号 | 预期行为 |
|---|---|
| `LRCK` | 48 kHz |
| `SCLK` | 通常是 1.536 MHz 或 3.072 MHz，取决于 slot 宽度 |
| `ASDOUT` | 麦克风信号有效时翻转 |

如果 `LRCK` 或 `SCLK` 不存在，Jetson 什么都采集不到。


<details>
<summary>English original</summary>

**6. ESP32-LyraT JP4 I2S header**

Espressif documents the LyraT V4.3 I2S header `JP4` as exposing the board I2S signals:

| LyraT JP4 signal | ESP32 pin | Meaning for this test |
|---|---|---|
| `MCLK` | `GPIO0` | master clock; usually leave unconnected for first Jetson capture test |
| `SCLK` | `GPIO5` | I2S bit clock |
| `LRCK` | `GPIO25` | I2S left/right frame clock |
| `DSDIN` | `GPIO26` | data into ES8388 DAC, useful for playback into LyraT |
| `ASDOUT` | `GPIO35` | ADC data out from ES8388, useful for mic capture into Jetson |
| `GND` | `GND` | common reference |

For capture from LyraT microphones into Jetson:

| LyraT signal | Jetson signal | Jetson pin |
|---|---|---:|
| `SCLK` | `I2S2_SCLK` | `12` |
| `LRCK` | `I2S2_FS` | `35` |
| `ASDOUT` | `I2S2_DIN` | `38` |
| `GND` | `GND` | `6` / `9` / `14` |

Optional playback direction:

| Jetson signal | LyraT signal | Purpose |
|---|---|---|
| `I2S2_DOUT` pin `40` | `DSDIN` | send Jetson audio into LyraT DAC |

Keep playback disconnected until capture works.

---

**7. Bring-up plan**

Use this order. It prevents chasing software routing while the wire-level clocks are still wrong.

1. Prove Jetson internal audio routing.
2. Prove LyraT microphones and codec work on the LyraT side.
3. Make LyraT output a stable I2S mic stream — see [§9 reference firmware `lyrat_jp4_passthrough`](#reference-firmware-lyrat_jp4_passthrough-verified) and [§14 verified result](#verified-test-result-lyrat-side-2026-05-11) for a known-good control.
4. Verify `SCLK`, `LRCK`, and `ASDOUT` on a scope or logic analyzer.
5. Enable Jetson 40-pin `I2S2` pinmux.
6. Configure Jetson audio route from `I2S2` to `ADMAIF1`.
7. Record raw audio with `arecord`.
8. Only then add GStreamer, wake-word, ASR, or beamforming.

---

**8. Jetson-side setup**

**Confirm JetPack and ALSA devices**

```bash
cat /etc/nv_tegra_release
uname -a

aplay -l
arecord -l
amixer -c APE controls | grep -E 'I2S2|ADMAIF'
```

If `APE` does not exist, stop and fix the Jetson audio stack before wiring external hardware.

**How to read a healthy JetPack 6 / R36 baseline**

If your Jetson shows output like this:

- `card 1: APE [NVIDIA Jetson Orin Nano APE]`
- capture and playback devices for `ADMAIF1` through `ADMAIF20`
- mixer controls such as `I2S2 Mux`, `I2S2 codec frame mode`, `I2S2 codec master mode`, and `ADMAIF1 Mux`

that is a good sign.

It means:

- the `APE` sound card is present
- the AHUB / ADMAIF fabric is registered
- the `I2S2` driver is exposed to ALSA
- `hw:APE,0` maps to `ADMAIF1`, which is the first practical external capture endpoint

It does **not** mean the external header capture path is already working.

You still need:

- `I2S2` enabled on the 40-pin header
- the correct master/slave clock relationship
- the correct route from `I2S2` into `ADMAIF1`
- actual `BCLK`, `LRCK`, and data on the wires

**Enable 40-pin I2S2 pinmux**

Use Jetson-IO if your image supports it:

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
```

Select the 40-pin header configuration that enables `I2S2`, save the overlay, and reboot.

After reboot:

```bash
amixer -c APE controls | grep I2S2
```

**Internal loopback first**

Before using the LyraT, prove that Jetson-side routing is alive:

```bash
amixer -c APE cset name="I2S2 Mux" "ADMAIF1"
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
amixer -c APE cset name="I2S2 Loopback" "on"

aplay -D hw:APE,0 test.wav &
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE jetson-i2s2-loopback.wav
```

This does not prove the external pins work. It proves the AHUB route is sane.

---

**9. LyraT-side setup**

On the LyraT, use ESP-ADF or ESP-IDF firmware that:

- initializes the `ES8388`
- selects the onboard microphone input
- configures sample rate, usually `48000`
- configures stereo or mono capture
- drives I2S clocks consistently
- leaves the ADC data visible on `ASDOUT`

For the first Jetson test, choose a boring format:

| Parameter | Recommended first value |
|---|---|
| Sample rate | `48000` Hz |
| Channels | `2` |
| Sample format | `S16_LE` or 16-bit samples inside 32-bit I2S slots |
| Clock role | LyraT master, Jetson slave |
| Data source | LyraT onboard microphone path through `ES8388` ADC |

Expected wire-level clocks for a common stereo 48 kHz setup:

| Signal | Expected behavior |
|---|---|
| `LRCK` | 48 kHz |
| `SCLK` | commonly 1.536 MHz or 3.072 MHz depending on slot width |
| `ASDOUT` | toggles when microphone signal is active |

If `LRCK` or `SCLK` is not present, Jetson cannot capture anything.

</details>

### 参考固件：`lyrat_jp4_passthrough`（已验证）

一个最小的、仅采集的 ESP-ADF 固件，满足上述全部要求，位于本地 `esp-adf` 工作树中：

```
esp-adf/examples/recorder/lyrat_jp4_passthrough/
├── CMakeLists.txt
├── Makefile
├── main/
│   ├── CMakeLists.txt
│   └── lyrat_jp4_passthrough.c
├── partitions_passthrough_example.csv
├── sdkconfig.defaults
└── sdkconfig.defaults.esp32
```

用 90 行 C 代码完成的工作：

1. `audio_board_init()` —— 使用树内的 `lyrat_v4_3` 板级配置；引脚映射见 `components/audio_board/lyrat_v4_3/board_pins_config.c`（无需覆盖）。
2. `audio_hal_ctrl_codec(... ENCODE, START)` —— 将 ES8388 置为 ADC 模式，驱动 ASDOUT/`GPIO35`。
3. 构建 `i2s_stream` reader，参数为 48 kHz / 16-bit / stereo / Philips，master 角色。
4. `raw_stream` sink + 一个小的 drain 任务丢弃字节 —— 该固件存在的唯一目的就是让 `BCLK`/`LRCK` 保持存活，并让 codec ADC 持续在 JP4 总线上推送数据。Jetson 才是真正的消费方。
5. 每 2 s 记录一次 `rate=N B/s (expected=192000 B/s)`，作为 runtime canary。

drain sink 为何重要：如果无人消费 ringbuffer，ESP-ADF 的 `i2s_stream` reader 就会停滞（并停止 I2S 时钟）。一个极小的 bytes-to-`/dev/null` 任务，是保证总线持续活跃的最简单方式。

**Upstream tracking：** [`espressif/esp-adf#1607`](https://github.com/espressif/esp-adf/issues/1607)（请求把该示例合并到上游的 feature request）。

**构建（macOS / Linux）：**

```bash
# One-time host prereqs (macOS)
brew install cmake ninja dfu-util
cd esp-adf/esp-idf && ./install.sh esp32

# Build
cd esp-adf/esp-idf && . ./export.sh
export ADF_PATH=$(realpath ..)
cd ../examples/recorder/lyrat_jp4_passthrough
idf.py set-target esp32 build
```

**生成的烧录产物**（偏移由项目的 `partitions_passthrough_example.csv` 固定）：

| 文件 | 偏移 |
|---|---|
| `build/bootloader/bootloader.bin` | `0x1000` |
| `build/partition_table/partition-table.bin` | `0x8000` |
| `build/lyrat_jp4_passthrough.bin` | `0x10000` |

**从 Windows 主机烧录**（一根 USB micro-B 线接到 LyraT —— 自动 bootloader 可用；CP2102N 枚举为 `COMx`）：

```powershell
pip install esptool
python -m esptool --chip esp32 -p COM3 -b 460800 erase_flash
python -m esptool --chip esp32 -p COM3 -b 460800 write_flash --flash_mode dio --flash_size detect --flash_freq 80m 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 build\lyrat_jp4_passthrough.bin
```

Espressif 的 “Flash Download Tool” GUI 同样可用，但有两处坑确实在这套 bring-up（上电点亮/调通）里踩过：

- 每个文件行左侧都有一个**复选框** —— 若未勾选，该行会被静默跳过。症状：`START` 在几毫秒内结束，板子启动的是一份无关固件（例如出厂蓝牙音箱 demo）。
- 之前会话的陈旧偏移会被自动填充。如果 `partition-table.bin` 缺失或其偏移错误，bootloader 会以 `flash_parts: partition 0 invalid magic number 0x...` / `Failed to verify partition table` 反复循环。重新输入全部三个偏移，并使用一次 **EraseAll** 即可恢复。

从 PowerShell 运行 `esptool` 可让这两类错误都不可能发生 —— 偏移在命令行上显式给出。

---


<details>
<summary>English original</summary>

**Reference firmware: `lyrat_jp4_passthrough` (verified)**

A minimal, capture-only ESP-ADF firmware that satisfies all of the requirements above is available in the local `esp-adf` working tree:

```
esp-adf/examples/recorder/lyrat_jp4_passthrough/
├── CMakeLists.txt
├── Makefile
├── main/
│   ├── CMakeLists.txt
│   └── lyrat_jp4_passthrough.c
├── partitions_passthrough_example.csv
├── sdkconfig.defaults
└── sdkconfig.defaults.esp32
```

What it does, in 90 lines of C:

1. `audio_board_init()` — uses the in-tree `lyrat_v4_3` board config; pin map at `components/audio_board/lyrat_v4_3/board_pins_config.c` (no overrides needed).
2. `audio_hal_ctrl_codec(... ENCODE, START)` — puts ES8388 into ADC mode driving ASDOUT/`GPIO35`.
3. Builds an `i2s_stream` reader at 48 kHz / 16-bit / stereo / Philips, master role.
4. `raw_stream` sink + a small drain task discards bytes — the firmware exists only to keep `BCLK`/`LRCK` alive and the codec ADC pushing data on the JP4 bus. Jetson is the actual consumer.
5. Logs `rate=N B/s (expected=192000 B/s)` every 2 s as a runtime canary.

Why the drain sink matters: ESP-ADF's `i2s_stream` reader will stall (and stop the I2S clocks) if no one consumes the ringbuffer. A tiny bytes-to-`/dev/null` task is the simplest way to guarantee the bus stays live.

**Upstream tracking:** [`espressif/esp-adf#1607`](https://github.com/espressif/esp-adf/issues/1607) (feature request to merge this example upstream).

**Build (macOS / Linux):**

```bash
# One-time host prereqs (macOS)
brew install cmake ninja dfu-util
cd esp-adf/esp-idf && ./install.sh esp32

# Build
cd esp-adf/esp-idf && . ./export.sh
export ADF_PATH=$(realpath ..)
cd ../examples/recorder/lyrat_jp4_passthrough
idf.py set-target esp32 build
```

**Flash artifacts produced** (offsets fixed by the project's `partitions_passthrough_example.csv`):

| File | Offset |
|---|---|
| `build/bootloader/bootloader.bin` | `0x1000` |
| `build/partition_table/partition-table.bin` | `0x8000` |
| `build/lyrat_jp4_passthrough.bin` | `0x10000` |

**Flash from a Windows host** (one USB micro-B cable to LyraT — auto-bootloader works; the CP2102N enumerates as `COMx`):

```powershell
pip install esptool
python -m esptool --chip esp32 -p COM3 -b 460800 erase_flash
python -m esptool --chip esp32 -p COM3 -b 460800 write_flash --flash_mode dio --flash_size detect --flash_freq 80m 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 build\lyrat_jp4_passthrough.bin
```

Espressif's "Flash Download Tool" GUI works too, but two gotchas have bitten this exact bring-up:

- Each file row has a **checkbox on the left** — if unchecked, the row is silently skipped. Symptom: `START` finishes in milliseconds and the board boots an unrelated firmware (e.g. the factory Bluetooth speaker demo).
- Stale offsets autofill from previous sessions. If `partition-table.bin` is missing or its offset is wrong, the bootloader loops with `flash_parts: partition 0 invalid magic number 0x...` / `Failed to verify partition table`. Re-typing all three offsets and using **EraseAll** once recovers it.

`esptool` from PowerShell makes both classes of mistake impossible — the offsets are explicit on the command line.

---

</details>

## 10. 在 Jetson 上做外部捕获

当 LyraT 已经在产生时钟和 ADC 数据后，就可以路由 Jetson 捕获：

```bash
amixer -c APE cset name="I2S2 Loopback" "off"
amixer -c APE cset name="I2S2 codec frame mode" "i2s"
amixer -c APE cset name="I2S2 codec master mode" "cbm-cfm"
amixer -c APE cset name="I2S2 Sample Rate" "48000"
amixer -c APE cset name="I2S2 Capture Audio Channels" "2"
amixer -c APE cset name="I2S2 Client Channels" "2"
amixer -c APE cset name="I2S2 Capture Audio Bit Format" "16"
amixer -c APE cset name="I2S2 Client Bit Format" "16"
amixer -c APE cset name="ADMAIF1 Capture Audio Channels" "2"
amixer -c APE cset name="ADMAIF1 Capture Client Channels" "2"
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
```

先试一组保守的捕获参数：

```bash
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE lyrat-i2s-capture.wav
```

检查文件：

```bash
file lyrat-i2s-capture.wav
aplay lyrat-i2s-capture.wav
```

如果回放是静音，检查信号电平：

```bash
sox lyrat-i2s-capture.wav -n stat
```

这些设置为什么重要：

- `I2S2 codec frame mode = i2s` 匹配 LyraT 常规的 `ES8388` I2S 帧格式
- `I2S2 codec master mode = cbm-cfm` 表示外部板是时钟主设备，Jetson 是从设备
- `ADMAIF1 Mux = I2S2` 把外部 `I2S2` 接收数据路由进 `hw:APE,0`
- 通道和位格式设置让 I2S CIF 侧与 ADMAIF 侧对齐

如果 LyraT 固件发送的是 32-bit slot 但只有 16-bit 有效麦克风样本，就试第二个变体：

```bash
amixer -c APE cset name="I2S2 Capture Audio Bit Format" "32"
amixer -c APE cset name="I2S2 Client Bit Format" "32"
arecord -D hw:APE,0 -r 48000 -c 2 -f S32_LE lyrat-i2s-capture-32.wav
```

如果你有意重新设计测试，让 Jetson 驱动 `BCLK` 和 `LRCK`，就要把时钟角色的假设反过来，并修改：

```bash
amixer -c APE cset name="I2S2 codec master mode" "cbs-cfs"
```

有用的快速检查：

- 静音且没有时钟：接线或 LyraT 固件问题
- 静音但有时钟：I2S 数据线、codec 增益或路由问题
- 音频失真：位深、slot 宽度或主/从配置不匹配
- 一个通道无声：codec 输入路由或通道顺序问题

---

## 11. 重要限制：这可能需要一个真正的 ASoC 绑定

Jetson 音频并不只是"GPIO 加 arecord"。

对于一块稳健的外部 I2S 采集卡，Jetson 通常需要：

- `I2S2` 引脚复用为 SFIO
- 一个使能的 `i2s2` 控制器
- 一条 sound-card DAI link
- codec 或 dummy-codec 绑定
- 正确的主/从时钟配置
- 正确的样本格式和 TDM/I2S 设置

如果你只用 mixer 命令和 `arecord`，可能会触及默认 Jetson sound card 所能暴露的能力上限。这很正常。

对于量产板，走音频深入章节里的完整路径：

```text
device tree overlay
  -> codec or dummy-codec node
  -> DAI link for I2S2
  -> AHUB route
  -> ALSA PCM
```

---

## 12. 由 Jetson 直接控制 LyraT `ES8388`

这是一条进阶路径，不是第一次 bring-up（上电点亮/调通）该走的路径。

它大致会是这样：

```text
Jetson I2C -> ES8388 control registers
Jetson I2S2 <-> ES8388 I2S audio
ESP32 side disabled or kept from driving the same bus
```

难点在哪：

- LyraT 的设计就是让 ESP32 拥有 codec
- ESP32 和 `ES8388` 已经连接好了
- Jetson 和 ESP32 不能同时驱动 I2C/I2S
- Linux 必须有可用的 `ES8388` codec 驱动和 DAI 绑定
- 板级上拉和 strap 行为可能有影响

只有当你刻意要把 LyraT 变成一块 codec 转接板时，才用这条路径。

对于真正的 Jetson 音频产品，通常更干净的做法是设计或购买一块小的 `ES7210`、`ES8388`、`TLV320` 或类似的 codec 板，专门供 Jetson 控制。

---

## 13. 调试检查清单

### Jetson 检查

```bash
arecord -l
aplay -l
amixer -c APE controls | grep I2S2
amixer -c APE controls | grep ADMAIF
dmesg | grep -i -E 'asoc|tegra|i2s|audio'
```

### 接线检查

用示波器或逻辑分析仪：

- `LRCK` 以预期的采样率翻转
- `SCLK` 在捕获期间持续翻转
- `ASDOUT` 在有声音到达麦克风时发生变化
- 没有两个设备同时驱动同一条时钟线
- 共地

### 格式检查

试常见的几种捕获变体：

```bash
arecord -D hw:APE,0 -r 48000 -c 1 -f S16_LE test-mono.wav
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE test-stereo.wav
arecord -D hw:APE,0 -r 48000 -c 2 -f S32_LE test-stereo-32.wav
```

用与 LyraT 固件的 I2S slot 格式相匹配的那一个。

---


<details>
<summary>English original</summary>

**10. External capture on Jetson**

Once the LyraT is generating clocks and ADC data, route Jetson capture:

```bash
amixer -c APE cset name="I2S2 Loopback" "off"
amixer -c APE cset name="I2S2 codec frame mode" "i2s"
amixer -c APE cset name="I2S2 codec master mode" "cbm-cfm"
amixer -c APE cset name="I2S2 Sample Rate" "48000"
amixer -c APE cset name="I2S2 Capture Audio Channels" "2"
amixer -c APE cset name="I2S2 Client Channels" "2"
amixer -c APE cset name="I2S2 Capture Audio Bit Format" "16"
amixer -c APE cset name="I2S2 Client Bit Format" "16"
amixer -c APE cset name="ADMAIF1 Capture Audio Channels" "2"
amixer -c APE cset name="ADMAIF1 Capture Client Channels" "2"
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
```

Try a conservative capture:

```bash
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE lyrat-i2s-capture.wav
```

Inspect the file:

```bash
file lyrat-i2s-capture.wav
aplay lyrat-i2s-capture.wav
```

If playback is silent, inspect signal level:

```bash
sox lyrat-i2s-capture.wav -n stat
```

Why these settings matter:

- `I2S2 codec frame mode = i2s` matches the normal LyraT `ES8388` I2S framing
- `I2S2 codec master mode = cbm-cfm` means the external board is clock master and Jetson is the slave
- `ADMAIF1 Mux = I2S2` routes external `I2S2` receive data into `hw:APE,0`
- the channel and bit-format settings keep the I2S CIF side and ADMAIF side aligned

If your LyraT firmware sends 32-bit slots with 16-bit valid microphone samples, try this second variant:

```bash
amixer -c APE cset name="I2S2 Capture Audio Bit Format" "32"
amixer -c APE cset name="I2S2 Client Bit Format" "32"
arecord -D hw:APE,0 -r 48000 -c 2 -f S32_LE lyrat-i2s-capture-32.wav
```

If you intentionally redesign the test so Jetson drives `BCLK` and `LRCK`, reverse the clock-role assumption and change:

```bash
amixer -c APE cset name="I2S2 codec master mode" "cbs-cfs"
```

Useful quick checks:

- silence with no clocks: wiring or LyraT firmware problem
- silence with clocks: I2S data line, codec gain, or route problem
- distorted audio: bit depth, slot width, or master/slave mismatch
- one channel dead: codec input route or channel ordering problem

---

**11. Important limitation: this may need a real ASoC binding**

Jetson audio is not just "GPIO plus arecord."

For a robust external I2S capture card, Jetson normally needs:

- `I2S2` pinmux as SFIO
- an enabled `i2s2` controller
- a sound-card DAI link
- codec or dummy-codec binding
- correct master/slave clock configuration
- correct sample format and TDM/I2S settings

If you only use mixer commands and `arecord`, you may reach the limit of what the default Jetson sound card exposes. That is normal.

For a production board, use the full path from the audio deep dive:

```text
device tree overlay
  -> codec or dummy-codec node
  -> DAI link for I2S2
  -> AHUB route
  -> ALSA PCM
```

---

**12. Direct Jetson control of LyraT `ES8388`**

This is the advanced path, not the first bring-up path.

It would look like this:

```text
Jetson I2C -> ES8388 control registers
Jetson I2S2 <-> ES8388 I2S audio
ESP32 side disabled or kept from driving the same bus
```

Why it is hard:

- the LyraT was designed for ESP32 to own the codec
- the ESP32 and `ES8388` are already connected
- Jetson and ESP32 must not both drive I2C/I2S
- Linux must have a working `ES8388` codec driver and DAI binding
- board-level pull-ups and strap behavior may matter

Use this path only if you intentionally turn the LyraT into a codec breakout board.

For a real Jetson audio product, it is usually cleaner to design or buy a small `ES7210`, `ES8388`, `TLV320`, or similar codec board intended to be controlled by the Jetson.

---

**13. Debug checklist**

**Jetson checks**

```bash
arecord -l
aplay -l
amixer -c APE controls | grep I2S2
amixer -c APE controls | grep ADMAIF
dmesg | grep -i -E 'asoc|tegra|i2s|audio'
```

**Wire checks**

Use a scope or logic analyzer:

- `LRCK` toggles at the expected sample rate
- `SCLK` toggles continuously during capture
- `ASDOUT` changes when sound reaches the microphone
- no two devices are driving the same clock line
- ground is shared

**Format checks**

Try common capture variants:

```bash
arecord -D hw:APE,0 -r 48000 -c 1 -f S16_LE test-mono.wav
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE test-stereo.wav
arecord -D hw:APE,0 -r 48000 -c 2 -f S32_LE test-stereo-32.wav
```

Use the one that matches the LyraT firmware's I2S slot format.

---

</details>

## 14. 成功的标志

最低限度的成功：

- LyraT 输出稳定的 I2S 时钟
- Jetson `I2S2` 在 40-pin 排针上已启用
- `ADMAIF1` 从 `I2S2` 路由出来
- `arecord` 生成非静音的 WAV 文件
- 通道数与采样率正确

更好的成功：

- 波形显示出干净的时钟与数据
- 语音可听清
- 增益未削波
- 左/右声道含义明确
- 重启后可反复录音

产品级成功：

- 正规的 codec 或 ADC 驱动
- 稳定的 device-tree overlay
- 可复现的 ALSA 路由设置
- GStreamer 流水线
- 应用层音频健康检查
- 使用所采集音频的唤醒词或 ASR 流水线

### 已验证测试结果 — LyraT 侧（2026-05-11）

将 `lyrat_jp4_passthrough.bin`（由 `esp-adf` `release/v2.x` 构建，ESP-IDF v5.5.3）烧录到 ESP32-LyraT V4.3。启动日志确认 codec/I2S 通路已工作，DMA 正在搬运真实数据：

```
I (172) boot: Loaded app from partition at offset 0x10000
I (197) app_init: Project name:     lyrat_jp4_passthrough
I (215) app_init: ESP-IDF:          v5.5.3
I (298) JP4_PASSTHROUGH: [1] Init audio board (LyraT v4.3) + ES8388 codec
I (329) JP4_PASSTHROUGH:     codec mode=ENCODE (ADC), input=LINPUT1/RINPUT1 (onboard mic)
I (334) JP4_PASSTHROUGH: [3] I2S: rate=48000 Hz, bits=16, channels=2, format=Philips, role=master
I (340) JP4_PASSTHROUGH:     JP4 pins: BCLK=GPIO5  LRCK=GPIO25  ASDOUT=GPIO35  MCLK=GPIO0
I (349) JP4_PASSTHROUGH: [4] Capture started — JP4 I2S bus is live. Samples drained on-chip.
I (2381) JP4_PASSTHROUGH: rate=194560 B/s (expected=192000 B/s)  total=389120 B
I (4389) JP4_PASSTHROUGH: rate=192512 B/s (expected=192000 B/s)  total=774144 B
I (6411) JP4_PASSTHROUGH: rate=194560 B/s (expected=192000 B/s)  total=1163264 B
```

这印证了 bring-up（上电点亮/调通）计划中 LyraT 侧的内容（第 7 节，步骤 1–4）：

- ✅ ES8388 已枚举并置于 ENCODE/ADC 模式
- ✅ 板载麦克风（`LINPUT1/RINPUT1`）已选中
- ✅ I2S 外设以 master 运行，`BCLK`/`LRCK` 在 JP4 上提供时钟
- ✅ DMA 正以 192 ± 1.3 % kB/s = 每秒 `48000 Hz × 2 ch × 2 B` 的样本量搬运数据（这点轻微超出源自 2 s 上报窗口内 `xTaskGetTickCount()` 的粒度，而非时钟漂移）

Jetson 侧（步骤 5–7）在下方单独验证 — 见 [§14 已验证结果 — Jetson 侧](#verified-test-result-jetson-side-2026-05-11)。

LyraT 侧的产物是已知良好的对照：若在该固件烧录后 Jetson 采集出现回归，故障在 Jetson 软件路径（pinmux、ASoC 路由、master/slave 配置），而非总线。

### 已验证测试结果 — Jetson 侧（2026-05-11）

在上述 LyraT bring-up 之后，JP4 的四根跳线全部接到 Jetson Orin Nano 的 40-pin 排针（`SCLK→12`、`LRCK→35`、`ASDOUT→38`、`GND→6`），共用同一个地。无需重建 JetPack，无需 out-of-tree 驱动 — 仅需 Jetson-IO overlay + `amixer` 路由。

#### 步骤 1 — 确认 overlay 已生效

```bash
amixer -c APE controls | grep I2S2          # expect ~19 I2S2 controls present
amixer -c APE cget name="ADMAIF1 Mux"       # expect 'I2S2' in items list
arecord -l                                  # expect "card 1: APE ... device 0: XBAR-ADMAIF1-0"
```

若 `I2S2 Mux`、`I2S2 codec frame mode` 和 `I2S2 codec master mode` 不在控制项列表中，说明 40-pin overlay 未生效 — 重新运行 `sudo /opt/nvidia/jetson-io/jetson-io.py`，保存，重启。

#### 步骤 2 — 设置 AHUB 路由与 I2S 格式

```bash
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
amixer -c APE cset name="I2S2 codec master mode" "cbm-cfm"     # LyraT drives BCLK + LRCK
amixer -c APE cset name="I2S2 codec frame mode" "i2s"          # Philips I2S framing
amixer -c APE cset name="I2S2 Sample Rate" 48000
amixer -c APE cset name="I2S2 Capture Audio Channels" 2
amixer -c APE cset name="I2S2 Capture Audio Bit Format" 16
amixer -c APE cset name="I2S2 Client Channels" 2
amixer -c APE cset name="I2S2 Client Bit Format" 16
amixer -c APE cset name="ADMAIF1 Capture Audio Channels" 2
amixer -c APE cset name="ADMAIF1 Capture Client Channels" 2
```

各控制项的作用：

- `cbm-cfm` = "codec-bit-master, codec-frame-master" → Jetson 是 I2S slave；两个时钟均由 LyraT 驱动。与固件中的 `I2S_ROLE_MASTER` 一致。
- `i2s` 帧格式 = 标准 Philips I2S。与 ES8388 / 我们的 `I2S_COMM_FORMAT_STAND_I2S` 一致。
- 各处 `48000 / 2 ch / 16-bit` → 与固件中的 `SAMPLE_RATE_HZ / SAMPLE_CHANNELS / SAMPLE_BITS` 一致。此处任一侧不匹配，是失真或无声最常见的原因。

#### 步骤 3 — 录音并检查

```bash
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE -d 5 lyrat.wav
ls -la lyrat.wav
sox lyrat.wav -n stat
aplay lyrat.wav
```


<details>
<summary>English original</summary>

**14. What success looks like**

Minimum success:

- LyraT outputs stable I2S clocks
- Jetson `I2S2` is enabled on the 40-pin header
- `ADMAIF1` routes from `I2S2`
- `arecord` creates a non-silent WAV file
- channel count and sample rate are correct

Better success:

- waveforms show clean clocks and data
- spoken audio is intelligible
- gain is not clipped
- left/right channels are understood
- recording works repeatedly after reboot

Product-level success:

- proper codec or ADC driver
- stable device-tree overlay
- reproducible ALSA route setup
- GStreamer pipeline
- application-level audio health checks
- wake-word or ASR pipeline using the captured audio

**Verified test result — LyraT side (2026-05-11)**

Flashed `lyrat_jp4_passthrough.bin` (built from `esp-adf` `release/v2.x`, ESP-IDF v5.5.3) to an ESP32-LyraT V4.3. Boot log confirms the codec/I2S path is live and DMA is moving real data:

```
I (172) boot: Loaded app from partition at offset 0x10000
I (197) app_init: Project name:     lyrat_jp4_passthrough
I (215) app_init: ESP-IDF:          v5.5.3
I (298) JP4_PASSTHROUGH: [1] Init audio board (LyraT v4.3) + ES8388 codec
I (329) JP4_PASSTHROUGH:     codec mode=ENCODE (ADC), input=LINPUT1/RINPUT1 (onboard mic)
I (334) JP4_PASSTHROUGH: [3] I2S: rate=48000 Hz, bits=16, channels=2, format=Philips, role=master
I (340) JP4_PASSTHROUGH:     JP4 pins: BCLK=GPIO5  LRCK=GPIO25  ASDOUT=GPIO35  MCLK=GPIO0
I (349) JP4_PASSTHROUGH: [4] Capture started — JP4 I2S bus is live. Samples drained on-chip.
I (2381) JP4_PASSTHROUGH: rate=194560 B/s (expected=192000 B/s)  total=389120 B
I (4389) JP4_PASSTHROUGH: rate=192512 B/s (expected=192000 B/s)  total=774144 B
I (6411) JP4_PASSTHROUGH: rate=194560 B/s (expected=192000 B/s)  total=1163264 B
```

What this confirms about the LyraT side of the bring-up plan (Section 7, steps 1–4):

- ✅ ES8388 enumerated and put in ENCODE/ADC mode
- ✅ Onboard mics (`LINPUT1/RINPUT1`) selected
- ✅ I2S peripheral running as master, `BCLK`/`LRCK` clocking on JP4
- ✅ DMA is moving 192 ± 1.3 % kB/s = `48000 Hz × 2 ch × 2 B` worth of samples per second (the small overshoot is `xTaskGetTickCount()` granularity in the 2 s reporting window, not clock drift)

The Jetson side (steps 5–7) is verified separately below — see [§14 verified result — Jetson side](#verified-test-result-jetson-side-2026-05-11).

The LyraT-side artifact is a known-good control: if Jetson capture ever regresses after this firmware is flashed, the failure is on the Jetson software path (pinmux, ASoC routing, master/slave config), not the bus.

**Verified test result — Jetson side (2026-05-11)**

After the LyraT bring-up above, all four JP4 jumpers wired to the Jetson Orin Nano 40-pin header (`SCLK→12`, `LRCK→35`, `ASDOUT→38`, `GND→6`) on a single shared ground. No JetPack rebuild, no out-of-tree driver — only Jetson-IO overlay + `amixer` route.

**Step 1 — confirm overlay applied**

```bash
amixer -c APE controls | grep I2S2          # expect ~19 I2S2 controls present
amixer -c APE cget name="ADMAIF1 Mux"       # expect 'I2S2' in items list
arecord -l                                  # expect "card 1: APE ... device 0: XBAR-ADMAIF1-0"
```

If `I2S2 Mux`, `I2S2 codec frame mode`, and `I2S2 codec master mode` are not in the control list, the 40-pin overlay didn't take — re-run `sudo /opt/nvidia/jetson-io/jetson-io.py`, save, reboot.

**Step 2 — set the AHUB route and I2S format**

```bash
amixer -c APE cset name="ADMAIF1 Mux" "I2S2"
amixer -c APE cset name="I2S2 codec master mode" "cbm-cfm"     # LyraT drives BCLK + LRCK
amixer -c APE cset name="I2S2 codec frame mode" "i2s"          # Philips I2S framing
amixer -c APE cset name="I2S2 Sample Rate" 48000
amixer -c APE cset name="I2S2 Capture Audio Channels" 2
amixer -c APE cset name="I2S2 Capture Audio Bit Format" 16
amixer -c APE cset name="I2S2 Client Channels" 2
amixer -c APE cset name="I2S2 Client Bit Format" 16
amixer -c APE cset name="ADMAIF1 Capture Audio Channels" 2
amixer -c APE cset name="ADMAIF1 Capture Client Channels" 2
```

Why each control:

- `cbm-cfm` = "codec-bit-master, codec-frame-master" → Jetson is the I2S slave; LyraT drives both clocks. Matches the firmware's `I2S_ROLE_MASTER`.
- `i2s` framing = standard Philips I2S. Matches the ES8388 / our `I2S_COMM_FORMAT_STAND_I2S`.
- `48000 / 2 ch / 16-bit` everywhere → matches the firmware's `SAMPLE_RATE_HZ / SAMPLE_CHANNELS / SAMPLE_BITS`. Mismatch on any side here is the most common cause of distortion or silence.

**Step 3 — record and inspect**

```bash
arecord -D hw:APE,0 -r 48000 -c 2 -f S16_LE -d 5 lyrat.wav
ls -la lyrat.wav
sox lyrat.wav -n stat
aplay lyrat.wav
```

</details>

#### 观测结果

```
-rw-r--r-- 1 aihpc aihpc 960044 May 10 20:14 lyrat.wav

Samples read:            480000
Length (seconds):      5.000000
Maximum amplitude:     0.233582
Minimum amplitude:    -0.178314
Midline amplitude:     0.027634
Mean    amplitude:    -0.000075
RMS     amplitude:     0.021088
Rough   frequency:         1356
Volume adjustment:        4.281
```

语音（"one two three test"）在 `aplay` 上清晰可闻 —— bit-accurate 采集。

每个数字证明了什么：

- **文件大小 960,044 字节完全精确**：`5.000 s × 48000 Hz × 2 ch × 2 B + 44 B WAV header = 960044`。DMA 无 underrun；LyraT-master / Jetson-slave 时钟稳如磐石。
- `RMS amplitude 0.021` ≈ –33 dBFS —— 正常语音范围，远高于本底噪声。
- `Mean amplitude -0.000075` ≈ 0 —— 无 DC 偏置；ES8388 输入偏置干净。
- `Volume adjustment 4.281` → 削波前约 12 dB 余量。若希望采集得更响，应在固件（`es8388_set_mic_gain()`）中调高 ES8388 PGA 增益，而不是做数字放大。
- `Rough frequency 1356` —— 语音频段内的主导音高；与语音内容一致。

#### Bring-up（上电点亮/调通）计划 §7 —— 最终状态

- ✅ 步骤 5：40-pin `I2S2` pinmux 已应用（经由 Jetson-IO overlay）
- ✅ 步骤 6：已配置从 `I2S2` → `ADMAIF1` 的 AHUB 路由；时钟方向设为 Jetson-slave
- ✅ 步骤 7：`arecord -D hw:APE,0` 生成 bit-accurate、byte-exact、可听的 WAV

#### 已知缺口 —— 设置在重启后无法保留

这 9 条 `amixer cset` 命令不是持久化的。`alsactl store` 能捕获其中大部分，但 NVIDIA APE 声卡的 `Mux` / `codec master mode` 控件历来无法被 `alsa-restore.service` 干净地恢复。若要在多次重启间获得可复现的 bring-up，可把这些命令封进一个小的 `systemd` unit，让它在 `sound.target` 之后（或在 Jetson-IO overlay 加载之后）运行；也可以用 `alsactl store -f /var/lib/alsa/asound.state` 保存，并在依赖它之前用 `alsactl restore` 验证。

#### 锚定于这条已验证基线的后续步骤

采集既然已经 bit-accurate，接下来的各层（第 15 节）便不再受阻：

- 用 GStreamer `alsasrc` 替换 `arecord`，并扇出到多个消费者
- 流式 ASR（`whisper.cpp` 配合 Orin 上的 CUDA）消费 alsasrc tap
- 双 mic 延迟求和波束成形，利用 LyraT 的 L+R 麦克风（实测物理间距 **3.5 cm** —— 注意这低于 Espressif 推荐的 4–6.5 cm，也低于 `esp-sr/include/.../esp_mase.h` 中的默认 `MASE_MIC_DISTANCE = 65 mm`，因此凡是用到 Espressif 的 `esp_afe_doa` / `MASE` API，都需要把 `mic_distance` 显式覆盖为 `0.035`）
- 最终，为真实产品把 LyraT 换成一块由 Jetson 控制的 4-mic codec 板（例如 ES7210）

---

## 15. 后续应用步骤

原始采集通路打通之后：

- 把 `arecord` 通路改造成 GStreamer 采集流水线
- 加入 WebRTC VAD 或唤醒词检测
- 测试本地 ASR，例如 Whisper 或更小的流式 ASR 模型
- 用一块专用的、由 Jetson 控制的 codec 板替换 LyraT
- 为智能音箱产品路线设计真正的麦克风前端

GStreamer 采集的示例起点：

```bash
gst-launch-1.0 alsasrc device=hw:APE,0 ! \
  audio/x-raw,rate=48000,channels=2,format=S16LE ! \
  wavenc ! filesink location=lyrat-i2s-gst.wav
```

---

## 16. 参考文献

- [NVIDIA Jetson Linux Developer Guide - Audio Setup and Development](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/SD/Communications/AudioSetupAndDevelopment.html)
- [NVIDIA Jetson Linux Developer Guide - Configuring the Jetson Expansion Headers](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/HR/ConfiguringTheJetsonExpansionHeaders.html)
- [ESP32-LyraT V4.3 Hardware Reference](https://espressif-docs.readthedocs-hosted.com/projects/esp-adf/en/latest/design-guide/dev-boards/board-esp32-lyrat-v4.3.html)
- [ESP32-LyraT V4.3 schematic](https://dl.espressif.com/dl/schematics/esp32-lyrat-v4.3-schematic.pdf)
- [Jetson Audio Setup and Development](../02-Jetson音频设置与开发/Guide.md)


<details>
<summary>English original</summary>

**Observed result**

```
-rw-r--r-- 1 aihpc aihpc 960044 May 10 20:14 lyrat.wav

Samples read:            480000
Length (seconds):      5.000000
Maximum amplitude:     0.233582
Minimum amplitude:    -0.178314
Midline amplitude:     0.027634
Mean    amplitude:    -0.000075
RMS     amplitude:     0.021088
Rough   frequency:         1356
Volume adjustment:        4.281
```

Speech ("one two three test") was clearly audible on `aplay` — bit-accurate capture.

What each number proves:

- **File size 960,044 bytes is exact**: `5.000 s × 48000 Hz × 2 ch × 2 B + 44 B WAV header = 960044`. Zero DMA underruns; LyraT-master / Jetson-slave clocking is rock solid.
- `RMS amplitude 0.021` ≈ –33 dBFS — normal speech range, well above noise floor.
- `Mean amplitude -0.000075` ≈ 0 — no DC offset; ES8388 input biasing is clean.
- `Volume adjustment 4.281` → ~12 dB of headroom before clipping. If louder capture is desired, bump ES8388 PGA gain in firmware (`es8388_set_mic_gain()`) rather than amplifying digitally.
- `Rough frequency 1356` — dominant pitch in the speech band; consistent with voice content.

**Bring-up plan §7 — final state**

- ✅ Step 5: 40-pin `I2S2` pinmux applied (via Jetson-IO overlay)
- ✅ Step 6: AHUB route from `I2S2` → `ADMAIF1` configured; clock direction set to Jetson-slave
- ✅ Step 7: `arecord -D hw:APE,0` produces a bit-accurate, byte-exact, audible WAV

**Known gap — settings do not survive reboot**

The 9 `amixer cset` commands are not persistent. `alsactl store` captures most of them but the NVIDIA APE card's `Mux` / `codec master mode` controls have historically not been restored cleanly by `alsa-restore.service`. For a reproducible bring-up across reboots, wrap the commands in a small `systemd` unit that runs after `sound.target` (or after the Jetson-IO overlay loads), or save with `alsactl store -f /var/lib/alsa/asound.state` and verify with `alsactl restore` before relying on it.

**Next steps anchored to this verified baseline**

Now that capture is bit-accurate, the next layers (Section 15) are unblocked:

- GStreamer `alsasrc` to replace `arecord` and fan-out to multiple consumers
- Streaming ASR (`whisper.cpp` with CUDA on Orin) consuming the alsasrc tap
- Two-mic delay-and-sum beamforming, taking advantage of LyraT's L+R mics (physical spacing measured **3.5 cm** — note this is below Espressif's recommended 4–6.5 cm and the default `MASE_MIC_DISTANCE = 65 mm` in `esp-sr/include/.../esp_mase.h`, so any use of Espressif's `esp_afe_doa` / `MASE` APIs needs `mic_distance` explicitly overridden to `0.035`)
- Eventually, replacing the LyraT with a Jetson-controlled 4-mic codec board (e.g. ES7210) for a real product

---

**15. Next application steps**

Once the raw capture path works:

- convert the `arecord` path into a GStreamer capture pipeline
- add WebRTC VAD or wake-word detection
- test local ASR such as Whisper or a smaller streaming ASR model
- replace LyraT with a dedicated Jetson-controlled codec board
- design a real microphone frontend for the smart-speaker product path

Example GStreamer capture starting point:

```bash
gst-launch-1.0 alsasrc device=hw:APE,0 ! \
  audio/x-raw,rate=48000,channels=2,format=S16LE ! \
  wavenc ! filesink location=lyrat-i2s-gst.wav
```

---

**16. References**

- [NVIDIA Jetson Linux Developer Guide - Audio Setup and Development](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/SD/Communications/AudioSetupAndDevelopment.html)
- [NVIDIA Jetson Linux Developer Guide - Configuring the Jetson Expansion Headers](https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/HR/ConfiguringTheJetsonExpansionHeaders.html)
- [ESP32-LyraT V4.3 Hardware Reference](https://espressif-docs.readthedocs-hosted.com/projects/esp-adf/en/latest/design-guide/dev-boards/board-esp32-lyrat-v4.3.html)
- [ESP32-LyraT V4.3 schematic](https://dl.espressif.com/dl/schematics/esp32-lyrat-v4.3-schematic.pdf)
- [Jetson Audio Setup and Development](../02-Jetson音频设置与开发/Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/4. Multimedia/ESP32-LyraT-I2S-Mic-Jetson-Orin-Nano/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/4.%20Multimedia/ESP32-LyraT-I2S-Mic-Jetson-Orin-Nano/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
