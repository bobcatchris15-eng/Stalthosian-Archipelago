# Antigravity worker pool

User authorization: separate worker pool, at most three concurrent `agy` tasks,
target Gemini 3.8 Flash. This is independent of native and Jules concurrency.
Existing wake duration and dispatch bounds remain in force.

Installed executable: `C:/Users/Chris/AppData/Local/agy/bin/agy.exe`.
`agy models` verified these IDs:

- `gemini-3.8-flash-low` — default for straightforward bounded tasks.
- `gemini-3.8-flash-medium` — use when complexity requires it.
- `gemini-3.8-flash-high` — use only when evidence warrants escalation.

Verified headless invocation:

```powershell
agy --model gemini-3.8-flash-low --effort low `
    --print 'Return exactly AGY_READY. Do not use any tools.' `
    --output-format json --print-timeout 45s
```

Actual smoke returned JSON `status: SUCCESS`, `response: AGY_READY`, one turn.
This verifies model access and single-prompt execution, not editing/compilation.
Raw response and stderr remain under ignored `var/reports/agy-readiness`.

Relevant installed flags: `--print`/`-p`, `--model`, `--effort low|medium|high`,
`--output-format text|json|stream-json`, `--print-timeout`, `--conversation`,
`--agent`, `--mode accept-edits|plan`, `--sandbox`, `--log-file`, and
`--disable-slash-commands`. Use `agy --help`, `agy models`, and `agy agents`
for the installed version. Agent listing returned no named agents in this probe.

Dispatch from an isolated project worktree, with bounded owned files, explicit
acceptance criteria, timeout, logs and a receipt. Track AGY processes separately
from native worker slots. Never launch a fourth concurrent AGY task. Do not use
`--dangerously-skip-permissions`; a tool-free smoke does not establish unattended
edit permission behavior. Preserve unrelated files and planning input. Root
reviews the diff and runs real local checks before merging or publishing.
