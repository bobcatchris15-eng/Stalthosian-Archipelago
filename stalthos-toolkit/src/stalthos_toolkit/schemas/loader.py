"""Definition loading (species / creature YAML or JSON) + schema validation.

Kept dependency-light on purpose: JSON always works, YAML requires PyYAML and
degrades with an actionable error (exit 2) rather than an ImportError.

Schema validation runs here, not in the callers: `core.contract.Context
.load_definition` is the single funnel every command uses, so validating on load
means no stage can forget it. A violation is a *usage* failure (exit 2,
"malformed definition"), not an acceptance failure (exit 3) -- acceptance is what
validate/ is for.

`validate=False` exists for tools that inspect a broken document on purpose
(the validator's own tests, `research_species --draft`). Callers that want
strictness back should assert on `validate_definition()` explicitly.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from ..core.errors import UsageError
from .registry import (
    ValidationIssue,
    require_valid,
    schema_for_definition,
    validate_against,
)

__all__ = [
    "load_definition",
    "validate_definition",
    "load_definition_file_list",
]


def load_definition(path: str | Path, *, validate: bool = True) -> dict[str, Any]:
    """Load a definition file into a plain dict, schema-checked by default."""
    path = Path(path)
    text = path.read_text(encoding="utf-8")
    suffix = path.suffix.lower()

    if suffix in {".json"}:
        return _checked(_as_dict(json.loads(text), path), path, validate)

    if suffix in {".yaml", ".yml"}:
        try:
            import yaml
        except ImportError as exc:  # pragma: no cover - environment dependent
            raise UsageError(
                "PyYAML is required to read YAML definitions",
                hint="pip install PyYAML, or convert the definition to JSON",
            ) from exc
        return _checked(_as_dict(yaml.safe_load(text), path), path, validate)

    if suffix in {".toml"}:
        import tomllib

        return _checked(_as_dict(tomllib.loads(text), path), path, validate)

    # Unknown suffix: try JSON, then YAML.
    try:
        return _checked(_as_dict(json.loads(text), path), path, validate)
    except json.JSONDecodeError:
        pass
    try:
        import yaml

        return _checked(_as_dict(yaml.safe_load(text), path), path, validate)
    except ImportError as exc:
        raise UsageError(f"cannot parse {path.name}: unknown format and PyYAML unavailable") from exc


def _checked(data: dict[str, Any], path: Path, validate: bool) -> dict[str, Any]:
    if validate:
        require_valid(schema_for_definition(data), data, source=path.name)
    return data


def _as_dict(data: Any, path: Path) -> dict[str, Any]:
    if data is None:
        raise UsageError(f"definition file is empty: {path}")
    if not isinstance(data, dict):
        raise UsageError(f"definition file must contain a mapping at the top level: {path}")
    return data


def load_definition_file_list(root: str | Path, pattern: str = "**/*.y*ml") -> list[Path]:
    return sorted(Path(root).glob(pattern))


def validate_definition(definition: dict[str, Any], *, source: str | None = None) -> list[ValidationIssue]:
    """Validate a loaded definition against its contract, returning every issue.

    Non-raising counterpart of `require_valid`; useful for reporting.
    """
    return validate_against(schema_for_definition(definition), definition)
