// Golf Dynamics — the C ABI for the browser (WebAssembly, wasi-sdk reactor).
// SPDX-License-Identifier: MIT
//
// Strings go in through gd_alloc'd buffers and come back through
// gd_result() / gd_result_length(): JSON both ways. Large arrays (the course
// rasters, the trees, a shot's path) are read in place from the module's
// memory through the pointer exports. One course and one match at a time.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "golf/loader.hpp"
#include "golf/match.hpp"

using namespace golf;

namespace {

constexpr int kAbiVersion = 1;

std::unique_ptr<Course> g_course;
std::unique_ptr<Match> g_match;
std::string g_result;
std::vector<float> g_path;   // t, x, y, z, phase per point
std::vector<float> g_trees;  // x, baseY, z, trunkR, trunkH, canopyY, canopyRx, canopyRy, kind
std::string g_error;

// JSON helpers (small; the core's writer is private to match.cpp).
std::string num(double v) {
    if (!std::isfinite(v)) return "0";
    char b[32];
    std::snprintf(b, sizeof b, "%.10g", v);
    return b;
}
std::string q(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (static_cast<unsigned char>(c) < 0x20) o += ' ';
        else o += c;
    }
    return o + "\"";
}
std::string vec(const Vec3& v) { return "[" + num(v.x) + "," + num(v.y) + "," + num(v.z) + "]"; }

int setResult(std::string s) {
    g_result = std::move(s);
    return static_cast<int>(g_result.size());
}
int fail(const std::string& msg) {
    g_error = msg;
    setResult("{\"error\":" + q(msg) + "}");
    return -1;
}

std::string courseInfo(const Course& c) {
    std::string o = "{";
    o += "\"id\":" + q(c.id) + ",\"name\":" + q(c.name) + ",\"description\":" + q(c.description);
    o += ",\"location\":" + q(c.location) + ",\"attribution\":" + q(c.attribution);
    o += ",\"site\":[" + num(c.siteEast) + "," + num(c.siteNorth) + "]";
    o += ",\"bounds\":[" + num(c.x0) + "," + num(c.z0) + "," + num(c.x1) + "," + num(c.z1) + "]";
    o += ",\"altitude\":" + num(c.conditions.altitudeM) + ",\"temperature\":" + num(c.conditions.temperatureC);
    o += ",\"humidity\":" + num(c.conditions.relativeHumidity) + ",\"stimp\":" + num(c.greenStimpFt);
    const HeightGrid& g = c.terrain.grid;
    o += ",\"height\":{\"nx\":" + std::to_string(g.nx) + ",\"nz\":" + std::to_string(g.nz) + ",\"x0\":" + num(g.x0) +
         ",\"z0\":" + num(g.z0) + ",\"cell\":" + num(g.cell) + "}";
    const SurfaceGrid& s = c.terrain.surfaces;
    o += ",\"surface\":{\"nx\":" + std::to_string(s.nx) + ",\"nz\":" + std::to_string(s.nz) + ",\"x0\":" + num(s.x0) +
         ",\"z0\":" + num(s.z0) + ",\"cell\":" + num(s.cell) + "}";
    o += ",\"surfaces\":[";
    for (int i = 0; i < c.surfaces.size(); ++i) {
        const Surface& sf = c.surfaces[i];
        if (i) o += ",";
        o += "{\"id\":" + q(sf.id) + ",\"name\":" + q(sf.name) + ",\"color\":" + q(sf.color) +
             ",\"water\":" + (sf.water ? "true" : "false") + ",\"puttable\":" + (sf.puttable ? "true" : "false") +
             ",\"stimp\":" + num(sf.stimpFeet()) + "}";
    }
    o += "],\"trees\":" + std::to_string(c.trees.size());
    o += ",\"boundary\":[";
    for (size_t i = 0; i < c.boundary.size(); ++i) o += (i ? "," : "") + std::string("[") + num(c.boundary[i].x) + "," + num(c.boundary[i].z) + "]";
    o += "],\"holes\":[";
    for (size_t h = 0; h < c.holes.size(); ++h) {
        const HoleDef& d = c.holes[h];
        if (h) o += ",";
        o += "{\"number\":" + std::to_string(d.number) + ",\"name\":" + q(d.name) + ",\"par\":" + std::to_string(d.par) +
             ",\"stroke_index\":" + std::to_string(d.strokeIndex);
        o += ",\"tees\":[";
        for (size_t t = 0; t < d.tees.size(); ++t)
            o += (t ? "," : "") + std::string("{\"name\":") + q(d.tees[t].name) + ",\"pos\":" + vec(d.tees[t].pos) +
                 ",\"heading\":" + num(d.tees[t].heading) + ",\"half_width\":" + num(d.tees[t].halfW) +
                 ",\"half_length\":" + num(d.tees[t].halfL) + "}";
        o += "],\"pins\":[";
        for (size_t p = 0; p < d.pins.size(); ++p) o += (p ? "," : "") + vec(d.pins[p]);
        o += "],\"line\":[";
        for (size_t p = 0; p < d.line.size(); ++p) o += (p ? "," : "") + std::string("[") + num(d.line[p].x) + "," + num(d.line[p].z) + "]";
        o += "],\"green\":{\"x\":" + num(d.green.shape.x) + ",\"z\":" + num(d.green.shape.z) + ",\"rx\":" + num(d.green.shape.rx) +
             ",\"rz\":" + num(d.green.shape.rz) + ",\"rot\":" + num(d.green.shape.rot) + ",\"level\":" + num(d.green.level) + "}";
        o += ",\"lakes\":[";
        for (size_t l = 0; l < d.lakes.size(); ++l) o += (l ? "," : "") + std::string("{\"level\":") + num(d.lakes[l].level) + "}";
        double len = 0;
        for (size_t p = 1; p < d.line.size(); ++p) len += std::hypot(d.line[p].x - d.line[p - 1].x, d.line[p].z - d.line[p - 1].z);
        o += "],\"length\":" + num(len) + "}";
    }
    o += "]}";
    return o;
}

void storePath(const ShotResult& r) {
    g_path.clear();
    g_path.reserve(r.path.size() * 5);
    for (const auto& p : r.path) {
        g_path.push_back(static_cast<float>(p.t));
        g_path.push_back(static_cast<float>(p.pos.x));
        g_path.push_back(static_cast<float>(p.pos.y));
        g_path.push_back(static_cast<float>(p.pos.z));
        g_path.push_back(static_cast<float>(static_cast<int>(p.phase)));
    }
}

std::string shotJson(const ShotResult& r) {
    std::string o = "{\"outcome\":" + q(outcomeName(r.outcome)) + ",\"final\":" + vec(r.final.pos) +
                    ",\"final_surface\":" + std::to_string(r.finalSurface);
    const ShotStats& s = r.stats;
    o += ",\"carry\":" + num(s.carry) + ",\"carry_along\":" + num(s.carryAlong) + ",\"carry_offline\":" + num(s.carryOffline) +
         ",\"total\":" + num(s.total) + ",\"total_along\":" + num(s.totalAlong) + ",\"total_offline\":" + num(s.totalOffline) +
         ",\"apex\":" + num(s.apex) + ",\"flight_time\":" + num(s.flightTime) + ",\"land_angle\":" + num(s.landingAngleDeg) +
         ",\"total_time\":" + num(s.totalTime) + ",\"bounces\":" + std::to_string(s.bounces);
    o += ",\"events\":[";
    for (size_t i = 0; i < r.events.size(); ++i) {
        const auto& e = r.events[i];
        o += (i ? "," : "") + std::string("{\"kind\":") + q(eventName(e.kind)) + ",\"t\":" + num(e.t) + ",\"pos\":" + vec(e.pos) +
             ",\"speed\":" + num(e.speed) + ",\"surface\":" + std::to_string(e.surface) + "}";
    }
    o += "],\"points\":" + std::to_string(r.path.size()) + "}";
    return o;
}

std::string recordJson(const ShotRecord& r, const Course& c) {
    std::string o = "{\"player\":" + std::to_string(r.player) + ",\"hole\":" + std::to_string(r.hole) +
                    ",\"stroke\":" + std::to_string(r.stroke) + ",\"input\":" + swingJson(r.input) +
                    ",\"from\":" + vec(r.from) + ",\"to\":" + vec(r.to) + ",\"from_surface\":" + q(c.surfaces[r.fromSurface].id) +
                    ",\"to_surface\":" + q(c.surfaces[r.toSurface].id);
    o += ",\"ruling\":{\"kind\":" + q(rulingName(r.ruling.kind)) + ",\"penalty\":" + std::to_string(r.ruling.penalty) +
         ",\"note\":" + q(r.ruling.note) + ",\"replay\":" + (r.ruling.replay ? "true" : "false") + "}";
    const ImpactResult& im = r.impact;
    const Delivery& d = r.delivery;
    o += ",\"launch\":{\"speed\":" + num(im.speed) + ",\"vla\":" + num(im.vlaDeg) + ",\"hla\":" + num(im.hlaDeg) +
         ",\"spin\":" + num(im.spinRpm) + ",\"axis\":" + num(im.spinAxisDeg) + ",\"smash\":" + num(im.smash) + "}";
    o += ",\"club\":{\"speed\":" + num(d.clubSpeed) + ",\"attack\":" + num(d.attackDeg) + ",\"path\":" + num(d.pathDeg) +
         ",\"face\":" + num(d.faceAngleDeg) + ",\"loft\":" + num(d.dynamicLoftDeg) + ",\"toe\":" + num(d.strikeToeM) +
         ",\"up\":" + num(d.strikeUpM) + "}";
    o += ",\"pin_before\":" + num(r.distanceToPinBefore) + ",\"pin_after\":" + num(r.distanceToPinAfter) +
         ",\"strokes_gained\":" + num(r.strokesGained) + "}";
    return o;
}

std::string planJson(const Plan& p) {
    return "{\"input\":" + swingJson(p.input) + ",\"target\":" + vec(p.target) + ",\"expected\":" + num(p.expected) +
           ",\"note\":" + q(p.note) + "}";
}

std::string str(const char* p, int n) { return p && n > 0 ? std::string(p, static_cast<size_t>(n)) : std::string(); }

}  // namespace

extern "C" {

int gd_abi_version() { return kAbiVersion; }
void* gd_alloc(int n) { return std::malloc(static_cast<size_t>(n > 0 ? n : 1)); }
void gd_free(void* p) { std::free(p); }
const char* gd_result() { return g_result.c_str(); }
int gd_result_length() { return static_cast<int>(g_result.size()); }

// --- Course -----------------------------------------------------------------

int gd_course_load(const char* text, int n) {
    const auto r = json::parse(str(text, n));
    if (!r.ok()) return fail("course JSON: " + r.error);
    auto c = std::make_unique<Course>();
    const std::string err = loadCourse(r.value, *c);
    if (!err.empty()) return fail(err);
    buildCourse(*c);
    g_match.reset();
    g_course = std::move(c);
    g_trees.clear();
    for (const auto& t : g_course->trees) {
        const float v[9] = {static_cast<float>(t.x), static_cast<float>(t.baseY), static_cast<float>(t.z),
                            static_cast<float>(t.trunkRadius), static_cast<float>(t.trunkHeight), static_cast<float>(t.canopyY),
                            static_cast<float>(t.canopyRx), static_cast<float>(t.canopyRy), static_cast<float>(t.kind)};
        g_trees.insert(g_trees.end(), v, v + 9);
    }
    return setResult(courseInfo(*g_course));
}

const float* gd_height_ptr() { return g_course ? g_course->terrain.grid.h.data() : nullptr; }
const std::uint8_t* gd_surface_ptr() { return g_course ? g_course->terrain.surfaces.s.data() : nullptr; }
const float* gd_trees_ptr() { return g_trees.data(); }
int gd_trees_count() { return static_cast<int>(g_trees.size() / 9); }

double gd_height(double x, double z) { return g_course ? g_course->terrain.height(x, z) : 0.0; }
int gd_surface_at(double x, double z) { return g_course ? g_course->terrain.sample(x, z).surface : 0; }
int gd_in_bounds(double x, double z) { return g_course && g_course->inBounds(x, z) ? 1 : 0; }

// Green speed for the next match (feet on the Stimpmeter).
int gd_set_stimp(double ft) {
    if (!g_course || !(ft > 4 && ft < 16)) return -1;
    g_course->greenStimpFt = ft;
    g_course->surfaces.at(g_course->idGreen).setStimp(ft);
    return 0;
}

// --- Match ------------------------------------------------------------------

int gd_match_new(const char* text, int n) {
    if (!g_course) return fail("no course loaded");
    const auto r = json::parse(str(text, n));
    if (!r.ok()) return fail("match JSON: " + r.error);
    MatchConfig cfg;
    const std::string err = parseMatchConfig(r.value, cfg);
    if (!err.empty()) return fail(err);
    if (cfg.stimpFt > 0) gd_set_stimp(cfg.stimpFt);
    g_match = std::make_unique<Match>(*g_course, cfg);
    return setResult(g_match->stateJson());
}

// Rebuild a match from gd_match_save output (configuration + shot inputs).
int gd_match_load(const char* text, int n) {
    if (!g_course) return fail("no course loaded");
    const auto r = json::parse(str(text, n));
    if (!r.ok()) return fail("saved match JSON: " + r.error);
    const json::Value* cv = r.value.find("config");
    if (!cv) return fail("saved match: no config");
    MatchConfig cfg;
    const std::string err = parseMatchConfig(*cv, cfg);
    if (!err.empty()) return fail(err);
    if (cfg.stimpFt > 0) gd_set_stimp(cfg.stimpFt);
    g_match = std::make_unique<Match>(*g_course, cfg);
    if (const json::Value* shots = r.value.find("shots"); shots && shots->isArray())
        for (const auto& s : shots->items()) {
            if (g_match->finished()) break;
            g_match->play(parseSwing(s));
        }
    return setResult(g_match->stateJson());
}

int gd_match_state() {
    if (!g_match) return fail("no match");
    return setResult(g_match->stateJson());
}

int gd_match_save() {
    if (!g_match) return fail("no match");
    return setResult(g_match->toJson());
}

int gd_match_play(const char* text, int n) {
    if (!g_match) return fail("no match");
    if (g_match->finished()) return fail("the round is over");
    const auto r = json::parse(str(text, n));
    if (!r.ok()) return fail("swing JSON: " + r.error);
    const Match::Played p = g_match->play(parseSwing(r.value));
    storePath(p.shot);
    return setResult("{\"record\":" + recordJson(p.record, *g_course) + ",\"shot\":" + shotJson(p.shot) +
                     ",\"hole_complete\":" + (p.holeComplete ? "true" : "false") + ",\"finished\":" +
                     (p.finished ? "true" : "false") + ",\"state\":" + g_match->stateJson() + "}");
}

int gd_match_plan() {
    if (!g_match || g_match->finished()) return fail("no shot to plan");
    return setResult(planJson(g_match->plan()));
}

// The shot without scatter or gusts: for the aim line and the caddie.
int gd_match_preview(const char* text, int n) {
    if (!g_match || g_match->finished()) return fail("no shot to preview");
    const auto r = json::parse(str(text, n));
    if (!r.ok()) return fail("swing JSON: " + r.error);
    const int p = g_match->turn();
    const PlayContext ctx = g_match->context(p);
    const GolferProfile g = findGolfer(g_match->config().players[static_cast<size_t>(p)].golfer);
    const ShotResult s = previewShot(ctx, parseSwing(r.value), g, true, true);
    storePath(s);
    return setResult(shotJson(s));
}

int gd_match_pickup() {
    if (!g_match) return fail("no match");
    g_match->pickUp();
    return setResult(g_match->stateJson());
}

const float* gd_path_ptr() { return g_path.data(); }
int gd_path_count() { return static_cast<int>(g_path.size() / 5); }

// --- Clubs ------------------------------------------------------------------

// The bag with each club's nominal full-swing carry for a golfer profile.
int gd_clubs(const char* golfer, int n) {
    const ClubSet bag = ClubSet::standard();
    const GolferProfile g = findGolfer(str(golfer, n));
    Conditions air = g_course ? g_course->conditions : Conditions{};
    std::string o = "{\"golfer\":" + q(g.name) + ",\"clubs\":[";
    for (size_t i = 0; i < bag.clubs.size(); ++i) {
        const Club& c = bag.clubs[i];
        const double carry = c.putter ? 0.0 : nominalCarry(c, g, BallModel::tour(), air);
        o += (i ? "," : "") + std::string("{\"code\":") + q(c.code) + ",\"name\":" + q(c.name) + ",\"loft\":" + num(c.loftDeg) +
             ",\"carry\":" + num(carry) + ",\"putter\":" + (c.putter ? "true" : "false") + "}";
    }
    o += "],\"golfers\":[";
    const auto all = standardGolfers();
    for (size_t i = 0; i < all.size(); ++i)
        o += (i ? "," : "") + std::string("{\"id\":") + q(all[i].id) + ",\"name\":" + q(all[i].name) + ",\"handicap\":" + num(all[i].handicap) + "}";
    o += "]}";
    return setResult(o);
}

// --- Launch monitor -----------------------------------------------------------

// A shot from launch data (GSPro Open Connect units: mph, degrees, rpm) on the
// current hole from the current player's ball, without changing the match.
int gd_launch_preview(double speedMph, double vla, double hla, double spinRpm, double axis, double heading) {
    if (!g_match || g_match->finished()) return fail("no match");
    const int p = g_match->turn();
    const PlayContext ctx = g_match->context(p);
    Launch l;
    l.position = ctx.ballPos;
    l.targetHeading = heading;
    l.speed = speedMph * kMph;
    l.vlaDeg = vla;
    l.hlaDeg = hla;
    l.spinRpm = spinRpm;
    l.spinAxisDeg = axis;
    const World w = g_course->world(&ctx.cup);
    const ShotResult s = simulateShot(l, BallModel::tour(), ctx.air, ctx.wind, w);
    storePath(s);
    return setResult(shotJson(s));
}

}  // extern "C"
