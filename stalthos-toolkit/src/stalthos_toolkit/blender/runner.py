"""Headless Blender execution layer.

Contract (docs/toolkit-architecture.md + planning docs/asset-toolkit.md):

    blender --background --factory-startup --python <script.py> -- --params <in.json>

* Python (this module) writes a **params JSON**: definition subset, derived
  seeds, absolute output paths, resolution, engine, action.
* The bpy script writes a **result JSON**: metrics, objects, triangle counts,
  materials, timings, blender version, warnings.
* bpy never prints the result to stdout. Stdout/stderr are captured and logged
  to `<workspace>/reports/logs/`.
* Every run gets its own temp working directory (CWD) so Blender cannot write
  into the source tree. Output paths are absolute and are created by Python
  before the run.

Exit-code mapping (frozen core):
    missing binary        -> MissingDependency  (4)
    timeout / interrupt   -> Interrupted        (7)
    non-zero blender exit -> ExternalToolFailed (6)
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Sequence

from ..core.contract import Context
from ..core.errors import (
    EXIT_EXTERNAL,
    EXIT_INTERRUPTED,
    EXIT_MISSING_DEPENDENCY,
    ExternalToolFailed,
    Interrupted,
    ToolkitError,
)
from ..core.workspace import find_blender

SCRIPTS_DIR = Path(__file__).resolve().parent / "scripts"

DEFAULT_TIMEOUT_S = 900.0

#: Environment forced on every Blender child process. Blender must not write
#: config, cache, autosave or crash dumps anywhere inside the source tree.
CHILD_ENV = {
    # --factory-startup already skips userpref; these keep caches in the temp dir.
    "BLENDER_USER_RESOURCES": "",
    "BLENDER_USER_CONFIG": "",
    "BLENDER_SYSTEM_DATAFILES": "",
    "TMPDIR": "",       # replaced per run
    "TEMP": "",         # replaced per run
    "TMP": "",          # replaced per run
    "PYTHONHASHSEED": "0",
    "PYTHONDONTWRITEBYTECODE": "1",
    "STALTHOS_HEADLESS": "1",
    "STALTHOS_TMP": "",      # replaced per run; Blender's scratch directory
}


class BlenderError(ToolkitError):
    """Base for headless-Blender problems that are not the child's fault."""

    exit_code = EXIT_EXTERNAL


@dataclass
class BlenderRun:
    """The outcome of one headless Blender invocation."""

    returncode: int
    stdout: str = ""
    stderr: str = ""
    duration_s: float = 0.0
    command: list[str] = field(default_factory=list)
    result: dict[str, Any] | None = None
    log_path: Path | None = None
    work_dir: Path | None = None
    timeout_s: float | None = None

    @property
    def ok(self) -> bool:
        return self.returncode == 0 and self.result is not None

    def result_or_raise(self) -> dict[str, Any]:
        if self.result is None:
            raise BlenderError(
                "blender produced no result JSON",
                detail={"returncode": self.returncode, "stderr_tail": self.stderr[-2000:]},
                hint="inspect the run log under <workspace>/reports/logs/",
            )
        return self.result

    def to_dict(self) -> dict[str, Any]:
        return {
            "returncode": self.returncode,
            "duration_s": round(self.duration_s, 3),
            "command": self.command,
            "timeout_s": self.timeout_s,
            "log_path": str(self.log_path) if self.log_path else None,
            "has_result": self.result is not None,
        }


def resolve_blender(ctx: Context | None = None, explicit: str | None = None) -> str:
    """Locate Blender, honouring --blender, then $STEALTHOS_BLENDER, then defaults.

    Raises core MissingDependency (exit 4) when absent -- never an ImportError.
    """
    if explicit is None and ctx is not None:
        explicit = ctx.options.get("blender")
    return find_blender(explicit)


def script_path(name: str) -> Path:
    """Resolve a bpy script by module name (``plants`` -> ``scripts/plants.py``)."""
    if name.endswith(".py"):
        candidate = SCRIPTS_DIR / name
    else:
        candidate = SCRIPTS_DIR / f"{name}.py"
    if not candidate.is_file():
        raise BlenderError(f"unknown bpy script: {name}", detail={"expected": str(candidate)})
    return candidate


def resolve_timeout(ctx: Context | None = None, override: float | None = None) -> float:
    if override is not None:
        return float(override)
    if ctx is not None:
        raw = ctx.options.get("timeout")
        if raw not in (None, "", 0):
            return float(raw)
    return DEFAULT_TIMEOUT_S


def build_argv(blender_exe: str, script: Path, params_path: Path) -> list[str]:
    """The frozen invocation shape. Kept in one place so tests can assert it."""
    return [
        blender_exe,
        "--background",
        "--factory-startup",
        # Blender 5.2 only accepts the single-dash spelling of this one flag
        "-noaudio",
        "--disable-autoexec",
        "--python-exit-code",
        "1",
        "--python",
        str(script),
        "--",
        "--params",
        str(params_path),
    ]


def run_blender(
    ctx: Context,
    *,
    script: str | Path,
    params: dict[str, Any],
    timeout: float | None = None,
    blender_exe: str | None = None,
    log_name: str | None = None,
    result_name: str = "result.json",
) -> BlenderRun:
    """Run one bpy script headless and return its parsed result.

    Parameters
    ----------
    script   module name (``plants``) or path inside ``blender/scripts``.
    params   the params payload; written verbatim to ``params.json``.
    timeout  seconds; falls back to ``ctx.options["timeout"]`` then 900.
    keep_workdir  leave the per-run temp directory in place for debugging.
            Successful runs are cleaned up by default; failing runs always keep
            their workdir so a crash can be inspected.

    Raises MissingDependency(4), Interrupted(7), ExternalToolFailed(6).
    """
    exe = resolve_blender(ctx, blender_exe)
    script_file = script_path(script) if not isinstance(script, Path) else Path(script)
    if not script_file.is_file():
        raise BlenderError(f"bpy script not found: {script_file}")
    limit = resolve_timeout(ctx, timeout)
    keep = bool(ctx.options.get("keep_workdir")) if ctx is not None else False

    work_dir = Path(tempfile.mkdtemp(prefix="stalthos_blender_"))
    params_path = work_dir / "params.json"
    result_path = work_dir / result_name
    params_path.write_text(json.dumps(params, indent=2, default=str, sort_keys=False), encoding="utf-8")

    argv = build_argv(exe, script_file, params_path)
    env = dict(os.environ)
    env.update(CHILD_ENV)
    for key in ("TMPDIR", "TEMP", "TMP"):
        env[key] = str(work_dir)
    env["BLENDER_USER_RESOURCES"] = str(work_dir / "blender_res")
    env["BLENDER_USER_CONFIG"] = str(work_dir / "blender_cfg")
    env["BLENDER_SYSTEM_DATAFILES"] = str(work_dir / "blender_data")
    env["STALTHOS_TMP"] = str(work_dir)

    started = time.time()
    timed_out = False
    stdout = stderr = ""
    returncode = -1
    try:
        completed = subprocess.run(  # noqa: S603 - argv is fully constructed here
            argv,
            cwd=str(work_dir),
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=limit,
            stdin=subprocess.DEVNULL,
        )
        returncode = completed.returncode
        stdout = completed.stdout or ""
        stderr = completed.stderr or ""
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        stdout = exc.stdout.decode("utf-8", "replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        stderr = exc.stderr.decode("utf-8", "replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
    except FileNotFoundError as exc:
        raise MissingDependency(
            f"blender (configured path {exc.filename!r} does not exist)",
            hint="set STEALTHOS_BLENDER or pass --blender <path>",
        ) from exc
    finally:
        duration = time.time() - started

    result: dict[str, Any] | None = None
    if result_path.is_file():
        try:
            result = json.loads(result_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            raise BlenderError(
                "blender result JSON is malformed",
                detail={"error": str(exc), "path": str(result_path)},
            ) from exc

    log_path = _write_log(ctx, log_name or f"{ctx.status.command if ctx.status else 'blender'}_{script_file.stem}",
                          argv, stdout, stderr, result, returncode, duration, timed_out)

    run = BlenderRun(
        returncode=returncode,
        stdout=stdout,
        stderr=stderr,
        duration_s=duration,
        command=argv,
        result=result,
        log_path=log_path,
        work_dir=work_dir,
        timeout_s=limit,
    )

    if timed_out:
        raise Interrupted(
            f"blender exceeded the {limit:g}s timeout",
            detail={"tool": "blender", "script": script_file.name, "timeout_s": limit,
                    "log": str(log_path)},
            hint="raise --timeout or reduce the definition's complexity budget",
        )
    if returncode != 0:
        raise ExternalToolFailed("blender", returncode, stderr=stderr or _tail(stdout))
    if result is None:
        raise BlenderError(
            "blender exited 0 but wrote no result JSON",
            detail={"log": str(log_path), "stderr_tail": (stderr or stdout)[-2000:]},
            hint="the bpy script must always write result.json, including on failure",
        )
    if not keep:
        # a stale temp dir must never fail an otherwise good run
        _discard_workdir(work_dir)
    return run


def _discard_workdir(work_dir: Path) -> None:
    """Remove a per-run temp directory (the run log keeps the transcript)."""
    shutil.rmtree(work_dir, ignore_errors=True)


def _tail(text: str, limit: int = 2000) -> str:
    return text[-limit:] if text else ""


def _write_log(
    ctx: Context,
    name: str,
    argv: Sequence[str],
    stdout: str,
    stderr: str,
    result: dict[str, Any] | None,
    returncode: int,
    duration: float,
    timed_out: bool,
) -> Path:
    """Persist the full transcript under the workspace (never the source tree)."""
    safe = "".join(ch if ch.isalnum() or ch in "-_." else "_" for ch in name)
    directory = ctx.workspace.reports / "logs"
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / f"{safe}.log"
    lines = [
        f"# stalthos headless blender run: {safe}",
        f"# argv: {json.dumps(list(argv))}",
        f"# returncode: {returncode}  timed_out: {timed_out}  duration_s: {duration:.3f}",
        "",
        "----- stdout -----",
        stdout or "",
        "----- stderr -----",
        stderr or "",
        "----- result -----",
        json.dumps(result, indent=2, default=str) if result else "(no result.json)",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def blender_version(ctx: Context, *, blender_exe: str | None = None, timeout: float = 60.0) -> str:
    """Query `blender --version` cheaply. Returns 'unknown' on any failure."""
    exe = resolve_blender(ctx, blender_exe)
    try:
        out = subprocess.run([exe, "--version"], capture_output=True, text=True,
                             encoding="utf-8", errors="replace",
                             timeout=timeout, stdin=subprocess.DEVNULL)
    except (OSError, subprocess.SubprocessError):
        return "unknown"
    first = (out.stdout or out.stderr or "").splitlines()
    return first[0].strip() if first else "unknown"


def python_executable() -> str:
    return sys.executable


__all__ = [
    "BlenderError",
    "BlenderRun",
    "DEFAULT_TIMEOUT_S",
    "SCRIPTS_DIR",
    "blender_version",
    "build_argv",
    "resolve_blender",
    "resolve_timeout",
    "run_blender",
    "script_path",
]
