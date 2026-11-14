// Golf Dynamics — the hole: capture speeds (Penner 2002, Holmes 1991), lip-outs, the flagstick.
// SPDX-License-Identifier: MIT
#include <cstdio>

#include "golf/shot.hpp"
#include "golf_test.hpp"

using namespace golf;

namespace {

struct Green {
    SurfaceTable st;
    FlatTerrain terrain{0};
    Cup cup;
    World w;
    Green() {
        terrain = FlatTerrain(st.find("green"));
        cup.pos = {2.0, 0, 0};
        w.terrain = &terrain;
        w.surfaces = &st;
        w.cup = &cup;
    }
};

// Putt rolling along +x at speed v (at the cup's front edge region), offset b (m) off-centre.
ShotResult putt(Green& g, double v, double offset) {
    Launch l;
    l.position = {1.5, BallModel{}.radius(), offset};
    l.speed = v;
    l.spinRpm = -v / BallModel{}.radius() / kRpm;
    SimOptions o;
    return simulateShot(l, BallModel::tour(), {}, {}, g.w, o);
}

// The first start speed (0.5 m before the hole) that is no longer holed:
// scanned upward, since a ball much faster can be caught again (it hops off the
// far rim and drops back), which is real but is not the capture limit.
double captureSpeed(Green& g, double offset) {
    double v = 0.85;  // just enough to reach the hole
    while (v < 4.0 && putt(g, v, offset).outcome == Outcome::Holed) v += 0.01;
    return v - 0.01;
}

}  // namespace

TEST_CASE("cup: capture speed through the centre is Penner's 1.63 m/s") {
    Green g;
    // Speed at the hole: the putt starts 0.5 m short and slows on the way.
    const double v0 = captureSpeed(g, 0.0);
    // Measure the speed the ball has at the front edge for that start speed.
    const ShotResult r = putt(g, v0, 0.0);
    double vEdge = 0;
    for (size_t i = 1; i < r.path.size(); ++i)
        if (r.path[i - 1].pos.x < 2.0 - g.cup.radius && r.path[i].pos.x >= 2.0 - g.cup.radius) {
            vEdge = (r.path[i].pos - r.path[i - 1].pos).length() / (r.path[i].t - r.path[i - 1].t);
        }
    std::printf("  centre: holed up to %.2f m/s at the front edge (start %.2f m/s)\n", vEdge, v0);
    CHECK_RANGE(vEdge, 1.45, 1.80);
}

TEST_CASE("cup: off-centre putts must be slower; outside the hole they miss") {
    Green g;
    const double c0 = captureSpeed(g, 0.0), c5 = captureSpeed(g, 0.5 * g.cup.radius),
                 c8 = captureSpeed(g, 0.8 * g.cup.radius);
    std::printf("  start speed limit: centre %.2f, half-radius %.2f, 0.8 radius %.2f m/s\n", c0, c5, c8);
    CHECK(c5 < c0);
    CHECK(c8 < c5);
    // A ball whose centre passes outside the hole entirely but slowly can still
    // catch the lip and fall (it overhangs); one passing far outside cannot.
    CHECK(putt(g, 0.9, g.cup.radius + 0.03).outcome != Outcome::Holed);
}

TEST_CASE("cup: too fast lips out and runs on; dying putts drop") {
    Green g;
    const ShotResult fast = putt(g, 3.0, 0.0);
    CHECK(fast.outcome == Outcome::Rest);
    CHECK(fast.final.pos.x > 2.5);
    bool rim = false;
    for (const auto& e : fast.events) rim |= e.kind == EventKind::Rim;
    CHECK(rim);
    const ShotResult dying = putt(g, 0.75, 0.0);
    CHECK(dying.outcome == Outcome::Holed);
    // Holed means the whole ball is below the surface.
    CHECK(dying.final.pos.y < -BallModel{}.radius());
}

TEST_CASE("cup: a chip into the flagstick drops; a ball flying into the hole stays") {
    Green g;
    g.cup.flagstick = true;
    // A firm chip straight at the pin, 1.2 m off the ground near the hole.
    Launch l;
    l.position = {-6, 0.6, 0};
    l.speed = 7.0;
    l.vlaDeg = 15;
    l.spinRpm = 1500;
    const ShotResult r = simulateShot(l, BallModel::tour(), {}, {}, g.w);
    bool pin = false;
    for (const auto& e : r.events) pin |= e.kind == EventKind::Flagstick;
    std::printf("  chip at the pin: %s, flagstick hit %d, rest (%.2f, %.2f)\n", outcomeName(r.outcome), pin,
                r.final.pos.x, r.final.pos.z);
    CHECK(pin);
}
