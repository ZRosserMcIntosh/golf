// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/rules.hpp"

#include <cmath>

namespace golf {

const char* rulingName(Ruling::Kind k) {
    switch (k) {
        case Ruling::Kind::Play: return "play";
        case Ruling::Kind::Holed: return "holed";
        case Ruling::Kind::PenaltyArea: return "penalty area";
        case Ruling::Kind::OutOfBounds: return "out of bounds";
        case Ruling::Kind::Lost: return "lost";
    }
    return "?";
}

namespace {

double horiz(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.z - b.z); }

bool playable(const Course& c, double x, double z) {
    if (!c.inBounds(x, z)) return false;
    const GroundSample g = c.terrain.sample(x, z);
    const Surface& s = c.surfaces[g.surface];
    // Not in the water, not in a bunker (a relief area outside the bunker), not too steep to hold a ball.
    return !s.water && !s.sand && g.normal.y > 0.94 && s.id != "green";
}

Vec3 onGround(const Course& c, double x, double z, double r) { return {x, c.terrain.height(x, z) + r, z}; }

}  // namespace

Ruling rule(const Course& c, const HoleDef&, const Vec3& pin, const Vec3& from, int fromSurface, const ShotResult& r,
            const RulesOptions& o) {
    Ruling ru;
    const double br = r.final.pos.y - c.terrain.height(r.final.pos.x, r.final.pos.z);  // ball radius above ground
    const double radius = br > 0.01 && br < 0.05 ? br : 0.02134;
    ru.next = r.final.pos;
    ru.nextSurface = r.finalSurface;
    if (r.outcome == Outcome::Holed) {
        ru.kind = Ruling::Kind::Holed;
        ru.note = "holed";
        return ru;
    }
    const bool lost = r.outcome == Outcome::Lost;
    const bool ob = !lost && !c.inBounds(r.final.pos.x, r.final.pos.z);
    if (lost || ob) {
        ru.kind = lost ? Ruling::Kind::Lost : Ruling::Kind::OutOfBounds;
        if (!o.localRuleE5) {
            ru.penalty = 1;
            ru.replay = true;
            ru.next = from;
            ru.nextSurface = fromSurface;
            ru.note = lost ? "lost: stroke and distance" : "out of bounds: stroke and distance";
            return ru;
        }
        // E-5: find where the ball crossed the boundary, drop on the course side
        // (two club-lengths in from it), two penalty strokes.
        Vec3 cross = r.final.pos;
        for (size_t i = 1; i < r.path.size(); ++i)
            if (c.inBounds(r.path[i - 1].pos.x, r.path[i - 1].pos.z) && !c.inBounds(r.path[i].pos.x, r.path[i].pos.z)) {
                cross = r.path[i - 1].pos;
                break;
            }
        Vec3 toPin = Vec3{pin.x - cross.x, 0, pin.z - cross.z}.normalized();
        Vec3 drop = cross;
        for (double s = 2 * o.clubLength; s < 80; s += 1.0) {
            const Vec3 q = cross + toPin * s;
            if (playable(c, q.x, q.z)) {
                drop = q;
                break;
            }
        }
        ru.penalty = 2;
        ru.next = onGround(c, drop.x, drop.z, radius);
        ru.nextSurface = c.terrain.sample(drop.x, drop.z).surface;
        ru.note = "out of bounds: Local Rule E-5, two strokes";
        return ru;
    }
    const bool inWater = r.outcome == Outcome::Water || c.surfaces[r.finalSurface].water;
    if (inWater) {
        ru.kind = Ruling::Kind::PenaltyArea;
        ru.penalty = 1;
        // Where the ball last crossed into the penalty area.
        Vec3 entry = r.final.pos;
        for (size_t i = 1; i < r.path.size(); ++i) {
            const auto& p = r.path[i].pos;
            if (c.surfaces[c.terrain.sample(p.x, p.z).surface].water && p.y - c.terrain.height(p.x, p.z) < 0.5) {
                entry = r.path[i - 1].pos;
                break;
            }
        }
        const HoleDef* lh = nullptr;
        const Lake* lake = c.lakeAt(r.final.pos.x, r.final.pos.z, &lh);
        const bool red = !lake || lake->red;
        const double dEntry = horiz(entry, pin);
        Vec3 drop = entry;
        bool found = false;
        if (red) {
            // Lateral relief: nearest playable spot within two club-lengths, not nearer the hole.
            double bestD = 1e9;
            for (double rad = 0.3; rad <= 2 * o.clubLength + 1e-9; rad += 0.3)
                for (int k = 0; k < 24; ++k) {
                    const double a = k * kPi / 12;
                    const Vec3 q{entry.x + rad * std::cos(a), 0, entry.z + rad * std::sin(a)};
                    if (horiz(q, pin) < dEntry - 1e-6 || !playable(c, q.x, q.z)) continue;
                    if (rad < bestD) {
                        bestD = rad;
                        drop = q;
                        found = true;
                    }
                }
        }
        if (!found) {
            // Back on the line: keep the entry point between the hole and the drop.
            const Vec3 back = Vec3{entry.x - pin.x, 0, entry.z - pin.z}.normalized();
            for (double s = 1.0; s < 250; s += 1.0) {
                const Vec3 q = entry + back * s;
                if (playable(c, q.x, q.z)) {
                    drop = q;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            ru.replay = true;
            ru.next = from;
            ru.nextSurface = fromSurface;
            ru.note = "penalty area: stroke and distance";
            return ru;
        }
        ru.next = onGround(c, drop.x, drop.z, radius);
        ru.nextSurface = c.terrain.sample(drop.x, drop.z).surface;
        ru.note = red ? "penalty area: lateral relief, one stroke" : "penalty area: back on the line, one stroke";
        return ru;
    }
    ru.plugged = r.outcome == Outcome::Plugged;
    ru.note = ru.plugged ? "plugged in the bunker: play it as it lies" : "";
    return ru;
}

}  // namespace golf
