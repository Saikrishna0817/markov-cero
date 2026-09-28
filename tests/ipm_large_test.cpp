// tests/ipm_large_test.cpp
// W5/D-14 verification: IPM with sparse normal equations on sc205 (205 rows, 317 variables)
// Gate: solution verified or graceful iteration-limit, no crash.

#include "markov_cero/api/solve.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>

static std::string repo_root() {
    const char* env = std::getenv("MARKOV_CERO_SOURCE_DIR");
    if (env && std::filesystem::exists(std::string(env) + "/data/netlib/sc205.mps"))
        return env;
    for (const char* rel : {".", "..", "../.."}) {
        std::string p = std::string(rel) + "/data/netlib/sc205.mps";
        if (std::filesystem::exists(p)) return rel;
    }
    return ".";
}

int main() {
    const std::string instance = repo_root() + "/data/netlib/sc205.mps";
    if (!std::filesystem::exists(instance)) {
        std::fprintf(stderr, "[ipm_large_test] SKIP: sc205.mps not found at %s\n", instance.c_str());
        return 0;  // skip, not failure
    }

    markov_cero::api::SolveOptions opts;
    opts.engine = "ipm";
    opts.enable_presolve = false;
    opts.enable_scale = true;

    const auto res = markov_cero::api::solve_file(instance, opts);

    const bool ok_status =
        res.status == markov_cero::lp::reference::SolveStatus::optimal ||
        res.status == markov_cero::lp::reference::SolveStatus::iteration_limit;

    if (!ok_status) {
        std::fprintf(stderr, "[ipm_large_test] FAIL: unexpected status on sc205: %s\n",
                     res.message.c_str());
        return 1;
    }

    if (res.status == markov_cero::lp::reference::SolveStatus::optimal) {
        if (!res.original_verified) {
            std::fprintf(stderr, "[ipm_large_test] FAIL: solution not verified. %s\n",
                         res.original_message.c_str());
            return 1;
        }
        std::fprintf(stdout, "[ipm_large_test] PASS: sc205 optimal obj=%.10g verified=%d "
                     "rows=%zu cols=%zu\n",
                     res.original_objective, (int)res.original_verified,
                     res.model_rows, res.model_cols);
    } else {
        std::fprintf(stdout, "[ipm_large_test] PARTIAL: sc205 hit iteration limit "
                     "(sparse normal equations did not crash, 200+ rows handled)\n");
    }
    return 0;
}
