"""`research_species` -- canonical pipeline stage 1 (source/research).

OFFLINE AND FREE, deliberately:

  * no network calls, no scraping, no API keys, no paid rates;
  * nothing is downloaded, resolved or guessed. A locator that needs a network
    round trip to be understood is recorded as *unresolved*, with the reason.

What it actually does is normalize what the definition already declares --
reference locators (local paths, URLs, DOIs), usage rights, redistribution
flags, confidence, declared evidence tier -- into an auditable research record
at `var/reports/research/<species_id>.json`.

Usage-rights policy (docs/asset-toolkit.md, "Data and provenance"):
  a reference without `usage_rights`, or with `usage_rights: unknown`, is
  recorded with uncertainty tier `unverified` and raises a warning. It never
  silently becomes redistributable.
"""

from __future__ import annotations

import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .. import __version__
from ..core import Context, Status, command, sha256_file
from ..core.errors import EXIT_OK, UsageError
from ..schemas.formats import LOCAL_KINDS, check_local_path, local_media_type
from ..schemas.registry import require_valid

RECORD_SCHEMA = "research-record"
RECORD_SCHEMA_VERSION = "1.0"

#: usage_rights values that count as established. Anything else is unverified.
VERIFIED_RIGHTS = frozenset(
    {"cc0", "cc-by", "cc-by-sa", "public_domain", "own_work", "permission_granted", "internal_reference"}
)

#: Evidence tiers ordered from most to least supported. `unverified` is the
#: degradation applied when a reference's rights could not be established.
TIER_ORDER = ("documented", "comparative_inference", "biomechanical_plausibility", "speculative", "unverified")


@command("research_species", "Normalize species references into an offline research record",
         stage="1 source/research")
def research_species(ctx: Context) -> Status:
    status = ctx.new_status("research_species")

    definition = ctx.load_definition(ctx.options.get("definition"))
    species_id = str(definition.get("id") or "")
    if not species_id:
        raise UsageError("definition has no id; cannot name the research record")

    definition_path = ctx.definition_path(ctx.options.get("definition"))
    warnings: list[str] = []

    references = definition.get("references") or []
    if not references:
        raise UsageError(
            f"{definition_path.name} declares no references; stage 1 has nothing to normalize",
            hint="add a `references:` list with at least one locator",
        )

    # Where relative local paths are resolved from: the definition's own folder
    # first, then the repository `species/` root it may sit under, then
    # $STEALTHOS_PLAN_DOCS when the definition cites planning docs that live
    # outside this repository (they do: Rauken-Archipelago/ is planning-only).
    base_dirs = [definition_path.parent, definition_path.parent.parent]
    plan_docs = os.environ.get("STEALTHOS_PLAN_DOCS")
    if plan_docs:
        base_dirs.append(Path(plan_docs).expanduser())
    declared = definition.get("uncertainty")
    declared_tier = declared if isinstance(declared, str) else None

    normalized: list[dict[str, Any]] = []
    unresolved: list[dict[str, Any]] = []
    rights_counts: dict[str, int] = {}

    for index, reference in enumerate(references):
        entry, entry_warnings = _normalize_reference(reference, index, base_dirs, declared_tier)
        normalized.append(entry)
        warnings.extend(entry_warnings)

        rights = entry["usage_rights"]
        rights_counts[rights] = rights_counts.get(rights, 0) + 1
        if not entry["resolved"]:
            unresolved.append({
                "reference_id": entry["id"],
                "locator": entry["locator"],
                "reason": "; ".join(entry["notes"]) or "unresolved offline",
            })

    verified = sum(count for rights, count in rights_counts.items() if rights in VERIFIED_RIGHTS)
    unverified_count = sum(count for rights, count in rights_counts.items()
                           if rights not in VERIFIED_RIGHTS)
    redistributable = sum(1 for entry in normalized if entry["redistributable"])

    effective = _effective_tier(declared, unverified_count)

    record: dict[str, Any] = {
        "schema_version": RECORD_SCHEMA_VERSION,
        "record_type": "research-record",
        "species_id": species_id,
        "definition_path": str(definition_path),
        "definition_sha256": sha256_file(definition_path),
        "definition_kind": definition.get("kind"),
        "generator": definition.get("generator"),
        "morphology_family": definition.get("morphology_family"),
        "seed": ctx.seed,
        "created_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "common_names": _common_names(definition),
        "habitat_tags": list(definition.get("habitat_tags") or []),
        "uncertainty": effective,
        "declared_uncertainty": declared if isinstance(declared, str) else None,
        "references": normalized,
        "unresolved": unresolved,
        "usage_rights_summary": {
            "total": len(normalized),
            "verified": verified,
            "unverified": unverified_count,
            "redistributable": redistributable,
            "by_rights": dict(sorted(rights_counts.items())),
        },
        "licenses": list(definition.get("licenses") or []),
        "warnings": sorted(set(warnings)),
        "tool": {
            "name": "stalthos research_species",
            "version": __version__,
            "offline": True,
            "python": ".".join(str(p) for p in __import__("sys").version_info[:2]),
        },
    }

    require_valid(RECORD_SCHEMA, record, source=f"research record for {species_id}")

    for message in record["warnings"]:
        status.warn(f"{species_id}: {message}")
    if effective == "unverified":
        status.warn(
            f"{species_id}: evidence tier degraded to 'unverified' -- "
            f"{unverified_count} reference(s) have no established usage rights"
        )
    if unresolved:
        status.warn(f"{species_id}: {len(unresolved)} locator(s) could not be resolved offline "
                    "(recorded in the report, never guessed)")

    out_path = _record_path(ctx, species_id)
    if ctx.options.get("dry_run"):
        status.log(f"dry-run: would write {out_path}")
        status.metrics = _metrics(record, out_path, written=False)
        status.asset_id = species_id
        status.provenance = _provenance(ctx, record)
        return status

    out_path.parent.mkdir(parents=True, exist_ok=True)
    import json

    out_path.write_text(json.dumps(record, indent=2, default=str) + "\n", encoding="utf-8")
    status.add_artifact(ctx.artifact(out_path, role="report"))
    status.log(f"wrote {out_path}")
    status.metrics = _metrics(record, out_path, written=True)
    status.asset_id = species_id
    status.provenance = _provenance(ctx, record)
    status.exit_code = EXIT_OK
    status.ok = True
    return status


# --- helpers ----------------------------------------------------------------
def _record_path(ctx: Context, species_id: str) -> Path:
    explicit = ctx.options.get("out")
    if explicit:
        return Path(explicit).expanduser()
    return ctx.workspace.reports / "research" / f"{species_id}.json"


def _common_names(definition: dict[str, Any]) -> dict[str, Any]:
    names = definition.get("common_names") or {}
    out: dict[str, Any] = {"primary": str(names.get("primary") or definition.get("id"))}
    if "working" in names:
        out["working"] = bool(names["working"])
    aka = names.get("aka") or []
    if aka:
        out["aka"] = [str(a) for a in aka]
    return out


def _effective_tier(declared: Any, unverified: int) -> str:
    tier = declared if isinstance(declared, str) else "comparative_inference"
    if unverified and tier not in ("documented",):
        return "unverified"
    return tier


#: confidence fallback when the definition declares no overall evidence tier.
_CONFIDENCE_TIER = {
    "high": "comparative_inference",
    "medium": "biomechanical_plausibility",
    "low": "speculative",
}


def _reference_tier(rights_verified: bool, reference: dict[str, Any],
                    declared_tier: str | None) -> str:
    """Per-reference uncertainty tier.

    Usage rights that cannot be established make the reference `unverified`
    regardless of how confident the author feels about the content: an
    unusable reference cannot support a design decision.
    """
    if not rights_verified:
        return "unverified"
    if declared_tier:
        return declared_tier
    return _CONFIDENCE_TIER.get(str(reference.get("confidence") or "medium"),
                                "biomechanical_plausibility")


def _normalize_reference(reference: dict[str, Any], index: int,
                         base_dirs: list[Path], declared_tier: str | None) -> tuple[dict[str, Any], list[str]]:
    notes: list[str] = []
    warnings: list[str] = []
    locator = str(reference.get("locator") or "").strip()
    ref_id = str(reference.get("id") or f"ref{index:02d}")
    kind = str(reference.get("kind") or "web")

    if not locator:
        return (
            {
                "id": ref_id,
                "kind": kind,
                "locator": "",
                "locator_type": "text",
                "normalized_locator": None,
                "title": None,
                "usage_rights": "absent",
                "usage_rights_verified": False,
                "uncertainty_tier": "unverified",
                "redistributable": False,
                "confidence": reference.get("confidence"),
                "supports": list(reference.get("supports") or []),
                "exists": None,
                "resolved": False,
                "notes": ["reference declares no locator"],
            },
            [f"reference {ref_id} declares no locator"],
        )

    info = None
    for base in base_dirs:
        info = check_local_path(locator, base)
        if info.type != "local_path" or info.exists:
            break

    exists = info.exists
    if info.type == "local_path":
        notes.append(f"resolved offline against {[str(b) for b in base_dirs]}; exists={exists}")
        if not exists:
            warnings.append(f"reference {ref_id}: local path not found: {locator}")
    elif info.type == "url":
        notes.append("URL shape normalized offline; content not fetched (no network access)")
    elif info.type == "doi":
        notes.append("DOI shape normalized; content not resolved (no network access)")

    media = local_media_type(locator) if info.type == "local_path" else None
    if info.type == "local_path" and media is None and kind in LOCAL_KINDS:
        notes.append("no recognizable media extension; treated as a raw local path")

    raw_rights = reference.get("usage_rights")
    rights = str(raw_rights) if raw_rights else "absent"
    rights_verified = rights in VERIFIED_RIGHTS
    if not rights_verified:
        if raw_rights is None:
            notes.append("usage_rights absent")
        elif rights == "unknown":
            notes.append("usage_rights explicitly unknown")
        warnings.append(
            f"reference {ref_id}: usage rights unverified ({rights}); "
            "record tier is 'unverified' and the asset is not redistributable"
        )

    redistributable = reference.get("redistributable")
    if not isinstance(redistributable, bool):
        # Conservative default: rights must be verified *and* permissive.
        redistributable = rights in {"cc0", "cc-by", "own_work", "public_domain", "permission_granted"}
        notes.append("redistributable flag absent; derived from usage_rights")

    resolved = bool(locator) and rights_verified and info.type in ("local_path", "doi", "url", "identifier", "text")
    if info.type == "local_path" and not exists:
        resolved = False

    entry: dict[str, Any] = {
        "id": ref_id,
        "kind": kind,
        "locator": locator,
        "locator_type": info.type,
        "normalized_locator": info.normalized,
        "title": str(reference.get("title")) if reference.get("title") else None,
        "usage_rights": rights,
        "usage_rights_verified": rights_verified,
        "uncertainty_tier": _reference_tier(rights_verified, reference, declared_tier),
        "redistributable": bool(redistributable),
        "confidence": str(reference.get("confidence")) if reference.get("confidence") else None,
        "supports": [str(s) for s in (reference.get("supports") or [])],
        "exists": exists,
        "resolved": resolved,
        "notes": notes,
    }
    return entry, warnings


def _metrics(record: dict[str, Any], out_path: Path, *, written: bool) -> dict[str, Any]:
    summary = record["usage_rights_summary"]
    return {
        "record_path": str(out_path),
        "written": written,
        "references": summary["total"],
        "references_verified": summary["verified"],
        "references_unverified": summary["unverified"],
        "unresolved_locators": len(record["unresolved"]),
        "uncertainty": record["uncertainty"],
        "offline": True,
    }


def _provenance(ctx: Context, record: dict[str, Any]) -> dict[str, Any]:
    import sys

    return {
        "generator": "stalthos-toolkit",
        "toolkit_version": __version__,
        "tool_versions": {"python": ".".join(str(p) for p in sys.version_info[:3]),
                          "network_access": "none"},
        "command": "research_species",
        "seed": ctx.seed,
        "input_hashes": ctx.input_hashes(),
        "record_schema": RECORD_SCHEMA,
        "declared_uncertainty": record["declared_uncertainty"],
    }


__all__ = ["research_species", "VERIFIED_RIGHTS", "TIER_ORDER", "RECORD_SCHEMA_VERSION"]