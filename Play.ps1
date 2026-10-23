[CmdletBinding(SupportsShouldProcess)]
param(
    [ValidateRange(0, 3600)][double]$Duration = 0,
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
Write-Host 'Adult Smalltrotter: B attempts a clutch on dry clear ground; one independent young can hatch.'
Write-Host 'Left alone, the animal follows its ordinary needs and explores.'
Write-Host 'Connected sessions currently start fresh and do not save. See PLAY.md for draft limits.'
$options = @{ Proxy = [bool]$Proxy }
if ($Duration -gt 0) { $options.Duration = $Duration } else { $options.Interactive = $true }
if ($PSCmdlet.ShouldProcess($executable, 'Launch connected world')) {
    & $launcher @options
}
