"""Offline and integration tests for conifer terminal needle/scale spray generation."""

from __future__ import annotations

import importlib
import json
from pathlib import Path

import pytest
import yaml

from stalthos_toolkit.blender.params import plant_shape_params
from stalthos_toolkit.cli import main

MOD = importlib.import_module("stalthos_toolkit.commands.generate_plant")
DEFINITION = Path(__file__).resolve().parents[1] / "species/plants/splitcrown.yaml"


def run(capsys, tmp_path, stage=None, dry=True):
    args = ["generate_plant", "-d", str(DEFINITION), "-s", "42", "--workspace", str(tmp_path)]
    if stage:
        args += ["--stage", stage]
    if dry:
        args += ["--dry-run"]
    code = main(args)
    envelope = json.loads(capsys.readouterr().out)
    return code, envelope


@pytest.mark.parametrize(
    "stage",
    ["sapling", "young_adult", "mature"],
)
def test_stage_needle_params_in_plan(capsys, tmp_path, stage):
    """Authored anatomy.foliage units must be consumed into the stage plan."""
    code, s = run(capsys, tmp_path, stage)
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    assert "needle_length_m" in plant
    assert plant["needle_length_m"] == pytest.approx(0.035)
    assert "needle_width_m" in plant
    assert plant["needle_width_m"] == pytest.approx(0.009)
    assert plant.get("foliage_organ") == "scale_spray"
    assert plant.get("foliage_shape") == "needled"


def test_no_stage_needle_params_absent(capsys, tmp_path):
    """No-stage payload must leave needle parameters unset, preserving canonical tree geometry."""
    code, s = run(capsys, tmp_path)
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    assert "needle_length_m" not in plant
    assert "needle_width_m" not in plant
    assert "foliage_organ" not in plant
    definition = yaml.safe_load(DEFINITION.read_text())
    assert plant == plant_shape_params(definition, "tree", stage=None)


def test_non_tree_family_ignores_needle_anatomy(capsys, tmp_path, monkeypatch):
    """Needle spray parameters are strictly scoped to the tree family."""
    definition = {
        "id": "plant.shrub.test",
        "kind": "plant",
        "generator": "layered_shrub",
        "height_m": 2.0,
        "anatomy": {
            "foliage": {
                "organ": "scale_spray",
                "overall_shape": "needled",
                "length": {"value": 3.5, "unit": "cm"},
                "width": {"value": 0.9, "unit": "cm"},
            }
        },
        "growth_stages": [
            {
                "id": "sapling",
                "height": {"value": 1.0, "unit": "m"},
                "crown_spread": {"value": 0.8, "unit": "m"},
            }
        ],
    }
    monkeypatch.setattr(MOD.Context, "load_definition", lambda ctx, path: definition)
    code, s = run(capsys, tmp_path, "sapling")
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    assert "needle_length_m" not in plant


def test_tree_without_needle_anatomy_leaves_needle_params_unset(capsys, tmp_path, monkeypatch):
    """Broadleaf trees lacking needle anatomy do not receive needle parameters."""
    definition = {
        "id": "plant.broadleaf.tree",
        "kind": "plant",
        "generator": "branching_tree",
        "height_m": 15.0,
        "anatomy": {
            "foliage": {
                "organ": "broadleaf",
                "overall_shape": "ovate",
                "length": {"value": 18.0, "unit": "cm"},
                "width": {"value": 12.0, "unit": "cm"},
            }
        },
        "growth_stages": {
            "sapling": {
                "height_m": 3.0,
                "crown_spread_m": 1.8,
            }
        },
    }
    monkeypatch.setattr(MOD.Context, "load_definition", lambda ctx, path: definition)
    code, s = run(capsys, tmp_path, "sapling")
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    assert "needle_length_m" not in plant
    assert "foliage_organ" not in plant


def test_script_payload_receives_needle_spray_params(capsys, tmp_path, monkeypatch):
    """Mocked Blender run script receives needle params in payload."""
    seen = {}
    monkeypatch.setattr(MOD.PL, "require_blender", lambda ctx: None)

    def script(ctx, status, **kwargs):
        seen.update(kwargs["params"])
        out = Path(kwargs["params"]["outputs"]["blend"])
        out.write_bytes(b"unit fixture")
        return None, {"artifacts": [{"path": str(out), "role": "source", "kind": "blend"}], "metrics": {}}

    monkeypatch.setattr(MOD.PL, "run_script", script)
    code, s = run(capsys, tmp_path, "sapling", dry=False)
    assert code == 0 and s["ok"]
    assert seen["plant"]["needle_length_m"] == pytest.approx(0.035)
    assert seen["plant"]["needle_width_m"] == pytest.approx(0.009)
    assert seen["plant"]["foliage_organ"] == "scale_spray"


def test_spray_envelope_and_branch_stability(capsys, tmp_path):
    """Verify spray parameters maintain leaf reach and crown spread targets."""
    code, s = run(capsys, tmp_path, "young_adult")
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    assert plant["crown_spread_m"] == pytest.approx(6.5)
    # The macro foliage reach used for branching headroom remains proportional
    h_scale = 14.0 / 26.0
    assert plant["leaf_length_m"] == pytest.approx(round(2.34 * h_scale, 4), rel=1e-3)


def test_needle_dimensions_bounded_and_positive(capsys, tmp_path):
    """Needle scale parameters must be positive and bounded (sub-decimetre, not macro billboards)."""
    code, s = run(capsys, tmp_path, "sapling")
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    needle_l = plant["needle_length_m"]
    needle_w = plant["needle_width_m"]
    assert 0.01 <= needle_l <= 0.10, "needle length must be true scale-conifer size"
    assert 0.002 <= needle_w <= 0.03, "needle width must be true needle/scale size"
    # Aspect ratio matches authored ~3.9
    aspect = needle_l / needle_w
    assert aspect == pytest.approx(3.8888, rel=0.05)

