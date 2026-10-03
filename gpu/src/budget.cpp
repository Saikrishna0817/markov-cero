#include "markov_cero/gpu/budget.hpp"

namespace markov_cero::gpu {

namespace {
thread_local DeviceBudget* t_current = nullptr;
} // namespace

DeviceBudget::DeviceBudget(std::optional<std::size_t> limit_bytes,
                           std::size_t* peak_sink) noexcept
    : previous_(t_current), peak_sink_(peak_sink), limit_(limit_bytes) {
    t_current = this;
}

DeviceBudget::~DeviceBudget() {
    if (peak_sink_ != nullptr) {
        *peak_sink_ = high_water_;
    }
    t_current = previous_;
}

DeviceBudget* DeviceBudget::current() noexcept { return t_current; }

bool DeviceBudget::refuse(std::size_t bytes) noexcept {
    if (!limit_.has_value()) {
        return false;
    }
    if (exhausted_) {
        return true;
    }
    // `charged_ <= *limit_` holds whenever a limit is set: charges only ever
    // increase through try_charge after passing this test.
    if (bytes > *limit_ - charged_) {
        exhausted_ = true;
        return true;
    }
    return false;
}

bool DeviceBudget::try_charge(std::size_t bytes) noexcept {
    if (refuse(bytes)) {
        return false;
    }
    charged_ += bytes;
    if (charged_ > high_water_) {
        high_water_ = charged_;
    }
    return true;
}

void DeviceBudget::release(std::size_t bytes) noexcept {
    charged_ = bytes > charged_ ? std::size_t{0} : charged_ - bytes;
}

void DeviceBudget::mark_exhausted() noexcept { exhausted_ = true; }

} // namespace markov_cero::gpu
