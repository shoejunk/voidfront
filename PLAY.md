# Voidfront â€” field trial 01

Launch `Voidfront.exe` beside its .pck and DLL. This is the first development skirmish, not the finished game. Windows x64; six Cairn walkers fight six opposing walkers on the Glass Reach test field. Enemy attack orders start after five seconds. Eliminate the opposing team to secure the sector.

- Left click selects a friendly walker; drag selects a group. Shift adds/removes selection.
- F2 selects all surviving friendly walkers.
- Right click orders movement. A then left click issues attack-move.
- S stops; H holds. Both stop movement immediately; nearby enemies can still be attacked.
- Arrow keys pan the camera; wheel zooms.
- R restarts the skirmish. Escape cancels attack targeting. Close the window to quit.

The current prototype has no economy, construction, fog, queued orders, control groups or match setup menu. Enemy AI only fights. Units now travel at arbitrary headings with clearance-aware routes around the test terrain. Single-unit moves retain the exact clicked position; group destinations still use spaced cell centers. Crowded units can wait indefinitely because dynamic detours and deadlock recovery are unfinished. Art, animations and interface are foundation work; audio is not present yet.

## Experimental two-window loopback skirmish

From the package directory, launch these commands in two PowerShell terminals:

```powershell
./Voidfront.exe -- --network --player=0 --port=39000 --remote-port=39001 --session=12345 --delay=2 --ticks=36000
./Voidfront.exe -- --network --player=1 --port=39001 --remote-port=39000 --session=12345 --delay=2 --ticks=36000
```

Both windows run on this computer. The transport currently supports loopback only;
these commands do not connect other computers. Both players must use matching
session, delay and tick limits. Player 0 controls cyan walkers, player 1 amber;
there is no AI in this mode. The HUD shows readiness, delayed input, stalls,
confirmation and connection errors. Select and order your own army after readiness.
Two delay ticks schedule input at least 100 ms ahead of its sampled source turn.
The 36,000-tick limit is a configured 30-minute session cap, not a verified soak.

Closing either window makes the other time out. R returns to the offline skirmish
after a network session completes or fails; it cannot restart an active shared
session. To play another network skirmish, close both windows and launch both
commands again with the same new session number. This is an experimental combat
skirmish, with no LAN/Internet support or complete RTS match content yet.

From source, run `./tools/build.ps1 -Configuration Debug`, then `./tools/run.ps1`. Rebuild assets with `./tools/export_assets.ps1`. Produce the release package with `./tools/package.ps1`. Tool paths and dependency revision are pinned in tools/toolchain.json; build instructions and evidence are in TESTING.md.

Movement verification from source: `./tools/capture.ps1 -Packaged -Movement -Ticks 400 -Name packaged-movement` exercises actual selection/orders, exact oblique arrival, a ridge detour, Stop and live retarget. Add `-Movie` for a recording. Rebuild the package first. Simulation compatibility changed for this movement increment; older recordings and old-build network peers are rejected rather than reinterpreted.
