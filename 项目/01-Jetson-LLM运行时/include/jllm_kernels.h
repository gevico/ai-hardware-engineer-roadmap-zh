// jllm_kernels.h — Orin-tuned CUDA kernels (SM 8.7 ONLY)
//
// Design rules:
//   1. Every kernel minimizes DRAM traffic (102 GB/s is the bottleneck)
//   2. Tile sizes tuned for 48 KB shared memory per SM
//   3. Thread blocks sized for 16 SMs × 48 warps max
//   4. Fuse everything possible (fewer kernels = fewer DRAM round-trips)
//   5. INT4 dequant fused into compute (never write dequantized weights to DRAM)
//
// No desktop GPU paths. No Volta. No Turing. Only SM 8.7.

#pragma once

#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cstdint>

// Orin SM 8.7 constants (duplicated here to avoid circular include with jllm.h)
#ifndef JLLM_SHARED_MEM_SM
#define JLLM_SHARED_MEM_SM   (48 * 1024)   // 48 KB per SM
#endif
#ifndef JLLM_WARP_SIZE
#define JLLM_WARP_SIZE       32
#endif

namespace jllm {

// Orin-optimal tile sizes (smaller than desktop — 48 KB shared mem)
constexpr int TILE_M = 32;
constexpr int TILE_N = 64;
constexpr int TILE_K = 32;
constexpr int ATTENTION_TILE_Q  = 32;   // query tokens per tile
constexpr int ATTENTION_TILE_KV = 64;   // KV tokens per tile
constexpr int BLOCK_SIZE = 128;         // threads per block (4 warps, good occupancy)

// ── Quantized GEMV (decode: batch=1, the hot path) ──────────────────────
// Q4_K dequant fused into GEMV — never writes dequantized weights to DRAM
//
// y[M] = W[M×K] (Q4) × x[K] (FP16)
// W stored as: 4-bit weights + FP16 scales per group of 32
//
void gemv_q4(
    half*          y,          // output [M]
    const void*    W_q4,       // quantized weights [M × K / 2] (4-bit packed)
    const half*    scales,     // per-group scales [M × K / group_size]
    const half*    x,          // input [K]
    int            M,
    int            K,
    int            group_size, // typically 32 or 128
    cudaStream_t   stream
);

// ── Fused RMSNorm + Residual Add ────────────────────────────────────────
// input = RMSNorm(x + residual) * weight
// One read of x, one read of residual, one write of output.
// 3× less DRAM traffic than separate kernels.
//
void fused_rmsnorm_residual(
    half*          output,
    const half*    x,
    const half*    residual,
    const void*    weight,     // norm weight [hidden_dim] — FP32 or FP16
    int            rows,
    int            hidden_dim,
    float          eps,
    bool           weight_fp32,// true if weight is float*, false if half*
    cudaStream_t   stream
);

// ── Fused SwiGLU ─────────────────────────────────────────────────────────
// output = silu(gate) * up
// gate and up are computed from the same input via two GEMV calls,
// but this kernel fuses the activation: no intermediate write to DRAM.
//
void fused_swiglu(
    half*          output,
    const half*    gate,       // [rows × intermediate_dim]
    const half*    up,         // [rows × intermediate_dim]
    int            rows,
    int            intermediate_dim,
    cudaStream_t   stream
);

// ── Rotary Position Embedding ────────────────────────────────────────────
// Applied in-place to Q and K before attention.
//
void rope_inplace(
    half*          q,          // [n_heads × head_dim]
    half*          k,          // [n_kv_heads × head_dim]
    int            n_heads,
    int            n_kv_heads,
    int            head_dim,
    int            position,   // token position in sequence
    float          theta_base, // RoPE base (typically 10000 or 500000)
    cudaStream_t   stream
);

// ── Flash Attention (single query, decode path) ──────────────────────────
// Fused: Q×K^T → scale → softmax → ×V, all in shared memory + registers.
// Tuned for 48 KB shared mem: TILE_Q=1 (decode), TILE_KV=64.
// KV cache can be FP16 or INT8.
//
void flash_attention_decode(
    half*          output,     // [n_heads × head_dim]
    const half*    q,          // [n_heads × head_dim] (single query)
    const void*    k_cache,    // [n_kv_heads × seq_len × head_dim] (FP16 or INT8)
    const void*    v_cache,    // [n_kv_heads × seq_len × head_dim]
    int            n_heads,
    int            n_kv_heads, // GQA: n_heads / n_kv_heads = group size
    int            head_dim,
    int            seq_len,    // current sequence length
    float          scale,      // 1/sqrt(head_dim)
    bool           kv_int8,    // true = INT8 KV cache
    const float*   kv_scales,  // per-head scales if kv_int8 (else nullptr)
    cudaStream_t   stream
);

// ── Softmax (standalone, for logits) ─────────────────────────────────────
void softmax_inplace(
    float*         x,
    int            n,
    cudaStream_t   stream
);

// ── FP16 ↔ INT8 conversion (for KV cache quantization) ──────────────────
void fp16_to_int8(
    int8_t*        dst,
    float*         scale_out,  // one scale per row
    const half*    src,
    int            rows,
    int            cols,
    cudaStream_t   stream
);

}  // namespace jllm
