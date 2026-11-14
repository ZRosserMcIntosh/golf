// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/club.hpp"

#include "golf/embedded.hpp"
#include "golf/impact.hpp"
#include "golf/math.hpp"

namespace golf {

int ClubSet::find(const std::string& code) const {
    for (size_t i = 0; i < clubs.size(); ++i)
        if (clubs[i].code == code) return static_cast<int>(i);
    return -1;
}

std::string ClubSet::load(const json::Value& root) {
    const json::Value* arr = root.isObject() ? root.find("clubs") : nullptr;
    if (!arr || !arr->isArray()) return "clubs: expected an array";
    std::vector<Club> out;
    const json::Value* src = root.find("source");
    for (const auto& v : arr->items()) {
        if (!v.isObject()) return "clubs: expected objects";
        Club c;
        if (src && src->isString()) c.source = src->asString();
        for (const auto& [k, f] : v.members()) {
            const double n = f.isNumber() ? f.asNumber() : 0.0;
            if (k == "code" && f.isString()) c.code = f.asString();
            else if (k == "name" && f.isString()) c.name = f.asString();
            else if (k == "loft") c.loftDeg = n;
            else if (k == "lie") c.lieDeg = n;
            else if (k == "length_in") c.lengthM = n * 0.0254;
            else if (k == "head_g") c.headMassKg = n / 1000;
            else if (k == "moi_heel_toe_gcm2") c.moiHeelToe = n * 1e-7;
            else if (k == "moi_vertical_gcm2") c.moiVertical = n * 1e-7;
            else if (k == "cg_depth_mm") c.cgDepthM = n / 1000;
            else if (k == "cg_below_center_mm") c.cgBelowCenterM = n / 1000;
            else if (k == "bulge_mm") c.bulgeRadiusM = n / 1000;
            else if (k == "roll_mm") c.rollRadiusM = n / 1000;
            else if (k == "cor") c.cor = n;
            else if (k == "tangential_restitution") c.tangentialRestitution = n;
            else if (k == "friction") c.friction = n;
            else if (k == "putter") c.putter = f.isBool() && f.asBool();
            else if (k == "wood") c.wood = f.isBool() && f.asBool();
            else if (k == "tour" && f.isObject()) {
                for (const auto& [tk, tv] : f.members()) {
                    const double t = tv.isNumber() ? tv.asNumber() : 0.0;
                    if (tk == "speed_mph") c.tourSpeedMph = t;
                    else if (tk == "attack") c.tourAttackDeg = t;
                    else if (tk == "launch") c.tourLaunchDeg = t;
                    else if (tk == "dynamic_loft") c.tourDynamicLoftDeg = t;
                    else if (tk == "ball_speed_mph") c.tourBallSpeedMph = t;
                }
            }
        }
        if (c.moiHeelToe <= 0 || c.moiVertical <= 0 || c.headMassKg <= 0) return "clubs: " + c.code + ": bad mass properties";
        // Calibrate: the dynamic loft that gives the tour launch angle, and the
        // head-speed factor that gives the tour ball speed (two passes, as each
        // depends a little on the other).
        const bool solveLoft = c.tourDynamicLoftDeg == 0;
        if (!solveLoft) c.speedCalibration = 1.0;
        for (int pass = 0; pass < 3; ++pass) {
            const double speed = c.tourSpeedMph * kMph * c.speedCalibration;
            if (solveLoft) c.tourDynamicLoftDeg = solveDynamicLoft(c, BallModel::tour(), speed, c.tourAttackDeg, c.tourLaunchDeg);
            if (c.tourBallSpeedMph > 0) {
                Delivery d;
                d.clubSpeed = speed;
                d.attackDeg = c.tourAttackDeg;
                d.dynamicLoftDeg = c.tourDynamicLoftDeg;
                const double got = strike(c, BallModel::tour(), d).speed;
                if (got > 0) c.speedCalibration *= c.tourBallSpeedMph * kMph / got;
            } else if (!out.empty() && !c.putter) {
                c.speedCalibration = out.back().speedCalibration;
            }
        }
        out.push_back(c);
    }
    clubs = std::move(out);
    return "";
}

ClubSet ClubSet::standard() {
    ClubSet s;
    const auto r = json::parse(embedded::clubsStandard());
    if (r.ok()) s.load(r.value);
    return s;
}

}  // namespace golf
