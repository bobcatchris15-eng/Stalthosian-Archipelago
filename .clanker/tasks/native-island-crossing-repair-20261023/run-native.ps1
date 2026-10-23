param([double]$CrossingDuration=240)
$ErrorActionPreference='Stop'
$task='.clanker/tasks/native-island-crossing-repair-20261023'
$save='var/test-saves/native-island-crossing-repair-20261023'
New-Item -ItemType Directory -Force $save | Out-Null
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -IslandCrossingProbe -Duration $CrossingDuration -AssetRoot 'E:/Stalthosian Archipelago' > "$task/native-crossing.log" 2>&1
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -IslandCameraProbe -Duration 16 -AssetRoot 'E:/Stalthosian Archipelago' > "$task/native-camera.log" 2>&1
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -IslandCrossingProbe -HeroSpecies Longstride -Duration 8 -AssetRoot 'E:/Stalthosian Archipelago' > "$task/native-long-start.log" 2>&1
foreach($species in @('Smalltrotter','Thistletail','Brackenback','Reedcrest','Longstride')){
 ./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -HeroSpecies $species -Duration 5 -AssetRoot 'E:/Stalthosian Archipelago' > "$task/native-ordinary-$species.log" 2>&1
}
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -Duration 5 -SaveState "$save/original.stl" > "$task/native-original-save.log" 2>&1
./stalthos-runtime/scripts/Run-PopulatedWorld.ps1 -Duration 5 -LoadState "$save/original.stl" > "$task/native-original-load.log" 2>&1
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedPersistence -Duration 5 -ConnectedSave "$save/partial.stlc" > "$task/native-partial-save.log" 2>&1
./stalthos-runtime/scripts/Run-ConnectedWorld.ps1 -ConnectedPersistence -Duration 5 -ConnectedLoad "$save/partial.stlc" > "$task/native-partial-load.log" 2>&1
