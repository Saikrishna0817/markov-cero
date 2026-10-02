#pragma once

#include "markov_cero/nlp/nlp_model.hpp"
#include "markov_cero/nlp/sqp_solver.hpp"

#include <string>
#include <vector>

namespace markov_cero::nlp {

// W1: independent KKT verification for NLP solutions (C4 zero-trust: a
// solver-declared optimum is not evidence). Checks at the returned point:
//   stationarity  ||grad f + J_g^T lam_g + J_h^T lam_h - z||_inf <= tol
//   feasibility   g(x) <= tol, |h(x)| <= tol
//   dual signs    lam_g >= -tol  (for the <= 0 convention)
//   complementarity  lam_g_i * max(0, -g_i(x)) approximately consistent

struct NlpVerificationReport {
    bool accepted{false};
    double stationarity_residual{0.0};
    double inequality_violation{0.0};
    double equality_violation{0.0};
    double worst_dual_sign{0.0};
    double complementarity_residual{0.0};
    std::string message;
};

struct NlpFeasibilityReport {
    bool feasible{false};
    double maximum_violation{0.0};
    // Split of maximum_violation (numerical-policy.md section 4): the
    // constraint-row side (max(0, g_i) over inequalities, |h_i| over
    // equalities) and the variable-bound side, measured separately so the
    // shared primal report can publish them in their own fields.
    // maximum_violation == max(maximum_constraint_violation,
    //                          maximum_bound_violation) on every path,
    // including rejection, where all three are +inf.
    double maximum_constraint_violation{0.0};
    double maximum_bound_violation{0.0};
    std::string message;
};

// Independently recompute all nonlinear/linear callback rows and variable
// bounds. This deliberately does not infer feasibility from the solver's
// status, KKT residual, or objective-gap tolerance.
[[nodiscard]] NlpFeasibilityReport verify_nlp_feasibility(
    const NlpModel& model, const std::vector<double>& x,
    double tolerance = 1e-6);

[[nodiscard]] NlpVerificationReport verify_nlp_solution(const NlpModel& model,
                                                        const SqpSolution& solution,
                                                        double tolerance = 1e-6);

} // namespace markov_cero::nlp
