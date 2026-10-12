"""Actual Blender contract for consuming the generated Smalltrotter joint graph."""

from __future__ import annotations

import json
import math
import os
from pathlib import Path

import pytest

from stalthos_toolkit.cli import main
from stalthos_toolkit.core.errors import EXIT_OK, MissingDependency
from stalthos_toolkit.core.workspace import Workspace, find_blender


@pytest.fixture
def blender_path():
    try:
        return find_blender(os.environ.get("STALTHOS_TEST_BLENDER"))
    except MissingDependency:
        pytest.skip("Blender is not installed on this machine")


def _run(command: str, workspace: Workspace, definition: Path, blender: str,
         capsys, *extra: str) -> dict:
    code = main(["--workspace", str(workspace.root), command, "-d", str(definition),
                 "--seed", "42", "--stage", "independent_juvenile",
                 "--blender", blender, "--timeout", "900", *extra])
    envelope = json.loads(capsys.readouterr().out)
    assert code == EXIT_OK, envelope
    assert envelope["ok"] is True
    return envelope


def test_smalltrotter_rig_uses_generated_shape_and_binds_reference_features(
        tmp_path, capsys, blender_path):
    toolkit = Path(__file__).resolve().parents[1]
    definition = toolkit / "species" / "creatures" / "smalltrotter.yaml"
    workspace = Workspace.open(tmp_path / "workspace")
    _run("build_creature", workspace, definition, blender_path, capsys)
    rigged = _run("rig_creature", workspace, definition, blender_path, capsys)

    metrics = rigged["metrics"]
    source = metrics["rig_source"]
    assert source["kind"] == "serialized_generated_shape_and_joint_graph"
    assert source["profile"] == "smalltrotter_reference_v1"
    alignment = metrics["source_joint_alignment"]
    assert math.isfinite(alignment["max_error_m"])
    assert alignment["max_error_m"] <= 1e-5
    assert set(alignment["per_chain_error_m"]) == {
        "spine", "neck", "head", "tail",
        "limbs.front_left", "limbs.front_right", "limbs.rear_left", "limbs.rear_right",
    }

    features = metrics["skinned_feature_geometry"]
    assert metrics["skinned_feature_geometry_passed"] is True
    assert set(features) == {
        "smalltrotter_eye_and_nostril_blockout",
        "smalltrotter_bristle_ridge_blockout",
    }
    for row in features.values():
        assert row["armature_modifier_bound"] is True
        assert row["material_slots_preserved"] > 0
        assert row["unassigned_vertex_count"] == 0
        bounds = row["world_bounds_m"]
        assert len(bounds["min"]) == len(bounds["max"]) == 3
        assert all(math.isfinite(v) and lo <= v
                   for lo, v in zip(bounds["min"], bounds["max"]))

    walk_path = next(Path(row["path"]) for row in rigged["artifacts"]
                     if row["path"].endswith(".walk_cycle.json"))
    walk = json.loads(walk_path.read_text(encoding="utf-8"))
    assert walk["loop"]["root_motion_matches_stride"] is True
    assert walk["contacts"]["foot_pad_selector_source"] == "serialized generated joint graph"
    for selector in ("bone:pes_foot_l", "bone:pes_foot_r"):
        row = walk["contacts"]["nominal_stance_samples"]["targets"][selector]
        assert row["samples"] > 0
        assert row["tolerance_m"] == pytest.approx(0.02)
        slip = walk["contacts"]["touchdown_anchored_stance_excursion"][selector]
        assert slip["max_excursion_m"] is not None
        assert math.isfinite(slip["max_excursion_m"])
