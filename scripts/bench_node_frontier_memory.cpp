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

std::size_t argument(char** argv, int index, std::size_t fallback) {
    if (!argv[index]) return fallback;
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
                     std::size_t cut_width, std::size_t workers) {
    constexpr std::size_t max_bytes = std::size_t{1} << 30;
    constexpr std::size_t max_dimension = 1'000'000;
    if (nodes > 100'000 || variables > max_dimension || basis_size > max_dimension ||
        cut_count > 1024 || cut_width > max_dimension || workers > 1024)
        throw std::invalid_argument("benchmark dimensions exceed the safety limit");
    const auto per_node = 2 * variables * sizeof(Bound) +
        basis_size * sizeof(std::size_t) +
        cut_count * cut_width * sizeof(double) + sizeof(MaterializedNode);
    const auto worker_bytes = workers * 2 * variables * sizeof(Bound);
    if (worker_bytes > max_bytes || per_node == 0 || nodes > max_bytes / per_node)
        throw std::invalid_argument("baseline payload estimate exceeds 1 GiB");
}

std::size_t peak_rss_kib() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::runtime_error("getrusage failed");
    return static_cast<std::size_t>(usage.ru_maxrss);
}

Model make_root(std::size_t variables) {
    Model root;
    root.objective.resize(variables, 0.0);
    root.variable_lower.resize(variables, Bound::finite(0.0));
    root.variable_upper.resize(variables, Bound::finite(1.0));
    root.variable_type.resize(variables, markov_cero::model::VariableType::binary);
    markov_cero::model::SparseMatrixBuilder matrix(variables, variables);
    for (std::size_t j = 0; j < variables; ++j) matrix.add(j, j, 1.0);
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
                      std::size_t cut_width, std::size_t workers) {
    const auto root = make_root(variables);
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
    std::cout << "{\"mode\":\"materialized-baseline\",\"nodes\":" << count
              << ",\"variables\":" << variables << ",\"workers\":" << workers
              << ",\"root_matrix_nnz\":" << root.matrix.value.size()
              << ",\"peak_rss_kib\":" << peak_rss_kib() << "}\n";
}

void run_persistent(std::size_t count, std::size_t variables,
                    std::size_t basis_size, std::size_t cut_count,
                    std::size_t cut_width, std::size_t workers) {
    const auto root = make_root(variables);
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
    std::cout << "{\"mode\":\"persistent-current\",\"nodes\":" << count
              << ",\"variables\":" << variables << ",\"workers\":" << workers
              << ",\"root_matrix_nnz\":" << root.matrix.value.size()
              << ",\"peak_rss_kib\":" << peak_rss_kib() << "}\n";
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || (std::string(argv[1]) != "current" &&
                         std::string(argv[1]) != "baseline")) {
            std::cerr << "usage: node_frontier_memory_benchmark current|baseline "
                         "[nodes=2000] [variables=1024] [basis=1024] "
                         "[cuts=4] [cut_width=32] [workers=4]\n";
            return 2;
        }
        const auto nodes = argument(argv, 2, 2000);
        const auto variables = argument(argv, 3, 1024);
        const auto basis = argument(argv, 4, 1024);
        const auto cuts = argument(argv, 5, 4);
        const auto cut_width = argument(argv, 6, 32);
        const auto workers = argument(argv, 7, 4);
        validate_budget(nodes, variables, basis, cuts, cut_width, workers);
        if (std::string(argv[1]) == "baseline")
            run_materialized(nodes, variables, basis, cuts, cut_width, workers);
        else
            run_persistent(nodes, variables, basis, cuts, cut_width, workers);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
