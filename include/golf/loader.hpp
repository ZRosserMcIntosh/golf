// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// Ball definitions from JSON (data/balls/balls.json).
#pragma once

#include <string>
#include <vector>

#include "golf/ball.hpp"
#include "golf/json.hpp"

namespace golf {

std::string loadBalls(const json::Value& root, std::vector<BallModel>& out);
std::vector<BallModel> standardBalls();
BallModel findBall(const std::string& id);

}  // namespace golf
