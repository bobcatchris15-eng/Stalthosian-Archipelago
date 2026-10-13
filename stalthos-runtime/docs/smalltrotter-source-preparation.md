# Smalltrotter source preparation utility

`scripts/assets/prepare_smalltrotter_source.py` provides read-only audit mode and a conservative derivative-only seam-preparation mode. Run it with Blender and `--disable-autoexec`, for example:

```powershell
blender.exe -b --disable-autoexec --python stalthos-runtime/scripts/assets/prepare_smalltrotter_source.py -- --mode audit --input C:/path/smalltrotter.blend --receipt var/smalltrotter-source-prep/audit.json
```

Audit opens the given file without saving it, requires the named `Smalltrotter_Body` mesh, and reports base topology, bounds, boundary counts, connected-component sizes, Multires levels, seam candidates, and coordinate/polygon fingerprints for the other mesh objects. It considers only body boundary vertices within `1e-4` of the local X=0 plane, only pairs across different connected components, and only pairs within `1e-4` Euclidean distance. Candidate output is sorted by vertex indices. Any candidate with multiple neighbors is marked ambiguous; seam-preparation refuses the entire operation when ambiguity is present.

Seam-preparation requires a separate output path that does not already exist and does not resolve to the source path. It works on `Smalltrotter_Body` only, requires exactly two source components, copies its base mesh, removes Multires from the derivative before topology edits, and welds only the explicit target map of proven pairs. It does not alter the source file, eyes, or bristles. After saving, it reopens the derivative and verifies unchanged eye/bristle coordinate and polygon-index fingerprints. The source SHA-256 is checked again. A derivative is not a watertightness or anatomy acceptance claim.

`--selftest` uses explicit failure checks rather than `assert`, so it remains active under optimized Python (`-O`). It covers one deterministic cross-component pair, repeat ordering, ambiguity, no-match, same-component rejection, and nonfinite coordinates. Missing `bpy` produces structured JSON and exit code 4.

## Run against the supplied source

Blender 5.2.0 LTS audit and seam attempt were run on `C:\Users\Chris\Downloads\smalltrotter.blend` with auto-execution disabled. Its SHA-256 before and after is `B987E7D7EAC4DFE0E09E2E188A592236ED57EB5E00DA7D8F7E6D1D901DE15350`.

The body base mesh has 34,144 vertices, 67,460 edges, and 33,318 faces. Its bounds are X `[-0.487511, 0.487511]`, Y `[-1.711490, 3.047774]`, Z `[0, 2.598147]`. It consists of two 17,072-vertex components, has 1,648 boundary edges and zero edges incident to more than two faces. Its Multires modifier has three total, sculpt, and render levels. The unmodified other meshes are 40,300-vertex/39,000-face bristles and 964-vertex/1,024-face eyes; audit fingerprints and their unchanged topology are in the JSON receipt.

The audit found 816 unambiguous candidate pairs but also 16 ambiguous near-plane boundary vertices (sample indices `2610, 2611, 2737, 2745, 2747, 2835, 3554, 3555, 3681, 3689, 3691, 3779, 3895, 3897, 4378, 4380`). Therefore the final seam-preparation run failed closed and created no derivative. This is an honest blocked preparation result: the candidate pairing rule needs a better supported disambiguation or the source topology needs manual review. No automatic subset weld was attempted.

Receipts and logs from actual invocations are kept in the ignored worktree path `var/smalltrotter-source-prep/` (`audit-final.json`, `seam-blocked.json`, and corresponding logs). The final corrected run reports 816 unambiguous candidate pairs plus 16 ambiguous vertices and exits `failed_closed` before saving; no valid prepared derivative exists. The source hash after the audit and failed attempt is still the original SHA-256 above.

An earlier experimental derivative was emitted before an ambiguity-reporting defect was corrected. Its receipt reports a tentative body result of 33,328 vertices, 66,649 edges, 33,318 faces, one component, and 26 boundary edges, with original bounds preserved. Although that earlier version reopened the saved file and verified the eye and bristle fingerprints, it had incorrectly discarded the 16 ambiguous indices before the ambiguity gate. Treat that derivative and its receipt (`smalltrotter-seam-derivative-verified.blend`, hash `ef56436f7ef2a53c3460a5b46581d3cceea4802f76e567a64b4e9c78249e2f87`) as invalid historical evidence; do not use it. It remains under ignored `var` to preserve the experiment. The corrected script itself was not used to create a derivative. No file in the current creature/runtime asset path was changed.

## Independent parent execution
Parent copied correctedbadceba script/documentation into the main project and ran Python -O selftest (six active checks), then Blender5.2 audit against the tracked rough master. Actualauditexit0 reports816candidatepairs/16ambiguousvertices. Separate seam-preparation exits5 with failed_closed, creates no output and leaves sourcehash unchanged. Parent rawreports var/reports/packet-twentytwo-smalltrotter-parent-result.json and var/smalltrotter-source-parent-20261013/{audit,seam}.json. No valid prepared derivative exists. Original Jules9a5682e and invalid experimental outputs remain isolated and must not be used. Next diagnosis must inspect16indices/adjacency and actualcross-component distances before any controlledrepair; no genericmergebypass or currentcreature replacement.
