---
title: 平均值与不确定性
description: 平均值与不确定性
---

# 平均值与不确定性

**级别：初中（13-15 岁）**

## 引言

你已经学过估计和合并信息。现在加入一些数学，让估计更精确！别担心——从简单的开始。

## 什么是平均值？

**平均值**（也叫**均值**）是找出一组数字“中间”值的方法。

### 公式
```
Average = (Sum of all numbers) ÷ (How many numbers)
```

### 例 1：考试成绩
你的考试成绩：85, 90, 88, 92, 85

平均值 = (85 + 90 + 88 + 92 + 85) ÷ 5 = 440 ÷ 5 = 88

你的平均分是 88。

### 例 2：测量身高
你测量身高 5 次：
- 65.2 英寸
- 65.5 英寸
- 65.1 英寸
- 65.4 英寸
- 65.3 英寸

平均值 = (65.2 + 65.5 + 65.1 + 65.4 + 65.3) ÷ 5 = 327.5 ÷ 5 = 65.5 英寸

平均值可能比任何单次测量都更接近你的真实身高！

## 为什么平均值对估计很重要

当测量带有噪声时，平均值通常是比任何单次测量更好的估计。

### 飞镖盘类比

想象向靶子投掷 10 支飞镖：
- 有些落在中心左侧
- 有些落在中心右侧
- 有些落得偏高
- 有些落得偏低

所有飞镖的**平均位置**可能非常接近你的瞄准点！

## 什么是不确定性？

**不确定性**是你对某件事有多不确定。就像说“我认为是 50，但我可能错大约 5。”

### 例：猜糖豆

你猜罐子里有 100 颗糖豆。

**低不确定性**：“我很确定在 95 到 105 之间”
**高不确定性**：“可能在 50 到 150 之间的任何地方”

## 度量不确定性：方差

**方差**度量数字相对平均值的分散程度。

### 例：两个学生

**学生 A 的考试成绩**：88, 89, 90, 89, 89
- 平均值：89
- 分数非常接近（低方差）
- 表现稳定！

**学生 B 的考试成绩**：70, 95, 85, 100, 95
- 平均值：89（平均分相同！）
- 分数分散（高方差）
- 表现不稳定！

### 计算方差

**第 1 步**：求平均值
**第 2 步**：求每个数字与平均值的距离
**第 3 步**：对这些差值平方
**第 4 步**：对平方后的差值求平均

#### 例：学生 A
分数：88, 89, 90, 89, 89
平均值：89

与平均值的差：
- 88 - 89 = -1
- 89 - 89 = 0
- 90 - 89 = 1
- 89 - 89 = 0
- 89 - 89 = 0

平方后的差：
- (-1)² = 1
- (0)² = 0
- (1)² = 1
- (0)² = 0
- (0)² = 0

方差 = (1 + 0 + 1 + 0 + 0) ÷ 5 = 0.4

#### 例：学生 B
分数：70, 95, 85, 100, 95
平均值：89

差值：-19, 6, -4, 11, 6
平方：361, 36, 16, 121, 36

方差 = (361 + 36 + 16 + 121 + 36) ÷ 5 = 114

学生 B 的方差高得多（114 vs 0.4）！

## 标准差

**标准差**是方差的平方根。它更容易理解，因为与测量值单位相同。

```
Standard Deviation = √Variance
```

**学生 A**：√0.4 = 0.63 分
**学生 B**：√114 = 10.68 分

学生 B 的分数变化约 11 分，而学生 A 的变化不到 1 分！

## 测量中的不确定性

### 例：温度传感器

你测量温度 10 次：
72, 73, 71, 72, 74, 72, 71, 73, 72, 71

**平均值**：72.1°F
**方差**：1.09
**标准差**：1.04°F

**这意味着**：温度可能是 72.1°F，误差约 1°F。

可以写成：**72.1 ± 1.0°F**

## 合并平均值

当有多个测量值时，可以把它们合并！

### 例：两个温度计

**温度计 A**（精度较低）：
- 测量值：70, 72, 71, 73, 70
- 平均值：71.2°F
- 标准差：1.3°F

**温度计 B**（精度较高）：
- 测量值：72.0, 72.1, 71.9, 72.0, 72.1
- 标准差：0.08°F
- 平均值：72.02°F

更应该信任哪个？温度计 B！它的不确定性低得多。

## 68-95-99.7 法则

对于正态分布数据（钟形曲线）：
- **68%** 的测量值落在 1 个标准差内
- **95%** 落在 2 个标准差内
- **99.7%** 落在 3 个标准差内

### 例：身高测量

你的平均身高：65.5 英寸
标准差：0.2 英寸

- 68% 的概率真实身高在 65.3 到 65.7 英寸之间
- 95% 的概率在 65.1 到 65.9 英寸之间
- 99.7% 的概率在 64.9 到 66.1 英寸之间

## 不确定性随测量次数增加而降低

测量的次数越多，你就越确定！

### 公式
```
Uncertainty of average = Standard Deviation ÷ √(Number of measurements)
```

### 例：测量桌子

一次测量：48 ± 0.5 英寸（不确定性 = 0.5）
四次测量：48 ± 0.25 英寸（不确定性 = 0.5 ÷ √4 = 0.25）
九次测量：48 ± 0.17 英寸（不确定性 = 0.5 ÷ √9 = 0.17）

更多测量 = 更确定！

## 与卡尔曼滤波的联系

卡尔曼滤波跟踪：
1. **估计值**（类似平均值）
2. **不确定性**（类似方差）

它在获得新信息时更新两者！


<details>
<summary>English original</summary>

**Averages and Uncertainty**

**Level: Middle School (Ages 13-15)**

**Introduction**

You've learned about estimation and combining information. Now let's add some math to make our estimates even better! Don't worry - we'll start simple.

**What is an Average?**

An **average** (also called the **mean**) is a way to find the "middle" of a group of numbers.

**Formula**
```
Average = (Sum of all numbers) ÷ (How many numbers)
```

**Example 1: Test Scores**
Your test scores: 85, 90, 88, 92, 85

Average = (85 + 90 + 88 + 92 + 85) ÷ 5 = 440 ÷ 5 = 88

Your average score is 88.

**Example 2: Measuring Height**
You measure your height 5 times:
- 65.2 inches
- 65.5 inches
- 65.1 inches
- 65.4 inches
- 65.3 inches

Average = (65.2 + 65.5 + 65.1 + 65.4 + 65.3) ÷ 5 = 327.5 ÷ 5 = 65.5 inches

The average is probably closer to your true height than any single measurement!

**Why Averages Matter for Estimation**

When you have noisy measurements, the average is usually a better estimate than any single measurement.

**The Dart Board Analogy**

Imagine throwing 10 darts at a target:
- Some land left of center
- Some land right of center
- Some land high
- Some land low

The **average position** of all darts is probably very close to where you were aiming!

**What is Uncertainty?**

**Uncertainty** is how much you're not sure about something. It's like saying "I think it's 50, but I could be wrong by about 5."

**Example: Guessing Jellybeans**

You guess there are 100 jellybeans in a jar.

**Low uncertainty**: "I'm pretty sure it's between 95 and 105"
**High uncertainty**: "It could be anywhere from 50 to 150"

**Measuring Uncertainty: Variance**

**Variance** measures how spread out numbers are from the average.

**Example: Two Students**

**Student A's test scores**: 88, 89, 90, 89, 89
- Average: 89
- Scores are very close together (low variance)
- Consistent performance!

**Student B's test scores**: 70, 95, 85, 100, 95
- Average: 89 (same average!)
- Scores are spread out (high variance)
- Inconsistent performance!

**Calculating Variance**

**Step 1**: Find the average
**Step 2**: Find how far each number is from the average
**Step 3**: Square those differences
**Step 4**: Average the squared differences

**Example: Student A**
Scores: 88, 89, 90, 89, 89
Average: 89

Differences from average:
- 88 - 89 = -1
- 89 - 89 = 0
- 90 - 89 = 1
- 89 - 89 = 0
- 89 - 89 = 0

Squared differences:
- (-1)² = 1
- (0)² = 0
- (1)² = 1
- (0)² = 0
- (0)² = 0

Variance = (1 + 0 + 1 + 0 + 0) ÷ 5 = 0.4

**Example: Student B**
Scores: 70, 95, 85, 100, 95
Average: 89

Differences: -19, 6, -4, 11, 6
Squared: 361, 36, 16, 121, 36

Variance = (361 + 36 + 16 + 121 + 36) ÷ 5 = 114

Student B has much higher variance (114 vs 0.4)!

**Standard Deviation**

**Standard deviation** is the square root of variance. It's easier to understand because it's in the same units as your measurements.

```
Standard Deviation = √Variance
```

**Student A**: √0.4 = 0.63 points
**Student B**: √114 = 10.68 points

Student B's scores vary by about 11 points, while Student A's vary by less than 1 point!

**Uncertainty in Measurements**

**Example: Temperature Sensor**

You measure temperature 10 times:
72, 73, 71, 72, 74, 72, 71, 73, 72, 71

**Average**: 72.1°F
**Variance**: 1.09
**Standard Deviation**: 1.04°F

**What this means**: The temperature is probably 72.1°F, give or take about 1°F.

We can write this as: **72.1 ± 1.0°F**

**Combining Averages**

When you have multiple measurements, you can combine them!

**Example: Two Thermometers**

**Thermometer A** (less accurate):
- Measurements: 70, 72, 71, 73, 70
- Average: 71.2°F
- Standard deviation: 1.3°F

**Thermometer B** (more accurate):
- Measurements: 72.0, 72.1, 71.9, 72.0, 72.1
- Standard deviation: 0.08°F
- Average: 72.02°F

Which should you trust more? Thermometer B! It has much lower uncertainty.

**The 68-95-99.7 Rule**

For normally distributed data (bell curve):
- **68%** of measurements fall within 1 standard deviation
- **95%** fall within 2 standard deviations
- **99.7%** fall within 3 standard deviations

**Example: Height Measurements**

Your average height: 65.5 inches
Standard deviation: 0.2 inches

- 68% chance your true height is between 65.3 and 65.7 inches
- 95% chance it's between 65.1 and 65.9 inches
- 99.7% chance it's between 64.9 and 66.1 inches

**Uncertainty Decreases with More Measurements**

The more measurements you take, the more certain you become!

**Formula**
```
Uncertainty of average = Standard Deviation ÷ √(Number of measurements)
```

**Example: Measuring a Table**

One measurement: 48 ± 0.5 inches (uncertainty = 0.5)
Four measurements: 48 ± 0.25 inches (uncertainty = 0.5 ÷ √4 = 0.25)
Nine measurements: 48 ± 0.17 inches (uncertainty = 0.5 ÷ √9 = 0.17)

More measurements = more certainty!

**Kalman Filter Connection**

The Kalman filter keeps track of:
1. **The estimate** (like an average)
2. **The uncertainty** (like variance)

It updates both as it gets new information!

</details>

## 练习题

### 问题 1：计算平均值与方差

测量一支铅笔的长度 5 次（单位：cm）：
19.2, 19.5, 19.3, 19.4, 19.1

a) 平均长度是多少？
b) 方差是多少？
c) 标准差是多少？
d) 把长度写成：平均值 ± 标准差

### 问题 2：比较不确定度

**Sensor A**：平均值 = 50，标准差 = 5
**Sensor B**：平均值 = 50，标准差 = 2

哪个传感器更可靠？为什么？

### 问题 3：更多测量

某物只测量一次，得到 100 ± 10。
如果测量 4 次并取平均，新的不确定度是多少？

### 问题 4：温度读数

一周的早晨气温：65, 67, 66, 68, 65, 66, 67

a) 平均气温是多少？
b) 标准差是多少？
c) 哪个温度范围包含 95% 的测量值？

## 实际应用

### GPS 精度

手机的 GPS 可能会显示：
- “你位于此位置 ± 5 meters”

±5 meters 就是不确定度！有时是 ±3 meters（更确定），有时是 ±20 meters（不那么确定）。

### 科学测量

科学家报告测量结果时总会带上不确定度：
- “光速是 299,792,458 ± 1 m/s”
- “病人的体温是 98.6 ± 0.2°F”

### 天气预报

“最高 75°F”实际意思是“大概在 73°F 到 77°F 之间”

预报存在不确定度！

## 动手试一试！

### 实验 1：反应时间

1. 使用在线反应时间测试
2. 测 20 次
3. 计算平均值
4. 计算标准差
5. 你的反应时间 ± 不确定度是多少？

### 实验 2：测量物体

1. 测量书桌长度 10 次
2. 计算平均值与标准差
3. 再测量 10 次
4. 计算新的平均值与标准差
5. 不确定度是否减小了？

### 实验 3：比较传感器

1. 用两把不同的尺子测量同一物体
2. 每把尺子各测 5 次
3. 分别计算平均值与标准差
4. 哪把尺子更一致（标准差更低）？

## 核心概念

1. **平均值** = 由多次测量得出的最佳单一估计
2. **方差** = 测量值的离散程度
3. **标准差** = 方差的平方根（更易解读）
4. **不确定度** = 你可能错多少
5. **更多测量** = 更小的不确定度

## 接下来学什么？

既然已经理解了平均值与不确定度，下一章将讲解 **加权平均** —— 当某些测量比其他测量更可靠时，如何把它们组合起来！

---

**关键术语**
- **均值/平均值**：各值之和除以个数
- **方差**：各值与均值之差的平方的平均值
- **标准差**：方差的平方根
- **不确定度**：估计值中可能含有的误差大小
- **正态分布**：许多自然测量值呈现的钟形曲线


<details>
<summary>English original</summary>

**Practice Problems**

**Problem 1: Calculate Average and Variance**

You measure the length of a pencil 5 times (in cm):
19.2, 19.5, 19.3, 19.4, 19.1

a) What's the average length?
b) What's the variance?
c) What's the standard deviation?
d) Write the length as: average ± standard deviation

**Problem 2: Comparing Uncertainty**

**Sensor A**: Average = 50, Standard Deviation = 5
**Sensor B**: Average = 50, Standard Deviation = 2

Which sensor is more reliable? Why?

**Problem 3: More Measurements**

You measure something once and get 100 ± 10.
If you take 4 measurements and average them, what's the new uncertainty?

**Problem 4: Temperature Readings**

Morning temperatures for a week: 65, 67, 66, 68, 65, 66, 67

a) What's the average temperature?
b) What's the standard deviation?
c) What temperature range contains 95% of the measurements?

**Real World Applications**

**GPS Accuracy**

Your phone's GPS might say:
- "You are at this location ± 5 meters"

The ±5 meters is the uncertainty! Sometimes it's ±3 meters (more certain), sometimes ±20 meters (less certain).

**Scientific Measurements**

Scientists always report measurements with uncertainty:
- "The speed of light is 299,792,458 ± 1 m/s"
- "The patient's temperature is 98.6 ± 0.2°F"

**Weather Forecasts**

"High of 75°F" really means "probably between 73°F and 77°F"

The forecast has uncertainty!

**Try It Yourself!**

**Experiment 1: Reaction Time**

1. Use an online reaction time test
2. Take the test 20 times
3. Calculate the average
4. Calculate the standard deviation
5. What's your reaction time ± uncertainty?

**Experiment 2: Measuring Objects**

1. Measure the length of your desk 10 times
2. Calculate average and standard deviation
3. Measure it 10 more times
4. Calculate the new average and standard deviation
5. Did the uncertainty decrease?

**Experiment 3: Comparing Sensors**

1. Use two different rulers to measure the same object
2. Take 5 measurements with each
3. Calculate average and standard deviation for each
4. Which ruler is more consistent (lower standard deviation)?

**Key Concepts**

1. **Average** = Best single estimate from multiple measurements
2. **Variance** = How spread out the measurements are
3. **Standard Deviation** = Square root of variance (easier to interpret)
4. **Uncertainty** = How much you might be wrong
5. **More measurements** = Less uncertainty

**What's Next?**

Now that we understand averages and uncertainty, the next chapter will teach us about **weighted averages** - how to combine measurements when some are more reliable than others!

---

**Key Vocabulary**
- **Mean/Average**: Sum of values divided by count
- **Variance**: Average of squared differences from mean
- **Standard Deviation**: Square root of variance
- **Uncertainty**: How much error might be in your estimate
- **Normal Distribution**: Bell curve shape of many natural measurements

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/04-averages-and-uncertainty.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/04-averages-and-uncertainty.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
