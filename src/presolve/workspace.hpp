#pragma once
#include "markov_cero/presolve/presolve.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
namespace markov_cero::presolve::detail {
struct ColEntry { std::size_t row; double val; };
struct RowEntry { std::size_t col; double val; };
struct Workspace {
    const transform::SparseCanonicalModel& input;
    const PresolveOptions& options;
    std::size_t m, n, reductions{0};
    PresolveResult result;
    std::vector<bool> row_active, col_active;
    std::vector<std::vector<ColEntry>> cols;
    std::vector<std::vector<RowEntry>> rows;
    std::vector<double> rhs, obj;
    double obj_offset{0};
    Workspace(const transform::SparseCanonicalModel& model, const PresolveOptions& settings)
        : input(model), options(settings), m(model.matrix.rows), n(model.matrix.columns) {}
    bool initialize();
    bool eliminate_empty_and_singletons();
    bool eliminate_duplicates();
    PresolveResult compact();
    PresolveResult run();
};
void ensure_finite(double, const char*);
std::uint64_t hash_mix(std::uint64_t, std::uint64_t);
std::uint64_t value_key(double);
}
