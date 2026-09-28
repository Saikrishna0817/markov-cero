#pragma once

#include "markov_cero/milp/cut_pool.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace markov_cero::milp {

/// Copy-on-write local-cut list. Branch siblings share inherited cuts; a node
/// gets a private vector only when its own separation pass changes the list.
class NodeCuts final {
  public:
    [[nodiscard]] bool empty() const noexcept { return !cuts_ || cuts_->empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return cuts_ ? cuts_->size() : 0; }
    [[nodiscard]] const std::vector<Cut>& values() const noexcept {
        static const std::vector<Cut> empty_cuts;
        return cuts_ ? *cuts_ : empty_cuts;
    }

    void append(const std::vector<Cut>& added) {
        if (added.empty()) return;
        detach();
        cuts_->insert(cuts_->end(), added.begin(), added.end());
    }

    void truncate(std::size_t count) {
        if (count >= size()) return;
        detach();
        cuts_->resize(count);
    }

    void sort_by_violation() {
        if (size() < 2) return;
        detach();
        std::stable_sort(cuts_->begin(), cuts_->end(),
                         [](const Cut& a, const Cut& b) { return a.violation > b.violation; });
    }

  private:
    std::shared_ptr<std::vector<Cut>> cuts_;

    void detach() {
        if (!cuts_) cuts_ = std::make_shared<std::vector<Cut>>();
        else if (!cuts_.unique()) cuts_ = std::make_shared<std::vector<Cut>>(*cuts_);
    }
};

} // namespace markov_cero::milp
