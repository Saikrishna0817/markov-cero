#pragma once
// markov-cero E2E Test Suite: Tier 2 - Milestone 1 Boundary & Corner Cases
// Tests boundary conditions, degeneracies, ill-conditioned matrices, zero dimensions,
// floating point extremes, and error paths for Milestone 1 features.

#include "markov_cero/api/solve.hpp"
#include "markov_cero/linalg/dense_lu.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/lp/first_order/pdlp.hpp"
#include "markov_cero/lp/interior/ipm.hpp"
#include "markov_cero/model/model.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/kkt.hpp"
#include "markov_cero/qp/verifier.hpp"
#include "markov_cero/transform/canonicalize.hpp"
#include "markov_cero/verify/primal_verifier.hpp"

#include "e2e_test_framework.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace markov_cero;
using namespace markov_cero::testing;

// ---------------------------------------------------------------------------
// Feature 1: SparseLU IPM Normal Equations Boundaries
// ---------------------------------------------------------------------------
