#include "kkt_internal.hpp"
namespace markov_cero::qp {
using namespace detail_kkt;
bool KktSolver::factorize(const SparseSymmetricMatrix& P,
                          const linalg::SparseCsc& A,
                          double sigma,
                          const std::vector<double>& rho,
                          std::optional<std::chrono::steady_clock::time_point> deadline) {
    factorized_ = false;
    deadline_reached_ = false;
    fill_limit_reached_ = false;
    if (!build_kkt_matrix(P, A, sigma, rho, deadline)) {
        deadline_reached_ = true;
        return false;
    }

    // Symbolic factorization
    L_col_ptr_.assign(total_dim_ + 1, 0);
    parent_.assign(total_dim_, std::numeric_limits<std::size_t>::max());
    std::vector<std::size_t> lnz(total_dim_, 0);
    std::vector<std::size_t> flag(total_dim_, 0);

    if (!ldl_symbolic(total_dim_, kkt_col_ptr_, kkt_row_ind_, L_col_ptr_, parent_, lnz,
                      flag, nullptr, nullptr, deadline)) {
        deadline_reached_ = true;
        return false;
    }
    if (deadline && std::chrono::steady_clock::now() >= *deadline) {
        deadline_reached_ = true;
        return false;
    }

    const std::size_t total_lnz = L_col_ptr_[total_dim_];
    if (total_lnz > kMaxKktFactorNonzeros) {
        fill_limit_reached_ = true;
        return false;
    }
    L_row_ind_.assign(total_lnz, 0);
    L_val_.assign(total_lnz, 0.0);
    D_.assign(total_dim_, 0.0);

    // Numeric factorization
    std::vector<double> Y(total_dim_, 0.0);
    std::vector<std::size_t> Pattern(total_dim_, 0);
    flag.assign(total_dim_, 0);
    lnz.assign(total_dim_, 0);

    factorized_ = ldl_numeric(total_dim_, kkt_col_ptr_, kkt_row_ind_, kkt_val_, L_col_ptr_,
                              parent_, lnz, L_row_ind_, L_val_, D_, Y, Pattern, flag,
                              nullptr, nullptr, deadline, deadline_reached_);

    return factorized_;
}
bool KktSolver::update_numeric(const SparseSymmetricMatrix& P,
                              const linalg::SparseCsc& A,
                              double sigma,
                              const std::vector<double>& rho,
                              std::optional<std::chrono::steady_clock::time_point> deadline) {
    factorized_ = false;
    deadline_reached_ = false;
    fill_limit_reached_ = false;
    if (!build_kkt_matrix(P, A, sigma, rho, deadline)) {
        deadline_reached_ = true;
        return false;
    }

    const std::size_t total_lnz = L_col_ptr_[total_dim_];
    L_row_ind_.assign(total_lnz, 0);
    L_val_.assign(total_lnz, 0.0);
    D_.assign(total_dim_, 0.0);

    std::vector<double> Y(total_dim_, 0.0);
    std::vector<std::size_t> Pattern(total_dim_, 0);
    std::vector<std::size_t> flag(total_dim_, 0);
    std::vector<std::size_t> lnz(total_dim_, 0);

    factorized_ = ldl_numeric(total_dim_, kkt_col_ptr_, kkt_row_ind_, kkt_val_, L_col_ptr_,
                              parent_, lnz, L_row_ind_, L_val_, D_, Y, Pattern, flag,
                              nullptr, nullptr, deadline, deadline_reached_);

    return factorized_;
}
void KktSolver::solve(const std::vector<double>& rhs_x,
                      const std::vector<double>& rhs_z,
                      std::vector<double>& sol_x,
                      std::vector<double>& sol_nu) const {
    if (!factorized_) {
        throw std::runtime_error("KktSolver::solve called on unfactorized system");
    }

    std::vector<double> work(total_dim_, 0.0);
    for (std::size_t j = 0; j < n_ && j < rhs_x.size(); ++j) {
        work[j] = rhs_x[j];
    }
    for (std::size_t i = 0; i < m_ && i < rhs_z.size(); ++i) {
        work[n_ + i] = rhs_z[i];
    }

    // 1. Forward substitution: L * v = rhs
    ldl_lsolve(total_dim_, work, L_col_ptr_, L_row_ind_, L_val_);

    // 2. Diagonal solve: D * w = v
    ldl_dsolve(total_dim_, work, D_);

    // 3. Backward substitution: L^T * [x; nu] = w
    ldl_ltsolve(total_dim_, work, L_col_ptr_, L_row_ind_, L_val_);

    // Extract solutions
    sol_x.resize(n_);
    for (std::size_t j = 0; j < n_; ++j) {
        sol_x[j] = work[j];
    }

    sol_nu.resize(m_);
    for (std::size_t i = 0; i < m_; ++i) {
        sol_nu[i] = work[n_ + i];
    }
}
std::size_t KktSolver::nonzeros_L() const noexcept {
    return L_col_ptr_.empty() ? 0 : L_col_ptr_.back();
}
double KktSolver::condition_estimate() const noexcept {
    if (!factorized_ || D_.empty()) {
        return 0.0;
    }
    double maximum = 0.0;
    double minimum = std::numeric_limits<double>::infinity();
    for (double value : D_) {
        const double magnitude = std::abs(value);
        if (!(magnitude > 0.0)) {
            // A zero (or subnormal) LDL^T pivot means the system was singular
            // in practice: report an unbounded condition, matching
            // linalg::sparse_condition_estimate's degenerate convention.
            return std::numeric_limits<double>::infinity();
        }
        maximum = std::max(maximum, magnitude);
        minimum = std::min(minimum, magnitude);
    }
    return maximum / minimum;
}
}
