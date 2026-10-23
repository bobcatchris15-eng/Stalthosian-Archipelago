$ErrorActionPreference='Stop'
$task='E:/Stalthosian Archipelago/.clanker/tasks/connected-persistence-20261023'
$saves='E:/Stalthosian Archipelago/var/test-saves/connected-persistence-20261023'
$originalHash=(Get-FileHash "$saves/final-first.stlc").Hash
$bytes=[IO.File]::ReadAllBytes("$saves/final-first.stlc");$bytes[60]=$bytes[60] -bxor 1;[IO.File]::WriteAllBytes("$saves/corrupt-owned.stlc",$bytes)
$cases=@(
 @{Name='corrupt';Options=@{ConnectedLoad="$saves/corrupt-owned.stlc";ConnectedSave="$saves/final-first.stlc"}},
 @{Name='asset-mismatch';Options=@{ConnectedLoad="$saves/final-first.stlc";ConnectedSave="$saves/final-first.stlc";Proxy=$true}},
 @{Name='species-mismatch';Options=@{ConnectedLoad="$saves/final-first.stlc";ConnectedSave="$saves/final-first.stlc";HeroSpecies='Thistletail'}},
 @{Name='original-in-connected';Options=@{ConnectedLoad="$saves/final-original.stl9";ConnectedSave="$saves/final-first.stlc"}}
)
$result=@()
foreach($case in $cases){$failed=$false;try{$argsForCase=$case.Options;./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 @argsForCase -Duration 3 > "$task/failure-$($case.Name).log"}catch{$failed=$true;$_ | Out-String | Add-Content "$task/failure-$($case.Name).log"};if(-not $failed){throw "Expected refusal: $($case.Name)"};if((Get-FileHash "$saves/final-first.stlc").Hash -ne $originalHash){throw 'Destination changed'};$result+=@{case=$case.Name;refused=$failed;destination_preserved=$true}}
$refused=$false;try{./stalthos-runtime/scripts/Run.ps1 -ConnectedWorld -SaveState "$saves/never-original-preview.stl9" -Duration 3 > "$task/failure-old-connected-flags.log"}catch{$refused=$true;$_ | Out-String | Add-Content "$task/failure-old-connected-flags.log"}
if(-not $refused -or (Test-Path "$saves/never-original-preview.stl9")){throw 'Old connected save refusal changed'}
$result+=@{case='old-connected-flags';refused=$true;no_destination_created=$true}
$result | ConvertTo-Json -Depth 6 | Set-Content "$task/native-failure-verification.json"
