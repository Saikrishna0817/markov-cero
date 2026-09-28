#include "lp_parser_internal.hpp"
namespace markov_cero::io::detail_lp {
model::Model LpParser::build_model() {
        model::SparseMatrixBuilder builder(row_names_.size(), variables_.size());
        for (const auto& entry : matrix_entries_) {
            builder.add(entry.row, entry.col, entry.val);
        }

        model::Model model;
        model.name = model_name_;
        model.objective_sense = sense_;
        model.objective_offset = objective_offset_;
        model.matrix = builder.build();
        model.row_name = std::move(row_names_);
        model.row_lower = std::move(row_lower_);
        model.row_upper = std::move(row_upper_);

        for (const auto& info : variables_) {
            model.variable_name.push_back(info.name);
            model.objective.push_back(objective_coeffs_[info.index]);
            model.variable_type.push_back(info.type);

            model::Bound lower;
            if (info.lower_set) {
                if (std::isinf(info.lower) && info.lower < 0.0) {
                    lower = model::Bound::negative_infinity();
                } else {
                    lower = model::Bound::finite(info.lower);
                }
            } else {
                lower = model::Bound::finite(0.0);
            }

            model::Bound upper;
            if (info.upper_set) {
                if (std::isinf(info.upper) && info.upper > 0.0) {
                    upper = model::Bound::positive_infinity();
                } else {
                    upper = model::Bound::finite(info.upper);
                }
            } else {
                if (info.type == model::VariableType::binary) {
                    upper = model::Bound::finite(1.0);
                } else {
                    upper = model::Bound::positive_infinity();
                }
            }

            model.variable_lower.push_back(lower);
            model.variable_upper.push_back(upper);
        }

        if (!quad_terms_.empty()) {
            model::SparseMatrixBuilder q_builder(variables_.size(), variables_.size());
            for (const auto& qt : quad_terms_) {
                q_builder.add(qt.col1, qt.col2, qt.coeff);
                if (qt.col1 != qt.col2) {
                    q_builder.add(qt.col2, qt.col1, qt.coeff);
                }
            }
            model.has_quadratic_objective = true;
            model.quadratic_matrix = q_builder.build();
        }

        model.validate();
        return model;
    }
}
