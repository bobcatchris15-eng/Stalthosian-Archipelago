"""Focused checks for project-local tool setup and doctor discovery."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from stalthos_toolkit import cli


TOOLKIT_ROOT = Path(__file__).resolve().parents[1]


def _powershell() -> str | None:
    configured = os.environ.get("STALTHOS_TEST_POWERSHELL")
    if configured:
        return configured
    return shutil.which("pwsh") or shutil.which("powershell")


@pytest.mark.skipif(_powershell() is None, reason="PowerShell is required for helper-script tests")
def test_tool_setup_scripts_parse_as_powershell():
    powershell = _powershell()
    assert powershell is not None
    scripts = [
        TOOLKIT_ROOT / "scripts" / "Prepare-Tools.ps1",
        TOOLKIT_ROOT / "scripts" / "Use-Tools.ps1",
        TOOLKIT_ROOT / "scripts" / "stalthos.ps1",
    ]
    for script in scripts:
        quoted_path = str(script).replace("'", "''")
        command = (
            "$tokens = $null; $errors = $null; "
            f"[System.Management.Automation.Language.Parser]::ParseFile('{quoted_path}', "
            "[ref]$tokens, [ref]$errors) | Out-Null; "
            "if ($errors.Count -gt 0) { $errors | Out-String | Write-Error }"
        )
        proc = subprocess.run(
            [powershell, "-NoProfile", "-Command", command],
            cwd=TOOLKIT_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        assert proc.returncode == 0, f"PowerShell parse failed for {script}: {proc.stderr}"


def test_doctor_lists_registry_tools_and_blender_version(tmp_path, monkeypatch, capsys):
    from stalthos_toolkit.core import workspace as workspace_module
    from stalthos_toolkit.optimize import tools as toolbox

    requested: list[str] = []

    def discover(name: str, *, with_version: bool = False):
        requested.append(name)
        return toolbox.ToolState(name=name, available=False, reason="test fixture")

    monkeypatch.setattr(toolbox, "discover", discover)
    monkeypatch.setattr(toolbox, "probe_version", lambda path: "5.2.1")
    monkeypatch.setattr(workspace_module, "find_blender", lambda: "C:/tools/blender.exe")

    code = cli.main(["--workspace", str(tmp_path / "workspace"), "doctor"])
    output = capsys.readouterr().out
    status = json.loads(output)

    assert code == 0
    assert status["ok"] is True
    tools = status["metrics"]["tools"]
    assert "gltf_validator" in requested
    assert "gltf_transform" in requested
    assert tools["gltf_validator"]["available"] is False
    assert tools["gltf_transform"]["available"] is False
    assert tools["blender"]["version"] == "5.2.1"


@pytest.mark.skipif(_powershell() is None, reason="PowerShell is required for helper-script tests")
def test_prepare_tools_rejects_unpinned_archive_without_touching_install(tmp_path):
    powershell = _powershell()
    assert powershell is not None
    source_root = tmp_path / "source"
    archive_dir = source_root / "downloads"
    archive_dir.mkdir(parents=True)
    (archive_dir / "gltfpack-windows-1.3.zip").write_bytes(b"not the pinned release")

    toolkit_root = tmp_path / "stalthos-toolkit"
    toolkit_root.mkdir()
    install_root = tmp_path / "installed-tools"
    existing = install_root / "gltfpack-1.3"
    existing.mkdir(parents=True)
    sentinel = existing / "keep-me.txt"
    sentinel.write_text("old install stays", encoding="utf-8")

    script = TOOLKIT_ROOT / "scripts" / "Prepare-Tools.ps1"
    proc = subprocess.run(
        [
            powershell,
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(script),
            "-Tool",
            "gltfpack",
            "-ToolkitRoot",
            str(toolkit_root),
            "-SourceToolsRoot",
            str(source_root),
            "-InstallRoot",
            str(install_root),
        ],
        cwd=TOOLKIT_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert proc.returncode != 0
    assert sentinel.read_text(encoding="utf-8") == "old install stays"
    assert not list(install_root.glob(".stage-*"))


@pytest.mark.skipif(_powershell() is None, reason="PowerShell is required for helper-script tests")
def test_use_tools_rejects_bad_provenance_before_changing_process_environment(tmp_path):
    powershell = _powershell()
    assert powershell is not None
    toolkit_root = tmp_path / "stalthos-toolkit"
    toolkit_root.mkdir()
    install_root = tmp_path / "installed-tools"
    tool_folder = install_root / "gltfpack-1.3"
    tool_folder.mkdir(parents=True)
    (tool_folder / "gltfpack.exe").write_bytes(b"untrusted executable")
    (tool_folder / "stalthos-tool-provenance.json").write_text("{}", encoding="utf-8")

    script = TOOLKIT_ROOT / "scripts" / "Use-Tools.ps1"
    quote = lambda value: str(value).replace("'", "''")
    command = (
        f"try {{ . '{quote(script)}' -Tool gltfpack -ToolkitRoot '{quote(toolkit_root)}' "
        f"-InstallRoot '{quote(install_root)}' }} catch {{ Write-Output 'verification-failed' }}; "
        "Write-Output ('RESULT=' + $env:STEALTHOS_GLTFPACK)"
    )
    environment = os.environ.copy()
    environment["STEALTHOS_GLTFPACK"] = "keep-existing-value"
    proc = subprocess.run(
        [powershell, "-NoProfile", "-Command", command],
        cwd=TOOLKIT_ROOT,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )

    assert proc.returncode == 0
    assert "verification-failed" in proc.stdout
    assert "RESULT=keep-existing-value" in proc.stdout
