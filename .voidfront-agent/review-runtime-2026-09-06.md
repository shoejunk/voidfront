# Independent packaged runtime regression review - 2026-09-06

Reviewed at 10:31 America/Los_Angeles by an independent runtime critic. Scope: preservation of the existing twelve-unit skirmish after the network foundation work. No source or art changes are attributed to this review, and no shipping or AAA approval is granted.

## Evidence actually examined

Read AGENTS.md, SPEC.md, PROGRESS.md, TESTING.md and the roadmap; STOP was absent. The primary automation owns the active run marker. Inspected `artifacts/packaged-network-regression.json`, `-host.json`, `-engine.log`, `tools/capture.ps1`, and relevant timing/result code in `client/main.gd` and `client/hud.gd`. Actually viewed both `artifacts/packaged-network-regression.png` and `artifacts/packaged-network-regression-battle.png`; both are 1920x1080. No movie, manual interactive session, SC2 reference comparison, or large-battle capture was examined in this review.

The programmatic smoke reports successful click/drag selection, accepted Move/Stop/AttackMove, movement, combat damage, stop without drift and restart. It reaches tick 400 with winner 1 and hash `b559cd93ea152fc0`, matching the documented foundation result. The extension reports ready with Godot 4.7.2.stable.official.ed1daf0bf and NVIDIA GeForce RTX 2070 SUPER. Five clips are imported and requested. The engine log contains only the previously documented root-certificate-store diagnostic, with no script/resource/restart error observed. This supports a successful bounded local skirmish regression; despite its filename, this capture is not evidence of a networked Godot match.

## Concrete visual and playability gaps

1. Walker cream highlights remain near white at RTS distance. Broad bright torso surfaces obscure surface detail and much of the smaller cyan/amber ownership accent. Team health bars and selection rings provide more obvious ownership cues than the mesh surfaces. Revisit lighting/material response using the same camera distance before increasing asset detail.
2. The battle image shows tightly arranged units with neighboring selection rings and health bars crowding one another. Silhouettes are countable here, but this twelve-unit composition does not establish readability in a 200-unit battle. Nothing in the still images proves physical overlap or a pathfinding defect.
3. The flat tiled floor, repeated border blocks and two repeated divider walls remain an obvious terrain blockout. There is little environmental hierarchy or indication of resource and strategic objectives. Production terrain and complete economic match readability remain absent from this evidence.
4. Defeat and restart text are legible against the dark panel, but `SIGNAL LOST` alone can be confused with a transport failure once networking reaches the client. An explicit defeat reason and separate disconnect state would make the outcome actionable. The result panel covers much of the central battlefield, limiting post-match inspection.
5. Corpse piles are darker than live units and distinguishable, but pile readability and selection obstruction cannot be evaluated from these two stills. Foot contact, skin deformation, attack recoil, hit reaction and death transitions require a new animation sequence or interactive inspection; imported/requested clips alone do not verify them.

The screenshots demonstrate an intact HUD, selection rendering, distinguishable teams and legible restart instructions at the captured resolution. They do not prove camera responsiveness, hotkey reliability under play, command latency, crowd movement, targeting quality, balance, audio, or complete match usability. Queued orders, control groups, economic play and manual input testing remain documented open gates.

## Performance interpretation

The current normal (non-movie) run reports frame-interval p95/p99 **3.036/3.611 ms**, bridge-advance p95/p99 **0.036/0.056 ms**, and sampled process peak working set **235,040,768 bytes** over **21.774 seconds**. These are current observations, not a regression threshold approval. The frame interval measures elapsed time between `_process` entries; it is not isolated CPU work time. Bridge timing surrounds `bridge.advance()` only and excludes snapshot extraction/presentation. The zero static allocator monitor is unavailable data, not zero memory use.

The frame intervals differ substantially from the prior approximately 17 ms capture, but these artifacts do not establish why (presentation/scheduling conditions are not normalized). Do not claim an optimization from unchanged client code. Neither run is the required 128x128 map with 200/500 units, nor a measurement on the reference hardware. Input-to-feedback latency, GPU work, CPU frame time and sustained battle frame stability remain unmeasured. SPEC budgets remain unchanged.

## Verdict and next evidence

**Bounded packaged local-skirmish regression passes its scripted assertions, with no newly demonstrated visual failure. Production visual, responsiveness and performance gates remain open.** Preserve this package as foundation evidence. As network play is attached to the client, the next runtime check should use two packaged clients, explicit defeat versus disconnect feedback, an actual command-latency trace, and a short recorded sequence showing selection, group movement through the divider and combat. Separately capture a representative crowded battle before judging large-scale readability or performance.
