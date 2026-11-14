// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/cup.hpp"

#include <cmath>

namespace golf {

namespace {

// Restitution of the contacts at the hole. The rim's (cut turf over soil) is
// the one the capture speed depends on: 0.5 puts the largest speed holed
// through the centre at 1.60 m/s, against Penner's (2002) 1.63 m/s
// (tests/test_cup.cpp). The liner (plastic) and flagstick (fibreglass) are
// estimates.
constexpr double kRimE = 0.50;
constexpr double kWallE = 0.25;
constexpr double kBottomE = 0.15;
constexpr double kPinE = 0.40;
constexpr double kMu = 0.40;
constexpr double kDt = 1e-4;

struct Frame {
    Vec3 c, n;  // centre, green normal
    double py(const Vec3& p) const { return (p - c).dot(n); }
    Vec3 tang(const Vec3& p) const {
        const Vec3 d = p - c;
        return d - n * d.dot(n);
    }
};

// Impulsive contact with outward normal m (from the contact toward the ball
// centre): restitution e on the normal velocity, Coulomb friction that drives
// the contact point's slip toward rolling (k / (1 + k) of the slip removes it).
void collide(BallState& s, const Vec3& m, double depth, double e, double r, double k, double mu, bool resting) {
    s.pos += m * depth;
    const double vn = s.vel.dot(m);
    if (vn >= 0) return;
    const double eff = resting && vn > -0.25 ? 0.0 : e;
    const double jn = -(1 + eff) * vn;
    const Vec3 rc = m * (-r);
    const Vec3 vt = s.vel - m * vn;
    const Vec3 slip = vt + s.spin.cross(rc);
    const double sl = slip.length();
    Vec3 jt{};
    if (sl > 1e-12) {
        const double mag = std::min(sl * k / (1 + k), mu * jn);
        jt = slip * (-mag / sl);
    }
    s.vel += m * jn + jt;
    s.spin += rc.cross(jt) / (k * r * r);
}

}  // namespace

bool nearCup(const Cup& cup, const Vec3& n, const Vec3& pos, double r) {
    const Frame f{cup.pos, n};
    const double rho = f.tang(pos).length();
    return rho < cup.radius + r && f.py(pos) < r + 0.05;
}

CupResult cupSimulate(const BallState& s0, const Cup& cup, const Vec3& n, const Surface& green, const BallModel& ball,
                      double tEnd, std::vector<PathPoint>* path, std::vector<ShotEvent>& events, double sampleInterval) {
    const double r = ball.radius(), k = ball.inertiaFactor;
    const Frame f{cup.pos, n};
    const Vec3 g{0, -kGravity, 0};
    BallState s = s0;
    double nextSample = s.t;
    bool holed = false, rimEvent = false, pinEvent = false;
    double holedAt = 0;
    CupResult res;
    while (s.t < tEnd) {
        // Gravity, and rolling resistance while the ball sits on the green.
        const Vec3 pt = f.tang(s.pos);
        const double rho = pt.length();
        const double py = f.py(s.pos);
        const bool onPlane = rho >= cup.radius && py <= r + 1e-4;
        Vec3 a = g;
        if (onPlane) {
            const Vec3 vt = s.vel - n * s.vel.dot(n);
            const double sp = vt.length();
            if (sp > 1e-6) a += vt * (-green.rollingFriction(sp) * kGravity * n.y / (1 + k) / sp);
        }
        s.vel += a * kDt;
        s.pos += s.vel * kDt;
        s.t += kDt;

        // Contacts.
        const Vec3 p2 = f.tang(s.pos);
        const double rho2 = p2.length();
        const double py2 = f.py(s.pos);
        const Vec3 rhat = rho2 > 1e-9 ? p2 / rho2 : Vec3{};
        if (rho2 >= cup.radius) {
            if (py2 < r) collide(s, n, r - py2, green.restitutionAt(-s.vel.dot(n)), r, k, kMu, true);
        } else {
            // Rim: the circle of radius R in the green plane.
            if (rho2 > 1e-9) {
                const Vec3 q = cup.pos + rhat * cup.radius;
                const Vec3 d = s.pos - q;
                const double dl = d.length();
                if (dl < r && dl > 1e-9 && py2 > -r) {
                    collide(s, d / dl, r - dl, kRimE, r, k, kMu, false);
                    if (!rimEvent && !holed) {
                        rimEvent = true;
                        events.push_back({EventKind::Rim, s.t, s.pos, s.vel.length(), 0});
                    }
                }
            }
            // Wall and bottom.
            if (py2 < 0 && rho2 > cup.radius - r) collide(s, rhat * -1.0, rho2 - (cup.radius - r), kWallE, r, k, kMu, false);
            if (py2 < -cup.depth + r) collide(s, n, -cup.depth + r - py2, kBottomE, r, k, kMu, true);
        }
        // Flagstick: vertical cylinder on the hole's axis.
        if (cup.flagstick) {
            Vec3 h{s.pos.x - cup.pos.x, 0, s.pos.z - cup.pos.z};
            const double sig = h.length();
            if (sig < cup.pinRadius + r && s.pos.y < cup.pos.y + cup.pinHeight && sig > 1e-9) {
                collide(s, h / sig, cup.pinRadius + r - sig, kPinE, r, k, 0.2, false);
                if (!pinEvent) {
                    pinEvent = true;
                    events.push_back({EventKind::Flagstick, s.t, s.pos, s.vel.length(), 0});
                }
            }
        }

        if (path && s.t >= nextSample) {
            path->push_back({s.t, s.pos, Phase::Cup, s.spin});
            nextSample = s.t + sampleInterval;
        }

        const Vec3 p3 = f.tang(s.pos);
        const double rho3 = p3.length(), py3 = f.py(s.pos);
        if (!holed && rho3 < cup.radius && py3 < -r) {
            holed = true;
            holedAt = s.t;
            events.push_back({EventKind::Holed, s.t, s.pos, s.vel.length(), 0});
        }
        if (holed) {
            // Let it settle on the bottom for the drawing, then stop.
            if (s.t - holedAt > 0.6 || (py3 < -cup.depth + r + 1e-3 && s.vel.length() < 0.02)) {
                res.end = CupResult::End::Holed;
                res.state = s;
                return res;
            }
            continue;
        }
        if (rho3 > cup.radius + r + 0.005 && py3 <= r + 1e-3) {
            res.end = CupResult::End::Left;
            res.state = s;
            return res;
        }
        if (py3 > r + 0.02 && s.vel.dot(n) > 0 && rho3 > cup.radius) {
            res.end = CupResult::End::Airborne;
            res.state = s;
            return res;
        }
        // At rest on the lip, overhanging: the Rules give it ten seconds
        // (Rule 13.3a); physically it either drops or stays. It stays.
        if (s.vel.length() < 1e-3 && py3 > -r) {
            res.end = CupResult::End::Left;
            res.state = s;
            return res;
        }
    }
    res.end = CupResult::End::Timeout;
    res.state = s;
    return res;
}

}  // namespace golf
