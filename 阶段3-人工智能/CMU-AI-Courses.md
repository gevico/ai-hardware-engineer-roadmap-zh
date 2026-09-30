---
title: 卡内基梅隆大学：AI 与视觉课程参考
description: 卡内基梅隆大学：AI 与视觉课程参考
---

# 卡内基梅隆大学：AI 与视觉课程参考

与 **AI Hardware Engineer Roadmap** 相关的一批 CMU 人工智能、机器学习与计算机视觉课程的参考清单 —— 依据官方课程页面与课程目录整理。

**来源：**
- [07-280 AI & ML I](https://www.cs.cmu.edu/~07280/#schedule)
- [15-463 Computational Photography](https://graphics.cs.cmu.edu/courses/15-463/2018_fall/)
- [16-385 Computer Vision Spring 2026 — Lectures](https://16385.courses.cs.cmu.edu/spring2026/lectures)
- [Szeliski: Computer Vision (2nd ed.)](https://szeliski.org/Book/)

---


## 1. 07-280: AI & Machine Learning I

**2026 春季新开** —— 取代 15-281 和 10-315。是 07-380 AI & ML II 的基础。

### 概览

| Item | Details |
|------|---------|
| **授课** | 周二 + 周四，11:00 am–12:20 pm，Tepper 1403 |
| **习题课** | 周五下午（5 个班） |
| **授课教师** | Nihar Shah、Pat Virtue |
| **教育助理** | Brynn Edmunds |
| **教材** | 无指定教材；阅读材料来自 AIMA、Bishop、Daume、Goodfellow、MML、Mitchell、Murphy、KMPA（均可在线获取或通过 CMU 图书馆获取） |

### 课程描述

AI 与 ML 的综合导论，把核心方法与现代方法衔接起来。学生动手实现里程碑式系统：**AlexNet**、**GPT-2** 和 **AlphaZero**。涵盖伦理与负责任的 AI 开发。

### 评分

| Component | Weight |
|-----------|--------|
| 期中考试 1 | 15% |
| 期中考试 2 | 15% |
| 期末考试 | 25% |
| 编程/书面作业 | 30% |
| 在线作业 | 5% |
| 课前阅读检查点 | 5% |
| 课堂参与（课上投票） | 5% |

**成绩档位（大致）：** A >=90%，B 80-90%，C 70-80%，D 60-70%。不按曲线调整。

### 前置要求（严格）

- **15-122** Principles of Imperative Computation
- **概率论**（同步修读）
- **线性代数**（先修）
- **15-151** 或 **21-127** Mathematical Foundations（先修）
- **微积分 2**（同步修读）

### 课程安排（2026 春季）

| Dates | Topic |
|-------|-------|
| 1/13 | 1. 导论 |
| 1/15 | 2. 启发式搜索 |
| 1/20 | 3. 对抗搜索 |
| 1/22 | 4. 约束满足问题 |
| 1/27 | 5. ML 问题形式化 |
| 1/29 | 6. 决策树 |
| 2/3 | 7. 线性回归 |
| 2/5 | 8. 优化 |
| 2/10 | 9. 逻辑回归 |
| 2/12 | 10. 特征工程与正则化 |
| 2/17 | 11. 神经网络 |
| 2/19 | 12. 神经网络（续） |
| **2/24** | **期中考试 1** |
| 2/26 | 13. AI 对齐 |
| 3/3, 3/5 | 春假 |
| 3/10 | 14. PyTorch、Autograd、预训练/迁移/微调 |
| 3/12 | 15. 面向计算机视觉的深度学习，GPU |
| 3/17 | 16. MLE 与概率建模 |
| 3/19 | 17. NLP、马尔可夫链、N-gram |
| 3/24 | 18. 特征学习、词嵌入 |
| 3/26 | 19. NLP：attention、位置编码 |
| 3/31 | 20. Transformer、大语言模型 |
| 4/2 | 21. 马尔可夫决策过程 |
| 4/7 | 22. 强化学习 |
| 4/9 | Carnival（停课） |
| 4/14 | 23. 深度强化学习 |
| 4/16 | 24. 蒙特卡洛树搜索 |
| **4/21** | **期中考试 2** |
| 4/23 | 25. AI/ML 伦理 |

### 作业

| HW | Type | Due |
|----|------|-----|
| HW0 | 在线 | 1/15 周四 |
| HW1 | 在线、书面、编程 | 1/22 周四 |
| HW2 | 在线、书面、编程（搜索与博弈） | 1/29 周四 |
| HW3 | 仅在线 | 2/5 周四 |
| HW4 | 仅书面 | 2/12 周四 |
| HW5 | 以编程为主 | 2/19 周四 |
| HW6 | 以书面为主 | 2/26 周四 |
| HW7 | 以编程为主 | 3/12 周四 |
| **HW8** | **构建 AlexNet** | 3/19 周四 |
| HW9 | 在线、书面、编程 | 3/26 周四 |
| HW10 | 在线、书面、编程 | 4/2 周四 |
| **HW11** | **构建 GPT-2** | 4/16 周四 |
| **HW12** | **构建 AlphaZero** | 4/23 周四 |

### 政策

- **迟交天数：** 所有作业合计 6 天；每份作业最多 2 天
- **课前阅读：** 去掉最低的 2 次检查点
- **课堂参与：** 课上投票参与率 >=80% 可获满分
- **协作：** 允许概念讨论；不得共享代码/文本；不得使用生成式 AI 生成提交内容
- **编程搭档：** 仅编程部分允许 2 人一组

### 对比：07-280 与 10-301

| 07-280 | 10-301 |
|--------|--------|
| 启发式搜索、对抗搜索、CSP | — |
| ML 并行/GPU 基础 | — |
| 蒙特卡洛树搜索 | — |
| Transformer 网络、大语言模型 | Y |
| 强化学习 | Y |
| ML 基础（决策树 -> 神经网络） | Y |
| 满足 AI 专业核心要求 | Y |
| 07-380 AI & ML II 的先修要求 | Y |

---

## 2. 15-463: Computational Photography —— 计算机视觉之前的深度洞察

**建议在 16-385 Computer Vision 之前修读。** 提供对成像物理、相机流水线以及支撑图形学与视觉的计算方法的基础理解。

*来源：[15-463 Fall 2018](https://graphics.cs.cmu.edu/courses/15-463/2018_fall/)*


<details>
<summary>English original</summary>

**Carnegie Mellon University: AI & Vision Courses Reference**

A reference of CMU's AI, machine learning, and computer vision courses relevant to the **AI Hardware Engineer Roadmap** — compiled from official course pages and catalogs.

**Sources:**
- [07-280 AI & ML I](https://www.cs.cmu.edu/~07280/#schedule)
- [15-463 Computational Photography](https://graphics.cs.cmu.edu/courses/15-463/2018_fall/)
- [16-385 Computer Vision Spring 2026 — Lectures](https://16385.courses.cs.cmu.edu/spring2026/lectures)
- [Szeliski: Computer Vision (2nd ed.)](https://szeliski.org/Book/)

---


**1. 07-280: AI & Machine Learning I**

**New in Spring 2026** — Replaces 15-281 and 10-315. Foundation for 07-380 AI & ML II.

**Overview**

| Item | Details |
|------|---------|
| **Lectures** | Tue + Thu, 11:00 am–12:20 pm, Tepper 1403 |
| **Recitation** | Friday afternoon (5 sections) |
| **Instructors** | Nihar Shah, Pat Virtue |
| **Education Associate** | Brynn Edmunds |
| **Textbook** | No required textbook; readings from AIMA, Bishop, Daume, Goodfellow, MML, Mitchell, Murphy, KMPA (all online or via CMU Library) |

**Course Description**

Integrated introduction to AI and ML bridging core methods with modern approaches. Students build implementations of landmark systems: **AlexNet**, **GPT-2**, and **AlphaZero**. Covers ethics and responsible AI development.

**Grading**

| Component | Weight |
|-----------|--------|
| Midterm 1 | 15% |
| Midterm 2 | 15% |
| Final Exam | 25% |
| Programming/Written Homework | 30% |
| Online Homework | 5% |
| Pre-reading Checkpoints | 5% |
| Participation (in-class polls) | 5% |

**Grade boundaries (rough):** A >=90%, B 80-90%, C 70-80%, D 60-70%. Not curved.

**Prerequisites (Strict)**

- **15-122** Principles of Imperative Computation
- **Probability** (concurrent)
- **Linear Algebra** (prior)
- **15-151** or **21-127** Mathematical Foundations (prior)
- **Calculus 2** (concurrent)

**Schedule (Spring 2026)**

| Dates | Topic |
|-------|-------|
| 1/13 | 1. Introduction |
| 1/15 | 2. Heuristic Search |
| 1/20 | 3. Adversarial Search |
| 1/22 | 4. Constraint Satisfaction Problems |
| 1/27 | 5. ML Problem Formulation |
| 1/29 | 6. Decision Trees |
| 2/3 | 7. Linear Regression |
| 2/5 | 8. Optimization |
| 2/10 | 9. Logistic Regression |
| 2/12 | 10. Feature Engineering and Regularization |
| 2/17 | 11. Neural Networks |
| 2/19 | 12. Neural Networks (cont.) |
| **2/24** | **Midterm Exam 1** |
| 2/26 | 13. AI Alignment |
| 3/3, 3/5 | Spring Break |
| 3/10 | 14. PyTorch, Autograd, Pre-training/Transfer/Fine-tuning |
| 3/12 | 15. Deep Learning for Computer Vision, GPUs |
| 3/17 | 16. MLE and Probabilistic Modeling |
| 3/19 | 17. NLP, Markov Chains, N-grams |
| 3/24 | 18. Feature Learning, Word Embeddings |
| 3/26 | 19. NLP: Attention, Position Encoding |
| 3/31 | 20. Transformers, LLMs |
| 4/2 | 21. Markov Decision Processes |
| 4/7 | 22. Reinforcement Learning |
| 4/9 | Carnival (no class) |
| 4/14 | 23. Deep Reinforcement Learning |
| 4/16 | 24. Monte Carlo Tree Search |
| **4/21** | **Midterm Exam 2** |
| 4/23 | 25. AI/ML Ethics |

**Assignments**

| HW | Type | Due |
|----|------|-----|
| HW0 | Online | 1/15 Thu |
| HW1 | Online, Written, Programming | 1/22 Thu |
| HW2 | Online, Written, Programming (Search & Games) | 1/29 Thu |
| HW3 | Online only | 2/5 Thu |
| HW4 | Written only | 2/12 Thu |
| HW5 | Mostly Programming | 2/19 Thu |
| HW6 | Mostly Written | 2/26 Thu |
| HW7 | Mostly Programming | 3/12 Thu |
| **HW8** | **Building AlexNet** | 3/19 Thu |
| HW9 | Online, Written, Programming | 3/26 Thu |
| HW10 | Online, Written, Programming | 4/2 Thu |
| **HW11** | **Building GPT-2** | 4/16 Thu |
| **HW12** | **Building AlphaZero** | 4/23 Thu |

**Policies**

- **Late days:** 6 total across all assignments; max 2 per assignment
- **Pre-reading:** Lowest 2 checkpoints dropped
- **Participation:** >=80% of in-class polls for full credit
- **Collaboration:** Conceptual discussion allowed; no sharing code/text; generative AI may not be used to generate submissions
- **Programming partners:** Groups of 2 allowed for programming components only

**Comparison: 07-280 vs 10-301**

| 07-280 | 10-301 |
|--------|--------|
| Heuristic Search, Adversarial Search, CSPs | — |
| ML Parallelism/GPU Basics | — |
| Monte Carlo Tree Search | — |
| Transformer networks, LLMs | Y |
| Reinforcement Learning | Y |
| ML fundamentals (decision trees -> neural nets) | Y |
| Fulfills AI Major core | Y |
| Prereq for 07-380 AI & ML II | Y |

---

**2. 15-463: Computational Photography — Deep Insight Before Computer Vision**

**Recommended before 16-385 Computer Vision.** Provides foundational understanding of imaging physics, camera pipelines, and computational methods that underpin both graphics and vision.

*Source: [15-463 Fall 2018](https://graphics.cs.cmu.edu/courses/15-463/2018_fall/)*

</details>

### 概述

| 项目 | 详情 |
|------|---------|
| **交叉选课** | 15-463（本科）、15-663（硕士）、15-862（博士） |
| **时间** | 周一 + 周三，12:00-1:20 PM |
| **教材** | [Computer Vision: Algorithms and Applications](http://szeliski.org/Book/)（Szeliski），在线免费 |

### 课程简介

计算摄影是**计算机图形学**、**计算机视觉**与**成像**的融合。它通过将成像与计算相结合，克服传统相机的局限，带来捕获、表示和与物理世界交互的新方式。

主题：现代图像处理流水线（mobile/DSLR）、图像/视频编辑、3D 扫描、编码摄影、光场成像、time-of-flight、VR/AR 显示、计算光传输。进阶主题：光速相机、非视距成像、穿透组织成像。

### 前置要求（满足其一）

- 18-793 图像与视频处理
- **15-462 计算机图形学**，或
- 16-720 计算机视觉，或
- **16-385 计算机视觉**

需要线性代数、微积分、编程和图像计算基础。

### 评分

| 组成 | 权重 |
|-----------|--------|
| 7 次作业 | 70% |
| 期末项目 | 25% |
| 课堂参与 | 5% |

**迟交政策：** 总共 6 天免费迟交；每额外迟交一天 = 扣 10%；每次作业最多迟交 4 天。

### 教学大纲（2018 年秋季）

| 主题 |
|-------|
| 引言 |
| 数字摄影流水线 |
| 针孔与镜头 |
| 摄影光学与曝光 |
| 高动态范围成像 |
| 色调映射与双边滤波 |
| 颜色 |
| 图像合成 |
| 梯度域图像处理 |
| 焦栈与光场 |
| 反卷积 |
| 相机模型与校准 |
| 双视图几何 |
| 辐射度学与反射率 |
| 光度立体 |
| 光传输矩阵 |
| 计算光传输 |
| 立体与结构光 |
| 飞行时间成像 |
| 非视距成像 |
| 傅里叶光学 |
| 蒙特卡洛渲染入门 |

### 作业

7 次作业，包含**编程（Matlab）**和**摄影（DSLR）**两部分。期末项目可使用光场相机、ToF 相机、深度传感器、结构光系统。

### 为什么在计算机视觉之前学

15-463 为**图像如何形成**（光学、辐射度学、传感器）和**如何处理图像**（HDR、反卷积、校准、立体）建立直觉。这一物理与算法基础使 16-385 计算机视觉（检测、识别、几何）更容易掌握。

---

## 3. 图形学与成像课程

| 代码 | 名称 | 备注 |
|------|------|-------|
| **15-462** | 计算机图形学 | 渲染、变换、着色 —— 15-463 的基础 |
| **15-463** | 计算摄影 | 成像物理、相机流水线、HDR、光场 —— **学 16-385 前的深入洞察** |
| **16-385** | 计算机视觉 | 图像处理、检测、识别、基于几何的视觉 |

---

## 4. 自学内容与路线图的对应

对于遵循 **AI Hardware Engineer Roadmap** 的学习者，以下是 CMU 的 AI/视觉课程如何对应：

| 路线图阶段 | CMU 课程 | 重叠部分 |
|---------------|---------------|---------|
| **阶段 3：神经网络** | 07-280（神经网络、AlexNet、PyTorch） | 图、训练、自动微分，先于阶段 4 方向 A/B 硬件 |
| **阶段 3：计算机视觉** | **15-463**（计算摄影）-> 16-385（计算机视觉） | 成像物理、相机流水线，然后是检测/识别 |
| **阶段 3：边缘 AI** | 07-280（部署主题）、Jetson 相关实验 | 端侧流水线、延迟/隐私上下文；与阶段 4 方向 B 搭配 |
| **阶段 3：传感器融合** | 15-463（成像）、16-385（视觉） | 多传感器感知，先于阶段 4 的 Jetson 集成 |
| **阶段 4 方向 B（Jetson）** | 07-280、16-385、Jetson/Holoscan 实验 | 端侧模型、流水线、延迟 |
| **阶段 5：边缘计算** | 07-280、16-385、Jetson/Holoscan | 高效模型、流式处理、Holoscan |
| **阶段 5：AI 芯片设计** | 07-280（优化、GPU/并行）、16-211（数学） | 并行计算、面向加速器的线性代数 |

### 建议自学顺序（受 CMU 启发，聚焦 AI）

1. **15-122 等效** —— 命令式编程（C/Python）
2. **21-120、21-122、21-241** —— 微积分、线性代数
3. **07-280 主题** —— 搜索 -> ML -> 神经网络 -> RL -> Transformer
4. **15-462**（可选）—— 计算机图形学 —— 渲染、变换
5. **15-463** —— **计算摄影** —— 成像物理、相机流水线、HDR、光场 *（学视觉前的深入洞察）*
6. **16-385** —— 计算机视觉

---

## 5. 补充资源：Szeliski 教材及相关课程


<details>
<summary>English original</summary>

**Overview**

| Item | Details |
|------|---------|
| **Cross-listing** | 15-463 (undergrad), 15-663 (Master's), 15-862 (PhD) |
| **Schedule** | Mon + Wed, 12:00-1:20 PM |
| **Textbook** | [Computer Vision: Algorithms and Applications](http://szeliski.org/Book/) (Szeliski), free online |

**Course Description**

Computational photography is the convergence of **computer graphics**, **computer vision**, and **imaging**. It overcomes traditional camera limitations by combining imaging and computation for new ways of capturing, representing, and interacting with the physical world.

Topics: modern image processing pipelines (mobile/DSLR), image/video editing, 3D scanning, coded photography, lightfield imaging, time-of-flight, VR/AR displays, computational light transport. Advanced topics: cameras at light speed, non-line-of-sight imaging, seeing through tissue.

**Prerequisites (one of)**

- 18-793 Image and Video Processing
- **15-462 Computer Graphics**, OR
- 16-720 Computer Vision, OR
- **16-385 Computer Vision**

Linear algebra, calculus, programming, and image computation required.

**Grading**

| Component | Weight |
|-----------|--------|
| 7 Homework Assignments | 70% |
| Final Project | 25% |
| Class Participation | 5% |

**Late policy:** 6 free late days total; each additional late day = 10% penalty; max 4 days late per assignment.

**Syllabus (Fall 2018)**

| Topic |
|-------|
| Introduction |
| Digital photography pipeline |
| Pinholes and lenses |
| Photographic optics and exposure |
| High dynamic range imaging |
| Tonemapping and bilateral filtering |
| Color |
| Image compositing |
| Gradient-domain image processing |
| Focal stacks and lightfields |
| Deconvolution |
| Camera models and calibration |
| Two-view geometry |
| Radiometry and reflectance |
| Photometric stereo |
| Light transport matrices |
| Computational light transport |
| Stereo and structured light |
| Time-of-flight imaging |
| Non-line-of-sight imaging |
| Fourier optics |
| Monte Carlo rendering 101 |

**Assignments**

7 homework assignments with **programming (Matlab)** and **photography (DSLR)** components. Final project may use lightfield cameras, ToF cameras, depth sensors, structured light systems.

**Why Before Computer Vision**

15-463 builds intuition for **how images are formed** (optics, radiometry, sensors) and **how to process them** (HDR, deconvolution, calibration, stereo). This physical and algorithmic foundation makes 16-385 Computer Vision (detection, recognition, geometry) much easier to grasp.

---

**3. Graphics & Imaging Courses**

| Code | Name | Notes |
|------|------|-------|
| **15-462** | Computer Graphics | Rendering, transforms, shading — foundation for 15-463 |
| **15-463** | Computational Photography | Imaging physics, camera pipelines, HDR, lightfields — **deep insight before 16-385** |
| **16-385** | Computer Vision | Image processing, detection, recognition, geometry-based vision |

---

**4. Self-Study Mapping to Roadmap**

For learners following the **AI Hardware Engineer Roadmap**, here is how CMU's AI/vision curriculum aligns:

| Roadmap Phase | CMU Course(s) | Overlap |
|---------------|---------------|---------|
| **Phase 3: Neural Networks** | 07-280 (Neural Nets, AlexNet, PyTorch) | Graphs, training, autodiff before Phase 4 Track A/B hardware |
| **Phase 3: Computer Vision** | **15-463** (Computational Photography) -> 16-385 (Computer Vision) | Imaging physics, camera pipelines, then detection/recognition |
| **Phase 3: Edge AI** | 07-280 (deployment themes), Jetson-adjacent labs | On-device pipeline, latency/privacy context; pairs with Phase 4 Track B |
| **Phase 3: Sensor Fusion** | 15-463 (imaging), 16-385 (Vision) | Multi-sensor perception before Phase 4 Jetson integration |
| **Phase 4 Track B (Jetson)** | 07-280, 16-385, Jetson/Holoscan labs | Models on device, pipelines, latency |
| **Phase 5: Edge Computing** | 07-280, 16-385, Jetson/Holoscan | Efficient models, streaming, Holoscan |
| **Phase 5: AI Chip Design** | 07-280 (optimization, GPU/parallel), 16-211 (math) | Parallel compute, linear algebra for accelerators |

**Suggested Self-Study Order (CMU-Inspired, AI focus)**

1. **15-122 equivalent** — Imperative programming (C/Python)
2. **21-120, 21-122, 21-241** — Calculus, linear algebra
3. **07-280 topics** — Search -> ML -> Neural Nets -> RL -> Transformers
4. **15-462** (optional) — Computer Graphics — rendering, transforms
5. **15-463** — **Computational Photography** — imaging physics, camera pipelines, HDR, lightfields *(deep insight before vision)*
6. **16-385** — Computer vision

---

**5. Additional Resources: Szeliski Book & Related Courses**

</details>

### Computer Vision: Algorithms and Applications (2nd ed.)

**[https://szeliski.org/Book/](https://szeliski.org/Book/)** — Richard Szeliski, University of Washington（c 2022）

计算机视觉领域的权威教材。可免费下载 PDF 供个人使用。被 15-463 Computational Photography 以及全球众多视觉课程采用。涵盖图像形成、特征检测、立体视觉、structure from motion、识别等。

### 相关课程（来自 [Szeliski Book](https://szeliski.org/Book/)）

计算机视觉与计算摄影的其他优质来源，大致按由近及远排序：

| 课程 | 院校 | 讲师 | 学期 |
|--------|-------------|---------------|------|
| [CS5670 Introduction to Computer Vision](https://www.cs.cornell.edu/courses/cs5670/2025sp/) | Cornell Tech | Noah Snavely | Spring 2025 |
| [6.8300/6.8301 Advances in Computer Vision](https://szeliski.org/Book/) | MIT | Bill Freeman, Antonio Torralba, Phillip Isola | Spring 2023 |
| [16-385 Computer Vision](http://www.cs.cmu.edu/~16385/) | CMU | Matthew O'Toole | Fall 2024 |
| [16-385 Computer Vision — Lectures](https://16385.courses.cs.cmu.edu/spring2026/lectures) | CMU | — | Spring 2026 |
| [CS194-26/294-26 Intro to Computer Vision and Computational Photography](https://szeliski.org/Book/) | Berkeley | Alyosha Efros | Fall 2024 |
| [15-463, 15-663, 15-862 Computational Photography](https://graphics.cs.cmu.edu/courses/15-463/) | CMU | Ioannis Gkioulekas | Fall 2024 |
| [CSCI 1430 Computer Vision](https://szeliski.org/Book/) | Brown | James Tompkin | Spring 2025 |
| [CMPT 412 and 762 Computer Vision](https://szeliski.org/Book/) | Simon Fraser | Yasutaka Furukawa | Fall 2023 |
| [CS 4476-A / 6476-A Computer Vision](https://szeliski.org/Book/) | Georgia Tech | James Hays | Fall 2022 |
| [EECS 498.008 / 598.008 Deep Learning for Computer Vision](https://szeliski.org/Book/) | U Michigan | Justin Johnson | Winter 2022 — *深度学习与视觉识别的出色入门课* |
| [DS-GA 1008 Deep Learning](https://szeliski.org/Book/) | NYU | Yann LeCun, Alfredo Canziani | Spring 2021 |
| [Fundamentals and Trends in Vision and Image Processing](https://szeliski.org/Book/) | IMPA | Luiz Velho | Spring 2021 |
| [CS294-158 Deep Unsupervised Learning](https://szeliski.org/Book/) | UC Berkeley | — | Spring 2020 |
| [CSCI 497P/597P Introduction to Computer Vision](https://szeliski.org/Book/) | Western Washington | Scott Wehrwein | Spring 2020 |
| [EECS 504 Foundations of Computer Vision](https://szeliski.org/Book/) | U Michigan | Andrew Owens | Winter 2020 |

*课程链接维护于 [szeliski.org/Book](https://szeliski.org/Book/)。如需添加你的课程，请联系作者。*

---

## 链接

| 资源 | URL |
|----------|-----|
| 07-280 AI & ML I | https://www.cs.cmu.edu/~07280/#schedule |
| 16-385 Computer Vision Spring 2026 Lectures | https://16385.courses.cs.cmu.edu/spring2026/lectures |
| 15-463 Computational Photography | https://graphics.cs.cmu.edu/courses/15-463/2018_fall/ |
| **Szeliski: Computer Vision (2nd ed.)** | https://szeliski.org/Book/ |

---

*最后更新：2025 年 2 月。课程开设情况与要求可能变化；请以 CMU 官方来源为准。*


<details>
<summary>English original</summary>

**Computer Vision: Algorithms and Applications (2nd ed.)**

**[https://szeliski.org/Book/](https://szeliski.org/Book/)** — Richard Szeliski, University of Washington (c 2022)

The canonical computer vision textbook. Free PDF download for personal use. Used by 15-463 Computational Photography and many vision courses worldwide. Covers image formation, feature detection, stereo, structure from motion, recognition, and more.

**Related Courses (from [Szeliski Book](https://szeliski.org/Book/))**

Additional good sources for computer vision and computational photography, sorted roughly by most recent:

| Course | Institution | Instructor(s) | Term |
|--------|-------------|---------------|------|
| [CS5670 Introduction to Computer Vision](https://www.cs.cornell.edu/courses/cs5670/2025sp/) | Cornell Tech | Noah Snavely | Spring 2025 |
| [6.8300/6.8301 Advances in Computer Vision](https://szeliski.org/Book/) | MIT | Bill Freeman, Antonio Torralba, Phillip Isola | Spring 2023 |
| [16-385 Computer Vision](http://www.cs.cmu.edu/~16385/) | CMU | Matthew O'Toole | Fall 2024 |
| [16-385 Computer Vision — Lectures](https://16385.courses.cs.cmu.edu/spring2026/lectures) | CMU | — | Spring 2026 |
| [CS194-26/294-26 Intro to Computer Vision and Computational Photography](https://szeliski.org/Book/) | Berkeley | Alyosha Efros | Fall 2024 |
| [15-463, 15-663, 15-862 Computational Photography](https://graphics.cs.cmu.edu/courses/15-463/) | CMU | Ioannis Gkioulekas | Fall 2024 |
| [CSCI 1430 Computer Vision](https://szeliski.org/Book/) | Brown | James Tompkin | Spring 2025 |
| [CMPT 412 and 762 Computer Vision](https://szeliski.org/Book/) | Simon Fraser | Yasutaka Furukawa | Fall 2023 |
| [CS 4476-A / 6476-A Computer Vision](https://szeliski.org/Book/) | Georgia Tech | James Hays | Fall 2022 |
| [EECS 498.008 / 598.008 Deep Learning for Computer Vision](https://szeliski.org/Book/) | U Michigan | Justin Johnson | Winter 2022 — *outstanding intro to deep learning and visual recognition* |
| [DS-GA 1008 Deep Learning](https://szeliski.org/Book/) | NYU | Yann LeCun, Alfredo Canziani | Spring 2021 |
| [Fundamentals and Trends in Vision and Image Processing](https://szeliski.org/Book/) | IMPA | Luiz Velho | Spring 2021 |
| [CS294-158 Deep Unsupervised Learning](https://szeliski.org/Book/) | UC Berkeley | — | Spring 2020 |
| [CSCI 497P/597P Introduction to Computer Vision](https://szeliski.org/Book/) | Western Washington | Scott Wehrwein | Spring 2020 |
| [EECS 504 Foundations of Computer Vision](https://szeliski.org/Book/) | U Michigan | Andrew Owens | Winter 2020 |

*Course links are maintained at [szeliski.org/Book](https://szeliski.org/Book/). Contact the author to add your course.*

---

**Links**

| Resource | URL |
|----------|-----|
| 07-280 AI & ML I | https://www.cs.cmu.edu/~07280/#schedule |
| 16-385 Computer Vision Spring 2026 Lectures | https://16385.courses.cs.cmu.edu/spring2026/lectures |
| 15-463 Computational Photography | https://graphics.cs.cmu.edu/courses/15-463/2018_fall/ |
| **Szeliski: Computer Vision (2nd ed.)** | https://szeliski.org/Book/ |

---

*Last updated: February 2025. Course offerings and requirements may change; verify with CMU official sources.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/CMU-AI-Courses.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/CMU-AI-Courses.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
