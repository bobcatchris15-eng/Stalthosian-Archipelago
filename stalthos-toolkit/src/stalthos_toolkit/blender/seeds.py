"""Deterministic seed derivation shared by Python and Blender.

This module is **the single source of truth** for the child-seed algorithm.
It is pure stdlib (no bpy), so:

* the toolkit side uses it directly;
* the bpy scripts import *this same file* (they add this package's
  ``blender/scripts`` shim to ``sys.path`` and import it), so the mirror can
  never drift from the original;
* the test suite can prove the two sides agree without Blender installed.

The algorithm is a byte-for-byte copy of ``core.seeding.derive_seed`` (which is
frozen): sha256 over the root seed's normalized decimal form, joined to the
``str()`` of each label with ``\x1f`` (ASCII unit separator), first 8 bytes
big-endian, masked to 63 bits. ``test_blender.py`` asserts the two agree for
several roots and label paths, so this file cannot silently drift from core.
"""

from __future__ import annotations

import hashlib
import random
from typing import Any

MASK = (1 << 63) - 1

#: Label separator used by core.seeding (ASCII unit separator, 0x1F).
SEPARATOR = "\x1f"


def normalize_seed(seed: int | str) -> int:
    """Accept an int or a stable string label; both map to a 63-bit seed."""
    if isinstance(seed, int):
        return seed & MASK
    digest = hashlib.sha256(str(seed).encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big") & MASK


def derive_seed(root: int | str, *labels: Any) -> int:
    """Derive a stable child seed from a root seed and a label path.

    ``derive_seed(42, "plant", "araucaria", "branch", 3)`` is stable across runs,
    machines, platforms and Python versions, and identical on both sides of the
    Blender process boundary.
    """
    parts = [str(normalize_seed(root))]
    parts += [str(label) for label in labels]
    digest = hashlib.sha256(SEPARATOR.join(parts).encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big") & MASK


def rng(root: int | str, *labels: Any) -> random.Random:
    """A ``random.Random`` seeded from :func:`derive_seed`. Use this, not the
    global ``random``."""
    return random.Random(derive_seed(root, *labels))


__all__ = ["MASK", "SEPARATOR", "derive_seed", "normalize_seed", "rng"]
