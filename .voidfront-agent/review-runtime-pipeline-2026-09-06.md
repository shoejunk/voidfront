# Independent packaged runtime regression review

Reviewed 2026-09-06 12:02 America/Los_Angeles. Scope: this run's existing LOCAL twelve-unit skirmish package after the network pipeline work. This reviewer made no code, build or Git changes. STOP was absent and the primary task's run marker was present. Read SPEC, PROGRESS, TESTING and roadmap before review.

## Evidence actually examined

- `artifacts/packaged-pipeline-regression.json`, `packaged-pipeline-regression-host.json` and `packaged-pipeline-regression-engine.log`.
- `artifacts/pipeline-package-2026-09-06.log` (export completion and package location) and `pipeline-capture-2026-09-06.log`.
- Both 1920x1080 images opened and visually inspected using view_image: `artifacts/packaged-pipeline-regression.png` and `artifacts/packaged-pipeline-regression-battle.png`.
- Client timing boundaries in `client/main.gd` inspected to distinguish bridge advance from snapshot/presentation costs.

## Verified regression result

The package/export log reports Godot 4.7.2.stable.official.ed1daf0bf; the packaged engine log reports the Release extension ready on RTX 2070 SUPER. Synthetic click selects one unit, drag selects six, Move/Stop/AttackMove are accepted, movement and combat damage occur, Stop does not drift, and restart returns the initial twelve-unit state/hash. The report passes at tick 400 with terminal hash `b559cd93ea152fc0`, matching the prior documented offline baseline. All five animation names are imported and requested. The inspected engine log contains the already documented root-certificate-store error and no other error.

This supports preservation of the bounded offline scripted regression. It does not establish human input responsiveness, a complete economic match, animation quality, or a networked Godot session. No new movie or manual playtest was supplied; imported/requested animation names are not evidence of deformation or transition quality. The independent network review and primary's new separate-process cases remain separate evidence.

## Measurements and limits

| Current packaged observation | Value | Interpretation |
| --- | ---: | --- |
| Frame interval p95 / p99 | 17.006 / 17.412 ms | Wall-clock frame interval includes vsync/scheduling; p99 exceeds unchanged 16.67 ms target. |
| Bridge advance p95 / p99 | 0.048 / 0.070 ms | Excludes subsequent snapshot extraction and rendering. |
| Sampled peak resident working set | 242,659,328 bytes | Windows PeakWorkingSet64 sampled once per second; the Release static allocator value of zero is unavailable telemetry, not zero memory. |
| Capture wall time | 21.6118719 seconds | Normal capture, movie=false. |

This is a twelve-unit, small prototype map on the development host. It is not the specified 128x128 map, 200-unit representative battle, 500-unit stress scenario, or reference hardware. The prior same-day ~3.6 ms p99 and current ~17.4 ms p99 do not demonstrate a code regression or improvement without normalized presentation/scheduling conditions. No measured CPU-frame or input-to-response latency is supplied, and the 12 ms CPU-frame target cannot be assessed from these intervals.

## Existing visible debt

1. Both captures retain blockout terrain: a flat grid floor, repetitive cuboid perimeter and two straight central barriers. They do not demonstrate finished terrain, biome, environmental storytelling or SC2-level art density/readability.
2. Walker upper surfaces read as very bright pale patches at RTS distance. Cyan/amber cues distinguish sides, but material form and fine silhouette detail are weak; closely spaced units and white selection rings make individual units harder to separate. The battle frame contains close formation spacing, but a still cannot establish collision failure or movement quality.
3. The outcome panel has readable contrast, but `SIGNAL LOST` remains ambiguous between defeat and connection failure. It should become explicit before actual client networking is added. The visible restart hint is clear.
4. Dark wrecks behind the outcome panel are partly obscured and difficult to distinguish on the dark floor. These images do not establish death collapse, hit response, attack timing or corpse readability through motion.
5. The large instruction HUD and sparse minimap fit a prototype; the captures supply no evidence for production economy/production UI, accessibility, audio, complete content or match UX.

Presentation was not changed in this run. These are existing debts observed in the current artifacts, not claimed visual regressions or improvements. No direct SC2 reference comparison was supplied or performed.

## Verdict and next verification

Accept only the bounded packaged LOCAL regression evidence. No AAA, shipping, animation-quality or playable-network approval. Preserve this smoke while progressing to two packaged clients with explicit connection/stall/disconnect/outcome behavior, then capture real input-response timing and a new gameplay/animation recording. Keep the required any-angle pathfinding, full-match systems, reference battle profiling and production art gates open.
