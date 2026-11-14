// Golf Dynamics — flight: launch conventions and carry against TrackMan.
// SPDX-License-Identifier: MIT
#include <cstdio>

#include "golf/shot.hpp"
#include "golf_test.hpp"
#include "test_util.hpp"

using namespace golf;

namespace {

Conditions trackmanAir() {
    Conditions c;
    c.temperatureC = 25.0;
    c.relativeHumidity = 0.5;
    return c;
}

Launch launch(double mph, double vla, double rpm, double axis = 0, double hla = 0) {
    Launch l;
    l.position = {0, BallModel{}.radius(), 0};
    l.speed = mph * kMph;
    l.vlaDeg = vla;
    l.spinRpm = rpm;
    l.spinAxisDeg = axis;
    l.hlaDeg = hla;
    return l;
}

}  // namespace

TEST_CASE("flight: launch frame conventions (HLA and spin axis positive right)") {
    const BallState s = launchState(launch(150, 12, 3000, 0, 5));
    CHECK(s.vel.z > 0);  // target +x, right is +z
    CHECK(s.vel.y > 0);
    // Pure backspin spins about the axis to the right of the line of flight.
    const BallState b = launchState(launch(150, 12, 3000));
    CHECK_NEAR(b.spin.z, 3000 * kRpm, 1e-9);
    // A right-tilted axis curves the ball right; left-tilted left.
    const ShotResult fade = simulateCarry(launch(150, 12, 3000, 15), BallModel::tour(), trackmanAir(), {});
    const ShotResult draw = simulateCarry(launch(150, 12, 3000, -15), BallModel::tour(), trackmanAir(), {});
    CHECK(fade.stats.carryOffline > 5);
    CHECK(draw.stats.carryOffline < -5);
    CHECK_NEAR(fade.stats.carryOffline, -draw.stats.carryOffline, 1e-6);
}

TEST_CASE("flight: TrackMan PGA Tour averages, carry, height and landing angle") {
    const json::Value tm = golftest::loadJson("validation/trackman_pga_tour.json");
    const json::Value* clubs = tm.find("clubs");
    CHECK(clubs && clubs->isArray());
    if (!clubs) return;
    double sumSq = 0;
    int n = 0;
    std::printf("  %-8s %18s %16s %14s\n", "club", "carry yd", "height yd", "land deg");
    for (const auto& c : clubs->items()) {
        const auto& name = c.find("club")->asString();
        const ShotResult r = simulateCarry(
            launch(golftest::num(c, "ball_speed"), golftest::num(c, "launch"), golftest::num(c, "spin")),
            BallModel::tour(), trackmanAir(), {});
        const double carry = r.stats.carry / kYard, h = r.stats.apex / kYard;
        const double tc = golftest::num(c, "carry"), th = golftest::num(c, "height"), tl = golftest::num(c, "land");
        std::printf("  %-8s %7.1f vs %5.0f %6.1f vs %4.0f %5.1f vs %3.0f\n", name.c_str(), carry, tc, h, th,
                    r.stats.landingAngleDeg, tl);
        CHECK(std::abs(carry / tc - 1) < 0.045);
        CHECK(std::abs(h - th) <= 5.0);
        CHECK(std::abs(r.stats.landingAngleDeg - tl) <= 6.0);
        sumSq += (carry - tc) * (carry - tc);
        ++n;
    }
    const double rmse = std::sqrt(sumSq / n);
    std::printf("  carry RMSE %.1f yd over %d clubs\n", rmse, n);
    CHECK(rmse < 6.0);
}

TEST_CASE("flight: integrator converged (halving the step moves a drive < 5 cm)") {
    SimOptions a, b;
    b.flightDt = a.flightDt / 2;
    const ShotResult r1 = simulateCarry(launch(167, 10.9, 2686), BallModel::tour(), trackmanAir(), {}, a);
    const ShotResult r2 = simulateCarry(launch(167, 10.9, 2686), BallModel::tour(), trackmanAir(), {}, b);
    CHECK_NEAR(r1.stats.carry, r2.stats.carry, 0.05);
}

TEST_CASE("flight: altitude and temperature (density) effects") {
    Conditions sea = trackmanAir();
    Conditions denver = sea;
    denver.altitudeM = 1609;
    const ShotResult s = simulateCarry(launch(167, 10.9, 2686), BallModel::tour(), sea, {});
    const ShotResult d = simulateCarry(launch(167, 10.9, 2686), BallModel::tour(), denver, {});
    const double gain = d.stats.carry / s.stats.carry - 1;
    std::printf("  Denver (1609 m) driver carry gain %.1f %%\n", gain * 100);
    // Published rules of thumb: about 6 % (e.g. TrackMan, Titleist): dimple +6.9 %.
    CHECK_RANGE(gain, 0.04, 0.09);
    Conditions hot = sea, cold = sea;
    hot.temperatureC = 32.2;   // 90 F
    cold.temperatureC = 10.0;  // 50 F
    const double dHot = simulateCarry(launch(167, 10.9, 2686), BallModel::tour(), hot, {}).stats.carry;
    const double dCold = simulateCarry(launch(167, 10.9, 2686), BallModel::tour(), cold, {}).stats.carry;
    std::printf("  90 F vs 50 F driver carry %+.1f yd\n", (dHot - dCold) / kYard);
    CHECK(dHot > dCold);
}

TEST_CASE("flight: wind into, down and across") {
    const Launch l = launch(167, 10.9, 2686);
    const double calm = simulateCarry(l, BallModel::tour(), trackmanAir(), {}).stats.carry;
    WindSpec into, down, cross;
    into.speed10 = 5;
    into.towardHeading = kPi;  // blowing toward the golfer
    down.speed10 = 5;
    down.towardHeading = 0;
    cross.speed10 = 5;
    cross.towardHeading = kPi / 2;  // toward +z, the right
    const ShotResult ri = simulateCarry(l, BallModel::tour(), trackmanAir(), into);
    const ShotResult rd = simulateCarry(l, BallModel::tour(), trackmanAir(), down);
    const ShotResult rc = simulateCarry(l, BallModel::tour(), trackmanAir(), cross);
    std::printf("  5 m/s: into %+.1f yd, down %+.1f yd, cross %+.1f yd right\n", (ri.stats.carry - calm) / kYard,
                (rd.stats.carry - calm) / kYard, rc.stats.carryOffline / kYard);
    CHECK(ri.stats.carry < calm - 10);
    CHECK(rd.stats.carry > calm + 5);
    // A headwind hurts more than a tailwind helps.
    CHECK(calm - ri.stats.carry > rd.stats.carry - calm);
    CHECK(rc.stats.carryOffline > 5);
}

TEST_CASE("flight: gusts replay exactly from the seed") {
    WindSpec w;
    w.speed10 = 6;
    w.gustIntensity = 0.2;
    SimOptions o1, o2, o3;
    o1.seed = o2.seed = 42;
    o3.seed = 43;
    const Launch l = launch(150, 12, 4000);
    const ShotResult a = simulateCarry(l, BallModel::tour(), trackmanAir(), w, o1);
    const ShotResult b = simulateCarry(l, BallModel::tour(), trackmanAir(), w, o2);
    const ShotResult c = simulateCarry(l, BallModel::tour(), trackmanAir(), w, o3);
    CHECK(a.final.pos == b.final.pos);
    CHECK(!(a.final.pos == c.final.pos));
}
