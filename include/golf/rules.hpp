// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// The Rules of Golf (R&A and USGA, 2023 edition) as far as a shot's result
// needs them:
//   - holed (Definitions: "Holed");
//   - out of bounds and lost: stroke and distance (Rule 18.2), or the Model
//     Local Rule E-5 alternative (two strokes, drop near where the ball
//     went out, on the fairway side; Committee Procedures 8E-5);
//   - penalty areas: one stroke, back-on-the-line relief (Rule 17.1d(2)), or
//     for a red area lateral relief within two club-lengths of where the ball
//     last crossed into it, not nearer the hole (Rule 17.1d(3));
//   - an embedded ball in a bunker is played as it lies (Rule 16.3 gives
//     relief only in the general area);
//   - a maximum score per hole (Rule 21.1, the Stableford-style pick-up most
//     casual rounds use, configurable).
#pragma once

#include <string>

#include "golf/course.hpp"
#include "golf/shot.hpp"

namespace golf {

struct RulesOptions {
    bool localRuleE5 = false;   // stroke-and-distance alternative for OB / lost
    double clubLength = 1.143;  // m: the longest club (driver, 45 in) measures relief areas
};

struct Ruling {
    enum class Kind { Play, Holed, PenaltyArea, OutOfBounds, Lost } kind = Kind::Play;
    int penalty = 0;
    bool replay = false;   // stroke and distance: play again from the previous spot
    Vec3 next;             // where the next stroke is played from
    int nextSurface = 0;
    bool plugged = false;
    std::string note;
};

Ruling rule(const Course& c, const HoleDef& h, const Vec3& pin, const Vec3& from, int fromSurface,
            const ShotResult& r, const RulesOptions& o = {});

const char* rulingName(Ruling::Kind k);

}  // namespace golf
