#include "equivalence_test_internal.hpp"
namespace test_equivalence_test {
using namespace detail_equivalence_test;
namespace detail_equivalence_test {
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}
}

namespace detail_equivalence_test {
double compute_max_abs_diff(const std::vector<double>& a, const std::vector<double>& b) {
    require(a.size() == b.size(), "vector sizes must match for abs diff computation");
    double max_diff = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        max_diff = std::max(max_diff, std::abs(a[i] - b[i]));
    }
    return max_diff;
}
}

namespace detail_equivalence_test {
double compute_max_rel_diff(const std::vector<double>& a, const std::vector<double>& b) {
    require(a.size() == b.size(), "vector sizes must match for rel diff computation");
    double max_rel = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        double abs_diff = std::abs(a[i] - b[i]);
        double scale = 1.0 + std::max(std::abs(a[i]), std::abs(b[i]));
        max_rel = std::max(max_rel, abs_diff / scale);
    }
    return max_rel;
}
}

namespace detail_equivalence_test {
void test_synthetic_spmv() {
    using namespace markov_cero;
    using namespace markov_cero::gpu;

    // 1. Empty matrix
    DeviceCsr empty_csr(0, 0, 0);
    DeviceBuffer<double> x_empty(0);
    DeviceBuffer<double> y_empty(0);
    spmv(empty_csr, x_empty, y_empty);

    // 2. 1x1 matrix: [2.5]
    model::SparseMatrixBuilder b1(1, 1);
    b1.add(0, 0, 2.5);
    DeviceCsr csr1 = DeviceCsr::from_csc(b1.build());
    DeviceBuffer<double> x1(std::vector<double>{4.0});
    DeviceBuffer<double> y1(1);
    spmv(csr1, x1, y1);
    std::vector<double> h_y1 = y1.to_vector();
    require(std::abs(h_y1[0] - 10.0) <= 1e-15, "1x1 spmv result mismatch");

    // 3. 3x3 diagonal matrix: diag(1.0, -2.0, 3.0)
    model::SparseMatrixBuilder b3(3, 3);
    b3.add(0, 0, 1.0);
    b3.add(1, 1, -2.0);
    b3.add(2, 2, 3.0);
    DeviceCsr csr3 = DeviceCsr::from_csc(b3.build());
    DeviceBuffer<double> x3(std::vector<double>{2.0, 3.0, 4.0});
    DeviceBuffer<double> y3(3);
    spmv(csr3, x3, y3);
    std::vector<double> h_y3 = y3.to_vector();
    require(std::abs(h_y3[0] - 2.0) <= 1e-15, "3x3 diagonal [0]");
    require(std::abs(h_y3[1] - (-6.0)) <= 1e-15, "3x3 diagonal [1]");
    require(std::abs(h_y3[2] - 12.0) <= 1e-15, "3x3 diagonal [2]");

    // 4. Dimension mismatch checks
    bool caught_col_mismatch = false;
    try {
        DeviceBuffer<double> x_wrong(4);
        spmv(csr3, x_wrong, y3);
    } catch (const std::invalid_argument&) {
        caught_col_mismatch = true;
    }
    require(caught_col_mismatch, "failed to catch column mismatch");

    bool caught_row_mismatch = false;
    try {
        DeviceBuffer<double> y_wrong(2);
        spmv(csr3, x3, y_wrong);
    } catch (const std::invalid_argument&) {
        caught_row_mismatch = true;
    }
    require(caught_row_mismatch, "failed to catch row mismatch");

    std::cout << "test_synthetic_spmv: PASS\n";
}
}

namespace detail_equivalence_test {
void test_synthetic_spmv_transpose() {
    using namespace markov_cero;
    using namespace markov_cero::gpu;

    model::SparseMatrixBuilder builder(2, 3);
    builder.add(0, 0, 1.0);
    builder.add(0, 2, 3.0);
    builder.add(1, 1, 2.0);
    builder.add(1, 2, 4.0);
    model::SparseMatrixCSC mat = builder.build();

    DeviceCsr At = DeviceCsr::transpose_from_csc(mat);
    require(At.rows() == 3, "At.rows must be 3");
    require(At.cols() == 2, "At.cols must be 2");
    require(At.nnz() == 4, "At.nnz must be 4");

    DeviceBuffer<double> d_y(std::vector<double>{2.0, -1.0});
    DeviceBuffer<double> d_z(3);
    spmv_transpose(At, d_y, d_z);

    std::vector<double> h_z = d_z.to_vector();
    require(std::abs(h_z[0] - 2.0) <= 1e-15, "transpose [0]");
    require(std::abs(h_z[1] - (-2.0)) <= 1e-15, "transpose [1]");
    require(std::abs(h_z[2] - 2.0) <= 1e-15, "transpose [2]");

    bool caught_y_mismatch = false;
    try {
        DeviceBuffer<double> d_y_wrong(3);
        spmv_transpose(At, d_y_wrong, d_z);
    } catch (const std::invalid_argument&) {
        caught_y_mismatch = true;
    }
    require(caught_y_mismatch, "failed to catch transpose y mismatch");

    std::cout << "test_synthetic_spmv_transpose: PASS\n";
}
}

namespace detail_equivalence_test {
void test_axpy_equivalence() {
    using namespace markov_cero::gpu;

    const std::vector<std::size_t> sizes = {0, 1, 17, 256, 10000};
    const std::vector<double> alphas = {0.0, 1.0, -1.0, 2.71828, -1.5e-4};

    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> dist(-100.0, 100.0);

    for (std::size_t n : sizes) {
        std::vector<double> h_x(n);
        std::vector<double> h_y(n);
        for (std::size_t i = 0; i < n; ++i) {
            h_x[i] = dist(rng);
            h_y[i] = dist(rng);
        }

        for (double alpha : alphas) {
            DeviceBuffer<double> d_x(h_x);
            DeviceBuffer<double> d_y(h_y);
            DeviceBuffer<double> d_y_cpu(h_y);

            axpy(alpha, d_x, d_y);
            axpy_cpu(alpha, d_x, d_y_cpu);

            std::vector<double> actual = d_y.to_vector();
            std::vector<double> expected = d_y_cpu.to_vector();

            double diff = compute_max_abs_diff(actual, expected);
            require(diff <= 1e-12, "axpy diff exceeds 1e-12");
        }
    }

    bool caught_mismatch = false;
    try {
        DeviceBuffer<double> d_x(10);
        DeviceBuffer<double> d_y(15);
        axpy(2.0, d_x, d_y);
    } catch (const std::invalid_argument&) {
        caught_mismatch = true;
    }
    require(caught_mismatch, "failed to catch axpy size mismatch");

    std::cout << "test_axpy_equivalence: PASS\n";
}
}

namespace detail_equivalence_test {
void test_scale_equivalence() {
    using namespace markov_cero::gpu;

    const std::vector<std::size_t> sizes = {0, 1, 33, 1024, 25000};
    const std::vector<double> alphas = {0.0, -2.5, 0.5, 3.14159265, 1e6};

    std::mt19937_64 rng(54321);
    std::uniform_real_distribution<double> dist(-50.0, 50.0);

    for (std::size_t n : sizes) {
        std::vector<double> h_x(n);
        for (std::size_t i = 0; i < n; ++i) {
            h_x[i] = dist(rng);
        }

        for (double alpha : alphas) {
            DeviceBuffer<double> d_x(h_x);
            DeviceBuffer<double> d_x_cpu(h_x);

            scale(alpha, d_x);
            scale_cpu(alpha, d_x_cpu);

            std::vector<double> actual = d_x.to_vector();
            std::vector<double> expected = d_x_cpu.to_vector();

            double diff = compute_max_abs_diff(actual, expected);
            require(diff <= 1e-12, "scale diff exceeds 1e-12");
        }
    }

    std::cout << "test_scale_equivalence: PASS\n";
}
}

}
using namespace test_equivalence_test;
using namespace test_equivalence_test::detail_equivalence_test;
int main() {
    std::cout << "=== Markov-Cero Kernel Equivalence Tests (T-5.03 - T-5.05) ===\n";
    test_synthetic_spmv();
    test_synthetic_spmv_transpose();
    test_axpy_equivalence();
    test_scale_equivalence();
    test_project_bounds_equivalence();
    test_all_netlib_matrices();
    std::cout << "=== All Kernel Equivalence Tests Passed Successfully ===\n";
    return 0;
}
