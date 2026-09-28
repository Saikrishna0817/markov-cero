#include "lp_parser_internal.hpp"
namespace markov_cero::io::detail_lp {
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
        if (tokens.size() >= 1000000) throw std::length_error("LP token limit exceeded");
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
}
