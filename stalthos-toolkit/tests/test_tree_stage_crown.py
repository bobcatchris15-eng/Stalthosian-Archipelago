"""Offline unit tests for tree growth stage crown spread and foliage scaling."""

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
    "stage,expected_spread",
    [
        ("sapling", 2.4),
        ("young_adult", 6.5),
        ("mature", 12.0),
    ],
)
def test_stage_crown_spread_in_plan(capsys, tmp_path, stage, expected_spread):
    """Authored crown_spread must be consumed as absolute metres for explicit tree stage."""
    code, s = run(capsys, tmp_path, stage)
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    assert "crown_spread_m" in plant
    assert plant["crown_spread_m"] == pytest.approx(expected_spread)


def test_no_stage_crown_spread_absent(capsys, tmp_path):
    """No-stage payload must leave crown_spread_m unset, preserving default geometry."""
    code, s = run(capsys, tmp_path)
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    assert "crown_spread_m" not in plant
    definition = yaml.safe_load(DEFINITION.read_text())
    assert plant == plant_shape_params(definition, "tree", stage=None)


def test_non_tree_family_stage_crown_spread_ignored(capsys, tmp_path, monkeypatch):
    """Crown spread consumption is scoped to tree family only."""
    definition = {
        "id": "plant.shrub.test",
        "kind": "plant",
        "generator": "layered_shrub",
        "height_m": 2.0,
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
    assert "crown_spread_m" not in plant


@pytest.mark.parametrize(
    "stage,expected_height",
    [
        ("sapling", 4.2),
        ("young_adult", 14.0),
    ],
)
def test_tree_stage_foliage_scaled_proportionally(capsys, tmp_path, stage, expected_height):
    """Mature-derived fallback leaf dimensions must scale with stage height scale."""
    code, s = run(capsys, tmp_path, stage)
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    # 26m mature has leaf_length 2.34m (26 * 0.09) and leaf_width 0.78m (26 * 0.03)
    # Stage height scale is stage_height / 26.0
    h_scale = expected_height / 26.0
    assert plant["leaf_length_m"] == pytest.approx(round(2.34 * h_scale, 4), rel=1e-3)
    assert plant["leaf_width_m"] == pytest.approx(round(0.78 * h_scale, 4), rel=1e-3)


def test_explicit_stage_leaf_dimensions_not_scaled(capsys, tmp_path, monkeypatch):
    """When stage provides explicit leaf_length_m, it remains absolute and unscaled."""
    definition = {
        "id": "plant.synthetic.tree",
        "kind": "plant",
        "generator": "branching_tree",
        "height_m": 20.0,
        "growth_stages": {
            "sapling": {
                "height_m": 4.0,
                "crown_spread_m": 2.0,
                "leaf_length_m": 0.15,
                "leaf_width_m": 0.05,
            }
        },
    }
    monkeypatch.setattr(MOD.Context, "load_definition", lambda ctx, path: definition)
    code, s = run(capsys, tmp_path, "sapling")
    assert code == 0 and s["ok"]
    plant = s["metrics"]["plan"]["plant"]
    assert plant["crown_spread_m"] == pytest.approx(2.0)
    assert plant["leaf_length_m"] == pytest.approx(0.15)
    assert plant["leaf_width_m"] == pytest.approx(0.05)


def test_script_payload_receives_crown_spread(capsys, tmp_path, monkeypatch):
    """Mocked Blender run script receives crown_spread_m in params payload."""
    seen = {}
    monkeypatch.setattr(MOD.PL, "require_blender", lambda ctx: None)

    def script(ctx, status, **kwargs):
        seen.update(kwargs["params"])
        out = Path(kwargs["params"]["outputs"]["blend"])
        out.write_bytes(b"unit fixture")
        return None, {"artifacts": [{"path": str(out), "role": "source", "kind": "blend"}], "metrics": {}}

    monkeypatch.setattr(MOD.PL, "run_script", script)
    code, s = run(capsys, tmp_path, "young_adult", dry=False)
    assert code == 0 and s["ok"]
    assert seen["plant"]["crown_spread_m"] == pytest.approx(6.5)

def test_stage_explicit_width_kept_independently(capsys, tmp_path, monkeypatch):
    definition = {
        "id": "plant.synthetic.tree", "kind": "plant", "generator": "branching_tree",
        "height_m": 20.0,
        "growth_stages": {"sapling": {"height_m": 4.0, "crown_spread_m": 2.0, "leaf_width_m": 0.05}},
    }
    monkeypatch.setattr(MOD.Context, "load_definition", lambda ctx, path: definition)
    code, envelope = run(capsys, tmp_path, "sapling")
    assert code == 0 and envelope["ok"]
    plant = envelope["metrics"]["plan"]["plant"]
    assert plant["leaf_width_m"] == pytest.approx(0.05)
    assert plant["leaf_length_m"] == pytest.approx(0.36)


def test_stage_fallback_leaves_follow_final_height_override(capsys, tmp_path, monkeypatch):
    definition = yaml.safe_load(DEFINITION.read_text())
    definition["height_m"] = 10.0
    monkeypatch.setattr(MOD.Context, "load_definition", lambda ctx, path: definition)
    code, envelope = run(capsys, tmp_path, "young_adult")
    assert code == 0 and envelope["ok"]
    plant = envelope["metrics"]["plan"]["plant"]
    assert plant["height_m"] == pytest.approx(14.0)
    assert plant["leaf_length_m"] == pytest.approx(1.26)
    assert plant["leaf_width_m"] == pytest.approx(0.42)
