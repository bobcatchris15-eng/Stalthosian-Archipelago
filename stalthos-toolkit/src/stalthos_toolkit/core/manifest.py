"""Asset manifest: the provenance record required by docs/asset-toolkit.md.

One manifest per produced asset, written to `<workspace>/reports/manifests/`.
It records identity, seed, tool versions, input hashes, source/baked
relationships, ecological affordances, measured metrics, validation outcome and
license provenance -- so a runtime asset can always be traced back to the exact
inputs that produced it.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

from .seeding import canonical_json

SCHEMA_VERSION = "1.0"


@dataclass
class Metric:
    name: str
    value: float
    unit: str = ""
    measured: bool = True     # False => estimated/inferred, never silently guessed

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class ArtifactRef:
    path: str
    role: str = "output"      # source | intermediate | runtime | preview | report
    sha256: str | None = None
    bytes: int | None = None
    importer_settings: dict[str, Any] = field(default_factory=dict)


@dataclass
class Provenance:
    generator: str = "stalthos-toolkit"
    toolkit_version: str = "0.1.0"
    tool_versions: dict[str, str] = field(default_factory=dict)
    command: str = ""
    command_args: dict[str, Any] = field(default_factory=dict)
    seed: int = 0
    input_hashes: dict[str, str] = field(default_factory=dict)   # definition file -> sha256
    parent_asset_id: str | None = None                            # source -> derived lineage
    licenses: list[dict[str, str]] = field(default_factory=list)  # refs, libraries, tools
    references: list[dict[str, str]] = field(default_factory=list)  # research/photo refs + usage rights

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class ValidationRecord:
    outcome: str = "not_run"    # pass | fail | not_run
    checks: list[dict[str, Any]] = field(default_factory=list)
    report_path: str | None = None

    def to_dict(self) -> dict:
        return asdict(self)


@dataclass
class Manifest:
    asset_id: str
    kind: str                          # plant | creature | prop | texture_set | scene
    species_id: str | None = None
    morphology_id: str | None = None
    stage: str | None = None           # growth stage / life stage
    variant: str | None = None
    schema_version: str = SCHEMA_VERSION
    created_at: str = ""               # ISO-8601 UTC, filled by the writer
    provenance: Provenance = field(default_factory=Provenance)
    artifacts: list[ArtifactRef] = field(default_factory=list)
    metrics: list[Metric] = field(default_factory=list)
    affordances: dict[str, Any] = field(default_factory=dict)   # edible foliage targets, contact bounds, ...
    validation: ValidationRecord = field(default_factory=ValidationRecord)
    previews: list[str] = field(default_factory=list)
    notes: str = ""

    # --- io ----------------------------------------------------------------
    def to_dict(self) -> dict:
        return {
            "schema_version": self.schema_version,
            "asset_id": self.asset_id,
            "kind": self.kind,
            "species_id": self.species_id,
            "morphology_id": self.morphology_id,
            "stage": self.stage,
            "variant": self.variant,
            "created_at": self.created_at,
            "provenance": self.provenance.to_dict(),
            "artifacts": [a if isinstance(a, dict) else asdict(a) for a in self.artifacts],
            "metrics": [m.to_dict() if isinstance(m, Metric) else m for m in self.metrics],
            "affordances": self.affordances,
            "validation": self.validation.to_dict(),
            "previews": self.previews,
            "notes": self.notes,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "Manifest":
        prov = Provenance(**data.get("provenance", {}))
        validation = ValidationRecord(**data.get("validation", {}))
        artifacts = [ArtifactRef(**a) for a in data.get("artifacts", [])]
        metrics = [Metric(**m) for m in data.get("metrics", [])]
        known = {f for f in cls.__dataclass_fields__}
        payload = {k: v for k, v in data.items() if k in known and k not in
                   {"provenance", "validation", "artifacts", "metrics", "schema_version"}}
        return cls(
            provenance=prov,
            validation=validation,
            artifacts=artifacts,
            metrics=metrics,
            schema_version=data.get("schema_version", SCHEMA_VERSION),
            **payload,
        )

    def save(self, path: str | Path) -> Path:
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.to_dict(), indent=2, default=str), encoding="utf-8")
        return path

    @classmethod
    def load(cls, path: str | Path) -> "Manifest":
        return cls.from_dict(json.loads(Path(path).read_text(encoding="utf-8")))

    def content_hash(self) -> str:
        """Hash of the semantic content, ignoring volatile timestamps."""
        payload = self.to_dict()
        payload.pop("created_at", None)
        payload["provenance"].pop("tool_versions", None)
        return __import__("hashlib").sha256(canonical_json(payload).encode("utf-8")).hexdigest()

    def add_metric(self, name: str, value: float, unit: str = "", *, measured: bool = True) -> Metric:
        metric = Metric(name=name, value=value, unit=unit, measured=measured)
        self.metrics.append(metric)
        return metric

    def find_artifact(self, role: str) -> ArtifactRef | None:
        for artifact in self.artifacts:
            if artifact.role == role:
                return artifact
        return None
