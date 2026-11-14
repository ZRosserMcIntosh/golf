// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// The default data files compiled into the library (generated at build time
// from data/ by cmake/embed.cmake, or web/embed.mjs for the WebAssembly
// build), so the core runs without a file system.
#pragma once

namespace golf::embedded {

const char* clubsStandard();     // data/clubs/standard.json
const char* surfaces();          // data/surfaces/surfaces.json
const char* balls();             // data/balls/balls.json
const char* golfers();           // data/golfers/golfers.json

}  // namespace golf::embedded
