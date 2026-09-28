#include "mps_internal.hpp"
#include <cctype>
#include <sstream>
#include <utility>

namespace markov_cero::io::detail {
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}
std::vector<std::string> tokens(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> out;
    for (std::string value; in >> value;)
        out.push_back(value);
    return out;
}
std::string unquote(std::string value) {
    if (value.size() >= 2U && ((value.front() == '\'' && value.back() == '\'') ||
                               (value.front() == '"' && value.back() == '"')))
        return value.substr(1U, value.size() - 2U);
    return value;
}
double number(const std::string& text, std::size_t line) {
    std::string normalized = text;
    std::replace(normalized.begin(), normalized.end(), 'D', 'E');
    std::replace(normalized.begin(), normalized.end(), 'd', 'e');
    std::size_t used = 0U;
    double value = 0.0;
    try {
        value = std::stod(normalized, &used);
    } catch (...) {
        throw MpsError(line, "invalid numeric token: " + text);
    }
    if (used != normalized.size() || !std::isfinite(value))
        throw MpsError(line, "numeric token must be finite: " + text);
    return value;
}
bool header(const std::string& token) {
    static const std::vector<std::string> names{"NAME", "OBJSENSE", "OBJNAME", "ROWS",
                                                "COLUMNS", "RHS", "RANGES", "BOUNDS",
                                                "QUADOBJ", "QMATRIX", "NLOBJ", "NLCON", "ENDATA"};
    return std::find(names.begin(), names.end(), token) != names.end();
}
    void Parser::require_name(const std::string& name) {
        if (name.empty() || name.size() > limits.maximum_name_bytes)
            throw MpsResourceLimitError(line_number, "name byte limit exceeded");
        const bool printable_ascii = std::all_of(name.begin(), name.end(), [](unsigned char value) {
            return value >= 33U && value <= 126U;
        });
        if (!printable_ascii)
            throw MpsError(line_number, "names must use printable ASCII without spaces");
    }
    std::size_t Parser::find_row(const std::string& name) {
        const auto it = row_by_name.find(name);
        if (it == row_by_name.end())
            throw MpsError(line_number, "unknown row: " + name);
        return it->second;
    }
    std::size_t Parser::find_or_add_column(const std::string& name) {
        require_name(name);
        const auto found = column_by_name.find(name);
        if (found != column_by_name.end())
            return found->second;
        if (columns.size() >= limits.maximum_columns)
            throw MpsResourceLimitError(line_number, "column limit exceeded");
        const auto index = columns.size();
        Column column;
        column.name = name;
        if (in_integer_block) {
            column.type = model::VariableType::integer;
            column.upper = model::Bound::finite(1.0);
            column.marker_upper_default = true;
        }
        columns.push_back(column);
        column_by_name.emplace(name, index);
        return index;
    }


bool Parser::read_line(std::istream& input, std::string& raw) {
    raw.clear();
    char ch;
    bool any = false;
    while (input.get(ch)) {
        any = true;
        if (bytes >= limits.maximum_bytes)
            throw MpsResourceLimitError(line_number + 1, "byte limit exceeded");
        ++bytes;
        if (ch == '\n') break;
        if (raw.size() >= limits.maximum_line_bytes)
            throw MpsResourceLimitError(line_number + 1, "line byte limit exceeded");
        raw.push_back(ch);
    }
    return any;
}
model::Model Parser::read(std::istream& input) {
    for (std::string raw; read_line(input, raw);) {
        if (++line_number > limits.maximum_lines)
            throw MpsResourceLimitError(line_number, "line limit exceeded");
        if (!raw.empty() && raw.back() == '\r') {
            raw.pop_back();
        }
        const std::string line = trim(raw);
        if (line.empty() || line.front() == '*')
            continue;
        const auto fields = tokens(line);
        if (fields.empty())
            continue;
        std::string first = fields.front();
        std::transform(first.begin(), first.end(), first.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        if (saw_end)
            throw MpsError(line_number, "content after ENDATA");
        const bool starts_in_col1 = (!raw.empty() && raw.front() != ' ' && raw.front() != '\t');
        if (starts_in_col1) {
            if (!header(first))
                throw MpsError(line_number, "unknown section: " + first);
            if (first != "NAME" && fields.size() != 1U)
                throw MpsError(line_number, "section header takes no values");
            if (first == "NAME") {
                section = Section::none;
                if (fields.size() >= 2U) {
                    require_name(fields[1]);
                    problem_name = fields[1];
                }
            } else if (first == "OBJSENSE")
                section = Section::objective_sense;
            else if (first == "OBJNAME")
                section = Section::objective_name;
            else if (first == "ROWS")
                section = Section::rows;
            else if (first == "COLUMNS")
                section = Section::columns;
            else if (first == "RHS")
                section = Section::rhs;
            else if (first == "RANGES")
                section = Section::ranges;
            else if (first == "BOUNDS")
                section = Section::bounds;
            else if (first == "QUADOBJ")
                section = Section::quadobj;
            else if (first == "QMATRIX")
                section = Section::qmatrix;
            else if (first == "NLOBJ")
                section = Section::nlobj;
            else if (first == "NLCON")
                section = Section::nlcon;
            else {
                section = Section::end;
                saw_end = true;
            }
            continue;
        }
        record(fields);
    }
    if (input.bad()) throw MpsError(line_number, "input read failure");
    return build();
}
} // namespace markov_cero::io::detail
namespace markov_cero::io {
MpsError::MpsError(std::size_t line, std::string message)
    : std::runtime_error("MPS line " + std::to_string(line) + ": " + std::move(message)),
      line_(line) {}
std::size_t MpsError::line() const noexcept { return line_; }
MpsResourceLimitError::MpsResourceLimitError(std::size_t line, std::string message)
    : std::length_error("MPS line " + std::to_string(line) + ": " + std::move(message)) {}

model::Model parse_mps(std::istream& input, const MpsLimits& limits) {
    if (!limits.maximum_bytes || !limits.maximum_lines || !limits.maximum_name_bytes ||
        !limits.maximum_line_bytes)
        throw std::invalid_argument("MPS byte, line and name limits must be positive");
    return detail::Parser(limits).read(input);
}
model::Model parse_mps_string(std::string_view input, const MpsLimits& limits) {
    if (input.size() > limits.maximum_bytes)
        throw MpsResourceLimitError(0U, "byte limit exceeded");
    std::istringstream stream{std::string(input)};
    return parse_mps(stream, limits);
}
} // namespace markov_cero::io
