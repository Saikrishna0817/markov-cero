#include "model_internal.hpp"
namespace markov_cero::qp {
using namespace detail_model;
QuadraticModel make_quadratic_model(const model::Model& model) {
    QuadraticModel qp;
    qp.name = model.name;
    qp.sense = model.objective_sense;
    qp.objective_offset = model.objective_offset;

    const std::size_t n = model.matrix.column_count;
    const std::size_t m_orig = model.matrix.row_count;
    const double sign = (model.objective_sense == model::ObjectiveSense::maximize) ? -1.0 : 1.0;

    // Linear objective q
    qp.q.resize(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
        if (j < model.objective.size()) {
            qp.q[j] = sign * model.objective[j];
        }
    }

    // Quadratic objective P (upper triangular)
    qp.P.dimension = n;
    qp.P.column_offsets.assign(n + 1, 0);
    if (model.has_quadratic_objective && model.quadratic_matrix.column_count == n) {
        std::vector<std::vector<std::pair<std::size_t, double>>> upper_entries(n);
        struct SymmetricPair {
            double upper{0.0};
            double lower{0.0};
            bool has_upper{false};
            bool has_lower{false};
        };
        std::map<std::pair<std::size_t, std::size_t>, SymmetricPair> symmetric_pairs;
        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t start = model.quadratic_matrix.column_start[j];
            const std::size_t end = model.quadratic_matrix.column_start[j + 1];
            for (std::size_t k = start; k < end; ++k) {
                const std::size_t i = model.quadratic_matrix.row_index[k];
                const double v = sign * model.quadratic_matrix.value[k];
                auto& pair = symmetric_pairs[{std::min(i, j), std::max(i, j)}];
                if (i <= j) {
                    pair.upper += v;
                    pair.has_upper = true;
                } else {
                    pair.lower += v;
                    pair.has_lower = true;
                }
            }
        }
        // Parsed QUADOBJ matrices contain a mirrored pair; QMATRIX inputs may
        // already be full symmetric or provide one triangle. Convert either
        // representation to the single upper-triangle value expected by
        // SparseSymmetricMatrix. Averaging a pair also gives the mathematically
        // relevant symmetric part when a supplied quadratic matrix is slightly
        // asymmetric, since x^T Q x = x^T (Q + Q^T)/2 x.
        for (const auto& [indices, pair] : symmetric_pairs) {
            double value = 0.0;
            if (pair.has_upper && pair.has_lower) {
                value = 0.5 * (pair.upper + pair.lower);
            } else {
                value = pair.has_upper ? pair.upper : pair.lower;
            }
            upper_entries[indices.second].emplace_back(indices.first, value);
        }
        for (std::size_t j = 0; j < n; ++j) {
            std::sort(upper_entries[j].begin(), upper_entries[j].end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
            // Merge duplicate entries
            for (const auto& [r, v] : upper_entries[j]) {
                if (!qp.P.row_indices.empty() && qp.P.row_indices.back() == r &&
                    qp.P.column_offsets[j] < qp.P.row_indices.size()) {
                    qp.P.values.back() += v;
                } else {
                    qp.P.row_indices.push_back(r);
                    qp.P.values.push_back(v);
                }
            }
            qp.P.column_offsets[j + 1] = qp.P.values.size();
        }
    }

    // Combined constraints: m = m_orig + n (general constraints + variable box bounds)
    const std::size_t m_total = m_orig + n;
    qp.l.resize(m_total);
    qp.u.resize(m_total);

    for (std::size_t i = 0; i < m_orig; ++i) {
        qp.l[i] = (i < model.row_lower.size() && model.row_lower[i].is_finite())
                      ? model.row_lower[i].value
                      : -std::numeric_limits<double>::infinity();
        qp.u[i] = (i < model.row_upper.size() && model.row_upper[i].is_finite())
                      ? model.row_upper[i].value
                      : std::numeric_limits<double>::infinity();
    }
    for (std::size_t j = 0; j < n; ++j) {
        qp.l[m_orig + j] = (j < model.variable_lower.size() && model.variable_lower[j].is_finite())
                               ? model.variable_lower[j].value
                               : -std::numeric_limits<double>::infinity();
        qp.u[m_orig + j] = (j < model.variable_upper.size() && model.variable_upper[j].is_finite())
                               ? model.variable_upper[j].value
                               : std::numeric_limits<double>::infinity();
    }

    // Combined constraint matrix A: [A_orig; I_n]
    qp.A.rows = m_total;
    qp.A.columns = n;
    qp.A.column_offsets.assign(n + 1, 0);
    for (std::size_t j = 0; j < n; ++j) {
        const std::size_t start = (j < model.matrix.column_start.size())
                                      ? model.matrix.column_start[j]
                                      : 0;
        const std::size_t end = (j + 1 < model.matrix.column_start.size())
                                    ? model.matrix.column_start[j + 1]
                                    : start;
        for (std::size_t k = start; k < end; ++k) {
            qp.A.row_indices.push_back(model.matrix.row_index[k]);
            qp.A.values.push_back(model.matrix.value[k]);
        }
        // Identity entry at row m_orig + j
        qp.A.row_indices.push_back(m_orig + j);
        qp.A.values.push_back(1.0);
        qp.A.column_offsets[j + 1] = qp.A.values.size();
    }

    qp.variable_types = model.variable_type;
    qp.variable_names = model.variable_name;
    qp.constraint_names = model.row_name;
    for (std::size_t j = 0; j < n; ++j) {
        std::string vname = (j < model.variable_name.size() && !model.variable_name[j].empty())
                                ? model.variable_name[j]
                                : ("x" + std::to_string(j));
        qp.constraint_names.push_back("bnd_" + vname);
    }
    return qp;
}
}
