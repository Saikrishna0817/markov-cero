#include "markov_cero/io/lp_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace markov_cero::io {
namespace {

struct Token {
    std::string text;
    std::size_t line{1};
};

std::string to_lower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::vector<Token> tokenize_lp(const std::string& content) {
    std::vector<Token> tokens;
    const std::size_t n = content.size();
    std::size_t i = 0;
    std::size_t line = 1;

    while (i < n) {
        char c = content[i];

        if (c == '\n') {
            ++line;
            ++i;
            continue;
        }
        if (c == '\r' || std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        // Comment: \ to end-of-line
        if (c == '\\') {
            ++i;
            while (i < n && content[i] != '\n') {
                ++i;
            }
            continue;
        }

        // Single punctuation tokens
        if (c == ':') {
            tokens.push_back({":", line});
            ++i;
            continue;
        }
        if (c == '[') {
            tokens.push_back({"[", line});
            ++i;
            continue;
        }
        if (c == ']') {
            tokens.push_back({"]", line});
            ++i;
            continue;
        }
        if (c == '*') {
            tokens.push_back({"*", line});
            ++i;
            continue;
        }
        if (c == '^') {
            tokens.push_back({"^", line});
            ++i;
            continue;
        }
        if (c == '/') {
            tokens.push_back({"/", line});
            ++i;
            continue;
        }

        // Relational operators
        if (c == '<') {
            if (i + 1 < n && content[i + 1] == '=') {
                tokens.push_back({"<=", line});
                i += 2;
            } else {
                tokens.push_back({"<=", line}); // In LP, '<' means '<='
                ++i;
            }
            continue;
        }
        if (c == '>') {
            if (i + 1 < n && content[i + 1] == '=') {
                tokens.push_back({">=", line});
                i += 2;
            } else {
                tokens.push_back({">=", line}); // In LP, '>' means '>='
                ++i;
            }
            continue;
        }
        if (c == '=') {
            if (i + 1 < n && content[i + 1] == '=') {
                tokens.push_back({"==", line});
                i += 2;
            } else {
                tokens.push_back({"==", line});
                ++i;
            }
            continue;
        }

        // Signs (+ and -) are tokens
        if (c == '+' || c == '-') {
            tokens.push_back({std::string(1, c), line});
            ++i;
            continue;
        }

        // Numbers or Identifiers
        // Check if starting a number: digit, or '.' followed by digit
        bool is_num = std::isdigit(static_cast<unsigned char>(c)) ||
                      (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(content[i + 1])));

        std::size_t start = i;
        if (is_num) {
            while (i < n && (std::isdigit(static_cast<unsigned char>(content[i])) || content[i] == '.')) {
                ++i;
            }
            // Scientific notation: e or E followed by optional + or - and digits
            if (i < n && (content[i] == 'e' || content[i] == 'E')) {
                std::size_t j = i + 1;
                if (j < n && (content[j] == '+' || content[j] == '-')) {
                    ++j;
                }
                if (j < n && std::isdigit(static_cast<unsigned char>(content[j]))) {
                    while (j < n && std::isdigit(static_cast<unsigned char>(content[j]))) {
                        ++j;
                    }
                    i = j;
                }
            }
            tokens.push_back({content.substr(start, i - start), line});
        } else {
            // Identifier / Word: read until whitespace or delimiter
            while (i < n && !std::isspace(static_cast<unsigned char>(content[i])) &&
                   content[i] != '\\' && content[i] != ':' && content[i] != '<' &&
                   content[i] != '>' && content[i] != '=' && content[i] != '+' &&
                   content[i] != '-' && content[i] != '[' && content[i] != ']' &&
                   content[i] != '*' && content[i] != '^' && content[i] != '/') {
                ++i;
            }
            tokens.push_back({content.substr(start, i - start), line});
        }
    }
    return tokens;
}

bool is_numeric_token(const std::string& s) {
    if (s.empty()) return false;
    if (std::isdigit(static_cast<unsigned char>(s[0]))) return true;
    if (s[0] == '.' && s.size() > 1 && std::isdigit(static_cast<unsigned char>(s[1]))) return true;
    return false;
}

struct VarInfo {
    std::size_t index{};
    std::string name;
    model::VariableType type{model::VariableType::continuous};
    bool lower_set{false};
    double lower{0.0};
    bool upper_set{false};
    double upper{std::numeric_limits<double>::infinity()};
};

struct QuadTerm {
    std::size_t col1;
    std::size_t col2;
    double coeff;
};

class LpParser {
public:
    explicit LpParser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    model::Model parse() {
        if (tokens_.empty()) {
            throw std::runtime_error("LP parser error: input is empty");
        }

        parse_objective_header();
        parse_objective();
        parse_constraints_header();
        parse_constraints();

        while (has_more()) {
            std::string sec = peek_lower();
            if (sec == "bounds" || sec == "bound") {
                parse_bounds();
            } else if (sec == "binary" || sec == "binaries" || sec == "bin") {
                parse_binaries();
            } else if (sec == "general" || sec == "generals" || sec == "gen" ||
                       sec == "integer" || sec == "integers" || sec == "int") {
                parse_generals();
            } else if (sec == "end") {
                next();
                break;
            } else {
                throw std::runtime_error("LP parser error at line " + std::to_string(peek().line) +
                                         ": unexpected section keyword '" + peek().text + "'");
            }
        }

        return build_model();
    }

private:
    std::vector<Token> tokens_;
    std::size_t pos_{0};

    std::string model_name_{"lp_problem"};
    model::ObjectiveSense sense_{model::ObjectiveSense::minimize};
    double objective_offset_{0.0};

    std::vector<VarInfo> variables_;
    std::unordered_map<std::string, std::size_t> var_map_;
    std::vector<double> objective_coeffs_;
    std::vector<QuadTerm> quad_terms_;

    std::vector<std::string> row_names_;
    std::vector<model::Bound> row_lower_;
    std::vector<model::Bound> row_upper_;

    struct CoeffEntry {
        std::size_t row;
        std::size_t col;
        double val;
    };
    std::vector<CoeffEntry> matrix_entries_;

    [[nodiscard]] bool has_more() const noexcept { return pos_ < tokens_.size(); }
    [[nodiscard]] const Token& peek() const {
        if (!has_more()) throw std::runtime_error("LP parser error: unexpected end of tokens");
        return tokens_[pos_];
    }
    Token next() {
        if (!has_more()) throw std::runtime_error("LP parser error: unexpected end of tokens");
        return tokens_[pos_++];
    }
    [[nodiscard]] std::string peek_lower() const { return to_lower(peek().text); }

    [[nodiscard]] bool is_section_keyword(std::size_t p) const {
        if (p >= tokens_.size()) return false;
        std::string s = to_lower(tokens_[p].text);
        if (s == "subject" && p + 1 < tokens_.size() && to_lower(tokens_[p + 1].text) == "to") return true;
        if (s == "such" && p + 1 < tokens_.size() && to_lower(tokens_[p + 1].text) == "that") return true;
        if (s == "s.t." || s == "st") return true;
        if (s == "bounds" || s == "bound") return true;
        if (s == "binary" || s == "binaries" || s == "bin") return true;
        if (s == "general" || s == "generals" || s == "gen") return true;
        if (s == "integer" || s == "integers" || s == "int") return true;
        if (s == "end") return true;
        return false;
    }

    std::size_t get_or_create_var(const std::string& name) {
        auto it = var_map_.find(name);
        if (it != var_map_.end()) return it->second;
        std::size_t idx = variables_.size();
        VarInfo info;
        info.index = idx;
        info.name = name;
        variables_.push_back(info);
        var_map_[name] = idx;
        objective_coeffs_.push_back(0.0);
        return idx;
    }

    void parse_objective_header() {
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

    void parse_objective() {
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

    void parse_quadratic_objective() {
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

    void parse_constraints_header() {
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

    void parse_constraints() {
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

    double read_bound_val() {
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

    void parse_bounds() {
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

    void parse_binaries() {
        next(); // skip 'Binary'
        while (has_more() && !is_section_keyword(pos_)) {
            std::string var = next().text;
            std::size_t v_idx = get_or_create_var(var);
            variables_[v_idx].type = model::VariableType::binary;
        }
    }

    void parse_generals() {
        next(); // skip 'General'
        while (has_more() && !is_section_keyword(pos_)) {
            std::string var = next().text;
            std::size_t v_idx = get_or_create_var(var);
            variables_[v_idx].type = model::VariableType::integer;
        }
    }

    model::Model build_model() {
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
};

} // namespace

model::Model parse_lp_string(const std::string& content) {
    auto tokens = tokenize_lp(content);
    LpParser parser(std::move(tokens));
    return parser.parse();
}

model::Model parse_lp_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open LP file: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return parse_lp_string(content);
}

} // namespace markov_cero::io
