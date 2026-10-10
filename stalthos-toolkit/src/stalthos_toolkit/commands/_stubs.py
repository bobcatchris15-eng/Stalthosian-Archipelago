"""Fallback implementation for declared-but-unimplemented commands.

Raising a typed error (exit 5) is the honest behaviour: the contract exists,
the stage does not yet. Never emit fake success.
"""

from __future__ import annotations

from ..core.contract import Context
from ..core.status import Status


def not_implemented(ctx: Context, name: str, summary: str) -> Status:
    status = ctx.new_status(name)
    status.ok = False
    status.exit_code = 5
    status.warnings.append(f"{name}: declared in the contract, not implemented yet")
    status.errors.append({
        "kind": "NotImplementedStage",
        "message": f"{name} is not implemented",
        "hint": "see docs/toolkit-pipeline.md for the owning stage and status",
    })
    return status
