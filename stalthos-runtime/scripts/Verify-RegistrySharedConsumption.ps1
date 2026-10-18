[CmdletBinding()]
param([double]$Duration=8)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$report=Join-Path $root ('var/reports/registry-shared-consumption/'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $report | Out-Null
$save=Join-Path $report 'shared.stl'
function Invoke-World([hashtable]$Options,[string]$Log) {
    $lines=@(& (Join-Path $PSScriptRoot 'Run.ps1') @Options)
    $lines | Set-Content -LiteralPath $Log -Encoding utf8
    $launch=$lines | Where-Object {$_ -match '^Process [0-9]+; evidence '} | Select-Object -First 1
    if(-not $launch) {throw 'Probe did not report an evidence path'}
    return ($launch -replace '^Process [0-9]+; evidence ','')
}
$first=Invoke-World @{PopulatedWorld=$true;PopulatedResourceProbe=$true;Duration=$Duration;SaveState=$save} (Join-Path $report 'first.log')
$runtime=Get-Content -LiteralPath (Join-Path $first 'runtime-receipt.json') -Raw | ConvertFrom-Json
$population=Get-Content -LiteralPath (Join-Path $first 'world-population.json') -Raw | ConvertFrom-Json
if ($runtime.populated_resource_probe -ne $true -or $runtime.drink_contacts_applied -ne 1 -or $runtime.resource_ledger_cap_exhaustions -ne 0) {throw 'Native hero shared-resource contact failed'}
if ($population.registry_owns_population -ne $true -or $population.registry_owns_shared_ledger -ne $true -or $population.water_consumed -le 0) {throw 'Native registry NPC consumption absent'}
if ($null -eq $population.snapshot_final -or $null -eq $population.resource_ledger_initial -or $null -eq $population.resource_ledger_final -or $null -eq $population.resource_sessions_final) {throw 'Complete native registry telemetry absent'}
if (@($population.resource_ledger_initial.resources | Where-Object id -eq 100).Count -ne 1 -or @($population.resource_ledger_final.resources | Where-Object id -eq 100).Count -ne 1) {throw 'Expected exactly one shared creek resource'}
$waterInitial=@($population.resource_ledger_initial.resources | Where-Object id -eq 100)[0]
$waterFinal=@($population.resource_ledger_final.resources | Where-Object id -eq 100)[0]
$owned=@($population.resource_sessions_final | ForEach-Object {$_[1]})
$heroKeys=@($population.resource_ledger_final.sessions | Where-Object {$_.resource_id -eq 100 -and $_.session_id -notin $owned})
if ($heroKeys.Count -ne 1 -or $heroKeys[0].amount -ne .5) {throw 'Hero did not retain one independent half-unit creek key'}
$depletion=$waterFinal.consumed_quantity-$waterInitial.consumed_quantity
$depletionError=[Math]::Abs($depletion-($population.water_consumed+.5))
if ($depletionError -gt 1e-8) {throw 'Hero and NPC depletion do not match one shared ledger'}
$second=Invoke-World @{PopulatedWorld=$true;Duration=4;LoadState=$save;SaveState=(Join-Path $report 'resumed.stl')} (Join-Path $report 'second.log')
$restored=Get-Content -LiteralPath (Join-Path $second 'world-population.json') -Raw | ConvertFrom-Json
if ($restored.population_restored -ne $true -or $null -eq $restored.snapshot_initial -or $null -eq $restored.resource_ledger_initial -or $null -eq $restored.resource_sessions_initial) {throw 'Restored native registry telemetry absent'}
foreach ($pair in @(@($population.resource_ledger_final,$restored.resource_ledger_initial),@($population.resource_sessions_final,$restored.resource_sessions_initial),@($population.snapshot_final,$restored.snapshot_initial))) {
    if ((ConvertTo-Json -InputObject $pair[0] -Depth 12 -Compress) -cne (ConvertTo-Json -InputObject $pair[1] -Depth 12 -Compress)) {throw 'Shared hero/NPC state changed across native save/load'}
}
$result=[ordered]@{passed=$true;diagnostic_probe=$true;manual_input_verified=$false;hero_drink_contacts=1;hero_water_consumed=.5;npc_water_consumed=$population.water_consumed;combined_depletion=$depletion;depletion_abs_error=$depletionError;complete_ledger_and_history_restored=$true;exact_actor_snapshots_restored=$true;exact_session_ownership_restored=$true;first_evidence=$first;second_evidence=$second;save=$save}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $report 'receipt.json') -Encoding utf8
$result | ConvertTo-Json
