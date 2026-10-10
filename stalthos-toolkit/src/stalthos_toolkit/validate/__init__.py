"""Acceptance checks for pipeline stage 8.

Blender-free by construction: geometry/LOD/texture facts are read from what other
stages already measured and recorded in the manifest.
"""

from __future__ import annotations

from .checks import (
    GEOMETRY_GATES,
    AcceptanceReport,
    Check,
    OUTCOME_FAIL,
    OUTCOME_PASS,
    OUTCOME_SKIP,
    OUTCOME_WARN,
    check_artifacts,
    check_determinism,
    check_geometry_sanity,
    check_identity,
    check_lod_chain,
    check_manifest_schema,
    check_previews,
    check_provenance,
    check_textures,
    run_acceptance,
    semantic_hash,
)
from .ktx import TextureFormatInfo, sniff_texture

__all__ = [
    "AcceptanceReport",
    "Check",
    "GEOMETRY_GATES",
    "OUTCOME_FAIL",
    "OUTCOME_PASS",
    "OUTCOME_SKIP",
    "OUTCOME_WARN",
    "TextureFormatInfo",
    "check_artifacts",
    "check_determinism",
    "check_geometry_sanity",
    "check_identity",
    "check_lod_chain",
    "check_manifest_schema",
    "check_previews",
    "check_provenance",
    "check_textures",
    "run_acceptance",
    "semantic_hash",
    "sniff_texture",
]