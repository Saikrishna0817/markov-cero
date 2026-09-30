#pragma once

#include "markov_cero/model/model.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <cstddef>

namespace markov_cero::api::detail {

// Coarse byte estimators behind the W02/IR-21 charge points. They are
// deliberately estimates: `memory_limit_bytes` meters the instrumented charge
// points only, not every allocation the host performs, and an over-estimate
// fails closed rather than letting a solve run past its budget.

inline std::size_t canonical_model_bytes(const transform::SparseCanonicalModel& model) {
    const std::size_t nonzeros = model.matrix.values.size();
    return nonzeros * (sizeof(double) + sizeof(std::size_t)) +
           model.matrix.column_offsets.size() * sizeof(std::size_t) +
           (model.rhs.size() + model.objective.size()) * sizeof(double) + 4096U;
}

} // namespace markov_cero::api::detail
