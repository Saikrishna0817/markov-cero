#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace markov_cero;
namespace {
void req(bool q, const char* m) {
    if (!q)
        throw std::runtime_error(m);
}
transform::CanonicalModel cm(std::size_t r, std::size_t c, std::vector<double> a,
                             std::vector<double> b, std::vector<double> cost, double off = 0) {
    transform::CanonicalModel m;
    m.matrix = {r, c, std::move(a)};
    m.rhs = std::move(b);
    m.objective = std::move(cost);
    m.objective_offset = off;
    m.record.objective_sign = 1;
    m.record.structural_variables = c;
    m.record.variables.resize(c);
    for (std::size_t j = 0; j < c; ++j) {
        m.record.variables[j].canonical_index = {j};
        m.record.variables[j].multiplier = {1};
    }
    m.validate();
    return m;
}
} // namespace
int main() {
    auto optimal = cm(1, 2, {1, 1}, {1}, {-1, 0}, 5);
    auto ro = lp::reference::solve(optimal);
    req(ro.status == lp::reference::SolveStatus::optimal, "optimal status");
    req(std::abs(ro.objective - 4) < 1e-9, "optimal objective");
    req(verify::verify_reference_result(optimal, ro).accepted, "optimal certificate");
    auto phase1 = cm(1, 2, {2, 3}, {6}, {1, 0});
    auto rp = lp::reference::solve(phase1);
    req(rp.status == lp::reference::SolveStatus::optimal, "phase I status");
    req(verify::verify_reference_result(phase1, rp).accepted, "phase I result");
    req(rp.phase_one_iterations > 0, "phase I executed");
    auto infeasible = cm(1, 1, {1}, {-1}, {0});
    auto ri = lp::reference::solve(infeasible);
    req(ri.status == lp::reference::SolveStatus::infeasible, "infeasible status");
    req(verify::verify_reference_result(infeasible, ri).accepted, "Farkas certificate");
    auto unbounded = cm(1, 2, {1, -1}, {0}, {-1, 0});
    auto ru = lp::reference::solve(unbounded);
    req(ru.status == lp::reference::SolveStatus::unbounded, "unbounded status");
    req(verify::verify_reference_result(unbounded, ru).accepted, "unbounded ray");
    auto degenerate = cm(2, 3, {1, 0, 1, 0, 1, 1}, {0, 1}, {0, 0, -1});
    auto rd = lp::reference::solve(degenerate);
    req(rd.status == lp::reference::SolveStatus::optimal, "degenerate status");
    req(verify::verify_reference_result(degenerate, rd).accepted, "degenerate verification");
    bool saw = false;
    for (const auto& e : rd.telemetry)
        saw = saw || e.degenerate;
    req(saw, "degenerate pivot telemetry");
    auto cycling =
        cm(3, 7, {0.5, -5.5, -2.5, 9, 1, 0, 0, 0.5, -1.5, -0.5, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1},
           {0, 0, 1}, {-10, 57, 9, 24, 0, 0, 0});
    auto rc = lp::reference::solve(cycling);
    req(rc.status == lp::reference::SolveStatus::optimal, "cycling status");
    req(std::abs(rc.objective + 1) < 1e-8, "cycling objective");
    req(verify::verify_reference_result(cycling, rc).accepted, "cycling verification");
    auto close_ratio = cm(2, 3, {1, 0, 1, 0, 1, 1000}, {1.0000000005, 1000}, {0, 0, -1});
    auto cr = lp::reference::solve(close_ratio);
    req(cr.status == lp::reference::SolveStatus::optimal, "close-ratio status");
    req(verify::verify_reference_result(close_ratio, cr).accepted, "close-ratio verification");
    req(std::abs(cr.objective + 1) < 1e-9, "close-ratio objective");
    auto ratio_overflow = cm(1, 2, {1, 1e-308}, {1e308}, {0, -1});
    lp::reference::Options tiny_pivot;
    tiny_pivot.pivot_tolerance = 1e-320;
    auto overflow_result = lp::reference::solve(ratio_overflow, tiny_pivot);
    req(overflow_result.status == lp::reference::SolveStatus::numerical_failure,
        "ratio overflow fails closed");
    lp::reference::Options huge_tolerance;
    huge_tolerance.feasibility_tolerance = std::numeric_limits<double>::max();
    req(lp::reference::solve(infeasible, huge_tolerance).status ==
            lp::reference::SolveStatus::invalid_options,
        "huge tolerance rejected");
    lp::reference::Options bounded_log;
    bounded_log.bland_anti_cycling = false;
    bounded_log.iteration_limit = 100000;
    bounded_log.telemetry_limit = 7;
    auto bounded_log_result = lp::reference::solve(cycling, bounded_log);
    req(bounded_log_result.status == lp::reference::SolveStatus::optimal,
        "EXPAND anti-stalling resolves cycling without Bland");
    req(std::abs(bounded_log_result.objective + 1) < 1e-8, "EXPAND cycling objective");
    req(verify::verify_reference_result(cycling, bounded_log_result).accepted,
        "EXPAND cycling certificate");
    req(bounded_log_result.telemetry.size() <= 7, "telemetry bounded");
    // Klee-Minty cube (n=5, Chvatal/Wikipedia scaled, inequality form with slack
    // columns FIRST so the crash basis is the all-slack origin): Dantzig pricing
    // takes 9 pivots, overflowing the 3-entry telemetry window and exercising the
    // truncation flag (EXPAND resolves the Beale example too quickly to overflow it).
    auto klee_minty =
        cm(5, 10, {1, 0, 0, 0, 0,   1, 0, 0, 0, 0,
                   0, 1, 0, 0, 0,   20, 1, 0, 0, 0,
                   0, 0, 1, 0, 0,   200, 20, 1, 0, 0,
                   0, 0, 0, 1, 0,   2000, 200, 20, 1, 0,
                   0, 0, 0, 0, 1,   20000, 2000, 200, 20, 1},
           {1, 100, 10000, 1000000, 100000000},
           {0, 0, 0, 0, 0,   -10000, -1000, -100, -10, -1});
    lp::reference::Options km_options;
    km_options.bland_anti_cycling = false;
    km_options.iteration_limit = 100000;
    km_options.telemetry_limit = 3;
    auto km_result = lp::reference::solve(klee_minty, km_options);
    req(km_result.status == lp::reference::SolveStatus::optimal, "Klee-Minty status");
    req(std::abs(km_result.objective + 1e8) < 1.0, "Klee-Minty objective");
    req(verify::verify_reference_result(klee_minty, km_result).accepted, "Klee-Minty certificate");
    req(km_result.telemetry.size() <= 3, "Klee-Minty telemetry bounded");
    req(km_result.telemetry_truncated, "Klee-Minty telemetry truncation reported");
    auto invalid_ray = ru;
    invalid_ray.ray[0] = -1e-308;
    req(!verify::verify_reference_result(unbounded, invalid_ray).accepted,
        "tiny negative ray rejected");
    auto corrupt = ro;
    corrupt.primal[0] += 0.1;
    req(!verify::verify_reference_result(optimal, corrupt).accepted, "corrupt optimum rejected");
    auto badray = ru;
    badray.ray[0] = 0;
    req(!verify::verify_reference_result(unbounded, badray).accepted, "corrupt ray rejected");
    auto badcert = ri;
    badcert.certificate[0] = 0;
    req(!verify::verify_reference_result(infeasible, badcert).accepted,
        "corrupt Farkas certificate rejected");
    lp::reference::Options limit;
    limit.iteration_limit = 1;
    auto rl = lp::reference::solve(cycling, limit);
    req(rl.status == lp::reference::SolveStatus::iteration_limit, "iteration limit");
    lp::reference::Options deadline;
    deadline.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    auto timed = lp::reference::solve(cycling, deadline);
    req(timed.status == lp::reference::SolveStatus::resource_limit,
        "expired wall-clock deadline returns ResourceLimit");
    std::cout << "primal revised simplex tests passed\n";
}
