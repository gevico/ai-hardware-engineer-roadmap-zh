---
title: 模块 5 — 安全标准与部署
description: 模块 5 — 安全标准与部署
---

# 模块 5 — 安全标准与部署

<div class="course-identity auto-course" style="--course-accent: #0d9488; --course-accent-rgb: 13, 148, 136;" markdown="1">
<div class="course-identity__icon">MSSA</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 专精方向</p>
<p class="course-identity__title">模块 5 — 安全标准与部署的专精课程标识。</p>
<p class="course-identity__meta">产物：专精案例研究 · 衡量指标：性能、可靠性、岗位契合度</p>
</div>
</div>


**父级：** [阶段 5 — 自动驾驶](../Guide.md)

**时间：** 3–6 个月

**前置要求：** 模块 1–2（基础 + openpilot 系统理解）。模块 4（高级感知）为推荐，但非必需。

---

## 为什么关注安全与部署

一个 99% 时间都能正常工作的感知模型，对于高速行驶的 2 吨重车辆而言并不足够安全。本模块涵盖安全标准、测试方法以及部署模式，它们填补了可用原型与量产 ADAS 之间的差距。

---

## 1. 功能安全标准

* **ISO 26262（道路车辆 — 功能安全）：**
    * ASIL 分级：从 A（最低）到 D（最高）的安全完整性等级。
    * 危害分析与风险评估（HARA）：严重度、暴露度、可控性。
    * 安全目标 → 功能安全需求 → 技术安全需求。
    * 安全生命周期：概念 → 开发 → 生产 → 运行。
    * 硬件指标：SPFM（单点故障度量）、LFM（潜在故障度量）。

* **SOTIF（ISO 21448 — 预期功能安全）：**
    * 处理来自传感器局限、算法不确定性以及不可预测环境的风险 —— 而不只是硬件故障（由 ISO 26262 覆盖）。
    * 已知/未知的不安全场景、触发条件。
    * 验证策略：降低预期功能带来的残余风险。

* **安全架构模式：**
    * **冗余：** 双通道监控（例如两条独立的感知路径）。
    * **异构冗余：** 同一安全功能采用不同算法或传感器。
    * **合理性监控：** 将感知输出与预期物理约束交叉校验。
    * **优雅降级：** 主系统性能下降时回退到更简单、更安全的行为。

**项目：**
* 为车道保持辅助功能执行一次 HARA。为识别出的安全目标分配 ASIL 等级，并提出缓解措施。
* 为自适应巡航控制（ACC）系统设计安全架构：识别单点故障，并提出冗余/监控方案。

---

## 2. V2X（车联万物）通信

* **通信标准：**
    * **DSRC（专用短程通信）：** 基于 802.11p，成熟但带宽有限。
    * **C-V2X（蜂窝 V2X）：** LTE-V2X（PC5 侧行链路）、5G NR-V2X —— 更高带宽、更低延迟。
    * V2V（车对车）、V2I（车对基础设施）、V2P（车对行人）。

* **协同感知：**
    * 车辆通过 V2X 共享传感器数据或目标检测结果。
    * 将有效感知范围扩展到单车 FoV 之外。
    * 挑战：延迟、带宽、数据格式标准化。

* **V2X 安全：**
    * IEEE 1609.2、ETSI ITS Security。
    * 证书颁发机构、假名认证。
    * 隐私保护通信（位置隐私与安全之间的权衡）。

---

## 3. ADAS 验证与测试

* **基于场景的测试：**
    * 场景数据库：OpenSCENARIO、ASAM OSI。
    * 系统化覆盖：边界情况、SOTIF 相关场景、ODD（运行设计域）边界。
    * 具体场景 vs. 逻辑场景 vs. 功能场景。

* **硬件在环（HIL）测试：**
    * 向量产 ECU 注入合成传感器数据。
    * 在受控、可重复的条件下验证 ADAS 软件。
    * 闭环 HIL：ECU 输出反馈回仿真。

* **影子模式部署：**
    * 让实验性感知与量产系统并行运行 —— 不作动。
    * 记录实验输出与量产输出之间的不一致。
    * 离线评估：从不一致中筛选出有针对性的测试集。
    * 指标：不一致率、假阳性/假阴性分析。

* **现场运行测试（FOT）：**
    * 由安全驾驶员陪同的受控真实道路测试。
    * 数据采集：指标、边界情况、ODD 下的系统性能。
    * 各地区的法规要求（美国、欧盟、中国）。

**项目：**
* 搭建一个简单的 HIL 测试台，向 ADAS 感知节点注入合成摄像头帧。在白天/夜晚/雾天场景中验证检测准确率。
* 将实验性感知算法以影子模式与基线并行部署。收集并分析不一致，以定位算法弱点。
* 为十字路口的行人过街创建一个 OpenSCENARIO 场景。用你的模块 1 控制器在 CARLA 中运行它。

---


<details>
<summary>English original</summary>

**Module 5 — Safety Standards and Deployment**

<div class="course-identity auto-course" style="--course-accent: #0d9488; --course-accent-rgb: 13, 148, 136;" markdown="1">
<div class="course-identity__icon">MSSA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for Module 5 — Safety Standards and Deployment.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [Phase 5 — Autonomous Driving](../Guide.md)

**Time:** 3–6 months

**Prerequisites:** Modules 1–2 (fundamentals + openpilot system understanding). Module 4 (Advanced Perception) is recommended but not required.

---

**Why safety and deployment**

A perception model that works 99% of the time is not safe enough for a 2-ton vehicle at highway speed. This module covers the standards, testing methodologies, and deployment patterns that bridge the gap between a working prototype and a production ADAS.

---

**1. Functional Safety Standards**

* **ISO 26262 (Road Vehicles — Functional Safety):**
    * ASIL classification: A (lowest) through D (highest) safety integrity levels.
    * Hazard Analysis and Risk Assessment (HARA): severity, exposure, controllability.
    * Safety goals → functional safety requirements → technical safety requirements.
    * Safety lifecycle: concept → development → production → operation.
    * Hardware metrics: SPFM (Single Point Fault Metric), LFM (Latent Fault Metric).

* **SOTIF (ISO 21448 — Safety of the Intended Functionality):**
    * Addresses risks from sensor limitations, algorithm uncertainty, and unpredictable environments — not just hardware faults (which ISO 26262 covers).
    * Known/unknown unsafe scenarios, triggering conditions.
    * Validation strategy: reduce residual risk from intended functionality.

* **Safety architecture patterns:**
    * **Redundancy:** Dual-channel monitoring (e.g., two independent perception paths).
    * **Diverse redundancy:** Different algorithms or sensors for the same safety function.
    * **Plausibility monitoring:** Cross-check perception outputs against expected physical constraints.
    * **Graceful degradation:** Fallback to simpler, safer behavior when primary system degrades.

**Projects:**
* Perform a HARA for a lane-keeping assist function. Assign ASIL levels to identified safety goals and propose mitigations.
* Design a safety architecture for an ACC system: identify single-point failures and propose redundancy/monitoring.

---

**2. V2X (Vehicle-to-Everything) Communication**

* **Communication standards:**
    * **DSRC (Dedicated Short-Range Communications):** 802.11p-based, mature but limited bandwidth.
    * **C-V2X (Cellular V2X):** LTE-V2X (PC5 sidelink), 5G NR-V2X — higher bandwidth, lower latency.
    * V2V (vehicle-to-vehicle), V2I (vehicle-to-infrastructure), V2P (vehicle-to-pedestrian).

* **Cooperative perception:**
    * Vehicles share sensor data or object detections via V2X.
    * Extends effective sensing range beyond individual vehicle FoV.
    * Challenges: latency, bandwidth, data format standardization.

* **V2X security:**
    * IEEE 1609.2, ETSI ITS Security.
    * Certificate authorities, pseudonymous authentication.
    * Privacy-preserving communication (location privacy vs. safety).

---

**3. ADAS Validation and Testing**

* **Scenario-based testing:**
    * Scenario databases: OpenSCENARIO, ASAM OSI.
    * Systematic coverage: edge cases, SOTIF-relevant scenarios, ODD (Operational Design Domain) boundaries.
    * Concrete vs. logical vs. functional scenarios.

* **Hardware-in-the-Loop (HIL) testing:**
    * Inject synthetic sensor data into production ECUs.
    * Validate ADAS software under controlled, repeatable conditions.
    * Closed-loop HIL: ECU outputs feed back into simulation.

* **Shadow mode deployment:**
    * Run experimental perception in parallel with production system — no actuation.
    * Log disagreements between experimental and production outputs.
    * Offline evaluation: curate targeted test sets from disagreements.
    * Metric: disagreement rate, false positive/negative analysis.

* **Field operational tests (FOT):**
    * Controlled real-world testing with safety drivers.
    * Data collection: metrics, edge cases, system performance under ODD.
    * Regulatory requirements by region (US, EU, China).

**Projects:**
* Build a simple HIL test rig that injects synthetic camera frames into an ADAS perception node. Validate detection accuracy across day/night/fog scenarios.
* Deploy an experimental perception algorithm in shadow mode alongside a baseline. Collect and analyze disagreements to identify algorithmic weaknesses.
* Create an OpenSCENARIO scenario for a pedestrian crossing at an intersection. Run it in CARLA with your Module 1 controller.

---

</details>

## Resources

| Resource | Why |
|----------|-----|
| ISO 26262 Standard | 汽车电子的基础安全标准 |
| ISO 21448 (SOTIF) | ADAS/AD 预期功能安全 |
| *Autonomous Vehicles and Functional Safety*（Tier 1 指南） | 应用 ISO 26262 + SOTIF 的实践指南 |
| [5GAA](https://5gaa.org/) | C-V2X 标准、用例、部署指南 |
| [OpenSCENARIO](https://www.asam.net/standards/detail/openscenario/) | ADAS 测试的场景描述标准 |
| [CARLA](https://carla.org/) | 用于 HIL 与场景化测试的仿真 |

---

## Next

→ **[模块 6 — Lauterbach TRACE32 Debug](../06-Lauterbach-TRACE32调试/Guide.md)**（可选）— 面向汽车 ECU 的在线调试与 trace。


<details>
<summary>English original</summary>

**Resources**

| Resource | Why |
|----------|-----|
| ISO 26262 Standard | Foundational safety standard for automotive electronics |
| ISO 21448 (SOTIF) | Safety of intended functionality for ADAS/AD |
| *Autonomous Vehicles and Functional Safety* (Tier 1 guides) | Practical guides to applying ISO 26262 + SOTIF |
| [5GAA](https://5gaa.org/) | C-V2X standards, use cases, deployment guidance |
| [OpenSCENARIO](https://www.asam.net/standards/detail/openscenario/) | Scenario description standard for ADAS testing |
| [CARLA](https://carla.org/) | Simulation for HIL and scenario-based testing |

---

**Next**

→ **[Module 6 — Lauterbach TRACE32 Debug](../06-Lauterbach-TRACE32调试/Guide.md)** (optional) — In-circuit debug and trace for automotive ECUs.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/5. Safety Standards and Deployment/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/5.%20Safety%20Standards%20and%20Deployment/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
