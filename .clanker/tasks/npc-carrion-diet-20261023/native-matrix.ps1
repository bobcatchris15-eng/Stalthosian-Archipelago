$ErrorActionPreference='Stop'
$t='E:/Stalthosian Archipelago/.clanker/tasks/npc-carrion-diet-20261023'
$s='E:/Stalthosian Archipelago/var/test-saves/npc-carrion-diet-20261023'
& 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe' --test-dir var/build/stalthos-runtime -C Release --output-on-failure > "$t/ctest.log"
if($LASTEXITCODE -ne 0){throw 'CTest failed'}
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -NpcCarrionProbe -Duration 85 > "$t/native-competition.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -NpcCarrionCameraProbe -Duration 18 > "$t/native-camera.log"
./Play.ps1 -HeroSpecies Smalltrotter -Duration 75 > "$t/native-ordinary-small.log"
./Play.ps1 -HeroSpecies Smalltrotter -PartialPersistence -ConnectedSave "$s/first.stlc" -Duration 125 > "$t/native-partial-save.log"
./Play.ps1 -ConnectedLoad "$s/first.stlc" -ConnectedSave "$s/restarted.stlc" -Duration 8 > "$t/native-partial-load.log"
./Play.ps1 -HeroSpecies Longstride -Duration 10 > "$t/native-ordinary-long.log"
foreach($sp in @('Thistletail','Brackenback','Reedcrest')){./Play.ps1 -HeroSpecies $sp -Duration 5 > "$t/native-$sp.log"}
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -SaveState "$s/original.stl9" -Duration 6 > "$t/native-original-save.log"
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -LoadState "$s/original.stl9" -SaveState "$s/original-restarted.stl9" -Duration 5 > "$t/native-original-load.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -HatchProbe -CalendarScale 100 -Duration 34 > "$t/native-small-hatch.log"

