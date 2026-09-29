#pragma once

// Test-only host allocation-failure injection (W02 / backlog item 3).
//
// Including this header REPLACES the program's global operator new/delete, so
// exactly ONE translation unit per test binary may include it, and only that
// binary may be linked against it.
//
// Usage: arm(nth) before the call under test, disarm() immediately after.
// The nth allocation counted from arming fails exactly once (std::bad_alloc
// for the throwing forms, nullptr for the nothrow forms); every other
// allocation passes through to malloc/free. Counting past the nth allocation
// keeps stack unwinding and the test's own bookkeeping safe.

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

namespace markov_cero::test {

class FailingNew final {
  public:
    /// Arm injection: the nth allocation from this call fails. `nth` is
    /// clamped to at least 1 so arming always targets a real allocation.
    static void arm(std::size_t nth) noexcept {
        counter_.store(0, std::memory_order_relaxed);
        fail_at_.store(nth == 0 ? 1 : nth, std::memory_order_relaxed);
        failures_.store(0, std::memory_order_relaxed);
        armed_.store(true, std::memory_order_release);
    }

    static void disarm() noexcept { armed_.store(false, std::memory_order_release); }

    /// Allocations observed since the last arm(), injected or not.
    static std::size_t allocations_while_armed() noexcept {
        return counter_.load(std::memory_order_relaxed);
    }

    /// Failures actually injected since the last arm().
    static std::size_t failures_injected() noexcept {
        return failures_.load(std::memory_order_relaxed);
    }

    static bool should_fail() noexcept {
        if (!armed_.load(std::memory_order_acquire)) return false;
        const std::size_t seen = counter_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (seen != fail_at_.load(std::memory_order_relaxed)) return false;
        failures_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

  private:
    inline static std::atomic<bool> armed_{false};
    inline static std::atomic<std::size_t> counter_{0};
    inline static std::atomic<std::size_t> fail_at_{0};
    inline static std::atomic<std::size_t> failures_{0};
};

} // namespace markov_cero::test

namespace {

void* mc_raw_allocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }

void* mc_raw_aligned_allocate(std::size_t size, std::size_t align) {
    if (align < alignof(std::max_align_t)) align = alignof(std::max_align_t);
    if (size > std::numeric_limits<std::size_t>::max() - (align - 1)) return nullptr;
    const std::size_t rounded = ((size + align - 1) / align) * align;
    return std::aligned_alloc(align, rounded == 0 ? align : rounded);
}

} // namespace

void* operator new(std::size_t size) {
    if (markov_cero::test::FailingNew::should_fail()) throw std::bad_alloc();
    if (void* p = mc_raw_allocate(size)) return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (markov_cero::test::FailingNew::should_fail()) return nullptr;
    return mc_raw_allocate(size);
}

void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept {
    return ::operator new(size, tag);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    if (markov_cero::test::FailingNew::should_fail()) throw std::bad_alloc();
    if (void* p = mc_raw_aligned_allocate(size, static_cast<std::size_t>(alignment))) return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
    if (markov_cero::test::FailingNew::should_fail()) return nullptr;
    return mc_raw_aligned_allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t& tag) noexcept {
    return ::operator new(size, alignment, tag);
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    std::free(p);
}
