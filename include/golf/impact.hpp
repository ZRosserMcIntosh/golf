// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// Club-ball impact in three dimensions: an oblique collision of two rigid
// bodies, the ball (mass m, inertia k m r^2) and the club head (mass M, an
// inertia tensor about its centre of gravity, the CG a depth b behind the
// face), at the strike point on a face that may be curved (bulge and roll).
//
//   Delta u = K J,   K = (1/m + 1/M) I - [r_b]x I_b^-1 [r_b]x - [r_c]x I_c^-1 [r_c]x
//
// J is the impulse on the ball, u the velocity of the ball's contact point
// relative to the club's. The normal relative velocity reverses with the
// coefficient of restitution e; the tangential one with Cross's tangential
// restitution e_t (Cross 2005, "Bounce of a spinning ball near normal
// incidence", Am. J. Phys. 73, 914: the compressed ball springs back and
// overspins; e_t = 0 is the classic "rolls off the face" model of Penner
// 2003); if that needs more than mu J_n the ball slides throughout (Coulomb).
// Gear effect, the face bulge and roll, and the D-plane all follow from the
// geometry: nothing is added for them.
//
// The 2-D form of this model (the site's golf aerodynamics page) was fitted
// to the TrackMan averages: e_t = 0.33, mu = 0.4.
#pragma once

#include "golf/ball.hpp"
#include "golf/club.hpp"
#include "golf/math.hpp"
#include "golf/shot.hpp"

namespace golf {

// What the golfer delivers at impact, in TrackMan's terms.
struct Delivery {
    double clubSpeed = 0;     // m/s, the head's CG
    double attackDeg = 0;     // + up
    double pathDeg = 0;       // + right of the target line (in-to-out for a right-hander)
    double dynamicLoftDeg = 0;
    double faceAngleDeg = 0;  // + open, right of the target line
    double strikeToeM = 0;    // strike point from the face centre toward the toe
    double strikeUpM = 0;     // ... toward the crown
    bool leftHanded = false;
    double frictionScale = 1.0;  // grass between face and ball (the lie) lowers mu and e_t
};

struct ImpactResult {
    Vec3 ballVelocity;   // world frame, target along +x
    Vec3 ballSpin;       // rad/s
    double speed = 0, vlaDeg = 0, hlaDeg = 0, spinRpm = 0, spinAxisDeg = 0;
    double smash = 0, spinLoftDeg = 0;
    bool sliding = false;
    double contactTimeS = 0;
};

// Impact for a target line along +x. The result's angles are relative to
// the target line, in the launch-monitor conventions of shot.hpp.
ImpactResult strike(const Club& club, const BallModel& ball, const Delivery& d);

// Turn an impact into a Launch from `position` toward `targetHeading`.
Launch toLaunch(const ImpactResult& r, const Vec3& position, double targetHeading);

// The dynamic loft that, at the club's tour speed and attack angle, launches
// the ball at the club's tour launch angle (solved by secant iteration).
double solveDynamicLoft(const Club& club, const BallModel& ball, double speed, double attackDeg, double launchDeg);

}  // namespace golf
