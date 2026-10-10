"""Command registry: the declared CLI contract from docs/asset-toolkit.md.

A command is a small dataclass binding a name to an implementation and a
description. The registry is the single source of truth for `stalthos --list`
and for docs/toolkit-pipeline.md.

Implementations MUST:
  * accept a `Context` and return a `Status`
  * never print to stdout (the CLI owns the status envelope)
  * record every produced file as an artifact, hashed
  * be deterministic given (definition, seed, tool versions)
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

from .errors import UsageError
from .manifest import Manifest
from .seeding import derive_seed, hash_config, normalize_seed, sha256_file
from .status import Artifact, Status
from .workspace import Workspace


@dataclass
class Context:
    """Everything a command implementation is allowed to depend on."""

    workspace: Workspace
    seed: int = 0
    options: dict[str, Any] = field(default_factory=dict)
    status: Status | None = None
    # populated by helpers
    _definition: dict | None = None
    _definition_path: Path | None = None

    # --- definition loading -------------------------------------------------
    def definition_path(self, explicit: str | None) -> Path:
        """Resolve a YAML/JSON definition file (species or creature profile)."""
        if explicit:
            path = Path(explicit).expanduser()
        else:
            raise UsageError("a definition file is required (--definition <path>)")
        if not path.exists():
            raise UsageError(f"definition file not found: {path}")
        return path.resolve()

    def load_definition(self, explicit: str | None) -> dict:
        from ..schemas.loader import load_definition  # local import: optional dep

        path = self.definition_path(explicit)
        data = load_definition(path)
        self._definition = data
        self._definition_path = path
        return data

    @property
    def definition(self) -> dict:
        if self._definition is None:
            raise UsageError("no definition loaded; call load_definition() first")
        return self._definition

    @property
    def definition_sha256(self) -> str | None:
        return sha256_file(self._definition_path) if self._definition_path else None

    # --- determinism --------------------------------------------------------
    def seed_for(self, *labels: Any) -> int:
        return derive_seed(self.seed, *labels)

    def rng(self, *labels: Any):
        from .seeding import rng as _rng

        return _rng(self.seed, *labels)

    def input_hashes(self) -> dict[str, str]:
        hashes: dict[str, str] = {}
        if self._definition_path:
            hashes[str(self._definition_path)] = self.definition_sha256 or ""
            hashes[f"definition:{self._definition_path.name}#content"] = hash_config(self.definition)
        return hashes

    # --- output helpers -----------------------------------------------------
    def artifact(self, path: str | Path, role: str = "output") -> Artifact:
        path = Path(path)
        try:
            return Artifact(path=str(path), role=role, sha256=sha256_file(path),
                            bytes=path.stat().st_size)
        except OSError:
            return Artifact(path=str(path), role=role)

    def new_status(self, command: str) -> Status:
        status = Status(command=command, seed=self.seed)
        self.status = status
        return status

    def new_manifest(self, *, asset_id: str, kind: str, command: str) -> Manifest:
        from datetime import datetime, timezone

        definition = self._definition or {}
        manifest = Manifest(
            asset_id=asset_id,
            kind=kind,
            species_id=definition.get("id") or definition.get("species_id"),
            morphology_id=definition.get("morphology_id") or definition.get("generator"),
            stage=definition.get("stage"),
            variant=self.options.get("variant"),
            created_at=datetime.now(timezone.utc).isoformat(timespec="seconds"),
        )
        manifest.provenance.seed = self.seed
        manifest.provenance.command = command
        manifest.provenance.command_args = {k: v for k, v in self.options.items() if _serializable(v)}
        manifest.provenance.input_hashes = self.input_hashes()
        manifest.provenance.references = list(definition.get("references", []))
        manifest.provenance.licenses = list(definition.get("licenses", []))
        manifest.affordances = dict(definition.get("affordances", {}))
        return manifest


def _serializable(value: Any) -> bool:
    return isinstance(value, (str, int, float, bool, list, dict, type(None)))


Handler = Callable[[Context], Status]


@dataclass
class CommandSpec:
    name: str
    summary: str
    handler: Handler | None = None
    stage: str = ""                      # canonical pipeline stage number/name
    implemented: bool = False

    def invoke(self, ctx: Context) -> Status:
        if self.handler is None:
            from ..commands._stubs import not_implemented

            status = not_implemented(ctx, self.name, self.summary)
            return status
        return self.handler(ctx)


class Registry:
    def __init__(self) -> None:
        self._commands: dict[str, CommandSpec] = {}

    def register(self, spec: CommandSpec) -> CommandSpec:
        self._commands[spec.name] = spec
        return spec

    def add(self, name: str, summary: str, handler: Handler | None = None, *, stage: str = "") -> CommandSpec:
        return self.register(CommandSpec(name=name, summary=summary, handler=handler,
                                         stage=stage, implemented=handler is not None))

    def get(self, name: str) -> CommandSpec:
        if name not in self._commands:
            raise UsageError(f"unknown command: {name}", hint="run `stalthos list`")
        return self._commands[name]

    def names(self) -> list[str]:
        return list(self._commands)

    def all(self) -> list[CommandSpec]:
        return list(self._commands.values())

    def table(self) -> list[dict[str, Any]]:
        return [
            {"name": c.name, "stage": c.stage, "implemented": c.implemented, "summary": c.summary}
            for c in self._commands.values()
        ]


REGISTRY = Registry()


def command(name: str, summary: str, *, stage: str = "") -> Callable[[Handler], Handler]:
    """Decorator: register a function as a command implementation."""

    def decorate(fn: Handler) -> Handler:
        REGISTRY.add(name, summary, fn, stage=stage)
        return fn

    return decorate


def normalize_root_seed(seed: int | str | None) -> int:
    if seed is None:
        return 0
    return normalize_seed(seed)
