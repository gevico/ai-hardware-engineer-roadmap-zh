// fused_norm.cu — Fused RMSNorm + Residual Add (SM 8.7)
// Handles both FP32 and FP16 norm weights (GGUF stores them as F32 typically).

#include "jllm_kernels.h"
#include <cuda_fp16.h>

namespace jllm {

__global__ void fused_rmsnorm_residual_kernel(
    half*       __restrict__ output,
    const half* __restrict__ x,
    const half* __restrict__ residual,
    const void* __restrict__ weight,
    int rows, int dim, float eps, bool weight_fp32)
{
    const int row = blockIdx.x;
    if (row >= rows) return;

    const int tid = threadIdx.x;
    const int stride = blockDim.x;
    const int offset = row * dim;

    extern __shared__ float sdata[];

    // Pass 1: compute x + residual and accumulate sum of squares
    float sum_sq = 0.0f;
    for (int i = tid; i < dim; i += stride) {
        float val = __half2float(x[offset + i]) + __half2float(residual[offset + i]);
        sdata[i] = val;
        sum_sq += val * val;
    }

    // Warp reduction
    #pragma unroll
    for (int off = 16; off > 0; off >>= 1)
        sum_sq += __shfl_xor_sync(0xFFFFFFFF, sum_sq, off);

    __shared__ float warp_sums[4];
    int warp_id = tid / 32;
    int warp_lane = tid & 31;
    if (warp_lane == 0) warp_sums[warp_id] = sum_sq;
    __syncthreads();

    if (tid == 0) {
        float total = 0.0f;
        for (int w = 0; w < (stride + 31) / 32; w++)
            total += warp_sums[w];
        warp_sums[0] = total;
    }
    __syncthreads();

    float rrms = rsqrtf(warp_sums[0] / dim + eps);

    // Pass 2: normalize and scale by weight
    for (int i = tid; i < dim; i += stride) {
        float normed = sdata[i] * rrms;
        float w;
        if (weight_fp32)
            w = ((const float*)weight)[i];
        else
            w = __half2float(((const half*)weight)[i]);
        output[offset + i] = __float2half(normed * w);
    }
}

void fused_rmsnorm_residual(half* output, const half* x, const half* residual,
                            const void* weight, int rows, int hidden_dim,
                            float eps, bool weight_fp32, cudaStream_t stream) {
    int block = BLOCK_SIZE;
    int smem = hidden_dim * sizeof(float);
    if (smem > JLLM_SHARED_MEM_SM) smem = JLLM_SHARED_MEM_SM;

    fused_rmsnorm_residual_kernel<<<rows, block, smem, stream>>>(
        output, x, residual, weight, rows, hidden_dim, eps, weight_fp32);
}

}  // namespace jllm
