#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace markov_cero::model {

/// Content hashes of one validated model (W01, D16, D10 model binding).
///
/// - `structural` covers dimensions, sparsity pattern, integer types, bound
///   kinds, senses, names and the structure of optional quadratic/NLP-poly
///   content: everything that is identical when only coefficients move.
/// - `numeric` covers objective coefficients/offset, bound values, matrix
///   values, quadratic values and polynomial coefficients. Doubles are hashed
///   as raw IEEE-754 bit patterns, so `0.0` and `-0.0` are distinct by design
///   and NaN payloads are never silently unified.
/// - `fingerprint()` mixes both into one stable identity: equal content gives
///   an equal fingerprint, so it can key caches, bind proofs to a model and
///   detect an incompatible artifact. Hashes are non-cryptographic.
///
/// Known limitation: user NLP callback bodies (std::function) cannot be
/// hashed; only their presence and dimensions are bound. A fingerprint is not
/// a checksum of executable behaviour.
struct ModelHashes final {
    std::uint64_t structural{0};
    std::uint64_t numeric{0};

    [[nodiscard]] std::uint64_t fingerprint() const noexcept;
};

/// Hashes a model without copying it. The model must already satisfy
/// `Model::validate()`; unvalidated input yields undefined (but stable) hashes.
[[nodiscard]] ModelHashes hash_model(const Model& model) noexcept;

/// Immutable, validated model with stable identity (W01 contract).
///
/// Ownership: a snapshot owns its data. `capture(Model&&)` takes ownership of
/// a caller-built model without copying; `capture(const Model&)` deep-copies,
/// for callers that keep their own builder. After construction there is no
/// mutable access path and no copy constructor, so every stage that receives
/// `const ModelSnapshot&` (or `snapshot.model()`) observes the same matrix,
/// objective, domains and names for the whole solve, and an accidental
/// per-node snapshot duplication fails to compile instead of quietly doubling
/// memory.
///
/// `capture` runs `Model::validate()` first and throws std::invalid_argument
/// on an invalid model, so the API boundary rejects bad input once instead of
/// letting each engine discover it independently.
class ModelSnapshot final {
  public:
    [[nodiscard]] static ModelSnapshot capture(const Model& source);
    [[nodiscard]] static ModelSnapshot capture(Model&& source);

    ModelSnapshot(const ModelSnapshot&) = delete;
    ModelSnapshot& operator=(const ModelSnapshot&) = delete;
    ModelSnapshot(ModelSnapshot&&) = default;
    ModelSnapshot& operator=(ModelSnapshot&&) = default;

    [[nodiscard]] const Model& model() const noexcept { return model_; }
    [[nodiscard]] const ModelHashes& hashes() const noexcept { return hashes_; }
    [[nodiscard]] std::uint64_t structural_hash() const noexcept {
        return hashes_.structural;
    }
    [[nodiscard]] std::uint64_t numeric_hash() const noexcept { return hashes_.numeric; }
    [[nodiscard]] std::uint64_t fingerprint() const noexcept {
        return hashes_.fingerprint();
    }

    /// Approximate solver-owned bytes held by this snapshot (M5 accounting:
    /// CSC arrays, objective/bound/type vectors, names and optional
    /// quadratic/poly content, excluding allocator overhead). Callers charge
    /// this against a solve memory budget when the snapshot is solver-owned.
    [[nodiscard]] std::size_t estimated_bytes() const noexcept;

  private:
    ModelSnapshot(Model model, ModelHashes hashes)
        : model_(std::move(model)), hashes_(hashes) {}

    // Private and exposed only through const accessors: the snapshot is
    // immutable through its API while remaining cheap to move.
    Model model_;
    ModelHashes hashes_;
};

} // namespace markov_cero::model
