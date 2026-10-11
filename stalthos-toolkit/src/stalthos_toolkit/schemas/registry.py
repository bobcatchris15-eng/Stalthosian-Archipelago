"""Definition loading and validation.

`stalthos_toolkit.core.contract.Context.load_definition` funnels through
`loader.load_definition`, so putting validation here means every command that
loads a definition gets schema checking for free (docs/toolkit-architecture.md).

Validation is dependency-free: `schemas/validator.py` implements the draft
2020-12 subset used by `contracts/*.schema.json` and reports RFC 6901 pointers.
"""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path
from typing import Any

from ..core.errors import UsageError
from .validator import SchemaError, SchemaValidator, ValidationIssue, UnsupportedSchema, validate

#: directory holding the checked-in JSON Schemas, overridable with
#: STEALTHOS_CONTRACTS for tests and off-tree contracts.
CONTRACTS_ENV = "STEALTHOS_CONTRACTS"

CONTRACT_FILES = {
    "species-definition": "species-definition.schema.json",
    "creature-definition": "creature-definition.schema.json",
    "asset-manifest": "asset-manifest.schema.json",
    "status-envelope": "status-envelope.schema.json",
    "research-record": "research-record.schema.json",
    "terrain-definition": "terrain-definition.schema.json",
}


def contracts_dir() -> Path:
    import os

    override = os.environ.get(CONTRACTS_ENV)
    if override:
        return Path(override).expanduser().resolve()
    # src/stalthos_toolkit/schemas/ -> package -> src -> stalthos-toolkit/
    here = Path(__file__).resolve()
    for parent in here.parents:
        if (parent / "pyproject.toml").exists():
            return parent / "contracts"
    return here.parents[2] / "contracts"


@lru_cache(maxsize=None)
def load_contract(name: str) -> dict[str, Any]:
    """Load and cache one contract schema by short name."""
    if name not in CONTRACT_FILES:
        raise SchemaError(
            f"unknown contract {name!r}; known: {sorted(CONTRACT_FILES)}"
        )
    path = contracts_dir() / CONTRACT_FILES[name]
    if not path.exists():
        raise SchemaError(
            f"contract schema not found: {path}",
        )
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise SchemaError(f"contract schema {path.name} is not valid JSON: {exc}") from exc


def contract_names() -> list[str]:
    return sorted(CONTRACT_FILES)


def validator_for(name: str) -> SchemaValidator:
    return SchemaValidator(load_contract(name))


def schema_for_definition(definition: dict[str, Any]) -> str:
    """Pick the contract a definition document claims (or implies)."""
    declared = definition.get("schema")
    if isinstance(declared, str):
        if declared in CONTRACT_FILES:
            return declared
        raise UsageError(
            f"definition declares unknown schema {declared!r}",
            detail={"known": contract_names()},
            hint=f"use one of: {', '.join(contract_names())}",
        )
    if definition.get("kind") == "creature":
        return "creature-definition"
    return "species-definition"


def format_issues(issues: list[ValidationIssue], *, source: str | None = None, limit: int = 25) -> str:
    lines = []
    if source:
        lines.append(f"{source} does not satisfy its schema:")
    for issue in issues[:limit]:
        lines.append(f"  {issue.path}: {issue.message} [{issue.keyword}]")
    if len(issues) > limit:
        lines.append(f"  ... and {len(issues) - limit} more")
    return "\n".join(lines)


def validate_against(name: str, instance: Any) -> list[ValidationIssue]:
    """Validate `instance` against contract `name`; [] means valid."""
    return validate(instance, load_contract(name))


def require_valid(name: str, instance: Any, *, source: str | None = None, hint: str | None = None) -> None:
    """Raise UsageError (exit 2) listing every schema violation."""
    try:
        issues = validate_against(name, instance)
    except (SchemaError, UnsupportedSchema) as exc:
        raise UsageError(f"contract schema unusable: {exc}") from exc
    if issues:
        raise UsageError(
            format_issues(issues, source=source),
            detail={"contract": name, "error_count": len(issues),
                    "errors": [issue.to_dict() for issue in issues[:25]]},
            hint=hint or f"see contracts/{CONTRACT_FILES[name]}",
        )


__all__ = [
    "CONTRACT_FILES",
    "contract_names",
    "contracts_dir",
    "format_issues",
    "load_contract",
    "require_valid",
    "schema_for_definition",
    "validate_against",
    "validator_for",
    "ValidationIssue",
    "SchemaError",
    "SchemaValidator",
    "UnsupportedSchema",
    "validate",
]