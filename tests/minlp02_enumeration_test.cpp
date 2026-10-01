// MINLP-02 contract §7 (docs/contracts/minlp-proof-replay.md): seeded random
// enumeration of tiny convex quadratic MINLPs with two binary variables. The
// oracle fixes each integer pair and solves the resulting 1-D convex QP
// exactly by clamping the stationary point; the API solve with proofs
// enabled must match it within §10 O1 and replay an accepted proof. A
// crafted infeasible member must return Infeasible with an accepted proof.

#include "minlp02_common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace markov_cero;
using minlp02::require;

namespace {
struct Lcg {
    std::uint64_t state;
    explicit Lcg(std::uint64_t seed) : state(seed) {}
    double unit() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>((state >> 11) & ((1ULL << 42) - 1)) /
               static_cast<double>(1ULL << 42);
    }
};

// PSD quadratic objective a*x^2 (a > 0), linear objective b*x + c0*y0 + c1*y1,
// one linking row x + y0 + y1 >= t, x in [0, 4], y0/y1 binary.
model::Model build_instance(double a, double b, double c0, double c1, double t) {
    model::Model source;
    source.name = "minlp02_enum";
    model::SparseMatrixBuilder rows(1, 3);
    rows.add(0, 0, 1.0);
    rows.add(0, 1, 1.0);
    rows.add(0, 2, 1.0);
    source.matrix = rows.build();
    source.objective = {b, c0, c1};
    source.row_name = {"link"};
    source.row_lower = {model::Bound::finite(t)};
    source.row_upper = {model::Bound::positive_infinity()};
    source.variable_name = {"x", "y0", "y1"};
    source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0),
                             model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(4.0), model::Bound::finite(1.0),
                             model::Bound::finite(1.0)};
    source.variable_type = {model::VariableType::continuous, model::VariableType::integer,
                            model::VariableType::integer};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{a, 0, 0, true}};
    source.validate();
    return source;
}

// Exact oracle: fix (y0, y1), the row gives x >= max(0, t - y0 - y1); the
// convex 1-D QP a*x^2 + b*x has stationary point -b/(2a), clamped to the
// feasible interval.
double enumerate_optimum(double a, double b, double c0, double c1, double t) {
    double best = std::numeric_limits<double>::infinity();
    for (int y0 = 0; y0 <= 1; ++y0) {
        for (int y1 = 0; y1 <= 1; ++y1) {
            const double lo = std::max(0.0, t - static_cast<double>(y0) - y1);
            if (lo > 4.0) continue;
            const double x = std::clamp(-b / (2.0 * a), lo, 4.0);
            best = std::min(best, a * x * x + b * x + c0 * y0 + c1 * y1);
        }
    }
    return best;
}

bool roundtrip_accepted(const model::Model& source, const verify::OaProof& proof) {
    std::istringstream in(minlp02::serialize(proof));
    const auto report = verify::verify_oa_proof(source, verify::read_oa_proof(in));
    return report.accepted && report.tier == verify::OaAssuranceTier::independent_oa;
}
} // namespace

int main() {
    Lcg rng(20261001);
    for (int instance = 0; instance < 6; ++instance) {
        const double a = 0.5 + 1.5 * rng.unit();
        const double b = -3.0 + 4.0 * rng.unit();
        const double c0 = 0.1 + 1.4 * rng.unit();
        const double c1 = 0.1 + 1.4 * rng.unit();
        const double t = 1.5 + 3.0 * rng.unit();
        const auto source = build_instance(a, b, c0, c1, t);
        const auto res = api::solve_model(source, {});
        require(res.status == lp::reference::SolveStatus::optimal,
                "enumeration member reaches Optimal");
        require(res.assurance == "oa_replayed" && res.guarantee_tier == "independent_oa",
                "enumeration member replays independently");
        require(res.verified && res.canonical_verified, "enumeration member canonical");
        require(res.oa_proof != nullptr, "enumeration member proof attached");
        require(roundtrip_accepted(source, *res.oa_proof),
                "enumeration member proof round-trips");
        const double oracle = enumerate_optimum(a, b, c0, c1, t);
        require(std::isfinite(oracle), "oracle is finite");
        const double tolerance = 1e-6 * std::max(1.0, std::abs(oracle));
        require(std::abs(res.objective - oracle) <= tolerance,
                "enumeration objective matches the exhaustive oracle");
    }

    // Crafted infeasible member: t beyond x + y0 + y1 <= 6.
    const auto infeasible_source = build_instance(1.0, -1.0, 0.4, 0.9, 7.0);
    const auto infeasible = api::solve_model(infeasible_source, {});
    require(infeasible.status == lp::reference::SolveStatus::infeasible,
            "infeasible member returns Infeasible");
    require(infeasible.verified && infeasible.canonical_verified,
            "infeasible member verified");
    require(infeasible.oa_proof != nullptr &&
                infeasible.oa_proof->kind == verify::OaProofKind::infeasible &&
                infeasible.oa_proof->master_proof.claims_infeasible &&
                infeasible.oa_proof->incumbent.empty(),
            "infeasible member carries an accepted infeasible proof");
    require(roundtrip_accepted(infeasible_source, *infeasible.oa_proof),
            "infeasible member proof round-trips");

    std::cout << "MINLP-02 seeded enumeration and infeasible member passed\n";
}
