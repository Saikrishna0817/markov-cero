#pragma once
#include "markov_cero/io/mps.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace markov_cero::io::detail {
enum class Section {
    none,
    objective_sense,
    objective_name,
    rows,
    columns,
    rhs,
    ranges,
    bounds,
    quadobj,
    qmatrix,
    nlobj,
    nlcon,
    end
};
struct QuadEntry {
    std::size_t col1;
    std::size_t col2;
    double value;
    bool is_quadobj;
};
struct Row {
    char type;
    std::string name;
    double rhs{0.0};
    bool has_rhs{false};
    double range{0.0};
    bool has_range{false};
};
struct Column {
    std::string name;
    model::VariableType type{model::VariableType::continuous};
    model::Bound lower{model::Bound::finite(0.0)};
    model::Bound upper{model::Bound::positive_infinity()};
    double objective{0.0};
    bool marker_upper_default{false};
};

std::string trim(std::string value);
std::vector<std::string> tokens(const std::string& line);
std::string unquote(std::string value);
double number(const std::string& text, std::size_t line);
bool header(const std::string& token);
class Parser {
  public:
    explicit Parser(const MpsLimits& requested) : limits(requested) {}
    model::Model read(std::istream& input);
  private:
    const MpsLimits& limits;
    Section section = Section::none;
    std::string problem_name;
    std::string objective_name;
    model::ObjectiveSense sense = model::ObjectiveSense::minimize;
    std::vector<Row> rows;
    std::unordered_map<std::string, std::size_t> row_by_name;
    std::vector<Column> columns;
    std::unordered_map<std::string, std::size_t> column_by_name;
    struct Coefficient {
        std::size_t row;
        std::size_t column;
        double value;
    };
    std::vector<Coefficient> coefficients;
    std::vector<QuadEntry> quad_entries;
    std::vector<model::NlobjTerm> nlobj_terms;
    std::vector<model::NlconConstraint> nlcon_constraints;
    std::unordered_map<std::string, std::size_t> nlcon_by_name;
    std::size_t unnamed_nlcon = 0;
    std::string rhs_vector, range_vector, bound_vector;
    bool in_integer_block = false;
    bool saw_end = false;
    std::size_t bytes = 0U;
    std::size_t line_number = 0U;

    std::size_t nonlinear_entries{0};
    bool read_line(std::istream& input, std::string& raw);
    void require_name(const std::string& name);
    std::size_t find_row(const std::string& name);
    std::size_t find_or_add_column(const std::string& name);
    void record(const std::vector<std::string>& fields);
    void extended(const std::vector<std::string>& fields);
    model::Model build();
    void check_nonlinear_limit() {
        if (nonlinear_entries >= limits.maximum_nonzeros)
            throw MpsError(line_number, "nonlinear term limit exceeded");
        ++nonlinear_entries;
    }
};
} // namespace markov_cero::io::detail
