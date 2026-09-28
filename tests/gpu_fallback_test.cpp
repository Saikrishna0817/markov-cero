// W4 Tier-1: requesting --backend gpu on a machine without CUDA must not
// crash — the solver falls back to CPU PDLP and still solves the model.
#include "markov_cero/api/solve.hpp"

#include <cstdlib>
#include <iostream>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

} // namespace

int main() {
    api::SolveOptions options;
    options.engine = "pdlp";
    options.backend = "gpu";  // explicit GPU request (Tier-1 fallback probe)

    const auto res = api::solve_file("examples/blend.mps", options);
    req((res.status == lp::reference::SolveStatus::optimal ||
         res.status == lp::reference::SolveStatus::feasible),
        "GPU request on CPU-only host still solves via CPU PDLP");
    req(res.original_verified, "fallback primal is independently verified");
    req(res.status != lp::reference::SolveStatus::feasible || !res.verified,
        "an uncertified optimum must not be reported as globally verified");

    std::cout << "gpu fallback tests passed\n";
    return 0;
}
