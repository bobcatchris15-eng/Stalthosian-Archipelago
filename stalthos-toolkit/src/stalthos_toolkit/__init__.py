"""Stalthos Asset Toolkit -- agent-facing asset production pipeline.

Public surface for stage implementations:

    from stalthos_toolkit.core import Context, Status, Workspace, command
"""

from .core.contract import REGISTRY, CommandSpec, Context, Registry, command
from .core.errors import (
    EXIT_EXTERNAL,
    EXIT_FAILURE,
    EXIT_INTERRUPTED,
    EXIT_MISSING_DEPENDENCY,
    EXIT_NOT_IMPLEMENTED,
    EXIT_OK,
    EXIT_USAGE,
    EXIT_VALIDATION,
    ExternalToolFailed,
    MissingDependency,
    NotImplementedStage,
    ToolkitError,
    UsageError,
    ValidationFailed,
)
from .core.manifest import ArtifactRef, Manifest, Metric, Provenance, ValidationRecord
from .core.seeding import derive_seed, hash_config, normalize_seed, rng, sha256_file
from .core.status import Artifact, Status, emit, status_from_exception
from .core.workspace import Workspace, find_blender, find_tool

__version__ = "0.1.0"

__all__ = [
    "REGISTRY", "Registry", "CommandSpec", "Context", "command",
    "Status", "Artifact", "emit", "status_from_exception",
    "Manifest", "Provenance", "Metric", "ArtifactRef", "ValidationRecord",
    "Workspace", "find_blender", "find_tool",
    "derive_seed", "normalize_seed", "rng", "sha256_file", "hash_config",
    "ToolkitError", "UsageError", "ValidationFailed", "MissingDependency",
    "NotImplementedStage", "ExternalToolFailed",
    "EXIT_OK", "EXIT_FAILURE", "EXIT_USAGE", "EXIT_VALIDATION",
    "EXIT_MISSING_DEPENDENCY", "EXIT_NOT_IMPLEMENTED", "EXIT_EXTERNAL",
    "EXIT_INTERRUPTED",
    "__version__",
]
