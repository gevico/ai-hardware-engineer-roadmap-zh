---
title: 硬件描述语言 — Verilog 聚焦
description: 硬件描述语言 — Verilog 聚焦
---

# 硬件描述语言 — Verilog 聚焦

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">HDLV</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 数字基础</p>
<p class="course-identity__title">硬件描述语言 — Verilog 聚焦的专项课程标识。</p>
<p class="course-identity__meta">产物：可运行的低层 demo · 度量：时序、内存、正确性</p>
</div>
</div>


Verilog 是将上一份指南中的数字设计概念转化为真实硬件的方法。你编写行为级或结构级描述；综合工具将它们转换成门电路；布局布线工具再把这些门电路变成物理版图。本指南通过逐步增大的设计来教授 Verilog — 从 wire 和门电路到流水线 MIPS 核心 — 每一步都有 testbench。

> **为什么选 Verilog 而不是 VHDL？** 两者都是 IEEE 标准且功能完备。Verilog 在美国产业界以及所有主要 AI 芯片公司（NVIDIA、AMD、Google、Apple）中占主导。VHDL 在欧洲国防/航空航天领域更强。SystemVerilog 用验证特性（UVM、断言）扩展 Verilog，将在阶段 2 中介绍。从 Verilog 开始 — 掌握一种 HDL 后，一个周末就能读懂 VHDL。

---

## 1. Verilog 基础

### 1.1 模块和端口

**模块**是基本构建单元 — 它有名称、端口和内部逻辑。把它想象成带有标注引脚的芯片。

```verilog
module adder (
    input  wire [3:0] a,      // 4-bit input
    input  wire [3:0] b,      // 4-bit input
    input  wire       cin,    // carry in
    output wire [3:0] sum,    // 4-bit sum
    output wire       cout    // carry out
);
    assign {cout, sum} = a + b + cin;
endmodule
```

**端口方向：** `input`、`output`、`inout`（双向 — 少见，主要用于焊盘）。

**端口类型：** `wire`（输入/输出的默认类型）或 `reg`（对由 `always` 块内部驱动的输出是必需的 — 尽管名字如此，`reg` 并不总是表示寄存器）。

### 1.2 数据类型

| 类型        | 表示什么                           | 驱动来源                      |
|-------------|----------------------------------------------|--------------------------------|
| `wire`      | 物理连接（组合逻辑）          | `assign`、模块输出、门  |
| `reg`       | 存储元件或组合逻辑变量    | `always`、`initial`            |
| `integer`   | 32 位有符号，用于循环/testbench         | 不能综合为硬件  |
| `parameter` | 命名常量                               | 在编译/实例化时设置   |
| `localparam`| 命名常量，不可覆盖         | 模块内部             |

**向量** — 多比特信号：

```verilog
wire [7:0]  data_bus;        // 8-bit bus, data_bus[7] is MSB
reg  [31:0] instruction;     // 32-bit register
wire [0:7]  reversed;        // bit 0 is MSB (uncommon, avoid)
```

**位选择与部分选择：**

```verilog
wire [31:0] instr;
wire [5:0]  opcode = instr[31:26];   // bits 31 down to 26
wire [4:0]  rs     = instr[25:21];
wire [4:0]  rt     = instr[20:16];
```

**存储器（寄存器数组）：**

```verilog
reg [7:0] mem [0:255];       // 256 bytes of memory
reg [31:0] reg_file [0:31];  // 32-entry × 32-bit register file
```

### 1.3 数字字面量

格式：`<width>'<radix><value>`

```verilog
8'b1010_0011     // 8-bit binary = 0xA3 (underscores for readability)
8'hA3            // 8-bit hex = 163
8'd163           // 8-bit decimal
32'hDEAD_BEEF    // 32-bit hex
4'b1             // 4-bit = 4'b0001 (zero-padded)
-8'd5            // 8-bit negative = two's complement of 5
```

**四值逻辑：** 每一位可以是 `0`、`1`、`x`（未知）或 `z`（高阻/未驱动）。

```verilog
4'bxx01          // upper 2 bits unknown
8'bzzzz_zzzz     // tri-state (all high-Z)
```

`x` 传播是仿真 bug 的头号来源。如果在波形中看到 `x`，说明有东西未驱动或未初始化 — 定位它。


<details>
<summary>English original</summary>

**Hardware Description Languages — Verilog Focus**

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">HDLV</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Digital Foundations</p>
<p class="course-identity__title">Specialized course identity for Hardware Description Languages — Verilog Focus.</p>
<p class="course-identity__meta">Artifact: working low-level demo · Measure: timing, memory, correctness</p>
</div>
</div>


Verilog is how you turn the digital design concepts from the previous guide into real hardware. You write behavioral or structural descriptions; synthesis tools convert them into gates; place-and-route tools turn those gates into physical layout. This guide teaches Verilog through progressively larger designs — from wires and gates to a pipelined MIPS core — with testbenches at every step.

> **Why Verilog over VHDL?** Both are IEEE standards and fully capable. Verilog dominates in US industry and all major AI chip companies (NVIDIA, AMD, Google, Apple). VHDL is stronger in European defense/aerospace. SystemVerilog extends Verilog with verification features (UVM, assertions) and is covered in Phase 2. Start with Verilog — you can read VHDL in a weekend once you know one HDL.

---

**1. Verilog Fundamentals**

**1.1 Modules and Ports**

A **module** is the basic building block — it has a name, ports, and internal logic. Think of it as a chip with labeled pins.

```verilog
module adder (
    input  wire [3:0] a,      // 4-bit input
    input  wire [3:0] b,      // 4-bit input
    input  wire       cin,    // carry in
    output wire [3:0] sum,    // 4-bit sum
    output wire       cout    // carry out
);
    assign {cout, sum} = a + b + cin;
endmodule
```

**Port directions:** `input`, `output`, `inout` (bidirectional — rare, mostly for pads).

**Port types:** `wire` (default for inputs/outputs) or `reg` (required for outputs driven inside `always` blocks — despite the name, `reg` does NOT always mean a register).

**1.2 Data Types**

| Type        | What it represents                           | Driven by                      |
|-------------|----------------------------------------------|--------------------------------|
| `wire`      | Physical connection (combinational)          | `assign`, module output, gate  |
| `reg`       | Storage element OR combinational variable    | `always`, `initial`            |
| `integer`   | 32-bit signed, for loops/testbenches         | Not synthesizable as hardware  |
| `parameter` | Named constant                               | Set at compile/instantiation   |
| `localparam`| Named constant, cannot be overridden         | Internal to module             |

**Vectors** — multi-bit signals:

```verilog
wire [7:0]  data_bus;        // 8-bit bus, data_bus[7] is MSB
reg  [31:0] instruction;     // 32-bit register
wire [0:7]  reversed;        // bit 0 is MSB (uncommon, avoid)
```

**Bit-select and part-select:**

```verilog
wire [31:0] instr;
wire [5:0]  opcode = instr[31:26];   // bits 31 down to 26
wire [4:0]  rs     = instr[25:21];
wire [4:0]  rt     = instr[20:16];
```

**Memory (array of registers):**

```verilog
reg [7:0] mem [0:255];       // 256 bytes of memory
reg [31:0] reg_file [0:31];  // 32-entry × 32-bit register file
```

**1.3 Number Literals**

Format: `<width>'<radix><value>`

```verilog
8'b1010_0011     // 8-bit binary = 0xA3 (underscores for readability)
8'hA3            // 8-bit hex = 163
8'd163           // 8-bit decimal
32'hDEAD_BEEF    // 32-bit hex
4'b1             // 4-bit = 4'b0001 (zero-padded)
-8'd5            // 8-bit negative = two's complement of 5
```

**Four-value logic:** every bit can be `0`, `1`, `x` (unknown), or `z` (high-impedance/undriven).

```verilog
4'bxx01          // upper 2 bits unknown
8'bzzzz_zzzz     // tri-state (all high-Z)
```

`x` propagation is the #1 source of simulation bugs. If you see `x` in your waveform, something is undriven or uninitialized — track it down.

</details>

### 1.4 算子

| 类别     | 算子                             | 说明                          |
|-------------|---------------------------------------|--------------------------------|
| 算术  | `+`, `-`, `*`, `/`, `%`              | `/` 与 `%` 可能无法综合 |
| 关系  | `<`, `>`, `<=`, `>=`                 | 结果为 1 bit               |
| 相等    | `==`, `!=`                            | 任一操作数为 `x` → 结果为 `x` |
| 全等比较  | `===`, `!==`                         | 按字面比较 x/z（仅仿真） |
| 逻辑     | `&&`, `\|\|`, `!`                    | 结果为 1 bit               |
| 按位     | `&`, `\|`, `^`, `~`, `~^`           | 逐位                    |
| 归约   | `&a`, `\|a`, `^a`, `~&a`, `~\|a`    | 将向量归约为 1 bit        |
| 移位       | `<<`, `>>`, `<<<`, `>>>`            | `>>>` 为算术移位（符号扩展） |
| 拼接 | `{a, b}`, `{4{a}}`                | `{4{1'b0}}` = `4'b0000`      |
| 条件 | `cond ? a : b`                       | 综合为 MUX             |

**归约算子示例** —— parity：

```verilog
wire [7:0] data;
wire parity = ^data;    // XOR all 8 bits → 1 if odd number of 1s
```

**拼接与复制 —— 符号扩展：**

```verilog
wire [15:0] imm16;
wire [31:0] sign_ext = {{16{imm16[15]}}, imm16};  // replicate sign bit 16 times
```

---

## 2. 建模风格

Verilog 提供三种描述硬件的方式。三者可以在同一个模块中混用。

### 2.1 dataflow 建模（assign）

连续赋值描述组合逻辑 —— 任一输入变化时输出即更新。

```verilog
module mux2 #(parameter WIDTH = 8) (
    input  wire [WIDTH-1:0] d0, d1,
    input  wire             sel,
    output wire [WIDTH-1:0] y
);
    assign y = sel ? d1 : d0;
endmodule
```

`?:` 算子在硬件中直接映射为 2:1 MUX。级联的 `?:` 构成优先级 MUX 链。

**dataflow 风格的全加器：**

```verilog
module full_adder (
    input  wire a, b, cin,
    output wire sum, cout
);
    assign sum  = a ^ b ^ cin;
    assign cout = (a & b) | (cin & (a ^ b));
endmodule
```

### 2.2 行为建模（always 块）

`always` 块描述事件发生时做什么 —— 时序逻辑看时钟边沿，组合逻辑看信号变化。

**组合逻辑 —— `always @(*)`：**

```verilog
module alu (
    input  wire [31:0] a, b,
    input  wire [2:0]  op,
    output reg  [31:0] result,
    output reg         zero
);
    always @(*) begin
        case (op)
            3'b000: result = a & b;       // AND
            3'b001: result = a | b;       // OR
            3'b010: result = a + b;       // ADD
            3'b110: result = a - b;       // SUB
            3'b111: result = (a < b) ? 32'd1 : 32'd0;  // SLT
            default: result = 32'bx;      // undefined
        endcase
        zero = (result == 32'd0);
    end
endmodule
```

**组合 `always @(*)` 的关键规则：**
- 使用 `always @(*)` —— `*` 会自动把所有被读取的信号加入敏感列表
- 在每一个分支中给每一个输出赋值（或使用 `default`）—— 赋值不完整会产生 latch（非预期的存储器）
- 使用阻塞赋值 `=`（按顺序执行，类似软件）

**时序逻辑 —— `always @(posedge clk)`：**

```verilog
module register #(parameter WIDTH = 32) (
    input  wire             clk,
    input  wire             rst,
    input  wire             en,
    input  wire [WIDTH-1:0] d,
    output reg  [WIDTH-1:0] q
);
    always @(posedge clk or posedge rst) begin
        if (rst)
            q <= {WIDTH{1'b0}};      // async reset
        else if (en)
            q <= d;                   // load on enable
        // else: q holds (implicit — this is a register)
    end
endmodule
```

**时序 `always @(posedge clk)` 的关键规则：**
- 使用非阻塞赋值 `<=`（先对所有右侧表达式求值，再更新所有左侧表达式 —— 模拟真实触发器行为）
- 复位判断置于最前（`if (rst)`）—— 可为异步（敏感列表中带 `posedge rst`）或同步（敏感列表中不带 `rst`）
- 省略 else 分支没问题 —— 表示「保持该值」（寄存器）

**阻塞（`=`）与非阻塞（`<=`）—— 关键区别：**

```verilog
// WRONG — blocking in sequential logic:
always @(posedge clk) begin
    b = a;     // b gets a immediately
    c = b;     // c gets the NEW b (= a)  → b and c both get a!
end

// CORRECT — non-blocking in sequential logic:
always @(posedge clk) begin
    b <= a;    // scheduled: b will get old a
    c <= b;    // scheduled: c will get old b  → shift register!
end
```

规则：**在时钟块中用 `<=`，在组合块中用 `=`。** 绝不可混用。


<details>
<summary>English original</summary>

**1.4 Operators**

| Category     | Operators                             | Notes                          |
|-------------|---------------------------------------|--------------------------------|
| Arithmetic  | `+`, `-`, `*`, `/`, `%`              | `/` and `%` may not synthesize |
| Relational  | `<`, `>`, `<=`, `>=`                 | Result is 1-bit               |
| Equality    | `==`, `!=`                            | `x` in either operand → `x` result |
| Case equal  | `===`, `!==`                         | Compares x/z literally (sim only) |
| Logical     | `&&`, `\|\|`, `!`                    | Result is 1-bit               |
| Bitwise     | `&`, `\|`, `^`, `~`, `~^`           | Bit-by-bit                    |
| Reduction   | `&a`, `\|a`, `^a`, `~&a`, `~\|a`    | Reduce vector to 1 bit        |
| Shift       | `<<`, `>>`, `<<<`, `>>>`            | `>>>` is arithmetic (sign-extend) |
| Concatenation | `{a, b}`, `{4{a}}`                | `{4{1'b0}}` = `4'b0000`      |
| Conditional | `cond ? a : b`                       | Synthesizes to MUX             |

**Reduction operator example** — parity:

```verilog
wire [7:0] data;
wire parity = ^data;    // XOR all 8 bits → 1 if odd number of 1s
```

**Concatenation and replication — sign extension:**

```verilog
wire [15:0] imm16;
wire [31:0] sign_ext = {{16{imm16[15]}}, imm16};  // replicate sign bit 16 times
```

---

**2. Modeling Styles**

Verilog gives you three ways to describe hardware. All three can be mixed in one module.

**2.1 Dataflow Modeling (assign)**

Continuous assignments describe combinational logic — the output updates whenever any input changes.

```verilog
module mux2 #(parameter WIDTH = 8) (
    input  wire [WIDTH-1:0] d0, d1,
    input  wire             sel,
    output wire [WIDTH-1:0] y
);
    assign y = sel ? d1 : d0;
endmodule
```

The `?:` operator maps directly to a 2:1 MUX in hardware. Chained `?:` becomes a priority MUX chain.

**Full adder in dataflow:**

```verilog
module full_adder (
    input  wire a, b, cin,
    output wire sum, cout
);
    assign sum  = a ^ b ^ cin;
    assign cout = (a & b) | (cin & (a ^ b));
endmodule
```

**2.2 Behavioral Modeling (always blocks)**

`always` blocks describe what happens on events — clock edges for sequential, signal changes for combinational.

**Combinational logic — `always @(*)`:**

```verilog
module alu (
    input  wire [31:0] a, b,
    input  wire [2:0]  op,
    output reg  [31:0] result,
    output reg         zero
);
    always @(*) begin
        case (op)
            3'b000: result = a & b;       // AND
            3'b001: result = a | b;       // OR
            3'b010: result = a + b;       // ADD
            3'b110: result = a - b;       // SUB
            3'b111: result = (a < b) ? 32'd1 : 32'd0;  // SLT
            default: result = 32'bx;      // undefined
        endcase
        zero = (result == 32'd0);
    end
endmodule
```

**Key rules for combinational `always @(*)`:**
- Use `always @(*)` — the `*` auto-includes all read signals in the sensitivity list
- Assign EVERY output in EVERY branch (or use `default`) — incomplete assignment creates a latch (unintended memory)
- Use blocking assignment `=` (executes in order, like software)

**Sequential logic — `always @(posedge clk)`:**

```verilog
module register #(parameter WIDTH = 32) (
    input  wire             clk,
    input  wire             rst,
    input  wire             en,
    input  wire [WIDTH-1:0] d,
    output reg  [WIDTH-1:0] q
);
    always @(posedge clk or posedge rst) begin
        if (rst)
            q <= {WIDTH{1'b0}};      // async reset
        else if (en)
            q <= d;                   // load on enable
        // else: q holds (implicit — this is a register)
    end
endmodule
```

**Key rules for sequential `always @(posedge clk)`:**
- Use non-blocking assignment `<=` (all right-hand sides evaluated first, then all left-hand sides updated — models real flip-flop behavior)
- Reset comes first (`if (rst)`) — can be async (`posedge rst` in sensitivity) or sync (no `rst` in sensitivity)
- Omitting an else branch is fine — it means "hold the value" (register)

**Blocking (`=`) vs non-blocking (`<=`) — the critical distinction:**

```verilog
// WRONG — blocking in sequential logic:
always @(posedge clk) begin
    b = a;     // b gets a immediately
    c = b;     // c gets the NEW b (= a)  → b and c both get a!
end

// CORRECT — non-blocking in sequential logic:
always @(posedge clk) begin
    b <= a;    // scheduled: b will get old a
    c <= b;    // scheduled: c will get old b  → shift register!
end
```

Rule: **use `<=` in clocked blocks, `=` in combinational blocks.** Never mix them.

</details>

### 2.3 结构建模（instantiation）

通过连接模块来构建层次结构：

```verilog
module ripple_carry_4 (
    input  wire [3:0] a, b,
    input  wire       cin,
    output wire [3:0] sum,
    output wire       cout
);
    wire c1, c2, c3;

    full_adder fa0 (.a(a[0]), .b(b[0]), .cin(cin),  .sum(sum[0]), .cout(c1));
    full_adder fa1 (.a(a[1]), .b(b[1]), .cin(c1),   .sum(sum[1]), .cout(c2));
    full_adder fa2 (.a(a[2]), .b(b[2]), .cin(c2),   .sum(sum[2]), .cout(c3));
    full_adder fa3 (.a(a[3]), .b(b[3]), .cin(c3),   .sum(sum[3]), .cout(cout));
endmodule
```

**始终使用命名端口连接**（`.port(signal)`）—— 按位置连接易出错且可读性差。

### 2.4 Generate 块

`generate` 在 elaboration 阶段创建硬件 —— 参数化、可扩展的结构：

```verilog
module ripple_carry #(parameter N = 32) (
    input  wire [N-1:0] a, b,
    input  wire         cin,
    output wire [N-1:0] sum,
    output wire         cout
);
    wire [N:0] carry;
    assign carry[0] = cin;
    assign cout = carry[N];

    genvar i;
    generate
        for (i = 0; i < N; i = i + 1) begin : fa_stage
            full_adder fa (
                .a(a[i]), .b(b[i]), .cin(carry[i]),
                .sum(sum[i]), .cout(carry[i+1])
            );
        end
    endgenerate
endmodule
```

`generate for` 在编译期展开 —— 它创建 N 个独立的 `full_adder` 实例，而不是在 runtime 运行的循环。

---

## 3. 组合逻辑设计模式

### 3.1 译码器

```verilog
module decoder_2to4 (
    input  wire [1:0] in,
    input  wire       en,
    output reg  [3:0] out
);
    always @(*) begin
        out = 4'b0000;
        if (en)
            case (in)
                2'b00: out = 4'b0001;
                2'b01: out = 4'b0010;
                2'b10: out = 4'b0100;
                2'b11: out = 4'b1000;
            endcase
    end
endmodule
```

### 3.2 优先级编码器

```verilog
module priority_enc (
    input  wire [7:0] req,
    output reg  [2:0] grant,
    output reg        valid
);
    always @(*) begin
        valid = 1'b1;
        casez (req)                         // casez treats z/? as don't-care
            8'b1???_????: grant = 3'd7;
            8'b01??_????: grant = 3'd6;
            8'b001?_????: grant = 3'd5;
            8'b0001_????: grant = 3'd4;
            8'b0000_1???: grant = 3'd3;
            8'b0000_01??: grant = 3'd2;
            8'b0000_001?: grant = 3'd1;
            8'b0000_0001: grant = 3'd0;
            default: begin grant = 3'd0; valid = 1'b0; end
        endcase
    end
endmodule
```

### 3.3 参数化 MUX

```verilog
module mux4 #(parameter W = 32) (
    input  wire [W-1:0] d0, d1, d2, d3,
    input  wire [1:0]   sel,
    output reg  [W-1:0] y
);
    always @(*) begin
        case (sel)
            2'b00: y = d0;
            2'b01: y = d1;
            2'b10: y = d2;
            2'b11: y = d3;
        endcase
    end
endmodule
```

### 3.4 带标志位的 ALU

一个完整的 ALU，与 MIPS 数据通路中的一致：

```verilog
module alu_32 (
    input  wire [31:0] a, b,
    input  wire [3:0]  alu_ctrl,
    output reg  [31:0] result,
    output wire        zero,
    output wire        negative,
    output wire        overflow,
    output wire        carry_out
);
    reg [32:0] tmp;    // 33-bit for carry detection

    always @(*) begin
        tmp = 33'd0;
        case (alu_ctrl)
            4'b0000: tmp = {1'b0, a & b};                    // AND
            4'b0001: tmp = {1'b0, a | b};                    // OR
            4'b0010: tmp = {1'b0, a} + {1'b0, b};           // ADD
            4'b0110: tmp = {1'b0, a} + {1'b0, ~b} + 33'd1;  // SUB
            4'b0111: begin                                    // SLT
                tmp = {1'b0, a} + {1'b0, ~b} + 33'd1;
                tmp = {32'd0, tmp[32] ^ overflow};  // signed less than
            end
            4'b1100: tmp = {1'b0, ~(a | b)};                 // NOR
            4'b1000: tmp = {1'b0, a ^ b};                    // XOR
            default: tmp = 33'd0;
        endcase
        result = tmp[31:0];
    end

    assign zero     = (result == 32'd0);
    assign negative = result[31];
    assign carry_out = tmp[32];
    assign overflow = (alu_ctrl == 4'b0010 || alu_ctrl == 4'b0110) ?
                      (a[31] ^ result[31]) & ~(a[31] ^ b[31] ^ alu_ctrl[2]) :
                      1'b0;
endmodule
```

---

## 4. 时序逻辑设计模式

### 4.1 D 触发器变体

```verilog
// Basic DFF with async reset
module dff (
    input  wire clk, rst, d,
    output reg  q
);
    always @(posedge clk or posedge rst)
        if (rst) q <= 1'b0;
        else     q <= d;
endmodule

// DFF with sync reset and enable
module dff_en (
    input  wire clk, rst, en, d,
    output reg  q
);
    always @(posedge clk)
        if (rst)      q <= 1'b0;
        else if (en)  q <= d;
endmodule
```


<details>
<summary>English original</summary>

**2.3 Structural Modeling (instantiation)**

Build hierarchy by connecting modules together:

```verilog
module ripple_carry_4 (
    input  wire [3:0] a, b,
    input  wire       cin,
    output wire [3:0] sum,
    output wire       cout
);
    wire c1, c2, c3;

    full_adder fa0 (.a(a[0]), .b(b[0]), .cin(cin),  .sum(sum[0]), .cout(c1));
    full_adder fa1 (.a(a[1]), .b(b[1]), .cin(c1),   .sum(sum[1]), .cout(c2));
    full_adder fa2 (.a(a[2]), .b(b[2]), .cin(c2),   .sum(sum[2]), .cout(c3));
    full_adder fa3 (.a(a[3]), .b(b[3]), .cin(c3),   .sum(sum[3]), .cout(cout));
endmodule
```

**Always use named port connections** (`.port(signal)`) — positional connections are error-prone and unreadable.

**2.4 Generate Blocks**

`generate` creates hardware at elaboration time — parameterized, scalable structures:

```verilog
module ripple_carry #(parameter N = 32) (
    input  wire [N-1:0] a, b,
    input  wire         cin,
    output wire [N-1:0] sum,
    output wire         cout
);
    wire [N:0] carry;
    assign carry[0] = cin;
    assign cout = carry[N];

    genvar i;
    generate
        for (i = 0; i < N; i = i + 1) begin : fa_stage
            full_adder fa (
                .a(a[i]), .b(b[i]), .cin(carry[i]),
                .sum(sum[i]), .cout(carry[i+1])
            );
        end
    endgenerate
endmodule
```

The `generate for` unrolls at compile time — it creates N separate `full_adder` instances, not a loop that runs at runtime.

---

**3. Combinational Design Patterns**

**3.1 Decoder**

```verilog
module decoder_2to4 (
    input  wire [1:0] in,
    input  wire       en,
    output reg  [3:0] out
);
    always @(*) begin
        out = 4'b0000;
        if (en)
            case (in)
                2'b00: out = 4'b0001;
                2'b01: out = 4'b0010;
                2'b10: out = 4'b0100;
                2'b11: out = 4'b1000;
            endcase
    end
endmodule
```

**3.2 Priority Encoder**

```verilog
module priority_enc (
    input  wire [7:0] req,
    output reg  [2:0] grant,
    output reg        valid
);
    always @(*) begin
        valid = 1'b1;
        casez (req)                         // casez treats z/? as don't-care
            8'b1???_????: grant = 3'd7;
            8'b01??_????: grant = 3'd6;
            8'b001?_????: grant = 3'd5;
            8'b0001_????: grant = 3'd4;
            8'b0000_1???: grant = 3'd3;
            8'b0000_01??: grant = 3'd2;
            8'b0000_001?: grant = 3'd1;
            8'b0000_0001: grant = 3'd0;
            default: begin grant = 3'd0; valid = 1'b0; end
        endcase
    end
endmodule
```

**3.3 Parameterized MUX**

```verilog
module mux4 #(parameter W = 32) (
    input  wire [W-1:0] d0, d1, d2, d3,
    input  wire [1:0]   sel,
    output reg  [W-1:0] y
);
    always @(*) begin
        case (sel)
            2'b00: y = d0;
            2'b01: y = d1;
            2'b10: y = d2;
            2'b11: y = d3;
        endcase
    end
endmodule
```

**3.4 ALU with Flags**

A complete ALU as you'd find in a MIPS datapath:

```verilog
module alu_32 (
    input  wire [31:0] a, b,
    input  wire [3:0]  alu_ctrl,
    output reg  [31:0] result,
    output wire        zero,
    output wire        negative,
    output wire        overflow,
    output wire        carry_out
);
    reg [32:0] tmp;    // 33-bit for carry detection

    always @(*) begin
        tmp = 33'd0;
        case (alu_ctrl)
            4'b0000: tmp = {1'b0, a & b};                    // AND
            4'b0001: tmp = {1'b0, a | b};                    // OR
            4'b0010: tmp = {1'b0, a} + {1'b0, b};           // ADD
            4'b0110: tmp = {1'b0, a} + {1'b0, ~b} + 33'd1;  // SUB
            4'b0111: begin                                    // SLT
                tmp = {1'b0, a} + {1'b0, ~b} + 33'd1;
                tmp = {32'd0, tmp[32] ^ overflow};  // signed less than
            end
            4'b1100: tmp = {1'b0, ~(a | b)};                 // NOR
            4'b1000: tmp = {1'b0, a ^ b};                    // XOR
            default: tmp = 33'd0;
        endcase
        result = tmp[31:0];
    end

    assign zero     = (result == 32'd0);
    assign negative = result[31];
    assign carry_out = tmp[32];
    assign overflow = (alu_ctrl == 4'b0010 || alu_ctrl == 4'b0110) ?
                      (a[31] ^ result[31]) & ~(a[31] ^ b[31] ^ alu_ctrl[2]) :
                      1'b0;
endmodule
```

---

**4. Sequential Design Patterns**

**4.1 D Flip-Flop Variants**

```verilog
// Basic DFF with async reset
module dff (
    input  wire clk, rst, d,
    output reg  q
);
    always @(posedge clk or posedge rst)
        if (rst) q <= 1'b0;
        else     q <= d;
endmodule

// DFF with sync reset and enable
module dff_en (
    input  wire clk, rst, en, d,
    output reg  q
);
    always @(posedge clk)
        if (rst)      q <= 1'b0;
        else if (en)  q <= d;
endmodule
```

</details>

### 4.2 移位寄存器与 LFSR

```verilog
module shift_reg #(parameter N = 8) (
    input  wire       clk, rst, si,    // serial in
    output wire       so,               // serial out
    output wire [N-1:0] q               // parallel out
);
    reg [N-1:0] sr;

    always @(posedge clk or posedge rst)
        if (rst) sr <= {N{1'b0}};
        else     sr <= {sr[N-2:0], si};   // shift left, insert si at LSB

    assign so = sr[N-1];
    assign q  = sr;
endmodule
```

**LFSR（最大长度，8 位）：**

```verilog
module lfsr8 (
    input  wire       clk, rst,
    output reg  [7:0] q
);
    // Polynomial: x^8 + x^6 + x^5 + x^4 + 1 (taps at 8,6,5,4)
    wire feedback = q[7] ^ q[5] ^ q[4] ^ q[3];

    always @(posedge clk or posedge rst)
        if (rst) q <= 8'h01;                       // seed (must be non-zero)
        else     q <= {q[6:0], feedback};           // shift + feedback
endmodule
// Period: 2^8 - 1 = 255 states before repeating
```

### 4.3 带使能与加载的计数器

```verilog
module counter #(parameter WIDTH = 8) (
    input  wire              clk, rst, en, load,
    input  wire [WIDTH-1:0]  d,
    output reg  [WIDTH-1:0]  count,
    output wire              tc           // terminal count
);
    always @(posedge clk or posedge rst) begin
        if (rst)       count <= {WIDTH{1'b0}};
        else if (load) count <= d;
        else if (en)   count <= count + 1'b1;
    end

    assign tc = &count;   // all 1s → terminal count (reduction AND)
endmodule
```

### 4.4 有限状态机 —— Moore 型

**示例：SPI 主控制器**（简化 —— 8 位发送）

```
States: IDLE → LOAD → SHIFT (×8) → DONE → IDLE

         start                  bit_cnt==7
  IDLE ─────────► LOAD ──► SHIFT ─────────► DONE ──► IDLE
                            │  ↑
                            └──┘ bit_cnt < 7
```

```verilog
module spi_tx (
    input  wire       clk, rst, start,
    input  wire [7:0] data_in,
    output reg        sclk, mosi, cs_n, done
);
    // State encoding
    localparam IDLE  = 2'b00,
               LOAD  = 2'b01,
               SHIFT = 2'b10,
               DONE  = 2'b11;

    reg [1:0] state, next_state;
    reg [7:0] shift_reg;
    reg [2:0] bit_cnt;

    // State register
    always @(posedge clk or posedge rst)
        if (rst) state <= IDLE;
        else     state <= next_state;

    // Next-state logic (combinational)
    always @(*) begin
        next_state = state;
        case (state)
            IDLE:  if (start)         next_state = LOAD;
            LOAD:                     next_state = SHIFT;
            SHIFT: if (bit_cnt == 7)  next_state = DONE;
            DONE:                     next_state = IDLE;
        endcase
    end

    // Datapath (sequential)
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            shift_reg <= 8'd0;
            bit_cnt   <= 3'd0;
            sclk      <= 1'b0;
        end else begin
            case (state)
                IDLE: begin
                    sclk    <= 1'b0;
                    bit_cnt <= 3'd0;
                end
                LOAD: begin
                    shift_reg <= data_in;
                end
                SHIFT: begin
                    sclk      <= ~sclk;
                    if (sclk) begin                    // shift on falling edge
                        shift_reg <= {shift_reg[6:0], 1'b0};
                        bit_cnt   <= bit_cnt + 1'b1;
                    end
                end
                DONE: begin
                    sclk <= 1'b0;
                end
            endcase
        end
    end

    // Output logic (Moore — depends only on state and datapath)
    assign mosi = shift_reg[7];
    assign cs_n = (state == IDLE);
    assign done = (state == DONE);
endmodule
```

**FSM 编码检查清单：**
1. 状态寄存器（`always @(posedge clk)`）与次态逻辑（`always @(*)`）分离
2. 必须有 `default` case（或在 `case` 之前先赋默认值）
3. FPGA 用 one-hot 编码，ASIC 用二进制编码（通常由工具选择）
4. 状态命名使用 `localparam` —— 绝不要用裸数字

### 4.5 同步 FIFO

FIFO 在硬件中无处不在——流水线级之间、时钟域之间、生产者-消费者缓冲：

```verilog
module sync_fifo #(
    parameter DEPTH = 16,
    parameter WIDTH = 8
) (
    input  wire             clk, rst,
    input  wire             wr_en, rd_en,
    input  wire [WIDTH-1:0] wr_data,
    output wire [WIDTH-1:0] rd_data,
    output wire             full, empty
);
    localparam ADDR_W = $clog2(DEPTH);

    reg [WIDTH-1:0] mem [0:DEPTH-1];
    reg [ADDR_W:0]  wr_ptr, rd_ptr;     // extra bit for full/empty detection

    // Write
    always @(posedge clk)
        if (wr_en && !full)
            mem[wr_ptr[ADDR_W-1:0]] <= wr_data;

    // Read
    assign rd_data = mem[rd_ptr[ADDR_W-1:0]];

    // Pointer update
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            wr_ptr <= 0;
            rd_ptr <= 0;
        end else begin
            if (wr_en && !full)  wr_ptr <= wr_ptr + 1;
            if (rd_en && !empty) rd_ptr <= rd_ptr + 1;
        end
    end

    // Full: pointers equal but MSBs differ (wrapped around)
    assign full  = (wr_ptr[ADDR_W] != rd_ptr[ADDR_W]) &&
                   (wr_ptr[ADDR_W-1:0] == rd_ptr[ADDR_W-1:0]);
    assign empty = (wr_ptr == rd_ptr);
endmodule
```

指针中多出的 MSB 用于区分「满」（地址相同、MSB 不同）与「空」（指针完全相同）。这是标准技巧。

---

## 5. Testbench 与仿真

### 5.1 Testbench 结构

Testbench 是一个**不可综合**的模块，负责驱动输入并检查输出：

```verilog
`timescale 1ns / 1ps    // time unit / precision

module adder_tb;
    // 1. Declare signals
    reg  [3:0] a, b;
    reg        cin;
    wire [3:0] sum;
    wire       cout;

    // 2. Instantiate DUT (Device Under Test)
    adder uut (
        .a(a), .b(b), .cin(cin),
        .sum(sum), .cout(cout)
    );

    // 3. Drive stimulus
    initial begin
        // Test vector 1: 3 + 4 + 0 = 7
        a = 4'd3; b = 4'd4; cin = 0;
        #10;   // wait 10ns
        if ({cout, sum} !== 5'd7)
            $display("FAIL: 3+4+0 = %0d, expected 7", {cout, sum});

        // Test vector 2: 15 + 1 + 0 = 16 (overflow to cout)
        a = 4'd15; b = 4'd1; cin = 0;
        #10;
        if ({cout, sum} !== 5'd16)
            $display("FAIL: 15+1+0 = %0d, expected 16", {cout, sum});

        // Test vector 3: 15 + 15 + 1 = 31
        a = 4'd15; b = 4'd15; cin = 1;
        #10;
        if ({cout, sum} !== 5'd31)
            $display("FAIL: 15+15+1 = %0d, expected 31", {cout, sum});

        $display("All tests passed");
        $finish;
    end

    // 4. Dump waveforms (for viewer)
    initial begin
        $dumpfile("adder_tb.vcd");
        $dumpvars(0, adder_tb);
    end
endmodule
```

### 5.2 时钟与复位生成

```verilog
// Clock: 100 MHz (10ns period)
reg clk;
initial clk = 0;
always #5 clk = ~clk;    // toggle every 5ns

// Reset: assert for 20ns, then deassert
reg rst;
initial begin
    rst = 1;
    #20;
    rst = 0;
end
```

### 5.3 带 task 的自检 Testbench

```verilog
module alu_tb;
    reg  [31:0] a, b;
    reg  [3:0]  op;
    wire [31:0] result;
    wire        zero;

    alu_32 uut (.a(a), .b(b), .alu_ctrl(op), .result(result),
                .zero(zero), .negative(), .overflow(), .carry_out());

    integer pass_count = 0;
    integer fail_count = 0;

    task check;
        input [31:0] expected;
        input [255:0] name;       // string
    begin
        #1;
        if (result !== expected) begin
            $display("FAIL %0s: got %h, expected %h", name, result, expected);
            fail_count = fail_count + 1;
        end else begin
            pass_count = pass_count + 1;
        end
    end
    endtask

    initial begin
        // AND
        a = 32'hFF00_FF00; b = 32'h0F0F_0F0F; op = 4'b0000;
        check(32'h0F00_0F00, "AND");

        // ADD
        a = 32'd100; b = 32'd200; op = 4'b0010;
        check(32'd300, "ADD 100+200");

        // SUB
        a = 32'd500; b = 32'd200; op = 4'b0110;
        check(32'd300, "SUB 500-200");

        // SLT: 5 < 10
        a = 32'd5; b = 32'd10; op = 4'b0111;
        check(32'd1, "SLT 5<10");

        $display("\nResults: %0d passed, %0d failed", pass_count, fail_count);
        $finish;
    end
endmodule
```

### 5.4 仿真工具

| 工具                | 免费？ | 说明                                    |
|---------------------|-------|------------------------------------------|
| **Icarus Verilog**  | 是   | 开源，CLI，输出 VCD 供 GTKWave 使用 |
| **Verilator**       | 是   | 开源，编译为 C++，速度快       |
| **GTKWave**         | 是   | 开源波形查看器              |
| **EDA Playground**  | 是   | 基于浏览器，无需安装                |
| ModelSim (Intel)    | 免费* | 随 Quartus Lite 免费提供                   |
| Vivado Simulator    | 免费* | 随 Vivado WebPACK 免费提供                 |
| Synopsys VCS        | 否    | 行业标准，速度最快               |
| Cadence Xcelium     | 否    | 行业标准                        |

**Icarus + GTKWave 工作流程：**

```bash
# Compile
iverilog -o sim.vvp adder.v adder_tb.v

# Run simulation
vvp sim.vvp

# View waveforms
gtkwave adder_tb.vcd
```

### 5.5 阅读波形

```
              ___     ___     ___     ___     ___
  clk    ____|   |___|   |___|   |___|   |___|   |___
              ↑       ↑       ↑       ↑       ↑
  rst    ‾‾‾‾‾‾‾‾‾‾‾‾‾\________________________________
                        ↑ rst deasserted
  d      ====[ 0x05 ]===[ 0x0A ]===[ 0x0F ]============
                              ↑       ↑
  q      XXXXXXXXX[ 0x00 ]===[ 0x05 ]===[ 0x0A ]======
                    ↑ reset      ↑ captured 0x05    ↑ captured 0x0A
                    value        on rising edge      on rising edge

Key observations:
  - q changes AFTER the clock edge (clock-to-Q delay)
  - q gets the value d had BEFORE the edge (setup time requirement)
  - X at start = uninitialized (no reset applied yet)
```

---

## 6. 面向综合的设计

### 6.1 什么能综合、什么不能

综合工具把你的 Verilog 转换成门级电路。并非所有 Verilog 结构都有对应的硬件：

| 可综合                     | 不可综合                  |
|-----------------------------------|------------------------------------|
| `wire`, `reg`, `integer`（作为循环） | `initial` 块（仅仿真）        |
| `assign`（连续赋值）             | `$display`, `$monitor`, `$finish`  |
| `always @(*)`（组合逻辑）     | `#delay`（被忽略或报错）        |
| `always @(posedge clk)`（时序）     | `real` / `time` 类型              |
| `if/else`, `case`, `?:`           | 文件 I/O（`$fopen`, `$readmemh`）   |
| `+`, `-`, `*`（算术）        | `/`, `%`（除法 —— 面积大）   |
| `<<`, `>>` 除以常量            | `>>` 除以变量（桶形移位器）  |
| `parameter`, `generate`           | `fork/join`                        |
| 模块实例化              | `deassign`, `force/release`        |

### 6.2 常见的综合陷阱

**非预期的 latch —— case/if 不完整：**

```verilog
// BAD — latch inferred for 'out' when sel=2'b11
always @(*) begin
    case (sel)
        2'b00: out = a;
        2'b01: out = b;
        2'b10: out = c;
        // missing 2'b11!
    endcase
end

// FIX 1: add default
always @(*) begin
    case (sel)
        2'b00: out = a;
        2'b01: out = b;
        2'b10: out = c;
        default: out = 32'd0;
    endcase
end

// FIX 2: assign default before case
always @(*) begin
    out = 32'd0;              // default
    case (sel)
        2'b00: out = a;
        2'b01: out = b;
        2'b10: out = c;
    endcase
end
```

**非预期的 latch —— if/else 不完整：**

```verilog
// BAD — latch on 'out' when en=0
always @(*) begin
    if (en)
        out = data;
    // no else branch!
end

// FIX
always @(*) begin
    if (en) out = data;
    else    out = 32'd0;
end
```

**混用阻塞/非阻塞赋值：**

```verilog
// BAD — blocking in sequential
always @(posedge clk) begin
    a = b;      // b's value assigned to a immediately
    c = a;      // c gets NEW a — NOT a pipeline!
end

// GOOD — non-blocking gives pipeline behavior
always @(posedge clk) begin
    a <= b;     // a gets old b
    c <= a;     // c gets old a → two-stage pipeline
end
```

### 6.3 综合结果 —— 阅读报告

综合完成后，在报告中检查以下内容：

```
=== Resource Usage ===
  LUTs:        1,247 / 53,200  (2.3%)       ← combinational logic
  Registers:     512 / 106,400 (0.5%)       ← flip-flops
  BRAM:            2 / 140     (1.4%)       ← block RAM
  DSP:             4 / 220     (1.8%)       ← multipliers

=== Timing Summary ===
  Target clock: 100 MHz (10.000 ns)
  Worst path:   9.237 ns                     ← critical path
  Slack:        0.763 ns                     ← positive = timing met ✓
  WNS:          0.763 ns                     ← worst negative slack

  If slack < 0 → timing violation → design won't work at target frequency
  Fix: pipeline the critical path, optimize logic, reduce fan-out
```


<details>
<summary>English original</summary>

**5.4 Simulation Tools**

| Tool                | Free? | Notes                                    |
|---------------------|-------|------------------------------------------|
| **Icarus Verilog**  | Yes   | Open-source, CLI, outputs VCD for GTKWave |
| **Verilator**       | Yes   | Open-source, compiles to C++, fast       |
| **GTKWave**         | Yes   | Open-source waveform viewer              |
| **EDA Playground**  | Yes   | Browser-based, no install                |
| ModelSim (Intel)    | Free* | Free with Quartus Lite                   |
| Vivado Simulator    | Free* | Free with Vivado WebPACK                 |
| Synopsys VCS        | No    | Industry standard, fastest               |
| Cadence Xcelium     | No    | Industry standard                        |

**Workflow with Icarus + GTKWave:**

```bash
# Compile
iverilog -o sim.vvp adder.v adder_tb.v

# Run simulation
vvp sim.vvp

# View waveforms
gtkwave adder_tb.vcd
```

**5.5 Reading Waveforms**

```
              ___     ___     ___     ___     ___
  clk    ____|   |___|   |___|   |___|   |___|   |___
              ↑       ↑       ↑       ↑       ↑
  rst    ‾‾‾‾‾‾‾‾‾‾‾‾‾\________________________________
                        ↑ rst deasserted
  d      ====[ 0x05 ]===[ 0x0A ]===[ 0x0F ]============
                              ↑       ↑
  q      XXXXXXXXX[ 0x00 ]===[ 0x05 ]===[ 0x0A ]======
                    ↑ reset      ↑ captured 0x05    ↑ captured 0x0A
                    value        on rising edge      on rising edge

Key observations:
  - q changes AFTER the clock edge (clock-to-Q delay)
  - q gets the value d had BEFORE the edge (setup time requirement)
  - X at start = uninitialized (no reset applied yet)
```

---

**6. Synthesis-Oriented Design**

**6.1 What Synthesizes and What Doesn't**

Synthesis tools convert your Verilog to gates. Not all Verilog constructs have a hardware equivalent:

| Synthesizable                     | NOT synthesizable                  |
|-----------------------------------|------------------------------------|
| `wire`, `reg`, `integer` (as loop)| `initial` blocks (sim only)        |
| `assign` (continuous)             | `$display`, `$monitor`, `$finish`  |
| `always @(*)` (combinational)     | `#delay` (ignored or error)        |
| `always @(posedge clk)` (seq)     | `real` / `time` types              |
| `if/else`, `case`, `?:`           | File I/O (`$fopen`, `$readmemh`)   |
| `+`, `-`, `*` (arithmetic)        | `/`, `%` (division — large area)   |
| `<<`, `>>` by constant            | `>>` by variable (barrel shifter)  |
| `parameter`, `generate`           | `fork/join`                        |
| Module instantiation              | `deassign`, `force/release`        |

**6.2 Common Synthesis Pitfalls**

**Unintended latch — incomplete case/if:**

```verilog
// BAD — latch inferred for 'out' when sel=2'b11
always @(*) begin
    case (sel)
        2'b00: out = a;
        2'b01: out = b;
        2'b10: out = c;
        // missing 2'b11!
    endcase
end

// FIX 1: add default
always @(*) begin
    case (sel)
        2'b00: out = a;
        2'b01: out = b;
        2'b10: out = c;
        default: out = 32'd0;
    endcase
end

// FIX 2: assign default before case
always @(*) begin
    out = 32'd0;              // default
    case (sel)
        2'b00: out = a;
        2'b01: out = b;
        2'b10: out = c;
    endcase
end
```

**Unintended latch — incomplete if/else:**

```verilog
// BAD — latch on 'out' when en=0
always @(*) begin
    if (en)
        out = data;
    // no else branch!
end

// FIX
always @(*) begin
    if (en) out = data;
    else    out = 32'd0;
end
```

**Mixing blocking/non-blocking:**

```verilog
// BAD — blocking in sequential
always @(posedge clk) begin
    a = b;      // b's value assigned to a immediately
    c = a;      // c gets NEW a — NOT a pipeline!
end

// GOOD — non-blocking gives pipeline behavior
always @(posedge clk) begin
    a <= b;     // a gets old b
    c <= a;     // c gets old a → two-stage pipeline
end
```

**6.3 Synthesis Results — Reading the Report**

After synthesis, check these in the report:

```
=== Resource Usage ===
  LUTs:        1,247 / 53,200  (2.3%)       ← combinational logic
  Registers:     512 / 106,400 (0.5%)       ← flip-flops
  BRAM:            2 / 140     (1.4%)       ← block RAM
  DSP:             4 / 220     (1.8%)       ← multipliers

=== Timing Summary ===
  Target clock: 100 MHz (10.000 ns)
  Worst path:   9.237 ns                     ← critical path
  Slack:        0.763 ns                     ← positive = timing met ✓
  WNS:          0.763 ns                     ← worst negative slack

  If slack < 0 → timing violation → design won't work at target frequency
  Fix: pipeline the critical path, optimize logic, reduce fan-out
```

</details>

### 6.4 面向推理的编码

综合工具会识别特定模式，并将其映射到经过优化的硬件块：

```verilog
// This infers a BRAM (block RAM) — tool recognizes sync read + write pattern
reg [31:0] mem [0:1023];
always @(posedge clk) begin
    if (we) mem[addr] <= wdata;
    rdata <= mem[addr];              // registered read → BRAM
end

// This infers a DSP multiplier
reg [31:0] product;
always @(posedge clk)
    product <= a * b;                // registered multiply → DSP48

// This infers distributed RAM (LUTs) — async read
reg [31:0] mem [0:31];
always @(posedge clk)
    if (we) mem[addr] <= wdata;
assign rdata = mem[addr];            // combinational read → LUT RAM
```

---

## 7. 整合起来 —— 用 Verilog 实现 MIPS 单周期

本节实现《数字设计基础》指南第 6 节中的单周期 MIPS 处理器。每个组件都是前面已经出现过的模块；本节把它们连接起来。

### 7.1 指令存储器

```verilog
module imem (
    input  wire [31:0] addr,
    output wire [31:0] instr
);
    reg [31:0] mem [0:255];

    initial $readmemh("program.hex", mem);    // load program at sim time

    assign instr = mem[addr[9:2]];            // word-aligned (addr >> 2)
endmodule
```

### 7.2 数据存储器

```verilog
module dmem (
    input  wire        clk,
    input  wire        we,         // write enable
    input  wire [31:0] addr,
    input  wire [31:0] wdata,
    output wire [31:0] rdata
);
    reg [31:0] mem [0:255];

    always @(posedge clk)
        if (we) mem[addr[9:2]] <= wdata;

    assign rdata = mem[addr[9:2]];
endmodule
```

### 7.3 寄存器堆

```verilog
module regfile (
    input  wire        clk,
    input  wire        we3,        // write enable (port 3)
    input  wire [4:0]  ra1, ra2,   // read addresses
    input  wire [4:0]  wa3,        // write address
    input  wire [31:0] wd3,        // write data
    output wire [31:0] rd1, rd2    // read data
);
    reg [31:0] rf [0:31];

    always @(posedge clk)
        if (we3) rf[wa3] <= wd3;

    // $0 is hardwired to zero
    assign rd1 = (ra1 != 5'd0) ? rf[ra1] : 32'd0;
    assign rd2 = (ra2 != 5'd0) ? rf[ra2] : 32'd0;
endmodule
```

### 7.4 符号扩展器

```verilog
module sign_ext (
    input  wire [15:0] in,
    output wire [31:0] out
);
    assign out = {{16{in[15]}}, in};
endmodule
```

### 7.5 控制单元

```verilog
module control (
    input  wire [5:0] opcode,
    output reg        reg_dst, alu_src, mem_to_reg,
    output reg        reg_write, mem_read, mem_write,
    output reg        branch, jump,
    output reg  [1:0] alu_op
);
    always @(*) begin
        // Defaults
        {reg_dst, alu_src, mem_to_reg, reg_write} = 4'b0;
        {mem_read, mem_write, branch, jump}        = 4'b0;
        alu_op = 2'b00;

        case (opcode)
            6'b000000: begin // R-type
                reg_dst   = 1; reg_write = 1;
                alu_op    = 2'b10;
            end
            6'b100011: begin // lw
                alu_src   = 1; mem_to_reg = 1;
                reg_write = 1; mem_read   = 1;
            end
            6'b101011: begin // sw
                alu_src   = 1; mem_write  = 1;
            end
            6'b000100: begin // beq
                branch = 1; alu_op = 2'b01;
            end
            6'b001000: begin // addi
                alu_src   = 1; reg_write  = 1;
            end
            6'b000010: begin // j
                jump = 1;
            end
        endcase
    end
endmodule
```

### 7.6 ALU 控制

```verilog
module alu_control (
    input  wire [1:0] alu_op,
    input  wire [5:0] funct,
    output reg  [3:0] alu_ctrl
);
    always @(*) begin
        case (alu_op)
            2'b00: alu_ctrl = 4'b0010;              // ADD (lw/sw/addi)
            2'b01: alu_ctrl = 4'b0110;              // SUB (beq)
            2'b10: case (funct)                      // R-type
                6'b100000: alu_ctrl = 4'b0010;       // add
                6'b100010: alu_ctrl = 4'b0110;       // sub
                6'b100100: alu_ctrl = 4'b0000;       // and
                6'b100101: alu_ctrl = 4'b0001;       // or
                6'b101010: alu_ctrl = 4'b0111;       // slt
                default:   alu_ctrl = 4'b0010;
            endcase
            default: alu_ctrl = 4'b0010;
        endcase
    end
endmodule
```

### 7.7 顶层：单周期 MIPS

```verilog
module mips_single_cycle (
    input wire clk, rst
);
    // ── Wires ──────────────────────────────────────────────────
    wire [31:0] pc, pc_plus4, pc_branch, pc_jump, pc_next;
    wire [31:0] instr;
    wire [31:0] rd1, rd2, alu_result, read_data, write_data;
    wire [31:0] sign_imm, sign_imm_sl2;
    wire [4:0]  write_reg;
    wire [3:0]  alu_ctrl;
    wire        zero;

    // Control signals
    wire reg_dst, alu_src, mem_to_reg, reg_write;
    wire mem_read, mem_write, branch, jump;
    wire [1:0] alu_op;

    // ── PC register ────────────────────────────────────────────
    reg [31:0] pc_reg;
    always @(posedge clk or posedge rst)
        if (rst) pc_reg <= 32'h0000_0000;
        else     pc_reg <= pc_next;
    assign pc = pc_reg;

    // ── PC logic ───────────────────────────────────────────────
    assign pc_plus4    = pc + 32'd4;
    assign sign_imm_sl2 = {sign_imm[29:0], 2'b00};       // shift left 2
    assign pc_branch   = pc_plus4 + sign_imm_sl2;
    assign pc_jump     = {pc_plus4[31:28], instr[25:0], 2'b00};

    wire  pc_src = branch & zero;
    assign pc_next = jump   ? pc_jump :
                     pc_src ? pc_branch :
                              pc_plus4;

    // ── Instruction memory ─────────────────────────────────────
    imem imem_inst (.addr(pc), .instr(instr));

    // ── Control ────────────────────────────────────────────────
    control ctrl (
        .opcode(instr[31:26]),
        .reg_dst(reg_dst), .alu_src(alu_src),
        .mem_to_reg(mem_to_reg), .reg_write(reg_write),
        .mem_read(mem_read), .mem_write(mem_write),
        .branch(branch), .jump(jump), .alu_op(alu_op)
    );

    // ── Register file ──────────────────────────────────────────
    assign write_reg = reg_dst ? instr[15:11] : instr[20:16];  // rd : rt

    regfile rf (
        .clk(clk), .we3(reg_write),
        .ra1(instr[25:21]), .ra2(instr[20:16]),
        .wa3(write_reg), .wd3(write_data),
        .rd1(rd1), .rd2(rd2)
    );

    // ── Sign extend ────────────────────────────────────────────
    sign_ext se (.in(instr[15:0]), .out(sign_imm));

    // ── ALU control ────────────────────────────────────────────
    alu_control alu_c (
        .alu_op(alu_op), .funct(instr[5:0]),
        .alu_ctrl(alu_ctrl)
    );

    // ── ALU ────────────────────────────────────────────────────
    wire [31:0] alu_b = alu_src ? sign_imm : rd2;

    alu_32 alu (
        .a(rd1), .b(alu_b), .alu_ctrl(alu_ctrl),
        .result(alu_result), .zero(zero),
        .negative(), .overflow(), .carry_out()
    );

    // ── Data memory ────────────────────────────────────────────
    dmem dmem_inst (
        .clk(clk), .we(mem_write),
        .addr(alu_result), .wdata(rd2),
        .rdata(read_data)
    );

    // ── Write-back MUX ─────────────────────────────────────────
    assign write_data = mem_to_reg ? read_data : alu_result;
endmodule
```

### 7.8 测试平台 — 运行程序

```verilog
`timescale 1ns / 1ps

module mips_tb;
    reg clk, rst;

    mips_single_cycle uut (.clk(clk), .rst(rst));

    // Clock: 100 MHz
    initial clk = 0;
    always #5 clk = ~clk;

    // Reset and run
    initial begin
        $dumpfile("mips_tb.vcd");
        $dumpvars(0, mips_tb);

        rst = 1;
        #20;
        rst = 0;

        // Run for 500 cycles
        #5000;

        // Check register results (read regfile directly in testbench)
        $display("$t0 (r8)  = %0d", uut.rf.rf[8]);
        $display("$t1 (r9)  = %0d", uut.rf.rf[9]);
        $display("$t2 (r10) = %0d", uut.rf.rf[10]);
        $finish;
    end
endmodule
```

**示例程序（`program.hex`）：**

```
// addi $t0, $zero, 5      →  0x20080005
// addi $t1, $zero, 3      →  0x20090003
// add  $t2, $t0, $t1      →  0x01095020
// sw   $t2, 0($zero)      →  0xAC0A0000
// lw   $t3, 0($zero)      →  0x8C0B0000
20080005
20090003
01095020
AC0A0000
8C0B0000
```

仿真后：`$t2` 应为 8（5 + 3），`$t3` 也应为 8（从内存加载）。

> **下一步：** 通过添加流水线寄存器（IF/ID、ID/EX、EX/MEM、MEM/WB）、转发单元和冒险检测单元，将此单周期设计扩展为 5 级流水线。《数字设计基础》指南第 6 节包含数据通路和逻辑 — 将其转换为 Verilog 遵循相同的每阶段一模块模式。

---

## 8. 综合与 FPGA 实现


<details>
<summary>English original</summary>

**7.7 Top-Level: Single-Cycle MIPS**

```verilog
module mips_single_cycle (
    input wire clk, rst
);
    // ── Wires ──────────────────────────────────────────────────
    wire [31:0] pc, pc_plus4, pc_branch, pc_jump, pc_next;
    wire [31:0] instr;
    wire [31:0] rd1, rd2, alu_result, read_data, write_data;
    wire [31:0] sign_imm, sign_imm_sl2;
    wire [4:0]  write_reg;
    wire [3:0]  alu_ctrl;
    wire        zero;

    // Control signals
    wire reg_dst, alu_src, mem_to_reg, reg_write;
    wire mem_read, mem_write, branch, jump;
    wire [1:0] alu_op;

    // ── PC register ────────────────────────────────────────────
    reg [31:0] pc_reg;
    always @(posedge clk or posedge rst)
        if (rst) pc_reg <= 32'h0000_0000;
        else     pc_reg <= pc_next;
    assign pc = pc_reg;

    // ── PC logic ───────────────────────────────────────────────
    assign pc_plus4    = pc + 32'd4;
    assign sign_imm_sl2 = {sign_imm[29:0], 2'b00};       // shift left 2
    assign pc_branch   = pc_plus4 + sign_imm_sl2;
    assign pc_jump     = {pc_plus4[31:28], instr[25:0], 2'b00};

    wire  pc_src = branch & zero;
    assign pc_next = jump   ? pc_jump :
                     pc_src ? pc_branch :
                              pc_plus4;

    // ── Instruction memory ─────────────────────────────────────
    imem imem_inst (.addr(pc), .instr(instr));

    // ── Control ────────────────────────────────────────────────
    control ctrl (
        .opcode(instr[31:26]),
        .reg_dst(reg_dst), .alu_src(alu_src),
        .mem_to_reg(mem_to_reg), .reg_write(reg_write),
        .mem_read(mem_read), .mem_write(mem_write),
        .branch(branch), .jump(jump), .alu_op(alu_op)
    );

    // ── Register file ──────────────────────────────────────────
    assign write_reg = reg_dst ? instr[15:11] : instr[20:16];  // rd : rt

    regfile rf (
        .clk(clk), .we3(reg_write),
        .ra1(instr[25:21]), .ra2(instr[20:16]),
        .wa3(write_reg), .wd3(write_data),
        .rd1(rd1), .rd2(rd2)
    );

    // ── Sign extend ────────────────────────────────────────────
    sign_ext se (.in(instr[15:0]), .out(sign_imm));

    // ── ALU control ────────────────────────────────────────────
    alu_control alu_c (
        .alu_op(alu_op), .funct(instr[5:0]),
        .alu_ctrl(alu_ctrl)
    );

    // ── ALU ────────────────────────────────────────────────────
    wire [31:0] alu_b = alu_src ? sign_imm : rd2;

    alu_32 alu (
        .a(rd1), .b(alu_b), .alu_ctrl(alu_ctrl),
        .result(alu_result), .zero(zero),
        .negative(), .overflow(), .carry_out()
    );

    // ── Data memory ────────────────────────────────────────────
    dmem dmem_inst (
        .clk(clk), .we(mem_write),
        .addr(alu_result), .wdata(rd2),
        .rdata(read_data)
    );

    // ── Write-back MUX ─────────────────────────────────────────
    assign write_data = mem_to_reg ? read_data : alu_result;
endmodule
```

**7.8 Testbench — Running a Program**

```verilog
`timescale 1ns / 1ps

module mips_tb;
    reg clk, rst;

    mips_single_cycle uut (.clk(clk), .rst(rst));

    // Clock: 100 MHz
    initial clk = 0;
    always #5 clk = ~clk;

    // Reset and run
    initial begin
        $dumpfile("mips_tb.vcd");
        $dumpvars(0, mips_tb);

        rst = 1;
        #20;
        rst = 0;

        // Run for 500 cycles
        #5000;

        // Check register results (read regfile directly in testbench)
        $display("$t0 (r8)  = %0d", uut.rf.rf[8]);
        $display("$t1 (r9)  = %0d", uut.rf.rf[9]);
        $display("$t2 (r10) = %0d", uut.rf.rf[10]);
        $finish;
    end
endmodule
```

**Example program (`program.hex`):**

```
// addi $t0, $zero, 5      →  0x20080005
// addi $t1, $zero, 3      →  0x20090003
// add  $t2, $t0, $t1      →  0x01095020
// sw   $t2, 0($zero)      →  0xAC0A0000
// lw   $t3, 0($zero)      →  0x8C0B0000
20080005
20090003
01095020
AC0A0000
8C0B0000
```

After simulation: `$t2` should hold 8 (5 + 3), and `$t3` should also hold 8 (loaded from memory).

> **Next step:** extend this single-cycle design to a 5-stage pipeline by adding pipeline registers (IF/ID, ID/EX, EX/MEM, MEM/WB), a forwarding unit, and a hazard detection unit. The Digital Design Fundamentals guide Section 6 has the datapath and logic — translating it to Verilog follows the same module-per-stage pattern.

---

**8. Synthesis and FPGA Implementation**

</details>

### 8.1 FPGA 架构概述

FPGA 通过配置可编程逻辑块（CLB）来实现你的设计，这些 CLB 由可编程布线资源互连：

```
FPGA chip (simplified):

  ┌─────────────────────────────────────────────┐
  │  IOB   IOB   IOB   IOB   IOB   IOB   IOB   │  ← I/O blocks (pins)
  │ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐   │
  │ │ CLB │─│ CLB │─│ CLB │─│ CLB │─│ CLB │   │
  │ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘   │
  │ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐   │
  │ │ CLB │─│BRAM │─│ CLB │─│ DSP │─│ CLB │   │
  │ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘   │
  │ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐   │
  │ │ CLB │─│ CLB │─│ CLB │─│ CLB │─│ CLB │   │
  │ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘   │
  │  IOB   IOB   IOB   IOB   IOB   IOB   IOB   │
  └─────────────────────────────────────────────┘

CLB contains: LUTs (6-input, configurable truth tables) + flip-flops
BRAM: 36 Kbit block RAM for memories
DSP: hardened multiplier-accumulate blocks
IOB: configurable I/O standards (LVCMOS, LVDS, etc.)
```

**CLB 细节（Xilinx 7 系列风格）：**

```
  ┌─────────────────────────────────────┐
  │  Slice (×2 per CLB)                 │
  │                                     │
  │  [6-LUT] → [MUX] → [FF]  →  out   │
  │  [6-LUT] → [MUX] → [FF]  →  out   │
  │  [6-LUT] → [MUX] → [FF]  →  out   │
  │  [6-LUT] → [MUX] → [FF]  →  out   │
  │                                     │
  │  + carry chain for fast arithmetic  │
  └─────────────────────────────────────┘

Each 6-LUT implements ANY function of 6 inputs (2^6 = 64-entry truth table)
Each FF = one flip-flop with sync/async reset/set, clock enable
```

### 8.2 FPGA 设计流程

```
Verilog source
    │
    ▼
Synthesis (Vivado / Quartus)
    │  → maps to LUTs, FFs, BRAM, DSP
    ▼
Implementation
    ├── Place: assign CLBs to physical locations
    ├── Route: connect CLBs through routing fabric
    └── Timing: verify setup/hold met at target frequency
    │
    ▼
Bitstream generation (.bit / .sof)
    │
    ▼
Program FPGA (JTAG / flash)
```

### 8.3 约束文件

向工具描述你的物理开发板 —— 引脚分配与时钟：

```tcl
# Xilinx XDC constraints (example: Basys 3 board)

# Clock: 100 MHz oscillator on pin W5
set_property -dict {PACKAGE_PIN W5 IOSTANDARD LVCMOS33} [get_ports clk]
create_clock -period 10.000 -name sys_clk [get_ports clk]

# Reset: center button
set_property -dict {PACKAGE_PIN U18 IOSTANDARD LVCMOS33} [get_ports rst]

# LEDs
set_property -dict {PACKAGE_PIN U16 IOSTANDARD LVCMOS33} [get_ports {led[0]}]
set_property -dict {PACKAGE_PIN E19 IOSTANDARD LVCMOS33} [get_ports {led[1]}]

# Switches
set_property -dict {PACKAGE_PIN V17 IOSTANDARD LVCMOS33} [get_ports {sw[0]}]
set_property -dict {PACKAGE_PIN V16 IOSTANDARD LVCMOS33} [get_ports {sw[1]}]
```

### 8.4 适合学习的 FPGA 开发板推荐

| 开发板                  | FPGA              | 价格  | 适用场景                        |
|------------------------|-------------------|--------|---------------------------------|
| Digilent Basys 3       | Xilinx Artix-7    | ~$150  | 第一块板、课程作业         |
| Digilent Nexys A7      | Xilinx Artix-7    | ~$265  | 较大规模设计、DDR 内存      |
| Terasic DE10-Lite      | Intel MAX 10      | ~$85   | 预算方案、Quartus          |
| Terasic DE1-SoC        | Intel Cyclone V   | ~$200  | ARM HPS + FPGA、Linux           |
| Digilent Arty A7       | Xilinx Artix-7    | ~$130  | Arduino 外形规格、RISC-V     |
| Xilinx KV260 / KR260   | Zynq UltraScale+  | ~$250 | AI 推理（DPU）、量产  |

---

## 资源

| 资源 | 类型 | 侧重 |
|----------|------|-------|
| *Digital Design and Computer Architecture* — Harris & Harris | 教材 | Verilog + 处理器设计（第 4–7 章） |
| *Verilog HDL* — Samir Palnitkar | 教材 | 全面的 Verilog 参考 |
| HDLBits (hdlbits.01xz.net) | 交互式 | 180+ 道 Verilog 练习，自动评分 |
| Nandland (nandland.com) | 教程 | 面向初学者的 Verilog + FPGA，配真实开发板 |
| ASIC World (asic-world.com) | 参考 | Verilog 语法、示例、testbench |
| EDA Playground (edaplayground.com) | 在线仿真 | 浏览器端 Verilog 仿真 |
| *Computer Organization and Design* — Patterson & Hennessy | 教材 | MIPS/RISC-V 数据通路与流水线 |
| OpenCores (opencores.org) | 开源 | 可供研究的真实 Verilog IP |
| Icarus Verilog + GTKWave | 工具 | 免费仿真器 + 波形查看器 |

---


<details>
<summary>English original</summary>

**8.1 FPGA Architecture Overview**

An FPGA implements your design by configuring programmable logic blocks (CLBs) connected by a programmable routing fabric:

```
FPGA chip (simplified):

  ┌─────────────────────────────────────────────┐
  │  IOB   IOB   IOB   IOB   IOB   IOB   IOB   │  ← I/O blocks (pins)
  │ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐   │
  │ │ CLB │─│ CLB │─│ CLB │─│ CLB │─│ CLB │   │
  │ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘   │
  │ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐   │
  │ │ CLB │─│BRAM │─│ CLB │─│ DSP │─│ CLB │   │
  │ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘   │
  │ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐ ┌──┴──┐   │
  │ │ CLB │─│ CLB │─│ CLB │─│ CLB │─│ CLB │   │
  │ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘   │
  │  IOB   IOB   IOB   IOB   IOB   IOB   IOB   │
  └─────────────────────────────────────────────┘

CLB contains: LUTs (6-input, configurable truth tables) + flip-flops
BRAM: 36 Kbit block RAM for memories
DSP: hardened multiplier-accumulate blocks
IOB: configurable I/O standards (LVCMOS, LVDS, etc.)
```

**CLB detail (Xilinx 7-series style):**

```
  ┌─────────────────────────────────────┐
  │  Slice (×2 per CLB)                 │
  │                                     │
  │  [6-LUT] → [MUX] → [FF]  →  out   │
  │  [6-LUT] → [MUX] → [FF]  →  out   │
  │  [6-LUT] → [MUX] → [FF]  →  out   │
  │  [6-LUT] → [MUX] → [FF]  →  out   │
  │                                     │
  │  + carry chain for fast arithmetic  │
  └─────────────────────────────────────┘

Each 6-LUT implements ANY function of 6 inputs (2^6 = 64-entry truth table)
Each FF = one flip-flop with sync/async reset/set, clock enable
```

**8.2 FPGA Design Flow**

```
Verilog source
    │
    ▼
Synthesis (Vivado / Quartus)
    │  → maps to LUTs, FFs, BRAM, DSP
    ▼
Implementation
    ├── Place: assign CLBs to physical locations
    ├── Route: connect CLBs through routing fabric
    └── Timing: verify setup/hold met at target frequency
    │
    ▼
Bitstream generation (.bit / .sof)
    │
    ▼
Program FPGA (JTAG / flash)
```

**8.3 Constraints File**

Tell the tools about your physical board — pin assignments and clock:

```tcl
# Xilinx XDC constraints (example: Basys 3 board)

# Clock: 100 MHz oscillator on pin W5
set_property -dict {PACKAGE_PIN W5 IOSTANDARD LVCMOS33} [get_ports clk]
create_clock -period 10.000 -name sys_clk [get_ports clk]

# Reset: center button
set_property -dict {PACKAGE_PIN U18 IOSTANDARD LVCMOS33} [get_ports rst]

# LEDs
set_property -dict {PACKAGE_PIN U16 IOSTANDARD LVCMOS33} [get_ports {led[0]}]
set_property -dict {PACKAGE_PIN E19 IOSTANDARD LVCMOS33} [get_ports {led[1]}]

# Switches
set_property -dict {PACKAGE_PIN V17 IOSTANDARD LVCMOS33} [get_ports {sw[0]}]
set_property -dict {PACKAGE_PIN V16 IOSTANDARD LVCMOS33} [get_ports {sw[1]}]
```

**8.4 Recommended FPGA Boards for Learning**

| Board                  | FPGA              | Price  | Best for                        |
|------------------------|-------------------|--------|---------------------------------|
| Digilent Basys 3       | Xilinx Artix-7    | ~$150  | First board, coursework         |
| Digilent Nexys A7      | Xilinx Artix-7    | ~$265  | Larger designs, DDR memory      |
| Terasic DE10-Lite      | Intel MAX 10      | ~$85   | Budget option, Quartus          |
| Terasic DE1-SoC        | Intel Cyclone V   | ~$200  | ARM HPS + FPGA, Linux           |
| Digilent Arty A7       | Xilinx Artix-7    | ~$130  | Arduino form-factor, RISC-V     |
| Xilinx KV260 / KR260   | Zynq UltraScale+  | ~$250 | AI inference (DPU), production  |

---

**Resources**

| Resource | Type | Focus |
|----------|------|-------|
| *Digital Design and Computer Architecture* — Harris & Harris | Textbook | Verilog + processor design (ch. 4–7) |
| *Verilog HDL* — Samir Palnitkar | Textbook | Comprehensive Verilog reference |
| HDLBits (hdlbits.01xz.net) | Interactive | 180+ Verilog exercises with auto-grading |
| Nandland (nandland.com) | Tutorial | Beginner Verilog + FPGA with real boards |
| ASIC World (asic-world.com) | Reference | Verilog syntax, examples, testbenches |
| EDA Playground (edaplayground.com) | Online sim | Browser-based Verilog simulation |
| *Computer Organization and Design* — Patterson & Hennessy | Textbook | MIPS/RISC-V datapath and pipeline |
| OpenCores (opencores.org) | Open-source | Real-world Verilog IP to study |
| Icarus Verilog + GTKWave | Tools | Free simulator + waveform viewer |

---

</details>

## 项目

| # | 项目 | 练习的概念 | 复杂度 |
|---|---------|-------------------|------------|
| 1 | **全加器 → 32 位 RCA** | dataflow、结构化、generate | 入门 |
| 2 | **支持 8 种运算 + 标志位的 ALU** | 行为级、case、拼接 | 入门 |
| 3 | **BCD 转 7 段译码器** | 组合逻辑、case、FPGA I/O | 入门 |
| 4 | **8 位移位寄存器 + LFSR** | 时序、反馈、PRNG | 入门 |
| 5 | **同步 FIFO（参数化）** | 指针、满/空、BRAM | 中级 |
| 6 | **SPI 主控制器** | FSM、移位寄存器、协议 | 中级 |
| 7 | **UART TX + RX** | FSM、波特率、过采样 | 中级 |
| 8 | **单周期 MIPS 处理器** | 以上全部：数据通路、控制、存储器 | 高级 |
| 9 | **流水线 MIPS + 前递** | 流水线寄存器、冒险检测、MUX | 高级 |
| 10 | **矩阵乘法加速器** | BRAM 分块、FSM 控制、DSP 推理 | 高级 |


<details>
<summary>English original</summary>

**Projects**

| # | Project | Concepts practiced | Complexity |
|---|---------|-------------------|------------|
| 1 | **Full adder → 32-bit RCA** | Dataflow, structural, generate | Beginner |
| 2 | **ALU with 8 operations + flags** | Behavioral, case, concatenation | Beginner |
| 3 | **BCD to 7-segment decoder** | Combinational, case, FPGA I/O | Beginner |
| 4 | **8-bit shift register + LFSR** | Sequential, feedback, PRNG | Beginner |
| 5 | **Synchronous FIFO (parameterized)** | Pointers, full/empty, BRAM | Intermediate |
| 6 | **SPI master controller** | FSM, shift register, protocol | Intermediate |
| 7 | **UART TX + RX** | FSM, baud rate, oversampling | Intermediate |
| 8 | **Single-cycle MIPS processor** | All of the above: datapath, control, memory | Advanced |
| 9 | **Pipelined MIPS + forwarding** | Pipeline registers, hazard detection, MUX | Advanced |
| 10 | **Matrix multiply accelerator** | BRAM tiling, FSM control, DSP inference | Advanced |

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/1. Digital Design and Hardware Description Languages/Hardware Description Languages (HDLs)/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/1.%20Digital%20Design%20and%20Hardware%20Description%20Languages/Hardware%20Description%20Languages%20%28HDLs%29/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
