// video_pipeline_template.cpp — oneTBB flow graph starter for AI/image pipelines
//
// Compile:
//   g++ -O2 -std=c++17 video_pipeline_template.cpp -ltbb -o video_pipeline
//
// With OpenCV (uncomment cv::Mat lines and link accordingly):
//   g++ -O2 -std=c++17 video_pipeline_template.cpp -ltbb $(pkg-config --cflags --libs opencv4) -o video_pipeline
//
// Pipeline topology:
//   input_node → limiter → preprocess → inference → postprocess → sequencer → output
//                  ▲                                                               │
//                  └───────────────── decrement ◄──────────────────────────────────┘

#include <oneapi/tbb/flow_graph.h>
#include <cstdio>
#include <vector>

using namespace oneapi::tbb::flow;

// ── Data types ────────────────────────────────────────────────────────────────

struct Frame {
    int sequence_number;
    std::vector<float> pixels;   // replace with cv::Mat for OpenCV
};

// ── Pipeline stage functions — replace with real implementations ──────────────

Frame preprocess(Frame f) {
    // resize, normalize, color conversion, etc.
    return f;
}

Frame run_inference(Frame f) {
    // CNN / YOLO / any model forward pass
    return f;
}

Frame postprocess(Frame f) {
    // draw bounding boxes, compute metrics, NMS, etc.
    return f;
}

void display_or_save(const Frame& f) {
    // cv::imshow("output", f.image) or write to file
    std::printf("frame %d done\n", f.sequence_number);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    // Simulated input frames — replace with camera/video capture
    const int TOTAL_FRAMES = 20;
    std::vector<Frame> frames;
    frames.reserve(TOTAL_FRAMES);
    for (int i = 0; i < TOTAL_FRAMES; ++i)
        frames.push_back({i, std::vector<float>(224 * 224 * 3, 0.5f)});

    const int MAX_INFLIGHT = 8;   // tune to available memory / GPU capacity
    graph g;

    // ── Source: emit one frame at a time (always serial) ─────────────────────
    int idx = 0;
    input_node<Frame> source(g, [&](oneapi::tbb::flow_control& fc) -> Frame {
        if (idx >= TOTAL_FRAMES) { fc.stop(); return {}; }
        return frames[idx++];
    });

    // ── Limiter: cap in-flight frames to bound memory usage ──────────────────
    // Placed before preprocessing so no more than MAX_INFLIGHT frames
    // are ever in the pipeline simultaneously.
    limiter_node<Frame> limiter(g, MAX_INFLIGHT);

    // ── Processing stages (parallel — independent frames) ────────────────────
    function_node<Frame, Frame> preprocess_node(g, unlimited, preprocess);
    function_node<Frame, Frame> inference_node (g, unlimited, run_inference);
    function_node<Frame, Frame> postprocess_node(g, unlimited, postprocess);

    // ── Sequencer: restore original frame order after parallel processing ─────
    sequencer_node<Frame> sequencer(g,
        [](const Frame& f) -> size_t { return f.sequence_number; }
    );

    // ── Output: display or write to file (serial — preserve order) ───────────
    function_node<Frame, continue_msg> output(g, serial,
        [](const Frame& f) -> continue_msg {
            display_or_save(f);
            return {};
        }
    );

    // ── Wire ──────────────────────────────────────────────────────────────────
    make_edge(source,          limiter);
    make_edge(limiter,         preprocess_node);
    make_edge(preprocess_node, inference_node);
    make_edge(inference_node,  postprocess_node);
    make_edge(postprocess_node, sequencer);
    make_edge(sequencer,       output);
    make_edge(output,          limiter.decrement);  // release slot when frame is done

    // ── Run ───────────────────────────────────────────────────────────────────
    source.activate();
    g.wait_for_all();

    return 0;
}
