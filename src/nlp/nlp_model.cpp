#include "nlp_helpers.hpp"

#include <algorithm>
#include <cmath>

namespace markov_cero::nlp {

namespace {

double inf_norm(const std::vector<double>& v) {
    double m = 0.0;
    for (double x : v) {
        m = std::max(m, std::abs(x));
    }
    return m;
}

} // namespace

double constraint_violation(const NlpModel& model, const std::vector<double>& x) {
    std::size_t n_ineq = 0, n_eq = 0;
    const auto v = constraint_values(model, x, n_ineq, n_eq);
    double worst = 0.0;
    for (std::size_t i = 0; i < n_ineq; ++i) {
        worst = std::max(worst, v[i]);  // g(x) <= 0
    }
    for (std::size_t i = n_ineq; i < v.size(); ++i) {
        worst = std::max(worst, std::abs(v[i]));  // h(x) = 0
    }
    for (std::size_t j = 0; j < x.size(); ++j) {
        const double lb = model.bound_lower(j);
        const double ub = model.bound_upper(j);
        if (std::isfinite(lb)) {
            worst = std::max(worst, lb - x[j]);
        }
        if (std::isfinite(ub)) {
            worst = std::max(worst, x[j] - ub);
        }
    }
    return worst;
}

double merit_value(const NlpModel& model, const std::vector<double>& x, double mu) {
    const double f = model.eval_objective(x);
    std::size_t n_ineq = 0, n_eq = 0;
    const auto v = constraint_values(model, x, n_ineq, n_eq);
    double penalty = 0.0;
    for (std::size_t i = 0; i < n_ineq; ++i) {
        penalty += std::max(0.0, v[i]);
    }
    for (std::size_t i = n_ineq; i < v.size(); ++i) {
        penalty += std::abs(v[i]);
    }
    return f + mu * penalty;
}

double inf_norm_of(const std::vector<double>& v) {
    return inf_norm(v);
}

std::vector<double> constraint_values(const NlpModel& model, const std::vector<double>& x,
                                      std::size_t& n_ineq, std::size_t& n_eq) {
    n_ineq = 0;
    n_eq = 0;
    std::vector<double> out;
    if (model.has_ineq()) {
        out = model.ineq_constraints(x);
        n_ineq = out.size();
    }
    if (model.has_eq()) {
        auto eq = model.eq_constraints(x);
        out.insert(out.end(), eq.begin(), eq.end());
        n_eq = eq.size();
    }
    return out;
}

std::vector<std::vector<double>> constraint_jacobian(const NlpModel& model,
                                                     const std::vector<double>& x,
                                                     std::size_t& n_ineq, std::size_t& n_eq) {
    std::vector<std::vector<double>> out;
    n_ineq = 0;
    n_eq = 0;
    if (model.has_ineq()) {
        out = model.ineq_jacobian(x);
        n_ineq = out.size();
    }
    if (model.has_eq()) {
        auto eq = model.eq_jacobian(x);
        out.insert(out.end(), eq.begin(), eq.end());
        n_eq = eq.size();
    }
    return out;
}

void projectToBounds(const NlpModel& model, std::vector<double>& x) {
    for (std::size_t j = 0; j < x.size(); ++j) {
        const double lb = model.bound_lower(j);
        const double ub = model.bound_upper(j);
        if (std::isfinite(lb) && x[j] < lb) {
            x[j] = lb;
        }
        if (std::isfinite(ub) && x[j] > ub) {
            x[j] = ub;
        }
    }
}

} // namespace markov_cero::nlp
