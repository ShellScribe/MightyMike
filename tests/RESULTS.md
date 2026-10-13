# Executed validation — 2026-10-13

Host: Apple Silicon macOS; Apple Clang 21; CMake 4.4.4; SDL 3.2.2.

- **Unmodified baseline:** recursive upstream checkout built successfully,
  `build-baseline`. Baseline warnings: macOS 10.13/libc++ support and linker
  common-section alignment.
- **Fresh release build:** `build-release`, arm64, macOS 14+, smoke-test code off.
  No compiler warnings; existing linker alignment warning remains.
  Game data and SDL framework are embedded in the 23 MB app.
- **Automated runtime smoke:** `build-coop --coop-smoke`, exit 0. Two SDL virtual
  gamepads, real maps/assets, actual movement/combat/animation code, dummy audio.
  Passed independent movement; separate simultaneous aim, ammo and projectile
  direction; both projectiles damaging one enemy; restoration of player globals;
  independent damage; death animation and revival spending one shared spare;
  survivor continuity; tether and camera visibility; last-bunny collection and
  both exit animations; level completion; second-level setup; team game-over;
  stable controller assignment across disconnect/reconnect; and original
  single-player initialization and movement.
- **Visual inspection:** rendered first-level frame shows both Petes, P1/P2
  markers and separate health bars. This is a test frame, not human play.
- **Source hygiene:** `git diff --check` passed.

The test controls enemy motion, uses a nonlethal enemy fixture for two hits, and
collects a final-bunny fixture referencing a real map item. It exercises the exit
and next-level setup, not a complete human traversal or the bonus/menu sequence.
No claim of physical-controller, audible audio or full-campaign acceptance.

## Short local acceptance checklist

1. Connect two gamepads; run `Play-Coop.command`. Continue past the map/scene
   screens. Move one Pete at a time, then move/aim/shoot both in opposite directions.
2. Defeat enemies together. Take hits on each Pete; check the partner's health
   and weapon inventory stay intact. Pick up a weapon with P2 and switch weapons.
3. Separate horizontally/vertically until tethered; confirm both stay visible.
   Unplug/reconnect either gamepad and confirm the other keeps its player.
4. Kill one Pete with a spare; confirm revival near the partner and brief
   invulnerability. With no spares, confirm one survivor can continue and that
   both dying ends the game.
5. Collect every bunny in the first level normally. Confirm both exit, the bonus
   sequence finishes, and both appear in the next level.
6. Launch the app without `--coop`; check title/menu, original single-player and
   alternating two-player mode. If continuing through later worlds, check
   teleporters, moving platforms, frog and spaceship transformations.
