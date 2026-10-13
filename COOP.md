# Couch co-op V1

## Implementation plan and baseline

Upstream baseline: cloned recursively from jorio/MightyMike on 2026-10-13.
Unmodified arm64 macOS build passed using SDL 3.2.2, CMake, and Apple Clang.
Baseline warnings: upstream macOS 10.13 deployment target with current libc++, and linker common-section alignment.

Small implementation: two PlayerState records, independent SDL gamepad input
snapshots, bounded compatibility scopes around object movement/animation, shared
midpoint camera with movement tether, independent health and weapons, shared
objectives/keys/score/spare lives, revival near the partner, and a minimal overlay.
Original one-player and alternating two-player modes stay available without --coop.
No networking or save migration.
