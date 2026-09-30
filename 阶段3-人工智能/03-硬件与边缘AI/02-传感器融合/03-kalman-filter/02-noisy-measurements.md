---
title: 带噪声的测量
description: 带噪声的测量
---

# 带噪声的测量

**级别：初级（8-12 岁）**

## 为什么不能直接精确测量？

设想用一把尺子量自己的身高。很简单，对吧？但等等……

- 你站得完全笔直吗？
- 尺子完全笔直吗？
- 你是从完全正确的角度读数吗？
- 你是从头顶的确切位置开始量的吗？

每次测量都有微小误差。我们把这些误差称为**噪声**。

## 什么是噪声？

**噪声**就像收音机的静电声或老电视的雪花点。它是使测量不那么完美的随机误差。

### 示例 1：浴室秤

连续 5 次站上浴室秤：
- 第一次：75.2 lbs
- 第二次：75.4 lbs
- 第三次：75.1 lbs
- 第四次：75.3 lbs
- 第五次：75.2 lbs

两次测量之间你并没有增重或减重！只是秤不完美。这就是**测量噪声**。

### 示例 2：掷飞镖

设想你非常擅长掷飞镖，总是瞄准靶心。但你掷出的飞镖落点：
- 偏左一点
- 偏右一点
- 偏高一点
- 偏低一点
- 正中目标！

你瞄准的是同一个点，但飞镖落点存在随机性。这就是噪声！

### 示例 3：温度传感器

你在室外放了一支温度计。每分钟查看一次：
- 72°F
- 73°F
- 72°F
- 71°F
- 72°F

温度并没有真的上下跳那么多！是温度计有噪声。

## 噪声的类型

### 随机噪声
就像掷飞镖——有时偏高，有时偏低，但平均下来会命中目标。

```
Target: 50
Measurements: 49, 51, 50, 52, 48, 50, 51, 49
Average: 50 ✓ (correct!)
```

### 有偏噪声
就像一台总是读数偏重 2 pounds 的秤。

```
Real weight: 75 lbs
Scale reads: 77, 77, 78, 76, 77
Average: 77 (always 2 lbs too high!)
```

## 为什么噪声很重要

### 问题 1：不能信任单次测量

如果你只量一次身高，得到 4 feet 3 inches，那完全正确吗？也许你实际是 4 feet 2.8 inches，或者 4 feet 3.2 inches！

### 问题 2：事物会变化

你在追踪一辆在地板上滚动的玩具车。你测量它的位置：
- 第 1 秒：10 inches
- 第 2 秒：19 inches（应该是 20！）
- 第 3 秒：31 inches（应该是 30！）

噪声让你很难知道车的确切位置。

## 应对噪声

### 策略 1：进行多次测量

不要只测量一次，而是测量很多次并取平均！

```
Measurements: 50, 52, 49, 51, 50
Average: 50.4

This is probably closer to the truth than any single measurement!
```

### 策略 2：信任你的预测

如果你知道玩具车每秒移动 10 inches：
- 在第 2 秒，你预测：20 inches
- 你测量：19 inches
- 你想：“它大概在 19 到 20 之间，也许是 19.5 inches”

你并不完全信任这个带噪声的测量值！

### 策略 3：使用多个传感器

就像有两支温度计：
- 温度计 A 显示：72°F
- 温度计 B 显示：74°F
- 你估计：“大概在 73°F 左右”

## 现实世界中的例子

### 手机中的 GPS

手机的 GPS 并不总是显示你的确切位置。有时它会显示你：
- 在街道中间（而你其实在人行道上）
- 离你的真实位置 20 feet 远
- 即使你站着不动，位置也在跳来跳去

这就是 GPS 噪声！你的手机使用巧妙的技巧（比如卡尔曼滤波！）来判断你的真实位置。

### 视频游戏控制器

当你把游戏控制器握稳不动时，它可能检测到微小移动。这是来自传感器的噪声！游戏会把它滤除，这样你的角色就不会抖动。

### 机器人吸尘器

机器人吸尘器有传感器来检测墙壁和家具。但传感器并不完美——有时会看到不存在的东西，或者漏掉存在的东西！机器人必须聪明地判断该相信什么。

## 核心思想

**我们无法完美地测量事物，因此需要聪明地：**
1. 不完全信任任何单次测量
2. 组合多次测量
3. 利用预测来辅助
4. 滤除噪声

这正是卡尔曼滤波所做的！

## 自己动手试试！

### 实验 1：带噪声的尺子

1. 画一条恰好 10 cm 长的线（仔细使用尺子）
2. 让 5 个朋友用他们自己的尺子测量
3. 写下所有测量值
4. 它们都恰好是 10 cm 吗？很可能不是！
5. 计算平均值——它接近 10 cm 吗？

### 实验 2：反应时间

1. 使用在线反应时间测试
2. 测试 10 次
3. 写下你所有的用时
4. 注意到它们都不同吗？这就是噪声！
5. 你的平均反应时间是多少？

### 实验 3：数步数

1. 正常走 20 步
2. 让一个朋友数你的步数
3. 再做一次——让另一个朋友数
4. 做第三次——让第三个朋友数
5. 他们都数到恰好 20 吗？很可能不是！


<details>
<summary>English original</summary>

**Noisy Measurements**

**Level: Elementary (Ages 8-12)**

**Why Can't We Just Measure Things Exactly?**

Imagine you're trying to measure how tall you are with a ruler. Easy, right? But wait...

- Are you standing perfectly straight?
- Is the ruler perfectly straight?
- Are you reading it from exactly the right angle?
- Did you measure from the exact top of your head?

Every measurement has tiny errors. We call these errors **noise**.

**What is Noise?**

**Noise** is like static on a radio or fuzz on an old TV. It's the random errors that make measurements not quite perfect.

**Example 1: The Bathroom Scale**

Step on your bathroom scale 5 times in a row:
- First time: 75.2 lbs
- Second time: 75.4 lbs
- Third time: 75.1 lbs
- Fourth time: 75.3 lbs
- Fifth time: 75.2 lbs

You didn't gain or lose weight between measurements! The scale just isn't perfect. That's **measurement noise**.

**Example 2: Throwing Darts**

Imagine you're really good at darts and always aim for the bullseye. But your throws land:
- A little to the left
- A little to the right
- A little high
- A little low
- Right on target!

You're aiming at the same spot, but there's randomness in where the dart lands. That's noise!

**Example 3: Temperature Sensor**

You have a thermometer outside. You check it every minute:
- 72°F
- 73°F
- 72°F
- 71°F
- 72°F

The temperature didn't really jump around that much! The thermometer has noise.

**Types of Noise**

**Random Noise**
Like throwing darts - sometimes high, sometimes low, but on average you hit the target.

```
Target: 50
Measurements: 49, 51, 50, 52, 48, 50, 51, 49
Average: 50 ✓ (correct!)
```

**Biased Noise**
Like a scale that always reads 2 pounds too heavy.

```
Real weight: 75 lbs
Scale reads: 77, 77, 78, 76, 77
Average: 77 (always 2 lbs too high!)
```

**Why Noise Matters**

**Problem 1: You Can't Trust One Measurement**

If you measure your height once and get 4 feet 3 inches, is that exactly right? Maybe you're really 4 feet 2.8 inches, or 4 feet 3.2 inches!

**Problem 2: Things Change**

You're tracking a toy car rolling across the floor. You measure its position:
- At 1 second: 10 inches
- At 2 seconds: 19 inches (should be 20!)
- At 3 seconds: 31 inches (should be 30!)

The noise makes it hard to know exactly where the car is.

**Dealing with Noise**

**Strategy 1: Take Multiple Measurements**

Instead of measuring once, measure many times and average them!

```
Measurements: 50, 52, 49, 51, 50
Average: 50.4

This is probably closer to the truth than any single measurement!
```

**Strategy 2: Trust Your Prediction**

If you know the toy car moves 10 inches per second:
- At 2 seconds, you predict: 20 inches
- You measure: 19 inches
- You think: "It's probably between 19 and 20, maybe 19.5 inches"

You don't completely trust the noisy measurement!

**Strategy 3: Use Multiple Sensors**

Like having two thermometers:
- Thermometer A says: 72°F
- Thermometer B says: 74°F
- You estimate: "Probably around 73°F"

**Real World Examples**

**GPS in Your Phone**

Your phone's GPS doesn't always show your exact location. Sometimes it shows you:
- In the middle of the street (when you're on the sidewalk)
- 20 feet away from where you really are
- Jumping around even when you're standing still

That's GPS noise! Your phone uses clever tricks (like the Kalman filter!) to figure out where you really are.

**Video Game Controllers**

When you hold a game controller still, it might detect tiny movements. That's noise from the sensors! Games filter this out so your character doesn't jitter.

**Robot Vacuum Cleaners**

A robot vacuum has sensors to detect walls and furniture. But the sensors aren't perfect - sometimes they see things that aren't there, or miss things that are! The robot has to be smart about what to believe.

**The Big Idea**

**We can't measure things perfectly, so we need to be smart about:**
1. Not trusting any single measurement completely
2. Combining multiple measurements
3. Using predictions to help
4. Filtering out the noise

This is exactly what the Kalman filter does!

**Try It Yourself!**

**Experiment 1: Noisy Ruler**

1. Draw a line exactly 10 cm long (use a ruler carefully)
2. Have 5 friends measure it with their own rulers
3. Write down all the measurements
4. Are they all exactly 10 cm? Probably not!
5. Calculate the average - is it close to 10 cm?

**Experiment 2: Reaction Time**

1. Use an online reaction time test
2. Take the test 10 times
3. Write down all your times
4. Notice how they're all different? That's noise!
5. What's your average reaction time?

**Experiment 3: Counting Steps**

1. Walk 20 steps normally
2. Have a friend count your steps
3. Do it again - have another friend count
4. Do it a third time - have a third friend count
5. Did they all count exactly 20? Probably not!

</details>

### 实验 4：温度追踪

1. 一天之内每小时查看一次天气 app
2. 记下温度
3. 画一张图
4. 它是平滑变化，还是来回跳？
5. 其中有些跳动就是噪声！

## 思考题

1. 你认为为什么多次站上体重秤，它显示的数值会不同？
2. 如果对某个量测量 10 次得到 10 个不同的答案，哪一个才是「正确」的？
3. 是相信一次非常仔细的测量更好，还是相信 10 次快速测量的平均值更好？
4. 你能想到生活中还有哪些东西带有「噪声」吗？

## 接下来是什么？

既然我们已知道测量是有噪声的，下一章将教我们如何把不同的信息组合起来，做出更好的估计！

---

**关键词汇**
- **噪声**：测量中的随机误差
- **测量**：用传感器或工具弄清某个量
- **平均值**：把各个数相加，再除以数的个数
- **传感器**：测量某个量的设备（温度计、体重秤、GPS 等）
- **偏置**：测量始终朝同一方向出错


<details>
<summary>English original</summary>

**Experiment 4: Temperature Tracking**

1. Check a weather app every hour for a day
2. Write down the temperature
3. Make a graph
4. Does it change smoothly, or jump around?
5. Some of those jumps are noise!

**Questions to Think About**

1. Why do you think scales show different numbers when you step on them multiple times?
2. If you measure something 10 times and get 10 different answers, which one is "right"?
3. Is it better to trust one very careful measurement, or the average of 10 quick measurements?
4. Can you think of other things in your life that have "noise"?

**What's Next?**

Now that we know measurements are noisy, the next chapter will teach us how to combine different pieces of information to make better estimates!

---

**Key Vocabulary**
- **Noise**: Random errors in measurements
- **Measurement**: Using a sensor or tool to find out something
- **Average**: Adding up numbers and dividing by how many there are
- **Sensor**: A device that measures something (thermometer, scale, GPS, etc.)
- **Bias**: When measurements are consistently wrong in the same direction

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/4. Sensor Fusion/kalman-filter/02-noisy-measurements.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/4.%20Sensor%20Fusion/kalman-filter/02-noisy-measurements.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
