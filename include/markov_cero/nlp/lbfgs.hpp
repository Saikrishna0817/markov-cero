#pragma once

#include <cstddef>
#include <vector>

namespace markov_cero::nlp {

// W1 / D-02: limited-memory BFGS approximation of the (Lagrangian) Hessian.
// Stores the last `memory` (s, y) pairs with curvature filtering so B stays
// positive definite; direction = -H_k grad via the classic two-loop recursion
// (Nocedal & Wright, Algorithm 7.4). BOUNDS: the "B" in L-BFGS-B refers to
// bound handling in the subproblem; H itself is the plain inverse-Hessian
// product used for the QP subproblem's Hessian block.

class Lbfgs {
  public:
    explicit Lbfgs(std::size_t memory = 10) : memory_(memory) {}

    void reset() {
        s_history_.clear();
        y_history_.clear();
        rho_history_.clear();
        gamma_ = 1.0;
    }

    [[nodiscard]] std::size_t pair_count() const noexcept { return s_history_.size(); }
    [[nodiscard]] std::size_t memory() const noexcept { return memory_; }
    // Initial Hessian scaling H_0 = gamma * I from the latest curvature pair
    // (Nocedal & Wright 7.20); feeds the QP subproblem's diagonal Hessian.
    [[nodiscard]] double gamma() const noexcept { return gamma_; }

    // Materialize the current limited-memory BFGS Hessian for the QP
    // subproblem. Storage is O(n^2) here because the ADMM QP interface takes
    // an explicit sparse matrix; the curvature history itself remains O(nm).
    [[nodiscard]] std::vector<double> hessian_matrix(std::size_t dimension) const;

    // curvature = s^T y of the accepted pair; pairs with non-positive curvature
    // are skipped (keeps B positive definite).
    void update(const std::vector<double>& s, const std::vector<double>& y);

    // Two-loop recursion: returns d = -H * grad (descent direction).
    [[nodiscard]] std::vector<double> search_direction(const std::vector<double>& grad) const;

  private:
    std::size_t memory_;
    std::vector<std::vector<double>> s_history_;
    std::vector<std::vector<double>> y_history_;
    std::vector<double> rho_history_;
    double gamma_{1.0};
};

} // namespace markov_cero::nlp
