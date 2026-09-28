#pragma once

#include "markov_cero/gpu/buffer.hpp"
#include "markov_cero/gpu/csr.hpp"

#include <cstddef>
#include <vector>

namespace markov_cero::gpu {

// W3 / D-08: GPU-accelerated ADMM x-update support for large convex QP.
//
// The ADMM x-update direction is dominated by the symmetric matrix-vector
// products w = (P + rho*I) x and v = P_full w. Both are SpMV-shaped and
// bandwidth-bound, which is exactly the workload the GPU wins at scale
// (plan D-08: GPU scope = PDLP large LP + ADMM large QP, nothing else).
//
// Activation contract (LOCKED, plan W3 section 3.1): the solver calls into
// this module only when
//   1. the caller explicitly requested --backend gpu,
//   2. NNZ of the quadratic matrix P > 100,000,
//   3. MARKOV_CERO_HAS_CUDA=1 at compile time.
// If any condition fails the caller stays on the CPU path silently.

struct AdmmGpuContext {
    // Device-resident matrices. P_full is the symmetric expansion of the
    // upper-triangular P; P_plus_rho is (P + rho*I) in the same CSR layout.
    DeviceCsr p_full;
    DeviceCsr p_plus_rho;
    // Scalar rho used to build p_plus_rho (rebuilt when adaptive rho changes
    // the diagonal by more than the rebuild tolerance).
    double rho_used{0.0};

    [[nodiscard]] bool valid() const noexcept { return p_plus_rho.rows() > 0; }
};

// Build the device-side matrices for the ADMM step. Returns an invalid context
// (valid() == false) when CUDA is unavailable; callers must treat that as the
// CPU fallback signal, not an error.
[[nodiscard]] AdmmGpuContext make_admm_gpu_context(
    const std::vector<std::size_t>& p_column_offsets,
    const std::vector<std::size_t>& p_row_indices,
    const std::vector<double>& p_values,
    std::size_t dimension,
    double rho);

// Compute w = (P + rho*I) x on the device (symmetric SpMV + diagonal scale).
void admm_p_rho_product(const AdmmGpuContext& ctx,
                        const DeviceBuffer<double>& x,
                        DeviceBuffer<double>& w);

// Compute v = P_full * w on the device.
void admm_p_product(const AdmmGpuContext& ctx,
                    const DeviceBuffer<double>& w,
                    DeviceBuffer<double>& v);

// CPU reference implementations (correctness oracle for gpu/tests/admm_gpu_test
// and the fallback path). y = (P + rho*I) x for a symmetric upper-triangular
// CSC P of the QP model's SparseSymmetricMatrix layout.
void admm_p_rho_product_cpu(const std::vector<std::size_t>& p_column_offsets,
                            const std::vector<std::size_t>& p_row_indices,
                            const std::vector<double>& p_values,
                            std::size_t dimension,
                            double rho,
                            const std::vector<double>& x,
                            std::vector<double>& w);

// y = P_full * x for the same upper-triangular symmetric layout.
void admm_p_product_cpu(const std::vector<std::size_t>& p_column_offsets,
                        const std::vector<std::size_t>& p_row_indices,
                        const std::vector<double>& p_values,
                        std::size_t dimension,
                        const std::vector<double>& x,
                        std::vector<double>& y);

} // namespace markov_cero::gpu
