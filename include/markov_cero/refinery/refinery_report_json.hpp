#pragma once

#include <iomanip>
#include <sstream>

namespace markov_cero::refinery {

namespace detail {

inline void write_string(std::ostream& out, const std::string& text) {
    out << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c == '\n') out << "\\n";
        else if (c == '\r') out << "\\r";
        else if (c == '\t') out << "\\t";
        else if (c < 0x20) {
            constexpr char hex[] = "0123456789abcdef";
            out << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
        }
        else out << c;
    }
    out << '"';
}

inline void write_number(std::ostream& out, double value) {
    if (std::isfinite(value)) out << std::setprecision(12) << value;
    else out << "null";
}

inline void write_margin(std::ostream& out, const SpecMargin& margin) {
    out << "{\"spec\":";
    write_string(out, margin.spec);
    out << ",\"unit\":";
    write_string(out, margin.unit);
    out << ",\"sense\":";
    write_string(out, margin.sense);
    out << ",\"limit\":";
    write_number(out, margin.limit);
    out << ",\"value\":";
    write_number(out, margin.value);
    out << ",\"margin\":";
    write_number(out, margin.margin);
    out << ",\"satisfied\":" << (margin.satisfied ? "true" : "false") << "}";
}

} // namespace detail

/// JSON document described in evidence/refinery-units-schema-20260928.json.
[[nodiscard]] inline std::string report_to_json(const RefineryReport& report) {
    std::ostringstream out;
    out << "{\"model\":";
    detail::write_string(out, report.model_name);
    out << ",\"status\":";
    detail::write_string(out, report.status);
    out << ",\"qualification\":";
    detail::write_string(out, report.qualification);
    out << ",\"objective\":{\"value\":";
    detail::write_number(out, report.objective);
    out << ",\"unit\":";
    detail::write_string(out, report.objective_unit);
    out << "},\"rows\":[";
    for (std::size_t i = 0; i < report.rows.size(); ++i) {
        const ReportRow& row = report.rows[i];
        if (i) out << ',';
        out << "{\"name\":";
        detail::write_string(out, row.name);
        out << ",\"unit\":";
        detail::write_string(out, row.unit);
        out << ",\"coefficient_unit\":";
        detail::write_string(out, row.coefficient_unit);
        out << ",\"dual_unit\":";
        detail::write_string(out, row.dual_unit);
        out << ",\"activity\":";
        detail::write_number(out, row.activity);
        out << ",\"lower_slack\":";
        detail::write_number(out, row.lower_slack);
        out << ",\"upper_slack\":";
        detail::write_number(out, row.upper_slack);
        out << ",\"dual\":";
        detail::write_number(out, row.dual);
        out << ",\"binding\":" << (row.binding ? "true" : "false") << "}";
    }
    out << "],\"variables\":[";
    for (std::size_t i = 0; i < report.variables.size(); ++i) {
        const ReportVariable& variable = report.variables[i];
        if (i) out << ',';
        out << "{\"name\":";
        detail::write_string(out, variable.name);
        out << ",\"unit\":";
        detail::write_string(out, variable.unit);
        out << ",\"value\":";
        detail::write_number(out, variable.value);
        out << ",\"objective_coefficient\":";
        detail::write_number(out, variable.objective_coefficient);
        out << ",\"objective_coefficient_unit\":";
        detail::write_string(out, variable.objective_coefficient_unit);
        out << '}';
    }
    out << "],\"active_bounds\":[";
    for (std::size_t i = 0; i < report.active_bounds.size(); ++i) {
        const ReportBound& bound = report.active_bounds[i];
        if (i) out << ',';
        out << "{\"name\":";
        detail::write_string(out, bound.name);
        out << ",\"variable\":";
        detail::write_string(out, bound.variable);
        out << ",\"unit\":";
        detail::write_string(out, bound.unit);
        out << ",\"side\":";
        detail::write_string(out, bound.side);
        out << ",\"value\":";
        detail::write_number(out, bound.value);
        out << ",\"reduced_cost\":";
        detail::write_number(out, bound.reduced_cost);
        out << ",\"reduced_cost_unit\":";
        detail::write_string(out, bound.reduced_cost_unit);
        out << '}';
    }
    out << "],\"quality_margins\":[";
    for (std::size_t i = 0; i < report.quality_margins.size(); ++i) {
        if (i) out << ',';
        detail::write_margin(out, report.quality_margins[i]);
    }
    out << "],\"iis\":[";
    for (std::size_t i = 0; i < report.iis.size(); ++i) {
        const IisEntry& entry = report.iis[i];
        if (i) out << ',';
        out << "{\"name\":";
        detail::write_string(out, entry.name);
        out << ",\"scope\":";
        detail::write_string(out, entry.scope);
        out << ",\"unit\":";
        detail::write_string(out, entry.unit);
        out << ",\"detail\":";
        detail::write_string(out, entry.detail);
        out << '}';
    }
    out << "],\"iis_scope_note\":";
    detail::write_string(out, report.iis_scope_note);
    out << ",\"unmodelled_specs\":[";
    for (std::size_t i = 0; i < report.unmodelled_specs.size(); ++i) {
        if (i) out << ',';
        detail::write_string(out, report.unmodelled_specs[i]);
    }
    out << "]}";
    return out.str();
}

} // namespace markov_cero::refinery
