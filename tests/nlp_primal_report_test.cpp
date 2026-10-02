// NLP/MINLP primal-report contract: numerical-policy.md section 4 (NLP/MINLP
// primal report mapping). engine_nonlinear.cpp used to leave primal_report
// default-constructed, so every NLP/MINLP result serialized
// "maximum_primal_violation": 0 with passed=false while the iterate was
// attached and original_verified=true — the sibling of the BENCH-02 QP
// reporting defect (evidence/bench02-primal-recheck-defect-2026-10-02.json,
// fixed for QP in e75201d; the NLP/MINLP case is recorded with before/after
// in evidence/nlp-primal-report-2026-10-02.json).
//
// Pins, per section 4:
//   row field     = max(0, g) / |h| over the callbacks, measured separately
//                   from bounds (no longer folded together);
//   variable field = bound side, its own field;
//   integrality   = max |x - round(x)| over integer variables only;
//   passed        = the engine gate verdict, never disagreeing with
//                   original_verified on any path that attaches a primal.
#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace markov_cero;

void req(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// minimize x0^2 + x1^2  s.t.  x0 + x1 >= 1.5,  bounds [0, 3];
// continuous (SQP) or with x1 integer (outer approximation).
model::Model quadratic_row_model(bool integer_second) {
    model::Model source;
    source.name = integer_second ? "minlp_primal_report" : "nlp_primal_report";
    model::SparseMatrixBuilder a(1, 2);
    a.add(0, 0, 1.0);
    a.add(0, 1, 1.0);
    source.matrix = a.build();
    source.objective = {0.0, 0.0};
    source.row_name = {"demand"};
    source.row_lower = {model::Bound::finite(1.5)};
    source.row_upper = {model::Bound::positive_infinity()};
    source.variable_name = {"x0", "x1"};
    source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(3.0), model::Bound::finite(3.0)};
    source.variable_type = {model::VariableType::continuous,
                            integer_second ? model::VariableType::integer
                                           : model::VariableType::continuous};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}, {1.0, 1, 1, true}};
    source.validate();
    return source;
}

// minimize x0^2 + x1^2  s.t.  x0 + x1 = 2 (equality row).
model::Model equality_model() {
    model::Model source;
    source.name = "nlp_equality_primal_report";
    model::SparseMatrixBuilder a(1, 2);
    a.add(0, 0, 1.0);
    a.add(0, 1, 1.0);
    source.matrix = a.build();
    source.objective = {0.0, 0.0};
    source.row_name = {"sum"};
    source.row_lower = {model::Bound::finite(2.0)};
    source.row_upper = {model::Bound::finite(2.0)};
    source.variable_name = {"x0", "x1"};
    source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(3.0), model::Bound::finite(3.0)};
    source.variable_type = {model::VariableType::continuous, model::VariableType::continuous};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}, {1.0, 1, 1, true}};
    source.validate();
    return source;
}

double direct_row_violation(const std::vector<double>& x, double lower) {
    // Row [lower, inf): violation = max(0, lower - (x0 + x1)).
    return std::max(0.0, lower - (x[0] + x[1]));
}

double direct_bound_violation(const std::vector<double>& x) {
    double worst = 0.0;
    for (double v : x) {
        worst = std::max(worst, std::max(0.0, 0.0 - v));
        worst = std::max(worst, std::max(0.0, v - 3.0));
    }
    return worst;
}

// The verifier's own split, recomputed independently here at the same
// tolerance: the engine must publish exactly this measurement.
void require_matches_recomputation(const model::Model& source,
                                   const std::vector<double>& x,
                                   const verify::PrimalVerificationReport& report,
                                   double tolerance, const std::string& label) {
    const auto feas = nlp::verify_nlp_feasibility(io::make_nlp_model(source), x, tolerance);
    req(report.passed == feas.feasible, label + ": pass flag equals the recomputation");
    req(report.maximum_row_violation == feas.maximum_constraint_violation,
        label + ": row field equals the recomputed constraint violation");
    req(report.maximum_variable_violation == feas.maximum_bound_violation,
        label + ": variable field equals the recomputed bound violation");
}

void split_semantics_at_chosen_points() {
    const auto ineq = io::make_nlp_model(quadratic_row_model(false));

    // x = (0, 0): row violated by 1.5, bounds satisfied exactly at 0.
    const auto at_zero = nlp::verify_nlp_feasibility(ineq, {0.0, 0.0}, 1e-6);
    req(!at_zero.feasible, "infeasible point rejected");
    req(at_zero.maximum_constraint_violation == 1.5,
        "inequality residual lands in the constraint split");
    req(at_zero.maximum_bound_violation == 0.0, "satisfied bounds contribute zero");
    req(at_zero.maximum_violation == at_zero.maximum_constraint_violation,
        "folded value is the max of the split");

    // x = (-1, 2): bound violated by 1.0, row residual 0.5 — the two sides
    // must land in their own fields, not merged.
    const auto split = nlp::verify_nlp_feasibility(ineq, {-1.0, 2.0}, 1e-6);
    req(!split.feasible, "bound violation rejected");
    req(split.maximum_constraint_violation == 0.5, "constraint side keeps 0.5");
    req(split.maximum_bound_violation == 1.0, "bound side keeps 1.0");
    req(split.maximum_violation == 1.0, "folded value takes the max side");

    const auto eq = io::make_nlp_model(equality_model());
    const auto at_origin = nlp::verify_nlp_feasibility(eq, {0.0, 0.0}, 1e-6);
    req(at_origin.maximum_constraint_violation == 2.0, "equality residual is |h|");
    req(at_origin.maximum_bound_violation == 0.0, "equality point satisfies bounds");

    // Rejection propagates +inf into the split too, never a silent 0.
    const auto rejected = nlp::verify_nlp_feasibility(ineq, {0.0}, 1e-6);
    req(!rejected.feasible, "dimension mismatch rejected");
    req(rejected.maximum_violation == std::numeric_limits<double>::infinity(),
        "folded rejection is +inf");
    req(rejected.maximum_constraint_violation ==
            std::numeric_limits<double>::infinity() &&
            rejected.maximum_bound_violation == std::numeric_limits<double>::infinity(),
        "split rejection is +inf on both sides");
}

void engine_nlp_report_carries_the_measurement() {
    const model::Model model = quadratic_row_model(false);
    api::SolveOptions options;
    options.engine = "sqp";
    const auto res = api::solve_model(model, options);
    req(res.status == lp::reference::SolveStatus::local_optimal,
        "NLP case reaches LocalStationary");
    req(res.original_verified && !res.primal.empty(), "iterate attached and verified");

    // Regression: before engine_nonlinear filled the report the JSON field
    // was exactly 0 while diagnostic.primal_residual carried ~1e-8 for this
    // converged active row, and passed=false contradicted
    // original_verified=true. The row residual stays strictly positive (the
    // solver converges inside the 1e-6 gate, not to exact zeros).
    req(res.primal_report.passed == res.original_verified,
        "report verdict equals the engine gate verdict");
    req(res.primal_report.passed, "verified NLP iterate passes the report");
    req(res.primal_report.maximum_row_violation > 0.0,
        "active row keeps a measured sub-tolerance residual, not a default 0");
    req(res.primal_report.maximum_row_violation <= 1e-6,
        "measured row violation stays inside the documented 1e-6 NLP gate");
    req(res.primal_report.maximum_row_violation ==
            direct_row_violation(res.primal, 1.5),
        "row field equals max(0, lower - (x0 + x1)) on the reported iterate");
    req(res.primal_report.maximum_variable_violation ==
            direct_bound_violation(res.primal),
        "variable field is the bound side alone");
    req(res.primal_report.maximum_integrality_violation == 0.0,
        "continuous model reports zero integrality");
    // SQP's own constraint_violation and this recomputation measure the same
    // quantity at the same point; they may differ only by rounding.
    req(std::abs(res.diagnostic.primal_residual -
                 res.primal_report.maximum_row_violation) <= 1e-15,
        "diagnostic residual and reported row violation agree on this model");
    require_matches_recomputation(model, res.primal, res.primal_report, 1e-6, "nlp");
}

void engine_nlp_equality_verdict() {
    const model::Model model = equality_model();
    api::SolveOptions options;
    options.engine = "sqp";
    const auto res = api::solve_model(model, options);
    req(res.status == lp::reference::SolveStatus::local_optimal,
        "equality case reaches LocalStationary");
    req(res.primal_report.passed == res.original_verified,
        "equality report verdict equals the gate verdict");
    req(res.primal_report.maximum_row_violation ==
            std::abs(2.0 - (res.primal[0] + res.primal[1])),
        "equality row field equals |2 - (x0 + x1)|");
    req(res.primal_report.maximum_variable_violation == 0.0,
        "converged point satisfies its bounds");
    // A true zero measurement must keep passed=true: the pre-fix default
    // report had the same 0 fields but passed=false.
    req(res.primal_report.passed, "zero-residual attach still reports pass");
    require_matches_recomputation(model, res.primal, res.primal_report, 1e-6,
                                  "nlp-eq");
}

void engine_minlp_report_carries_the_measurement() {
    const model::Model model = quadratic_row_model(true);
    api::SolveOptions options;
    options.engine = "outer_approx";
    const auto res = api::solve_model(model, options);
    req(res.status == lp::reference::SolveStatus::optimal, "MINLP case optimal");
    req(res.original_verified && !res.primal.empty(), "incumbent attached and verified");
    req(res.primal_report.passed == res.original_verified,
        "MINLP report verdict equals the gate verdict");

    const double integrality =
        std::abs(res.primal[1] - std::round(res.primal[1]));
    req(res.primal_report.maximum_integrality_violation == integrality,
        "integrality field equals the integer residual of the incumbent");
    // engine_nonlinear sets diagnostic.primal_residual from the same folded
    // feasibility report the split fields come from: max(row, variable) is
    // exactly that value.
    req(std::max(res.primal_report.maximum_row_violation,
                 res.primal_report.maximum_variable_violation) ==
            res.diagnostic.primal_residual,
        "max of the split equals the engine's folded diagnostic residual");
    require_matches_recomputation(model, res.primal, res.primal_report, 1e-6,
                                  "minlp");
}
} // namespace

int main() {
    try {
        split_semantics_at_chosen_points();
        engine_nlp_report_carries_the_measurement();
        engine_nlp_equality_verdict();
        engine_minlp_report_carries_the_measurement();
    } catch (const std::exception& exc) {
        std::cerr << "nlp_primal_report: FAILED: " << exc.what() << "\n";
        return 1;
    }
    std::cout << "nlp_primal_report: all cases passed\n";
    return 0;
}
