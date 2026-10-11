"""Exit codes and the exception hierarchy for the Stalthos Asset Toolkit.

Every CLI failure must map to exactly one of these codes. Actionable, stable
exit codes matter more than a pretty UI (see docs/asset-toolkit.md in the
planning repository).
"""

from __future__ import annotations

# --- Exit codes -------------------------------------------------------------
EXIT_OK = 0
EXIT_FAILURE = 1          # generic, unclassified runtime failure
EXIT_USAGE = 2            # bad arguments / malformed definition file
EXIT_VALIDATION = 3       # asset produced but failed validation or acceptance
EXIT_MISSING_DEPENDENCY = 4  # required external tool absent (Blender, gltfpack, ...)
EXIT_NOT_IMPLEMENTED = 5  # command is a declared contract with no implementation yet
EXIT_EXTERNAL = 6         # an external tool ran and failed
EXIT_INTERRUPTED = 7      # timeout or operator interrupt


class ToolkitError(Exception):
    """Base for all toolkit failures. Carries an exit code and optional detail."""

    exit_code = EXIT_FAILURE

    def __init__(self, message: str, *, detail: object | None = None, hint: str | None = None):
        super().__init__(message)
        self.message = message
        self.detail = detail
        self.hint = hint

    def to_dict(self) -> dict:
        out = {"kind": type(self).__name__, "message": self.message}
        if self.detail is not None:
            out["detail"] = self.detail
        if self.hint:
            out["hint"] = self.hint
        return out


class UsageError(ToolkitError):
    exit_code = EXIT_USAGE


class ValidationFailed(ToolkitError):
    exit_code = EXIT_VALIDATION


class MissingDependency(ToolkitError):
    exit_code = EXIT_MISSING_DEPENDENCY

    def __init__(self, tool: str, *, hint: str | None = None):
        super().__init__(
            f"required external tool not available: {tool}",
            detail={"tool": tool},
            hint=hint or f"install {tool} or place it on PATH; see docs/toolkit-architecture.md",
        )
        self.tool = tool


class NotImplementedStage(ToolkitError):
    exit_code = EXIT_NOT_IMPLEMENTED


class ExternalToolFailed(ToolkitError):
    exit_code = EXIT_EXTERNAL

    def __init__(self, tool: str, returncode: int, *, stderr: str | None = None,
                 reason: str | None = None):
        """An external tool could not produce a usable result.

        `reason` describes *how* it failed when the process itself exited
        cleanly. Without it the message reads "external tool failed: blender
        (exit 0)" for a Blender run that exited 0 and merely reported ok:false
        — self-contradictory, and it blames the binary for a script-level
        failure. Reported by worker W15, fixed by worker zero 2026-10-10.

        Exit code is unchanged (6) either way: the §2.3 contract table has no
        distinct code for "tool ran, script refused", and inventing one would
        break the frozen contract to fix a message.
        """
        how = reason if reason else f"exit {returncode}"
        super().__init__(
            f"external tool failed: {tool} ({how})",
            detail={"tool": tool, "returncode": returncode, "reason": reason,
                    "stderr": (stderr or "")[-4000:]},
        )
        self.tool = tool
        self.returncode = returncode


class Interrupted(ToolkitError):
    exit_code = EXIT_INTERRUPTED
