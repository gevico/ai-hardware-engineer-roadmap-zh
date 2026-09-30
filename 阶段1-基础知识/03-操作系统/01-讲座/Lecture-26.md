---
title: 第 26 讲：eBPF —— 可编程的内核可观测性与网络
description: 第 26 讲：eBPF —— 可编程的内核可观测性与网络
---

# 第 26 讲：eBPF —— 可编程的内核可观测性与网络

## 概述

第 4 讲将 eBPF 引入为生产性能剖析的"只读显微镜"。本讲深入展开：eBPF 虚拟机如何工作，verifier 究竟检查什么，如何用 libbpf 从零编写程序，以及如何用 eBPF 实现 AI 系统可观测性 —— 追踪 GPU 驱动延迟、测量 DMA 吞吐、剖析推理线程上的调度器决策，以及为自动驾驶车辆遥测构建自定义网络过滤。核心挑战在于：如何在不编写内核模块、不重启的前提下，构建**生产安全、低开销、内核级插桩**？心智模型是：eBPF 将 Linux 内核变成一个**可编程平台** —— 你在特定 hook 点注入小型经过验证的程序，每当该 hook 触发时，内核都以原生速度执行它们。对 AI 硬件工程师而言，eBPF 就是"推理有时耗时 30 ms 而非 10 ms，我们却不知道原因"与一张精确直方图之间的差别——该直方图显示，当 ISP（图像信号处理器）处理 HDR 帧时，VIDIOC_DQBUF 阻塞了 20 ms。

---

## eBPF 虚拟机

eBPF 不是"一个追踪工具"。它是一台**通用内核内虚拟机**，具备类 RISC 指令集、经过验证的执行，以及编译到原生代码的 JIT。`bpftrace` 和 BCC 之类的工具是前端 —— 它们生成在此 VM 上运行的 eBPF 字节码。

### 寄存器组

| 寄存器 | 用途 |
|---|---|
| `r0` | 返回值（也是：辅助函数返回值） |
| `r1`–`r5` | 函数参数（调用者保存） |
| `r6`–`r9` | 被调用者保存（跨辅助函数调用保持） |
| `r10` | 帧指针（只读；指向 512 字节的栈） |

eBPF ISA 有 **11 个寄存器** —— 刻意设计得与 ARM64 的调用约定相似。这使得编译到 ARM（Jetson、移动 SoC）的 JIT 非常高效：eBPF 寄存器**几乎 1:1 映射到硬件寄存器**。

### 指令集

```
eBPF Instruction Format (64-bit):
┌──────────┬──────┬──────┬──────────┬────────────────────────────────┐
│  opcode  │ dst  │ src  │  offset  │           immediate            │
│  8 bits  │4 bits│4 bits│ 16 bits  │           32 bits              │
└──────────┴──────┴──────┴──────────┴────────────────────────────────┘
```

| 类别 | 指令 | 示例 |
|---|---|---|
| ALU（64 位） | `add`, `sub`, `mul`, `div`, `mod`, `or`, `and`, `xor`, `lsh`, `rsh`, `arsh`, `neg` | `r0 += r1` |
| ALU（32 位） | 相同操作，带 `w` 后缀 | `w0 += w1`（32 位加） |
| 内存 | `ldx`, `stx`, `st`（1/2/4/8 字节） | `r0 = *(u64 *)(r1 + 16)` |
| 分支 | `jeq`, `jne`, `jgt`, `jge`, `jlt`, `jle`, `jset` | `if r0 > r1 goto +5` |
| 调用 | `call imm` | `call bpf_ktime_get_ns` |
| 退出 | `exit` | 向调用者返回 `r0` |
| 原子 | `lock xadd`, `lock cmpxchg`, `lock xchg` | 原子计数器自增 |

**无浮点。** eBPF 的 ALU 仅支持整数。延迟直方图使用整数纳秒；吞吐计算使用整数字节/秒。这是有意为之 —— 内核上下文中的浮点异常既危险又非确定性。

### JIT 编译

内核在加载时将 eBPF 字节码 JIT 编译为**原生机器指令**：

| 架构 | JIT 质量 | 说明 |
|---|---|---|
| x86-64 | 优秀 | 近乎 1:1 映射；eBPF 设计时即考虑了 x86 |
| ARM64（AArch64） | 优秀 | Jetson Orin、Qualcomm SoC —— 寄存器映射很自然 |
| ARM32 | 良好 | 较老的嵌入式、部分 Cortex-A 设备 |
| RISC-V | 良好 | 支持不断增长；与开源 AI 芯片相关 |

JIT 之后，eBPF 程序以**原生指令速度**运行 —— 而非解释执行。每次 probe 命中的开销通常为 **50–200 ns**，其中占主导的是函数调用 trampoline，而非 eBPF 指令。

---

## Verifier：eBPF 为何安全

在任何 eBPF 程序执行之前，内核 verifier 都会对每一条可能的执行路径执行**静态分析**。这正是 eBPF 在生产环境中安全的原因 —— 有 bug 的 eBPF 程序会在加载时被拒绝，而绝不在 runtime。


<details>
<summary>English original</summary>

**Lecture 26: eBPF — Programmable Kernel Observability & Networking**

**Overview**

Lecture 4 introduced eBPF as a "read-only microscope" for production profiling. This lecture goes deep: how the eBPF virtual machine works, what the verifier actually checks, how to write programs from scratch with libbpf, and how to use eBPF for AI system observability — tracing GPU driver latency, measuring DMA throughput, profiling scheduler decisions on inference threads, and building custom network filtering for autonomous vehicle telemetry. The core challenge is: how do you build **production-safe, low-overhead, kernel-level instrumentation** without writing kernel modules or rebooting? The mental model is that eBPF turns the Linux kernel into a **programmable platform** — you inject small verified programs at specific hook points, and the kernel executes them at native speed every time that hook fires. For an AI hardware engineer, eBPF is the difference between "inference sometimes takes 30 ms instead of 10 ms and we don't know why" and a precise histogram showing that VIDIOC_DQBUF blocks for 20 ms when the ISP is processing HDR frames.

---

**eBPF Virtual Machine**

eBPF is not "a tracing tool." It is a **general-purpose in-kernel virtual machine** with a RISC-like instruction set, verified execution, and JIT compilation to native code. Tools like `bpftrace` and BCC are frontends — they generate eBPF bytecode that runs on this VM.

**Register File**

| Register | Purpose |
|---|---|
| `r0` | Return value (also: helper function return) |
| `r1`–`r5` | Function arguments (caller-saved) |
| `r6`–`r9` | Callee-saved (preserved across helper calls) |
| `r10` | Frame pointer (read-only; points to 512-byte stack) |

The eBPF ISA has **11 registers** — deliberately similar to ARM64's calling convention. This makes JIT compilation to ARM (Jetson, mobile SoCs) very efficient: eBPF registers map **nearly 1:1 to hardware registers**.

**Instruction Set**

```
eBPF Instruction Format (64-bit):
┌──────────┬──────┬──────┬──────────┬────────────────────────────────┐
│  opcode  │ dst  │ src  │  offset  │           immediate            │
│  8 bits  │4 bits│4 bits│ 16 bits  │           32 bits              │
└──────────┴──────┴──────┴──────────┴────────────────────────────────┘
```

| Category | Instructions | Example |
|---|---|---|
| ALU (64-bit) | `add`, `sub`, `mul`, `div`, `mod`, `or`, `and`, `xor`, `lsh`, `rsh`, `arsh`, `neg` | `r0 += r1` |
| ALU (32-bit) | Same ops with `w` suffix | `w0 += w1` (32-bit add) |
| Memory | `ldx`, `stx`, `st` (1/2/4/8 byte) | `r0 = *(u64 *)(r1 + 16)` |
| Branch | `jeq`, `jne`, `jgt`, `jge`, `jlt`, `jle`, `jset` | `if r0 > r1 goto +5` |
| Call | `call imm` | `call bpf_ktime_get_ns` |
| Exit | `exit` | Return `r0` to caller |
| Atomic | `lock xadd`, `lock cmpxchg`, `lock xchg` | Atomic counter increment |

**No floating point.** eBPF has integer-only ALU. Latency histograms use integer nanoseconds; throughput calculations use integer bytes/sec. This is by design — floating-point exceptions in kernel context are dangerous and non-deterministic.

**JIT Compilation**

The kernel JIT-compiles eBPF bytecode to **native machine instructions** at load time:

| Architecture | JIT Quality | Notes |
|---|---|---|
| x86-64 | Excellent | Near 1:1 mapping; eBPF was designed with x86 in mind |
| ARM64 (AArch64) | Excellent | Jetson Orin, Qualcomm SoCs — register mapping is natural |
| ARM32 | Good | Older embedded, some Cortex-A devices |
| RISC-V | Good | Growing support; relevant for open-source AI chips |

After JIT, eBPF programs run at **native instruction speed** — not interpreted. The overhead per probe hit is typically **50–200 ns**, dominated by the function call trampoline, not the eBPF instructions.

---

**The Verifier: Why eBPF Is Safe**

Before any eBPF program executes, the kernel verifier performs **static analysis** on every possible execution path. This is what makes eBPF safe for production — a buggy eBPF program is **rejected at load time, never at runtime**.

</details>

### 验证检查

| 检查项 | 防止的问题 |
|---|---|
| **可达性** | 所有指令必须可达；不允许隐藏恶意路径的死代码 |
| **无不可达指令** | 程序必须在所有路径上通过 `exit` 终止 |
| **有界循环** | 循环必须具有可证明的有界迭代次数（自 Linux 5.3 起：允许有界循环；在此之前：完全不允许循环） |
| **内存安全** | 每次指针解引用都要做边界检查；不允许任意内核内存访问 |
| **类型追踪** | 寄存器被追踪为 `NOT_INIT`、`SCALAR`、`PTR_TO_MAP_VALUE`、`PTR_TO_CTX` 等 —— 不能把标量当作指针使用 |
| **栈边界** | 栈访问必须位于 512 字节栈帧内；不允许缓冲区溢出 |
| **辅助函数参数类型** | 每个辅助函数都指定了期望的参数类型；验证器会检查二者是否匹配 |
| **特权级别** | 非特权用户只能运行 cgroup/socket 程序；链路追踪需要 `CAP_BPF` 或 `CAP_SYS_ADMIN` |

### 验证示例

```c
// This program PASSES verification:
SEC("tracepoint/syscalls/sys_enter_ioctl")
int trace_ioctl(struct trace_event_raw_sys_enter *ctx) {
    u64 ts = bpf_ktime_get_ns();           // helper call — verifier knows return type
    u32 pid = bpf_get_current_pid_tgid();   // another safe helper
    bpf_map_update_elem(&start_ts, &pid, &ts, BPF_ANY);  // map access — verifier checks key/value sizes
    return 0;                                // explicit exit
}

// This program FAILS verification:
SEC("tracepoint/syscalls/sys_enter_ioctl")
int bad_program(struct trace_event_raw_sys_enter *ctx) {
    char *ptr = (char *)0xffff888000000000;  // arbitrary kernel pointer
    char c = *ptr;                            // REJECTED: direct kernel memory access
    return 0;
}
// verifier error: "R1 type=scalar expected=fp"
```

### 复杂度限制

| 限制项 | 数值（Linux 6.x） | 目的 |
|---|---|---|
| 最大指令数 | 1,000,000 | 防止验证耗时过长 |
| 最大已验证状态数 | 每条指令 64 个 | 限制验证器内存 |
| 栈大小 | 512 字节 | 固定；无动态分配 |
| 尾调用深度 | 33 | 防止无限递归 |
| 最大 map 条目数 | 每个 map 可配置 | 内存预算 |
| 辅助函数调用嵌套 | 8 | 限制栈深度 |

> **关键洞察：** 验证器正是 eBPF 在生产环境中被信任的原因。与内核模块不同 —— 内核模块可以解引用任意指针、破坏任意数据结构、使系统崩溃 —— eBPF 程序在运行之前就被数学证明是安全的。代价是表达能力：你无法用 eBPF 编写任意的内核代码。但对于可观测性和网络而言，这种受限模型已经足够，而安全性保证则价值无可估量。

---

## BPF Maps：内核↔用户态数据结构

BPF maps 是 eBPF 程序（内核侧）与用户态应用之间的**共享数据结构**。它们是把数据从 eBPF 程序中取出的主要机制。

| Map 类型 | 用途 | AI/嵌入式示例 |
|---|---|---|
| `BPF_MAP_TYPE_HASH` | 键值存储 | 按 PID 追踪 ioctl 延迟 |
| `BPF_MAP_TYPE_ARRAY` | 固定大小的索引数组 | 按 CPU 统计中断频率的计数器 |
| `BPF_MAP_TYPE_RINGBUF` | 无锁 SPSC 环形缓冲区（首选） | 把带时间戳的事件流送往用户态 |
| `BPF_MAP_TYPE_PERF_EVENT_ARRAY` | 按 CPU 的 perf 事件环 | 旧式事件流（建议改用 ringbuf） |
| `BPF_MAP_TYPE_PERCPU_HASH` | 按 CPU 的哈希（无加锁） | 并发的按 CPU 统计 |
| `BPF_MAP_TYPE_LRU_HASH` | 自动淘汰的哈希 | 追踪近期连接而不无限增长 |
| `BPF_MAP_TYPE_STACK_TRACE` | 内核/用户态栈捕获 | 剖析 modeld 在内核中的耗时分布 |
| `BPF_MAP_TYPE_PROG_ARRAY` | 尾调用分发表 | 串联 eBPF 程序以实现复杂链路追踪逻辑 |
| `BPF_MAP_TYPE_BLOOM_FILTER` | 概率性集合成员判定 | 用于链路追踪的快速 PID 过滤 |


<details>
<summary>English original</summary>

**Verification Checks**

| Check | What It Prevents |
|---|---|
| **Reachability** | All instructions must be reachable; no dead code hiding malicious paths |
| **No unreachable instructions** | Program must terminate via `exit` on all paths |
| **Bounded loops** | Loops must have provably bounded iteration count (since Linux 5.3: bounded loops allowed; before that, no loops at all) |
| **Memory safety** | Every pointer dereference is checked for bounds; no arbitrary kernel memory access |
| **Type tracking** | Registers are tracked as `NOT_INIT`, `SCALAR`, `PTR_TO_MAP_VALUE`, `PTR_TO_CTX`, etc. — can't use a scalar as a pointer |
| **Stack bounds** | Stack accesses must be within the 512-byte frame; no buffer overflow |
| **Helper argument types** | Each helper function specifies expected argument types; verifier checks they match |
| **Privilege level** | Unprivileged users can only run cgroup/socket programs; `CAP_BPF` or `CAP_SYS_ADMIN` required for tracing |

**Verification Example**

```c
// This program PASSES verification:
SEC("tracepoint/syscalls/sys_enter_ioctl")
int trace_ioctl(struct trace_event_raw_sys_enter *ctx) {
    u64 ts = bpf_ktime_get_ns();           // helper call — verifier knows return type
    u32 pid = bpf_get_current_pid_tgid();   // another safe helper
    bpf_map_update_elem(&start_ts, &pid, &ts, BPF_ANY);  // map access — verifier checks key/value sizes
    return 0;                                // explicit exit
}

// This program FAILS verification:
SEC("tracepoint/syscalls/sys_enter_ioctl")
int bad_program(struct trace_event_raw_sys_enter *ctx) {
    char *ptr = (char *)0xffff888000000000;  // arbitrary kernel pointer
    char c = *ptr;                            // REJECTED: direct kernel memory access
    return 0;
}
// verifier error: "R1 type=scalar expected=fp"
```

**Complexity Limits**

| Limit | Value (Linux 6.x) | Purpose |
|---|---|---|
| Max instructions | 1,000,000 | Prevent excessive verification time |
| Max verified states | 64 per instruction | Bound verifier memory |
| Stack size | 512 bytes | Fixed; no dynamic allocation |
| Tail calls depth | 33 | Prevent infinite recursion |
| Max map entries | Configurable per map | Memory budget |
| Helper call nesting | 8 | Bound stack depth |

> **Key Insight:** The verifier is the reason eBPF is trusted in production. Unlike a kernel module — which can dereference any pointer, corrupt any data structure, and crash the system — an eBPF program is mathematically proven safe before it runs. The trade-off is expressiveness: you cannot write arbitrary kernel code in eBPF. But for observability and networking, the restricted model is sufficient and the safety guarantee is invaluable.

---

**BPF Maps: Kernel↔User Data Structures**

BPF maps are **shared data structures** between eBPF programs (kernel side) and user-space applications. They are the primary mechanism for getting data out of eBPF programs.

| Map Type | Use Case | AI/Embedded Example |
|---|---|---|
| `BPF_MAP_TYPE_HASH` | Key-value store | Per-PID ioctl latency tracking |
| `BPF_MAP_TYPE_ARRAY` | Fixed-size indexed array | Per-CPU counters for interrupt frequency |
| `BPF_MAP_TYPE_RINGBUF` | Lock-free SPSC ring buffer (preferred) | Stream of timestamped events to user space |
| `BPF_MAP_TYPE_PERF_EVENT_ARRAY` | Per-CPU perf event ring | Legacy event streaming (prefer ringbuf) |
| `BPF_MAP_TYPE_PERCPU_HASH` | Per-CPU hash (no locking) | Concurrent per-CPU statistics |
| `BPF_MAP_TYPE_LRU_HASH` | Auto-evicting hash | Track recent connections without unbounded growth |
| `BPF_MAP_TYPE_STACK_TRACE` | Kernel/user stack capture | Profile where modeld spends time in kernel |
| `BPF_MAP_TYPE_PROG_ARRAY` | Tail call dispatch table | Chain eBPF programs for complex tracing logic |
| `BPF_MAP_TYPE_BLOOM_FILTER` | Probabilistic set membership | Fast PID filtering for tracing |

</details>

### Ring Buffer（BPF_MAP_TYPE_RINGBUF）

ring buffer 是**从 kernel 向用户空间流式传输事件的首选机制**。它使用**单个共享缓冲区**（不是 per-CPU），支持变长记录，并且具有极佳的 cache 行为。

```c
// Kernel side (eBPF program)
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);      // 256 KB ring buffer
} events SEC(".maps");

struct event {
    u64 timestamp_ns;
    u32 pid;
    u32 latency_us;
    char comm[16];
};

SEC("tracepoint/syscalls/sys_exit_ioctl")
int trace_ioctl_exit(struct trace_event_raw_sys_exit *ctx) {
    struct event *e;
    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e) return 0;  // ring full — drop event (safe)

    e->timestamp_ns = bpf_ktime_get_ns();
    e->pid = bpf_get_current_pid_tgid() >> 32;
    e->latency_us = /* computed from start timestamp */;
    bpf_get_current_comm(&e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);  // publish to user space
    return 0;
}
```

```c
// User side (C application using libbpf)
static int handle_event(void *ctx, void *data, size_t data_sz) {
    struct event *e = data;
    printf("%-16s pid=%-6d latency=%u us\n", e->comm, e->pid, e->latency_us);
    return 0;
}

struct ring_buffer *rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                                          handle_event, NULL, NULL);
while (!stop) {
    ring_buffer__poll(rb, 100 /* timeout ms */);
}
```

> **关键洞察：** `BPF_MAP_TYPE_RINGBUF` 取代 `BPF_MAP_TYPE_PERF_EVENT_ARRAY` 成为首选的事件流式传输机制。ring buffer 是单个共享缓冲区（不是 per-CPU），因此事件按时间戳全局有序——这对于跨不同 CPU 关联摄像头帧事件与 GPU 完成事件至关重要。per-CPU perf buffer 需要在用户空间做合并与排序，这会增加延迟和复杂度。

---

## 用 libbpf 编写 eBPF 程序

**libbpf** 是用于加载 eBPF 程序并与之交互的规范 C 库。它提供了 "CO-RE"（Compile Once — Run Everywhere）机制，使 eBPF 程序可跨 kernel 版本移植。

### CO-RE：编译一次，到处运行

问题在于：**kernel 数据结构会随版本变化**。一个 `struct task_struct` 字段在 kernel 5.10 中可能位于偏移 1248，而在 kernel 6.1 中位于偏移 1264。没有 CO-RE，就得为每个 kernel 版本分别编译 eBPF 程序。

CO-RE 用 **BTF（BPF Type Format）** 解决这个问题——BTF 是嵌入 kernel 的紧凑类型元数据，描述了运行时所有结构的布局。

```c
// Without CO-RE — fragile, breaks across kernel versions:
u32 pid = *(u32 *)((char *)task + 1248);  // hardcoded offset!

// With CO-RE — portable:
u32 pid = BPF_CORE_READ(task, tgid);
// At load time, libbpf reads kernel BTF and adjusts the offset automatically
```


<details>
<summary>English original</summary>

**Ring Buffer (BPF_MAP_TYPE_RINGBUF)**

The ring buffer is the **preferred mechanism for streaming events** from kernel to user space. It uses a **single shared buffer** (not per-CPU), supports variable-length records, and has excellent cache behavior.

```c
// Kernel side (eBPF program)
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);      // 256 KB ring buffer
} events SEC(".maps");

struct event {
    u64 timestamp_ns;
    u32 pid;
    u32 latency_us;
    char comm[16];
};

SEC("tracepoint/syscalls/sys_exit_ioctl")
int trace_ioctl_exit(struct trace_event_raw_sys_exit *ctx) {
    struct event *e;
    e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e) return 0;  // ring full — drop event (safe)

    e->timestamp_ns = bpf_ktime_get_ns();
    e->pid = bpf_get_current_pid_tgid() >> 32;
    e->latency_us = /* computed from start timestamp */;
    bpf_get_current_comm(&e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);  // publish to user space
    return 0;
}
```

```c
// User side (C application using libbpf)
static int handle_event(void *ctx, void *data, size_t data_sz) {
    struct event *e = data;
    printf("%-16s pid=%-6d latency=%u us\n", e->comm, e->pid, e->latency_us);
    return 0;
}

struct ring_buffer *rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                                          handle_event, NULL, NULL);
while (!stop) {
    ring_buffer__poll(rb, 100 /* timeout ms */);
}
```

> **Key Insight:** `BPF_MAP_TYPE_RINGBUF` replaced `BPF_MAP_TYPE_PERF_EVENT_ARRAY` as the preferred event streaming mechanism. The ring buffer is a single shared buffer (not per-CPU), so events are globally ordered by timestamp — critical for correlating camera frame events with GPU completion events across different CPUs. Per-CPU perf buffers require user-space merging and sorting, which adds latency and complexity.

---

**Writing eBPF Programs with libbpf**

**libbpf** is the canonical C library for loading and interacting with eBPF programs. It provides the "CO-RE" (Compile Once — Run Everywhere) mechanism that makes eBPF programs portable across kernel versions.

**CO-RE: Compile Once, Run Everywhere**

The problem: **kernel data structures change between versions**. A `struct task_struct` field might be at offset 1248 in kernel 5.10 and offset 1264 in kernel 6.1. Without CO-RE, you'd need to compile eBPF programs per kernel version.

CO-RE solves this with **BTF (BPF Type Format)** — a compact type metadata embedded in the kernel that describes the layout of all structures at runtime.

```c
// Without CO-RE — fragile, breaks across kernel versions:
u32 pid = *(u32 *)((char *)task + 1248);  // hardcoded offset!

// With CO-RE — portable:
u32 pid = BPF_CORE_READ(task, tgid);
// At load time, libbpf reads kernel BTF and adjusts the offset automatically
```

</details>

### 完整的 libbpf 程序：追踪 V4L2 ioctl 延迟

本示例测量来自 `camerad` 的**每次 V4L2 ioctl 调用的延迟** —— 即 openpilot 中的相机 pipeline 进程。

**BPF 程序（`ioctl_lat.bpf.c`）：**

```c
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#define MAX_ENTRIES 10240
#define TASK_COMM_LEN 16

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, u32);          // tid
    __type(value, u64);        // start timestamp
} start SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

struct event {
    u64 ts;
    u32 pid;
    u32 tid;
    u64 latency_ns;
    int ret;
    unsigned int cmd;
    char comm[TASK_COMM_LEN];
};

// Filter: only trace ioctl from "camerad"
static __always_inline bool should_trace(void) {
    char comm[TASK_COMM_LEN];
    bpf_get_current_comm(&comm, sizeof(comm));
    // Compare first 7 chars: "camerad"
    return comm[0] == 'c' && comm[1] == 'a' && comm[2] == 'm' &&
           comm[3] == 'e' && comm[4] == 'r' && comm[5] == 'a' &&
           comm[6] == 'd';
}

SEC("tracepoint/syscalls/sys_enter_ioctl")
int trace_ioctl_enter(struct trace_event_raw_sys_enter *ctx) {
    if (!should_trace()) return 0;

    u32 tid = (u32)bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&start, &tid, &ts, BPF_ANY);
    return 0;
}

SEC("tracepoint/syscalls/sys_exit_ioctl")
int trace_ioctl_exit(struct trace_event_raw_sys_exit *ctx) {
    if (!should_trace()) return 0;

    u32 tid = (u32)bpf_get_current_pid_tgid();
    u64 *tsp = bpf_map_lookup_elem(&start, &tid);
    if (!tsp) return 0;

    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e) {
        bpf_map_delete_elem(&start, &tid);
        return 0;
    }

    u64 now = bpf_ktime_get_ns();
    e->ts = now;
    e->pid = bpf_get_current_pid_tgid() >> 32;
    e->tid = tid;
    e->latency_ns = now - *tsp;
    e->ret = ctx->ret;
    bpf_get_current_comm(&e->comm, sizeof(e->comm));

    bpf_ringbuf_submit(e, 0);
    bpf_map_delete_elem(&start, &tid);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
```

**用户态加载器（`ioctl_lat.c`）：**

```c
#include <stdio.h>
#include <signal.h>
#include <bpf/libbpf.h>
#include "ioctl_lat.skel.h"  // auto-generated by bpftool gen skeleton

static volatile bool running = true;
static void sig_handler(int sig) { running = false; }

static int handle_event(void *ctx, void *data, size_t data_sz) {
    struct event *e = data;
    printf("%-8.3f %-16s pid=%-6d tid=%-6d latency=%.3f ms ret=%d\n",
           (double)e->ts / 1e9, e->comm, e->pid, e->tid,
           (double)e->latency_ns / 1e6, e->ret);
    return 0;
}

int main(void) {
    signal(SIGINT, sig_handler);

    // Open, load, and attach BPF programs
    struct ioctl_lat_bpf *skel = ioctl_lat_bpf__open_and_load();
    if (!skel) { fprintf(stderr, "Failed to load BPF\n"); return 1; }

    ioctl_lat_bpf__attach(skel);

    // Set up ring buffer polling
    struct ring_buffer *rb = ring_buffer__new(
        bpf_map__fd(skel->maps.events), handle_event, NULL, NULL);

    printf("Tracing camerad ioctl latency... Ctrl-C to stop.\n");
    while (running) {
        ring_buffer__poll(rb, 100);
    }

    ring_buffer__free(rb);
    ioctl_lat_bpf__destroy(skel);
    return 0;
}
```

**构建：**
```bash
# Generate vmlinux.h from kernel BTF
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# Compile BPF program
clang -g -O2 -target bpf -D__TARGET_ARCH_arm64 -c ioctl_lat.bpf.c -o ioctl_lat.bpf.o

# Generate skeleton header
bpftool gen skeleton ioctl_lat.bpf.o > ioctl_lat.skel.h

# Compile user-space loader
gcc -o ioctl_lat ioctl_lat.c -lbpf -lelf -lz
```

---

## eBPF 程序类型与挂载点

eBPF 程序挂载到**不同的内核子系统**。每种程序类型都有特定的上下文结构和一组可用的辅助函数。

### 跟踪程序类型

| 程序类型 | 钩子 | 上下文 | 用例 |
|---|---|---|---|
| `BPF_PROG_TYPE_TRACEPOINT` | 静态 kernel tracepoint | `struct trace_event_raw_*` | 稳定、可移植：`sched:sched_switch`、`irq:irq_handler_entry` |
| `BPF_PROG_TYPE_KPROBE` | 任意 kernel 函数 | `struct pt_regs`（寄存器） | 深度驱动跟踪：GPU 命令提交、DMA 引擎 |
| `BPF_PROG_TYPE_KRETPROBE` | kernel 函数返回 | `struct pt_regs` | 测量函数耗时 |
| `BPF_PROG_TYPE_FENTRY` / `FEXIT` | BPF trampoline（更快） | 直接获取函数参数 | 低开销跟踪（5.5+）；优于 kprobe |
| `BPF_PROG_TYPE_UPROBE` | 用户态函数 | `struct pt_regs` | 跟踪 TensorRT API 调用、Python 函数入口 |
| `BPF_PROG_TYPE_RAW_TRACEPOINT` | 原始 tracepoint（无格式化） | 原始 `struct` | 开销最低的跟踪 |
| `BPF_PROG_TYPE_PERF_EVENT` | 硬件 PMU / 软件事件 | `struct bpf_perf_event_data` | 剖析缓存未命中、分支预测失败 |

### 网络程序类型

| 程序类型 | 钩子 | 用例 |
|---|---|---|
| `BPF_PROG_TYPE_XDP` | NIC 驱动（sk_buff 之前） | 线速报文过滤、DDoS 缓解 |
| `BPF_PROG_TYPE_SCHED_CLS`（TC） | 流量控制入口/出口 | 报文修改、QoS、延迟标记 |
| `BPF_PROG_TYPE_CGROUP_SKB` | 每 cgroup 的 socket 缓冲区 | 容器级网络策略 |
| `BPF_PROG_TYPE_SK_MSG` | socket 消息层级 | 透明代理、service mesh |
| `BPF_PROG_TYPE_SOCK_OPS` | TCP 事件回调 | 每连接拥塞控制 |

### 安全与调度程序类型

| 程序类型 | 钩子 | 用例 |
|---|---|---|
| `BPF_PROG_TYPE_LSM` | Linux Security Module 钩子 | 运行时安全策略强制执行 |
| `BPF_PROG_TYPE_STRUCT_OPS` | 内核 struct ops 替换 | 自定义 TCP 拥塞控制、自定义调度器（sched_ext） |

### fentry/fexit vs. kprobe（性能）

```
kprobe overhead:     ~100–200 ns per hit
fentry/fexit overhead: ~10–50 ns per hit  (4–10× faster)
```

`fentry`/`fexit` 使用 **BPF trampoline**——内核修补函数序言，直接调用 eBPF 程序，避免 kprobe 所用的 `int3` 断点机制。**在 kernel ≥ 5.5 上始终优先使用 `fentry`/`fexit`。**

```c
// fentry — trace entry to the v4l2_ioctl kernel function
SEC("fentry/video_ioctl2")
int BPF_PROG(trace_v4l2_entry, struct file *file, unsigned int cmd, unsigned long arg) {
    // Direct access to function arguments — no pt_regs parsing needed
    u32 tid = (u32)bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&start, &tid, &ts, BPF_ANY);
    return 0;
}

// fexit — trace return
SEC("fexit/video_ioctl2")
int BPF_PROG(trace_v4l2_exit, struct file *file, unsigned int cmd, unsigned long arg, int ret) {
    // 'ret' is the return value — available as the last argument
    // ... compute latency, emit event ...
    return 0;
}
```

---

## 面向 AI 系统可观测性的 eBPF

### 1. GPU 驱动延迟跟踪

跟踪 NVIDIA GPU 驱动的 ioctl，以测量 Jetson 上的命令提交与完成延迟：

```bash
# Trace all ioctls to /dev/nvhost-gpu and /dev/nvgpu
bpftrace -e '
tracepoint:syscalls:sys_enter_ioctl
/comm == "modeld" || comm == "camerad"/ {
    @start[tid] = nsecs;
    @cmd[tid] = args->cmd;
}

tracepoint:syscalls:sys_exit_ioctl
/comm == "modeld" || comm == "camerad"/ {
    if (@start[tid]) {
        $lat = (nsecs - @start[tid]) / 1000;
        @latency_us = hist($lat);
        if ($lat > 1000) {
            printf("SLOW ioctl: %s tid=%d cmd=0x%x lat=%d us ret=%d\n",
                   comm, tid, @cmd[tid], $lat, args->ret);
        }
        delete(@start[tid]);
        delete(@cmd[tid]);
    }
}'
```

### 2. 面向 RT 推理线程的调度器分析

```bash
# How long does modeld wait in the run queue before getting CPU time?
bpftrace -e '
tracepoint:sched:sched_wakeup /args->comm == "modeld"/ {
    @wake[args->pid] = nsecs;
}

tracepoint:sched:sched_switch /args->next_comm == "modeld"/ {
    if (@wake[args->next_pid]) {
        @runq_latency_us = hist((nsecs - @wake[args->next_pid]) / 1000);
        delete(@wake[args->next_pid]);
    }
}'
# If the histogram shows a long tail (>1ms), modeld is being preempted.
# Solutions: SCHED_FIFO, isolcpus, or SCHED_DEADLINE
```

### 3. DMA 传输性能剖析

```bash
# Trace DMA-related functions in the kernel
# Useful for understanding camera → model data pipeline
bpftrace -e '
kprobe:dma_map_page { @dma_map[comm] = count(); }
kretprobe:dma_map_page { @dma_map_lat = hist(nsecs - @start[tid]); }
'

# Memory-mapped I/O tracing for FPGA accelerators
bpftrace -e '
kprobe:pci_iomap { printf("PCI IOMAP: %s bar=%d\n", comm, arg1); }
'
```


<details>
<summary>English original</summary>

**Tracing Program Types**

| Program Type | Hook | Context | Use Case |
|---|---|---|---|
| `BPF_PROG_TYPE_TRACEPOINT` | Static kernel tracepoints | `struct trace_event_raw_*` | Stable, portable: `sched:sched_switch`, `irq:irq_handler_entry` |
| `BPF_PROG_TYPE_KPROBE` | Any kernel function | `struct pt_regs` (registers) | Deep driver tracing: GPU command submit, DMA engine |
| `BPF_PROG_TYPE_KRETPROBE` | Kernel function return | `struct pt_regs` | Measure function duration |
| `BPF_PROG_TYPE_FENTRY` / `FEXIT` | BPF trampoline (faster) | Function arguments directly | Low-overhead tracing (5.5+); preferred over kprobe |
| `BPF_PROG_TYPE_UPROBE` | User-space function | `struct pt_regs` | Trace TensorRT API calls, Python function entry |
| `BPF_PROG_TYPE_RAW_TRACEPOINT` | Raw tracepoint (no formatting) | Raw `struct` | Lowest overhead tracing |
| `BPF_PROG_TYPE_PERF_EVENT` | Hardware PMU / software events | `struct bpf_perf_event_data` | Profile cache misses, branch mispredictions |

**Networking Program Types**

| Program Type | Hook | Use Case |
|---|---|---|
| `BPF_PROG_TYPE_XDP` | NIC driver (before sk_buff) | Line-rate packet filtering, DDoS mitigation |
| `BPF_PROG_TYPE_SCHED_CLS` (TC) | Traffic control ingress/egress | Packet modification, QoS, latency tagging |
| `BPF_PROG_TYPE_CGROUP_SKB` | Per-cgroup socket buffer | Container-level network policy |
| `BPF_PROG_TYPE_SK_MSG` | Socket message level | Transparent proxy, service mesh |
| `BPF_PROG_TYPE_SOCK_OPS` | TCP event callbacks | Per-connection congestion control |

**Security & Scheduling Program Types**

| Program Type | Hook | Use Case |
|---|---|---|
| `BPF_PROG_TYPE_LSM` | Linux Security Module hooks | Runtime security policy enforcement |
| `BPF_PROG_TYPE_STRUCT_OPS` | Kernel struct ops replacement | Custom TCP congestion control, custom scheduler (sched_ext) |

**fentry/fexit vs. kprobe (Performance)**

```
kprobe overhead:     ~100–200 ns per hit
fentry/fexit overhead: ~10–50 ns per hit  (4–10× faster)
```

`fentry`/`fexit` use **BPF trampolines** — the kernel patches the function prologue to call the eBPF program directly, avoiding the `int3` breakpoint mechanism that kprobes use. **Always prefer `fentry`/`fexit` on kernels ≥ 5.5.**

```c
// fentry — trace entry to the v4l2_ioctl kernel function
SEC("fentry/video_ioctl2")
int BPF_PROG(trace_v4l2_entry, struct file *file, unsigned int cmd, unsigned long arg) {
    // Direct access to function arguments — no pt_regs parsing needed
    u32 tid = (u32)bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();
    bpf_map_update_elem(&start, &tid, &ts, BPF_ANY);
    return 0;
}

// fexit — trace return
SEC("fexit/video_ioctl2")
int BPF_PROG(trace_v4l2_exit, struct file *file, unsigned int cmd, unsigned long arg, int ret) {
    // 'ret' is the return value — available as the last argument
    // ... compute latency, emit event ...
    return 0;
}
```

---

**eBPF for AI System Observability**

**1. GPU Driver Latency Tracing**

Trace NVIDIA GPU driver ioctls to measure command submission and completion latency on Jetson:

```bash
# Trace all ioctls to /dev/nvhost-gpu and /dev/nvgpu
bpftrace -e '
tracepoint:syscalls:sys_enter_ioctl
/comm == "modeld" || comm == "camerad"/ {
    @start[tid] = nsecs;
    @cmd[tid] = args->cmd;
}

tracepoint:syscalls:sys_exit_ioctl
/comm == "modeld" || comm == "camerad"/ {
    if (@start[tid]) {
        $lat = (nsecs - @start[tid]) / 1000;
        @latency_us = hist($lat);
        if ($lat > 1000) {
            printf("SLOW ioctl: %s tid=%d cmd=0x%x lat=%d us ret=%d\n",
                   comm, tid, @cmd[tid], $lat, args->ret);
        }
        delete(@start[tid]);
        delete(@cmd[tid]);
    }
}'
```

**2. Scheduler Analysis for RT Inference Threads**

```bash
# How long does modeld wait in the run queue before getting CPU time?
bpftrace -e '
tracepoint:sched:sched_wakeup /args->comm == "modeld"/ {
    @wake[args->pid] = nsecs;
}

tracepoint:sched:sched_switch /args->next_comm == "modeld"/ {
    if (@wake[args->next_pid]) {
        @runq_latency_us = hist((nsecs - @wake[args->next_pid]) / 1000);
        delete(@wake[args->next_pid]);
    }
}'
# If the histogram shows a long tail (>1ms), modeld is being preempted.
# Solutions: SCHED_FIFO, isolcpus, or SCHED_DEADLINE
```

**3. DMA Transfer Profiling**

```bash
# Trace DMA-related functions in the kernel
# Useful for understanding camera → model data pipeline
bpftrace -e '
kprobe:dma_map_page { @dma_map[comm] = count(); }
kretprobe:dma_map_page { @dma_map_lat = hist(nsecs - @start[tid]); }
'

# Memory-mapped I/O tracing for FPGA accelerators
bpftrace -e '
kprobe:pci_iomap { printf("PCI IOMAP: %s bar=%d\n", comm, arg1); }
'
```

</details>

### 4. 推理流水线端到端延迟

构建一个自定义 tracer，测量完整流水线：相机帧到达 → ISP 处理 → 模型推理 → 控制输出：

```bash
# Trace the full openpilot pipeline
bpftrace -e '
uprobe:/data/openpilot/selfdrive/modeld/modeld:run_model {
    @model_start[tid] = nsecs;
}
uretprobe:/data/openpilot/selfdrive/modeld/modeld:run_model {
    if (@model_start[tid]) {
        @model_latency_ms = hist((nsecs - @model_start[tid]) / 1000000);
        delete(@model_start[tid]);
    }
}'
```

### 5. 内存分配追踪

```bash
# Track large allocations from AI processes (potential OOM debugging)
bpftrace -e '
tracepoint:kmem:mm_page_alloc /args->order >= 4/ {
    printf("%s pid=%d order=%d (%d KB)\n",
           comm, pid, args->order, (1 << args->order) * 4);
    @large_allocs[comm] = count();
}'

# Track CMA (Contiguous Memory Allocator) for camera DMA buffers
bpftrace -e '
tracepoint:cma:cma_alloc_start { @cma_start[tid] = nsecs; }
tracepoint:cma:cma_alloc_finish {
    @cma_latency = hist((nsecs - @cma_start[tid]) / 1000);
    delete(@cma_start[tid]);
}'
```

---

## XDP：可编程数据包处理

**XDP（eXpress Data Path）** 在 **NIC 驱动层**运行 eBPF 程序，此时内核尚未分配 `sk_buff` 结构体。这使得数据包处理能达到**每秒数百万个数据包**，且零内存分配开销。

### XDP 动作

| 返回码 | 动作 | 包/秒 (10GbE) |
|---|---|---|
| `XDP_DROP` | 在 NIC 处丢弃数据包 | ~24 Mpps |
| `XDP_PASS` | 发送到常规内核协议栈 | ~1–5 Mpps |
| `XDP_TX` | 将数据包从同一块 NIC 反弹发出 | ~20 Mpps |
| `XDP_REDIRECT` | 发送到不同的 NIC、CPU 或 socket | ~15 Mpps |

### XDP 用于 AV 遥测过滤

在自动驾驶汽车部署中，计算单元通过以太网接收**高带宽传感器数据**（相机、LiDAR（激光雷达）、雷达）。XDP 可以在**无内核开销**的情况下过滤这些流量并区分其优先级：

```c
SEC("xdp")
int xdp_sensor_filter(struct xdp_md *ctx) {
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;

    struct ethhdr *eth = data;
    if (data + sizeof(*eth) > data_end) return XDP_DROP;

    // Pass camera frames (specific EtherType or VLAN)
    if (eth->h_proto == htons(0x88B5))  // IEEE 802.1 local experimental
        return XDP_PASS;

    // Drop non-essential traffic (telemetry, debug) under CPU pressure
    struct iphdr *ip = data + sizeof(*eth);
    if (data + sizeof(*eth) + sizeof(*ip) > data_end) return XDP_DROP;

    // High-priority: LiDAR data (UDP port 2368 — Velodyne default)
    if (ip->protocol == IPPROTO_UDP) {
        struct udphdr *udp = (void *)ip + sizeof(*ip);
        if ((void *)udp + sizeof(*udp) > data_end) return XDP_DROP;
        if (udp->dest == htons(2368)) return XDP_PASS;
    }

    return XDP_DROP;  // drop everything else
}
```

---

## sched_ext：用 eBPF 写自定义调度器

从 Linux 6.12 起，**sched_ext** 允许把 **CPU 调度器写成 eBPF 程序**——这是 AI 工作负载优化的一项革命性能力。

```c
// Define scheduling callbacks as eBPF programs
SEC("struct_ops/enqueue")
void BPF_PROG(enqueue, struct task_struct *p, u64 enq_flags) {
    // Custom logic: if this is modeld, enqueue to high-priority DSQ
    if (is_inference_task(p))
        scx_bpf_dispatch(p, HIGH_PRIO_DSQ, SCX_SLICE_DFL, enq_flags);
    else
        scx_bpf_dispatch(p, DEFAULT_DSQ, SCX_SLICE_DFL, enq_flags);
}

SEC("struct_ops/dispatch")
void BPF_PROG(dispatch, s32 cpu, struct task_struct *prev) {
    // Drain high-priority DSQ first (inference tasks)
    scx_bpf_consume(HIGH_PRIO_DSQ);
    // Then default
    scx_bpf_consume(DEFAULT_DSQ);
}
```

这带来：
- **推理优先调度**：modeld 总是先于非关键任务拿到 CPU
- **核心亲和性策略**：把推理线程绑定到特定核心，无需 `isolcpus`
- **延迟感知调度**：相机帧到达时抢占后台任务
- **自定义负载均衡**：根据工作负载特征把 AI 流水线各阶段分布到各核心上

> **关键洞察：** sched_ext 意味着你可以为自己的 AI 工作负载实现一个自定义调度器原型，在生产环境中测试并迭代——全程无需编写内核模块，也无需重启。对于调度确定性属于安全关键的自动驾驶系统而言，这是变革性的：你可以构建一个调度器，保证 `modeld` 等待 CPU 时间从不超过 100 µs，并用 eBPF 链路追踪来证明这一点。

---


<details>
<summary>English original</summary>

**4. Inference Pipeline End-to-End Latency**

Build a custom tracer that measures the full pipeline: camera frame arrival → ISP processing → model inference → control output:

```bash
# Trace the full openpilot pipeline
bpftrace -e '
uprobe:/data/openpilot/selfdrive/modeld/modeld:run_model {
    @model_start[tid] = nsecs;
}
uretprobe:/data/openpilot/selfdrive/modeld/modeld:run_model {
    if (@model_start[tid]) {
        @model_latency_ms = hist((nsecs - @model_start[tid]) / 1000000);
        delete(@model_start[tid]);
    }
}'
```

**5. Memory Allocation Tracking**

```bash
# Track large allocations from AI processes (potential OOM debugging)
bpftrace -e '
tracepoint:kmem:mm_page_alloc /args->order >= 4/ {
    printf("%s pid=%d order=%d (%d KB)\n",
           comm, pid, args->order, (1 << args->order) * 4);
    @large_allocs[comm] = count();
}'

# Track CMA (Contiguous Memory Allocator) for camera DMA buffers
bpftrace -e '
tracepoint:cma:cma_alloc_start { @cma_start[tid] = nsecs; }
tracepoint:cma:cma_alloc_finish {
    @cma_latency = hist((nsecs - @cma_start[tid]) / 1000);
    delete(@cma_start[tid]);
}'
```

---

**XDP: Programmable Packet Processing**

**XDP (eXpress Data Path)** runs eBPF programs at the **NIC driver level**, before the kernel allocates `sk_buff` structures. This enables packet processing at **millions of packets per second** with zero memory allocation overhead.

**XDP Actions**

| Return Code | Action | Packets/sec (10GbE) |
|---|---|---|
| `XDP_DROP` | Drop packet at NIC | ~24 Mpps |
| `XDP_PASS` | Send to normal kernel stack | ~1–5 Mpps |
| `XDP_TX` | Bounce packet back out same NIC | ~20 Mpps |
| `XDP_REDIRECT` | Send to different NIC, CPU, or socket | ~15 Mpps |

**XDP for AV Telemetry Filtering**

In autonomous vehicle deployments, the compute unit receives **high-bandwidth sensor data** (cameras, LiDAR, radar) over Ethernet. XDP can filter and prioritize this traffic **without kernel overhead**:

```c
SEC("xdp")
int xdp_sensor_filter(struct xdp_md *ctx) {
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;

    struct ethhdr *eth = data;
    if (data + sizeof(*eth) > data_end) return XDP_DROP;

    // Pass camera frames (specific EtherType or VLAN)
    if (eth->h_proto == htons(0x88B5))  // IEEE 802.1 local experimental
        return XDP_PASS;

    // Drop non-essential traffic (telemetry, debug) under CPU pressure
    struct iphdr *ip = data + sizeof(*eth);
    if (data + sizeof(*eth) + sizeof(*ip) > data_end) return XDP_DROP;

    // High-priority: LiDAR data (UDP port 2368 — Velodyne default)
    if (ip->protocol == IPPROTO_UDP) {
        struct udphdr *udp = (void *)ip + sizeof(*ip);
        if ((void *)udp + sizeof(*udp) > data_end) return XDP_DROP;
        if (udp->dest == htons(2368)) return XDP_PASS;
    }

    return XDP_DROP;  // drop everything else
}
```

---

**sched_ext: Custom Schedulers with eBPF**

Since Linux 6.12, **sched_ext** allows writing **CPU schedulers as eBPF programs** — a revolutionary capability for AI workload optimization.

```c
// Define scheduling callbacks as eBPF programs
SEC("struct_ops/enqueue")
void BPF_PROG(enqueue, struct task_struct *p, u64 enq_flags) {
    // Custom logic: if this is modeld, enqueue to high-priority DSQ
    if (is_inference_task(p))
        scx_bpf_dispatch(p, HIGH_PRIO_DSQ, SCX_SLICE_DFL, enq_flags);
    else
        scx_bpf_dispatch(p, DEFAULT_DSQ, SCX_SLICE_DFL, enq_flags);
}

SEC("struct_ops/dispatch")
void BPF_PROG(dispatch, s32 cpu, struct task_struct *prev) {
    // Drain high-priority DSQ first (inference tasks)
    scx_bpf_consume(HIGH_PRIO_DSQ);
    // Then default
    scx_bpf_consume(DEFAULT_DSQ);
}
```

This enables:
- **Inference-priority scheduling**: modeld always gets CPU before non-critical tasks
- **Core-affinity policies**: pin inference threads to specific cores without `isolcpus`
- **Latency-aware scheduling**: preempt background tasks when camera frame arrives
- **Custom load balancing**: distribute AI pipeline stages across cores based on workload characteristics

> **Key Insight:** sched_ext means you can prototype a custom scheduler for your AI workload, test it in production, and iterate — all without writing a kernel module or rebooting. For autonomous driving systems where scheduling determinism is safety-critical, this is transformative: you can build a scheduler that guarantees `modeld` never waits more than 100 µs for CPU time, and prove it with eBPF tracing.

---

</details>

## eBPF 工具生态

| 工具 | 层级 | 使用场景 |
|---|---|---|
| **bpftrace** | 单行命令、脚本 | 快速排查、临时追踪 |
| **BCC**（BPF Compiler Collection） | Python + 内嵌 C | 预置工具（`runqlat`、`biolatency`、`offcputime`）、自定义工具 |
| **libbpf** | C 库 | 生产工具、CO-RE、极致性能 |
| **libbpf-rs** | Rust 绑定 | 用 Rust 编写安全的 eBPF 工具 |
| **cilium/ebpf**（Go） | Go 库 | Kubernetes 网络、云原生工具 |
| **bpftool** | CLI 工具 | 检查已加载程序、导出 map、生成 skeleton |
| **Aya** | Rust eBPF 框架 | 用 Rust 编写 eBPF 程序 |

### bpftool 命令

```bash
# List all loaded eBPF programs
bpftool prog list

# Show details of a specific program
bpftool prog show id 42

# Dump JIT-compiled instructions
bpftool prog dump jited id 42

# List all BPF maps
bpftool map list

# Dump map contents
bpftool map dump id 5

# Generate vmlinux.h for CO-RE development
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# Generate skeleton header from compiled BPF object
bpftool gen skeleton my_program.bpf.o > my_program.skel.h
```

---

## 动手练习

1. **调度器延迟直方图：** 用 `bpftrace` 测量特定进程（例如运行模型的 Python 推理脚本）的 run-queue 延迟。对比默认 CFS 与 `SCHED_FIFO` 下的延迟（用 `chrt -f 50` 设置优先级）。分别生成直方图并解释差异。

2. **ioctl 延迟追踪器：** 编写一个完整的 libbpf CO-RE 程序，追踪指定 PID 的所有 `ioctl` 调用，记录 ioctl 命令号与延迟，并通过 ring buffer 将事件流式传输到用户态进程打印。在你的开发机上构建并测试。

3. **内存分配性能分析器：** 用 `bpftrace` 追踪特定进程中大于 4096 字节的 `kmalloc` 调用。统计模型加载阶段与推理阶段各发生多少次大块分配。提出优化方案以降低推理期间的分配压力。

4. **XDP 包过滤器：** 编写一个 XDP 程序，按 IP 协议（TCP、UDP、ICMP）统计包数并丢弃 ICMP 包。将其挂载到虚拟接口（`veth`），用 `ping` 和 `iperf3` 测试。验证 ICMP 被丢弃而 TCP/UDP 正常通过。

5. **自定义 BCC 工具：** 编写一个 BCC 工具，追踪名字中含 "model" 的进程的 `read()` 和 `write()` 系统调用延迟。按秒输出汇总，包含 p50、p90、p99 延迟。用它刻画模型加载期间的 I/O 模式。

6. **sched_ext 探索（Linux 6.12+）：** 从 Linux 内核源码（`tools/sched_ext/`）构建并运行一个示例的 `sched_ext` 调度器。在混合工作负载（推理 + 后台编译）下对比 `scx_simple` 与 `scx_central` 的调度行为。分别测量各调度器下推理进程的尾延迟。

---

## 关键要点

| 概念 | 对 AI 硬件为何重要 |
|---|---|
| eBPF 虚拟机 | 可编程内核——安全、JIT 编译、原生速度 |
| 验证器 | 生产环境安全保证——有缺陷的程序在加载时被拒，而非运行时 |
| BPF map（ring buffer） | 内核到用户态的零拷贝事件流 |
| CO-RE（libbpf） | 一次编写，任意内核版本运行——可移植的可观测性 |
| fentry/fexit | 比 kprobe 快 10 倍——低开销的生产追踪 |
| XDP | NIC 层包处理——为传感器数据提供数百万 pps |
| sched_ext | 通过 eBPF 自定义 CPU 调度器——推理优先调度 |
| bpftrace 单行命令 | 第一响应工具——30 秒内诊断延迟 |

---

## 资源

* **[BPF Performance Tools](https://www.brendangregg.com/bpf-performance-tools-book.html)，作者 Brendan Gregg：** eBPF 用于系统性能分析的权威著作。涵盖 150+ 个 BCC/bpftrace 工具及实战案例。
* **[Learning eBPF](https://isovalent.com/books/learning-ebpf/)，作者 Liz Rice（O'Reilly）：** 用 libbpf 和 Go 进行 eBPF 编程的实用入门。
* **[libbpf-bootstrap](https://github.com/libbpf/libbpf-bootstrap)：** 构建 CO-RE eBPF 程序的最小脚手架——自定义工具从这里开始。
* **[bpftrace 参考指南](https://github.com/bpftrace/bpftrace/blob/master/docs/reference_guide.md)：** 完整的单行命令与脚本语法。
* **[Brendan Gregg 的 eBPF 页面](https://www.brendangregg.com/ebpf.html)：** 持续更新的 eBPF 工具、用例与性能分析方法论合集。
* **[Linux kernel：tools/sched_ext/](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/tools/sched_ext)：** sched_ext 示例调度器与文档。
* **[Cilium eBPF 文档](https://docs.cilium.io/en/stable/bpf/)：** 全面的 eBPF/XDP 网络参考。
* **[BTF 与 CO-RE](https://nakryiko.com/posts/bpf-core-reference-guide/)：** Andrii Nakryiko 的编写可移植 eBPF 程序指南。


<details>
<summary>English original</summary>

**eBPF Tooling Ecosystem**

| Tool | Level | Use Case |
|---|---|---|
| **bpftrace** | One-liners, scripts | Quick investigation, ad-hoc tracing |
| **BCC** (BPF Compiler Collection) | Python + embedded C | Pre-built tools (`runqlat`, `biolatency`, `offcputime`), custom tools |
| **libbpf** | C library | Production tools, CO-RE, maximum performance |
| **libbpf-rs** | Rust bindings | Safe eBPF tooling in Rust |
| **cilium/ebpf** (Go) | Go library | Kubernetes networking, cloud-native tools |
| **bpftool** | CLI utility | Inspect loaded programs, dump maps, generate skeletons |
| **Aya** | Rust eBPF framework | Write eBPF programs in Rust |

**bpftool Commands**

```bash
# List all loaded eBPF programs
bpftool prog list

# Show details of a specific program
bpftool prog show id 42

# Dump JIT-compiled instructions
bpftool prog dump jited id 42

# List all BPF maps
bpftool map list

# Dump map contents
bpftool map dump id 5

# Generate vmlinux.h for CO-RE development
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# Generate skeleton header from compiled BPF object
bpftool gen skeleton my_program.bpf.o > my_program.skel.h
```

---

**Hands-On Exercises**

1. **Scheduler latency histogram:** Use `bpftrace` to measure run-queue latency for a specific process (e.g., a Python inference script running a model). Compare latency under default CFS vs. `SCHED_FIFO` (use `chrt -f 50` to set priority). Produce histograms for both and explain the difference.

2. **ioctl latency tracer:** Write a complete libbpf CO-RE program that traces all `ioctl` calls from a specified PID, records the ioctl command number and latency, and streams events via ring buffer to a user-space process that prints them. Build and test on your development machine.

3. **Memory allocation profiler:** Use `bpftrace` to trace `kmalloc` calls larger than 4096 bytes from a specific process. Count how many large allocations happen during model loading vs. inference. Propose optimizations to reduce allocation pressure during inference.

4. **XDP packet filter:** Write an XDP program that counts packets by IP protocol (TCP, UDP, ICMP) and drops ICMP packets. Attach it to a virtual interface (`veth`) and test with `ping` and `iperf3`. Verify that ICMP drops while TCP/UDP passes.

5. **Custom BCC tool:** Write a BCC tool that traces the latency of `read()` and `write()` syscalls from processes that have "model" in their name. Output a per-second summary with p50, p90, p99 latencies. Use this to characterize the I/O pattern during model loading.

6. **sched_ext exploration (Linux 6.12+):** Build and run one of the example `sched_ext` schedulers from the Linux kernel source (`tools/sched_ext/`). Compare the scheduling behavior of `scx_simple` vs. `scx_central` under a mixed workload (inference + background compilation). Measure tail latency for the inference process under each scheduler.

---

**Key Takeaways**

| Concept | Why It Matters for AI Hardware |
|---|---|
| eBPF virtual machine | Programmable kernel — safe, JIT-compiled, native speed |
| Verifier | Production safety guarantee — buggy programs rejected at load, not runtime |
| BPF maps (ring buffer) | Zero-copy event streaming from kernel to user space |
| CO-RE (libbpf) | Write once, run on any kernel version — portable observability |
| fentry/fexit | 10× faster than kprobe — low-overhead production tracing |
| XDP | Packet processing at NIC level — millions of pps for sensor data |
| sched_ext | Custom CPU schedulers via eBPF — inference-priority scheduling |
| bpftrace one-liners | First responder tool — diagnose latency in 30 seconds |

---

**Resources**

* **[BPF Performance Tools](https://www.brendangregg.com/bpf-performance-tools-book.html) by Brendan Gregg:** The definitive book on eBPF for system performance analysis. Covers 150+ BCC/bpftrace tools with real-world examples.
* **[Learning eBPF](https://isovalent.com/books/learning-ebpf/) by Liz Rice (O'Reilly):** Practical introduction to eBPF programming with libbpf and Go.
* **[libbpf-bootstrap](https://github.com/libbpf/libbpf-bootstrap):** Minimal scaffolding for building CO-RE eBPF programs — start here for custom tools.
* **[bpftrace Reference Guide](https://github.com/bpftrace/bpftrace/blob/master/docs/reference_guide.md):** Complete one-liner and script syntax.
* **[Brendan Gregg's eBPF Page](https://www.brendangregg.com/ebpf.html):** Updated collection of eBPF tools, use cases, and performance analysis methodology.
* **[Linux kernel: tools/sched_ext/](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/tools/sched_ext):** sched_ext example schedulers and documentation.
* **[Cilium eBPF Documentation](https://docs.cilium.io/en/stable/bpf/):** Comprehensive eBPF/XDP networking reference.
* **[BTF and CO-RE](https://nakryiko.com/posts/bpf-core-reference-guide/):** Andrii Nakryiko's guide to writing portable eBPF programs.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-26.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-26.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
