// rope.cu — Rotary Position Embedding (in-place, SM 8.7)
//
// Applies RoPE to Q and K tensors before attention.
// Fused: processes both Q and K in one kernel launch.
//
// RoPE formula for dimension pair (2i, 2i+1):
//   q'[2i]   = q[2i]   * cos(θ) - q[2i+1] * sin(θ)
//   q'[2i+1] = q[2i]   * sin(θ) + q[2i+1] * cos(θ)
//   where θ = position / (theta_base ^ (2i / head_dim))

#include "jllm_kernels.h"
#include <cuda_fp16.h>
#include <cmath>

namespace jllm {

__global__ void rope_kernel(
    half* __restrict__ q,       // [n_heads × head_dim]
    half* __restrict__ k,       // [n_kv_heads × head_dim]
    int n_heads, int n_kv_heads, int head_dim,
    int position, float theta_base)
{
    // One thread per dimension pair
    const int tid = blockIdx.x * blockDim.x + threadIdx.x;
    const int half_dim = head_dim / 2;
    const int total_pairs = (n_heads + n_kv_heads) * half_dim;

    if (tid >= total_pairs) return;

    // Determine if this is a Q or K pair
    const int q_pairs = n_heads * half_dim;
    bool is_q = (tid < q_pairs);

    int head, pair_idx;
    half* ptr;
    if (is_q) {
        head = tid / half_dim;
        pair_idx = tid % half_dim;
        ptr = q + head * head_dim;
    } else {
        int k_tid = tid - q_pairs;
        head = k_tid / half_dim;
        pair_idx = k_tid % half_dim;
        ptr = k + head * head_dim;
    }

    // Compute rotation angle
    float freq = 1.0f / powf(theta_base, (2.0f * pair_idx) / head_dim);
    float angle = position * freq;
    float cos_val = cosf(angle);
    float sin_val = sinf(angle);

    // Rotate the pair
    int idx0 = pair_idx * 2;
    int idx1 = pair_idx * 2 + 1;
    float v0 = __half2float(ptr[idx0]);
    float v1 = __half2float(ptr[idx1]);

    ptr[idx0] = __float2half(v0 * cos_val - v1 * sin_val);
    ptr[idx1] = __float2half(v0 * sin_val + v1 * cos_val);
}

void rope_inplace(half* q, half* k, int n_heads, int n_kv_heads,
                  int head_dim, int position, float theta_base,
                  cudaStream_t stream) {
    int total_pairs = (n_heads + n_kv_heads) * (head_dim / 2);
    int block = BLOCK_SIZE;  // 128
    int grid = (total_pairs + block - 1) / block;

    rope_kernel<<<grid, block, 0, stream>>>(
        q, k, n_heads, n_kv_heads, head_dim, position, theta_base);
}

}  // namespace jllm
