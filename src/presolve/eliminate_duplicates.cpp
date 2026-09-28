#include "workspace.hpp"
namespace markov_cero::presolve::detail {
bool Workspace::eliminate_duplicates() {
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

return true;
}
}
