---
title: 第 1 讲：AI 驱动的无线通信 —— 神经 PHY、智能无线电与可学习的频谱
description: 第 1 讲：AI 驱动的无线通信 —— 神经 PHY、智能无线电与可学习的频谱
---

# 第 1 讲：AI 驱动的无线通信 —— 神经 PHY、智能无线电与可学习的频谱

## 概述

经典无线协议栈 —— 信道编码、调制、信道估计、均衡、译码 —— 由**手工推导的数学模块**（LDPC、Polar、MMSE、Viterbi）搭建而成，这些模块数十年来针对可处理的信道模型（AWGN、Rayleigh、3GPP TDL）反复调优。而在实际部署中，信道并非上述任何一种：它是**多径、硬件损伤、阻塞信号、干扰与业务流量**混成的一锅不断翻搅的杂烩。AI 驱动的无线用**学习得到的模型**替换或增强这些经典模块，这些模型拟合的是摆在它们面前*实际*的信道与*实际*的硬件。

本讲面向 AI **硬件**工程师，而非通信理论研究者。问题不是「该用哪个损失函数」—— 而是**「NPU/DSP/FPGA 究竟处在射频链的哪个位置、哪些数据跨越这条边界、延迟预算有多紧？」** 本讲涵盖三个层面：

* **Part A —— PHY 层 ML：** 神经接收机、信道估计、波束赋形、基于自编码器的空口。即「1 ms TTI、每 slot 十亿次 MAC」的世界。
* **Part B —— RAN 级 ML：** O-RAN 的 RIC 架构、调度、MIMO 管理、节能。秒级到分钟级的回路，GPU/CPU 放在机房底层。
* **Part C —— SDR + DL：** 调制识别、射频指纹、异常检测，以及真正能在 HackRF / USRP / Jetson 上跑起来的 GNU Radio + PyTorch 流水线。

读完之后，你应当能为神经接收机**估算硅片规模**，能在 UE / DU / CU / RIC 之间正确**放置** ML 推理，并能在通用 SDR 硬件上**搭建**一个小型端到端 demo。

---

## 无线链路与 AI 的插入点

```
   Tx bits ──► Source ──► Channel ──► Modulator ──► Pulse shaping ──► RF ──► Antenna
              coding     coding      (QAM/OFDM)     + DAC          front-end   array
                                                                                 │
                                                                                 ▼
                                                                        Wireless channel
                                                                        (multipath, fading,
                                                                         interference, noise)
                                                                                 │
   Rx bits ◄── Source ◄── Channel ◄── Demod ◄── Equalize ◄── Channel ◄── ADC ◄── RF ◄────┘
              decode     decode      (soft       + sync       est.       front-end
                                      LLR)
                ▲          ▲            ▲          ▲           ▲
                │          │            │          │           │
            (NN decoder)(NN belief    (NN demap)(NN equal.)(NN channel
                        prop)                              est. — the
                                                           biggest win)
```

ML 已经取得**已部署或接近部署**成果的模块，按成熟度排序：

| 模块 | 经典方法 | ML 替代方案 | 状态（2026） |
|---|---|---|---|
| 信道估计 | 基于导频的 LS / LMMSE | 基于导频网格的 CNN / U-Net | **已部署**（Qualcomm、Samsung modem） |
| 解映射 / 软 LLR | 闭式 QAM 解映射 | 逐 RE 的 MLP 解映射器 | **外场试验**，3GPP Rel-18 研究 |
| MIMO 检测 | MMSE / 球形译码器 | DetNet / 展开的 MMSE-Net | 研究 → 试验 |
| 信道译码（LDPC/Polar） | BP / SC-L | NN 辅助 BP、神经列表译码器 | 研究 |
| 波束管理 | 码本扫描 + RSRP | 基于 CSI 的 CNN / 视觉辅助波束预测 | **3GPP Rel-19 WI** |
| 端到端自编码器 PHY | n/a | Tx 与 Rx 联合训练的 NN | 仅研究（尚无空口标准） |
| MAC 调度 | 比例公平等 | RL / 上下文 bandit | **O-RAN xApp 已部署** |
| 射频损伤补偿 | 多项式 DPD | 基于 NN 的 DPD | **已在旗舰 5G PA 中部署** |

关键洞见：**越靠近天线，延迟预算就越紧**。每个 OFDM slot 跑一次的神经信道估计器，含 DMA 在内的端到端时间约 125 µs（5G numerology 1）。而 RAN 节能 xApp 的时间预算是分钟级。

---

## Part A —— PHY 层 ML


<details>
<summary>English original</summary>

**Lecture 1: AI-Driven Wireless Communication — Neural PHY, Smart Radios, and Learned Spectrum**

**Overview**

The classical wireless stack — channel coding, modulation, channel estimation, equalization, decoding — is built from **hand-derived mathematical blocks** (LDPC, Polar, MMSE, Viterbi) that have been tuned over decades against tractable channel models (AWGN, Rayleigh, 3GPP TDL). In real deployments the channel is none of these: it is a moving stew of **multipath, hardware impairments, blockers, interference, and traffic**. AI-driven wireless replaces or augments these classical blocks with **learned models** that fit the *actual* channel and *actual* hardware in front of them.

This lecture is written for the AI **hardware** engineer, not the communications theorist. The question is not "which loss function should we use" — it is **"where in the radio chain does an NPU/DSP/FPGA actually sit, what data crosses that boundary, and how tight is the latency budget?"** We cover three layers:

* **Part A — PHY-layer ML:** neural receivers, channel estimation, beamforming, autoencoder-based air interfaces. The "1 ms TTI, billion-MAC-per-slot" world.
* **Part B — RAN-level ML:** O-RAN's RIC architecture, scheduling, MIMO management, energy savings. Seconds-to-minutes loop, GPU/CPU in the basement.
* **Part C — SDR + DL:** modulation classification, RF fingerprinting, anomaly detection, GNU Radio + PyTorch pipelines you can actually run on a HackRF / USRP / Jetson.

By the end you should be able to **size the silicon** for a neural receiver, **place** ML inference correctly across UE / DU / CU / RIC, and **build** a small end-to-end demo on commodity SDR hardware.

---

**The Wireless Chain and Where AI Plugs In**

```
   Tx bits ──► Source ──► Channel ──► Modulator ──► Pulse shaping ──► RF ──► Antenna
              coding     coding      (QAM/OFDM)     + DAC          front-end   array
                                                                                 │
                                                                                 ▼
                                                                        Wireless channel
                                                                        (multipath, fading,
                                                                         interference, noise)
                                                                                 │
   Rx bits ◄── Source ◄── Channel ◄── Demod ◄── Equalize ◄── Channel ◄── ADC ◄── RF ◄────┘
              decode     decode      (soft       + sync       est.       front-end
                                      LLR)
                ▲          ▲            ▲          ▲           ▲
                │          │            │          │           │
            (NN decoder)(NN belief    (NN demap)(NN equal.)(NN channel
                        prop)                              est. — the
                                                           biggest win)
```

The blocks where ML has produced **deployed or near-deployed** wins, in order of maturity:

| Block | Classical method | ML replacement | Status (2026) |
|---|---|---|---|
| Channel estimation | LS / LMMSE on pilots | CNN / U-Net on pilot grid | **Deployed** (Qualcomm, Samsung modems) |
| Demapping / soft LLRs | Closed-form QAM demap | MLP demapper per RE | **Field trials**, 3GPP Rel-18 study |
| MIMO detection | MMSE / sphere decoder | DetNet / unfolded MMSE-Net | Research → trials |
| Channel decoding (LDPC/Polar) | BP / SC-L | NN-aided BP, neural list decoder | Research |
| Beam management | Codebook sweep + RSRP | CNN on CSI / vision-aided beam pred. | **3GPP Rel-19 WI** |
| End-to-end autoencoder PHY | n/a | Tx & Rx co-trained NNs | Research only (no air-interface standard) |
| MAC scheduling | Proportional fair, etc. | RL / contextual bandits | **O-RAN xApps deployed** |
| RF impairment compensation | Polynomial DPD | NN-based DPD | **Deployed in flagship 5G PAs** |

The key insight: **the closer to the antenna, the harder the latency budget**. A neural channel estimator running per OFDM slot has ~125 µs (5G numerology 1) end-to-end including DMA. A RAN energy-saving xApp has minutes.

---

**Part A — PHY-Layer ML**

</details>

### A.1 神经信道估计 —— 典型个案研究

在 5G NR 中，UE/gNB 把已知的 **DMRS（解调参考信号）** 导频插入资源网格。经典估计器在导频 RE 上做**最小二乘**，再把结果插值（线性、MMSE、Wiener）到数据 RE 上。若已知信道协方差，MMSE 估计器就是最优的 —— 但实际并不知道，所以已部署的接收机使用针对「平均」3GPP 信道 profile 调优过的简化 Wiener 滤波器。

学习型估计器把导频网格 → 全网格这一问题当作**图像超分辨率**来处理：

```
Input  : tensor[num_pilots_freq × num_pilots_time × 2]   (real, imag)
                + noise variance estimate
Output : tensor[num_subcarriers × num_symbols × 2]       (estimated channel)
```

一个参考架构（ChannelNet / SRCNN 风格）：

```python
import torch
import torch.nn as nn

class ChannelEstNet(nn.Module):
    """
    Input  : LS estimate on pilot grid, zero-filled to full grid.
             Shape: [B, 2, F, T]   (real/imag, subcarriers, symbols)
    Output : Refined channel estimate over the full grid.
    """
    def __init__(self, channels=64):
        super().__init__()
        self.conv1 = nn.Conv2d(2, channels, 9, padding=4)
        self.conv2 = nn.Conv2d(channels, channels // 2, 1)
        self.conv3 = nn.Conv2d(channels // 2, 2, 5, padding=2)
        self.act = nn.ReLU(inplace=True)

    def forward(self, x):                  # x: [B, 2, F, T]
        x = self.act(self.conv1(x))
        x = self.act(self.conv2(x))
        return self.conv3(x)
```

**硬件规模估算 —— 粗算。**

对 5G NR、100 MHz、numerology 1：273 个 PRB × 12 = 3276 个子载波，每时隙 14 个符号，时隙 0.5 ms。DMRS 图样（Type 1，1 符号）每时隙给出约 6552 个导频 RE。

用上述网络并配合 `channels=64`：
- conv1: 2·64·9·9 = 10,368 MACs/输出 × 3276·14 个输出 ≈ 475 M MACs
- conv2: 64·32·1·1 = 2,048 MACs/输出 × 3276·14 ≈ 94 M MACs
- conv3: 32·2·5·5 = 1,600 MACs/输出 × 3276·14 ≈ 73 M MACs

→ **每时隙约 640 M MACs，× 2000 时隙/s = 1.28 TMACs/s = 2.56 TOPS**，对应单个 UE 上的单个 100 MHz 载波。

Qualcomm Hexagon NPU（X75 modem 级别）在手机功耗包络内以 INT8 提供约 10–15 TOPS。所以这个估计器装得下 —— 但只有靠 **INT8、结构化剪枝和 tile 友好的布局** 才行。朴素的 FP16 部署则不行。

**这部分落在硅上的位置：**
- **UE 侧：** modem NPU（Hexagon Tensor、Samsung 的 Exynos NPU、Apple 的 RF 基带神经网络块）。通过片上 SRAM 与 LDPC/Polar 译码器紧耦合。
- **gNB 侧：** 基带 DU。**Nvidia Aerial cuPHY** 在 L4/L40S/A100 GPU 上跑这套东西。**Intel FlexRAN** 在 Xeon AVX-512 / AMX 上跑，可选 FPGA 卸载。

### A.2 神经解映射 —— 廉价、高 ROI 的模块

软解映射器把均衡后的符号转换为供信道译码器使用的**对数似然比（LLR）**。有色干扰下 QAM-256 的精确 LLR 很难算；经典接收机使用高斯近似，在真实条件下会损失 0.3–0.8 dB。

神经解映射器很小 —— 通常是一个**多层感知机**，两个 64–128 单元的隐藏层，**逐资源单元**运行。它高度并行（无时序依赖），能干净地映射到向量引擎上，是最常见的已部署「第一个真正的 ML PHY 模块」。

```python
class NeuralDemapper(nn.Module):
    """Map (equalized_symbol_re, equalized_symbol_im, noise_var) -> LLRs"""
    def __init__(self, bits_per_symbol=8, hidden=128):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(3, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden), nn.ReLU(),
            nn.Linear(hidden, bits_per_symbol),
        )
    def forward(self, x):  # x: [N, 3] -> [N, bits_per_symbol]
        return self.net(x)
```

这是一个**逐 RE 的逐点操作**。吞吐由来自均衡器输出的内存带宽决定，而非由算力决定。在 Hexagon NPU 上，它以 INT8 维持每秒数亿次解映射 —— 对 100 MHz 载波来说很宽裕。


<details>
<summary>English original</summary>

**A.1 Neural channel estimation — the canonical case study**

In 5G NR the UE/gNB inserts known **DMRS (Demodulation Reference Signal)** pilots into the resource grid. The classical estimator does **least-squares** on the pilot REs, then interpolates (linear, MMSE, Wiener) onto the data REs. The MMSE estimator is optimal if you know the channel covariance — which you don't, so deployed receivers use simplified Wiener filters tuned for "average" 3GPP channel profiles.

A learned estimator treats the pilot-grid → full-grid problem as **image super-resolution**:

```
Input  : tensor[num_pilots_freq × num_pilots_time × 2]   (real, imag)
                + noise variance estimate
Output : tensor[num_subcarriers × num_symbols × 2]       (estimated channel)
```

A reference architecture (ChannelNet / SRCNN-style):

```python
import torch
import torch.nn as nn

class ChannelEstNet(nn.Module):
    """
    Input  : LS estimate on pilot grid, zero-filled to full grid.
             Shape: [B, 2, F, T]   (real/imag, subcarriers, symbols)
    Output : Refined channel estimate over the full grid.
    """
    def __init__(self, channels=64):
        super().__init__()
        self.conv1 = nn.Conv2d(2, channels, 9, padding=4)
        self.conv2 = nn.Conv2d(channels, channels // 2, 1)
        self.conv3 = nn.Conv2d(channels // 2, 2, 5, padding=2)
        self.act = nn.ReLU(inplace=True)

    def forward(self, x):                  # x: [B, 2, F, T]
        x = self.act(self.conv1(x))
        x = self.act(self.conv2(x))
        return self.conv3(x)
```

**Hardware sizing — back of the envelope.**

For 5G NR, 100 MHz, numerology 1: 273 PRBs × 12 = 3276 subcarriers, 14 symbols per slot, 0.5 ms slot. The DMRS pattern (Type 1, 1-symbol) gives ~6552 pilot REs per slot.

With the network above and `channels=64`:
- conv1: 2·64·9·9 = 10,368 MACs/output × 3276·14 outputs ≈ 475 M MACs
- conv2: 64·32·1·1 = 2,048 MACs/output × 3276·14 ≈ 94 M MACs
- conv3: 32·2·5·5 = 1,600 MACs/output × 3276·14 ≈ 73 M MACs

→ **≈ 640 M MACs per slot, × 2000 slots/s = 1.28 TMACs/s = 2.56 TOPS** for a single 100 MHz carrier on a single UE.

A Qualcomm Hexagon NPU (X75 modem class) delivers ~10–15 TOPS at INT8 in a phone power envelope. So this estimator fits — but only with **INT8, structured pruning, and tile-friendly layouts**. A naive FP16 deployment would not.

**Where this lives on silicon:**
- **UE side:** modem NPU (Hexagon Tensor, Samsung's Exynos NPU, Apple's RF baseband neural blocks). Tightly coupled to the LDPC/Polar decoder via on-die SRAM.
- **gNB side:** baseband DU. **Nvidia Aerial cuPHY** runs this on an L4/L40S/A100 GPU. **Intel FlexRAN** runs it on Xeon AVX-512 / AMX with optional FPGA offload.

**A.2 Neural demapping — the cheap, high-ROI block**

Soft demappers convert equalized symbols into **log-likelihood ratios (LLRs)** for the channel decoder. The exact LLR for QAM-256 in colored interference is messy; classical receivers use a Gaussian approximation that loses 0.3–0.8 dB in realistic conditions.

A neural demapper is tiny — usually an **MLP** with two hidden layers of 64–128 units, run **per resource element**. It is highly parallel (no temporal dependency), maps cleanly onto vector engines, and is the most common "first real ML PHY block" deployed.

```python
class NeuralDemapper(nn.Module):
    """Map (equalized_symbol_re, equalized_symbol_im, noise_var) -> LLRs"""
    def __init__(self, bits_per_symbol=8, hidden=128):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(3, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden), nn.ReLU(),
            nn.Linear(hidden, bits_per_symbol),
        )
    def forward(self, x):  # x: [N, 3] -> [N, bits_per_symbol]
        return self.net(x)
```

This is a **per-RE pointwise op**. Throughput is gated by memory bandwidth from the equalizer output, not by compute. On a Hexagon NPU it sustains 100s of millions of demaps/sec at INT8 — comfortable for a 100 MHz carrier.

</details>

### A.3 波束管理 —— Rel-19 的故事

mmWave（FR2）和新兴的 FR3 系统使用**数百**个窄波束。经典的波束捕获会对 SSB 码本做穷举式扫描 —— 又慢又耗能。ML 方法从以下来源预测最佳波束：

* 对少数几个波束做**部分扫描**（空域波束预测），或
* **历史波束**（时域预测），或
* **旁路信道**：摄像头、GPS、环境地图（传感器辅助 / "Synesthesia of Machines"）。

3GPP Rel-19 有一个针对 **AI/ML 波束管理**的规范性工作项，包含两个子用例（空域和时域）—— 这是学习模型首次在标准中获得显式信令支持。

一个参考架构是在扫描波束的 RSRP 网格上运行的小型卷积神经网络：

```
Input  : RSRP grid [num_swept_beams_az × num_swept_beams_el]
Hidden : 2 conv layers + 1 FC
Output : softmax over the full codebook (e.g., 64 or 256 beams)
```

硬件层面的问题是**摄像头/传感器特征在哪里与 RF 特征融合** —— 通常是在 AP 的 NPU（Snapdragon Auto、Jetson）上，而不是调制解调器本身。

### A.4 端到端自编码器 PHY（仅限研究 —— 但很重要）

在这一范式中，Tx（编码器）和 Rx（解码器）是**联合训练的神经网络**。"信道"是一个**可微层**（AWGN 噪声、多径冲激响应，可能还有硬件损伤）。星座、脉冲成形和接收机全部联合学习。

```
bits ──► [NN encoder] ──► waveform ──► [channel] ──► [NN decoder] ──► bits
              ▲                                            ▲
              └───── jointly trained via BCE / BLER loss ───┘
```

结果是一种**非标准空口**。星座看起来很怪（不是 QAM），导频消失，而且不再有清晰的 PHY/MAC 划分。这与 3GPP 信令不兼容 —— 因此它存在于**非蜂窝**的细分领域：水声、自由空间光、LEO ISL、军用波形。

它也是无线电中**软硬件协同设计**最纯粹的展示：编码器/解码器的拓扑在硅设计阶段就已固定，权重随固件发布，而更新则是模型重训练。

### A.5 硬件现实检验 —— 实际出货的是什么

| 厂商 / 平台 | PHY 可用的 AI 算力 | 用途 |
|---|---|---|
| **Qualcomm Snapdragon X75/X80 modem** | 调制解调器内部的 Hexagon Tensor + 第二代 "AI Processor" | 信道估计、解映射、链路自适应、PA DPD |
| **Samsung Exynos Modem 5400** | 经紧耦合 L2 与 AP 共享的 NPU | 信道估计、预测性调度提示 |
| **MediaTek M90 (Dimensity)** | APU + DSP 簇 | 波束管理、链路自适应 |
| **Nvidia Aerial cuPHY** | 每 DU 配备 A100 / L40S / H100 GPU | 完整内联 GPU PHY：FFT、信道估计、MIMO、解映射、LDPC，全部为 CUDA kernel |
| **Intel FlexRAN** | Xeon AVX-512/AMX + 可选 vRAN FPGA（ACC100/200） | 软件 PHY，FEC 由 FPGA 卸载 |
| **Marvell OCTEON 10 Fusion** | DPU + ML 加速器 | 面向 O-RAN 的内联 DU PHY |
| **AMD T2 (Pensando)** | 自适应 SoC + ML 引擎 | 厂商专用的 O-RAN DU |

> **关键洞察：** 内联 PHY ML 最难的地方恰恰是你意料之中的 —— **UE 调制解调器**，只有几十毫瓦的余量和微秒级的预算。相比之下，地下室（DU/gNB）的算力实际上不受限制；那里的工程问题在于**调度延迟和确定性**，而不是吞吐。

---

## Part B —— RAN 级 ML 与 O-RAN RIC

3GPP PHY 在**微秒**级处理一个时隙。MAC 调度器每 1 ms 运行一次。RRM（资源管理）决策发生在几十毫秒的尺度上。在这之上 —— 负载均衡、节能、移动性、异常检测 —— 存在一个**秒到分钟**级的循环，它是"基础设施 ML"的天然归属。

O-RAN 用 **RIC（RAN Intelligent Controller）**将其规范化：

```
   ┌──────────────────────────────────────────────────────────┐
   │              Non-RT RIC  (in SMO, > 1 s)                  │
   │   rApps: training pipelines, policy generation, A1 mgmt   │
   │                  GPU/CPU servers, off-RAN                 │
   └──────────────────────────┬───────────────────────────────┘
                              │ A1 (policy/intent)
                              ▼
   ┌──────────────────────────────────────────────────────────┐
   │             Near-RT RIC  (10 ms – 1 s)                    │
   │   xApps: traffic steering, QoS, anomaly det., handover    │
   │             CPU + GPU/FPGA accelerator                    │
   └──────────────────────────┬───────────────────────────────┘
                              │ E2 (per-cell telemetry + actions)
                              ▼
   ┌──────────────────────────────────────────────────────────┐
   │   O-CU  ◄────►  O-DU  ◄────►  O-RU                        │
   │                  ▲                                        │
   │              (μs–ms PHY/MAC, inline ML here)              │
   └──────────────────────────────────────────────────────────┘
```


<details>
<summary>English original</summary>

**A.3 Beam management — the Rel-19 story**

mmWave (FR2) and emerging FR3 systems use **hundreds** of narrow beams. Classical beam acquisition does an exhaustive sweep of an SSB codebook — slow and energy-hungry. ML approaches predict the best beam from:

* a **partial sweep** of a few beams (spatial-domain beam prediction), or
* **historical beams** (temporal-domain prediction), or
* **side channels**: camera, GPS, environment map (sensor-aided / "Synesthesia of Machines").

3GPP Rel-19 has a normative work item for **AI/ML beam management** with two sub-use-cases (spatial and temporal) — the first time a learned model gets explicit signaling support in the standard.

A reference architecture is a small CNN over the RSRP grid of swept beams:

```
Input  : RSRP grid [num_swept_beams_az × num_swept_beams_el]
Hidden : 2 conv layers + 1 FC
Output : softmax over the full codebook (e.g., 64 or 256 beams)
```

The hardware question is **where the camera/sensor features fuse with the RF features** — typically the AP's NPU (Snapdragon Auto, Jetson) rather than the modem itself.

**A.4 End-to-end autoencoder PHY (research only — but important)**

In this paradigm, the Tx (encoder) and Rx (decoder) are **co-trained neural networks**. The "channel" is a **differentiable layer** (AWGN noise, multipath impulse response, possibly hardware impairments). The constellation, pulse shape, and receiver are all learned jointly.

```
bits ──► [NN encoder] ──► waveform ──► [channel] ──► [NN decoder] ──► bits
              ▲                                            ▲
              └───── jointly trained via BCE / BLER loss ───┘
```

The result is a **non-standard air interface**. Constellations look weird (not QAM), pilots disappear, and there is no longer a clean PHY/MAC split. This is incompatible with 3GPP signaling — so it lives in **non-cellular** niches: underwater acoustic, free-space optical, LEO ISL, military waveforms.

It is also the cleanest demonstration of **hardware-software co-design** in radio: the encoder/decoder topology is fixed at silicon-design time, weights ship in firmware, and updates are model retrains.

**A.5 Hardware reality check — what actually ships**

| Vendor / Platform | AI compute available to the PHY | Used for |
|---|---|---|
| **Qualcomm Snapdragon X75/X80 modem** | Hexagon Tensor + 2nd-gen "AI Processor" inside the modem | Channel est., demapping, link adaptation, PA DPD |
| **Samsung Exynos Modem 5400** | NPU shared with AP via tightly coupled L2 | Channel est., predictive scheduling hints |
| **MediaTek M90 (Dimensity)** | APU + DSP cluster | Beam mgmt, link adaptation |
| **Nvidia Aerial cuPHY** | A100 / L40S / H100 GPU per DU | Full inline GPU PHY: FFT, ch.est., MIMO, demap, LDPC, all CUDA kernels |
| **Intel FlexRAN** | Xeon AVX-512/AMX + optional vRAN FPGA (ACC100/200) | Software PHY with FPGA offload for FEC |
| **Marvell OCTEON 10 Fusion** | DPU + ML accelerator | Inline DU PHY for O-RAN |
| **AMD T2 (Pensando)** | Adaptive SoC + ML engines | Vendor-specific O-RAN DU |

> **Key Insight:** Inline PHY ML is hardest where you'd expect — the **UE modem**, with tens of milliwatts to spare and microsecond budgets. The basement (DU/gNB) has effectively unlimited compute by comparison; the engineering problem there is **scheduling latency and determinism**, not throughput.

---

**Part B — RAN-Level ML and the O-RAN RIC**

The 3GPP PHY processes a slot in **microseconds**. The MAC scheduler runs every 1 ms. RRM (resource management) decisions happen on tens of milliseconds. Above that — load balancing, energy savings, mobility, anomaly detection — there is a **seconds-to-minutes** loop that is the natural home of "infrastructure ML".

O-RAN formalized this with the **RIC (RAN Intelligent Controller)**:

```
   ┌──────────────────────────────────────────────────────────┐
   │              Non-RT RIC  (in SMO, > 1 s)                  │
   │   rApps: training pipelines, policy generation, A1 mgmt   │
   │                  GPU/CPU servers, off-RAN                 │
   └──────────────────────────┬───────────────────────────────┘
                              │ A1 (policy/intent)
                              ▼
   ┌──────────────────────────────────────────────────────────┐
   │             Near-RT RIC  (10 ms – 1 s)                    │
   │   xApps: traffic steering, QoS, anomaly det., handover    │
   │             CPU + GPU/FPGA accelerator                    │
   └──────────────────────────┬───────────────────────────────┘
                              │ E2 (per-cell telemetry + actions)
                              ▼
   ┌──────────────────────────────────────────────────────────┐
   │   O-CU  ◄────►  O-DU  ◄────►  O-RU                        │
   │                  ▲                                        │
   │              (μs–ms PHY/MAC, inline ML here)              │
   └──────────────────────────────────────────────────────────┘
```

</details>

### B.1 哪些东西作为 xApp 运行

* **流量引导** —— 在给定负载和 QoS 等级的情况下，选择 UE 应驻留在哪个小区 / 载波上。
* **MIMO 模式选择** —— SU-MIMO 还是 MU-MIMO、秩自适应、码本选择。
* **节能** —— 在低负载下关闭小区 / 载波 / 天线端口（3GPP TR 38.864）。
* **异常 / 入侵检测** —— 标记伪基站、干扰器、IMSI catcher。
* **QoS 预测** —— 预测某条路线上的吞吐（例如用于 V2X）。

这些大多是 **contextual bandit** 或**轻量级 DRL**。决策间隔很宽松，但动作空间可能很大（想想：“对 1000 个 UE 中的每一个，每 100 ms 从 8 个小区中挑一个”），因此模型规模受限于推理延迟 × 小区数，而不是准确率。

### B.2 哪些东西作为 rApp 运行

Non-RT RIC 负责**训练**。一个 rApp 通常：

1. 从 SMO 数据湖拉取聚合 KPI 和 per-UE 的 trace。
2. 训练模型（通常离线，通常在 colo 的 GPU 上）。
3. 通过 A1 把模型产物 + 一份**策略**推送给一个或多个 xApp。

在这里，ML pipeline 看起来最像传统的 MLOps —— Kubeflow、Airflow、MLflow、模型注册表。有意思的麻烦在于，**部署目标**（near-RT RIC 上的一个 xApp）带有硬性延迟约束，训练任务必须遵守这些约束。

### B.3 用于节能的 ML —— 挑大梁的用例

运营商关注无线 ML，主要是因为它能**降低 opex**。小区载波消耗了 **RAN 能耗的 40–60%**。基于预测流量和移动性的 ML 驱动小区关断决策已规模化部署（Vodafone、DT、NTT、KDDI 都有正在运行的生产 rApp）。

模型通常很不起眼 —— 梯度提升树，或者跑在单小区流量时间序列上的小 LSTM。真正不平凡的是**系统**层面的问题：
- 把小区重新唤醒的延迟是 100–500 ms（RF PA 预热、同步）。
- 一次糟糕的预测 = 会话掉线或覆盖空洞。
- 策略必须对运维人员**可解释**，并且**有界**（例如“在这个 tracking area 内关断的小区不得超过 N 个”）。

这是一个绝佳案例：AI 硬件工程师的活儿**不是**去加 TOPS —— 而是把可解释性 + 安全 harness 装到一套本已可用的部署上。

---

## Part C — SDR + 深度学习

**软件定义无线电（SDR）**是合适的教学平台：它能让你把一个真实的 PyTorch 模型放到真实的 **IQ 采样**上。最小可用的实验台：

| 硬件 | 采样率 | 用途 |
|---|---|---|
| **RTL-SDR ($30)** | ≤ 2.4 MS/s | 调制识别、FM/AIS 解调 |
| **HackRF One** | 20 MS/s | 调制识别、宽带扫描 |
| **Adalm-Pluto** | 60 MS/s, TX+RX | 环回实验、端到端 NN PHY |
| **USRP B210 / B205mini** | 56 MS/s, 2×2 MIMO | 正经实验 |
| **USRP X310 / N310** | 200 MS/s | 生产级 |
| **Jetson Orin + USRP** | 视情况而定 | 在 IQ 上做边缘 ML |

### C.1 调制识别 —— RF ML 的 “hello world”

给一段 IQ 采样，判断其调制方式（BPSK / QPSK / 16QAM / 64QAM / GFSK / OFDM / AM / FM / …）。DeepSig 的 **RML2018.01a** 数据集是标准 benchmark。

一个参考 CNN（O'Shea / DeepSig 的 “ResNet 风格”）：

```python
class ModClassNet(nn.Module):
    def __init__(self, n_classes=24):
        super().__init__()
        # Input: [B, 2, 1024]  (I/Q over 1024 samples)
        self.features = nn.Sequential(
            nn.Conv1d(2, 64, 7, padding=3), nn.ReLU(), nn.MaxPool1d(2),
            nn.Conv1d(64, 64, 5, padding=2), nn.ReLU(), nn.MaxPool1d(2),
            nn.Conv1d(64, 64, 3, padding=1), nn.ReLU(), nn.AdaptiveAvgPool1d(1),
        )
        self.head = nn.Linear(64, n_classes)
    def forward(self, x):
        return self.head(self.features(x).squeeze(-1))
```

在 RML2018.01a 上，这类模型在 SNR 高于 10 dB 时 top-1 达到约 80%（相比之下，基于手工累积量的分类器约为 60%）。


<details>
<summary>English original</summary>

**B.1 What runs as an xApp**

* **Traffic steering** — pick which cell / carrier a UE should be on, given load and QoS class.
* **MIMO mode selection** — SU- vs MU-MIMO, rank adaptation, codebook selection.
* **Energy savings** — switch off cells / carriers / antenna ports under low load (3GPP TR 38.864).
* **Anomaly / intrusion detection** — flag rogue base stations, jammers, IMSI catchers.
* **QoS prediction** — predict throughput on a route (e.g., for V2X).

These are mostly **contextual bandits** or **lightweight DRL**. The decision interval is comfortable, but the action space can be large (think: "for each of 1000 UEs, pick one of 8 cells, every 100 ms"), so model size is gated by inference latency × cell count, not by accuracy.

**B.2 What runs as an rApp**

The Non-RT RIC handles **training**. An rApp typically:

1. Pulls aggregated KPIs and per-UE traces from the SMO data lake.
2. Trains a model (often offline, often on GPUs in a colo).
3. Pushes the model artifact + a **policy** to one or more xApps via A1.

This is where ML pipelines look most like conventional MLOps — Kubeflow, Airflow, MLflow, model registry. The interesting wrinkle is that the **deployment target** (an xApp on a near-RT RIC) has hard latency constraints that the training job must respect.

**B.3 ML for energy savings — the load-bearing use case**

Operators care about wireless ML mostly because it **reduces opex**. Cell carriers consume **40–60% of RAN energy**. ML-driven cell shutdown decisions, based on predicted traffic and mobility, are deployed at scale (Vodafone, DT, NTT, KDDI all have running production rApps).

The model is usually unimpressive — gradient-boosted trees or a small LSTM on per-cell traffic timeseries. The **systems** problem is non-trivial:
- Latency for waking a cell back up is 100–500 ms (RF PA warmup, sync).
- A bad prediction = dropped session or coverage hole.
- The policy must be **explainable** to operations staff and **bounded** (e.g., "never shut off more than N cells in this tracking area").

This is a perfect case where the AI hardware engineer's job is **not** to add TOPS — it is to fit the explainability + safety harness onto a deployment that already works.

---

**Part C — SDR + Deep Learning**

**Software-defined radio (SDR)** is the right teaching platform: it lets you put a real PyTorch model on real **IQ samples**. The minimum viable bench:

| Hardware | Sample rate | Use |
|---|---|---|
| **RTL-SDR ($30)** | ≤ 2.4 MS/s | Modulation classification, FM/AIS demod |
| **HackRF One** | 20 MS/s | Modulation classification, wideband scan |
| **Adalm-Pluto** | 60 MS/s, TX+RX | Loopback experiments, end-to-end NN PHY |
| **USRP B210 / B205mini** | 56 MS/s, 2×2 MIMO | Serious experiments |
| **USRP X310 / N310** | 200 MS/s | Production-grade |
| **Jetson Orin + USRP** | depends | Edge ML on IQ |

**C.1 Modulation classification — the "hello world" of RF ML**

Given a snippet of IQ samples, classify the modulation (BPSK / QPSK / 16QAM / 64QAM / GFSK / OFDM / AM / FM / …). DeepSig's **RML2018.01a** dataset is the standard benchmark.

A reference CNN (O'Shea / DeepSig "ResNet-style"):

```python
class ModClassNet(nn.Module):
    def __init__(self, n_classes=24):
        super().__init__()
        # Input: [B, 2, 1024]  (I/Q over 1024 samples)
        self.features = nn.Sequential(
            nn.Conv1d(2, 64, 7, padding=3), nn.ReLU(), nn.MaxPool1d(2),
            nn.Conv1d(64, 64, 5, padding=2), nn.ReLU(), nn.MaxPool1d(2),
            nn.Conv1d(64, 64, 3, padding=1), nn.ReLU(), nn.AdaptiveAvgPool1d(1),
        )
        self.head = nn.Linear(64, n_classes)
    def forward(self, x):
        return self.head(self.features(x).squeeze(-1))
```

On RML2018.01a, this kind of model hits ~80% top-1 above 10 dB SNR (vs ~60% for handcrafted cumulant-based classifiers).

</details>

### C.2 GNU Radio + PyTorch 回环

SDR 工作实际上就是靠 GNU Radio flowgraph 完成的。PyTorch 接口是一个自定义 Python block：

```python
# torch_classifier_block.py — GNU Radio sink that runs a PyTorch model
import numpy as np
import torch
from gnuradio import gr

class TorchClassifier(gr.sync_block):
    def __init__(self, model_path, window=1024):
        gr.sync_block.__init__(
            self,
            name="torch_classifier",
            in_sig=[np.complex64],
            out_sig=None,
        )
        self.window = window
        self.model = torch.jit.load(model_path).eval().cuda()
        self.buf = np.zeros(window, dtype=np.complex64)

    def work(self, input_items, output_items):
        x = input_items[0][: self.window]
        if len(x) < self.window:
            return len(x)
        iq = np.stack([x.real, x.imag], axis=0)             # [2, W]
        t = torch.from_numpy(iq).float().unsqueeze(0).cuda()
        with torch.no_grad():
            logits = self.model(t)
        # publish via stream tags / ZMQ / gr.tag_t — flow-graph specific
        return len(x)
```

实际会踩到的坑：

* **采样率不匹配** — 要在部署时所用的*同一*采样率（或*同一*重采样方式）下训练，否则 IQ 统计特性会发生变化。
* **直流偏置 / IQ 不平衡** — 每台 SDR 都有；要么校准，要么把它们纳入训练增广。
* **频率偏移** — 训练时随机化；假设载波完全同步的分类器，在其训练所用的 SNR 下就会崩掉。
* **帧边界** — 对具有包结构的协议（Wi-Fi、BLE），要么让输入窗口与前导码对齐，要么把对齐视为随机并据此训练。

### C.3 RF 指纹识别 — 与安全相邻的用例

两台运行*同一*标准的发射机，由于**各自独有的 RF 损伤**（PA 非线性、振荡器相位噪声、混频器泄漏），会产生细微不同的 IQ 尾部。在**原始 IQ** 上训练的模型能够区分单个设备 — 这对认证（“这真的是我的设备吗？”）和对抗场景（“这台设备真的是它宣称的那台吗？”）都有用。

硬件层面的说法：这在紧邻 SDR 的 **Jetson Orin Nano** 上跑得很好，因为分类器很小，**瓶颈在 RF 而不在算力**。这是一个干净的路线图项目，能完整演练整个嵌入式 AI 栈。

### C.4 在 SDR 中 ML 有用之处与无用之处

| 任务 | ML 是否有用？ | 原因 |
|---|---|---|
| 调制识别（低 SNR） | **是** | 胜过手工设计的特征 |
| 载波 / 时序同步 | 很少 | 经典算法成熟，难以超越 |
| FFT / 信道化 | **否** | DSP 在功耗与可预测性上胜出 |
| 频谱感知（认知无线电） | **是** | 能捕获非高斯干扰 |
| 测向（DoA） | 有时 | 低快拍场景下 NN-MUSIC 有竞争力 |
| 解码已知协议（Wi-Fi、BLE） | **否** | 符合标准的解码器已是最优且免费 |
| 解码**未知**协议 | **是** | 这正是“RF 逆向工程”的意义所在 |

---

## 推理实际发生在哪里 — 一张延迟地图

```
   Budget       Block                            Hardware
   ───────────────────────────────────────────────────────────────
   ~ns          Per-sample IQ processing         RFIC / dedicated DSP
                (filters, mixers, FFT)
   ~µs          Per-OFDM-symbol                  FPGA / hard accel
                (timing sync, FFT, eq.)
   ~10 µs       Per-RE demap, LLR                NPU / GPU tensor core
   ~100 µs      Per-slot channel est.,           NPU / GPU
                MIMO detect, decode
   ~1 ms        Per-TTI MAC scheduling           CPU + small NPU hints
   ~10 ms       Link adaptation, BSR/PHR proc.   CPU
   ~100 ms      Mobility prediction, beam mgmt   xApp on near-RT RIC
   ~1 s         Cell on/off, slice allocation    xApp / rApp
   ~min         Capacity planning, fault triage  rApp / SMO ML platform
   ~hour+       Network design, model retraining offline GPU clusters
```

这是硬件工程师的地图。“ML 在这套无线电上该放在哪里？”这个问题归结为：**我处在表格中的哪一行？** 每一行对应的加速器、编程模型、部署节奏都不同，模型出错时的风险画像也不同。

---


<details>
<summary>English original</summary>

**C.2 GNU Radio + PyTorch loopback**

GNU Radio flowgraphs are how SDR work actually happens. The PyTorch interface is a custom Python block:

```python
# torch_classifier_block.py — GNU Radio sink that runs a PyTorch model
import numpy as np
import torch
from gnuradio import gr

class TorchClassifier(gr.sync_block):
    def __init__(self, model_path, window=1024):
        gr.sync_block.__init__(
            self,
            name="torch_classifier",
            in_sig=[np.complex64],
            out_sig=None,
        )
        self.window = window
        self.model = torch.jit.load(model_path).eval().cuda()
        self.buf = np.zeros(window, dtype=np.complex64)

    def work(self, input_items, output_items):
        x = input_items[0][: self.window]
        if len(x) < self.window:
            return len(x)
        iq = np.stack([x.real, x.imag], axis=0)             # [2, W]
        t = torch.from_numpy(iq).float().unsqueeze(0).cuda()
        with torch.no_grad():
            logits = self.model(t)
        # publish via stream tags / ZMQ / gr.tag_t — flow-graph specific
        return len(x)
```

The realistic pitfalls:

* **Sample-rate mismatch** — train at the *same* sample rate (or with the *same* resampling) you'll deploy at, otherwise the IQ statistics change.
* **DC offset / IQ imbalance** — every SDR has them; either calibrate or include them in training augmentations.
* **Frequency offset** — randomize during training; classifiers that assume perfectly synced carriers fall apart at SNRs they were trained on.
* **Frame boundary** — for protocols with packet structure (Wi-Fi, BLE), align the input window with the preamble or accept the alignment as random and train accordingly.

**C.3 RF fingerprinting — the security-adjacent use case**

Two transmitters running the *same* standard produce subtly different IQ tails because of **unique RF impairments** (PA nonlinearity, oscillator phase noise, mixer leakage). Models trained on **raw IQ** can distinguish individual devices — useful for authentication ("is this really my device?") and adversarial ("is this device the one that was advertised?").

The hardware story: this works well on **Jetson Orin Nano** sitting next to an SDR, because the classifier is small and the **bottleneck is RF, not compute**. It is a clean roadmap project that exercises the entire embedded-AI stack.

**C.4 Where ML helps vs where it doesn't (in SDR)**

| Task | ML helps? | Why |
|---|---|---|
| Modulation classification (low SNR) | **Yes** | Beats hand-engineered features |
| Carrier / timing sync | Rarely | Mature classical algorithms, hard to beat |
| FFT / channelization | **No** | DSP wins on power and predictability |
| Spectrum sensing (cognitive radio) | **Yes** | Captures non-Gaussian interference |
| Direction finding (DoA) | Sometimes | NN-MUSIC competitive in low snapshot regimes |
| Decoding known protocols (Wi-Fi, BLE) | **No** | Standards-compliant decoders are optimal and free |
| Decoding **unknown** protocols | **Yes** | The whole point of "RF reverse engineering" |

---

**Where Inference Actually Lives — A Latency Map**

```
   Budget       Block                            Hardware
   ───────────────────────────────────────────────────────────────
   ~ns          Per-sample IQ processing         RFIC / dedicated DSP
                (filters, mixers, FFT)
   ~µs          Per-OFDM-symbol                  FPGA / hard accel
                (timing sync, FFT, eq.)
   ~10 µs       Per-RE demap, LLR                NPU / GPU tensor core
   ~100 µs      Per-slot channel est.,           NPU / GPU
                MIMO detect, decode
   ~1 ms        Per-TTI MAC scheduling           CPU + small NPU hints
   ~10 ms       Link adaptation, BSR/PHR proc.   CPU
   ~100 ms      Mobility prediction, beam mgmt   xApp on near-RT RIC
   ~1 s         Cell on/off, slice allocation    xApp / rApp
   ~min         Capacity planning, fault triage  rApp / SMO ML platform
   ~hour+       Network design, model retraining offline GPU clusters
```

This is the hardware engineer's map. The "where does ML belong on this radio?" question reduces to: **which row of this table am I in?** Each row dictates a different accelerator, a different programming model, a different deployment cadence, and a different risk profile when the model is wrong.

---

</details>

## 动手练习

1. **合成 3GPP 数据上的神经信道估计器。** 生成一个 TDL-C 信道实现（Python：`py3gpp` 或 `sionna`）。计算 LS 导频估计，训练 §A.1 中的 `ChannelEstNet`，用于去噪/插值到整个网格。在 −5 到 25 dB 的 SNR 范围内比较 MSE 与 LMMSE。然后量化到 INT8（PTQ）并重新测量——预期退化 <0.5 dB；若退化更多，说明你的激活值范围校准有误。

2. **神经解映射器直接替换。** 在 Sionna（NVIDIA 的 GPU 加速链路级仿真器）中搭建 64-QAM 发射机与 OFDM 接收机。把它的解析解映射器替换为 §A.2 中的 `NeuralDemapper`。在有和没有神经模块两种情况下测量 BLER 对 SNR。在主机 GPU 和 Jetson Orin 上剖析推理延迟（TensorRT FP16 与 INT8）。

3. **真实 IQ 上的调制分类器。** 用 RTL-SDR 或 HackRF 在繁忙的 ISM 频段采集 30 分钟 IQ。切分为 1024 采样点的窗口。在 **合成** GNU Radio 数据上训练 §C.1 中的 `ModClassNet`（这样标签才是正确的）。在采集的真实数据上跑推理；把预测结果与你在瀑布图上手工辨识的结果对比。记录模型过度自信之处。

4. **GNU Radio + PyTorch 流水线。** 搭建一个流图：`osmocom_source` → `low_pass_filter` → `torch_classifier_block` → `file_sink`。用经 TensorRT 转换的权重在 Jetson Orin Nano 上运行。测量从 RF 抽头到分类发布的端到端延迟，并找出主要开销（IQ DMA、host↔device 拷贝、推理、后处理）。

5. **硬件规模估算练习。** 选定一个目标空口（Wi-Fi 7、5G FR1 100 MHz、5G FR2 400 MHz、LEO Ka 频段）。选择**一个** PHY 模块进行神经化。推导：
   - 该模块输入与输出处的 MACs/sec 和 bytes/sec，
   - 对应的 SRAM 工作集，
   - 以合理的 MAC/mm² 数值估算的 5 nm 硅面积，
   - 该模块是能放进 modem 现有 NPU，还是需要专用分块。

   与已部署 modem 的公开信息对比（例如 Snapdragon X80 参考文档）。指出你的估算在哪里有偏差以及原因。

6. **挑一个 O-RAN xApp 并阅读 E2 规范。** 略读 O-RAN.WG3.E2SM-KPM，挑一个目标用例（例如流量导引）。画出数据流：逐 UE 的 KPI 上报 → xApp 模型推理 → A1 / RIC 控制消息。记下**动作延迟**预算（通常 10–100 ms），并判断该预算允许你部署多大的模型。

---

## 关键要点

| 要点 | 对 AI 硬件的意义 |
|---|---|
| 信道估计是部署最成熟的 PHY-ML 模块 | 若在设计 modem NPU，工作负载画像即从这里开始 |
| 延迟阶梯决定加速器选型 | µs 级任务给 FPGA/NPU，ms 级给 CPU+NPU，秒级以上给 GPU 服务器 |
| 内联 PHY ML 是带宽受限，而非算力受限 | 布局布线、分块和 SRAM 设计占主导；裸 TOPS 只是虚荣指标 |
| O-RAN 的 RIC 是无线中“慢 ML”的标准归属地 | 标准化的遥测（E2/A1）意味着 MLOps 工具可平滑迁移 |
| 端到端自编码器 PHY 已存在，但未在 3GPP 中落地 | 适用于非蜂窝小众场景；尚未驱动蜂窝芯片路线图 |
| 波束管理是下一个标准化的 ML 模块（3GPP Rel-19） | 蜂窝标准中首个规范性 ML——芯片厂商将会跟进 |
| SDR + Jetson 是合适的教学平台 | 用现成器件即可搭出完整的神经接收机演示 |

---


<details>
<summary>English original</summary>

**Hands-On Exercises**

1. **Neural channel estimator on synthetic 3GPP data.** Generate a TDL-C channel realization (Python: `py3gpp` or `sionna`). Compute LS pilot estimates, train the `ChannelEstNet` from §A.1 to denoise/interpolate to the full grid. Compare MSE vs LMMSE at SNRs from −5 to 25 dB. Then quantize to INT8 (PTQ) and re-measure — expect <0.5 dB degradation; if you see more, your activation range calibration is wrong.

2. **Neural demapper drop-in.** Build a 64-QAM transmitter and an OFDM receiver in Sionna (NVIDIA's GPU-accelerated link-level simulator). Replace its analytic demapper with the `NeuralDemapper` from §A.2. Measure BLER vs SNR with and without the neural block. Profile inference latency on the host GPU and on a Jetson Orin (TensorRT FP16 and INT8).

3. **Modulation classifier on real IQ.** Capture 30 minutes of IQ in a busy ISM band with an RTL-SDR or HackRF. Slice into 1024-sample windows. Train the `ModClassNet` from §C.1 on **synthetic** GNU Radio data (so labels are correct). Run inference on the captured real data; compare predictions to what you can identify by hand on a waterfall. Document where the model is over-confident.

4. **GNU Radio + PyTorch pipeline.** Build a flowgraph: `osmocom_source` → `low_pass_filter` → `torch_classifier_block` → `file_sink`. Run on Jetson Orin Nano with TensorRT-converted weights. Measure end-to-end latency from RF tap to classification publish, and identify the dominant cost (IQ DMA, host↔device copy, inference, post-processing).

5. **Hardware sizing exercise.** Pick a target air interface (Wi-Fi 7, 5G FR1 100 MHz, 5G FR2 400 MHz, LEO Ka-band). Choose **one** PHY block to neuralize. Derive:
   - MACs/sec and bytes/sec at the block's input and output,
   - the implied SRAM working-set,
   - the silicon area at 5 nm with a reasonable MAC/mm² figure,
   - whether the block fits in the modem's existing NPU or needs a dedicated tile.

   Compare to public information about a deployed modem (e.g., Snapdragon X80 reference docs). Identify where your estimate is off and why.

6. **Pick an O-RAN xApp and read the E2 spec.** Skim O-RAN.WG3.E2SM-KPM and pick a target use case (e.g., traffic steering). Sketch the data flow from per-UE KPI report → xApp model inference → A1 / RIC control message. Note the **action latency** budget (typically 10–100 ms) and identify what model size that lets you ship.

---

**Key Takeaways**

| Takeaway | Why it matters for AI hardware |
|---|---|
| Channel estimation is the most mature deployed PHY-ML block | If you're designing a modem NPU, the workload profile starts here |
| The latency ladder dictates the accelerator | µs jobs go to FPGA/NPU, ms jobs to CPU+NPU, seconds+ go to GPU servers |
| Inline PHY ML is bandwidth-bound, not compute-bound | Layout, tiling, and SRAM design dominate; raw TOPS is a vanity metric |
| O-RAN's RIC is the canonical home for "slow ML" in wireless | Standardized telemetry (E2/A1) means MLOps tools transfer cleanly |
| End-to-end autoencoder PHY exists but doesn't ship in 3GPP | Useful for non-cellular niches; doesn't drive cellular silicon roadmaps yet |
| Beam management is the next standardized ML block (3GPP Rel-19) | First normative ML in cellular standards — silicon vendors will react |
| SDR + a Jetson is the right teaching bench | You can build a complete neural-receiver demo with off-the-shelf parts |

---

</details>

## Resources

* **[NVIDIA Sionna](https://nvlabs.github.io/sionna/):** GPU 加速的链路级仿真器，原生集成 PyTorch / JAX。PHY-ML 研究的参考平台。
* **[NVIDIA Aerial / cuPHY](https://developer.nvidia.com/aerial):** 面向 5G/6G gNB DU 的生产级 GPU-resident PHY。
* **[Intel FlexRAN Reference Architecture](https://networkbuilders.intel.com/solutionslibrary/flexran):** 软件 RAN 的 CPU+FPGA 参考方案 —— 可与 Aerial 对照。
* **[O-RAN Alliance Specifications](https://www.o-ran.org/specifications):** 了解 RIC、E2、A1 与 ML 用例的权威出处。从 WG2（Non-RT RIC）和 WG3（Near-RT RIC）入手。
* **[3GPP TR 38.843 — AI/ML for the air interface](https://www.3gpp.org/dynareport/38843.htm):** Rel-19 规范工作背后的研究报告；涵盖 CSI feedback、beam management、positioning。
* **[DeepSig RadioML datasets](https://www.deepsig.ai/datasets):** RML2016 / RML2018 —— 调制分类的标准 benchmark。
* **["An Introduction to Deep Learning for the Physical Layer," O'Shea & Hoydis (2017)](https://arxiv.org/abs/1702.00832):** 自编码器-PHY 的开创性论文。读一遍。
* **["Machine Learning at the Wireless Edge," Park et al.](https://ieeexplore.ieee.org/document/9145080):** 综述，覆盖无线场景下的 edge-ML 硬件约束。
* **[Qualcomm Wireless AI Research](https://www.qualcomm.com/research/artificial-intelligence/wireless-ai):** 来自 Hexagon-resident PHY ML 背后团队的论文与白皮书。
* **[GNU Radio](https://www.gnuradio.org/) + [gr-torch](https://github.com/gnuradio):** SDR 框架 + 社区 PyTorch 集成。
* **[Adalm-Pluto Getting Started](https://wiki.analog.com/university/tools/pluto):** 最便宜的具备 TX 能力的 SDR —— 适合端到端 NN PHY 环回实验。


<details>
<summary>English original</summary>

**Resources**

* **[NVIDIA Sionna](https://nvlabs.github.io/sionna/):** GPU-accelerated link-level simulator with first-class PyTorch / JAX integration. The reference platform for PHY-ML research.
* **[NVIDIA Aerial / cuPHY](https://developer.nvidia.com/aerial):** Production GPU-resident PHY for 5G/6G gNB DU.
* **[Intel FlexRAN Reference Architecture](https://networkbuilders.intel.com/solutionslibrary/flexran):** CPU+FPGA reference for software RAN — useful counterpoint to Aerial.
* **[O-RAN Alliance Specifications](https://www.o-ran.org/specifications):** The canonical place to read about RIC, E2, A1, and ML use cases. Start with WG2 (Non-RT RIC) and WG3 (Near-RT RIC).
* **[3GPP TR 38.843 — AI/ML for the air interface](https://www.3gpp.org/dynareport/38843.htm):** The study report behind the Rel-19 normative work; covers CSI feedback, beam management, positioning.
* **[DeepSig RadioML datasets](https://www.deepsig.ai/datasets):** RML2016 / RML2018 — the standard benchmarks for modulation classification.
* **["An Introduction to Deep Learning for the Physical Layer," O'Shea & Hoydis (2017)](https://arxiv.org/abs/1702.00832):** The foundational autoencoder-PHY paper. Read this once.
* **["Machine Learning at the Wireless Edge," Park et al.](https://ieeexplore.ieee.org/document/9145080):** Survey covering edge-ML hardware constraints in wireless.
* **[Qualcomm Wireless AI Research](https://www.qualcomm.com/research/artificial-intelligence/wireless-ai):** Publications and white papers from the team behind Hexagon-resident PHY ML.
* **[GNU Radio](https://www.gnuradio.org/) + [gr-torch](https://github.com/gnuradio):** SDR framework + community PyTorch integrations.
* **[Adalm-Pluto Getting Started](https://wiki.analog.com/university/tools/pluto):** Cheapest TX-capable SDR — good for end-to-end NN PHY loopback labs.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/AI-Driven Wireless Communication/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/AI-Driven%20Wireless%20Communication/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
