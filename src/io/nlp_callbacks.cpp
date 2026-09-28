#include "markov_cero/io/nlobj_parser.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace markov_cero::io {
void compose_callbacks(const model::Model& model, nlp::NlpModel& nlp, double sign) {
    const auto n = nlp.n_vars;
    // W1/D-01 Path A: compose the programmatic callback companion when one
    // accompanies the model. Locked rules (model::Model::nlp_callbacks):
    // companion objective ADDED to the linear/NLOBJ objective, companion
    // g/h rows APPENDED after the linear rows, bounds intersected (tightest
    // finite bound wins), n_vars must match the Model's column count.
    if (model.nlp_callbacks) {
        const nlp::NlpModel& cb = *model.nlp_callbacks;
        cb.validate();
        if (!cb.has_callbacks()) {
            throw std::invalid_argument(
                "NLP callback companion requires objective and gradient callbacks (Path A)");
        }
        if (cb.n_vars != n) {
            throw std::invalid_argument("NLP callback companion n_vars (" +
                                        std::to_string(cb.n_vars) +
                                        ") must equal model column count (" +
                                        std::to_string(n) + ")");
        }
        if (cb.n_ineq > 0 && (!cb.ineq_constraints || !cb.ineq_jacobian)) {
            throw std::invalid_argument(
                "NLP callback companion declares inequality rows without constraint callbacks");
        }
        if (cb.n_eq > 0 && (!cb.eq_constraints || !cb.eq_jacobian)) {
            throw std::invalid_argument(
                "NLP callback companion declares equality rows without constraint callbacks");
        }

        // Objective: snapshot of the base (linear + NLOBJ polynomial terms,
        // already sign-normalized for maximize) plus sign * companion f.
        // The snapshot keeps eval_objective/eval_gradient on the linear path
        // regardless of assignment order below.
        const nlp::NlpModel lin_base = nlp;
        const double sgn = sign;
        const auto cb_obj = cb.objective;
        const auto cb_grad = cb.gradient;
        nlp.objective = [lin_base, sgn, cb_obj](const std::vector<double>& x) {
            return lin_base.eval_objective(x) + sgn * cb_obj(x);
        };
        nlp.gradient = [lin_base, sgn, cb_grad](const std::vector<double>& x) {
            std::vector<double> g = lin_base.eval_gradient(x);
            const std::vector<double> cg = cb_grad(x);
            if (cg.size() != g.size()) throw std::invalid_argument("NLP companion gradient dimension mismatch");
            const std::size_t common = g.size();
            for (std::size_t j = 0; j < common; ++j) {
                g[j] += sgn * cg[j];
            }
            return g;
        };

        // Inequalities: append the companion's g(x) <= 0 rows after the
        // Model's linear rows.
        if (cb.has_ineq()) {
            if (nlp.n_ineq == 0) {
                nlp.ineq_constraints = cb.ineq_constraints;
                nlp.ineq_jacobian = cb.ineq_jacobian;
                nlp.n_ineq = cb.n_ineq;
            } else {
                const auto base_fn = nlp.ineq_constraints;
                const auto base_jac = nlp.ineq_jacobian;
                const auto cb_fn = cb.ineq_constraints;
                const auto cb_jac = cb.ineq_jacobian;
                nlp.ineq_constraints = [base_fn, cb_fn](const std::vector<double>& x) {
                    std::vector<double> v = base_fn(x);
                    const std::vector<double> w = cb_fn(x);
                    v.insert(v.end(), w.begin(), w.end());
                    return v;
                };
                nlp.ineq_jacobian = [base_jac, cb_jac](const std::vector<double>& x) {
                    std::vector<std::vector<double>> v = base_jac(x);
                    const std::vector<std::vector<double>> w = cb_jac(x);
                    v.insert(v.end(), w.begin(), w.end());
                    return v;
                };
                nlp.n_ineq += cb.n_ineq;
            }
        }

        // Equalities: the Model's linear rows ride as inequalities above, so
        // the companion's h(x) = 0 rows normally start from an empty base;
        // append in the general case anyway.
        if (cb.has_eq()) {
            if (nlp.n_eq == 0) {
                nlp.eq_constraints = cb.eq_constraints;
                nlp.eq_jacobian = cb.eq_jacobian;
                nlp.n_eq = cb.n_eq;
            } else {
                const auto base_fn = nlp.eq_constraints;
                const auto base_jac = nlp.eq_jacobian;
                const auto cb_fn = cb.eq_constraints;
                const auto cb_jac = cb.eq_jacobian;
                nlp.eq_constraints = [base_fn, cb_fn](const std::vector<double>& x) {
                    std::vector<double> v = base_fn(x);
                    const std::vector<double> w = cb_fn(x);
                    v.insert(v.end(), w.begin(), w.end());
                    return v;
                };
                nlp.eq_jacobian = [base_jac, cb_jac](const std::vector<double>& x) {
                    std::vector<std::vector<double>> v = base_jac(x);
                    const std::vector<std::vector<double>> w = cb_jac(x);
                    v.insert(v.end(), w.begin(), w.end());
                    return v;
                };
                nlp.n_eq += cb.n_eq;
            }
        }

        // Bounds: intersection — the tightest finite bound wins; a free side
        // (NaN) takes the companion's bound, and a non-finite companion bound
        // means "no additional restriction".
        for (std::size_t j = 0; j < n; ++j) {
            const double clo = j < cb.lower_bounds.size() ? cb.lower_bounds[j]
                                                          : std::numeric_limits<double>::quiet_NaN();
            if (std::isfinite(clo)) {
                nlp.lower_bounds[j] = std::isfinite(nlp.lower_bounds[j])
                                          ? std::max(nlp.lower_bounds[j], clo)
                                          : clo;
            }
            const double chi = j < cb.upper_bounds.size() ? cb.upper_bounds[j]
                                                          : std::numeric_limits<double>::quiet_NaN();
            if (std::isfinite(chi)) {
                nlp.upper_bounds[j] = std::isfinite(nlp.upper_bounds[j])
                                          ? std::min(nlp.upper_bounds[j], chi)
                                          : chi;
            }
        }
    }

}
}
