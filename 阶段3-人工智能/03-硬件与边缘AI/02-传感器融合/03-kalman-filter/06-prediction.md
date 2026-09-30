---
title: 预测
description: 预测
---

# 预测

**级别：初中（13-15 岁）**

## 引言

到目前为止，我们已经学习了如何组合测量值。但如果你需要在测量之前知道某物的位置呢？这就是**预测**的用武之地！

## 什么是预测？

**预测**是利用你现在所知来估计未来会发生什么。

### 示例 1：接球

当有人向你扔球时：
1. 你看到它现在的位置
2. 你看到它移动的速度
3. 你的大脑预测它 1 second 后会在哪里
4. 你把双手移到那个位置！

你不会等待球到达——你预测并行动！

### 示例 2：步行去学校

你在 8:00 AM 离开家。学校在 10 minutes 路程外。

**预测**：“我将在 8:10 AM 到达”

这个预测基于：
- 你现在的位置（家）
- 你步行的速度（1 minute per block）
- 学校有多远（10 blocks）

## 运动模型

**运动模型**描述物体如何移动。从简单的开始！

### 恒定位置模型

**规则**：物体不移动。

```
Future Position = Current Position
```

**示例**：桌上的书
- 现在：位置 = 距墙 5 feet
- 10 seconds 后：位置 = 距墙 5 feet

这适用于静止物体！

### 恒定速度模型

**规则**：物体以恒定速度移动。

```
Future Position = Current Position + Velocity × Time
```

**示例**：以 2 feet per second 滚动的玩具车
- 现在：位置 = 10 feet
- 速度：2 feet/second
- 5 seconds 后：位置 = 10 + (2 × 5) = 20 feet

### 恒定加速度模型

**规则**：物体稳定加速或减速。

```
Future Position = Current Position + Velocity × Time + ½ × Acceleration × Time²
```

**示例**：滚下斜坡的球
- 现在：位置 = 0 feet，速度 = 0 ft/s
- 加速度：2 ft/s²
- 3 seconds 后：位置 = 0 + 0×3 + ½×2×3² = 9 feet

## 状态与状态转移

### 什么是状态？

**状态**是你需要了解系统在某一时刻的全部信息。

**示例：跟踪一辆汽车**
- 状态 = [位置, 速度]
- 状态 = [100 meters, 20 m/s]

这告诉你汽车的位置以及它的速度。

### 状态转移

**状态转移**是状态随时间的变化。

**示例**：以恒定速度移动的汽车
```
Initial state: [position=100m, velocity=20m/s]
Time passes: 2 seconds
New state: [position=140m, velocity=20m/s]
```

位置改变了，但速度保持不变！

## 预测方程

### 恒定速度

**状态**：[位置, 速度]

**时间 Δt 后的预测**：
```
new_position = old_position + velocity × Δt
new_velocity = old_velocity
```

**示例**：
```
Current state: [100m, 20m/s]
Time step: 0.5 seconds

Predicted position = 100 + 20×0.5 = 110m
Predicted velocity = 20m/s

Predicted state: [110m, 20m/s]
```

### 恒定加速度

**状态**：[位置, 速度, 加速度]

**时间 Δt 后的预测**：
```
new_position = old_position + velocity×Δt + ½×acceleration×Δt²
new_velocity = old_velocity + acceleration×Δt
new_acceleration = old_acceleration
```

## 预测不确定性

预测并不完美！你预测的未来越远，确定性越低。

### 示例：天气预报

- 明天的预报：±2°F 不确定性
- 下周的预报：±5°F 不确定性
- 下个月的预报：±10°F 不确定性

**规则**：不确定性随时间增长！

### 为什么不确定性会增长

**示例**：玩具车滚动

你知道：
- 位置：10 ± 0.1 feet
- 速度：2 ± 0.2 feet/second

5 seconds 后：
- 位置 = 10 + 2×5 = 20 feet
- 但速度不确定性会影响位置！
- 位置不确定性 = 0.1 + 0.2×5 = 1.1 feet

预测为 20 ± 1.1 feet（不确定性大得多！）

## 过程噪声

真实系统不会遵循完美模型。总存在一些随机性，称为**过程噪声**。

### 示例：步行去学校

你的模型说：“我以恰好 1 block per minute 的速度行走”

现实：
- 有时你走得更快
- 有时你停下来系鞋带
- 有时你在人行横道等待

这种随机性就是过程噪声！

### 将过程噪声加入预测

```
Prediction Uncertainty = Model Uncertainty + Process Noise
```

**示例**：汽车跟踪
- 模型不确定性：±2 meters
- 过程噪声（风、路面颠簸）：±1 meter per second
- 3 seconds 后：±2 + ±1×3 = ±5 meters

## 预测-更新循环

卡尔曼滤波在两个步骤之间交替：

### 步骤 1：预测
“基于物理，物体应该在哪里？”

```
Predicted State = f(Previous State, Time)
Predicted Uncertainty = Previous Uncertainty + Process Noise
```

### 步骤 2：更新（测量）
“我的传感器实际看到什么？”

```
New State = Weighted Average(Prediction, Measurement)
New Uncertainty = Reduced (because we combined information!)
```


<details>
<summary>English original</summary>

**Prediction**

**Level: Middle School (Ages 13-15)**

**Introduction**

So far we've learned how to combine measurements. But what if you need to know where something is BEFORE you measure it? That's where **prediction** comes in!

**What is Prediction?**

**Prediction** is using what you know now to estimate what will happen in the future.

**Example 1: Catching a Ball**

When someone throws you a ball:
1. You see where it is NOW
2. You see how fast it's moving
3. Your brain PREDICTS where it will be in 1 second
4. You move your hands to that spot!

You don't wait for the ball to arrive - you predict and act!

**Example 2: Walking to School**

You leave home at 8:00 AM. School is 10 minutes away.

**Prediction**: "I'll arrive at 8:10 AM"

This prediction is based on:
- Where you are now (home)
- How fast you walk (1 minute per block)
- How far school is (10 blocks)

**Motion Models**

A **motion model** describes how things move. Let's start simple!

**Constant Position Model**

**Rule**: Things don't move.

```
Future Position = Current Position
```

**Example**: A book on a table
- Now: position = 5 feet from wall
- In 10 seconds: position = 5 feet from wall

This works for stationary objects!

**Constant Velocity Model**

**Rule**: Things move at a steady speed.

```
Future Position = Current Position + Velocity × Time
```

**Example**: A toy car rolling at 2 feet per second
- Now: position = 10 feet
- Velocity: 2 feet/second
- After 5 seconds: position = 10 + (2 × 5) = 20 feet

**Constant Acceleration Model**

**Rule**: Things speed up or slow down steadily.

```
Future Position = Current Position + Velocity × Time + ½ × Acceleration × Time²
```

**Example**: A ball rolling down a ramp
- Now: position = 0 feet, velocity = 0 ft/s
- Acceleration: 2 ft/s²
- After 3 seconds: position = 0 + 0×3 + ½×2×3² = 9 feet

**State and State Transition**

**What is State?**

**State** is everything you need to know about a system at one moment.

**Example: Tracking a car**
- State = [position, velocity]
- State = [100 meters, 20 m/s]

This tells you where the car is AND how fast it's going.

**State Transition**

**State transition** is how the state changes over time.

**Example**: Car moving at constant velocity
```
Initial state: [position=100m, velocity=20m/s]
Time passes: 2 seconds
New state: [position=140m, velocity=20m/s]
```

The position changed, but velocity stayed the same!

**Prediction Equations**

**For Constant Velocity**

**State**: [position, velocity]

**Prediction after time Δt**:
```
new_position = old_position + velocity × Δt
new_velocity = old_velocity
```

**Example**:
```
Current state: [100m, 20m/s]
Time step: 0.5 seconds

Predicted position = 100 + 20×0.5 = 110m
Predicted velocity = 20m/s

Predicted state: [110m, 20m/s]
```

**For Constant Acceleration**

**State**: [position, velocity, acceleration]

**Prediction after time Δt**:
```
new_position = old_position + velocity×Δt + ½×acceleration×Δt²
new_velocity = old_velocity + acceleration×Δt
new_acceleration = old_acceleration
```

**Prediction Uncertainty**

Predictions aren't perfect! The further into the future you predict, the less certain you are.

**Example: Weather Forecast**

- Tomorrow's forecast: ±2°F uncertainty
- Next week's forecast: ±5°F uncertainty
- Next month's forecast: ±10°F uncertainty

**Rule**: Uncertainty grows with time!

**Why Uncertainty Grows**

**Example**: Toy car rolling

You know:
- Position: 10 ± 0.1 feet
- Velocity: 2 ± 0.2 feet/second

After 5 seconds:
- Position = 10 + 2×5 = 20 feet
- But velocity uncertainty affects position!
- Position uncertainty = 0.1 + 0.2×5 = 1.1 feet

The prediction is 20 ± 1.1 feet (much more uncertain!)

**Process Noise**

Real systems don't follow perfect models. There's always some randomness called **process noise**.

**Example: Walking to School**

Your model says: "I walk at exactly 1 block per minute"

Reality:
- Sometimes you walk faster
- Sometimes you stop to tie your shoe
- Sometimes you wait at a crosswalk

This randomness is process noise!

**Adding Process Noise to Predictions**

```
Prediction Uncertainty = Model Uncertainty + Process Noise
```

**Example**: Car tracking
- Model uncertainty: ±2 meters
- Process noise (wind, road bumps): ±1 meter per second
- After 3 seconds: ±2 + ±1×3 = ±5 meters

**The Predict-Update Cycle**

The Kalman filter alternates between two steps:

**Step 1: PREDICT**
"Based on physics, where should things be?"

```
Predicted State = f(Previous State, Time)
Predicted Uncertainty = Previous Uncertainty + Process Noise
```

**Step 2: UPDATE (Measure)**
"What do my sensors actually see?"

```
New State = Weighted Average(Prediction, Measurement)
New Uncertainty = Reduced (because we combined information!)
```

</details>

### 示例：跟踪无人机

**时间 0.0s**：
- 状态：[高度=100m，速度=5m/s 向上]
- 不确定性：±1m

**时间 0.5s - 预测**：
- 预测高度：100 + 5×0.5 = 102.5m
- 预测速度：5m/s
- 预测不确定性：±1.5m（增大了！）

**时间 0.5s - 更新**：
- 测量值：102m ± 2m
- 组合估计：102.3m ± 1.2m
- （不确定性减小了！）

**时间 1.0s - 预测**：
- 预测高度：102.3 + 5×0.5 = 104.8m
- 预测不确定性：±1.7m

以此类推……

## 练习题

### 问题 1：简单预测

一个机器人在位置 50cm 处，以 10cm/s 的速度移动。
3秒后它会在哪里？

### 问题 2：带加速度

一辆汽车从位置 0m 开始，速度为 0m/s。
它以 2m/s² 的加速度加速。
a) 5秒后它在哪里？
b) 5秒后它的速度是多少？

### 问题 3：不确定性增长

初始：位置 = 100 ± 2 米，速度 = 10 ± 1 m/s
4秒后，位置不确定性是多少？
（假设速度不确定性对位置有贡献）

### 问题 4：预测-更新循环

**初始状态**：[位置=0m，速度=5m/s]，不确定性=±1m

**步骤 1**：预测 2 秒后
**步骤 2**：测量位置=11m ± 2m
**步骤 3**：更新估计（使用卡尔曼增益）

最终估计是多少？

### 问题 5：过程噪声

你正在跟踪一个在颠簸表面上滚动的球。
- 初始不确定性：±0.5m
- 过程噪声：±0.3m 每秒
- 经过 10 秒的预测（无测量），不确定性是多少？

## 实际应用

### 自动驾驶汽车

汽车预测：
- “根据我的速度和转向，我将在 0.1秒后到达这里”
- 然后用摄像头和 GPS 测量
- 更新估计
- 每秒重复 10 次！

### 导弹跟踪

雷达跟踪导弹：
- 根据弹道预测它将在哪里
- 用雷达测量（有噪声！）
- 更新估计
- 预测下一个位置
- 重复

### 机器人导航

一个扫地机器人：
- 根据轮子转动预测位置
- 用墙壁传感器测量位置
- 更新估计
- 预测下一个位置
- 重复

### 天气预报

气象学家：
- 运行物理模型预测天气
- 从气象站获取测量值
- 更新模型
- 预测更远的未来
- 重复

## 自己动手试试！

### 实验 1：滚球

1. 让球在地板上滚动
2. 在 0s、1s、2s 时测量其位置
3. 根据前两次测量计算速度
4. 预测它在 3s 时将在哪里
5. 测量它在 3s 时实际在哪里
6. 你的预测有多接近？

### 实验 2：步行速度

1. 以稳定的步伐行走
2. 测量 10 秒后的距离
3. 计算你的速度
4. 预测你在 30 秒内能走多远
5. 实际行走 30 秒并测量
6. 你的预测接近吗？为什么接近或不接近？

### 实验 3：预测不确定性

1. 从不同高度丢下球
2. 预测它落地需要多长时间（使用 t = √(2h/g)）
3. 测量实际时间
4. 计算每个高度的预测误差
5. 误差会随高度增加吗？

### 实验 4：过程噪声

1. 设置一辆玩具车从斜坡滚下
2. 计时 5 次
3. 计算平均时间和标准差
4. 标准差就是你的过程噪声！
5. 用它来预测未来运行的不确定性

## 关键概念

1. **预测**使用当前状态来估计未来状态
2. **运动模型**描述物体如何移动（恒定速度、加速度等）
3. **状态**包含所需的所有信息（位置、速度等）
4. **不确定性在预测期间增长**
5. **过程噪声**考虑模型的不完美
6. **预测-更新循环**是卡尔曼滤波的核心

## 接下来是什么？

现在我们已经有了所有的拼图！下一章将把所有内容整合在一起，向你展示完整的**卡尔曼滤波**算法。准备好——这就是所有内容汇聚的地方！

---

**关键术语**
- **预测**：根据当前状态估计未来状态
- **运动模型**：描述物体如何移动的数学描述
- **状态**：描述系统所需的所有变量
- **状态转移**：状态如何随时间变化
- **过程噪声**：系统中的随机变化
- **预测-更新循环**：在预测和测量之间交替


<details>
<summary>English original</summary>

**Example: Tracking a Drone**

**Time 0.0s**:
- State: [altitude=100m, velocity=5m/s up]
- Uncertainty: ±1m

**Time 0.5s - PREDICT**:
- Predicted altitude: 100 + 5×0.5 = 102.5m
- Predicted velocity: 5m/s
- Predicted uncertainty: ±1.5m (grew!)

**Time 0.5s - UPDATE**:
- Measurement: 102m ± 2m
- Combined estimate: 102.3m ± 1.2m
- (Uncertainty decreased!)

**Time 1.0s - PREDICT**:
- Predicted altitude: 102.3 + 5×0.5 = 104.8m
- Predicted uncertainty: ±1.7m

And so on...

**Practice Problems**

**Problem 1: Simple Prediction**

A robot is at position 50cm, moving at 10cm/s.
Where will it be after 3 seconds?

**Problem 2: With Acceleration**

A car starts at position 0m with velocity 0m/s.
It accelerates at 2m/s².
a) Where is it after 5 seconds?
b) What's its velocity after 5 seconds?

**Problem 3: Uncertainty Growth**

Initial: position = 100 ± 2 meters, velocity = 10 ± 1 m/s
After 4 seconds, what's the position uncertainty?
(Assume velocity uncertainty contributes to position)

**Problem 4: Predict-Update Cycle**

**Initial state**: [position=0m, velocity=5m/s], uncertainty=±1m

**Step 1**: Predict after 2 seconds
**Step 2**: Measure position=11m ± 2m
**Step 3**: Update estimate (use Kalman gain)

What's the final estimate?

**Problem 5: Process Noise**

You're tracking a ball rolling on a bumpy surface.
- Initial uncertainty: ±0.5m
- Process noise: ±0.3m per second
- After 10 seconds of prediction (no measurements), what's the uncertainty?

**Real World Applications**

**Self-Driving Cars**

The car predicts:
- "Based on my speed and steering, I'll be here in 0.1 seconds"
- Then measures with cameras and GPS
- Updates the estimate
- Repeats 10 times per second!

**Missile Tracking**

Radar tracks a missile:
- Predict where it will be based on trajectory
- Measure with radar (noisy!)
- Update estimate
- Predict next position
- Repeat

**Robot Navigation**

A robot vacuum:
- Predicts position based on wheel rotations
- Measures position with wall sensors
- Updates estimate
- Predicts next position
- Repeat

**Weather Forecasting**

Meteorologists:
- Run physics models to predict weather
- Get measurements from weather stations
- Update the model
- Predict further into future
- Repeat

**Try It Yourself!**

**Experiment 1: Ball Rolling**

1. Roll a ball across the floor
2. Measure its position at 0s, 1s, 2s
3. Calculate velocity from first two measurements
4. PREDICT where it will be at 3s
5. MEASURE where it actually is at 3s
6. How close was your prediction?

**Experiment 2: Walking Speed**

1. Walk at a steady pace
2. Measure distance after 10 seconds
3. Calculate your velocity
4. PREDICT how far you'll walk in 30 seconds
5. Actually walk for 30 seconds and measure
6. Was your prediction close? Why or why not?

**Experiment 3: Prediction Uncertainty**

1. Drop a ball from different heights
2. Predict how long it will take to hit ground (use t = √(2h/g))
3. Measure actual time
4. Calculate prediction error for each height
5. Does error grow with height?

**Experiment 4: Process Noise**

1. Set up a toy car to roll down a ramp
2. Time it 5 times
3. Calculate average time and standard deviation
4. The standard deviation is your process noise!
5. Use this to predict uncertainty for future runs

**Key Concepts**

1. **Prediction** uses current state to estimate future state
2. **Motion models** describe how things move (constant velocity, acceleration, etc.)
3. **State** contains all information needed (position, velocity, etc.)
4. **Uncertainty grows** during prediction
5. **Process noise** accounts for model imperfections
6. **Predict-Update cycle** is the heart of Kalman filtering

**What's Next?**

Now we have all the pieces! The next chapter will put everything together and show you the complete **Kalman Filter** algorithm. Get ready - this is where it all comes together!

---

**Key Vocabulary**
- **Prediction**: Estimating future state from current state
- **Motion Model**: Mathematical description of how things move
- **State**: All variables needed to describe a system
- **State Transition**: How state changes over time
- **Process Noise**: Random variations in the system
- **Predict-Update Cycle**: Alternating between prediction and measurement

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/06-prediction.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/06-prediction.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
