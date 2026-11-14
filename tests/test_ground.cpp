// Golf Dynamics — bounce, roll, the Stimpmeter and slopes.
// SPDX-License-Identifier: MIT
#include <cstdio>

#include "golf/shot.hpp"
#include "golf_test.hpp"

using namespace golf;

namespace {

Conditions air25() {
    Conditions c;
    c.temperatureC = 25;
    return c;
}

Launch fullShot(double mph, double vla, double rpm) {
    Launch l;
    l.position = {0, BallModel{}.radius(), 0};
    l.speed = mph * kMph;
    l.vlaDeg = vla;
    l.spinRpm = rpm;
    return l;
}

// A ball already rolling (pure topspin) at speed v along +x.
Launch rolling(double v, double heading = 0, Vec3 at = {0, 0, 0}) {
    Launch l;
    l.position = at + Vec3{0, BallModel{}.radius(), 0};
    l.targetHeading = heading;
    l.speed = v;
    l.spinRpm = -v / BallModel{}.radius() / kRpm;
    return l;
}

}  // namespace

TEST_CASE("ground: Penner restitution and the Stimpmeter closed form") {
    Surface s;
    CHECK_NEAR(s.restitutionAt(0), 0.510, 1e-12);
    CHECK_NEAR(s.restitutionAt(10), 0.510 - 0.375 + 0.0903, 1e-12);
    CHECK_NEAR(s.restitutionAt(25), 0.120, 1e-12);
    Surface g = s;
    g.setStimp(11.0);
    CHECK_NEAR(g.stimpFeet(), 11.0, 1e-9);
    // Constant law: d = (1+k) v0^2 / (2 mu g)  =>  stimp 10 ft is mu = 0.0819.
    Surface c = s;
    c.rollLaw = RollLaw::Constant;
    c.setStimp(10.0);
    CHECK_NEAR(c.muRoll, 1.4 * 1.83 * 1.83 / (2 * kGravity * 3.048), 1e-9);
}

TEST_CASE("ground: a ball rolled off the Stimpmeter rolls its reading") {
    SurfaceTable st;
    for (double stimp : {8.0, 10.0, 13.0}) {
        SurfaceTable t = st;
        t.at(t.find("green")).setStimp(stimp);
        const FlatTerrain green(t.find("green"));
        World w;
        w.terrain = &green;
        w.surfaces = &t;
        const ShotResult r = simulateShot(rolling(kStimpmeterSpeed), BallModel::tour(), air25(), {}, w);
        const double feet = r.stats.total / kFoot;
        std::printf("  stimp %.0f: rolled %.2f ft\n", stimp, feet);
        CHECK(std::abs(feet / stimp - 1) < 0.01);
        CHECK(r.outcome == Outcome::Rest);
    }
}

TEST_CASE("ground: drives roll out like the USGA/R&A bounce model") {
    SurfaceTable st;
    const FlatTerrain fairway(st.find("fairway"));
    World w;
    w.terrain = &fairway;
    w.surfaces = &st;
    const ShotResult r = simulateShot(fullShot(167, 10.9, 2686), BallModel::tour(), air25(), {}, w);
    const double run = (r.stats.total - r.stats.carry) / kYard;
    // USGA & R&A (2021) "Proposed bounce model": run = 79.1 - 1.6 * landing angle (yd).
    const double model = 79.1 - 1.6 * r.stats.landingAngleDeg;
    std::printf("  drive: carry %.1f yd, land %.1f deg, run %.1f yd (USGA/R&A model %.1f yd), %d bounces\n",
                r.stats.carry / kYard, r.stats.landingAngleDeg, run, model, r.stats.bounces);
    CHECK(std::abs(run - model) < 8.0);
    CHECK(r.stats.bounces >= 2 && r.stats.bounces < 15);
}

TEST_CASE("ground: a wedge checks up on the green; a high-spin wedge spins back on a firm one") {
    SurfaceTable st;
    const FlatTerrain green(st.find("green"));
    World w;
    w.terrain = &green;
    w.surfaces = &st;
    const ShotResult pw = simulateShot(fullShot(102, 24.2, 9304), BallModel::tour(), air25(), {}, w);
    const double run = (pw.stats.total - pw.stats.carry) / kYard;
    std::printf("  PW on a green: run %+.1f yd\n", run);
    CHECK(std::abs(run) < 6);
    SurfaceTable firm = st;
    firm.at(firm.find("green")).muSlide = 0.25;  // Penner's firm green
    World wf = w;
    wf.surfaces = &firm;
    // Penner (2002) Fig. 8: a 9-iron landing with heavy backspin bounces forward and runs back.
    const ShotResult lob = simulateShot(fullShot(85, 32, 11000), BallModel::tour(), air25(), {}, wf);
    // Furthest point reached after landing, against the rest point.
    double far = 0;
    for (const auto& p : lob.path) far = std::max(far, p.pos.x);
    std::printf("  high-spin wedge on a firm green: carry %.1f m, furthest %.1f m, rest %.1f m\n", lob.stats.carry, far,
                lob.final.pos.x);
    CHECK(lob.final.pos.x < far - 0.3);  // came back
}

TEST_CASE("ground: slopes hold, roll and break putts") {
    SurfaceTable st;
    const int g = st.find("green");
    const Surface& green = st[g];
    // tan(theta) <= mu_roll holds a ball at rest.
    {
        const double grade = green.muRoll * 0.8;
        const PlaneTerrain plane(g, 0, grade, 0);
        World w;
        w.terrain = &plane;
        w.surfaces = &st;
        const ShotResult r = simulateShot(rolling(0.0), BallModel::tour(), air25(), {}, w);
        CHECK(r.stats.total < 0.01);
    }
    {
        const double grade = green.muRoll * 1.5;
        const PlaneTerrain plane(g, 0, grade, 0);
        World w;
        w.terrain = &plane;
        w.surfaces = &st;
        w.minX = -30;
        const ShotResult r = simulateShot(rolling(0.01), BallModel::tour(), air25(), {}, w);
        CHECK(r.final.pos.x < -0.5);  // rolled downhill (toward -x)
    }
    {
        // A putt along +x on a 2 % slope falling toward +z breaks toward +z.
        const PlaneTerrain plane(g, 0, 0, -0.02);
        World w;
        w.terrain = &plane;
        w.surfaces = &st;
        const ShotResult r = simulateShot(rolling(2.0), BallModel::tour(), air25(), {}, w);
        std::printf("  2 %% side slope, 2 m/s putt: %.2f m long, %.2f m break\n", r.final.pos.x, r.final.pos.z);
        CHECK(r.final.pos.z > 0.2);
        // The ball must end at rest on the plane.
        CHECK_NEAR(r.final.pos.y, plane.height(r.final.pos.x, r.final.pos.z) + BallModel{}.radius(), 1e-9);
    }
}

TEST_CASE("ground: water, sand and the cart path") {
    SurfaceTable st;
    World w;
    w.surfaces = &st;
    const FlatTerrain water(st.find("water"));
    w.terrain = &water;
    CHECK(simulateShot(fullShot(120, 16.3, 7097), BallModel::tour(), air25(), {}, w).outcome == Outcome::Water);
    // A low, fast ball skips.
    const ShotResult skip = simulateShot(fullShot(140, 3, 1200), BallModel::tour(), air25(), {}, w);
    bool skipped = false;
    for (const auto& e : skip.events) skipped |= e.kind == EventKind::Bounce;
    CHECK(skipped);
    const FlatTerrain sand(st.find("bunker"));
    w.terrain = &sand;
    const ShotResult inSand = simulateShot(fullShot(102, 24.2, 9304), BallModel::tour(), air25(), {}, w);
    std::printf("  PW into sand: %s, run %.1f m\n", outcomeName(inSand.outcome), inSand.stats.total - inSand.stats.carry);
    CHECK(inSand.stats.total - inSand.stats.carry < 2.0);
    const FlatTerrain path(st.find("path"));
    w.terrain = &path;
    const FlatTerrain fair(st.find("fairway"));
    World wf = w;
    wf.terrain = &fair;
    const double onPath = simulateShot(fullShot(120, 16.3, 7097), BallModel::tour(), air25(), {}, w).stats.total;
    const double onFair = simulateShot(fullShot(120, 16.3, 7097), BallModel::tour(), air25(), {}, wf).stats.total;
    CHECK(onPath > onFair + 5);
}
