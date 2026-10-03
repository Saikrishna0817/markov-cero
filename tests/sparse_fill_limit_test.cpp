// RES-01 (docs/contracts/resource-limits.md section 4): factor fill is bounded
// by its fill cap — an over-full factorization must throw std::length_error
// before any partial factor can be returned, so the API boundary maps it to a
// work_limit resource stop and never to a numerical result. The same fill is
// also charged live-outstanding to memory_limit_bytes; a refused charge stops
// with memory_budget_exhausted (IR-21, readiness_edge_cases_test.cpp).
#include "markov_cero/linalg/sparse_basis.hpp"

#include <stdexcept>
#include <string>
#include <vector>

using markov_cero::linalg::SparseCsc;
using markov_cero::linalg::SparseLu;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    const std::vector<std::vector<double>> cols{{4, 1, 0}, {1, 5, 1}, {0, 2, 6}};
    const SparseCsc csc = SparseCsc::from_columns(3, cols);

    bool refused = false;
    std::string what;
    try {
        const SparseLu over_full = SparseLu::factorize(csc, 1e-14, /*cap=*/1);
        (void)over_full;
    } catch (const std::length_error& error) {
        refused = true;
        what = error.what();
    }
    require(refused, "a factor fill cap of one entry refuses the factorization");
    require(what.find("fill limit") != std::string::npos,
            "the refusal names the fill limit");

    const SparseLu control = SparseLu::factorize(csc);
    require(control.dimension() == 3, "the uncapped control factorization succeeds");
    return 0;
}
