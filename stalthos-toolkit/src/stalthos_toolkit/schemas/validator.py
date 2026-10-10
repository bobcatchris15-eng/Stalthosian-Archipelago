"""A dependency-free JSON Schema (draft 2020-12 subset) validator.

Why hand-rolled: the toolkit contract is `pyproject` = stdlib + PyYAML only
(docs/toolkit-architecture.md section 1). Adding `jsonschema` would make every
operator install a C-extension dependency for what is a small, well-understood
vocabulary, so this module implements the subset the contracts/ schemas use and
reports precise RFC 6901 JSON pointers.

Supported keywords
------------------
  structural : type, properties, patternProperties, additionalProperties,
               propertyNames, required, dependentRequired, minProperties
  arrays     : items, prefixItems, minItems, maxItems, uniqueItems
  strings    : minLength, maxLength, pattern, format (annotation only), enum, const
  numbers    : minimum, maximum, exclusiveMinimum, exclusiveMaximum, multipleOf
  combinator : allOf, anyOf, oneOf, not, if/then/else
  references : $ref (local "#/$defs/..." pointers only), $defs
  annotations: $id, $schema, $comment, title, description, default, examples
               (carried through, never enforced)

Anything a schema uses that this validator does not implement is a *hard error*
at validation time (`UnsupportedSchema`), never a silently ignored keyword --
an unenforced constraint is worse than no constraint.

`format` is deliberately annotation-only (assertion format is a separate opt-in
in 2020-12). Semantic formats that the toolkit actually cares about (`date-time`,
`uri`) are checked by `stalthos_toolkit.schemas.formats` and can be layered on
top.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any, Iterator

__all__ = [
    "SchemaError",
    "UnsupportedSchema",
    "ValidationIssue",
    "SchemaValidator",
    "validate",
    "iter_errors",
]

_IGNORED_ANNOTATIONS = frozenset(
    {
        "$schema",
        "$id",
        "$anchor",
        "$comment",
        "$defs",
        "title",
        "description",
        "default",
        "examples",
        "deprecated",
        "readOnly",
        "writeOnly",
        "contentMediaType",
        "contentEncoding",
        "format",  # annotation only in draft 2020-12
        "unevaluatedProperties",  # asserted by nothing we generate
        "$comment_text",
    }
)

_SUPPORTED = frozenset(
    {
        "type",
        "properties",
        "patternProperties",
        "additionalProperties",
        "propertyNames",
        "required",
        "dependentRequired",
        "minProperties",
        "maxProperties",
        "items",
        "prefixItems",
        "minItems",
        "maxItems",
        "uniqueItems",
        "minLength",
        "maxLength",
        "pattern",
        "enum",
        "const",
        "minimum",
        "maximum",
        "exclusiveMinimum",
        "exclusiveMaximum",
        "multipleOf",
        "allOf",
        "anyOf",
        "oneOf",
        "not",
        "if",
        "then",
        "else",
        "$ref",
    }
)


class SchemaError(Exception):
    """The schema itself is unusable (bad regex, unresolvable $ref, ...)."""


class UnsupportedSchema(SchemaError):
    """The schema uses a keyword this validator does not implement."""


def escape_token(token: str) -> str:
    return str(token).replace("~", "~0").replace("/", "~1")


@dataclass(frozen=True)
class ValidationIssue:
    """One schema violation, anchored at a JSON pointer."""

    pointer: str
    keyword: str
    message: str
    value: Any = field(default=None, repr=False)

    @property
    def path(self) -> str:
        return self.pointer or "<root>"

    def __str__(self) -> str:  # pragma: no cover - human formatting
        return f"{self.path}: {self.message} [{self.keyword}]"

    def to_dict(self) -> dict[str, Any]:
        return {"pointer": self.path, "keyword": self.keyword, "message": self.message}


def iter_errors(instance: Any, schema: dict[str, Any], *, root_schema: dict[str, Any] | None = None) -> Iterator[ValidationIssue]:
    """Yield every violation of `schema` by `instance`."""
    yield from SchemaValidator(root_schema if root_schema is not None else schema).iter_errors(instance, schema)


def validate(instance: Any, schema: dict[str, Any], *, max_issues: int = 100) -> list[ValidationIssue]:
    """Return up to `max_issues` violations, deepest-and-earliest first."""
    issues = list(iter_errors(instance, schema))
    return issues[:max_issues]


# --- type checking ----------------------------------------------------------
_TYPE_MAP: dict[str, tuple[type, ...]] = {
    "object": (dict,),
    "array": (list, tuple),
    "string": (str,),
    "boolean": (bool,),
    "null": (type(None),),
    "number": (int, float),
    "integer": (int,),
}


def _type_name(value: Any) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "boolean"
    if isinstance(value, int):
        return "integer"
    if isinstance(value, float):
        return "number"
    if isinstance(value, str):
        return "string"
    if isinstance(value, dict):
        return "object"
    if isinstance(value, (list, tuple)):
        return "array"
    return type(value).__name__


def _matches_type(value: Any, expected: str) -> bool:
    types = _TYPE_MAP.get(expected)
    if types is None:
        raise SchemaError(f"unknown type name: {expected!r}")
    if expected in ("number", "integer") and isinstance(value, bool):
        return False  # JSON booleans are not numbers
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if expected == "boolean":
        return isinstance(value, bool)
    return isinstance(value, types)


def _is_hashable(value: Any) -> bool:
    try:
        hash(value)
    except TypeError:
        return False
    return True


def _canon(value: Any) -> Any:
    """A hashable projection of a JSON value, for enum/const/unique comparison."""
    if isinstance(value, dict):
        return ("__obj__", tuple(sorted((k, _canon(v)) for k, v in value.items())))
    if isinstance(value, (list, tuple)):
        return ("__arr__", tuple(_canon(v) for v in value))
    return value


class SchemaValidator:
    """Validate JSON instances against a draft 2020-12 subset."""

    def __init__(self, root_schema: dict[str, Any]):
        if not isinstance(root_schema, dict):
            raise SchemaError("schema root must be an object")
        self.root = root_schema
        self._regex_cache: dict[str, re.Pattern[str]] = {}

    # --- public ------------------------------------------------------------
    def iter_errors(self, instance: Any, schema: dict[str, Any] | bool) -> Iterator[ValidationIssue]:
        yield from self._validate(instance, schema, "")

    def is_valid(self, instance: Any, schema: dict[str, Any] | bool) -> bool:
        for _ in self.iter_errors(instance, schema):
            return False
        return True

    # --- internals ---------------------------------------------------------
    def _regex(self, pattern: str) -> re.Pattern[str]:
        compiled = self._regex_cache.get(pattern)
        if compiled is None:
            try:
                compiled = re.compile(pattern)
            except re.error as exc:
                raise SchemaError(f"invalid regular expression {pattern!r}: {exc}") from exc
            self._regex_cache[pattern] = compiled
        return compiled

    def _resolve(self, ref: str) -> dict[str, Any]:
        if not ref.startswith("#"):
            raise UnsupportedSchema(
                f"only local JSON pointer $ref is supported, got {ref!r}"
            )
        pointer = ref[1:]
        if not pointer.startswith("/"):
            raise UnsupportedSchema(f"$ref must start with '#/', got {ref!r}")
        node: Any = self.root
        for raw in pointer.lstrip("/").split("/"):
            token = raw.replace("~1", "/").replace("~0", "~")
            if isinstance(node, list):
                try:
                    node = node[int(token)]
                except (ValueError, IndexError) as exc:
                    raise SchemaError(f"unresolvable $ref: {ref!r}") from exc
            elif isinstance(node, dict) and token in node:
                node = node[token]
            else:
                raise SchemaError(f"unresolvable $ref: {ref!r}")
        if not isinstance(node, dict):
            raise SchemaError(f"$ref does not point at a schema object: {ref!r}")
        return node

    def _validate(self, value: Any, schema: dict[str, Any] | bool, pointer: str) -> Iterator[ValidationIssue]:
        if schema is True or schema == {}:
            return
        if schema is False:
            yield ValidationIssue(pointer, "false", "schema forbids any value here", value)
            return
        if not isinstance(schema, dict):
            raise SchemaError(f"schema at {pointer or '<root>'} must be an object or boolean")

        unknown = set(schema) - _SUPPORTED - _IGNORED_ANNOTATIONS
        if unknown:
            raise UnsupportedSchema(
                f"schema at {pointer or '<root>'} uses unsupported keyword(s): {sorted(unknown)}"
            )

        if "$ref" in schema:
            target = self._resolve(schema["$ref"])
            yield from self._validate(value, target, pointer)
            # 2020-12 allows siblings of $ref; keep validating them.

        yield from self._check_type(value, schema, pointer)
        yield from self._check_combinators(value, schema, pointer)
        yield from self._check_generic(value, schema, pointer)

        expected = schema.get("type")
        names = [expected] if isinstance(expected, str) else list(expected or [])
        object_keywords = ("properties", "patternProperties", "additionalProperties", "propertyNames",
                           "required", "dependentRequired", "minProperties", "maxProperties")
        if "object" in names or (not names and isinstance(value, dict)
                                 and any(key in schema for key in object_keywords)):
            yield from self._check_object(value, schema, pointer)
        if "array" in names or (not names and isinstance(value, (list, tuple))
                               and any(key in schema for key in ("items", "prefixItems", "minItems",
                                                                  "maxItems", "uniqueItems"))):
            yield from self._check_array(value, schema, pointer)

    def _check_type(self, value: Any, schema: dict[str, Any], pointer: str) -> Iterator[ValidationIssue]:
        expected = schema.get("type")
        if expected is None:
            return
        names = [expected] if isinstance(expected, str) else list(expected)
        for name in names:
            if _matches_type(value, name):
                return
        got = _type_name(value)
        expected_text = " or ".join(names)
        yield ValidationIssue(pointer, "type", f"expected {expected_text}, got {got}", value)

    def _check_combinators(self, value: Any, schema: dict[str, Any], pointer: str) -> Iterator[ValidationIssue]:
        for sub in schema.get("allOf", []) or []:
            yield from self._validate(value, sub, pointer)

        if "anyOf" in schema:
            branches = schema["anyOf"]
            if not any(self.is_valid(value, sub) for sub in branches):
                yield ValidationIssue(pointer, "anyOf", "value matches none of the anyOf branches", value)

        if "oneOf" in schema:
            branches = schema["oneOf"]
            matched = [i for i, sub in enumerate(branches) if self.is_valid(value, sub)]
            if len(matched) != 1:
                detail = "none" if not matched else f"{len(matched)} ({matched})"
                yield ValidationIssue(pointer, "oneOf", f"value must match exactly one oneOf branch, matched {detail}", value)

        if "not" in schema and self.is_valid(value, schema["not"]):
            yield ValidationIssue(pointer, "not", "value must not match the 'not' schema", value)

        if "if" in schema:
            if self.is_valid(value, schema["if"]):
                if "then" in schema:
                    yield from self._validate(value, schema["then"], pointer)
            elif "else" in schema:
                yield from self._validate(value, schema["else"], pointer)

    def _check_generic(self, value: Any, schema: dict[str, Any], pointer: str) -> Iterator[ValidationIssue]:
        if "const" in schema and _canon(value) != _canon(schema["const"]):
            yield ValidationIssue(pointer, "const", f"value must equal {schema['const']!r}", value)

        if "enum" in schema:
            options = schema["enum"]
            if not any(_canon(value) == _canon(option) for option in options):
                rendered = ", ".join(repr(option) for option in options)
                yield ValidationIssue(pointer, "enum", f"value must be one of [{rendered}]", value)

        if isinstance(value, str):
            yield from self._check_string(value, schema, pointer)
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            yield from self._check_number(value, schema, pointer)

    def _check_string(self, value: str, schema: dict[str, Any], pointer: str) -> Iterator[ValidationIssue]:
        if "minLength" in schema and len(value) < schema["minLength"]:
            yield ValidationIssue(pointer, "minLength", f"string shorter than minLength {schema['minLength']}", value)
        if "maxLength" in schema and len(value) > schema["maxLength"]:
            yield ValidationIssue(pointer, "maxLength", f"string longer than maxLength {schema['maxLength']}", value)
        if "pattern" in schema and not self._regex(schema["pattern"]).search(value):
            yield ValidationIssue(pointer, "pattern", f"string does not match pattern {schema['pattern']!r}", value)

    def _check_number(self, value: float, schema: dict[str, Any], pointer: str) -> Iterator[ValidationIssue]:
        if "minimum" in schema and value < schema["minimum"]:
            yield ValidationIssue(pointer, "minimum", f"value {value} < minimum {schema['minimum']}", value)
        if "maximum" in schema and value > schema["maximum"]:
            yield ValidationIssue(pointer, "maximum", f"value {value} > maximum {schema['maximum']}", value)
        if "exclusiveMinimum" in schema and value <= schema["exclusiveMinimum"]:
            yield ValidationIssue(pointer, "exclusiveMinimum", f"value {value} <= exclusiveMinimum {schema['exclusiveMinimum']}", value)
        if "exclusiveMaximum" in schema and value >= schema["exclusiveMaximum"]:
            yield ValidationIssue(pointer, "exclusiveMaximum", f"value {value} >= exclusiveMaximum {schema['exclusiveMaximum']}", value)
        if "multipleOf" in schema:
            divisor = schema["multipleOf"]
            if divisor and abs(value / divisor - round(value / divisor)) > 1e-9:
                yield ValidationIssue(pointer, "multipleOf", f"value {value} is not a multiple of {divisor}", value)

    def _check_object(self, value: dict[str, Any], schema: dict[str, Any], pointer: str) -> Iterator[ValidationIssue]:
        for name in schema.get("required", []) or []:
            if name not in value:
                yield ValidationIssue(
                    f"{pointer}/{escape_token(name)}",
                    "required",
                    f"required property {name!r} is missing",
                    None,
                )

        if "minProperties" in schema and len(value) < schema["minProperties"]:
            yield ValidationIssue(pointer, "minProperties", f"object has fewer than {schema['minProperties']} properties", value)
        if "maxProperties" in schema and len(value) > schema["maxProperties"]:
            yield ValidationIssue(pointer, "maxProperties", f"object has more than {schema['maxProperties']} properties", value)

        properties = schema.get("properties", {}) or {}
        pattern_properties = schema.get("patternProperties", {}) or {}
        additional = schema.get("additionalProperties", True)
        property_names = schema.get("propertyNames")

        for key in value:
            child_pointer = f"{pointer}/{escape_token(key)}"
            if property_names is not None and not self.is_valid(key, property_names):
                yield ValidationIssue(child_pointer, "propertyNames", f"property name {key!r} is not allowed", key)

            matched = False
            if key in properties:
                matched = True
                yield from self._validate(value[key], properties[key], child_pointer)
            for pattern, sub in pattern_properties.items():
                if self._regex(pattern).search(key):
                    matched = True
                    yield from self._validate(value[key], sub, child_pointer)

            if not matched and additional is not None:
                if additional is False:
                    yield ValidationIssue(child_pointer, "additionalProperties", f"unknown property {key!r} is not allowed", key)
                elif additional is not True:
                    yield from self._validate(value[key], additional, child_pointer)

        for name, dependents in (schema.get("dependentRequired") or {}).items():
            if name in value:
                for dependent in dependents:
                    if dependent not in value:
                        yield ValidationIssue(
                            pointer, "dependentRequired",
                            f"property {name!r} requires {dependent!r}", value,
                        )

    def _check_array(self, value: list[Any], schema: dict[str, Any], pointer: str) -> Iterator[ValidationIssue]:
        if "minItems" in schema and len(value) < schema["minItems"]:
            yield ValidationIssue(pointer, "minItems", f"array has fewer than {schema['minItems']} items", value)
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            yield ValidationIssue(pointer, "maxItems", f"array has more than {schema['maxItems']} items", value)

        if schema.get("uniqueItems"):
            seen: dict[Any, int] = {}
            for index, item in enumerate(value):
                key = _canon(item)
                if not _is_hashable(key):
                    continue
                if key in seen:
                    yield ValidationIssue(
                        f"{pointer}/{index}", "uniqueItems",
                        f"duplicate item (also at index {seen[key]})", item,
                    )
                else:
                    seen[key] = index

        prefix = schema.get("prefixItems") or []
        for index, sub in enumerate(prefix):
            if index < len(value):
                yield from self._validate(value[index], sub, f"{pointer}/{index}")

        items = schema.get("items")
        if items is not None:
            for index in range(len(prefix), len(value)):
                yield from self._validate(value[index], items, f"{pointer}/{index}")