---
title: 第 4 讲：多机器人系统与群体机器人学
description: 第 4 讲：多机器人系统与群体机器人学
---

# 第 4 讲：多机器人系统与群体机器人学

## 概述

单机器人 ROS 2 已经是分布式系统。**多机器人**系统增加了**协调**：谁做哪个任务、哪张地图是真值、机器人如何在没有单点失效的情况下避免**干扰**。**群体**机器人学朝着**大量**廉价单元与**局部规则**的方向推进；**HRI** 层把**人**作为对等体加入回路。

**学完本讲后你应当能够：**

* 对比**集中式**、**去中心化**和**分层式**多机器人架构。
* 在高层次解释**任务分配**（拍卖 vs 优化）并说出其失效模式。
* 描述**多机器人 SLAM / 地图合并**问题（跨机器人回环闭合、相对位姿）。
* 概括 **Reynolds flocking** 及其为何可扩展。
* 列举 **HRI** 模式：命令、澄清、共享自主，以及**安全额定**的降速。

**前置要求：** 熟悉 [第 1 讲 —— 高级机器人操作系统](../02-高级机器人操作系统/Lecture-01.md) 中的 Nav2、TF2 和 ROS 2 图。

---

## 1. 多机器人协调

### 1.1 多机器人为何困难

* **共享工作空间：** 碰撞现在是**机器人之间**的；你的局部规划器必须（近似地）知道其他机器人在哪里。
* **部分可观测：** 没有机器人能看到完整世界；**通信**有损且有延迟。
* **一致性：** 每个机器人可能维护自己的**地图坐标系**；合并需要**相对位姿**估计。

### 1.2 任务分配

**问题：** 把任务（访问位置、搬运物体、巡逻）分配给具有不同**能力**和**电量**状态的机器人。

| 方法 | 思路 | 权衡 |
|----------|------|-----------|
| **拍卖 / 市场** | 机器人从一个中心或分布式拍卖者处对任务出价 | 简单；可能次优 |
| **优化**（MIP、OR-Tools） | 全局代价最小化 | 质量更好；需要模型并注意规模 |
| **贪心启发式** | 快速分配 | 适合在线重规划 |

**失效模式：** 任务互相阻塞时的**死锁**；一个机器人永远分不到活的**饥饿**；通信抖动时的**重规划风暴**。

### 1.3 多机器人 SLAM 与地图合并

每个机器人可能在自己的坐标系中运行**局部 SLAM**。**协同 SLAM** 在机器人之间的**相对变换**已知时合并地图（共享观测、已知地标，或位姿图的通信）。像 **Kimera-Multi** 这样的系统是研究级**分布式**建图的范例。

**ROS 2 实践角度：** 命名空间（`/robot1`、`/robot2`）、**多机器人 Nav2** 配置，以及要么共享一个公共 `map` 要么周期性**对齐**地图的 **tf** 树。

### 1.4 编队控制

**领航–跟随：** 跟随者以偏移量跟踪领航者位姿；简单，但领航者是单点失效。

**虚拟结构：** 团队表现得像刚体；适合**巡检**或**协同搬运**。

**基于一致性：** 每个 agent 用**邻居**误差调整状态；需要**连通性**图（谁与谁通信）。

---

## 2. 群体机器人学

### 2.1 Reynolds 规则（boids）

经典 **flocking** 组合三种力：

1. **分离：** 避免与邻居拥挤。
2. **对齐：** 匹配邻居的平均朝向。
3. **聚合：** 朝邻居的平均位置移动。

再加上**避障**，你就有了**大量** agent 的基线。参数（权重、邻域半径）决定群体是**聚簇**、**散开**还是**振荡**。

### 2.2 去中心化 vs 集中化

| 风格 | 优点 | 缺点 |
|-------|------|------|
| **集中式** | 全局最优更容易 | 单点失效、通信瓶颈 |
| **去中心化** | 鲁棒、可扩展 | 分析更难、涌现式失效 |

**共识主动性（Stigmergy）**（通过环境间接协调，例如信息素轨迹）出现在大型机器人群体和蚁群启发算法中。

### 2.3 可扩展性与容错

问：**10 → 100 → 1000** 个 agent 会发生什么？除非使用**空间哈希**或**有限距离**感知，否则 **O(n²)** 的邻居检查会爆炸。

**容错：** 机器人退出；任务应当**降级**（每小时任务数减少），而非**崩溃**——设计**冗余**和**基于超时**的重分配。

---

## 3. 人–机器人交互（HRI）

### 3.1 自然语言

**LLM + ASR** 可以把语音转成**任务图**，但**接地**仍然困难：机器人必须把词语映射到它实际能够到达的**物体**和**地点**。用于**澄清**的对话（"哪个箱子？"）可减少代价高昂的错误。

### 3.2 共享自主

人提供**意图**（摇杆方向、高层的"打扫这个房间"）；机器人处理**障碍物**、**抓取可行性**和**安全**。调节自主程度**多少**，以同时避免**无聊**（帮助太多）和**不信任**（帮助太少）。


<details>
<summary>English original</summary>

**Lecture 4: Multi-Robot Systems and Swarm Robotics**

**Overview**

Single-robot ROS 2 is already a distributed system. **Multi-robot** systems add **coordination**: who does which task, which map is ground truth, and how robots avoid **interference** without a single point of failure. **Swarm** robotics pushes toward **many** cheap units with **local rules**; **HRI** layers add **humans** as peers in the loop.

**By the end of this lecture you should be able to:**

* Contrast **centralized**, **decentralized**, and **hierarchical** multi-robot architectures.
* Explain **task allocation** at a high level (auction vs optimization) and name failure modes.
* Describe **multi-robot SLAM / map merging** problems (loop closure across robots, relative pose).
* Summarize **Reynolds flocking** and why it scales.
* List **HRI** patterns: command, clarification, shared autonomy, and **safety-rated** speed reduction.

**Prerequisite:** Comfortable with Nav2, TF2, and ROS 2 graphs from [Lecture 1 — Advanced Robot Operating System](../02-高级机器人操作系统/Lecture-01.md).

---

**1. Multi-robot coordination**

**1.1 Why multi-robot is hard**

* **Shared workspace:** Collisions are now **inter-robot**; your local planner must know (approximately) where others are.
* **Partial observability:** No robot sees the full world; **communication** is lossy and delayed.
* **Consistency:** Each robot may maintain its own **map frame**; merging requires **relative pose** estimates.

**1.2 Task allocation**

**Problem:** Assign tasks (visit locations, transport objects, patrol) to robots with different **capabilities** and **battery** states.

| Approach | Idea | Trade-off |
|----------|------|-----------|
| **Auction / market** | Robots bid on tasks from a central or distributed auctioneer | Simple; can be suboptimal |
| **Optimization** (MIP, OR-Tools) | Global cost minimization | Better quality; needs model and scale care |
| **Greedy heuristics** | Fast assignments | Good for online re-planning |

**Failure modes:** **Deadlock** when tasks block each other; **starvation** when one robot never gets work; **replanning storms** when communication flaps.

**1.3 Multi-robot SLAM and map merging**

Each robot may run **local SLAM** in its own frame. **Collaborative SLAM** merges maps when **relative transforms** between robots are known (shared observations, known landmarks, or communication of pose graphs). Systems like **Kimera-Multi** exemplify **distributed** mapping at research grade.

**Practical ROS 2 angle:** Namespaces (`/robot1`, `/robot2`), **multi-robot Nav2** setups, and **tf** trees that either share a common `map` or periodically **align** maps.

**1.4 Formation control**

**Leader–follower:** Follower tracks leader pose with offset; simple but single point of failure at leader.

**Virtual structure:** Team behaves as a rigid body; good for **inspection** or **coordinated transport**.

**Consensus-based:** Each agent adjusts state using **neighbor** errors; requires **connectivity** graphs (who talks to whom).

---

**2. Swarm robotics**

**2.1 Reynolds rules (boids)**

Classic **flocking** combines three forces:

1. **Separation:** Avoid crowding neighbors.
2. **Alignment:** Match average heading of neighbors.
3. **Cohesion:** Move toward average position of neighbors.

Add **obstacle avoidance** and you have a baseline for **many** agents. Parameters (weights, neighborhood radius) determine whether the swarm **clusters**, **disperses**, or **oscillates**.

**2.2 Decentralized vs centralized**

| Style | Pros | Cons |
|-------|------|------|
| **Centralized** | Global optimum easier | Single point of failure, comms bottleneck |
| **Decentralized** | Robust, scalable | Harder analysis, emergent failures |

**Stigmergy** (indirect coordination via environment, e.g. trails) appears in large robot swarms and ant-inspired algorithms.

**2.3 Scalability and fault tolerance**

Ask: What happens at **10 → 100 → 1000** agents? **O(n²)** neighbor checks explode unless you use **spatial hashing** or **limited-range** sensing.

**Fault tolerance:** Robots drop out; the mission should **degrade** (fewer tasks/hour), not **collapse**—design **redundancy** and **timeout-based** reassignment.

---

**3. Human–robot interaction (HRI)**

**3.1 Natural language**

**LLMs + ASR** can turn speech into **task graphs**, but **grounding** remains hard: the robot must map words to **objects** and **places** it can actually reach. Dialogue for **clarification** (“Which bin?”) reduces costly mistakes.

**3.2 Shared autonomy**

The human provides **intent** (joystick direction, high-level “clean this room”); the robot handles **obstacles**, **grasp feasibility**, and **safety**. Tune **how much** autonomy to avoid both **boredom** (too much help) and **mistrust** (too little).

</details>

### 3.3 靠近人类时的安全

**ISO/TS 15066** 为人类进入协作空间时的**速度**与**力**提供依据。软件在近距离时实现**降速**；**硬件**仍负责 **E-stop** 与**保护性停止**链路。

---

## 4. ROS 2 实践要点

* **命名空间与发现：** 同一网络上的多台机器人需要明确的 **ROS_DOMAIN_ID** / DDS 配置与**命名空间划分**，以避免 topic 冲突。
* **Nav2 多机器人：** 在仿真中对**多机器人**采用官方模式；在设定**机群**目标前验证**全局坐标系**对齐。
* **Open-RMF**（超出本讲深度）面向设施内的**机群**管理；若要在楼宇中部署**大量**机器人，值得后续跟进。

---

## 5. 项目（来自本路线图）

* **多机器人探索：** 在 Gazebo 类仿真中的三台机器人；**前沿探索**；合并地图或对齐坐标系；记录**通信**中断并重新规划。
* **集群编队：** 约 20 个 agent，采用 Reynolds 规则 + 障碍物；绘制随时间变化的**分离最小值**。
* **共享自治：** 摇杆 + 避障混合；用户研究可选——至少记录**每分钟干预次数**。

---

## 6. 自检

1. 为什么**地图合并**需要机器人之间的**相对位姿**，而不只是两张独立的 SLAM 地图？
2. 纯**贪心**任务分配的一个**失效模式**是什么？
3. Reynolds 中的**分离**与**避障**有何不同？

---

## 资源

* **"Multi-Robot Systems"**，Lynne Parker 著 —— 协调、任务分配、体系结构。
* **"Swarm Intelligence: From Natural to Artificial Systems"**，Bonabeau、Dorigo 与 Theraulaz 著。
* **ROS 2 / Nav2 多机器人：** 在当前 Nav2 与 ROS 2 文档中检索**多机器人**示例。
* **Open-RMF：** [openrmf.org](https://openrmf.org/) —— 机群管理（可选深入阅读）。

---

## 相关讲座

* [Advanced Robot Operating System](../02-高级机器人操作系统/Lecture-01.md) —— Nav2、TF2、单机器人基线。
* [Advanced Perception and AI for Robotics](../01-机器人高级感知与AI/Lecture-01.md) —— 面向协同团队的感知与学习。


<details>
<summary>English original</summary>

**3.3 Safety near humans**

**ISO/TS 15066** informs **speed** and **force** when humans enter collaborative spaces. Software implements **reduced speed** in proximity; **hardware** still owns **E-stop** and **protective stop** chains.

---

**4. ROS 2 practical notes**

* **Namespaces and discovery:** Multiple robots on one network need clear **ROS_DOMAIN_ID** / DDS config and **namespacing** to avoid topic collisions.
* **Nav2 multi-robot:** Use official patterns for **multiple robots** in simulation; verify **global frame** alignment before **fleet** goals.
* **Open-RMF** (outside this lecture’s depth) targets **fleet** management in facilities; worth a follow-on if you deploy **many** robots in buildings.

---

**5. Projects (from this roadmap)**

* **Multi-robot exploration:** Three robots in Gazebo-class sim; **frontier exploration**; merge maps or align frames; log **comm** drops and replan.
* **Swarm formation:** ~20 agents with Reynolds + obstacles; plot **separation minima** over time.
* **Shared autonomy:** Joystick + obstacle avoidance blending; user study optional—at minimum, log **interventions per minute**.

---

**6. Self-check**

1. Why does **map merging** need **relative pose** between robots, not just two independent SLAM maps?
2. What is one **failure mode** of purely **greedy** task assignment?
3. How does **separation** in Reynolds differ from **obstacle avoidance**?

---

**Resources**

* **"Multi-Robot Systems"** by Lynne Parker — coordination, task allocation, architectures.
* **"Swarm Intelligence: From Natural to Artificial Systems"** by Bonabeau, Dorigo, and Theraulaz.
* **ROS 2 / Nav2 multi-robot:** Search current Nav2 and ROS 2 docs for **multi-robot** examples.
* **Open-RMF:** [openrmf.org](https://openrmf.org/) — fleet management (optional deep dive).

---

**Related lectures**

* [Advanced Robot Operating System](../02-高级机器人操作系统/Lecture-01.md) — Nav2, TF2, single-robot baseline.
* [Advanced Perception and AI for Robotics](../01-机器人高级感知与AI/Lecture-01.md) — perception and learning for coordinated teams.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track D - Robotics/Multi-Robot Systems and Swarm Robotics/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20D%20-%20Robotics/Multi-Robot%20Systems%20and%20Swarm%20Robotics/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
