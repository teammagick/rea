# Cube World Alpha / Omega interoperability study

**Status:** Static archive investigation completed. Independently authored C++17 Omega-inspired rules compiled and tested. No Alpha executable modification, live injection, loader verification, or Omega runtime access has occurred.

## Evidence from user-provided Alpha ZIPs
- "Original Cube World Alpha.zip": PE32 x86 client stamped 2013-07-02 12:59:08 UTC.
- "Updated Cube World Alpha.zip" and "cubeworld-20130624.zip": byte-identical PE32 x86 Cube.exe and Server.exe; client stamped 2013-07-20 13:49:18 UTC.
- "Cube World Alpha.zip": installer + game resources and save data; its Cube.exe differs from the later client by exactly 6 bytes in a single run at file offset 0x5a18d (NOPs). This is an **unattributed patch** and is not used or endorsed.
- The resource data1-4.db files are SQLite asset-blob databases. The separate metadata SQLite file is archive catalog metadata, not a gameplay database.

Archive fingerprints and data were retained in a local private JSON report, not pushed. Never commit proprietary ZIPs/executables/assets/saves.

## Implemented, isolated and tested
See src/omega_rules.hpp and tests/test_omega_rules.cpp. Includes War Frenzy 10/20/40/80% life steal (superseding an earlier description), Guardian cyclone reflection, Berserker cyclone nonstacking bleed (linear tick is a prototype assumption), 40/40/20 equipment categorization, reduced equipped-enemy drops, caller-capped inventory stack merging, ten-village MRU, multi-item bulk selling, buff cleanup on death, voxel edit bounds and bounded undo.

```sh
cmake -S research/cubeworld-omega -B /tmp/cubeworld-omega-build
cmake --build /tmp/cubeworld-omega-build
ctest --test-dir /tmp/cubeworld-omega-build --output-on-failure
```

## Integration boundary
REA is a general-purpose **analysis** platform. Do not merge runtime patch/injection functionality into REA without its maintainer scope review. The prototype is isolated under research/. For a functioning Alpha mod, first investigate exact x86 functions and structures with REA/Ghidra; verify the compatible 2013 Alpha loader SDK, then build a separate local mod with explicit opt-in. Test offline with saved-game backups. No license bypass, game files, or extracted third-party code are distributed.

## Next steps
1. Map Alpha damage/buff and inventory systems through read-only Ghidra evidence.
2. Validate the community [Qube-Loader](https://github.com/qad3n/Qube-Loader) against the exact July 20 executable hash; do not assume CWSDK for Steam Cube World is compatible.
3. Add an isolated x86 game-side adapter when observed hook contracts and permissions are validated.
4. Test save integrity, crash recovery, world behavior and multiplayer implications before claiming compatibility.
5. Treat Omega's worldgen/engine/realms as separate large projects, not a copyable patch.

## Source status
Public Omega behavior descriptions: https://wollay.com/patch-notes/ . Binary identities: locally measured hashes. Gameplay rules: independent prototypes. Actual Omega source / engine behavior: not available and not claimed.
