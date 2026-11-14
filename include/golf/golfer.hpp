// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// The golfer: how a swing the player chooses (club, aim, power, shape)
// becomes a delivery at impact, with the scatter a golfer of a given skill
// adds; and the computer golfer, which plans its shots the way a caddie would:
// club and target chosen to minimise the expected strokes to hole out
// (Broadie's strokes-gained baselines, "Every Shot Counts", 2014), aimed for
// the wind by simulating the shot, and putts read by simulating the roll.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "golf/ball.hpp"
#include "golf/club.hpp"
#include "golf/course.hpp"
#include "golf/impact.hpp"
#include "golf/json.hpp"
#include "golf/rng.hpp"
#include "golf/shot.hpp"

namespace golf {

struct GolferProfile {
    std::string id = "scratch", name = "Scratch";
    double speedFactor = 0.93;  // club speed relative to the tour averages
    double faceSdDeg = 2.2, pathSdDeg = 2.4, strikeSdM = 0.008, speedSd = 0.022, loftSdDeg = 1.4;
    double puttSpeedSd = 0.05, puttAimSdDeg = 0.65;
    double handicap = 0;
};

std::vector<GolferProfile> loadGolfers(const json::Value& root);
std::vector<GolferProfile> standardGolfers();  // data/golfers/golfers.json
GolferProfile findGolfer(const std::string& id);

// The player's choice. Angles in degrees, heading in radians.
struct SwingInput {
    std::string club = "I7";
    double aimHeading = 0;  // the target line
    double power = 1.0;     // fraction of a full swing's club speed (putter: of 4 m/s)
    double faceDeg = 0;     // face angle to the target line at impact (+ open/right)
    double pathDeg = 0;     // club path (+ right / in-to-out)
    double loftDeg = 0;     // added dynamic loft (+ high/flop, - punch)
    double attackDeg = 0;   // added attack angle
    double strikeToeMm = 0, strikeUpMm = 0;
    bool exact = false;     // no golfer scatter (practice, previews, tests)
};

inline constexpr double kPutterFullSpeed = 4.0;  // m/s head speed at power 1

struct Lie {
    const Surface* surface = nullptr;
    bool plugged = false;
    bool teed = false;
};

// Delivery for an input, golfer and lie; draws scatter from rng unless input.exact.
Delivery makeDelivery(const SwingInput& in, const Club& club, const GolferProfile& g, const Lie& lie,
                      double difficulty, bool leftHanded, Rng& rng);

// Expected strokes to hole out (tour baseline) from `distance` metres on `surface`.
double expectedStrokes(double distanceM, const Surface& surface, bool water = false);

// Everything a plan needs to try a shot.
struct PlayContext {
    const Course* course = nullptr;
    const HoleDef* hole = nullptr;
    Cup cup;
    const ClubSet* clubs = nullptr;
    const BallModel* ball = nullptr;
    Conditions air;
    WindSpec wind;  // mean wind (no gusts) for planning
    Vec3 ballPos;
    int surface = 0;
    bool plugged = false;
    bool teed = false;
    bool leftHanded = false;
};

// Simulate an input from the context without scatter.
ShotResult previewShot(const PlayContext& ctx, const SwingInput& in, const GolferProfile& g, bool withCup = true,
                       bool recordPath = false);

struct Plan {
    SwingInput input;
    Vec3 target;          // where the plan aims to finish
    double expected = 0;  // expected strokes after this shot
    std::string note;
};

Plan planShot(const PlayContext& ctx, const GolferProfile& g);
Plan planPutt(const PlayContext& ctx, const GolferProfile& g);

// A club's nominal full-swing carry and total on level fairway, no wind.
double nominalCarry(const Club& club, const GolferProfile& g, const BallModel& ball, const Conditions& air);

}  // namespace golf
