// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// Clubs: the head's mass properties and face, and the delivery a tour player
// gives it (TrackMan PGA Tour averages: club speed and attack angle).
// Codes follow GSPro Open Connect (DR, W3, H4, I7, PW, GW, SW, LW, PT).
#pragma once

#include <string>
#include <vector>

#include "golf/json.hpp"

namespace golf {

struct Club {
    std::string code = "I7";
    std::string name = "7-iron";
    double loftDeg = 33;     // static loft
    double lieDeg = 62.5;
    double lengthM = 0.946;
    double headMassKg = 0.269;
    // Head moments of inertia about its centre of gravity (kg m^2):
    double moiHeelToe = 0.6e-4;  // about the heel-toe axis (vertical gear effect)
    double moiVertical = 2.5e-4; // about the vertical axis (twist on toe/heel strikes)
    double cgDepthM = 0.006;     // CG behind the face
    double cgBelowCenterM = 0.004; // CG below the face centre (a centred strike is this far above the CG)
    double bulgeRadiusM = 0;     // face curvature heel-toe (0: flat face)
    double rollRadiusM = 0;      // face curvature crown-sole
    double cor = 0.78;           // normal restitution with the ball
    double tangentialRestitution = 0.33;  // Cross (2005): the ball's tangential spring-back
    double friction = 0.40;      // ball-face Coulomb friction
    bool putter = false;
    bool wood = false;
    // Tour delivery (full swing): TrackMan PGA Tour averages.
    double tourSpeedMph = 90;
    double tourAttackDeg = -4.3;
    double tourLaunchDeg = 16.3;   // the launch the delivered loft is solved for
    double tourDynamicLoftDeg = 0; // solved at load (club.cpp) unless given
    double tourBallSpeedMph = 0;   // TrackMan ball speed, if known
    double speedCalibration = 1.0; // delivered head speed / nominal, solved so the tour ball speed matches
    std::string source;
};

struct ClubSet {
    std::vector<Club> clubs;
    int find(const std::string& code) const;
    // Load from JSON; returns "" or an error.
    std::string load(const json::Value& root);
    static ClubSet standard();  // the 14-club bag of data/clubs/standard.json
};

}  // namespace golf
