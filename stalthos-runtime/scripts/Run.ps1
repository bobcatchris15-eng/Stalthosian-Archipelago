[CmdletBinding()]
param([double]$Duration = 15, [string]$Plant = '', [string]$Creature = '', [double]$CalendarScale = 1, [switch]$Interactive,
      [string]$LoadState = '', [string]$SaveState = '', [switch]$DiagnosticWalk, [string]$SuccessorId = '',
      [double]$CreatureSoleOffset = .652438)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$exe = Join-Path $root 'var/build/stalthos-runtime/Release/Stalthos.exe'
if (-not (Test-Path $exe)) { throw 'Runtime executable absent; run Build.ps1' }
if (-not [double]::IsFinite($CalendarScale) -or $CalendarScale -lt 0 -or $CalendarScale -gt 10000) { throw 'CalendarScale must be 0..10000' }
if (-not [double]::IsFinite($CreatureSoleOffset) -or $CreatureSoleOffset -lt 0 -or $CreatureSoleOffset -gt 5) { throw 'CreatureSoleOffset must be 0..5 meters' }
if ($SuccessorId) {
    [UInt64]$parsedSuccessorId = 0
    if ($SuccessorId -notmatch '^[0-9]+$' -or -not [UInt64]::TryParse($SuccessorId, [ref]$parsedSuccessorId) -or $parsedSuccessorId -eq 0) {
        throw 'SuccessorId must be an unsigned decimal identity greater than zero'
    }
    if (-not $LoadState) { throw 'SuccessorId requires LoadState of a save with recorded hero death' }
}
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$output = Join-Path $root "var/reports/runtime/$id"
New-Item -ItemType Directory -Force $output | Out-Null
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $exe
$start.WorkingDirectory = Split-Path $exe
$start.UseShellExecute = $false
$start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
foreach ($arg in @('dx12','alwaysactive','--output',$output,'--duration',$Duration.ToString([Globalization.CultureInfo]::InvariantCulture))) { $start.ArgumentList.Add($arg) }
if ($Plant) { $start.ArgumentList.Add('--plant'); $start.ArgumentList.Add((Resolve-Path $Plant).Path) }
if ($Creature) { $start.ArgumentList.Add('--creature'); $start.ArgumentList.Add((Resolve-Path $Creature).Path) }
if ($CalendarScale -ne 1) { $start.ArgumentList.Add('--calendar-scale'); $start.ArgumentList.Add($CalendarScale.ToString([Globalization.CultureInfo]::InvariantCulture)) }
if ($Creature) { $start.ArgumentList.Add('--creature-sole-offset'); $start.ArgumentList.Add($CreatureSoleOffset.ToString([Globalization.CultureInfo]::InvariantCulture)) }
if ($Interactive) { $start.ArgumentList.Add('--interactive') }
if ($DiagnosticWalk) { $start.ArgumentList.Add('--diagnostic-walk') }
if ($SuccessorId) { $start.ArgumentList.Add('--successor-id'); $start.ArgumentList.Add($SuccessorId) }
foreach ($stateOption in @(@('--load-state', $LoadState), @('--save-state', $SaveState))) {
    if (-not $stateOption[1]) { continue }
    $statePath = [IO.Path]::GetFullPath($stateOption[1])
    $stateRoot = [IO.Path]::GetFullPath((Join-Path $root 'var')).TrimEnd('\') + '\'
    if (-not $statePath.StartsWith($stateRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Diagnostic save/load paths must stay inside this project var directory'
    }
    if ($stateOption[0] -eq '--load-state' -and -not (Test-Path -LiteralPath $statePath -PathType Leaf)) {
        throw "Load state is absent: $statePath"
    }
    if ($stateOption[0] -eq '--save-state') {
        New-Item -ItemType Directory -Force (Split-Path $statePath) | Out-Null
    }
    $start.ArgumentList.Add($stateOption[0]); $start.ArgumentList.Add($statePath)
}
$process = [Diagnostics.Process]::Start($start)
[ordered]@{process_id=$process.Id;executable=$exe;executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant();evidence=$output;interactive=[bool]$Interactive;requested_seconds=$Duration} | ConvertTo-Json | Set-Content (Join-Path $output 'launch.json') -Encoding utf8
Write-Output "Process $($process.Id); evidence $output"
if (-not $Interactive) {
    if (-not $process.WaitForExit([int](($Duration+60)*1000))) { $process.Kill(); throw "Bounded runtime did not exit; evidence retained at $output" }
    if ($process.ExitCode -ne 0) { throw "Runtime exited $($process.ExitCode); evidence retained at $output" }
    Get-Content (Join-Path $output 'runtime-receipt.json') -Raw
}
