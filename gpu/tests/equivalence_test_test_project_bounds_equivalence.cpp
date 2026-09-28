#include "equivalence_test_internal.hpp"
namespace test_equivalence_test {
using namespace detail_equivalence_test;
namespace detail_equivalence_test {
void test_project_bounds_equivalence() {
    using namespace markov_cero::gpu;

    constexpr std::size_t n = 2000;
    std::mt19937_64 rng(999);
    std::uniform_real_distribution<double> dist(-20.0, 20.0);

    std::vector<double> h_x(n);
    std::vector<double> h_lo(n);
    std::vector<double> h_hi(n);

    for (std::size_t i = 0; i < n; ++i) {
        h_x[i] = dist(rng);
        if (i < 400) {
            // Finite box [-2.0, 5.0]
            h_lo[i] = -2.0;
            h_hi[i] = 5.0;
        } else if (i < 800) {
            // Semi-infinite lower [0.0, +1e300]
            h_lo[i] = 0.0;
            h_hi[i] = 1e300;
        } else if (i < 1200) {
            // Semi-infinite upper [-1e300, 1.0]
            h_lo[i] = -1e300;
            h_hi[i] = 1.0;
        } else if (i < 1600) {
            // Equality bound [3.1415, 3.1415]
            h_lo[i] = 3.1415;
            h_hi[i] = 3.1415;
        } else {
            // Free [-1e300, +1e300]
            h_lo[i] = -1e300;
            h_hi[i] = 1e300;
        }
    }

    DeviceBuffer<double> d_x(h_x);
    DeviceBuffer<double> d_x_cpu(h_x);
    DeviceBuffer<double> d_lo(h_lo);
    DeviceBuffer<double> d_hi(h_hi);

    project_bounds(d_x, d_lo, d_hi);
    project_bounds_cpu(d_x_cpu, d_lo, d_hi);

    std::vector<double> actual = d_x.to_vector();
    std::vector<double> expected = d_x_cpu.to_vector();

    double diff = compute_max_abs_diff(actual, expected);
    require(diff <= 1e-12, "project_bounds diff exceeds 1e-12");

    // Check invariants
    for (std::size_t i = 0; i < n; ++i) {
        require(actual[i] >= h_lo[i], "actual[i] violates lower bound");
        require(actual[i] <= h_hi[i], "actual[i] violates upper bound");
    }

    bool caught_mismatch = false;
    try {
        DeviceBuffer<double> d_wrong_hi(n + 5);
        project_bounds(d_x, d_lo, d_wrong_hi);
    } catch (const std::invalid_argument&) {
        caught_mismatch = true;
    }
    require(caught_mismatch, "failed to catch project_bounds size mismatch");

    std::cout << "test_project_bounds_equivalence: PASS\n";
}
}

namespace detail_equivalence_test {
std::vector<double> csc_multiply_transpose(const markov_cero::model::SparseMatrixCSC& mat,
                                           const std::vector<double>& y) {
    std::vector<double> z(mat.column_count, 0.0);
    for (std::size_t col = 0; col < mat.column_count; ++col) {
        double sum = 0.0;
        const std::size_t start = mat.column_start[col];
        const std::size_t end = mat.column_start[col + 1];
        for (std::size_t p = start; p < end; ++p) {
            sum += mat.value[p] * y[mat.row_index[p]];
        }
        z[col] = sum;
    }
    return z;
}
}

namespace detail_equivalence_test {
void test_netlib_instance(const std::string& instance_name,
                          const std::string& filepath,
                          std::size_t seed) {
    using namespace markov_cero;
    using namespace markov_cero::gpu;

    std::ifstream file(filepath);
    model::Model mdl;
    if (file.is_open()) {
        mdl = io::parse_mps(file);
    } else {
        std::ifstream alt_file("../" + filepath);
        if (alt_file.is_open()) {
            mdl = io::parse_mps(alt_file);
        } else {
            const char* src_dir = std::getenv("MARKOV_CERO_SOURCE_DIR");
            if (src_dir) {
                std::ifstream env_file(std::string(src_dir) + "/" + filepath);
                if (env_file.is_open()) {
                    mdl = io::parse_mps(env_file);
                } else {
                    throw std::runtime_error("Cannot open Netlib file: " + filepath);
                }
            } else {
                throw std::runtime_error("Cannot open Netlib file: " + filepath);
            }
        }
    }

    const std::size_t m = mdl.matrix.row_count;
    const std::size_t n = mdl.matrix.column_count;
    const std::size_t nnz = mdl.matrix.value.size();

    DeviceCsr A = DeviceCsr::from_csc(mdl.matrix);
    require(A.rows() == m, "A.rows mismatch");
    require(A.cols() == n, "A.cols mismatch");
    require(A.nnz() == nnz, "A.nnz mismatch");

    DeviceCsr At = DeviceCsr::transpose_from_csc(mdl.matrix);
    require(At.rows() == n, "At.rows mismatch");
    require(At.cols() == m, "At.cols mismatch");
    require(At.nnz() == nnz, "At.nnz mismatch");

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    std::vector<double> h_x(n);
    for (std::size_t j = 0; j < n; ++j) {
        h_x[j] = dist(rng);
    }
    std::vector<double> expected_y = mdl.matrix.multiply(h_x);
    DeviceBuffer<double> d_x(h_x);
    DeviceBuffer<double> d_y(m);
    DeviceBuffer<double> d_y_cpu(m);
    spmv(A, d_x, d_y);
    spmv_cpu(A, d_x, d_y_cpu);
    std::vector<double> actual_y = d_y.to_vector();
    std::vector<double> actual_y_cpu = d_y_cpu.to_vector();

    double fwd_diff_cpu = compute_max_abs_diff(actual_y, actual_y_cpu);
    double fwd_diff_csc = compute_max_rel_diff(expected_y, actual_y);
    require(fwd_diff_cpu <= 1e-12, "forward kernel vs CPU diff exceeds 1e-12");
    require(fwd_diff_csc <= 1e-12, "forward CSC vs CSR relative diff exceeds 1e-12");

    std::vector<double> h_y(m);
    for (std::size_t i = 0; i < m; ++i) {
        h_y[i] = dist(rng);
    }
    std::vector<double> expected_z = csc_multiply_transpose(mdl.matrix, h_y);
    DeviceBuffer<double> d_yt(h_y);
    DeviceBuffer<double> d_z(n);
    DeviceBuffer<double> d_z_cpu(n);
    spmv_transpose(At, d_yt, d_z);
    spmv_transpose_cpu(At, d_yt, d_z_cpu);
    std::vector<double> actual_z = d_z.to_vector();
    std::vector<double> actual_z_cpu = d_z_cpu.to_vector();

    double trans_diff_cpu = compute_max_abs_diff(actual_z, actual_z_cpu);
    double trans_diff_csc = compute_max_rel_diff(expected_z, actual_z);
    require(trans_diff_cpu <= 1e-12, "transpose kernel vs CPU diff exceeds 1e-12");
    require(trans_diff_csc <= 1e-12, "transpose CSC vs CSR relative diff exceeds 1e-12");

    std::cout << "  " << std::left << std::setw(12) << instance_name
              << " (" << std::right << std::setw(5) << m << " x "
              << std::setw(5) << n << ", nnz="
              << std::setw(6) << nnz << ") fwd_diff="
              << std::scientific << std::setprecision(2) << fwd_diff_cpu
              << " trans_diff="
              << std::scientific << std::setprecision(2) << trans_diff_cpu
              << " [PASS]\n";
}
}

namespace detail_equivalence_test {
void test_all_netlib_matrices() {
    std::cout << "--- Testing SpMV & Transpose SpMV on Netlib Matrix Corpus ---\n";
    const std::vector<std::string> netlib_instances = {
        "afiro", "adlittle", "beaconfd", "blend", "kb2", "lotfi",
        "recipe", "sc105", "sc205", "sc50a", "sc50b", "scagr7",
        "scorpion", "scsd1", "scsd6", "share1b", "share2b"
    };

    std::size_t index = 0;
    for (const auto& name : netlib_instances) {
        const std::string path = "data/netlib/" + name + ".mps";
        test_netlib_instance(name, path, 1000U + index * 37U);
        ++index;
    }
    std::cout << "All " << netlib_instances.size()
              << " Netlib matrices passed forward & transpose equivalence tests.\n";
}
}

}
