# Luna snapshot v2 evidence

Base: `28c1bdd0c468004c4c8c86096621a3f4f723b712`. Work remained inside the assigned isolated worktree. No host, CMake, toolkit, planning-input, network, or commit changes were made.

Configured the standalone domain build using the requested Visual Studio CMake executable:

```text
cmake.exe -S stalthos-runtime -B var/build/luna-snapshot-v2 -DSTALTHOS_BUILD_WICKED_HOST=OFF -DCMAKE_BUILD_TYPE=Release
```

Configuration completed with MSVC 19.51.36243.0. The Visual Studio generator reported `CMAKE_BUILD_TYPE` is unused; Release was selected on the build command:

```text
cmake.exe --build var/build/luna-snapshot-v2 --config Release --parallel 4
```

Build completed successfully with `/W4 /WX /permissive-`.

```text
ctest.exe --test-dir var/build/luna-snapshot-v2 -C Release --output-on-failure
```

```text
1/2 Test #1: domain-scenarios ................. Passed
2/2 Test #2: domain-persistence ............... Passed
100% tests passed, 0 tests failed out of 2
Total Test time (real) = 0.19 sec
```

The scenario executable reports 7/7 groups passed. Persistence reports 8/8 groups passed, including full capture/write/read/restore/recapture byte equivalence, STL1 compatibility, malformed save rejection, and prior-save preservation. Full CTest details are preserved at `var/build/luna-snapshot-v2/Testing/Temporary/LastTest.log`.