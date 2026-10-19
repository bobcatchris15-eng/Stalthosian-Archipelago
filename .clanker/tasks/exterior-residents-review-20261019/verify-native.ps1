$ErrorActionPreference='Stop'
$p='E:/Stalthosian Archipelago/var/worktrees/exterior-residents-20261019/var/reports/runtime/20261019-031208-8de37e84'
$w=Get-Content "$p/world-population.json" -Raw|ConvertFrom-Json
$old=Get-Content 'E:/Stalthosian Archipelago/var/worktrees/wildlife-awareness-20261019/var/reports/runtime/20261019-030440-9595fa10/world-population.json' -Raw|ConvertFrom-Json
$a=Get-Content "$p/wildlife-awareness.json" -Raw|ConvertFrom-Json
$h=Get-Content "$p/exterior-residents-habitat.json" -Raw|ConvertFrom-Json
$t=Get-Content "$p/populated-terrain.json" -Raw|ConvertFrom-Json
for($i=0;$i-lt 8;$i++){if(($w.snapshot_initial.actors[$i]|ConvertTo-Json -Compress)-cne($old.snapshot_initial.actors[$i]|ConvertTo-Json -Compress)){throw "Old actor $i changed"}}
if($w.snapshot_initial.actors.Count-ne 12 -or $w.snapshot_final.actors.Count-ne 12){throw 'Count'}
if(($w.resource_sessions_initial|ConvertTo-Json -Compress)-cne($w.resource_sessions_final|ConvertTo-Json -Compress)){throw 'Session owners changed'}
$new=@();for($i=8;$i-lt 12;$i++){$x=$w.snapshot_initial.actors[$i];$y=$w.snapshot_final.actors[$i];if($x.id-ne$y.id-or$y.age-le$x.age-or$y.distance_m-le0){throw 'New resident continuity'};$new+=@{id=$x.id.ToString();distance_m=$y.distance_m;age_delta=$y.age-$x.age}}
function Seg($x,$z,$a,$b){$dx=$b.x-$a.x;$dz=$b.z-$a.z;$l=$dx*$dx+$dz*$dz;$u=0.;if($l-gt 0){$u=[Math]::Max([double]0,[Math]::Min([double]1,(($x-$a.x)*$dx+($z-$a.z)*$dz)/$l))};[Math]::Sqrt([Math]::Pow($x-$a.x-$u*$dx,2)+[Math]::Pow($z-$a.z-$u*$dz,2))}
$max=0.;foreach($s in $a.samples){foreach($actor in $s.actors){$best=[double]::PositiveInfinity;foreach($site in $h.sites){foreach($j in $site.neighbors){$best=[Math]::Min($best,(Seg $actor.x $actor.z $site $h.sites[$j]))}};$max=[Math]::Max($max,$best);if($best-gt 1e-6){throw 'Off graph'}}}
if($h.exterior_resource_anchors-ne0-or$h.max_measured_grade-gt.6-or$t.rendered_support_fallbacks-ne0){throw 'Support or resource claim failed'}
[ordered]@{passed=$true;evidence=$p;old_eight_native_initial_exact=$true;retained_session_owners=$true;samples=$a.samples.Count;all_actor_graph_error_m=$max;new_residents=$new;max_grade=$h.max_measured_grade;terrain_fallbacks=$t.rendered_support_fallbacks;ground_error=$t.max_root_ground_error_m;scope='Independent inspection of worker native outputs, not independent process launch'}|ConvertTo-Json -Depth 6|Set-Content "$PSScriptRoot/native-evidence-review.json"
