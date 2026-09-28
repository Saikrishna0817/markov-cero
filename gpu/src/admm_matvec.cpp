#include "markov_cero/gpu/admm_step.hpp"
#include "markov_cero/gpu/device.hpp"
#include "markov_cero/gpu/kernels.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace markov_cero::gpu {
namespace {

// Expand an upper-triangular symmetric CSC matrix (i <= j entries only) into
// full CSR rows on the host. Mirrors SparseSymmetricMatrix::multiply semantics:
// P*x = sum_{i<=j} P_ij * (e_i x_j + e_j x_i) with diagonal counted once.
struct FullCsrHost {
    std::vector<std::size_t> row_offsets;
    std::vector<std::size_t> col_indices;
    std::vector<double> values;
    std::size_t nnz{0};
};

FullCsrHost expand_symmetric_csc_to_csr(const std::vector<std::size_t>& column_offsets,
                                        const std::vector<std::size_t>& row_indices,
                                        const std::vector<double>& values,
                                        std::size_t dimension) {
    std::vector<std::vector<std::pair<std::size_t, double>>> rows(dimension);
    for (std::size_t j = 0; j < dimension; ++j) {
        const std::size_t start = column_offsets[j];
        const std::size_t end = column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            const std::size_t i = row_indices[k];
            const double v = values[k];
            if (i == j) {
                rows[i].emplace_back(j, v);
            } else {
                rows[i].emplace_back(j, v);
                rows[j].emplace_back(i, v);
            }
        }
    }
    FullCsrHost out;
    out.row_offsets.assign(dimension + 1, 0);
    for (std::size_t i = 0; i < dimension; ++i) {
        // CSR rows must be strictly increasing in column index.
        std::sort(rows[i].begin(), rows[i].end());
        for (const auto& [col, val] : rows[i]) {
            out.col_indices.push_back(col);
            out.values.push_back(val);
        }
        out.row_offsets[i + 1] = out.col_indices.size();
    }
    out.nnz = out.col_indices.size();
    return out;
}

// Precondition: csr rows are sorted per row.
FullCsrHost add_diagonal_to_csr(const FullCsrHost& csr, std::size_t dimension, double rho) {
    FullCsrHost out;
    out.row_offsets.assign(dimension + 1, 0);
    for (std::size_t i = 0; i < dimension; ++i) {
        bool diag_inserted = false;
        for (std::size_t p = csr.row_offsets[i]; p < csr.row_offsets[i + 1]; ++p) {
            const std::size_t col = csr.col_indices[p];
            if (!diag_inserted && col > i) {
                out.col_indices.push_back(i);
                out.values.push_back(rho);
                diag_inserted = true;
            }
            out.col_indices.push_back(col);
            out.values.push_back(col == i ? csr.values[p] + rho : csr.values[p]);
            if (col == i) {
                diag_inserted = true;
            }
        }
        if (!diag_inserted) {
            out.col_indices.push_back(i);
            out.values.push_back(rho);
        }
        out.row_offsets[i + 1] = out.col_indices.size();
    }
    out.nnz = out.col_indices.size();
    return out;
}

// Convert an expanded, row-sorted CSR layout into CSC so DeviceCsr::from_csc
// can ingest it (its constructor performs the CSR conversion internally).
linalg::SparseCsc csr_to_csc_host(const FullCsrHost& csr, std::size_t dimension) {
    std::vector<std::vector<std::pair<std::size_t, double>>> columns(dimension);
    for (std::size_t i = 0; i < dimension; ++i) {
        for (std::size_t p = csr.row_offsets[i]; p < csr.row_offsets[i + 1]; ++p) {
            columns[csr.col_indices[p]].emplace_back(i, csr.values[p]);
        }
    }
    linalg::SparseCsc out;
    out.rows = dimension;
    out.columns = dimension;
    out.column_offsets.assign(dimension + 1, 0);
    for (std::size_t j = 0; j < dimension; ++j) {
        std::sort(columns[j].begin(), columns[j].end());
        for (const auto& [row, val] : columns[j]) {
            out.row_indices.push_back(row);
            out.values.push_back(val);
        }
        out.column_offsets[j + 1] = out.row_indices.size();
    }
    return out;
}

} // namespace

void admm_p_rho_product_cpu(const std::vector<std::size_t>& p_column_offsets,
                            const std::vector<std::size_t>& p_row_indices,
                            const std::vector<double>& p_values,
                            std::size_t dimension,
                            double rho,
                            const std::vector<double>& x,
                            std::vector<double>& w) {
    w.assign(dimension, 0.0);
    for (std::size_t j = 0; j < dimension; ++j) {
        const std::size_t start = p_column_offsets[j];
        const std::size_t end = p_column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            const std::size_t i = p_row_indices[k];
            const double v = p_values[k];
            w[i] += v * x[j];
            if (i != j) {
                w[j] += v * x[i];
            }
        }
    }
    for (std::size_t i = 0; i < dimension; ++i) {
        w[i] += rho * x[i];
    }
}

void admm_p_product_cpu(const std::vector<std::size_t>& p_column_offsets,
                        const std::vector<std::size_t>& p_row_indices,
                        const std::vector<double>& p_values,
                        std::size_t dimension,
                        const std::vector<double>& x,
                        std::vector<double>& y) {
    y.assign(dimension, 0.0);
    for (std::size_t j = 0; j < dimension; ++j) {
        const std::size_t start = p_column_offsets[j];
        const std::size_t end = p_column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            const std::size_t i = p_row_indices[k];
            const double v = p_values[k];
            y[i] += v * x[j];
            if (i != j) {
                y[j] += v * x[i];
            }
        }
    }
}

AdmmGpuContext make_admm_gpu_context(const std::vector<std::size_t>& p_column_offsets,
                                     const std::vector<std::size_t>& p_row_indices,
                                     const std::vector<double>& p_values,
                                     std::size_t dimension,
                                     double rho) {
    AdmmGpuContext ctx;
    ctx.rho_used = rho;
    if (!is_gpu_available() || dimension == 0) {
        return ctx;  // invalid context -> caller falls back to CPU silently
    }

    const FullCsrHost full =
        expand_symmetric_csc_to_csr(p_column_offsets, p_row_indices, p_values, dimension);
    const FullCsrHost shifted = add_diagonal_to_csr(full, dimension, rho);

    ctx.p_full = DeviceCsr::from_csc(csr_to_csc_host(full, dimension));
    ctx.p_plus_rho = DeviceCsr::from_csc(csr_to_csc_host(shifted, dimension));
    return ctx;
}

void admm_p_rho_product(const AdmmGpuContext& ctx,
                        const DeviceBuffer<double>& x,
                        DeviceBuffer<double>& w) {
    spmv(ctx.p_plus_rho, x, w);
}

void admm_p_product(const AdmmGpuContext& ctx,
                    const DeviceBuffer<double>& w,
                    DeviceBuffer<double>& v) {
    spmv(ctx.p_full, w, v);
}

} // namespace markov_cero::gpu
