#include "minlp_solver_internal.hpp"
namespace markov_cero::minlp {
using namespace detail_minlp_solver;
namespace detail_minlp_solver {
qp::SparseSymmetricMatrix make_symmetric_matrix(std::size_t n,
                                                 const SymmetricEntries& entries) {
    qp::SparseSymmetricMatrix matrix;
    matrix.dimension = n;
    matrix.column_offsets.reserve(n + 1);
    matrix.column_offsets.push_back(0);
    std::vector<std::map<std::size_t, double>> columns(n);
    for (const auto& [ij, value] : entries) {
        if (value != 0.0)
            columns[ij.second][ij.first] += value;
    }
    for (std::size_t j = 0; j < n; ++j) {
        for (const auto& [i, value] : columns[j]) {
            if (value != 0.0) {
                matrix.row_indices.push_back(i);
                matrix.values.push_back(value);
            }
        }
        matrix.column_offsets.push_back(matrix.values.size());
    }
    return matrix;
}
}

namespace detail_minlp_solver {
qp::ConvexityReport hessian_convexity(std::size_t n,
                                     const std::vector<model::NlobjTerm>& terms,
                                     const SymmetricEntries& base, double sign) {
    SymmetricEntries entries = base;
    for (const auto& term : terms) {
        if (!term.quadratic)
            continue;
        const auto i = std::min(term.var0, term.var1);
        const auto j = std::max(term.var0, term.var1);
        // check_convexity expects the Hessian itself (as does the QP P
        // matrix): c*x_i^2 contributes 2c on the diagonal, while
        // c*x_i*x_j contributes c to both symmetric off-diagonal entries.
        entries[{i, j}] += sign * term.coefficient * (i == j ? 2.0 : 1.0);
    }
    return qp::assess_convexity(make_symmetric_matrix(n, entries), 1e-10);
}
}

namespace detail_minlp_solver {
void require_convex_quadratic_structure(const model::Model& source) {
    source.validate();
    if (source.nlp_callbacks) {
        throw UnsupportedMinlp(
            "minlp: cannot check convexity of arbitrary callback companions; "
            "use structurally represented quadratic MPS functions");
    }
    const std::size_t n = source.matrix.column_count;
    if (n > 512) {
        throw UnsupportedMinlp(
            "minlp: structural convexity screening is limited to 512 variables");
    }

    const auto qp_model = qp::make_quadratic_model(source);
    SymmetricEntries qp_objective;
    for (std::size_t j = 0; j < qp_model.P.dimension; ++j) {
        for (std::size_t k = qp_model.P.column_offsets[j]; k < qp_model.P.column_offsets[j + 1]; ++k) {
            qp_objective[{qp_model.P.row_indices[k], j}] += qp_model.P.values[k];
        }
    }
    const double objective_sign = source.objective_sense == model::ObjectiveSense::maximize
                                      ? -1.0
                                      : 1.0;
    const auto objective_convexity =
        hessian_convexity(n, source.nlobj_terms, qp_objective, objective_sign);
    if (objective_convexity.status == qp::ConvexityStatus::non_convex) {
        throw NonConvexMinlp(
            "minlp: objective Hessian is not positive semidefinite; non-convex MINLP is unsupported");
    }
    if (objective_convexity.status != qp::ConvexityStatus::positive_semidefinite) {
        throw UnsupportedMinlp("minlp: objective convexity could not be certified: " +
                               objective_convexity.message);
    }
    for (const auto& constraint : source.nlcon_constraints) {
        const auto constraint_convexity = hessian_convexity(n, constraint.terms, {}, 1.0);
        if (constraint_convexity.status == qp::ConvexityStatus::non_convex) {
            throw NonConvexMinlp("minlp: NLCON Hessian is not positive semidefinite: " +
                                 constraint.name);
        }
        if (constraint_convexity.status != qp::ConvexityStatus::positive_semidefinite) {
            throw UnsupportedMinlp("minlp: convexity of NLCON could not be certified: " +
                                   constraint.name + ": " + constraint_convexity.message);
        }
    }
}
}

namespace detail_minlp_solver {
model::Model build_master(const NlpModel& nlp,
                          const std::vector<std::size_t>& integer_indices,
                          const std::vector<std::vector<double>>& obj_grads,
                          const std::vector<double>& obj_rhs,
                          const std::vector<std::vector<double>>& cut_grads,
                          const std::vector<double>& cut_rhs) {
    const std::size_t n = nlp.n_vars;
    const std::size_t n_rows = obj_grads.size() + cut_grads.size();
    const std::size_t n_cols = n + 1;  // + eta

    model::Model master;
    master.name = "minlp_oa_master";
    master.objective_sense = model::ObjectiveSense::minimize;
    master.objective.assign(n_cols, 0.0);
    master.objective[n] = 1.0;  // min eta

    model::SparseMatrixBuilder builder(n_rows, n_cols);
    std::size_t r = 0;
    for (std::size_t k = 0; k < obj_grads.size(); ++k, ++r) {
        for (std::size_t j = 0; j < n; ++j) {
            if (obj_grads[k][j] != 0.0) {
                builder.add(r, j, obj_grads[k][j]);
            }
        }
        builder.add(r, n, -1.0);
    }
    for (std::size_t k = 0; k < cut_grads.size(); ++k, ++r) {
        for (std::size_t j = 0; j < n; ++j) {
            if (cut_grads[k][j] != 0.0) {
                builder.add(r, j, cut_grads[k][j]);
            }
        }
    }
    master.matrix = builder.build();

    master.row_lower.assign(n_rows, model::Bound::negative_infinity());
    master.row_upper.resize(n_rows);
    for (std::size_t k = 0; k < obj_grads.size(); ++k) {
        master.row_upper[k] = model::Bound::finite(obj_rhs[k]);
    }
    for (std::size_t k = 0; k < cut_grads.size(); ++k) {
        master.row_upper[obj_grads.size() + k] = model::Bound::finite(cut_rhs[k]);
    }

    master.variable_lower.resize(n_cols);
    master.variable_upper.resize(n_cols);
    master.variable_type.assign(n_cols, model::VariableType::continuous);
    for (std::size_t j = 0; j < n; ++j) {
        const double lb = nlp.bound_lower(j);
        const double ub = nlp.bound_upper(j);
        master.variable_lower[j] = std::isfinite(lb) ? model::Bound::finite(lb)
                                                     : model::Bound::negative_infinity();
        master.variable_upper[j] = std::isfinite(ub) ? model::Bound::finite(ub)
                                                     : model::Bound::positive_infinity();
    }
    // Epigraph variable: free (bound rows already under-approximate f).
    master.variable_lower[n] = model::Bound::negative_infinity();
    master.variable_upper[n] = model::Bound::positive_infinity();

    for (std::size_t idx : integer_indices) {
        master.variable_type[idx] = model::VariableType::integer;
    }

    master.row_name.resize(n_rows);
    for (std::size_t k = 0; k < n_rows; ++k) {
        master.row_name[k] = "oa_row_" + std::to_string(k);
    }
    master.variable_name.resize(n_cols);
    for (std::size_t j = 0; j < n; ++j) {
        master.variable_name[j] = "x" + std::to_string(j);
    }
    master.variable_name[n] = "eta";
    return master;
}
}

namespace detail_minlp_solver {
NlpModel with_fixed_integers(const NlpModel& nlp,
                             const std::vector<std::size_t>& integer_indices,
                             const std::vector<double>& assignment) {
    NlpModel fixed = nlp;
    fixed.lower_bounds = nlp.lower_bounds;
    fixed.upper_bounds = nlp.upper_bounds;
    if (fixed.lower_bounds.size() < nlp.n_vars) {
        fixed.lower_bounds.resize(nlp.n_vars, -kInf);
    }
    if (fixed.upper_bounds.size() < nlp.n_vars) {
        fixed.upper_bounds.resize(nlp.n_vars, kInf);
    }
    for (std::size_t idx : integer_indices) {
        const double v = assignment[idx];
        fixed.lower_bounds[idx] = v;
        fixed.upper_bounds[idx] = v;
    }
    return fixed;
}
}

}
