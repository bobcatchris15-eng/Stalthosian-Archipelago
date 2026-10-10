"""Definition loading (species / creature YAML or JSON).

Kept dependency-light on purpose: JSON always works, YAML requires PyYAML and
degrades with an actionable error (exit 2) rather than an ImportError.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from ..core.errors import UsageError


def load_definition(path: str | Path) -> dict[str, Any]:
    """Load a definition file into a plain dict."""
    path = Path(path)
    text = path.read_text(encoding="utf-8")
    suffix = path.suffix.lower()

    if suffix in {".json"}:
        return _as_dict(json.loads(text), path)
    if suffix in {".yaml", ".yml"}:
        try:
            import yaml
        except ImportError as exc:  # pragma: no cover - environment dependent
            raise UsageError(
                "PyYAML is required to read YAML definitions",
                hint="pip install PyYAML, or convert the definition to JSON",
            ) from exc
        return _as_dict(yaml.safe_load(text), path)
    if suffix in {".toml"}:
        import tomllib

        return _as_dict(tomllib.loads(text), path)

    # Unknown suffix: try JSON, then YAML.
    try:
        return _as_dict(json.loads(text), path)
    except json.JSONDecodeError:
        pass
    try:
        import yaml

        return _as_dict(yaml.safe_load(text), path)
    except ImportError as exc:
        raise UsageError(f"cannot parse {path.name}: unknown format and PyYAML unavailable") from exc


def _as_dict(data: Any, path: Path) -> dict[str, Any]:
    if data is None:
        raise UsageError(f"definition file is empty: {path}")
    if not isinstance(data, dict):
        raise UsageError(f"definition file must contain a mapping at the top level: {path}")
    return data


def load_definition_file_list(root: str | Path, pattern: str = "**/*.y*ml") -> list[Path]:
    return sorted(Path(root).glob(pattern))
