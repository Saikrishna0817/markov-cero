#include "ipm_internal.hpp"
namespace markov_cero::lp::interior {
using namespace detail_ipm;
Result solve(const transform::CanonicalModel& model, const Options& options) {
    // Dense adapter: shared conversion, then the sparse primary.
    transform::SparseCanonicalModel sparse = transform::sparse_from_dense(model);
    auto res = solve(sparse, options);
    // If crossover succeeded and produced a basis, recreate basis_state against the sparse model
    if (res.crossover_applied && res.basis_state.has_value()) {
        try {
            res.basis_state = lp::dual::make_basis_state(sparse, res.basis_state->basic_variables);
        } catch (...) {}
    }
    return res;
}
}
