#include "markov_cero/presolve/presolve.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace markov_cero::presolve {
namespace {

void ensure_finite(double v, const char* message) {
    if (!std::isfinite(v)) {
        throw std::overflow_error(message);
    }
}

std::uint64_t hash_mix(std::uint64_t h, std::uint64_t v) {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

std::uint64_t value_key(double v) {
    return std::bit_cast<std::uint64_t>(v + 0.0);  // normalize -0.0
}

} // namespace

PresolveResult presolve(const transform::SparseCanonicalModel& input,
                        const PresolveOptions& options) {
    input.validate();

    PresolveResult result;
    result.status = lp::reference::SolveStatus::optimal;
    result.statistics.original_rows = input.matrix.rows;
    result.statistics.original_cols = input.matrix.columns;
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.model = input;
        result.message = "presolve wall-clock deadline reached before reduction";
        return result;
    }

    const std::size_t m = input.matrix.rows;
    const std::size_t n = input.matrix.columns;

    std::vector<bool> row_active(m, true);
    std::vector<bool> col_active(n, true);

    struct ColEntry {
        std::size_t row;
        double val;
    };
    std::vector<std::vector<ColEntry>> cols(n);
    for (std::size_t j = 0; j < n; ++j) {
        const std::size_t start = input.matrix.column_offsets[j];
        const std::size_t end = input.matrix.column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            cols[j].push_back({input.matrix.row_indices[k], input.matrix.values[k]});
        }
    }

    struct RowEntry {
        std::size_t col;
        double val;
    };
    std::vector<std::vector<RowEntry>> rows(m);
    for (std::size_t j = 0; j < n; ++j) {
        for (const auto& ce : cols[j]) {
            rows[ce.row].push_back({j, ce.val});
        }
    }

    std::vector<double> rhs = input.rhs;
    std::vector<double> obj = input.objective;
    double obj_offset = input.objective_offset;

    std::size_t pass = 0;
    for (; pass < options.max_passes; ++pass) {
        if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.model = input;
            result.message = "presolve wall-clock deadline reached between passes";
            result.statistics.passes_executed = pass;
            return result;
        }
        std::size_t reductions = 0;

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
                    return result;
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
                    return result;
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
                    return result;
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

        // 4. AP-9: forcing rows — every active entry strictly positive and the
        // rhs ~ 0. The equality sum(a_ij x_j) = b_i with x >= 0 then forces each
        // incident x_j to 0. The row is dropped and its dual is chosen at
        // postsolve; fixing a column at zero is always primal-feasible, and the
        // dual formula keeps every incident reduced cost dual-feasible.
        for (std::size_t i = 0; i < m; ++i) {
            if (!row_active[i]) {
                continue;
            }
            auto& r_entries = rows[i];
            bool all_positive = !r_entries.empty();
            bool any_removed = false;
            for (const auto& e : r_entries) {
                if (!col_active[e.col]) {
                    any_removed = true;
                    continue;
                }
                if (!(e.val > options.pivot_tolerance)) {
                    all_positive = false;
                    break;
                }
            }
            if (!all_positive || any_removed ||
                std::abs(rhs[i]) > options.feasibility_tolerance) {
                continue;
            }
            ForcingRowRecord rec;
            rec.original_row_index = i;
            for (const auto& e : r_entries) {
                rec.column_indices.push_back(e.col);
                rec.coefficients.push_back(e.val);
            }
            result.stack.push(std::move(rec));
            row_active[i] = false;
            for (const auto& e : r_entries) {
                col_active[e.col] = false;
                ++result.statistics.fixed_vars_removed;
            }
            ++result.statistics.forcing_rows_removed;
            ++reductions;
        }

        // 5. AP-9: duplicate rows — two active rows with identical (column,
        // value) patterns and identical rhs are redundant; keep the first, drop
        // the second (its constraint is implied). Postsolve assigns it dual 0,
        // which keeps the reduced-cost identities of every incident column
        // intact because the dropped row's dual contribution is zero.
        {
            std::unordered_map<std::uint64_t, std::vector<std::size_t>> candidates;
            for (std::size_t i = 0; i < m; ++i) {
                if (!row_active[i]) {
                    continue;
                }
                auto& r_entries = rows[i];
                r_entries.erase(
                    std::remove_if(r_entries.begin(), r_entries.end(),
                                   [&](const RowEntry& e) {
                                       return !col_active[e.col] ||
                                              std::abs(e.val) <= options.pivot_tolerance;
                                   }),
                    r_entries.end());
                std::uint64_t h = value_key(rhs[i]);
                for (const auto& e : r_entries) {
                    h = hash_mix(h, static_cast<std::uint64_t>(e.col));
                    h = hash_mix(h, value_key(e.val));
                }
                candidates[h].push_back(i);
            }
            for (auto& [h, group] : candidates) {
                (void)h;
                if (group.size() < 2) {
                    continue;
                }
                for (std::size_t g = 1; g < group.size(); ++g) {
                    const std::size_t keep = group[0];
                    const std::size_t drop = group[g];
                    bool identical = rhs[keep] == rhs[drop] &&
                                     rows[keep].size() == rows[drop].size();
                    if (identical) {
                        for (std::size_t k = 0; k < rows[keep].size() && identical; ++k) {
                            identical = rows[keep][k].col == rows[drop][k].col &&
                                        rows[keep][k].val == rows[drop][k].val;
                        }
                    }
                    if (identical) {
                        row_active[drop] = false;
                        ++result.statistics.duplicate_rows_removed;
                        ++reductions;
                    }
                }
            }
        }

        // 6. AP-9: dominated duplicate columns — proportional active (row,
        // value) patterns with a positive scale. Columns in one normalized
        // group satisfy a_b = k * a_a row-wise with k = s_b / s_a > 0
        // (s = first-entry value; the key normalizes by |s| and keeps the
        // sign of the first entry, so k > 0 inside a group). Producing
        // column b's row activity through a (x_a += k * x_b, x_b := 0) keeps
        // every row activity unchanged and costs obj_a * k * x_b, which is
        // <= obj_b * x_b exactly when e_a <= e_b for e_j = obj_j / |s_j|.
        // Canonical columns are non-negative with implicit +inf upper bound
        // (bounds ride in rows), so the scaled column stays feasible and the
        // optimal value is preserved. The dual certificate extends too: for
        // the optimal reduced dual pi with reduced costs d >= 0, the dropped
        // column's reduced cost is
        //   d_b = obj_b - k * (obj_a - d_a) = obj_b - k * obj_a + k * d_a >= 0
        // under the same dominance inequality. Keep the cheapest-per-scale
        // column of each group and fix the worse twins at zero; exact-cost
        // ties are interchangeable and keep the first.
        {
            std::unordered_map<std::uint64_t, std::vector<std::size_t>> candidates;
            for (std::size_t j = 0; j < n; ++j) {
                if (!col_active[j]) {
                    continue;
                }
                auto& c_entries = cols[j];
                c_entries.erase(
                    std::remove_if(c_entries.begin(), c_entries.end(),
                                   [&](const ColEntry& e) {
                                       return !row_active[e.row] ||
                                              std::abs(e.val) <= options.pivot_tolerance;
                                   }),
                    c_entries.end());
                if (c_entries.empty()) {
                    continue;
                }
                // Normalize the pattern by |first value|: proportional
                // columns collide, opposite-sign columns keep first entry
                // +1 vs -1 and stay apart (k must be positive).
                const double inv_scale = 1.0 / std::abs(c_entries[0].val);
                std::uint64_t h = 0;
                for (const auto& e : c_entries) {
                    h = hash_mix(h, static_cast<std::uint64_t>(e.row));
                    h = hash_mix(h, value_key(e.val * inv_scale));
                }
                candidates[h].push_back(j);
            }
            auto efficiency = [&](std::size_t j) {
                return obj[j] / std::abs(cols[j][0].val);
            };
            for (auto& [h, group] : candidates) {
                (void)h;
                if (group.size() < 2) {
                    continue;
                }
                std::size_t keeper = group[0];
                for (std::size_t g = 1; g < group.size(); ++g) {
                    if (efficiency(group[g]) < efficiency(keeper)) {
                        keeper = group[g];
                    }
                }
                for (std::size_t g = 0; g < group.size(); ++g) {
                    const std::size_t b = group[g];
                    if (b == keeper || !col_active[b]) {
                        continue;
                    }
                    // Re-verify proportionality — the hash is 64-bit and can
                    // collide; a false pairing must never drop a column.
                    const auto& ka = cols[keeper];
                    const auto& kb = cols[b];
                    if (ka.size() != kb.size()) {
                        continue;
                    }
                    const double k = kb[0].val / ka[0].val;
                    if (!(k > 0.0) || !std::isfinite(k)) {
                        continue;  // k > 0: replacement x_a += k * x_b stays feasible
                    }
                    bool proportional = true;
                    for (std::size_t i = 0; i < ka.size() && proportional; ++i) {
                        proportional = ka[i].row == kb[i].row &&
                                       kb[i].val == k * ka[i].val;
                    }
                    if (!proportional) {
                        continue;
                    }
                    if (efficiency(b) < efficiency(keeper)) {
                        continue;  // cannot happen (keeper is the argmin)
                    }
                    result.stack.push(EmptyColumnRecord{b, obj[b], 0.0});
                    col_active[b] = false;
                    ++result.statistics.dominated_cols_removed;
                    ++reductions;
                }
            }
        }

        if (reductions == 0) {
            break;
        }
    }
    result.statistics.passes_executed = pass;

    // Compact remaining active rows and columns
    std::vector<std::size_t> presolved_to_orig_row;
    std::vector<std::size_t> orig_to_presolved_row(m, static_cast<std::size_t>(-1));
    for (std::size_t i = 0; i < m; ++i) {
        if (row_active[i]) {
            orig_to_presolved_row[i] = presolved_to_orig_row.size();
            presolved_to_orig_row.push_back(i);
        }
    }

    std::vector<std::size_t> presolved_to_orig_col;
    std::vector<std::size_t> orig_to_presolved_col(n, static_cast<std::size_t>(-1));
    for (std::size_t j = 0; j < n; ++j) {
        if (col_active[j]) {
            orig_to_presolved_col[j] = presolved_to_orig_col.size();
            presolved_to_orig_col.push_back(j);
        }
    }

    result.stack.set_row_map(presolved_to_orig_row);
    result.stack.set_col_map(presolved_to_orig_col);

    const std::size_t new_m = presolved_to_orig_row.size();
    const std::size_t new_n = presolved_to_orig_col.size();
    result.statistics.presolved_rows = new_m;
    result.statistics.presolved_cols = new_n;

    // Build the compacted model
    result.model.matrix.rows = new_m;
    result.model.matrix.columns = new_n;
    result.model.rhs.resize(new_m);
    for (std::size_t new_i = 0; new_i < new_m; ++new_i) {
        result.model.rhs[new_i] = rhs[presolved_to_orig_row[new_i]];
    }

    result.model.objective.resize(new_n);
    for (std::size_t new_j = 0; new_j < new_n; ++new_j) {
        result.model.objective[new_j] = obj[presolved_to_orig_col[new_j]];
    }

    result.model.objective_offset = obj_offset;
    result.model.record = input.record;
    // AP-9 fix: the compaction drops canonical columns, so the variable map's
    // canonical_index entries must be remapped into the compacted space or
    // validate()'s range check (canonical_index < matrix.columns) rejects the
    // presolved model outright. Every column-elimination rule folds its
    // contribution into the presolved model (objective offset / rhs), so a
    // variable-map entry that references ONLY eliminated columns is a constant
    // in the presolved space: it is dropped from the map and its contribution
    // is already accounted for. Entries that reference surviving columns are
    // remapped index-by-index. The map is consumed only by reconstruction on
    // the ORIGINAL canonical model (postsolve and record of the input are
    // untouched), so this remap cannot affect solution fidelity.
    {
        std::vector<std::size_t> orig_to_presolved_col(
            n, static_cast<std::size_t>(-1));
        for (std::size_t new_j = 0; new_j < new_n; ++new_j) {
            orig_to_presolved_col[presolved_to_orig_col[new_j]] = new_j;
        }
        for (auto& vm : result.model.record.variables) {
            std::vector<std::size_t> remapped_index;
            std::vector<double> remapped_mult;
            remapped_index.reserve(vm.canonical_index.size());
            remapped_mult.reserve(vm.multiplier.size());
            for (std::size_t q = 0; q < vm.canonical_index.size(); ++q) {
                const std::size_t orig_idx = vm.canonical_index[q];
                if (orig_to_presolved_col[orig_idx] != static_cast<std::size_t>(-1)) {
                    remapped_index.push_back(orig_to_presolved_col[orig_idx]);
                    remapped_mult.push_back(vm.multiplier[q]);
                }
            }
            vm.canonical_index = std::move(remapped_index);
            vm.multiplier = std::move(remapped_mult);
        }
        // Canonical columns are ordered [structural..., slack...] and the
        // compaction preserves order, so the retained structural count is the
        // number of retained columns below the original structural count.
        std::size_t retained_structural = 0;
        for (std::size_t new_j = 0; new_j < new_n; ++new_j) {
            if (presolved_to_orig_col[new_j] < input.record.structural_variables) {
                ++retained_structural;
            }
        }
        result.model.record.structural_variables = retained_structural;
    }

    result.model.matrix.column_offsets.assign(new_n + 1, 0);
    std::size_t offset_count = 0;
    for (std::size_t new_j = 0; new_j < new_n; ++new_j) {
        result.model.matrix.column_offsets[new_j] = offset_count;
        const std::size_t orig_j = presolved_to_orig_col[new_j];
        for (const auto& ce : cols[orig_j]) {
            if (row_active[ce.row] && std::abs(ce.val) > options.pivot_tolerance) {
                const std::size_t new_i = orig_to_presolved_row[ce.row];
                result.model.matrix.row_indices.push_back(new_i);
                result.model.matrix.values.push_back(ce.val);
                ++offset_count;
            }
        }
    }
    result.model.matrix.column_offsets[new_n] = offset_count;

    if (new_m > 0 && new_n > 0) {
        result.model.validate();
    }
    result.message = "presolve complete";
    return result;
}

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

} // namespace markov_cero::presolve
