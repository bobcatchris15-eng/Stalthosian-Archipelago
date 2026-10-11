[CmdletBinding()]
param([string]$SourceMirror = '', [string]$ReleaseCache = '')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$lock = Get-Content (Join-Path $root 'stalthos-runtime/config/wicked.lock.json') -Raw | ConvertFrom-Json
$vendor = Join-Path $root 'var/vendor'
$source = Join-Path $vendor 'WickedEngine'
$cacheOut = Join-Path $vendor 'wicked-release-cache'
New-Item -ItemType Directory -Force $vendor | Out-Null
if (-not (Test-Path $source)) {
    $origin = if ($SourceMirror) { (Resolve-Path $SourceMirror).Path } else { $lock.repository }
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
    foreach ($entry in @(
        @('WickedEngine/Release/WickedEngine.lib','libraries/WickedEngine.lib'),
        @('WickedEngine/Release/Jolt.lib','libraries/Jolt.lib'),
        @('WickedEngine/LUA/Release/LUA.lib','libraries/LUA.lib'),
        @('WickedEngine/Utility/Release/Utility.lib','libraries/Utility.lib'),
        @('Release/dxcompiler.dll','bin/dxcompiler.dll'))) {
        $inputs += @{Path=(Join-Path $shared $entry[0]);Relative=$entry[1]}
    }
    foreach ($input in $inputs) {
        if (-not (Test-Path $input.Path)) { throw "Missing cached artifact: $($input.Path)" }
        $target = Join-Path $cacheOut $input.Relative
        New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
        $sha = (Get-FileHash -LiteralPath $input.Path -Algorithm SHA256).Hash.ToLowerInvariant()
        if (-not (Test-Path $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) {
            Copy-Item -LiteralPath $input.Path -Destination $target
        }
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) { throw "Copy digest mismatch: $target" }
        $artifacts += @{path=$input.Relative;sha256=$sha;bytes=(Get-Item $target).Length}
    }
    Copy-Item -LiteralPath (Join-Path $shared 'CMakeCache.txt') -Destination (Join-Path $cacheOut 'source-CMakeCache.txt')
    $receipt = [ordered]@{schema_version=1;engine_revision=$revision;configuration='Release';cache_source=$shared;source_root=$cacheSource;artifacts=$artifacts;note='Verified local copy of existing compiled inputs; not a fresh engine rebuild'}
    $receipt | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $cacheOut 'provenance.json') -Encoding utf8
}
[ordered]@{ok=$true;engine_revision=$revision;source=$source;cache_available=(Test-Path (Join-Path $cacheOut 'provenance.json'));copied_artifacts=$artifacts.Count} | ConvertTo-Json
