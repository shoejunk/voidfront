"""Export the editable Cairn command anchor with Blender 5.1.2."""
import argparse
from pathlib import Path
import sys

import bpy


ART = Path(__file__).resolve().parent
SOURCE = ART / "command_anchor.blend"
sys.path.insert(0, str(ART))
from generate_deposits import Mesh, material


def shell(mesh, outline, levels, lid):
    """Loft an armored casting through scaled rings about its plan center."""
    cx = sum(x for x, _ in outline) / len(outline)
    cy = sum(y for _, y in outline) / len(outline)
    vertices = []
    faces = []
    count = len(outline)
    for n, (height, scale_x, scale_y, surface) in enumerate(levels):
        vertices += [(cx+(x-cx)*scale_x, cy+(y-cy)*scale_y, height) for x, y in outline]
        if n:
            for i in range(count):
                a = (n-1)*count+i
                b = (n-1)*count+(i+1) % count
                faces.append(((a, b, b+count, a+count), surface))
    faces.append((tuple(range(count)), 0))
    faces.append((tuple(range(len(vertices)-count, len(vertices))), lid))
    mesh.part(vertices, faces)


def octagon(x, y, half_x, half_y, cut):
    return [(x+dx, y+dy) for dx, dy in [
        (-half_x+cut,-half_y),(half_x-cut,-half_y),(half_x,-half_y+cut),(half_x,half_y-cut),
        (half_x-cut,half_y),(-half_x+cut,half_y),(-half_x,half_y-cut),(-half_x,-half_y+cut)]]


def create_source():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.name = "Cairn command anchor"
    # Blender -Y faces the camera. Keep the rear cargo apron and centerline
    # low, with the elevated signal vanes on the forward shoulder brackets.
    mesh = Mesh()
    shell(mesh, octagon(0,0,0.97,0.97,0.31),
          [(0,0.92,0.92,0),(0.09,1,1,0),(0.19,1,1,0),(0.25,0.95,0.95,2)], 0)
    for side in (-1,1):
        shoulder = [(side*x,y) for x,y in [(0.30,-0.87),(0.68,-0.87),(0.91,-0.62),
                    (0.86,0.52),(0.62,0.88),(0.40,0.86),(0.28,0.24)]]
        shell(mesh, shoulder, [(0.24,1,1,0),(0.40,1,1,0),(0.50,0.84,0.89,1)], 3)
        # Two raised collars and the slanted fin separate the twin signal
        # vanes from the broad radial flywheel at harvest sites.
        shell(mesh, octagon(side*0.62,-0.44,0.20,0.27,0.075),
              [(0.48,1,1,0),(0.69,0.83,0.8,2),(0.74,0.86,0.82,1)], 1)
        footprint = [(side*x,y) for x,y in [(0.54,-0.57),(0.71,-0.56),(0.71,-0.37),(0.55,-0.31)]]
        shell(mesh, footprint, [(0.72,1,1,0),(1.24,0.83,0.72,3),(1.35,0.65,0.61,2)], 2)
    # Recessed spine and shallow ceramic operator hood between the vanes.
    shell(mesh, octagon(0,-0.18,0.45,0.61,0.17),
          [(0.25,1,1,0),(0.46,0.99,0.99,2),(0.56,0.86,0.90,1),(0.72,0.68,0.66,1)], 1)
    shell(mesh, octagon(0,-0.23,0.23,0.29,0.07),
          [(0.71,1,1,0),(0.77,0.88,0.91,0)], 3)
    for y in (0.48,0.63,0.78):
        shell(mesh, octagon(0,y,0.27,0.05,0.035),
              [(0.25,1,1,0),(0.29,0.94,0.88,2)], 2)
    mesh.finish("Anchor with loading apron and signal vanes", [
        material("Basalt recessed metal", (0.085,0.11,0.12), 0.48, 0.54),
        material("Ceramic cast armor", (0.63,0.58,0.44), 0.14, 0.45),
        material("Machined brass", (0.56,0.30,0.093), 0.61, 0.40),
        material("Team enamel", (0.09,0.42,0.39), 0.28, 0.40),
    ])
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recreate-source", action="store_true")
    parser.add_argument("--output", type=Path, default=ART.parent / "client" / "assets" / "command_anchor.glb")
    args = parser.parse_args(sys.argv[sys.argv.index("--")+1:] if "--" in sys.argv else [])
    if args.recreate_source:
        create_source()
    else:
        bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    bpy.ops.export_scene.gltf(filepath=str(args.output), export_format="GLB",
                              export_texcoords=True, export_normals=True, export_animations=False,
                              export_cameras=False, export_lights=False)
