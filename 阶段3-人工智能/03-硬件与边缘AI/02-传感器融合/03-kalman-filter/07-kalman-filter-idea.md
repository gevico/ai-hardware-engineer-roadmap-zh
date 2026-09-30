---
title: 卡尔曼滤波的思想
description: 卡尔曼滤波的思想
---

# 卡尔曼滤波的思想

**难度：高中（16-18 岁）**

## 融会贯通

你已经学完了所有零件。现在看看它们如何拼成一个 **卡尔曼滤波**！

## 整体图景

卡尔曼滤波是一种算法，它：
1. 跟踪你知道的东西（状态 + 不确定性）
2. 预测接下来会发生什么
3. 测量实际发生了什么
4. 最优地融合预测与测量
5. 永远重复

这就像拥有一个非常聪明的助手，它：
- 记住一切
- 做出有理有据的猜测
- 倾听传感器
- 推断出真相
- 从不停止学习

## 两步之舞

### 第 1 步：预测（时间更新）

“我预期会发生什么？”

**输入**：
- 上一时刻的状态估计
- 上一时刻的不确定性
- 运动模型
- 经过的时间

**输出**：
- 预测状态
- 预测不确定性（更大！）

**示例**：跟踪一辆汽车
```
Previous: position=100m ± 2m, velocity=20m/s ± 1m/s
Time: 1 second passes
Predicted: position=120m ± 3m, velocity=20m/s ± 1m/s
```

### 第 2 步：更新（测量更新）

“我实际观察到了什么？”

**输入**：
- 预测状态
- 预测不确定性
- 测量值
- 测量不确定性

**输出**：
- 更新后的状态（加权平均！）
- 更新后的不确定性（更小！）

**示例**：接上面的例子
```
Predicted: position=120m ± 3m
Measured: position=118m ± 4m
Updated: position=119.3m ± 2.4m
```

注意：不确定性从 ±3m 降到 ±2.4m！

## 为什么它是最优的

对于带高斯噪声的线性系统，卡尔曼滤波是**数学上最优的**。这意味着：

1. **没有别的算法能做得更好**（在这些条件下）
2. **它让估计的误差最小**
3. **它是可能的加权平均中最优的**

### “最优”是什么意思

想象有 1000 种不同的方式融合预测与测量。卡尔曼滤波会自动找出平均误差最小的那唯一一种方式！

## 卡尔曼滤波循环

```
Initialize:
  - Set initial state
  - Set initial uncertainty

Loop forever:
  1. PREDICT:
     - Use motion model to predict next state
     - Increase uncertainty (process noise)

  2. UPDATE:
     - Get measurement from sensor
     - Calculate Kalman Gain
     - Update state (weighted average)
     - Decrease uncertainty

  3. Repeat
```

## 详细示例：跟踪一列火车

来跟踪一列匀速运动的火车。

### 设置

**状态**：[位置, 速度]
**运动模型**：匀速
**传感器**：GPS（只测位置）

### 初始条件（t=0）

```
State: [0m, 25m/s]
Uncertainty: P = [10² 0  ] = [100  0]
                 [0  5² ]   [0   25]
```

位置不确定性：±10m
速度不确定性：±5m/s

### t=1s：预测

**运动模型**：
```
new_position = old_position + velocity × Δt
new_velocity = old_velocity
```

**预测状态**：
```
position = 0 + 25×1 = 25m
velocity = 25m/s
State: [25m, 25m/s]
```

**预测不确定性**（变大！）：
```
P = [144  0] (position uncertainty grew to ±12m)
    [0   25] (velocity uncertainty stayed ±5m/s)
```

### t=1s：更新

**测量**：GPS 给出的位置 = 23m ± 5m

**卡尔曼增益**（对测量的信任程度）：
```
K = Predicted_Uncertainty / (Predicted_Uncertainty + Measurement_Uncertainty)
  = 144 / (144 + 25)
  = 144 / 169
  = 0.85
```

K=0.85 意味着相当信任测量值！

**更新后的位置**：
```
new_position = predicted + K × (measured - predicted)
             = 25 + 0.85 × (23 - 25)
             = 25 + 0.85 × (-2)
             = 25 - 1.7
             = 23.3m
```

**更新后的不确定性**（变小！）：
```
new_uncertainty = (1 - K) × predicted_uncertainty
                = (1 - 0.85) × 144
                = 0.15 × 144
                = 21.6
                = ±4.6m
```

现在确定多了！（±4.6m 对比 ±12m）

**t=1s 时的最终状态**：
```
State: [23.3m, 25m/s]
Uncertainty: [21.6  0  ]
             [0    25 ]
```

### t=2s：预测

```
position = 23.3 + 25×1 = 48.3m
velocity = 25m/s

Uncertainty grows again...
```

如此循环往复！

## 关键洞见

### 洞见 1：不确定性会振荡

```
PREDICT → Uncertainty increases
UPDATE → Uncertainty decreases
PREDICT → Uncertainty increases
UPDATE → Uncertainty decreases
...
```

随着时间推移，它会趋于一个稳定值！

### 洞见 2：卡尔曼增益会自适应

- 预测越确定：K 越小（信任预测）
- 测量越确定：K 越大（信任测量）
- 它会自动调整！

### 洞见 3：信息从不丢失

每一次测量都会改善你的估计。即使是有噪声的测量也有帮助！

### 洞见 4：可实时运行

你不需要等所有数据到齐。测量一到就处理！


<details>
<summary>English original</summary>

**The Kalman Filter Idea**

**Level: High School (Ages 16-18)**

**Putting It All Together**

You've learned all the pieces. Now let's see how they fit together into the **Kalman Filter**!

**The Big Picture**

The Kalman filter is an algorithm that:
1. Keeps track of what you know (state + uncertainty)
2. Predicts what will happen next
3. Measures what actually happened
4. Combines prediction and measurement optimally
5. Repeats forever

It's like having a really smart assistant that:
- Remembers everything
- Makes educated guesses
- Listens to sensors
- Figures out the truth
- Never stops learning

**The Two-Step Dance**

**Step 1: PREDICT (Time Update)**

"What do I expect to happen?"

**Inputs**:
- Previous state estimate
- Previous uncertainty
- Motion model
- Time elapsed

**Outputs**:
- Predicted state
- Predicted uncertainty (larger!)

**Example**: Tracking a car
```
Previous: position=100m ± 2m, velocity=20m/s ± 1m/s
Time: 1 second passes
Predicted: position=120m ± 3m, velocity=20m/s ± 1m/s
```

**Step 2: UPDATE (Measurement Update)**

"What did I actually observe?"

**Inputs**:
- Predicted state
- Predicted uncertainty
- Measurement
- Measurement uncertainty

**Outputs**:
- Updated state (weighted average!)
- Updated uncertainty (smaller!)

**Example**: Continuing from above
```
Predicted: position=120m ± 3m
Measured: position=118m ± 4m
Updated: position=119.3m ± 2.4m
```

Notice: Uncertainty decreased from ±3m to ±2.4m!

**Why It's Optimal**

The Kalman filter is **mathematically optimal** for linear systems with Gaussian noise. This means:

1. **No other algorithm can do better** (for these conditions)
2. **It minimizes the error** in your estimates
3. **It's the best possible weighted average**

**What "Optimal" Means**

Imagine 1000 different ways to combine prediction and measurement. The Kalman filter automatically finds the ONE way that gives the smallest average error!

**The Kalman Filter Loop**

```
Initialize:
  - Set initial state
  - Set initial uncertainty

Loop forever:
  1. PREDICT:
     - Use motion model to predict next state
     - Increase uncertainty (process noise)

  2. UPDATE:
     - Get measurement from sensor
     - Calculate Kalman Gain
     - Update state (weighted average)
     - Decrease uncertainty

  3. Repeat
```

**Detailed Example: Tracking a Train**

Let's track a train moving at constant velocity.

**Setup**

**State**: [position, velocity]
**Motion model**: Constant velocity
**Sensor**: GPS (measures position only)

**Initial Conditions (t=0)**

```
State: [0m, 25m/s]
Uncertainty: P = [10² 0  ] = [100  0]
                 [0  5² ]   [0   25]
```

Position uncertainty: ±10m
Velocity uncertainty: ±5m/s

**Time t=1s: PREDICT**

**Motion model**:
```
new_position = old_position + velocity × Δt
new_velocity = old_velocity
```

**Predicted state**:
```
position = 0 + 25×1 = 25m
velocity = 25m/s
State: [25m, 25m/s]
```

**Predicted uncertainty** (grows!):
```
P = [144  0] (position uncertainty grew to ±12m)
    [0   25] (velocity uncertainty stayed ±5m/s)
```

**Time t=1s: UPDATE**

**Measurement**: GPS says position = 23m ± 5m

**Kalman Gain** (how much to trust measurement):
```
K = Predicted_Uncertainty / (Predicted_Uncertainty + Measurement_Uncertainty)
  = 144 / (144 + 25)
  = 144 / 169
  = 0.85
```

K=0.85 means trust the measurement quite a bit!

**Updated position**:
```
new_position = predicted + K × (measured - predicted)
             = 25 + 0.85 × (23 - 25)
             = 25 + 0.85 × (-2)
             = 25 - 1.7
             = 23.3m
```

**Updated uncertainty** (decreases!):
```
new_uncertainty = (1 - K) × predicted_uncertainty
                = (1 - 0.85) × 144
                = 0.15 × 144
                = 21.6
                = ±4.6m
```

Much more certain now! (±4.6m vs ±12m)

**Final state at t=1s**:
```
State: [23.3m, 25m/s]
Uncertainty: [21.6  0  ]
             [0    25 ]
```

**Time t=2s: PREDICT**

```
position = 23.3 + 25×1 = 48.3m
velocity = 25m/s

Uncertainty grows again...
```

And the cycle continues!

**Key Insights**

**Insight 1: Uncertainty Oscillates**

```
PREDICT → Uncertainty increases
UPDATE → Uncertainty decreases
PREDICT → Uncertainty increases
UPDATE → Uncertainty decreases
...
```

Over time, it settles to a steady value!

**Insight 2: Kalman Gain Adapts**

- When prediction is certain: K is small (trust prediction)
- When measurement is certain: K is large (trust measurement)
- It automatically adjusts!

**Insight 3: Information Never Lost**

Every measurement improves your estimate. Even noisy measurements help!

**Insight 4: Works in Real-Time**

You don't need to wait for all data. Process measurements as they arrive!

</details>

## 凭什么叫“Kalman”？

Rudolf Kalman 于 1960 年为 NASA 发明了它。其革命性在于：

1. **递归**：只需当前状态，无需全部历史
2. **最优**：数学上已证明为最优解
3. **高效**：快到足以实时运行
4. **通用**：适用于许多不同问题

## 假设与局限

卡尔曼滤波在以下条件下效果最佳：

### ✓ 成立的假设

1. **线性系统**：状态线性变化
2. **高斯噪声**：误差服从钟形曲线
3. **模型已知**：知道物体如何运动
4. **白噪声**：误差相互独立

### ✗ 局限

1. **非线性系统**：需用扩展卡尔曼滤波
2. **非高斯噪声**：需用粒子滤波
3. **模型未知**：需用自适应滤波器
4. **离群值**：需用鲁棒方法

## 与其他方法的比较

### 简单平均

```
Estimate = Average of all measurements
```

**问题**：
- 对所有测量一视同仁
- 不使用运动模型
- 不适应变化的不确定性

### 滑动平均

```
Estimate = Average of last N measurements
```

**问题**：
- 窗口大小任意确定
- 不使用运动模型
- 不按不确定性加权

### 卡尔曼滤波

```
Estimate = Optimal weighted average of prediction and measurement
```

**优势**：
- 使用运动模型
- 按不确定性加权
- 自动适应
- 数学上最优

## 真实世界中的成功案例

### 阿波罗登月（1960 年代）

卡尔曼滤波引导阿波罗飞船飞抵月球！它融合了：
- 惯性测量（加速度计、陀螺仪）
- 星敏感器观测
- 地面雷达

### GPS 导航（1990 年代至今）

手机用卡尔曼滤波来：
- 融合 GPS 卫星
- 利用运动传感器
- 平滑噪声
- 在两次更新之间预测位置

### 自动驾驶汽车（2010 年代至今）

自动驾驶车辆用卡尔曼滤波来：
- 融合摄像头、雷达、激光雷达
- 跟踪其他车辆
- 估计自身位置
- 预测未来状态

### 天气预报

气象学家用卡尔曼滤波来：
- 融合天气模型
- 纳入测量值
- 更新预报
- 降低不确定性

## 练习题

### 问题 1：概念理解

一个机器人在跟踪自身位置。判断对错：

a) 不确定性随时间总是减小
b) 卡尔曼增益总是在 0 与 1 之间
c) 预测总是比测量更准确
d) 滤波器需要存储所有过去的测量值

### 问题 2：预测—更新序列

已知：
- 状态：[position=50m, velocity=10m/s]
- 位置不确定性：±5m
- 速度不确定性：±2m/s

2 秒后：
a) 预测位置是多少？
b) 若测量值为 68m ± 8m，计算卡尔曼增益
c) 更新后的位置是多少？

### 问题 3：不确定性分析

初始不确定性：±10m
PREDICT 之后：±12m
用测量值（±5m）UPDATE 之后：？

计算更新后的不确定性。

### 问题 4：卡尔曼增益的含义

对每种情形，判断 K 会接近 0、0.5 还是 1：

a) 预测：±2m，测量：±20m
b) 预测：±20m，测量：±2m
c) 预测：±5m，测量：±5m

### 问题 5：实际应用

你在跟踪一架无人机，配有：
- GPS：每 1 秒更新一次，精度 ±5m
- IMU：每 0.01 秒更新一次，每秒漂移 ±0.1m

设计一套卡尔曼滤波策略：
a) 状态向量是什么？
b) 何时 PREDICT？
c) 何时 UPDATE？
d) 短期哪个传感器更可靠？长期呢？

## 动手试一试！

### 实验 1：手工卡尔曼滤波

跟踪一个滚动的球：
1. 测量 t=0、1、2 秒时的位置
2. 由前两个点计算速度
3. PREDICT t=3 时的位置
4. MEASURE t=3 时的实际位置
5. 用加权平均 UPDATE 估计值
6. 对 t=4、5、6……重复

### 实验 2：不确定性跟踪

1. 从位置估计开始：0 ± 10m
2. 1 秒后 PREDICT（加入 ±2m 过程噪声）
3. 用测量值（±5m）UPDATE
4. 跟踪 10 个周期内不确定性如何变化
5. 绘制不确定性随时间的变化曲线

### 实验 3：卡尔曼增益的行为

对不同不确定性比值：
1. 预测：±1m，测量：±10m → 计算 K
2. 预测：±5m，测量：±5m → 计算 K
3. 预测：±10m，测量：±1m → 计算 K

绘制 K 随不确定性比值的变化曲线。

### 实验 4：对比

用以下方法跟踪同一物体：
1. 所有测量值的简单平均
2. 滑动平均（最近 3 次测量）
3. 卡尔曼滤波

哪种最准确？哪种响应最快？

## 核心概念

1. **两步流程**：先 Predict，再 Update
2. **不确定性振荡**：在 predict 中增大，在 update 中缩小
3. **卡尔曼增益**：自动平衡预测与测量
4. **最优**：最佳线性估计器
5. **递归**：只需当前状态
6. **实时**：数据到达即处理


<details>
<summary>English original</summary>

**What Makes It "Kalman"?**

Rudolf Kalman invented this in 1960 for NASA. What made it revolutionary:

1. **Recursive**: Only needs current state, not all history
2. **Optimal**: Mathematically proven to be best possible
3. **Efficient**: Fast enough to run in real-time
4. **General**: Works for many different problems

**Assumptions and Limitations**

The Kalman filter works best when:

**✓ Good Assumptions**

1. **Linear system**: State changes linearly
2. **Gaussian noise**: Errors follow bell curve
3. **Known models**: You know how things move
4. **White noise**: Errors are independent

**✗ Limitations**

1. **Non-linear systems**: Need Extended Kalman Filter
2. **Non-Gaussian noise**: Need Particle Filter
3. **Unknown models**: Need adaptive filters
4. **Outliers**: Need robust methods

**Comparison to Other Methods**

**Simple Average**

```
Estimate = Average of all measurements
```

**Problems**:
- Treats all measurements equally
- Doesn't use motion model
- Doesn't adapt to changing uncertainty

**Moving Average**

```
Estimate = Average of last N measurements
```

**Problems**:
- Arbitrary window size
- Doesn't use motion model
- Doesn't weight by uncertainty

**Kalman Filter**

```
Estimate = Optimal weighted average of prediction and measurement
```

**Advantages**:
- Uses motion model
- Weights by uncertainty
- Adapts automatically
- Mathematically optimal

**Real-World Success Stories**

**Apollo Moon Landing (1960s)**

The Kalman filter guided Apollo spacecraft to the moon! It combined:
- Inertial measurements (accelerometers, gyros)
- Star tracker observations
- Ground radar

**GPS Navigation (1990s-present)**

Your phone uses Kalman filtering to:
- Combine GPS satellites
- Use motion sensors
- Smooth out noise
- Predict position between updates

**Self-Driving Cars (2010s-present)**

Autonomous vehicles use Kalman filters to:
- Fuse camera, radar, lidar
- Track other vehicles
- Estimate own position
- Predict future states

**Weather Forecasting**

Meteorologists use Kalman filtering to:
- Combine weather models
- Incorporate measurements
- Update forecasts
- Reduce uncertainty

**Practice Problems**

**Problem 1: Conceptual Understanding**

A robot is tracking its position. Answer true or false:

a) Uncertainty always decreases over time
b) Kalman Gain is always between 0 and 1
c) Predictions are always more accurate than measurements
d) The filter needs to store all past measurements

**Problem 2: Predict-Update Sequence**

Given:
- State: [position=50m, velocity=10m/s]
- Position uncertainty: ±5m
- Velocity uncertainty: ±2m/s

After 2 seconds:
a) What's the predicted position?
b) If measurement is 68m ± 8m, calculate Kalman Gain
c) What's the updated position?

**Problem 3: Uncertainty Analysis**

Initial uncertainty: ±10m
After PREDICT: ±12m
After UPDATE with measurement (±5m): ?

Calculate the updated uncertainty.

**Problem 4: Kalman Gain Interpretation**

For each scenario, predict if K will be close to 0, 0.5, or 1:

a) Prediction: ±2m, Measurement: ±20m
b) Prediction: ±20m, Measurement: ±2m
c) Prediction: ±5m, Measurement: ±5m

**Problem 5: Real-World Application**

You're tracking a drone with:
- GPS: Updates every 1 second, ±5m accuracy
- IMU: Updates every 0.01 seconds, drifts ±0.1m per second

Design a Kalman filter strategy:
a) What's your state vector?
b) When do you PREDICT?
c) When do you UPDATE?
d) Which sensor is more reliable short-term? Long-term?

**Try It Yourself!**

**Experiment 1: Manual Kalman Filter**

Track a rolling ball:
1. Measure position at t=0, 1, 2 seconds
2. Calculate velocity from first two points
3. PREDICT position at t=3
4. MEASURE actual position at t=3
5. UPDATE estimate using weighted average
6. Repeat for t=4, 5, 6...

**Experiment 2: Uncertainty Tracking**

1. Start with position estimate: 0 ± 10m
2. PREDICT after 1 second (add ±2m process noise)
3. UPDATE with measurement (±5m)
4. Track how uncertainty changes over 10 cycles
5. Plot uncertainty vs time

**Experiment 3: Kalman Gain Behavior**

For different uncertainty ratios:
1. Prediction: ±1m, Measurement: ±10m → Calculate K
2. Prediction: ±5m, Measurement: ±5m → Calculate K
3. Prediction: ±10m, Measurement: ±1m → Calculate K

Plot K vs uncertainty ratio.

**Experiment 4: Comparison**

Track the same object with:
1. Simple average of all measurements
2. Moving average (last 3 measurements)
3. Kalman filter

Which is most accurate? Most responsive?

**Key Concepts**

1. **Two-step process**: Predict, then Update
2. **Uncertainty oscillates**: Grows in predict, shrinks in update
3. **Kalman Gain**: Automatically balances prediction vs measurement
4. **Optimal**: Best possible linear estimator
5. **Recursive**: Only needs current state
6. **Real-time**: Processes data as it arrives

</details>

## 接下来是什么？

下一章将用 Python 实现一个完整的 1D 卡尔曼滤波。你将看到实际代码并亲自运行它！

---

**关键术语**
- **卡尔曼滤波**：最优递归状态估计器
- **时间更新**：预测步骤
- **测量更新**：校正步骤
- **卡尔曼增益**：最优加权因子
- **递归**：仅使用当前状态，而非完整历史
- **最优**：最小化均方误差


<details>
<summary>English original</summary>

**What's Next?**

The next chapter will implement a complete 1D Kalman filter in Python. You'll see the actual code and run it yourself!

---

**Key Vocabulary**
- **Kalman Filter**: Optimal recursive state estimator
- **Time Update**: Prediction step
- **Measurement Update**: Correction step
- **Kalman Gain**: Optimal weighting factor
- **Recursive**: Uses only current state, not full history
- **Optimal**: Minimizes mean squared error

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/07-kalman-filter-idea.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/07-kalman-filter-idea.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
