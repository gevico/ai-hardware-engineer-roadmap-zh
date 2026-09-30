---
title: 第 42 讲 - 机密且可验证的 AI 智能体：NVIDIA Confidential Computing、zk-STARK 远程证明与 PearlChain 栈
description: 第 42 讲 - 机密且可验证的 AI 智能体：NVIDIA Confidential Computing、zk-STARK 远程证明与 PearlChain 栈
---

# 第 42 讲 - 机密且可验证的 AI 智能体：NVIDIA Confidential Computing、zk-STARK 远程证明与 PearlChain 栈

**课程：** [AI 智能体开发 2026](../Guide.md) | **上一讲：** [第 41 讲](Lecture-41.md) | **下一讲：** [实验 01](Lab-01-Research-Agent.md)

---

企业 agent 只有在能够触及真正重要的数据时才有用——合同、账本、患者记录、供应链清单。这些正是企业无法仅凭信任就交给黑盒云端点的数据。当今的默认架构要求客户相信四个无法证明的承诺：他们的数据没有被复制，宣称的模型确实运行了，输出没有被篡改，以及某个人、某个地方保存了诚实的日志。对于受监管的银行或医院来说，“信任我们”不是一种控制。

本讲旨在将那四个承诺转化为**密码学保证**。现在已有这些工具：一个**机密计算**层，它在云运营商自己都无法读取的硬件内部运行 agent；**远程证明**，它证明运行的代码和模型正是你批准的那些；**零知识证明**（特别是 **zk-STARK**），它证明计算被正确执行而不泄露其输入；以及一个**区块链**审计层，它使记录不可篡改，并可在相互不信任的各方之间共享。将这些堆叠在一起，就产生了**可验证的 AI 智能体**——它可以在敏感数据上自主行动，同时发出隐私、正确性和可审计性的证明。

使用 NVIDIA 的机密 GPU 和基于 STARK 的验证作为具体原语，逐层构建该栈，然后将其组装成本讲所参照的参考架构——**PearlChain**，一个面向企业的机密且可验证的 agent runtime。（PearlChain 在此被视为集成平台；下面的层技术根据其公开规范描述。）

---

## 学习目标

本讲结束时，你将能够：

1. 陈述企业需要 AI 智能体提供的**四项信任保证**——机密性、计算完整性、输出完整性、可审计性——并将每项映射到具体的密码学或硬件控制。
2. 解释 **NVIDIA Confidential Computing**（H100/Blackwell）如何将可信执行环境扩展到 GPU，以及为什么“云运营商无法读取 VRAM”现在是一种硬件属性，而不是策略。
3. 描述**远程证明**（NRAS + CPU TEE）以及统领一切的原则：*预期代码 == 运行代码*，通过在度量验证之后才释放数据来强制执行。
4. 对比**基于 TEE** 的信任与 **zk-STARK / ZKML** 证明，了解为什么 STARK（透明、后量子）适合多方企业环境，并诚实地推理当今大语言模型规模的证明成本。
5. 解释什么属于**链上**（哈希、证明、审计跟踪、权限），什么属于**链下**（数据、权重、完整 trace），以及为什么区块链是审计层——永远不是推理层。
6. 将这些层组装成 **PearlChain** 近期架构和 GPU 推理 + STARK 远程证明流水线，并阐明残余威胁模型。

---


<details>
<summary>English original</summary>

**Lecture 42 - Confidential & Verifiable AI Agents: NVIDIA Confidential Computing, zk-STARK Attestation & the PearlChain Stack**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 41](Lecture-41.md) | **Next:** [Lab 01](Lab-01-Research-Agent.md)

---

An enterprise agent is only useful if it can touch the data that matters — contracts, ledgers, patient records, supply-chain manifests. That is exactly the data an enterprise cannot afford to hand to a black-box cloud endpoint on trust alone. Today's default architecture asks the customer to believe four unprovable promises: that their data was not copied, that the advertised model actually ran, that the output was not altered, and that someone, somewhere, kept an honest log. For a regulated bank or hospital, "trust us" is not a control.

This lecture is about turning those four promises into **cryptographic guarantees**. The tools exist now: a **confidential computing** layer that runs the agent inside hardware the cloud operator itself cannot read; **attestation** that proves the code and model running are the exact ones you approved; **zero-knowledge proofs** (specifically **zk-STARK**) that prove a computation was performed correctly without revealing its inputs; and a **blockchain** audit layer that makes the record immutable and shareable across mutually-distrusting parties. Stacked together they produce a **Verifiable AI Agent** — one that can act autonomously on sensitive data while emitting proof of privacy, correctness, and auditability.

We build the stack layer by layer using NVIDIA's confidential GPUs and STARK-based verification as the concrete primitives, then assemble them into the reference architecture this lecture is modeled on — **PearlChain**, a confidential-and-verifiable agent runtime for enterprises. (PearlChain is treated here as the integrating platform; the layer technologies below are described from their public specifications.)

---

**Learning Objectives**

By the end of this lecture you will be able to:

1. State the **four trust guarantees** an enterprise needs from an AI agent — confidentiality, computational integrity, output integrity, auditability — and map each to a specific cryptographic or hardware control.
2. Explain how **NVIDIA Confidential Computing** (H100/Blackwell) extends a Trusted Execution Environment to the GPU, and why "the cloud operator cannot read VRAM" is now a hardware property, not a policy.
3. Describe **remote attestation** (NRAS + a CPU TEE) and the principle that gates everything: *expected code == running code*, enforced by releasing data only after measurement verification.
4. Contrast **TEE-based** trust with **zk-STARK / ZKML** proof, know why STARKs (transparent, post-quantum) suit multi-party enterprise settings, and reason honestly about today's LLM-scale proving cost.
5. Explain what belongs **on-chain** (hashes, proofs, audit trail, permissions) versus **off-chain** (data, weights, full traces), and why blockchain is the audit layer — never the inference layer.
6. Assemble the layers into the **PearlChain** near-term architecture and the GPU-inference + STARK-attestation pipeline, and articulate the residual threat model.

---

</details>

## 1. 云端 AI 智能体的信任鸿沟

传统企业 agent 是一条单向通道，直通他人的基础设施：

```text
   Enterprise Data ──▶ Cloud LLM ──▶ Agent Decision ──▶ Action
        (plaintext)     (opaque)        (unlogged)       (unverified)
```

这一图景衍生出四个问题，每一个都是合规阻塞项，而非可有可无的润色：

| 问题 | 企业无法证明的事 | 受影响方 |
|---|---|---|
| 信任服务提供方 | RAM/VRAM 中的数据从未被读取或复制 | 数据所有者、DPO |
| 无密码学证明 | 所声称的模型/代码确实运行过 | 风险、审计 |
| 审计困难 | agent 做了什么、何时做、代表谁做 | 监管、法务 |
| 数据隐私 | prompt、检索内容与输出始终保密 | 客户、合作伙伴 |

解法是把每一项无法证明的承诺，转化为可核查的保证：

| 保证 | 含义 | 主要机制 |
|---|---|---|
| **机密性** | 无人——包括运营方——能看到使用中的数据 | 机密计算（TEE，含 GPU） |
| **计算完整性** | 约定的模型/代码未经修改地运行 | 远程证明；执行的 zk 证明 |
| **输出完整性** | 结果在传输途中未被篡改 | 签名/经证明的输出；链上承诺 |
| **可审计性** | 每个动作都被记录且不可否认 | 区块链审计日志 |

可验证 agent 的流程把这条单向通道改造成一个能产生证据的闭环：

```text
   Enterprise Data ──▶ Confidential Environment (TEE / Secure Enclave)
                              │
                              ▼
                         AI Agent  ──▶  Cryptographic Proof  ──▶  Blockchain
                                                                      │
   verify: which model · which code · when · output unaltered ◀───────┘
            …all without exposing the underlying data.
```

本讲的其余部分，就是让这个闭环成真的五个 layer。

---

## 2. Layer 1 —— 使用 NVIDIA 的机密计算

**可信执行环境（TEE）** 是一块硬件隔离的区域，其内存被加密并受完整性保护，以至于 *即使是特权软件*——hypervisor、宿主 OS、云运营方——也无法读取或修改它。CPU 在这件事上已积累多年：**Intel TDX** 与 **AMD SEV-SNP** 创建机密 VM；**Intel SGX** 创建更小的 enclave；**Google Confidential Space** 把它打包成一种服务。AI 面临的难题始终如一：真正有价值的计算发生在 **GPU** 上，而 GPU 在历史上处在信任边界 *之外*。

NVIDIA 补上了这个缺口。从 **Hopper H100** 开始，GPU 以 **机密计算（CC-On）模式** 运行，从而成为 TEE 的一等成员：

- **加密的 VRAM 与加密的总线。** H100 的 DMA 引擎用 **AES-GCM-256** 加密在 CPU 与 GPU 之间移动的数据；机密 VM 与 GPU 通过加密的 bounce buffer 交换数据。数据只在受保护的硅片内部才是明文。
- **运营方被挡在门外。** 开启 CC-On 后，宿主侧窥探 GPU 内存的路径被禁用。即便云管理员在该机器上拥有 root，也无法读取模型权重或激活值。
- **经度量的固件。** GPU 的固件与 CC 状态会被度量，因此可被远程证明（Layer 2）。

> **性能提示。** 由于 LLM 推理受算力与带宽约束，对真实的生成类工作负载而言，CC 模式的相对开销很小（通常在个位数百分比的低段）；开销集中在大量小规模 host↔device 传输上，而批处理推理已把这些传输降到最低。机密 GPU 推理在 **2026 年已是生产级原语**，不是研究演示。

**Blackwell** 一代把这一点从单块 GPU 扩展到整个 pod：**跨 NVLink/NVSwitch 的多 GPU 机密计算**（因此切分到多块 GPU 上的模型仍处于同一个信任域内）与 **Trusted I/O**，并基于 Intel TDX 与 Intel Trust Authority 构建端到端的 CPU+GPU 远程证明。NVIDIA 提供开源的 **nvTrust** 工具包与 SDK 来驱动这一切。

> **硬件视角。** 这里正是路线图中 GPU 这条线索与安全交汇之处：你为吞吐而调优的那块 H100/Blackwell（阶段 5 —— [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md))），同时也是机密性保证的根基。一台机密的 **TensorRT-LLM** 服务器，就是一台普通的高性能服务器，额外拒绝泄露它处理过的内容。

---


<details>
<summary>English original</summary>

**1. The Trust Gap in Cloud AI Agents**

The conventional enterprise agent is a one-way street into someone else's infrastructure:

```text
   Enterprise Data ──▶ Cloud LLM ──▶ Agent Decision ──▶ Action
        (plaintext)     (opaque)        (unlogged)       (unverified)
```

Four problems fall out of that picture, and each is a compliance blocker, not a nicety:

| Problem | What the enterprise cannot prove | Who is exposed |
|---|---|---|
| Trust the provider | that data in RAM/VRAM was never read or copied | data owner, DPO |
| No cryptographic proof | that the claimed model/code actually ran | risk, audit |
| Difficult auditing | what the agent did, when, on whose behalf | regulator, legal |
| Data privacy | that prompts, retrievals, and outputs stayed confidential | customers, partners |

The fix is to convert each unprovable promise into a checkable guarantee:

| Guarantee | Meaning | Primary mechanism |
|---|---|---|
| **Confidentiality** | nobody — including the operator — sees the data in use | confidential computing (TEE, incl. GPU) |
| **Computational integrity** | the agreed model/code ran, unmodified | attestation; zk proof of execution |
| **Output integrity** | the result was not tampered with in flight | signed/attested output; on-chain commitment |
| **Auditability** | every action is recorded and non-repudiable | blockchain audit log |

The verifiable-agent flow rearranges the one-way street into a loop that produces evidence:

```text
   Enterprise Data ──▶ Confidential Environment (TEE / Secure Enclave)
                              │
                              ▼
                         AI Agent  ──▶  Cryptographic Proof  ──▶  Blockchain
                                                                      │
   verify: which model · which code · when · output unaltered ◀───────┘
            …all without exposing the underlying data.
```

The rest of the lecture is the five layers that make that loop real.

---

**2. Layer 1 — Confidential Computing with NVIDIA**

A **Trusted Execution Environment (TEE)** is a hardware-isolated region whose memory is encrypted and integrity-protected such that *even privileged software* — the hypervisor, the host OS, the cloud operator — cannot read or modify it. CPUs have had this for years: **Intel TDX** and **AMD SEV-SNP** create confidential VMs; **Intel SGX** creates smaller enclaves; **Google Confidential Space** packages it as a service. The problem for AI was always the same: the interesting computation happens on the **GPU**, which historically sat *outside* the trust boundary.

NVIDIA closed that gap. Starting with the **Hopper H100**, the GPU runs in a **Confidential Computing (CC-On) mode** that makes it a first-class member of the TEE:

- **Encrypted VRAM and an encrypted bus.** The H100's DMA engine encrypts data moving between CPU and GPU with **AES-GCM-256**; the confidential VM and the GPU exchange data through encrypted bounce buffers. Data is in plaintext only inside the protected silicon.
- **The operator is locked out.** With CC-On, host-side inspection paths into GPU memory are disabled. A cloud admin with root on the box still cannot read model weights or activations.
- **Measured firmware.** The GPU's firmware and CC state are measured so they can be attested (Layer 2).

> **Performance note.** Because LLM inference is compute- and bandwidth-bound, the relative overhead of CC mode is small for realistic generation workloads (often low single-digit percent); the cost concentrates in many small host↔device transfers, which batched inference already minimizes. Confidential GPU inference is a **production primitive in 2026**, not a research demo.

The **Blackwell** generation extends this from one GPU to a pod: **multi-GPU confidential computing across NVLink/NVSwitch** (so a model sharded over many GPUs stays inside one trust domain) and **Trusted I/O**, with end-to-end CPU+GPU attestation built on Intel TDX and Intel Trust Authority. NVIDIA ships the open **nvTrust** toolkit and SDKs to drive all of this.

> **Hardware lens.** This is where the roadmap's GPU thread meets security: the same H100/Blackwell you tune for throughput (Phase 5 — [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)) is also the root of a confidentiality guarantee. A confidential **TensorRT-LLM** server is a normal high-performance server that additionally refuses to reveal what it processed.

---

</details>

## 3. Layer 2 — 远程证明：“预期代码 == 运行代码”

如果无法确定 enclave 内运行的*是什么*，机密性就毫无价值。远程证明是整个技术栈的基石：它让远端一方能够以密码学方式验证环境是真实的，且运行的正是其所批准的代码与模型——在任何秘密被交出**之前**。

企业在信任 agent 之前执行的检查：

```text
   Enterprise
       │   1. request attestation
       ▼
   Verify enclave/firmware measurement  ─┐
   Verify model hash                      ├─▶  expected == measured ?
   Verify software/agent version          ─┘        │
       │                                      yes ──┴──▶ release data + keys to the enclave
       │                                      no  ─────▶ abort; provision nothing
```

在 NVIDIA 硬件上，这项工作由 **NVIDIA Remote Attestation Service (NRAS)** 承担：

- GPU 生成一份**已签名的远程证明报告**，描述其身份、固件度量值和 CC 状态。
- 验证方依据 **NRAS JWKS** 端点校验 NVIDIA 的签名 token（JWT）；CPU 侧的远程证明（TDX/SEV-SNP，通常由 **Intel Trust Authority** 代理）覆盖机密 VM。二者合起来证明*整个*技术栈——CPU enclave + GPU。
- 关键在于，远程证明与**秘密释放**绑定：密钥管理/依赖方服务**仅在**度量值与预期黄金值匹配时，才把数据解密密钥（或数据本身）交给工作负载。替换模型或篡改 agent 二进制，度量值就会改变，检查失败，什么都不会释放。

```python
# Attestation-gated secret release (conceptual)
def provision_enterprise_data(enclave_quote, gpu_attestation):
    cpu_ok  = verify_tdx_quote(enclave_quote, expected_mrtd=GOLDEN_VM_MEASUREMENT)
    gpu_ok  = verify_nras_token(gpu_attestation, jwks=NRAS_JWKS, cc_required=True)
    model_ok = (gpu_attestation.bound_model_hash == APPROVED_MODEL_HASH)

    if not (cpu_ok and gpu_ok and model_ok):
        raise AttestationError("expected code/model != running code/model")
    return kms.release_data_key(audience=enclave_quote.identity)   # only now
```

这单一原则——*将数据密钥绑定到确切代码与模型的度量值*——正是把“信任提供商”变成“验证提供商”的关键。这是机密 AI 中最重要的概念。

---


<details>
<summary>English original</summary>

**3. Layer 2 — Attestation: "Expected Code == Running Code"**

Confidentiality is worthless if you cannot be sure *what* is running inside the enclave. Attestation is the keystone of the whole stack: it lets a remote party verify, cryptographically, that the environment is genuine and runs exactly the code and model they approved — **before** any secret is handed over.

The check an enterprise runs before trusting the agent:

```text
   Enterprise
       │   1. request attestation
       ▼
   Verify enclave/firmware measurement  ─┐
   Verify model hash                      ├─▶  expected == measured ?
   Verify software/agent version          ─┘        │
       │                                      yes ──┴──▶ release data + keys to the enclave
       │                                      no  ─────▶ abort; provision nothing
```

On NVIDIA hardware this is the job of the **NVIDIA Remote Attestation Service (NRAS)**:

- The GPU produces a **signed attestation report** describing its identity, firmware measurements, and CC state.
- A verifier validates NVIDIA's signed token (a JWT) against the **NRAS JWKS** endpoint; a CPU-side attestation (TDX/SEV-SNP, often brokered by **Intel Trust Authority**) covers the confidential VM. Together they attest the *whole* stack — CPU enclave + GPU.
- Crucially, attestation is wired to **secret release**: a key-management/relying service hands the data-decryption key (or the data itself) to the workload **only if** the measurement matches the expected golden value. Substitute the model or tamper with the agent binary and the measurement changes, the check fails, and nothing is released.

```python
# Attestation-gated secret release (conceptual)
def provision_enterprise_data(enclave_quote, gpu_attestation):
    cpu_ok  = verify_tdx_quote(enclave_quote, expected_mrtd=GOLDEN_VM_MEASUREMENT)
    gpu_ok  = verify_nras_token(gpu_attestation, jwks=NRAS_JWKS, cc_required=True)
    model_ok = (gpu_attestation.bound_model_hash == APPROVED_MODEL_HASH)

    if not (cpu_ok and gpu_ok and model_ok):
        raise AttestationError("expected code/model != running code/model")
    return kms.release_data_key(audience=enclave_quote.identity)   # only now
```

This single principle — *bind the data key to a measurement of the exact code and model* — is what turns "trust the provider" into "verify the provider." It is the most important concept in confidential AI.

---

</details>

## 4. Layer 3 — 基于 zk-STARK 的可验证执行（ZKML）

远程证明表明 *正确的程序在真实硬件上运行*。它仍然要求你信任**硬件厂商的信任根**。**零知识证明**提供了一种互补的、与硬件无关的保证：一个数学证明，表明某次计算被正确执行，任何人都可以检验，且不泄露任何关于输入的信息。

ZKML 所宣称的，正是企业所期望的：

```text
   given   input X, model M  ──▶  output Y
   produce a proof π that "Y = M(X) was computed correctly"
   while revealing  none of:  X · the weights of M · any private data
```

验证者用毫秒到秒级的时间检查 `π`，只得知对于已承诺的模型而言输出是正确的。**zk-STARK** 特别适合企业场景：

- **透明** — 无需可信设置仪式（不同于许多 SNARK）。对于一个由互不信任的多方（银行、保险公司、监管机构）组成的联盟，这一点很重要：没人需要去信任一个没人能审计的设置。
- **抗量子** — 安全性仅依赖抗碰撞哈希，而非椭圆曲线假设。
- **可扩展** — 验证相对于计算规模是**多对数级**的；证明比 SNARK 更大，但校验成本很低。

你实际会接触到的工具生态：

| 系统 | 方法 | 备注 |
|---|---|---|
| **EZKL** | ONNX → Halo2 电路 | 最常用的 ZKML 工具链；基于 SNARK |
| **RISC Zero** | 把推理编译到 RISC-V **zkVM**（STARK） | 通用；「证明任意代码」；Bonsai 证明服务 |
| **Cairo / Starknet**（例如 Orion、Giza） | STARK 原生的 ML 算子 | 在 STARK L2 上进行链上验证 |
| **Modulus Labs、Nexus、NANOZK** | ML 专用 / 逐层 LLM 证明 | NANOZK（2026）面向 LLM 推理的逐层证明 |

> **成本的真实情况（2026）。** 证明不是免费的。对于小到中等规模的模型，证明生成需要**数秒到几分钟**；对于一个 **7B 参数的 LLM**，端到端的证明仍可能耗时**数小时**。对完整的前沿 LLM 推理做密码学证明尚不现实。这是该领域最重要的一条工程事实——它也决定了下面的架构。

正因为这一成本，2026 年现实可行的设计是**混合 TEE + ZK**，而非纯 ZK：

- 把**繁重的 LLM 前向传播放在机密 GPU 内**运行（Layer 1–2）——速度快，并带有硬件远程证明。
- 对**你负担得起、且最需要公开证明的部分使用 zk-STARK 证明**：agent 的**执行 trace** 与控制流、**检索/RAG 完整性**（用对了文档）、**策略/权限检查**、较小的分类器或 routing 模型，以及**链接**输入、模型与输出的**承诺**。*optimistic TEE-rollups* 和轻量级可验证推理框架等研究路线，正是把这种划分形式化。

> **你的背景正好契合这里。** 对一次机密 GPU 运行做基于 STARK 的**执行 trace 证明**——`Agent Request → TensorRT-LLM → GPU inference (CC) → execution trace → zk-STARK proof → on-chain verification`——正是 GPU 推理、机密计算与 STARK 密码学三者交汇处的那块领域，而这套技术栈正是建立在此之上。

---

## 5. Layer 4 — 区块链作为审计与多方信任锚

一个常见的误解是推理「在链上」运行。并非如此——那会昂贵到天文数字，还会泄露数据。**区块链是审计与信任锚定层**，它只存储小型的、非敏感的承诺：

```text
   ON-CHAIN (small, public-ish)            OFF-CHAIN (large, private)
   ───────────────────────────            ──────────────────────────
   agent ID                                enterprise documents
   model hash                              vector DB / embeddings
   inference hash (input/output commit)    model weights
   attestation & zk-proof references       full execution traces
   audit trail (who, when, which policy)   raw prompts & outputs
   permissions / access grants
```

账本为你带来的东西：

- **不可篡改的日志** — 审计记录无法在事后被悄悄改写。
- **不可否认性** — 一个带签名、带时间戳的承诺意味着任何一方事后都无法否认某个行为。
- **多方信任** — 当多家组织共用一个 agent 时，**没有任何单一一方控制日志**。

最后这条性质才是真正的关键。设想一个由**银行、保险公司和监管机构**共用的理赔 agent：每一方都能独立验证模型哈希、远程证明以及任意决策所引用的证明，且没有任何一方能篡改其他方的视图。链就是中立地带。

---


<details>
<summary>English original</summary>

**4. Layer 3 — Verifiable Execution with zk-STARK (ZKML)**

Attestation proves *the right program ran in genuine hardware*. It still asks you to trust the **hardware vendor's root of trust**. **Zero-knowledge proofs** offer a complementary, hardware-independent guarantee: a mathematical proof that a computation was performed correctly, checkable by anyone, revealing nothing about the inputs.

The ZKML claim is exactly the enterprise's wish:

```text
   given   input X, model M  ──▶  output Y
   produce a proof π that "Y = M(X) was computed correctly"
   while revealing  none of:  X · the weights of M · any private data
```

A verifier checks `π` in milliseconds-to-seconds and learns only that the output is correct for the committed model. **zk-STARK** is a particularly good fit for the enterprise setting:

- **Transparent** — no trusted setup ceremony (unlike many SNARKs). For a consortium of mutually-distrusting parties (bank, insurer, regulator) this matters: nobody has to trust a setup nobody can audit.
- **Post-quantum** — security rests only on collision-resistant hashes, not on elliptic-curve assumptions.
- **Scalable** — verification is **polylogarithmic** in the size of the computation; proofs are larger than SNARKs but cheap to check.

The tooling landscape you will actually touch:

| System | Approach | Notes |
|---|---|---|
| **EZKL** | ONNX → Halo2 circuit | most-used ZKML toolchain; SNARK-based |
| **RISC Zero** | inference compiled to a RISC-V **zkVM** (STARK) | general-purpose; "prove arbitrary code"; Bonsai proving service |
| **Cairo / Starknet** (e.g. Orion, Giza) | STARK-native ML ops | on-chain verification on a STARK L2 |
| **Modulus Labs, Nexus, NANOZK** | ML-specialized / layerwise LLM proving | NANOZK (2026) targets layerwise proofs for LLM inference |

> **The honest cost reality (2026).** Proving is not free. For small-to-moderate models, proof generation runs **seconds to a few minutes**; for a **7B-parameter LLM**, an end-to-end proof can still take **hours**. Proving full frontier-LLM inference cryptographically is not yet practical. This is the single most important engineering fact in the space — and it dictates the architecture below.

Because of that cost, the realistic 2026 design is **hybrid TEE + ZK**, not pure ZK:

- Run the **heavy LLM forward pass inside the confidential GPU** (Layer 1–2) — fast, with a hardware attestation.
- Use **zk-STARK proofs for the parts you can afford and most need to prove publicly**: the agent's **execution trace** and control flow, **retrieval/RAG integrity** (the right documents were used), **policy/permission checks**, smaller classifier or routing models, and the **commitment that links** input, model, and output. Research lines like *optimistic TEE-rollups* and lightweight verifiable-inference frameworks formalize exactly this split.

> **Your background fits here.** A STARK-based **execution-trace proof** over a confidential GPU run — `Agent Request → TensorRT-LLM → GPU inference (CC) → execution trace → zk-STARK proof → on-chain verification` — is precisely the niche at the intersection of GPU inference, confidential computing, and STARK cryptography that this stack is built on.

---

**5. Layer 4 — Blockchain as the Audit & Multi-Party Trust Anchor**

A common misconception is that the inference runs "on-chain." It does not — that would be astronomically expensive and would expose the data. **Blockchain is the audit and trust-anchoring layer**, and it stores only small, non-sensitive commitments:

```text
   ON-CHAIN (small, public-ish)            OFF-CHAIN (large, private)
   ───────────────────────────            ──────────────────────────
   agent ID                                enterprise documents
   model hash                              vector DB / embeddings
   inference hash (input/output commit)    model weights
   attestation & zk-proof references       full execution traces
   audit trail (who, when, which policy)   raw prompts & outputs
   permissions / access grants
```

What the ledger buys you:

- **Immutable logs** — the audit record cannot be quietly rewritten after the fact.
- **Non-repudiation** — a signed, timestamped commitment means no party can later deny an action.
- **Multi-party trust** — when several organizations share one agent, **no single party controls the log**.

That last property is the real unlock. Picture a claims agent shared by a **bank, an insurer, and a regulator**: each can independently verify the model hash, the attestation, and the proof references for any decision, and none can tamper with the others' view. The chain is the neutral ground.

---

</details>

## 6. PearlChain 技术栈 —— 组装各层

**PearlChain** 是参考架构，把第 1–5 层串成一个机密、可验证的 agent runtime。对应关系：

| 层 | 角色 | 具体技术 |
|---|---|---|
| L1 机密计算 | 让 agent 运行在运营方看不到的地方 | NVIDIA H100/Blackwell CC、Intel TDX / AMD SEV-SNP |
| L2 远程证明 | 证明预期代码/模型 == 实际运行 | NRAS + CPU-TEE quote、由证明门控的密钥释放 |
| L3 可验证执行 | 在不泄露数据的前提下证明正确性 | 对执行 trace 做 zk-STARK（与 TEE 混合） |
| L4 区块链 | 不可篡改的多方审计 | 链上哈希、证明、审计轨迹、权限 |
| L5 Agent | 对企业数据做规划、检索、行动 | 规划 · 工具使用 · 检索 · 记忆 · 行动 |

**现实的近期**部署让数据留在本地、让链保持轻量：

```text
   Enterprise Documents ─▶ Vector DB ─▶ Confidential Agent ─▶ TEE (NVIDIA CC GPU)
                                                                    │
                                                          execution trace
                                                                    │
                                                            zk-STARK proof
                                                                    │
                                                              Audit Log ─▶ Blockchain (anchor)
```

……明确**不是**"文档 → 公共区块链 → LLM"，那既负担不起，也是一次隐私泄露。

对性能至关重要的内层流水线——把本路线图的 GPU 工作与密码学连接起来的那部分——是：

```text
   Agent Request ─▶ TensorRT-LLM ─▶ GPU inference (CC-On) ─▶ execution trace ─▶ zk-STARK proof ─▶ blockchain verification
```

在这套底座之上，agent 做的是普通的企业工作——**合同分析、财务报告、供应链优化、内部知识搜索、客户支持**——区别只在于每次运行都会留下一张可验证、保护隐私的回执。

企业现在可以**在不暴露数据的前提下验证**：*用了哪个模型、执行了哪段代码、推理发生在何时、输出是否被篡改。*

---

## 7. 威胁模型 —— 每层防住什么，还剩什么需要信任

把各层叠起来，只有在你能够精确说出每一层拦住了什么时才有用：

| 威胁 | 由谁防御 | 残留假设 |
|---|---|---|
| 运营方 / 同租户读取使用中的数据 | L1 机密计算（加密 VRAM） | 对芯片信任根的信任 |
| 代码/模型被悄悄替换 | L2 远程证明（度量门控） | 黄金度量值正确且是最新的 |
| 提供商谎报它算了什么 | L3 执行的 zk-STARK 证明 | 被证明的范围（混合模式：仅被证明的部分） |
| 输出在传输中被改动 / 日志被改写 | L4 链上承诺 + 签名 | 链的活性与密钥保管 |
| 某个联盟成员伪造记录 | L4 多方账本 | 法定人数 / 共识的完整性 |

两条诚实的告诫，避免这一切沦为安全表演：

- **TEE 减少信任，但不消除信任。** 你仍然要信任硬件厂商的信任根，以及不存在侧信道。zk-STARK **只对你真正证明的那部分**去掉这一假设——而今天这并不是整个 LLM。要把边界讲清楚。
- **机密性 ≠ agent 安全。** 这些层没有一层能拦住**经由工具结果的 prompt injection**，也拦不住**致命三元组**（私有数据 + 不可信内容 + 外泄通道）。一个机密的、经过远程证明的、链上留痕的 agent，仍然可能被社工诱导而滥用其访问权限。来自 [Lecture 24 — Runtime Discipline](Lecture-24.md)、[Lecture 40 — OpenClaw Threat Model](Lecture-40.md) 和 [MCP Security (MCP course · Lecture 07)](../02-AI智能体MCP/Lecture-07.md) 的 agent 级控制仍然必需。机密计算保护数据*不被运营方看到*；它不保护 agent *不受输入影响*。

---


<details>
<summary>English original</summary>

**6. The PearlChain Stack — Assembling the Layers**

**PearlChain** is the reference architecture that ties Layers 1–5 into one confidential, verifiable agent runtime. The mapping:

| Layer | Role | Concrete tech |
|---|---|---|
| L1 Confidential computing | run the agent where the operator can't look | NVIDIA H100/Blackwell CC, Intel TDX / AMD SEV-SNP |
| L2 Attestation | prove expected code/model == running | NRAS + CPU-TEE quote, attestation-gated key release |
| L3 Verifiable execution | prove correctness without revealing data | zk-STARK over the execution trace (hybrid with TEE) |
| L4 Blockchain | immutable, multi-party audit | on-chain hashes, proofs, audit trail, permissions |
| L5 Agent | plan, retrieve, act on enterprise data | planning · tool use · retrieval · memory · action |

The **realistic near-term** deployment keeps data local and the chain thin:

```text
   Enterprise Documents ─▶ Vector DB ─▶ Confidential Agent ─▶ TEE (NVIDIA CC GPU)
                                                                    │
                                                          execution trace
                                                                    │
                                                            zk-STARK proof
                                                                    │
                                                              Audit Log ─▶ Blockchain (anchor)
```

…explicitly **not** "documents → public blockchain → LLM," which would be both unaffordable and a privacy breach.

The performance-critical inner pipeline — the part that connects this roadmap's GPU work to the crypto — is:

```text
   Agent Request ─▶ TensorRT-LLM ─▶ GPU inference (CC-On) ─▶ execution trace ─▶ zk-STARK proof ─▶ blockchain verification
```

On this substrate the agent does ordinary enterprise work — **contract analysis, financial reporting, supply-chain optimization, internal knowledge search, customer support** — except that every run leaves behind a verifiable, privacy-preserving receipt.

What the enterprise can now **verify without ever exposing the data**: *which model was used, which code executed, when inference happened, and whether the output was altered.*

---

**7. Threat Model — What Each Layer Defends, What Remains Trusted**

Stacking the layers only helps if you can say precisely what each one stops:

| Threat | Defended by | Residual assumption |
|---|---|---|
| Operator / co-tenant reads data in use | L1 confidential computing (encrypted VRAM) | trust in the silicon root of trust |
| Code/model silently swapped | L2 attestation (measurement gate) | golden measurements are correct & current |
| Provider lies about what it computed | L3 zk-STARK proof of execution | proven scope (hybrid: only proven parts) |
| Output altered in transit / log rewritten | L4 on-chain commitment + signatures | chain liveness & key custody |
| One consortium member forges the record | L4 multi-party ledger | quorum / consensus integrity |

Two honest caveats keep this from becoming security theater:

- **TEEs reduce trust; they do not eliminate it.** You still trust the hardware vendor's root of trust and the absence of side channels. zk-STARK removes that assumption **for the parts you actually prove** — which today is not the whole LLM. Be explicit about the boundary.
- **Confidentiality ≠ agent safety.** None of these layers stop a **prompt injection via tool results** or the **lethal trifecta** (private data + untrusted content + an exfiltration channel). A confidential, attested, on-chain-logged agent can still be socially engineered into misusing its access. The agent-level controls from [Lecture 24 — Runtime Discipline](Lecture-24.md), [Lecture 40 — OpenClaw Threat Model](Lecture-40.md), and [MCP Security (MCP course · Lecture 07)](../02-AI智能体MCP/Lecture-07.md) are still mandatory. Confidential computing protects the data *from the operator*; it does not protect the agent *from the inputs*.

---

</details>

## 核心要点

- 企业需要 AI 智能体提供四项**可证明**属性——机密性、计算完整性、输出完整性、可审计性——每一项都对应一个具体控制手段，而非一句承诺。
- **NVIDIA Confidential Computing**（H100 加密 VRAM + AES-GCM-256 DMA；Blackwell 多 GPU CC）把「云运营方无法读取你的数据或权重」变成硬件保证，而 LLM 推理的开销很小。
- **远程证明是基石：** 把数据解密密钥绑定到对确切代码与模型的度量（NRAS + CPU-TEE），因此只有当 *预期值 == 实际运行值* 时才释放密钥。
- **zk-STARK / ZKML** 增加了与硬件无关的正确执行证明——透明且后量子，适合多方信任——但**完整的 LLM 证明仍需数小时**，因此 2026 年务实的设计是 **TEE + ZK 混合**。
- **区块链只存承诺，绝不存推理：** 哈希、证明、审计轨迹、权限——提供不可篡改、不可否认、多方共享的日志。
- **PearlChain** 技术栈把这些组装成一个 Verifiable AI Agent；面向 GPU 的流水线是 `TensorRT-LLM → CC inference → execution trace → zk-STARK proof → on-chain verification`。
- 机密 ≠ 安全：保留安全课程中 agent 级的注入/外泄防御。

---

## 练习

### 练习 1 —— 映射各项保证

选一个企业任务（例如机密合同分析）。对四项保证（机密性、计算完整性、输出完整性、可审计性）中的每一项，指出提供它的确切 layer 与机制，以及仍残留的那一条信任假设。以审计人员能读懂的表格形式给出。

### 练习 2 —— 远程证明闸门

为一个由远程证明把关的推理服务编写伪代码：客户端发送加密提示；服务器必须 (a) 出示 GPU + CPU 远程证明，(b) 证明所绑定的模型哈希等于已批准值，然后 (c) 才收到解密密钥。明确指出如果攻击者替换模型，究竟什么会失败——以及什么*不会*被泄漏。

### 练习 3 —— 划定混合边界

对于一个基于私密文档作答的 RAG agent，已知 7B-LLM 的证明要花数小时，判断哪些部分放进 TEE 运行、哪些用 zk-STARK 证明。按成本以及每个利益相关方最需要验证的内容来论证这一划分。然后说明你会为每个请求在链上提交什么。

### 练习 4 —— 为什么不在链上推理？

用两段话向非技术高管解释：为什么 agent 的 LLM 推理**不**在区块链上运行，什么*才*会上链，以及企业如何仍从这种安排中获得密码学可审计性。

---

## 截至

**2026 年 6 月。** NVIDIA Confidential Computing 已在 Hopper（H100）上量产，并在 Blackwell 上扩展到多 GPU（HGX B200、NVLink/NVSwitch、Trusted I/O）；远程证明通过 **NRAS** + CPU TEE（Intel TDX / AMD SEV-SNP，由 Intel Trust Authority 代理）完成。ZKML 工具链（EZKL、RISC Zero、Cairo/Starknet、NANOZK）推进很快，但**证明完整的 LLM 推理仍然昂贵（7B 模型需数小时）**，因此混合 **TEE + zk-STARK** 是当今现实可行的企业架构——在定下设计前先核实当前的证明成本。**PearlChain** 在此作为集成参考平台介绍；其各 layer 技术依据公开规范描述，该平台的具体实现细节应以其自身文档为准。所有厂商/规范事实变动都很快——请把这些数字当作锚点，而非常量。

---

*下一节：[Lab 01](Lab-01-Research-Agent.md)*


<details>
<summary>English original</summary>

**Key Takeaways**

- Enterprises need four **provable** properties from an AI agent — confidentiality, computational integrity, output integrity, auditability — and each maps to a specific control, not a promise.
- **NVIDIA Confidential Computing** (H100 encrypted VRAM + AES-GCM-256 DMA; Blackwell multi-GPU CC) makes "the cloud operator cannot read your data or weights" a hardware guarantee, at small overhead for LLM inference.
- **Attestation is the keystone:** bind the data-decryption key to a measurement of the exact code and model (NRAS + CPU-TEE), so secrets are released only when *expected == running*.
- **zk-STARK / ZKML** adds a hardware-independent proof of correct execution — transparent and post-quantum, ideal for multi-party trust — but **full-LLM proving still costs hours**, so the practical 2026 design is **hybrid TEE + ZK**.
- **Blockchain stores commitments, never inference:** hashes, proofs, audit trail, permissions — delivering immutable, non-repudiable, multi-party-shareable logs.
- The **PearlChain** stack assembles these into a Verifiable AI Agent; the GPU-facing pipeline is `TensorRT-LLM → CC inference → execution trace → zk-STARK proof → on-chain verification`.
- Confidential ≠ safe: keep the agent-level injection/exfiltration defenses from the security lectures.

---

**Exercises**

**Exercise 1 — Map the Guarantees**

Take one enterprise task (e.g., confidential contract analysis). For each of the four guarantees (confidentiality, computational integrity, output integrity, auditability), name the exact layer and mechanism that delivers it, and the one residual trust assumption that remains. Produce it as a table an auditor could read.

**Exercise 2 — Attestation Gate**

Write pseudocode for an attestation-gated inference service: the client sends an encrypted prompt; the server must (a) present a GPU + CPU attestation, (b) prove the bound model hash equals an approved value, and (c) only then receive the decryption key. Specify exactly what fails — and what is *not* leaked — if an attacker swaps the model.

**Exercise 3 — Draw the Hybrid Boundary**

For a RAG agent answering on private documents, decide which parts you would run in a TEE and which you would prove with zk-STARK, given that a 7B-LLM proof costs hours. Justify the split by cost and by what each stakeholder most needs to verify. Then state what you would commit on-chain for each request.

**Exercise 4 — Why Not On-Chain Inference?**

In two paragraphs, explain to a non-technical executive why the agent's LLM inference does **not** run on the blockchain, what *does* go on-chain, and how the enterprise still gets cryptographic auditability from that arrangement.

---

**Current as of**

**June 2026.** NVIDIA Confidential Computing is production on Hopper (H100) and extends to multi-GPU on Blackwell (HGX B200, NVLink/NVSwitch, Trusted I/O); attestation via **NRAS** + a CPU TEE (Intel TDX / AMD SEV-SNP, brokered by Intel Trust Authority). ZKML tooling (EZKL, RISC Zero, Cairo/Starknet, NANOZK) is advancing fast, but **proving full LLM inference remains expensive (hours for a 7B model)**, so hybrid **TEE + zk-STARK** is the realistic enterprise architecture today — verify current proving costs before committing a design. **PearlChain** is presented here as the integrating reference platform; its layer technologies are described from their public specifications, and the platform's specific implementation details should be confirmed against its own documentation. All vendor/spec facts move quickly — treat the numbers as anchors, not constants.

---

*Next: [Lab 01](Lab-01-Research-Agent.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-42.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-42.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
