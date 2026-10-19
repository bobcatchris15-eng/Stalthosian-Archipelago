param([Parameter(Mandatory)][string]$Evidence)
$ErrorActionPreference='Stop'
$samples=@(Get-Content -LiteralPath (Join-Path $Evidence 'connected-route-trajectory.json') -Raw | ConvertFrom-Json)
if($samples.Count -lt 100){throw 'Insufficient native trajectory samples'}
$maxDistanceError=0.;$maxLateral=0.;$maxGrade=0.;$crossed96=$false;$crossed128=$false
$px=24.;$pz=24.;$py=$null;$distance=0.
foreach($s in $samples){
 if($s.dt -lt 0 -or $s.dt -gt .100001){throw 'Movement dt outside bounded integrator'}
 $dx=[double]$s.x-$px;$dz=[double]$s.z-$pz;$step=[Math]::Sqrt($dx*$dx+$dz*$dz)
 $expected=[Math]::Abs([double]$s.integrator_speed_m_s)*[double]$s.dt
 $error=[Math]::Abs($step-$expected);$maxDistanceError=[Math]::Max($maxDistanceError,$error)
 if($error -gt .00004){throw "Body movement differs from ordinary speed*dt integration: $error"}
 if($expected -gt 0 -and -not $s.forward_intent){throw 'Body advanced without recorded forward intent'}
 if($s.integrator_speed_m_s -notin @(0,2)){throw 'Proof changed ordinary proxy speed'}
 if($py -ne $null -and $step -gt .0001){$grade=[Math]::Abs([double]$s.ground_y-$py)/$step;$maxGrade=[Math]::Max($maxGrade,$grade);if($grade -gt .5){throw "Route grade exceeds bounded walking corridor: $grade"}}
 $maxLateral=[Math]::Max($maxLateral,[Math]::Abs([double]$s.z-24))
 $distance+=$step;$px=[double]$s.x;$pz=[double]$s.z;$py=[double]$s.ground_y
 if($px -ge 96){$crossed96=$true};if($px -ge 128){$crossed128=$true}
}
if(-not($crossed96 -and $crossed128 -and $px -ge 200)){throw 'Actual body did not reach exterior meadow across both joins'}
if($maxLateral -gt .01){throw 'Proof deviated from sampled center corridor'}
$result=[ordered]@{passed=$true;evidence=$Evidence;sample_count=$samples.Count;ordinary_proxy_speed_m_s=2;body_end_x=$px;body_end_z=$pz;total_integrated_distance_m=$distance;max_speed_dt_displacement_error_m=$maxDistanceError;max_sampled_grade=$maxGrade;max_lateral_drift_m=$maxLateral;crossed_reference_boundary96=$crossed96;crossed_tile_boundary128=$crossed128;reached_meadow=$true;scope='Independent native body trajectory check against ordinary speed*dt integrator; player captures and emitted mesh support reviewed separately'}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'trajectory-review.json') -Encoding utf8
$result | ConvertTo-Json
