---
title: 数字设计基础
description: 数字设计基础
---

# 数字设计基础

<div class="course-identity auto-course" style="--course-accent: #65a30d; --course-accent-rgb: 101, 163, 13;" markdown="1">
<div class="course-identity__icon">DDF</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入剖析 · 数字基础</p>
<p class="course-identity__title">数字设计基础的专属课程标识。</p>
<p class="course-identity__meta">产物：可运行的低层 demo · 度量：时序、内存、正确性</p>
</div>
</div>


每个 AI 加速器内部的电路——张量核心、SRAM bank、脉动阵列——都由本文所讲的同一批原语构成。本指南带你从 bit 一路走到存储层次，并刻意关注硬件设计中每天都会出现的那些表示方式与构建模块。

在深入技术构建模块之前，先把握整体图景：一个芯片构想如何变成一块真实的硅片。

---

## 0. 从设计到硅片——芯片是怎样制造出来的

### 0.1 Tapeout 流程

“Tapeout”是芯片设计团队把最终设计文件交给代工厂（TSMC、Samsung、Intel Foundry、GlobalFoundries）进行制造的那一刻。这个名字来自早年设计真的会被写到磁带上的年代。如今它意味着交付一组经过验证的 GDSII/OASIS 文件——即芯片上每个晶体管、wire 和通孔的几何描述。

从构想走向可工作硅片的完整旅程：

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        CHIP DESIGN FLOW                                 │
│                                                                         │
│  Specification                                                          │
│      │                                                                  │
│      ▼                                                                  │
│  Architecture Design          "What does the chip do?"                  │
│      │                        ISA, block diagram, memory map            │
│      ▼                                                                  │
│  RTL Design (Verilog/VHDL)    "Describe behavior in HDL"                │
│      │                        modules, FSMs, datapaths                  │
│      ▼                                                                  │
│  Functional Verification      "Does the RTL match the spec?"            │
│      │                        testbenches, UVM, formal verification     │
│      ▼                                                                  │
│  Logic Synthesis              "Convert RTL to gates"                    │
│      │                        tool: Synopsys Design Compiler            │
│      │                        input: RTL + standard cell library        │
│      │                        output: gate-level netlist                │
│      ▼                                                                  │
│  Place and Route (PnR)        "Put gates on chip, connect wires"        │
│      │                        tool: Cadence Innovus / Synopsys ICC2     │
│      │                        floorplan → placement → CTS → routing     │
│      ▼                                                                  │
│  Sign-off Checks              "Is it manufacturable and correct?"        │
│      │   ├── Timing (STA)     static timing analysis, all corners       │
│      │   ├── DRC              design rule check (min spacing, width)    │
│      │   ├── LVS              layout vs schematic (geometry = netlist?) │
│      │   ├── ERC              electrical rule check                     │
│      │   └── IR drop / EM     power integrity, electromigration         │
│      ▼                                                                  │
│  ★ TAPEOUT ★                  "Ship GDSII to foundry"                   │
│      │                                                                  │
│      ▼                                                                  │
│  Fabrication (TSMC etc.)      12–18 weeks for advanced nodes            │
│      │                                                                  │
│      ▼                                                                  │
│  Packaging & Test             die → package → burn-in → final test      │
│      │                                                                  │
│      ▼                                                                  │
│  Silicon Bring-up             first boot, characterization              │
└─────────────────────────────────────────────────────────────────────────┘
```


<details>
<summary>English original</summary>

**Digital Design Fundamentals**

<div class="course-identity auto-course" style="--course-accent: #65a30d; --course-accent-rgb: 101, 163, 13;" markdown="1">
<div class="course-identity__icon">DDF</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Digital Foundations</p>
<p class="course-identity__title">Specialized course identity for Digital Design Fundamentals.</p>
<p class="course-identity__meta">Artifact: working low-level demo · Measure: timing, memory, correctness</p>
</div>
</div>


The circuits inside every AI accelerator — tensor cores, SRAM banks, systolic arrays — are built from the same primitives covered here. This guide takes you from bits to memory hierarchies, with deliberate attention to the representations and building blocks that show up every day in hardware design.

Before diving into the technical building blocks, let's understand the big picture: how a chip idea becomes a physical piece of silicon.

---

**0. From Design to Silicon — How Chips Are Made**

**0.1 The Tapeout Flow**

"Tapeout" is the moment a chip design team hands off the final design files to a foundry (TSMC, Samsung, Intel Foundry, GlobalFoundries) for manufacturing. The name comes from the old days when designs were literally written to magnetic tape. Today it means delivering a set of verified GDSII/OASIS files — the geometric description of every transistor, wire, and via on the chip.

The full journey from idea to working silicon:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        CHIP DESIGN FLOW                                 │
│                                                                         │
│  Specification                                                          │
│      │                                                                  │
│      ▼                                                                  │
│  Architecture Design          "What does the chip do?"                  │
│      │                        ISA, block diagram, memory map            │
│      ▼                                                                  │
│  RTL Design (Verilog/VHDL)    "Describe behavior in HDL"                │
│      │                        modules, FSMs, datapaths                  │
│      ▼                                                                  │
│  Functional Verification      "Does the RTL match the spec?"            │
│      │                        testbenches, UVM, formal verification     │
│      ▼                                                                  │
│  Logic Synthesis              "Convert RTL to gates"                    │
│      │                        tool: Synopsys Design Compiler            │
│      │                        input: RTL + standard cell library        │
│      │                        output: gate-level netlist                │
│      ▼                                                                  │
│  Place and Route (PnR)        "Put gates on chip, connect wires"        │
│      │                        tool: Cadence Innovus / Synopsys ICC2     │
│      │                        floorplan → placement → CTS → routing     │
│      ▼                                                                  │
│  Sign-off Checks              "Is it manufacturable and correct?"        │
│      │   ├── Timing (STA)     static timing analysis, all corners       │
│      │   ├── DRC              design rule check (min spacing, width)    │
│      │   ├── LVS              layout vs schematic (geometry = netlist?) │
│      │   ├── ERC              electrical rule check                     │
│      │   └── IR drop / EM     power integrity, electromigration         │
│      ▼                                                                  │
│  ★ TAPEOUT ★                  "Ship GDSII to foundry"                   │
│      │                                                                  │
│      ▼                                                                  │
│  Fabrication (TSMC etc.)      12–18 weeks for advanced nodes            │
│      │                                                                  │
│      ▼                                                                  │
│  Packaging & Test             die → package → burn-in → final test      │
│      │                                                                  │
│      ▼                                                                  │
│  Silicon Bring-up             first boot, characterization              │
└─────────────────────────────────────────────────────────────────────────┘
```

</details>

### 0.2 从 RTL 到门级：逻辑综合

你用 Verilog 或 VHDL 编写硬件——对每个寄存器、MUX 和 ALU 的行为描述。综合工具（Synopsys Design Compiler 是行业标准）会将其映射为代工厂提供的 **标准单元库** 中的实际逻辑门。

```
Your RTL (Verilog):                     Synthesized gates:

  assign y = a & b | c;        →       NAND2 → INV → NOR2 → ...
                                        (mapped to TSMC N5 cell library)

  always @(posedge clk)
    if (en) q <= d;             →       DFF with enable (EDFCND2)
```

标准单元库包含数百个预先设计、预先表征的单元：

| 单元类型          | 示例                              |
|--------------------|---------------------------------------|
| 逻辑门        | INV, NAND2, NAND3, NOR2, AOI22, MUX2 |
| 时序单元         | DFF、带复位的 DFF、带使能的 DFF  |
| 缓冲器/驱动    | BUF, CLKBUF, CLKINV                  |
| 特殊            | Tie-high、tie-low、填充单元       |

每个单元都附带：
- **版图**（每个工艺层的几何图形）
- **时序模型**（.lib）——各种电压/温度 corner 下的 delay、setup、hold
- **功耗模型**——漏电功耗 + 开关功耗
- **寄生模型**——电容、电阻

综合工具会针对你的约束进行优化——通常是时序（目标时钟频率）、面积和功耗——并做出权衡，例如在关键路径上使用更快（更大）的单元，在其他位置使用更慢（更小）的单元。

### 0.3 布局布线

综合之后，你得到一个网表：约十亿个门及其连接的列表。布局布线会将其转化为物理版图。

```
Step 1: Floorplanning
  ┌──────────────────────────────────┐
  │  ┌──────┐  ┌────────┐  ┌──────┐ │
  │  │ SRAM │  │Compute │  │ SRAM │ │
  │  │Block │  │ Core   │  │Block │ │
  │  └──────┘  └────────┘  └──────┘ │
  │  ┌────────────────────────────┐  │
  │  │        I/O ring            │  │
  │  └────────────────────────────┘  │
  └──────────────────────────────────┘
  Decide where major blocks go, power grid, pin placement

Step 2: Placement
  Standard cells placed in rows between power rails (VDD/VSS)
  ┌─VDD─────────────────────────────┐
  │ [NAND][INV][DFF][BUF][MUX]...  │
  ├─VSS─────────────────────────────┤
  │ [NOR][AOI][DFF][INV][NAND]...  │
  ├─VDD─────────────────────────────┤
  │ ...                             │
  └─────────────────────────────────┘

Step 3: Clock Tree Synthesis (CTS)
  Build a balanced tree of clock buffers so CLK arrives
  at every flip-flop within ~50ps skew

Step 4: Routing
  Connect all signals using metal layers (M1–M15+ on advanced nodes)
  Lower metals: short local wires
  Upper metals: long global wires, power distribution
```


<details>
<summary>English original</summary>

**0.2 RTL to Gates: Logic Synthesis**

You write hardware in Verilog or VHDL — a behavioral description of what each register, MUX, and ALU does. The synthesis tool (Synopsys Design Compiler is the industry standard) maps this to actual logic gates from a **standard cell library** provided by the foundry.

```
Your RTL (Verilog):                     Synthesized gates:

  assign y = a & b | c;        →       NAND2 → INV → NOR2 → ...
                                        (mapped to TSMC N5 cell library)

  always @(posedge clk)
    if (en) q <= d;             →       DFF with enable (EDFCND2)
```

A standard cell library contains hundreds of pre-designed, pre-characterized cells:

| Cell type          | Examples                              |
|--------------------|---------------------------------------|
| Logic gates        | INV, NAND2, NAND3, NOR2, AOI22, MUX2 |
| Sequential         | DFF, DFF with reset, DFF with enable  |
| Buffers/drivers    | BUF, CLKBUF, CLKINV                  |
| Special            | Tie-high, tie-low, filler cells       |

Each cell comes with:
- **Layout** (geometric shapes for each process layer)
- **Timing model** (.lib) — delay, setup, hold at various voltage/temperature corners
- **Power model** — leakage + switching power
- **Parasitic model** — capacitance, resistance

The synthesis tool optimizes for your constraints — typically timing (target clock frequency), area, and power — making trade-offs like using faster (larger) cells on critical paths and slower (smaller) cells elsewhere.

**0.3 Place and Route**

After synthesis, you have a netlist: a list of ~billion gates and their connections. Place and route turns this into a physical layout.

```
Step 1: Floorplanning
  ┌──────────────────────────────────┐
  │  ┌──────┐  ┌────────┐  ┌──────┐ │
  │  │ SRAM │  │Compute │  │ SRAM │ │
  │  │Block │  │ Core   │  │Block │ │
  │  └──────┘  └────────┘  └──────┘ │
  │  ┌────────────────────────────┐  │
  │  │        I/O ring            │  │
  │  └────────────────────────────┘  │
  └──────────────────────────────────┘
  Decide where major blocks go, power grid, pin placement

Step 2: Placement
  Standard cells placed in rows between power rails (VDD/VSS)
  ┌─VDD─────────────────────────────┐
  │ [NAND][INV][DFF][BUF][MUX]...  │
  ├─VSS─────────────────────────────┤
  │ [NOR][AOI][DFF][INV][NAND]...  │
  ├─VDD─────────────────────────────┤
  │ ...                             │
  └─────────────────────────────────┘

Step 3: Clock Tree Synthesis (CTS)
  Build a balanced tree of clock buffers so CLK arrives
  at every flip-flop within ~50ps skew

Step 4: Routing
  Connect all signals using metal layers (M1–M15+ on advanced nodes)
  Lower metals: short local wires
  Upper metals: long global wires, power distribution
```

</details>

### 0.4 在 TSMC 制造

GDSII 文件到达晶圆厂后，要经过光刻——把设计逐层印制到硅晶圆上。

```
Wafer cross-section (simplified, TSMC N5):

  ══════════  Metal 15 (thick, global power/signal)
     ...      (13 more metal layers)
  ──────────  Metal 2
  ──────────  Metal 1 (thin, local connections)
  ──────────  Contact layer
  ▓▓▓▓▓▓▓▓▓  Gate (FinFET or GAA nanosheet at N3/N2)
  ░░░░░░░░░  Fin / channel
  ▒▒▒▒▒▒▒▒▒  Silicon substrate (300mm wafer)

Each layer requires:
  1. Deposit material (metal, oxide, etc.)
  2. Spin photoresist
  3. Expose with EUV light through reticle (mask)
  4. Develop (remove exposed/unexposed resist)
  5. Etch pattern into material
  6. Strip remaining resist
  7. Repeat for next layer (~80-100 mask steps for N5)
```

**工艺节点对比（截至 2025–2026）：**

| Node     | Foundry      | Transistor type  | Density (MTr/mm²) | Used in               |
|----------|-------------|------------------|--------------------|------------------------|
| N7       | TSMC        | FinFET           | ~91                | A100, AMD Zen 2        |
| N5       | TSMC        | FinFET           | ~171               | H100, Apple M2, Zen 4  |
| N4P      | TSMC        | FinFET           | ~180               | B100/B200              |
| N3E      | TSMC        | FinFET           | ~208               | Apple M4, Vera (2026)  |
| N2       | TSMC (2025) | GAA nanosheet    | ~250+              | 下一代 AI 芯片          |
| 20A/18A  | Intel       | RibbonFET (GAA)  | ~200+              | Intel 下一代            |
| 2nm      | Samsung     | GAA              | ~200+              | 晶圆厂客户              |

**FinFET vs GAA (Gate-All-Around)：**

```
FinFET (N7–N3):                     GAA Nanosheet (N2 and below):

      Gate                              Gate (wraps all sides)
     ┌───┐                            ┌─────────────┐
     │   │                            │ ┌─────────┐ │
     │ F │  ← gate wraps             │ │nanosheet│ │  ← gate wraps
     │ I │     3 sides                │ └─────────┘ │     ALL 4 sides
     │ N │                            │ ┌─────────┐ │
     │   │                            │ │nanosheet│ │  (stacked sheets)
     └─┬─┘                            │ └─────────┘ │
       │                              └──────┬──────┘
  ─────┴─────                         ───────┴───────
   substrate                            substrate

GAA advantage: better electrostatic control → less leakage,
               higher drive current, better scaling below 3nm
```

### 0.5 封装

裸 die 从晶圆上切割下来，放入封装中，封装提供：
- 电气连接（电源、地、I/O 信号）
- 机械保护
- 散热路径

```
Modern AI chip packaging (CoWoS — Chip on Wafer on Substrate):

        ┌─── Heat spreader / lid ───┐
        │                           │
   ┌────┴──────┬───────┬──────┬─────┴───┐
   │  HBM      │  GPU  │  HBM │  HBM    │  ← dies on interposer
   │  stack     │  die  │ stack│  stack   │
   └───┬────────┴───┬───┴──────┴────┬────┘
       │   Silicon interposer       │      ← passive routing layer
   ┌───┴────────────┴───────────────┴────┐
   │         Organic substrate           │  ← BGA ball grid below
   └─────────────────────────────────────┘
                  │││││
            solder balls → PCB

CoWoS enables:
  - GPU die + HBM stacks on same interposer (short, wide buses)
  - 1024-bit HBM interface (impossible with PCB traces)
  - H100: ~814mm² GPU die + 6× HBM3 stacks on ~2× larger interposer
```

**封装技术对比：**

| Package       | Description                              | Used in                    |
|---------------|------------------------------------------|----------------------------|
| Wire bond     | 从 die 焊盘到封装引脚的金线                | 低成本、legacy 芯片         |
| Flip-chip     | 焊球，die 面朝下                          | CPU、GPU（标准）            |
| 2.5D (CoWoS)  | 多个 die 置于硅 interposer 上             | AI GPU（H100、MI300X）      |
| 3D stacking   | die 通过 TSV 垂直堆叠                     | HBM、AMD 3D V-Cache         |
| Chiplets       | 一个封装内多个小 die                      | AMD EPYC、Intel Ponte Vecchio|


<details>
<summary>English original</summary>

**0.4 Fabrication at TSMC**

Once the GDSII file arrives at the foundry, it goes through photolithography — printing the design layer by layer onto a silicon wafer.

```
Wafer cross-section (simplified, TSMC N5):

  ══════════  Metal 15 (thick, global power/signal)
     ...      (13 more metal layers)
  ──────────  Metal 2
  ──────────  Metal 1 (thin, local connections)
  ──────────  Contact layer
  ▓▓▓▓▓▓▓▓▓  Gate (FinFET or GAA nanosheet at N3/N2)
  ░░░░░░░░░  Fin / channel
  ▒▒▒▒▒▒▒▒▒  Silicon substrate (300mm wafer)

Each layer requires:
  1. Deposit material (metal, oxide, etc.)
  2. Spin photoresist
  3. Expose with EUV light through reticle (mask)
  4. Develop (remove exposed/unexposed resist)
  5. Etch pattern into material
  6. Strip remaining resist
  7. Repeat for next layer (~80-100 mask steps for N5)
```

**Process node comparison (as of 2025–2026):**

| Node     | Foundry      | Transistor type  | Density (MTr/mm²) | Used in               |
|----------|-------------|------------------|--------------------|------------------------|
| N7       | TSMC        | FinFET           | ~91                | A100, AMD Zen 2        |
| N5       | TSMC        | FinFET           | ~171               | H100, Apple M2, Zen 4  |
| N4P      | TSMC        | FinFET           | ~180               | B100/B200              |
| N3E      | TSMC        | FinFET           | ~208               | Apple M4, Vera (2026)  |
| N2       | TSMC (2025) | GAA nanosheet    | ~250+              | Next-gen AI chips       |
| 20A/18A  | Intel       | RibbonFET (GAA)  | ~200+              | Intel next-gen          |
| 2nm      | Samsung     | GAA              | ~200+              | Foundry customers       |

**FinFET vs GAA (Gate-All-Around):**

```
FinFET (N7–N3):                     GAA Nanosheet (N2 and below):

      Gate                              Gate (wraps all sides)
     ┌───┐                            ┌─────────────┐
     │   │                            │ ┌─────────┐ │
     │ F │  ← gate wraps             │ │nanosheet│ │  ← gate wraps
     │ I │     3 sides                │ └─────────┘ │     ALL 4 sides
     │ N │                            │ ┌─────────┐ │
     │   │                            │ │nanosheet│ │  (stacked sheets)
     └─┬─┘                            │ └─────────┘ │
       │                              └──────┬──────┘
  ─────┴─────                         ───────┴───────
   substrate                            substrate

GAA advantage: better electrostatic control → less leakage,
               higher drive current, better scaling below 3nm
```

**0.5 Packaging**

The raw die is cut from the wafer and placed into a package that provides:
- Electrical connections (power, ground, I/O signals)
- Mechanical protection
- Heat dissipation path

```
Modern AI chip packaging (CoWoS — Chip on Wafer on Substrate):

        ┌─── Heat spreader / lid ───┐
        │                           │
   ┌────┴──────┬───────┬──────┬─────┴───┐
   │  HBM      │  GPU  │  HBM │  HBM    │  ← dies on interposer
   │  stack     │  die  │ stack│  stack   │
   └───┬────────┴───┬───┴──────┴────┬────┘
       │   Silicon interposer       │      ← passive routing layer
   ┌───┴────────────┴───────────────┴────┐
   │         Organic substrate           │  ← BGA ball grid below
   └─────────────────────────────────────┘
                  │││││
            solder balls → PCB

CoWoS enables:
  - GPU die + HBM stacks on same interposer (short, wide buses)
  - 1024-bit HBM interface (impossible with PCB traces)
  - H100: ~814mm² GPU die + 6× HBM3 stacks on ~2× larger interposer
```

**Packaging technologies comparison:**

| Package       | Description                              | Used in                    |
|---------------|------------------------------------------|----------------------------|
| Wire bond     | Gold wires from die pads to package leads| Low-cost, legacy chips      |
| Flip-chip     | Solder bumps, die face-down              | CPUs, GPUs (standard)       |
| 2.5D (CoWoS)  | Multiple dies on silicon interposer     | AI GPUs (H100, MI300X)      |
| 3D stacking   | Dies stacked vertically with TSVs       | HBM, AMD 3D V-Cache         |
| Chiplets       | Multiple small dies in one package      | AMD EPYC, Intel Ponte Vecchio|

</details>

### 0.6 测试与良率

晶圆上的每一颗 die 并非都能正常工作——来自颗粒的缺陷、光刻误差以及工艺偏差会杀死部分晶体管。

```
300mm wafer with ~100 GPU dies:

  ┌─────────────────────────────┐
  │   ○ ○ ○ ○ ○ ○ ○ ○ ○        │
  │  ○ ● ○ ○ ● ○ ○ ○ ○ ○      │    ○ = good die
  │ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○     │    ● = defective die
  │  ○ ○ ○ ● ○ ○ ○ ○ ○ ○      │
  │ ○ ○ ○ ○ ○ ○ ● ○ ○ ○ ○     │    Edge dies = partial/wasted
  │  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○      │
  │   ○ ○ ○ ○ ○ ○ ○ ○ ○       │    Yield = good dies / total dies
  └─────────────────────────────┘

  H100 die: ~814mm²  →  ~60 dies per 300mm wafer
  At 80% yield → ~48 good dies per wafer
  Wafer cost at N5: ~$16,000–$20,000
  Cost per good die: ~$330–$415 (before packaging, test, profit margin)
```

**缺陷容忍——分档与挽救：**

NVIDIA 通过禁用损坏的 SM 来挽救部分有缺陷的 die：
- H100 SXM：设计 132 个 SM，启用 114 个（禁用 18 个有缺陷的）
- 这大幅提升了有效良率

**测试流程：**

```
1. Wafer sort (probe test)    — test each die on wafer, mark bad dies
2. Die singulation            — cut wafer into individual dies
3. Packaging                  — bond good dies into packages
4. Final test (ATE)           — automated test at speed, burn-in
5. Binning                    — sort by max frequency / power tier
                                (e.g., RTX 4090 vs 4080 = same die, different bin)
```

### 0.7 时间线与成本

一颗现代 AI 芯片从架构到量产：

| Phase                    | Duration        | Key deliverable            |
|--------------------------|-----------------|----------------------------|
| Architecture & spec      | 3–6 months      | Microarchitecture document |
| RTL design               | 12–18 months    | Verilog/VHDL codebase      |
| Verification             | Overlaps RTL    | 60–70% of total effort     |
| Synthesis + PnR          | 3–6 months      | GDSII layout               |
| Tapeout → first silicon  | 3–4 months      | Engineering samples (ES)   |
| Validation & bring-up    | 3–6 months      | Working chips              |
| **Total**                | **~2–3 years**  | Production silicon         |

**在 N5 节点设计一颗芯片的成本：** $300M–$5 亿美元以上（包括 EDA 工具、IP 授权、掩模版、工程团队）。这就是为什么只有少数几家公司（NVIDIA、AMD、Apple、Google 等）设计最先进制程的芯片——从经济性上讲，需要巨大的出货量才能摊薄 NRE（Non-Recurring Engineering，一次性工程）成本。

> **这对 AI 硬件工程师为何重要：** 理解 tapeout 流程能让你明白设计决策*为何*如此做出。当架构师选择更简单的流水线而非更快但更复杂的方案时，往往是因为验证工作量随复杂度呈超线性增长。当 NVIDIA 用同一颗 die 覆盖多个 GPU 档位（分档）时，那是良率经济学。当 HBM 放在 interposer 上而非封装内 DRAM 时，那是因为 1024-bit 总线用 PCB 布线在物理上不可能实现。本指南的每一节——从布尔代数到 MIPS 流水线——都是这一流程中的一块积木。

---

## 1. 数制

### 1.1 二进制、十六进制与位置记数法

数字系统中的每个数都是比特的模式。位置记数法让每个比特的值取决于其位置：

```
Decimal 173  =  1×10² + 7×10¹ + 3×10⁰
Binary 10101101  =  1×2⁷ + 0×2⁶ + 1×2⁵ + 0×2⁴ + 1×2³ + 1×2² + 0×2¹ + 1×2⁰
               =  128 + 32 + 8 + 4 + 1  =  173
Hex 0xAD       =  10×16¹ + 13×16⁰  =  160 + 13  =  173
```

硬件中到处使用十六进制，因为一个十六进制数字正好对应四位比特——更易读，更易输入。

```
Binary group:  1010  1101
Hex digit:      A     D      →  0xAD
```

**转换练习**（手算完成这些，直到形成本能反应）：

| Decimal | Binary     | Hex  |
|---------|-----------|------|
| 0       | 0000 0000 | 0x00 |
| 127     | 0111 1111 | 0x7F |
| 128     | 1000 0000 | 0x80 |
| 255     | 1111 1111 | 0xFF |
| 256     | 0001 0000 0000 | 0x100 |


<details>
<summary>English original</summary>

**0.6 Testing and Yield**

Not every die on a wafer works — defects from particles, lithography errors, and process variation kill some transistors.

```
300mm wafer with ~100 GPU dies:

  ┌─────────────────────────────┐
  │   ○ ○ ○ ○ ○ ○ ○ ○ ○        │
  │  ○ ● ○ ○ ● ○ ○ ○ ○ ○      │    ○ = good die
  │ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○     │    ● = defective die
  │  ○ ○ ○ ● ○ ○ ○ ○ ○ ○      │
  │ ○ ○ ○ ○ ○ ○ ● ○ ○ ○ ○     │    Edge dies = partial/wasted
  │  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○      │
  │   ○ ○ ○ ○ ○ ○ ○ ○ ○       │    Yield = good dies / total dies
  └─────────────────────────────┘

  H100 die: ~814mm²  →  ~60 dies per 300mm wafer
  At 80% yield → ~48 good dies per wafer
  Wafer cost at N5: ~$16,000–$20,000
  Cost per good die: ~$330–$415 (before packaging, test, profit margin)
```

**Defect tolerance — binning and salvage:**

NVIDIA harvests partially defective dies by disabling broken SMs:
- H100 SXM: 132 SMs designed, 114 enabled (disable 18 defective ones)
- This dramatically improves effective yield

**Test flow:**

```
1. Wafer sort (probe test)    — test each die on wafer, mark bad dies
2. Die singulation            — cut wafer into individual dies
3. Packaging                  — bond good dies into packages
4. Final test (ATE)           — automated test at speed, burn-in
5. Binning                    — sort by max frequency / power tier
                                (e.g., RTX 4090 vs 4080 = same die, different bin)
```

**0.7 Timeline and Cost**

A modern AI chip from architecture to production:

| Phase                    | Duration        | Key deliverable            |
|--------------------------|-----------------|----------------------------|
| Architecture & spec      | 3–6 months      | Microarchitecture document |
| RTL design               | 12–18 months    | Verilog/VHDL codebase      |
| Verification             | Overlaps RTL    | 60–70% of total effort     |
| Synthesis + PnR          | 3–6 months      | GDSII layout               |
| Tapeout → first silicon  | 3–4 months      | Engineering samples (ES)   |
| Validation & bring-up    | 3–6 months      | Working chips              |
| **Total**                | **~2–3 years**  | Production silicon         |

**Cost to design a chip at N5:** $300M–$500M+ (including EDA tools, IP licenses, masks, engineering team). This is why only a handful of companies (NVIDIA, AMD, Apple, Google, etc.) design leading-edge chips — the economics require massive volume to amortize the NRE (Non-Recurring Engineering) cost.

> **Why this matters for AI hardware engineers:** understanding the tapeout flow tells you *why* design decisions are made the way they are. When an architect chooses a simpler pipeline over a faster but more complex one, it's often because verification effort scales superlinearly with complexity. When NVIDIA uses the same die for multiple GPU tiers (binning), it's yield economics. When HBM is on an interposer instead of on-package DRAM, it's because the 1024-bit bus is physically impossible with PCB routing. Every section in this guide — from Boolean algebra through MIPS pipelines — is a building block in this flow.

---

**1. Number Systems**

**1.1 Binary, Hexadecimal, and Positional Notation**

Every number in a digital system is a pattern of bits. Positional notation makes the value of each bit depend on its position:

```
Decimal 173  =  1×10² + 7×10¹ + 3×10⁰
Binary 10101101  =  1×2⁷ + 0×2⁶ + 1×2⁵ + 0×2⁴ + 1×2³ + 1×2² + 0×2¹ + 1×2⁰
               =  128 + 32 + 8 + 4 + 1  =  173
Hex 0xAD       =  10×16¹ + 13×16⁰  =  160 + 13  =  173
```

Hex is used everywhere in hardware because one hex digit maps exactly to four bits — easier to read, easier to type.

```
Binary group:  1010  1101
Hex digit:      A     D      →  0xAD
```

**Conversion drill** (do these by hand until they feel automatic):

| Decimal | Binary     | Hex  |
|---------|-----------|------|
| 0       | 0000 0000 | 0x00 |
| 127     | 0111 1111 | 0x7F |
| 128     | 1000 0000 | 0x80 |
| 255     | 1111 1111 | 0xFF |
| 256     | 0001 0000 0000 | 0x100 |

</details>

### 1.2 有符号整数：二进制补码

硬件上曾尝试过三种表示负数的方法。只有一种存活下来。

| 方案          | +5       | −5       | 问题                          |
|-----------------|----------|----------|----------------------------------|
| 原码  | 0000 0101 | 1000 0101 | 两个零（+0、−0）；加法器复杂 |
| 反码 | 0000 0101 | 1111 1010 | 两个零；循环进位      |
| **二进制补码** | **0000 0101** | **1111 1011** | 一个零；加/减共用同一加法器 |

**二进制补码规则：** 所有位取反，再加 1。

```
+5  =  0000 0101
flip   1111 1010
+1     1111 1011   ← this is −5 in 8-bit two's complement
```

**用加法验证** —— 正确的取负结果相加得零：

```
  0000 0101   (+5)
+ 1111 1011   (−5)
-----------
  0000 0000   (0, carry out discarded)  ✓
```

N 位二进制补码的**范围**：−2^(N−1) 到 +2^(N−1) − 1。
8 位：−128 到 +127。32 位：−2,147,483,648 到 +2,147,483,647。

**符号扩展** —— 加宽数值时（例如 INT8 → INT32），把符号位复制到所有新增的高位：

```
INT8  −5  =  1111 1011
INT32 −5  =  1111 1111 1111 1111 1111 1111 1111 1011
                ↑ all new bits filled with sign bit (1)
```

> **AI 硬件关联：** 矩阵乘累加单元（MMA/WMMA）以 INT8 运算，用 INT32 累加器。INT8 到 INT32 的符号扩展在硬件中是自动的，但在混用有符号/无符号操作数时就有影响。

### 1.3 IEEE 754 浮点数

浮点数用整数固定的范围，换取一个滑动的精度窗口。

```
Value = (−1)^S  ×  1.Mantissa  ×  2^(Exponent − Bias)
```

| 格式    | 位宽 | 符号 | 指数 | 尾数 | 偏置 | ~范围          | ~精度 |
|-----------|-------|------|----------|----------|------|-----------------|------------|
| FP32      | 32b   | 1    | 8        | 23       | 127  | ±3.4×10^38      | ~7 位十进制数字  |
| FP64      | 64b   | 1    | 11       | 52       | 1023 | ±1.8×10^308     | ~15 位十进制数字 |
| FP16      | 16b   | 1    | 5        | 10       | 15   | ±65504          | ~3 位十进制数字  |
| BF16      | 16b   | 1    | 8        | 7        | 127  | 与 FP32 相同    | ~2 位十进制数字  |
| FP8 E4M3  | 8b    | 1    | 4        | 3        | 7    | ±448            | ~1 位十进制数字   |
| FP8 E5M2  | 8b    | 1    | 5        | 2        | 15   | ±57344          | ~0.5 位十进制数字 |

**FP32 示例 —— 表示 0.15625：**

```
0.15625 = 0.00101 in binary = 1.01 × 2^(−3)

S = 0
Exponent field = −3 + 127 = 124 = 0111 1100
Mantissa = 01 followed by 21 zeros

Bit pattern: 0  01111100  01000000000000000000000
             S  exponent  mantissa
```

**特殊值**（每个硬件工程师都必须知道）：

| 模式                  | 含义   |
|--------------------------|-----------|
| Exp=0, Mantissa=0        | ±零     |
| Exp=0, Mantissa≠0        | 非规格化数 |
| Exp=255, Mantissa=0      | ±无穷大 |
| Exp=255, Mantissa≠0      | NaN       |

**舍入模式**（标准规定四种；默认是就近舍入到偶数）：

```
Exact result  →  nearest representable value
Ties (exactly halfway)  →  choose the one with even last bit
```

舍入模式在 AI 中很重要，因为数百万次运算累积的舍入误差会导致训练不稳定。随机舍入（按接近程度加权，随机向上或向下舍入）用于某些 FP8 训练流程。

### 1.4 AI 浮点格式：FP16、BF16、FP8

现代 AI 训练和推理使用降低精度的格式，以便在 SRAM 中放下更多数据，并降低乘加能耗。

```
FP32   [S|EEEEEEEE|MMMMMMMMMMMMMMMMMMMMMMM]   32 bits
BF16   [S|EEEEEEEE|MMMMMMM]                   16 bits  (truncated FP32 mantissa)
FP16   [S|EEEEE|MMMMMMMMMM]                   16 bits  (different exponent width)
FP8    [S|EEEE|MMM]  or  [S|EEEEE|MM]          8 bits
```

**BF16 与 FP16 的权衡：**

| 属性         | FP16                  | BF16                        |
|------------------|-----------------------|-----------------------------|
| 指数位    | 5                     | 8（与 FP32 相同）            |
| 范围            | ±65504                | ±3.4×10^38                  |
| 精度        | ~3 位十进制数字     | ~2 位十进制数字           |
| FP32 转换  | 需要范围钳位     | 截断 16 位尾数   |
| 用途            | 推理、部分训练 | 混合精度训练 |

**FP8 —— 两个变体，承担不同角色：**

- **E4M3**（4 位指数、3 位尾数）：精度更高、范围更小 → 权重和激活值
- **E5M2**（5 位指数、2 位尾数）：范围更大、精度更低 → 梯度

FP8 训练需要损失缩放并小心处理溢出 —— 不做缩放的话，范围对原始梯度来说太小。


<details>
<summary>English original</summary>

**1.2 Signed Integers: Two's Complement**

Three ways to represent negative numbers have been tried in hardware. Only one survived.

| Scheme          | +5       | −5       | Problem                          |
|-----------------|----------|----------|----------------------------------|
| Sign-magnitude  | 0000 0101 | 1000 0101 | Two zeros (+0, −0); complex adder |
| One's complement | 0000 0101 | 1111 1010 | Two zeros; end-around carry      |
| **Two's complement** | **0000 0101** | **1111 1011** | One zero; same adder for add/sub |

**Two's complement rule:** flip all bits, add 1.

```
+5  =  0000 0101
flip   1111 1010
+1     1111 1011   ← this is −5 in 8-bit two's complement
```

**Verify by addition** — a correct negation adds to zero:

```
  0000 0101   (+5)
+ 1111 1011   (−5)
-----------
  0000 0000   (0, carry out discarded)  ✓
```

**Range** for N-bit two's complement: −2^(N−1) to +2^(N−1) − 1.
For 8-bit: −128 to +127. For 32-bit: −2,147,483,648 to +2,147,483,647.

**Sign extension** — when widening a value (e.g., INT8 → INT32), copy the sign bit into all new high bits:

```
INT8  −5  =  1111 1011
INT32 −5  =  1111 1111 1111 1111 1111 1111 1111 1011
                ↑ all new bits filled with sign bit (1)
```

> **AI hardware connection:** matrix multiply accumulate units (MMA/WMMA) operate on INT8 with INT32 accumulators. The sign extension from INT8 to INT32 is automatic in hardware, but matters when you mix signed/unsigned operands.

**1.3 IEEE 754 Floating-Point**

Floating-point trades the fixed range of integers for a sliding window of precision.

```
Value = (−1)^S  ×  1.Mantissa  ×  2^(Exponent − Bias)
```

| Format    | Width | Sign | Exponent | Mantissa | Bias | ~Range          | ~Precision |
|-----------|-------|------|----------|----------|------|-----------------|------------|
| FP32      | 32b   | 1    | 8        | 23       | 127  | ±3.4×10^38      | ~7 digits  |
| FP64      | 64b   | 1    | 11       | 52       | 1023 | ±1.8×10^308     | ~15 digits |
| FP16      | 16b   | 1    | 5        | 10       | 15   | ±65504          | ~3 digits  |
| BF16      | 16b   | 1    | 8        | 7        | 127  | same as FP32    | ~2 digits  |
| FP8 E4M3  | 8b    | 1    | 4        | 3        | 7    | ±448            | ~1 digit   |
| FP8 E5M2  | 8b    | 1    | 5        | 2        | 15   | ±57344          | ~0.5 digit |

**FP32 example — representing 0.15625:**

```
0.15625 = 0.00101 in binary = 1.01 × 2^(−3)

S = 0
Exponent field = −3 + 127 = 124 = 0111 1100
Mantissa = 01 followed by 21 zeros

Bit pattern: 0  01111100  01000000000000000000000
             S  exponent  mantissa
```

**Special values** (every hardware engineer must know these):

| Pattern                  | Meaning   |
|--------------------------|-----------|
| Exp=0, Mantissa=0        | ±Zero     |
| Exp=0, Mantissa≠0        | Subnormal |
| Exp=255, Mantissa=0      | ±Infinity |
| Exp=255, Mantissa≠0      | NaN       |

**Rounding modes** (four are standard; round-to-nearest-even is default):

```
Exact result  →  nearest representable value
Ties (exactly halfway)  →  choose the one with even last bit
```

Rounding modes matter in AI because accumulated rounding error over millions of operations can cause training instability. Stochastic rounding (rounding up or down randomly, weighted by proximity) is used in some FP8 training flows.

**1.4 AI Float Formats: FP16, BF16, FP8**

Modern AI training and inference use reduced-precision formats to fit more data in SRAM and reduce multiply-add energy.

```
FP32   [S|EEEEEEEE|MMMMMMMMMMMMMMMMMMMMMMM]   32 bits
BF16   [S|EEEEEEEE|MMMMMMM]                   16 bits  (truncated FP32 mantissa)
FP16   [S|EEEEE|MMMMMMMMMM]                   16 bits  (different exponent width)
FP8    [S|EEEE|MMM]  or  [S|EEEEE|MM]          8 bits
```

**BF16 vs FP16 trade-off:**

| Property         | FP16                  | BF16                        |
|------------------|-----------------------|-----------------------------|
| Exponent bits    | 5                     | 8 (same as FP32)            |
| Range            | ±65504                | ±3.4×10^38                  |
| Precision        | ~3 decimal digits     | ~2 decimal digits           |
| FP32 conversion  | Needs range clamp     | Truncate 16 mantissa bits   |
| Usage            | Inference, some training | Mixed-precision training |

**FP8 — two variants for different roles:**

- **E4M3** (4-bit exp, 3-bit mantissa): more precision, less range → weights and activations
- **E5M2** (5-bit exp, 2-bit mantissa): more range, less precision → gradients

FP8 training requires loss scaling and careful handling of overflow — the range is too small for raw gradients without scaling.

</details>

### 1.5 定点与量化

定点表示 Q = integer × 2^(−f)，其中 f 为小数位数。

```
Q8.8 format  (8 integer bits, 8 fractional bits, 16 bits total)

Value 3.75:
  Integer part:  3  = 0000 0011
  Fraction part: 0.75 = 0.11 in binary → stored as 1100 0000

Stored: 0000 0011  1100 0000  = 0x03C0
```

**量化**将浮点权重映射为 INT8，以实现高效推理：

```
INT8 = round(FP32_weight / scale)
scale = max(|weights|) / 127          ← per-tensor
scale = max(|row|) / 127              ← per-row (better quality)

Dequantize: FP32 ≈ INT8 × scale
```

乘累加（MAC）仍在 INT32 中执行（INT8 × INT8 → INT32 累加），随后结果被缩放回去。这正是 CUDA 的 Tensor Core IMMA 指令所实现的内容。

### 1.6 错误检测与纠正

**奇偶校验**——增加一个比特，使 1 的总数为偶数（偶校验）或奇数（奇校验）：

```
Data: 1011 0110   →  five 1s  →  odd
Even parity bit: 1  (makes six 1s → even)
Transmitted: 1011 0110 1
```

奇偶校验可检测任意单比特错误，但无法定位或纠正它。

**Hamming(7,4)**——将 4 个数据比特编码为 7 个比特，其中 3 个校验比特位于 2 的幂次位置上：

```
Positions:  p1  p2  d1  p4  d2  d3  d4
            1   2   3   4   5   6   7

p1 covers positions 1,3,5,7  (odd positions)
p2 covers positions 2,3,6,7
p4 covers positions 4,5,6,7

Syndrome = (check p1)(check p2)(check p4) → 3-bit number = position of error
```

Hamming(7,4) 可纠正任意单比特错误，并检测任意双比特错误。扩展 Hamming 增加第四个校验比特，用于检测（而非纠正）双比特错误。

**CRC（循环冗余校验，Cyclic Redundancy Check）**——将数据视为多项式，并除以生成多项式：

```
Data polynomial D(x) × x^r  mod  G(x)  =  R(x)   (remainder = CRC)
Transmitted: D(x) concat R(x)
Receiver: divide received polynomial by G(x) — nonzero remainder means error
```

CRC-32（用于 Ethernet、ZIP、PNG）使用 G(x) = x^32 + x^26 + x^23 + ... + 1。
ECC DRAM 在 64 位数据总线加 8 个 ECC 比特上使用基于 Hamming 的 SECDED（单错纠正、双错检测）码。

---

## 2. 布尔代数与逻辑门

### 2.1 门与真值表

每个数字函数都可归约为 NAND 门（或 NOR 门）——它们在功能上是完备的。

```
AND gate:          OR gate:          NOT gate:         NAND gate:
  A──┐              A──┐              A──○──Y           A──┐
     ├──Y              ├──Y                             ├──○──Y
  B──┘              B──┘                             B──┘

Truth tables:

A B | AND  OR  NAND  NOR  XOR  XNOR
0 0 |  0   0    1    1    0    1
0 1 |  0   1    1    0    1    0
1 0 |  0   1    1    0    1    0
1 1 |  1   1    0    0    0    1
```

**通用门——NAND 可实现一切：**

```
NOT A    =  NAND(A, A)
A AND B  =  NAND(NAND(A,B), NAND(A,B))   i.e. NOT(NAND(A,B))
A OR B   =  NAND(NAND(A,A), NAND(B,B))   i.e. NOT(NOT A) NAND NOT(NOT B)
```

### 2.2 布尔定律

| 定律            | AND 形式                | OR 形式                 |
|-----------------|-------------------------|-------------------------|
| 恒等律         | A · 1 = A               | A + 0 = A               |
| 零律            | A · 0 = 0               | A + 1 = 1               |
| 幂等律          | A · A = A               | A + A = A               |
| 互补律          | A · Ā = 0               | A + Ā = 1               |
| 双重否定        | ¬(¬A) = A               |                         |
| 交换律          | A · B = B · A           | A + B = B + A           |
| 结合律          | (A·B)·C = A·(B·C)       | (A+B)+C = A+(B+C)       |
| 分配律          | A·(B+C) = A·B + A·C     | A+(B·C) = (A+B)·(A+C)   |
| **德摩根定律**  | ¬(A·B) = Ā + B̄          | ¬(A+B) = Ā · B̄          |
| 吸收律          | A·(A+B) = A             | A + A·B = A             |

**德摩根定律**是逻辑设计中使用最多的定律——它可以将取反推过门，并在 AND↔OR 之间转换。

### 2.3 卡诺图

卡诺图以可视化方式对最小项分组，以求出最小积之和（SOP）表达式。

**4 变量卡诺图布局**（格雷码顺序——相邻单元仅差一个比特）：

```
        CD
AB    00  01  11  10
  00 | 0 | 1 | 3 | 2 |
  01 | 4 | 5 | 7 | 6 |
  11 |12 |13 |15 |14 |
  10 | 8 | 9 |11 |10 |
```

**分组规则：**
- 组大小必须为 2 的幂：1、2、4、8、16
- 组必须为矩形（可环绕边缘）
- 使用尽可能大的组
- 覆盖每一个 1；不要覆盖 0
- 每个组消去组内发生变化的变量

**示例**——化简 F(A,B,C,D) = Σm(0,2,4,5,6,7,8,10,13,15)：

```
        CD
AB    00  01  11  10
  00 | 1 | 0 | 0 | 1 |   →  Group of 4: cells 0,2,8,10 → B̄D̄
  01 | 1 | 1 | 1 | 1 |   →  Group of 4: cells 4,5,6,7 → AB̄
  11 | 0 | 1 | 1 | 0 |   →  Group of 2: cells 13,15 → ACD
  10 | 1 | 0 | 0 | 1 |

F = B̄D̄ + AB̄ + ACD
```

不用卡诺图：10 个最小项 = 最多 10 个四文字项。卡诺图：3 个项。


<details>
<summary>English original</summary>

**1.5 Fixed-Point and Quantization**

Fixed-point represents Q = integer × 2^(−f) where f is the number of fractional bits.

```
Q8.8 format  (8 integer bits, 8 fractional bits, 16 bits total)

Value 3.75:
  Integer part:  3  = 0000 0011
  Fraction part: 0.75 = 0.11 in binary → stored as 1100 0000

Stored: 0000 0011  1100 0000  = 0x03C0
```

**Quantization** maps floating-point weights to INT8 for efficient inference:

```
INT8 = round(FP32_weight / scale)
scale = max(|weights|) / 127          ← per-tensor
scale = max(|row|) / 127              ← per-row (better quality)

Dequantize: FP32 ≈ INT8 × scale
```

The multiply-accumulate (MAC) still runs in INT32 (INT8 × INT8 → INT32 accumulate), then the result is scaled back. This is exactly what CUDA's Tensor Core IMMA instructions implement.

**1.6 Error Detection and Correction**

**Parity** — add one bit so the total number of 1s is even (even parity) or odd (odd parity):

```
Data: 1011 0110   →  five 1s  →  odd
Even parity bit: 1  (makes six 1s → even)
Transmitted: 1011 0110 1
```

Parity detects any single-bit error but cannot locate or correct it.

**Hamming(7,4)** — encode 4 data bits into 7 bits with 3 parity bits positioned at powers of 2:

```
Positions:  p1  p2  d1  p4  d2  d3  d4
            1   2   3   4   5   6   7

p1 covers positions 1,3,5,7  (odd positions)
p2 covers positions 2,3,6,7
p4 covers positions 4,5,6,7

Syndrome = (check p1)(check p2)(check p4) → 3-bit number = position of error
```

Hamming(7,4) corrects any single-bit error and detects any double-bit error. Extended Hamming adds a fourth parity bit to detect (not correct) double errors.

**CRC (Cyclic Redundancy Check)** — treats the data as a polynomial and divides by a generator polynomial:

```
Data polynomial D(x) × x^r  mod  G(x)  =  R(x)   (remainder = CRC)
Transmitted: D(x) concat R(x)
Receiver: divide received polynomial by G(x) — nonzero remainder means error
```

CRC-32 (used in Ethernet, ZIP, PNG) uses G(x) = x^32 + x^26 + x^23 + ... + 1.
ECC DRAM uses a Hamming-based SECDED (Single Error Correct, Double Error Detect) code across the 64-bit data bus plus 8 ECC bits.

---

**2. Boolean Algebra and Logic Gates**

**2.1 Gates and Truth Tables**

Every digital function reduces to NAND gates (or NOR gates) — they are functionally complete.

```
AND gate:          OR gate:          NOT gate:         NAND gate:
  A──┐              A──┐              A──○──Y           A──┐
     ├──Y              ├──Y                             ├──○──Y
  B──┘              B──┘                             B──┘

Truth tables:

A B | AND  OR  NAND  NOR  XOR  XNOR
0 0 |  0   0    1    1    0    1
0 1 |  0   1    1    0    1    0
1 0 |  0   1    1    0    1    0
1 1 |  1   1    0    0    0    1
```

**Universal gates — NAND implements everything:**

```
NOT A    =  NAND(A, A)
A AND B  =  NAND(NAND(A,B), NAND(A,B))   i.e. NOT(NAND(A,B))
A OR B   =  NAND(NAND(A,A), NAND(B,B))   i.e. NOT(NOT A) NAND NOT(NOT B)
```

**2.2 Boolean Laws**

| Law             | AND form                | OR form                 |
|-----------------|-------------------------|-------------------------|
| Identity        | A · 1 = A               | A + 0 = A               |
| Null            | A · 0 = 0               | A + 1 = 1               |
| Idempotent      | A · A = A               | A + A = A               |
| Complement      | A · Ā = 0               | A + Ā = 1               |
| Double negative | ¬(¬A) = A               |                         |
| Commutative     | A · B = B · A           | A + B = B + A           |
| Associative     | (A·B)·C = A·(B·C)       | (A+B)+C = A+(B+C)       |
| Distributive    | A·(B+C) = A·B + A·C     | A+(B·C) = (A+B)·(A+C)  |
| **De Morgan's** | ¬(A·B) = Ā + B̄          | ¬(A+B) = Ā · B̄          |
| Absorption      | A·(A+B) = A             | A + A·B = A             |

**De Morgan's** is the most used law in logic design — it lets you push negations through gates and convert AND↔OR.

**2.3 Karnaugh Maps**

K-maps group minterms visually to find the minimal sum-of-products (SOP) expression.

**4-variable K-map layout** (Gray code order — adjacent cells differ by one bit):

```
        CD
AB    00  01  11  10
  00 | 0 | 1 | 3 | 2 |
  01 | 4 | 5 | 7 | 6 |
  11 |12 |13 |15 |14 |
  10 | 8 | 9 |11 |10 |
```

**Rules for grouping:**
- Group size must be a power of 2: 1, 2, 4, 8, 16
- Groups must be rectangular (wrap around edges)
- Use the largest possible groups
- Cover every 1; don't cover 0s
- Each group eliminates the variables that change within it

**Example** — minimize F(A,B,C,D) = Σm(0,2,4,5,6,7,8,10,13,15):

```
        CD
AB    00  01  11  10
  00 | 1 | 0 | 0 | 1 |   →  Group of 4: cells 0,2,8,10 → B̄D̄
  01 | 1 | 1 | 1 | 1 |   →  Group of 4: cells 4,5,6,7 → AB̄
  11 | 0 | 1 | 1 | 0 |   →  Group of 2: cells 13,15 → ACD
  10 | 1 | 0 | 0 | 1 |

F = B̄D̄ + AB̄ + ACD
```

Without K-map: 10 minterms = up to 10 four-literal terms. K-map: 3 terms.

</details>

### 2.4 CMOS 实现

CMOS（Complementary MOS）使用 PMOS 上拉网络与 NMOS 下拉网络。这两个网络互为对偶。

```
CMOS NAND gate (2-input):

        VDD
         |
    ─────┤  PMOS (A)     ← parallel pull-up
    │    └──┐
    └──────┤  PMOS (B)
            └─── Y
            ┌──
    ┌──────┤  NMOS (A)   ← series pull-down
    │    ┌──┘
    ─────┤  NMOS (B)
         |
        GND

Logic:  Y = NAND(A,B) = ¬(A · B)

If A=1 AND B=1:  both NMOS on (pull Y low), both PMOS off → Y = 0  ✓
If A=0 OR  B=0:  one PMOS on (pull Y high), NMOS chain broken → Y = 1  ✓
```

**CMOS 的关键特性：**

| 特性                 | 取值                                         |
|----------------------|----------------------------------------------|
| 静态功耗             | 接近零（仅漏电）                             |
| 动态功耗             | C × V² × f（随开关活动变化）                 |
| 噪声容限             | VDD 的 ~40%                                  |
| 扇出                 | 理论上无限（MOS 栅极是容性的）               |
| 反相                 | 每个 CMOS 门都反相（NAND、NOR、NOT）         |

> **为什么只有 NAND/NOR？** AND = NAND + NOT = 两级门。多一次反相要付出延迟与面积。现代综合工具内部一律以 NAND/NOR/NOT 处理。

---

## 3. 组合逻辑

### 3.1 多路选择器与译码器

**MUX（2:1）** — 依据控制信号从两路数据输入中选一路：

```
  D0 ──┐
        ├── Y   Y = S̄·D0 + S·D1
  D1 ──┘
        ↑
        S (select)

  S=0 → Y=D0
  S=1 → Y=D1
```

由两层 2:1 MUX 构成的 **4:1 MUX**：

```
  D0 ─┐                  S1=0: top 2:1 → D0 or D1
  D1 ─┤ 2:1 MUX ─┐       S1=1: bot 2:1 → D2 or D3
       └──(S0)    │
  D2 ─┐           ├─ 2:1 MUX ── Y
  D3 ─┤ 2:1 MUX ─┘         └──(S1)
       └──(S0)
```

**译码器（2 到 4）** — 恰好激活 N 个输出中的一个：

```
Inputs: A1, A0
Outputs: Y0..Y3

Y0 = Ā1·Ā0   (active when A=00)
Y1 = Ā1·A0   (active when A=01)
Y2 = A1·Ā0   (active when A=10)
Y3 = A1·A0   (active when A=11)
```

译码器在每个 SRAM 和寄存器堆中都作为行/列地址译码器出现。

### 3.2 加法器：行波进位 vs 超前进位

**全加器（1 位）：**

```
  A ──┐
  B ──┼── XOR ──────── Sum  =  A ⊕ B ⊕ Cin
  Cin─┘

  Carry-out  =  A·B  +  Cin·(A⊕B)
             =  A·B  +  Cin·(A XOR B)
```

**行波进位加法器（RCA）** — 级联 N 个全加器：

```
  A3 B3    A2 B2    A1 B1    A0 B0
   │  │     │  │     │  │     │  │
  ┌┴──┴┐   ┌┴──┴┐   ┌┴──┴┐   ┌┴──┴┐
  │ FA │◄──│ FA │◄──│ FA │◄──│ FA │◄── Cin=0
  └────┘   └────┘   └────┘   └────┘
  Cout S3     S2       S1       S0

Delay: N × t_FA   (linear in word width)
32-bit: ~32 × 0.5ns = 16ns at 1 GHz  (too slow for the critical path)
```

**超前进位加法器（CLA）** — 并行计算所有进位：

```
Generate: Gi = Ai · Bi        (this bit produces a carry regardless of Cin)
Propagate: Pi = Ai ⊕ Bi      (this bit passes Cin to Cout)

C1 = G0 + P0·C0
C2 = G1 + P1·G0 + P1·P0·C0
C3 = G2 + P2·G1 + P2·P1·G0 + P2·P1·P0·C0
C4 = G3 + P3·G2 + P3·P2·G1 + P3·P2·P1·G0 + P3·P2·P1·P0·C0

All four carries computed in 2 gate delays (one AND, one OR).
```

**延迟对比：**

| 加法器类型          | 延迟（N 位）    | 面积      |
|---------------------|-----------------|-----------|
| 行波进位            | O(N)            | O(N)      |
| 超前进位            | O(log N)        | O(N log N)|
| 进位选择            | O(√N)           | O(N)      |
| Kogge-Stone（树形） | O(log N)        | O(N log N)|

现代 ALU 使用 Kogge-Stone 或 Han-Carlson 前缀树，以规则、可流水化的结构实现 O(log N) 延迟。

### 3.3 乘法器与 MAC 单元

**阵列乘法器（4×4 无符号）：**

```
           A3  A2  A1  A0
         ×  B3  B2  B1  B0
         ─────────────────
         A3B0 A2B0 A1B0 A0B0     ← partial product row 0 (AND gates)
    A3B1 A2B1 A1B1 A0B1          ← partial product row 1 (shift 1)
A3B2 A2B2 A1B2 A0B2              ← partial product row 2 (shift 2)
A3B3 A2B3 A1B3 A0B3              ← partial product row 3 (shift 3)
─────────────────────────────────
P7   P6   P5   P4   P3  P2  P1  P0

Partial products are summed with an adder tree (Wallace tree reduces rows in O(log N) delay).
```

**MAC（乘累加）：**

```
ACC ← ACC + (A × B)

  A ─┐
     ├── MULTIPLIER ─┐
  B ─┘               ├── ADDER ── ACC ──┐
                      │                  │
             ACC ─────┘                  │
              ↑                          │
              └──────────────────────────┘

One MAC = one ×, one +, one register write
```

MAC 是矩阵乘法中的基本运算。脉动阵列将 N×N 个 MAC 分块，以计算完整的矩阵乘积：

```
A[i,k] flows right →      B[k,j] flows down ↓

Each PE: acc += A × B, then pass A right and B down.
After N cycles, PE[i,j] holds C[i,j] = Σ_k A[i,k]×B[k,j].
```


<details>
<summary>English original</summary>

**2.4 CMOS Implementation**

CMOS (Complementary MOS) uses PMOS pull-up networks and NMOS pull-down networks. The two networks are duals of each other.

```
CMOS NAND gate (2-input):

        VDD
         |
    ─────┤  PMOS (A)     ← parallel pull-up
    │    └──┐
    └──────┤  PMOS (B)
            └─── Y
            ┌──
    ┌──────┤  NMOS (A)   ← series pull-down
    │    ┌──┘
    ─────┤  NMOS (B)
         |
        GND

Logic:  Y = NAND(A,B) = ¬(A · B)

If A=1 AND B=1:  both NMOS on (pull Y low), both PMOS off → Y = 0  ✓
If A=0 OR  B=0:  one PMOS on (pull Y high), NMOS chain broken → Y = 1  ✓
```

**Key CMOS properties:**

| Property             | Value                                        |
|----------------------|----------------------------------------------|
| Static power         | Near zero (only leakage)                     |
| Dynamic power        | C × V² × f (scales with switching activity) |
| Noise margin         | ~40% of VDD                                  |
| Fan-out              | Theoretically unlimited (MOS gate is capacitive) |
| Inversion            | Every CMOS gate inverts (NAND, NOR, NOT)     |

> **Why only NAND/NOR?** AND = NAND + NOT = two gate stages. An extra inversion costs delay and area. Modern synthesis tools always work in NAND/NOR/NOT internally.

---

**3. Combinational Logic**

**3.1 Multiplexers and Decoders**

**MUX (2:1)** — select one of two data inputs based on a control signal:

```
  D0 ──┐
        ├── Y   Y = S̄·D0 + S·D1
  D1 ──┘
        ↑
        S (select)

  S=0 → Y=D0
  S=1 → Y=D1
```

**4:1 MUX** from two layers of 2:1 MUX:

```
  D0 ─┐                  S1=0: top 2:1 → D0 or D1
  D1 ─┤ 2:1 MUX ─┐       S1=1: bot 2:1 → D2 or D3
       └──(S0)    │
  D2 ─┐           ├─ 2:1 MUX ── Y
  D3 ─┤ 2:1 MUX ─┘         └──(S1)
       └──(S0)
```

**Decoder (2-to-4)** — activate exactly one of N outputs:

```
Inputs: A1, A0
Outputs: Y0..Y3

Y0 = Ā1·Ā0   (active when A=00)
Y1 = Ā1·A0   (active when A=01)
Y2 = A1·Ā0   (active when A=10)
Y3 = A1·A0   (active when A=11)
```

Decoders appear as row/column address decoders in every SRAM and register file.

**3.2 Adders: Ripple-Carry vs Carry-Lookahead**

**Full adder (1-bit):**

```
  A ──┐
  B ──┼── XOR ──────── Sum  =  A ⊕ B ⊕ Cin
  Cin─┘

  Carry-out  =  A·B  +  Cin·(A⊕B)
             =  A·B  +  Cin·(A XOR B)
```

**Ripple-carry adder (RCA)** — chain N full adders:

```
  A3 B3    A2 B2    A1 B1    A0 B0
   │  │     │  │     │  │     │  │
  ┌┴──┴┐   ┌┴──┴┐   ┌┴──┴┐   ┌┴──┴┐
  │ FA │◄──│ FA │◄──│ FA │◄──│ FA │◄── Cin=0
  └────┘   └────┘   └────┘   └────┘
  Cout S3     S2       S1       S0

Delay: N × t_FA   (linear in word width)
32-bit: ~32 × 0.5ns = 16ns at 1 GHz  (too slow for the critical path)
```

**Carry-Lookahead Adder (CLA)** — compute all carries in parallel:

```
Generate: Gi = Ai · Bi        (this bit produces a carry regardless of Cin)
Propagate: Pi = Ai ⊕ Bi      (this bit passes Cin to Cout)

C1 = G0 + P0·C0
C2 = G1 + P1·G0 + P1·P0·C0
C3 = G2 + P2·G1 + P2·P1·G0 + P2·P1·P0·C0
C4 = G3 + P3·G2 + P3·P2·G1 + P3·P2·P1·G0 + P3·P2·P1·P0·C0

All four carries computed in 2 gate delays (one AND, one OR).
```

**Delay comparison:**

| Adder type          | Delay (N-bit)   | Area      |
|---------------------|-----------------|-----------|
| Ripple-carry        | O(N)            | O(N)      |
| Carry-lookahead     | O(log N)        | O(N log N)|
| Carry-select        | O(√N)           | O(N)      |
| Kogge-Stone (tree)  | O(log N)        | O(N log N)|

Modern ALUs use Kogge-Stone or Han-Carlson prefix trees to achieve O(log N) delay with regular, pipeable structure.

**3.3 Multipliers and MAC Units**

**Array multiplier (4×4 unsigned):**

```
           A3  A2  A1  A0
         ×  B3  B2  B1  B0
         ─────────────────
         A3B0 A2B0 A1B0 A0B0     ← partial product row 0 (AND gates)
    A3B1 A2B1 A1B1 A0B1          ← partial product row 1 (shift 1)
A3B2 A2B2 A1B2 A0B2              ← partial product row 2 (shift 2)
A3B3 A2B3 A1B3 A0B3              ← partial product row 3 (shift 3)
─────────────────────────────────
P7   P6   P5   P4   P3  P2  P1  P0

Partial products are summed with an adder tree (Wallace tree reduces rows in O(log N) delay).
```

**MAC (Multiply-Accumulate):**

```
ACC ← ACC + (A × B)

  A ─┐
     ├── MULTIPLIER ─┐
  B ─┘               ├── ADDER ── ACC ──┐
                      │                  │
             ACC ─────┘                  │
              ↑                          │
              └──────────────────────────┘

One MAC = one ×, one +, one register write
```

The MAC is the fundamental operation in matrix multiplication. A systolic array tiles N×N MACs to compute a full matrix product:

```
A[i,k] flows right →      B[k,j] flows down ↓

Each PE: acc += A × B, then pass A right and B down.
After N cycles, PE[i,j] holds C[i,j] = Σ_k A[i,k]×B[k,j].
```

</details>

### 3.4 ALU 设计

一个最小 ALU 通过 opcode 在多个操作之间选择：

```
                  ┌──────────────────────────────────────────────┐
  A[31:0] ─────►  │  ADD   SUB   AND   OR   XOR   SLT   SHR   SHL │
  B[31:0] ─────►  │                                                │
                  └──────────────────┬───────────────────────────┘
                                     │ 8:1 MUX
                         OpCode[2:0] ┘
                                     ▼
                                 Result[31:0]
                                 + Flags: Zero, Carry, Overflow, Negative
```

**标志位：**

| 标志     | 含义                              | 用途                         |
|----------|--------------------------------------|-----------------------------|
| 零（Z） | 结果 == 0                          | 相等/不等分支      |
| 进位（C）| 无符号溢出 / 借位           | 多字算术       |
| 溢出（V） | 有符号溢出                  | 有符号算术检查     |
| 负（N） | 结果的 MSB 为 1（2 的补码中为负） | 有符号比较     |

### 3.5 传播延迟与关键路径

每个门都有传播延迟 t_p —— 从输入变化到输出稳定的时间。

```
Critical path: the longest delay path from any input to any output.
Minimum clock period = critical path delay + setup time + clock skew.

Example: 32-bit ripple-carry adder
  t_FA = t_XOR + t_AND + t_OR ≈ 3 × 100ps = 300ps per stage
  Critical path = 32 × 300ps = 9.6ns  →  max freq ≈ 104 MHz

Same adder, 4-bit CLA blocks (4 blocks):
  CLA block delay ≈ 2 gate delays = 200ps
  Carry ripple between 4 blocks: 4 × 200ps = 800ps
  Sum out: 800ps + 300ps ≈ 1.1ns  →  max freq ≈ 900 MHz
```

**流水线（Pipelining）** 在路径中间插入寄存器，从而切分关键路径：

```
Before pipelining (one long combinational path):
  Input → [Stage A: 3ns] → [Stage B: 4ns] → [Stage C: 2ns] → Output
  Throughput: 1 result / 9ns  →  ~111 MHz

After pipelining (registers between stages):
  Input → [A: 3ns] → REG → [B: 4ns] → REG → [C: 2ns] → Output
  Clock period = max(3, 4, 2) + setup ≈ 4.2ns  →  ~238 MHz
  Throughput: 1 result / 4.2ns  (latency is 3× longer, throughput ~2.1× better)
```

---

## 4. 时序逻辑

### 4.1 D 触发器

D 触发器（DFF）是最典型的存储元件。它在时钟上升沿捕获 D。

```
       D ──┤D    Q├── Q
    CLK ──┤CLK   │
           │    Q̄├── Q̄ (complement)
           └──────┘

Timing:
                ___     ___     ___
  CLK:      ___|   |___|   |___|   |___
                     ↑           ↑       ← capture edges
  D:    ──[stable]──[stable]──[stable]──
  Q:    ──────────[D₀]─────[D₁]────────
                     ^ after t_clk-to-q
```

**关键时序参数：**

| 参数      | 符号    | 定义                                    |
|----------------|-----------|-----------------------------------------------|
| 建立时间     | t_su      | 在时钟沿之前 D 必须保持稳定的时长  |
| 保持时间     | t_h       | 在时钟沿之后 D 必须保持稳定的时长   |
| 时钟到 Q     | t_cq      | 从时钟沿到 Q 有效的时间               |
| 传播延迟    | t_p       | DFF 之间的组合逻辑延迟        |

**建立时间违例**（D 变化离时钟沿太近 → Q 进入亚稳态）：

```
  CLK:    _____|‾‾‾|_____
  D:      ────[old]X[new]    ← X = transition inside setup window
  Q:      ──────────[???]    ← metastable! may settle to wrong value
```

**最小周期约束：**  t_clk ≥ t_cq + t_p + t_su

### 4.2 寄存器、移位寄存器与 LFSR

**N 位寄存器** —— N 个 DFF 共享时钟和可选的加载使能：

```
  D[N-1:0] ──► [DFF] [DFF] ... [DFF] ──► Q[N-1:0]
                CLK shared across all
```

**移位寄存器** —— DFF 级联；每个周期数据移动一位：

```
  D_in → [DFF₀] → [DFF₁] → [DFF₂] → [DFF₃] → D_out
           Q₀       Q₁       Q₂       Q₃

  Use: serial-to-parallel conversion, FIFO, delay line, CRC computation
```

**LFSR（线性反馈移位寄存器）** —— 从特定位置抽头 XOR 后反馈到输入端：

```
  Fibonacci LFSR, polynomial x⁴ + x³ + 1 (taps at positions 4 and 3):

  ┌─── XOR ◄──────────────────────────────┐
  │    ↑                                   │
  │  [DFF₄] ← [DFF₃] ← [DFF₂] ← [DFF₁]  │
  │    Q₄       Q₃       Q₂       Q₁      │
  │                                        │
  └──────────────────────────────── Q₄ → output

  Period: 2⁴ - 1 = 15 (maximal length for this polynomial)
  Output sequence: 1111, 0111, 1011, 1101, 0110, 1010, 0101, 1010, ...
```

LFSR 用于：伪随机数生成、BIST（内建自测试）、扩频时钟、CRC 硬件，以及 PCIe/USB 中的加扰。


<details>
<summary>English original</summary>

**3.4 ALU Design**

A minimal ALU selects among operations via an opcode:

```
                  ┌──────────────────────────────────────────────┐
  A[31:0] ─────►  │  ADD   SUB   AND   OR   XOR   SLT   SHR   SHL │
  B[31:0] ─────►  │                                                │
                  └──────────────────┬───────────────────────────┘
                                     │ 8:1 MUX
                         OpCode[2:0] ┘
                                     ▼
                                 Result[31:0]
                                 + Flags: Zero, Carry, Overflow, Negative
```

**Flag bits:**

| Flag     | Meaning                              | Use                         |
|----------|--------------------------------------|-----------------------------|
| Zero (Z) | Result == 0                          | Branch equal/not-equal      |
| Carry (C)| Unsigned overflow / borrow           | Multi-word arithmetic       |
| Overflow (V) | Signed overflow                  | Signed arithmetic check     |
| Negative (N) | Result MSB is 1 (negative in 2's C) | Signed comparisons     |

**3.5 Propagation Delay and Critical Path**

Every gate has a propagation delay t_p — the time from input change to output stable.

```
Critical path: the longest delay path from any input to any output.
Minimum clock period = critical path delay + setup time + clock skew.

Example: 32-bit ripple-carry adder
  t_FA = t_XOR + t_AND + t_OR ≈ 3 × 100ps = 300ps per stage
  Critical path = 32 × 300ps = 9.6ns  →  max freq ≈ 104 MHz

Same adder, 4-bit CLA blocks (4 blocks):
  CLA block delay ≈ 2 gate delays = 200ps
  Carry ripple between 4 blocks: 4 × 200ps = 800ps
  Sum out: 800ps + 300ps ≈ 1.1ns  →  max freq ≈ 900 MHz
```

**Pipelining** cuts the critical path by adding registers mid-path:

```
Before pipelining (one long combinational path):
  Input → [Stage A: 3ns] → [Stage B: 4ns] → [Stage C: 2ns] → Output
  Throughput: 1 result / 9ns  →  ~111 MHz

After pipelining (registers between stages):
  Input → [A: 3ns] → REG → [B: 4ns] → REG → [C: 2ns] → Output
  Clock period = max(3, 4, 2) + setup ≈ 4.2ns  →  ~238 MHz
  Throughput: 1 result / 4.2ns  (latency is 3× longer, throughput ~2.1× better)
```

---

**4. Sequential Logic**

**4.1 D Flip-Flop**

The D flip-flop (DFF) is the canonical memory element. It captures D on the rising clock edge.

```
       D ──┤D    Q├── Q
    CLK ──┤CLK   │
           │    Q̄├── Q̄ (complement)
           └──────┘

Timing:
                ___     ___     ___
  CLK:      ___|   |___|   |___|   |___
                     ↑           ↑       ← capture edges
  D:    ──[stable]──[stable]──[stable]──
  Q:    ──────────[D₀]─────[D₁]────────
                     ^ after t_clk-to-q
```

**Critical timing parameters:**

| Parameter      | Symbol    | Definition                                    |
|----------------|-----------|-----------------------------------------------|
| Setup time     | t_su      | D must be stable this long BEFORE clock edge  |
| Hold time      | t_h       | D must be stable this long AFTER clock edge   |
| Clock-to-Q     | t_cq      | Time from clock edge to Q valid               |
| Propagation    | t_p       | Combinational logic delay between DFFs        |

**Setup time violation** (D changes too close to clock edge → Q becomes metastable):

```
  CLK:    _____|‾‾‾|_____
  D:      ────[old]X[new]    ← X = transition inside setup window
  Q:      ──────────[???]    ← metastable! may settle to wrong value
```

**Minimum period constraint:**  t_clk ≥ t_cq + t_p + t_su

**4.2 Registers, Shift Registers, and LFSR**

**N-bit register** — N DFFs sharing a clock and optional load-enable:

```
  D[N-1:0] ──► [DFF] [DFF] ... [DFF] ──► Q[N-1:0]
                CLK shared across all
```

**Shift register** — DFFs chained; data shifts one position per cycle:

```
  D_in → [DFF₀] → [DFF₁] → [DFF₂] → [DFF₃] → D_out
           Q₀       Q₁       Q₂       Q₃

  Use: serial-to-parallel conversion, FIFO, delay line, CRC computation
```

**LFSR (Linear Feedback Shift Register)** — XOR taps from specific positions feed back to input:

```
  Fibonacci LFSR, polynomial x⁴ + x³ + 1 (taps at positions 4 and 3):

  ┌─── XOR ◄──────────────────────────────┐
  │    ↑                                   │
  │  [DFF₄] ← [DFF₃] ← [DFF₂] ← [DFF₁]  │
  │    Q₄       Q₃       Q₂       Q₁      │
  │                                        │
  └──────────────────────────────── Q₄ → output

  Period: 2⁴ - 1 = 15 (maximal length for this polynomial)
  Output sequence: 1111, 0111, 1011, 1101, 0110, 1010, 0101, 1010, ...
```

LFSRs are used in: pseudo-random number generation, BIST (Built-In Self-Test), spread spectrum clocking, CRC hardware, and scrambling in PCIe/USB.

</details>

### 4.3 有限状态机

**两种类型：**

| 类型   | 输出取决于        | 输出变化时刻    |
|--------|--------------------------|-------------------|
| Moore  | 仅状态               | 时钟边沿之后  |
| Mealy  | 状态与当前输入 | 立即       |

**交通灯 FSM（Moore）——4 个状态：**

```
States: GREEN_NS, YELLOW_NS, GREEN_EW, YELLOW_EW
Input:  timer_expired (1 = time to change)

         timer_expired
GREEN_NS ──────────────► YELLOW_NS
   ↑                          │
   │                   always │
   │                          ▼
YELLOW_EW ◄──────── GREEN_EW
              always
```

**HDL 风格的状态寄存器（可综合模式）：**

```verilog
// State register — always sequential
always @(posedge clk or posedge rst) begin
    if (rst)  state <= GREEN_NS;
    else      state <= next_state;
end

// Next-state logic — always combinational
always @(*) begin
    case (state)
        GREEN_NS:  next_state = timer_expired ? YELLOW_NS : GREEN_NS;
        YELLOW_NS: next_state = GREEN_EW;
        GREEN_EW:  next_state = timer_expired ? YELLOW_EW : GREEN_EW;
        YELLOW_EW: next_state = GREEN_NS;
        default:   next_state = GREEN_NS;
    endcase
end

// Output logic — Moore: outputs depend on state only
assign light_ns = (state == GREEN_NS)  ? GREEN  :
                  (state == YELLOW_NS) ? YELLOW : RED;
assign light_ew = (state == GREEN_EW)  ? GREEN  :
                  (state == YELLOW_EW) ? YELLOW : RED;
```

**状态编码策略：**

| 编码方式   | 所需位宽 | 功耗 | 速度     | 适用场景                        |
|------------|-------------|-------|-----------|---------------------------------|
| 二进制     | log₂(N)     | 低   | 中等  | 面积受限的 FPGA、ASIC   |
| 独热    | N           | 高  | 快      | FPGA（触发器资源充足）      |
| 格雷码  | log₂(N)     | 低   | 中等  | 减少输出逻辑中的毛刺 |

> **AI 硬件关联：** GPU SM 内部的控制单元就是一个大型 FSM。warp 调度器跟踪 warp 状态（就绪、等待访存、等待依赖），并在每个周期让每个 warp 做状态转移。最多 64 个 warp = 64 个 FSM 实例并行运行。

### 4.4 时序分析与关键路径

**建立时间分析** —— 找出全部寄存器到寄存器路径中的关键路径：

```
  REG A → [combo logic, delay t₁] → REG B

  Setup check: t₁ + t_su ≤ t_clk - t_skew
  (data must arrive before clock arrives at destination minus skew)
```

**保持时间分析** —— 确保数据变化不会过快：

```
  Hold check: t_cq + t_min_combo ≥ t_h + t_skew
  (data must stay stable long enough after clock edge)
```

**时钟偏斜** —— 两个触发器上时钟到达时间的差值：

```
   CLK source
      │
   ┌──┴──┐
   │     │   ← routing delays differ
 REG A  REG B
  CLK   CLK
  t₁    t₂     skew = t₂ - t₁

  Positive skew (t₂ > t₁): relaxes setup, tightens hold
  Negative skew (t₂ < t₁): tightens setup, relaxes hold
```

**时钟门控** —— 在空闲时关断寄存器组的时钟，以节省动态功耗：

```
  CLK ───┐
          ├── AND ──► gated CLK → register bank
  EN  ───┘

  Saves: C_register × V² × f per gated cycle
  Risk: glitches if EN changes while CLK=1 (use latch-based clock gate)
```

---

## 5. 存储器技术

### 5.1 SRAM —— 静态随机存取存储器

SRAM 用一对交叉耦合的反相器（即 **6T 单元**）存储一个位：

```
        VDD
         │
    ─────┼──────────────────────────────
    │    PMOS₁                PMOS₂   │
    │      │                    │     │
    │      ├── Q ─────── Q̄ ───┤      │
    │    NMOS₁       ┌───┐  NMOS₂    │
    │      │         │INV│    │      │
    │      │         └───┘    │      │
    ─────NMOS₃            NMOS₄──────
    │      │                    │    │
    BL     WL─────────────────WL    BL̄

6T SRAM cell:
  - 2 PMOS (pull-up)
  - 2 NMOS (pull-down) forming the latch
  - 2 NMOS access transistors (NMOS₃, NMOS₄) controlled by Word Line
```

**操作：**

| 阶段     | WL  | BL  | BL̄ | 动作                                     |
|-----------|-----|-----|----|--------------------------------------------|
| 待机   | 0   | Pre | Pre| 锁存器保持状态；访问晶体管关断  |
| 读      | 1   | Pre | Pre| 单元对一条 BL 放电；灵敏放大器检测  |
| 写     | 1   | D   | D̄  | 驱动器压过锁存器；单元翻转         |

**SRAM 特性：**

| 属性            | 取值                                    |
|---------------------|------------------------------------------|
| 访问时间         | 0.5–5 ns（L1：~1ns，LLC：~10ns）         |
| 密度             | 6T 单元 ≈ 120–150 F²（F = 特征尺寸） |
| 功耗               | 静态漏电 + 动态读写      |
| 数据保持           | 只要 VDD 持续加电就保持数据        |
| 片上用途         | 寄存器、缓存、scratchpad、FIFO     |


<details>
<summary>English original</summary>

**4.3 Finite State Machines**

**Two types:**

| Type   | Output depends on        | Outputs change    |
|--------|--------------------------|-------------------|
| Moore  | State only               | After clock edge  |
| Mealy  | State AND current inputs | Immediately       |

**Traffic light FSM (Moore) — 4 states:**

```
States: GREEN_NS, YELLOW_NS, GREEN_EW, YELLOW_EW
Input:  timer_expired (1 = time to change)

         timer_expired
GREEN_NS ──────────────► YELLOW_NS
   ↑                          │
   │                   always │
   │                          ▼
YELLOW_EW ◄──────── GREEN_EW
              always
```

**HDL-style state register (synthesizable pattern):**

```verilog
// State register — always sequential
always @(posedge clk or posedge rst) begin
    if (rst)  state <= GREEN_NS;
    else      state <= next_state;
end

// Next-state logic — always combinational
always @(*) begin
    case (state)
        GREEN_NS:  next_state = timer_expired ? YELLOW_NS : GREEN_NS;
        YELLOW_NS: next_state = GREEN_EW;
        GREEN_EW:  next_state = timer_expired ? YELLOW_EW : GREEN_EW;
        YELLOW_EW: next_state = GREEN_NS;
        default:   next_state = GREEN_NS;
    endcase
end

// Output logic — Moore: outputs depend on state only
assign light_ns = (state == GREEN_NS)  ? GREEN  :
                  (state == YELLOW_NS) ? YELLOW : RED;
assign light_ew = (state == GREEN_EW)  ? GREEN  :
                  (state == YELLOW_EW) ? YELLOW : RED;
```

**State encoding strategies:**

| Encoding   | Bits needed | Power | Speed     | Use case                        |
|------------|-------------|-------|-----------|---------------------------------|
| Binary     | log₂(N)     | Low   | Moderate  | Area-constrained FPGAs, ASICs   |
| One-hot    | N           | High  | Fast      | FPGA (abundant flip-flops)      |
| Gray code  | log₂(N)     | Low   | Moderate  | Reduces glitches in output logic |

> **AI hardware connection:** the control unit inside a GPU SM is a large FSM. The warp scheduler tracks warp state (ready, waiting on memory, waiting on dependency) and transitions each warp on every cycle. Up to 64 warps = 64 simultaneous FSM instances running in parallel.

**4.4 Timing Analysis and Critical Path**

**Setup analysis** — find the critical path across all register-to-register paths:

```
  REG A → [combo logic, delay t₁] → REG B

  Setup check: t₁ + t_su ≤ t_clk - t_skew
  (data must arrive before clock arrives at destination minus skew)
```

**Hold analysis** — ensure data doesn't change too fast:

```
  Hold check: t_cq + t_min_combo ≥ t_h + t_skew
  (data must stay stable long enough after clock edge)
```

**Clock skew** — the difference in clock arrival time at two flip-flops:

```
   CLK source
      │
   ┌──┴──┐
   │     │   ← routing delays differ
 REG A  REG B
  CLK   CLK
  t₁    t₂     skew = t₂ - t₁

  Positive skew (t₂ > t₁): relaxes setup, tightens hold
  Negative skew (t₂ < t₁): tightens setup, relaxes hold
```

**Clock gating** — disable the clock to a register bank when idle to save dynamic power:

```
  CLK ───┐
          ├── AND ──► gated CLK → register bank
  EN  ───┘

  Saves: C_register × V² × f per gated cycle
  Risk: glitches if EN changes while CLK=1 (use latch-based clock gate)
```

---

**5. Memory Technologies**

**5.1 SRAM — Static Random Access Memory**

SRAM stores a bit in a cross-coupled inverter pair (the **6T cell**):

```
        VDD
         │
    ─────┼──────────────────────────────
    │    PMOS₁                PMOS₂   │
    │      │                    │     │
    │      ├── Q ─────── Q̄ ───┤      │
    │    NMOS₁       ┌───┐  NMOS₂    │
    │      │         │INV│    │      │
    │      │         └───┘    │      │
    ─────NMOS₃            NMOS₄──────
    │      │                    │    │
    BL     WL─────────────────WL    BL̄

6T SRAM cell:
  - 2 PMOS (pull-up)
  - 2 NMOS (pull-down) forming the latch
  - 2 NMOS access transistors (NMOS₃, NMOS₄) controlled by Word Line
```

**Operation:**

| Phase     | WL  | BL  | BL̄ | Action                                     |
|-----------|-----|-----|----|--------------------------------------------|
| Standby   | 0   | Pre | Pre| Latch holds state; access transistors off  |
| Read      | 1   | Pre | Pre| Cell discharges one BL; sense amp detects  |
| Write     | 1   | D   | D̄  | Driver overrides latch; cell flips         |

**SRAM characteristics:**

| Property            | Value                                    |
|---------------------|------------------------------------------|
| Access time         | 0.5–5 ns (L1: ~1ns, LLC: ~10ns)         |
| Density             | 6T cell ≈ 120–150 F² (F = feature size) |
| Power               | Static leakage + dynamic read/write      |
| Retention           | Holds data as long as VDD applied        |
| On-chip use         | Registers, caches, scratchpad, FIFOs     |

</details>

### 5.2 DRAM — Dynamic RAM

DRAM 把电荷存储在电容上——一个晶体管、一个电容（1T1C）：

```
       BL
        │
       NMOS  ← access transistor (WL controls)
        │
       CAP   ← ~20–30 fF capacitor
        │
       GND

Charged cap (≥ Vth/2) = logic 1
Discharged cap (≤ Vth/2) = logic 0
```

**与 SRAM 的关键差异：**

| Property      | SRAM                    | DRAM                        |
|---------------|-------------------------|-----------------------------|
| Cell size     | 6 transistors           | 1 transistor + 1 capacitor  |
| Density       | Low                     | 4–8× higher than SRAM       |
| Speed         | ~1–10 ns                | ~50–100 ns (row access)     |
| Refresh       | Not needed              | Every ~64 ms (capacitor leaks) |
| Destructive read | No                   | Yes (must restore after read) |
| Cost/GB       | ~50–100×  more than DRAM | Baseline                   |

**DRAM 访问序列：**

```
1. Row activate (RAS): open one row (wordline) into row buffer (~45 ns)
2. Column access (CAS): select columns from row buffer (~15 ns)
3. Precharge: close row, precharge bit lines
4. Refresh: periodically rewrite all rows before charge leaks away

DRAM bandwidth = (bus width × frequency × burst length) / 8
DDR5-6400: 64-bit × 3200 MT/s × burst-8 = 51.2 GB/s per channel
```

### 5.3 HBM — High Bandwidth Memory

HBM 将 DRAM 裸片垂直堆叠在硅中介层上，通过硅通孔（TSV）连接：

```
  GPU Die ──────────────────────────── HBM stack
              Silicon interposer

  HBM stack cross-section:

    ┌─────────────┐   ← DRAM die 4
    ├─────────────┤   ← DRAM die 3
    ├─────────────┤   ← DRAM die 2
    ├─────────────┤   ← DRAM die 1
    └─────────────┘   ← Base die (logic)
           │││         ← TSVs (Through-Silicon Vias)
    ─────────────────  ← Silicon interposer
           │││
    ┌─────────────┐   ← GPU/accelerator die
```

**HBM vs GDDR6 vs DDR5：**

| Spec             | DDR5-6400   | GDDR6X       | HBM3          |
|------------------|-------------|--------------|---------------|
| Bus width/stack  | 64-bit      | 32-bit       | 1024-bit      |
| Bandwidth/stack  | 51 GB/s     | 96 GB/s      | ~900 GB/s     |
| Capacity/stack   | Unlimited   | 16–24 GB     | 16–48 GB      |
| Power efficiency | Moderate    | Moderate     | High          |
| On-package       | No          | No           | Yes (interposer)|
| Used in          | CPU systems | Gaming GPUs  | AI GPUs/TPUs  |

A100 通过 5 个 stack 提供 80 GB HBM2e，带宽 ~2 TB/s。H100 SXM 使用 HBM3，带宽 ~3.35 TB/s。GPU 裸片与 HBM 之间的互连是 AI 计算中最大的单一内存带宽瓶颈。

### 5.4 Flash Memory

Flash 把电荷存储在浮栅或电荷陷阱层中——保持数据无需供电。

```
NAND Flash cell (floating gate):

    Control gate (CG)
         │
    ─────┼────   ← tunnel oxide
    Floating gate (FG) ← charge stored here
    ─────┼────   ← gate oxide
    Source           Drain
       │                │
       └─────── ────────┘
                Channel

Writing: tunnel electrons onto FG (Fowler-Nordheim tunneling) → raises Vt → reads as 0
Erasing: remove electrons from FG → lowers Vt → reads as 1
```

**按每单元比特数划分的 Flash 单元类型：**

| Type | Bits/cell | Voltage levels | Endurance  | Speed  | Use                |
|------|-----------|---------------|------------|--------|--------------------|
| SLC  | 1         | 2             | 100K P/E   | Fast   | Enterprise SSD     |
| MLC  | 2         | 4             | 10K P/E    | Medium | Consumer SSD       |
| TLC  | 3         | 8             | 1K P/E     | Slower | Bulk storage       |
| QLC  | 4         | 16            | 300 P/E    | Slow   | Archival, cold data |

Flash **写入时不是按字节寻址**——写入以页为单位（4–16 KB），擦除以块为单位（128–512 页）。这种非对称性催生了 SSD 控制器中的 Flash Translation Layer（FTL）。


<details>
<summary>English original</summary>

**5.2 DRAM — Dynamic RAM**

DRAM stores charge on a capacitor — one transistor, one capacitor (1T1C):

```
       BL
        │
       NMOS  ← access transistor (WL controls)
        │
       CAP   ← ~20–30 fF capacitor
        │
       GND

Charged cap (≥ Vth/2) = logic 1
Discharged cap (≤ Vth/2) = logic 0
```

**Key differences from SRAM:**

| Property      | SRAM                    | DRAM                        |
|---------------|-------------------------|-----------------------------|
| Cell size     | 6 transistors           | 1 transistor + 1 capacitor  |
| Density       | Low                     | 4–8× higher than SRAM       |
| Speed         | ~1–10 ns                | ~50–100 ns (row access)     |
| Refresh       | Not needed              | Every ~64 ms (capacitor leaks) |
| Destructive read | No                   | Yes (must restore after read) |
| Cost/GB       | ~50–100×  more than DRAM | Baseline                   |

**DRAM access sequence:**

```
1. Row activate (RAS): open one row (wordline) into row buffer (~45 ns)
2. Column access (CAS): select columns from row buffer (~15 ns)
3. Precharge: close row, precharge bit lines
4. Refresh: periodically rewrite all rows before charge leaks away

DRAM bandwidth = (bus width × frequency × burst length) / 8
DDR5-6400: 64-bit × 3200 MT/s × burst-8 = 51.2 GB/s per channel
```

**5.3 HBM — High Bandwidth Memory**

HBM stacks DRAM dies vertically on silicon interposer, connected via Through-Silicon Vias (TSVs):

```
  GPU Die ──────────────────────────── HBM stack
              Silicon interposer

  HBM stack cross-section:

    ┌─────────────┐   ← DRAM die 4
    ├─────────────┤   ← DRAM die 3
    ├─────────────┤   ← DRAM die 2
    ├─────────────┤   ← DRAM die 1
    └─────────────┘   ← Base die (logic)
           │││         ← TSVs (Through-Silicon Vias)
    ─────────────────  ← Silicon interposer
           │││
    ┌─────────────┐   ← GPU/accelerator die
```

**HBM vs GDDR6 vs DDR5:**

| Spec             | DDR5-6400   | GDDR6X       | HBM3          |
|------------------|-------------|--------------|---------------|
| Bus width/stack  | 64-bit      | 32-bit       | 1024-bit      |
| Bandwidth/stack  | 51 GB/s     | 96 GB/s      | ~900 GB/s     |
| Capacity/stack   | Unlimited   | 16–24 GB     | 16–48 GB      |
| Power efficiency | Moderate    | Moderate     | High          |
| On-package       | No          | No           | Yes (interposer)|
| Used in          | CPU systems | Gaming GPUs  | AI GPUs/TPUs  |

A100 has 80 GB HBM2e across 5 stacks at ~2 TB/s. H100 SXM uses HBM3 at ~3.35 TB/s. The interconnect between the GPU die and HBM is the single biggest memory bandwidth bottleneck in AI compute.

**5.4 Flash Memory**

Flash stores charge in a floating gate or charge trap layer — no power needed for retention.

```
NAND Flash cell (floating gate):

    Control gate (CG)
         │
    ─────┼────   ← tunnel oxide
    Floating gate (FG) ← charge stored here
    ─────┼────   ← gate oxide
    Source           Drain
       │                │
       └─────── ────────┘
                Channel

Writing: tunnel electrons onto FG (Fowler-Nordheim tunneling) → raises Vt → reads as 0
Erasing: remove electrons from FG → lowers Vt → reads as 1
```

**Flash cell types by bits per cell:**

| Type | Bits/cell | Voltage levels | Endurance  | Speed  | Use                |
|------|-----------|---------------|------------|--------|--------------------|
| SLC  | 1         | 2             | 100K P/E   | Fast   | Enterprise SSD     |
| MLC  | 2         | 4             | 10K P/E    | Medium | Consumer SSD       |
| TLC  | 3         | 8             | 1K P/E     | Slower | Bulk storage       |
| QLC  | 4         | 16            | 300 P/E    | Slow   | Archival, cold data |

Flash is **not byte-addressable for writes** — writes go to a page (4–16 KB), erasures go to a block (128–512 pages). This asymmetry drives the Flash Translation Layer (FTL) in SSD controllers.

</details>

### 5.5 缓存层级

缓存利用空间局部性和时间局部性，弥合 CPU/GPU 寄存器与主存之间的速度差距。

```
GPU/CPU Memory Hierarchy (approximate, H100/server class):

Registers          ~0.2 ns   ~256 KB     per SM (64K 32-bit regs × 108 SMs)
L1 / Shared mem   ~5 ns     ~228 KB     per SM (configurable split)
L2 cache          ~50 ns    ~50 MB      on-chip, shared
HBM3              ~100 ns   ~80 GB      off-chip, on-package
System DDR5       ~200 ns   ~TBs        off-package
NVMe SSD          ~100 μs   ~TBs        PCIe storage

Bandwidth (rough):
  Register file   → FPU:         100+ TB/s
  L1/Shared mem  → register:    ~15 TB/s per SM
  L2              → SM:          ~12 TB/s total
  HBM3            → chip:        ~3.35 TB/s
  PCIe5 x16       → host:        ~64 GB/s
```

**缓存组织：**

```
Direct-mapped (1-way):     Set-associative (4-way):
  Tag | Index | Offset       Tag | Index | Offset

  One slot per set          Four slots per set (4 ways)
  Fast, simple              Fewer conflict misses

  Index selects row         Index selects set, all 4 ways checked in parallel
  Tag must match            Any way whose tag matches → hit
```

**替换策略：**

| 策略 | 驱逐对象                            | 硬件开销 |
|--------|-----------------------------------|---------------|
| LRU    | 最近最少使用的路           | 每 set O(log W) 位 |
| PLRU   | 伪 LRU（树形近似）   | 每 set W−1 位 |
| Random | 随机一路                        | LFSR 计数器  |
| FIFO   | 最早载入的路                 | 每 set 一个指针 |

GPU L1 使用简化版 LRU 或 PLRU。CPU 末级缓存常在 16 路组相联下使用 QLRU（quad-age LRU）。

### 5.6 片上 scratchpad 与分块

与缓存（硬件管理、对软件透明）不同，GPU 中的 **共享内存** 是由软件管理的 SRAM——程序员显式控制其中存放的内容。

**为什么分块重要**——GPU 上不做分块的矩阵乘法：

```
Naive: each thread loads A[row, k] and B[k, col] from HBM for every k

For M=N=K=1024, C=A×B:
  Loads from HBM: M × N × K × 2 (A and B) = 2 × 10⁹ float reads
  At 3.35 TB/s HBM bandwidth: 2 × 10⁹ × 4B / 3.35TB/s ≈ 2.4 ms
  Compute at 312 TFLOPS: 2 × 10⁹ FLOPs / 312 TFLOPS ≈ 0.006 ms
  → memory-bound by 400×
```

**采用分块（共享内存）：**

```
Block size: 16×16 output tile
  Load tile of A (16×K_chunk) → shared memory    } one time per block
  Load tile of B (K_chunk×16) → shared memory    }
  Each thread computes its dot product from shared memory (fast)

Arithmetic intensity = FLOPs / bytes from HBM
  Naive: 2 FLOPs per 8 bytes (FMA = 2 ops, loads A and B) → 0.25 FLOP/byte
  Tiled: 2 × T FLOPs per (2 × T × 4) bytes → T/4 FLOP/byte (T = tile size)
  T=16: 4 FLOP/byte
  T=128: 32 FLOP/byte  →  compute-bound at H100 (312 TFLOPS / 3350 GB/s = 93)
```

**共享内存 bank 冲突**——32 个 bank，每个宽 4 字节；warp 中的线程若命中不同 bank，则访问无冲突：

```
No conflict: thread i accesses address i × 4   (each thread → different bank)
2-way conflict: threads 0 and 16 both access bank 0 (serialized → 2× slower)
Broadcast: all threads read same address (one bank, no conflict → hardware broadcasts)

Fix: pad shared memory arrays by one element
  float tile[16][16+1];   ← +1 padding shifts each row to different bank
```

---

## 6. MIPS 处理器设计

用第 1–5 节的组件搭建一个处理器，是最经典的数字设计练习。MIPS 是标准的教学 ISA，因为其固定的 32 位指令格式能干净地映射到硬件。从单周期数据通路一路搭建到 5 级流水线——现代每一款处理器（包括 GPU shader core）的流水线都源自这一结构。


<details>
<summary>English original</summary>

**5.5 Cache Hierarchy**

A cache exploits spatial and temporal locality to bridge the speed gap between CPU/GPU registers and main memory.

```
GPU/CPU Memory Hierarchy (approximate, H100/server class):

Registers          ~0.2 ns   ~256 KB     per SM (64K 32-bit regs × 108 SMs)
L1 / Shared mem   ~5 ns     ~228 KB     per SM (configurable split)
L2 cache          ~50 ns    ~50 MB      on-chip, shared
HBM3              ~100 ns   ~80 GB      off-chip, on-package
System DDR5       ~200 ns   ~TBs        off-package
NVMe SSD          ~100 μs   ~TBs        PCIe storage

Bandwidth (rough):
  Register file   → FPU:         100+ TB/s
  L1/Shared mem  → register:    ~15 TB/s per SM
  L2              → SM:          ~12 TB/s total
  HBM3            → chip:        ~3.35 TB/s
  PCIe5 x16       → host:        ~64 GB/s
```

**Cache organization:**

```
Direct-mapped (1-way):     Set-associative (4-way):
  Tag | Index | Offset       Tag | Index | Offset

  One slot per set          Four slots per set (4 ways)
  Fast, simple              Fewer conflict misses

  Index selects row         Index selects set, all 4 ways checked in parallel
  Tag must match            Any way whose tag matches → hit
```

**Replacement policies:**

| Policy | Evicts                            | Hardware cost |
|--------|-----------------------------------|---------------|
| LRU    | Least recently used way           | O(log W) bits per set |
| PLRU   | Pseudo-LRU (tree approximation)   | W−1 bits per set |
| Random | Random way                        | LFSR counter  |
| FIFO   | Oldest loaded way                 | Pointer per set |

GPU L1 uses a simplified LRU or PLRU. CPU last-level caches often use QLRU (quad-age LRU) at 16-way associativity.

**5.6 On-Chip Scratchpad and Tiling**

Unlike a cache (hardware-managed, transparent), **shared memory** in GPUs is software-managed SRAM — the programmer explicitly controls what lives there.

**Why tiling matters** — matrix multiply on GPU without tiling:

```
Naive: each thread loads A[row, k] and B[k, col] from HBM for every k

For M=N=K=1024, C=A×B:
  Loads from HBM: M × N × K × 2 (A and B) = 2 × 10⁹ float reads
  At 3.35 TB/s HBM bandwidth: 2 × 10⁹ × 4B / 3.35TB/s ≈ 2.4 ms
  Compute at 312 TFLOPS: 2 × 10⁹ FLOPs / 312 TFLOPS ≈ 0.006 ms
  → memory-bound by 400×
```

**With tiling (shared memory):**

```
Block size: 16×16 output tile
  Load tile of A (16×K_chunk) → shared memory    } one time per block
  Load tile of B (K_chunk×16) → shared memory    }
  Each thread computes its dot product from shared memory (fast)

Arithmetic intensity = FLOPs / bytes from HBM
  Naive: 2 FLOPs per 8 bytes (FMA = 2 ops, loads A and B) → 0.25 FLOP/byte
  Tiled: 2 × T FLOPs per (2 × T × 4) bytes → T/4 FLOP/byte (T = tile size)
  T=16: 4 FLOP/byte
  T=128: 32 FLOP/byte  →  compute-bound at H100 (312 TFLOPS / 3350 GB/s = 93)
```

**Shared memory bank conflicts** — 32 banks, each 4 bytes wide; threads in a warp access conflict-free if they hit different banks:

```
No conflict: thread i accesses address i × 4   (each thread → different bank)
2-way conflict: threads 0 and 16 both access bank 0 (serialized → 2× slower)
Broadcast: all threads read same address (one bank, no conflict → hardware broadcasts)

Fix: pad shared memory arrays by one element
  float tile[16][16+1];   ← +1 padding shifts each row to different bank
```

---

**6. MIPS Processor Design**

Building a processor from the components in sections 1–5 is the definitive digital design exercise. MIPS is the standard teaching ISA because its fixed 32-bit instruction format maps cleanly onto hardware. We build up from a single-cycle datapath through a 5-stage pipeline — the same pipeline that every modern processor (including GPU shader cores) descends from.

</details>

### 6.1 MIPS ISA 子集

最小核支持三种指令格式：

```
R-type (register):   [opcode 6][rs 5][rt 5][rd 5][shamt 5][funct 6]
I-type (immediate):  [opcode 6][rs 5][rt 5][imm 16]
J-type (jump):       [opcode 6][address 26]
```

**数据通路的关键指令：**

| 指令 | 格式 | 操作                    | 示例              |
|-------------|--------|------------------------------|----------------------|
| `add`       | R      | rd = rs + rt                 | `add $t0, $s1, $s2` |
| `sub`       | R      | rd = rs − rt                 | `sub $t0, $s1, $s2` |
| `and`       | R      | rd = rs & rt                 | `and $t0, $s1, $s2` |
| `or`        | R      | rd = rs \| rt                | `or  $t0, $s1, $s2` |
| `slt`       | R      | rd = (rs < rt) ? 1 : 0      | `slt $t0, $s1, $s2` |
| `lw`        | I      | rt = Mem[rs + imm]           | `lw  $t0, 4($s1)`   |
| `sw`        | I      | Mem[rs + imm] = rt           | `sw  $t0, 4($s1)`   |
| `beq`       | I      | if (rs == rt) PC += imm<<2  | `beq $s1, $s2, L1`  |
| `addi`      | I      | rt = rs + sign_ext(imm)      | `addi $t0, $s1, 10` |
| `j`         | J      | PC = {PC[31:28], addr, 2'b0}| `j   target`        |

**寄存器堆 — 32 个寄存器，$0 硬连线为 0：**

```
$zero ($0)  = always 0          $t0-$t9  = temporaries
$at   ($1)  = assembler temp    $s0-$s7  = saved (callee-preserved)
$v0-$v1     = return values     $sp ($29)= stack pointer
$a0-$a3     = arguments         $ra ($31)= return address
```

### 6.2 单周期数据通路

每条指令均在一个时钟周期内完成。数据通路把第 3–5 节的构建块连接起来：

```
                            ┌────────────────────────────────────────────────────┐
                            │                    Control Unit                    │
                            │  (opcode, funct → RegDst, ALUSrc, MemtoReg,       │
                            │   RegWrite, MemRead, MemWrite, Branch, ALUOp, Jump)│
                            └──────┬──────┬──────┬──────┬──────┬──────┬─────────┘
                                   │      │      │      │      │      │
  ┌──────┐    ┌──────────┐   ┌─────┴──┐  MUX   MUX    │      │      │
  │      │    │ Register │   │        │   │     │      │      │      │
  │  PC  ├──►│  File    ├──►│  ALU   ├───┘     │    ┌─┴──┐   │      │
  │      │    │ (32×32)  │   │        │         └───►│Data│   │      │
  └──┬───┘    │          │   └────────┘              │Mem │   │      │
     │        │  rd1─────┼───► A                     │    │   │      │
     │        │  rd2─────┼───► B / MUX ◄── imm       │    │   │      │
     ▼        └──────────┘                           └────┘   │      │
 ┌──────┐                                                     │      │
 │Instr │                                                     │      │
 │ Mem  │◄────────────────────────────────────────────────────┘      │
 └──────┘                                                            │
     │                                                               │
     └─── PC+4 / branch target / jump target ◄──────────────────────┘
```

**`lw $t0, 8($s1)` 的数据通路走查：**

```
Cycle:
  1. Fetch:     Instr Mem[PC] → instruction word
  2. Decode:    rs=$s1(17), rt=$t0(8), imm=8
                Register File reads: rd1 = Reg[17] = value of $s1
  3. Execute:   ALU computes: $s1 + sign_ext(8) = address
  4. Memory:    Data Mem[address] → loaded word
  5. Writeback: MUX selects memory output → Reg[8] ($t0)
  6. PC update: PC ← PC + 4

All in ONE cycle. Clock period = longest path (lw: through Instr Mem + Reg File + ALU + Data Mem + MUX).
```

**各指令类型的控制信号：**

| 信号    | R 型 | `lw`  | `sw`  | `beq` | `addi`| `j`   |
|-----------|--------|-------|-------|-------|-------|-------|
| RegDst    | 1 (rd) | 0 (rt)| X     | X     | 0 (rt)| X     |
| ALUSrc    | 0 (rt) | 1 (imm)| 1 (imm)| 0 (rt)| 1 (imm)| X  |
| MemtoReg  | 0 (ALU)| 1 (Mem)| X     | X     | 0 (ALU)| X   |
| RegWrite  | 1      | 1     | 0     | 0     | 1     | 0     |
| MemRead   | 0      | 1     | 0     | 0     | 0     | 0     |
| MemWrite  | 0      | 0     | 1     | 0     | 0     | 0     |
| Branch    | 0      | 0     | 0     | 1     | 0     | 0     |
| Jump      | 0      | 0     | 0     | 0     | 0     | 1     |
| ALUOp     | 10     | 00    | 00    | 01    | 00    | XX    |

**ALU 控制** — 两级译码：

```
ALUOp (from main control) + funct field (from instruction) → ALU operation

ALUOp=00 → ADD  (lw, sw, addi: address calculation)
ALUOp=01 → SUB  (beq: compare by subtraction)
ALUOp=10 → look at funct:
             funct=100000 → ADD
             funct=100010 → SUB
             funct=100100 → AND
             funct=100101 → OR
             funct=101010 → SLT
```


<details>
<summary>English original</summary>

**6.1 MIPS ISA Subset**

A minimal core supports three instruction formats:

```
R-type (register):   [opcode 6][rs 5][rt 5][rd 5][shamt 5][funct 6]
I-type (immediate):  [opcode 6][rs 5][rt 5][imm 16]
J-type (jump):       [opcode 6][address 26]
```

**Key instructions for the datapath:**

| Instruction | Format | Operation                    | Example              |
|-------------|--------|------------------------------|----------------------|
| `add`       | R      | rd = rs + rt                 | `add $t0, $s1, $s2` |
| `sub`       | R      | rd = rs − rt                 | `sub $t0, $s1, $s2` |
| `and`       | R      | rd = rs & rt                 | `and $t0, $s1, $s2` |
| `or`        | R      | rd = rs \| rt                | `or  $t0, $s1, $s2` |
| `slt`       | R      | rd = (rs < rt) ? 1 : 0      | `slt $t0, $s1, $s2` |
| `lw`        | I      | rt = Mem[rs + imm]           | `lw  $t0, 4($s1)`   |
| `sw`        | I      | Mem[rs + imm] = rt           | `sw  $t0, 4($s1)`   |
| `beq`       | I      | if (rs == rt) PC += imm<<2  | `beq $s1, $s2, L1`  |
| `addi`      | I      | rt = rs + sign_ext(imm)      | `addi $t0, $s1, 10` |
| `j`         | J      | PC = {PC[31:28], addr, 2'b0}| `j   target`        |

**Register file — 32 registers, $0 hardwired to zero:**

```
$zero ($0)  = always 0          $t0-$t9  = temporaries
$at   ($1)  = assembler temp    $s0-$s7  = saved (callee-preserved)
$v0-$v1     = return values     $sp ($29)= stack pointer
$a0-$a3     = arguments         $ra ($31)= return address
```

**6.2 Single-Cycle Datapath**

Every instruction completes in one clock cycle. The datapath connects the building blocks from sections 3–5:

```
                            ┌────────────────────────────────────────────────────┐
                            │                    Control Unit                    │
                            │  (opcode, funct → RegDst, ALUSrc, MemtoReg,       │
                            │   RegWrite, MemRead, MemWrite, Branch, ALUOp, Jump)│
                            └──────┬──────┬──────┬──────┬──────┬──────┬─────────┘
                                   │      │      │      │      │      │
  ┌──────┐    ┌──────────┐   ┌─────┴──┐  MUX   MUX    │      │      │
  │      │    │ Register │   │        │   │     │      │      │      │
  │  PC  ├──►│  File    ├──►│  ALU   ├───┘     │    ┌─┴──┐   │      │
  │      │    │ (32×32)  │   │        │         └───►│Data│   │      │
  └──┬───┘    │          │   └────────┘              │Mem │   │      │
     │        │  rd1─────┼───► A                     │    │   │      │
     │        │  rd2─────┼───► B / MUX ◄── imm       │    │   │      │
     ▼        └──────────┘                           └────┘   │      │
 ┌──────┐                                                     │      │
 │Instr │                                                     │      │
 │ Mem  │◄────────────────────────────────────────────────────┘      │
 └──────┘                                                            │
     │                                                               │
     └─── PC+4 / branch target / jump target ◄──────────────────────┘
```

**Datapath walk-through for `lw $t0, 8($s1)`:**

```
Cycle:
  1. Fetch:     Instr Mem[PC] → instruction word
  2. Decode:    rs=$s1(17), rt=$t0(8), imm=8
                Register File reads: rd1 = Reg[17] = value of $s1
  3. Execute:   ALU computes: $s1 + sign_ext(8) = address
  4. Memory:    Data Mem[address] → loaded word
  5. Writeback: MUX selects memory output → Reg[8] ($t0)
  6. PC update: PC ← PC + 4

All in ONE cycle. Clock period = longest path (lw: through Instr Mem + Reg File + ALU + Data Mem + MUX).
```

**Control signals for each instruction type:**

| Signal    | R-type | `lw`  | `sw`  | `beq` | `addi`| `j`   |
|-----------|--------|-------|-------|-------|-------|-------|
| RegDst    | 1 (rd) | 0 (rt)| X     | X     | 0 (rt)| X     |
| ALUSrc    | 0 (rt) | 1 (imm)| 1 (imm)| 0 (rt)| 1 (imm)| X  |
| MemtoReg  | 0 (ALU)| 1 (Mem)| X     | X     | 0 (ALU)| X   |
| RegWrite  | 1      | 1     | 0     | 0     | 1     | 0     |
| MemRead   | 0      | 1     | 0     | 0     | 0     | 0     |
| MemWrite  | 0      | 0     | 1     | 0     | 0     | 0     |
| Branch    | 0      | 0     | 0     | 1     | 0     | 0     |
| Jump      | 0      | 0     | 0     | 0     | 0     | 1     |
| ALUOp     | 10     | 00    | 00    | 01    | 00    | XX    |

**ALU control** — two-level decode:

```
ALUOp (from main control) + funct field (from instruction) → ALU operation

ALUOp=00 → ADD  (lw, sw, addi: address calculation)
ALUOp=01 → SUB  (beq: compare by subtraction)
ALUOp=10 → look at funct:
             funct=100000 → ADD
             funct=100010 → SUB
             funct=100100 → AND
             funct=100101 → OR
             funct=101010 → SLT
```

</details>

### 6.3 单周期时序问题

单周期设计的时钟周期等于最慢指令的耗时：

```
lw critical path:
  Instr Mem  +  Reg Read  +  ALU  +  Data Mem  +  Reg Write MUX
    200ps    +   150ps    + 200ps +   200ps    +     25ps
  = 775ps  →  max freq ≈ 1.29 GHz

But EVERY instruction — even a simple add (375ps) — takes 775ps.
  add waste:  775 - 375 = 400ps idle per add
```

这会在快速指令上浪费大量时间。解决办法：流水线。

### 6.4 五级流水线

将数据通路拆成 5 级，每级耗时一个时钟周期。流水线寄存器分隔各级：

```
Stage 1       Stage 2        Stage 3       Stage 4        Stage 5
  IF             ID             EX            MEM             WB
┌──────┐    ┌──────────┐   ┌────────┐    ┌────────┐    ┌──────────┐
│Instr │    │ Register │   │        │    │  Data  │    │  Write   │
│Fetch │ ►REG► Decode  │►REG► ALU  │►REG►Memory │►REG► Back    │
│      │    │ + Read   │   │        │    │        │    │          │
└──────┘    └──────────┘   └────────┘    └────────┘    └──────────┘
  PC+4       rs, rt, imm   ALU result   Mem data       rd ← result
  instr      control sigs  branch addr

Clock period = max(stage delay) + register overhead
             = 200ps + 20ps = 220ps  →  max freq ≈ 4.5 GHz
Throughput = 1 instruction per 220ps (vs 775ps single-cycle → 3.5× faster)
Latency = 5 × 220ps = 1100ps per instruction (worse, but throughput wins)
```

**流水线时序图 — 5 条指令流过：**

```
Time (cycles):  1     2     3     4     5     6     7     8     9
Instr 1:       IF    ID    EX    MEM   WB
Instr 2:             IF    ID    EX    MEM   WB
Instr 3:                   IF    ID    EX    MEM   WB
Instr 4:                         IF    ID    EX    MEM   WB
Instr 5:                               IF    ID    EX    MEM   WB

After cycle 5, one instruction completes EVERY cycle.
Steady-state CPI = 1 (Cycles Per Instruction).
```

### 6.5 流水线冒险

三类冒险会破坏理想的 CPI = 1：

#### 数据冒险

后面的指令读取前面指令尚未写入的寄存器：

```
add $s0, $t0, $t1    # WB writes $s0 in cycle 5
sub $t2, $s0, $t3    # ID reads $s0 in cycle 3 — STALE value!

Time:      1     2     3     4     5
add:      IF    ID    EX    MEM   WB ← $s0 written here
sub:            IF    ID ←reads $s0 here (old value!)
```

**方案 1 — 前递（旁路）：** 不等 WB，直接把 ALU 结果送回 ALU 输入：

```
                       ┌──────── forward path ────────┐
                       │                               │
add:   IF    ID    EX ─┤─ MEM    WB                    │
sub:         IF    ID  ─┘─ EX ◄──┘  MEM    WB
                           ↑
                    ALU gets $s0 from EX/MEM register
                    instead of register file

Forwarding unit checks: if (EX/MEM.rd == ID/EX.rs) → forward EX/MEM.ALUresult
```

**方案 2 — load-use 冒险（前递无法解决）：**

```
lw  $s0, 0($t0)    # data available after MEM (cycle 4)
add $t2, $s0, $t1  # EX needs $s0 in cycle 3 — too early!

Must stall (insert bubble) for 1 cycle:

Time:      1     2     3     4     5     6
lw:       IF    ID    EX    MEM   WB
add:            IF    ID   stall  EX    MEM    WB
                       ↑          ↑
                   hazard      forward from MEM/WB register
                  detected
```

停顿代价为 1 个周期。编译器在可能时重排指令以填充 load 延迟槽。

#### 控制冒险

分支结果要到 EX（或 MEM）级才可知，但下一条指令已经被取指：

```
beq $s0, $s1, target   # branch resolved in EX (cycle 3)
add ...                 # fetched in cycle 2 — might be wrong path!
or  ...                 # fetched in cycle 3 — might be wrong path!
```

**解决方案：**

| 策略           | 代价  | 硬件开销 | 说明                          |
|--------------------|----------|---------------|--------------------------------------|
| 停顿（冲刷）      | 1–2 周期| 极小      | 丢弃错误路径指令         |
| 分支预测  | 预测正确时为 0 | 中等  | 预测 taken/not-taken              |
| 延迟分支     | 0        | 编译器      | 总是执行下一条指令（MIPS） |
| 提前分支判定 | 1 周期 | ID 级 MUX | 将比较移到 ID 级          |

**静态预测：** 预测 not-taken（顺序取指）。若预测错误，冲刷 1 条指令。

**动态预测（分支历史表）：**

```
1-bit predictor: remember last outcome → predict same
  Problem: loops always mispredict twice (entering and exiting)

2-bit saturating counter:
  States: Strongly Taken (11) → Weakly Taken (10) → Weakly Not (01) → Strongly Not (00)
  Must mispredict twice to switch direction → better loop behavior
```


<details>
<summary>English original</summary>

**6.3 Single-Cycle Timing Problem**

The single-cycle design's clock period equals the slowest instruction:

```
lw critical path:
  Instr Mem  +  Reg Read  +  ALU  +  Data Mem  +  Reg Write MUX
    200ps    +   150ps    + 200ps +   200ps    +     25ps
  = 775ps  →  max freq ≈ 1.29 GHz

But EVERY instruction — even a simple add (375ps) — takes 775ps.
  add waste:  775 - 375 = 400ps idle per add
```

This wastes significant time on fast instructions. The solution: pipelining.

**6.4 Five-Stage Pipeline**

Break the datapath into 5 stages, each taking one clock cycle. Pipeline registers separate stages:

```
Stage 1       Stage 2        Stage 3       Stage 4        Stage 5
  IF             ID             EX            MEM             WB
┌──────┐    ┌──────────┐   ┌────────┐    ┌────────┐    ┌──────────┐
│Instr │    │ Register │   │        │    │  Data  │    │  Write   │
│Fetch │ ►REG► Decode  │►REG► ALU  │►REG►Memory │►REG► Back    │
│      │    │ + Read   │   │        │    │        │    │          │
└──────┘    └──────────┘   └────────┘    └────────┘    └──────────┘
  PC+4       rs, rt, imm   ALU result   Mem data       rd ← result
  instr      control sigs  branch addr

Clock period = max(stage delay) + register overhead
             = 200ps + 20ps = 220ps  →  max freq ≈ 4.5 GHz
Throughput = 1 instruction per 220ps (vs 775ps single-cycle → 3.5× faster)
Latency = 5 × 220ps = 1100ps per instruction (worse, but throughput wins)
```

**Pipeline timing diagram — 5 instructions flowing through:**

```
Time (cycles):  1     2     3     4     5     6     7     8     9
Instr 1:       IF    ID    EX    MEM   WB
Instr 2:             IF    ID    EX    MEM   WB
Instr 3:                   IF    ID    EX    MEM   WB
Instr 4:                         IF    ID    EX    MEM   WB
Instr 5:                               IF    ID    EX    MEM   WB

After cycle 5, one instruction completes EVERY cycle.
Steady-state CPI = 1 (Cycles Per Instruction).
```

**6.5 Pipeline Hazards**

Three types of hazards break the ideal CPI = 1:

**Data Hazards**

A later instruction reads a register that an earlier instruction hasn't written yet:

```
add $s0, $t0, $t1    # WB writes $s0 in cycle 5
sub $t2, $s0, $t3    # ID reads $s0 in cycle 3 — STALE value!

Time:      1     2     3     4     5
add:      IF    ID    EX    MEM   WB ← $s0 written here
sub:            IF    ID ←reads $s0 here (old value!)
```

**Solution 1 — Forwarding (bypassing):** route the ALU result directly back to the ALU input without waiting for WB:

```
                       ┌──────── forward path ────────┐
                       │                               │
add:   IF    ID    EX ─┤─ MEM    WB                    │
sub:         IF    ID  ─┘─ EX ◄──┘  MEM    WB
                           ↑
                    ALU gets $s0 from EX/MEM register
                    instead of register file

Forwarding unit checks: if (EX/MEM.rd == ID/EX.rs) → forward EX/MEM.ALUresult
```

**Solution 2 — Load-use hazard (forwarding can't fix):**

```
lw  $s0, 0($t0)    # data available after MEM (cycle 4)
add $t2, $s0, $t1  # EX needs $s0 in cycle 3 — too early!

Must stall (insert bubble) for 1 cycle:

Time:      1     2     3     4     5     6
lw:       IF    ID    EX    MEM   WB
add:            IF    ID   stall  EX    MEM    WB
                       ↑          ↑
                   hazard      forward from MEM/WB register
                  detected
```

The stall costs 1 cycle. Compilers reorder instructions to fill the load-delay slot when possible.

**Control Hazards**

Branch outcome isn't known until EX (or MEM), but the next instruction is already fetched:

```
beq $s0, $s1, target   # branch resolved in EX (cycle 3)
add ...                 # fetched in cycle 2 — might be wrong path!
or  ...                 # fetched in cycle 3 — might be wrong path!
```

**Solutions:**

| Strategy           | Penalty  | Hardware cost | Description                          |
|--------------------|----------|---------------|--------------------------------------|
| Stall (flush)      | 1–2 cycles| Minimal      | Kill wrong-path instructions         |
| Branch prediction  | 0 if correct | Moderate  | Predict taken/not-taken              |
| Delayed branch     | 0        | Compiler      | Always execute next instruction (MIPS) |
| Early branch resolution | 1 cycle | MUX in ID | Move comparison to ID stage          |

**Static prediction:** predict not-taken (sequential fetch). If wrong, flush 1 instruction.

**Dynamic prediction (branch history table):**

```
1-bit predictor: remember last outcome → predict same
  Problem: loops always mispredict twice (entering and exiting)

2-bit saturating counter:
  States: Strongly Taken (11) → Weakly Taken (10) → Weakly Not (01) → Strongly Not (00)
  Must mispredict twice to switch direction → better loop behavior
```

</details>

#### 结构冒险

两个阶段在同一周期需要同一个硬件单元（例如 IF 与 MEM 共用单个存储器）：

```
Solution: use separate instruction memory and data memory (Harvard architecture)
          or use a multi-ported cache
```

经典的 MIPS 流水线使用独立的指令存储器和数据存储器——基础设计中不存在结构冒险。

### 6.6 完整的流水线数据通路

```
 ┌─────┐  IF/ID  ┌─────┐  ID/EX  ┌─────┐ EX/MEM ┌─────┐ MEM/WB ┌─────┐
 │     │  reg    │     │  reg    │     │  reg   │     │  reg   │     │
 │ IF  ├────────►│ ID  ├────────►│ EX  ├───────►│ MEM ├───────►│ WB  │
 │     │         │     │         │     │        │     │        │     │
 └──┬──┘         └──┬──┘         └──┬──┘        └─────┘        └──┬──┘
    │               │               │                              │
    │            ┌──┴──┐         ┌──┴──┐                           │
    │            │Hazard│        │Fwd  │                           │
    │            │Detect│        │Unit │◄──────────────────────────┘
    │            └──┬──┘        └─────┘         (forward paths)
    │               │
    │          stall/flush
    │
  ┌─┴──────────────────────────┐
  │   PC MUX                    │
  │   (PC+4 / branch / jump)   │
  └────────────────────────────┘

Pipeline registers store ALL signals needed by later stages:
  IF/ID:  instruction, PC+4
  ID/EX:  control signals, rs data, rt data, sign-ext imm, rd/rt dest
  EX/MEM: control signals, ALU result, rt data (for sw), dest register
  MEM/WB: control signals, ALU result OR memory data, dest register
```

**前递单元逻辑（简化版）：**

```
// Forward from EX/MEM stage
if (EX/MEM.RegWrite && EX/MEM.rd != 0 && EX/MEM.rd == ID/EX.rs)
    ForwardA = EX/MEM.ALUresult

// Forward from MEM/WB stage
if (MEM/WB.RegWrite && MEM/WB.rd != 0 && MEM/WB.rd == ID/EX.rs
    && !(EX/MEM.RegWrite && EX/MEM.rd == ID/EX.rs))   // EX/MEM has priority
    ForwardA = MEM/WB.WriteData

// Same logic for ForwardB (rt input)
```

**冒险检测单元（load-use 停顿）：**

```
if (ID/EX.MemRead && (ID/EX.rt == IF/ID.rs || ID/EX.rt == IF/ID.rt))
    stall pipeline:
      - IF/ID register holds (re-decode same instruction)
      - insert NOP into ID/EX (bubble)
      - PC holds (re-fetch same PC+4)
```

### 6.7 性能指标

```
Execution time = IC × CPI × T_clk

  IC    = Instruction Count (depends on ISA and compiler)
  CPI   = Cycles Per Instruction (depends on pipeline + hazards)
  T_clk = Clock period (depends on critical path in longest stage)

Single-cycle:  CPI = 1,  T_clk = 775ps    →  time/instr = 775ps
Pipelined:     CPI ≈ 1.2 (stalls + flushes), T_clk = 220ps
               →  time/instr = 264ps  →  ~2.9× speedup

Ideal speedup from N-stage pipeline = N (limited by hazard stalls)
5-stage MIPS: ideal 5×, typical 3–4× in practice
```

**CPI 分解示例（MIPS 上的 gcc）：**

| 组成部分      | CPI 贡献 |
|----------------|-----------------|
| 基准            | 1.0            |
| load-use 停顿 | +0.05         |
| 分支预测错误 | +0.12      |
| 缓存缺失    | +0.07         |
| **总 CPI**   | **1.24**      |

> **AI 硬件关联：** GPU shader core 的流水线深得多（现代 GPU 中 20+ 级），但它靠大规模线程级并行（TLP）而非分支预测来隐藏延迟。当某个 warp 因缓存缺失而停顿时，调度器切换到另一个就绪的 warp——与流水线交错是同一原理，只是作用于数千个线程而非单条指令。MIPS 5 级流水线是概念基础：一旦理解了前递、冒险和 CPI，GPU 的做法就是自然的延伸。

---

## 资源

| 资源 | 类型 | 关注点 |
|----------|------|-------|
| *Digital Design and Computer Architecture* — Harris & Harris | 教材 | 从门级到 ISA，含 HDL 示例 |
| *Computer Organization and Design* — Patterson & Hennessy | 教材 | RISC-V、存储层次、流水线 |
| *Digital Design and Computer Architecture: ARM Edition* — Harris & Harris | 教材 | 单周期 + 流水线处理器、HDL |
| *Modern VLSI Design* — Wolf | 教材 | CMOS、时序、布局布线 |
| *CMOS VLSI Design* — Weste & Harris | 教材 | 晶体管级设计、版图、制造 |
| Sam Zeloof（YouTube） | 视频 | DIY 半导体制造——亲手看整个工艺 |
| Chips and Cheese（chipsandcheese.com） | 博客 | 深入剖析真实芯片微架构 |
| Nandland（nandland.com） | 在线 | FPGA/Verilog/VHDL 动手实践 |
| EEVblog / Ben Eater | 视频 | 数字电路的直觉、面包板 CPU |
| JEDEC JESD79-5B（DDR5 规范） | 规范 | DRAM 时序参数 |
| JEDEC HBM3 规范 | 规范 | HBM 架构与接口 |
| IEEE 754-2019 | 标准 | 浮点标准 |

---


<details>
<summary>English original</summary>

**Structural Hazards**

Two stages need the same hardware unit in the same cycle (e.g., single memory for both IF and MEM):

```
Solution: use separate instruction memory and data memory (Harvard architecture)
          or use a multi-ported cache
```

The classic MIPS pipeline uses separate instruction and data memories — no structural hazards in the base design.

**6.6 Complete Pipelined Datapath**

```
 ┌─────┐  IF/ID  ┌─────┐  ID/EX  ┌─────┐ EX/MEM ┌─────┐ MEM/WB ┌─────┐
 │     │  reg    │     │  reg    │     │  reg   │     │  reg   │     │
 │ IF  ├────────►│ ID  ├────────►│ EX  ├───────►│ MEM ├───────►│ WB  │
 │     │         │     │         │     │        │     │        │     │
 └──┬──┘         └──┬──┘         └──┬──┘        └─────┘        └──┬──┘
    │               │               │                              │
    │            ┌──┴──┐         ┌──┴──┐                           │
    │            │Hazard│        │Fwd  │                           │
    │            │Detect│        │Unit │◄──────────────────────────┘
    │            └──┬──┘        └─────┘         (forward paths)
    │               │
    │          stall/flush
    │
  ┌─┴──────────────────────────┐
  │   PC MUX                    │
  │   (PC+4 / branch / jump)   │
  └────────────────────────────┘

Pipeline registers store ALL signals needed by later stages:
  IF/ID:  instruction, PC+4
  ID/EX:  control signals, rs data, rt data, sign-ext imm, rd/rt dest
  EX/MEM: control signals, ALU result, rt data (for sw), dest register
  MEM/WB: control signals, ALU result OR memory data, dest register
```

**Forwarding unit logic (simplified):**

```
// Forward from EX/MEM stage
if (EX/MEM.RegWrite && EX/MEM.rd != 0 && EX/MEM.rd == ID/EX.rs)
    ForwardA = EX/MEM.ALUresult

// Forward from MEM/WB stage
if (MEM/WB.RegWrite && MEM/WB.rd != 0 && MEM/WB.rd == ID/EX.rs
    && !(EX/MEM.RegWrite && EX/MEM.rd == ID/EX.rs))   // EX/MEM has priority
    ForwardA = MEM/WB.WriteData

// Same logic for ForwardB (rt input)
```

**Hazard detection unit (load-use stall):**

```
if (ID/EX.MemRead && (ID/EX.rt == IF/ID.rs || ID/EX.rt == IF/ID.rt))
    stall pipeline:
      - IF/ID register holds (re-decode same instruction)
      - insert NOP into ID/EX (bubble)
      - PC holds (re-fetch same PC+4)
```

**6.7 Performance Metrics**

```
Execution time = IC × CPI × T_clk

  IC    = Instruction Count (depends on ISA and compiler)
  CPI   = Cycles Per Instruction (depends on pipeline + hazards)
  T_clk = Clock period (depends on critical path in longest stage)

Single-cycle:  CPI = 1,  T_clk = 775ps    →  time/instr = 775ps
Pipelined:     CPI ≈ 1.2 (stalls + flushes), T_clk = 220ps
               →  time/instr = 264ps  →  ~2.9× speedup

Ideal speedup from N-stage pipeline = N (limited by hazard stalls)
5-stage MIPS: ideal 5×, typical 3–4× in practice
```

**CPI breakdown example (gcc on MIPS):**

| Component      | CPI contribution |
|----------------|-----------------|
| Base            | 1.0            |
| Load-use stalls | +0.05         |
| Branch mispredicts | +0.12      |
| Cache misses    | +0.07         |
| **Total CPI**   | **1.24**      |

> **AI hardware connection:** GPU shader cores use a much deeper pipeline (20+ stages in modern GPUs) but hide latency through massive thread-level parallelism (TLP) rather than branch prediction. When a warp stalls on a cache miss, the scheduler switches to another ready warp — same principle as pipeline interleaving, but across thousands of threads instead of individual instructions. The MIPS 5-stage pipeline is the conceptual foundation: once you understand forwarding, hazards, and CPI, the GPU's approach is a natural extension.

---

**Resources**

| Resource | Type | Focus |
|----------|------|-------|
| *Digital Design and Computer Architecture* — Harris & Harris | Textbook | Gates through ISA, HDL examples |
| *Computer Organization and Design* — Patterson & Hennessy | Textbook | RISC-V, memory hierarchy, pipelining |
| *Digital Design and Computer Architecture: ARM Edition* — Harris & Harris | Textbook | Single-cycle + pipelined processor, HDL |
| *Modern VLSI Design* — Wolf | Textbook | CMOS, timing, place-and-route |
| *CMOS VLSI Design* — Weste & Harris | Textbook | Transistor-level design, layout, fabrication |
| Sam Zeloof (YouTube) | Video | DIY semiconductor fab — see the process hands-on |
| Chips and Cheese (chipsandcheese.com) | Blog | Deep dives into real chip microarchitectures |
| Nandland (nandland.com) | Online | FPGA/Verilog/VHDL hands-on |
| EEVblog / Ben Eater | Video | Intuition for digital circuits, breadboard CPU |
| JEDEC JESD79-5B (DDR5 spec) | Spec | DRAM timing parameters |
| JEDEC HBM3 spec | Spec | HBM architecture and interface |
| IEEE 754-2019 | Standard | Floating-point standard |

---

</details>

## 项目

| 项目 | 技能 | 成果 |
|---------|--------|---------|
| **Binary/BCD/Gray code converter** | 数制、卡诺图 | 用 Verilog 实现组合逻辑 |
| **32-bit CLA adder in Verilog** | 进位生成/传播、前缀树 | RTL 设计 + 时序分析 |
| **IEEE 754 FP32 adder** | 对齐移位、规格化、舍入 | 理解浮点硬件 |
| **INT8 MAC unit** | 定点、二进制补码、累加 | 量化推理的核心 |
| **LFSR-based PRNG** | 移位寄存器、XOR 反馈、多项式 | BIST、测试激励生成 |
| **4-state FSM (Moore + Mealy)** | 状态编码、次态逻辑 | 控制单元设计模式 |
| **Tiled matrix multiply (CUDA)** | 共享内存、bank 冲突、分块 | 把理论与 GPU 编程联系起来 |
| **Single-cycle MIPS in Verilog** | 数据通路、控制单元、ALU、寄存器堆 | 从门级到 ISA 的完整处理器 |
| **Pipelined MIPS in Verilog** | 流水线寄存器、前递、冒险检测 | 理解 CPI、停顿、旁路 |
| **Cache simulator (Python/C++)** | 直接映射 vs 组相联、LRU、缺失率 | 存储层次的直觉 |
| **RTL-to-GDS with OpenLane** | 综合、PnR、DRC/LVS、SkyWater PDK | 体验完整的流片流程（开源） |


<details>
<summary>English original</summary>

**Projects**

| Project | Skills | Outcome |
|---------|--------|---------|
| **Binary/BCD/Gray code converter** | Number systems, K-map | Combinational logic in Verilog |
| **32-bit CLA adder in Verilog** | Generate/propagate, prefix tree | RTL design + timing analysis |
| **IEEE 754 FP32 adder** | Alignment shift, normalization, rounding | Understand FP hardware |
| **INT8 MAC unit** | Fixed-point, two's complement, accumulation | Core of quantized inference |
| **LFSR-based PRNG** | Shift registers, XOR feedback, polynomial | BIST, test stimulus generation |
| **4-state FSM (Moore + Mealy)** | State encoding, next-state logic | Control unit design pattern |
| **Tiled matrix multiply (CUDA)** | Shared memory, bank conflict, tiling | Connect theory to GPU programming |
| **Single-cycle MIPS in Verilog** | Datapath, control unit, ALU, register file | Complete processor from gates to ISA |
| **Pipelined MIPS in Verilog** | Pipeline registers, forwarding, hazard detection | Understand CPI, stalls, bypassing |
| **Cache simulator (Python/C++)** | Direct-map vs set-assoc, LRU, miss rates | Memory hierarchy intuition |
| **RTL-to-GDS with OpenLane** | Synthesis, PnR, DRC/LVS, SkyWater PDK | Experience the full tapeout flow (open-source) |

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/1. Digital Design and Hardware Description Languages/Digital Design Fundamentals/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/1.%20Digital%20Design%20and%20Hardware%20Description%20Languages/Digital%20Design%20Fundamentals/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
