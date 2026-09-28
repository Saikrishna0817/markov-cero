// W3 / D-08 (LOCKED): end-to-end QP solve with --backend gpu.
// The GPU ADMM x-update path only engages when CUDA is compiled in, the
// device exists, and NNZ(P) > 100k; on every other host the solver must fall
// back to the CPU ADMM silently and still certify the optimum (C4).
#include "markov_cero/api/solve.hpp"
#include "markov_cero/gpu/device.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/qp/verifier.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace markov_cero;

namespace {

void req(bool q, const char* m) {
    if (!q) {
        std::cerr << "FAIL: " << m << "\n";
        std::exit(1);
    }
}

} // namespace

static qp::QuadraticModel make_large_convex_qp() {
    constexpr std::size_t n = 4096;
    constexpr std::size_t half_bandwidth = 26;
    qp::QuadraticModel model;
    model.name = "gpu_admm_large_sparse_qp";
    model.P.dimension = n;
    model.P.column_offsets.reserve(n + 1);
    model.P.column_offsets.push_back(0);
    for (std::size_t j = 0; j < n; ++j) {
        const std::size_t first_row = j > half_bandwidth ? j - half_bandwidth : 0;
        for (std::size_t i = first_row; i <= j; ++i) {
            model.P.row_indices.push_back(i);
            model.P.values.push_back(i == j ? 2.0 : 0.005);
        }
        model.P.column_offsets.push_back(model.P.values.size());
    }
    model.q.assign(n, -1.0);

    // One inactive box constraint keeps m positive for ADMM's per-row rho
    // storage without changing the unconstrained minimizer.
    model.A.rows = 1;
    model.A.columns = n;
    model.A.column_offsets.assign(n + 1, 1);
    model.A.column_offsets[0] = 0;
    model.A.row_indices = {0};
    model.A.values = {1.0};
    model.l = {-10.0};
    model.u = {10.0};
    model.validate();
    return model;
}

int main() {
    api::SolveOptions gpu_options;
    gpu_options.engine = "qp";
    gpu_options.backend = "gpu";
    const auto gpu_res = api::solve_file("examples/qp_portfolio.mps", gpu_options);
    req(gpu_res.status == lp::reference::SolveStatus::optimal,
        "gpu-backed QP request reaches Optimal (GPU or silent CPU fallback)");
    req(gpu_res.verified, "gpu-backed QP solution passes the primal verifier");

    api::SolveOptions cpu_options;
    cpu_options.engine = "qp";
    cpu_options.backend = "cpu";
    const auto cpu_res = api::solve_file("examples/qp_portfolio.mps", cpu_options);
    req(cpu_res.status == lp::reference::SolveStatus::optimal, "cpu QP solve is optimal");

    const double scale = std::max(1.0, std::abs(cpu_res.objective));
    req(std::abs(gpu_res.objective - cpu_res.objective) / scale < 1e-6,
        "gpu and cpu QP paths agree on the objective");

    // Cross the locked NNZ(P) activation gate. On a CUDA host this must run
    // the GPU residual product path and agree with the CPU solution.
    {
        auto model = make_large_convex_qp();
        req(model.P.values.size() > 100'000, "large QP exceeds GPU activation threshold");
        qp::QpOptions options;
        options.absolute_tolerance = 1e-5;
        options.relative_tolerance = 1e-5;
        const auto gpu_large = qp::solve_qp(model, options, /*gpu_backend_requested=*/true);
        if (gpu_large.status != qp::QpStatus::optimal) {
            std::cerr << "large QP status=" << qp::to_string(gpu_large.status)
                      << " message=" << gpu_large.message << '\n';
        }
        req(gpu_large.status == qp::QpStatus::optimal, "large GPU-requested QP is optimal");
        req(qp::verify_qp_solution(model, gpu_large, 1e-5).passed,
            "large GPU-requested QP passes independent KKT verification");
        if (gpu::is_gpu_available()) {
            req(gpu_large.gpu_path_active, "large QP activates GPU path on CUDA hardware");
        }
        const auto cpu_large = qp::solve_qp(model, options, /*gpu_backend_requested=*/false);
        req(cpu_large.status == qp::QpStatus::optimal, "large CPU QP is optimal");
        const double large_scale = std::max(1.0, std::abs(cpu_large.objective_value));
        req(std::abs(gpu_large.objective_value - cpu_large.objective_value) / large_scale < 1e-5,
            "large GPU and CPU QP objectives agree");
        std::cout << std::setprecision(17)
                  << "[+] large QP activation passed: P.nnz=" << model.P.values.size()
                  << " gpu_path_active=" << std::boolalpha << gpu_large.gpu_path_active
                  << " gpu_objective=" << gpu_large.objective_value
                  << " cpu_objective=" << cpu_large.objective_value
                  << " gpu_iterations=" << gpu_large.iterations
                  << " cpu_iterations=" << cpu_large.iterations
                  << " gpu_kkt_verified="
                  << qp::verify_qp_solution(model, gpu_large, 1e-5).passed << "\n";
    }

    std::cout << "[+] gpu_qp_test PASSED: obj=" << gpu_res.objective
              << " rho_updates=" << gpu_res.admm_rho_updates << "\n";
    return 0;
}
