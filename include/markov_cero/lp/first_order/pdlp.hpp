#pragma once

// markov-cero: sovereign first-order LP engine
// Primal-Dual Hybrid Gradient (PDHG / PDLP)
// Grounding: Chambolle & Pock (2011); Applegate et al. (2021)

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace markov_cero::lp::first_order {

enum class Backend { cpu, gpu };
enum class RestartStrategy { none, adaptive, fixed };

struct PdlpOptions {
    std::size_t max_iterations{100000};
    std::size_t restart_every{40};
    double primal_tolerance{1e-4};
    double dual_tolerance{1e-4};
    double gap_tolerance{1e-4};
    double step_size_reduction{0.9};
    Backend backend{Backend::cpu};
    RestartStrategy restart_strategy{RestartStrategy::adaptive};
    double restart_reduction_factor{0.368};
    bool adaptive_step_size{true};
    bool adaptive_primal_weight{true};
    std::optional<std::chrono::steady_clock::time_point> deadline;
    double initial_primal_weight{0.0};
    double primal_weight_smoothing{0.5};
    bool ruiz_scaling{true};
    std::size_t ruiz_iterations{10};

    // Stagnation detection and dual simplex crossover
    bool enable_crossover{false};
    std::size_t stagnation_window{1000};
    double stagnation_threshold{0.999};
    double crossover_primal_tolerance{1e-4};
    double crossover_dual_tolerance{1e-4};
    std::size_t crossover_simplex_limit{100000};

    void set_tolerance(double tol) noexcept {
        primal_tolerance = tol;
        dual_tolerance = tol;
        gap_tolerance = tol;
    }
};

enum class PdlpStatus { optimal, iteration_limit, resource_limit, numerical_failure };

struct PdlpResult {
    PdlpStatus status{PdlpStatus::iteration_limit};
    std::vector<double> primal;
    std::vector<double> dual;
    double objective{0.0};
    double primal_infeasibility{0.0};
    double dual_infeasibility{0.0};
    double duality_gap{0.0};
    double tolerance{1e-4};
    std::size_t iterations{0};
    std::string message;
    double h2d_ms{0.0};
    double kernel_ms{0.0};
    double d2h_ms{0.0};
    double total_ms{0.0};

    // C-3: absolute dual objective of the reported iterate (engine dual
    // convention); complementarity gap = |objective - dual_objective|.
    double dual_objective{0.0};

    bool crossover_applied{false};
    // D-15: non-empty when the iterate stagnated and the dual-simplex
    // crossover could not certify a basis (singular extraction). Carried into
    // the JSON output as "convergence_note".
    std::string convergence_note{};
    // PDLP is matrix-free and factorizes nothing, so this is 0.0 unless the
    // dual-simplex crossover produced a certified basis (then it carries that
    // basis' pivot-ratio condition proxy).
    double condition_estimate{0.0};
    // Actual execution path, including CPU emulation of a GPU request.
    std::string backend_actually_used{"cpu"};
};

// Solve an LP using matrix-free Primal-Dual Hybrid Gradient.
// Uses only SpMV: A*x and A^T*y -- never factorizes a basis matrix.
[[nodiscard]] PdlpResult solve_pdlp(const model::Model& model, const PdlpOptions& options = {});

} // namespace markov_cero::lp::first_order
