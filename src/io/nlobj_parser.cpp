// W1 / D-01 Path B / D-12 / D-20: NLOBJ section support.
//
// The MPS parser (src/io/mps.cpp) reads the NLOBJ section into
// model::Model::nlobj_terms. This translation unit bridges those terms into
// an nlp::NlpModel consumable by the SQP engine, so file-based NLP models and
// callback-based NLP models share one solver path. The format specification
// is a non-standard MPS extension; its original format note is in Git history.

#include "markov_cero/io/nlobj_parser.hpp"

#include "markov_cero/model/model.hpp"
#include "markov_cero/qp/model.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace markov_cero::io {
void compose_callbacks(const model::Model&, nlp::NlpModel&, double);

nlp::NlpModel make_nlp_model(const model::Model& model) {
    nlp::NlpModel nlp;
    nlp.name = model.name.empty() ? "nlobj_model" : model.name;
    const std::size_t n = model.matrix.column_count;
    nlp.n_vars = n;

    // Base linear objective (sign-normalized to minimize).
    const bool maximize = model.objective_sense == model::ObjectiveSense::maximize;
    const double sign = maximize ? -1.0 : 1.0;
    nlp.linear_objective.resize(n, 0.0);
    for (std::size_t j = 0; j < n && j < model.objective.size(); ++j) {
        nlp.linear_objective[j] = sign * model.objective[j];
    }
    nlp.objective_offset = sign * model.objective_offset;

    // NLOBJ polynomial terms (coefficients carry the same sign convention).
    nlp.from_nlobj = true;
    nlp.poly_terms.reserve(model.nlobj_terms.size());
    for (const auto& term : model.nlobj_terms) {
        nlp::NlpModel::PolyTerm p;
        p.coefficient = sign * term.coefficient;
        p.var0 = term.var0;
        p.var1 = term.var1;
        p.quadratic = term.quadratic;
        nlp.poly_terms.push_back(p);
    }

    // A source may contain both a standard MPS QUADOBJ and the NLOBJ
    // extension. Keep the QUADOBJ contribution in the NLP objective as well;
    // `make_quadratic_model` applies the source objective sense and uses the
    // solver convention 0.5 * x^T P x. Previously MINLP convexity screening
    // saw this Hessian while SQP silently optimized only the linear/NLOBJ part.
    if (model.has_quadratic_objective) {
        const auto qp = qp::make_quadratic_model(model);
        const auto P = qp.P;
        const auto base = nlp;
        nlp.objective = [base, P](const std::vector<double>& x) {
            return base.eval_objective(x) + 0.5 * P.evaluate_energy(x);
        };
        nlp.gradient = [base, P](const std::vector<double>& x) {
            auto g = base.eval_gradient(x);
            const auto Px = P.multiply(x);
            if (Px.size() != g.size())
                throw std::runtime_error("NLP QUADOBJ gradient dimension mismatch");
            for (std::size_t j = 0; j < g.size(); ++j) g[j] += Px[j];
            return g;
        };
    }

    // Variable bounds pass through.
    nlp.lower_bounds.resize(n);
    nlp.upper_bounds.resize(n);
    for (std::size_t j = 0; j < n; ++j) {
        nlp.lower_bounds[j] = model.variable_lower[j].is_finite()
                                  ? model.variable_lower[j].value
                                  : std::numeric_limits<double>::quiet_NaN();
        nlp.upper_bounds[j] = model.variable_upper[j].is_finite()
                                  ? model.variable_upper[j].value
                                  : std::numeric_limits<double>::quiet_NaN();
    }

    // Linear constraints ride as callbacks in the g(x) <= 0 convention:
    // row_lower <= a^T x <= row_upper  ->  two inequality rows each.
    const std::size_t m = model.matrix.row_count;
    std::vector<double> arow(m, 0.0);
    std::vector<std::vector<double>> ineq_rows;
    ineq_rows.reserve(2 * m);
    // dense row-major view of the sparse matrix for the callbacks
    std::vector<std::vector<double>> dense(m, std::vector<double>(n, 0.0));
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t k = model.matrix.column_start[j];
             k < model.matrix.column_start[j + 1]; ++k) {
            dense[model.matrix.row_index[k]][j] = model.matrix.value[k];
        }
    }
    for (std::size_t i = 0; i < m; ++i) {
        // a^T x - upper <= 0  (when upper is finite)
        // lower - a^T x <= 0  (when lower is finite)
        if (model.row_upper[i].is_finite()) {
            ineq_rows.push_back(dense[i]);
        }
        if (model.row_lower[i].is_finite()) {
            std::vector<double> neg = dense[i];
            for (double& v : neg) {
                v = -v;
            }
            ineq_rows.push_back(neg);
        }
    }
    nlp.n_ineq = ineq_rows.size();

    // Precompute RHS values (dense[i] captures coefficients; the RHS constants
    // are captured by closure below).
    auto ineq_fn = [dense, m, n, model](
                       const std::vector<double>& x) -> std::vector<double> {
        std::vector<double> out;
        out.reserve(2 * m);
        for (std::size_t i = 0; i < m; ++i) {
            if (model.row_upper[i].is_finite()) {
                double ax = 0.0;
                for (std::size_t j = 0; j < n; ++j) {
                    ax += dense[i][j] * x[j];
                }
                out.push_back(ax - model.row_upper[i].value);
            }
            if (model.row_lower[i].is_finite()) {
                double ax = 0.0;
                for (std::size_t j = 0; j < n; ++j) {
                    ax += dense[i][j] * x[j];
                }
                out.push_back(model.row_lower[i].value - ax);
            }
        }
        return out;
    };
    auto ineq_jac_fn = [ineq_rows](const std::vector<double>&) {
        return ineq_rows;
    };
    nlp.ineq_constraints = ineq_fn;
    nlp.ineq_jacobian = ineq_jac_fn;

    // NLCON polynomial rows use g_i(x) = sum(term_i) - rhs_i <= 0.
    if (!model.nlcon_constraints.empty()) {
        const auto nonlinear_rows = model.nlcon_constraints;
        const auto nonlinear_fn = [nonlinear_rows](const std::vector<double>& x) {
            std::vector<double> values;
            values.reserve(nonlinear_rows.size());
            for (const auto& row : nonlinear_rows) {
                double value = -row.rhs;
                for (const auto& term : row.terms) {
                    value += term.coefficient * x[term.var0] *
                             (term.quadratic ? x[term.var1] : 1.0);
                }
                values.push_back(value);
            }
            return values;
        };
        const auto nonlinear_jac = [nonlinear_rows, n](const std::vector<double>& x) {
            std::vector<std::vector<double>> jac(nonlinear_rows.size(), std::vector<double>(n, 0.0));
            for (std::size_t i = 0; i < nonlinear_rows.size(); ++i) {
                for (const auto& term : nonlinear_rows[i].terms) {
                    if (!term.quadratic) {
                        jac[i][term.var0] += term.coefficient;
                    } else if (term.var0 == term.var1) {
                        jac[i][term.var0] += 2.0 * term.coefficient * x[term.var0];
                    } else {
                        jac[i][term.var0] += term.coefficient * x[term.var1];
                        jac[i][term.var1] += term.coefficient * x[term.var0];
                    }
                }
            }
            return jac;
        };
        const auto base_fn = nlp.ineq_constraints;
        const auto base_jac = nlp.ineq_jacobian;
        nlp.ineq_constraints = [base_fn, nonlinear_fn](const std::vector<double>& x) {
            auto values = base_fn(x);
            const auto extra = nonlinear_fn(x);
            values.insert(values.end(), extra.begin(), extra.end());
            return values;
        };
        nlp.ineq_jacobian = [base_jac, nonlinear_jac](const std::vector<double>& x) {
            auto rows = base_jac(x);
            const auto extra = nonlinear_jac(x);
            rows.insert(rows.end(), extra.begin(), extra.end());
            return rows;
        };
        nlp.n_ineq += nonlinear_rows.size();
    }

    compose_callbacks(model, nlp, sign);

    if (!nlp.has_callbacks() && nlp.poly_terms.empty()) {
        const auto linear = nlp.linear_objective;
        nlp.objective = [linear](const std::vector<double>& x) {
            double value = 0.0;
            for (std::size_t j = 0; j < linear.size(); ++j)
                value += linear[j] * x[j];
            return value;
        };
        nlp.gradient = [linear](const std::vector<double>&) { return linear; };
    }

    nlp.validate();
    return nlp;
}

} // namespace markov_cero::io
