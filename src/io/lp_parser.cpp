#include "lp_parser_internal.hpp"
namespace markov_cero::io::detail_lp {
model::Model LpParser::parse() {
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
                if (has_more()) throw std::runtime_error("LP parser: data after End");
                break;
            } else {
                throw std::runtime_error("LP parser error at line " + std::to_string(peek().line) +
                                         ": unexpected section keyword '" + peek().text + "'");
            }
        }

        return build_model();
    }
[[nodiscard]] bool LpParser::has_more() const noexcept { return pos_ < tokens_.size(); }
[[nodiscard]] const Token& LpParser::peek() const {
        if (!has_more()) throw std::runtime_error("LP parser error: unexpected end of tokens");
        return tokens_[pos_];
    }
Token LpParser::next() {
        if (!has_more()) throw std::runtime_error("LP parser error: unexpected end of tokens");
        return tokens_[pos_++];
    }
[[nodiscard]] std::string LpParser::peek_lower() const { return to_lower(peek().text); }
[[nodiscard]] bool LpParser::is_section_keyword(std::size_t p) const {
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
std::size_t LpParser::get_or_create_var(const std::string& name) {
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
}
namespace markov_cero::io {
using namespace detail_lp;
model::Model parse_lp_string(const std::string& content) {
    if (content.size() > 16U * 1024U * 1024U) throw std::length_error("LP input exceeds 16 MiB");
    std::size_t line_size = 0;
    for (unsigned char c : content) {
        if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c >= 127)
            throw std::invalid_argument("LP input must use printable ASCII names");
        if (c == '\n') line_size = 0;
        else if (++line_size > 65536) throw std::length_error("LP line exceeds 64 KiB");
    }
    auto tokens = tokenize_lp(content);
    LpParser parser(std::move(tokens));
    return parser.parse();
}
model::Model parse_lp_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open LP file: " + path);
    }
    std::string content;
    char chunk[8192];
    while (file.read(chunk, sizeof(chunk)) || file.gcount()) {
        if (content.size() + static_cast<std::size_t>(file.gcount()) > 16U * 1024U * 1024U)
            throw std::length_error("LP input exceeds 16 MiB");
        content.append(chunk, static_cast<std::size_t>(file.gcount()));
    }
    if (file.bad()) throw std::runtime_error("LP input read failed");
    return parse_lp_string(content);
}
}
