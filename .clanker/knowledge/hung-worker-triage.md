---
title: "Hung workers are usually unretired roster entries, not hangs"
status: confirmed
tags: [operations, triage, workers, registry]
---

## Symptom

`worker_list` shows a roster of a dozen workers, most with stale
`last_activity`, and the human reports "the workers have gotten hung up".

## Diagnosis that actually applied (2026-10-11)

Checked the three apparent suspects (W24/W25/W27): **all three node.exe
processes were alive**, `last_activity` was within ~30s of the spawn, and the
wall clock was `02:23:51Z` against a `02:23:17Z` spawn. They were 34 seconds
into startup. Not hung.

The real problem was *upstream* of them: W14, W15 and W20 had finished
(`COMPLETE`, one result each, real work committed) and six workers were
`CANCELLED` — none of the nine had been retired. A full roster of dead
entries reads as "everything is stuck".

## Triage rule

1. **Get UTC now before believing any staleness number.**
   `last_activity` is an absolute UTC timestamp; the registry file mtime is
   local. Comparing the two, or eyeballing a list, manufactures hangs.
2. **A worker is hung only if its PID is gone** (`tasklist //FI "PID eq N"`
   from `workers/registry.json`) **and** status is not terminal. No PID means
   it exited; terminal status means it finished.
3. **Zero `tokens_used` is normal during startup**, not a stall signal.
4. `current_action` on a `COMPLETE` worker holds a *description*, not an
   action — do not read it as "still working on this".

## Prevention

- Retire finished and cancelled workers the same turn their result arrives.
  An unretired terminal worker is indistinguishable from a hung one at a glance.
- `worker_list` output should be triaged as: alive-PID-and-fresh = waiting;
  terminal-status = harvest the result then retire; PID-gone-and-running = the
  genuine hang, the only case that needs `worker_cancel`.

## Related

- [[profile-repair-lessons]] — same shape: trust the file on disk over what a
  tool call seemed to accomplish.