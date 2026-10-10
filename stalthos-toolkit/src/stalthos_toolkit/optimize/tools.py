"""External tool discovery for the optimization stage.

Every tool here is **optional**. A missing tool is data, not an exception:
callers decide whether absence means "exit 4" or "degrade with a warning".
Nothing in this module imports Blender, and nothing prints to stdout.

Tool names are probed in a fixed order so a machine with both `gltfpack` and a
`meshoptimizer` build behaves deterministically.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

#: Environment variable prefix for pointing the toolkit at a tool explicitly,
#: e.g. ``STEALTHOS_GLTFPACK=C:\\tools\\gltfpack.exe``.
ENV_PREFIX = "STEALTHOS_"

#: Candidate executable names per logical tool, most preferred first.
TOOL_CANDIDATES: dict[str, tuple[str, ...]] = {
    "gltfpack": ("gltfpack", "gltfpack.exe"),
    "meshoptimizer": ("meshoptimizer",),
    "compressonator": ("compressonatorcli", "compressonator", "compressonatorcli.exe", "compressonator.exe"),
    "gltf_validator": ("gltf_validator", "gltf-validator"),
    "gltf_transform": ("gltf-transform", "gltf_transform"),
    "presentmon": ("PresentMon", "presentmon"),
    "renderdoc": ("renderdoccmd", "renderdoccmd.exe"),
}

#: Human install hints used when a MissingDependency is raised.
INSTALL_HINTS: dict[str, str] = {
    "gltfpack": "install gltfpack/meshoptimizer (https://github.com/zeux/meshoptimizer) and put it on PATH",
    "meshoptimizer": "install meshoptimizer (https://github.com/zeux/meshoptimizer) and put it on PATH",
    "compressonator": "install AMD Compressonator "
                      "(https://gpuopen.com/compressonator/) and put compressonatorcli on PATH",
    "gltf_validator": "install the Khronos glTF-Validator (npm i -g gltf-validator) and put it on PATH",
    "gltf_transform": "install gltf-Transform (npm i -g @gltf-transform/cli) and put it on PATH",
    "presentmon": "install PresentMon (https://github.com/gameforprofiler/PresentMon) to measure frame time",
    "renderdoc": "install RenderDoc to capture a frame breakdown",
}


@dataclass(frozen=True)
class ToolState:
    """Availability of one logical external tool."""

    name: str
    available: bool
    path: str | None = None
    version: str | None = None      # None => version unknown (not run)
    reason: str = ""

    def to_dict(self) -> dict:
        out: dict = {"name": self.name, "available": self.available}
        if self.path:
            out["path"] = self.path
        if self.version:
            out["version"] = self.version
        if self.reason:
            out["reason"] = self.reason
        return out

    def version_or_unknown(self) -> str:
        return self.version or "unknown"


def _env_override(name: str) -> str | None:
    value = os.environ.get(f"{ENV_PREFIX}{name.upper()}")
    if not value:
        return None
    if Path(value).exists():
        return str(value)
    found = shutil.which(value)
    return found


def locate(name: str) -> str | None:
    """Return an executable path for a logical tool, or None when absent."""
    override = _env_override(name)
    if override:
        return override
    for candidate in TOOL_CANDIDATES.get(name, (name,)):
        found = shutil.which(candidate)
        if found:
            return found
    return None


_VERSION_RE = re.compile(r"(\d+(?:\.\d+){1,3}(?:[-+][0-9A-Za-z.]+)?)")


def probe_version(path: str | None, *, timeout: float = 20.0) -> str | None:
    """Ask a tool for its version. Returns None if it will not say."""
    if not path:
        return None
    for args in (("--version",), ("-version",), ("version",)):
        try:
            proc = subprocess.run(  # noqa: S603 - fixed argv, no shell
                [path, *args],
                capture_output=True, text=True, timeout=timeout, check=False,
            )
        except (OSError, subprocess.SubprocessError):
            return None
        blob = (proc.stdout or "") + "\n" + (proc.stderr or "")
        match = _VERSION_RE.search(blob)
        if match:
            return match.group(1)
    return None


def discover(name: str, *, with_version: bool = False) -> ToolState:
    """Probe one logical tool without raising."""
    path = locate(name)
    if not path:
        return ToolState(name=name, available=False, reason="not found on PATH")
    version = probe_version(path) if with_version else None
    return ToolState(name=name, available=True, path=path, version=version)


def discover_many(names: Iterable[str], *, with_version: bool = False) -> dict[str, ToolState]:
    return {name: discover(name, with_version=with_version) for name in names}


def first_available(names: Sequence[str]) -> ToolState | None:
    """First available tool among logical names, in order."""
    for name in names:
        state = discover(name)
        if state.available:
            return state
    return None


def require(name: str) -> ToolState:
    """Probe a tool or raise `MissingDependency` (exit 4) with an install hint."""
    from ..core.errors import MissingDependency

    state = discover(name)
    if not state.available:
        raise MissingDependency(name, hint=INSTALL_HINTS.get(name))
    return state


def run_tool(path: str, args: Sequence[str], *, timeout: float | None = None,
             cwd: str | Path | None = None) -> subprocess.CompletedProcess:
    """Run an external tool with a fixed argv (never a shell string)."""
    return subprocess.run(  # noqa: S603 - argv list, shell=False
        [path, *args],
        capture_output=True, text=True, timeout=timeout, cwd=cwd, check=False,
    )


__all__ = [
    "INSTALL_HINTS",
    "TOOL_CANDIDATES",
    "ToolState",
    "discover",
    "discover_many",
    "first_available",
    "locate",
    "probe_version",
    "require",
    "run_tool",
]