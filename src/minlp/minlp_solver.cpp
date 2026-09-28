#include "markov_cero/minlp/minlp_solver.hpp"

#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/qp/model.hpp"

#include "../nlp/nlp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace markov_cero::minlp {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using SymmetricEntries = std::map<std::pair<std::size_t, std::size_t>, double>;

class UnsupportedMinlp final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class NonConvexMinlp final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

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

// Build the OA master MILP in epigraph form (D-03 LOCKED; Duran & Grossmann
// 1986). Variables: x_0..x_{n-1} plus epigraph variable eta (index n).
//
//   minimize   eta
//   s.t.       grad f(x^k)^T x - eta <= grad f(x^k)^T x^k - f(x^k)   (obj rows)
//              J_i(x^k)^T x     <= J_i(x^k)^T x^k - g_i(x^k)          (cuts)
//              x bounds; x_j integer for j in integer_indices; eta free
//
// For convex f and g every row is a valid global under-approximation, so the
// master optimum is a valid LOWER bound; each NLP subproblem point supplies
// one new row set, and the bound rises monotonically until the gap closes.
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

// Copy of the model with integer variables pinned to `assignment` (the OA NLP
// subproblem: integers fixed by the master, continuous variables optimized).
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

} // namespace

MinlpSolution solve_minlp(const MinlpProblem& problem, const std::vector<double>& x0,
                          const MinlpOptions& options) {
    MinlpSolution out;
    if (options.max_iterations == 0 || !std::isfinite(options.gap_tolerance) ||
        options.gap_tolerance <= 0.0 || !std::isfinite(options.feasibility_tolerance) ||
        options.feasibility_tolerance <= 0.0 ||
        !std::isfinite(options.sqp_options.kkt_tolerance) ||
        options.sqp_options.kkt_tolerance <= 0.0 || options.milp_max_nodes == 0 ||
        !std::isfinite(options.milp_time_limit) || options.milp_time_limit <= 0.0) {
        out.status = lp::reference::SolveStatus::invalid_options;
        out.message = "minlp: iteration, tolerance, and master limits must be positive and finite";
        return out;
    }
    NlpModel nlp = problem.nlp;
    if (!problem.source_model) {
        out.status = lp::reference::SolveStatus::unsupported;
        out.message = "minlp: structurally checkable source model is required for convexity "
                      "screening; arbitrary NLP callbacks are not accepted";
        return out;
    }
    try {
        require_convex_quadratic_structure(*problem.source_model);
        std::vector<std::size_t> expected_integer_indices;
        for (std::size_t j = 0; j < problem.source_model->variable_type.size(); ++j) {
            if (problem.source_model->variable_type[j] != model::VariableType::continuous)
                expected_integer_indices.push_back(j);
        }
        auto supplied_integer_indices = problem.integer_indices;
        std::sort(expected_integer_indices.begin(), expected_integer_indices.end());
        std::sort(supplied_integer_indices.begin(), supplied_integer_indices.end());
        if (std::adjacent_find(supplied_integer_indices.begin(), supplied_integer_indices.end()) !=
            supplied_integer_indices.end()) {
            throw std::invalid_argument("minlp: duplicate integer variable index");
        }
        if (expected_integer_indices != supplied_integer_indices) {
            throw std::invalid_argument(
                "minlp: integer indices must match the source model's discrete variables");
        }
        nlp = io::make_nlp_model(*problem.source_model);
        nlp.validate();
    } catch (const NonConvexMinlp& e) {
        out.status = lp::reference::SolveStatus::non_convex_minlp;
        out.message = e.what();
        return out;
    } catch (const UnsupportedMinlp& e) {
        out.status = lp::reference::SolveStatus::unsupported;
        out.message = e.what();
        return out;
    } catch (const std::exception& e) {
        out.status = lp::reference::SolveStatus::invalid_model;
        out.message = e.what();
        return out;
    }
    if (x0.size() != nlp.n_vars) {
        out.message = "x0 dimension mismatch";
        return out;
    }
    if (std::any_of(x0.begin(), x0.end(), [](double value) { return !std::isfinite(value); })) {
        out.status = lp::reference::SolveStatus::invalid_model;
        out.message = "minlp: x0 contains a non-finite value";
        return out;
    }
    if (nlp.n_eq > 0) {
        out.status = lp::reference::SolveStatus::invalid_model;
        out.message = "minlp: equality constraints are not supported by the OA master; "
                      "reformulate as affine inequalities or use an NLP solver";
        return out;
    }
    for (std::size_t idx : problem.integer_indices) {
        if (idx >= nlp.n_vars) {
            out.message = "integer index out of range";
            return out;
        }
    }

    double best_obj = kInf;
    std::vector<double> best_x;
    double best_bound = -kInf;
    std::size_t cuts = 0;
    std::size_t sqp_failures = 0;

    // Accumulated master rows.
    std::vector<std::vector<double>> obj_grads;
    std::vector<double> obj_rhs;   // grad f(x^k)^T x^k - f(x^k)
    std::vector<std::vector<double>> cut_grads;
    std::vector<double> cut_rhs;   // J_i(x^k)^T x^k - g_i(x^k)

    // Add objective + constraint rows at point p (works for feasible and
    // infeasible points alike; rows are valid under-approximations for convex
    // problems at ANY point).
    const auto add_rows_at = [&](const std::vector<double>& p, double f_at_p) {
        std::vector<double> grad_f = nlp.eval_gradient(p);
        double dot = 0.0;
        for (std::size_t j = 0; j < nlp.n_vars; ++j) {
            dot += grad_f[j] * p[j];
        }
        obj_grads.push_back(std::move(grad_f));
        obj_rhs.push_back(dot - f_at_p);
        std::size_t n_ineq = 0, n_eq = 0;
        const auto cvals = constraint_values(nlp, p, n_ineq, n_eq);
        const auto J = constraint_jacobian(nlp, p, n_ineq, n_eq);
        for (std::size_t i = 0; i < n_ineq; ++i) {
            double shift = 0.0;
            for (std::size_t j = 0; j < nlp.n_vars; ++j) {
                shift += J[i][j] * p[j];
            }
            cut_grads.push_back(J[i]);
            cut_rhs.push_back(shift - cvals[i]);
            ++cuts;
        }
        out.cuts_added = cuts;
    };

    std::vector<double> x = x0;
    for (std::size_t iter = 0; iter < options.max_iterations; ++iter) {
        if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
            out.status = lp::reference::SolveStatus::resource_limit;
            out.message = "minlp outer-approximation wall-clock deadline reached";
            if (out.integer_feasible) {
                out.x = best_x;
                out.objective = best_obj;
                out.best_bound = best_bound;
            }
            return out;
        }
        out.iterations = iter + 1;

        // 1) NLP subproblem. Iteration 1 (and any iteration where the master
        // has no usable integer point): relaxed start. Afterwards the master's
        // integer solution pins the integer variables.
        // The first subproblem is the continuous relaxation. Later subproblems
        // fix the integer variables to the master MILP assignment, as required
        // by the OA loop. Fixing them to x0 on iteration zero would constrain
        // an arbitrary (often fractional) starting guess and is not a valid
        // relaxation step.
        NlpModel fixed_model = iter == 0
                                  ? nlp
                                  : with_fixed_integers(nlp, problem.integer_indices, x);
        const auto sub = nlp::solve_sqp(fixed_model, x, options.sqp_options);
        if (sub.status == lp::reference::SolveStatus::numerical_failure) {
            // OA tolerates failed subproblems: rows at the current point are
            // still valid cuts. Abort only after repeated failures.
            ++sqp_failures;
            if (sqp_failures > 3) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: NLP subproblem failed repeatedly: " + sub.message;
                return out;
            }
        } else {
            sqp_failures = 0;
        }

        const std::vector<double> point =
            sub.x.empty() ? x : sub.x;
        if (point.size() != nlp.n_vars) {
            out.status = lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: NLP subproblem returned a primal with the wrong dimension";
            return out;
        }
        const double point_obj = nlp.eval_objective(point);
        if (!std::isfinite(point_obj)) {
            out.status = lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: NLP subproblem returned a non-finite objective";
            return out;
        }

        // 2) Incumbent update: the fixed-integer subproblem is integer
        // feasible by construction when it converges cleanly.
        bool integer_ok = true;
        for (std::size_t idx : problem.integer_indices) {
            if (std::abs(point[idx] - std::round(point[idx])) > 1e-6) {
                integer_ok = false;
                break;
            }
        }
        const double actual_violation = nlp::constraint_violation(nlp, point);
        const double actual_objective = nlp.eval_objective(point);
        if (integer_ok && sub.status == lp::reference::SolveStatus::optimal &&
            std::isfinite(actual_objective) && std::isfinite(actual_violation) &&
            actual_violation <= options.feasibility_tolerance) {
            if (actual_objective < best_obj) {
                best_obj = actual_objective;
                best_x = point;
                out.integer_feasible = true;
            }
        }

        // 3) Refine the outer approximation at the subproblem point.
        add_rows_at(point, point_obj);

        // 4) Solve the master MILP (existing engine, unmodified per D-03).
        model::Model master = build_master(nlp, problem.integer_indices,
                                           obj_grads, obj_rhs,
                                           cut_grads, cut_rhs);
        try {
            master.validate();
        } catch (const std::exception& e) {
            out.status = lp::reference::SolveStatus::invalid_model;
            out.message = std::string("minlp: master model invalid: ") + e.what();
            return out;
        }
        milp::Options milp_opts;
        milp_opts.max_nodes = options.milp_max_nodes;
        milp_opts.time_limit_seconds = options.milp_time_limit;
        if (options.deadline) {
            const double remaining_seconds = std::chrono::duration<double>(
                *options.deadline - std::chrono::steady_clock::now()).count();
            if (remaining_seconds <= 0.0) {
                out.status = lp::reference::SolveStatus::resource_limit;
                out.message = "minlp deadline reached before OA master solve";
                if (out.integer_feasible) {
                    out.x = best_x;
                    out.objective = best_obj;
                    out.best_bound = best_bound;
                }
                return out;
            }
            milp_opts.time_limit_seconds = std::min(milp_opts.time_limit_seconds,
                                                    remaining_seconds);
        }
        milp_opts.enable_cuts = false;   // OA rows are the cuts; keep master lean
        milp_opts.enable_heuristics = false;
        milp_opts.enable_strong_branching = false;
        const auto master_sol = milp::solve(master, milp_opts);

        if (master_sol.status == lp::reference::SolveStatus::infeasible) {
            if (out.integer_feasible) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: master reported infeasible despite a verified incumbent; "
                              "global optimality cannot be certified";
                out.x = best_x;
                out.objective = best_obj;
                return out;
            }
            out.status = lp::reference::SolveStatus::infeasible;
            out.message = "minlp: master MILP infeasible (no integer point)";
            return out;
        }

        // The OA master is a relaxation. Its unboundedness does not imply
        // that the original convex MINLP is unbounded: nonlinear convex
        // constraints can bound the feasible region even when their current
        // supporting hyperplanes do not. Do not propagate that status.
        if (master_sol.status == lp::reference::SolveStatus::unbounded) {
            out.status = lp::reference::SolveStatus::iteration_limit;
            out.message = "minlp: OA master relaxation is unbounded; original MINLP "
                          "boundedness and global optimality are undetermined";
            if (out.integer_feasible) {
                out.x = best_x;
                out.objective = best_obj;
            }
            return out;
        }

        // 5) Lower bound from the master (its dual bound stays valid even on
        // an early stop).
        if (master_sol.status == lp::reference::SolveStatus::optimal ||
            master_sol.status == lp::reference::SolveStatus::iteration_limit ||
            master_sol.status == lp::reference::SolveStatus::resource_limit) {
            if (std::isfinite(master_sol.best_bound)) {
                best_bound = std::max(best_bound, master_sol.best_bound);
            }
            out.best_bound = best_bound;
        } else {
            out.status = master_sol.status;
            out.message = "minlp: OA master did not return a certified bound: " +
                          master_sol.message;
            if (out.integer_feasible) {
                out.x = best_x;
                out.objective = best_obj;
            }
            return out;
        }

        // 6) Gap check (LOCKED 1e-3 relative).
        if (out.integer_feasible) {
            const double bound_tolerance = options.feasibility_tolerance *
                                           std::max(1.0, std::abs(best_obj));
            if (best_bound > best_obj + bound_tolerance) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: master lower bound exceeds the feasible incumbent; "
                              "global optimality cannot be certified";
                out.x = best_x;
                out.objective = best_obj;
                return out;
            }
            const double gap = std::max(0.0, best_obj - best_bound) /
                               std::max(1.0, std::abs(best_obj));
            out.relative_gap = gap;
            if (gap <= options.gap_tolerance) {
                out.status = lp::reference::SolveStatus::optimal;
                out.message = "minlp: outer approximation gap satisfied";
                out.x = best_x;
                out.objective = best_obj;
                return out;
            }
        }

        // 7) Next NLP start = master integer solution.
        if (master_sol.primal.size() != nlp.n_vars + 1) {
            out.status = master_sol.status == lp::reference::SolveStatus::resource_limit
                             ? lp::reference::SolveStatus::resource_limit
                             : lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: OA master did not return a complete integer assignment";
            if (out.integer_feasible) {
                out.x = best_x;
                out.objective = best_obj;
            }
            return out;
        }
        for (std::size_t idx : problem.integer_indices) {
            if (!std::isfinite(master_sol.primal[idx]) ||
                std::abs(master_sol.primal[idx] - std::round(master_sol.primal[idx])) >
                    options.feasibility_tolerance) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: OA master returned a non-integral variable assignment";
                if (out.integer_feasible) {
                    out.x = best_x;
                    out.objective = best_obj;
                }
                return out;
            }
        }
        x.assign(master_sol.primal.begin(),
                 master_sol.primal.begin() + static_cast<std::ptrdiff_t>(nlp.n_vars));
    }

    if (out.integer_feasible) {
        out.status = lp::reference::SolveStatus::iteration_limit;
        out.message = "minlp: iteration limit reached before the optimality gap tolerance; "
                      "feasible incumbent returned without an optimality claim";
        out.x = best_x;
        out.objective = best_obj;
        out.best_bound = best_bound;
        out.relative_gap = std::abs(best_obj - best_bound) /
                           std::max(1.0, std::abs(best_obj));
    } else {
        out.status = lp::reference::SolveStatus::iteration_limit;
        out.message = "minlp: maximum outer iterations reached";
    }
    return out;
}

} // namespace markov_cero::minlp
