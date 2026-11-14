// Golf Dynamics — compare a round computed by the WebAssembly module with the
// native replay of the same inputs (golfdemo replay). Every browser runs the
// same .wasm, so players in a multiplayer round agree bit for bit; native
// builds use a different maths library (glibc against wasi-libc), so they agree
// to rounding: scores identical, positions within a micrometre.
// SPDX-License-Identifier: MIT
// Usage: node web/cross_check.mjs web/dist/smoke_round.json native_state.json
import fs from 'node:fs';
const w = JSON.parse(fs.readFileSync(process.argv[2], 'utf8')).state;
const n = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
let worst = 0, ok = w.players.length === n.players.length && w.shots === n.shots && w.finished === n.finished;
w.players.forEach((p, i) => {
  const q = n.players[i];
  ok &&= JSON.stringify(p.card) === JSON.stringify(q.card) && p.strokes === q.strokes && p.surface === q.surface;
  for (let k = 0; k < 3; k++) worst = Math.max(worst, Math.abs(p.pos[k] - q.pos[k]));
});
ok &&= worst < 1e-6;
console.log(`${ok ? 'ok  ' : 'FAIL'} native and WebAssembly agree: same scores, positions within ${worst.toExponential(1)} m`);
process.exit(ok ? 0 : 1);
