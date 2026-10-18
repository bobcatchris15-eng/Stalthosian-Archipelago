$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$report=Join-Path $root 'var/reports/regional-habitat-admission-20261018'
$common=@{PopulatedWorld=$true;CameraDistance=16;Creature=(Join-Path $root 'var/bristle-coherence-parent-verified-20261013/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb');CreatureAnimationMetadata=(Join-Path $root 'var/bristle-coherence-parent-verified-20261013/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.json');CreatureSoleOffset=0;Plant=(Join-Path $root 'var/resource-contact-plant/plant.basketfern.fn2.fixture.glb')}
$runs=@{}
foreach($name in @('departure','dormant','return','camera')) {
    $options=$common.Clone()
    if($name -eq 'departure') {$options.Duration=16;$options.PopulatedDeparture=$true}
    if($name -eq 'dormant') {$options.Duration=4;$options.LoadState=Join-Path $report 'activation-departure.stl'}
    if($name -eq 'return') {$options.Duration=16;$options.LoadState=Join-Path $report 'activation-dormant.stl';$options.PopulatedTraversal=$true}
    if($name -eq 'camera') {$options.Duration=10;$options.DiagnosticCamera=$true}
    $options.SaveState=Join-Path $report "activation-$name.stl"
    $lines=@(& (Join-Path $root 'stalthos-runtime/scripts/Run.ps1') @options)
    $lines | Set-Content (Join-Path $report "activation-$name.log")
    $runs[$name]=(($lines | Where-Object {$_ -match '^Process [0-9]+; evidence '} | Select-Object -First 1) -replace '^Process [0-9]+; evidence ','')
}
$runs | ConvertTo-Json | Set-Content (Join-Path $report 'activation-runs.json')
& python (Join-Path $root 'stalthos-runtime/scripts/verify_population_activation.py') --departure $runs.departure --dormant $runs.dormant --return $runs.return --camera $runs.camera --require-skins
if($LASTEXITCODE -ne 0) {throw 'Native activation regression failed'}
$runs | ConvertTo-Json | Set-Content (Join-Path $report 'activation-runs.json')
