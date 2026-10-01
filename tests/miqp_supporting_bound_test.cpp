// MIQP-01 contract §7.1/§7.2 (docs/contracts/miqp-node-bounds.md): direct
// supporting-lower-bound algebra. Every row-multiplier sign, endpoint case
// and fail-closed case is checked against an independent exact rational
// restatement of §2.2 — the helper shares no code with
// src/qp/supporting_bound.cpp.

#include "markov_cero/model/model.hpp"
#include "markov_cero/qp/model.hpp"
#include "markov_cero/qp/verifier.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "support/tiny_exact.hpp"

using namespace markov_cero;
using test_support::Rational;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// One original row for the exact restatement: coefficient a, multiplier y,
// raw bounds (nullopt = infinite).
struct RowSpec {
    Rational a, y;
    std::optional<Rational> lo, hi;
};
struct Exact {
    std::optional<Rational> value; // nullopt = fail-closed −∞
    long double magnitude{0.0L};   // §2.2 guard magnitude (used terms only)
};

// Independent restatement of contract §2.2 for n = 1. Applies the sign and
// endpoint rules itself: y > 0 selects hi, y < 0 selects lo, y = 0 skips;
// reduced >= 0 selects lo, reduced < 0 selects hi, reduced = 0 skips.
Exact support_exact(const Rational& offset, const Rational& q, const Rational& px,
                    const Rational& xhat, const std::vector<RowSpec>& rows,
                    const std::optional<Rational>& box_lo,
                    const std::optional<Rational>& box_hi) {
    Exact out;
    const Rational half(1, 2);
    Rational bound = offset;
    out.magnitude = std::fabs(static_cast<long double>(offset.numerator()) / offset.denominator());
    const Rational support_term = half * xhat * px;
    bound = bound - support_term;
    out.magnitude += std::fabs(static_cast<long double>(support_term.numerator()) /
                               support_term.denominator());
    Rational reduced = q + px;
    for (const auto& row : rows) {
        reduced = reduced + row.a * row.y;
        if (row.y == Rational(0)) continue;
        const std::optional<Rational>& side = row.y < Rational(0) ? row.lo : row.hi;
        if (!side) { out.value = std::nullopt; return out; }
        const Rational term = row.y * *side;
        bound = bound - term;
        out.magnitude += std::fabs(static_cast<long double>(term.numerator()) / term.denominator());
    }
    if (reduced != Rational(0)) {
        const std::optional<Rational>& endpoint = reduced < Rational(0) ? box_hi : box_lo;
        if (!endpoint) { out.value = std::nullopt; return out; }
        const Rational term = reduced * *endpoint;
        bound = bound + term;
        out.magnitude += std::fabs(static_cast<long double>(term.numerator()) / term.denominator());
    }
    out.value = bound;
    return out;
}

model::Model make_model(double p, double q, double offset, model::Bound lo, model::Bound hi,
                        const std::vector<std::pair<double, std::pair<model::Bound, model::Bound>>>& rows) {
    model::Model m;
    m.name = "UNIT";
    m.objective_sense = model::ObjectiveSense::minimize;
    m.objective_offset = offset;
    m.objective = {q};
    m.has_quadratic_objective = true;
    m.quadratic_matrix.row_count = 1;
    m.quadratic_matrix.column_count = 1;
    m.quadratic_matrix.column_start = {0, 1};
    m.quadratic_matrix.row_index = {0};
    m.quadratic_matrix.value = {p};
    model::SparseMatrixBuilder builder(rows.size(), 1);
    for (std::size_t i = 0; i < rows.size(); ++i) builder.add(i, 0, rows[i].first);
    m.matrix = builder.build();
    for (const auto& row : rows) {
        m.row_lower.push_back(row.second.first);
        m.row_upper.push_back(row.second.second);
        m.row_name.push_back("R" + std::to_string(m.row_name.size()));
    }
    m.variable_lower = {lo};
    m.variable_upper = {hi};
    m.variable_type = {model::VariableType::continuous};
    m.variable_name = {"X"};
    m.validate();
    return m;
}

void check(const char* name, const model::Model& m, double xhat,
           const std::vector<double>& y, const Exact& expected) {
    const auto q = qp::make_quadratic_model(m);
    qp::QpSolution sol;
    sol.x = {xhat};
    sol.y = y;
    const double got = qp::supporting_lower_bound(m, q, sol);
    if (!expected.value) {
        require(got == -std::numeric_limits<double>::infinity(),
                std::string(name) + ": fail-closed cases return exactly -inf");
        std::cout << "[+] " << name << " fails closed\n";
        return;
    }
    const double exact_d =
        static_cast<double>(expected.value->numerator()) / expected.value->denominator();
    const long double slack = 4096.0L * std::numeric_limits<double>::epsilon() *
                              (1.0L + std::fabs(static_cast<long double>(exact_d)));
    require(std::isfinite(got), std::string(name) + ": finite case stays finite");
    require(static_cast<long double>(got) <= static_cast<long double>(exact_d) + slack,
            std::string(name) + ": stored value never exceeds the exact formula");
    require(static_cast<long double>(exact_d) - got <=
                4096.0L * std::numeric_limits<double>::epsilon() * (1.0L + expected.magnitude),
            std::string(name) + ": weakening stays inside the 512ε guard envelope");
    std::cout << "[+] " << name << " matches the rational restatement\n";
}

void run_cases() {
    const auto neg_inf = model::Bound::negative_infinity();
    const auto pos_inf = model::Bound::positive_infinity();
    const Rational one(1), two(2), half(1, 2);

    // f = x² − 2x + 1, no rows, box [0,3]: support constant only.
    {
        const auto m = make_model(2.0, -2.0, 1.0, {model::Bound::finite(0.0)},
                                  {model::Bound::finite(3.0)}, {});
        check("basic_offset_and_support", m, 1.5, {},
              support_exact(Rational(1), Rational(-2), Rational(3), Rational(3, 2), {},
                            Rational(0), Rational(3)));
    }
    // y > 0 selects the upper endpoint (−y·u).
    {
        const auto m = make_model(2.0, 0.0, 0.0, {model::Bound::finite(0.0)},
                                  {model::Bound::finite(1.0)},
                                  {{1.0, {neg_inf, model::Bound::finite(2.0)}}});
        check("positive_multiplier_selects_upper", m, 1.0, {1.0},
              support_exact(Rational(0), Rational(0), Rational(2), one,
                                   {{one, one, std::nullopt, two}}, Rational(0), Rational(1)));
    }
    // y < 0 selects the lower endpoint (−y·l).
    {
        const auto m = make_model(2.0, 0.0, 0.0, {model::Bound::finite(0.0)},
                                  {model::Bound::finite(1.0)},
                                  {{1.0, {model::Bound::finite(0.5), pos_inf}}});
        check("negative_multiplier_selects_lower", m, 1.0, {-1.0},
              support_exact(Rational(0), Rational(0), Rational(2), one,
                            {{one, Rational(-1), half, std::nullopt}}, Rational(0), Rational(1)));
    }
    // y = 0 skips the term even when both row sides are infinite.
    {
        const auto m = make_model(2.0, 0.0, 0.0, {model::Bound::finite(0.0)},
                                  {model::Bound::finite(1.0)}, {{1.0, {neg_inf, pos_inf}}});
        check("zero_multiplier_ignores_infinite_sides", m, 1.0, {0.0},
              support_exact(Rational(0), Rational(0), Rational(2), one,
                            {{one, Rational(0), std::nullopt, std::nullopt}}, Rational(0), Rational(1)));
    }
    // A used multiplier on an infinite side fails closed (same model, y = +1).
    {
        const auto m = make_model(2.0, 0.0, 0.0, {model::Bound::finite(0.0)},
                                  {model::Bound::finite(1.0)}, {{1.0, {neg_inf, pos_inf}}});
        check("nonfinite_used_side_fails_closed", m, 1.0, {1.0},
              support_exact(Rational(0), Rational(0), Rational(2), one,
                            {{one, one, std::nullopt, std::nullopt}}, Rational(0), Rational(1)));
    }
    // reduced = 0 skips the box term even on an infinite box (x̂ = 0, q = 0).
    {
        const auto m = make_model(2.0, 0.0, 0.0, neg_inf, pos_inf, {});
        check("zero_reduced_ignores_infinite_endpoints", m, 0.0, {},
              support_exact(Rational(0), Rational(0), Rational(0), Rational(0), {},
                            std::nullopt, std::nullopt));
    }
    // reduced > 0 needs a finite lower endpoint; infinite lower fails closed.
    {
        const auto m = make_model(2.0, 0.0, 0.0, neg_inf, {model::Bound::finite(1.0)}, {});
        check("infinite_lower_fails_closed", m, 1.0, {},
              support_exact(Rational(0), Rational(0), Rational(2), one, {}, std::nullopt, one));
    }
    // reduced < 0 needs a finite upper endpoint; finite upper gives r·hi.
    {
        const auto m = make_model(2.0, 0.0, 0.0, {model::Bound::finite(0.0)},
                                  {model::Bound::finite(1.0)}, {});
        check("negative_reduced_uses_upper", m, -1.0, {},
              support_exact(Rational(0), Rational(0), Rational(-2), Rational(-1), {},
                            Rational(0), one));
    }
    {
        const auto m = make_model(2.0, 0.0, 0.0, {model::Bound::finite(0.0)}, pos_inf, {});
        check("infinite_upper_fails_closed", m, -1.0, {},
              support_exact(Rational(0), Rational(0), Rational(-2), Rational(-1), {},
                            Rational(0), std::nullopt));
    }
    // Directed weakening: 100000-offset case, stored strictly below exact.
    {
        const auto m = make_model(2.0, -2.0, 100000.0, {model::Bound::finite(0.0)},
                                  {model::Bound::finite(3.0)}, {});
        const auto exact = support_exact(Rational(100000), Rational(-2), Rational(3),
                                         Rational(3, 2), {}, Rational(0), Rational(3));
        const auto q = qp::make_quadratic_model(m);
        qp::QpSolution sol;
        sol.x = {1.5};
        sol.y = {};
        const double got = qp::supporting_lower_bound(m, q, sol);
        const double exact_d =
            static_cast<double>(exact.value->numerator()) / exact.value->denominator();
        require(got < exact_d, "guard weakens the bound strictly downward at scale");
        check("guard_weakening_directed", m, 1.5, {}, exact);
    }
    // §2.1/§7.1 input-space boundary: the formula reads model.objective_offset
    // and the node box, so an un-normalized *maximize* model does NOT yield a
    // valid minimization-space bound — this is why search normalizes the model
    // at search_initialize.cpp:45-47 before any call. min-space optimum here
    // is (x−1)²−5 = −5.
    {
        model::Model mx;
        mx.name = "MAX_BOUNDARY";
        mx.objective_sense = model::ObjectiveSense::maximize;
        mx.objective_offset = 4.0; // max −(x−1)²+5 = −x²+2x+4
        mx.objective = {2.0};
        mx.has_quadratic_objective = true;
        mx.quadratic_matrix.row_count = 1;
        mx.quadratic_matrix.column_count = 1;
        mx.quadratic_matrix.column_start = {0, 1};
        mx.quadratic_matrix.row_index = {0};
        mx.quadratic_matrix.value = {-2.0};
        mx.matrix = model::SparseMatrixBuilder(0, 1).build();
        mx.variable_lower = {model::Bound::finite(0.0)};
        mx.variable_upper = {model::Bound::finite(2.0)};
        mx.variable_type = {model::VariableType::continuous};
        mx.variable_name = {"X"};
        mx.validate();
        qp::QpSolution witness;
        witness.x = {1.0};
        const double raw = qp::supporting_lower_bound(mx, qp::make_quadratic_model(mx), witness);
        require(raw > -4.0, "un-normalized maximize call is not a sound min-space bound");
        // The search's own normalization: negate objective, offset and quadratic.
        mx.objective_sense = model::ObjectiveSense::minimize;
        mx.objective = {-2.0};
        mx.objective_offset = -4.0; // min x²−2x−4 = (x−1)²−5
        mx.quadratic_matrix.value = {2.0};
        const double normalized =
            qp::supporting_lower_bound(mx, qp::make_quadratic_model(mx), witness);
        require(normalized <= -5.0 + 1e-12 && normalized > -5.0 - 1e-6,
                "normalized minimize-space bound is sound (≈ −5)");
        std::cout << "[+] input-space boundary: raw maximize call unsound, normalized sound\n";
    }
    // Dimension mismatches fail closed (§2.3).
    {
        const auto m = make_model(2.0, 0.0, 0.0, {model::Bound::finite(0.0)},
                                  {model::Bound::finite(1.0)},
                                  {{1.0, {neg_inf, model::Bound::finite(2.0)}}});
        const auto q = qp::make_quadratic_model(m);
        qp::QpSolution short_x;
        short_x.y = {1.0};
        require(qp::supporting_lower_bound(m, q, short_x) ==
                    -std::numeric_limits<double>::infinity(),
                "short primal fails closed");
        qp::QpSolution short_y;
        short_y.x = {1.0};
        require(qp::supporting_lower_bound(m, q, short_y) ==
                    -std::numeric_limits<double>::infinity(),
                "short dual vector fails closed");
        qp::QpSolution ok;
        ok.x = {1.0};
        ok.y = {1.0};
        require(qp::supporting_lower_bound(m, q, ok, {}, {model::Bound::finite(1.0)}) ==
                    -std::numeric_limits<double>::infinity(),
                "short bound vector fails closed");
        std::cout << "[+] dimension mismatches fail closed\n";
    }
}
} // namespace

int main() {
    try {
        run_cases();
        std::cout << "All MIQP supporting-bound tests PASSED successfully!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[-] Error: " << error.what() << "\n";
        return 1;
    }
}
