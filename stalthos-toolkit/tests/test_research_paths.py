"""Offline path resolution regressions."""
import pytest
from stalthos_toolkit.commands.research_species import _reference_base_dirs, _normalize_reference

@pytest.fixture
def checkout(tmp_path, monkeypatch):
    monkeypatch.delenv("STEALTHOS_PROJECT_ROOT", raising=False)
    monkeypatch.delenv("STEALTHOS_PLAN_DOCS", raising=False)
    toolkit = tmp_path / "project/stalthos-toolkit"
    (toolkit / "src/stalthos_toolkit").mkdir(parents=True)
    (toolkit / "pyproject.toml").write_text("[project]\n")
    definition = toolkit / "species/plants/softbough.yaml"
    definition.parent.mkdir(parents=True)
    definition.write_text("id: plant.softbough\n")
    return toolkit, definition

def check(definition, locator):
    return _normalize_reference({"id":"local", "kind":"planning_doc", "locator":locator,
        "usage_rights":"internal_reference"}, 0, _reference_base_dirs(definition), "speculative")

def test_sibling_docs_from_toolkit_cwd(checkout, monkeypatch):
    toolkit, definition = checkout
    docs = toolkit.parent / "Rauken-Archipelago/docs"
    docs.mkdir(parents=True)
    (docs / "flora.md").write_text("planning")
    monkeypatch.chdir(toolkit)
    entry, warnings = check(definition, "Rauken-Archipelago/docs/flora.md")
    assert entry["resolved"] is True and entry["exists"] is True
    assert warnings == []

def test_cwd_independent_toolkit_relative_reference(checkout, tmp_path, monkeypatch):
    toolkit, definition = checkout
    (toolkit / "local.md").write_text("reference")
    monkeypatch.chdir(tmp_path)
    assert check(definition, "local.md")[0]["resolved"] is True

def test_explicit_project_for_detached_worktree(checkout, tmp_path, monkeypatch):
    _, definition = checkout
    project = tmp_path / "external"
    docs = project / "Rauken-Archipelago/docs"
    docs.mkdir(parents=True)
    (docs / "flora.md").write_text("planning")
    monkeypatch.setenv("STEALTHOS_PROJECT_ROOT", str(project))
    assert check(definition, "Rauken-Archipelago/docs/flora.md")[0]["resolved"] is True

@pytest.mark.parametrize("locator", ["flora.md", "Rauken-Archipelago/docs/flora.md"])
def test_docs_override(checkout, tmp_path, monkeypatch, locator):
    _, definition = checkout
    docs = tmp_path / "original/Rauken-Archipelago/docs"
    docs.mkdir(parents=True)
    (docs / "flora.md").write_text("planning")
    monkeypatch.setenv("STEALTHOS_PLAN_DOCS", str(docs))
    assert check(definition, locator)[0]["resolved"] is True

def test_local_priority_and_deduplication(checkout, monkeypatch, tmp_path):
    _, definition = checkout
    explicit = tmp_path / "explicit"
    monkeypatch.setenv("STEALTHOS_PROJECT_ROOT", str(explicit))
    bases = _reference_base_dirs(definition)
    assert bases[:2] == [definition.parent, definition.parent.parent]
    assert bases.index(explicit) > 1
    assert len(bases) == len(set(bases))

def test_missing_photo_remains_unresolved(checkout):
    _, definition = checkout
    entry, warnings = check(definition, "species/plants/reference/absent.jpg")
    assert entry["resolved"] is False and entry["exists"] is False
    assert len(warnings) == 1

def test_absolute_path(checkout, tmp_path):
    _, definition = checkout
    source = tmp_path / "absolute.md"
    source.write_text("source")
    assert check(definition, str(source))[0]["resolved"] is True
