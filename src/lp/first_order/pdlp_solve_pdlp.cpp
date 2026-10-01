#include "pdlp_internal.hpp"
namespace markov_cero::lp::first_order {
using namespace detail_pdlp;
PdlpResult solve_pdlp(const model::Model& model, const PdlpOptions& options) {
    if (options.backend == Backend::gpu) {
        // gpu::solve_pdlp_gpu falls back to the CPU path itself when the host
        // has no usable device, so the label below stays honest either way.
        const bool has_device = gpu::is_gpu_available();
        auto result = gpu::solve_pdlp_gpu(model, options);
        result.backend_actually_used = has_device ? "cuda" : "cpu_fallback";
        return result;
    }

    const auto t_start = std::chrono::steady_clock::now();
    const std::size_t m = model.matrix.row_count;
    const std::size_t n = model.matrix.column_count;

    if (n == 0 || m == 0) return solve_degenerate(model, options);

    model::Model mdl = model;
    scale::RuizScalers scalers;
    if (options.ruiz_scaling) {
        scale::RuizOptions scale_options;
        scale_options.max_iterations = options.ruiz_iterations;
        scale_options.deadline = options.deadline;
        scalers = scale::equilibrate_model(mdl, scale_options);
    }
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        PdlpResult res;
        res.status = PdlpStatus::resource_limit;
        res.message = "wall-clock deadline reached during PDLP preprocessing";
        return res;
    }

    return iterate_pdlp(model, mdl, scalers, options, t_start);
}
}
