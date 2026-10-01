#pragma once

#include "markov_cero/nlp/nlp_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace markov_cero::nlp {

// NLP-01 contract nlp-local-sqp.md §3: developer-only finite-difference
// diagnostic for user callbacks. Never called inside the production solve
// path and never used to replace analytic derivatives.

struct DerivativeReport {
    bool gradient_ok{true};
    bool jacobian_ok{true};
    double worst_gradient_error{0.0};
    double worst_jacobian_error{0.0};
    double base_step{0.0};
    std::size_t gradient_checks{0};
    std::size_t jacobian_checks{0};
    std::string message;

    [[nodiscard]] bool passed() const noexcept { return gradient_ok && jacobian_ok; }
};

/// Compares the analytic gradient and constraint Jacobian at x against
/// centered finite differences in the interior and one-sided differences
/// within one step of a finite bound. Step rule: h_j =
/// (step_hint > 0 ? step_hint : cbrt(eps)) * max(1, |x_j|), shrunk to fit
/// finite bounds (coordinates with no room in either direction are skipped).
/// Pass verdict: |analytic - fd| <= 1e-5 + 1e-4 * max(|fd|, |analytic|).
[[nodiscard]] DerivativeReport check_derivatives(const NlpModel& model,
                                                  const std::vector<double>& x,
                                                  double step_hint = 0.0);

} // namespace markov_cero::nlp
