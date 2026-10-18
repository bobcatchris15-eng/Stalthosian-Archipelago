[CmdletBinding()]
param([double]$FirstDuration=8, [double]$SecondDuration=4, [switch]$FirstTraversal,
      [string]$InitialSave='', [string]$InitialPopulationEvidence='')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ([bool]$InitialSave -ne [bool]$InitialPopulationEvidence) {
    throw 'InitialSave and InitialPopulationEvidence must be supplied together to verify legacy migration'
}
if ($FirstTraversal -and $InitialSave) {throw 'FirstTraversal and InitialSave cannot be combined'}
$legacyPopulation=$null
if ($InitialSave) {
    $InitialSave=(Resolve-Path -LiteralPath $InitialSave).Path
    $legacyPopulation=Get-Content -LiteralPath $InitialPopulationEvidence -Raw | ConvertFrom-Json
    if ($legacyPopulation.schema_version -ne 2 -or $null -eq $legacyPopulation.snapshot_final) {
        throw 'Legacy population evidence must contain a full schema 2 final snapshot'
    }
}
$report=Join-Path $root ('var/reports/population-continuation/'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $report | Out-Null
$firstSave=Join-Path $report 'first.stl'
$secondSave=Join-Path $report 'second.stl'
function Invoke-Probe([double]$Seconds,[string]$Load,[string]$Save,[string]$Log) {
    $options=@{PopulatedWorld=$true;Duration=$Seconds;CameraDistance=16;SaveState=$Save}
    if($FirstTraversal -and -not $Load) {$options.PopulatedTraversal=$true}
    if($Load) {$options.LoadState=$Load}
    $lines=@(& (Join-Path $PSScriptRoot 'Run.ps1') @options)
    $lines | Set-Content -LiteralPath $Log -Encoding utf8
    $launch=$lines | Where-Object {$_ -match '^Process [0-9]+; evidence '} | Select-Object -First 1
    if(-not $launch) {throw 'Probe did not report its evidence path'}
    return ($launch -replace '^Process [0-9]+; evidence ','')
}
$first=Invoke-Probe $FirstDuration $InitialSave $firstSave (Join-Path $report 'first.log')
$second=Invoke-Probe $SecondDuration $firstSave $secondSave (Join-Path $report 'second.log')
$before=Get-Content -LiteralPath (Join-Path $first 'world-population.json') -Raw | ConvertFrom-Json
$after=Get-Content -LiteralPath (Join-Path $second 'world-population.json') -Raw | ConvertFrom-Json
$firstRuntime=Get-Content -LiteralPath (Join-Path $first 'runtime-receipt.json') -Raw | ConvertFrom-Json
$secondRuntime=Get-Content -LiteralPath (Join-Path $second 'runtime-receipt.json') -Raw | ConvertFrom-Json
foreach ($population in @($before,$after)) {
    if ($population.registry_owns_population -ne $true -or $population.registry_owns_shared_ledger -ne $true) {
        throw 'Native host did not report registry ownership of both population and shared ledger'
    }
    if ($population.actors_initial.Count -ne 8 -or $population.actors_final.Count -ne 8) {
        throw 'Original watershed population must retain exactly eight actors'
    }
    if ($population.snapshot_initial.habitat_signature -ne '6489859816375510053') {
        throw 'Original watershed habitat signature changed'
    }
}
if ($legacyPopulation) {
    $legacyExpected=ConvertTo-Json -InputObject $legacyPopulation.snapshot_final -Depth 10 -Compress
    $legacyActual=ConvertTo-Json -InputObject $before.snapshot_initial -Depth 10 -Compress
    if ($legacyExpected -cne $legacyActual -or -not $before.population_restored) {
        throw 'Legacy population changed during registry migration'
    }
}
if($firstRuntime.resource_ledger_cap_exhaustions -ne 0 -or $secondRuntime.resource_ledger_cap_exhaustions -ne 0) {throw 'Resource ledger exhausted during continuation'}
# Reconstruct activation quantity from final quantity plus complete NPC consumption.
$waterError=[Math]::Abs($firstRuntime.resources_final.creek-($secondRuntime.resources_final.creek+$after.water_consumed))
if($waterError -gt 1e-8) {throw 'Saved creek depletion did not restore exactly'}
$consumptionError=[Math]::Abs(($firstRuntime.resources_final.creek-$secondRuntime.resources_final.creek)-$after.water_consumed)
if($after.water_consumed -le 0 -or $consumptionError -gt 1e-7) {throw 'Resumed creek consumption did not match the shared ledger'}
if($before.schema_version -ne 2 -or $after.schema_version -ne 2 -or $null -eq $before.snapshot_final -or $null -eq $after.snapshot_initial) {throw 'Full population snapshots absent'}
$expected=ConvertTo-Json -InputObject $before.snapshot_final -Depth 10 -Compress
$actual=ConvertTo-Json -InputObject $after.snapshot_initial -Depth 10 -Compress
if($expected -cne $actual) {throw "Population changed across save/load; evidence retained at $report"}
$expectedSessions=ConvertTo-Json -InputObject $before.resource_sessions_final -Depth 6 -Compress
$actualSessions=ConvertTo-Json -InputObject $after.resource_sessions_initial -Depth 6 -Compress
if ($null -eq $before.resource_sessions_final -or $null -eq $after.resource_sessions_initial -or $expectedSessions -cne $actualSessions) {
    throw 'Actor resource session ownership changed across save/load'
}
$expectedLedger=ConvertTo-Json -InputObject $before.resource_ledger_final -Depth 10 -Compress
$actualLedger=ConvertTo-Json -InputObject $after.resource_ledger_initial -Depth 10 -Compress
if ($null -eq $before.resource_ledger_final -or $null -eq $after.resource_ledger_initial -or $expectedLedger -cne $actualLedger) {throw 'Complete resource ledger/history changed across save/load'}
if(-not $after.population_restored -or -not $after.population_persisted) {throw 'Population continuation flags missing'}
$advanced=$false
for($i=0;$i -lt $after.actors_final.Count;$i++) {
    if($after.actors_final[$i].distance_m -gt $after.actors_initial[$i].distance_m) {$advanced=$true}
}
if(-not $advanced) {throw 'No resumed actor advanced along its route'}
$result=[ordered]@{passed=$true;actor_count=$after.actors_initial.Count;exact_initial_state_restored=$true;full_snapshot_fields_verified=$true;
    registry_owns_population=$true;registry_owns_shared_ledger=$true;exact_session_ownership_restored=$true;exact_complete_ledger_restored=$true;legacy_migration_verified=[bool]$legacyPopulation;legacy_save=$InitialSave;
    first_run_traversal=[bool]$FirstTraversal;resumed_motion=$advanced;water_restore_abs_error=$waterError;water_consumption_abs_error=$consumptionError;ledger_cap_exhaustions=0;first_evidence=$first;second_evidence=$second;first_save=$firstSave;second_save=$secondSave}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $report 'receipt.json') -Encoding utf8
$result | ConvertTo-Json
