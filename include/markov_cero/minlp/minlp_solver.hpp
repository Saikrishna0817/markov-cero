#pragma once

#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"
#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace markov_cero::minlp {

using markov_cero::nlp::NlpModel;

// W1 / D-03 (LOCKED): Outer Approximation for CONVEX MINLP only
// (Duran & Grossmann 1986; Bonami et al. 2008 for the B&B variant).
//
//   minimize    f(x)
//   subject to  g(x) <= 0
//               x_j integer for j in integer_indices
//
// Master MILP accumulates linearization cuts
//   g_i(x^k) + grad g_i(x^k)^T (x - x^k) <= 0
// plus the objective linearization; the integer solution of the master becomes
// the next NLP subproblem start. Gap tolerance is LOCKED at 1e-3 (NLP accuracy
// is bounded by the SQP tolerance, looser than the LP/MILP 1e-4).
//
// Global convexity is screened structurally from the source MPS quadratic
// objective and every NLCON Hessian. Callback-only MINLPs are rejected because
// arbitrary callbacks do not expose a globally checkable quadratic structure.
// Equality callbacks are also rejected until the OA master represents them.

struct MinlpOptions {
    std::size_t max_iterations{50};
    double gap_tolerance{1e-3};          // LOCKED (plan W1/D-03)
    double feasibility_tolerance{1e-6};  // Independent primal-feasibility gate
    nlp::SqpOptions sqp_options{};
    std::size_t milp_max_nodes{20000};
    double milp_time_limit{30.0};
    bool verbose{false};
    std::optional<std::chrono::steady_clock::time_point> deadline;
};

struct MinlpSolution {
    lp::reference::SolveStatus status{lp::reference::SolveStatus::numerical_failure};
    std::string message;
    std::vector<double> x;
    double objective{0.0};
    double best_bound{std::numeric_limits<double>::quiet_NaN()};
    double relative_gap{std::numeric_limits<double>::infinity()};
    std::size_t iterations{0};
    std::size_t cuts_added{0};
    bool integer_feasible{false};
};

struct MinlpProblem {
    markov_cero::nlp::NlpModel nlp;      // retained for source/API compatibility
    std::vector<std::size_t> integer_indices;
    // Required. The solver rebuilds the NLP view from this model and checks
    // convexity of its quadratic structure before generating OA cuts.
    std::optional<model::Model> source_model;
};

[[nodiscard]] MinlpSolution solve_minlp(const MinlpProblem& problem,
                                        const std::vector<double>& x0,
                                        const MinlpOptions& options = {});

} // namespace markov_cero::minlp
