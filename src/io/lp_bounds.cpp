#include "lp_parser_internal.hpp"
namespace markov_cero::io::detail_lp {
double LpParser::read_bound_val() {
        double sign = 1.0;
        if (has_more() && peek().text == "+") {
            next();
        } else if (has_more() && peek().text == "-") {
            sign = -1.0;
            next();
        }
        std::string s = to_lower(next().text);
        if (s == "inf" || s == "infinity") {
            return sign > 0 ? std::numeric_limits<double>::infinity()
                            : -std::numeric_limits<double>::infinity();
        }
        return sign * std::stod(s);
    }
void LpParser::parse_bounds() {
        next(); // skip 'Bounds'
        while (has_more() && !is_section_keyword(pos_)) {
            // Check if begins with bound value (number, sign, or inf)
            bool starts_val = false;
            if (peek().text == "+" || peek().text == "-") {
                if (pos_ + 1 < tokens_.size()) {
                    std::string nxt = to_lower(tokens_[pos_ + 1].text);
                    if (is_numeric_token(nxt) || nxt == "inf" || nxt == "infinity") {
                        starts_val = true;
                    }
                }
            } else {
                std::string cur = to_lower(peek().text);
                if (is_numeric_token(cur) || cur == "inf" || cur == "infinity") {
                    starts_val = true;
                }
            }

            if (starts_val) {
                double val1 = read_bound_val();
                if (!has_more() || (peek().text != "<=" && peek().text != ">=")) {
                    throw std::runtime_error("LP parser error in Bounds: expected operator after value");
                }
                std::string op1 = next().text;
                std::string var = next().text;
                std::size_t v_idx = get_or_create_var(var);

                if (has_more() && (peek().text == "<=" || peek().text == ">=")) {
                    std::string op2 = next().text;
                    double val2 = read_bound_val();
                    if (op1 == "<=" && op2 == "<=") {
                        variables_[v_idx].lower = val1;
                        variables_[v_idx].lower_set = true;
                        variables_[v_idx].upper = val2;
                        variables_[v_idx].upper_set = true;
                    } else if (op1 == ">=" && op2 == ">=") {
                        variables_[v_idx].upper = val1;
                        variables_[v_idx].upper_set = true;
                        variables_[v_idx].lower = val2;
                        variables_[v_idx].lower_set = true;
                    }
                } else {
                    if (op1 == "<=") { // val1 <= var => var >= val1
                        variables_[v_idx].lower = val1;
                        variables_[v_idx].lower_set = true;
                    } else { // val1 >= var => var <= val1
                        variables_[v_idx].upper = val1;
                        variables_[v_idx].upper_set = true;
                    }
                }
            } else {
                std::string var = next().text;
                std::size_t v_idx = get_or_create_var(var);

                if (has_more() && to_lower(peek().text) == "free") {
                    next();
                    variables_[v_idx].lower = -std::numeric_limits<double>::infinity();
                    variables_[v_idx].lower_set = true;
                    variables_[v_idx].upper = std::numeric_limits<double>::infinity();
                    variables_[v_idx].upper_set = true;
                } else if (has_more() && (peek().text == "<=" || peek().text == ">=" || peek().text == "==")) {
                    std::string op = next().text;
                    double val = read_bound_val();
                    if (op == "<=") {
                        variables_[v_idx].upper = val;
                        variables_[v_idx].upper_set = true;
                    } else if (op == ">=") {
                        variables_[v_idx].lower = val;
                        variables_[v_idx].lower_set = true;
                    } else { // "=="
                        variables_[v_idx].lower = val;
                        variables_[v_idx].lower_set = true;
                        variables_[v_idx].upper = val;
                        variables_[v_idx].upper_set = true;
                    }
                }
            }
        }
    }
void LpParser::parse_binaries() {
        next(); // skip 'Binary'
        while (has_more() && !is_section_keyword(pos_)) {
            std::string var = next().text;
            std::size_t v_idx = get_or_create_var(var);
            variables_[v_idx].type = model::VariableType::binary;
        }
    }
void LpParser::parse_generals() {
        next(); // skip 'General'
        while (has_more() && !is_section_keyword(pos_)) {
            std::string var = next().text;
            std::size_t v_idx = get_or_create_var(var);
            variables_[v_idx].type = model::VariableType::integer;
        }
    }
}
