// Golf Dynamics — build a course from OpenStreetMap (and SRTM ground).
// SPDX-License-Identifier: MIT
//
// Usage:
//   node tools/osm_course.mjs --name "São Paulo Golf Club" --id spgc \
//        --terrain <terrain_sp_city.avt.gz> [--overpass https://overpass-api.de/api/interpreter] \
//        [--osm cached.json] > data/courses/spgc.json
//
// What it reads (OpenStreetMap golf tagging, wiki.openstreetmap.org/wiki/Tag:leisure=golf_course):
//   leisure=golf_course   the course's outline: its out-of-bounds line
//   golf=hole             the line of play, tee to green; ref (number), par, handicap (stroke index)
//   golf=tee / green / fairway / bunker (and natural=sand) / rough
//   golf=water_hazard (yellow) / lateral_water_hazard (red) / natural=water (red)
//   golf=cartpath / golf=path, natural=tree (fixed trees)
// What it adds, because the map does not say: green contours (a gentle tilt
// with the ground and two seeded swells), the hole positions (three per green,
// inside it), tee names by distance (blue, white, red), the trees in the
// wooded areas off the holes (seeded).
//
// Coordinates: WGS84 to EPSG:31983 (SIRGAS 2000 / UTM 23S; GRS80, which
// differs from WGS84 by well under a millimetre here), less the São Paulo
// World origin (330986.00, 7393310.72): the site frame. The course frame is
// x east, z south from the course's centre.
// Data © OpenStreetMap contributors, ODbL 1.0: the output is a derived
// database and carries the attribution.
import fs from 'node:fs';
import zlib from 'node:zlib';

const args = Object.fromEntries(process.argv.slice(2).reduce((a, v, i, arr) => (v.startsWith('--') ? [...a, [v.slice(2), arr[i + 1]]] : a), []));
const NAME = args.name || 'São Paulo Golf Club';
const ID = args.id || 'osm';
const OVERPASS = args.overpass || 'https://overpass-api.de/api/interpreter';
const ORIGIN_E = 330986.0, ORIGIN_N = 7393310.72;

// ---- Projection: transverse Mercator, UTM zone 23 south (Snyder 1987, USGS PP 1395, eqs. 8-9 to 8-15).
function utm23s(lat, lon) {
  const a = 6378137.0, f = 1 / 298.257222101, k0 = 0.9996, lon0 = -45 * Math.PI / 180;
  const e2 = f * (2 - f), ep2 = e2 / (1 - e2);
  const phi = lat * Math.PI / 180, lam = lon * Math.PI / 180;
  const N = a / Math.sqrt(1 - e2 * Math.sin(phi) ** 2);
  const T = Math.tan(phi) ** 2, C = ep2 * Math.cos(phi) ** 2, A = (lam - lon0) * Math.cos(phi);
  const M = a * ((1 - e2 / 4 - 3 * e2 ** 2 / 64 - 5 * e2 ** 3 / 256) * phi - (3 * e2 / 8 + 3 * e2 ** 2 / 32 + 45 * e2 ** 3 / 1024) * Math.sin(2 * phi)
    + (15 * e2 ** 2 / 256 + 45 * e2 ** 3 / 1024) * Math.sin(4 * phi) - (35 * e2 ** 3 / 3072) * Math.sin(6 * phi));
  const x = k0 * N * (A + (1 - T + C) * A ** 3 / 6 + (5 - 18 * T + T * T + 72 * C - 58 * ep2) * A ** 5 / 120) + 500000;
  const y = k0 * (M + N * Math.tan(phi) * (A * A / 2 + (5 - T + 9 * C + 4 * C * C) * A ** 4 / 24 + (61 - 58 * T + T * T + 600 * C - 330 * ep2) * A ** 6 / 720)) + 10000000;
  return [x - ORIGIN_E, y - ORIGIN_N];
}

// ---- Terrain (AVT1, as tools/design_course.mjs) ----
function readAvt(path) {
  let b = fs.readFileSync(path);
  if (path.endsWith('.gz')) b = zlib.gunzipSync(b);
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

// ---- Overpass ----
async function overpass(q) {
  for (let attempt = 0; attempt < 4; attempt++) {
    try {
      const r = await fetch(OVERPASS, { method: 'POST', body: 'data=' + encodeURIComponent(q), headers: { 'content-type': 'application/x-www-form-urlencoded', 'user-agent': 'golf-dynamics-course-import' } });
      if (r.ok) return await r.json();
      console.error('overpass', r.status);
    } catch (e) { console.error('overpass', e.message); }
    await new Promise((res) => setTimeout(res, 5000 * (attempt + 1)));
  }
  throw new Error('overpass failed');
}

async function fetchCourse() {
  if (args.osm && fs.existsSync(args.osm)) return JSON.parse(fs.readFileSync(args.osm, 'utf8'));
  const esc = NAME.replace(/"/g, '\\"');
  const q = `[out:json][timeout:120];
(way["leisure"="golf_course"]["name"="${esc}"](-24.1,-47.2,-23.2,-46.2);relation["leisure"="golf_course"]["name"="${esc}"](-24.1,-47.2,-23.2,-46.2);)->.gc;
.gc out geom;
.gc map_to_area->.a;
(way(area.a)["golf"];way(area.a)["natural"~"^(water|sand|wood)$"];relation(area.a)["natural"="water"];node(area.a)["natural"="tree"];);
out geom;`;
  const d = await overpass(q);
  if (args.osm) fs.writeFileSync(args.osm, JSON.stringify(d));
  return d;
}

// ---- Geometry ----
const r1 = (v) => Math.round(v * 10) / 10;
const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);
function area(poly) { let s = 0; for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) s += poly[j][0] * poly[i][1] - poly[i][0] * poly[j][1]; return Math.abs(s) / 2; }
function centroid(poly) {
  let a = 0, cx = 0, cz = 0;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) { const f = poly[j][0] * poly[i][1] - poly[i][0] * poly[j][1]; a += f; cx += (poly[j][0] + poly[i][0]) * f; cz += (poly[j][1] + poly[i][1]) * f; }
  return Math.abs(a) < 1e-9 ? poly[0] : [cx / (3 * a), cz / (3 * a)];
}
function inside(poly, p) {
  let c = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const a = poly[i], b = poly[j];
    if ((a[1] > p[1]) !== (b[1] > p[1]) && p[0] < (b[0] - a[0]) * (p[1] - a[1]) / (b[1] - a[1]) + a[0]) c = !c;
  }
  return c;
}
function edgeDist(poly, p) {
  let d = Infinity;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const a = poly[i], b = poly[j], vx = b[0] - a[0], vz = b[1] - a[1], L = vx * vx + vz * vz;
    const t = L > 0 ? Math.min(Math.max(((p[0] - a[0]) * vx + (p[1] - a[1]) * vz) / L, 0), 1) : 0;
    d = Math.min(d, Math.hypot(p[0] - a[0] - t * vx, p[1] - a[1] - t * vz));
  }
  return d;
}
function segDist(p, a, b) {
  const vx = b[0] - a[0], vz = b[1] - a[1], L = vx * vx + vz * vz;
  const t = L > 0 ? Math.min(Math.max(((p[0] - a[0]) * vx + (p[1] - a[1]) * vz) / L, 0), 1) : 0;
  return Math.hypot(p[0] - a[0] - t * vx, p[1] - a[1] - t * vz);
}
const lineDist = (line, p) => { let d = Infinity; for (let k = 1; k < line.length; k++) d = Math.min(d, segDist(p, line[k - 1], line[k])); return d; };
// Douglas-Peucker, to keep the polygons light (tolerance in metres).
function simplify(pts, tol) {
  if (pts.length < 4) return pts;
  const keep = new Uint8Array(pts.length); keep[0] = keep[pts.length - 1] = 1;
  const stack = [[0, pts.length - 1]];
  while (stack.length) {
    const [i, j] = stack.pop(); let best = -1, bd = 0;
    for (let k = i + 1; k < j; k++) { const d = segDist(pts[k], pts[i], pts[j]); if (d > bd) { bd = d; best = k; } }
    if (bd > tol) { keep[best] = 1; stack.push([i, best], [best, j]); }
  }
  return pts.filter((_, k) => keep[k]);
}
function rng(seed) { let s = seed >>> 0; return () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296); }

// Stitch a relation's outer member ways into one ring.
function stitch(members) {
  const ways = members.filter((m) => m.type === 'way' && (m.role === 'outer' || m.role === '') && m.geometry).map((m) => m.geometry.map((g) => [g.lat, g.lon]));
  if (!ways.length) return [];
  const ring = ways.shift();
  const same = (a, b) => Math.abs(a[0] - b[0]) < 1e-9 && Math.abs(a[1] - b[1]) < 1e-9;
  let guard = 0;
  while (ways.length && guard++ < 10000) {
    const end = ring[ring.length - 1];
    const k = ways.findIndex((w) => same(w[0], end) || same(w[w.length - 1], end));
    if (k < 0) break;
    const w = ways.splice(k, 1)[0];
    if (!same(w[0], end)) w.reverse();
    ring.push(...w.slice(1));
  }
  return ring;
}

// ---- Build ----
const osm = await fetchCourse();
const els = osm.elements || [];
const gc = els.find((e) => e.tags && e.tags.leisure === 'golf_course');
if (!gc) throw new Error(`no leisure=golf_course named "${NAME}"`);
const H = args.terrain ? readAvt(args.terrain) : null;
const ringLL = gc.type === 'way' ? gc.geometry.map((g) => [g.lat, g.lon]) : stitch(gc.members || []);
const ringSite = ringLL.map(([la, lo]) => utm23s(la, lo));
let E0 = 0, N0 = 0;
{ const xs = ringSite.map((p) => p[0]), ys = ringSite.map((p) => p[1]); E0 = Math.round((Math.min(...xs) + Math.max(...xs)) / 2); N0 = Math.round((Math.min(...ys) + Math.max(...ys)) / 2); }
const toCourse = (la, lo) => { const [e, n] = utm23s(la, lo); return [e - E0, -(n - N0)]; };
const geo = (e) => (e.geometry || []).map((g) => toCourse(g.lat, g.lon));
const ring = (e) => { let p = geo(e); if (p.length > 2 && dist(p[0], p[p.length - 1]) < 0.01) p = p.slice(0, -1); return p; };
const boundary = simplify(ringSite.map(([e, n]) => [e - E0, -(n - N0)]), 1.0);
const tag = (e, k) => (e.tags || {})[k];

const holeWays = els.filter((e) => e.type === 'way' && tag(e, 'golf') === 'hole');
if (!holeWays.length) throw new Error('no golf=hole ways mapped on this course');
const polysOf = (pred) => els.filter((e) => e.type === 'way' && pred(e)).map((e) => ({ e, p: simplify(ring(e), 0.4) })).filter((x) => x.p.length >= 3);
const greens = polysOf((e) => tag(e, 'golf') === 'green');
const tees = polysOf((e) => tag(e, 'golf') === 'tee');
const fairways = polysOf((e) => tag(e, 'golf') === 'fairway');
const bunkers = polysOf((e) => tag(e, 'golf') === 'bunker' || tag(e, 'natural') === 'sand');
const waters = polysOf((e) => ['water_hazard', 'lateral_water_hazard'].includes(tag(e, 'golf')) || tag(e, 'natural') === 'water');
const paths = els.filter((e) => e.type === 'way' && ['cartpath', 'path'].includes(tag(e, 'golf'))).map((e) => ({ points: simplify(geo(e), 0.5).map(([x, z]) => [r1(x), r1(z)]), half_width: 1.3 }));
const treeNodes = els.filter((e) => e.type === 'node' && tag(e, 'natural') === 'tree');

// Holes, in order of their number.
const holes = holeWays.map((e) => ({ e, line: simplify(geo(e), 1.0), ref: parseInt(tag(e, 'ref'), 10) })).filter((h) => h.line.length >= 2)
  .sort((a, b) => (a.ref || 99) - (b.ref || 99));
const nearestHole = (p) => { let best = 0, bd = Infinity; holes.forEach((h, i) => { const d = lineDist(h.line, p); if (d < bd) { bd = d; best = i; } }); return [best, bd]; };
const out = holes.map((h, i) => {
  const L = h.line.reduce((s, p, k) => (k ? s + dist(h.line[k - 1], p) : 0), 0);
  const par = parseInt(tag(h.e, 'par'), 10) || (L < 230 ? 3 : L < 430 ? 4 : 5);
  return { number: h.ref || i + 1, name: tag(h.e, 'name') || '', par, stroke_index: parseInt(tag(h.e, 'handicap'), 10) || 0, line: h.line.map(([x, z]) => [r1(x), r1(z)]),
    tees: [], pins: [], fairway: [], fairway_polygons: [], bunkers: [], water: [], rough_width: 12, _len: L };
});
// Stroke index when not mapped: longest holes hardest.
if (out.some((h) => !h.stroke_index)) [...out].sort((a, b) => b._len - a._len).forEach((h, k) => { h.stroke_index = k + 1; });

// Greens: the one at the end of each hole's line.
const rand = rng(1554);
out.forEach((h) => {
  const end = h.line[h.line.length - 1];
  let g = null, bd = Infinity;
  for (const c of greens) { const d = inside(c.p, end) ? 0 : edgeDist(c.p, end); if (d < bd) { bd = d; g = c; } }
  if (!g || bd > 40) throw new Error(`hole ${h.number}: no green near the end of its line`);
  const c = centroid(g.p);
  const pts = g.p.map(([x, z]) => [r1(x), r1(z)]);
  // A gentle back-to-front tilt and two swells (the map has no contours).
  const L = h.line.length, a = h.line[L - 2], b = h.line[L - 1], dx = b[0] - a[0], dz = b[1] - a[1], dl = Math.hypot(dx, dz) || 1;
  const swell = () => { for (let k = 0; k < 40; k++) { const p = [c[0] + (rand() - 0.5) * 20, c[1] + (rand() - 0.5) * 20]; if (inside(g.p, p) && edgeDist(g.p, p) > 4) return { x: r1(p[0]), z: r1(p[1]), r: 4.5 + rand() * 1.5, h: r1((rand() - 0.4) * 0.25 * 100) / 100 }; } return null; };
  h.green = { x: r1(c[0]), z: r1(c[1]), polygon: pts, raise: 0.3, tilt_x: r1((dx / dl) * 0.015 * 1000) / 1000, tilt_z: r1((dz / dl) * 0.015 * 1000) / 1000, bumps: [swell(), swell()].filter(Boolean) };
  // Hole positions: three spread over the green, at least 4 m from its edge.
  const cand = [];
  for (let k = 0; k < 400 && cand.length < 60; k++) { const p = [c[0] + (rand() - 0.5) * 50, c[1] + (rand() - 0.5) * 50]; if (inside(g.p, p) && edgeDist(g.p, p) >= 4) cand.push(p); }
  if (!cand.length) cand.push(c);
  const pick = [cand[0]];
  while (pick.length < 3 && pick.length < cand.length) { let best = null, bd2 = -1; for (const p of cand) { const d = Math.min(...pick.map((q) => dist(p, q))); if (d > bd2) { bd2 = d; best = p; } } pick.push(best); }
  h.pins = pick.map(([x, z]) => [r1(x), r1(z)]);
  g.used = true;
});
// Tees: those near the start of each hole, named by distance from the green.
out.forEach((h) => {
  const start = h.line[0], end = h.line[h.line.length - 1];
  const near = tees.filter((t) => { const c = centroid(t.p); return dist(c, start) < 70 && nearestHole(c)[0] === out.indexOf(h); })
    .map((t) => ({ t, c: centroid(t.p), d: dist(centroid(t.p), end) })).sort((a, b) => b.d - a.d);
  const names = near.length >= 3 ? ['blue', 'white', 'red'] : near.length === 2 ? ['blue', 'white'] : ['white'];
  const box = (c, t) => { const s = Math.sqrt(area(t.p)); return { x: r1(c[0]), z: r1(c[1]), half_width: Math.max(2.5, Math.min(8, s / 2.4)), half_length: Math.max(3, Math.min(12, s / 2)) }; };
  h.tees = near.slice(0, names.length).map(({ t, c }, k) => ({ name: names[k], ...box(c, t) }));
  if (near.length === 2) h.tees.push({ name: 'red', ...box(near[1].c, near[1].t) });
  if (!h.tees.length) h.tees = [{ name: 'white', x: start[0], z: start[1] }];
  if (!h.tees.some((t) => t.name === 'white')) h.tees[0].name = 'white';
});
// Fairways, bunkers and water: to the hole whose line they are nearest.
for (const f of fairways) { const [i] = nearestHole(centroid(f.p)); out[i].fairway_polygons.push(f.p.map(([x, z]) => [r1(x), r1(z)])); }
for (const b of bunkers) { const [i, d] = nearestHole(centroid(b.p)); if (d < 120) out[i].bunkers.push({ polygon: b.p.map(([x, z]) => [r1(x), r1(z)]), depth: area(b.p) > 400 ? 0.6 : 0.85 }); }
for (const w of waters) {
  const [i, d] = nearestHole(centroid(w.p));
  if (d > 250 || area(w.p) < 30) continue;
  out[i].water.push({ polygon: w.p.map(([x, z]) => [r1(x), r1(z)]), colour: tag(w.e, 'golf') === 'water_hazard' ? 'yellow' : 'red' });
}
const fixedTrees = treeNodes.map((n) => { const [x, z] = toCourse(n.lat, n.lon); return { x: r1(x), z: r1(z), kind: 0, height: parseFloat(tag(n, 'height')) || 12 }; })
  .filter((t) => out.every((h) => lineDist(h.line, [t.x, t.z]) > 18));
out.forEach((h) => { delete h._len; });

// Bounds: the course and a margin; the ground from the city grid.
const xs = boundary.map((p) => p[0]), zs = boundary.map((p) => p[1]);
const B = [Math.floor(Math.min(...xs) - 60), Math.floor(Math.min(...zs) - 60), Math.ceil(Math.max(...xs) + 60), Math.ceil(Math.max(...zs) + 60)];
let grid = null, lo = 760;
if (H) {
  const cell = 25, gx0 = B[0] - 50, gz0 = B[1] - 50;
  const nx = Math.ceil((B[2] - gx0 + 50) / cell) + 1, nz = Math.ceil((B[3] - gz0 + 50) / cell) + 1;
  const raw = [];
  lo = Infinity;
  for (let j = 0; j < nz; j++) for (let i = 0; i < nx; i++) { const v = H(E0 + gx0 + i * cell, N0 - (gz0 + j * cell)); raw.push(v); lo = Math.min(lo, v); }
  const offset = Math.floor(lo);
  grid = { x0: gx0, z0: gz0, cell, nx, nz, scale: 0.1, offset, h: raw.map((v) => Math.round((v - offset) * 10)) };
}
const totalPar = out.reduce((s, h) => s + h.par, 0);
const course = {
  id: ID,
  name: NAME,
  description: `${out.length} holes, par ${totalPar}, as mapped on OpenStreetMap: tees, fairways, greens, bunkers and water from the map, on SRTM ground. Green contours and hole positions are not mapped and are generated.`,
  location: `São Paulo, Brazil (site frame east ${E0} m, north ${N0} m)`,
  attribution: 'Course layout © OpenStreetMap contributors (ODbL 1.0), imported by tools/osm_course.mjs. Ground: SRTM 1 arc-second (NASA/USGS). Not affiliated with or endorsed by the club.',
  site: { east: E0, north: N0 },
  conditions: { altitude_m: Math.round(lo + 10), temperature_c: 23, humidity: 0.68 },
  wind: { speed: 3.0, toward_deg: 200, roughness_m: 0.2, gust_intensity: 0.18, gust_time_s: 2.5 },
  stimp_ft: 10.5,
  bounds: B,
  cell: 1.0,
  surface_cell: 0.5,
  boundary: boundary.map(([x, z]) => [r1(x), r1(z)]),
  terrain: { base: 0, noise_amp: 0.5, noise_scale: 55, seed: 1901, ...(grid ? { grid } : {}), mounds: [] },
  trees: { seed: 1901, spacing: 12, clearance: 6, corridor_extra: 4, fixed: fixedTrees.slice(0, 3000) },
  paths,
  holes: out,
};
process.stdout.write(JSON.stringify(course) + '\n');
console.error(`${course.name}: ${out.length} holes, par ${totalPar}; ${fairways.length} fairways, ${bunkers.length} bunkers, ${waters.length} water, ${fixedTrees.length} mapped trees; bounds ${B.join(' ')}`);
