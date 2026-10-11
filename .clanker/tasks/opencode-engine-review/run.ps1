$ErrorActionPreference = 'Stop'
$env:OPENCODE_CONFIG_CONTENT = '{"permission":{"*":"deny","read":"allow","glob":"allow","grep":"allow","bash":"ask","edit":"deny","webfetch":"deny"}}'
& 'C:/Users/Chris/AppData/Roaming/npm/opencode.ps1' run 'You have read the necessary sources. Do not use any more tools. Return the actionable report now as final text based on your completed source reads; mark unverified details. This follow-up requires no shell commands.' --pure --agent build --dir 'E:/Stalthosian Archipelago/var/worktrees/opencode-engine-review' --format json --model 'opencode/space-bunny-free' --title 'opencode-engine-review' --session 'ses_ed409ead5ffeI9lQbMTTJsxZ4f' 1> 'E:/Stalthosian Archipelago/.clanker/tasks/opencode-engine-review/events.jsonl' 2> 'E:/Stalthosian Archipelago/.clanker/tasks/opencode-engine-review/stderr.log'
$exitCode = $LASTEXITCODE
@{exit_code=$exitCode; model='opencode/space-bunny-free'; ended_at=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content 'E:/Stalthosian Archipelago/.clanker/tasks/opencode-engine-review/process-result.json' -Encoding utf8
exit $exitCode
