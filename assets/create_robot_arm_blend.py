"""Run with Blender's Python to create the editable robot_arm.blend project."""
import math
from pathlib import Path

import bpy
import mathutils

OUT = Path(__file__).resolve().parent


def material(name, color, metallic=0.25, roughness=0.3):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.0) if len(mat.diffuse_color) == 4 else color
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
    else:
        # Blender 2.79's internal renderer shades from Material.diffuse_color.
        # Disable nodes there; newer Blender versions use the Principled BSDF above.
        mat.use_nodes = False
    return mat


materials = {
    "steel": material("Steel", (0.22, 0.31, 0.40)),
    "blue": material("Arm Blue", (0.12, 0.42, 0.67)),
    "gold": material("Joint Gold", (0.88, 0.48, 0.15)),
    "dark": material("Dark Metal", (0.08, 0.11, 0.14)),
    "rubber": material("Gripper Rubber", (0.035, 0.045, 0.055), 0.0, 0.72),
}

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def cube(name, center, dimensions, material_name, direction=(1.0, 0.0)):
    try:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=center)
    except TypeError:  # Blender 2.79 used "radius" instead of "size".
        bpy.ops.mesh.primitive_cube_add(radius=1.0, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    obj.rotation_euler[2] = math.atan2(direction[1], direction[0])
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(materials[material_name])
    bevel = obj.modifiers.new("Soft machined edges", "BEVEL")
    bevel.width = 0.035
    bevel.segments = 2
    try:
        obj.modifiers.new("Weighted corner normals", "WEIGHTED_NORMAL")
    except (TypeError, ValueError, RuntimeError):
        pass
    return obj


def cylinder(name, center, radius, depth, material_name, vertices=32, axis="y"):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=center)
    obj = bpy.context.object
    obj.name = name
    if axis == "y":
        obj.rotation_euler[0] = math.pi / 2
    obj.data.materials.append(materials[material_name])
    bevel = obj.modifiers.new("Machined rim", "BEVEL")
    bevel.width = 0.025
    bevel.segments = 2
    try:
        obj.modifiers.new("Weighted corner normals", "WEIGHTED_NORMAL")
    except (TypeError, ValueError, RuntimeError):
        pass
    return obj


def sphere(name, center, radius, material_name):
    try:
        bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, radius=radius, location=center)
    except TypeError:  # Blender 2.79 used "size" for sphere radius.
        bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, size=radius, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(materials[material_name])
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


# The separate solids make the shoulder, elbow, wrist, and gripper easy to edit.
cylinder("Base foot", (0, 0.15, 0), 0.58, 0.30, "dark")
cylinder("Rotating bearing", (0, 0.34, 0), 0.40, 0.14, "steel")
cylinder("Bearing trim", (0, 0.45, 0), 0.31, 0.10, "gold")
sphere("Shoulder joint", (0, 0.67, 0), 0.31, "steel")
cube("Upper arm link", (0.08, 1.13, 0), (1.02, 0.31, 0.30), "blue", (0.20, 0.98))
sphere("Elbow joint", (0.18, 1.63, 0), 0.22, "dark")
cylinder("Elbow pin", (0.18, 1.63, 0), 0.24, 0.38, "gold", axis="z")
cube("Forearm link", (-0.08, 2.02, 0), (0.92, 0.25, 0.24), "gold", (-0.56, 0.83))
sphere("Wrist joint", (-0.34, 2.40, 0), 0.18, "steel")
cube("Tool mount", (-0.34, 2.58, 0), (0.35, 0.20, 0.20), "steel", (0, 1))
cube("Gripper palm", (-0.34, 2.78, 0), (0.12, 0.45, 0.16), "dark")
cube("Left finger", (-0.34, 2.98, -0.18), (0.34, 0.075, 0.075), "rubber", (0, 1))
cube("Right finger", (-0.34, 2.98, 0.18), (0.34, 0.075, 0.075), "rubber", (0, 1))

scene = bpy.context.scene
engine_ids = bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items.keys()
if "BLENDER_EEVEE_NEXT" in engine_ids:
    scene.render.engine = "BLENDER_EEVEE_NEXT"
elif "BLENDER_EEVEE" in engine_ids:
    scene.render.engine = "BLENDER_EEVEE"
elif "CYCLES" in engine_ids:
    scene.render.engine = "CYCLES"
else:
    scene.render.engine = "BLENDER_RENDER"
scene.render.resolution_x = 1200
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
if hasattr(scene.world, "color"):
    scene.world.color = (0.08, 0.08, 0.08)

try:
    bpy.ops.object.light_add(type="SUN", location=(3.5, 5.0, 4.5))
except AttributeError:  # Blender 2.79 called lights "lamps".
    bpy.ops.object.lamp_add(type="SUN", location=(3.5, 5.0, 4.5))
key = bpy.context.object
key.name = "Sun key light"
key.data.energy = 3.0
key_target = mathutils.Vector((0.0, 1.4, 0.0))
key.rotation_euler = (key_target - key.location).to_track_quat("-Z", "Y").to_euler()

bpy.ops.object.camera_add(location=(4.2, 3.2, 6.8))
camera = bpy.context.object
camera.name = "Robot arm preview camera"
target = mathutils.Vector((0.0, 1.45, 0.0))
direction = target - camera.location
camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
camera.data.lens = 52
scene.camera = camera

for obj in scene.objects:
    if hasattr(obj, "select_set"):
        obj.select_set(False)
    else:
        obj.select = False
if hasattr(bpy.context, "view_layer"):
    bpy.context.view_layer.objects.active = None
else:
    scene.objects.active = None
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "robot_arm.blend"))
print("Saved editable Blender model: {}".format(OUT / "robot_arm.blend"))
