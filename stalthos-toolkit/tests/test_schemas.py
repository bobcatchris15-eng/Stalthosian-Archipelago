"""Schema/definition tests. These must pass WITHOUT Blender.

Covers:
  * the baseline contracts (plus any registered later, e.g. terrain) load
    and are themselves well-formed
  * every checked-in definition satisfies its contract
  * the hand-rolled validator's error reporting (JSON pointers, keyword set)
  * the loader wires validation into Context.load_definition (exit 2)
  * research_species produces a schema-valid, offline, deterministic record
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from stalthos_toolkit.cli import main
from stalthos_toolkit.core.contract import Context
from stalthos_toolkit.core.errors import EXIT_OK, EXIT_USAGE, UsageError
from stalthos_toolkit.core.workspace import Workspace
from stalthos_toolkit.schemas import (
    CONTRACT_FILES,
    SchemaError,
    UnsupportedSchema,
    contract_names,
    load_contract,
    load_definition,
    load_definition_file_list,
    require_valid,
    schema_for_definition,
    validate_against,
)
from stalthos_toolkit.schemas.validator import SchemaValidator, validate

REPO_ROOT = Path(__file__).resolve().parents[1]


# --- contracts --------------------------------------------------------------
def test_baseline_contracts_exist_and_every_contract_declares_2020_12():
    # The original five must all still exist; later stages may register more
    # (e.g. terrain-definition), so only the baseline is pinned exactly.
    assert {
        "species-definition", "creature-definition", "asset-manifest",
        "status-envelope", "research-record",
    } <= set(CONTRACT_FILES)
    assert set(contract_names()) == set(CONTRACT_FILES)
    for name in contract_names():
        schema = load_contract(name)
        assert schema["$schema"] == "https://json-schema.org/draft/2020-12/schema"
        assert schema["type"] == "object"
        assert schema["required"], f"{name} constrains nothing"


def test_contracts_use_only_supported_keywords():
    """An unenforced constraint is worse than none: every keyword must be known."""
    from stalthos_toolkit.schemas.validator import (  # noqa: PLC2701 - internal by design
        _IGNORED_ANNOTATIONS,
        _SUPPORTED,
    )

    # keys whose value is a *mapping of name -> schema*
    mapping_keywords = ("properties", "patternProperties", "$defs")
    # keys whose value is a schema, or a list of schemas
    schema_keywords = ("additionalProperties", "propertyNames", "items", "not", "if", "then", "else")
    list_keywords = ("allOf", "anyOf", "oneOf", "prefixItems")

    def schema_nodes(node):
        if isinstance(node, bool):
            return
        if not isinstance(node, dict):
            return
        yield node
        for keyword in mapping_keywords:
            child = node.get(keyword)
            if isinstance(child, dict):
                for value in child.values():
                    yield from schema_nodes(value)
        for keyword in schema_keywords:
            if keyword in node:
                yield from schema_nodes(node[keyword])
        for keyword in list_keywords:
            child = node.get(keyword)
            if isinstance(child, list):
                for value in child:
                    yield from schema_nodes(value)

    for name in contract_names():
        for node in schema_nodes(load_contract(name)):
            if not isinstance(node, dict):
                continue
            unknown = set(node) - _SUPPORTED - _IGNORED_ANNOTATIONS
            assert not unknown, f"{name} uses keywords the validator cannot enforce: {sorted(unknown)}"


def test_unknown_contract_name_raises():
    with pytest.raises(SchemaError):
        load_contract("no-such-contract")


# --- definitions ------------------------------------------------------------
# Sidecar reference files (species/**/references/*.yaml) are a different document
# type -- they carry verified citation metadata and are merged into profiles by
# the procedure in docs/reference-policy.md -- so they are not definitions.
def _definition_files():
    return sorted(p for p in (REPO_ROOT / "species").rglob("*.yaml")
                  if "references" not in p.parts)


@pytest.mark.parametrize("path", _definition_files(), ids=lambda p: p.name)
def test_every_checked_in_definition_validates(path):
    definition = load_definition(path)
    assert definition["id"]
    assert validate_against(schema_for_definition(definition), definition) == []


def test_six_reference_plants_are_present():
    """docs/flora.md initial production set: Splitcrown, Softbough, Jointreed,
    Royal frond, Basketfern, bracket fungus.

    The roster has since grown past the initial six (see tests/test_roster.py),
    so this asserts the six are still PRESENT, not that they are the only ones.
    """
    names = {load_definition(p)["common_names"]["primary"].lower() for p in
             load_definition_file_list(REPO_ROOT / "species" / "plants")}
    assert {"splitcrown", "softbough", "jointreed", "royal frond",
            "basketfern", "bracketpolypore"} <= names


def test_creature_set_covers_flagship_plus_lineage_stubs():
    creatures = [load_definition(p) for p in load_definition_file_list(REPO_ROOT / "species" / "creatures")]
    ids = {c["id"] for c in creatures}
    assert "creature.smalltrotter.st1" in ids
    assert len(creatures) >= 3
    generators = {c["generator"] for c in creatures}
    assert len(generators) >= 3, "lineage stubs must exercise different generators"
    # every creature declares the contract-specific blocks
    for creature in creatures:
        assert creature["body_plan"]["skeleton"]
        assert creature["rig"]["ik"]
        assert creature["animation_contacts"]
        assert creature["material_masks"]
        assert creature["integument"]["scale_zones"]


def test_smalltrotter_marks_invented_anatomy_speculative():
    st = load_definition(REPO_ROOT / "species" / "creatures" / "smalltrotter.yaml")
    assert st["uncertainty"] == "speculative"
    for zone in st["integument"]["bristle_zones"]:
        assert zone["uncertainty"] == "speculative"
    for zone in st["integument"]["osteoderm_zones"]:
        assert zone["uncertainty"] == "speculative"
    assert st["body_plan"]["mass_estimate"]["measured"] is False


def test_definitions_never_fabricate_citations():
    """No DOI/URL may be asserted until a real reference has been collected."""
    for path in _definition_files():
        for reference in load_definition(path)["references"]:
            assert reference["kind"] in ("planning_doc", "local_image", "local_model", "local_scan")
            assert not reference["locator"].startswith(("http://", "https://", "doi:"))
            assert not reference["locator"].startswith("10.")


# --- loader / Context wiring ------------------------------------------------
def test_loader_rejects_invalid_definition_with_json_pointer(tmp_path):
    bad = tmp_path / "bad.yaml"
    bad.write_text("schema: species-definition\nkind: plant\nid: Bad Id!\n", encoding="utf-8")
    with pytest.raises(UsageError) as excinfo:
        load_definition(bad)
    assert excinfo.value.exit_code == EXIT_USAGE
    message = str(excinfo.value)
    assert "/id" in message


def test_context_load_definition_validates(tmp_path, monkeypatch):
    """The funnel every command uses must reject a malformed definition."""
    workspace = Workspace.open(tmp_path / "var")
    good = tmp_path / "good.yaml"
    good.write_text("schema: species-definition\nkind: plant\nid: x\n", encoding="utf-8")
    ctx = Context(workspace=workspace)
    with pytest.raises(UsageError):
        ctx.load_definition(str(good))

    real = REPO_ROOT / "species" / "plants" / "softbough.yaml"
    ctx2 = Context(workspace=workspace)
    assert ctx2.load_definition(str(real))["id"] == "plant.softbough.sh1"
    assert ctx2.definition_sha256


def test_loader_can_skip_validation_for_tooling(tmp_path):
    path = tmp_path / "draft.json"
    path.write_text(json.dumps({"schema": "species-definition", "kind": "plant", "id": "x"}), encoding="utf-8")
    assert load_definition(path, validate=False)["id"] == "x"
    with pytest.raises(UsageError):
        load_definition(path)


def test_schema_for_definition_dispatch():
    assert schema_for_definition({"kind": "creature"}) == "creature-definition"
    assert schema_for_definition({"kind": "plant"}) == "species-definition"
    assert schema_for_definition({"schema": "research-record"}) == "research-record"
    with pytest.raises(UsageError):
        schema_for_definition({"schema": "bogus"})


# --- the validator itself ---------------------------------------------------
def _schema(**kwargs):
    return kwargs


def test_validator_reports_pointer_for_nested_value():
    issues = validate({"a": {"b": [1, "two", 3]}},
                      {"properties": {"a": {"properties": {"b": {"items": {"type": "integer"}}}}}})
    assert [(i.pointer, i.keyword) for i in issues] == [("/a/b/1", "type")]


def test_validator_required_and_additional_properties():
    schema = {"required": ["x"], "additionalProperties": False, "properties": {"x": {"type": "string"}}}
    issues = validate({"x": 1, "y": 2}, schema)
    keywords = {(i.pointer, i.keyword) for i in issues}
    assert ("/x", "type") in keywords
    assert ("/y", "additionalProperties") in keywords
    assert validate({"x": "ok"}, schema) == []


def test_validator_bounds_pattern_and_enum():
    schema = {"minimum": 1, "maximum": 5, "pattern": "^a+$"}
    assert [i.keyword for i in validate(0, schema)] == ["minimum"]
    assert [i.keyword for i in validate(9, schema)] == ["maximum"]
    assert [i.keyword for i in validate("b", schema)] == ["pattern"]
    assert [i.keyword for i in validate(3, {"enum": [1, 2]})] == ["enum"]
    assert validate(3, {"const": 3}) == []


def test_validator_unique_items_and_min_items():
    assert [i.keyword for i in validate([1, 1], {"uniqueItems": True})] == ["uniqueItems"]
    assert [i.keyword for i in validate([], {"minItems": 1})] == ["minItems"]
    assert [i.keyword for i in validate({"a": 1}, {"minProperties": 2})] == ["minProperties"]


def test_validator_ref_and_combinators():
    schema = {
        "$defs": {"positive": {"type": "number", "exclusiveMinimum": 0}},
        "properties": {"count": {"$ref": "#/$defs/positive"}},
    }
    assert validate({"count": 0}, schema)[0].pointer == "/count"
    assert validate({"count": 1}, schema) == []
    both = {"anyOf": [{"type": "string"}, {"type": "integer"}]}
    assert validate("x", both) == []
    assert validate(1.5, both)[0].keyword == "anyOf"
    one = {"oneOf": [{"type": "integer"}, {"type": "number"}]}
    assert [i.keyword for i in validate(1, one)] == ["oneOf"]


def test_validator_escapes_pointer_tokens():
    issues = validate({"a/b": 1}, {"properties": {"a/b": {"type": "string"}}})
    assert issues[0].pointer == "/a~1b"


def test_validator_refuses_unsupported_keyword():
    with pytest.raises(UnsupportedSchema):
        validate({}, {"if": {"type": "object"}, "dependentSchemas": {}})
    with pytest.raises(UnsupportedSchema):
        validate({}, {"$ref": "https://example.com/other.json"})


# --- research_species (stage 1) --------------------------------------------
def test_research_species_writes_valid_record_offline(tmp_path, monkeypatch):
    monkeypatch.setenv("STEALTHOS_PLAN_DOCS", str(REPO_ROOT.parent))
    workspace = tmp_path / "var"
    exit_code = main([
        "-w", str(workspace), "-s", "42", "research_species",
        "-d", str(REPO_ROOT / "species" / "plants" / "softbough.yaml"),
    ])
    assert exit_code == EXIT_OK

    record_path = workspace / "reports" / "research" / "plant.softbough.sh1.json"
    assert record_path.exists()
    record = json.loads(record_path.read_text(encoding="utf-8"))
    assert validate_against("research-record", record) == []
    assert record["tool"]["offline"] is True
    assert record["references"], "references must be normalized, not dropped"

    # a reference whose rights were not established -> unverified tier + warning
    unverified = [r for r in record["references"] if r["usage_rights"] == "unknown"]
    assert unverified, "the fixture definition intentionally has an uncleared reference"
    assert all(r["uncertainty_tier"] == "unverified" for r in unverified)
    assert record["usage_rights_summary"]["unverified"] == len(unverified)
    assert any("usage rights" in w for w in record["warnings"])
    assert record["uncertainty"] == "unverified"


def test_research_species_is_deterministic_apart_from_timestamp(tmp_path, monkeypatch):
    monkeypatch.setenv("STEALTHOS_PLAN_DOCS", str(REPO_ROOT.parent))
    definition = str(REPO_ROOT / "species" / "creatures" / "smalltrotter.yaml")
    records = []
    for name in ("a", "b"):
        workspace = tmp_path / name
        assert main(["-w", str(workspace), "-s", "7", "research_species", "-d", definition]) == EXIT_OK
        record = json.loads((workspace / "reports" / "research" / "creature.smalltrotter.st1.json")
                            .read_text(encoding="utf-8"))
        record.pop("created_at")
        record.pop("definition_path")
        records.append(record)
    assert records[0] == records[1]


def test_research_species_status_envelope_matches_contract(capsys):
    exit_code = main([
        "-w", str(Path.cwd() / "var"), "-s", "1", "research_species",
        "-d", str(REPO_ROOT / "species" / "plants" / "jointreed.yaml"),
    ])
    assert exit_code == EXIT_OK
    envelope = json.loads(capsys.readouterr().out)
    assert validate_against("status-envelope", envelope) == []
    assert envelope["command"] == "research_species"
    assert envelope["artifacts"] and all("sha256" in a for a in envelope["artifacts"])


def test_research_species_rejects_definition_without_references(tmp_path):
    definition = tmp_path / "no_refs.yaml"
    definition.write_text("schema: species-definition\nkind: plant\nid: plant.x\n", encoding="utf-8")
    assert main(["-w", str(tmp_path / "var"), "research_species", "-d", str(definition)]) == EXIT_USAGE


def test_require_valid_mentions_the_contract(tmp_path):
    with pytest.raises(UsageError) as excinfo:
        require_valid("species-definition", {"id": "nope"})
    assert "species-definition.schema.json" in (excinfo.value.hint or "")


def test_load_definition_file_list(tmp_path):
    (tmp_path / "valid.yaml").touch()
    (tmp_path / "also_valid.yml").touch()
    (tmp_path / "_ignored.yaml").touch()

    refs_dir = tmp_path / "references"
    refs_dir.mkdir()
    (refs_dir / "ref.yaml").touch()

    subdir = tmp_path / "subdir"
    subdir.mkdir()
    (subdir / "sub_valid.yaml").touch()
    (subdir / "_sub_ignored.yaml").touch()

    sub_refs = subdir / "references"
    sub_refs.mkdir()
    (sub_refs / "sub_ref.yaml").touch()

    json_file = tmp_path / "data.json"
    json_file.touch()

    # Default pattern: **/*.y*ml
    found = load_definition_file_list(tmp_path)
    assert found == sorted([
        tmp_path / "also_valid.yml",
        tmp_path / "subdir" / "sub_valid.yaml",
        tmp_path / "valid.yaml",
    ])

    # Custom pattern matching JSON
    found_json = load_definition_file_list(tmp_path, pattern="*.json")
    assert found_json == [json_file]
