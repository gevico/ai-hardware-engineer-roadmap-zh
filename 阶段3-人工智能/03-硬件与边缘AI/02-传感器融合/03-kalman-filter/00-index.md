---
title: 卡尔曼滤波学习路径
description: 卡尔曼滤波学习路径
---

# 卡尔曼滤波学习路径

从基础概念到专家级实现的完整旅程。

## 学习路径

### 第 1 级：初级（8-12 岁）
- [01 - 什么是估计？](01-what-is-estimation.md) - 理解猜测与改进猜测
- [02 - 有噪声的测量](02-noisy-measurements.md) - 为什么传感器并不完美
- [03 - 信息融合](03-combining-information.md) - 利用多条线索

### 第 2 级：初中（13-15 岁）
- [04 - 平均值与不确定性](04-averages-and-uncertainty.md) - 均值与方差
- [05 - 加权平均](05-weighted-averages.md) - 更信任某些测量
- [06 - 预测](06-prediction.md) - 由过去推测未来

### 第 3 级：高中（16-18 岁）
- [07 - 卡尔曼滤波的思想](07-kalman-filter-idea.md) - 把一切整合起来
- [08 - 一维卡尔曼滤波](08-1d-kalman-filter.md) - 第一个真正的实现
- [09 - 跟踪运动目标](09-tracking-moving-object.md) - 位置与速度

### 第 4 级：本科（大学）
- [10 - 矩阵形式](10-matrix-form.md) - 数学记号
- [11 - 多维卡尔曼滤波](11-multidimensional-kf.md) - 多个状态
- 12 - 过程噪声与测量噪声 — 协方差矩阵 *(即将推出)*
- 13 - 卡尔曼滤波算法 — 完整方程 *(即将推出)*

### 第 5 级：研究生（进阶）
- 14 - 扩展卡尔曼滤波 — 处理非线性 *(即将推出)*
- 15 - 无迹卡尔曼滤波 — 更好的非线性处理 *(即将推出)*
- 16 - 误差状态卡尔曼滤波 — 用于姿态估计 *(即将推出)*
- 17 - 多状态约束 KF — 用于视觉里程计 *(即将推出)*

### 第 6 级：专家（专业）
- 18 - 卡尔曼平滑 — RTS 平滑器 *(即将推出)*
- 19 - 异常值剔除 — 鲁棒估计 *(即将推出)*
- 20 - 实现技巧 — 数值稳定性 *(即将推出)*
- 21 - 实际应用 — GPS、IMU、相机 *(即将推出)*

## 如何使用本指南

1. **从你所在的级别开始** - 不要跳得太快
2. **做练习题** - 每章都有动手题目
3. **跟着写代码** - Python 实现示例
4. **先建立直觉** - 先理解“为什么”，再理解“怎么做”

## 各级别的前置要求

- **第 1-2 级**：基础算术
- **第 3 级**：代数、基础概率
- **第 4 级**：线性代数、概率论
- **第 5 级**：微积分、统计学
- **第 6 级**：高等数学、编程

## 快速查阅

掌握基础之后，可用以下内容快速查阅：
- [卡尔曼滤波方程速查表](cheat-sheet.md)
- 常见错误与解决方案（即将推出）
- Python 实现模板（即将推出）

开始这段旅程吧！


<details>
<summary>English original</summary>

**Kalman Filter Learning Path**

A complete journey from elementary concepts to expert-level implementation.

**Learning Path**

**Level 1: Elementary (Ages 8-12)**
- [01 - What is Estimation?](01-what-is-estimation.md) - Understanding guessing and improving guesses
- [02 - Noisy Measurements](02-noisy-measurements.md) - Why sensors aren't perfect
- [03 - Combining Information](03-combining-information.md) - Using multiple clues

**Level 2: Middle School (Ages 13-15)**
- [04 - Averages and Uncertainty](04-averages-and-uncertainty.md) - Mean and variance
- [05 - Weighted Averages](05-weighted-averages.md) - Trusting some measurements more
- [06 - Prediction](06-prediction.md) - Guessing the future from the past

**Level 3: High School (Ages 16-18)**
- [07 - The Kalman Filter Idea](07-kalman-filter-idea.md) - Putting it all together
- [08 - One Dimensional Kalman Filter](08-1d-kalman-filter.md) - First real implementation
- [09 - Tracking a Moving Object](09-tracking-moving-object.md) - Position and velocity

**Level 4: Undergraduate (College)**
- [10 - Matrix Form](10-matrix-form.md) - Mathematical notation
- [11 - Multidimensional Kalman Filter](11-multidimensional-kf.md) - Multiple states
- 12 - Process and Measurement Noise — Covariance matrices *(coming soon)*
- 13 - Kalman Filter Algorithm — Complete equations *(coming soon)*

**Level 5: Graduate (Advanced)**
- 14 - Extended Kalman Filter — Handling non-linearity *(coming soon)*
- 15 - Unscented Kalman Filter — Better non-linear handling *(coming soon)*
- 16 - Error State Kalman Filter — For orientation estimation *(coming soon)*
- 17 - Multi-State Constraint KF — For visual odometry *(coming soon)*

**Level 6: Expert (Professional)**
- 18 - Kalman Smoothing — RTS smoother *(coming soon)*
- 19 - Outlier Rejection — Robust estimation *(coming soon)*
- 20 - Implementation Tricks — Numerical stability *(coming soon)*
- 21 - Real World Applications — GPS, IMU, cameras *(coming soon)*

**How to Use This Guide**

1. **Start at your level** - Don't skip ahead too fast
2. **Do the exercises** - Each chapter has hands-on problems
3. **Code along** - Implementation examples in Python
4. **Build intuition first** - Understand "why" before "how"

**Prerequisites by Level**

- **Level 1-2**: Basic arithmetic
- **Level 3**: Algebra, basic probability
- **Level 4**: Linear algebra, probability theory
- **Level 5**: Calculus, statistics
- **Level 6**: Advanced mathematics, programming

**Quick Reference**

Once you've learned the basics, use these for quick lookup:
- [Kalman Filter Equations Cheat Sheet](cheat-sheet.md)
- Common Mistakes and Solutions (coming soon)
- Python Implementation Templates (coming soon)

Let's begin the journey!

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/00-index.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/00-index.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
