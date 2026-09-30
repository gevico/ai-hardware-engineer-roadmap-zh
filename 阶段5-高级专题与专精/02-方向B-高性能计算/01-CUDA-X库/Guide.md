---
title: NVIDIA CUDA-X 库
description: NVIDIA CUDA-X 库
---

# NVIDIA CUDA-X 库

<div class="course-identity auto-course" style="--course-accent: #475569; --course-accent-rgb: 71, 85, 105;" markdown="1">
<div class="course-identity__icon">NCL</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入探索 · 专项</p>
<p class="course-identity__title">NVIDIA CUDA-X 库的专项课程标识。</p>
<p class="course-identity__meta">产物：专项案例研究 · 度量：性能、可靠性、角色匹配度</p>
</div>
</div>


**父级：** [阶段 5 — 高性能计算](../Guide.md)

**时间线：** 持续参考 —— 按项目需要逐类学习。

**前置要求：** 阶段 1 §4（C++/CUDA）、阶段 4 方向 B（Jetson、CUDA runtime）、阶段 4 方向 C 第 2 部分（DL 推理优化）。

---

## CUDA-X 是什么

**CUDA-X** 是 NVIDIA 基于 CUDA runtime 构建的全套 GPU 加速库。CUDA 提供编程模型（kernel、流、内存），而 CUDA-X 提供**生产级实现**，涵盖原本要花数月手写的算法：线性代数、FFT、神经网络原语、图分析、视频编解码、多 GPU 通信等等。

AI 芯片栈的每一层都会消费 CUDA-X 库：
- **L1（应用）：** cuDNN、TensorRT、DALI、CV-CUDA、DeepStream
- **L2（编译器）：** CUTLASS、FlashInfer、作为代码生成目标的 cuDNN 原语
- **L3（Runtime）：** cuBLAS、cuFFT、NCCL、NVSHMEM、GPUDirect Storage
- **L1–L3（数据）：** cuDF、cuML、cuGraph、cuVS、NeMo Curator

理解每个库做什么 —— 以及何时使用它、何时编写自定义 kernel —— 对任何 GPU 基础设施岗位都至关重要。

---

## CUDA 数学库

计算密集型工作负载的基石：分子动力学、CFD、医学成像、地震勘探，以及每个神经网络内部的矩阵运算。

| 库 | 功能 | 关键 API / 概念 | 使用场景 |
|---------|-------------|--------------------|-----------------------|
| **cuBLAS** | GPU 加速的 BLAS（基础线性代数子程序） | GEMM（矩阵-矩阵乘）(`cublasSgemm`, `cublasGemmEx`)、批处理 GEMM、混合精度（FP16/BF16/FP8 累加到 FP32） | 任意稠密矩阵乘 —— AI 推理与训练最重要的单一库 |
| **cuFFT** | GPU 上的快速傅里叶变换 | 1D/2D/3D FFT、实数到复数、批处理 FFT、多 GPU FFT | 信号处理、音频、频谱分析、通过 FFT 实现卷积 |
| **cuRAND** | GPU 上的随机数生成 | 伪随机（XORWOW、MRG32k3a、Philox）与准随机（Sobol）生成器；host 与 device API | 蒙特卡洛模拟、训练中的 dropout、数据增强 |
| **cuSOLVER** | 稠密与稀疏直接求解器 | LU、Cholesky、QR、SVD、特征值分解；稀疏 Cholesky | 线性系统、最小二乘、PCA、矩阵分解 |
| **cuSPARSE** | 稀疏矩阵运算 | SpMV、SpMM、稀疏三角求解；CSR、CSC、COO 格式 | GNN、稀疏 attention、稀疏矩阵科学计算 |
| **cuTENSOR** | 张量收缩与运算 | 张量收缩、归约、多维数组的逐元素运算 | 量子化学、张量网络、高维收缩 |
| **cuDSS** | 直接稀疏求解器 | GPU 加速的稀疏 LU/Cholesky | 结构分析、电路仿真中的大型稀疏系统 |
| **CUDA Math API** | 标准数学函数 | `sin`, `cos`, `exp`, `log`, `sqrt` —— GPU 优化，单精度与双精度 | 任何需要标准数学的 kernel —— 这些是 intrinsics，不是库调用 |
| **AmgX** | 代数多重网格求解器 | AMG 预条件器 + Krylov 求解器，用于隐式非结构化方法 | CFD、结构力学、任何使用隐式方法的 PDE 求解器 |
| **nvmath-python** | CUDA 数学的 Python 接口 | `nvmath.fft`, `nvmath.linalg` —— 兼容 NumPy 但 GPU 加速 | 在 Python 中快速原型化 GPU 数学计算，无需编写 C++ |

### 先学什么

**cuBLAS** —— 如果你理解 GEMM（形状、主维度、转置、批处理、混合精度），你就理解了 AI 计算的 80%。从这里开始。

### 项目

1. **cuBLAS GEMM benchmark** —— 在 GPU 上 benchmark `cublasGemmEx` 的 FP32、FP16、INT8 性能。绘制 TFLOPS 与矩阵大小的关系图。找出 GPU 超越 CPU BLAS 的交叉点。
2. **cuFFT 频谱分析** —— 对图像应用 2D FFT，滤除高频，逆 FFT。对比 4K 图像的 GPU 与 CPU 耗时。
3. **cuSOLVER SVD** —— 在 GPU 上计算大型矩阵的 SVD。用于图像压缩（保留 top-k 奇异值）。与 NumPy 对比 benchmark。

---


<details>
<summary>English original</summary>

**NVIDIA CUDA-X Libraries**

<div class="course-identity auto-course" style="--course-accent: #475569; --course-accent-rgb: 71, 85, 105;" markdown="1">
<div class="course-identity__icon">NCL</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for NVIDIA CUDA-X Libraries.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [Phase 5 — High Performance Computing](../Guide.md)

**Timeline:** Ongoing reference — study each category as your projects demand it.

**Prerequisites:** Phase 1 §4 (C++/CUDA), Phase 4 Track B (Jetson, CUDA runtime), Phase 4 Track C Part 2 (DL inference optimization).

---

**What CUDA-X is**

**CUDA-X** is NVIDIA's full suite of GPU-accelerated libraries built on top of the CUDA runtime. While CUDA gives you the programming model (kernels, streams, memory), CUDA-X gives you **production-grade implementations** of the algorithms you'd otherwise spend months writing: linear algebra, FFTs, neural network primitives, graph analytics, video codecs, multi-GPU communication, and more.

Every layer of the AI chip stack consumes CUDA-X libraries:
- **L1 (Application):** cuDNN, TensorRT, DALI, CV-CUDA, DeepStream
- **L2 (Compiler):** CUTLASS, FlashInfer, cuDNN primitives as codegen targets
- **L3 (Runtime):** cuBLAS, cuFFT, NCCL, NVSHMEM, GPUDirect Storage
- **L1–L3 (Data):** cuDF, cuML, cuGraph, cuVS, NeMo Curator

Understanding what each library does — and when to use it vs. writing a custom kernel — is essential for any GPU infrastructure role.

---

**CUDA Math Libraries**

The foundation for compute-intensive workloads: molecular dynamics, CFD, medical imaging, seismic exploration, and the matrix math inside every neural network.

| Library | What it does | Key APIs / Concepts | When to use it |
|---------|-------------|--------------------|-----------------------|
| **cuBLAS** | GPU-accelerated BLAS (Basic Linear Algebra Subprograms) | GEMM (`cublasSgemm`, `cublasGemmEx`), batched GEMM, mixed-precision (FP16/BF16/FP8 accumulate to FP32) | Any dense matrix multiply — the single most important library for AI inference and training |
| **cuFFT** | Fast Fourier Transform on GPU | 1D/2D/3D FFTs, real-to-complex, batched FFTs, multi-GPU FFT | Signal processing, audio, spectral analysis, conv via FFT |
| **cuRAND** | Random number generation on GPU | Pseudorandom (XORWOW, MRG32k3a, Philox) and quasirandom (Sobol) generators; host and device API | Monte Carlo simulations, dropout in training, data augmentation |
| **cuSOLVER** | Dense and sparse direct solvers | LU, Cholesky, QR, SVD, eigenvalue decomposition; sparse Cholesky | Linear systems, least squares, PCA, matrix factorization |
| **cuSPARSE** | Sparse matrix operations | SpMV, SpMM, sparse triangular solve; CSR, CSC, COO formats | GNNs, sparse attention, scientific computing with sparse matrices |
| **cuTENSOR** | Tensor contractions and operations | Tensor contraction, reduction, element-wise ops on multi-dimensional arrays | Quantum chemistry, tensor networks, high-dimensional contractions |
| **cuDSS** | Direct sparse solver | Sparse LU/Cholesky with GPU acceleration | Large sparse systems in structural analysis, circuit simulation |
| **CUDA Math API** | Standard math functions | `sin`, `cos`, `exp`, `log`, `sqrt` — GPU-optimized, single and double precision | Any kernel that needs standard math — these are intrinsics, not library calls |
| **AmgX** | Algebraic multigrid solver | AMG preconditioner + Krylov solvers for implicit unstructured methods | CFD, structural mechanics, any PDE solver using implicit methods |
| **nvmath-python** | Python interface to CUDA math | `nvmath.fft`, `nvmath.linalg` — NumPy-compatible but GPU-accelerated | Rapid prototyping of GPU math in Python without writing C++ |

**What to study first**

**cuBLAS** — if you understand GEMM (shapes, leading dimensions, transposition, batching, mixed precision), you understand 80% of AI compute. Start here.

**Projects**

1. **cuBLAS GEMM benchmark** — Benchmark `cublasGemmEx` for FP32, FP16, INT8 on your GPU. Plot TFLOPS vs matrix size. Find the crossover where GPU beats CPU BLAS.
2. **cuFFT spectral analysis** — Apply 2D FFT to an image, filter high frequencies, inverse FFT. Compare GPU vs CPU time for 4K images.
3. **cuSOLVER SVD** — Compute SVD of a large matrix on GPU. Use it for image compression (keep top-k singular values). Benchmark vs NumPy.

---

</details>

## 科学计算库

面向需要神经网络遵循数学对称性的应用——分子结构、蛋白质、材料科学。

| 库 | 功能 | 领域 |
|---------|-------------|--------|
| **cuEquivariance** | 加速几何感知神经网络（3D 中的旋转/平移等变性） | 分子动力学、蛋白质折叠、材料发现 |
| **NVIDIA ALCHEMI** | 用于化学与材料发现的 NIM 微服务 | 药物发现、电池材料、催化剂 |
| **cuLitho** | 计算光刻加速 | 半导体制造（L7/L8 关联） |
| **cuEST** | 在 GPU 上进行电子结构计算 | 量子化学、DFT |

---

## 物理库

GPU 加速的物理仿真——与机器人仿真、自动驾驶汽车测试和数字孪生相关。

| 库 | 功能 | 用例 |
|---------|-------------|----------|
| **NVIDIA Warp** | 用于 GPU 物理 kernel 的 Python 框架 | 仿真 AI、机器人、可微物理 |
| **NVIDIA PhysicsNeMo** | 训练与微调物理 AI 模型 | 天气预报、CFD 代理模型 |
| **NVIDIA Earth-2** | 天气与气候 AI 模型 | 气候建模、天气预报 |

---

## 量子计算库

| 库 | 功能 |
|---------|-------------|
| **cuQuantum** | 在 GPU 上加速量子电路仿真 |
| **cuPQC** | 后量子密码学加速 |
| **CUDA-Q QEC** | 量子纠错仿真 |
| **CUDA-Q Solvers** | 量子-经典混合优化 |

---

## 深度学习核心库

**直接实现神经网络推理与训练**的库。这些对 AI 硬件工程师最为关键——它们是 GPU 算力的主要消费者。

| 库 | 功能 | 层级 | 关键概念 |
|---------|-------------|:-----:|-------------|
| **cuDNN** | 神经网络原语：conv、attention、normalization、pooling、activation | L1/L2 | `cudnnConvolutionForward`、`cudnnMultiHeadAttnForward`、用于融合的 graph API；auto-tuning 会为每种硬件选择最快的算法 |
| **TensorRT** | 推理优化器 + runtime：图优化、layer 融合、精度校准、engine 序列化 | L2/L3 | 构建阶段（优化图）→ 序列化 → 部署阶段（执行）。INT8/FP8 校准、动态 shape、DLA offload |
| **[TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)** | 面向 LLM 的推理引擎：in-flight batching、paged KV-cache、张量/流水线并行、投机解码、FP8/INT4 | L2/L3 | 将 LLM 编译为采用定制 Hopper/Blackwell kernel 的优化 TRT engine。用于构建 + 部署的 Python API。与 Triton 集成以用于生产推理服务。深入内容见 [阶段 4C 第 2 部分第 05 单元](../../../阶段4-路线C-深度学习推理优化/05-推理运行时与部署/Guide.md) |
| **CUTLASS** | 面向 Tensor Core 编写定制 GEMM、conv 和 attention kernel 的 C++ 模板库 | L2/L3 | CuTe layout DSL、基于分块的编程、epilogue 融合、warp 级 MMA。生产级 GEMM kernel 结构的参考 |
| **FlashInfer** | 通过 Python API 为 LLM 推理提供优化的 attention 与 MoE kernel | L2 | Paged KV-cache attention、变长序列、支持投机解码 |

### 先学什么

1. **cuDNN** — 了解有哪些原语，以及框架（PyTorch、TensorRT）如何调用它们
2. **TensorRT** — 面向视觉/小模型的端到端推理优化（见阶段 4 方向 C 第 2 部分与方向 B §8）
3. **TensorRT-LLM** — 面向 LLM 的推理：in-flight batching、paged KV-cache、多 GPU 推理服务（见阶段 4 方向 C 第 2 部分第 05 单元）
4. **CUTLASS** — 当需要编写或修改 GEMM/attention kernel 时（见阶段 4 方向 C 第 2 部分第 02 单元）

### 项目

1. **cuDNN 算法 benchmark** — 针对某个具体 conv 层（ResNet-50 的第一个 conv），枚举所有 cuDNN 算法（`cudnnFindConvolutionForwardAlgorithm`）。比较每种算法的时间与 workspace。理解 auto-tuning 为何重要。
2. **CUTLASS 自定义 GEMM** — 构建一个带融合 bias+ReLU epilogue 的 CUTLASS GEMM。与 cuBLAS 做 benchmark 对比。研究 CuTe layout 以理解分块。
3. **FlashInfer attention** — 在 Transformer 模型上运行 FlashInfer paged attention。对比 4K、16K、64K 上下文长度下与标准 PyTorch attention 的延迟。

---


<details>
<summary>English original</summary>

**Scientific Computing Libraries**

For applications requiring neural networks that respect mathematical symmetries — molecular structures, proteins, materials science.

| Library | What it does | Domain |
|---------|-------------|--------|
| **cuEquivariance** | Accelerate geometry-aware neural networks (rotation/translation equivariance in 3D) | Molecular dynamics, protein folding, materials discovery |
| **NVIDIA ALCHEMI** | NIM microservices for chemical and materials discovery | Drug discovery, battery materials, catalysts |
| **cuLitho** | Computational lithography acceleration | Semiconductor manufacturing (L7/L8 connection) |
| **cuEST** | Electronic structure calculations on GPU | Quantum chemistry, DFT |

---

**Physics Libraries**

GPU-accelerated physics simulation — relevant for robotics simulation, autonomous vehicle testing, and digital twins.

| Library | What it does | Use case |
|---------|-------------|----------|
| **NVIDIA Warp** | Python framework for GPU physics kernels | Simulation AI, robotics, differentiable physics |
| **NVIDIA PhysicsNeMo** | Training and fine-tuning physics AI models | Weather prediction, CFD surrogate models |
| **NVIDIA Earth-2** | Weather and climate AI models | Climate modeling, weather forecasting |

---

**Quantum Computing Libraries**

| Library | What it does |
|---------|-------------|
| **cuQuantum** | Accelerate quantum circuit simulation on GPUs |
| **cuPQC** | Post-quantum cryptography acceleration |
| **CUDA-Q QEC** | Quantum error correction simulation |
| **CUDA-Q Solvers** | Hybrid quantum-classical optimization |

---

**Deep Learning Core Libraries**

The libraries that **directly implement neural network inference and training**. These are the most critical for AI hardware engineers — they are the primary consumers of GPU compute.

| Library | What it does | Layer | Key concepts |
|---------|-------------|:-----:|-------------|
| **cuDNN** | Neural network primitives: conv, attention, normalization, pooling, activation | L1/L2 | `cudnnConvolutionForward`, `cudnnMultiHeadAttnForward`, graph API for fusion; auto-tuning selects fastest algorithm per hardware |
| **TensorRT** | Inference optimizer + runtime: graph optimization, layer fusion, precision calibration, engine serialization | L2/L3 | Build phase (optimize graph) → serialize → deploy phase (execute). INT8/FP8 calibration, dynamic shapes, DLA offload |
| **[TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)** | LLM-specific inference engine: in-flight batching, paged KV-cache, tensor/pipeline parallelism, speculative decoding, FP8/INT4 | L2/L3 | Compiles LLMs to optimized TRT engines with custom Hopper/Blackwell kernels. Python API for build + deploy. Integrates with Triton for production serving. See [Phase 4C Part 2 Unit 05](../../../阶段4-路线C-深度学习推理优化/05-推理运行时与部署/Guide.md) for deep dive |
| **CUTLASS** | C++ template library for custom GEMM, conv, and attention kernels targeting Tensor Cores | L2/L3 | CuTe layout DSL, tile-based programming, epilogue fusion, warp-level MMA. The reference for how production GEMM kernels are structured |
| **FlashInfer** | Optimized attention and MoE kernels for LLM inference via Python API | L2 | Paged KV-cache attention, variable-length sequences, speculative decoding support |

**What to study first**

1. **cuDNN** — understand what primitives exist and how frameworks (PyTorch, TensorRT) call them
2. **TensorRT** — end-to-end inference optimization for vision/small models (covered in Phase 4 Track C Part 2 and Track B §8)
3. **TensorRT-LLM** — LLM-specific inference: in-flight batching, paged KV-cache, multi-GPU serving (covered in Phase 4 Track C Part 2 Unit 05)
4. **CUTLASS** — when you need to write or modify GEMM/attention kernels (covered in Phase 4 Track C Part 2, unit 02)

**Projects**

1. **cuDNN algorithm benchmark** — For a specific conv layer (ResNet-50 first conv), enumerate all cuDNN algorithms (`cudnnFindConvolutionForwardAlgorithm`). Compare time and workspace for each. Understand why auto-tuning matters.
2. **CUTLASS custom GEMM** — Build a CUTLASS GEMM with a fused bias+ReLU epilogue. Benchmark against cuBLAS. Study the CuTe layout to understand tiling.
3. **FlashInfer attention** — Run FlashInfer paged attention on a transformer model. Compare latency vs standard PyTorch attention for 4K, 16K, 64K context lengths.

---

</details>

## 并行算法库 (CCCL)

**CUDA Core Compute Libraries** —— 用 C++ 和 Python 编写 GPU 算法的基础构建块。

| 库 | 功能 | 使用场景 |
|---------|-------------|---------------|
| **Thrust** | 类似 C++ STL 的并行算法：sort、reduce、scan、transform | 编写高层并行算法，无需手写底层 CUDA kernel |
| **CUB** | warp 级、block 级、设备级协作原语 | 自定义 CUDA kernel 内部 —— block reduce、block scan、radix sort |
| **cuda.compute** | CCCL 设备级算法的 Python 接口 | 从 Python 调用 GPU 加速算法 |
| **cuda.parallel** | 标准化的 sort、scan、归约原语 | 分布式和本地并行模式 |

### 首先学什么

**CUB** —— 编写自定义 CUDA kernel 时，会频繁用到 CUB 的 `BlockReduce`、`BlockScan` 和 `WarpReduce`。它是构建块层。

### 项目

1. **Thrust 与手写 kernel 对比** —— 用 Thrust、CUB 和手写 kernel 实现并行前缀和（scan）。对三者做 benchmark。理解抽象开销。
2. **CUB 直方图** —— 用 CUB 的 `BlockHistogram` 构建 GPU 直方图。与基于 `atomicAdd` 的方法对比。

---

## 数据处理库

GPU 加速的数据流水线 —— 对足够快地喂给模型、不让 GPU 断粮至关重要。

| 库 | 功能 | 替代 |
|---------|-------------|----------|
| **cuDF** | GPU 加速的 DataFrame | pandas、Polars、Spark（零代码改动） |
| **cuVS** | GPU 向量搜索（最近邻、CAGRA 算法） | FAISS、Annoy —— 用于 RAG（检索增强生成）、推荐、语义搜索 |
| **cuML** | GPU 加速的 ML 算法 | scikit-learn、UMAP、HDBSCAN（零代码改动） |
| **cuOpt** | 决策优化引擎（数百万变量） | OR-Tools、Gurobi，用于布线/调度 |
| **cuGraph** | GPU 图分析 | NetworkX —— 用于知识图谱、社交网络 |
| **NeMo Curator** | 训练/微调 LLM 的数据流水线 | 自定义数据清洗脚本 —— 大规模文本、图像、视频 |
| **Morpheus** | 网络安全 AI 流水线 | 自定义 SIEM 分析流水线 |
| **nvComp** | GPU 加速的压缩/解压缩 | zstd、lz4 —— 用于训练数据 I/O 瓶颈 |
| **GPUDirect Storage** | 直接路径：NVMe → GPU 内存，绕过 CPU | 标准 `read()` → `cudaMemcpy()` —— 消除 bounce buffer |
| **Dask** | 带 RAPIDS GPU 支持的分布式计算框架 | 单节点 pandas/scikit-learn —— 可扩展到集群 |

### 项目

1. **cuDF 与 pandas 对比** —— 用 pandas 和 cuDF 加载 1000 万行的 CSV。对 groupby、join、filter 操作做 benchmark。测量加速比。
2. **GPUDirect Storage benchmark** —— 对比标准 I/O 与 GDS 直连 GPU 路径的模型加载时间。测量吞吐（GB/s）。

---

## 图像和视频库

硬件加速的编码/解码和视觉预处理 —— 它们为推理流水线提供输入。

| 库 | 功能 | 用例 |
|---------|-------------|----------|
| **nvImageCodec** | GPU 图像编码/解码（JPEG、PNG、WebP） | 训练用高吞吐数据加载 |
| **NVIDIA DALI** | GPU 加速的 DL 数据加载和预处理 | 训练流水线 —— 替代受限于 CPU 的 DataLoader |
| **CV-CUDA** | 视觉 AI 流水线的 GPU 前/后处理 | 生产推理：在 GPU 上做 resize、normalize、color convert |
| **cuCIM** | 面向生物医学/地理空间的加速图像处理 | 全切片成像、卫星影像 |
| **NPP（Performance Primitives）** | 2D 图像和信号处理原语 | 滤波、颜色转换、形态学操作 |
| **Video Codec SDK** | 硬件 NVDEC/NVENC 编码和解码 | 视频分析、转码、DeepStream 输入 |
| **Optical Flow SDK** | 硬件加速的光流 | 运动估计、视频稳定、帧插值 |

### 项目

1. **DALI 训练流水线** —— 用 DALI 替换 PyTorch DataLoader 做 ImageNet 训练。测量吞吐（images/sec）提升。
2. **CV-CUDA 推理预处理** —— 用 CV-CUDA 构建预处理流水线（resize + normalize + color convert）。对比 OpenCV CPU 的延迟。
3. **视频解码流水线** —— 用 NVDEC（Video Codec SDK）解码 4K 视频流 → 经 TensorRT 将帧送入检测模型。测量端到端 FPS。

---


<details>
<summary>English original</summary>

**Parallel Algorithm Libraries (CCCL)**

**CUDA Core Compute Libraries** — fundamental building blocks for writing GPU algorithms in C++ and Python.

| Library | What it does | When to use it |
|---------|-------------|---------------|
| **Thrust** | C++ STL-like parallel algorithms: sort, reduce, scan, transform | High-level parallel algorithms without writing raw CUDA kernels |
| **CUB** | Warp-wide, block-wide, and device-wide cooperative primitives | Inside custom CUDA kernels — block reduce, block scan, radix sort |
| **cuda.compute** | Python interface to CCCL device-level algorithms | GPU-accelerated algorithms from Python |
| **cuda.parallel** | Standardized sort, scan, reduction primitives | Distributed and local parallel patterns |

**What to study first**

**CUB** — if you write custom CUDA kernels, you'll use CUB's `BlockReduce`, `BlockScan`, and `WarpReduce` constantly. It's the building block layer.

**Projects**

1. **Thrust vs custom kernel** — Implement parallel prefix sum (scan) using Thrust, CUB, and a hand-written kernel. Benchmark all three. Understand the abstraction cost.
2. **CUB histogram** — Build a GPU histogram using CUB's `BlockHistogram`. Compare with `atomicAdd`-based approach.

---

**Data Processing Libraries**

GPU-accelerated data pipelines — critical for feeding models fast enough that the GPU doesn't starve.

| Library | What it does | Replaces |
|---------|-------------|----------|
| **cuDF** | GPU-accelerated DataFrames | pandas, Polars, Spark (zero code changes) |
| **cuVS** | GPU vector search (nearest neighbors, CAGRA algorithm) | FAISS, Annoy — for RAG, recommendation, semantic search |
| **cuML** | GPU-accelerated ML algorithms | scikit-learn, UMAP, HDBSCAN (zero code changes) |
| **cuOpt** | Decision optimization engine (millions of variables) | OR-Tools, Gurobi for routing/scheduling |
| **cuGraph** | GPU graph analytics | NetworkX — for knowledge graphs, social networks |
| **NeMo Curator** | Data pipeline for training/fine-tuning LLMs | Custom data cleaning scripts — text, image, video at scale |
| **Morpheus** | Cybersecurity AI pipeline | Custom SIEM analysis pipelines |
| **nvComp** | GPU-accelerated compression/decompression | zstd, lz4 — for training data I/O bottlenecks |
| **GPUDirect Storage** | Direct path: NVMe → GPU memory, bypassing CPU | Standard `read()` → `cudaMemcpy()` — eliminates bounce buffers |
| **Dask** | Distributed computing framework with RAPIDS GPU support | Single-node pandas/scikit-learn — scales to clusters |

**Projects**

1. **cuDF vs pandas** — Load a 10M-row CSV with pandas and cuDF. Benchmark groupby, join, filter operations. Measure speedup.
2. **GPUDirect Storage benchmark** — Compare model loading time with standard I/O vs GDS direct-to-GPU path. Measure throughput in GB/s.

---

**Image and Video Libraries**

Hardware-accelerated encode/decode and vision preprocessing — these feed the inference pipeline.

| Library | What it does | Use case |
|---------|-------------|----------|
| **nvImageCodec** | GPU image encode/decode (JPEG, PNG, WebP) | High-throughput data loading for training |
| **NVIDIA DALI** | GPU-accelerated data loading and preprocessing for DL | Training pipelines — replaces CPU-bound DataLoader |
| **CV-CUDA** | GPU pre/post-processing for vision AI pipelines | Production inference: resize, normalize, color convert on GPU |
| **cuCIM** | Accelerated image processing for biomedical/geospatial | Whole-slide imaging, satellite imagery |
| **NPP (Performance Primitives)** | 2D image and signal processing primitives | Filtering, color conversion, morphological operations |
| **Video Codec SDK** | Hardware NVDEC/NVENC encode and decode | Video analytics, transcoding, DeepStream input |
| **Optical Flow SDK** | Hardware-accelerated optical flow | Motion estimation, video stabilization, frame interpolation |

**Projects**

1. **DALI training pipeline** — Replace PyTorch DataLoader with DALI for ImageNet training. Measure throughput (images/sec) improvement.
2. **CV-CUDA inference preprocessing** — Build a preprocessing pipeline (resize + normalize + color convert) using CV-CUDA. Compare latency vs OpenCV CPU.
3. **Video decode pipeline** — Decode a 4K video stream using NVDEC (Video Codec SDK) → feed frames to a detection model via TensorRT. Measure end-to-end FPS.

---

</details>

## 通信库

**多 GPU 与多节点通信** —— 规模化的瓶颈。这些库决定簇训练和推理服务的速度。

| 库 | 作用 | 关键操作 | 适用场景 |
|---------|-------------|---------------|---------------|
| **NCCL** | 面向 NVIDIA 硬件优化的多 GPU/多节点集合通信 | AllReduce、AllGather、ReduceScatter、Broadcast、Send/Recv | 分布式训练（梯度同步）、模型并行、推理分片 |
| **NVSHMEM** | 跨 GPU 内存的分区全局地址空间 | `nvshmem_put`、`nvshmem_get` —— 单边远程内存访问 | 细粒度 GPU 间通信，无集合通信开销 |
| **NIXL** | 低延迟推理传输库 | KV-cache 迁移、GPU/内存层级间的张量传输 | 大语言模型推理服务 —— 为分离式推理服务搬运 KV-cache |

### 先学什么

**NCCL** —— 若从事分布式训练或多 GPU 推理，NCCL 是打交道最多的库。理解 AllReduce（数据并行）、AllGather（模型并行），以及如何让通信与计算重叠。

### 项目

1. **NCCL AllReduce benchmark** —— 在 2 块以上 GPU 上运行 `nccl-tests`。测量不同消息大小下 AllReduce 的带宽和延迟。比较 NVLink 与 PCIe 拓扑。
2. **通信-计算重叠** —— 在简单的 2 GPU 训练循环中，让梯度 AllReduce 与下一次前向传播重叠。测量相对同步 AllReduce 的吞吐提升。

---

## 合作方库

由 CUDA 提供 GPU 加速的社区与第三方库。

| 库 | 作用 |
|---------|-------------|
| **OpenCV** | 计算机视觉、图像处理、机器学习 —— GPU 加速模块 |
| **FFmpeg** | 多媒体框架 —— NVDEC/NVENC 硬件编解码集成 |
| **ArrayFire** | 高层 C++/Python GPU 数组库 |
| **CuPy** | 兼容 NumPy/SciPy 的 Python GPU 数组库 |
| **MAGMA** | 面向异构架构的 GPU 线性代数 |
| **Gunrock** | GPU 图处理库 |

---

## 按角色选择要学的 CUDA-X 库

| 你的角色 | 必须掌握 | 应当掌握 | 加分项 |
|-----------|----------|------------|-------------|
| **ML 推理优化工程师** | cuDNN、TensorRT、CUTLASS | cuBLAS、NCCL、CV-CUDA、DALI | FlashInfer、nvComp |
| **AI 编译器工程师** | CUTLASS、cuDNN（作为代码生成目标）、cuBLAS | TensorRT、FlashInfer | CUB、Thrust |
| **GPU Runtime 工程师** | cuBLAS、NCCL、NVSHMEM、GPUDirect Storage | CUB、Thrust、CUDA Math API | nvComp、NIXL |
| **GPU 基础设施 / 高性能计算工程师** | NCCL、NVSHMEM、GPUDirect Storage、Slurm | cuBLAS、cuFFT、nvComp | Dask、cuDF |
| **边缘 AI / Jetson 工程师** | TensorRT、cuDNN、VPI、DeepStream | CV-CUDA、Video Codec SDK、NPP | DALI、GStreamer |
| **AI 加速器架构师** | CUTLASS（理解硬件必须支持什么）、cuDNN | cuBLAS、TensorRT | FlashInfer、NCCL |

---

## 资源

| 资源 | URL |
|----------|-----|
| CUDA-X Library Overview | https://developer.nvidia.com/gpu-accelerated-libraries |
| CUDA Toolkit Documentation | https://docs.nvidia.com/cuda/ |
| cuBLAS Documentation | https://docs.nvidia.com/cuda/cublas/ |
| cuDNN Documentation | https://docs.nvidia.com/deeplearning/cudnn/ |
| CUTLASS GitHub | https://github.com/NVIDIA/cutlass |
| NCCL Documentation | https://docs.nvidia.com/deeplearning/nccl/ |
| TensorRT Documentation | https://docs.nvidia.com/deeplearning/tensorrt/ |
| FlashInfer Documentation | https://docs.flashinfer.ai/ |
| RAPIDS (cuDF, cuML, cuGraph) | https://rapids.ai/ |
| NVIDIA Developer Program | https://developer.nvidia.com/developer-program |


<details>
<summary>English original</summary>

**Communication Libraries**

**Multi-GPU and multi-node communication** — the bottleneck at scale. These libraries determine how fast your cluster can train and serve models.

| Library | What it does | Key operations | When to use it |
|---------|-------------|---------------|---------------|
| **NCCL** | Multi-GPU/multi-node collectives optimized for NVIDIA hardware | AllReduce, AllGather, ReduceScatter, Broadcast, Send/Recv | Distributed training (gradient sync), model parallelism, inference sharding |
| **NVSHMEM** | Partitioned global address space across GPU memories | `nvshmem_put`, `nvshmem_get` — one-sided remote memory access | Fine-grained GPU-to-GPU communication without collective overhead |
| **NIXL** | Low-latency inference transfer library | KV-cache migration, tensor transfer between GPUs/memory tiers | LLM inference serving — moving KV-cache for disaggregated serving |

**What to study first**

**NCCL** — if you work on distributed training or multi-GPU inference, NCCL is the library you'll interact with most. Understand AllReduce (data parallelism), AllGather (model parallelism), and how to overlap communication with compute.

**Projects**

1. **NCCL AllReduce benchmark** — Run `nccl-tests` on 2+ GPUs. Measure bandwidth and latency for AllReduce across different message sizes. Compare NVLink vs PCIe topology.
2. **Communication-compute overlap** — In a simple 2-GPU training loop, overlap gradient AllReduce with the next forward pass. Measure the throughput improvement vs synchronous AllReduce.

---

**Partner Libraries**

Community and third-party libraries with GPU acceleration via CUDA.

| Library | What it does |
|---------|-------------|
| **OpenCV** | Computer vision, image processing, ML — GPU-accelerated modules |
| **FFmpeg** | Multimedia framework — NVDEC/NVENC hardware codec integration |
| **ArrayFire** | High-level C++/Python GPU array library |
| **CuPy** | NumPy/SciPy-compatible GPU array library for Python |
| **MAGMA** | GPU linear algebra for heterogeneous architectures |
| **Gunrock** | GPU graph processing library |

---

**Which CUDA-X Libraries to Learn by Role**

| Your role | Must know | Should know | Nice to have |
|-----------|----------|------------|-------------|
| **ML Inference Optimization Engineer** | cuDNN, TensorRT, CUTLASS | cuBLAS, NCCL, CV-CUDA, DALI | FlashInfer, nvComp |
| **AI Compiler Engineer** | CUTLASS, cuDNN (as codegen target), cuBLAS | TensorRT, FlashInfer | CUB, Thrust |
| **GPU Runtime Engineer** | cuBLAS, NCCL, NVSHMEM, GPUDirect Storage | CUB, Thrust, CUDA Math API | nvComp, NIXL |
| **GPU Infrastructure / HPC Engineer** | NCCL, NVSHMEM, GPUDirect Storage, Slurm | cuBLAS, cuFFT, nvComp | Dask, cuDF |
| **Edge AI / Jetson Engineer** | TensorRT, cuDNN, VPI, DeepStream | CV-CUDA, Video Codec SDK, NPP | DALI, GStreamer |
| **AI Accelerator Architect** | CUTLASS (understand what hardware must support), cuDNN | cuBLAS, TensorRT | FlashInfer, NCCL |

---

**Resources**

| Resource | URL |
|----------|-----|
| CUDA-X Library Overview | https://developer.nvidia.com/gpu-accelerated-libraries |
| CUDA Toolkit Documentation | https://docs.nvidia.com/cuda/ |
| cuBLAS Documentation | https://docs.nvidia.com/cuda/cublas/ |
| cuDNN Documentation | https://docs.nvidia.com/deeplearning/cudnn/ |
| CUTLASS GitHub | https://github.com/NVIDIA/cutlass |
| NCCL Documentation | https://docs.nvidia.com/deeplearning/nccl/ |
| TensorRT Documentation | https://docs.nvidia.com/deeplearning/tensorrt/ |
| FlashInfer Documentation | https://docs.flashinfer.ai/ |
| RAPIDS (cuDF, cuML, cuGraph) | https://rapids.ai/ |
| NVIDIA Developer Program | https://developer.nvidia.com/developer-program |

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track B - High Performance Computing/CUDA-X Libraries/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20B%20-%20High%20Performance%20Computing/CUDA-X%20Libraries/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
