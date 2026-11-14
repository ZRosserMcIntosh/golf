// Golf Dynamics — design the "Clube de Golfe da Várzea" course (fictional).
// SPDX-License-Identifier: MIT
//
// Nine original holes laid out on the real ground of São Paulo's Pinheiros
// floodplain and the Butantã hill above it, about 8 km west of MASP. The
// ground comes from the São Paulo World city terrain (SRTM 1", NASA/USGS,
// public domain; processed by Aviation Dynamics' tools/terrain into a 50 m
// grid in the site frame: EPSG:31983 less the origin 330986.00, 7393310.72).
//
// Usage: node tools/design_course.mjs <terrain_sp_city.avt.gz> > data/courses/varzea.json
//
// The course frame: x east, z south (y up), origin at site east -7950 m,
// north -100 m.
import fs from 'node:fs';
import zlib from 'node:zlib';

const E0 = -7950, N0 = -100;           // course origin in the site frame
const BOUNDS = [-680, -640, 680, 640];  // x0, z0, x1, z1 (course frame)

// ---- terrain --------------------------------------------------------------
function readAvt(path) {
  let b = fs.readFileSync(path);
  if (path.endsWith('.gz')) b = zlib.gunzipSync(b);
  if (b.toString('latin1', 0, 4) !== 'AVT1') throw new Error('not an AVT1 grid');
  const nx = b.readInt32LE(4), ny = b.readInt32LE(8);
  const e0 = b.readDoubleLE(12), n0 = b.readDoubleLE(20), sp = b.readDoubleLE(28), sc = b.readDoubleLE(36);
  const h = new Float32Array(nx * ny);
  for (let k = 0; k < nx * ny; k++) h[k] = b.readInt16LE(44 + 2 * k) * sc;
  return (e, n) => {
    const fx = (e - e0) / sp, fy = (n - n0) / sp;
    const i = Math.min(Math.max(Math.floor(fx), 0), nx - 2), j = Math.min(Math.max(Math.floor(fy), 0), ny - 2);
    const u = fx - i, v = fy - j, g = (a, c) => h[c * nx + a];
    return (g(i, j) * (1 - u) + g(i + 1, j) * u) * (1 - v) + (g(i, j + 1) * (1 - u) + g(i + 1, j + 1) * u) * v;
  };
}

// ---- geometry helpers -----------------------------------------------------
const len = (a, b) => Math.hypot(b[0] - a[0], b[1] - a[1]);
const r1 = (v) => Math.round(v * 10) / 10;
function along(line, d, lateral = 0) {
  // Point `d` metres along a polyline, `lateral` metres to the right (+z side of +x).
  for (let i = 1; i < line.length; i++) {
    const a = line[i - 1], b = line[i], L = len(a, b);
    if (d <= L || i === line.length - 1) {
      const t = Math.min(Math.max(d / L, 0), 1.2);
      const ux = (b[0] - a[0]) / L, uz = (b[1] - a[1]) / L;
      // right of the direction (ux, uz) in an x-east, z-south frame is (-uz, ux)
      return [a[0] + ux * L * t - uz * lateral, a[1] + uz * L * t + ux * lateral];
    }
    d -= L;
  }
  return line[line.length - 1];
}
function lineLength(line) { let s = 0; for (let i = 1; i < line.length; i++) s += len(line[i - 1], line[i]); return s; }
function dirAt(line, d) {
  const p = along(line, Math.max(d - 1, 0)), q = along(line, d + 1);
  const L = len(p, q); return [(q[0] - p[0]) / L, (q[1] - p[1]) / L];
}

// ---- the holes ------------------------------------------------------------
// Each: par, stroke index, line (tee -> [dogleg] -> green centre), green size
// and contours, bunkers (by distance along the line and lateral offset, or by
// angle and distance round the green), water.
const HOLES = [
  { name: 'Mirante', par: 4, si: 5, line: [[-30, 420], [-140, 230], [-200, 118]], green: { rx: 15, rz: 12, slope: 0.018, bumps: [[4, -3, 4.5, 0.12]], tiers: [] },
    fwHalf: 18, bunkers: [{ d: 238, lat: 22, rx: 9, rz: 5 }, { d: 250, lat: -24, rx: 8, rz: 4 }, { g: 140, r: 17, rx: 7, rz: 3.5 }, { g: -35, r: 17, rx: 6, rz: 3.5 }] },
  { name: 'Ipê', par: 3, si: 8, line: [[-238, 92], [-382, 20]], green: { rx: 13, rz: 11, slope: 0.015, bumps: [], tiers: [{ off: -2, h: 0.3, w: 5.5 }] },
    bunkers: [{ g: 60, r: 15, rx: 8, rz: 3.5 }, { g: -60, r: 15, rx: 7, rz: 3.5 }, { g: 180, r: 16, rx: 6, rz: 3 }] },
  { name: 'Várzea', par: 5, si: 2, line: [[-420, -22], [-468, -250], [-292, -462]], green: { rx: 16, rz: 11, slope: 0.012, bumps: [[-4, 3, 4.5, 0.12], [5, -2, 4.5, -0.1]], tiers: [] },
    fwHalf: 19, bunkers: [{ d: 255, lat: 24, rx: 10, rz: 5 }, { d: 395, lat: -20, rx: 8, rz: 4 }, { g: 120, r: 17, rx: 8, rz: 3.5 }, { g: -90, r: 18, rx: 9, rz: 4 }],
    water: [{ d: 150, lat: -46, lobes: [[0, 0, 32, 18, 20], [22, -14, 18, 12, 0]] }] },
  { name: 'Ponte', par: 4, si: 4, line: [[-205, -520], [160, -432]], green: { rx: 14, rz: 12, slope: 0.02, bumps: [], tiers: [{ off: 1, h: 0.3, w: 5.5 }] },
    fwHalf: 17, bunkers: [{ d: 245, lat: -22, rx: 11, rz: 5 }, { d: 262, lat: 24, rx: 7, rz: 4 }, { g: 40, r: 16, rx: 8, rz: 3.5 }, { g: -40, r: 16, rx: 8, rz: 3.5 }] },
  { name: 'Garças', par: 4, si: 7, line: [[205, -382], [330, -470], [462, -548]], green: { rx: 13, rz: 11, slope: 0.016, bumps: [[3, 3, 4.5, 0.12]], tiers: [] },
    fwHalf: 16, bunkers: [{ d: 225, lat: 20, rx: 8, rz: 4 }, { g: 160, r: 15, rx: 7, rz: 3.5 }],
    water: [{ d: 290, lat: -38, lobes: [[0, 0, 26, 14, 35]] }] },
  { name: 'Lago', par: 3, si: 9, line: [[505, -482], [560, -306]], green: { rx: 14, rz: 12, slope: 0.014, bumps: [[-3, -2, 4.5, 0.1]], tiers: [] },
    bunkers: [{ g: 120, r: 16, rx: 7, rz: 3 }, { g: -110, r: 16, rx: 6, rz: 3 }],
    water: [{ d: 95, lat: 0, lobes: [[0, 0, 40, 26, 10], [-18, 18, 24, 16, 40]] }] },
  { name: 'Subida', par: 4, si: 3, line: [[522, -246], [470, -40], [420, 130]], green: { rx: 15, rz: 12, slope: 0.02, bumps: [], tiers: [{ off: 0, h: 0.3, w: 5.5 }] },
    fwHalf: 17, bunkers: [{ d: 240, lat: 21, rx: 10, rz: 5 }, { g: 70, r: 17, rx: 8, rz: 3.5 }, { g: -70, r: 17, rx: 8, rz: 3.5 }] },
  { name: 'Jacarandá', par: 5, si: 1, line: [[328, 168], [130, -18], [-58, -118]], green: { rx: 16, rz: 12, slope: 0.012, bumps: [[-5, 0, 4.5, 0.12], [4, 4, 4.5, 0.1]], tiers: [] },
    fwHalf: 19, bunkers: [{ d: 248, lat: -24, rx: 10, rz: 5 }, { d: 262, lat: 22, rx: 9, rz: 4 }, { d: 410, lat: 18, rx: 8, rz: 4 }, { g: 30, r: 18, rx: 9, rz: 3.5 }, { g: -120, r: 17, rx: 7, rz: 3.5 }] },
  { name: 'Clube', par: 4, si: 6, line: [[-22, -92], [20, 100], [62, 280]], green: { rx: 15, rz: 12, slope: 0.02, bumps: [[0, 4, 4.5, 0.12]], tiers: [] },
    fwHalf: 18, bunkers: [{ d: 230, lat: -22, rx: 10, rz: 5 }, { g: 50, r: 17, rx: 8, rz: 3.5 }, { g: -50, r: 17, rx: 8, rz: 3.5 }, { g: 180, r: 17, rx: 7, rz: 3 }] },
];

function blobAt(p, rx, rz, rotDeg) { return { x: r1(p[0]), z: r1(p[1]), rx, rz, rot: Math.round(rotDeg), p: 2.4 }; }

function buildHole(h, n) {
  const L = lineLength(h.line);
  const end = h.line[h.line.length - 1];
  const appr = dirAt(h.line, L - 2);  // approach direction
  const apprDeg = Math.atan2(appr[1], appr[0]) * 180 / Math.PI;
  const start = h.line[0], first = dirAt(h.line, 1);
  const tees = [
    { name: 'blue', x: r1(start[0] - first[0] * 16), z: r1(start[1] - first[1] * 16) },
    { name: 'white', x: r1(start[0]), z: r1(start[1]) },
    { name: 'red', x: r1(start[0] + first[0] * (h.par === 3 ? 22 : 38)), z: r1(start[1] + first[1] * (h.par === 3 ? 22 : 38)) },
  ];
  for (const t of tees) { t.half_width = 4.5; t.half_length = 5; }
  const g = h.green;
  // Back-to-front slope: the green rises along the approach.
  const green = { x: end[0], z: end[1], rx: g.rx, rz: g.rz, rot: Math.round(apprDeg), p: 2.3, raise: 0.35,
    tilt_x: r1(appr[0] * g.slope * 1000) / 1000, tilt_z: r1(appr[1] * g.slope * 1000) / 1000,
    bumps: g.bumps.map(([a, b, r, hh]) => { const p = toWorld(end, appr, a, b); return { x: r1(p[0]), z: r1(p[1]), r, h: hh }; }),
    tiers: g.tiers.map((t) => { const p = toWorld(end, appr, t.off, 0); return { x: r1(p[0]), z: r1(p[1]), dir_deg: Math.round(apprDeg), h: t.h, w: t.w }; }) };
  // Hole positions: centre-front, back-left, back-right.
  const pins = [toWorld(end, appr, -0.45 * g.rx, 0.15 * g.rz), toWorld(end, appr, 0.45 * g.rx, -0.45 * g.rz), toWorld(end, appr, 0.35 * g.rx, 0.5 * g.rz)]
    .map((p) => [r1(p[0]), r1(p[1])]);
  // Fairway: from 75 m off the tee to the front of the green.
  const fairway = [];
  if (h.par > 3) {
    for (let d = 75; d < L - g.rx; d += 30) {
      const p = along(h.line, d);
      let w = h.fwHalf;
      if (d > 215 && d < 275) w -= 3;  // the landing zone pinches
      if (d > L - 70) w = Math.max(10, w - (d - (L - 70)) * 0.12);
      fairway.push([r1(p[0]), r1(p[1]), w]);
    }
    const p = along(h.line, L - g.rx + 3);
    fairway.push([r1(p[0]), r1(p[1]), 10]);
  }
  const bunkers = (h.bunkers || []).map((b) => {
    let p, rot;
    if (b.g !== undefined) {
      const a = (apprDeg + b.g) * Math.PI / 180;  // angle round the green from the approach line
      p = [end[0] - Math.cos(a) * b.r, end[1] - Math.sin(a) * b.r];  // 0 deg is the front of the green
      rot = apprDeg + b.g + 90;
    } else {
      p = along(h.line, b.d, b.lat);
      const dd = dirAt(h.line, b.d);
      rot = Math.atan2(dd[1], dd[0]) * 180 / Math.PI;
    }
    const lobes = [blobAt(p, b.rx, b.rz, rot), blobAt([p[0] + 0.4 * b.rx * Math.cos((rot + 30) * Math.PI / 180), p[1] + 0.4 * b.rx * Math.sin((rot + 30) * Math.PI / 180)], b.rx * 0.6, b.rz * 0.9, rot + 25)];
    return { lobes, depth: 0.9 };
  });
  const water = (h.water || []).map((w) => {
    const p = along(h.line, w.d, w.lat);
    const dd = dirAt(h.line, w.d);
    return { lobes: w.lobes.map(([a, b, rx, rz, rot]) => blobAt(toWorld(p, dd, a, b), rx, rz, Math.atan2(dd[1], dd[0]) * 180 / Math.PI + rot)), colour: 'red' };
  });
  return {
    number: n, name: h.name, par: h.par, stroke_index: h.si,
    line: h.line.map(([x, z]) => [x, z]),
    tees, pins, fairway, green, bunkers, water, rough_width: 13,
  };
}
// A point a metres along `dir` from `o` and b metres to its right.
function toWorld(o, dir, a, b) { return [o[0] + dir[0] * a - dir[1] * b, o[1] + dir[1] * a + dir[0] * b]; }

// ---- assemble ---------------------------------------------------------------
const avtPath = process.argv[2];
if (!avtPath) { console.error('usage: node tools/design_course.mjs <terrain_sp_city.avt.gz>'); process.exit(1); }
const H = readAvt(avtPath);
const cell = 25, [x0, z0, x1, z1] = BOUNDS;
const gx0 = x0 - 50, gz0 = z0 - 50, nx = Math.ceil((x1 - gx0 + 50) / cell) + 1, nz = Math.ceil((z1 - gz0 + 50) / cell) + 1;
let lo = Infinity;
const raw = [];
for (let j = 0; j < nz; j++) for (let i = 0; i < nx; i++) {
  const x = gx0 + i * cell, z = gz0 + j * cell;
  const v = H(E0 + x, N0 - z);  // site north = -z
  raw.push(v); lo = Math.min(lo, v);
}
const offset = Math.floor(lo);
const holes = HOLES.map((h, i) => buildHole(h, i + 1));
const paths = [];
for (let i = 0; i + 1 < holes.length; i++) {
  const g = holes[i].green, t = holes[i + 1].tees[0];
  paths.push({ points: [[r1(g.x + 22), r1(g.z + 8)], [r1(t.x + 9), r1(t.z - 6)]], half_width: 1.3 });
}
const course = {
  id: 'varzea',
  name: 'Clube de Golfe da Várzea',
  description: 'Nine original holes (fictional) on the real ground of the Pinheiros floodplain and the Butantã hill above it, São Paulo: downhill from the clubhouse to the river flats, a par-3 over water, and an uphill finish.',
  location: 'São Paulo, Brazil (site frame east -7950 m, north -100 m; about 8 km west of MASP)',
  attribution: 'Ground: SRTM 1 arc-second (NASA/USGS, public domain) via São Paulo World / Aviation Dynamics tools/terrain. Course design: original, fictional.',
  site: { east: E0, north: N0 },
  conditions: { altitude_m: Math.round(lo + 15), temperature_c: 24, humidity: 0.65 },
  wind: { speed: 3.5, toward_deg: 200, roughness_m: 0.15, gust_intensity: 0.18, gust_time_s: 2.5 },
  stimp_ft: 10.5,
  bounds: BOUNDS,
  cell: 1.0,
  surface_cell: 0.5,
  terrain: { base: 0, noise_amp: 0.9, noise_scale: 55, seed: 1554,
    grid: { x0: gx0, z0: gz0, cell, nx, nz, scale: 0.1, offset, h: raw.map((v) => Math.round((v - offset) * 10)) },
    mounds: [] },
  trees: { seed: 2026, spacing: 11, clearance: 7, corridor_extra: 9 },
  paths,
  holes,
};
process.stdout.write(JSON.stringify(course) + '\n');
