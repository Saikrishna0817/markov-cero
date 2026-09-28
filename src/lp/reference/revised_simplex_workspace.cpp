#include "revised_simplex_internal.hpp"
namespace markov_cero::lp::reference {
using namespace detail_revised_simplex;
namespace detail_revised_simplex {
Work make_work(const transform::SparseCanonicalModel& m) {
    Work w;
    w.rows = m.matrix.rows;
    w.original_rows = w.rows;
    w.original_columns = m.matrix.columns;
    if (w.rows > maximum_rows || w.original_columns > maximum_columns) {
        throw std::length_error("simplex reference dimension limit exceeded");
    }
    w.total_columns = checked_add(w.original_columns, w.original_rows);
    w.a.resize(w.total_columns);
    w.b = m.rhs;
    w.row_sign.assign(w.rows, 1);
    w.row_origin.resize(w.rows);
    for (std::size_t i = 0; i < w.rows; ++i) {
        w.row_origin[i] = i;
        if (w.b[i] < 0) {
            w.b[i] = -w.b[i];
            w.row_sign[i] = -1;
        }
        w.a[w.original_columns + i].push_back({i, 1});
    }
    for (std::size_t j = 0; j < w.original_columns; ++j)
        for (auto k = m.matrix.column_offsets[j]; k < m.matrix.column_offsets[j + 1]; ++k) {
            const auto i = m.matrix.row_indices[k];
            w.a[j].push_back({i, w.row_sign[i] * m.matrix.values[k]});
        }
    return w;
}
}

namespace detail_revised_simplex {
bool crash_basis(Work& w, double tol, const linalg::SparseBasisOptions& s_opts) {
    w.basis.assign(w.rows, w.total_columns);
    for (std::size_t j = 0; j < w.original_columns; ++j) {
        if (w.a[j].size() == 1 && std::abs(w.a[j][0].second - 1.0) <= tol) {
            const auto i = w.a[j][0].first;
            if (w.basis[i] == w.total_columns) w.basis[i] = j;
        }
    }
    if (std::find(w.basis.begin(), w.basis.end(), w.total_columns) != w.basis.end()) return false;
    try {
        auto factor = make_factor(w, s_opts);
        auto xb = factor.solve(w.b);
        for (double v : xb) {
            if (v < -tol) {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}
}

namespace detail_revised_simplex {
void remove_row(Work& w, std::size_t victim) {
    for (auto& column : w.a) {
        std::erase_if(column, [victim](const auto& entry) { return entry.first == victim; });
        for (auto& [i, value] : column) { if (i > victim) --i; (void)value; }
    }
    w.b.erase(w.b.begin() + static_cast<std::ptrdiff_t>(victim));
    w.row_sign.erase(w.row_sign.begin() + static_cast<std::ptrdiff_t>(victim));
    w.row_origin.erase(w.row_origin.begin() + static_cast<std::ptrdiff_t>(victim));
    w.basis.erase(w.basis.begin() + static_cast<std::ptrdiff_t>(victim));
    --w.rows;
}
}

namespace detail_revised_simplex {
void remove_artificials(Work& w, double tol, double feas_tol) {
    for (std::size_t i = 0; i < w.rows;) {
        if (w.basis[i] < w.original_columns) {
            ++i;
            continue;
        }
        Options opts; opts.pivot_tolerance = tol;
        auto lu = make_factor(w, sparse_options(opts));
        std::vector<bool> basic(w.total_columns);
        for (auto j : w.basis) {
            basic[j] = true;
        }
        std::size_t entering = w.original_columns;
        for (std::size_t j = 0; j < w.original_columns; ++j) {
            if (basic[j]) {
                continue;
            }
            auto d = lu.solve(column(w, j));
            if (std::abs(d[i]) > tol) {
                // Verify candidate basis preserves primal feasibility
                const auto previous = w.basis[i];
                w.basis[i] = j;
                try {
                    auto test_lu = make_factor(w, sparse_options(opts));
                    w.basis[i] = previous;
                    auto test_xb = test_lu.solve(w.b);
                    bool feasible = true;
                    for (double val : test_xb) {
                        if (val < -feas_tol) {
                            feasible = false;
                            break;
                        }
                    }
                    if (feasible) {
                        entering = j;
                        break;
                    }
                } catch (...) {
                    w.basis[i] = previous;
                    continue;
                }
            }
        }
        if (entering < w.original_columns) {
            w.basis[i] = entering;
            ++i;
        } else {
            // Cannot safely replace without violating feasibility;
            // keep artificial in basis with cost 0 (Phase II enter_limit prevents artificial entry).
            ++i;
        }
    }
}
}

}
