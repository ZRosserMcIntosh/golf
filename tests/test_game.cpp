// Golf Dynamics — rounds: the computer golfer, the rules, scoring and exact replay.
// SPDX-License-Identifier: MIT
#include <cstdio>

#include "golf/match.hpp"
#include "golf_test.hpp"
#include "test_util.hpp"

using namespace golf;

namespace {
Course& varzea() {
    static Course c = [] {
        Course k;
        loadCourse(golftest::loadJson("courses/varzea.json"), k);
        buildCourse(k);
        return k;
    }();
    return c;
}

MatchConfig aiPair(std::uint64_t seed) {
    MatchConfig cfg;
    cfg.seed = seed;
    PlayerConfig a, b;
    a.name = "Ana";
    a.golfer = "tour";
    a.ai = true;
    b.name = "Bruno";
    b.golfer = "mid";
    b.ai = true;
    b.handicapIndex = 14;
    cfg.players = {a, b};
    return cfg;
}

int playOut(Match& m, int cap = 400) {
    int shots = 0;
    while (!m.finished() && shots < cap) {
        const Plan p = m.plan();
        m.play(p.input);
        ++shots;
    }
    return shots;
}
}  // namespace

TEST_CASE("game: two computer golfers play nine holes") {
    Match m(varzea(), aiPair(7));
    const int shots = playOut(m);
    CHECK(m.finished());
    std::printf("  %d shots\n", shots);
    for (int p = 0; p < 2; ++p) {
        std::printf("  %-6s", m.config().players[static_cast<size_t>(p)].name.c_str());
        for (int h = 0; h < m.holeCount(); ++h) std::printf(" %d", m.player(p).gross[static_cast<size_t>(h)]);
        std::printf("  = %d (%+d), %d Stableford points\n", m.totalGross(p), m.totalToPar(p), m.totalStableford(p));
    }
    // A tour pro on nine holes of par 36: within a few shots of par.
    CHECK_RANGE(m.totalToPar(0), -7, 7);
    CHECK_RANGE(m.totalToPar(1), -3, 25);
}

TEST_CASE("game: a round replays exactly from its configuration and inputs") {
    Match a(varzea(), aiPair(11));
    playOut(a, 30);
    const std::string saved = a.toJson();
    const auto parsed = json::parse(saved);
    CHECK(parsed.ok());
    MatchConfig cfg;
    CHECK(parseMatchConfig(*parsed.value.find("config"), cfg).empty());
    Match b(varzea(), cfg);
    for (const auto& s : parsed.value.find("shots")->items()) b.play(parseSwing(s));
    CHECK(a.log().size() == b.log().size());
    bool same = true;
    for (size_t i = 0; i < a.log().size(); ++i) same &= a.log()[i].to == b.log()[i].to;
    CHECK(same);
    CHECK(a.stateJson() == b.stateJson());
}

TEST_CASE("game: penalty area relief and stroke and distance") {
    Course& c = varzea();
    // Hole 6 plays over water: a weak shot finds it.
    MatchConfig cfg;
    cfg.holes = {5};
    cfg.windScale = 0;
    PlayerConfig p;
    p.golfer = "tour";
    cfg.players = {p};
    Match m(c, cfg);
    SwingInput in;
    in.club = "PW";
    in.exact = true;
    const HoleDef& h = c.holes[5];
    const Lake& lake = h.lakes[0];
    in.aimHeading = std::atan2(lake.lobes[0].z - m.player(0).pos.z, lake.lobes[0].x - m.player(0).pos.x);
    // The power that drops the ball into the middle of the lake.
    for (double pw = 0.2; pw < 1.0; pw += 0.02) {
        in.power = pw;
        if (previewShot(m.context(0), in, findGolfer("tour"), true, false).outcome == Outcome::Water) break;
    }
    const auto r = m.play(in);
    std::printf("  short over the water: %s, penalty %d, %s\n", outcomeName(r.shot.outcome), r.record.ruling.penalty,
                r.record.ruling.note.c_str());
    CHECK(r.shot.outcome == Outcome::Water);
    CHECK(r.record.ruling.penalty == 1);
    CHECK(m.player(0).strokes == 2);
    CHECK(!c.surfaces[c.terrain.sample(m.player(0).pos.x, m.player(0).pos.z).surface].water);
    // Out of bounds: stroke and distance back to where the shot was played.
    const Vec3 before = m.player(0).pos;
    SwingInput ob = in;
    ob.club = "DR";
    ob.power = 1.0;
    ob.aimHeading += kPi;  // straight back over the tee, off the property
    const auto r2 = m.play(ob);
    std::printf("  hit backwards: %s, %s\n", rulingName(r2.record.ruling.kind), r2.record.ruling.note.c_str());
    if (r2.record.ruling.kind == Ruling::Kind::OutOfBounds || r2.record.ruling.kind == Ruling::Kind::Lost) {
        CHECK(m.player(0).strokes == 4);
        CHECK(std::abs(m.player(0).pos.x - before.x) < 1e-9);
    }
}

TEST_CASE("game: the computer reads and holes short putts") {
    Course& c = varzea();
    MatchConfig cfg;
    cfg.holes = {0};
    cfg.windScale = 0;
    PlayerConfig p;
    p.golfer = "tour";
    p.ai = true;
    cfg.players = {p};
    int made = 0, tries = 0;
    for (std::uint64_t s = 1; s <= 12; ++s) {
        cfg.seed = s;
        Match m(c, cfg);
        // Put the ball 2.5 m from the hole on the green and let the computer putt.
        const Cup cup = m.cup(0, true);
        PlayContext ctx = m.context(0);
        const double a = static_cast<double>(s) * 0.5;
        const Vec3 at{cup.pos.x + 2.5 * std::cos(a), 0, cup.pos.z + 2.5 * std::sin(a)};
        ctx.ballPos = {at.x, c.terrain.height(at.x, at.z) + BallModel{}.radius(), at.z};
        ctx.surface = c.idGreen;
        ctx.teed = false;
        ctx.cup = cup;
        const Plan plan = planPutt(ctx, findGolfer("tour"));
        const ShotResult r = previewShot(ctx, plan.input, findGolfer("tour"), true, false);
        made += r.outcome == Outcome::Holed;
        ++tries;
    }
    std::printf("  2.5 m putts holed without scatter: %d of %d\n", made, tries);
    CHECK(made >= tries - 1);
}

TEST_CASE("game: Stableford and handicap strokes") {
    Match m(varzea(), aiPair(3));
    // A 14 handicap over nine holes receives 7 strokes, one each on the 7 lowest stroke indexes.
    int got = 0;
    for (int h = 0; h < m.holeCount(); ++h) got += m.strokesReceived(1, h);
    CHECK(got == 7);
    CHECK(m.strokesReceived(0, 0) == 0);
}
