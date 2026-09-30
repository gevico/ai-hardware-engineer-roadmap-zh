// kv_cache.cpp — Pre-allocated KV cache with GPU/CPU tiering
//
// On Jetson unified memory:
//   cudaMallocHost → pinned DRAM, GPU reads at full bandwidth, CPU can also read
//   malloc         → pageable DRAM, GPU reads via page faults (slower, but doesn't OOM)
//
// Strategy:
//   - "Fast pool" (cudaMallocHost): recent tokens, GPU reads at ~102 GB/s
//   - "Overflow pool" (malloc): old tokens, GPU reads via page faults (~50 GB/s)
//   - When fast pool full: evict oldest to overflow (just a memcpy within same DRAM)

#include "jllm_memory.h"
#include <cuda_runtime.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace jllm {

bool KVCachePool::init(const Config& cfg) {
    cfg_ = cfg;

    int64_t fast_bytes = (int64_t)cfg.n_layers * entry_bytes() * cfg.max_context;
    int64_t overflow_bytes = (int64_t)cfg.n_layers * entry_bytes() * cfg.overflow_context;

    fprintf(stderr, "[kv_cache] Allocating fast pool: %ld MB (%d tokens)\n",
            fast_bytes / (1024*1024), cfg.max_context);

    // Fast pool: pinned memory (GPU + CPU accessible, no page faults)
    cudaError_t err = cudaMallocHost(&gpu_pool_, fast_bytes);
    if (err != cudaSuccess) {
        fprintf(stderr, "[kv_cache] FATAL: cudaMallocHost failed: %s\n",
                cudaGetErrorString(err));
        return false;
    }
    memset(gpu_pool_, 0, fast_bytes);

    // Overflow pool: regular malloc (optional)
    if (cfg.overflow_context > 0 && overflow_bytes > 0) {
        fprintf(stderr, "[kv_cache] Allocating overflow pool: %ld MB (%d tokens)\n",
                overflow_bytes / (1024*1024), cfg.overflow_context);
        cpu_pool_ = malloc(overflow_bytes);
        if (!cpu_pool_) {
            fprintf(stderr, "[kv_cache] WARNING: overflow pool alloc failed, disabling\n");
            cfg_.overflow_context = 0;
        } else {
            memset(cpu_pool_, 0, overflow_bytes);
        }
    }

    used_tokens_ = 0;
    gpu_tokens_ = 0;
    return true;
}

void KVCachePool::destroy() {
    if (gpu_pool_) { cudaFreeHost(gpu_pool_); gpu_pool_ = nullptr; }
    if (cpu_pool_) { free(cpu_pool_); cpu_pool_ = nullptr; }
    used_tokens_ = 0;
    gpu_tokens_ = 0;
}

void* KVCachePool::key_ptr(int layer, int pos) {
    int64_t eb = entry_bytes() / 2;  // key is half of entry (key + value)
    if (pos < cfg_.max_context) {
        return (char*)gpu_pool_ + layer * layer_stride() + pos * eb;
    } else {
        int overflow_pos = pos - cfg_.max_context;
        return (char*)cpu_pool_ + layer * (int64_t)cfg_.overflow_context * eb + overflow_pos * eb;
    }
}

void* KVCachePool::val_ptr(int layer, int pos) {
    int64_t eb = entry_bytes() / 2;
    int64_t key_offset = cfg_.max_context * eb;  // values stored after all keys
    if (pos < cfg_.max_context) {
        return (char*)gpu_pool_ + layer * layer_stride() + key_offset + pos * eb;
    } else {
        int overflow_pos = pos - cfg_.max_context;
        int64_t ov_key_offset = cfg_.overflow_context * eb;
        return (char*)cpu_pool_ + layer * (int64_t)cfg_.overflow_context * eb * 2
               + ov_key_offset + overflow_pos * eb;
    }
}

int64_t KVCachePool::used_bytes() const {
    return (int64_t)used_tokens_ * entry_bytes() * cfg_.n_layers;
}

int64_t KVCachePool::capacity_bytes() const {
    return (int64_t)(cfg_.max_context + cfg_.overflow_context) * entry_bytes() * cfg_.n_layers;
}

void KVCachePool::evict(int n_tokens) {
    if (!cpu_pool_ || n_tokens <= 0) return;

    int64_t eb = entry_bytes();
    for (int l = 0; l < cfg_.n_layers; l++) {
        char* src = (char*)gpu_pool_ + l * layer_stride();
        char* dst = (char*)cpu_pool_ + l * (int64_t)cfg_.overflow_context * eb;

        // Copy oldest n_tokens to overflow
        memcpy(dst, src, n_tokens * eb);

        // Shift remaining in fast pool
        int remaining = gpu_tokens_ - n_tokens;
        if (remaining > 0) {
            memmove(src, src + n_tokens * eb, remaining * eb);
        }
    }

    gpu_tokens_ -= n_tokens;
    fprintf(stderr, "[kv_cache] Evicted %d tokens to overflow (gpu: %d, total: %d)\n",
            n_tokens, gpu_tokens_, used_tokens_);
}

void KVCachePool::clear() {
    used_tokens_ = 0;
    gpu_tokens_ = 0;
}

}  // namespace jllm
