// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// What a shot needs to know about the place it is played: the ground (height,
// normal and surface at any horizontal point), the trees, and the hole with
// its flagstick. The game layer (course.hpp) implements Terrain for a real
// hole; tests use the flat and planar terrains below.
#pragma once

#include <vector>

#include "golf/math.hpp"
#include "golf/surface.hpp"

namespace golf {

struct GroundSample {
    double height = 0.0;  // ground height at (x, z), m
    Vec3 normal = kUp;    // unit outward normal
    int surface = 0;      // index into the SurfaceTable
};

class Terrain {
public:
    virtual ~Terrain() = default;
    virtual GroundSample sample(double x, double z) const = 0;
    virtual double height(double x, double z) const { return sample(x, z).height; }
};

// Level ground of one surface.
class FlatTerrain final : public Terrain {
public:
    explicit FlatTerrain(int surface = 0, double height = 0.0) : surface_(surface), h_(height) {}
    GroundSample sample(double, double) const override { return {h_, kUp, surface_}; }
    double height(double, double) const override { return h_; }

private:
    int surface_;
    double h_;
};

// A plane y = h0 + gx x + gz z of one surface.
class PlaneTerrain final : public Terrain {
public:
    PlaneTerrain(int surface, double h0, double gx, double gz)
        : surface_(surface), h0_(h0), gx_(gx), gz_(gz), n_(Vec3{-gx, 1, -gz}.normalized()) {}
    GroundSample sample(double x, double z) const override { return {height(x, z), n_, surface_}; }
    double height(double x, double z) const override { return h0_ + gx_ * x + gz_ * z; }

private:
    int surface_;
    double h0_, gx_, gz_;
    Vec3 n_;
};

// A tree: a trunk (vertical cylinder) and a canopy (ellipsoid of revolution).
// A ball crossing the canopy meets leaves and branches with a probability
// that grows with the path length inside it (a Beer-Lambert law in the
// canopy's density); see flight.cpp.
struct Tree {
    double x = 0, z = 0, baseY = 0;
    double trunkRadius = 0.2, trunkHeight = 4.0;
    double canopyY = 7.0;  // canopy centre above the base
    double canopyRx = 3.5, canopyRy = 3.0;
    double density = 0.35;  // expected branch strikes per metre of path at the centre
    int kind = 0;           // rendering hint
};

// The hole. Rules of Golf (Definitions, "Hole"): 4.25 in (108 mm) diameter,
// at least 4 in (101.6 mm) deep. The flagstick is 2.13 m (7 ft) by
// convention (USGA/R&A recommend at least 7 ft); 12.7 mm (1/2 in) diameter.
struct Cup {
    Vec3 pos;  // centre of the hole on the green surface
    double radius = 0.054;
    double depth = 0.1016;
    bool flagstick = false;  // attended / in the hole
    double pinHeight = 2.13;
    double pinRadius = 0.00635;
};

// A uniform grid over the trees, so a ball only tests the trees near it.
struct TreeIndex {
    double x0 = 0, z0 = 0, cell = 20;
    int nx = 0, nz = 0;
    std::vector<int> start;  // nx*nz + 1 offsets into items
    std::vector<int> items;
    void build(const std::vector<Tree>& trees, double minX, double minZ, double maxX, double maxZ, double cellSize = 20);
    // Calls f(index) for trees whose cell lies within r of (x, z).
    template <class F>
    void query(double x, double z, double r, F&& f) const {
        if (nx == 0) return;
        const int i0 = static_cast<int>((x - r - x0) / cell), i1 = static_cast<int>((x + r - x0) / cell);
        const int j0 = static_cast<int>((z - r - z0) / cell), j1 = static_cast<int>((z + r - z0) / cell);
        for (int j = j0 < 0 ? 0 : j0; j <= j1 && j < nz; ++j)
            for (int i = i0 < 0 ? 0 : i0; i <= i1 && i < nx; ++i) {
                const int c = j * nx + i;
                for (int k = start[static_cast<size_t>(c)]; k < start[static_cast<size_t>(c) + 1]; ++k) f(items[static_cast<size_t>(k)]);
            }
    }
};

struct World {
    const Terrain* terrain = nullptr;
    const SurfaceTable* surfaces = nullptr;
    const std::vector<Tree>* trees = nullptr;
    const TreeIndex* treeIndex = nullptr;  // optional; without it every tree is tested
    const Cup* cup = nullptr;
    // A ball that leaves this box (horizontal) is out of the simulation.
    double minX = -1e5, maxX = 1e5, minZ = -1e5, maxZ = 1e5;
};

}  // namespace golf
