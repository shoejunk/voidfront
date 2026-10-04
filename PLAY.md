# Voidfront — salvage outpost

Launch `Voidfront.exe` beside its .pck and DLL. The default offline mode uses a 64 x 48 battlefield (four times the former area), with your outpost in the northwest and the opponent in the southeast. It starts with three workers, a command anchor and a finite salvage deposit for each side. The opponent gathers resources, constructs a Foundry, researches plating, trains Striders and Lancers, and attacks through the same simulation commands. Destroy its command anchor while protecting yours. Faction breadth, production art and balance remain unfinished.

- A skirmish setup screen opens first: Tab toggles the Economic AI or a passive opponent, Left/Right change the match seed, Enter starts. R restarts with the chosen settings; M returns to setup (offline only).
- Left click selects a worker, anchor, deposit or Foundry. Drag selects units. F1 selects your living workers; F2 selects your living army, so army orders leave miners working.
- Select workers and right click a salvage deposit. They mine, carry up to 10 salvage each, return it to your anchor, and repeat until the deposit is depleted.
- Right click your anchor to return carried salvage immediately and stop.
- With workers selected, press B and click a valid site to construct a Foundry for 100 salvage. Green preview means affordable and geometrically valid; red means blocked or unaffordable. Sites must be within eight world units of your anchor and leave clearance around structures, deposits and terrain. Construction takes five seconds of worker activity after arrival.
- S stops work without losing carried salvage or an unfinished foundation. Right click an unfinished Foundry with workers selected to resume it without paying again.
- Select your completed Foundry and press T to train a Strider for 50 salvage. Each unit takes five seconds, and a Foundry holds up to five queued units. The population cap is 12, including living workers, Striders and reserved queue slots.
- With a Foundry selected, X cancels its last queued unit and refunds 50 salvage. Remaining active training keeps its progress; canceling the only unit clears progress. A blocked exit keeps the completed unit queued: move nearby units to open space.
- Select a newly produced Strider and right click to move it; A then click attack-moves, S stops and H holds. Right click an enemy building to attack-move toward it; nearby enemy units can take priority, so this is not a target lock.
- Shift + right click queues movement; A then Shift + left click queues attack-move. Each unit holds four pending legs beyond its active destination. Selected units show numbered destinations: 0 is active, 1-4 are pending; cyan means move and amber means attack-move. Dashed lines join destinations and may cross terrain the unit will route around. A full queue ignores extra legs. Stop, Hold and ordinary replacement orders clear it; deselection hides the markers.
- Scout the cyan flux deposits and send a worker to gather. Select a completed Foundry and press G to research Hardened Plating for 50 flux. Once complete, L trains a ranged Lancer for 75 salvage and 25 flux; X refunds both resources when canceling a queued Lancer.
- Destroyed buildings disappear from the field and minimap. Losing a Foundry loses its paid queue without a refund. Destroying a command anchor ends gameplay and displays victory or defeat; workers alone cannot shoot buildings.
- Escape or right click cancels placement. R restarts the economy, clears selections and control groups, and starts the opponent again.
- Ctrl + 0–9 saves a control group; 0–9 recalls it. Ctrl + Shift + number adds to a group; Shift + number adds a group to selection. Groups clear on restart.
- Arrow keys pan; hold Shift for faster panning. Hold the middle mouse button and drag to pan, or click/drag the minimap. Home returns to your anchor. The mouse wheel zooms; the minimap outline shows your view. Close the window to quit.
- Explore with workers or Striders: unexplored ground is black, explored ground outside current vision is dim, and nearby terrain is fully visible. Workers reveal a seven-unit radius, Striders nine, anchors ten and Foundries seven. Team vision is shared; ridges currently block movement but do not block sight.
- Enemy units, structures, deposits, labels, health bars and minimap markers are hidden outside current vision. Explored terrain remains known, but hidden buildings are not shown as remembered silhouettes. Moving the camera reveals nothing. Use right-click movement or A + click to scout dark terrain; R clears exploration for a new match.
- The AI uses the same vision sources and commands. It can raid the known opposite starting location without first seeing the anchor. Full fog-aware strategic scouting remains future work.

The HUD shows resources, carried salvage, costs, construction, production queue/progress, live and reserved population, and purchase rejection or blocked-exit feedback. Workers reuse the existing walker mesh, and buildings/deposits are readable blockouts, not finished production art. Crowds, audio, animation polish and performance acceptance remain unfinished.

## Combat skirmish

Launch `./Voidfront.exe -- --skirmish` for the existing six-versus-six offline combat trial. Enemy attack orders begin after five seconds; eliminate the opposing team. Right click moves, A then left click attack-moves, S stops and H holds. R restarts that skirmish.

## Large offline field

From the package directory, launch `./Voidfront.exe -- --scale128` for the
128x128 terrain field with 250 walkers per side. Use `--units-per-team=100`
with that option for 200 total walkers. Controls are the same; R preserves the
large map and chosen population. The camera starts over your army; arrows pan
across the field and the wheel zooms out for an overview. The minimap shows the
whole map. Enemy combat AI starts after five seconds.

This field exposes the existing scale scenario for offline play and diagnosis.
Dense crowds still jam, and rendering budgets and complete RTS features remain
unfinished. Large-map network sessions are not implemented.

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
- Two cyan flux deposits lie on the dark central crossing. Scout them (units reveal fog), then right click one with workers to mine flux; it is banked separately from salvage. Select your Foundry and press G to spend 50 flux on Hardened Plating (200 ticks): every Strider gains +50 hp.

## Experimental two-window economy match (loopback)

The same lockstep session now carries the 64x48 economy map: each player starts with
an anchor and three workers, gathers salvage and flux, builds Foundries, trains Striders
and Lancers, researches Hardened Plating and fights for the opposing anchor. There is no
AI in this mode. Launch in two terminals (matching `--seed`, `--session`, `--delay`, `--ticks`):

```powershell
./Voidfront.exe -- --network-economy --player=0 --port=39000 --remote-port=39001 --session=12345 --seed=7 --ticks=36000
./Voidfront.exe -- --network-economy --player=1 --port=39001 --remote-port=39000 --session=12345 --seed=7 --ticks=36000
```

Controls are the offline economy controls (B build, T/L train, G research, Shift-queued orders).
Loopback only; when an anchor falls R returns to offline play. Different seeds/maps are
rejected at the handshake. Not yet verified by human play, LAN/Internet or a 30-minute soak.
