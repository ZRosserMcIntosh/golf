// Golf Dynamics — course build: surfaces where the design puts them, smooth greens, deterministic.
// SPDX-License-Identifier: MIT
#include <cstdio>

#include "golf/course.hpp"
#include "golf_test.hpp"
#include "test_util.hpp"

using namespace golf;

namespace {
const Course& varzea() {
    static Course c = [] {
        Course k;
        const std::string err = loadCourse(golftest::loadJson("courses/varzea.json"), k);
        if (!err.empty()) std::fprintf(stderr, "%s\n", err.c_str());
        buildCourse(k);
        return k;
    }();
    return c;
}
}  // namespace

TEST_CASE("course: loads and builds the Várzea course") {
    const Course& c = varzea();
    CHECK(c.holes.size() == 9);
    int par = 0;
    for (const auto& h : c.holes) par += h.par;
    CHECK(par == 36);
    std::printf("  %zu trees, height grid %d x %d, surface grid %d x %d, altitude %.0f m\n", c.trees.size(),
                c.terrain.grid.nx, c.terrain.grid.nz, c.terrain.surfaces.nx, c.terrain.surfaces.nz,
                c.conditions.altitudeM);
    CHECK(c.trees.size() > 300);
}

TEST_CASE("course: every feature has its surface") {
    const Course& c = varzea();
    for (const auto& h : c.holes) {
        for (const auto& t : h.tees) CHECK(c.terrain.sample(t.pos.x, t.pos.z).surface == c.idTee);
        CHECK(c.terrain.sample(h.green.shape.x, h.green.shape.z).surface == c.idGreen);
        for (const auto& p : h.pins) {
            const GroundSample g = c.terrain.sample(p.x, p.z);
            CHECK(g.surface == c.idGreen);
            CHECK(g.normal.y > 0.99965);  // holes are cut where the green is gentle (< 3 %)
        }
        for (const auto& b : h.bunkers) CHECK(c.terrain.sample(b.lobes[0].x, b.lobes[0].z).surface == c.idBunker);
        for (const auto& l : h.lakes) CHECK(c.terrain.sample(l.lobes[0].x, l.lobes[0].z).surface == c.idWater);
        if (!h.fairway.empty()) {
            const auto& f = h.fairway[h.fairway.size() / 2];
            CHECK(c.terrain.sample(f.x, f.z).surface == c.idFairway);
        }
        CHECK(c.inBounds(h.line.front().x, h.line.front().z));
        CHECK(c.inBounds(h.line.back().x, h.line.back().z));
    }
}

TEST_CASE("course: no trees on the playing surfaces") {
    const Course& c = varzea();
    int bad = 0;
    for (const auto& t : c.trees) {
        const int s = c.terrain.sample(t.x, t.z).surface;
        if (s == c.idFairway || s == c.idGreen || s == c.idTee || s == c.idBunker || s == c.idWater) ++bad;
    }
    CHECK(bad == 0);
}

TEST_CASE("course: the build is deterministic") {
    Course a, b;
    loadCourse(golftest::loadJson("courses/varzea.json"), a);
    loadCourse(golftest::loadJson("courses/varzea.json"), b);
    buildCourse(a);
    buildCourse(b);
    CHECK(a.terrain.grid.h == b.terrain.grid.h);
    CHECK(a.terrain.surfaces.s == b.terrain.surfaces.s);
    CHECK(a.trees.size() == b.trees.size());
}

TEST_CASE("course: real ground: the clubhouse hill stands above the floodplain") {
    const Course& c = varzea();
    const auto& h1 = c.holes[0];
    const double drop = h1.tees[1].pos.y - h1.green.level;
    std::printf("  hole 1 plays %.1f m downhill\n", drop);
    CHECK(drop > 15);
}
