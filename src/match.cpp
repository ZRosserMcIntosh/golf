// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/match.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace golf {

namespace {

double horiz(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.z - b.z); }

// Minimal JSON writer.
struct W {
    std::string s;
    bool first = true;
    void raw(const std::string& t) { s += t; }
    void comma() {
        if (!first) s += ',';
        first = false;
    }
    void key(const char* k) {
        comma();
        s += '"';
        s += k;
        s += "\":";
    }
    static std::string num(double v) {
        if (!std::isfinite(v)) return "0";
        char b[32];
        std::snprintf(b, sizeof b, "%.17g", v);
        return b;
    }
    static std::string str(const std::string& v) {
        std::string o = "\"";
        for (char c : v) {
            if (c == '"' || c == '\\') { o += '\\'; o += c; }
            else if (static_cast<unsigned char>(c) < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += c;
        }
        return o + "\"";
    }
    void kv(const char* k, double v) { key(k); s += num(v); }
    void kv(const char* k, int v) { key(k); s += std::to_string(v); }
    void kv(const char* k, bool v) { key(k); s += v ? "true" : "false"; }
    void kv(const char* k, const std::string& v) { key(k); s += str(v); }
    void kv(const char* k, const char* v) { key(k); s += str(v); }
    void vec(const char* k, const Vec3& v) { key(k); s += "[" + num(v.x) + "," + num(v.y) + "," + num(v.z) + "]"; }
    void open(const char* k = nullptr) { if (k) key(k); else comma(); s += '{'; first = true; }
    void close() { s += '}'; first = false; }
    void openArr(const char* k = nullptr) { if (k) key(k); else comma(); s += '['; first = true; }
    void closeArr() { s += ']'; first = false; }
};

double jnum(const json::Value& v, const char* k, double d) {
    const json::Value* f = v.find(k);
    return f && f->isNumber() ? f->asNumber() : d;
}
bool jbool(const json::Value& v, const char* k, bool d) {
    const json::Value* f = v.find(k);
    return f && f->isBool() ? f->asBool() : d;
}
std::string jstr(const json::Value& v, const char* k, const std::string& d) {
    const json::Value* f = v.find(k);
    return f && f->isString() ? f->asString() : d;
}

const char* formatName(Format f) {
    return f == Format::Stableford ? "stableford" : f == Format::MatchPlay ? "match" : "stroke";
}

}  // namespace

std::string swingJson(const SwingInput& in) {
    W w;
    w.open();
    w.kv("club", in.club);
    w.kv("aim", in.aimHeading);
    w.kv("power", in.power);
    w.kv("face", in.faceDeg);
    w.kv("path", in.pathDeg);
    w.kv("loft", in.loftDeg);
    w.kv("attack", in.attackDeg);
    w.kv("toe", in.strikeToeMm);
    w.kv("up", in.strikeUpMm);
    if (in.exact) w.kv("exact", true);
    w.close();
    return w.s;
}

SwingInput parseSwing(const json::Value& v) {
    SwingInput in;
    in.club = jstr(v, "club", "I7");
    in.aimHeading = jnum(v, "aim", 0);
    in.power = jnum(v, "power", 1);
    in.faceDeg = jnum(v, "face", 0);
    in.pathDeg = jnum(v, "path", 0);
    in.loftDeg = jnum(v, "loft", 0);
    in.attackDeg = jnum(v, "attack", 0);
    in.strikeToeMm = jnum(v, "toe", 0);
    in.strikeUpMm = jnum(v, "up", 0);
    in.exact = jbool(v, "exact", false);
    return in;
}

std::string matchConfigJson(const MatchConfig& c) {
    W w;
    w.open();
    w.key("holes");
    w.raw("[");
    for (size_t i = 0; i < c.holes.size(); ++i) w.raw((i ? "," : "") + std::to_string(c.holes[i]));
    w.raw("]");
    w.openArr("players");
    for (const auto& p : c.players) {
        w.open();
        w.kv("name", p.name);
        w.kv("golfer", p.golfer);
        w.kv("tee", p.tee);
        w.kv("ai", p.ai);
        w.kv("left", p.leftHanded);
        w.kv("handicap", p.handicapIndex);
        w.close();
    }
    w.closeArr();
    w.kv("format", formatName(c.format));
    // The seed travels as a string: a 64-bit integer does not survive a JavaScript number.
    w.kv("seed", std::to_string(c.seed));
    w.kv("wind", c.windScale);
    w.kv("gusts", c.gusts);
    w.kv("stimp", c.stimpFt);
    w.kv("max_over_par", c.maxOverPar);
    w.kv("gimme", c.gimmeM);
    w.kv("e5", c.localRuleE5);
    w.kv("difficulty", c.difficulty);
    w.kv("flag_putts", c.flagstickForPutts);
    w.close();
    return w.s;
}

std::string parseMatchConfig(const json::Value& v, MatchConfig& c) {
    if (!v.isObject()) return "match: expected an object";
    c = MatchConfig{};
    if (const json::Value* h = v.find("holes"); h && h->isArray())
        for (const auto& x : h->items()) c.holes.push_back(static_cast<int>(x.asNumber()));
    if (const json::Value* p = v.find("players"); p && p->isArray())
        for (const auto& x : p->items()) {
            PlayerConfig pc;
            pc.name = jstr(x, "name", "Player");
            pc.golfer = jstr(x, "golfer", "scratch");
            pc.tee = jstr(x, "tee", "white");
            pc.ai = jbool(x, "ai", false);
            pc.leftHanded = jbool(x, "left", false);
            pc.handicapIndex = jnum(x, "handicap", 0);
            c.players.push_back(pc);
        }
    if (c.players.empty()) return "match: no players";
    if (c.players.size() > 8) return "match: at most 8 players";
    const std::string f = jstr(v, "format", "stroke");
    c.format = f == "stableford" ? Format::Stableford : f == "match" ? Format::MatchPlay : Format::Stroke;
    if (const json::Value* s = v.find("seed")) {
        if (s->isString()) c.seed = std::strtoull(s->asString().c_str(), nullptr, 10);
        else if (s->isNumber()) c.seed = static_cast<std::uint64_t>(s->asNumber());
    }
    c.windScale = jnum(v, "wind", 1);
    c.gusts = jbool(v, "gusts", true);
    c.stimpFt = jnum(v, "stimp", 0);
    c.maxOverPar = static_cast<int>(jnum(v, "max_over_par", 5));
    c.gimmeM = jnum(v, "gimme", 0);
    c.localRuleE5 = jbool(v, "e5", false);
    c.difficulty = jnum(v, "difficulty", 1);
    c.flagstickForPutts = jbool(v, "flag_putts", false);
    return "";
}

Match::Match(const Course& course, MatchConfig cfg, ClubSet clubs, BallModel ball)
    : course_(course), cfg_(std::move(cfg)), clubs_(std::move(clubs)), ball_(std::move(ball)) {
    if (cfg_.holes.empty())
        for (int i = 0; i < static_cast<int>(course_.holes.size()); ++i) cfg_.holes.push_back(i);
    cfg_.holes.erase(std::remove_if(cfg_.holes.begin(), cfg_.holes.end(),
                                    [&](int h) { return h < 0 || h >= static_cast<int>(course_.holes.size()); }),
                     cfg_.holes.end());
    if (cfg_.players.empty()) cfg_.players.push_back(PlayerConfig{});
    if (cfg_.format == Format::MatchPlay && cfg_.players.size() != 2) cfg_.format = Format::Stroke;
    for (const auto& p : cfg_.players) {
        golfers_.push_back(findGolfer(p.golfer));
        PlayerState s;
        s.gross.assign(cfg_.holes.size(), -1);
        players_.push_back(s);
    }
    for (int i = 0; i < playerCount(); ++i) order_.push_back(i);
    hole_ = 0;
    if (!cfg_.holes.empty()) startHole();
    else turn_ = -1;
}

std::uint64_t Match::shotSeed(int n) const { return Rng::mix(cfg_.seed, 0x5307000000ull + static_cast<std::uint64_t>(n)); }

Cup Match::cup(int h, bool forPutt) const {
    const HoleDef& hd = hole(h);
    Cup c;
    // Today's hole position: one of the green's pins, chosen by the match seed.
    Rng r(Rng::mix(cfg_.seed, 0x9147 + static_cast<std::uint64_t>(h)));
    const size_t k = static_cast<size_t>(r.uniform() * static_cast<double>(hd.pins.size())) % hd.pins.size();
    c.pos = hd.pins[k];
    c.flagstick = !forPutt || cfg_.flagstickForPutts;
    return c;
}

WindSpec Match::wind(int h) const {
    WindSpec w = course_.wind;
    Rng r(Rng::mix(cfg_.seed, 0x3171d + static_cast<std::uint64_t>(h)));
    // A day's wind: the course's prevailing wind, varied hole by hole.
    w.speed10 = std::max(0.0, course_.wind.speed10 * cfg_.windScale * (0.55 + 0.9 * r.uniform()));
    w.towardHeading = course_.wind.towardHeading + r.normal() * 25 * kDeg;
    if (!cfg_.gusts) w.gustIntensity = 0;
    return w;
}

PlayContext Match::context(int p) const {
    PlayContext ctx;
    ctx.course = &course_;
    ctx.hole = &hole(hole_ < holeCount() ? hole_ : holeCount() - 1);
    const PlayerState& ps = players_[static_cast<size_t>(p)];
    const bool onGreen = course_.surfaces[ps.surface].id == "green";
    ctx.cup = cup(hole_ < holeCount() ? hole_ : holeCount() - 1, onGreen);
    ctx.clubs = &clubs_;
    ctx.ball = &ball_;
    ctx.air = course_.conditions;
    ctx.wind = wind(hole_ < holeCount() ? hole_ : holeCount() - 1);
    ctx.wind.gustIntensity = 0;
    ctx.ballPos = ps.pos;
    ctx.surface = ps.surface;
    ctx.plugged = ps.plugged;
    ctx.teed = ps.teed;
    ctx.leftHanded = cfg_.players[static_cast<size_t>(p)].leftHanded;
    return ctx;
}

void Match::startHole() {
    const HoleDef& h = hole(hole_);
    for (size_t i = 0; i < players_.size(); ++i) {
        PlayerState& s = players_[i];
        const std::string& teeName = cfg_.players[i].tee;
        const TeeBox* tee = &h.tees[0];
        for (const auto& t : h.tees)
            if (t.name == teeName) tee = &t;
        // Tee markers: each player tees up a little to one side of the centre.
        const Vec3 side = heading(tee->heading).cross(kUp);
        const double off = (static_cast<double>(i) - 0.5 * static_cast<double>(players_.size() - 1)) * 0.8;
        const Vec3 p = tee->pos + side * off;
        s.pos = {p.x, course_.terrain.height(p.x, p.z) + ball_.radius() + 0.02, p.z};  // on a tee peg
        s.surface = course_.terrain.sample(p.x, p.z).surface;
        s.plugged = false;
        s.teed = true;
        s.strokes = 0;
        s.done = false;
        s.pickedUp = false;
    }
    // Honour: lowest score on the previous hole; ties keep the order.
    if (hole_ > 0) {
        const int prev = hole_ - 1;
        std::stable_sort(order_.begin(), order_.end(), [&](int a, int b) {
            return players_[static_cast<size_t>(a)].gross[static_cast<size_t>(prev)] <
                   players_[static_cast<size_t>(b)].gross[static_cast<size_t>(prev)];
        });
    }
    turn_ = order_[0];
}

void Match::advanceTurn() {
    if (finished()) {
        turn_ = -1;
        return;
    }
    const Cup c = cup(hole_);
    // Everyone still on the tee plays in honour order first; then the farthest from the hole.
    for (int p : order_) {
        const PlayerState& s = players_[static_cast<size_t>(p)];
        if (!s.done && s.strokes == 0) {
            turn_ = p;
            return;
        }
    }
    int best = -1;
    double far = -1;
    for (int p : order_) {
        const PlayerState& s = players_[static_cast<size_t>(p)];
        if (s.done) continue;
        const double d = horiz(s.pos, c.pos);
        if (d > far + 1e-9) {
            far = d;
            best = p;
        }
    }
    if (best >= 0) {
        turn_ = best;
        return;
    }
    // Hole complete.
    ++hole_;
    if (finished()) {
        turn_ = -1;
        return;
    }
    startHole();
}

Plan Match::plan() const {
    if (turn_ < 0) return {};
    const PlayContext ctx = context(turn_);
    return planShot(ctx, golfers_[static_cast<size_t>(turn_)]);
}

void Match::pickUp() {
    if (turn_ < 0) return;
    PlayerState& s = players_[static_cast<size_t>(turn_)];
    s.done = true;
    s.pickedUp = true;
    const int limit = hole(hole_).par + (cfg_.maxOverPar > 0 ? cfg_.maxOverPar : 5);
    s.gross[static_cast<size_t>(hole_)] = std::max(limit, s.strokes);
    advanceTurn();
}

Match::Played Match::play(const SwingInput& in) {
    Played out;
    if (turn_ < 0) return out;
    const int p = turn_;
    PlayerState& s = players_[static_cast<size_t>(p)];
    const HoleDef& h = hole(hole_);
    const bool onGreen = course_.surfaces[s.surface].id == "green";
    const Cup c = cup(hole_, onGreen);
    const int ci = clubs_.find(in.club);
    const Club& club = clubs_.clubs[static_cast<size_t>(ci < 0 ? clubs_.find("I7") : ci)];
    const std::uint64_t seed = shotSeed(static_cast<int>(log_.size()));
    Rng rng(seed);
    Lie lie{&course_.surfaces[s.surface], s.plugged, s.teed};
    const Delivery d = makeDelivery(in, club, golfers_[static_cast<size_t>(p)], lie, cfg_.difficulty,
                                    cfg_.players[static_cast<size_t>(p)].leftHanded, rng);
    const ImpactResult imp = strike(club, ball_, d);
    const Launch launch = toLaunch(imp, s.pos, in.aimHeading);
    SimOptions o;
    o.seed = Rng::mix(seed, 0xF11);
    const World world = course_.world(&c);
    out.shot = simulateShot(launch, ball_, course_.conditions, wind(hole_), world, o);

    ShotRecord& r = out.record;
    r.player = p;
    r.hole = hole_;
    r.stroke = s.strokes + 1;
    r.input = in;
    r.seed = seed;
    r.from = s.pos;
    r.fromSurface = s.surface;
    r.impact = imp;
    r.delivery = d;
    r.stats = out.shot.stats;
    r.outcome = out.shot.outcome;
    r.distanceToPinBefore = horiz(s.pos, c.pos);
    RulesOptions ro;
    ro.localRuleE5 = cfg_.localRuleE5;
    r.ruling = rule(course_, h, c.pos, s.pos, s.surface, out.shot, ro);
    const double eBefore = expectedStrokes(r.distanceToPinBefore, course_.surfaces[s.surface]);

    s.strokes += 1 + r.ruling.penalty;
    s.teed = false;
    if (r.ruling.kind == Ruling::Kind::Holed) {
        s.done = true;
        r.to = out.shot.final.pos;
        r.toSurface = out.shot.finalSurface;
        r.distanceToPinAfter = 0;
    } else {
        s.pos = r.ruling.next;
        s.surface = r.ruling.nextSurface;
        s.plugged = r.ruling.plugged;
        // Stroke and distance from the tee: tee it up again.
        if (r.ruling.replay && r.stroke == 1) s.teed = true;
        r.to = s.pos;
        r.toSurface = s.surface;
        r.distanceToPinAfter = horiz(s.pos, c.pos);
        if (cfg_.gimmeM > 0 && course_.surfaces[s.surface].id == "green" && r.distanceToPinAfter < cfg_.gimmeM) {
            s.strokes += 1;
            s.done = true;
            r.ruling.note = "conceded";
        }
    }
    const double eAfter = s.done ? 0.0 : expectedStrokes(r.distanceToPinAfter, course_.surfaces[s.surface]);
    r.strokesGained = eBefore - eAfter - 1.0 - r.ruling.penalty;
    if (!s.done && cfg_.maxOverPar > 0 && s.strokes >= h.par + cfg_.maxOverPar) {
        s.done = true;
        s.pickedUp = true;
    }
    if (s.done) s.gross[static_cast<size_t>(hole_)] = s.strokes;
    log_.push_back(r);
    const int before = hole_;
    advanceTurn();
    out.holeComplete = hole_ != before;
    out.finished = finished();
    return out;
}

// ---------------------------------------------------------------------------
// Scores

int Match::strokesReceived(int p, int h) const {
    // Course handicap from the Handicap Index (World Handicap System), pro rata
    // for the holes played; strokes given on the lowest stroke indexes first.
    const double idx = cfg_.players[static_cast<size_t>(p)].handicapIndex;
    const int n = holeCount();
    const int ch = static_cast<int>(std::lround(idx * n / 18.0));
    if (ch <= 0) return 0;
    std::vector<int> si;
    for (int i = 0; i < n; ++i) si.push_back(hole(i).strokeIndex);
    // Rank of this hole's stroke index among the holes played.
    int rank = 1;
    for (int i = 0; i < n; ++i)
        if (si[static_cast<size_t>(i)] < hole(h).strokeIndex) ++rank;
    return ch / n + (rank <= ch % n ? 1 : 0);
}

int Match::net(int p, int h) const {
    const int g = players_[static_cast<size_t>(p)].gross[static_cast<size_t>(h)];
    return g < 0 ? -1 : g - strokesReceived(p, h);
}

int Match::stablefordPoints(int p, int h) const {
    const int n = net(p, h);
    if (n < 0) return 0;
    return std::max(0, 2 + hole(h).par - n);
}

int Match::totalGross(int p) const {
    int t = 0;
    for (int g : players_[static_cast<size_t>(p)].gross) t += g > 0 ? g : 0;
    return t;
}

int Match::totalToPar(int p) const {
    int t = 0;
    for (int i = 0; i < holeCount(); ++i) {
        const int g = players_[static_cast<size_t>(p)].gross[static_cast<size_t>(i)];
        if (g > 0) t += g - hole(i).par;
    }
    return t;
}

int Match::totalStableford(int p) const {
    int t = 0;
    for (int i = 0; i < holeCount(); ++i) t += stablefordPoints(p, i);
    return t;
}

int Match::matchStatus(int upTo) const {
    if (playerCount() != 2) return 0;
    int s = 0;
    for (int i = 0; i < std::min(upTo, holeCount()); ++i) {
        const int a = net(0, i), b = net(1, i);
        if (a < 0 || b < 0) continue;
        s += a < b ? 1 : a > b ? -1 : 0;
    }
    return s;
}

// ---------------------------------------------------------------------------
// Serialisation

std::string Match::toJson() const {
    W w;
    w.open();
    w.key("config");
    w.raw(matchConfigJson(cfg_));
    w.kv("course", course_.id);
    w.openArr("shots");
    for (const auto& r : log_) {
        w.comma();
        w.raw(swingJson(r.input));
    }
    w.closeArr();
    w.close();
    return w.s;
}

std::string Match::stateJson() const {
    W w;
    w.open();
    w.kv("hole", hole_);
    w.kv("holes", holeCount());
    w.kv("turn", turn_);
    w.kv("finished", finished());
    w.kv("shots", static_cast<int>(log_.size()));
    w.kv("format", formatName(cfg_.format));
    if (!finished()) {
        const HoleDef& h = hole(hole_);
        w.kv("number", h.number);
        w.kv("par", h.par);
        w.kv("stroke_index", h.strokeIndex);
        const Cup c = cup(hole_);
        w.vec("pin", c.pos);
        const WindSpec ws = wind(hole_);
        w.kv("wind_speed", ws.speed10);
        w.kv("wind_toward", ws.towardHeading);
    }
    w.openArr("players");
    for (int i = 0; i < playerCount(); ++i) {
        const PlayerState& s = players_[static_cast<size_t>(i)];
        w.open();
        w.kv("name", cfg_.players[static_cast<size_t>(i)].name);
        w.kv("ai", cfg_.players[static_cast<size_t>(i)].ai);
        w.vec("pos", s.pos);
        w.kv("surface", course_.surfaces[s.surface].id);
        w.kv("strokes", s.strokes);
        w.kv("done", s.done);
        w.kv("teed", s.teed);
        w.kv("plugged", s.plugged);
        w.kv("gross", totalGross(i));
        w.kv("to_par", totalToPar(i));
        w.kv("stableford", totalStableford(i));
        w.key("card");
        w.raw("[");
        for (int hI = 0; hI < holeCount(); ++hI) w.raw((hI ? "," : "") + std::to_string(s.gross[static_cast<size_t>(hI)]));
        w.raw("]");
        w.key("received");
        w.raw("[");
        for (int hI = 0; hI < holeCount(); ++hI) w.raw((hI ? "," : "") + std::to_string(strokesReceived(i, hI)));
        w.raw("]");
        w.close();
    }
    w.closeArr();
    if (cfg_.format == Format::MatchPlay) w.kv("match_status", matchStatus(holeCount()));
    w.close();
    return w.s;
}

}  // namespace golf
