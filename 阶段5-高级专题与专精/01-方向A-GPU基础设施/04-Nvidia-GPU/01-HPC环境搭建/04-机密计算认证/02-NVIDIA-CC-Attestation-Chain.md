---
title: 02 — NVIDIA CC 远程证明链
description: 02 — NVIDIA CC 远程证明链
---

# 02 — NVIDIA CC 远程证明链

## 1. CC-On 模式回顾

本 HPC 方向涉及的 GPU —— H100（[8x H200 Training & Inference](../01-8x-H200训练推理/README.md) 的 GH100 die、[Blackwell B200](../02-Blackwell-B200-Qwen推理/README.md) —— 支持一种 **Confidential Computing (CC-On)** 模式，它把 CPU 侧的 TEE 从 [file 01](01-Intel-TDX-Attestation-Chain.md) 一路延伸到加速器上。三点特性使其成为可能：

- **加密的 VRAM 与加密总线。** GPU 的 DMA 引擎用 AES-GCM-256 加密在 CPU 与 GPU 之间搬运的数据；机密 VM 与 GPU 通过加密的 bounce buffer 交换数据。明文只存在于受保护的硅片内部。
- **运维方被挡在门外。** 启用 CC-On 后，主机侧深入 GPU 内存的检查路径被禁用 —— 即便拥有机器 root 的云管理员，也无法读取驻留在 VRAM 中的模型权重、激活值或训练数据。
- **固件被度量。** GPU 的固件与 CC 状态在启动时被度量，这正是本文件其余内容得以成立的基础：这些度量值会汇入一份签名的证明报告。

Blackwell 把它从单块 GPU 扩展到整个 pod：跨 NVLink/NVSwitch 的多 GPU 机密计算（因而切分到多块 GPU 上的模型 —— 正是 [Multi-B200 NVL72](../02-Blackwell-B200-Qwen推理/04-Multi-B200-NVL72.md) 拓扑 —— 始终处于同一个信任域内），外加基于 Intel TDX 构建、带端到端 CPU+GPU 远程证明的 **Trusted I/O**。NVIDIA 提供开源 **nvTrust** 工具链来驱动这一切。

## 2. GPU 证明报告：EAT

一块 CC-On GPU 会生成一份签名的 **Entity Attestation Token (EAT)** —— 一个描述其身份、固件度量值与 CC 状态的 JWT。实践中，一轮验证会产生**不止一个 token**：每块物理 GPU 设备一个，再加一个覆盖整个平台的 token，因为多 GPU 节点包含多块硅片，每块都需要各自的度量值。

```text
GPU 0 EAT (JWT)   ─┐
GPU 1 EAT (JWT)   ─┤── all signed independently by NRAS, verified independently
Platform EAT (JWT) ─┘
```

## 3. 绑定声明：`eat_nonce`

正如 Intel TDX 把 REPORTDATA 留空给应用数据使用（[file 01, §5](01-Intel-TDX-Attestation-Chain.md)），NVIDIA EAT 格式也有一个等价的槽位：`eat_nonce`。应用想要被证明的任何 hash 都在请求 token 之前写入该处：

```text
application_hash ──▶ eat_nonce field ──▶ GPU attestation request ──▶ signed EAT (JWT)
```

验证方的绑定检查与 TDX 的那套如出一辙，并且有完全相同的局限：

```python
def check_gpu_binding(claim_hash, gpu_eat):
    return gpu_eat.eat_nonce == claim_hash    # binding only — see file 03 for why this alone is forgeable
```

## 4. 真实性：NRAS 签名 + JWKS 密钥解析

`eat_bound: true` 只证明*某个* JWT 携带了正确的 nonce —— 仍未说明它由谁签名。**NVIDIA Remote Attestation Service (NRAS)** 填补了这一缺口：验证方从 NVIDIA 公布的 JWKS 端点解析出真正的签名密钥，并据此校验签名，而不是依据证明方提供的任何东西。

```text
   EAT (JWT)
      │
      ├─ header.kid ──▶ resolve signing key from NVIDIA's published JWKS endpoint
      ├─ signature   ──▶ ES384 verify against the resolved key
      ├─ iss         ──▶ must equal https://nras.attestation.nvidia.com
      └─ exp         ──▶ not expired
```

```python
def verify_gpu_token(eat_jwt, jwks):
    header = jwt.get_unverified_header(eat_jwt)
    key = jwks.resolve(header["kid"])
    claims = jwt.decode(eat_jwt, key=key, algorithms=["ES384"])
    assert claims["iss"] == "https://nras.attestation.nvidia.com"
    assert claims["exp"] > now()
    return claims
```

JWT 头部的 `kid`（key ID）让验证方能够从一个随时间轮换密钥的 JWKS 端点取到*正确*的密钥 —— 绝不要硬编码单个预期密钥；应逐 token 解析。

## 5. 对称密钥自证陷阱

GPU 证明 SDK 通常还会返回一个额外的便利字段：一个 "overall" JWT，它**用 SDK 自己持有的密钥做 HS256 签名**。这是 GPU 证明验证代码中最常见的错误，因为它同时也是最容易够到的字段：

```python
# WRONG — this only proves the SDK agrees with itself
jwt.decode(overall_token, key=sdk_local_secret, algorithms=["HS256"])
```

由生成证据的同一进程所控制的对称密钥，不能充当第三方证明 —— 验证它只能确认内部一致性，并不能说明 NVIDIA 的硬件为任何东西背书。唯一可算作证据的 token，是 §4 中那些 **NRAS 签名、ES384、非对称**的逐设备与平台 EAT。正确的验证方会把 SDK 本地的 HS256 token 明确排除在任何作为签名检查上报的结果之外，尽管直接取 `overall.passed` 是最省事的路径。


<details>
<summary>English original</summary>

**02 — NVIDIA CC Attestation Chain**

**1. CC-On Mode Recap**

The GPUs in this HPC track — H100 ([8x H200 Training & Inference](../01-8x-H200训练推理/README.md)'s GH100 die, [Blackwell B200](../02-Blackwell-B200-Qwen推理/README.md) — support a **Confidential Computing (CC-On)** mode that extends the CPU-side TEE from [file 01](01-Intel-TDX-Attestation-Chain.md) all the way onto the accelerator. Three properties make this possible:

- **Encrypted VRAM and an encrypted bus.** The GPU's DMA engine encrypts data moving between CPU and GPU with AES-GCM-256; the confidential VM and GPU exchange data through encrypted bounce buffers. Plaintext exists only inside the protected silicon.
- **The operator is locked out.** With CC-On enabled, host-side inspection paths into GPU memory are disabled — a cloud admin with root on the box still cannot read model weights, activations, or training data resident in VRAM.
- **Measured firmware.** The GPU's firmware and CC state are measured at boot, which is what makes the rest of this file possible: those measurements feed into a signed attestation report.

Blackwell extends this from a single GPU to a pod: multi-GPU confidential computing across NVLink/NVSwitch (so a model sharded across many GPUs — exactly the [Multi-B200 NVL72](../02-Blackwell-B200-Qwen推理/04-Multi-B200-NVL72.md) topology — stays inside one trust domain), plus **Trusted I/O** with end-to-end CPU+GPU attestation built on Intel TDX. NVIDIA ships the open **nvTrust** toolkit to drive this.

**2. The GPU Attestation Report: EAT**

A CC-On GPU produces a signed **Entity Attestation Token (EAT)** — a JWT describing its identity, firmware measurements, and CC state. In practice a verification round produces **more than one token**: one per physical GPU device plus one covering the platform as a whole, since a multi-GPU node has multiple pieces of silicon each needing their own measurement.

```text
GPU 0 EAT (JWT)   ─┐
GPU 1 EAT (JWT)   ─┤── all signed independently by NRAS, verified independently
Platform EAT (JWT) ─┘
```

**3. Binding a Claim: `eat_nonce`**

Just as Intel TDX leaves REPORTDATA free for application data ([file 01, §5](01-Intel-TDX-Attestation-Chain.md)), the NVIDIA EAT format has an equivalent slot: `eat_nonce`. Whatever hash the application wants attested goes there before requesting the token:

```text
application_hash ──▶ eat_nonce field ──▶ GPU attestation request ──▶ signed EAT (JWT)
```

A verifier's binding check mirrors the TDX one, and has the exact same limitation:

```python
def check_gpu_binding(claim_hash, gpu_eat):
    return gpu_eat.eat_nonce == claim_hash    # binding only — see file 03 for why this alone is forgeable
```

**4. Authenticity: NRAS Signature + JWKS Key Resolution**

`eat_bound: true` only proves *some* JWT carries the right nonce — nothing yet says who signed it. The **NVIDIA Remote Attestation Service (NRAS)** closes that gap: a verifier resolves the actual signing key from NVIDIA's published JWKS endpoint and checks the signature against it, not against anything the prover supplied.

```text
   EAT (JWT)
      │
      ├─ header.kid ──▶ resolve signing key from NVIDIA's published JWKS endpoint
      ├─ signature   ──▶ ES384 verify against the resolved key
      ├─ iss         ──▶ must equal https://nras.attestation.nvidia.com
      └─ exp         ──▶ not expired
```

```python
def verify_gpu_token(eat_jwt, jwks):
    header = jwt.get_unverified_header(eat_jwt)
    key = jwks.resolve(header["kid"])
    claims = jwt.decode(eat_jwt, key=key, algorithms=["ES384"])
    assert claims["iss"] == "https://nras.attestation.nvidia.com"
    assert claims["exp"] > now()
    return claims
```

The `kid` (key ID) in the JWT header is what lets the verifier fetch the *correct* key from a JWKS endpoint that rotates keys over time — never hardcode a single expected key; resolve it per-token.

**5. The Symmetric-Key Self-Attestation Trap**

GPU attestation SDKs commonly hand back an additional convenience field: an "overall" JWT that's **HS256-signed with a key the SDK itself holds**. This is the single most common mistake in GPU-attestation verifier code, because it's also the easiest field to reach for:

```python
# WRONG — this only proves the SDK agrees with itself
jwt.decode(overall_token, key=sdk_local_secret, algorithms=["HS256"])
```

A symmetric key controlled by the same process generating the evidence cannot serve as third-party proof — verifying it just confirms internal consistency, not that NVIDIA's hardware vouches for anything. The only tokens that count as evidence are the **NRAS-signed, ES384, asymmetric** per-device and platform EATs from §4. A correct verifier explicitly excludes the SDK-local HS256 token from anything it reports as a signature check, even though pulling `overall.passed` is the path of least resistance.

</details>

## 6. Composed Verifier Sketch

把 §3–§5 合到一起，单次 GPU 断言校验如下所示：

```python
def verify_gpu_claim(claim_hash, gpu_eat_tokens, jwks):
    results = []
    for eat_jwt in gpu_eat_tokens:          # per-device + platform, NRAS-signed only
        claims = verify_gpu_token(eat_jwt, jwks)          # §4 — authenticity
        bound = (claims["eat_nonce"] == claim_hash)        # §3 — binding
        results.append({"bound": bound, "authentic": True})
    return all(r["bound"] and r["authentic"] for r in results)
```

注意，**每个** token 上两项检查都是必需的，而不只是某一个代表性 token——多 GPU 断言的可信度，取决于其中最弱的、经校验的设备。


<details>
<summary>English original</summary>

**6. Composed Verifier Sketch**

Putting §3–§5 together, a single GPU claim check looks like:

```python
def verify_gpu_claim(claim_hash, gpu_eat_tokens, jwks):
    results = []
    for eat_jwt in gpu_eat_tokens:          # per-device + platform, NRAS-signed only
        claims = verify_gpu_token(eat_jwt, jwks)          # §4 — authenticity
        bound = (claims["eat_nonce"] == claim_hash)        # §3 — binding
        results.append({"bound": bound, "authentic": True})
    return all(r["bound"] and r["authentic"] for r in results)
```

Note both checks are required on **every** token, not just one representative token — a multi-GPU claim is only as strong as its weakest verified device.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Confidential-Computing-Attestation/02-NVIDIA-CC-Attestation-Chain.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Confidential-Computing-Attestation/02-NVIDIA-CC-Attestation-Chain.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
