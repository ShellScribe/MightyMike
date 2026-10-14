# Couch co-op continuation

## Launch

Connect two SDL-compatible gamepads, then double-click `Play-Coop.command` on
macOS, or run the executable with `--coop`:

```sh
./build-release/MightyMike.app/Contents/MacOS/MightyMike --coop
```

On Windows/Linux, run `MightyMike.exe --coop` / `./MightyMike --coop`.
Co-op starts a fresh campaign directly, skipping the title/player-selection
menus. Without `--coop`, the original menus, single-player, and alternating
two-player game remain available. This continuation retains the zero-spare survivor fix. Automated checks and
owner-reported play are recorded separately in `tests/RESULTS.md`. Physical
acceptance of the continuation is still pending.

## Controls and rules

- Left stick or D-pad: move. Right stick: aim in eight directions.
- Right trigger or west face button (X on Xbox): shoot. Tap repeatedly for
  suction cups; automatic weapons fire while held, matching the original game.
- Shoulder buttons: previous/next weapon. East face button also selects next.
- North face button or Back: bunny radar. Start: pause/quit dialog.
- Two distinct devices are required. The first two detected devices are initially
  assigned P1/P2. Setup shows their names; selecting a device row cycles available
  devices, or swaps the two assignments. A disconnect never migrates the other
  controller into the missing player's slot. Reconnection/reassignment requires
  readiness before simulation resumes. P1 retains keyboard/mouse support.
- Each player has separate co-op bindings, independent of single-player settings.
  The compact top overlay shows separate health, selected weapon artwork, ammo,
  `fallen`/`reviving`/`revived` status, and shared spare lives. The legacy border
  still shows P1's weapon. A fallen player with no spare rejoins next level.
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

## Controller setup and calibration

The first gameplay frame of each level waits in setup. Both players press their
assigned **pause** control to mark ready, then select **save and resume**. Enemies
and all gameplay timers remain frozen. During play, either player's pause control
or keyboard **F1** opens setup. Disconnection opens it automatically before the
next simulation step; held inputs are cleared and must be released before rearming.
Setup also remains accessible from the radar and the original quit dialog via F1.

Use keyboard arrows to select a row and Enter to activate it. Configured movement
controls navigate, and shooting confirms. Device rows cycle/swap assignments.
`configure P1/P2 all controls` walks through movement, four aim directions,
shooting, previous/next weapon, radar, and pause. The two individual binding rows
show the primary binding; Left/Right select an action and Enter remaps only it.
Default mapped gamepads retain two alternatives for several actions; capturing
an action replaces both alternatives with the captured input.

For an unfamiliar or unmapped controller, use the keyboard for initial setup:

1. Select its player and start configuration. Release buttons, center sticks,
   leave triggers at rest, then press Enter to measure neutral for that action.
2. Press the intended button, move one stick axis fully, or press one hat/D-pad
   direction. The screen displays the actual raw button/axis/hat number and axis
   polarity/rest value. Release the control and press Enter to confirm it.
3. Backspace retries capture; Escape cancels the entire all-controls wizard and
   restores its previous bindings. Space leaves an action unbound. Separate aim
   directions are optional; movement direction aims when no aim input is held.
   Movement, shoot, both weapon selectors, radar, and pause must be bound to start.
4. For N64-style controllers, capture the four C buttons as aim up/down/left/right.
   Two aim buttons together aim diagonally. No N64 button numbering is assumed.
5. Both players press their captured pause buttons, then save and resume.

Axis activation uses a one-third-range threshold relative to the captured neutral
value; this handles asymmetric triggers and modest stick drift. Defaults rely on
SDL's existing gamepad mapping. Unmapped devices start unbound and never receive
an inferred layout or an automatically generated SDL database entry. No terminal
commands or database editing are needed for calibration. A device not exposed by
SDL at all still needs its OS connection/driver resolved.

Reset restores SDL gamepad defaults for mapped devices, or clears an unmapped
profile for recapture. **Cancel changes** restores the setup entry's bindings and
still-connected assignments. It cannot bypass the missing-device/readiness gate.
**Save and resume** persists a versioned co-op file in SDL's standard user
preferences directory (`Pangea Software/MightyMike/coop-controls-v1.txt`), separate
from legacy preferences. The last device GUID and bindings are retained for each
player; a different device starts with its own defaults/capture. This is two
last-used profiles, not a library of every controller previously connected.


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

Automated coverage includes all 15 area initializations, simultaneous frog/ship
ownership and disposal, a real teleporter trigger plus transformed warp completion,
platform carry, shared bonus rewards, both exit animations, and next-level setup.
These are controlled regression checks, not a full campaign traversal. Physical
controller feel, audible audio, long map traversal, complete platform paths, and
a full two-person campaign remain pending. Co-op saves remain unsupported.
No networking, split-screen, new artwork, or broader engine rewrite.

## Playtest follow-ups

- The continuation implements assignment, independent capture/calibration,
  button aiming, readiness/reconnection, and P2 inventory display.
- Owner reported basic play working with Switch Pro + N64, then P2 confined
  after P1 died. Reproduced a dead-partner tether defect and changed camera/
  tether rules for a sole survivor. Physical confirmation of that fix remains
  pending. Owner confirmed the lock began after P1 stayed dead with zero lives;
  play had worked normally until then. The exact map location is not captured.
