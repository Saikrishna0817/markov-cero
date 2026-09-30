// MIP-01 contract §7.2: exhaustive small integer programs. Every production
// solve must match an in-test brute-force enumeration exactly — objective,
// bound consistency and an independently feasible incumbent.

#include "markov_cero/milp/milp_solver.hpp"
#include "support/tiny_exact.hpp"

#include <cmath>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Row {
    std::vector<double> coeffs;
    double lower; // -inf allowed
    double upper; // +inf allowed
};

struct Instance {
    std::vector<std::vector<double>> objective; // sense handled by milp (minimize here)
    std::vector<Row> rows;
    std::vector<int> lo, hi; // integer box for enumeration
};

markov_cero::model::Model make_model(const Instance& inst) {
    using namespace markov_cero;
    model::Model m;
    m.name = "BRUTE";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective = inst.objective[0];
    model::SparseMatrixBuilder builder(
        static_cast<std::size_t>(inst.rows.size()), inst.lo.size());
    for (std::size_t i = 0; i < inst.rows.size(); ++i)
        for (std::size_t j = 0; j < inst.rows[i].coeffs.size(); ++j)
            if (inst.rows[i].coeffs[j] != 0.0)
                builder.add(static_cast<std::size_t>(i), j, inst.rows[i].coeffs[j]);
    m.matrix = builder.build();
    for (const auto& row : inst.rows) {
        m.row_lower.push_back(std::isfinite(row.lower)
            ? model::Bound::finite(row.lower) : model::Bound::negative_infinity());
        m.row_upper.push_back(std::isfinite(row.upper)
            ? model::Bound::finite(row.upper) : model::Bound::positive_infinity());
    }
    for (std::size_t j = 0; j < inst.lo.size(); ++j) {
        m.variable_lower.push_back(model::Bound::finite(static_cast<double>(inst.lo[j])));
        m.variable_upper.push_back(model::Bound::finite(static_cast<double>(inst.hi[j])));
        m.variable_type.push_back(model::VariableType::integer);
        m.variable_name.push_back("V" + std::to_string(j));
    }
    m.row_name.reserve(inst.rows.size());
    for (std::size_t i = 0; i < inst.rows.size(); ++i)
        m.row_name.push_back("R" + std::to_string(i));
    m.validate();
    return m;
}

bool feasible_at(const Instance& inst, const std::vector<int>& p) {
    for (const auto& row : inst.rows) {
        double lhs = 0.0;
        for (std::size_t j = 0; j < p.size(); ++j) lhs += row.coeffs[j] * p[j];
        if (lhs < row.lower - 1e-9 || lhs > row.upper + 1e-9) return false;
    }
    return true;
}

double objective_at(const Instance& inst, const std::vector<int>& p) {
    double value = 0.0;
    for (std::size_t j = 0; j < p.size(); ++j) value += inst.objective[0][j] * p[j];
    return value;
}

// Brute-force enumeration over the box; returns false when no point is feasible.
bool brute_force(const Instance& inst, double& best, std::vector<int>& best_point,
                 std::size_t& feasible_count) {
    feasible_count = 0;
    best = std::numeric_limits<double>::infinity();
    markov_cero::test_support::enumerate_integer_box(
        inst.lo, inst.hi, [&](const std::vector<int>& p) {
            if (!feasible_at(inst, p)) return;
            ++feasible_count;
            const double value = objective_at(inst, p);
            if (value < best) { best = value; best_point = p; }
        });
    return feasible_count > 0;
}

void check_instance(const std::string& name, const Instance& inst) {
    const auto model = make_model(inst);
    double brute = 0.0;
    std::vector<int> point;
    std::size_t feasible_count = 0;
    const bool has_feasible = brute_force(inst, brute, point, feasible_count);
    const auto result = markov_cero::milp::solve(model, markov_cero::milp::Options{});
    if (!has_feasible) {
        require(result.status == markov_cero::lp::reference::SolveStatus::infeasible,
                name + ": production must prove infeasibility when the box is empty");
        return;
    }
    require(result.status == markov_cero::lp::reference::SolveStatus::optimal,
            name + ": production must prove optimality");
    const double scale = 1.0 + std::fabs(brute);
    require(std::fabs(result.objective - brute) <= 1e-6 * scale,
            name + ": objective must match brute-force enumeration exactly");
    require(std::isfinite(result.best_bound) &&
                result.best_bound <= result.objective + 1e-6 * scale,
            name + ": reported bound is never overstated");
    require(result.best_bound >= result.objective - 1e-4 * scale,
            name + ": closed tree reports a bound at the incumbent");
    require(result.primal.size() == inst.lo.size(), name + ": incumbent dimension");
    for (std::size_t j = 0; j < result.primal.size(); ++j)
        require(std::fabs(result.primal[j] - std::round(result.primal[j])) <= 1e-6,
                name + ": incumbent is integral");
    for (const auto& row : inst.rows) {
        double lhs = 0.0;
        for (std::size_t j = 0; j < result.primal.size(); ++j)
            lhs += row.coeffs[j] * result.primal[j];
        require(lhs >= row.lower - 1e-6 && lhs <= row.upper + 1e-6,
                name + ": independently verified incumbent feasibility");
    }
    std::cout << "[+] " << name << " matched brute force (" << feasible_count
              << " feasible points, objective " << brute << ")\n";
}

Instance knapsack() {
    Instance inst;
    inst.objective = {{-10.0, -14.0, -12.0, -7.0}};
    inst.rows = {{{4.0, 6.0, 5.0, 3.0}, -std::numeric_limits<double>::infinity(), 10.0}};
    inst.lo = {0, 0, 0, 0};
    inst.hi = {1, 1, 1, 1};
    return inst;
}

Instance general_integers() {
    Instance inst;
    inst.objective = {{-2.0, -3.0, -1.0}};
    inst.rows = {
        {{2.0, 3.0, 1.0}, -std::numeric_limits<double>::infinity(), 8.0},
        {{1.0, 1.0, 2.0}, 3.0, std::numeric_limits<double>::infinity()},
    };
    inst.lo = {0, 0, 0};
    inst.hi = {5, 5, 5};
    return inst;
}

Instance negative_bounds() {
    Instance inst;
    inst.objective = {{1.0, -4.0}};
    inst.rows = {{{1.0, 2.0}, -std::numeric_limits<double>::infinity(), 5.0}};
    inst.lo = {-3, -2};
    inst.hi = {3, 4};
    return inst;
}

Instance infeasible_box() {
    Instance inst;
    inst.objective = {{1.0}};
    inst.rows = {{{1.0}, -std::numeric_limits<double>::infinity(), 0.0}, {{1.0}, 1.0, std::numeric_limits<double>::infinity()}};
    inst.lo = {0};
    inst.hi = {4};
    return inst;
}

// Deterministic LCG so the seeded cases are reproducible across platforms.
Instance seeded_random(std::uint32_t& state, std::size_t variables) {
    auto next = [&state]() {
        state = state * 1664525u + 1013904223u;
        return state;
    };
    Instance inst;
    std::vector<double> objective(variables);
    for (auto& c : objective)
        c = -static_cast<double>(next() % 21u) + 10.0; // [-10, 10]
    inst.objective = {objective};
    const auto capacity = static_cast<double>(next() % 20u) + 4.0;
    std::vector<double> first(variables);
    for (auto& c : first) c = static_cast<double>(next() % 5u) + 1.0;
    std::vector<double> second(variables);
    for (auto& c : second) c = static_cast<double>(next() % 4u) + 1.0;
    inst.rows = {
        {first, -std::numeric_limits<double>::infinity(), capacity},
        {second, 3.0, std::numeric_limits<double>::infinity()},
    };
    inst.lo.assign(variables, 0);
    inst.hi.assign(variables, 1);
    return inst;
}

} // namespace

int main() {
    check_instance("knapsack4", knapsack());
    check_instance("general_integers", general_integers());
    check_instance("negative_bounds", negative_bounds());
    check_instance("infeasible_box", infeasible_box());
    std::uint32_t state = 20260930u;
    for (std::size_t i = 0; i < 8; ++i)
        check_instance("seeded_random_" + std::to_string(i), seeded_random(state, 6));
    std::cout << "All brute-force cross-checks PASSED successfully!\n";
}
