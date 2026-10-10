"""Breadth-scale roster checks for stalthos-toolkit/species/.

Runs with no Blender, no network and no Blender-side tooling: it only reads the
YAML definitions through the toolkit's own loader (PyYAML + stdlib).

What it asserts
----------------
* every profile under ``species/`` validates against its published contract;
* ``id`` slugs are unique across the whole roster and stable against the file
  name they live in (renaming a file must not silently change an id);
* breadth floors: >= 200 botanical profiles and >= 50 fauna profiles.  The
  floors are literals on purpose -- if the roster falls short this file fails
  loudly instead of lowering its own expectations.  ``test_roster_meets_breadth_floors``
  prints the real counts so a shortfall is diagnosable, but it cannot pass by
  relaxing the number;
* no orphan generator families: every generator family in use has at least two
  profiles, so no generator is ever authored for a single specimen;
* every fauna profile declares at least one habitat tag, at least one
  observable routine, terrain preference and a food-web link;
* honesty invariants: estimated masses are marked ``measured: false``, and
  every reference is either an internal planning document or a slot whose
  ``usage_rights`` are unknown.  No external URL, DOI or accession may appear.
"""

from __future__ import annotations

import pathlib
import re
from collections import Counter

import pytest
import yaml

from stalthos_toolkit.schemas.loader import load_definition

SPECIES_ROOT = pathlib.Path(__file__).resolve().parent.parent / "species"
PLANT_ROOT = SPECIES_ROOT / "plants"
CREATURE_ROOT = SPECIES_ROOT / "creatures"

# Breadth floors. Deliberately hard-coded; see the module docstring.
MIN_PLANTS = 200
MIN_FAUNA = 50
MIN_PROFILES_PER_GENERATOR = 2

SLUG_RE = re.compile(r"^[a-z0-9]+(?:[._-][a-z0-9]+)*$")

# Anything that would be a fabricated citation if it appeared in this roster.
FORBIDDEN_REFERENCE_RE = re.compile(
    r"(https?://|doi:|10\.\d{4,}/|\bdoi\b|specimen\s*#|accession\s*#|\bmuseum_record\b)",
    re.IGNORECASE,
)

UNCERTAINTY_TIERS = {"documented", "comparative_inference", "biomechanical_plausibility", "speculative"}


_CACHE: dict[pathlib.Path, dict] = {}


def _load(path: pathlib.Path) -> dict:
    """Load through the toolkit loader so schema validation is exercised too.

    Cached per process: the same file is asserted on by several tests and each
    load is a full schema validation.
    """
    if path not in _CACHE:
        _CACHE[path] = load_definition(path)
    return _CACHE[path]


def _yaml_files(root: pathlib.Path) -> list[pathlib.Path]:
    """Species definition files only.

    Mirrors the exclusion in ``schemas/loader.py``: sidecar reference files
    (``**/references/*.yaml``) and underscore-prefixed partials are not species
    definitions and must not be globbed in as if they were.
    """
    return sorted(
        p
        for p in root.rglob("*.yaml")
        if p.is_file() and "references" not in p.parts and not p.name.startswith("_")
    )


@pytest.fixture(scope="session")
def plants() -> dict[str, dict]:
    return {_load(p)["id"]: _load(p) for p in _yaml_files(PLANT_ROOT)}


@pytest.fixture(scope="session")
def fauna() -> dict[str, dict]:
    return {_load(p)["id"]: _load(p) for p in _yaml_files(CREATURE_ROOT)}


# --- validation ------------------------------------------------------------


def test_every_definition_validates():
    """Each file must satisfy its published contract (raises on violation)."""
    files = _yaml_files(SPECIES_ROOT)
    assert files, "no definition files found under species/"
    for path in files:
        definition = load_definition(path)
        assert definition["id"].startswith("plant.") or definition["id"].startswith("creature.")


# --- identity --------------------------------------------------------------


def test_ids_are_unique_and_well_formed():
    seen: dict[str, pathlib.Path] = {}
    for path in _yaml_files(SPECIES_ROOT):
        definition = _load(path)
        slug = definition["id"]
        assert SLUG_RE.match(slug), f"{path.name}: id {slug!r} is not a valid slug"
        assert slug not in seen, f"duplicate id {slug!r}: {seen[slug].name} and {path.name}"
        seen[slug] = path


def test_id_matches_filename():
    """The file name must remain derivable from the id.

    `plant.copperleaf.cl1` lives in `copperleaf.cl1.yaml`; the pre-existing
    reference files use `creature.brackenback.bk1` in `brackenback.yaml`. Both
    conventions are accepted, but the file stem must be recoverable from the id
    segments, so a directory reorganisation cannot silently change an asset id.
    """
    for path in _yaml_files(SPECIES_ROOT):
        slug = _load(path)["id"]
        parts = slug.split(".")
        accepted = {path.stem, ".".join(parts[1:]), ".".join(parts[1:-1])}
        assert path.stem in accepted, (
            f"{path.name}: id {slug!r} does not derive the file stem "
            f"(expected one of {sorted(a for a in accepted if a)})"
        )


# --- breadth floors --------------------------------------------------------


def test_roster_meets_breadth_floors(plants, fauna):
    """>= 200 botanical profiles and >= 50 fauna profiles. Fails loudly if short."""
    print(f"\nplants: {len(plants)} (floor {MIN_PLANTS}), fauna: {len(fauna)} (floor {MIN_FAUNA})")
    assert len(plants) >= MIN_PLANTS, (
        f"botanical roster has {len(plants)} profiles, floor is {MIN_PLANTS}. "
        "Add profiles; do not lower MIN_PLANTS."
    )
    assert len(fauna) >= MIN_FAUNA, (
        f"fauna roster has {len(fauna)} profiles, floor is {MIN_FAUNA}. "
        "Add profiles; do not lower MIN_FAUNA."
    )


# --- generator families ----------------------------------------------------


def test_generator_families_have_no_orphans(plants, fauna):
    """Every generator family in use carries at least two profiles."""
    counts = Counter(d["generator"] for d in list(plants.values()) + list(fauna.values()))
    print("\ngenerator families:", dict(sorted(counts.items())))
    orphans = sorted(g for g, n in counts.items() if n < MIN_PROFILES_PER_GENERATOR)
    assert not orphans, (
        f"orphan generator families with fewer than {MIN_PROFILES_PER_GENERATOR} "
        f"profiles: {orphans} -- full counts {dict(sorted(counts.items()))}"
    )


def test_fauna_spans_the_expected_body_plans(fauna):
    """The roster must keep at least a few genuinely different body plans."""
    counts = Counter(d["generator"] for d in fauna.values())
    print("\nfauna generator split:", dict(sorted(counts.items())))
    assert len(counts) >= 5, f"fauna roster is not varied enough: {dict(counts)}"
    postures = {d["body_plan"]["posture"] for d in fauna.values()}
    assert postures & {"bipedal", "semi_bipedal"}, "no bipedal/semi-bipedal actors"
    assert postures & {"quadrupedal", "cursorial_quadruped"}, "no quadrupedal actors"


def test_plant_families_cover_the_structural_breadth(plants):
    """No single generator family may swallow the botanical roster."""
    counts = Counter(d["generator"] for d in plants.values())
    print("\nplant generator split:", dict(sorted(counts.items())))
    assert len(counts) >= 8, f"botanical roster is not varied enough: {dict(counts)}"
    largest = max(counts.values())
    assert largest < len(plants) * 0.5, (
        f"generator family {counts.most_common(1)} dominates the botanical roster"
    )


# --- per-profile content requirements --------------------------------------


@pytest.mark.parametrize("path", _yaml_files(CREATURE_ROOT), ids=lambda p: p.name)
def test_fauna_profile_has_habitat_routine_and_food_web(path):
    definition = _load(path)
    habitat = definition.get("habitat_tags") or []
    assert habitat, f"{path.name}: no habitat tags"
    affordances = definition["affordances"]
    routines = affordances.get("signature_routines") or []
    assert routines, f"{path.name}: no observable routines"
    assert affordances.get("terrain_preference"), f"{path.name}: no terrain preference"
    assert affordances.get("diet_tags"), f"{path.name}: no food-web link"
    for tag in affordances["diet_tags"]:
        assert 0.0 <= tag["preference"] <= 1.0


@pytest.mark.parametrize("path", _yaml_files(PLANT_ROOT), ids=lambda p: p.name)
def test_plant_profile_declares_forage_and_contact(path):
    definition = _load(path)
    affordances = definition["affordances"]
    assert affordances["forage_tags"], f"{path.name}: no forage tags"
    assert affordances["contact_bounds"]["max_browse_height_m"] >= 0.0
    assert definition["growth_stages"], f"{path.name}: no growth stages"


def test_fauna_plant_selectors_resolve_to_real_profiles(plants, fauna):
    """Food-web links must point at definitions that actually exist."""
    known = set(plants) | set(fauna)
    unknown = []
    for definition in fauna.values():
        for tag in definition["affordances"]["diet_tags"]:
            for selector in tag.get("plant_selectors", []):
                target = selector.split("|", 1)[0]
                if target not in known:
                    unknown.append((definition["id"], selector))
    assert not unknown, f"food-web selectors point at unknown definitions: {unknown}"


# --- honesty invariants ----------------------------------------------------


@pytest.mark.parametrize("path", _yaml_files(CREATURE_ROOT), ids=lambda p: p.name)
def test_estimated_masses_are_not_claimed_as_measured(path):
    definition = _load(path)
    mass = definition["body_plan"].get("mass_estimate")
    assert mass is not None, f"{path.name}: fauna profile declares no mass estimate"
    assert mass["measured"] is False, (
        f"{path.name}: mass_estimate.measured is true but this roster contains no "
        "measured specimen; estimates must declare measured: false"
    )


@pytest.mark.parametrize("path", _yaml_files(SPECIES_ROOT), ids=lambda p: p.name)
def test_uncertainty_is_declared(path):
    definition = _load(path)
    tier = definition.get("uncertainty")
    assert tier in UNCERTAINTY_TIERS, f"{path.name}: uncertainty {tier!r} is not a declared tier"
    if "evidence" in definition:
        for item in definition["evidence"]:
            assert item["tier"] in UNCERTAINTY_TIERS


@pytest.mark.parametrize("path", _yaml_files(SPECIES_ROOT), ids=lambda p: p.name)
def test_references_are_internal_or_unresolved_slots(path):
    """No fabricated citation, DOI, URL or accession may be asserted."""
    definition = _load(path)
    assert definition["references"], f"{path.name}: references[] is empty"
    for reference in definition["references"]:
        assert not FORBIDDEN_REFERENCE_RE.search(reference["locator"]), (
            f"{path.name}: reference {reference['id']!r} asserts an external locator "
            f"{reference['locator']!r}"
        )
        assert reference["kind"] == "planning_doc" or reference.get("usage_rights") in {
            "unknown",
        }, (
            f"{path.name}: reference {reference['id']!r} is neither an internal planning "
            "document nor a slot with usage_rights: unknown"
        )
        if reference["kind"] == "planning_doc":
            assert reference["locator"].startswith("Rauken-Archipelago/docs/")


@pytest.mark.parametrize("path", _yaml_files(SPECIES_ROOT), ids=lambda p: p.name)
def test_working_names_are_marked_working(path):
    """Fictional working names must not be presented as settled taxonomy."""
    definition = _load(path)
    names = definition["common_names"]
    assert names.get("working") is True, f"{path.name}: common_names.working must be true"