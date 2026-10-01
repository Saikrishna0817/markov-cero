// MIQP-01 contract §7.7 (docs/contracts/miqp-node-bounds.md): seeded
// small integer boxes with quadratic objectives — milp::solve must match
// exhaustive enumeration on the optimum, the honest global bound and the
// feasible incumbent. Infeasible regions must report infeasible.

#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/model/model.hpp"

#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/tiny_exact.hpp"

using namespace markov_cero;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct RandomCase {
    std::vector<std::vector<double>> quad; // full symmetric, diagonally dominant ≥ 0
    std::vector<double> linear;
    std::vector<std::vector<double>> rows;
    std::vector<double> rhs;
    std::vector<bool> row_sense_upper; // true: a·x ≤ rhs, false: a·x ≥ rhs
    double offset{};
    std::vector<int> lower, upper;
};

RandomCase generate(std::mt19937& rng) {
    std::uniform_int_distribution<int> dim_dist(1, 3);
    std::uniform_real_distribution<double> lin_dist(-3.0, 3.0);
    std::uniform_real_distribution<double> off_dist(-1.0, 1.0);
    std::uniform_real_distribution<double> offset_dist(-2.0, 2.0);
    std::uniform_int_distribution<int> row_count_dist(0, 2);
    std::uniform_int_distribution<int> row_entry_dist(-1, 1);
    std::uniform_int_distribution<int> rhs_dist(-3, 6);
    std::uniform_int_distribution<int> bound_dist(0, 3);
    RandomCase c;
    const int n = dim_dist(rng);
    c.quad.assign(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        double coupling = 0.0;
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            const double v = off_dist(rng);
            c.quad[i][j] = v;
            coupling += std::fabs(v);
        }
        c.quad[i][i] = coupling + std::fabs(lin_dist(rng)) * 0.25 + 0.5; // ≥ 0, dominant
    }
    c.linear.resize(n);
    for (int j = 0; j < n; ++j) c.linear[j] = lin_dist(rng);
    c.offset = offset_dist(rng);
    const int m = row_count_dist(rng);
    for (int i = 0; i < m; ++i) {
        std::vector<double> a(n);
        bool all_zero = true;
        for (int j = 0; j < n; ++j) {
            a[j] = static_cast<double>(row_entry_dist(rng));
            all_zero = all_zero && a[j] == 0.0;
        }
        if (all_zero) a[0] = 1.0;
        c.rows.push_back(a);
        c.rhs.push_back(static_cast<double>(rhs_dist(rng)));
        c.row_sense_upper.push_back(rng() % 2 == 0);
    }
    for (int j = 0; j < n; ++j) {
        int lo = bound_dist(rng);
        int hi = bound_dist(rng);
        while (hi < lo) hi = bound_dist(rng);
        c.lower.push_back(lo);
        c.upper.push_back(hi);
    }
    return c;
}

// Independent long-double recompute over the generated raw arrays — shares
// no code with the solver's objective paths.
long double evaluate(const RandomCase& c, const std::vector<int>& x) {
    long double value = c.offset;
    for (std::size_t j = 0; j < x.size(); ++j)
        value += static_cast<long double>(c.linear[j]) * x[j];
    for (std::size_t i = 0; i < x.size(); ++i)
        for (std::size_t j = 0; j < x.size(); ++j)
            value += 0.5L * static_cast<long double>(c.quad[i][j]) * x[i] * x[j];
    return value;
}

bool feasible(const RandomCase& c, const std::vector<int>& x) {
    for (std::size_t i = 0; i < c.rows.size(); ++i) {
        long double activity = 0.0L;
        for (std::size_t j = 0; j < x.size(); ++j)
            activity += static_cast<long double>(c.rows[i][j]) * x[j];
        if (c.row_sense_upper[i] && activity > c.rhs[i] + 1e-9) return false;
        if (!c.row_sense_upper[i] && activity < c.rhs[i] - 1e-9) return false;
    }
    return true;
}

model::Model make_model(const RandomCase& c) {
    const std::size_t n = c.lower.size();
    model::Model m;
    m.name = "RANDOM_MIQP";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective_offset = c.offset;
    m.objective = c.linear;
    m.has_quadratic_objective = true;
    model::SparseMatrixBuilder qb(n, n);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
            if (c.quad[i][j] != 0.0) qb.add(i, j, c.quad[i][j]);
    m.quadratic_matrix = qb.build();
    model::SparseMatrixBuilder ab(c.rows.size(), n);
    for (std::size_t i = 0; i < c.rows.size(); ++i) {
        for (std::size_t j = 0; j < n; ++j)
            if (c.rows[i][j] != 0.0) ab.add(i, j, c.rows[i][j]);
        m.row_lower.push_back(c.row_sense_upper[i] ? model::Bound::negative_infinity()
                                                   : model::Bound::finite(c.rhs[i]));
        m.row_upper.push_back(c.row_sense_upper[i] ? model::Bound::finite(c.rhs[i])
                                                   : model::Bound::positive_infinity());
        m.row_name.push_back("R" + std::to_string(i));
    }
    m.matrix = ab.build();
    for (std::size_t j = 0; j < n; ++j) {
        m.variable_lower.push_back(model::Bound::finite(c.lower[j]));
        m.variable_upper.push_back(model::Bound::finite(c.upper[j]));
        m.variable_type.push_back(model::VariableType::integer);
        m.variable_name.push_back("X" + std::to_string(j));
    }
    m.validate();
    return m;
}

void differential(std::mt19937& rng) {
    for (int trial = 0; trial < 12; ++trial) {
        const auto c = generate(rng);
        bool any_feasible = false;
        long double optimum = 0.0L;
        test_support::enumerate_integer_box(c.lower, c.upper, [&](const std::vector<int>& x) {
            if (!feasible(c, x)) return;
            const long double value = evaluate(c, x);
            if (!any_feasible || value < optimum) {
                any_feasible = true;
                optimum = value;
            }
        });
        const auto m = make_model(c);
        milp::Options options;
        options.time_limit_seconds = 30.0;
        const auto result = milp::solve(m, options);
        if (!any_feasible) {
            // Exhaustive enumeration proves the box empty. The search must
            // never report an optimum, keep an incumbent or fabricate a
            // bound. Whether ADMM certifies its Farkas witness at the 1e-6
            // node gate decides Infeasible (P2) versus the fail-closed
            // numerical_failure (P5, contract §3) — both are honest.
            require(result.status != lp::reference::SolveStatus::optimal &&
                        result.status != lp::reference::SolveStatus::gap_satisfied,
                    "empty box never reports an optimum");
            require(result.primal.empty(), "empty box keeps no incumbent");
            require(!std::isfinite(result.best_bound), "empty box keeps no global bound");
            continue;
        }
        require(result.status == lp::reference::SolveStatus::optimal,
                "small convex MIQP must solve to optimality");
        require(result.primal.size() == c.lower.size(), "incumbent dimension");
        std::vector<int> rounded;
        for (std::size_t j = 0; j < result.primal.size(); ++j) {
            require(std::fabs(result.primal[j] - std::round(result.primal[j])) < 1e-6,
                    "incumbent is integral");
            require(result.primal[j] >= c.lower[j] - 1e-6 && result.primal[j] <= c.upper[j] + 1e-6,
                    "incumbent inside the box");
            rounded.push_back(static_cast<int>(std::lround(result.primal[j])));
        }
        require(feasible(c, rounded), "incumbent satisfies the rows");
        const long double reported = static_cast<long double>(result.objective);
        require(std::fabs(reported - optimum) < 1e-5, "objective matches exhaustive enumeration");
        require(std::isfinite(result.best_bound), "global bound reported");
        require(static_cast<long double>(result.best_bound) <= optimum + 1e-5L,
                "global bound never overstates the enumerated optimum");
        require(static_cast<long double>(result.best_bound) >= optimum - 1e-5L,
                "global bound closes at optimality");
    }
}
} // namespace

int main() {
    try {
        std::mt19937 rng(20261001U);
        differential(rng);
        std::cout << "All MIQP randomized enumeration tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] Error: " << error.what() << "\n";
        return 1;
    }
}
