#include "lp_parser_internal.hpp"
namespace markov_cero::io::detail_lp {
void LpParser::parse_objective_header() {
        std::string s = peek_lower();
        if (s == "minimize" || s == "min") {
            sense_ = model::ObjectiveSense::minimize;
            next();
        } else if (s == "maximize" || s == "max") {
            sense_ = model::ObjectiveSense::maximize;
            next();
        } else {
            throw std::runtime_error("LP parser error at line " + std::to_string(peek().line) +
                                     ": expected 'Minimize' or 'Maximize', got '" + peek().text + "'");
        }

        // Optional objective name like "obj:" or "cost:"
        if (has_more() && pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].text == ":") {
            model_name_ = tokens_[pos_].text;
            pos_ += 2;
        }
    }
void LpParser::parse_objective() {
        while (has_more() && !is_section_keyword(pos_)) {
            if (peek().text == "[") {
                parse_quadratic_objective();
                continue;
            }

            double sign = 1.0;
            if (peek().text == "+") {
                next();
            } else if (peek().text == "-") {
                sign = -1.0;
                next();
            }

            if (!has_more() || is_section_keyword(pos_)) break;

            if (peek().text == "[") {
                parse_quadratic_objective();
                continue;
            }

            if (is_numeric_token(peek().text)) {
                double val = std::stod(next().text) * sign;
                if (has_more() && !is_section_keyword(pos_) &&
                    peek().text != "+" && peek().text != "-" && peek().text != "[") {
                    std::string var = next().text;
                    std::size_t v_idx = get_or_create_var(var);
                    objective_coeffs_[v_idx] += val;
                } else {
                    objective_offset_ += val;
                }
            } else {
                std::string var = next().text;
                std::size_t v_idx = get_or_create_var(var);
                objective_coeffs_[v_idx] += (1.0 * sign);
            }
        }
    }
void LpParser::parse_quadratic_objective() {
        next(); // skip '['
        while (has_more() && peek().text != "]") {
            double sign = 1.0;
            if (peek().text == "+") {
                next();
            } else if (peek().text == "-") {
                sign = -1.0;
                next();
            }
            if (peek().text == "]") break;

            double coeff = 1.0;
            if (is_numeric_token(peek().text)) {
                coeff = std::stod(next().text);
            }
            coeff *= sign;

            std::string var1 = next().text;
            std::string var2 = var1;
            if (has_more() && peek().text == "*") {
                next(); // skip '*'
                var2 = next().text;
            } else if (has_more() && peek().text == "^") {
                next(); // skip '^'
                if (has_more() && peek().text == "2") {
                    next();
                }
            }

            std::size_t c1 = get_or_create_var(var1);
            std::size_t c2 = get_or_create_var(var2);
            quad_terms_.push_back({c1, c2, coeff});
        }
        if (has_more() && peek().text == "]") {
            next();
        }
        // Optional "/ 2"
        if (has_more() && peek().text == "/") {
            next();
            if (has_more() && peek().text == "2") {
                next();
            }
        }
    }
}
