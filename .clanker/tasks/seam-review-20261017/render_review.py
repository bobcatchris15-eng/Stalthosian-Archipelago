import bpy
from mathutils import Vector
from pathlib import Path
root=Path('E:/Stalthosian Archipelago/var/seam-review-20261017')
bpy.ops.wm.open_mainfile(filepath=str(root/'derivative.blend'),load_ui=False)
s=bpy.context.scene
s.render.engine='BLENDER_WORKBENCH'
s.render.resolution_x=1000;s.render.resolution_y=800;s.render.resolution_percentage=100
s.display.shading.light='STUDIO';s.display.shading.color_type='SINGLE';s.display.shading.single_color=(0.57,0.49,0.37)
s.display.shading.show_shadows=True;s.display.shading.show_cavity=True
s.display.shading.background_type='WORLD';s.world.color=(0.08,0.08,0.08)
cam=bpy.data.cameras.new('ReviewCamera');ob=bpy.data.objects.new('ReviewCamera',cam);s.collection.objects.link(ob);s.camera=ob
cam.type='ORTHO';cam.ortho_scale=6.4
for name,pos in [('three-quarter',(6,-7,4.2)),('front',(0,-8,2.3)),('side',(7,0.6,2.6))]:
 ob.location=pos;ob.rotation_euler=(Vector((0,0.6,1.3))-ob.location).to_track_quat('-Z','Y').to_euler()
 s.render.filepath=str(root/(name+'.png'));bpy.ops.render.render(write_still=True)
