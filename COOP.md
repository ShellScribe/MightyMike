# Couch co-op V1

## Launch

Connect two SDL-compatible gamepads, then double-click `Play-Coop.command` on
macOS, or run the executable with `--coop`:

```sh
./build-release/MightyMike.app/Contents/MacOS/MightyMike --coop
```

On Windows/Linux, run `MightyMike.exe --coop` / `./MightyMike --coop`.
Co-op starts a fresh campaign directly, skipping the title/player-selection
menus. Without `--coop`, the original menus, single-player, and alternating
two-player game remain available. This build is a candidate for human playtesting;
it has not been accepted by two people using physical controllers.

## Controls and rules

- Left stick or D-pad: move. Right stick: aim in eight directions.
- Right trigger or west face button (X on Xbox): shoot. Tap repeatedly for
  suction cups; automatic weapons fire while held, matching the original game.
- Shoulder buttons: previous/next weapon. East face button also selects next.
- North face button or Back: bunny radar. Start: pause/quit dialog.
- First controller detected owns P1; second owns P2. A connected controller
  retains its slot when its partner disconnects; reconnect fills the vacant slot.
  P1 also accepts the normal keyboard/mouse controls.
- P1/P2 labels identify Pete. The upper-left overlay shows separate health bars.
  Weapons/ammo and power-ups belong to the player who collects them. The original
  weapon icon/ammo display shows **P1's** inventory; P2's weapon is visible when
  firing but has no inventory HUD in V1.
- Bunnies, keys, coins, score and spare lives are shared. Either player can collect
  the final bunny; both perform the exit animation.
- While both players are alive, the camera follows their midpoint and a movement
  tether keeps both on screen. With one dead player, the camera follows the
  survivor and releases the tether to the dead partner.
  Teleporters take both players to the destination.
- A dead player respawns at the partner's last safe location with two seconds of
  invulnerability if a shared spare remains. With no spare, the survivor can keep
  playing. If both die with a spare, one spare revives both; otherwise game over.
  Finishing a level brings a fallen partner into the next level.
- Co-op saving/loading is disabled and the bonus save selection is skipped.
  Existing single-player save files are not overwritten by a co-op run.

## Build

The normal prerequisites and dependency downloads are described in `BUILD.md`.
With Python, CMake, and Xcode installed, `python3 build.py` remains supported.
For macOS Command Line Tools, after `python3 build.py -1` prepares SDL:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build-release -j 8
```

Use `x86_64` for an Intel Mac, or `arm64;x86_64` for a universal build. The build
copies game data and embeds SDL into the app, including with Makefiles. This
workspace's executable is arm64, targeting macOS 14+. CMake was obtained in
`/private/tmp/mightymike-build-tools/cmake/data/bin/cmake`; that temporary tool
can be replaced with any installed CMake 3.21+.

## Validation

A small optional test uses **real SDL virtual gamepads**, assets, player movement,
weapons, enemy collision code, death and exit animations. No mocks or test service.
Disconnect physical gamepads before running it:

```sh
cmake -S . -B build-coop -DCOOP_SMOKE_TESTS=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build-coop -j 8
SDL_AUDIODRIVER=dummy ./build-coop/MightyMike.app/Contents/MacOS/MightyMike --coop-smoke
```

The test briefly opens a window and exits nonzero on failure. Dummy audio is for
unattended tests; it does not validate audible playback. The test freezes enemy
movement in a clear patch of the real first map to isolate player behavior, and
uses a nonlethal enemy fixture and final-bunny fixture for combat/completion.

See `tests/RESULTS.md` for executed results and the remaining manual checklist.
The screenshot `/private/tmp/mightymike-coop-smoke.ppm` is a rendered test frame.

## Scope and implementation notes

`PlayerState` contains independent avatar, movement, health, transformation,
shield, inventory and firing state. Input is sampled into `PlayerInput` once per
simulation tick; gameplay reads snapshots rather than querying a device itself.
The simulation remains 32 Hz with the existing rendering interpolation.

The object dispatcher brackets each move **and animation** with one explicit
compatibility scope. Owned avatars/shields/flames load their owner's state;
other objects use the nearest living player's state for legacy AI/proximity
behavior. Every exit path saves that state and restores P1 globals and UI input.
Keys/objectives, world objects, enemy freeze, bullet count and economy stay shared.
Death animation flags are redirected to each player's record; the outer death
flag is set only for team game-over. Rendering runs after scopes have closed.

Risk: new mutable player globals, new player-following effects, or new callbacks
must be added to this adapter/ownership tagging. It is an interim compatibility
layer rather than an engine-wide conversion. Nearby AI targeting is deliberately
simple. The existing shared object/projectile limits still apply.

Deferred/unverified: physical-controller feel, audible audio, full campaign
playthrough (particularly frog/ship transformations, moving platforms and
teleporters), P2 inventory HUD, co-op saves, and polished join/rebind menus.
The first-level vertical slice and next-level setup receive automated coverage;
full campaign support is available in the code but is not claimed validated.
No networking, split-screen, new artwork, or broader engine rewrite.

## Playtest follow-ups

- Add an in-game screen to assign and map both controllers independently.
  Include calibration and controls for controllers without a right stick.
  This is noted for a later increment, not implemented in V1.
- Owner reported basic play working with Switch Pro + N64, then P2 confined
  after P1 died. Reproduced a dead-partner tether defect and changed camera/
  tether rules for a sole survivor. Physical confirmation of that fix remains
  pending; the precise playtest location/respawn situation has not been captured.
