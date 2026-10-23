[CmdletBinding()]
param([switch]$ParentCareProbe,[switch]$ParentCareCameraProbe,[switch]$ConnectedPersistence,[switch]$ConnectedPersistenceProbe,[switch]$ConnectedPersistenceCameraProbe,[string]$ConnectedLoad='',[string]$ConnectedSave='',[ValidateSet("Smalltrotter","Thistletail","Brackenback","Reedcrest","Longstride")][string]$HeroSpecies="Smalltrotter",[switch]$CourtshipProbe,[switch]$CourtshipOverrideProbe,[switch]$CourtshipCameraProbe,[switch]$CourtshipRefusalProbe,[switch]$CarrionProbe,[switch]$CarrionOverrideProbe,[switch]$CarrionCameraProbe,[switch]$ReedPeerProbe,[switch]$ReedLeadingProbe,[switch]$ReedCameraProbe,[switch]$ReedNeedProbe,[switch]$ReedSeparationProbe,[switch]$ProtectionProbe,[switch]$ProtectionOverrideProbe,[switch]$ProtectionCameraProbe,[switch]$BirdProbe,[switch]$BirdOverrideProbe,[switch]$BirdCameraProbe,[double]$Duration=30,[double]$CalendarScale=1,[switch]$Interactive,[switch]$Proxy,[double]$CameraDistance=5,
      [string]$AssetRoot='', [switch]$AutonomyProbe,[switch]$ExteriorProbe,[switch]$WoodlandProbe,[switch]$HatchProbe,[switch]$HatchCameraProbe,[switch]$NestProbe,[switch]$LifeStageProbe,[switch]$DeathNextLifeProbe,[switch]$ExplorationProbe,[switch]$ExplorationOverrideProbe,[switch]$ExplorationCameraProbe,[switch]$SaplingProbe,[switch]$SaplingCameraProbe,[switch]$BodyAudioProbe,[switch]$BodyAudioCameraProbe,[switch]$NpcPeerProbe,[switch]$NpcPeerCameraProbe,[switch]$NpcPeerNeedProbe,[switch]$ShoreRouteProbe,[switch]$ShoreEntryProbe,[switch]$ShoreCameraProbe,[switch]$ShoreStillProbe)
$ErrorActionPreference='Stop'
if($ParentCareProbe -or $ParentCareCameraProbe){$HeroSpecies='Longstride'}
if($CarrionProbe -or $CarrionOverrideProbe -or $CarrionCameraProbe){$HeroSpecies='Longstride'}
if($ReedPeerProbe -or $ReedLeadingProbe -or $ReedCameraProbe -or $ReedNeedProbe -or $ReedSeparationProbe){$HeroSpecies='Reedcrest'}
if($ProtectionProbe -or $ProtectionOverrideProbe -or $ProtectionCameraProbe){$HeroSpecies='Brackenback'}
if($BirdProbe -or $BirdOverrideProbe -or $BirdCameraProbe){$HeroSpecies='Thistletail'}
if(($BodyAudioProbe -or $BodyAudioCameraProbe) -and ($Interactive -or $SaplingProbe -or $SaplingCameraProbe -or $ExplorationProbe -or $ExplorationOverrideProbe -or $ExplorationCameraProbe -or $DeathNextLifeProbe -or $LifeStageProbe -or $WoodlandProbe -or $AutonomyProbe -or $ExteriorProbe -or ($BodyAudioProbe -and $BodyAudioCameraProbe))){throw 'Audio probe requires standalone timed mode'}
if($SaplingProbe -or $SaplingCameraProbe){if($Interactive -or $LifeStageProbe -or $DeathNextLifeProbe -or $AutonomyProbe -or $ExteriorProbe -or $WoodlandProbe -or $ExplorationProbe -or $ExplorationOverrideProbe -or $ExplorationCameraProbe -or ($SaplingProbe -and $SaplingCameraProbe)){throw 'Sapling probe requires standalone timed mode'}}
if($ExplorationProbe -or $ExplorationOverrideProbe -or $ExplorationCameraProbe){if($Interactive -or $LifeStageProbe -or $DeathNextLifeProbe -or $AutonomyProbe -or $ExteriorProbe -or $WoodlandProbe){throw 'Exploration probe requires standalone timed mode'};if(@($ExplorationProbe,$ExplorationOverrideProbe,$ExplorationCameraProbe).Where({$_}).Count -ne 1){throw 'Choose one exploration probe'}}
if($DeathNextLifeProbe -and ($LifeStageProbe -or $AutonomyProbe -or $ExteriorProbe -or $WoodlandProbe -or $Interactive)){throw 'Death/next-life probe requires standalone timed mode'}
if($LifeStageProbe -and ($AutonomyProbe -or $ExteriorProbe -or $WoodlandProbe -or $Interactive)){throw 'Life stage probe requires timed standalone mode'}
if($WoodlandProbe -and ($AutonomyProbe -or $ExteriorProbe -or $Interactive)){throw 'Woodland probe requires timed standalone mode'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($AutonomyProbe -and $ExteriorProbe) {throw 'Select one embodied diagnostic at a time'}
$options=@{ConnectedWorld=$true;Duration=$Duration;CameraDistance=$CameraDistance;CalendarScale=$CalendarScale;HeroSpecies=$HeroSpecies}
if($ConnectedPersistence){$options.ConnectedPersistence=$true}
if($ConnectedPersistenceProbe){$options.ConnectedPersistenceProbe=$true}
if($ConnectedPersistenceCameraProbe){$options.ConnectedPersistenceCameraProbe=$true}
if($ConnectedLoad){$options.ConnectedLoad=$ConnectedLoad;if(-not $PSBoundParameters.ContainsKey('HeroSpecies')){$options.Remove('HeroSpecies')}}
if($ConnectedSave){$options.ConnectedSave=$ConnectedSave}
if($ShoreStillProbe){$options.ShoreStillProbe=$true}
if($ShoreRouteProbe){$options.ShoreRouteProbe=$true}
if($ShoreEntryProbe){$options.ShoreEntryProbe=$true}
if($ShoreCameraProbe){$options.ShoreCameraProbe=$true}
if($NpcPeerProbe){$options.NpcPeerProbe=$true}
if($NpcPeerCameraProbe){$options.NpcPeerCameraProbe=$true}
if($NpcPeerNeedProbe){$options.NpcPeerNeedProbe=$true}
if($ParentCareProbe){$options.ParentCareProbe=$true}
if($ParentCareCameraProbe){$options.ParentCareCameraProbe=$true}
if($CourtshipProbe){$options.CourtshipProbe=$true}
if($CourtshipOverrideProbe){$options.CourtshipOverrideProbe=$true}
if($CourtshipCameraProbe){$options.CourtshipCameraProbe=$true}
if($CourtshipRefusalProbe){$options.CourtshipRefusalProbe=$true}
if($CarrionProbe){$options.CarrionProbe=$true}
if($CarrionOverrideProbe){$options.CarrionOverrideProbe=$true}
if($CarrionCameraProbe){$options.CarrionCameraProbe=$true}
if($ReedPeerProbe){$options.ReedPeerProbe=$true}
if($ReedLeadingProbe){$options.ReedLeadingProbe=$true}
if($ReedCameraProbe){$options.ReedCameraProbe=$true}
if($ReedNeedProbe){$options.ReedNeedProbe=$true}
if($ReedSeparationProbe){$options.ReedSeparationProbe=$true}
if($ProtectionProbe){$options.ProtectionProbe=$true}
if($ProtectionOverrideProbe){$options.ProtectionOverrideProbe=$true}
if($ProtectionCameraProbe){$options.ProtectionCameraProbe=$true}
if($BirdProbe){$options.BirdProbe=$true}
if($BirdOverrideProbe){$options.BirdOverrideProbe=$true}
if($BirdCameraProbe){$options.BirdCameraProbe=$true}
if($HatchProbe){$options.HatchProbe=$true}
if($HatchCameraProbe){$options.HatchCameraProbe=$true}
if($NestProbe){$options.NestProbe=$true}
if($LifeStageProbe){$options.LifeStageProbe=$true}
if($DeathNextLifeProbe){$options.DeathNextLifeProbe=$true}
if($BodyAudioProbe){$options.BodyAudioProbe=$true}
if($BodyAudioCameraProbe){$options.BodyAudioCameraProbe=$true}
if($SaplingProbe){$options.SaplingProbe=$true}
if($SaplingCameraProbe){$options.SaplingCameraProbe=$true}
if($ExplorationProbe){$options.ExplorationProbe=$true}
if($ExplorationOverrideProbe){$options.ExplorationOverrideProbe=$true}
if($ExplorationCameraProbe){$options.ExplorationCameraProbe=$true}
if($WoodlandProbe){$options.ExteriorWoodlandProbe=$true}
if($Interactive) {$options.Interactive=$true}
if($AutonomyProbe) {$options.HeroAutonomyProbe=$true}
if($ExteriorProbe) {$options.ConnectedEmbodimentProbe=$true}
if(-not $Proxy) {
    $pin=Get-Content -LiteralPath (Join-Path $root 'stalthos-runtime/config/connected-creature.json') -Raw | ConvertFrom-Json
    $assets=if($AssetRoot) {[IO.Path]::GetFullPath($AssetRoot)} else {$root}
    $candidateRoot=Join-Path $assets $pin.asset_root
    $creature=Join-Path $candidateRoot $pin.creature
    $metadata=Join-Path $candidateRoot $pin.metadata
    if((Test-Path -LiteralPath $creature -PathType Leaf) -and (Test-Path -LiteralPath $metadata -PathType Leaf)) {
        if((Get-FileHash -LiteralPath $creature -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.creature_sha256 -or
           (Get-FileHash -LiteralPath $metadata -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.metadata_sha256) {
            throw 'Reviewed connected creature artifact changed; verify provenance or use -Proxy explicitly'
        }
        $options.Creature=$creature;$options.CreatureAnimationMetadata=$metadata;$options.CreatureSoleOffset=[double]$pin.sole_offset_m
        if($ConnectedLoad){Write-Output 'Hero species/embodiment: restored from checkpoint; pinned Smalltrotter asset remains available to NPC replicas.'}elseif($HeroSpecies -ne 'Smalltrotter'){Write-Output "Hero: procedural $HeroSpecies. Pinned Smalltrotter asset retained for NPC replicas only."}else{Write-Output 'Connected embodiment: pinned reviewed Smalltrotter draft, existing metadata/pace, sole offset0'}
    } else {
        Write-Warning 'Reviewed local Smalltrotter/metadata unavailable; using explicit announced proxy fallback. Pass -AssetRoot for the repository holding reviewed var artifacts.'
    }
} else {Write-Output 'Connected embodiment: explicit proxy mode'}
$plantPin=Get-Content -LiteralPath (Join-Path $root 'stalthos-runtime/config/connected-plant.json') -Raw|ConvertFrom-Json
$plantAssets=if($AssetRoot){[IO.Path]::GetFullPath($AssetRoot)}else{$root}
$browsePlant=Join-Path $plantAssets $plantPin.path
if(Test-Path -LiteralPath $browsePlant -PathType Leaf){if((Get-FileHash -LiteralPath $browsePlant -Algorithm SHA256).Hash.ToLowerInvariant() -ne $plantPin.sha256){throw 'Reviewed browse plant changed; verify provenance'};$options.Plant=$browsePlant;Write-Output 'Connected foraging: pinned provisional Basketfern browse plant'}else{Write-Warning 'Reviewed browse plant unavailable; no autonomous foraging affordance. Pass -AssetRoot for reviewed var artifacts.'}
$treePin=Get-Content -LiteralPath (Join-Path $root 'stalthos-runtime/config/connected-woodland.json') -Raw|ConvertFrom-Json
foreach($stage in @('adult','sapling')){
 $tree=Join-Path $plantAssets $treePin.$stage.path
 if(Test-Path -LiteralPath $tree -PathType Leaf){
  if((Get-FileHash -LiteralPath $tree -Algorithm SHA256).Hash.ToLowerInvariant() -ne $treePin.$stage.sha256){throw "Pinned $stage woodland artifact changed"}
  $key=if($stage -eq 'adult'){'WoodlandAdultTree'}else{'WoodlandSaplingTree'};$options[$key]=$tree
 }else{Write-Warning "Existing $stage woodland asset missing; that stage will be absent"}
}
& (Join-Path $PSScriptRoot 'Run.ps1') @options
