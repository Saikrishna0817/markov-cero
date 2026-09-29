#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"
#include <cmath>
#include <iostream>
#include <random>
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
    m.validate();
    return m;
}
} // namespace
int main() {
    auto seed = make_model(0, 20);
    auto cold = lp::dual::solve(seed);
    auto basis = cold.basis_state;
    std::mt19937_64 rng(0x4d3450524f50ULL);
    std::uniform_real_distribution<double> d(0, 20);
    for (int k = 0; k < 500; ++k) {
        double a = d(rng), b = d(rng);
        double lo = std::min(a, b), up = std::max(a, b);
        auto m = make_model(lo, up);
        auto warm = lp::dual::solve(m, {}, basis);
        auto reference = lp::reference::solve(m);
        req(warm.solution.status == lp::reference::SolveStatus::optimal, "random warm status");
        req(reference.status == warm.solution.status, "random status parity");
        req(std::abs(reference.objective - warm.solution.objective) < 1e-8,
            "random objective parity");
        req(verify::verify_reference_result(m, warm.solution).accepted, "random warm verification");
        lp::dual::Options strict;
        strict.harris_ratio = false;
        auto second = lp::dual::solve(m, strict, basis);
        req(second.solution.status == warm.solution.status &&
                std::abs(second.solution.objective - warm.solution.objective) < 1e-8,
            "Harris strict parity");
    }
    for (int k = 0; k < 100; ++k) {
        double up = d(rng);
        auto m = make_model(up + 1 + d(rng), up);
        auto warm = lp::dual::solve(m, {}, basis);
        req(warm.solution.status == lp::reference::SolveStatus::infeasible,
            "random infeasible status");
        req(verify::verify_reference_result(m, warm.solution).accepted,
            "random Farkas verification");
    }
    // Backlog item 8: repeated-solve session over an RHS/bound-only sequence.
    // Every repeat must keep status/objective parity with the legacy one-shot
    // warm path AND be re-verified: a reuse that skipped verification fails.
    {
        auto session = lp::dual::make_session();
        const auto bootstrap = session.resolve(seed);
        req(bootstrap.verified, "session bootstrap verified");
        req(!bootstrap.factor_reused, "session bootstrap is cold");
        std::size_t reuses = 0;
        for (int k = 0; k < 300; ++k) {
            double a = d(rng), b = d(rng);
            double lo = std::min(a, b), up = std::max(a, b);
            auto m = make_model(lo, up);
            const auto prior = session.basis();
            const auto cold_repeat = lp::dual::solve(m);
            const auto legacy = lp::dual::solve(m, {}, prior);
            const auto repeat = session.resolve(m);
            req(repeat.verified, "session repeat verified");
            req(repeat.solution.status == lp::reference::SolveStatus::optimal,
                "session repeat status");
            req(repeat.solution.status == legacy.solution.status, "session legacy status parity");
            req(cold_repeat.verified && repeat.solution.status == cold_repeat.solution.status,
                "session cold status and verification parity");
            req(std::abs(repeat.solution.objective - legacy.solution.objective) < 1e-8,
                "session legacy objective parity");
            req(std::abs(repeat.solution.objective - cold_repeat.solution.objective) < 1e-8,
                "session cold objective parity");
            req(verify::verify_reference_result(m, repeat.solution).accepted,
                "session repeat independent verification");
            req(repeat.factor_reused == (k >= 1), "session factor reuse cadence");
            reuses += repeat.factor_reused ? 1 : 0;
        }
        req(reuses == 299, "session reused the cached factorization");
        req(session.factor_reuse_count() == reuses, "session reuse counter");
        req(session.verified_count() == session.resolve_count(),
            "session verified every accepted result");
        req(session.basis().has_value(), "session keeps an accepted basis");
        session.reset();
        req(!session.basis().has_value(), "session reset drops the basis");
        req(!session.cache().valid, "session reset drops the cached factor");
    }
    std::cout << "randomized warm-start tests passed\n";
}
