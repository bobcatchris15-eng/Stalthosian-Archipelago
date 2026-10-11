[CmdletBinding()]
param([string]$SourceMirror = '', [string]$ReleaseCache = '', [string]$Source = '', [string]$CacheDirectory = '')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$lock = Get-Content (Join-Path $root 'stalthos-runtime/config/wicked.lock.json') -Raw | ConvertFrom-Json
$vendor = Join-Path $root 'var/vendor'
$source = if ($Source) { [IO.Path]::GetFullPath($Source) } else { Join-Path $vendor 'WickedEngine' }
$cacheOut = if ($CacheDirectory) { [IO.Path]::GetFullPath($CacheDirectory) } else { Join-Path $vendor 'wicked-release-cache' }
function Assert-VarPath([string]$Path, [string]$Name) {
    $varRoot = [IO.Path]::GetFullPath((Join-Path $root 'var')).TrimEnd('\') + '\'
    if (-not ([IO.Path]::GetFullPath($Path).StartsWith($varRoot, [StringComparison]::OrdinalIgnoreCase))) {
        throw "$Name must resolve inside this checkout's var directory: $Path"
    }
}
Assert-VarPath $cacheOut 'CacheDirectory'
if ([IO.Path]::GetFullPath($cacheOut).TrimEnd('\').StartsWith([IO.Path]::GetFullPath($source).TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFullPath($source).TrimEnd('\').StartsWith([IO.Path]::GetFullPath($cacheOut).TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFullPath($source).TrimEnd('\') -eq [IO.Path]::GetFullPath($cacheOut).TrimEnd('\')) { throw 'CacheDirectory must be separate from the pinned source tree' }
New-Item -ItemType Directory -Force $vendor | Out-Null
if (-not (Test-Path $source)) {
    Assert-VarPath $source 'Source when creating a new checkout'
    $origin = if ($SourceMirror) { (Resolve-Path $SourceMirror).Path } else { $lock.repository }
    New-Item -ItemType Directory -Force (Split-Path $source) | Out-Null
    & git clone --no-hardlinks $origin $source | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Wicked source clone failed' }
    & git -C $source checkout --detach $lock.revision | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Pinned Wicked revision checkout failed' }
    & git -C $source remote set-url origin $lock.repository
    if ($LASTEXITCODE -ne 0) { throw 'Could not record official source origin' }
}
$revision = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $revision -ne $lock.revision) { throw 'Wicked source pin mismatch; existing source was preserved' }
$dirty = & git -C $source status --porcelain
if ($dirty) { throw 'Wicked source has local changes; refusing to reuse it' }
if (-not (Test-Path (Join-Path $source 'LICENSE.txt'))) { throw 'Missing Wicked license' }
$artifacts = @()
if ($ReleaseCache) {
    $shared = (Resolve-Path $ReleaseCache).Path
    $cache = Get-Content (Join-Path $shared 'CMakeCache.txt') -Raw
    foreach ($flag in @('WICKED_EMBED_SHADERS:BOOL=ON','WICKED_DYNAMIC_LIBRARY:BOOL=OFF','USE_AVX:BOOL=ON','USE_AVX2:BOOL=OFF','CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026')) {
        if (-not $cache.Contains($flag)) { throw "Incompatible cached build: $flag" }
    }
    $cacheSource = ([regex]::Match($cache,'(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$')).Groups[1].Value.Trim()
    if (-not $cacheSource) { throw 'Cached build has no source root' }
    $cacheRevision = (& git -C $cacheSource rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $cacheRevision -ne $lock.revision) { throw 'Cached source revision mismatch' }
    if (& git -C $cacheSource status --porcelain) { throw 'Cached source is dirty; refusing compiled input reuse' }
    $objects = @(Get-ChildItem (Join-Path $shared 'WickedEngine/WickedEngine_common.dir/Release') -Filter '*.obj' | Sort-Object Name)
    if ($objects.Count -ne 80) { throw "Pinned cache expects 80 common objects, found $($objects.Count)" }
    $inputs = @($objects | ForEach-Object { @{Path=$_.FullName;Relative=('objects/'+$_.Name)} })
    $dxcompiler = Join-Path $shared 'WickedEngine/dxcompiler.dll'
    if (-not (Test-Path -LiteralPath $dxcompiler -PathType Leaf)) {
        # Older compatible cache layouts placed this file in the Release directory.
        $dxcompiler = Join-Path $shared 'Release/dxcompiler.dll'
    }
    foreach ($entry in @(
        @('WickedEngine/Release/WickedEngine.lib','libraries/WickedEngine.lib'),
        @('WickedEngine/Release/Jolt.lib','libraries/Jolt.lib'),
        @('WickedEngine/LUA/Release/LUA.lib','libraries/LUA.lib'),
        @('WickedEngine/Utility/Release/Utility.lib','libraries/Utility.lib'))) {
        $inputs += @{Path=(Join-Path $shared $entry[0]);Relative=$entry[1]}
    }
    $inputs += @{Path=$dxcompiler;Relative='bin/dxcompiler.dll'}
    foreach ($input in $inputs) {
        if (-not (Test-Path -LiteralPath $input.Path -PathType Leaf)) { throw "Missing cached artifact: $($input.Path)" }
    }
    $cacheParent = Split-Path $cacheOut
    New-Item -ItemType Directory -Force $cacheParent | Out-Null
    $stage = Join-Path $cacheParent ((Split-Path $cacheOut -Leaf) + '.stage-' + [guid]::NewGuid().ToString('N'))
    $backup = Join-Path $cacheParent ((Split-Path $cacheOut -Leaf) + '.backup-' + [guid]::NewGuid().ToString('N'))
    $lockHandle = $null
    try {
        try { $lockHandle = [IO.File]::Open("$cacheOut.prepare.lock", [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
        catch { throw "Another Wicked cache preparation/publication is active: $cacheOut" }
        New-Item -ItemType Directory -Force $stage, (Join-Path $stage 'objects'), (Join-Path $stage 'libraries'), (Join-Path $stage 'bin') | Out-Null
        foreach ($input in $inputs) {
            $target = Join-Path $stage $input.Relative
            $sha = (Get-FileHash -LiteralPath $input.Path -Algorithm SHA256).Hash.ToLowerInvariant()
            Copy-Item -LiteralPath $input.Path -Destination $target
            if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) { throw "Copy digest mismatch: $target" }
            $artifacts += @{path=$input.Relative;sha256=$sha;bytes=(Get-Item -LiteralPath $target).Length}
        }
        Copy-Item -LiteralPath (Join-Path $shared 'CMakeCache.txt') -Destination (Join-Path $stage 'source-CMakeCache.txt')
        $receipt = [ordered]@{schema_version=1;build_kind='cache-import';engine_revision=$revision;configuration='Release';cache_source=$shared;source_root=$cacheSource;artifacts=$artifacts;note='Verified local copy of existing compiled inputs; not a fresh engine rebuild'}
        $receiptPath = Join-Path $stage 'provenance.json'
        $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath -Encoding utf8
        $check = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
        if (@($check.artifacts).Count -ne 85) { throw 'Staged imported cache must contain exactly 85 artifacts' }
        foreach ($artifact in $check.artifacts) {
            if ((Get-FileHash -LiteralPath (Join-Path $stage $artifact.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $artifact.sha256) { throw "Staged artifact failed final digest validation: $($artifact.path)" }
        }
        if (Test-Path -LiteralPath $cacheOut) { Move-Item -LiteralPath $cacheOut -Destination $backup }
        try { Move-Item -LiteralPath $stage -Destination $cacheOut }
        catch {
            if (Test-Path -LiteralPath $backup) { Move-Item -LiteralPath $backup -Destination $cacheOut }
            throw
        }
        if (Test-Path -LiteralPath $backup) { Remove-Item -LiteralPath $backup -Recurse -Force }
    }
    finally {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
        if ($lockHandle) { $lockHandle.Dispose() }
    }
}
[ordered]@{ok=$true;engine_revision=$revision;source=$source;cache_available=(Test-Path (Join-Path $cacheOut 'provenance.json'));copied_artifacts=$artifacts.Count} | ConvertTo-Json
