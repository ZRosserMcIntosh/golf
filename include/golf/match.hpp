// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// A round: players (people or computer golfers), the order of play, the
// strokes and penalties of every hole, and the scores (stroke play, net
// Stableford, match play).
//
// A match is fully determined by its configuration and the inputs of its
// shots: every random draw (wind for each hole, gusts and scatter for each
// shot, branches) comes from the match seed. Replaying the shot log
// reproduces the round exactly, on any machine and in any build, which is
// what multiplayer sends over the network: inputs, never positions.
//
// Order of play follows the Rules: the honour on the tee (Rule 6.4a: lowest
// score on the previous hole, ties keeping the previous order), then the ball
// farthest from the hole (Rule 6.4b).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "golf/ball.hpp"
#include "golf/club.hpp"
#include "golf/course.hpp"
#include "golf/golfer.hpp"
#include "golf/impact.hpp"
#include "golf/json.hpp"
#include "golf/rules.hpp"
#include "golf/shot.hpp"

namespace golf {

enum class Format { Stroke, Stableford, MatchPlay };

struct PlayerConfig {
    std::string name = "Player";
    std::string golfer = "scratch";
    std::string tee = "white";
    bool ai = false;
    bool leftHanded = false;
    double handicapIndex = 0;
};

struct MatchConfig {
    std::vector<int> holes;  // indices into the course's holes; empty: all
    std::vector<PlayerConfig> players;
    Format format = Format::Stroke;
    std::uint64_t seed = 1;
    double windScale = 1.0;  // 0: calm
    bool gusts = true;
    double stimpFt = 0;      // 0: the course's
    int maxOverPar = 5;      // pick up at par + this (0: no limit)
    double gimmeM = 0;       // conceded putts inside this distance
    bool localRuleE5 = false;
    double difficulty = 1.0; // scales every golfer's scatter
    bool flagstickForPutts = false;
};

struct PlayerState {
    Vec3 pos;
    int surface = 0;
    bool plugged = false;
    bool teed = true;
    int strokes = 0;   // on this hole, penalties included
    bool done = false;
    bool pickedUp = false;
    std::vector<int> gross;  // per hole of the round; -1 not played
};

struct ShotRecord {
    int player = 0, hole = 0, stroke = 0;
    SwingInput input;
    std::uint64_t seed = 0;
    Vec3 from, to;
    int fromSurface = 0, toSurface = 0;
    Outcome outcome = Outcome::Rest;
    Ruling ruling;
    ShotStats stats;
    ImpactResult impact;
    Delivery delivery;
    double distanceToPinBefore = 0, distanceToPinAfter = 0;
    double strokesGained = 0;
};

class Match {
public:
    Match(const Course& course, MatchConfig cfg, ClubSet clubs = ClubSet::standard(), BallModel ball = BallModel::tour());

    const Course& course() const { return course_; }
    const MatchConfig& config() const { return cfg_; }
    const ClubSet& clubs() const { return clubs_; }
    const BallModel& ball() const { return ball_; }
    int holeIndex() const { return hole_; }  // into config().holes; == size(): finished
    int holeCount() const { return static_cast<int>(cfg_.holes.size()); }
    const HoleDef& hole(int i) const { return course_.holes[static_cast<size_t>(cfg_.holes[static_cast<size_t>(i)])]; }
    bool finished() const { return hole_ >= holeCount(); }
    int turn() const { return turn_; }
    const PlayerState& player(int i) const { return players_[static_cast<size_t>(i)]; }
    int playerCount() const { return static_cast<int>(players_.size()); }
    const std::vector<ShotRecord>& log() const { return log_; }

    Cup cup(int holeIdx, bool forPutt = false) const;
    WindSpec wind(int holeIdx) const;
    PlayContext context(int player) const;

    struct Played {
        ShotRecord record;
        ShotResult shot;
        bool holeComplete = false;
        bool finished = false;
    };
    Played play(const SwingInput& in);  // the player whose turn it is
    Plan plan() const;                  // the computer's suggestion for the current player
    void pickUp();                      // the current player picks up (scores par + maxOverPar)

    // Scores.
    int strokesReceived(int player, int holeIdx) const;
    int net(int player, int holeIdx) const;  // -1 if not played
    int stablefordPoints(int player, int holeIdx) const;
    int totalGross(int player) const;
    int totalToPar(int player) const;
    int totalStableford(int player) const;
    // Match play between players 0 and 1: + means player 0 is up.
    int matchStatus(int upToHole) const;

    // Serialisation: configuration and shot inputs (replayable), and state for display.
    std::string toJson() const;
    std::string stateJson() const;

private:
    void startHole();
    void advanceTurn();
    std::uint64_t shotSeed(int shotNumber) const;

    const Course& course_;
    MatchConfig cfg_;
    ClubSet clubs_;
    BallModel ball_;
    SurfaceTable surfaces_;
    std::vector<GolferProfile> golfers_;
    std::vector<PlayerState> players_;
    std::vector<int> order_;
    std::vector<ShotRecord> log_;
    int hole_ = 0;
    int turn_ = 0;
};

// Rebuild a match from toJson() output by replaying its shots. Returns "" or an error.
std::string parseMatchConfig(const json::Value& v, MatchConfig& cfg);
std::string matchConfigJson(const MatchConfig& cfg);
SwingInput parseSwing(const json::Value& v);
std::string swingJson(const SwingInput& in);

}  // namespace golf
