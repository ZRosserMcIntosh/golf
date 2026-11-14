// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// Deterministic random numbers. Every random effect in the core (gusts, tree
// deflections, a golfer's dispersion) draws from an Rng seeded explicitly, so
// a shot replays bit for bit from its inputs and seed: native and WebAssembly
// builds, and every player in a multiplayer round, compute the same result.
// SplitMix64 (Steele, Lea & Flood 2014) seeds xoshiro256** (Blackman & Vigna
// 2018); the normal deviate uses Box–Muller.
#pragma once

#include <cmath>
#include <cstdint>

namespace golf {

class Rng {
public:
    explicit Rng(std::uint64_t seed = 0x9E3779B97F4A7C15ull) { reseed(seed); }

    void reseed(std::uint64_t seed) {
        std::uint64_t x = seed;
        for (auto& s : s_) s = splitmix(x);
        haveSpare_ = false;
    }

    std::uint64_t next() {
        const std::uint64_t result = rotl(s_[1] * 5, 7) * 9;
        const std::uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    // Uniform in [0, 1).
    double uniform() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
    double uniform(double lo, double hi) { return lo + (hi - lo) * uniform(); }

    // Standard normal deviate.
    double normal() {
        if (haveSpare_) {
            haveSpare_ = false;
            return spare_;
        }
        double u = uniform();
        while (u <= 1e-300) u = uniform();
        const double v = uniform();
        const double r = std::sqrt(-2.0 * std::log(u));
        const double a = 2.0 * 3.14159265358979323846 * v;
        spare_ = r * std::sin(a);
        haveSpare_ = true;
        return r * std::cos(a);
    }
    double normal(double mean, double sd) { return mean + sd * normal(); }

    // A derived seed: lets one match seed give every hole and shot its own stream.
    static std::uint64_t mix(std::uint64_t a, std::uint64_t b) {
        std::uint64_t x = a ^ (b + 0x9E3779B97F4A7C15ull + (a << 6) + (a >> 2));
        return splitmix(x);
    }

private:
    static std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    static std::uint64_t splitmix(std::uint64_t& x) {
        std::uint64_t z = (x += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    std::uint64_t s_[4]{};
    double spare_ = 0;
    bool haveSpare_ = false;
};

}  // namespace golf
