# Cairn Compact survey walker

Original procedural Blender foundation asset for Voidfront: tapered warm ceramic
command capsule, brass joints, four articulated mechanical legs, offset induction
tool and raised sensor flag. The asymmetry and broad cyan identification stripe
are intended to remain recognizable from the RTS camera.

Run `tools/export_assets.ps1` with Blender 5.1.2 installed at the pinned path.
`generate_walker.py` recreates the editable `cairn_walker.blend`, runtime
`client/assets/cairn_walker.glb` and `walker_preview.png` from local source without
downloaded assets. No raster texture is required for this foundation. Meshes have
UV islands, five PBR materials and normalized rigid skinning to a 15-bone hierarchy.
The exported model uses one mesh and five material surfaces, 8,888 triangles and
17,436 seam-separated vertices. Materials: `Team` (instance recoloring),
`Ceramic | warm ivory`, `Brass | machined edges`, `Basalt | joint housing`, and
`Signal | amber lens`.

Blender +Y forward exports to Godot -Z forward, with Godot +Y up. Bind footprint
is 1.030 by 1.115 world units; top is 1.425 and soles are 0.040 above model origin.
No additional scale/rotation is required. Collision radius belongs to simulation.

All five actions are baked on every bone at 30 Hz: `idle` (2 s), `walk` (1 s),
`attack` (0.8 s), `hit` (0.5 s), `death` (1.4 s). Set idle/walk looping in the
client importer or Animation resource; other clips should be one-shots. Root
motion is absent. Events are presentation only. Walk is an initial alternating
diagonal gait; foot locking and terrain adaptation remain presentation work.

`validate_walker.py` parses the exported GLB, verifies all clip names and varying
animation channels, 15 skin joints, UV/normal/position attributes, normalized
nonnegative weights and valid joint indices, five material surfaces, and imported
bind dimensions. It writes `validation.json`. Export/import and structural checks
passed on 2026-09-03. The Blender beauty render was actually inspected: four legs,
separated joints, asymmetric tool/sensor and team stripe are visible without
obvious missing meshes. This is foundation art, not production or AAA approval.

Remaining evidence/quality gates: inspect skeleton deformation and transitions in
the running packaged Godot build; inspect silhouettes/team colors at actual RTS
distance; record animation motion; refine locomotion/contact, UV layout/material
variation, LODs, destruction and damage feedback. The 8,888-triangle mesh and five
surfaces require actual battle profiling. Blender may log a denied thumbnail-cache
write outside the project; project source, runtime export and preview still save.

Integration follow-up, 2026-09-03: the Release Godot package loads all five clips,
and its recorded gameplay/contact sheets show changing leg stance and orientation
without obvious detached limbs. Independent visual review confirms basic motion,
team/wreck distinction and improved lighting. Close attack/hit/death distinction,
foot contact and full transitions remain unverified; this is not production art
approval. See artifacts/packaged-animation.mp4 and .voidfront-agent/review-visual-2026-09-03.md.
