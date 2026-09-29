#include "support/failing_new.hpp"

#include "markov_cero/api/solve.hpp"
#include "markov_cero/core/memory_budget.hpp"
#include "markov_cero/milp/node_bounds.hpp"
#include "markov_cero/milp/node_view.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/model/model_snapshot.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using markov_cero::api::SolveOptions;
using markov_cero::api::SolveResult;
using markov_cero::core::MemoryBudget;
using markov_cero::lp::reference::SolveStatus;
using markov_cero::milp::MaterializationResult;
using markov_cero::milp::MaterializationStatus;
using markov_cero::milp::NodeBounds;
using markov_cero::milp::NodeView;
using markov_cero::model::Bound;
using markov_cero::model::Model;
using markov_cero::model::ModelSnapshot;
using markov_cero::test::FailingNew;

using Contribution = NodeView::Contribution;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Keeps a pointer observable so the optimizer cannot prove the allocation
// dead and delete the probe (GCC's dead-allocation elimination would then
// skip the harness entirely).
void keep_alive(const void* pointer) {
    __asm__ __volatile__("" : : "r"(pointer) : "memory");
}

Model tiny_lp() {
    Model model;
    model.name = "ALLOCATION_FAILURE_HARNESS";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-2.0, -3.0};
    model.variable_name = {"x1", "x2"};
    model.variable_lower = {Bound::finite(0.0), Bound::finite(0.0)};
    model.variable_upper = {Bound::finite(10.0), Bound::finite(10.0)};
    model.variable_type = {markov_cero::model::VariableType::continuous,
                           markov_cero::model::VariableType::continuous};
    model.row_name = {"c1", "c2"};
    model.row_lower = {markov_cero::model::Bound::negative_infinity(),
                       markov_cero::model::Bound::negative_infinity()};
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

void test_harness_self_check() {
    volatile int observed = 0;
    FailingNew::arm(1);
    bool threw = false;
    try {
        int* probe = new int(7);
        keep_alive(probe);
        observed = *probe;
        delete probe;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    const std::size_t injected = FailingNew::failures_injected();
    FailingNew::disarm();
    require(threw && observed == 0, "the first armed allocation throws std::bad_alloc");
    require(injected == 1, "exactly one failure is injected per arm");

    FailingNew::arm(3);
    int* first = new int(1);
    keep_alive(first);
    int* second = new int(2);
    keep_alive(second);
    observed = *first + *second;
    bool third_threw = false;
    try {
        int* third = new int(3);
        keep_alive(third);
        observed = *third;
        delete third;
    } catch (const std::bad_alloc&) {
        third_threw = true;
    }
    const std::size_t counted = FailingNew::allocations_while_armed();
    FailingNew::disarm();
    delete first;
    delete second;
    require(third_threw, "the nth armed allocation is the one that fails");
    require(counted == 3, "allocations before the nth are counted and pass through");

    FailingNew::arm(1);
    void* nothrow = ::operator new(16, std::nothrow);
    const bool nothrow_failed = FailingNew::failures_injected() == 1;
    FailingNew::disarm();
    ::operator delete(nothrow);
    require(nothrow_failed && nothrow == nullptr,
            "the nothrow form reports failure as nullptr");

    int* control = new int(11);
    keep_alive(control);
    observed = *control;
    delete control;
    require(observed == 11, "disarmed allocations behave normally");
}

void test_snapshot_capture_fails_closed() {
    const Model model = tiny_lp();
    const std::uint64_t expected = ModelSnapshot::capture(model).fingerprint();
    require(expected != 0, "a captured snapshot carries a fingerprint");

    std::size_t reached = 0;
    // Seeded with a successful capture so the final comparison stays true even
    // when every injected iteration throws before reaching the assignment.
    volatile std::uint64_t observed = ModelSnapshot::capture(model).fingerprint();
    require(observed == expected, "back-to-back captures agree");
    for (std::size_t nth = 1; nth <= 6; ++nth) {
        FailingNew::arm(nth);
        bool threw = false;
        try {
            const ModelSnapshot snapshot = ModelSnapshot::capture(model);
            observed = snapshot.fingerprint();
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        const std::size_t injected = FailingNew::failures_injected();
        FailingNew::disarm();
        if (injected > 0) {
            ++reached;
            require(threw, "a failed capture reports the allocation failure");
        }
        require(ModelSnapshot::capture(model).fingerprint() == expected,
                "capture is usable again once the harness is disarmed");
    }
    require(reached >= 1, "the harness reached the snapshot copy path");
    require(observed == expected,
            "captures that succeed return the expected fingerprint");
}

void test_node_view_materialize_fails_closed() {
    const std::size_t variables = 6;
    const std::vector<Bound> root_lower(variables, Bound::finite(0.0));
    const std::vector<Bound> root_upper(variables, Bound::finite(10.0));
    auto node = NodeView::root();
    Contribution contribution;
    contribution.tightened_lower = std::make_pair<std::size_t, Bound>(2, Bound::finite(3.0));
    node = node->child(contribution);

    std::size_t reached = 0;
    for (std::size_t nth = 1; nth <= 6; ++nth) {
        MemoryBudget budget(1U << 20U);
        NodeBounds::MaterializationScratch scratch;
        std::vector<Bound> lower_out(variables, Bound::finite(-7.0));
        std::vector<Bound> upper_out(variables, Bound::finite(-7.0));

        FailingNew::arm(nth);
        const MaterializationResult result =
            node->materialize(root_lower, root_upper, lower_out, upper_out, scratch, budget);
        const std::size_t injected = FailingNew::failures_injected();
        FailingNew::disarm();

        if (injected == 0) continue;
        ++reached;
        require(result.status == MaterializationStatus::allocation_failed,
                "a host allocation failure is not reported as budget exhaustion");
        require(result.charged_bytes == 0, "a failed materialization charges nothing");
        require(budget.charged() == 0, "the charge is released on allocation failure");
        require(!budget.exhausted(),
                "the machine running out is not recorded as solve-budget exhaustion");
        for (std::size_t i = 0; i < variables; ++i) {
            require(lower_out[i].value == -7.0 && upper_out[i].value == -7.0,
                    "outputs stay untouched when allocation fails");
        }
    }
    require(reached >= 1, "the harness reached the materialization allocation path");

    MemoryBudget short_budget(node->materialization_bytes(variables) - 1U);
    NodeBounds::MaterializationScratch scratch;
    std::vector<Bound> lower_out(variables, Bound::finite(-7.0));
    std::vector<Bound> upper_out(variables, Bound::finite(-7.0));
    const auto refused =
        node->materialize(root_lower, root_upper, lower_out, upper_out, scratch, short_budget);
    require(refused.status == MaterializationStatus::budget_exhausted,
            "budget refusal stays distinct from allocation failure");
    require(short_budget.exhausted(), "budget refusal is still sticky and fail-closed");
}

template <class Solve>
std::size_t inject_failures(Solve&& solve, std::size_t attempts, std::size_t& mapped,
                            std::size_t& escaped) {
    std::size_t reached = 0;
    for (std::size_t nth = 1; nth <= attempts; ++nth) {
        SolveResult result;
        FailingNew::arm(nth);
        bool did_escape = false;
        try {
            result = solve();
        } catch (...) {
            did_escape = true;
        }
        const std::size_t injected = FailingNew::failures_injected();
        FailingNew::disarm();
        if (injected == 0) continue;
        ++reached;
        if (did_escape) {
            ++escaped;
            continue;
        }
        require(result.status != SolveStatus::infeasible && result.status != SolveStatus::unbounded,
                "an allocation failure is never reported as a model property");
        if (result.status == SolveStatus::resource_limit &&
            result.diagnostic.failure_site == "allocation_failure") {
            ++mapped;
        }
    }
    return reached;
}

void test_api_boundary_reports_allocation_failure() {
    const Model model = tiny_lp();
    SolveOptions options;
    options.engine = "simplex";

    const SolveResult control = markov_cero::api::solve_model(model, options);
    require(control.status == SolveStatus::optimal, "the control solve succeeds");
    require(control.verified, "the control solve is verified");

    std::size_t mapped = 0;
    std::size_t escaped = 0;
    const std::size_t reached =
        inject_failures([&] { return markov_cero::api::solve_model(model, options); }, 16, mapped,
                        escaped);
    require(reached >= 1, "the harness reached the model solve path");
    require(escaped == 0, "no allocation failure escapes solve_model");
    require(mapped >= 1, "the boundary maps an allocation failure to a resource limit");

    mapped = 0;
    escaped = 0;
    // The path string is built before arming: converting the literal to a
    // std::string at the call site is the caller's allocation, not the API's.
    const std::string blend_path = "examples/blend.mps";
    const std::size_t file_reached =
        inject_failures([&] { return markov_cero::api::solve_file(blend_path, options); }, 8,
                        mapped, escaped);
    require(file_reached >= 1, "the harness reached the file solve path");
    require(escaped == 0, "no allocation failure escapes solve_file");
    require(mapped >= 1, "the file boundary maps an allocation failure to a resource limit");

    const SolveResult after = markov_cero::api::solve_model(model, options);
    require(after.status == SolveStatus::optimal,
            "the API behaves normally after the harness is disarmed");
}

void test_engine_allocation_sweep() {
    struct Case { const char* engine; std::string path; };
    const std::vector<Case> cases = {
        {"primal", "examples/blend.mps"},
        {"milp", "examples/phase3/tiny_milp.mps"},
        {"parallel", "examples/phase3/tiny_milp.mps"},
        {"pdlp", "examples/blend.mps"},
        {"qp", "examples/phase3/tiny_qp.mps"},
    };
    for (const auto& item : cases) {
        SolveOptions options;
        options.engine = item.engine;
        options.num_threads = 2;
        const auto control = markov_cero::api::solve_file(item.path, options);
        require(control.resolved_engine == item.engine, "sweep reaches requested engine");
        std::size_t mapped = 0, escaped = 0;
        const auto reached = inject_failures(
            [&] { return markov_cero::api::solve_file(item.path, options); },
            48, mapped, escaped);
        require(reached > 0 && mapped > 0, "allocation failures map for every engine");
        require(escaped == 0, "no engine allocation failure escapes API");
    }
}
} // namespace

int main() {
    test_harness_self_check();
    test_snapshot_capture_fails_closed();
    test_node_view_materialize_fails_closed();
    test_api_boundary_reports_allocation_failure();
    test_engine_allocation_sweep();
    return 0;
}
