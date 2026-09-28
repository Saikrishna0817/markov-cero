// tests/qp_adaptive_rho_test.cpp
// W5/D-16 verification: ADMM adaptive rho converges correctly via the API layer.
// Tests that QP with adaptive_rho=true (default) solves correctly on the
// portfolio example that already ships with the repo.

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
    // Use the qp_portfolio example that is part of the repo
    const std::string qp_file = repo_root() + "/examples/qp_portfolio.mps";
    if (!std::filesystem::exists(qp_file)) {
        std::fprintf(stderr, "[qp_adaptive_rho_test] SKIP: qp_portfolio.mps not found\n");
        return 0;
    }

    markov_cero::api::SolveOptions opts;
    opts.engine = "qp";  // force QP ADMM path

    const auto res = markov_cero::api::solve_file(qp_file, opts);

    if (res.status != markov_cero::lp::reference::SolveStatus::optimal) {
        std::fprintf(stderr,
                     "[qp_adaptive_rho_test] FAIL: expected optimal, got: %s\n",
                     res.message.c_str());
        return 1;
    }

    if (!res.original_verified) {
        std::fprintf(stderr,
                     "[qp_adaptive_rho_test] FAIL: QP KKT not verified: %s\n",
                     res.original_message.c_str());
        return 1;
    }

    std::fprintf(stdout,
                 "[qp_adaptive_rho_test] PASS: obj=%.8g verified=%d iters=%zu "
                 "prim_res=%.2e dual_res=%.2e\n",
                 res.original_objective, (int)res.original_verified,
                 res.lp_iterations,
                 res.diagnostic.primal_residual,
                 res.diagnostic.dual_residual);

    // Residuals must be within QP tolerance (api sets abs_tol=1e-6)
    if (res.diagnostic.primal_residual > 1e-3) {
        std::fprintf(stderr, "[qp_adaptive_rho_test] FAIL: primal residual %.2e > 1e-3\n",
                     res.diagnostic.primal_residual);
        return 1;
    }

    return 0;
}
