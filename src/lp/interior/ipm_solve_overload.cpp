#include "ipm_internal.hpp"
namespace markov_cero::lp::interior {
using namespace detail_ipm;
Result solve(const transform::CanonicalModel& model, const Options& options) {
    transform::SparseCanonicalModel sparse;
    sparse.matrix.rows = model.matrix.rows;
    sparse.matrix.columns = model.matrix.columns;
    sparse.matrix.column_offsets.assign(model.matrix.columns + 1, 0);
    sparse.rhs = model.rhs;
    sparse.objective = model.objective;
    sparse.objective_offset = model.objective_offset;
    sparse.record = model.record;
    for (std::size_t j = 0; j < model.matrix.columns; ++j) {
        sparse.matrix.column_offsets[j + 1] = sparse.matrix.column_offsets[j];
        for (std::size_t i = 0; i < model.matrix.rows; ++i) {
            const double val = model.matrix.values[i * model.matrix.columns + j];
            if (val != 0.0) {
                sparse.matrix.row_indices.push_back(i);
                sparse.matrix.values.push_back(val);
                ++sparse.matrix.column_offsets[j + 1];
            }
        }
    }
    sparse.validate();
    auto res = solve(sparse, options);
    // If crossover succeeded and produced a basis, recreate basis_state against the dense model
    if (res.crossover_applied && res.basis_state.has_value()) {
        try {
            res.basis_state = lp::dual::make_basis_state(model, res.basis_state->basic_variables);
        } catch (...) {}
    }
    return res;
}
}
