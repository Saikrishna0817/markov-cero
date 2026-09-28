#include "mps_internal.hpp"

namespace markov_cero::io::detail {
void Parser::record(const std::vector<std::string>& fields) {
        if (section == Section::objective_sense) {
            if (fields.size() != 1U)
                throw MpsError(line_number, "OBJSENSE requires one value");
            if (fields[0] == "MIN" || fields[0] == "MINIMIZE")
                sense = model::ObjectiveSense::minimize;
            else if (fields[0] == "MAX" || fields[0] == "MAXIMIZE")
                sense = model::ObjectiveSense::maximize;
            else {
                throw MpsError(line_number, "unknown objective sense");
            }
            section = Section::none;
            return;
        }
        if (section == Section::objective_name) {
            if (fields.size() != 1U)
                throw MpsError(line_number, "OBJNAME requires one row name");
            objective_name = fields[0];
            section = Section::none;
            return;
        }
        if (section == Section::rows) {
            if (fields.size() != 2U || fields[0].size() != 1U)
                throw MpsError(line_number, "ROWS record requires type and name");
            const char type = fields[0][0];
            if (type != 'N' && type != 'E' && type != 'L' && type != 'G')
                throw MpsError(line_number, "unsupported row type");
            require_name(fields[1]);
            if (row_by_name.contains(fields[1]))
                throw MpsError(line_number, "duplicate row name: " + fields[1]);
            if (rows.size() >= limits.maximum_rows)
                throw MpsError(line_number, "row limit exceeded");
            row_by_name.emplace(fields[1], rows.size());
            rows.push_back({type, fields[1]});
            if (type == 'N' && objective_name.empty())
                objective_name = fields[1];
            return;
        }
        if (section == Section::columns) {
            if (fields.size() == 3U && unquote(fields[1]) == "MARKER") {
                const auto marker = unquote(fields[2]);
                if (marker == "INTORG") {
                    if (in_integer_block)
                        throw MpsError(line_number, "nested INTORG");
                    in_integer_block = true;
                } else if (marker == "INTEND") {
                    if (!in_integer_block)
                        throw MpsError(line_number, "INTEND without INTORG");
                    in_integer_block = false;
                } else {
                    throw MpsError(line_number, "unknown MARKER value");
                }
                return;
            }
            if (fields.size() != 3U && fields.size() != 5U)
                throw MpsError(line_number, "COLUMNS record requires one or two row/value pairs");
            const auto column = find_or_add_column(fields[0]);
            if (in_integer_block && columns[column].type == model::VariableType::continuous) {
                columns[column].type = model::VariableType::integer;
                columns[column].upper = model::Bound::finite(1.0);
                columns[column].marker_upper_default = true;
            }
            for (std::size_t p = 1U; p < fields.size(); p += 2U) {
                const auto row = find_row(fields[p]);
                const double value = number(fields[p + 1U], line_number);
                if (rows[row].type == 'N') {
                    if (rows[row].name != objective_name)
                        throw MpsError(line_number, "coefficient references non-objective N row");
                    columns[column].objective += value;
                    if (!std::isfinite(columns[column].objective))
                        throw MpsError(line_number, "objective coefficient overflow");
                } else {
                    if (coefficients.size() >= limits.maximum_nonzeros)
                        throw MpsError(line_number, "nonzero limit exceeded");
                    coefficients.push_back({row, column, value});
                }
            }
            return;
        }
        if (section == Section::rhs || section == Section::ranges) {
            std::size_t start_p = 1U;
            if (fields.size() == 2U || fields.size() == 4U) {
                start_p = 0U;
            } else if (fields.size() != 3U && fields.size() != 5U) {
                throw MpsError(line_number,
                               "RHS/RANGES record requires vector and row/value pairs");
            }
            if (start_p == 1U) {
                auto& selected = section == Section::rhs ? rhs_vector : range_vector;
                if (selected.empty())
                    selected = fields[0];
                else if (selected != fields[0])
                    throw MpsError(line_number, "multiple rim vectors are unsupported");
            }
            for (std::size_t p = start_p; p < fields.size(); p += 2U) {
                const auto row = find_row(fields[p]);
                if (rows[row].type == 'N') {
                    throw MpsError(line_number, "objective-row RHS/RANGES values are unsupported");
                }
                const double value = number(fields[p + 1U], line_number);
                if (section == Section::rhs) {
                    if (rows[row].has_rhs)
                        throw MpsError(line_number, "duplicate RHS row");
                    rows[row].rhs = value;
                    rows[row].has_rhs = true;
                } else {
                    if (rows[row].has_range)
                        throw MpsError(line_number, "duplicate RANGES row");
                    rows[row].range = value;
                    rows[row].has_range = true;
                }
            }
            return;
        }
        if (section == Section::bounds) {
            if (fields.size() != 3U && fields.size() != 4U)
                throw MpsError(line_number, "BOUNDS record has invalid field count");
            const auto type = fields[0];
            if (bound_vector.empty())
                bound_vector = fields[1];
            else if (bound_vector != fields[1])
                throw MpsError(line_number, "multiple bound vectors are unsupported");
            const auto column = find_or_add_column(fields[2]);
            auto& value = columns[column];
            const bool needs_number =
                type == "LO" || type == "UP" || type == "FX" || type == "LI" || type == "UI";
            if (needs_number != (fields.size() == 4U))
                throw MpsError(line_number, "bound type has wrong value count");
            const double bound = needs_number ? number(fields[3], line_number) : 0.0;
            if ((type == "LO" || type == "LI") && value.marker_upper_default)
                value.upper = model::Bound::positive_infinity();
            if (type == "LO" || type == "LI" || type == "UP" || type == "UI" ||
                type == "FX" || type == "FR" || type == "PL" || type == "BV")
                value.marker_upper_default = false;
            if (type == "LO")
                value.lower = model::Bound::finite(bound);
            else if (type == "UP")
                value.upper = model::Bound::finite(bound);
            else if (type == "FX")
                value.lower = value.upper = model::Bound::finite(bound);
            else if (type == "FR") {
                value.lower = model::Bound::negative_infinity();
                value.upper = model::Bound::positive_infinity();
            } else if (type == "MI")
                value.lower = model::Bound::negative_infinity();
            else if (type == "PL")
                value.upper = model::Bound::positive_infinity();
            else if (type == "BV") {
                value.type = model::VariableType::binary;
                value.lower = model::Bound::finite(0.0);
                value.upper = model::Bound::finite(1.0);
            } else if (type == "LI") {
                value.type = model::VariableType::integer;
                value.lower = model::Bound::finite(bound);
            } else if (type == "UI") {
                value.type = model::VariableType::integer;
                value.upper = model::Bound::finite(bound);
            } else
                throw MpsError(line_number, "unsupported bound type: " + type);
            return;
        }
    extended(fields);
}
} // namespace markov_cero::io::detail
