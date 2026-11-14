// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/terrain.hpp"

#include <cmath>

namespace golf {

namespace {
// Catmull-Rom weights and their derivatives at t in [0, 1).
inline void crWeights(double t, double w[4], double dw[4]) {
    const double t2 = t * t, t3 = t2 * t;
    w[0] = 0.5 * (-t3 + 2 * t2 - t);
    w[1] = 0.5 * (3 * t3 - 5 * t2 + 2);
    w[2] = 0.5 * (-3 * t3 + 4 * t2 + t);
    w[3] = 0.5 * (t3 - t2);
    dw[0] = 0.5 * (-3 * t2 + 4 * t - 1);
    dw[1] = 0.5 * (9 * t2 - 10 * t);
    dw[2] = 0.5 * (-9 * t2 + 8 * t + 1);
    dw[3] = 0.5 * (3 * t2 - 2 * t);
}
}  // namespace

double HeightGrid::sample(double x, double z, double* gx, double* gz) const {
    if (nx < 2 || nz < 2) {
        if (gx) *gx = 0;
        if (gz) *gz = 0;
        return h.empty() ? 0.0 : h[0];
    }
    const double fx = (x - x0) / cell, fz = (z - z0) / cell;
    const double cx = clamp(fx, 0.0, nx - 1.000001), cz = clamp(fz, 0.0, nz - 1.000001);
    const int i = static_cast<int>(std::floor(cx)), j = static_cast<int>(std::floor(cz));
    const double tx = cx - i, tz = cz - j;
    double wx[4], dwx[4], wz[4], dwz[4];
    crWeights(tx, wx, dwx);
    crWeights(tz, wz, dwz);
    double v = 0, dvx = 0, dvz = 0;
    for (int b = 0; b < 4; ++b) {
        double row = 0, drow = 0;
        for (int a = 0; a < 4; ++a) {
            const double hv = at(i - 1 + a, j - 1 + b);
            row += wx[a] * hv;
            drow += dwx[a] * hv;
        }
        v += wz[b] * row;
        dvz += dwz[b] * row;
        dvx += wz[b] * drow;
    }
    // Outside the grid the ground continues level.
    const bool inX = fx >= 0 && fx <= nx - 1, inZ = fz >= 0 && fz <= nz - 1;
    if (gx) *gx = inX ? dvx / cell : 0.0;
    if (gz) *gz = inZ ? dvz / cell : 0.0;
    return v;
}

int SurfaceGrid::at(double x, double z, int outside) const {
    const double fx = (x - x0) / cell, fz = (z - z0) / cell;
    if (fx < 0 || fz < 0 || fx >= nx || fz >= nz) return outside;
    const int i = static_cast<int>(fx), j = static_cast<int>(fz);
    return s[static_cast<size_t>(j) * static_cast<size_t>(nx) + static_cast<size_t>(i)];
}

void TreeIndex::build(const std::vector<Tree>& trees, double minX, double minZ, double maxX, double maxZ, double cellSize) {
    cell = cellSize;
    x0 = minX;
    z0 = minZ;
    nx = static_cast<int>((maxX - minX) / cell) + 1;
    nz = static_cast<int>((maxZ - minZ) / cell) + 1;
    std::vector<std::vector<int>> cells(static_cast<size_t>(nx) * static_cast<size_t>(nz));
    for (size_t t = 0; t < trees.size(); ++t) {
        const Tree& tr = trees[t];
        const double r = (tr.canopyRx > tr.trunkRadius ? tr.canopyRx : tr.trunkRadius) + 0.1;
        const int i0 = static_cast<int>((tr.x - r - x0) / cell), i1 = static_cast<int>((tr.x + r - x0) / cell);
        const int j0 = static_cast<int>((tr.z - r - z0) / cell), j1 = static_cast<int>((tr.z + r - z0) / cell);
        for (int j = j0 < 0 ? 0 : j0; j <= j1 && j < nz; ++j)
            for (int i = i0 < 0 ? 0 : i0; i <= i1 && i < nx; ++i)
                cells[static_cast<size_t>(j) * static_cast<size_t>(nx) + static_cast<size_t>(i)].push_back(static_cast<int>(t));
    }
    start.assign(cells.size() + 1, 0);
    items.clear();
    for (size_t c = 0; c < cells.size(); ++c) {
        start[c] = static_cast<int>(items.size());
        items.insert(items.end(), cells[c].begin(), cells[c].end());
    }
    start[cells.size()] = static_cast<int>(items.size());
}

GroundSample RasterTerrain::sample(double x, double z) const {
    GroundSample g;
    double gx = 0, gz = 0;
    g.height = grid.sample(x, z, &gx, &gz);
    g.normal = Vec3{-gx, 1.0, -gz}.normalized();
    g.surface = surfaces.at(x, z, outsideSurface);
    return g;
}

}  // namespace golf
