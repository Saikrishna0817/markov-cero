#pragma once
#include "markov_cero/gpu/buffer.hpp"
#include "markov_cero/gpu/csr.hpp"
#include "markov_cero/gpu/kernels.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/model/model.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace test_equivalence_test {
namespace detail_equivalence_test {}
namespace detail_equivalence_test { void require(bool condition, const std::string& message); }
namespace detail_equivalence_test { double compute_max_abs_diff(const std::vector<double>& a, const std::vector<double>& b); }
namespace detail_equivalence_test { double compute_max_rel_diff(const std::vector<double>& a, const std::vector<double>& b); }
namespace detail_equivalence_test { void test_synthetic_spmv(); }
namespace detail_equivalence_test { void test_synthetic_spmv_transpose(); }
namespace detail_equivalence_test { void test_axpy_equivalence(); }
namespace detail_equivalence_test { void test_scale_equivalence(); }
namespace detail_equivalence_test { void test_project_bounds_equivalence(); }
namespace detail_equivalence_test { std::vector<double> csc_multiply_transpose(const markov_cero::model::SparseMatrixCSC& mat,
                                           const std::vector<double>& y); }
namespace detail_equivalence_test { void test_netlib_instance(const std::string& instance_name,
                          const std::string& filepath,
                          std::size_t seed); }
namespace detail_equivalence_test { void test_all_netlib_matrices(); }
}
