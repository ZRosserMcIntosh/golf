// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// A golf course: holes made of shapes (tees, a fairway along the line of
// play, a green with its contours, bunkers, water), the ground they sit on,
// the trees around them, and the course's out-of-bounds line. A course is
// described compactly in JSON (data/courses/*.json); buildCourse() turns it
// into the height and surface rasters the physics and the renderer share,
// and scatters the trees deterministically from the course's seed.
//
// Shapes are signed-distance primitives (super-ellipses, rectangles,
// polylines with a varying width), so the build needs no polygon clipping
// and the edges blend smoothly into the surrounding ground.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "golf/atmosphere.hpp"
#include "golf/json.hpp"
#include "golf/surface.hpp"
#include "golf/terrain.hpp"
#include "golf/wind.hpp"
#include "golf/world.hpp"

namespace golf {

struct Blob {  // a rotated super-ellipse
    double x = 0, z = 0, rx = 5, rz = 5, rot = 0, p = 2.2;
    double sdf(double px, double pz) const;
};

struct TeeBox {
    std::string name = "white";
    Vec3 pos;           // centre of the tee box (pos.y filled by the build)
    double heading = 0; // toward the first aim point
    double halfW = 4, halfL = 5;
    double raise = 0.4;
};

struct GreenDef {
    Blob shape;
    double raise = 0.3;      // above the surrounding ground at its centre
    double tiltX = 0, tiltZ = 0;  // grade (dh/dx, dh/dz) across the green
    struct Bump { double x, z, r, h; };
    std::vector<Bump> bumps;
    struct Tier { double x, z, nx, nz, h, w; };  // a step of height h across a line, width w
    std::vector<Tier> tiers;
    double level = 0;        // filled by the build
    double contour(double x, double z) const;  // height relative to `level`
};

struct Bunker {
    std::vector<Blob> lobes;
    double depth = 0.8;
    double sdf(double x, double z) const;
};

struct Lake {
    std::vector<Blob> lobes;
    double level = 0;      // water surface height; 0: set by the build from the ground
    bool red = true;       // red penalty area (lateral relief) or yellow
    double sdf(double x, double z) const;
};

struct HoleDef {
    int number = 1, par = 4, strokeIndex = 1;
    std::string name;
    std::vector<TeeBox> tees;
    std::vector<Vec3> pins;  // hole positions on the green (y filled by the build)
    std::vector<Vec3> line;  // line of play, tee to green (y unused)
    struct FairwayPoint { double x, z, halfWidth; };
    std::vector<FairwayPoint> fairway;  // empty for a par 3
    GreenDef green;
    std::vector<Bunker> bunkers;
    std::vector<Lake> lakes;
    double roughWidth = 14;  // first-cut rough beyond the fairway edge
    double bx0 = 0, bz0 = 0, bx1 = 0, bz1 = 0;  // bounding box of everything (set by the build)
    bool near(double x, double z, double margin) const {
        return x > bx0 - margin && x < bx1 + margin && z > bz0 - margin && z < bz1 + margin;
    }
    double fairwaySdf(double x, double z) const;
    double corridorDistance(double x, double z) const;  // distance from the line of play
};

struct PathDef {
    std::vector<Vec3> points;
    double halfWidth = 1.25;
};

struct TerrainDef {
    double base = 0;
    double gradeX = 0, gradeZ = 0;
    double noiseAmp = 1.5, noiseScale = 90.0;
    std::uint64_t seed = 7;
    struct Mound { double x, z, r, h; };
    std::vector<Mound> mounds;
    HeightGrid grid;  // optional real ground (e.g. SRTM), bicubic; added to base
};

struct TreeDef {
    std::uint64_t seed = 11;
    double spacing = 7.0;       // mean spacing in wooded areas, m
    double clearance = 6.0;     // keep this far from the edge of any played surface
    double corridorExtra = 10;  // ... and this far beyond the rough
    std::vector<Tree> fixed;    // explicitly placed
};

struct Course {
    std::string id, name, description, location, attribution;
    double siteEast = 0, siteNorth = 0;  // where the course's origin sits in the host world's frame
    Conditions conditions;
    WindSpec wind;              // a typical day; matches vary it
    double greenStimpFt = 10.5;
    double x0 = -100, z0 = -100, x1 = 100, z1 = 100;  // bounds
    double cell = 1.0, surfaceCell = 0.5;
    std::vector<Vec3> boundary;  // out-of-bounds line (polygon); empty: bounds inset 4 m
    TerrainDef terrainDef;
    TreeDef treeDef;
    std::vector<HoleDef> holes;
    std::vector<PathDef> paths;

    // Built.
    SurfaceTable surfaces;
    RasterTerrain terrain;
    std::vector<Tree> trees;
    TreeIndex treeIndex;
    int idRough = 0, idDeepRough = 0, idNative = 0, idFairway = 0, idGreen = 0, idFringe = 0, idTee = 0,
        idBunker = 0, idWater = 0, idPath = 0;

    bool inBounds(double x, double z) const;
    const Lake* lakeAt(double x, double z, const HoleDef** hole = nullptr) const;
    World world(const Cup* cup) const;
    double baseHeight(double x, double z) const;  // before any feature
};

// Parse a course; returns "" or an error message.
std::string loadCourse(const json::Value& root, Course& out);
// Build the rasters and trees (deterministic).
void buildCourse(Course& c);

}  // namespace golf
