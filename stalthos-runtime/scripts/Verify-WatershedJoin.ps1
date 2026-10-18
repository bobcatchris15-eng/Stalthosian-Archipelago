[CmdletBinding()]
param([double]$Duration = 12, [string]$Evidence = '')
$ErrorActionPreference = 'Stop'
if (-not [double]::IsFinite($Duration) -or $Duration -lt 6 -or $Duration -gt 60) {
    throw 'Watershed join verification duration must be 6..60 seconds'
}
if (-not $Evidence) {
    $lines = @(& (Join-Path $PSScriptRoot 'Run.ps1') -WatershedJoinProbe -Duration $Duration)
    if ($lines.Count -lt 2 -or $lines[0] -notmatch '; evidence (.+)$') { throw 'Missing terrain launch evidence' }
    $Evidence = $Matches[1]
}
function Test-FiniteNumber($Value) {
    return (($Value -is [int] -or $Value -is [long] -or $Value -is [double] -or $Value -is [decimal]) -and
        [double]::IsFinite([double]$Value))
}
$receipt = Get-Content (Join-Path $evidence 'runtime-receipt.json') -Raw | ConvertFrom-Json
foreach ($name in @('body_tile_crossings','visited_body_tiles','peak_resident_tiles','evicted_tiles',
        'shared_edge_samples','max_shared_height_error_m','max_shared_normal_component_error',
        'support_samples','max_proxy_bottom_center_error_m','max_rendered_bounds_center_error_m',
        'final_body_x','watershed_tile_evictions','regenerated_tiles','regenerated_mesh_mismatches','watershed_tiles_pinned','interior_mesh_samples','max_interior_mesh_height_error_m')) {
    $property = $receipt.PSObject.Properties[$name]
    if ($null -eq $property -or $null -eq $property.Value -or
        -not (Test-FiniteNumber $property.Value) -or [double]$property.Value -lt 0) {
        throw "Missing or invalid numeric terrain metric '$name'; evidence retained at $evidence"
    }
}
$checks = [ordered]@{
    native_mode = $receipt.mode -eq 'watershed-join-probe'
    outbound_return_crossings = $receipt.body_tile_crossings -ge 8
    visited_five_body_tiles = $receipt.visited_body_tiles -ge 5
    resident_budget = $receipt.peak_resident_tiles -le 13 -and $receipt.peak_resident_tiles -ge 9
    retired_tiles = $receipt.evicted_tiles -ge 12
    regenerated_exactly = $receipt.regenerated_tiles -ge 6 -and $receipt.regenerated_mesh_mismatches -eq 0
    retained_watershed = $receipt.watershed_tiles_pinned -eq 4 -and $receipt.watershed_tile_evictions -eq 0
    returned_to_watershed = $receipt.route_returned -is [bool] -and $receipt.route_returned -eq $true -and [Math]::Abs($receipt.final_body_x-24) -lt 1e-9
    original_interior_mesh = $receipt.interior_mesh_samples -gt 20 -and $receipt.max_interior_mesh_height_error_m -lt 1e-9
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
$diagnostics = @($receipt.capture_diagnostics)
$checks.capture_locations_and_visibility = $diagnostics.Count -eq 3
for ($i=0; $i -lt $diagnostics.Count; ++$i) {
    $d=$diagnostics[$i]
    $x=$d.PSObject.Properties['body_x']
    $index=$d.PSObject.Properties['capture']
    if ($null -eq $x -or $null -eq $x.Value -or -not (Test-FiniteNumber $x.Value) -or $null -eq $index -or
        -not (Test-FiniteNumber $index.Value) -or $index.Value -ne $i -or
        $d.visible_object -isnot [bool] -or $d.visible_object -ne $true) {
        $checks.capture_locations_and_visibility=$false
        continue
    }
    if (($i -eq 0 -and ($x.Value -lt 24 -or $x.Value -gt 96)) -or
        ($i -eq 1 -and ($x.Value -lt 230 -or $x.Value -gt 300)) -or
        ($i -eq 2 -and $x.Value -lt 350)) { $checks.capture_locations_and_visibility=$false }
}
$passed = @($checks.Values | Where-Object { $_ -ne $true }).Count -eq 0
$summary = [ordered]@{ passed=$passed; evidence=$evidence; checks=$checks; receipt=$receipt }
$summary | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $evidence 'watershed-join-verification.json') -Encoding utf8
$summary | ConvertTo-Json -Depth 8
if (-not $passed) { throw "Watershed join verification failed; evidence retained at $evidence" }
