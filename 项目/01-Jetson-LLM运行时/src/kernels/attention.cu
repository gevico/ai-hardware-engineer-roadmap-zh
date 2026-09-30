// attention.cu — Flash Attention decode (single query) for Orin SM 8.7
//
// BUG #6 FIX: proper per-dimension accumulator using shared memory output buffer
// instead of broken acc[d % 4]. Each thread handles head_dim / blockDim.x dimensions.
//
// One block per query head. Tiles KV in chunks of 64.
// Online softmax: never materializes full seq×seq attention matrix.
// Supports GQA and INT8 KV cache.

#include "jllm_kernels.h"
#include <cuda_fp16.h>
#include <cfloat>

namespace jllm {

static constexpr int ATTN_TILE_KV = 64;
static constexpr int ATTN_BLOCK   = 128;

// Each thread handles ceil(head_dim / blockDim.x) output dimensions.
// Accumulators stored in shared memory (visible to all threads in block).

__global__ void flash_attention_decode_kernel(
    half*        __restrict__ output,
    const half*  __restrict__ q,
    const void*  __restrict__ k_cache,
    const void*  __restrict__ v_cache,
    int n_heads, int n_kv_heads, int head_dim, int seq_len,
    float scale, bool kv_int8, const float* kv_scales)
{
    const int head = blockIdx.x;
    const int kv_head = head / (n_heads / n_kv_heads);  // GQA
    const int tid = threadIdx.x;

    // Shared memory layout:
    //   s_scores[ATTN_TILE_KV]     — attention scores for current KV tile
    //   s_out[head_dim]            — output accumulator (BUG #6 FIX)
    extern __shared__ float smem[];
    float* s_scores = smem;
    float* s_out    = smem + ATTN_TILE_KV;

    // Initialize output accumulator to zero
    for (int d = tid; d < head_dim; d += blockDim.x)
        s_out[d] = 0.0f;
    __syncthreads();

    // Q is read directly from global memory in the dot product loop below.
    // (No register pre-load needed — Q is small and stays in L1 cache.)

    float running_max = -FLT_MAX;
    float running_sum = 0.0f;

    // Tile over KV sequence
    for (int kv_start = 0; kv_start < seq_len; kv_start += ATTN_TILE_KV) {
        int tile_len = min(ATTN_TILE_KV, seq_len - kv_start);

        // ── Step 1: Q × K^T for tile ────────────────────────────
        for (int t = tid; t < tile_len; t += blockDim.x) {
            int kv_pos = kv_start + t;
            float dot = 0.0f;

            for (int d = 0; d < head_dim; d++) {
                float q_val = __half2float(q[head * head_dim + d]);
                float k_val;
                if (kv_int8) {
                    const int8_t* ki = (const int8_t*)k_cache;
                    float ks = kv_scales ? kv_scales[kv_head] : 1.0f;
                    k_val = ki[(int64_t)kv_head * seq_len * head_dim + kv_pos * head_dim + d] * ks;
                } else {
                    const half* kf = (const half*)k_cache;
                    k_val = __half2float(kf[(int64_t)kv_head * seq_len * head_dim + kv_pos * head_dim + d]);
                }
                dot += q_val * k_val;
            }
            s_scores[t] = dot * scale;
        }
        __syncthreads();

        // ── Step 2: Online softmax ──────────────────────────────
        float tile_max = -FLT_MAX;
        for (int t = tid; t < tile_len; t += blockDim.x)
            tile_max = fmaxf(tile_max, s_scores[t]);

        // Reduce max across block
        #pragma unroll
        for (int off = 16; off > 0; off >>= 1)
            tile_max = fmaxf(tile_max, __shfl_xor_sync(0xFFFFFFFF, tile_max, off));

        __shared__ float smax;
        if (tid == 0) smax = -FLT_MAX;
        __syncthreads();
        if (tid % 32 == 0) atomicMax((int*)&smax, __float_as_int(tile_max));
        __syncthreads();
        tile_max = smax;

        // Correction factor for running max update
        float old_max = running_max;
        running_max = fmaxf(running_max, tile_max);
        float correction = expf(old_max - running_max);

        // Correct existing accumulators
        running_sum *= correction;
        for (int d = tid; d < head_dim; d += blockDim.x)
            s_out[d] *= correction;
        __syncthreads();

        // Exponentiate scores
        float tile_sum = 0.0f;
        for (int t = tid; t < tile_len; t += blockDim.x) {
            float p = expf(s_scores[t] - running_max);
            s_scores[t] = p;
            tile_sum += p;
        }

        // Reduce sum
        #pragma unroll
        for (int off = 16; off > 0; off >>= 1)
            tile_sum += __shfl_xor_sync(0xFFFFFFFF, tile_sum, off);

        __shared__ float ssum;
        if (tid == 0) ssum = 0.0f;
        __syncthreads();
        if (tid % 32 == 0) atomicAdd(&ssum, tile_sum);
        __syncthreads();
        running_sum += ssum;

        // ── Step 3: Accumulate P × V (BUG #6 FIX — per-dimension) ──
        // Each thread handles its slice of head_dim
        for (int d = tid; d < head_dim; d += blockDim.x) {
            float val = 0.0f;
            for (int t = 0; t < tile_len; t++) {
                int kv_pos = kv_start + t;
                float v_val;
                if (kv_int8) {
                    const int8_t* vi = (const int8_t*)v_cache;
                    float vs = kv_scales ? kv_scales[n_kv_heads + kv_head] : 1.0f;
                    v_val = vi[(int64_t)kv_head * seq_len * head_dim + kv_pos * head_dim + d] * vs;
                } else {
                    const half* vf = (const half*)v_cache;
                    v_val = __half2float(vf[(int64_t)kv_head * seq_len * head_dim + kv_pos * head_dim + d]);
                }
                val += s_scores[t] * v_val;
            }
            s_out[d] += val;  // BUG #6 FIX: accumulate into correct dimension
        }
        __syncthreads();
    }

    // ── Finalize: normalize and write output ─────────────────────
    float inv_sum = (running_sum > 0.0f) ? 1.0f / running_sum : 0.0f;
    for (int d = tid; d < head_dim; d += blockDim.x) {
        output[head * head_dim + d] = __float2half(s_out[d] * inv_sum);
    }
}

void flash_attention_decode(
    half* output, const half* q, const void* k_cache, const void* v_cache,
    int n_heads, int n_kv_heads, int head_dim, int seq_len,
    float scale, bool kv_int8, const float* kv_scales, cudaStream_t stream)
{
    // Shared memory: scores[TILE_KV] + output[head_dim]
    int smem = (ATTN_TILE_KV + head_dim) * sizeof(float);

    flash_attention_decode_kernel<<<n_heads, ATTN_BLOCK, smem, stream>>>(
        output, q, k_cache, v_cache,
        n_heads, n_kv_heads, head_dim, seq_len,
        scale, kv_int8, kv_scales);
}

}  // namespace jllm
