#include "markov_cero/verify/reference_lp_verifier.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"
#include <new>
#include <stdexcept>

namespace markov_cero::verify {
ReferenceVerification verify_reference_result(const transform::CanonicalModel& model,
                                              const lp::reference::Result& result,
                                              double tolerance,
                                              const core::Deadline& deadline) {
    try {
        model.validate();
        transform::SparseCanonicalModel sparse;
        sparse.rhs = model.rhs;
        sparse.objective = model.objective;
        sparse.objective_offset = model.objective_offset;
        sparse.record = model.record;
        sparse.matrix.rows = model.matrix.rows;
        sparse.matrix.columns = model.matrix.columns;
        sparse.matrix.column_offsets.push_back(0);
        for (std::size_t j = 0; j < model.matrix.columns; ++j) {
            if ((j & 1023U) == 0U && deadline.expired()) {
                ReferenceVerification report;
                report.message = "verification stopped: deadline exceeded";
                return report;
            }
            for (std::size_t i = 0; i < model.matrix.rows; ++i) {
                if (model.matrix(i, j) == 0.0) continue;
                sparse.matrix.row_indices.push_back(i);
                sparse.matrix.values.push_back(model.matrix(i, j));
            }
            sparse.matrix.column_offsets.push_back(sparse.matrix.values.size());
        }
        return verify_sparse_result(sparse, result, tolerance, deadline);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::length_error&) {
        throw;
    } catch (const std::exception& error) {
        ReferenceVerification report;
        report.message = std::string("verification failed: ") + error.what();
        return report;
    }
}
} // namespace markov_cero::verify
