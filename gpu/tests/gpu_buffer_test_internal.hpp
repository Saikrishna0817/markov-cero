#pragma once
#include "markov_cero/gpu/buffer.hpp"
#include "markov_cero/gpu/csr.hpp"
#include "markov_cero/gpu/device.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/linalg/sparse_basis.hpp"
#include "markov_cero/model/model.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

namespace test_gpu_buffer_test {
namespace detail_gpu_buffer_test {}
namespace detail_gpu_buffer_test { void test_buffer_lifecycle(); }
namespace detail_gpu_buffer_test { void test_buffer_roundtrip_double(); }
namespace detail_gpu_buffer_test { void test_buffer_roundtrip_size_t(); }
namespace detail_gpu_buffer_test { void test_buffer_bounds_checking(); }
namespace detail_gpu_buffer_test { void test_csr_construction_roundtrip(); }
namespace detail_gpu_buffer_test { void test_model_sparse_matrix_roundtrip(); }
namespace detail_gpu_buffer_test { void test_refinery_mps_roundtrip(); }
namespace detail_gpu_buffer_test { void test_device_query(); }
}
