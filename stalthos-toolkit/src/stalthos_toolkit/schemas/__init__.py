"""Definition loading, contracts and the dependency-free schema validator."""

from __future__ import annotations

from .loader import load_definition, load_definition_file_list, validate_definition
from .registry import (
    CONTRACT_FILES,
    ValidationIssue,
    contract_names,
    contracts_dir,
    format_issues,
    load_contract,
    require_valid,
    schema_for_definition,
    validate_against,
    validator_for,
)
from .validator import SchemaError, SchemaValidator, UnsupportedSchema, validate

__all__ = [
    "CONTRACT_FILES",
    "contract_names",
    "contracts_dir",
    "format_issues",
    "load_contract",
    "load_definition",
    "load_definition_file_list",
    "require_valid",
    "schema_for_definition",
    "validate_against",
    "validate_definition",
    "validator_for",
    "SchemaError",
    "SchemaValidator",
    "UnsupportedSchema",
    "ValidationIssue",
    "validate",
]