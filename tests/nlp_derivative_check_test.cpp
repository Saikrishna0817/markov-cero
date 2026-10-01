// NLP-01 contract nlp-local-sqp.md §6.5: the developer-only derivative
// diagnostic detects deliberately wrong gradient/Jacobian callbacks, passes
// correct ones (including one-sided differences at a bound and the NLOBJ
// polynomial path), and fails closed on malformed input.
#include "markov_cero/nlp/derivative_check.hpp"
#include "markov_cero/nlp/nlp_model.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

// f(x) = x0^2 + 3 x0 x1, grad = (2 x0 + 3 x1, 3 x0).
nlp::NlpModel quadratic(bool correct_gradient) {
    nlp::NlpModel model;
    model.name = "fd_quadratic";
    model.n_vars = 2;
    model.objective = [](const std::vector<double>& x) {
        return x[0] * x[0] + 3.0 * x[0] * x[1];
    };
    model.gradient = correct_gradient
                         ? nlp::VectorFunction([](const std::vector<double>& x) {
                               return std::vector<double>{2.0 * x[0] + 3.0 * x[1],
                                                          3.0 * x[0]};
                           })
                         : nlp::VectorFunction([](const std::vector<double>& x) {
                               // Wrong: drops the 3*x1 term in component 0.
                               return std::vector<double>{2.0 * x[0], 3.0 * x[0]};
                           });
    model.validate();
    return model;
}

} // namespace

int main() {
    // Correct callbacks pass with two gradient checks recorded.
    const auto good = quadratic(true);
    const auto good_rep = nlp::check_derivatives(good, {1.0, 2.0});
    req(good_rep.passed(), "correct gradient passes the diagnostic");
    req(good_rep.gradient_ok, "gradient block flagged ok");
    req(good_rep.gradient_checks == 2, "both coordinates checked");
    req(good_rep.worst_gradient_error < 1e-5,
        "correct gradient error is inside the tolerance");

    // A deliberately wrong gradient is detected with a measurable error.
    const auto bad = quadratic(false);
    const auto bad_rep = nlp::check_derivatives(bad, {1.0, 2.0});
    req(!bad_rep.passed(), "wrong gradient fails the diagnostic");
    req(!bad_rep.gradient_ok, "gradient block flagged failing");
    req(bad_rep.worst_gradient_error > 1e-3, "detected error magnitude is reported");
    std::cout << "[+] wrong gradient worst error=" << bad_rep.worst_gradient_error
              << "\n";

    // Inequality Jacobian: g = x0*x1, correct row (x1, x0), wrong row (0,0).
    nlp::NlpModel con;
    con.name = "fd_constraints";
    con.n_vars = 2;
    con.n_ineq = 1;
    con.n_eq = 1;
    con.objective = [](const std::vector<double>& x) { return x[0] * x[0] + x[1] * x[1]; };
    con.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * x[0], 2.0 * x[1]};
    };
    con.ineq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] * x[1] - 4.0};
    };
    con.eq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[0] + 2.0 * x[1] - 1.0};
    };
    con.validate();

    con.ineq_jacobian = [](const std::vector<double>& x) {
        return std::vector<std::vector<double>>{{x[1], x[0]}};
    };
    con.eq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0, 2.0}};
    };
    const auto jac_good = nlp::check_derivatives(con, {1.0, 1.5});
    req(jac_good.passed(), "correct constraint Jacobians pass");
    req(jac_good.jacobian_checks == 4, "every Jacobian entry is checked");

    con.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{0.0, 0.0}};
    };
    const auto jac_bad = nlp::check_derivatives(con, {1.0, 1.5});
    req(!jac_bad.passed() && !jac_bad.jacobian_ok,
        "wrong inequality Jacobian row is detected");
    req(jac_bad.worst_jacobian_error > 1e-3, "Jacobian error magnitude is reported");

    con.ineq_jacobian = [](const std::vector<double>& x) {
        return std::vector<std::vector<double>>{{x[1], x[0]}};
    };
    con.eq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{1.0, 1.0}};
    };
    const auto eq_bad = nlp::check_derivatives(con, {1.0, 1.5});
    req(!eq_bad.passed() && !eq_bad.jacobian_ok,
        "wrong equality Jacobian row is detected");

    // One-sided differences at a finite bound still validate a correct
    // gradient (x0 sits exactly on its lower bound).
    auto bounded = quadratic(true);
    bounded.lower_bounds = {0.0, -10.0};
    bounded.upper_bounds = {10.0, 10.0};
    const auto at_bound = nlp::check_derivatives(bounded, {0.0, 2.0});
    req(at_bound.passed(), "bound-adjacent coordinate passes via one-sided differences");

    // Malformed input fails closed.
    const auto dim = nlp::check_derivatives(good, {1.0});
    req(!dim.passed(), "wrong x dimension fails closed");
    const auto nan_x = nlp::check_derivatives(
        good, {std::numeric_limits<double>::quiet_NaN(), 1.0});
    req(!nan_x.passed(), "non-finite x fails closed");

    // NLOBJ polynomial path (no callbacks): f = 2 x0^2 from a quadratic
    // term, analytic gradient 4 x0 must match the finite difference.
    nlp::NlpModel poly;
    poly.name = "fd_poly";
    poly.n_vars = 1;
    poly.from_nlobj = true;
    poly.linear_objective = {0.0};
    poly.poly_terms.push_back({2.0, 0, 0, true});
    const auto poly_rep = nlp::check_derivatives(poly, {1.5});
    req(poly_rep.passed(), "polynomial (NLOBJ) gradient passes the diagnostic");

    std::cout << "nlp derivative check tests passed\n";
    return 0;
}
