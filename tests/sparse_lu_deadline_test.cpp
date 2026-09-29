#include "markov_cero/linalg/sparse_basis.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using markov_cero::linalg::SparseBasisFactorization;
using markov_cero::linalg::SparseBasisOptions;
using markov_cero::linalg::SparseCsc;
using markov_cero::linalg::SparseLu;

static void req(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

int main() {
    const std::size_t n = 80;
    std::vector<std::vector<double>> columns(n, std::vector<double>(n, 0.0));
    for (std::size_t j = 0; j < n; ++j) {
        columns[j][j] = 2.0;
        if (j + 1 < n) columns[j][j + 1] = -1.0;
        if (j > 0) columns[j][j - 1] = -1.0;
    }
    const auto csc = SparseCsc::from_columns(n, columns);
    const auto past = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    bool threw = false;
    try {
        (void)SparseLu::factorize(csc, 1e-14, 4U * 1024U * 1024U, false, past);
    } catch (const std::runtime_error& e) {
        threw = std::string(e.what()).find("deadline reached") != std::string::npos;
    }
    req(threw, "expired deadline stops sparse LU");
    SparseBasisOptions opts;
    opts.deadline = past;
    bool basis_threw = false;
    try {
        (void)SparseBasisFactorization::factorize(csc, opts);
    } catch (const std::runtime_error& e) {
        basis_threw =
            std::string(e.what()).find("deadline reached") != std::string::npos;
    }
    req(basis_threw, "expired deadline stops SparseBasisFactorization");
    std::cout << "sparse_lu_deadline tests passed\n";
    return 0;
}
