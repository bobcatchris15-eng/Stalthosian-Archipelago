"""Offline locator/format classification used by pipeline stage 1.

Pure string/path inspection: nothing here opens a socket, resolves a hostname or
downloads anything. A locator that *looks* like a URL is still recorded as
unresolved content -- we only ever normalize its shape.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

#: `10.1016/j.whatever` style DOI, plus the bare `doi:10.xxxx/yyy` form.
_DOI_RE = re.compile(r"^(?:doi:\s*|https?://(?:dx\.)?doi\.org/)?(10\.\d{4,9}/\S+)$", re.IGNORECASE)
_URL_RE = re.compile(r"^[a-z][a-z0-9+.-]*://", re.IGNORECASE)
_IDENTIFIER_RE = re.compile(r"^(?:urn:|ark:|isbn:|bi:|gbif:|taxon:)", re.IGNORECASE)

_URL_NORMALIZERS = {
    "HTTP": "http",
    "HTTPS": "https",
    "DOI": "https://doi.org/",
    "FTP": "ftp",
}


@dataclass(frozen=True)
class LocatorInfo:
    type: str                     # local_path | url | doi | identifier | text
    normalized: str | None
    exists: bool | None = None    # only checked for local_path


def classify_locator(locator: str) -> LocatorInfo:
    """Classify a reference locator without touching the network.

    `exists` is computed only for local paths, resolved against the caller's
    base directory; for everything else it stays None ("not checkable offline")
    so a record never claims a URL was verified.
    """
    raw = locator.strip()
    if not raw:
        return LocatorInfo("text", None, None)

    doi_match = _DOI_RE.match(raw)
    if doi_match:
        return LocatorInfo("doi", doi_match.group(1), None)

    if _URL_RE.match(raw):
        scheme = raw.split("://", 1)[0].lower()
        prefix = _URL_NORMALIZERS.get(scheme, f"{scheme}://")
        tail = raw.split("://", 1)[1]
        return LocatorInfo("url", f"{prefix}{tail}", None)

    if _IDENTIFIER_RE.match(raw):
        return LocatorInfo("identifier", raw.lower(), None)

    path = Path(raw)
    if path.is_absolute() or "/" in raw or "\\" in raw or path.suffix:
        return LocatorInfo("local_path", raw, None)   # existence resolved by the caller
    return LocatorInfo("text", raw, None)


def check_local_path(locator: str, base: Path | None = None) -> LocatorInfo:
    """`classify_locator` plus a local existence probe (offline, read-only)."""
    info = classify_locator(locator)
    if info.type != "local_path":
        return info
    path = Path(locator)
    if not path.is_absolute() and base is not None:
        path = (base / path).resolve()
    try:
        return LocatorInfo("local_path", info.normalized, path.exists())
    except OSError:                                  # pragma: no cover - unreadable mount
        return LocatorInfo("local_path", info.normalized, None)


#: Reference kinds that can be resolved to a local file at all.
LOCAL_KINDS = frozenset({"local_image", "local_model", "local_scan", "field_note"})

LOCAL_SUFFIXES = {
    ".png": "image",
    ".jpg": "image",
    ".jpeg": "image",
    ".webp": "image",
    ".exr": "image",
    ".tif": "image",
    ".tiff": "image",
    ".blend": "model",
    ".glb": "model",
    ".gltf": "model",
    ".fbx": "model",
    ".obj": "model",
    ".ply": "scan",
    ".stl": "scan",
    ".stp": "scan",
    ".md": "text",
    ".txt": "text",
}


def local_media_type(locator: str) -> str | None:
    return LOCAL_SUFFIXES.get(Path(locator.strip()).suffix.lower())


def rfc3339(value: str) -> bool:
    """Cheap RFC 3339 shape check (no `datetime` parsing cost, no tz library)."""
    return bool(
        re.match(
            r"^\d{4}-\d{2}-\d{2}[Tt ]\d{2}:\d{2}:\d{2}(\.\d+)?([Zz]|[+-]\d{2}:\d{2})?$",
            value.strip(),
        )
    )