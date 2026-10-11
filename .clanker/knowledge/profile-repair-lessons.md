---
title: Curated profile repair — how setProfile can silently no-op
status: confirmed
tags: [catalog, profiles, operations]
---

## Symptom

`profile_set` on a model glob appeared to succeed, but `endpoint_status` kept
returning the old ratings afterwards. Two layers contributed:

1. **`profiles.json` is layered, not merged-flat.** A wildcard entry
   (`provider/*`) and a specific entry (`provider/id`) both apply. The
   specific entry wins per-capability, but stale entries survive and leak.
2. **Invalid capability values pass silently.** `setProfile` validates incoming
   capabilities against `CAPABILITIES` (research, coding, ...), but *ratings*
   are only `Rating` ("poor"|"fair"|"good"|"excellent"|"unknown"). Values like
   `"weak"` were written to disk, and `resolveProfile` only filters by
   *capability name* — so `"weak"` landed in the ratings map and flowed into
   scoring, where it matches nothing.

## Prevention

- Write ratings only from `Rating` in `catalog/types.ts`. Spell failure as
  `"poor"`, never `"weak"`.
- Before trusting an `endpoint_status`, read the profile file and check for
  duplicate wildcard + specific entries.
- After repairing by hand, re-read the file from disk; do not assume the tool
  call landed.

## Verification

`endpoint_status` shows `ratings` and a parallel `source` map. Anything you did
not just write should read `unknown`/`derived`. A capability with source
`curated` that you never set is a leak from a stale entry.

## Related

- [[living-context]] — where this knowledge graph lives and how it is revised.
