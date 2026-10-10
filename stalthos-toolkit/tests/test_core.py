"""Core module tests: seeding, manifest, status, workspace, errors, contract.

These tests run WITHOUT Blender. They exercise the core API contracts.
"""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

import pytest

from stalthos_toolkit.core.contract import (
    Context,
    Registry,
    CommandSpec,
    command,
    normalize_root_seed,
)
from stalthos_toolkit.core.errors import (
    EXIT_OK,
    EXIT_USAGE,
    EXIT_VALIDATION,
    EXIT_MISSING_DEPENDENCY,
    EXIT_NOT_IMPLEMENTED,
    EXIT_EXTERNAL,
    EXIT_INTERRUPTED,
    ToolkitError,
    UsageError,
    ValidationFailed,
    MissingDependency,
    NotImplementedStage,
    ExternalToolFailed,
    Interrupted,
)
from stalthos_toolkit.core.manifest import (
    Manifest,
    Metric,
    ArtifactRef,
    Provenance,
    ValidationRecord,
)
from stalthos_toolkit.core.seeding import (
    derive_seed,
    normalize_seed,
    rng,
    sha256_file,
    hash_config,
    canonical_json,
)
from stalthos_toolkit.core.status import (
    Status,
    Artifact,
    emit,
    status_from_exception,
)
from stalthos_toolkit.core.workspace import (
    Workspace,
    find_blender,
    find_tool,
)


class TestExitCodes:
    """Test exit code constants match the contract."""

    def test_exit_codes_match_contract(self):
        assert EXIT_OK == 0
        assert EXIT_USAGE == 2
        assert EXIT_VALIDATION == 3
        assert EXIT_MISSING_DEPENDENCY == 4
        assert EXIT_NOT_IMPLEMENTED == 5
        assert EXIT_EXTERNAL == 6
        assert EXIT_INTERRUPTED == 7


class TestErrorHierarchy:
    """Test exception hierarchy and exit codes."""

    def test_toolkit_error_base(self):
        err = ToolkitError("base error")
        assert err.exit_code == 1  # EXIT_FAILURE
        assert err.message == "base error"
        d = err.to_dict()
        assert d["kind"] == "ToolkitError"
        assert d["message"] == "base error"

    def test_usage_error_exit_2(self):
        err = UsageError("bad args")
        assert err.exit_code == EXIT_USAGE

    def test_validation_failed_exit_3(self):
        err = ValidationFailed("validation failed")
        assert err.exit_code == EXIT_VALIDATION

    def test_missing_dependency_exit_4(self):
        err = MissingDependency("blender")
        assert err.exit_code == EXIT_MISSING_DEPENDENCY
        assert err.tool == "blender"
        d = err.to_dict()
        assert d["kind"] == "MissingDependency"
        assert d["detail"]["tool"] == "blender"

    def test_not_implemented_stage_exit_5(self):
        err = NotImplementedStage("not done")
        assert err.exit_code == EXIT_NOT_IMPLEMENTED

    def test_external_tool_failed_exit_6(self):
        err = ExternalToolFailed("blender", 1, stderr="crash")
        assert err.exit_code == EXIT_EXTERNAL
        assert err.tool == "blender"
        assert err.returncode == 1
        d = err.to_dict()
        assert d["detail"]["tool"] == "blender"
        assert d["detail"]["returncode"] == 1

    def test_interrupted_exit_7(self):
        err = Interrupted("interrupted")
        assert err.exit_code == EXIT_INTERRUPTED


class TestSeeding:
    """Test deterministic seeding functions."""

    def test_normalize_seed_int(self):
        assert normalize_seed(0) == 0
        assert normalize_seed(42) == 42
        assert normalize_seed(2**63) == 0  # masked to 63 bits
        assert normalize_seed(-1) == (2**63) - 1  # two's complement masked

    def test_normalize_seed_string(self):
        s1 = normalize_seed("hello")
        s2 = normalize_seed("hello")
        assert s1 == s2
        assert 0 <= s1 < 2**63
        assert normalize_seed("hello") != normalize_seed("world")

    def test_derive_seed_stable(self):
        root = 42
        labels = ("plant", "splitcrown", "branch", 3)
        s1 = derive_seed(root, *labels)
        s2 = derive_seed(root, *labels)
        assert s1 == s2
        assert 0 <= s1 < 2**63

    def test_derive_seed_different_labels_different(self):
        s1 = derive_seed(42, "a")
        s2 = derive_seed(42, "b")
        assert s1 != s2

    def test_derive_seed_string_root(self):
        s1 = derive_seed("stable-root", "label")
        s2 = derive_seed("stable-root", "label")
        assert s1 == s2

    def test_rng_reproducible(self):
        r1 = rng(42, "test")
        r2 = rng(42, "test")
        assert [r1.random() for _ in range(10)] == [r2.random() for _ in range(10)]

    def test_rng_different_labels_different(self):
        r1 = rng(42, "a")
        r2 = rng(42, "b")
        assert [r1.random() for _ in range(10)] != [r2.random() for _ in range(10)]

    def test_sha256_file(self):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"hello world")
            path = f.name
        try:
            h = sha256_file(path)
            assert len(h) == 64
            assert all(c in "0123456789abcdef" for c in h)
            # Known hash
            assert h == "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9"
        finally:
            Path(path).unlink()

    def test_canonical_json_stable(self):
        # Dict order shouldn't matter
        j1 = canonical_json({"b": 1, "a": 2})
        j2 = canonical_json({"a": 2, "b": 1})
        assert j1 == j2
        # Nested
        j3 = canonical_json({"outer": {"b": 1, "a": 2}})
        j4 = canonical_json({"outer": {"a": 2, "b": 1}})
        assert j3 == j4

    def test_hash_config_stable(self):
        config = {"species": "splitcrown", "params": {"tiers": 5, "angle": 45}}
        h1 = hash_config(config)
        h2 = hash_config(config)
        assert h1 == h2
        assert len(h1) == 64


class TestWorkspace:
    """Test workspace layout and tool discovery."""

    def test_default_root_from_env(self, monkeypatch):
        with tempfile.TemporaryDirectory() as tmp:
            monkeypatch.setenv("STEALTHOS_HOME", tmp)
            ws = Workspace.open()
            assert ws.root == Path(tmp).resolve()

    def test_default_root_from_repo(self):
        # Should find pyproject.toml in parents
        ws = Workspace.open()
        assert ws.root.name == "var"
        assert (ws.root.parent / "pyproject.toml").exists()

    def test_workspace_ensure_creates_five_dirs(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp).ensure()
            for name in ("sources", "intermediate", "runtime", "preview", "reports"):
                assert getattr(ws, name).exists()
                assert getattr(ws, name).is_dir()

    def test_path_for_refuses_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            with pytest.raises(ValueError, match="escapes workspace root"):
                ws.path_for("../escape")
            with pytest.raises(ValueError, match="escapes workspace root"):
                ws.path_for("/absolute")

    def test_path_for_resolves_relative(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            p = ws.path_for("sources/test.blend")
            assert p == ws.sources / "test.blend"

    def test_sub_creates_nested(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp).ensure()
            p = ws.sub("sources", "plants", "splitcrown")
            assert p == ws.sources / "plants" / "splitcrown"
            assert p.exists()


class TestManifest:
    """Test manifest serialization and content_hash."""

    def test_manifest_minimal(self):
        m = Manifest(asset_id="test", kind="plant")
        assert m.asset_id == "test"
        assert m.kind == "plant"
        assert m.schema_version == "1.0"

    def test_manifest_save_load(self):
        with tempfile.TemporaryDirectory() as tmp:
            m = Manifest(asset_id="plant.test", kind="plant", species_id="plant.test")
            m.provenance.seed = 42
            m.provenance.input_hashes = {"def.yaml": "abc123"}
            m.metrics.append(Metric(name="triangles", value=1000, measured=True))
            m.affordances = {"browse_min": 0.5}

            path = Path(tmp) / "manifest.json"
            m.save(path)

            loaded = Manifest.load(path)
            assert loaded.asset_id == m.asset_id
            assert loaded.provenance.seed == 42
            assert loaded.provenance.input_hashes == {"def.yaml": "abc123"}
            assert len(loaded.metrics) == 1
            assert loaded.metrics[0].name == "triangles"
            assert loaded.affordances == {"browse_min": 0.5}

    def test_content_hash_ignores_volatile(self):
        m1 = Manifest(asset_id="test", kind="plant")
        m1.provenance = Provenance(seed=42, tool_versions={"blender": "5.1"})
        m1.created_at = "2024-01-01"
        m2 = Manifest(asset_id="test", kind="plant")
        m2.provenance = Provenance(seed=42, tool_versions={"blender": "5.2"})
        m2.created_at = "2024-12-31"
        assert m1.content_hash() == m2.content_hash()

    def test_content_hash_changes_with_seed(self):
        m1 = Manifest(asset_id="test", kind="plant")
        m1.provenance = Provenance(seed=42)
        m2 = Manifest(asset_id="test", kind="plant")
        m2.provenance = Provenance(seed=43)
        assert m1.content_hash() != m2.content_hash()

    def test_add_metric(self):
        m = Manifest(asset_id="test", kind="plant")
        metric = m.add_metric("vertices", 5000, "count", measured=False)
        assert metric.name == "vertices"
        assert metric.measured is False
        assert len(m.metrics) == 1

    def test_find_artifact(self):
        m = Manifest(asset_id="test", kind="plant")
        m.artifacts = [
            ArtifactRef(path="a.glb", role="runtime"),
            ArtifactRef(path="b.glb", role="runtime"),
            ArtifactRef(path="c.png", role="preview"),
        ]
        runtime = m.find_artifact("runtime")
        assert runtime is not None
        assert runtime.path == "a.glb"  # First match
        preview = m.find_artifact("preview")
        assert preview.path == "c.png"
        assert m.find_artifact("missing") is None


class TestStatusEnvelope:
    """Test Status dataclass and emit."""

    def test_status_to_dict(self):
        s = Status(command="test_cmd", seed=42, asset_id="asset.1")
        s.artifacts.append(Artifact(path="out.glb", role="runtime", sha256="abc", bytes=100))
        s.metrics["triangles"] = 1000
        s.warnings.append("minor issue")
        s.errors.append({"kind": "TestError", "message": "failed"})
        s.logs.append("log line")

        d = s.to_dict()
        assert d["command"] == "test_cmd"
        assert d["seed"] == 42
        assert d["asset_id"] == "asset.1"
        assert len(d["artifacts"]) == 1
        assert d["artifacts"][0]["sha256"] == "abc"
        assert d["metrics"]["triangles"] == 1000
        assert d["warnings"] == ["minor issue"]
        assert d["errors"][0]["kind"] == "TestError"
        assert d["logs"] == ["log line"]

    def test_status_to_json(self):
        s = Status(command="test", ok=True, exit_code=0)
        json_str = s.to_json(indent=None)
        parsed = json.loads(json_str)
        assert parsed["command"] == "test"
        assert parsed["ok"] is True

    def test_emit_writes_to_stream(self):
        import io
        s = Status(command="test", ok=True)
        stream = io.StringIO()
        from stalthos_toolkit.core.status import emit
        emit(s, stream=stream)
        output = stream.getvalue()
        parsed = json.loads(output.strip())
        assert parsed["command"] == "test"

    def test_status_from_exception_toolkit_error(self):
        err = UsageError("bad usage", detail={"arg": "foo"}, hint="fix it")
        status = Status(command="cmd", ok=True)  # placeholder
        from stalthos_toolkit.core.status import status_from_exception
        status = status_from_exception("cmd", err, seed=123)
        assert status.ok is False
        assert status.exit_code == EXIT_USAGE
        assert status.seed == 123
        assert len(status.errors) == 1
        assert status.errors[0]["kind"] == "UsageError"
        assert status.errors[0]["detail"]["arg"] == "foo"

    def test_status_from_exception_keyboard_interrupt(self):
        from stalthos_toolkit.core.status import status_from_exception
        status = status_from_exception("cmd", KeyboardInterrupt(), seed=0)
        assert status.exit_code == 7
        assert status.errors[0]["kind"] == "KeyboardInterrupt"

    def test_status_from_exception_generic(self):
        from stalthos_toolkit.core.status import status_from_exception
        status = status_from_exception("cmd", ValueError("oops"), seed=0)
        assert status.exit_code == 1
        assert status.errors[0]["kind"] == "ValueError"


class TestContractRegistry:
    """Test command registry and decorator."""

    def test_registry_register_and_get(self):
        reg = Registry()
        def handler(ctx): return Status(command="test")
        spec = reg.add("test_cmd", "Test command", handler, stage="1 test")
        assert spec.name == "test_cmd"
        assert spec.implemented is True
        got = reg.get("test_cmd")
        assert got is spec

    def test_registry_unknown_raises_usage_error(self):
        reg = Registry()
        with pytest.raises(UsageError, match="unknown command"):
            reg.get("nonexistent")

    def test_registry_table(self):
        reg = Registry()
        reg.add("cmd1", "Command 1", None, stage="1")
        reg.add("cmd2", "Command 2", lambda ctx: Status(command="cmd2"), stage="2")
        table = reg.table()
        assert len(table) == 2
        assert table[0]["name"] == "cmd1"
        assert table[0]["implemented"] is False
        assert table[1]["name"] == "cmd2"
        assert table[1]["implemented"] is True

    def test_command_decorator(self):
        # The @command decorator registers to the global REGISTRY
        from stalthos_toolkit.core.contract import REGISTRY
        
        @command("decorated_test", "Decorated command", stage="3")
        def my_handler(ctx):
            return Status(command="decorated_test")
        spec = REGISTRY.get("decorated_test")
        assert spec.handler is my_handler
        assert spec.implemented is True

    def test_command_spec_invoke_unimplemented(self):
        spec = CommandSpec(name="unimpl", summary="Not done", handler=None, stage="1")
        ctx = Context(workspace=Workspace.open(tempfile.mkdtemp()), seed=0)
        status = spec.invoke(ctx)
        assert status.ok is False
        assert status.exit_code == 5
        assert any(e["kind"] == "NotImplementedStage" for e in status.errors)


class TestContext:
    """Test Context helpers."""

    def test_context_seed_for(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            ctx = Context(workspace=ws, seed=42)
            child = ctx.seed_for("plant", "splitcrown", 0)
            assert child == derive_seed(42, "plant", "splitcrown", 0)

    def test_context_rng(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            ctx = Context(workspace=ws, seed=42)
            r = ctx.rng("test")
            assert hasattr(r, "random")

    def test_context_input_hashes_empty(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            ctx = Context(workspace=ws, seed=0)
            hashes = ctx.input_hashes()
            assert hashes == {}

    def test_context_artifact_hashes_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            ctx = Context(workspace=ws, seed=0)
            test_file = Path(tmp) / "test.txt"
            test_file.write_text("content")
            artifact = ctx.artifact(test_file, role="output")
            assert artifact.path == str(test_file)
            assert artifact.role == "output"
            assert artifact.sha256 is not None
            assert artifact.bytes == len("content")

    def test_context_new_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            ctx = Context(workspace=ws, seed=99)
            status = ctx.new_status("my_command")
            assert status.command == "my_command"
            assert status.seed == 99

    def test_context_new_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            ctx = Context(workspace=ws, seed=42, options={"variant": "test"})
            ctx._definition = {"id": "plant.test", "stage": "mature", "affordances": {}, "references": [], "licenses": []}
            manifest = ctx.new_manifest(asset_id="plant.test.v1", kind="plant", command="generate_plant")
            assert manifest.asset_id == "plant.test.v1"
            assert manifest.kind == "plant"
            assert manifest.provenance.seed == 42
            assert manifest.provenance.command == "generate_plant"
            assert manifest.provenance.command_args == {"variant": "test"}
            assert manifest.species_id == "plant.test"
            assert manifest.stage == "mature"
            assert manifest.variant == "test"


class TestFindBlender:
    """Test Blender discovery (mocked)."""

    def test_find_blender_explicit_path(self, monkeypatch):
        with tempfile.TemporaryDirectory() as tmp:
            fake_blender = Path(tmp) / "blender.exe"
            fake_blender.write_text("fake")
            # Should find explicit path
            path = find_blender(str(fake_blender))
            assert path == str(fake_blender)

    def test_find_blender_missing_explicit_raises(self, monkeypatch):
        with pytest.raises(MissingDependency, match="blender"):
            find_blender("/nonexistent/blender")

    def test_find_tool_missing_raises(self):
        with pytest.raises(MissingDependency, match="nonexistent_tool_xyz"):
            find_tool("nonexistent_tool_xyz", required_for="test")


class TestNormalizeRootSeed:
    """Test the CLI's normalize_root_seed helper."""

    def test_none_returns_zero(self):
        assert normalize_root_seed(None) == 0

    def test_int_passes_through(self):
        assert normalize_root_seed(42) == 42

    def test_string_normalizes(self):
        assert normalize_root_seed("label") == normalize_seed("label")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])