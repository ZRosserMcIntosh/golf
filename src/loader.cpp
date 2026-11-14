// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/loader.hpp"

#include <cmath>

#include "golf/embedded.hpp"

namespace golf {

std::string loadBalls(const json::Value& root, std::vector<BallModel>& out) {
    const json::Value* arr = root.isObject() ? root.find("balls") : nullptr;
    if (!arr || !arr->isArray()) return "balls: expected an array";
    for (const auto& v : arr->items()) {
        if (!v.isObject()) return "balls: expected objects";
        BallModel b;
        for (const auto& [k, f] : v.members()) {
            const double n = f.isNumber() ? f.asNumber() : 0.0;
            auto triple = [&](double& a, double& bb, double& c) {
                if (f.isArray() && f.items().size() == 3) {
                    a = f.items()[0].asNumber();
                    bb = f.items()[1].asNumber();
                    c = f.items()[2].asNumber();
                }
            };
            if (k == "id" && f.isString()) b.id = f.asString();
            else if (k == "name" && f.isString()) b.name = f.asString();
            else if (k == "mass_g") b.massKg = n / 1000;
            else if (k == "diameter_mm") b.diameterM = n / 1000;
            else if (k == "inertia_factor") b.inertiaFactor = n;
            else if (k == "model" && f.isString())
                b.model = f.asString() == "quadratic" ? BallModel::Coefficients::Quadratic : BallModel::Coefficients::Sigmoid;
            else if (k == "cd_sub") b.cdSub = n;
            else if (k == "cd_super") b.cdSuper = n;
            else if (k == "cd_spin") b.cdSpin = n;
            else if (k == "sr_sat") b.srSat = n;
            else if (k == "cl_sub") b.clSub = n;
            else if (k == "cl_super") b.clSuper = n;
            else if (k == "sr_scale") b.srScale = n;
            else if (k == "re_crit") b.reCrit = n;
            else if (k == "re_width") b.reWidth = n;
            else if (k == "cd") triple(b.cd0, b.cd1, b.cd2);
            else if (k == "cl") triple(b.cl0, b.cl1, b.cl2);
            else if (k == "s_max") b.sMax = n;
            else if (k == "spin_decay" && f.isString())
                b.decay = f.asString() == "speed" ? BallModel::SpinDecay::Speed : BallModel::SpinDecay::Exponential;
            else if (k == "decay_rate") b.decayRate = n;
            else if (k == "decay_speed_k") b.decaySpeedK = n;
        }
        if (!(b.massKg > 0 && b.diameterM > 0 && b.inertiaFactor > 0)) return "balls: " + b.id + ": bad mass properties";
        out.push_back(b);
    }
    return "";
}

std::vector<BallModel> standardBalls() {
    std::vector<BallModel> out;
    const auto r = json::parse(embedded::balls());
    if (r.ok()) loadBalls(r.value, out);
    if (out.empty()) out.push_back(BallModel::tour());
    return out;
}

BallModel findBall(const std::string& id) {
    for (const auto& b : standardBalls())
        if (b.id == id) return b;
    return BallModel::tour();
}

}  // namespace golf
