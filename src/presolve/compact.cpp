#include "workspace.hpp"
namespace markov_cero::presolve::detail {
PresolveResult Workspace::compact() {
    // Compact remaining active rows and columns
    std::vector<std::size_t> presolved_to_orig_row;
    std::vector<std::size_t> orig_to_presolved_row(m, static_cast<std::size_t>(-1));
    for (std::size_t i = 0; i < m; ++i) {
        if (row_active[i]) {
            orig_to_presolved_row[i] = presolved_to_orig_row.size();
            presolved_to_orig_row.push_back(i);
        }
    }

    std::vector<std::size_t> presolved_to_orig_col;
    std::vector<std::size_t> orig_to_presolved_col(n, static_cast<std::size_t>(-1));
    for (std::size_t j = 0; j < n; ++j) {
        if (col_active[j]) {
            orig_to_presolved_col[j] = presolved_to_orig_col.size();
            presolved_to_orig_col.push_back(j);
        }
    }

    result.stack.set_row_map(presolved_to_orig_row);
    result.stack.set_col_map(presolved_to_orig_col);

    const std::size_t new_m = presolved_to_orig_row.size();
    const std::size_t new_n = presolved_to_orig_col.size();
    result.statistics.presolved_rows = new_m;
    result.statistics.presolved_cols = new_n;

    // Build the compacted model
    result.model.matrix.rows = new_m;
    result.model.matrix.columns = new_n;
    result.model.rhs.resize(new_m);
    for (std::size_t new_i = 0; new_i < new_m; ++new_i) {
        result.model.rhs[new_i] = rhs[presolved_to_orig_row[new_i]];
    }

    result.model.objective.resize(new_n);
    for (std::size_t new_j = 0; new_j < new_n; ++new_j) {
        result.model.objective[new_j] = obj[presolved_to_orig_col[new_j]];
    }

    result.model.objective_offset = obj_offset;
    result.model.record = input.record;
    // AP-9 fix: the compaction drops canonical columns, so the variable map's
    // canonical_index entries must be remapped into the compacted space or
    // validate()'s range check (canonical_index < matrix.columns) rejects the
    // presolved model outright. Every column-elimination rule folds its
    // contribution into the presolved model (objective offset / rhs), so a
    // variable-map entry that references ONLY eliminated columns is a constant
    // in the presolved space: it is dropped from the map and its contribution
    // is already accounted for. Entries that reference surviving columns are
    // remapped index-by-index. The map is consumed only by reconstruction on
    // the ORIGINAL canonical model (postsolve and record of the input are
    // untouched), so this remap cannot affect solution fidelity.
    {
        std::vector<std::size_t> orig_to_presolved_col(
            n, static_cast<std::size_t>(-1));
        for (std::size_t new_j = 0; new_j < new_n; ++new_j) {
            orig_to_presolved_col[presolved_to_orig_col[new_j]] = new_j;
        }
        for (auto& vm : result.model.record.variables) {
            std::vector<std::size_t> remapped_index;
            std::vector<double> remapped_mult;
            remapped_index.reserve(vm.canonical_index.size());
            remapped_mult.reserve(vm.multiplier.size());
            for (std::size_t q = 0; q < vm.canonical_index.size(); ++q) {
                const std::size_t orig_idx = vm.canonical_index[q];
                if (orig_to_presolved_col[orig_idx] != static_cast<std::size_t>(-1)) {
                    remapped_index.push_back(orig_to_presolved_col[orig_idx]);
                    remapped_mult.push_back(vm.multiplier[q]);
                }
            }
            vm.canonical_index = std::move(remapped_index);
            vm.multiplier = std::move(remapped_mult);
        }
        // Canonical columns are ordered [structural..., slack...] and the
        // compaction preserves order, so the retained structural count is the
        // number of retained columns below the original structural count.
        std::size_t retained_structural = 0;
        for (std::size_t new_j = 0; new_j < new_n; ++new_j) {
            if (presolved_to_orig_col[new_j] < input.record.structural_variables) {
                ++retained_structural;
            }
        }
        result.model.record.structural_variables = retained_structural;
    }

    result.model.matrix.column_offsets.assign(new_n + 1, 0);
    std::size_t offset_count = 0;
    for (std::size_t new_j = 0; new_j < new_n; ++new_j) {
        result.model.matrix.column_offsets[new_j] = offset_count;
        const std::size_t orig_j = presolved_to_orig_col[new_j];
        for (const auto& ce : cols[orig_j]) {
            if (row_active[ce.row] && std::abs(ce.val) > options.pivot_tolerance) {
                const std::size_t new_i = orig_to_presolved_row[ce.row];
                result.model.matrix.row_indices.push_back(new_i);
                result.model.matrix.values.push_back(ce.val);
                ++offset_count;
            }
        }
    }
    result.model.matrix.column_offsets[new_n] = offset_count;

    if (new_m > 0 && new_n > 0) {
        result.model.validate();
    }
    result.message = "presolve complete";
    return result;
}
}
