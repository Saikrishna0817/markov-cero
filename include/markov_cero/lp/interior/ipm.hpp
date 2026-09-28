#pragma once

#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/transform/canonicalize.hpp"

#include <cstddef>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "markov_cero/transform/sparse_canonical_model.hpp"

namespace markov_cero::lp::interior {

// AP-1 (PS R4): interior-point engine. Mehrotra predictor-corrector primal-dual
// IPM on the canonical standard form
//
//     minimize c^T x   subject to   Ax = b,  x >= 0,
//
// with normal-equation Newton systems  (A D A^T) dy = rhs,  D = diag(x_j/s_j),
// factorized dense per iteration (same scale class as the dense verification
// oracle). The interior optimum is then converted to a certified vertex by the
// crossover below — an IPM without crossover produces no basis and would not
// satisfy R4 (see docs/research/concepts/Crossover.md).

struct Options {
    std::size_t iteration_limit{100};
    double relative_tolerance{1e-8};   // primal/dual residual + relative gap
    bool enable_crossover{true};
    std::optional<std::chrono::steady_clock::time_point> deadline;
};

struct Result {
    lp::reference::SolveStatus status{lp::reference::SolveStatus::numerical_failure};
    std::vector<double> primal;   // canonical x (vertex after crossover)
    std::vector<double> dual;     // canonical y
    double objective{0.0};
    std::size_t iterations{0};    // IPM iterations (crossover simplex excluded)
    // max|Uii|/min|Uii| of the last normal-equation factorization, replaced by
    // the certified crossover basis estimate when crossover applied. 0.0 = not
    // computed (no successful factorization).
    double condition_estimate{0.0};
    std::string message;

    // Crossover artifacts (present only when status == optimal and the
    // crossover produced a certified vertex basis).
    bool crossover_applied{false};
    std::optional<lp::dual::BasisState> basis_state;
};

// Crossover design (per docs/research/concepts/Crossover.md and
// [[Ye-1998-Crossover-Interior-Point]]): the engine CONSTRUCTS a candidate
// basis from the interior iterate (columns with large interior value first,
// with rank-completion fallback so the candidate is always nonsingular — the
// canonical model has no full slack identity block), then delegates
// certification of the vertex to the certified dual simplex via a warm start.
// If the candidate is rejected the interior optimum is kept (still optimal to
// IPM tolerance) and crossover_applied stays false — the result is never
// uncertified.
//
// Robustness (per [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]):
// the normal-equation system is factorized with a diagonal-perturbation
// fallback, and a solve that cannot be certified (numerical failure or an
// iteration limit) is reported honestly so the caller can apply the same
// engine-fallback policy the dual engine uses for an unusable warm start.
//
// Solves the canonical (minimization, Ax=b, x>=0) model using sparse normal equations.
[[nodiscard]] Result solve(const transform::SparseCanonicalModel& model, const Options& options = {});
[[nodiscard]] Result solve(const transform::CanonicalModel& model, const Options& options = {});

} // namespace markov_cero::lp::interior
