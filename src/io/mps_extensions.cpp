#include "mps_internal.hpp"

namespace markov_cero::io::detail {
void Parser::extended(const std::vector<std::string>& fields) {
        if (section == Section::quadobj || section == Section::qmatrix) {
            if (fields.size() != 3U)
                throw MpsError(line_number,
                               "QUADOBJ/QMATRIX record requires two columns and a value");
            const auto col1 = find_or_add_column(fields[0]);
            const auto col2 = find_or_add_column(fields[1]);
            const double val = number(fields[2], line_number);
            check_nonlinear_limit();
            quad_entries.push_back({col1, col2, val, section == Section::quadobj});
            return;
        }
        if (section == Section::nlobj) {
            // D-12/D-20: "COEFF VAR1 [VAR2]" polynomial term, degree <= 2.
            // Comment lines (leading *) were already skipped by the tokenizer.
            if (fields.size() != 2U && fields.size() != 3U)
                throw MpsError(line_number,
                               "NLOBJ record requires COEFF VAR1 [VAR2]");
            check_nonlinear_limit();
            const double coeff = number(fields[0], line_number);
            require_name(fields[1]);
            const auto col0 = column_by_name.find(fields[1]);
            if (col0 == column_by_name.end())
                throw MpsError(line_number, "unknown NLOBJ variable: " + fields[1]);
            const auto var0 = col0->second;
            if (fields.size() == 3U) {
                require_name(fields[2]);
                const auto col1 = column_by_name.find(fields[2]);
                if (col1 == column_by_name.end())
                    throw MpsError(line_number, "unknown NLOBJ variable: " + fields[2]);
                const auto var1 = col1->second;
                nlobj_terms.push_back({coeff, var0, var1, true});
            } else {
                nlobj_terms.push_back({coeff, var0, var0, false});
            }
            return;
        }
        if (section == Section::nlcon) {
            const auto op = std::find(fields.begin(), fields.end(), "<=");
            if (op == fields.end())
                throw MpsError(line_number, "NLCON record requires COEFF VAR [VAR] <= RHS [NAME]");
            const std::size_t op_index = static_cast<std::size_t>(op - fields.begin());
            if ((op_index != 2U && op_index != 3U) ||
                (fields.size() != op_index + 2U && fields.size() != op_index + 3U))
                throw MpsError(line_number, "NLCON record requires COEFF VAR [VAR] <= RHS [NAME]");
            check_nonlinear_limit();
            const double coeff = number(fields[0], line_number);
            const double rhs = number(fields[op_index + 1U], line_number);
            require_name(fields[1]);
            const auto col0 = column_by_name.find(fields[1]);
            if (col0 == column_by_name.end())
                throw MpsError(line_number, "unknown NLCON variable: " + fields[1]);
            std::size_t var1 = col0->second;
            const bool quadratic = op_index == 3U;
            std::string name;
            if (quadratic) {
                require_name(fields[2]);
                const auto col1 = column_by_name.find(fields[2]);
                if (col1 == column_by_name.end())
                    throw MpsError(line_number, "unknown NLCON variable: " + fields[2]);
                var1 = col1->second;
            }
            if (fields.size() == op_index + 3U) {
                name = fields[op_index + 2U];
            } else {
                name = "nlcon_" + std::to_string(++unnamed_nlcon);
            }
            std::size_t idx;
            const auto found = nlcon_by_name.find(name);
            if (found == nlcon_by_name.end()) {
                require_name(name);
                idx = nlcon_constraints.size();
                nlcon_by_name.emplace(name, idx);
                nlcon_constraints.push_back({name, rhs, {}});
            } else {
                idx = found->second;
                if (nlcon_constraints[idx].rhs != rhs)
                    throw MpsError(line_number, "inconsistent RHS for NLCON constraint: " + name);
            }
            nlcon_constraints[idx].terms.push_back({coeff, col0->second, var1, quadratic});
            return;
        }
        throw MpsError(line_number, "record outside a supported section");
}
} // namespace markov_cero::io::detail
