[CmdletBinding()]
param([ValidateSet("Smalltrotter","Thistletail","Brackenback","Reedcrest","Longstride")][string]$HeroSpecies="Smalltrotter",[switch]$CarrionProbe,[switch]$CarrionOverrideProbe,[switch]$CarrionCameraProbe,[switch]$ReedPeerProbe,[switch]$ReedLeadingProbe,[switch]$ReedCameraProbe,[switch]$ReedNeedProbe,[switch]$ReedSeparationProbe,[switch]$ProtectionProbe,[switch]$ProtectionOverrideProbe,[switch]$ProtectionCameraProbe,[switch]$BirdProbe,[switch]$BirdOverrideProbe,[switch]$BirdCameraProbe,[double]$Duration = 15, [string]$Plant = '', [string]$Creature = '', [double]$CalendarScale = 1, [switch]$Interactive,
      [string]$LoadState = '', [string]$SaveState = '', [switch]$DiagnosticWalk, [switch]$DiagnosticNeeds, [string]$SuccessorId = '',
      [double]$CreatureSoleOffset = .652438, [string]$CreatureAnimationMetadata = '', [switch]$DiagnosticReverseWalk,
      [switch]$InspectionCamera, [double]$CameraDistance = 8, [switch]$DiagnosticCamera, [switch]$DisableTerrainSupport,
      [switch]$DiagnosticTransition, [string]$WoodlandAdultTree = '', [string]$WoodlandSaplingTree = '', [switch]$PopulatedWorld,
      [switch]$TerrainStreamProbe, [switch]$WatershedJoinProbe, [switch]$PopulatedTraversal, [switch]$PopulatedDeparture, [switch]$PopulatedResourceProbe, [switch]$ConnectedWorld, [switch]$ConnectedRouteWalk, [switch]$WeatherPreview, [switch]$HeroAutonomyProbe, [switch]$HeroAutonomyOverrideProbe, [switch]$HeroAutonomyCameraProbe, [switch]$ConnectedEmbodimentProbe, [switch]$WildlifeAwarenessProbe, [switch]$WildlifeCameraProbe, [switch]$ExteriorResidentsProbe, [switch]$HeroForagingProbe, [switch]$HeroForagingOverrideProbe, [switch]$HeroForagingCameraProbe, [switch]$HeroRestProbe, [switch]$HeroRestOverrideProbe, [switch]$ExposureProbe, [switch]$DestinationProbe, [switch]$DestinationOverrideProbe, [switch]$DestinationBoundaryProbe, [switch]$ExteriorBrowseProbe, [switch]$ExteriorBrowseCameraProbe, [switch]$ExteriorWaterProbe, [switch]$ExteriorWaterCameraProbe, [switch]$ExteriorWaterOverrideProbe, [switch]$DayNightPreview, [switch]$NightControlsProbe, [switch]$ExteriorWoodlandProbe, [switch]$PeerProbe, [switch]$PeerCameraProbe, [switch]$LifeStageProbe,[switch]$DeathNextLifeProbe,[switch]$ExplorationProbe,[switch]$ExplorationOverrideProbe,[switch]$ExplorationCameraProbe,[switch]$SaplingProbe,[switch]$SaplingCameraProbe,[switch]$BodyAudioProbe,[switch]$BodyAudioCameraProbe,[switch]$NpcPeerProbe,[switch]$NpcPeerCameraProbe,[switch]$NpcPeerNeedProbe,[switch]$ShoreRouteProbe,[switch]$ShoreEntryProbe,[switch]$ShoreCameraProbe,[switch]$ShoreStillProbe,[switch]$HatchProbe,[switch]$HatchCameraProbe,[switch]$NestProbe,[switch]$CarnivoreProbe,[switch]$CarnivoreCancelProbe,[switch]$CarnivoreCameraProbe)
$ErrorActionPreference = 'Stop'
if($CarrionProbe -or $CarrionOverrideProbe -or $CarrionCameraProbe){if(@($CarrionProbe,$CarrionOverrideProbe,$CarrionCameraProbe).Where({$_}).Count -ne 1){throw 'Choose one carrion diagnostic'};$ConnectedWorld=$true;$HeroSpecies='Longstride'}
if($ReedPeerProbe -or $ReedLeadingProbe -or $ReedCameraProbe -or $ReedNeedProbe -or $ReedSeparationProbe){if(@($ReedPeerProbe,$ReedLeadingProbe,$ReedCameraProbe,$ReedNeedProbe,$ReedSeparationProbe).Where({$_}).Count -ne 1){throw 'Choose one Reedcrest diagnostic'};$ConnectedWorld=$true;$HeroSpecies='Reedcrest'}
if($ProtectionProbe -or $ProtectionOverrideProbe -or $ProtectionCameraProbe){if(@($ProtectionProbe,$ProtectionOverrideProbe,$ProtectionCameraProbe).Where({$_}).Count -ne 1){throw 'Choose one protection diagnostic'};$ConnectedWorld=$true;$HeroSpecies='Brackenback'}
if($BirdProbe -or $BirdOverrideProbe -or $BirdCameraProbe){if(@($BirdProbe,$BirdOverrideProbe,$BirdCameraProbe).Where({$_}).Count -ne 1){throw 'Choose one bird diagnostic'};$ConnectedWorld=$true;$HeroSpecies='Thistletail'}
if($HeroSpecies -ne 'Smalltrotter' -and -not ($ConnectedWorld -or $ConnectedRouteWalk)){throw 'Selected nondefault hero is implemented in connected mode only'}
if($HatchProbe -or $HatchCameraProbe){if($HatchProbe -and $HatchCameraProbe){throw 'Choose one hatch diagnostic'};$ConnectedWorld=$true}
if($CarnivoreProbe -or $CarnivoreCancelProbe -or $CarnivoreCameraProbe){$ConnectedWorld=$true}
if($NestProbe){$ConnectedWorld=$true;if($Interactive){throw 'Nest probe requires standalone timed mode'}}
if($ShoreRouteProbe -or $ShoreEntryProbe -or $ShoreCameraProbe -or $ShoreStillProbe){$ConnectedWorld=$true}
if($NpcPeerProbe -or $NpcPeerCameraProbe -or $NpcPeerNeedProbe){$ConnectedWorld=$true}
if($BodyAudioProbe -or $BodyAudioCameraProbe){$ConnectedWorld=$true}
if($SaplingProbe -or $SaplingCameraProbe){$ConnectedWorld=$true}
if($ExplorationProbe -or $ExplorationOverrideProbe -or $ExplorationCameraProbe){$ConnectedWorld=$true}
if($DeathNextLifeProbe){$ConnectedWorld=$true}
if($LifeStageProbe){$ConnectedWorld=$true}
if($PeerProbe -or $PeerCameraProbe){$ConnectedWorld=$true}
if($ExteriorWoodlandProbe){$ConnectedWorld=$true}
if($DayNightPreview -or $NightControlsProbe){$ConnectedWorld=$true}
if($ExteriorWaterProbe -or $ExteriorWaterCameraProbe -or $ExteriorWaterOverrideProbe){$ConnectedWorld=$true}
if($ExteriorBrowseProbe -or $ExteriorBrowseCameraProbe){$ConnectedWorld=$true}
if($DestinationProbe -or $DestinationOverrideProbe -or $DestinationBoundaryProbe){$ConnectedWorld=$true}
if($ExposureProbe){$ConnectedWorld=$true}
if($HeroRestProbe -or $HeroRestOverrideProbe) {$ConnectedWorld=$true}
if($WildlifeAwarenessProbe -or $WildlifeCameraProbe) {$ConnectedWorld=$true}
if($HeroForagingProbe -or $HeroForagingOverrideProbe -or $HeroForagingCameraProbe) {$ConnectedWorld=$true}
if($ExteriorResidentsProbe) {$ConnectedWorld=$true}
if($ConnectedEmbodimentProbe) {$ConnectedWorld=$true}
if($HeroAutonomyProbe -or $HeroAutonomyOverrideProbe -or $HeroAutonomyCameraProbe) {$ConnectedWorld=$true}
if($WeatherPreview) {$ConnectedWorld=$true}
if($ConnectedWorld -or $ConnectedRouteWalk) {$PopulatedWorld=$true}
if(($ConnectedWorld -or $ConnectedRouteWalk) -and ($LoadState -or $SaveState)) {throw 'Connected world preview does not support save/load; existing saves use original populated mode'}
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
if ($ConnectedWorld) { $start.ArgumentList.Add('--connected-world') }
if ($ConnectedRouteWalk) { $start.ArgumentList.Add('--connected-route-walk') }
if ($WeatherPreview) { $start.ArgumentList.Add('--weather-preview') }
if ($HeroAutonomyProbe) { $start.ArgumentList.Add('--hero-autonomy-probe') }
if ($HeroAutonomyOverrideProbe) { $start.ArgumentList.Add('--hero-autonomy-override-probe') }
if ($HeroAutonomyCameraProbe) { $start.ArgumentList.Add('--hero-autonomy-camera-probe') }
if ($ConnectedEmbodimentProbe) { $start.ArgumentList.Add('--connected-embodiment-probe') }
if ($ExteriorResidentsProbe) { $start.ArgumentList.Add('--exterior-residents-probe') }
if ($WildlifeAwarenessProbe) { $start.ArgumentList.Add('--wildlife-awareness-probe') }
if($ConnectedWorld -or $ConnectedRouteWalk){$start.ArgumentList.Add('--hero-species');$start.ArgumentList.Add($HeroSpecies)}
if($CarrionProbe){$start.ArgumentList.Add('--carrion-probe')}
if($CarrionOverrideProbe){$start.ArgumentList.Add('--carrion-override-probe')}
if($CarrionCameraProbe){$start.ArgumentList.Add('--carrion-camera-probe')}
if($ReedPeerProbe){$start.ArgumentList.Add('--reed-peer-probe')}
if($ReedLeadingProbe){$start.ArgumentList.Add('--reed-leading-probe')}
if($ReedCameraProbe){$start.ArgumentList.Add('--reed-camera-probe')}
if($ReedNeedProbe){$start.ArgumentList.Add('--reed-need-probe')}
if($ReedSeparationProbe){$start.ArgumentList.Add('--reed-separation-probe')}
if($ProtectionProbe){$start.ArgumentList.Add('--protection-probe')}
if($ProtectionOverrideProbe){$start.ArgumentList.Add('--protection-override-probe')}
if($ProtectionCameraProbe){$start.ArgumentList.Add('--protection-camera-probe')}
if($BirdProbe){$start.ArgumentList.Add('--bird-probe')}
if($BirdOverrideProbe){$start.ArgumentList.Add('--bird-override-probe')}
if($BirdCameraProbe){$start.ArgumentList.Add('--bird-camera-probe')}
if($HatchProbe){$start.ArgumentList.Add('--hatch-probe')}
if($HatchCameraProbe){$start.ArgumentList.Add('--hatch-camera-probe')}
if($NestProbe){$start.ArgumentList.Add('--nest-probe')}
if($LifeStageProbe){$start.ArgumentList.Add('--life-stage-probe')}
if($DeathNextLifeProbe){$start.ArgumentList.Add('--death-next-life-probe')}
if($CarnivoreProbe){$start.ArgumentList.Add('--carnivore-probe')}
if($CarnivoreCancelProbe){$start.ArgumentList.Add('--carnivore-cancel-probe')}
if($CarnivoreCameraProbe){$start.ArgumentList.Add('--carnivore-camera-probe')}
if($ShoreStillProbe){$start.ArgumentList.Add('--shore-still-probe')}
if($ShoreRouteProbe){$start.ArgumentList.Add('--shore-route-probe')}
if($ShoreEntryProbe){$start.ArgumentList.Add('--shore-entry-probe')}
if($ShoreCameraProbe){$start.ArgumentList.Add('--shore-camera-probe')}
if($NpcPeerProbe){$start.ArgumentList.Add('--npc-peer-probe')}
if($NpcPeerCameraProbe){$start.ArgumentList.Add('--npc-peer-camera-probe')}
if($NpcPeerNeedProbe){$start.ArgumentList.Add('--npc-peer-need-probe')}
if($BodyAudioProbe){$start.ArgumentList.Add('--body-audio-probe')}
if($BodyAudioCameraProbe){$start.ArgumentList.Add('--body-audio-camera-probe')}
if($SaplingProbe){$start.ArgumentList.Add('--sapling-probe')}
if($SaplingCameraProbe){$start.ArgumentList.Add('--sapling-camera-probe')}
if($ExplorationProbe){$start.ArgumentList.Add('--exploration-probe')}
if($ExplorationOverrideProbe){$start.ArgumentList.Add('--exploration-override-probe')}
if($ExplorationCameraProbe){$start.ArgumentList.Add('--exploration-camera-probe')}
if($PeerProbe){$start.ArgumentList.Add('--peer-probe')}
if($PeerCameraProbe){$start.ArgumentList.Add('--peer-camera-probe')}
if($ExteriorWoodlandProbe){$start.ArgumentList.Add('--exterior-woodland-probe')}
if($DayNightPreview){$start.ArgumentList.Add('--day-night-preview')}
if($NightControlsProbe){$start.ArgumentList.Add('--night-controls-probe')}
if($ExteriorWaterProbe){$start.ArgumentList.Add('--exterior-water-probe')}
if($ExteriorWaterCameraProbe){$start.ArgumentList.Add('--exterior-water-camera-probe')}
if($ExteriorWaterOverrideProbe){$start.ArgumentList.Add('--exterior-water-override-probe')}
if($ExteriorBrowseProbe){$start.ArgumentList.Add('--exterior-browse-probe')}
if($ExteriorBrowseCameraProbe){$start.ArgumentList.Add('--exterior-browse-camera-probe')}
if($DestinationProbe){$start.ArgumentList.Add('--destination-probe')}
if($DestinationOverrideProbe){$start.ArgumentList.Add('--destination-override-probe')}
if($DestinationBoundaryProbe){$start.ArgumentList.Add('--destination-boundary-probe')}
if($ExposureProbe){$start.ArgumentList.Add('--exposure-probe')}
if($HeroRestProbe){$start.ArgumentList.Add('--hero-rest-probe')}
if($HeroRestOverrideProbe){$start.ArgumentList.Add('--hero-rest-override-probe')}
if ($HeroForagingProbe) {$start.ArgumentList.Add('--hero-foraging-probe')}
if ($HeroForagingOverrideProbe) {$start.ArgumentList.Add('--hero-foraging-override-probe')}
if ($HeroForagingCameraProbe) {$start.ArgumentList.Add('--hero-foraging-camera-probe')}
if ($WildlifeCameraProbe) { $start.ArgumentList.Add('--wildlife-camera-probe') }
if ($PopulatedResourceProbe) { $start.ArgumentList.Add('--populated-resource-probe') }
if ($PopulatedTraversal) { $start.ArgumentList.Add('--populated-traversal') }
if ($PopulatedDeparture) { $start.ArgumentList.Add('--populated-departure') }
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
