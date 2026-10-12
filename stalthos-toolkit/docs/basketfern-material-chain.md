# Basketfern Material and Export Chain Receipt

## Execution Context

- **Date / Time**: 2026-10-12 11:53 Central
- **Branch**: `codex/basketfern-material-chain`
- **Seed**: `42`
- **Definition**: `stalthos-toolkit/species/plants/basketfern.yaml` (`plant.basketfern.fn2`)
- **Python**: `C:/Python314/python.exe` (3.14.5)
- **Blender**: `C:/Program Files/Blender Foundation/Blender 5.2/blender.exe` (5.2.0 LTS)
- **Dedicated Root Workspace**: `E:/Stalthosian Archipelago/var/basketfern-material-chain-asset` (untracked under `var/`)

---

## 1. Context & Fixture Comparison

Prior test fixture `var/resource-contact-plant/plant.basketfern.fn2.fixture.glb` (248,044 bytes):
- Rendered completely flat white in Wicked Engine runtime because it contained **0 embedded images** and **0 textures**, referencing only placeholder engine material slots (`tree_foliage`, `tree_bark`) without bound texture channels.
- Mesh bounding box: height ~3.909 m (Y-up: min `Y=0.0`, max `Y=3.909 m`), X extent ~2.11 m, Z extent ~0.77 m.

Goal:
Surfaced Basketfern asset with true baked PBR maps (Albedo, Roughness, Normal, Thickness) bound to exportable glTF 2.0 PBR materials for Wicked Engine import.

---

## 2. CLI Execution & Pipeline Stages

### Stage 3: High-Resolution Generation (`generate_plant`)

Command:
```powershell
C:/Python314/python.exe -m stalthos_toolkit.cli generate_plant \
  --definition species/plants/basketfern.yaml \
  --workspace "E:/Stalthosian Archipelago/var/basketfern-material-chain-asset" \
  --seed 42
```

Stdout JSON status envelope:
- `ok`: `true`
- `exit_code`: `0`
- `asset_id`: `plant.basketfern.fn2`
- `duration_s`: `1.834`
- Produced editable source blend: `sources/plant.basketfern.fn2/plant.basketfern.fn2.blend`
- Mesh metrics: 2 objects (`tree_structure`, `tree_foliage`), 5,460 triangles, 5,502 vertices, height 3.9091 m, spread 2.1139 m.

### Stage 4: Texture / Material Bake (`bake_materials`)

Help inspection:
- `stalthos bake_materials --help` supports options: `--definition`, `--workspace`, `--seed`, `--stage`, `--variant`, `--out`, `--blender`, `--timeout`, `--strict`, `--dry-run`, `--allow-degraded`, `--levels`.
- Resolution defaults to 512, derived from `ctx.options.get("resolution") or definition.get("texture_resolution") or 512`. Bounded execution run at 512x512 resolution (standard memory footprint for test verification).

Command:
```powershell
C:/Python314/python.exe -m stalthos_toolkit.cli bake_materials \
  --definition species/plants/basketfern.yaml \
  --workspace "E:/Stalthosian Archipelago/var/basketfern-material-chain-asset" \
  --seed 42
```

Stdout JSON status envelope:
- `ok`: `true`
- `exit_code`: `0`
- `asset_id`: `plant.basketfern.fn2.bake`
- `duration_s`: `5.106`
- `directional_light_baked`: `false` (albedo produced via emission shader in a black world without lights)
- `material_export_ready`: `true`
- `authored_material_complete`: `true`
- Produced baked blend master: `intermediate/bakes/plant.basketfern.fn2.baked.blend`
- Generated baked maps:
  - `tree_foliage_albedo.png`
  - `tree_structure_albedo.png`
  - `tree_foliage_roughness.png`
  - `tree_structure_roughness.png`
  - `tree_foliage_normal.png`
  - `tree_structure_normal.png`
  - `tree_foliage_thickness.png`
  - `tree_structure_thickness.png`

### Stage 8: Review Turntable Render (`render_turntable`)

Rendered from the baked master blend to evaluate actual surfaced coloration and silhouette:

Command:
```powershell
C:/Python314/python.exe -m stalthos_toolkit.cli render_turntable \
  --definition species/plants/basketfern.yaml \
  --workspace "E:/Stalthosian Archipelago/var/basketfern-material-chain-asset" \
  --seed 42 \
  --out "E:/Stalthosian Archipelago/var/basketfern-material-chain-asset/intermediate/bakes/plant.basketfern.fn2.baked.blend"
```

Stdout JSON status envelope:
- `ok`: `true`
- `exit_code`: `0`
- `duration_s`: `5.544`
- Angles: 0°, 90°, 180°, 270°
- Renders:
  - `plant.basketfern.fn2_neutral_000.png`
  - `plant.basketfern.fn2_neutral_090.png`
  - `plant.basketfern.fn2_neutral_180.png`
  - `plant.basketfern.fn2_neutral_270.png`

### Stage: glTF / GLB Export (`export_baked.py`)

Blender glTF 2.0 export with verified PBR channel bindings:

Command:
```powershell
C:/Program Files/Blender Foundation/Blender 5.2/blender.exe \
  --background --factory-startup -noaudio --disable-autoexec --python-exit-code 1 \
  --python "E:/Stalthosian Archipelago/var/worktrees/basketfern-material-chain/stalthos-toolkit/src/stalthos_toolkit/blender/scripts/export_baked.py" \
  -- --params "E:/Stalthosian Archipelago/var/basketfern-material-chain-asset/exports/export_params.json"
```

Verification status:
- `ok`: `true`
- `output_glb`: `exports/plant.basketfern.fn2.baked.glb` (1,151,760 bytes)
- `material_bindings`:
  - `ok`: `true`
  - `materials_checked`: 2 (`tree_foliage_baked`, `tree_bark_baked`)
  - `texture_bindings_checked`: 6 (Albedo, Roughness, Normal per material)
  - `issues`: `[]`

---

## 3. Hashed Artifact Registry

All raw files are stored in `E:/Stalthosian Archipelago/var/basketfern-material-chain-asset/`:

| Relative Path | Bytes | SHA256 Hash |
| :--- | :--- | :--- |
| `sources/plant.basketfern.fn2/plant.basketfern.fn2.blend` | 961,657 | `c0857b1a2d2efd4a92441392a6689e83823eb0aedfd56c3c46b6c6b146d8f367` |
| `sources/plant.basketfern.fn2/plant.basketfern.fn2.result.json` | 2,861 | `1c66a813a5691c0175320941551739f6ff914cb5f836ea9360a6ca5eca3c4066` |
| `intermediate/bakes/plant.basketfern.fn2.baked.blend` | 1,796,575 | `fb71ed6da6a64fb2bbf7423df06d746fb3cf6f991c9c3fb2410ac4679f3f9a32` |
| `intermediate/bakes/plant.basketfern.fn2/tree_foliage_albedo.png` | 208,226 | `99ba06abd5e5aa0660ead566bfb015a9eda5e0566d7cd76cfb122944f100eafd` |
| `intermediate/bakes/plant.basketfern.fn2/tree_foliage_normal.png` | 153,965 | `3e6af05183c0c9d9a2bfb35af4514869b4f1143fea7b9831346a860becdc2916` |
| `intermediate/bakes/plant.basketfern.fn2/tree_foliage_roughness.png` | 136,071 | `fe00356eee762a8ba927b1f5540505d14d57931cea442b43e600e51aa2f122ca` |
| `intermediate/bakes/plant.basketfern.fn2/tree_foliage_thickness.png` | 158,694 | `5039ff3cdac9cc489fa1dbfdd486074ea3b94b940e79d9a80d63cb23479e722e` |
| `intermediate/bakes/plant.basketfern.fn2/tree_structure_albedo.png` | 115,521 | `3e09065c38c925dec49abeae6503543aee1e5e0370d31bcc9e2cb063a849d07b` |
| `intermediate/bakes/plant.basketfern.fn2/tree_structure_normal.png` | 169,534 | `886370bcb484e0416147e1a01477a1f9ffbe093818282a5c61fbf5046c9441b2` |
| `intermediate/bakes/plant.basketfern.fn2/tree_structure_roughness.png` | 65,229 | `7d5599841b7f7d001ecd9b3cb256c698d8cb05d858b3c6ac35ad1d9256cfd784` |
| `intermediate/bakes/plant.basketfern.fn2/tree_structure_thickness.png` | 48,141 | `9b13355b8703bd3f88f7d602a9b2b65eddec69b048b30ea734645233c9398022` |
| `intermediate/bakes/plant.basketfern.fn2/plant.basketfern.fn2.bake.result.json` | 4,908 | `74025a12058a96371f55349c148556268829196c565d66a1431b7e5aacb3a8c5` |
| `exports/plant.basketfern.fn2.baked.glb` | 1,151,760 | `a5c2ca769bcd2a644d3d85d631f0f777e4b96b740a01f025176c668b31fd3cb8` |
| `exports/plant.basketfern.fn2.export.result.json` | 307 | `2701fd057246c37e0b2112363a2c659cd9533e906e7f4a70757fff18750852e7` |
| `preview/plant.basketfern.fn2/turntable/plant.basketfern.fn2_neutral_000.png` | 234,135 | `e362db4ff89db72b8be3bd55b042e49998ec98b0825c07e14377a218ecf41ccd` |
| `preview/plant.basketfern.fn2/turntable/plant.basketfern.fn2_neutral_090.png` | 256,614 | `691a22d707399f14586b5d3dd5ca12cc10bd6bbfee01e65d3711878e72c34855` |
| `preview/plant.basketfern.fn2/turntable/plant.basketfern.fn2_neutral_180.png` | 230,915 | `a0efadfe7b76940a8d2cf5d760f4e30dcb464d6aaae7c6e580b3eb016b77fd39` |
| `preview/plant.basketfern.fn2/turntable/plant.basketfern.fn2_neutral_270.png` | 251,824 | `9ee0b31a3c9c33a9da9c6eb08247c8db2fa9935cc292b460d65564ef11a1b3ba` |
| `preview/plant.basketfern.fn2/turntable/plant.basketfern.fn2.turntable.result.json` | 2,156 | `e0bf54b443c4b807acd041e6982fe70bf480a4ad3aed5bab081b0574ad0f7e24` |
| `reports/logs/generate_plant_plant.basketfern.fn2.log` | 3,567 | `5d808691a8b3daea7af81ae02711f1ea066cf0ce63d2934cc9f6b11eb4fa6925` |
| `reports/logs/bake_materials_plant.basketfern.fn2.log` | 7,307 | `a4796788c4ceb7879212eb73f27faa8916cd2faeae0f913840d66182ffe6e2b4` |
| `reports/logs/render_turntable_plant.basketfern.fn2.log` | 4,099 | `c6282d4ec29a3748b4ab98e7f04005544a86b6e2ad2c4d8ca41d55551b148d48` |
| `reports/manifests/plant.basketfern.fn2.json` | 6,299 | `553e0baabc3c8039e087d6ba357151f6e4ce19ef087b286fb76d968c9ec1adc2` |
| `reports/manifests/plant.basketfern.fn2.bake.json` | 8,272 | `51e377b16f8abf02ec7c4d8e91083aba1be9776c4cbda882d61a5d234040bcac` |
| `reports/manifests/plant.basketfern.fn2.turntable.json` | 7,436 | `2d0ee3bc44c8a4e7a0998e0421b41fc8114b76f053ef928ccbdbf5e48b9d9cd6` |

---

## 4. Visual & Silhouette Inspection

From turntable renders (`plant.basketfern.fn2_neutral_*.png`):
- **Coloration**:
  - Foliage shows natural rich olive-forest green (`#3f6f36` base tint) with procedural tonal mottling, fully diffuse and free of burnt-in directional highlights/shadows.
  - Structure/trunk exhibits fibrous warm ochre-brown bark tones (`#6a563c`), transitioning cleanly to petiole bases.
- **Silhouette & Morphology**:
  - The generated mesh uses the `tree` morphology family (assigned by generator resolver for `tree_frond_crown`), producing an erect central trunk (~3.6 m tall) with radial branches terminating in leaf blade clusters.
  - Overall height is **3.91 m**, crown spread is **2.11 m** across X and **0.77 m** across Z.
- **Remaining Observations & Proposed Adjustments**:
  - *Silhouette Discrepancy*: While the definition describes a *vase-shaped rosette crown* with huge arching fronds radiating from a short stout rhizome (0.55 m base diameter, 3.2 m frond length), the current generator mapped `tree_frond_crown` into the branch-and-twig `tree` architecture instead of a direct rachis/crozier arching crown (`fern` / `rosette`).
  - *Scale*: The 3.91 m height is consistent with the mature stage target (3.6 m stem + crown foliage), but the branching trunk gives it an arborescent shrub/tree silhouette rather than a massive marattioid tree fern vase.
  - Future morphology pipeline updates can bind `tree_frond_crown` generator to true frond rachis curves with pinna series to match the intended vase crown silhouette.

Parent verification: native run20261012-115956-477ec565 imports exactbakedGLB while loading existingresourcecontinuitysave3 and correctedfootGLB. Greenfoliage/barkasset is visible distantly, not a closefloraqualityreview. Materialpipeline runs; fernmorphology is rejected. No botanical/reference/30FPSacceptance.
