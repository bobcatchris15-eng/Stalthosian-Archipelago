"""The single JSON status envelope every command emits.

Contract (docs/toolkit-architecture.md):
  * one JSON object on stdout, always, even on failure
  * human-readable logs go to stderr or a log file, never stdout
  * `ok` mirrors the exit code
"""

from __future__ import annotations

import json
import sys
import time
from dataclasses import dataclass, field
from typing import Any

from .errors import EXIT_OK, ToolkitError


@dataclass
class Artifact:
    """A file the command produced, with its content hash."""

    path: str
    role: str = "output"          # source | intermediate | runtime | preview | report
    sha256: str | None = None
    bytes: int | None = None

    def to_dict(self) -> dict:
        out: dict[str, Any] = {"path": self.path, "role": self.role}
        if self.sha256:
            out["sha256"] = self.sha256
        if self.bytes is not None:
            out["bytes"] = self.bytes
        return out


@dataclass
class Status:
    command: str
    ok: bool = True
    exit_code: int = EXIT_OK
    seed: int | None = None
    asset_id: str | None = None
    started_at: float = field(default_factory=time.time)
    duration_s: float = 0.0
    artifacts: list[Artifact] = field(default_factory=list)
    metrics: dict[str, Any] = field(default_factory=dict)
    provenance: dict[str, Any] = field(default_factory=dict)
    warnings: list[str] = field(default_factory=list)
    errors: list[dict] = field(default_factory=list)
    logs: list[str] = field(default_factory=list)
    partial: bool = False          # partial exports exist but were NOT published

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    def log(self, message: str) -> None:
        self.logs.append(message)

    def add_artifact(self, artifact: Artifact) -> Artifact:
        self.artifacts.append(artifact)
        return artifact

    def to_dict(self) -> dict:
        return {
            "command": self.command,
            "ok": self.ok,
            "exit_code": self.exit_code,
            "seed": self.seed,
            "asset_id": self.asset_id,
            "started_at": round(self.started_at, 3),
            "duration_s": round(self.duration_s, 3),
            "partial": self.partial,
            "artifacts": [a.to_dict() for a in self.artifacts],
            "metrics": self.metrics,
            "provenance": self.provenance,
            "warnings": self.warnings,
            "errors": self.errors,
            "logs": self.logs,
        }

    def to_json(self, *, indent: int | None = 2) -> str:
        return json.dumps(self.to_dict(), indent=indent, sort_keys=False, default=str)


def emit(status: Status, stream=None) -> None:
    """Write the status envelope as the only thing on stdout."""
    stream = stream or sys.stdout
    stream.write(status.to_json() + "\n")
    stream.flush()


def status_from_exception(command: str, exc: BaseException, *, seed: int | None = None) -> Status:
    if isinstance(exc, ToolkitError):
        code, err = exc.exit_code, exc.to_dict()
    elif isinstance(exc, KeyboardInterrupt):
        code, err = 7, {"kind": "KeyboardInterrupt", "message": "interrupted by operator"}
    else:
        code, err = 1, {"kind": type(exc).__name__, "message": str(exc)}
    return Status(
        command=command,
        ok=False,
        exit_code=code,
        seed=seed,
        errors=[err],
    )
