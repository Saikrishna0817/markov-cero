#include "markov_cero/api/solve.hpp"
#include "markov_cero/milp/parallel_tree_search.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/verify/mip_proof.hpp"

#include <cmath>
#include <stdexcept>

using namespace markov_cero;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

model::Model knapsack() {
    model::Model instance;
    instance.name = "PARALLEL_PROOF_KNAPSACK";
    instance.objective = {-10.0, -14.0, -12.0};
    model::SparseMatrixBuilder builder(1, 3);
    builder.add(0, 0, 4.0);
    builder.add(0, 1, 6.0);
    builder.add(0, 2, 5.0);
    instance.matrix = builder.build();
    instance.row_lower = {model::Bound::negative_infinity()};
    instance.row_upper = {model::Bound::finite(10.0)};
    instance.row_name = {"CAPACITY"};
    instance.variable_name = {"X1", "X2", "X3"};
    instance.variable_lower.assign(3, model::Bound::finite(0.0));
    instance.variable_upper.assign(3, model::Bound::finite(1.0));
    instance.variable_type.assign(3, model::VariableType::binary);
    instance.validate();
    return instance;
}

model::Model tiny_integral_root() {
    model::Model instance;
    instance.name = "PARALLEL_PROOF_INTEGRAL_ROOT";
    instance.objective = {-1.0};
    model::SparseMatrixBuilder builder(1, 1);
    builder.add(0, 0, 1.0);
    instance.matrix = builder.build();
    instance.row_lower = {model::Bound::negative_infinity()};
    instance.row_upper = {model::Bound::finite(1.0)};
    instance.row_name = {"CAP"};
    instance.variable_name = {"Y1"};
    instance.variable_lower = {model::Bound::finite(0.0)};
    instance.variable_upper = {model::Bound::finite(1.0)};
    instance.variable_type = {model::VariableType::binary};
    instance.validate();
    return instance;
}

int main() {
    const auto model_with_cuts = knapsack();
    milp::ParallelOptions parallel;
    parallel.num_threads = 2;
    const auto direct = milp::solve_parallel(model_with_cuts, parallel);
    require(direct.status == lp::reference::SolveStatus::optimal,
            "parallel knapsack must solve to optimality");
    require(std::abs(direct.objective + 24.0) < 1e-6,
            "parallel knapsack integer optimum");
    require(!direct.obligations.empty(), "parallel result must contain optimizer events");

    api::SolveOptions options;
    options.engine = "parallel";
    options.num_threads = 2;
    options.enable_mip_proof = true;
    const auto out = api::solve_model(model_with_cuts, options);
    require(out.status == lp::reference::SolveStatus::optimal,
            "API parallel knapsack must solve to optimality");
    require(out.proof_status == "accepted" && out.canonical_verified,
            "parallel knapsack proof must be accepted");
    require(out.mip_proof && !out.mip_proof->obligations.empty(),
            "parallel proof must carry optimizer events");
    require(out.mip_proof->obligations[0].kind == verify::MipObligationKind::cut ||
            out.mip_proof->obligations[0].kind == verify::MipObligationKind::propagation,
            "parallel proof event kind");

    const auto no_cut = api::solve_model(tiny_integral_root(), options);
    require(no_cut.status == lp::reference::SolveStatus::optimal,
            "integral-root MILP must solve to optimality");
    require(no_cut.proof_status == "accepted" && no_cut.canonical_verified,
            "integral-root proof must be accepted");
    require(no_cut.mip_proof && no_cut.mip_proof->obligations.empty(),
            "integral-root proof needs no optimizer annotations");

    options.mip_proof_max_nodes = 1;
    const auto exhausted = api::solve_model(model_with_cuts, options);
    require(exhausted.proof_status == "exhausted" && exhausted.proof_budget_exhausted,
            "node-starved parallel proof must report exhaustion");
}
