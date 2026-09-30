---
title: 应用开发
description: 应用开发
---

# 应用开发

<div class="course-identity jetson-apps" markdown="1">
<div class="course-identity__icon">APP</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 B5 · Jetson 应用开发</p>
<p class="course-identity__title">构建横跨外设、网络、UI、多媒体、ML 推理与 ROS 2 的应用。</p>
<p class="course-identity__meta">产物：集成的 Jetson 应用 · 度量：延迟、资源、UX、可靠性</p>
</div>
</div>


**阶段 4 — 方向 B — Nvidia Jetson** · 模块 5 / 共 7

> **重点：** 在 **Jetson Orin Nano 8GB** 上构建量产应用软件 —— 从底层外设访问（GPIO、UART、SPI、I2C、CAN）到网络、GUI、多媒体流水线、**ML/AI 推理**（TensorRT、DLA（深度学习加速器）、tinygrad）以及 **ROS 2** 集成。
>
> **主要硬件：** 定制载板或 dev-kit 载板上的 Jetson Orin Nano 8GB

**上一节：** [4. FSP 定制](../04-FSP固件支持包定制/Guide.md) · **下一节：** [6. 安全与 OTA](../06-安全与OTA/Guide.md)

---

## 子模块

| # | 子模块 | 重点 |
|---|-----------|-------|
| 1 | [**外设访问**](01-外设访问/Guide.md) | GPIO、PWM、UART、SPI、I2C、CAN、USB、NVMe/SD、背光 —— Linux 用户空间与内核驱动接口 |
| 2 | [**网络与连接**](02-网络与连接/Guide.md) | 以太网、Wi-Fi、蓝牙、VPN、Web 服务器 —— 有线与无线连接栈 |
| 3 | [**GUI**](03-图形界面/Guide.md) | Qt、LVGL、基于 Web 的 UI、framebuffer —— 嵌入式产品的显示与触摸 |
| 4 | [**多媒体**](04-多媒体/Guide.md) | 音频、摄像头（USB + CSI）、GStreamer、显示输出、视频编码/解码 —— 硬件加速媒体流水线 |
| 5 | [**ML 与 AI**](05-机器学习与人工智能/Guide.md) | 量化、TensorRT、DLA、tinygrad、DeepStream、性能剖析 —— 边缘推理优化 |
| 6 | [**ROS 2**](06-ROS-2/Guide.md) | 节点、Nav2、DDS、实时、Jetson 部署 —— 机器人软件框架 |

---

## 子模块如何契合产品流程

```
Custom carrier board (Module 2) + L4T BSP (Module 3) + FSP (Module 4)
  │
  ├─ Peripheral Access ─── talk to sensors, actuators, buses
  ├─ Network ──────────── connect to cloud, fleet, local network
  ├─ GUI ──────────────── user-facing display (if applicable)
  ├─ Multimedia ────────── cameras, audio, video pipelines
  ├─ ML / AI ──────────── inference optimization on Orin Nano
  └─ ROS 2 ────────────── robot middleware tying it all together
        │
        ▼
  Security & OTA (Module 6) → Compliance (Module 7)
```

根据产品需求，以任意顺序完成子模块 1–4。**ML/AI**（子模块 5）与 **ROS 2**（子模块 6）通常建立在外设与多媒体的基础之上。


<details>
<summary>English original</summary>

**Application Development**

<div class="course-identity jetson-apps" markdown="1">
<div class="course-identity__icon">APP</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track B5 · Jetson Application Development</p>
<p class="course-identity__title">Build applications across peripherals, networking, UI, multimedia, ML inference, and ROS 2.</p>
<p class="course-identity__meta">Artifact: integrated Jetson app · Measure: latency, resources, UX, reliability</p>
</div>
</div>


**Phase 4 — Track B — Nvidia Jetson** · Module 5 of 7

> **Focus:** Build production application software on the **Jetson Orin Nano 8GB** — from low-level peripheral access (GPIO, UART, SPI, I2C, CAN) through networking, GUI, multimedia pipelines, **ML/AI inference** (TensorRT, DLA, tinygrad), and **ROS 2** integration.
>
> **Primary hardware:** Jetson Orin Nano 8GB on custom or dev-kit carrier

**Previous:** [4. FSP Customization](../04-FSP固件支持包定制/Guide.md) · **Next:** [6. Security and OTA](../06-安全与OTA/Guide.md)

---

**Sub-modules**

| # | Sub-module | Focus |
|---|-----------|-------|
| 1 | [**Peripheral Access**](01-外设访问/Guide.md) | GPIO, PWM, UART, SPI, I2C, CAN, USB, NVMe/SD, backlight — Linux userspace and kernel driver interfaces |
| 2 | [**Network and Connectivity**](02-网络与连接/Guide.md) | Ethernet, Wi-Fi, Bluetooth, VPN, web server — wired and wireless connectivity stack |
| 3 | [**GUI**](03-图形界面/Guide.md) | Qt, LVGL, web-based UI, framebuffer — display and touch for embedded products |
| 4 | [**Multimedia**](04-多媒体/Guide.md) | Audio, cameras (USB + CSI), GStreamer, display output, video encode/decode — hardware-accelerated media pipelines |
| 5 | [**ML and AI**](05-机器学习与人工智能/Guide.md) | Quantization, TensorRT, DLA, tinygrad, DeepStream, profiling — edge inference optimization |
| 6 | [**ROS 2**](06-ROS-2/Guide.md) | Nodes, Nav2, DDS, real-time, Jetson deployment — robot software framework |

---

**How the sub-modules fit the product flow**

```
Custom carrier board (Module 2) + L4T BSP (Module 3) + FSP (Module 4)
  │
  ├─ Peripheral Access ─── talk to sensors, actuators, buses
  ├─ Network ──────────── connect to cloud, fleet, local network
  ├─ GUI ──────────────── user-facing display (if applicable)
  ├─ Multimedia ────────── cameras, audio, video pipelines
  ├─ ML / AI ──────────── inference optimization on Orin Nano
  └─ ROS 2 ────────────── robot middleware tying it all together
        │
        ▼
  Security & OTA (Module 6) → Compliance (Module 7)
```

Work through sub-modules 1–4 in any order based on your product's needs. **ML/AI** (sub-module 5) and **ROS 2** (sub-module 6) typically build on the peripheral and multimedia foundations.

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
