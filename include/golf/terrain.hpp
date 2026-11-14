// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
//
// A course's ground as rasters: heights on a regular grid, sampled with
// Catmull-Rom bicubic interpolation (C1-continuous, so the normal a rolling
// ball sees changes smoothly from cell to cell), and surface ids on a finer
// grid. The renderer draws the same rasters the physics uses.
#pragma once

#include <cstdint>
#include <vector>

#include "golf/world.hpp"

namespace golf {

struct HeightGrid {
    int nx = 0, nz = 0;
    double x0 = 0, z0 = 0, cell = 1.0;
    std::vector<float> h;  // row-major, z rows of nx

    float at(int i, int j) const {
        i = i < 0 ? 0 : (i >= nx ? nx - 1 : i);
        j = j < 0 ? 0 : (j >= nz ? nz - 1 : j);
        return h[static_cast<size_t>(j) * static_cast<size_t>(nx) + static_cast<size_t>(i)];
    }
    // Height and its gradient (dh/dx, dh/dz).
    double sample(double x, double z, double* gx = nullptr, double* gz = nullptr) const;
};

struct SurfaceGrid {
    int nx = 0, nz = 0;
    double x0 = 0, z0 = 0, cell = 0.5;
    std::vector<std::uint8_t> s;
    int at(double x, double z, int outside) const;
};

class RasterTerrain final : public Terrain {
public:
    HeightGrid grid;
    SurfaceGrid surfaces;
    int outsideSurface = 0;

    GroundSample sample(double x, double z) const override;
    double height(double x, double z) const override { return grid.sample(x, z); }
};

}  // namespace golf
