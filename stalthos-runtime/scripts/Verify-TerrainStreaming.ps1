[CmdletBinding()]
param([double]$Duration = 12, [string]$Evidence = '')
$ErrorActionPreference = 'Stop'
if (-not [double]::IsFinite($Duration) -or $Duration -lt 6 -or $Duration -gt 60) {
    throw 'Terrain verification duration must be 6..60 seconds'
}
if (-not $Evidence) {
    $lines = @(& (Join-Path $PSScriptRoot 'Run.ps1') -TerrainStreamProbe -Duration $Duration)
    if ($lines.Count -lt 2 -or $lines[0] -notmatch '; evidence (.+)$') { throw 'Missing terrain launch evidence' }
    $Evidence = $Matches[1]
}
$receipt = Get-Content (Join-Path $evidence 'runtime-receipt.json') -Raw | ConvertFrom-Json
foreach ($name in @('body_tile_crossings','visited_body_tiles','peak_resident_tiles','evicted_tiles',
        'shared_edge_samples','max_shared_height_error_m','max_shared_normal_component_error',
        'support_samples','max_proxy_bottom_center_error_m','max_rendered_bounds_center_error_m')) {
    $property = $receipt.PSObject.Properties[$name]
    if ($null -eq $property -or $null -eq $property.Value -or
        $property.Value -is [bool] -or $property.Value -is [string] -or
        -not [double]::IsFinite([double]$property.Value) -or [double]$property.Value -lt 0) {
        throw "Missing or invalid numeric terrain metric '$name'; evidence retained at $evidence"
    }
}
$checks = [ordered]@{
    native_mode = $receipt.mode -eq 'terrain-stream-probe'
    crossed_five_boundaries = $receipt.body_tile_crossings -ge 5
    visited_six_body_tiles = $receipt.visited_body_tiles -ge 6
    resident_budget = $receipt.peak_resident_tiles -eq 9
    retired_tiles = $receipt.evicted_tiles -ge 15
    exact_shared_heights = $receipt.shared_edge_samples -gt 0 -and $receipt.max_shared_height_error_m -eq 0
    exact_shared_normals = $receipt.max_shared_normal_component_error -eq 0
    bottom_center_support = $receipt.support_samples -gt 20 -and $receipt.max_proxy_bottom_center_error_m -lt 0.0001
    rendered_proxy_bounds = $receipt.max_rendered_bounds_center_error_m -lt 0.0001
    captures_saved = @($receipt.captures).Count -eq 3 -and
        @($receipt.captures | Where-Object { $_ -is [bool] -and $_ -eq $true }).Count -eq 3
}
for ($i=0; $i -lt 3; ++$i) {
    if (-not (Test-Path -LiteralPath (Join-Path $evidence "terrain-$i.png") -PathType Leaf)) { $checks.captures_saved = $false }
}
$passed = @($checks.Values | Where-Object { $_ -ne $true }).Count -eq 0
$summary = [ordered]@{ passed=$passed; evidence=$evidence; checks=$checks; receipt=$receipt }
$summary | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $evidence 'terrain-verification.json') -Encoding utf8
$summary | ConvertTo-Json -Depth 8
if (-not $passed) { throw "Terrain streaming verification failed; evidence retained at $evidence" }
