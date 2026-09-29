#include "markov_cero/model/model_snapshot.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {
using markov_cero::model::Bound;
using markov_cero::model::Model;
using markov_cero::model::ModelHashes;
using markov_cero::model::ModelSnapshot;
using markov_cero::model::ObjectiveSense;
using markov_cero::model::SparseMatrixBuilder;
using markov_cero::model::VariableType;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Model make_model() {
    Model model;
    model.name = "snapshot_lp";
    model.objective_sense = ObjectiveSense::minimize;
    model.objective_offset = 0.5;
    model.objective = {1.0, -2.0, 0.25};
    model.variable_name = {"x", "y", "z"};
    model.variable_lower = {Bound::finite(0.0), Bound::finite(-1.0), Bound::negative_infinity()};
    model.variable_upper = {Bound::finite(10.0), Bound::positive_infinity(),
                            Bound::positive_infinity()};
    model.variable_type = {VariableType::continuous, VariableType::integer,
                           VariableType::continuous};
    model.row_name = {"r1"};
    model.row_lower = {Bound::negative_infinity()};
    model.row_upper = {Bound::finite(4.0)};
    SparseMatrixBuilder builder(1, 3);
    builder.add(0, 0, 2.0);
    builder.add(0, 1, 3.0);
    builder.add(0, 2, -1.0);
    model.matrix = builder.build();
    model.validate();
    return model;
}

static_assert(!std::is_copy_constructible_v<ModelSnapshot>,
              "a snapshot must not be duplicable by accident");

void test_capture_copies_and_binds_content() {
    const Model source = make_model();
    const ModelSnapshot snapshot = ModelSnapshot::capture(source);
    require(snapshot.model().name == source.name, "capture keeps the model name");
    require(snapshot.model().matrix.value == source.matrix.value, "capture keeps matrix values");
    require(snapshot.model().objective == source.objective, "capture keeps the objective");
    require(snapshot.model().row_upper.size() == 1, "capture keeps row metadata");
    require(snapshot.estimated_bytes() > 0, "solver-owned bytes are accounted");

    const ModelSnapshot again = ModelSnapshot::capture(source);
    require(snapshot.fingerprint() == again.fingerprint(),
            "equal content yields an equal identity");
    require(snapshot.structural_hash() == again.structural_hash(),
            "equal content yields an equal structural hash");
    require(snapshot.numeric_hash() == again.numeric_hash(),
            "equal content yields an equal numeric hash");
    require(snapshot.hashes().fingerprint() == snapshot.fingerprint(),
            "hashes() and fingerprint() agree");
}

void test_capture_moves_without_copying() {
    Model source = make_model();
    const std::size_t nnz = source.matrix.value.size();
    ModelSnapshot snapshot = ModelSnapshot::capture(std::move(source));
    require(snapshot.model().matrix.value.size() == nnz, "moved snapshot keeps the matrix");
    require(snapshot.model().objective.size() == 3, "moved snapshot keeps the objective");

    ModelSnapshot moved = std::move(snapshot);
    require(moved.model().name == "snapshot_lp", "snapshots are movable");
    require(moved.fingerprint() == ModelSnapshot::capture(make_model()).fingerprint(),
            "a moved snapshot keeps its identity");
}

void test_numeric_change_keeps_structure() {
    const Model base = make_model();
    Model perturbed = base;
    perturbed.objective[0] += 1e-9;
    const ModelHashes before = markov_cero::model::hash_model(base);
    const ModelHashes after = markov_cero::model::hash_model(perturbed);
    require(before.structural == after.structural,
            "a coefficient change is not a structural change");
    require(before.numeric != after.numeric, "a coefficient change changes the numeric hash");
    require(before.fingerprint() != after.fingerprint(), "identity follows the numeric change");
}

void test_signed_zero_is_distinct_by_design() {
    Model positive = make_model();
    Model negative = make_model();
    positive.objective[0] = 0.0;
    negative.objective[0] = -0.0;
    // Documented behaviour: doubles hash as raw IEEE-754 bit patterns, so the
    // two spellings are distinct rather than silently unified.
    require(positive.objective[0] == negative.objective[0],
            "the two objectives compare equal as numbers");
    require(markov_cero::model::hash_model(positive).numeric !=
                markov_cero::model::hash_model(negative).numeric,
            "signed zero changes the numeric hash by design");
}

void test_structural_changes_are_separable() {
    const Model base = make_model();
    const ModelHashes original = markov_cero::model::hash_model(base);

    Model renamed = base;
    renamed.row_name[0] = "renamed_row";
    const ModelHashes renamed_hashes = markov_cero::model::hash_model(renamed);
    require(renamed_hashes.structural != original.structural,
            "a row rename changes the structural hash");
    require(renamed_hashes.numeric == original.numeric,
            "a row rename leaves the numeric hash unchanged");

    Model retyped = base;
    retyped.variable_type[0] = VariableType::binary;
    const ModelHashes retyped_hashes = markov_cero::model::hash_model(retyped);
    require(retyped_hashes.structural != original.structural,
            "an integrality change is structural");
    require(retyped_hashes.numeric == original.numeric,
            "an integrality change leaves numeric content alone");

    Model restructured = base;
    SparseMatrixBuilder builder(1, 3);
    builder.add(0, 0, 2.0);
    builder.add(0, 2, -1.0); // drops the (0,1) entry
    restructured.matrix = builder.build();
    const ModelHashes restructured_hashes = markov_cero::model::hash_model(restructured);
    require(restructured_hashes.structural != original.structural,
            "a sparsity change is structural");
    require(restructured_hashes.numeric != original.numeric,
            "dropping an entry also changes numeric content");
}

void test_callback_presence_is_structural() {
    const Model base = make_model();
    Model with_callbacks = base;
    with_callbacks.nlp_callbacks = markov_cero::nlp::NlpModel{};
    require(markov_cero::model::hash_model(with_callbacks).structural !=
                markov_cero::model::hash_model(base).structural,
            "adding an NLP callback view is structural");
}

void test_invalid_model_is_rejected() {
    Model broken = make_model();
    broken.objective.pop_back();
    bool threw = false;
    try {
        (void)ModelSnapshot::capture(broken);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "capture validates before hashing");

    Model broken_move = make_model();
    broken_move.matrix.row_index[0] = 99;
    threw = false;
    try {
        (void)ModelSnapshot::capture(std::move(broken_move));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "the moving capture validates too");
}

void test_estimated_bytes_track_content() {
    const ModelSnapshot small = ModelSnapshot::capture(make_model());
    Model large = make_model();
    large.objective = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    large.variable_name = {"a", "b", "c", "d", "e", "f", "g", "h"};
    large.variable_lower.assign(8, Bound::finite(0.0));
    large.variable_upper.assign(8, Bound::finite(1.0));
    large.variable_type.assign(8, VariableType::continuous);
    SparseMatrixBuilder builder(1, 8);
    for (std::size_t column = 0; column < 8; ++column) builder.add(0, column, 1.0);
    large.matrix = builder.build();
    large.validate();
    const ModelSnapshot bigger = ModelSnapshot::capture(large);
    require(bigger.estimated_bytes() > small.estimated_bytes(),
            "estimated bytes grow with the model");
}
} // namespace

int main() {
    test_capture_copies_and_binds_content();
    test_capture_moves_without_copying();
    test_numeric_change_keeps_structure();
    test_signed_zero_is_distinct_by_design();
    test_structural_changes_are_separable();
    test_callback_presence_is_structural();
    test_invalid_model_is_rejected();
    test_estimated_bytes_track_content();
    return 0;
}
