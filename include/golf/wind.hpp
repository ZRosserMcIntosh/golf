// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// Wind over a course: a mean wind that grows with height above the local
// ground by the neutral log law of the surface layer,
//     U(h) = U_ref ln((h + z0) / z0) / ln((h_ref + z0) / z0),
// (the logarithmic wind profile; e.g. Stull 1988, "An Introduction to
// Boundary Layer Meteorology", sec. 9.7), with the reference wind at the
// standard 10 m anemometer height (WMO No. 8), plus gusts: each horizontal
// component an Ornstein-Uhlenbeck process (exponentially correlated, the
// simplest model with a finite integral time scale), sampled exactly at
// the integrator's fixed step from a seeded Rng so a shot replays exactly.
// z0 = 0.03 m is "open flat terrain, grass" in the Davenport-Wieringa
// roughness classification (Wieringa 1992); a parkland course with trees is
// rougher (0.1-0.25 m).
#pragma once

#include <cstdint>

#include "golf/math.hpp"
#include "golf/rng.hpp"

namespace golf {

struct WindSpec {
    double speed10 = 0.0;          // mean wind at 10 m, m/s
    double towardHeading = 0.0;    // direction the wind blows toward (radians, from +x toward +z)
    double roughnessM = 0.03;      // z0
    double gustIntensity = 0.0;    // turbulence intensity sigma_u / U10 (0.1-0.2 typical over grass)
    double gustTimeScaleS = 2.0;   // integral time scale of the gusts, s
};

// The wind seen by one shot. Gusts are a function of shot time only (the ball
// moves a few hundred metres, small against a gust's length scale), advanced
// by step(); the profile is a function of height above the local ground.
class WindField {
public:
    WindField() = default;
    WindField(const WindSpec& spec, std::uint64_t seed);

    const WindSpec& spec() const { return spec_; }
    // Wind velocity (m/s) at height h above the ground, including the current gust.
    Vec3 at(double heightAboveGround) const;
    // Mean wind only (no gust).
    Vec3 mean(double heightAboveGround) const;
    // Advance the gust state by dt seconds.
    void step(double dt);
    // Profile factor U(h)/U_ref.
    double profile(double h) const;

private:
    WindSpec spec_{};
    Rng rng_{};
    double gustAlong_ = 0, gustAcross_ = 0;
};

}  // namespace golf
