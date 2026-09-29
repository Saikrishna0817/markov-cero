#pragma once

// Structural byte accounting for the W01/IR-19 frontier benchmarks.
//
// Every helper reports allocated *capacity*, not logical size, so the
// numbers describe what the process actually holds for each storage class
// (root / worker / queue). Allocator metadata and std::string payloads are
// excluded; the benchmark instances leave every string empty (SSO, no heap
// allocation), so only vector arrays and object sizes are counted.

#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/milp/cut_pool.hpp"
#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <vector>

namespace frontier_storage_bytes {

inline std::size_t root(const markov_cero::model::Model& root) {
    const auto& matrix = root.matrix;
    return matrix.column_start.capacity() * sizeof(std::size_t) +
           matrix.row_index.capacity() * sizeof(std::size_t) +
           matrix.value.capacity() * sizeof(double) +
           root.objective.capacity() * sizeof(double) +
           root.variable_lower.capacity() * sizeof(markov_cero::model::Bound) +
           root.variable_upper.capacity() * sizeof(markov_cero::model::Bound) +
           root.variable_type.capacity() * sizeof(markov_cero::model::VariableType);
}

inline std::size_t root_nnz(const markov_cero::model::Model& root) {
    return root.matrix.value.size();
}

inline std::size_t basis(const markov_cero::lp::dual::BasisState& state) {
    return sizeof(state) + state.basic_variables.capacity() * sizeof(std::size_t);
}

inline std::size_t cuts(const std::vector<markov_cero::milp::Cut>& cuts) {
    std::size_t bytes = cuts.capacity() * sizeof(markov_cero::milp::Cut);
    for (const auto& cut : cuts) bytes += cut.coefficients.capacity() * sizeof(double);
    return bytes;
}

} // namespace frontier_storage_bytes
