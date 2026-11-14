// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/course.hpp"

#include <algorithm>
#include <cmath>

#include "golf/rng.hpp"

namespace golf {

// ---------------------------------------------------------------------------
// Shapes

double Blob::sdf(double px, double pz) const {
    const double dx = px - x, dz = pz - z;
    const double big = rx > rz ? rx : rz;
    const double dc2 = dx * dx + dz * dz;
    // Far away the exact shape does not matter: a cheap bound.
    if (dc2 > 9 * big * big) return std::sqrt(dc2) - big;
    const double c = std::cos(-rot), s = std::sin(-rot);
    const double u = dx * c - dz * s, v = dx * s + dz * c;
    const double a = std::pow(std::abs(u) / rx, p) + std::pow(std::abs(v) / rz, p);
    const double rho = std::pow(a, 1.0 / p);
    return (rho - 1.0) * std::min(rx, rz);
}

double Bunker::sdf(double x, double z) const {
    double d = 1e9;
    for (const auto& b : lobes) d = std::min(d, b.sdf(x, z));
    return d;
}

double Lake::sdf(double x, double z) const {
    double d = 1e9;
    for (const auto& b : lobes) d = std::min(d, b.sdf(x, z));
    return d;
}

double GreenDef::contour(double x, double z) const {
    double h = tiltX * (x - shape.x) + tiltZ * (z - shape.z);
    for (const auto& b : bumps) {
        const double d2 = (x - b.x) * (x - b.x) + (z - b.z) * (z - b.z);
        h += b.h * std::exp(-d2 / (b.r * b.r));
    }
    for (const auto& t : tiers) {
        const double s = (x - t.x) * t.nx + (z - t.z) * t.nz;
        h += t.h * smoothstep(-t.w / 2, t.w / 2, s);
    }
    return h;
}

namespace {

double segDist(double px, double pz, double ax, double az, double bx, double bz, double* tOut) {
    const double vx = bx - ax, vz = bz - az;
    const double l2 = vx * vx + vz * vz;
    double t = l2 > 0 ? ((px - ax) * vx + (pz - az) * vz) / l2 : 0;
    t = clamp(t, 0.0, 1.0);
    if (tOut) *tOut = t;
    const double dx = px - (ax + t * vx), dz = pz - (az + t * vz);
    return std::sqrt(dx * dx + dz * dz);
}

double rectSdf(const TeeBox& t, double x, double z) {
    const double c = std::cos(-t.heading), s = std::sin(-t.heading);
    const double dx = x - t.pos.x, dz = z - t.pos.z;
    const double u = std::abs(dx * c - dz * s) - t.halfL, v = std::abs(dx * s + dz * c) - t.halfW;
    const double ou = std::max(u, 0.0), ov = std::max(v, 0.0);
    return std::sqrt(ou * ou + ov * ov) + std::min(std::max(u, v), 0.0);
}

// Smooth value noise (deterministic from the seed), fractal sum.
double hash2(std::int64_t i, std::int64_t j, std::uint64_t seed) {
    std::uint64_t h = Rng::mix(seed, static_cast<std::uint64_t>(i) * 0x9E3779B1ull ^ (static_cast<std::uint64_t>(j) << 32));
    return static_cast<double>(h >> 11) * 0x1.0p-53 * 2.0 - 1.0;
}
double valueNoise(double x, double z, std::uint64_t seed) {
    const double fx = std::floor(x), fz = std::floor(z);
    const auto i = static_cast<std::int64_t>(fx), j = static_cast<std::int64_t>(fz);
    const double tx = x - fx, tz = z - fz;
    const double sx = tx * tx * (3 - 2 * tx), sz = tz * tz * (3 - 2 * tz);
    const double a = hash2(i, j, seed), b = hash2(i + 1, j, seed), c = hash2(i, j + 1, seed), d = hash2(i + 1, j + 1, seed);
    return lerp(lerp(a, b, sx), lerp(c, d, sx), sz);
}
double fbm(double x, double z, std::uint64_t seed) {
    double sum = 0, amp = 1, norm = 0;
    for (int o = 0; o < 4; ++o) {
        sum += amp * valueNoise(x, z, seed + static_cast<std::uint64_t>(o) * 101);
        norm += amp;
        amp *= 0.5;
        x *= 2.03;
        z *= 2.03;
    }
    return sum / norm;
}

bool pointInPolygon(const std::vector<Vec3>& poly, double x, double z) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const Vec3& a = poly[i];
        const Vec3& b = poly[j];
        if ((a.z > z) != (b.z > z) && x < (b.x - a.x) * (z - a.z) / (b.z - a.z) + a.x) in = !in;
    }
    return in;
}

double num(const json::Value& v, const char* k, double def) {
    const json::Value* f = v.find(k);
    return f && f->isNumber() ? f->asNumber() : def;
}
std::string str(const json::Value& v, const char* k, const std::string& def = "") {
    const json::Value* f = v.find(k);
    return f && f->isString() ? f->asString() : def;
}
std::vector<Vec3> points(const json::Value* arr) {
    std::vector<Vec3> out;
    if (!arr || !arr->isArray()) return out;
    for (const auto& p : arr->items())
        if (p.isArray() && p.items().size() >= 2) out.push_back({p.items()[0].asNumber(), 0, p.items()[1].asNumber()});
    return out;
}
Blob blob(const json::Value& v) {
    Blob b;
    b.x = num(v, "x", 0);
    b.z = num(v, "z", 0);
    b.rx = num(v, "rx", 5);
    b.rz = num(v, "rz", b.rx);
    b.rot = num(v, "rot", 0) * kDeg;
    b.p = num(v, "p", 2.2);
    return b;
}
std::vector<Blob> lobes(const json::Value& v) {
    std::vector<Blob> out;
    const json::Value* l = v.find("lobes");
    if (l && l->isArray())
        for (const auto& b : l->items()) out.push_back(blob(b));
    else
        out.push_back(blob(v));
    return out;
}

}  // namespace

double HoleDef::fairwaySdf(double x, double z) const {
    double d = 1e9;
    for (size_t i = 1; i < fairway.size(); ++i) {
        const auto& a = fairway[i - 1];
        const auto& b = fairway[i];
        double t = 0;
        const double dist = segDist(x, z, a.x, a.z, b.x, b.z, &t);
        d = std::min(d, dist - lerp(a.halfWidth, b.halfWidth, t));
    }
    return d;
}

double HoleDef::corridorDistance(double x, double z) const {
    double d = 1e9;
    for (size_t i = 1; i < line.size(); ++i) d = std::min(d, segDist(x, z, line[i - 1].x, line[i - 1].z, line[i].x, line[i].z, nullptr));
    return d;
}

double Course::baseHeight(double x, double z) const {
    const TerrainDef& t = terrainDef;
    double h = t.base + t.gradeX * x + t.gradeZ * z;
    if (!t.grid.h.empty()) h += t.grid.sample(x, z);
    // The course's own undulation fades out toward its edges, so it meets the
    // ground around it (the host world's) without a step.
    const double edge = std::min({x - x0, x1 - x, z - z0, z1 - z});
    if (t.noiseAmp > 0) h += t.noiseAmp * smoothstep(0.0, 60.0, edge) * fbm(x / t.noiseScale, z / t.noiseScale, t.seed);
    for (const auto& m : t.mounds) {
        const double d2 = (x - m.x) * (x - m.x) + (z - m.z) * (z - m.z);
        h += m.h * std::exp(-d2 / (m.r * m.r));
    }
    return h;
}

bool Course::inBounds(double x, double z) const {
    if (boundary.size() >= 3) return pointInPolygon(boundary, x, z);
    return x > x0 + 4 && x < x1 - 4 && z > z0 + 4 && z < z1 - 4;
}

const Lake* Course::lakeAt(double x, double z, const HoleDef** hole) const {
    for (const auto& h : holes)
        for (const auto& l : h.lakes)
            if (l.sdf(x, z) < 0) {
                if (hole) *hole = &h;
                return &l;
            }
    return nullptr;
}

World Course::world(const Cup* cup) const {
    World w;
    w.terrain = &terrain;
    w.surfaces = &surfaces;
    w.trees = &trees;
    w.treeIndex = &treeIndex;
    w.cup = cup;
    // The simulation runs a little beyond the course (balls fly out of bounds).
    w.minX = x0 - 150;
    w.maxX = x1 + 150;
    w.minZ = z0 - 150;
    w.maxZ = z1 + 150;
    return w;
}

// ---------------------------------------------------------------------------
// Loading

std::string loadCourse(const json::Value& root, Course& c) {
    if (!root.isObject()) return "course: expected an object";
    c.id = str(root, "id", "course");
    c.name = str(root, "name", c.id);
    c.description = str(root, "description");
    c.location = str(root, "location");
    c.attribution = str(root, "attribution");
    if (const json::Value* site = root.find("site")) {
        c.siteEast = num(*site, "east", 0);
        c.siteNorth = num(*site, "north", 0);
    }
    if (const json::Value* cd = root.find("conditions")) {
        c.conditions.altitudeM = num(*cd, "altitude_m", 0);
        c.conditions.temperatureC = num(*cd, "temperature_c", 20);
        c.conditions.relativeHumidity = num(*cd, "humidity", 0.6);
    }
    if (const json::Value* w = root.find("wind")) {
        c.wind.speed10 = num(*w, "speed", 3);
        c.wind.towardHeading = num(*w, "toward_deg", 0) * kDeg;
        c.wind.roughnessM = num(*w, "roughness_m", 0.1);
        c.wind.gustIntensity = num(*w, "gust_intensity", 0.15);
        c.wind.gustTimeScaleS = num(*w, "gust_time_s", 2.0);
    }
    c.greenStimpFt = num(root, "stimp_ft", 10.5);
    if (const json::Value* b = root.find("bounds"); b && b->isArray() && b->items().size() == 4) {
        c.x0 = b->items()[0].asNumber();
        c.z0 = b->items()[1].asNumber();
        c.x1 = b->items()[2].asNumber();
        c.z1 = b->items()[3].asNumber();
    }
    c.cell = num(root, "cell", 1.0);
    c.surfaceCell = num(root, "surface_cell", 0.5);
    c.boundary = points(root.find("boundary"));
    if (const json::Value* t = root.find("terrain")) {
        c.terrainDef.base = num(*t, "base", 0);
        c.terrainDef.gradeX = num(*t, "grade_x", 0);
        c.terrainDef.gradeZ = num(*t, "grade_z", 0);
        c.terrainDef.noiseAmp = num(*t, "noise_amp", 1.5);
        c.terrainDef.noiseScale = num(*t, "noise_scale", 90);
        c.terrainDef.seed = static_cast<std::uint64_t>(num(*t, "seed", 7));
        if (const json::Value* gr = t->find("grid"); gr && gr->isObject()) {
            HeightGrid& g = c.terrainDef.grid;
            g.x0 = num(*gr, "x0", 0);
            g.z0 = num(*gr, "z0", 0);
            g.cell = num(*gr, "cell", 25);
            g.nx = static_cast<int>(num(*gr, "nx", 0));
            g.nz = static_cast<int>(num(*gr, "nz", 0));
            const double scale = num(*gr, "scale", 1.0), offset = num(*gr, "offset", 0.0);
            if (const json::Value* hv = gr->find("h"); hv && hv->isArray())
                for (const auto& v : hv->items()) g.h.push_back(static_cast<float>(v.asNumber() * scale + offset));
            if (g.nx < 2 || g.nz < 2 || g.h.size() != static_cast<size_t>(g.nx) * static_cast<size_t>(g.nz))
                return "course: terrain grid size does not match nx * nz";
        }
        if (const json::Value* m = t->find("mounds"); m && m->isArray())
            for (const auto& v : m->items())
                c.terrainDef.mounds.push_back({num(v, "x", 0), num(v, "z", 0), num(v, "r", 20), num(v, "h", 1)});
    }
    if (const json::Value* t = root.find("trees")) {
        c.treeDef.seed = static_cast<std::uint64_t>(num(*t, "seed", 11));
        c.treeDef.spacing = num(*t, "spacing", 7);
        c.treeDef.clearance = num(*t, "clearance", 6);
        c.treeDef.corridorExtra = num(*t, "corridor_extra", 10);
        if (const json::Value* f = t->find("fixed"); f && f->isArray())
            for (const auto& v : f->items()) {
                Tree tr;
                tr.x = num(v, "x", 0);
                tr.z = num(v, "z", 0);
                tr.kind = static_cast<int>(num(v, "kind", 0));
                const double hgt = num(v, "height", 12);
                tr.trunkHeight = hgt * 0.45;
                tr.canopyY = hgt * 0.62;
                tr.canopyRy = hgt * 0.38;
                tr.canopyRx = num(v, "spread", hgt * 0.35);
                tr.trunkRadius = num(v, "trunk", 0.25);
                c.treeDef.fixed.push_back(tr);
            }
    }
    if (const json::Value* p = root.find("paths"); p && p->isArray())
        for (const auto& v : p->items()) c.paths.push_back({points(v.find("points")), num(v, "half_width", 1.25)});
    const json::Value* holes = root.find("holes");
    if (!holes || !holes->isArray() || holes->items().empty()) return "course: no holes";
    for (const auto& hv : holes->items()) {
        HoleDef h;
        h.number = static_cast<int>(num(hv, "number", static_cast<double>(c.holes.size() + 1)));
        h.par = static_cast<int>(num(hv, "par", 4));
        h.strokeIndex = static_cast<int>(num(hv, "stroke_index", h.number));
        h.name = str(hv, "name");
        h.roughWidth = num(hv, "rough_width", 14);
        h.line = points(hv.find("line"));
        if (h.line.size() < 2) return "course: hole " + std::to_string(h.number) + ": line needs two points";
        if (const json::Value* ts = hv.find("tees"); ts && ts->isArray())
            for (const auto& tv : ts->items()) {
                TeeBox t;
                t.name = str(tv, "name", "white");
                t.pos = {num(tv, "x", h.line[0].x), 0, num(tv, "z", h.line[0].z)};
                const Vec3 aim = h.line[1] - t.pos;
                t.heading = tv.find("heading_deg") ? num(tv, "heading_deg", 0) * kDeg : std::atan2(aim.z, aim.x);
                t.halfW = num(tv, "half_width", 4);
                t.halfL = num(tv, "half_length", 5);
                h.tees.push_back(t);
            }
        if (h.tees.empty()) {
            TeeBox t;
            t.pos = h.line[0];
            const Vec3 aim = h.line[1] - h.line[0];
            t.heading = std::atan2(aim.z, aim.x);
            h.tees.push_back(t);
        }
        h.pins = points(hv.find("pins"));
        if (const json::Value* f = hv.find("fairway"); f && f->isArray())
            for (const auto& p : f->items())
                if (p.isArray() && p.items().size() >= 3)
                    h.fairway.push_back({p.items()[0].asNumber(), p.items()[1].asNumber(), p.items()[2].asNumber()});
        if (const json::Value* g = hv.find("green")) {
            h.green.shape = blob(*g);
            h.green.raise = num(*g, "raise", 0.3);
            h.green.tiltX = num(*g, "tilt_x", 0);
            h.green.tiltZ = num(*g, "tilt_z", 0);
            if (const json::Value* b = g->find("bumps"); b && b->isArray())
                for (const auto& v : b->items())
                    h.green.bumps.push_back({num(v, "x", 0), num(v, "z", 0), num(v, "r", 3), num(v, "h", 0.1)});
            if (const json::Value* b = g->find("tiers"); b && b->isArray())
                for (const auto& v : b->items()) {
                    const double a = num(v, "dir_deg", 0) * kDeg;
                    h.green.tiers.push_back({num(v, "x", 0), num(v, "z", 0), std::cos(a), std::sin(a), num(v, "h", 0.3), num(v, "w", 2.5)});
                }
        } else {
            return "course: hole " + std::to_string(h.number) + ": no green";
        }
        if (h.pins.empty()) h.pins.push_back({h.green.shape.x, 0, h.green.shape.z});
        if (const json::Value* b = hv.find("bunkers"); b && b->isArray())
            for (const auto& v : b->items()) {
                Bunker bk;
                bk.lobes = lobes(v);
                bk.depth = num(v, "depth", 0.8);
                h.bunkers.push_back(bk);
            }
        if (const json::Value* b = hv.find("water"); b && b->isArray())
            for (const auto& v : b->items()) {
                Lake lk;
                lk.lobes = lobes(v);
                lk.level = num(v, "level", 0);
                lk.red = str(v, "colour", "red") != "yellow";
                h.lakes.push_back(lk);
            }
        c.holes.push_back(std::move(h));
    }
    if (const json::Value* s = root.find("surfaces")) {
        const std::string err = c.surfaces.load(*s);
        if (!err.empty()) return err;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Building

void buildCourse(Course& c) {
    SurfaceTable& st = c.surfaces;
    c.idFairway = std::max(0, st.find("fairway"));
    c.idRough = std::max(0, st.find("rough"));
    c.idDeepRough = std::max(0, st.find("deep_rough"));
    c.idNative = std::max(0, st.find("native"));
    c.idGreen = std::max(0, st.find("green"));
    c.idFringe = std::max(0, st.find("fringe"));
    c.idTee = std::max(0, st.find("tee"));
    c.idBunker = std::max(0, st.find("bunker"));
    c.idWater = std::max(0, st.find("water"));
    c.idPath = std::max(0, st.find("path"));
    st.at(c.idGreen).setStimp(c.greenStimpFt);

    // Bounding boxes, and levels of tees, greens and lakes from the undisturbed ground.
    for (auto& h : c.holes) {
        h.bx0 = h.bz0 = 1e18;
        h.bx1 = h.bz1 = -1e18;
        auto grow = [&](double x, double z, double r) {
            h.bx0 = std::min(h.bx0, x - r);
            h.bx1 = std::max(h.bx1, x + r);
            h.bz0 = std::min(h.bz0, z - r);
            h.bz1 = std::max(h.bz1, z + r);
        };
        for (const auto& p : h.line) grow(p.x, p.z, 0);
        for (const auto& t : h.tees) grow(t.pos.x, t.pos.z, t.halfL + t.halfW);
        grow(h.green.shape.x, h.green.shape.z, std::max(h.green.shape.rx, h.green.shape.rz));
        for (const auto& b : h.bunkers)
            for (const auto& l : b.lobes) grow(l.x, l.z, std::max(l.rx, l.rz));
        for (const auto& lk : h.lakes)
            for (const auto& l : lk.lobes) grow(l.x, l.z, std::max(l.rx, l.rz));
        for (auto& t : h.tees) t.pos.y = c.baseHeight(t.pos.x, t.pos.z) + t.raise;
        h.green.level = c.baseHeight(h.green.shape.x, h.green.shape.z) + h.green.raise;
        for (auto& l : h.lakes) {
            if (l.level == 0) {
                double lo = 1e9;
                for (const auto& b : l.lobes) lo = std::min(lo, c.baseHeight(b.x, b.z));
                l.level = lo - 1.0;
            }
        }
    }

    // Heights.
    HeightGrid& g = c.terrain.grid;
    g.cell = c.cell;
    g.x0 = c.x0;
    g.z0 = c.z0;
    g.nx = static_cast<int>(std::ceil((c.x1 - c.x0) / c.cell)) + 1;
    g.nz = static_cast<int>(std::ceil((c.z1 - c.z0) / c.cell)) + 1;
    g.h.assign(static_cast<size_t>(g.nx) * static_cast<size_t>(g.nz), 0.0f);
    for (int j = 0; j < g.nz; ++j) {
        const double z = g.z0 + j * g.cell;
        for (int i = 0; i < g.nx; ++i) {
            const double x = g.x0 + i * g.cell;
            double h = c.baseHeight(x, z);
            for (const auto& hole : c.holes) {
                // Quick reject: far from everything on this hole.
                if (!hole.near(x, z, 12)) continue;
                for (const auto& t : hole.tees) {
                    const double d = rectSdf(t, x, z);
                    if (d < 5) h = lerp(h, t.pos.y, 1.0 - smoothstep(0.0, 5.0, d));
                }
                const double dg = hole.green.shape.sdf(x, z);
                if (dg < 7) {
                    const double target = hole.green.level + hole.green.contour(x, z);
                    h = lerp(h, target, 1.0 - smoothstep(0.0, 7.0, dg));
                }
                for (const auto& b : hole.bunkers) {
                    const double d = b.sdf(x, z);
                    if (d < 0) h -= b.depth * smoothstep(0.0, 1.6, -d);
                    else if (d < 1.5) h += 0.18 * (1.0 - d / 1.5);  // a lip
                }
                for (const auto& l : hole.lakes) {
                    const double d = l.sdf(x, z);
                    if (d < 0) h = l.level;
                    else if (d < 10) h = std::min(h, l.level + 0.15 + d * 0.22);
                }
            }
            g.h[static_cast<size_t>(j) * static_cast<size_t>(g.nx) + static_cast<size_t>(i)] = static_cast<float>(h);
        }
    }

    // Surfaces.
    SurfaceGrid& s = c.terrain.surfaces;
    s.cell = c.surfaceCell;
    s.x0 = c.x0;
    s.z0 = c.z0;
    s.nx = static_cast<int>(std::ceil((c.x1 - c.x0) / s.cell));
    s.nz = static_cast<int>(std::ceil((c.z1 - c.z0) / s.cell));
    s.s.assign(static_cast<size_t>(s.nx) * static_cast<size_t>(s.nz), static_cast<std::uint8_t>(c.idNative));
    c.terrain.outsideSurface = c.idNative;
    const std::uint64_t edgeSeed = Rng::mix(c.terrainDef.seed, 0xED6E);
    for (int j = 0; j < s.nz; ++j) {
        const double z = s.z0 + (j + 0.5) * s.cell;
        for (int i = 0; i < s.nx; ++i) {
            const double x = s.x0 + (i + 0.5) * s.cell;
            // A little wobble on the mown edges.
            const double wob = 1.2 * valueNoise(x / 9.0, z / 9.0, edgeSeed);
            int id = c.idNative;
            for (const auto& hole : c.holes) {
                if (!hole.near(x, z, 70)) continue;
                const double cd = hole.corridorDistance(x, z);
                // Rough: within the corridor; deep rough beyond.
                const double fsdf = hole.fairway.empty() ? 1e9 : hole.fairwaySdf(x, z);
                const double dg = hole.green.shape.sdf(x, z);
                double teeD = 1e9;
                for (const auto& t : hole.tees) teeD = std::min(teeD, rectSdf(t, x, z));
                const double play = std::min({fsdf - hole.roughWidth, dg - 10.0, teeD - 8.0, cd - 16.0});
                if (play < 0 && id != c.idFairway && id != c.idGreen && id != c.idFringe && id != c.idTee) id = c.idRough;
                else if (play < 10 + wob && id == c.idNative) id = c.idDeepRough;
                if (fsdf + wob * 0.6 < 0) id = c.idFairway;
                if (teeD < 0) id = c.idTee;
                if (dg < 1.4 + wob * 0.3) id = c.idFringe;
                if (dg < 0) id = c.idGreen;
            }
            for (const auto& p : c.paths)
                for (size_t k = 1; k < p.points.size(); ++k) {
                    const Vec3& a = p.points[k - 1];
                    const Vec3& b = p.points[k];
                    if (x < std::min(a.x, b.x) - 3 || x > std::max(a.x, b.x) + 3 || z < std::min(a.z, b.z) - 3 || z > std::max(a.z, b.z) + 3) continue;
                    if (segDist(x, z, a.x, a.z, b.x, b.z, nullptr) < p.halfWidth) id = c.idPath;
                }
            for (const auto& hole : c.holes) {
                if (!hole.near(x, z, 1)) continue;
                for (const auto& b : hole.bunkers)
                    if (b.sdf(x, z) < 0) id = c.idBunker;
                for (const auto& l : hole.lakes)
                    if (l.sdf(x, z) < 0) id = c.idWater;
            }
            s.s[static_cast<size_t>(j) * static_cast<size_t>(s.nx) + static_cast<size_t>(i)] = static_cast<std::uint8_t>(id);
        }
    }

    c.treeIndex = TreeIndex{};
    // Pins sit on the green, where it is gentle: a greenkeeper cuts the hole
    // where the slope is under about 3 % (the usual guidance for green speeds of
    // 10-11 ft), so a pin on a steeper spot moves to the gentlest within 4 m.
    for (auto& h : c.holes)
        for (auto& p : h.pins) {
            auto slope = [&](double x, double z) {
                const GroundSample gs = c.terrain.sample(x, z);
                return gs.surface == c.idGreen && h.green.shape.sdf(x, z) < -1.5 ? std::sqrt(1.0 / (gs.normal.y * gs.normal.y) - 1.0) : 1e9;
            };
            double best = slope(p.x, p.z);
            if (best > 0.025) {
                const double px = p.x, pz = p.z;
                for (double r = 0.5; r <= 4.0; r += 0.5)
                    for (int k = 0; k < 16; ++k) {
                        const double x = px + r * std::cos(k * kPi / 8), z = pz + r * std::sin(k * kPi / 8);
                        const double sl = slope(x, z);
                        if (sl < best - 1e-9) {
                            best = sl;
                            p.x = x;
                            p.z = z;
                        }
                    }
            }
            p.y = c.terrain.height(p.x, p.z);
        }

    // Trees: a jittered grid over the wooded ground, thinned by a clumping
    // noise, kept clear of every played surface.
    c.trees = c.treeDef.fixed;
    for (auto& t : c.trees) t.baseY = c.terrain.height(t.x, t.z);
    Rng rng(c.treeDef.seed);
    const double sp = c.treeDef.spacing;
    for (double z = c.z0 + sp / 2; z < c.z1; z += sp) {
        for (double x = c.x0 + sp / 2; x < c.x1; x += sp) {
            const double tx = x + rng.uniform(-0.45, 0.45) * sp, tz = z + rng.uniform(-0.45, 0.45) * sp;
            const double clump = valueNoise(tx / 45.0, tz / 45.0, c.treeDef.seed);
            const double roll = rng.uniform();
            const int kindRoll = static_cast<int>(rng.uniform() * 100);
            const double sizeRoll = rng.uniform();
            if (clump < -0.15 || roll > 0.55 + 0.45 * clump) continue;
            if (!c.inBounds(tx, tz) && clump < 0.2) continue;
            bool ok = true;
            for (const auto& hole : c.holes) {
                if (!hole.near(tx, tz, 60)) continue;
                if (hole.corridorDistance(tx, tz) < 26 + c.treeDef.corridorExtra) { ok = false; break; }
                if (!hole.fairway.empty() && hole.fairwaySdf(tx, tz) < hole.roughWidth + c.treeDef.clearance) { ok = false; break; }
                if (hole.green.shape.sdf(tx, tz) < 14) { ok = false; break; }
                for (const auto& t : hole.tees)
                    if (rectSdf(t, tx, tz) < 8) { ok = false; break; }
                for (const auto& l : hole.lakes)
                    if (l.sdf(tx, tz) < 3) { ok = false; break; }
                if (!ok) break;
            }
            for (const auto& p : c.paths)
                for (size_t k = 1; k < p.points.size() && ok; ++k)
                    if (segDist(tx, tz, p.points[k - 1].x, p.points[k - 1].z, p.points[k].x, p.points[k].z, nullptr) < 4) ok = false;
            if (!ok) continue;
            Tree t;
            t.x = tx;
            t.z = tz;
            t.baseY = c.terrain.height(tx, tz);
            // São Paulo species mix (rendering kinds): 0 sibipiruna (broadleaf),
            // 1 ipê-amarelo, 2 ipê-roxo, 3 jerivá palm, 4 eucalyptus, 5 araucaria.
            t.kind = kindRoll < 48 ? 0 : kindRoll < 54 ? 1 : kindRoll < 59 ? 2 : kindRoll < 71 ? 3 : kindRoll < 92 ? 4 : 5;
            const double hgt = (t.kind == 4 ? 20.0 : t.kind == 3 ? 11.0 : t.kind == 5 ? 17.0 : 12.0) * (0.75 + 0.5 * sizeRoll);
            t.trunkRadius = t.kind == 3 ? 0.15 : 0.22 + 0.1 * sizeRoll;
            switch (t.kind) {
                case 3:  // palm: tall bare trunk, small crown
                    t.trunkHeight = hgt * 0.85;
                    t.canopyY = hgt * 0.9;
                    t.canopyRx = 3.0;
                    t.canopyRy = 1.6;
                    t.density = 0.25;
                    break;
                case 4:  // eucalyptus: tall and narrow
                    t.trunkHeight = hgt * 0.5;
                    t.canopyY = hgt * 0.7;
                    t.canopyRx = hgt * 0.17;
                    t.canopyRy = hgt * 0.32;
                    t.density = 0.25;
                    break;
                case 5:  // araucaria: a high, flat crown
                    t.trunkHeight = hgt * 0.75;
                    t.canopyY = hgt * 0.86;
                    t.canopyRx = hgt * 0.32;
                    t.canopyRy = hgt * 0.16;
                    t.density = 0.4;
                    break;
                default:  // broad crowns
                    t.trunkHeight = hgt * 0.4;
                    t.canopyY = hgt * 0.62;
                    t.canopyRx = hgt * 0.42;
                    t.canopyRy = hgt * 0.36;
                    t.density = 0.4;
            }
            c.trees.push_back(t);
        }
    }
    c.treeIndex.build(c.trees, c.x0 - 200, c.z0 - 200, c.x1 + 200, c.z1 + 200, 16);
}

}  // namespace golf
