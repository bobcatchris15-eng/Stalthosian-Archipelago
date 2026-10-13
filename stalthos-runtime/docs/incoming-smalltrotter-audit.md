# Incoming Smalltrotter asset audit

Audit date: 2026-10-13. Source inspected with Blender 5.2.0 LTS, background mode, `--disable-autoexec`. The source file was copied before opening; the delivered source and working copy both hash to `B987E7D7EAC4DFE0E09E2E188A592236ED57EB5E00DA7D8F7E6D1D901DE15350` (SHA-256).

## Inventory

The file contains one scene (`Scene`), metric units with scale length 1.0, and three mesh objects. Their object transforms are identity; dimensions are in Blender units (meters under the scene unit setting). The body bounds span X `[-0.488, 0.488]`, Y `[-1.712, 3.048]`, Z approximately `[-0.0003, 2.598]`.

| Object | Base vertices | Base polygons | Evaluated vertices | Evaluated triangles | Notes |
|---|---:|---:|---:|---:|---|
| `Smalltrotter_Body` | 34,144 | 33,318 | 134,922 | 266,544 | Multires modifier enabled in viewport |
| `Smalltrotter_Eyes` | 964 | 1,024 | 964 | 1,920 | Separate mesh |
| `Smalltrotter_Bristles` | 40,300 | 39,000 | 40,300 | 71,500 | Separate mesh |

The scene has one material datablock (`Material`), but none of the three mesh objects has a material slot. There are no UV layers. The two image datablocks are `VIEWER` placeholders (`Render Result` and `Viewer Node`), with no file paths and no packed image files; thus no actual external or packed texture images were found. There is no armature, bone, vertex group, shape key, object animation data, or action. This is a static sculpted mesh set; the file does not contain a rig or animation clip.

## Trial export and preview

Blender's installed glTF 2.0 exporter completed a GLB export from the working copy. The GLB is a separate trial artifact, not a production-ready runtime asset. The scene has no assigned mesh materials or UV maps, and no animation to export. Export artifact: `var/incoming-smalltrotter-20261013/smalltrotter-trial.glb`, SHA-256 `32A09D308E510FD453E1E7CC558806D7F40314092A9BFDF281B9297F5050E218`.

A neutral-gray 768×768 preview was rendered after adding temporary camera and area lights and a preview-only neutral material in a separate Blender process. These additions were not saved to the source copy. Preview: `var/incoming-smalltrotter-20261013/smalltrotter-neutral-preview.png`, SHA-256 `667E825FD2E18D6DF3B3EE560D5760B67CDD889031891FEF07CC0AEC369B0D66`.

The complete Blender API inventory and file hashes are recorded in `var/incoming-smalltrotter-20261013/audit.json` and `var/incoming-smalltrotter-20261013/receipt-hashes.json`. The scripts used for inspection/export and preview are retained alongside these receipts. The source copy hash was checked against the supplied source after processing and remains identical. No remeshing, decimation, material reassignment on the delivered file, rigging, animation creation, or runtime integration was performed.

This audit and export establish file contents and exporter feasibility only. They do not establish character suitability, visual acceptance, native runtime import, performance, or draft completion.

## Follow-up: close views, export geometry, and topology

Close neutral previews were rendered from the same source copy with temporary in-memory camera, light, and neutral-material setup (never saved into the copy). The accurate side profile is `var/incoming-smalltrotter-20261013/smalltrotter-close-profile.png`; the three-quarter view is `var/incoming-smalltrotter-20261013/smalltrotter-close-threequarter.png`. Both are 768×768. Hashes for these, the other exploratory views, the source copy, and GLB are in `var/incoming-smalltrotter-20261013/followup-hashes.json`.

The trial GLB is 2,652,380 bytes. Direct parsing of its glTF accessors and indices gives:

| Primitive | Exported vertices | Exported triangles | Exported position bounds |
|---|---:|---:|---|
| `Smalltrotter_Body` | 34,144 | 66,636 | X `[-0.488, 0.488]`, Y `[0, 2.598]`, Z `[-3.048, 1.711]` |
| `Smalltrotter_Bristles` | 40,300 | 71,500 | X `[-0.410, 0.460]`, Y `[0.860, 2.617]`, Z `[-2.331, 1.317]` |
| `Smalltrotter_Eyes` | 964 | 1,920 | X `[-0.104, 0.104]`, Y `[2.498, 2.544]`, Z `[-1.466, -1.420]` |

Total exported geometry is 75,408 vertices and 140,056 triangles across the three primitives. The body remains at its 34,144-vertex base mesh: this trial invoked the glTF exporter with modifier application disabled, while the separately measured viewport evaluation includes the Multires modifier and is 134,922 vertices / 266,544 triangles. Thus the smaller GLB reflects the explicit export setting and is not evidence that the displayed Multires level was baked.

Read-only edge/topology counts (receipt: `var/incoming-smalltrotter-20261013/topology.json`) show two equal 17,072-vertex components in the body, with 1,648 boundary edges and no edges incident to more than two faces. Its evaluated Multires form has 3,296 boundary edges and no nonmanifold edges. The eyes are two closed 482-vertex components with no boundary or nonmanifold edges. Bristles are 1,300 disconnected 31-vertex components, each an open element overall; together they have 6,500 boundary edges and no nonmanifold edges. These graph counts establish connectedness/boundary properties only, not anatomical attachment, normals, self-intersections, or watertightness of the whole character assembly.

Provenance scan found zero embedded text datablocks, zero linked Blender libraries, no external or packed image assets, no fonts, and no linked or overridden scene objects (`var/incoming-smalltrotter-20261013/provenance.json`). This supports a self-contained Blender scene at inspection time, subject to the original file hash and the limits of datablock-path scanning.

Preparation roadmap for a later authorized asset pass: preserve this supplied file as the source master; first establish intended body/eye/bristle attachment and application axis/scale in a reviewable preview. Keep the base sculpt intact as a recoverable source and compare export choices against it: base resolution versus an explicitly approved baked Multires level, since current viewport evaluation is about 267k body triangles by itself. Check feet, tail clearance, and whether the body’s two connected components are expected before deciding on repair. Then create explicit material slots and UV/texture strategy, define whether bristles remain separate static geometry or need authored deformation, and plan a skeleton/weights around intended gait only after movement requirements and joint locations are reviewed. Validate each change in the native importer and in pose/contact views. No mesh, material, rig, or animation changes were made during this audit.

## Source retained for project workers
The supplied master is retained byte-for-byte under assets/source/creatures/smalltrotter/smalltrotter-rough-master.blend with provenance README. Root verified its source hash. Isolated audit commits29fc17d/a394d7a are retained; generated previews/export remain under var/worktrees/incoming-smalltrotter-audit/var/incoming-smalltrotter-20261013. Future remote workers can derive copies from the tracked master. The rigged-creature importer still requires skin and animation; this static mesh is not passed off as animated playable content.
