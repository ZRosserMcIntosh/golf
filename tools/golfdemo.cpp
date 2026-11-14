// Golf Dynamics — command-line demo: validation tables and a computer round.
// SPDX-License-Identifier: MIT
//
//   golfdemo validate          flight and impact against the TrackMan averages
//   golfdemo round [seed]      two computer golfers play the Várzea course
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "golf/match.hpp"

using namespace golf;

static std::string readFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static int validate() {
    const ClubSet bag = ClubSet::standard();
    Conditions air;
    air.temperatureC = 25;
    std::printf("%-6s %8s %8s %8s %8s %8s %8s\n", "club", "dynloft", "ball", "launch", "spin", "carry", "total");
    for (const Club& c : bag.clubs) {
        if (c.putter) continue;
        Delivery d;
        d.clubSpeed = c.tourSpeedMph * kMph * c.speedCalibration;
        d.attackDeg = c.tourAttackDeg;
        d.dynamicLoftDeg = c.tourDynamicLoftDeg;
        const ImpactResult r = strike(c, BallModel::tour(), d);
        const ShotResult s = simulateCarry(toLaunch(r, {0, BallModel::tour().radius(), 0}, 0), BallModel::tour(), air, {});
        std::printf("%-6s %7.1f° %5.0fmph %7.1f° %6.0frpm %6.0fyd\n", c.code.c_str(), c.tourDynamicLoftDeg,
                    r.speed / kMph, r.vlaDeg, r.spinRpm, s.stats.carry / kYard);
    }
    return 0;
}

static int playRound(std::uint64_t seed) {
    Course c;
    const auto j = json::parse(readFile(std::string(GOLF_DATA_DIR) + "/courses/varzea.json"));
    if (!j.ok()) return 1;
    const std::string err = loadCourse(j.value, c);
    if (!err.empty()) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    buildCourse(c);
    MatchConfig cfg;
    cfg.seed = seed;
    PlayerConfig a, b;
    a.name = "Ana (tour)";
    a.golfer = "tour";
    a.ai = true;
    b.name = "Bruno (14)";
    b.golfer = "mid";
    b.ai = true;
    b.handicapIndex = 14;
    cfg.players = {a, b};
    Match m(c, cfg);
    int lastHole = -1;
    while (!m.finished()) {
        if (m.holeIndex() != lastHole) {
            lastHole = m.holeIndex();
            const HoleDef& h = m.hole(lastHole);
            std::printf("\nHole %d \"%s\", par %d\n", h.number, h.name.c_str(), h.par);
        }
        const int p = m.turn();
        const Plan plan = m.plan();
        const auto r = m.play(plan.input);
        std::printf("  %-11s %-3s %6.1f m -> %6.1f m  %-10s %s\n", cfg.players[static_cast<size_t>(p)].name.c_str(),
                    plan.input.club.c_str(), r.record.distanceToPinBefore, r.record.distanceToPinAfter,
                    c.surfaces[r.record.toSurface].name.c_str(), r.record.ruling.note.c_str());
    }
    std::printf("\n");
    for (int p = 0; p < 2; ++p)
        std::printf("%-11s %d (%+d)\n", cfg.players[static_cast<size_t>(p)].name.c_str(), m.totalGross(p), m.totalToPar(p));
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && !std::strcmp(argv[1], "validate")) return validate();
    if (argc > 1 && !std::strcmp(argv[1], "round")) return playRound(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1);
    std::printf("usage: golfdemo validate | round [seed]\n");
    return 0;
}
