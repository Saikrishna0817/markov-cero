#pragma once

// Internal (library-private) NLP helpers shared by the SQP solver and the
// KKT verifier. Not part of the public API surface.

#include "markov_cero/nlp/nlp_model.hpp"

#include <cstddef>
#include <vector>

namespace markov_cero::nlp {

/// Worst constraint violation at x: max(g_i(x), |h_j(x)|, bound violations).
[[nodiscard]] double constraint_violation(const NlpModel& model, const std::vector<double>& x);

/// l1 merit function phi(x; mu) = f(x) + mu * [sum max(0,g_i) + sum |h_j|].
[[nodiscard]] double merit_value(const NlpModel& model, const std::vector<double>& x, double mu);

/// Infinity norm helper.
[[nodiscard]] double inf_norm_of(const std::vector<double>& v);

/// Unified constraint values: [g (ineq)..., h (eq)...]; sizes reported via
/// the out params.
[[nodiscard]] std::vector<double> constraint_values(const NlpModel& model,
                                                    const std::vector<double>& x,
                                                    std::size_t& n_ineq, std::size_t& n_eq);

/// Unified constraint Jacobian rows in the same order as constraint_values.
[[nodiscard]] std::vector<std::vector<double>> constraint_jacobian(const NlpModel& model,
                                                                   const std::vector<double>& x,
                                                                   std::size_t& n_ineq,
                                                                   std::size_t& n_eq);

/// Project x onto the variable bounds (NaN-free finite bounds only).
void projectToBounds(const NlpModel& model, std::vector<double>& x);

} // namespace markov_cero::nlp
