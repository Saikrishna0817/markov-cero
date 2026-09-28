#pragma once

#include "markov_cero/gpu/device.hpp"

namespace markov_cero::gpu {

// W4 (LOCKED scope): AMD and Intel GPU support is NOT implemented in this
// plan. This header is the documented extension point so a future implementer
// can add it without restructuring the codebase.
//
// NOTE FOR FUTURE IMPLEMENTERS:
// To add AMD HIP support:
//   1. Add -DMARKOV_CERO_ENABLE_HIP=ON CMake option
//   2. Replace #include <cuda_runtime.h> with #include <hip/hip_runtime.h>
//      in all gpu/kernels/*.cu files (rename to *.hip.cpp)
//   3. Replace CUDA macros with HIP equivalents (they are identical in naming)
//   4. Add find_package(hip REQUIRED) and enable_language(HIP) to CMakeLists.txt
//   5. Test with: hipcc gpu/kernels/spmv.cu → confirms no CUDA-specific API usage
//
// To add Intel OneAPI support:
//   1. Rewrite gpu/kernels/*.cu as SYCL kernels (different API; cannot share source)
//   2. Add find_package(IntelSYCL REQUIRED) and -fsycl compiler flag
//
// Current supported backends: CUDA (NVIDIA, sm_50+), CPU (always available)

enum class HardwareBackend { cpu_reference, cuda_nvidia };

inline const char* to_string(HardwareBackend b) noexcept {
    return b == HardwareBackend::cuda_nvidia ? "cuda_nvidia" : "cpu_reference";
}

// Detection mirrors is_gpu_available(): CUDA device at sm_50+ when compiled
// with MARKOV_CERO_HAS_CUDA, CPU reference otherwise.
inline HardwareBackend detect_best_backend() noexcept {
#ifdef MARKOV_CERO_HAS_CUDA
    return is_gpu_available() ? HardwareBackend::cuda_nvidia
                              : HardwareBackend::cpu_reference;
#else
    return HardwareBackend::cpu_reference;
#endif
}

} // namespace markov_cero::gpu
