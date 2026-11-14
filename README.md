# Golf Dynamics

A physically grounded golf simulation core in C++20, built to be the golf
mini-game of the São Paulo open world. It has no renderer and no game-engine
dependency. Balls, clubs, surfaces, golfers and courses are JSON, and every
parameter records where it came from. The same core runs natively and in the
browser as WebAssembly.

Work in progress. See `docs/` (to come) for the validation tables.

## What it simulates

- **Flight.** RK4 with gravity, drag and Magnus lift.
  - The coefficients depend on Reynolds number and spin. The model is
    dimple's "TOUR" fit; Ferguson et al.'s 2022 launch-monitor fit is the
    alternative.
  - Spin decays in flight.
  - Air density is CIPM-2007, so altitude, temperature and humidity all
    count. Viscosity follows Sutherland's law.
  - The wind follows the log-law profile over the course's roughness, with
    seeded gusts.
  - Shots can hit trees (branches and trunks) and the flagstick.
- **Impact.** A three-dimensional rigid-body collision of ball and club head.
  - Head mass, moments of inertia and centre-of-gravity depth are modelled,
    along with the face's bulge and roll.
  - The ball rebounds with Cross's tangential restitution and Coulomb
    friction.
  - The D-plane and gear effect come out of the geometry; nothing is added
    for them.
  - Grass trapped between face and ball cuts the spin, producing flyers.
- **Bounce and roll.** Penner's (2002) turf model in 3-D, after dimple.
  - Slide-to-roll, then roll laws set by the Stimpmeter reading.
  - Slopes hold the ball or let it roll; putts break.
  - Water skips and hazards, sand plugging, hard cart paths.
- **The hole.** A rigid-body simulation at the cup: rim, wall, bottom and
  flagstick. Capture speed through the centre is 1.60 m/s (Penner 2002:
  1.63 m/s); lip-outs and dying putts drop.
- **The game.**
  - Courses: tees, fairways, contoured greens, bunkers and water, laid on
    real terrain.
  - The Rules of Golf: penalty areas, out of bounds and lost balls, honour
    and order of play.
  - Scoring: stroke play, net Stableford, match play and handicaps.
  - Golfers of four skill levels.
  - A computer golfer that plans with strokes-gained baselines and reads
    putts by simulating them.
  - Rounds replay exactly from their seed and shot inputs, which is how
    multiplayer works.

## Validation (so far)

| Check | Result |
|---|---|
| Carry, height and landing angle for 12 clubs against the TrackMan PGA Tour averages | carry RMSE 2.1 yd |
| Impact: ball speed and spin from the tour club delivery | spin within 10 % for all ten clubs (only launch angle and speed were fitted) |
| Stimpmeter: a ball rolled at 1.83 m/s | rolls its reading to 0.1 % |
| Drive run-out against the USGA/R&A (2021) bounce model | 17 yd against 14.5 yd |
| Centre capture speed of the hole | 1.60 m/s against Penner's 1.63 m/s |

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/tests/golf_tests
./build/golfdemo validate
./build/golfdemo round 7
```

## Licence

MIT. See NOTICE for the parts derived from dimple (MIT) and the data sources.
