#include "workspace.hpp"
namespace markov_cero::presolve::detail {
bool Workspace::eliminate_empty_and_singletons() {
        // 1. Scan for empty rows
        for (std::size_t i = 0; i < m; ++i) {
            if (!row_active[i]) {
                continue;
            }
            auto& r_entries = rows[i];
            r_entries.erase(std::remove_if(r_entries.begin(), r_entries.end(),
                                           [&](const RowEntry& e) {
                                               return !col_active[e.col] ||
                                                      std::abs(e.val) <= options.pivot_tolerance;
                                           }),
                            r_entries.end());

            if (r_entries.empty()) {
                if (std::abs(rhs[i]) > options.feasibility_tolerance) {
                    result.status = lp::reference::SolveStatus::infeasible;
                    result.message = "presolve: empty row with non-zero RHS detected";
                    return false;
                }
                row_active[i] = false;
                result.stack.push(EmptyRowRecord{i, rhs[i]});
                ++result.statistics.empty_rows_removed;
                ++reductions;
            }
        }

        // 2. Scan for empty columns
        for (std::size_t j = 0; j < n; ++j) {
            if (!col_active[j]) {
                continue;
            }
            auto& c_entries = cols[j];
            c_entries.erase(std::remove_if(c_entries.begin(), c_entries.end(),
                                           [&](const ColEntry& e) {
                                               return !row_active[e.row] ||
                                                      std::abs(e.val) <= options.pivot_tolerance;
                                           }),
                            c_entries.end());

            if (c_entries.empty()) {
                if (obj[j] < -options.dual_tolerance) {
                    result.status = lp::reference::SolveStatus::unbounded;
                    result.message = "presolve: empty column with negative cost detected";
                    return false;
                }
                col_active[j] = false;
                result.stack.push(EmptyColumnRecord{j, obj[j], 0.0});
                ++result.statistics.empty_cols_removed;
                ++reductions;
            }
        }

        // 3. Scan for row singletons
        for (std::size_t i = 0; i < m; ++i) {
            if (!row_active[i]) {
                continue;
            }
            auto& r_entries = rows[i];
            r_entries.erase(std::remove_if(r_entries.begin(), r_entries.end(),
                                           [&](const RowEntry& e) {
                                               return !col_active[e.col] ||
                                                      std::abs(e.val) <= options.pivot_tolerance;
                                           }),
                            r_entries.end());

            if (r_entries.size() == 1) {
                const auto entry = r_entries[0];
                const std::size_t j = entry.col;
                const double a_ij = entry.val;
                double fixed_val = rhs[i] / a_ij;

                if (fixed_val < -options.feasibility_tolerance) {
                    result.status = lp::reference::SolveStatus::infeasible;
                    result.message = "presolve: row singleton implies negative value for canonical "
                                     "non-negative variable";
                    return false;
                }
                if (fixed_val < 0.0) {
                    fixed_val = 0.0;
                }

                result.stack.push(RowSingletonRecord{i, j, a_ij, rhs[i]});
                row_active[i] = false;
                ++result.statistics.row_singletons_removed;
                ++reductions;

                // Fix variable j and substitute into other incident rows
                std::vector<std::size_t> inc_rows;
                std::vector<double> inc_coeffs;
                for (const auto& ce : cols[j]) {
                    if (row_active[ce.row]) {
                        rhs[ce.row] -= ce.val * fixed_val;
                        inc_rows.push_back(ce.row);
                        inc_coeffs.push_back(ce.val);
                    }
                }
                obj_offset += obj[j] * fixed_val;
                result.stack.push(FixedVariableRecord{j, fixed_val, obj[j], std::move(inc_rows),
                                                      std::move(inc_coeffs)});
                col_active[j] = false;
                ++result.statistics.fixed_vars_removed;
            }
        }

return true;
}
}
