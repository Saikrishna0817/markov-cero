#include "pdlp_internal.hpp"
namespace markov_cero::lp::first_order {
using namespace detail_pdlp;
namespace detail_pdlp {
std::vector<double> spmv(const model::SparseMatrixCSC& A, const std::vector<double>& x) {
    std::vector<double> y(A.row_count, 0.0);
    for (std::size_t col = 0; col < A.column_count; ++col) {
        const double xc = x[col];
        if (xc == 0.0) {
            continue;
        }
        for (std::size_t ptr = A.column_start[col]; ptr < A.column_start[col + 1]; ++ptr) {
            y[A.row_index[ptr]] += A.value[ptr] * xc;
        }
    }
    return y;
}
}

namespace detail_pdlp {
std::vector<double> spmv_t(const model::SparseMatrixCSC& A, const std::vector<double>& y) {
    std::vector<double> z(A.column_count, 0.0);
    for (std::size_t col = 0; col < A.column_count; ++col) {
        double s = 0.0;
        for (std::size_t ptr = A.column_start[col]; ptr < A.column_start[col + 1]; ++ptr) {
            s += A.value[ptr] * y[A.row_index[ptr]];
        }
        z[col] = s;
    }
    return z;
}
}

namespace detail_pdlp {
double project_bound(double x, const model::Bound& lo, const model::Bound& hi) {
    double lo_val = (lo.kind == model::BoundKind::negative_infinity) ? -1e300 : lo.value;
    double hi_val = (hi.kind == model::BoundKind::positive_infinity) ? +1e300 : hi.value;
    return std::clamp(x, lo_val, hi_val);
}
}

namespace detail_pdlp {
UnscaledResiduals compute_unscaled_residuals(
    const model::Model& model,
    const std::vector<double>& x_avg,
    const std::vector<double>& y_avg,
    const std::vector<double>& Ax_avg,
    const std::vector<double>& At_y_avg,
    const scale::RuizScalers& scalers,
    bool ruiz_scaling) {
    const std::size_t m = model.matrix.row_count;
    const std::size_t n = model.matrix.column_count;
    const double obj_sign = (model.objective_sense == model::ObjectiveSense::maximize)
                                ? -1.0 : 1.0;

    UnscaledResiduals res;
    res.x.resize(n);
    res.y.resize(m);
    for (std::size_t j = 0; j < n; ++j) {
        res.x[j] = ruiz_scaling ? (x_avg[j] * scalers.col_scale[j]) : x_avg[j];
    }
    for (std::size_t i = 0; i < m; ++i) {
        res.y[i] = ruiz_scaling ? (y_avg[i] * scalers.row_scale[i]) : y_avg[i];
    }

    double max_prim_viol = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        const double ax_i = ruiz_scaling ? (Ax_avg[i] / scalers.row_scale[i]) : Ax_avg[i];
        const double lo = (model.row_lower[i].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.row_lower[i].value;
        const double hi = (model.row_upper[i].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.row_upper[i].value;
        const double proj = std::clamp(ax_i, lo, hi);
        const double r = std::abs(ax_i - proj);
        const double scale = 1.0 + std::max(std::abs(ax_i),
                                            std::abs(proj) < 1e299 ? std::abs(proj) : 0.0);
        max_prim_viol = std::max(max_prim_viol, r / scale);
    }
    for (std::size_t j = 0; j < n; ++j) {
        const double lo = (model.variable_lower[j].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.variable_lower[j].value;
        const double hi = (model.variable_upper[j].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.variable_upper[j].value;
        const double proj = std::clamp(res.x[j], lo, hi);
        const double r = std::abs(res.x[j] - proj);
        const double scale = 1.0 + std::max(std::abs(res.x[j]),
                                            std::abs(proj) < 1e299 ? std::abs(proj) : 0.0);
        max_prim_viol = std::max(max_prim_viol, r / scale);
    }
    res.primal_infeas = max_prim_viol;

    double c_scale = 1.0;
    double dual_res_sq = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        const double at_y = ruiz_scaling ? (At_y_avg[j] / scalers.col_scale[j]) : At_y_avg[j];
        const double c_orig = obj_sign * model.objective[j];
        c_scale = std::max(c_scale, std::abs(c_orig));
        const double g = c_orig + at_y;
        const double lo = (model.variable_lower[j].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.variable_lower[j].value;
        const double hi = (model.variable_upper[j].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.variable_upper[j].value;
        const double x_proj = std::clamp(res.x[j] - g, lo, hi);
        const double diff = res.x[j] - x_proj;
        dual_res_sq += diff * diff;
    }
    res.dual_infeas = std::sqrt(dual_res_sq) / c_scale;

    double primal_obj = model.objective_offset;
    for (std::size_t j = 0; j < n; ++j) {
        primal_obj += model.objective[j] * res.x[j];
    }
    res.objective = primal_obj;

    double dual_obj = model.objective_offset;
    for (std::size_t i = 0; i < m; ++i) {
        const double lo = (model.row_lower[i].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.row_lower[i].value;
        const double hi = (model.row_upper[i].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.row_upper[i].value;
        if (res.y[i] > 0.0 && hi < 1e299) {
            dual_obj -= res.y[i] * hi;
        } else if (res.y[i] < 0.0 && lo > -1e299) {
            dual_obj -= res.y[i] * lo;
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        const double at_y = ruiz_scaling ? (At_y_avg[j] / scalers.col_scale[j]) : At_y_avg[j];
        const double c_orig = obj_sign * model.objective[j];
        const double g = c_orig + at_y;
        const double lo = (model.variable_lower[j].kind == model::BoundKind::negative_infinity)
                              ? -1e300 : model.variable_lower[j].value;
        const double hi = (model.variable_upper[j].kind == model::BoundKind::positive_infinity)
                              ? +1e300 : model.variable_upper[j].value;
        if (g > 0.0 && lo > -1e299) {
            dual_obj += g * lo;
        } else if (g < 0.0 && hi < 1e299) {
            dual_obj += g * hi;
        }
    }
    res.duality_gap = std::abs(primal_obj - dual_obj) /
                      (1.0 + std::abs(primal_obj) + std::abs(dual_obj));
    res.dual_objective = dual_obj;
    res.score = std::max({res.primal_infeas, res.dual_infeas, res.duality_gap});
    return res;
}
}

namespace detail_pdlp {
BasisExtraction extract_approximate_basis(const transform::CanonicalModel& canon,
                                           const std::vector<double>& z,
                                           lp::dual::BasisState& out_state) {
    const std::size_t m = canon.matrix.rows;
    const std::size_t n = canon.matrix.columns;

    // Rank-revealing column selection: prefer large z_j > 1e-7 first
    const double xi = 1e-7;
    std::vector<std::size_t> large;
    std::vector<std::size_t> rest;
    for (std::size_t j = 0; j < n; ++j) {
        (z[j] > xi ? large : rest).push_back(j);
    }
    std::sort(large.begin(), large.end(), [&](std::size_t a, std::size_t b) {
        return z[a] > z[b];
    });

    struct Pivot {
        std::size_t row;
        std::vector<double> column;
    };
    std::vector<Pivot> pivots;
    pivots.reserve(m);
    std::vector<char> row_used(m, 0);
    std::vector<std::size_t> basis;
    basis.reserve(m);

    const auto try_insert = [&](std::size_t j) {
        std::vector<double> c(m);
        double norm = 0.0;
        for (std::size_t i = 0; i < m; ++i) {
            c[i] = canon.matrix(i, j);
            norm = std::max(norm, std::abs(c[i]));
        }
        if (norm == 0.0) return;
        for (const Pivot& p : pivots) {
            const double factor = c[p.row];
            if (factor == 0.0) continue;
            for (std::size_t i = 0; i < m; ++i) {
                c[i] -= factor * p.column[i];
            }
        }
        const double threshold = 1e-10 * std::max(1.0, norm);
        std::size_t pivot_row = m;
        double best = threshold;
        for (std::size_t i = 0; i < m; ++i) {
            if (!row_used[i] && std::abs(c[i]) > best) {
                best = std::abs(c[i]);
                pivot_row = i;
            }
        }
        if (pivot_row == m) return;
        const double piv = c[pivot_row];
        for (std::size_t i = 0; i < m; ++i) {
            c[i] /= piv;
        }
        row_used[pivot_row] = 1;
        pivots.push_back(Pivot{pivot_row, std::move(c)});
        basis.push_back(j);
    };

    for (std::size_t j : large) {
        if (basis.size() == m) break;
        try_insert(j);
    }
    for (std::size_t j : rest) {
        if (basis.size() == m) break;
        try_insert(j);
    }
    if (basis.size() != m) return BasisExtraction::missing;

    // Verify basis non-singularity before the dual simplex warm start sees it.
    std::vector<std::vector<double>> basis_cols(m, std::vector<double>(m, 0.0));
    for (std::size_t p = 0; p < m; ++p) {
        const std::size_t col_idx = basis[p];
        for (std::size_t i = 0; i < m; ++i) {
            basis_cols[p][i] = canon.matrix(i, col_idx);
        }
    }
    auto basis_csc = linalg::SparseCsc::from_columns(m, basis_cols);
    try {
        (void)linalg::SparseLu::factorize(basis_csc, 1e-14);
    } catch (...) {
        return BasisExtraction::singular;
    }
    out_state = lp::dual::make_basis_state(canon, basis);
    return BasisExtraction::ok;
}
}

}
