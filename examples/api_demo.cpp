#include "markov_cero/api/solve.hpp"
#include "markov_cero/model/model.hpp"

#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== markov-cero Library API Demonstration ===\n\n";

    // 1. Solve LP instance from file using solve_file()
    std::cout << "[1] Solving examples/blend.mps via API...\n";
    markov_cero::api::SolveOptions lp_opts;
    lp_opts.engine = "auto";
    const auto lp_res = markov_cero::api::solve_file("examples/blend.mps", lp_opts);
    std::cout << "    Status:   " << markov_cero::lp::reference::to_string(lp_res.status) << "\n";
    std::cout << "    Verified: " << (lp_res.verified ? "YES" : "NO") << "\n";
    std::cout << "    Obj:      " << std::fixed << std::setprecision(6) << lp_res.objective << "\n\n";

    // 2. Solve QP instance from file
    std::cout << "[2] Solving examples/cases/crude_oil_blending.mps via API...\n";
    markov_cero::api::SolveOptions qp_opts;
    qp_opts.engine = "qp";
    const auto qp_res = markov_cero::api::solve_file("examples/cases/crude_oil_blending.mps", qp_opts);
    std::cout << "    Status:   " << markov_cero::lp::reference::to_string(qp_res.status) << "\n";
    std::cout << "    Verified: " << (qp_res.verified ? "YES" : "NO") << "\n";
    std::cout << "    Obj:      " << std::fixed << std::setprecision(6) << qp_res.objective << "\n\n";

    // 3. Programmatic Model Construction & Solve via solve_model()
    std::cout << "[3] Building and solving in-memory model via API...\n";
    // min  -2*x1 - 3*x2
    // s.t.  x1 + 2*x2 <= 8
    //       2*x1 + x2 <= 10
    //       x1, x2 >= 0
    markov_cero::model::Model model;
    model.name = "in_memory_demo";
    model.objective_sense = markov_cero::model::ObjectiveSense::minimize;
    model.objective = {-2.0, -3.0};
    model.variable_name = {"x1", "x2"};
    model.variable_lower = {markov_cero::model::Bound::finite(0.0), markov_cero::model::Bound::finite(0.0)};
    model.variable_upper = {markov_cero::model::Bound::positive_infinity(), markov_cero::model::Bound::positive_infinity()};
    model.variable_type = {markov_cero::model::VariableType::continuous, markov_cero::model::VariableType::continuous};

    model.row_name = {"c1", "c2"};
    model.row_lower = {markov_cero::model::Bound::negative_infinity(), markov_cero::model::Bound::negative_infinity()};
    model.row_upper = {markov_cero::model::Bound::finite(8.0), markov_cero::model::Bound::finite(10.0)};

    markov_cero::model::SparseMatrixBuilder mb(2, 2);
    mb.add(0, 0, 1.0);
    mb.add(0, 1, 2.0);
    mb.add(1, 0, 2.0);
    mb.add(1, 1, 1.0);
    model.matrix = mb.build();
    model.validate();

    const auto prog_res = markov_cero::api::solve_model(model);
    std::cout << "    Status:   " << markov_cero::lp::reference::to_string(prog_res.status) << "\n";
    std::cout << "    Verified: " << (prog_res.verified ? "YES" : "NO") << "\n";
    std::cout << "    Obj:      " << std::fixed << std::setprecision(6) << prog_res.objective << " (expected -14.0)\n";
    if (!prog_res.primal.empty()) {
        std::cout << "    x1 = " << prog_res.primal[0] << ", x2 = " << prog_res.primal[1] << "\n\n";
    }

    std::cout << "All API demonstrations completed successfully.\n";
    return (lp_res.verified && qp_res.verified && prog_res.verified) ? 0 : 1;
}
