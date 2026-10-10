"""CLI entrypoint: `stalthos <command> [options]`.

Guarantees:
  * exactly one JSON status object on stdout, always
  * human-readable chatter on stderr
  * exit code from core.errors, never a traceback on stdout
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Any

from . import __version__
from .core.contract import REGISTRY, Context, normalize_root_seed
from .core.errors import EXIT_USAGE, ToolkitError, UsageError
from .core.status import Status, emit, status_from_exception
from .core.workspace import Workspace

PROGRAM = "stalthos"

# name -> (summary, canonical pipeline stage)
DECLARED_COMMANDS: list[tuple[str, str, str]] = [
    ("research_species", "Collect and normalize species references into a research record", "1 source/research"),
    ("generate_plant", "Generate canonical plant source geometry from a species definition", "3 high-resolution generation"),
    ("generate_plant_variants", "Generate seeded morphological variants and growth stages", "5 variants/stages"),
    ("build_creature", "Build creature anatomical proxy geometry from a creature definition", "3 high-resolution generation"),
    ("rig_creature", "Create/refresh the authored rig and export neutral poses", "3 high-resolution generation"),
    ("bake_materials", "Bake multiscale material structure (albedo/roughness/normal/thickness)", "4 texture/material creation"),
    ("generate_lods", "Generate LOD chain, collision proxy and streaming cost metrics", "6 optimization"),
    ("render_turntable", "Render a deterministic turntable review sheet", "8 quality assurance"),
    ("render_growth_stages", "Render a growth/life stage comparison sheet", "8 quality assurance"),
    ("validate_asset", "Run geometry/material/rig/provenance checks against acceptance rules", "8 quality assurance"),
    ("benchmark_asset", "Measure geometry, texture, memory and frame-time cost", "8 quality assurance"),
    ("publish_asset", "Promote validated artifacts to runtime/ with a reproducible receipt", "9 publish"),
]


def _load_registered() -> None:
    """Import command modules so decorated implementations register themselves."""
    from .commands import (  # noqa: F401
        bake_materials,
        benchmark_asset,
        build_creature,
        generate_lods,
        generate_plant,
        generate_plant_variants,
        publish_asset,
        render_growth_stages,
        render_turntable,
        research_species,
        rig_creature,
        validate_asset,
    )


def register_declared() -> None:
    for name, summary, stage in DECLARED_COMMANDS:
        if name not in REGISTRY.names():
            REGISTRY.add(name, summary, None, stage=stage)


def _global_options(parser: argparse.ArgumentParser) -> argparse.ArgumentParser:
    """Global options. Attached to the root parser AND every subcommand, so both
    `stalthos -w var generate_plant ...` and `stalthos generate_plant -w var ...`
    work. Agents should not have to remember argument order."""
    parser.add_argument("--workspace", "-w", default=argparse.SUPPRESS,
                        help="workspace root (default: $STEALTHOS_HOME or ./var)")
    parser.add_argument("--seed", "-s", default=argparse.SUPPRESS,
                        help="root seed (int or stable string label)")
    parser.add_argument("--quiet", "-q", action="store_true", default=argparse.SUPPRESS,
                        help="suppress stderr logs")
    return parser


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog=PROGRAM,
        description="Stalthos Asset Toolkit: deterministic, reproducible asset production.",
    )
    parser.add_argument("--version", action="version", version=f"{PROGRAM} {__version__}")
    _global_options(parser)
    # Suppressed defaults on the root parser: argparse only writes an attribute
    # when the flag was actually seen, so a value given after the subcommand wins.
    parser.set_defaults(workspace=None, seed=None, quiet=False)

    common = argparse.ArgumentParser(add_help=False)
    _global_options(common)
    # NOTE: deliberately no set_defaults() on the subparser parent. argparse copies a
    # subparser namespace over the main one, so defaults here would clobber a global
    # option that was given BEFORE the subcommand. SUPPRESS means the subparser only
    # writes the attributes it actually saw.

    subparsers = parser.add_subparsers(dest="command", metavar="<command>")

    subparsers.add_parser("list", help="list declared commands and implementation status", parents=[common])
    subparsers.add_parser("doctor", help="report external tool availability", parents=[common])

    for name, summary, _stage in DECLARED_COMMANDS:
        sub = subparsers.add_parser(name, help=summary, description=summary, parents=[common])
        sub.add_argument("--definition", "-d", default=None, help="species/creature YAML or JSON definition")
        sub.add_argument("--asset-id", default=None, help="override the generated asset id")
        sub.add_argument("--stage", default=None, help="growth or life stage selector")
        sub.add_argument("--variant", default=None, help="named morphology variant")
        sub.add_argument("--count", type=int, default=None, help="number of variants (batch commands)")
        sub.add_argument("--out", default=None, help="explicit output path override")
        sub.add_argument("--blender", default=None, help="path to the Blender executable")
        sub.add_argument("--timeout", type=float, default=None, help="external tool timeout in seconds")
        sub.add_argument("--strict", action="store_true", help="treat warnings as failures")
        sub.add_argument("--dry-run", action="store_true", help="plan the work without writing artifacts")

    return parser


def _options_from_args(args: argparse.Namespace) -> dict[str, Any]:
    keys = ("definition", "asset_id", "stage", "variant", "count", "out",
            "blender", "timeout", "strict", "dry_run")
    return {k: getattr(args, k) for k in keys if hasattr(args, k)}


def _cmd_list() -> int:
    register_declared()
    try:
        _load_registered()
    except Exception as exc:  # a broken stage module must not hide the contract
        print(f"warning: some command modules failed to import: {exc}", file=sys.stderr)
    register_declared()
    rows = REGISTRY.table()
    width = max(len(r["name"]) for r in rows)
    for row in rows:
        mark = "ok " if row["implemented"] else "--- "
        print(f"{mark}{row['name']:<{width}}  [{row['stage'] or '-'}]  {row['summary']}")
    return 0


def _cmd_doctor(workspace: Workspace) -> int:
    status = Status(command="doctor")
    tools: dict[str, Any] = {}
    for tool, probe in (("blender", None), ("gltfpack", "gltfpack"), ("meshoptimizer", "meshoptimizer"),
                        ("compressonator", "compressonator"), ("ktx", "ktx"), ("git", "git"),
                        ("magick", "magick"), ("ffmpeg", "ffmpeg")):
        if tool == "blender":
            from .core.workspace import find_blender

            try:
                tools[tool] = {"available": True, "path": find_blender()}
            except ToolkitError as exc:
                tools[tool] = {"available": False, "error": exc.message}
        else:
            import shutil

            path = shutil.which(probe)
            tools[tool] = {"available": bool(path), "path": path}
    status.metrics["tools"] = tools
    status.metrics["workspace"] = str(workspace.root)
    status.metrics["python"] = sys.version.split()[0]
    status.ok = True
    emit(status)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if not args.command:
        parser.print_help(sys.stderr)
        return EXIT_USAGE

    if args.command == "list":
        return _cmd_list()

    seed = normalize_root_seed(getattr(args, "seed", None))
    try:
        workspace = Workspace.open(getattr(args, "workspace", None))
    except Exception as exc:
        status = status_from_exception(args.command, exc, seed=seed)
        emit(status)
        return status.exit_code

    if args.command == "doctor":
        return _cmd_doctor(workspace)

    register_declared()
    try:
        _load_registered()
    except Exception as exc:
        status = status_from_exception(
            args.command,
            UsageError(f"command module import failed: {exc}"),
            seed=seed,
        )
        emit(status)
        return status.exit_code
    register_declared()

    ctx = Context(workspace=workspace, seed=seed, options=_options_from_args(args))
    started = time.time()
    try:
        spec = REGISTRY.get(args.command)
        status = spec.invoke(ctx)
    except ToolkitError as exc:
        status = status_from_exception(args.command, exc, seed=seed)
    except KeyboardInterrupt:
        status = status_from_exception(args.command, KeyboardInterrupt(), seed=seed)
    except Exception as exc:  # never leak a traceback on stdout
        status = status_from_exception(args.command, exc, seed=seed)
        if not args.quiet:
            import traceback

            traceback.print_exc(file=sys.stderr)
    status.duration_s = time.time() - started
    if getattr(args, "strict", False) and status.ok and status.warnings:
        status.ok = False
        status.exit_code = 3
        status.errors.append({"kind": "StrictMode", "message": f"{len(status.warnings)} warning(s) under --strict"})
    emit(status)
    return status.exit_code


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
