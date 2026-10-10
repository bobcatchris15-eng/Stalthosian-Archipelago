"""Workspace layout and tool discovery.

Everything the toolkit writes lives under one workspace root so a run can be
inspected, hashed, diffed or thrown away. Nothing is written next to the source
tree outside the root.

    <root>/
      species/            # checked-in species + creature definitions (YAML)
      schemas/            # checked-in JSON schemas (mirror of contracts/)
      var/
        sources/          # editable Blender masters (.blend)  -- LFS territory
        intermediate/     # bakes, LOD intermediates, scratch renders
        runtime/          # published glTF/GLB + compressed textures
        preview/          # turntables, growth stages, review sheets
        reports/          # validation, benchmark, receipts, status JSON
"""

from __future__ import annotations

import os
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

from .errors import MissingDependency


def default_root() -> Path:
    """`STEALTHOS_HOME` if set, else `<repo>/var`, else the user's home."""
    env = os.environ.get("STEALTHOS_HOME")
    if env:
        return Path(env).expanduser().resolve()
    here = Path(__file__).resolve()
    for parent in here.parents:
        if (parent / "pyproject.toml").exists():
            return parent / "var"
    return Path.home() / ".stalthos"


@dataclass(frozen=True)
class Workspace:
    root: Path

    @classmethod
    def open(cls, root: str | Path | None = None) -> "Workspace":
        ws = cls(Path(root).expanduser().resolve() if root else default_root())
        ws.ensure()
        return ws

    # --- directories -------------------------------------------------------
    @property
    def sources(self) -> Path:
        return self.root / "sources"

    @property
    def intermediate(self) -> Path:
        return self.root / "intermediate"

    @property
    def runtime(self) -> Path:
        return self.root / "runtime"

    @property
    def preview(self) -> Path:
        return self.root / "preview"

    @property
    def reports(self) -> Path:
        return self.root / "reports"

    def ensure(self) -> "Workspace":
        for path in (self.sources, self.intermediate, self.runtime, self.preview, self.reports):
            path.mkdir(parents=True, exist_ok=True)
        return self

    def path_for(self, relative: str | Path) -> Path:
        """Resolve a workspace-relative path, refusing escapes."""
        target = (self.root / relative).resolve()
        if not str(target).startswith(str(self.root)):
            raise ValueError(f"path escapes workspace root: {relative}")
        return target

    def sub(self, kind: str, *parts: str) -> Path:
        """Create and return a subdirectory of one of the five roots."""
        base = getattr(self, kind)
        target = base.joinpath(*parts)
        target.mkdir(parents=True, exist_ok=True)
        return target


# --- external tool discovery ------------------------------------------------
# Every external tool is optional at import time. A missing tool must produce
# MissingDependency (exit 4) with an actionable hint, never an ImportError.

_BLENDER_ENV = "STEALTHOS_BLENDER"
_BLENDER_CANDIDATES = (
    "blender",
    r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe",
    r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe",
    r"C:\Program Files\Blender Foundation\Blender 4.3\blender.exe",
    "/usr/bin/blender",
    "/Applications/Blender.app/Contents/MacOS/Blender",
)


def find_blender(explicit: str | None = None) -> str:
    """Locate the Blender executable or raise MissingDependency."""
    for candidate in (explicit, os.environ.get(_BLENDER_ENV)):
        if candidate:
            if Path(candidate).exists():
                return str(candidate)
            found = shutil.which(candidate)
            if found:
                return found
            raise MissingDependency(
                f"blender (configured path {candidate!r} does not exist)",
                hint="set STEALTHOS_BLENDER or pass --blender <path>",
            )
    for candidate in _BLENDER_CANDIDATES:
        found = shutil.which(candidate) if os.sep not in candidate else (candidate if Path(candidate).exists() else None)
        if found:
            return found
    raise MissingDependency(
        "blender",
        hint="install Blender 4.3+ and set STEALTHOS_BLENDER, or pass --blender <path>",
    )


def find_tool(name: str, *, required_for: str) -> str:
    """Locate a required CLI tool on PATH or raise MissingDependency."""
    found = shutil.which(name)
    if not found:
        raise MissingDependency(name, hint=f"required for {required_for}; install it and ensure it is on PATH")
    return found


def python_executable() -> str:
    return sys.executable
