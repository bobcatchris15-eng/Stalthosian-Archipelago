$ErrorActionPreference='Stop'
$base='E:/Stalthosian Archipelago/var/worktrees/wildlife-awareness-20261019/var/reports/runtime'
$approachPath=Join-Path $base '20261019-030440-9595fa10';$cameraPath=Join-Path $base '20261019-030513-1294811b'
$a=Get-Content (Join-Path $approachPath 'wildlife-awareness.json') -Raw | ConvertFrom-Json
$c=Get-Content (Join-Path $cameraPath 'wildlife-awareness.json') -Raw | ConvertFrom-Json
$p=Get-Content (Join-Path $approachPath 'world-population.json') -Raw | ConvertFrom-Json
$watched=@($a.samples|ForEach-Object {$_.actors|Where-Object {$_.id.ToString() -eq $a.watched_actor.ToString()}})
if(-not @($watched|Where-Object {$_.noticed}).Count -or -not @($watched|Where-Object {$_.avoid}).Count -or $watched[-1].noticed -or $watched[-1].distance_travelled -le 0){throw 'Notice/avoid/withdraw sequence absent'}
$initialIds=@($p.snapshot_initial.actors|ForEach-Object {$_.id.ToString()}|Sort-Object);$finalIds=@($p.snapshot_final.actors|ForEach-Object {$_.id.ToString()}|Sort-Object)
if(($initialIds -join ',') -cne ($finalIds -join ',') -or ($p.resource_sessions_initial|ConvertTo-Json -Compress) -cne ($p.resource_sessions_final|ConvertTo-Json -Compress)){throw 'Retained identities/session owners changed'}
function SegmentDistance($x,$z,$a,$b){$dx=$b.x-$a.x;$dz=$b.z-$a.z;$len=$dx*$dx+$dz*$dz;if($len -eq 0){$t=0}else{$t=[Math]::Max([double]0,[Math]::Min([double]1,(($x-$a.x)*$dx+($z-$a.z)*$dz)/$len))};return [Math]::Sqrt([Math]::Pow($x-($a.x+$dx*$t),2)+[Math]::Pow($z-($a.z+$dz*$t),2))}
$maxGraphError=0.;$maxHeroStepError=0.;$previous=$null
foreach($s in $a.samples){
 foreach($actor in $s.actors){$best=[double]::PositiveInfinity;for($i=0;$i -lt $p.habitat_sites.Count;$i++){foreach($j in $p.habitat_sites[$i].neighbors){$best=[Math]::Min($best,(SegmentDistance $actor.x $actor.z $p.habitat_sites[$i] $p.habitat_sites[$j]))}};$maxGraphError=[Math]::Max($maxGraphError,$best);if($best -gt 1e-6){throw "Native actor $($actor.id) at$($actor.x),$($actor.z) is off authored graph by$best atelapsed$($s.elapsed)"}}
 if($null -ne $previous){$dx=$s.body_x-$previous.body_x;$dz=$s.body_z-$previous.body_z;$step=[Math]::Sqrt($dx*$dx+$dz*$dz);if($step -gt 2*($s.elapsed-$previous.elapsed)+.201){throw 'Hero moved beyond ordinary integrator bound'}};$previous=$s
}
$first=$c.samples[0];$maxCameraSeparation=0.
foreach($s in $c.samples){if($s.body_x -ne $first.body_x -or $s.body_z -ne $first.body_z -or @($s.actors|Where-Object {$_.noticed -or $_.avoid}).Count){throw 'Camera-only granted reaction or moved body'};$maxCameraSeparation=[Math]::Max($maxCameraSeparation,[Math]::Sqrt([Math]::Pow($s.camera_x-$s.body_x,2)+[Math]::Pow($s.camera_z-$s.body_z,2)))}
if($maxCameraSeparation -lt 24){throw 'Detached camera never reached remote actor area'}
$result=[ordered]@{passed=$true;approach_evidence=$approachPath;camera_evidence=$cameraPath;approach_samples=@($a.samples).Count;camera_samples=@($c.samples).Count;watched_actor=$a.watched_actor.ToString();watched_notice_samples=@($watched|Where-Object {$_.noticed}).Count;watched_avoid_samples=@($watched|Where-Object {$_.avoid}).Count;watched_final_distance_travelled_m=$watched[-1].distance_travelled;withdrawal_cleared_notice=$true;all_actor_graph_error_m=$maxGraphError;exact_identity_and_session_owner_preservation=$true;camera_body_fixed=$true;camera_reaction_count=0;max_camera_body_separation_m=$maxCameraSeparation;scope='Independent actual domain/native graph/body/camera/ownership proof; proxy visuals reviewed separately, synthetic hero startup/input declared'}
$result|ConvertTo-Json -Depth 5|Set-Content (Join-Path $PSScriptRoot 'native-evidence-review.json') -Encoding utf8
$result|ConvertTo-Json -Depth 5
