$ErrorActionPreference='Stop'
$t='E:/Stalthosian Archipelago/.clanker/tasks/npc-carrion-diet-20261023'
$s='E:/Stalthosian Archipelago/var/test-saves/npc-carrion-diet-20261023'
./Play.ps1 -ConnectedLoad "$s/restarted.stlc" -ConnectedSave "$s/drained.stlc" -Duration 8 > "$t/native-drain.log"
./Play.ps1 -ConnectedLoad "$s/drained.stlc" -ConnectedSave "$s/drained-restarted.stlc" -Duration 5 > "$t/native-drained-load.log"
$old='E:/Stalthosian Archipelago/var/test-saves/connected-persistence-20261023/final-first.stlc'
$oldHash=(Get-FileHash $old).Hash;$newHash=(Get-FileHash "$s/drained.stlc").Hash
$refused=$false
try{./Play.ps1 -ConnectedLoad $old -ConnectedSave "$s/drained.stlc" -Duration 3 > "$t/failure-old-load.log"}catch{$refused=$true;$_ | Out-String | Add-Content "$t/failure-old-load.log"}
if(-not $refused -or (Get-FileHash "$s/drained.stlc").Hash -ne $newHash){throw 'Old config load was not safely refused'}
Copy-Item -LiteralPath $old -Destination "$s/old-copy-owned.stlc"
./Play.ps1 -HeroSpecies Smalltrotter -PartialPersistence -ConnectedSave "$s/old-copy-owned.stlc" -Duration 5 > "$t/failure-old-write.log"
if((Get-FileHash "$s/old-copy-owned.stlc").Hash -ne $oldHash -or (Get-FileHash $old).Hash -ne $oldHash){throw 'Old copy/original changed'}
$oldFlagRefused=$false
try{./stalthos-runtime/scripts/Run.ps1 -ConnectedWorld -SaveState "$s/forbidden-old-flags.stl9" -Duration 3 > "$t/failure-old-flags.log"}catch{$oldFlagRefused=$true;$_ | Out-String | Add-Content "$t/failure-old-flags.log"}
if(-not $oldFlagRefused -or (Test-Path "$s/forbidden-old-flags.stl9")){throw 'Old connected flags unexpectedly wrote'}
@{old_load_refused=$refused;new_destination_preserved=$true;old_original_and_owned_copy_preserved=$true;old_connected_flags_refused=$oldFlagRefused} | ConvertTo-Json | Set-Content "$t/native-failure-verification.json"
