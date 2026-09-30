// QP-01 contract §6 (docs/contracts/convex-qp.md): the independent QP
// verifier must reject every altered-witness attack on a solver-produced
// solution; edge cases must resolve to the §4 statuses with accepted
// witnesses; the repeated symbolic cache must survive numeric-only data
// changes; and api::solve must disclose the actual CPU/GPU path and publish
// certificate_type only for accepted witnesses.
#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "qp_kkt_attack_models.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace markov_cero;

void req(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

qp::QpOptions tight_options() {
    qp::QpOptions opts;
    opts.absolute_tolerance = 1e-6;
    opts.relative_tolerance = 1e-6;
    opts.max_iterations = 4000;
    return opts;
}

// Contract §6.1: every rejected-witness path, attacked on a solver-produced
// solution of a real model.
void altered_witness_attacks() {
    const auto equality = qp::make_quadratic_model(io::parse_mps_string(kEqualityMps));
    const auto solved = qp::solve_qp(equality, tight_options());
    req(solved.status == qp::QpStatus::optimal, "baseline equality QP optimal");
    req(qp::verify_qp_solution(equality, solved).passed, "baseline witness accepted");

    auto flipped = solved;
    for (double& y : flipped.y) y = -y;
    req(!qp::verify_qp_solution(equality, flipped).passed,
        "sign-flipped multipliers accepted");

    auto scaled = solved;
    for (double& y : scaled.y) y *= 2.0;
    req(!qp::verify_qp_solution(equality, scaled).passed,
        "scaled multipliers accepted");

    auto missing = solved;
    missing.y.clear();
    req(!qp::verify_qp_solution(equality, missing).passed,
        "missing multipliers accepted");

    auto perturbed = solved;
    perturbed.x[0] += 0.1;
    req(!qp::verify_qp_solution(equality, perturbed).passed,
        "perturbed primal accepted");

    auto restated = solved;
    restated.objective_value += 1.0;
    req(!qp::verify_qp_solution(equality, restated).passed,
        "altered objective accepted");

    auto nonfinite = solved;
    nonfinite.y[0] = std::numeric_limits<double>::quiet_NaN();
    req(!qp::verify_qp_solution(equality, nonfinite).passed,
        "non-finite multiplier accepted");

    // Wrong active side and complementarity breach on a bound-active solve.
    const auto box = qp::make_quadratic_model(io::parse_mps_string(kBoxMps));
    const auto boxed = qp::solve_qp(box, tight_options());
    req(boxed.status == qp::QpStatus::optimal, "box QP optimal");
    req(qp::verify_qp_solution(box, boxed).passed, "box witness accepted");
    auto wrong_side = boxed;
    wrong_side.x[0] = 0.0;
    wrong_side.objective_value = 0.0;
    req(!qp::verify_qp_solution(box, wrong_side).passed,
        "wrong active bound side accepted");
    auto nonzero_comp = boxed;
    nonzero_comp.x[0] = 0.5;
    nonzero_comp.objective_value = -0.5;
    req(!qp::verify_qp_solution(box, nonzero_comp).passed,
        "nonzero complementarity accepted");

    // Non-PSD curvature is rejected before any witness arithmetic.
    const auto nonconvex = qp::make_quadratic_model(io::parse_mps_string(kNonConvexMps));
    qp::QpSolution trivial;
    trivial.status = qp::QpStatus::optimal;
    trivial.x.assign(nonconvex.num_variables(), 0.0);
    trivial.y.assign(nonconvex.num_constraints(), 0.0);
    trivial.objective_value = 0.0;
    const auto rejected = qp::verify_qp_solution(nonconvex, trivial);
    req(!rejected.passed, "non-PSD witness accepted");
    req(rejected.failure_reason.find("positive semidefinite") != std::string::npos,
        "non-PSD rejection names the curvature check");
}

// Contract §6.2: edge cases resolve to the §4 statuses with accepted
// witnesses — solver level first, then the api::boundary mapping.
void edge_case_statuses() {
    const auto zero_hess = qp::make_quadratic_model(io::parse_mps_string(kBoxMps));
    const auto zh = qp::solve_qp(zero_hess, tight_options());
    req(zh.status == qp::QpStatus::optimal, "zero Hessian QP optimal");
    req(zh.verified, "zero Hessian witness verified by the solver");
    req(std::abs(zh.objective_value + 1.0) < 1e-3, "zero Hessian objective near -1");

    const auto unbounded = qp::make_quadratic_model(io::parse_mps_string(kUnboundedMps));
    const auto ub = qp::solve_qp(unbounded, tight_options());
    req(ub.status == qp::QpStatus::dual_infeasible, "unbounded box reports dual infeasibility");
    req(qp::verify_qp_unbounded(unbounded, ub), "recession ray accepted");

    const auto singular = qp::make_quadratic_model(io::parse_mps_string(kSingularPsdMps));
    req(qp::check_convexity(singular.P), "singular PSD Hessian certified");
    const auto sp = qp::solve_qp(singular, tight_options());
    req(sp.status == qp::QpStatus::optimal, "singular PSD QP optimal on its flat face");
    req(qp::verify_qp_solution(singular, sp).passed, "singular PSD witness accepted");

    const auto empty_row = qp::make_quadratic_model(io::parse_mps_string(kEmptyRowMps));
    const auto er = qp::solve_qp(empty_row, tight_options());
    req(er.status == qp::QpStatus::optimal, "empty-row QP optimal");
    req(qp::verify_qp_solution(empty_row, er).passed, "empty-row witness accepted");

    const auto infeasible = qp::make_quadratic_model(io::parse_mps_string(kInfeasibleMps));
    const auto infeas = qp::solve_qp(infeasible, tight_options());
    req(infeas.status == qp::QpStatus::primal_infeasible, "conflicting bounds infeasible");
    req(qp::verify_qp_infeasibility(infeasible, infeas), "Farkas witness accepted");

    // The api boundary applies §4: accepted witnesses publish the one QP
    // certificate name, get the witness assurance and stay verified; a GPU
    // request below the activation threshold discloses cpu_fallback while
    // producing the identical CPU result (§5).
    api::SolveOptions options;
    options.engine = "qp";
    const auto optimal = api::solve_model(io::parse_mps_string(kEqualityMps), options);
    req(optimal.status == lp::reference::SolveStatus::optimal, "api optimal");
    req(optimal.certificate_type == "convex_qp_kkt", "accepted KKT witness publishes certificate");
    req(optimal.assurance == "optimality_witness_checked", "accepted witness assurance");
    req(optimal.verified && optimal.canonical_verified, "api optimal verified");
    req(optimal.backend_actually_used == "cpu", "cpu request reports cpu");

    api::SolveOptions gpu_options = options;
    gpu_options.backend = "gpu";
    const auto gpu = api::solve_model(io::parse_mps_string(kEqualityMps), gpu_options);
    req(gpu.backend_actually_used == "cpu_fallback",
        "small-QP GPU request discloses cpu_fallback");
    req(gpu.recommended_backend == "gpu", "explicit engine keeps the requested backend");
    req(gpu.status == optimal.status &&
            std::abs(gpu.original_objective - optimal.original_objective) < 1e-9,
        "GPU-request fallback matches the CPU result");

    const auto unb = api::solve_model(io::parse_mps_string(kUnboundedMps), options);
    req(unb.status == lp::reference::SolveStatus::unbounded, "api unbounded");
    req(unb.certificate_type == "convex_qp_kkt", "accepted recession witness publishes certificate");
    req(unb.assurance == "optimality_witness_checked" && unb.verified,
        "accepted recession witness is verified with witness assurance");

    const auto inf = api::solve_model(io::parse_mps_string(kInfeasibleMps), options);
    req(inf.status == lp::reference::SolveStatus::infeasible, "api infeasible");
    req(inf.certificate_type == "convex_qp_kkt", "accepted Farkas witness publishes certificate");
    req(inf.assurance == "optimality_witness_checked" && inf.verified,
        "accepted Farkas witness is verified with witness assurance");
}

// Contract §6.3: a numeric-only change (different P values, same pattern)
// reuses the cached symbolic factorization and both results verify.
void symbolic_cache_numeric_change() {
    const auto model_a = qp::make_quadratic_model(io::parse_mps_string(kCacheAMps));
    const auto model_b = qp::make_quadratic_model(io::parse_mps_string(kCacheBMps));
    const auto options = tight_options();
    qp::AdmmQpSolver session;

    const auto a = qp::solve_qp(model_a, options, session, false);
    req(a.status == qp::QpStatus::optimal, "cache model A optimal");
    req(qp::verify_qp_solution(model_a, a).passed, "cache model A verified");
    req(session.symbolic_factorizations() == 1 && session.symbolic_reuses() == 0,
        "first solve runs exactly one symbolic factorization");

    const auto b = qp::solve_qp(model_b, options, session, false);
    req(b.status == qp::QpStatus::optimal, "cache model B optimal");
    req(qp::verify_qp_solution(model_b, b).passed, "cache model B verified");
    req(b.kkt_symbolic_reuse >= 1, "changed P values reuse the symbolic pass");
    req(session.symbolic_factorizations() == 1 && session.symbolic_reuses() >= 1,
        "no second symbolic factorization for a pattern-identical model");
    req(std::abs(a.objective_value - b.objective_value) > 1e-3,
        "the numeric change actually changed the solve");
}
} // namespace

int main() {
    altered_witness_attacks();
    edge_case_statuses();
    symbolic_cache_numeric_change();
    std::cout << "qp_kkt_attack: all cases passed\n";
    return 0;
}
