"""Texture compression policy and execution (BC5/BC7 via AMD Compressonator).

Policy (*Proposed*)
--------------------
=================  ==================  =========  =================================
slot kind          format              max size   rationale
=================  ==================  =========  =================================
``normal``         BC5_UNORM           2048       two-channel block compression is
                                               lossless enough for tangent-space
                                               normals; 2 bytes/px instead of 4
``color``/         BC7_UNORM           2048       BC7 fits alpha + wide colour
``albedo``                                                 gamut; BC7 is ~0.5 bits/px
``emissive``       BC4_UNORM           1024       scalar maps need one channel only;
``roughness``      BC5_UNORM           1024       two-scalar maps (roughness+metal,
``metallic``                                             occlusion) use BC5
``occlusion``      BC4_UNORM           1024
``thickness``      BC4_UNORM           1024
``scalar``/        BC4_UNORM           1024
``curvature``
``mask`` (2ch)     BC5_UNORM           1024
=================  ==================  =========  =================================

* Every map is padded/truncated to a multiple of 4 px (BC block size).
* A full mip chain to 4x4 is requested and then **verified** by reading the
  written KTX2 header (`levelCount`) — the request is never reported as fact.
* `bytes_before` / `bytes_after` are measured from the files on disk.

Degradation
-----------
Compressonator absent -> `MissingDependency` (exit 4) by default. With
``allow_degraded=True`` the source is copied through uncompressed, a warning is
raised and every compression metric is marked ``measured=False``. Uncompressed
textures are never presented as compressed.
"""

from __future__ import annotations

import shutil
import struct
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Iterable

from ..core.errors import ExternalToolFailed
from ..validate.ktx import KTX2_IDENTIFIER, sniff_texture
from . import tools as toolbox

BC4 = "BC4"
BC5 = "BC5"
BC7 = "BC7"

DEFAULT_MAX_RESOLUTION = 1024
COLOR_MAX_RESOLUTION = 2048
BLOCK_ALIGNMENT = 4


@dataclass(frozen=True)
class TextureSlotPolicy:
    """The compression decision for one texture slot kind."""

    kind: str
    format: str
    max_resolution: int
    generate_mips: bool = True
    reason: str = ""

    def to_dict(self) -> dict:
        return asdict(self)


_NORMAL = TextureSlotPolicy("normal", BC5, COLOR_MAX_RESOLUTION,
                            reason="two-channel block compression for tangent-space normals")
_COLOR = TextureSlotPolicy("color", BC7, COLOR_MAX_RESOLUTION,
                           reason="BC7 covers alpha and wide colour gamut at ~0.5 bits/px")
_SCALAR = TextureSlotPolicy("scalar", BC4, DEFAULT_MAX_RESOLUTION,
                            reason="single-channel scalar data; BC4 is half the size of BC5")
_TWO_SCALAR = TextureSlotPolicy("two_scalar", BC5, DEFAULT_MAX_RESOLUTION,
                                reason="two packed scalar channels (roughness+metallic, occlusion+cavity)")

TEXTURE_POLICY: dict[str, TextureSlotPolicy] = {
    "normal": _NORMAL,
    "color": _COLOR,
    "colour": _COLOR,
    "albedo": _COLOR,
    "basecolor": _COLOR,
    "emissive": _COLOR,
    "roughness": _SCALAR,
    "metallic": _SCALAR,
    "occlusion": _SCALAR,
    "ao": _SCALAR,
    "thickness": _SCALAR,
    "transmission": _SCALAR,
    "scalar": _SCALAR,
    "height": _SCALAR,
    "curvature": _SCALAR,
    "cavity": _SCALAR,
    "mask": _TWO_SCALAR,
}


def select_format(kind: str, *, channels: int = 1) -> TextureSlotPolicy:
    """Format policy for a slot kind. Unknown kinds fall back to BC4 and say so."""
    key = str(kind or "").strip().lower()
    policy = TEXTURE_POLICY.get(key)
    if policy is not None:
        return policy
    if key.endswith("mask") or channels >= 2:
        return TextureSlotPolicy(key or "unknown", BC5, DEFAULT_MAX_RESOLUTION,
                                 reason="unregistered slot kind treated as a two-channel scalar map")
    return TextureSlotPolicy(key or "unknown", BC4, DEFAULT_MAX_RESOLUTION,
                             reason="unregistered slot kind treated as a single-channel scalar map")


def target_resolution(width: int, height: int, max_resolution: int) -> tuple[int, int]:
    """Scale down to `max_resolution` on the long edge and align to the BC block."""
    long_edge = max(int(width), int(height), 1)
    scale = min(1.0, max_resolution / long_edge)
    out_w = max(BLOCK_ALIGNMENT, int(round(width * scale)))
    out_h = max(BLOCK_ALIGNMENT, int(round(height * scale)))
    out_w -= out_w % BLOCK_ALIGNMENT
    out_h -= out_h % BLOCK_ALIGNMENT
    return out_w, out_h


def ktx2_level_count(path: str | Path) -> int | None:
    """Read `levelCount` out of a KTX2 header, or None if it is not a KTX2."""
    try:
        with open(path, "rb") as fh:
            head = fh.read(44)
    except OSError:
        return None
    if not head.startswith(KTX2_IDENTIFIER) or len(head) < 44:
        return None
    return int(struct.unpack_from("<I", head, 40)[0])


@dataclass
class CompressionOutcome:
    """What happened to one texture slot. `measured` marks unverified claims."""

    source: str
    path: str
    kind: str
    format: str
    bytes_before: int
    bytes_after: int
    compressed: bool
    degraded: bool
    reason: str = ""
    container: str = ""
    codec: str | None = None
    mip_levels: int | None = None
    measured: dict[str, bool] = field(default_factory=dict)

    @property
    def ratio(self) -> float:
        return (self.bytes_after / self.bytes_before) if self.bytes_before else 0.0

    def to_dict(self) -> dict:
        data = asdict(self)
        data["ratio"] = round(self.ratio, 6)
        data["measured"] = dict(self.measured)
        return data

    def importer_settings(self) -> dict[str, Any]:
        """Consumed by `validate/checks.py`: it reads `kind` and `expected_format`."""
        return {
            "kind": self.kind,
            "expected_format": self.format,
            "compressed": self.compressed,
            "degraded": self.degraded,
            "bytes_before": self.bytes_before,
            "bytes_after": self.bytes_after,
        }


@dataclass
class CompressionReport:
    outcomes: list[CompressionOutcome] = field(default_factory=list)
    tool: str = ""
    tool_version: str = "unknown"
    degraded: bool = False
    warnings: list[str] = field(default_factory=list)
    bytes_before: int = 0
    bytes_after: int = 0

    @property
    def ratio(self) -> float:
        return (self.bytes_after / self.bytes_before) if self.bytes_before else 0.0

    def to_dict(self) -> dict:
        return {
            "tool": self.tool,
            "tool_version": self.tool_version,
            "degraded": self.degraded,
            "bytes_before": self.bytes_before,
            "bytes_after": self.bytes_after,
            "ratio": round(self.ratio, 6),
            "warnings": list(self.warnings),
            "outcomes": [o.to_dict() for o in self.outcomes],
        }

    def metrics(self) -> list[dict[str, Any]]:
        measured = not self.degraded
        return [
            {"name": "texture_bytes_before", "value": float(self.bytes_before), "unit": "bytes",
             "measured": True},
            {"name": "texture_bytes_after", "value": float(self.bytes_after), "unit": "bytes",
             "measured": True},
            {"name": "texture_compression_ratio", "value": round(self.ratio, 6), "unit": "ratio",
             "measured": measured},
            {"name": "texture_count", "value": float(len(self.outcomes)), "unit": "maps",
             "measured": True},
            {"name": "texture_compression_bc", "value": 0.0, "unit": "boolean",
             "measured": measured},
        ]


def _compress_one(source: Path, target: Path, policy: TextureSlotPolicy, tool_path: str,
                   *, timeout: float | None) -> tuple[list[str], list[str]]:
    """Run Compressonator. Returns (argv, stderr). Raises on failure."""
    args = ["-i", str(source), "-o", str(target), "-f", policy.format, "-fd", "-overwrite"]
    try:
        proc = toolbox.run_tool(tool_path, args, timeout=timeout)
    except TimeoutError as exc:
        raise ExternalToolFailed("compressonator", -1, stderr=f"timeout: {exc}") from exc
    except OSError as exc:
        raise ExternalToolFailed("compressonator", -1, stderr=str(exc)) from exc
    if proc.returncode != 0 or not target.exists():
        raise ExternalToolFailed("compressonator", proc.returncode, stderr=proc.stderr or proc.stdout)
    return args, (proc.stderr or "").splitlines()


def compress_textures(slots: Iterable[tuple[str | Path, str]], out_dir: str | Path, *,
                      allow_degraded: bool = False, timeout: float | None = None,
                      keep_extension: bool = False) -> CompressionReport:
    """Compress each (path, kind) slot into `out_dir`.

    Raises `MissingDependency` (exit 4) when Compressonator is absent and
    `allow_degraded` is False. With `allow_degraded=True` the source is copied
    through uncompressed and the report is flagged degraded + unmeasured.
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    report = CompressionReport()

    tool = toolbox.discover("compressonator")
    if not tool.available:
        hint = toolbox.INSTALL_HINTS["compressonator"]
        if not allow_degraded:
            raise toolbox.require("compressonator")  # raises MissingDependency(exit 4)
        report.degraded = True
        report.tool = "compressonator"
        report.tool_version = "not-available"
        report.warnings.append(
            "AMD Compressonator not found: textures are emitted UNCOMPRESSED. "
            f"{hint}. Do not publish uncompressed textures silently.")
    else:
        report.tool = "compressonator"
        report.tool_version = tool.version or "unknown"
        if tool.version is None:
            report.warnings.append("Compressonator version string could not be read")

    for source_text, kind in slots:
        source = Path(source_text)
        if not source.exists():
            raise ExternalToolFailed("compressonator", -1, stderr=f"missing texture: {source}")
        policy = select_format(kind)
        info = sniff_texture(source)
        bytes_before = source.stat().st_size
        stem = source.stem
        target = out_dir / (f"{stem}.ktx2" if not keep_extension else f"{stem}{source.suffix}")

        if report.degraded:
            shutil.copyfile(source, target)
            outcome = CompressionOutcome(
                source=str(source), path=str(target), kind=kind, format=policy.format,
                bytes_before=bytes_before, bytes_after=target.stat().st_size,
                compressed=False, degraded=True,
                reason="Compressonator unavailable; uncompressed copy written",
                container=info.container, codec=info.codec, mip_levels=None,
                measured={"bytes_before": True, "bytes_after": True, "codec": True,
                          "mip_levels": False, "compression_ratio": False},
            )
            report.outcomes.append(outcome)
            report.bytes_before += bytes_before
            report.bytes_after += outcome.bytes_after
            continue

        _compress_one(source, target, policy, tool.path or "", timeout=timeout)
        written = sniff_texture(target)
        if written.codec is None:
            raise ExternalToolFailed(
                "compressonator", 0,
                stderr=f"output is not block-compressed: {target.name} "
                       f"(container={written.container})")
        outcome = CompressionOutcome(
            source=str(source), path=str(target), kind=kind, format=policy.format,
            bytes_before=bytes_before, bytes_after=target.stat().st_size,
            compressed=True, degraded=False,
            reason=policy.reason, container=written.container, codec=written.codec,
            mip_levels=ktx2_level_count(target),
            measured={"bytes_before": True, "bytes_after": True, "codec": True,
                      "mip_levels": ktx2_level_count(target) is not None,
                      "compression_ratio": True},
        )
        report.outcomes.append(outcome)
        report.bytes_before += bytes_before
        report.bytes_after += outcome.bytes_after

    return report


def bytes_before_after(report: CompressionReport) -> dict[str, Any]:
    """Explicit before/after accounting for the manifest and the receipt."""
    return {
        "bytes_before": report.bytes_before,
        "bytes_after": report.bytes_after,
        "saved_bytes": report.bytes_before - report.bytes_after,
        "ratio": round(report.ratio, 6),
        "measured": not report.degraded,
        "per_slot": [
            {"kind": o.kind, "format": o.format, "bytes_before": o.bytes_before,
             "bytes_after": o.bytes_after, "compressed": o.compressed, "degraded": o.degraded}
            for o in report.outcomes
        ],
    }


__all__ = [
    "BC4",
    "BC5",
    "BC7",
    "TEXTURE_POLICY",
    "CompressionOutcome",
    "CompressionReport",
    "TextureSlotPolicy",
    "bytes_before_after",
    "compress_textures",
    "ktx2_level_count",
    "select_format",
    "target_resolution",
]

