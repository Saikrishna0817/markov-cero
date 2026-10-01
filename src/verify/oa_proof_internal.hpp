#pragma once

// MINLP-02 (docs/contracts/minlp-proof-replay.md §3/§6.5): verifier-side
// helpers shared by build and replay — convexity evidence, source
// re-derivation and the master assembly. This is deliberately NOT the
// solve-side detail_minlp_solver::build_master / hessian_convexity code: a
// defect in either assembly then fails closed (embedded fingerprint
// mismatch => rejected) instead of reproducing itself across the check.

#include "mip_proof_internal.hpp"
#include "markov_cero/verify/oa_proof.hpp"
#include "markov_cero/qp/model.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace markov_cero::verify::oa_detail {

using SymmetricEntries = std::map<std::pair<std::size_t, std::size_t>, double>;

// Mirrors the mathematical layout of a symmetric CSC matrix from unordered
// upper-triangle entries (contract O3; independent copy, see header comment).
inline qp::SparseSymmetricMatrix symmetric_matrix(std::size_t n,
                                                  const SymmetricEntries& entries) {
    qp::SparseSymmetricMatrix matrix;
    matrix.dimension = n;
    matrix.column_offsets.reserve(n + 1);
    matrix.column_offsets.push_back(0);
    std::vector<std::map<std::size_t, double>> columns(n);
    for (const auto& [ij, value] : entries) {
        if (value != 0.0) columns[ij.second][ij.first] += value;
    }
    for (std::size_t j = 0; j < n; ++j) {
        for (const auto& [i, value] : columns[j]) {
            if (value != 0.0) {
                matrix.row_indices.push_back(i);
                matrix.values.push_back(value);
            }
        }
        matrix.column_offsets.push_back(matrix.values.size());
    }
    return matrix;
}

inline void add_quadratic(SymmetricEntries& entries, std::size_t i, std::size_t j,
                          double contribution) {
    const auto key = std::make_pair(std::min(i, j), std::max(i, j));
    entries[key] += contribution;
}

// Objective Hessian: the QUADOBJ part through qp::make_quadratic_model (its
// P already carries the sense sign) plus sense-signed NLOBJ quadratic terms;
// c*x_i^2 contributes 2c on the diagonal, c*x_i*x_j contributes c off-diagonal.
inline SymmetricEntries objective_hessian(const model::Model& source) {
    SymmetricEntries entries;
    const auto qp_model = qp::make_quadratic_model(source);
    for (std::size_t j = 0; j < qp_model.P.dimension; ++j) {
        for (std::size_t k = qp_model.P.column_offsets[j]; k < qp_model.P.column_offsets[j + 1];
             ++k) {
            add_quadratic(entries, qp_model.P.row_indices[k], j, qp_model.P.values[k]);
        }
    }
    const double sign = source.objective_sense == model::ObjectiveSense::maximize ? -1.0 : 1.0;
    for (const auto& term : source.nlobj_terms) {
        if (term.quadratic)
            add_quadratic(entries, term.var0, term.var1, sign * term.coefficient);
    }
    return entries;
}

inline SymmetricEntries nlcon_hessian(const std::vector<model::NlobjTerm>& terms) {
    SymmetricEntries entries;
    for (const auto& term : terms) {
        if (term.quadratic) add_quadratic(entries, term.var0, term.var1, term.coefficient);
    }
    return entries;
}

// Contract O3: require positive_semidefinite for the objective (sense-signed)
// and every NLCON Hessian; returns minimum_pivot per Hessian in source order.
// Throws std::invalid_argument when any classification is not certified PSD.
inline std::vector<double> convexity_pivots(const model::Model& source) {
    const std::size_t n = source.matrix.column_count;
    std::vector<double> pivots;
    pivots.reserve(1 + source.nlcon_constraints.size());
    const auto assess = [&](const SymmetricEntries& entries, const std::string& what) {
        const auto report = qp::assess_convexity(symmetric_matrix(n, entries), 1e-10);
        if (report.status != qp::ConvexityStatus::positive_semidefinite) {
            throw std::invalid_argument("oa proof: " + what +
                                        " convexity could not be certified: " + report.message);
        }
        pivots.push_back(report.minimum_pivot);
    };
    assess(objective_hessian(source), "objective");
    for (const auto& constraint : source.nlcon_constraints)
        assess(nlcon_hessian(constraint.terms), "NLCON " + constraint.name);
    return pivots;
}

// Contract O4/O5: re-derive every stored cut from the source polynomial;
// malformed kinds/indexes/dimensions throw std::invalid_argument.
inline std::vector<minlp::OaCut> derive_all(const model::Model& source,
                                            const std::vector<minlp::OaCut>& cuts) {
    std::vector<minlp::OaCut> derived;
    derived.reserve(cuts.size());
    for (const auto& cut : cuts)
        derived.push_back(
            minlp::derive_oa_cut(source, cut.source_kind, cut.source_index, cut.point));
    return derived;
}

// Contract O5: rebuild the OA master from re-derived cuts, mirroring the
// solve-side layout from source data only — n+1 columns with free epigraph
// eta last, objective rows before constraint rows (partitioned by cut kind,
// order preserved), row/variable names, source bounds, integer marks.
inline model::Model assemble_master(const model::Model& source,
                                    const std::vector<minlp::OaCut>& cuts) {
    const std::size_t n = source.matrix.column_count;
    const std::size_t n_cols = n + 1;
    model::Model master;
    master.name = "minlp_oa_master";
    master.objective_sense = model::ObjectiveSense::minimize;
    master.objective.assign(n_cols, 0.0);
    master.objective[n] = 1.0;

    model::SparseMatrixBuilder builder(cuts.size(), n_cols);
    std::size_t row = 0;
    std::vector<double> rhs_by_row(cuts.size(), 0.0);
    for (const bool objective_pass : {true, false}) {
        for (const auto& cut : cuts) {
            const bool is_objective = cut.source_kind == minlp::OaCutSource::objective;
            if (is_objective != objective_pass) continue;
            for (std::size_t j = 0; j < n; ++j) {
                if (cut.gradient[j] != 0.0) builder.add(row, j, cut.gradient[j]);
            }
            if (is_objective) builder.add(row, n, -1.0);
            rhs_by_row[row] = cut.rhs;
            ++row;
        }
    }
    master.matrix = builder.build();

    master.row_lower.assign(cuts.size(), model::Bound::negative_infinity());
    master.row_upper.resize(cuts.size());
    for (std::size_t k = 0; k < cuts.size(); ++k)
        master.row_upper[k] = model::Bound::finite(rhs_by_row[k]);

    master.variable_lower.resize(n_cols);
    master.variable_upper.resize(n_cols);
    master.variable_type.assign(n_cols, model::VariableType::continuous);
    for (std::size_t j = 0; j < n; ++j) {
        master.variable_lower[j] = source.variable_lower[j].is_finite()
                                       ? model::Bound::finite(source.variable_lower[j].value)
                                       : model::Bound::negative_infinity();
        master.variable_upper[j] = source.variable_upper[j].is_finite()
                                       ? model::Bound::finite(source.variable_upper[j].value)
                                       : model::Bound::positive_infinity();
        if (source.variable_type[j] != model::VariableType::continuous)
            master.variable_type[j] = model::VariableType::integer;
    }
    master.variable_lower[n] = model::Bound::negative_infinity();
    master.variable_upper[n] = model::Bound::positive_infinity();

    master.row_name.resize(cuts.size());
    for (std::size_t k = 0; k < cuts.size(); ++k)
        master.row_name[k] = "oa_row_" + std::to_string(k);
    master.variable_name.resize(n_cols);
    for (std::size_t j = 0; j < n; ++j) master.variable_name[j] = "x" + std::to_string(j);
    master.variable_name[n] = "eta";
    return master;
}

// Contract §4.3: derive + assemble in one step for both sides.
inline model::Model rebuild_master(const model::Model& source,
                                   const std::vector<minlp::OaCut>& cuts) {
    return assemble_master(source, derive_all(source, cuts));
}

inline double sense_sign(const model::Model& source) {
    return source.objective_sense == model::ObjectiveSense::maximize ? -1.0 : 1.0;
}

} // namespace markov_cero::verify::oa_detail
