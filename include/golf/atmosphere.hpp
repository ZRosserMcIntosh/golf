// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// Air: density and viscosity from altitude, temperature, humidity and
// (optionally) measured station pressure.
//
// - Pressure from altitude: ICAO Standard Atmosphere (ISO 2533:1975)
//   troposphere, p = p0 (1 - L h / T0)^(g M / (R L)).
// - Moist-air density: the CIPM-2007 equation (Picard, Davis, Gläser &
//   Fujii 2008, "Revised formula for the density of moist air (CIPM-2007)",
//   Metrologia 45, 149-155), with its saturation vapour pressure, enhancement
//   factor and compressibility factor. CO2 mole fraction 0.0004.
// - Dynamic viscosity: Sutherland's law with the constants of the U.S.
//   Standard Atmosphere 1976 (beta = 1.458e-6 kg/(m s K^0.5), S = 110.4 K).
#pragma once

namespace golf {

struct Conditions {
    double altitudeM = 0.0;         // course elevation above mean sea level
    double temperatureC = 15.0;
    double relativeHumidity = 0.5;  // 0..1
    double stationPressurePa = 0;   // 0: derive from altitude (ISA)
};

struct Air {
    double density = 1.225;       // kg/m^3
    double viscosity = 1.789e-5;  // Pa s
    double pressure = 101325.0;   // Pa
    double temperatureK = 288.15;
};

namespace atmosphere {

double isaPressure(double altitudeM);
double saturationVapourPressure(double temperatureK);  // Pa, CIPM-2007
double moistAirDensity(double pressurePa, double temperatureK, double relativeHumidity);
double sutherlandViscosity(double temperatureK);
Air air(const Conditions& c);

}  // namespace atmosphere
}  // namespace golf
