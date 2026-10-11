[CmdletBinding()]
param(
    [string]$CMake = '',
    [string]$Source = '',
    [string]$BuildDirectory = '',
    [string]$CacheDirectory = '',
    [switch]$ConfigureOnly
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$lock = Get-Content (Join-Path $PSScriptRoot '../config/wicked.lock.json') -Raw | ConvertFrom-Json
function Resolve-VarPath([string]$Value, [string]$Default, [string]$Name) {
    $path = [IO.Path]::GetFullPath($(if ($Value) { $Value } else { $Default }))
    $varRoot = [IO.Path]::GetFullPath((Join-Path $root 'var')).TrimEnd('\') + '\'
    if (-not $path.StartsWith($varRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "$Name must stay inside this checkout's var directory: $path" }
    return $path
}
$engine = [IO.Path]::GetFullPath($(if ($Source) { $Source } else { Join-Path $root 'var/vendor/WickedEngine' }))
if (-not (Test-Path -LiteralPath $engine -PathType Container)) { throw "Wicked source directory is missing: $engine" }
$build = Resolve-VarPath $BuildDirectory (Join-Path $root 'var/build/wicked-engine') 'BuildDirectory'
$cache = Resolve-VarPath $CacheDirectory (Join-Path $root 'var/vendor/wicked-release-cache') 'CacheDirectory'
function PathsOverlap([string]$Left, [string]$Right) {
    $a = [IO.Path]::GetFullPath($Left).TrimEnd('\') + '\'
    $b = [IO.Path]::GetFullPath($Right).TrimEnd('\') + '\'
    return $a.StartsWith($b, [StringComparison]::OrdinalIgnoreCase) -or $b.StartsWith($a, [StringComparison]::OrdinalIgnoreCase)
}
if (PathsOverlap $build $cache) { throw 'BuildDirectory and CacheDirectory must be separate, non-overlapping var paths' }
if (PathsOverlap $engine $cache) { throw 'CacheDirectory must not overlap the pinned source tree' }
if (PathsOverlap $engine $build) { throw 'BuildDirectory must not overlap the pinned source tree' }
if (-not $CMake) {
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { $CMake = $onPath.Source } else { $CMake = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' }
}
if (-not (Test-Path -LiteralPath $CMake)) { throw "CMake unavailable: $CMake" }
if (-not (Test-Path -LiteralPath (Join-Path $engine 'CMakeLists.txt'))) { throw "Pinned Wicked source is missing: $engine" }
$revision = (& git -C $engine rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $revision -ne $lock.revision) { throw "Wicked source pin mismatch; expected $($lock.revision), got $revision" }
if (& git -C $engine status --porcelain) { throw 'Wicked source has local changes; refusing to compile it' }
if (-not (Test-Path (Join-Path $engine 'LICENSE.txt'))) { throw 'Pinned Wicked license is missing' }

# The lock serializes builds targeting the same cache across this shared checkout.
$parent = Split-Path $cache
New-Item -ItemType Directory -Force $parent | Out-Null
if (Test-Path -LiteralPath $build) {
    $existing = @(Get-ChildItem -LiteralPath $build -Force)
    if ($existing.Count -gt 0) { throw "Fresh Wicked build requires an empty build directory: $build" }
} else {
    New-Item -ItemType Directory -Force $build | Out-Null
}
$lockPaths = @("$build.build.lock", "$cache.build.lock") | Sort-Object -Unique
$lockHandles = @()
$stage = Join-Path $parent ((Split-Path $cache -Leaf) + '.stage-' + [guid]::NewGuid().ToString('N'))
$backup = Join-Path $parent ((Split-Path $cache -Leaf) + '.backup-' + [guid]::NewGuid().ToString('N'))
try {
    foreach ($lockPath in $lockPaths) {
        try { $lockHandles += [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
        catch { throw "Another Wicked build/publication is active (or lock is unavailable): $lockPath" }
    }
    $configureLog = Join-Path $build 'configure.log'
    $buildLog = Join-Path $build 'build.log'
    $args = @('-S', $engine, '-B', $build, '-G', 'Visual Studio 18 2026', '-A', 'x64',
        '-DWICKED_DYNAMIC_LIBRARY=OFF', '-DWICKED_EMBED_SHADERS=ON', '-DUSE_AVX=ON', '-DUSE_AVX2=OFF',
        '-DWICKED_EDITOR=OFF', '-DWICKED_TESTS=OFF', '-DWICKED_IMGUI_EXAMPLE=OFF', '-DWICKED_WINDOWS_TEMPLATE=OFF')
    & $CMake @args 2>&1 | Tee-Object -FilePath $configureLog | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Wicked configure failed; see $configureLog" }
    $cmakeCache = Get-Content (Join-Path $build 'CMakeCache.txt') -Raw
    foreach ($flag in @('WICKED_EMBED_SHADERS:BOOL=ON','WICKED_DYNAMIC_LIBRARY:BOOL=OFF','USE_AVX:BOOL=ON','USE_AVX2:BOOL=OFF','CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026')) {
        if (-not $cmakeCache.Contains($flag)) { throw "Configured Wicked cache is missing required setting: $flag" }
    }
    $sourceRoot = ([regex]::Match($cmakeCache,'(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$')).Groups[1].Value.Trim()
    if ([IO.Path]::GetFullPath($sourceRoot) -ne [IO.Path]::GetFullPath($engine)) { throw 'Configured CMake source root differs from the pinned source path' }
    if ($ConfigureOnly) {
        [ordered]@{ok=$true;configured_only=$true;engine_revision=$revision;configuration='Release';build_directory=$build} | ConvertTo-Json -Compress
        return
    }
    & $CMake --build $build --config Release --target WickedEngine_emb_shaders --parallel 2 2>&1 | Tee-Object -FilePath $buildLog | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Wicked Release build failed; see $buildLog" }

    $objDir = Join-Path $build 'WickedEngine/WickedEngine_common.dir/Release'
    $objects = @(Get-ChildItem -LiteralPath $objDir -Filter '*.obj' -File | Sort-Object Name)
    if ($objects.Count -ne 80) { throw "Fresh Wicked build produced $($objects.Count) common objects; host contract requires exactly 80" }
    $mapping = @($objects | ForEach-Object { @{Path=$_.FullName;Relative=('objects/'+$_.Name)} })
    foreach ($entry in @(
        @('WickedEngine/Release/WickedEngine.lib','libraries/WickedEngine.lib'),
        @('WickedEngine/Release/Jolt.lib','libraries/Jolt.lib'),
        @('WickedEngine/LUA/Release/LUA.lib','libraries/LUA.lib'),
        @('WickedEngine/Utility/Release/Utility.lib','libraries/Utility.lib'),
        @('WickedEngine/dxcompiler.dll','bin/dxcompiler.dll'))) {
        $mapping += @{Path=(Join-Path $build $entry[0]);Relative=$entry[1]}
    }
    foreach ($item in $mapping) { if (-not (Test-Path -LiteralPath $item.Path -PathType Leaf)) { throw "Missing built Wicked artifact: $($item.Path)" } }

    New-Item -ItemType Directory -Force $stage, (Join-Path $stage 'objects'), (Join-Path $stage 'libraries'), (Join-Path $stage 'bin') | Out-Null
    $artifacts = @()
    foreach ($item in $mapping) {
        $destination = Join-Path $stage $item.Relative
        $sourceHash = (Get-FileHash -LiteralPath $item.Path -Algorithm SHA256).Hash.ToLowerInvariant()
        Copy-Item -LiteralPath $item.Path -Destination $destination
        $copiedHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($copiedHash -ne $sourceHash) { throw "Wicked artifact copy digest mismatch: $($item.Relative)" }
        $artifacts += @{path=$item.Relative;sha256=$copiedHash;bytes=(Get-Item -LiteralPath $destination).Length}
    }
    Copy-Item -LiteralPath (Join-Path $build 'CMakeCache.txt') -Destination (Join-Path $stage 'source-CMakeCache.txt')
    $cmakeVersion = (& $CMake --version | Select-Object -First 1).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Could not record CMake version' }
    $compilerVersion = ([regex]::Match($cmakeCache,'(?m)^CMAKE_CXX_COMPILER_VERSION:INTERNAL=(.+)$')).Groups[1].Value.Trim()
    if (-not $compilerVersion) { $compilerVersion = 'CMake did not record CMAKE_CXX_COMPILER_VERSION' }
    $compilerPath = ([regex]::Match($cmakeCache,'(?m)^CMAKE_CXX_COMPILER:FILEPATH=(.+)$')).Groups[1].Value.Trim()
    $receipt = [ordered]@{
        schema_version=1; build_kind='fresh'; engine_revision=$revision; configuration='Release'
        cache_source=$build; source_root=$engine; generator='Visual Studio 18 2026'; architecture='x64'
        cmake_version=$cmakeVersion; compiler_version=$compilerVersion; compiler_path=$compilerPath; artifacts=$artifacts
        note='Fresh local Release compile of the pinned Wicked source; compilation completion and exported artifacts were verified.'
    }
    $receiptPath = Join-Path $stage 'provenance.json'
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath -Encoding utf8
    # Re-read and validate the complete staged manifest before publication.
    $check = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    if (@($check.artifacts).Count -ne 85 -or $check.engine_revision -ne $lock.revision) { throw 'Staged Wicked provenance is incomplete' }
    foreach ($artifact in $check.artifacts) {
        $file = Join-Path $stage $artifact.path
        if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $artifact.sha256) { throw "Staged artifact failed final digest validation: $($artifact.path)" }
    }
    if (Test-Path -LiteralPath $cache) { Move-Item -LiteralPath $cache -Destination $backup }
    try { Move-Item -LiteralPath $stage -Destination $cache }
    catch {
        if (Test-Path -LiteralPath $backup) { Move-Item -LiteralPath $backup -Destination $cache }
        throw
    }
    if (Test-Path -LiteralPath $backup) { Remove-Item -LiteralPath $backup -Recurse -Force }
    [ordered]@{ok=$true;build_kind='fresh';engine_revision=$revision;configuration='Release';artifacts=$artifacts.Count;cache=$cache;build_directory=$build} | ConvertTo-Json -Compress
}
finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    foreach ($handle in $lockHandles) { $handle.Dispose() }
}
