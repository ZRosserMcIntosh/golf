// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// The ball and its aerodynamics: drag and lift coefficients as functions of
// Reynolds number Re = rho V D / mu and spin parameter S = omega r / V, and
// the decay of the spin in flight.
//
// Two coefficient models, chosen per ball in data/balls/*.json:
//
// - "sigmoid": the form and the "tour" constants of dimple
//   (github.com/divotmaker/dimple, Eric Thill, MIT/Apache-2.0; see NOTICE):
//       sigma = 1 / (1 + exp(-(Re - Re_crit) / Re_width))
//       Cd = Cd_sub + (Cd_super - Cd_sub) sigma + Cd_spin S / (1 + S / S_sat)
//       Cl = [Cl_sub + (Cl_super - Cl_sub) sigma] (1 - exp(-S / S_scale))
//   A drag crisis between a subcritical and a supercritical branch (Bearman &
//   Harvey 1976; Choi, Jeon & Choi 2006), spin drag that saturates, and lift
//   that saturates in S (Lyu et al. 2018). dimple fits the constants to the
//   TrackMan 2024 PGA Tour averages.
// - "quadratic": Cd and Cl quadratic in S, held above S_max, independent of
//   Re: Ferguson, McNally & McPhee (2022), "Golf ball aerodynamic
//   coefficients from launch-monitor data", ISEA, doi:10.5703/1288284317493.
//
// Spin decay, also per ball:
// - "exponential": d omega/dt = -lambda omega (about 4 %/s for a driver:
//   dimple and openflight both use 0.04/s);
// - "speed": d omega/dt = -k V omega / r, the speed-proportional form Alan
//   Nathan's golf trajectory calculator uses (k = 2e-5 there).
#pragma once

#include <string>

#include "golf/math.hpp"

namespace golf {

struct BallModel {
    std::string id = "tour";
    std::string name = "Tour ball (dimple 'TOUR' fit)";
    double massKg = 0.04593;   // Rules of Golf maximum, 1.620 oz
    double diameterM = 0.04267;  // Rules of Golf minimum, 1.680 in
    double inertiaFactor = 0.4;  // I / (m r^2); uniform sphere. Multi-layer balls 0.38-0.40

    enum class Coefficients { Sigmoid, Quadratic };
    Coefficients model = Coefficients::Sigmoid;
    // Sigmoid (dimple TOUR).
    double cdSub = 0.295, cdSuper = 0.186, cdSpin = 0.85, srSat = 0.25;
    double clSub = 0.203, clSuper = 0.324, srScale = 0.115;
    double reCrit = 69600.0, reWidth = 106.0;
    // Quadratic (Ferguson et al. 2022).
    double cd0 = 0.1304, cd1 = 0.9287, cd2 = -0.8259;
    double cl0 = 0.0504, cl1 = 1.2031, cl2 = -1.1490;
    double sMax = 0.75;

    enum class SpinDecay { Exponential, Speed };
    SpinDecay decay = SpinDecay::Exponential;
    double decayRate = 0.04;    // 1/s, exponential
    double decaySpeedK = 2e-5;  // speed form

    double radius() const { return diameterM / 2; }
    double area() const { return kPi * radius() * radius(); }

    struct Coeffs {
        double cd = 0, cl = 0, re = 0, s = 0;
    };
    // speed: airspeed (m/s); spin: the spin component across the airflow (rad/s).
    Coeffs coefficients(double speed, double spin, double density, double viscosity) const;

    // d omega / dt divided by omega (negative), for airspeed `speed`.
    double spinDecayRate(double speed) const;

    static BallModel tour();
    static BallModel ferguson2022();
};

}  // namespace golf
