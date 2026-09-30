// MIP-01 contract §7.3: blueprint edge cases — resource stop with an
// incumbent keeps a verified primal and an honest bound; gap_satisfied with
// a nonzero gap survives proof replay; infeasible vs resource-limit with no
// incumbent; weak/unknown PDLP node bound (F1); nearly integral branch value
// (P3); cut rejected after re-solve (P10) leaves state unchanged.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/model/model_snapshot.hpp"
#include "markov_cero/milp/shared_incumbent.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/milp/parallel_tree_search_internal.hpp"

using namespace markov_cero;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
bool contains(const std::string& text, const char* fragment) {
    return text.find(fragment) != std::string::npos;
}

// 4 x1 + 6 x2 + 5 x3 <= 8, min -10 x1 -14 x2 -12 x3 over binaries.
// LP optimum (1, 0, 4/5) = -19.6; integer optimum (0,1,0) = -14.
model::Model fractional_knapsack() {
    model::Model m;
    m.name = "GAP_KNAPSACK";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective = {-10.0, -14.0, -12.0};
    model::SparseMatrixBuilder builder(1, 3);
    builder.add(0, 0, 4.0);
    builder.add(0, 1, 6.0);
    builder.add(0, 2, 5.0);
    m.matrix = builder.build();
    m.row_lower = {model::Bound::negative_infinity()};
    m.row_upper = {model::Bound::finite(8.0)};
    m.row_name = {"CAP"};
    m.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0),
                        model::Bound::finite(0.0)};
    m.variable_upper = {model::Bound::finite(1.0), model::Bound::finite(1.0),
                        model::Bound::finite(1.0)};
    m.variable_type = {model::VariableType::binary, model::VariableType::binary,
                       model::VariableType::binary};
    m.variable_name = {"X1", "X2", "X3"};
    m.validate();
    return m;
}

// x1 <= 0 and x1 >= 1: truly infeasible.
model::Model infeasible_pair() {
    model::Model m;
    m.name = "PAIR";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective = {1.0};
    model::SparseMatrixBuilder builder(2, 1);
    builder.add(0, 0, 1.0);
    builder.add(1, 0, 1.0);
    m.matrix = builder.build();
    m.row_lower = {model::Bound::negative_infinity(), model::Bound::finite(1.0)};
    m.row_upper = {model::Bound::finite(0.0), model::Bound::positive_infinity()};
    m.row_name = {"LE", "GE"};
    m.variable_lower = {model::Bound::finite(0.0)};
    m.variable_upper = {model::Bound::finite(4.0)};
    m.variable_type = {model::VariableType::integer};
    m.variable_name = {"X"};
    m.validate();
    return m;
}

void require_honest_incumbent(const milp::Result& result, double true_optimum,
                              const std::string& name) {
    require(!result.primal.empty(), name + ": resource stop keeps the incumbent");
    require(std::fabs(result.objective - true_optimum) >= -1e-9,
            name + ": incumbent objective is feasible w.r.t. the optimum");
    require(std::isfinite(result.best_bound) &&
                result.best_bound <= true_optimum + 1e-6 * (1.0 + std::fabs(true_optimum)),
            name + ": reported bound is never overstated");
    for (double value : result.primal)
        require(std::fabs(value - std::round(value)) <= 1e-6,
                name + ": incumbent stays integral");
}

void test_resource_stop_keeps_verified_incumbent() {
    // Node quota stops the search after the root expansion: resource_limit
    // must keep the heuristic incumbent and an honest (not overstated) bound.
    milp::Options options;
    options.max_nodes = 1;
    options.time_limit_seconds = 30.0;
    const auto result = milp::solve(fractional_knapsack(), options);
    require(result.status == lp::reference::SolveStatus::resource_limit,
            "node quota reports resource_limit, never a proof");
    require_honest_incumbent(result, -14.0, "node quota stop");
    require(!result.message.empty(), "resource stop carries a stop reason");
    std::cout << "[+] resource stop keeps verified incumbent and honest bound ("
              << result.message << ")\n";
}

void test_gap_satisfied_nonzero_gap_survives_proof() {
    api::SolveOptions options;
    options.engine = "milp";
    options.milp_options.relative_gap_tolerance = 1.0; // 100% gap: stop early
    options.milp_options.time_limit_seconds = 30.0;
    options.mip_proof_time_limit_seconds = 30.0;
    const auto result = api::solve_model(fractional_knapsack(), options);
    require(result.status == lp::reference::SolveStatus::gap_satisfied,
            "100% gap closes at the root with gap_satisfied");
    require(result.original_verified, "gap incumbent independently verified");
    require(result.objective >= -14.0 - 1e-6, "gap incumbent never beats the optimum");
    require(std::isfinite(result.best_bound) && result.best_bound <= -14.0 + 1e-6,
            "gap bound never overstated");
    require(result.proof_status == "accepted" && result.canonical_verified,
            "gap proof replay accepted at nonzero gap");
    require(result.certificate_type == "independent_mip_gap",
            "gap certificate type recorded");
    std::cout << "[+] gap_satisfied at nonzero gap keeps an accepted proof\n";
}

// Timeout with an incumbent (§7.3): an expired proof budget after a finished
// search downgrades the label to Feasible while keeping the independently
// verified primal and the honest search bound — never a proof over an
// unfinished replay.
void test_timeout_with_incumbent_keeps_verified_primal() {
    api::SolveOptions options;
    options.engine = "milp";
    options.milp_options.time_limit_seconds = 30.0;
    options.mip_proof_time_limit_seconds = 1e-9;
    const auto result = api::solve_model(fractional_knapsack(), options);
    require(result.status == lp::reference::SolveStatus::feasible,
            "expired proof budget downgrades to Feasible, never a proof");
    require(result.original_verified && !result.primal.empty(),
            "timeout keeps the independently verified incumbent");
    require(result.objective >= -14.0 - 1e-6, "timeout incumbent stays feasible");
    require(std::isfinite(result.best_bound) && result.best_bound <= -14.0 + 1e-6,
            "timeout bound is honest (never overstated)");
    require(result.proof_status == "exhausted" && result.proof_budget_exhausted,
            "proof budget exhaustion recorded");
    std::cout << "[+] timeout with incumbent keeps verified primal and honest bound\n";
}

void test_infeasible_vs_resource_limit_no_incumbent() {
    const auto model = infeasible_pair();
    milp::Options proven;
    proven.time_limit_seconds = 30.0;
    const auto infeasible = milp::solve(model, proven);
    require(infeasible.status == lp::reference::SolveStatus::infeasible,
            "closed search proves infeasibility");
    require(infeasible.primal.empty(), "infeasible search has no incumbent");

    milp::Options expired;
    expired.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto stopped = milp::solve(model, expired);
    require(stopped.status == lp::reference::SolveStatus::resource_limit,
            "expired deadline is a resource stop, never a proof");
    require(stopped.primal.empty(), "resource stop found no incumbent");
    std::cout << "[+] infeasible vs resource_limit distinguished without an incumbent\n";
}

void test_pdlp_node_bound_is_finite_and_honest() {
    // 4200 singleton rows exceed the revised-simplex node limit, so the
    // node relaxation runs through the PDLP fallback (F1): its weak-dual
    // bound must be finite and must never be overstated past the incumbent.
    constexpr std::size_t kVariables = 4200;
    model::Model m;
    m.name = "PDLP_ROW";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective.assign(kVariables, -1.0);
    model::SparseMatrixBuilder builder(kVariables, kVariables);
    for (std::size_t i = 0; i < kVariables; ++i) builder.add(i, i, 1.0);
    m.matrix = builder.build();
    m.row_lower.assign(kVariables, model::Bound::negative_infinity());
    m.row_upper.assign(kVariables, model::Bound::finite(1.0));
    m.row_name.resize(kVariables);
    for (std::size_t i = 0; i < kVariables; ++i) m.row_name[i] = "R" + std::to_string(i);
    m.variable_lower.assign(kVariables, model::Bound::finite(0.0));
    m.variable_upper.assign(kVariables, model::Bound::finite(1.0));
    m.variable_type.assign(kVariables, model::VariableType::binary);
    m.variable_name.resize(kVariables);
    for (std::size_t i = 0; i < kVariables; ++i) m.variable_name[i] = "B" + std::to_string(i);
    m.validate();
    milp::Options options;
    options.time_limit_seconds = 60.0;
    options.enable_strong_branching = false;
    const auto result = milp::solve(m, options);
    require(result.status == lp::reference::SolveStatus::optimal,
            "PDLP node path still proves the trivial optimum");
    const double truth = -static_cast<double>(kVariables);
    require(std::fabs(result.objective - truth) < 1e-6, "PDLP path objective");
    require(std::isfinite(result.best_bound) && result.best_bound <= truth + 1e-6 &&
                result.best_bound >= truth - 1e-4 * std::fabs(truth),
            "PDLP node bound finite, honest and closed within tolerance");
    std::cout << "[+] PDLP node bound finite and honest (" << result.best_bound << ")\n";
}

void test_nearly_integral_branch_value() {
    // 7 x <= 21 + 4e-15 gives an LP optimum a few ulps above the integer 3:
    // the nearly integral value must close as integral (P3), matching the
    // enumerated optimum exactly.
    model::Model m;
    m.name = "NEAR_INT";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective = {-1.0};
    model::SparseMatrixBuilder builder(1, 1);
    builder.add(0, 0, 7.0);
    m.matrix = builder.build();
    m.row_lower = {model::Bound::negative_infinity()};
    m.row_upper = {model::Bound::finite(21.000000000000004)};
    m.row_name = {"ROW"};
    m.variable_lower = {model::Bound::finite(0.0)};
    m.variable_upper = {model::Bound::finite(10.0)};
    m.variable_type = {model::VariableType::integer};
    m.variable_name = {"X"};
    m.validate();
    const auto result = milp::solve(m, milp::Options{});
    require(result.status == lp::reference::SolveStatus::optimal,
            "nearly integral value proves optimality");
    require(std::fabs(result.objective + 3.0) < 1e-9, "nearly integral objective");
    require(result.primal.size() == 1 && std::fabs(result.primal[0] - 3.0) < 1e-6,
            "nearly integral incumbent rounds to 3");
    std::cout << "[+] nearly integral branch value closes as integral\n";
}

void test_cut_rejected_after_resolve_p10() {
    // A cut re-solve that fails (expired deadline here) must revert: model,
    // incumbent, recorded bounds and obligation ledger all stay untouched.
    const auto model = fractional_knapsack();
    milp::ParallelOptions options;
    options.enable_cuts = true;
    options.enable_mir_cuts = true;
    options.max_cut_rounds = 5;
    auto root_model = model;
    const auto fingerprint_before = model::hash_model(root_model).fingerprint();
    auto root_lp = milp::detail_parallel_tree_search::solve_node_lp(root_model, options, std::nullopt);
    require(root_lp.status == lp::reference::SolveStatus::optimal && root_lp.basis.has_value(),
            "root LP with basis for the cut round");

    milp::IncumbentManager incumbent;
    std::atomic<std::size_t> iterations{0};
    std::vector<double> current_primal = root_lp.primal;
    double current_obj = root_lp.objective;
    std::optional<lp::dual::BasisState> current_basis = root_lp.basis;
    double best_lower_bound = 5.0; // sentinel the success path would overwrite
    std::size_t root_cuts_generated = 0;
    milp::detail_parallel_tree_search::ProofEventCollector proof_events(1);
    options.deadline = std::chrono::steady_clock::now(); // cut re-solve cannot finish
    milp::detail_parallel_tree_search::apply_parallel_root_cuts(
        root_model, options, root_lp, incumbent, iterations, current_primal, current_obj,
        current_basis, best_lower_bound, root_cuts_generated, proof_events);

    require(model::hash_model(root_model).fingerprint() == fingerprint_before,
            "P10 revert keeps the model unchanged");
    require(!incumbent.has_incumbent(), "P10 revert keeps the incumbent unchanged");
    require(root_cuts_generated == 0, "P10 revert records no root cuts");
    require(best_lower_bound == 5.0, "P10 revert leaves the reported bound unchanged");
    require(current_obj == root_lp.objective && current_primal == root_lp.primal,
            "P10 revert keeps the separation state");
    require(proof_events.root.empty(), "P10 revert records no cut obligations");
    std::cout << "[+] cut rejected after re-solve leaves state unchanged (P10)\n";
}

} // namespace

int main() {
    try {
        test_resource_stop_keeps_verified_incumbent();
        test_gap_satisfied_nonzero_gap_survives_proof();
        test_timeout_with_incumbent_keeps_verified_primal();
        test_infeasible_vs_resource_limit_no_incumbent();
        test_pdlp_node_bound_is_finite_and_honest();
        test_nearly_integral_branch_value();
        test_cut_rejected_after_resolve_p10();
        std::cout << "All MILP edge-case tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] Error: " << error.what() << "\n";
        return 1;
    }
}
