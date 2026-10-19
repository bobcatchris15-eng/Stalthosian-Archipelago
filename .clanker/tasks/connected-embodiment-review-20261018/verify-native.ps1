$ErrorActionPreference='Stop'
$base='E:/Stalthosian Archipelago/var/worktrees/connected-embodiment-20261018/var/reports/runtime'
$approach=Join-Path $base '20261018-231453-3c19a0dc';$exterior=Join-Path $base '20261018-231544-f48ca1f1'
$a=Get-Content (Join-Path $approach 'hero-autonomy.json') -Raw | ConvertFrom-Json
$ar=Get-Content (Join-Path $approach 'runtime-receipt.json') -Raw | ConvertFrom-Json
$pop=Get-Content (Join-Path $approach 'world-population.json') -Raw | ConvertFrom-Json
$er=Get-Content (Join-Path $exterior 'runtime-receipt.json') -Raw | ConvertFrom-Json
$terrain=Get-Content (Join-Path $exterior 'populated-terrain.json') -Raw | ConvertFrom-Json
$samples=@(Get-Content (Join-Path $exterior 'connected-route-trajectory.json') -Raw | ConvertFrom-Json)
foreach($r in @($ar,$er)){
 if(-not $r.creature_loaded -or $r.creature_mesh_objects -ne 4 -or $r.creature_skin_count -ne 1 -or $r.creature_animation_count -ne 2 -or $r.creature_sole_offset_m -ne 0 -or [Math]::Abs($r.walk_stride_length_m-.16128) -gt 1e-9 -or $r.animated_channel_local_pose_delta -le 1e-5){throw 'Imported renderer/metadata/animation evidence mismatch'}
}
if(@($a.contacts).Count -ne 1){throw 'Embodied contact count mismatch'}
$c=$a.contacts[0];$keys=@($pop.resource_ledger_final.sessions | Where-Object {$_.session_id -eq $c.session -and $_.resource_id -eq 100})
if($keys.Count -ne 1 -or $keys[0].amount -ne .5 -or [Math]::Abs($c.thirst_before-$c.thirst_after-.5) -gt 1e-12){throw 'Embodied authoritative contact mismatch'}
$max=0.
foreach($s in $samples){if([Math]::Abs($s.integrator_speed_m_s-.16128) -gt 1e-6){throw 'Asset pace changed'};$max=[Math]::Max($max,[Math]::Abs($s.step_distance_m-[Math]::Abs($s.integrator_speed_m_s)*$s.dt))}
if($max -gt .00001 -or $samples[0].x -lt 104 -or $samples[0].x -gt 104.05 -or $samples[-1].x -lt 105 -or $terrain.rendered_support_fallbacks -ne 0 -or $terrain.max_root_ground_error_m -gt .0001){throw 'Exterior ordinary movement/triangle support failed'}
$asset='E:/Stalthosian Archipelago/var/bristle-coherence-parent-verified-20261013/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb'
if((Get-FileHash $asset).Hash.ToLowerInvariant() -ne 'a05dc160f7b2f74a5fc8fff8cae9e701159595364b7070b17d25afe1db9eb3e3' -or (Get-FileHash ([IO.Path]::ChangeExtension($asset,'.json'))).Hash.ToLowerInvariant() -ne '6516fef2c8475e52663988590a511ed063e044022f82c710b25beb19c2e29149'){throw 'Source assets changed'}
[ordered]@{passed=$true;autonomy_evidence=$approach;exterior_evidence=$exterior;imported_meshes=4;skins=1;clips=2;sole_offset_m=0;measured_pace_m_s=.16128;hero_contact_session=$c.session;hero_contact_amount=$c.amount;exact_thirst_relief=$c.thirst_before-$c.thirst_after;exterior_samples=$samples.Count;exterior_first_x=$samples[0].x;exterior_last_x=$samples[-1].x;max_speed_dt_step_error_m=$max;max_root_ground_error_m=$terrain.max_root_ground_error_m;rendered_support_fallbacks=$terrain.rendered_support_fallbacks;source_asset_hashes_unchanged=$true;outside_fixture_sole_contact_accepted=$false;full_animal_route_crossing_proven=$false;views_inspected=@('hero-autonomy-1.png','draft-host.png');tested_executable_sha256=(Get-Content (Join-Path $exterior 'launch.json') -Raw | ConvertFrom-Json).executable_sha256} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $PSScriptRoot 'native-evidence-review.json') -Encoding utf8
Get-Content (Join-Path $PSScriptRoot 'native-evidence-review.json')
