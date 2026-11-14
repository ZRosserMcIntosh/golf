// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// The ball at the hole: a small rigid-body simulation of a sphere against the
// green plane, the circular rim, the cup's wall and bottom, and the
// flagstick, under gravity, with impulsive contacts (restitution plus
// Coulomb friction that couples spin and slip).
//
// Nothing about capture is scripted: the ball drops when its centre passes
// over the hole and falls until it meets the rim, the wall or the far edge.
// Over the centre of the hole it is captured if it has dropped about one ball
// radius by the time it reaches the far edge, so the rim strikes it above its
// centre. That gives a capture speed of 1.6 m/s, the 1.63 m/s of Penner
// (2002, "The physics of putting", Can. J. Phys. 80, 83); a ball arriving
// off-centre has a shorter chord to cross and must be slower, as Holmes
// (1991, Am. J. Phys. 59, 129) found. tests/test_cup.cpp checks both.
//
// "Holed" follows the Rules of Golf (Definitions): the ball is at rest in
// the hole after a stroke and the whole of it is below the surface of the
// putting green. A ball resting against the flagstick with part of it below
// the surface is also holed (Rule 13.2c); the simulation lets such a ball
// drop or not.
#pragma once

#include <vector>

#include "golf/ball.hpp"
#include "golf/shot.hpp"
#include "golf/surface.hpp"
#include "golf/world.hpp"

namespace golf {

struct CupResult {
    enum class End { Left, Holed, Airborne, Timeout } end = End::Left;
    BallState state;
};

// True if the ball (centre `pos`) is close enough to the hole to hand it to
// cupSimulate: within a ball radius of the rim horizontally and low.
bool nearCup(const Cup& cup, const Vec3& greenNormal, const Vec3& pos, double ballRadius);

// Simulate from `s` until the ball leaves the hole's neighbourhood, is
// holed, or goes airborne. Appends path points and events.
CupResult cupSimulate(const BallState& s, const Cup& cup, const Vec3& greenNormal, const Surface& green,
                      const BallModel& ball, double tEnd, std::vector<PathPoint>* path, std::vector<ShotEvent>& events,
                      double sampleInterval);

}  // namespace golf
