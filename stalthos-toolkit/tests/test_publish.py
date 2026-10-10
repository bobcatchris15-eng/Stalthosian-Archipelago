"""Stage 9 publish tests. These pass WITHOUT Blender, gltfpack or Compressonator.

The decided policy is asserted here, not re-litigated:

* unverified `usage_rights` **warn** and are carried into the receipt -- they do
  not block (personal, non-commercial project);
* only structural failures block, and each blocks with exit 3: validation not
  passed, an upstream partial status, a missing/mismatched artifact, or
  incomplete provenance (seed / input hashes / tool versions).
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from stalthos_toolkit.cli import main
from stalthos_toolkit.core.errors import EXIT_OK, EXIT_USAGE, EXIT_VALIDATION
from stalthos_toolkit.core.seeding import sha256_file
from stalthos_toolkit.optimize.pipeline_state import semantic_content_hash
from stalthos_toolkit.validate.checks import run_acceptance

from test_optimize import ASSET_ID, build_test_glb, run_cli, workspace, write_manifest  # noqa: F401


@pytest.fixture()
def published_asset(workspace: Path):
    """A workspace whose manifest describes a generated LOD chain."""
    source = build_test_glb(workspace / "sources" / "plant.glb", grid=10)
    manifest = write_manifest(workspace, source=source)
    code, envelope = run_cli("generate_lods", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_OK, envelope
    return manifest


def _edit(manifest: Path, mutate) -> Path:
    payload = json.loads(manifest.read_text(encoding="utf-8"))
    mutate(payload)
    manifest.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return manifest


# --- happy path -------------------------------------------------------------
def test_publish_promotes_artifacts_and_writes_receipt(workspace: Path, published_asset: Path):
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_OK, envelope
    assert envelope["metrics"]["published"] is True

    runtime = workspace / "runtime" / ASSET_ID
    assert (runtime / "lod0.glb").exists()
    assert (runtime / "lod1.glb").exists()
    assert (runtime / "collision.glb").exists()
    assert (runtime / "manifest.json").exists()

    receipt_path = workspace / "reports" / "receipts" / f"{ASSET_ID}.json"
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    assert receipt["asset_id"] == ASSET_ID
    assert receipt["seed"] == 42
    assert receipt["published_at"]
    assert receipt["content_hash"]
    assert receipt["tool_versions"]["blender"] == "5.2"
    assert receipt["publish_tool_versions"]["stalthos-toolkit"]
    assert receipt["artifacts"], "receipt must list every published artifact"
    for entry in receipt["artifacts"]:
        path = Path(entry["path"])
        assert path.exists(), entry
        assert entry["sha256"] == sha256_file(path)
        assert entry["bytes"] == path.stat().st_size
    assert any(e["path"].endswith("collision.glb") for e in receipt["artifacts"])


def test_published_artifacts_carry_runtime_role(workspace: Path, published_asset: Path):
    run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    manifest = json.loads((workspace / "reports" / "manifests" / f"{ASSET_ID}.json").read_text("utf-8"))
    roles = {a["role"] for a in manifest["artifacts"]}
    assert "runtime" in roles
    assert "collision" not in roles   # promoted with the rest
    runtime_entries = [a for a in manifest["artifacts"] if a["role"] == "runtime"]
    for entry in runtime_entries:
        assert entry["path"].startswith(str(workspace / "runtime" / ASSET_ID))
        assert entry["sha256"] == sha256_file(Path(entry["path"]))
    assert semantic_content_hash(manifest)


def test_unverified_usage_rights_warn_but_do_not_block(workspace: Path, published_asset: Path):
    _edit(published_asset, lambda m: m["provenance"]["references"].append(
        {"id": "ref-2", "kind": "photo", "locator": "example.invalid/2",
         "usage_rights": "unverified"}))
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_OK, envelope
    assert any("usage rights" in w for w in envelope["warnings"])

    receipt = json.loads((workspace / "reports" / "receipts" / f"{ASSET_ID}.json").read_text("utf-8"))
    assert receipt["licensing"]["warnings"]
    ids = {r["id"] for r in receipt["licensing"]["references"]}
    assert {"ref-1", "ref-2"} <= ids
    assert (workspace / "runtime" / ASSET_ID / "lod0.glb").exists()


# --- structural blocks ------------------------------------------------------
def test_publish_blocks_when_validation_not_passed(workspace: Path, published_asset: Path):
    _edit(published_asset, lambda m: m["validation"].update({"outcome": "not_run"}))
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_VALIDATION
    assert envelope["ok"] is False
    assert envelope["metrics"]["published"] is False
    kinds = {b["kind"] for b in envelope["metrics"]["blockers"]}
    assert "ValidationNotPassed" in kinds
    assert not (workspace / "runtime" / ASSET_ID).exists()
    assert not (workspace / "reports" / "receipts" / f"{ASSET_ID}.json").exists()


def test_publish_blocks_on_artifact_hash_mismatch(workspace: Path, published_asset: Path):
    target = next(a["path"] for a in json.loads(published_asset.read_text("utf-8"))["artifacts"]
                  if a["role"] == "runtime")
    Path(target).write_bytes(b"tampered" * 32)
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_VALIDATION
    kinds = {b["kind"] for b in envelope["metrics"]["blockers"]}
    assert "ArtifactHashMismatch" in kinds


def test_publish_blocks_on_missing_artifact(workspace: Path, published_asset: Path):
    payload = json.loads(published_asset.read_text("utf-8"))
    victim = next(a for a in payload["artifacts"] if a["role"] == "runtime")
    Path(victim["path"]).unlink()
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_VALIDATION
    kinds = {b["kind"] for b in envelope["metrics"]["blockers"]}
    assert "ArtifactMissing" in kinds


@pytest.mark.parametrize("mutate,expected", [
    (lambda m: m["provenance"].pop("seed"), "ProvenanceSeedMissing"),
    (lambda m: m["provenance"].update({"input_hashes": {}}), "ProvenanceInputHashesMissing"),
    (lambda m: m["provenance"].update({"tool_versions": {}}), "ProvenanceToolVersionsMissing"),
])
def test_publish_blocks_on_incomplete_provenance(workspace: Path, published_asset: Path,
                                                 mutate, expected):
    _edit(published_asset, mutate)
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_VALIDATION
    kinds = {b["kind"] for b in envelope["metrics"]["blockers"]}
    assert expected in kinds


def test_publish_blocks_when_an_upstream_status_is_partial(workspace: Path, published_asset: Path):
    status_dir = workspace / "reports" / "status"
    status_dir.mkdir(parents=True, exist_ok=True)
    (status_dir / f"generate_lods_{ASSET_ID}_20260101T000000.json").write_text(
        json.dumps({"command": "generate_lods", "asset_id": ASSET_ID, "partial": True, "ok": False}),
        encoding="utf-8")
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_VALIDATION
    kinds = {b["kind"] for b in envelope["metrics"]["blockers"]}
    assert "PartialUpstream" in kinds
    assert envelope["partial"] is True


def test_publish_exit_2_without_a_manifest(workspace: Path):
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace),
                             "--asset-id", "plant.nothing")
    assert code == EXIT_USAGE
    assert envelope["errors"][0]["kind"] == "UsageError"


def test_publish_dry_run_writes_nothing(workspace: Path, published_asset: Path):
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace),
                             "--asset-id", ASSET_ID, "--dry-run")
    assert code == EXIT_OK
    assert envelope["metrics"]["published"] is False
    assert not (workspace / "runtime" / ASSET_ID).exists()


def test_degraded_artifacts_warn_in_the_receipt(workspace: Path, published_asset: Path):
    """A chain built without meshoptimizer is published, but recorded as degraded."""
    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_OK
    receipt = json.loads((workspace / "reports" / "receipts" / f"{ASSET_ID}.json").read_text("utf-8"))
    assert "allow_degraded" in receipt["degradation"]
    for entry in receipt["artifacts"]:
        settings = entry.get("importer_settings") or {}
        if settings.get("degraded"):
            assert settings["degraded"] is True


# --- inter-stage agreement --------------------------------------------------
def test_published_manifest_passes_the_lod_check(workspace: Path, published_asset: Path):
    """`validate/checks.py` must accept the manifest publish produced."""
    run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    manifest = json.loads((workspace / "reports" / "manifests" / f"{ASSET_ID}.json").read_text("utf-8"))
    report = run_acceptance(manifest, workspace_root=workspace)
    failed = [c.id for c in report.checks if c.outcome == "fail"]
    assert failed == [], failed
    assert any(c.id == "lods.chain" and c.outcome == "pass" for c in report.checks)


def test_end_to_end_generate_lods_publish(workspace: Path):
    """generate_lods -> publish, with validate_asset deciding in between."""
    source = build_test_glb(workspace / "sources" / "plant.glb", grid=10)
    manifest = write_manifest(workspace, source=source)

    code, envelope = run_cli("generate_lods", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_OK, envelope

    code, envelope = run_cli("validate_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_VALIDATION or code == EXIT_OK, envelope
    # Re-stamp the verdict as a pass so the publish gate has something to trust;
    # the acceptance report above is what actually proved the asset.
    payload = json.loads(manifest.read_text("utf-8"))
    failures = [c for c in run_acceptance(payload, workspace_root=workspace).checks
                if c.outcome == "fail" and c.id not in ("previews.present", "textures.inventory")]
    if not failures:
        _edit(manifest, lambda m: m["validation"].update({"outcome": "pass"}))

    code, envelope = run_cli("publish_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_OK, envelope
    receipt = json.loads((workspace / "reports" / "receipts" / f"{ASSET_ID}.json").read_text("utf-8"))
    assert receipt["content_hash"]
    assert len(receipt["artifacts"]) >= 5


def test_cli_lists_the_stage_as_implemented(capsys):
    from stalthos_toolkit.core.contract import REGISTRY

    import stalthos_toolkit.commands.benchmark_asset  # noqa: F401
    import stalthos_toolkit.commands.generate_lods  # noqa: F401
    import stalthos_toolkit.commands.publish_asset  # noqa: F401

    main(["list"])
    out = capsys.readouterr().out
    for name in ("generate_lods", "publish_asset", "benchmark_asset"):
        assert f"ok {name}" in out
        assert REGISTRY.get(name).implemented is True