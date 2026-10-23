$ErrorActionPreference='Stop'
$task='E:/Stalthosian Archipelago/.clanker/tasks/connected-persistence-20261023'
$saves='E:/Stalthosian Archipelago/var/test-saves/connected-persistence-20261023'
& 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe' --test-dir var/build/stalthos-runtime -C Release --output-on-failure > "$task/ctest-final.log"
if($LASTEXITCODE -ne 0){throw 'CTest failed'}
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -HeroSpecies Longstride -ConnectedPersistenceProbe -ConnectedSave "$saves/final-first.stlc" -Duration 22 > "$task/native-primary.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedLoad "$saves/final-first.stlc" -ConnectedSave "$saves/final-restarted.stlc" -Duration 8 > "$task/native-restart.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedPersistenceCameraProbe -ConnectedSave "$saves/final-camera.stlc" -Duration 12 > "$task/native-camera.log"
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedLoad "$saves/final-camera.stlc" -ConnectedSave "$saves/final-camera-restarted.stlc" -Duration 8 > "$task/native-camera-restart.log"
foreach($species in @('Smalltrotter','Thistletail','Brackenback','Reedcrest')){
 ./Play.ps1 -HeroSpecies $species -PartialPersistence -ConnectedSave "$saves/final-$species.stlc" -Duration 6 > "$task/native-$species-save.log"
 ./Play.ps1 -ConnectedLoad "$saves/final-$species.stlc" -ConnectedSave "$saves/final-$species-restarted.stlc" -Duration 5 > "$task/native-$species-load.log"
}
foreach($species in @('Smalltrotter','Thistletail','Brackenback','Reedcrest','Longstride')){./Play.ps1 -HeroSpecies $species -Duration 5 > "$task/native-$species-normal.log"}
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -CourtshipProbe -Duration 16 > "$task/native-courtship.log"
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -SaveState "$saves/final-original.stl9" -Duration 6 > "$task/native-original-save.log"
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -LoadState "$saves/final-original.stl9" -SaveState "$saves/final-original-restarted.stl9" -Duration 5 > "$task/native-original-load.log"
