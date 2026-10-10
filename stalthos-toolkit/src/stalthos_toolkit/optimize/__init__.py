"""Stage 6 optimization: LOD chain, texture compression, glTF/GLB interchange.

All external tools (gltfpack/meshoptimizer, AMD Compressonator, a glTF validator)
are optional. A missing tool is reported, never silently substituted:

* LOD chain    -> `lods.py`      (gltfpack primary, pure-python fallback)
* textures     -> `textures.py`  (Compressonator, exit 4 when absent)
* interchange  -> `glb.py`       (standard glTF 2.0 only)
* decimation   -> `decimate.py`  (deterministic clustering fallback)
* discovery    -> `tools.py`
"""

from __future__ import annotations

from .decimate import DecimateResult, cluster_decimate, collision_proxy
from .glb import (
    GlbDocument,
    GlbError,
    GlbSummary,
    MeshArrays,
    check_rig_integrity,
    external_tool_versions,
    rig_summary,
    validate_structure,
    validate_with_tools,
)
from .lods import (
    CREATURE_POLICY,
    PLANT_POLICY,
    POLICIES,
    PROP_POLICY,
    ChainResult,
    LodPolicy,
    LodRecord,
    build_lod_chain,
    chain_metrics,
    policy_for,
)
from .textures import (
    TEXTURE_POLICY,
    CompressionOutcome,
    TextureSlotPolicy,
    bytes_before_after,
    compress_textures,
    select_format,
)
from .tools import ToolState, discover, discover_many, require

__all__ = [
    "CREATURE_POLICY",
    "ChainResult",
    "CompressionOutcome",
    "DecimateResult",
    "GlbDocument",
    "GlbError",
    "GlbSummary",
    "LodPolicy",
    "LodRecord",
    "MeshArrays",
    "PLANT_POLICY",
    "POLICIES",
    "PROP_POLICY",
    "TEXTURE_POLICY",
    "TextureSlotPolicy",
    "ToolState",
    "build_lod_chain",
    "bytes_before_after",
    "chain_metrics",
    "check_rig_integrity",
    "cluster_decimate",
    "collision_proxy",
    "compress_textures",
    "discover",
    "discover_many",
    "external_tool_versions",
    "policy_for",
    "require",
    "rig_summary",
    "select_format",
    "validate_structure",
    "validate_with_tools",
]