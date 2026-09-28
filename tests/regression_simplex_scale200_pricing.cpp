// Regression: guards against pathological primal simplex slowdown past ~200 rows
// caused by allocating a full dense column vector for every nonbasic on every
// pricing pass (Phase 2 remediation). scale_200 is ~126 rows / 224 cols.
#include "markov_cero/io/mps.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const std::string path = "tests/fixtures/scale_200.mps";
    std::ifstream in(path);
    if (!in) {
        std::cerr << "FAIL: cannot open " << path << "\n";
        return 1;
    }
    markov_cero::model::Model model;
    try {
        model = markov_cero::io::parse_mps(in);
    } catch (const std::exception& e) {
        std::cerr << "FAIL: parse " << path << ": " << e.what() << "\n";
        return 1;
    }
    auto canon = markov_cero::transform::canonicalize(model);
    const auto t0 = std::chrono::steady_clock::now();
    auto result = markov_cero::lp::reference::solve(canon);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    if (result.status != markov_cero::lp::reference::SolveStatus::optimal) {
        std::cerr << "FAIL: expected optimal, got "
                  << markov_cero::lp::reference::to_string(result.status)
                  << " msg=" << result.message << "\n";
        return 1;
    }
    const auto certificate = markov_cero::verify::verify_reference_result(canon, result);
    if (!certificate.accepted) {
        std::cerr << "FAIL: optimum certificate rejected: " << certificate.message << "\n";
        return 1;
    }
    // Guard: previously multi-second on this size due to pricing allocations.
    constexpr double wall_limit_ms = 15000.0;
    if (ms > wall_limit_ms) {
        std::cerr << "FAIL: scale_200 primal wall " << ms << " ms exceeds " << wall_limit_ms
                  << " ms (pricing regression)\n";
        return 1;
    }
    std::cout << "PASS: scale_200 optimal in " << ms << " ms iterations="
              << (result.phase_one_iterations + result.phase_two_iterations) << "\n";
    return 0;
}
