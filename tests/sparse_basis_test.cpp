#include "markov_cero/linalg/dense_lu.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
using namespace markov_cero::linalg;
namespace {
void req(bool q, const char* m) {
    if (!q)
        throw std::runtime_error(m);
}
DenseMatrix dense_from_columns(const std::vector<std::vector<double>>& c) {
    DenseMatrix a{c.size(), c.size(), std::vector<double>(c.size() * c.size())};
    for (std::size_t j = 0; j < c.size(); ++j)
        for (std::size_t i = 0; i < c.size(); ++i)
            a.values[i * c.size() + j] = c[j][i];
    a.validate();
    return a;
}
double error(const std::vector<double>& a, const std::vector<double>& b) {
    double e = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        e = std::max(e, std::abs(a[i] - b[i]));
    return e;
}
} // namespace
int main() {
    std::vector<std::vector<double>> cols{{4, 1, 0}, {1, 5, 1}, {0, 2, 6}};
    auto csc = SparseCsc::from_columns(3, cols);
    auto dense = dense_from_columns(cols);
    auto sparse = SparseLu::factorize(csc);
    auto dlu = DenseLu::factorize(dense);
    std::vector<double> b{7, -2, 4};
    auto x = sparse.solve(b);
    auto xd = dlu.solve(b);
    req(error(x, xd) < 1e-12, "sparse FTRAN parity");
    req(sparse_infinity_residual(csc, x, b) < 1e-12, "sparse residual");
    auto xt = sparse.solve_transpose(b);
    auto xdt = dlu.solve_transpose(b);
    req(error(xt, xdt) < 1e-12, "sparse BTRAN parity");
    req(sparse_infinity_residual(csc, xt, b, true) < 1e-12, "sparse transpose residual");
    req(sparse.diagnostics().factor_nonzeros >= csc.values.size(), "factor metrics");
    SparseBasisOptions options;
    options.maximum_updates = 3;
    options.eta_density_trigger = 1;
    auto basis = SparseBasisFactorization::factorize(csc, options);
    std::vector<double> replacement{2, 1, 1};
    basis.replace_column(1, replacement);
    cols[1] = replacement;
    auto updated_dense = dense_from_columns(cols);
    auto updated_lu = DenseLu::factorize(updated_dense);
    x = basis.solve(b);
    req(error(x, updated_lu.solve(b)) < 1e-11, "eta FTRAN parity");
    xt = basis.solve_transpose(b);
    req(error(xt, updated_lu.solve_transpose(b)) < 1e-11, "eta BTRAN parity");
    req(basis.statistics().updates == 1 && basis.statistics().current_update_chain == 1,
        "eta statistics");
    basis.replace_column(1, std::vector<double>{1, 3, 1});
    cols[1] = {1, 3, 1};
    basis.replace_column(1, std::vector<double>{1, 4, 2});
    cols[1] = {1, 4, 2};
    req(basis.needs_refactorization(), "update trigger");
    basis.refactorize();
    req(!basis.needs_refactorization() && basis.statistics().current_update_chain == 0 &&
            basis.statistics().refactorizations == 2,
        "refactor reset");
    updated_dense = dense_from_columns(cols);
    updated_lu = DenseLu::factorize(updated_dense);
    req(error(basis.solve(b), updated_lu.solve(b)) < 1e-11, "post-refactor parity");
    bool threw = false;
    try {
        (void)SparseCsc{2, 2, {0, 2, 2}, {1, 0}, {1, 2}}.dense_column(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    req(threw, "unsorted CSC rejected");
    threw = false;
    try {
        (void)SparseCsc::from_columns(2, {{1, 0}, {0, std::numeric_limits<double>::infinity()}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    req(threw, "non-finite sparse value rejected");
    threw = false;
    try {
        (void)SparseLu::factorize(SparseCsc::from_columns(2, {{1, 2}, {2, 4}}));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    req(threw, "singular sparse basis rejected");
    // Degenerate dimensions are legal: canonical models with every constraint
    // absorbed upstream must validate (RW-5 regression guard).
    SparseCsc{0, 2, {0, 0, 0}, {}, {}}.validate();
    SparseCsc{3, 0, {0}, {}, {}}.validate();
    req(sparse_infinity_residual(SparseCsc{0, 2, {0, 0, 0}, {}, {}}, std::vector<double>(2, 0.0), {}) == 0,
        "zero-row residual is zero");
    std::mt19937_64 rng(0x4d35535041525345ULL);
    std::uniform_real_distribution<double> v(-1, 1);
    for (int trial = 0; trial < 250; ++trial) {
        const std::size_t n = 1 + static_cast<std::size_t>(rng() % 18);
        std::vector<std::vector<double>> a(n, std::vector<double>(n));
        for (std::size_t j = 0; j < n; ++j)
            for (std::size_t i = 0; i < n; ++i)
                if ((rng() % 5) == 0)
                    a[j][i] = v(rng);
        for (std::size_t i = 0; i < n; ++i)
            a[i][i] += static_cast<double>(n) + 2;
        auto sm = SparseCsc::from_columns(n, a);
        auto sl = SparseLu::factorize(sm);
        auto dm = dense_from_columns(a);
        auto dl = DenseLu::factorize(dm);
        std::vector<double> rhs(n);
        for (auto& q : rhs)
            q = v(rng);
        req(error(sl.solve(rhs), dl.solve(rhs)) < 1e-9, "random FTRAN parity");
        req(error(sl.solve_transpose(rhs), dl.solve_transpose(rhs)) < 1e-9, "random BTRAN parity");
    }
    // RW-8: iterative refinement (Skeel). Extended-precision residuals plus a
    // correction step drive the residual (backward error) toward machine level;
    // forward error improves by whatever fraction of it came from solver
    // rounding rather than data error.
    const std::size_t n = 18;
    auto measure = [&](SparseBasisFactorization& basis, const SparseCsc& matrix,
                       const std::vector<double>& b_in, const std::vector<double>& t) {
        const auto x = basis.solve(b_in);
        double ferr = 0;
        for (std::size_t i = 0; i < t.size(); ++i)
            ferr = std::max(ferr, std::abs(x[i] - t[i]));
        return std::pair<double, double>{ferr, sparse_infinity_residual(matrix, x, b_in)};
    };
    SparseBasisOptions refine_off = SparseBasisOptions{};
    refine_off.maximum_refinement_steps = 0;
    // (a) Documented limitation: A = L1*L2^T from unit-diagonal integer
    // bidiagonals has cond ~ 2^2n, yet its LU has ALL unit pivots and zero
    // growth — pivot-based proxies see a perfectly healthy factorization. The
    // proxies are a screening signal, not a condition number.
    auto make_moler_l = [&n]() {
        std::vector<std::vector<double>> m(n, std::vector<double>(n, 0.0));
        for (std::size_t i = 0; i < n; ++i)
            m[i][i] = 1;
        for (std::size_t i = 1; i < n; ++i)
            m[i][i - 1] = -1;
        m[n - 1][0] = 1;
        return m;
    };
    const auto l1 = make_moler_l();
    const auto l2 = make_moler_l();
    std::vector<std::vector<double>> a_cols(n, std::vector<double>(n, 0.0));
    for (std::size_t j = 0; j < n; ++j)  // A = L1 * L2^T, columns of A
        for (std::size_t i = 0; i < n; ++i) {
            long double sum = 0;
            for (std::size_t k = 0; k < n; ++k)
                sum += static_cast<long double>(l1[i][k]) * l2[j][k];
            a_cols[j][i] = static_cast<double>(sum);
        }
    auto a_csc = SparseCsc::from_columns(n, a_cols);
    auto moler = SparseBasisFactorization::factorize(a_csc);
    req(moler.diagnostics().minimum_absolute_pivot == 1.0 &&
            moler.diagnostics().growth_factor == 1.0,
        "moler fixture has unit pivots");
    std::vector<double> ones(n, 1.0), moler_b(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        long double sum = 0;
        for (std::size_t j = 0; j < n; ++j)
            sum += static_cast<long double>(a_cols[j][i]) * ones[j];
        moler_b[i] = static_cast<double>(sum);
    }
    (void)moler.solve(moler_b);
    req(moler.statistics().refinement_attempts == 0,
        "documented limitation: unit-pivot conditioning evades proxies");
    // (b) Benefit case: random 24x24 basis, last column scaled by 1e-9. The LU
    // pivot ratio ~1e9 fires the default condition trigger, and random entries
    // force genuine rounding so both error metrics are non-zero.
    std::mt19937_64 rw_rng(0x5245463238ULL);
    const std::size_t m = 24;
    std::vector<std::vector<double>> ill_cols(m, std::vector<double>(m));
    for (auto& col : ill_cols)
        for (auto& q : col)
            q = v(rw_rng);
    for (std::size_t i = 0; i < m; ++i)
        ill_cols[i][i] += static_cast<double>(m);
    for (std::size_t i = 0; i < m; ++i)
        ill_cols[m - 1][i] *= 1e-9;
    auto ill_csc = SparseCsc::from_columns(m, ill_cols);
    const std::vector<double> truth(m, 10.0);
    std::vector<double> b_ill(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        long double sum = 0;
        for (std::size_t j = 0; j < m; ++j)
            sum += static_cast<long double>(ill_cols[j][i]) * truth[j];
        b_ill[i] = static_cast<double>(sum);
    }
    auto ill_off = SparseBasisFactorization::factorize(ill_csc, refine_off);
    const auto [ferr0, res0] = measure(ill_off, ill_csc, b_ill, truth);
    req(ill_off.statistics().refinements_applied == 0, "disabled refinement stays off");
    auto ill_on = SparseBasisFactorization::factorize(ill_csc);
    const auto [ferr1, res1] = measure(ill_on, ill_csc, b_ill, truth);
    req(ill_on.statistics().refinement_attempts >= 1, "condition trigger fires by default");
    req(ill_on.statistics().refinements_applied >= 1, "refinement engaged");
    req(ferr1 < ferr0, "refinement improves forward error");
    req(res1 <= res0, "refinement never worsens backward error");
    req(ferr1 < 1e-5, "refined forward error floor");
    // Well-conditioned basis with a short chain must not pay for refinement.
    auto small = SparseBasisFactorization::factorize(csc);
    (void)small.solve(b);
    req(small.statistics().refinement_attempts == 0, "no refinement when clean");
    // Chain-length trigger: force updates past the threshold, then refine.
    SparseBasisOptions trigger = SparseBasisOptions{};
    trigger.refinement_trigger_updates = 2;
    trigger.eta_density_trigger = 1;  // keep the chain: density must not refactor
    auto trig = SparseBasisFactorization::factorize(csc, trigger);
    trig.replace_column(0, std::vector<double>{3, 1, 1});
    trig.replace_column(2, std::vector<double>{1, 1, 4});
    std::vector<double> b_trig{7e6, -2e6, 4e6};
    (void)trig.solve(b_trig);
    req(trig.statistics().refinement_attempts >= 1, "update-chain trigger fires");
    // Pivot-ratio condition proxy.
    SparseLuDiagnostics diag;
    diag.minimum_absolute_pivot = 1e-3;
    diag.maximum_absolute_pivot = 1e3;
    req(sparse_condition_estimate(diag) == 1e6, "condition estimate ratio");
    diag.minimum_absolute_pivot = 0;
    req(std::isinf(sparse_condition_estimate(diag)), "degenerate condition estimate");
    // R6: fill-reducing (minimum-degree) column ordering. Two invariants: the
    // ordering must never change the answer, and it must actually cut fill on a
    // structured basis (a 5-point-stencil-like banded pattern, where the natural
    // order is far from minimum degree).
    {
        const std::size_t band = 300;
        std::vector<std::vector<double>> banded(band, std::vector<double>(band, 0.0));
        for (std::size_t j = 0; j < band; ++j) {
            banded[j][j] = 4.0;
            if (j + 1 < band) banded[j + 1][j] = -1.0;
            if (j >= 1) banded[j - 1][j] = -1.0;
            if (j + 28 < band) banded[j + 28][j] = -1.0;
            if (j >= 28) banded[j - 28][j] = -1.0;
        }
        auto banded_csc = SparseCsc::from_columns(band, banded);
        std::vector<double> rhs(band);
        for (std::size_t i = 0; i < band; ++i)
            rhs[i] = 1.0 + static_cast<double>(i % 5);
        const auto natural = SparseLu::factorize(banded_csc, 1e-14, 4U * 1024U * 1024U, false);
        const auto ordered = SparseLu::factorize(banded_csc, 1e-14, 4U * 1024U * 1024U, true);
        req(ordered.diagnostics().factor_nonzeros < natural.diagnostics().factor_nonzeros,
            "minimum-degree ordering reduces fill");
        const auto x_natural = natural.solve(rhs);
        const auto x_ordered = ordered.solve(rhs);
        req(error(x_natural, x_ordered) < 1e-9, "ordering does not change the solve");
        const auto xt_natural = natural.solve_transpose(rhs);
        const auto xt_ordered = ordered.solve_transpose(rhs);
        req(error(xt_natural, xt_ordered) < 1e-9, "ordering does not change the transpose solve");
        // The basis-level option must thread through to SparseLu.
        SparseBasisOptions ordered_opts = SparseBasisOptions{};
        ordered_opts.fill_reducing_ordering = true;
        auto basis_ordered = SparseBasisFactorization::factorize(banded_csc, ordered_opts);
        req(basis_ordered.diagnostics().factor_nonzeros ==
                ordered.diagnostics().factor_nonzeros,
            "basis option threads fill ordering through");
        std::cout << "[+] R6 fill reduction: " << natural.diagnostics().factor_nonzeros << " -> "
                  << ordered.diagnostics().factor_nonzeros << " nnz ("
                  << static_cast<int>(100.0 * (1.0 - double(ordered.diagnostics().factor_nonzeros) /
                                               double(natural.diagnostics().factor_nonzeros)))
                  << "%)\n";
    }
    std::cout << "sparse basis tests passed\n";
}
