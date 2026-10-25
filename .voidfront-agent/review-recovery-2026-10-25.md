# Independent recovery/integration review - 2026-10-25

Status: bounded repair and integration review PASS. Final source/build/offline/paired evidence inspected. No AAA/shipping approval.
Reviewer owned only this report. No implementation, Git, build or fixture changes performed.

## Corrected diagnosis

Independently opened JSONs generated this run from the pre-existing Release binary (its exact correspondence to current baseline source was not established). Roster sizes are
18, 28, 36, 52 and 68 at ticks 4000, 6000, 8000, 12000 and 16000, respectively.
IDs are retained tombstones, so these increases show repeated production. At 16000,
worker 3 (P0) and 6 (P1) each carry 10 salvage on their return trip. At 16020,
banks rise from 22/22 to 32/32 and that cargo is zero. These diagnostics contradict the frozen-state reading for that binary, but do not alone prove source-current behavior. The old claim of a frozen
22-salvage post-combat economy is false. At 32000 the finite-resource state is
salvage0/0, roster82, workers on flux/Stop; this is distinct from a 16000 recovery
stall. Retained artifacts: recovery-baseline-{4000,6000,8000,12000,16020,32000}-2026-10-25.json
and recovery-baseline-2026-10-25.{json,vfr,trace} under artifacts/.

## Source findings and feedback

- Found actual out-of-bounds test instrumentation: economic_match used array<int,10>
  while canonical TrainLancer is10. Reported to primary/AI specialist before integration.
- Oct20 acquisition distances read tick_start, but chase goal still reads live
  target->x/z and service_point sees progressively moved units. Therefore narrow
  observed mirrored-duel symmetry must not become a general claim of eliminated
  processing-order dependence.
- The old match auditor omitted Research/TrainLancer, treated only Strider as F2
  army, valued all production at50 salvage, and prohibited the legitimate plating
  HP gain. Reviewed the new parser/resource/HP diff. Structure orders7..10 now bind
  singleton owned structure IDs and zero coordinates; orders11/12 remain units.
  Separate salvage/flux inventory includes Lancer75+25, queue-mask kind costs,
  research50 flux, and forfeited mixed queue costs. Health allows the50HP bonus
  only for a living existing Strider on research transition and rejects resurrection.
- Requested the resource-kind corruption preserve nonnegative banks and total
  combined resources. The final mutation transfers one deposit unit salvage->flux
  and specifically fails resource conservation. Requested synthetic helper cases
  remain explicitly distinct from live packaged tier-two evidence.

## Independently executed checks

Imported current tools/verify_match_capture.py and called commands_from_replay,
audit, audit_roles and tier_two_regressions on retained weekly-victory-2026-10-18.json.
Passed4432ticks,38736unitrows,125damage ticks, conserved resources[4000,2000],
seven role-selection events; four positive helper boundaries and five rejected
helper corruptions. No native replay was executed in this check, and this older
capture is not current-package validation.

## Open evidence gates

Fresh AI recovery diffs, MSVC builds and current packaged captures still pending
at this report checkpoint. Synthetic helper boundaries do not prove live Lancer
combat. Scripted held defenders do not prove contested1v1. No human play,
responsiveness, audio listening, animation quality, SC2 side-by-side comparison,
reference performance, balance, network soak/LAN/Internet, AAA or shipping approval.
Every existing SPEC/crowd/frame/response/content/art/audio gate remains open.
## Lone surviving worker repair - source reviewed

The separate casualty reproducer is valid: when the only surviving worker is
already gathering flux, old AI never redirects it to salvage, so it cannot fund
new combat production. Independently read artifacts/recovery-before-tests-2026-10-25.log:
exact failure is `sole flux workers never resumed salvage income`.

Reviewed the bounded policy diff. It counts live owned workers, forbids new flux
assignment with only one survivor, and allows an existing sole flux Gather worker
through nearest explored salvage selection once. The resource_id then becomes
salvage, preventing repeated Gather resets. apply_economy's existing mixed-cargo
rule returns carried flux before starting salvage; explicit ReturnCargo is allowed
to finish. No engine/runtime authority, floating point or clock was added.

The new regression controls mirrored casualties while preserving banks and paid
queues, begins with five carried flux per survivor, checks wire roundtrip/replay
hash equality and mirrored worker/cargo/banks every tick, and requires new births
beyond all pre-existing production reservations. The earlier standard duel now
requires actual replacement production after an earlier complete army wipe,
without demanding an artificial winner. The counter array now covers all13orders.
No source blocker found in the bounded repair; fresh passed build/package still
pending at this checkpoint. This does not validate general AI recovery when all
workers die, finite resource stalemates, general routing progress or match balance.

## Fresh source-current verification checkpoint

Independently read recovery-current-16000-2026-10-25.json and
recovery-current-16020-2026-10-25.json after final Release rebuild. Content ID is
339002293416796399 (04b460ca2d26a0ef); roster68, salvage22/22->32/32 and returning
salvage cargo10->0. This establishes the corrected diagnosis on freshly built
current source, separately from the earlier pre-existing-binary diagnostics.
The apparent full/state hash difference raised during integration came from
hash modes (headless stdout prints full Sim::hash; match_tests prints state_hash),
not evidence of a binary/source mismatch. No mismatch is inferred from it.

Read recovery-verify-approved-2026-10-25.log and Release LastTest.log. Final natural
duel prints `AI duel unresolved symmetric cap with recovery tick=16000` with state
hash7942617639625604552. Full verify reports both Debug/Release12/12 and repeated
2000tick trace matches. Earlier sandbox C1902 compilation errors remain in
recovery-verify-2026-10-25.log; approved retry is the passing build evidence.

Actually opened new exploration home/unexplored PNGs. Base units, anchor, salvage,
fog boundary and minimap camera displacement are visible. Anchor HP line overlaps
its label text; walkers remain bright reused silhouettes among blockout cubes and
flat terrain. Images do not prove motion, input responsiveness or finished art.

## Fresh offline package evidence - independently inspected

Frozen package is recovery-package-2026-10-25. Read both final audit summaries:
exploration2864ticks/37checks, Debug+ten Release replays and5corruption rejections;
victory4512ticks/38934unitrows, winner0/hashfa4c07b256e51eaf, Debug+ten Release
replays,13report corruption rejections,7role events,187rendered attack clip requests.
Canonical command orders in victory are{4:8,6:2,7:17,2:16}; no Research/TrainLancer.
Tier-two helper4positive/5negative boundaries are synthetic and separate.
Both captures use accelerated eight ticks per frame; no pacing/performance claim.

Actually opened victory-victory, victory-counterattack, victory-restarted,
exploration-scouted and exploration-defeat PNGs. Victory/defeat banners, reset base
and scout's revealed ground are visible. Counterattack image contains dark ground,
HUD/minimap and a selection marker but no army; it does not show the counterattack.
Victory survivors and reduced health are visible, partly masked by terminal overlay.
Blockout cubes, flat grid terrain, bright reused walkers and HUD/health-label
occlusion remain. Screenshots establish static display only, not animation,
responsiveness, balance, sound or a human complete-match experience.

During the real paired run, actually opened p1-researching.png, p1-lancer.png and
p1-group-queued.png from recovery-net-match-2026-10-25. RUNNING/confirmed ticks,
plating progress then DONE, one selected Strider plus one Lancer and4/4FULL queue
are visible; cyan/amber legs and ordinal labels can be distinguished for this
two-unit selection. Lancer retains the reused walker silhouette, and HUD panels
occlude lower scene content. These images supply static interface evidence only.
Final paired native replay/prefix audit remained pending when these were inspected.

## Paired rendered client evidence - native audit pending

Read both p0.json/p1.json final reports and host.json. Real process IDs38140/35512,
UDP54774/54775, seed7, delay2; both COMPLETE with executed/confirmed7147,
winner0/hash34f802c27f1e60b2 below cap12000. Frozen package identity in host:
PCKce656b5951859d7031b95a5177f7c340091e3f8608d22bdf3a4bb0c55b6d4cf6,
DLL217687b5ae1881732045cfb6df2df28cdf54ca5b5c4b04bcdf026d5ef4c14ea.
Hostwall364.391s and peak220262400/224186368bytes are concurrent process diagnostics.

Opened p0-anchor-damaged, p0-combat, p0-group-queued, p0.png, p1-combat and p1.png.
Anchor damage978HP and terminal COMPLETE/offline-return text are visible. P0combat
is dark empty ground; P1combat shows two defenders parked north. P0six-unit queue
labels overlap; top HUD obscures some army/buildings. Terminal overlay hides part
of the last fight. Parsed final snapshot independently: all8combatunits retain
fullHP; P1Strider150/Lancer70 are both Hold(order3), all P0sixcombatunits fullHP.
Thus this is scripted anchor reachability, not contested army combat or balance.
Native replay/prefix/corruption audit summary awaited before final bounded verdict.

Independently executed verify_net_match.audit_reports against both final reports
and the actual replay bytes: PASS7147/7147, hash34f802c27f1e60b2,40canonical commands,
byte-identical VFR3streams,39/31fullphase snapshots and14/14capture snapshots.
This checks endpoint/cap/input/recording/report bindings without duplicating the
primary's full native replay/prefix suite.

## Final independent verdict - 2026-10-25 12:21 America/Los_Angeles

Read actual recovery-net-audit-2026-10-25/summary.json, completed19:20:42UTC:
PASS7147ticks, first-outcome7147 below cap12000, winner0/hash34f802c27f1e60b2,
22complete native replays,106unique native prefixes,70fullphases,28capture
snapshots and24rejected corruptions. Independently read prefix7146.json with
winner-1 and prefix7147.json with winner0. Independently checked current
Debug/Release binary and verifier SHA256 values against summary and compared
all22complete native trace files byte-for-byte with the client's7147hash trace:
all pass. No duplicate native execution was needed.

The final bounded repair/integration is acceptable: one surviving flux worker
now resumes salvage and funds replacement production, regression instrumentation
is safe for all canonical orders, and the offline auditor understands tier-two
resource/HP/role state. The standard duel22-salvage stagnation diagnosis is corrected
with current-source evidence. No substantive defect found in these scoped changes.

This verdict does not approve contested1v1, human complete-match play, general
processing-order independence, stationary-blocker/dense-crowd progress, physical
networking/impairment/soak, reference performance/latency, balance, finished
factions/maps, Blender/animation/audio/VFX/UI quality, AAA/SC2 parity or shipping.
Existing failed/unverified SPEC gates remain unchanged; COMPLETE.md is prohibited.
Next evidence should exercise coordinated setup/rematch/offline return and
contested armies with pending combat queues, with camera captures that include
the battle, before any claim beyond scripted anchor reachability.
