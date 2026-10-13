[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$AdultWorkspace,
 [Parameter(Mandatory=$true)][string]$SaplingWorkspace,
 [Parameter(Mandatory=$true)][string]$Creature,
 [Parameter(Mandatory=$true)][string]$CreatureAnimationMetadata,
 [string]$Plant='',
 [double]$Duration=5,
 [double]$CameraDistance=8
)
$ErrorActionPreference='Stop'
$adult=Join-Path $AdultWorkspace 'exports/plant.splitcrown.ar1.young_adult.baked.glb'
$sapling=Join-Path $SaplingWorkspace 'exports/plant.splitcrown.ar1.sapling.baked.glb'
foreach($inputPath in @($adult,$sapling,$Creature,$CreatureAnimationMetadata)){
 if(-not (Test-Path -LiteralPath $inputPath -PathType Leaf)){throw "Comparison input absent: $inputPath"}
}
$options=@{
 Duration=$Duration;InspectionCamera=$true;CameraDistance=$CameraDistance;CreatureSoleOffset=0
 Creature=$Creature;CreatureAnimationMetadata=$CreatureAnimationMetadata
 WoodlandAdultTree=$adult;WoodlandSaplingTree=$sapling
}
if($Plant){$options.Plant=$Plant}
& (Join-Path $PSScriptRoot 'Run.ps1') @options
