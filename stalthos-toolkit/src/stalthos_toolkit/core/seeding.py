"""Deterministic seeding and content hashing.

Reproducibility rule: every stochastic decision in the pipeline is derived from
the root seed via a named, stable sub-seed function. Never use `random` or
`hash()` directly in stage code -- use `derive_seed` / `rng`.
"""

from __future__ import annotations

import hashlib
import json
import random
from pathlib import Path
from typing import Any

_MASK = (1 << 63) - 1


def normalize_seed(seed: int | str) -> int:
    """Accept an int or a stable string label; both map to a 63-bit seed."""
    if isinstance(seed, int):
        return seed & _MASK
    digest = hashlib.sha256(str(seed).encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big") & _MASK


def derive_seed(root: int | str, *labels: Any) -> int:
    """Derive a stable child seed from a root seed and a label path.

    `derive_seed(42, "plant", "araucaria", "branch", 3)` is stable across runs,
    machines, platforms and Python versions.
    """
    parts = [str(normalize_seed(root))]
    parts += [str(label) for label in labels]
    digest = hashlib.sha256("".join(parts).encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big") & _MASK


def rng(root: int | str, *labels: Any) -> random.Random:
    """A `random.Random` seeded from `derive_seed`. Use this, not global random."""
    return random.Random(derive_seed(root, *labels))


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: str | Path, *, chunk: int = 1 << 20) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        while True:
            block = fh.read(chunk)
            if not block:
                break
            h.update(block)
    return h.hexdigest()


def canonical_json(value: Any) -> str:
    """Stable JSON serialization used for input hashing."""
    return json.dumps(value, sort_keys=True, separators=(",", ":"), default=str)


def hash_config(config: Any) -> str:
    """Hash a definition/config object so provenance can prove what was built."""
    return sha256_bytes(canonical_json(config).encode("utf-8"))
