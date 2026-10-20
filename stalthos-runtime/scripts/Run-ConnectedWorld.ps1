[CmdletBinding()]
param([double]$Duration=30,[switch]$Interactive,[switch]$Proxy,[double]$CameraDistance=5,
      [string]$AssetRoot='', [switch]$AutonomyProbe,[switch]$ExteriorProbe,[switch]$WoodlandProbe,[switch]$LifeStageProbe,[switch]$DeathNextLifeProbe)
$ErrorActionPreference='Stop'
if($DeathNextLifeProbe -and ($LifeStageProbe -or $AutonomyProbe -or $ExteriorProbe -or $WoodlandProbe -or $Interactive)){throw 'Death/next-life probe requires standalone timed mode'}
if($LifeStageProbe -and ($AutonomyProbe -or $ExteriorProbe -or $WoodlandProbe -or $Interactive)){throw 'Life stage probe requires timed standalone mode'}
if($WoodlandProbe -and ($AutonomyProbe -or $ExteriorProbe -or $Interactive)){throw 'Woodland probe requires timed standalone mode'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($AutonomyProbe -and $ExteriorProbe) {throw 'Select one embodied diagnostic at a time'}
$options=@{ConnectedWorld=$true;Duration=$Duration;CameraDistance=$CameraDistance}
if($LifeStageProbe){$options.LifeStageProbe=$true}
if($DeathNextLifeProbe){$options.DeathNextLifeProbe=$true}
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
        Write-Output 'Connected embodiment: pinned reviewed Smalltrotter draft, existing metadata/pace, sole offset0'
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
