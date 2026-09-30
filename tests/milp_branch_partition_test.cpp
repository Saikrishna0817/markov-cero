// MIP-01 contract §4 (docs/contracts/milp-node-bounds.md): exhaustive branch
// partition certificate. Every integer point of the parent domain lies in
// exactly one child of a non-degenerate split; degenerate splits and empty
// integer domains are rejected with statuses, never pushed and never silent;
// both engines record empty domains on reachable production paths.
#include "markov_cero/milp/branch_selector.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/milp/parallel_tree_search.hpp"
#include "markov_cero/milp/work_queue.hpp"
#include "markov_cero/verify/mip_proof.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../src/verify/mip_proof_internal.hpp"

using namespace markov_cero;

namespace {
void require_true(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void test_exhaustive_partition() {
    const double vals[] = {-3.4, -2.0, -1.5, -0.5, 0.0, 0.5, 1.0, 1.5, 2.7, 5.0, 6.25, 10.999};
    struct Dom { double lo, hi; };
    const Dom doms[] = {{-10.0, 10.0}, {-5.5, 5.5}, {0.0, 0.0}, {2.75, 2.99},
                        {-3.2, -2.8}, {2.5, 10.0}, {-8.0, -1.75}, {7.9, 7.9}};
    for (double v : vals) {
        for (const auto& d : doms) {
            const auto split = milp::evaluate_split(
                v, model::Bound::finite(d.lo), model::Bound::finite(d.hi));
            // Degenerate detection is exactly integrality of v.
            assert((split.floor_value < split.ceil_value) == (std::floor(v) != v));
            if (split.floor_value >= split.ceil_value) continue;
            // Every integer inside the parent domain lies in exactly one child,
            // and the child that owns it exists.
            const long first = static_cast<long>(std::ceil(d.lo - 1e-9));
            const long last = static_cast<long>(std::floor(d.hi + 1e-9));
            for (long k = first; k <= last; ++k) {
                const double point = static_cast<double>(k);
                const bool in_down = point <= split.floor_value;
                const bool in_up = point >= split.ceil_value;
                assert(in_down != in_up);
                if (in_down) assert(split.down_valid);
                if (in_up) assert(split.up_valid);
            }
        }
    }
    const auto open = milp::evaluate_split(2.5, model::Bound::negative_infinity(),
                                           model::Bound::positive_infinity());
    assert(open.down_valid && open.up_valid);
    std::cout << "[+] test_exhaustive_partition passed\n";
}

void test_push_statuses() {
    std::atomic<std::size_t> next_id{1};
    milp::ThreadSafeNodeQueue queue;
    milp::BranchNode parent;
    parent.id = 7;
    parent.depth = 2;
    parent.lower_bound = -3.0;
    const std::vector<model::Bound> lo0 = {model::Bound::finite(0)};
    const std::vector<model::Bound> hi10 = {model::Bound::finite(10)};

    // Integral split value: degenerate, rejected (§2 P6), nothing pushed.
    const auto degenerate = milp::push_branch_children(
        queue, parent, 0, 2.0, parent.lower_bound, lo0, hi10, std::nullopt, next_id);
    assert(degenerate == milp::ChildPushStatus::split_rejected);
    assert(queue.size() == 0 && queue.empty_domain_count() == 0);

    // [2.75, 2.99] holds no integer: both gates reject (§4.2), recorded.
    const std::vector<model::Bound> lo275 = {model::Bound::finite(2.75)};
    const std::vector<model::Bound> hi299 = {model::Bound::finite(2.99)};
    const auto empty = milp::push_branch_children(
        queue, parent, 0, 2.8, parent.lower_bound, lo275, hi299, std::nullopt, next_id);
    assert(empty == milp::ChildPushStatus::empty_integer_domain);
    assert(queue.size() == 0 && queue.empty_domain_count() == 1);

    // Both gates open: down and up children with the certified boundaries.
    const auto both = milp::push_branch_children(
        queue, parent, 0, 2.5, parent.lower_bound, lo0, hi10, std::nullopt, next_id);
    assert(both == milp::ChildPushStatus::accepted);
    assert(queue.size() == 2);
    bool became_active = false;
    const auto batch = queue.pop_batch(false, 100.0, became_active);
    assert(batch.size() == 2);
    bool saw_down = false, saw_up = false;
    const std::vector<model::Bound> root_lo = {model::Bound::finite(0)};
    const std::vector<model::Bound> root_hi = {model::Bound::finite(10)};
    for (const auto& child : batch) {
        std::vector<model::Bound> out_lo, out_hi;
        child->bounds.materialize(root_lo, root_hi, out_lo, out_hi);
        if (child->is_down_branch) {
            saw_down = true;
            assert(out_hi[0].is_finite() && out_hi[0].value == 2.0);
        } else {
            saw_up = true;
            assert(out_lo[0].is_finite() && out_lo[0].value == 3.0);
        }
        assert(child->parent_id == 7 && child->depth == 3);
        assert(child->lower_bound == parent.lower_bound);
    }
    assert(saw_down && saw_up);

    // One-sided gate: floor below the parent lower bound drops only the down child.
    const std::vector<model::Bound> lo25 = {model::Bound::finite(2.5)};
    const auto up_only = milp::push_branch_children(
        queue, parent, 0, 2.5, parent.lower_bound, lo25, hi10, std::nullopt, next_id);
    assert(up_only == milp::ChildPushStatus::accepted);
    assert(queue.size() == 1);
    std::cout << "[+] test_push_statuses passed\n";
}

model::Model build_empty_domain_model() {
    // x1 LP-optimal at 1.4 (fractional, most-fractional) and x2 confined to
    // [2.75, 2.99], an integer band that holds no integer point: whichever
    // node first branches on x2 concludes a conclusive empty domain.
    model::Model m;
    m.objective = {1.0, 1.0};
    model::SparseMatrixBuilder a(1, 2);
    a.add(0, 0, 1);
    m.matrix = a.build();
    m.row_lower = {model::Bound::finite(0)};
    m.row_upper = {model::Bound::positive_infinity()};
    m.row_name = {"LOWER"};
    m.variable_lower = {model::Bound::finite(1.4), model::Bound::finite(2.75)};
    m.variable_upper = {model::Bound::finite(10.0), model::Bound::finite(2.99)};
    m.variable_type = {model::VariableType::integer, model::VariableType::integer};
    m.variable_name = {"X1", "X2"};
    m.validate();
    return m;
}

void test_serial_empty_domain_recorded() {
    const auto m = build_empty_domain_model();
    const auto result = milp::solve(m, milp::Options{});
    assert(result.status == lp::reference::SolveStatus::infeasible);
    assert(result.empty_domain_nodes == 1);
    std::cout << "[+] test_serial_empty_domain_recorded passed\n";
}

void test_parallel_empty_domain_recorded() {
    const auto m = build_empty_domain_model();
    milp::ParallelOptions options;
    options.num_threads = 2;
    // With root strong branching on, P11 would prove infeasibility first; this
    // test targets the §4.2 gate record on the push path.
    options.enable_strong_branching = false;
    const auto result = milp::solve_parallel(m, options);
    assert(result.status == lp::reference::SolveStatus::infeasible);
    assert(result.empty_domain_nodes == 1);
    std::cout << "[+] test_parallel_empty_domain_recorded passed\n";
}

void test_parallel_root_empty_recorded() {
    // Single empty band: the parallel root split is itself empty and must
    // fail early as infeasible with the record attached.
    model::Model m;
    m.objective = {1.0};
    model::SparseMatrixBuilder a(1, 1);
    a.add(0, 0, 1);
    m.matrix = a.build();
    m.row_lower = {model::Bound::finite(0)};
    m.row_upper = {model::Bound::positive_infinity()};
    m.row_name = {"LOWER"};
    m.variable_lower = {model::Bound::finite(2.75)};
    m.variable_upper = {model::Bound::finite(2.99)};
    m.variable_type = {model::VariableType::integer};
    m.variable_name = {"X"};
    m.validate();
    milp::ParallelOptions options;
    options.num_threads = 1;
    options.enable_strong_branching = false;
    const auto result = milp::solve_parallel(m, options);
    assert(result.status == lp::reference::SolveStatus::infeasible);
    assert(result.empty_domain_nodes == 1);
    assert(result.message.find("root integer domain") != std::string::npos);
    std::cout << "[+] test_parallel_root_empty_recorded passed\n";
}
// §7.4: push_branch_children is the shared production constructor for the
// serial search and both parallel push sites; its materialized children must
// equal the domains replay derives from the same split record.
void test_push_matches_replay_rules() {    const double lows[] = {0.0, 1.4, -2.5, 2.5};
    const double highs[] = {3.0, 5.7, 10.0, 8.25};
    const double values[] = {0.5, 1.75, 2.4, 4.5, 5.999};
    std::atomic<std::size_t> next_id{1};
    milp::ThreadSafeNodeQueue queue;
    std::size_t checked = 0, two_child_cells = 0;
    for (double lo : lows)
        for (double hi : highs)
            for (double v : values) {
                if (v < lo || v > hi) continue;
                const auto split = milp::evaluate_split(
                    v, model::Bound::finite(lo), model::Bound::finite(hi));
                if (!(split.floor_value < split.ceil_value)) continue;
                milp::BranchNode parent;
                parent.id = next_id.load();
                parent.lower_bound = -1e9;
                const std::vector<model::Bound> root_lo = {model::Bound::finite(lo)};
                const std::vector<model::Bound> root_hi = {model::Bound::finite(hi)};
                const auto status = milp::push_branch_children(
                    queue, parent, 0, v, parent.lower_bound, root_lo, root_hi,
                    std::nullopt, next_id);
                const std::size_t expected =
                    (split.down_valid ? 1U : 0U) + (split.up_valid ? 1U : 0U);
                require_true(status == milp::ChildPushStatus::accepted && expected >= 1, "production push accepts the non-degenerate split");
                bool became_active = false;
                const auto batch = queue.pop_batch(false, 100.0, became_active);
                require_true(batch.size() == expected,
                             "pushed child count matches the open split gates");
                if (expected == 2) ++two_child_cells;
                verify::mip_detail::Domain replay_parent;
                replay_parent.node = 0;
                replay_parent.lower = root_lo;
                replay_parent.upper = root_hi;
                verify::MipProofNode record{verify::MipProofKind::split};
                record.variable = 0;
                record.split_value = split.floor_value;
                auto [replay_down, replay_up] =
                    verify::mip_detail::children(replay_parent, record);
                for (const auto& child : batch) {
                    std::vector<model::Bound> out_lo, out_hi;
                    child->bounds.materialize(root_lo, root_hi, out_lo, out_hi);
                    if (child->is_down_branch) {
                        require_true(out_hi[0].is_finite() &&
                                         out_hi[0].value == replay_down.upper[0].value &&
                                         out_hi[0].value == split.floor_value,
                                     "pushed down child equals replay down domain");
                    } else {
                        require_true(out_lo[0].is_finite() &&
                                         out_lo[0].value == replay_up.lower[0].value &&
                                         out_lo[0].value == split.ceil_value,
                                     "pushed up child equals replay up domain");
                    }
                }
                ++checked;
            }
    require_true(checked >= 15 && two_child_cells >= 10,
                 "push/replay grid exercised both gate combinations");
    std::cout << "[+] test_push_matches_replay_rules passed (" << checked
              << " cells, " << two_child_cells << " two-child)\n";
}

// §2 P6 + §7.4: exactly integral LP point whose recomputed objective
// disagrees with the solver's summation (1e16-scale cancellation): no
// fractional variable remains, the node is counted unresolved (counter in
// the message), `proven` stays blocked, the held bound is never overstated.
void test_p6_unsolved_blocks_proof() {
    model::Model m;
    m.name = "P6_UNSOLVED";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective = {1e16, 1.0, -1e16};
    model::SparseMatrixBuilder a(1, 3);
    a.add(0, 0, 1);
    a.add(0, 1, 1);
    a.add(0, 2, 1);
    m.matrix = a.build();
    m.row_lower = {model::Bound::negative_infinity()};
    m.row_upper = {model::Bound::finite(3.0)};
    m.row_name = {"DUMMY"};
    m.variable_lower = {model::Bound::finite(1.0), model::Bound::finite(1.0), model::Bound::finite(0.0)};
    m.variable_upper = {model::Bound::finite(1.0), model::Bound::finite(1.0), model::Bound::finite(1.0)};
    m.variable_type = {model::VariableType::integer, model::VariableType::integer, model::VariableType::binary};
    m.variable_name = {"X1", "X2", "X3"};
    m.validate();
    const auto result = milp::solve(m, milp::Options{});
    require_true(result.status == lp::reference::SolveStatus::resource_limit,
                 "P6 unsolved node blocks proven: resource_limit, never Optimal");
    require_true(result.message.find("could not be certified") != std::string::npos,
                 "unsolved counter surfaces in the stop message");
    require_true(result.nodes_explored == 1, "the unresolved node is counted once");
    require_true(result.primal == std::vector<double>({1.0, 1.0, 1.0}),
                 "verified incumbent kept at x = (1,1,1)");
    require_true(std::fabs(result.objective - 1.0) < 1e-9, "incumbent objective is 1");
    require_true(std::isfinite(result.best_bound) && result.best_bound <= 1.0 + 1e-6,
                 "held bound honest: never overstated past the optimum");
    std::cout << "[+] test_p6_unsolved_blocks_proof passed\n";
}
} // namespace

int main() {
    try {
        test_exhaustive_partition();
        test_push_statuses();
        test_push_matches_replay_rules();
        test_p6_unsolved_blocks_proof();
        test_serial_empty_domain_recorded();
        test_parallel_empty_domain_recorded();
        test_parallel_root_empty_recorded();
        std::cout << "All MIP-01 branch partition tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] Error: " << error.what() << "\n";
        return 1;
    }
}
