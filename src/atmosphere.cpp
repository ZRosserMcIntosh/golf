// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/atmosphere.hpp"

#include <cmath>

#include "golf/math.hpp"

namespace golf::atmosphere {

double isaPressure(double h) {
    // ISO 2533 troposphere: T0 = 288.15 K, L = 0.0065 K/m, exponent g M / (R L) = 5.25588.
    const double t = 1.0 - 0.0065 * h / 288.15;
    return 101325.0 * std::pow(t > 0.05 ? t : 0.05, 5.25588);
}

double saturationVapourPressure(double T) {
    // CIPM-2007 (Picard et al. 2008), eq. A1.4.
    constexpr double A = 1.2378847e-5, B = -1.9121316e-2, C = 33.93711047, D = -6.3431645e3;
    return std::exp(A * T * T + B * T + C + D / T);
}

double moistAirDensity(double p, double T, double rh) {
    // CIPM-2007, eqs. A1.1-A1.5.
    constexpr double R = 8.314472;      // J/(mol K)
    constexpr double Ma = 28.96546e-3;  // kg/mol, dry air with x_CO2 = 0.0004
    constexpr double Mv = 18.01528e-3;  // kg/mol, water
    const double t = T - 273.15;
    rh = clamp(std::isnan(rh) ? 0.0 : rh, 0.0, 1.0);
    const double f = 1.00062 + 3.14e-8 * p + 5.6e-7 * t * t;  // enhancement factor
    const double xv = rh * f * saturationVapourPressure(T) / p;
    constexpr double a0 = 1.58123e-6, a1 = -2.9331e-8, a2 = 1.1043e-10, b0 = 5.707e-6, b1 = -2.051e-8,
                     c0 = 1.9898e-4, c1 = -2.376e-6, d = 1.83e-11, e = -0.765e-8;
    const double Z = 1.0 - (p / T) * (a0 + a1 * t + a2 * t * t + (b0 + b1 * t) * xv + (c0 + c1 * t) * xv * xv) +
                     (p * p) / (T * T) * (d + e * xv * xv);
    return p * Ma / (Z * R * T) * (1.0 - xv * (1.0 - Mv / Ma));
}

double sutherlandViscosity(double T) { return 1.458e-6 * T * std::sqrt(T) / (T + 110.4); }

Air air(const Conditions& c) {
    Air a;
    a.temperatureK = c.temperatureC + 273.15;
    a.pressure = c.stationPressurePa > 0 ? c.stationPressurePa : isaPressure(c.altitudeM);
    a.density = moistAirDensity(a.pressure, a.temperatureK, c.relativeHumidity);
    a.viscosity = sutherlandViscosity(a.temperatureK);
    return a;
}

}  // namespace golf::atmosphere
