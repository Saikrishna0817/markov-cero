#pragma once

#include <stdexcept>
#include <string>

namespace markov_cero::core {

/// Base exception for all markov-cero solver runtime errors.
class SolverException : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

/// Thrown when a numerical basis factorization encounters an unrecoverable singular matrix.
class SingularBasisException : public SolverException {
  public:
    explicit SingularBasisException(const std::string& message)
        : SolverException("Singular basis: " + message) {}
};

/// Thrown when numerical condition number exceeds stable floating-point threshold.
class IllConditionedBasisException : public SolverException {
  public:
    explicit IllConditionedBasisException(double estimated_condition)
        : SolverException("Ill-conditioned basis: condition number exceeds threshold: " +
                          std::to_string(estimated_condition)),
          condition_number(estimated_condition) {}

    double condition_number{0.0};
};

/// Thrown when numerical divergence or NaN/Inf is detected in iterative updates.
class NumericalDivergenceException : public SolverException {
  public:
    explicit NumericalDivergenceException(const std::string& message)
        : SolverException("Numerical divergence: " + message) {}
};

/// Thrown when time or iteration resource bounds are exceeded.
class ResourceLimitException : public SolverException {
  public:
    using SolverException::SolverException;
};

} // namespace markov_cero::core
