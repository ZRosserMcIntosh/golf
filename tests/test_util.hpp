// Golf Dynamics — test helpers.
// SPDX-License-Identifier: MIT
#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include "golf/json.hpp"

namespace golftest {

inline std::string readData(const std::string& rel) {
    std::ifstream f(std::string(GOLF_DATA_DIR) + "/" + rel, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline golf::json::Value loadJson(const std::string& rel) {
    auto r = golf::json::parse(readData(rel));
    if (!r.ok()) std::cerr << rel << ": " << r.error << "\n";
    return r.value;
}

inline double num(const golf::json::Value& v, const char* key) {
    const auto* f = v.find(key);
    return f && f->isNumber() ? f->asNumber() : 0.0;
}

}  // namespace golftest
