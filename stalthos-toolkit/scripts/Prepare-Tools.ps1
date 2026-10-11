<#
.SYNOPSIS
    Install or register the pinned portable optimization tools in the checkout's ignored var folder.

.DESCRIPTION
    This script performs no network access and never changes machine or user PATH.
    It verifies the script's pins against local archives and extracted
    executables. New packages are staged before installation; matching existing
    packages without provenance are registered only after all selected packages
    validate. Existing mismatched packages are preserved and rejected.

.PARAMETER SourceToolsRoot
    Root containing downloads\<archive>. Defaults to the main checkout's var\tools
    (including when this toolkit is running in a Git worktree).

.PARAMETER InstallRoot
    Destination for project-local tools. Defaults to repository-root var\tools.
#>
[CmdletBinding()]
param(
    [ValidateSet('all', 'gltfpack', 'compressonator')]
    [string]$Tool = 'all',
    [string]$ToolkitRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$SourceToolsRoot = '',
    [string]$InstallRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$toolPins = @(
    [ordered]@{
        Name = 'gltfpack'; Version = '1.3'; InstallDirectory = 'gltfpack-1.3'
        Archive = 'downloads\gltfpack-windows-1.3.zip'
        ArchiveSha256 = 'f6e9c09d66af23da3da71b86f0652d2413e81c734e0aecd1cd3d0e6e6e8645c0'
        Executable = 'gltfpack.exe'
        ExecutableSha256 = 'a082a0db2332908133100332caeeddaa304afcf5e8094adc755ebdea8c720984'
        Repository = 'https://github.com/zeux/meshoptimizer'
    },
    [ordered]@{
        Name = 'compressonator'; Version = '4.5.52'; InstallDirectory = 'compressonator-4.5.52'
        Archive = 'downloads\compressonatorcli-4.5.52-win64.zip'
        ArchiveSha256 = '49ca7c85e299fc104878aef82470d731a7c853cfb5facd879e9bc633fb48a444'
        Executable = 'compressonatorcli-4.5.52-win64\compressonatorcli.exe'
        ExecutableSha256 = '68eb6352585ef48471f3eec55926c333e637c1465091128f078bcb0c7ac6179b'
        Repository = 'https://github.com/GPUOpen-Tools/compressonator'
    }
)

function Resolve-DefaultSourceToolsRoot {
    param([string]$ResolvedToolkitRoot)
    $checkoutRoot = Split-Path -Parent $ResolvedToolkitRoot
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        try {
            $commonDirectory = (& $git.Source -C $checkoutRoot rev-parse --path-format=absolute --git-common-dir 2>$null | Select-Object -First 1)
            if ($LASTEXITCODE -eq 0 -and $commonDirectory) {
                $mainCheckout = Split-Path -Parent ([string]$commonDirectory).Trim()
                if (Test-Path -LiteralPath (Join-Path $mainCheckout '.git')) {
                    return Join-Path $mainCheckout 'var\tools'
                }
            }
        } catch { }
    }
    return Join-Path $checkoutRoot 'var\tools'
}

$ToolkitRoot = (Resolve-Path -LiteralPath $ToolkitRoot).Path
if (-not $SourceToolsRoot) { $SourceToolsRoot = Resolve-DefaultSourceToolsRoot -ResolvedToolkitRoot $ToolkitRoot }
$SourceToolsRoot = [System.IO.Path]::GetFullPath($SourceToolsRoot)
$repositoryRoot = Split-Path -Parent $ToolkitRoot
if (-not $InstallRoot) { $InstallRoot = Join-Path $repositoryRoot 'var\tools' }
$InstallRoot = [System.IO.Path]::GetFullPath($InstallRoot)

if (-not (Test-Path -LiteralPath $SourceToolsRoot -PathType Container)) {
    throw "Tool source folder does not exist: $SourceToolsRoot. Provide -SourceToolsRoot with the local var\tools cache. This script does not download tools."
}

$selectedPins = if ($Tool -eq 'all') { $toolPins } else { @($toolPins | Where-Object { $_.Name -eq $Tool }) }
$null = New-Item -ItemType Directory -Path $InstallRoot -Force
$transactionId = [guid]::NewGuid().ToString('N')
$staged = New-Object System.Collections.Generic.List[object]
$actions = New-Object System.Collections.Generic.List[object]

try {
    # Validate every archive and installed binary before changing any package.
    foreach ($pin in $selectedPins) {
        $archivePath = Join-Path $SourceToolsRoot $pin.Archive
        if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
            throw "Pinned archive is missing: $archivePath"
        }
        $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($archiveHash -ne $pin.ArchiveSha256) {
            throw "Pinned archive hash mismatch for $($pin.Name): expected $($pin.ArchiveSha256), got $archiveHash"
        }

        $target = Join-Path $InstallRoot $pin.InstallDirectory
        $action = 'install'
        if (Test-Path -LiteralPath $target) {
            if (-not (Test-Path -LiteralPath $target -PathType Container)) {
                throw "Pinned installation path exists but is not a folder: $target"
            }
            $exePath = Join-Path $target $pin.Executable
            if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
                throw "Existing $($pin.Name) folder lacks the pinned executable: $($pin.Executable). Existing contents were preserved."
            }
            $exeHash = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($exeHash -ne $pin.ExecutableSha256) {
                throw "Existing $($pin.Name) executable hash mismatch: expected $($pin.ExecutableSha256), got $exeHash. Existing contents were preserved."
            }
            $manifestPath = Join-Path $target 'stalthos-tool-provenance.json'
            if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
                try { $existingManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json }
                catch { throw "Existing $($pin.Name) provenance is invalid; existing contents were preserved." }
                if ($existingManifest.schema_version -ne 1 -or $existingManifest.tool -ne $pin.Name -or
                    $existingManifest.version -ne $pin.Version -or
                    $existingManifest.source_archive_sha256 -ne $archiveHash -or
                    $existingManifest.executable -ne ($pin.Executable -replace '\\', '/') -or
                    $existingManifest.executable_sha256 -ne $pin.ExecutableSha256) {
                    throw "Existing $($pin.Name) provenance does not match the pins; existing contents were preserved."
                }
                $action = 'verified-existing'
            } else {
                $action = 'registered-existing'
            }
        } else {
            $stagePath = Join-Path $InstallRoot ('.stage-' + $pin.Name + '-' + $transactionId)
            $null = New-Item -ItemType Directory -Path $stagePath
            Expand-Archive -LiteralPath $archivePath -DestinationPath $stagePath -Force

            $exePath = Join-Path $stagePath $pin.Executable
            if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
                throw "Archive for $($pin.Name) did not contain the pinned executable: $($pin.Executable)"
            }
            $exeHash = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($exeHash -ne $pin.ExecutableSha256) {
                throw "Pinned executable hash mismatch for $($pin.Name): expected $($pin.ExecutableSha256), got $exeHash"
            }
        }

        $provenance = [ordered]@{
            schema_version = 1
            tool = $pin.Name
            version = $pin.Version
            repository = $pin.Repository
            source_archive = [System.IO.Path]::GetFileName($archivePath)
            source_archive_sha256 = $archiveHash
            executable = $pin.Executable -replace '\\', '/'
            executable_sha256 = $exeHash
        }
        $json = $provenance | ConvertTo-Json -Depth 5
        $encoding = New-Object System.Text.UTF8Encoding($false)
        if ($action -eq 'install') {
            $provenancePath = Join-Path $stagePath 'stalthos-tool-provenance.json'
            [System.IO.File]::WriteAllText($provenancePath, ($json + "`n"), $encoding)
        } elseif ($action -eq 'registered-existing') {
            $stagePath = Join-Path $InstallRoot ('.stage-' + $pin.Name + '-' + $transactionId)
            $null = New-Item -ItemType Directory -Path $stagePath
            $provenancePath = Join-Path $stagePath 'stalthos-tool-provenance.json'
            [System.IO.File]::WriteAllText($provenancePath, ($json + "`n"), $encoding)
        } else {
            $stagePath = $null
        }
        $staged.Add([pscustomobject]@{ Pin = $pin; Path = $stagePath; ArchiveHash = $archiveHash; Action = $action })
    }

    # Register verified existing binaries or install new verified folders.
    foreach ($item in $staged) {
        if ($item.Action -eq 'verified-existing') { continue }
        $target = Join-Path $InstallRoot $item.Pin.InstallDirectory
        if ($item.Action -eq 'registered-existing') {
            $manifestTarget = Join-Path $target 'stalthos-tool-provenance.json'
            if (Test-Path -LiteralPath $manifestTarget) {
                throw "Provenance appeared during registration for $($item.Pin.Name); existing contents were preserved."
            }
            $actions.Add([pscustomobject]@{ Kind = 'manifest'; Target = $manifestTarget })
            Move-Item -LiteralPath (Join-Path $item.Path 'stalthos-tool-provenance.json') -Destination $manifestTarget
        } else {
            if (Test-Path -LiteralPath $target) {
                throw "Installation folder appeared during setup for $($item.Pin.Name); existing contents were preserved."
            }
            $actions.Add([pscustomobject]@{ Kind = 'folder'; Target = $target })
            Move-Item -LiteralPath $item.Path -Destination $target
        }
    }
} catch {
    for ($i = $actions.Count - 1; $i -ge 0; $i--) {
        $action = $actions[$i]
        if (Test-Path -LiteralPath $action.Target) {
            if ($action.Kind -eq 'folder') {
                Remove-Item -LiteralPath $action.Target -Recurse -Force
            } else {
                Remove-Item -LiteralPath $action.Target -Force
            }
        }
    }
    throw
} finally {
    foreach ($item in $staged) {
        if ($item.Path -and (Test-Path -LiteralPath $item.Path)) {
            Remove-Item -LiteralPath $item.Path -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

$result = foreach ($item in $staged) {
    $pin = $item.Pin
    $target = Join-Path $InstallRoot $pin.InstallDirectory
    [ordered]@{
        tool = $pin.Name
        version = $pin.Version
        executable = Join-Path $target $pin.Executable
        executable_sha256 = $pin.ExecutableSha256
        provenance = Join-Path $target 'stalthos-tool-provenance.json'
        action = $item.Action
    }
}
@{ ok = $true; install_root = $InstallRoot; tools = @($result) } | ConvertTo-Json -Depth 6
