# Voidfront

An original 3D competitive RTS about rival salvage civilizations reclaiming a broken orbital foundry. StarCraft II is the quality benchmark, not a content template. Completion requires production quality and measured evidence; this is a continuing project without a release deadline.

## Game identity

Two asymmetric factions: the industrial **Cairn Compact**, with articulated ceramic-and-brass walking machines, and the **Vesper Choir**, a lattice civilization of living signal and folded mineral. Readable silhouettes, crisp team accents, charcoal basalt, pale sand, oxidized metal and cyan/amber energy define the art direction. Avoid borrowed units, UI, lore or recognizable silhouettes.

Match objective: destroy the opponent's command anchor. Economy uses salvage (common) and flux (contested); workers mine deposits, construct anchor-linked structures and expose themselves to raids. Population and two technology tiers produce meaningful timing windows. Map control supplies flux and visibility. Final scope: two complete factions, each with worker, command structure, two production structures, technology structure, defense, and at least six combat units with distinct counterplay; three tournament maps; tutorials and skirmish settings.

## Architecture

Godot 4.7.2 stable standard Windows x64 handles presentation, input, camera, UI, animation, sound and effects. The independent C++20 simulation handles all movement, collision, pathfinding, combat, economy, fog, AI and victory. Thin godot-cpp GDExtension adapts canonical commands to sim and exports snapshots. No engine physics/navigation/animation event determines gameplay.

Simulation: 20 fixed ticks/s, integer coordinates in 1/256 world units, stable entity IDs and ordering, explicit unsigned seeded RNG, canonical bounded commands. Movement uses deterministic grid routing and integer stepping. AI and replays use the same command path as humans. State hashes serialize fields explicitly, never object padding. Network protocol/version and content identity are checked before joining. Commands are buffered in deterministic lockstep; every player contributes a turn (including empty turns), late turns stall advancement. Separate-process transport, timeout/disconnect, checksum diagnostics and recorded inputs are required.

Presentation interpolates previous/current snapshots; it may choose animation and visual rotation freely, but cannot change outcomes. First milestone is a small animated walker skirmish; it does not satisfy full RTS or networking gates.

## Pinned tools

- Console: C:\dev\Godot\4.7.2\Godot_v4.7.2-stable_win64_console.exe
- Editor: C:\dev\Godot\4.7.2\Godot_v4.7.2-stable_win64.exe
- Required version: 4.7.2.stable (observed 4.7.2.stable.official.ed1daf0bf).
- Templates: %APPDATA%\Godot\export_templates\4.7.2.stable.
- Blender: C:\Program Files\Blender Foundation\Blender 5.1\blender.exe; observed 5.1.2 ec6e62d40fa9.
- MSVC: Visual Studio 2022 Professional, x64; CMake VS 17 2022 generator.
- godot-cpp: official repository, exact revision 05057de73de4b99f114d36c40d84ca46926c0e25, API 4.7, verified in Godot 4.7.2 packaged runtime. Engine/template SHA256 pins live in tools/toolchain.json; no floating dependency updates.

## Acceptance and budgets

Reference target: Windows x64, Ryzen 5 5600, RTX 3060 12 GB, 16 GB RAM, 1920x1080 High at 60 FPS. Development host observed: Ryzen 7 3700X (registry), RTX 2070 SUPER (Godot renderer), 68,644,352,000 bytes physical RAM visible to Windows (GlobalMemoryStatusEx). This is not the reference machine. Budgets remain targets until measured on the reference or documented comparable hardware. Do not weaken them to pass.

- 200-unit representative battle and 500-unit stress scenario on a 128x128 navigable grid: p95 CPU frame <=12 ms, p99 frame <=16.67 ms, simulation p95 <=4 ms/tick and p99 <=8 ms/tick; peak resident memory <=2 GB.
- Local selection/order visual feedback <=50 ms p95; authoritative response <=150 ms p95 at 80 ms RTT. Separate-process tests at 0/80/160 ms RTT, 20 ms jitter and 1% packet loss, with 30-minute complete matches and no divergent hashes.
- Bit-identical replay/hash traces across 10 repeated runs and MSVC Debug/Release, with first-divergence reporting. Protocol truncation, incompatibility, invalid ownership, duplicate/reordered commands and disconnects covered.
- Pathfinding: crowded chokes, opposing streams, moving blockers, unreachable targets and dynamic construction; no permanent crowd lock or overlaps. Runtime group responsiveness is a separate visual/playtest gate.
- Fully playable 1v1 and 1vAI from setup to victory/defeat, restart and replay. AI must gather/build/produce/tech/scout/fight using canonical commands.
- Finished Blender meshes, UVs, materials, rigs, skin weights and idle/walk/attack/hit/death clips; verified bind pose, transitions and deformation in packaged game. Preserve .blend and reproducible export.
- Production terrain, lighting, VFX, audio, UI, feedback, accessibility and controls (selection, queued orders, groups, hotkeys, attack-move, stop/hold). No placeholders at completion.
- Capture and inspect screenshots/video at RTS distance, compare concrete SC2 references, and measure responsiveness/frame stability separately. Balance needs repeated complete-match evidence.
- Reproducible Windows package and play instructions; full regression and independent adversarial shipping review. Missing evidence, placeholders or substantive gaps prevent completion.
