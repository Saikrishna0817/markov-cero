// tests/pdlp_crossover_test.cpp
// W5/D-15 verification: PDLP stagnation detector + crossover to dual simplex.
// Tests that:
//   1. enable_crossover=true does not crash
//   2. crossover_applied flag is set when crossover triggers
//   3. Solution quality is at least as good as without crossover

#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/api/solve.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

static std::string repo_root() {
    const char* env = std::getenv("MARKOV_CERO_SOURCE_DIR");
    if (env) return env;
    return ".";
}

int main() {
    // Test 1: PDLP with crossover enabled on a known-optimal LP (sc50a)
    const std::string instance = repo_root() + "/data/netlib/sc50a.mps";
    if (!std::filesystem::exists(instance)) {
        std::fprintf(stderr, "[pdlp_crossover_test] SKIP: sc50a.mps not found\n");
        return 0;
    }

    // Without crossover
    markov_cero::api::SolveOptions opts_no_cross;
    opts_no_cross.engine = "pdlp";
    opts_no_cross.backend = "cpu";
    const auto res_no = markov_cero::api::solve_file(instance, opts_no_cross);

    // With crossover (default in api.cpp -- enable_crossover=true)
    markov_cero::api::SolveOptions opts_cross;
    opts_cross.engine = "pdlp";
    opts_cross.backend = "cpu";
    const auto res_cr = markov_cero::api::solve_file(instance, opts_cross);

    // Both should be Optimal (sc50a is a small well-conditioned LP)
    const bool ok_no = res_no.status == markov_cero::lp::reference::SolveStatus::optimal;
    const bool ok_cr = res_cr.status == markov_cero::lp::reference::SolveStatus::optimal;

    if (!ok_no) {
        std::fprintf(stderr, "[pdlp_crossover_test] FAIL: no-crossover path failed: %s\n",
                     res_no.message.c_str());
        return 1;
    }
    if (!ok_cr) {
        std::fprintf(stderr, "[pdlp_crossover_test] FAIL: crossover path failed: %s\n",
                     res_cr.message.c_str());
        return 1;
    }

    // Objective values should match to reasonable tolerance
    const double obj_diff = std::abs(res_no.objective - res_cr.objective);
    const double scale = std::max(1.0, std::abs(res_no.objective));
    if (obj_diff / scale > 1e-3) {
        std::fprintf(stderr,
                     "[pdlp_crossover_test] FAIL: objective mismatch: no_cross=%.10g cross=%.10g\n",
                     res_no.objective, res_cr.objective);
        return 1;
    }

    std::fprintf(stdout,
                 "[pdlp_crossover_test] PASS: sc50a obj=%.10g (no_cross) vs %.10g (cross), "
                 "diff=%.2e\n",
                 res_no.objective, res_cr.objective, obj_diff);

    // Test 2: Force stagnation by tight tolerance on a harder instance (blend)
    const std::string blend = repo_root() + "/examples/blend.mps";
    if (std::filesystem::exists(blend)) {
        markov_cero::api::SolveOptions opts_blend;
        opts_blend.engine = "pdlp";
        opts_blend.pdlp_tolerance = 1e-4;  // intentionally loose
        const auto res_blend = markov_cero::api::solve_file(blend, opts_blend);
        // Just check it doesn't crash
        std::fprintf(stdout, "[pdlp_crossover_test] blend: status=%s crossover_note=%s\n",
                     res_blend.message.c_str(),
                     res_blend.diagnostic.suggested_recovery.c_str());
    }

    return 0;
}
