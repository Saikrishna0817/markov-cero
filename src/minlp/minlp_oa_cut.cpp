// MINLP-01 (docs/contracts/minlp-oa.md §5): source-polynomial evaluation,
// cut derivation with provenance and outward weakening, and independent cut
// replay. The replay path shares only representation accessors with the
// make_nlp_model closure construction; it never reuses those closures.
#include "markov_cero/minlp/oa_cut.hpp"

#include "markov_cero/qp/model.hpp"

#include <cmath>
#include <stdexcept>

namespace markov_cero::minlp {
namespace {

struct SourceEval {
    double value{0.0};
    std::vector<double> grad;
};

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    double out = 0.0;
    for (std::size_t j = 0; j < a.size(); ++j) out += a[j] * b[j];
    return out;
}

std::vector<double> dense_row(const model::Model& source, std::size_t row) {
    const std::size_t n = source.matrix.column_count;
    std::vector<double> a(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t k = source.matrix.column_start[j];
             k < source.matrix.column_start[j + 1]; ++k) {
            if (source.matrix.row_index[k] == row) a[j] += source.matrix.value[k];
        }
    }
    return a;
}

SourceEval evaluate_source(const model::Model& source, OaCutSource kind, std::size_t index,
                           const std::vector<double>& x) {
    const std::size_t n = source.matrix.column_count;
    if (x.size() != n) {
        throw std::invalid_argument("minlp replay: expansion point has the wrong dimension");
    }
    const bool maximize = source.objective_sense == model::ObjectiveSense::maximize;
    const double sign = maximize ? -1.0 : 1.0;
    SourceEval e;
    switch (kind) {
    case OaCutSource::objective: {
        e.grad.assign(n, 0.0);
        double v = sign * source.objective_offset;
        for (std::size_t j = 0; j < n && j < source.objective.size(); ++j) {
            const double c = sign * source.objective[j];
            v += c * x[j];
            e.grad[j] += c;
        }
        for (const auto& term : source.nlobj_terms) {
            if (term.var0 >= n || (term.quadratic && term.var1 >= n)) {
                throw std::invalid_argument("minlp replay: NLOBJ term index out of range");
            }
            const double c = sign * term.coefficient;
            v += c * x[term.var0] * (term.quadratic ? x[term.var1] : 1.0);
            if (!term.quadratic) {
                e.grad[term.var0] += c;
            } else if (term.var0 == term.var1) {
                e.grad[term.var0] += 2.0 * c * x[term.var0];
            } else {
                e.grad[term.var0] += c * x[term.var1];
                e.grad[term.var1] += c * x[term.var0];
            }
        }
        if (source.has_quadratic_objective) {
            const auto qp = qp::make_quadratic_model(source);
            v += 0.5 * qp.P.evaluate_energy(x);
            const auto px = qp.P.multiply(x);
            for (std::size_t j = 0; j < n && j < px.size(); ++j) e.grad[j] += px[j];
        }
        e.value = v;
        return e;
    }
    case OaCutSource::linear_upper: {
        if (index >= source.matrix.row_count || !source.row_upper[index].is_finite()) {
            throw std::invalid_argument("minlp replay: no finite upper bound for source row");
        }
        e.grad = dense_row(source, index);
        e.value = dot(e.grad, x) - source.row_upper[index].value;
        return e;
    }
    case OaCutSource::linear_lower: {
        if (index >= source.matrix.row_count || !source.row_lower[index].is_finite()) {
            throw std::invalid_argument("minlp replay: no finite lower bound for source row");
        }
        e.grad = dense_row(source, index);
        e.value = source.row_lower[index].value - dot(e.grad, x); // lower - a^T x
        for (double& v : e.grad) v = -v;                          // grad = -a
        return e;
    }
    case OaCutSource::nlcon: {
        if (index >= source.nlcon_constraints.size()) {
            throw std::invalid_argument("minlp replay: NLCON index out of range");
        }
        const auto& row = source.nlcon_constraints[index];
        e.grad.assign(n, 0.0);
        double v = -row.rhs;
        for (const auto& term : row.terms) {
            if (term.var0 >= n || (term.quadratic && term.var1 >= n)) {
                throw std::invalid_argument("minlp replay: NLCON term index out of range");
            }
            v += term.coefficient * x[term.var0] * (term.quadratic ? x[term.var1] : 1.0);
            if (!term.quadratic) {
                e.grad[term.var0] += term.coefficient;
            } else if (term.var0 == term.var1) {
                e.grad[term.var0] += 2.0 * term.coefficient * x[term.var0];
            } else {
                e.grad[term.var0] += term.coefficient * x[term.var1];
                e.grad[term.var1] += term.coefficient * x[term.var0];
            }
        }
        e.value = v;
        return e;
    }
    }
    throw std::invalid_argument("minlp replay: unknown cut source kind");
}

std::string fail(const std::string& at, const std::string& what) { return at + what; }

} // namespace

OaCut derive_oa_cut(const model::Model& source, OaCutSource kind, std::size_t source_index,
                    const std::vector<double>& point) {
    const SourceEval e = evaluate_source(source, kind, source_index, point);
    const double exact = dot(e.grad, point) - e.value;
    OaCut cut;
    cut.source_kind = kind;
    cut.source_index = kind == OaCutSource::objective ? kOaObjectiveSource : source_index;
    cut.source_maximize = source.objective_sense == model::ObjectiveSense::maximize;
    cut.point = point;
    cut.gradient = e.grad;
    cut.value = e.value;
    cut.weakening = kOaCutWeakening * (1.0 + std::abs(exact));
    cut.rhs = exact + cut.weakening;
    return cut;
}

OaReplayReport replay_oa_cuts(const model::Model& source, const std::vector<OaCut>& cuts) {
    OaReplayReport rep;
    const bool source_maximize = source.objective_sense == model::ObjectiveSense::maximize;
    for (std::size_t k = 0; k < cuts.size(); ++k) {
        const OaCut& c = cuts[k];
        const std::string at = "cut " + std::to_string(k) + ": ";
        if (c.source_kind == OaCutSource::objective && c.source_index != kOaObjectiveSource) {
            rep.message = fail(at, "objective cut carries a source row index");
            return rep;
        }
        if (c.source_maximize != source_maximize) {
            rep.message = fail(at, "recorded objective sense does not match the source");
            return rep;
        }
        if (!std::isfinite(c.value) || !std::isfinite(c.rhs) || !std::isfinite(c.weakening)) {
            rep.message = fail(at, "stored value, rhs or weakening is not finite");
            return rep;
        }
        SourceEval e;
        try {
            e = evaluate_source(source, c.source_kind, c.source_index, c.point);
        } catch (const std::exception& ex) {
            rep.message = fail(at, std::string("source re-evaluation failed: ") + ex.what());
            return rep;
        }
        if (c.gradient.size() != e.grad.size()) {
            rep.message = fail(at, "gradient dimension does not match the source");
            return rep;
        }
        const double tol_v = kOaCutReplayTolerance * (1.0 + std::abs(e.value));
        if (std::abs(c.value - e.value) > tol_v) {
            rep.message = fail(at, "stored value " + std::to_string(c.value) +
                                       " != source value " + std::to_string(e.value));
            return rep;
        }
        for (std::size_t j = 0; j < e.grad.size(); ++j) {
            const double tol_g = kOaCutReplayTolerance * (1.0 + std::abs(e.grad[j]));
            if (!std::isfinite(c.gradient[j]) ||
                std::abs(c.gradient[j] - e.grad[j]) > tol_g) {
                rep.message = fail(at, "stored gradient component " + std::to_string(j) +
                                           " != source gradient");
                return rep;
            }
        }
        if (c.weakening < 0.0) {
            rep.message = fail(at, "weakening is negative (row moved inward)");
            return rep;
        }
        const double exact = dot(e.grad, c.point) - e.value;
        const double tol_r = kOaCutReplayTolerance * (1.0 + std::abs(exact));
        if (std::abs((c.rhs - c.weakening) - exact) > tol_r) {
            rep.message = fail(at, "rhs minus weakening does not match gradient^T p - value");
            return rep;
        }
        ++rep.checked;
    }
    rep.accepted = true;
    return rep;
}

std::vector<OaRowSource> oa_ineq_source_map(const model::Model& source) {
    std::vector<OaRowSource> map;
    map.reserve(2 * source.matrix.row_count + source.nlcon_constraints.size());
    for (std::size_t i = 0; i < source.matrix.row_count; ++i) {
        if (source.row_upper[i].is_finite()) map.push_back({OaCutSource::linear_upper, i});
        if (source.row_lower[i].is_finite()) map.push_back({OaCutSource::linear_lower, i});
    }
    for (std::size_t j = 0; j < source.nlcon_constraints.size(); ++j) {
        map.push_back({OaCutSource::nlcon, j});
    }
    return map;
}

} // namespace markov_cero::minlp
