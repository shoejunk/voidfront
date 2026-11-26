"""Voidfront static resource models for Blender 5.1.2.

Each scene in deposits.blend has its own ground-centered export origin. Edit
the scenes and run tools/export_assets.ps1 -DepositsOnly to export. Use
-RecreateDeposits explicitly to replace the editable file with the seed art.
"""
import argparse
import math
from pathlib import Path
import sys

import bmesh
import bpy


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "art" / "deposits.blend"
ASSETS = {"Salvage": "salvage_deposit.glb", "Flux": "flux_deposit.glb"}


def material(name, color, metallic, roughness, glow=0):
    surface = bpy.data.materials.new(name)
    surface.diffuse_color = (*color, 1)
    surface.use_nodes = True
    shader = surface.node_tree.nodes.get("Principled BSDF")
    for socket, value in (("Base Color", (*color, 1)), ("Metallic", metallic),
                          ("Roughness", roughness)):
        shader.inputs[socket].default_value = value
    if glow:
        shader.inputs["Emission Color"].default_value = (*color, 1)
        shader.inputs["Emission Strength"].default_value = glow
    return surface


class Mesh:
    def __init__(self):
        self.vertices = []
        self.faces = []
        self.surfaces = []

    def part(self, vertices, faces):
        start = len(self.vertices)
        self.vertices.extend(vertices)
        for indices, surface in faces:
            self.faces.append(tuple(start + i for i in indices))
            self.surfaces.append(surface)

    def finish(self, name, materials):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.vertices, [], self.faces)
        for surface in materials:
            mesh.materials.append(surface)
        for polygon, surface in zip(mesh.polygons, self.surfaces):
            polygon.material_index = surface
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(mesh)
        bm.free()
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(island_margin=0.025)
        bpy.ops.object.mode_set(mode="OBJECT")


def rim(mesh, center, outer, inner, bottom, height, start, end, sections, toothed):
    """A broken casting, with an inset rim bevel and open, bright cut faces."""
    vertices, faces = [], []
    for i in range(sections + 1):
        angle = math.radians(start + (end - start) * i / sections)
        radius = outer + (0.028 if toothed and i % 4 in (1, 2) else 0)
        profile = [(inner, bottom), (radius, bottom), (radius, bottom + height - 0.024),
                   (radius - 0.022, bottom + height), (inner + 0.018, bottom + height),
                   (inner, bottom + height - 0.02)]
        for r, z in profile:
            vertices.append((center[0] + r * math.cos(angle), center[1] + r * math.sin(angle), z))
        if i:
            for j, surface in enumerate((0, 0, 2, 1, 2, 0)):
                a = (i - 1) * 6 + j
                b = (i - 1) * 6 + (j + 1) % 6
                faces.append(((a, b, b + 6, a + 6), surface))
    faces.extend([(tuple(range(6)), 2), (tuple(range(sections * 6, sections * 6 + 6)), 2)])
    mesh.part(vertices, faces)


def ingot(mesh, footprint, base, height):
    """A sheared metal bar, with four broad top-edge bevels."""
    cx = sum(p[0] for p in footprint) / len(footprint)
    cy = sum(p[1] for p in footprint) / len(footprint)
    vertices = [(x, y, base) for x, y in footprint]
    vertices += [(x, y, base + height * 0.68) for x, y in footprint]
    vertices += [(cx + (x - cx) * 0.93, cy + (y - cy) * 0.82, base + height) for x, y in footprint]
    n = len(footprint)
    faces = [(tuple(range(n)), 0), (tuple(range(2 * n, 3 * n)), 1)]
    for i in range(n):
        j = (i + 1) % n
        faces.extend([((i, j, n + j, n + i), 0), ((n + i, n + j, 2*n + j, 2*n + i), 2)])
    mesh.part(vertices, faces)


def salvage():
    mesh = Mesh()
    # The missing sector faces the camera. A lower second ring is visible
    # under the fractured crown. Two cut-off bars lie across the open side.
    rim(mesh, (-0.04, 0.03), 0.355, 0.22, 0.0, 0.077, -61, 208, 24, False)
    rim(mesh, (-0.04, 0.03), 0.377, 0.222, 0.077, 0.115, -28, 211, 24, True)
    rim(mesh, (-0.04, 0.03), 0.340, 0.262, 0.192, 0.053, 16, 151, 12, False)
    ingot(mesh, [(-0.175,-0.266), (0.276,-0.375), (0.342,-0.313), (-0.159,-0.182)], 0.0, 0.084)
    ingot(mesh, [(-0.095,-0.155), (0.314,-0.252), (0.354,-0.155), (-0.062,-0.063)], 0.0, 0.112)
    mesh.finish("Fractured flywheel and cut stock", [
        material("Oxide in casting grooves", (0.12, 0.054, 0.026), 0.4, 0.55),
        material("Reclaimed ochre brass", (0.72, 0.41, 0.115), 0.45, 0.48),
        material("Machined brass edge", (0.86, 0.62, 0.24), 0.55, 0.33),
    ])


def shard(mesh, root, tip, width, crest):
    """Folded mineral plates have a high spine, a low keel and broad facets."""
    x, y, z = root
    dx, dy = tip[0] - x, tip[1] - y
    length = math.hypot(dx, dy)
    wx, wy = -width * dy / length, width * dx / length
    sx, sy = x + dx * 0.43, y + dy * 0.43
    vertices = [root, (sx + wx, sy + wy, crest * 0.48), tip,
                (sx - wx, sy - wy, crest * 0.58), (sx, sy, crest), (sx, sy, 0.014)]
    mesh.part(vertices, [
        ((0, 1, 4), 1), ((1, 2, 4), 2), ((2, 3, 4), 1), ((3, 0, 4), 0),
        ((0, 5, 1), 0), ((1, 5, 2), 0), ((2, 5, 3), 1), ((3, 5, 0), 0),
    ])


def flux():
    mesh = Mesh()
    shard(mesh, (-0.13, -0.13, 0), (0.21, 0.44, 0.32), 0.117, 0.28)
    shard(mesh, (-0.12, -0.14, 0), (-0.44, 0.14, 0.24), 0.093, 0.21)
    shard(mesh, (-0.05, -0.13, 0), (0.44, -0.03, 0.19), 0.094, 0.175)
    shard(mesh, (-0.13, -0.09, 0), (-0.30, -0.405, 0.135), 0.070, 0.145)
    mesh.finish("Four folded flux plates", [
        material("Flux slate keel", (0.026, 0.20, 0.24), 0.18, 0.48),
        material("Petrol crystal", (0.024, 0.51, 0.63), 0.18, 0.35, 0.06),
        material("Ice on the cleavage plane", (0.33, 0.86, 0.88), 0.13, 0.32, 0.07),
    ])


def create_source():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.name = "Salvage"
    salvage()
    bpy.context.window.scene = bpy.data.scenes.new("Flux")
    flux()
    bpy.context.window.scene = bpy.data.scenes["Salvage"]
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == "VIEW_3D":
                area.spaces.active.region_3d.view_distance = 2.5
                area.spaces.active.shading.color_type = "MATERIAL"
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))


def export(folder):
    for scene, filename in ASSETS.items():
        bpy.context.window.scene = bpy.data.scenes[scene]
        bpy.ops.export_scene.gltf(filepath=str(folder / filename), export_format="GLB",
                                  use_active_scene=True, export_texcoords=True,
                                  export_normals=True, export_animations=False,
                                  export_cameras=False, export_lights=False)


if __name__ == "__main__":
    options = argparse.ArgumentParser(description=__doc__)
    options.add_argument("--recreate-source", action="store_true")
    options.add_argument("--output", type=Path, default=ROOT / "client" / "assets")
    args = options.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    if args.recreate_source:
        create_source()
    else:
        bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    export(args.output)
