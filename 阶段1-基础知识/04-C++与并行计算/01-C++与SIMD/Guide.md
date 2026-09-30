---
title: C++ 与 SIMD（阶段 1 §4 —— Sub-Track 1）
description: C++ 与 SIMD（阶段 1 §4 —— Sub-Track 1）
---

# C++ 与 SIMD（阶段 1 §4 —— Sub-Track 1）

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">ASPS</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入探索 · 数字基础</p>
<p class="course-identity__title">C++ 与 SIMD（阶段 1 §4 —— Sub-Track 1）的专属课程标识。</p>
<p class="course-identity__meta">产物：可运行的低层 demo · 度量：时序、内存、正确性</p>
</div>
</div>


**父课程：** [C++ 与并行计算](../Guide.md)

> *现代 C++ 让你既有高层语法，又有低层性能。先掌握这些语言特性 —— 它们是后续每一个并行框架的构建块。*

**前置要求：** 基础 C 编程（来自阶段 1 §3 的 OS 部分）。熟悉指针、数组、函数。

---

## 为什么先学现代 C++

本课程中的每一项并行计算技术 —— SIMD intrinsics、OpenMP、oneTBB、CUDA、HIP、SYCL —— 都建立在 C++ 之上。但不是 1990 年代的 C++。现代 C++（C++11 到 C++17）引入的特性专门为性能、安全性与并行编程而设计：

- **Lambda** → 用于 oneTBB、SYCL、现代 CUDA、parallel STL
- **移动语义** → 零拷贝数据传输，对 GPU 内存至关重要
- **模板** → 泛型 CUDA kernel、CUTLASS、CuTe
- **Parallel STL** → 一行代码内建 SIMD + 线程化
- **`constexpr`** → 面向 HPC 优化的编译期计算

现在就学这些特性。Sub-Track 2–5 中每一个都会用到。

---

## 第 1 节：面向并行计算的现代 C++17

### 1.1 类型推导（`auto`、`decltype`）

**`auto` —— 让编译器自己判断类型：**

```cpp
auto x = 10;          // int
auto y = 3.14;        // double
auto z = vec.begin(); // std::vector<int>::iterator — much cleaner
```

`auto` 减少冗长写法并避免类型不匹配的 bug。把它用于局部变量，尤其是复杂类型（迭代器、模板结果）。

---

#### `decltype` 解决的问题

在一个泛型乘法函数中，返回类型是什么？

```cpp
template<typename T, typename U>
??? multiply(T a, U b) {
    return a * b;
}
// int   * int    → int
// int   * double → double
// float * double → double
```

**C++11 之前的变通做法 —— 两种都不够用：**

```cpp
// Option 1: force a type — loses precision, not generic
template<typename T, typename U>
double multiply(T a, U b) { return a * b; }

// Option 2: std::common_type — verbose, breaks for custom operators
template<typename T, typename U>
typename std::common_type<T, U>::type multiply(T a, U b) { return a * b; }
```

**C++11 的解法 —— 配合 `decltype` 的尾置返回类型：**

```cpp
template<typename T, typename U>
auto multiply(T a, U b) -> decltype(a * b) {
    return a * b;
}
```

编译器推导出 **表达式 `a * b` 的确切类型** —— 对内置类型、用户自定义运算符以及任意复杂的模板表达式都成立。

**C++14 的简化 —— 从 return 语句推导 `decltype`：**

```cpp
template<typename T, typename U>
auto multiply(T a, U b) {   // return type deduced automatically
    return a * b;
}
```

不需要尾置 `-> decltype(...)`。编译器从 `return` 表达式推导。

---

#### `auto` 与 `decltype` —— 关键区别

`auto` 会剥掉引用和 `const`。`decltype` 保留确切的类型语义。

```cpp
int x = 5;
int& ref = x;

auto        a = ref;  // a is int   — reference stripped
decltype(ref) b = x;  // b is int&  — reference preserved

const int c = 10;
auto        d = c;    // d is int        — const stripped
decltype(c) e = c;    // e is const int  — const preserved
```

**经验法则：**
- 90% 的场合用 `auto`（局部变量、循环迭代器、lambda 捕获）
- 需要**确切的类型语义**时用 `decltype`（模板返回类型、转发、trait 风格代码）

---

#### 对比：旧式 vs 现代

| | C++11 之前 | C++11 `decltype` | C++14 `auto` return |
|---|---|---|---|
| 返回类型推导 | 手工，易出错 | 精确的表达式类型 | 由 `return` 推导 |
| 泛型正确性 | 有限 | 精确 | 精确 |
| 自定义运算符支持 | 困难 | 支持 | 支持 |
| 可读性 | 冗长 | 简洁 | 最简洁 |

---

**这对并行计算为何重要：** CUTLASS、CuTe、oneAPI DPC++ 和 ROCm 都大量使用 `auto` + `decltype`，原因在于：
- kernel 按精度做模板化（`float`、`half`、`int8_t`）
- 中间表达式的类型取决于模板参数
- `decltype` 让库能够表达「`a*b` 产出的任意类型」，而无需硬编码

在整个 CUDA 模板库中，你都会看到 `decltype(auto)`、`std::declval<T>()` 以及尾置返回类型这类写法 —— 现在就认出它们，能省下之后大量的困惑。

---


<details>
<summary>English original</summary>

**C++ and SIMD (Phase 1 §4 — Sub-Track 1)**

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">ASPS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Digital Foundations</p>
<p class="course-identity__title">Specialized course identity for C++ and SIMD (Phase 1 §4 — Sub-Track 1).</p>
<p class="course-identity__meta">Artifact: working low-level demo · Measure: timing, memory, correctness</p>
</div>
</div>


**Parent:** [C++ and Parallel Computing](../Guide.md)

> *Modern C++ gives you high-level syntax with low-level performance. Master the language features first — they are the building blocks of every parallel framework that follows.*

**Prerequisites:** Basic C programming (from Phase 1 §3 OS work). Familiarity with pointers, arrays, functions.

---

**Why Modern C++ First**

Every parallel computing technology in this curriculum — SIMD intrinsics, OpenMP, oneTBB, CUDA, HIP, SYCL — is built on C++. But not the C++ of the 1990s. Modern C++ (C++11 through C++17) introduced features specifically designed for performance, safety, and parallel programming:

- **Lambdas** → used in oneTBB, SYCL, modern CUDA, parallel STL
- **Move semantics** → zero-copy data transfer, critical for GPU memory
- **Templates** → generic CUDA kernels, CUTLASS, CuTe
- **Parallel STL** → built-in SIMD + threading with one line of code
- **`constexpr`** → compile-time computation for HPC optimization

Learn these features now. You'll use every single one in Sub-Tracks 2–5.

---

**Section 1: Modern C++17 for Parallel Computing**

**1.1 Type Deduction (`auto`, `decltype`)**

**`auto` — let the compiler figure out the type:**

```cpp
auto x = 10;          // int
auto y = 3.14;        // double
auto z = vec.begin(); // std::vector<int>::iterator — much cleaner
```

`auto` reduces verbosity and prevents type mismatch bugs. Use it for local variables, especially with complex types (iterators, template results).

---

**The problem `decltype` solves**

In a generic multiply function, what is the return type?

```cpp
template<typename T, typename U>
??? multiply(T a, U b) {
    return a * b;
}
// int   * int    → int
// int   * double → double
// float * double → double
```

**Pre-C++11 workarounds — both inadequate:**

```cpp
// Option 1: force a type — loses precision, not generic
template<typename T, typename U>
double multiply(T a, U b) { return a * b; }

// Option 2: std::common_type — verbose, breaks for custom operators
template<typename T, typename U>
typename std::common_type<T, U>::type multiply(T a, U b) { return a * b; }
```

**C++11 solution — trailing return type with `decltype`:**

```cpp
template<typename T, typename U>
auto multiply(T a, U b) -> decltype(a * b) {
    return a * b;
}
```

The compiler deduces the **exact type of the expression `a * b`** — works for built-in types, user-defined operators, and arbitrarily complex template expressions.

**C++14 simplification — `decltype` inferred from the return statement:**

```cpp
template<typename T, typename U>
auto multiply(T a, U b) {   // return type deduced automatically
    return a * b;
}
```

No trailing `-> decltype(...)` needed. The compiler infers from the `return` expression.

---

**`auto` vs `decltype` — the key difference**

`auto` strips references and `const`. `decltype` preserves exact type semantics.

```cpp
int x = 5;
int& ref = x;

auto        a = ref;  // a is int   — reference stripped
decltype(ref) b = x;  // b is int&  — reference preserved

const int c = 10;
auto        d = c;    // d is int        — const stripped
decltype(c) e = c;    // e is const int  — const preserved
```

**Rule of thumb:**
- Use `auto` → 90% of the time (local variables, loop iterators, lambda captures)
- Use `decltype` → when you need **exact type semantics** (template return types, forwarding, trait-style code)

---

**Comparison: old vs modern**

| | Pre-C++11 | C++11 `decltype` | C++14 `auto` return |
|---|---|---|---|
| Return type deduction | Manual, error-prone | Exact expression type | Inferred from `return` |
| Generic correctness | Limited | Exact | Exact |
| Custom operator support | Hard | Yes | Yes |
| Readability | Verbose | Clean | Cleanest |

---

**Why it matters for parallel computing:** CUTLASS, CuTe, oneAPI DPC++, and ROCm all use `auto` + `decltype` extensively because:
- Kernels are templated on precision (`float`, `half`, `int8_t`)
- Types of intermediate expressions depend on template parameters
- `decltype` lets the library express "whatever type `a*b` produces" without hardcoding it

You will see patterns like `decltype(auto)`, `std::declval<T>()`, and trailing return types throughout CUDA template libraries — recognizing them now saves significant confusion later.

---

</details>

### 1.2 智能指针（通过 RAII 实现内存安全）

**原始指针的问题：**

```cpp
int* p = new int(5);
// ... 200 lines of code ...
// Did you remember to delete p? Memory leak.
// Did you delete it twice? Undefined behavior.
```

**解决方案 — RAII（Resource Acquisition Is Initialization，资源获取即初始化）：**

```cpp
#include <memory>

// Exclusive ownership — automatically freed when scope ends
auto p = std::make_unique<int>(5);
// No delete needed. Ever.

// Shared ownership — freed when last reference dies
auto q = std::make_shared<int>(10);
auto r = q;  // Reference count = 2
// Freed when both q and r go out of scope
```

**三种智能指针类型：**

| 类型 | 所有权 | 开销 | 使用场景 |
|------|-----------|----------|----------|
| `std::unique_ptr` | 独占（单一所有者） | 零（与原始指针相同） | 默认选择。单一所有者。 |
| `std::shared_ptr` | 共享（引用计数） | 原子引用计数（控制块开销，依赖实现） | 需要多个所有者 |
| `std::weak_ptr` | 非拥有观察者 | 极小 | 打破循环引用 |

**规则：** 现代 C++ 中绝不使用 `new`/`delete`。使用 `make_unique` 和 `make_shared`。

---

#### 用 `std::weak_ptr` 打破循环

`shared_ptr` 使用引用计数。只有当**没有任何 `shared_ptr` 指向该对象**时，计数才会归零 —— 内存才会被释放。如果两个对象互相持有对方的 `shared_ptr`，两者的计数都永远不会归零。这就是**引用循环**，它会静默地永久泄漏内存。

**循环问题：**

```cpp
struct Node {
    std::shared_ptr<Node> next;   // strong reference
    int value;
};

auto a = std::make_shared<Node>();  // a: refcount = 1
auto b = std::make_shared<Node>();  // b: refcount = 1

a->next = b;   // b refcount = 2
b->next = a;   // a refcount = 2

// a and b go out of scope:
//   a's refcount drops to 1 (b->next still holds it)
//   b's refcount drops to 1 (a->next still holds it)
//   Neither reaches 0 → MEMORY LEAK
```

**修复方法 —— 对回边使用 `weak_ptr`：**

`weak_ptr` 观察一个 `shared_ptr`，**且不增加引用计数**。被观察的对象仍可正常销毁。使用 `weak_ptr` 之前，必须先对其调用 `lock()` —— 这会返回一个 `shared_ptr`，它要么有效（对象存活），要么为空（对象已被销毁）。

```cpp
struct Node {
    std::shared_ptr<Node> next;   // strong: owns the next node
    std::weak_ptr<Node>   prev;   // weak: observes without owning
    int value;
};

auto a = std::make_shared<Node>();  // a: refcount = 1
auto b = std::make_shared<Node>();  // b: refcount = 1

a->next = b;        // b refcount = 2  (strong)
b->prev = a;        // a refcount still = 1  (weak — no increment)

// a and b go out of scope:
//   a's refcount drops to 0 → destroyed → a->next released
//   b's refcount drops to 0 → destroyed
//   No leak.

// Accessing through weak_ptr — always check if still alive:
if (auto owner = b->prev.lock()) {   // lock() returns shared_ptr or nullptr
    std::cout << "prev value: " << owner->value << "\n";
} else {
    std::cout << "prev was destroyed\n";
}
```

**实践中循环出现的场景：**

| 模式 | 循环边 | 修复 |
|---------|------------|-----|
| 双向链表 | `prev` 指针 | 将 `prev` 作为 `weak_ptr` |
| 带父指针的树 | `parent` 指针 | 将 `parent` 作为 `weak_ptr` |
| 观察者 / 事件系统 | 订阅者持有对发布者的引用 | 订阅者存储指向发布者的 `weak_ptr` |
| 计算图（ML） | 从输出节点到输入节点的回边 | 将回边作为 `weak_ptr` |

**`weak_ptr` API：**

```cpp
auto sp = std::make_shared<int>(42);
std::weak_ptr<int> wp = sp;          // observe, no refcount increment

wp.expired();                        // true if the object was destroyed
wp.use_count();                      // same as sp.use_count() (0 if expired)
auto sp2 = wp.lock();                // returns shared_ptr<int> or nullptr — ALWAYS use this
if (sp2) { /* safe to use */ }
```

> **不要直接解引用 `weak_ptr`。** 始终调用 `.lock()` 并检查结果。对象可能在 `expired()` 返回 `false` 到你下一行代码之间被销毁。

---

**为什么它对并行计算很重要：**
- GPU 内存（`cudaMalloc`/`cudaFree`）遵循同样的 RAII 模式 —— 用智能指针或自定义 RAII 类包装
- 线程安全的引用计数（`shared_ptr`）对多线程数据共享至关重要
- 计算图（PyTorch autograd、JAX jaxpr）是带循环的图 —— 真实实现使用弱回边来避免内存泄漏
- HPC 中的内存泄漏 = 你的 80 GB GPU 在训练中途耗尽内存

---


<details>
<summary>English original</summary>

**1.2 Smart Pointers (Memory Safety via RAII)**

**The problem with raw pointers:**

```cpp
int* p = new int(5);
// ... 200 lines of code ...
// Did you remember to delete p? Memory leak.
// Did you delete it twice? Undefined behavior.
```

**The solution — RAII (Resource Acquisition Is Initialization):**

```cpp
#include <memory>

// Exclusive ownership — automatically freed when scope ends
auto p = std::make_unique<int>(5);
// No delete needed. Ever.

// Shared ownership — freed when last reference dies
auto q = std::make_shared<int>(10);
auto r = q;  // Reference count = 2
// Freed when both q and r go out of scope
```

**Three smart pointer types:**

| Type | Ownership | Overhead | Use when |
|------|-----------|----------|----------|
| `std::unique_ptr` | Exclusive (one owner) | Zero (same as raw pointer) | Default choice. Single owner. |
| `std::shared_ptr` | Shared (reference counted) | Atomic ref count (control block overhead, impl-dependent) | Multiple owners needed |
| `std::weak_ptr` | Non-owning observer | Minimal | Break circular references |

**Rule:** Never use `new`/`delete` in modern C++. Use `make_unique` and `make_shared`.

---

**Breaking Cycles with `std::weak_ptr`**

`shared_ptr` uses reference counting. The count only reaches zero — and the memory only frees — when **no `shared_ptr` points to the object**. If two objects hold `shared_ptr` to each other, neither count ever reaches zero. This is a **reference cycle** and it silently leaks memory forever.

**The cycle problem:**

```cpp
struct Node {
    std::shared_ptr<Node> next;   // strong reference
    int value;
};

auto a = std::make_shared<Node>();  // a: refcount = 1
auto b = std::make_shared<Node>();  // b: refcount = 1

a->next = b;   // b refcount = 2
b->next = a;   // a refcount = 2

// a and b go out of scope:
//   a's refcount drops to 1 (b->next still holds it)
//   b's refcount drops to 1 (a->next still holds it)
//   Neither reaches 0 → MEMORY LEAK
```

**The fix — `weak_ptr` for the back-edge:**

`weak_ptr` observes a `shared_ptr` **without incrementing the reference count**. The observed object can still be destroyed normally. Before using a `weak_ptr`, you must `lock()` it — this returns a `shared_ptr` that is either valid (object alive) or empty (object was destroyed).

```cpp
struct Node {
    std::shared_ptr<Node> next;   // strong: owns the next node
    std::weak_ptr<Node>   prev;   // weak: observes without owning
    int value;
};

auto a = std::make_shared<Node>();  // a: refcount = 1
auto b = std::make_shared<Node>();  // b: refcount = 1

a->next = b;        // b refcount = 2  (strong)
b->prev = a;        // a refcount still = 1  (weak — no increment)

// a and b go out of scope:
//   a's refcount drops to 0 → destroyed → a->next released
//   b's refcount drops to 0 → destroyed
//   No leak.

// Accessing through weak_ptr — always check if still alive:
if (auto owner = b->prev.lock()) {   // lock() returns shared_ptr or nullptr
    std::cout << "prev value: " << owner->value << "\n";
} else {
    std::cout << "prev was destroyed\n";
}
```

**When cycles appear in practice:**

| Pattern | Cycle edge | Fix |
|---------|------------|-----|
| Doubly-linked list | `prev` pointer | `prev` as `weak_ptr` |
| Tree with parent pointer | `parent` pointer | `parent` as `weak_ptr` |
| Observer / event system | Subscriber holds reference to publisher | Subscriber stores `weak_ptr` to publisher |
| Computation graph (ML) | Back-edge from output node to input | Back-edges as `weak_ptr` |

**`weak_ptr` API:**

```cpp
auto sp = std::make_shared<int>(42);
std::weak_ptr<int> wp = sp;          // observe, no refcount increment

wp.expired();                        // true if the object was destroyed
wp.use_count();                      // same as sp.use_count() (0 if expired)
auto sp2 = wp.lock();                // returns shared_ptr<int> or nullptr — ALWAYS use this
if (sp2) { /* safe to use */ }
```

> **Do not dereference a `weak_ptr` directly.** Always call `.lock()` and check the result. The object could be destroyed between `expired()` returning `false` and your next line.

---

**Why it matters for parallel computing:**
- GPU memory (`cudaMalloc`/`cudaFree`) follows the same RAII pattern — wrap in a smart pointer or custom RAII class
- Thread-safe reference counting (`shared_ptr`) is essential for multi-threaded data sharing
- Computation graphs (PyTorch autograd, JAX jaxpr) are graphs with cycles — real implementations use weak back-edges to avoid memory leaks
- Memory leaks in HPC = your 80 GB GPU runs out of memory mid-training

---

</details>

### 1.3 基于范围的循环

```cpp
std::vector<float> data = {1.0, 2.0, 3.0, 4.0};

// Modern (clean, less error-prone)
for (auto& x : data) {
    x *= 2.0f;
}

// Old style (verbose, index bugs)
for (int i = 0; i < data.size(); i++) {
    data[i] *= 2.0f;
}
```

**为什么重要：** 基于范围的循环与 STL 算法和并行 STL（`std::execution::par`）自然配合。编译器也更容易对它们自动向量化。

---

### 1.4 结构化绑定（C++17）

直接解包 tuple、pair 和 struct：

```cpp
// Pair unpacking
std::pair<int, float> p = {1, 2.5f};
auto [id, value] = p;   // id = 1, value = 2.5f

// Map iteration (clean)
std::map<std::string, int> scores = {{"alice", 95}, {"bob", 87}};
for (auto& [name, score] : scores) {
    std::cout << name << ": " << score << "\n";
}

// Function returning multiple values
auto [min_val, max_val] = std::minmax_element(data.begin(), data.end());
```

**为什么重要：** 在性能关键代码中数据处理更简洁。用于性能剖析结果、benchmark 数据、多返回值函数。

---

### 1.5 Lambda 函数（对并行计算至关重要）

Lambda 是**并行编程中最重要的单一 C++ 特性**。每个并行框架都在使用它们。

**语法：**

```
[ captures ] ( parameters ) -> return_type { body }
```
- `captures` → 从外层作用域引入哪些变量
- `parameters` → 像普通函数一样
- `return_type` → 可选，通常可推导
- `body` → 代码块

**基本 lambda：**

```cpp
auto add = [](int a, int b) {
    return a + b;
};

int result = add(3, 4);  // 7
```

---

#### 捕获模式

| 模式 | 含义 | 线程安全？ | 使用场景 |
|------|---------|-------------|---------|
| `[=]` | 所有外层变量按值（拷贝） | 是 | 并行回调的默认选择 |
| `[&]` | 所有外层变量按引用 | 否 | 仅单线程 |
| `[var]` | 指定变量按值 | 是 | 显式，推荐 |
| `[&var]` | 指定变量按引用 | 否 | 只读，单线程 |
| `[=, &var]` | 混合：多数按值，一个按引用 | 部分 | 细粒度控制 |
| `[var = std::move(obj)]` | 将对象移动进 lambda | 是 | 转移所有权（C++14） |

```cpp
int multiplier = 10;

auto scale_copy     = [=](int x)       { return x * multiplier; };  // copy — safe in threads
auto scale_ref      = [&](int x)       { return x * multiplier; };  // ref — dangerous in parallel
auto scale_specific = [multiplier](int x) { return x * multiplier; };  // best practice
```

---

#### 可变 Lambda

默认情况下，按值捕获的值在 lambda 内部是 `const`。使用 `mutable` 修改内部副本，而不影响原变量：

```cpp
int a = 10;

auto f = [a]() mutable {
    a += 5;
    std::cout << a;  // prints 15
};
f();
std::cout << a;  // still 10 — original unchanged
```

`mutable` 修改的是 lambda 的**自身副本**，而不是外层变量。

---

#### 泛型 Lambda（C++14）

使用 `auto` 参数让 lambda 自动模板化——无需显式 `template<>`：

```cpp
auto add = [](auto a, auto b) { return a + b; };

add(3, 4);        // int + int = 7
add(3.14, 2.71);  // double + double = 5.85
add(1.0f, 2.0f);  // float + float
```

这在并行 STL、oneTBB 和 SYCL 中被大量用于编写类型通用的 kernel。

---

#### 线程安全规则

**按值捕获 `[x]`** —— 每个线程获得自己的副本。安全。

**按引用捕获 `[&x]`** —— 所有线程共享同一变量。任意线程写入即产生竞态条件。

```cpp
std::vector<int> data = {1, 2, 3, 4};
int multiplier = 2;

// Safe: each element read is independent, multiplier captured by value
std::for_each(std::execution::par, data.begin(), data.end(),
              [multiplier](int& x) { x *= multiplier; });

// Unsafe: if multiplier were captured by ref and modified by any thread
std::for_each(std::execution::par, data.begin(), data.end(),
              [&](int& x) { x *= multiplier; });  // race condition if multiplier changes
```

> **规则：在并行循环中优先使用 `[=]` 或具名按值捕获。仅在单线程代码中，或当捕获的变量为只读时，才使用 `[&]`。**

---

#### Lambda 生命周期 —— 悬垂引用陷阱

如果 lambda 的生命周期超过其捕获引用的作用域，就会产生悬垂引用：

```cpp
std::function<int()> make_lambda() {
    int local = 42;
    return [&]() { return local; };  // DANGLING — local is destroyed when function returns
}

auto f = make_lambda();
f();  // undefined behavior
```

**修复 1 —— 按值捕获：**

```cpp
return [local]() { return local; };  // safe copy
```

**修复 2 —— 将所有权移动进 lambda（C++14）：**

```cpp
auto buf = std::make_unique<int>(42);
std::thread t([buf = std::move(buf)]() {
    std::cout << *buf;  // safe: ownership moved into lambda
});
t.join();
```

`[buf = std::move(buf)]` 将 `unique_ptr` 的所有权转移进 lambda。lambda 现在拥有该资源——资源无法比 lambda 存活更久。

---


<details>
<summary>English original</summary>

**1.3 Range-Based Loops**

```cpp
std::vector<float> data = {1.0, 2.0, 3.0, 4.0};

// Modern (clean, less error-prone)
for (auto& x : data) {
    x *= 2.0f;
}

// Old style (verbose, index bugs)
for (int i = 0; i < data.size(); i++) {
    data[i] *= 2.0f;
}
```

**Why it matters:** Range-based loops work naturally with STL algorithms and parallel STL (`std::execution::par`). They're also easier for the compiler to auto-vectorize.

---

**1.4 Structured Bindings (C++17)**

Unpack tuples, pairs, and structs directly:

```cpp
// Pair unpacking
std::pair<int, float> p = {1, 2.5f};
auto [id, value] = p;   // id = 1, value = 2.5f

// Map iteration (clean)
std::map<std::string, int> scores = {{"alice", 95}, {"bob", 87}};
for (auto& [name, score] : scores) {
    std::cout << name << ": " << score << "\n";
}

// Function returning multiple values
auto [min_val, max_val] = std::minmax_element(data.begin(), data.end());
```

**Why it matters:** Cleaner data handling in performance-critical code. Used in profiling results, benchmark data, multi-return-value functions.

---

**1.5 Lambda Functions (Critical for Parallel Computing)**

Lambdas are **the single most important C++ feature for parallel programming**. Every parallel framework uses them.

**Syntax:**

```
[ captures ] ( parameters ) -> return_type { body }
```
- `captures` → which variables from the outer scope to bring in
- `parameters` → like a normal function
- `return_type` → optional, usually inferred
- `body` → code block

**Basic lambda:**

```cpp
auto add = [](int a, int b) {
    return a + b;
};

int result = add(3, 4);  // 7
```

---

**Capture Modes**

| Mode | Meaning | Thread-safe? | Use when |
|------|---------|-------------|---------|
| `[=]` | All outer vars by value (copy) | Yes | Default for parallel callbacks |
| `[&]` | All outer vars by reference | No | Single-threaded only |
| `[var]` | Specific var by value | Yes | Explicit, recommended |
| `[&var]` | Specific var by reference | No | Read-only, single-threaded |
| `[=, &var]` | Mixed: most by value, one by ref | Partial | Fine-grained control |
| `[var = std::move(obj)]` | Move object into lambda | Yes | Transfer ownership (C++14) |

```cpp
int multiplier = 10;

auto scale_copy     = [=](int x)       { return x * multiplier; };  // copy — safe in threads
auto scale_ref      = [&](int x)       { return x * multiplier; };  // ref — dangerous in parallel
auto scale_specific = [multiplier](int x) { return x * multiplier; };  // best practice
```

---

**Mutable Lambdas**

By default, values captured by value are `const` inside the lambda. Use `mutable` to modify the internal copy without touching the original:

```cpp
int a = 10;

auto f = [a]() mutable {
    a += 5;
    std::cout << a;  // prints 15
};
f();
std::cout << a;  // still 10 — original unchanged
```

`mutable` modifies the lambda's **own copy**, not the outer variable.

---

**Generic Lambdas (C++14)**

Use `auto` parameters to make a lambda templated automatically — no explicit `template<>` needed:

```cpp
auto add = [](auto a, auto b) { return a + b; };

add(3, 4);        // int + int = 7
add(3.14, 2.71);  // double + double = 5.85
add(1.0f, 2.0f);  // float + float
```

This is used heavily in parallel STL, oneTBB, and SYCL to write type-generic kernels.

---

**Thread Safety Rules**

**Capture by value `[x]`** — each thread gets its own copy. Safe.

**Capture by reference `[&x]`** — all threads share the same variable. Race condition if any thread writes to it.

```cpp
std::vector<int> data = {1, 2, 3, 4};
int multiplier = 2;

// Safe: each element read is independent, multiplier captured by value
std::for_each(std::execution::par, data.begin(), data.end(),
              [multiplier](int& x) { x *= multiplier; });

// Unsafe: if multiplier were captured by ref and modified by any thread
std::for_each(std::execution::par, data.begin(), data.end(),
              [&](int& x) { x *= multiplier; });  // race condition if multiplier changes
```

> **Rule: prefer `[=]` or named value captures in parallel loops. Use `[&]` only in single-threaded code or when the captured variables are read-only.**

---

**Lambda Lifetime — Dangling Reference Trap**

If a lambda outlives the scope of its captured references, you get a dangling reference:

```cpp
std::function<int()> make_lambda() {
    int local = 42;
    return [&]() { return local; };  // DANGLING — local is destroyed when function returns
}

auto f = make_lambda();
f();  // undefined behavior
```

**Fix 1 — capture by value:**

```cpp
return [local]() { return local; };  // safe copy
```

**Fix 2 — move ownership into the lambda (C++14):**

```cpp
auto buf = std::make_unique<int>(42);
std::thread t([buf = std::move(buf)]() {
    std::cout << *buf;  // safe: ownership moved into lambda
});
t.join();
```

`[buf = std::move(buf)]` transfers ownership of the `unique_ptr` into the lambda. The lambda now owns the resource — it cannot outlive it.

---

</details>

#### Move-Into-Lambda — 深入剖析

这一点值得彻底理解，因为它会不断出现在并行与异步代码中。

**为什么 `unique_ptr` 不能被按值捕获：**

```cpp
std::unique_ptr<int> ptr = std::make_unique<int>(42);

auto f = [ptr]() { std::cout << *ptr; };  // DOES NOT COMPILE
// unique_ptr has no copy constructor — copying would violate unique ownership
```

`unique_ptr` 表达的是「恰好一个所有者」。按值捕获需要复制指针，从而产生两个所有者。编译器禁止这样做。

**解决方案 —— 按移动捕获（C++14）：**

```cpp
auto f = [p = std::move(ptr)]() {
    std::cout << *p << "\n";   // lambda owns it, safe to use
};

// What happened to ptr?
std::cout << (ptr ? "not empty" : "empty") << "\n";  // "empty" — ptr is now nullptr
f();  // prints 42
```

**所有权转移可视化：**

```
Before move:
  ptr ──────────────► [ 42 ]   (heap)

After [p = std::move(ptr)]:
  ptr ──► nullptr
  lambda.p ─────────► [ 42 ]   (same heap allocation, new owner)
```

没有复制。没有新的分配。lambda 拿走了背包 —— 原来的持有者现在为空。

**线程所有权 —— 每个线程获得自己的资源：**

```cpp
#include <thread>
#include <memory>

int main() {
    auto buf = std::make_unique<int[]>(1000);  // 4 KB buffer

    std::thread t([b = std::move(buf)]() {
        b[0] = 42;
        std::cout << b[0] << "\n";  // lambda owns the buffer exclusively
    });

    t.join();
    // buf is empty here — the thread owned and (after join) released it
}
```

线程 lambda 独占拥有 `buf`。没有数据竞争。不需要 mutex。当线程结束时，lambda 析构，`unique_ptr` 自动释放内存。

**移动其他可移动类型：**

任何带移动构造函数的类型都可以 —— 不只是 `unique_ptr`：

```cpp
std::vector<float> weights(1000000);   // 4 MB
std::string config = load_config();
auto handle = open_gpu_context();      // hypothetical RAII GPU handle

// Move all three into a thread lambda — zero copying
std::thread worker([
    w = std::move(weights),
    cfg = std::move(config),
    ctx = std::move(handle)
]() {
    // worker owns all three resources
    run_inference(w, cfg, ctx);
});
worker.join();
```

| 移动的对象 | 原因 |
|--------------|-----|
| `unique_ptr<T>` | 独占的 GPU/CPU 资源句柄 |
| `vector<float>` | 大型权重/激活值缓冲区 |
| `string` | 配置或序列化的模型数据 |
| `std::thread` | 把线程所有权转移给 lambda |
| 自定义 RAII 句柄 | GPU 上下文、文件、socket |

**规则：** 如果某个类型不可复制（或复制代价高昂），而 lambda 需要拥有它，或需要比当前作用域存活更久，就使用 `[x = std::move(obj)]`。

---

#### 设备端 Lambda（CUDA / SYCL）

现代 CUDA 和 SYCL 支持在 GPU 设备代码中使用 lambda：

```cpp
// CUDA — device lambda inside kernel
__global__ void kernel(float* a, float* b, int N) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i < N) {
        auto square = [=](float x) { return x * x; };  // [=] only — no refs on device
        a[i] = square(b[i]);
    }
}

// SYCL — lambda IS the kernel
q.parallel_for(sycl::range{N}, [=](sycl::id<1> i) {
    C[i] = A[i] + B[i];
});
```

> **GPU 设备 lambda 中不允许 `[&]`。** 设备代码不能引用主机内存地址。GPU lambda 一律使用 `[=]`（按值捕获）。

---

#### Lambda 速查

| 特性 | 语法 | 说明 |
|---------|--------|-------|
| 全部按值捕获 | `[=]` | 在线程中安全 |
| 全部按引用捕获 | `[&]` | 并行中危险 |
| 特定变量按值捕获 | `[var]` | 最佳实践 |
| 特定变量按引用捕获 | `[&var]` | 仅限单线程 |
| 泛型（auto 参数） | `[](auto x, auto y){}` | C++14，类型泛化 |
| Mutable | `[x]() mutable {}` | 修改内部副本 |
| 移入 lambda | `[x = std::move(obj)]` | 转移所有权，C++14 |
| 立即调用 | `[&]() { ... }()` | IIFE —— 就地执行 |

---

**本课程中 lambda 的使用位置：**

| 框架 | lambda 用法 |
|-----------|-------------|
| **Parallel STL** | `std::sort(std::execution::par, v.begin(), v.end(), [](auto a, auto b) { return a > b; });` |
| **oneTBB** | `tbb::parallel_for(0, N, [&](int i) { C[i] = A[i] + B[i]; });` |
| **OpenMP**（C++17） | `#pragma omp parallel for` + lambda 任务：`omp_set_num_threads(N); #pragma omp task` |
| **CUDA**（现代） | `__device__` 上下文中的设备 lambda —— 仅 `[=]` |
| **SYCL** | `q.parallel_for(range, [=](id<1> i) { C[i] = A[i] + B[i]; });` |

**核心信息：** 如果不理解 lambda 与捕获，就无法用现代 C++ 写并行代码。

---


<details>
<summary>English original</summary>

**Move-Into-Lambda — Deep Dive**

This is worth understanding completely because it appears constantly in parallel and async code.

**Why `unique_ptr` cannot be captured by value:**

```cpp
std::unique_ptr<int> ptr = std::make_unique<int>(42);

auto f = [ptr]() { std::cout << *ptr; };  // DOES NOT COMPILE
// unique_ptr has no copy constructor — copying would violate unique ownership
```

`unique_ptr` expresses "exactly one owner". Capture-by-value would require copying the pointer, creating two owners. The compiler forbids it.

**Solution — capture by move (C++14):**

```cpp
auto f = [p = std::move(ptr)]() {
    std::cout << *p << "\n";   // lambda owns it, safe to use
};

// What happened to ptr?
std::cout << (ptr ? "not empty" : "empty") << "\n";  // "empty" — ptr is now nullptr
f();  // prints 42
```

**Ownership transfer visualized:**

```
Before move:
  ptr ──────────────► [ 42 ]   (heap)

After [p = std::move(ptr)]:
  ptr ──► nullptr
  lambda.p ─────────► [ 42 ]   (same heap allocation, new owner)
```

No copy. No new allocation. The lambda took the backpack — the original holder is now empty.

**Thread ownership — each thread gets its own resource:**

```cpp
#include <thread>
#include <memory>

int main() {
    auto buf = std::make_unique<int[]>(1000);  // 4 KB buffer

    std::thread t([b = std::move(buf)]() {
        b[0] = 42;
        std::cout << b[0] << "\n";  // lambda owns the buffer exclusively
    });

    t.join();
    // buf is empty here — the thread owned and (after join) released it
}
```

The thread lambda owns `buf` exclusively. No data race. No need for a mutex. When the thread finishes, the lambda destructs and `unique_ptr` frees the memory automatically.

**Moving other moveable types:**

Any type with a move constructor works — not just `unique_ptr`:

```cpp
std::vector<float> weights(1000000);   // 4 MB
std::string config = load_config();
auto handle = open_gpu_context();      // hypothetical RAII GPU handle

// Move all three into a thread lambda — zero copying
std::thread worker([
    w = std::move(weights),
    cfg = std::move(config),
    ctx = std::move(handle)
]() {
    // worker owns all three resources
    run_inference(w, cfg, ctx);
});
worker.join();
```

| What you move | Why |
|--------------|-----|
| `unique_ptr<T>` | Exclusive GPU/CPU resource handle |
| `vector<float>` | Large weight/activation buffer |
| `string` | Config or serialized model data |
| `std::thread` | Transfer thread ownership to lambda |
| Custom RAII handle | GPU context, file, socket |

**Rule:** If a type is non-copyable (or copying is expensive), and the lambda needs to own it or outlive the current scope, use `[x = std::move(obj)]`.

---

**Device Lambdas (CUDA / SYCL)**

Modern CUDA and SYCL support lambdas on GPU device code:

```cpp
// CUDA — device lambda inside kernel
__global__ void kernel(float* a, float* b, int N) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i < N) {
        auto square = [=](float x) { return x * x; };  // [=] only — no refs on device
        a[i] = square(b[i]);
    }
}

// SYCL — lambda IS the kernel
q.parallel_for(sycl::range{N}, [=](sycl::id<1> i) {
    C[i] = A[i] + B[i];
});
```

> **`[&]` is not allowed in GPU device lambdas.** Device code cannot reference host memory addresses. Always use `[=]` (capture by value) for GPU lambdas.

---

**Lambda Quick Reference**

| Feature | Syntax | Notes |
|---------|--------|-------|
| Capture all by value | `[=]` | Safe in threads |
| Capture all by ref | `[&]` | Dangerous in parallel |
| Specific var by value | `[var]` | Best practice |
| Specific var by ref | `[&var]` | Single-threaded only |
| Generic (auto params) | `[](auto x, auto y){}` | C++14, type-generic |
| Mutable | `[x]() mutable {}` | Modify internal copy |
| Move into lambda | `[x = std::move(obj)]` | Transfer ownership, C++14 |
| Immediately invoked | `[&]() { ... }()` | IIFE — run in-place |

---

**Where lambdas are used in this curriculum:**

| Framework | Lambda usage |
|-----------|-------------|
| **Parallel STL** | `std::sort(std::execution::par, v.begin(), v.end(), [](auto a, auto b) { return a > b; });` |
| **oneTBB** | `tbb::parallel_for(0, N, [&](int i) { C[i] = A[i] + B[i]; });` |
| **OpenMP** (C++17) | `#pragma omp parallel for` + lambda tasks: `omp_set_num_threads(N); #pragma omp task` |
| **CUDA** (modern) | Device lambdas in `__device__` context — `[=]` only |
| **SYCL** | `q.parallel_for(range, [=](id<1> i) { C[i] = A[i] + B[i]; });` |

**Key message:** If you don't understand lambdas and captures, you cannot write parallel code in modern C++.

---

</details>

### 1.6 `std::optional`, `std::variant`, `std::any` (C++17)

**`std::optional` — 一个可能存在也可能不存在的值：**

```cpp
#include <optional>

std::optional<int> find_index(const std::vector<int>& v, int target) {
    for (int i = 0; i < v.size(); i++) {
        if (v[i] == target) return i;
    }
    return std::nullopt;  // "not found" — no magic numbers like -1
}

auto result = find_index(data, 42);
if (result) {
    std::cout << "Found at index " << *result << "\n";
}
```

**`std::variant` — 类型安全的 union：**

#### `union` vs `std::variant` — 为什么旧做法很危险

旧的 C `union` 把多种类型塞进同一块内存，却不知道当前活跃的究竟是哪种类型：

```cpp
union Data {
    int   i;
    float f;
};

Data d;
d.i = 42;
std::cout << d.f << "\n";  // undefined behavior — reading int bits as float
```

`union` 的内存布局 —— 4 bytes，不含任何类型信息：

```
+------------------+
|  42 (int bits)   |   <- what's stored
+------------------+
  ^ no type tag     <- compiler has no idea which member is active
```

读错成员是 **undefined behavior**。没有编译器报错，没有 runtime 检查 —— 只有错误结果或崩溃，而且往往只在 release build 中才暴露。

`std::variant` 在存储值旁边加上了一个 **类型标签**：

```
+------------------+
|  3.14f (float)   |   <- stored data
+------------------+
  type tag: float       <- runtime always knows which type is active
```

```cpp
#include <variant>

std::variant<int, float, std::string> v;
v = 42;         // holds int   — type tag = int
v = 3.14f;      // holds float — type tag = float
v = "hello";    // holds string — type tag = string

// std::visit dispatches to the right lambda branch based on the type tag
std::visit([](auto&& val) { std::cout << val << "\n"; }, v);
```

#### `union` vs `std::variant` 对比

| | `union` | `std::variant` |
|--|---------|----------------|
| 类型跟踪 | 无 —— 由程序员自行负责 | runtime 类型标签与值一同存储 |
| 读错类型 | undefined behavior，且悄无声息 | `std::bad_variant_access` 异常 |
| 空间开销 | 零（仅占最大成员的大小） | 最大成员大小 + 小标签（通常 1–8 bytes） |
| 并行安全性 | 危险 —— 跨线程类型混淆 | 安全 —— 每个元素自带类型信息 |
| 在 HPC 中的使用 | 新代码中避免使用 | 异构数组、命令分发、AST 节点 |

#### `std::visit` — 用 lambda 做类型分发

`std::visit` 以**实际存储的类型**调用你的 lambda，而不是某个泛化的 variant：

```cpp
std::variant<int, float> v = 3.14f;

// Generic lambda — works for any type in the variant
std::visit([](auto&& x) {
    std::cout << x << "\n";                           // prints 3.14
    std::cout << typeid(x).name() << "\n";            // prints "f" (float)
}, v);

// Typed overloads using overload pattern (C++17)
std::visit(overloaded{
    [](int   x) { std::cout << "int: "   << x << "\n"; },
    [](float x) { std::cout << "float: " << x << "\n"; },
}, v);  // prints: float: 3.14
```

#### 并行示例 — 安全地处理混合类型数组

```cpp
#include <variant>
#include <vector>
#include <algorithm>
#include <execution>

std::vector<std::variant<int, float>> data = {1, 2.5f, 3, 4.5f};

// Double every element in parallel — type-safe, no UB
std::for_each(std::execution::par, data.begin(), data.end(),
    [](auto& val) {
        std::visit([](auto&& x) { x *= 2; }, val);
    });

// Output: 2  5  6  9
for (auto& v : data)
    std::visit([](auto&& x) { std::cout << x << " "; }, v);
```

每个元素自带类型标签，因此即便访问相邻元素，并行线程也绝不会把 `int` 和 `float` 搞混。

**为什么这对 HPC 很重要：** 编译器 IR 节点、kernel 分发表，以及混合精度计算图（FP32/FP16/INT8 混合推理）都需要安全地存储异构类型。`std::variant` + `std::visit` 取代了旧的「`void*` 配一个类型 enum」写法，且不会产生任何 undefined behavior。

---


<details>
<summary>English original</summary>

**1.6 `std::optional`, `std::variant`, `std::any` (C++17)**

**`std::optional` — a value that may or may not exist:**

```cpp
#include <optional>

std::optional<int> find_index(const std::vector<int>& v, int target) {
    for (int i = 0; i < v.size(); i++) {
        if (v[i] == target) return i;
    }
    return std::nullopt;  // "not found" — no magic numbers like -1
}

auto result = find_index(data, 42);
if (result) {
    std::cout << "Found at index " << *result << "\n";
}
```

**`std::variant` — type-safe union:**

**`union` vs `std::variant` — why the old way is dangerous**

The old C `union` stores multiple types in the same memory, but has no idea which type is currently active:

```cpp
union Data {
    int   i;
    float f;
};

Data d;
d.i = 42;
std::cout << d.f << "\n";  // undefined behavior — reading int bits as float
```

Memory layout of a `union` — 4 bytes, no type information:

```
+------------------+
|  42 (int bits)   |   <- what's stored
+------------------+
  ^ no type tag     <- compiler has no idea which member is active
```

Reading the wrong member is **undefined behavior**. No compiler error, no runtime check — just wrong results or crashes, often appearing only in release builds.

`std::variant` adds a **type tag** alongside the stored value:

```
+------------------+
|  3.14f (float)   |   <- stored data
+------------------+
  type tag: float       <- runtime always knows which type is active
```

```cpp
#include <variant>

std::variant<int, float, std::string> v;
v = 42;         // holds int   — type tag = int
v = 3.14f;      // holds float — type tag = float
v = "hello";    // holds string — type tag = string

// std::visit dispatches to the right lambda branch based on the type tag
std::visit([](auto&& val) { std::cout << val << "\n"; }, v);
```

**`union` vs `std::variant` comparison**

| | `union` | `std::variant` |
|--|---------|----------------|
| Type tracking | None — programmer's responsibility | Runtime type tag stored alongside value |
| Wrong-type read | Undefined behavior, silent | `std::bad_variant_access` exception |
| Size overhead | Zero (just the max member size) | Max member size + small tag (usually 1–8 bytes) |
| Parallel safety | Dangerous — type confusion across threads | Safe — each element carries its own type info |
| Use in HPC | Avoid in new code | Heterogeneous arrays, command dispatch, AST nodes |

**`std::visit` — type-dispatching with a lambda**

`std::visit` calls your lambda with the **actually stored type**, not a generic variant:

```cpp
std::variant<int, float> v = 3.14f;

// Generic lambda — works for any type in the variant
std::visit([](auto&& x) {
    std::cout << x << "\n";                           // prints 3.14
    std::cout << typeid(x).name() << "\n";            // prints "f" (float)
}, v);

// Typed overloads using overload pattern (C++17)
std::visit(overloaded{
    [](int   x) { std::cout << "int: "   << x << "\n"; },
    [](float x) { std::cout << "float: " << x << "\n"; },
}, v);  // prints: float: 3.14
```

**Parallel example — mixed-type array processed safely**

```cpp
#include <variant>
#include <vector>
#include <algorithm>
#include <execution>

std::vector<std::variant<int, float>> data = {1, 2.5f, 3, 4.5f};

// Double every element in parallel — type-safe, no UB
std::for_each(std::execution::par, data.begin(), data.end(),
    [](auto& val) {
        std::visit([](auto&& x) { x *= 2; }, val);
    });

// Output: 2  5  6  9
for (auto& v : data)
    std::visit([](auto&& x) { std::cout << x << " "; }, v);
```

Each element carries its own type tag, so parallel threads never confuse `int` and `float` even when accessing adjacent elements.

**Why it matters for HPC:** Compiler IR nodes, kernel dispatch tables, and mixed-precision computation graphs (FP32/FP16/INT8 hybrid inference) all need to store heterogeneous types safely. `std::variant` + `std::visit` replaces the old `void*`-with-a-type-enum pattern with zero undefined behavior.

---

</details>

### 1.7 移动语义（性能核心）

**问题——代价高昂的拷贝：**

```cpp
std::vector<float> create_large_buffer() {
    std::vector<float> buf(10000000);  // 40 MB
    // ... fill buffer ...
    return buf;  // Does this copy 40 MB? NO — move semantics!
}
```

**拷贝 vs 移动：**

```cpp
std::vector<int> a = {1, 2, 3, 4, 5};

// Copy: duplicates all data (expensive)
std::vector<int> b = a;           // b is a copy, a unchanged

// Move: transfers ownership (cheap — just pointer swap)
std::vector<int> c = std::move(a); // c owns the data, a is now empty
```

**移动在内部如何工作：**

```
Before move:
  a → [heap: 1, 2, 3, 4, 5]    (owns the buffer)

After std::move(a) → c:
  a → nullptr                    (empty, moved-from)
  c → [heap: 1, 2, 3, 4, 5]    (now owns the buffer)
```

没有拷贝任何数据。只是三次指针/长度赋值。O(1) 而非 O(n)。

**何时使用 `std::move`：**
- 从函数返回大对象（编译器通常会自动完成——NRVO）
- 把 buffer 的所有权转移给另一个线程
- 把数据移动进容器：`vec.push_back(std::move(large_object));`

**为什么它对并行计算重要：**
- GPU 内存传输代价高昂。移动语义 = 零拷贝思维。
- 线程池在线程之间移动工作项，无需拷贝。
- CUDA 的 `cudaMemcpyAsync` + pinned memory 就是 GPU 上等价于移动语义的做法。

---

### 1.8 多线程基础（`std::thread`，`std::mutex`）

**启动一个线程：**

```cpp
#include <thread>

void compute(int id) {
    std::cout << "Thread " << id << " running\n";
}

int main() {
    std::thread t1(compute, 1);
    std::thread t2(compute, 2);

    t1.join();  // Wait for t1 to finish
    t2.join();  // Wait for t2 to finish
}
```

**用 mutex 保护共享数据：**

```cpp
#include <mutex>

std::mutex mtx;
int shared_counter = 0;

void increment(int n) {
    for (int i = 0; i < n; i++) {
        std::lock_guard<std::mutex> lock(mtx);  // RAII lock
        shared_counter++;
    }  // lock released here automatically
}
```

**更好：用 `std::scoped_lock`（C++17）处理多个 mutex：**

```cpp
std::mutex m1, m2;
{
    std::scoped_lock lock(m1, m2);  // Locks both, prevents deadlock
    // ... critical section ...
}
```

**对简单共享变量用 `std::atomic`：**

```cpp
#include <atomic>

std::atomic<int> counter{0};

void increment(int n) {
    for (int i = 0; i < n; i++) {
        counter++;  // Thread-safe, no mutex needed
    }
}
```

**为什么它重要：** `std::thread` 和 `std::mutex` 是 OpenMP 和 oneTBB 所抽象掉的原语。理解它们有助于你在任何并行框架中调试竞态条件和死锁。

---

### 1.9 STL 算法与并行 STL（C++17）

STL 算法是预置的、经过优化的函数，可作用于任何容器。它们消除了手写循环，避免了索引 bug，并且——借助 C++17 执行策略——只需多传一个参数就能并行运行。

#### 核心 STL 算法

| 算法 | 作用 | 示例 |
|-----------|-------------|---------|
| `std::sort` | 对元素排序 | `sort(v.begin(), v.end())` |
| `std::for_each` | 对每个元素应用函数 | `for_each(v.begin(), v.end(), f)` |
| `std::transform` | 应用函数，写入输出 | `transform(a, a+n, b, out, f)` |
| `std::reduce` | 并行安全的求和/求积等 | `reduce(v.begin(), v.end())` |
| `std::find_if` | 按谓词查找 | `find_if(v.begin(), v.end(), pred)` |
| `std::minmax_element` | 一趟求出最小值和最大值 | `auto [lo, hi] = minmax_element(...)` |
| `std::count_if` | 统计满足条件的元素个数 | `count_if(v.begin(), v.end(), pred)` |
| `std::copy_if` | 带过滤的拷贝 | `copy_if(src, src+n, dst, pred)` |

#### 顺序示例——排序与遍历

```cpp
#include <vector>
#include <algorithm>
#include <iostream>

std::vector<int> v = {4, 2, 7, 1, 5};

std::sort(v.begin(), v.end());

for (auto x : v) std::cout << x << " ";  // 1 2 4 5 7
```

#### Lambda + 算法——注入自定义行为

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

// Multiply every element by 2 — no loop index, no off-by-one
std::for_each(v.begin(), v.end(), [](int& x) { x *= 2; });
// v = {2, 4, 6, 8, 10}

// Transform: square each element into a new vector
std::vector<int> sq(v.size());
std::transform(v.begin(), v.end(), sq.begin(), [](int x) { return x * x; });

// Find first element > 5
auto it = std::find_if(v.begin(), v.end(), [](int x) { return x > 5; });
```


<details>
<summary>English original</summary>

**1.7 Move Semantics (Performance Core)**

**The problem — expensive copies:**

```cpp
std::vector<float> create_large_buffer() {
    std::vector<float> buf(10000000);  // 40 MB
    // ... fill buffer ...
    return buf;  // Does this copy 40 MB? NO — move semantics!
}
```

**Copy vs move:**

```cpp
std::vector<int> a = {1, 2, 3, 4, 5};

// Copy: duplicates all data (expensive)
std::vector<int> b = a;           // b is a copy, a unchanged

// Move: transfers ownership (cheap — just pointer swap)
std::vector<int> c = std::move(a); // c owns the data, a is now empty
```

**How move works internally:**

```
Before move:
  a → [heap: 1, 2, 3, 4, 5]    (owns the buffer)

After std::move(a) → c:
  a → nullptr                    (empty, moved-from)
  c → [heap: 1, 2, 3, 4, 5]    (now owns the buffer)
```

No data was copied. Just three pointer/size assignments. O(1) instead of O(n).

**When to use `std::move`:**
- Returning large objects from functions (compiler often does this automatically — NRVO)
- Transferring ownership of buffers to another thread
- Moving data into containers: `vec.push_back(std::move(large_object));`

**Why it matters for parallel computing:**
- GPU memory transfers are expensive. Move semantics = zero-copy mindset.
- Thread pools move work items between threads without copying.
- CUDA's `cudaMemcpyAsync` + pinned memory is the GPU equivalent of move semantics.

---

**1.8 Multithreading Basics (`std::thread`, `std::mutex`)**

**Launching a thread:**

```cpp
#include <thread>

void compute(int id) {
    std::cout << "Thread " << id << " running\n";
}

int main() {
    std::thread t1(compute, 1);
    std::thread t2(compute, 2);

    t1.join();  // Wait for t1 to finish
    t2.join();  // Wait for t2 to finish
}
```

**Protecting shared data with mutex:**

```cpp
#include <mutex>

std::mutex mtx;
int shared_counter = 0;

void increment(int n) {
    for (int i = 0; i < n; i++) {
        std::lock_guard<std::mutex> lock(mtx);  // RAII lock
        shared_counter++;
    }  // lock released here automatically
}
```

**Better: `std::scoped_lock` (C++17) for multiple mutexes:**

```cpp
std::mutex m1, m2;
{
    std::scoped_lock lock(m1, m2);  // Locks both, prevents deadlock
    // ... critical section ...
}
```

**`std::atomic` for simple shared variables:**

```cpp
#include <atomic>

std::atomic<int> counter{0};

void increment(int n) {
    for (int i = 0; i < n; i++) {
        counter++;  // Thread-safe, no mutex needed
    }
}
```

**Why it matters:** `std::thread` and `std::mutex` are the primitives that OpenMP and oneTBB abstract over. Understanding them helps you debug race conditions and deadlocks in any parallel framework.

---

**1.9 STL Algorithms and Parallel STL (C++17)**

STL algorithms are pre-built, optimized functions that operate on any container. They eliminate manual loops, prevent index bugs, and — with C++17 execution policies — run in parallel with a single extra argument.

**Core STL Algorithms**

| Algorithm | What it does | Example |
|-----------|-------------|---------|
| `std::sort` | Sort elements | `sort(v.begin(), v.end())` |
| `std::for_each` | Apply function to every element | `for_each(v.begin(), v.end(), f)` |
| `std::transform` | Apply function, write to output | `transform(a, a+n, b, out, f)` |
| `std::reduce` | Parallel-safe sum/product/etc. | `reduce(v.begin(), v.end())` |
| `std::find_if` | Search by predicate | `find_if(v.begin(), v.end(), pred)` |
| `std::minmax_element` | Min and max in one pass | `auto [lo, hi] = minmax_element(...)` |
| `std::count_if` | Count matching elements | `count_if(v.begin(), v.end(), pred)` |
| `std::copy_if` | Filtered copy | `copy_if(src, src+n, dst, pred)` |

**Sequential example — sort and iterate**

```cpp
#include <vector>
#include <algorithm>
#include <iostream>

std::vector<int> v = {4, 2, 7, 1, 5};

std::sort(v.begin(), v.end());

for (auto x : v) std::cout << x << " ";  // 1 2 4 5 7
```

**Lambda + algorithm — custom behavior injected**

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

// Multiply every element by 2 — no loop index, no off-by-one
std::for_each(v.begin(), v.end(), [](int& x) { x *= 2; });
// v = {2, 4, 6, 8, 10}

// Transform: square each element into a new vector
std::vector<int> sq(v.size());
std::transform(v.begin(), v.end(), sq.begin(), [](int x) { return x * x; });

// Find first element > 5
auto it = std::find_if(v.begin(), v.end(), [](int x) { return x > 5; });
```

</details>

#### 并行 STL —— 一个参数改变一切

C++17 执行策略可并行化任何 STL 算法：

```cpp
#include <algorithm>
#include <execution>
#include <vector>

std::vector<float> data(10'000'000);

// Sequential (default, single core)
std::sort(std::execution::seq,      data.begin(), data.end());

// Parallel (multi-threaded across all cores)
std::sort(std::execution::par,      data.begin(), data.end());

// Parallel + SIMD (threads + vectorization)
std::sort(std::execution::par_unseq, data.begin(), data.end());
```

**执行策略：**

| 策略 | 行为 | 使用场景 |
|--------|----------|-------------|
| `seq` | 顺序 | 小数据量、调试 |
| `par` | 多线程 | 大数据量、元素相互独立 |
| `par_unseq` | 多线程 + SIMD | 最大吞吐、无依赖 |
| `unseq`（C++20） | 仅 SIMD（单线程） | 可向量化、单核 |

#### 并行 transform、reduce、for_each

```cpp
// Parallel transform: c[i] = a[i] + b[i]
std::transform(std::execution::par,
    a.begin(), a.end(), b.begin(), c.begin(),
    [](float x, float y) { return x + y; });

// Parallel reduce: sum all elements
float total = std::reduce(std::execution::par, data.begin(), data.end());

// Parallel reduce with custom op: product
float product = std::reduce(std::execution::par,
    data.begin(), data.end(), 1.0f, std::multiplies<float>{});

// Parallel for_each + SIMD: sqrt every element
std::for_each(std::execution::par_unseq, data.begin(), data.end(),
    [](float& x) { x = std::sqrt(x); });
```

#### 手写循环 vs STL vs 并行 STL

| | 手写循环 | STL + lambda | 并行 STL |
|--|------------|--------------|--------------|
| 代码长度 | 长 | 短 | 短 |
| 下标 bug 风险 | 中 | 无 | 无 |
| 并行性 | 手动（`std::thread`） | 手动 | 自动 |
| SIMD | 编译器可能向量化 | 编译器可能向量化 | `par_unseq` 强制向量化 |
| 调试 | 难 | 易 | 较难（非确定性） |

> **`par_unseq` 要求：**循环体必须**无数据竞争**且**无循环携带依赖**（`a[i]` 不得读取 `a[i-1]`）。违反任意一条都会产生未定义行为 —— 不是编译错误，甚至不是稳定的错误结果。

> **警告 —— 并非总是更快：**并行 STL 有开销（线程池启动、同步）。对于小数组（< ~100K 个元素），`seq` 通常更快。在 HPC（高性能计算）和生产级 ML 系统中，团队往往更偏好 **OpenMP** 或 **oneTBB**，以获得更可预测、可调优、可调试的并行性。选定 `par` 或 `par_unseq` 之前先做 benchmark。

**编译器支持：**GCC 9+（需 `-ltbb`）、MSVC 2017+、Intel DPC++。Clang 的支持正在改善。

---

### 1.10 模板与泛型编程

**函数模板：**

```cpp
template<typename T>
T add(T a, T b) {
    return a + b;
}

add(3, 4);        // T = int
add(3.14, 2.71);  // T = double
```

**类模板：**

```cpp
template<typename T, int N>
struct Vector {
    T data[N];
    T& operator[](int i) { return data[i]; }
};

Vector<float, 4> v;  // 4-element float vector
```

**模板为何对并行计算重要：**
- CUDA kernel 常按数据类型与分块大小做模板化：`matmul<float, 32><<<grid, block>>>(...)`
- CUTLASS 完全基于模板 —— 可配置的 GEMM（矩阵-矩阵乘）维度、精度与分块
- CuTe（CUDA Template Engine）用模板实现布局与拷贝抽象
- 零开销抽象：模板在编译期解析，无 runtime 开销

---

### 1.11 `constexpr`（编译期计算）

**把计算从 runtime 移到编译期：**

```cpp
constexpr int square(int x) {
    return x * x;
}

constexpr int tile_size = square(16);  // Computed at compile time = 256

// Use in array declarations, template parameters, etc.
float buffer[tile_size];  // float buffer[256] — no runtime cost
```

**`constexpr` + `if`（C++17）：**

```cpp
template<typename T>
void process(T value) {
    if constexpr (std::is_integral_v<T>) {
        // Integer-specific code — compiled away for floats
    } else {
        // Float-specific code — compiled away for integers
    }
}
```

**为什么这对 HPC 重要：**
- GPU kernel 中的分块大小、block 维度与 buffer 大小常常是编译期常量
- `constexpr` 使编译器能激进优化（展开循环、消除分支）
- 在 CUTLASS 中大量用于可配置的 kernel 参数

---


<details>
<summary>English original</summary>

**Parallel STL — one argument changes everything**

C++17 execution policies parallelize any STL algorithm:

```cpp
#include <algorithm>
#include <execution>
#include <vector>

std::vector<float> data(10'000'000);

// Sequential (default, single core)
std::sort(std::execution::seq,      data.begin(), data.end());

// Parallel (multi-threaded across all cores)
std::sort(std::execution::par,      data.begin(), data.end());

// Parallel + SIMD (threads + vectorization)
std::sort(std::execution::par_unseq, data.begin(), data.end());
```

**Execution policies:**

| Policy | Behavior | When to use |
|--------|----------|-------------|
| `seq` | Sequential | Small data, debugging |
| `par` | Multi-threaded | Large data, independent elements |
| `par_unseq` | Multi-threaded + SIMD | Maximum throughput, no dependencies |
| `unseq` (C++20) | SIMD only (single thread) | Vectorizable, single core |

**Parallel transform, reduce, for_each**

```cpp
// Parallel transform: c[i] = a[i] + b[i]
std::transform(std::execution::par,
    a.begin(), a.end(), b.begin(), c.begin(),
    [](float x, float y) { return x + y; });

// Parallel reduce: sum all elements
float total = std::reduce(std::execution::par, data.begin(), data.end());

// Parallel reduce with custom op: product
float product = std::reduce(std::execution::par,
    data.begin(), data.end(), 1.0f, std::multiplies<float>{});

// Parallel for_each + SIMD: sqrt every element
std::for_each(std::execution::par_unseq, data.begin(), data.end(),
    [](float& x) { x = std::sqrt(x); });
```

**Manual loop vs STL vs Parallel STL**

| | Manual loop | STL + lambda | Parallel STL |
|--|------------|--------------|--------------|
| Code length | Long | Short | Short |
| Index bug risk | Medium | None | None |
| Parallelism | Manual (`std::thread`) | Manual | Automatic |
| SIMD | Compiler may vectorize | Compiler may vectorize | `par_unseq` forces it |
| Debugging | Hard | Easy | Harder (non-deterministic) |

> **`par_unseq` requirements:** Loop body must have **no data races** AND **no loop-carried dependencies** (`a[i]` must not read `a[i-1]`). Violating either produces undefined behavior — not a compile error, not even consistent wrong results.

> **Warning — not always faster:** Parallel STL has overhead (thread pool spin-up, synchronization). For small arrays (< ~100K elements), `seq` is often faster. In HPC and production ML systems, teams frequently prefer **OpenMP** or **oneTBB** for more predictable, tunable, and debuggable parallelism. Benchmark before committing to `par` or `par_unseq`.

**Compiler support:** GCC 9+ (with `-ltbb`), MSVC 2017+, Intel DPC++. Clang support is improving.

---

**1.10 Templates & Generic Programming**

**Function templates:**

```cpp
template<typename T>
T add(T a, T b) {
    return a + b;
}

add(3, 4);        // T = int
add(3.14, 2.71);  // T = double
```

**Class templates:**

```cpp
template<typename T, int N>
struct Vector {
    T data[N];
    T& operator[](int i) { return data[i]; }
};

Vector<float, 4> v;  // 4-element float vector
```

**Why templates matter for parallel computing:**
- CUDA kernels are often templated on data type and tile size: `matmul<float, 32><<<grid, block>>>(...)`
- CUTLASS is entirely template-based — configurable GEMM dimensions, precisions, and tiling
- CuTe (CUDA Template Engine) uses templates for layout and copy abstractions
- Zero-cost abstraction: templates are resolved at compile time, no runtime overhead

---

**1.11 `constexpr` (Compile-Time Computation)**

**Move computation from runtime to compile time:**

```cpp
constexpr int square(int x) {
    return x * x;
}

constexpr int tile_size = square(16);  // Computed at compile time = 256

// Use in array declarations, template parameters, etc.
float buffer[tile_size];  // float buffer[256] — no runtime cost
```

**`constexpr` + `if` (C++17):**

```cpp
template<typename T>
void process(T value) {
    if constexpr (std::is_integral_v<T>) {
        // Integer-specific code — compiled away for floats
    } else {
        // Float-specific code — compiled away for integers
    }
}
```

**Why it matters for HPC:**
- Tile sizes, block dimensions, and buffer sizes in GPU kernels are often compile-time constants
- `constexpr` enables the compiler to optimize aggressively (unroll loops, eliminate branches)
- Used extensively in CUTLASS for configurable kernel parameters

---

</details>

### 1.12 C++ 特性如何映射到并行框架

| C++ 特性 | 用于 | 方式 |
|-------------|---------|-----|
| **Lambda 表达式** | oneTBB、SYCL、Parallel STL、现代 CUDA | kernel 函数、任务体、parallel_for 回调 |
| **移动语义** | GPU 内存、线程池 | 零拷贝缓冲区传输、工作项传递 |
| **模板** | CUDA、CUTLASS、CuTe、CK | 可配置 kernel、泛型算法 |
| **`constexpr`** | CUDA、HLS、高性能计算（HPC）库 | 编译期分块大小、缓冲区维度 |
| **`auto`** | 各处 | 复杂返回类型、模板推导 |
| **智能指针** | 资源管理 | 针对 GPU 内存、文件句柄的 RAII 封装 |
| **Parallel STL** | CPU 并行 | 一行实现 SIMD + 多线程 |
| **`std::thread`/`mutex`** | 手动多线程 | OpenMP、oneTBB 内部实现的基础 |
| **`std::atomic`** | 无锁算法 | 多线程代码中的计数器、标志位 |
| **结构化绑定** | 数据处理 | 干净地遍历结果、tuple |

---

## 第 2 节：CPU 上的 SIMD

> *通往 GPU 思维的入口 —— 同一操作，多个数据。*

既然已经有了 C++ 基础，就把它用到第一层并行：SIMD 向量指令。

### SIMD 是什么

一条指令处理 4、8、16 或 32 个数据元素的 CPU 向量指令：

| 指令集 | 宽度 | 每指令数据量 | 平台 |
|----------------|-------|---------------------|----------|
| SSE (1999) | 128-bit | 4x float32 | x86 |
| AVX (2011) | 256-bit | 8x float32 | x86 |
| AVX-512 (2017) | 512-bit | 16x float32 | x86（服务器） |
| ARM NEON | 128-bit | 4x float32 | ARM（Jetson、移动端） |

### 从 SIMD 到 GPU —— 一个思想，三种表达

并行栈的每一层都在**同时把同一操作施加到多个数据元素上**。变化的是宽度、编程模型，以及由谁管理索引。

拿最简单的操作来说：`c[i] = a[i] + b[i]`。以下是它在各层的表达方式：

```
Scalar (1 element at a time):
  c[0] = a[0] + b[0]
  c[1] = a[1] + b[1]   ← you write a loop, CPU executes one add per cycle
  c[2] = a[2] + b[2]
  ...

CPU SIMD / AVX (8 elements at a time):
  c[0..7] = a[0..7] + b[0..7]  ← you write i += 8, CPU runs 8 adds in one instruction
  c[8..15] = ...

GPU warp / CUDA (32 elements at a time):
  each thread handles one i      ← you write kernel for one element,
  32 threads run in lock-step       GPU runs 32 threads simultaneously (one warp)
  c[0..31] = a[0..31] + b[0..31]

GPU grid (thousands at a time):
  all threads launch at once     ← GPU runs thousands of warps across SM clusters
```

```cpp
// ── Scalar ──────────────────────────────────────────────────────
for (int i = 0; i < n; i++)
    c[i] = a[i] + b[i];            // 1 add/cycle

// ── CPU SIMD (AVX) ───────────────────────────────────────────────
for (int i = 0; i < n; i += 8) {
    __m256 va = _mm256_loadu_ps(&a[i]);
    __m256 vb = _mm256_loadu_ps(&b[i]);
    _mm256_storeu_ps(&c[i], _mm256_add_ps(va, vb));  // 8 adds/cycle
}

// ── GPU (CUDA) ───────────────────────────────────────────────────
__global__ void add(float* a, float* b, float* c, int n) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;  // each thread owns one i
    if (i < n) c[i] = a[i] + b[i];                  // 32 threads run this simultaneously
}
```

**关键差异 —— 不是新思想，只是新机制：**

| | 标量 | CPU SIMD（AVX） | GPU（CUDA warp） |
|--|--------|---------------|-----------------|
| 宽度 | 1 个元素 | 8 个 float | 32 个线程 |
| 谁挑选 `i` | 你的循环计数器 | 你的步长（`+= 8`） | 硬件（`threadIdx + blockIdx`） |
| 快速内存 | L1/L2 缓存 | L1/L2 缓存 | 共享内存（48 KB/SM） |
| 慢速内存 | RAM | RAM | 全局内存（VRAM） |
| 隐藏延迟 | 乱序执行 | 跨循环迭代的 ILP | warp 切换 |
| 发散代价 | 分支预测错误 | 破坏向量化 | 串行化 warp lane |

**扩展阶梯：**

```
AVX2:       8 floats  × ~4 GHz × 2 (FMA) = ~64 GFLOPS / core
GPU warp:  32 threads × thousands of warps  → tens of TFLOPS
```

SIMD 是思维基础。一旦理解为什么 `i += 8` 能一次处理 8 个元素，GPU 模型 —— “让每个线程成为一个 lane，然后启动数百万个” —— 就自然随之而来。

---

### 自动向量化（编译器完成）

最省事的路径 —— 写一个简单的循环，让编译器向量化：

```cpp
// This loop is auto-vectorizable
void add_vectors(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}
```

```bash
# Check if compiler vectorized your loop
g++ -O2 -march=native -fopt-info-vec-optimized my_code.cpp
# Output: "loop vectorized using 32 byte vectors" = success
```

**自动向量化的规则：**
- 无循环携带依赖（`a[i] = a[i-1] + 1` 无法向量化）
- 数据类型简单（float、int —— 不是复杂对象）
- 对齐内存有帮助（`alignas(32) float data[1024];`）
- 使用 `-O2` 或 `-O3` 配合 `-march=native`


<details>
<summary>English original</summary>

**1.12 How C++ Features Map to Parallel Frameworks**

| C++ Feature | Used in | How |
|-------------|---------|-----|
| **Lambdas** | oneTBB, SYCL, Parallel STL, modern CUDA | Kernel functions, task bodies, parallel_for callbacks |
| **Move semantics** | GPU memory, thread pools | Zero-copy buffer transfer, work item passing |
| **Templates** | CUDA, CUTLASS, CuTe, CK | Configurable kernels, generic algorithms |
| **`constexpr`** | CUDA, HLS, HPC libraries | Compile-time tile sizes, buffer dimensions |
| **`auto`** | Everywhere | Complex return types, template deduction |
| **Smart pointers** | Resource management | RAII wrappers for GPU memory, file handles |
| **Parallel STL** | CPU parallelism | One-line SIMD + threading |
| **`std::thread`/`mutex`** | Manual threading | Foundation for OpenMP, oneTBB internals |
| **`std::atomic`** | Lock-free algorithms | Counters, flags in multi-threaded code |
| **Structured bindings** | Data processing | Clean iteration over results, tuples |

---

**Section 2: SIMD on CPUs**

> *The gateway to GPU thinking — same operation, multiple data.*

Now that you have the C++ foundation, apply it to the first level of parallelism: SIMD vector instructions.

**What SIMD Is**

CPU vector instructions that process 4, 8, 16, or 32 data elements in one instruction:

| Instruction Set | Width | Data per instruction | Platform |
|----------------|-------|---------------------|----------|
| SSE (1999) | 128-bit | 4x float32 | x86 |
| AVX (2011) | 256-bit | 8x float32 | x86 |
| AVX-512 (2017) | 512-bit | 16x float32 | x86 (server) |
| ARM NEON | 128-bit | 4x float32 | ARM (Jetson, mobile) |

**From SIMD to GPU — One Idea, Three Expressions**

Every level of the parallel stack applies **the same operation to multiple data elements at once**. What changes is the width, the programming model, and who manages the indices.

Take the simplest possible operation: `c[i] = a[i] + b[i]`. Here is how you express it at each level:

```
Scalar (1 element at a time):
  c[0] = a[0] + b[0]
  c[1] = a[1] + b[1]   ← you write a loop, CPU executes one add per cycle
  c[2] = a[2] + b[2]
  ...

CPU SIMD / AVX (8 elements at a time):
  c[0..7] = a[0..7] + b[0..7]  ← you write i += 8, CPU runs 8 adds in one instruction
  c[8..15] = ...

GPU warp / CUDA (32 elements at a time):
  each thread handles one i      ← you write kernel for one element,
  32 threads run in lock-step       GPU runs 32 threads simultaneously (one warp)
  c[0..31] = a[0..31] + b[0..31]

GPU grid (thousands at a time):
  all threads launch at once     ← GPU runs thousands of warps across SM clusters
```

```cpp
// ── Scalar ──────────────────────────────────────────────────────
for (int i = 0; i < n; i++)
    c[i] = a[i] + b[i];            // 1 add/cycle

// ── CPU SIMD (AVX) ───────────────────────────────────────────────
for (int i = 0; i < n; i += 8) {
    __m256 va = _mm256_loadu_ps(&a[i]);
    __m256 vb = _mm256_loadu_ps(&b[i]);
    _mm256_storeu_ps(&c[i], _mm256_add_ps(va, vb));  // 8 adds/cycle
}

// ── GPU (CUDA) ───────────────────────────────────────────────────
__global__ void add(float* a, float* b, float* c, int n) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;  // each thread owns one i
    if (i < n) c[i] = a[i] + b[i];                  // 32 threads run this simultaneously
}
```

**The key differences — not a new idea, just new mechanics:**

| | Scalar | CPU SIMD (AVX) | GPU (CUDA warp) |
|--|--------|---------------|-----------------|
| Width | 1 element | 8 floats | 32 threads |
| Who picks `i` | Your loop counter | Your stride (`+= 8`) | Hardware (`threadIdx + blockIdx`) |
| Fast memory | L1/L2 cache | L1/L2 cache | Shared memory (48 KB/SM) |
| Slow memory | RAM | RAM | Global memory (VRAM) |
| Hiding latency | Out-of-order execution | ILP across loop iterations | Warp switching |
| Divergence cost | Branch misprediction | Breaks vectorization | Serializes warp lanes |

**The scaling ladder:**

```
AVX2:       8 floats  × ~4 GHz × 2 (FMA) = ~64 GFLOPS / core
GPU warp:  32 threads × thousands of warps  → tens of TFLOPS
```

SIMD is the mental foundation. Once you understand why `i += 8` processes 8 elements at once, the GPU model — "just make every thread a lane, and launch millions" — follows naturally.

---

**Auto-Vectorization (Compiler Does It)**

The easiest path — write a simple loop, let the compiler vectorize:

```cpp
// This loop is auto-vectorizable
void add_vectors(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}
```

```bash
# Check if compiler vectorized your loop
g++ -O2 -march=native -fopt-info-vec-optimized my_code.cpp
# Output: "loop vectorized using 32 byte vectors" = success
```

**Rules for auto-vectorization:**
- No loop-carried dependencies (`a[i] = a[i-1] + 1` cannot vectorize)
- Simple data types (float, int — not complex objects)
- Aligned memory helps (`alignas(32) float data[1024];`)
- Use `-O2` or `-O3` with `-march=native`

</details>

### 手动 Intrinsics（完全控制）

当自动向量化失败，或需要特定指令时：

```cpp
#include <immintrin.h>

void add_vectors_avx(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i += 8) {
        __m256 va = _mm256_load_ps(&a[i]);   // Load 8 floats from a
        __m256 vb = _mm256_load_ps(&b[i]);   // Load 8 floats from b
        __m256 vc = _mm256_add_ps(va, vb);   // Add 8 pairs at once
        _mm256_store_ps(&c[i], vc);           // Store 8 results to c
    }
}
```

**关键 intrinsics 模式：**

| 操作 | Intrinsic | 作用 |
|-----------|-----------|-------------|
| 对齐加载 | `_mm256_load_ps(ptr)` | 加载 8 个 **32 字节对齐**的 float —— **未对齐会崩溃** |
| 非对齐加载 | `_mm256_loadu_ps(ptr)` | 加载 8 个 float（任意对齐）—— 更安全的默认选择 |
| 存储 | `_mm256_store_ps(ptr, v)` | 存储 8 个 float（对齐） |
| 流式存储 | `_mm256_stream_ps(ptr, v)` | 绕过缓存写入 —— 用于大块只写一次的缓冲区 |
| 加法 | `_mm256_add_ps(a, b)` | 8 路并行加法 |
| 乘法 | `_mm256_mul_ps(a, b)` | 8 路并行乘法 |
| **FMA** | `_mm256_fmadd_ps(a, b, c)` | 8 路并行 `a*b + c` —— **一条指令，而非两条；也更精确** |
| 广播 | `_mm256_set1_ps(x)` | 用同一个值填充全部 8 个 lane |
| 水平加法 | `_mm256_hadd_ps(a, b)` | 在每个 128 位半区内把相邻元素对相加 |
| 比较 | `_mm256_cmp_ps(a, b, _CMP_GT_OS)` | 逐 lane 比较 → 全 0 或全 1 掩码 |
| 混合 | `_mm256_blendv_ps(a, b, mask)` | 逐 lane 选择：`mask[i] ? b[i] : a[i]` |
| 置换（跨 lane） | `_mm256_permutevar8x32_ps(v, idx)` | 按索引向量做完整的跨 lane 重排 |
| 聚集 | `_mm256_i32gather_ps(base, idx, 4)` | 从非连续地址加载 |

> **`_mm256_load_ps` 对齐：** 如果 `ptr` 不是 32 字节对齐，这会在 runtime 触发 segfault（`#GP` fault）—— 不是编译错误。拿不准时就用 `_mm256_loadu_ps`（`u` = 非对齐）。在现代 CPU 上，性能差异可以忽略不计。

> **FMA 是深度学习的核心：** `_mm256_fmadd_ps(a, b, c)` 用一条融合指令完成 `a*b + c` 的计算。GEMM（矩阵-矩阵乘）、卷积以及 Transformer attention 的内层循环完全由 FMA 组成。这也是 FLOPS 计数通常以「FMA 的 FLOPS」形式给出的原因。在 AVX2 上，8 宽 FMA、每周期 2 次操作 = **每核 16 GFLOPS/GHz**。

**对齐内存：**

```cpp
// Aligned allocation (required for _mm256_load_ps)
alignas(32) float data[1024];

// Or dynamic allocation
float* data = (float*)aligned_alloc(32, 1024 * sizeof(float));
```

---


<details>
<summary>English original</summary>

**Manual Intrinsics (Full Control)**

When auto-vectorization fails or you need specific instructions:

```cpp
#include <immintrin.h>

void add_vectors_avx(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i += 8) {
        __m256 va = _mm256_load_ps(&a[i]);   // Load 8 floats from a
        __m256 vb = _mm256_load_ps(&b[i]);   // Load 8 floats from b
        __m256 vc = _mm256_add_ps(va, vb);   // Add 8 pairs at once
        _mm256_store_ps(&c[i], vc);           // Store 8 results to c
    }
}
```

**Key intrinsics patterns:**

| Operation | Intrinsic | What it does |
|-----------|-----------|-------------|
| Load aligned | `_mm256_load_ps(ptr)` | Load 8 **32-byte-aligned** floats — **crashes if misaligned** |
| Load unaligned | `_mm256_loadu_ps(ptr)` | Load 8 floats (any alignment) — safer default |
| Store | `_mm256_store_ps(ptr, v)` | Store 8 floats (aligned) |
| Stream store | `_mm256_stream_ps(ptr, v)` | Write bypassing cache — use for large write-once buffers |
| Add | `_mm256_add_ps(a, b)` | 8 parallel additions |
| Multiply | `_mm256_mul_ps(a, b)` | 8 parallel multiplications |
| **FMA** | `_mm256_fmadd_ps(a, b, c)` | 8 parallel `a*b + c` — **one instruction, not two; also more precise** |
| Broadcast | `_mm256_set1_ps(x)` | Fill all 8 lanes with same value |
| Horizontal add | `_mm256_hadd_ps(a, b)` | Add adjacent pairs within each 128-bit half |
| Compare | `_mm256_cmp_ps(a, b, _CMP_GT_OS)` | Per-lane compare → all-0s or all-1s mask |
| Blend | `_mm256_blendv_ps(a, b, mask)` | Select per-lane: `mask[i] ? b[i] : a[i]` |
| Permute (cross-lane) | `_mm256_permutevar8x32_ps(v, idx)` | Full cross-lane reorder by index vector |
| Gather | `_mm256_i32gather_ps(base, idx, 4)` | Load from non-contiguous addresses |

> **`_mm256_load_ps` alignment:** If `ptr` is not 32-byte aligned, this generates a segfault (`#GP` fault) at runtime — not a compile error. When in doubt, use `_mm256_loadu_ps` (the `u` = unaligned). On modern CPUs the performance difference is negligible.

> **FMA is the core of deep learning:** `_mm256_fmadd_ps(a, b, c)` computes `a*b + c` in a single fused instruction. GEMM (matrix multiplication), convolutions, and transformer attention inner loops are entirely composed of FMA. This is why FLOPS counts are usually reported as "FLOPS of FMA." On AVX2, 8-wide FMA at 2 ops/cycle = **16 GFLOPS/GHz per core**.

**Aligned memory:**

```cpp
// Aligned allocation (required for _mm256_load_ps)
alignas(32) float data[1024];

// Or dynamic allocation
float* data = (float*)aligned_alloc(32, 1024 * sizeof(float));
```

---

</details>

### 真实代码库中最常用的 Top 10 Intrinsics

对 GitHub 仓库（生产级 ML 框架、数据库、解析器）中 SIMD 用法的分析显示，在实际向量化代码的 80%+ 中反复出现的是同样的约 10 个 intrinsics。这些最值得优先记住。

| 排名 | Intrinsic | ISA | 作用 | 为何无处不在 |
|------|-----------|-----|--------------|---------------------|
| 1 | `_mm256_add_ps(a, b)` | AVX | 加 8 个 float32 | 每个 FP 循环的核心 |
| 2 | `_mm256_loadu_ps(ptr)` | AVX | 加载 8 个 float，非对齐 | 默认加载 —— 无对齐约束 |
| 3 | `_mm256_storeu_ps(ptr, v)` | AVX | 存储 8 个 float，非对齐 | 默认存储 |
| 4 | `_mm256_fmadd_ps(a, b, c)` | FMA+AVX2 | 一条指令完成 `a*b + c` | GEMM、卷积、attention |
| 5 | `_mm_add_ps(a, b)` | SSE | 加 4 个 float32（128-bit） | SSE 兼容代码、水平操作 |
| 6 | `_mm_set1_epi32(x)` | SSE2 | 将 int 广播到全部 4 个 lane | 加载整数常量 |
| 7 | `_mm256_cmp_ps(a, b, pred)` | AVX | 比较 → 位掩码 | 条件选择、NaN 处理 |
| 8 | `_mm_loadu_si128(ptr)` | SSE2 | 加载 128-bit 整数向量 | 字符串/字节处理 |
| 9 | `_mm_shuffle_epi32(v, imm)` | SSE2 | 重排 32-bit int lane | 数据重排、AoS→SoA |
| 10 | `_mm_popcnt_u32(x)` | SSE4.2 | 统计 32-bit int 中置位的位数 | 数据库过滤、Hamming 距离 |

**排名背后的规律：**

- **`_ps`（packed single）占主导** —— float32 是 ML 推理与科学计算的主力。优先学习 `_ps` 变体。
- **非对齐加载/存储更受青睐**（`_loadu_`、`_storeu_`）—— 在 Haswell 及更新的架构上，跨 cache line 加载的惩罚 ≤1 cycle。换来的代码简洁性值得。
- **SSE2 128-bit 仍然常见** —— 许多代码库出于兼容性，或为处理 128-bit 子操作（水平归约、余数循环、字节处理）而使用 SSE2。
- **`_mm_shuffle_epi32` 是整数重排的主力** —— 几乎出现在每一个 AoS→SoA 转换、哈希函数与字符串解析器中。
- **`_mm_popcnt_u32/u64` 是个异类** —— 严格来说它不是 SIMD 寄存器指令（作用于标量寄存器），但用到了 SSE4.2 这一 ISA 特性位，并在数据库与位操作代码中占主导（Hamming 距离、统计过滤器中置位的位数）。

```cpp
// The most common inner loop pattern in real ML code (ranks 2, 4, 3):
for (int i = 0; i < n; i += 8) {
    __m256 a = _mm256_loadu_ps(&A[i]);     // rank 2
    __m256 b = _mm256_loadu_ps(&B[i]);     // rank 2
    acc      = _mm256_fmadd_ps(a, b, acc); // rank 4
}
_mm256_storeu_ps(out, acc);                // rank 3
```

> **来源：** 使用统计数据来自 [simd.info intrinsic statistics](https://simd.info/blog/intrinsic_statistics_repositories_insights_and_patterns/) 以及对 GitHub 仓库（包括生产级 ML 框架、数据库与编解码器库）的分析。

---

### 按功能分类的 Intrinsics

换一个视角 —— 按你要做的事情分组，SSE（128-bit）与 AVX（256-bit）两个版本并排给出。解决问题时正是这样查阅它们。

#### 1. 数据搬移与初始化

```cpp
// Load — unaligned is the default today (negligible penalty on modern CPUs)
__m128 a = _mm_loadu_ps(ptr);       // SSE:  4 floats
__m256 b = _mm256_loadu_ps(ptr);    // AVX:  8 floats

// Store — write register back to memory
_mm_storeu_ps(ptr, a);              // SSE
_mm256_storeu_ps(ptr, b);           // AVX

// Broadcast — fill all lanes with one scalar (scaling, bias addition)
__m128 scale = _mm_set1_ps(2.0f);        // SSE:  [2, 2, 2, 2]
__m256 scale = _mm256_set1_ps(2.0f);     // AVX:  [2, 2, 2, 2, 2, 2, 2, 2]

// Set individual lanes (note: highest lane first)
__m256 v = _mm256_set_ps(7,6,5,4,3,2,1,0);   // reverse order
__m256 v = _mm256_setr_ps(0,1,2,3,4,5,6,7);  // natural order (r = reversed param order)
```

#### 2. 算术运算

```cpp
// Add / Subtract / Multiply / Divide
__m256 r = _mm256_add_ps(a, b);     // a[i] + b[i]
__m256 r = _mm256_sub_ps(a, b);     // a[i] - b[i]
__m256 r = _mm256_mul_ps(a, b);     // a[i] * b[i]
__m256 r = _mm256_div_ps(a, b);     // a[i] / b[i]  (slow — ~20 cycles)

// FMA — the most important arithmetic intrinsic
__m256 r = _mm256_fmadd_ps(a, b, c); // a[i]*b[i] + c[i], single instruction

// Integer arithmetic (add 8 × int32)
__m256i r = _mm256_add_epi32(a, b);
__m256i r = _mm256_mullo_epi32(a, b);  // low 32 bits of each 32×32 product
```

> **`_ps` = packed single（float32）。`_pd` = packed double。`_epi32` = packed int32。**
> 学会后缀体系后，每个 intrinsic 名称都会变得自解释。


<details>
<summary>English original</summary>

**Top 10 Most-Used Intrinsics in Real Codebases**

Analysis of SIMD usage across GitHub repositories (production ML frameworks, databases, parsers) shows the same ~10 intrinsics appear in 80%+ of vectorized code. These are the ones worth memorizing first.

| Rank | Intrinsic | ISA | What it does | Why it's everywhere |
|------|-----------|-----|--------------|---------------------|
| 1 | `_mm256_add_ps(a, b)` | AVX | Add 8 float32 | Core of every FP loop |
| 2 | `_mm256_loadu_ps(ptr)` | AVX | Load 8 floats, unaligned | Default load — no alignment constraint |
| 3 | `_mm256_storeu_ps(ptr, v)` | AVX | Store 8 floats, unaligned | Default store |
| 4 | `_mm256_fmadd_ps(a, b, c)` | FMA+AVX2 | `a*b + c` in one instruction | GEMM, convolution, attention |
| 5 | `_mm_add_ps(a, b)` | SSE | Add 4 float32 (128-bit) | SSE compat code, horizontal ops |
| 6 | `_mm_set1_epi32(x)` | SSE2 | Broadcast int to all 4 lanes | Loading integer constants |
| 7 | `_mm256_cmp_ps(a, b, pred)` | AVX | Compare → bitmask | Conditional selection, NaN handling |
| 8 | `_mm_loadu_si128(ptr)` | SSE2 | Load 128-bit integer vector | String/byte processing |
| 9 | `_mm_shuffle_epi32(v, imm)` | SSE2 | Reorder 32-bit int lanes | Data rearrangement, AoS→SoA |
| 10 | `_mm_popcnt_u32(x)` | SSE4.2 | Count set bits in 32-bit int | Database filters, Hamming distance |

**Patterns behind the rankings:**

- **`_ps` (packed single) dominates** — float32 is the workhorse of ML inference and scientific computing. Learn `_ps` variants first.
- **Unaligned loads/stores are preferred** (`_loadu_`, `_storeu_`) — on Haswell and newer, the penalty for cache-line-crossing loads is ≤1 cycle. The code simplicity is worth it.
- **SSE2 128-bit still common** — many codebases use SSE2 for compatibility or for 128-bit sub-operations (horizontal reduction, remainder loops, byte processing).
- **`_mm_shuffle_epi32` is the workhorse for integer rearrangement** — appears in virtually every AoS→SoA conversion, hash function, and string parser.
- **`_mm_popcnt_u32/u64` is an outlier** — not strictly a SIMD register instruction (operates on scalar registers), but uses the SSE4.2 ISA feature bit and dominates database and bit-manipulation code (Hamming distance, counting set bits in filters).

```cpp
// The most common inner loop pattern in real ML code (ranks 2, 4, 3):
for (int i = 0; i < n; i += 8) {
    __m256 a = _mm256_loadu_ps(&A[i]);     // rank 2
    __m256 b = _mm256_loadu_ps(&B[i]);     // rank 2
    acc      = _mm256_fmadd_ps(a, b, acc); // rank 4
}
_mm256_storeu_ps(out, acc);                // rank 3
```

> **Source:** Usage statistics from [simd.info intrinsic statistics](https://simd.info/blog/intrinsic_statistics_repositories_insights_and_patterns/) and analysis of GitHub repositories including production ML frameworks, databases, and codec libraries.

---

**Intrinsics by Functional Category**

A different lens — grouped by what you're trying to do, with both SSE (128-bit) and AVX (256-bit) versions side by side. This is how you look them up when solving a problem.

**1. Data Movement & Initialization**

```cpp
// Load — unaligned is the default today (negligible penalty on modern CPUs)
__m128 a = _mm_loadu_ps(ptr);       // SSE:  4 floats
__m256 b = _mm256_loadu_ps(ptr);    // AVX:  8 floats

// Store — write register back to memory
_mm_storeu_ps(ptr, a);              // SSE
_mm256_storeu_ps(ptr, b);           // AVX

// Broadcast — fill all lanes with one scalar (scaling, bias addition)
__m128 scale = _mm_set1_ps(2.0f);        // SSE:  [2, 2, 2, 2]
__m256 scale = _mm256_set1_ps(2.0f);     // AVX:  [2, 2, 2, 2, 2, 2, 2, 2]

// Set individual lanes (note: highest lane first)
__m256 v = _mm256_set_ps(7,6,5,4,3,2,1,0);   // reverse order
__m256 v = _mm256_setr_ps(0,1,2,3,4,5,6,7);  // natural order (r = reversed param order)
```

**2. Arithmetic**

```cpp
// Add / Subtract / Multiply / Divide
__m256 r = _mm256_add_ps(a, b);     // a[i] + b[i]
__m256 r = _mm256_sub_ps(a, b);     // a[i] - b[i]
__m256 r = _mm256_mul_ps(a, b);     // a[i] * b[i]
__m256 r = _mm256_div_ps(a, b);     // a[i] / b[i]  (slow — ~20 cycles)

// FMA — the most important arithmetic intrinsic
__m256 r = _mm256_fmadd_ps(a, b, c); // a[i]*b[i] + c[i], single instruction

// Integer arithmetic (add 8 × int32)
__m256i r = _mm256_add_epi32(a, b);
__m256i r = _mm256_mullo_epi32(a, b);  // low 32 bits of each 32×32 product
```

> **`_ps` = packed single (float32). `_pd` = packed double. `_epi32` = packed int32.**
> Learn the suffix system once and every intrinsic name becomes self-describing.

</details>

#### 3. 比较与选择（无分支条件）

SIMD 没有分支。基本模式：**compare → mask → blend**。

```cpp
// Step 1: Compare → produces per-lane all-1s (true) or all-0s (false)
__m256 mask = _mm256_cmp_ps(a, b, _CMP_GT_OS);   // mask[i] = a[i] > b[i] ? 0xFFFFFFFF : 0

// SSE equivalent with explicit predicate:
__m128 mask = _mm_cmplt_ps(a, b);                 // mask[i] = a[i] < b[i]

// Step 2: Blend — select elements using mask
__m256 result = _mm256_blendv_ps(if_false, if_true, mask);
// result[i] = mask[i] ? if_true[i] : if_false[i]

// Practical example: clamp to [lo, hi] without any branch
__m256 clamped = _mm256_min_ps(_mm256_max_ps(v, lo), hi);

// Practical example: absolute value without branch
__m256 sign_mask = _mm256_set1_ps(-0.0f);         // sign bit only
__m256 abs_v     = _mm256_andnot_ps(sign_mask, v); // clear sign bit = |v|
```

> **`_mm256_cmp_ps` 的比较谓词：**`_CMP_EQ_OS`、`_CMP_LT_OS`、`_CMP_LE_OS`、`_CMP_GT_OS`、`_CMP_GE_OS`、`_CMP_NEQ_OS`、`_CMP_UNORD_Q`（NaN 检查）。`_OS` 后缀 = ordered、signaling（遇 NaN 抛异常）。quiet（不抛异常）用 `_OQ`。

#### 4. 数据混洗与归约

```cpp
// Shuffle — rearrange within 128-bit half (compile-time imm8 control)
__m128 s = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3,2,1,0));  // SSE: pick 4 elements from a and b
__m256 s = _mm256_shuffle_ps(a, b, imm8);                // AVX: operates per 128-bit half!

// Horizontal add — adds adjacent pairs (use sparingly, ~3 cycles each)
__m128 h = _mm_hadd_ps(a, b);   // [a0+a1, a2+a3, b0+b1, b2+b3]

// Better horizontal sum pattern (see "Horizontal sum" section above)
// hadd is slow — only use it 1-2 times at the very end of a reduction loop
```

> **`_mm_hadd_ps` 警告：**Horizontal add 是最慢的 SIMD 指令之一（约 3-5 周期，而 `_mm_add_ps` 只要 0.5）。绝不要把它放进循环里。只在末尾用一次，把累加寄存器归约收敛掉 —— 或者更好的做法，用 Inspecting SIMD Registers 小节里的 `hsum` 惯用法。

---

### SSE 与 AVX 并排对照

| 类别 | SSE (128-bit, 4× float) | AVX (256-bit, 8× float) | 用途 |
|----------|------------------------|------------------------|---------|
| 加载 | `_mm_loadu_ps(ptr)` | `_mm256_loadu_ps(ptr)` | 从内存读取 |
| 存储 | `_mm_storeu_ps(ptr, v)` | `_mm256_storeu_ps(ptr, v)` | 写入内存 |
| 广播 | `_mm_set1_ps(x)` | `_mm256_set1_ps(x)` | 用标量填充所有 lane |
| 加法 | `_mm_add_ps(a, b)` | `_mm256_add_ps(a, b)` | 逐元素相加 |
| 乘法 | `_mm_mul_ps(a, b)` | `_mm256_mul_ps(a, b)` | 逐元素相乘 |
| FMA | `_mm_fmadd_ps(a, b, c)` | `_mm256_fmadd_ps(a, b, c)` | `a*b + c` |
| 比较 | `_mm_cmplt_ps(a, b)` | `_mm256_cmp_ps(a, b, pred)` | 向量化比较 → 掩码 |
| 混合 | `_mm_blendv_ps(a, b, m)` | `_mm256_blendv_ps(a, b, m)` | 向量化的 if/else |
| 混洗 | `_mm_shuffle_ps(a, b, i)` | `_mm256_shuffle_ps(a, b, i)` | 重排元素 |
| Hadd | `_mm_hadd_ps(a, b)` | `_mm256_hadd_ps(a, b)` | 相邻对相加 |

**何时用 SSE、何时用 AVX：**
- **AVX 换吞吐** —— 每条指令处理 2× 的数据，延迟相同
- **SSE 换兼容** —— 自 ~2003 年起，每颗 x86-64 CPU 都提供 SSE2
- **SSE 处理余数循环** —— 主 AVX 循环处理完 `n/8*8` 个元素后，用 SSE 或标量处理最后 `n%8` 个

---


<details>
<summary>English original</summary>

**3. Comparison & Selection (Branchless Conditionals)**

SIMD has no branches. The fundamental pattern: **compare → mask → blend**.

```cpp
// Step 1: Compare → produces per-lane all-1s (true) or all-0s (false)
__m256 mask = _mm256_cmp_ps(a, b, _CMP_GT_OS);   // mask[i] = a[i] > b[i] ? 0xFFFFFFFF : 0

// SSE equivalent with explicit predicate:
__m128 mask = _mm_cmplt_ps(a, b);                 // mask[i] = a[i] < b[i]

// Step 2: Blend — select elements using mask
__m256 result = _mm256_blendv_ps(if_false, if_true, mask);
// result[i] = mask[i] ? if_true[i] : if_false[i]

// Practical example: clamp to [lo, hi] without any branch
__m256 clamped = _mm256_min_ps(_mm256_max_ps(v, lo), hi);

// Practical example: absolute value without branch
__m256 sign_mask = _mm256_set1_ps(-0.0f);         // sign bit only
__m256 abs_v     = _mm256_andnot_ps(sign_mask, v); // clear sign bit = |v|
```

> **Comparison predicates for `_mm256_cmp_ps`:** `_CMP_EQ_OS`, `_CMP_LT_OS`, `_CMP_LE_OS`, `_CMP_GT_OS`, `_CMP_GE_OS`, `_CMP_NEQ_OS`, `_CMP_UNORD_Q` (NaN check). The `_OS` suffix = ordered, signaling (raises exception on NaN). Use `_OQ` for quiet (no exception).

**4. Data Shuffling & Reduction**

```cpp
// Shuffle — rearrange within 128-bit half (compile-time imm8 control)
__m128 s = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3,2,1,0));  // SSE: pick 4 elements from a and b
__m256 s = _mm256_shuffle_ps(a, b, imm8);                // AVX: operates per 128-bit half!

// Horizontal add — adds adjacent pairs (use sparingly, ~3 cycles each)
__m128 h = _mm_hadd_ps(a, b);   // [a0+a1, a2+a3, b0+b1, b2+b3]

// Better horizontal sum pattern (see "Horizontal sum" section above)
// hadd is slow — only use it 1-2 times at the very end of a reduction loop
```

> **`_mm_hadd_ps` warning:** Horizontal add is one of the slowest SIMD instructions (~3-5 cycles vs 0.5 for `_mm_add_ps`). Never put it inside a loop. Use it once at the end to collapse an accumulator register — or better, use the `hsum` idiom from the Inspecting SIMD Registers section.

---

**SSE vs AVX Side-by-Side Reference**

| Category | SSE (128-bit, 4× float) | AVX (256-bit, 8× float) | Purpose |
|----------|------------------------|------------------------|---------|
| Load | `_mm_loadu_ps(ptr)` | `_mm256_loadu_ps(ptr)` | Read from memory |
| Store | `_mm_storeu_ps(ptr, v)` | `_mm256_storeu_ps(ptr, v)` | Write to memory |
| Broadcast | `_mm_set1_ps(x)` | `_mm256_set1_ps(x)` | Fill all lanes with scalar |
| Add | `_mm_add_ps(a, b)` | `_mm256_add_ps(a, b)` | Element-wise addition |
| Multiply | `_mm_mul_ps(a, b)` | `_mm256_mul_ps(a, b)` | Element-wise multiply |
| FMA | `_mm_fmadd_ps(a, b, c)` | `_mm256_fmadd_ps(a, b, c)` | `a*b + c` |
| Compare | `_mm_cmplt_ps(a, b)` | `_mm256_cmp_ps(a, b, pred)` | Vectorized compare → mask |
| Blend | `_mm_blendv_ps(a, b, m)` | `_mm256_blendv_ps(a, b, m)` | Vectorized if/else |
| Shuffle | `_mm_shuffle_ps(a, b, i)` | `_mm256_shuffle_ps(a, b, i)` | Rearrange elements |
| Hadd | `_mm_hadd_ps(a, b)` | `_mm256_hadd_ps(a, b)` | Adjacent-pair add |

**When to use SSE vs AVX:**
- **AVX for throughput** — 2× the data per instruction, same latency
- **SSE for compatibility** — SSE2 is available on every x86-64 CPU since ~2003
- **SSE for remainder loops** — after the main AVX loop processes `n/8*8` elements, handle the last `n%8` with SSE or scalar

---

</details>

### 最小示例 — 逐个隔离每个内建函数

在组合内建函数之前，先看每个函数只做一件事：

```cpp
#include <immintrin.h>

// ── Data Movement ──────────────────────────────────────────────────────────

// loadu: load 8 floats from any address
float data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
__m256 v = _mm256_loadu_ps(data);           // v = [1, 2, 3, 4, 5, 6, 7, 8]

// set1: broadcast scalar to every lane
__m256 v_pi = _mm256_set1_ps(3.14f);        // v_pi = [3.14, 3.14, 3.14, 3.14, 3.14, 3.14, 3.14, 3.14]

// storeu: write register back to array
float out[8];
_mm256_storeu_ps(out, v);                   // out[] = {1, 2, 3, 4, 5, 6, 7, 8}

// ── Arithmetic ─────────────────────────────────────────────────────────────

__m256 v1 = _mm256_set1_ps(2.0f);           // [2, 2, 2, 2, 2, 2, 2, 2]
__m256 v2 = _mm256_set1_ps(3.0f);           // [3, 3, 3, 3, 3, 3, 3, 3]

__m256 sum     = _mm256_add_ps(v1, v2);     // [5, 5, 5, 5, 5, 5, 5, 5]
__m256 product = _mm256_mul_ps(v1, v2);     // [6, 6, 6, 6, 6, 6, 6, 6]
__m256 fma_res = _mm256_fmadd_ps(v1, v2, v1); // v1*v2 + v1 = [8, 8, 8, 8, 8, 8, 8, 8]

// ── Comparison & Selection (vectorized if/else) ────────────────────────────

__m256 a    = _mm256_setr_ps(1,5,3,7,2,6,4,8);
__m256 b    = _mm256_set1_ps(4.0f);
__m256 mask = _mm256_cmp_ps(a, b, _CMP_GT_OS); // mask[i] = a[i] > 4 ? 0xFFFFFFFF : 0
                                                // [0, 1, 0, 1, 0, 1, 0, 1]
__m256 result = _mm256_blendv_ps(b, a, mask);   // result[i] = mask[i] ? a[i] : b[i]
                                                // [4, 5, 4, 7, 4, 6, 4, 8]

// ── Shuffling ──────────────────────────────────────────────────────────────

__m256 sv = _mm256_setr_ps(0,1,2,3,4,5,6,7);
// _MM_SHUFFLE(d,c,b,a) → lane 0 = src[a], lane 1 = src[b], lane 2 = src[c], lane 3 = src[d]
__m256 shuffled = _mm256_shuffle_ps(sv, sv, _MM_SHUFFLE(0,1,2,3));
// Result: [3,2,1,0, 7,6,5,4] — reversed within each 128-bit half

// ── Horizontal add (slow — only at loop end) ───────────────────────────────

__m256 hv   = _mm256_setr_ps(1,2,3,4,5,6,7,8);
__m256 hsum = _mm256_hadd_ps(hv, hv);
// [1+2, 3+4, 1+2, 3+4 | 5+6, 7+8, 5+6, 7+8] = [3, 7, 3, 7, 11, 15, 11, 15]
```

---

### 完整示例：缩放向量加法

五个核心内建函数在一条贴近实际的循环中协同工作 — 这一模式用于神经网络激活值、图像处理和信号缩放：

```cpp
#include <immintrin.h>

// Computes: c[i] = (a[i] + b[i]) * scale  for all i
// Processes 8 elements per iteration using AVX
void scaled_add(const float* a, const float* b, float* c, float scale, int n) {
    // 1. set1: broadcast the scalar constant once, outside the loop
    __m256 v_scale = _mm256_set1_ps(scale);

    int i = 0;
    for (; i <= n - 8; i += 8) {
        // 2. loadu: load 8 elements from each input array
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);

        // 3. add_ps: element-wise addition
        __m256 vsum = _mm256_add_ps(va, vb);

        // 4. mul_ps: scale the result
        //    (alternatively: _mm256_fmadd_ps(va, v_scale, _mm256_mul_ps(vb, v_scale)))
        __m256 vres = _mm256_mul_ps(vsum, v_scale);

        // 5. storeu: write 8 results back to memory
        _mm256_storeu_ps(&c[i], vres);
    }

    // Scalar remainder for elements that don't fill a full AVX register
    for (; i < n; i++) {
        c[i] = (a[i] + b[i]) * scale;
    }
}
```

**它演示了什么：**
- `_mm256_set1_ps` 放在循环外 — 常量只算一次，每次迭代复用
- `_mm256_loadu_ps` — 无需对齐约束
- `_mm256_add_ps` → `_mm256_mul_ps` — 两个算术操作，各处理 8 个元素，背靠背执行
- `_mm256_storeu_ps` — 写回结果
- 余数循环 — 干净地处理 `n % 8` 个尾部元素

**FMA 版本** — 如果 `a` 和 `b` 已经分别缩放完成，`fmadd` 可把两个操作合并：
```cpp
// c[i] = a[i] * scale + b[i]
__m256 vres = _mm256_fmadd_ps(va, v_scale, vb);
```

---

### ARM NEON 内建函数（Jetson、Apple Silicon、移动端）

ARM NEON 是 ARM 处理器的 128 位 SIMD 扩展 — 它运行在 NVIDIA Jetson（Cortex-A78AE）、Apple Silicon（M 系列）、Qualcomm Snapdragon、AWS Graviton 以及每一款现代手机上。若在边缘部署 AI，NEON 就是你在 CPU 侧做预处理向量化的手段。


<details>
<summary>English original</summary>

**Minimal Examples — Each Intrinsic in Isolation**

Before combining intrinsics, see each one do exactly one thing:

```cpp
#include <immintrin.h>

// ── Data Movement ──────────────────────────────────────────────────────────

// loadu: load 8 floats from any address
float data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
__m256 v = _mm256_loadu_ps(data);           // v = [1, 2, 3, 4, 5, 6, 7, 8]

// set1: broadcast scalar to every lane
__m256 v_pi = _mm256_set1_ps(3.14f);        // v_pi = [3.14, 3.14, 3.14, 3.14, 3.14, 3.14, 3.14, 3.14]

// storeu: write register back to array
float out[8];
_mm256_storeu_ps(out, v);                   // out[] = {1, 2, 3, 4, 5, 6, 7, 8}

// ── Arithmetic ─────────────────────────────────────────────────────────────

__m256 v1 = _mm256_set1_ps(2.0f);           // [2, 2, 2, 2, 2, 2, 2, 2]
__m256 v2 = _mm256_set1_ps(3.0f);           // [3, 3, 3, 3, 3, 3, 3, 3]

__m256 sum     = _mm256_add_ps(v1, v2);     // [5, 5, 5, 5, 5, 5, 5, 5]
__m256 product = _mm256_mul_ps(v1, v2);     // [6, 6, 6, 6, 6, 6, 6, 6]
__m256 fma_res = _mm256_fmadd_ps(v1, v2, v1); // v1*v2 + v1 = [8, 8, 8, 8, 8, 8, 8, 8]

// ── Comparison & Selection (vectorized if/else) ────────────────────────────

__m256 a    = _mm256_setr_ps(1,5,3,7,2,6,4,8);
__m256 b    = _mm256_set1_ps(4.0f);
__m256 mask = _mm256_cmp_ps(a, b, _CMP_GT_OS); // mask[i] = a[i] > 4 ? 0xFFFFFFFF : 0
                                                // [0, 1, 0, 1, 0, 1, 0, 1]
__m256 result = _mm256_blendv_ps(b, a, mask);   // result[i] = mask[i] ? a[i] : b[i]
                                                // [4, 5, 4, 7, 4, 6, 4, 8]

// ── Shuffling ──────────────────────────────────────────────────────────────

__m256 sv = _mm256_setr_ps(0,1,2,3,4,5,6,7);
// _MM_SHUFFLE(d,c,b,a) → lane 0 = src[a], lane 1 = src[b], lane 2 = src[c], lane 3 = src[d]
__m256 shuffled = _mm256_shuffle_ps(sv, sv, _MM_SHUFFLE(0,1,2,3));
// Result: [3,2,1,0, 7,6,5,4] — reversed within each 128-bit half

// ── Horizontal add (slow — only at loop end) ───────────────────────────────

__m256 hv   = _mm256_setr_ps(1,2,3,4,5,6,7,8);
__m256 hsum = _mm256_hadd_ps(hv, hv);
// [1+2, 3+4, 1+2, 3+4 | 5+6, 7+8, 5+6, 7+8] = [3, 7, 3, 7, 11, 15, 11, 15]
```

---

**Complete Example: Scaled Vector Addition**

All five core intrinsics working together in a realistic loop — the pattern used in neural network activations, image processing, and signal scaling:

```cpp
#include <immintrin.h>

// Computes: c[i] = (a[i] + b[i]) * scale  for all i
// Processes 8 elements per iteration using AVX
void scaled_add(const float* a, const float* b, float* c, float scale, int n) {
    // 1. set1: broadcast the scalar constant once, outside the loop
    __m256 v_scale = _mm256_set1_ps(scale);

    int i = 0;
    for (; i <= n - 8; i += 8) {
        // 2. loadu: load 8 elements from each input array
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);

        // 3. add_ps: element-wise addition
        __m256 vsum = _mm256_add_ps(va, vb);

        // 4. mul_ps: scale the result
        //    (alternatively: _mm256_fmadd_ps(va, v_scale, _mm256_mul_ps(vb, v_scale)))
        __m256 vres = _mm256_mul_ps(vsum, v_scale);

        // 5. storeu: write 8 results back to memory
        _mm256_storeu_ps(&c[i], vres);
    }

    // Scalar remainder for elements that don't fill a full AVX register
    for (; i < n; i++) {
        c[i] = (a[i] + b[i]) * scale;
    }
}
```

**What this demonstrates:**
- `_mm256_set1_ps` outside the loop — compute constants once, reuse every iteration
- `_mm256_loadu_ps` — no alignment constraint needed
- `_mm256_add_ps` → `_mm256_mul_ps` — two arithmetic ops, 8 elements each, back-to-back
- `_mm256_storeu_ps` — write results
- Remainder loop — handle `n % 8` trailing elements cleanly

**FMA version** — if `a` and `b` have already been scaled separately, `fmadd` collapses two ops:
```cpp
// c[i] = a[i] * scale + b[i]
__m256 vres = _mm256_fmadd_ps(va, v_scale, vb);
```

---

**ARM NEON Intrinsics (Jetson, Apple Silicon, Mobile)**

ARM NEON is the 128-bit SIMD extension for ARM processors — it's what runs on NVIDIA Jetson (Cortex-A78AE), Apple Silicon (M-series), Qualcomm Snapdragon, AWS Graviton, and every modern phone. If you deploy AI at the edge, NEON is how you vectorize CPU-side preprocessing.

</details>

#### NEON vs x86 SIMD

| | x86 SSE | x86 AVX2 | ARM NEON | ARM SVE/SVE2 |
|---|---------|----------|----------|--------------|
| 位宽 | 128-bit | 256-bit | **128-bit** | 128–2048-bit（可伸缩） |
| FP32 通道数 | 4 | 8 | **4** | 4–64 |
| 寄存器 | 16（XMM） | 16（YMM） | **32（V0–V31）** | 32（Z0–Z31） |
| FMA | SSE：否，FMA3：是 | 是 | **是（vfmaq）** | 是 |
| 头文件 | `<immintrin.h>` | `<immintrin.h>` | **`<arm_neon.h>`** | `<arm_sve.h>` |
| 编译器 | gcc/clang `-msse4.2` | `-mavx2` | **`-march=armv8-a`** | `-march=armv8.2-a+sve` |

NEON 有 **32 个寄存器**（x86 AVX2 为 16 个）——寄存器更多意味着编译器可以让更多值保持活跃而不发生溢出。这在一定程度上补偿了 128-bit 位宽较窄的劣势。

#### NEON 数据类型

```cpp
#include <arm_neon.h>

// Type naming: <base><bits>x<lanes>_t
float32x4_t    // 4× FP32 in a 128-bit register
float16x8_t    // 8× FP16 in a 128-bit register
int32x4_t      // 4× INT32
int8x16_t      // 16× INT8
uint8x16_t     // 16× UINT8

// "×2" variants — register pairs (used for interleaved load/store)
float32x4x2_t  // pair of float32x4_t (8 floats across 2 registers)
```

#### NEON Intrinsics — 最常用的 15 个

| Intrinsic | 操作 | x86 对应项 |
|-----------|-----------|----------------|
| `vld1q_f32(ptr)` | 加载 4 个 float | `_mm_loadu_ps` |
| `vst1q_f32(ptr, v)` | 存储 4 个 float | `_mm_storeu_ps` |
| `vaddq_f32(a, b)` | a + b（4 通道） | `_mm_add_ps` |
| `vsubq_f32(a, b)` | a − b | `_mm_sub_ps` |
| `vmulq_f32(a, b)` | a × b | `_mm_mul_ps` |
| `vfmaq_f32(acc, a, b)` | **acc + a×b（FMA）** | `_mm_fmadd_ps` |
| `vmaxq_f32(a, b)` | max(a, b) | `_mm_max_ps` |
| `vminq_f32(a, b)` | min(a, b) | `_mm_min_ps` |
| `vdupq_n_f32(s)` | 将标量广播到全部 4 个通道 | `_mm_set1_ps` |
| `vmovq_n_f32(0)` | 零向量 | `_mm_setzero_ps` |
| `vcvtq_f32_s32(v)` | INT32→FP32 转换 | `_mm_cvtepi32_ps` |
| `vcvtq_s32_f32(v)` | FP32→INT32 转换 | `_mm_cvtps_epi32` |
| `vaddvq_f32(v)` | **水平求和（全部 4 个通道）** | 无单条指令！ |
| `vmaxvq_f32(v)` | **水平求最大值** | 无单条指令！ |
| `vld2q_f32(ptr)` | **交错加载**（AoS→SoA） | 无对应项 |

> **NEON 优势：** `vaddvq_f32` 用单条指令完成水平求和。在 x86 上，水平归约需要 3–4 步 shuffle+add。NEON 还有 `vld2q`/`vld3q`/`vld4q` 可在加载时自动完成 AoS→SoA 反交错——对 RGB/RGBA 图像处理极为有用。

#### 示例 1：向量加法（NEON）

```cpp
#include <arm_neon.h>

void add_vectors_neon(const float* a, const float* b, float* c, int n) {
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        float32x4_t vc = vaddq_f32(va, vb);
        vst1q_f32(&c[i], vc);
    }
    // Scalar remainder
    for (; i < n; i++) c[i] = a[i] + b[i];
}
```

```bash
# Compile on Jetson / ARM64
g++ -O2 -march=armv8.2-a+fp16 -o vecadd vecadd.cpp
```

#### 示例 2：使用 FMA 的点积

```cpp
float dot_neon(const float* a, const float* b, int n) {
    float32x4_t acc = vmovq_n_f32(0.0f);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        acc = vfmaq_f32(acc, va, vb);    // acc += va * vb (fused multiply-add)
    }
    float result = vaddvq_f32(acc);       // horizontal sum — single instruction!
    // Scalar remainder
    for (; i < n; i++) result += a[i] * b[i];
    return result;
}
```

与 x86 对比：末尾的 `vaddvq_f32` 取代了 4 条指令的水平求和 shuffle 舞（`hadd → hadd → extract`）。

#### 示例 3：ReLU 激活函数（INT8，16 通道）

INT8 量化推理在 Jetson 上至关重要——NEON 每条指令处理 16 个 INT8 元素：

```cpp
void relu_int8_neon(const int8_t* input, int8_t* output, int n) {
    int8x16_t zero = vmovq_n_s8(0);
    int i = 0;
    for (; i + 16 <= n; i += 16) {
        int8x16_t v = vld1q_s8(&input[i]);
        int8x16_t result = vmaxq_s8(v, zero);   // max(v, 0) = ReLU, 16 elements
        vst1q_s8(&output[i], result);
    }
    for (; i < n; i++) output[i] = input[i] > 0 ? input[i] : 0;
}
```

每条指令 16 次 ReLU 运算——这就是 ARM 上量化 INT8 推理如此之快的原因。


<details>
<summary>English original</summary>

**NEON vs x86 SIMD**

| | x86 SSE | x86 AVX2 | ARM NEON | ARM SVE/SVE2 |
|---|---------|----------|----------|--------------|
| Width | 128-bit | 256-bit | **128-bit** | 128–2048-bit (scalable) |
| FP32 lanes | 4 | 8 | **4** | 4–64 |
| Registers | 16 (XMM) | 16 (YMM) | **32 (V0–V31)** | 32 (Z0–Z31) |
| FMA | SSE: No, FMA3: Yes | Yes | **Yes (vfmaq)** | Yes |
| Header | `<immintrin.h>` | `<immintrin.h>` | **`<arm_neon.h>`** | `<arm_sve.h>` |
| Compiler | gcc/clang `-msse4.2` | `-mavx2` | **`-march=armv8-a`** | `-march=armv8.2-a+sve` |

NEON has **32 registers** (vs 16 on x86 AVX2) — more registers means the compiler can keep more values live without spilling. This partly compensates for the narrower 128-bit width.

**NEON Data Types**

```cpp
#include <arm_neon.h>

// Type naming: <base><bits>x<lanes>_t
float32x4_t    // 4× FP32 in a 128-bit register
float16x8_t    // 8× FP16 in a 128-bit register
int32x4_t      // 4× INT32
int8x16_t      // 16× INT8
uint8x16_t     // 16× UINT8

// "×2" variants — register pairs (used for interleaved load/store)
float32x4x2_t  // pair of float32x4_t (8 floats across 2 registers)
```

**NEON Intrinsics — Top 15 Most Used**

| Intrinsic | Operation | x86 equivalent |
|-----------|-----------|----------------|
| `vld1q_f32(ptr)` | Load 4 floats | `_mm_loadu_ps` |
| `vst1q_f32(ptr, v)` | Store 4 floats | `_mm_storeu_ps` |
| `vaddq_f32(a, b)` | a + b (4-wide) | `_mm_add_ps` |
| `vsubq_f32(a, b)` | a − b | `_mm_sub_ps` |
| `vmulq_f32(a, b)` | a × b | `_mm_mul_ps` |
| `vfmaq_f32(acc, a, b)` | **acc + a×b (FMA)** | `_mm_fmadd_ps` |
| `vmaxq_f32(a, b)` | max(a, b) | `_mm_max_ps` |
| `vminq_f32(a, b)` | min(a, b) | `_mm_min_ps` |
| `vdupq_n_f32(s)` | Broadcast scalar to all 4 lanes | `_mm_set1_ps` |
| `vmovq_n_f32(0)` | Zero vector | `_mm_setzero_ps` |
| `vcvtq_f32_s32(v)` | INT32→FP32 convert | `_mm_cvtepi32_ps` |
| `vcvtq_s32_f32(v)` | FP32→INT32 convert | `_mm_cvtps_epi32` |
| `vaddvq_f32(v)` | **Horizontal sum (all 4 lanes)** | No single instruction! |
| `vmaxvq_f32(v)` | **Horizontal max** | No single instruction! |
| `vld2q_f32(ptr)` | **Interleaved load** (AoS→SoA) | No equivalent |

> **NEON advantage:** `vaddvq_f32` does a horizontal sum in a single instruction. On x86, horizontal reduction requires 3–4 shuffle+add steps. NEON also has `vld2q`/`vld3q`/`vld4q` for automatic AoS→SoA deinterleaving on load — extremely useful for RGB/RGBA image processing.

**Example 1: Vector Addition (NEON)**

```cpp
#include <arm_neon.h>

void add_vectors_neon(const float* a, const float* b, float* c, int n) {
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        float32x4_t vc = vaddq_f32(va, vb);
        vst1q_f32(&c[i], vc);
    }
    // Scalar remainder
    for (; i < n; i++) c[i] = a[i] + b[i];
}
```

```bash
# Compile on Jetson / ARM64
g++ -O2 -march=armv8.2-a+fp16 -o vecadd vecadd.cpp
```

**Example 2: Dot Product with FMA**

```cpp
float dot_neon(const float* a, const float* b, int n) {
    float32x4_t acc = vmovq_n_f32(0.0f);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        acc = vfmaq_f32(acc, va, vb);    // acc += va * vb (fused multiply-add)
    }
    float result = vaddvq_f32(acc);       // horizontal sum — single instruction!
    // Scalar remainder
    for (; i < n; i++) result += a[i] * b[i];
    return result;
}
```

Compare to x86: the `vaddvq_f32` at the end replaces the 4-instruction horizontal sum shuffle dance (`hadd → hadd → extract`).

**Example 3: ReLU Activation (INT8, 16-wide)**

INT8 quantized inference is critical on Jetson — NEON processes 16 INT8 elements per instruction:

```cpp
void relu_int8_neon(const int8_t* input, int8_t* output, int n) {
    int8x16_t zero = vmovq_n_s8(0);
    int i = 0;
    for (; i + 16 <= n; i += 16) {
        int8x16_t v = vld1q_s8(&input[i]);
        int8x16_t result = vmaxq_s8(v, zero);   // max(v, 0) = ReLU, 16 elements
        vst1q_s8(&output[i], result);
    }
    for (; i < n; i++) output[i] = input[i] > 0 ? input[i] : 0;
}
```

16 ReLU operations per instruction — this is why quantized INT8 inference on ARM is so fast.

</details>

#### 示例 4：RGB 解交织（用 vld3q 做 AoS→SoA）

Jetson 上的图像预处理往往从 RGBRGBRGB...（AoS）开始。NEON 的 `vld3q` 会自动解交织：

```cpp
void rgb_to_planar(const uint8_t* rgb, uint8_t* r, uint8_t* g, uint8_t* b, int npixels) {
    int i = 0;
    for (; i + 16 <= npixels; i += 16) {
        // Load 48 bytes (16 pixels × 3 channels) and deinterleave
        uint8x16x3_t pixel = vld3q_u8(&rgb[i * 3]);
        //   pixel.val[0] = R R R R R R R R R R R R R R R R  (16 red values)
        //   pixel.val[1] = G G G G G G G G G G G G G G G G  (16 green values)
        //   pixel.val[2] = B B B B B B B B B B B B B B B B  (16 blue values)

        vst1q_u8(&r[i], pixel.val[0]);
        vst1q_u8(&g[i], pixel.val[1]);
        vst1q_u8(&b[i], pixel.val[2]);
    }
    // Scalar remainder
    for (; i < npixels; i++) {
        r[i] = rgb[i*3]; g[i] = rgb[i*3+1]; b[i] = rgb[i*3+2];
    }
}
```

在 x86 上，这种解交织需要多条 shuffle/permute/blend 指令。NEON 用一次加载就完成。

#### 示例 5：FP16 向量加法（Jetson + Apple Silicon）

ARMv8.2-a 加入了原生 FP16 算术 —— 对 AI 推理至关重要：

```cpp
#include <arm_neon.h>

void add_fp16(const __fp16* a, const __fp16* b, __fp16* c, int n) {
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        float16x8_t va = vld1q_f16(&a[i]);     // Load 8× FP16
        float16x8_t vb = vld1q_f16(&b[i]);
        float16x8_t vc = vaddq_f16(va, vb);    // 8-wide FP16 add
        vst1q_f16(&c[i], vc);
    }
    for (; i < n; i++) c[i] = a[i] + b[i];
}
```

```bash
# Must enable FP16 arithmetic
g++ -O2 -march=armv8.2-a+fp16 -o fp16add fp16add.cpp
```

每个 128 位寄存器装 8 个 FP16 值 —— 每寄存器吞吐与 4× FP32 相同，但内存流量减半。

#### 示例 6：分块 4×4 矩阵乘法（NEON）

小矩阵乘法在端侧 Transformer attention 中很常见：

```cpp
void matmul_4x4_neon(const float* A, const float* B, float* C) {
    // Load all 4 columns of B
    float32x4_t b0 = vld1q_f32(&B[0]);
    float32x4_t b1 = vld1q_f32(&B[4]);
    float32x4_t b2 = vld1q_f32(&B[8]);
    float32x4_t b3 = vld1q_f32(&B[12]);

    for (int i = 0; i < 4; i++) {
        float32x4_t a_row = vld1q_f32(&A[i * 4]);

        // C[i] = A[i,0]*B[0] + A[i,1]*B[1] + A[i,2]*B[2] + A[i,3]*B[3]
        float32x4_t c = vmulq_laneq_f32(b0, a_row, 0);      // broadcast A[i,0] × B col 0
        c = vfmaq_laneq_f32(c, b1, a_row, 1);                // += A[i,1] × B col 1
        c = vfmaq_laneq_f32(c, b2, a_row, 2);                // += A[i,2] × B col 2
        c = vfmaq_laneq_f32(c, b3, a_row, 3);                // += A[i,3] × B col 3

        vst1q_f32(&C[i * 4], c);
    }
}
```

`vmulq_laneq_f32(vec, src, lane)` 广播 `src` 的一个 lane，并与 `vec` 的全部 4 个 lane 相乘。`vfmaq_laneq_f32` 用融合乘加做同样的事。这个模式是 ARM 上更大分块 GEMM 的基本构件。

#### NEON 与 AVX2 对比

| | AVX2 (x86) | NEON (ARM) |
|---|------------|------------|
| 寄存器宽度 | 256-bit | 128-bit |
| FP32 lane 数 | 8 | 4 |
| 寄存器数 | 16 | **32** |
| 水平求和 | 3–4 条指令 | **1 条指令**（`vaddvq`） |
| AoS 解交织 | 复杂的 shuffle | **1 条指令**（`vld3q/vld4q`） |
| 原生 FP16 | 否（需转成 FP32） | **是**（ARMv8.2-a） |
| 跨 lane 陷阱 | 128-bit lane 边界 | 无（真正的 128-bit） |
| 典型平台 | 服务器、工作站 | **Jetson、手机、Apple Silicon、Graviton** |

**何时用 NEON，何时交给编译器向量化：** 与 x86 同样的规则 —— 先试自动向量化（`-O2 -march=armv8.2-a`）。当编译器失败时（访问模式复杂、数据交织、FP16 算术），或需要性能有保证时（Jetson 上的实时推理），改用手写 NEON intrinsics。

#### ARM SVE/SVE2 —— 未来

SVE（Scalable Vector Extension）是 ARM 对定宽问题的回答。SVE 寄存器不是 128 位，而是**可变宽度**（128–2048 位，按芯片设定）。代码无需重新编译即可在任意宽度上运行：

```cpp
#include <arm_sve.h>

void add_sve(const float* a, const float* b, float* c, int n) {
    for (int i = 0; i < n; i += svcntw()) {           // svcntw() = FP32 lanes at runtime
        svbool_t pred = svwhilelt_b32(i, n);           // predicate mask for bounds
        svfloat32_t va = svld1(pred, &a[i]);
        svfloat32_t vb = svld1(pred, &b[i]);
        svst1(pred, &c[i], svadd_f32_m(pred, va, vb));
    }
}
// Same binary runs on 128-bit (Neoverse N1) and 256-bit (Neoverse V2) and future 512-bit
```

SVE2 已在 Neoverse V2（AWS Graviton4）、Apple M4 以及即将推出的 Jetson 上提供。NEON 的知识可直接迁移 —— SVE 就是带可伸缩宽度和谓词化的 NEON。

---


<details>
<summary>English original</summary>

**Example 4: RGB Deinterleave (AoS→SoA with vld3q)**

Image preprocessing on Jetson often starts with RGBRGBRGB... (AoS). NEON's `vld3q` deinterleaves automatically:

```cpp
void rgb_to_planar(const uint8_t* rgb, uint8_t* r, uint8_t* g, uint8_t* b, int npixels) {
    int i = 0;
    for (; i + 16 <= npixels; i += 16) {
        // Load 48 bytes (16 pixels × 3 channels) and deinterleave
        uint8x16x3_t pixel = vld3q_u8(&rgb[i * 3]);
        //   pixel.val[0] = R R R R R R R R R R R R R R R R  (16 red values)
        //   pixel.val[1] = G G G G G G G G G G G G G G G G  (16 green values)
        //   pixel.val[2] = B B B B B B B B B B B B B B B B  (16 blue values)

        vst1q_u8(&r[i], pixel.val[0]);
        vst1q_u8(&g[i], pixel.val[1]);
        vst1q_u8(&b[i], pixel.val[2]);
    }
    // Scalar remainder
    for (; i < npixels; i++) {
        r[i] = rgb[i*3]; g[i] = rgb[i*3+1]; b[i] = rgb[i*3+2];
    }
}
```

On x86, this deinterleave requires multiple shuffle/permute/blend instructions. NEON does it in one load.

**Example 5: FP16 Vector Addition (Jetson + Apple Silicon)**

ARMv8.2-a added native FP16 arithmetic — critical for AI inference:

```cpp
#include <arm_neon.h>

void add_fp16(const __fp16* a, const __fp16* b, __fp16* c, int n) {
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        float16x8_t va = vld1q_f16(&a[i]);     // Load 8× FP16
        float16x8_t vb = vld1q_f16(&b[i]);
        float16x8_t vc = vaddq_f16(va, vb);    // 8-wide FP16 add
        vst1q_f16(&c[i], vc);
    }
    for (; i < n; i++) c[i] = a[i] + b[i];
}
```

```bash
# Must enable FP16 arithmetic
g++ -O2 -march=armv8.2-a+fp16 -o fp16add fp16add.cpp
```

8 FP16 values per 128-bit register — same throughput per register as 4× FP32, but half the memory traffic.

**Example 6: Tiled 4×4 Matrix Multiply (NEON)**

Small matrix multiply is common in on-device transformer attention:

```cpp
void matmul_4x4_neon(const float* A, const float* B, float* C) {
    // Load all 4 columns of B
    float32x4_t b0 = vld1q_f32(&B[0]);
    float32x4_t b1 = vld1q_f32(&B[4]);
    float32x4_t b2 = vld1q_f32(&B[8]);
    float32x4_t b3 = vld1q_f32(&B[12]);

    for (int i = 0; i < 4; i++) {
        float32x4_t a_row = vld1q_f32(&A[i * 4]);

        // C[i] = A[i,0]*B[0] + A[i,1]*B[1] + A[i,2]*B[2] + A[i,3]*B[3]
        float32x4_t c = vmulq_laneq_f32(b0, a_row, 0);      // broadcast A[i,0] × B col 0
        c = vfmaq_laneq_f32(c, b1, a_row, 1);                // += A[i,1] × B col 1
        c = vfmaq_laneq_f32(c, b2, a_row, 2);                // += A[i,2] × B col 2
        c = vfmaq_laneq_f32(c, b3, a_row, 3);                // += A[i,3] × B col 3

        vst1q_f32(&C[i * 4], c);
    }
}
```

`vmulq_laneq_f32(vec, src, lane)` broadcasts one lane of `src` and multiplies with all 4 lanes of `vec`. `vfmaq_laneq_f32` does the same with fused multiply-add. This pattern is the building block of larger tiled GEMM on ARM.

**NEON vs AVX2 Summary**

| | AVX2 (x86) | NEON (ARM) |
|---|------------|------------|
| Register width | 256-bit | 128-bit |
| FP32 lanes | 8 | 4 |
| Registers | 16 | **32** |
| Horizontal sum | 3–4 instructions | **1 instruction** (`vaddvq`) |
| AoS deinterleave | Complex shuffles | **1 instruction** (`vld3q/vld4q`) |
| FP16 native | No (convert to FP32) | **Yes** (ARMv8.2-a) |
| Cross-lane gotchas | 128-bit lane boundary | None (true 128-bit) |
| Typical platform | Server, workstation | **Jetson, phone, Apple Silicon, Graviton** |

**When to use NEON vs let the compiler vectorize:** Same rule as x86 — try auto-vectorization first (`-O2 -march=armv8.2-a`). Use manual NEON intrinsics when the compiler fails (complex access patterns, interleaved data, FP16 arithmetic) or when you need guaranteed performance (real-time inference on Jetson).

**ARM SVE/SVE2 — The Future**

SVE (Scalable Vector Extension) is ARM's answer to the fixed-width problem. Instead of 128 bits, SVE registers are **variable width** (128–2048 bits, set per chip). Your code works on any width without recompiling:

```cpp
#include <arm_sve.h>

void add_sve(const float* a, const float* b, float* c, int n) {
    for (int i = 0; i < n; i += svcntw()) {           // svcntw() = FP32 lanes at runtime
        svbool_t pred = svwhilelt_b32(i, n);           // predicate mask for bounds
        svfloat32_t va = svld1(pred, &a[i]);
        svfloat32_t vb = svld1(pred, &b[i]);
        svst1(pred, &c[i], svadd_f32_m(pred, va, vb));
    }
}
// Same binary runs on 128-bit (Neoverse N1) and 256-bit (Neoverse V2) and future 512-bit
```

SVE2 is available on Neoverse V2 (AWS Graviton4), Apple M4, and upcoming Jetson. NEON knowledge transfers directly — SVE is NEON with scalable width and predication.

---

</details>

### 跨 lane 限制（AVX2 的坑）

大多数 AVX2 的“256-bit”指令实际上按 **两个独立的 128-bit 半宽**执行。这是 AVX2 中最令人意外的架构怪癖，当你期望完整的跨 lane 操作时，会引发难以察觉的 bug。

```cpp
// shuffle_ps looks like it works on 256-bit, but it only shuffles within each 128-bit half:
__m256 v = {7, 6, 5, 4, 3, 2, 1, 0};  // lanes 0-7
__m256 s = _mm256_shuffle_ps(v, v, 0b00011011);
// Result: {4,5,6,7, 0,1,2,3} — reversed within each half, NOT across the full register

// To truly cross lanes, you need explicit cross-lane moves:
// Swap the two 128-bit halves:
__m256 swapped = _mm256_permute2f128_ps(v, v, 0x01);

// Full cross-lane element reorder (AVX2) — the most flexible option:
__m256i idx = _mm256_set_epi32(0, 1, 2, 3, 4, 5, 6, 7);  // reverse order
__m256 reversed = _mm256_permutevar8x32_ps(v, idx);       // true 8-element permute
```

| Operation | Cross-lane? | Notes |
|-----------|-------------|-------|
| `_mm256_shuffle_ps` | 否 | 仅在各自的 128-bit 半宽内 |
| `_mm256_hadd_ps` | 否 | 各半宽内相邻成对 |
| `_mm256_permute2f128_ps` | 是 | 交换/复制 128-bit 半宽 |
| `_mm256_permutevar8x32_ps` | 是 | 完整 8 元素 permute（仅 AVX2） |

> **经验法则：** 如果某个 intrinsic 标称“256-bit”，但在 AVX（而非 AVX2）中就已存在，就要怀疑它按两个 128-bit 半宽运作。用 [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/) 或 [uops.info](https://uops.info/) 验证。

---

### 查看 SIMD 寄存器（调试）

你无法 `printf` 一个 `__m256`。标准做法是用 union，这样可以逐个读取 lane：

```cpp
// Union for safe lane inspection (from hands-on-simd-programming)
union float8 {
    __m256  v;
    float   a[8];
    float8() : v(_mm256_setzero_ps()) {}
    float8(__m256 x) : v(x) {}
};

// Usage
float8 result(_mm256_fmadd_ps(va, vb, vc));
printf("lane 0: %f, lane 3: %f\n", result.a[0], result.a[3]);

// Or with structured binding in C++17:
auto [l0,l1,l2,l3,l4,l5,l6,l7] = result.a;
```

**水平求和 —— 将向量归约成标量：**

这个模式随处可见：点积、attention 分数、归约。

```cpp
// Sum all 8 floats in a __m256 to a scalar
float hsum(__m256 v) {
    // Step 1: add pairs within each 128-bit half → [a+e, b+f, c+g, d+h | a+e, b+f, c+g, d+h]
    __m128 lo  = _mm256_castps256_ps128(v);          // lower 128 bits
    __m128 hi  = _mm256_extractf128_ps(v, 1);        // upper 128 bits
    __m128 sum = _mm_add_ps(lo, hi);                 // add the two halves
    // Step 2: horizontal adds until scalar
    sum = _mm_hadd_ps(sum, sum);                     // [a+b+e+f, c+d+g+h, ...]
    sum = _mm_hadd_ps(sum, sum);                     // [total, total, ...]
    return _mm_cvtss_f32(sum);                       // extract lane 0
}
```

**大块一次性写缓冲区的流存储：**

```cpp
// Non-temporal store: bypasses CPU cache entirely
// Use when writing large output buffers you won't read back soon
// (e.g., preprocessing output, frame buffer writes)
for (int i = 0; i < n; i += 8) {
    __m256 result = /* compute */;
    _mm256_stream_ps(&out[i], result);   // skip cache, write direct to memory
}
_mm_sfence();  // memory fence — ensure all stream writes are visible

// When to use: write-once buffers > L3 cache size
// When NOT: if you'll read the data back soon (defeats the purpose)
```

**Gather —— 从非连续地址加载：**

```cpp
// Gather: load from base_ptr + indices[i] * scale
float table[1024] = { /* lookup table */ };
__m256i indices = _mm256_set_epi32(7, 3, 15, 0, 42, 8, 1, 100);  // arbitrary indices
__m256 gathered = _mm256_i32gather_ps(table, indices, 4);          // scale=4 (sizeof float)

// Note: gather throughput ≈ scalar loads — no bandwidth benefit for the gather itself.
// The value: the *computation* after gather runs vectorized (8 FMAs instead of 8 scalar ops).
// Good for: embedding lookups, sparse vector ops, non-sequential access patterns.
```

---


<details>
<summary>English original</summary>

**Cross-Lane Limitations (The AVX2 Gotcha)**

Most AVX2 "256-bit" instructions actually execute as **two independent 128-bit halves**. This is the single most surprising architectural quirk in AVX2 and causes subtle bugs when you expect full cross-lane operations.

```cpp
// shuffle_ps looks like it works on 256-bit, but it only shuffles within each 128-bit half:
__m256 v = {7, 6, 5, 4, 3, 2, 1, 0};  // lanes 0-7
__m256 s = _mm256_shuffle_ps(v, v, 0b00011011);
// Result: {4,5,6,7, 0,1,2,3} — reversed within each half, NOT across the full register

// To truly cross lanes, you need explicit cross-lane moves:
// Swap the two 128-bit halves:
__m256 swapped = _mm256_permute2f128_ps(v, v, 0x01);

// Full cross-lane element reorder (AVX2) — the most flexible option:
__m256i idx = _mm256_set_epi32(0, 1, 2, 3, 4, 5, 6, 7);  // reverse order
__m256 reversed = _mm256_permutevar8x32_ps(v, idx);       // true 8-element permute
```

| Operation | Cross-lane? | Notes |
|-----------|-------------|-------|
| `_mm256_shuffle_ps` | No | Within each 128-bit half only |
| `_mm256_hadd_ps` | No | Adjacent pairs within each half |
| `_mm256_permute2f128_ps` | Yes | Swaps/copies 128-bit halves |
| `_mm256_permutevar8x32_ps` | Yes | Full 8-element permute (AVX2 only) |

> **Rule of thumb:** If an intrinsic says "256-bit" but was available in AVX (not AVX2), suspect it operates on two 128-bit halves. Verify with [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/) or [uops.info](https://uops.info/).

---

**Inspecting SIMD Registers (Debugging)**

You can't `printf` a `__m256`. The standard pattern is a union, which lets you read individual lanes:

```cpp
// Union for safe lane inspection (from hands-on-simd-programming)
union float8 {
    __m256  v;
    float   a[8];
    float8() : v(_mm256_setzero_ps()) {}
    float8(__m256 x) : v(x) {}
};

// Usage
float8 result(_mm256_fmadd_ps(va, vb, vc));
printf("lane 0: %f, lane 3: %f\n", result.a[0], result.a[3]);

// Or with structured binding in C++17:
auto [l0,l1,l2,l3,l4,l5,l6,l7] = result.a;
```

**Horizontal sum — reducing a vector to a scalar:**

This pattern appears everywhere: dot products, attention scores, reductions.

```cpp
// Sum all 8 floats in a __m256 to a scalar
float hsum(__m256 v) {
    // Step 1: add pairs within each 128-bit half → [a+e, b+f, c+g, d+h | a+e, b+f, c+g, d+h]
    __m128 lo  = _mm256_castps256_ps128(v);          // lower 128 bits
    __m128 hi  = _mm256_extractf128_ps(v, 1);        // upper 128 bits
    __m128 sum = _mm_add_ps(lo, hi);                 // add the two halves
    // Step 2: horizontal adds until scalar
    sum = _mm_hadd_ps(sum, sum);                     // [a+b+e+f, c+d+g+h, ...]
    sum = _mm_hadd_ps(sum, sum);                     // [total, total, ...]
    return _mm_cvtss_f32(sum);                       // extract lane 0
}
```

**Stream stores for large write-once buffers:**

```cpp
// Non-temporal store: bypasses CPU cache entirely
// Use when writing large output buffers you won't read back soon
// (e.g., preprocessing output, frame buffer writes)
for (int i = 0; i < n; i += 8) {
    __m256 result = /* compute */;
    _mm256_stream_ps(&out[i], result);   // skip cache, write direct to memory
}
_mm_sfence();  // memory fence — ensure all stream writes are visible

// When to use: write-once buffers > L3 cache size
// When NOT: if you'll read the data back soon (defeats the purpose)
```

**Gather — loading from non-contiguous addresses:**

```cpp
// Gather: load from base_ptr + indices[i] * scale
float table[1024] = { /* lookup table */ };
__m256i indices = _mm256_set_epi32(7, 3, 15, 0, 42, 8, 1, 100);  // arbitrary indices
__m256 gathered = _mm256_i32gather_ps(table, indices, 4);          // scale=4 (sizeof float)

// Note: gather throughput ≈ scalar loads — no bandwidth benefit for the gather itself.
// The value: the *computation* after gather runs vectorized (8 FMAs instead of 8 scalar ops).
// Good for: embedding lookups, sparse vector ops, non-sequential access patterns.
```

---

</details>

### 缓存行与内存对齐

CPU 以 **64-byte 缓存行**为单位取内存 —— 而不是单个字节。这是 SIMD 对齐之所以关键的物理原因，也是多线程代码中大多数 false sharing bug 的根源。

```
Cache line = 64 bytes = 16 floats = 8 doubles = 2 AVX registers

[float 0][float 1]...[float 15]   ← one cache line fetch
         └─ AVX load ──┘└─ AVX load ──┘  ← if misaligned, spans two lines
```

**为什么重要：**

| 场景 | 缓存行影响 |
|----------|------------------|
| `_mm256_load_ps` 对齐 | 每次 load 只触及一个缓存行 |
| `_mm256_loadu_ps` 未对齐 | 可能触及两个缓存行 —— 内存流量最多 2× |
| false sharing（线程） | 两个线程写同一 64-byte 行内的不同变量 → 缓存乒乓 |
| `alignas(64)` struct | 保证 struct 从缓存行边界开始 |

```cpp
// False sharing — avoid this
struct Workers {
    int counter_a;  // same 64-byte line as counter_b
    int counter_b;
};

// Fix: pad to cache line boundary
struct alignas(64) WorkerA { int counter; };
struct alignas(64) WorkerB { int counter; };
```

**规则：** 为 AVX 把 SIMD buffer 对齐到 32 bytes（`alignas(32)`）。把每线程数据对齐到 64 bytes（`alignas(64)`）以防 false sharing。

---

### 数据布局：AoS vs SoA

数据在内存中如何排布，往往是 SIMD 与 GPU 代码里**杠杆最高的单项优化** —— 比指令选择影响更大。

#### Struct 数组（AoS）—— 自然、面向对象、对 SIMD 不友好

每个粒子是一个连续对象。便于传递，便于理解。

```cpp
struct Particle {
    float x, y, z;    // position
    float vx, vy, vz; // velocity
};

Particle particles[4];
```

内存布局 —— 字段在所有粒子间交错：

```
particle 0          particle 1          particle 2          particle 3
[x][y][z][vx][vy][vz][x][y][z][vx][vy][vz][x][y][z][vx][vy][vz][x][y][z][vx][vy][vz]
```

当要给所有 x 位置加上 `dx` 时：

```cpp
// AoS: stride between consecutive x values = sizeof(Particle) = 24 bytes
for (int i = 0; i < N; i++)
    particles[i].x += dx;
// An AVX load at &particles[0].x picks up: x0,y0,z0,vx0,vy0,vz0,x1,y1
//                                                           ^^^ garbage fields in the register
// You wanted: x0,x1,x2,x3,x4,x5,x6,x7 — but they're 24 bytes apart, not contiguous
```

x 值在内存中是**分散**的。SIMD 无法高效加载它们 —— 需要 gather（`_mm256_i32gather_ps`），其吞吐与标量加载相同。

#### 数组的 Struct（SoA）—— 对 SIMD 友好

每个字段各自拥有一个连续数组。所有 x 值在一起，所有 y 值在一起。

```cpp
struct Particles {
    float x[N], y[N], z[N];
    float vx[N], vy[N], vz[N];
};

Particles p;
p.x[0]=1; p.x[1]=4; p.x[2]=7;   // all x values contiguous
p.y[0]=2; p.y[1]=5; p.y[2]=8;
```

内存布局 —— 每个字段是一个扁平的连续数组：

```
x:  [x0][x1][x2][x3][x4][x5][x6][x7]...  ← one AVX load = 8 x-values
y:  [y0][y1][y2][y3][y4][y5][y6][y7]...
z:  [z0][z1][z2][z3]...
vx: [vx0][vx1][vx2]...
```

现在 SIMD 循环很干净 —— 一次 load、一次 add、一次 store：

```cpp
// SoA: x values are contiguous → direct sequential AVX load
__m256 vdx = _mm256_set1_ps(dx);
for (int i = 0; i < N; i += 8) {
    __m256 vx = _mm256_loadu_ps(&p.x[i]);       // load x0..x7 — perfectly contiguous
    _mm256_storeu_ps(&p.x[i], _mm256_add_ps(vx, vdx));  // add dx to all 8 at once
}
```

#### 循环并排对比

```cpp
// AoS — compiler cannot auto-vectorize the x update (stride too large)
for (int i = 0; i < N; i++)
    particles[i].x += dx;           // stride = 24 bytes between x values

// SoA — compiler auto-vectorizes, or you write AVX explicitly
for (int i = 0; i < N; i++)
    p.x[i] += dx;                   // stride = 4 bytes — perfectly sequential
```


<details>
<summary>English original</summary>

**Cache Lines and Memory Alignment**

The CPU fetches memory in **64-byte cache lines** — not individual bytes. This is the physical reason SIMD alignment matters and the root cause of most false-sharing bugs in multithreaded code.

```
Cache line = 64 bytes = 16 floats = 8 doubles = 2 AVX registers

[float 0][float 1]...[float 15]   ← one cache line fetch
         └─ AVX load ──┘└─ AVX load ──┘  ← if misaligned, spans two lines
```

**Why it matters:**

| Scenario | Cache line impact |
|----------|------------------|
| `_mm256_load_ps` aligned | One cache line touched per load |
| `_mm256_loadu_ps` misaligned | May touch two cache lines — up to 2× memory traffic |
| False sharing (threads) | Two threads write different variables in same 64-byte line → cache ping-pong |
| `alignas(64)` struct | Guarantees struct starts at cache line boundary |

```cpp
// False sharing — avoid this
struct Workers {
    int counter_a;  // same 64-byte line as counter_b
    int counter_b;
};

// Fix: pad to cache line boundary
struct alignas(64) WorkerA { int counter; };
struct alignas(64) WorkerB { int counter; };
```

**Rule:** Align SIMD buffers to 32 bytes (`alignas(32)`) for AVX. Align per-thread data to 64 bytes (`alignas(64)`) to prevent false sharing.

---

**Data Layout: AoS vs SoA**

How you arrange data in memory is often **the single highest-leverage optimization** in SIMD and GPU code — more impactful than instruction selection.

**Array of Structs (AoS) — natural, object-oriented, SIMD-unfriendly**

Each particle is one contiguous object. Easy to pass around, easy to reason about.

```cpp
struct Particle {
    float x, y, z;    // position
    float vx, vy, vz; // velocity
};

Particle particles[4];
```

Memory layout — fields interleaved across all particles:

```
particle 0          particle 1          particle 2          particle 3
[x][y][z][vx][vy][vz][x][y][z][vx][vy][vz][x][y][z][vx][vy][vz][x][y][z][vx][vy][vz]
```

When you want to add `dx` to all x-positions:

```cpp
// AoS: stride between consecutive x values = sizeof(Particle) = 24 bytes
for (int i = 0; i < N; i++)
    particles[i].x += dx;
// An AVX load at &particles[0].x picks up: x0,y0,z0,vx0,vy0,vz0,x1,y1
//                                                           ^^^ garbage fields in the register
// You wanted: x0,x1,x2,x3,x4,x5,x6,x7 — but they're 24 bytes apart, not contiguous
```

The x-values are **scattered** in memory. SIMD cannot load them efficiently — it would need gather (`_mm256_i32gather_ps`), which has the same throughput as scalar loads.

**Struct of Arrays (SoA) — SIMD-friendly**

Each field gets its own contiguous array. All x values together, all y values together.

```cpp
struct Particles {
    float x[N], y[N], z[N];
    float vx[N], vy[N], vz[N];
};

Particles p;
p.x[0]=1; p.x[1]=4; p.x[2]=7;   // all x values contiguous
p.y[0]=2; p.y[1]=5; p.y[2]=8;
```

Memory layout — each field is a flat contiguous array:

```
x:  [x0][x1][x2][x3][x4][x5][x6][x7]...  ← one AVX load = 8 x-values
y:  [y0][y1][y2][y3][y4][y5][y6][y7]...
z:  [z0][z1][z2][z3]...
vx: [vx0][vx1][vx2]...
```

Now the SIMD loop is clean — one load, one add, one store:

```cpp
// SoA: x values are contiguous → direct sequential AVX load
__m256 vdx = _mm256_set1_ps(dx);
for (int i = 0; i < N; i += 8) {
    __m256 vx = _mm256_loadu_ps(&p.x[i]);       // load x0..x7 — perfectly contiguous
    _mm256_storeu_ps(&p.x[i], _mm256_add_ps(vx, vdx));  // add dx to all 8 at once
}
```

**Side-by-side loop comparison**

```cpp
// AoS — compiler cannot auto-vectorize the x update (stride too large)
for (int i = 0; i < N; i++)
    particles[i].x += dx;           // stride = 24 bytes between x values

// SoA — compiler auto-vectorizes, or you write AVX explicitly
for (int i = 0; i < N; i++)
    p.x[i] += dx;                   // stride = 4 bytes — perfectly sequential
```

</details>

#### 布局对比

| | AoS | SoA | AoSoA |
|--|-----|-----|-------|
| 内存模式 | `xyzxyzxyz...` | `xxx...yyy...zzz...` | `[xxxx yyyy][xxxx yyyy]...` |
| SIMD 效率 | 低 — 需要 gather/scatter | 高 — 顺序加载 | 高 + 缓存友好 |
| GPU 合并访存 | 差 | 好 | 好 |
| 代码可读性 | 高 — `p[i].x` | 中 — `p.x[i]` | 低 — 分块索引 |
| 适用场景 | 小结构体、OOP | 物理、ML kernel、SIMD | Intel oneDNN、CUTLASS 分块 |

**各自的选用时机：**
- **AoS** — 小数据集、所有字段一起使用、可读性比吞吐更重要
- **SoA** — 大数组、按字段操作、SIMD 循环、GPU kernel
- **AoSoA** — 既需要 SoA 的效率，又想要 cache line 大小的分块以复用 L1（oneDNN 权重格式、CUTLASS fragment 布局）

> **深度学习关联：** cuDNN 的 NCHW 与 NHWC 正是这一选择。NCHW = 在空间维度上做 SoA（先所有红色像素，再所有绿色）。NHWC = 在通道上做 AoS（一个像素的所有通道放在一起）。Tensor Core 布局（如 `mma.sync` fragment）使用 AoSoA — 16×16 分块打包以在寄存器级复用。布局选错会引入昂贵的转置，进而主导 runtime。

---

### `std::simd`（C++26，实验性）

可移植 SIMD 的未来 — 无需 intrinsic，无需平台相关代码：

```cpp
#include <experimental/simd>
namespace stdx = std::experimental;

void add_vectors(float* a, float* b, float* c, int n) {
    using V = stdx::native_simd<float>;  // Auto-selects best width
    for (int i = 0; i < n; i += V::size()) {
        V va(&a[i], stdx::element_aligned);
        V vb(&b[i], stdx::element_aligned);
        V vc = va + vb;
        vc.copy_to(&c[i], stdx::element_aligned);
    }
}
```

在 GCC 11+ 中配合 `-std=c++23 -lstdc++exp` 可用。尚未生产就绪，但值得关注。

### 先测量

> *"Measure, don't guess. The bottleneck is never where you think it is."*

在写下哪怕一个 intrinsic 之前，先做 profile 确认瓶颈究竟属于哪一类。优化错的对象只会浪费时间。

```bash
# perf stat — hardware counter overview
perf stat -e instructions,cycles,cache-misses,fp_arith_inst_retired.256b_packed_single \
    ./my_program

# Key ratios to check:
#   instructions/cycles < 2.0  → probably stalled on memory
#   cache-misses high           → data layout problem (try SoA)
#   256b_packed_single low      → auto-vectorization failed, add -fopt-info-vec

# Confirm vectorization happened
g++ -O2 -march=native -fopt-info-vec-optimized my_code.cpp
# "loop vectorized using 32 byte vectors" = success

# VTune (Intel) — visual hotspots and vectorization analysis
vtune -collect hotspots ./my_program
```

### Roofline 模型（性能上界模型）

**Roofline 模型**会告诉你 kernel 是算力受限还是带宽受限 — 从而判断 SIMD 是否有帮助。

```
Peak FLOPS  = freq × cores × SIMD_width × FMA_factor
Peak BW     = DRAM bandwidth (GB/s)
AI          = FLOPs / bytes_accessed  (arithmetic intensity)

If  AI > Peak FLOPS / Peak BW  → compute-bound  (SIMD / FMA helps)
If  AI < Peak FLOPS / Peak BW  → memory-bound   (better layout / caching helps)
```

**现代桌面 CPU 的示例：**

```
Peak AVX2 FP32 = 3.6 GHz × 8 lanes × 2 (FMA) = ~58 GFLOPS/core
Peak DRAM BW   = ~50 GB/s
Ridge point AI = 58 / 50 ≈ 1.2 FLOP/byte

Vector add:  AI = 4 bytes out / 8 bytes in = 0.5 FLOP/byte → memory-bound
Dot product: AI = 2N FLOP / 2N×4 bytes    = 0.25 FLOP/byte → memory-bound
Matrix mult: AI = O(N³) FLOP / O(N²) bytes → compute-bound for large N ✓
```

**含义：** 只有在算力受限的 kernel 上，SIMD 才能带来完整的 8× 加速。对于带宽受限的 kernel（如 vector add），瓶颈在 DRAM 带宽 — 除了减少循环开销，SIMD 帮不上多少忙。

### SIMD → GPU：心智模型桥接

从 SIMD 跳到 CUDA 看似跨度很大，但底层思想相同。关键区别在于由*谁*来挑选元素索引：

```cpp
// ── SIMD mindset: one thread, one instruction covers N elements ──
for (int i = 0; i < n; i += 8) {
    __m256 va = _mm256_load_ps(&a[i]);
    __m256 vb = _mm256_load_ps(&b[i]);
    _mm256_store_ps(&c[i], _mm256_add_ps(va, vb));
}
// You write the stride (i += 8). The hardware runs 8 lanes silently.

// ── GPU / SIMT mindset: one thread per element, hardware spawns thousands ──
__global__ void add(float* a, float* b, float* c, int n) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i < n) c[i] = a[i] + b[i];
}
// You write one element. The hardware runs 32 (warp) or thousands in parallel.
```

| 方面 | AVX（CPU SIMD） | CUDA warp（GPU SIMT） |
|--------|---------------|----------------------|
| 并行宽度 | 8× float32 | 32 个线程 |
| 寄存器堆 | `__m256`（256-bit） | 每线程 32 个 32-bit 寄存器 |
| 存储 | L1/L2 缓存 | 共享内存 / 全局内存 |
| 发散代价 | 分支破坏向量化 | warp 发散会串行化线程 |
| 编程模型 | 显式 lane（intrinsic） | 隐式（线程索引） |

---


<details>
<summary>English original</summary>

**Layout comparison**

| | AoS | SoA | AoSoA |
|--|-----|-----|-------|
| Memory pattern | `xyzxyzxyz...` | `xxx...yyy...zzz...` | `[xxxx yyyy][xxxx yyyy]...` |
| SIMD efficiency | Low — gather/scatter needed | High — sequential load | High + cache-friendly |
| GPU coalescing | Poor | Good | Good |
| Code readability | High — `p[i].x` | Medium — `p.x[i]` | Low — tiled indexing |
| Best for | Small structs, OOP | Physics, ML kernels, SIMD | Intel oneDNN, CUTLASS tiling |

**When to choose each:**
- **AoS** — small datasets, all fields used together, readability matters more than throughput
- **SoA** — large arrays, per-field operations, SIMD loops, GPU kernels
- **AoSoA** — when you need SoA efficiency but also want cache-line-sized tiles for L1 reuse (oneDNN weight format, CUTLASS fragment layouts)

> **Deep learning connection:** cuDNN NCHW vs NHWC is exactly this choice. NCHW = SoA over spatial dimensions (all red pixels, then all green). NHWC = AoS over channels (all channels for one pixel together). Tensor Core layouts (e.g., `mma.sync` fragments) use AoSoA — 16×16 tiles packed for register-level reuse. Choosing the wrong layout forces expensive transposes that dominate runtime.

---

**`std::simd` (C++26, Experimental)**

The future of portable SIMD — no intrinsics, no platform-specific code:

```cpp
#include <experimental/simd>
namespace stdx = std::experimental;

void add_vectors(float* a, float* b, float* c, int n) {
    using V = stdx::native_simd<float>;  // Auto-selects best width
    for (int i = 0; i < n; i += V::size()) {
        V va(&a[i], stdx::element_aligned);
        V vb(&b[i], stdx::element_aligned);
        V vc = va + vb;
        vc.copy_to(&c[i], stdx::element_aligned);
    }
}
```

Available in GCC 11+ with `-std=c++23 -lstdc++exp`. Not yet production-ready but worth tracking.

**Measurement First**

> *"Measure, don't guess. The bottleneck is never where you think it is."*

Before writing a single intrinsic, profile to confirm what kind of bottleneck you actually have. Optimizing the wrong thing wastes time.

```bash
# perf stat — hardware counter overview
perf stat -e instructions,cycles,cache-misses,fp_arith_inst_retired.256b_packed_single \
    ./my_program

# Key ratios to check:
#   instructions/cycles < 2.0  → probably stalled on memory
#   cache-misses high           → data layout problem (try SoA)
#   256b_packed_single low      → auto-vectorization failed, add -fopt-info-vec

# Confirm vectorization happened
g++ -O2 -march=native -fopt-info-vec-optimized my_code.cpp
# "loop vectorized using 32 byte vectors" = success

# VTune (Intel) — visual hotspots and vectorization analysis
vtune -collect hotspots ./my_program
```

**Roofline Model**

The **Roofline model** tells you whether your kernel is compute-bound or memory-bound — and therefore whether SIMD helps.

```
Peak FLOPS  = freq × cores × SIMD_width × FMA_factor
Peak BW     = DRAM bandwidth (GB/s)
AI          = FLOPs / bytes_accessed  (arithmetic intensity)

If  AI > Peak FLOPS / Peak BW  → compute-bound  (SIMD / FMA helps)
If  AI < Peak FLOPS / Peak BW  → memory-bound   (better layout / caching helps)
```

**Example for a modern desktop CPU:**

```
Peak AVX2 FP32 = 3.6 GHz × 8 lanes × 2 (FMA) = ~58 GFLOPS/core
Peak DRAM BW   = ~50 GB/s
Ridge point AI = 58 / 50 ≈ 1.2 FLOP/byte

Vector add:  AI = 4 bytes out / 8 bytes in = 0.5 FLOP/byte → memory-bound
Dot product: AI = 2N FLOP / 2N×4 bytes    = 0.25 FLOP/byte → memory-bound
Matrix mult: AI = O(N³) FLOP / O(N²) bytes → compute-bound for large N ✓
```

**Implication:** SIMD gives the full 8× speedup only on compute-bound kernels. For memory-bound kernels (like vector add), the bottleneck is DRAM bandwidth — SIMD won't help much beyond reducing loop overhead.

**SIMD → GPU: Mental Model Bridge**

The jump from SIMD to CUDA looks large but the underlying idea is the same. The key difference is *who* picks the element index:

```cpp
// ── SIMD mindset: one thread, one instruction covers N elements ──
for (int i = 0; i < n; i += 8) {
    __m256 va = _mm256_load_ps(&a[i]);
    __m256 vb = _mm256_load_ps(&b[i]);
    _mm256_store_ps(&c[i], _mm256_add_ps(va, vb));
}
// You write the stride (i += 8). The hardware runs 8 lanes silently.

// ── GPU / SIMT mindset: one thread per element, hardware spawns thousands ──
__global__ void add(float* a, float* b, float* c, int n) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i < n) c[i] = a[i] + b[i];
}
// You write one element. The hardware runs 32 (warp) or thousands in parallel.
```

| Aspect | AVX (CPU SIMD) | CUDA warp (GPU SIMT) |
|--------|---------------|----------------------|
| Parallel width | 8× float32 | 32 threads |
| Register file | `__m256` (256-bit) | 32× 32-bit registers per thread |
| Memory | L1/L2 cache | Shared memory / global memory |
| Divergence cost | Branch kills vectorization | Warp divergence serializes threads |
| Programmer model | Explicit lanes (intrinsics) | Implicit (thread index) |

---

</details>

### 与栈的连接

- **L1（应用）：** cuDNN 与 CUTLASS 内部使用向量化内存加载
- **L2（编译器）：** MLIR 的 `vector` 方言（阶段 4C）瞄准的正是这一抽象层级
- **L5（架构）：** 设计加速器的向量单元时，你就是在设计定制 SIMD 硬件
- **子赛道 3（CUDA）：** GPU 的“SIMT”就是放大到数千线程的 SIMD——同一心智模型

---

## 性能陷阱与常见错误

这些就是真实 HPC 与 ML 系统中会耗掉数周调试时间的问题。在这里把它们学会。

| 陷阱 | 会发生什么 | 修复方法 |
|------|-------------|-----|
| **热循环中的 `shared_ptr`** | 每次迭代都做原子引用计数增减 → 缓存行争用 | 在热路径内部使用裸指针或 `unique_ptr`；在循环外一次性增加 `shared_ptr` 计数 |
| **伪共享** | 两个线程更新同一 64 字节缓存行中的不同变量 → 不可见的串行化 | 将线程本地数据填充到 `alignas(64)` |
| **未对齐的 `_mm256_load_ps`** | 运行时报段错误（`#GP`），而非编译期 | 除非已用 `alignas(32)` 验证过 32 字节对齐，否则使用 `_mm256_loadu_ps` |
| **向量化循环内的分支** | 编译器无法向量化；回退到标量执行 | 把条件判断移出循环；使用 SIMD blend 指令（`_mm256_blendv_ps`） |
| **带宽受限的 SIMD** | SIMD 增加了复杂度，加速却微乎其微 | 先做 profile（Roofline 性能上界模型）；在引入 intrinsics 前先重构数据布局（SoA） |
| **带依赖的 `par_unseq`** | 静默的数据损坏或未定义行为 | 仅在元素**完全独立**时使用——不读取相邻下标的元素 |
| **忽视 NUMA** | 多路系统：线程在 socket 0 上分配内存，socket 1 的线程去读它 → 2× 带宽 | 在将使用该内存的 NUMA 节点上分配内存（`numactl`、`mbind`） |

---

## 项目

1. **现代 C++ 热身** —— 用 `std::vector`、移动语义、模板与运算符重载写一个矩阵类。用 `-fno-elide-constructors` 检查，验证从函数返回一个大矩阵时用的是移动（而非拷贝）。
2. **Lambda benchmark** —— 用三种方式实现同一计算：裸循环、带 lambda 的 `std::for_each`、带 lambda 的 `std::transform`。用 `-O2` 对三者做 benchmark。验证编译器生成完全相同的汇编（`-S`）。
3. **并行 STL 排序** —— 用 `std::execution::seq`、`par`、`par_unseq` 对 10M 个 float 排序。测量加速比。画出时间对数组大小的曲线。
4. **自动向量化** —— 写一个点积循环。用 `-O2 -march=native -fopt-info-vec` 编译。检查编译器是否将其向量化。若没有，重构循环直到向量化为止。
5. **AVX intrinsics 点积** —— 用 `_mm256_fmadd_ps` 实现点积。与标量版和自动向量化版做 benchmark。测量 GFLOPS。
6. **对齐 vs 未对齐** —— 对大数组 benchmark `_mm256_load_ps`（对齐）与 `_mm256_loadu_ps`（未对齐）。测量性能差异。
7. **AoS → SoA benchmark** —— 为 `Vec3` 结构体数组（AoS）实现向量点积，与 SoA 布局版本比较。观察加速差距（预计仅靠布局就能带来 10–40×）。
8. **水平求和** —— 实现一个点积，用 `_mm256_fmadd_ps` 一次累加 8 个，再用水平求和惯用法归约到标量。与标量结果核对。
9. **微型 MHA 块** —— 用原始 AVX2 intrinsics 为 seq=8、head_dim=16、heads=4 实现一个批处理多头 attention 前向传播。预转置权重矩阵。与标量版本做 benchmark。观察到加速约 2–3×，而非 8×——解释原因（带宽受限）。
10. **NEON 点积** —— 在 ARM 上用 `vfmaq_f32` 和 `vaddvq_f32` 实现点积。在 Jetson 或 Apple Silicon 上做 benchmark。与 AVX2 版本比较 GFLOPS。
11. **NEON RGB 去交织** —— 用 `vld3q_u8` 把 RGBRGB... 转换为平面 R,G,B 数组。与标量循环做 benchmark。在 Jetson 上用真实相机帧（1920×1080）测量。
12. **NEON INT8 ReLU** —— 在 16 宽 INT8 向量上实现 `max(x, 0)`。处理 1M 个元素。测量吞吐（GB/s），并与 Jetson 上 LPDDR5 的理论带宽比较。
13. **NEON FP16 GEMM** —— 用 `vfmaq_f16` 实现一个分块 64×64 FP16 矩阵乘。与 FP32 版本比较吞吐。通过上转为 FP32 验证输出正确性。
14. **NEON vs 自动向量化** —— 把同样的 5 个 kernel（add、dot、relu、scale、reduce）既写成手写 NEON，也写成用 `-O2 -march=armv8.2-a` 编译的普通循环。在 Godbolt 上比较汇编输出。注意编译器在哪些地方与你的 intrinsics 一致、哪些地方不一致。

---

## 资源


<details>
<summary>English original</summary>

**Connection to the Stack**

- **L1 (Application):** cuDNN and CUTLASS use vectorized memory loads internally
- **L2 (Compiler):** MLIR's `vector` dialect (Phase 4C) targets exactly this abstraction level
- **L5 (Architecture):** When you design an accelerator's vector unit, you're designing custom SIMD hardware
- **Sub-Track 3 (CUDA):** GPU "SIMT" is SIMD scaled to thousands of threads — same mental model

---

**Performance Traps and Common Mistakes**

These are the issues that consume weeks of debugging in real HPC and ML systems. Learn them here.

| Trap | What happens | Fix |
|------|-------------|-----|
| **`shared_ptr` in hot loops** | Atomic ref-count inc/dec every iteration → cache line contention | Use raw pointer or `unique_ptr` inside hot paths; bump `shared_ptr` count once outside the loop |
| **False sharing** | Two threads update different variables in the same 64-byte cache line → invisible serialization | Pad thread-local data to `alignas(64)` |
| **Unaligned `_mm256_load_ps`** | Segfault (`#GP`) at runtime, not compile time | Use `_mm256_loadu_ps` unless you've verified 32-byte alignment with `alignas(32)` |
| **Branching inside vectorized loops** | Compiler cannot vectorize; scalar fallback runs | Move conditionals outside the loop; use SIMD blend instructions (`_mm256_blendv_ps`) |
| **Memory-bound SIMD** | SIMD adds complexity, negligible speedup | Profile first (Roofline); restructure data layout (SoA) before adding intrinsics |
| **`par_unseq` with dependencies** | Silent data corruption or undefined behavior | Only use when elements are **completely independent** — no reads from neighboring indices |
| **Ignoring NUMA** | Multi-socket systems: thread allocates memory on socket 0, socket 1 thread reads it → 2× bandwidth | Allocate memory on the NUMA node that will use it (`numactl`, `mbind`) |

---

**Projects**

1. **Modern C++ warmup** — Write a matrix class using `std::vector`, move semantics, templates, and operator overloading. Verify that returning a large matrix from a function uses move (not copy) by checking with `-fno-elide-constructors`.
2. **Lambda benchmark** — Implement the same computation three ways: raw loop, `std::for_each` with lambda, and `std::transform` with lambda. Benchmark all three with `-O2`. Verify the compiler generates identical assembly (`-S`).
3. **Parallel STL sort** — Sort 10M floats with `std::execution::seq`, `par`, and `par_unseq`. Measure speedup. Plot time vs array size.
4. **Auto-vectorization** — Write a dot product loop. Compile with `-O2 -march=native -fopt-info-vec`. Check if the compiler vectorized it. If not, restructure the loop until it does.
5. **AVX intrinsics dot product** — Implement dot product using `_mm256_fmadd_ps`. Benchmark against scalar and auto-vectorized versions. Measure GFLOPS.
6. **Aligned vs unaligned** — Benchmark `_mm256_load_ps` (aligned) vs `_mm256_loadu_ps` (unaligned) for a large array. Measure the performance difference.
7. **AoS → SoA benchmark** — Implement vector dot product for an array of `Vec3` structs (AoS) and compare to the SoA layout version. Observe the speedup gap (expect 10–40× from layout alone).
8. **Horizontal sum** — Implement a dot product that uses `_mm256_fmadd_ps` to accumulate 8 at a time, then uses the horizontal sum idiom to reduce to a scalar. Verify against scalar result.
9. **Tiny MHA block** — Implement a batched multi-head attention forward pass for seq=8, head_dim=16, heads=4 using raw AVX2 intrinsics. Pre-transpose weight matrices. Benchmark against scalar version. Observe that the speedup is ~2–3×, not 8× — explain why (memory-bound).
10. **NEON dot product** — Implement dot product on ARM using `vfmaq_f32` and `vaddvq_f32`. Benchmark on Jetson or Apple Silicon. Compare GFLOPS with the AVX2 version.
11. **NEON RGB deinterleave** — Use `vld3q_u8` to convert RGBRGB... to planar R,G,B arrays. Benchmark against scalar loop. Measure on Jetson with a real camera frame (1920×1080).
12. **NEON INT8 ReLU** — Implement `max(x, 0)` on 16-wide INT8 vectors. Process 1M elements. Measure throughput (GB/s) and compare to theoretical LPDDR5 bandwidth on Jetson.
13. **NEON FP16 GEMM** — Implement a tiled 64×64 FP16 matrix multiply using `vfmaq_f16`. Compare throughput with FP32 version. Verify output correctness by upconverting to FP32.
14. **NEON vs auto-vec** — Write the same 5 kernels (add, dot, relu, scale, reduce) both as manual NEON and as plain loops compiled with `-O2 -march=armv8.2-a`. Compare assembly output on Godbolt. Note where the compiler matches your intrinsics and where it doesn't.

---

**Resources**

</details>

### 学习

| 资源 | 内容 |
|----------|---------------|
| *A Tour of C++*（Stroustrup）| 现代 C++ 概览，简明扼要 |
| [cppreference.com](https://en.cppreference.com/) | C++ 权威参考 |
| *C++ Concurrency in Action*（Williams）| 线程、原子操作、内存模型 |
| [SIMD for C++ Developers (const.me)](http://const.me/articles/simd/simd.pdf) | 最佳实践者教程：加载、算术、shuffle、masking、跨 lane 陷阱。23 页。先读这篇。 |
| [hands-on-simd-programming](https://github.com/yuninxia/hands-on-simd-programming) | 循序渐进的实验：从基础 intrinsics → 图像处理 → 完整 MHA block → 量化 GPT decoder。运行 `./runme.sh` 查看 benchmark。 |
| [awesome-simd](https://github.com/awesome-simd/awesome-simd) | 真实世界 SIMD 库、工具、博客与参考资料的精选索引 |
| [Agner Fog Optimization Guides](https://www.agner.org/optimize/) | CPU 微架构、指令延迟、调用约定的权威参考 |

### ARM / NEON

| 资源 | 内容 |
|----------|---------------|
| [ARM NEON Intrinsics Reference](https://developer.arm.com/architectures/instruction-sets/intrinsics/) | 官方可搜索 intrinsics 指南（类似 Intel 的，但面向 ARM） |
| [ARM Neon Programmer's Guide](https://developer.arm.com/documentation/den0018/latest) | 官方教程：数据类型、操作、优化 |
| [ncnn NEON optimization](https://github.com/Tencent/ncnn/wiki/how-ncnn-optimizes) | 面向神经网络推理的真实 NEON 优化 |
| [Arm Performance Libraries](https://developer.arm.com/tools-and-software/server-and-hpc/downloads/arm-performance-libraries) | 面向 ARM 的优化 BLAS/LAPACK/FFT（类似 x86 上的 MKL） |
| [SIMDe](https://github.com/simd-everywhere/simde) | 在 ARM 上模拟 x86 intrinsics —— 在 Jetson 上运行 AVX 代码 |

### 参考资料与工具

| 资源 | 内容 |
|----------|---------------|
| [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/) | 每条 x86 SIMD intrinsic 的可搜索参考 |
| [uops.info](https://uops.info/) | 每种 CPU 微架构的指令延迟、吞吐与端口占用 |
| [Compiler Explorer (godbolt.org)](https://godbolt.org/) | 查看任意 C++ 代码的汇编输出 —— 验证是否发生了向量化 |
| [SIMD-Visualiser](https://github.com/piotte13/SIMD-Visualiser) | 基于浏览器的可视化图示，展示每条 intrinsic 对寄存器内容的操作 |
| [Felix Cloutier x86 reference](https://www.felixcloutier.com/x86/) | HTML 版 x86/x64 指令参考 |

### 可移植 SIMD 库（可跳过裸 intrinsics）

| 库 | 语言 | 说明 |
|---------|----------|------------|
| [Google Highway](https://github.com/google/highway) | C++ | 最佳可移植 SIMD：长度无关、runtime 分派、SSE/AVX/AVX-512/NEON/SVE |
| [xsimd](https://github.com/QuantStack/xsimd) | C++ | SSE/AVX/AVX-512/NEON 的 header-only 封装 |
| [SIMDe](https://github.com/simd-everywhere/simde) | C/C++ | 在 ARM 及其他目标平台上模拟 x86 intrinsics |
| [Intel ISPC](https://ispc.github.io/) | ISPC | 类 C 语言，可编译为面向 SSE/AVX/AVX-512/NEON/PS5/Xbox 的最优 SIMD |

### 值得研究的真实世界 SIMD 库

| 库 | 作用 |
|---------|-------------|
| [simdjson](https://github.com/lemire/simdjson) | 用 SIMD 实现 >2 GB/s 的 JSON 解析 —— 真实世界向量化解析的典范 |
| [SimSIMD](https://github.com/ashvardanian/SimSIMD) | 面向 embedding 向量的 SIMD 加速相似度度量（cosine、L2） |
| [ncnn](https://github.com/Tencent/ncnn) | 端侧神经网络推理，针对 ARM 与 x86 手工调优的 SIMD |
| [StringZilla](https://github.com/ashvardanian/StringZilla) | SIMD 子串搜索、模糊匹配、排序 |

### 博客（编写生产级 SIMD 代码的实践者）

| 博客 | 作者 |
|------|--------|
| [lemire.me/blog](https://lemire.me/blog/) | Daniel Lemire —— simdjson、SIMDComp、实用向量化 |
| [branchfree.org](https://branchfree.org/) | Geoff Langdale —— 无分支与 SIMD 技术 |
| [0x80.pl/notesen](http://0x80.pl/notesen.html) | Wojciech Muła —— 底层 SIMD 算法 |

---

## 下一步

→ [**Sub-Track 2 — OpenMP 与 oneTBB**](../04-OpenMP与OneTBB/Guide.md) —— 从 SIMD 扩展到多核 CPU 并行。


<details>
<summary>English original</summary>

**Learning**

| Resource | What it covers |
|----------|---------------|
| *A Tour of C++* (Stroustrup) | Modern C++ overview, concise |
| [cppreference.com](https://en.cppreference.com/) | Definitive C++ reference |
| *C++ Concurrency in Action* (Williams) | Threads, atomics, memory model |
| [SIMD for C++ Developers (const.me)](http://const.me/articles/simd/simd.pdf) | Best practitioner tutorial: loads, arithmetic, shuffles, masking, cross-lane gotchas. 23 pages. Read this first. |
| [hands-on-simd-programming](https://github.com/yuninxia/hands-on-simd-programming) | Progressive labs from basic intrinsics → image processing → full MHA block → quantized GPT decoder. Run `./runme.sh` to see benchmarks. |
| [awesome-simd](https://github.com/awesome-simd/awesome-simd) | Curated index of real-world SIMD libraries, tools, blogs, and references |
| [Agner Fog Optimization Guides](https://www.agner.org/optimize/) | Definitive reference on CPU micro-architecture, instruction latencies, calling conventions |

**ARM / NEON**

| Resource | What it covers |
|----------|---------------|
| [ARM NEON Intrinsics Reference](https://developer.arm.com/architectures/instruction-sets/intrinsics/) | Official searchable intrinsics guide (like Intel's, but for ARM) |
| [ARM Neon Programmer's Guide](https://developer.arm.com/documentation/den0018/latest) | Official tutorial: data types, operations, optimization |
| [ncnn NEON optimization](https://github.com/Tencent/ncnn/wiki/how-ncnn-optimizes) | Real-world NEON optimization for neural network inference |
| [Arm Performance Libraries](https://developer.arm.com/tools-and-software/server-and-hpc/downloads/arm-performance-libraries) | Optimized BLAS/LAPACK/FFT for ARM (like MKL for x86) |
| [SIMDe](https://github.com/simd-everywhere/simde) | Emulates x86 intrinsics on ARM — run AVX code on Jetson |

**Reference and Tooling**

| Resource | What it covers |
|----------|---------------|
| [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/) | Searchable reference for every x86 SIMD intrinsic |
| [uops.info](https://uops.info/) | Instruction latency, throughput, and port usage for every CPU micro-architecture |
| [Compiler Explorer (godbolt.org)](https://godbolt.org/) | See assembly output for any C++ code — verify vectorization happened |
| [SIMD-Visualiser](https://github.com/piotte13/SIMD-Visualiser) | Browser-based visual diagram of what each intrinsic does to register contents |
| [Felix Cloutier x86 reference](https://www.felixcloutier.com/x86/) | HTML x86/x64 instruction reference |

**Portable SIMD Libraries (skip raw intrinsics)**

| Library | Language | What it is |
|---------|----------|------------|
| [Google Highway](https://github.com/google/highway) | C++ | Best portable SIMD: length-agnostic, runtime dispatch, SSE/AVX/AVX-512/NEON/SVE |
| [xsimd](https://github.com/QuantStack/xsimd) | C++ | Header-only wrappers for SSE/AVX/AVX-512/NEON |
| [SIMDe](https://github.com/simd-everywhere/simde) | C/C++ | Emulates x86 intrinsics on ARM and other targets |
| [Intel ISPC](https://ispc.github.io/) | ISPC | C-like language that compiles to optimal SIMD for SSE/AVX/AVX-512/NEON/PS5/Xbox |

**Real-World SIMD Libraries to Study**

| Library | What it does |
|---------|-------------|
| [simdjson](https://github.com/lemire/simdjson) | JSON parsing at >2 GB/s using SIMD — canonical example of real-world vectorized parsing |
| [SimSIMD](https://github.com/ashvardanian/SimSIMD) | SIMD-accelerated similarity measures (cosine, L2) for embedding vectors |
| [ncnn](https://github.com/Tencent/ncnn) | On-device neural network inference with hand-tuned SIMD for ARM and x86 |
| [StringZilla](https://github.com/ashvardanian/StringZilla) | SIMD substring search, fuzzy matching, sorting |

**Blogs (practitioners writing production SIMD code)**

| Blog | Author |
|------|--------|
| [lemire.me/blog](https://lemire.me/blog/) | Daniel Lemire — simdjson, SIMDComp, practical vectorization |
| [branchfree.org](https://branchfree.org/) | Geoff Langdale — branchless and SIMD techniques |
| [0x80.pl/notesen](http://0x80.pl/notesen.html) | Wojciech Muła — low-level SIMD algorithms |

---

**Next**

→ [**Sub-Track 2 — OpenMP and oneTBB**](../04-OpenMP与OneTBB/Guide.md) — scale from SIMD to multi-core CPU parallelism.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/4. C++ and Parallel Computing/C++ and SIMD/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/4.%20C%2B%2B%20and%20Parallel%20Computing/C%2B%2B%20and%20SIMD/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
