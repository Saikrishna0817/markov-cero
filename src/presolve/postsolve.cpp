#include "workspace.hpp"
namespace markov_cero::presolve {
using detail::ensure_finite;
lp::reference::Result postsolve(const PresolveStack& stack,
                                const lp::reference::Result& reduced_solution,
                                const transform::SparseCanonicalModel& original_model,
                                double /*tolerance*/) {

    lp::reference::Result restored = reduced_solution;
    const std::size_t m = original_model.matrix.rows;
    const std::size_t n = original_model.matrix.columns;

    restored.primal.assign(n, 0.0);
    restored.dual.assign(m, 0.0);

    // 1. Copy over un-eliminated reduced solution values
    const auto& col_map = stack.presolved_to_original_cols();
    for (std::size_t new_j = 0; new_j < col_map.size(); ++new_j) {
        if (new_j < reduced_solution.primal.size()) {
            restored.primal[col_map[new_j]] = reduced_solution.primal[new_j];
        }
    }

    const auto& row_map = stack.presolved_to_original_rows();
    for (std::size_t new_i = 0; new_i < row_map.size(); ++new_i) {
        if (new_i < reduced_solution.dual.size()) {
            restored.dual[row_map[new_i]] = reduced_solution.dual[new_i];
        }
    }

    // 2. Pop reduction records in reverse (LIFO) order
    const auto& records = stack.records();
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        std::visit(
            [&](const auto& rec) {
                using T = std::decay_t<decltype(rec)>;
                if constexpr (std::is_same_v<T, EmptyRowRecord>) {
                    restored.dual[rec.original_row_index] = 0.0;
                } else if constexpr (std::is_same_v<T, EmptyColumnRecord>) {
                    restored.primal[rec.original_col_index] = rec.fixed_value;
                } else if constexpr (std::is_same_v<T, FixedVariableRecord>) {
                    restored.primal[rec.original_col_index] = rec.fixed_value;
                } else if constexpr (std::is_same_v<T, RowSingletonRecord>) {
                    // Variable k was fixed by row i: a_{ik} x_k = b_i
                    // Satisfy dual optimality: pi_i = (c_k - sum_{r != i} a_{rk} pi_r) / a_{ik}
                    const std::size_t i = rec.original_row_index;
                    const std::size_t k = rec.variable_index;
                    const double a_ik = rec.coefficient;
                    const double c_k = original_model.objective[k];

                    long double sum_other = 0.0;
                    const std::size_t c_start = original_model.matrix.column_offsets[k];
                    const std::size_t c_end = original_model.matrix.column_offsets[k + 1];
                    for (std::size_t p = c_start; p < c_end; ++p) {
                        const std::size_t r = original_model.matrix.row_indices[p];
                        if (r != i) {
                            sum_other += static_cast<long double>(original_model.matrix.values[p]) *
                                         restored.dual[r];
                        }
                    }
                    const double pi_i = static_cast<double>((c_k - sum_other) / a_ik);
                    ensure_finite(pi_i, "non-finite dual multiplier in postsolve");
                    restored.dual[i] = pi_i;
                } else if constexpr (std::is_same_v<T, ForcingRowRecord>) {
                    // Row i forced every incident x_j to 0 (all a_ij > 0 and
                    // rhs_i ~ 0). Dual feasibility of the incident reduced costs
                    //     d_j = c_j - sum_{r != i} a_rj*pi_r - a_ij*pi_i >= 0
                    // with a_ij > 0 requires, for every incident j,
                    //     pi_i <= (c_j - sum_{r != i} a_rj*pi_r) / a_ij,
                    // so pi_i is the minimum of those ratios — the RowSingleton
                    // formula with the minimum taken over the incident columns.
                    // (Using c_j/a_ij alone would ignore the other rows' dual
                    // contributions and can violate dual feasibility, which the
                    // canonical witness check then rejects.)
                    const std::size_t i = rec.original_row_index;
                    double pi_i = std::numeric_limits<double>::infinity();
                    for (std::size_t k = 0; k < rec.column_indices.size(); ++k) {
                        const std::size_t j = rec.column_indices[k];
                        long double sum_other = 0.0L;
                        const std::size_t c_start = original_model.matrix.column_offsets[j];
                        const std::size_t c_end = original_model.matrix.column_offsets[j + 1];
                        for (std::size_t p = c_start; p < c_end; ++p) {
                            const std::size_t r = original_model.matrix.row_indices[p];
                            if (r != i) {
                                sum_other +=
                                    static_cast<long double>(
                                        original_model.matrix.values[p]) *
                                    restored.dual[r];
                            }
                        }
                        const double ratio = static_cast<double>(
                            (static_cast<long double>(original_model.objective[j]) -
                             sum_other) /
                            rec.coefficients[k]);
                        pi_i = std::min(pi_i, ratio);
                    }
                    ensure_finite(pi_i, "non-finite forcing-row dual in postsolve");
                    restored.dual[i] = pi_i;
                }
            },
            *it);
    }

    // 3. Recompute exact objective value in original canonical space
    long double exact_obj = original_model.objective_offset;
    for (std::size_t j = 0; j < n; ++j) {
        exact_obj += static_cast<long double>(original_model.objective[j]) * restored.primal[j];
    }
    restored.objective = static_cast<double>(exact_obj);
    ensure_finite(restored.objective, "non-finite objective in postsolve");

    return restored;
}

}
