---
title: 加权平均
description: 加权平均
---

# 加权平均

**难度：初中（13-15 岁）**

## 普通平均的问题

设想你要测出室外的温度。你手头有：
- 你的猜测：80°F（纯粹是猜的）
- 便宜温度计：72°F（还算可靠）
- 气象站：70°F（非常可靠）

普通平均：(80 + 72 + 70) ÷ 3 = 74°F

但等等！你那个胡乱猜的数，凭什么和气象站同等分量？显然不该！

## 什么是加权平均？

**加权平均**让某些数比其他数更重要。

### 公式
```
Weighted Average = (value₁ × weight₁ + value₂ × weight₂ + ...) ÷ (sum of weights)
```

### 例：带权重的温度

按可靠性给权重：
- 你的猜测：80°F，权重 = 1（不太可靠）
- 便宜温度计：72°F，权重 = 3（还算可靠）
- 气象站：70°F，权重 = 6（非常可靠）

```
Weighted Average = (80×1 + 72×3 + 70×6) ÷ (1+3+6)
                 = (80 + 216 + 420) ÷ 10
                 = 716 ÷ 10
                 = 71.6°F
```

注意：71.6°F 离气象站的 70°F 比离你猜的 80°F 近得多！

## 权重为何重要

### 例 1：学校成绩

老师计算你的成绩：
- 作业：85%（权重 = 20%）
- 小测：90%（权重 = 30%）
- 期末考试：78%（权重 = 50%）

**普通平均**：(85 + 90 + 78) ÷ 3 = 84.3%

**加权平均**：
```
(85×0.20 + 90×0.30 + 78×0.50)
= 17 + 27 + 39
= 83%
```

期末考试占比更大，把成绩拉低了！

### 例 2：合并多次测量

你测量一张桌子的长度：
- 测量 1：48.2 英寸（不确定度 ± 0.5 英寸）
- 测量 2：48.0 英寸（不确定度 ± 0.1 英寸）

哪次测量更该信？测量 2！它的不确定度更低。

## 由不确定度得到权重

关键洞见在此：**不确定度越低 = 权重越高**

### 规则
```
Weight = 1 ÷ (Uncertainty²)
```

用数学式子表达：
```
Weight = 1 ÷ Variance
```

### 例：桌子测量

**测量 1**：48.2 英寸，不确定度 = 0.5
- 方差 = 0.5² = 0.25
- 权重 = 1 ÷ 0.25 = 4

**测量 2**：48.0 英寸，不确定度 = 0.1
- 方差 = 0.1² = 0.01
- 权重 = 1 ÷ 0.01 = 100

测量 2 得到的权重是 25 倍！（100 对 4）

**加权平均**：
```
(48.2×4 + 48.0×100) ÷ (4+100)
= (192.8 + 4800) ÷ 104
= 4992.8 ÷ 104
= 48.01 inches
```

结果与更准的那次测量非常接近！

## 卡尔曼滤波的做法

卡尔曼滤波用加权平均来合并：
1. **预测**（你预期的东西）
2. **测量**（你观测到的东西）

两者各有各的不确定度，所以各有各的权重！

### 例：跟踪一辆汽车

**预测**：汽车位于 100 米处（不确定度 ± 5 米）
**测量**：GPS 显示 95 米（不确定度 ± 10 米）

更该信哪个？预测！它的不确定度更低。

**预测权重**：1 ÷ 5² = 1 ÷ 25 = 0.04
**测量权重**：1 ÷ 10² = 1 ÷ 100 = 0.01

**加权估计**：
```
(100×0.04 + 95×0.01) ÷ (0.04+0.01)
= (4 + 0.95) ÷ 0.05
= 4.95 ÷ 0.05
= 99 meters
```

估计值离预测的 100m 比离测量的 95m 更近，因为预测更确定！

## 卡尔曼增益：神奇的数字

卡尔曼滤波用一个叫 **卡尔曼增益**（K）的东西来决定该多信测量、多信预测。

### 公式
```
K = Prediction Uncertainty ÷ (Prediction Uncertainty + Measurement Uncertainty)
```

### K 的含义

- **K = 0**：完全不信测量（用预测）
- **K = 1**：完全不信预测（用测量）
- **K = 0.5**：两者同等相信

### 例：汽车跟踪

预测不确定度：5 米
测量不确定度：10 米

```
K = 5² ÷ (5² + 10²)
  = 25 ÷ (25 + 100)
  = 25 ÷ 125
  = 0.2
```

**更新公式**：
```
New Estimate = Prediction + K × (Measurement - Prediction)
             = 100 + 0.2 × (95 - 100)
             = 100 + 0.2 × (-5)
             = 100 - 1
             = 99 meters
```

与加权平均得到相同的答案！

## 理解卡尔曼增益

### 情形 1：预测非常确定

预测：100 ± 1 米
测量：95 ± 10 米

```
K = 1² ÷ (1² + 10²) = 1 ÷ 101 ≈ 0.01
```

K 非常小！更信预测。

```
New Estimate = 100 + 0.01 × (95 - 100)
             = 100 + 0.01 × (-5)
             = 100 - 0.05
             = 99.95 meters
```

非常接近预测值 100m！

### 情形 2：测量非常确定

预测：100 ± 10 米
测量：95 ± 1 米

```
K = 10² ÷ (10² + 1²) = 100 ÷ 101 ≈ 0.99
```

K 几乎等于 1！更信测量。

```
New Estimate = 100 + 0.99 × (95 - 100)
             = 100 + 0.99 × (-5)
             = 100 - 4.95
             = 95.05 meters
```

非常接近测量值 95m！


<details>
<summary>English original</summary>

**Weighted Averages**

**Level: Middle School (Ages 13-15)**

**The Problem with Regular Averages**

Imagine you're trying to find out the temperature outside. You have:
- Your guess: 80°F (you're just guessing)
- A cheap thermometer: 72°F (somewhat reliable)
- A weather station: 70°F (very reliable)

Regular average: (80 + 72 + 70) ÷ 3 = 74°F

But wait! Should your wild guess count as much as the weather station? Probably not!

**What is a Weighted Average?**

A **weighted average** gives more importance to some numbers than others.

**Formula**
```
Weighted Average = (value₁ × weight₁ + value₂ × weight₂ + ...) ÷ (sum of weights)
```

**Example: Temperature with Weights**

Let's give weights based on reliability:
- Your guess: 80°F, weight = 1 (not very reliable)
- Cheap thermometer: 72°F, weight = 3 (somewhat reliable)
- Weather station: 70°F, weight = 6 (very reliable)

```
Weighted Average = (80×1 + 72×3 + 70×6) ÷ (1+3+6)
                 = (80 + 216 + 420) ÷ 10
                 = 716 ÷ 10
                 = 71.6°F
```

Notice: 71.6°F is much closer to the weather station (70°F) than to your guess (80°F)!

**Why Weights Matter**

**Example 1: School Grades**

Your teacher calculates your grade:
- Homework: 85% (weight = 20%)
- Quizzes: 90% (weight = 30%)
- Final exam: 78% (weight = 50%)

**Regular average**: (85 + 90 + 78) ÷ 3 = 84.3%

**Weighted average**:
```
(85×0.20 + 90×0.30 + 78×0.50)
= 17 + 27 + 39
= 83%
```

The final exam counts more, so it pulls your grade down!

**Example 2: Combining Measurements**

You measure the length of a table:
- Measurement 1: 48.2 inches (uncertainty ± 0.5 inches)
- Measurement 2: 48.0 inches (uncertainty ± 0.1 inches)

Which measurement should you trust more? Measurement 2! It has lower uncertainty.

**Weights from Uncertainty**

Here's the key insight: **Lower uncertainty = Higher weight**

**The Rule**
```
Weight = 1 ÷ (Uncertainty²)
```

Or in math terms:
```
Weight = 1 ÷ Variance
```

**Example: Table Measurements**

**Measurement 1**: 48.2 inches, uncertainty = 0.5
- Variance = 0.5² = 0.25
- Weight = 1 ÷ 0.25 = 4

**Measurement 2**: 48.0 inches, uncertainty = 0.1
- Variance = 0.1² = 0.01
- Weight = 1 ÷ 0.01 = 100

Measurement 2 gets 25 times more weight! (100 vs 4)

**Weighted average**:
```
(48.2×4 + 48.0×100) ÷ (4+100)
= (192.8 + 4800) ÷ 104
= 4992.8 ÷ 104
= 48.01 inches
```

The result is very close to the more accurate measurement!

**The Kalman Filter Way**

The Kalman filter uses weighted averages to combine:
1. **Predictions** (what you expect)
2. **Measurements** (what you observe)

Each has its own uncertainty, so each gets its own weight!

**Example: Tracking a Car**

**Prediction**: The car is at position 100 meters (uncertainty ± 5 meters)
**Measurement**: GPS says 95 meters (uncertainty ± 10 meters)

Which should you trust more? The prediction! It has lower uncertainty.

**Prediction weight**: 1 ÷ 5² = 1 ÷ 25 = 0.04
**Measurement weight**: 1 ÷ 10² = 1 ÷ 100 = 0.01

**Weighted estimate**:
```
(100×0.04 + 95×0.01) ÷ (0.04+0.01)
= (4 + 0.95) ÷ 0.05
= 4.95 ÷ 0.05
= 99 meters
```

The estimate is closer to the prediction (100m) than the measurement (95m) because the prediction is more certain!

**Kalman Gain: The Magic Number**

The Kalman filter uses something called **Kalman Gain** (K) to decide how much to trust the measurement vs. the prediction.

**Formula**
```
K = Prediction Uncertainty ÷ (Prediction Uncertainty + Measurement Uncertainty)
```

**What K Means**

- **K = 0**: Don't trust the measurement at all (use prediction)
- **K = 1**: Don't trust the prediction at all (use measurement)
- **K = 0.5**: Trust both equally

**Example: Car Tracking**

Prediction uncertainty: 5 meters
Measurement uncertainty: 10 meters

```
K = 5² ÷ (5² + 10²)
  = 25 ÷ (25 + 100)
  = 25 ÷ 125
  = 0.2
```

**Update formula**:
```
New Estimate = Prediction + K × (Measurement - Prediction)
             = 100 + 0.2 × (95 - 100)
             = 100 + 0.2 × (-5)
             = 100 - 1
             = 99 meters
```

Same answer as the weighted average!

**Understanding Kalman Gain**

**Case 1: Very Certain Prediction**

Prediction: 100 ± 1 meter
Measurement: 95 ± 10 meters

```
K = 1² ÷ (1² + 10²) = 1 ÷ 101 ≈ 0.01
```

K is very small! Trust the prediction more.

```
New Estimate = 100 + 0.01 × (95 - 100)
             = 100 + 0.01 × (-5)
             = 100 - 0.05
             = 99.95 meters
```

Very close to the prediction (100m)!

**Case 2: Very Certain Measurement**

Prediction: 100 ± 10 meters
Measurement: 95 ± 1 meter

```
K = 10² ÷ (10² + 1²) = 100 ÷ 101 ≈ 0.99
```

K is almost 1! Trust the measurement more.

```
New Estimate = 100 + 0.99 × (95 - 100)
             = 100 + 0.99 × (-5)
             = 100 - 4.95
             = 95.05 meters
```

Very close to the measurement (95m)!

</details>

### 案例 3：不确定度相等

预测：100 ± 5 米
测量：95 ± 5 米

```
K = 5² ÷ (5² + 5²) = 25 ÷ 50 = 0.5
```

K 是 0.5！两者同等可信。

```
New Estimate = 100 + 0.5 × (95 - 100)
             = 100 + 0.5 × (-5)
             = 100 - 2.5
             = 97.5 meters
```

正好在中间！

## 更新不确定度

把预测和测量结合之后，不确定度也会更新！

### 公式
```
New Uncertainty = (1 - K) × Prediction Uncertainty
```

### 示例

预测不确定度：5 米
K = 0.2

```
New Uncertainty = (1 - 0.2) × 5
                = 0.8 × 5
                = 4 meters
```

不确定度下降了！结合信息之后确定性更高。

### 关键洞察

**结合信息总能降低不确定度！**

即使两个来源都不确定，把它们结合起来也会更确定。

## 练习题

### 问题 1：简单加权平均

有三个测量值：
- A：50（权重 = 1）
- B：60（权重 = 2）
- C：55（权重 = 3）

计算加权平均。

### 问题 2：从不确定度到权重

两个测量值：
- 测量 1：100 ± 5
- 测量 2：110 ± 10

a) 计算每个测量值的权重
b) 计算加权平均
c) 哪个测量值影响更大？为什么？

### 问题 3：卡尔曼增益

预测：75 ± 3
测量：80 ± 6

a) 计算卡尔曼增益（K）
b) 计算新的估计值
c) 计算新的不确定度
d) 不确定度是增大还是减小？

### 问题 4：极端情况

对每种情况，预测 K 会接近 0、0.5 还是 1：

a) 预测：50 ± 1，测量：60 ± 20
b) 预测：50 ± 20，测量：60 ± 1
c) 预测：50 ± 5，测量：60 ± 5

### 问题 5：真实场景

正在跟踪一架无人机：
- 物理模型预测：高度 = 100 英尺 ± 2 英尺
- 气压计测量：高度 = 95 英尺 ± 8 英尺

a) 计算 K
b) 高度的最佳估计是多少？
c) 新的不确定度是多少？

## 真实应用

### GPS + 惯性导航

手机把以下两者结合起来：
- **GPS**：精度高但更新慢（1 Hz）
- **加速度计**：更新快但会随时间漂移（100 Hz）

卡尔曼滤波用加权平均把两者结合起来！

### 机器人定位

机器人使用：
- **轮式编码器**：测量轮子转过的距离
- **激光扫描仪**：测量到墙壁的距离

两者都有不确定度。加权平均给出最佳位置估计！

### 天气预报

气象学家把以下两者结合起来：
- **计算机模型**：预测天气
- **实际测量值**：来自气象站

加权平均给出你看到的预报！

## 自己动手试试！

### 实验 1：加权抛硬币

1. 抛一枚硬币 10 次，统计正面次数
2. 抛另一枚硬币 100 次，统计正面次数
3. 估计正面概率时，更信任哪个结果？
4. 计算加权平均（以抛掷次数为权重）

### 实验 2：用不同工具测量

1. 用直尺测量某个物体（5 次）
2. 用卷尺测量同一物体（5 次）
3. 分别计算两者的平均值和标准差
4. 根据标准差计算权重
5. 计算两个平均值的加权平均

### 实验 3：预测与测量

1. 从某一高度释放小球
2. 预测它落地需要多长时间（用物理公式：t = √(2h/g)）
3. 用秒表测量实际时间
4. 估计各自的不确定度
5. 计算加权平均

## 核心概念

1. **加权平均**给予更可靠的信息更高的权重
2. **权重 = 1 ÷ 方差**（不确定度越低，权重越高）
3. **卡尔曼增益**决定在多大程度上信任测量值而非预测值
4. **结合信息可以降低不确定度**
5. **卡尔曼滤波本质上就是一种聪明的加权平均**

## 接下来

既然已经理解了加权平均，下一章将讲解**预测** —— 如何根据事物现在的位置估计它们未来会在哪里！

---

**关键术语**
- **加权平均**：某些值比其他值权重更大的平均
- **权重**：赋予某个值的重要程度
- **卡尔曼增益**：卡尔曼滤波中赋予测量值的权重
- **方差**：不确定度的平方
- **更新**：结合预测和测量得到新的估计值


<details>
<summary>English original</summary>

**Case 3: Equal Uncertainty**

Prediction: 100 ± 5 meters
Measurement: 95 ± 5 meters

```
K = 5² ÷ (5² + 5²) = 25 ÷ 50 = 0.5
```

K is 0.5! Trust both equally.

```
New Estimate = 100 + 0.5 × (95 - 100)
             = 100 + 0.5 × (-5)
             = 100 - 2.5
             = 97.5 meters
```

Right in the middle!

**Updating Uncertainty**

After combining prediction and measurement, the uncertainty also updates!

**Formula**
```
New Uncertainty = (1 - K) × Prediction Uncertainty
```

**Example**

Prediction uncertainty: 5 meters
K = 0.2

```
New Uncertainty = (1 - 0.2) × 5
                = 0.8 × 5
                = 4 meters
```

The uncertainty decreased! We're more certain after combining information.

**Key Insight**

**Combining information always reduces uncertainty!**

Even if both sources are uncertain, combining them makes you more certain.

**Practice Problems**

**Problem 1: Simple Weighted Average**

You have three measurements:
- A: 50 (weight = 1)
- B: 60 (weight = 2)
- C: 55 (weight = 3)

Calculate the weighted average.

**Problem 2: From Uncertainty to Weights**

Two measurements:
- Measurement 1: 100 ± 5
- Measurement 2: 110 ± 10

a) Calculate the weight for each measurement
b) Calculate the weighted average
c) Which measurement had more influence? Why?

**Problem 3: Kalman Gain**

Prediction: 75 ± 3
Measurement: 80 ± 6

a) Calculate the Kalman Gain (K)
b) Calculate the new estimate
c) Calculate the new uncertainty
d) Did the uncertainty increase or decrease?

**Problem 4: Extreme Cases**

For each case, predict whether K will be close to 0, 0.5, or 1:

a) Prediction: 50 ± 1, Measurement: 60 ± 20
b) Prediction: 50 ± 20, Measurement: 60 ± 1
c) Prediction: 50 ± 5, Measurement: 60 ± 5

**Problem 5: Real World**

You're tracking a drone:
- Your physics model predicts: altitude = 100 feet ± 2 feet
- Barometer measures: altitude = 95 feet ± 8 feet

a) Calculate K
b) What's your best estimate of the altitude?
c) What's the new uncertainty?

**Real World Applications**

**GPS + Inertial Navigation**

Your phone combines:
- **GPS**: Accurate but slow updates (1 Hz)
- **Accelerometer**: Fast but drifts over time (100 Hz)

The Kalman filter uses weighted averages to combine both!

**Robot Localization**

A robot uses:
- **Wheel encoders**: Measure how far wheels turned
- **Laser scanner**: Measures distance to walls

Both have uncertainty. Weighted average gives best position estimate!

**Weather Forecasting**

Meteorologists combine:
- **Computer models**: Predict weather
- **Actual measurements**: From weather stations

Weighted average gives the forecast you see!

**Try It Yourself!**

**Experiment 1: Weighted Coin Flip**

1. Flip a coin 10 times, count heads
2. Flip a different coin 100 times, count heads
3. Which result do you trust more for estimating probability of heads?
4. Calculate a weighted average (weight by number of flips)

**Experiment 2: Measuring with Different Tools**

1. Measure something with a ruler (5 times)
2. Measure the same thing with a tape measure (5 times)
3. Calculate average and standard deviation for each
4. Calculate weights based on standard deviations
5. Calculate weighted average of the two averages

**Experiment 3: Prediction vs Measurement**

1. Drop a ball from a height
2. Predict how long it will take to hit the ground (use physics: t = √(2h/g))
3. Measure the actual time with a stopwatch
4. Estimate uncertainty for each
5. Calculate weighted average

**Key Concepts**

1. **Weighted averages** give more importance to more reliable information
2. **Weight = 1 ÷ Variance** (lower uncertainty = higher weight)
3. **Kalman Gain** determines how much to trust measurement vs prediction
4. **Combining information reduces uncertainty**
5. **The Kalman filter is essentially a smart weighted average**

**What's Next?**

Now that we understand weighted averages, the next chapter will teach us about **prediction** - how to estimate where things will be in the future based on where they are now!

---

**Key Vocabulary**
- **Weighted Average**: Average where some values count more than others
- **Weight**: How much importance to give a value
- **Kalman Gain**: The weight given to the measurement in Kalman filter
- **Variance**: Uncertainty squared
- **Update**: Combining prediction and measurement to get new estimate

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/05-weighted-averages.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/05-weighted-averages.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
