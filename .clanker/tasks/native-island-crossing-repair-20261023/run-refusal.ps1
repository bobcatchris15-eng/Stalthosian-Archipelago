$ErrorActionPreference='Stop'
$task='.clanker/tasks/native-island-crossing-repair-20261023'
$old='var/island-partial-regression.stlc'
$destination='var/test-saves/native-island-crossing-repair-20261023/refusal-sentinel.stlc'
if(Test-Path -LiteralPath $destination){throw 'Refusal sentinel already exists; preserve it and choose another owned path'}
[IO.File]::WriteAllText((Join-Path $PWD $destination),'OWNED REFUSAL SENTINEL - preserve exactly')
(Get-FileHash -LiteralPath $old).Hash | Set-Content "$task/old-save-before.sha256"
(Get-FileHash -LiteralPath $destination).Hash | Set-Content "$task/destination-before.sha256"
try{./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedPersistence -ConnectedLoad $old -ConnectedSave $destination -Duration 5 > "$task/old-config-refusal.log" 2>&1}catch{ $_.Exception.Message | Set-Content "$task/old-config-refusal-exit.txt" }
(Get-FileHash -LiteralPath $old).Hash | Set-Content "$task/old-save-after.sha256"
(Get-FileHash -LiteralPath $destination).Hash | Set-Content "$task/destination-after.sha256"
