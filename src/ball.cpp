// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/ball.hpp"

#include <cmath>

namespace golf {

BallModel::Coeffs BallModel::coefficients(double speed, double spin, double density, double viscosity) const {
    Coeffs c;
    if (speed < 1e-9) {
        c.cd = model == Coefficients::Sigmoid ? cdSub : cd0;
        return c;
    }
    c.re = density * speed * diameterM / viscosity;
    c.s = std::abs(spin) * radius() / speed;
    if (model == Coefficients::Sigmoid) {
        // dimple TOUR form (see header). The exponent is clamped so a far
        // sub- or supercritical Re cannot overflow.
        const double arg = clamp(-(c.re - reCrit) / reWidth, -60.0, 60.0);
        const double sigma = 1.0 / (1.0 + std::exp(arg));
        const double satDen = std::isinf(srSat) ? 1.0 : 1.0 + c.s / srSat;
        c.cd = cdSub + (cdSuper - cdSub) * sigma + cdSpin * c.s / satDen;
        const double clMax = clSub + (clSuper - clSub) * sigma;
        c.cl = clMax * (1.0 - std::exp(-c.s / srScale));
    } else {
        const double s = c.s < sMax ? c.s : sMax;
        c.cd = cd0 + cd1 * s + cd2 * s * s;
        c.cl = cl0 + cl1 * s + cl2 * s * s;
        if (c.s < 1e-9) c.cl = 0;  // no spin, no Magnus lift
    }
    return c;
}

double BallModel::spinDecayRate(double speed) const {
    if (decay == SpinDecay::Exponential) return -decayRate;
    return -decaySpeedK * speed / radius();
}

BallModel BallModel::tour() { return BallModel{}; }

BallModel BallModel::ferguson2022() {
    BallModel b;
    b.id = "ferguson2022";
    b.name = "Launch-monitor fit (Ferguson, McNally & McPhee 2022)";
    b.model = Coefficients::Quadratic;
    return b;
}

}  // namespace golf
