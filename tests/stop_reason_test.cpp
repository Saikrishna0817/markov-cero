#include "support/failing_new.hpp"

#include "markov_cero/api/solve.hpp"
#include "markov_cero/core/solve_context.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/model/model.hpp"

#include <stdexcept>
#include <string>
#include <vector>

using markov_cero::api::SolveOptions;
using markov_cero::api::SolveResult;
using markov_cero::lp::reference::SolveStatus;
using markov_cero::model::Bound;
using markov_cero::model::Model;
using markov_cero::test::FailingNew;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// R2 (docs/contracts/resource-limits.md section 3): every resource_limit
// result carries a non-empty stop_reason before it reaches the caller.
void require_described(const SolveResult& result, const char* message) {
    if (result.status == SolveStatus::resource_limit)
        require(!result.stop_reason.empty(), message);
}

Model tiny_lp() {
    Model model;
    model.name = "STOP_REASON_TINY_LP";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-2.0, -3.0};
    model.variable_name = {"x1", "x2"};
    model.variable_lower = {Bound::finite(0.0), Bound::finite(0.0)};
    model.variable_upper = {Bound::finite(10.0), Bound::finite(10.0)};
    model.variable_type = {markov_cero::model::VariableType::continuous,
                           markov_cero::model::VariableType::continuous};
    model.row_name = {"c1", "c2"};
    model.row_lower = {Bound::negative_infinity(), Bound::negative_infinity()};
    model.row_upper = {Bound::finite(8.0), Bound::finite(10.0)};
    markov_cero::model::SparseMatrixBuilder builder(2, 2);
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 2.0);
    builder.add(1, 0, 2.0);
    builder.add(1, 1, 1.0);
    model.matrix = builder.build();
    model.validate();
    return model;
}

// More bounded rows than the dense canonicalization dimension cap (4096): the
// dense conversion throws std::length_error, which the API boundary maps to
// the work_limit stop (contract section 3, R3).
Model bounded_rows_lp(std::size_t rows) {
    Model model;
    model.name = "STOP_REASON_WORK_LIMIT";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {1.0};
    model.variable_name = {"x"};
    model.variable_lower = {Bound::finite(0.0)};
    model.variable_upper = {Bound::finite(10.0)};
    model.variable_type = {markov_cero::model::VariableType::continuous};
    model.row_name.resize(rows);
    model.row_lower.assign(rows, Bound::negative_infinity());
    model.row_upper.assign(rows, Bound::finite(8.0));
    markov_cero::model::SparseMatrixBuilder builder(rows, 1);
    for (std::size_t i = 0; i < rows; ++i) {
        model.row_name[i] = "r" + std::to_string(i);
        builder.add(i, 0, 1.0);
    }
    model.matrix = builder.build();
    model.validate();
    return model;
}

// LP relaxation optimum x1 = 0.8 is fractional, so one explored node cannot
// close the tree: the serial search must stop with a resource outcome.
Model knapsack_milp() {
    Model model;
    model.name = "STOP_REASON_KNAPSACK";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-3.0, -4.0};
    model.variable_name = {"x1", "x2"};
    model.variable_lower = {Bound::finite(0.0), Bound::finite(0.0)};
    model.variable_upper = {Bound::finite(1.0), Bound::finite(1.0)};
    model.variable_type = {markov_cero::model::VariableType::integer,
                           markov_cero::model::VariableType::integer};
    model.row_name = {"capacity"};
    model.row_lower = {Bound::negative_infinity()};
    model.row_upper = {Bound::finite(10.0)};
    markov_cero::model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 5.0);
    builder.add(0, 1, 6.0);
    model.matrix = builder.build();
    model.validate();
    return model;
}

void test_parser_cap_is_input_limit() {
    SolveOptions options;
    options.maximum_input_bytes = 1;
    const SolveResult limited =
        markov_cero::api::solve_file("examples/blend.mps", options);
    require(limited.status == SolveStatus::resource_limit,
            "a parser byte cap stops with resource_limit");
    require(limited.stop_reason == "input_limit",
            "the parser byte cap is attributed to the input boundary");
    require(limited.diagnostic.failure_site == "input_resource_limit",
            "the input failure site is preserved");
    require(!limited.verified, "a rejected input is never verified");
    options.maximum_input_bytes.reset();
    const SolveResult control =
        markov_cero::api::solve_file("examples/blend.mps", options);
    require(control.status == SolveStatus::optimal, "the control solve succeeds");
    require(control.stop_reason.empty(), "a normal solve records no stop");
    require(control.memory_charged_peak_bytes > 0,
            "instrumented charges report a non-zero peak on a normal solve");
}

void test_dimension_limit_is_work_limit() {
    SolveOptions options;
    options.engine = "primal";
    options.enable_presolve = false;
    const SolveResult limited =
        markov_cero::api::solve_model(bounded_rows_lp(4097), options);
    require(limited.status == SolveStatus::resource_limit,
            "crossing the dense dimension cap stops with resource_limit");
    require(limited.stop_reason == "work_limit",
            "an escaped length_error is attributed to the work limit");
    require(limited.diagnostic.failure_site == "memory_or_factor_limit",
            "the factor/size failure site is preserved");
    require(!limited.verified, "a work-limited result is never verified");
    require_described(limited, "work limit results describe themselves");
}

void test_allocation_failure_is_attributed() {
    const Model model = tiny_lp();
    SolveOptions options;
    options.engine = "primal";
    std::size_t mapped = 0;
    std::size_t escaped = 0;
    for (std::size_t nth = 1; nth <= 16; ++nth) {
        SolveResult result;
        FailingNew::arm(nth);
        bool did_escape = false;
        try {
            result = markov_cero::api::solve_model(model, options);
        } catch (...) {
            did_escape = true;
        }
        const std::size_t injected = FailingNew::failures_injected();
        FailingNew::disarm();
        if (injected == 0) continue;
        if (did_escape) {
            ++escaped;
            continue;
        }
        require(result.status != SolveStatus::infeasible &&
                    result.status != SolveStatus::unbounded,
                "an allocation failure is never reported as a model property");
        require_described(result, "every resource stop carries a reason");
        if (result.status == SolveStatus::resource_limit &&
            result.diagnostic.failure_site == "allocation_failure") {
            require(result.stop_reason == "allocation_failure",
                    "a mapped host OOM names the allocation failure");
            ++mapped;
        }
    }
    require(escaped == 0, "no allocation failure escapes solve_model");
    require(mapped >= 1, "the harness mapped at least one allocation failure");
    const SolveResult after = markov_cero::api::solve_model(model, options);
    require(after.status == SolveStatus::optimal, "the API recovers after the sweep");
    require(after.stop_reason.empty(), "recovery leaves no stop reason");
}

void test_cooperative_stops_keep_their_reason() {
    SolveOptions options;
    options.engine = "primal";
    options.total_time_limit_seconds = 1e-12;
    const SolveResult expired = markov_cero::api::solve_model(tiny_lp(), options);
    require(expired.status == SolveStatus::resource_limit,
            "an expired solve-wide deadline stops with resource_limit");
    require(expired.stop_reason == "deadline_exceeded",
            "the deadline stop is attributed to the wall clock");
    options.total_time_limit_seconds.reset();
    options.memory_limit_bytes = 1;
    const SolveResult budgeted = markov_cero::api::solve_model(tiny_lp(), options);
    require(budgeted.status == SolveStatus::resource_limit,
            "an exhausted memory budget stops with resource_limit");
    require(budgeted.stop_reason == "memory_budget_exhausted",
            "the budget stop is attributed to the memory budget");
    require(budgeted.diagnostic.failure_site == "memory_budget",
            "the memory failure site is preserved");
    require(budgeted.memory_charged_peak_bytes <= 1,
            "admitted charges never exceed the configured budget");
}

void test_milp_node_cap_is_quota() {
    SolveOptions options;
    options.engine = "milp";
    options.milp_options.max_nodes = 1;
    options.milp_options.enable_cuts = false;
    options.milp_options.enable_heuristics = false;
    const SolveResult capped =
        markov_cero::api::solve_model(knapsack_milp(), options);
    require(capped.status == SolveStatus::resource_limit,
            "a one-node budget cannot close the tree");
    require(capped.stop_reason == "quota_exhausted",
            "the serial node cap is attributed to the node quota");
    require(capped.nodes_explored >= 1, "the node quota was actually reached");
}

void test_milp_engine_time_limit_is_deadline() {
    SolveOptions options;
    options.engine = "milp";
    options.milp_options.time_limit_seconds = 1e-6;
    options.milp_options.enable_cuts = false;
    options.milp_options.enable_heuristics = false;
    const SolveResult timed_out =
        markov_cero::api::solve_model(knapsack_milp(), options);
    require(timed_out.status == SolveStatus::resource_limit,
            "an engine-local MILP time limit stops with resource_limit");
    require(timed_out.stop_reason == "deadline_exceeded",
            "the engine-local time limit is attributed to the wall clock");
}

void test_serial_queue_capacity_is_described() {
    SolveOptions options;
    options.engine = "milp";
    options.milp_options.max_queued_nodes = 1;
    options.milp_options.enable_cuts = false;
    options.milp_options.enable_heuristics = false;
    const SolveResult queued = markov_cero::api::solve_file(
        "examples/phase3/tiny_milp.mps", options);
    require(queued.status == SolveStatus::resource_limit,
            "a queued-node capacity of one stops the serial search");
    require_described(queued, "engine-internal serial stops still describe themselves");
}

// RES-01 slice B (resource contract section 4): the serial search polls the
// shared context at its phase boundaries. A cancellation recorded before the
// search starts must surface as an unproven resource stop naming the cancel,
// never as a completed search.
void test_serial_search_observes_shared_cancellation() {
    markov_cero::core::SolveContext cancelled;
    cancelled.request_cancel();
    markov_cero::milp::Options milp_options;
    milp_options.context = &cancelled;
    milp_options.enable_cuts = false;
    milp_options.enable_heuristics = false;
    const markov_cero::milp::Result search =
        markov_cero::milp::solve(knapsack_milp(), milp_options);
    require(search.status == SolveStatus::resource_limit,
            "a pre-cancelled shared context stops the serial search");
    require(cancelled.stop_reason() == markov_cero::core::StopReason::cancelled,
            "the shared context records the stop the search observed");
    require(search.message == "cancelled with no incumbent",
            "the interrupted search reports its stop without a completion claim");
}
// RES-01 slice C: the serial frontier charges the shared budget before it
// allocates queue nodes (contract section 4). A refused charge stops the
// search with the budget attribution and never a model-property status.
void test_serial_queue_charge_refusal_is_described() {
    markov_cero::core::SolveContext::Config config;
    config.memory_limit_bytes = 1;
    markov_cero::core::SolveContext budgeted(config);
    markov_cero::milp::Options milp_options;
    milp_options.context = &budgeted;
    milp_options.enable_cuts = false;
    milp_options.enable_heuristics = false;
    milp_options.enable_strong_branching = false;
    const markov_cero::milp::Result search =
        markov_cero::milp::solve(knapsack_milp(), milp_options);
    require(search.status == SolveStatus::resource_limit,
            "a refused node charge stops the serial search");
    require(budgeted.stop_reason() == markov_cero::core::StopReason::memory_budget_exhausted,
            "the shared budget records the refusal");
    require(search.message.find("memory budget") != std::string::npos,
            "the serial search reports the refused allocation");
}
} // namespace

int main() {
    test_parser_cap_is_input_limit();
    test_dimension_limit_is_work_limit();
    test_allocation_failure_is_attributed();
    test_cooperative_stops_keep_their_reason();
    test_milp_node_cap_is_quota();
    test_milp_engine_time_limit_is_deadline();
    test_serial_queue_capacity_is_described();
    test_serial_search_observes_shared_cancellation();
    test_serial_queue_charge_refusal_is_described();
    return 0;
}
