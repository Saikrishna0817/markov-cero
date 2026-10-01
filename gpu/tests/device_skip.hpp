#pragma once

#include "markov_cero/gpu/device.hpp"

#include <iostream>

namespace markov_cero::gpu::test {

// Hosted runners compile the CUDA path but have no device behind it: the
// device-only sections below would die inside cudaMalloc. Report a CTest skip
// (exit 77) for that case instead of a failure. CPU-only builds never define
// MARKOV_CERO_HAS_CUDA, so they keep running these tests in full - the
// host-fallback coverage they carry is the coverage a CUDA-less build has.
[[nodiscard]] inline bool skip_without_device() {
#ifdef MARKOV_CERO_HAS_CUDA
    if (get_device_count() <= 0) {
        std::cout << "SKIP: no CUDA device available on this host\n";
        return true;
    }
#endif
    return false;
}

} // namespace markov_cero::gpu::test
