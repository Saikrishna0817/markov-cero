#pragma once
#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/milp/cuts.hpp"
#include "markov_cero/milp/strong_branching.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

namespace test_strong_branching_test {
namespace detail_strong_branching_test {}
namespace detail_strong_branching_test { void test_mir_mathematical_function(); }
namespace detail_strong_branching_test { void test_cut_efficacy_and_filtering(); }
namespace detail_strong_branching_test { void test_mir_cuts_tighten_relaxation_and_preserve_integers(); }
namespace detail_strong_branching_test { void test_strong_branching_and_domain_reduction(); }
namespace detail_strong_branching_test { void test_zero_trust_primal_verifier_integration(); }
}
