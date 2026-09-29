#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/model/model.hpp"
#include "api_parallel_options_test.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

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
    assert(res.stop_reason == "deadline_exceeded");
    std::cout << "[+] test_api_expired_deadline passed\n";
}

void test_api_resource_options() {
    using markov_cero::lp::reference::SolveStatus;
    markov_cero::api::SolveOptions options;
    options.total_time_limit_seconds = 1e-12;
    const auto expired = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(expired.status == SolveStatus::resource_limit);
    assert(expired.stop_reason == "deadline_exceeded");
    assert(!expired.verified);
    options.total_time_limit_seconds.reset();
    options.memory_limit_bytes = 1;
    const auto limited = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(limited.status == SolveStatus::resource_limit);
    assert(limited.stop_reason == "memory_budget_exhausted");
    assert(limited.diagnostic.failure_site == "memory_budget");
    options.memory_limit_bytes.reset();
    const auto solved = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(solved.status == SolveStatus::optimal && solved.verified);
    assert(solved.stop_reason.empty());
    options.memory_limit_bytes = 0;
    assert(markov_cero::api::solve_file("examples/blend.mps", options).status ==
           SolveStatus::invalid_options);
    options.memory_limit_bytes.reset();
    options.total_time_limit_seconds = 0;
    assert(markov_cero::api::solve_file("examples/blend.mps", options).status ==
           SolveStatus::invalid_options);
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

void test_api_model_fingerprint_binding() {
    markov_cero::api::SolveOptions options;
    options.engine = "auto";
    // W01/D16: every path that reaches a solve binds the result to a stable
    // model identity captured at the API boundary.
    const auto from_file = markov_cero::api::solve_file("examples/blend.mps", options);
    assert(from_file.model_fingerprint != 0);

    markov_cero::model::Model model;
    model.name = "fingerprint_lp";
    model.objective = {1.0};
    model.variable_name = {"x"};
    model.variable_lower = {markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::positive_infinity()};
    model.variable_type = {markov_cero::model::VariableType::continuous};
    model.row_name = {"demand"};
    model.row_lower = {markov_cero::model::Bound::finite(1.0)};
    model.row_upper = {markov_cero::model::Bound::positive_infinity()};
    markov_cero::model::SparseMatrixBuilder builder(1, 1);
    builder.add(0, 0, 1.0);
    model.matrix = builder.build();
    model.validate();

    const auto first = markov_cero::api::solve_model(model, options);
    const auto second = markov_cero::api::solve_model(model, options);
    assert(first.model_fingerprint != 0);
    assert(first.model_fingerprint == second.model_fingerprint);
    assert(first.model_fingerprint != from_file.model_fingerprint);

    model.objective[0] = 2.0;
    const auto changed = markov_cero::api::solve_model(model, options);
    assert(changed.model_fingerprint != first.model_fingerprint);
    std::cout << "[+] test_api_model_fingerprint_binding passed\n";
}

#define CHECK_MIP(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
void test_api_mip_guarantee_fields() {
    using markov_cero::lp::reference::SolveStatus;
    const auto model = markov_cero::io::parse_mps_string(
        "NAME TEST\nROWS\n N COST\n L CAP\nCOLUMNS\n X COST -1 CAP 1\n"
        "RHS\n R CAP 1.5\nBOUNDS\n LI B X 0\n UI B X 2\nENDATA\n");
    for (const char* engine : {"milp", "parallel"}) {
        markov_cero::api::SolveOptions options;
        options.engine = engine;
        options.num_threads = 2;
        const auto accepted = markov_cero::api::solve_model(model, options);
        CHECK_MIP(accepted.status == SolveStatus::optimal);
        CHECK_MIP(accepted.verified && accepted.canonical_verified);
        CHECK_MIP(accepted.certificate_type == "independent_mip_tree");
        CHECK_MIP(accepted.guarantee_tier == "independent_tree");
        CHECK_MIP(accepted.proof_status == "accepted" && !accepted.proof_budget_exhausted);
        CHECK_MIP(accepted.proof_budget_kind == "none");
        CHECK_MIP(accepted.proof_nodes_used >= 3 && accepted.proof_checked_nodes == accepted.proof_nodes_used);
        CHECK_MIP(accepted.proof_witness_values_used > 0 && accepted.proof_checked_witness_values > 0);
        CHECK_MIP(accepted.proof_budget_time_ms >= 0 && accepted.mip_proof_build_ms >= 0 &&
               accepted.mip_proof_verify_ms >= 0);
        CHECK_MIP(accepted.proof_format_version == markov_cero::verify::kMipProofFormatVersion);
        CHECK_MIP(accepted.proof_model_fingerprint == std::to_string(accepted.model_fingerprint));
        CHECK_MIP(accepted.mip_proof && accepted.mip_proof->model_fingerprint ==
               accepted.proof_model_fingerprint);

        options.mip_proof_max_nodes = 1;
        const auto exhausted = markov_cero::api::solve_model(model, options);
        CHECK_MIP(exhausted.status == SolveStatus::feasible);
        CHECK_MIP(!exhausted.verified && !exhausted.canonical_verified);
        CHECK_MIP(exhausted.original_verified);
        CHECK_MIP(exhausted.certificate_type == "incumbent_feasibility");
        CHECK_MIP(exhausted.guarantee_tier == "unverified");
        CHECK_MIP(exhausted.proof_status == "exhausted" && exhausted.proof_budget_exhausted);
        CHECK_MIP(exhausted.proof_budget_kind == "node_limit");
        CHECK_MIP(exhausted.mip_proof && exhausted.mip_proof->budget_exhausted);
        CHECK_MIP(exhausted.message.find("independent proof exhausted; optimality not certified") != std::string::npos);
        CHECK_MIP(std::isfinite(exhausted.objective) && std::isfinite(exhausted.best_bound));
        CHECK_MIP(std::isfinite(exhausted.relative_gap));
    }
    std::cout << "[+] test_api_mip_guarantee_fields passed\n";
}

int main() {
    try {
        test_api_solve_model_continuous();
        test_api_solve_file();
        test_api_numerical_diagnostic();
        test_api_expired_deadline();
        test_api_resource_options();
        test_api_input_byte_budget();
        test_api_nlp_callbacks_path_a();
        test_api_model_fingerprint_binding();
        test_api_mip_guarantee_fields();
        test_api_parallel_node_selection_pass_through();
        std::cout << "All API unit tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[-] Error in api test: " << e.what() << "\n";
        return 1;
    }
}
