---
title: 合规与制造
description: 合规与制造
---

# 合规与制造

<div class="course-identity compliance" markdown="1">
<div class="course-identity__icon">MFG</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 B7 · 合规与制造</p>
<p class="course-identity__title">为认证、测试夹具、生产与现场可靠性准备嵌入式 AI 硬件。</p>
<p class="course-identity__meta">产物：制造就绪包 · 度量：测试覆盖率、良率风险、合规缺口</p>
</div>
</div>


**阶段 4 — 方向 B — Nvidia Jetson** · 模块 7 / 7

> **重点：** 将一款已验证的 **Jetson Orin Nano 8GB** 产品从工程原型推进过 **法规认证**（FCC/CE/IC）、**DFM 评审**、**量产烧录基础设施**、**供应链管理** 与 **设备群运营** —— 这是量产出货与现场维护前的最后一公里。
>
> **主要硬件：** 定制载板上的 Jetson Orin Nano 8GB（来自模块 2）

**上一节：** [6. 安全与 OTA](../06-安全与OTA/Guide.md)

---


## 1. 最后一公里 —— 为什么会有本模块

工程原型在实验台上能跑通。要出货的产品还必须是：

- **合法** —— 销售前通过法规测试（FCC、CE）
- **可制造** —— 代工厂能以规模化的方式可重复地制造
- **可支持** —— 能在现场对设备进行更新、监控与维修

本模块覆盖从“板子能跑”到“产品交到客户手中”之间的全部环节。

---

## 2. Jetson 产品的法规版图

### 适用于你的产品的法规

| 法规 | 地区 | 适用情形 |
|-----------|--------|-------------|
| **FCC Part 15 Subpart B** | 美国 | 在美国销售的任何数字设备 |
| **CE（EMC 指令）** | 欧盟 | 在欧盟销售的任何电子产品 |
| **IC (ISED)** | 加拿大 | 在加拿大销售的任何数字设备 |
| **UKCA** | 英国 | 脱欧后 CE 的等效认证 |
| **RCM** | 澳大利亚/新西兰 | 在 AU/NZ 销售的任何电气产品 |
| **MIC / VCCI** | 日本 | 在日本销售的任何数字设备 |

### 模块级认证 vs 系统级认证

Jetson Orin Nano **模块**可能有 NVIDIA 提供的自身 EMC 特性数据，但你的 **系统**（模块 + 定制载板 + 外壳 + 线缆）需要 **系统级测试**。外壳、线缆布线与载板设计都会影响辐射。

**你的完整产品几乎总是需要系统级 FCC/CE 测试**。

---

## 3. FCC 认证（无意辐射器）

大多数不带内置无线的 Jetson 产品，属于 FCC Part 15 Subpart B 下的 **无意辐射器**。

### 测试类别

| 测试 | 标准 | 测量内容 |
|------|----------|-----------------|
| **辐射发射** | ANSI C63.4 | 设备与线缆辐射出的 RF 能量 |
| **传导发射** | CISPR 32 / FCC Part 15 | 传导回电源线上的 RF 噪声 |

### Jetson 载板上常见的发射源

| 来源 | 频率范围 | 典型对策 |
|--------|----------------|-------------|
| **USB 3.0 super-speed** | 2.4–5 GHz 扩频 | 屏蔽线缆、共模扼流圈、扩频时钟 |
| **HDMI/DP 时钟** | 像素时钟的谐波 | 屏蔽连接器、线缆上加磁环 |
| **开关稳压器** | 100 kHz–30 MHz | 合理的布局布线、输入/输出滤波、屏蔽 |
| **高速 PCIe** | 2.5–16 GHz（Gen 1–4） | 正确的端接、短走线、接地外壳 |
| **以太网** | 100 MHz–1 GHz | 正确的磁性元件、合理的 PHY 布局布线 |

### 流程时间表

| 步骤 | 时长 | 备注 |
|------|----------|-------|
| 预合规扫描（内部） | 1–2 周 | 在支付实验室费用前发现问题 |
| 修复已发现的问题 | 1–4 周 | 布局布线更改、滤波、屏蔽 |
| 正式实验室测试 | 1–2 周 | 在认可的测试实验室（A2LA、NVLAP）进行 |
| 报告与 FCC 备案 | 2–4 周 | 针对无意辐射器的 SDoC（供应商符合性声明） |
| **总计** | **6–12 周** | 预留至少一轮复测的预算 |

### 成本估算

| 项目 | 成本范围 |
|------|-----------|
| 预合规扫描（租用或实验室） | $500–$2,000 |
| 正式 FCC Part 15B 测试 + 报告 | $3,000–$8,000 |
| 复测（若失败 + 修复） | $2,000–$5,000 |

---

## 4. CE 标志（欧盟）

CE 标志要求符合多项指令：

| 指令 | 标准 | 适用于 |
|-----------|----------|-----------|
| **EMC 指令（2014/30/EU）** | EN 55032（发射）、EN 55035（抗扰度） | 所有电子产品 |
| **LVD（2014/35/EU）** | EN 62368-1 | 工作在 50–1000 VAC 或 75–1500 VDC 的产品 |
| **RED（2014/53/EU）** | EN 300 328 等 | 带有意无线电（WiFi、BT、蜂窝）的产品 |
| **RoHS（2011/65/EU）** | — | 限制电子产品中的有害物质 |


<details>
<summary>English original</summary>

**Compliance and Manufacturing**

<div class="course-identity compliance" markdown="1">
<div class="course-identity__icon">MFG</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track B7 · Compliance & Manufacturing</p>
<p class="course-identity__title">Prepare embedded AI hardware for certification, test fixtures, production, and field reliability.</p>
<p class="course-identity__meta">Artifact: manufacturing readiness pack · Measure: test coverage, yield risks, compliance gaps</p>
</div>
</div>


**Phase 4 — Track B — Nvidia Jetson** · Module 7 of 7

> **Focus:** Take a validated **Jetson Orin Nano 8GB** product from engineering prototype through **regulatory certification** (FCC/CE/IC), **DFM review**, **production flashing infrastructure**, **supply-chain management**, and **fleet operations** — the last mile before volume shipping and field sustaining.
>
> **Primary hardware:** Jetson Orin Nano 8GB on custom carrier (from Module 2)

**Previous:** [6. Security and OTA](../06-安全与OTA/Guide.md)

---


**1. The last mile — why this module exists**

Engineering prototypes work on the bench. Shipping products must also be:

- **Legal** — pass regulatory testing (FCC, CE) before you can sell
- **Manufacturable** — a contract manufacturer can build them repeatably at scale
- **Supportable** — you can update, monitor, and repair devices in the field

This module covers everything between "board works" and "product in customer hands."

---

**2. Regulatory landscape for Jetson products**

**What applies to your product**

| Regulation | Region | Applies when |
|-----------|--------|-------------|
| **FCC Part 15 Subpart B** | USA | Any digital device marketed in the US |
| **CE (EMC Directive)** | EU | Any electronic product sold in the EU |
| **IC (ISED)** | Canada | Any digital device marketed in Canada |
| **UKCA** | UK | Post-Brexit equivalent of CE |
| **RCM** | Australia/NZ | Any electrical product sold in AU/NZ |
| **MIC / VCCI** | Japan | Any digital device marketed in Japan |

**Modular vs system-level certification**

The Jetson Orin Nano **module** may have its own EMC characterization data from NVIDIA, but your **system** (module + custom carrier + enclosure + cables) requires **system-level testing**. The enclosure, cable routing, and carrier board design all affect emissions.

**You almost always need system-level FCC/CE testing** for your complete product.

---

**3. FCC certification (unintentional radiator)**

Most Jetson products without built-in wireless are **unintentional radiators** under FCC Part 15 Subpart B.

**Testing categories**

| Test | Standard | What it measures |
|------|----------|-----------------|
| **Radiated emissions** | ANSI C63.4 | RF energy radiated from the device and cables |
| **Conducted emissions** | CISPR 32 / FCC Part 15 | RF noise conducted back onto the power line |

**Common emission sources on Jetson carriers**

| Source | Frequency range | Typical fix |
|--------|----------------|-------------|
| **USB 3.0 super-speed** | 2.4–5 GHz spread spectrum | Shielded cables, common-mode chokes, spread-spectrum clocking |
| **HDMI/DP clock** | Harmonics of pixel clock | Shielded connector, ferrite on cable |
| **Switching regulators** | 100 kHz–30 MHz | Proper layout, input/output filtering, shielding |
| **High-speed PCIe** | 2.5–16 GHz (Gen 1–4) | Proper termination, short traces, grounded enclosure |
| **Ethernet** | 100 MHz–1 GHz | Correct magnetics, proper PHY layout |

**Process timeline**

| Step | Duration | Notes |
|------|----------|-------|
| Pre-compliance scan (in-house) | 1–2 weeks | Identify issues before paying for lab time |
| Fix identified issues | 1–4 weeks | Layout changes, filtering, shielding |
| Formal lab testing | 1–2 weeks | At an accredited test lab (A2LA, NVLAP) |
| Report and FCC filing | 2–4 weeks | SDoC (Supplier's Declaration of Conformity) for unintentional radiators |
| **Total** | **6–12 weeks** | Budget for at least one re-test cycle |

**Cost estimate**

| Item | Cost range |
|------|-----------|
| Pre-compliance scan (rental or lab) | $500–$2,000 |
| Formal FCC Part 15B test + report | $3,000–$8,000 |
| Re-test (if fail + fix) | $2,000–$5,000 |

---

**4. CE marking (EU)**

CE marking requires compliance with multiple directives:

| Directive | Standard | Applies to |
|-----------|----------|-----------|
| **EMC Directive (2014/30/EU)** | EN 55032 (emissions), EN 55035 (immunity) | All electronic products |
| **LVD (2014/35/EU)** | EN 62368-1 | Products operating 50–1000 VAC or 75–1500 VDC |
| **RED (2014/53/EU)** | EN 300 328, etc. | Products with intentional radio (WiFi, BT, cellular) |
| **RoHS (2011/65/EU)** | — | Restriction of hazardous substances in electronics |

</details>

### 与 FCC 的主要区别

- CE 包含**抗扰度测试**（ESD、浪涌、传导抗扰度、辐射抗扰度）——FCC 不包含
- CE 要求**符合性声明（Declaration of Conformity，DoC）**——由你自己声明
- 如果加入 WiFi/BT/cellular → 适用 RED，会额外增加无线电专项测试

### 抗扰度测试（常见不合格项）

| 测试 | 标准 | 常见失效模式 |
|------|----------|-------------------|
| **ESD**（接触 ±8 kV，空气 ±15 kV） | EN 61000-4-2 | USB 端口、外露连接器、金属外壳 |
| **辐射抗扰度**（3 V/m） | EN 61000-4-3 | 未屏蔽电缆充当天线 |
| **EFT（Electrical Fast Transient，电快速瞬变脉冲群）** | EN 61000-4-4 | 电源输入端 |
| **浪涌** | EN 61000-4-5 | 电源输入、Ethernet |

---

## 5. 其他市场（IC、MIC、UKCA、RCM）

### 优先级策略

- **以 FCC + CE 首发**——覆盖美国 + 欧盟这两个最大市场
- **再加 IC（加拿大）**——通常与 FCC 测试打包（同一实验室、同一趟）
- 视市场需求**再加其他认证**

| 认证 | 在 FCC+CE 之外的工作量 | 备注 |
|--------------|---------------------|-------|
| **IC（ISED）** | 极小——测试相同，只是申报不同 | 通常与 FCC 同时完成 |
| **UKCA** | 与 CE 类似，声明单独出 | 脱欧后的要求 |
| **RCM** | 基于 CISPR 32（与 EN 55032 类似） | 在 ACMA 注册 |
| **MIC/VCCI** | VCCI 为自愿性自我声明 | 基于 CISPR 32 |

---

## 6. 预合规测试

在花钱买正式实验室时间**之前**，先抓出发射问题。

### 最低设备配置

| 设备 | 成本 | 用途 |
|-----------|------|---------|
| **近场探头组** | $200–$500 | 定位 PCB 上的发射源 |
| **频谱分析仪**（或 HackRF 之类的 SDR） | $300–$2,000 | 查看发射频谱 |
| **电流探头**（RF） | $100–$300 | 测量电缆上的传导发射 |

### 预扫描流程

1. 按预期配置搭建设备（所有电缆、外壳）
2. 运行最坏情况工作负载（AI 推理 + USB + Ethernet 同时活动）
3. 用近场探头扫描，找出 PCB 上的热点
4. 用频谱分析仪测量大致场强
5. 留出余量，与 FCC/CISPR 限值比较
6. 迭代：加滤波、加屏蔽，或修改布局布线

---

## 7. DFM 评审与面向装配的设计

### Jetson 载板的 DFM 检查清单

| 项目 | 检查项 |
|------|-------|
| **拼板** | 板子适配标准拼板尺寸，以配合 CM 的贴片 |
| **基准点** | 全局与局部基准点按 IPC-7351 放置 |
| **锡膏钢网** | 细间距元件的开孔内缩（SoM 连接器 0.5 mm 间距） |
| **元件方向** | 所有极性元件方向一致，便于检查 |
| **贴片坐标** | 生成并核对 centroid 文件 |
| **回流焊温度曲线** | 针对 SoM 连接器和所有元件验证（无铅 SAC305） |
| **测试可达性** | ICT 测试点置于底面，可被针床访问 |
| **三防漆** | 如有需要（工业/户外），为连接器留出禁涂区 |
| **装配顺序** | SoM 连接器 → SMT → 通孔 → 插入 SoM → 结构件 |

### 面向装配的设计（DFA）

- 尽量减少螺丝种类
- 尽可能采用卡扣或免工具装配
- 设计外壳，使 PCB 从一个方向放入
- 在丝印上标注测试点和调试排针

---

## 8. 量产烧录基础设施

### 烧录站设计

```
Flash station:
  ├─ Host PC (Ubuntu 22.04, 32 GB RAM, NVMe SSD)
  │     └─ Linux_for_Tegra + signed images
  │
  ├─ USB hub (powered, USB 3.0)
  │     └─ 1–4 USB recovery cables to Jetson devices
  │
  ├─ Power supply (one per device, or multi-output)
  │
  └─ Flash script (automated: flash + validate + log serial number)
```

### 用 `l4t_initrd_flash.sh` 批量烧录

```bash
# Flash external NVMe on multiple devices
sudo ./tools/kernel_flash/l4t_initrd_flash.sh \
    --massflash 4 \
    <board_config> \
    external
```

`--massflash N` 生成 N 个烧录镜像，可同时并行烧写到 N 台设备。

### 烧录时间优化

| 方式 | 烧录时间（单台） |
|----------|----------------------|
| 标准 `flash.sh`（USB 2.0） | 15–25 min |
| `l4t_initrd_flash.sh`（USB 3.0） | 8–15 min |
| 批量克隆镜像（把 raw 镜像写入 NVMe） | 3–5 min |
| 预烧录 NVMe（由供应商预先写入的 SSD） | 0 min（上线即用） |

### 工厂镜像管理

- **Golden master：** 经过签名与测试的镜像，标注版本和构建日期
- **镜像签名：** 属于 CI/CD 的一环（见 [Module 7 — Security and OTA](../06-安全与OTA/Guide.md)）
- **版本追踪：** 烧录脚本把 `{serial_number, image_version, timestamp, pass/fail}` 记录到数据库

---

## 9. 工厂测试治具与流程


<details>
<summary>English original</summary>

**Key differences from FCC**

- CE includes **immunity testing** (ESD, surge, conducted immunity, radiated immunity) — FCC does not
- CE requires a **Declaration of Conformity (DoC)** — you self-declare
- If you add WiFi/BT/cellular → RED applies, which adds radio-specific testing

**Immunity tests (commonly failed)**

| Test | Standard | Common failure mode |
|------|----------|-------------------|
| **ESD** (±8 kV contact, ±15 kV air) | EN 61000-4-2 | USB ports, exposed connectors, metal enclosure |
| **Radiated immunity** (3 V/m) | EN 61000-4-3 | Unshielded cables act as antennas |
| **EFT (Electrical Fast Transient)** | EN 61000-4-4 | Power supply input |
| **Surge** | EN 61000-4-5 | Power input, Ethernet |

---

**5. Other markets (IC, MIC, UKCA, RCM)**

**Prioritization strategy**

- **Launch with FCC + CE** — covers USA + EU, the two largest markets
- **Add IC (Canada)** — often bundled with FCC testing (same lab, same trip)
- **Add others** as market demand requires

| Certification | Effort beyond FCC+CE | Notes |
|--------------|---------------------|-------|
| **IC (ISED)** | Minimal — same tests, different filing | Often done simultaneously with FCC |
| **UKCA** | Similar to CE, separate declaration | Post-Brexit requirement |
| **RCM** | Based on CISPR 32 (similar to EN 55032) | Register with ACMA |
| **MIC/VCCI** | VCCI is voluntary self-declaration | Based on CISPR 32 |

---

**6. Pre-compliance testing**

Catch emissions problems **before** paying for formal lab time.

**Minimum equipment**

| Equipment | Cost | Purpose |
|-----------|------|---------|
| **Near-field probe set** | $200–$500 | Localize emission sources on the PCB |
| **Spectrum analyzer** (or SDR like HackRF) | $300–$2,000 | View spectrum of emissions |
| **Current probe** (RF) | $100–$300 | Measure conducted emissions on cables |

**Pre-scan workflow**

1. Set up the device in its intended configuration (all cables, enclosure)
2. Run worst-case workload (AI inference + USB + Ethernet active)
3. Scan with near-field probes to identify hot spots on the PCB
4. Use spectrum analyzer to measure approximate field strength
5. Compare against FCC/CISPR limits with margin
6. Iterate: add filtering, shielding, or layout fixes

---

**7. DFM review and design-for-assembly**

**DFM checklist for Jetson carrier boards**

| Item | Check |
|------|-------|
| **Panelization** | Board fits standard panel sizes for your CM's pick-and-place |
| **Fiducials** | Global and local fiducials placed per IPC-7351 |
| **Solder paste stencil** | Aperture reductions for fine-pitch parts (SoM connector 0.5 mm pitch) |
| **Component orientation** | All polarized components oriented consistently for inspection |
| **Pick-and-place coordinates** | Centroid file generated and verified |
| **Reflow profile** | Validated for the SoM connector and all components (lead-free SAC305) |
| **Test access** | ICT test points on bottom side, bed-of-nails accessible |
| **Conformal coating** | If required (industrial/outdoor), mask keep-out for connectors |
| **Assembly sequence** | SoM connector → SMT → through-hole → SoM insertion → mechanical |

**Design-for-assembly (DFA)**

- Minimize the number of unique screw types
- Use snap-fit or tool-free assembly where possible
- Design the enclosure so the PCB drops in from one direction
- Label test points and debug headers on the silkscreen

---

**8. Production flashing infrastructure**

**Flash station design**

```
Flash station:
  ├─ Host PC (Ubuntu 22.04, 32 GB RAM, NVMe SSD)
  │     └─ Linux_for_Tegra + signed images
  │
  ├─ USB hub (powered, USB 3.0)
  │     └─ 1–4 USB recovery cables to Jetson devices
  │
  ├─ Power supply (one per device, or multi-output)
  │
  └─ Flash script (automated: flash + validate + log serial number)
```

**Batch flashing with `l4t_initrd_flash.sh`**

```bash
# Flash external NVMe on multiple devices
sudo ./tools/kernel_flash/l4t_initrd_flash.sh \
    --massflash 4 \
    <board_config> \
    external
```

`--massflash N` creates N flash images that can be applied in parallel to N devices simultaneously.

**Flash time optimization**

| Approach | Flash time (per device) |
|----------|----------------------|
| Standard `flash.sh` (USB 2.0) | 15–25 min |
| `l4t_initrd_flash.sh` (USB 3.0) | 8–15 min |
| Mass-cloned image (write raw image to NVMe) | 3–5 min |
| Pre-flashed NVMe (SSD pre-loaded by supplier) | 0 min (on-line) |

**Factory image management**

- **Golden master:** A signed, tested image tagged with version and build date
- **Image signing:** Part of CI/CD (see [Module 7 — Security and OTA](../06-安全与OTA/Guide.md))
- **Version tracking:** Flash script logs `{serial_number, image_version, timestamp, pass/fail}` to a database

---

**9. Factory test fixtures and procedures**

</details>

### 测试治具设计

对于带定制连接器的载板，搭建 **bed-of-nails** 测试治具：

- **Pogo pin** 接触 PCB 底面的测试点
- **气动或手动压机** 将板压紧以保持接触
- **测试控制器**（Raspberry Pi、STM32 或主机 PC）运行自动化测试序列

### 自动化测试序列

```
1. Apply power → verify rails (pass/fail per rail)
2. Insert SoM module (or pre-inserted)
3. Boot → wait for UART "login:" prompt (timeout = 60s)
4. Run test script on target:
   a. USB: enumerate test device
   b. Ethernet: ping gateway, iperf short burst
   c. NVMe: read/write test
   d. CSI: capture one frame (if camera connected)
   e. I2C: scan for expected device addresses
   f. CAN: send/receive loopback frame
   g. GPIO: toggle test pins, read back
   h. Temperature: read thermal zone (sanity check)
5. Program serial number and MAC address to EEPROM
6. Log results to database
7. Print pass/fail label

Total time target: < 60 seconds per unit
```

### 通过/失败判据

为每项测试定义 **量化阈值**：

| 测试项 | 通过判据 |
|------|-------------|
| 5V 电源轨 | 4.85–5.15 V |
| 以太网吞吐 | > 900 Mbps |
| NVMe 顺序读 | > 2 GB/s |
| 启动时间 | < 30 s 到达登录提示符 |
| 空闲时 GPU 温度 | < 50 C |

---

## 10. 供应链管理

### Jetson 模块采购

| 来源 | 交期 | MOQ | 备注 |
|--------|-----------|-----|-------|
| **NVIDIA 直供** | 12–16 周 | 视情况 | 大批量需 NVIDIA 账号 |
| **Arrow / Avnet** | 8–16 周 | 1–100+ | 授权分销商 |
| **Mouser / Digikey** | 现货或 8–12 周 | 1+ | 用于原型制作和小批量 |

### 关键元器件跟踪

为元器件维护一份 **风险登记表**：

| 风险等级 | 元器件 | 缓解措施 |
|-----------|-----------|------------|
| **高** | SoM 连接器（Molex，单源） | 备货（3 个月以上） |
| **高** | Jetson Orin Nano 模块 | 交期长 — 提前下单 |
| **中** | 以太网 PHY | 认证合格的第二货源 |
| **低** | 无源器件 | 多厂商、标准规格 |

### 管理 SoM 版本变更

NVIDIA 会定期发布更新后的模块版本。载板和 BSP 必须针对每个新版本完成验证：

1. 监控 NVIDIA 的 **产品变更通知（PCN）**
2. 新版本发布后，订购样品
3. 重新执行板级 bring-up（上电点亮/调通）验证（模块 4）和 BSP 测试
4. 若需要修改设备树或驱动，则更新 BSP
5. 在量产中接收新版本模块之前，完成更新后固件的认证与发布

---

## 11. 配置管理与可追溯性

### 序列号方案

```
JET-<year><week>-<sequence>
Example: JET-2626-00142
         │    │     │
         │    │     └─ Unit 142
         │    └─ Year 2026, week 26
         └─ Product prefix (example)
```

### 每台设备需跟踪的内容

| 字段 | 来源 | 存储位置 |
|-------|--------|-----------|
| 序列号 | 工厂测试时分配 | EEPROM + 数据库 |
| MAC 地址 | 从分配的地址段中分配 | EEPROM + 数据库 |
| SoM 序列号 | 从模块 EEPROM 读取 | 数据库 |
| SoM 版本 | 从模块读取 | 数据库 |
| 固件版本 | 烧录镜像的版本标签 | 数据库 + 设备 |
| 工厂测试结果 | 测试脚本输出 | 数据库 |
| 发货日期 | 履约系统 | 数据库 |

### 构建产物管理

- 在 git 中为每个发布镜像打标签：`v1.0.0-rc1`、`v1.0.0`
- 将签名镜像存放在带版本管理的产物仓库中（S3、Artifactory 或本地 NAS）
- 绝不覆盖 — 追加新版本
- 将构建日志和测试报告与镜像一同保存

---

## 12. 设备集群管理与现场支持

### 远程访问

| 方式 | 使用场景 | 安全性 |
|--------|----------|----------|
| **基于 VPN 的 SSH**（WireGuard、Tailscale） | 调试、日志收集、人工干预 | 强（加密隧道、基于密钥） |
| **反向 SSH 隧道** | 位于 NAT 后的设备，无 VPN 基础设施 | 中（需要跳板机） |
| **MQTT 遥测** | 心跳、指标、轻量命令 | 中（TLS + 认证） |
| **OTA agent** | 固件与配置更新 | 强（签名镜像、TLS） |

### 集群仪表盘

连接到 [模块 7 — 安全与 OTA](../06-安全与OTA/Guide.md) 中的遥测技术栈：

- **设备清单：** 在线/离线状态、固件版本、最近上报时间
- **健康指标：** CPU/GPU 温度、磁盘占用、内存占用、运行时长
- **OTA 状态：** 每台设备的当前版本、待执行更新、回滚事件
- **告警：** 设备离线、重启循环、温度告警、磁盘写满

### 固件更新节奏

| 类型 | 频率 | 触发条件 |
|------|-----------|---------|
| **安全补丁** | 按需（尽快） | kernel、L4T 或应用依赖中出现 CVE |
| **功能更新** | 每月或每季度 | 产品路线图 |
| **紧急热修复** | 立即 | 影响现场设备的严重 bug |

---


<details>
<summary>English original</summary>

**Test jig design**

For carrier boards with custom connectors, build a **bed-of-nails** test jig:

- **Pogo pins** contact test points on the bottom of the PCB
- **Pneumatic or manual press** holds the board in contact
- **Test controller** (Raspberry Pi, STM32, or host PC) runs automated test sequence

**Automated test sequence**

```
1. Apply power → verify rails (pass/fail per rail)
2. Insert SoM module (or pre-inserted)
3. Boot → wait for UART "login:" prompt (timeout = 60s)
4. Run test script on target:
   a. USB: enumerate test device
   b. Ethernet: ping gateway, iperf short burst
   c. NVMe: read/write test
   d. CSI: capture one frame (if camera connected)
   e. I2C: scan for expected device addresses
   f. CAN: send/receive loopback frame
   g. GPIO: toggle test pins, read back
   h. Temperature: read thermal zone (sanity check)
5. Program serial number and MAC address to EEPROM
6. Log results to database
7. Print pass/fail label

Total time target: < 60 seconds per unit
```

**Pass/fail criteria**

Define **quantitative thresholds** for every test:

| Test | Pass criteria |
|------|-------------|
| 5V rail | 4.85–5.15 V |
| Ethernet throughput | > 900 Mbps |
| NVMe sequential read | > 2 GB/s |
| Boot time | < 30 s to login prompt |
| GPU temperature at idle | < 50 C |

---

**10. Supply chain management**

**Jetson module procurement**

| Source | Lead time | MOQ | Notes |
|--------|-----------|-----|-------|
| **NVIDIA direct** | 12–16 weeks | Varies | For large volumes, requires NVIDIA account |
| **Arrow / Avnet** | 8–16 weeks | 1–100+ | Authorized distributors |
| **Mouser / Digikey** | Stock or 8–12 weeks | 1+ | For prototyping and small runs |

**Critical component tracking**

Maintain a **risk register** for components:

| Risk level | Component | Mitigation |
|-----------|-----------|------------|
| **High** | SoM connector (Molex, single source) | Buffer stock (3+ months) |
| **High** | Jetson Orin Nano module | Long lead time — order early |
| **Medium** | Ethernet PHY | Second-source qualified alternate |
| **Low** | Passive components | Multiple manufacturers, standard values |

**Managing SoM revision changes**

NVIDIA periodically releases updated module revisions. Your carrier and BSP must be validated against each new revision:

1. Monitor NVIDIA **Product Change Notifications (PCN)**
2. When a new revision is announced, order samples
3. Re-run board bring-up validation (Module 4) and BSP tests
4. Update BSP if device tree or driver changes are required
5. Qualify and release updated firmware before accepting new-revision modules in production

---

**11. Configuration management and traceability**

**Serial number scheme**

```
JET-<year><week>-<sequence>
Example: JET-2626-00142
         │    │     │
         │    │     └─ Unit 142
         │    └─ Year 2026, week 26
         └─ Product prefix (example)
```

**What to track per device**

| Field | Source | Stored in |
|-------|--------|-----------|
| Serial number | Assigned during factory test | EEPROM + database |
| MAC address(es) | Assigned from allocated range | EEPROM + database |
| SoM serial number | Read from module EEPROM | Database |
| SoM revision | Read from module | Database |
| Firmware version | Flashed image version tag | Database + device |
| Factory test result | Test script output | Database |
| Ship date | Fulfillment system | Database |

**Build artifact management**

- Tag every release image in git: `v1.0.0-rc1`, `v1.0.0`
- Store signed images in a versioned artifact repository (S3, Artifactory, or local NAS)
- Never overwrite — append new versions
- Keep build logs and test reports alongside the image

---

**12. Fleet management and field support**

**Remote access**

| Method | Use case | Security |
|--------|----------|----------|
| **SSH over VPN** (WireGuard, Tailscale) | Debug, log collection, manual intervention | Strong (encrypted tunnel, key-based) |
| **Reverse SSH tunnel** | Devices behind NAT, no VPN infrastructure | Medium (requires jump server) |
| **MQTT telemetry** | Heartbeat, metrics, light commands | Medium (TLS + auth) |
| **OTA agent** | Firmware and configuration updates | Strong (signed images, TLS) |

**Fleet dashboard**

Connect to the telemetry stack from [Module 7 — Security and OTA](../06-安全与OTA/Guide.md):

- **Device inventory:** Online/offline status, firmware version, last check-in
- **Health metrics:** CPU/GPU temperature, disk usage, memory usage, uptime
- **OTA status:** Current version per device, pending updates, rollback events
- **Alerts:** Offline devices, reboot loops, temperature alarms, disk full

**Firmware update cadence**

| Type | Frequency | Trigger |
|------|-----------|---------|
| **Security patches** | As needed (ASAP) | CVE in kernel, L4T, or application dependency |
| **Feature updates** | Monthly or quarterly | Product roadmap |
| **Emergency hotfix** | Immediate | Critical bug affecting field devices |

---

</details>

## 13. RMA 与失效分析

### RMA 流程

```
Customer reports issue
  │
  ├─ Remote triage (logs, telemetry, OTA status check)
  │     └─ Software fix? → Push OTA update, close
  │
  ├─ Hardware suspected → Issue RMA authorization
  │     └─ Customer ships device back
  │
  ├─ Incoming inspection
  │     ├─ Visual inspection (physical damage, corrosion, burnt components)
  │     ├─ Re-run factory test sequence
  │     └─ Compare with original factory test log
  │
  ├─ Fault isolation
  │     ├─ Swap SoM → carrier issue
  │     ├─ Swap carrier → SoM issue
  │     └─ Neither → environmental or software root cause
  │
  └─ Resolution
        ├─ Repair and return
        ├─ Replace with new unit
        └─ Update design if systemic issue
```

### 失效跟踪

将所有失效按类别记录到数据库中：

| 类别 | 示例 | 措施 |
|----------|---------|--------|
| **DOA（到货即损）** | 单元在客户现场从未启动 | 提高工厂测试覆盖率 |
| **早期失效** | 前 30 天内失效 | 可能是焊接缺陷——复查回流焊曲线 |
| **磨损失效** | 18 个月后风扇失效 | 选用 MTBF 更高的风扇，或增加风扇监控 |
| **环境** | 外露连接器腐蚀 | 增加三防漆或密封连接器 |
| **软件** | OTA 使设备变砖 | 改进回滚机制（模块 7） |

---

## 14. 项目

- **量产烧录站：** 使用 `--massflash` 搭建一个可并行烧录 4 台 Jetson Orin Nano 的烧录站。为流程计时并优化。
- **工厂测试脚本：** 编写自动化测试脚本，在 60 秒内验证自定义载板上的所有外设。输出结构化的 JSON 通过/失败报告。
- **预合规扫描：** 使用近场探头和频谱分析仪（或 SDR），扫描运行最坏情况 AI 工作负载的载板。记录前 3 大发射源及建议的缓解措施。
- **机群仪表盘：** 部署 3 台以上带唯一序列号的单元，搭建 Grafana 仪表盘，展示设备健康、OTA 版本和运行时间。模拟向所有设备推送 OTA。

---

## 15. 资源

| 资源 | 说明 |
|----------|-------------|
| **FCC 设备认证** | FCC.gov 的 Part 15 认证流程指南 |
| **CISPR 32 (EN 55032)** | 多媒体设备发射的国际标准 |
| **EN 62368-1** | 音频/视频、IT 与通信设备的安全标准 |
| **IPC-A-610** | 电子组件的可接受性（工艺标准） |
| **IPC-7711/7721** | 电子组件的返工、改装与维修 |
| **NVIDIA Jetson 合作伙伴硬件设计** | OEM 设计指南、参考设计文件、认证说明 |
| [2. 自定义载板设计](../02-定制载板设计与启动/Guide.md) | 硬件设计（本模块假定载板已构建并验证） |
| [7. 安全与 OTA](../06-安全与OTA/Guide.md) | 签名镜像、机群遥测（为工厂烧录和机群运维提供输入） |


<details>
<summary>English original</summary>

**13. RMA and failure analysis**

**RMA process**

```
Customer reports issue
  │
  ├─ Remote triage (logs, telemetry, OTA status check)
  │     └─ Software fix? → Push OTA update, close
  │
  ├─ Hardware suspected → Issue RMA authorization
  │     └─ Customer ships device back
  │
  ├─ Incoming inspection
  │     ├─ Visual inspection (physical damage, corrosion, burnt components)
  │     ├─ Re-run factory test sequence
  │     └─ Compare with original factory test log
  │
  ├─ Fault isolation
  │     ├─ Swap SoM → carrier issue
  │     ├─ Swap carrier → SoM issue
  │     └─ Neither → environmental or software root cause
  │
  └─ Resolution
        ├─ Repair and return
        ├─ Replace with new unit
        └─ Update design if systemic issue
```

**Failure tracking**

Track all failures in a database with categories:

| Category | Example | Action |
|----------|---------|--------|
| **DOA (Dead on Arrival)** | Unit never booted at customer site | Improve factory test coverage |
| **Infant mortality** | Fails within first 30 days | Possible solder defect — review reflow profile |
| **Wear-out** | Fan failure after 18 months | Spec higher-MTBF fan, or add fan monitoring |
| **Environmental** | Corrosion on exposed connector | Add conformal coating or sealed connector |
| **Software** | OTA bricked device | Improve rollback mechanism (Module 7) |

---

**14. Projects**

- **Production flash station:** Build a flash station that can flash 4 Jetson Orin Nano units in parallel using `--massflash`. Time the process and optimize.
- **Factory test script:** Write an automated test script that validates all peripherals on your custom carrier in under 60 seconds. Output a structured JSON pass/fail report.
- **Pre-compliance scan:** Using a near-field probe and spectrum analyzer (or SDR), scan your carrier board running a worst-case AI workload. Document the top 3 emission sources and proposed mitigations.
- **Fleet dashboard:** Deploy 3+ units with unique serial numbers, set up a Grafana dashboard showing device health, OTA version, and uptime. Simulate an OTA rollout to all devices.

---

**15. Resources**

| Resource | Description |
|----------|-------------|
| **FCC Equipment Authorization** | FCC.gov guide to Part 15 certification process |
| **CISPR 32 (EN 55032)** | International standard for multimedia equipment emissions |
| **EN 62368-1** | Safety standard for audio/video, IT, and communication equipment |
| **IPC-A-610** | Acceptability of electronic assemblies (workmanship standard) |
| **IPC-7711/7721** | Rework, modification, and repair of electronic assemblies |
| **NVIDIA Jetson Partner Hardware Design** | OEM Design Guide, reference design files, certification notes |
| [2. Custom Carrier Board Design](../02-定制载板设计与启动/Guide.md) | Hardware design (this module assumes carrier is built and validated) |
| [7. Security and OTA](../06-安全与OTA/Guide.md) | Signed images, fleet telemetry (feeds into factory flashing and fleet ops) |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/7. Compliance and Manufacturing/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/7.%20Compliance%20and%20Manufacturing/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
