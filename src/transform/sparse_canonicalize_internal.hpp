#pragma once
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace markov_cero::transform {
namespace detail_sparse_canonicalize {}

namespace detail_sparse_canonicalize { void ensure_finite(double v, const char* message); }
SparseCanonicalModel sparse_canonicalize(const model::Model& in, bool relax_integrality);
SparseCanonicalModel sparse_canonicalize(const model::Model& in, bool relax_integrality,
    const std::vector<model::Bound>& variable_lower,
    const std::vector<model::Bound>& variable_upper);
std::vector<double> reconstruct_primal(const SparseCanonicalModel& model,
                                       const std::vector<double>& canonical_primal);
double reconstruct_objective(const SparseCanonicalModel& model, double canonical_objective);
}
