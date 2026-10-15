from __future__ import annotations

import pytest
from pathlib import Path

from stalthos_toolkit.schemas.formats import local_media_type

def test_local_media_type_images():
    assert local_media_type("image.png") == "image"
    assert local_media_type("photo.JPG") == "image"
    assert local_media_type("texture.webp") == "image"
    assert local_media_type("scan.exr") == "image"
    assert local_media_type("data.TIF") == "image"
    assert local_media_type("data.tiff") == "image"

def test_local_media_type_models():
    assert local_media_type("model.blend") == "model"
    assert local_media_type("mesh.glb") == "model"
    assert local_media_type("scene.gltf") == "model"
    assert local_media_type("asset.fbx") == "model"
    assert local_media_type("raw.obj") == "model"

def test_local_media_type_scans():
    assert local_media_type("pointcloud.ply") == "scan"
    assert local_media_type("mesh.stl") == "scan"
    assert local_media_type("cad.stp") == "scan"

def test_local_media_type_text():
    assert local_media_type("readme.md") == "text"
    assert local_media_type("notes.txt") == "text"

def test_local_media_type_unrecognized_or_missing():
    assert local_media_type("data.bin") is None
    assert local_media_type("archive.zip") is None
    assert local_media_type("no_extension") is None
    assert local_media_type(".hidden_file") is None # Path(".hidden_file").suffix is '.hidden_file', not in LOCAL_SUFFIXES

def test_local_media_type_paths_and_whitespace():
    assert local_media_type("  /path/to/image.png  ") == "image"
    assert local_media_type("C:\\windows\\path\\model.GLB") == "model"
    assert local_media_type("\tfile.txt\n") == "text"
