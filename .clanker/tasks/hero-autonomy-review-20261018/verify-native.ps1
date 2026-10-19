param([string]$EvidenceRoot='E:/Stalthosian Archipelago/var/worktrees/hero-autonomy-20261018/var/reports/runtime')
$ErrorActionPreference='Stop'
$names=@('20261018-230435-b889fd18','20261018-230534-ae59c182','20261018-230600-822b2ce1')
$results=@()
foreach($name in $names){
 $path=Join-Path $EvidenceRoot $name
 $r=Get-Content (Join-Path $path 'hero-autonomy.json') -Raw | ConvertFrom-Json
 $runtime=Get-Content (Join-Path $path 'runtime-receipt.json') -Raw | ConvertFrom-Json
 $population=Get-Content (Join-Path $path 'world-population.json') -Raw | ConvertFrom-Json
 $launch=Get-Content (Join-Path $path 'launch.json') -Raw | ConvertFrom-Json
 if($r.probe_mode -in @(1,2)){
  if(@($r.contacts).Count -ne 1 -or $runtime.drink_contacts_applied -ne 1){throw "Expected exactly one actual hero contact: $name"}
  $c=$r.contacts[0];$keys=@($population.resource_ledger_final.sessions | Where-Object {$_.session_id -eq $c.session -and $_.resource_id -eq 100})
  if($keys.Count -ne 1 -or $keys[0].amount -ne .5 -or $c.amount -ne .5 -or -not $c.eligible -or [Math]::Abs($c.thirst_before-$c.thirst_after-.5) -gt 1e-12){throw 'Real contact/ledger/need relief mismatch'}
  $p=$r.samples[0];foreach($s in $r.samples | Select-Object -Skip 1){$dx=$s.body_x-$p.body_x;$dz=$s.body_z-$p.body_z;$step=[Math]::Sqrt($dx*$dx+$dz*$dz);if($step -gt 2*($s.elapsed-$p.elapsed)+.201){throw 'Body sample exceeds bounded ordinary movement'};$p=$s}
  if([Math]::Abs($r.samples[-1].body_x-$r.samples[0].body_x) -lt 10){throw 'No meaningful physical approach'}
 }
 $direct=@($r.samples | Where-Object {$_.synthetic_direct})
 if($r.probe_mode -eq 2){
  if($direct.Count -lt 5 -or @($direct | Where-Object {-not $_.direct -or $_.authority -ne 2 -or $_.auto_active -or $_.auto_forward -or $_.auto_contact}).Count){throw 'Direct input failed priority'}
  if(-not @($r.samples | Where-Object {$_.elapsed -gt 4 -and $_.auto_forward}).Count){throw 'Autonomy did not resume after direct release'}
 }
 if($r.probe_mode -eq 3){
  if(@($r.contacts).Count -or @($r.samples | Where-Object {$_.body_x -ne 40 -or $_.body_z -ne 24 -or $_.body_attention -or $_.auto_active -or $_.auto_forward -or $_.auto_contact}).Count){throw 'Camera-only trial granted knowledge/moved body/contact'}
  if(-not @($r.samples | Where-Object {[Math]::Abs($_.camera_target_x-40) -gt 24}).Count){throw 'Camera never reached resource side independently'}
 }
 $results+=[ordered]@{probe_mode=$r.probe_mode;evidence=$path;sample_count=@($r.samples).Count;contact_count=@($r.contacts).Count;direct_priority_samples=$direct.Count;executable_sha256=$launch.executable_sha256;passed=$true}
}
[ordered]@{passed=$true;cases=$results;scope='Independently inspected native decisions, contacts, per-session full ledger, need relief, sampled movement/direct priority/camera-only separation; synthetic setup explicitly retained'} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $PSScriptRoot 'native-evidence-review.json') -Encoding utf8
Get-Content (Join-Path $PSScriptRoot 'native-evidence-review.json')
