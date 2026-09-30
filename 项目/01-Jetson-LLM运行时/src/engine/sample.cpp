// sample.cpp — Token sampling (top-k, top-p, temperature)
//
// Runs on CPU — logits are small (vocab_size floats) and sampling
// has branchy control flow that GPUs handle poorly.

#include "jllm_engine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <vector>
#include <random>

namespace jllm {

static thread_local std::mt19937 g_rng(42);

struct TokenProb {
    int   id;
    float prob;
};

// Apply temperature scaling
static void apply_temperature(float* logits, int n, float temperature) {
    if (temperature <= 0.0f || temperature == 1.0f) return;
    float inv_t = 1.0f / temperature;
    for (int i = 0; i < n; i++)
        logits[i] *= inv_t;
}

// Apply repetition penalty
static void apply_repeat_penalty(float* logits, const int* recent_tokens,
                                  int n_recent, float penalty) {
    if (penalty <= 1.0f) return;
    for (int i = 0; i < n_recent; i++) {
        int tok = recent_tokens[i];
        if (logits[tok] > 0) logits[tok] /= penalty;
        else                 logits[tok] *= penalty;
    }
}

// Top-K: keep only the K highest logits
static int top_k_filter(TokenProb* candidates, int n, int k) {
    if (k >= n) return n;
    std::partial_sort(candidates, candidates + k, candidates + n,
                      [](const TokenProb& a, const TokenProb& b) {
                          return a.prob > b.prob;
                      });
    return k;
}

// Top-P (nucleus): keep tokens until cumulative probability > p
static int top_p_filter(TokenProb* candidates, int n, float p) {
    // Already sorted by probability (descending) from top-k
    float cumsum = 0.0f;
    int cutoff = n;
    for (int i = 0; i < n; i++) {
        cumsum += candidates[i].prob;
        if (cumsum > p) {
            cutoff = i + 1;
            break;
        }
    }
    return cutoff;
}

// Sample from filtered distribution
static int sample_from(const TokenProb* candidates, int n) {
    // Compute total probability for renormalization
    float total = 0.0f;
    for (int i = 0; i < n; i++) total += candidates[i].prob;

    std::uniform_real_distribution<float> dist(0.0f, total);
    float r = dist(g_rng);

    float cumsum = 0.0f;
    for (int i = 0; i < n; i++) {
        cumsum += candidates[i].prob;
        if (cumsum >= r) return candidates[i].id;
    }
    return candidates[n - 1].id;
}

// Greedy: just pick the highest logit
static int sample_greedy(const float* logits, int n) {
    int best = 0;
    for (int i = 1; i < n; i++)
        if (logits[i] > logits[best]) best = i;
    return best;
}

// ── Public API ──────────────────────────────────────────────────────────

int sample_token(float* logits, int vocab_size, const GenParams& params,
                 const int* recent_tokens, int n_recent) {
    // Greedy at temperature 0
    if (params.temperature <= 0.0f)
        return sample_greedy(logits, vocab_size);

    // Apply penalties
    apply_repeat_penalty(logits, recent_tokens, n_recent, params.repeat_penalty);
    apply_temperature(logits, vocab_size, params.temperature);

    // Softmax (on CPU — small array)
    float max_val = *std::max_element(logits, logits + vocab_size);
    float sum = 0.0f;
    for (int i = 0; i < vocab_size; i++) {
        logits[i] = expf(logits[i] - max_val);
        sum += logits[i];
    }
    float inv_sum = 1.0f / sum;

    // Build candidates
    std::vector<TokenProb> candidates(vocab_size);
    for (int i = 0; i < vocab_size; i++) {
        candidates[i] = {i, logits[i] * inv_sum};
    }

    // Filter
    int n = top_k_filter(candidates.data(), vocab_size, params.top_k);
    n = top_p_filter(candidates.data(), n, params.top_p);

    return sample_from(candidates.data(), n);
}

}  // namespace jllm
