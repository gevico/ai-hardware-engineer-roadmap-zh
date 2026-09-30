// softmax.cu — Numerically stable softmax for logits (SM 8.7)
//
// Used only for final logit → probability conversion (vocab_size elements).
// Attention softmax is fused inside attention.cu.
//
// Two-pass: max reduction → exp and sum → normalize.
// Single block for vocab ≤ 128K (covers all current LLMs).

#include "jllm_kernels.h"
#include <cuda_fp16.h>
#include <cfloat>

namespace jllm {

__global__ void softmax_kernel(float* __restrict__ x, int n) {
    const int tid = threadIdx.x;
    const int stride = blockDim.x;

    extern __shared__ float smem[];

    // Pass 1: find max
    float local_max = -FLT_MAX;
    for (int i = tid; i < n; i += stride)
        local_max = fmaxf(local_max, x[i]);

    // Warp reduce max
    #pragma unroll
    for (int off = 16; off > 0; off >>= 1)
        local_max = fmaxf(local_max, __shfl_xor_sync(0xFFFFFFFF, local_max, off));

    // Cross-warp reduce
    int warp_id = tid / 32;
    int lane = tid & 31;
    if (lane == 0) smem[warp_id] = local_max;
    __syncthreads();

    if (tid == 0) {
        float m = -FLT_MAX;
        for (int w = 0; w < (stride + 31) / 32; w++)
            m = fmaxf(m, smem[w]);
        smem[0] = m;
    }
    __syncthreads();
    float max_val = smem[0];

    // Pass 2: exp(x - max) and sum
    float local_sum = 0.0f;
    for (int i = tid; i < n; i += stride) {
        float e = expf(x[i] - max_val);
        x[i] = e;
        local_sum += e;
    }

    // Reduce sum
    #pragma unroll
    for (int off = 16; off > 0; off >>= 1)
        local_sum += __shfl_xor_sync(0xFFFFFFFF, local_sum, off);

    if (lane == 0) smem[warp_id] = local_sum;
    __syncthreads();

    if (tid == 0) {
        float s = 0.0f;
        for (int w = 0; w < (stride + 31) / 32; w++)
            s += smem[w];
        smem[0] = s;
    }
    __syncthreads();
    float inv_sum = 1.0f / smem[0];

    // Pass 3: normalize
    for (int i = tid; i < n; i += stride)
        x[i] *= inv_sum;
}

void softmax_inplace(float* x, int n, cudaStream_t stream) {
    // Use 1 block, 256 threads for vocab sizes up to 128K
    int block = 256;
    int smem = (block / 32 + 1) * sizeof(float);
    softmax_kernel<<<1, block, smem, stream>>>(x, n);
}

}  // namespace jllm
