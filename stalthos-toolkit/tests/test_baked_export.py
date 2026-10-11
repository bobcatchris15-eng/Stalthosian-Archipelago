"""Offline checks for image-backed material bindings in the baked GLB bridge."""

from stalthos_toolkit.blender.scripts.export_baked import validate_material_bindings
from pathlib import Path


def fixture_gltf():
    return {
        "images": [{"name": "leaf_albedo"}, {"name": "leaf_roughness"},
                   {"name": "leaf_normal"}, {"name": "stem_albedo"},
                   {"name": "stem_roughness"}, {"name": "stem_normal"}],
        "textures": [{"source": index} for index in range(6)],
        "materials": [
            {"name": "Leaf_baked", "pbrMetallicRoughness": {
                "baseColorTexture": {"index": 0}, "metallicRoughnessTexture": {"index": 1}},
             "normalTexture": {"index": 2}},
            {"name": "Stem_baked", "pbrMetallicRoughness": {
                "baseColorTexture": {"index": 3}, "metallicRoughnessTexture": {"index": 4}},
             "normalTexture": {"index": 5}},
        ],
        "meshes": [{"primitives": [
            {"material": 0, "attributes": {"POSITION": 0, "TEXCOORD_0": 1}},
            {"material": 1, "attributes": {"POSITION": 2, "TEXCOORD_0": 3}},
        ]}],
    }


def test_baked_materials_keep_per_material_channel_and_uv_bindings():
    expected = {
        "Leaf_baked": {"albedo": "leaf_albedo", "roughness": "leaf_roughness",
                       "normal": "leaf_normal"},
        "Stem_baked": {"albedo": "stem_albedo", "roughness": "stem_roughness",
                       "normal": "stem_normal"},
    }

    result = validate_material_bindings(fixture_gltf(), expected)

    assert result == {"ok": True, "materials_checked": 2,
                      "texture_bindings_checked": 6, "issues": []}


def test_baked_export_rejects_cross_bound_material_or_missing_uvs():
    gltf = fixture_gltf()
    gltf["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"] = 3
    gltf["meshes"][0]["primitives"][1]["attributes"].pop("TEXCOORD_0")
    expected = {"Leaf_baked": {"albedo": "leaf_albedo"},
                "Stem_baked": {"albedo": "stem_albedo"}}

    result = validate_material_bindings(gltf, expected)

    assert result["ok"] is False
    assert any("expected 'leaf_albedo'" in issue for issue in result["issues"])
    assert any("has no TEXCOORD_0 UVs" in issue for issue in result["issues"])


def test_bake_restores_authored_multislot_objects_between_passes(monkeypatch, tmp_path):
    """Exercise bake orchestration with fake bpy so slot and image routing regressions surface offline."""
    import importlib.util
    import sys
    import types

    class Material(dict):
        def __init__(self, name):
            super().__init__()
            self.name = name
            self.users = 0

        def copy(self):
            clone = Material(self.name + "_copy")
            clone.users = 1
            return clone

    class Slots(list):
        def clear(self):
            super().clear()

        def append(self, item):
            super().append(item)

    class Polygon:
        def __init__(self, index):
            self.material_index = index

    class Obj(dict):
        type = "MESH"
        hide_render = False

        def __init__(self, name, mats, indices):
            super().__init__()
            self.name = name
            self.data = types.SimpleNamespace(materials=Slots(mats),
                                              polygons=[Polygon(i) for i in indices])
            self.material_slots = []
            self.refresh_slots()

        def refresh_slots(self):
            self.material_slots = [types.SimpleNamespace(material=m) for m in self.data.materials]

        def select_set(self, value):
            pass

    class Image:
        def __init__(self, name):
            self.name = name

        def pack(self):
            pass

    authored = {
        "Leaf": [Material("leaf_skin"), Material("leaf_vein")],
        "Stem": [Material("stem_bark"), Material("stem_cut")],
    }
    objects = [Obj("Leaf", authored["Leaf"], [0, 1, 1]),
               Obj("Stem", authored["Stem"], [1, 0, 1])]
    observations = []
    bake_passes = [("albedo", "albedo"), ("roughness", "rough"), ("normal", "normal")]

    class Materials(list):
        def remove(self, material):
            super().remove(material)

    mats_collection = Materials()
    for obj in objects:
        mats_collection.extend(obj.data.materials)
    fake_bpy = types.ModuleType("bpy")
    fake_bpy.types = types.SimpleNamespace(Object=object, Image=object, Material=object)
    fake_bpy.data = types.SimpleNamespace(objects=objects, materials=mats_collection)
    fake_bpy.context = types.SimpleNamespace(
        scene=types.SimpleNamespace(render=types.SimpleNamespace(), cycles=types.SimpleNamespace()),
        view_layer=types.SimpleNamespace(objects=types.SimpleNamespace(active=None)))
    def do_bake(**kwargs):
        current = {o.name: ([s.material.name for s in o.material_slots],
                            [p.material_index for p in o.data.polygons]) for o in objects}
        observations.append(current)
        for o in objects:
            pass_name = bake_passes[len(observations) - 1][1]
            assert len(current[o.name][0]) == 2
            assert all(pass_name in name for name in current[o.name][0])
            expected_indices = [0, 1, 1] if o.name == "Leaf" else [1, 0, 1]
            assert current[o.name][1] == expected_indices
        return {"FINISHED"}

    fake_bpy.ops = types.SimpleNamespace(object=types.SimpleNamespace(
        select_all=lambda **kwargs: None,
        bake=lambda **kwargs: do_bake(**kwargs)))
    fake_s = types.ModuleType("_stalthos")
    fake_s.bootstrap = lambda: None
    fake_s.save_blend = lambda path: path
    fake_mat = types.ModuleType("_materials")
    def make_temp(name, *args, **kwargs):
        # New pass setup must see the untouched authored slots on every object.
        for obj in objects:
            assert [slot.material for slot in obj.material_slots] == authored[obj.name]
        material = Material(name)
        mats_collection.append(material)
        return material
    fake_mat.structure_emission_material = make_temp
    fake_mat.structure_roughness_material = make_temp
    fake_mat.structure_thickness_material = make_temp
    fake_mat.build_procedural_material = make_temp

    monkeypatch.setitem(sys.modules, "bpy", fake_bpy)
    monkeypatch.setitem(sys.modules, "_stalthos", fake_s)
    monkeypatch.setitem(sys.modules, "_materials", fake_mat)
    script = (Path(__file__).parents[1] / "src/stalthos_toolkit/blender/scripts/bake.py")
    spec = importlib.util.spec_from_file_location("fake_bake_script", script)
    bake_script = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bake_script)

    class Reporter:
        class Timer:
            def __enter__(self): pass
            def __exit__(self, *args): pass
        warnings = []
        def time(self, label): return self.Timer()
        def warn(self, message): self.warnings.append(message)

    image_index = 0
    def new_image(name, size, **kwargs):
        nonlocal image_index
        image_index += 1
        return Image(f"image_{image_index}_{name}")
    bake_script._ensure_uv = lambda obj, report: "existing"
    bake_script._ensure_thickness_attribute = lambda obj, reference, report: "existing"
    bake_script._new_image = new_image
    bake_script._activate_bake_target = lambda mat, img: None
    bake_script._save = lambda img, path: path
    bake_script._image_stats = lambda img: {"flat": False}
    bound_maps = {}
    def make_baked(name, images):
        if name.startswith("leaf_"):
            bound_maps.setdefault("Leaf", []).append(
                {key: value.name for key, value in images.items()})
        elif name.startswith("stem_"):
            bound_maps.setdefault("Stem", []).append(
                {key: value.name for key, value in images.items()})
        return Material(f"{name}_baked")
    bake_script._baked_pbr_material = make_baked
    original_set = bake_script._set_object_materials
    def set_materials(obj, materials, indices):
        original_set(obj, materials, indices)
        obj.refresh_slots()
    bake_script._set_object_materials = set_materials
    result = bake_script.bake({"passes": ["albedo", "roughness", "normal"],
                               "outputs": {"textures": str(tmp_path), "blend": str(tmp_path / "x.blend")}}, Reporter())

    assert len(observations) == 3
    assert result["metrics"]["authored_material_complete"] is False
    assert result["metrics"]["material_export_ready"] is False
    assert any("MATERIAL_CHANNEL_FALLBACK:" in warning for warning in Reporter.warnings)
    assert set(result["object_names"]) == {"Leaf", "Stem"}
    for index, observed in enumerate(observations):
        for name in authored:
            assert observed[name][1] == ([0, 1, 1] if name == "Leaf" else [1, 0, 1])
            assert len(observed[name][0]) == 2
            assert all(bake_passes[index][1] in material for material in observed[name][0])
    for obj_name, slots in bound_maps.items():
        assert len(slots) == 2
        expected_index = 0 if obj_name == "Leaf" else 1
        for maps in slots:
            assert all(image_name.endswith(f"_{expected_index}") for image_name in maps.values())
    for obj in objects:
        assert len(obj.data.materials) == 2
        assert [p.material_index for p in obj.data.polygons] == (
            [0, 1, 1] if obj.name == "Leaf" else [1, 0, 1])
        assert all(m.name.endswith("_baked") for m in obj.data.materials)
