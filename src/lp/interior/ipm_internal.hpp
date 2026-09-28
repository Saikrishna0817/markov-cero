#pragma once
#include "markov_cero/lp/interior/ipm.hpp"

// D-14 (LOCKED): the normal-equation Newton system A D A^T dy = rhs is
// factorized with the sparse Markowitz LU below; the former dense LU path is
// gone (it broke down around m = 200 rows).
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/scale/ruiz_scaling.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace markov_cero::lp::interior {
namespace detail_ipm {}
namespace detail_ipm {
using linalg::SparseCsc;
}
namespace detail_ipm {
using linalg::SparseLu;
}
namespace detail_ipm {
using linalg::SparseLuSymbolicAnalysis;
}
namespace detail_ipm {
struct Direction {
    std::vector<double> dx, dy, ds;
};
}
namespace detail_ipm { SparseCsc transpose_csc(const SparseCsc& A); }
namespace detail_ipm { std::vector<double> spmv(const SparseCsc& A, const std::vector<double>& x); }
namespace detail_ipm { std::vector<double> spmv_t(const SparseCsc& A, const std::vector<double>& y); }
namespace detail_ipm { SparseCsc compute_sparse_normal_matrix(
    const SparseCsc& A,
    const SparseCsc& At,
    const std::vector<double>& d,
    double delta = 1e-12); }
namespace detail_ipm { Direction newton_direction_sparse(
    const SparseCsc& a,
    const SparseCsc& M,
    const std::vector<double>& x,
    const std::vector<double>& s,
    const std::vector<double>& d,
    const std::vector<double>& r_p,
    const std::vector<double>& r_d,
    const std::vector<double>& tau,
    const SparseLu& factor); }
namespace detail_ipm { double max_step(const std::vector<double>& v, const std::vector<double>& dv,
                double fraction_to_boundary); }
namespace detail_ipm { std::optional<std::vector<std::size_t>> crossover_basis_sparse(
    const SparseCsc& a, const std::vector<double>& x_ipm); }
Result solve(const transform::SparseCanonicalModel& model, const Options& options);
Result solve(const transform::CanonicalModel& model, const Options& options);
}
