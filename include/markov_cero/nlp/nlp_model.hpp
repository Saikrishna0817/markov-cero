#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace markov_cero::nlp {

// W1 / D-01 / D-02: nonlinear programming model with callback input (Path A).
//
// Problem form understood by the SQP solver:
//   minimize    f(x)
//   subject to  g(x) <= 0        (n_ineq constraints)
//               h(x) = 0         (n_eq constraints)
//               lb <= x <= ub    (variable bounds)
//
// The user supplies function objects for f, grad f, g, Jacobian of g, h and
// Jacobian of h. No Hessian is required: the SQP solver approximates the
// Lagrangian Hessian with L-BFGS-B (D-02, LOCKED).

using VectorFunction = std::function<std::vector<double>(const std::vector<double>&)>;
using MatrixFunction = std::function<std::vector<std::vector<double>>(const std::vector<double>&)>;

struct NlpModel {
    std::string name{"nlp"};
    std::size_t n_vars{0};
    std::size_t n_ineq{0};
    std::size_t n_eq{0};

    // Objective and gradient (required).
    std::function<double(const std::vector<double>&)> objective;
    VectorFunction gradient;

    // Nonlinear inequalities g(x) <= 0 and Jacobian (rows = constraints).
    VectorFunction ineq_constraints;
    MatrixFunction ineq_jacobian;

    // Equalities h(x) = 0 and Jacobian (rows = constraints).
    VectorFunction eq_constraints;
    MatrixFunction eq_jacobian;

    // Variable bounds (default: free). NaN entries mean unbounded.
    std::vector<double> lower_bounds;
    std::vector<double> upper_bounds;

    // W1 Path B bridge: polynomial objective terms parsed from an NLOBJ MPS
    // section. When present (and callbacks are absent) the SQP solver builds
    // f and grad f from the base linear objective plus these terms.
    struct PolyTerm {
        double coefficient{0.0};
        std::size_t var0{0};
        std::size_t var1{0};
        bool quadratic{false};
    };
    bool from_nlobj{false};
    std::vector<PolyTerm> poly_terms;
    std::vector<double> linear_objective;
    double objective_offset{0.0};

    [[nodiscard]] bool has_callbacks() const noexcept {
        return objective != nullptr && gradient != nullptr;
    }

    [[nodiscard]] bool has_ineq() const noexcept {
        return n_ineq > 0 && ineq_constraints != nullptr;
    }

    [[nodiscard]] bool has_eq() const noexcept {
        return n_eq > 0 && eq_constraints != nullptr;
    }

    void validate() const {
        if (n_vars == 0) {
            throw std::invalid_argument("NlpModel requires at least one variable");
        }
        if (!has_callbacks() && poly_terms.empty()) {
            throw std::invalid_argument("NlpModel requires objective callbacks or NLOBJ terms");
        }
        if (has_callbacks() && n_vars != 0 && lower_bounds.empty() && upper_bounds.empty()) {
            // free variables are fine; nothing to check
        }
        if (lower_bounds.size() > n_vars || upper_bounds.size() > n_vars) {
            throw std::invalid_argument("NlpModel bound dimension mismatch");
        }
        if (has_ineq() && n_ineq == 0) {
            throw std::invalid_argument("NlpModel ineq callbacks set but n_ineq == 0");
        }
        if (has_eq() && n_eq == 0) {
            throw std::invalid_argument("NlpModel eq callbacks set but n_eq == 0");
        }
    }

    [[nodiscard]] double bound_lower(std::size_t j) const noexcept {
        return j < lower_bounds.size() ? lower_bounds[j]
                                       : -std::numeric_limits<double>::infinity();
    }

    [[nodiscard]] double bound_upper(std::size_t j) const noexcept {
        return j < upper_bounds.size() ? upper_bounds[j]
                                       : std::numeric_limits<double>::infinity();
    }

    // Evaluate f(x) honoring both input paths.
    [[nodiscard]] double eval_objective(const std::vector<double>& x) const {
        if (has_callbacks()) {
            return objective(x);
        }
        double value = objective_offset;
        for (std::size_t j = 0; j < x.size() && j < linear_objective.size(); ++j) {
            value += linear_objective[j] * x[j];
        }
        for (const PolyTerm& term : poly_terms) {
            if (term.quadratic) {
                value += term.coefficient * x[term.var0] * x[term.var1];
            } else {
                value += term.coefficient * x[term.var0];
            }
        }
        return value;
    }

    // Evaluate grad f(x) honoring both input paths.
    [[nodiscard]] std::vector<double> eval_gradient(const std::vector<double>& x) const {
        if (has_callbacks()) {
            return gradient(x);
        }
        std::vector<double> g(n_vars, 0.0);
        for (std::size_t j = 0; j < n_vars && j < linear_objective.size(); ++j) {
            g[j] += linear_objective[j];
        }
        for (const PolyTerm& term : poly_terms) {
            if (term.quadratic) {
                // d/dx_a (c * x_a * x_b) = c * x_b (and symmetric term for b
                // only when a == b the derivative is 2 c x_a).
                if (term.var0 == term.var1) {
                    g[term.var0] += 2.0 * term.coefficient * x[term.var0];
                } else {
                    // A quadratic monomial x0*x1 with coefficient c comes from
                    // NLOBJ as written; the symmetric half is the term's own.
                    g[term.var0] += term.coefficient * x[term.var1];
                    g[term.var1] += term.coefficient * x[term.var0];
                }
            } else {
                g[term.var0] += term.coefficient;
            }
        }
        return g;
    }
};

} // namespace markov_cero::nlp
