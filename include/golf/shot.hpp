// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// One shot from launch to rest: flight (gravity, drag, Magnus lift, spin
// decay, wind, trees, the flagstick), the bounces (Penner 2002), the slide and
// roll on the terrain under the ball, and the hole (cup.hpp).
//
// Launch conventions follow the launch-monitor ones (TrackMan, GSPro Open
// Connect): vertical launch angle (VLA) up from horizontal; horizontal launch
// angle (HLA) and spin axis positive to the RIGHT of the target line, for any
// golfer; a positive spin axis curves the ball right.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "golf/atmosphere.hpp"
#include "golf/ball.hpp"
#include "golf/math.hpp"
#include "golf/wind.hpp"
#include "golf/world.hpp"

namespace golf {

struct Launch {
    Vec3 position;           // ball centre at address
    double targetHeading = 0;  // the target line (radians, from +x toward +z)
    double speed = 0;        // m/s
    double vlaDeg = 0;
    double hlaDeg = 0;       // + right of the target line
    double spinRpm = 0;      // total spin
    double spinAxisDeg = 0;  // + tilted right (ball curves right)
};

struct BallState {
    double t = 0;
    Vec3 pos, vel, spin;  // centre position, velocity, angular velocity (rad/s)
};

enum class Phase : std::uint8_t { Flight = 0, Hop = 1, Slide = 2, Roll = 3, Cup = 4, Rest = 5 };

enum class EventKind : std::uint8_t {
    Launch, Apex, Landing, Bounce, Tree, Trunk, Flagstick, Rim, Holed, Water, Plugged, Rest, Lost
};

struct ShotEvent {
    EventKind kind;
    double t;
    Vec3 pos;
    double speed;
    int surface;
};

struct PathPoint {
    double t;
    Vec3 pos;
    Phase phase;
    Vec3 spin;  // for drawing the ball turning
};

enum class Outcome : std::uint8_t { Rest = 0, Holed = 1, Water = 2, Lost = 3, Plugged = 4 };

struct ShotStats {
    double carry = 0;          // horizontal distance from launch to first landing, m
    double carryAlong = 0;     // ... along the target line
    double carryOffline = 0;   // ... right of the target line (+)
    double total = 0;          // horizontal distance launch to rest, m
    double totalAlong = 0, totalOffline = 0;
    double apex = 0;           // maximum height above the launch point, m
    double flightTime = 0;     // s to first landing
    double landingAngleDeg = 0;  // descent angle at first landing
    double landingSpeed = 0;
    double landingSpinRpm = 0;
    double totalTime = 0;
    int bounces = 0;
};

struct SimOptions {
    double flightDt = 0.001;   // s (RK4)
    double groundDt = 0.001;   // s
    double maxTime = 60.0;     // s
    double sampleInterval = 1.0 / 60.0;  // path decimation for drawing
    std::uint64_t seed = 1;    // gusts, trees
    bool recordPath = true;
};

struct ShotResult {
    Outcome outcome = Outcome::Rest;
    BallState final;
    int finalSurface = 0;
    ShotStats stats;
    std::vector<PathPoint> path;
    std::vector<ShotEvent> events;
};

// Ball state just after launch.
BallState launchState(const Launch& l);

ShotResult simulateShot(const Launch& launch, const BallModel& ball, const Conditions& cond, const WindSpec& wind,
                        const World& world, const SimOptions& opt = {});

// Flight only, to the first time the ball reaches the ground: for
// validation against launch-monitor carry tables (no bounce or roll).
ShotResult simulateCarry(const Launch& launch, const BallModel& ball, const Conditions& cond, const WindSpec& wind,
                         const SimOptions& opt = {});

const char* eventName(EventKind k);
const char* outcomeName(Outcome o);

}  // namespace golf
