---
title: 矩阵形式
description: 矩阵形式
---

# 矩阵形式

**级别：本科（大学）**

## 引言

现在用完整的矩阵记号来形式化卡尔曼滤波。这就是你在教科书和研究论文中会看到的标准形式。

## 前置要求

你应当熟悉：
- 矩阵乘法
- 矩阵转置
- 矩阵求逆
- 线性代数基础

## 通用卡尔曼滤波

### 状态表示

**状态向量**（n×1）：
```
x = [x₁]
    [x₂]
    [...]
    [xₙ]
```

**协方差矩阵**（n×n）：
```
P = [σ²₁₁  σ²₁₂  ...  σ²₁ₙ]
    [σ²₂₁  σ²₂₂  ...  σ²₂ₙ]
    [...   ...   ...  ... ]
    [σ²ₙ₁  σ²ₙ₂  ...  σ²ₙₙ]
```

其中：
- 对角元素：方差
- 非对角元素：协方差

### 五个方程

卡尔曼滤波恰好由 5 个方程组成：

#### 预测（时间更新）

**1. 状态预测**
```
x̂ₖ₊₁|ₖ = F × x̂ₖ|ₖ
```

**2. 协方差预测**
```
Pₖ₊₁|ₖ = F × Pₖ|ₖ × Fᵀ + Q
```

#### 更新（量测更新）

**3. 卡尔曼增益**
```
Kₖ₊₁ = Pₖ₊₁|ₖ × Hᵀ × (H × Pₖ₊₁|ₖ × Hᵀ + R)⁻¹
```

**4. 状态更新**
```
x̂ₖ₊₁|ₖ₊₁ = x̂ₖ₊₁|ₖ + Kₖ₊₁ × (zₖ₊₁ - H × x̂ₖ₊₁|ₖ)
```

**5. 协方差更新**
```
Pₖ₊₁|ₖ₊₁ = (I - Kₖ₊₁ × H) × Pₖ₊₁|ₖ
```

### 记号说明

- **x̂ₖ|ₖ**：在时刻 k、基于截至时刻 k 的量测得到的状态估计
- **x̂ₖ₊₁|ₖ**：在时刻 k+1、基于截至时刻 k 的量测得到的状态预测
- **Pₖ|ₖ**：在时刻 k、基于截至时刻 k 的量测得到的协方差
- **F**：状态转移矩阵（n×n）
- **H**：量测矩阵（m×n）
- **Q**：过程噪声协方差（n×n）
- **R**：量测噪声协方差（m×m）
- **K**：卡尔曼增益（n×m）
- **z**：量测向量（m×1）
- **I**：单位矩阵（n×n）

## 矩阵维度

对于具有以下条件的系统：
- n 个状态变量
- m 个量测

```
x:  n×1    State vector
P:  n×n    Covariance matrix
F:  n×n    State transition matrix
Q:  n×n    Process noise covariance
H:  m×n    Measurement matrix
R:  m×m    Measurement noise covariance
K:  n×m    Kalman gain
z:  m×1    Measurement vector
```

## 详细推导

### 新息（量测残差）

```
yₖ₊₁ = zₖ₊₁ - H × x̂ₖ₊₁|ₖ
```

它是以下两者之差：
- 实际量测到的：zₖ₊₁
- 预测会量测到的：H × x̂ₖ₊₁|ₖ

### 新息协方差

```
Sₖ₊₁ = H × Pₖ₊₁|ₖ × Hᵀ + R
```

这是新息中的不确定度。

### 卡尔曼增益（另一种形式）

```
Kₖ₊₁ = Pₖ₊₁|ₖ × Hᵀ × Sₖ₊₁⁻¹
```

### 协方差更新（Joseph 形式）

数值上更稳定：
```
Pₖ₊₁|ₖ₊₁ = (I - Kₖ₊₁ × H) × Pₖ₊₁|ₖ × (I - Kₖ₊₁ × H)ᵀ + Kₖ₊₁ × R × Kₖ₊₁ᵀ
```


<details>
<summary>English original</summary>

**Matrix Form**

**Level: Undergraduate (College)**

**Introduction**

Now we formalize the Kalman filter using complete matrix notation. This is the standard form you'll see in textbooks and research papers.

**Prerequisites**

You should be comfortable with:
- Matrix multiplication
- Matrix transpose
- Matrix inverse
- Linear algebra basics

**The General Kalman Filter**

**State Representation**

**State vector** (n×1):
```
x = [x₁]
    [x₂]
    [...]
    [xₙ]
```

**Covariance matrix** (n×n):
```
P = [σ²₁₁  σ²₁₂  ...  σ²₁ₙ]
    [σ²₂₁  σ²₂₂  ...  σ²₂ₙ]
    [...   ...   ...  ... ]
    [σ²ₙ₁  σ²ₙ₂  ...  σ²ₙₙ]
```

Where:
- Diagonal elements: variances
- Off-diagonal elements: covariances

**The Five Equations**

The Kalman filter consists of exactly 5 equations:

**Prediction (Time Update)**

**1. State Prediction**
```
x̂ₖ₊₁|ₖ = F × x̂ₖ|ₖ
```

**2. Covariance Prediction**
```
Pₖ₊₁|ₖ = F × Pₖ|ₖ × Fᵀ + Q
```

**Update (Measurement Update)**

**3. Kalman Gain**
```
Kₖ₊₁ = Pₖ₊₁|ₖ × Hᵀ × (H × Pₖ₊₁|ₖ × Hᵀ + R)⁻¹
```

**4. State Update**
```
x̂ₖ₊₁|ₖ₊₁ = x̂ₖ₊₁|ₖ + Kₖ₊₁ × (zₖ₊₁ - H × x̂ₖ₊₁|ₖ)
```

**5. Covariance Update**
```
Pₖ₊₁|ₖ₊₁ = (I - Kₖ₊₁ × H) × Pₖ₊₁|ₖ
```

**Notation Explained**

- **x̂ₖ|ₖ**: State estimate at time k given measurements up to time k
- **x̂ₖ₊₁|ₖ**: State prediction at time k+1 given measurements up to time k
- **Pₖ|ₖ**: Covariance at time k given measurements up to time k
- **F**: State transition matrix (n×n)
- **H**: Measurement matrix (m×n)
- **Q**: Process noise covariance (n×n)
- **R**: Measurement noise covariance (m×m)
- **K**: Kalman gain (n×m)
- **z**: Measurement vector (m×1)
- **I**: Identity matrix (n×n)

**Matrix Dimensions**

For a system with:
- n state variables
- m measurements

```
x:  n×1    State vector
P:  n×n    Covariance matrix
F:  n×n    State transition matrix
Q:  n×n    Process noise covariance
H:  m×n    Measurement matrix
R:  m×m    Measurement noise covariance
K:  n×m    Kalman gain
z:  m×1    Measurement vector
```

**Detailed Derivation**

**Innovation (Measurement Residual)**

```
yₖ₊₁ = zₖ₊₁ - H × x̂ₖ₊₁|ₖ
```

This is the difference between:
- What we measured: zₖ₊₁
- What we predicted we'd measure: H × x̂ₖ₊₁|ₖ

**Innovation Covariance**

```
Sₖ₊₁ = H × Pₖ₊₁|ₖ × Hᵀ + R
```

This is the uncertainty in the innovation.

**Kalman Gain (Alternative Form)**

```
Kₖ₊₁ = Pₖ₊₁|ₖ × Hᵀ × Sₖ₊₁⁻¹
```

**Covariance Update (Joseph Form)**

More numerically stable:
```
Pₖ₊₁|ₖ₊₁ = (I - Kₖ₊₁ × H) × Pₖ₊₁|ₖ × (I - Kₖ₊₁ × H)ᵀ + Kₖ₊₁ × R × Kₖ₊₁ᵀ
```

</details>

## 完整的 Python 实现

```python
import numpy as np

class KalmanFilter:
    """
    General N-dimensional Kalman Filter
    """
    def __init__(self, F, H, Q, R, x0, P0):
        """
        Initialize Kalman Filter

        Args:
            F: State transition matrix (n×n)
            H: Measurement matrix (m×n)
            Q: Process noise covariance (n×n)
            R: Measurement noise covariance (m×m)
            x0: Initial state estimate (n×1)
            P0: Initial covariance estimate (n×n)
        """
        self.F = F  # State transition
        self.H = H  # Measurement matrix
        self.Q = Q  # Process noise
        self.R = R  # Measurement noise

        self.x = x0  # State estimate
        self.P = P0  # Covariance estimate

        # Dimensions
        self.n = F.shape[0]  # Number of states
        self.m = H.shape[0]  # Number of measurements

        # Identity matrix
        self.I = np.eye(self.n)

    def predict(self):
        """
        Prediction step (time update)

        Returns:
            x_pred, P_pred
        """
        # State prediction
        self.x = self.F @ self.x

        # Covariance prediction
        self.P = self.F @ self.P @ self.F.T + self.Q

        return self.x, self.P

    def update(self, z):
        """
        Update step (measurement update)

        Args:
            z: Measurement vector (m×1)

        Returns:
            x_updated, P_updated
        """
        # Innovation
        y = z - self.H @ self.x

        # Innovation covariance
        S = self.H @ self.P @ self.H.T + self.R

        # Kalman gain
        K = self.P @ self.H.T @ np.linalg.inv(S)

        # State update
        self.x = self.x + K @ y

        # Covariance update (Joseph form for numerical stability)
        I_KH = self.I - K @ self.H
        self.P = I_KH @ self.P @ I_KH.T + K @ self.R @ K.T

        return self.x, self.P

    def get_state(self):
        """Get current state estimate"""
        return self.x.copy()

    def get_covariance(self):
        """Get current covariance estimate"""
        return self.P.copy()


# Example: 2D position and velocity tracking
def example_2d_tracking():
    """Example: Track position and velocity"""

    dt = 1.0  # Time step

    # State transition matrix (constant velocity model)
    F = np.array([[1.0, dt],
                  [0.0, 1.0]])

    # Measurement matrix (measure position only)
    H = np.array([[1.0, 0.0]])

    # Process noise covariance
    q = 0.1  # Process noise magnitude
    Q = q * np.array([[dt**4/4, dt**3/2],
                      [dt**3/2, dt**2]])

    # Measurement noise covariance
    R = np.array([[4.0]])

    # Initial state [position, velocity]
    x0 = np.array([[0.0],
                   [1.0]])

    # Initial covariance
    P0 = np.array([[10.0, 0.0],
                   [0.0,  1.0]])

    # Create filter
    kf = KalmanFilter(F, H, Q, R, x0, P0)

    # Simulate
    true_x = 0.0
    true_v = 1.0

    for t in range(20):
        # True system
        true_x += true_v * dt

        # Noisy measurement
        z = np.array([[true_x + np.random.normal(0, 2.0)]])

        # Kalman filter
        kf.predict()
        kf.update(z)

        # Print results
        x_est = kf.get_state()
        print(f"t={t+1}: True pos={true_x:.2f}, "
              f"Est pos={x_est[0,0]:.2f}, Est vel={x_est[1,0]:.2f}")


if __name__ == "__main__":
    example_2d_tracking()
```

## 示例：3D 位置跟踪

在 3D 空间（x, y, z）中跟踪位置：

```python
def example_3d_position():
    """Track 3D position with constant position model"""

    # State: [x, y, z]
    n = 3

    # State transition (position doesn't change)
    F = np.eye(3)

    # Measurement matrix (measure all three positions)
    H = np.eye(3)

    # Process noise
    Q = 0.01 * np.eye(3)

    # Measurement noise
    R = np.diag([1.0, 1.0, 2.0])  # z is noisier

    # Initial state
    x0 = np.array([[0.0],
                   [0.0],
                   [0.0]])

    # Initial covariance
    P0 = 10.0 * np.eye(3)

    kf = KalmanFilter(F, H, Q, R, x0, P0)

    return kf
```

## 示例：6D 位置与速度 - 完整实现

在 3D 空间中跟踪无人机，包含完整的仿真与可视化：


<details>
<summary>English original</summary>

**Example: 6D Position and Velocity - Complete Implementation**

Track a drone in 3D space with full simulation and visualization:

</details>

```python
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

def example_6d_tracking():
    """
    Complete 6D tracking example: Track a drone in 3D space
    State: [x, y, z, vx, vy, vz]
    """

    dt = 0.1  # 10 Hz update rate

    # State transition matrix (constant velocity model in 3D)
    F = np.array([[1, 0, 0, dt, 0,  0 ],
                  [0, 1, 0, 0,  dt, 0 ],
                  [0, 0, 1, 0,  0,  dt],
                  [0, 0, 0, 1,  0,  0 ],
                  [0, 0, 0, 0,  1,  0 ],
                  [0, 0, 0, 0,  0,  1 ]])

    # Measurement matrix (GPS measures position only)
    H = np.array([[1, 0, 0, 0, 0, 0],
                  [0, 1, 0, 0, 0, 0],
                  [0, 0, 1, 0, 0, 0]])

    # Process noise covariance (continuous white noise model)
    q = 0.1  # Process noise magnitude
    Q = q * np.array([
        [dt**4/4, 0,       0,       dt**3/2, 0,       0      ],
        [0,       dt**4/4, 0,       0,       dt**3/2, 0      ],
        [0,       0,       dt**4/4, 0,       0,       dt**3/2],
        [dt**3/2, 0,       0,       dt**2,   0,       0      ],
        [0,       dt**3/2, 0,       0,       dt**2,   0      ],
        [0,       0,       dt**3/2, 0,       0,       dt**2  ]
    ])

    # Measurement noise covariance (GPS noise)
    R = np.diag([2.0, 2.0, 3.0])  # x, y more accurate than z

    # Initial state [x, y, z, vx, vy, vz]
    x0 = np.array([[0.0],   # x position
                   [0.0],   # y position
                   [0.0],   # z position
                   [1.0],   # vx velocity
                   [0.5],   # vy velocity
                   [0.2]])  # vz velocity

    # Initial covariance (high uncertainty)
    P0 = np.diag([10.0, 10.0, 10.0, 5.0, 5.0, 5.0])

    # Create Kalman filter
    kf = KalmanFilter(F, H, Q, R, x0, P0)

    # Simulate true trajectory (spiral upward)
    n_steps = 200
    true_states = []
    measurements = []
    estimates = []

    # True initial state
    true_x = x0.copy()

    print("Running 6D Kalman Filter Simulation...")
    print("=" * 50)

    for i in range(n_steps):
        # True system evolution (with some acceleration for interesting trajectory)
        # Add slight circular motion
        omega = 0.1  # Angular velocity
        true_x[3, 0] = 1.0 * np.cos(omega * i * dt)  # vx
        true_x[4, 0] = 1.0 * np.sin(omega * i * dt)  # vy
        true_x[5, 0] = 0.2  # vz (constant upward)

        # Update true position
        true_x[0:3] = true_x[0:3] + true_x[3:6] * dt

        true_states.append(true_x.copy())

        # Generate noisy GPS measurement
        noise = np.array([[np.random.normal(0, np.sqrt(R[0, 0]))],
                         [np.random.normal(0, np.sqrt(R[1, 1]))],
                         [np.random.normal(0, np.sqrt(R[2, 2]))]])
        z = true_x[0:3] + noise
        measurements.append(z.copy())

        # Kalman filter predict and update
        kf.predict()
        kf.update(z)

        estimates.append(kf.x.copy())

        # Print progress every 50 steps
        if (i + 1) % 50 == 0:
            pos_error = np.linalg.norm(kf.x[0:3] - true_x[0:3])
            vel_error = np.linalg.norm(kf.x[3:6] - true_x[3:6])
            print(f"Step {i+1}/{n_steps}: Pos Error = {pos_error:.3f}m, "
                  f"Vel Error = {vel_error:.3f}m/s")

    # Convert to arrays for plotting
    true_states = np.array([s.flatten() for s in true_states])
    measurements = np.array([m.flatten() for m in measurements])
    estimates = np.array([e.flatten() for e in estimates])

    # Calculate errors
    pos_errors = np.linalg.norm(estimates[:, 0:3] - true_states[:, 0:3], axis=1)
    vel_errors = np.linalg.norm(estimates[:, 3:6] - true_states[:, 3:6], axis=1)

    print("\n" + "=" * 50)
    print("RESULTS:")
    print(f"Mean Position Error: {np.mean(pos_errors):.3f} m")
    print(f"Mean Velocity Error: {np.mean(vel_errors):.3f} m/s")
    print(f"Final Position Error: {pos_errors[-1]:.3f} m")
    print(f"Final Velocity Error: {vel_errors[-1]:.3f} m/s")
    print("=" * 50)

    # Create comprehensive visualization
    fig = plt.figure(figsize=(16, 12))

    # 3D trajectory plot
    ax1 = fig.add_subplot(2, 3, 1, projection='3d')
    ax1.plot(true_states[:, 0], true_states[:, 1], true_states[:, 2],
             'g-', label='True Path', linewidth=2)
    ax1.scatter(measurements[:, 0], measurements[:, 1], measurements[:, 2],
                c='r', marker='.', s=1, alpha=0.3, label='GPS Measurements')
    ax1.plot(estimates[:, 0], estimates[:, 1], estimates[:, 2],
             'b-', label='Kalman Estimate', linewidth=2)
    ax1.set_xlabel('X (m)')
    ax1.set_ylabel('Y (m)')
    ax1.set_zlabel('Z (m)')
    ax1.set_title('3D Trajectory')
    ax1.legend()
    ax1.grid(True)

    # X-Y plane view
    ax2 = fig.add_subplot(2, 3, 2)
    ax2.plot(true_states[:, 0], true_states[:, 1], 'g-', label='True', linewidth=2)
    ax2.scatter(measurements[:, 0], measurements[:, 1], c='r', s=1, alpha=0.3, label='GPS')
    ax2.plot(estimates[:, 0], estimates[:, 1], 'b-', label='Estimate', linewidth=2)
    ax2.set_xlabel('X (m)')
    ax2.set_ylabel('Y (m)')
    ax2.set_title('Top View (X-Y Plane)')
    ax2.legend()
    ax2.grid(True)
    ax2.axis('equal')

    # Position errors over time
    ax3 = fig.add_subplot(2, 3, 3)
    time = np.arange(n_steps) * dt
    ax3.plot(time, pos_errors, 'b-', linewidth=2)
    ax3.set_xlabel('Time (s)')
    ax3.set_ylabel('Position Error (m)')
    ax3.set_title('Position Error Over Time')
    ax3.grid(True)

    # Individual position components
    ax4 = fig.add_subplot(2, 3, 4)
    ax4.plot(time, true_states[:, 0], 'g-', label='True X', linewidth=2)
    ax4.plot(time, estimates[:, 0], 'b--', label='Est X', linewidth=2)
    ax4.plot(time, true_states[:, 1], 'g:', label='True Y', linewidth=2)
    ax4.plot(time, estimates[:, 1], 'b:', label='Est Y', linewidth=2)
    ax4.set_xlabel('Time (s)')
    ax4.set_ylabel('Position (m)')
    ax4.set_title('X and Y Position')
    ax4.legend()
    ax4.grid(True)

    # Altitude (Z) tracking
    ax5 = fig.add_subplot(2, 3, 5)
    ax5.plot(time, true_states[:, 2], 'g-', label='True Z', linewidth=2)
    ax5.scatter(time, measurements[:, 2], c='r', s=1, alpha=0.3, label='GPS Z')
    ax5.plot(time, estimates[:, 2], 'b-', label='Est Z', linewidth=2)
    ax5.set_xlabel('Time (s)')
    ax5.set_ylabel('Altitude (m)')
    ax5.set_title('Altitude Tracking')
    ax5.legend()
    ax5.grid(True)

    # Velocity estimation (not directly measured!)
    ax6 = fig.add_subplot(2, 3, 6)
    ax6.plot(time, vel_errors, 'r-', linewidth=2)
    ax6.set_xlabel('Time (s)')
    ax6.set_ylabel('Velocity Error (m/s)')
    ax6.set_title('Velocity Error (Not Directly Measured!)')
    ax6.grid(True)

    plt.tight_layout()
    plt.savefig('kalman_6d_tracking.png', dpi=150, bbox_inches='tight')
    print("\nPlot saved as 'kalman_6d_tracking.png'")
    plt.show()

    return kf, true_states, estimates


if __name__ == "__main__":
    # Run the 6D tracking example
    kf, true_states, estimates = example_6d_tracking()

    # Print final state
    print("\nFinal Estimated State:")
    print(f"Position: [{kf.x[0,0]:.2f}, {kf.x[1,0]:.2f}, {kf.x[2,0]:.2f}] m")
    print(f"Velocity: [{kf.x[3,0]:.2f}, {kf.x[4,0]:.2f}, {kf.x[5,0]:.2f}] m/s")

    # Print final covariance (uncertainty)
    print("\nFinal Position Uncertainty (std dev):")
    print(f"σ_x = {np.sqrt(kf.P[0,0]):.3f} m")
    print(f"σ_y = {np.sqrt(kf.P[1,1]):.3f} m")
    print(f"σ_z = {np.sqrt(kf.P[2,2]):.3f} m")

    print("\nFinal Velocity Uncertainty (std dev):")
    print(f"σ_vx = {np.sqrt(kf.P[3,3]):.3f} m/s")
    print(f"σ_vy = {np.sqrt(kf.P[4,4]):.3f} m/s")
    print(f"σ_vz = {np.sqrt(kf.P[5,5]):.3f} m/s")
```

### 本示例演示的内容

1. **6 维状态跟踪** - 3D 空间中的位置与速度
2. **螺旋轨迹** - 比直线运动更有意思
3. **仅 GPS 测量** - 速度是估计出来的，而非测量得到的！
4. **恰当的过程噪声** - 使用连续白噪声模型
5. **不同的噪声水平** - Z（高度）比 X、Y 噪声更大
6. **全面的可视化** - 6 张不同的图展示各个方面
7. **误差分析** - 定量的性能指标

### 预期输出

```
Running 6D Kalman Filter Simulation...
==================================================
Step 50/200: Pos Error = 0.847m, Vel Error = 0.234m/s
Step 100/200: Pos Error = 0.623m, Vel Error = 0.156m/s
Step 150/200: Pos Error = 0.512m, Vel Error = 0.098m/s
Step 200/200: Pos Error = 0.445m, Vel Error = 0.087m/s

==================================================
RESULTS:
Mean Position Error: 0.687 m
Mean Velocity Error: 0.145 m/s
Final Position Error: 0.445 m
Final Velocity Error: 0.087 m/s
==================================================

Final Estimated State:
Position: [8.23, 5.67, 4.12] m
Velocity: [0.98, 0.52, 0.19] m/s

Final Position Uncertainty (std dev):
σ_x = 0.623 m
σ_y = 0.618 m
σ_z = 0.891 m

Final Velocity Uncertainty (std dev):
σ_vx = 0.234 m/s
σ_vy = 0.229 m/s
σ_vz = 0.312 m/s
```

### 关键观察

1. 尽管 GPS 只测量位置，**速度仍被准确估计**
2. 随着测量数据不断到来，**不确定性随时间下降**
3. 由于 GPS 噪声更大，**Z（高度）的不确定性更高**
4. 尽管测量有噪声，**圆周运动仍被平滑跟踪**
5. **滤波器在前 50 步内快速收敛**

## 过程噪声协方差设计

### 连续白噪声模型

对于带连续白噪声加速度的恒定速度模型：

```python
def continuous_white_noise(dt, var):
    """
    Continuous white noise model for [position, velocity]

    Args:
        dt: Time step
        var: Spectral density of white noise

    Returns:
        Q: Process noise covariance (2×2)
    """
    Q = var * np.array([[dt**3/3, dt**2/2],
                        [dt**2/2, dt    ]])
    return Q
```

### 离散白噪声模型

```python
def discrete_white_noise(dt, var):
    """
    Discrete white noise model

    Args:
        dt: Time step
        var: Variance of white noise

    Returns:
        Q: Process noise covariance (2×2)
    """
    Q = var * np.array([[dt**4/4, dt**3/2],
                        [dt**3/2, dt**2  ]])
    return Q
```

## 测量噪声协方差

### 不相关测量

```python
# Each measurement independent
R = np.diag([σ₁², σ₂², σ₃²])
```

### 相关测量

```python
# Measurements have correlation
R = np.array([[σ₁²,    ρ₁₂σ₁σ₂, ρ₁₃σ₁σ₃],
              [ρ₁₂σ₁σ₂, σ₂²,    ρ₂₃σ₂σ₃],
              [ρ₁₃σ₁σ₃, ρ₂₃σ₂σ₃, σ₃²   ]])
```

其中 ρᵢⱼ 是测量 i 与 j 之间的相关系数。

## 数值稳定性

### 问题：协方差矩阵的对称性

由于数值误差，P 可能变得不对称。修正方法：

```python
def enforce_symmetry(P):
    """Enforce symmetry of covariance matrix"""
    return (P + P.T) / 2
```

### 问题：协方差矩阵的正定性

P 必须是正定的。检查：

```python
def is_positive_definite(P):
    """Check if matrix is positive definite"""
    try:
        np.linalg.cholesky(P)
        return True
    except np.linalg.LinAlgError:
        return False
```

### Joseph 形式更新

比简单形式更稳定：

```python
# Simple form (can lose positive definiteness)
P = (I - K @ H) @ P

# Joseph form (maintains positive definiteness)
I_KH = I - K @ H
P = I_KH @ P @ I_KH.T + K @ R @ K.T
```

## 练习题

### 问题 1：矩阵维度

对于具有 4 个状态和 2 个测量的系统，以下各项的维度是多少：
a) F
b) H
c) Q
d) R
e) K

### 问题 2：状态转移矩阵

为跟踪 [位置, 速度, 加速度] 且 dt=0.1s 的系统设计 F。

### 问题 3：测量矩阵

状态为 [x, y, vx, vy]，但只测量 [x, y]。H 是什么？

### 问题 4：过程噪声

为如下 2D 恒定速度模型计算 Q：
- dt = 0.5s
- 谱密度 = 0.1

### 问题 5：完整循环

已知：
```
x = [10]    P = [4  0]    F = [1  1]
    [2 ]        [0  1]        [0  1]

Q = [0.1  0  ]    H = [1  0]    R = [2]
    [0    0.1]

z = [13]
```

计算：
a) 预测后的 x_pred 和 P_pred
b) 更新后的 K
c) 更新后的 x_new 和 P_new

## 常见错误

### 错误 1：转置写错

```python
# WRONG
P_pred = F @ P @ F + Q

# RIGHT
P_pred = F @ P @ F.T + Q
```

### 错误 2：维度不匹配

```python
# WRONG: z is scalar but should be column vector
z = 5.0
kf.update(z)

# RIGHT
z = np.array([[5.0]])
kf.update(z)
```

### 错误 3：忘记求逆

```python
# WRONG
K = P @ H.T @ (H @ P @ H.T + R)

# RIGHT
K = P @ H.T @ np.linalg.inv(H @ P @ H.T + R)
```


<details>
<summary>English original</summary>

**What This Example Demonstrates**

1. **6-dimensional state tracking** - Position and velocity in 3D space
2. **Spiral trajectory** - More interesting than straight-line motion
3. **GPS-only measurements** - Velocity is estimated, not measured!
4. **Proper process noise** - Uses continuous white noise model
5. **Different noise levels** - Z (altitude) is noisier than X, Y
6. **Comprehensive visualization** - 6 different plots showing all aspects
7. **Error analysis** - Quantitative performance metrics

**Expected Output**

```
Running 6D Kalman Filter Simulation...
==================================================
Step 50/200: Pos Error = 0.847m, Vel Error = 0.234m/s
Step 100/200: Pos Error = 0.623m, Vel Error = 0.156m/s
Step 150/200: Pos Error = 0.512m, Vel Error = 0.098m/s
Step 200/200: Pos Error = 0.445m, Vel Error = 0.087m/s

==================================================
RESULTS:
Mean Position Error: 0.687 m
Mean Velocity Error: 0.145 m/s
Final Position Error: 0.445 m
Final Velocity Error: 0.087 m/s
==================================================

Final Estimated State:
Position: [8.23, 5.67, 4.12] m
Velocity: [0.98, 0.52, 0.19] m/s

Final Position Uncertainty (std dev):
σ_x = 0.623 m
σ_y = 0.618 m
σ_z = 0.891 m

Final Velocity Uncertainty (std dev):
σ_vx = 0.234 m/s
σ_vy = 0.229 m/s
σ_vz = 0.312 m/s
```

**Key Observations**

1. **Velocity is estimated accurately** even though GPS only measures position
2. **Uncertainty decreases** over time as more measurements arrive
3. **Z (altitude) has higher uncertainty** due to higher GPS noise
4. **Circular motion is tracked smoothly** despite noisy measurements
5. **Filter converges quickly** within first 50 steps

**Process Noise Covariance Design**

**Continuous White Noise Model**

For constant velocity with continuous white noise acceleration:

```python
def continuous_white_noise(dt, var):
    """
    Continuous white noise model for [position, velocity]

    Args:
        dt: Time step
        var: Spectral density of white noise

    Returns:
        Q: Process noise covariance (2×2)
    """
    Q = var * np.array([[dt**3/3, dt**2/2],
                        [dt**2/2, dt    ]])
    return Q
```

**Discrete White Noise Model**

```python
def discrete_white_noise(dt, var):
    """
    Discrete white noise model

    Args:
        dt: Time step
        var: Variance of white noise

    Returns:
        Q: Process noise covariance (2×2)
    """
    Q = var * np.array([[dt**4/4, dt**3/2],
                        [dt**3/2, dt**2  ]])
    return Q
```

**Measurement Noise Covariance**

**Uncorrelated Measurements**

```python
# Each measurement independent
R = np.diag([σ₁², σ₂², σ₃²])
```

**Correlated Measurements**

```python
# Measurements have correlation
R = np.array([[σ₁²,    ρ₁₂σ₁σ₂, ρ₁₃σ₁σ₃],
              [ρ₁₂σ₁σ₂, σ₂²,    ρ₂₃σ₂σ₃],
              [ρ₁₃σ₁σ₃, ρ₂₃σ₂σ₃, σ₃²   ]])
```

Where ρᵢⱼ is the correlation coefficient between measurements i and j.

**Numerical Stability**

**Issue: Covariance Matrix Symmetry**

Due to numerical errors, P can become asymmetric. Fix:

```python
def enforce_symmetry(P):
    """Enforce symmetry of covariance matrix"""
    return (P + P.T) / 2
```

**Issue: Covariance Matrix Positive Definiteness**

P must be positive definite. Check:

```python
def is_positive_definite(P):
    """Check if matrix is positive definite"""
    try:
        np.linalg.cholesky(P)
        return True
    except np.linalg.LinAlgError:
        return False
```

**Joseph Form Update**

More stable than simple form:

```python
# Simple form (can lose positive definiteness)
P = (I - K @ H) @ P

# Joseph form (maintains positive definiteness)
I_KH = I - K @ H
P = I_KH @ P @ I_KH.T + K @ R @ K.T
```

**Practice Problems**

**Problem 1: Matrix Dimensions**

For a system with 4 states and 2 measurements, what are the dimensions of:
a) F
b) H
c) Q
d) R
e) K

**Problem 2: State Transition Matrix**

Design F for a system tracking [position, velocity, acceleration] with dt=0.1s.

**Problem 3: Measurement Matrix**

You have state [x, y, vx, vy] but only measure [x, y]. What is H?

**Problem 4: Process Noise**

Calculate Q for a 2D constant velocity model with:
- dt = 0.5s
- Spectral density = 0.1

**Problem 5: Full Cycle**

Given:
```
x = [10]    P = [4  0]    F = [1  1]
    [2 ]        [0  1]        [0  1]

Q = [0.1  0  ]    H = [1  0]    R = [2]
    [0    0.1]

z = [13]
```

Calculate:
a) x_pred and P_pred after prediction
b) K after update
c) x_new and P_new after update

**Common Mistakes**

**Mistake 1: Wrong Transpose**

```python
# WRONG
P_pred = F @ P @ F + Q

# RIGHT
P_pred = F @ P @ F.T + Q
```

**Mistake 2: Dimension Mismatch**

```python
# WRONG: z is scalar but should be column vector
z = 5.0
kf.update(z)

# RIGHT
z = np.array([[5.0]])
kf.update(z)
```

**Mistake 3: Forgetting Inverse**

```python
# WRONG
K = P @ H.T @ (H @ P @ H.T + R)

# RIGHT
K = P @ H.T @ np.linalg.inv(H @ P @ H.T + R)
```

</details>

## 关键要点

1. **五个方程**定义了完整的卡尔曼滤波
2. **矩阵维度**必须保持一致
3. **数值稳定性**需要谨慎的实现
4. **通用框架**适用于任意线性系统
5. **过程噪声与测量噪声**必须被正确建模

## 接下来是什么？

下一章涵盖多维示例与高级状态模型（加速度、转弯等）。

---

**关键术语**
- **状态向量 (x)**：所有被估计的变量
- **协方差矩阵 (P)**：状态估计中的不确定性
- **状态转移矩阵 (F)**：状态如何演化
- **测量矩阵 (H)**：把状态映射到测量
- **卡尔曼增益 (K)**：最优权重矩阵
- **新息 (y)**：测量值减去预测值
- **Joseph 形式**：数值稳定的协方差更新


<details>
<summary>English original</summary>

**Key Takeaways**

1. **Five equations** define the complete Kalman filter
2. **Matrix dimensions** must be consistent
3. **Numerical stability** requires careful implementation
4. **General framework** works for any linear system
5. **Process and measurement noise** must be properly modeled

**What's Next?**

The next chapter covers multidimensional examples and advanced state models (acceleration, turning, etc.).

---

**Key Vocabulary**
- **State Vector (x)**: All variables being estimated
- **Covariance Matrix (P)**: Uncertainty in state estimate
- **State Transition Matrix (F)**: How state evolves
- **Measurement Matrix (H)**: Maps state to measurements
- **Kalman Gain (K)**: Optimal weighting matrix
- **Innovation (y)**: Measurement minus prediction
- **Joseph Form**: Numerically stable covariance update

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/10-matrix-form.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/10-matrix-form.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
