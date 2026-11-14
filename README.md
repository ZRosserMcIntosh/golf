# Golf Dynamics

A physically grounded golf simulation core in C++20. It is the golf game of
São Paulo World ([zrossermcintosh.com/sao-paulo/golf/play](https://zrossermcintosh.com/sao-paulo/golf/play/)).

- It has no renderer and no game-engine dependency.
- Balls, clubs, surfaces, golfers and courses are JSON, and every parameter
  records where it came from.
- The same core runs natively and in the browser as WebAssembly. Every
  browser computes a shot bit for bit the same, so multiplayer sends only
  each player's swing.

## What it simulates

| Area | Model | Source |
|---|---|---|
| Air | Density by the CIPM-2007 moist-air equation, from altitude (ISA), temperature and humidity. Viscosity by Sutherland's law. | Picard et al. 2008; U.S. Standard Atmosphere 1976 |
| Aerodynamics | Drag and lift as functions of Reynolds number and spin parameter: a drag crisis, spin drag that saturates, lift that saturates. Spin decays about 4 %/s. Alternative: the quadratic launch-monitor fit. | dimple "TOUR" (MIT, see NOTICE); Ferguson, McNally & McPhee 2022 |
| Wind | Log-law profile over the course's roughness, measured from the local ground. Gusts are Ornstein–Uhlenbeck, seeded. | Stull 1988; Wieringa 1992 |
| Flight | RK4 at 1 ms, relative airspeed, Magnus lift across the spin axis. Trees (canopies and trunks) and the flagstick in the way. | |
| Impact | A 3-D rigid-body collision of ball and club head. Covers head mass, both moments of inertia, CG depth and height, and face bulge and roll. Uses Cross's tangential restitution with a Coulomb limit. The D-plane and gear effect emerge from the geometry. Grass between face and ball cuts spin (flyers). | Cross 2005; Penner 2001, 2003 |
| Bounce | Penner's turf model in 3-D: speed-dependent restitution, an effective crater angle, friction, and pivot friction on spin about the normal. | Penner 2002; dimple |
| Roll | Exact slide-to-roll transition. Rolling resistance with a quadratic speed law, set from a Stimpmeter reading in closed form. Slopes hold the ball or let it roll. | Holmes 1986; Penner 2002; dimple |
| The hole | A rigid-body simulation at the cup: rim, wall, bottom and flagstick. The ball drops when its centre is over the hole. "Holed" follows the Rules (whole ball below the surface). | Holmes 1991; Penner 2002 |
| Surfaces | Tee, fairway, fringe, green, rough, deep rough, bunker (plugging), cart path, native areas, water (with skipping). Each sets its bounce, roll, and lie effects (club speed, face friction, scatter). | `data/surfaces/surfaces.json` |
| Courses | Shapes laid on real ground (SRTM): tees, fairways along the line of play, contoured greens, bunkers, lakes. Built into 1 m height and 0.5 m surface rasters that the physics and the renderer share. Trees are scattered from a seed. | `data/courses/varzea.json`, `tools/design_course.mjs` |
| Rules | Honour and order of play; penalty areas (lateral and back-on-the-line relief); out of bounds and lost ball (stroke and distance, or Model Local Rule E-5); maximum score; gimmes. | Rules of Golf 2023 |
| Scoring | Stroke play, net Stableford, match play, and handicaps under the World Handicap System (strokes by stroke index, pro rata for nine holes). | |
| Golfers | Four skill profiles (tour, scratch, 14, 25): club-speed factor and scatter in face, path (correlated), strike, speed and loft, plus putting scatter. | `data/golfers/golfers.json` |
| The computer golfer | Picks club and target by minimising expected strokes against strokes-gained baselines, with lateral risk. Corrects aim for wind by simulating the shot. Solves pace and line for putts and chips by bisection. Opens the face out of bunkers. | Broadie 2014 |

## Validation

Every number below is checked by the test suite (`tests/`).

**Flight against the TrackMan PGA Tour averages** (measured launch in,
sea level, 25 °C):

| Club | Carry (yd) | Height (yd) | Landing angle |
|---|---|---|---|
| Driver | 274.3 vs 275 | 36.8 vs 32 | 40.4° vs 38° |
| 3-wood | 246.5 vs 243 | 34.2 vs 30 | 45.9° vs 43° |
| 5-wood | 232.0 vs 230 | 34.2 vs 31 | 47.7° vs 47° |
| Hybrid | 221.8 vs 225 | 32.8 vs 29 | 47.3° vs 47° |
| 3-iron | 214.1 vs 212 | 31.3 vs 27 | 46.7° vs 46° |
| 4-iron | 204.7 vs 203 | 30.0 vs 28 | 46.2° vs 48° |
| 5-iron | 194.2 vs 194 | 30.0 vs 31 | 46.9° vs 49° |
| 6-iron | 183.0 vs 183 | 31.4 vs 30 | 48.8° vs 50° |
| 7-iron | 168.9 vs 172 | 30.9 vs 32 | 49.4° vs 50° |
| 8-iron | 158.5 vs 160 | 30.5 vs 31 | 49.9° vs 50° |
| 9-iron | 147.1 vs 148 | 29.8 vs 30 | 50.3° vs 51° |
| PW | 133.8 vs 136 | 30.1 vs 29 | 51.7° vs 52° |

Carry RMSE is 2.1 yd.

**Impact: the tour club delivery in, ball speed and spin out.** Only the
dynamic loft (solved to give the launch angle) and a head-speed calibration
(solved to give the ball speed) are fitted; spin is a prediction.

| Club | Spin, model vs TrackMan |
|---|---|
| Driver | 2796 vs 2686 rpm |
| 3-wood | 3825 vs 3655 |
| Hybrid | 4719 vs 4437 |
| 4-iron | 5280 vs 4836 |
| 5-iron | 5661 vs 5361 |
| 6-iron | 6448 vs 6231 |
| 7-iron | 7100 vs 7097 |
| 8-iron | 7750 vs 7998 |
| 9-iron | 8465 vs 8647 |
| PW | 9973 vs 9304 |

The rigid-head model under-predicts the smash factor of woods by about 4 %.
The speed calibration absorbs it, and the docs state it.

**Ground and the hole:**

| Check | Model | Reference |
|---|---|---|
| A ball rolled off the Stimpmeter, 8 / 10 / 13 ft | 8.00 / 10.00 / 13.00 ft | the reading |
| Drive run-out on a fairway | 17 yd | USGA/R&A (2021) bounce model: 14.5 yd |
| Fastest putt holed through the centre | 1.60 m/s | Penner (2002): 1.63 m/s |
| A high-spin wedge on a firm green | runs back | Penner (2002), Fig. 8 |
| Off-centre putts | must be slower to drop | Holmes (1991) |

**The golfers** (four rounds each on the Várzea course, computer play):

- The tour profile holes 97 % from 3 ft, 61 % from 6 ft, 47 % from 10 ft and
  20 % from 20 ft. Broadie's PGA Tour figures are 96, 70, 40 and 15 %.
- Tour: −0.5 per nine holes, 69 % greens in regulation.
- 14 handicap: +8.8 per nine.

**Determinism.** A round replays exactly from its configuration and shot
inputs. The WebAssembly and native builds agree to within a micrometre
(different maths libraries); every browser runs the same `.wasm`, so players
agree bit for bit.

## Layout

```
include/golf/   the core's headers (atmosphere, ball, wind, shot, impact, club, surface,
                cup, terrain, course, golfer, rules, match, json, rng)
src/            the core
data/           balls, clubs, surfaces, golfers, courses, validation data
tests/          the test suite (dependency-free harness)
tools/          golfdemo (validation tables, computer rounds, replay),
                design_course.mjs (the Várzea course from SRTM ground),
                relay/ (the live-multiplayer relay)
web/            the WebAssembly build: C ABI (golf_web.cpp), loader, smoke test
```

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/tests/golf_tests            # 36 test cases
./build/golfdemo validate           # the impact and flight table
./build/golfdemo round 7            # two computer golfers play the course
```

WebAssembly (wasi-sdk 25):

```sh
WASI_SDK=/path/to/wasi-sdk web/build_wasi.sh   # web/dist: golf.wasm, golf.mjs, varzea.json
node web/smoke_test.mjs
```

The São Paulo World page copies `web/dist/golf.wasm`, `golf.mjs` and
`varzea.json` into `sao-paulo/golf/play/`.

## Multiplayer

A round is its configuration plus its shot inputs. Players only ever exchange
those, never positions or scores.

- **Invite by link, taking turns.** The whole round travels in the link's
  `#g=` fragment, deflate-compressed: play your shots, then send the link on.
  It needs no server, and the fragment never reaches the web server.
- **Live rooms.** The players' pages exchange swings through
  `tools/relay/server.mjs`. The relay is a WebSocket server with no
  dependencies and no game state; it forwards each message to the others in
  the room.
  - Run it anywhere Node 18+ runs: `node tools/relay/server.mjs` (port
    `$PORT` or 8787). Optional `ALLOWED_ORIGINS=https://zrossermcintosh.com`.
  - Point the page at it with `window.GOLF_RELAY = 'wss://…'`, or
    `?relay=wss://…`. `?relay=local` plays between tabs of one browser.
  - A dropped player rejoins and is sent the round so far.

## Licence

MIT. NOTICE lists the parts derived from dimple (MIT) and the data sources.
