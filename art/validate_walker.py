"""Structural evidence for exported asset; runtime deformation remains a separate gate."""
from pathlib import Path
import json
import struct
root=Path(__file__).resolve().parents[1]
data=(root/'client/assets/cairn_walker.glb').read_bytes()
magic,version,length=struct.unpack_from('<III',data)
assert magic==0x46546c67 and version==2 and length==len(data)
chunk_length,chunk_type=struct.unpack_from('<II',data,12)
assert chunk_type==0x4e4f534a
gltf=json.loads(data[20:20+chunk_length])
binary_start=20+chunk_length+8
component={5121:('B',1),5123:('H',2),5125:('I',4),5126:('f',4)}
width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
def values(index):
    a=gltf['accessors'][index]; v=gltf['bufferViews'][a['bufferView']]
    fmt,size=component[a['componentType']]; count=width[a['type']]
    offset=binary_start+v.get('byteOffset',0)+a.get('byteOffset',0)
    stride=v.get('byteStride',size*count)
    return [struct.unpack_from('<'+fmt*count,data,offset+i*stride) for i in range(a['count'])]
names=[a['name'] for a in gltf['animations']]
assert set(names)=={'idle','walk','attack','hit','death'}, names
assert len(gltf['skins'])>=1
joints=max(len(s['joints']) for s in gltf['skins'])
assert joints==15,joints
primitives=[p for m in gltf['meshes'] for p in m['primitives']]
for p in primitives:
    assert {'POSITION','NORMAL','TEXCOORD_0','JOINTS_0','WEIGHTS_0'}<=set(p['attributes']),p['attributes']
    for weight in values(p['attributes']['WEIGHTS_0']):
        assert abs(sum(weight)-1)<1e-5 and min(weight)>=0,weight
    for joint in values(p['attributes']['JOINTS_0']):
        assert min(joint)>=0 and max(joint)<joints,joint
assert len(primitives)==5,len(primitives)
for animation in gltf['animations']:
    assert any(len(set(values(s['output'])))>1 for s in animation['samplers']),animation['name']
assert 'Team' in [m['name'] for m in gltf['materials']]
counts={'joints':joints,'mesh_primitives':len(primitives),'animations':names,'glb_bytes':len(data),
        'vertices':sum(gltf['accessors'][p['attributes']['POSITION']]['count'] for p in primitives),
        'triangles':sum(gltf['accessors'][p['indices']]['count']//3 for p in primitives)}
import bpy
from mathutils import Vector
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(root/'client/assets/cairn_walker.glb'))
bpy.context.view_layer.update()
# Blender's importer creates an unlinked Icosphere as a bone widget; exclude it.
points=[o.matrix_world@Vector(v) for o in bpy.context.scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers) for v in o.bound_box]
counts['bind_dimensions_blender_xyz']=[round(max(v[i] for v in points)-min(v[i] for v in points),4) for i in range(3)]
counts['ground_min_z']=round(min(v.z for v in points),4)
assert .9<counts['bind_dimensions_blender_xyz'][0]<1.2
assert .9<counts['bind_dimensions_blender_xyz'][1]<1.3
assert 1.2<counts['bind_dimensions_blender_xyz'][2]<1.6
assert 0<=counts['ground_min_z']<.05
print(json.dumps(counts,indent=2))
(root/'art/validation.json').write_text(json.dumps(counts,indent=2)+'\n')
