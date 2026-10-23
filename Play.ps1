[CmdletBinding(SupportsShouldProcess)]
param(
    [ValidateRange(0, 3600)][double]$Duration = 0,
    [ValidateSet("Smalltrotter","Thistletail","Brackenback","Reedcrest","Longstride")][string]$HeroSpecies = "Smalltrotter",
    [switch]$PartialPersistence,[string]$ConnectedLoad='',[string]$ConnectedSave='',
    [switch]$Proxy
)
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -lt 7) {
    throw 'Use PowerShell 7: pwsh -NoProfile -File .\Play.ps1, or double-click Play.cmd.'
}
$launcher = Join-Path $PSScriptRoot 'stalthos-runtime/scripts/Run-ConnectedWorld.ps1'
$executable = Join-Path $PSScriptRoot 'var/build/stalthos-runtime/Release/Stalthos.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Game executable missing. Build with pwsh -File stalthos-runtime/scripts/Build.ps1 first. See PLAY.md.'
}
Write-Host 'The Stalthosian Archipelago - connected working draft'
Write-Host 'W/S move; A/D turn. F1 detach/return; detached WASD pans the camera.'
Write-Host 'Left-drag orbit; wheel zoom; V inspect; right-click nearby ground to travel.'
Write-Host 'E interact; F disturb a nearby eligible sapling; N next life after death; Esc exit.'
Write-Host 'Adult Smalltrotter: B invites a nearby calm adult mate; an actual response permits a clutch and independent young.'
Write-Host "Selected hero: $HeroSpecies. Thistletail is a feathered biped; Brackenback is armored; Reedcrest has reciprocal peers; Longstride is a biped carnivore with finite authored carrion."
if(-not ($PartialPersistence -or $ConnectedLoad -or $ConnectedSave)){Write-Host 'Left alone, the animal follows its ordinary needs and explores.'}
if($PartialPersistence -or $ConnectedLoad -or $ConnectedSave){Write-Host 'PARTIAL PERSISTENCE: manual E contact / R rest / F5 save; no social links, courtship, exploration or automatic hero behavior. Finish destinations and release controls to save. Restart with ConnectedLoad to reload.'}else{Write-Host 'Ordinary connected sessions start fresh and do not save. See PLAY.md for the separate partial persistence profile.'}
$options = @{ Proxy = [bool]$Proxy; HeroSpecies=$HeroSpecies }
if($PartialPersistence -or $ConnectedLoad -or $ConnectedSave){$options.ConnectedPersistence=$true;if($ConnectedLoad){$options.ConnectedLoad=$ConnectedLoad;if(-not $PSBoundParameters.ContainsKey('HeroSpecies')){$options.Remove('HeroSpecies')}};if($ConnectedSave){$options.ConnectedSave=$ConnectedSave}}
if ($Duration -gt 0) { $options.Duration = $Duration } else { $options.Interactive = $true }
if ($PSCmdlet.ShouldProcess($executable, 'Launch connected world')) {
    & $launcher @options
}
