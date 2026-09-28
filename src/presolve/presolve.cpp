#include "workspace.hpp"
namespace markov_cero::presolve::detail {
void ensure_finite(double v, const char* message) {
    if (!std::isfinite(v)) {
        throw std::overflow_error(message);
    }
}

std::uint64_t hash_mix(std::uint64_t h, std::uint64_t v) {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

std::uint64_t value_key(double v) {
    return std::bit_cast<std::uint64_t>(v + 0.0);  // normalize -0.0
}

bool Workspace::initialize() {

    input.validate();


    result.status = lp::reference::SolveStatus::optimal;
    result.statistics.original_rows = input.matrix.rows;
    result.statistics.original_cols = input.matrix.columns;
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.model = input;
        result.message = "presolve wall-clock deadline reached before reduction";
        return false;
    }


    row_active.assign(m, true);
    col_active.assign(n, true);

    cols.resize(n);
    for (std::size_t j = 0; j < n; ++j) {
        const std::size_t start = input.matrix.column_offsets[j];
        const std::size_t end = input.matrix.column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            cols[j].push_back({input.matrix.row_indices[k], input.matrix.values[k]});
        }
    }

    rows.resize(m);
    for (std::size_t j = 0; j < n; ++j) {
        for (const auto& ce : cols[j]) {
            rows[ce.row].push_back({j, ce.val});
        }
    }

    rhs = input.rhs;
    obj = input.objective;
    obj_offset = input.objective_offset;

return true;
}
PresolveResult Workspace::run() {
if (!initialize()) return result;
    std::size_t pass = 0;
    for (; pass < options.max_passes; ++pass) {
        if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.model = input;
            result.message = "presolve wall-clock deadline reached between passes";
            result.statistics.passes_executed = pass;
            return result;
        }
        reductions = 0;

        if (!eliminate_empty_and_singletons() || !eliminate_duplicates()) return result;
        if (reductions == 0) {
            break;
        }
    }
    result.statistics.passes_executed = pass;

return compact();
}
}
namespace markov_cero::presolve {
PresolveResult presolve(const transform::SparseCanonicalModel& input, const PresolveOptions& options) { return detail::Workspace(input, options).run(); }
}
