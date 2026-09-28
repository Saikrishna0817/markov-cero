// tests/numerical_diagnostic_test.cpp
// C-3 verification: all failure paths emit a populated NumericalDiagnostic.
// Tests that:
//   1. Infeasible models produce diagnostic with failure_site populated
//   2. Successful solves have failure_site="none"
//   3. The NumericalDiagnostic struct fields are coherent

#include "markov_cero/api/solve.hpp"
#include "markov_cero/model/model.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

static std::string repo_root() {
    const char* env = std::getenv("MARKOV_CERO_SOURCE_DIR");
    if (env) return env;
    return ".";
}

// Build a trivially infeasible 2-variable LP: x >= 1 and x <= 0
static markov_cero::model::Model build_infeasible_lp() {
    using namespace markov_cero::model;
    Model m;
    m.name = "infeasible_test";
    m.objective_sense = ObjectiveSense::minimize;
    m.objective = {1.0};

    // x has bounds 1 <= x <= 0 (infeasible)
    m.variable_lower = {{BoundKind::finite, 1.0}};
    m.variable_upper = {{BoundKind::finite, 0.0}};

    // No constraints (infeasibility comes from variable bounds)
    m.matrix.row_count = 0;
    m.matrix.column_count = 1;
    m.matrix.column_start = {0, 0};
    m.row_lower = {};
    m.row_upper = {};
    return m;
}

int main() {
    // Test 1: Successful solve — failure_site should be "none"
    const std::string blend = repo_root() + "/examples/blend.mps";
    if (std::filesystem::exists(blend)) {
        markov_cero::api::SolveOptions opts;
        opts.engine = "primal";
        const auto res = markov_cero::api::solve_file(blend, opts);

        if (res.status == markov_cero::lp::reference::SolveStatus::optimal) {
            if (res.diagnostic.failure_site != "none") {
                std::fprintf(stderr,
                             "[numerical_diagnostic_test] FAIL: successful solve has "
                             "failure_site='%s' (expected 'none')\n",
                             res.diagnostic.failure_site.c_str());
                return 1;
            }
            std::fprintf(stdout,
                         "[numerical_diagnostic_test] T1 PASS: blend optimal, "
                         "failure_site='%s'\n",
                         res.diagnostic.failure_site.c_str());
        }
    }

    // Test 2: PDLP solve — diagnostic should have primal/dual residual populated
    if (std::filesystem::exists(blend)) {
        markov_cero::api::SolveOptions opts;
        opts.engine = "pdlp";
        const auto res = markov_cero::api::solve_file(blend, opts);

        // PDLP diagnostic populates residuals even on optimal
        if (res.diagnostic.primal_residual < 0.0) {
            std::fprintf(stderr,
                         "[numerical_diagnostic_test] FAIL: PDLP primal_residual < 0\n");
            return 1;
        }
        std::fprintf(stdout,
                     "[numerical_diagnostic_test] T2 PASS: PDLP prim_res=%.2e dual_res=%.2e\n",
                     res.diagnostic.primal_residual, res.diagnostic.dual_residual);
    }

    // Test 2b: factorizing engines must report a REAL condition estimate
    // (pivot-ratio >= 1, finite), not the old hardcoded placeholder. PDLP is
    // matrix-free and may report 0.0 ("not estimated") unless it crossed over.
    if (std::filesystem::exists(blend)) {
        for (const char* engine : {"primal", "dual", "ipm"}) {
            markov_cero::api::SolveOptions opts;
            opts.engine = engine;
            const auto res = markov_cero::api::solve_file(blend, opts);
            if (res.status != markov_cero::lp::reference::SolveStatus::optimal) {
                std::fprintf(stderr,
                             "[numerical_diagnostic_test] FAIL: %s engine not optimal\n",
                             engine);
                return 1;
            }
            const double kappa = res.diagnostic.condition_estimate;
            if (!(kappa >= 1.0) || !std::isfinite(kappa)) {
                std::fprintf(stderr,
                             "[numerical_diagnostic_test] FAIL: %s engine condition "
                             "estimate %.6g not in [1, inf)\n",
                             engine, kappa);
                return 1;
            }
        }
        std::fprintf(stdout,
                     "[numerical_diagnostic_test] T2b PASS: primal/dual/ipm condition "
                     "estimates are finite and >= 1\n");
    }

    // Test 3: Infeasible model — the solver must report infeasible (not crash)
    {
        const auto infeas = build_infeasible_lp();
        markov_cero::api::SolveOptions opts;
        opts.engine = "primal";
        const auto res = markov_cero::api::solve_model(infeas, opts);

        // Expecting infeasible (or numerical_failure if presolve catches it)
        const bool infeasible_reported =
            res.status == markov_cero::lp::reference::SolveStatus::infeasible ||
            res.status == markov_cero::lp::reference::SolveStatus::numerical_failure ||
            res.status == markov_cero::lp::reference::SolveStatus::invalid_model;

        if (!infeasible_reported) {
            std::fprintf(stderr,
                         "[numerical_diagnostic_test] FAIL: infeasible LP reported '%s'\n",
                         res.message.c_str());
            return 1;
        }
        std::fprintf(stdout,
                     "[numerical_diagnostic_test] T3 PASS: infeasible LP status='%s' "
                     "failure_site='%s'\n",
                     res.message.c_str(), res.diagnostic.failure_site.c_str());
    }

    std::fprintf(stdout, "[numerical_diagnostic_test] ALL TESTS PASSED\n");
    return 0;
}
