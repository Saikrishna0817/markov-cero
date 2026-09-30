// QP-01 contract §6 (BENCHMARK REQUIRED): measure convexity classification,
// KKT factor fill, ADMM iteration/refactorization behavior and witness
// acceptance on the tracked QPLIB subset. One JSON object per model on
// stdout; no speed claim.
//
//   qp_kkt_benchmark <model.mps>...
//
// kkt_L_nonzeros is a separate probe factorization of the as-built KKT
// matrix at the solver's initial penalties (sigma = 1e-6, rho = 0.1), so the
// fill number does not depend on which rho an adaptive solve ended on.
#include "markov_cero/io/mps.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/kkt.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/qp/verifier.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace markov_cero;
using Clock = std::chrono::steady_clock;

std::string basename_of(const std::string& path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string convexity_name(qp::ConvexityStatus status) {
    switch (status) {
    case qp::ConvexityStatus::positive_semidefinite: return "positive_semidefinite";
    case qp::ConvexityStatus::non_convex: return "non_convex";
    case qp::ConvexityStatus::indeterminate: return "indeterminate";
    }
    return "indeterminate";
}

void emit(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open " + path);
    const auto model = io::parse_mps(input);
    const auto qp_model = qp::make_quadratic_model(model);

    const auto convexity = qp::assess_convexity(qp_model.P);

    qp::QpOptions options;
    options.absolute_tolerance = 1e-6;
    options.relative_tolerance = 1e-6;
    options.max_iterations = 10000;
    const auto start = Clock::now();
    const auto solution = qp::solve_qp(qp_model, options);
    const double solve_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();

    bool witness_accepted = false;
    double discrepancy = -1.0;
    if (solution.status == qp::QpStatus::optimal) {
        const auto report = qp::verify_qp_solution(qp_model, solution);
        witness_accepted = report.passed;
        discrepancy = report.objective_discrepancy;
    } else if (solution.status == qp::QpStatus::primal_infeasible) {
        witness_accepted = qp::verify_qp_infeasibility(qp_model, solution);
    } else if (solution.status == qp::QpStatus::dual_infeasible) {
        witness_accepted = qp::verify_qp_unbounded(qp_model, solution);
    }

    // Fill probe: fresh factorization of the same KKT shape at initial rho.
    qp::KktSolver fill_probe;
    const std::vector<double> rho(qp_model.num_constraints(), 0.1);
    const bool fill_ok = fill_probe.factorize(qp_model.P, qp_model.A, 1e-6, rho);
    const long long l_nonzeros =
        fill_ok ? static_cast<long long>(fill_probe.nonzeros_L()) : -1;

    std::cout << "{\"instance\":\"" << basename_of(path) << "\""
              << ",\"convexity_status\":\"" << convexity_name(convexity.status) << "\""
              << ",\"minimum_pivot\":" << convexity.minimum_pivot
              << ",\"convexity_factor_nonzeros\":" << convexity.factor_nonzeros
              << ",\"status\":\"" << qp::to_string(solution.status) << "\""
              << ",\"objective\":" << solution.objective_value
              << ",\"witness_accepted\":" << (witness_accepted ? "true" : "false")
              << ",\"objective_discrepancy\":" << discrepancy
              << ",\"admm_iterations\":" << solution.iterations
              << ",\"rho_updates\":" << solution.refactorization_count
              << ",\"condition_estimate\":" << solution.condition_estimate
              << ",\"kkt_symbolic_reuse\":" << solution.kkt_symbolic_reuse
              << ",\"kkt_L_nonzeros\":" << l_nonzeros
              << ",\"kkt_fill_limit\":" << (fill_probe.fill_limit_reached() ? "true" : "false")
              << ",\"kkt_dimension\":" << (fill_ok ? fill_probe.dimension() : 0)
              << ",\"variables\":" << qp_model.num_variables()
              << ",\"canonical_rows\":" << qp_model.num_constraints()
              << ",\"p_nonzeros\":" << qp_model.P.values.size()
              << ",\"solve_ms\":" << solve_ms
              << "}\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: qp_kkt_benchmark <model.mps>...\n";
        return 2;
    }
    try {
        for (int i = 1; i < argc; ++i) emit(argv[i]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
    return 0;
}
