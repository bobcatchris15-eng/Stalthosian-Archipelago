"""Export bridge integrity tests; no Blender executable is needed."""
import struct
from pathlib import Path

import pytest

from stalthos_toolkit.blender import runner
from stalthos_toolkit.core.contract import Context
from stalthos_toolkit.core.workspace import Workspace
from stalthos_toolkit.core.errors import ExternalToolFailed, UsageError

@pytest.fixture
def paths(tmp_path):
    source = tmp_path / "source.blend"
    source.write_bytes(b"master preserved")
    target = tmp_path / "runtime/output.glb"
    target.parent.mkdir()
    return source, target


def test_success_replaces_target_and_registers_hashed_artifacts(paths, monkeypatch):
    source, target = paths
    target.write_bytes(b"previous asset")
    ctx = Context(workspace=Workspace(target.parent), options={"timeout":37}, seed=9)
    ctx.new_status("generate_lods")
    payload = struct.pack("<4sII", b"glTF", 2, 20) + b"12345678"
    def run(context, **kwargs):
        assert context is ctx
        assert kwargs["params"]["source_blend"] == str(source)
        assert kwargs["params"]["seed"] == 9
        assert Path(kwargs["script"]).is_file()
        assert "export_animations=True" in Path(kwargs["script"]).read_text()
        Path(kwargs["params"]["output_glb"]).write_bytes(payload)
        log = target.parent / "export.log"
        log.write_text("recorded")
        return runner.BlenderRun(0, result={"ok":True}, log_path=log)
    monkeypatch.setattr(runner, "run_blender", run)
    result = runner.export_gltf(source, target, seed=9, ctx=ctx)
    assert result.ok
    assert target.read_bytes() == payload
    assert source.read_bytes() == b"master preserved"
    assert {a.role for a in ctx.status.artifacts} == {"intermediate", "log"}
    assert all(a.sha256 for a in ctx.status.artifacts)
    assert list(target.parent.glob(".output_*.glb")) == []

@pytest.mark.parametrize("blob", [b"", b"not a glb", struct.pack("<4sII", b"glTF", 2, 99)])
def test_invalid_output_preserves_existing_target(paths, monkeypatch, blob):
    source, target = paths
    target.write_bytes(b"accepted prior asset")
    def run(ctx, **kwargs):
        Path(kwargs["params"]["output_glb"]).write_bytes(blob)
        return runner.BlenderRun(0, result={"ok":True})
    monkeypatch.setattr(runner, "run_blender", run)
    with pytest.raises(runner.BlenderError):
        runner.export_gltf(source, target, seed=0)
    assert target.read_bytes() == b"accepted prior asset"
    assert list(target.parent.glob(".output_*.glb")) == []


def test_tool_failure_preserves_target_and_exit_code(paths, monkeypatch):
    source, target = paths
    target.write_bytes(b"accepted prior asset")
    def fail(ctx, **kwargs):
        raise ExternalToolFailed("blender", 1, stderr="export failure")
    monkeypatch.setattr(runner, "run_blender", fail)
    with pytest.raises(ExternalToolFailed) as caught:
        runner.export_gltf(source, target, seed=0)
    assert caught.value.exit_code == 6
    assert target.read_bytes() == b"accepted prior asset"
    assert list(target.parent.glob(".output_*.glb")) == []


def test_missing_source_is_usage_error_without_running_blender(paths, monkeypatch):
    source, target = paths
    source.unlink()
    monkeypatch.setattr(runner, "run_blender", lambda *args,**kwargs: pytest.fail("must not run"))
    with pytest.raises(UsageError):
        runner.export_gltf(source, target, seed=0)
