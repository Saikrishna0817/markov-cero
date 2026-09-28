#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace markov_cero::milp {

/// Persistent bound overlay for a branch-and-bound node. Each child stores
/// only the bound change that created it and shares its parent's history.
/// Materialization is confined to a reusable worker model.
class NodeBounds final {
  private:
    struct Delta;

  public:
    class MaterializationScratch final {
      public:
        [[nodiscard]] std::size_t retained_capacity() const noexcept {
            return path_.capacity();
        }

      private:
        friend class NodeBounds;
        std::vector<const Delta*> path_;
    };

    [[nodiscard]] std::size_t delta_count() const noexcept { return delta_count_; }

    [[nodiscard]] NodeBounds with_lower(std::size_t variable, model::Bound value) const {
        return append(variable, value, std::nullopt);
    }

    [[nodiscard]] NodeBounds with_upper(std::size_t variable, model::Bound value) const {
        return append(variable, std::nullopt, value);
    }

    void materialize(const std::vector<model::Bound>& root_lower,
                     const std::vector<model::Bound>& root_upper,
                     std::vector<model::Bound>& lower_out,
                     std::vector<model::Bound>& upper_out) const {
        MaterializationScratch scratch;
        materialize(root_lower, root_upper, lower_out, upper_out, scratch);
    }

    void materialize(const std::vector<model::Bound>& root_lower,
                     const std::vector<model::Bound>& root_upper,
                     std::vector<model::Bound>& lower_out,
                     std::vector<model::Bound>& upper_out,
                     MaterializationScratch& scratch) const {
        if (root_lower.size() != root_upper.size()) {
            throw std::invalid_argument("root variable bound dimensions differ");
        }
        if (&root_lower == &lower_out || &root_lower == &upper_out ||
            &root_upper == &lower_out || &root_upper == &upper_out ||
            &lower_out == &upper_out) {
            throw std::invalid_argument("node bound materialization requires separate vectors");
        }
        lower_out = root_lower;
        upper_out = root_upper;
        auto& path = scratch.path_;
        path.clear();
        if (path.capacity() < delta_count_) path.reserve(delta_count_);
        for (auto delta = tail_; delta; delta = delta->parent) path.push_back(delta.get());
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            const auto& delta = **it;
            if (delta.variable >= lower_out.size()) {
                throw std::invalid_argument("node bound variable index is out of range");
            }
            if (delta.lower) lower_out[delta.variable] = *delta.lower;
            if (delta.upper) upper_out[delta.variable] = *delta.upper;
        }
    }

  private:
    struct Delta final {
        std::shared_ptr<const Delta> parent;
        std::size_t variable{};
        std::optional<model::Bound> lower;
        std::optional<model::Bound> upper;
    };

    std::shared_ptr<const Delta> tail_;
    std::size_t delta_count_{};

    [[nodiscard]] NodeBounds append(std::size_t variable,
                                    std::optional<model::Bound> lower_bound,
                                    std::optional<model::Bound> upper_bound) const {
        NodeBounds result;
        result.tail_ = std::make_shared<Delta>(Delta{tail_, variable, lower_bound, upper_bound});
        result.delta_count_ = delta_count_ + 1;
        return result;
    }
};

} // namespace markov_cero::milp
