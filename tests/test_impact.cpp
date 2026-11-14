// Golf Dynamics — club-ball impact against TrackMan, conservation and gear effect.
// SPDX-License-Identifier: MIT
#include <cstdio>

#include "golf/impact.hpp"
#include "golf_test.hpp"
#include "test_util.hpp"

using namespace golf;

namespace {
Delivery tour(const Club& c) {
    Delivery d;
    d.clubSpeed = c.tourSpeedMph * kMph * c.speedCalibration;
    d.attackDeg = c.tourAttackDeg;
    d.dynamicLoftDeg = c.tourDynamicLoftDeg;
    return d;
}
}  // namespace

TEST_CASE("impact: tour delivery reproduces TrackMan ball speed and spin") {
    const ClubSet bag = ClubSet::standard();
    CHECK(bag.clubs.size() == 14);
    const json::Value tm = golftest::loadJson("validation/trackman_pga_tour.json");
    const char* codes[] = {"DR", "W3", "W5", "H4", "I3", "I4", "I5", "I6", "I7", "I8", "I9", "PW"};
    int i = 0;
    std::printf("  %-7s %6s %16s %18s %8s\n", "club", "dynlft", "ball mph", "spin rpm", "smash");
    for (const auto& row : tm.find("clubs")->items()) {
        const char* code = codes[i++];
        const int ci = bag.find(code);
        if (ci < 0) continue;  // no 3-iron in the standard bag
        const Club& c = bag.clubs[static_cast<size_t>(ci)];
        const ImpactResult r = strike(c, BallModel::tour(), tour(c));
        const double bs = golftest::num(row, "ball_speed"), sp = golftest::num(row, "spin");
        std::printf("  %-7s %6.1f %7.1f vs %5.0f %8.0f vs %5.0f %8.2f\n", code, c.tourDynamicLoftDeg, r.speed / kMph, bs,
                    r.spinRpm, sp, r.smash);
        CHECK_NEAR(r.vlaDeg, golftest::num(row, "launch"), 1e-6);  // solved for
        CHECK(std::abs(r.speed / kMph / bs - 1) < 0.002);  // calibrated
        CHECK_RANGE(c.speedCalibration, 0.97, 1.06);
        CHECK(std::abs(r.spinRpm / sp - 1) < 0.12);  // a prediction: only loft and speed were fitted
        CHECK(std::abs(r.hlaDeg) < 1e-9);
        CHECK(std::abs(r.spinAxisDeg) < 1e-9);
    }
}

TEST_CASE("impact: momentum and the smash-factor limit") {
    const ClubSet bag = ClubSet::standard();
    const Club& dr = bag.clubs[0];
    const ImpactResult r = strike(dr, BallModel::tour(), tour(dr));
    // Head-on rigid limit: (1 + e) / (1 + m/M) = 1.83 / 1.226 = 1.49.
    CHECK_RANGE(r.smash, 1.38, 1.46);  // the model's rigid head
    CHECK_NEAR(r.speed / (dr.tourSpeedMph * kMph), 1.478, 0.005);  // TrackMan 167 / 113
    CHECK(r.contactTimeS > 3e-4 && r.contactTimeS < 6e-4);
}

TEST_CASE("impact: the D-plane, face controls start, path controls curve") {
    const ClubSet bag = ClubSet::standard();
    const Club& c = bag.clubs[static_cast<size_t>(bag.find("I7"))];
    Delivery open = tour(c);
    open.faceAngleDeg = 3;  // face open, path square: starts right, fades
    const ImpactResult a = strike(c, BallModel::tour(), open);
    CHECK(a.hlaDeg > 2.0 && a.hlaDeg < 3.0);  // mostly face (TrackMan: ~75-85 % face for irons)
    CHECK(a.spinAxisDeg > 3);
    Delivery inOut = tour(c);
    inOut.pathDeg = 4;  // in-to-out path, square face: starts slightly right, draws
    const ImpactResult b = strike(c, BallModel::tour(), inOut);
    CHECK(b.hlaDeg > 0 && b.hlaDeg < 1.5);
    CHECK(b.spinAxisDeg < -3);
}

TEST_CASE("impact: gear effect on toe and heel strikes of a driver") {
    const ClubSet bag = ClubSet::standard();
    const Club& dr = bag.clubs[0];
    Delivery toe = tour(dr), heel = tour(dr);
    toe.strikeToeM = 0.015;
    heel.strikeToeM = -0.015;
    const ImpactResult t = strike(dr, BallModel::tour(), toe);
    const ImpactResult h = strike(dr, BallModel::tour(), heel);
    // Right-hander: a toe strike starts right (bulge) with a draw axis; heel the reverse.
    std::printf("  toe: HLA %+.1f axis %+.1f  heel: HLA %+.1f axis %+.1f  centre speed %.1f toe %.1f mph\n", t.hlaDeg,
                t.spinAxisDeg, h.hlaDeg, h.spinAxisDeg, strike(dr, BallModel::tour(), tour(dr)).speed / kMph,
                t.speed / kMph);
    CHECK(t.hlaDeg > 0.5);
    CHECK(t.spinAxisDeg < -1);
    CHECK(h.hlaDeg < -0.5);
    CHECK(h.spinAxisDeg > 1);
    CHECK(t.speed < strike(dr, BallModel::tour(), tour(dr)).speed);  // off-centre loses ball speed
    // High on the face: less spin (vertical gear effect), as fitters advise.
    Delivery high = tour(dr);
    high.strikeUpM = 0.01;
    CHECK(strike(dr, BallModel::tour(), high).spinRpm < strike(dr, BallModel::tour(), tour(dr)).spinRpm - 200);
}

TEST_CASE("impact: grass between face and ball cuts the spin (flyer)") {
    const ClubSet bag = ClubSet::standard();
    const Club& c = bag.clubs[static_cast<size_t>(bag.find("I7"))];
    Delivery clean = tour(c), flyer = tour(c);
    flyer.frictionScale = 0.15;
    const ImpactResult a = strike(c, BallModel::tour(), clean), b = strike(c, BallModel::tour(), flyer);
    std::printf("  7-iron spin clean %.0f rpm, from rough %.0f rpm\n", a.spinRpm, b.spinRpm);
    CHECK(b.spinRpm < 0.75 * a.spinRpm);
}

TEST_CASE("impact: putter launches low with little spin") {
    const ClubSet bag = ClubSet::standard();
    const Club& p = bag.clubs[static_cast<size_t>(bag.find("PT"))];
    Delivery d;
    d.clubSpeed = 2.0;
    d.attackDeg = 2;
    d.dynamicLoftDeg = 4;
    const ImpactResult r = strike(p, BallModel::tour(), d);
    std::printf("  putt: %.2f m/s, launch %.1f deg, %.0f rpm, smash %.2f\n", r.speed, r.vlaDeg, r.spinRpm, r.smash);
    CHECK_RANGE(r.smash, 1.4, 1.75);
    CHECK_RANGE(r.vlaDeg, 1.0, 5.0);
    CHECK(std::abs(r.spinRpm) < 200);
}
