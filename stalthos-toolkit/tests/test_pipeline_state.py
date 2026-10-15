from __future__ import annotations

import pytest

from stalthos_toolkit.core.contract import Context
from stalthos_toolkit.core.workspace import Workspace
from stalthos_toolkit.optimize.pipeline_state import allow_degraded

@pytest.fixture
def base_context(tmp_path):
    return Context(workspace=Workspace.open(tmp_path), options={})

def test_allow_degraded_default(base_context, monkeypatch):
    monkeypatch.delenv("STEALTHOS_ALLOW_DEGRADED", raising=False)

    enabled, source = allow_degraded(base_context)
    assert not enabled
    assert source == "default"

def test_allow_degraded_cli_option_true(base_context):
    base_context.options["allow_degraded"] = True

    enabled, source = allow_degraded(base_context)
    assert enabled
    assert source == "option"

def test_allow_degraded_cli_option_false(base_context, monkeypatch):
    # Option wins over environment
    monkeypatch.setenv("STEALTHOS_ALLOW_DEGRADED", "1")
    base_context.options["allow_degraded"] = False

    enabled, source = allow_degraded(base_context)
    assert not enabled
    assert source == "option"

@pytest.mark.parametrize("env_val", ["1", "true", "yes", "on", "  TRUE  "])
def test_allow_degraded_env_true(base_context, monkeypatch, env_val):
    monkeypatch.setenv("STEALTHOS_ALLOW_DEGRADED", env_val)

    enabled, source = allow_degraded(base_context)
    assert enabled
    assert source == "STEALTHOS_ALLOW_DEGRADED"

@pytest.mark.parametrize("env_val", ["0", "false", "no", "off", "invalid", ""])
def test_allow_degraded_env_false(base_context, monkeypatch, env_val):
    monkeypatch.setenv("STEALTHOS_ALLOW_DEGRADED", env_val)

    enabled, source = allow_degraded(base_context)
    assert not enabled
    assert source == "default"
