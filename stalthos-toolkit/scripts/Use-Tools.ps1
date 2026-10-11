<#
.SYNOPSIS
    Use verified project-local gltfpack and Compressonator in this PowerShell process.

.DESCRIPTION
    Dot-source this script to set process-only STEALTHOS_* overrides and prepend
    the verified executable folders to this process's PATH. It never changes the
    user or machine environment.
#>
param(
    [ValidateSet('all', 'gltfpack', 'compressonator')]
    [string]$Tool = 'all',
    [string]$ToolkitRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$InstallRoot = ''
)

$runner = {
param([string]$RequestedTool, [string]$RequestedToolkitRoot, [string]$RequestedInstallRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$pins = @(
    [ordered]@{
        Name = 'gltfpack'; Version = '1.3'; InstallDirectory = 'gltfpack-1.3'
        Executable = 'gltfpack.exe'
        ArchiveSha256 = 'f6e9c09d66af23da3da71b86f0652d2413e81c734e0aecd1cd3d0e6e6e8645c0'
        ExecutableSha256 = 'a082a0db2332908133100332caeeddaa304afcf5e8094adc755ebdea8c720984'
    },
    [ordered]@{
        Name = 'compressonator'; Version = '4.5.52'; InstallDirectory = 'compressonator-4.5.52'
        Executable = 'compressonatorcli-4.5.52-win64\compressonatorcli.exe'
        ArchiveSha256 = '49ca7c85e299fc104878aef82470d731a7c853cfb5facd879e9bc633fb48a444'
        ExecutableSha256 = '68eb6352585ef48471f3eec55926c333e637c1465091128f078bcb0c7ac6179b'
    }
)

$RequestedToolkitRoot = (Resolve-Path -LiteralPath $RequestedToolkitRoot).Path
$repositoryRoot = Split-Path -Parent $RequestedToolkitRoot
if (-not $RequestedInstallRoot) { $RequestedInstallRoot = Join-Path $repositoryRoot 'var\tools' }
$RequestedInstallRoot = [System.IO.Path]::GetFullPath($RequestedInstallRoot)
$selectedPins = if ($RequestedTool -eq 'all') { $pins } else { @($pins | Where-Object { $_.Name -eq $RequestedTool }) }
$verified = New-Object System.Collections.Generic.List[object]

foreach ($pin in $selectedPins) {
    $folder = Join-Path $RequestedInstallRoot $pin.InstallDirectory
    $exe = Join-Path $folder $pin.Executable
    $manifestPath = Join-Path $folder 'stalthos-tool-provenance.json'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf) -or -not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Verified project-local $($pin.Name) $($pin.Version) is not installed. Run scripts\Prepare-Tools.ps1 first."
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $actualExeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($manifest.schema_version -ne 1 -or $manifest.tool -ne $pin.Name -or
        $manifest.version -ne $pin.Version -or
        $manifest.source_archive_sha256 -ne $pin.ArchiveSha256 -or
        $manifest.executable -ne ($pin.Executable -replace '\\', '/') -or
        $manifest.executable_sha256 -ne $pin.ExecutableSha256 -or
        $actualExeHash -ne $pin.ExecutableSha256) {
        throw "Project-local $($pin.Name) provenance or executable hash failed verification. Re-run scripts\Prepare-Tools.ps1 using the pinned local archive."
    }
    $verified.Add([pscustomobject]@{ Name = $pin.Name; Path = $exe; Directory = Split-Path -Parent $exe; Version = $pin.Version; Hash = $actualExeHash })
}

# Mutate only this PowerShell process after every requested tool has verified.
foreach ($item in $verified) {
    $envName = 'STEALTHOS_' + $item.Name.ToUpperInvariant()
    Set-Item -Path ('Env:' + $envName) -Value $item.Path
}
$newDirectories = @($verified | ForEach-Object { $_.Directory } | Select-Object -Unique)
$pathEntries = @($env:PATH -split ';' | Where-Object { $_ })
foreach ($directory in $newDirectories) {
    if (-not ($pathEntries | Where-Object { $_.TrimEnd('\') -ieq $directory.TrimEnd('\') })) {
        $env:PATH = $directory + ';' + $env:PATH
    }
}

@{ ok = $true; process_only = $true; tools = @($verified | ForEach-Object { [ordered]@{ tool = $_.Name; version = $_.Version; executable = $_.Path; sha256 = $_.Hash } }) } | ConvertTo-Json -Depth 5
}

try {
    & $runner $Tool $ToolkitRoot $InstallRoot
} finally {
    Remove-Variable -Name runner -ErrorAction SilentlyContinue
}
