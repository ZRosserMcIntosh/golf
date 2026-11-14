// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/surface.hpp"

#include <cmath>

#include "golf/embedded.hpp"
#include "golf/math.hpp"

namespace golf {

double Surface::restitutionAt(double v) const {
    if (!penner) return restitution;
    // Penner (2002), "The run of a golf ball", eq. 5: a fit to Haake's (1989)
    // measured ball-turf restitution against normal impact speed.
    v = std::abs(v);
    const double e = v <= 20.0 ? 0.510 - 0.0375 * v + 0.000903 * v * v : 0.120;
    return clamp(e * restitutionScale, 0.0, 0.95);
}

double Surface::rollingFriction(double v) const {
    if (rollLaw == RollLaw::Constant) return muRoll;
    const double s = v / rollVRef;
    return muRoll * (1.0 + s * s);
}

namespace {
double stimpIntegral(const Surface& s) {
    const double v0 = kStimpmeterSpeed;
    if (s.rollLaw == RollLaw::Constant) return v0 * v0;
    const double x = v0 * v0 / (s.rollVRef * s.rollVRef);
    return v0 * v0 * std::log1p(x) / x;
}
}  // namespace

double Surface::stimpFeet(double k) const {
    if (muRoll <= 0) return INFINITY;
    return (1.0 + k) * stimpIntegral(*this) / (2.0 * kGravity * muRoll) / kFoot;
}

void Surface::setStimp(double feet, double k) {
    if (!(feet > 0)) return;
    muRoll = (1.0 + k) * stimpIntegral(*this) / (2.0 * kGravity * feet * kFoot);
}

int SurfaceTable::add(const Surface& s) {
    const int i = find(s.id);
    if (i >= 0) {
        list_[static_cast<size_t>(i)] = s;
        return i;
    }
    list_.push_back(s);
    return static_cast<int>(list_.size()) - 1;
}

int SurfaceTable::find(const std::string& id) const {
    for (size_t i = 0; i < list_.size(); ++i)
        if (list_[i].id == id) return static_cast<int>(i);
    return -1;
}

SurfaceTable::SurfaceTable() {
    // Index 0 (fairway) is the fallback for unknown ids; the rest come from
    // data/surfaces/surfaces.json, compiled in.
    Surface fairway;
    add(fairway);
    const auto r = json::parse(embedded::surfaces());
    if (r.ok()) load(r.value);
}

std::string SurfaceTable::load(const json::Value& root) {
    const json::Value* arr = root.isObject() ? root.find("surfaces") : nullptr;
    if (!arr || !arr->isArray()) return "surfaces: expected an array";
    for (const auto& v : arr->items()) {
        if (!v.isObject()) return "surfaces: expected objects";
        const json::Value* idv = v.find("id");
        if (!idv || !idv->isString()) return "surfaces: every surface needs an id";
        const int existing = find(idv->asString());
        Surface s = existing >= 0 ? list_[static_cast<size_t>(existing)] : Surface{};
        s.id = idv->asString();
        double stimp = 0;
        for (const auto& [k, f] : v.members()) {
            const double n = f.isNumber() ? f.asNumber() : 0.0;
            if (k == "name" && f.isString()) s.name = f.asString();
            else if (k == "restitution") {
                if (f.isString() && f.asString() == "penner2002") s.penner = true;
                else if (f.isNumber()) { s.penner = false; s.restitution = n; }
            }
            else if (k == "restitution_scale") s.restitutionScale = n;
            else if (k == "mu_slide") s.muSlide = n;
            else if (k == "compliance") s.compliance = n;
            else if (k == "pivot_m") s.pivot = n;
            else if (k == "mu_roll") s.muRoll = n;
            else if (k == "stimp_ft") stimp = n;
            else if (k == "roll_law" && f.isString()) s.rollLaw = f.asString() == "constant" ? RollLaw::Constant : RollLaw::Quadratic;
            else if (k == "roll_v_ref") s.rollVRef = n;
            else if (k == "water") s.water = f.isBool() && f.asBool();
            else if (k == "sand") s.sand = f.isBool() && f.asBool();
            else if (k == "lie_speed") s.lieSpeed = n;
            else if (k == "lie_friction") s.lieFriction = n;
            else if (k == "lie_dispersion") s.lieDispersion = n;
            else if (k == "puttable") s.puttable = f.isBool() && f.asBool();
            else if (k == "color" && f.isString()) s.color = f.asString();
            else if (k == "source" && f.isString()) s.source = f.asString();
        }
        if (stimp > 0) s.setStimp(stimp);
        add(s);
    }
    return "";
}

}  // namespace golf
