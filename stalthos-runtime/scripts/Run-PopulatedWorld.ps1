[CmdletBinding()]
param([double]$Duration = 30, [switch]$Interactive, [double]$CalendarScale = 1,
      [string]$LoadState = '', [string]$SaveState = '')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$options = @{ PopulatedWorld=$true; Duration=$Duration; CameraDistance=16; CalendarScale=$CalendarScale }
if ($Interactive) { $options.Interactive=$true }
if ($LoadState) { $options.LoadState=$LoadState }
if ($SaveState) { $options.SaveState=$SaveState }
# Local draft assets are optional. The runtime supplies simple population proxies.
$creature = Join-Path $root 'var/bristle-coherence-parent-verified-20261013/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb'
$metadata = [IO.Path]::ChangeExtension($creature, '.json')
if ((Test-Path -LiteralPath $creature) -and (Test-Path -LiteralPath $metadata)) {
    $options.Creature=$creature; $options.CreatureAnimationMetadata=$metadata; $options.CreatureSoleOffset=0
}
foreach ($asset in @(
    @('Plant', 'var/resource-contact-plant/plant.basketfern.fn2.fixture.glb'),
    @('WoodlandAdultTree', 'var/splitcrown-crown-parent-young_adult-20261013/exports/plant.splitcrown.ar1.young_adult.baked.glb'),
    @('WoodlandSaplingTree', 'var/splitcrown-crown-parent-sapling-20261013/exports/plant.splitcrown.ar1.sapling.baked.glb')
)) {
    $path = Join-Path $root $asset[1]
    if (Test-Path -LiteralPath $path) { $options[$asset[0]]=$path }
}
& (Join-Path $PSScriptRoot 'Run.ps1') @options
