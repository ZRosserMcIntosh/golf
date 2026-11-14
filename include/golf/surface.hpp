// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// Ground materials and how the ball meets them. Each surface carries the
// parameters of the bounce and roll models in ground.hpp, plus the "lie"
// effects a ball sitting on it has on the next strike (impact.hpp).
//
// The green and fairway bounce is Penner's (2002) green model; the green's
// rolling resistance is set from its Stimpmeter reading. The other surfaces
// are estimates and are labelled as such in data/surfaces/surfaces.json.
#pragma once

#include <string>
#include <vector>

#include "golf/json.hpp"

namespace golf {

enum class RollLaw { Constant, Quadratic };

struct Surface {
    std::string id = "fairway";
    std::string name = "Fairway";
    // Bounce.
    bool penner = true;      // Penner (2002) eq. 5 speed-dependent restitution
    double restitution = 0;  // used when !penner
    double restitutionScale = 1.0;  // firmness multiplier on Penner's e
    double muSlide = 0.40;   // Coulomb friction, ball on turf (Penner 2002: 0.4)
    double compliance = 1.0; // multiplier on Penner's effective contact (crater) angle
    double pivot = 0.001;    // torsional friction lever, m (dimple: Hertz full-slip couple)
    // Roll.
    double muRoll = 0.12;    // rolling resistance at zero speed
    RollLaw rollLaw = RollLaw::Quadratic;
    double rollVRef = 2.5;   // m/s, quadratic law f = 1 + (v / vRef)^2 (dimple)
    // Special behaviour.
    bool water = false;      // the ball is lost (it may skip first)
    bool sand = false;       // the ball can plug on a steep landing
    // Lie: the strike from a ball resting here.
    double lieSpeed = 1.0;       // fraction of the club-head speed delivered to the ball
    double lieFriction = 1.0;    // multiplier on the face-ball friction (grass between them)
    double lieDispersion = 1.0;  // multiplier on the golfer's dispersion
    bool puttable = false;       // the putter is the natural club here
    // Rendering hint (sRGB hex), used by the web page only.
    std::string color = "#3f8a3a";
    std::string source;

    // Normal restitution at a normal impact speed v (m/s).
    double restitutionAt(double v) const;
    // Rolling resistance at speed v.
    double rollingFriction(double v) const;
    // Stimpmeter reading (feet) of this surface: how far a ball leaving the
    // ramp rolling at 1.83 m/s (6.0 ft/s; Holmes 1986, Penner 2002) rolls on
    // level ground. Closed form for both roll laws (dimple).
    double stimpFeet(double inertiaFactor = 0.4) const;
    // Set muRoll so the surface reads `feet` on the Stimpmeter.
    void setStimp(double feet, double inertiaFactor = 0.4);
};

inline constexpr double kStimpmeterSpeed = 1.83;  // m/s (6.0 ft/s)

class SurfaceTable {
public:
    SurfaceTable();  // built-in defaults (same values as data/surfaces/surfaces.json)
    // Replace the table from JSON; returns an error message or "".
    std::string load(const json::Value& root);
    int find(const std::string& id) const;  // -1 if absent
    const Surface& operator[](int i) const { return list_[static_cast<size_t>(i < 0 || i >= (int)list_.size() ? 0 : i)]; }
    Surface& at(int i) { return list_[static_cast<size_t>(i)]; }
    int size() const { return static_cast<int>(list_.size()); }
    int add(const Surface& s);

private:
    std::vector<Surface> list_;
};

}  // namespace golf
