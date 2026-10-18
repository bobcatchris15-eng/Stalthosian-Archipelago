[CmdletBinding()]
param([double]$Duration = 18, [string]$Evidence = '')
$ErrorActionPreference = 'Stop'
if (-not [double]::IsFinite($Duration) -or $Duration -lt 12 -or $Duration -gt 60) {
    throw 'Populated streaming verification duration must be 12..60 seconds'
}
if (-not $Evidence) {
    $lines = @(& (Join-Path $PSScriptRoot 'Run.ps1') -PopulatedWorld -PopulatedTraversal -Duration $Duration -CameraDistance 16)
    $launch = $lines | Where-Object { $_ -match '^Process [0-9]+; evidence ' } | Select-Object -First 1
    if (-not $launch) { throw 'Missing populated traversal launch evidence' }
    $Evidence = $launch -replace '^Process [0-9]+; evidence ',''
}
function Test-Number($Value) {
    return (($Value -is [int] -or $Value -is [long] -or $Value -is [double] -or $Value -is [decimal]) -and
        [double]::IsFinite([double]$Value))
}
function Assert-True($Value, [string]$Name) {
    if ($Value -isnot [bool] -or $Value -ne $true) { throw "Invalid populated traversal claim: $Name" }
}
$r = Get-Content -LiteralPath (Join-Path $Evidence 'populated-terrain.json') -Raw | ConvertFrom-Json
foreach ($name in @('created_tiles','evicted_tiles','regenerated_tiles','regenerated_mesh_mismatches',
        'peak_resident_tiles','resident_tiles','body_tile_crossings','visited_body_tiles','shared_edge_samples',
        'max_shared_height_error_m','max_shared_normal_component_error','final_body_x','final_body_z','population_count_retained','max_body_x',
        'support_samples','max_root_ground_error_m','max_proxy_bottom_center_error_m','max_rendered_bounds_center_error_m','rendered_support_fallbacks')) {
    $p = $r.PSObject.Properties[$name]
    if ($null -eq $p -or -not (Test-Number $p.Value) -or $p.Value -lt 0) { throw "Missing or invalid populated terrain metric: $name" }
}
foreach ($name in @('created_tiles','evicted_tiles','regenerated_tiles','regenerated_mesh_mismatches','peak_resident_tiles',
        'resident_tiles','body_tile_crossings','visited_body_tiles','shared_edge_samples','support_samples','population_count_retained','rendered_support_fallbacks')) {
    if ([Math]::Truncate([double]$r.$name) -ne $r.$name) { throw "Noninteger populated terrain count: $name" }
}
Assert-True $r.reference_mesh_retained 'reference mesh retained'
Assert-True $r.diagnostic_traversal 'actual populated traversal'
Assert-True $r.route_returned 'route returned'
if ($r.exterior_cells_overlap_reference -isnot [bool] -or $r.exterior_cells_overlap_reference -ne $false) { throw 'Exterior overlaps original detailed terrain' }
if ($r.residency_driver -cne 'body' -or $r.population_activation_implemented -isnot [bool] -or $r.population_activation_implemented -ne $false) { throw 'Incorrect activation scope' }
if ($r.population_count_retained -ne 8 -or [Math]::Abs($r.final_body_z-24) -gt 0.001 -or $r.max_body_x -lt 540 -or [Math]::Abs($r.final_body_x-24) -gt 0.001 -or
    $r.body_tile_crossings -lt 8 -or $r.visited_body_tiles -lt 5 -or
    $r.peak_resident_tiles -gt 13 -or $r.peak_resident_tiles -lt 9 -or $r.resident_tiles -lt 4 -or
    $r.evicted_tiles -lt 10 -or $r.regenerated_tiles -lt 1 -or $r.regenerated_mesh_mismatches -ne 0 -or
    $r.shared_edge_samples -lt 100 -or $r.max_shared_height_error_m -ne 0 -or
    $r.max_shared_normal_component_error -ne 0 -or $r.support_samples -lt 20 -or
    $r.max_root_ground_error_m -ge 0.0001 -or $r.max_rendered_bounds_center_error_m -ge 0.0001 -or
    $r.rendered_support_fallbacks -ne 0) { throw "Populated terrain acceptance thresholds failed: $Evidence" }
$population = Get-Content -LiteralPath (Join-Path $Evidence 'world-population.json') -Raw | ConvertFrom-Json
$initial = @($population.actors_initial); $final = @($population.actors_final)
if ($initial.Count -ne 8 -or $final.Count -ne 8) { throw 'Eight local population members were not retained' }
$idsBefore = @($initial | ForEach-Object { $_.id })
$idsAfter = @($final | ForEach-Object { $_.id })
if (@($idsBefore | Select-Object -Unique).Count -ne 8 -or
    (ConvertTo-Json -InputObject $idsBefore -Compress) -cne (ConvertTo-Json -InputObject $idsAfter -Compress)) { throw 'Population identities changed during traversal' }
$runtime = Get-Content -LiteralPath (Join-Path $Evidence 'runtime-receipt.json') -Raw | ConvertFrom-Json
if ($runtime.populated_world -isnot [bool] -or -not $runtime.populated_world -or
    -not (Test-Number $runtime.resource_ledger_cap_exhaustions) -or $runtime.resource_ledger_cap_exhaustions -ne 0) { throw 'Actual populated host or shared resource ledger failed' }
$captures = @($r.capture_diagnostics)
if ($captures.Count -ne 4) { throw 'Missing populated boundary capture diagnostics' }
for ($i=0; $i -lt 4; ++$i) {
    $d=$captures[$i]
    if (-not (Test-Number $d.capture) -or $d.capture -ne $i -or -not (Test-Number $d.body_x)) { throw 'Invalid populated capture index/location' }
    Assert-True $d.saved "capture $i saved"
    if ($d.filename -cne "populated-terrain-$i.png") { throw 'Incorrect populated capture filename' }
    Assert-True $d.visible_object "capture $i hero visibility"
    $ranges=@(@(24,96),@(98,150),@(230,300),@(350,545))
    if ($d.body_x -lt $ranges[$i][0] -or $d.body_x -gt $ranges[$i][1]) { throw "Capture $i outside required spatial bracket" }
    $file=Join-Path $Evidence "populated-terrain-$i.png"
    if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -lt 1000) { throw "Missing populated capture $i" }
}
$result=[ordered]@{passed=$true;evidence=$Evidence;retained_actor_count=8;exact_shared_mesh_edges=$true;
    body_outbound_return=$true;support_from_rendered_triangles=$true;captures_require_visual_review=$true;receipt=$r}
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $Evidence 'populated-streaming-verification.json') -Encoding utf8
$result | ConvertTo-Json -Depth 12
