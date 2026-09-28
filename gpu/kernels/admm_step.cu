#include "markov_cero/gpu/kernels.hpp"
#include "markov_cero/gpu/admm_step.hpp"

#ifdef MARKOV_CERO_HAS_CUDA
#include <cuda_runtime.h>
#include <stdexcept>
#include <string>

namespace markov_cero::gpu::detail {

namespace {

// Symmetric SpMV for the ADMM x-update: y = A * x with A in CSR. Reuses the
// warp-per-row csr-vector pattern of spmv.cu; A carries the (P + rho*I) or
// P_full layout prepared on the host by make_admm_gpu_context.
__global__ void admm_spmv_kernel(std::size_t rows,
                                 const std::size_t* __restrict__ row_offsets,
                                 const std::size_t* __restrict__ col_indices,
                                 const double* __restrict__ values,
                                 const double* __restrict__ x,
                                 double* __restrict__ y) {
    const std::size_t warp_id = (blockIdx.x * blockDim.x + threadIdx.x) / 32U;
    const unsigned int lane_id = threadIdx.x & 31U;

    if (warp_id >= rows) {
        return;
    }

    const std::size_t row_start = row_offsets[warp_id];
    const std::size_t row_end = row_offsets[warp_id + 1];

    double sum = 0.0;
    for (std::size_t p = row_start + lane_id; p < row_end; p += 32U) {
        sum += values[p] * x[col_indices[p]];
    }

    #pragma unroll
    for (int offset = 16; offset > 0; offset /= 2) {
        sum += __shfl_down_sync(0xffffffffU, sum, offset);
    }

    if (lane_id == 0U) {
        y[warp_id] = sum;
    }
}

void launch_admm_spmv(std::size_t rows,
                      const std::size_t* row_offsets,
                      const std::size_t* col_indices,
                      const double* values,
                      const double* x,
                      double* y) {
    if (rows == 0) {
        return;
    }
    constexpr unsigned int threads_per_block = 256U;
    constexpr unsigned int warps_per_block = threads_per_block / 32U;
    const unsigned int num_blocks = static_cast<unsigned int>((rows + warps_per_block - 1U) /
                                                              warps_per_block);
    admm_spmv_kernel<<<num_blocks, threads_per_block>>>(rows, row_offsets, col_indices,
                                                        values, x, y);
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string("admm_spmv_kernel launch failed: ") +
                                 cudaGetErrorString(err));
    }
}

} // namespace
} // namespace markov_cero::gpu::detail

#endif // MARKOV_CERO_HAS_CUDA
