// pool.cpp — Scratch memory pool (bump allocator, zero malloc during inference)

#include "jllm_memory.h"
#include <cuda_runtime.h>
#include <cstdio>

namespace jllm {

bool ScratchPool::init(int64_t size_bytes) {
    cudaError_t err = cudaMallocHost(&base_, size_bytes);
    if (err != cudaSuccess) {
        fprintf(stderr, "[scratch] cudaMallocHost(%ld MB) failed: %s\n",
                size_bytes / (1024*1024), cudaGetErrorString(err));
        return false;
    }
    capacity_ = size_bytes;
    offset_ = 0;
    fprintf(stderr, "[scratch] Allocated %ld MB scratch pool\n", size_bytes / (1024*1024));
    return true;
}

void ScratchPool::destroy() {
    if (base_) { cudaFreeHost(base_); base_ = nullptr; }
    capacity_ = 0;
    offset_ = 0;
}

void* ScratchPool::get(int64_t size) {
    // Align to 256 bytes (GPU coalescing + cache line friendly)
    size = (size + 255) & ~255LL;

    if (offset_ + size > capacity_) {
        fprintf(stderr, "[scratch] FATAL: pool exhausted (%ld / %ld bytes)\n",
                offset_ + size, capacity_);
        return nullptr;
    }

    void* ptr = (char*)base_ + offset_;
    offset_ += size;
    return ptr;
}

void ScratchPool::reset() {
    offset_ = 0;
}

}  // namespace jllm
