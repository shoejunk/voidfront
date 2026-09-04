# Voidfront development

Read .voidfront-agent/STOP before changing files or starting helpers. If present, stop and pause the weekly automation. Read SPEC.md, PROGRESS.md, TESTING.md and .voidfront-agent/roadmap.md at the beginning of each run. Preserve existing work. Never restart this game from scratch.

Use dev for development. The primary agent owns all Git operations and integration. Specialist agents may own explicitly separate file sets. Independent critics must examine actual evidence and identify gaps; no unsupported quality claims.

Authoritative logic lives in sim/ and must use no Godot headers, floats, wall clocks, physics or runtime callbacks. Godot presentation uses snapshots and canonical commands only. C++ uses MSVC and C++20. Build both Debug and Release and run relevant tests before committing. A build is not gameplay proof.

Pin Godot 4.7.2 stable, matching templates and the recorded godot-cpp SHA. Keep editable Blender sources and export scripts. Do not create COMPLETE.md until every SPEC gate and independent shipping review passes.

The ignored .voidfront-agent/run.json identifies an active automation task. Before reclaiming a marker, check the named task with the Codex task tools; a date or absent shell PID alone does not establish inactivity. Create the marker exclusively before editing; release it at a clean checkpoint.
