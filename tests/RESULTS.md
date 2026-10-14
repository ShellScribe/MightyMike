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
  both exit animations; level completion; second-level setup restoring both fallen players to health; team game-over;
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

## Physical-controller setup — 2026-10-13

- Switch Pro Controller detected by SDL3's built-in gamepad mapping.
- USB GameSir G7 HE initially appeared only as an unmapped joystick. Added a
  hardware-specific macOS mapping; SDL then recognized it as a gamepad.
- Owner-generated raw input confirmed left stick axes 0/1, right stick axes 2/3,
  right trigger axis 4, A=0, B=1, X=4, Y=3, LB=6, RB=7, View=10, Menu=11.
  The X/Y assignment differs from the related G7 family mapping.
- GameSir D-pad, left trigger, stick clicks and Guide remain unverified.
- Physical gameplay acceptance is still pending; setup/calibration is not a
  claim that movement, combat or level completion passed with physical devices.

## Owner playtest and dead-partner regression — 2026-10-13

- Owner played with Switch Pro (P1) and Nintendo N64 (P2), reporting that basic
  play seemed to work. Reported P2 confined to part of the level after P1 died.
  No full campaign/level-completion acceptance inferred from that report.
- Added focused tests placing a dead partner beyond the old tether radius while
  the survivor is inside the viewport. Before the fix, the suite exited 1:
  `FAIL: P2 can move beyond dead P1's tether`.
- Fixed player-distance limiting to ignore a dead partner and made the camera
  follow the sole survivor. The midpoint/tether behavior returns with two living
  players. Either survivor now passes the targeted check and the full smoke
  suite exits 0, including existing revival, combat and single-player checks.
- Physical devices stayed connected; this test process used
  `SDL_JOYSTICK_HIDAPI=0 SDL_JOYSTICK_IOKIT=0 SDL_JOYSTICK_MFI=0` and dummy audio
  to isolate its virtual devices. No system/controller settings changed.
- Release app rebuilt successfully; only the existing linker alignment warning.
- Owner subsequently confirmed P1 stayed dead with zero lives and play had
  worked normally until that point. This matches the reproduced dead-partner
  tether failure. Physical confirmation of the rebuilt fix remains pending.
- Follow-up noted: in-game assignment and mapping for both controllers.

## Continuation validation — 2026-10-14

Branch: `feature/couch-coop-complete`, continued from local `cbcaef8` with
`906e7f3` retained. Published `feature/couch-coop-v1-initial` was verified at
`14b670c53a9dddf8c06573d91f225ca93e5b6d49` before the continuation push.

Automated results (real SDL virtual devices, dummy audio):

- Final expanded `--coop-smoke`: exit 0; retained in
  `tests/continuation-smoke.log`. Earlier combat/movement/revival checks still pass.
- Independent captured shooting, button/diagonal aiming, unchanged legacy
  bindings, held-input suppression/rearming, and profile save/load round trip.
  Smoke profiles use an isolated temporary test file, not the owner's controls.
- An actual SDL **unmapped joystick** fixture works without an SDL database
  mapping: raw axis/hat movement, button aim, shooting, radar and pause; a missing
  or unconfigured device keeps the readiness gate closed. Its removal clears
  shooting/aim snapshots and preserves the other player's controller identity.
- Actual death animations with zero shared spares tested for **either** player:
  permanent death leaves a moving survivor, camera follows the survivor, the
  dead-partner tether is released, and the shared enemy-freeze timer still ticks.
  A subsequently available spare revives the fallen player.
- All **15 campaign area initializations** produce two owned avatars/objectives.
  Simultaneous frog/ship conversion, three simulation steps, hidden-avatar
  separation, warp completion, and disposal tested in their three respective
  world areas. These checks do not traverse all maps.
- A real map-backed teleporter trigger moves the team; collision boxes and
  momentum are immediately synchronized. Separate transformed-warp checks cover
  hidden Pete positions. A real platform sprite fixture tests the carry/collision
  routine for both Petes with a controlled location/delta; full path traversal
  remains a physical check.
- Final bunny pickup by P2, both real exit animations, the real bonus screen
  (including 201 shared coins), shared score and **both** max-health rewards,
  skipped co-op save prompt, subsequent level initialization, and team game-over.
- Original single-player startup/movement and original alternating-player mode
  initialization still pass. Complete alternating turns/menu play remain manual.

Native setup UI verification with `--coop-setup-smoke`:

- Keyboard navigation, actual virtual axis/button capture with displayed raw
  input identity, Backspace retry, wizard cancel, distinct device swap and swap
  back, and separate readiness indicators were exercised through the native UI.
- Final screen visually inspected after font/selection improvements. Final UI
  run exits 0, asserting unchanged simulation frame count/health while setup is
  open and requiring both players ready before return. This is virtual input,
  not physical controller acceptance.
- Test-only F5/F6 drive the two virtual Start buttons; F7 drives virtual P1 shoot;
  F8 drives its raw X axis. These hooks are absent from release builds.

Delivery: release app rebuilt with smoke code disabled, Apple Silicon arm64,
macOS 14+, embedded game assets and SDL framework. Only the inherited linker
common-section alignment warning remains. `git diff --check` passed.

Owner-reported physical evidence remains the earlier Switch Pro/N64 basic play
report above. The continuation's calibration, disconnect/reconnect behavior,
button aiming, audible playback, and zero-spare fix still require physical
confirmation. A complete two-person campaign is not claimed accepted.

Focused physical checklist:

1. Verify the two device names/assignments, capture N64 movement and C-button
   aiming without guessing button numbers, and test reset/cancel/save/relaunch.
2. Verify both readiness gates; unplug either controller while moving/firing.
   Confirm enemies pause, the other assignment stays put, and reconnection plus
   readiness resumes without stale movement or firing.
3. Verify both HUD inventories after pickups/damage. Test zero-spare permanent
   death and survivor freedom for P1 and P2.
4. Traverse actual teleporter destinations/platform paths, transformations,
   final-bunny exits, bonus screens and subsequent levels together. Check audio.
5. Launch without `--coop` and play single-player and alternating turns normally.
