#pragma once

#include "markov_cero/model/model.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace markov_cero::milp {
// MIP-01 contract §5.3: a GMI/MIR row may only be derived when every integer
// column it touches sits on the original integer lattice under the
// canonicalization — integral shift (offset) and unit scale (multiplier of
// magnitude one). sparse_canonicalize only ever emits ±1 multipliers, so the
// shift is the live condition; an integer variable with a non-integral bound
// (e.g. x >= 0.5 or an upper-bound-only x <= 3.7) lives on a shifted lattice
// in canonical space, and a row derived there would mix original-unit f0 with
// canonical-unit coefficients. Returns false when the generator must skip.
// row_coeffs are the structural (non-slack) coefficients of the candidate row.
inline bool cut_row_lattice_preserving(const transform::SparseCanonicalModel& canonical,
                                       const model::Model& model,
                                       const std::vector<double>& row_coeffs,
                                       std::size_t structural_count) {
    for (std::size_t orig_j = 0; orig_j < model.matrix.column_count; ++orig_j) {
        if (model.variable_type[orig_j] == model::VariableType::continuous) continue;
        const auto& vmap = canonical.record.variables[orig_j];
        for (std::size_t q = 0; q < vmap.canonical_index.size(); ++q) {
            const std::size_t c_idx = vmap.canonical_index[q];
            if (c_idx >= structural_count) continue;
            if (std::abs(row_coeffs[c_idx]) <= 1e-12) continue;
            const double off = vmap.offset;
            if (std::abs(off - std::round(off)) > 1e-9) return false;
            if (std::abs(std::abs(vmap.multiplier[q]) - 1.0) > 1e-12) return false;
        }
    }
    return true;
}
} // namespace markov_cero::milp
