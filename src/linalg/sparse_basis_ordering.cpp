#include "sparse_basis_internal.hpp"
namespace markov_cero::linalg {
using namespace detail_sparse_basis;
namespace detail_sparse_basis {
void require_finite(double v, const char* message) {
    if (!std::isfinite(v))
        throw std::invalid_argument(message);
}
}

namespace detail_sparse_basis {
std::size_t count_nonzero(const std::vector<double>& v) {
    return static_cast<std::size_t>(
        std::count_if(v.begin(), v.end(), [](double x) { return x != 0; }));
}
}

namespace detail_sparse_basis {
std::vector<std::size_t> minimum_degree_column_order(
    std::size_t n, const std::vector<std::vector<std::size_t>>& column_rows) {
    std::vector<std::size_t> identity(n);
    for (std::size_t i = 0; i < n; ++i)
        identity[i] = i;
    if (n < 32 || n > kMaxOrderingDimension)
        return identity;

    std::size_t edges = 0;
    for (const auto& rows : column_rows)
        edges += rows.size();
    if (static_cast<double>(edges) > kMaxOrderingDensity * static_cast<double>(n) * n)
        return identity;  // dense basis: fill is unavoidable, ordering is pure cost

    // Dense adjacency matrix (n <= 2048 keeps this <= 4 MB) with degree counters;
    // the greedy pick is O(n) and each elimination merges the neighbourhood.
    std::vector<std::vector<char>> adjacent(n, std::vector<char>(n, 0));
    std::vector<std::size_t> degree(n, 0);
    // Build adjacency by bucketing columns per row (intersection graph).
    std::vector<std::vector<std::size_t>> row_columns;
    std::size_t max_row = 0;
    for (const auto& rows : column_rows)
        for (std::size_t r : rows)
            max_row = std::max(max_row, r);
    row_columns.assign(max_row + 1, {});
    for (std::size_t c = 0; c < n; ++c)
        for (std::size_t r : column_rows[c])
            row_columns[r].push_back(c);
    for (const auto& cols : row_columns)
        for (std::size_t a = 0; a < cols.size(); ++a)
            for (std::size_t b = a + 1; b < cols.size(); ++b) {
                const std::size_t u = cols[a];
                const std::size_t v = cols[b];
                if (!adjacent[u][v]) {
                    adjacent[u][v] = 1;
                    adjacent[v][u] = 1;
                    ++degree[u];
                    ++degree[v];
                }
            }

    constexpr std::size_t kEliminated = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> order;
    order.reserve(n);
    for (std::size_t step = 0; step < n; ++step) {
        std::size_t pick = n;
        for (std::size_t c = 0; c < n; ++c) {
            if (degree[c] == kEliminated)
                continue;
            if (pick == n || degree[c] < degree[pick])
                pick = c;
        }
        if (pick == n)
            return identity;  // defensive: should not happen
        order.push_back(pick);
        // Quotient-graph update: fill edges between the neighbours of `pick`,
        // then detach the column from its neighbours.
        std::vector<std::size_t> neighbours;
        for (std::size_t v = 0; v < n; ++v)
            if (adjacent[pick][v])
                neighbours.push_back(v);
        for (std::size_t a = 0; a < neighbours.size(); ++a)
            for (std::size_t b = a + 1; b < neighbours.size(); ++b) {
                const std::size_t u = neighbours[a];
                const std::size_t v = neighbours[b];
                if (!adjacent[u][v]) {
                    adjacent[u][v] = 1;
                    adjacent[v][u] = 1;
                    ++degree[u];
                    ++degree[v];
                }
            }
        for (std::size_t v : neighbours) {
            adjacent[pick][v] = 0;
            adjacent[v][pick] = 0;
            --degree[v];
        }
        degree[pick] = kEliminated;
    }
    return order;
}
}

namespace detail_sparse_basis {
void validate_options(const SparseBasisOptions& o) {
    require_finite(o.singular_tolerance, "non-finite sparse singular tolerance");
    require_finite(o.update_pivot_tolerance, "non-finite sparse update tolerance");
    require_finite(o.eta_density_trigger, "non-finite eta density trigger");
    if (o.singular_tolerance <= 0 || o.update_pivot_tolerance <= 0 || o.eta_density_trigger <= 0 ||
        o.eta_density_trigger > 1 || !o.maximum_updates || !o.maximum_dimension ||
        !o.maximum_nonzeros || !o.maximum_factor_nonzeros)
        throw std::invalid_argument("invalid sparse basis options");
    require_finite(o.refinement_trigger_growth, "non-finite refinement growth trigger");
    require_finite(o.refinement_trigger_condition, "non-finite refinement condition trigger");
    if (o.refinement_trigger_growth <= 0 || o.refinement_trigger_condition <= 0)
        throw std::invalid_argument("invalid sparse basis options");
}
}

}
