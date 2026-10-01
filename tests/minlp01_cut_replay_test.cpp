// MINLP-01 (docs/contracts/minlp-oa.md §10 cases H + random validation):
// independent cut replay rejects every corruption of a stored tangent, the
// solve-time provenance list survives seeded feasible-point sampling, and
// malformed source terms fail closed at entry.
#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/io/nlobj_parser.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    double out = 0.0;
    for (std::size_t j = 0; j < a.size(); ++j) out += a[j] * b[j];
    return out;
}

void require_rejected(const model::Model& source, const minlp::OaCut& cut,
                      const std::string& needle) {
    const minlp::OaReplayReport report = minlp::replay_oa_cuts(source, {cut});
    const std::string message =
        "corrupted tangent was accepted (expected rejection containing '" + needle + "')";
    req(!report.accepted, message.c_str());
    const std::string detail = "rejection message does not name the corrupted field: " +
                               report.message;
    req(report.message.find(needle) != std::string::npos, detail.c_str());
}

// Case A source (minlp-oa.md §10 case A) for corruption and sampling tests.
model::Model case_a_source() {
    model::Model source;
    source.name = "minlp01_replay_case_a";
    source.matrix = model::SparseMatrixBuilder(0, 2).build();
    source.objective = {-2.0, 0.2};
    source.objective_offset = 1.0;
    source.variable_name = {"x", "y"};
    source.variable_lower = {model::Bound::finite(0.0), model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(2.0), model::Bound::finite(1.0)};
    source.variable_type = {model::VariableType::continuous, model::VariableType::integer};
    source.has_nlobj_section = true;
    source.nlobj_terms = {{1.0, 0, 0, true}};
    source.nlcon_constraints = {
        {"x2_le_y", 0.0, {{1.0, 0, 0, true}, {-1.0, 1, 0, false}}}};
    source.validate();
    return source;
}

// Banded single-row source: 0 <= x <= 2 so both linear cut kinds exist.
model::Model band_source() {
    model::Model source;
    source.name = "minlp01_replay_band";
    model::SparseMatrixBuilder rows(1, 1);
    rows.add(0, 0, 1.0);
    source.matrix = rows.build();
    source.objective = {0.0};
    source.row_name = {"band"};
    source.row_lower = {model::Bound::finite(0.0)};
    source.row_upper = {model::Bound::finite(2.0)};
    source.variable_name = {"x"};
    source.variable_lower = {model::Bound::finite(0.0)};
    source.variable_upper = {model::Bound::finite(3.0)};
    source.variable_type = {model::VariableType::continuous};
    source.validate();
    return source;
}

} // namespace

int main() {
    const model::Model source = case_a_source();
    const std::vector<double> point = {0.5, 1.0};

    // ---- Case H: valid cuts replay; every corruption is rejected. ----
    const auto obj_cut = minlp::derive_oa_cut(
        source, minlp::OaCutSource::objective, minlp::kOaObjectiveSource, point);
    const auto nlcon_cut = minlp::derive_oa_cut(source, minlp::OaCutSource::nlcon, 0, point);
    const auto band = band_source();
    const auto upper_cut = minlp::derive_oa_cut(band, minlp::OaCutSource::linear_upper, 0, {1.5});
    const auto lower_cut = minlp::derive_oa_cut(band, minlp::OaCutSource::linear_lower, 0, {1.5});
    const auto valid = minlp::replay_oa_cuts(source, {obj_cut, nlcon_cut});
    req(valid.accepted && valid.checked == 2, "valid cuts replay and are counted");
    const auto valid_linear = minlp::replay_oa_cuts(band, {upper_cut, lower_cut});
    req(valid_linear.accepted && valid_linear.checked == 2, "linear cuts replay");

    {
        auto cut = obj_cut;
        cut.gradient[0] += 0.1;
        require_rejected(source, cut, "gradient");
    }
    {
        auto cut = obj_cut;
        cut.value += 0.1;
        require_rejected(source, cut, "value");
    }
    {
        auto cut = nlcon_cut;
        cut.rhs += 0.1;
        require_rejected(source, cut, "rhs");
    }
    {
        auto cut = nlcon_cut;
        cut.weakening = -1e-3;
        require_rejected(source, cut, "weakening");
    }
    {
        auto cut = obj_cut;
        cut.source_maximize = !cut.source_maximize;
        require_rejected(source, cut, "sense");
    }
    {
        auto cut = upper_cut;
        cut.gradient[0] = -cut.gradient[0];
        require_rejected(band, cut, "gradient");
    }
    {
        bool threw = false;
        try {
            (void)minlp::derive_oa_cut(source, minlp::OaCutSource::nlcon, 7, point);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        req(threw, "derive_oa_cut fails closed on an unknown source row");
    }
    {
        bool threw = false;
        try {
            auto lower_only = band;
            lower_only.row_upper[0] = model::Bound::positive_infinity();
            (void)minlp::derive_oa_cut(lower_only, minlp::OaCutSource::linear_upper, 0, {1.5});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        req(threw, "derive_oa_cut fails closed when the addressed row has no such bound");
    }

    // ---- Seeded feasible-point validation of the solve's own cuts. ----
    minlp::MinlpOptions opts;
    minlp::MinlpProblem problem;
    problem.source_model = source;
    problem.nlp = io::make_nlp_model(source);
    problem.integer_indices = {1};
    const auto sol = minlp::solve_minlp(problem, {0.0, 0.0}, opts);
    req(sol.status == lp::reference::SolveStatus::optimal && !sol.oa_cuts.empty(),
        "sampling test solves case A and exposes its provenance list");

    std::mt19937 gen(42);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::vector<std::vector<double>> samples = {{0.0, 0.0}};
    for (int k = 0; k < 48; ++k) samples.push_back({unit(gen), 1.0});

    for (const auto& cut : sol.oa_cuts) {
        for (const auto& at : samples) {
            // Samples are feasible for x^2 <= y (x in [0,1] with y=1).
            const double actual =
                minlp::derive_oa_cut(source, cut.source_kind, cut.source_index, at).value;
            if (cut.source_kind == minlp::OaCutSource::nlcon) {
                const std::string infeasible =
                    "random sample is not feasible for the source constraint: " +
                    std::to_string(actual);
                req(actual <= 1e-9, infeasible.c_str());
            }
            const double row_value = dot(cut.gradient, at) - cut.rhs;
            const std::string message =
                "stored tangent excluded or under-estimated a random feasible point (row " +
                std::to_string(row_value) + " vs actual " + std::to_string(actual) + ")";
            req(row_value <= actual + 1e-6, message.c_str());
        }
    }

    // ---- Malformed terms fail closed at entry (contract §2.2). ----
    model::Model oob = source;
    oob.nlcon_constraints[0].terms[1].var0 = 5;
    minlp::MinlpProblem oob_problem;
    oob_problem.source_model = oob;
    oob_problem.nlp = io::make_nlp_model(source); // rebuilt from `oob` at entry
    oob_problem.integer_indices = {1};
    const auto oob_sol = minlp::solve_minlp(oob_problem, {0.0, 0.0}, opts);
    req(oob_sol.status == lp::reference::SolveStatus::invalid_model,
        "out-of-range NLCON term index is invalid_model");

    model::Model nan_source = source;
    nan_source.nlobj_terms[0].coefficient = std::nan("");
    minlp::MinlpProblem nan_problem;
    nan_problem.source_model = nan_source;
    nan_problem.integer_indices = {1};
    const auto nan_sol = minlp::solve_minlp(nan_problem, {0.0, 0.0}, opts);
    req(nan_sol.status == lp::reference::SolveStatus::invalid_model,
        "non-finite NLOBJ coefficient is invalid_model");

    std::cout << "minlp01 cut replay tests passed\n";
    return 0;
}
