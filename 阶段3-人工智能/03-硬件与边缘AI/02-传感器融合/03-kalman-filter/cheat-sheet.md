---
title: 卡尔曼滤波方程速查表
description: 卡尔曼滤波方程速查表
---

# 卡尔曼滤波方程速查表

## 五个核心方程

### 预测（时间更新）

**1. 状态预测**
```
x̂ₖ₊₁|ₖ = F × x̂ₖ|ₖ
```

**2. 协方差预测**
```
Pₖ₊₁|ₖ = F × Pₖ|ₖ × Fᵀ + Q
```

### 更新（量测更新）

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

## 变量

| 符号 | 名称 | 维度 | 说明 |
|--------|------|------------|-------------|
| x̂ | 状态估计 | n×1 | 系统状态的最优猜测 |
| P | 协方差 | n×n | 状态估计的不确定性 |
| F | 状态转移 | n×n | 状态如何演化 |
| H | 量测矩阵 | m×n | 将状态映射到量测 |
| Q | 过程噪声 | n×n | 模型不确定性 |
| R | 量测噪声 | m×m | 传感器不确定性 |
| K | 卡尔曼增益 | n×m | 最优加权 |
| z | 量测 | m×1 | 传感器读数 |
| I | 单位阵 | n×n | 单位矩阵 |

## Python 模板

```python
import numpy as np

class KalmanFilter:
    def __init__(self, F, H, Q, R, x0, P0):
        self.F = F  # State transition
        self.H = H  # Measurement matrix
        self.Q = Q  # Process noise
        self.R = R  # Measurement noise
        self.x = x0  # Initial state
        self.P = P0  # Initial covariance
        self.I = np.eye(F.shape[0])

    def predict(self):
        self.x = self.F @ self.x
        self.P = self.F @ self.P @ self.F.T + self.Q

    def update(self, z):
        y = z - self.H @ self.x
        S = self.H @ self.P @ self.H.T + self.R
        K = self.P @ self.H.T @ np.linalg.inv(S)
        self.x = self.x + K @ y
        self.P = (self.I - K @ self.H) @ self.P
```

## 常见模型

### 匀速（1D）
```python
dt = 0.1
F = np.array([[1, dt],
              [0, 1 ]])
H = np.array([[1, 0]])
Q = q * np.array([[dt**4/4, dt**3/2],
                  [dt**3/2, dt**2  ]])
R = np.array([[σ²]])
```

### 匀速（2D）
```python
F = np.array([[1, 0, dt, 0 ],
              [0, 1, 0,  dt],
              [0, 0, 1,  0 ],
              [0, 0, 0,  1 ]])
H = np.array([[1, 0, 0, 0],
              [0, 1, 0, 0]])
```

### 匀加速（1D）
```python
F = np.array([[1, dt, dt**2/2],
              [0, 1,  dt     ],
              [0, 0,  1      ]])
H = np.array([[1, 0, 0]])
```

## 快速参考

### 何时使用哪种模型

| 场景 | 模型 | 状态 |
|----------|-------|-------|
| 静止目标 | 恒定位置 | [x] |
| 匀速运动 | 匀速 | [x, v] |
| 加速运动 | 匀加速 | [x, v, a] |
| 2D 跟踪 | 2D 匀速 | [x, y, vx, vy] |
| 3D 跟踪 | 3D 匀速 | [x, y, z, vx, vy, vz] |

### 调参准则

**过程噪声（Q）**：
- 过小 → 滤波器忽视量测，适应缓慢
- 过大 → 滤波器输出跳动，估计噪声大
- 起始值：Q = 0.01 × I

**量测噪声（R）**：
- 应与实际传感器噪声匹配
- 从传感器数据手册或实验中测得
- R = σ²，其中 σ 为标准差

**初始协方差（P₀）**：
- 表示初始不确定性
- 取值大 → 更信任最初的量测
- 起始值：P₀ = 10 × I

## 常见错误

❌ **错误**：`P = F @ P @ F + Q`
✅ **正确**：`P = F @ P @ F.T + Q`

❌ **错误**：`K = P @ H.T / S`
✅ **正确**：`K = P @ H.T @ np.linalg.inv(S)`

❌ **错误**：`z = 5.0`（标量）
✅ **正确**：`z = np.array([[5.0]])`（列向量）

## 诊断

### 检查滤波器健康度

```python
# 1. Innovation should be small
innovation = z - H @ x_pred
if np.abs(innovation) > 3 * np.sqrt(S):
    print("Warning: Large innovation!")

# 2. Covariance should be positive definite
eigenvalues = np.linalg.eigvals(P)
if np.any(eigenvalues <= 0):
    print("Warning: P not positive definite!")

# 3. Kalman gain should be 0 < K < 1
if np.any(K < 0) or np.any(K > 1):
    print("Warning: K out of range!")
```

## 扩展卡尔曼滤波（EKF）

适用于非线性系统：

```python
def predict_ekf(x, P, f, F_jacobian, Q):
    """
    f: non-linear state transition function
    F_jacobian: Jacobian of f
    """
    x_pred = f(x)
    F = F_jacobian(x)
    P_pred = F @ P @ F.T + Q
    return x_pred, P_pred

def update_ekf(x_pred, P_pred, z, h, H_jacobian, R):
    """
    h: non-linear measurement function
    H_jacobian: Jacobian of h
    """
    H = H_jacobian(x_pred)
    y = z - h(x_pred)
    S = H @ P_pred @ H.T + R
    K = P_pred @ H.T @ np.linalg.inv(S)
    x = x_pred + K @ y
    P = (I - K @ H) @ P_pred
    return x, P
```

## 常用公式

### 新息协方差
```
S = H × P × Hᵀ + R
```

### 后验协方差（Joseph 形式）
```
P = (I - K×H) × P × (I - K×H)ᵀ + K × R × Kᵀ
```

### 马氏距离
```
d² = yᵀ × S⁻¹ × y
```

### 对数似然
```
log L = -½ × (yᵀ×S⁻¹×y + log|S| + m×log(2π))
```


<details>
<summary>English original</summary>

**Kalman Filter Equations Cheat Sheet**

**The Five Core Equations**

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

**Variables**

| Symbol | Name | Dimensions | Description |
|--------|------|------------|-------------|
| x̂ | State estimate | n×1 | Best guess of system state |
| P | Covariance | n×n | Uncertainty in state estimate |
| F | State transition | n×n | How state evolves |
| H | Measurement matrix | m×n | Maps state to measurements |
| Q | Process noise | n×n | Model uncertainty |
| R | Measurement noise | m×m | Sensor uncertainty |
| K | Kalman gain | n×m | Optimal weighting |
| z | Measurement | m×1 | Sensor reading |
| I | Identity | n×n | Identity matrix |

**Python Template**

```python
import numpy as np

class KalmanFilter:
    def __init__(self, F, H, Q, R, x0, P0):
        self.F = F  # State transition
        self.H = H  # Measurement matrix
        self.Q = Q  # Process noise
        self.R = R  # Measurement noise
        self.x = x0  # Initial state
        self.P = P0  # Initial covariance
        self.I = np.eye(F.shape[0])

    def predict(self):
        self.x = self.F @ self.x
        self.P = self.F @ self.P @ self.F.T + self.Q

    def update(self, z):
        y = z - self.H @ self.x
        S = self.H @ self.P @ self.H.T + self.R
        K = self.P @ self.H.T @ np.linalg.inv(S)
        self.x = self.x + K @ y
        self.P = (self.I - K @ self.H) @ self.P
```

**Common Models**

**Constant Velocity (1D)**
```python
dt = 0.1
F = np.array([[1, dt],
              [0, 1 ]])
H = np.array([[1, 0]])
Q = q * np.array([[dt**4/4, dt**3/2],
                  [dt**3/2, dt**2  ]])
R = np.array([[σ²]])
```

**Constant Velocity (2D)**
```python
F = np.array([[1, 0, dt, 0 ],
              [0, 1, 0,  dt],
              [0, 0, 1,  0 ],
              [0, 0, 0,  1 ]])
H = np.array([[1, 0, 0, 0],
              [0, 1, 0, 0]])
```

**Constant Acceleration (1D)**
```python
F = np.array([[1, dt, dt**2/2],
              [0, 1,  dt     ],
              [0, 0,  1      ]])
H = np.array([[1, 0, 0]])
```

**Quick Reference**

**When to Use What**

| Scenario | Model | State |
|----------|-------|-------|
| Stationary object | Constant position | [x] |
| Moving at steady speed | Constant velocity | [x, v] |
| Accelerating object | Constant acceleration | [x, v, a] |
| 2D tracking | 2D constant velocity | [x, y, vx, vy] |
| 3D tracking | 3D constant velocity | [x, y, z, vx, vy, vz] |

**Tuning Guidelines**

**Process Noise (Q)**:
- Too small → Filter ignores measurements, slow to adapt
- Too large → Filter jumps around, noisy estimates
- Start with: Q = 0.01 × I

**Measurement Noise (R)**:
- Should match actual sensor noise
- Measure from sensor datasheet or experiments
- R = σ² where σ is standard deviation

**Initial Covariance (P₀)**:
- Represents initial uncertainty
- Large values → Trust first measurements more
- Start with: P₀ = 10 × I

**Common Mistakes**

❌ **Wrong**: `P = F @ P @ F + Q`
✅ **Right**: `P = F @ P @ F.T + Q`

❌ **Wrong**: `K = P @ H.T / S`
✅ **Right**: `K = P @ H.T @ np.linalg.inv(S)`

❌ **Wrong**: `z = 5.0` (scalar)
✅ **Right**: `z = np.array([[5.0]])` (column vector)

**Diagnostics**

**Check Filter Health**

```python
# 1. Innovation should be small
innovation = z - H @ x_pred
if np.abs(innovation) > 3 * np.sqrt(S):
    print("Warning: Large innovation!")

# 2. Covariance should be positive definite
eigenvalues = np.linalg.eigvals(P)
if np.any(eigenvalues <= 0):
    print("Warning: P not positive definite!")

# 3. Kalman gain should be 0 < K < 1
if np.any(K < 0) or np.any(K > 1):
    print("Warning: K out of range!")
```

**Extended Kalman Filter (EKF)**

For non-linear systems:

```python
def predict_ekf(x, P, f, F_jacobian, Q):
    """
    f: non-linear state transition function
    F_jacobian: Jacobian of f
    """
    x_pred = f(x)
    F = F_jacobian(x)
    P_pred = F @ P @ F.T + Q
    return x_pred, P_pred

def update_ekf(x_pred, P_pred, z, h, H_jacobian, R):
    """
    h: non-linear measurement function
    H_jacobian: Jacobian of h
    """
    H = H_jacobian(x_pred)
    y = z - h(x_pred)
    S = H @ P_pred @ H.T + R
    K = P_pred @ H.T @ np.linalg.inv(S)
    x = x_pred + K @ y
    P = (I - K @ H) @ P_pred
    return x, P
```

**Useful Formulas**

**Innovation Covariance**
```
S = H × P × Hᵀ + R
```

**Posterior Covariance (Joseph Form)**
```
P = (I - K×H) × P × (I - K×H)ᵀ + K × R × Kᵀ
```

**Mahalanobis Distance**
```
d² = yᵀ × S⁻¹ × y
```

**Log Likelihood**
```
log L = -½ × (yᵀ×S⁻¹×y + log|S| + m×log(2π))
```

</details>

## 矩阵维度检查

对于 n 个状态和 m 个测量：

```
x:  n × 1
P:  n × n
F:  n × n
Q:  n × n
H:  m × n
R:  m × m
K:  n × m
z:  m × 1
y:  m × 1  (innovation)
S:  m × m  (innovation covariance)
```

## 性能技巧

1. **使用 Joseph 形式**进行协方差更新（更稳定）
2. **强制对称性**：`P = (P + P.T) / 2`
3. **定期检查正定性**
4. **尽可能使用 Cholesky 分解**代替矩阵求逆
5. **归一化四元数**（若需跟踪姿态）

## 延伸阅读

- 第 8 章：1D 卡尔曼滤波实现
- 第 10 章：矩阵形式细节
- 第 14 章：扩展卡尔曼滤波
- 第 16 章：误差状态卡尔曼滤波
- 第 20 章：实现技巧


<details>
<summary>English original</summary>

**Matrix Dimensions Check**

For n states and m measurements:

```
x:  n × 1
P:  n × n
F:  n × n
Q:  n × n
H:  m × n
R:  m × m
K:  n × m
z:  m × 1
y:  m × 1  (innovation)
S:  m × m  (innovation covariance)
```

**Performance Tips**

1. **Use Joseph form** for covariance update (more stable)
2. **Enforce symmetry**: `P = (P + P.T) / 2`
3. **Check positive definiteness** regularly
4. **Use Cholesky decomposition** instead of matrix inverse when possible
5. **Normalize quaternions** if tracking orientation

**Further Reading**

- Chapter 8: 1D Kalman Filter Implementation
- Chapter 10: Matrix Form Details
- Chapter 14: Extended Kalman Filter
- Chapter 16: Error State Kalman Filter
- Chapter 20: Implementation Tricks

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/cheat-sheet.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/cheat-sheet.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
