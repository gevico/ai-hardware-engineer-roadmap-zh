---
title: 跟踪运动物体
description: 跟踪运动物体
---

# 跟踪运动物体

**难度级别：高中（16-18 岁）**

## 引言

上一章用 1D 卡尔曼滤波跟踪位置。现在同时跟踪位置和速度。这更强大，也更贴近实际！

## 状态向量

不再分别跟踪位置和速度，而是把它们合并为一个**状态向量**：

```
x = [position]
    [velocity]
```

这是一个 2×1 矩阵（2 行 1 列）。

### 示例
```
x = [100]  means: position = 100m
    [20 ]         velocity = 20m/s
```

## 运动模型（矩阵形式）

对于匀速运动：
```
position(t+Δt) = position(t) + velocity(t) × Δt
velocity(t+Δt) = velocity(t)
```

写成矩阵形式：
```
x(t+Δt) = F × x(t)

where F = [1  Δt]
          [0   1]
```

### 计算示例
```
Current state: x = [100]
                   [20 ]

Time step: Δt = 1 second

F = [1  1]
    [0  1]

New state: x_new = F × x
                 = [1  1] × [100]
                   [0  1]   [20 ]
                 = [100 + 20×1]
                   [20       ]
                 = [120]
                   [20 ]
```

位置更新为 120m，速度保持 20m/s！

## 协方差矩阵

不确定性现在是一个 2×2 矩阵，称为**协方差矩阵** P：

```
P = [σ²_position      σ_position,velocity]
    [σ_position,velocity    σ²_velocity   ]
```

### 各元素的含义

- **P[0,0]**：位置方差（位置的不确定性）
- **P[1,1]**：速度方差（速度的不确定性）
- **P[0,1] = P[1,0]**：协方差（位置误差与速度误差的关联程度）

### 示例
```
P = [4   0]
    [0   1]
```

这表示：
- 位置不确定性：±2m（√4）
- 速度不确定性：±1m/s（√1）
- 误差之间无相关性（非对角元素 = 0）

## 完整的 2D 卡尔曼滤波

### 初始化

```python
import numpy as np

# State vector [position, velocity]
x = np.array([[0.0],    # position
              [1.0]])   # velocity

# Covariance matrix
P = np.array([[10.0, 0.0],
              [0.0,  1.0]])

# Process noise covariance
Q = np.array([[0.01, 0.0],
              [0.0,  0.01]])

# Measurement noise (scalar, we only measure position)
R = 4.0
```

### 预测步

```python
def predict(x, P, F, Q):
    """
    Predict next state

    Args:
        x: state vector (2×1)
        P: covariance matrix (2×2)
        F: state transition matrix (2×2)
        Q: process noise covariance (2×2)

    Returns:
        x_pred, P_pred
    """
    # State prediction
    x_pred = F @ x

    # Covariance prediction
    P_pred = F @ P @ F.T + Q

    return x_pred, P_pred
```

### 更新步

```python
def update(x_pred, P_pred, z, H, R):
    """
    Update state with measurement

    Args:
        x_pred: predicted state (2×1)
        P_pred: predicted covariance (2×2)
        z: measurement (scalar)
        H: measurement matrix (1×2)
        R: measurement noise (scalar)

    Returns:
        x_new, P_new
    """
    # Innovation
    y = z - H @ x_pred

    # Innovation covariance
    S = H @ P_pred @ H.T + R

    # Kalman Gain
    K = P_pred @ H.T / S

    # State update
    x_new = x_pred + K * y

    # Covariance update
    I = np.eye(2)
    P_new = (I - K @ H) @ P_pred

    return x_new, P_new
```

## 测量矩阵

位置可以直接测量，速度则不能。**测量矩阵** H 从状态中提取位置：

```
H = [1  0]
```

这表示：measurement = 1×position + 0×velocity

### 示例
```
State: x = [100]
           [20 ]

Measurement: z = H × x
              = [1  0] × [100]
                         [20 ]
              = 100
```


<details>
<summary>English original</summary>

**Tracking a Moving Object**

**Level: High School (Ages 16-18)**

**Introduction**

In the previous chapter, we tracked position using a 1D Kalman filter. Now let's track BOTH position AND velocity simultaneously. This is more powerful and realistic!

**The State Vector**

Instead of tracking position and velocity separately, we combine them into a **state vector**:

```
x = [position]
    [velocity]
```

This is a 2×1 matrix (2 rows, 1 column).

**Example**
```
x = [100]  means: position = 100m
    [20 ]         velocity = 20m/s
```

**The Motion Model (Matrix Form)**

For constant velocity motion:
```
position(t+Δt) = position(t) + velocity(t) × Δt
velocity(t+Δt) = velocity(t)
```

In matrix form:
```
x(t+Δt) = F × x(t)

where F = [1  Δt]
          [0   1]
```

**Example Calculation**
```
Current state: x = [100]
                   [20 ]

Time step: Δt = 1 second

F = [1  1]
    [0  1]

New state: x_new = F × x
                 = [1  1] × [100]
                   [0  1]   [20 ]
                 = [100 + 20×1]
                   [20       ]
                 = [120]
                   [20 ]
```

Position updated to 120m, velocity stayed 20m/s!

**The Covariance Matrix**

Uncertainty is now a 2×2 matrix called the **covariance matrix** P:

```
P = [σ²_position      σ_position,velocity]
    [σ_position,velocity    σ²_velocity   ]
```

**What Each Element Means**

- **P[0,0]**: Position variance (uncertainty in position)
- **P[1,1]**: Velocity variance (uncertainty in velocity)
- **P[0,1] = P[1,0]**: Covariance (how position and velocity errors are related)

**Example**
```
P = [4   0]
    [0   1]
```

This means:
- Position uncertainty: ±2m (√4)
- Velocity uncertainty: ±1m/s (√1)
- No correlation between errors (off-diagonal = 0)

**Complete 2D Kalman Filter**

**Initialization**

```python
import numpy as np

# State vector [position, velocity]
x = np.array([[0.0],    # position
              [1.0]])   # velocity

# Covariance matrix
P = np.array([[10.0, 0.0],
              [0.0,  1.0]])

# Process noise covariance
Q = np.array([[0.01, 0.0],
              [0.0,  0.01]])

# Measurement noise (scalar, we only measure position)
R = 4.0
```

**Predict Step**

```python
def predict(x, P, F, Q):
    """
    Predict next state

    Args:
        x: state vector (2×1)
        P: covariance matrix (2×2)
        F: state transition matrix (2×2)
        Q: process noise covariance (2×2)

    Returns:
        x_pred, P_pred
    """
    # State prediction
    x_pred = F @ x

    # Covariance prediction
    P_pred = F @ P @ F.T + Q

    return x_pred, P_pred
```

**Update Step**

```python
def update(x_pred, P_pred, z, H, R):
    """
    Update state with measurement

    Args:
        x_pred: predicted state (2×1)
        P_pred: predicted covariance (2×2)
        z: measurement (scalar)
        H: measurement matrix (1×2)
        R: measurement noise (scalar)

    Returns:
        x_new, P_new
    """
    # Innovation
    y = z - H @ x_pred

    # Innovation covariance
    S = H @ P_pred @ H.T + R

    # Kalman Gain
    K = P_pred @ H.T / S

    # State update
    x_new = x_pred + K * y

    # Covariance update
    I = np.eye(2)
    P_new = (I - K @ H) @ P_pred

    return x_new, P_new
```

**The Measurement Matrix**

We measure position but not velocity directly. The **measurement matrix** H extracts position from the state:

```
H = [1  0]
```

This means: measurement = 1×position + 0×velocity

**Example**
```
State: x = [100]
           [20 ]

Measurement: z = H × x
              = [1  0] × [100]
                         [20 ]
              = 100
```

</details>

## Complete Implementation

```python
import numpy as np
import matplotlib.pyplot as plt

class KalmanFilter2D:
    def __init__(self, dt=1.0):
        """
        Initialize 2D Kalman Filter for position and velocity

        Args:
            dt: time step
        """
        self.dt = dt

        # State vector [position, velocity]
        self.x = np.array([[0.0],
                          [1.0]])

        # Covariance matrix
        self.P = np.array([[10.0, 0.0],
                          [0.0,  1.0]])

        # State transition matrix
        self.F = np.array([[1.0, dt],
                          [0.0, 1.0]])

        # Measurement matrix (measure position only)
        self.H = np.array([[1.0, 0.0]])

        # Process noise covariance
        self.Q = np.array([[0.01, 0.0],
                          [0.0,  0.01]])

        # Measurement noise
        self.R = 4.0

        # History
        self.history = {
            'x': [self.x[0, 0]],
            'v': [self.x[1, 0]],
            'P_x': [self.P[0, 0]],
            'P_v': [self.P[1, 1]]
        }

    def predict(self):
        """Prediction step"""
        # State prediction
        self.x = self.F @ self.x

        # Covariance prediction
        self.P = self.F @ self.P @ self.F.T + self.Q

        return self.x

    def update(self, z):
        """
        Update step with measurement

        Args:
            z: position measurement (scalar)
        """
        # Innovation
        y = z - self.H @ self.x

        # Innovation covariance
        S = self.H @ self.P @ self.H.T + self.R

        # Kalman Gain
        K = self.P @ self.H.T / S

        # State update
        self.x = self.x + K * y

        # Covariance update
        I = np.eye(2)
        self.P = (I - K @ self.H) @ self.P

        # Save history
        self.history['x'].append(self.x[0, 0])
        self.history['v'].append(self.x[1, 0])
        self.history['P_x'].append(self.P[0, 0])
        self.history['P_v'].append(self.P[1, 1])

        return self.x

    def get_state(self):
        """Get current state"""
        return self.x[0, 0], self.x[1, 0]


def simulate_tracking():
    """Simulate tracking with 2D Kalman filter"""

    # True system
    true_x = 0.0
    true_v = 1.0
    dt = 1.0

    # Create Kalman filter
    kf = KalmanFilter2D(dt=dt)

    # Storage
    times = [0]
    true_positions = [true_x]
    true_velocities = [true_v]
    measurements = []
    est_positions = [kf.x[0, 0]]
    est_velocities = [kf.x[1, 0]]

    # Simulate for 20 seconds
    np.random.seed(42)
    for t in range(1, 21):
        # True system evolves
        true_x = true_x + true_v * dt

        # Noisy measurement
        measurement = true_x + np.random.normal(0, 2.0)

        # Kalman filter
        kf.predict()
        kf.update(measurement)

        # Store results
        times.append(t)
        true_positions.append(true_x)
        true_velocities.append(true_v)
        measurements.append(measurement)
        est_positions.append(kf.x[0, 0])
        est_velocities.append(kf.x[1, 0])

    # Plot results
    fig, axes = plt.subplots(3, 1, figsize=(12, 10))

    # Position plot
    ax = axes[0]
    ax.plot(times, true_positions, 'g-', label='True Position', linewidth=2)
    ax.plot(times[1:], measurements, 'r.', label='Measurements', markersize=8)
    ax.plot(times, est_positions, 'b-', label='Kalman Estimate', linewidth=2)

    # Uncertainty bounds
    P_x = np.array(kf.history['P_x'])
    est_pos_array = np.array(est_positions)
    ax.fill_between(times,
                     est_pos_array - 2*np.sqrt(P_x),
                     est_pos_array + 2*np.sqrt(P_x),
                     alpha=0.3, color='blue', label='95% Confidence')

    ax.set_xlabel('Time (seconds)')
    ax.set_ylabel('Position (meters)')
    ax.set_title('Position Tracking')
    ax.legend()
    ax.grid(True)

    # Velocity plot
    ax = axes[1]
    ax.plot(times, true_velocities, 'g-', label='True Velocity', linewidth=2)
    ax.plot(times, est_velocities, 'b-', label='Estimated Velocity', linewidth=2)

    P_v = np.array(kf.history['P_v'])
    est_vel_array = np.array(est_velocities)
    ax.fill_between(times,
                     est_vel_array - 2*np.sqrt(P_v),
                     est_vel_array + 2*np.sqrt(P_v),
                     alpha=0.3, color='blue', label='95% Confidence')

    ax.set_xlabel('Time (seconds)')
    ax.set_ylabel('Velocity (m/s)')
    ax.set_title('Velocity Estimation (Not Directly Measured!)')
    ax.legend()
    ax.grid(True)

    # Uncertainty plot
    ax = axes[2]
    ax.plot(times, np.sqrt(P_x), 'b-', label='Position Uncertainty', linewidth=2)
    ax.plot(times, np.sqrt(P_v), 'r-', label='Velocity Uncertainty', linewidth=2)
    ax.set_xlabel('Time (seconds)')
    ax.set_ylabel('Uncertainty (std dev)')
    ax.set_title('Uncertainty Over Time')
    ax.legend()
    ax.grid(True)

    plt.tight_layout()
    plt.savefig('kalman_2d_tracking.png', dpi=150)
    plt.show()

    # Statistics
    pos_errors = np.array(est_positions) - np.array(true_positions)
    vel_errors = np.array(est_velocities) - np.array(true_velocities)

    print(f"Position RMSE: {np.sqrt(np.mean(pos_errors**2)):.3f} meters")
    print(f"Velocity RMSE: {np.sqrt(np.mean(vel_errors**2)):.3f} m/s")
    print(f"Final position estimate: {est_positions[-1]:.3f} m (true: {true_positions[-1]:.3f} m)")
    print(f"Final velocity estimate: {est_velocities[-1]:.3f} m/s (true: {true_velocities[-1]:.3f} m/s)")


if __name__ == "__main__":
    simulate_tracking()
```

## 关键观察

### 1. 无需直接测量即可估计速度！

奇妙之处在于：从不直接测量速度，滤波器却能准确估计它！

**如何做到？** 通过观察位置随时间的变化。

### 2. 协方差耦合

位置与速度的不确定性通过协方差矩阵耦合。位置估计得越准，速度估计也越准！

### 3. 收敛更快

2D 滤波器比 1D 滤波器收敛更快，因为它利用了位置与速度之间的关系。

## 矩阵维度汇总

```
State vector:           x (2×1)
Covariance matrix:      P (2×2)
State transition:       F (2×2)
Process noise:          Q (2×2)
Measurement matrix:     H (1×2)
Measurement noise:      R (1×1 or scalar)
Kalman Gain:            K (2×1)
```

## 可尝试的实验

### 实验 1：改变速度

修改仿真，使速度发生变化：

```python
# Add acceleration
true_a = 0.5  # m/s²
true_v = true_v + true_a * dt
```

滤波器会怎样？（提示：它会滞后，因为它假设速度恒定！）

### 实验 2：同时测量速度

加入速度测量：

```python
# Measure both position and velocity
H = np.array([[1.0, 0.0],
              [0.0, 1.0]])

R = np.array([[4.0, 0.0],
              [0.0, 1.0]])
```

这如何改善性能？

### 实验 3：相关噪声

为过程噪声加入相关性：

```python
Q = np.array([[0.01, 0.005],
              [0.005, 0.01]])
```

会产生什么影响？

### 实验 4：不同的时间步长

尝试 dt = 0.1 秒（更新更快）或 dt = 5.0 秒（更新更慢）。

这如何影响不确定性的增长？

## 练习题

### 问题 1：矩阵乘法

已知：
```
F = [1  2]    x = [10]
    [0  1]        [5 ]
```

手工计算 F × x。

### 问题 2：协方差预测

已知：
```
P = [4  0]    F = [1  1]    Q = [0.1  0  ]
    [0  1]        [0  1]        [0    0.1]
```

计算 P_pred = F × P × F^T + Q

### 问题 3：卡尔曼增益

已知：
```
P_pred = [5  0]    H = [1  0]    R = 2
         [0  1]
```

计算 K = P_pred × H^T × (H × P_pred × H^T + R)^(-1)

### 问题 4：设计挑战

为跟踪一辆汽车设计卡尔曼滤波，要求：
- 每 0.5 秒测量一次位置（准确率 ±3m）
- 每 2 秒测量一次速度（准确率 ±1m/s）
- 过程噪声为 ±0.5m 和 ±0.2m/s

你的 F、H、Q 和 R 矩阵是什么？

## 常见陷阱

### 陷阱 1：矩阵维度错误

```python
# WRONG: H should be 1×2, not 2×1
H = np.array([[1.0],
              [0.0]])

# RIGHT:
H = np.array([[1.0, 0.0]])
```

### 陷阱 2：忘记转置

```python
# WRONG: Missing .T
P_pred = F @ P @ F + Q

# RIGHT:
P_pred = F @ P @ F.T + Q
```

### 陷阱 3：标量与矩阵

```python
# WRONG: R should be scalar or 1×1 matrix
R = np.array([[4.0, 0.0],
              [0.0, 4.0]])

# RIGHT (for single measurement):
R = 4.0
# or
R = np.array([[4.0]])
```

## 关键要点

1. **状态向量**组合多个变量
2. **协方差矩阵**跟踪不确定性与相关性
3. **矩阵形式**可推广到任意维数
4. **速度可以估计**而无需直接测量
5. **耦合变量**帮助彼此更快收敛

## 接下来是什么？

下一章介绍完整的矩阵记法，并推广到 N 维。这就是完整、通用的卡尔曼滤波！

---

**关键术语**
- **状态向量**：包含所有状态变量的列矩阵
- **协方差矩阵**：方差与协方差构成的矩阵
- **状态转移矩阵（F）**：描述状态如何演化
- **测量矩阵（H）**：从状态中提取测量值
- **卡尔曼增益矩阵（K）**：对每个状态变量的最优加权


<details>
<summary>English original</summary>

**Key Observations**

**1. Velocity Estimation Without Direct Measurement!**

The amazing thing: We NEVER measure velocity directly, but the filter estimates it accurately!

**How?** By observing how position changes over time.

**2. Covariance Coupling**

Position and velocity uncertainties are coupled through the covariance matrix. Knowing position better helps estimate velocity better!

**3. Faster Convergence**

The 2D filter converges faster than the 1D filter because it uses the relationship between position and velocity.

**Matrix Dimensions Summary**

```
State vector:           x (2×1)
Covariance matrix:      P (2×2)
State transition:       F (2×2)
Process noise:          Q (2×2)
Measurement matrix:     H (1×2)
Measurement noise:      R (1×1 or scalar)
Kalman Gain:            K (2×1)
```

**Experiments to Try**

**Experiment 1: Changing Velocity**

Modify the simulation so velocity changes:

```python
# Add acceleration
true_a = 0.5  # m/s²
true_v = true_v + true_a * dt
```

What happens to the filter? (Hint: It will lag because it assumes constant velocity!)

**Experiment 2: Measure Velocity Too**

Add velocity measurements:

```python
# Measure both position and velocity
H = np.array([[1.0, 0.0],
              [0.0, 1.0]])

R = np.array([[4.0, 0.0],
              [0.0, 1.0]])
```

How does this improve performance?

**Experiment 3: Correlated Noise**

Add correlation to process noise:

```python
Q = np.array([[0.01, 0.005],
              [0.005, 0.01]])
```

What effect does this have?

**Experiment 4: Different Time Steps**

Try dt = 0.1 seconds (faster updates) or dt = 5.0 seconds (slower updates).

How does this affect uncertainty growth?

**Practice Problems**

**Problem 1: Matrix Multiplication**

Given:
```
F = [1  2]    x = [10]
    [0  1]        [5 ]
```

Calculate F × x by hand.

**Problem 2: Covariance Prediction**

Given:
```
P = [4  0]    F = [1  1]    Q = [0.1  0  ]
    [0  1]        [0  1]        [0    0.1]
```

Calculate P_pred = F × P × F^T + Q

**Problem 3: Kalman Gain**

Given:
```
P_pred = [5  0]    H = [1  0]    R = 2
         [0  1]
```

Calculate K = P_pred × H^T × (H × P_pred × H^T + R)^(-1)

**Problem 4: Design Challenge**

Design a Kalman filter for tracking a car that:
- Measures position every 0.5 seconds (±3m accuracy)
- Measures velocity every 2 seconds (±1m/s accuracy)
- Has process noise of ±0.5m and ±0.2m/s

What are your F, H, Q, and R matrices?

**Common Pitfalls**

**Pitfall 1: Wrong Matrix Dimensions**

```python
# WRONG: H should be 1×2, not 2×1
H = np.array([[1.0],
              [0.0]])

# RIGHT:
H = np.array([[1.0, 0.0]])
```

**Pitfall 2: Forgetting Transpose**

```python
# WRONG: Missing .T
P_pred = F @ P @ F + Q

# RIGHT:
P_pred = F @ P @ F.T + Q
```

**Pitfall 3: Scalar vs Matrix**

```python
# WRONG: R should be scalar or 1×1 matrix
R = np.array([[4.0, 0.0],
              [0.0, 4.0]])

# RIGHT (for single measurement):
R = 4.0
# or
R = np.array([[4.0]])
```

**Key Takeaways**

1. **State vector** combines multiple variables
2. **Covariance matrix** tracks uncertainty and correlations
3. **Matrix form** generalizes to any number of dimensions
4. **Velocity can be estimated** without direct measurement
5. **Coupled variables** help each other converge faster

**What's Next?**

The next chapter introduces the full matrix notation and generalizes to N dimensions. This is the complete, general Kalman filter!

---

**Key Vocabulary**
- **State Vector**: Column matrix containing all state variables
- **Covariance Matrix**: Matrix of variances and covariances
- **State Transition Matrix (F)**: Describes how state evolves
- **Measurement Matrix (H)**: Extracts measurements from state
- **Kalman Gain Matrix (K)**: Optimal weighting for each state variable

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/09-tracking-moving-object.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/09-tracking-moving-object.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
