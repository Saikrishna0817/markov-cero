#include "strong_branching_test_internal.hpp"
namespace test_strong_branching_test {
using namespace detail_strong_branching_test;
namespace detail_strong_branching_test {
void test_strong_branching_and_domain_reduction() {
    // Construct a model where branching down on a variable causes immediate LP infeasibility:
    // Min -2 x1 - 2 x2 - x3
    // s.t. x1 + x2 >= 1.5   (if x1 <= 0, x2 >= 1.5 > 1 => down branch infeasible!)
    //      x1 + x2 + 0.1 x3 <= 2.0
    // x1, x2, x3 in [0, 1] binary
    markov_cero::model::Model model;
    model.name = "STRONG_BRANCH_TEST";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-2.0, -2.0, -1.0};

    markov_cero::model::SparseMatrixBuilder builder(2, 3);
    // Row 0: x1 + x2 >= 1.5
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 1.0);
    // Row 1: x1 + x2 + 0.1 x3 <= 2.0
    builder.add(1, 0, 1.0);
    builder.add(1, 1, 1.0);
    builder.add(1, 2, 0.1);
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::finite(1.5),
                       markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::positive_infinity(),
                       markov_cero::model::Bound::finite(2.0)};
    model.row_name = {"ROW_LOWER", "ROW_UPPER"};

    model.variable_lower = {markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0)};
    model.variable_type = {markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary};
    model.variable_name = {"X1", "X2", "X3"};
    model.validate();

    // Solve root continuous LP
    const auto canon =
        markov_cero::transform::sparse_canonicalize(model, /*relax_integrality=*/true);
    const auto dense = canon.to_dense();
    const auto lpres = markov_cero::lp::reference::solve(dense);
    assert(lpres.status == markov_cero::lp::reference::SolveStatus::optimal);

    const auto primal = markov_cero::transform::reconstruct_primal(canon, lpres.primal);
    const double root_obj = markov_cero::transform::reconstruct_objective(canon, lpres.objective);
    const auto basis_state = markov_cero::lp::dual::make_basis_state(dense, lpres.basis);

    // Initial pseudo-cost tracker
    std::vector<markov_cero::milp::VariablePseudoCost> pseudo_costs(3);

    // Evaluate strong branching
    markov_cero::milp::StrongBranchingOptions sb_opts;
    sb_opts.max_lookahead_iterations = 30;
    sb_opts.score_mu = 0.16;
    sb_opts.update_pseudo_costs = true;

    const auto sb_res = markov_cero::milp::evaluate_strong_branching(
        model, primal, root_obj, basis_state, sb_opts, &pseudo_costs);
    auto expired_sb_options = sb_opts;
    expired_sb_options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto expired_sb = markov_cero::milp::evaluate_strong_branching(
        model, primal, root_obj, basis_state, expired_sb_options, nullptr);
    assert(expired_sb.deadline_reached);
    assert(expired_sb.candidates.empty());

    // 1. Verify that candidates were evaluated
    assert(!sb_res.candidates.empty());

    // 2. Verify domain reduction on x1 or x2 (down branch x <= 0 is infeasible, so x >= 1)
    bool detected_infeasible_branch = false;
    for (const auto& cand : sb_res.candidates) {
        if (cand.variable_index == 0 || cand.variable_index == 1) {
            if (cand.is_down_infeasible) {
                assert(cand.down_resolved);
                detected_infeasible_branch = true;
            }
        }
    }
    assert(detected_infeasible_branch);

    // 3. Verify domain reductions list contains lower bound tightening
    assert(!sb_res.domain_reductions.empty());
    for (const auto& dr : sb_res.domain_reductions) {
        if (dr.variable_index == 0 || dr.variable_index == 1) {
            // Tightened lower bound should be 1.0!
            assert(dr.new_lower.is_finite() && dr.new_lower.value >= 1.0 - 1e-6);
            assert(dr.is_fixed); // Since upper was 1.0, variable is fixed to 1!
        }
    }

    // 4. Verify candidate score combination and ranking
    assert(sb_res.best_score > 0.0);

    // 5. Verify pseudo-costs tracker received updates
    bool has_pseudo_cost = false;
    for (const auto& pc : pseudo_costs) {
        if (pc.down_count > 0 || pc.up_count > 0) {
            has_pseudo_cost = true;
            break;
        }
    }
    assert(has_pseudo_cost);

    // An interrupted child LP cannot be converted into a zero-degradation
    // training/selection label just because it has a primal telemetry point.
    auto interrupted_opts = sb_opts;
    interrupted_opts.max_lookahead_iterations = 0;
    interrupted_opts.update_pseudo_costs = false;
    const auto interrupted = markov_cero::milp::evaluate_strong_branching(
        model, primal, root_obj, basis_state, interrupted_opts, nullptr);
    bool saw_unresolved = false;
    for (const auto& cand : interrupted.candidates) {
        saw_unresolved = saw_unresolved || !cand.down_resolved || !cand.up_resolved;
    }
    assert(saw_unresolved);

    std::cout << "[+] test_strong_branching_and_domain_reduction passed (best_var="
              << sb_res.best_variable << ", score=" << sb_res.best_score
              << ", domain reductions=" << sb_res.domain_reductions.size() << ")\n";
}
}

namespace detail_strong_branching_test {
void test_zero_trust_primal_verifier_integration() {
    // Test integration with markov_cero::verify::verify_primal
    markov_cero::model::Model model;
    model.name = "VERIFY_TEST";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {1.0, 2.0};

    markov_cero::model::SparseMatrixBuilder builder(1, 2);
    builder.add(0, 0, 1.0);
    builder.add(0, 1, 1.0);
    model.matrix = builder.build();

    model.row_lower = {markov_cero::model::Bound::finite(1.0)};
    model.row_upper = {markov_cero::model::Bound::positive_infinity()};
    model.row_name = {"R1"};

    model.variable_lower = {markov_cero::model::Bound::finite(0.0),
                            markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::finite(1.0),
                            markov_cero::model::Bound::finite(1.0)};
    model.variable_type = {markov_cero::model::VariableType::binary,
                           markov_cero::model::VariableType::binary};
    model.variable_name = {"X1", "X2"};
    model.validate();

    // Feasible candidate point: x1 = 1, x2 = 0, obj = 1.0
    markov_cero::verify::Candidate candidate;
    candidate.primal = {1.0, 0.0};
    candidate.claimed_objective = 1.0;

    const auto report = markov_cero::verify::verify_primal(model, candidate);
    assert(report.passed);
    assert(report.violations.empty());
    assert(report.maximum_integrality_violation < 1e-6);
    assert(std::abs(report.recomputed_objective - 1.0) < 1e-6);

    // Infeasible candidate point: x1 = 0, x2 = 0
    markov_cero::verify::Candidate inf_candidate;
    inf_candidate.primal = {0.0, 0.0};
    inf_candidate.claimed_objective = 0.0;

    const auto inf_report = markov_cero::verify::verify_primal(model, inf_candidate);
    assert(!inf_report.passed);
    assert(!inf_report.violations.empty());

    std::cout << "[+] test_zero_trust_primal_verifier_integration passed\n";
}
}

}
