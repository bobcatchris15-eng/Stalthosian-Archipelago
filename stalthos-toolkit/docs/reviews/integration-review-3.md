# Integration Review 3

## Findings

| Severity | File:line | What breaks | Evidence | Suggested fix |
|----------|-----------|-------------|----------|---------------|
| major | stalthos-toolkit/src/stalthos_toolkit/core/status.py:log field and to_dict | Logs are placed in the JSON status envelope under the "logs" key, violating the contract that logs must go to stderr. | Running `stalthos research_species` shows a "logs" array in the JSON output, and stderr is empty. | Remove the `logs` field from the Status class and instead log messages to stderr (e.g., using `print(message, file=sys.stderr)` or a logging mechanism). The `log` method should write to stderr. |
| major | stalthos-toolkit/src/stalthos_toolkit/cli.py:--seed argument parsing | The CLI's --seed argument is always treated as a string and hashed, even when the user intends to pass an integer seed. This violates the documented interface that accepts an integer or a string. Moreover, the seed recorded in the status is the hash of the string, not the integer seed if the user passed an integer as a string. | Running `stalthos research_species --seed 0` results in a status seed of 6912158355717386040, which is the hash of the string "0". Running with --seed 12345 gives a different seed. The seed is not the integer provided. | Change the CLI to accept an integer seed by trying to parse the argument as an integer first; if successful, pass the integer to `normalize_root_seed`; otherwise, pass the string. This will allow integer seeds to be used directly. |
| major | stalthos-toolkit/src/stalthos_toolkit/schemas/validator.py:format ignored | The JSON Schema validator ignores the "format" keyword, treating it as annotation-only. This means that schemas that rely on format constraints (e.g., "date-time" for created_at) are not being validated. This could lead to accepting invalid data, violating the contract that the schema is enforced. | The validator returns no errors for both valid and invalid date-time strings when the schema has "format": "date-time". | Either implement format validation for the semantic formats that the toolkit cares about (date-time, uri) or remove the format constraints from the schemas and rely on explicit validation in the code. Given the validator's documentation notes that semantic formats are checked by `stalthos_toolkit.schemas.formats`, we should implement that checking. |
| minor | stalthos-toolkit/tests/ (various) | The test suite times out when running all tests, indicating that some tests may be hanging. This could be due to tests that require Blender not being skipped properly, or other infinite loops. | Running `pytest -q` from the stalthos-toolkit directory timed out after 30 seconds. | Ensure that tests that require Blender are properly skipped when Blender is absent. Also, review tests for potential infinite loops. |

## What I tried that failed to break

- **Safety: path traversal in workspace resolution**: I attempted to trick the workspace resolution by using paths like `../../../etc/passwd` in the `--definition` or `--out` arguments, but the workspace resolution correctly resolves paths relative to the workspace root and does not allow escaping.
- **Safety: shell injection when invoking Blender**: I checked the Blender runner code and saw that it uses `subprocess.run` with a list of arguments, not a shell string, so shell injection is not possible.
- **Safety: unsafe YAML loading**: The definition loading uses `yaml.safe_load`, which is safe.
- **Honesty: fabricated metrics**: I tried to find any code path that reports success it did not achieve, but the commands that are implemented (research_species, validate_asset) appear to be honest about their success and failure conditions.
- **Contract: exit codes**: I tested unimplemented commands (e.g., generate_plant) and they correctly return exit 5 with a NotImplementedStage error.
- **Determinism: global random**: I searched for uses of `random` or `hash()` in the codebase and found none outside of the seeding module, which uses deterministic seeding.

## Honesty claims assessment

The project's first non-negotiable is "Honesty over completeness." I investigated whether any code path reports success it did not achieve, fabricates a metric, or swallows an error.

- The implemented commands (`research_species`, `validate_asset`) return honest status envelopes: they set `ok` and `exit_code` correctly, and they do not claim success when validation fails or when an error occurs.
- The metrics recorded in the status envelope are actual measurements (e.g., record path, written flag) and not fabricated.
- Errors and warnings are populated appropriately.
- The schema validation in `validate_asset` does run and reports validation failures.
- The CLI correctly reports exit 5 for unimplemented commands and does not write any artifacts.

Thus, the honesty claim holds for the implemented parts of the toolkit. However, the honesty claim is undermined by the contract violations listed above, particularly the seed handling and logging issues, which could lead to users misunderstanding the actual seed used or where logs go.

The claim that "Same (definition, seed, tool versions) ⇒ same bytes out" is true for the seeding mechanism itself, but the CLI's seed handling means that the seed recorded in the status is not the user-provided integer if they intended to pass one, which could confuse reproducibility. However, given the same string input to the CLI, the output is deterministic.

Overall, the honesty claim survives for the core functionality, but the contract violations reduce trust in the system's adherence to its documented interface.

## Conclusion

The stalthos-toolkit foundation shows promise but has several contract violations that need to be addressed before it can be fully trusted. The honesty of the implemented commands is good, but the system does not fully adhere to its own documented interface, particularly in seed handling and logging. The schema validator also fails to enforce format constraints, which could lead to accepting invalid data. These issues should be fixed to ensure the toolkit can be trusted to build the remaining stages on.