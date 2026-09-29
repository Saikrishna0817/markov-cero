#include "frontier_storage_bytes.hpp"
#include "markov_cero/milp/branch_node.hpp"
#include "markov_cero/model/model.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <vector>

namespace {
using markov_cero::milp::BranchNode;
using markov_cero::milp::Cut;
using markov_cero::model::Bound;
using markov_cero::model::Model;

struct MaterializedNode {
    std::vector<Bound> lower;
    std::vector<Bound> upper;
    std::optional<markov_cero::lp::dual::BasisState> basis;
    std::vector<Cut> cuts;
};

// argv[argc] is the only guaranteed out-of-range slot: scan for it before
// touching argv[index], so omitted trailing arguments stay well defined.
int argc_of(char** argv) {
    int count = 0;
    while (argv[count]) ++count;
    return count;
}

std::size_t argument(char** argv, int index, std::size_t fallback) {
    if (index >= argc_of(argv)) return fallback;
    if (argv[index][0] == '-')
        throw std::invalid_argument("benchmark arguments cannot be negative");
    char* end = nullptr;
    const auto parsed = std::strtoull(argv[index], &end, 10);
    if (end == argv[index] || *end != '\0' || parsed == 0)
        throw std::invalid_argument("benchmark arguments must be positive integers");
    return static_cast<std::size_t>(parsed);
}

void validate_budget(std::size_t nodes, std::size_t variables,
                     std::size_t basis_size, std::size_t cut_count,
                     std::size_t cut_width, std::size_t workers, std::size_t fill) {
    constexpr std::size_t max_bytes = std::size_t{1} << 30;
    constexpr std::size_t max_dimension = 1'000'000;
    if (nodes > 100'000 || variables > max_dimension || basis_size > max_dimension ||
        cut_count > 1024 || cut_width > max_dimension || workers > 1024)
        throw std::invalid_argument("benchmark dimensions exceed the safety limit");
    if (fill > variables || fill > 4096)
        throw std::invalid_argument("matrix fill must be at most the variable count");
    const auto per_node = 2 * variables * sizeof(Bound) +
        basis_size * sizeof(std::size_t) +
        cut_count * cut_width * sizeof(double) + sizeof(MaterializedNode);
    const auto worker_bytes = workers * 2 * variables * sizeof(Bound);
    const auto root_bytes = (variables + 1) * sizeof(std::size_t) +
        variables * fill * (sizeof(std::size_t) + sizeof(double)) +
        variables * (sizeof(double) + 2 * sizeof(Bound) +
                     sizeof(markov_cero::model::VariableType));
    if (worker_bytes > max_bytes || root_bytes > max_bytes || per_node == 0 ||
        nodes > max_bytes / per_node)
        throw std::invalid_argument("payload estimate exceeds 1 GiB");
}

std::size_t peak_rss_kib() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::runtime_error("getrusage failed");
    return static_cast<std::size_t>(usage.ru_maxrss);
}

std::size_t worker_bytes(const std::vector<std::vector<Bound>>& worker_bounds) {
    std::size_t bytes = 0;
    for (const auto& bounds : worker_bounds)
        bytes += bounds.capacity() * sizeof(Bound);
    return bytes;
}

Model make_root(std::size_t variables, std::size_t fill) {
    Model root;
    root.objective.resize(variables, 0.0);
    root.variable_lower.resize(variables, Bound::finite(0.0));
    root.variable_upper.resize(variables, Bound::finite(1.0));
    root.variable_type.resize(variables, markov_cero::model::VariableType::binary);
    markov_cero::model::SparseMatrixBuilder matrix(variables, variables);
    for (std::size_t j = 0; j < variables; ++j)
        for (std::size_t k = 0; k < fill; ++k) matrix.add((j + k) % variables, j, 1.0);
    root.matrix = matrix.build();
    return root;
}

std::vector<Cut> make_cuts(std::size_t count, std::size_t width) {
    std::vector<Cut> cuts;
    cuts.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        cuts.push_back(Cut{std::vector<double>(width, 1.0), 1.0, 1.0});
    return cuts;
}

void run_materialized(std::size_t count, std::size_t variables,
                      std::size_t basis_size, std::size_t cut_count,
                      std::size_t cut_width, std::size_t workers, std::size_t fill) {
    const auto root = make_root(variables, fill);
    const auto cuts = make_cuts(cut_count, cut_width);
    markov_cero::lp::dual::BasisState basis;
    basis.rows = basis_size;
    basis.columns = variables;
    basis.basic_variables.resize(basis_size);
    std::vector<std::vector<Bound>> worker_bounds;
    worker_bounds.reserve(workers * 2);
    for (std::size_t i = 0; i < workers; ++i) {
        worker_bounds.push_back(root.variable_lower);
        worker_bounds.push_back(root.variable_upper);
    }
    std::vector<std::unique_ptr<MaterializedNode>> frontier;
    frontier.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto node = std::make_unique<MaterializedNode>();
        node->lower = root.variable_lower;
        node->upper = root.variable_upper;
        node->lower[i % variables] = Bound::finite(0.5);
        node->basis = basis;
        node->cuts = cuts;
        frontier.push_back(std::move(node));
    }
    // Queue storage: every queued node owns full bound arrays, its own basis
    // copy and its own deep cut copy (the materialized baseline design).
    std::size_t queue_bytes = frontier.capacity() * sizeof(std::unique_ptr<MaterializedNode>);
    for (const auto& node : frontier) {
        queue_bytes += sizeof(MaterializedNode) +
                       (node->lower.capacity() + node->upper.capacity()) * sizeof(Bound) +
                       (node->basis ? frontier_storage_bytes::basis(*node->basis) : 0U) +
                       frontier_storage_bytes::cuts(node->cuts);
    }
    std::cout << "{\"mode\":\"materialized-baseline\",\"nodes\":" << count
              << ",\"variables\":" << variables << ",\"workers\":" << workers
              << ",\"fill\":" << fill
              << ",\"root_matrix_nnz\":" << frontier_storage_bytes::root_nnz(root)
              << ",\"root_bytes\":" << frontier_storage_bytes::root(root)
              << ",\"worker_bytes\":" << worker_bytes(worker_bounds)
              << ",\"queue_bytes\":" << queue_bytes
              << ",\"peak_rss_kib\":" << peak_rss_kib() << "}\n";
}

void run_persistent(std::size_t count, std::size_t variables,
                    std::size_t basis_size, std::size_t cut_count,
                    std::size_t cut_width, std::size_t workers, std::size_t fill) {
    const auto root = make_root(variables, fill);
    auto basis = std::make_shared<markov_cero::lp::dual::BasisState>();
    basis->rows = basis_size;
    basis->columns = variables;
    basis->basic_variables.resize(basis_size);
    markov_cero::milp::NodeCuts cuts;
    cuts.append(make_cuts(cut_count, cut_width));
    const markov_cero::milp::NodeBounds parent_bounds;
    std::vector<std::vector<Bound>> worker_bounds;
    worker_bounds.reserve(workers * 2);
    for (std::size_t i = 0; i < workers; ++i) {
        worker_bounds.push_back(root.variable_lower);
        worker_bounds.push_back(root.variable_upper);
    }
    std::vector<std::shared_ptr<BranchNode>> frontier;
    frontier.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto node = std::make_shared<BranchNode>();
        node->bounds = parent_bounds.with_lower(i % variables, Bound::finite(0.5));
        node->local_cuts = cuts;
        node->warm_basis = basis;
        frontier.push_back(std::move(node));
    }
    // Queue storage: heap record + that node's delta-chain payload. The warm
    // basis and the local-cut list are shared by every queued node here, so
    // their payloads are counted exactly once, not once per node.
    std::size_t queue_bytes = frontier.capacity() * sizeof(std::shared_ptr<BranchNode>);
    for (const auto& node : frontier)
        queue_bytes += sizeof(BranchNode) + node->bounds.retained_bytes();
    queue_bytes += frontier_storage_bytes::basis(*basis) + frontier_storage_bytes::cuts(cuts.values());
    std::cout << "{\"mode\":\"persistent-current\",\"nodes\":" << count
              << ",\"variables\":" << variables << ",\"workers\":" << workers
              << ",\"fill\":" << fill
              << ",\"root_matrix_nnz\":" << frontier_storage_bytes::root_nnz(root)
              << ",\"root_bytes\":" << frontier_storage_bytes::root(root)
              << ",\"worker_bytes\":" << worker_bytes(worker_bounds)
              << ",\"queue_bytes\":" << queue_bytes
              << ",\"peak_rss_kib\":" << peak_rss_kib() << "}\n";
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || (std::string(argv[1]) != "current" &&
                         std::string(argv[1]) != "baseline")) {
            std::cerr << "usage: node_frontier_memory_benchmark current|baseline "
                         "[nodes=2000] [variables=1024] [basis=1024] "
                         "[cuts=4] [cut_width=32] [workers=4] [fill=1]\n";
            return 2;
        }
        const auto nodes = argument(argv, 2, 2000);
        const auto variables = argument(argv, 3, 1024);
        const auto basis = argument(argv, 4, 1024);
        const auto cuts = argument(argv, 5, 4);
        const auto cut_width = argument(argv, 6, 32);
        const auto workers = argument(argv, 7, 4);
        const auto fill = argument(argv, 8, 1);
        validate_budget(nodes, variables, basis, cuts, cut_width, workers, fill);
        if (std::string(argv[1]) == "baseline")
            run_materialized(nodes, variables, basis, cuts, cut_width, workers, fill);
        else
            run_persistent(nodes, variables, basis, cuts, cut_width, workers, fill);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
