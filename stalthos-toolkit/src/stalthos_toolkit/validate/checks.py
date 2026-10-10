"""Acceptance checks for pipeline stage 8 (quality assurance).

Everything here reads *recorded* data: the manifest, its metrics, the artifact
files and their bytes. **Nothing imports or requires Blender** -- geometry facts
are taken from the metrics other stages measured (docs/toolkit-architecture.md
section 7: report, do not infer). A gate that cannot be evaluated is reported
as `skip`/`warn`, never as `pass`.

Metric/importer conventions this module relies on
-------------------------------------------------
metrics (list of core.manifest.Metric):
  ``triangles``            total triangles of the reference (LOD0) asset
  ``vertices``             vertex count
  ``degenerate_face_ratio`` fraction of zero-area faces
  ``inverted_normal_ratio``fraction of outward-inconsistent normals
  ``uv_overlap_ratio``     fraction of UV area consumed by the top 1% overlap
  ``unreferenced_vertex_ratio``
  ``lod_count``            number of LODs expected (optional)
  ``lod_triangles.<name>`` fallback triangle counts per LOD
  ``texture_count``, ``texture_bytes`` (optional)

artifact importer_settings (dict):
  ``{"lod": 0, "triangles": 48213}``          LOD chain membership
  ``{"kind": "normal", "expected_format": "BC5", "size": [w, h]}``  texture slot
  ``{"lod": 0, "triangles": N, "generated_by": "generate_lods"}``
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Iterable

from ..core.seeding import canonical_json, sha256_file, sha256_bytes
from ..schemas.formats import rfc3339
from ..schemas.registry import format_issues, validate_against
from .ktx import COLOR_FORMATS, NORMAL_FORMATS, TextureFormatInfo, sniff_texture

OUTCOME_PASS = "pass"
OUTCOME_FAIL = "fail"
OUTCOME_WARN = "warn"
OUTCOME_SKIP = "skip"

#: gate metrics and the acceptable (min, max) range. None = unbounded on that side.
GEOMETRY_GATES: dict[str, tuple[float | None, float | None]] = {
    "triangles": (1.0, None),
    "vertices": (3.0, None),
    "degenerate_face_ratio": (None, 0.001),
    "inverted_normal_ratio": (None, 0.0),
    "uv_overlap_ratio": (None, 0.05),
    "unreferenced_vertex_ratio": (None, 0.10),
}

#: expected block compression per texture slot kind.
FORMAT_EXPECTATIONS: dict[str, tuple[frozenset[str], str]] = {
    "normal": (NORMAL_FORMATS, "BC5"),
    "color": (COLOR_FORMATS, "BC7"),
    "colour": (COLOR_FORMATS, "BC7"),
}


@dataclass
class Check:
    id: str
    outcome: str
    detail: str = ""
    severity: str = "error"
    data: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        out: dict[str, Any] = {"id": self.id, "outcome": self.outcome, "severity": self.severity}
        if self.detail:
            out["detail"] = self.detail
        if self.data:
            out["data"] = self.data
        return out


@dataclass
class AcceptanceReport:
    asset_id: str
    outcome: str = OUTCOME_PASS                 # pass | fail
    checks: list[Check] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    def add(self, check: Check) -> Check:
        self.checks.append(check)
        return check

    def add_all(self, checks: Iterable[Check]) -> None:
        for check in checks:
            self.add(check)

    def counts(self) -> dict[str, int]:
        out = {OUTCOME_PASS: 0, OUTCOME_FAIL: 0, OUTCOME_WARN: 0, OUTCOME_SKIP: 0}
        for check in self.checks:
            out[check.outcome] = out.get(check.outcome, 0) + 1
        return out

    def finish(self) -> "AcceptanceReport":
        counts = self.counts()
        self.outcome = OUTCOME_FAIL if counts[OUTCOME_FAIL] else OUTCOME_PASS
        return self

    def check_dicts(self) -> list[dict[str, Any]]:
        return [check.to_dict() for check in self.checks]

    def to_dict(self) -> dict[str, Any]:
        return {
            "asset_id": self.asset_id,
            "outcome": self.outcome,
            "counts": self.counts(),
            "checks": self.check_dicts(),
        }


# --- helpers ----------------------------------------------------------------
def _metrics_map(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {str(m.get("name")): m for m in manifest.get("metrics", []) or [] if isinstance(m, dict)}


def _artifacts(manifest: dict[str, Any], role: str | None = None) -> list[dict[str, Any]]:
    artifacts = [a for a in manifest.get("artifacts", []) or [] if isinstance(a, dict)]
    if role is None:
        return artifacts
    return [a for a in artifacts if a.get("role") == role]


def _resolve(path_text: str, workspace_root: Path | None) -> Path:
    path = Path(path_text)
    if not path.is_absolute() and workspace_root is not None:
        return (workspace_root / path).resolve()
    return path


def semantic_hash(manifest: dict[str, Any]) -> str:
    """Hash of the manifest's semantic content.

    Extends `core.manifest.Manifest.content_hash` (which already drops
    `created_at` and `tool_versions`) by also dropping the `validation` record.
    Reason: stage 8 writes its own verdict into that record, so including it
    would change the hash on every run and make cross-run comparison impossible.
    Proposed upstream change: `Manifest.semantic_hash()` in core.
    """
    payload = dict(manifest)
    payload.pop("created_at", None)
    payload.pop("validation", None)
    provenance = dict(payload.get("provenance") or {})
    provenance.pop("tool_versions", None)
    payload["provenance"] = provenance
    return sha256_bytes(canonical_json(payload).encode("utf-8"))


# --- individual checks ------------------------------------------------------
def check_manifest_schema(manifest: dict[str, Any]) -> list[Check]:
    issues = validate_against("asset-manifest", manifest)
    if not issues:
        return [Check("manifest.schema_completeness", OUTCOME_PASS,
                      "manifest satisfies asset-manifest.schema.json")]
    return [Check("manifest.schema_completeness", OUTCOME_FAIL,
                  format_issues(issues, source="manifest"),
                  data={"error_count": len(issues),
                        "errors": [issue.to_dict() for issue in issues[:20]]})]


def check_identity(manifest: dict[str, Any]) -> list[Check]:
    asset_id = manifest.get("asset_id")
    checks: list[Check] = []
    if isinstance(asset_id, str) and asset_id.strip():
        checks.append(Check("manifest.identity", OUTCOME_PASS, f"asset_id={asset_id}",
                            data={"asset_id": asset_id}))
    else:
        checks.append(Check("manifest.identity", OUTCOME_FAIL, "asset_id missing or empty"))
    return checks


def check_artifacts(manifest: dict[str, Any], workspace_root: Path | None) -> list[Check]:
    artifacts = manifest.get("artifacts", []) or []
    if not artifacts:
        return [Check("artifacts.present", OUTCOME_FAIL, "manifest lists no artifacts")]

    missing: list[str] = []
    unhashed: list[str] = []
    mismatched: list[dict[str, Any]] = []
    unreadable: list[str] = []
    size_mismatch: list[str] = []

    for artifact in artifacts:
        raw_path = str(artifact.get("path", ""))
        path = _resolve(raw_path, workspace_root)
        if not path.exists():
            missing.append(raw_path)
            continue
        declared = artifact.get("sha256")
        if not declared:
            unhashed.append(raw_path)
            continue
        try:
            actual = sha256_file(path)
        except OSError:
            unreadable.append(raw_path)
            continue
        if actual != declared:
            mismatched.append({"path": raw_path, "declared": declared, "actual": actual})
        declared_bytes = artifact.get("bytes")
        if isinstance(declared_bytes, int) and path.exists():
            try:
                if path.stat().st_size != declared_bytes:
                    size_mismatch.append(raw_path)
            except OSError:  # pragma: no cover - race
                unreadable.append(raw_path)

    checks = [Check("artifacts.present", OUTCOME_FAIL if missing else OUTCOME_PASS,
                    (f"{len(missing)} artifact(s) missing on disk" if missing
                     else f"all {len(artifacts)} artifacts exist"),
                    data={"missing": missing})]
    checks.append(Check("artifacts.hashed", OUTCOME_FAIL if unhashed else OUTCOME_PASS,
                        (f"{len(unhashed)} artifact(s) carry no sha256" if unhashed
                         else "every artifact carries a sha256"),
                        data={"unhashed": unhashed}))
    checks.append(Check("artifacts.hash_match", OUTCOME_FAIL if mismatched else OUTCOME_PASS,
                        (f"{len(mismatched)} artifact hash mismatch(es)" if mismatched
                         else "every recorded hash matches the file on disk"),
                        data={"mismatched": mismatched}))
    if size_mismatch:
        checks.append(Check("artifacts.byte_size", OUTCOME_WARN,
                            f"{len(size_mismatch)} artifact byte count(s) differ",
                            severity="warning", data={"paths": size_mismatch}))
    if unreadable:
        checks.append(Check("artifacts.readable", OUTCOME_FAIL,
                            f"{len(unreadable)} artifact(s) could not be read",
                            data={"paths": unreadable}))
    return checks


def check_geometry_sanity(manifest: dict[str, Any]) -> list[Check]:
    """Read geometry facts from recorded metrics. Blender is never invoked."""
    metrics = _metrics_map(manifest)
    present = {name: m for name, m in metrics.items() if name in GEOMETRY_GATES}
    checks: list[Check] = []

    if not present:
        checks.append(Check("geometry.sanity", OUTCOME_SKIP,
                            "no geometry metrics recorded by earlier stages; "
                            "cannot evaluate without Blender-free recorded metrics"))
        return checks

    failures: list[dict[str, Any]] = []
    unmeasured: list[str] = []
    for name, (minimum, maximum) in GEOMETRY_GATES.items():
        metric = present.get(name)
        if metric is None:
            continue
        value = metric.get("value")
        if not isinstance(value, (int, float)):
            failures.append({"metric": name, "error": "value is not numeric"})
            continue
        if not metric.get("measured", True):
            unmeasured.append(name)
            continue  # an estimate never gates; reported separately below
        if minimum is not None and value < minimum:
            failures.append({"metric": name, "value": value, "error": f"< minimum {minimum}"})
        if maximum is not None and value > maximum:
            failures.append({"metric": name, "value": value, "error": f"> maximum {maximum}"})

    detail = "recorded geometry metrics within gate bounds" if not failures else (
        f"{len(failures)} geometry gate(s) violated")
    checks.append(Check("geometry.sanity", OUTCOME_FAIL if failures else OUTCOME_PASS,
                        detail, data={"violations": failures, "checked": sorted(present)}))

    if unmeasured:
        checks.append(Check("geometry.measured", OUTCOME_WARN,
                            f"{len(unmeasured)} gate metric(s) are estimates (measured=false): "
                            f"{', '.join(sorted(unmeasured))} -- estimates never gate acceptance",
                            severity="warning", data={"metrics": sorted(unmeasured)}))
    return checks


def check_lod_chain(manifest: dict[str, Any]) -> list[Check]:
    entries: list[dict[str, Any]] = []
    for artifact in manifest.get("artifacts", []) or []:
        settings = artifact.get("importer_settings") or {}
        if isinstance(settings, dict) and "lod" in settings and "triangles" in settings:
            triangles = settings.get("triangles")
            if isinstance(triangles, (int, float)):
                entries.append({
                    "lod": settings.get("lod"),
                    "triangles": triangles,
                    "path": artifact.get("path"),
                    "role": artifact.get("role"),
                })

    metrics = _metrics_map(manifest)
    if not entries:
        # Fall back to metric naming: lod_triangles.<name>
        for name, metric in sorted(metrics.items()):
            if name.startswith("lod_triangles.") and isinstance(metric.get("value"), (int, float)):
                entries.append({"lod": name.rsplit(".", 1)[1], "triangles": metric["value"],
                                "path": None, "role": "runtime"})

    if not entries:
        return [Check("lods.chain", OUTCOME_WARN,
                      "no LOD chain recorded in the manifest (no artifacts with "
                      "importer_settings.lod and no lod_triangles.* metrics); "
                      "LOD presence is unverified, not passed",
                      severity="warning")]

    entries.sort(key=lambda item: (str(item["lod"])))
    try:
        ordered = sorted(entries, key=lambda item: int(item["lod"]))
    except (TypeError, ValueError):
        ordered = entries

    counts = [int(item["triangles"]) for item in ordered]
    non_monotonic = []
    for previous, current, item_a, item_b in zip(counts, counts[1:], ordered, ordered[1:]):
        if current >= previous:
            non_monotonic.append({
                "from_lod": item_a["lod"], "from_triangles": previous,
                "to_lod": item_b["lod"], "to_triangles": current,
            })

    missing_roles = [item["lod"] for item in ordered if item.get("role") not in ("runtime", "source")]
    checks = [Check("lods.chain", OUTCOME_FAIL if non_monotonic else OUTCOME_PASS,
                    (f"triangle counts are not strictly decreasing across {len(ordered)} LODs"
                     if non_monotonic else
                     f"{len(ordered)} LOD(s) present with strictly decreasing triangle counts"),
                    data={"chain": ordered, "non_monotonic": non_monotonic})]
    if missing_roles:
        checks.append(Check("lods.published", OUTCOME_FAIL,
                            f"LOD artifact(s) {missing_roles} are not published runtime assets",
                            data={"lods": missing_roles}))
    if len(ordered) < 2:
        checks.append(Check("lods.count", OUTCOME_WARN,
                            "only one LOD recorded; a streamable asset needs a chain",
                            severity="warning"))
    return checks


def check_textures(manifest: dict[str, Any], workspace_root: Path | None) -> list[Check]:
    textures = _artifacts(manifest, "texture")
    all_artifacts = manifest.get("artifacts", []) or []
    if not textures:
        loose = [a for a in all_artifacts
                 if isinstance(a, dict) and str(a.get("path", "")).lower().endswith((".ktx2", ".png", ".tga", ".dds"))]
        if loose:
            return [Check("textures.inventory", OUTCOME_FAIL,
                          f"{len(loose)} texture file(s) present but not declared with role='texture'",
                          data={"paths": [a.get("path") for a in loose]})]
        return [Check("textures.inventory", OUTCOME_SKIP,
                      "manifest declares no textures; texture checks not applicable")]

    metrics = _metrics_map(manifest)
    undeclared = [a.get("path") for a in textures
                  if not isinstance(a.get("importer_settings"), dict)
                  or "kind" not in (a.get("importer_settings") or {})]
    checks = [Check("textures.inventory", OUTCOME_FAIL if undeclared else OUTCOME_PASS,
                    (f"{len(undeclared)} texture(s) lack importer_settings.kind (slot classification)"
                     if undeclared else f"{len(textures)} texture slot(s) declared"),
                    data={"slots": [a.get("path") for a in textures], "undeclared": undeclared})]

    format_failures: list[dict[str, Any]] = []
    format_warnings: list[dict[str, Any]] = []
    inspected: list[dict[str, Any]] = []

    for artifact in textures:
        settings = artifact.get("importer_settings") or {}
        slot = str(settings.get("kind", "unknown"))
        expected = str(settings.get("expected_format", "")) or None
        path = _resolve(str(artifact.get("path", "")), workspace_root)
        if not path.exists():
            format_failures.append({"path": str(path), "error": "texture file missing"})
            continue
        info: TextureFormatInfo = sniff_texture(path)
        inspected.append({"path": artifact.get("path"), "slot": slot, **info.to_dict()})

        if slot in FORMAT_EXPECTATIONS:
            accepted, canonical = FORMAT_EXPECTATIONS[slot]
            if info.codec is None:
                format_warnings.append({
                    "path": artifact.get("path"), "slot": slot, "container": info.container,
                    "error": f"{slot} slot is stored uncompressed as {info.container}; "
                             f"{canonical} expected and compression was not verified",
                })
            elif info.codec not in accepted:
                format_failures.append({
                    "path": artifact.get("path"), "slot": slot, "codec": info.codec,
                    "error": f"{slot} slot must be {canonical}-family, found {info.codec}",
                })
        if expected and info.codec and not info.codec.upper().startswith(expected.upper()):
            format_failures.append({
                "path": artifact.get("path"), "declared_expected_format": expected,
                "codec": info.codec, "error": "declared expected_format does not match the file",
            })
        if not info.readable:
            format_failures.append({"path": artifact.get("path"), "error": "texture file unreadable"})

    if textures:
        checks.append(Check("textures.format", OUTCOME_FAIL if format_failures else OUTCOME_PASS,
                            (f"{len(format_failures)} texture format violation(s)" if format_failures
                             else f"{len(textures)} texture(s) match the expected slot formats"),
                            data={"failures": format_failures, "inspected": inspected}))
        if format_warnings:
            checks.append(Check("textures.compression", OUTCOME_WARN,
                                f"{len(format_warnings)} texture(s) uncompressed/unverified",
                                severity="warning", data={"entries": format_warnings}))

    total_bytes = metrics.get("texture_bytes")
    if isinstance(total_bytes, dict) and total_bytes.get("measured") is False:
        checks.append(Check("textures.memory", OUTCOME_WARN,
                            "texture_bytes is an estimate (measured=false)",
                            severity="warning"))
    return checks


def check_provenance(manifest: dict[str, Any]) -> list[Check]:
    provenance = manifest.get("provenance") or {}
    checks: list[Check] = []

    # `created_at` carries "format": "date-time" in asset-manifest.schema.json,
    # but the hand-rolled validator treats `format` as annotation-only (correct
    # for draft 2020-12, where assertion format is a separate opt-in). schemas/
    # validator.py documents that the semantic formats are layered on here
    # instead — this is that layer, and until it existed the check it advertises
    # was dead code and `created_at: "banana"` passed validation.
    created_at = manifest.get("created_at")
    if isinstance(created_at, str) and created_at.strip():
        if rfc3339(created_at):
            checks.append(Check("provenance.created_at", OUTCOME_PASS,
                                f"created_at={created_at}", data={"created_at": created_at}))
        else:
            checks.append(Check("provenance.created_at", OUTCOME_FAIL,
                                f"created_at is not RFC 3339: {created_at!r}",
                                data={"created_at": created_at}))
    else:
        checks.append(Check("provenance.created_at", OUTCOME_FAIL,
                            "provenance.created_at missing or not a string"))

    seed = provenance.get("seed")
    if isinstance(seed, int) and not isinstance(seed, bool):
        checks.append(Check("provenance.seed", OUTCOME_PASS, f"seed={seed}", data={"seed": seed}))
    else:
        checks.append(Check("provenance.seed", OUTCOME_FAIL, "provenance.seed missing or not an integer"))

    input_hashes = provenance.get("input_hashes") or {}
    bad_hashes = [k for k, v in input_hashes.items()
                  if not isinstance(v, str) or len(v) != 64]
    if not input_hashes:
        checks.append(Check("provenance.input_hashes", OUTCOME_FAIL,
                            "provenance.input_hashes is empty; the run cannot be proven reproducible"))
    elif bad_hashes:
        checks.append(Check("provenance.input_hashes", OUTCOME_FAIL,
                            f"{len(bad_hashes)} input hash(es) are not sha256 hex",
                            data={"keys": bad_hashes}))
    else:
        checks.append(Check("provenance.input_hashes", OUTCOME_PASS,
                            f"{len(input_hashes)} input hash(es) recorded"))

    tool_versions = provenance.get("tool_versions") or {}
    if not tool_versions:
        checks.append(Check("provenance.tool_versions", OUTCOME_FAIL,
                            "provenance.tool_versions is empty; reproducibility cannot be claimed"))
    else:
        checks.append(Check("provenance.tool_versions", OUTCOME_PASS,
                            f"tool versions recorded: {sorted(tool_versions)}",
                            data={"tool_versions": tool_versions}))

    licenses = provenance.get("licenses") or []
    incomplete = [l.get("id") for l in licenses
                  if not isinstance(l, dict) or not l.get("license") or not l.get("applies_to")]
    if not licenses:
        checks.append(Check("provenance.licenses", OUTCOME_FAIL,
                            "no license provenance recorded for the asset"))
    elif incomplete:
        checks.append(Check("provenance.licenses", OUTCOME_FAIL,
                            f"{len(incomplete)} license record(s) lack license/applies_to",
                            data={"ids": incomplete}))
    else:
        checks.append(Check("provenance.licenses", OUTCOME_PASS,
                            f"{len(licenses)} license record(s) present"))

    references = provenance.get("references") or []
    unknown_rights = [r.get("id") for r in references
                      if isinstance(r, dict) and str(r.get("usage_rights", "unknown")) in ("unknown", "")]
    if references and unknown_rights:
        checks.append(Check("provenance.reference_rights", OUTCOME_WARN,
                            f"{len(unknown_rights)} reference(s) have unestablished usage rights",
                            severity="warning", data={"ids": unknown_rights}))
    return checks


def check_determinism(manifest: dict[str, Any]) -> list[Check]:
    """Content-hash stability, provable without regenerating anything."""
    checks: list[Check] = []
    current = semantic_hash(manifest)

    validation = manifest.get("validation") or {}
    prior = None
    for check in validation.get("checks", []) or []:
        if isinstance(check, dict) and check.get("id") == "determinism.semantic_hash":
            prior = (check.get("data") or {}).get("semantic_hash")
            break

    if prior and prior != current:
        checks.append(Check("determinism.semantic_hash", OUTCOME_FAIL,
                            "manifest content changed between runs: recorded "
                            f"{prior[:12]}... != current {current[:12]}...; the asset is not "
                            "reproducible from the manifest",
                            data={"recorded": prior, "current": current}))
    elif prior:
        checks.append(Check("determinism.semantic_hash", OUTCOME_PASS,
                            "manifest content hash matches the previous validated run",
                            data={"semantic_hash": current, "baseline": "previous_run"}))
    else:
        checks.append(Check("determinism.semantic_hash", OUTCOME_PASS,
                            "no previous validation record to compare; baseline recorded now",
                            severity="info", data={"semantic_hash": current, "baseline": "this_run"}))

    # A round-trip through the core dataclass must not change the meaning.
    try:
        from ..core.manifest import Manifest

        round_trip = Manifest.from_dict(manifest).to_dict()
        stable = (semantic_hash(_fill_core_defaults(manifest))
                  == semantic_hash(_fill_core_defaults(round_trip)))
        checks.append(Check("determinism.round_trip", OUTCOME_PASS if stable else OUTCOME_FAIL,
                            "manifest survives a save/load round trip unchanged" if stable
                            else "manifest does not round-trip through core.manifest.Manifest",
                            data={"semantic_hash": current if stable else None}))
    except Exception as exc:  # pragma: no cover - defensive
        checks.append(Check("determinism.round_trip", OUTCOME_WARN,
                            f"round-trip check could not run: {exc}", severity="warning"))
    return checks


def _fill_core_defaults(manifest: dict[str, Any]) -> dict[str, Any]:
    """Fill in the dataclass defaults core.manifest applies on load.

    Without this, a hand-written manifest that omits `unit` on a metric would
    differ from its own round trip for a reason that carries no meaning. The
    defaults are read from the frozen dataclasses, never hard-coded here.
    """
    import dataclasses

    from ..core.manifest import ArtifactRef, Metric, Provenance, ValidationRecord

    payload = dict(manifest)

    def fill(dataclass_type: Any, entry: dict[str, Any]) -> dict[str, Any]:
        filled = dict(entry)
        for field in dataclasses.fields(dataclass_type):
            if field.name not in filled:
                default = field.default
                filled[field.name] = {} if default is dataclasses.MISSING else default
        return filled

    payload["provenance"] = fill(Provenance, payload.get("provenance") or {})
    payload["validation"] = fill(ValidationRecord, payload.get("validation") or {})
    payload["artifacts"] = [fill(ArtifactRef, a) for a in payload.get("artifacts") or []
                            if isinstance(a, dict)]
    payload["metrics"] = [fill(Metric, m) for m in payload.get("metrics") or []
                          if isinstance(m, dict)]
    return payload


def check_previews(manifest: dict[str, Any], workspace_root: Path | None) -> list[Check]:
    previews = manifest.get("previews") or []
    if not previews:
        return [Check("previews.present", OUTCOME_FAIL,
                      "no preview images recorded; QA review requires a turntable/growth sheet")]
    missing = [str(p) for p in previews if not _resolve(str(p), workspace_root).exists()]
    return [Check("previews.present", OUTCOME_FAIL if missing else OUTCOME_PASS,
                  (f"{len(missing)} preview(s) missing on disk" if missing
                   else f"{len(previews)} preview(s) present"),
                  data={"previews": [str(p) for p in previews], "missing": missing})]


# --- orchestration ----------------------------------------------------------
def run_acceptance(manifest: dict[str, Any], *, workspace_root: Path | None = None,
                   only: Iterable[str] | None = None) -> AcceptanceReport:
    """Run every acceptance check against a manifest dict.

    Returns an `AcceptanceReport`; the caller decides the exit code. Checks that
    cannot be evaluated offline report `skip`/`warn`, never `pass`.
    """
    report = AcceptanceReport(asset_id=str(manifest.get("asset_id", "<unknown>")))
    selected = set(only) if only else None

    plan: list[tuple[str, Callable[[], list[Check]]]] = [
        ("manifest", lambda: check_manifest_schema(manifest)),
        ("manifest", lambda: check_identity(manifest)),
        ("artifacts", lambda: check_artifacts(manifest, workspace_root)),
        ("geometry", lambda: check_geometry_sanity(manifest)),
        ("lods", lambda: check_lod_chain(manifest)),
        ("textures", lambda: check_textures(manifest, workspace_root)),
        ("provenance", lambda: check_provenance(manifest)),
        ("determinism", lambda: check_determinism(manifest)),
        ("previews", lambda: check_previews(manifest, workspace_root)),
    ]

    for group, runner in plan:
        if selected is not None and group not in selected:
            continue
        try:
            report.add_all(runner())
        except Exception as exc:  # a broken check must not hide the rest
            report.add(Check(f"{group}.error", OUTCOME_FAIL,
                             f"acceptance check raised {type(exc).__name__}: {exc}"))
    return report.finish()


__all__ = [
    "AcceptanceReport",
    "Check",
    "GEOMETRY_GATES",
    "OUTCOME_FAIL",
    "OUTCOME_PASS",
    "OUTCOME_SKIP",
    "OUTCOME_WARN",
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
]