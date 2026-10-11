[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$runtime = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repo = [IO.Path]::GetFullPath((Join-Path $runtime '..'))
$source = Join-Path $repo '../../vendor/WickedEngine'
if (-not (Test-Path (Join-Path $source 'CMakeLists.txt'))) { throw "Pinned source required for test: $source" }
$testRoot = Join-Path $repo ('var/tests/wicked-build-' + [guid]::NewGuid().ToString('N'))
$fakeBin = Join-Path $testRoot 'fake-cmake'
$build = Join-Path $testRoot 'build'
$failedBuild = Join-Path $testRoot 'failed-build'
$dirtyBuild = Join-Path $testRoot 'dirty-build'
$cache = Join-Path $testRoot 'cache'
$import = Join-Path $testRoot 'shared'
New-Item -ItemType Directory -Force $fakeBin | Out-Null
@'
@echo off
pwsh -NoLogo -NoProfile -File "%~dp0fake-cmake.ps1" %*
'@ | Set-Content -LiteralPath (Join-Path $fakeBin 'cmake.cmd') -Encoding ascii
@'
$A=$args
$ErrorActionPreference='Stop'
if ($A[0] -eq '--version') { 'cmake version test-double'; exit 0 }
if ($A[0] -eq '-S') {
  $src=$A[[Array]::IndexOf($A,'-S')+1]; $b=$A[[Array]::IndexOf($A,'-B')+1]
  New-Item -ItemType Directory -Force $b | Out-Null
  @('WICKED_EMBED_SHADERS:BOOL=ON','WICKED_DYNAMIC_LIBRARY:BOOL=OFF','USE_AVX:BOOL=ON','USE_AVX2:BOOL=OFF','CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026',"CMAKE_HOME_DIRECTORY:INTERNAL=$src",'CMAKE_CXX_COMPILER_VERSION:INTERNAL=19.50','CMAKE_CXX_COMPILER:FILEPATH=cl.exe') | Set-Content (Join-Path $b 'CMakeCache.txt')
  exit 0
}
if ($A[0] -eq '--build') {
  $b=$A[1]
  $target=$A[[Array]::IndexOf($A,'--target')+1]
  if ($target -ne 'WickedEngine_emb_shaders') { exit 11 }
  if ($env:FAKE_WICKED_BUILD_FAIL -eq '1') { exit 9 }
  $base=Join-Path $b 'WickedEngine'
  $obj=Join-Path $base 'WickedEngine_common.dir/Release'; New-Item -ItemType Directory -Force $obj,(Join-Path $base 'Release'),(Join-Path $base 'LUA/Release'),(Join-Path $base 'Utility/Release') | Out-Null
  1..80 | ForEach-Object { "obj$_" | Set-Content (Join-Path $obj ("unit{0:D2}.obj" -f $_)) }
  foreach($p in @('Release/WickedEngine.lib','Release/Jolt.lib','LUA/Release/LUA.lib','Utility/Release/Utility.lib','dxcompiler.dll')) { $f=Join-Path $base $p; "fixture:$p" | Set-Content $f }
  exit 0
}
exit 0
'@ | Set-Content -LiteralPath (Join-Path $fakeBin 'fake-cmake.ps1') -Encoding utf8
$fake = Join-Path $fakeBin 'cmake.cmd'
try {
    # Model the actual CMake build tree, including the DLL under WickedEngine/.
    $engine = Join-Path $import 'WickedEngine'
    $objects = Join-Path $engine 'WickedEngine_common.dir/Release'
    New-Item -ItemType Directory -Force $objects,(Join-Path $engine 'Release'),(Join-Path $engine 'LUA/Release'),(Join-Path $engine 'Utility/Release') | Out-Null
    1..80 | ForEach-Object { "import$_" | Set-Content (Join-Path $objects ("import{0:D2}.obj" -f $_)) }
    foreach($p in @('Release/WickedEngine.lib','Release/Jolt.lib','LUA/Release/LUA.lib','Utility/Release/Utility.lib','dxcompiler.dll')) { Set-Content -LiteralPath (Join-Path $engine $p) -Value "fixture:$p" }
    $sourcePath = [IO.Path]::GetFullPath($source)
    @('WICKED_EMBED_SHADERS:BOOL=ON','WICKED_DYNAMIC_LIBRARY:BOOL=OFF','USE_AVX:BOOL=ON','USE_AVX2:BOOL=OFF','CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026',"CMAKE_HOME_DIRECTORY:INTERNAL=$sourcePath") | Set-Content (Join-Path $import 'CMakeCache.txt')

    & (Join-Path $runtime 'scripts/Prepare-Wicked.ps1') -Source $sourcePath -ReleaseCache $import -CacheDirectory $cache | Out-Null
    $receiptPath = Join-Path $cache 'provenance.json'
    $receipt = Get-Content $receiptPath -Raw | ConvertFrom-Json
    if ($receipt.build_kind -ne 'cache-import' -or @($receipt.artifacts).Count -ne 85) { throw 'Imported cache receipt did not validate as 85 artifacts' }
    $dllArtifact = @($receipt.artifacts | Where-Object path -eq 'bin/dxcompiler.dll')
    if ($dllArtifact.Count -ne 1 -or (Get-FileHash (Join-Path $cache 'bin/dxcompiler.dll') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $dllArtifact[0].sha256) { throw 'Real-layout WickedEngine/dxcompiler.dll was not imported and verified' }
    $priorReceiptHash = (Get-FileHash $receiptPath -Algorithm SHA256).Hash
    $priorArtifactHash = (Get-FileHash (Join-Path $cache 'libraries/WickedEngine.lib') -Algorithm SHA256).Hash

    Remove-Item -LiteralPath (Join-Path $engine 'dxcompiler.dll')
    $prepareFailed = $false
    try { & (Join-Path $runtime 'scripts/Prepare-Wicked.ps1') -Source $sourcePath -ReleaseCache $import -CacheDirectory $cache | Out-Null } catch { $prepareFailed = $true }
    if (-not $prepareFailed) { throw 'Missing imported input was incorrectly accepted' }
    if ((Get-FileHash $receiptPath -Algorithm SHA256).Hash -ne $priorReceiptHash -or (Get-FileHash (Join-Path $cache 'libraries/WickedEngine.lib') -Algorithm SHA256).Hash -ne $priorArtifactHash) { throw 'Failed cache preparation changed prior cache contents or receipt' }

    & (Join-Path $runtime 'scripts/Build-Wicked.ps1') -CMake $fake -Source $sourcePath -BuildDirectory $build -CacheDirectory $cache | Out-Null
    $receipt = Get-Content $receiptPath -Raw | ConvertFrom-Json
    if ($receipt.build_kind -ne 'fresh' -or @($receipt.artifacts).Count -ne 85) { throw 'Fresh cache receipt did not validate as 85 artifacts' }
    if (-not $receiptPath.StartsWith($cache, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected cache receipt location' }
    $priorReceiptHash = (Get-FileHash $receiptPath -Algorithm SHA256).Hash
    $priorArtifactHash = (Get-FileHash (Join-Path $cache 'libraries/WickedEngine.lib') -Algorithm SHA256).Hash

    New-Item -ItemType Directory -Force $dirtyBuild | Out-Null
    Set-Content -LiteralPath (Join-Path $dirtyBuild 'stale.obj') -Value 'stale'
    $dirtyRejected = $false
    try { & (Join-Path $runtime 'scripts/Build-Wicked.ps1') -CMake $fake -Source $sourcePath -BuildDirectory $dirtyBuild -CacheDirectory $cache | Out-Null } catch { $dirtyRejected = $true }
    if (-not $dirtyRejected) { throw 'Fresh-build script accepted a populated build directory' }

    $env:FAKE_WICKED_BUILD_FAIL = '1'
    $buildFailed = $false
    try { & (Join-Path $runtime 'scripts/Build-Wicked.ps1') -CMake $fake -Source $sourcePath -BuildDirectory $failedBuild -CacheDirectory $cache | Out-Null } catch { $buildFailed = $true }
    if (-not $buildFailed) { throw 'Mocked engine failure was incorrectly reported as success' }
    if ((Get-FileHash $receiptPath -Algorithm SHA256).Hash -ne $priorReceiptHash) { throw 'Failed build replaced the prior cache receipt' }
    if ((Get-FileHash (Join-Path $cache 'libraries/WickedEngine.lib') -Algorithm SHA256).Hash -ne $priorArtifactHash) { throw 'Failed build changed a prior cache artifact' }
    [ordered]@{ok=$true;target='WickedEngine_emb_shaders';fresh_directory_enforced=$true;import_layout='WickedEngine/dxcompiler.dll';prepare_failure_preserved_prior_cache=$true;build_failure_preserved_prior_cache=$true;artifact_count=85} | ConvertTo-Json -Compress
}
finally {
    Remove-Item Env:FAKE_WICKED_BUILD_FAIL -ErrorAction SilentlyContinue
    if (Test-Path $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
