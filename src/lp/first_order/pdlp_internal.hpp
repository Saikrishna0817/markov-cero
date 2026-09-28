#pragma once
// markov-cero: sovereign first-order LP engine
// Primal-Dual Hybrid Gradient (PDHG / PDLP)
// Grounding: Chambolle & Pock (2011); Applegate et al. (2021)
//            "Practical Large-Scale Linear Programming using Primal-Dual Hybrid Gradient"

#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/gpu/pdhg_step.hpp"
#include "markov_cero/gpu/device.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"
#include "markov_cero/transform/canonicalize.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>

namespace markov_cero::lp::first_order {
namespace detail_pdlp {}
namespace detail_pdlp {
struct UnscaledResiduals {
    double primal_infeas{0.0};
    double dual_infeas{0.0};
    double duality_gap{0.0};
    double score{0.0};
    std::vector<double> x;
    std::vector<double> y;
    double objective{0.0};
    // C-3: absolute dual objective in the engine's dual convention so callers
    // can form the complementarity gap |pobj - dobj| directly.
    double dual_objective{0.0};
};
}
namespace detail_pdlp {
struct CrossoverAttempt {
    std::optional<PdlpResult> result;
    bool basis_singular{false};
};
}
namespace detail_pdlp {
enum class BasisExtraction { ok, missing, singular };
}
namespace detail_pdlp { std::vector<double> spmv(const model::SparseMatrixCSC& A, const std::vector<double>& x); }
namespace detail_pdlp { std::vector<double> spmv_t(const model::SparseMatrixCSC& A, const std::vector<double>& y); }
namespace detail_pdlp { double project_bound(double x, const model::Bound& lo, const model::Bound& hi); }
namespace detail_pdlp { UnscaledResiduals compute_unscaled_residuals(
    const model::Model& model,
    const std::vector<double>& x_avg,
    const std::vector<double>& y_avg,
    const std::vector<double>& Ax_avg,
    const std::vector<double>& At_y_avg,
    const scale::RuizScalers& scalers,
    bool ruiz_scaling); }
namespace detail_pdlp { BasisExtraction extract_approximate_basis(const transform::CanonicalModel& canon,
                                           const std::vector<double>& z,
                                           lp::dual::BasisState& out_state); }
namespace detail_pdlp { CrossoverAttempt try_dual_simplex_crossover(
    const model::Model& model,
    const model::Model& mdl,
    const std::vector<double>& x_scaled,
    const std::vector<double>& y_scaled,
    const scale::RuizScalers& scalers,
    bool ruiz_scaling,
    const PdlpOptions& options,
    std::size_t iter,
    std::chrono::steady_clock::time_point t_start); }
PdlpResult iterate_pdlp(const model::Model& model, const model::Model& mdl,
    const scale::RuizScalers& scalers, const PdlpOptions& options,
    std::chrono::steady_clock::time_point t_start);
PdlpResult solve_pdlp(const model::Model& model, const PdlpOptions& options);
}

namespace markov_cero::lp::first_order::detail_pdlp {
PdlpResult solve_degenerate(const model::Model&, const PdlpOptions&);
}
