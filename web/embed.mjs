// Golf Dynamics — generate the embedded data source (as cmake/embed.cmake) for the WebAssembly build.
// SPDX-License-Identifier: MIT
import fs from 'node:fs';
const [dir, out] = process.argv.slice(2);
const pairs = [['clubsStandard', 'clubs/standard.json'], ['surfaces', 'surfaces/surfaces.json'], ['balls', 'balls/balls.json'], ['golfers', 'golfers/golfers.json']];
let s = '// Generated from data/ by web/embed.mjs. Do not edit.\n#include "golf/embedded.hpp"\nnamespace golf::embedded {\n';
for (const [fn, file] of pairs) s += `const char* ${fn}() {\n    return R"golfjson(${fs.readFileSync(`${dir}/${file}`, 'utf8')})golfjson";\n}\n`;
s += '}  // namespace golf::embedded\n';
fs.writeFileSync(out, s);
