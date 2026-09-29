#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace markov_cero;
namespace {
void req(bool q, const char* m) {
    if (!q)
        throw std::runtime_error(m);
}
transform::CanonicalModel make_model(double lo, double up) {
    transform::CanonicalModel m;
    m.matrix = {2, 3, {-1, 1, 0, 1, 0, 1}};
    m.rhs = {-lo, up};
    m.objective = {1, 0, 0};
    m.record.objective_sign = 1;
    m.record.structural_variables = 1;
    m.record.variables.resize(1);
    m.record.variables[0].canonical_index = {0};
    m.record.variables[0].multiplier = {1};
    m.validate();
    return m;
}
} // namespace
int main() {
    auto base = make_model(0, 10);
    auto cold = lp::dual::solve(base);
    req(cold.used_cold_fallback, "cold bootstrap");
    req(cold.solution.status == lp::reference::SolveStatus::optimal, "cold optimal");
    req(verify::verify_reference_result(base, cold.solution).accepted, "cold verified");
    req(cold.basis_state.basic_variables.size() == 2, "cold basis");
    auto text = lp::dual::serialize_basis(cold.basis_state);
    auto parsed = lp::dual::parse_basis(text);
    req(parsed.basic_variables == cold.basis_state.basic_variables, "basis round trip");
    auto hot_model = make_model(3, 10);
    auto hot = lp::dual::solve(hot_model, {}, parsed);
    req(hot.used_warm_start && !hot.used_cold_fallback, "warm path");
    req(hot.solution.status == lp::reference::SolveStatus::optimal, "warm optimal");
    req(std::abs(hot.solution.objective - 3) < 1e-9, "warm objective");
    req(verify::verify_reference_result(hot_model, hot.solution).accepted, "warm verified");
    req(!hot.telemetry.empty(), "dual pivot telemetry");
    auto ref = lp::reference::solve(hot_model);
    req(ref.status == hot.solution.status &&
            std::abs(ref.objective - hot.solution.objective) < 1e-9,
        "warm cold parity");
    auto infeasible = make_model(11, 10);
    auto inf = lp::dual::solve(infeasible, {}, parsed);
    req(inf.solution.status == lp::reference::SolveStatus::infeasible, "dual infeasible");
    req(verify::verify_reference_result(infeasible, inf.solution).accepted, "dual Farkas verified");
    lp::dual::Options bland;
    bland.pricing = lp::dual::PricingPolicy::bland;
    bland.harris_ratio = false;
    auto bh = lp::dual::solve(hot_model, bland, parsed);
    req(bh.solution.status == lp::reference::SolveStatus::optimal, "Bland strict ratio");
    lp::dual::Options dse_opt;
    dse_opt.pricing = lp::dual::PricingPolicy::steepest_edge;
    auto dse_res = lp::dual::solve(hot_model, dse_opt, parsed);
    req(dse_res.solution.status == lp::reference::SolveStatus::optimal, "DSE optimal");
    req(verify::verify_reference_result(hot_model, dse_res.solution).accepted, "DSE verified");
    lp::dual::Options tn_opt;
    tn_opt.pricing = lp::dual::PricingPolicy::tableau_norm;
    auto tn_res = lp::dual::solve(hot_model, tn_opt, parsed);
    req(tn_res.solution.status == lp::reference::SolveStatus::optimal, "tableau norm optimal");
    req(verify::verify_reference_result(hot_model, tn_res.solution).accepted, "tableau norm verified");
    auto duplicate = parsed;
    duplicate.basic_variables[1] = duplicate.basic_variables[0];
    lp::dual::Options no_fallback;
    no_fallback.allow_cold_fallback = false;
    auto bad = lp::dual::solve(hot_model, no_fallback, duplicate);
    req(bad.solution.status == lp::reference::SolveStatus::numerical_failure,
        "duplicate basis rejected");
    auto fallback = lp::dual::solve(hot_model, {}, duplicate);
    req(fallback.used_cold_fallback &&
            fallback.solution.status == lp::reference::SolveStatus::optimal,
        "invalid warm fallback");
    auto damaged = text;
    damaged.back() = 'x';
    bool threw = false;
    try {
        (void)lp::dual::parse_basis(damaged);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    req(threw, "basis checksum rejection");
    auto changed = hot_model;
    changed.objective[0] = 2;
    changed.validate();
    bad = lp::dual::solve(changed, no_fallback, parsed);
    req(bad.solution.status == lp::reference::SolveStatus::numerical_failure,
        "stale fingerprint rejected");
    lp::dual::Options limited;
    limited.iteration_limit = 1;
    limited.allow_cold_fallback = false;
    auto lim = lp::dual::solve(infeasible, limited, parsed);
    req(lim.solution.status == lp::reference::SolveStatus::iteration_limit, "dual iteration limit");
    transform::CanonicalModel zero;
    zero.matrix = {0, 1, {}};
    zero.objective = {1};
    zero.record.objective_sign = 1;
    zero.record.structural_variables = 1;
    zero.record.variables.resize(1);
    zero.validate();
    auto zero_cold = lp::dual::solve(zero);
    auto zero_warm = lp::dual::solve(zero, {}, zero_cold.basis_state);
    req(zero_cold.solution.status == lp::reference::SolveStatus::optimal &&
            zero_warm.solution.status == lp::reference::SolveStatus::optimal,
        "zero-row warm parity");
    req(verify::verify_reference_result(zero, zero_warm.solution).accepted,
        "zero-row warm verified");
    lp::dual::Options invalid_options;
    invalid_options.feasibility_tolerance = std::numeric_limits<double>::infinity();
    auto invalid_option_result = lp::dual::solve(hot_model, invalid_options, parsed);
    req(!invalid_option_result.used_cold_fallback &&
            invalid_option_result.message == "invalid dual simplex options",
        "invalid options attributed correctly");
    auto offset_base = base;
    offset_base.objective_offset = 7;
    offset_base.validate();
    auto offset_cold = lp::dual::solve(offset_base);
    auto offset_hot = offset_base;
    offset_hot.rhs = {-3, 10};
    auto offset_result = lp::dual::solve(offset_hot, {}, offset_cold.basis_state);
    req(!offset_result.telemetry.empty() &&
            std::abs(offset_result.telemetry.front().objective - 7) < 1e-12,
        "telemetry objective includes offset");
    auto duplicate_artifact = parsed;
    duplicate_artifact.basic_variables[1] = duplicate_artifact.basic_variables[0];
    threw = false;
    try {
        (void)lp::dual::serialize_basis(duplicate_artifact);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    req(threw, "serializer rejects duplicate basis");
    // Backlog item 8: the session caches a factor only for the exact
    // (model fingerprint, basis) it was built for; a constraint-matrix change
    // forces a cold solve and the session re-bases before reusing again.
    auto changed_matrix = [](double lo, double up) {
        auto m = make_model(lo, up);
        m.matrix.values[0] = -2.0;
        m.validate();
        return m;
    };
    {
        auto session = lp::dual::make_session();
        const auto first = session.resolve(make_model(0, 10));
        req(first.verified && !first.factor_reused, "session first resolve verified");
        const auto second = session.resolve(make_model(3, 10));
        req(second.verified && !second.factor_reused, "first warm resolve factorizes");
        const auto third = session.resolve(make_model(6, 14));
        req(third.verified && third.factor_reused, "RHS-only change reuses the cached factor");
        const auto fourth = session.resolve(changed_matrix(3, 10));
        req(fourth.used_cold_fallback, "constraint change drops to a cold solve");
        req(!fourth.factor_reused, "constraint change cannot reuse the cached factor");
        req(fourth.verified, "constraint change result verified");
        const auto fifth = session.resolve(changed_matrix(5, 12));
        req(fifth.verified && !fifth.factor_reused, "session re-bases on the new matrix");
        const auto sixth = session.resolve(changed_matrix(6, 14));
        req(sixth.verified && sixth.factor_reused, "reuse resumes on the new matrix");
        const auto reference = lp::reference::solve(changed_matrix(6, 14));
        req(reference.status == sixth.solution.status &&
                std::abs(reference.objective - sixth.solution.objective) < 1e-9,
            "session parity with reference after invalidation");
        req(session.cold_fallback_count() == 2, "session counts bootstrap and matrix fallback");
        const auto impossible = session.resolve(make_model(11, 10));
        req(impossible.solution.status == lp::reference::SolveStatus::infeasible &&
                impossible.verified, "session infeasibility is certified");
        req(!session.basis().has_value() && !session.cache().valid,
            "infeasible resolve drops the session basis and factor");
        const auto restarted = session.resolve(make_model(3, 10));
        req(restarted.verified && restarted.used_cold_fallback && !restarted.factor_reused,
            "session restarts cold after infeasibility");
    }
    std::cout << "dual simplex tests passed\n";
}
