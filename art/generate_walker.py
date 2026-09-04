"""Build the original Cairn survey walker. Blender 5.1.2, no external assets.

Blender +Y is forward; glTF/Godot -Z is forward. All mesh vertices have UVs
and rigid mechanical bone weights. Animation poses are baked at 30 Hz.
"""
import bpy
import math
from mathutils import Vector
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "client" / "assets"
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def material(name, color, metallic=0.0, roughness=0.4, emission=0.0):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Metallic'].default_value = metallic
    bsdf.inputs['Roughness'].default_value = roughness
    if emission:
        bsdf.inputs['Emission Color'].default_value = (*color, 1)
        bsdf.inputs['Emission Strength'].default_value = emission
    return m

ceramic = material('Ceramic | warm ivory', (0.65, 0.57, 0.40), 0.22, 0.32)
brass = material('Brass | machined edges', (0.32, 0.18, 0.065), 0.78, 0.26)
dark = material('Basalt | joint housing', (0.034, 0.046, 0.053), 0.62, 0.3)
team = material('Team', (0.03, 0.67, 0.84), 0.35, 0.27, 0.65)
lens = material('Signal | amber lens', (1.0, 0.35, 0.025), 0.25, 0.2, 3.0)
objects = []

def finish(obj, name, mat, bone, bevel=0.015):
    obj.name = name
    obj.data.materials.append(mat)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = obj.modifiers.new('Machined edge', 'BEVEL')
        mod.width = bevel
        mod.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=1.15192, island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')
    group = obj.vertex_groups.new(name=bone)
    group.add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')
    objects.append(obj)
    return obj

def box(name, pos, dims, mat, bone, bevel=0.015):
    bpy.ops.mesh.primitive_cube_add(size=1, location=pos)
    o = bpy.context.object
    o.dimensions = dims
    return finish(o, name, mat, bone, bevel)

def rod(name, a, b, radius, mat, bone, vertices=10):
    a, b = Vector(a), Vector(b)
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=(b-a).length, location=(a+b)/2)
    o = bpy.context.object
    o.rotation_euler = (b-a).to_track_quat('Z','Y').to_euler()
    return finish(o, name, mat, bone, 0.006)

# Six-sided prow: a low, forward-tapered ceramic command capsule.
verts = [(-.28,-.23,.81),(.28,-.23,.81),(.31,.14,.81),(.16,.36,.81),(-.16,.36,.81),(-.31,.14,.81),
         (-.24,-.20,1.05),(.24,-.20,1.05),(.25,.12,1.05),(.12,.29,.99),(-.12,.29,.99),(-.25,.12,1.05)]
faces = [tuple(range(5,-1,-1)),tuple(range(6,12))]+[(i,(i+1)%6,(i+1)%6+6,i+6) for i in range(6)]
mesh = bpy.data.meshes.new('Prow geometry'); mesh.from_pydata(verts,[],faces); mesh.update()
o = bpy.data.objects.new('Ceramic command capsule',mesh); bpy.context.collection.objects.link(o)
bpy.context.view_layer.objects.active=o; o.select_set(True)
finish(o,o.name,ceramic,'body',.022)
box('Underslung reactor', (0,-.02,.76),(.43,.40,.13),dark,'body')
box('Dorsal team stripe',(0,-.015,1.069),(.085,.39,.024),team,'body',.006)
box('Front optical mask',(0,.309,.924),(.245,.055,.061),dark,'body',.012)
rod('Single amber rangefinder',(-.055,.342,.925),(-.055,.359,.925),.031,lens,'body',12)
box('Sensor index bar',(.061,.345,.925),(.075,.018,.024),team,'body',.002)

# Original asymmetric silhouette: elevated crescent sensor on port side,
# short offset induction tool on starboard; neither resembles a humanoid head.
rod('Sensor mast',(-.21,-.11,1.04),(-.24,-.14,1.32),.024,brass,'body')
rod('Sensor crossbar',(-.32,-.14,1.34),(-.15,-.14,1.34),.028,dark,'body')
box('Sensor ceramic flag',(-.325,-.14,1.355),(.06,.095,.14),ceramic,'body',.012)
box('Sensor luminous flag',(-.325,-.085,1.355),(.035,.012,.095),team,'body',.003)
rod('Tool pivot',(.24,.04,.94),(.37,.04,.94),.068,brass,'body',12)
box('Induction carriage',(.395,.065,.965),(.14,.25,.13),dark,'tool')
box('Induction ceramic shroud',(.395,.08,1.044),(.16,.22,.045),ceramic,'tool')
rod('Induction barrel',(.395,.14,.966),(.395,.365,.966),.048,brass,'tool',8)
rod('Dark bore',(.395,.36,.966),(.395,.383,.966),.034,dark,'tool',8)
rod('Live coil',(.395,.225,.966),(.395,.261,.966),.055,team,'tool',8)
for i in range(4):
    box('Rear heat fin %d'%i,(-.13+i*.085,-.235,.945),(.035,.055,.14),brass,'body',.004)

bone_defs = [('root',(0,0,0),(0,0,.2),None),('body',(0,0,.77),(0,0,1.0),'root'),('tool',(.395,.04,.94),(.395,.24,.94),'body')]
legs=[]
for side,s in [('L',-1),('R',1)]:
    for end,f in [('front',1),('rear',-1)]:
        name=side+'_'+end
        hip=(s*.22,f*.18,.80); knee=(s*.43,f*.30,.47); ankle=(s*.43,f*.41,.13)
        upper=name+'_upper'; lower=name+'_lower'; foot=name+'_foot'
        bone_defs.extend([(upper,hip,knee,'body'),(lower,knee,ankle,upper),(foot,ankle,(s*.43,f*.47,.08),lower)])
        legs.append((name,s,f))
        rod(name+' hip axle',(s*.18,f*.18,.8),(s*.32,f*.18,.8),.088,brass,upper,12)
        rod(name+' upper strut',hip,knee,.071,dark,upper)
        armor=box(name+' upper ceramic',(s*.36,f*.24,.66),(.17,.19,.27),ceramic,upper,.025)
        armor.rotation_euler[1]=s*.42
        box(name+' team inlay',(s*.36,f*.24,.808),(.11,.105,.018),team,upper,.004)
        rod(name+' knee axle',(s*.36,f*.30,.47),(s*.49,f*.30,.47),.066,brass,lower,12)
        rod(name+' shin piston',knee,ankle,.04,brass,lower)
        rod(name+' shin housing',(s*.43,f*.32,.41),(s*.43,f*.385,.22),.061,dark,lower,8)
        box(name+' foot',(s*.43,f*.445,.085),(.17,.225,.09),dark,foot,.018)
        box(name+' foot ceramic toe',(s*.43,f*.505,.132),(.15,.09,.025),ceramic,foot,.006)

# One skinned mesh with five material surfaces, rather than 56 draw objects.
bpy.ops.object.select_all(action='DESELECT')
for obj in objects: obj.select_set(True)
bpy.context.view_layer.objects.active=objects[0]
bpy.ops.object.join()
objects=[bpy.context.object]
objects[0].name='CairnWalkerMesh'
bpy.ops.object.select_all(action='DESELECT')
armdata=bpy.data.armatures.new('Cairn mechanical skeleton')
arm=bpy.data.objects.new('CairnWalker',armdata); bpy.context.collection.objects.link(arm)
bpy.context.view_layer.objects.active=arm; arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for name,head,tail,parent in bone_defs:
    b=armdata.edit_bones.new(name); b.head=head; b.tail=tail
    if parent: b.parent=armdata.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')
for obj in objects:
    mod=obj.modifiers.new('Mechanical skin','ARMATURE'); mod.object=arm
    obj.parent=arm
for pb in arm.pose.bones: pb.rotation_mode='XYZ'

scene=bpy.context.scene; scene.render.fps=30
clips={'idle':60,'walk':30,'attack':24,'hit':15,'death':42}
for clip, duration in clips.items():
    action=bpy.data.actions.new(clip)
    arm.animation_data_create(); arm.animation_data.action=action
    for frame in range(1,duration+2):
        t=(frame-1)/duration
        for pb in arm.pose.bones: pb.location=(0,0,0); pb.rotation_euler=(0,0,0); pb.scale=(1,1,1)
        body=arm.pose.bones['body']; tool=arm.pose.bones['tool']
        if clip=='idle':
            body.location.y=.008*math.sin(t*math.tau)
            body.rotation_euler[1]=.012*math.sin(t*math.tau)
        elif clip=='walk':
            body.location.y=.016*math.cos(t*math.tau*2)
            body.rotation_euler[1]=.025*math.sin(t*math.tau)
            for name,s,f in legs:
                phase=t*math.tau+(0 if s*f>0 else math.pi)
                arm.pose.bones[name+'_upper'].rotation_euler[0]=.28*math.sin(phase)
                arm.pose.bones[name+'_lower'].rotation_euler[0]=-.32*max(0,math.cos(phase))
                arm.pose.bones[name+'_foot'].rotation_euler[0]=-.08*math.sin(phase)
        elif clip=='attack':
            kick=math.exp(-((t-.19)/.09)**2)
            tool.location.y=-.10*kick; body.rotation_euler[0]=-.045*kick
        elif clip=='hit':
            shock=math.sin(t*math.pi)*math.exp(-t*3)
            body.rotation_euler[1]=.24*shock; body.rotation_euler[0]=.12*shock
        else:
            fall=min(1,t*1.6); eased=fall*fall*(3-2*fall)
            body.location.y=-.46*eased
            body.rotation_euler[1]=.35*eased
            body.rotation_euler[0]=-.18*eased
            for name,s,f in legs:
                arm.pose.bones[name+'_upper'].rotation_euler[1]=s*.55*eased
                arm.pose.bones[name+'_lower'].rotation_euler[0]=f*.55*eased
        for pb in arm.pose.bones:
            pb.keyframe_insert('location',frame=frame,group=pb.name)
            pb.keyframe_insert('rotation_euler',frame=frame,group=pb.name)
            pb.keyframe_insert('scale',frame=frame,group=pb.name)
    action.use_fake_user=True
arm.animation_data.action=bpy.data.actions['idle']
scene.frame_set(1); scene.frame_start=1; scene.frame_end=61

# Keep the source portable: preview stage, lights and camera are excluded from GLB.
bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True)
for obj in objects: obj.select_set(True)
bpy.context.view_layer.objects.active=arm
bpy.ops.export_scene.gltf(filepath=str(OUT/'cairn_walker.glb'),export_format='GLB',use_selection=True,
    export_animations=True,export_animation_mode='ACTIONS',export_bake_animation=True,
    export_skins=True,export_yup=True,export_apply=False,export_optimize_animation_size=False)

stage=material('Preview stage',(0.065,.078,.084),.12,.65)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,.025)); bpy.context.object.data.materials.append(stage)
for pos,energy,size in [((3,1,5),500,4),((-3,2,3),350,3),((0,-3,4),650,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos)
    light=bpy.context.object; light.data.energy=energy; light.data.shape='DISK'; light.data.size=size
    light.rotation_euler=(Vector((0,0,.6))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2.6,3.8,3.3)); camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,.66))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO'; camera.data.ortho_scale=2.6; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32
scene.render.resolution_x=1000; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.world.color=(.13,.13,.13)
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'art'/'walker_preview.png')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'art'/'cairn_walker.blend'))
bpy.ops.render.render(write_still=True)
print('CAIRN_ASSET_EXPORTED',OUT/'cairn_walker.glb')
