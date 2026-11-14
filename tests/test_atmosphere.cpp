// Golf Dynamics — air density and viscosity.
// SPDX-License-Identifier: MIT
#include "golf/atmosphere.hpp"
#include "golf/ball.hpp"
#include "golf_test.hpp"

using namespace golf;

TEST_CASE("atmosphere: ISA sea level and pressure lapse") {
    CHECK_NEAR(atmosphere::isaPressure(0), 101325.0, 1e-6);
    // ISO 2533: 89 875 Pa at 1000 m, 79 495 Pa at 2000 m.
    CHECK_NEAR(atmosphere::isaPressure(1000), 89875.0, 15.0);
    CHECK_NEAR(atmosphere::isaPressure(2000), 79495.0, 15.0);
}

TEST_CASE("atmosphere: CIPM-2007 density") {
    // Dry air at 15 C, 101 325 Pa: 1.2250 kg/m^3 in the ISA (CIPM gives 1.2254
    // with 400 ppm CO2).
    CHECK_NEAR(atmosphere::moistAirDensity(101325, 288.15, 0.0), 1.2252, 0.0006);
    // Picard et al. (2008) worked example: 20 C, 101 325 Pa, 50 % RH, 1.1992 kg/m^3
    // (the paper's table example with x_CO2 = 0.0004 rounds to 1.199).
    CHECK_NEAR(atmosphere::moistAirDensity(101325, 293.15, 0.5), 1.1992, 0.0008);
    // Humid air is lighter than dry air.
    CHECK(atmosphere::moistAirDensity(101325, 303.15, 0.9) < atmosphere::moistAirDensity(101325, 303.15, 0.0));
    // Saturation vapour pressure at 20 C: 2339 Pa.
    CHECK_NEAR(atmosphere::saturationVapourPressure(293.15), 2339.0, 5.0);
}

TEST_CASE("atmosphere: Sutherland viscosity") {
    // U.S. Standard Atmosphere 1976: 1.7894e-5 Pa s at 288.15 K.
    CHECK_NEAR(atmosphere::sutherlandViscosity(288.15), 1.7894e-5, 2e-9);
}

TEST_CASE("ball: coefficient limits and trends") {
    const BallModel b = BallModel::tour();
    const auto lo = b.coefficients(70, 0, 1.2, 1.8e-5);
    CHECK_NEAR(lo.cl, 0.0, 1e-12);
    CHECK_RANGE(lo.cd, 0.18, 0.20);  // supercritical, no spin
    const auto slow = b.coefficients(10, 0, 1.2, 1.8e-5);
    CHECK(slow.cd > 0.28);  // below the drag crisis
    const auto s1 = b.coefficients(50, 300, 1.2, 1.8e-5), s2 = b.coefficients(50, 600, 1.2, 1.8e-5);
    CHECK(s2.cl > s1.cl);
    CHECK(s2.cd > s1.cd);
    // Ferguson et al. (2022) at S = 0.2: Cd = 0.1304 + 0.9287*0.2 - 0.8259*0.04 = 0.2831.
    const BallModel f = BallModel::ferguson2022();
    const double spin = 0.2 * 50 / f.radius();
    CHECK_NEAR(f.coefficients(50, spin, 1.2, 1.8e-5).cd, 0.283104, 1e-6);
}
