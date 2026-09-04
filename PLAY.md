# Voidfront — field trial 01

Launch `Voidfront.exe` beside its .pck and DLL. This is the first development skirmish, not the finished game. Windows x64; six Cairn walkers fight six opposing walkers on the Glass Reach test field. Enemy attack orders start after five seconds. Eliminate the opposing team to secure the sector.

- Left click selects a friendly walker; drag selects a group. Shift adds/removes selection.
- F2 selects all surviving friendly walkers.
- Right click orders movement. A then left click issues attack-move.
- S stops; H holds. Both stop movement immediately; nearby enemies can still be attacked.
- Arrow keys pan the camera; wheel zooms.
- R restarts the skirmish. Escape cancels attack targeting. Close the window to quit.

The current prototype has no economy, construction, fog, queued orders, control groups, menus or network multiplayer. Enemy AI only fights. Group pathing is a conservative grid prototype and needs crowd improvements. Art, animations and interface are foundation work; audio is not present yet.

From source, run `./tools/build.ps1 -Configuration Debug`, then `./tools/run.ps1`. Rebuild assets with `./tools/export_assets.ps1`. Produce the release package with `./tools/package.ps1`. Tool paths and dependency revision are pinned in tools/toolchain.json; build instructions and evidence are in TESTING.md.
