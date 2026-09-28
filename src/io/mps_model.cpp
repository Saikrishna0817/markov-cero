#include "mps_internal.hpp"

namespace markov_cero::io::detail {
model::Model Parser::build() {
    if (!saw_end)
        throw MpsError(line_number, "missing ENDATA");
    if (in_integer_block)
        throw MpsError(line_number, "missing INTEND");
    if (objective_name.empty() || !row_by_name.contains(objective_name) ||
        rows[row_by_name.at(objective_name)].type != 'N')
        throw MpsError(line_number, "missing objective N row");
    std::vector<std::size_t> row_map(rows.size(), std::numeric_limits<std::size_t>::max());
    std::vector<std::string> row_names;
    std::vector<model::Bound> row_lower, row_upper;
    for (std::size_t old = 0; old < rows.size(); ++old) {
        const auto& row = rows[old];
        if (row.type == 'N')
            continue;
        row_map[old] = row_names.size();
        row_names.push_back(row.name);
        const double rhs = row.rhs;
        model::Bound lower = model::Bound::negative_infinity(),
                     upper = model::Bound::positive_infinity();
        if (row.type == 'E')
            lower = upper = model::Bound::finite(rhs);
        else if (row.type == 'L')
            upper = model::Bound::finite(rhs);
        else
            lower = model::Bound::finite(rhs);
        if (row.has_range) {
            const double width = std::abs(row.range);
            if (row.type == 'L')
                lower = model::Bound::finite(rhs - width);
            else if (row.type == 'G')
                upper = model::Bound::finite(rhs + width);
            else if (row.type == 'E') {
                if (row.range >= 0.0)
                    upper = model::Bound::finite(rhs + width);
                else
                    lower = model::Bound::finite(rhs - width);
            }
        }
        row_lower.push_back(lower);
        row_upper.push_back(upper);
    }
    model::SparseMatrixBuilder builder(row_names.size(), columns.size());
    for (const auto& entry : coefficients)
        builder.add(row_map[entry.row], entry.column, entry.value);
    model::Model result;
    result.name = problem_name;
    result.objective_sense = sense;
    result.matrix = builder.build();
    result.row_name = std::move(row_names);
    result.row_lower = std::move(row_lower);
    result.row_upper = std::move(row_upper);
    for (const auto& column : columns) {
        result.variable_name.push_back(column.name);
        result.objective.push_back(column.objective);
        result.variable_lower.push_back(column.lower);
        result.variable_upper.push_back(column.upper);
        result.variable_type.push_back(column.type);
    }
    if (!quad_entries.empty()) {
        model::SparseMatrixBuilder q_builder(columns.size(), columns.size());
        for (const auto& qe : quad_entries) {
            q_builder.add(qe.col1, qe.col2, qe.value);
            if (qe.is_quadobj && qe.col1 != qe.col2) {
                q_builder.add(qe.col2, qe.col1, qe.value);
            }
        }
        result.has_quadratic_objective = true;
        result.quadratic_matrix = q_builder.build();
    }
    result.has_nlobj_section = !nlobj_terms.empty() || !nlcon_constraints.empty();
    result.nlobj_terms = std::move(nlobj_terms);
    result.nlcon_constraints = std::move(nlcon_constraints);
    result.validate();
    return result;
}
} // namespace markov_cero::io::detail
