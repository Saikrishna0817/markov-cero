#pragma once

// NLP-01 contract docs/contracts/nlp-local-sqp.md §2: fail-closed validation
// of user callback output plus the callback-evaluation counter (§2.4).
// Library-internal; shared by the SQP solver, its helpers and the verifier.

#include "markov_cero/nlp/nlp_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace markov_cero::nlp {

/// Thrown when a user callback violates the §2 contract (dimension,
/// finiteness). Callers on the solve path map it to NumericalFailure.
struct CallbackError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// Validates a returned gradient: exact model.n_vars rows and finiteness.
void require_gradient(const NlpModel& model, const std::vector<double>& grad);

/// Validates a returned objective value (finite).
void require_objective(double value);

namespace detail {

/// Increments the per-solve user-callback invocation counter.
void note_callback_evaluation() noexcept;
/// Clears the counter at solve start.
void reset_callback_evaluations() noexcept;
/// Reads and clears the counter (taken when the solve reports its result).
std::size_t take_callback_evaluations() noexcept;

} // namespace detail
} // namespace markov_cero::nlp
