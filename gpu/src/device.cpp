#include "markov_cero/gpu/device.hpp"

#ifdef MARKOV_CERO_HAS_CUDA
#include <cuda_runtime.h>
#endif

#include <cstdio>

namespace markov_cero::gpu {

bool is_gpu_available() noexcept {
#ifdef MARKOV_CERO_HAS_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess || count <= 0) {
        return false;
    }
    // D-06 (LOCKED): devices below sm_50 are outside the compiled
    // architecture range (all-major = sm_50+). Warn (not fail) and report the
    // device as unavailable so callers fall back to CPU engines. The warning
    // goes to stderr once per detection.
    static bool warned = false;
    for (int device = 0; device < count; ++device) {
        cudaDeviceProp props;
        if (cudaGetDeviceProperties(&props, device) != cudaSuccess) {
            continue;
        }
        if (props.major < 5) {
            if (!warned) {
                std::fprintf(stderr,
                             "[gpu] WARNING: device sm_%d%d is below the supported sm_50 minimum. "
                             "Falling back to CPU PDLP.\n",
                             props.major, props.minor);
                warned = true;
            }
            continue;
        }
        return true;
    }
    return false;
#else
    return false;
#endif
}

int get_device_count() noexcept {
#ifdef MARKOV_CERO_HAS_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess || count < 0) {
        return 0;
    }
    return count;
#else
    return 0;
#endif
}

DeviceInfo get_device_info(int device_id) noexcept {
    DeviceInfo info;
#ifdef MARKOV_CERO_HAS_CUDA
    int count = get_device_count();
    if (device_id < 0 || device_id >= count) {
        return info;
    }

    cudaDeviceProp props;
    if (cudaGetDeviceProperties(&props, device_id) != cudaSuccess) {
        return info;
    }

    size_t free_bytes = 0;
    size_t total_bytes = 0;
    cudaSetDevice(device_id);
    cudaMemGetInfo(&free_bytes, &total_bytes);

    info.available = true;
    info.device_id = device_id;
    info.name = props.name;
    info.total_memory_bytes = total_bytes;
    info.free_memory_bytes = free_bytes;
    info.compute_capability_major = props.major;
    info.compute_capability_minor = props.minor;
    info.warp_size = props.warpSize;
    info.max_threads_per_block = props.maxThreadsPerBlock;
    return info;
#else
    (void)device_id;
    info.name = "CPU Fallback (No CUDA)";
    return info;
#endif
}

void synchronize_device() {
#ifdef MARKOV_CERO_HAS_CUDA
    cudaDeviceSynchronize();
#endif
}

} // namespace markov_cero::gpu
