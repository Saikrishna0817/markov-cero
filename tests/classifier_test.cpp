#include "markov_cero/model/classifier.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace markov_cero::model;

namespace {

Model minimal_model(std::size_t vars = 2, bool integer_vars = false, bool quadratic = false) {
    Model m;
    m.name = "classifier_probe";
    m.objective_sense = ObjectiveSense::minimize;
    SparseMatrixBuilder builder(1, vars);
    for (std::size_t j = 0; j < vars; ++j) {
        builder.add(0, j, 1.0);
    }
    m.matrix = builder.build();
    m.objective.assign(vars, 1.0);
    m.row_lower.push_back(Bound::finite(0.0));
    m.row_upper.push_back(Bound::positive_infinity());
    m.variable_lower.assign(vars, Bound::finite(0.0));
    m.variable_upper.assign(vars, Bound::positive_infinity());
    m.variable_type.assign(vars, integer_vars ? VariableType::integer : VariableType::continuous);
    m.variable_name.resize(vars);
    for (std::size_t j = 0; j < vars; ++j) {
        m.variable_name[j] = "x" + std::to_string(j);
    }
    m.row_name.push_back("c1");
    if (quadratic) {
        m.has_quadratic_objective = true;
        SparseMatrixBuilder qbuilder(vars, vars);
        for (std::size_t j = 0; j < vars; ++j) {
            qbuilder.add(j, j, 2.0);
        }
        m.quadratic_matrix = qbuilder.build();
    }
    m.validate();
    return m;
}

void expect_class(ProblemClass got, ProblemClass want, const char* what) {
    if (got != want) {
        std::cerr << "FAIL: " << what << " expected " << to_string(want) << " got "
                  << to_string(got) << "\n";
        std::exit(1);
    }
}

} // namespace

int main() {
    // Path 1: NLP callbacks + continuous -> NLP
    {
        auto m = minimal_model();
        ClassificationInputs in;
        in.has_nlp_callbacks = true;
        const auto r = classify_model(m, in);
        expect_class(r.problem_class, ProblemClass::nlp, "callbacks continuous");
        if (r.default_engine != "sqp") { std::cerr << "FAIL: nlp engine\n"; return 1; }
    }
    // Path 2: NLP callbacks + integer -> MINLP
    {
        auto m = minimal_model(2, true, false);
        ClassificationInputs in;
        in.has_nlp_callbacks = true;
        const auto r = classify_model(m, in);
        expect_class(r.problem_class, ProblemClass::minlp, "callbacks integer");
        if (r.default_engine != "outer_approx") { std::cerr << "FAIL: minlp engine\n"; return 1; }
    }
    // Path 3: NLOBJ section + continuous -> NLP
    {
        auto m = minimal_model();
        ClassificationInputs in;
        in.has_nlobj_section = true;
        const auto r = classify_model(m, in);
        expect_class(r.problem_class, ProblemClass::nlp, "nlobj continuous");
    }
    // Path 4: NLOBJ section + integer -> MINLP
    {
        auto m = minimal_model(2, true, false);
        ClassificationInputs in;
        in.has_nlobj_section = true;
        const auto r = classify_model(m, in);
        expect_class(r.problem_class, ProblemClass::minlp, "nlobj integer");
    }
    // Path 5: quadratic + continuous -> QP
    {
        auto m = minimal_model(2, false, true);
        const auto r = classify_model(m, {});
        expect_class(r.problem_class, ProblemClass::qp, "quadratic continuous");
        if (r.default_engine != "qp") { std::cerr << "FAIL: qp engine\n"; return 1; }
    }
    // Path 6: quadratic + integer -> MIQP
    {
        auto m = minimal_model(2, true, true);
        const auto r = classify_model(m, {});
        expect_class(r.problem_class, ProblemClass::miqp, "quadratic integer");
        if (r.default_engine != "miqp") { std::cerr << "FAIL: miqp engine\n"; return 1; }
    }
    // Path 7a: integer, linear -> MILP
    {
        auto m = minimal_model(2, true, false);
        const auto r = classify_model(m, {});
        expect_class(r.problem_class, ProblemClass::milp, "integer linear");
        if (r.default_engine != "milp") { std::cerr << "FAIL: milp engine\n"; return 1; }
    }
    // Path 7b: continuous, linear -> LP
    {
        auto m = minimal_model();
        const auto r = classify_model(m, {});
        expect_class(r.problem_class, ProblemClass::lp, "continuous linear");
    }
    // LOCKED threshold: LP stays primal at exactly 50,000 NNZ...
    {
        auto m = minimal_model();
        ClassificationStats stats = classify_stats(m);
        stats.nonzeros = EngineThresholds::kLpPdlpNnzThreshold;
        if (select_engine(ProblemClass::lp, stats, false) != "primal") {
            std::cerr << "FAIL: threshold boundary must keep primal at exactly 50000\n";
            return 1;
        }
        // ...and dispatches to pdlp strictly above it.
        stats.nonzeros = EngineThresholds::kLpPdlpNnzThreshold + 1;
        if (select_engine(ProblemClass::lp, stats, false) != "pdlp") {
            std::cerr << "FAIL: above threshold must dispatch pdlp\n";
            return 1;
        }
    }
    // LOCKED threshold: QP GPU path only above 100,000 quadratic NNZ AND explicit backend.
    {
        auto m = minimal_model(2, false, true);
        ClassificationStats stats = classify_stats(m);
        stats.quadratic_nonzeros = EngineThresholds::kQpGpuNnzThreshold + 1;
        // GPU requested but quadratic NNZ below threshold -> CPU ADMM
        stats.quadratic_nonzeros = 10;
        if (select_engine(ProblemClass::qp, stats, true) != "qp") {
            std::cerr << "FAIL: small QP must stay on CPU ADMM\n";
            return 1;
        }
        // GPU requested above threshold -> still engine "qp"; backend resolution
        // (GPU kernel vs CPU) is enforced inside the QP engine per plan W3.
        stats.quadratic_nonzeros = EngineThresholds::kQpGpuNnzThreshold + 1;
        if (select_engine(ProblemClass::qp, stats, true) != "qp") {
            std::cerr << "FAIL: large QP engine label\n";
            return 1;
        }
    }
    // MILP never auto-dispatches to parallel; the explicit --threads > 1 request
    // is honored at the CLI/API layer, not by the classifier.
    {
        auto m = minimal_model(2, true, false);
        ClassificationStats stats = classify_stats(m);
        if (select_engine(ProblemClass::milp, stats, false) != "milp") {
            std::cerr << "FAIL: milp default engine\n";
            return 1;
        }
    }

    std::cout << "classifier tests passed\n";
    return 0;
}
