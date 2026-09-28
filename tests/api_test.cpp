#include "markov_cero/api/solve.hpp"
#include "markov_cero/model/model.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>

void test_api_solve_model_continuous() {
    markov_cero::model::Model model;
    model.name = "API_TEST_LP";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-2.0, -3.0};
    model.variable_name = {"x1", "x2"};
    model.variable_lower = {markov_cero::model::Bound::finite(0.0), markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::positive_infinity(), markov_cero::model::Bound::positive_infinity()};
    model.variable_type = {markov_cero::model::VariableType::continuous, markov_cero::model::VariableType::continuous};

    model.row_name = {"c1", "c2"};
    model.row_lower = {markov_cero::model::Bound::negative_infinity(), markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::finite(8.0), markov_cero::model::Bound::finite(10.0)};

    markov_cero::model::SparseMatrixBuilder mb(2, 2);
    mb.add(0, 0, 1.0);
    mb.add(0, 1, 2.0);
    mb.add(1, 0, 2.0);
    mb.add(1, 1, 1.0);
    model.matrix = mb.build();
    model.validate();

    markov_cero::api::SolveOptions options;
    options.engine = "simplex";
    const auto res = markov_cero::api::solve_model(model, options);
    assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(std::abs(res.objective - (-14.0)) < 1e-5);
    assert(res.verified);
    // res.primal is the canonical witness (original columns + slacks);
    // the user-facing solution lives in original_primal.
    assert(res.original_primal.size() == 2);
    assert(std::abs(res.original_primal[0] - 4.0) < 1e-5);
    assert(std::abs(res.original_primal[1] - 2.0) < 1e-5);
    std::cout << "[+] test_api_solve_model_continuous passed\n";
}

void test_api_solve_file() {
    markov_cero::api::SolveOptions options;
    options.engine = "auto";
    const auto res = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(res.status == markov_cero::lp::reference::SolveStatus::optimal);
    assert(res.verified);
    // examples/blend.mps: min 40a + 30b, a + b = 100, 0.01a + 0.03b <= 2
    // -> a = b = 50, objective 3500.
    assert(std::abs(res.objective - 3500.0) < 1e-6);
    std::cout << "[+] test_api_solve_file passed: obj=" << res.objective << "\n";
}

void test_api_numerical_diagnostic() {
    markov_cero::api::SolveOptions options;
    options.engine = "auto";
    const auto res = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(res.diagnostic.failure_site == "none");
    assert(res.diagnostic.suggested_recovery == "none");
    assert(res.diagnostic.condition_estimate >= 1.0);
    assert(res.diagnostic.primal_residual >= 0.0);
    assert(res.diagnostic.dual_residual >= 0.0);

    const auto bad_res = markov_cero::api::solve_file("nonexistent_model.mps", options);
    assert(bad_res.status == markov_cero::lp::reference::SolveStatus::invalid_model);
    assert(bad_res.diagnostic.failure_site == "file_io");
    assert(bad_res.diagnostic.suggested_recovery == "verify_file_exists_and_has_read_permissions");

    std::cout << "[+] test_api_numerical_diagnostic passed\n";
}

void test_api_expired_deadline() {
    markov_cero::api::SolveOptions options;
    options.engine = "primal";
    options.lp_options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto res = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(res.status == markov_cero::lp::reference::SolveStatus::resource_limit);
    assert(!res.verified);
    std::cout << "[+] test_api_expired_deadline passed\n";
}

void test_api_input_byte_budget() {
    markov_cero::api::SolveOptions options;
    options.maximum_input_bytes = 1;
    const auto limited = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(limited.status == markov_cero::lp::reference::SolveStatus::resource_limit);
    assert(limited.diagnostic.failure_site == "input_resource_limit");
    options.maximum_input_bytes = 0;
    const auto invalid = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(invalid.status == markov_cero::lp::reference::SolveStatus::invalid_options);
}

void test_api_nlp_callbacks_path_a() {
    using markov_cero::model::Bound;
    using markov_cero::model::ObjectiveSense;
    using markov_cero::model::VariableType;
    markov_cero::model::Model model;
    model.name = "NLP_PATH_A";
    model.objective_sense = ObjectiveSense::minimize;
    model.objective = {0.0, 0.0};
    model.variable_name = {"x0", "x1"};
    model.variable_lower = {Bound::finite(-10.0), Bound::finite(-10.0)};
    model.variable_upper = {Bound::finite(10.0), Bound::finite(10.0)};
    model.variable_type = {VariableType::continuous, VariableType::continuous};

    // Linear row: x0 + x1 <= 3 (must survive as a constraint next to the
    // companion's callback row).
    model.row_name = {"lin"};
    model.row_lower = {Bound::negative_infinity()};
    model.row_upper = {Bound::finite(3.0)};
    markov_cero::model::SparseMatrixBuilder mb(1, 2);
    mb.add(0, 0, 1.0);
    mb.add(0, 1, 1.0);
    model.matrix = mb.build();

    // Companion (Path A): f = (x0-1)^2 + (x1-2)^2,
    // g = x1 - x0 - 0.5 <= 0, companion bounds [0, inf).
    // Intersection with the Model's [-10, 10] gives [0, 10].
    // Optimum: projection of (1,2) onto {x1 = x0 + 0.5} clipped by the
    // linear row -> x* = (1.25, 1.75), f* = 0.125, both rows active.
    markov_cero::nlp::NlpModel cb;
    cb.name = "cb";
    cb.n_vars = 2;
    cb.objective = [](const std::vector<double>& x) {
        return (x[0] - 1.0) * (x[0] - 1.0) + (x[1] - 2.0) * (x[1] - 2.0);
    };
    cb.gradient = [](const std::vector<double>& x) {
        return std::vector<double>{2.0 * (x[0] - 1.0), 2.0 * (x[1] - 2.0)};
    };
    cb.n_ineq = 1;
    cb.ineq_constraints = [](const std::vector<double>& x) {
        return std::vector<double>{x[1] - x[0] - 0.5};
    };
    cb.ineq_jacobian = [](const std::vector<double>&) {
        return std::vector<std::vector<double>>{{-1.0, 1.0}};
    };
    cb.lower_bounds = {0.0, 0.0};
    model.nlp_callbacks = cb;
    model.validate();

    markov_cero::api::SolveOptions options;
    const auto res = markov_cero::api::solve_model(model, options);
    assert(res.problem_class == "NLP");
    assert(res.classification_reason == "nlp_callbacks");
    assert(res.resolved_engine == "sqp");
    assert(res.status == markov_cero::lp::reference::SolveStatus::local_optimal);
    assert(res.certificate_type == "local_kkt");
    assert(!res.verified); // Local KKT does not establish global optimality.
    assert(res.primal.size() == 2);
    assert(std::abs(res.primal[0] - 1.25) < 1e-4);
    assert(std::abs(res.primal[1] - 1.75) < 1e-4);
    assert(std::abs(res.objective - 0.125) < 1e-4);
    assert(res.original_verified);
    assert(res.diagnostic.nlp_stationarity_residual <= 1e-6);
    assert(res.diagnostic.nlp_inequality_violation <= 1e-6);
    assert(res.diagnostic.nlp_equality_violation <= 1e-6);
    assert(res.diagnostic.nlp_worst_dual_sign >= -1e-6);
    assert(res.diagnostic.nlp_complementarity_residual <= 1e-6);
    options.lp_options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto expired = markov_cero::api::solve_model(model, options);
    assert(expired.status == markov_cero::lp::reference::SolveStatus::resource_limit);
    std::cout << "[+] test_api_nlp_callbacks_path_a passed: x=(" << res.primal[0]
              << ", " << res.primal[1] << ") obj=" << res.objective << "\n";
}

int main() {
    try {
        test_api_solve_model_continuous();
        test_api_solve_file();
        test_api_numerical_diagnostic();
        test_api_expired_deadline();
        test_api_input_byte_budget();
        test_api_nlp_callbacks_path_a();
        std::cout << "All API unit tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error in api test: " << e.what() << "\n";
        return 1;
    }
}
