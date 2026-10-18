[CmdletBinding()]
param([double]$Duration = 15, [string]$Plant = '', [string]$Creature = '', [double]$CalendarScale = 1, [switch]$Interactive,
      [string]$LoadState = '', [string]$SaveState = '', [switch]$DiagnosticWalk, [switch]$DiagnosticNeeds, [string]$SuccessorId = '',
      [double]$CreatureSoleOffset = .652438, [string]$CreatureAnimationMetadata = '', [switch]$DiagnosticReverseWalk,
      [switch]$InspectionCamera, [double]$CameraDistance = 8, [switch]$DiagnosticCamera, [switch]$DisableTerrainSupport,
      [switch]$DiagnosticTransition, [string]$WoodlandAdultTree = '', [string]$WoodlandSaplingTree = '', [switch]$PopulatedWorld,
      [switch]$TerrainStreamProbe, [switch]$WatershedJoinProbe, [switch]$PopulatedTraversal)
$ErrorActionPreference = 'Stop'
$diagnosticModes = @($DiagnosticWalk, $DiagnosticReverseWalk, $DiagnosticNeeds, $DiagnosticTransition) | Where-Object { $_ }
if ($diagnosticModes.Count -gt 1) {
    throw 'Conflicting diagnostic modes specified: only one of DiagnosticWalk, DiagnosticReverseWalk, DiagnosticNeeds, or DiagnosticTransition may be active'
}
if ($DiagnosticTransition -and -not $PSBoundParameters.ContainsKey('Duration')) { $Duration = 10 }
if ($DiagnosticTransition -and (-not [double]::IsFinite($Duration) -or $Duration -lt 10)) { throw 'DiagnosticTransition requires at least ten seconds' }
if ($DiagnosticTransition -and -not $Creature) { throw 'DiagnosticTransition requires a two-clip Creature' }
if ($InspectionCamera -and -not $PSBoundParameters.ContainsKey('CameraDistance')) { $CameraDistance = 3 }
if (-not [double]::IsFinite($CameraDistance) -or $CameraDistance -lt 2 -or $CameraDistance -gt 18) { throw 'CameraDistance must be 2..18 meters' }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$exe = Join-Path $root 'var/build/stalthos-runtime/Release/Stalthos.exe'
if (-not (Test-Path $exe)) { throw 'Runtime executable absent; run Build.ps1' }
if (-not [double]::IsFinite($CalendarScale) -or $CalendarScale -lt 0 -or $CalendarScale -gt 10000) { throw 'CalendarScale must be 0..10000' }
if (-not [double]::IsFinite($CreatureSoleOffset) -or $CreatureSoleOffset -lt 0 -or $CreatureSoleOffset -gt 5) { throw 'CreatureSoleOffset must be 0..5 meters' }
if ($DiagnosticNeeds -and -not $Plant) { throw 'DiagnosticNeeds requires Plant so browsing eligibility is tested against imported shrubs' }
if ($Creature -and -not $CreatureAnimationMetadata) { throw 'Creature requires CreatureAnimationMetadata with measured duration_s and stride_length_m' }
$walkDuration = 0.0; $strideLength = 0.0
if ($Creature) {
    $metadataPath = (Resolve-Path -LiteralPath $CreatureAnimationMetadata -ErrorAction Stop).Path
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    $walk = if ($metadata.walk_cycle) { $metadata.walk_cycle } else { $metadata }
    $walkDuration = [double]$walk.duration_s
    $strideLength = [double]$walk.stride_length_m
    if (-not $walk.stride_length_m -and $walk.terrain_contact_height) { $strideLength = [double]$walk.terrain_contact_height.stride_length_m }
    if (-not [double]::IsFinite($walkDuration) -or $walkDuration -le 0 -or -not [double]::IsFinite($strideLength) -or $strideLength -le 0) {
        throw 'CreatureAnimationMetadata must provide finite positive duration_s and stride_length_m (directly or under terrain_contact_height)'
    }
}
if ($SuccessorId) {
    [UInt64]$parsedSuccessorId = 0
    if ($SuccessorId -notmatch '^[0-9]+$' -or -not [UInt64]::TryParse($SuccessorId, [ref]$parsedSuccessorId) -or $parsedSuccessorId -eq 0) {
        throw 'SuccessorId must be an unsigned decimal identity greater than zero'
    }
    if (-not $LoadState) { throw 'SuccessorId requires LoadState of a save with recorded hero death' }
}
$woodlandAdultPath = ''; $woodlandSaplingPath = ''
if ($WoodlandAdultTree) {
    $woodlandAdultPath = (Resolve-Path -LiteralPath $WoodlandAdultTree -ErrorAction Stop).Path
    if (-not (Test-Path -LiteralPath $woodlandAdultPath -PathType Leaf)) { throw "WoodlandAdultTree file not found: $WoodlandAdultTree" }
}
if ($WoodlandSaplingTree) {
    $woodlandSaplingPath = (Resolve-Path -LiteralPath $WoodlandSaplingTree -ErrorAction Stop).Path
    if (-not (Test-Path -LiteralPath $woodlandSaplingPath -PathType Leaf)) { throw "WoodlandSaplingTree file not found: $WoodlandSaplingTree" }
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
if ($WoodlandAdultTree) { $start.ArgumentList.Add('--woodland-adult-tree'); $start.ArgumentList.Add($woodlandAdultPath) }
if ($WoodlandSaplingTree) { $start.ArgumentList.Add('--woodland-sapling-tree'); $start.ArgumentList.Add($woodlandSaplingPath) }
if ($Creature) { $start.ArgumentList.Add('--creature'); $start.ArgumentList.Add((Resolve-Path $Creature).Path) }
if ($Creature) { $start.ArgumentList.Add('--walk-duration'); $start.ArgumentList.Add($walkDuration.ToString([Globalization.CultureInfo]::InvariantCulture)); $start.ArgumentList.Add('--stride-length'); $start.ArgumentList.Add($strideLength.ToString([Globalization.CultureInfo]::InvariantCulture)) }
if ($CalendarScale -ne 1) { $start.ArgumentList.Add('--calendar-scale'); $start.ArgumentList.Add($CalendarScale.ToString([Globalization.CultureInfo]::InvariantCulture)) }
if ($Creature) { $start.ArgumentList.Add('--creature-sole-offset'); $start.ArgumentList.Add($CreatureSoleOffset.ToString([Globalization.CultureInfo]::InvariantCulture)) }
if ($Interactive) { $start.ArgumentList.Add('--interactive') }
if ($DisableTerrainSupport) { $start.ArgumentList.Add('--disable-terrain-support') }
if ($InspectionCamera) { $start.ArgumentList.Add('--inspection-camera') }
$start.ArgumentList.Add('--camera-distance'); $start.ArgumentList.Add($CameraDistance.ToString([Globalization.CultureInfo]::InvariantCulture))
if ($DiagnosticWalk) { $start.ArgumentList.Add('--diagnostic-walk') }
if ($DiagnosticReverseWalk) { $start.ArgumentList.Add('--diagnostic-reverse-walk') }
if ($DiagnosticNeeds) { $start.ArgumentList.Add('--diagnostic-needs') }
if ($DiagnosticTransition) { $start.ArgumentList.Add('--diagnostic-transition') }
if ($PopulatedWorld) { $start.ArgumentList.Add('--populated-world') }
if ($PopulatedTraversal) { $start.ArgumentList.Add('--populated-traversal') }
if ($TerrainStreamProbe) { $start.ArgumentList.Add('--terrain-stream-probe') }
if ($WatershedJoinProbe) { $start.ArgumentList.Add('--watershed-join-probe') }
if ($DiagnosticCamera) { $start.ArgumentList.Add('--diagnostic-camera') }
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
[ordered]@{process_id=$process.Id;executable=$exe;executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant();evidence=$output;interactive=[bool]$Interactive;requested_seconds=$Duration;walk_metadata=$metadataPath;walk_duration_s=$walkDuration;walk_stride_length_m=$strideLength;woodland_adult_tree=$woodlandAdultPath;woodland_sapling_tree=$woodlandSaplingPath} | ConvertTo-Json | Set-Content (Join-Path $output 'launch.json') -Encoding utf8
Write-Output "Process $($process.Id); evidence $output"
if (-not $Interactive) {
    if (-not $process.WaitForExit([int](($Duration+60)*1000))) { $process.Kill(); throw "Bounded runtime did not exit; evidence retained at $output" }
    if ($process.ExitCode -ne 0) { throw "Runtime exited $($process.ExitCode); evidence retained at $output" }
    Get-Content (Join-Path $output 'runtime-receipt.json') -Raw
}
