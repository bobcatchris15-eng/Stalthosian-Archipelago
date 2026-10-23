$ErrorActionPreference='Stop'
$t='E:/Stalthosian Archipelago/.clanker/tasks/longstride-parent-care-20261023'
$s='E:/Stalthosian Archipelago/var/test-saves/longstride-parent-care-20261023'
& 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe' --test-dir var/build/stalthos-runtime -C Release --output-on-failure > "$t/ctest.log"
if($LASTEXITCODE -ne 0){throw 'CTest failed'}
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ParentCareProbe -Duration 28 > "$t/native-care.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ParentCareCameraProbe -Duration 12 > "$t/native-camera.log"
./Play.ps1 -HeroSpecies Longstride -Duration 20 > "$t/native-normal.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedPersistenceProbe -HeroSpecies Longstride -ConnectedSave "$s/first.stlc" -Duration 22 > "$t/native-partial-save.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedLoad "$s/first.stlc" -ConnectedSave "$s/restarted.stlc" -Duration 8 > "$t/native-partial-load.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -HatchProbe -CalendarScale 100 -Duration 34 > "$t/native-small-hatch.log"
foreach($sp in @('Smalltrotter','Thistletail','Brackenback','Reedcrest')){./Play.ps1 -HeroSpecies $sp -Duration 5 > "$t/native-$sp.log"}
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -Duration 5 > "$t/native-original.log"
