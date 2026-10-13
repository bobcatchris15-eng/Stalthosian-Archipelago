# Smalltrotter Skin-Scale Surfacing Refinement

## Summary

This work delivers a bounded refinement to the Smalltrotter dwarf titanosaur procedural skin surfacing against the user reference (`codex-clipboard-0ed107a6-e5c9-456d-977e-18bcc1d9a265.png`), replacing previous high-contrast chocolate blotches with a coherent warm ochre/tan hide featuring fine natural scale rosettes, fine wrinkles, and restrained organic umber mottling.

## Key Observations & Problems Addressed

1. **Polygon-Scale Patchwork / Camouflage Blotches:**
   - The previous procedural hide map used coarse noise (`cells=7`, `cells=27`) with sharp step thresholds that cut across the mesh's Smart-UV unwrap, creating large, arbitrary chocolate camouflage patches and angular polygon-boundary discontinuities across the flank and dorsal trunk.
2. **Missing Fine Scale & Wrinkle Structure:**
   - Previous hide had little visible scale relief or epidermal texture. The refined implementation introduces a deterministic periodic Voronoi rosette structure (`cells=36`) with rounded scale domes and interstitial grooved margins, paired with high-frequency wrinkle/fold striations.
3. **Roughness Modulation:**
   - The scale domes are modulated with lower roughness (~0.52) providing tight specular highlights on convex scale crests, while interstitial grooves and fine epidermal pores have higher roughness (~0.76).
4. **Ventral Throat Tracking:**
   - The ventral throat material assignment in `creature.py` was refined with `poly.normal.z < 0.20` so the pale cream bib cleanly traces the anterior throat and chin without spilling onto upward-facing dorsal neck segments.
5. **Preserved Boundaries:**
   - All geometry, skeleton joints, limb chains, foot toes/nails, eye structure, and authored glTF metallic-roughness PBR channels remain strictly preserved.
   - All randomness is SHA-256 derived from the supplied integer seed (`seed=42`), requiring no external, downloaded, or paid assets.

## Validation & Verification

- **Offline tests:**
  - `PYTHONPATH=src C:/Python314/python.exe -m pytest tests/test_reference_surfacing.py tests/test_creature_materials.py tests/test_baked_export.py tests/test_authored_material_channels.py -q` passed (18/18 tests passing).
- **Blender Execution:**
  - Blender 5.2 execution verified with `build_creature` and `render_turntable` on stage `independent_juvenile` with seed `42`.
  - Artifact hashes:
    - Blend source: `b0db1cda6dde3c4e535cb955f9330aa4478bb934e85dba2425dbb9217c983c35`
    - Turntable preview 000: `b51926fe78b49b846dbdcf92224a02c9f40c2fd2a1ed821c8741090221f4e972`
    - Turntable preview 090: `1115c37ef1cd7dc7b79bb89d0d470703b955e0e67dd125f6dd2cad719aa089a2`
    - Turntable preview 180: `ea443a6e44bcf83dbc5b9cde1ee240a9c08c7d3ea901829d053179043dc57f35`
    - Turntable preview 270: `5265c19916d07c4a514045429f92049853e38245cdd4ec431caa9902e3150324`

## Limitations & Non-Goals

- Material improvement is provisional look development, not full reference parity.
- The underlying proxy mesh remains coarse polygonal blockout geometry; silhouette polygonal facets reflect mesh resolution rather than material mapping.

Parent independently built, rigged and rendered this frozen source in
var/smalltrotter-skin-verified-asset. All18 artifact hashes match, and source hashes
remained stable through the full1988passed/2skipped suite (563.54s). Actual Wicked
20261013-012924-33e7a641 loads the rebuilt asset. Softer tan mottling is visible;
coarse anatomy, pale throat seams and final reference parity remain unaccepted.
