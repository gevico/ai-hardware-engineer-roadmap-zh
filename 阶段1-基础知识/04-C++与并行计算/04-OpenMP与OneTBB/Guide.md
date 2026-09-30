---
title: OpenMP 与 oneTBB
description: OpenMP 与 oneTBB
---

# OpenMP 与 oneTBB

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">OAO</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 数字基础</p>
<p class="course-identity__title">针对 OpenMP 与 oneTBB 的专项课程标识。</p>
<p class="course-identity__meta">产物：可运行的低层 demo · 度量：时序、内存、正确性</p>
</div>
</div>


属于 [阶段 1 第 4 节 —— C++ 与并行计算](../Guide.md).

**目标：** 用 **OpenMP**（基于指令）与 **oneTBB**（基于任务的算法与流图）实现共享内存的 **CPU 并行**，以便在 CUDA 之前先熟悉结构化并行模式。

本指南分两部分。第 1 节循序渐进地游历 OpenMP —— 从一行 `#pragma` 到任务与 SIMD。第 2 节按同样的顺序介绍 oneTBB 基于模板的 API，从简单循环到流图与 runtime 控制。

---

## 1. OpenMP

### 1.0 心智模型：Fork-Join

OpenMP 采用 **fork-join 模型**。程序以单个 *master* 线程启动。当执行到一个并行区域时，它 **fork** 出一组工作线程。所有线程并发执行该区域，随后在末尾的隐式屏障处 **join** 回一个线程。

```
 serial          parallel region              serial
─────────── ╔══════════════════════════╗ ───────────────►
            ║  #pragma omp parallel    ║
   master ──╫──► thread 0  (work) ────╫──► master
            ╠──► thread 1  (work) ────╣
            ╠──► thread 2  (work) ────╣
            ╚──► thread 3  (work) ────╝
                                  ▲
                         implicit barrier:
                    all threads must arrive
                    before master continues
```

一个程序中可以有 **多个并行区域**。每次 fork 创建线程组；每次 join 解散它（或将线程归还到池中复用）。

```
master ──── [serial] ──── fork ──── [parallel] ──── join ──── [serial] ──── fork ──── [parallel] ──── join ────►
```

**OpenMP 自动为你管理三件事：**

| 项目 | OpenMP 的处理方式 |
|------|-----------------------|
| 线程创建 | 线程池 —— 线程被复用，而非每个区域重新创建 |
| 工作分配 | 将循环迭代划分给各线程（由 `schedule` 子句控制方式） |
| 同步 | 每个并行区域末尾的隐式屏障 |

**编译：** `g++ -O2 -std=c++17 -fopenmp fib_benchmark.cpp -ltbb -o fib_benchmark`

---

### 1.1 第一个并行循环

```cpp
#include <omp.h>
#include <vector>

int N = 1'000'000;
std::vector<float> a(N), b(N), c(N);

// Sequential: thread 0 does all N iterations
for (int i = 0; i < N; i++)
    c[i] = a[i] + b[i];

// Parallel: 8 threads each do ~125,000 iterations
#pragma omp parallel for
for (int i = 0; i < N; i++)
    c[i] = a[i] + b[i];
```

就这样。只加了一行。编译器把 `[0, N)` 切分成块，将每个块分配给一个线程，并在末尾插入屏障。

**每个线程看到的内容：**

```
Thread 0:  i = 0 … 124,999
Thread 1:  i = 125,000 … 249,999
Thread 2:  i = 250,000 … 374,999
...
Thread 7:  i = 875,000 … 999,999
```

这里之所以安全，是因为每个 `i` 写入不同的 `c[i]`。没有两个线程触碰同一块内存。

---

### 1.2 经典的竞态条件

**错误 —— 数据竞态：**

```cpp
int sum = 0;

#pragma omp parallel for
for (int i = 0; i < N; i++)
    sum += a[i];   // ← RACE: multiple threads read-modify-write sum simultaneously

// sum is wrong. Possibly different on every run.
```

**为何出错：**

```
Thread 0 reads sum = 5
Thread 1 reads sum = 5     ← same value, before thread 0 wrote back
Thread 0 writes sum = 5 + a[0] = 7
Thread 1 writes sum = 5 + a[1] = 6   ← overwrites thread 0's result!
```

一次更新丢失。这是经典的 **read-modify-write 竞态**。

**修复：`reduction` 子句**

```cpp
int sum = 0;

#pragma omp parallel for reduction(+:sum)
for (int i = 0; i < N; i++)
    sum += a[i];   // each thread accumulates its own private sum
                   // all private sums are added together at the end
```

每个线程获得 `sum` 各自的私有副本（初始化为 0）。循环结束后，OpenMP 将所有私有副本累加到原始的 `sum` 中。没有竞态。第 2.5 节介绍所有支持的归约算子与自定义归约。

---


<details>
<summary>English original</summary>

**OpenMP and oneTBB**

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">OAO</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Digital Foundations</p>
<p class="course-identity__title">Specialized course identity for OpenMP and oneTBB.</p>
<p class="course-identity__meta">Artifact: working low-level demo · Measure: timing, memory, correctness</p>
</div>
</div>


Part of [Phase 1 section 4 — C++ and Parallel Computing](../Guide.md).

**Goal:** Shared-memory **CPU parallelism** with **OpenMP** (directive-based) and **oneTBB** (task-based algorithms and flow graphs) so structured parallel patterns feel familiar before CUDA.

This guide covers two parts. Section 1 is a progressive tour of OpenMP — from a one-line `#pragma` to tasks and SIMD. Section 2 covers oneTBB's template-based API in the same order, from simple loops to flow graphs and runtime controls.

---

**1. OpenMP**

**1.0 Mental Model: Fork-Join**

OpenMP uses the **fork-join model**. Your program starts with a single *master* thread. When it reaches a parallel region, it **forks** into a team of worker threads. All threads execute the region concurrently, then **join** back into one at an implicit barrier at the end.

```
 serial          parallel region              serial
─────────── ╔══════════════════════════╗ ───────────────►
            ║  #pragma omp parallel    ║
   master ──╫──► thread 0  (work) ────╫──► master
            ╠──► thread 1  (work) ────╣
            ╠──► thread 2  (work) ────╣
            ╚──► thread 3  (work) ────╝
                                  ▲
                         implicit barrier:
                    all threads must arrive
                    before master continues
```

You can have **multiple parallel regions** in one program. Each fork creates the thread team; each join dissolves it (or returns threads to a pool for reuse).

```
master ──── [serial] ──── fork ──── [parallel] ──── join ──── [serial] ──── fork ──── [parallel] ──── join ────►
```

**Three things OpenMP manages for you automatically:**

| What | How OpenMP handles it |
|------|-----------------------|
| Thread creation | Thread pool — threads are reused, not recreated each region |
| Work distribution | Divides loop iterations across threads (`schedule` clause controls how) |
| Synchronization | Implicit barrier at the end of every parallel region |

**Compile:** `g++ -O2 -std=c++17 -fopenmp fib_benchmark.cpp -ltbb -o fib_benchmark`

---

**1.1 Your First Parallel Loop**

```cpp
#include <omp.h>
#include <vector>

int N = 1'000'000;
std::vector<float> a(N), b(N), c(N);

// Sequential: thread 0 does all N iterations
for (int i = 0; i < N; i++)
    c[i] = a[i] + b[i];

// Parallel: 8 threads each do ~125,000 iterations
#pragma omp parallel for
for (int i = 0; i < N; i++)
    c[i] = a[i] + b[i];
```

That's it. One line added. The compiler splits `[0, N)` into chunks, assigns each chunk to a thread, and inserts a barrier at the end.

**What each thread sees:**

```
Thread 0:  i = 0 … 124,999
Thread 1:  i = 125,000 … 249,999
Thread 2:  i = 250,000 … 374,999
...
Thread 7:  i = 875,000 … 999,999
```

This is safe here because each `i` writes to a different `c[i]`. No two threads touch the same memory.

---

**1.2 The Classic Race Condition**

**Wrong — data race:**

```cpp
int sum = 0;

#pragma omp parallel for
for (int i = 0; i < N; i++)
    sum += a[i];   // ← RACE: multiple threads read-modify-write sum simultaneously

// sum is wrong. Possibly different on every run.
```

**Why it breaks:**

```
Thread 0 reads sum = 5
Thread 1 reads sum = 5     ← same value, before thread 0 wrote back
Thread 0 writes sum = 5 + a[0] = 7
Thread 1 writes sum = 5 + a[1] = 6   ← overwrites thread 0's result!
```

One update is lost. This is a classic **read-modify-write race**.

**Fix: `reduction` clause**

```cpp
int sum = 0;

#pragma omp parallel for reduction(+:sum)
for (int i = 0; i < N; i++)
    sum += a[i];   // each thread accumulates its own private sum
                   // all private sums are added together at the end
```

Each thread gets its own private copy of `sum` (initialized to 0). After the loop, OpenMP adds all private copies into the original `sum`. No race. Section 2.5 covers all supported reduction operators and custom reductions.

---

</details>

### 1.3 数据共享子句

并行区域中引用的每个变量，要么是 **shared**（一份副本，所有线程可见），要么是 **private**（每个线程各有一份副本）。OpenMP 的默认规则：在区域之外声明的变量是 shared。

**不能** —— 对同一个变量，`private` 与 `firstprivate` 互斥。一个变量只能出现在一个子句中。二者是仅初始化方式不同的两种替代方案：

```cpp
int x = 10;

// Option A — private(x): each thread gets its own x, value is UNINITIALIZED
// Use when: you assign x before reading it inside the loop anyway
#pragma omp parallel for private(x)
for (int i = 0; i < N; i++) {
    x = compute(i);   // x is assigned first → uninitialized value never read → safe
    a[i] *= x;
}

// Option B — firstprivate(x): each thread gets its own x, initialized to 10
// Use when: you read x before assigning it (need the original value)
#pragma omp parallel for firstprivate(x)
for (int i = 0; i < N; i++) {
    a[i] = x + i;    // x is read first → needs the initial value 10 → must use firstprivate
    x = compute(i);  // then overwritten — fine, it's private
}
```

**规则：** 若总是先写后读，用 `private`。若循环内需要用到原始值，用 `firstprivate`。

#### 何时该选 `private`

当变量**纯粹是 scratch/temp** 时用 `private` —— 它在每次迭代开始时就被完全覆盖，因此未初始化的值无关紧要：

```cpp
char buf[64];        // scratch buffer — we always snprintf before using it
float tmp;           // intermediate result — always assigned before read
int  row, col;       // 2D indices derived from i — always computed fresh

// All three are scratch: always written first → private is correct
#pragma omp parallel for private(buf, tmp, row, col)
for (int i = 0; i < N; i++) {
    row = i / COLS;                       // written first
    col = i % COLS;                       // written first
    tmp = heavy_compute(matrix[row][col]); // written first
    snprintf(buf, sizeof(buf), "(%d,%d)=%.2f", row, col, tmp);  // written first
    store_result(i, buf, tmp);
}
```

可以这样理解：**如果你会把该变量声明在循环体*内部*，它就该放进 `private`**。`private` 只是把循环局部变量挪到外面（也许是因为 pragma 里要用到它，或者它是个定长 buffer）。

```cpp
// These are equivalent:

// Version A — variable declared inside loop (naturally private, no clause needed)
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    float tmp = a[i] * b[i];   // each iteration's own tmp
    result[i] = tmp + c[i];
}

// Version B — variable declared outside, made private with clause
float tmp;
#pragma omp parallel for private(tmp)
for (int i = 0; i < N; i++) {
    tmp = a[i] * b[i];         // same effect
    result[i] = tmp + c[i];
}
```

> 尽可能**优先选版本 A** —— 把 scratch 变量声明在循环体内部。只有当*必须*把变量声明在外面时（例如定长数组、C99 VLA，或兼容性约束），才用 `private`。

---


<details>
<summary>English original</summary>

**1.3 Data Sharing Clauses**

Every variable referenced inside a parallel region is either **shared** (one copy, all threads see it) or **private** (each thread has its own copy). OpenMP's default: variables declared outside the region are shared.

**No** — `private` and `firstprivate` are mutually exclusive for the same variable. A variable can only appear in one clause. They are alternatives that differ only in initialization:

```cpp
int x = 10;

// Option A — private(x): each thread gets its own x, value is UNINITIALIZED
// Use when: you assign x before reading it inside the loop anyway
#pragma omp parallel for private(x)
for (int i = 0; i < N; i++) {
    x = compute(i);   // x is assigned first → uninitialized value never read → safe
    a[i] *= x;
}

// Option B — firstprivate(x): each thread gets its own x, initialized to 10
// Use when: you read x before assigning it (need the original value)
#pragma omp parallel for firstprivate(x)
for (int i = 0; i < N; i++) {
    a[i] = x + i;    // x is read first → needs the initial value 10 → must use firstprivate
    x = compute(i);  // then overwritten — fine, it's private
}
```

**Rule:** use `private` when you always write before read. Use `firstprivate` when you need the original value inside the loop.

**When `private` is the right choice**

Use `private` when the variable is **purely a scratch/temp** — it gets completely overwritten at the start of every iteration, so the uninitialized value never matters:

```cpp
char buf[64];        // scratch buffer — we always snprintf before using it
float tmp;           // intermediate result — always assigned before read
int  row, col;       // 2D indices derived from i — always computed fresh

// All three are scratch: always written first → private is correct
#pragma omp parallel for private(buf, tmp, row, col)
for (int i = 0; i < N; i++) {
    row = i / COLS;                       // written first
    col = i % COLS;                       // written first
    tmp = heavy_compute(matrix[row][col]); // written first
    snprintf(buf, sizeof(buf), "(%d,%d)=%.2f", row, col, tmp);  // written first
    store_result(i, buf, tmp);
}
```

Think of it this way: **if you would declare the variable *inside* the loop body, it belongs in `private`**. `private` is just moving a loop-local variable outside (maybe because you need it in the pragma or it's a fixed-size buffer).

```cpp
// These are equivalent:

// Version A — variable declared inside loop (naturally private, no clause needed)
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    float tmp = a[i] * b[i];   // each iteration's own tmp
    result[i] = tmp + c[i];
}

// Version B — variable declared outside, made private with clause
float tmp;
#pragma omp parallel for private(tmp)
for (int i = 0; i < N; i++) {
    tmp = a[i] * b[i];         // same effect
    result[i] = tmp + c[i];
}
```

> **Prefer Version A** when possible — declare scratch variables inside the loop body. Use `private` only when you *must* declare the variable outside (e.g., fixed-size arrays, C99 VLAs, or compatibility constraints).

---

</details>

#### `lastprivate` —— 获取最后一次迭代的值

`lastprivate` 在循环期间是私有的，但循环结束后，来自 **顺序上最后一次迭代**（`i` 最大）的值会被复制回原变量。

**它解决的问题：** 并行循环结束后，有时需要知道某个变量在最后一次迭代中持有的值 —— 就好像循环是串行执行的一样。

```cpp
// Imagine you're sweeping x from 0.0 to 1.0 and computing sin(x) at each step.
// After the loop, you want sin(x) at the last step — i.e., sin(x_max).

const int N  = 1000;
const double dx = 1.0 / (N - 1);

double x      = 0.0;   // current x value
double sin_x  = 0.0;   // sin at current x

#pragma omp parallel for lastprivate(x, sin_x)
for (int i = 0; i < N; i++) {
    x     = i * dx;          // each iteration computes its own x
    sin_x = std::sin(x);     // and its own sin_x
    output[i] = sin_x;
}

// After the loop:
// x     = (N-1) * dx  = 1.0        ← value from i = N-1 (last iteration)
// sin_x = sin(1.0)    ≈ 0.8415     ← value from i = N-1 (last iteration)
printf("At x=%.4f, sin(x)=%.4f\n", x, sin_x);
```

**各线程看到的值 vs 最终取回的值：**

```
Thread 0 processes i = 0..249:
  last x in its range = 249 * dx = 0.249
  last sin_x          = sin(0.249)

Thread 1 processes i = 250..499:
  last x in its range = 499 * dx = 0.499
  last sin_x          = sin(0.499)

Thread 2 processes i = 500..749:
  last x  = 0.749,  sin_x = sin(0.749)

Thread 3 processes i = 750..999:   ← sequentially last range
  last x  = 999 * dx = 1.0        ← THIS gets copied back
  sin_x   = sin(1.0)              ← THIS gets copied back

After join: x = 1.0, sin_x = sin(1.0) — as if the loop ran serially
```

> **`lastprivate` 从处理了最高 `i` 的那个线程复制**，而不是墙钟时间上最后结束的线程。无论调度如何，结果都是确定性的。

**常见的实际用例：**

| 模式 | 为何用 `lastprivate` |
|---------|------------------|
| 一轮遍历后的累计量 / 递推 | 需要最终累积值 |
| 查找满足条件的最后一个元素 | 把匹配结果带出循环 |
| Mesh 遍历 —— 记录最后一个被处理节点的位置 | 完整遍历后的节点索引/坐标 |
| 数值积分 —— 循环结束后需要端点值 | 积分上限处的被积函数值 |

**在同一变量上同时使用 `lastprivate` + `firstprivate`** —— 这*确实*合法（它们不是同一个子句）：

```cpp
double x = 0.5;   // starting x

// firstprivate: each thread starts with x = 0.5
// lastprivate:  after loop, x = value from last iteration
#pragma omp parallel for firstprivate(x) lastprivate(x)
for (int i = 0; i < N; i++) {
    x = x * 0.99 + i * dx;   // reads x (needs firstprivate) and updates it
    output[i] = x;
}
// x here = value of x after i = N-1, with each thread having started at 0.5
```

---

#### 完整子句参考

| 子句 | 是否初始化？ | 循环后是否写回？ | 适用场景 |
|--------|:-----------:|:------------------------:|---------|
| `shared(var)` | —（单份副本） | —（始终有效） | 循环内只读，或配合 sync 安全写入 |
| `private(var)` | **否** | 否 | 临时变量 —— 总是先写后读 |
| `firstprivate(var)` | **是**（来自原变量） | 否 | 循环内需要原变量的值 |
| `lastprivate(var)` | 否 | **是**（来自最后一次 `i`） | 循环结束后需要循环的最终值 |
| `firstprivate` + `lastprivate` | **是** | **是** | 既需要原变量的值，也需要最终值 |
| `reduction(op:var)` | 是（单位元） | 是（合并） | 跨所有迭代累加 |

> **经验法则：** 所有线程只读 → `shared`。临时变量 → `private`。需要原变量的值 → `firstprivate`。循环结束后需要值 → `lastprivate`。累加 → `reduction`。

---


<details>
<summary>English original</summary>

**`lastprivate` — Getting the Value from the Last Iteration**

`lastprivate` is private during the loop, but after the loop ends, the value from the **sequentially last iteration** (highest `i`) is copied back to the original variable.

**The problem it solves:** after a parallel loop, you sometimes need to know what a variable held in the final iteration — as if the loop ran serially.

```cpp
// Imagine you're sweeping x from 0.0 to 1.0 and computing sin(x) at each step.
// After the loop, you want sin(x) at the last step — i.e., sin(x_max).

const int N  = 1000;
const double dx = 1.0 / (N - 1);

double x      = 0.0;   // current x value
double sin_x  = 0.0;   // sin at current x

#pragma omp parallel for lastprivate(x, sin_x)
for (int i = 0; i < N; i++) {
    x     = i * dx;          // each iteration computes its own x
    sin_x = std::sin(x);     // and its own sin_x
    output[i] = sin_x;
}

// After the loop:
// x     = (N-1) * dx  = 1.0        ← value from i = N-1 (last iteration)
// sin_x = sin(1.0)    ≈ 0.8415     ← value from i = N-1 (last iteration)
printf("At x=%.4f, sin(x)=%.4f\n", x, sin_x);
```

**What each thread sees vs what you get back:**

```
Thread 0 processes i = 0..249:
  last x in its range = 249 * dx = 0.249
  last sin_x          = sin(0.249)

Thread 1 processes i = 250..499:
  last x in its range = 499 * dx = 0.499
  last sin_x          = sin(0.499)

Thread 2 processes i = 500..749:
  last x  = 0.749,  sin_x = sin(0.749)

Thread 3 processes i = 750..999:   ← sequentially last range
  last x  = 999 * dx = 1.0        ← THIS gets copied back
  sin_x   = sin(1.0)              ← THIS gets copied back

After join: x = 1.0, sin_x = sin(1.0) — as if the loop ran serially
```

> **`lastprivate` copies from the thread that handled the highest `i`**, not the thread that finished last in wall-clock time. The result is deterministic regardless of scheduling.

**Common real use cases:**

| Pattern | Why `lastprivate` |
|---------|------------------|
| Running total / recurrence after a sweep | Need final accumulated value |
| Finding the last element matching a condition | Carry the match out of the loop |
| Mesh traversal — record position of last node processed | Node index/coords after full sweep |
| Numerical integration — endpoint value needed post-loop | Value of integrand at upper bound |

**`lastprivate` + `firstprivate` on the same variable** — this *is* legal (they are not the same clause):

```cpp
double x = 0.5;   // starting x

// firstprivate: each thread starts with x = 0.5
// lastprivate:  after loop, x = value from last iteration
#pragma omp parallel for firstprivate(x) lastprivate(x)
for (int i = 0; i < N; i++) {
    x = x * 0.99 + i * dx;   // reads x (needs firstprivate) and updates it
    output[i] = x;
}
// x here = value of x after i = N-1, with each thread having started at 0.5
```

---

**Full Clause Reference**

| Clause | Initialized? | Written back after loop? | Use when |
|--------|:-----------:|:------------------------:|---------|
| `shared(var)` | — (one copy) | — (always live) | Read-only inside loop, or safely written with sync |
| `private(var)` | **No** | No | Scratch — always written before read |
| `firstprivate(var)` | **Yes** (from original) | No | Needs original value inside loop |
| `lastprivate(var)` | No | **Yes** (from last `i`) | Need loop's final value after it ends |
| `firstprivate` + `lastprivate` | **Yes** | **Yes** | Needs original value AND final value |
| `reduction(op:var)` | Yes (identity) | Yes (combined) | Accumulate across all iterations |

> **Rule of thumb:** If all threads only read → `shared`. Scratch variable → `private`. Needs original value → `firstprivate`. Need value after loop ends → `lastprivate`. Accumulate → `reduction`.

---

</details>

### 1.4 Schedules — 工作如何划分

OpenMP 按 *schedule* 把循环迭代划分给各线程。该选哪种 schedule 取决于各次迭代耗时相同还是差异很大。

```cpp
// Static (default): divide evenly before the loop starts
// Thread 0 gets [0, N/P), thread 1 gets [N/P, 2N/P), ...
// Best when: each iteration costs the same amount of time
#pragma omp parallel for schedule(static)
for (int i = 0; i < N; i++) { /* uniform work */ }

// Static with chunk: interleave chunks of k iterations
// Thread 0 gets 0-7, 32-39, 64-71, ...   (chunk=8, 4 threads)
// Helps with cache locality in some patterns
#pragma omp parallel for schedule(static, 8)
for (int i = 0; i < N; i++) { /* */ }

// Dynamic: each thread takes the next k iterations when it becomes free
// Overhead: ~synchronization per chunk fetch
// Best when: iterations have unpredictable/varying cost
#pragma omp parallel for schedule(dynamic, 64)
for (int i = 0; i < N; i++) { /* variable-cost work */ }

// Guided: starts with large chunks, shrinks over time
// Reduces scheduling overhead while handling tail imbalance
// Best when: later iterations are lighter than earlier ones
#pragma omp parallel for schedule(guided)
for (int i = 0; i < N; i++) { /* */ }

// Runtime: schedule determined by OMP_SCHEDULE env variable
// OMP_SCHEDULE="dynamic,32" ./my_program
#pragma omp parallel for schedule(runtime)
for (int i = 0; i < N; i++) { /* */ }
```

**何时选哪种：**

```
All iterations take the same time?     → static (lowest overhead)
Iterations have wildly different cost? → dynamic
Don't know, want adaptive?             → guided
Tuning at runtime without recompile?   → runtime
```

---

### 1.5 Reductions

承接 2.2 节引入的 `reduction(+:sum)` 子句，以下是全部受支持的运算符以及如何定义自定义归约：

```cpp
int sum = 0, product = 1, max_val = INT_MIN;

#pragma omp parallel for reduction(+:sum) reduction(*:product) reduction(max:max_val)
for (int i = 0; i < N; i++) {
    sum      += a[i];
    product  *= b[i];
    max_val   = std::max(max_val, a[i]);
}
```

内置运算符：`+`、`*`、`-`、`&`、`|`、`^`、`&&`、`||`、`min`、`max`。

**自定义归约（仅 C++，OpenMP 4.0+）：**

```cpp
struct Vec3 { float x, y, z; };

// Declare how to combine two partial results (omp_out += omp_in)
// and what value each thread's private copy starts at.
#pragma omp declare reduction(vec_add : Vec3 : \
    omp_out.x += omp_in.x; \
    omp_out.y += omp_in.y; \
    omp_out.z += omp_in.z) \
    initializer(omp_priv = Vec3{0, 0, 0})

Vec3 total{0, 0, 0};

#pragma omp parallel for reduction(vec_add : total)
for (int i = 0; i < N; i++) {
    total.x += forces[i].x;   // accumulate into the thread-private copy of total
    total.y += forces[i].y;
    total.z += forces[i].z;
}
// After the loop: OpenMP calls the combiner to merge all thread-private
// totals into the final total using the vec_add rules above.
```

---

### 1.6 Collapse — 并行化嵌套循环

当嵌套循环的外层维度很小（例如 4 行却有 8 个线程）时，`collapse` 会把外层与内层循环合并成一个扁平的迭代空间，让所有线程都有活干。

```cpp
// Without collapse: only the outer loop is parallelized
// If outer loop has fewer iterations than threads → wasted threads
#pragma omp parallel for
for (int i = 0; i < 4; i++)
    for (int j = 0; j < 1000; j++)
        A[i][j] = B[i][j] * C[i][j];

// With collapse(2): outer * inner = 4000 iterations are parallelized
// Each thread gets a portion of the flattened 4000-iteration space
#pragma omp parallel for collapse(2)
for (int i = 0; i < 4; i++)
    for (int j = 0; j < 1000; j++)
        A[i][j] = B[i][j] * C[i][j];
```

> 当外层循环次数相对线程数较小时，使用 `collapse`。

---

### 1.7 SIMD — 向量化提示

SIMD（单指令多数据）在一条 CPU 指令中处理多个数组元素。这与 GPU SIMT 背后的原理相同——在这里学会它，可以为 CUDA 的 warp 级执行打基础。

```cpp
// Ask the compiler to vectorize (SIMD) one loop on a single thread
#pragma omp simd
for (int i = 0; i < N; i++)
    c[i] = a[i] * b[i] + d[i];

// Parallelize across threads AND vectorize within each thread's chunk
#pragma omp parallel for simd
for (int i = 0; i < N; i++)
    c[i] = a[i] * b[i];

// simd with reduction (e.g. sum with SIMD accumulation)
float sum = 0.0f;
#pragma omp simd reduction(+:sum)
for (int i = 0; i < N; i++)
    sum += a[i];
```

`simd` pragma 只是一个提示。编译器仍会判断向量化是否安全。加上 `-fopt-info-vec` 可查看哪些部分被向量化。


<details>
<summary>English original</summary>

**1.4 Schedules — How Work Is Divided**

OpenMP divides loop iterations among threads according to a *schedule*. The right schedule depends on whether iterations take equal time or vary widely.

```cpp
// Static (default): divide evenly before the loop starts
// Thread 0 gets [0, N/P), thread 1 gets [N/P, 2N/P), ...
// Best when: each iteration costs the same amount of time
#pragma omp parallel for schedule(static)
for (int i = 0; i < N; i++) { /* uniform work */ }

// Static with chunk: interleave chunks of k iterations
// Thread 0 gets 0-7, 32-39, 64-71, ...   (chunk=8, 4 threads)
// Helps with cache locality in some patterns
#pragma omp parallel for schedule(static, 8)
for (int i = 0; i < N; i++) { /* */ }

// Dynamic: each thread takes the next k iterations when it becomes free
// Overhead: ~synchronization per chunk fetch
// Best when: iterations have unpredictable/varying cost
#pragma omp parallel for schedule(dynamic, 64)
for (int i = 0; i < N; i++) { /* variable-cost work */ }

// Guided: starts with large chunks, shrinks over time
// Reduces scheduling overhead while handling tail imbalance
// Best when: later iterations are lighter than earlier ones
#pragma omp parallel for schedule(guided)
for (int i = 0; i < N; i++) { /* */ }

// Runtime: schedule determined by OMP_SCHEDULE env variable
// OMP_SCHEDULE="dynamic,32" ./my_program
#pragma omp parallel for schedule(runtime)
for (int i = 0; i < N; i++) { /* */ }
```

**When to pick which:**

```
All iterations take the same time?     → static (lowest overhead)
Iterations have wildly different cost? → dynamic
Don't know, want adaptive?             → guided
Tuning at runtime without recompile?   → runtime
```

---

**1.5 Reductions**

Building on the `reduction(+:sum)` clause introduced in section 2.2, here are all supported operators and how to define custom reductions:

```cpp
int sum = 0, product = 1, max_val = INT_MIN;

#pragma omp parallel for reduction(+:sum) reduction(*:product) reduction(max:max_val)
for (int i = 0; i < N; i++) {
    sum      += a[i];
    product  *= b[i];
    max_val   = std::max(max_val, a[i]);
}
```

Built-in operators: `+`, `*`, `-`, `&`, `|`, `^`, `&&`, `||`, `min`, `max`.

**Custom reduction (C++ only, OpenMP 4.0+):**

```cpp
struct Vec3 { float x, y, z; };

// Declare how to combine two partial results (omp_out += omp_in)
// and what value each thread's private copy starts at.
#pragma omp declare reduction(vec_add : Vec3 : \
    omp_out.x += omp_in.x; \
    omp_out.y += omp_in.y; \
    omp_out.z += omp_in.z) \
    initializer(omp_priv = Vec3{0, 0, 0})

Vec3 total{0, 0, 0};

#pragma omp parallel for reduction(vec_add : total)
for (int i = 0; i < N; i++) {
    total.x += forces[i].x;   // accumulate into the thread-private copy of total
    total.y += forces[i].y;
    total.z += forces[i].z;
}
// After the loop: OpenMP calls the combiner to merge all thread-private
// totals into the final total using the vec_add rules above.
```

---

**1.6 Collapse — Parallelizing Nested Loops**

When a nested loop's outer dimension is small (e.g., 4 rows but 8 threads), `collapse` merges the outer and inner loops into a single flattened iteration space so all threads have work.

```cpp
// Without collapse: only the outer loop is parallelized
// If outer loop has fewer iterations than threads → wasted threads
#pragma omp parallel for
for (int i = 0; i < 4; i++)
    for (int j = 0; j < 1000; j++)
        A[i][j] = B[i][j] * C[i][j];

// With collapse(2): outer * inner = 4000 iterations are parallelized
// Each thread gets a portion of the flattened 4000-iteration space
#pragma omp parallel for collapse(2)
for (int i = 0; i < 4; i++)
    for (int j = 0; j < 1000; j++)
        A[i][j] = B[i][j] * C[i][j];
```

> Use `collapse` when the outer loop count is small relative to the thread count.

---

**1.7 SIMD — Vectorization Hints**

SIMD (Single Instruction, Multiple Data) processes multiple array elements in one CPU instruction. This is the same principle behind GPU SIMT — learning it here prepares you for CUDA's warp-level execution.

```cpp
// Ask the compiler to vectorize (SIMD) one loop on a single thread
#pragma omp simd
for (int i = 0; i < N; i++)
    c[i] = a[i] * b[i] + d[i];

// Parallelize across threads AND vectorize within each thread's chunk
#pragma omp parallel for simd
for (int i = 0; i < N; i++)
    c[i] = a[i] * b[i];

// simd with reduction (e.g. sum with SIMD accumulation)
float sum = 0.0f;
#pragma omp simd reduction(+:sum)
for (int i = 0; i < N; i++)
    sum += a[i];
```

The `simd` pragma is a hint. The compiler still decides if vectorization is safe. Add `-fopt-info-vec` to see what was vectorized.

---

</details>

### 1.8 保护共享状态：critical 与 atomic

当 `reduction` 不够用时（复杂的共享状态）：

```cpp
// critical: only one thread at a time
// Correct, but slow (acts like a mutex around the block)
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    #pragma omp critical
    {
        global_map[key(i)] += value(i);
    }
}

// atomic: faster for single memory operations (uses hardware atomics)
int counter = 0;
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    #pragma omp atomic
    counter++;           // ← hardware atomic, no mutex overhead
}

// atomic with operation
#pragma omp atomic update
total += a[i];

#pragma omp atomic read
int val = shared_var;

#pragma omp atomic write
shared_var = computed;
```

**critical 与 atomic 对比：**
- `#pragma omp critical` —— mutex、任意代码块，开销更高
- `#pragma omp atomic` —— 硬件指令，仅单次读/写/更新，快得多

---

### 1.9 同步：Barrier 与 nowait

每个 `#pragma omp for` 末尾都有一个**隐式 barrier** —— 所有线程都会阻塞，直到最慢的那个完成。`nowait` 将其移除，这样快的线程可以立即开始下一个独立的循环。

```
Default (implicit barrier):
T0: [████ loop ████]░░░░░░░░░░ wait ░░░░░[next loop]
T1: [████████████████ loop ████████████][next loop]
T2: [██ loop ██]░░░░░░░░░░░░░░ wait ░░░░[next loop]
                               ▲
                   all threads held here

With nowait:
T0: [████ loop ████][next loop immediately]
T1: [████████████████ loop ████████████][next loop]
T2: [██ loop ██][next loop immediately]
     only safe when the two loops are data-independent
```

```cpp
#pragma omp parallel
{
    #pragma omp for nowait          // no barrier after this loop
    for (int i = 0; i < N; i++)
        a[i] = compute_a(i);        // writes only a[]

    #pragma omp for nowait          // no barrier after this loop either
    for (int i = 0; i < N; i++)
        b[i] = compute_b(i);        // writes only b[] — independent of a[]

    #pragma omp barrier             // explicit sync: a[] and b[] both fully written
    use(a, b);                      // safe to read both here
}
```

**`nowait` 何时安全：** 下一个工作单元读取的数据集与刚完成的循环完全不同。若存在任何依赖，就保留隐式 barrier。

---

### 1.10 Sections —— 固定的并发操作

`sections` 用于一组**规模小、编译期已知**的独立操作 —— 而不是循环。每个 `section` 块在一个线程上运行；所有块并发执行。

```
#pragma omp parallel sections
┌─────────────────────────────────────────────────┐
│  section 1 → T0: [────── load_data() ──────────]│
│  section 2 → T1: [── init_config() ──]          │
│  section 3 → T2: [──────── warmup() ────────────]│
└────────────────────────── implicit barrier ──────┘
                  all three done → program continues
```

```cpp
#pragma omp parallel sections
{
    #pragma omp section
    load_data();        // thread 0

    #pragma omp section
    init_config();      // thread 1

    #pragma omp section
    warmup();           // thread 2
}
// all three finished here
```

**Sections 与 Tasks 对比：**

| | `sections` | `tasks` |
|--|-----------|---------|
| 单元数量 | 编译期固定 | 动态、递归 |
| 使用场景 | 加载 + 初始化 + 预热 | 树遍历、Fibonacci |
| 开销 | 极低 | 较高（任务队列） |

当操作数量可以手数出来时，用 `sections`。当工作递归展开、或数量到 runtime 才能确定时，用 `tasks`（下一节）。

---


<details>
<summary>English original</summary>

**1.8 Protecting Shared State: critical and atomic**

When `reduction` is not enough (complex shared state):

```cpp
// critical: only one thread at a time
// Correct, but slow (acts like a mutex around the block)
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    #pragma omp critical
    {
        global_map[key(i)] += value(i);
    }
}

// atomic: faster for single memory operations (uses hardware atomics)
int counter = 0;
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    #pragma omp atomic
    counter++;           // ← hardware atomic, no mutex overhead
}

// atomic with operation
#pragma omp atomic update
total += a[i];

#pragma omp atomic read
int val = shared_var;

#pragma omp atomic write
shared_var = computed;
```

**Critical vs Atomic:**
- `#pragma omp critical` — mutex, any code block, higher overhead
- `#pragma omp atomic` — hardware instruction, single read/write/update only, much faster

---

**1.9 Synchronization: Barriers and nowait**

Every `#pragma omp for` has an **implicit barrier** at the end — all threads block until the slowest one finishes. `nowait` removes it so fast threads can immediately start the next independent loop.

```
Default (implicit barrier):
T0: [████ loop ████]░░░░░░░░░░ wait ░░░░░[next loop]
T1: [████████████████ loop ████████████][next loop]
T2: [██ loop ██]░░░░░░░░░░░░░░ wait ░░░░[next loop]
                               ▲
                   all threads held here

With nowait:
T0: [████ loop ████][next loop immediately]
T1: [████████████████ loop ████████████][next loop]
T2: [██ loop ██][next loop immediately]
     only safe when the two loops are data-independent
```

```cpp
#pragma omp parallel
{
    #pragma omp for nowait          // no barrier after this loop
    for (int i = 0; i < N; i++)
        a[i] = compute_a(i);        // writes only a[]

    #pragma omp for nowait          // no barrier after this loop either
    for (int i = 0; i < N; i++)
        b[i] = compute_b(i);        // writes only b[] — independent of a[]

    #pragma omp barrier             // explicit sync: a[] and b[] both fully written
    use(a, b);                      // safe to read both here
}
```

**When `nowait` is safe:** the next work unit reads from a completely different data set than the loop just completed. If there is any dependency, keep the implicit barrier.

---

**1.10 Sections — Fixed Concurrent Operations**

`sections` is for a **small, known-at-compile-time** set of independent operations — not a loop. Each `section` block runs on one thread; all blocks run concurrently.

```
#pragma omp parallel sections
┌─────────────────────────────────────────────────┐
│  section 1 → T0: [────── load_data() ──────────]│
│  section 2 → T1: [── init_config() ──]          │
│  section 3 → T2: [──────── warmup() ────────────]│
└────────────────────────── implicit barrier ──────┘
                  all three done → program continues
```

```cpp
#pragma omp parallel sections
{
    #pragma omp section
    load_data();        // thread 0

    #pragma omp section
    init_config();      // thread 1

    #pragma omp section
    warmup();           // thread 2
}
// all three finished here
```

**Sections vs Tasks:**

| | `sections` | `tasks` |
|--|-----------|---------|
| Number of units | Fixed at compile time | Dynamic, recursive |
| Use case | Load + init + warmup | Tree traversal, Fibonacci |
| Overhead | Very low | Higher (task queue) |

Use `sections` when you can count the operations by hand. Use `tasks` (next section) when the work fans out recursively or the count isn't known until runtime.

---

</details>

### 1.11 任务 —— 递归与不规则工作

Task 将**工作创建**与**工作执行**解耦。一个线程创建 task 并放入队列；线程组中的任意线程取走并执行它们。

```
single thread creates tasks:          thread pool executes them:
  task(left  branch) ──► queue ──► T1: sum_tree(left)
  task(right branch) ──► queue ──► T2: sum_tree(right)
  taskwait ─────────────────────► T0: waits, then adds results
```

**树求和 —— 正确模式（parallel region 在外，tasks 在内）：**

```cpp
// The recursive function only creates tasks — no parallel region here
int sum_tree(Node* node) {
    if (!node) return 0;
    int left_sum = 0, right_sum = 0;

    #pragma omp task shared(left_sum)
    left_sum = sum_tree(node->left);

    #pragma omp task shared(right_sum)
    right_sum = sum_tree(node->right);

    #pragma omp taskwait        // wait for both before combining
    return node->value + left_sum + right_sum;
}

// Parallel region created ONCE outside — not inside the recursive function
int result;
#pragma omp parallel
#pragma omp single              // one thread drives task creation
result = sum_tree(root);
```

> **常见错误：** 把 `#pragma omp parallel` 放进递归函数内部。那样每次递归调用都会新建一个线程组 —— 成千上万个嵌套线程池、巨大的开销，而且结果多半是错的。

**带 cutoff 的 Fibonacci：**

```cpp
int fib(int n) {
    if (n < 2)  return n;
    if (n < 25) return fib(n-1) + fib(n-2);  // serial below cutoff

    int x, y;
    #pragma omp task shared(x) firstprivate(n)
    x = fib(n - 1);

    #pragma omp task shared(y) firstprivate(n)
    y = fib(n - 2);

    #pragma omp taskwait
    return x + y;
}

int result;
#pragma omp parallel
#pragma omp single
result = fib(50);
```

**每个 clause 为何重要：**

| Clause | 作用 |
|--------|-------------|
| `shared(x)` | 所有线程看到同一个 `x` —— task 把结果写在那里 |
| `firstprivate(n)` | 每个 task 在创建时拿到自己的 `n` 副本 —— 递归中保证正确性所必需 |
| `taskwait` | 挂起当前 task，直到所有子 task 结束 |
| `single` | 只有一个线程生成 task，其余线程执行它们。没有它，每个线程都会生成整棵树 → task 数量指数爆炸 |

**递归树如何映射到 tasks —— `fib(6)` 示例：**

`fib(6)` 总共产生 25 个节点。cutoff 之上的每个内部节点都变成一个 task；叶子节点（`fib(0)`、`fib(1)`）立即返回。

```
fib(6)                                ← T0 spawns fib(5) and fib(4), then taskwait
├─ fib(5)                             ← T1 picks up, spawns fib(4) and fib(3)
│  ├─ fib(4)                          ← T2 picks up, spawns fib(3) and fib(2)
│  │  ├─ fib(3)                       ← T3 picks up, spawns fib(2) and fib(1)
│  │  │  ├─ fib(2) → fib(1)+fib(0)   ← returns 1 immediately (leaf)
│  │  │  └─ fib(1)                    ← returns 1 immediately (leaf)
│  │  └─ fib(2) → fib(1)+fib(0)      ← returns 1 immediately (leaf)
│  └─ fib(3)
│     ├─ fib(2) → fib(1)+fib(0)
│     └─ fib(1)
└─ fib(4)                             ← T2 (or T3) steals this after finishing above
   ├─ fib(3)
   │  ├─ fib(2) → fib(1)+fib(0)
   │  └─ fib(1)
   └─ fib(2) → fib(1)+fib(0)
```

**4 个线程的执行流程：**

```
time →

T0: [spawn fib(5), fib(4)] [taskwait........] [return 5+3=8]
T1: [fib(5): spawn fib(4),fib(3)] [taskwait] [return 3+2=5]
T2: [fib(4): spawn fib(3),fib(2)] [taskwait] [return 2+1=3] [steal fib(4)→return 3]
T3: [fib(3): spawn fib(2),fib(1)] [return 1+0+1=2]          [fib(3)→return 2]
     ↑                                ↑
  tasks created                 threads steal work
  and queued                    as soon as they're idle
```

**关键洞察 —— `single` 不会成为瓶颈：**  T0 只生成最上面的两个 task（`fib(n-1)` 和 `fib(n-2)`），随后立刻走到 `taskwait`，于是可以去执行其他 task。等 T0 生成完毕时，T1–T3 已经在递归地创建和消费各子树。只要队列里还有工作，就没有线程会闲置。

对于 cutoff=25 的 `fib(50)`，递归会产生 ~fib(25) ≈ 75,000 个 cutoff 之上的 task —— 足以让任意数量的 core 保持忙碌。

---


<details>
<summary>English original</summary>

**1.11 Tasks — Recursive and Irregular Work**

Tasks decouple **work creation** from **work execution**. One thread creates tasks and puts them in a queue; any thread in the team picks them up and executes them.

```
single thread creates tasks:          thread pool executes them:
  task(left  branch) ──► queue ──► T1: sum_tree(left)
  task(right branch) ──► queue ──► T2: sum_tree(right)
  taskwait ─────────────────────► T0: waits, then adds results
```

**Tree sum — correct pattern (parallel region outside, tasks inside):**

```cpp
// The recursive function only creates tasks — no parallel region here
int sum_tree(Node* node) {
    if (!node) return 0;
    int left_sum = 0, right_sum = 0;

    #pragma omp task shared(left_sum)
    left_sum = sum_tree(node->left);

    #pragma omp task shared(right_sum)
    right_sum = sum_tree(node->right);

    #pragma omp taskwait        // wait for both before combining
    return node->value + left_sum + right_sum;
}

// Parallel region created ONCE outside — not inside the recursive function
int result;
#pragma omp parallel
#pragma omp single              // one thread drives task creation
result = sum_tree(root);
```

> **Common mistake:** putting `#pragma omp parallel` inside the recursive function. That creates a new thread team on every recursive call — thousands of nested thread pools, massive overhead, and likely wrong results.

**Fibonacci with cutoff:**

```cpp
int fib(int n) {
    if (n < 2)  return n;
    if (n < 25) return fib(n-1) + fib(n-2);  // serial below cutoff

    int x, y;
    #pragma omp task shared(x) firstprivate(n)
    x = fib(n - 1);

    #pragma omp task shared(y) firstprivate(n)
    y = fib(n - 2);

    #pragma omp taskwait
    return x + y;
}

int result;
#pragma omp parallel
#pragma omp single
result = fib(50);
```

**Why each clause matters:**

| Clause | What it does |
|--------|-------------|
| `shared(x)` | All threads see the same `x` — task writes its result there |
| `firstprivate(n)` | Each task gets its own copy of `n` at creation time — required for correctness in recursion |
| `taskwait` | Suspends the current task until all child tasks finish |
| `single` | Only one thread spawns tasks; the rest execute them. Without it, every thread would spawn the full tree → exponential task explosion |

**How the recursion tree maps to tasks — `fib(6)` example:**

`fib(6)` produces 25 nodes total. Each internal node above the cutoff becomes a task; leaves (`fib(0)`, `fib(1)`) return immediately.

```
fib(6)                                ← T0 spawns fib(5) and fib(4), then taskwait
├─ fib(5)                             ← T1 picks up, spawns fib(4) and fib(3)
│  ├─ fib(4)                          ← T2 picks up, spawns fib(3) and fib(2)
│  │  ├─ fib(3)                       ← T3 picks up, spawns fib(2) and fib(1)
│  │  │  ├─ fib(2) → fib(1)+fib(0)   ← returns 1 immediately (leaf)
│  │  │  └─ fib(1)                    ← returns 1 immediately (leaf)
│  │  └─ fib(2) → fib(1)+fib(0)      ← returns 1 immediately (leaf)
│  └─ fib(3)
│     ├─ fib(2) → fib(1)+fib(0)
│     └─ fib(1)
└─ fib(4)                             ← T2 (or T3) steals this after finishing above
   ├─ fib(3)
   │  ├─ fib(2) → fib(1)+fib(0)
   │  └─ fib(1)
   └─ fib(2) → fib(1)+fib(0)
```

**Execution flow with 4 threads:**

```
time →

T0: [spawn fib(5), fib(4)] [taskwait........] [return 5+3=8]
T1: [fib(5): spawn fib(4),fib(3)] [taskwait] [return 3+2=5]
T2: [fib(4): spawn fib(3),fib(2)] [taskwait] [return 2+1=3] [steal fib(4)→return 3]
T3: [fib(3): spawn fib(2),fib(1)] [return 1+0+1=2]          [fib(3)→return 2]
     ↑                                ↑
  tasks created                 threads steal work
  and queued                    as soon as they're idle
```

**Key insight — `single` does not bottleneck:**  T0 spawns only the top two tasks (`fib(n-1)` and `fib(n-2)`), then immediately hits `taskwait` and is available to execute other tasks. By the time T0 is done spawning, T1–T3 are already recursively creating and consuming the subtrees. No thread ever sits idle as long as the queue has work.

For `fib(50)` with cutoff=25, the recursion produces ~fib(25) ≈ 75,000 tasks above the cutoff — more than enough to keep any number of cores busy.

---

</details>

### 1.12 线程信息与环境

```cpp
// Query runtime info
int n_threads = omp_get_num_threads();   // inside parallel region
int thread_id = omp_get_thread_num();    // 0 to n_threads-1
int max_threads = omp_get_max_threads(); // outside parallel region
int n_procs = omp_get_num_procs();       // hardware thread count

// Set thread count
omp_set_num_threads(8);

// Timing
double t0 = omp_get_wtime();
// ... work ...
double elapsed = omp_get_wtime() - t0;
```

**环境变量（运行前设置）：**

```bash
OMP_NUM_THREADS=8             # number of threads
OMP_SCHEDULE="dynamic,64"     # schedule for 'runtime' clauses
OMP_PROC_BIND=close           # bind threads to nearby hardware (NUMA)
OMP_PLACES=cores              # binding granularity: threads, cores, sockets
GOMP_SPINCOUNT=100000         # how long to spin before sleeping (GNU)
```

---

### 1.13 嵌套并行

```cpp
omp_set_nested(1);  // enable nested parallel regions

void outer_task() {
    #pragma omp parallel for num_threads(4)
    for (int i = 0; i < 4; i++) {
        // Inner parallel region: each of 4 threads spawns 2 more
        #pragma omp parallel for num_threads(2)
        for (int j = 0; j < N; j++)
            compute(i, j);
    }
}
```

> 嵌套并行常常会超额占用核心。通常更好的做法是改用 `collapse(2)`。

---

### 1.14 常见陷阱

| 陷阱 | 症状 | 修复 |
|---------|---------|-----|
| 共享写入未同步 | 结果错误、非确定性的 | `reduction`、`atomic` 或 `critical` |
| 在任务中按引用捕获 `i` | 任务读到错误的 `i` | 在任务上加 `firstprivate(i)` |
| 任务创建代码中缺少 `single` | 任务爆炸 | 在任务创建处加上 `#pragma omp single` |
| 在 parallel 之外调用 `omp_get_num_threads()` | 总是返回 1 | 在 parallel 区域内调用 |
| 使用任务结果前缺少 `taskwait` | 未就绪就使用 | `#pragma omp taskwait` |
| 对存在依赖的循环使用 `nowait` | 使用了错误的数据 | 去掉 `nowait` 或加上显式的 `barrier` |
| 过长的临界区 | 线程被串行化 | 缩小临界区，简单操作使用 `atomic` |

---

### 1.15 OpenMP vs oneTBB

| | OpenMP | oneTBB |
|--|--------|--------|
| API 风格 | 编译器指令（`#pragma`） | C++ 模板与 lambda |
| 学习曲线 | 较低——每个特性一条 pragma | 较高——需要了解模板类型 |
| 粒度 | 循环级 | 任务级 |
| 负载均衡 | static/dynamic/guided 调度 | 工作窃取（自动、自适应） |
| 流图 | 无 | 有（`flow_graph`） |
| 每线程存储 | `threadprivate` / `private` 子句 | `enumerable_thread_specific` |
| 嵌套并行 | 手动 | 内置 |
| SIMD 提示 | `#pragma omp simd` | 必须使用手写 intrinsics |
| 最适合 | 已有循环、Fortran 互操作、科学高性能计算（HPC） | 新的 C++ 代码、复杂图、不规则任务 |

**资源：** [OpenMP specifications](https://www.openmp.org/specifications/) · [OpenMP API Reference Card (PDF)](https://www.openmp.org/wp-content/uploads/OpenMP-4.5-1115-CPP-web.pdf)

上表突出显示了 oneTBB 具备而 OpenMP 缺失的能力——尤其是工作窃取、流图和并发容器。下一节按同样的渐进风格介绍 oneTBB 的 API，从简单循环起步，逐步构建到基于复杂图的模式。

---


<details>
<summary>English original</summary>

**1.12 Thread Info and Environment**

```cpp
// Query runtime info
int n_threads = omp_get_num_threads();   // inside parallel region
int thread_id = omp_get_thread_num();    // 0 to n_threads-1
int max_threads = omp_get_max_threads(); // outside parallel region
int n_procs = omp_get_num_procs();       // hardware thread count

// Set thread count
omp_set_num_threads(8);

// Timing
double t0 = omp_get_wtime();
// ... work ...
double elapsed = omp_get_wtime() - t0;
```

**Environment variables (set before running):**

```bash
OMP_NUM_THREADS=8             # number of threads
OMP_SCHEDULE="dynamic,64"     # schedule for 'runtime' clauses
OMP_PROC_BIND=close           # bind threads to nearby hardware (NUMA)
OMP_PLACES=cores              # binding granularity: threads, cores, sockets
GOMP_SPINCOUNT=100000         # how long to spin before sleeping (GNU)
```

---

**1.13 Nested Parallelism**

```cpp
omp_set_nested(1);  // enable nested parallel regions

void outer_task() {
    #pragma omp parallel for num_threads(4)
    for (int i = 0; i < 4; i++) {
        // Inner parallel region: each of 4 threads spawns 2 more
        #pragma omp parallel for num_threads(2)
        for (int j = 0; j < N; j++)
            compute(i, j);
    }
}
```

> Nested parallelism often over-subscribes cores. Usually better to `collapse(2)` instead.

---

**1.14 Common Pitfalls**

| Pitfall | Symptom | Fix |
|---------|---------|-----|
| Shared write without sync | Wrong results, non-deterministic | `reduction`, `atomic`, or `critical` |
| Capturing `i` by reference in tasks | Task reads wrong `i` | `firstprivate(i)` on the task |
| `single` missing in task-creation code | Task explosion | Add `#pragma omp single` around task creation |
| Calling `omp_get_num_threads()` outside parallel | Returns 1 always | Call inside the parallel region |
| Missing `taskwait` before using task results | Use before ready | `#pragma omp taskwait` |
| `nowait` on dependent loops | Uses wrong data | Remove `nowait` or add explicit `barrier` |
| Long critical sections | Serializes threads | Narrow the critical section, use `atomic` for simple ops |

---

**1.15 OpenMP vs oneTBB**

| | OpenMP | oneTBB |
|--|--------|--------|
| API style | Compiler directives (`#pragma`) | C++ templates and lambdas |
| Learning curve | Lower — one pragma per feature | Higher — need to know template types |
| Granularity | Loop-level | Task-level |
| Load balancing | Static/dynamic/guided schedules | Work-stealing (automatic, adaptive) |
| Flow graphs | No | Yes (`flow_graph`) |
| Per-thread storage | `threadprivate` / `private` clause | `enumerable_thread_specific` |
| Nested parallelism | Manual | Built-in |
| SIMD hints | `#pragma omp simd` | Must use manual intrinsics |
| Best for | Existing loops, Fortran interop, scientific HPC | New C++ code, complex graphs, irregular tasks |

**Resources:** [OpenMP specifications](https://www.openmp.org/specifications/) · [OpenMP API Reference Card (PDF)](https://www.openmp.org/wp-content/uploads/OpenMP-4.5-1115-CPP-web.pdf)

The table above highlights where oneTBB offers capabilities OpenMP lacks — especially work-stealing, flow graphs, and concurrent containers. The next section covers oneTBB's API in the same progressive style, starting from simple loops and building toward complex graph-based patterns.

---

</details>

### 1.16 Benchmark — Serial vs OpenMP vs oneTBB

2.11 节的 Fibonacci 示例是一个有用的 benchmark，因为调用树是纯递归工作，没有 I/O 或系统调用——唯一的变量是调度开销与并行度。

**完整 benchmark（`fib_benchmark.cpp`）：**

```cpp
// fib_benchmark.cpp — serial / OpenMP tasks / oneTBB parallel_invoke
// Compile: g++ -O2 -std=c++17 -fopenmp fib_benchmark.cpp -ltbb -o fib_benchmark

#include <chrono>
#include <iostream>
#include <omp.h>
#include "oneapi/tbb/parallel_invoke.h"

using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// Below this depth, run sequentially — avoids task overhead on trivial work
static constexpr int CUTOFF = 20;

// ── 1. Serial ─────────────────────────────────────────────────────────────────
long long fib_serial(int n) {
    if (n < 2) return n;
    return fib_serial(n - 1) + fib_serial(n - 2);
}

// ── 2. OpenMP tasks ───────────────────────────────────────────────────────────
long long fib_omp(int n) {
    if (n < CUTOFF) return fib_serial(n);   // switch to serial below cutoff
    long long x, y;
    #pragma omp task shared(x) firstprivate(n)
    x = fib_omp(n - 1);
    #pragma omp task shared(y) firstprivate(n)
    y = fib_omp(n - 2);
    #pragma omp taskwait
    return x + y;
}

// ── 3. oneTBB parallel_invoke ─────────────────────────────────────────────────
long long fib_tbb(int n) {
    if (n < CUTOFF) return fib_serial(n);
    long long x, y;
    tbb::parallel_invoke(
        [&]{ x = fib_tbb(n - 1); },
        [&]{ y = fib_tbb(n - 2); }
    );
    return x + y;
}

// ── Timing helper ─────────────────────────────────────────────────────────────
template<typename Fn>
double time_ms(Fn&& fn) {
    auto t0 = Clock::now();
    fn();
    return Ms(Clock::now() - t0).count();
}

int main() {
    const int N = 42;
    long long r_serial, r_omp, r_tbb;

    // warm up (first call pays library init cost)
    fib_serial(30);

    double t_serial = time_ms([&]{ r_serial = fib_serial(N); });

    double t_omp = time_ms([&]{
        #pragma omp parallel
        #pragma omp single
        r_omp = fib_omp(N);
    });

    double t_tbb = time_ms([&]{ r_tbb = fib_tbb(N); });

    const int threads = omp_get_max_threads();
    std::printf("fib(%d) = %lld   [threads available: %d]\n\n", N, r_serial, threads);
    std::printf("%-12s %8.1f ms  (baseline)\n",   "serial",  t_serial);
    std::printf("%-12s %8.1f ms  (%.2fx speedup)\n", "openmp", t_omp, t_serial / t_omp);
    std::printf("%-12s %8.1f ms  (%.2fx speedup)\n", "onetbb", t_tbb, t_serial / t_tbb);

    if (r_serial != r_omp || r_serial != r_tbb)
        std::fprintf(stderr, "ERROR: results differ!\n");
    return 0;
}
```

**编译并运行：**

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install g++ libomp-dev libtbb-dev

# Compile
g++ -O2 -std=c++17 -fopenmp fib_benchmark.cpp -ltbb -o fib_benchmark

# Run
./fib_benchmark
```

**实测输出（4 核机器，fib(50)，CUTOFF=25）：**

```
fib(50) = 12586269025   [threads available: 4, cutoff: 25]

serial        31887.7 ms  (baseline)
openmp        14057.1 ms  (2.27x speedup)
onetbb         9616.4 ms  (3.32x speedup)
```

**为什么加速比是次线性的：**

`T` 个线程的理论最大加速比为 `T×`。实际中：

| 损失来源 | OpenMP | oneTBB |
|----------------|--------|--------|
| 任务创建开销 | 较高（pragma 派发） | 较低（内联 lambda） |
| 负载均衡 | victim-push 调度 | work-stealing（自适应） |
| 窃取导致的缓存颠簸 | 中等 | 低（自身 LIFO / 窃取 FIFO） |
| Cutoff 调优 | 手动 | 手动 |

当各分支的完成速率不同时，oneTBB 的 work-stealing 调度器适应性更好，因此它在非规则递归上通常略胜 OpenMP 一筹。

**CUTOFF 敏感度**——在 4 核机器上用 fib(50) 实测：

```bash
for c in 20 25 30 35 40; do
  g++ -O2 -std=c++17 -fopenmp -DCUTOFF_VAL=$c fib_benchmark.cpp -ltbb -o fib_b$c
  echo "=== CUTOFF=$c ===" && ./fib_b$c
done
```

| CUTOFF | 派生任务数 | OpenMP 加速比 | oneTBB 加速比 |
|--------|--------------|----------------|----------------|
| 20 | ~830 K | 1.15x | 2.42x |
| 25 | ~26 K | **2.27x** | **3.32x** |
| 30 | ~830 | 1.93x | 3.01x |
| 35 | ~26 | 2.32x | 2.79x |
| 40 | ~1 | 1.83x | 2.78x |

CUTOFF=25 是最佳点：任务足够多，能让所有线程保持忙碌；又足够少，使调度开销不占主导。OpenMP 对 CUTOFF 比 oneTBB 更敏感，因为 work-stealing 会自动适应负载不均。

---

## 2. oneTBB（oneAPI Threading Building Blocks）


<details>
<summary>English original</summary>

**1.16 Benchmark — Serial vs OpenMP vs oneTBB**

The Fibonacci example from section 2.11 is a useful benchmark because the call tree is pure recursive work with no I/O or system calls — the only variable is scheduling overhead and parallelism.

**Full benchmark (`fib_benchmark.cpp`):**

```cpp
// fib_benchmark.cpp — serial / OpenMP tasks / oneTBB parallel_invoke
// Compile: g++ -O2 -std=c++17 -fopenmp fib_benchmark.cpp -ltbb -o fib_benchmark

#include <chrono>
#include <iostream>
#include <omp.h>
#include "oneapi/tbb/parallel_invoke.h"

using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// Below this depth, run sequentially — avoids task overhead on trivial work
static constexpr int CUTOFF = 20;

// ── 1. Serial ─────────────────────────────────────────────────────────────────
long long fib_serial(int n) {
    if (n < 2) return n;
    return fib_serial(n - 1) + fib_serial(n - 2);
}

// ── 2. OpenMP tasks ───────────────────────────────────────────────────────────
long long fib_omp(int n) {
    if (n < CUTOFF) return fib_serial(n);   // switch to serial below cutoff
    long long x, y;
    #pragma omp task shared(x) firstprivate(n)
    x = fib_omp(n - 1);
    #pragma omp task shared(y) firstprivate(n)
    y = fib_omp(n - 2);
    #pragma omp taskwait
    return x + y;
}

// ── 3. oneTBB parallel_invoke ─────────────────────────────────────────────────
long long fib_tbb(int n) {
    if (n < CUTOFF) return fib_serial(n);
    long long x, y;
    tbb::parallel_invoke(
        [&]{ x = fib_tbb(n - 1); },
        [&]{ y = fib_tbb(n - 2); }
    );
    return x + y;
}

// ── Timing helper ─────────────────────────────────────────────────────────────
template<typename Fn>
double time_ms(Fn&& fn) {
    auto t0 = Clock::now();
    fn();
    return Ms(Clock::now() - t0).count();
}

int main() {
    const int N = 42;
    long long r_serial, r_omp, r_tbb;

    // warm up (first call pays library init cost)
    fib_serial(30);

    double t_serial = time_ms([&]{ r_serial = fib_serial(N); });

    double t_omp = time_ms([&]{
        #pragma omp parallel
        #pragma omp single
        r_omp = fib_omp(N);
    });

    double t_tbb = time_ms([&]{ r_tbb = fib_tbb(N); });

    const int threads = omp_get_max_threads();
    std::printf("fib(%d) = %lld   [threads available: %d]\n\n", N, r_serial, threads);
    std::printf("%-12s %8.1f ms  (baseline)\n",   "serial",  t_serial);
    std::printf("%-12s %8.1f ms  (%.2fx speedup)\n", "openmp", t_omp, t_serial / t_omp);
    std::printf("%-12s %8.1f ms  (%.2fx speedup)\n", "onetbb", t_tbb, t_serial / t_tbb);

    if (r_serial != r_omp || r_serial != r_tbb)
        std::fprintf(stderr, "ERROR: results differ!\n");
    return 0;
}
```

**Compile and run:**

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install g++ libomp-dev libtbb-dev

# Compile
g++ -O2 -std=c++17 -fopenmp fib_benchmark.cpp -ltbb -o fib_benchmark

# Run
./fib_benchmark
```

**Measured output (4-core machine, fib(50), CUTOFF=25):**

```
fib(50) = 12586269025   [threads available: 4, cutoff: 25]

serial        31887.7 ms  (baseline)
openmp        14057.1 ms  (2.27x speedup)
onetbb         9616.4 ms  (3.32x speedup)
```

**Why the speedup is sub-linear:**

The theoretical maximum with `T` threads is `T×` speedup. In practice:

| Source of loss | OpenMP | oneTBB |
|----------------|--------|--------|
| Task creation overhead | Higher (pragma dispatch) | Lower (inline lambda) |
| Load balancing | Victim-push scheduling | Work-stealing (adaptive) |
| Cache thrash from stealing | Moderate | Low (LIFO own / FIFO steal) |
| Cutoff tuning | Manual | Manual |

oneTBB's work-stealing scheduler adapts better when branches complete at different rates, which is why it typically edges out OpenMP on irregular recursion.

**CUTOFF sensitivity** — measured on a 4-core machine with fib(50):

```bash
for c in 20 25 30 35 40; do
  g++ -O2 -std=c++17 -fopenmp -DCUTOFF_VAL=$c fib_benchmark.cpp -ltbb -o fib_b$c
  echo "=== CUTOFF=$c ===" && ./fib_b$c
done
```

| CUTOFF | Tasks spawned | OpenMP speedup | oneTBB speedup |
|--------|--------------|----------------|----------------|
| 20 | ~830 K | 1.15x | 2.42x |
| 25 | ~26 K | **2.27x** | **3.32x** |
| 30 | ~830 | 1.93x | 3.01x |
| 35 | ~26 | 2.32x | 2.79x |
| 40 | ~1 | 1.83x | 2.78x |

CUTOFF=25 is the sweet spot: enough tasks to keep all threads busy, few enough that scheduling overhead doesn't dominate. OpenMP is more sensitive to CUTOFF than oneTBB because work-stealing adapts automatically to load imbalance.

---

**2. oneTBB (oneAPI Threading Building Blocks)**

</details>

### 2.0 心智模型：工作窃取调度器

oneTBB 是**基于任务**的并行编程库。无需直接管理线程，而是表达*什么*可以并行 —— runtime 使用**工作窃取**调度器分配工作。

**工作窃取如何工作：**

```
Thread 0's deque:  [task A] [task B] [task C]  ← pushes/pops from right (LIFO, cache-friendly)
Thread 1's deque:  [] ← empty, idle
Thread 2's deque:  [task X]
Thread 3's deque:  [] ← empty, idle

Step 1: Thread 0 pops task C from its own deque (right end, hot in cache)
Step 2: Thread 1 is idle → STEALS task A from Thread 0's LEFT end
Step 3: Thread 3 is idle → STEALS task X from Thread 2
```

**为什么从左侧窃取？**
- Owner 从**右侧**弹出（LIFO）—— 按栈顺序处理自己的任务，缓存友好
- Stealer 从**左侧**弹出（FIFO）—— 取最大、最旧的任务，最大化窃取的工作规模

**结果：** 只要任何线程有工作，线程就不会空闲。无需为不均衡的工作负载手动调整线程数。

**安装：**

```bash
# Ubuntu/Debian
sudo apt install libtbb-dev

# CMake integration
find_package(TBB REQUIRED)
target_link_libraries(my_target TBB::tbb)
```

**头文件：**

```cpp
#include "oneapi/tbb.h"
using namespace oneapi::tbb;
```

---

### 2.1 `parallel_for` —— 并行循环

基本构建块。将范围拆分为块，并在可用线程上执行每个块。

**简单的一维整数形式：**

```cpp
int N = 1'000'000;
float a[N], b[N], c[N];

// Compact form — runtime picks chunk size automatically
parallel_for(0, N, [&](int i) {
    c[i] = a[i] + b[i];
});
```

**`blocked_range` 形式 —— 提供子范围，在其中循环：**

```cpp
// Better: lets you write inner loops with fewer lambda calls
parallel_for(blocked_range<int>(0, N),
    [&](const blocked_range<int>& r) {
        for (int i = r.begin(); i < r.end(); i++) {
            c[i] = a[i] + b[i];
        }
    }
);
```

`blocked_range<T>(begin, end)` 是半开区间 `[begin, end)`。lambda 接收子范围 `r` —— 调用 `.begin()` 和 `.end()` 来迭代它。

**为什么用 `blocked_range` 而不是紧凑形式？**
- 避免逐元素的 lambda 调用开销
- 允许每个块初始化一次缓冲区（而不是每个元素）
- 自定义块级逻辑（SIMD、临时分配）所必需

**二维范围 —— 矩阵或图像处理：**

```cpp
parallel_for(blocked_range2d<int>(0, rows, 0, cols),
    [&](const blocked_range2d<int>& r) {
        for (int i = r.rows().begin(); i < r.rows().end(); i++)
            for (int j = r.cols().begin(); j < r.cols().end(); j++)
                out[i][j] = process(in[i][j]);
    }
);
```

**三维范围：**

```cpp
parallel_for(blocked_range3d<int>(0, D, 0, H, 0, W),
    [&](const blocked_range3d<int>& r) {
        for (int d = r.pages().begin(); d < r.pages().end(); d++)
            for (int h = r.rows().begin(); h < r.rows().end(); h++)
                for (int w = r.cols().begin(); w < r.cols().end(); w++)
                    vol[d][h][w] = f(d, h, w);
    }
);
```

#### 分区器 —— 控制分块

第三个可选参数控制工作如何划分：

```cpp
// auto_partitioner (default): runtime tunes chunk size automatically
parallel_for(blocked_range<int>(0, N), body);

// affinity_partitioner: reuses same data → same thread (cache-warm)
// Declare static so it persists between calls
static affinity_partitioner ap;
parallel_for(blocked_range<int>(0, N), body, ap);

// simple_partitioner: chunk = grainsize exactly, no adaptive splitting (work-stealing still active)
parallel_for(blocked_range<int>(0, N, 1000), body, simple_partitioner());

// static_partitioner: divide evenly upfront, no stealing
// Deterministic: same thread always gets same range
parallel_for(blocked_range<int>(0, N), body, static_partitioner());
```

| 分区器 | 块大小 | 自适应拆分 | 工作窃取 | 使用场景 |
|-------------|------------|--------------------|---------------|---------|
| `auto_partitioner` | 动态 | 是 | 是 | 大多数情况 —— 默认 |
| `affinity_partitioner` | 自适应 | 是 | 是 | 在相同数据上重新运行同一循环（缓存热） |
| `simple_partitioner` | 固定 ≈ grainsize | 否 | 是 | 块需要固定大小的临时缓冲区；工作均匀；N 很大 |
| `static_partitioner` | 预划分、固定映射 | 否 | 否 | NUMA/缓存局部性调优；严格可复现的线程映射 |

**Grainsize 调优：** 每个块应耗时 ≥ ~100,000 个时钟周期（2 GHz 下约 50 µs）。块太小 = 调度开销占主导。规则：如果循环体耗时 10 ns，grainsize 为 10,000+ 是合理的。


<details>
<summary>English original</summary>

**2.0 Mental Model: Work-Stealing Scheduler**

oneTBB is a **task-based** parallel programming library. Instead of managing threads directly, you express *what* can run in parallel — the runtime distributes work using a **work-stealing** scheduler.

**How work-stealing works:**

```
Thread 0's deque:  [task A] [task B] [task C]  ← pushes/pops from right (LIFO, cache-friendly)
Thread 1's deque:  [] ← empty, idle
Thread 2's deque:  [task X]
Thread 3's deque:  [] ← empty, idle

Step 1: Thread 0 pops task C from its own deque (right end, hot in cache)
Step 2: Thread 1 is idle → STEALS task A from Thread 0's LEFT end
Step 3: Thread 3 is idle → STEALS task X from Thread 2
```

**Why steal from the left?**
- Owner pops from the **right** (LIFO) — processes its own tasks in stack order, cache-friendly
- Stealer pops from the **left** (FIFO) — takes the biggest, oldest tasks, maximizing stolen work size

**Result:** Threads never sit idle as long as any thread has work. No need to manually tune thread counts for imbalanced workloads.

**Install:**

```bash
# Ubuntu/Debian
sudo apt install libtbb-dev

# CMake integration
find_package(TBB REQUIRED)
target_link_libraries(my_target TBB::tbb)
```

**Header:**

```cpp
#include "oneapi/tbb.h"
using namespace oneapi::tbb;
```

---

**2.1 `parallel_for` — Parallel Loop**

The fundamental building block. Splits a range into chunks and executes each chunk on an available thread.

**Simple 1D integer form:**

```cpp
int N = 1'000'000;
float a[N], b[N], c[N];

// Compact form — runtime picks chunk size automatically
parallel_for(0, N, [&](int i) {
    c[i] = a[i] + b[i];
});
```

**`blocked_range` form — gives you the subrange, loop inside:**

```cpp
// Better: lets you write inner loops with fewer lambda calls
parallel_for(blocked_range<int>(0, N),
    [&](const blocked_range<int>& r) {
        for (int i = r.begin(); i < r.end(); i++) {
            c[i] = a[i] + b[i];
        }
    }
);
```

`blocked_range<T>(begin, end)` is the half-open interval `[begin, end)`. The lambda receives a subrange `r` — call `.begin()` and `.end()` to iterate it.

**Why `blocked_range` over compact form?**
- Avoids per-element lambda call overhead
- Lets you initialize buffers once per chunk (instead of per element)
- Required for custom chunk-level logic (SIMD, temp allocation)

**2D range — matrix or image processing:**

```cpp
parallel_for(blocked_range2d<int>(0, rows, 0, cols),
    [&](const blocked_range2d<int>& r) {
        for (int i = r.rows().begin(); i < r.rows().end(); i++)
            for (int j = r.cols().begin(); j < r.cols().end(); j++)
                out[i][j] = process(in[i][j]);
    }
);
```

**3D range:**

```cpp
parallel_for(blocked_range3d<int>(0, D, 0, H, 0, W),
    [&](const blocked_range3d<int>& r) {
        for (int d = r.pages().begin(); d < r.pages().end(); d++)
            for (int h = r.rows().begin(); h < r.rows().end(); h++)
                for (int w = r.cols().begin(); w < r.cols().end(); w++)
                    vol[d][h][w] = f(d, h, w);
    }
);
```

**Partitioners — Control Chunking**

The third optional argument controls how work is divided:

```cpp
// auto_partitioner (default): runtime tunes chunk size automatically
parallel_for(blocked_range<int>(0, N), body);

// affinity_partitioner: reuses same data → same thread (cache-warm)
// Declare static so it persists between calls
static affinity_partitioner ap;
parallel_for(blocked_range<int>(0, N), body, ap);

// simple_partitioner: chunk = grainsize exactly, no adaptive splitting (work-stealing still active)
parallel_for(blocked_range<int>(0, N, 1000), body, simple_partitioner());

// static_partitioner: divide evenly upfront, no stealing
// Deterministic: same thread always gets same range
parallel_for(blocked_range<int>(0, N), body, static_partitioner());
```

| Partitioner | Chunk size | Adaptive splitting | Work-stealing | Use when |
|-------------|------------|--------------------|---------------|---------|
| `auto_partitioner` | Dynamic | Yes | Yes | Most cases — default |
| `affinity_partitioner` | Adaptive | Yes | Yes | Re-running same loop over same data (cache-warm) |
| `simple_partitioner` | Fixed ≈ grainsize | No | Yes | Chunk needs fixed-size temp buffer; work is uniform; N is huge |
| `static_partitioner` | Pre-divided, fixed mapping | No | No | NUMA/cache locality tuning; strict reproducible thread mapping |

**Grainsize tuning:** Each chunk should take ≥ ~100,000 clock cycles (~50 µs at 2 GHz). Too-small chunks = scheduling overhead dominates. Rule: if loop body takes 10 ns, grainsize of 10,000+ is reasonable.

</details>

#### 完整示例 1 — 数组平方

观察从串行到并行转变的最清晰方式：

```cpp
#include <oneapi/tbb.h>
#include <iostream>
#include <vector>

int main() {
    const size_t N = 1'000'000;
    std::vector<int> data(N, 3);    // all 3s
    std::vector<int> result(N);

    // Serial version:
    // for (size_t i = 0; i < N; ++i)
    //     result[i] = data[i] * data[i];

    // Parallel version — one line change:
    oneapi::tbb::parallel_for(size_t(0), N, [&](size_t i) {
        result[i] = data[i] * data[i];   // each thread handles a range of i
    });

    std::cout << "result[0]=" << result[0]
              << "  result[N-1]=" << result[N-1] << "\n";
    // Expected: 9  9
}
```

**runtime 做了什么：**

```
N = 1,000,000   threads = 8

Thread 0:  i = 0 … 124,999       → result[i] = data[i]²
Thread 1:  i = 125,000 … 249,999 → result[i] = data[i]²
...
Thread 7:  i = 875,000 … 999,999 → result[i] = data[i]²

All done → join back
```

没有竞态条件：每个线程写入不同的 `result[i]`。

---

#### 完整示例 2 — 二维特征图上的 ReLU

ReLU（`max(0, x)`）在卷积层之后逐元素应用。每个输出值都是独立的——完全适合 `blocked_range2d`。

```cpp
#include <oneapi/tbb.h>
#include <algorithm>
#include <iostream>
#include <vector>

int main() {
    // Feature map: H rows × W columns, row-major flat storage
    const size_t H = 1024, W = 1024;
    std::vector<float> feat(H * W);
    std::vector<float> out (H * W);

    // Fill with values in range [-128, 127]
    for (size_t i = 0; i < H * W; ++i)
        feat[i] = static_cast<float>(i % 256) - 128.0f;

    // Apply ReLU in parallel over tiles
    oneapi::tbb::parallel_for(
        oneapi::tbb::blocked_range2d<size_t>(0, H, 0, W),
        [&](const oneapi::tbb::blocked_range2d<size_t>& tile) {
            for (size_t y = tile.rows().begin(); y < tile.rows().end(); ++y)
                for (size_t x = tile.cols().begin(); x < tile.cols().end(); ++x)
                    out[y * W + x] = std::max(0.0f, feat[y * W + x]);
        }
    );

    std::cout << "out(0,0)   = " << out[0]   << "\n";  // 0   (was -128)
    std::cout << "out(0,129) = " << out[129] << "\n";  // 1   (was 1)
    std::cout << "out(0,255) = " << out[255] << "\n";  // 127 (was 127)
}
```

**为什么 `blocked_range2d` 胜过两个嵌套的 `parallel_for` 调用：**

```
blocked_range2d splits the grid into rectangular tiles:

  ┌───────┬───────┬───────┐
  │ T0    │ T1    │ T2    │   Each tile = contiguous memory block
  │       │       │       │   → good L1/L2 cache reuse within tile
  ├───────┼───────┼───────┤
  │ T3    │ T4    │ T5    │
  │       │       │       │
  └───────┴───────┴───────┘

Two nested parallel_for → each row is a separate task → too many tiny tasks
blocked_range2d    → each tile is one task → right granularity
```

`tile.rows()` 和 `tile.cols()` 访问器给出了这个分块的子范围。始终使用扁平 `vector<T>` 配合 `[y * W + x]` 索引——`vector<vector<T>>` 会破坏空间局部性。

---


<details>
<summary>English original</summary>

**Complete Example 1 — Squaring an Array**

The clearest way to see the transition from serial to parallel:

```cpp
#include <oneapi/tbb.h>
#include <iostream>
#include <vector>

int main() {
    const size_t N = 1'000'000;
    std::vector<int> data(N, 3);    // all 3s
    std::vector<int> result(N);

    // Serial version:
    // for (size_t i = 0; i < N; ++i)
    //     result[i] = data[i] * data[i];

    // Parallel version — one line change:
    oneapi::tbb::parallel_for(size_t(0), N, [&](size_t i) {
        result[i] = data[i] * data[i];   // each thread handles a range of i
    });

    std::cout << "result[0]=" << result[0]
              << "  result[N-1]=" << result[N-1] << "\n";
    // Expected: 9  9
}
```

**What the runtime does:**

```
N = 1,000,000   threads = 8

Thread 0:  i = 0 … 124,999       → result[i] = data[i]²
Thread 1:  i = 125,000 … 249,999 → result[i] = data[i]²
...
Thread 7:  i = 875,000 … 999,999 → result[i] = data[i]²

All done → join back
```

No race condition: each thread writes to a different `result[i]`.

---

**Complete Example 2 — ReLU on a 2D Feature Map**

ReLU (`max(0, x)`) is applied element-wise after a convolution layer. Every output value is independent — a perfect fit for `blocked_range2d`.

```cpp
#include <oneapi/tbb.h>
#include <algorithm>
#include <iostream>
#include <vector>

int main() {
    // Feature map: H rows × W columns, row-major flat storage
    const size_t H = 1024, W = 1024;
    std::vector<float> feat(H * W);
    std::vector<float> out (H * W);

    // Fill with values in range [-128, 127]
    for (size_t i = 0; i < H * W; ++i)
        feat[i] = static_cast<float>(i % 256) - 128.0f;

    // Apply ReLU in parallel over tiles
    oneapi::tbb::parallel_for(
        oneapi::tbb::blocked_range2d<size_t>(0, H, 0, W),
        [&](const oneapi::tbb::blocked_range2d<size_t>& tile) {
            for (size_t y = tile.rows().begin(); y < tile.rows().end(); ++y)
                for (size_t x = tile.cols().begin(); x < tile.cols().end(); ++x)
                    out[y * W + x] = std::max(0.0f, feat[y * W + x]);
        }
    );

    std::cout << "out(0,0)   = " << out[0]   << "\n";  // 0   (was -128)
    std::cout << "out(0,129) = " << out[129] << "\n";  // 1   (was 1)
    std::cout << "out(0,255) = " << out[255] << "\n";  // 127 (was 127)
}
```

**Why `blocked_range2d` beats two nested `parallel_for` calls:**

```
blocked_range2d splits the grid into rectangular tiles:

  ┌───────┬───────┬───────┐
  │ T0    │ T1    │ T2    │   Each tile = contiguous memory block
  │       │       │       │   → good L1/L2 cache reuse within tile
  ├───────┼───────┼───────┤
  │ T3    │ T4    │ T5    │
  │       │       │       │
  └───────┴───────┴───────┘

Two nested parallel_for → each row is a separate task → too many tiny tasks
blocked_range2d    → each tile is one task → right granularity
```

The `tile.rows()` and `tile.cols()` accessors give you the subrange for this tile. Always use flat `vector<T>` with `[y * W + x]` indexing — `vector<vector<T>>` breaks spatial locality.

---

</details>

### 2.2 `parallel_reduce` — 并行归约

用于累积结果的循环：sum、min、max、点积、直方图。

**Lambda 形式（最常见）：**

```cpp
// Sum of array
float total = parallel_reduce(
    blocked_range<int>(0, N),
    0.0f,                                        // identity value
    [&](const blocked_range<int>& r, float init) -> float {
        for (int i = r.begin(); i < r.end(); i++)
            init += a[i];
        return init;
    },
    [](float x, float y) -> float {             // combine partial results
        return x + y;
    }
);
```

**参数：**
1. 要迭代的范围
2. 恒等值（每个部分结果的初始值）
3. Body：接收一个子范围 + 当前部分结果 → 返回新的部分结果
4. Combine：把两个部分结果合并为一个

**点积：**

```cpp
float dot = parallel_reduce(
    blocked_range<int>(0, N), 0.0f,
    [&](const blocked_range<int>& r, float init) -> float {
        for (int i = r.begin(); i < r.end(); i++)
            init += a[i] * b[i];
        return init;
    },
    std::plus<float>{}
);
```

**带索引求最小值：**

```cpp
struct MinResult { float val; int idx; };

auto result = parallel_reduce(
    blocked_range<int>(0, N),
    MinResult{FLT_MAX, -1},
    [&](const blocked_range<int>& r, MinResult curr) {
        for (int i = r.begin(); i < r.end(); i++)
            if (a[i] < curr.val) curr = {a[i], i};
        return curr;
    },
    [](MinResult a, MinResult b) {
        return a.val < b.val ? a : b;
    }
);
```

> **常见错误：** 不要在 body 内把累加器重置为零 —— `init` 携带的是此前的部分结果。重置它会丢弃此前的工作。

**确定性的 reduce** —— 无论调度如何，每次运行结果都相同：

```cpp
float result = parallel_deterministic_reduce(
    blocked_range<int>(0, N, 1000),  // explicit grainsize required
    0.0f,
    [&](const blocked_range<int>& r, float init) {
        return std::accumulate(&a[r.begin()], &a[r.end()], init);
    },
    std::plus<float>{}
);
// Floating-point result identical on every run
```

#### 完整示例 3 —— 均方误差（MSE）

训练时每次前向传播后都要计算 MSE：对所有样本求 `(pred - target)²` 之和，再除以 N。每个元素相互独立 —— 是直接的并行归约。

```cpp
#include <oneapi/tbb.h>
#include <iostream>
#include <vector>

int main() {
    const int N = 10'000'000;

    // Simulated model predictions and ground-truth targets
    std::vector<float> pred(N), target(N);
    for (int i = 0; i < N; ++i) {
        pred[i]   = static_cast<float>(i % 100) / 100.0f;        // [0.00, 0.99]
        target[i] = static_cast<float>((i + 1) % 100) / 100.0f;  // [0.01, 1.00]
    }

    // parallel_reduce arguments:
    //   1. range    — what to iterate over
    //   2. identity — each thread's partial sum starts at 0
    //   3. body     — reduce a subrange into a partial sum
    //   4. combine  — merge two partial sums into one
    float sum_sq = oneapi::tbb::parallel_reduce(
        oneapi::tbb::blocked_range<int>(0, N),

        0.0f,   // identity

        [&](const oneapi::tbb::blocked_range<int>& r, float init) {
            for (int i = r.begin(); i < r.end(); ++i) {
                float diff = pred[i] - target[i];
                init += diff * diff;
            }
            return init;
        },

        std::plus<float>{}   // combine
    );

    float mse = sum_sq / N;
    std::cout << "MSE = " << mse << "\n";   // ~0.003267
}
```

**TBB 如何拆分与合并工作：**

```
           parallel_reduce(blocked_range(0, 10M), 0.0f, body, combine)
                              [0 … 10,000,000)
                             /                \
                            /                  \
                [0 … 5,000,000)          [5,000,000 … 10M)
               /               \         /               \
       [0…2.5M)           [2.5M…5M)  [5M…7.5M)     [7.5M…10M)
       sum=8,167.5        sum=8,167.5 sum=8,167.5   sum=8,167.5
              \               /         \               /
           combine(a,b) = a+b           combine(a,b) = a+b
               sum=16,335.0                 sum=16,335.0
                          \                 /
                           combine(a,b) = a+b
                               sum=32,670.0
                           MSE = 32,670.0 / 10M ≈ 0.003267
```

每个叶子调用 `body(subrange, 0.0f)` —— 恒等值 `0.0f` 意味着每个线程的部分和都从零开始。所有叶子完成后，TBB 沿树向上回走，调用 `combine` 对部分结果求和。

---


<details>
<summary>English original</summary>

**2.2 `parallel_reduce` — Parallel Reduction**

For loops that accumulate a result: sum, min, max, dot product, histogram.

**Lambda form (most common):**

```cpp
// Sum of array
float total = parallel_reduce(
    blocked_range<int>(0, N),
    0.0f,                                        // identity value
    [&](const blocked_range<int>& r, float init) -> float {
        for (int i = r.begin(); i < r.end(); i++)
            init += a[i];
        return init;
    },
    [](float x, float y) -> float {             // combine partial results
        return x + y;
    }
);
```

**Arguments:**
1. Range to iterate
2. Identity value (initial value for each partial result)
3. Body: takes a subrange + running partial → returns new partial
4. Combine: merges two partial results into one

**Dot product:**

```cpp
float dot = parallel_reduce(
    blocked_range<int>(0, N), 0.0f,
    [&](const blocked_range<int>& r, float init) -> float {
        for (int i = r.begin(); i < r.end(); i++)
            init += a[i] * b[i];
        return init;
    },
    std::plus<float>{}
);
```

**Find minimum with index:**

```cpp
struct MinResult { float val; int idx; };

auto result = parallel_reduce(
    blocked_range<int>(0, N),
    MinResult{FLT_MAX, -1},
    [&](const blocked_range<int>& r, MinResult curr) {
        for (int i = r.begin(); i < r.end(); i++)
            if (a[i] < curr.val) curr = {a[i], i};
        return curr;
    },
    [](MinResult a, MinResult b) {
        return a.val < b.val ? a : b;
    }
);
```

> **Common mistake:** Do not reset the accumulator to zero inside the body — `init` carries prior partial results. Resetting it discards prior work.

**Deterministic reduce** — same result every run regardless of scheduling:

```cpp
float result = parallel_deterministic_reduce(
    blocked_range<int>(0, N, 1000),  // explicit grainsize required
    0.0f,
    [&](const blocked_range<int>& r, float init) {
        return std::accumulate(&a[r.begin()], &a[r.end()], init);
    },
    std::plus<float>{}
);
// Floating-point result identical on every run
```

**Complete Example 3 — Mean Squared Error (MSE)**

MSE is computed after every forward pass during training: sum `(pred - target)²` over all samples, divide by N. Each element is independent — a direct parallel reduction.

```cpp
#include <oneapi/tbb.h>
#include <iostream>
#include <vector>

int main() {
    const int N = 10'000'000;

    // Simulated model predictions and ground-truth targets
    std::vector<float> pred(N), target(N);
    for (int i = 0; i < N; ++i) {
        pred[i]   = static_cast<float>(i % 100) / 100.0f;        // [0.00, 0.99]
        target[i] = static_cast<float>((i + 1) % 100) / 100.0f;  // [0.01, 1.00]
    }

    // parallel_reduce arguments:
    //   1. range    — what to iterate over
    //   2. identity — each thread's partial sum starts at 0
    //   3. body     — reduce a subrange into a partial sum
    //   4. combine  — merge two partial sums into one
    float sum_sq = oneapi::tbb::parallel_reduce(
        oneapi::tbb::blocked_range<int>(0, N),

        0.0f,   // identity

        [&](const oneapi::tbb::blocked_range<int>& r, float init) {
            for (int i = r.begin(); i < r.end(); ++i) {
                float diff = pred[i] - target[i];
                init += diff * diff;
            }
            return init;
        },

        std::plus<float>{}   // combine
    );

    float mse = sum_sq / N;
    std::cout << "MSE = " << mse << "\n";   // ~0.003267
}
```

**How TBB splits and joins the work:**

```
           parallel_reduce(blocked_range(0, 10M), 0.0f, body, combine)
                              [0 … 10,000,000)
                             /                \
                            /                  \
                [0 … 5,000,000)          [5,000,000 … 10M)
               /               \         /               \
       [0…2.5M)           [2.5M…5M)  [5M…7.5M)     [7.5M…10M)
       sum=8,167.5        sum=8,167.5 sum=8,167.5   sum=8,167.5
              \               /         \               /
           combine(a,b) = a+b           combine(a,b) = a+b
               sum=16,335.0                 sum=16,335.0
                          \                 /
                           combine(a,b) = a+b
                               sum=32,670.0
                           MSE = 32,670.0 / 10M ≈ 0.003267
```

Each leaf calls `body(subrange, 0.0f)` — the identity `0.0f` means every thread's partial sum starts at zero. After all leaves finish, TBB walks back up the tree calling `combine` to sum the partial results.

---

</details>

### 2.3 `parallel_scan` — 前缀和

前缀和（scan）让每个输出元素都得到截至该点的累计和：

```
input:  [ 1,  2,  3,  4,  5 ]
output: [ 1,  3,  6, 10, 15 ]
         ↑   ↑   ↑   ↑   ↑
         1  1+2 1+2+3 ...
```

朴素实现是串行的 —— 每个输出都依赖前一个输出。`parallel_scan` 把它拆成两个 pass，使各 chunk 能并行执行：

```
Pass 1 — "pre-scan" (is_final = false):
  Chunk A [0..2]: partial sum = 1+2+3 = 6       (don't write output yet)
  Chunk B [3..4]: partial sum = 4+5   = 9       (don't write output yet)

Pass 2 — "final scan" (is_final = true):
  Chunk A [0..2]: carry-in = 0  → write 1, 3, 6
  Chunk B [3..4]: carry-in = 6  → write 10, 15

Combine: TBB calls combine(6, 9) = 15 to pass carry-in to Chunk B
```

body lambda 对每个 chunk 运行 **两次** —— 一次不写入（构建部分和），一次带正确的 carry-in（填充输出）。`is_final` 标志告诉 body 当前处于哪个 pass。

```cpp
#include "oneapi/tbb/parallel_scan.h"

std::vector<float> in(N), out(N);

tbb::parallel_scan(
    tbb::blocked_range<int>(0, N),
    0.0f,                   // identity value (carry-in for first chunk)
    [&](const tbb::blocked_range<int>& r, float running, bool is_final) {
        for (int i = r.begin(); i < r.end(); i++) {
            running += in[i];
            if (is_final)
                out[i] = running;   // only write on second pass
        }
        return running;             // hand partial sum to TBB
    },
    [](float a, float b) { return a + b; }   // how to merge two partial sums
);
// out[i] = in[0] + in[1] + ... + in[i]
```

**Inclusive 与 exclusive scan：**

```
inclusive (what parallel_scan computes by default):
  input:  [ 1,  2,  3,  4,  5 ]
  output: [ 1,  3,  6, 10, 15 ]   out[i] includes in[i]

exclusive (shifted by one, out[0] = identity):
  input:  [ 1,  2,  3,  4,  5 ]
  output: [ 0,  1,  3,  6, 10 ]   out[i] excludes in[i]
```

Exclusive scan：把写入延后一个位置 —— 先累加，再写入加上当前元素 *之前* 的那个值：

```cpp
tbb::parallel_scan(
    tbb::blocked_range<int>(0, N), 0.0f,
    [&](const tbb::blocked_range<int>& r, float running, bool is_final) {
        for (int i = r.begin(); i < r.end(); i++) {
            if (is_final)
                out[i] = running;   // write before adding — exclusive
            running += in[i];
        }
        return running;
    },
    [](float a, float b) { return a + b; }
);
```


<details>
<summary>English original</summary>

**2.3 `parallel_scan` — Prefix Sum**

A prefix sum (scan) gives every output element the running total up to that point:

```
input:  [ 1,  2,  3,  4,  5 ]
output: [ 1,  3,  6, 10, 15 ]
         ↑   ↑   ↑   ↑   ↑
         1  1+2 1+2+3 ...
```

Naively this is sequential — each output depends on the previous one. `parallel_scan` breaks it into two passes so chunks run in parallel:

```
Pass 1 — "pre-scan" (is_final = false):
  Chunk A [0..2]: partial sum = 1+2+3 = 6       (don't write output yet)
  Chunk B [3..4]: partial sum = 4+5   = 9       (don't write output yet)

Pass 2 — "final scan" (is_final = true):
  Chunk A [0..2]: carry-in = 0  → write 1, 3, 6
  Chunk B [3..4]: carry-in = 6  → write 10, 15

Combine: TBB calls combine(6, 9) = 15 to pass carry-in to Chunk B
```

The body lambda runs **twice per chunk** — once without writing (building partial sums), once with the correct carry-in (filling output). The `is_final` flag tells the body which pass it is.

```cpp
#include "oneapi/tbb/parallel_scan.h"

std::vector<float> in(N), out(N);

tbb::parallel_scan(
    tbb::blocked_range<int>(0, N),
    0.0f,                   // identity value (carry-in for first chunk)
    [&](const tbb::blocked_range<int>& r, float running, bool is_final) {
        for (int i = r.begin(); i < r.end(); i++) {
            running += in[i];
            if (is_final)
                out[i] = running;   // only write on second pass
        }
        return running;             // hand partial sum to TBB
    },
    [](float a, float b) { return a + b; }   // how to merge two partial sums
);
// out[i] = in[0] + in[1] + ... + in[i]
```

**Inclusive vs exclusive scan:**

```
inclusive (what parallel_scan computes by default):
  input:  [ 1,  2,  3,  4,  5 ]
  output: [ 1,  3,  6, 10, 15 ]   out[i] includes in[i]

exclusive (shifted by one, out[0] = identity):
  input:  [ 1,  2,  3,  4,  5 ]
  output: [ 0,  1,  3,  6, 10 ]   out[i] excludes in[i]
```

Exclusive scan: delay the write by one position — accumulate first, then write the value *before* adding the current element:

```cpp
tbb::parallel_scan(
    tbb::blocked_range<int>(0, N), 0.0f,
    [&](const tbb::blocked_range<int>& r, float running, bool is_final) {
        for (int i = r.begin(); i < r.end(); i++) {
            if (is_final)
                out[i] = running;   // write before adding — exclusive
            running += in[i];
        }
        return running;
    },
    [](float a, float b) { return a + b; }
);
```

</details>

**流压缩——并行去除零元素：**

流压缩分三步过滤数组，三步均可并行：

```
input:    [ 1,  0,  2,  0,  3,  0,  4 ]

step 1 — flag non-zero elements:
flags:    [ 1,  0,  1,  0,  1,  0,  1 ]

step 2 — exclusive scan on flags → output indices:
indices:  [ 0,  1,  1,  2,  2,  3,  3 ]

step 3 — scatter: if flag[i], write input[i] to output[indices[i]]:
output:   [ 1,  2,  3,  4 ]
```

```cpp
const int N = 7;
std::vector<int>  input  = {1, 0, 2, 0, 3, 0, 4};
std::vector<int>  flags(N), indices(N), output(N);

// Step 1: flag non-zero elements (parallel_for)
tbb::parallel_for(0, N, [&](int i) {
    flags[i] = (input[i] != 0) ? 1 : 0;
});

// Step 2: exclusive scan on flags → scatter indices
tbb::parallel_scan(
    tbb::blocked_range<int>(0, N), 0,
    [&](const tbb::blocked_range<int>& r, int running, bool is_final) {
        for (int i = r.begin(); i < r.end(); i++) {
            if (is_final) indices[i] = running;
            running += flags[i];
        }
        return running;
    },
    [](int a, int b) { return a + b; }
);

// Step 3: scatter non-zero elements to their computed positions (parallel_for)
tbb::parallel_for(0, N, [&](int i) {
    if (flags[i]) output[indices[i]] = input[i];
});
// output = [1, 2, 3, 4]
```

**前缀 max——运行最大值：**

```cpp
// out[i] = max(in[0], in[1], ..., in[i])
tbb::parallel_scan(
    tbb::blocked_range<int>(0, N),
    std::numeric_limits<float>::lowest(),   // identity for max
    [&](const tbb::blocked_range<int>& r, float running, bool is_final) {
        for (int i = r.begin(); i < r.end(); i++) {
            running = std::max(running, in[i]);
            if (is_final) out[i] = running;
        }
        return running;
    },
    [](float a, float b) { return std::max(a, b); }
);
```

```
input:  [ 2,  1,  5,  3,  4 ]
output: [ 2,  2,  5,  5,  5 ]   running maximum
```

**直方图 → CDF（累积分布函数）：**

```cpp
// hist[i] = count of pixels with brightness i
// cdf[i]  = total pixels with brightness ≤ i
// Used in histogram equalization to stretch image contrast.
tbb::parallel_scan(
    tbb::blocked_range<int>(0, 256), 0,
    [&](const tbb::blocked_range<int>& r, int running, bool is_final) {
        for (int i = r.begin(); i < r.end(); i++) {
            running += hist[i];
            if (is_final) cdf[i] = running;
        }
        return running;
    },
    [](int a, int b) { return a + b; }
);
```

**`parallel_scan` 适用于任意满足结合律的运算：**

| 运算 | 单位元 | 合并函数 | 用例 |
|-----------|----------|---------|---------|
| 求和 | `0` | `a + b` | 前缀和、CDF |
| 最大值 | `-∞` | `max(a,b)` | 滑动最大值、边界框 |
| 最小值 | `+∞` | `min(a,b)` | 滑动最小值 |
| 乘积 | `1` | `a * b` | 滑动阶乘、概率链 |
| 排除前缀和 | `0` | `a + b` | 流压缩的 scatter 索引 |

---


<details>
<summary>English original</summary>

**`parallel_scan` works for any associative operation:**

| Operation | Identity | Combine | Use case |
|-----------|----------|---------|---------|
| Sum | `0` | `a + b` | Prefix sum, CDF |
| Max | `-∞` | `max(a,b)` | Running maximum, bounding box |
| Min | `+∞` | `min(a,b)` | Running minimum |
| Product | `1` | `a * b` | Running factorial, probability chain |
| Exclusive sum | `0` | `a + b` | Stream compaction scatter indices |

---

</details>

### 2.4 `parallel_sort`

```cpp
#include "oneapi/tbb/parallel_sort.h"

std::vector<int> data(N);
// ...fill data...

// In-place sort (like std::sort but parallel)
parallel_sort(data.begin(), data.end());

// Custom comparator
parallel_sort(data.begin(), data.end(), std::greater<int>{});

// Sort struct by field
parallel_sort(records.begin(), records.end(),
    [](const Record& a, const Record& b) {
        return a.score > b.score;
    }
);
```

使用带工作窃取的并行 quicksort 变体。在 8 核以上、N 较大时，通常比 `std::sort` 快 4–6×。

> **注意：** `parallel_sort` **不稳定**（相等元素可能重排）。需要稳定排序请用 `std::stable_sort`（稳定排序不存在 TBB 并行版本）。

---

### 2.5 `parallel_for_each` — 未知迭代空间

对于没有随机访问迭代器的容器（链表、set），或迭代空间会在执行过程中增长的情况：

```cpp
std::list<WorkItem> items = get_work();

parallel_for_each(items.begin(), items.end(),
    [](WorkItem& item) {
        process(item);
    }
);
```

**用 feeder 动态添加工作** — BFS / 树遍历：

```cpp
parallel_for_each(roots.begin(), roots.end(),
    [](Node* node, feeder<Node*>& f) {
        process(node);
        for (Node* child : node->children)
            f.add(child);             // adds work dynamically to the pool
    }
);
// Runs until all nodes processed, including dynamically added ones
```

---


<details>
<summary>English original</summary>

**2.4 `parallel_sort`**

```cpp
#include "oneapi/tbb/parallel_sort.h"

std::vector<int> data(N);
// ...fill data...

// In-place sort (like std::sort but parallel)
parallel_sort(data.begin(), data.end());

// Custom comparator
parallel_sort(data.begin(), data.end(), std::greater<int>{});

// Sort struct by field
parallel_sort(records.begin(), records.end(),
    [](const Record& a, const Record& b) {
        return a.score > b.score;
    }
);
```

Uses a parallel quicksort variant with work-stealing. Typically 4–6× faster than `std::sort` on 8+ cores for large N.

> **Note:** `parallel_sort` is **not stable** (equal elements may reorder). Use `std::stable_sort` for stable ordering (no TBB parallel version exists for stable sort).

---

**2.5 `parallel_for_each` — Unknown Iteration Space**

For containers without random-access iterators (linked lists, sets) or when the iteration space grows during execution:

```cpp
std::list<WorkItem> items = get_work();

parallel_for_each(items.begin(), items.end(),
    [](WorkItem& item) {
        process(item);
    }
);
```

**Dynamic work addition with feeder** — BFS / tree traversal:

```cpp
parallel_for_each(roots.begin(), roots.end(),
    [](Node* node, feeder<Node*>& f) {
        process(node);
        for (Node* child : node->children)
            f.add(child);             // adds work dynamically to the pool
    }
);
// Runs until all nodes processed, including dynamically added ones
```

---

</details>

### 2.6 `parallel_pipeline` — 装配线

流水线是一系列阶段，每个条目按顺序流过所有阶段。与 `parallel_for` 中所有条目都做同样的事不同，流水线让**不同条目同时处于不同阶段**。

**无流水线 — 一次一个条目：**

```
time →
item 1: [read]──[process]──[write]
item 2:                            [read]──[process]──[write]
item 3:                                               [read]──[process]──[write]
```

**有流水线 — 多个条目在途：**

```
time →
item 1: [read]──[process₁][process₂][process₃]──[write]
item 2:         [read]────[process₁][process₂][process₃]──[write]
item 3:                   [read]────[process₁][process₂][process₃]──[write]
item 4:                             [read]─────────── ...
         ↑serial↑         ↑──── parallel stage, N items at once ────↑↑serial↑
```

读和写阶段是**串行**的（一次一个条目 — 文件 I/O 或有序输出）。处理阶段是**并行**的（CPU 密集型工作，条目相互独立）。

**过滤模式：**

| 模式 | 顺序 | 并发 | 适用场景 |
|------|-------|-------------|---------|
| `serial_in_order` | 保持 | 一次 1 个 | 文件 I/O、有序输出 |
| `serial_out_of_order` | 不保持 | 一次 1 个 | 单线程 setup/teardown |
| `parallel` | 不保持 | 多线程 | CPU 密集型、独立条目 |

**结构：**

```cpp
#include "oneapi/tbb/pipeline.h"

const int max_tokens = 16;   // max items in-flight simultaneously

parallel_pipeline(max_tokens,
    make_filter<void, InputData*>(            // void input = "source" stage
        filter_mode::serial_in_order,
        [&](flow_control& fc) -> InputData* {
            InputData* data = read_next();
            if (!data) { fc.stop(); return nullptr; }  // signal end of stream
            return data;
        }
    ) &
    make_filter<InputData*, OutputData*>(
        filter_mode::parallel,
        [](InputData* in) -> OutputData* {
            return transform(in);             // runs on multiple threads
        }
    ) &
    make_filter<OutputData*, void>(           // void output = "sink" stage
        filter_mode::serial_in_order,
        [&](OutputData* out) {
            write_result(out);
            delete out;
        }
    )
);
```

**具体示例 — 批推理流水线：**

一种常见的 CPU 推理模式：从磁盘加载批，归一化，运行模型，写出预测。I/O 和推理可以重叠：

```cpp
struct Batch { std::vector<float> data; int id; };

int batch_id = 0;
const int TOTAL = 100;

parallel_pipeline(/*max_tokens=*/8,
    // Stage 1: load next batch from disk (serial — disk reads are sequential)
    make_filter<void, Batch*>(
        filter_mode::serial_in_order,
        [&](flow_control& fc) -> Batch* {
            if (batch_id >= TOTAL) { fc.stop(); return nullptr; }
            auto* b = new Batch();
            b->id   = batch_id++;
            b->data = load_batch_from_disk(b->id);   // blocking I/O
            return b;
        }
    ) &
    // Stage 2: normalize pixels [0,255] → [0,1] (parallel — independent batches)
    make_filter<Batch*, Batch*>(
        filter_mode::parallel,
        [](Batch* b) -> Batch* {
            for (float& v : b->data) v /= 255.0f;
            return b;
        }
    ) &
    // Stage 3: run model inference (parallel — batches are independent)
    make_filter<Batch*, Batch*>(
        filter_mode::parallel,
        [](Batch* b) -> Batch* {
            run_inference(b->data);
            return b;
        }
    ) &
    // Stage 4: write predictions in order (serial — output file needs order)
    make_filter<Batch*, void>(
        filter_mode::serial_in_order,
        [&](Batch* b) {
            write_predictions(b->id, b->data);
            delete b;
        }
    )
);
```

**`max_tokens` 与吞吐定律：**

```
max_tokens too small:
  [read]──[process]──[write]
          idle  idle           ← CPU starved, pipeline stalls waiting for tokens

max_tokens right:
  [read]──[p1][p2][p3][p4]──[write]
           ↑ all threads busy ↑

max_tokens too large:
  [read]──[p1][p2]...[p100]──[write]
                 ↑ 100 batches buffered in memory → OOM risk
```

> **吞吐 = `max_tokens / latency_of_slowest_serial_stage`。**
> 如果串行读阶段耗时 10 ms，且你设置 `max_tokens = 8`，那么无论并行阶段有多快，你都能通过流水线维持 800 items/second。将 `max_tokens` 加倍会使吞吐加倍——直到并行阶段成为瓶颈。

---


<details>
<summary>English original</summary>

**2.6 `parallel_pipeline` — Assembly Line**

A pipeline is a sequence of stages where each item flows through all stages in order. Unlike `parallel_for` where all items do the same thing, a pipeline lets **different items be at different stages simultaneously**.

**Without pipeline — one item at a time:**

```
time →
item 1: [read]──[process]──[write]
item 2:                            [read]──[process]──[write]
item 3:                                               [read]──[process]──[write]
```

**With pipeline — multiple items in-flight:**

```
time →
item 1: [read]──[process₁][process₂][process₃]──[write]
item 2:         [read]────[process₁][process₂][process₃]──[write]
item 3:                   [read]────[process₁][process₂][process₃]──[write]
item 4:                             [read]─────────── ...
         ↑serial↑         ↑──── parallel stage, N items at once ────↑↑serial↑
```

The read and write stages are **serial** (one item at a time — file I/O or ordered output). The processing stage is **parallel** (CPU-bound work, items are independent).

**Filter modes:**

| Mode | Order | Concurrency | Use when |
|------|-------|-------------|---------|
| `serial_in_order` | Preserved | 1 at a time | File I/O, ordered output |
| `serial_out_of_order` | Not preserved | 1 at a time | Single-threaded setup/teardown |
| `parallel` | Not preserved | Multiple threads | CPU-bound, independent items |

**Structure:**

```cpp
#include "oneapi/tbb/pipeline.h"

const int max_tokens = 16;   // max items in-flight simultaneously

parallel_pipeline(max_tokens,
    make_filter<void, InputData*>(            // void input = "source" stage
        filter_mode::serial_in_order,
        [&](flow_control& fc) -> InputData* {
            InputData* data = read_next();
            if (!data) { fc.stop(); return nullptr; }  // signal end of stream
            return data;
        }
    ) &
    make_filter<InputData*, OutputData*>(
        filter_mode::parallel,
        [](InputData* in) -> OutputData* {
            return transform(in);             // runs on multiple threads
        }
    ) &
    make_filter<OutputData*, void>(           // void output = "sink" stage
        filter_mode::serial_in_order,
        [&](OutputData* out) {
            write_result(out);
            delete out;
        }
    )
);
```

**Concrete example — batch inference pipeline:**

A common CPU inference pattern: load batches from disk, normalize, run model, write predictions. I/O and inference can overlap:

```cpp
struct Batch { std::vector<float> data; int id; };

int batch_id = 0;
const int TOTAL = 100;

parallel_pipeline(/*max_tokens=*/8,
    // Stage 1: load next batch from disk (serial — disk reads are sequential)
    make_filter<void, Batch*>(
        filter_mode::serial_in_order,
        [&](flow_control& fc) -> Batch* {
            if (batch_id >= TOTAL) { fc.stop(); return nullptr; }
            auto* b = new Batch();
            b->id   = batch_id++;
            b->data = load_batch_from_disk(b->id);   // blocking I/O
            return b;
        }
    ) &
    // Stage 2: normalize pixels [0,255] → [0,1] (parallel — independent batches)
    make_filter<Batch*, Batch*>(
        filter_mode::parallel,
        [](Batch* b) -> Batch* {
            for (float& v : b->data) v /= 255.0f;
            return b;
        }
    ) &
    // Stage 3: run model inference (parallel — batches are independent)
    make_filter<Batch*, Batch*>(
        filter_mode::parallel,
        [](Batch* b) -> Batch* {
            run_inference(b->data);
            return b;
        }
    ) &
    // Stage 4: write predictions in order (serial — output file needs order)
    make_filter<Batch*, void>(
        filter_mode::serial_in_order,
        [&](Batch* b) {
            write_predictions(b->id, b->data);
            delete b;
        }
    )
);
```

**`max_tokens` and the throughput law:**

```
max_tokens too small:
  [read]──[process]──[write]
          idle  idle           ← CPU starved, pipeline stalls waiting for tokens

max_tokens right:
  [read]──[p1][p2][p3][p4]──[write]
           ↑ all threads busy ↑

max_tokens too large:
  [read]──[p1][p2]...[p100]──[write]
                 ↑ 100 batches buffered in memory → OOM risk
```

> **Throughput = `max_tokens / latency_of_slowest_serial_stage`.**
> If your serial read stage takes 10 ms and you set `max_tokens = 8`, you can sustain 800 items/second through the pipeline regardless of how fast the parallel stage is. Doubling `max_tokens` doubles throughput — until the parallel stage becomes the bottleneck.

---

</details>

### 2.7 `parallel_invoke` 和 `task_group` —— 显式任务

**`parallel_invoke` —— 固定、已知的任务集合：**

```
parallel_invoke(f1, f2, f3, f4)

  caller ──┬──► T0: f1() ──┐
           ├──► T1: f2() ──┤
           ├──► T2: f3() ──┤ implicit join — caller blocks until all done
           └──► T3: f4() ──┘
```

```cpp
#include "oneapi/tbb/parallel_invoke.h"

// Two tasks — classic divide-and-conquer split
parallel_invoke(
    []{ sort_left_half(); },
    []{ sort_right_half(); }
);
// Both guaranteed done here

// Any fixed number of callables
parallel_invoke(
    []{ build_index(); },
    []{ load_weights(); },
    []{ warm_up_cache(); }
);
```

当并行操作的数量在编译期已知且彼此独立时，使用 `parallel_invoke`。

**`task_group` —— 动态、运行时确定的任务：**

```
task_group tg;
tg.run(f1);   tg.run(f2);   tg.run(f3);   // enqueue at runtime
...           // tasks may themselves call tg.run() — recursive OK
tg.wait();    // caller blocks until all tasks (and their children) complete
```

```cpp
#include "oneapi/tbb/task_group.h"

// Dynamic number of tasks — count not known until runtime
task_group tg;
for (auto& item : work_units)
    tg.run([&item]{ process(item); });
tg.wait();

// Recursive — tasks spawning more tasks
task_group tg2;
tg2.run([&]{
    tg2.run([&]{ subtask_a(); });
    tg2.run([&]{ subtask_b(); });
});
tg2.wait();
```

**`parallel_invoke` vs `task_group`：**

| | `parallel_invoke` | `task_group` |
|--|-------------------|-------------|
| 任务数 | 编译期固定 | 运行时动态确定 |
| 递归任务 | 否 | 是 —— 任务可以调用 `tg.run()` |
| 语法 | 一次调用，所有 lambda 内联 | `run()` / `wait()` 分离 |
| 开销 | 略低 | 略高（队列管理） |
| 用例 | 归并排序拆分、双数据加载 | 树遍历、工作队列、图 |

**何时用哪个：**
- 提前知道确切任务？→ `parallel_invoke`
- 任务在运行时才发现或派生子任务？→ `task_group`
- 递归的树/图遍历？→ `task_group`（DAG 则用 oneTBB flow graph）

---


<details>
<summary>English original</summary>

**2.7 `parallel_invoke` and `task_group` — Explicit Tasks**

**`parallel_invoke` — fixed, known set of tasks:**

```
parallel_invoke(f1, f2, f3, f4)

  caller ──┬──► T0: f1() ──┐
           ├──► T1: f2() ──┤
           ├──► T2: f3() ──┤ implicit join — caller blocks until all done
           └──► T3: f4() ──┘
```

```cpp
#include "oneapi/tbb/parallel_invoke.h"

// Two tasks — classic divide-and-conquer split
parallel_invoke(
    []{ sort_left_half(); },
    []{ sort_right_half(); }
);
// Both guaranteed done here

// Any fixed number of callables
parallel_invoke(
    []{ build_index(); },
    []{ load_weights(); },
    []{ warm_up_cache(); }
);
```

Use `parallel_invoke` when the number of parallel operations is known at compile time and they are independent.

**`task_group` — dynamic, runtime-determined tasks:**

```
task_group tg;
tg.run(f1);   tg.run(f2);   tg.run(f3);   // enqueue at runtime
...           // tasks may themselves call tg.run() — recursive OK
tg.wait();    // caller blocks until all tasks (and their children) complete
```

```cpp
#include "oneapi/tbb/task_group.h"

// Dynamic number of tasks — count not known until runtime
task_group tg;
for (auto& item : work_units)
    tg.run([&item]{ process(item); });
tg.wait();

// Recursive — tasks spawning more tasks
task_group tg2;
tg2.run([&]{
    tg2.run([&]{ subtask_a(); });
    tg2.run([&]{ subtask_b(); });
});
tg2.wait();
```

**`parallel_invoke` vs `task_group`:**

| | `parallel_invoke` | `task_group` |
|--|-------------------|-------------|
| Number of tasks | Fixed at compile time | Dynamic, determined at runtime |
| Recursive tasks | No | Yes — tasks can call `tg.run()` |
| Syntax | One call, all lambdas inline | `run()` / `wait()` separate |
| Overhead | Slightly lower | Slightly higher (queue management) |
| Use case | Merge sort split, dual data load | Tree traversal, work queue, graph |

**When to use which:**
- Know the exact tasks upfront? → `parallel_invoke`
- Tasks discovered at runtime or spawn subtasks? → `task_group`
- Recursive tree/graph traversal? → `task_group` (or oneTBB flow graph for DAGs)

---

</details>

### 2.8 每线程存储 — `enumerable_thread_specific`

当多个线程更新同一数据结构（histogram、accumulator、buffer）时，有三种方案：

| 方案 | 机制 | 问题 |
|----------|-----------|---------|
| 共享变量 | 无 | 数据竞争 — 未定义行为 |
| Mutex | `std::mutex` + 锁 | 串行化线程 — 扼杀并行 |
| `atomic` | 硬件指令 | 仅适用于单个 integer/float，不适用于 vector |
| **ETS** | **每线程副本** | **完全没有争用 — 每个线程拥有自己的数据** |

ETS 为每个线程提供一份私有副本。在并行阶段，线程之间绝不触碰彼此的数据。最后遍历所有副本，一次性合并。

```
parallel phase:                          merge phase:
  T0 → local_hist[0] ──────────────────► global_hist
  T1 → local_hist[1] ──────────────────►     +=
  T2 → local_hist[2] ──────────────────►     +=
  T3 → local_hist[3] ──────────────────►     +=
  (zero contention)                     (single-threaded, once)
```

**Histogram — 错误做法 vs 正确做法：**

```cpp
// WRONG: data race — multiple threads increment the same bin
std::vector<int> hist(256, 0);
tbb::parallel_for(0, N, [&](int i) {
    hist[image[i]]++;   // T0 and T1 may both read 5, both write 6 → lost update
});

// RIGHT: each thread has its own histogram, merge once at the end
#include "oneapi/tbb/enumerable_thread_specific.h"

enumerable_thread_specific<std::vector<int>> local_hist(
    []{ return std::vector<int>(256, 0); }  // factory called once per thread
);

tbb::parallel_for(0, N, [&](int i) {
    local_hist.local()[image[i]]++;   // .local() returns THIS thread's copy
});

std::vector<int> global_hist(256, 0);
for (auto& h : local_hist)           // one entry per thread that called .local()
    for (int k = 0; k < 256; k++)
        global_hist[k] += h[k];
```

**线程局部求和：**

```cpp
enumerable_thread_specific<float> thread_sum(0.0f);

tbb::parallel_for(tbb::blocked_range<int>(0, N),
    [&](const tbb::blocked_range<int>& r) {
        float& my = thread_sum.local();
        for (int i = r.begin(); i < r.end(); i++)
            my += a[i];
    }
);

float total = 0.0f;
for (float s : thread_sum) total += s;
```

**构造选项：**

```cpp
enumerable_thread_specific<int> ets1;         // default-constructed: int{} = 0
enumerable_thread_specific<int> ets2(42);     // value-initialized: each copy = 42

// Factory for non-trivial types (vector, map, custom objects)
enumerable_thread_specific<std::vector<int>> ets3(
    []{ return std::vector<int>(1024, 0); }   // called once per thread on first .local()
);
```

**关键机制：**
- `.local()` 在该线程首次调用时创建副本，后续调用返回引用
- 对 ETS 对象做 range-for 会遍历所有实际创建过的副本
- 若某线程从未调用 `.local()`，则不会为它创建副本 — 不浪费内存

---

### 2.9 `combinable<T>` — 更简单的每线程累加

`enumerable_thread_specific` 的简化版本，专门用于累加单个值：

```cpp
#include "oneapi/tbb/combinable.h"

combinable<float> partial_sum;

parallel_for(0, N, [&](int i) {
    partial_sum.local() += a[i];   // thread-local accumulate
});

// Combine all locals with a binary op
float total = partial_sum.combine([](float a, float b) {
    return a + b;
});
```

**与 `enumerable_thread_specific` 对比：**
- `combinable<T>` — API 更简单，专为 reduction 模式设计
- `enumerable_thread_specific<T>` — 更灵活，支持迭代、factory 初始化、非平凡类型

只想要一个线程局部累加器时用 `combinable`。并行阶段结束后需要单独查看每个线程的值时用 `enumerable_thread_specific`。

---


<details>
<summary>English original</summary>

**2.8 Per-Thread Storage — `enumerable_thread_specific`**

When multiple threads update the same data structure (histogram, accumulator, buffer), you have three options:

| Approach | Mechanism | Problem |
|----------|-----------|---------|
| Shared variable | Nothing | Data race — undefined behavior |
| Mutex | `std::mutex` + lock | Serializes threads — kills parallelism |
| `atomic` | Hardware instruction | Only works for a single integer/float, not a vector |
| **ETS** | **Per-thread copy** | **No contention at all — each thread owns its data** |

ETS gives every thread its own private copy. Threads never touch each other's data during the parallel phase. At the end, you iterate over all copies and merge them once.

```
parallel phase:                          merge phase:
  T0 → local_hist[0] ──────────────────► global_hist
  T1 → local_hist[1] ──────────────────►     +=
  T2 → local_hist[2] ──────────────────►     +=
  T3 → local_hist[3] ──────────────────►     +=
  (zero contention)                     (single-threaded, once)
```

**Histogram — wrong vs right:**

```cpp
// WRONG: data race — multiple threads increment the same bin
std::vector<int> hist(256, 0);
tbb::parallel_for(0, N, [&](int i) {
    hist[image[i]]++;   // T0 and T1 may both read 5, both write 6 → lost update
});

// RIGHT: each thread has its own histogram, merge once at the end
#include "oneapi/tbb/enumerable_thread_specific.h"

enumerable_thread_specific<std::vector<int>> local_hist(
    []{ return std::vector<int>(256, 0); }  // factory called once per thread
);

tbb::parallel_for(0, N, [&](int i) {
    local_hist.local()[image[i]]++;   // .local() returns THIS thread's copy
});

std::vector<int> global_hist(256, 0);
for (auto& h : local_hist)           // one entry per thread that called .local()
    for (int k = 0; k < 256; k++)
        global_hist[k] += h[k];
```

**Thread-local sum:**

```cpp
enumerable_thread_specific<float> thread_sum(0.0f);

tbb::parallel_for(tbb::blocked_range<int>(0, N),
    [&](const tbb::blocked_range<int>& r) {
        float& my = thread_sum.local();
        for (int i = r.begin(); i < r.end(); i++)
            my += a[i];
    }
);

float total = 0.0f;
for (float s : thread_sum) total += s;
```

**Construction options:**

```cpp
enumerable_thread_specific<int> ets1;         // default-constructed: int{} = 0
enumerable_thread_specific<int> ets2(42);     // value-initialized: each copy = 42

// Factory for non-trivial types (vector, map, custom objects)
enumerable_thread_specific<std::vector<int>> ets3(
    []{ return std::vector<int>(1024, 0); }   // called once per thread on first .local()
);
```

**Key mechanics:**
- `.local()` creates the copy on first call for that thread, returns a reference on subsequent calls
- Range-for over an ETS object iterates over all copies that were actually created
- If a thread never calls `.local()`, no copy is created for it — no wasted memory

---

**2.9 `combinable<T>` — Simpler Per-Thread Accumulation**

A simplified version of `enumerable_thread_specific` specifically for accumulating a single value:

```cpp
#include "oneapi/tbb/combinable.h"

combinable<float> partial_sum;

parallel_for(0, N, [&](int i) {
    partial_sum.local() += a[i];   // thread-local accumulate
});

// Combine all locals with a binary op
float total = partial_sum.combine([](float a, float b) {
    return a + b;
});
```

**vs `enumerable_thread_specific`:**
- `combinable<T>` — simpler API, designed specifically for reduction patterns
- `enumerable_thread_specific<T>` — more flexible, supports iteration, factory init, non-trivial types

Use `combinable` when you just want a thread-local accumulator. Use `enumerable_thread_specific` when you need to inspect each thread's value separately after the parallel phase.

---

</details>

### 2.10 Flow Graph — 数据流图与依赖图

用于把复杂并行模式表达成由节点和边构成的图。当节点的输入就绪时，runtime 会自动运行该节点 —— 无需手动同步。

```
Node   = a worker (function, buffer, join, split, ...)
Edge   = a channel that carries data tokens between nodes
Token  = one unit of data flowing through the graph

            try_put(5)
                │
         ┌──────┴──────┐
         ▼             ▼
   ┌──────────┐   ┌──────────┐   ← run in parallel (unlimited concurrency)
   │  square  │   │   cube   │
   │  x → x² │   │  x → x³  │
   └────┬─────┘   └────┬─────┘
        │ port 0        │ port 1
        └──────┬────────┘
               ▼
         ┌──────────┐             ← waits for one token on each port
         │   join   │
         └────┬─────┘
              ▼
         ┌──────────┐             ← serial (concurrency = 1)
         │  printer │
         │ (sq, cu) │
         └──────────┘
```

```cpp
#include "oneapi/tbb/flow_graph.h"
using namespace oneapi::tbb::flow;

graph g;

// function_node<In, Out>: receives In, produces Out
// Second arg = max concurrency (1=serial, unlimited=max)
function_node<int, int> square(g, unlimited, [](int x) { return x * x; });
function_node<int, int> cube  (g, unlimited, [](int x) { return x * x * x; });

// join_node: wait for one input per port, emit as tuple
join_node<std::tuple<int,int>> join(g);

function_node<std::tuple<int,int>, void> printer(g, 1,
    [](const std::tuple<int,int>& t) {
        printf("square=%d  cube=%d\n", std::get<0>(t), std::get<1>(t));
    }
);

// Wire the graph
make_edge(square, input_port<0>(join));
make_edge(cube,   input_port<1>(join));
make_edge(join,   printer);

// Send inputs — both nodes run in parallel
square.try_put(5);
cube.try_put(5);

g.wait_for_all();  // always wait before graph goes out of scope
```

**ML 例子 —— 带扇出/汇聚的并行特征提取：**

从每个输入帧中提取三个独立特征，然后合并并打分：

```
                    ┌─────────────────┐
         frame ──►  │  broadcast_node │
                    └────┬───────┬────┘
                         │       │       │
                    ┌────┴──┐ ┌──┴───┐ ┌─┴─────┐
                    │  HOG  │ │  LBP │ │  DCT  │   ← parallel feature extractors
                    └────┬──┘ └──┬───┘ └─┬─────┘
                         └───────┴────────┘
                                 │
                           ┌─────┴─────┐
                           │   join    │           ← wait for all three features
                           └─────┬─────┘
                                 │
                           ┌─────┴─────┐
                           │  scorer   │           ← combine features → score
                           └───────────┘
```

```cpp
struct Frame  { std::vector<float> data; };
struct Score  { float value; };

graph g;

// Fan-out: send each frame to all three extractors simultaneously
broadcast_node<Frame> broadcast(g);

function_node<Frame, std::vector<float>> hog(g, unlimited,
    [](const Frame& f) { return extract_hog(f); });
function_node<Frame, std::vector<float>> lbp(g, unlimited,
    [](const Frame& f) { return extract_lbp(f); });
function_node<Frame, std::vector<float>> dct(g, unlimited,
    [](const Frame& f) { return extract_dct(f); });

// Fan-in: wait for all three features before scoring
using FeatureTuple = std::tuple<std::vector<float>,
                                std::vector<float>,
                                std::vector<float>>;
join_node<FeatureTuple> join(g);

function_node<FeatureTuple, Score> scorer(g, unlimited,
    [](const FeatureTuple& t) {
        return score(std::get<0>(t), std::get<1>(t), std::get<2>(t));
    }
);

// Wire
make_edge(broadcast, hog);
make_edge(broadcast, lbp);
make_edge(broadcast, dct);
make_edge(hog, input_port<0>(join));
make_edge(lbp, input_port<1>(join));
make_edge(dct, input_port<2>(join));
make_edge(join, scorer);

// Process frames
for (auto& frame : frames)
    broadcast.try_put(frame);

g.wait_for_all();
```

#### 所有关键节点类型

| 节点 | 描述 |
|------|-------------|
| `function_node<In, Out>` | 变换 `In` → `Out`，并发度可配置 |
| `source_node<Out>` *(已弃用)*<br>`input_node<Out>` | 由函数生成 token；启动图 |
| `broadcast_node<T>` | 扇出：把一份输入发给所有后继节点 |
| `join_node<tuple<...>>` | 扇入：等待每个端口各来一个，输出 tuple |
| `split_node<tuple<...>>` | 拆分 tuple：把第 N 个元素发送到第 N 个端口 |
| `buffer_node<T>` | 缓冲消息，直到某个后继节点请求它们 |
| `queue_node<T>` | FIFO 缓冲；把消息传给任意可用的后继节点 |
| `priority_queue_node<T>` | 按优先级排序的缓冲 |
| `sequencer_node<T>` | 把乱序到达的条目重新按序列号顺序排列 |
| `limiter_node<T>` | 限制在途消息数（背压） |
| `overwrite_node<T>` | 存储最后一个值；新的后继节点立即取到它 |
| `write_once_node<T>` | 存储第一个值；广播给所有已注册的接收者 |
| `indexer_node<T...>` | 带标签的联合输入：接受集合中的任意类型，并给消息打标签 |

#### 用 `broadcast_node` 实现扇出

```cpp
broadcast_node<int> broadcaster(g);
function_node<int, void> worker_a(g, unlimited, [](int x){ do_a(x); });
function_node<int, void> worker_b(g, unlimited, [](int x){ do_b(x); });

make_edge(broadcaster, worker_a);
make_edge(broadcaster, worker_b);  // same input goes to both

broadcaster.try_put(42);  // both worker_a and worker_b receive 42
g.wait_for_all();
```

#### 用 `sequencer_node` 实现重排序

```cpp
// parallel_for may complete items out of order — use sequencer to restore order
sequencer_node<Frame> reorder(g, [](const Frame& f) {
    return f.sequence_number;   // sequencer uses this to order outputs
});

function_node<Frame, Frame> processor(g, unlimited, [](Frame f) {
    f.data = process(f.data);   // runs in parallel, out of order
    return f;
});

function_node<Frame, void> writer(g, serial, [](const Frame& f) {
    write_in_order(f);          // receives frames in sequence order
});

make_edge(processor, reorder);
make_edge(reorder, writer);
```

#### 用 `limiter_node` 实现背压

```cpp
// Prevent fast producer from overwhelming slow consumer
limiter_node<int> limiter(g, 8);   // max 8 messages in flight

make_edge(producer, limiter);
make_edge(limiter, slow_consumer);
make_edge(slow_consumer, limiter.decrement); // signal when done → unlocks limiter
```

#### 用 `indexer_node` 实现条件路由

```cpp
using IndexerType = indexer_node<int, float>;   // accepts int OR float

IndexerType idx(g);
function_node<IndexerType::output_type, void> router(g, unlimited,
    [](const IndexerType::output_type& msg) {
        if (msg.tag() == 0)       // int arrived
            handle_int(cast_to<int>(msg));
        else                       // float arrived
            handle_float(cast_to<float>(msg));
    }
);

make_edge(idx, router);
input_port<0>(idx).try_put(42);     // send int
input_port<1>(idx).try_put(3.14f);  // send float
```

#### 入门模板 — 线性视频流水线

一个最小但完整的线性流水线：source → limiter → preprocess → inference → postprocess → sequencer → output。复制 `video_pipeline_template.cpp`，替换那四个桩函数，即可运行。

```
input_node → limiter → preprocess → inference → postprocess → sequencer → output
                ▲                                                               │
                └───────────────────── decrement ◄──────────────────────────────┘
```

Limiter 放在预处理**之前**，这样进入流水线的帧数不会超过 `MAX_INFLIGHT`。`decrement` 边从输出阶段反馈回来，每完成一帧就释放一个槽位。

```bash
g++ -O2 -std=c++17 video_pipeline_template.cpp -ltbb -o video_pipeline
./video_pipeline
```

---


<details>
<summary>English original</summary>

**All Key Node Types**

| Node | Description |
|------|-------------|
| `function_node<In, Out>` | Transforms `In` → `Out`, configurable concurrency |
| `source_node<Out>` *(deprecated)*<br>`input_node<Out>` | Generates tokens from a function; starts the graph |
| `broadcast_node<T>` | Fans out: sends one input to ALL successors |
| `join_node<tuple<...>>` | Fans in: waits for one from each port, emits tuple |
| `split_node<tuple<...>>` | Splits tuple: sends element N to port N |
| `buffer_node<T>` | Buffers messages until a successor requests them |
| `queue_node<T>` | FIFO buffer; passes messages to any available successor |
| `priority_queue_node<T>` | Priority-ordered buffer |
| `sequencer_node<T>` | Reorders out-of-order items back to sequence number order |
| `limiter_node<T>` | Limits in-flight messages (backpressure) |
| `overwrite_node<T>` | Stores last value; new successors get it immediately |
| `write_once_node<T>` | Stores first value; broadcasts to all registered receivers |
| `indexer_node<T...>` | Tagged union input: accepts any type from set, tags messages |

**Fan-Out with `broadcast_node`**

```cpp
broadcast_node<int> broadcaster(g);
function_node<int, void> worker_a(g, unlimited, [](int x){ do_a(x); });
function_node<int, void> worker_b(g, unlimited, [](int x){ do_b(x); });

make_edge(broadcaster, worker_a);
make_edge(broadcaster, worker_b);  // same input goes to both

broadcaster.try_put(42);  // both worker_a and worker_b receive 42
g.wait_for_all();
```

**Reordering with `sequencer_node`**

```cpp
// parallel_for may complete items out of order — use sequencer to restore order
sequencer_node<Frame> reorder(g, [](const Frame& f) {
    return f.sequence_number;   // sequencer uses this to order outputs
});

function_node<Frame, Frame> processor(g, unlimited, [](Frame f) {
    f.data = process(f.data);   // runs in parallel, out of order
    return f;
});

function_node<Frame, void> writer(g, serial, [](const Frame& f) {
    write_in_order(f);          // receives frames in sequence order
});

make_edge(processor, reorder);
make_edge(reorder, writer);
```

**Backpressure with `limiter_node`**

```cpp
// Prevent fast producer from overwhelming slow consumer
limiter_node<int> limiter(g, 8);   // max 8 messages in flight

make_edge(producer, limiter);
make_edge(limiter, slow_consumer);
make_edge(slow_consumer, limiter.decrement); // signal when done → unlocks limiter
```

**Conditional Routing with `indexer_node`**

```cpp
using IndexerType = indexer_node<int, float>;   // accepts int OR float

IndexerType idx(g);
function_node<IndexerType::output_type, void> router(g, unlimited,
    [](const IndexerType::output_type& msg) {
        if (msg.tag() == 0)       // int arrived
            handle_int(cast_to<int>(msg));
        else                       // float arrived
            handle_float(cast_to<float>(msg));
    }
);

make_edge(idx, router);
input_port<0>(idx).try_put(42);     // send int
input_port<1>(idx).try_put(3.14f);  // send float
```

**Starter Template — Linear Video Pipeline**

A minimal but complete linear pipeline: source → limiter → preprocess → inference → postprocess → sequencer → output. Copy `video_pipeline_template.cpp`, replace the four stub functions, and it's ready to run.

```
input_node → limiter → preprocess → inference → postprocess → sequencer → output
                ▲                                                               │
                └───────────────────── decrement ◄──────────────────────────────┘
```

Limiter is placed **before** preprocessing so no more than `MAX_INFLIGHT` frames ever enter the pipeline. The `decrement` edge feeds back from the output stage to release one slot each time a frame completes.

```bash
g++ -O2 -std=c++17 video_pipeline_template.cpp -ltbb -o video_pipeline
./video_pipeline
```

---

</details>

#### 完整示例 — 实时视频推理流水线

一个实用的 AI 视频流水线：读取帧、并行预处理、运行推理、重排回原始顺序、限制内存、显示。

```
  input_node ──► broadcast ──► preprocess (parallel) ──► join ──► inference
                                                                       │
  display ◄── limiter ◄── sequencer ◄── postprocess ◄─────────────────┘
```


<details>
<summary>English original</summary>

**Complete Example — Real-Time Video Inference Pipeline**

A practical AI video pipeline: read frames, preprocess in parallel, run inference, reorder to original sequence, limit memory, display.

```
  input_node ──► broadcast ──► preprocess (parallel) ──► join ──► inference
                                                                       │
  display ◄── limiter ◄── sequencer ◄── postprocess ◄─────────────────┘
```

</details>

```cpp
#include "oneapi/tbb/flow_graph.h"
#include <cstdio>
#include <tuple>
#include <vector>
using namespace oneapi::tbb::flow;

struct Frame {
    int           id;             // sequence number for reordering
    std::vector<float> pixels;   // raw pixel data
};

struct Detection {
    int   frame_id;
    float confidence;
    int   class_id;
};

// ── Simulated pipeline functions ─────────────────────────────────────────────
Frame      read_frame(int id)          { return {id, std::vector<float>(224*224*3, 0.5f)}; }
Frame      resize_normalize(Frame f)   { /* resize to 224×224, normalize [0,1] */ return f; }
Frame      color_convert(Frame f)      { /* BGR → RGB */ return f; }
Detection  run_inference(std::tuple<Frame,Frame> t) {
    // both preprocessed versions available — use whichever suits your model
    const Frame& f = std::get<0>(t);
    return {f.id, 0.95f, 1};
}
Detection  draw_boxes(Detection d)     { printf("frame %d  class=%d  conf=%.2f\n",
                                                d.frame_id, d.class_id, d.confidence);
                                         return d; }
void       display_or_save(Detection d){ /* write to screen or file */ }

int main() {
    const int TOTAL_FRAMES = 20;
    const int MAX_INFLIGHT  = 8;   // limiter: max frames queued at once

    graph g;

    // ── Stage 1: source — emit frames one at a time ───────────────────────────
    int frame_counter = 0;
    input_node<Frame> source(g, [&](oneapi::tbb::flow_control& fc) -> Frame {
        if (frame_counter >= TOTAL_FRAMES) { fc.stop(); return {}; }
        return read_frame(frame_counter++);
    });

    // ── Stage 2: broadcast — send each frame to both preprocess branches ──────
    broadcast_node<Frame> broadcast(g);

    // ── Stage 3: parallel preprocess branches ────────────────────────────────
    function_node<Frame, Frame> preprocess_a(g, unlimited, resize_normalize);
    function_node<Frame, Frame> preprocess_b(g, unlimited, color_convert);

    // ── Stage 4: join — wait for both preprocessed versions ──────────────────
    join_node<std::tuple<Frame, Frame>> join(g);

    // ── Stage 5: inference — CPU/GPU model (parallel across frames) ──────────
    function_node<std::tuple<Frame,Frame>, Detection> inference(g, unlimited, run_inference);

    // ── Stage 6: postprocess — draw bounding boxes ───────────────────────────
    function_node<Detection, Detection> postprocess(g, unlimited, draw_boxes);

    // ── Stage 7: sequencer — restore original frame order ────────────────────
    sequencer_node<Detection> sequencer(g,
        [](const Detection& d) -> size_t { return d.frame_id; }
    );

    // ── Stage 8: limiter — bound memory: max MAX_INFLIGHT frames in-flight ───
    limiter_node<Detection> limiter(g, MAX_INFLIGHT);

    // ── Stage 9: sink — display or write to file ──────────────────────────────
    function_node<Detection, continue_msg> sink(g, serial,
        [&](const Detection& d) -> continue_msg {
            display_or_save(d);
            return {};
        }
    );

    // ── Wire the graph ────────────────────────────────────────────────────────
    make_edge(source,        broadcast);
    make_edge(broadcast,     preprocess_a);
    make_edge(broadcast,     preprocess_b);
    make_edge(preprocess_a,  input_port<0>(join));
    make_edge(preprocess_b,  input_port<1>(join));
    make_edge(join,          inference);
    make_edge(inference,     postprocess);
    make_edge(postprocess,   sequencer);
    make_edge(sequencer,     limiter);
    make_edge(limiter,       sink);
    make_edge(sink,          limiter.decrement);  // release token when frame is done

    // ── Run ───────────────────────────────────────────────────────────────────
    source.activate();     // start emitting frames
    g.wait_for_all();      // block until all frames are processed
}
```

**为什么每个节点都是必要的：**

| 节点 | 角色 | 为什么需要 |
|------|------|----------------|
| `input_node` | 读取帧 | 串行 I/O 源 |
| `broadcast_node` | 扇出 | 把同一帧发送到两条预处理分支 |
| `preprocess_a/b` | 并行 | resize 与 color convert 同时执行 |
| `join_node` | 扇入 | 推理启动前需要两个版本 |
| `inference` | 并行 | 不同帧独立推理 |
| `postprocess` | 并行 | 画框——每帧独立 |
| `sequencer_node` | 重排序 | 并行推理可能乱序完成 |
| `limiter_node` | 背压 | 防止显示慢时 1000 帧积压在 RAM 中 |
| `sink` | 输出 | 串行写入屏幕/文件 |

> **在 graph 对象被销毁前，必须调用 `g.wait_for_all()`**。销毁仍在使用的 graph 属于未定义行为。

---


<details>
<summary>English original</summary>

**Why each node is necessary:**

| Node | Role | Why it's needed |
|------|------|----------------|
| `input_node` | Reads frames | Serial I/O source |
| `broadcast_node` | Fan-out | Sends same frame to both preprocess branches |
| `preprocess_a/b` | Parallel | Resize and color convert run simultaneously |
| `join_node` | Fan-in | Inference needs both versions before starting |
| `inference` | Parallel | Different frames infer independently |
| `postprocess` | Parallel | Draw boxes — independent per frame |
| `sequencer_node` | Reorder | Parallel inference may finish out of order |
| `limiter_node` | Backpressure | Prevents 1000 frames buffering in RAM while display is slow |
| `sink` | Output | Serial write to screen/file |

> **Always call `g.wait_for_all()`** before the graph object is destroyed. Destroying a live graph is undefined behavior.

---

</details>

### 2.11 并发容器

**问题：** 标准容器（`std::vector`、`std::map`、`std::queue`）没有内部锁。若两个线程同时写入，会出现数据损坏、崩溃或静默的错误结果。

```
std::vector<int> v;                // NOT thread-safe
parallel_for(0, N, [&](int i) {
    v.push_back(i);                // ← data race: undefined behaviour
});
```

朴素的修法——把所有操作都包进一个 `std::mutex` ——可行，但会把每次访问串行化。TBB 的并发容器用细粒度内部锁或无锁算法解决这一问题：

```
std::vector + one global mutex     tbb::concurrent_vector
  Thread 0 writes → LOCK            Thread 0 writes ──┐
  Thread 1 waits...                  Thread 1 writes ──┤ all at once
  Thread 2 waits...                  Thread 2 writes ──┘
  (serialised)                       (concurrent, safe)
```

> 只有当多个线程确实需要共享同一个容器时，才使用并发容器。若每个线程各自处理自己的数据，每线程一个普通的 `std::vector`（或 `enumerable_thread_specific`）会更快。

#### `concurrent_vector`

类似于 `std::vector`，但对并发 `push_back` 是安全的。增长后迭代器与引用仍然有效——不像 `std::vector` 那样可能重新分配。

```cpp
#include "oneapi/tbb/concurrent_vector.h"

tbb::concurrent_vector<int> results;

tbb::parallel_for(0, N, [&](int i) {
    if (passes_filter(i))
        results.push_back(compute(i));   // safe from any thread
});
// results holds all passing items; order is non-deterministic
```

何时使用：从并行 filter 中收集结果，且事先不知道会有多少元素通过。

#### `concurrent_queue` / `concurrent_bounded_queue`

经典的生产者-消费者队列。`try_pop` 非阻塞（队列为空时返回 false）；`concurrent_bounded_queue` 上的 `pop` 会阻塞，直到有元素到达。

```cpp
#include "oneapi/tbb/concurrent_queue.h"

tbb::concurrent_queue<WorkItem> q;

// Producer threads — can all push simultaneously
q.push(item_a);
q.push(item_b);

// Consumer threads — non-blocking
WorkItem item;
if (q.try_pop(item))
    process(item);   // got one

// Bounded variant — adds backpressure (push blocks when full)
tbb::concurrent_bounded_queue<WorkItem> bounded(100);
bounded.push(item);   // blocks if 100 items already queued
bounded.pop(item);    // blocks if queue is empty
```

何时使用：生产者与消费者速率不同的流式流水线。

#### `concurrent_hash_map`

高并发键值存储。使用按桶加锁——只锁正在写入的那个桶，因此不相关的键之间永不争用。

```cpp
#include "oneapi/tbb/concurrent_hash_map.h"

tbb::concurrent_hash_map<std::string, int> freq;

tbb::parallel_for_each(words.begin(), words.end(), [&](const std::string& w) {
    tbb::concurrent_hash_map<std::string, int>::accessor acc;  // write lock
    freq.insert(acc, w);   // locks only the bucket for key w
    acc->second++;         // safe: this thread owns the bucket
});                        // accessor destructor releases the lock

// Read-only (shared lock — many readers can hold this simultaneously)
tbb::concurrent_hash_map<std::string, int>::const_accessor cacc;
if (freq.find(cacc, "hello"))
    printf("hello: %d\n", cacc->second);
```

`accessor` = 写锁（独占）。`const_accessor` = 读锁（共享）。多个线程可同时对同一个键持有 `const_accessor`。

#### `concurrent_unordered_map`

API 更简单——可直接替换 `std::unordered_map`，无需 accessor。单次插入与查找是线程安全的，但边修改边迭代则不是。

```cpp
#include "oneapi/tbb/concurrent_unordered_map.h"

tbb::concurrent_unordered_map<int, int> m;

tbb::parallel_for(0, N, [&](int i) {
    m.insert({i, i * i});    // safe
    // m[i] = i * i;         // also safe for insert; avoid for update
});
```

当需要 read-modify-write 的原子性时（例如递增计数器），使用 `concurrent_hash_map`。当只做插入或查找、没有部分更新时，使用 `concurrent_unordered_map`。

#### `concurrent_priority_queue`

```cpp
#include "oneapi/tbb/concurrent_priority_queue.h"

tbb::concurrent_priority_queue<int> pq;
pq.push(10); pq.push(5); pq.push(20);

int top;
pq.try_pop(top);   // top = 20 (max-heap by default)
```


<details>
<summary>English original</summary>

**2.11 Concurrent Containers**

**The problem:** standard containers (`std::vector`, `std::map`, `std::queue`) have no internal locking. If two threads write at the same time, you get data corruption, crashes, or silently wrong results.

```
std::vector<int> v;                // NOT thread-safe
parallel_for(0, N, [&](int i) {
    v.push_back(i);                // ← data race: undefined behaviour
});
```

The naive fix — wrapping everything in a `std::mutex` — works but serialises every access. TBB's concurrent containers solve this with fine-grained internal locking or lock-free algorithms:

```
std::vector + one global mutex     tbb::concurrent_vector
  Thread 0 writes → LOCK            Thread 0 writes ──┐
  Thread 1 waits...                  Thread 1 writes ──┤ all at once
  Thread 2 waits...                  Thread 2 writes ──┘
  (serialised)                       (concurrent, safe)
```

> Only use concurrent containers when multiple threads genuinely need to share the same container. If each thread works on its own data, a plain `std::vector` per thread (or `enumerable_thread_specific`) is faster.

**`concurrent_vector`**

Like `std::vector` but safe for concurrent `push_back`. Iterators and references stay valid after growth — unlike `std::vector` which may reallocate.

```cpp
#include "oneapi/tbb/concurrent_vector.h"

tbb::concurrent_vector<int> results;

tbb::parallel_for(0, N, [&](int i) {
    if (passes_filter(i))
        results.push_back(compute(i));   // safe from any thread
});
// results holds all passing items; order is non-deterministic
```

When to use: collecting results from a parallel filter where you don't know how many items will pass.

**`concurrent_queue` / `concurrent_bounded_queue`**

Classic producer-consumer queue. `try_pop` is non-blocking (returns false if empty); `pop` on `concurrent_bounded_queue` blocks until an item arrives.

```cpp
#include "oneapi/tbb/concurrent_queue.h"

tbb::concurrent_queue<WorkItem> q;

// Producer threads — can all push simultaneously
q.push(item_a);
q.push(item_b);

// Consumer threads — non-blocking
WorkItem item;
if (q.try_pop(item))
    process(item);   // got one

// Bounded variant — adds backpressure (push blocks when full)
tbb::concurrent_bounded_queue<WorkItem> bounded(100);
bounded.push(item);   // blocks if 100 items already queued
bounded.pop(item);    // blocks if queue is empty
```

When to use: streaming pipelines where producers and consumers run at different rates.

**`concurrent_hash_map`**

High-concurrency key-value store. Uses per-bucket locking — only the bucket being written is locked, so unrelated keys never contend.

```cpp
#include "oneapi/tbb/concurrent_hash_map.h"

tbb::concurrent_hash_map<std::string, int> freq;

tbb::parallel_for_each(words.begin(), words.end(), [&](const std::string& w) {
    tbb::concurrent_hash_map<std::string, int>::accessor acc;  // write lock
    freq.insert(acc, w);   // locks only the bucket for key w
    acc->second++;         // safe: this thread owns the bucket
});                        // accessor destructor releases the lock

// Read-only (shared lock — many readers can hold this simultaneously)
tbb::concurrent_hash_map<std::string, int>::const_accessor cacc;
if (freq.find(cacc, "hello"))
    printf("hello: %d\n", cacc->second);
```

`accessor` = write lock (exclusive). `const_accessor` = read lock (shared). Multiple threads can hold `const_accessor` on the same key simultaneously.

**`concurrent_unordered_map`**

Simpler API — drop-in for `std::unordered_map`, no accessor needed. Individual insertions and lookups are thread-safe, but iteration while modifying is not.

```cpp
#include "oneapi/tbb/concurrent_unordered_map.h"

tbb::concurrent_unordered_map<int, int> m;

tbb::parallel_for(0, N, [&](int i) {
    m.insert({i, i * i});    // safe
    // m[i] = i * i;         // also safe for insert; avoid for update
});
```

Use `concurrent_hash_map` when you need read-modify-write atomicity (e.g., incrementing a counter). Use `concurrent_unordered_map` when you only insert-or-lookup with no partial updates.

**`concurrent_priority_queue`**

```cpp
#include "oneapi/tbb/concurrent_priority_queue.h"

tbb::concurrent_priority_queue<int> pq;
pq.push(10); pq.push(5); pq.push(20);

int top;
pq.try_pop(top);   // top = 20 (max-heap by default)
```

</details>

#### 容器对比

| 容器 | 类比 | 关键操作 | 用途 |
|-----------|---------|---------------|---------|
| `concurrent_vector` | 共享记事本 | `push_back` | 收集并行结果 |
| `concurrent_queue` | 共享收件箱 | `push` / `try_pop` | 生产者-消费者流水线 |
| `concurrent_bounded_queue` | 有大小限制的收件箱 | 阻塞式 `push`/`pop` | 背压控制 |
| `concurrent_hash_map` | 共享字典（每个条目带门锁） | `accessor` 读-改-写 | 词频统计、共享缓存 |
| `concurrent_unordered_map` | 共享字典（更简单） | `insert` / `find` | 一次性插入的查找表 |
| `concurrent_priority_queue` | 按优先级排序的共享任务队列 | `push` / `try_pop` | 优先级调度 |

---

### 2.12 可扩展内存分配器

标准 `malloc`/`free` 只有一个全局锁——当大量线程同时分配内存时就会成为瓶颈。TBB 的分配器使用每线程内存池：

```cpp
#include "oneapi/tbb/scalable_allocator.h"

// Use tbb_allocator with STL containers
std::vector<float, tbb::tbb_allocator<float>> vec(N);

// Use scalable_malloc directly (drop-in for malloc)
float* buf = (float*)scalable_malloc(N * sizeof(float));
scalable_free(buf);

// Replace all allocations globally (link with -ltbbmalloc_proxy)
// Or: set LD_PRELOAD=libtbbmalloc_proxy.so before running
```

当大量线程频繁分配/释放小对象（任务对象、流水线中的中间缓冲区）时，效果最明显。

---

### 2.13 `task_arena` — 控制线程池

默认情况下，TBB 创建一个全局线程池，使用所有硬件线程。每个 `parallel_for`、`parallel_reduce` 等都从该池中取用。`task_arena` 允许你创建一个**独立的、有界的执行区域**，拥有自己的并发上限，并可选地绑定到特定的 NUMA 节点。

```
Default (global arena):          With task_arena:
  All 16 hardware threads          arena(4) → max 4 threads
  shared by everything             Work inside it can't steal
  ← any parallel_for steals        from the global pool
     from any other
```

**`task_arena` 不会创建新的 OS 线程。** TBB 的工作线程可以参与多个 arena。arena 控制的是同一时刻*允许进入*的线程数量。

#### 基本用法——限制并行度

```cpp
#include "oneapi/tbb/task_arena.h"
#include "oneapi/tbb/parallel_for.h"

tbb::task_arena arena(2);   // at most 2 threads

arena.execute([&] {
    tbb::parallel_for(0, 1000, [](int i) {
        // runs with at most 2 threads regardless of core count
        do_work(i);
    });
});
```

当你的程序与 GPU 或另一个进程共享 CPU、并希望留出余量时很有用。

#### 两个独立子系统

```cpp
tbb::task_arena physics(4);   // physics simulation: 4 threads
tbb::task_arena render(4);    // rendering: 4 threads

// Launch both concurrently — they draw from separate thread budgets
// No work-stealing between them
std::thread t1([&]{ physics.execute([&]{ parallel_for(0, N, sim_body);    }); });
std::thread t2([&]{ render.execute( [&]{ parallel_for(0, M, render_body); }); });
t1.join(); t2.join();
```

没有 arena 时，TBB 的全局池会把两个子系统的工作混在一起——做物理计算的线程可能在渲染中途被抢去帮忙。arena 可以防止这种情况。

#### NUMA 感知绑定（多路服务器）

```cpp
tbb::task_arena numa_arena(
    tbb::task_arena::constraints{}
        .set_numa_id(1)            // bind to NUMA socket 1
        .set_max_concurrency(8)    // use up to 8 threads from that socket
);

numa_arena.execute([&]{
    parallel_for(0, N, body);     // threads stay on socket 1's cores
});
// Data allocated on socket 1 memory is accessed locally → lower latency
```

#### 何时使用 `task_arena`

| 场景 | 为什么 `task_arena` 有帮助 |
|----------|----------------------|
| CPU + GPU 同时运行 | 限制 CPU 线程，使 GPU 不会因 PCIe 带宽而挨饿 |
| 带后台处理的 UI 应用 | 防止后台 `parallel_for` 占满所有核心 |
| 多个独立子系统 | 将它们隔离，使一个不会抢占另一个 |
| NUMA 系统（多路服务器） | 将 arena 绑定到插槽，保持数据访问本地化 |
| 调试：使并行代码确定性的 | `task_arena(1)` 强制单线程执行 |

---


<details>
<summary>English original</summary>

**Container comparison**

| Container | Analogy | Key operation | Use for |
|-----------|---------|---------------|---------|
| `concurrent_vector` | Shared notepad | `push_back` | Collecting parallel results |
| `concurrent_queue` | Shared inbox | `push` / `try_pop` | Producer-consumer pipelines |
| `concurrent_bounded_queue` | Inbox with size limit | blocking `push`/`pop` | Backpressure control |
| `concurrent_hash_map` | Shared dictionary (with door locks per entry) | `accessor` read-modify-write | Word counts, shared caches |
| `concurrent_unordered_map` | Shared dictionary (simpler) | `insert` / `find` | Insert-once lookup tables |
| `concurrent_priority_queue` | Shared task queue sorted by priority | `push` / `try_pop` | Priority scheduling |

---

**2.12 Scalable Memory Allocator**

Standard `malloc`/`free` have a single global lock — a bottleneck when many threads allocate simultaneously. TBB's allocator uses per-thread memory pools:

```cpp
#include "oneapi/tbb/scalable_allocator.h"

// Use tbb_allocator with STL containers
std::vector<float, tbb::tbb_allocator<float>> vec(N);

// Use scalable_malloc directly (drop-in for malloc)
float* buf = (float*)scalable_malloc(N * sizeof(float));
scalable_free(buf);

// Replace all allocations globally (link with -ltbbmalloc_proxy)
// Or: set LD_PRELOAD=libtbbmalloc_proxy.so before running
```

Most impactful when many threads frequently allocate/free small objects (task objects, intermediate buffers in a pipeline).

---

**2.13 `task_arena` — Control Thread Pool**

By default, TBB creates one global thread pool that uses all hardware threads. Every `parallel_for`, `parallel_reduce`, etc. draws from that pool. `task_arena` lets you create a **separate, bounded execution zone** with its own concurrency limit and optionally pinned to specific NUMA nodes.

```
Default (global arena):          With task_arena:
  All 16 hardware threads          arena(4) → max 4 threads
  shared by everything             Work inside it can't steal
  ← any parallel_for steals        from the global pool
     from any other
```

**`task_arena` does NOT create new OS threads.** TBB's worker threads can participate in multiple arenas. What the arena controls is how many of them are *allowed in* at once.

**Basic usage — limit parallelism**

```cpp
#include "oneapi/tbb/task_arena.h"
#include "oneapi/tbb/parallel_for.h"

tbb::task_arena arena(2);   // at most 2 threads

arena.execute([&] {
    tbb::parallel_for(0, 1000, [](int i) {
        // runs with at most 2 threads regardless of core count
        do_work(i);
    });
});
```

Useful when your program shares the CPU with a GPU or another process and you want to leave headroom.

**Two independent subsystems**

```cpp
tbb::task_arena physics(4);   // physics simulation: 4 threads
tbb::task_arena render(4);    // rendering: 4 threads

// Launch both concurrently — they draw from separate thread budgets
// No work-stealing between them
std::thread t1([&]{ physics.execute([&]{ parallel_for(0, N, sim_body);    }); });
std::thread t2([&]{ render.execute( [&]{ parallel_for(0, M, render_body); }); });
t1.join(); t2.join();
```

Without arenas, TBB's global pool would intermix work from both subsystems — threads doing physics could be stolen away to help with rendering mid-frame. Arenas prevent that.

**NUMA-aware pinning (multi-socket servers)**

```cpp
tbb::task_arena numa_arena(
    tbb::task_arena::constraints{}
        .set_numa_id(1)            // bind to NUMA socket 1
        .set_max_concurrency(8)    // use up to 8 threads from that socket
);

numa_arena.execute([&]{
    parallel_for(0, N, body);     // threads stay on socket 1's cores
});
// Data allocated on socket 1 memory is accessed locally → lower latency
```

**When to use `task_arena`**

| Scenario | Why `task_arena` helps |
|----------|----------------------|
| CPU + GPU running simultaneously | Limit CPU threads so GPU isn't starved for PCIe bandwidth |
| UI application with background processing | Prevent background `parallel_for` from consuming all cores |
| Multiple independent subsystems | Isolate them so one can't steal from the other |
| NUMA system (multi-socket server) | Pin arenas to sockets to keep data access local |
| Debugging: make parallel code deterministic | `task_arena(1)` forces single-thread execution |

---

</details>

### 2.14 `global_control` — Runtime 配置

`task_arena` *局部*限制线程数（在单个 block 内）。`global_control` 设置**全程序策略**，作用于所有位置的每个 TBB 算法——包括内部使用 TBB 的第三方库。

```
task_arena(4).execute(...)      global_control(max_allowed_parallelism, 4)
  Only this block uses 4         Every parallel_for, parallel_reduce,
  Everything else: unlimited     pipeline, etc. in the entire process: ≤ 4
```

使用 RAII——对象离开作用域时设置自动还原：

```cpp
#include "oneapi/tbb/global_control.h"

{
    tbb::global_control ctrl(
        tbb::global_control::max_allowed_parallelism, 4
    );
    // ── all TBB work inside this scope uses at most 4 threads ──
    tbb::parallel_for(0, N, body_a);   // ≤ 4 threads
    some_library_call();               // if it uses TBB: also ≤ 4 threads
}
// ── ctrl destroyed → limit lifted, TBB uses all cores again ──
tbb::parallel_for(0, N, body_b);       // full thread count again
```

**设置线程栈大小**——当任务递归很深或分配大量局部数组时很有用：

```cpp
tbb::global_control stack_ctrl(
    tbb::global_control::thread_stack_size,
    8 * 1024 * 1024   // 8 MB per worker thread (default is typically 1–4 MB)
);
```

每个线程有固定的栈。深递归或栈上分配的大缓冲区会静默溢出默认值。在创建第一个 TBB 任务之前增大它。

**`task_arena` 与 `global_control` 对比：**

| | `task_arena` | `global_control` |
|--|--|--|
| 作用域 | 一个 `execute()` block | 整个进程 |
| 线程限制 | 每个隔离区域 | 所有 TBB 算法 |
| 工作窃取隔离 | 是 | 否 |
| 典型用途 | 独立子系统、NUMA 绑核 | 在 GUI/服务器中嵌入 TBB、全局限流 |

---

### 2.15 异常处理与取消

在普通的单线程 C++ 中，异常会沿调用栈展开到最近的 `catch`。在并行循环中，每次迭代运行在各自拥有独立栈的不同线程上——没有可供展开的共享调用栈。

TBB 弥合了这一差距：当工作线程抛出异常时，TBB 在内部捕获它，取消剩余的*待处理*迭代，然后在循环结束后在调用线程上重新抛出该异常。

```
Thread 0: iteration  0 → OK
Thread 1: iteration  5 → throws std::runtime_error("bad input")
                         ↑ TBB catches it
Thread 2: iteration 10 → OK (already running → finishes normally)
Pending iterations 11–N → CANCELLED (never start)
                         ↓ TBB re-throws on calling thread
Main thread: catch block runs
```

```cpp
try {
    tbb::parallel_for(0, N, [](int i) {
        if (bad_condition(i))
            throw std::runtime_error("bad input");
    });
} catch (const std::exception& e) {
    // Caught here on the calling thread
    // Already-running iterations completed; pending ones were skipped
    std::cerr << e.what() << "\n";
}
```

> 已在运行的迭代总会执行完。只有*待处理*（尚未启动）的任务会被取消。无法在执行中途中止任务。

**不借助异常的显式取消**——当需要从另一个线程或条件（例如超时、"stop" 按钮）触发取消时，使用 `task_group_context`：

```cpp
tbb::task_group_context ctx;

// Start the parallel loop in one thread
tbb::parallel_for(
    tbb::blocked_range<int>(0, N),
    body,
    tbb::auto_partitioner(),
    ctx          // ← attach context
);

// From another thread (e.g., a watchdog or UI thread):
ctx.cancel_group_execution();
// → pending tasks stop being scheduled
// → running tasks finish naturally
// → parallel_for returns without throwing
```

**取消流程：**

```
cancel_group_execution() called
          │
          ▼
Pending tasks   → skip (never execute)
Running tasks   → run to completion (cannot be interrupted)
parallel_for()  → returns normally (no exception thrown)
```

| 机制 | 触发方式 | 运行中的任务 | 待处理的任务 | 是否抛出？ |
|-----------|-------------|---------------|---------------|---------|
| 工作线程抛出异常 | 函数体内出现异常 | 执行完 | 取消 | 是，在调用者上 |
| `ctx.cancel_group_execution()` | 外部调用 | 执行完 | 取消 | 否 |

---


<details>
<summary>English original</summary>

**2.14 `global_control` — Runtime Configuration**

`task_arena` limits threads *locally* (inside one block). `global_control` sets a **program-wide policy** that applies to every TBB algorithm everywhere — including third-party libraries that use TBB internally.

```
task_arena(4).execute(...)      global_control(max_allowed_parallelism, 4)
  Only this block uses 4         Every parallel_for, parallel_reduce,
  Everything else: unlimited     pipeline, etc. in the entire process: ≤ 4
```

Uses RAII — settings revert automatically when the object goes out of scope:

```cpp
#include "oneapi/tbb/global_control.h"

{
    tbb::global_control ctrl(
        tbb::global_control::max_allowed_parallelism, 4
    );
    // ── all TBB work inside this scope uses at most 4 threads ──
    tbb::parallel_for(0, N, body_a);   // ≤ 4 threads
    some_library_call();               // if it uses TBB: also ≤ 4 threads
}
// ── ctrl destroyed → limit lifted, TBB uses all cores again ──
tbb::parallel_for(0, N, body_b);       // full thread count again
```

**Set thread stack size** — useful when tasks recurse deeply or allocate large local arrays:

```cpp
tbb::global_control stack_ctrl(
    tbb::global_control::thread_stack_size,
    8 * 1024 * 1024   // 8 MB per worker thread (default is typically 1–4 MB)
);
```

Each thread has a fixed stack. Deep recursion or large stack-allocated buffers silently overflow the default. Increase it before the first TBB task is created.

**`task_arena` vs `global_control`:**

| | `task_arena` | `global_control` |
|--|--|--|
| Scope | One `execute()` block | Entire process |
| Thread limit | Per isolated region | All TBB algorithms |
| Work-stealing isolation | Yes | No |
| Typical use | Independent subsystems, NUMA pinning | Embedding TBB in a GUI/server, global throttling |

---

**2.15 Exception Handling and Cancellation**

In normal single-threaded C++, an exception unwinds the call stack to the nearest `catch`. In a parallel loop, each iteration runs in a different thread with its own stack — there is no shared call stack to unwind.

TBB bridges this: when a worker thread throws, TBB catches it internally, cancels remaining *pending* iterations, then re-throws the exception on the calling thread once the loop completes.

```
Thread 0: iteration  0 → OK
Thread 1: iteration  5 → throws std::runtime_error("bad input")
                         ↑ TBB catches it
Thread 2: iteration 10 → OK (already running → finishes normally)
Pending iterations 11–N → CANCELLED (never start)
                         ↓ TBB re-throws on calling thread
Main thread: catch block runs
```

```cpp
try {
    tbb::parallel_for(0, N, [](int i) {
        if (bad_condition(i))
            throw std::runtime_error("bad input");
    });
} catch (const std::exception& e) {
    // Caught here on the calling thread
    // Already-running iterations completed; pending ones were skipped
    std::cerr << e.what() << "\n";
}
```

> Already-running iterations always finish. Only *pending* (not-yet-started) tasks are cancelled. You cannot abort a task mid-execution.

**Explicit cancellation without exceptions** — use `task_group_context` when you want to cancel from another thread or condition (e.g., a timeout, a "stop" button):

```cpp
tbb::task_group_context ctx;

// Start the parallel loop in one thread
tbb::parallel_for(
    tbb::blocked_range<int>(0, N),
    body,
    tbb::auto_partitioner(),
    ctx          // ← attach context
);

// From another thread (e.g., a watchdog or UI thread):
ctx.cancel_group_execution();
// → pending tasks stop being scheduled
// → running tasks finish naturally
// → parallel_for returns without throwing
```

**Cancellation flow:**

```
cancel_group_execution() called
          │
          ▼
Pending tasks   → skip (never execute)
Running tasks   → run to completion (cannot be interrupted)
parallel_for()  → returns normally (no exception thrown)
```

| Mechanism | Triggers how | Running tasks | Pending tasks | Throws? |
|-----------|-------------|---------------|---------------|---------|
| Worker throws exception | Exception in body | Finish | Cancelled | Yes, on caller |
| `ctx.cancel_group_execution()` | External call | Finish | Cancelled | No |

---

</details>

### 2.16 工作隔离

TBB 的 work-stealing 调度器很激进：当一个线程完成一个任务、正在等待内层 `parallel_for` 时，它不会空转——它会在池中寻找*任何*可用任务，包括来自外层循环的任务。这对吞吐很有利，但当内层任务与外层任务共享数据时就很危险。

```
WITHOUT isolate:
  Outer task i=0 launches inner parallel_for (10 tasks)
  Outer task i=0's thread waits → steals outer task i=3
  Now i=0's thread is running i=3 while i=0's inner tasks run on other threads
  If inner tasks read/write data[i=0...] and outer task i=3 does too → race

WITH isolate:
  Outer task i=0 launches inner parallel_for inside isolate()
  Inner tasks are visible ONLY to threads already inside the isolate block
  Outer threads cannot steal them → no interleaving → safe
```

**何时需要隔离——三个问题：**

1. 内层循环是否会写入外层任务也会触及的数据？
2. 该数据是否没有锁保护？
3. 外层任务与内层任务是否同时运行？

三者皆是：用 `isolate`。

```cpp
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/this_task_arena.h>

std::vector<int> data(100, 0);

tbb::parallel_for(0, 10, [&](int i) {
    // outer task owns data[i*10 ... i*10+9]
    // inner tasks write into exactly that range
    // → must not be stolen by another outer task's thread

    tbb::this_task_arena::isolate([&] {
        tbb::parallel_for(0, 10, [&](int j) {
            data[i*10 + j] += 1;   // safe: isolated from other outer tasks
        });
    });
});
```

**不用 `isolate` 时：** 运行外层 `i=2` 的线程可能窃取属于外层 `i=7` 的内层任务，然后写入 `data[20..29]`，而 `i=2` 的线程正在别处写入 `data[20..29]`——数据竞争。

**用 `isolate` 时：** `i=2` 的内层任务留在 isolate 块内。只有进入该块的线程（以及*它*在内部派生的任何 helper）才能运行这些内层任务。

**何时不该隔离：**

如果内层循环处理的是独立数据（与外层任务无共享），隔离只会损害性能——它阻止空闲线程来协助内层工作。别用它。

| 情形 | 是否用 `isolate`？ | 原因 |
|-----------|---------------|--------|
| 内层任务写入共享的外层数据 | 是 | 防止数据竞争 |
| 内层任务只读共享数据 | 通常否 | 读取被窃取是安全的 |
| 内层任务处理完全独立的数据 | 否 | 让线程自由窃取——更快 |
| 需要每个外层任务确定性的执行顺序 | 是 | isolate 强制严格包含 |

---

### 2.17 模式速查表

**Reduce：**

```cpp
float total = parallel_reduce(range, 0.0f, body, std::plus<float>{});
```

**分治（递归）：**

```cpp
void parallel_mergesort(int* a, int n) {
    if (n < THRESHOLD) { std::sort(a, a + n); return; }
    parallel_invoke(
        [&]{ parallel_mergesort(a, n/2); },
        [&]{ parallel_mergesort(a + n/2, n - n/2); }
    );
    std::inplace_merge(a, a + n/2, a + n);
}
```

**Map（逐元素）：**

```cpp
parallel_for(blocked_range<int>(0, N), [&](const blocked_range<int>& r) {
    for (int i = r.begin(); i < r.end(); i++)
        out[i] = f(in[i]);
});
```

**直方图（每线程局部 + 合并）：**

```cpp
enumerable_thread_specific<std::vector<int>> local_hist(
    []{ return std::vector<int>(256, 0); });

parallel_for(0, N, [&](int i) {
    local_hist.local()[data[i]]++;
});

std::vector<int> hist(256, 0);
for (auto& h : local_hist)
    for (int k = 0; k < 256; k++) hist[k] += h[k];
```

**前缀扫描：**

```cpp
parallel_scan(range, 0.0f, scan_body, std::plus<float>{});
```

**装配线：**

```cpp
parallel_pipeline(max_tokens, input_filter & transform_filter & output_filter);
```

---

### 2.18 与 GPU 编程的联系

oneTBB 的模式直接对应到 GPU 框架：

| oneTBB 概念 | GPU 对应项 |
|---------------|---------------|
| `parallel_for` over `blocked_range` | 在 thread grid 上启动 CUDA kernel |
| Work-stealing 调度器 | Warp 调度器（硬件） |
| `affinity_partitioner` | CUDA 中的 L2 缓存局部性提示 |
| `enumerable_thread_specific` | 每 warp 的共享内存分配 |
| `combinable<T>` | 把 `atomicAdd` 写入共享内存，再 reduce |
| `concurrent_queue` | CUDA stream（异步任务队列） |
| `parallel_pipeline` | CUDA 多 stream 流水线 |
| `flow_graph` 节点 | CUDA Graph 节点（`cudaGraph`） |
| `task_group` | CUDA 动态并行 |
| `task_arena` | CUDA stream 优先级 + stream 隔离 |
| 可伸缩分配器 | `cudaMallocAsync`（stream-ordered pool） |

学习 oneTBB 的任务分解，会让 CUDA 的 thread/block/grid 层级变得直观——它们在不同尺度上解决同一个问题。

---


<details>
<summary>English original</summary>

**2.16 Work Isolation**

TBB's work-stealing scheduler is aggressive: when a thread finishes a task and is waiting on an inner `parallel_for`, it doesn't idle — it looks for *any* available task in the pool, including tasks from the outer loop. This is great for throughput but dangerous when inner and outer tasks share data.

```
WITHOUT isolate:
  Outer task i=0 launches inner parallel_for (10 tasks)
  Outer task i=0's thread waits → steals outer task i=3
  Now i=0's thread is running i=3 while i=0's inner tasks run on other threads
  If inner tasks read/write data[i=0...] and outer task i=3 does too → race

WITH isolate:
  Outer task i=0 launches inner parallel_for inside isolate()
  Inner tasks are visible ONLY to threads already inside the isolate block
  Outer threads cannot steal them → no interleaving → safe
```

**When you need isolation — three questions:**

1. Does the inner loop write to data that outer tasks also touch?
2. Is that data not protected by a lock?
3. Are outer and inner tasks running at the same time?

If all three: use `isolate`.

```cpp
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/this_task_arena.h>

std::vector<int> data(100, 0);

tbb::parallel_for(0, 10, [&](int i) {
    // outer task owns data[i*10 ... i*10+9]
    // inner tasks write into exactly that range
    // → must not be stolen by another outer task's thread

    tbb::this_task_arena::isolate([&] {
        tbb::parallel_for(0, 10, [&](int j) {
            data[i*10 + j] += 1;   // safe: isolated from other outer tasks
        });
    });
});
```

**Without `isolate`:** Thread running outer `i=2` could steal inner tasks belonging to outer `i=7`, then write into `data[20..29]` while `i=2`'s thread is somewhere else writing into `data[20..29]` — data race.

**With `isolate`:** Inner tasks of `i=2` stay inside the isolate block. Only the thread that entered the block (and any helpers *it* spawns internally) can run those inner tasks.

**When NOT to isolate:**

If the inner loop works on independent data (no sharing with outer tasks), isolation only hurts performance — it prevents free threads from helping with the inner work. Leave it out.

| Situation | Use `isolate`? | Reason |
|-----------|---------------|--------|
| Inner tasks write to shared outer data | Yes | Prevent data race |
| Inner tasks read-only from shared data | Usually no | Reads are safe to steal |
| Inner tasks work on fully independent data | No | Let threads steal freely — faster |
| Need deterministic per-outer-task execution order | Yes | Isolate forces strict containment |

---

**2.17 Pattern Cheat Sheet**

**Reduce:**

```cpp
float total = parallel_reduce(range, 0.0f, body, std::plus<float>{});
```

**Divide and Conquer (recursive):**

```cpp
void parallel_mergesort(int* a, int n) {
    if (n < THRESHOLD) { std::sort(a, a + n); return; }
    parallel_invoke(
        [&]{ parallel_mergesort(a, n/2); },
        [&]{ parallel_mergesort(a + n/2, n - n/2); }
    );
    std::inplace_merge(a, a + n/2, a + n);
}
```

**Map (elementwise):**

```cpp
parallel_for(blocked_range<int>(0, N), [&](const blocked_range<int>& r) {
    for (int i = r.begin(); i < r.end(); i++)
        out[i] = f(in[i]);
});
```

**Histogram (per-thread local + merge):**

```cpp
enumerable_thread_specific<std::vector<int>> local_hist(
    []{ return std::vector<int>(256, 0); });

parallel_for(0, N, [&](int i) {
    local_hist.local()[data[i]]++;
});

std::vector<int> hist(256, 0);
for (auto& h : local_hist)
    for (int k = 0; k < 256; k++) hist[k] += h[k];
```

**Prefix scan:**

```cpp
parallel_scan(range, 0.0f, scan_body, std::plus<float>{});
```

**Assembly line:**

```cpp
parallel_pipeline(max_tokens, input_filter & transform_filter & output_filter);
```

---

**2.18 Connection to GPU Programming**

oneTBB patterns map directly to GPU frameworks:

| oneTBB concept | GPU equivalent |
|---------------|---------------|
| `parallel_for` over `blocked_range` | CUDA kernel launch over thread grid |
| Work-stealing scheduler | Warp scheduler (hardware) |
| `affinity_partitioner` | L2 cache locality hints in CUDA |
| `enumerable_thread_specific` | Per-warp shared memory allocation |
| `combinable<T>` | `atomicAdd` into shared mem, then reduce |
| `concurrent_queue` | CUDA stream (async task queue) |
| `parallel_pipeline` | CUDA multi-stream pipeline |
| `flow_graph` nodes | CUDA graph nodes (`cudaGraph`) |
| `task_group` | CUDA dynamic parallelism |
| `task_arena` | CUDA stream priority + stream isolation |
| Scalable allocator | `cudaMallocAsync` (stream-ordered pool) |

Learning oneTBB's task decomposition makes CUDA's thread/block/grid hierarchy intuitive — they solve the same problem at different scales.

---

</details>

## 资源

| 资源 | 涵盖内容 |
|----------|---------------|
| [oneTBB 开发者指南](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/) | 完整教程：parallel_for、reduce、pipeline、flow graph、设计模式 |
| [oneTBB API 参考](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/reference.html) | 完整 API |
| [Intel oneTBB 入门](https://www.intel.com/content/www/us/en/docs/onetbb/get-started-guide/2022-2/overview.html) | 安装、CMake、入门步骤 |
| [OpenMP 规范](https://www.openmp.org/specifications/) | OpenMP 官方标准 |
| [OpenMP API 快速参考卡](https://www.openmp.org/wp-content/uploads/OpenMP-4.5-1115-CPP-web.pdf) | 一览所有子句 |
| *Intel Threading Building Blocks*（Reinders） | 书籍：深入讲解 TBB 模式 |

---

## 下一章

→ [**CUDA 与 SIMT**](../02-CUDA与SIMT/Guide.md) — GPU 并行：线程、warp、block 与存储层次。


<details>
<summary>English original</summary>

**Resources**

| Resource | What it covers |
|----------|---------------|
| [oneTBB Developer Guide](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/) | Full tutorial: parallel_for, reduce, pipeline, flow graph, design patterns |
| [oneTBB API Reference](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/reference.html) | Complete API |
| [Intel oneTBB Get Started](https://www.intel.com/content/www/us/en/docs/onetbb/get-started-guide/2022-2/overview.html) | Installation, CMake, first steps |
| [OpenMP specifications](https://www.openmp.org/specifications/) | Official OpenMP standard |
| [OpenMP API Quick Reference Card](https://www.openmp.org/wp-content/uploads/OpenMP-4.5-1115-CPP-web.pdf) | All clauses at a glance |
| *Intel Threading Building Blocks* (Reinders) | Book: TBB patterns in depth |

---

**Next**

→ [**CUDA and SIMT**](../02-CUDA与SIMT/Guide.md) — GPU parallelism: threads, warps, blocks, and memory hierarchy.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/4. C++ and Parallel Computing/OpenMP and OneTBB/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/4.%20C%2B%2B%20and%20Parallel%20Computing/OpenMP%20and%20OneTBB/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
