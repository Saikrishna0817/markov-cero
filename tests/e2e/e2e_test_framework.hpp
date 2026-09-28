#pragma once

// markov-cero: Clean-Room Sovereign E2E Test Framework
// Pure C++20, zero external test dependencies (no gtest, boost, eigen, fmt).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace markov_cero::testing {

class AssertionFailure : public std::exception {
public:
    AssertionFailure(std::string message, const char* file, int line)
        : message_(std::move(message)), file_(file), line_(line) {
        std::ostringstream oss;
        oss << file_ << ":" << line_ << ": " << message_;
        full_what_ = oss.str();
    }

    [[nodiscard]] const char* what() const noexcept override {
        return full_what_.c_str();
    }

    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] const char* file() const noexcept { return file_; }
    [[nodiscard]] int line() const noexcept { return line_; }

private:
    std::string message_;
    const char* file_;
    int line_;
    std::string full_what_;
};

#define E2E_ASSERT(condition, message)                                         \
    do {                                                                       \
        if (!(condition)) {                                                    \
            throw ::markov_cero::testing::AssertionFailure(                    \
                std::string("Assertion failed: (") + #condition + ") - " +     \
                    std::string(message),                                      \
                __FILE__, __LINE__);                                           \
        }                                                                      \
    } while (false)

#define E2E_ASSERT_TRUE(condition) E2E_ASSERT(condition, "expected true")
#define E2E_ASSERT_FALSE(condition) E2E_ASSERT(!(condition), "expected false")

#define E2E_ASSERT_NEAR(actual, expected, tol, message)                        \
    do {                                                                       \
        const double act_val = static_cast<double>(actual);                   \
        const double exp_val = static_cast<double>(expected);                   \
        const double diff = std::abs(act_val - exp_val);                       \
        if (diff > (tol) || std::isnan(diff)) {                                \
            std::ostringstream oss;                                            \
            oss << message << " (expected: " << exp_val                        \
                << ", actual: " << act_val << ", diff: " << diff               \
                << ", tolerance: " << (tol) << ")";                            \
            throw ::markov_cero::testing::AssertionFailure(                    \
                oss.str(), __FILE__, __LINE__);                                \
        }                                                                      \
    } while (false)

#define E2E_ASSERT_KKT(primal_res, dual_res, tol)                              \
    do {                                                                       \
        E2E_ASSERT_NEAR(primal_res, 0.0, tol, "Primal feasibility violation"); \
        E2E_ASSERT_NEAR(dual_res, 0.0, tol, "Dual feasibility violation");     \
    } while (false)

struct TestCase {
    std::string id;
    std::string name;
    std::string tier;
    std::string milestone;
    std::size_t feature_id{0};
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void add_test(TestCase tc) {
        tests_.push_back(std::move(tc));
    }

    [[nodiscard]] const std::vector<TestCase>& tests() const noexcept {
        return tests_;
    }

    int run(int argc, char** argv) {
        std::string tier_filter;
        std::string milestone_filter;
        std::string name_filter;
        bool verbose = false;
        bool list_only = false;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "--list") {
                list_only = true;
            } else if (arg == "--verbose" || arg == "-v") {
                verbose = true;
            } else if (arg == "--tier" && i + 1 < argc) {
                tier_filter = argv[++i];
            } else if (arg == "--milestone" && i + 1 < argc) {
                milestone_filter = argv[++i];
            } else if (arg == "--filter" && i + 1 < argc) {
                name_filter = argv[++i];
            }
        }

        if (list_only) {
            std::cout << "Registered E2E Tests (" << tests_.size() << "):\n";
            for (const auto& tc : tests_) {
                std::cout << "  [" << tc.id << "] " << tc.name
                          << " [Tier: " << tc.tier
                          << ", Milestone: " << tc.milestone
                          << ", Feature: " << tc.feature_id << "]\n";
            }
            return 0;
        }

        std::size_t passed = 0;
        std::size_t failed = 0;
        std::size_t skipped = 0;

        std::cout << "=======================================================\n";
        std::cout << " running E2E Test Suite (" << tests_.size() << " tests registered)\n";
        std::cout << "=======================================================\n";

        const auto suite_start = std::chrono::steady_clock::now();

        for (const auto& tc : tests_) {
            if (!tier_filter.empty() && tc.tier != tier_filter) {
                ++skipped;
                continue;
            }
            if (!milestone_filter.empty() && tc.milestone != milestone_filter) {
                ++skipped;
                continue;
            }
            if (!name_filter.empty() && tc.name.find(name_filter) == std::string::npos &&
                tc.id.find(name_filter) == std::string::npos) {
                ++skipped;
                continue;
            }

            std::cout << "  RUN      " << tc.id << ": " << tc.name << " ... " << std::flush;
            const auto t0 = std::chrono::steady_clock::now();
            try {
                tc.func();
                const auto t1 = std::chrono::steady_clock::now();
                const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::cout << "OK (" << std::fixed << std::setprecision(2) << ms << " ms)\n";
                ++passed;
            } catch (const AssertionFailure& ex) {
                std::cout << "FAILED\n";
                std::cerr << "    >>> " << ex.what() << "\n";
                ++failed;
            } catch (const std::exception& ex) {
                std::cout << "ERROR\n";
                std::cerr << "    >>> Unexpected exception: " << ex.what() << "\n";
                ++failed;
            } catch (...) {
                std::cout << "ERROR\n";
                std::cerr << "    >>> Unknown non-standard exception thrown\n";
                ++failed;
            }
        }

        const auto suite_end = std::chrono::steady_clock::now();
        const double total_s = std::chrono::duration<double>(suite_end - suite_start).count();

        std::cout << "=======================================================\n";
        std::cout << " RESULTS: " << passed << " passed, " << failed << " failed, "
                  << skipped << " skipped in " << std::fixed << std::setprecision(3)
                  << total_s << " s\n";
        std::cout << "=======================================================\n";

        return (failed == 0) ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

struct TestRegistrar {
    TestRegistrar(std::string id, std::string name, std::string tier,
                  std::string milestone, std::size_t feature_id,
                  std::function<void()> func) {
        TestRegistry::instance().add_test(
            {std::move(id), std::move(name), std::move(tier),
             std::move(milestone), feature_id, std::move(func)});
    }
};

#define E2E_TEST(id, name, tier, milestone, feature_id)                        \
    static void test_func_##id();                                              \
    static const ::markov_cero::testing::TestRegistrar registrar_##id(          \
        #id, name, #tier, #milestone, feature_id, test_func_##id);             \
    static void test_func_##id()

} // namespace markov_cero::testing
