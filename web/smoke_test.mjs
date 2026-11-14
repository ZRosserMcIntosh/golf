// Golf Dynamics — smoke test of the WebAssembly module in Node.
// SPDX-License-Identifier: MIT
// Usage: node web/smoke_test.mjs [dist-dir]   (after web/build_wasi.sh)
// Loads the course, plays a few holes with two computer golfers, checks the
// round replays exactly from its saved inputs, and writes the saved round and
// final state to dist/smoke_round.json for the native cross-check
// (golfdemo replay).
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const dist = path.resolve(process.argv[2] || new URL('./dist', import.meta.url).pathname);
const { default: createGolfModule } = await import(pathToFileURL(path.join(dist, 'golf.mjs')).href);
const G = await createGolfModule();
let failures = 0;
const check = (ok, msg) => { console.log(`${ok ? 'ok  ' : 'FAIL'} ${msg}`); if (!ok) failures++; };

let t0 = performance.now();
const info = G.loadCourse(fs.readFileSync(path.join(dist, 'varzea.json'), 'utf8'));
check(info.holes.length === 9, `course loaded: ${info.name}, ${info.holes.length} holes, ${info.trees} trees (${(performance.now() - t0).toFixed(0)} ms)`);
const h = G.heights(), s = G.surfaces();
check(h.length === info.height.nx * info.height.nz && s.length === info.surface.nx * info.surface.nz, 'rasters readable');
check(Math.abs(G.heightAt(info.holes[0].tees[1].pos[0], info.holes[0].tees[1].pos[2]) - info.holes[0].tees[1].pos[1]) < 0.01, 'tee on its pad');

const cfg = { seed: '20261114', holes: [0, 1, 2], players: [
  { name: 'Ana', golfer: 'tour', ai: true }, { name: 'Bruno', golfer: 'mid', ai: true, handicap: 14 }] };
let st = G.newMatch(cfg);
let shots = 0, planMs = 0, playMs = 0;
while (!st.finished && shots < 120) {
  t0 = performance.now(); const plan = G.plan(); planMs += performance.now() - t0;
  t0 = performance.now(); const r = G.play(plan.input); playMs += performance.now() - t0;
  st = r.state; shots++;
  if (shots === 1) check(G.path().length > 50 && r.shot.events.length >= 3, `tee shot: ${r.shot.carry.toFixed(1)} m carry, ${r.shot.total.toFixed(1)} m total, ${r.shot.events.length} events`);
}
check(st.finished, `three holes in ${shots} shots; plan ${(planMs / shots).toFixed(0)} ms, play ${(playMs / shots).toFixed(0)} ms a shot`);
console.log('     ' + st.players.map((p) => `${p.name} ${p.card.slice(0, 3).join(' ')} (${p.to_par >= 0 ? '+' : ''}${p.to_par})`).join(', '));
const saved = G.save();
const replayed = G.loadMatch(saved);
check(JSON.stringify(replayed) === JSON.stringify(st), 'the round replays exactly from its saved inputs');
fs.writeFileSync(path.join(dist, 'smoke_round.json'), JSON.stringify({ saved, state: st }));
const clubs = G.clubs('tour');
check(clubs.clubs.length === 14 && clubs.clubs[0].carry > 230, `bag: driver carries ${clubs.clubs[0].carry.toFixed(0)} m for a tour pro at ${info.altitude} m`);
process.exit(failures ? 1 : 0);
