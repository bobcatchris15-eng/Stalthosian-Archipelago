import bpy
from mathutils import Vector
bpy.ops.wm.open_mainfile(filepath=r'var/luna-walk-cycle/sources/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.rig.blend',load_ui=False)
arm=bpy.data.objects['creature_rig']; body=bpy.data.objects['body_proxy']; hlen=.07488; stance=.95; legr=.0315
boxes={}; excluded=set()
for side in ('front_L','front_R','rear_L','rear_R'):
 a=arm.pose.bones[f'toe_{side}'].head; center=Vector((a.x+hlen*.25,a.y,a.z-max(.02,stance*.06))); idx=[]; pts=[]
 for v in body.data.vertices:
  q=v.co
  if abs(q.x-center.x)<=hlen*.5+1e-5 and abs(q.y-center.y)<=legr*1.1+1e-5 and abs(q.z-center.z)<=legr*.5+1e-5: idx.append(v.index); pts.append(q.copy())
 boxes[side]=idx; excluded.update(idx)
 print('FOOTPAD',side,'vertices=',len(idx),'world_z_bounds=',(min(p.z for p in pts),max(p.z for p in pts)) if pts else None)
rest=[v.co.z for v in body.data.vertices if v.index not in excluded]
print('NON_PAD_BODY_Z',min(rest),max(rest),'low_vertex',tuple(body.data.vertices[min(range(len(body.data.vertices)),key=lambda i:body.data.vertices[i].co.z)].co))
