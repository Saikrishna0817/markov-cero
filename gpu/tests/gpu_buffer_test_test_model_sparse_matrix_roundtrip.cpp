#include "gpu_buffer_test_internal.hpp"
namespace test_gpu_buffer_test {
using namespace detail_gpu_buffer_test;
namespace detail_gpu_buffer_test {
void test_model_sparse_matrix_roundtrip() {
    using namespace markov_cero;
    using namespace markov_cero::gpu;

    model::SparseMatrixBuilder builder(3, 3);
    builder.add(0, 0, 11.0);
    builder.add(0, 2, 13.0);
    builder.add(1, 1, 22.0);
    builder.add(2, 0, 31.0);
    builder.add(2, 1, 32.0);
    builder.add(2, 2, 33.0);

    model::SparseMatrixCSC orig = builder.build();
    DeviceCsr dev_csr = DeviceCsr::from_csc(orig);

    assert(dev_csr.rows() == 3);
    assert(dev_csr.cols() == 3);
    assert(dev_csr.nnz() == 6);

    model::SparseMatrixCSC rt = dev_csr.to_model_csc_host();
    assert(rt.row_count == orig.row_count);
    assert(rt.column_count == orig.column_count);
    assert(rt.column_start == orig.column_start);
    assert(rt.row_index == orig.row_index);
    int val_cmp = std::memcmp(rt.value.data(), orig.value.data(),
                              orig.value.size() * sizeof(double));
    assert(val_cmp == 0);

    std::cout << "test_model_sparse_matrix_roundtrip: PASS\n";
}
}

namespace detail_gpu_buffer_test {
void test_refinery_mps_roundtrip() {
    using namespace markov_cero;
    using namespace markov_cero::gpu;

    const std::string path = "examples/refinery/refinery-feasible.mps";
    std::ifstream file(path);
    model::Model mdl;
    if (file.is_open()) {
        mdl = io::parse_mps(file);
    } else {
        std::ifstream alt_file("../" + path);
        assert(alt_file.is_open());
        mdl = io::parse_mps(alt_file);
    }

    DeviceCsr dev_csr = DeviceCsr::from_csc(mdl.matrix);
    assert(dev_csr.rows() == mdl.matrix.row_count);
    assert(dev_csr.cols() == mdl.matrix.column_count);
    assert(dev_csr.nnz() == mdl.matrix.value.size());

    model::SparseMatrixCSC rt = dev_csr.to_model_csc_host();
    assert(rt.row_count == mdl.matrix.row_count);
    assert(rt.column_count == mdl.matrix.column_count);
    assert(rt.column_start == mdl.matrix.column_start);
    assert(rt.row_index == mdl.matrix.row_index);

    int val_cmp = std::memcmp(rt.value.data(), mdl.matrix.value.data(),
                              mdl.matrix.value.size() * sizeof(double));
    assert(val_cmp == 0);

    std::cout << "test_refinery_mps_roundtrip: PASS ("
              << dev_csr.rows() << "x" << dev_csr.cols() << ", "
              << dev_csr.nnz() << " nonzeros)\n";
}
}

namespace detail_gpu_buffer_test {
void test_device_query() {
    using namespace markov_cero::gpu;

    bool available = is_gpu_available();
    int count = get_device_count();
    DeviceInfo info = get_device_info(0);

    std::cout << "test_device_query: available=" << std::boolalpha << available
              << ", count=" << count << ", device_name='" << info.name << "'\n";

    synchronize_device();
    std::cout << "test_device_query: PASS\n";
}
}

}
