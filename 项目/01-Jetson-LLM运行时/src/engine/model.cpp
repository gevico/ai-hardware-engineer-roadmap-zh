// model.cpp — GGUF model loader (minimal, Jetson-optimized)
//
// We only read what we need from GGUF:
//   - Model config (n_layers, n_heads, etc.)
//   - Weight tensors (mmap for zero-copy, then pin for GPU access)
//   - Tokenizer vocabulary
//
// No support for FP32 models (waste of memory on 8 GB).
// Only Q4_K_M, Q5_K_M, Q8_0, FP16 quantizations.

#include "jllm_engine.h"
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace jllm {

// ── GGUF magic and header ────────────────────────────────────────────────

static constexpr uint32_t GGUF_MAGIC = 0x46554747;  // "GGUF" as uint32 little-endian (bytes: G G U F)

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t n_tensors;
    uint64_t n_kv;
};

// Minimal KV metadata reader
struct GGUFMetaKV {
    char     key[256];
    uint32_t value_type;
    union {
        uint32_t u32;
        int32_t  i32;
        float    f32;
        uint64_t u64;
    };
};

// ── Read model config from GGUF ──────────────────────────────────────────

ModelConfig load_gguf_config(const std::string& path) {
    ModelConfig cfg = {};
    cfg.name = path;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "[gguf] Cannot open %s\n", path.c_str());
        return cfg;
    }

    GGUFHeader hdr;
    fread(&hdr, sizeof(hdr), 1, f);

    if (hdr.magic != GGUF_MAGIC) {
        fprintf(stderr, "[gguf] Invalid magic: 0x%08X (expected 0x%08X)\n",
                hdr.magic, GGUF_MAGIC);
        fclose(f);
        return cfg;
    }

    fprintf(stderr, "[gguf] Version %u, %lu tensors, %lu metadata entries\n",
            hdr.version, hdr.n_tensors, hdr.n_kv);

    // Read metadata key-value pairs
    // This is a simplified reader — production code should handle all GGUF types
    for (uint64_t i = 0; i < hdr.n_kv && i < 256; i++) {
        // Read key length + key string
        uint64_t key_len;
        fread(&key_len, sizeof(key_len), 1, f);
        if (key_len > 255) {
            // Skip long key + its value
            fseek(f, key_len, SEEK_CUR);
            uint32_t skip_type;
            fread(&skip_type, 4, 1, f);
            // Skip value based on type
            if (skip_type <= 1 || skip_type == 7) fseek(f, 1, SEEK_CUR);
            else if (skip_type <= 3) fseek(f, 2, SEEK_CUR);
            else if (skip_type <= 6) fseek(f, 4, SEEK_CUR);
            else if (skip_type >= 10 && skip_type <= 12) fseek(f, 8, SEEK_CUR);
            else if (skip_type == 8) { uint64_t sl; fread(&sl, 8, 1, f); fseek(f, sl, SEEK_CUR); }
            else if (skip_type == 9) {
                uint32_t at; uint64_t al; fread(&at, 4, 1, f); fread(&al, 8, 1, f);
                if (at == 8) { for (uint64_t a = 0; a < al; a++) { uint64_t sl; fread(&sl, 8, 1, f); fseek(f, sl, SEEK_CUR); } }
                else { int esz = (at<=1||at==7)?1:(at<=3)?2:(at<=6)?4:8; fseek(f, al*esz, SEEK_CUR); }
            }
            else fseek(f, 8, SEEK_CUR);
            continue;
        }

        char key[256] = {};
        fread(key, 1, key_len, f);

        // Read value type
        uint32_t vtype;
        fread(&vtype, sizeof(vtype), 1, f);

        // Parse based on type
        if (vtype == 4) {  // GGUF_TYPE_UINT32
            uint32_t val;
            fread(&val, sizeof(val), 1, f);

            if (strstr(key, "block_count"))        cfg.n_layers = val;
            else if (strstr(key, "head_count_kv")) cfg.n_kv_heads = val;
            else if (strstr(key, "head_count"))    cfg.n_heads = val;
            else if (strstr(key, "embedding_length")) cfg.hidden_dim = val;
            else if (strstr(key, "feed_forward_length")) cfg.intermediate_dim = val;
            else if (strstr(key, "vocab_size"))    cfg.vocab_size = val;
            else if (strstr(key, "context_length")) cfg.max_seq_len = val;
        } else if (vtype == 6) {  // GGUF_TYPE_FLOAT32
            float val;
            fread(&val, sizeof(val), 1, f);
            if (strstr(key, "rope.freq_base")) cfg.rope_theta = val;
        } else if (vtype == 8) {  // GGUF_TYPE_STRING
            uint64_t str_len;
            fread(&str_len, sizeof(str_len), 1, f);
            if (strstr(key, "general.name") && str_len < 256) {
                char name[256] = {};
                fread(name, 1, str_len, f);
                cfg.name = name;
            } else {
                fseek(f, str_len, SEEK_CUR);
            }
        } else {
            // Skip unknown types (need proper GGUF parser for production)
            // For now, try to skip common sizes
            fseek(f, 8, SEEK_CUR);  // rough skip
        }
    }

    // Derive head_dim
    if (cfg.n_heads > 0 && cfg.hidden_dim > 0)
        cfg.head_dim = cfg.hidden_dim / cfg.n_heads;

    // Defaults for missing fields
    if (cfg.rope_theta == 0.0f) cfg.rope_theta = 10000.0f;
    if (cfg.max_seq_len == 0) cfg.max_seq_len = 2048;
    if (cfg.n_kv_heads == 0) cfg.n_kv_heads = cfg.n_heads;

    // vocab_size: if not found in metadata, derive from token_embd tensor shape
    // (many GGUF files don't have a vocab_size metadata key)
    if (cfg.vocab_size == 0 && cfg.hidden_dim > 0) {
        // Quick scan tensor infos for token_embd.weight shape[0]
        fseek(f, 0, SEEK_SET);
        // Re-read header
        GGUFHeader hdr2;
        fread(&hdr2, sizeof(hdr2), 1, f);
        // Skip all KV pairs (reuse the skip logic from parse_tensor_infos)
        // For simplicity, just set a common default
        cfg.vocab_size = 32000;  // Llama default — will be overridden by tokenizer
        fprintf(stderr, "[gguf] vocab_size not in metadata, defaulting to %d\n", cfg.vocab_size);
    }

    fclose(f);
    return cfg;
}

// ── Load weights via mmap + pin ──────────────────────────────────────────

bool load_gguf_weights(const std::string& path, void** weights, int64_t* size) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[gguf] Cannot open %s\n", path.c_str());
        return false;
    }

    struct stat st;
    fstat(fd, &st);
    *size = st.st_size;

    fprintf(stderr, "[gguf] Mapping %ld MB...\n", *size / (1024 * 1024));

    // mmap the entire file (read-only)
    void* mapped = mmap(nullptr, *size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mapped == MAP_FAILED) {
        fprintf(stderr, "[gguf] mmap failed\n");
        return false;
    }

    // Advise kernel: we'll read sequentially during load, then random during inference
    madvise(mapped, *size, MADV_SEQUENTIAL);

    // On Jetson unified memory, mmap'd file is already in DRAM.
    // For GPU access, we need it pinned or use cudaHostRegister.
    // cudaHostRegister pins existing pages without copying.
    cudaError_t err = cudaHostRegister(mapped, *size, cudaHostRegisterReadOnly);
    if (err != cudaSuccess) {
        fprintf(stderr, "[gguf] cudaHostRegister failed: %s (continuing without pin)\n",
                cudaGetErrorString(err));
        // Not fatal — GPU can still read via page faults (slower but works)
    }

    madvise(mapped, *size, MADV_RANDOM);  // switch to random access for inference

    *weights = mapped;
    fprintf(stderr, "[gguf] Loaded and pinned %ld MB\n", *size / (1024 * 1024));
    return true;
}

int64_t ModelConfig::weight_bytes() const {
    // Rough estimate based on param count and quant type
    int64_t params = (int64_t)n_layers *
        (4LL * hidden_dim * hidden_dim +       // Q, K, V, O projections
         2LL * hidden_dim * intermediate_dim);  // gate + up projections
    params += (int64_t)vocab_size * hidden_dim; // embedding + output

    // Q4_K_M: ~0.5 bytes per param + scales
    return params / 2 + params / 32 * 2;  // weights + scales
}

// ── Weight tensor mapping ────────────────────────────────────────────────
// GGUF stores tensor info (name, shape, offset) in the header.
// After loading the blob via mmap, we compute pointers into it.
//
// This is a simplified mapper — assumes standard Llama-style tensor names.
// A full implementation would parse the GGUF tensor info section.

// ── GGUF tensor info structures ──────────────────────────────────────────

// GGUF tensor element types
enum GGMLType {
    GGML_TYPE_F32   = 0,
    GGML_TYPE_F16   = 1,
    GGML_TYPE_Q4_0  = 2,
    GGML_TYPE_Q4_1  = 3,
    GGML_TYPE_Q5_0  = 6,
    GGML_TYPE_Q5_1  = 7,
    GGML_TYPE_Q8_0  = 8,
    GGML_TYPE_Q8_1  = 9,
    GGML_TYPE_Q2_K  = 10,
    GGML_TYPE_Q3_K  = 11,
    GGML_TYPE_Q4_K  = 12,
    GGML_TYPE_Q5_K  = 13,
    GGML_TYPE_Q6_K  = 14,
    GGML_TYPE_IQ2_XXS = 16,
    GGML_TYPE_IQ2_XS  = 17,
};

// Bytes per block for quantized types (block = 32 elements for Q4_K)
static int64_t ggml_type_block_size(int type) {
    switch (type) {
        case GGML_TYPE_F32:   return 1;
        case GGML_TYPE_F16:   return 1;
        case GGML_TYPE_Q4_0:  return 32;
        case GGML_TYPE_Q4_1:  return 32;
        case GGML_TYPE_Q5_0:  return 32;
        case GGML_TYPE_Q5_1:  return 32;
        case GGML_TYPE_Q8_0:  return 32;
        case GGML_TYPE_Q8_1:  return 32;
        case GGML_TYPE_Q4_K:  return 256;
        case GGML_TYPE_Q5_K:  return 256;
        case GGML_TYPE_Q6_K:  return 256;
        case GGML_TYPE_Q2_K:  return 256;
        case GGML_TYPE_Q3_K:  return 256;
        default: return 1;
    }
}

static int64_t ggml_type_bytes_per_block(int type) {
    switch (type) {
        case GGML_TYPE_F32:   return 4;
        case GGML_TYPE_F16:   return 2;
        case GGML_TYPE_Q4_0:  return 18;     // 32 × 4-bit + 1 FP16 scale = 16 + 2
        case GGML_TYPE_Q4_1:  return 20;
        case GGML_TYPE_Q5_0:  return 22;
        case GGML_TYPE_Q5_1:  return 24;
        case GGML_TYPE_Q8_0:  return 34;     // 32 × 8-bit + 1 FP16 scale = 32 + 2
        case GGML_TYPE_Q8_1:  return 36;
        case GGML_TYPE_Q4_K:  return 144;    // 256 elements, mixed precision
        case GGML_TYPE_Q5_K:  return 176;
        case GGML_TYPE_Q6_K:  return 210;
        case GGML_TYPE_Q2_K:  return 84;
        case GGML_TYPE_Q3_K:  return 110;
        default: return 4;
    }
}

static int64_t tensor_bytes(int type, int64_t n_elements) {
    int64_t block_size = ggml_type_block_size(type);
    int64_t n_blocks = (n_elements + block_size - 1) / block_size;
    return n_blocks * ggml_type_bytes_per_block(type);
}

struct TensorInfo {
    char     name[256];
    uint32_t n_dims;
    int64_t  shape[4];
    uint32_t type;        // GGMLType
    uint64_t offset;      // offset from data start in file
};

// ── Parse GGUF tensor info section ───────────────────────────────────────
// Returns data_offset (where tensor data starts in the file)

static int64_t parse_tensor_infos(const std::string& path,
                                   std::vector<TensorInfo>& tensors) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return -1;

    // Read header
    uint32_t magic, version;
    uint64_t n_tensors, n_kv;
    fread(&magic, 4, 1, f);
    fread(&version, 4, 1, f);
    fread(&n_tensors, 8, 1, f);
    fread(&n_kv, 8, 1, f);

    // BUG #1 FIX: skip metadata KV pairs using exact GGUF type sizes
    // GGUF value types (from gguf spec):
    //   0=UINT8  1=INT8  2=UINT16 3=INT16 4=UINT32 5=INT32
    //   6=FLOAT32 7=BOOL 8=STRING 9=ARRAY 10=UINT64 11=INT64 12=FLOAT64

    // Helper: size of a scalar GGUF value type
    auto gguf_scalar_size = [](uint32_t type) -> int {
        switch (type) {
            case 0: case 1: case 7: return 1;   // uint8, int8, bool
            case 2: case 3:         return 2;   // uint16, int16
            case 4: case 5: case 6: return 4;   // uint32, int32, float32
            case 10: case 11: case 12: return 8; // uint64, int64, float64
            default: return 0;                   // string, array — variable
        }
    };

    for (uint64_t i = 0; i < n_kv; i++) {
        uint64_t key_len;
        fread(&key_len, 8, 1, f);
        fseek(f, key_len, SEEK_CUR);  // skip key string

        uint32_t vtype;
        fread(&vtype, 4, 1, f);

        int scalar_sz = gguf_scalar_size(vtype);
        if (scalar_sz > 0) {
            // Fixed-size scalar type — skip exact bytes
            fseek(f, scalar_sz, SEEK_CUR);
        } else if (vtype == 8) {
            // String: 8-byte length prefix + string data
            uint64_t slen;
            fread(&slen, 8, 1, f);
            fseek(f, slen, SEEK_CUR);
        } else if (vtype == 9) {
            // Array: 4-byte element type + 8-byte count + elements
            uint32_t atype;
            uint64_t alen;
            fread(&atype, 4, 1, f);
            fread(&alen, 8, 1, f);

            int elem_sz = gguf_scalar_size(atype);
            if (elem_sz > 0) {
                // Array of fixed-size scalars — skip all at once
                fseek(f, (long)(alen * elem_sz), SEEK_CUR);
            } else if (atype == 8) {
                // Array of strings — must read each length
                for (uint64_t a = 0; a < alen; a++) {
                    uint64_t sl;
                    fread(&sl, 8, 1, f);
                    fseek(f, sl, SEEK_CUR);
                }
            } else {
                // Nested array (rare) — skip with best guess
                fprintf(stderr, "[gguf] WARNING: nested array type %u, skipping\n", atype);
                for (uint64_t a = 0; a < alen; a++)
                    fseek(f, 8, SEEK_CUR);
            }
        } else {
            fprintf(stderr, "[gguf] WARNING: unknown value type %u at KV %lu\n", vtype, i);
            fseek(f, 8, SEEK_CUR);  // best guess
        }
    }

    // Now read tensor info entries
    tensors.resize(n_tensors);
    for (uint64_t t = 0; t < n_tensors; t++) {
        uint64_t name_len;
        fread(&name_len, 8, 1, f);
        memset(tensors[t].name, 0, sizeof(tensors[t].name));
        fread(tensors[t].name, 1, std::min(name_len, (uint64_t)255), f);
        if (name_len > 255) fseek(f, name_len - 255, SEEK_CUR);

        fread(&tensors[t].n_dims, 4, 1, f);
        for (uint32_t d = 0; d < tensors[t].n_dims; d++)
            fread(&tensors[t].shape[d], 8, 1, f);
        for (uint32_t d = tensors[t].n_dims; d < 4; d++)
            tensors[t].shape[d] = 1;

        fread(&tensors[t].type, 4, 1, f);
        fread(&tensors[t].offset, 8, 1, f);
    }

    // Data section starts at alignment boundary after tensor infos
    int64_t cur = ftell(f);
    int64_t alignment = 32;  // GGUF default alignment
    int64_t data_offset = (cur + alignment - 1) / alignment * alignment;

    fclose(f);

    fprintf(stderr, "[gguf] Parsed %zu tensor infos, data starts at offset %ld\n",
            tensors.size(), data_offset);
    return data_offset;
}

// ── Map tensor names to weight struct pointers ───────────────────────────

ModelWeights map_weights(const void* blob, int64_t blob_size, const ModelConfig& cfg) {
    ModelWeights mw = {};
    mw.n_layers = cfg.n_layers;
    mw.layers = new LayerWeights[cfg.n_layers];
    memset(mw.layers, 0, sizeof(LayerWeights) * cfg.n_layers);

    // We need to re-parse tensor infos to get offsets
    // (the blob is already mmap'd, we just need name→offset mapping)
    // For this, we read the GGUF header from the blob itself
    const char* base = (const char*)blob;

    // Parse header from blob
    uint32_t version = *(uint32_t*)(base + 4);
    uint64_t n_tensors = *(uint64_t*)(base + 8);
    uint64_t n_kv = *(uint64_t*)(base + 16);

    fprintf(stderr, "[model] Mapping %lu tensors to layer weights...\n", n_tensors);

    // For the mapping, we need to use the file-based parser since
    // navigating KV pairs in-memory is complex with variable-length fields.
    // The parse_tensor_infos function handles this.
    // NOTE: In a production runtime, you'd parse once and cache the result.

    // For now, return the allocated structure — the decode loop will check
    // for null pointers and error if weights aren't mapped.
    // The full tensor info parser (parse_tensor_infos) is implemented above
    // and can be called from load() to do the complete mapping.

    fprintf(stderr, "[model] Allocated %d layer structs (%zu bytes)\n",
            cfg.n_layers, sizeof(LayerWeights) * cfg.n_layers);
    return mw;
}

// ── Full weight loading with tensor mapping ──────────────────────────────
// Call this instead of map_weights for complete loading.

bool load_and_map_weights(const std::string& path, void** blob, int64_t* blob_size,
                          ModelWeights* mw, const ModelConfig& cfg) {
    // Step 1: Parse tensor infos from file
    std::vector<TensorInfo> tensors;
    int64_t data_offset = parse_tensor_infos(path, tensors);
    if (data_offset < 0) return false;

    // Step 2: mmap the file
    if (!load_gguf_weights(path, blob, blob_size)) return false;

    const char* data = (const char*)*blob + data_offset;

    // Step 3: Allocate layer structs
    mw->n_layers = cfg.n_layers;
    mw->layers = new LayerWeights[cfg.n_layers];
    memset(mw->layers, 0, sizeof(LayerWeights) * cfg.n_layers);

    // Step 4: Map each tensor name to the appropriate pointer
    int mapped = 0;
    for (auto& ti : tensors) {
        const void* ptr = data + ti.offset;
        int layer = -1;

        // Parse "blk.{N}.xxx" pattern
        if (sscanf(ti.name, "blk.%d.", &layer) == 1 && layer >= 0 && layer < cfg.n_layers) {
            auto& lw = mw->layers[layer];

            if (strstr(ti.name, "attn_q.weight"))       lw.wq = ptr;
            else if (strstr(ti.name, "attn_k.weight"))   lw.wk = ptr;
            else if (strstr(ti.name, "attn_v.weight"))   lw.wv = ptr;
            else if (strstr(ti.name, "attn_output.weight")) lw.wo = ptr;
            else if (strstr(ti.name, "ffn_gate.weight")) lw.w_gate = ptr;
            else if (strstr(ti.name, "ffn_up.weight"))   lw.w_up = ptr;
            else if (strstr(ti.name, "ffn_down.weight")) lw.w_down = ptr;
            else if (strstr(ti.name, "attn_norm.weight")) {
                lw.rms_attn = ptr;
                lw.rms_type = (ti.type == 0) ? 0 : 1;
                if (layer == 0) fprintf(stderr, "[model] blk.0.attn_norm type=%d offset=%lu\n", ti.type, ti.offset);
            }
            else if (strstr(ti.name, "ffn_norm.weight")) {
                lw.rms_ffn = ptr;
                lw.rms_type = (ti.type == 0) ? 0 : 1;
            }
            else continue;
            mapped++;
        }
        else if (strcmp(ti.name, "token_embd.weight") == 0) {
            mw->tok_embd = ptr;
            mw->embd_type = ti.type;  // store actual GGML type (0=F32, 1=F16, 12=Q4_K, etc.)
            fprintf(stderr, "[model] token_embd type=%d\n", ti.type);
            mapped++;
        }
        else if (strcmp(ti.name, "output_norm.weight") == 0) {
            mw->output_norm = ptr;
            fprintf(stderr, "[model] output_norm type=%d\n", ti.type);
            mapped++;
        }
        else if (strcmp(ti.name, "output.weight") == 0) {
            mw->output = ptr;
            mapped++;
        }
    }

    fprintf(stderr, "[model] Mapped %d / %zu tensors to weight structs\n",
            mapped, tensors.size());

    // Verify critical tensors are present
    if (!mw->tok_embd) fprintf(stderr, "[model] WARNING: token_embd.weight not found\n");
    if (!mw->output)   fprintf(stderr, "[model] WARNING: output.weight not found\n");
    for (int l = 0; l < cfg.n_layers; l++) {
        if (!mw->layers[l].wq)
            fprintf(stderr, "[model] WARNING: blk.%d.attn_q.weight not found\n", l);
    }

    return mapped > 0;
}

}  // namespace jllm
