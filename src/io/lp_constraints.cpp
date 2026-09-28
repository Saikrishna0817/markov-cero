#include "lp_parser_internal.hpp"
namespace markov_cero::io::detail_lp {
void LpParser::parse_constraints_header() {
        if (!has_more()) throw std::runtime_error("LP parser error: missing constraints section");
        std::string s = peek_lower();
        if (s == "subject") {
            next();
            if (has_more() && peek_lower() == "to") next();
        } else if (s == "such") {
            next();
            if (has_more() && peek_lower() == "that") next();
        } else if (s == "s.t." || s == "st") {
            next();
        } else {
            throw std::runtime_error("LP parser error at line " + std::to_string(peek().line) +
                                     ": expected 'Subject To', got '" + peek().text + "'");
        }
    }
void LpParser::parse_constraints() {
        while (has_more() && !is_section_keyword(pos_)) {
            std::string r_name;
            if (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].text == ":") {
                r_name = tokens_[pos_].text;
                pos_ += 2;
            } else {
                r_name = "c" + std::to_string(row_names_.size() + 1);
            }

            std::vector<std::pair<std::size_t, double>> terms;
            double lhs_constant = 0.0;

            while (has_more() && peek().text != "<=" && peek().text != ">=" &&
                   peek().text != "==" && !is_section_keyword(pos_)) {
                double sign = 1.0;
                if (peek().text == "+") {
                    next();
                } else if (peek().text == "-") {
                    sign = -1.0;
                    next();
                }

                if (!has_more() || peek().text == "<=" || peek().text == ">=" ||
                    peek().text == "==" || is_section_keyword(pos_)) {
                    break;
                }

                if (is_numeric_token(peek().text)) {
                    double val = std::stod(next().text) * sign;
                    if (has_more() && peek().text != "<=" && peek().text != ">=" &&
                        peek().text != "==" && peek().text != "+" && peek().text != "-" &&
                        !is_section_keyword(pos_)) {
                        std::string var = next().text;
                        terms.push_back({get_or_create_var(var), val});
                    } else {
                        lhs_constant += val;
                    }
                } else {
                    std::string var = next().text;
                    terms.push_back({get_or_create_var(var), 1.0 * sign});
                }
            }

            if (!has_more() || (peek().text != "<=" && peek().text != ">=" && peek().text != "==")) {
                throw std::runtime_error("LP parser error at line " + std::to_string(peek().line) +
                                         ": expected comparison operator in constraint " + r_name);
            }

            std::string op = next().text;

            double rhs_sign = 1.0;
            if (has_more() && peek().text == "+") {
                next();
            } else if (has_more() && peek().text == "-") {
                rhs_sign = -1.0;
                next();
            }

            if (!has_more() || !is_numeric_token(peek().text)) {
                throw std::runtime_error("LP parser error: expected numeric RHS value");
            }
            double rhs = std::stod(next().text) * rhs_sign - lhs_constant;

            std::size_t r_idx = row_names_.size();
            row_names_.push_back(r_name);

            if (op == "<=") {
                row_lower_.push_back(model::Bound::negative_infinity());
                row_upper_.push_back(model::Bound::finite(rhs));
            } else if (op == ">=") {
                row_lower_.push_back(model::Bound::finite(rhs));
                row_upper_.push_back(model::Bound::positive_infinity());
            } else { // "=="
                row_lower_.push_back(model::Bound::finite(rhs));
                row_upper_.push_back(model::Bound::finite(rhs));
            }

            for (const auto& [col, val] : terms) {
                matrix_entries_.push_back({r_idx, col, val});
            }
        }
    }
}
