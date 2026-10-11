"""Offline checks for authored channel extraction in the Blender bake script."""

from pathlib import Path
import importlib.util
import copy
import sys
import types


class Socket:
    def __init__(self, name, value=None):
        self.name = name
        self.default_value = value
        self.links = []
        self.is_linked = False


class Node:
    def __init__(self, kind, inputs=None, outputs=None):
        self.type = kind
        self.name = kind
        self.inputs = inputs or {}
        self.outputs = outputs or {}
        self.location = types.SimpleNamespace(x=0, y=0)
        self.is_active_output = True
        for socket in (*self.inputs.values(), *self.outputs.values()):
            socket.node = self


class Links(list):
    def new(self, source, target):
        link = types.SimpleNamespace(from_socket=source, to_socket=target,
                                     from_node=source.node, to_node=target.node)
        self.append(link)
        target.links.append(link)
        target.is_linked = True

    def remove(self, link):
        super().remove(link)
        link.to_socket.links.remove(link)
        link.to_socket.is_linked = bool(link.to_socket.links)


class Nodes(list):
    def new(self, kind):
        if kind == "ShaderNodeEmission":
            node = Node("EMISSION", {"Color": Socket("Color"), "Strength": Socket("Strength", 1.0)},
                        {"Emission": Socket("Emission")})
        else:
            node = Node(kind)
        for socket in (*node.inputs.values(), *node.outputs.values()):
            socket.node = node
        self.append(node)
        return node


class Material(dict):
    def __init__(self, name, color_source, rough_source):
        super().__init__()
        self.name = name
        self.use_nodes = True
        self.node_tree = types.SimpleNamespace()
        color = Socket("Base Color", (0.2, 0.3, 0.4, 1.0))
        rough = Socket("Roughness", 0.37)
        principled = Node("BSDF_PRINCIPLED", {"Base Color": color, "Roughness": rough},
                          {"BSDF": Socket("BSDF")})
        color.node = principled
        rough.node = principled
        output_surface = Socket("Surface")
        output = Node("OUTPUT_MATERIAL", {"Surface": output_surface})
        output_surface.node = output
        nodes = Nodes([principled, output])
        links = Links()
        if not hasattr(color_source, "node"):
            color_source.node = Node("TEX_NOISE", outputs={"Color": color_source})
        if not hasattr(rough_source, "node"):
            rough_source.node = Node("TEX_NOISE", outputs={"Fac": rough_source})
        links.new(color_source, color)
        links.new(rough_source, rough)
        links.new(principled.outputs["BSDF"], output_surface)
        self.node_tree.nodes = nodes
        self.node_tree.links = links

    def copy(self):
        return copy.deepcopy(self)


def load_bake(monkeypatch):
    fake_bpy = types.ModuleType("bpy")
    fake_bpy.types = types.SimpleNamespace(Material=object, Image=object)
    fake_s = types.ModuleType("_stalthos")
    fake_s.bootstrap = lambda: None
    fake_mat = types.ModuleType("_materials")
    monkeypatch.setitem(sys.modules, "bpy", fake_bpy)
    monkeypatch.setitem(sys.modules, "_stalthos", fake_s)
    monkeypatch.setitem(sys.modules, "_materials", fake_mat)
    script = Path(__file__).parents[1] / "src/stalthos_toolkit/blender/scripts/bake.py"
    spec = importlib.util.spec_from_file_location("authored_bake_script", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_authored_color_and_roughness_links_feed_unlit_targets(monkeypatch):
    bake = load_bake(monkeypatch)
    color_source = Socket("Color", (0.8, 0.1, 0.06, 1.0))
    rough_source = Socket("Fac", 0.82)
    authored = Material("coral_leaf", color_source, rough_source)
    image = types.SimpleNamespace()
    bake._activate_bake_target = lambda material, target: setattr(material, "active_target", target)

    albedo = bake._authored_channel_material(authored, image, "albedo")
    roughness = bake._authored_channel_material(authored, image, "roughness")

    assert albedo["stalthos_authored_channel"] == "albedo"
    assert roughness["stalthos_authored_channel"] == "roughness"
    for material, authored_source in ((albedo, color_source), (roughness, rough_source)):
        emission = next(node for node in material.node_tree.nodes if node.type == "EMISSION")
        output = next(node for node in material.node_tree.nodes if node.type == "OUTPUT_MATERIAL")
        assert emission.inputs["Color"].links[0].from_socket.name == authored_source.name
        assert output.inputs["Surface"].links[0].from_node is emission
        assert material.active_target is image
    assert authored.node_tree.nodes[-1].inputs["Surface"].links[0].from_node.type == "BSDF_PRINCIPLED"


def test_unsupported_authored_shader_is_declined_for_typed_fallback(monkeypatch):
    bake = load_bake(monkeypatch)
    unsupported = types.SimpleNamespace(name="volume", use_nodes=False, node_tree=None)
    assert bake._authored_channel_material(unsupported, object(), "albedo") is None


def test_unlinked_roughness_default_becomes_grayscale_emission(monkeypatch):
    bake = load_bake(monkeypatch)
    authored = Material("matte_leaf", Socket("Color", (0.2, 0.7, 0.1, 1.0)),
                        Socket("Fac", 0.82))
    principled = next(node for node in authored.node_tree.nodes
                      if node.type == "BSDF_PRINCIPLED")
    roughness_socket = principled.inputs["Roughness"]
    authored.node_tree.links.remove(roughness_socket.links[0])
    roughness_socket.default_value = 0.41
    bake._activate_bake_target = lambda material, target: None

    target = bake._authored_channel_material(authored, object(), "roughness")
    emission = next(node for node in target.node_tree.nodes if node.type == "EMISSION")

    assert not emission.inputs["Color"].is_linked
    assert tuple(emission.inputs["Color"].default_value) == (0.41, 0.41, 0.41, 1.0)
