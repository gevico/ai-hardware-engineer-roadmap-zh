---
title: 03 — 双供应商声明绑定与验证审查
description: 03 — 双供应商声明绑定与验证审查
---

# 03 — 双供应商声明绑定与验证审查

## 1. 本文件解决的问题

面向远程或租用算力任务的验证者需要两条路径：

```text
   Slow path:  recipe + dataset ──▶ re-execute from source ──▶ compare to claim   (expensive, always correct)
   Fast path:  claim + attestation bundle ──▶ verify cryptographically             (cheap, only as strong as the attestation)
```

只有当声明无法被伪造时，快速路径才成立。定义一个单一摘要，承诺该任务所有关键信息：

```text
claim_sha256 = sha256(result_scores ‖ job_metadata ‖ output_manifest)
```

`output_manifest` —— 对输出产物（检查点、结果）逐文件计算的 SHA-256 —— 让声明能够承诺输出的*身份*，而无需验证者下载数 GB 的载荷。文件 [01](01-Intel-TDX-Attestation-Chain.md) 与 [02](02-NVIDIA-CC-Attestation-Chain.md) 各自展示了如何把该哈希绑定进某一个硬件供应商的签名报告（Intel TDX 用 `REPORTDATA`，NVIDIA CC 用 `eat_nonce`），并把该报告认证回供应商的可信根。本文件将两者组合起来，并推广其底层的审查技能。

## 2. 组合两条链

一个 `claim_sha256` 独立锚定到两个硬件根中：

```text
                                    claim_sha256
                        (result_scores ‖ job_metadata ‖ output_manifest)
                                          │
                       ┌───────────────────┴───────────────────┐
                       ▼                                       ▼
              eat_nonce (NVIDIA CC)                REPORTDATA (Intel TDX, zero-padded)
                       │                                       │
        gpu_signature: NRAS JWKS,                  tdx_signature: DCAP —
        ES384, kid-resolved,                        ECDSA sig, PCK → Intel root CA,
        iss + exp checked                           QE identity, live TCB status
                       │                                       │
                       └───────────────────┬───────────────────┘
                                            ▼
                     claim is bound to a genuine GPU AND
                     a genuine, measured TDX VM — independently
```

验证者对单个声明的输出，应把每一项检查暴露为独立字段 —— 绝不把它们合并成一个布尔值：

```text
claim_bound:            true              # claim digest found in eat_nonce
gpu_signature:           true, 2 tokens    # NVIDIA JWKS, ES384, per-device + platform
tdx_bound:               true              # claim digest found in REPORTDATA
tdx_signature:           true, UpToDate    # Intel DCAP / PCS
output_hash_match:       true              # claim manifest matches submitted output files
```

现在伪造它需要**同时**攻破 Intel 与 NVIDIA 的签名基础设施 —— 这与针对单供应商检查写出一段可信 JSON 的门槛有实质差别。

## 3. 绑定与真实性 —— 核心审查准则

真实双供应商远程证明落地中的每一处缺口，往往都是同一个模式的重复：**字段先于其签名被检查。** 这完全超越了 TDX 与 NVIDIA CC —— 这正是*任何*远程证明代码审查中要找的东西（SGX enclave、TPM 支撑的启动链、其他供应商的 CC 方案）：

| 代码中的症状 | 它实际检查的东西 | 缺失的东西 | 具体伪造手法 |
|---|---|---|---|
| `if quote["reportdata"] == claim_hash` | 仅做绑定 | 签名/链验证 | 手写 JSON，在 `reportdata` 中放入正确字节；根本没有真正的 TDX Module 运行过 |
| `if attestation["passed"] == True` | 信任自报的标志位 | 谁计算了 `passed` 以及如何计算 | 伪造 JSON；没有任何机制强制 `passed` 反映真实检查 |
| `jwt.decode(token, verify=False)`，或只检查 `HS256` | JWT 格式是否合法 | *正确类型*的签名（非对称、由供应商持有密钥） | 用自己生成的密钥给自己的 token 签名；没有任何东西把它与 NVIDIA/Intel 绑在一起 |
| 只有布尔值的 TCB/状态输出 | 通过/失败 | 实际风险等级（公告 ID、陈旧程度） | 不是伪造，但验证者失去了应用自身策略的能力 —— 无声的风险洗白 |

在审查或设计任何以远程证明为门控的系统时，按顺序问同样三个问题：

1. **绑定进来的是什么值？**（声明哈希、nonce、公钥）
2. **承载该值的容器由谁签名？**（证明者自身，还是硬件供应商的密钥？）
3. **该签名是否链接到一个你真正信任的根，且是实时检查而非缓存？**

如果 (2) 或 (3) 的答案是“没有任何东西验证这一点”，那么 (1) 中的绑定只是装饰性的 —— 它会通过一次只读到绑定检查就停下的审查。


<details>
<summary>English original</summary>

**03 — Dual-Vendor Claim Binding & Verification Review**

**1. The Problem This Solves**

A verifier for a remote or rented compute job wants two paths:

```text
   Slow path:  recipe + dataset ──▶ re-execute from source ──▶ compare to claim   (expensive, always correct)
   Fast path:  claim + attestation bundle ──▶ verify cryptographically             (cheap, only as strong as the attestation)
```

The fast path only works if the claim can't be forged. Define a single digest that commits to everything about the job that matters:

```text
claim_sha256 = sha256(result_scores ‖ job_metadata ‖ output_manifest)
```

`output_manifest` — a per-file SHA-256 of output artifacts (checkpoints, results) — lets the claim commit to output *identity* without the verifier downloading multi-gigabyte payloads. Files [01](01-Intel-TDX-Attestation-Chain.md) and [02](02-NVIDIA-CC-Attestation-Chain.md) each showed how to bind this hash into one hardware vendor's signed report (`REPORTDATA` for Intel TDX, `eat_nonce` for NVIDIA CC) and authenticate that report back to the vendor's root of trust. This file composes both and generalizes the underlying review skill.

**2. Composing the Two Chains**

One `claim_sha256` anchored independently into both hardware roots:

```text
                                    claim_sha256
                        (result_scores ‖ job_metadata ‖ output_manifest)
                                          │
                       ┌───────────────────┴───────────────────┐
                       ▼                                       ▼
              eat_nonce (NVIDIA CC)                REPORTDATA (Intel TDX, zero-padded)
                       │                                       │
        gpu_signature: NRAS JWKS,                  tdx_signature: DCAP —
        ES384, kid-resolved,                        ECDSA sig, PCK → Intel root CA,
        iss + exp checked                           QE identity, live TCB status
                       │                                       │
                       └───────────────────┬───────────────────┘
                                            ▼
                     claim is bound to a genuine GPU AND
                     a genuine, measured TDX VM — independently
```

A verifier's output for one claim should surface every check as a distinct field — never collapse them into one boolean:

```text
claim_bound:            true              # claim digest found in eat_nonce
gpu_signature:           true, 2 tokens    # NVIDIA JWKS, ES384, per-device + platform
tdx_bound:               true              # claim digest found in REPORTDATA
tdx_signature:           true, UpToDate    # Intel DCAP / PCS
output_hash_match:       true              # claim manifest matches submitted output files
```

Forging this now requires compromising **both** Intel's and NVIDIA's signing infrastructure simultaneously — a materially different bar than writing convincing JSON against a single-vendor check.

**3. Binding vs. Authenticity — The Core Review Discipline**

Every gap in a real dual-vendor attestation rollout tends to be the same pattern, repeated: **a field got checked before its signature did.** This generalizes past TDX and NVIDIA CC entirely — it's what to look for in *any* attestation code review (an SGX enclave, a TPM-backed boot chain, a different vendor's CC offering):

| Symptom in code | What it actually checks | What it's missing | Concrete forgery |
|---|---|---|---|
| `if quote["reportdata"] == claim_hash` | binding only | signature/chain validation | Hand-write JSON with the right bytes in `reportdata`; no real TDX Module ever ran |
| `if attestation["passed"] == True` | trusting a self-reported flag | who computed `passed` and how | Fabricate the JSON; nothing forces `passed` to reflect a real check |
| `jwt.decode(token, verify=False)` or checking only `HS256` | JWT well-formedness | the *right kind* of signature (asymmetric, vendor-held key) | Sign your own token with a key you generated; nothing ties it to NVIDIA/Intel |
| Boolean-only TCB/status output | pass/fail | the actual risk level (advisory IDs, staleness) | Not a forgery, but a validator loses the ability to apply its own policy — silent risk-laundering |

When reviewing or designing any attestation-gated system, ask the same three questions in order:

1. **What value is being bound in?** (a claim hash, a nonce, a public key)
2. **Who signed the container that value lives in?** (the prover itself, or a hardware vendor's key?)
3. **Does that signature chain to a root you actually trust, checked live rather than cached?**

If the answer to (2) or (3) is "nothing verifies that," the binding in (1) is decorative — it will pass a review that only reads the binding check and stops.

</details>

## 4. 威胁模型 —— 什么已被证明，什么仍需信任

| 威胁 | 防御手段 | 残余假设 |
|---|---|---|
| 不涉及真实硬件的伪造 claim JSON | 仅靠绑定检查 | **没有 —— 这正是缺口，而非防御；还需要文件 01 §4 / 02 §4 中的真实性检查** |
| 带正确 REPORTDATA 字节的伪造 TD Quote | DCAP 签名 + PCK 证书链（[文件 01，§4](01-Intel-TDX-Attestation-Chain.md)） | Intel 的根 CA 与 PCK 基础设施未被攻破 |
| 带正确 eat_nonce 的伪造 GPU EAT | NRAS JWKS 签名检查（[文件 02，§4](02-NVIDIA-CC-Attestation-Chain.md)） | NVIDIA 的 NRAS 签名密钥与 JWKS 端点未被攻破 |
| 真实硬件、错误或被篡改的软件栈 | MRTD / RTMR 度量 | 验证者的 *golden* 参考度量本身正确且为最新 —— 过时或错误的 golden 值会静默地使这项防御失效 |
| 真实证明、语义错误的结果（输入错误、参数错误、逻辑悄悄损坏） | **这里什么都没有** | 完全超出硬件证明的范围 —— 这需要重新执行（慢路径）或 proof-of-computation 方案（例如 zk-STARK 执行 trace 证明） |
| 带已披露 CPU 漏洞的节点 | TCB status 字段 | 这是一个*上报信号*，不是自动拒绝 —— 验证者的策略必须真正消费它 |

三点需要如实说明：

- **MRTD 是指纹，不是审计。** 它证明*哪个*镜像启动了，逐位可复现。它不说明该镜像的代码是否正确，除非验证者独立维护从“expected job harness”到“expected MRTD value”的可信映射。没有可信的 golden-measurement 注册表，证明只能说明“某个特定的、可复现的东西跑了”—— 而不是“对的东西跑了”。
- **硬件证明与计算正确性证明回答的是不同问题。** TDX/NRAS 证明*来源*：真实硬件、真实被度量的栈。它们不证明计算本身是否逐步正确执行。对于大规模训练，完整的计算证明（zk-STARK / ZKML）今天仍远比硬件证明昂贵 —— 这正是 proof-of-training 系统让快速验证通道依赖硬件路径、把重新执行或 proof-of-computation 留给抽查的原因。
- **“Attested”不等于“audited”。** 只检查 `tdx_bound` 与 `gpu_bound`（绑定，没有签名链）的验证者，在 §2 里看起来像全貌，却不提供其中的任何保证。这是此处最值得记住的失效模式，胜过这里其他任何一个。

## 5. 验证审查检查清单

在搭建或审查任何远程计算 claim 验证器时使用，不限于这套特定的 TDX + NVIDIA CC 组合：

- [ ] 每个被声明的值都有 **绑定检查**（它出现在硬件报告的某个特定字段中）*并且*有独立的 **真实性检查**（该报告的签名可链接到实时验证的厂商根）—— 绝不能只有其一。
- [ ] TCB/status 字段以其实际值（`UpToDate` / `OutOfDate` / advisory ID / `Revoked`）呈现，而不是在策略层看到之前就被压平成一个 pass/fail。
- [ ] 任何来自生成证据的同一 SDK/进程的自签名或对称密钥签名的 token，都要明确排除在任何被报告为“signature verified”的结果之外。
- [ ] 多组件声明（多 GPU、多节点）要检查**每一个**组件的绑定与签名，而不是一个代表性样本。
- [ ] golden/期望度量值（MRTD、RTMR、期望的 MRSIGNER）来自维护中的可信注册表 —— 而不是硬编码一次，就随着硬件/固件版本发布被遗忘。
- [ ] 设计明确说明证明**不**证明什么 —— 计算正确性以及数据/结果质量 —— 并说明什么机制（重新执行、抽样、proof-of-computation）覆盖该缺口，如果有的话。


<details>
<summary>English original</summary>

**4. Threat Model — What's Proven, What Remains Trusted**

| Threat | Defended by | Residual assumption |
|---|---|---|
| Fabricated claim JSON with no real hardware involved | binding checks alone | **none — this is the gap, not a defense; needs the authenticity checks in files 01 §4 / 02 §4 too** |
| Forged TD Quote with correct REPORTDATA bytes | DCAP signature + PCK chain ([file 01, §4](01-Intel-TDX-Attestation-Chain.md)) | Intel's root CA and PCK infrastructure are uncompromised |
| Forged GPU EAT with correct eat_nonce | NRAS JWKS signature check ([file 02, §4](02-NVIDIA-CC-Attestation-Chain.md)) | NVIDIA's NRAS signing key and JWKS endpoint are uncompromised |
| Genuine hardware, wrong/tampered software stack | MRTD / RTMR measurement | the verifier's *golden* reference measurement is itself correct and current — a stale or wrong golden value defeats this silently |
| Genuine attestation, semantically wrong result (bad inputs, wrong parameters, quietly-broken logic) | **nothing here** | out of scope for hardware attestation entirely — this needs re-execution (the slow path) or a proof-of-computation approach (e.g. zk-STARK execution-trace proofs) |
| Node with a disclosed CPU vulnerability | TCB status field | this is a *reported signal*, not an automatic reject — the validator's policy has to actually consume it |

Three honest caveats:

- **MRTD is a fingerprint, not an audit.** It proves *which* image booted, bit-for-bit reproducibly. It says nothing about whether that image's code is correct, unless the verifier independently maintains a trusted mapping from "expected job harness" to "expected MRTD value." Attestation without a trustworthy golden-measurement registry just proves "something specific and reproducible ran" — not "the right thing ran."
- **Hardware attestation and computation-correctness proofs answer different questions.** TDX/NRAS prove *provenance*: genuine hardware, genuine measured stack. They prove nothing about whether the computation itself executed correctly step by step. For large training runs, full computation proofs (zk-STARK / ZKML) remain far more expensive than hardware attestation today — which is exactly why proof-of-training systems lean on the hardware path for their fast verification lane, reserving re-execution or proof-of-computation for spot checks.
- **"Attested" is not "audited."** A verifier that only checks `tdx_bound` and `gpu_bound` (binding, no signature chain) looks like the full picture in §2 but provides none of its guarantees. This is the failure mode worth remembering above every other one here.

**5. Verification Review Checklist**

Use this when standing up or reviewing any remote-compute claim verifier, not just this specific TDX + NVIDIA CC pairing:

- [ ] Every claimed value has a **binding check** (it appears in a specific field of a hardware report) *and* a separate **authenticity check** (that report's signature chains to a live-verified vendor root) — never one without the other.
- [ ] TCB/status fields are surfaced as their actual value (`UpToDate` / `OutOfDate` / advisory ID / `Revoked`), not flattened into a single pass/fail before the policy layer sees them.
- [ ] Any self-signed or symmetric-key-signed token from the same SDK/process generating the evidence is explicitly excluded from anything reported as a "signature verified" result.
- [ ] Multi-component claims (multi-GPU, multi-node) check **every** component's binding and signature, not one representative sample.
- [ ] Golden/expected measurement values (MRTD, RTMR, expected MRSIGNER) are sourced from a maintained, trusted registry — not hardcoded once and forgotten as hardware/firmware revisions ship.
- [ ] The design is explicit about what attestation does **not** prove — computational correctness and data/result quality — and states what mechanism (re-execution, sampling, proof-of-computation) covers that gap, if anything does.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Confidential-Computing-Attestation/03-Dual-Vendor-Claim-Binding-and-Verification.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Confidential-Computing-Attestation/03-Dual-Vendor-Claim-Binding-and-Verification.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
