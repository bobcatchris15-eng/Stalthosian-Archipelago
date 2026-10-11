[CmdletBinding()]
param([string]$CMake = '', [switch]$DomainOnly, [string]$Source = '', [string]$BuildDirectory = '', [string]$CacheDirectory = '')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $CMake) {
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { $CMake = $onPath.Source } else { $CMake = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' }
}
if (-not (Test-Path $CMake)) { throw 'CMake unavailable; install Windows C++ build tools or pass -CMake' }
function Resolve-VarPath([string]$Value, [string]$Default, [string]$Name) {
    $path = [IO.Path]::GetFullPath($(if ($Value) { $Value } else { $Default }))
    $varRoot = [IO.Path]::GetFullPath((Join-Path $root 'var')).TrimEnd('\') + '\'
    if (-not $path.StartsWith($varRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "$Name must stay inside this checkout's var directory: $path" }
    return $path
}
$build = Resolve-VarPath $BuildDirectory (Join-Path $root 'var/build/stalthos-runtime') 'BuildDirectory'
$engine = [IO.Path]::GetFullPath($(if ($Source) { $Source } else { Join-Path $root 'var/vendor/WickedEngine' }))
$cache = Resolve-VarPath $CacheDirectory (Join-Path $root 'var/vendor/wicked-release-cache') 'CacheDirectory'
if ($build.TrimEnd('\').StartsWith($cache.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or $cache.TrimEnd('\').StartsWith($build.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or $build.TrimEnd('\') -eq $cache.TrimEnd('\')) { throw 'BuildDirectory and CacheDirectory must be separate, non-overlapping var paths' }
New-Item -ItemType Directory -Force $build | Out-Null
if (-not $DomainOnly) {
    $lock = Get-Content (Join-Path $root 'stalthos-runtime/config/wicked.lock.json') -Raw | ConvertFrom-Json
    $rev = (& git -C $engine rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $rev -ne $lock.revision) { throw 'Engine pin mismatch; run Prepare-Wicked.ps1' }
    if (& git -C $engine status --porcelain) { throw 'Engine source is dirty' }
    $provenance = Get-Content (Join-Path $cache 'provenance.json') -Raw | ConvertFrom-Json
    if ($provenance.schema_version -ne 1) { throw 'Unsupported cache provenance schema' }
    if ($provenance.engine_revision -ne $lock.revision) { throw 'Cache revision mismatch' }
    if ($provenance.configuration -ne 'Release') { throw 'Cache must be Release configuration' }
    $expected = @((Get-ChildItem (Join-Path $cache 'objects') -Filter '*.obj' | ForEach-Object { 'objects/'+$_.Name })) + @('libraries/WickedEngine.lib','libraries/Jolt.lib','libraries/LUA.lib','libraries/Utility.lib','bin/dxcompiler.dll')
    if ($expected.Count -ne 85 -or @($provenance.artifacts).Count -ne 85) { throw 'Cache manifest must contain exactly 80 objects and five libraries/runtime inputs' }
    $actual = @($provenance.artifacts | ForEach-Object { $_.path })
    if (@($actual | Select-Object -Unique).Count -ne 85 -or @(Compare-Object ($expected | Sort-Object) ($actual | Sort-Object)).Count -ne 0) { throw 'Cache manifest artifact set does not match consumed inputs' }
    foreach ($artifact in $provenance.artifacts) {
        if ((Get-FileHash -LiteralPath (Join-Path $cache $artifact.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $artifact.sha256) { throw "Cache artifact changed: $($artifact.path)" }
    }
}
$enabled = if ($DomainOnly) { 'OFF' } else { 'ON' }
& $CMake -S (Join-Path $root 'stalthos-runtime') -B $build -G 'Visual Studio 18 2026' -A x64 "-DSTALTHOS_BUILD_WICKED_HOST=$enabled" "-DWICKED_ENGINE_ROOT=$engine" "-DWICKED_RELEASE_CACHE=$cache" 2>&1 | Tee-Object (Join-Path $build 'configure.log') | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Runtime configure failed' }
& $CMake --build $build --config Release --parallel 2 2>&1 | Tee-Object (Join-Path $build 'build.log') | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Runtime Release build failed' }
$exe = Join-Path $build 'Release/Stalthos.exe'
if (-not $DomainOnly -and -not (Test-Path $exe)) { throw 'Build did not produce the game executable' }
if (-not $DomainOnly) {
    $cacheNote = $provenance.note
    [ordered]@{schema_version=1;engine_revision=$rev;configuration='Release';executable=$exe;executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant();cache_provenance_sha256=(Get-FileHash (Join-Path $cache 'provenance.json') -Algorithm SHA256).Hash.ToLowerInvariant();source_revision=(& git -C $root rev-parse HEAD).Trim();dirty_working_tree=$true;note="Application built locally against engine inputs: $cacheNote"} | ConvertTo-Json | Set-Content (Join-Path $build 'build-receipt.json') -Encoding utf8
}
