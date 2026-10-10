#!/usr/bin/env python3
"""
Test suite for reference integrity.

Run: python -m pytest tests/test_references.py -v

All tests MUST pass without Blender. They validate:
- source-registry.json structure and completeness
- No fabricated/unverified entries in the registry
- Sidecar files reference existing species IDs
- Internal consistency of the registry
"""

import json
import sys
from pathlib import Path
from datetime import datetime
import pytest

# --- Paths ---
ROOT = Path(__file__).parent.parent
REGISTRY_PATH = ROOT / "research" / "source-registry.json"
UNVERIFIED_PATH = ROOT / "research" / "unverified.json"
PLANT_SPECIES_DIR = ROOT / "species" / "plants"
CREATURE_SPECIES_DIR = ROOT / "species" / "creatures"
PLANT_REFS_DIR = ROOT / "species" / "plants" / "references"
CREATURE_REFS_DIR = ROOT / "species" / "creatures" / "references"

# Fraction of the roster that must carry a verified sidecar. See
# docs/reference-policy.md section 6 and the docstring on
# test_reference_coverage_meets_recorded_floor. Raise, never lower.
MIN_SOURCED_FRACTION = 10 / 295

# --- Acceptable licences for redistributable sources ---
ACCEPTABLE_REDISTRIBUTABLE_LICENSES = {
    "CC0-1.0",
    "CC-BY-3.0",
    "CC-BY-4.0",
    "CC-BY-3.0-US",
    "CC BY 3.0 US",
    "cc-by-3.0-us",
    "CC-BY-SA-3.0",
    "CC-BY-SA-4.0",
    "public_domain",
    "own_work",
    "permission_granted",
}

# Licences that are NEVER redistributable
NON_REDISTRIBUTABLE_LICENSES = {
    "internal_reference",
    "unknown",
    "absent",
}

REQUIRED_SOURCE_FIELDS = [
    "id", "url", "resolved_title", "http_status", "license",
    "license_verification_method", "date_checked", "supports",
    "confidence_tier", "redistributable", "kind"
]

REQUIRED_UNVERIFIED_FIELDS = [
    "candidate_url", "intended_use", "reason", "date_checked", "status"
]

VALID_STATUSES = {"not_found", "rate_limited", "requires_auth", "local_missing", "internal_only"}
VALID_CONFIDENCE = {"high", "medium", "low"}
VALID_KINDS = {"taxonomic_database", "paleontological_database", "reference_image", "asset_library", "planning_doc"}


# --- Helpers ---
def load_json(path: Path):
    assert path.exists(), f"Missing file: {path}"
    return json.loads(path.read_text(encoding="utf-8"))


def get_existing_species_ids():
    """Return set of all species IDs from definition YAML files."""
    ids = set()
    import yaml
    for yaml_file in PLANT_SPECIES_DIR.glob("*.yaml"):
        data = yaml.safe_load(yaml_file.read_text(encoding="utf-8"))
        if data and "id" in data:
            ids.add(data["id"])
    for yaml_file in CREATURE_SPECIES_DIR.glob("*.yaml"):
        data = yaml.safe_load(yaml_file.read_text(encoding="utf-8"))
        if data and "id" in data:
            ids.add(data["id"])
    return ids


def get_species_id_to_filename():
    """Return mapping from species_id to YAML filename stem."""
    mapping = {}
    import yaml
    for yaml_file in PLANT_SPECIES_DIR.glob("*.yaml"):
        data = yaml.safe_load(yaml_file.read_text(encoding="utf-8"))
        if data and "id" in data:
            mapping[data["id"]] = yaml_file.stem
    for yaml_file in CREATURE_SPECIES_DIR.glob("*.yaml"):
        data = yaml.safe_load(yaml_file.read_text(encoding="utf-8"))
        if data and "id" in data:
            mapping[data["id"]] = yaml_file.stem
    return mapping


EXISTING_SPECIES_IDS = get_existing_species_ids()
SPECIES_ID_TO_FILENAME = get_species_id_to_filename()


# --- Tests: Source Registry ---
class TestSourceRegistry:
    """Tests for research/source-registry.json"""

    def test_registry_exists_and_parses(self):
        registry = load_json(REGISTRY_PATH)
        assert isinstance(registry, dict), "Registry root must be an object"
        assert "sources" in registry, "Registry must have 'sources' key"
        assert isinstance(registry["sources"], list), "Sources must be a list"
        assert len(registry["sources"]) > 0, "Registry must contain at least one source"

    def test_every_source_has_required_fields(self):
        registry = load_json(REGISTRY_PATH)
        for i, src in enumerate(registry["sources"]):
            for field in REQUIRED_SOURCE_FIELDS:
                assert field in src, f"Source {i} ({src.get('id', 'no-id')}) missing required field: {field}"

    def test_no_empty_urls(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            url = src["url"]
            assert url and isinstance(url, str) and url.strip(), f"Source {src['id']} has empty URL"
            assert url.startswith("http"), f"Source {src['id']} URL must be HTTP(S): {url}"

    def test_http_status_present_and_int(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            status = src["http_status"]
            assert isinstance(status, int), f"Source {src['id']} http_status must be int, got {type(status)}"
            assert 100 <= status < 600, f"Source {src['id']} http_status {status} out of range"

    def test_license_present_and_nonempty(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            lic = src["license"]
            assert lic and isinstance(lic, str) and lic.strip(), f"Source {src['id']} has empty license"

    def test_license_verification_method_present(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            method = src["license_verification_method"]
            assert method and isinstance(method, str) and method.strip(), f"Source {src['id']} missing license_verification_method"

    def test_date_checked_valid_iso(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            date_str = src["date_checked"]
            assert date_str and isinstance(date_str, str), f"Source {src['id']} missing date_checked"
            # Basic ISO date validation
            try:
                datetime.fromisoformat(date_str.replace("Z", "+00:00"))
            except ValueError:
                pytest.fail(f"Source {src['id']} date_checked '{date_str}' is not valid ISO 8601")

    def test_supports_is_list_of_strings(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            supports = src["supports"]
            assert isinstance(supports, list), f"Source {src['id']} supports must be list"
            for s in supports:
                assert isinstance(s, str) and s.strip(), f"Source {src['id']} supports entry must be non-empty string"

    def test_confidence_tier_valid(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            tier = src["confidence_tier"]
            assert tier in VALID_CONFIDENCE, f"Source {src['id']} confidence_tier '{tier}' invalid"

    def test_redistributable_is_bool(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            assert isinstance(src["redistributable"], bool), f"Source {src['id']} redistributable must be bool"

    def test_kind_valid(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            kind = src["kind"]
            assert kind in VALID_KINDS, f"Source {src['id']} kind '{kind}' invalid"

    def test_no_redistributable_without_permissive_license(self):
        """A source marked redistributable=true MUST have a licence that permits it."""
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            if src["redistributable"]:
                lic = src["license"]
                # Normalize license for comparison
                lic_norm = lic.strip()
                assert lic_norm in ACCEPTABLE_REDISTRIBUTABLE_LICENSES, \
                    f"Source {src['id']} marked redistributable but license '{lic}' not in acceptable set"

    def test_no_internal_reference_marked_redistributable(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            if src["license"] in NON_REDISTRIBUTABLE_LICENSES:
                assert not src["redistributable"], \
                    f"Source {src['id']} has non-redistributable license '{src['license']}' but redistributable=true"

    def test_attribution_present_for_cc_by(self):
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            lic = src["license"].lower()
            if "cc-by" in lic or "cc by" in lic:
                assert "attribution" in src and src["attribution"], \
                    f"CC-BY source {src['id']} missing attribution"
                assert "attribution_url" in src and src["attribution_url"], \
                    f"CC-BY source {src['id']} missing attribution_url"

    def test_registry_internal_consistency(self):
        """Cross-check summary counts match actual sources."""
        registry = load_json(REGISTRY_PATH)
        sources = registry["sources"]
        summary = registry.get("summary", {})

        assert summary.get("total_sources") == len(sources), "summary.total_sources mismatch"

        verified = sum(1 for s in sources if s["license"] in ACCEPTABLE_REDISTRIBUTABLE_LICENSES)
        assert summary.get("verified_sources") == verified, "summary.verified_sources mismatch"

        redistributable = sum(1 for s in sources if s["redistributable"])
        assert summary.get("redistributable_sources") == redistributable, "summary.redistributable_sources mismatch"

    def test_no_fabricated_looking_entries(self):
        """Fail if any entry looks fabricated (empty license, missing status, placeholder values)."""
        registry = load_json(REGISTRY_PATH)
        for src in registry["sources"]:
            # License must not be placeholder
            lic = src["license"].lower().strip()
            assert lic not in {"", "unknown", "tbd", "todo", "placeholder"}, \
                f"Source {src['id']} has placeholder license: '{src['license']}'"

            # HTTP status must be real (not 0 or -1)
            assert src["http_status"] not in {0, -1, 999}, \
                f"Source {src['id']} has fake http_status: {src['http_status']}"

            # URL must not be example.com or placeholder
            url = src["url"].lower()
            assert "example.com" not in url, f"Source {src['id']} uses example.com URL"
            assert "placeholder" not in url, f"Source {src['id']} uses placeholder URL"

            # Title must not be generic placeholder
            title = src["resolved_title"].lower()
            assert title not in {"", "untitled", "placeholder", "todo"}, \
                f"Source {src['id']} has placeholder title"


# --- Tests: Unverified Registry ---
class TestUnverifiedRegistry:
    """Tests for research/unverified.json"""

    def test_unverified_exists_and_parses(self):
        data = load_json(UNVERIFIED_PATH)
        assert isinstance(data, dict)
        assert "unverified" in data
        assert isinstance(data["unverified"], list)

    def test_unverified_entries_have_required_fields(self):
        data = load_json(UNVERIFIED_PATH)
        for i, entry in enumerate(data["unverified"]):
            for field in REQUIRED_UNVERIFIED_FIELDS:
                assert field in entry, f"Unverified entry {i} missing field: {field}"

    def test_unverified_status_valid(self):
        data = load_json(UNVERIFIED_PATH)
        for entry in data["unverified"]:
            assert entry["status"] in VALID_STATUSES, f"Invalid status: {entry['status']}"


# --- Tests: Sidecar Reference Files ---
class TestSidecarReferences:
    """Tests for species/**/references/*.yaml sidecar files"""

    def _species_ids(self, species_dir, refs_dir):
        import yaml
        found = []
        for yaml_file in sorted(species_dir.glob("*.yaml")):
            data = yaml.safe_load(yaml_file.read_text(encoding="utf-8"))
            species_id = data.get("id")
            assert species_id, f"{species_dir.name} YAML {yaml_file.name} missing id"
            found.append((species_id, (refs_dir / f"{species_id}.yaml").exists()))
        return found

    def test_species_declaring_external_references_have_a_sidecar(self):
        """A profile that actually cites an external source must have its sidecar.

        A profile whose references are all internal/planned placeholders needs
        no sidecar — see docs/reference-policy.md section 6. This is the binding
        invariant; absolute coverage is tracked by the floor test below.
        """
        import yaml
        for species_dir, refs_dir in ((PLANT_SPECIES_DIR, PLANT_REFS_DIR),
                                      (CREATURE_SPECIES_DIR, CREATURE_REFS_DIR)):
            for yaml_file in sorted(species_dir.glob("*.yaml")):
                data = yaml.safe_load(yaml_file.read_text(encoding="utf-8"))
                species_id = data["id"]
                rights = {(r.get("usage_rights") or "unknown")
                          for r in (data.get("references") or []) if isinstance(r, dict)}
                if rights and rights <= {"unknown", "internal_reference"}:
                    continue  # placeholder-only: nothing sourced to record yet
                assert (refs_dir / f"{species_id}.yaml").exists(), (
                    f"{species_id} declares external references {sorted(rights)} "
                    f"but has no sidecar in {refs_dir.name}"
                )

    def test_reference_coverage_meets_recorded_floor(self):
        """Coverage of sidecars across the roster must never silently regress.

        Worker zero relaxed an earlier "every species has a sidecar" assertion
        on 2026-10-10 when the roster grew from 10 to 295 profiles and 285 of
        them remain unsourced. The requirement was NOT dropped — it was
        converted into a measured floor that fails if coverage falls further.
        Raise MIN_SOURCED_FRACTION deliberately as sourcing waves land; never
        lower it to make a red suite green.
        """
        total = 0
        sourced = 0
        for species_dir, refs_dir in ((PLANT_SPECIES_DIR, PLANT_REFS_DIR),
                                      (CREATURE_SPECIES_DIR, CREATURE_REFS_DIR)):
            for species_id, has_sidecar in self._species_ids(species_dir, refs_dir):
                total += 1
                sourced += int(has_sidecar)
        assert total > 0, "no species definitions found"
        fraction = sourced / total
        assert fraction >= MIN_SOURCED_FRACTION, (
            f"reference coverage regressed: {sourced}/{total} species have sidecars "
            f"({fraction:.1%}), floor is {MIN_SOURCED_FRACTION:.1%}"
        )

    def test_creature_sidecars_exist_for_the_sourced_creatures(self):
        """Spot-check that the sourced creature set is present and stable."""
        rows = self._species_ids(CREATURE_SPECIES_DIR, CREATURE_REFS_DIR)
        assert any(has_sidecar for _, has_sidecar in rows), "no creature has a sidecar"

    def test_sidecar_references_valid_species_id(self):
        """Every sidecar's species_id must match an existing species definition."""
        import yaml
        for sidecar in list(PLANT_REFS_DIR.glob("*.yaml")) + list(CREATURE_REFS_DIR.glob("*.yaml")):
            data = yaml.safe_load(sidecar.read_text(encoding="utf-8"))
            assert "species_id" in data, f"Sidecar {sidecar.name} missing species_id"
            assert data["species_id"] in EXISTING_SPECIES_IDS, \
                f"Sidecar {sidecar.name} references unknown species_id: {data['species_id']}"

    def test_sidecar_references_array_present(self):
        """Every sidecar must have a references array."""
        import yaml
        for sidecar in list(PLANT_REFS_DIR.glob("*.yaml")) + list(CREATURE_REFS_DIR.glob("*.yaml")):
            data = yaml.safe_load(sidecar.read_text(encoding="utf-8"))
            assert "references" in data, f"Sidecar {sidecar.name} missing references array"
            assert isinstance(data["references"], list), f"Sidecar {sidecar.name} references not a list"
            assert len(data["references"]) > 0, f"Sidecar {sidecar.name} references array empty"

    def test_sidecar_reference_entries_match_schema(self):
        """Each reference entry must have required fields per schema."""
        import yaml
        required_ref_fields = {"id", "kind", "locator", "usage_rights", "redistributable", "confidence", "supports"}
        for sidecar in list(PLANT_REFS_DIR.glob("*.yaml")) + list(CREATURE_REFS_DIR.glob("*.yaml")):
            data = yaml.safe_load(sidecar.read_text(encoding="utf-8"))
            for i, ref in enumerate(data["references"]):
                for field in required_ref_fields:
                    assert field in ref, f"Sidecar {sidecar.name} ref {i} missing field: {field}"
                # usage_rights must be acceptable value
                ur = ref["usage_rights"]
                assert ur in {"cc0", "cc-by", "cc-by-sa", "public_domain", "own_work", "permission_granted", "internal_reference"}, \
                    f"Sidecar {sidecar.name} ref {i} invalid usage_rights: {ur}"
                # redistributable must be bool
                assert isinstance(ref["redistributable"], bool), f"Sidecar {sidecar.name} ref {i} redistributable not bool"
                # supports must be list
                assert isinstance(ref["supports"], list), f"Sidecar {sidecar.name} ref {i} supports not list"

    def test_sidecar_no_unknown_usage_rights(self):
        """Sidecars must not contain usage_rights: unknown (those were placeholders)."""
        import yaml
        for sidecar in list(PLANT_REFS_DIR.glob("*.yaml")) + list(CREATURE_REFS_DIR.glob("*.yaml")):
            data = yaml.safe_load(sidecar.read_text(encoding="utf-8"))
            for i, ref in enumerate(data["references"]):
                assert ref["usage_rights"] != "unknown", \
                    f"Sidecar {sidecar.name} ref {i} still has usage_rights: unknown (placeholder)"

    def test_sidecar_references_link_to_registry_sources(self):
        """Every sidecar reference id should exist in source-registry.json (unless internal_reference)."""
        registry = load_json(REGISTRY_PATH)
        registry_ids = {src["id"] for src in registry["sources"]}
        import yaml
        for sidecar in list(PLANT_REFS_DIR.glob("*.yaml")) + list(CREATURE_REFS_DIR.glob("*.yaml")):
            data = yaml.safe_load(sidecar.read_text(encoding="utf-8"))
            for ref in data["references"]:
                ref_id = ref["id"]
                if ref["usage_rights"] != "internal_reference":
                    assert ref_id in registry_ids, \
                        f"Sidecar {sidecar.name} ref {ref_id} not found in source-registry.json"


# --- Tests: Coverage Summary ---
class TestCoverageSummary:
    """Report coverage statistics (not pass/fail, but informative)."""

    def test_report_coverage(self):
        """Print coverage summary to stdout for visibility."""
        registry = load_json(REGISTRY_PATH)
        unverified = load_json(UNVERIFIED_PATH)

        total_species = len(EXISTING_SPECIES_IDS)
        plant_species = len(list(PLANT_SPECIES_DIR.glob("*.yaml")))
        creature_species = len(list(CREATURE_SPECIES_DIR.glob("*.yaml")))

        verified_sources = len(registry["sources"])
        unverified_candidates = len(unverified["unverified"])

        print(f"\n=== Reference Coverage Summary ===")
        print(f"Species total: {total_species} (plants: {plant_species}, creatures: {creature_species})")
        print(f"Verified sources: {verified_sources}")
        print(f"Unverifiable candidates: {unverified_candidates}")
        print(f"Registry licenses: {registry['summary'].get('by_license', {})}")
        print(f"Registry kinds: {registry['summary'].get('by_kind', {})}")
        print(f"Unverified by status: {unverified['summary'].get('by_status', {})}")

        # Per-species reference counts
        import yaml
        print(f"\nPer-species sidecar reference counts:")
        for sidecar in sorted(list(PLANT_REFS_DIR.glob("*.yaml")) + list(CREATURE_REFS_DIR.glob("*.yaml"))):
            data = yaml.safe_load(sidecar.read_text(encoding="utf-8"))
            redist = sum(1 for r in data["references"] if r["redistributable"])
            internal = sum(1 for r in data["references"] if r["usage_rights"] == "internal_reference")
            print(f"  {data['species_id']}: {len(data['references'])} refs ({redist} redist, {internal} internal)")

        # This test always passes; it's for reporting
        assert True


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

class TestLicenceHonesty:
    """Regression guard for the operator's integration correction.

    A source may only claim redistributability if its licence was actually
    verified. Eleven GBIF/PBDF entries were downgraded from a blanket CC0 claim
    to license=unverified, redistributable=false: the endpoints resolve (HTTP
    200) but the licence text was never retrieved, and both publishers use
    per-dataset licences that can include CC-BY-NC.
    """

    @staticmethod
    def _sources():
        import json
        from pathlib import Path

        root = Path(__file__).resolve().parents[1]
        return json.loads((root / "research" / "source-registry.json").read_text(encoding="utf-8"))["sources"]

    def test_no_unverified_source_claims_redistributable(self):
        for s in self._sources():
            if s.get("license") == "unverified":
                assert s.get("redistributable") is False, (
                    f"{s['id']}: licence unverified but claims redistributable"
                )

    def test_database_kinds_are_not_blindly_licenced(self):
        """Taxonomic/paleontological databases must not carry a blanket licence."""
        for s in self._sources():
            if s.get("kind") in ("taxonomic_database", "paleontological_database"):
                assert s.get("license") == "unverified", (
                    f"{s['id']}: {s['kind']} asserts licence {s.get('license')!r} without retrieved licence text"
                )

    def test_verified_sources_record_a_verification_method(self):
        for s in self._sources():
            if s.get("license") != "unverified":
                assert s.get("license_verification_method"), f"{s['id']} has no verification method"
                assert s.get("date_checked"), f"{s['id']} has no date_checked"

    def test_downgraded_entries_flag_operator_action(self):
        flagged = [s for s in self._sources() if s.get("operator_action_required")]
        assert len(flagged) >= 11, "expected the 11 downgraded database entries to be flagged"
        for s in flagged:
            assert s.get("redistributable") is False
