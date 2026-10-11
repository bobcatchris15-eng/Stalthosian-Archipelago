status: confirmed
tags: [stalthos, blender, environment, testing]

## Blender is not installed on this machine

`where blender` finds nothing; no Blender in Program Files or the usual
install roots. The `--launcher` probe found no `blender-launcher.exe`.

Consequence: every `@needs_blender` test **skips** here. The toolkit's Blender
paths (running real scripts, stage variation, preview render, mesh validation)
are therefore **unverified against real Blender** — verified only by unit tests
that stub `subprocess`. `docs/asset-toolkit.md` says Blender 4.2+ is required.

## What this bites, concretely

On 2026-10-11 a `UnicodeDecodeError` in `blender --version` surfaced only in
"CI", because it was a Windows **locale** issue (cp1252 banner byte 0x90) and
CI reports machine-checkout. It silently degraded the recorded tool version to
`"unknown"` in provenance, which the determinism rules care about.

## Triage rule

A `.cmd` path is a valid way to invoke Blender from Git Bash, and it resolved
here — but I could not execute it, because no Blender is installed. **Do not
conflate "the launcher path resolves" with "Blender runs".** Verify with an
actual version string before trusting any real-Blender claim.

## Prevention

- New decode paths that read tool output must pin `encoding=`/`errors=`
  explicitly, never inherit the locale codec. See [[stalthos-determinism-rules]].
- Tests that pin down Windows encoding behaviour can and should reproduce it
  without the tool installed (stub `subprocess.run`, feed `0x90`). Verified this
  reproduces the real CI traceback.
- Un-verifiable-by-test work must be marked `measured=False` / *To test*, not
  quietly assumed.

## Related

- [[stalthos-determinism-rules]] — provenance must record a real tool version.