// QP reporting contract: apps/json_output.hpp publishes
// "maximum_primal_violation" from Result::primal_report.maximum_row_violation,
// so a QP engine that never fills the report advertises 0 while the ADMM
// residual and the row slacks carry the measured value. engine_qp fills it
// now; this test pins that behavior on QPLIB_0010, the model whose five
// BENCH-02 repeats failed the campaign harness's independent primal re-check
// (1.28e-6, above the harness's 1e-6 bar and inside the QP engine's 1e-4
// verification gate).
#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace markov_cero;

void req(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

model::Model read_tracked_qp() {
    const char* source_dir = std::getenv("MARKOV_CERO_SOURCE_DIR");
    req(source_dir != nullptr,
        "MARKOV_CERO_SOURCE_DIR must point at the source tree");
    std::ifstream file(std::string(source_dir) + "/data/qp/QPLIB_0010.mps");
    req(static_cast<bool>(file), "open data/qp/QPLIB_0010.mps");
    return io::parse_mps(file);
}

void qp_report_carries_the_measured_violation() {
    const model::Model model = read_tracked_qp();

    api::SolveOptions options;
    options.engine = "qp";
    const auto res = api::solve_model(model, options);
    req(res.status == lp::reference::SolveStatus::optimal,
        "QPLIB_0010 solves to optimality");
    req(res.verified && res.original_verified,
        "QPLIB_0010 witness accepted at the 1e-4 QP gate");

    // The report is a measurement of the same iterate at the same tolerances
    // the engine documents for this report, recomputed independently here.
    const verify::Candidate candidate{res.primal, res.objective};
    const auto expected = verify::verify_primal(model, candidate, {1e-4, 1e-4},
                                                {1e-4, 1e-4}, 1e-6, true);
    req(res.primal_report.maximum_row_violation ==
            expected.maximum_row_violation,
        "reported row violation equals an independent recomputation");
    req(res.primal_report.maximum_variable_violation ==
            expected.maximum_variable_violation,
        "reported variable violation equals an independent recomputation");

    // Regression: the default-constructed report emits 0, so this would have
    // failed before engine_qp filled it. The budget row of this model is
    // violated at the ~1e-6 ADMM tolerance, never at exactly zero.
    req(res.primal_report.maximum_row_violation > 1e-9,
        "QP report carries the measured row violation, not a default zero");
    req(res.primal_report.maximum_row_violation <= 1e-4,
        "measured violation stays inside the documented QP gate");
    req(res.primal_report.passed == res.original_verified,
        "report verdict cannot contradict the engine's own gate verdict");

    std::cout << "qp_primal_report: maximum_row_violation = "
              << res.primal_report.maximum_row_violation << "\n";
}

void qp_report_matches_diagnostic_residual() {
    const model::Model model = read_tracked_qp();
    api::SolveOptions options;
    options.engine = "qp";
    const auto res = api::solve_model(model, options);
    req(res.status == lp::reference::SolveStatus::optimal,
        "QPLIB_0010 second solve optimal");
    // Before the fix these disagreed: diagnostic.primal_residual was the ADMM
    // residual (1.28e-6) while maximum_primal_violation was the default 0.
    req(res.diagnostic.primal_residual > 1e-9,
        "diagnostic residual stays nonzero");
    req(res.primal_report.maximum_row_violation > 1e-9,
        "report violation stays nonzero");
    // Same iterate, two computations (ADMM residual vs original-model
    // recomputation): they may differ by rounding, not by definition.
    req(std::abs(res.diagnostic.primal_residual -
                 res.primal_report.maximum_row_violation) <= 1e-15,
        "diagnostic residual and reported violation agree on this model");
}
} // namespace

int main() {
    try {
        qp_report_carries_the_measured_violation();
        qp_report_matches_diagnostic_residual();
    } catch (const std::exception& exc) {
        std::cerr << "qp_primal_report: FAILED: " << exc.what() << "\n";
        return 1;
    }
    std::cout << "qp_primal_report: all cases passed\n";
    return 0;
}
