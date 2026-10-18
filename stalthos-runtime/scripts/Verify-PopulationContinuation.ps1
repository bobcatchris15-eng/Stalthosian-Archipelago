[CmdletBinding()]
param([double]$FirstDuration=8, [double]$SecondDuration=4, [switch]$FirstTraversal)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
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
$first=Invoke-Probe $FirstDuration '' $firstSave (Join-Path $report 'first.log')
$second=Invoke-Probe $SecondDuration $firstSave $secondSave (Join-Path $report 'second.log')
$before=Get-Content -LiteralPath (Join-Path $first 'world-population.json') -Raw | ConvertFrom-Json
$after=Get-Content -LiteralPath (Join-Path $second 'world-population.json') -Raw | ConvertFrom-Json
$firstRuntime=Get-Content -LiteralPath (Join-Path $first 'runtime-receipt.json') -Raw | ConvertFrom-Json
$secondRuntime=Get-Content -LiteralPath (Join-Path $second 'runtime-receipt.json') -Raw | ConvertFrom-Json
if($firstRuntime.resource_ledger_cap_exhaustions -ne 0 -or $secondRuntime.resource_ledger_cap_exhaustions -ne 0) {throw 'Resource ledger exhausted during continuation'}
# Runtime resources_initial is sampled after the first update. Reconstruct the
# activation quantity from final quantity plus this run's complete NPC consumption.
$waterError=[Math]::Abs($firstRuntime.resources_final.creek-($secondRuntime.resources_final.creek+$after.water_consumed))
if($waterError -gt 1e-8) {throw 'Saved creek depletion did not restore exactly'}
$consumptionError=[Math]::Abs(($firstRuntime.resources_final.creek-$secondRuntime.resources_final.creek)-$after.water_consumed)
if($after.water_consumed -le 0 -or $consumptionError -gt 1e-7) {throw 'Resumed creek consumption did not match the shared ledger'}
if($before.schema_version -ne 2 -or $after.schema_version -ne 2 -or $null -eq $before.snapshot_final -or $null -eq $after.snapshot_initial) {throw 'Full population snapshots absent'}
$expected=ConvertTo-Json -InputObject $before.snapshot_final -Depth 10 -Compress
$actual=ConvertTo-Json -InputObject $after.snapshot_initial -Depth 10 -Compress
if($expected -cne $actual) {throw "Population changed across save/load; evidence retained at $report"}
if(-not $after.population_restored -or -not $after.population_persisted) {throw 'Population continuation flags missing'}
$advanced=$false
for($i=0;$i -lt $after.actors_final.Count;$i++) {
    if($after.actors_final[$i].distance_m -gt $after.actors_initial[$i].distance_m) {$advanced=$true}
}
if(-not $advanced) {throw 'No resumed actor advanced along its route'}
$result=[ordered]@{passed=$true;actor_count=$after.actors_initial.Count;exact_initial_state_restored=$true;full_snapshot_fields_verified=$true;
    first_run_traversal=[bool]$FirstTraversal;resumed_motion=$advanced;water_restore_abs_error=$waterError;water_consumption_abs_error=$consumptionError;ledger_cap_exhaustions=0;first_evidence=$first;second_evidence=$second;first_save=$firstSave;second_save=$secondSave}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $report 'receipt.json') -Encoding utf8
$result | ConvertTo-Json
