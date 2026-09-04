# Verification ledger

Build and test instructions and measured results are added as working increments land. All commands run from C:\dev\voidfront in PowerShell. A compilation result proves only compilation. Headless simulation tests do not prove networking or visual quality. A screenshot does not prove input response or stable frame time.

Required validation: MSVC Debug/Release sim regressions and cross-config traces; Godot version/import check; C++ extension in packaged build; Blender skeleton/clips and actual running animation; screenshots inspected at game distance; independent deterministic and visual/playability critics. Future milestones add separate-process network impairment, complete AI matches and representative battle profiling.

Evidence goes in ignored artifacts/; durable results and commands go here and in PROGRESS.md. No shipping gates have passed yet.

## 2026-09-03 verified foundation

Reproduce from the project root:

```powershell
./tools/bootstrap.ps1
./tools/export_assets.ps1
./tools/verify.ps1
./tools/package.ps1
./tools/capture.ps1 -Packaged -Name packaged-smoke -Ticks 400
./tools/capture.ps1 -Packaged -Movie -Name packaged-animation -Ticks 400
```

`build.ps1` verifies pinned Godot version, executable/template hashes and godot-cpp revision before configuring CMake and MSVC. It normalizes duplicate PATH/Path keys from the desktop host and uses one MSBuild node, with compiler parallelism inside godot-cpp. Godot automation uses artifacts/godot-profile to preserve the user's editor settings; Windows templates are copied and hash-checked. Import/export exit codes AND script/resource errors are fatal. One precisely recognized sandbox diagnostic remains: `ERROR: Failed to read the root certificate store.` This run does not use runtime TLS; all other runtime errors are fatal to capture verification.

Godot observed: 4.7.2.stable.official.ed1daf0bf. Windows release template independently reports the same version. Compiler: MSVC 19.38.33145.0, VS2022 Professional tool directory 14.38.33130; SDK 10.0.26100.0. Blender 5.1.2 ec6e62d40fa9. Dependency rationale: official godot-cpp explicitly supports its 4.7 API target; a narrow RefCounted/OS profile avoids generating the entire engine API. See https://github.com/godotengine/godot-cpp/tree/05057de73de4b99f114d36c40d84ca46926c0e25 and https://docs.godotengine.org/en/4.7/tutorials/scripting/cpp/index.html.

### Determinism and replay

Integrated MSVC Debug and Release CTest: 1/1 executable passes in each configuration. Coverage includes canonical serialization/truncation/version bounds, invalid ownership/IDs/duplicates, command sequence/tick ordering, ridges and blocked goals, immediate stop, twelve-unit spacing, combat/AI and repeated replay. Independent critic's arrival-time dead-unit command admission defect was fixed and regressed.

`verify.ps1` also invokes distinct recording and playback processes in each configuration; replay playback consumes recorded canonical inputs and generates no AI. Both 2,000-tick traces match, plus ten repeated Release playback traces. Trace SHA256: B5FBB540C91632A5FDF71952A3E8C6B315526AC3C500FE5F7E171C2ACECFD1B1. Fifteen malformed saved replay fixtures are rejected per configuration, and a valid empty-input replay remains idle. `tools/compare_traces.py` reports the first differing trace line/tick. Evidence: artifacts/verification.log and artifacts/determinism/{Debug,Release}/record.vfr, record.trace, replay.trace. These are separate file-playback processes, **not communicating network peers**.

### Packaged runtime and art

artifacts/package/Voidfront.exe loads its adjacent PCK and Release C++ bridge. The final normal capture passes all assertions: click selects one, drag selects six, Move/Stop/AttackMove accepted, no stop drift, actual combat damage, five imported clips with idle/walk/attack/hit/death requests, and restart recreates initial state. 400-tick terminal hash b559cd93ea152fc0; scripted player loses. The movie run reaches the same hash and passes. This is programmatic input, not a manual playtest.

Artifacts: packaged-smoke.json, packaged-smoke-host.json, packaged-smoke-engine.log, packaged-smoke.png, packaged-smoke-battle.png. Screenshots are 1920x1080. packaged-animation.avi and MP4 have 1,202 frames at 60 FPS, 20.033 seconds, **1600x900**: Godot's movie writer uses the project's base viewport despite the requested window size. Do not call the movie native 1080p. animation-overview.png, walk-contact.png and combat-contact.png are extracted sequences used for actual visual inspection. Movie encoding took longer than real time; its frame timings are excluded from performance claims.

Blender validation: 15 joints, normalized skin weights, UV/normal attributes, five clips, 8,888 triangles and five surfaces. Source: art/cairn_walker.blend; reproducible generator/validator plus GLB. Imported animation and moving limbs/corpse poses are visible in recorded sequences. This does not establish production deformation, foot contact or transitions. Lighting, corpse visibility and outcome panel were improved after screenshot criticism. Final restart no longer logs GLES3 null-material errors after sharing cached presentation materials. No 2D raster assets were needed in this milestone; HUD is native controls/drawing.

### Measurements and limits

Development machine: Ryzen 7 3700X, RTX 2070 SUPER, 68,644,352,000 bytes physical RAM visible to Windows; not the reference target. Normal 12-unit package, requested 1920x1080 window: p95/p99 wall-clock frame intervals **17.075/17.503 ms**, bridge advance **0.046/0.084 ms**, peak resident working set **231,268,352 bytes**, sampled via Windows Process.PeakWorkingSet64. Frame intervals include vsync/scheduling; advance timings exclude snapshot extraction. Godot static allocator monitor returns zero in Release and is unavailable, not zero memory use. The frame p99 exceeds 16.67 ms; target is unchanged. CPU frame-time percentiles and input-to-response latency have not been measured.

Standalone active-tick preliminary Release results: 200 units p95/p99 314/638 us over 939 active ticks; 500 units 1132/1694 us over 1992 active ticks. Both use the prototype 32x24 map and may include concurrent-build noise. They do not meet the required 128x128 benchmark or establish large-battle rendering/crowd quality.

Independent reports: .voidfront-agent/review-simulation-2026-09-03.md and review-visual-2026-09-03.md. No independent shipping approval. Official SC2 screenshots could not be retrieved for direct side-by-side comparison; that evidence remains missing.

Not executed/implemented: real network exchange, latency/jitter/loss, complete economic AI or 1v1 matches, production-map performance, manual input responsiveness, dynamic-blocker/opposing-flow crowd suite, complete content, audio or production animation polish. No COMPLETE.md is justified.
