// budget.cpp — Read real Jetson memory state from /proc

#include "jllm_memory.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace jllm {

static int64_t parse_meminfo_field(const char* field) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    char line[256];
    int64_t value = -1;
    size_t flen = strlen(field);

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, field, flen) == 0) {
            // Format: "FieldName:     12345 kB"
            char* p = line + flen;
            while (*p == ' ' || *p == ':') p++;
            value = strtoll(p, nullptr, 10) / 1024;  // kB → MB
            break;
        }
    }
    fclose(f);
    return value;
}

MemoryBudget probe_system_memory() {
    MemoryBudget b = {};

    b.total_mb = parse_meminfo_field("MemTotal");
    int64_t available = parse_meminfo_field("MemAvailable");
    b.cma_mb = parse_meminfo_field("CmaTotal");

    // Estimate OS usage = total - available (before we load anything)
    b.os_mb = b.total_mb - available - b.cma_mb;
    if (b.os_mb < 0) b.os_mb = 500;  // fallback estimate

    // CUDA context: probe by actually initializing
    // (deferred — set estimate, update after cudaSetDevice)
    b.cuda_ctx_mb = 300;

    b.model_mb = 0;
    b.kv_cache_mb = 0;
    b.scratch_mb = 0;
    b.safety_mb = 256;

    return b;
}

// ── OOM Guard ────────────────────────────────────────────────────────────

bool OOMGuard::can_extend(int64_t additional_bytes) const {
    int64_t free = real_free_mb();
    int64_t needed_mb = additional_bytes / (1024 * 1024) + 1;
    return free > (needed_mb + safety_mb_);
}

int64_t OOMGuard::real_free_mb() const {
    return parse_meminfo_field("MemAvailable");
}

void OOMGuard::emergency_free() {
    // Drop filesystem caches (requires root)
    FILE* f = fopen("/proc/sys/vm/drop_caches", "w");
    if (f) {
        fputs("3\n", f);
        fclose(f);
    }
    // Trigger compaction
    f = fopen("/proc/sys/vm/compact_memory", "w");
    if (f) {
        fputs("1\n", f);
        fclose(f);
    }
}

}  // namespace jllm
