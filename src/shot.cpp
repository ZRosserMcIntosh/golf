// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// The bounce (penner_impact) and the slide/roll step follow dimple's ground
// model (github.com/divotmaker/dimple, src/ground.rs, Eric Thill,
// MIT/Apache-2.0; see NOTICE), which implements Penner (2002) in 3-D.
#include "golf/shot.hpp"

#include <algorithm>
#include <cmath>

#include "golf/cup.hpp"

namespace golf {

const char* eventName(EventKind k) {
    switch (k) {
        case EventKind::Launch: return "launch";
        case EventKind::Apex: return "apex";
        case EventKind::Landing: return "landing";
        case EventKind::Bounce: return "bounce";
        case EventKind::Tree: return "tree";
        case EventKind::Trunk: return "trunk";
        case EventKind::Flagstick: return "flagstick";
        case EventKind::Rim: return "rim";
        case EventKind::Holed: return "holed";
        case EventKind::Water: return "water";
        case EventKind::Plugged: return "plugged";
        case EventKind::Rest: return "rest";
        case EventKind::Lost: return "lost";
    }
    return "?";
}

const char* outcomeName(Outcome o) {
    switch (o) {
        case Outcome::Rest: return "rest";
        case Outcome::Holed: return "holed";
        case Outcome::Water: return "water";
        case Outcome::Lost: return "lost";
        case Outcome::Plugged: return "plugged";
    }
    return "?";
}

BallState launchState(const Launch& l) {
    const Vec3 target = heading(l.targetHeading);
    const Vec3 dir = rotate(target, kUp, -l.hlaDeg * kDeg);  // + HLA is to the right (toward -up x target)
    // Right of a horizontal direction d is d x up.
    const double vla = l.vlaDeg * kDeg;
    BallState s;
    s.pos = l.position;
    s.vel = (dir * std::cos(vla) + kUp * std::sin(vla)) * l.speed;
    // Backspin spins about the axis to the right of the line of flight;
    // tilting the axis right (positive) turns the lift to the right.
    const Vec3 right = dir.cross(kUp);
    const double a = l.spinAxisDeg * kDeg;
    const Vec3 axis = right * std::cos(a) - kUp * std::sin(a);
    s.spin = axis * (l.spinRpm * kRpm);
    return s;
}

namespace {

constexpr double kMinHop = 0.003;  // m: rebounds lower than this become a slide

struct Sim {
    const BallModel& ball;
    const Air air;
    WindField wind;
    const World& world;
    const SimOptions& opt;
    Rng rng;
    ShotResult& res;
    double r;
    double nextSample = 0;
    bool landed = false;
    Vec3 origin;
    Vec3 target;
    Vec3 cupNormal = kUp;

    Sim(const BallModel& b, const Conditions& c, const WindSpec& w, const World& wo, const SimOptions& o, ShotResult& out)
        : ball(b), air(atmosphere::air(c)), wind(w, Rng::mix(o.seed, 0x77696e64)), world(wo), opt(o),
          rng(Rng::mix(o.seed, 0x74726565)), res(out), r(b.radius()) {
        if (world.cup) cupNormal = ground(world.cup->pos).normal;
    }

    GroundSample ground(const Vec3& p) const {
        if (!world.terrain) return {0, kUp, 0};
        return world.terrain->sample(p.x, p.z);
    }
    double groundHeight(const Vec3& p) const { return world.terrain ? world.terrain->height(p.x, p.z) : 0.0; }
    const Surface& surf(int i) const {
        static const SurfaceTable fallback;
        return world.surfaces ? (*world.surfaces)[i] : fallback[i];
    }

    void sample(const BallState& s, Phase ph, bool force = false) {
        if (!opt.recordPath) return;
        if (force || s.t >= nextSample) {
            res.path.push_back({s.t, s.pos, ph, s.spin});
            nextSample = s.t + opt.sampleInterval;
        }
    }
    void event(EventKind k, const BallState& s, int surface = 0) {
        res.events.push_back({k, s.t, s.pos, s.vel.length(), surface});
    }

    // --- Flight ---------------------------------------------------------
    struct Deriv {
        Vec3 dpos, dvel, dspin;
    };
    Deriv deriv(const Vec3& pos, const Vec3& vel, const Vec3& spin) const {
        const double hag = pos.y - r - groundHeight(pos);
        const Vec3 rel = vel - wind.at(hag);
        const double v = rel.length();
        Deriv d{vel, {0, -kGravity, 0}, {}};
        if (v < 1e-9) return d;
        const Vec3 vhat = rel / v;
        const Vec3 wPerp = spin - vhat * spin.dot(vhat);
        const double w = wPerp.length();
        const BallModel::Coeffs c = ball.coefficients(v, w, air.density, air.viscosity);
        const double q = 0.5 * air.density * v * v * ball.area() / ball.massKg;
        d.dvel += vhat * (-c.cd * q);
        if (w > 1e-9 && c.cl != 0) {
            const Vec3 lift = wPerp.cross(vhat);
            const double l = lift.length();
            if (l > 1e-12) d.dvel += lift * (c.cl * q / l);
        }
        d.dspin = spin * ball.spinDecayRate(v);
        return d;
    }

    BallState rk4(const BallState& s, double h) const {
        const Deriv k1 = deriv(s.pos, s.vel, s.spin);
        const Deriv k2 = deriv(s.pos + k1.dpos * (h / 2), s.vel + k1.dvel * (h / 2), s.spin + k1.dspin * (h / 2));
        const Deriv k3 = deriv(s.pos + k2.dpos * (h / 2), s.vel + k2.dvel * (h / 2), s.spin + k2.dspin * (h / 2));
        const Deriv k4 = deriv(s.pos + k3.dpos * h, s.vel + k3.dvel * h, s.spin + k3.dspin * h);
        BallState o;
        o.t = s.t + h;
        o.pos = s.pos + (k1.dpos + k2.dpos * 2 + k3.dpos * 2 + k4.dpos) * (h / 6);
        o.vel = s.vel + (k1.dvel + k2.dvel * 2 + k3.dvel * 2 + k4.dvel) * (h / 6);
        o.spin = s.spin + (k1.dspin + k2.dspin * 2 + k3.dspin * 2 + k4.dspin) * (h / 6);
        return o;
    }

    static Vec3 hermitePos(const BallState& a, const BallState& b, double h, double tau) {
        const double u = tau / h, u2 = u * u, u3 = u2 * u;
        return a.pos * (2 * u3 - 3 * u2 + 1) + a.vel * ((u3 - 2 * u2 + u) * h) + b.pos * (-2 * u3 + 3 * u2) +
               b.vel * ((u3 - u2) * h);
    }
    static Vec3 hermiteVel(const BallState& a, const BallState& b, double h, double tau) {
        const double u = tau / h, u2 = u * u;
        return a.pos * ((6 * u2 - 6 * u) / h) + a.vel * (3 * u2 - 4 * u + 1) + b.pos * ((-6 * u2 + 6 * u) / h) +
               b.vel * (3 * u2 - 2 * u);
    }
    double clearance(const Vec3& p) const { return p.y - r - groundHeight(p); }

    // Trees met by the segment a -> b. Returns true if the state was changed.
    bool trees(BallState& b, const BallState& a) {
        if (!world.trees) return false;
        const Vec3 mid = (a.pos + b.pos) * 0.5;
        const double L = (b.pos - a.pos).length();
        bool hit = false;
        auto test = [&](const Tree& t) {
            if (hit) return;
            const double dx = mid.x - t.x, dz = mid.z - t.z;
            const double reach = std::max(t.canopyRx, t.trunkRadius) + 1.0;
            if (dx * dx + dz * dz > reach * reach) return;
            if (mid.y > t.baseY + t.canopyY + t.canopyRy + 1.0) return;
            // Trunk: vertical cylinder.
            const double trunkTop = t.baseY + t.trunkHeight;
            if (b.pos.y < trunkTop) {
                const double ex = b.pos.x - t.x, ez = b.pos.z - t.z;
                const double dist = std::sqrt(ex * ex + ez * ez);
                const double reachT = t.trunkRadius + r;
                if (dist < reachT && dist > 1e-9) {
                    const Vec3 m{ex / dist, 0, ez / dist};
                    const double vn = b.vel.dot(m);
                    if (vn < 0) {
                        // Bark: restitution about 0.45, friction keeps 80 % of the
                        // tangential speed (estimates).
                        const Vec3 vt = b.vel - m * vn;
                        b.vel = vt * 0.8 - m * (0.45 * vn);
                        b.spin = b.spin * 0.5;
                        b.pos += m * (reachT - dist);
                        event(EventKind::Trunk, b);
                        hit = true;
                        return;
                    }
                }
            }
            // Canopy: an ellipsoid. Strike probability per step from the path
            // length inside, denser toward the centre (a Beer-Lambert law).
            const double cy = t.baseY + t.canopyY;
            const double qx = dx / t.canopyRx, qy = (mid.y - cy) / t.canopyRy, qz = dz / t.canopyRx;
            const double q2 = qx * qx + qy * qy + qz * qz;
            if (q2 < 1.0) {
                const double p = 1.0 - std::exp(-t.density * (1.0 - q2) * L);
                if (rng.uniform() < p) {
                    // A branch: the ball loses most of its speed and leaves in
                    // a scattered direction (estimates, for play).
                    const double keep = rng.uniform(0.15, 0.55);
                    Vec3 rnd{rng.normal(), rng.normal(), rng.normal()};
                    rnd = rnd.normalized();
                    const double sp = b.vel.length();
                    Vec3 dir = (b.vel / std::max(sp, 1e-9)) * 0.6 + rnd;
                    dir = dir.normalized();
                    b.vel = dir * (sp * keep);
                    b.spin = b.spin * 0.3;
                    event(EventKind::Tree, b);
                    hit = true;
                }
            }
        };
        if (world.treeIndex) {
            // Trees are indexed by canopy footprint; a cell's list may repeat a tree in
            // neighbouring cells, but the query visits only the ball's own cell.
            world.treeIndex->query(mid.x, mid.z, 0.0, [&](int i) { test((*world.trees)[static_cast<size_t>(i)]); });
        } else {
            for (const Tree& t : *world.trees) test(t);
        }
        return hit;
    }

    // The flagstick in flight: a vertical cylinder above the hole.
    bool flagstick(BallState& b) {
        const Cup* c = world.cup;
        if (!c || !c->flagstick) return false;
        if (b.pos.y > c->pos.y + c->pinHeight || b.pos.y < c->pos.y) return false;
        const double ex = b.pos.x - c->pos.x, ez = b.pos.z - c->pos.z;
        const double dist = std::sqrt(ex * ex + ez * ez);
        const double reach = c->pinRadius + r;
        if (dist >= reach || dist < 1e-9) return false;
        const Vec3 m{ex / dist, 0, ez / dist};
        const double vn = b.vel.dot(m);
        if (vn >= 0) return false;
        const Vec3 vt = b.vel - m * vn;
        b.vel = vt * 0.9 - m * (0.40 * vn);  // fibreglass, estimated
        b.pos += m * (reach - dist);
        event(EventKind::Flagstick, b);
        return true;
    }

    enum class FlightEnd { Ground, Cup, Lost, Timeout };

    // Fly from s until the ball meets the ground (s is then the contact state).
    FlightEnd fly(BallState& s, Phase ph, double tEnd) {
        const double h = opt.flightDt;
        double apexY = s.pos.y;
        bool rising = s.vel.y > 0;
        while (s.t < tEnd) {
            BallState b = rk4(s, h);
            wind.step(h);
            if (rising && b.vel.y <= 0) {
                rising = false;
                if (ph == Phase::Flight) event(EventKind::Apex, b);
            }
            apexY = std::max(apexY, b.pos.y);
            if (b.pos.x < world.minX || b.pos.x > world.maxX || b.pos.z < world.minZ || b.pos.z > world.maxZ) {
                s = b;
                return FlightEnd::Lost;
            }
            trees(b, s);
            flagstick(b);
            if (world.cup && nearCup(*world.cup, cupNormal, b.pos, r)) {
                const Vec3 n = cupNormal;
                const Vec3 d = b.pos - world.cup->pos;
                const double rho = (d - n * d.dot(n)).length();
                if (rho < world.cup->radius || b.vel.length() < 4.0) {
                    s = b;
                    return FlightEnd::Cup;
                }
            }
            const double cb = clearance(b.pos);
            if (cb <= 0) {
                // Bisect the Hermite arc for the contact time.
                double lo = 0, hi = h;
                for (int i = 0; i < 40; ++i) {
                    const double m = 0.5 * (lo + hi);
                    if (clearance(hermitePos(s, b, h, m)) > 0) lo = m;
                    else hi = m;
                }
                const double tau = hi;
                BallState c;
                c.t = s.t + tau;
                c.pos = hermitePos(s, b, h, tau);
                c.vel = hermiteVel(s, b, h, tau);
                c.spin = s.spin + (b.spin - s.spin) * (tau / h);
                c.pos.y = groundHeight(c.pos) + r;
                s = c;
                sample(s, ph, true);
                return FlightEnd::Ground;
            }
            s = b;
            sample(s, ph);
            if (ph == Phase::Flight) res.stats.apex = std::max(res.stats.apex, s.pos.y - origin.y);
        }
        return FlightEnd::Timeout;
    }

    // --- Bounce (Penner 2002 in 3-D, after dimple) ----------------------
    double contactAngle(const Surface& sf, double vt, double vn) const {
        if (vt <= 0.01) return 0;
        const double speed = std::hypot(vt, vn);
        const double incidence = std::atan2(vt, vn);
        // Penner (2002) eq. 8: theta_c = 15.4 deg (v / 18.6 m/s)(phi / 44.4 deg),
        // his fit to Haake's green impact; faded in below 2 m/s normal speed
        // (dimple), capped at the incidence angle.
        const double thetaC = 15.4 * kDeg * (speed / 18.6) * (incidence / (44.4 * kDeg));
        const double fade = std::min(1.0, vn / 2.0);
        return std::min(sf.compliance * thetaC * fade, incidence);
    }

    BallState impact(const BallState& s, const GroundSample& g) const {
        const Surface& sf = surf(g.surface);
        const Vec3 n = g.normal;
        const Vec3 v = s.vel;
        const double vdn = v.dot(n);
        if (vdn >= -1e-12 * v.length()) return s;
        const double k = ball.inertiaFactor;
        const Vec3 vtv = v - n * vdn;
        const double vt = vtv.length();
        const double beta = contactAngle(sf, vt, -vdn);
        const Vec3 that = vt > 0 ? vtv / vt : Vec3{};
        const Vec3 neff = n * std::cos(beta) - that * std::sin(beta);
        const double vne = v.dot(neff);
        if (vne >= 0) return s;
        const double e = sf.restitutionAt(vne);
        const double jn = (1 + e) * -vne;
        const Vec3 rc = neff * -r;
        const Vec3 slip = (v - neff * vne) + s.spin.cross(rc);
        const double sl = slip.length();
        const double jt = std::min(sl * k / (1 + k), sf.muSlide * jn);
        const Vec3 jtv = sl > 0 ? slip * (-jt / sl) : Vec3{};
        BallState o = s;
        o.vel = v + neff * jn + jtv;
        o.spin = s.spin + rc.cross(jtv) / (k * r * r);
        // Pivot friction removes spin about the normal.
        const double wn = o.spin.dot(n);
        const double dw = std::min(std::abs(wn), sf.pivot * jn / (k * r * r));
        o.spin -= n * (wn > 0 ? dw : -dw);
        return o;
    }

    // --- Slide and roll --------------------------------------------------
    static Vec3 slipOf(const Vec3& vel, const Vec3& spin, const Vec3& n, double r) {
        return (vel - n * vel.dot(n)) + spin.cross(n * -r);
    }
    void lockRolling(BallState& s, const Vec3& n) const {
        const double k = ball.inertiaFactor;
        const Vec3 u = slipOf(s.vel, s.spin, n, r);
        s.vel = s.vel - u * (k / (1 + k));
        s.spin = s.spin + n.cross(u) / ((1 + k) * r);
    }

    enum class GroundEnd { Rest, Cup, Water, Lost, Timeout, Airborne };

    GroundEnd roll(BallState& s, double tEnd) {
        const double h = opt.groundDt;
        const double k = ball.inertiaFactor;
        GroundSample g = ground(s.pos);
        bool rolling = false;
        Phase last = Phase::Slide;
        while (s.t < tEnd) {
            const Surface& sf = surf(g.surface);
            if (sf.water) return GroundEnd::Water;
            const Vec3 n = g.normal;
            const double gn = kGravity * n.y;
            const Vec3 gt = tangential({0, -kGravity, 0}, n);
            Vec3 vel = s.vel - n * s.vel.dot(n);
            s.vel = vel;
            const Vec3 u = slipOf(vel, s.spin, n, r);
            const double ul = u.length();
            const double wn = s.spin.dot(n);
            Phase ph;
            if (!rolling && ul > 1e-4) {
                ph = Phase::Slide;
                const Vec3 uh = u / ul;
                const double fr = sf.muSlide * gn;
                s.vel = vel + (gt - uh * fr) * h;
                s.spin = s.spin + n.cross(uh) * (fr / (k * r) * h);
                const Vec3 u2 = slipOf(s.vel, s.spin, n, r);
                if (u2.dot(u) <= 0 || u2.length() < 1e-4) {
                    lockRolling(s, n);
                    rolling = true;
                }
            } else {
                ph = Phase::Roll;
                if (!rolling) {
                    lockRolling(s, n);
                    rolling = true;
                }
                const double sp = vel.length();
                const Vec3 slope = gt / (1 + k);
                const bool holds = gt.length() <= sf.muRoll * gn;
                if (sp < 1e-6) {
                    if (holds) {
                        s.vel = {};
                        return GroundEnd::Rest;
                    }
                    s.vel = slope * h;
                } else {
                    const Vec3 vh = vel / sp;
                    const double dec = sf.rollingFriction(sp) * gn / (1 + k);
                    Vec3 nv = vel + (slope - vh * dec) * h;
                    if (nv.dot(vh) <= 0 && holds) {
                        // Stops within this step.
                        s.vel = {};
                        s.pos += vel * (0.5 * sp / std::max(dec - slope.dot(vh), 1e-9));
                        GroundSample g2 = ground(s.pos);
                        s.pos.y = g2.height + r;
                        s.spin = n * wn;
                        sample(s, Phase::Roll, true);
                        return GroundEnd::Rest;
                    }
                    s.vel = nv;
                }
                s.spin = n.cross(s.vel) / r + n * wn;
            }
            // Pivot friction on spin about the normal.
            if (wn != 0) {
                const double dw = sf.pivot * gn / (k * r * r) * h;
                const double nw = std::abs(wn) <= dw ? 0.0 : wn - (wn > 0 ? dw : -dw);
                s.spin += n * (nw - wn);
            }
            s.pos += s.vel * h;
            s.t += h;
            if (s.pos.x < world.minX || s.pos.x > world.maxX || s.pos.z < world.minZ || s.pos.z > world.maxZ)
                return GroundEnd::Lost;
            const GroundSample g2 = ground(s.pos);
            // Back onto the terrain: height, and velocity in the new tangent plane.
            s.pos.y = g2.height + r;
            if (!(g2.normal == n)) {
                s.vel = s.vel - g2.normal * s.vel.dot(g2.normal);
                if (rolling) s.spin = g2.normal.cross(s.vel) / r + g2.normal * s.spin.dot(n);
            }
            if (ph != last) sample(s, ph, true);
            else sample(s, ph);
            last = ph;
            g = g2;
            if (world.cup && nearCup(*world.cup, cupNormal, s.pos, r)) return GroundEnd::Cup;
            // A very slow ball that the slope cannot move: at rest.
            if (rolling && s.vel.length() < 0.002 && gt.length() <= sf.muRoll * gn) {
                s.vel = {};
                return GroundEnd::Rest;
            }
        }
        return GroundEnd::Timeout;
    }

    // --- The whole shot ---------------------------------------------------
    void run(const Launch& l) {
        BallState s = launchState(l);
        origin = s.pos;
        target = heading(l.targetHeading);
        nextSample = 0;
        event(EventKind::Launch, s);
        sample(s, Phase::Flight, true);
        const double tEnd = opt.maxTime;
        Phase ph = Phase::Flight;
        int waterSkips = 0;
        for (int guard = 0; guard < 400; ++guard) {
            const FlightEnd fe = fly(s, ph, tEnd);
            if (fe == FlightEnd::Lost) return finish(s, Outcome::Lost, EventKind::Lost);
            if (fe == FlightEnd::Timeout) return finish(s, Outcome::Rest, EventKind::Rest);
            if (fe == FlightEnd::Cup) {
                if (cupPhase(s, tEnd)) return;
                ph = Phase::Hop;
                continue;
            }
            // Contact with the ground.
            GroundSample g = ground(s.pos);
            const Surface& sf = surf(g.surface);
            if (!landed) firstLanding(s, l);
            const double vdn = s.vel.dot(g.normal);
            const double speed = s.vel.length();
            const double impactAngle = std::asin(clamp(-vdn / std::max(speed, 1e-9), -1.0, 1.0));
            if (sf.water) {
                // Skipping: a ball at a grazing angle and high speed skips off
                // the water, as stones do below about 20 deg (Rosellini, Hersen,
                // Clanet & Bocquet 2005, Europhys. Lett. 71, 1005); golf balls,
                // being light and smooth, need lower angles. Estimated: below
                // 12 deg and above 15 m/s, up to three skips.
                if (impactAngle < 12 * kDeg && speed > 15.0 && waterSkips < 3) {
                    ++waterSkips;
                    s.vel = (s.vel - g.normal * vdn) * 0.65 - g.normal * (0.35 * vdn);
                    s.spin = s.spin * 0.5;
                    s.pos.y += 1e-4;
                    event(EventKind::Bounce, s, g.surface);
                    ph = Phase::Hop;
                    continue;
                }
                return finish(s, Outcome::Water, EventKind::Water, g.surface);
            }
            if (sf.sand && -vdn > 12.0 && impactAngle > 45 * kDeg) {
                // A steep, fast landing in soft sand plugs (a "fried egg").
                s.vel = {};
                s.spin = {};
                event(EventKind::Plugged, s, g.surface);
                return finish(s, Outcome::Plugged, EventKind::Rest, g.surface);
            }
            const BallState out = impact(s, g);
            const double vn = out.vel.dot(g.normal);
            ++res.stats.bounces;
            event(res.stats.bounces == 1 ? EventKind::Landing : EventKind::Bounce, s, g.surface);
            if (vn > 0 && vn * vn >= 2 * kGravity * g.normal.y * kMinHop) {
                s = out;
                s.pos += g.normal * 1e-5;
                ph = Phase::Hop;
                continue;
            }
            s = out;
            const GroundEnd ge = roll(s, tEnd);
            switch (ge) {
                case GroundEnd::Rest:
                    return finish(s, Outcome::Rest, EventKind::Rest, ground(s.pos).surface);
                case GroundEnd::Water:
                    return finish(s, Outcome::Water, EventKind::Water, ground(s.pos).surface);
                case GroundEnd::Lost:
                    return finish(s, Outcome::Lost, EventKind::Lost);
                case GroundEnd::Timeout:
                    return finish(s, Outcome::Rest, EventKind::Rest, ground(s.pos).surface);
                case GroundEnd::Airborne:
                    ph = Phase::Hop;
                    continue;
                case GroundEnd::Cup:
                    if (cupPhase(s, tEnd)) return;
                    ph = Phase::Hop;
                    continue;
            }
        }
        finish(s, Outcome::Rest, EventKind::Rest, ground(s.pos).surface);
    }

    // Returns true if the shot ended (holed or rested); otherwise s is airborne or rolling away.
    bool cupPhase(BallState& s, double tEnd) {
        const Cup& cup = *world.cup;
        const GroundSample gc = ground(cup.pos);
        const CupResult cr = cupSimulate(s, cup, gc.normal, surf(gc.surface), ball, tEnd,
                                         opt.recordPath ? &res.path : nullptr, res.events, opt.sampleInterval);
        s = cr.state;
        if (!landed) firstLanding(s, Launch{});
        if (cr.end == CupResult::End::Holed) {
            finish(s, Outcome::Holed, EventKind::Rest, gc.surface);
            return true;
        }
        if (cr.end == CupResult::End::Timeout) {
            finish(s, Outcome::Rest, EventKind::Rest, gc.surface);
            return true;
        }
        if (cr.end == CupResult::End::Airborne) return false;
        // Rolling away from the hole: continue on the green, past the hole.
        const GroundEnd ge = roll(s, tEnd);
        if (ge == GroundEnd::Cup) {
            // Came back toward the hole (a slope): simulate it again.
            return cupPhase(s, tEnd);
        }
        if (ge == GroundEnd::Water) finish(s, Outcome::Water, EventKind::Water, ground(s.pos).surface);
        else if (ge == GroundEnd::Lost) finish(s, Outcome::Lost, EventKind::Lost);
        else finish(s, Outcome::Rest, EventKind::Rest, ground(s.pos).surface);
        return true;
    }

    void firstLanding(const BallState& s, const Launch&) {
        landed = true;
        ShotStats& st = res.stats;
        const Vec3 d = s.pos - origin;
        st.carry = d.horizontalLength();
        st.carryAlong = d.dot(target);
        st.carryOffline = d.dot(target.cross(kUp));
        st.flightTime = s.t;
        const double vh = std::hypot(s.vel.x, s.vel.z);
        st.landingAngleDeg = std::atan2(-s.vel.y, vh) / kDeg;
        st.landingSpeed = s.vel.length();
        st.landingSpinRpm = s.spin.length() / kRpm;
    }

    void finish(BallState s, Outcome o, EventKind k, int surface = 0) {
        res.outcome = o;
        res.final = s;
        res.finalSurface = surface;
        if (!landed) firstLanding(s, Launch{});
        const Vec3 d = s.pos - origin;
        res.stats.total = d.horizontalLength();
        res.stats.totalAlong = d.dot(target);
        res.stats.totalOffline = d.dot(target.cross(kUp));
        res.stats.totalTime = s.t;
        sample(s, o == Outcome::Holed ? Phase::Cup : Phase::Rest, true);
        event(k, s, surface);
    }
};

}  // namespace

ShotResult simulateShot(const Launch& launch, const BallModel& ball, const Conditions& cond, const WindSpec& wind,
                        const World& world, const SimOptions& opt) {
    ShotResult res;
    Sim sim(ball, cond, wind, world, opt, res);
    sim.run(launch);
    return res;
}

ShotResult simulateCarry(const Launch& launch, const BallModel& ball, const Conditions& cond, const WindSpec& wind,
                         const SimOptions& opt) {
    ShotResult res;
    // Level ground at the launch height (minus the ball radius: the ball sits on it).
    const FlatTerrain flat(0, launch.position.y - ball.radius());
    World w;
    w.terrain = &flat;
    Sim sim(ball, cond, wind, w, opt, res);
    BallState s = launchState(launch);
    sim.origin = s.pos;
    sim.target = heading(launch.targetHeading);
    sim.event(EventKind::Launch, s);
    sim.sample(s, Phase::Flight, true);
    sim.fly(s, Phase::Flight, opt.maxTime);
    sim.firstLanding(s, launch);
    res.final = s;
    res.stats.total = res.stats.carry;
    res.stats.totalAlong = res.stats.carryAlong;
    res.stats.totalOffline = res.stats.carryOffline;
    res.stats.totalTime = s.t;
    return res;
}

}  // namespace golf
