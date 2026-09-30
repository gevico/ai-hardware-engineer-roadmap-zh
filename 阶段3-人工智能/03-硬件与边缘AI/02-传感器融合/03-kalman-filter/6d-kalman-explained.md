---
title: 6D 卡尔曼滤波简明解释
description: 6D 卡尔曼滤波简明解释
---

# 6D 卡尔曼滤波简明解释

## 什么是 6D 卡尔曼滤波？

6D 卡尔曼滤波同时跟踪 **6 个量**：
- 3D 位置：目标在哪里（x, y, z）
- 3D 速度：它移动多快（vx, vy, vz）

可以类比跟踪一架在空中飞行的无人机 —— 你既想知道它在哪，又想知道它在三个方向上各飞得多快。

## 全局概览

设想你用 GPS 跟踪一架无人机：
- GPS 告诉你无人机**在哪**（但带有一定误差）
- GPS 不会告诉你它**移动多快**
- 卡尔曼滤波会自动推算出速度！

### 工作原理

滤波器基于一个简单的想法：
> "如果我知道目标刚才在哪，又知道它现在在哪，我就能推算出它移动多快！"

## 状态向量

**状态向量**就像一份成绩单，里面装着我们已知的全部信息：

```
State = [x ]  ← position in x direction (meters)
        [y ]  ← position in y direction (meters)
        [z ]  ← position in z direction (meters)
        [vx]  ← velocity in x direction (meters/second)
        [vy]  ← velocity in y direction (meters/second)
        [vz]  ← velocity in z direction (meters/second)
```

### 状态示例

```
State = [10.5]  ← 10.5 meters east
        [8.2 ]  ← 8.2 meters north
        [3.0 ]  ← 3.0 meters up
        [2.0 ]  ← moving 2 m/s east
        [1.5 ]  ← moving 1.5 m/s north
        [0.5 ]  ← moving 0.5 m/s up
```

它告诉我们：无人机位于位置 (10.5, 8.2, 3.0)，以速度 (2.0, 1.5, 0.5) 移动。

## 两步舞

卡尔曼滤波反复做两件事：

### 第 1 步：PREDICT（它将在哪？）

**问题**："根据它现在的位置和移动速度，0.1 秒后它会在哪？"

**数学**（简化版）：
```
new_position = old_position + velocity × time
new_velocity = old_velocity (stays the same)
```

**示例**：
```
Current: x = 10.5m, vx = 2.0 m/s
Time: 0.1 seconds

Predicted: x = 10.5 + 2.0 × 0.1 = 10.7m
           vx = 2.0 m/s (unchanged)
```

### 第 2 步：UPDATE（实际测到了什么？）

**问题**："GPS 说它在 10.6m，我的预测说是 10.7m。哪个才是真的？"

**答案**：用加权平均！
- 如果预测更确定 → 更信任预测
- 如果 GPS 更确定 → 更信任 GPS

**示例**：
```
Prediction: 10.7m (uncertainty ±0.5m)
GPS: 10.6m (uncertainty ±2.0m)

Prediction is more certain, so trust it more!
Final estimate: 10.68m (closer to prediction)
```

## 神奇之处：不测速度也能估计速度！

这是最精彩的部分！GPS 只给出位置，滤波器却推算出速度。

### 怎么做到的？

**观测 1**：时刻 0s，位置 = 10.0m
**观测 2**：时刻 1s，位置 = 12.0m

**滤波器的想法**：
> "1 秒里移动了 2 米，所以速度大约是 2 m/s！"

但它比这更聪明 —— 它会利用随时间积累的全部测量值，得到相当好的估计。

## 理解不确定性

滤波器不只跟踪位置和速度 —— 它还跟踪**自己有多确定**。

### 不确定性可视化

```
Very Uncertain:  [====±5m====]
Somewhat Certain: [==±2m==]
Very Certain:     [±0.5m]
```

### 不确定性如何变化

**PREDICT 期间**：不确定性增大
```
Before: Position ±0.5m
After:  Position ±0.8m (less certain because things can change)
```

**UPDATE 期间**：不确定性减小
```
Before: Position ±0.8m
After:  Position ±0.4m (more certain because we got new info!)
```

随时间推移，不确定性会振荡，但总体呈下降趋势：
```
Time:        0s    1s    2s    3s    4s    5s
Uncertainty: ±5m → ±3m → ±2m → ±1.5m → ±1m → ±0.8m
```

## 矩阵简明解释

别怕矩阵！它们只是把数学运算组织起来的方式。

### F 矩阵（状态转移）

**作用**：预测下一个状态

```
F = [1  0  0  dt  0   0 ]
    [0  1  0  0   dt  0 ]
    [0  0  1  0   0   dt]
    [0  0  0  1   0   0 ]
    [0  0  0  0   1   0 ]
    [0  0  0  0   0   1 ]
```

**含义**：
- 第 1 行：`new_x = old_x + vx × dt`（位置按速度 × 时间变化）
- 第 4 行：`new_vx = old_vx`（速度保持不变）

### H 矩阵（测量）

**作用**：从状态中提取我们能够测量的量

```
H = [1  0  0  0  0  0]  ← measure x position
    [0  1  0  0  0  0]  ← measure y position
    [0  0  1  0  0  0]  ← measure z position
```

**含义**：GPS 给出位置（x, y, z），但不给速度！

### Q 矩阵（过程噪声）

**作用**：表示"世界是不可预测的"

**为什么需要它**：
- 风可能推动无人机
- 电机输出可能略有波动
- 物理规律并不完美

**效果**：在预测期间增加不确定性

### R 矩阵（测量噪声）

**作用**：表示"GPS 并不完美"

```
R = [2.0  0    0  ]  ← x has ±√2 = ±1.4m error
    [0    2.0  0  ]  ← y has ±√2 = ±1.4m error
    [0    0    3.0]  ← z has ±√3 = ±1.7m error
```

**含义**：GPS 在高度（z）上的噪声比水平方向（x, y）更大


<details>
<summary>English original</summary>

**6D Kalman Filter Explained Simply**

**What is a 6D Kalman Filter?**

A 6D Kalman filter tracks **6 things at once**:
- 3D Position: Where something is (x, y, z)
- 3D Velocity: How fast it's moving (vx, vy, vz)

Think of it like tracking a drone flying through the air - you want to know both where it is AND how fast it's going in all three directions.

**The Big Picture**

Imagine you're tracking a drone with GPS:
- GPS tells you **where** the drone is (but with some error)
- GPS does NOT tell you **how fast** it's moving
- The Kalman filter figures out the velocity automatically!

**How Does It Work?**

The filter uses a simple idea:
> "If I know where something was a moment ago, and I know where it is now, I can figure out how fast it's moving!"

**The State Vector**

The **state vector** is like a report card that contains everything we know:

```
State = [x ]  ← position in x direction (meters)
        [y ]  ← position in y direction (meters)
        [z ]  ← position in z direction (meters)
        [vx]  ← velocity in x direction (meters/second)
        [vy]  ← velocity in y direction (meters/second)
        [vz]  ← velocity in z direction (meters/second)
```

**Example State**

```
State = [10.5]  ← 10.5 meters east
        [8.2 ]  ← 8.2 meters north
        [3.0 ]  ← 3.0 meters up
        [2.0 ]  ← moving 2 m/s east
        [1.5 ]  ← moving 1.5 m/s north
        [0.5 ]  ← moving 0.5 m/s up
```

This tells us: The drone is at position (10.5, 8.2, 3.0) and moving at velocity (2.0, 1.5, 0.5).

**The Two-Step Dance**

The Kalman filter does two things over and over:

**Step 1: PREDICT (Where will it be?)**

**The Question**: "Based on where it is now and how fast it's moving, where will it be in 0.1 seconds?"

**The Math** (simple version):
```
new_position = old_position + velocity × time
new_velocity = old_velocity (stays the same)
```

**Example**:
```
Current: x = 10.5m, vx = 2.0 m/s
Time: 0.1 seconds

Predicted: x = 10.5 + 2.0 × 0.1 = 10.7m
           vx = 2.0 m/s (unchanged)
```

**Step 2: UPDATE (What did we actually measure?)**

**The Question**: "GPS says it's at 10.6m. My prediction said 10.7m. What's the truth?"

**The Answer**: Use a weighted average!
- If prediction is more certain → trust it more
- If GPS is more certain → trust it more

**Example**:
```
Prediction: 10.7m (uncertainty ±0.5m)
GPS: 10.6m (uncertainty ±2.0m)

Prediction is more certain, so trust it more!
Final estimate: 10.68m (closer to prediction)
```

**The Magic: Estimating Velocity Without Measuring It!**

This is the coolest part! GPS only tells us position, but the filter figures out velocity.

**How?**

**Observation 1**: At time 0s, position = 10.0m
**Observation 2**: At time 1s, position = 12.0m

**The Filter Thinks**:
> "It moved 2 meters in 1 second, so velocity must be about 2 m/s!"

But it's smarter than that - it uses ALL the measurements over time to get a really good estimate.

**Understanding Uncertainty**

The filter doesn't just track position and velocity - it also tracks **how sure it is**.

**Uncertainty Visualization**

```
Very Uncertain:  [====±5m====]
Somewhat Certain: [==±2m==]
Very Certain:     [±0.5m]
```

**How Uncertainty Changes**

**During PREDICT**: Uncertainty GROWS
```
Before: Position ±0.5m
After:  Position ±0.8m (less certain because things can change)
```

**During UPDATE**: Uncertainty SHRINKS
```
Before: Position ±0.8m
After:  Position ±0.4m (more certain because we got new info!)
```

Over time, uncertainty oscillates but generally decreases:
```
Time:        0s    1s    2s    3s    4s    5s
Uncertainty: ±5m → ±3m → ±2m → ±1.5m → ±1m → ±0.8m
```

**The Matrices Explained Simply**

Don't be scared of matrices! They're just organized ways to do math.

**F Matrix (State Transition)**

**What it does**: Predicts the next state

```
F = [1  0  0  dt  0   0 ]
    [0  1  0  0   dt  0 ]
    [0  0  1  0   0   dt]
    [0  0  0  1   0   0 ]
    [0  0  0  0   1   0 ]
    [0  0  0  0   0   1 ]
```

**What it means**:
- Row 1: `new_x = old_x + vx × dt` (position changes by velocity × time)
- Row 4: `new_vx = old_vx` (velocity stays the same)

**H Matrix (Measurement)**

**What it does**: Extracts what we can measure from the state

```
H = [1  0  0  0  0  0]  ← measure x position
    [0  1  0  0  0  0]  ← measure y position
    [0  0  1  0  0  0]  ← measure z position
```

**What it means**: GPS gives us position (x, y, z) but NOT velocity!

**Q Matrix (Process Noise)**

**What it does**: Says "the world is unpredictable"

**Why we need it**:
- Wind might push the drone
- Motors might vary slightly
- Physics isn't perfect

**Effect**: Adds uncertainty during prediction

**R Matrix (Measurement Noise)**

**What it does**: Says "GPS isn't perfect"

```
R = [2.0  0    0  ]  ← x has ±√2 = ±1.4m error
    [0    2.0  0  ]  ← y has ±√2 = ±1.4m error
    [0    0    3.0]  ← z has ±√3 = ±1.7m error
```

**What it means**: GPS is noisier in altitude (z) than horizontal (x, y)

</details>

## 分步示例

一起追踪无人机 3 个时间步！

### 初始状态（t=0s）

```
Position: [0, 0, 0] meters
Velocity: [1, 0.5, 0.2] m/s
Uncertainty: ±10m for position, ±5 m/s for velocity
```

### 时间 t=0.1s

**PREDICT**：
```
New position = [0, 0, 0] + [1, 0.5, 0.2] × 0.1
             = [0.1, 0.05, 0.02] meters

New velocity = [1, 0.5, 0.2] m/s (unchanged)

Uncertainty: ±10.1m (grew slightly)
```

**GPS MEASUREMENT**：[0.15, 0.08, 0.01] meters（带噪声！）

**UPDATE**：
```
Prediction: [0.1, 0.05, 0.02] ± 10.1m
GPS:        [0.15, 0.08, 0.01] ± 1.4m

GPS is more certain, so trust it more!

Final estimate: [0.14, 0.07, 0.015] meters
Uncertainty: ±1.2m (much better!)
```

### 时间 t=0.2s

**PREDICT**：
```
Position = [0.14, 0.07, 0.015] + [1, 0.5, 0.2] × 0.1
         = [0.24, 0.12, 0.035] meters

Uncertainty: ±1.3m (grew slightly)
```

**GPS MEASUREMENT**：[0.22, 0.13, 0.04] meters

**UPDATE**：
```
Prediction: [0.24, 0.12, 0.035] ± 1.3m
GPS:        [0.22, 0.13, 0.04] ± 1.4m

About equal certainty, so average them!

Final estimate: [0.23, 0.125, 0.0375] meters
Uncertainty: ±0.9m (even better!)
```

### 时间 t=0.3s

以此类推……滤波器会越来越好！

## 为什么用 6D 而不是 3D？

你可能会问："为什么不只追踪位置（3D），然后自己算速度呢？"

### 简单做法的问题

**做法 1**：只用 GPS 位置
- ❌ 噪声很大
- ❌ 没有速度信息
- ❌ 无法预测未来位置

**做法 2**：计算速度 = (new_pos - old_pos) / time
- ❌ 噪声很大（位置的噪声会被放大！）
- ❌ 没有平滑
- ❌ 跳动剧烈

### 6D 卡尔曼滤波的优势

✅ 平滑的位置估计
✅ 平滑的速度估计
✅ 可以预测未来位置
✅ 能处理缺失的测量
✅ 对所有信息做最优融合
✅ 追踪不确定性

## 实际应用

### 1. 无人机导航

```
GPS → 6D Kalman Filter → Smooth position + velocity
                       → Control system
                       → Stable flight!
```

### 2. 自动驾驶汽车

```
GPS + Wheel sensors → 6D Kalman Filter → Car position + speed
                                       → Navigation system
                                       → Safe driving!
```

### 3. 智能手机定位

```
GPS + Accelerometer → 6D Kalman Filter → Your location + speed
                                       → Maps app
                                       → Accurate directions!
```

### 4. 火箭追踪

```
Radar → 6D Kalman Filter → Rocket position + velocity
                         → Trajectory prediction
                         → Mission control!
```

## 常见问题

### Q1：为什么预测过程中不确定性会增加？

**A**：因为未来是不确定的！即使你完全知道某个东西现在在哪里，也无法 100% 确定它未来会在哪里。事情是会变的！

### Q2：滤波器怎么知道该信任每个来源多少？

**A**：它使用不确定性数值！不确定性越低 = 越信任。

```
If prediction uncertainty = ±0.5m and GPS uncertainty = ±2m
→ Trust prediction 4× more than GPS!
```

### Q3：如果 GPS 完全错了怎么办？

**A**：滤波器会察觉！如果 GPS 给出离谱的结果（比如无人机瞬移了 100 米），那么新息（预测与测量之差）会非常大。滤波器可以检测并剔除离群值。

### Q4：它也能追踪加速度吗？

**A**：可以！把它做成 9D 滤波器：
```
State = [x, y, z, vx, vy, vz, ax, ay, az]
```

但那样更复杂，而且通常没必要。

## 可视化滤波器

### 螺旋轨迹示例

想象一架无人机一边盘旋上升一边飞行：

```
Top View (X-Y):          Side View (X-Z):

    ╱─╲                      ╱
   ╱   ╲                    ╱
  │  •  │                  ╱
   ╲   ╱                  ╱
    ╲─╱                  •────────

  Circular              Climbing
  motion                upward
```

6D 卡尔曼滤波追踪：
- X、Y 位置（圆周运动）
- Z 位置（爬升）
- VX、VY 速度（随圆周运动变化）
- VZ 速度（恒定向上）

### 滤波器看到的内容

```
Time 0s:  Position (0, 0, 0),    Velocity (1.0, 0.0, 0.2)
Time 1s:  Position (0.9, 0.1, 0.2), Velocity (0.9, 0.4, 0.2)
Time 2s:  Position (1.6, 0.5, 0.4), Velocity (0.7, 0.7, 0.2)
Time 3s:  Position (2.0, 1.0, 0.6), Velocity (0.4, 0.9, 0.2)
...
```

滤波器能平滑地追踪这一复杂运动！

## 关键要点

1. **6D = 位置 + 速度**：既追踪物体在哪里，也追踪它移动多快

2. **两个步骤**：
   - PREDICT：用物理规律推测下一状态
   - UPDATE：用测量值修正推测

3. **估计速度**：尽管 GPS 只测量位置！

4. **追踪不确定性**：知道自己的置信度

5. **最优**：数学上已证明是最佳线性估计器

6. **实时**：测量一到就能工作，无需等待


<details>
<summary>English original</summary>

**Step-by-Step Example**

Let's track a drone for 3 time steps!

**Initial State (t=0s)**

```
Position: [0, 0, 0] meters
Velocity: [1, 0.5, 0.2] m/s
Uncertainty: ±10m for position, ±5 m/s for velocity
```

**Time t=0.1s**

**PREDICT**:
```
New position = [0, 0, 0] + [1, 0.5, 0.2] × 0.1
             = [0.1, 0.05, 0.02] meters

New velocity = [1, 0.5, 0.2] m/s (unchanged)

Uncertainty: ±10.1m (grew slightly)
```

**GPS MEASUREMENT**: [0.15, 0.08, 0.01] meters (noisy!)

**UPDATE**:
```
Prediction: [0.1, 0.05, 0.02] ± 10.1m
GPS:        [0.15, 0.08, 0.01] ± 1.4m

GPS is more certain, so trust it more!

Final estimate: [0.14, 0.07, 0.015] meters
Uncertainty: ±1.2m (much better!)
```

**Time t=0.2s**

**PREDICT**:
```
Position = [0.14, 0.07, 0.015] + [1, 0.5, 0.2] × 0.1
         = [0.24, 0.12, 0.035] meters

Uncertainty: ±1.3m (grew slightly)
```

**GPS MEASUREMENT**: [0.22, 0.13, 0.04] meters

**UPDATE**:
```
Prediction: [0.24, 0.12, 0.035] ± 1.3m
GPS:        [0.22, 0.13, 0.04] ± 1.4m

About equal certainty, so average them!

Final estimate: [0.23, 0.125, 0.0375] meters
Uncertainty: ±0.9m (even better!)
```

**Time t=0.3s**

And so on... The filter keeps getting better!

**Why 6D Instead of 3D?**

You might ask: "Why not just track position (3D) and calculate velocity ourselves?"

**Problems with Simple Approach**

**Approach 1**: Just use GPS position
- ❌ Very noisy
- ❌ No velocity information
- ❌ Can't predict future position

**Approach 2**: Calculate velocity = (new_pos - old_pos) / time
- ❌ Very noisy (noise in position gets amplified!)
- ❌ No smoothing
- ❌ Jumps around a lot

**Benefits of 6D Kalman Filter**

✅ Smooth position estimates
✅ Smooth velocity estimates
✅ Can predict future positions
✅ Handles missing measurements
✅ Optimal combination of all information
✅ Tracks uncertainty

**Real-World Applications**

**1. Drone Navigation**

```
GPS → 6D Kalman Filter → Smooth position + velocity
                       → Control system
                       → Stable flight!
```

**2. Self-Driving Cars**

```
GPS + Wheel sensors → 6D Kalman Filter → Car position + speed
                                       → Navigation system
                                       → Safe driving!
```

**3. Smartphone Location**

```
GPS + Accelerometer → 6D Kalman Filter → Your location + speed
                                       → Maps app
                                       → Accurate directions!
```

**4. Rocket Tracking**

```
Radar → 6D Kalman Filter → Rocket position + velocity
                         → Trajectory prediction
                         → Mission control!
```

**Common Questions**

**Q1: Why does uncertainty grow during prediction?**

**A**: Because the future is uncertain! Even if you know exactly where something is now, you can't be 100% sure where it will be in the future. Things can change!

**Q2: How does the filter know how much to trust each source?**

**A**: It uses the uncertainty values! Lower uncertainty = more trust.

```
If prediction uncertainty = ±0.5m and GPS uncertainty = ±2m
→ Trust prediction 4× more than GPS!
```

**Q3: What if GPS is completely wrong?**

**A**: The filter will notice! If GPS says something crazy (like the drone teleported 100 meters), the innovation (difference between prediction and measurement) will be huge. The filter can detect and reject outliers.

**Q4: Can it track acceleration too?**

**A**: Yes! You'd make it a 9D filter:
```
State = [x, y, z, vx, vy, vz, ax, ay, az]
```

But that's more complex and often not needed.

**Visualizing the Filter**

**The Spiral Trajectory Example**

Imagine a drone flying in a spiral while climbing:

```
Top View (X-Y):          Side View (X-Z):

    ╱─╲                      ╱
   ╱   ╲                    ╱
  │  •  │                  ╱
   ╲   ╱                  ╱
    ╲─╱                  •────────

  Circular              Climbing
  motion                upward
```

The 6D Kalman filter tracks:
- X, Y positions (circular motion)
- Z position (climbing)
- VX, VY velocities (changing for circular motion)
- VZ velocity (constant upward)

**What the Filter Sees**

```
Time 0s:  Position (0, 0, 0),    Velocity (1.0, 0.0, 0.2)
Time 1s:  Position (0.9, 0.1, 0.2), Velocity (0.9, 0.4, 0.2)
Time 2s:  Position (1.6, 0.5, 0.4), Velocity (0.7, 0.7, 0.2)
Time 3s:  Position (2.0, 1.0, 0.6), Velocity (0.4, 0.9, 0.2)
...
```

The filter smoothly tracks this complex motion!

**Key Takeaways**

1. **6D = Position + Velocity**: Track where something is AND how fast it's moving

2. **Two Steps**:
   - PREDICT: Use physics to guess next state
   - UPDATE: Use measurements to correct the guess

3. **Estimates Velocity**: Even though GPS only measures position!

4. **Tracks Uncertainty**: Knows how confident it is

5. **Optimal**: Mathematically proven to be the best linear estimator

6. **Real-Time**: Works as measurements arrive, no need to wait

</details>

## 亲自试一试！

运行示例代码：
```bash
python kalman_6d.py
```

你会看到：
- 一条 3D 螺旋轨迹
- 带噪声的 GPS 测量值（红点）
- 平滑的卡尔曼估计（蓝线）
- 不确定性如何随时间减小
- 无需直接测量即可估计速度！

## 延伸阅读

- [第 10 章：矩阵形式](10-matrix-form.md) - 完整的数学处理
- [第 11 章：多维 KF](11-multidimensional-kf.md) - 更多示例
- [速查表](cheat-sheet.md) - 公式快速参考

---

**记住**：卡尔曼滤波只是一种把预测和测量结合起来的聪明办法。它就像一位非常出色的助手，它会：
1. 记住事物原来在哪
2. 预测它们将到哪
3. 核对测量值
4. 推断出真实情况
5. 永远不停学习！


<details>
<summary>English original</summary>

**Try It Yourself!**

Run the example code:
```bash
python kalman_6d.py
```

You'll see:
- A 3D spiral trajectory
- Noisy GPS measurements (red dots)
- Smooth Kalman estimates (blue line)
- How uncertainty decreases over time
- Velocity estimation without direct measurement!

**Further Reading**

- [Chapter 10: Matrix Form](10-matrix-form.md) - Full mathematical treatment
- [Chapter 11: Multidimensional KF](11-multidimensional-kf.md) - More examples
- [Cheat Sheet](cheat-sheet.md) - Quick reference for equations

---

**Remember**: The Kalman filter is just a smart way to combine predictions and measurements. It's like having a really good assistant who:
1. Remembers where things were
2. Predicts where they'll be
3. Checks measurements
4. Figures out the truth
5. Never stops learning!

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/6d-kalman-explained.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/6d-kalman-explained.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
