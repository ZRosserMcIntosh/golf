// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/wind.hpp"

#include <cmath>

namespace golf {

WindField::WindField(const WindSpec& spec, std::uint64_t seed) : spec_(spec), rng_(seed) {
    // Start the gusts from their stationary distribution, not from zero.
    const double sigma = spec_.gustIntensity * spec_.speed10;
    gustAlong_ = sigma * rng_.normal();
    gustAcross_ = 0.8 * sigma * rng_.normal();
}

double WindField::profile(double h) const {
    const double z0 = spec_.roughnessM > 1e-4 ? spec_.roughnessM : 1e-4;
    if (h < 0) h = 0;
    return std::log((h + z0) / z0) / std::log((10.0 + z0) / z0);
}

Vec3 WindField::mean(double h) const {
    return heading(spec_.towardHeading) * (spec_.speed10 * profile(h));
}

Vec3 WindField::at(double h) const {
    const Vec3 along = heading(spec_.towardHeading);
    const Vec3 across = kUp.cross(along);
    const double p = profile(h);
    return along * ((spec_.speed10 + gustAlong_) * p) + across * (gustAcross_ * p);
}

void WindField::step(double dt) {
    const double sigma = spec_.gustIntensity * spec_.speed10;
    if (sigma <= 0) return;
    // Exact OU update: x' = x a + sigma sqrt(1 - a^2) N, a = exp(-dt / T).
    // Lateral turbulence is about 0.8 of the longitudinal near the surface
    // (Panofsky & Dutton 1984: sigma_v / u* ~ 1.9 against sigma_u / u* ~ 2.4).
    const double a = std::exp(-dt / spec_.gustTimeScaleS);
    const double b = std::sqrt(1.0 - a * a);
    gustAlong_ = gustAlong_ * a + sigma * b * rng_.normal();
    gustAcross_ = gustAcross_ * a + 0.8 * sigma * b * rng_.normal();
}

}  // namespace golf
