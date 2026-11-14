// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/golfer.hpp"

#include <algorithm>
#include <cmath>

#include "golf/embedded.hpp"

namespace golf {

std::vector<GolferProfile> loadGolfers(const json::Value& root) {
    std::vector<GolferProfile> out;
    const json::Value* arr = root.isObject() ? root.find("golfers") : nullptr;
    if (!arr || !arr->isArray()) return out;
    for (const auto& v : arr->items()) {
        GolferProfile g;
        for (const auto& [k, f] : v.members()) {
            const double n = f.isNumber() ? f.asNumber() : 0.0;
            if (k == "id" && f.isString()) g.id = f.asString();
            else if (k == "name" && f.isString()) g.name = f.asString();
            else if (k == "speed_factor") g.speedFactor = n;
            else if (k == "face_sd_deg") g.faceSdDeg = n;
            else if (k == "path_sd_deg") g.pathSdDeg = n;
            else if (k == "strike_sd_mm") g.strikeSdM = n / 1000;
            else if (k == "speed_sd") g.speedSd = n;
            else if (k == "loft_sd_deg") g.loftSdDeg = n;
            else if (k == "putt_speed_sd") g.puttSpeedSd = n;
            else if (k == "putt_aim_sd_deg") g.puttAimSdDeg = n;
            else if (k == "handicap") g.handicap = n;
        }
        out.push_back(g);
    }
    return out;
}

std::vector<GolferProfile> standardGolfers() {
    const auto r = json::parse(embedded::golfers());
    return r.ok() ? loadGolfers(r.value) : std::vector<GolferProfile>{GolferProfile{}};
}

GolferProfile findGolfer(const std::string& id) {
    static const std::vector<GolferProfile> all = standardGolfers();
    for (const auto& g : all)
        if (g.id == id) return g;
    return all.empty() ? GolferProfile{} : all[std::min<size_t>(1, all.size() - 1)];
}

Delivery makeDelivery(const SwingInput& in, const Club& club, const GolferProfile& g, const Lie& lie,
                      double difficulty, bool leftHanded, Rng& rng) {
    Delivery d;
    d.leftHanded = leftHanded;
    const Surface* s = lie.surface;
    const double lieSpeed = s ? s->lieSpeed : 1.0;
    double scatter = in.exact ? 0.0 : difficulty * (s ? s->lieDispersion : 1.0);
    if (lie.plugged) scatter *= 1.6;
    // Swinging harder than full costs control.
    const double effort = std::max(0.0, in.power - 1.0);
    scatter *= 1.0 + 6.0 * effort;
    // Always draw the same number of deviates, so a shot's random stream does
    // not depend on the input (multiplayer replays rely on it).
    const double nSpeed = rng.normal(), nFace = rng.normal(), nPath = rng.normal(), nLoft = rng.normal(),
                 nToe = rng.normal(), nUp = rng.normal();
    if (club.putter) {
        d.clubSpeed = kPutterFullSpeed * clamp(in.power, 0.0, 1.5) * (1.0 + g.puttSpeedSd * scatter * nSpeed);
        d.attackDeg = 2.0 + in.attackDeg;
        d.dynamicLoftDeg = club.loftDeg + 1.0 + in.loftDeg;
        d.faceAngleDeg = in.faceDeg + g.puttAimSdDeg * scatter * nFace;
        d.pathDeg = in.pathDeg + 0.5 * g.puttAimSdDeg * scatter * nPath;
        d.strikeToeM = in.strikeToeMm / 1000 + 0.4 * g.strikeSdM * scatter * nToe;
        d.strikeUpM = in.strikeUpMm / 1000;
        d.frictionScale = 1.0;
        return d;
    }
    const double full = club.tourSpeedMph * kMph * club.speedCalibration * g.speedFactor;
    d.clubSpeed = full * clamp(in.power, 0.05, 1.12) * lieSpeed * (1.0 + g.speedSd * scatter * nSpeed);
    d.attackDeg = club.tourAttackDeg + in.attackDeg;
    // A shorter swing is delivered with a little less shaft lean (more loft).
    d.dynamicLoftDeg = club.tourDynamicLoftDeg + in.loftDeg + 3.0 * (1.0 - std::min(in.power, 1.0)) +
                       g.loftSdDeg * scatter * nLoft;
    d.pathDeg = in.pathDeg + g.pathSdDeg * scatter * nPath;
    d.faceAngleDeg = in.faceDeg + g.faceSdDeg * scatter * nFace;
    d.strikeToeM = in.strikeToeMm / 1000 + g.strikeSdM * scatter * nToe;
    d.strikeUpM = in.strikeUpMm / 1000 + 0.7 * g.strikeSdM * scatter * nUp;
    d.frictionScale = s ? s->lieFriction : 1.0;
    if (lie.plugged) {
        d.clubSpeed *= 0.7;
        d.frictionScale *= 0.5;
    }
    return d;
}

// ---------------------------------------------------------------------------
// Strokes-gained baselines: PGA Tour averages after Broadie (2014), "Every
// Shot Counts", appendix tables (putting by feet, fairway and tee by yards),
// interpolated; other lies as offsets from the fairway (estimates close to
// the book's rough and sand tables).

namespace {
double interpTable(const double (*t)[2], int n, double x) {
    if (x <= t[0][0]) return t[0][1];
    for (int i = 1; i < n; ++i)
        if (x <= t[i][0]) return t[i - 1][1] + (t[i][1] - t[i - 1][1]) * (x - t[i - 1][0]) / (t[i][0] - t[i - 1][0]);
    return t[n - 1][1] + (x - t[n - 1][0]) * 0.0025;  // beyond the table: about a stroke per 400 yd
}
const double kPutting[][2] = {{0, 1.0}, {1, 1.001}, {2, 1.009}, {3, 1.04}, {4, 1.13}, {5, 1.23}, {6, 1.34}, {7, 1.42},
                              {8, 1.50}, {10, 1.61}, {15, 1.78}, {20, 1.87}, {30, 1.98}, {40, 2.06}, {50, 2.14},
                              {60, 2.21}, {90, 2.40}};
const double kFairway[][2] = {{0, 2.2},   {10, 2.35}, {20, 2.40}, {40, 2.60}, {60, 2.70}, {80, 2.75}, {100, 2.80},
                              {120, 2.85}, {140, 2.91}, {160, 2.98}, {180, 3.08}, {200, 3.19}, {220, 3.32}, {240, 3.45},
                              {260, 3.58}, {280, 3.69}, {300, 3.78}, {350, 3.88}, {400, 3.99}, {450, 4.17}, {500, 4.41},
                              {550, 4.69}, {600, 4.82}};
}  // namespace

double expectedStrokes(double d, const Surface& s, bool water) {
    if (water || s.water) return 1.0 + expectedStrokes(d, Surface{}, false) + 0.1;
    const double feet = d / kFoot, yards = d / kYard;
    if (s.puttable && (s.id == "green" || feet < 30)) {
        const double e = interpTable(kPutting, 17, feet);
        return s.id == "green" ? e : e + 0.08;
    }
    double e = interpTable(kFairway, 23, yards);
    if (s.sand) e += yards < 40 ? 0.13 : 0.30;
    else if (s.id == "rough") e += yards < 40 ? 0.10 : 0.20;
    else if (s.id == "deep_rough") e += 0.38;
    else if (s.id == "native") e += 0.55;
    else if (s.id == "path") e += 0.05;
    return e;
}

// ---------------------------------------------------------------------------

ShotResult previewShot(const PlayContext& ctx, const SwingInput& in, const GolferProfile& g, bool withCup,
                       bool recordPath) {
    const int ci = ctx.clubs->find(in.club);
    const Club& club = ctx.clubs->clubs[static_cast<size_t>(ci < 0 ? 0 : ci)];
    Lie lie{&ctx.course->surfaces[ctx.surface], ctx.plugged, ctx.teed};
    SwingInput exact = in;
    exact.exact = true;
    Rng rng(1);
    const Delivery d = makeDelivery(exact, club, g, lie, 1.0, ctx.leftHanded, rng);
    const ImpactResult r = strike(club, *ctx.ball, d);
    const Launch l = toLaunch(r, ctx.ballPos, in.aimHeading);
    WindSpec w = ctx.wind;
    w.gustIntensity = 0;
    SimOptions o;
    o.recordPath = recordPath;
    o.seed = 1;
    const World world = ctx.course->world(withCup ? &ctx.cup : nullptr);
    return simulateShot(l, *ctx.ball, ctx.air, w, world, o);
}

double nominalCarry(const Club& club, const GolferProfile& g, const BallModel& ball, const Conditions& air) {
    // Cached: the planner asks for every club before every shot.
    struct Key {
        std::string club, golfer, ball;
        double speed, density;
        bool operator==(const Key& o) const {
            return club == o.club && golfer == o.golfer && ball == o.ball && speed == o.speed && density == o.density;
        }
    };
    static std::vector<std::pair<Key, double>> cache;
    const Key key{club.code, g.id, ball.id, g.speedFactor * club.tourSpeedMph * club.speedCalibration,
                  air.altitudeM * 1e3 + air.temperatureC};
    for (const auto& [k, v] : cache)
        if (k == key) return v;
    SwingInput in;
    in.club = club.code;
    in.exact = true;
    Rng rng(1);
    const Delivery d = makeDelivery(in, club, g, Lie{}, 1.0, false, rng);
    Launch l = toLaunch(strike(club, ball, d), {0, ball.radius(), 0}, 0);
    SimOptions o;
    o.recordPath = false;
    const double carry = simulateCarry(l, ball, air, {}, o).stats.carry;
    if (cache.size() > 512) cache.clear();
    cache.push_back({key, carry});
    return carry;
}

namespace {

double horiz(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.z - b.z); }

// Expected strokes after a shot finishing at `p`, with lateral risk: the
// mean over the nominal finish and points a lateral sigma either side.
double risk(const PlayContext& ctx, const Vec3& from, const Vec3& p, double lateralSd) {
    const Course& c = *ctx.course;
    const Vec3 dir = Vec3{p.x - from.x, 0, p.z - from.z}.normalized();
    const Vec3 side = dir.cross(kUp);
    double sum = 0, wsum = 0;
    const double offs[] = {-1.3, -0.6, 0.0, 0.6, 1.3};
    const double wts[] = {0.15, 0.2, 0.3, 0.2, 0.15};
    for (int i = 0; i < 5; ++i) {
        const Vec3 q = p + side * (offs[i] * lateralSd);
        double e;
        if (!c.inBounds(q.x, q.z)) e = 1.0 + expectedStrokes(horiz(from, ctx.cup.pos), c.surfaces[ctx.surface]) + 0.2;
        else {
            const GroundSample gs = c.terrain.sample(q.x, q.z);
            const Surface& s = c.surfaces[gs.surface];
            e = expectedStrokes(horiz(q, ctx.cup.pos), s, s.water);
            // Trees nearby make the next shot harder.
            if (s.id == "native" || s.id == "deep_rough") {
                for (const auto& t : c.trees)
                    if (std::hypot(t.x - q.x, t.z - q.z) < t.canopyRx + 1) {
                        e += 0.25;
                        break;
                    }
            }
        }
        sum += wts[i] * e;
        wsum += wts[i];
    }
    return sum / wsum;
}

// A point along the hole's line of play, `ahead` metres beyond the projection of `from`.
Vec3 alongLine(const HoleDef& h, const Vec3& from, double ahead) {
    // Project onto the polyline.
    double bestD = 1e18, bestS = 0, s = 0;
    for (size_t i = 1; i < h.line.size(); ++i) {
        const Vec3 a = h.line[i - 1], b = h.line[i];
        const Vec3 ab{b.x - a.x, 0, b.z - a.z};
        const double len = ab.length();
        double t = len > 0 ? clamp(((from.x - a.x) * ab.x + (from.z - a.z) * ab.z) / (len * len), 0.0, 1.0) : 0;
        const double d = std::hypot(from.x - (a.x + ab.x * t), from.z - (a.z + ab.z * t));
        if (d < bestD) {
            bestD = d;
            bestS = s + t * len;
        }
        s += len;
    }
    double target = bestS + ahead;
    s = 0;
    for (size_t i = 1; i < h.line.size(); ++i) {
        const Vec3 a = h.line[i - 1], b = h.line[i];
        const double len = std::hypot(b.x - a.x, b.z - a.z);
        if (target <= s + len || i + 1 == h.line.size()) {
            const double t = len > 0 ? clamp((target - s) / len, 0.0, 1.0) : 0;
            return {a.x + (b.x - a.x) * t, 0, a.z + (b.z - a.z) * t};
        }
        s += len;
    }
    return h.line.back();
}

bool clubAllowed(const Club& c, const Surface& s, bool teed) {
    if (c.putter) return false;
    if (c.code == "DR") return teed;
    if (s.sand) return !c.wood && c.loftDeg >= 28;
    if (s.id == "deep_rough" || s.id == "native") return !c.wood || c.code == "H4";
    return true;
}

}  // namespace

namespace {

struct Measure {
    double along = 0;    // rest point along the aim line from the ball (m)
    double lateral = 0;  // signed miss right of the target where the ball passes it
    ShotResult shot;
};

// Simulate and measure against a target point.
Measure measure(const PlayContext& ctx, const SwingInput& in, const GolferProfile& g, const Vec3& target, bool path) {
    Measure m;
    m.shot = previewShot(ctx, in, g, false, path);
    const Vec3 to{target.x - ctx.ballPos.x, 0, target.z - ctx.ballPos.z};
    const double D = std::max(to.length(), 1e-6);
    const Vec3 u = to / D;
    const Vec3 side = u.cross(kUp);
    const Vec3 rel = m.shot.final.pos - ctx.ballPos;
    m.along = rel.x * u.x + rel.z * u.z;
    m.lateral = rel.x * side.x + rel.z * side.z;
    if (path) {
        for (size_t i = 1; i < m.shot.path.size(); ++i) {
            const Vec3 p0 = m.shot.path[i - 1].pos - ctx.ballPos, p1 = m.shot.path[i].pos - ctx.ballPos;
            const double a0 = p0.x * u.x + p0.z * u.z, a1 = p1.x * u.x + p1.z * u.z;
            if (a0 < D && a1 >= D) {
                const double t = (D - a0) / std::max(a1 - a0, 1e-9);
                const Vec3 pm = p0 + (p1 - p0) * t;
                m.lateral = pm.x * side.x + pm.z * side.z;
                break;
            }
        }
    }
    return m;
}

// Solve power (bisection: the rest point moves out monotonically with power)
// and aim (the miss angle) so the ball finishes `want` metres along the line
// to `target`, on it.
SwingInput solve(const PlayContext& ctx, SwingInput in, const GolferProfile& g, const Vec3& target, double want,
                 double pLo, double pHi, int rounds, int bisections, bool path) {
    const Vec3 to{target.x - ctx.ballPos.x, 0, target.z - ctx.ballPos.z};
    const double D = std::max(to.length(), 0.5);
    const double h0 = std::atan2(to.z, to.x);
    for (int round = 0; round < rounds; ++round) {
        double lo = pLo, hi = pHi;
        for (int i = 0; i < bisections; ++i) {
            in.power = 0.5 * (lo + hi);
            const Measure m = measure(ctx, in, g, target, false);
            if (m.along < want) lo = in.power;
            else hi = in.power;
        }
        in.power = 0.5 * (lo + hi);
        const Measure m = measure(ctx, in, g, target, path);
        const double miss = path ? m.lateral : m.lateral * D / std::max(m.along, 0.5);
        if (std::abs(miss) < 0.02) break;
        in.aimHeading -= 0.95 * std::atan2(miss, D);
        in.aimHeading = h0 + clamp(wrapPi(in.aimHeading - h0), -0.9, 0.9);
    }
    return in;
}

}  // namespace

Plan planPutt(const PlayContext& ctx, const GolferProfile& g) {
    Plan plan;
    SwingInput in;
    in.club = "PT";
    const Vec3 to{ctx.cup.pos.x - ctx.ballPos.x, 0, ctx.cup.pos.z - ctx.ballPos.z};
    const double D = to.length();
    in.aimHeading = std::atan2(to.z, to.x);
    // Finish about 35 cm past (Pelz's "17 inches past"; less on a long putt's dying pace).
    const double want = D + (D < 6 ? 0.35 : 0.25);
    in = solve(ctx, in, g, ctx.cup.pos, want, 0.005, 1.6, 4, 16, true);
    plan.input = in;
    plan.target = ctx.cup.pos;
    plan.expected = expectedStrokes(D, ctx.course->surfaces[ctx.course->idGreen]) - 1;
    plan.note = "putt";
    return plan;
}

Plan planShot(const PlayContext& ctx, const GolferProfile& g) {
    const Course& c = *ctx.course;
    const Surface& lie = c.surfaces[ctx.surface];
    const double D = horiz(ctx.ballPos, ctx.cup.pos);
    if (lie.puttable && (lie.id == "green" || D < 25)) return planPutt(ctx, g);

    Plan best;
    best.expected = 1e9;
    const double lateralPerM = std::tan((g.faceSdDeg * 1.1 + 0.3) * kDeg) * lie.lieDispersion;
    auto consider = [&](const SwingInput& in, const char* note) {
        const ShotResult r = previewShot(ctx, in, g, true, false);
        const double travelled = horiz(ctx.ballPos, r.final.pos);
        double e = r.outcome == Outcome::Water || r.outcome == Outcome::Lost
                       ? 2.0 + expectedStrokes(D, lie)
                       : risk(ctx, ctx.ballPos, r.final.pos, lateralPerM * std::max(travelled, 10.0));
        if (r.outcome == Outcome::Holed) e = 0;
        if (e < best.expected) {
            best.expected = e;
            best.input = in;
            best.target = r.final.pos;
            best.note = note;
        }
    };
    const double h0 = std::atan2(ctx.cup.pos.z - ctx.ballPos.z, ctx.cup.pos.x - ctx.ballPos.x);

    if (D < 75) {
        // The short game: every wedge (and a bump-and-run iron), square or
        // with the face opened, pace solved to finish at the hole.
        for (const Club& club : ctx.clubs->clubs) {
            if (!clubAllowed(club, lie, ctx.teed) || club.loftDeg < 28) continue;
            if (lie.sand && club.loftDeg < 50) continue;
            for (const double loft : {0.0, 8.0, 16.0}) {
                if (loft > 0 && club.loftDeg < 50) continue;
                SwingInput in;
                in.club = club.code;
                in.loftDeg = loft;
                in.aimHeading = h0;
                in = solve(ctx, in, g, ctx.cup.pos, D + 0.3, 0.05, 1.04, 2, 10, false);
                consider(in, loft > 0 ? "open face, at the pin" : "at the pin");
            }
        }
        if (best.expected < 1e9) return best;
    }

    for (const Club& club : ctx.clubs->clubs) {
        if (!clubAllowed(club, lie, ctx.teed)) continue;
        const double carry = nominalCarry(club, g, *ctx.ball, ctx.air) * lie.lieSpeed;
        if (carry > D * 1.35 && club.loftDeg < 44) continue;  // far too much club
        if (carry * 1.08 >= D) {
            // At the pin: pace and line solved for the finish.
            SwingInput in;
            in.club = club.code;
            in.aimHeading = h0;
            in = solve(ctx, in, g, ctx.cup.pos, D, 0.2, 1.04, 2, 9, false);
            consider(in, "at the pin");
        } else {
            // Out of reach: a full swing to points along the line of play.
            for (const double f : {1.05, 0.85}) {
                const Vec3 target = alongLine(*ctx.hole, ctx.ballPos, carry * f);
                SwingInput in;
                in.club = club.code;
                in.power = 1.0;
                in.aimHeading = std::atan2(target.z - ctx.ballPos.z, target.x - ctx.ballPos.x);
                for (int it = 0; it < 2; ++it) {
                    const ShotResult r = previewShot(ctx, in, g, false, false);
                    const double gotH = std::atan2(r.final.pos.z - ctx.ballPos.z, r.final.pos.x - ctx.ballPos.x);
                    const double wantH = std::atan2(target.z - ctx.ballPos.z, target.x - ctx.ballPos.x);
                    in.aimHeading += 0.9 * wrapPi(wantH - gotH);
                }
                consider(in, "along the line");
            }
        }
    }
    if (best.expected >= 1e9) {
        best.input.club = "SW";
        best.input.aimHeading = h0;
        best.input.power = 0.5;
        best.target = ctx.cup.pos;
    }
    return best;
}

}  // namespace golf
