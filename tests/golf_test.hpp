// Golf Dynamics — minimal dependency-free test harness (from Aviation Dynamics,
// same author, MIT).
// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace golftest {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}
inline int& failures() {
    static int f = 0;
    return f;
}
struct Registrar {
    Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

// Long scenarios run at full length in optimised builds and shortened in
// Debug/sanitizer builds (same invariants, fewer simulated seconds).
inline constexpr bool kOptimisedBuild =
#ifdef NDEBUG
    true;
#else
    false;
#endif

}  // namespace golftest

#define GOLF_CAT2(a, b) a##b
#define GOLF_CAT(a, b) GOLF_CAT2(a, b)
#define TEST_CASE(name)                                                                                 \
    static void GOLF_CAT(golf_test_fn_, __LINE__)();                                                      \
    static golftest::Registrar GOLF_CAT(golf_test_reg_, __LINE__)(name, &GOLF_CAT(golf_test_fn_, __LINE__)); \
    static void GOLF_CAT(golf_test_fn_, __LINE__)()

#define CHECK(cond)                                                                        \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            ++golftest::failures();                                                         \
            std::cerr << "  FAILED " << __FILE__ << ":" << __LINE__ << ": " #cond << "\n"; \
        }                                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                                                  \
    do {                                                                                                       \
        const double golf_a_ = (a), golf_b_ = (b), golf_t_ = (tol);                                               \
        if (!(std::abs(golf_a_ - golf_b_) <= golf_t_)) {                                                          \
            ++golftest::failures();                                                                             \
            std::cerr << "  FAILED " << __FILE__ << ":" << __LINE__ << ": |" #a " - " #b "| <= " #tol << "  (" \
                      << golf_a_ << " vs " << golf_b_ << ")\n";                                                  \
        }                                                                                                      \
    } while (0)

#define CHECK_RANGE(v, lo, hi)                                                                                     \
    do {                                                                                                           \
        const double golf_v_ = (v);                                                                                 \
        if (!(golf_v_ >= (lo) && golf_v_ <= (hi))) {                                                                 \
            ++golftest::failures();                                                                                 \
            std::cerr << "  FAILED " << __FILE__ << ":" << __LINE__ << ": " #v " = " << golf_v_ << " not in [" << (lo) \
                      << ", " << (hi) << "]\n";                                                                    \
        }                                                                                                          \
    } while (0)
