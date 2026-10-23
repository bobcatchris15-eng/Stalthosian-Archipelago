$ErrorActionPreference='Stop'
$t='E:/Stalthosian Archipelago/.clanker/tasks/npc-carrion-diet-20261023'
$s='E:/Stalthosian Archipelago/var/test-saves/npc-carrion-diet-20261023'
./Play.ps1 -HeroSpecies Smalltrotter -PartialPersistence -ConnectedSave "$s/first.stlc" -Duration 115 > "$t/native-partial-save.log"
./Play.ps1 -ConnectedLoad "$s/first.stlc" -ConnectedSave "$s/restarted.stlc" -Duration 8 > "$t/native-partial-load.log"
./Play.ps1 -HeroSpecies Longstride -Duration 10 > "$t/native-ordinary-long.log"
foreach($sp in @('Thistletail','Brackenback','Reedcrest')){./Play.ps1 -HeroSpecies $sp -Duration 5 > "$t/native-$sp.log"}
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -SaveState "$s/original.stl9" -Duration 6 > "$t/native-original-save.log"
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -LoadState "$s/original.stl9" -SaveState "$s/original-restarted.stl9" -Duration 5 > "$t/native-original-load.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -HatchProbe -CalendarScale 100 -Duration 34 > "$t/native-small-hatch.log"

