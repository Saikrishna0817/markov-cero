#pragma once

#include "markov_cero/nlp/nlp_model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace markov_cero::model {

enum class ObjectiveSense { minimize, maximize };
enum class VariableType { continuous, integer, binary };
enum class BoundKind { negative_infinity, finite, positive_infinity };

struct Bound final {
    BoundKind kind{BoundKind::finite};
    double value{0.0};

    [[nodiscard]] static Bound negative_infinity() noexcept;
    [[nodiscard]] static Bound finite(double value);
    [[nodiscard]] static Bound positive_infinity() noexcept;
    [[nodiscard]] bool is_finite() const noexcept;
};

struct SparseMatrixCSC final {
    std::size_t row_count{};
    std::size_t column_count{};
    std::vector<std::size_t> column_start;
    std::vector<std::size_t> row_index;
    std::vector<double> value;

    void validate() const;
    [[nodiscard]] std::vector<double> multiply(const std::vector<double>& x) const;
};

class SparseMatrixBuilder final {
  public:
    SparseMatrixBuilder(std::size_t row_count, std::size_t column_count);
    void add(std::size_t row, std::size_t column, double value);
    [[nodiscard]] SparseMatrixCSC build() const;

  private:
    std::size_t row_count_;
    std::size_t column_count_;
    struct Entry final {
        std::size_t row;
        std::size_t column;
        double value;
    };
    std::vector<Entry> entries_;
};

// W1/D-12: one polynomial term of the custom MPS NLOBJ section.
// Degree-1 term:  coefficient * x[var0]
// Degree-2 term:  coefficient * x[var0] * x[var1]
struct NlobjTerm final {
    double coefficient{0.0};
    std::size_t var0{0};
    std::size_t var1{0};
    bool quadratic{false};
};

struct NlconConstraint final {
    std::string name;
    double rhs{0.0};
    std::vector<NlobjTerm> terms;
};

struct Model final {
    std::string name;
    ObjectiveSense objective_sense{ObjectiveSense::minimize};
    double objective_offset{0.0};
    SparseMatrixCSC matrix;
    std::vector<double> objective;
    std::vector<Bound> row_lower;
    std::vector<Bound> row_upper;
    std::vector<Bound> variable_lower;
    std::vector<Bound> variable_upper;
    std::vector<VariableType> variable_type;
    std::vector<std::string> row_name;
    std::vector<std::string> variable_name;
    bool has_quadratic_objective{false};
    SparseMatrixCSC quadratic_matrix;

    // W1/D-01 Path B: polynomial MPS extension content. The classification
    // signal is set for non-empty NLOBJ or NLCON content.
    bool has_nlobj_section{false};
    std::vector<NlobjTerm> nlobj_terms;
    std::vector<NlconConstraint> nlcon_constraints;

    // W1/D-01 Path A: programmatic NLP callbacks accompanying this model.
    // When set, the locked classifier sees callback presence
    // (ClassificationInputs::has_nlp_callbacks) and the unified solve API
    // composes the companion into the SQP/outer-approx view
    // (io::make_nlp_model):
    //   - objective: companion f(x) is ADDED to the Model's linear/NLOBJ
    //     objective (same additivity rule as NLOBJ degree-1 terms); the
    //     Model's objective sense (maximize) negates both parts,
    //   - constraints: companion g(x) <= 0 rows are APPENDED after the
    //     Model's linear rows, companion h(x) = 0 rows ride as equalities,
    //   - bounds: intersection — the tightest finite bound wins,
    //   - companion n_vars must equal the Model's column count.
    std::optional<nlp::NlpModel> nlp_callbacks;

    void validate() const;
};

[[nodiscard]] const char* to_string(ObjectiveSense sense) noexcept;
[[nodiscard]] const char* to_string(VariableType type) noexcept;

} // namespace markov_cero::model
