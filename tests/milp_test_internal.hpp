#pragma once
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

namespace test_milp_test {
namespace detail_milp_test {}
namespace detail_milp_test { void test_knapsack_01(); }
namespace detail_milp_test { void test_sparse_pdlp_root_relaxation_above_dense_row_limit(); }
namespace detail_milp_test { void test_maximization_objective_and_bound_signs(); }
namespace detail_milp_test { void test_refinery_discrete_dispatch(); }
namespace detail_milp_test { void test_infeasible_milp(); }
void test_node_selection_policies();
void test_pseudo_cost_branching();
}
