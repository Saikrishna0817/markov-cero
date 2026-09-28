#pragma once
#include "markov_cero/milp/parallel_tree_search.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace test_parallel_tree_search_test {
namespace detail_parallel_tree_search_test {}
namespace detail_parallel_tree_search_test { markov_cero::model::Model build_knapsack_model(); }
namespace detail_parallel_tree_search_test { markov_cero::model::Model build_refinery_dispatch_model(); }
namespace detail_parallel_tree_search_test { markov_cero::model::Model build_infeasible_milp_model(); }
namespace detail_parallel_tree_search_test { void test_knapsack_multi_threads(); }
namespace detail_parallel_tree_search_test { void test_refinery_dispatch_multi_threads(); }
namespace detail_parallel_tree_search_test { void test_thread_safety_repeated_runs(); }
namespace detail_parallel_tree_search_test { void test_infeasible_parallel(); }
namespace detail_parallel_tree_search_test { void test_thread_safe_queue_unit(); }
namespace detail_parallel_tree_search_test { void test_queue_lazy_prune_batch(); }
namespace detail_parallel_tree_search_test { void test_queue_batch_interleave_order(); }
namespace detail_parallel_tree_search_test { void test_incumbent_manager_unit(); }
}
