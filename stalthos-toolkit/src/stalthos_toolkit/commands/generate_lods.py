"""`generate_lods` -- canonical pipeline stage 6 (optimization).

Reads the source geometry recorded in an asset manifest and writes, under
`var/intermediate/<kind>/<asset_id>/`:

* `lod0.glb .. lodN.glb`  -- the chain, strictly decreasing triangle counts
* `collision.glb`         -- decimated shell (NOT a convex hull; recorded as such)
* `impostor.json`         -- an honest stub: no impostor bake is implemented

Then, if the manifest declares texture slots, compresses them with AMD
Compressonator (BC5 normals / BC7 colour / BC4 scalars) and updates the
manifest with LOD, memory and texture metrics.

Degradation policy (decided, see docs/toolkit-architecture.md section 6)
------------------------------------------------------------------------
* gltfpack/meshoptimizer absent -> the pure-python cluster decimator is used and
  the chain is flagged degraded: every affected metric is `measured=False` and
  a warning is emitted. No LOD is ever called "optimized" when no optimizer ran.
* Compressonator absent -> exit 4, unless `allow_degraded` is set, in which case
  the textures are copied through uncompressed, warned about, and recorded with
  `measured=False`.

Exit codes: 0 ok, 2 usage, 3 acceptance failure (non-monotonic chain), 4 missing
external tool, 6 external tool failure, 7 timeout.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

from .. import __version__
from ..core import Context, Status, command
from ..core.errors import (
    EXIT_OK,
    EXIT_VALIDATION,
    ExternalToolFailed,
    MissingDependency,
    UsageError,
    ValidationFailed,
)
from ..optimize import tools as toolbox
from ..optimize.lods import build_lod_chain, chain_metrics, policy_for
from ..optimize.pipeline_state import (
    BLEND_SUFFIXES,
    allow_degraded,
    artifact_entry,
    asset_id_from,
    find_source_asset,
    load_manifest,
    resolve_manifest_path,
    texture_slots,
    update_manifest,
)
from ..optimize.textures import bytes_before_after, compress_textures

KIND_DIRS = {"plant": "plants", "creature": "creatures", "prop": "props", "scene": "scenes",
             "fungus": "plants", "texture_set": "textures"}


@command("generate_lods", "Generate the LOD chain, collision proxy and streaming metrics",
         stage="6 optimization")
def generate_lods(ctx: Context) -> Status:
    status = ctx.new_status("generate_lods")
    if ctx.options.get("definition"):
        ctx.load_definition(ctx.options.get("definition"))

    manifest_path = resolve_manifest_path(ctx)
    manifest = load_manifest(manifest_path)
    asset_id = asset_id_from(ctx, manifest)
    kind = str(manifest.get("kind") or "prop")
    status.asset_id = asset_id

    source = find_source_asset(manifest, ctx.workspace.root)
    if source is None:
        raise UsageError(
            f"manifest {manifest_path.name} declares no source geometry artifact",
            hint="run the generation stage (stage 3) first, or register the source .blend/.glb "
                 "in the manifest with role='source'")

    degraded_ok, degraded_source = allow_degraded(ctx)
    slots = texture_slots(manifest, ctx.workspace.root)

    # Fail fast on a missing Compressonator: never write a half-optimized asset
    # and then discover the texture tool is absent.
    compressonator = toolbox.discover("compressonator")
    if slots and not compressonator.available and not degraded_ok:
        raise MissingDependency("compressonator", hint=toolbox.INSTALL_HINTS["compressonator"])

    policy = policy_for(kind, levels=ctx.options.get("levels"))
    out_dir = ctx.workspace.intermediate / KIND_DIRS.get(kind, "assets") / asset_id
    timeout = ctx.options.get("timeout")

    status.metrics["plan"] = {
        "asset_id": asset_id,
        "kind": kind,
        "policy": policy.name,
        "triangle_ratios": list(policy.ratios),
        "collision_max_triangles": policy.collision_triangles,
        "out_dir": str(out_dir),
        "source": source.get("path"),
        "texture_slots": [{"path": str(p), "kind": k} for p, k in slots],
        "simplifier": "gltfpack" if toolbox.discover("gltfpack").available else "python",
        "allow_degraded": degraded_ok,
        "allow_degraded_source": degraded_source,
        "textures_will_be_compressed": bool(slots) and compressonator.available,
    }

    if ctx.options.get("dry_run"):
        status.log("dry-run: no artifacts written")
        status.provenance = _provenance(ctx, tool_versions={})
        return status

    # --- geometry ---------------------------------------------------------
    geometry_source = Path(source["_resolved"])
    if geometry_source.suffix.lower() in BLEND_SUFFIXES:
        geometry_source = _export_blend(ctx, geometry_source, out_dir, asset_id)

    chain = build_lod_chain(geometry_source, out_dir, asset_id=asset_id, policy=policy,
                            prefer=str(ctx.options.get("simplifier") or "auto"),
                            allow_degraded=degraded_ok, timeout=timeout, seed=ctx.seed)

    for message in chain.warnings:
        status.warn(message)

    artifacts: list[dict[str, Any]] = []
    for record in chain.records:
        # Role describes what the artifact IS, not where it currently sits: the
        # LOD files are runtime assets staged under var/intermediate until
        # publish_asset promotes them. `validate/checks.py` reads role='runtime'
        # to decide a LOD is a published runtime asset (lods.published).
        status.add_artifact(ctx.artifact(record.path, role="runtime"))
        artifacts.append(artifact_entry(record.path, record.sha256, record.bytes, "runtime",
                                        record.importer_settings()))
    if chain.collision:
        status.add_artifact(ctx.artifact(chain.collision["path"], role="collision"))
        artifacts.append(artifact_entry(chain.collision["path"], chain.collision["sha256"],
                                        chain.collision["bytes"], "collision",
                                        {"triangles": chain.collision["triangles"],
                                         "vertices": chain.collision["vertices"],
                                         "convex": False,
                                         "convex_hull_implemented": False,
                                         "generated_by": "generate_lods"}))
    if chain.impostor:
        status.add_artifact(ctx.artifact(chain.impostor["path"], role="intermediate"))
        artifacts.append(artifact_entry(chain.impostor["path"], chain.impostor["sha256"],
                                        chain.impostor["bytes"], "intermediate",
                                        {"kind": "impostor_stub", "generated": False,
                                         "generated_by": "generate_lods"}))

    metrics = chain_metrics(chain)
    # Geometry memory floor per LOD. It is a byte count of real buffer views, but
    # GPU residency is not what a driver allocates, so it is never "measured".
    from ..benchmark.measure import resident_bytes

    for record in chain.records:
        metrics.append({
            "name": f"gpu_resident_bytes.{record.name}",
            "value": float(resident_bytes(record.path)),
            "unit": "bytes",
            "measured": False,
        })

    # --- textures ---------------------------------------------------------
    texture_report = None
    if slots:
        texture_dir = out_dir / "textures"
        texture_report = compress_textures(
            [(str(path), kind_of) for path, kind_of in slots], texture_dir,
            allow_degraded=degraded_ok, timeout=timeout)
        for outcome in texture_report.outcomes:
            artifact = ctx.artifact(outcome.path, role="texture")
            status.add_artifact(artifact)
            artifacts.append(artifact_entry(outcome.path, artifact.sha256 or "",
                                            outcome.bytes_after, "texture",
                                            outcome.importer_settings()))
        for message in texture_report.warnings:
            status.warn(message)
        if texture_report.degraded:
            status.warn("textures written UNCOMPRESSED (Compressonator unavailable); "
                        "this asset must not be published as if it were compressed")
        metrics.extend(texture_report.metrics())

    # --- manifest ---------------------------------------------------------
    updated = update_manifest(manifest_path, manifest, metrics=metrics, artifacts_to_add=artifacts)
    status.add_artifact(ctx.artifact(updated, role="report"))

    status.metrics.update({
        "lod_count": len(chain.records),
        "triangles": [r.triangles for r in chain.records],
        "vertices": [r.vertices for r in chain.records],
        "monotonic": chain.monotonic(),
        "simplifier": chain.simplifier,
        "degraded": chain.degraded,
        "lod_bytes": [r.bytes for r in chain.records],
        "out_dir": str(out_dir),
    })
    if texture_report is not None:
        status.metrics["texture_bytes"] = bytes_before_after(texture_report)

    tool_versions = {
        "gltfpack": (toolbox.discover("gltfpack").version
                     or ("present" if toolbox.discover("gltfpack").available else "not-available")),
        "compressonator": (compressonator.version
                           or ("present" if compressonator.available else "not-available")),
        "simplifier": chain.simplifier,
    }
    status.provenance = _provenance(ctx, tool_versions=tool_versions,
                                    input_hashes={str(manifest_path): ctx.artifact(manifest_path).sha256})

    # --- acceptance of the chain itself ------------------------------------
    structural = chain.validation.get("structural_issues") or []
    status.metrics["structural_issues"] = structural
    external = chain.validation.get("external") or {}
    status.metrics["external_validation"] = external
    if external.get("reason"):
        status.warn(f"external glTF validation not performed: {external['reason']}")
    if structural:
        raise ValidationFailed(
            f"LOD chain failed its structural self-check: {structural[0].get('kind')}",
            detail={"issues": structural[:10]},
            hint="see var/reports/manifests for the recorded metrics")
    if chain.degraded:
        status.warn("LOD chain was built without meshoptimizer; simplification quality is "
                    "unverified (metrics measured=False)")
    status.exit_code = EXIT_OK
    status.ok = True
    return status


def _provenance(ctx: Context, *, tool_versions: dict[str, str],
                input_hashes: dict[str, str] | None = None) -> dict[str, Any]:
    hashes = dict(input_hashes or {})
    if ctx.options.get("definition"):
        hashes.update(ctx.input_hashes())
    return {
        "generator": "stalthos-toolkit",
        "toolkit_version": __version__,
        "tool_versions": tool_versions or {"stage6": "no-external-tool"},
        "command": "generate_lods",
        "seed": ctx.seed,
        "input_hashes": hashes,
        "allow_degraded_source": allow_degraded(ctx)[1],
    }


def _export_blend(ctx: Context, blend: Path, out_dir: Path, asset_id: str) -> Path:
    """Export a Blender master to glTF via the stage-3 Blender runner.

    The runner is owned by another stage and is imported lazily: this module
    never re-implements it. If it is absent, so is the capability, and the
    honest result is exit 4 with an actionable hint.
    """
    from ..core.workspace import find_blender

    find_blender(ctx.options.get("blender"))  # raises MissingDependency (exit 4) when absent
    target = out_dir / f"{asset_id}.source.glb"
    try:
        from ..blender import runner  # noqa: PLC0415 - lazy, optional dependency
    except Exception as exc:
        raise MissingDependency(
            "blender.runner",
            hint="the Blender runner is provided by the generation stage; without it the "
                 "toolkit cannot export a .blend master to glTF (import failed: "
                 f"{exc}). Supply an existing .glb as the manifest source artifact instead.",
        ) from exc

    for name in ("export_gltf", "export_glb"):
        function = getattr(runner, name, None)
        if callable(function):
            function(blend, target, seed=ctx.seed)
            if target.exists():
                return target
            raise ExternalToolFailed(f"blender.runner.{name}", -1,
                                     stderr=f"did not write {target}")
    raise MissingDependency(
        "blender.runner.export_gltf",
        hint="the installed Blender runner exposes no export entry point; add "
             "export_gltf(blend_path, glb_path, *, seed) or publish a .glb source artifact",
    )


__all__ = ["generate_lods", "KIND_DIRS"]